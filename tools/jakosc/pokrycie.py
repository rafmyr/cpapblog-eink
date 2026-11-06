#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Zapadka pokrycia linii rdzenia.

Co robi
-------
Czyta raporty gcov w formacie JSON (`gcov --json-format --stdout`, gcc 9+)
z kompilacji testów z `--coverage` i liczy pokrycie linii plików
firmware/prototyp/src/ (bez main.cpp, który nie wchodzi do testów rdzenia).
Linia z nagłówka widziana w kilku plikach .cpp liczy się raz (wykonana,
gdy wykonana w którymkolwiek). Porównuje z bazą w repo: pokrycie poniżej
bazy (dokładnie, z liczby linii pokrytych i wszystkich) to błąd.

Użycie
------
  gcov --json-format --stdout -o OBJ src/*.cpp > gcov.json   (robi to tools/jakosc/sprawdz.sh pokrycie)
  python3 tools/jakosc/pokrycie.py gcov.json            # porównanie z bazą
  python3 tools/jakosc/pokrycie.py gcov.json --zapisz   # nowa baza

Plik bazy: jedna linia `pokryte wszystkie procent`, np. `2897 3102 93.39`.
Kod wyjścia: 0 = nie mniej niż baza, 1 = spadek, 2 = błąd odczytu.
"""

import argparse
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BAZA = os.path.join(REPO, "tools", "jakosc", "pokrycie-baza.txt")
PRZEDROSTEK = "firmware/prototyp/src/"


def _wzgledna(p, katalog):
    if not os.path.isabs(p):
        p = os.path.join(katalog, p)
    return os.path.relpath(os.path.normpath(p), REPO).replace(os.sep, "/")


def linie(tekst):
    """{plik: {nr: wykonana}} z sklejonych dokumentów JSON gcov (jeden na linię)."""
    wynik = {}
    for kawalek in tekst.splitlines():
        kawalek = kawalek.strip()
        if not kawalek.startswith("{"):
            continue
        dok = json.loads(kawalek)
        katalog = dok.get("current_working_directory", REPO)
        for f in dok.get("files", []):
            plik = _wzgledna(f["file"], katalog)
            if not plik.startswith(PRZEDROSTEK) or plik.endswith("/main.cpp"):
                continue
            mapa = wynik.setdefault(plik, {})
            for l in f.get("lines", []):
                nr = l["line_number"]
                mapa[nr] = mapa.get(nr, False) or l.get("count", 0) > 0
    return wynik


def suma(mapy):
    wszystkie = sum(len(m) for m in mapy.values())
    pokryte = sum(sum(1 for v in m.values() if v) for m in mapy.values())
    return pokryte, wszystkie, (round(100.0 * pokryte / wszystkie, 2) if wszystkie else 0.0)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Zapadka pokrycia linii rdzenia.")
    ap.add_argument("gcov_json", help="wyjście gcov --json-format --stdout (- = stdin)")
    ap.add_argument("--baza", default=BAZA)
    ap.add_argument("--zapisz", action="store_true")
    ap.add_argument("--pliki", action="store_true", help="wypisz pokrycie każdego pliku")
    a = ap.parse_args(argv)
    try:
        tekst = sys.stdin.read() if a.gcov_json == "-" else open(a.gcov_json, encoding="utf-8").read()
        mapy = linie(tekst)
    except (OSError, ValueError) as e:
        print("Nie da się odczytać raportu gcov: %s" % e, file=sys.stderr)
        return 2
    if not mapy:
        print("Pokrycie: brak plików rdzenia w raporcie gcov.", file=sys.stderr)
        return 2
    pokryte, wszystkie, procent = suma(mapy)
    if a.pliki:
        for plik in sorted(mapy):
            p, w, pr = suma({plik: mapy[plik]})
            print("  %-40s %5d / %5d  %6.2f%%" % (plik[len(PRZEDROSTEK):], p, w, pr))
    if a.zapisz:
        with open(a.baza, "w", encoding="utf-8") as f:
            f.write("%d %d %.2f\n" % (pokryte, wszystkie, procent))
        print("pokrycie: zapisano bazę %.2f%% (%d z %d linii)" % (procent, pokryte, wszystkie))
        return 0
    try:
        with open(a.baza, encoding="utf-8") as f:
            pola = f.read().split()
            baza_p, baza_w, baza = int(pola[0]), int(pola[1]), float(pola[2])
    except (OSError, IndexError, ValueError) as e:
        print("Nie da się odczytać bazy pokrycia: %s" % e, file=sys.stderr)
        return 2
    print("pokrycie linii rdzenia: %.2f%% (%d z %d), baza %.2f%%" % (procent, pokryte, wszystkie, baza))
    if baza_w and pokryte * baza_w < baza_p * wszystkie:  # dokładnie: pokryte/wszystkie < baza_p/baza_w
        print("  BŁĄD: pokrycie spadło poniżej bazy o %.2f pkt proc." % (baza - procent))
        return 1
    if pokryte * baza_w > baza_p * wszystkie:
        print("  pokrycie wzrosło: można podnieść bazę (tools/jakosc/sprawdz.sh pokrycie --zapisz)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
