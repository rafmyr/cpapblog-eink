#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pakiet scen karty z budzikiem (spec funkcjonalna 5.7.2 „Przygotowanie scen”, 5.7.4, R-HUM-11).

Z każdej sceny źródłowej 800×480 w assets/budzik/zrodlo/ (ID literowe 5.7.4: B01…W12, 70 scen) robi bitmapę karty
w assets/budzik/karta-240x205/{nazwa}-240x205.png tą samą metodą co generuj.py sprite(): przycięcie do zawartości,
odbicie lustrzane (budzik patrzy w lewo, na tekst), skalowanie do slotu ≤ 240×205 z zachowaniem proporcji, próg 1-bit
po skalowaniu. Istniejących plików karty nie nadpisuje (są przejrzane w kontroli jakości; 25 z 27 wychodzi z tej
metody bit w bit, B04 i B05 mają 240×133 zamiast 240×135 z tej metody), chyba że z --nadpisz.

Potem (bez --bez-obrazow) odświeża firmware/prototyp/include/obrazy.h przez generuj.py --tylko-obrazy, więc wszystkie
sceny karty są wbudowane w obraz firmware jako zapas (5.7.4: „Zapas scen jest wbudowany w oprogramowanie”).

Użycie:
  python3 tools/prototyp/sceny_karty.py              brakujące sceny karty + obrazy.h
  python3 tools/prototyp/sceny_karty.py --sprawdz    tylko kontrola kompletu (kod 1, gdy scenie źródłowej brakuje karty)
"""
import argparse
import glob
import os
import re
import sys

TU = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TU)
import generuj  # noqa: E402  (ten sam katalog; sprite() i obrazy_h())

SLOT_W, SLOT_H = 240, 205


def zrodla():
    """ID sceny → ścieżka sceny źródłowej (np. M05 → zrodlo/M05_rocznica-terapii.png); plansza kontrolna pominięta."""
    w = {}
    for p in sorted(glob.glob(os.path.join(generuj.KATALOG_ZRODEL, "*.png"))):
        m = re.match(r"([A-Z]\d\d)(?:[._])", os.path.basename(p))
        if m:
            assert m.group(1) not in w, "dwa źródła tej samej sceny: " + m.group(1)
            w[m.group(1)] = p
    return w


def plik_karty(zrodlo):
    """Ścieżka bitmapy karty dla sceny źródłowej: nazwa bez .png + -240x205.png."""
    nazwa = os.path.splitext(os.path.basename(zrodlo))[0]
    return os.path.join(generuj.KATALOG_SCEN, nazwa + "-240x205.png")


def karta(zrodlo):
    """Bitmapa karty (PIL, tryb 1) ze sceny źródłowej."""
    return generuj.sprite(zrodlo, SLOT_W, SLOT_H, True)


def braki():
    """ID scen źródłowych bez bitmapy karty."""
    karty = generuj.sceny()
    return [n for n in zrodla() if n not in karty]


def main(argv=None):
    a = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    a.add_argument("--sprawdz", action="store_true", help="tylko kontrola kompletu, bez zapisu")
    a.add_argument("--nadpisz", action="store_true", help="przelicz też istniejące bitmapy karty")
    a.add_argument("--bez-obrazow", action="store_true", help="bez odświeżenia include/obrazy.h")
    a = a.parse_args(argv)
    zr = zrodla()
    if a.sprawdz:
        b = braki()
        print(f"sceny karty: {len(zr) - len(b)} z {len(zr)} scen źródłowych" + (f"; brak: {', '.join(b)}" if b else ""))
        return 1 if b else 0
    nowe = []
    for n, p in zr.items():
        cel = plik_karty(p)
        if n in generuj.sceny() and not a.nadpisz:
            continue
        karta(p).save(cel)
        nowe.append(n)
    print(f"sceny karty: zapisano {len(nowe)}" + (f" ({', '.join(nowe)})" if nowe else "") + f", komplet {len(zr)}")
    if not a.bez_obrazow:
        generuj.obrazy_h()
    return 0


if __name__ == "__main__":
    sys.exit(main())
