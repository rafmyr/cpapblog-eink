#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Wydanie firmware do aktualizacji z GitHub (ADR-0032, ADR-0038).

Buduje obraz bez sekretów (`pio run -e e1001_wydanie`, flaga BEZ_SEKRETOW, numer wersji z argumentu), sprawdza, że
w obrazie nie ma wartości z lokalnego include/secrets.h, liczy SHA-256, pisze `wydanie.json` i nazywa obraz
`cpap-eink-{wersja}.bin`. Z flagą --publikuj tworzy wydanie `v{wersja}` w repozytorium wydań (`gh release create`).
Bez --publikuj niczego nie wysyła.

Użycie:
  python3 tools/wydanie/wydaj.py 1.0.1                       # budowa i pliki w build/wydanie-1.0.1/
  python3 tools/wydanie/wydaj.py 1.0.1 --obraz firmware.bin  # bez budowy, z gotowego obrazu bez sekretów
  python3 tools/wydanie/wydaj.py 1.0.1 --publikuj            # jak wyżej i wydanie na GitHubie (tylko na wyraźne polecenie)
  python3 tools/wydanie/wydaj.py --nastepna                  # tylko wypisuje następny numer po ostatnim wydaniu
Na co dzień: tools/wydanie/wydaj.sh (sprawdza main i CI, sam bierze następny numer, pyta „tak”, publikuje).
"""
import argparse
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FW = os.path.join(REPO, "firmware", "prototyp")
REPO_WYDAN = "rafmyr/cpapblog-eink"       # wartość domyślna jak REPO_WYDAN w src/aktualizacja.h (ADR-0038 pkt 5)
PLYTKA = "reterminal_e1001"               # PLYTKA_WYDANIA w src/aktualizacja.h
SLOT = 0x400000                           # slot aplikacji 4 MB (firmware/partycje-e1001.csv, spec techniczna 8.2)
TABLICA = os.path.join(REPO, "firmware", "partycje-e1001.csv")

# SemVer jak aktualizacja::parsujWersje(): liczby bez zer wiodących (najwyżej 6 cyfr), identyfikatory [0-9A-Za-z-]
_LICZBA = r"(?:0|[1-9][0-9]{0,5})"
_ID = r"(?:0|[1-9][0-9]{0,5}|[0-9]*[A-Za-z-][0-9A-Za-z-]*)"
WERSJA_RE = re.compile(r"^%s\.%s\.%s(?:-%s(?:\.%s)*)?$" % (_LICZBA, _LICZBA, _LICZBA, _ID, _ID))


def wersja_ok(w):
    return len(w) <= 64 and bool(WERSJA_RE.match(w))


def klucz_wersji(w):
    """Klucz sortowania SemVer (semver.org 11): wersja bez `-pre` wyższa od każdej `-pre` tej samej trójki,
    identyfikatory liczbowe niżej od tekstowych i porównywane liczbowo."""
    w = w[1:] if w.startswith("v") else w
    rdzen, _, pre = w.partition("-")
    trojka = tuple(int(x) for x in rdzen.split("."))
    if not pre:
        return trojka + ((1,),)
    ids = tuple((0, int(i), "") if i.isdigit() else (1, 0, i) for i in pre.split("."))
    return trojka + ((0,) + ids,)


def nastepna_wersja(tagi):
    """Następny numer po najwyższym tagu wydania (`v1.0.0-beta.2` → `1.0.0-beta.3`, `v1.0.1` → `1.0.2`).
    Tagi spoza SemVer pomija; ostatni identyfikator `-pre` niebędący liczbą dostaje `.1`. None bez żadnego tagu."""
    ok = [t[1:] if t.startswith("v") else t for t in tagi]
    ok = [t for t in ok if wersja_ok(t)]
    if not ok:
        return None
    w = max(ok, key=klucz_wersji)
    rdzen, _, pre = w.partition("-")
    if pre:
        ids = pre.split(".")
        if ids[-1].isdigit():
            ids[-1] = str(int(ids[-1]) + 1)
        else:
            ids.append("1")
        return rdzen + "-" + ".".join(ids)
    a, b, c = rdzen.split(".")
    return "%s.%s.%d" % (a, b, int(c) + 1)


def tagi_wydan(repo):
    """Tagi wydań z GitHuba (`gh api`); wyjątek, gdy gh nie odpowiada albo nie ma dostępu."""
    r = subprocess.run(["gh", "api", "repos/%s/releases" % repo, "--paginate", "--jq", ".[].tag_name"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("gh api repos/%s/releases: %s" % (repo, r.stderr.strip()))
    return [t for t in r.stdout.split() if t]


def sprawdz_opublikowane(repo, wersja):
    """Lista błędów po publikacji: najnowsze wydanie to v{wersja}, nie robocze, z obrazem i wydanie.json."""
    r = subprocess.run(["gh", "api", "repos/%s/releases/latest" % repo], capture_output=True, text=True)
    if r.returncode != 0:
        return ["GitHub nie zwraca najnowszego wydania: " + r.stderr.strip()]
    return bledy_wydania(json.loads(r.stdout), wersja)


def bledy_wydania(w, wersja):
    b = []
    if w.get("tag_name") != "v" + wersja:
        b.append("najnowsze wydanie to %s, nie v%s (wydanie robocze albo pre-release panel pomija)" %
                 (w.get("tag_name"), wersja))
    if w.get("draft") or w.get("prerelease"):
        b.append("wydanie oznaczone jako robocze albo pre-release: panel go nie zobaczy")
    nazwy = {a.get("name"): a.get("state") for a in w.get("assets") or []}
    for n in (nazwa_obrazu(wersja), "wydanie.json"):
        if nazwy.get(n) != "uploaded":
            b.append("brak pliku %s w wydaniu" % n)
    return b


def nazwa_obrazu(wersja):
    return "cpap-eink-%s.bin" % wersja


def uklad(tablica=TABLICA):
    """Identyfikator tablicy partycji tym samym algorytmem co firmware (firmware/prototyp/uklad_partycji.py)."""
    spec = importlib.util.spec_from_file_location("uklad_partycji", os.path.join(FW, "uklad_partycji.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m.identyfikator_pliku(tablica)


def opis(wersja, obraz, uklad_id, plytka=PLYTKA):
    """Słownik `wydanie.json` w kolejności pól aktualizacja::zapiszOpis()."""
    return {
        "wersja": wersja,
        "plytka": plytka,
        "uklad": "0x%08x" % uklad_id,
        "rozmiar": len(obraz),
        "sha256": hashlib.sha256(obraz).hexdigest(),
        "plik": nazwa_obrazu(wersja),
    }


def tekst_opisu(o):
    """Treść pliku jak aktualizacja::zapiszOpis(): JSON bez spacji, jedna linia, znak nowej linii na końcu."""
    return json.dumps(o, ensure_ascii=False, separators=(",", ":")) + "\n"


def sekrety_z_nazwami(sciezka=os.path.join(FW, "include", "secrets.h")):
    """Pary (nazwa, wartość) z `#define NAZWA "wartość"` w lokalnym secrets.h (wartość co najmniej 6 znaków)."""
    if not os.path.exists(sciezka):
        return []
    w = []
    with open(sciezka, encoding="utf-8") as f:
        for linia in f:
            m = re.match(r'\s*#define\s+(\w+)\s+"([^"]{6,})"', linia)
            if m:
                w.append((m.group(1), m.group(2)))
    return w


def sekrety_lokalne(sciezka=os.path.join(FW, "include", "secrets.h")):
    """Wartości tekstowe z `#define` w lokalnym secrets.h (co najmniej 6 znaków); [] bez pliku."""
    return [v for _, v in sekrety_z_nazwami(sciezka)]


def tekst_kodu(katalogi=(os.path.join(FW, "src"), os.path.join(FW, "include"))):
    """Treść kodu programu bez plików z sekretami (secrets.h i jego wzoru): to, co i tak trafia do obrazu jawnie."""
    czesci = []
    for k in katalogi:
        for korzen, _, pliki in os.walk(k):
            for n in sorted(pliki):
                if n in ("secrets.h", "secrets.example.h"):
                    continue
                with open(os.path.join(korzen, n), encoding="utf-8", errors="ignore") as f:
                    czesci.append(f.read())
    return "\n".join(czesci)


def podziel_sekrety(pary, kod):
    """(wartości do sprawdzenia w obrazie, nazwy pominięte). Wartość, która stoi jawnie w kodzie programu (np.
    fabryczne hasło AirBridge równe słowu z napisów), jest w każdym obrazie, więc jej obecność niczego nie mówi:
    pomijamy ją i zgłaszamy z nazwą (fałszywy alarm z 10.11.2026: AB_HOST i AB_PASS)."""
    sprawdzane, pominiete = [], []
    for nazwa, wartosc in pary:
        (pominiete if wartosc in kod else sprawdzane).append(nazwa if wartosc in kod else wartosc)
    return sprawdzane, pominiete


def katalog_budowy():
    """Katalog wyników `pio run` dla e1001_wydanie: PLATFORMIO_WORKSPACE_DIR (wydaj.sh: poza folderem z iCloud,
    bo iCloud dorabia kopie „plik 2.cpp” w .pio i budowa pada na podwójnych definicjach) albo .pio w firmware."""
    baza = os.environ.get("PLATFORMIO_WORKSPACE_DIR") or os.path.join(FW, ".pio")
    return os.path.join(baza, "build", "e1001_wydanie")


# Znacznik wkompilowany tylko z -DBEZ_SEKRETOW (src/main.cpp ZNACZNIK_OBRAZU_WYDANIA): obraz bez niego
# nie jest wydawany, także gdy na tej maszynie nie ma include/secrets.h do porównania.
ZNACZNIK = b"cpap-eink:obraz-bez-sekretow:v1"
# Wartości przykładowe ze wzoru include/secrets.example.h i atrapy testów: obraz z nimi to obraz ze wzoru, nie wydanie.
WZORZEC = ["nazwa-sieci", "haslo-wifi", "haslo-airbridge", "wpisz-wlasne-losowe-haslo", "token-zrodla-otoczenia",
           "atrapa-hasla-okna-wgrywania"]


def sprawdz_obraz(obraz, sekrety):
    """Lista błędów: obraz pusty, większy niż slot, bez nagłówka obrazu ESP32 (0xE9), bez znacznika obrazu wydania
    (zbudowany bez BEZ_SEKRETOW), z wartością przykładową ze wzoru albo z wartością z lokalnego secrets.h."""
    b = []
    if obraz and ZNACZNIK not in obraz:
        b.append("brak znacznika obrazu wydania: obraz nie jest zbudowany z -DBEZ_SEKRETOW (pio run -e e1001_wydanie)")
    for w in WZORZEC:
        if w.encode("utf-8") in obraz:
            b.append("obraz zawiera wartość przykładową ze wzoru secrets.example.h")
            break
    if not obraz:
        b.append("obraz pusty")
    if len(obraz) > SLOT:
        b.append("obraz %d B większy niż slot %d B" % (len(obraz), SLOT))
    if obraz and obraz[0] != 0xE9:
        b.append("brak nagłówka obrazu ESP32 (pierwszy bajt 0xE9)")
    for s in sekrety:
        if s.encode("utf-8") in obraz:
            b.append("obraz zawiera wartość z include/secrets.h (%d znaków): wydanie musi być bez sekretów" % len(s))
    return b


def polecenie_budowy(wersja, repo):
    """Polecenie i środowisko budowy obrazu bez sekretów."""
    env = dict(os.environ)
    env["PLATFORMIO_BUILD_FLAGS"] = '-DWERSJA_WYDANIA=\\"%s\\" -DREPO_WYDAN=\\"%s\\"' % (wersja, repo)
    return ["pio", "run", "-e", "e1001_wydanie"], env


def polecenie_publikacji(wersja, repo, pliki, nota):
    return ["gh", "release", "create", "v" + wersja, *pliki, "--repo", repo, "--title", "cpap-eink " + wersja,
            "--notes-file", nota]


def main(argv=None):
    a = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    a.add_argument("wersja", nargs="?", help="numer wersji bez `v`, np. 1.0.1 (wyższy od wersji na panelach)")
    a.add_argument("--nastepna", action="store_true", help="wypisz następny numer po ostatnim wydaniu i zakończ")
    a.add_argument("--repo", default=REPO_WYDAN, help="repozytorium wydań (domyślnie %s)" % REPO_WYDAN)
    a.add_argument("--obraz", help="gotowy obraz zbudowany z BEZ_SEKRETOW (bez budowy)")
    a.add_argument("--katalog", help="katalog wynikowy (domyślnie build/wydanie-{wersja})")
    a.add_argument("--nota", default="", help="krótka nota wydania")
    a.add_argument("--publikuj", action="store_true", help="utwórz wydanie na GitHubie (gh release create)")
    o = a.parse_args(argv)
    if o.nastepna:
        try:
            n = nastepna_wersja(tagi_wydan(o.repo))
        except RuntimeError as e:
            print("BŁĄD:", e, file=sys.stderr)
            return 1
        if not n:
            print("BŁĄD: w %s nie ma jeszcze wydań; podaj numer ręcznie" % o.repo, file=sys.stderr)
            return 1
        print(n)
        return 0
    if not o.wersja or not wersja_ok(o.wersja):
        print("BŁĄD: wersja %r nie jest w zapisie MAJOR.MINOR.PATCH[-pre]" % o.wersja, file=sys.stderr)
        return 2
    if o.obraz:
        sciezka = o.obraz
    else:
        cmd, env = polecenie_budowy(o.wersja, o.repo)
        print("budowa:", " ".join(cmd))
        if subprocess.run(cmd, cwd=FW, env=env).returncode != 0:
            print("BŁĄD: budowa nieudana", file=sys.stderr)
            return 1
        sciezka = os.path.join(katalog_budowy(), "firmware.bin")
    with open(sciezka, "rb") as f:
        obraz = f.read()
    lokalne, pominiete = podziel_sekrety(sekrety_z_nazwami(), tekst_kodu())
    for n in pominiete:
        print("UWAGA: %s z include/secrets.h ma wartość, która stoi jawnie w kodzie programu (np. fabryczne hasło "
              "AirBridge): tej wartości nie da się sprawdzić w obrazie, pominięta; zmień ją na własną" % n,
              file=sys.stderr)
    if not lokalne and not pominiete:
        print("UWAGA: brak include/secrets.h na tej maszynie: porównanie z lokalnymi hasłami pominięte; "
              "obraz sprawdzony znacznikiem BEZ_SEKRETOW i wartościami wzoru", file=sys.stderr)
    bledy = sprawdz_obraz(obraz, lokalne)
    if bledy:
        for b in bledy:
            print("BŁĄD:", b, file=sys.stderr)
        return 1
    kat = o.katalog or os.path.join(REPO, "build", "wydanie-" + o.wersja)
    os.makedirs(kat, exist_ok=True)
    d = opis(o.wersja, obraz, uklad())
    plik_obrazu = os.path.join(kat, nazwa_obrazu(o.wersja))
    shutil.copyfile(sciezka, plik_obrazu)
    plik_opisu = os.path.join(kat, "wydanie.json")
    with open(plik_opisu, "w", encoding="utf-8") as f:
        f.write(tekst_opisu(d))
    plik_noty = os.path.join(kat, "nota.md")
    with open(plik_noty, "w", encoding="utf-8") as f:
        f.write("cpap-eink %s\n\n%s\n" % (o.wersja, o.nota or "Wydanie firmware panelu (obraz bez sekretów)."))
    print("obraz %s, %d B, SHA-256 %s" % (plik_obrazu, d["rozmiar"], d["sha256"]))
    print("UWAGA: obraz wydania nie ma haseł ani adresów. Wolno go instalować tylko na panelu, który ma już w "
          "pamięci sieć, AirBridge, hasło okna serwisowego, źródło otoczenia i współrzędne z wersji z aktualizatorem "
          "wgranej z secrets.h kablem albo oknem serwisowym.", file=sys.stderr)
    if not o.publikuj:
        print("bez --publikuj: nic nie wysłano")
        return 0
    cmd = polecenie_publikacji(o.wersja, o.repo, [plik_obrazu, plik_opisu], plik_noty)
    print("publikacja:", " ".join(cmd))
    rc = subprocess.run(cmd).returncode
    if rc != 0:
        return rc
    bledy = sprawdz_opublikowane(o.repo, o.wersja)
    for b in bledy:
        print("BŁĄD po publikacji:", b, file=sys.stderr)
    if not bledy:
        print("opublikowane: https://github.com/%s/releases/tag/v%s (obraz i wydanie.json na miejscu)" %
              (o.repo, o.wersja))
    return 1 if bledy else 0


if __name__ == "__main__":
    sys.exit(main())
