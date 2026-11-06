#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Krój slotu dziennego: Source Serif 4, 72 px, 600, tylko znaki liczb faktów S-K-34 i S-K-39
(`−4°`, `10%`, `20`, `1 h 10`; kanwa SK34-jutro, SK39-drzemka-wyciek: `font-size 72, font-weight 600`).

Osobny plik firmware/prototyp/include/fonty_slot.h, żeby nie przegenerowywać fonty.h (inne z niego
korzystają, a nowe wersje plików TTF zmieniłyby glify wszystkich krojów). Te same funkcje i mapowanie znaków co
tools/prototyp/generuj.py (kody od 0x7F według EXTRA), więc tekst() z src/ekrany.cpp rysuje ten krój bez zmian;
szerokości bez tablicy 1/16 px (zapas xAdvance), co przy liczbach nie ma znaczenia.
Użycie: python3 tools/prototyp/generuj_slot.py --serif SourceSerif4[opsz,wght].ttf --sans PublicSans[wght].ttf
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generuj as g  # noqa: E402

KROJE = [("SN72", "serif", 72, 600, "0123456789−°%h,–:")]


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--serif", required=True)
    a.add_argument("--sans", required=True)
    a = a.parse_args()
    pliki = {"serif": a.serif, "sans": a.sans}
    czesci = [g.LICENCJA_FONTY_SLOT, g.NAGLOWEK, "#include <gfxfont.h>",
              "// krój slotu dziennego: wygenerowane przez tools/prototyp/generuj_slot.py, nie edytować ręcznie"]
    for nazwa, kroj, px, waga, tylko in KROJE:
        kolejnosc = [kroj, "sans"]
        kroje = [(g.wczytaj(pliki[k], px, waga), g.wczytaj(pliki[k], px * g.SS, waga, opsz=px)) for k in kolejnosc]
        c, brak, b = g.font_c(nazwa, kroje, [g.cmap(pliki[k]) for k in kolejnosc], px, g.PROG_DUZY, tylko)
        if brak:
            raise SystemExit(f"{nazwa}: brak glifów {''.join(brak)}")
        czesci.append(c.replace("tools/prototyp/generuj.py", "tools/prototyp/generuj_slot.py"))
        print(f"{nazwa}: {b} B")
    open(os.path.join(g.WYJ, "fonty_slot.h"), "w").write("\n".join(czesci) + "\n")


if __name__ == "__main__":
    main()
