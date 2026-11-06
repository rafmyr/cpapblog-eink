#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bazy bramek wobec `main`.

Co robi
-------
Bazy `tools/jakosc/clang-tidy-baza.txt` i `tools/jakosc/pokrycie-baza.txt` leżą
w repo i PR może je przepisać (`sprawdz.sh … --zapisz`). Ten skrypt porównuje
je z wersją z punktu odniesienia (domyślnie `git merge-base origin/main HEAD`):

  - baza clang-tidy z kluczem, którego nie ma w bazie z `main` (albo z większą
    krotnością), czyli pogorszenie, wymaga uzasadnienia w opisie commitu;
  - baza pokrycia niższa niż na `main` (porównanie dokładne: pokryte/wszystkie)
    wymaga uzasadnienia w opisie commitu;
  - ulepszenie (mniej ostrzeżeń, wyższe pokrycie) i baza, której na `main`
    jeszcze nie ma, przechodzą bez wpisu.

Uzasadnienie: w opisie dowolnego commitu od punktu odniesienia do HEAD linia
`zmiana bazy: <plik> — powód` (zamiast „—” wolno „:” albo „-”; plik pełną
ścieżką albo samą nazwą, także w odwrotnych apostrofach), powód co najmniej
4 znaki.

Użycie
------
  python3 tools/jakosc/porownaj_bazy.py [--ref REF]

Kod wyjścia: 0 = bazy nie gorsze albo pogorszenie z uzasadnieniem, 1 = pogorszenie
bez wpisu, 2 = nie da się ustalić punktu odniesienia (np. brak origin/main).
"""

import argparse
import collections
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TIDY = "tools/jakosc/clang-tidy-baza.txt"
POKRYCIE = "tools/jakosc/pokrycie-baza.txt"


def klucze_tidy(tekst):
    return [l for l in tekst.split("\n") if l.strip() and not l.startswith("#")]


def pokrycie(tekst):
    """(pokryte, wszystkie) z pliku bazy pokrycia."""
    p = tekst.split()
    return int(p[0]), int(p[1])


def gorsza_tidy(obecna, na_main):
    """Klucze ponad bazę z main (z krotnością)."""
    return sorted((collections.Counter(klucze_tidy(obecna)) - collections.Counter(klucze_tidy(na_main))).elements())


def gorsze_pokrycie(obecna, na_main):
    (p, w), (pm, wm) = pokrycie(obecna), pokrycie(na_main)
    return w > 0 and wm > 0 and p * wm < pm * w  # p/w < pm/wm bez zaokrąglenia


def ma_wpis(plik, opisy):
    """Czy któryś opis commitu ma linię „zmiana bazy: <plik> — powód”."""
    nazwy = "|".join(re.escape(n) for n in (plik, os.path.basename(plik)))
    wzor = re.compile(r"zmiana bazy:\s*`?(?:%s)`?\s*(?:—|–|-|:)\s*\S.{3,}" % nazwy, re.I)
    return any(wzor.search(t) for t in opisy)


def git(*args):
    w = subprocess.run(["git", "-C", REPO] + list(args), stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return w.stdout.decode("utf-8", "replace") if w.returncode == 0 else None


def sprawdz(obecne, na_main, opisy):
    """obecne, na_main: {plik: tekst albo None}; zwraca (błędy, informacje)."""
    bledy, info = [], []
    for plik in (TIDY, POKRYCIE):
        teraz, przed = obecne.get(plik), na_main.get(plik)
        if teraz is None:
            bledy.append("%s: brak pliku bazy w gałęzi." % plik)
            continue
        if przed is None:
            info.append("%s: nowa baza (na main jej nie ma), bez porównania." % plik)
            continue
        if plik == TIDY:
            gorsze = gorsza_tidy(teraz, przed)
            opis = "%d ostrzeżeń ponad bazę z main: %s" % (len(gorsze), "; ".join(gorsze[:3])) if gorsze else ""
        else:
            opis = ("pokrycie bazy %d/%d niższe niż na main %d/%d" % (pokrycie(teraz) + pokrycie(przed))
                    if gorsze_pokrycie(teraz, przed) else "")
        if not opis:
            continue
        if ma_wpis(plik, opisy):
            info.append("%s: pogorszona (%s), z uzasadnieniem w opisie commitu." % (plik, opis))
        else:
            bledy.append("%s: pogorszona bez linii „zmiana bazy: %s — powód” w opisie commitu (%s)."
                         % (plik, plik, opis))
    return bledy, info


def main(argv=None):
    ap = argparse.ArgumentParser(description="Bazy bramek wobec main.")
    ap.add_argument("--ref", help="punkt odniesienia (domyślnie merge-base origin/main HEAD)")
    a = ap.parse_args(argv)
    ref = a.ref or (git("merge-base", "origin/main", "HEAD") or "").strip()
    if not ref:
        print("Bazy: nie da się ustalić merge-base z origin/main (git fetch origin main).", file=sys.stderr)
        return 2
    obecne, na_main = {}, {}
    for plik in (TIDY, POKRYCIE):
        try:
            with open(os.path.join(REPO, plik), encoding="utf-8") as f:
                obecne[plik] = f.read()
        except OSError:
            obecne[plik] = None
        na_main[plik] = git("show", "%s:%s" % (ref, plik))
    opisy = [(git("log", "--format=%B", "%s..HEAD" % ref) or "")]
    bledy, info = sprawdz(obecne, na_main, opisy)
    for i in info:
        print("  " + i)
    for b in bledy:
        print("  BŁĄD: " + b)
    print("bazy wobec %s: %d błędów" % (ref[:7], len(bledy)))
    return 1 if bledy else 0


if __name__ == "__main__":
    sys.exit(main())
