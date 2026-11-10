#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Zrzut plików z pamięci panelu (LittleFS) przez port szeregowy do katalogu danych poza repo (WY12).

Zrzut zabezpiecza historię nocy, odpowiedzi „Co było inaczej?”, dziennik ekranów, odczyty sypialni, pomiar baterii
i ustawienia panelu (`/konfiguracja.txt`: daty wymiany sprzętu, model maski, ustawienia z formularza) przed
przejściem na układ partycji 32 MB (spec techniczna 8.9, krok 1; tools/prototyp/przejscie_32mb.py woła ten skrypt
dwa razy) i jako kopia zapasowa. Do panelu dane wracają komendą `W` (tools/prototyp/wgraj_pliki.py), nie przez
`uploadfs`: 8.8 wyklucza `uploadfs` na egzemplarzu z danymi.

Firmware w oknie czuwania (20 s po naciśnięciu przycisku) po znaku 'Z' wypisuje pliki między
liniami PLIKI_START / PLIK <nazwa> / PLIK_KONIEC / PLIKI_KONIEC; także pliki `/dane/…` i `/log/…`
(bez pustych sekcji plików prototypu, których nie ma) i `/konfiguracja.txt`, gdy jest. Przed sekcją konfiguracji
panel pisze wiersz `konfiguracja w zrzucie: {n} B, sha256 {skrót}`; skrypt składa z sekcji treść bajt w bajt
(puste wiersze, koniec pliku bez `\\n`), czytając sekcję według tego rozmiaru (wiersz treści równy znacznikowi
protokołu nie kończy sekcji), i sprawdza ją tym skrótem. Konfiguracja trafia do `konfiguracja.txt` w korzeniu
katalogu zrzutu; przy niezgodnym skrócie (błąd przesyłu) albo bez wiersza skrótu do `konfiguracja.txt.niezgodny`,
a skrypt kończy się błędem (zrzut do powtórzenia). Wcześniejsze wersje zostają jako kopie z datą i godziną
(`konfiguracja.txt.poprzedni-RRRRMMDD-HHMMSS`). Treści konfiguracji skrypt nie wypisuje, tylko rozmiar.

Użycie (pyserial jest w środowisku PlatformIO):
  ~/.platformio/penv/bin/python tools/prototyp/zrzut_plikow.py /dev/cu.usbserial-110 <katalog danych poza repo> [--reset]
  ~/.platformio/penv/bin/python tools/prototyp/zrzut_plikow.py /dev/cu.usbserial-110 <katalog> --tylko-konfiguracja
