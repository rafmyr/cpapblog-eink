#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Zrzut plików z pamięci panelu (LittleFS) przez port szeregowy do katalogu danych poza repo (WY12).

Zrzut zabezpiecza historię nocy, odpowiedzi „Co było inaczej?”, dziennik ekranów, odczyty sypialni i pomiar baterii
przed przejściem na układ partycji 32 MB (spec techniczna 8.9, krok 1; tools/prototyp/przejscie_32mb.py woła ten skrypt
dwa razy). Do panelu dane wracają komendą `W` w oknie przywracania (tools/prototyp/wgraj_pliki.py), nie przez
`uploadfs`: 8.8 wyklucza `uploadfs` na egzemplarzu z danymi.

Firmware w oknie czuwania (20 s po narysowaniu ekranu) po znaku 'Z' wypisuje pliki między
liniami PLIKI_START / PLIK <nazwa> / PLIK_KONIEC / PLIKI_KONIEC; także pliki `/dane/…` i `/log/…`
(bez pustych sekcji plików prototypu, których nie ma).

Użycie (pyserial jest w środowisku PlatformIO):
  ~/.platformio/penv/bin/python tools/prototyp/zrzut_plikow.py /dev/cu.usbserial-110 <katalog danych poza repo> [--reset]
Bez --reset: najpierw naciśnij dowolny przycisk panelu (budzi go na 20 s).
"""
import argparse, os, sys, time
import serial

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main():
    a = argparse.ArgumentParser()
    a.add_argument("port")
    a.add_argument("katalog", help="katalog danych poza repozytorium")
    a.add_argument("--reset", action="store_true", help="zrestartuj panel (okno czuwania zaczyna się po rysowaniu)")
    a.add_argument("--czas", type=int, default=90, help="ile sekund próbować")
    a = a.parse_args()
    kat = os.path.abspath(a.katalog)
    if kat == REPO or kat.startswith(REPO + os.sep):
        sys.exit("katalog danych musi być poza repozytorium (WY12)")
    os.makedirs(kat, exist_ok=True)
    s = serial.Serial(a.port, 115200, timeout=0.5)
    if a.reset:
        s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
    koniec = time.time() + a.czas
    pliki, biezacy, zbieram = {}, None, False
    ostatnieZ = 0
    while time.time() < koniec:
        if not zbieram and time.time() - ostatnieZ > 1:
            s.write(b"Z")  # panel czyta znak tylko w oknie czuwania, więc wysyłamy co sekundę
            ostatnieZ = time.time()
        l = s.readline().decode("utf-8", "replace").rstrip("\r\n")
        if not l:
            continue
        if l == "PLIKI_START":
            zbieram, pliki = True, {}
        elif zbieram and l.startswith("PLIK "):
            biezacy = l[5:].strip()
            pliki[biezacy] = []
        elif zbieram and l == "PLIK_KONIEC":
            biezacy = None
        elif zbieram and l == "PLIKI_KONIEC":
            for nazwa, wiersze in pliki.items():
                p = os.path.join(kat, nazwa.lstrip("/"))
                if ".." in nazwa.split("/"):
                    sys.exit(f"nazwa pliku z '..': {nazwa}")
                os.makedirs(os.path.dirname(p), exist_ok=True)  # pliki w /dane/…, /log/…
                if os.path.exists(p):
                    os.replace(p, p + ".poprzedni")
                with open(p, "w", encoding="utf-8") as f:
                    f.write("".join(w + "\n" for w in wiersze))
                print(f"zapisano {p}: {len(wiersze)} wierszy")
            print(f"gotowe: zrzut w {kat}. Przejście na układ 32 MB: tools/prototyp/przejscie_32mb.py "
                  "(zrzut dwa razy, wgranie kablem, pliki komendą W; spec 8.9). Bez uploadfs (8.8).")
            return
        elif zbieram and biezacy:
            pliki[biezacy].append(l)
        else:
            print(l, flush=True)
    sys.exit("nie dostałem plików: obudź panel przyciskiem albo użyj --reset")


if __name__ == "__main__":
    main()
