#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bramka clang-tidy „zero nowych ostrzeżeń”.

Co robi
-------
Czyta wyjście clang-tidy (plik albo stdin), zamienia każde ostrzeżenie na klucz
niezależny od numeru linii: `plik | reguła | treść linii kodu bez wcięć`
(numery linii przesuwają się z każdą zmianą, treść linii nie). To samo
ostrzeżenie z nagłówka wypisane przez kilka plików .cpp liczy się raz
(plik, linia, kolumna, reguła). Porównuje z plikiem bazy w repo:

  - ostrzeżenie, którego nie ma w bazie (albo jest go więcej niż w bazie), to błąd;
  - ostrzeżenie z bazy, którego już nie ma, to informacja (bazę można zmniejszyć
    przez --zapisz).

Dodatkowo każdy komentarz NOLINT w plikach src/ musi mieć nazwę reguły w
nawiasie i powód po niej (`// NOLINT(cert-err34-c): powód`); goły NOLINT to błąd.

Użycie
------
  clang-tidy … > tidy.txt
  python3 tools/jakosc/porownaj_tidy.py tidy.txt                 # porównanie z bazą
  python3 tools/jakosc/porownaj_tidy.py tidy.txt --zapisz        # nowa baza

Pusty wynik przy niepustej bazie (bez --zapisz) to błąd: clang-tidy najpewniej nie ruszył.

