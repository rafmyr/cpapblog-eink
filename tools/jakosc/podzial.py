#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Podział programów testów rdzenia (TESTY z tools/prototyp/zrodla.sh) na części CI po zmierzonym czasie.

Sanitizery i pokrycie biegną w CI w kilku równoległych zadaniach (`tools/jakosc/sprawdz.sh <krok> --czesc k/n`).
Podział: najdłuższy program najpierw, każdy do części o najmniejszej dotąd sumie (LPT, deterministycznie: remis
wygrywa niższy numer części, potem nazwa). Program bez czasu w pliku dostaje medianę czasów kroku. Najdłuższy
program (testy_ladowarka_brzegowe_2 pod ASan, 10–18 min) trafia sam do jednej części, gdy jest dłuższy niż
średnia część.

Polecenia:
  czesc --krok K --czesc k/n -- <programy TESTY…>   programy k-tej części, w kolejności TESTY (ścieżki jak na wejściu)
  plan --krok K --czesci n -- <programy…>           tabela części z szacowanym czasem
  sprawdz --lista <plik z TESTY> <programy.txt części…>
                                                      każdy program z listy wykonany dokładnie raz w sumie części
  z-logu --krok K <log…> [--zapisz]                 czasy z wierszy `czas: <krok> <program> <s> s` (log CI albo
                                                      czasy.txt części) do tools/jakosc/czasy-testow.txt; program
                                                      w kilku logach (np. dwa przebiegi): najdłuższy z czasów, bo
                                                      czas tego samego programu w CI waha się nawet dwukrotnie