Bez --reset: najpierw naciśnij dowolny przycisk panelu (budzi go na 20 s).
--tylko-konfiguracja: zapisuje w katalogu tylko `konfiguracja.txt` (do zmiany jednej linii i wgrania przez
`wgraj_pliki.py --konfiguracja`). Do panelu idzie wyłącznie znak `Z` (odczyt), nigdy komenda `W` ani `K`.
pyserial jest importowany dopiero w main(), więc moduł działa w testach bez niego (tools/test_wgraj_pliki.py).
"""
import argparse
import hashlib
import os
import re
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KONFIGURACJA = "/konfiguracja.txt"
PLIK_KONFIGURACJI = "konfiguracja.txt"  # nazwa w katalogu zrzutu; tę samą wysyła wgraj_pliki.py
NIEZGODNY = ".niezgodny"
WZOR_SKROTU = re.compile(r"konfiguracja w zrzucie: (\d+) B, sha256 ([0-9a-f]{64})\Z")


def _bez_konca(w):
    """Wiersz portu bez znaku końca linii (`\\r\\n` z Serial.println albo `\\n`); reszta bajtów bez zmian."""
    if w.endswith(b"\r\n"):
        return w[:-2]
    if w.endswith(b"\n"):
        return w[:-1]
    return w


def czytaj_zrzut(port, limit_s, log=None, co_s=1.0):
    """Wysyła `Z` co `co_s` s (panel czyta znak tylko w czuwaniu) do PLIKI_START, potem zbiera sekcje do PLIKI_KONIEC.

    Wynik: (pliki, skrot) albo None po limit_s sekundach. `pliki`: nazwa → lista wierszy; dla `/konfiguracja.txt`
    wiersze jako bajty, każdy także pusty, czytane według rozmiaru z wiersza skrótu (wiersz treści równy znacznikowi
    protokołu nie kończy sekcji; bez wiersza skrótu do `PLIK_KONIEC`), dla pozostałych plików tekst bez pustych
    wierszy (jak dotąd). `skrot`:
    (rozmiar, sha256) z wiersza `konfiguracja w zrzucie: …` albo None. Inne wiersze (log panelu) idą do `log`.
    Do portu idzie wyłącznie b"Z"."""
    koniec = time.monotonic() + limit_s
    pliki, biezacy, zbieram, skrot = {}, None, False, None
    ostatnie = None
    zostalo = None  # bajty treści konfiguracji do przeczytania (sekcja czytana według rozmiaru z wiersza skrótu)
    while time.monotonic() < koniec:
        if not zbieram and (ostatnie is None or time.monotonic() - ostatnie >= co_s):
            port.write(b"Z")
            ostatnie = time.monotonic()
        surowy = port.readline()
        if not surowy:  # koniec limitu portu, nie pusty wiersz
            continue
        dokladny = _bez_konca(surowy)
        if zostalo is not None:
            # wiersz treści, także równy znacznikowi (`PLIK_KONIEC`, `PLIK …`): każdy wiersz to treść plus `\n`
            # (ostatni może być bez niego), więc sekcja kończy się, gdy wiersze pokryją rozmiar z panelu
            if zostalo > 0:
                pliki[KONFIGURACJA].append(dokladny)
                zostalo -= len(dokladny) + 1
                continue
            zostalo = None  # treść kompletna: ten wiersz to znacznik końca sekcji (inny = skrót się nie zgodzi)
        l = dokladny.decode("utf-8", "replace").rstrip("\r\n")
        if l == "PLIKI_START":
            zbieram, pliki, skrot, biezacy = True, {}, None, None
        elif zbieram and biezacy is None and l.startswith("PLIK "):
            biezacy = l[5:].strip()
            pliki[biezacy] = []
            if biezacy == KONFIGURACJA and skrot is not None:
                zostalo = skrot[0]
                if zostalo == 0:
                    zostalo = None
        elif zbieram and l == "PLIK_KONIEC":
            biezacy = None
        elif zbieram and l == "PLIKI_KONIEC":
            return pliki, skrot
        elif zbieram and biezacy == KONFIGURACJA:
            pliki[biezacy].append(dokladny)
        elif zbieram and biezacy:
            if l:
                pliki[biezacy].append(l)
        else:
            m = WZOR_SKROTU.match(l) if zbieram else None
            if m:
                skrot = (int(m.group(1)), m.group(2))
            if log:
                log(l)
    return None


def zloz_konfiguracje(wiersze, skrot):
    """Treść `/konfiguracja.txt` z wierszy sekcji: (bytes, stan), stan "zgodny", "niezgodny" albo "bez skrotu".

    Panel wypisuje każdy wiersz, a `\\n` na końcu pliku nie daje pustego wiersza, więc z wierszy wynikają dwie treści
    (z `\\n` na końcu i bez); wybiera tę, która zgadza się z rozmiarem i skrótem z panelu. Bez skrótu: z `\\n`."""
    z_koncem = b"".join(w + b"\n" for w in wiersze)
    bez_konca = z_koncem[:-1] if z_koncem else b""
    if skrot is None:
        return z_koncem, "bez skrotu"
    for t in (z_koncem, bez_konca):
        if len(t) == skrot[0] and hashlib.sha256(t).hexdigest() == skrot[1]:
            return t, "zgodny"
    return z_koncem, "niezgodny"


def _odloz(p):
    if os.path.exists(p):
        os.replace(p, p + ".poprzedni")


POPRZEDNI = ".poprzedni"


def _odloz_z_data(p, przyrostek, znacznik):
    """`p` (jeśli jest) do `{plik konfiguracji}{przyrostek}-{RRRRMMDD-HHMMSS}`, z numerem przy kolizji; nigdy nie
    nadpisuje istniejącej kopii."""
    if not os.path.exists(p):
        return
    baza = os.path.join(os.path.dirname(p), PLIK_KONFIGURACJI + przyrostek + "-" + znacznik)
    cel, n = baza, 1
    while os.path.exists(cel):
        n += 1
        cel = "%s-%d" % (baza, n)
    os.replace(p, cel)


def zapisz_konfiguracje(kat, wiersze, skrot, log=print):
    """Zapis konfiguracji ze zrzutu do `kat`; wynik stan z zloz_konfiguracje(). Przy niezgodnym skrócie treść idzie do
    `konfiguracja.txt.niezgodny`, żeby przywracanie z katalogu nie wysłało ani uszkodzonej, ani starej treści; bez
    wiersza skrótu tak samo (treść niesprawdzona). Wcześniejszy `konfiguracja.txt` i `konfiguracja.txt.niezgodny` idą
    do kopii z datą i godziną (`konfiguracja.txt.poprzedni-RRRRMMDD-HHMMSS`, `….niezgodny-…`), bez nadpisywania.
    Treść nie trafia do `log`, tylko rozmiar."""
    tresc, stan = zloz_konfiguracje(wiersze, skrot)
    p = os.path.join(kat, PLIK_KONFIGURACJI)
    znacznik = time.strftime("%Y%m%d-%H%M%S")
    _odloz_z_data(p, POPRZEDNI, znacznik)  # nic nie ginie: każda wcześniejsza wersja w kopii z datą i godziną
    _odloz_z_data(p + NIEZGODNY, NIEZGODNY, znacznik)  # stary nieudany odczyt nie zostaje obok dobrego pliku
    if stan != "zgodny":
        with open(p + NIEZGODNY, "wb") as f:
            f.write(tresc)
        powod = "bez wiersza skrótu z panelu" if stan == "bez skrotu" else "skrót niezgodny z panelem (błąd przesyłu)"
        log("UWAGA: konfiguracja: %s; zapisana jako %s, nie jako %s; zrób zrzut jeszcze raz"
            % (powod, p + NIEZGODNY, PLIK_KONFIGURACJI))
        return stan
    with open(p, "wb") as f:
        f.write(tresc)
    log("zapisano %s: %d B" % (p, len(tresc)))
    return stan


def zapisz_zrzut(kat, pliki, skrot, tylko_konfiguracja=False, log=print):
    """Zapis plików zrzutu do `kat` (poprzednia wersja pliku jako `.poprzedni`). Wynik: stan konfiguracji
    ("zgodny", "niezgodny", "bez skrotu") albo "brak" (panel nie ma `/konfiguracja.txt`)."""
    for nazwa in pliki:
        if ".." in nazwa.split("/"):
            sys.exit(f"nazwa pliku z '..': {nazwa}")
    stan = "brak"
    for nazwa, wiersze in pliki.items():
        if nazwa == KONFIGURACJA:
            stan = zapisz_konfiguracje(kat, wiersze, skrot, log)
            continue
        if tylko_konfiguracja:
            continue
        p = os.path.join(kat, nazwa.lstrip("/"))
        os.makedirs(os.path.dirname(p), exist_ok=True)  # pliki w /dane/…, /log/…
        _odloz(p)
        with open(p, "w", encoding="utf-8") as f:
            f.write("".join(w + "\n" for w in wiersze))
        log(f"zapisano {p}: {len(wiersze)} wierszy")
    return stan


def main(argv=None):
    a = argparse.ArgumentParser()
    a.add_argument("port")
    a.add_argument("katalog", help="katalog danych poza repozytorium")
    a.add_argument("--reset", action="store_true", help="zrestartuj panel (okno czuwania zaczyna się po rysowaniu)")
    a.add_argument("--czas", type=int, default=90, help="ile sekund próbować")
    a.add_argument("--tylko-konfiguracja", action="store_true",
                   help="zapisz tylko konfiguracja.txt (odczyt; do panelu idzie tylko znak Z)")
    a = a.parse_args(argv)
    kat = os.path.abspath(a.katalog)
    if kat == REPO or kat.startswith(REPO + os.sep):
        sys.exit("katalog danych musi być poza repozytorium (WY12)")
    import serial  # leniwie: testy bez pyserial

    os.makedirs(kat, exist_ok=True)
    s = serial.Serial(a.port, 115200, timeout=0.5)
    if a.reset:
        s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
    wynik = czytaj_zrzut(s, a.czas, log=lambda t: print(t, flush=True))
    if wynik is None:
        sys.exit("nie dostałem plików: obudź panel przyciskiem albo użyj --reset")
    stan = zapisz_zrzut(kat, wynik[0], wynik[1], a.tylko_konfiguracja, log=lambda t: print(t, flush=True))
    sys.exit(podsumowanie(kat, stan, a.tylko_konfiguracja, a.port))


def podsumowanie(kat, stan, tylko_konfiguracja, port):
    """Komunikat końcowy; wynik jak sys.exit(): None przy sukcesie, tekst błędu przy porażce."""
    p = os.path.join(kat, PLIK_KONFIGURACJI)
    if tylko_konfiguracja:
        if stan == "brak":
            return "panel nie ma /konfiguracja.txt (ustawienia domyślne); nowy plik wgrasz przez wgraj_pliki.py --konfiguracja"
        if stan != "zgodny":
            return "konfiguracja odczytana z błędem przesyłu albo bez skrótu z panelu: uruchom skrypt jeszcze raz"
        print("gotowe: %s. Zmień w nim linię i wgraj go: tools/prototyp/wgraj_pliki.py %s --konfiguracja %s"
              % (p, port, p))
        return None
    print(f"gotowe: zrzut w {kat}. Przejście na układ 32 MB: tools/prototyp/przejscie_32mb.py "
          "(zrzut dwa razy, wgranie kablem, pliki komendą W; spec 8.9). Bez uploadfs (8.8).")
    if stan == "brak":
        print("panel nie ma /konfiguracja.txt: w zrzucie bez konfiguracji")
    if stan in ("niezgodny", "bez skrotu"):
        return "konfiguracja w zrzucie z błędem przesyłu albo bez skrótu (pozostałe pliki zapisane): zrób zrzut jeszcze raz"
    return None


if __name__ == "__main__":
    main()