Kod wyjścia: 0 = brak nowych ostrzeżeń, 1 = są nowe, goły NOLINT albo pusty wynik, 2 = błąd odczytu.
Tylko biblioteka standardowa Pythona 3.
"""

import argparse
import collections
import glob
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FW = os.path.join(REPO, "firmware", "prototyp")
BAZA = os.path.join(REPO, "tools", "jakosc", "clang-tidy-baza.txt")

OSTRZEZENIE_RE = re.compile(r"^(?P<plik>[^\s:][^:]*):(?P<linia>\d+):(?P<kol>\d+): warning: (?P<tresc>.*?) "
                            r"\[(?P<regula>[\w.,-]+)\]$")
NOLINT_RE = re.compile(r"NOLINT(?:NEXTLINE|BEGIN|END)?(?P<reszta>.*)$")
NOLINT_OK_RE = re.compile(r"^\([\w.,*-]+\)\s*:?\s*\S.{3,}")


def wzgledna(sciezka, repo=REPO):
    s = os.path.normpath(sciezka)
    if os.path.isabs(s):
        s = os.path.relpath(s, repo)
    return s.replace(os.sep, "/")


def klucze(tekst, czytaj_linie, repo=REPO):
    """Lista kluczy (z powtórzeniami) z wyjścia clang-tidy.
    czytaj_linie(ścieżka względna) -> lista linii pliku albo None."""
    widziane = set()
    wynik = []
    for linia in tekst.splitlines():
        m = OSTRZEZENIE_RE.match(linia.strip())
        if not m:
            continue
        plik = wzgledna(m.group("plik"), repo)
        if not plik.startswith("firmware/prototyp/"):
            continue  # nagłówki systemowe i atrapy poza bramką
        miejsce = (plik, m.group("linia"), m.group("kol"), m.group("regula"))
        if miejsce in widziane:
            continue
        widziane.add(miejsce)
        zrodlo = czytaj_linie(plik)
        nr = int(m.group("linia"))
        kod = zrodlo[nr - 1].strip() if zrodlo and 0 < nr <= len(zrodlo) else "?"
        wynik.append("%s | %s | %s" % (plik, m.group("regula"), kod))
    return wynik


def czytnik(repo=REPO):
    pamiec = {}

    def czytaj(plik):
        if plik not in pamiec:
            try:
                with open(os.path.join(repo, plik), encoding="utf-8", errors="replace") as f:
                    pamiec[plik] = f.read().split("\n")
            except OSError:
                pamiec[plik] = None
        return pamiec[plik]
    return czytaj


def porownaj(obecne, baza):
    """(nowe, ustapione): listy kluczy ponad bazę i z bazy, których już nie ma (z krotnością)."""
    o, b = collections.Counter(obecne), collections.Counter(baza)
    nowe = sorted((o - b).elements())
    ustapione = sorted((b - o).elements())
    return nowe, ustapione


def gole_nolint(pliki):
    """[(plik, nr, linia)] z NOLINT bez reguły w nawiasie albo bez powodu."""
    wynik = []
    for p in pliki:
        with open(p, encoding="utf-8", errors="replace") as f:
            for nr, linia in enumerate(f, start=1):
                m = NOLINT_RE.search(linia)
                if m and not NOLINT_OK_RE.match(m.group("reszta").strip()):
                    wynik.append((wzgledna(p), nr, linia.strip()))
    return wynik


def wczytaj_baze(sciezka):
    with open(sciezka, encoding="utf-8") as f:
        return [l.rstrip("\n") for l in f if l.strip() and not l.startswith("#")]


def main(argv=None):
    ap = argparse.ArgumentParser(description="Bramka clang-tidy: zero nowych ostrzeżeń wobec bazy.")
    ap.add_argument("wyjscie", help="plik z wyjściem clang-tidy (- = stdin)")
    ap.add_argument("--baza", default=BAZA)
    ap.add_argument("--zapisz", action="store_true", help="zapisz obecne ostrzeżenia jako nową bazę")
    a = ap.parse_args(argv)
    try:
        tekst = sys.stdin.read() if a.wyjscie == "-" else open(a.wyjscie, encoding="utf-8", errors="replace").read()
    except OSError as e:
        print("Nie da się odczytać: %s" % e, file=sys.stderr)
        return 2
    obecne = klucze(tekst, czytnik())
    if a.zapisz:
        with open(a.baza, "w", encoding="utf-8") as f:
            f.write("# Baza ostrzeżeń clang-tidy rdzenia (profil firmware/prototyp/.clang-tidy).\n"
                    "# Klucz: plik | reguła | treść linii. Zapis: tools/jakosc/sprawdz.sh tidy --zapisz.\n")
            for k in sorted(obecne):
                f.write(k + "\n")
        print("clang-tidy: zapisano bazę, %d ostrzeżeń: %s" % (len(obecne), wzgledna(a.baza)))
        return 0
    try:
        baza = wczytaj_baze(a.baza)
    except OSError as e:
        print("Nie da się odczytać bazy: %s" % e, file=sys.stderr)
        return 2
    if not obecne and baza:
        # pusty wynik przy niepustej bazie to prawie na pewno clang-tidy, który nie ruszył;
        # naprawdę czysty kod zapisuje się jawnie przez --zapisz
        print("  BŁĄD: clang-tidy nie zgłosił żadnego ostrzeżenia, a baza ma %d; sprawdź, czy narzędzie działało "
              "(przy naprawdę czystym kodzie: --zapisz)" % len(baza))
        return 1
    nowe, ustapione = porownaj(obecne, baza)
    nolint = gole_nolint(sorted(glob.glob(os.path.join(FW, "src", "*.cpp")) + glob.glob(os.path.join(FW, "src", "*.h"))))
    for k in nowe:
        print("  NOWE: " + k)
    for p, nr, l in nolint:
        print("  BŁĄD: %s:%d: NOLINT bez reguły w nawiasie albo bez powodu: %s" % (p, nr, l))
    for k in ustapione:
        print("  ustąpiło (można zmniejszyć bazę): " + k)
    print("clang-tidy: %d ostrzeżeń, baza %d, nowych %d, ustąpiło %d, gołych NOLINT %d"
          % (len(obecne), len(baza), len(nowe), len(ustapione), len(nolint)))
    return 1 if nowe or nolint else 0


if __name__ == "__main__":
    sys.exit(main())
