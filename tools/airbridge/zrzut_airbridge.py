#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Zrzut odpowiedzi AirBridge: to samo, co dostaje panel. Wyłącznie żądania GET, niczego nie zmienia.

Zapisuje surowe odpowiedzi status.json, lista-nocy.json i noc-{dzien}.json poza repozytorium (dane zdrowotne)
oraz podsumowanie.txt: same nazwy pól i tryby, bez wyników nocy i bez numeru seryjnego.
Hasło: pytanie w Terminalu albo zmienna AB_HASLO; nigdzie nie jest zapisywane.
"""
import argparse
import datetime
import getpass
import json
import os
import pathlib
import sys
import time
import urllib.error
import urllib.request

LISTA = "/api/report?source=edf&action=days"
NOC = "/api/report?source=edf&day={}"
JOB = "/api/report?source=edf&job={}"
CZEKANIE_S = 120  # raport nocy powstaje na AirBridge, duża karta potrzebuje chwili


def w_repozytorium(sciezka):
    # dane zdrowotne nigdy w repo: odmowa także w drzewie roboczym git (tam .git jest plikiem)
    for k in [sciezka, *sciezka.parents]:
        if (k / ".git").exists():
            return True
    return False


def klient(adres, konto, haslo):
    baza = "http://" + adres
    if not konto:
        return urllib.request.build_opener(), baza
    menedzer = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    menedzer.add_password(None, baza, konto, haslo)
    return urllib.request.build_opener(urllib.request.HTTPDigestAuthHandler(menedzer)), baza


def pobierz(opener, baza, sciezka):
    try:
        with opener.open(baza + sciezka, timeout=30) as o:
            return o.status, o.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def raport(opener, baza, sciezka, czekanie=CZEKANIE_S, pauza=1.0):
    """GET z dopytywaniem o zadanie, tak jak panel: 202 albo {"job":…,"pending":true}, potem job={id}."""
    koniec = time.monotonic() + czekanie
    kod, tresc = pobierz(opener, baza, sciezka)
    while True:
        try:
            j = json.loads(tresc)
        except ValueError:
            j = None
        job = j.get("job") if isinstance(j, dict) else None
        gotowy = isinstance(j, dict) and ("days" in j or "present" in j)
        if kod not in (200, 202) or not job or (kod == 200 and gotowy):
            return kod, tresc
        if time.monotonic() > koniec:
            return kod, tresc
        time.sleep(pauza)
        kod, tresc = pobierz(opener, baza, JOB.format(urllib.request.quote(str(job), safe="-_.~")))


def dni_z_listy(j):
    wy = []
    for d in j.get("days", []) if isinstance(j, dict) else []:
        v = d.get("day") if isinstance(d, dict) else d
        if isinstance(v, int):
            wy.append(v)
    return sorted(set(wy))


def podsumuj_noc(j):
    u = j.get("settings") or {}
    pola = u.get("fields") or []
    tagi = [p.get("tag") for p in pola if isinstance(p, dict) and p.get("tag")]
    mop = next((p.get("value", p.get("option")) for p in pola if isinstance(p, dict) and p.get("tag") == "MOP"), None)
    ch = j.get("channels") or []
    # na prawdziwym AirBridge `channels` to obiekt z nazwami w kluczach, nie lista
    kanaly = list(ch) if isinstance(ch, dict) else [k.get("name") if isinstance(k, dict) else k for k in ch]
    return {
        "present": j.get("present"),
        "partial": j.get("partial"),
        "settings.available": u.get("available"),
        "MOP": mop,
        "tagi": tagi,
        "kanaly": [k for k in kanaly if k],
        "pola_raportu": sorted(j.keys()),
    }


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--adres", default="airbridge.local", help="adres AirBridge, np. 192.168.1.50")
    p.add_argument("--konto", default="admin", help="konto AirBridge (puste: bez logowania)")
    p.add_argument("--noce", type=int, default=7, help="ile ostatnich nocy pobrać")
    p.add_argument("--wyjscie", default=None, help="katalog wyniku, poza repozytorium")
    a = p.parse_args(argv)

    dzis = datetime.date.today().strftime("%Y%m%d")
    wy = pathlib.Path(a.wyjscie or f"~/cpap-eink-dane/zrzut-airbridge-{dzis}").expanduser().resolve()
    if w_repozytorium(wy):
        sys.exit(f"Odmowa: {wy} leży w repozytorium, a to dane zdrowotne. Podaj katalog poza nim.")
    wy.mkdir(parents=True, exist_ok=True)

    haslo = os.environ.get("AB_HASLO") or (getpass.getpass("Hasło AirBridge: ") if a.konto else "")
    opener, baza = klient(a.adres, a.konto, haslo)
    wiersze = [f"zrzut {dzis}, adres {a.adres}"]

    try:
        kod, tresc = pobierz(opener, baza, "/api/status")
    except OSError as e:
        sys.exit(f"Nie mogę połączyć się z {a.adres} ({e}). Sprawdź, czy aparat jest włączony, a Mac w tej samej sieci Wi-Fi.")
    if kod == 401:
        sys.exit("AirBridge odrzucił konto albo hasło (401).")
    (wy / "status.json").write_bytes(tresc)
    st = json.loads(tresc) if kod == 200 else {}
    wiersze += [f"status: kod {kod}", f"wersja: {st.get('version')}", f"model (pna): {st.get('pna')}", f"tryb (mode): {st.get('mode')}",
                f"pola statusu: {sorted(st.keys())}"]

    kod, tresc = raport(opener, baza, LISTA)
    (wy / "lista-nocy.json").write_bytes(tresc)
    dni = dni_z_listy(json.loads(tresc)) if kod == 200 else []
    wiersze.append(f"lista nocy: kod {kod}, {len(dni)} nocy")

    for d in dni[-a.noce:]:
        kod, tresc = raport(opener, baza, NOC.format(d))
        (wy / f"noc-{d}.json").write_bytes(tresc)
        try:
            opis = podsumuj_noc(json.loads(tresc)) if kod == 200 else {}
        except ValueError:
            opis = {"blad": "odpowiedź to nie JSON"}
        wiersze.append(f"noc {d}: kod {kod} {json.dumps(opis, ensure_ascii=False)}")

    tekst = "\n".join(wiersze) + "\n"
    (wy / "podsumowanie.txt").write_text(tekst, encoding="utf-8")
    print(tekst)
    print(f"Zapisane w: {wy}\nTe pliki to dane zdrowotne: nie wysyłaj ich mailem ani do repozytorium.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
