#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Skrypt spójności specyfikacji technicznej (cpap-eink, ADR-0020 pkt 6).

Co robi
-------
Czyta specyfikację techniczną, specyfikację funkcjonalną i listę wycofanych
ID. Sprawdza ID z rodzin T- i ich powiązania z wymaganiami funkcjonalnymi:

  a. każde ID T- jest zdefiniowane dokładnie raz (definicja = ID w pierwszej
     kolumnie wiersza tabeli, której pierwsza kolumna nagłówka to „ID”);
  b. każde użyte ID T- jest zdefiniowane albo wycofane (status „wycofane”
     w tools/wycofane.csv; „zmienione” musi mieć definicję); tabela w sekcji
     „Wycofane…” nie definiuje ID;
     ID wyglądające jak T- spoza schematu (np. „T-KOMP-1”, „T-FOO-01”) to błąd;
  c. brak odwołań do wycofanych ID T- poza sekcją „Wycofane…”;
  d. kolumny „Realizuje” (T-KOMP) i „Sprawdza” (T-TEST) wskazują tylko
     wymagania R-* zdefiniowane w spec funkcjonalnej i niewycofane, ID T-WYM
     i T-NF zdefiniowane w tym dokumencie (Realizuje: R-* i T-WYM; Sprawdza:
     R-*, T-WYM, T-NF); wiersz bez ID i bez „brak (powód)” to błąd; każde
     R-* w dowolnym miejscu dokumentu poza sekcją „Wycofane…” musi istnieć
     w spec i nie być wycofane; zakres R-* ponad 60 numerów to ostrzeżenie;
  e. wymagania T-WYM: kolumna „Weryfikacja” w postaci „wejście → wynik”
     i słowa EARS zgodne z kolumną „Wzorzec” (te same reguły co spojnosc.py);
  f. każdy znacznik [do zmierzenia] ma na tej samej linii własny opis
     pomiaru: odwołanie „pomiar N” do wiersza tabeli „Pomiary” planu pomiarów
     (tabela z pierwszą kolumną „Nr” i kolumną „Co mierzymy”); numer musi
     stać w tej tabeli;
  g. tabela stanu sekcji (pierwsza kolumna „Sekcja”, kolumna „Stan”) ma
     tylko stany: do napisania, szkic, do przeglądu, przyjęta; każda sekcja
     „## N.” dokumentu stoi w tabeli dokładnie raz i odwrotnie;
  h. wiersz „Odwołanie” tabeli nagłówka wskazuje wersję spec funkcjonalnej
     (hash commitu w `…` albo tag spec-vX.Y); gdy jest .git, hash albo tag
     musi istnieć (inaczej ostrzeżenie). Porównanie ID między hashem a tagiem
     (ADR-0020 pkt 3) czeka na powstanie tagu spec-v1.2;
  i. pokrycie: każde niewycofane wymaganie R-* ma komponent i test albo
     wiersz w tabeli „Wymagania bez komponentu” (sekcja „N.x Wymagania bez
     komponentu” albo jej podsekcja; pierwsza kolumna „Wymaganie”, kolumna
     „Powód”). Brak pokrycia to informacja, a z
     --koniec-etapu błąd (kryterium zakończenia etapu 3);
  j. odwołania do kodu prototypu:
     W4): `src/plik` z nazwą funkcji albo stałej w odwrotnych apostrofach tuż
     za nim (`src/main.cpp` `spij()`, także kilka nazw po przecinku albo „i”)
     wskazują plik, który istnieje w firmware/prototyp/, a nazwa z nawiasami
     musi mieć w tym pliku definicję albo deklarację (linia od kolumny 0
     z „nazwa(”), nazwa bez nawiasów musi w nim wystąpić; odwołanie po numerze
     linii (`src/main.cpp:405`) bez commitu (`src/main.cpp:405@c518470`) to
     błąd, bo numery przesuwają się z każdą zmianą kodu;

Zakresy i skróty ID R-* („R-DANE-01…R-DANE-05”, „R-DANE-01, 02”) rozwija
ta sama funkcja co w spojnosc.py. Spec funkcjonalna nie jest zmieniana
i nie powinna wskazywać ID T- (ADR-0020 pkt 3).

Użycie
------
  python3 tools/spojnosc_tech.py
  python3 tools/spojnosc_tech.py --macierz docs/spec/macierz-techniczna.csv
  python3 tools/spojnosc_tech.py TECH --spec SPEC --wycofane CSV --koniec-etapu

  --macierz       zapisuje CSV: jeden wiersz na wymaganie (R-* i T-WYM),
                  kolumny „wymaganie, sekcja_spec, komponenty, testy,
                  powod_braku”
  --koniec-etapu  brak pokrycia (i) i sekcje „do napisania” liczą się
                  jako błąd
  --kod KATALOG   katalog firmware prototypu dla sprawdzenia j
                  (domyślnie firmware/prototyp)

Kod wyjścia: 0 = brak błędów, 1 = są błędy, 2 = nie da się odczytać pliku.
Tylko biblioteka standardowa Pythona 3.
"""

import argparse
import csv
import os
import re
import subprocess
import sys
from collections import OrderedDict, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import spojnosc as s  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOMYSLNE = {
    "tech": os.path.join(REPO, "docs/spec/specyfikacja-techniczna.md"),
    "spec": os.path.join(REPO, "docs/spec/specyfikacja-funkcjonalna.md"),
    "wycofane": os.path.join(REPO, "tools/wycofane.csv"),
    "kod": os.path.join(REPO, "firmware/prototyp"),
}

# Rodziny T- (ADR-0020 pkt 4): jedno miejsce do zmiany.
RODZINY_T = OrderedDict([
    ("T-KOMP", r"\d{2}"),
    ("T-INT", r"\d{2}"),
    ("T-DANE", r"\d{2}"),
    ("T-WYM", r"\d{2}"),
    ("T-NF", r"\d{2}"),
    ("T-SOUP", r"\d{2}"),
    ("T-TEST", r"\d{3}"),
])
_G_PRZED, _G_PO = r"(?<![\w-])", r"(?![\w-])"
T_RE = re.compile(_G_PRZED + "((?:" + "|".join(p + "-" + c for p, c in RODZINY_T.items()) + "))" + _G_PO)
# Coś, co wygląda jak ID T-, ale nie pasuje do schematu.
T_KANDYDAT_RE = re.compile(_G_PRZED + r"(T-[A-Z]+-\d+)" + _G_PO)
T_DEF_RE = re.compile(r"(" + T_RE.pattern[len(_G_PRZED):-len(_G_PO)] + r")(?:\s*\(.*\))?")

STANY = ("do napisania", "szkic", "do przeglądu", "przyjęta")
DO_ZMIERZENIA_RE = re.compile(r"\[do zmierzenia\]", re.IGNORECASE)
# (f) Opis pomiaru przy [do zmierzenia]: „pomiar 4”, „pomiary 2 i 3” liczą się po numerach.
POMIAR_RE = re.compile(r"\bpomiar(?:y|ów)?\s+(\d{1,2}(?:(?:,\s*|\s+i\s+)\d{1,2})*)(?!\d)")
# Wersja spec funkcjonalnej: tylko w wierszu „Odwołanie” tabeli nagłówka.
WERSJA_SPEC_RE = re.compile(r"[Ss]pecyfikacj\w* funkcjonaln\w*.*?(?:`([0-9a-f]{7,40})`|(spec-v\d+\.\d+))")
BEZ_KOMPONENTU_RE = re.compile(r"^[\d.x]+\s+Wymagania bez komponentu$", re.IGNORECASE)
NUMER_SEKCJI_RE = re.compile(r"^(\d+)\.\s")
# Furtka w „Realizuje”/„Sprawdza”: „brak (powód)”, np. „brak (konfiguracja, nie kod)”.
BRAK_Z_POWODEM_RE = re.compile(r"^brak\s*\(.+\)$")
OPERATOR_ZAKRESU_RE = re.compile(r"(R-[A-Z]+-\d{2})\s*(?:…|–|do)\s*(?:R-[A-Z]+-)?(\d{2})(?![\w-])")
# (j) Odwołania do kodu prototypu: plik (także z przedrostkiem firmware/prototyp/),
# opcjonalnie linia z commitem, potem nazwy. Numer linii łapany także bez apostrofów.
KOD_PLIK = r"(?:firmware/prototyp/)?(src/[\w/]+\.(?:cpp|h))"
KOD_LINIA_RE = re.compile(r"(?<![\w/.])" + KOD_PLIK + r":(\d+)(?:-(\d+))?(?:@([0-9A-Za-z]+))?(?![\w-])")
_NAZWA = r"`[A-Za-z_]\w*(?:::[A-Za-z_]\w*)?(?:\(\))?`"
KOD_LANCUCH_RE = re.compile(r"`" + KOD_PLIK + r"(?::\d+(?:-\d+)?@[0-9a-f]{7,40})?`"
                            r"((?:(?:, | i | oraz | |: | \()" + _NAZWA + r")*)")
KOD_NAZWA_RE = re.compile(r"`([A-Za-z_]\w*(?:::[A-Za-z_]\w*)?)(\(\))?`")
HASH_RE = re.compile(r"[0-9a-f]{7,40}")


def rodzina_t(id_):
    return id_.rsplit("-", 1)[0]


def t_definicji(komorka):
    m = T_DEF_RE.fullmatch(s._czysc(komorka))
    return m.group(1) if m else None


def ids_r(tekst):
    """ID wymagań R-* w tekście, z rozwinięciem zakresów i skrótów."""
    return [i for i, _ in s.znajdz_id(tekst) if s.rodzina(i) == "R"]


def stosy_naglowkow(linie):
    """Dla każdej linii: lista tytułów nagłówków od najwyższego do bieżącego."""
    stos, wynik = [], []
    for linia in linie:
        mh = s.NAGLOWEK_RE.match(linia)
        if mh:
            poziom = len(mh.group(1))
            while stos and stos[-1][0] >= poziom:
                stos.pop()
            stos.append((poziom, mh.group(2).strip()))
        wynik.append([tyt for _, tyt in stos])
    return wynik


def numery_pomiarow(dok):
    """Numery z tabeli „Pomiary” planu pomiarów (pierwsza kolumna „Nr”, kolumna „Co mierzymy”);
    None, gdy w dokumencie nie ma takiej tabeli."""
    numery = None
    for t in dok.tabele:
        if not t["naglowek"] or t["naglowek"][0].strip().lower() != "nr" or \
                _kolumna(t["naglowek"], "co mierzymy") is None:
            continue
        numery = numery or set()
        for _nr, kom in t["wiersze"]:
            m = re.fullmatch(r"\d{1,2}", s._czysc(kom[0]) if kom else "")
            if m:
                numery.add(int(m.group(0)))
    return numery


def _kolumna(naglowek, *nazwy):
    for i, n in enumerate(naglowek):
        if n.strip().lower() in nazwy:
            return i
    return None


# ---------------------------------------------------------------------------
# Spec funkcjonalna: definicje (tylko odczyt)
# ---------------------------------------------------------------------------
def definicje_spec(tekst):
    """id -> sekcja, dla ID zdefiniowanych w spec (pierwsze wystąpienie)."""
    dok = s.Dokument(tekst)
    wynik = OrderedDict()
    for t in dok.tabele:
        if not t["naglowek"] or t["naglowek"][0] != "ID":
            continue
        for nr, kom in t["wiersze"]:
            if dok.w_wycofanych[nr - 1]:
                continue
            id_ = s.id_definicji(kom[0] if kom else "")
            if id_ and id_ not in wynik:
                wynik[id_] = dok.sekcje[nr - 1]
    return wynik


# ---------------------------------------------------------------------------
# Sprawdzenia
# ---------------------------------------------------------------------------
def ma_definicje(kod, nazwa):
    """Czy w tekście pliku C++ jest definicja albo deklaracja „nazwa(” od kolumny 0.
    Nazwa kwalifikowana „Klasa::metoda”: definicja „Klasa::metoda(” albo klasa
    „Klasa” w pliku i deklaracja „metoda(” (także z wcięciem, w ciele klasy)."""
    if "::" in nazwa:
        klasa, metoda = nazwa.rsplit("::", 1)
        if re.search(r"\b" + re.escape(nazwa) + r"\s*\(", kod):
            return True
        return bool(re.search(r"\b(?:class|struct)\s+" + re.escape(klasa) + r"\b", kod)
                    and re.search(r"(?<![\w.>])" + re.escape(metoda) + r"\s*\([^;{]*\)\s*(?:const\s*)?[;{]", kod))
    wzor = re.compile(r"^(?![\s#/*])[^\n;]*?(?<![\w.>])" + re.escape(nazwa) + r"\s*\(", re.M)
    return bool(wzor.search(kod))


def sprawdz_kod(dok, czytaj_kod, wynik, czytaj_commit=None):
    """(j) Odwołania do kodu prototypu po nazwie; czytaj_kod(ścieżka) -> tekst albo None;
    czytaj_commit(hash, ścieżka) -> tekst pliku w tym commicie albo None (None = bez
    sprawdzenia commitu)."""
    frag = s._fragment
    for i, linia in enumerate(dok.linie, start=1):
        for m in KOD_LINIA_RE.finditer(linia):
            plik, od, do, commit = m.group(1), int(m.group(2)), int(m.group(3) or m.group(2)), m.group(4)
            if not commit:
                wynik.dodaj("blad", "j", i, "Odwołanie do kodu po numerze linii bez commitu: %s; podaj nazwę "
                            "funkcji (`%s` `nazwa()`) albo commit (`%s:%s@hash`)."
                            % (m.group(0), plik, plik, m.group(2)), fragment=frag(linia, m.group(0)))
                continue
            if not HASH_RE.fullmatch(commit):
                wynik.dodaj("blad", "j", i, "Odwołanie %s: „%s” nie jest skrótem commitu." % (m.group(0), commit),
                            fragment=frag(linia, m.group(0)))
                continue
            if czytaj_commit is None:
                continue
            tekst = czytaj_commit(commit, plik)
            istnieje = getattr(czytaj_commit, "istnieje", None)
            if tekst is None and istnieje is not None and not istnieje(commit):
                # The public repository starts with a new history: commits pinned before it cannot be
                # checked here. Reported, not treated as an error; convert such references to names.
                wynik.dodaj("ostrzezenie", "j", i, "Odwołanie %s: commitu %s nie ma w historii tego repozytorium "
                            "(sprzed publikacji); zamień na odwołanie po nazwie." % (m.group(0), commit),
                            fragment=frag(linia, m.group(0)))
            elif tekst is None:
                wynik.dodaj("blad", "j", i, "Odwołanie %s: commitu %s nie ma w repozytorium albo nie ma w nim "
                            "pliku %s." % (m.group(0), commit, plik), fragment=frag(linia, m.group(0)))
            elif do > len(tekst.split("\n")) or od > do:
                wynik.dodaj("blad", "j", i, "Odwołanie %s: plik %s w %s ma %d linii." % (
                    m.group(0), plik, commit, len(tekst.split("\n"))), fragment=frag(linia, m.group(0)))
        for m in KOD_LANCUCH_RE.finditer(linia):
            plik = m.group(1)
            kod = czytaj_kod(plik)
            if kod is None:
                wynik.dodaj("blad", "j", i, "Plik %s nie istnieje w firmware/prototyp/." % plik,
                            fragment=frag(linia, plik))
                continue
            for mn in KOD_NAZWA_RE.finditer(m.group(2)):
                nazwa, funkcja = mn.group(1), bool(mn.group(2))
                if funkcja and not ma_definicje(kod, nazwa):
                    wynik.dodaj("blad", "j", i, "Funkcji %s() nie ma w %s (definicja albo deklaracja)."
                                % (nazwa, plik), fragment=frag(linia, nazwa))
                elif not funkcja and not re.search(r"\b" + re.escape(nazwa) + r"\b", kod):
                    wynik.dodaj("blad", "j", i, "Nazwy %s nie ma w %s." % (nazwa, plik), fragment=frag(linia, nazwa))


def czytnik_commitu(repo):
    """Tekst pliku firmware/prototyp/{plik} w commicie przez `git show`, albo None."""
    def czytaj(commit, plik):
        w = subprocess.run(["git", "-C", repo, "show", "%s:firmware/prototyp/%s" % (commit, plik)],
                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        return w.stdout.decode("utf-8", "replace") if w.returncode == 0 else None

    def istnieje(commit):
        return subprocess.run(["git", "-C", repo, "cat-file", "-e", "%s^{commit}" % commit],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
    czytaj.istnieje = istnieje
    return czytaj


def czytnik_kodu(katalog):
    def czytaj(plik):
        try:
            with open(os.path.join(katalog, plik), encoding="utf-8", errors="replace") as f:
                return f.read()
        except OSError:
            return None
    return czytaj


def sprawdz(tekst, spec_def, wycofane, koniec_etapu=False, ref_istnieje=None, czytaj_kod=None,
            czytaj_commit=None):
    """ref_istnieje: funkcja(hash albo tag) -> bool albo None (bez sprawdzenia w git);
    czytaj_kod: funkcja(ścieżka względem firmware/prototyp/) -> tekst albo None
    (None = bez sprawdzenia j); czytaj_commit: funkcja(hash, ścieżka) -> tekst albo None
    (None = bez sprawdzenia commitu w odwołaniach z numerem linii)."""
    dok = s.Dokument(tekst)
    stosy = stosy_naglowkow(dok.linie)
    pomiary = numery_pomiarow(dok)
    wynik = s.Wynik()
    frag = s._fragment

    def wycofane_r(id_):
        return wycofane.get(id_, {}).get("status") == "wycofane"

    # --- (a) definicje ------------------------------------------------------
    definicje = defaultdict(list)
    wiersz_def = {}  # nr linii -> (id, naglowek, komorki)
    for t in dok.tabele:
        if not t["naglowek"] or t["naglowek"][0] != "ID":
            continue
        for nr, kom in t["wiersze"]:
            if dok.w_wycofanych[nr - 1]:
                continue  # tabela wycofanych ID nie definiuje
            id_ = t_definicji(kom[0] if kom else "")
            if id_:
                definicje[id_].append(nr)
                wiersz_def[nr] = (id_, t["naglowek"], kom)
    for id_, linie in definicje.items():
        if len(linie) > 1:
            wynik.dodaj("blad", "a", linie[1], "ID %s zdefiniowane %d razy (l. %s)."
                        % (id_, len(linie), ", ".join(map(str, linie))), id_=id_)

    # --- (b) użycia, (c) wycofane, (f) [do zmierzenia] ----------------------
    for i, linia in enumerate(dok.linie, start=1):
        dobre = set(T_RE.findall(linia))
        for kand in T_KANDYDAT_RE.findall(linia):
            if kand not in dobre:
                wynik.dodaj("blad", "b", i, "„%s” wygląda jak ID T-, ale nie pasuje do schematu ADR-0020 "
                            "(rodziny %s; T-TEST trzy cyfry, pozostałe dwie)." % (kand, ", ".join(RODZINY_T)),
                            id_=kand, fragment=frag(linia, kand))
        for id_ in sorted(dobre):
            if i in wiersz_def and wiersz_def[i][0] == id_:
                pass
            elif id_ not in definicje and not wycofane_r(id_):  # „zmienione” musi mieć definicję
                wynik.dodaj("blad", "b", i, "ID %s użyte bez definicji." % id_, id_=id_, fragment=frag(linia, id_))
            if not dok.w_wycofanych[i - 1] and wycofane_r(id_):
                wynik.dodaj("blad", "c", i, "Odwołanie do wycofanego ID %s (następca: %s)."
                            % (id_, wycofane[id_]["nastepca"] or "brak"), id_=id_, fragment=frag(linia, id_))
        # R-* w dowolnym miejscu poza sekcjami „Wycofane…”: musi istnieć w spec i nie być wycofane.
        if not dok.w_wycofanych[i - 1]:
            for r in sorted(set(ids_r(linia))):
                if wycofane_r(r):
                    wynik.dodaj("blad", "d", i, "Odwołanie do wycofanego wymagania %s (następca: %s)."
                                % (r, wycofane[r]["nastepca"] or "brak"), id_=r, fragment=frag(linia, r))
                elif r not in spec_def:
                    wynik.dodaj("blad", "d", i, "Wymaganie %s nie istnieje w spec funkcjonalnej." % r,
                                id_=r, fragment=frag(linia, r))
            for m in OPERATOR_ZAKRESU_RE.finditer(linia):
                lewy = int(m.group(1)[-2:])
                if int(m.group(2)) - lewy > 60:
                    wynik.dodaj("ostrzezenie", "d", i, "Zakres %s nie jest rozwijany (ponad 60 numerów); "
                                "wypisz go w częściach." % m.group(0), fragment=frag(linia))
        # Znacznik w odwrotnych apostrofach to wzmianka o konwencji, nie znacznik.
        znaczniki = len(DO_ZMIERZENIA_RE.findall(s._SEG_RE.sub("", linia)))
        if znaczniki:
            opisy = POMIAR_RE.findall(linia)
            if len(opisy) < znaczniki:
                wynik.dodaj("blad", "f", i, "[do zmierzenia] (%d na linii) bez własnego opisu pomiaru "
                            "„pomiar N” z planu pomiarów (%d)." % (znaczniki, len(opisy)), fragment=frag(linia))
            for opis in opisy:
                for n in map(int, re.findall(r"\d+", opis)):
                    if pomiary is not None and n not in pomiary:
                        wynik.dodaj("blad", "f", i, "Pomiar %d przy [do zmierzenia] nie stoi w tabeli „Pomiary” "
                                    "planu pomiarów." % n, fragment=frag(linia, "pomiar"))

    # --- (d) Realizuje / Sprawdza, (e) T-WYM -------------------------------
    realizuje = defaultdict(list)  # wymaganie -> [T-KOMP]
    sprawdza = defaultdict(list)   # wymaganie -> [T-TEST]
    for nr, (id_, nagl, kom) in sorted(wiersz_def.items()):
        r = rodzina_t(id_)
        if r in ("T-KOMP", "T-TEST"):
            nazwa = "realizuje" if r == "T-KOMP" else "sprawdza"
            k = _kolumna(nagl, nazwa)
            if k is None or k >= len(kom) or not s._czysc(kom[k]):
                wynik.dodaj("blad", "d", nr, "%s nie ma kolumny „%s” albo jest pusta." % (id_, nazwa.capitalize()),
                            id_=id_)
                continue
            komorka = kom[k]
            dozwolone = ("T-WYM",) if r == "T-KOMP" else ("T-WYM", "T-NF")
            obce = [x for x in T_RE.findall(komorka) if rodzina_t(x) not in dozwolone]
            if obce:
                wynik.dodaj("blad", "d", nr, "%s: w kolumnie „%s” wolno wskazać R-* i %s, nie %s."
                            % (id_, nazwa.capitalize(), ", ".join(dozwolone), ", ".join(obce)), id_=id_)
            cele = ids_r(komorka) + [x for x in T_RE.findall(komorka) if rodzina_t(x) in dozwolone]
            if not cele and not BRAK_Z_POWODEM_RE.match(s._czysc(komorka)):
                wynik.dodaj("blad", "d", nr, "%s: kolumna „%s” bez ID wymagań (R-*, %s) i bez „brak (powód)”."
                            % (id_, nazwa.capitalize(), ", ".join(dozwolone)), id_=id_, fragment=frag(komorka))
            for cel in cele:
                if cel.startswith("R-"):
                    if cel not in spec_def or wycofane_r(cel):
                        continue  # zgłoszone przy przeglądzie linii
                elif cel not in definicje:
                    continue  # zgłoszone w (b)
                (realizuje if r == "T-KOMP" else sprawdza)[cel].append(id_)
        elif r == "T-WYM":
            kw = _kolumna(nagl, "weryfikacja")
            kz = _kolumna(nagl, "wzorzec")
            kt = _kolumna(nagl, "wymaganie")
            if kw is None or kw >= len(kom) or not s._czysc(kom[kw]):
                wynik.dodaj("blad", "e", nr, "%s nie ma kolumny „Weryfikacja” albo jest pusta." % id_, id_=id_)
            else:
                s.sprawdz_weryfikacje(id_, nr, kom[kw], wynik, strict=True)
            if kz is not None and kt is not None and kz < len(kom) and kt < len(kom):
                s.sprawdz_ears(id_, nr, s._czysc(kom[kz]), kom[kt], wynik)
            else:
                wynik.dodaj("blad", "e", nr, "%s: tabela T-WYM potrzebuje kolumn „Wzorzec” i „Wymaganie”." % id_,
                            id_=id_)

    # --- (g) stan sekcji, (h) wersja spec ----------------------------------
    stany = OrderedDict()
    for t in dok.tabele:
        if not t["naglowek"] or t["naglowek"][0].lower() != "sekcja":
            continue
        k = _kolumna(t["naglowek"], "stan")
        if k is None:
            continue
        for nr, kom in t["wiersze"]:
            stan = s._czysc(kom[k]).lower() if k < len(kom) else ""
            nazwa = s._czysc(kom[0])
            if nazwa in stany:
                wynik.dodaj("blad", "g", nr, "Sekcja „%s” dwa razy w tabeli stanu." % nazwa)
            stany[nazwa] = stan
            if stan not in STANY:
                wynik.dodaj("blad", "g", nr, "Sekcja „%s”: nieznany stan „%s” (dozwolone: %s)."
                            % (s._czysc(kom[0]), stan, ", ".join(STANY)))
            elif stan == "do napisania" and koniec_etapu:
                wynik.dodaj("blad", "g", nr, "Sekcja „%s” ma stan „do napisania”." % s._czysc(kom[0]))
    if not stany:
        wynik.dodaj("blad", "g", 0, "Brak tabeli stanu sekcji (pierwsza kolumna „Sekcja”, kolumna „Stan”).")
    else:
        # Kompletność: numery sekcji w tabeli = numery nagłówków „## N. …” w dokumencie.
        w_tabeli = {m.group(1) for m in (NUMER_SEKCJI_RE.match(n) for n in stany) if m}
        w_tekscie = {m.group(1) for m in (NUMER_SEKCJI_RE.match(l[3:]) for l in dok.linie if l.startswith("## ")) if m}
        for n in sorted(w_tekscie - w_tabeli, key=int):
            wynik.dodaj("blad", "g", 0, "Sekcja %s. jest w dokumencie, a nie ma jej w tabeli stanu." % n)
        for n in sorted(w_tabeli - w_tekscie, key=int):
            wynik.dodaj("blad", "g", 0, "Sekcja %s. jest w tabeli stanu, a nie ma jej nagłówka w dokumencie." % n)
    ref = None
    if dok.tabele and dok.tabele[0]["start"] <= 40:
        for nr, kom in dok.tabele[0]["wiersze"]:
            if s._czysc(kom[0]).lower() == "odwołanie" and len(kom) > 1:
                m = WERSJA_SPEC_RE.search(kom[1])
                if m:
                    ref = m.group(1) or m.group(2)
    if not ref:
        wynik.dodaj("blad", "h", 1, "Wiersz „Odwołanie” tabeli nagłówka nie wskazuje wersji spec funkcjonalnej "
                    "(hash commitu w `…` albo tag spec-vX.Y).")
    elif ref_istnieje is not None and not ref_istnieje(ref):
        wynik.dodaj("ostrzezenie", "h", 1, "Wersja spec %s z nagłówka nie istnieje w repozytorium git "
                    "(sprawdź hash albo czy tag już powstał)." % ref)

    # --- (i) pokrycie --------------------------------------------------------
    bez_komponentu = OrderedDict()
    for t in dok.tabele:
        if not t["naglowek"] or t["naglowek"][0].lower() != "wymaganie":
            continue
        if not any(BEZ_KOMPONENTU_RE.match(tyt) for tyt in stosy[t["start"] - 1]):
            continue
        kp = _kolumna(t["naglowek"], "powód")
        for nr, kom in t["wiersze"]:
            powod = s._czysc(kom[kp]) if kp is not None and kp < len(kom) else ""
            for r in ids_r(kom[0]):
                if not powod:
                    wynik.dodaj("blad", "i", nr, "%s w „Wymagania bez komponentu” bez powodu." % r, id_=r)
                if r not in spec_def:
                    wynik.dodaj("blad", "i", nr, "%s w „Wymagania bez komponentu” nie istnieje w spec." % r, id_=r)
                bez_komponentu[r] = powod
    wymagania = [i for i in spec_def if s.rodzina(i) == "R" and not wycofane_r(i)]
    wymagania += sorted(i for i in definicje if rodzina_t(i) == "T-WYM")
    braki = 0
    for w in wymagania:
        # Test bez komponentu nie przeczy tabeli (np. przegląd licencji albo test wariantu zastępczego).
        if w in bez_komponentu and realizuje.get(w):
            wynik.dodaj("ostrzezenie", "i", 0, "%s ma komponent, a stoi też w „Wymagania bez komponentu”."
                        % w, id_=w)
        if w in bez_komponentu or (realizuje.get(w) and sprawdza.get(w)):
            continue
        braki += 1
        if koniec_etapu:
            czego = " i ".join(x for x, ok in (("komponentu", realizuje.get(w)), ("testu", sprawdza.get(w))) if not ok)
            wynik.dodaj("blad", "i", 0, "%s nie ma %s ani wiersza w „Wymagania bez komponentu”." % (w, czego), id_=w)
    if braki and not koniec_etapu:
        wynik.dodaj("info", "i", 0, "Bez pełnego pokrycia (komponent i test): %d z %d wymagań. "
                    "Z --koniec-etapu to błędy." % (braki, len(wymagania)))

    if czytaj_kod is not None:
        sprawdz_kod(dok, czytaj_kod, wynik, czytaj_commit)

    stat = OrderedDict()
    stat["definicje"] = OrderedDict((r, sum(1 for i in definicje if rodzina_t(i) == r)) for r in RODZINY_T)
    stat["stany_sekcji"] = OrderedDict((st, sum(1 for v in stany.values() if v == st)) for st in STANY)
    stat["wymagania"] = len(wymagania)
    stat["pokryte"] = len(wymagania) - braki
    stat["do_zmierzenia"] = sum(len(DO_ZMIERZENIA_RE.findall(s._SEG_RE.sub("", l))) for l in dok.linie)
    macierz = []
    for w in wymagania:
        sekcja = spec_def.get(w, "spec techniczna")
        macierz.append((w, sekcja, "; ".join(sorted(set(realizuje.get(w, [])))),
                        "; ".join(sorted(set(sprawdza.get(w, [])))), bez_komponentu.get(w, "")))
    return wynik, stat, macierz


def raport(sciezka, wynik, stat):
    out = ["Spójność specyfikacji technicznej: %s" % os.path.relpath(sciezka, REPO)]
    for poziom, nazwa in (("blad", "BŁĄD"), ("ostrzezenie", "OSTRZEŻENIE"), ("info", "INFO")):
        for w in wynik.wpisy:
            if w["poziom"] == poziom:
                out.append("  %s [%s] l. %s: %s" % (nazwa, w["sprawdzenie"], w["linia"], w["tresc"]))
    bl = sum(1 for w in wynik.wpisy if w["poziom"] == "blad")
    os_ = sum(1 for w in wynik.wpisy if w["poziom"] == "ostrzezenie")
    out.append("Definicje T-: " + ", ".join("%s %d" % kv for kv in stat["definicje"].items()))
    out.append("Stany sekcji: " + ", ".join("%s %d" % kv for kv in stat["stany_sekcji"].items()))
    out.append("Pokrycie: %d z %d wymagań ma komponent i test albo powód braku; [do zmierzenia]: %d"
               % (stat["pokryte"], stat["wymagania"], stat["do_zmierzenia"]))
    out.append("Wynik: %d błędów, %d ostrzeżeń." % (bl, os_))
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Spójność specyfikacji technicznej (ADR-0020).")
    ap.add_argument("tech", nargs="?", default=DOMYSLNE["tech"])
    ap.add_argument("--spec", default=DOMYSLNE["spec"])
    ap.add_argument("--wycofane", default=DOMYSLNE["wycofane"])
    ap.add_argument("--kod", default=DOMYSLNE["kod"], help="katalog firmware prototypu (sprawdzenie j)")
    ap.add_argument("--macierz")
    ap.add_argument("--koniec-etapu", action="store_true")
    a = ap.parse_args(argv)
    try:
        with open(a.tech, encoding="utf-8") as f:
            tech = f.read()
        with open(a.spec, encoding="utf-8") as f:
            spec_def = definicje_spec(f.read())
        wycofane = s.wczytaj_wycofane(a.wycofane)
    except (OSError, UnicodeDecodeError) as e:
        print("Nie da się odczytać pliku: %s" % e, file=sys.stderr)
        return 2
    ref_istnieje = None
    czytaj_commit = None
    if os.path.exists(os.path.join(REPO, ".git")):  # także gdy .git jest plikiem, a nie katalogiem
        czytaj_commit = czytnik_commitu(REPO)
        def ref_istnieje(ref):
            return subprocess.run(["git", "-C", REPO, "rev-parse", "--verify", "--quiet", ref + "^{commit}"],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
    wynik, stat, macierz = sprawdz(tech, spec_def, wycofane, a.koniec_etapu, ref_istnieje,
                                   czytnik_kodu(a.kod), czytaj_commit)
    print(raport(a.tech, wynik, stat))
    if a.macierz:
        with open(a.macierz, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["wymaganie", "sekcja_spec", "komponenty", "testy", "powod_braku"])
            w.writerows(macierz)
    return 1 if any(x["poziom"] == "blad" for x in wynik.wpisy) else 0


if __name__ == "__main__":
    sys.exit(main())
