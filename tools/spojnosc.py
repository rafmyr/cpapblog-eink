#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Skrypt spójności specyfikacji funkcjonalnej (cpap-eink).

Co robi
-------
Czyta specyfikację w markdownie i sprawdza, czy identyfikatory (ID) są
używane spójnie. Sprawdzenia:

  a. każde ID jest zdefiniowane dokładnie raz
     (definicja = ID w pierwszej kolumnie wiersza tabeli, której pierwsza
     kolumna nagłówka to „ID”; inne wystąpienia to użycia);
  b. każde użyte ID jest zdefiniowane albo jest na liście wycofanych;
  c. brak odwołań do ID wycofanych poza sekcją, której nagłówek zawiera
     słowo „Wycofane”; dla ID zmienionych tylko informacja;
  d. każde wymaganie R-* ma niepustą kolumnę „Weryfikacja” w postaci
     „wejście → wynik” (brak „→” to ostrzeżenie, z --strict błąd;
     pusty wynik po „→”, bez liczby, ID i tekstu, to ostrzeżenie);
     słowa EARS w treści zgadzają się
     z kolumną „Wzorzec” (Event: WHEN, State: WHILE, IF/THEN: IF i THEN,
     Optional: WHERE, Ubiquitous: żadne z nich; „A + B” łączy oba),
     niezgodność to błąd;
  e. każdy ekran jest powiązany z co najmniej jednym wymaganiem R-*
     (ostrzeżenie; z --strict błąd). Ekrany: rodziny S-K, L, U, E, O,
     ekrany z kanwy, stany i kokpit (S1…S8, K1, K2, gdy występują
     w katalogu) oraz ekrany nazwane (pytania, potwierdzenie, raport,
     „Sprzęt”) z pierwszej kolumny tabel katalogu. Powiązanie daje
     odwołanie w wierszu wymagania albo kolumna „Wymagania” katalogu.
     Wiersz katalogu z kilkoma ekranami w pierwszej kolumnie (wiersz
     zbiorczy) wiąże wymaganie tylko z tymi ekranami, które to wymaganie
     samo wymienia (źródło „katalog-zbiorczy”); tabela skrócona 9.2
     (pierwsza kolumna „Ekran albo grupa”) nie tworzy powiązań;
  f. zgodność z kanwą: artboard SKnn albo Lnn ma ID w spec i odwrotnie
     (ostrzeżenie); ekrany konfiguracji U-nn nie mają prefiksu artboardu,
     więc f ich nie sprawdza;
  g. statystyka: liczba [PRÓG], [DANE?], wymagań na prefiks;
  h. długość tekstów: szablony w odwrotnych apostrofach z kolumn
     „Tytuł”, „Powód”, „Krok”, „Linia”, „Linia na ekranie…”, „Zdanie…”
     i „Komunikat” po wstawieniu najdłuższych wartości zmiennych
     (NAJDLUZSZE) mieszczą się w limitach (LIMITY); krok w wierszu
     z „tak” w kolumnie „Krok na karcie” albo w tabeli tekstów karty
     odkryć: limit karty. Zmienna spoza NAJDLUZSZE to błąd.
  i. unikalne tytuły: ten sam tytuł w odwrotnych apostrofach (kolumna
     „Tytuł” albo tytuł z kolumny „Komunikat”) nie należy do dwóch różnych
     ekranów; właściciel = ID z pierwszej kolumny, a bez ID jej treść.

Zakresy i skróty są rozwijane do pojedynczych ID, np.
„S-K-22 do S-K-25”, „S-K-22 do 25”, „L-01…L-12”, „E-40–E-42”,
„E-30/32/34/38”, „S-K-01, 02, 04”.

Użycie
------
  python3 tools/spojnosc.py
  python3 tools/spojnosc.py docs/spec/specyfikacja-funkcjonalna.md
  python3 tools/spojnosc.py SPEC --wycofane tools/wycofane.csv \\
        --kanwa design/kanwa/canvas.json --strict --json \\
        --macierz macierz.csv

  --strict    brak wymagania dla ekranu (e) liczy się jako błąd
  --json      wynik jako JSON zamiast raportu tekstowego
  --macierz   zapisuje CSV macierzy śledzenia z kolumnami
              „wymaganie, id_odwolania, rodzina_odwolania, ekran,
              sekcja_wymagania, zrodlo_powiazania, przypadek_testowy”
              (ekran = tak dla ekranów z sprawdzenia e; zrodlo_powiazania:
              wymaganie, katalog, katalog-zbiorczy albo ich połączenie
              znakiem „+”; przypadek_testowy:
              weryfikacja wymagania i numery przypadków z tabeli 7.8)
  --kanwa ''  pomija sprawdzenie f

Kod wyjścia: 0 = brak błędów, 1 = są błędy, 2 = nie da się odczytać pliku.

