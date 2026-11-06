#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Wersja specyfikacji do czytania (po polsku).

Specyfikacja jest źródłem prawdy i zawiera dużo historii: kolumny „Źródło”
w tabelach, dopiski w nawiasach o audycie i recenzjach, rozdziały 10.2
(wycofane ID) i 10.3 (historia wersji). Ten skrypt robi z niej kopię do
czytania, np. dla lekarzy konsultujących projekt, bez tej historii. Kopii się nie
edytuje: każda zmiana idzie do specyfikacji, a kopię generuje się od nowa.

Co skrypt usuwa:
  1. kolumnę o nagłówku dokładnie „Źródło” z każdej tabeli (kolumny typu
     „Próg: źródło i poziom dowodu” zostają, bo to treść);
  2. w nawiasach okrągłych poza `kodem` fragmenty, które są tylko odwołaniem
     do historii (audyt, recenzja, odpowiedź autora, raport audytu, plik
     roboczy), bez fragmentów z dwukropkiem (te niosą wyjaśnienie); gdy
     w nawiasie nic nie zostaje, znika cały nawias; status „zablokowane do
     pomiaru …” i źródło dowodu progu („ATS 2013, W”) z usuwanej kolumny „Źródło”
     przechodzą do kolumny „Wymaganie”; ID z usuwanego fragmentu nawiasu zostaje;
  3. rozdziały 10.2 i 10.3 (zostaje jedno zdanie z odesłaniem);
  4. długie wiersze „Wersja” i „Status” w tabeli nagłówka (zostaje numer
     wersji i pierwsze zdanie statusu).

Użycie:
  python3 tools/wersja_do_czytania.py [spec] [-o WYJŚCIE]
