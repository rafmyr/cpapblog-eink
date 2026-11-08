#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bazy bramek wobec gałęzi bazowej (domyślnie `main`).

Co robi
-------
Bazy `tools/jakosc/clang-tidy-baza.txt` i `tools/jakosc/pokrycie-baza.txt` leżą
w repo i PR może je przepisać (`sprawdz.sh … --zapisz`). Ten skrypt porównuje
je z wersją z punktu odniesienia: `git merge-base <gałąź bazowa> HEAD`. Gałąź
bazowa to `--galaz`, a bez niej zmienna środowiska `BAZA_GALAZ`, a bez niej
`origin/main`. Gałąź, która odchodzi od innej gałęzi integracji niż `main`,
porównuje się z nią (np. `--galaz origin/moja-integracja`).

  - baza clang-tidy z kluczem, którego nie ma w bazie z `main` (albo z większą
    krotnością), czyli pogorszenie, wymaga uzasadnienia;
  - baza pokrycia niższa niż na `main` (porównanie dokładne: pokryte/wszystkie)
    wymaga uzasadnienia;
  - ulepszenie (mniej ostrzeżeń, wyższe pokrycie) i baza, której na `main`
    jeszcze nie ma, przechodzą bez wpisu.

Uzasadnienie: linia `zmiana bazy: <plik> — powód` (zamiast „—” wolno „:”
albo „-”; plik pełną ścieżką albo samą nazwą, także w odwrotnych apostrofach),
powód co najmniej 4 znaki, w opisie dowolnego commitu od punktu odniesienia
do HEAD albo w linii CHANGELOG.md dopisanej w tej gałęzi (diff wobec punktu
odniesienia; stare wpisy w CHANGELOG nie zwalniają następnych pogorszeń).

Użycie
------
  python3 tools/jakosc/porownaj_bazy.py [--galaz GAŁĄŹ] [--ref REF]

Kod wyjścia: 0 = bazy nie gorsze albo pogorszenie z uzasadnieniem, 1 = pogorszenie
bez wpisu, 2 = nie da się ustalić punktu odniesienia (np. brak gałęzi bazowej).
"""

import argparse
import collections
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GALAZ_DOMYSLNA = "origin/main"
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


def galaz_bazowa(arg=None, srodowisko=None):
    """Gałąź bazowa: argument, potem BAZA_GALAZ, potem origin/main (pusta wartość = brak)."""
    srodowisko = os.environ if srodowisko is None else srodowisko
    return (arg or srodowisko.get("BAZA_GALAZ") or GALAZ_DOMYSLNA).strip()


def dopisane_linie(diff):
    """Linie dopisane w wyjściu `git diff` (bez nagłówka `+++`), jako jeden tekst."""
    return "\n".join(l[1:] for l in (diff or "").split("\n") if l.startswith("+") and not l.startswith("+++"))


def sprawdz(obecne, na_main, opisy, galaz="main"):
    """obecne, na_main: {plik: tekst albo None} (na_main = baza z gałęzi bazowej); zwraca (błędy, informacje)."""
    bledy, info = [], []
    for plik in (TIDY, POKRYCIE):
        teraz, przed = obecne.get(plik), na_main.get(plik)
        if teraz is None:
            bledy.append("%s: brak pliku bazy w gałęzi." % plik)
            continue
        if przed is None:
            info.append("%s: nowa baza (na %s jej nie ma), bez porównania." % (plik, galaz))
            continue
        if plik == TIDY:
            gorsze = gorsza_tidy(teraz, przed)
            opis = ("%d ostrzeżeń ponad bazę z %s: %s" % (len(gorsze), galaz, "; ".join(gorsze[:3]))
                    if gorsze else "")
        else:
            opis = ("pokrycie bazy %d/%d niższe niż na %s %d/%d" % (pokrycie(teraz) + (galaz,) + pokrycie(przed))
                    if gorsze_pokrycie(teraz, przed) else "")
        if not opis:
            continue
        if ma_wpis(plik, opisy):
            info.append("%s: pogorszona (%s), z wpisem „zmiana bazy:”." % (plik, opis))
        else:
            bledy.append("%s: pogorszona bez linii „zmiana bazy: %s — powód” w opisie commitu albo w CHANGELOG (%s)."
                         % (plik, plik, opis))
    return bledy, info


def main(argv=None):
    ap = argparse.ArgumentParser(description="Bazy bramek wobec gałęzi bazowej.")
    ap.add_argument("--galaz", help="gałąź bazowa (domyślnie $BAZA_GALAZ albo %s)" % GALAZ_DOMYSLNA)
    ap.add_argument("--ref", help="punkt odniesienia wprost (zamiast merge-base gałęzi bazowej i HEAD)")
    a = ap.parse_args(argv)
    galaz = galaz_bazowa(a.galaz)
    ref = a.ref or (git("merge-base", galaz, "HEAD") or "").strip()
    if not ref:
        print("Bazy: nie da się ustalić merge-base z %s (git fetch origin %s)." % (galaz, galaz.split("/", 1)[-1]),
              file=sys.stderr)
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
    opisy.append(dopisane_linie(git("diff", ref, "--", "CHANGELOG.md")))  # wpisy dopisane w tej gałęzi
    nazwa = a.ref[:7] if a.ref else galaz
    bledy, info = sprawdz(obecne, na_main, opisy, nazwa)
    for i in info:
        print("  " + i)
    for b in bledy:
        print("  BŁĄD: " + b)
    print("bazy wobec %s (%s): %d błędów" % (nazwa, ref[:7], len(bledy)))
    return 1 if bledy else 0


if __name__ == "__main__":
    sys.exit(main())