Rodziny ID są zdefiniowane w jednym miejscu: słownik RODZINY poniżej.
Tylko biblioteka standardowa Pythona 3.
"""

import argparse
import csv
import json
import os
import re
import sys
from collections import Counter, OrderedDict, defaultdict

# ---------------------------------------------------------------------------
# Rodziny ID: jedno miejsce do zmiany. Kolejność ma znaczenie (dłuższe
# prefiksy przed krótszymi, np. S-K przed innymi).
# ---------------------------------------------------------------------------
RODZINY = OrderedDict([
    ("S-K", r"S-K-\d{2}"),
    ("R", r"R-[A-Z]+-\d{2}b?"),
    ("E", r"E-\d{2}w?"),
    ("L", r"L-\d{2}"),
    ("H", r"H-\d{2}"),
    ("O", r"O-\d{2}"),
    ("M", r"M-\d{2}p?"),
    ("U", r"U-\d{2}"),
])

# Rodziny, które są ekranami (sprawdzenie e i f). Sprawdzenie f obejmuje
# tylko rodziny, które mają prefiks artboardu w KANWA_PREFIKSY (U-xx nie ma).
RODZINY_EKRANOW = ("S-K", "L", "U", "E", "O")

# Ekrany bez rodziny ID: stany ekranu dnia i kokpit. Sprawdzane w e tylko
# wtedy, gdy stoją w pierwszej kolumnie tabeli katalogu (z kolumną „Wymagania”).
EKRANY_STALE = ("S1", "S2a", "S2b", "S2c", "S2d", "S3", "S5", "S6", "S7", "S8", "S9", "K1", "K2")
STALE_RE = re.compile(r"(?<![\w-])(" + "|".join(EKRANY_STALE) + r")(?![\w-])")

# Ekrany nazwane: nazwa w macierzy -> wzorzec szukany w pierwszej kolumnie
# katalogu (i w wierszach wymagań).
EKRANY_NAZWANE = OrderedDict([
    ("pytanie „Co było inaczej?”", r"Co było inaczej"),
    ("pytanie o senność", r"[Ss]enn(ość|y w dzień)"),
    ("pytanie „gorzej”", r"gorzej"),
    ("potwierdzenie „Omówione”", r"Omówione"),
    ("raport (QR)", r"raport \(QR\)|Raport dla lekarza"),
    ("ekran-ściąga", r"ściąg"),
    ("strona A4", r"strona A4|raport A4|PDF"),
    ("„Sprzęt”", r"Sprzęt"),
    ("„Otwarte sprawy”", r"Otwarte sprawy"),
    ("potwierdzenie „Zapisano”", r"Zapisano: (?:tak|nie)|„Zapisano”"),
    ("pytanie o przerwę", r"Przerwa była planowana|przerw[aęy] planowan"),
    ("„Co znaczą te liczby”", r"Co znaczą te liczby"),
])

# Limity długości (5.4.4) i najdłuższe wartości zmiennych (R-EKR-07).
LIMITY = OrderedDict([("tytuł", 30), ("powód", 70), ("krok", 60), ("linia", 60),
                      ("zdanie", 90), ("krok karty", 34),
                      ("tytuł s2d", 90), ("powód s2d", 100)])
NAJDLUZSZE = {
    "data": "30.11", "tryb": "AutoSet Her", "godzina": "23:59", "hh:mm": "23:59", "min": "999", "n": "999", "m": "999",
    "x": "6 h 52", "y": "6 h 52", "norma": "6 h 52", "czas": "12 h 59", "h": "12", "t": "35,5",
    "w": "99", "p": "100", "próg": "36", "max": "9999", "miesiąc": "października",
    "poprzednim miesiącu": "październiku", "Metryka": "Czas na masce",
    "P10": "99,9", "P90": "99,9", "metryki": "wyciek, oddechy na minutę",
    "metryka": "wentylacja minutowa", "czynniku": "lekach nasennych", "czynnik": "katar lub infekcja",
    "zmiana": "krótsza maska", "nazwa": "X" * 30, "tytuł": "X" * 30,
    "wzorzec": "ograniczenie przepływu", "rodzaj": "brzozy", "cecha": "spłaszczony szczyt",
    "różnica": "krótsza o 50 min", "ile": "godzinę", "stare": "CPAP 12,5 · EPR 3",
    "nowe": "CPAP 12,5 · EPR 3", "a": "99,9", "b": "99,9", "P": "12,5", "d": "30",
    "ceil(0,7·d)": "21", "k": "8", "wersja": "v1.12.10", "zakres": "v1.12.10–v1.12.10", "cel": "365", "lepiej o x": "lepiej o 99,9",
    "x razy / o y": "o 99,9", "sygnał": "wentylacja wyższa",
    "ustawienie": "okres wymiany: komora nawilżacza",
    "przesunięcie": "45 min",
    "element": "komora nawilżacza",  # S-K-35 (ADR-0027)
    "elemencie": "filtrze",  # S-K-35 krok `przy {elemencie}` (masce, filtrze)
    "v": "150",  # S-K-34: porywy wiatru w km/h  # R-AW-36: 15, 30, 45 min albo 1…99 h, od 100 h „99+ h”
}
# {Metryka} wielką literą stoi tylko w tekstach odkryć O-xx (metryki M-01, M-02,
# M-03, M-07, M-17: najdłuższa nazwa „Czas na masce”); {metryka} małą literą
# także w liniach normy (wszystkie metryki z tabeli 4.5).
# Szablony sprawdzane osobną regułą (tytuł O-12: reguła skrótu w 6.5).
POMIJANE_ZMIENNE = ("Odmiana",)

# Pierwsza kolumna tabeli skróconej 9.2 (streszczenie, bez powiązań w macierzy).
TABELA_SKROCONA = "ekran albo grupa"

# Artboardy kanwy: prefiks nazwy pliku -> prefiks ID w spec.
KANWA_PREFIKSY = OrderedDict([("SK", "S-K-"), ("L", "L-")])

# Granica ID: przed ID nie może stać litera, cyfra ani łącznik; po ID też nie.
_GRANICA_PRZED = r"(?<![\w-])"
_GRANICA_PO = r"(?![\w-])"
ID_RE = re.compile(_GRANICA_PRZED + "(" + "|".join(RODZINY.values()) + ")" + _GRANICA_PO)
ID_PELNE_RE = re.compile("(?:" + "|".join(RODZINY.values()) + ")")
ROZBIOR_RE = re.compile(r"^(.*-)(\d{2,3})([a-z]?)$")
# Liczba cyfr numeru w skrótach i zakresach według prefiksu rodziny (domyślnie dwie; dziś każda rodzina ma dwie).
CYFRY_RODZINY = {}

# Operatory zakresu: „do”, wielokropek, trzy kropki, półpauza.
_ZAKRES_OP = r"\s*(?:do|…|\.\.\.|–)\s*"
# Po liczbie w skrócie musi stać koniec, interpunkcja albo spójnik/zakres;
# nie spacja i jednostka („M-17, 10 nocy” to nie skrót).
_KONIEC_SKROTU = r"(?=$|[,/;:)\]|]|\.(?!\d)|\s+(?:do|i|oraz)\b|\s*(?:…|–|\.\.\.)|\s*$)"

NAGLOWEK_RE = re.compile(r"^(#{1,6})\s+(.*)$")
PROG_RE = re.compile(r"\[PRÓG\]")
DANE_RE = re.compile(r"\[DANE\?\]")
KANWA_RE = re.compile(r"^(" + "|".join(KANWA_PREFIKSY) + r")(\d{2})-")
STAN_ZAKRES_RE = re.compile(r"(?<![\w-])S(\d)\s*(?:–|…|do)\s*S(\d)(?![\w-])")


def rodzina(id_):
    for nazwa, wzor in RODZINY.items():
        if re.fullmatch(wzor, id_):
            return nazwa
    return None


def prefiks_wymagania(id_):
    m = re.match(r"^(R-[A-Z]+)-", id_)
    return m.group(1) if m else None


def rodzina_ekranu(id_):
    """Rodzina do macierzy: rodzina ID, „stan”, „kokpit”, „nazwany” albo „inne”."""
    r = rodzina(id_)
    if r:
        return r
    if id_ in EKRANY_STALE:
        return "kokpit" if id_.startswith("K") else "stan"
    if id_ in EKRANY_NAZWANE:
        return "nazwany"
    return "inne"


def jest_ekranem(id_):
    return rodzina_ekranu(id_) in RODZINY_EKRANOW + ("stan", "kokpit", "nazwany")


def podstaw(szablon):
    """Wstawia najdłuższe wartości zmiennych; zwraca (tekst, lista nieznanych zmiennych)."""
    brak = []

    def f(m):
        nazwa = m.group(1)
        if nazwa in NAJDLUZSZE:
            return NAJDLUZSZE[nazwa]
        brak.append(nazwa)
        return ""
    return re.sub(r"\{([^{}]+)\}", f, szablon), brak


_SEG_RE = re.compile(r"`([^`]+)`")


def _rodzaj_kolumny(naglowek, sekcja=""):
    h = naglowek.strip().lower()
    # karty przerywników S2d: teksty autora, osobne limity (decyzja 05.11.2026, 5.7.6); tylko w tabelach
    # sekcji 5.7.6, inaczej kolumna o tej nazwie gdzie indziej dostałaby limit 90/100 zamiast 30/70
    if h == "tytuł (s2d)" and sekcja.startswith("5.7.6"):
        return "tytuł s2d"
    if h == "powód (s2d)" and sekcja.startswith("5.7.6"):
        return "powód s2d"
    if h == "tytuł":
        return "tytuł"
    if h == "powód":
        return "powód"
    if h == "krok":
        return "krok"
    if h == "linia" or h.startswith("linia na ekranie"):
        return "linia"
    if h.startswith("zdanie"):
        return "zdanie"
    if h == "komunikat":
        return "komunikat"
    if h == "wymaganie":
        return "wymaganie"
    return None


def _segmenty_komunikatu(komorka):
    """Dzieli komórkę „Komunikat” (6.4) na pary (rodzaj, tekst)."""
    wynik = []
    for czesc in komorka.split(";"):
        segs = _SEG_RE.findall(czesc)
        if not segs:
            continue
        low = czesc.strip().lower()
        if low.startswith("linia") or (re.search(r"\blinia\b", low) and len(segs) == 1):
            wynik += [("linia", s_) for s_ in segs]
            continue
        if len(segs) == 1 and "tytuł" in low and not segs[0].startswith("→"):
            wynik.append(("tytuł", segs[0]))
            continue
        if len(segs) == 1 and "powód" in low and not segs[0].startswith("→"):
            wynik.append(("powód", segs[0]))
            continue
        kolejne = ["tytuł", "powód"]
        for s_ in segs:
            if s_.startswith("→"):
                wynik.append(("krok", s_))
            else:
                wynik.append((kolejne.pop(0) if kolejne else "powód", s_))
    return wynik


def sprawdz_dlugosci(dok, wynik):
    """Sprawdzenie h: długość szablonów tekstów po wstawieniu najdłuższych wartości."""
    for tab in dok.tabele:
        nag = [h.lower() for h in tab["naglowek"]]
        rodzaje = {i: _rodzaj_kolumny(h, dok.sekcje[tab["start"] - 1]) for i, h in enumerate(nag)}
        if not any(rodzaje.values()):
            continue
        k_karta = nag.index("krok na karcie") if "krok na karcie" in nag else None
        k_poziom = nag.index("poziom") if "poziom" in nag else None
        karta_tabeli = bool(nag) and nag[0] == "odkrycie"
        for nr, kom in tab["wiersze"]:
            karta = karta_tabeli or (k_karta is not None and k_karta < len(kom)
                                     and _czysc(kom[k_karta]).lower().startswith("tak"))
            pary = []
            for i, rodz in rodzaje.items():
                if not rodz or i >= len(kom):
                    continue
                kom_i = kom[i]
                if rodz == "komunikat":
                    poziom = _czysc(kom[k_poziom]).lower() if k_poziom is not None and k_poziom < len(kom) else ""
                    if "linia" in poziom and "problem" not in poziom and "brak danych" not in poziom:
                        pary += [("linia", s_) for s_ in _SEG_RE.findall(kom_i)]
                    else:
                        pary += _segmenty_komunikatu(kom_i)
                elif rodz == "wymaganie":
                    for m in _SEG_RE.finditer(kom_i):
                        if "lini" in kom_i[max(0, m.start() - 30):m.start()].lower():
                            pary.append(("linia", m.group(1)))
                elif rodz == "krok":
                    pary += [("krok", s_) for s_ in _SEG_RE.findall(kom_i) if s_.startswith("→")]
                else:
                    pary += [(rodz, s_) for s_ in _SEG_RE.findall(kom_i)]
            for rodz, szablon in pary:
                if any("{" + z + "}" in szablon for z in POMIJANE_ZMIENNE):
                    continue
                tekst, brak = podstaw(szablon)
                for z in brak:
                    wynik.dodaj("blad", "h", nr, "Zmienna {%s} nie ma wartości w NAJDLUZSZE (szablon „%s”)."
                                % (z, szablon), fragment=_fragment(dok.linie[nr - 1]))
                limity = [(rodz, LIMITY[rodz])]
                if rodz == "krok" and karta:
                    limity.append(("krok karty", LIMITY["krok karty"]))
                for nazwa, lim in limity:
                    if len(tekst) > lim:
                        wynik.dodaj("blad", "h", nr, "%s „%s” ma %d znaków po wstawieniu najdłuższych wartości, "
                                    "limit %d." % (nazwa.capitalize(), szablon, len(tekst), lim),
                                    fragment=tekst)


def sprawdz_tytuly(dok, wynik):
    """Sprawdzenie i: ten sam tytuł nie należy do dwóch różnych ekranów."""
    tytuly = defaultdict(list)   # tekst -> [(właściciel, linia)]
    for tab in dok.tabele:
        nag = [h.lower() for h in tab["naglowek"]]
        rodzaje = {i: _rodzaj_kolumny(h, dok.sekcje[tab["start"] - 1]) for i, h in enumerate(nag)}
        if not any(r in ("tytuł", "komunikat") for r in rodzaje.values()):
            continue
        for nr, kom in tab["wiersze"]:
            if not kom:
                continue
            wl = id_definicji(kom[0]) or _czysc(kom[0])
            for i, rodz in rodzaje.items():
                if i >= len(kom) or i == 0:
                    continue
                if rodz == "tytuł":
                    teksty = _SEG_RE.findall(kom[i])
                elif rodz == "komunikat":
                    teksty = [s_ for r_, s_ in _segmenty_komunikatu(kom[i]) if r_ == "tytuł"]
                else:
                    continue
                for tx in teksty:
                    tytuly[tx.strip()].append((wl, nr))
    for tx, wl in sorted(tytuly.items()):
        wlasciciele = sorted({w for w, _ in wl})
        if len(wlasciciele) > 1:
            nr = wl[0][1]
            wynik.dodaj("blad", "i", nr, "Tytuł „%s” mają różne ekrany: %s (l. %s)."
                        % (tx, ", ".join(wlasciciele), ", ".join(str(n) for _, n in wl)),
                        fragment=tx)


# ---------------------------------------------------------------------------
# Wyszukiwanie ID w tekście, z rozwijaniem zakresów i skrótów
# ---------------------------------------------------------------------------
def znajdz_id(tekst):
    """Zwraca listę (id, forma) dla wszystkich ID w tekście.

    forma: 'pelne' dla ID wypisanego w całości, 'zakres' dla ID dodanego
    z rozwinięcia zakresu, 'skrot' dla ID z listy skróconej („/32”, „, 02”).
    """
    wynik = []
    for m in ID_RE.finditer(tekst):
        id_ = m.group(1)
        wynik.append((id_, "pelne"))
        r = ROZBIOR_RE.match(id_)
        if not r:
            continue
        prefiks, num, _sufiks = r.group(1), int(r.group(2)), r.group(3)
        p = re.escape(prefiks)
        c = CYFRY_RODZINY.get(prefiks, r"\d{2}")
        # Prawy koniec zakresu: pełne ID albo goła liczba z granicą jak w skrócie
        # („M-01 do 14 nocy” to nie zakres).
        zakres_re = re.compile(_ZAKRES_OP + "(?:" + p + "(" + c + r")(?![\w-])|(" + c + ")" + _KONIEC_SKROTU + ")")
        # Po przecinku liczba musi kończyć się interpunkcją albo spójnikiem;
        # po ukośniku wystarczy granica słowa („L-01/03/04 tylko”).
        lista_re = re.compile(r"(?:,[ \t]*(?:" + p + r")?(" + c + ")([a-z]?)" + _KONIEC_SKROTU
                              + r"|/(?:" + p + r")?(" + c + r")([a-z]?)(?![\w-]|,\d))")
        poz = m.end()
        ostatni = num
        while True:
            mz = zakres_re.match(tekst, poz)
            if mz:
                prawy_pelny = mz.group(1) is not None
                koniec = int(mz.group(1) or mz.group(2))
                if ostatni < koniec <= ostatni + 60:
                    # Prawy koniec wypisany w całości złapie główne wyszukiwanie.
                    gora = koniec - 1 if prawy_pelny else koniec
                    for n in range(ostatni + 1, gora + 1):
                        wynik.append(("%s%02d" % (prefiks, n), "zakres"))
                    ostatni = koniec
                    poz = mz.end()
                    continue
                break
            ml = lista_re.match(tekst, poz)
            if ml:
                liczba = ml.group(1) or ml.group(3)
                sufiks = ml.group(2) if ml.group(1) else ml.group(4)
                cel = "%s%s%s" % (prefiks, liczba, sufiks)
                pelne = ml.group(0).lstrip(",/ \t").startswith(prefiks)
                if not pelne:
                    wynik.append((cel, "skrot"))
                ostatni = int(liczba)
                poz = ml.end()
                continue
            break
    return wynik


def znajdz_niepelnych_kandydatow(tekst, ids_wycofane_spoza_rodzin):
    """ID z listy wycofanych spoza rodzin (np. S4): tylko samodzielne słowo."""
    wynik = []
    for id_ in ids_wycofane_spoza_rodzin:
        if re.search(_GRANICA_PRZED + re.escape(id_) + _GRANICA_PO, tekst):
            wynik.append(id_)
    return wynik


# ---------------------------------------------------------------------------
# Parsowanie dokumentu
# ---------------------------------------------------------------------------
_PIPE_RE = re.compile(r"(?<!\\)\|")


def _komorki(linia):
    """Komórki wiersza tabeli; „\\|” wewnątrz komórki nie dzieli kolumn."""
    s = linia.strip()
    if s.startswith("|"):
        s = s[1:]
    if s.endswith("|") and not s.endswith("\\|"):
        s = s[:-1]
    return [c.strip() for c in _PIPE_RE.split(s)]


def _czysc(komorka):
    return komorka.replace("**", "").replace("`", "").strip()


_DEF_RE = re.compile(r"(" + "|".join(RODZINY.values()) + r")(?:\s*\(.*\))?")


def id_definicji(komorka):
    """ID definiowane przez pierwszą komórkę wiersza albo None.

    Komórka musi zawierać samo ID albo ID z dopiskiem w nawiasie
    („E-20 (od v0.3: …)”). Komórka z kilkoma ID („L-07, L-08”) nie jest
    definicją.
    """
    m = _DEF_RE.fullmatch(_czysc(komorka))
    return m.group(1) if m else None


def _separator(linia):
    return bool(re.fullmatch(r"\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)*\|?", linia.strip()))


class Dokument:
    def __init__(self, tekst):
        self.linie = tekst.split("\n")
        self.sekcje = []      # dla każdej linii: tytuł bieżącego nagłówka (łańcuch)
        self.w_wycofanych = []  # bool dla każdej linii
        self.tabele = []      # lista dict: start, naglowek, wiersze[(nr, komorki)]
        self._parsuj()

    def _parsuj(self):
        stos = []  # (poziom, tytuł)
        tabela = None
        for i, linia in enumerate(self.linie, start=1):
            mh = NAGLOWEK_RE.match(linia)
            if mh:
                poziom = len(mh.group(1))
                while stos and stos[-1][0] >= poziom:
                    stos.pop()
                stos.append((poziom, mh.group(2).strip()))
            self.sekcje.append(stos[-1][1] if stos else "")
            self.w_wycofanych.append(any("wycofane" in t.lower() for _, t in stos))

            if linia.strip().startswith("|"):
                if tabela is None:
                    tabela = {"start": i, "naglowek": [_czysc(c) for c in _komorki(linia)], "wiersze": []}
                    self.tabele.append(tabela)
                elif _separator(linia) and not tabela["wiersze"]:
                    pass
                else:
                    tabela["wiersze"].append((i, _komorki(linia)))
            else:
                tabela = None


# ---------------------------------------------------------------------------
# Lista wycofanych
# ---------------------------------------------------------------------------
class Wycofane(OrderedDict):
    """Lista wycofanych i zmienionych ID; `powtorzone`: ID -> numery wierszy pliku, gdy ID stoi w kilku
    wierszach (skrypt bierze tylko ostatni, więc wcześniejsze opisy znikałyby bez ostrzeżenia)."""

    def __init__(self, *a, **kw):
        super().__init__(*a, **kw)
        self.powtorzone = OrderedDict()


def wczytaj_wycofane(sciezka):
    wynik = Wycofane()
    if not sciezka:
        return wynik
    wiersze = OrderedDict()
    with open(sciezka, encoding="utf-8", newline="") as f:
        czytnik = csv.DictReader(f)
        for wiersz in czytnik:
            id_ = (wiersz.get("id") or "").strip()
            if not id_:
                continue
            wiersze.setdefault(id_, []).append(czytnik.line_num)
            wynik[id_] = {
                "status": (wiersz.get("status") or "").strip(),
                "nastepca": (wiersz.get("nastepca") or "").strip(),
                "zrodlo": (wiersz.get("zrodlo") or "").strip(),
            }
    for id_, nr in wiersze.items():
        if len(nr) > 1:
            wynik.powtorzone[id_] = nr
    return wynik


def wczytaj_kanwe(sciezka):
    """Zwraca dict: ID w spec -> nazwa artboardu."""
    if not sciezka:
        return None
    with open(sciezka, encoding="utf-8") as f:
        dane = json.load(f)
    boards = dane.get("boards", {})
    nazwy = list(boards.keys()) if isinstance(boards, dict) else [b.get("file", "") for b in boards]
    wynik = OrderedDict()
    for nazwa in nazwy:
        m = KANWA_RE.match(os.path.basename(nazwa))
        if m:
            wynik["%s%s" % (KANWA_PREFIKSY[m.group(1)], m.group(2))] = nazwa
    return wynik


# ---------------------------------------------------------------------------
# Sprawdzenia
# ---------------------------------------------------------------------------
class Wynik:
    def __init__(self):
        self.wpisy = []  # dict: poziom, sprawdzenie, linia, id, tresc, fragment

    def dodaj(self, poziom, sprawdzenie, linia, tresc, id_=None, fragment=None):
        self.wpisy.append({
            "poziom": poziom, "sprawdzenie": sprawdzenie, "linia": linia,
            "id": id_, "tresc": tresc, "fragment": fragment,
        })

    def ile(self, poziom, sprawdzenie=None):
        return sum(1 for w in self.wpisy if w["poziom"] == poziom
                   and (sprawdzenie is None or w["sprawdzenie"] == sprawdzenie))


def _fragment(linia, id_=None, szer=90):
    s = linia.strip()
    if id_ and id_ in s:
        i = s.index(id_)
        start = max(0, i - 35)
        s2 = s[start:start + szer]
        return ("…" if start > 0 else "") + s2 + ("…" if start + szer < len(s) else "")
    return s[:szer] + ("…" if len(s) > szer else "")


# Słowa EARS wymagane przez wzorzec (kolumna „Wzorzec”); „A + B” łączy oba.
EARS_SLOWA = OrderedDict([("Event", ("WHEN",)), ("State", ("WHILE",)), ("IF/THEN", ("IF", "THEN")),
                          ("Optional", ("WHERE",)), ("Ubiquitous", ())])
_EARS_RE = re.compile(r"\b(WHEN|WHILE|IF|THEN|WHERE)\b")
# Wynik po „→”: liczba, ID albo tekst, czyli co najmniej jedna litera albo cyfra
# (także w odwrotnych apostrofach). Pusty wynik („wejście →”) to ostrzeżenie.
_WYNIK_RE = re.compile(r"\w")


def sprawdz_weryfikacje(id_, nr, komorka, wynik, strict=False):
    """Weryfikacja w postaci „wejście → wynik” (część sprawdzenia d)."""
    # Strzałka w tekście ekranu (`→ krok`) nie jest strzałką weryfikacji.
    bez_tekstow = _SEG_RE.sub("`tekst`", komorka)
    if "→" not in bez_tekstow:
        wynik.dodaj("blad" if strict else "ostrzezenie", "d", nr,
                    "Weryfikacja wymagania %s nie ma postaci „wejście → wynik” (brak „→”)." % id_,
                    id_=id_, fragment=_fragment(komorka))
        return
    for czesc in bez_tekstow.split("→")[1:]:
        wynik_tekst = czesc.split(";")[0]
        if not _WYNIK_RE.search(wynik_tekst):
            wynik.dodaj("ostrzezenie", "d", nr,
                        "Weryfikacja wymagania %s: po „→” brak wyniku (liczby, ID ani tekstu)." % id_,
                        id_=id_, fragment=_fragment(komorka))
            return


def sprawdz_ears(id_, nr, wzorzec, tresc, wynik):
    """Słowa EARS w treści wymagania zgodne z kolumną „Wzorzec” (część sprawdzenia d)."""
    czesci = [c.strip() for c in wzorzec.split("+")]
    nieznane = [c for c in czesci if c not in EARS_SLOWA]
    if nieznane:
        wynik.dodaj("blad", "d", nr, "Wymaganie %s ma nieznany wzorzec EARS „%s”." % (id_, wzorzec), id_=id_)
        return
    potrzebne = {s_ for c in czesci for s_ in EARS_SLOWA[c]}
    obecne = set(_EARS_RE.findall(tresc))
    brak, nadmiar = sorted(potrzebne - obecne), sorted(obecne - potrzebne)
    if brak or nadmiar:
        opis = []
        if brak:
            opis.append("brak %s" % ", ".join(brak))
        if nadmiar:
            opis.append("nadmiarowe %s" % ", ".join(nadmiar))
        wynik.dodaj("blad", "d", nr, "Wymaganie %s: wzorzec „%s” niezgodny ze słowami EARS w treści (%s)."
                    % (id_, wzorzec, "; ".join(opis)), id_=id_, fragment=_fragment(tresc))


def sprawdz(tekst, wycofane, kanwa, strict=False):
    dok = Dokument(tekst)
    wynik = Wynik()
    wycofane_spoza_rodzin = [i for i in wycofane if rodzina(i) is None]

    # --- definicje (a) --------------------------------------------------
    definicje = defaultdict(list)    # id -> [linia]
    wiersz_def = {}                  # (linia) -> (id, tabela, komorki)
    tabele_uzup = []
    for t in dok.tabele:
        if not t["naglowek"] or t["naglowek"][0] != "ID":
            continue
        ids_w_tabeli = []
        for nr, kom in t["wiersze"]:
            id_ = id_definicji(kom[0] if kom else "")
            if id_:
                ids_w_tabeli.append((nr, id_, kom))
        if not ids_w_tabeli:
            continue
        juz = all(id_ in definicje for _, id_, _ in ids_w_tabeli)
        if juz:
            # Tabela uzupełniająca (np. teksty ekranów do katalogu wyżej).
            tabele_uzup.append(t)
            pierwsze = sorted({definicje[id_][0] for _, id_, _ in ids_w_tabeli})
            wynik.dodaj("ostrzezenie", "a", t["start"],
                        "Tabela uzupełniająca: wszystkie ID w jej pierwszej kolumnie (%d) są już "
                        "zdefiniowane wyżej (pierwsza definicja l. %s). Liczę ją jako użycia, nie definicje."
                        % (len(ids_w_tabeli), ", ".join(str(x) for x in pierwsze[:3])
                           + ("…" if len(pierwsze) > 3 else "")),
                        fragment=_fragment(dok.linie[t["start"] - 1]))
            continue
        for nr, id_, kom in ids_w_tabeli:
            definicje[id_].append(nr)
            wiersz_def[nr] = (id_, t, kom)

    for id_, linie in definicje.items():
        if len(linie) > 1:
            wynik.dodaj("blad", "a", linie[1],
                        "ID %s zdefiniowane %d razy (l. %s)." % (id_, len(linie), ", ".join(map(str, linie))),
                        id_=id_, fragment=_fragment(dok.linie[linie[1] - 1], id_))

    # --- użycia ----------------------------------------------------------
    uzycia = defaultdict(list)  # id -> [(linia, forma)]
    na_linii = []               # dla każdej linii: set ID
    for i, linia in enumerate(dok.linie, start=1):
        ids = set()
        for id_, forma in znajdz_id(linia):
            ids.add(id_)
            # Wystąpienie w komórce definicji nie jest użyciem.
            if not (i in wiersz_def and wiersz_def[i][0] == id_ and forma == "pelne"):
                uzycia[id_].append((i, forma))
        for id_ in znajdz_niepelnych_kandydatow(linia, wycofane_spoza_rodzin):
            ids.add(id_)
            uzycia[id_].append((i, "slowo"))
        na_linii.append(ids)

    # --- (b) użyte, a niezdefiniowane --------------------------------------
    for id_ in sorted(uzycia):
        if id_ in definicje or id_ in wycofane:
            continue
        linie = sorted({l for l, _ in uzycia[id_]})
        wynik.dodaj("blad", "b", linie[0],
                    "ID %s użyte bez definicji w tabeli katalogu (l. %s)."
                    % (id_, ", ".join(map(str, linie))),
                    id_=id_, fragment=_fragment(dok.linie[linie[0] - 1], id_))

    # --- (c) odwołania do wycofanych -------------------------------------
    # Powtórzone ID w tools/wycofane.csv: skrypt bierze ostatni wiersz, więc wcześniejszy opis
    # (albo status „wycofane” przykryty przez „zmienione”) znika bez śladu.
    for id_, nr in getattr(wycofane, "powtorzone", {}).items():
        wynik.dodaj("ostrzezenie", "c", 0,
                    "tools/wycofane.csv: ID %s stoi w %d wierszach (l. %s); skrypt bierze tylko ostatni. "
                    "Połącz opisy w jednym wierszu („; ”)." % (id_, len(nr), ", ".join(map(str, nr))), id_=id_)
    for i, ids in enumerate(na_linii, start=1):
        if dok.w_wycofanych[i - 1]:
            continue
        for id_ in sorted(ids):
            info = wycofane.get(id_)
            if not info:
                continue
            nast = info["nastepca"] or "brak"
            if info["status"] == "wycofane":
                rodzaj = "definicja" if (i in wiersz_def and wiersz_def[i][0] == id_) else "odwołanie"
                wynik.dodaj("blad", "c", i,
                            "%s wycofanego ID %s (następca: %s)." % (rodzaj.capitalize(), id_, nast),
                            id_=id_, fragment=_fragment(dok.linie[i - 1], id_))
            elif info["status"] == "zmienione":
                wynik.dodaj("info", "c", i,
                            "Odwołanie do zmienionego ID %s (zmiana: %s), sprawdź, czy treść jest aktualna."
                            % (id_, nast), id_=id_, fragment=_fragment(dok.linie[i - 1], id_))
    # Podpowiedź: zakresy stanów („S3–S5”) obejmujące wycofany stan.
    for id_ in wycofane_spoza_rodzin:
        ms = re.fullmatch(r"S(\d)", id_)
        if not ms or wycofane[id_]["status"] != "wycofane":
            continue
        n = int(ms.group(1))
        for i, linia in enumerate(dok.linie, start=1):
            if dok.w_wycofanych[i - 1]:
                continue
            for mz in STAN_ZAKRES_RE.finditer(linia):
                if int(mz.group(1)) < n < int(mz.group(2)):
                    wynik.dodaj("info", "c", i,
                                "Zakres stanów „%s” obejmuje wycofany stan %s (nie liczę jako błąd)."
                                % (mz.group(0), id_), id_=id_, fragment=_fragment(linia, mz.group(0)))

    # --- (d) weryfikacja wymagań ------------------------------------------
    wymagania = OrderedDict()  # id -> (linia, komorki, tabela)
    for nr, (id_, t, kom) in sorted(wiersz_def.items()):
        if rodzina(id_) != "R":
            continue
        wymagania[id_] = (nr, kom, t)
        nag = [h.lower() for h in t["naglowek"]]
        if "weryfikacja" not in nag:
            wynik.dodaj("blad", "d", nr, "Wymaganie %s jest w tabeli bez kolumny „Weryfikacja”." % id_,
                        id_=id_, fragment=_fragment(dok.linie[nr - 1], id_))
            continue
        k = nag.index("weryfikacja")
        wart = _czysc(kom[k]) if k < len(kom) else ""
        if wart in ("", "-", "–", "\u2014"):
            wynik.dodaj("blad", "d", nr, "Wymaganie %s ma pustą kolumnę „Weryfikacja”." % id_,
                        id_=id_, fragment=_fragment(dok.linie[nr - 1], id_))
        else:
            sprawdz_weryfikacje(id_, nr, kom[k], wynik, strict)
        if "wzorzec" in nag and "wymaganie" in nag:
            kz, kr = nag.index("wzorzec"), nag.index("wymaganie")
            if kz < len(kom) and kr < len(kom):
                sprawdz_ears(id_, nr, _czysc(kom[kz]), kom[kr], wynik)

    # --- macierz: wymaganie -> ID, do których się odwołuje ----------------
    macierz = []
    for id_, (nr, kom, t) in wymagania.items():
        tekst_wiersza = " | ".join(kom[1:])
        widziane = set()
        for cel, _f in znajdz_id(tekst_wiersza):
            if cel != id_ and cel not in widziane:
                widziane.add(cel)
                macierz.append((id_, cel))
        for cel in znajdz_niepelnych_kandydatow(tekst_wiersza, wycofane_spoza_rodzin):
            if cel not in widziane:
                widziane.add(cel)
                macierz.append((id_, cel))

    # --- macierz: ekrany bez rodziny ID w wierszach wymagań ---------------
    zrodla = defaultdict(set)
    for wym, cel in macierz:
        zrodla[(wym, cel)].add("wymaganie")
    for id_, (nr, kom, t) in wymagania.items():
        tekst_wiersza = " | ".join(kom[1:])
        cele = list(OrderedDict.fromkeys(STALE_RE.findall(tekst_wiersza)))
        cele += [n for n, wz in EKRANY_NAZWANE.items() if re.search(wz, tekst_wiersza)]
        for cel in cele:
            if (id_, cel) not in zrodla:
                macierz.append((id_, cel))
            zrodla[(id_, cel)].add("wymaganie")

    # --- macierz: kolumna „Wymagania” w tabelach katalogu ----------------
    w_katalogu = OrderedDict()  # ekran spoza rodzin ID -> linia
    wymienione = defaultdict(set)  # wymaganie -> ekrany wymienione w jego wierszu
    for wym, cel in macierz:
        wymienione[wym].add(cel)
    for tab in dok.tabele:
        nag = [h.lower() for h in tab["naglowek"]]
        if "wymagania" not in nag or not nag or nag[0] == "id":
            continue
        if nag[0] == TABELA_SKROCONA:
            # Tabela skrócona 9.2 tylko streszcza; nie tworzy powiązań.
            continue
        k = nag.index("wymagania")
        for nr, kom in tab["wiersze"]:
            if k >= len(kom):
                continue
            pierwsza = kom[0]
            cele = [i for i, _ in znajdz_id(pierwsza) if rodzina(i) in RODZINY_EKRANOW]
            for tok in STALE_RE.findall(pierwsza):
                cele.append(tok)
                w_katalogu.setdefault(tok, nr)
            for nazwa, wz in EKRANY_NAZWANE.items():
                if re.search(wz, pierwsza):
                    cele.append(nazwa)
                    w_katalogu.setdefault(nazwa, nr)
            reqs = [i for i, _ in znajdz_id(kom[k]) if i in wymagania]
            cele = list(OrderedDict.fromkeys(cele))
            zbiorczy = len(cele) > 1
            for wym in reqs:
                for cel in cele:
                    if zbiorczy:
                        # Wiersz zbiorczy: powiązanie tylko z ekranem, który wymaganie samo wymienia.
                        if cel in wymienione[wym]:
                            zrodla[(wym, cel)].add("katalog-zbiorczy")
                        continue
                    if (wym, cel) not in zrodla:
                        macierz.append((wym, cel))
                    zrodla[(wym, cel)].add("katalog")

    # --- przypadki testowe z tabeli „#, Sytuacja, Oczekiwany ekran” -------
    przypadki = defaultdict(list)
    for tab in dok.tabele:
        nag = [h.lower() for h in tab["naglowek"]]
        if not nag or nag[0] != "#" or "oczekiwany ekran" not in nag:
            continue
        for nr, kom in tab["wiersze"]:
            for wym in OrderedDict.fromkeys(i for i, _ in znajdz_id(" | ".join(kom)) if i in wymagania):
                przypadki[wym].append(_czysc(kom[0]))

    # --- (e) ekran bez wymagania ------------------------------------------
    ekrany = OrderedDict()
    for id_, linie in definicje.items():
        if rodzina(id_) in RODZINY_EKRANOW and wycofane.get(id_, {}).get("status") != "wycofane":
            ekrany[id_] = linie[0]
    if kanwa:
        for id_ in kanwa:
            if id_ not in ekrany and wycofane.get(id_, {}).get("status") != "wycofane":
                ekrany[id_] = definicje.get(id_, [0])[0]
    for id_, nr in w_katalogu.items():
        ekrany.setdefault(id_, nr)
    pokryte = {cel for _, cel in macierz}
    z_powiazaniem = {x for para in macierz for x in para}
    for id_, (nr, _k, _t) in wymagania.items():
        if id_ not in z_powiazaniem:
            wynik.dodaj("info", "e", nr, "Wymaganie %s nie ma powiązania w macierzy ani jako źródło, ani jako cel (wiersz z „brak”)." % id_,
                        id_=id_)
    poziom_e = "blad" if strict else "ostrzezenie"
    for id_, nr in ekrany.items():
        if id_ not in pokryte:
            wynik.dodaj(poziom_e, "e", nr, "Ekran %s nie jest wymieniony w żadnym wymaganiu R-*." % id_,
                        id_=id_, fragment=_fragment(dok.linie[nr - 1], id_) if nr else None)

    # --- (h) długość tekstów ------------------------------------------------
    sprawdz_dlugosci(dok, wynik)
    sprawdz_tytuly(dok, wynik)

    # --- (f) kanwa ----------------------------------------------------------
    # Wzorce bez pełnego ekranu (6.7, wiersz „nie dotyczy (L-07; …)”: pierwszy człon przed „;”)
    # nie potrzebują artboardu.
    bez_ekranu = set()
    for tab in dok.tabele:
        for _, kom in tab["wiersze"]:
            pierwsza = _czysc(kom[0]) if kom else ""
            if pierwsza.startswith("nie dotyczy ("):
                czlon = pierwsza[len("nie dotyczy ("):].split(";")[0].split(")")[0]
                bez_ekranu |= {i for i, _ in znajdz_id(czlon)}
    if kanwa is not None:
        for id_, plik in kanwa.items():
            if id_ not in definicje:
                status = wycofane.get(id_, {}).get("status")
                dop = " (ID jest na liście wycofanych)" if status == "wycofane" else ""
                wynik.dodaj("ostrzezenie", "f", 0, "Artboard %s: ID %s nie ma definicji w spec%s."
                            % (plik, id_, dop), id_=id_)
        for id_, linie in definicje.items():
            if rodzina(id_) not in RODZINY_EKRANOW:
                continue
            if not any(id_.startswith(p) for p in KANWA_PREFIKSY.values()):
                continue
            if wycofane.get(id_, {}).get("status") == "wycofane":
                continue
            if id_ in bez_ekranu:
                continue
            if id_ not in kanwa:
                wynik.dodaj("ostrzezenie", "f", linie[0], "Ekran %s nie ma artboardu na kanwie." % id_,
                            id_=id_, fragment=_fragment(dok.linie[linie[0] - 1], id_))

    # --- (g) statystyka -----------------------------------------------------
    prefiksy = Counter(prefiks_wymagania(i) for i in wymagania)
    statystyka = OrderedDict([
        ("linie", len(dok.linie)),
        ("prog_wystapienia", len(PROG_RE.findall(tekst))),
        ("prog_linie", sum(1 for l in dok.linie if PROG_RE.search(l))),
        ("dane_wystapienia", len(DANE_RE.findall(tekst))),
        ("dane_linie", sum(1 for l in dok.linie if DANE_RE.search(l))),
        ("wymagania_razem", len(wymagania)),
        ("wymagania_na_prefiks", OrderedDict(sorted(prefiksy.items()))),
        ("definicje_na_rodzine", OrderedDict(sorted(Counter(rodzina(i) for i in definicje).items()))),
        ("ekrany_sprawdzane_w_e", len(ekrany)),
        ("ekrany_z_wymaganiem", sum(1 for i in ekrany if i in pokryte)),
        ("sekcje_wymagan", OrderedDict((i, dok.sekcje[nr - 1]) for i, (nr, _k, _t) in wymagania.items())),
        ("zrodla_powiazan", OrderedDict(("%s|%s" % k, "+".join(sorted(v))) for k, v in zrodla.items())),
        ("przypadki", OrderedDict((k, v) for k, v in przypadki.items())),
    ])

    return wynik, statystyka, macierz


# ---------------------------------------------------------------------------
# Raport
# ---------------------------------------------------------------------------
NAZWY_SPRAWDZEN = OrderedDict([
    ("a", "ID zdefiniowane dokładnie raz"),
    ("b", "Użyte ID ma definicję"),
    ("c", "Odwołania do wycofanych i zmienionych ID"),
    ("d", "Wymaganie ma weryfikację"),
    ("e", "Ekran ma wymaganie"),
    ("f", "Zgodność z kanwą"),
    ("h", "Długość tekstów w limicie"),
    ("i", "Tytuł ekranu unikalny"),
])


def raport_tekst(sciezka, wynik, stat):
    out = []
    b, o, n = wynik.ile("blad"), wynik.ile("ostrzezenie"), wynik.ile("info")
    out.append("Raport spójności: %s" % sciezka)
    out.append("Wynik: %d błędów, %d ostrzeżeń, %d informacji." % (b, o, n))
    out.append("")
    out.append("Podsumowanie według sprawdzeń (błędy / ostrzeżenia / informacje):")
    for k, nazwa in NAZWY_SPRAWDZEN.items():
        out.append("  %s. %-42s %4d / %4d / %4d" % (k, nazwa, wynik.ile("blad", k),
                                                    wynik.ile("ostrzezenie", k), wynik.ile("info", k)))
    for poziom, tytul in (("blad", "BŁĘDY"), ("ostrzezenie", "OSTRZEŻENIA"), ("info", "INFORMACJE")):
        wpisy = [w for w in wynik.wpisy if w["poziom"] == poziom]
        out.append("")
        out.append("=" * 70)
        out.append("%s (%d)" % (tytul, len(wpisy)))
        out.append("=" * 70)
        if not wpisy:
            out.append("  brak")
            continue
        for k, nazwa in NAZWY_SPRAWDZEN.items():
            grupa = sorted((w for w in wpisy if w["sprawdzenie"] == k), key=lambda w: (w["linia"], w["id"] or ""))
            if not grupa:
                continue
            out.append("")
            out.append("[%s] %s" % (k, nazwa))
            for w in grupa:
                gdzie = "l. %d" % w["linia"] if w["linia"] else "kanwa"
                out.append("  %-8s %s" % (gdzie + ":", w["tresc"]))
                if w["fragment"]:
                    out.append("           „%s”" % w["fragment"])
    out.append("")
    out.append("=" * 70)
    out.append("STATYSTYKA")
    out.append("=" * 70)
    out.append("  linie dokumentu:            %d" % stat["linie"])
    out.append("  [PRÓG]:                     %d wystąpień w %d liniach" % (stat["prog_wystapienia"], stat["prog_linie"]))
    out.append("  [DANE?]:                    %d wystąpień w %d liniach" % (stat["dane_wystapienia"], stat["dane_linie"]))
    out.append("  wymagania R-* razem:        %d" % stat["wymagania_razem"])
    for p, ile in stat["wymagania_na_prefiks"].items():
        out.append("    %-10s %d" % (p, ile))
    out.append("  definicje według rodzin:")
    for r, ile in stat["definicje_na_rodzine"].items():
        out.append("    %-10s %d" % (r, ile))
    out.append("  ekrany z wymaganiem:        %d z %d" % (stat["ekrany_z_wymaganiem"], stat["ekrany_sprawdzane_w_e"]))
    return "\n".join(out)


def wiersze_macierzy(macierz, stat):
    """Wiersze CSV macierzy śledzenia: nagłówek + jeden wiersz na powiązanie."""
    sekcje = stat.get("sekcje_wymagan", {})
    zrodla = stat.get("zrodla_powiazan", {})
    przypadki = stat.get("przypadki", {})
    out = [["wymaganie", "id_odwolania", "rodzina_odwolania", "ekran", "sekcja_wymagania",
            "zrodlo_powiazania", "przypadek_testowy"]]
    for wym, cel in macierz:
        test = "weryfikacja %s" % wym
        if przypadki.get(wym):
            test += "; przypadki 7.8: %s" % ", ".join(przypadki[wym])
        out.append([wym, cel, rodzina_ekranu(cel), "tak" if jest_ekranem(cel) else "nie", sekcje.get(wym, ""),
                    zrodla.get("%s|%s" % (wym, cel), "wymaganie"), test])
    # Wymaganie bez żadnego powiązania (ani jako źródło, ani jako cel) też ma wiersz, żeby nie
    # znikało z macierzy razem ze swoim przypadkiem testowym.
    z_powiazaniem = {x for para in macierz for x in para}
    for wym in sekcje:
        if wym not in z_powiazaniem:
            test = "weryfikacja %s" % wym
            if przypadki.get(wym):
                test += "; przypadki 7.8: %s" % ", ".join(przypadki[wym])
            out.append([wym, "", "brak", "nie", sekcje.get(wym, ""), "brak powiązania", test])
    return out


def main(argv=None):
    tu = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(tu)
    ap = argparse.ArgumentParser(description="Sprawdzenie spójności ID w specyfikacji (po polsku).")
    ap.add_argument("spec", nargs="?", default=os.path.join(repo, "docs/spec/specyfikacja-funkcjonalna.md"))
    ap.add_argument("--wycofane", default=os.path.join(tu, "wycofane.csv"))
    ap.add_argument("--kanwa", default=os.path.join(repo, "design/kanwa/canvas.json"))
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--macierz", default=None)
    a = ap.parse_args(argv)

    try:
        with open(a.spec, encoding="utf-8") as f:
            tekst = f.read()
        wycofane = wczytaj_wycofane(a.wycofane) if a.wycofane and os.path.exists(a.wycofane) else OrderedDict()
        kanwa = wczytaj_kanwe(a.kanwa) if a.kanwa and os.path.exists(a.kanwa) else None
    except (OSError, ValueError) as e:
        print("Nie mogę odczytać pliku: %s" % e, file=sys.stderr)
        return 2

    wynik, stat, macierz = sprawdz(tekst, wycofane, kanwa, strict=a.strict)

    if a.macierz:
        with open(a.macierz, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            w.writerows(wiersze_macierzy(macierz, stat))

    if a.json:
        print(json.dumps({"spec": a.spec, "wpisy": wynik.wpisy, "statystyka": stat},
                         ensure_ascii=False, indent=2))
    else:
        print(raport_tekst(a.spec, wynik, stat))
    return 1 if wynik.ile("blad") else 0


if __name__ == "__main__":
    sys.exit(main())