Domyślnie: docs/spec/specyfikacja-funkcjonalna.md → build/specyfikacja-do-czytania.md
"""
import argparse
import os
import re
import sys

DOMYSLNA_SPEC = "docs/spec/specyfikacja-funkcjonalna.md"
DOMYSLNE_WYJSCIE = "build/specyfikacja-do-czytania.md"

# Fragment nawiasu, który jest tylko historią (sprawdzane od początku fragmentu).
_HISTORIA = re.compile(
    r"^\s*(?:"
    r"audyt\b|audytu\b|"
    r"(?:pierwsza |druga |trzecia |czwarta )?recenzj[aię]\b|"
    r"po recenzji\b|uwaga z recenzji\b|"
    r"odpowied[źz] (?:autora|na recenzj)|"
    r"raport \d+\b|raport audytu\b|"
    r"plik roboczy\b|plik 0\d\b|"
    r"decyzja właściciela(?: projektu)?\b"
    r")",
    re.IGNORECASE,
)


_STATUS = re.compile(r"zablokowane do pomiaru(?: \d+(?: i \d+)?| na egzemplarzu referencyjnym)?")
# Źródło dowodu progu w kolumnie „Źródło” (np. „ATS 2013, W”, „Liu, Chest 2017, A”,
# „Javaheri 2009, Cassel 2011, B”): treść, nie historia.
_DOWOD = re.compile(r"(?<![\w-])[A-ZŁŚŻ][\w-]+(?:, [A-Z]\w+)? (?:19|20)\d{2}(?:, [A-ZŁŚŻ][\w-]+ (?:19|20)\d{2})*, [ABCDWP](?![\w])")
try:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from spojnosc import znajdz_id as _znajdz_id
except Exception:  # pragma: no cover
    _znajdz_id = None


def _id_w(tekst):
    return [i for i, _ in _znajdz_id(tekst)] if _znajdz_id else []


def podziel_wiersz(linia):
    """Dzieli wiersz tabeli markdown na komórki; pomija | w `kodzie` i \\|."""
    komorki, biezaca, w_kodzie, i = [], [], False, 0
    tekst = linia.strip()
    if tekst.startswith("|"):
        tekst = tekst[1:]
    if tekst.endswith("|") and not tekst.endswith("\\|"):
        tekst = tekst[:-1]
    while i < len(tekst):
        z = tekst[i]
        if z == "\\" and i + 1 < len(tekst) and tekst[i + 1] == "|":
            biezaca.append("\\|")
            i += 2
            continue
        if z == "`":
            w_kodzie = not w_kodzie
        if z == "|" and not w_kodzie:
            komorki.append("".join(biezaca))
            biezaca = []
        else:
            biezaca.append(z)
        i += 1
    komorki.append("".join(biezaca))
    return komorki


def zloz_wiersz(komorki):
    return "|" + "|".join(komorki) + "|"


def czy_wiersz_tabeli(linia):
    return linia.lstrip().startswith("|")


def usun_kolumny_zrodla(linie, raport):
    """Usuwa kolumnę „Źródło” z każdej tabeli. Tabela z niespójną liczbą
    komórek zostaje bez zmian i trafia do raportu jako ostrzeżenie."""
    wynik, i = [], 0
    while i < len(linie):
        if not czy_wiersz_tabeli(linie[i]):
            wynik.append(linie[i])
            i += 1
            continue
        j = i
        while j < len(linie) and czy_wiersz_tabeli(linie[j]):
            j += 1
        blok = linie[i:j]
        naglowek = [k.strip() for k in podziel_wiersz(blok[0])]
        if "Źródło" in naglowek and len(blok) >= 2:
            idx = naglowek.index("Źródło")
            wiersze = [podziel_wiersz(w) for w in blok]
            if all(len(w) == len(naglowek) for w in wiersze):
                cel = naglowek.index("Wymaganie") if "Wymaganie" in naglowek else max(idx - 1, 0)
                for w in wiersze:
                    # status blokady (np. „zablokowane do pomiaru 21”) jest treścią, nie historią
                    statusy = _STATUS.findall(w[idx])
                    if statusy and cel != idx:
                        w[cel] = w[cel].rstrip() + " (status: %s) " % "; ".join(dict.fromkeys(statusy))
                    dowody = _DOWOD.findall(w[idx])
                    if dowody and cel != idx:
                        w[cel] = w[cel].rstrip() + " (źródło: %s) " % "; ".join(dict.fromkeys(dowody))
                    del w[idx]
                blok = [zloz_wiersz(w) for w in wiersze]
                raport["tabele"] += 1
            else:
                raport["ostrzezenia"].append(
                    "l. %d: tabela z kolumną „Źródło” ma niespójną liczbę komórek, zostaje bez zmian" % (i + 1))
        wynik.extend(blok)
        i = j
    return wynik


def wyczysc_nawiasy(linia, raport):
    """Usuwa z nawiasów (bez zagnieżdżeń) fragmenty, które są tylko historią."""
    def zamien(m):
        wnetrze = m.group(1)
        if "`" in wnetrze:
            return m.group(0)
        czesci = wnetrze.split(";")
        # fragment z dwukropkiem niesie wyjaśnienie (np. „audyt A-76: bez nawilżania…”), więc zostaje
        zostaja = []
        for c in czesci:
            if _HISTORIA.match(c) and ":" not in c:
                ids = _id_w(c)
                if ids:
                    zostaja.append(" " + ", ".join(dict.fromkeys(ids)) if zostaja else ", ".join(dict.fromkeys(ids)))
            else:
                zostaja.append(c)
        if zostaja == czesci:
            return m.group(0)
        raport["nawiasy"] += 1
        if not zostaja:
            return "\x00"
        return "(" + ";".join(zostaja).strip() + ")"
    # nawiasy w `kodzie` (teksty ekranów) zostają bez zmian
    czesci = linia.split("`")
    for k in range(0, len(czesci), 2):
        czesci[k] = re.sub(r"\(([^()]*)\)", zamien, czesci[k])
    nowa = "`".join(czesci)
    nowa = re.sub(r" ?\x00", "", nowa)
    if not linia.startswith(" "):
        nowa = nowa.lstrip(" ")
    return nowa


def skroc_naglowek(linie):
    wynik = []
    for linia in linie:
        if linia.startswith("| Wersja | "):
            m = re.search(r"\*\*([^*]+)\*\*", linia)
            if m:
                linia = "| Wersja | **%s** |" % m.group(1)
        elif linia.startswith("| Status | "):
            tresc = linia[len("| Status | "):].rstrip(" |")
            linia = "| Status | %s |" % tresc.split(";")[0].strip()
        wynik.append(linia)
    return wynik


def usun_historie(linie):
    wynik, pomijaj = [], False
    for linia in linie:
        if linia.startswith("### 10.2 "):
            pomijaj = True
            wynik.append("### 10.2 i 10.3 Wycofane ID i historia wersji")
            wynik.append("")
            wynik.append("W wersji do czytania pominięte. Pełna treść: specyfikacja w repozytorium, rozdziały 10.2 i 10.3, oraz CHANGELOG.md.")
            wynik.append("")
            continue
        if pomijaj and (linia.startswith("## ") or linia.startswith("---")):
            pomijaj = False
        if not pomijaj:
            wynik.append(linia)
    return wynik


def przetworz(tekst):
    raport = {"tabele": 0, "nawiasy": 0, "ostrzezenia": []}
    linie = tekst.split("\n")
    linie = skroc_naglowek(linie)
    linie = usun_historie(linie)
    linie = usun_kolumny_zrodla(linie, raport)
    linie = [wyczysc_nawiasy(l, raport) for l in linie]
    uwaga = ("> **Wersja do czytania**, wygenerowana skryptem `tools/wersja_do_czytania.py`. "
             "Bez kolumn „Źródło”, bez rozdziałów 10.2 i 10.3 i bez większości dopisków o audycie i recenzjach "
             "(zostają te, które niosą wyjaśnienie albo stoją poza nawiasem). "
             "Nie edytować: źródłem prawdy jest `docs/spec/specyfikacja-funkcjonalna.md`.")
    if linie and linie[0].startswith("# "):
        linie = [linie[0], "", uwaga] + linie[1:]
    else:
        linie = [uwaga, ""] + linie
    return "\n".join(linie), raport


def main(argv=None):
    ap = argparse.ArgumentParser(description="Wersja specyfikacji do czytania, bez historii.")
    ap.add_argument("spec", nargs="?", default=DOMYSLNA_SPEC)
    ap.add_argument("-o", "--wyjscie", default=DOMYSLNE_WYJSCIE)
    a = ap.parse_args(argv)
    tekst = open(a.spec, encoding="utf-8").read()
    wynik, raport = przetworz(tekst)
    os.makedirs(os.path.dirname(a.wyjscie) or ".", exist_ok=True)
    open(a.wyjscie, "w", encoding="utf-8").write(wynik)
    print("Zapisano: %s" % a.wyjscie)
    print("Rozmiar: %d → %d znaków (%.0f%%)" % (len(tekst), len(wynik), 100.0 * len(wynik) / len(tekst)))
    print("Tabele bez kolumny „Źródło”: %d; nawiasy oczyszczone: %d" % (raport["tabele"], raport["nawiasy"]))
    for o in raport["ostrzezenia"]:
        print("OSTRZEŻENIE: " + o)
    return 0


if __name__ == "__main__":
    sys.exit(main())