Plik czasów: wiersz `program sanitizery pokrycie` (sekundy, kompilacja testu i przebieg), `#` = komentarz.
Kod wyjścia: 0 dobrze, 1 niezgodność (sprawdz), 2 złe wywołanie.
"""
import argparse
import os
import re
import statistics
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CZASY = os.path.join(REPO, "tools", "jakosc", "czasy-testow.txt")
KROKI = ("sanitizery", "pokrycie")


def nazwa(p):
    b = os.path.basename(p.strip())
    return b[:-4] if b.endswith(".cpp") else b


def wczytaj_czasy(plik=CZASY):
    """{krok: {program: sekundy}}."""
    wynik = {k: {} for k in KROKI}
    if not os.path.exists(plik):
        return wynik
    with open(plik, encoding="utf-8") as f:
        for w in f:
            w = w.split("#", 1)[0].split()
            if not w:
                continue
            if len(w) != 1 + len(KROKI):
                raise ValueError("zły wiersz pliku czasów: %r" % " ".join(w))
            for k, v in zip(KROKI, w[1:]):
                wynik[k][w[0]] = float(v)
    return wynik


def podziel(programy, czasy, n):
    """Lista n list programów (wejściowe napisy), każdy program dokładnie raz, kolejność wejścia w części."""
    if n < 1:
        raise ValueError("liczba części < 1")
    znane = [czasy[nazwa(p)] for p in programy if nazwa(p) in czasy]
    domysl = statistics.median(znane) if znane else 1.0
    t = {p: czasy.get(nazwa(p), domysl) for p in programy}
    if len(set(programy)) != len(programy):
        raise ValueError("powtórzony program na liście")
    sumy = [0.0] * n
    przydzial = {}
    for p in sorted(programy, key=lambda p: (-t[p], nazwa(p))):
        i = min(range(n), key=lambda j: (sumy[j], j))
        przydzial[p] = i
        sumy[i] += t[p]
    czesci = [[p for p in programy if przydzial[p] == i] for i in range(n)]
    return czesci, sumy


def czesc(arg):
    m = re.fullmatch(r"([1-9][0-9]*)/([1-9][0-9]*)", arg)
    if not m or int(m.group(1)) > int(m.group(2)):
        raise ValueError("część ma mieć postać k/n z 1 ≤ k ≤ n, jest %r" % arg)
    return int(m.group(1)), int(m.group(2))


def sprawdz(lista, wykonane):
    """Błędy: program z listy niewykonany, wykonany więcej niż raz albo spoza listy."""
    oczekiwane = [nazwa(p) for p in lista if p.strip()]
    licz = {}
    for plik_czesci in wykonane:
        for p in plik_czesci:
            if p.strip():
                licz[nazwa(p)] = licz.get(nazwa(p), 0) + 1
    bledy = []
    for p in oczekiwane:
        if licz.get(p, 0) != 1:
            bledy.append("%s wykonany %d razy" % (p, licz.get(p, 0)))
    for p in sorted(set(licz) - set(oczekiwane)):
        bledy.append("%s spoza listy TESTY" % p)
    return bledy


def z_logu(teksty, krok):
    wynik = {}
    wz = re.compile(r"czas: %s (\S+) ([0-9]+) s" % krok)
    for t in teksty:
        for m in wz.finditer(t):  # program w kilku logach (kilka przebiegów): najdłuższy czas
            wynik[m.group(1)] = max(wynik.get(m.group(1), 0.0), float(m.group(2)))
    return wynik


def zapisz_czasy(czasy, plik=CZASY):
    nazwy = sorted(set().union(*[set(c) for c in czasy.values()]))
    with open(plik, "w", encoding="utf-8") as f:
        f.write("# Czasy programów testów rdzenia w CI (sekundy: kompilacja testu i przebieg), do podziału na części\n"
                "# (tools/jakosc/podzial.py). Odświeżenie z logów zadań CI: podzial.py z-logu --krok <krok> <log> --zapisz\n"
                "# program sanitizery pokrycie\n")
        for p in nazwy:
            f.write("%s %d %d\n" % (p, round(czasy["sanitizery"].get(p, 0)), round(czasy["pokrycie"].get(p, 0))))


def main(argv=None):
    ap = argparse.ArgumentParser(description="Podział testów rdzenia na części CI.")
    pod = ap.add_subparsers(dest="polecenie", required=True)
    for nazwa_p in ("czesc", "plan"):
        a = pod.add_parser(nazwa_p)
        a.add_argument("--krok", choices=KROKI, required=True)
        if nazwa_p == "czesc":
            a.add_argument("--czesc", required=True)
        else:
            a.add_argument("--czesci", type=int, required=True)
        a.add_argument("--czasy", default=CZASY)
        a.add_argument("programy", nargs="*")
    a = pod.add_parser("sprawdz")
    a.add_argument("--lista", required=True, help="plik z programami TESTY (jeden w wierszu)")
    a.add_argument("wykonane", nargs="+", help="programy.txt części")
    a = pod.add_parser("z-logu")
    a.add_argument("--krok", choices=KROKI, required=True)
    a.add_argument("--czasy", default=CZASY)
    a.add_argument("--zapisz", action="store_true")
    a.add_argument("logi", nargs="+")
    a = ap.parse_args(argv)
    try:
        if a.polecenie in ("czesc", "plan"):
            if not a.programy:
                raise ValueError("brak programów (po --)")
            czasy = wczytaj_czasy(a.czasy)[a.krok]
            if a.polecenie == "czesc":
                k, n = czesc(a.czesc)
                czesci, _ = podziel(a.programy, czasy, n)
                for p in czesci[k - 1]:
                    print(p)
            else:
                czesci, sumy = podziel(a.programy, czasy, a.czesci)
                for i, (c, s) in enumerate(zip(czesci, sumy), 1):
                    print("część %d/%d: %d programów, ok. %.1f min: %s" % (
                        i, a.czesci, len(c), s / 60, " ".join(nazwa(p) for p in c)))
        elif a.polecenie == "sprawdz":
            with open(a.lista, encoding="utf-8") as f:
                lista = f.read().splitlines()
            wykonane = []
            for p in a.wykonane:
                with open(p, encoding="utf-8") as f:
                    wykonane.append(f.read().splitlines())
            bledy = sprawdz(lista, wykonane)
            for b in bledy:
                print("BŁĄD: " + b)
            if bledy:
                return 1
            print("podział: %d programów, każdy wykonany dokładnie raz w %d częściach" % (
                len([p for p in lista if p.strip()]), len(wykonane)))
        else:
            teksty = []
            for p in a.logi:
                with open(p, encoding="utf-8", errors="replace") as f:
                    teksty.append(f.read())
            nowe = z_logu(teksty, a.krok)
            if not nowe:
                raise ValueError("brak wierszy `czas: %s …` w logach" % a.krok)
            czasy = wczytaj_czasy(a.czasy)
            czasy[a.krok].update(nowe)
            print("czasy %s: %d programów, razem %.1f min" % (a.krok, len(nowe), sum(nowe.values()) / 60))
            if a.zapisz:
                zapisz_czasy(czasy, a.czasy)
    except (OSError, ValueError) as e:
        print("BŁĄD: %s" % e, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
