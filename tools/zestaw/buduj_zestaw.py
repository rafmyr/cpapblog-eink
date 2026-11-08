#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Budowa zestawu danych reguł.

Wejście:
  docs/spec/progi.csv               progi (separator `,`, nazwy kolumn w pierwszym wierszu)
  docs/spec/katalog/{E,L,H,O}.csv   wiersze katalogu: pola 9.3 + kolumny stan;pilnosc;zrodlo (separator `;`)
  docs/spec/katalog/teksty.csv      id_tekstu;rola;limit;tekst (tekst do końca wiersza)
  docs/spec/specyfikacja-funkcjonalna.md   źródło tekstów (każdy tekst musi w niej stać dosłownie)

Wyjście:
  firmware/prototyp/src/zestaw_wbudowany.h   trzy pliki jako stałe tekstowe (zestaw wbudowany, `pochodzenie=wbudowany`)
  --wyjscie KATALOG                          opcjonalnie trzy pliki progi.csv, katalog.csv, teksty.csv (pakiet na kartę)

Kontrole (błąd = kod wyjścia 1 i komunikat po polsku na stderr):
  - każdy `id_progu` z katalogu (parametry progowe i pole `okno`) jest w progi.csv;
  - każdy rodzaj warunku jest z tabeli 9.3 (RODZAJE), każdy parametr z kontraktu 6 (PARAMETRY);
  - każdy tekst katalogu jest w teksty.csv i stoi dosłownie w spec funkcjonalnej (białe znaki znormalizowane,
    `{prog:id}` zamienione na wartość progu, `{zmienna}` pasuje do dowolnego ciągu);
  - liczba w tekście równa wartości progu tej reguły, wpisana bez `{prog:…}` = błąd;
  - tekst dłuższy niż limit (po wstawieniu progów i najdłuższych wartości zmiennych, SZEROKOSC_ZMIENNYCH) = błąd;
  - pola `stan`, `pilnosc`, `liczone_noce`, `wymaga_normy`, `w_rozruchu` z dozwolonych wartości;
  - zależności (`wymaga`, `zlozenie.skladniki`, `trwanie.regula`) bez cyklu: wiersze są układane tak, żeby składnik
    był oceniany przed regułą, która go używa (poza tym kolejność plików E, L, H i wierszy w pliku).

Skrót `skrot=sha256:…` liczy się z bajtów UTF-8 pliku po pierwszym wierszu (z końcowym `\\n`), tak jak
reguly::wczytajZestaw() w src/reguly.cpp.

Użycie: python3 tools/zestaw/buduj_zestaw.py [--wyjscie KAT] [--pochodzenie wbudowany|karta] [--firmware 1.0.0]
        [--firmware-do 1.99.99] [--zrodlo HASH] [--zamrozenie TAG] [--bez-h]
"""

import argparse
import csv
import hashlib
import io
import os
import re
import subprocess
import sys

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(TU))

# Tabela rodzajów warunków ze spec technicznej 9.3 (27 nazw).
RODZAJE = [
    "prog_nocy", "czas_poza_progiem", "prog_biezacy", "k_z_n_nocy", "kolejne_noce", "statystyka_okna",
    "okno_wobec_normy", "odchylenie_nocy", "stan_odchylenia", "porownanie_okien", "stosunek_metryk",
    "prog_pochodny", "porownanie_grup", "rekord_historii", "kamien", "licznik_pokazan", "trend",
    "rozklad_w_nocy", "epizody_serii", "percentyl_sezonu", "blisko_ustawienia", "odpowiedzi_tygodni",
    "trwanie", "od_zdarzenia", "zawsze", "sygnal_komponentu", "zlozenie",
]
# Rodzaje i parametry spoza tabeli 9.3 deklaruje każda we własnym pliku docs/spec/katalog/rodzaje-*.csv
# (`nazwa;typ;parametry`, typ: `rodzaj`, `parametr_prog`, `parametr_slowny`; przy rodzaju `parametry` = lista
# dozwolonych parametrów po przecinku albo puste = bez kontroli), żeby nie zmieniały wspólnej listy w skrypcie.
TYPY_DEKLARACJI = ("rodzaj", "parametr_prog", "parametr_slowny")
# Parametry z kontraktu 6 i dopisane w silniku reguł (opis na górze src/reguly.cpp): nazwa → czy wartość
# jest id_progu (True) czy stałą słowną (False).
PARAMETRY = {
    "metryka": False, "prog": True, "por": False, "kzn": True, "n": True, "stat": False, "wzgledem": False,
    "delta": True, "razy": True, "regula": False, "zdarzenie": False, "min": True, "max": True,
    "jednostka": False, "pytanie": False, "odpowiedz": False, "okno_a": True, "okno_b": True, "op": False,
    "skladniki": False, "sygnal": False,
    # dopisane 04.11.2026: próg warunku jednej nocy w statystyce `udzial` i jego kierunek
    "prog_nocy": True, "por_nocy": False,
    # dopisane przez (rodzaje fali 3, opis w src/reguly_rodzaje.cpp)
    "metryki": False, "delta_z": False, "rocznica": False, "prog_udzial": True, "cecha": False,
    # dopisane 08.11.2026: noc bez wartości metryki w `prog_nocy` i `k_z_n_nocy` (L-09, ADR-0034)
    "brak": False,
}
# Słowa z przeglądu granicy ADR-0002 (9.3): tekst z trafieniem tylko ostrzeżenie (tekst i tak musi stać dosłownie
# w spec funkcjonalnej, np. `EPAP wyższe niż zwykle` z 4.11 jest dozwolony); przegląd ręczny według 9.3.
SLOWA_GRANICY = ["cmH2O", "EPR", "EPAP", "PS", "wyższ", "niższ", "obniż", "podnieś", "ustaw"]
POLA_KATALOGU = ["id_reguly", "wariant", "rodzaj", "parametry", "liczone_noce", "okno", "poziom", "grupa", "slot",
                 "tryby", "wymaga_normy", "w_rozruchu", "wyklucza", "wymaga", "bramki", "limit", "teksty"]
POLA_BETY = ["stan", "pilnosc"]
POLA_ZRODLA = POLA_KATALOGU + POLA_BETY + ["zrodlo"]
LICZONE_NOCE = ["", "kompletne", "z_rekordem", "z_terapia", "z_terapia_bez_niepewnych", "wazne", "wazne_bez_niepewnych",
                "wszystkie", "z_terapia_spo2"]
STANY = ["aktywna", "tylko_raport", "zablokowana"]
PILNOSCI = ["", "A", "B", "C"]
TAK_NIE = ["tak", "nie", ""]
# Limity 5.4.4 (ekran danych) dla roli bez podanego limitu.
LIMITY_ROLI = {"tytul": 30, "powod": 70, "krok": 60, "linia": 60, "zdanie_budzika": 45, "pytanie": 60}
# Najdłuższa wartość zmiennej w znakach (do kontroli limitu); reszta: SZEROKOSC_DOMYSLNA.
SZEROKOSC_ZMIENNYCH = {"n": 2, "z": 2, "x": 4, "wartosc": 4, "norma": 4, "data": 5, "od": 5, "nocy": 3}
SZEROKOSC_DOMYSLNA = 4
TRYBY = "CPAP,AutoSet,ASV,ASVAuto"


class BladZestawu(Exception):
    pass


def norm(t):
    return re.sub(r"\s+", " ", t).strip()


def liczba(t):
    try:
        return float(t.replace(",", "."))
    except (ValueError, AttributeError):
        return None


def bez_bom(sciezka, nazwa):
    with open(sciezka, "rb") as f:
        if f.read(3) == b"\xef\xbb\xbf":
            raise BladZestawu("%s: plik zaczyna się znacznikiem BOM (zapis z Excela); zapisz jako UTF-8 bez BOM" % nazwa)


def wczytaj_progi(sciezka):
    """id_progu → lista wierszy (9.3: kilka wierszy jednego id z różnym `tryb`)."""
    bez_bom(sciezka, "progi.csv")
    with open(sciezka, encoding="utf-8", newline="") as f:
        wiersze = list(csv.DictReader(f))
    progi = {}
    for w in wiersze:
        if not w.get("id_progu"):
            raise BladZestawu("progi.csv: wiersz bez id_progu")
        for inny in progi.get(w["id_progu"], []):
            if set(inny["tryb"].split(";")) & set(w["tryb"].split(";")) or "wszystkie" in (inny["tryb"], w["tryb"]) \
                    and inny["tryb"] == w["tryb"]:
                raise BladZestawu("progi.csv: powtórzony id_progu %s z trybem %s" % (w["id_progu"], w["tryb"]))
        progi.setdefault(w["id_progu"], []).append(w)

    def cel_wiersz(cel, tryb):
        kandydaci = progi[cel]
        for k in kandydaci:
            if k["tryb"] == tryb:
                return k
        for k in kandydaci:
            if k["tryb"] == "wszystkie":
                return k
        return kandydaci[0]

    def wartosc(w, glebia=0):
        v = w["wartosc"].strip()
        if v.startswith("="):
            cel = v[1:]
            if cel not in progi or glebia > 10:
                raise BladZestawu("progi.csv: %s odwołuje się do nieznanego progu %s" % (w["id_progu"], cel))
            return wartosc(cel_wiersz(cel, w["tryb"]), glebia + 1)
        return v

    for lst in progi.values():
        for w in lst:
            w["_wartosc"] = wartosc(w)
    return progi


def glowny(progi, idp):
    """Wiersz progu do tekstów i kontroli: `wszystkie`, inaczej pierwszy."""
    for w in progi[idp]:
        if w["tryb"] == "wszystkie":
            return w
    return progi[idp][0]


def wartosci_progu(progi, idp):
    return [x for x in (liczba(w["_wartosc"]) for w in progi[idp]) if x is not None]


def czytaj_srednikowy(sciezka, pola_wymagane, nazwa):
    bez_bom(sciezka, nazwa)
    with open(sciezka, encoding="utf-8") as f:
        linie = [l.rstrip("\n").rstrip("\r") for l in f]
    if not linie:
        raise BladZestawu("%s: plik pusty (pierwszy wiersz to nazwy pól)" % nazwa)
    nazwy = linie[0].split(";")
    if nazwy[: len(pola_wymagane)] != pola_wymagane:
        raise BladZestawu("%s: pierwszy wiersz musi zaczynać się od pól %s" % (nazwa, ";".join(pola_wymagane)))
    wynik = []
    for nr, l in enumerate(linie[1:], start=2):
        if not l.strip() or l.startswith("#"):
            continue
        pola = l.split(";", len(nazwy) - 1)
        if len(pola) != len(nazwy):
            raise BladZestawu("%s, wiersz %d: %d pól zamiast %d" % (nazwa, nr, len(pola), len(nazwy)))
        d = dict(zip(nazwy, pola))
        d["_nr"] = nr
        d["_plik"] = nazwa
        wynik.append(d)
    return wynik


def parametry(tekst, gdzie):
    p = {}
    for kawalek in [x for x in tekst.split(",") if x.strip()]:
        if "=" not in kawalek:
            raise BladZestawu("%s: parametr bez `=`: %s" % (gdzie, kawalek))
        k, v = kawalek.split("=", 1)
        p[k.strip()] = v.strip()
    return p


def lista(t):
    return [x.strip() for x in t.split(",") if x.strip()]


def format_progu(v):
    """Jak reguly::liczbaProgu: do 3 miejsc po przecinku bez zer na końcu."""
    x = liczba(v)
    if x is None:
        return v
    t = ("%.3f" % x).rstrip("0").rstrip(".")
    return "0" if t == "-0" else t.replace(".", ",")


def podstaw_progi(tekst, progi, gdzie):
    def zam(m):
        idp = m.group(1)
        if idp not in progi:
            raise BladZestawu("%s: {prog:%s} wskazuje próg, którego nie ma w progi.csv" % (gdzie, idp))
        return format_progu(glowny(progi, idp)["_wartosc"])

    return re.sub(r"\{prog:([^}]+)\}", zam, tekst)


def klucz(w):
    return w["id_reguly"] + ("/" + w["wariant"] if w["wariant"] else "")


def zaleznosci(w):
    p = w["_param"]
    z = list(lista(w["wymaga"]))
    if w["rodzaj"] == "zlozenie" and "skladniki" in p:
        z += [x for x in p["skladniki"].split("+") if x]
    if w["rodzaj"] == "trwanie" and "regula" in p:
        z.append(p["regula"])
    return z


def sprawdz_odwolania(wiersze, katalog_dir):
    """Każde odwołanie do reguły w `wyklucza`, `wymaga` i parametrach `regula`, `skladniki` wskazuje
    istniejący wiersz katalogów E, L, H, O albo SK (ekrany kontekstowe, SK.csv): `id/wariant` dokładnie ten wariant,
    samo `id` dowolny wiersz tego id; reguła, której w katalogach jeszcze nie ma, nie jest błędem. Wariant bez celu (np. `S-K-10/od_kilku_nocy` przy wariancie `kilka`) daje
    regułę wiecznie „nie oceniam” albo martwe wykluczenie, więc jest błędem zestawu."""
    klucze = set(klucz(w) for w in wiersze)
    ids = set(w["id_reguly"] for w in wiersze)
    sk = os.path.join(katalog_dir, "SK.csv")
    if os.path.exists(sk):
        with open(sk, encoding="utf-8-sig") as f:
            for i, l in enumerate(f):
                c = l.rstrip("\r\n").split(";")
                if i == 0 or not c or not c[0] or c[0].startswith("#"):
                    continue
                ids.add(c[0])
                klucze.add(c[0] + ("/" + c[1] if len(c) > 1 and c[1] else ""))
    for w in wiersze:
        refs = [("wyklucza", x) for x in lista(w["wyklucza"])] + [("wymaga", x) for x in lista(w["wymaga"])]
        for k in ("regula", "skladniki"):
            if k in w["_param"]:
                refs += [(k, x) for x in w["_param"][k].split("+") if x]
        for pole, x in refs:
            x = x.strip()
            # reguła spoza katalogu (np. E-31 przed zdarzeniami E) nie jest błędem; wariant, którego dana reguła nie ma, tak
            if x and "/" in x and x.split("/")[0] in ids and x not in klucze:
                raise BladZestawu("%s, wiersz %d (%s): %s=%s: odwołanie do reguły albo wariantu, którego nie ma w "
                                  "katalogach E, L, H, O, SK" % (w["_plik"], w["_nr"], klucz(w), pole, x))


def uloz(wiersze):
    """Stabilne sortowanie topologiczne: składnik przed regułą, która go używa."""
    wg = {}
    for w in wiersze:
        wg.setdefault(w["id_reguly"], []).append(w)
        wg.setdefault(klucz(w), []).append(w) if w["wariant"] else None
    wynik, stan = [], {}

    def odwiedz(w, sciezka):
        k = id(w)
        if stan.get(k) == 2:
            return
        if stan.get(k) == 1:
            raise BladZestawu("katalog: cykl zależności %s" % " → ".join(sciezka + [klucz(w)]))
        stan[k] = 1
        for z in zaleznosci(w):
            for c in wg.get(z, []):
                if c is not w:
                    odwiedz(c, sciezka + [klucz(w)])
        stan[k] = 2
        wynik.append(w)

    for w in wiersze:
        odwiedz(w, [])
    return wynik


def sprawdz_tekst_w_spec(tekst, spec_norm, gdzie):
    t = norm(tekst)
    czesci = re.split(r"\{[^}:]+\}", t)
    # zmienna to krótki ciąg w jednym zdaniu, nie dowolny kawałek spec
    wzor = r"[^.`\n]{1,40}?".join(re.escape(c) for c in czesci)
    if not re.search(wzor, spec_norm):
        raise BladZestawu("%s: tekst `%s` nie stoi dosłownie w spec funkcjonalnej (granica ADR-0002)" % (gdzie, tekst))


# (ADR-0002, 9.3 „mechanizmy”): tekst z drogą albo wartością zmiany ustawień zatrzymuje budowę
# nawet wtedy, gdy stoi w spec (spec cytuje zakazane przykłady, np. `Obniż EPR o 1`).
CZASOWNIKI_ZMIANY = (r"(?i)(?<!\w)(obniż|obniżyć|obniżcie|podnieś|podnieść|ustaw|ustawić|ustawcie|zmień|zmienić|"
                     r"zmieńcie|zwiększ|zwiększyć|zmniejsz|zmniejszyć|przestaw|przestawić|wyłącz|wyłączyć|włącz|"
                     r"włączyć)(?!\w)")
USTAWIENIA = r"(?i)(EPR|EPAP|IPAP|(?<!\w)PS(?!\w)|ciśnieni|minimum|maksimum|cmH2O|ustawie|tryb|rampę|ramp|wilgotno)"
DROGA = r"(?i)(?<!\w)menu(?!\w)"
LICZBA_CISNIENIA = r"\d+(?:[.,]\d+)?\s*cmH2O"


def sprawdz_granice(tekst, rola, gdzie):
    if re.search(CZASOWNIKI_ZMIANY, tekst) and re.search(USTAWIENIA, tekst):
        raise BladZestawu("%s: tekst `%s` podaje zmianę ustawień aparatu (granica ADR-0002)" % (gdzie, tekst))
    if re.search(DROGA, tekst):
        raise BladZestawu("%s: tekst `%s` podaje drogę w menu (granica ADR-0002)" % (gdzie, tekst))
    if rola == "krok" and re.search(LICZBA_CISNIENIA, tekst):
        raise BladZestawu("%s: krok `%s` z wartością ciśnienia (granica ADR-0002)" % (gdzie, tekst))


# zmienne, które rodzaj rdzenia wypełnia (src/reguly.cpp); rodzaj spoza listy (np. zdarzeń E, wzorców L) bez
# kontroli zmiennych, dopóki nie dopisze tu swojej listy.
ZMIENNE_RODZAJU = {
    "prog_nocy": {"x", "wartosc", "data"},
    "k_z_n_nocy": {"n", "z", "x", "data"},
    "statystyka_okna": {"wartosc", "n", "z", "x", "data"},
    "kolejne_noce": {"z", "x", "data"},
    "okno_wobec_normy": {"wartosc", "norma", "z", "x", "data"},
    "odchylenie_nocy": {"n", "z", "x", "norma", "sygnaly"},
    "trend": {"wartosc"},
    "rekord_historii": {"wartosc", "n"},
    "kamien": {"n", "data"},
    "czas_poza_progiem": {"x", "wartosc"},
    "porownanie_grup": {"wartosc", "p"},
    "percentyl_sezonu": {"wartosc"},
    "stan_odchylenia": {"od", "nocy", "n", "norma", "x"},
    "porownanie_okien": {"wartosc", "wartosc_b", "z"},
    "trwanie": {"n", "od"},
    "od_zdarzenia": {"n", "data"},
    "odpowiedzi_tygodni": {"n", "z", "data"},
    "zawsze": set(),
    "zlozenie": set(),
    "prog_biezacy": {"wartosc", "x"},
}


def wczytaj_deklaracje(katalog_dir):
    """Rodzaje i parametry paczek z docs/spec/katalog/rodzaje-*.csv: (rodzaj → lista parametrów albo None, parametry)."""
    rodzaje, parametry_ = {}, {}
    for nazwa in sorted(os.listdir(katalog_dir)):
        if not (nazwa.startswith("rodzaje-") and nazwa.endswith(".csv")):
            continue
        for d in czytaj_srednikowy(os.path.join(katalog_dir, nazwa), ["nazwa", "typ", "parametry"], nazwa):
            g = "%s, wiersz %d" % (nazwa, d["_nr"])
            if d["typ"] not in TYPY_DEKLARACJI:
                raise BladZestawu("%s: typ `%s` spoza %s" % (g, d["typ"], ", ".join(TYPY_DEKLARACJI)))
            # rodzaj z tabeli 9.3 może zadeklarować, która go implementuje (np. blisko_ustawienia)
            if d["nazwa"] in rodzaje or d["nazwa"] in parametry_ or (d["typ"] != "rodzaj" and d["nazwa"] in PARAMETRY):
                raise BladZestawu("%s: `%s` już zadeklarowany (w rdzeniu albo w innej deklaracji)" % (g, d["nazwa"]))
            if d["typ"] == "rodzaj":
                rodzaje[d["nazwa"]] = lista(d["parametry"]) or None
            else:
                parametry_[d["nazwa"]] = d["typ"] == "parametr_prog"
    return rodzaje, parametry_


# Kontrola liczby progu w tekście: liczba przyklejona do liter (`K2`, `M-07`, `S2a`) nie jest
# liczbą; liczba z jednostką inną niż jednostka progu (`od 2 tygodni` wobec 2 cmH2O) nie jest tym progiem.
LICZBA_W_TEKSCIE = r"(?<![A-Za-z0-9-])\d+(?:,\d+)?(?![A-Za-z0-9])"
JEDNOSTKI_PO_LICZBIE = [("L/min", r"L\s*/\s*min"), ("/min", r"/\s*min"), ("/h", r"/\s*h"), ("cmH2O", r"cmH2O"),
                        ("min", r"min"), ("h", r"h(?![A-Za-ząćęłńóśźż])"), ("%", r"%"), ("noce", r"noc"),
                        ("dni", r"(dni|dzie|dob)"), ("tygodnie", r"tydz|tyg"), ("miesiące", r"miesi")]


def jednostka_po(tekst, koniec):
    reszta = tekst[koniec:koniec + 12].lstrip()
    for nazwa, wz in JEDNOSTKI_PO_LICZBIE:
        if re.match(wz, reszta):
            return nazwa
    return None


def baza_jednostki(j):
    j = j.split(" (")[0].strip()
    return {"noc trwania": "noce", "noc okresu": "noce"}.get(j, j)


def buduj(progi_csv, katalog_dir, spec_md, rodzaje_dodatkowe=(), ostrzezenia=None):
    rodzaje_paczek, parametry_paczek = wczytaj_deklaracje(katalog_dir)
    znane = set(RODZAJE) | set(rodzaje_paczek) | set(rodzaje_dodatkowe)
    wszystkie_parametry = dict(PARAMETRY)
    wszystkie_parametry.update(parametry_paczek)
    progi = wczytaj_progi(progi_csv)
    bez_bom(spec_md, "spec funkcjonalna")
    with open(spec_md, encoding="utf-8") as f:
        spec_norm = norm(f.read())
    teksty = {}
    tp = os.path.join(katalog_dir, "teksty.csv")
    for t in czytaj_srednikowy(tp, ["id_tekstu", "rola", "limit", "tekst"], "teksty.csv"):
        if t["id_tekstu"] in teksty:
            raise BladZestawu("teksty.csv, wiersz %d: powtórzony id_tekstu %s" % (t["_nr"], t["id_tekstu"]))
        lim = t["limit"].strip()
        if not lim:
            if t["rola"] not in LIMITY_ROLI:
                raise BladZestawu("teksty.csv, wiersz %d: nieznana rola %s bez limitu" % (t["_nr"], t["rola"]))
            lim = str(LIMITY_ROLI[t["rola"]])
        if not lim.isdigit():
            raise BladZestawu("teksty.csv, wiersz %d: limit `%s` nie jest liczbą" % (t["_nr"], lim))
        if "\t" in t["tekst"]:
            raise BladZestawu("teksty.csv, wiersz %d: tabulator w tekście" % t["_nr"])
        t["limit"] = lim
        teksty[t["id_tekstu"]] = t
    # każdy tekst pliku (także niewskazany przez katalog, np. teksty firmware U-01…U-05) przechodzi
    # kontrolę spec, granicy i limitu
    for idt, t in teksty.items():
        g = "teksty.csv, wiersz %d (%s)" % (t["_nr"], idt)
        z_progami = podstaw_progi(t["tekst"], progi, g)
        sprawdz_tekst_w_spec(z_progami, spec_norm, g)
        sprawdz_granice(z_progami, t["rola"], g)
        for slowo in SLOWA_GRANICY:
            if re.search(r"(?<![A-Za-z0-9])" + re.escape(slowo), t["tekst"]) and ostrzezenia is not None:
                ostrzezenia.append("%s: słowo granicy ADR-0002 `%s`: tekst do przeglądu ręcznego (9.3)" % (g, slowo))
        dlugi = re.sub(r"\{([^}]+)\}", lambda m: "x" * SZEROKOSC_ZMIENNYCH.get(m.group(1), SZEROKOSC_DOMYSLNA),
                       z_progami)
        if len(dlugi) > int(t["limit"]):
            raise BladZestawu("%s: tekst ma %d znaków po wstawieniu wartości, limit %s" % (g, len(dlugi), t["limit"]))
    wiersze = []
    for nazwa in ("E.csv", "L.csv", "H.csv", "O.csv"):  # O.csv: odkrycia (plik opcjonalny)
        p = os.path.join(katalog_dir, nazwa)
        if os.path.exists(p):
            wiersze += czytaj_srednikowy(p, POLA_ZRODLA, nazwa)
    klucze = set()
    for w in wiersze:
        gdzie = "%s, wiersz %d (%s)" % (w["_plik"], w["_nr"], klucz(w))
        if klucz(w) in klucze:
            raise BladZestawu("%s: powtórzony wiersz reguły i wariantu" % gdzie)
        klucze.add(klucz(w))
        if w["rodzaj"] not in znane:
            raise BladZestawu("%s: nieznany rodzaj warunku `%s` (spoza tabeli 9.3 i docs/spec/katalog/rodzaje-*.csv)"
                              % (gdzie, w["rodzaj"]))
        w["_param"] = parametry(w["parametry"], gdzie)
        progi_reguly = set(x for x in progi if glowny(progi, x)["regula"] == w["id_reguly"])
        for k, v in w["_param"].items():
            if k not in wszystkie_parametry:
                raise BladZestawu("%s: parametr `%s` spoza kontraktu 6 i rodzaje-*.csv (zadeklaruj go w pliku)"
                                  % (gdzie, k))
            dozw = rodzaje_paczek.get(w["rodzaj"])
            if dozw is not None and k not in dozw:
                raise BladZestawu("%s: parametr `%s` spoza listy rodzaju %s w rodzaje-*.csv" % (gdzie, k, w["rodzaj"]))
            if wszystkie_parametry[k]:
                if v not in progi:
                    raise BladZestawu("%s: parametr %s=%s: brak progu w progi.csv" % (gdzie, k, v))
                progi_reguly.add(v)
                if k == "kzn":
                    for pw in progi[v]:
                        m = re.fullmatch(r"(\d+)/(\d+)", pw["_wartosc"].strip())
                        if not m or not 1 <= int(m.group(1)) <= int(m.group(2)):
                            raise BladZestawu("%s: kzn=%s: wartość `%s` nie jest k/n z 1 ≤ k ≤ n"
                                              % (gdzie, v, pw["_wartosc"]))
            elif liczba(v) is not None:
                raise BladZestawu("%s: parametr %s=%s: liczba w katalogu (liczby tylko w progi.csv)" % (gdzie, k, v))
        if w["okno"]:
            if w["okno"] not in progi:
                raise BladZestawu("%s: okno %s: brak progu w progi.csv" % (gdzie, w["okno"]))
            progi_reguly.add(w["okno"])
        if w["liczone_noce"] not in LICZONE_NOCE:
            raise BladZestawu("%s: liczone_noce `%s` spoza %s" % (gdzie, w["liczone_noce"], LICZONE_NOCE[1:]))
        if w["stan"] not in STANY:
            raise BladZestawu("%s: stan `%s` spoza %s" % (gdzie, w["stan"], STANY))
        if w["pilnosc"] not in PILNOSCI:
            raise BladZestawu("%s: pilnosc `%s` spoza A, B, C" % (gdzie, w["pilnosc"]))
        for pole in ("wymaga_normy", "w_rozruchu"):
            if w[pole] not in TAK_NIE:
                raise BladZestawu("%s: %s `%s` (dozwolone: tak, nie)" % (gdzie, pole, w[pole]))
        wartosci = [(x, baza_jednostki(r["jednostka"])) for idp in progi_reguly for r in progi[idp]
                    for x in [liczba(r["_wartosc"])] if x is not None]
        for idt in lista(w["teksty"]):
            if idt not in teksty:
                raise BladZestawu("%s: tekst %s: brak w teksty.csv" % (gdzie, idt))
            t = teksty[idt]
            g = "%s, tekst %s" % (gdzie, idt)
            bez = re.sub(r"\{[^}]*\}", " ", t["tekst"])
            for m in re.finditer(LICZBA_W_TEKSCIE, bez):
                x = liczba(m.group(0))
                jt = jednostka_po(bez, m.end())
                if any(abs(x - y) < 1e-9 and (jt is None or jt == jy) for y, jy in wartosci):
                    raise BladZestawu("%s: liczba %s równa wartości progu reguły; wpisz ją przez {prog:…}"
                                      % (g, m.group(0)))
            if w["rodzaj"] in ZMIENNE_RODZAJU:
                for zm in re.findall(r"\{([^}:]+)\}", t["tekst"]):
                    if zm not in ZMIENNE_RODZAJU[w["rodzaj"]]:
                        raise BladZestawu("%s: zmienna {%s} nie jest wypełniana przez rodzaj %s"
                                          % (g, zm, w["rodzaj"]))
    for w in wiersze:
        for z in zaleznosci(w):
            if z not in klucze and z.split("/")[0] not in set(x["id_reguly"] for x in wiersze):
                raise BladZestawu("%s, wiersz %d (%s): zależność %s: brak takiej reguły w katalogu"
                                  % (w["_plik"], w["_nr"], klucz(w), z))
        # wykluczenie reguły, której jeszcze nie ma w katalogu (np. E-31 przed zdarzeniami E), nie jest błędem
    sprawdz_odwolania(wiersze, katalog_dir)
    wiersze = uloz(wiersze)

    def czyste(v):
        return v.replace(";", ",").replace("\n", " ").replace("\r", " ")

    progi_body = ["id_progu;wartosc;jednostka;tryb"]
    def wartosc_panelu(v):
        # lista z progi.csv (`7;30;100`) → `7,30,100`; lista dwóch liczb dostaje przecinek na końcu (`7,30,`), żeby
        # panel nie wziął jej za liczbę z przecinkiem dziesiętnym (7,30)
        if ";" in v:
            el = [x.strip() for x in v.split(";")]
            t = ",".join(el)
            return t + "," if len(el) == 2 and all(liczba(x) is not None for x in el) else czyste(t)
        return czyste(v)

    for k, lst in progi.items():
        for w in lst:
            progi_body.append(";".join([k, wartosc_panelu(w["_wartosc"]), czyste(w["jednostka"]), czyste(w["tryb"])]))
    kat_body = [";".join(POLA_KATALOGU + POLA_BETY)]
    for w in wiersze:
        kat_body.append(";".join(w[p] for p in POLA_KATALOGU + POLA_BETY))
    tek_body = ["id_tekstu;rola;limit;tekst"]
    for k in sorted(teksty):
        t = teksty[k]
        tek_body.append(";".join([k, t["rola"], t["limit"], t["tekst"]]))
    return ["\n".join(b) + "\n" for b in (progi_body, kat_body, tek_body)]


def zrodlo_git():
    try:
        r = subprocess.run(["git", "rev-parse", "--short=12", "HEAD"], cwd=REPO, capture_output=True, text=True,
                           timeout=10)
        return r.stdout.strip() if r.returncode == 0 and r.stdout.strip() else "brak"
    except (OSError, subprocess.SubprocessError):
        return "brak"


def zrodlo_z_naglowka(sciezka_h, zestaw):
    """Pole `zrodlo` z istniejącego zestaw_wbudowany.h, gdy treść zestawu się nie zmieniła (ten sam identyfikator).
    Dzięki temu ponowne uruchomienie bez zmiany źródeł daje plik identyczny bajt w bajt, a `zrodlo` wskazuje
    commit, na którym treść zestawu zmieniła się ostatnio. None, gdy pliku nie ma albo treść jest inna."""
    try:
        with open(sciezka_h, encoding="utf-8") as f:
            pierwsza = next((l for l in f if "#cpap-eink " in l), "")
    except OSError:
        return None
    m = re.search(r"zestaw=(\S+) .*zrodlo=(\S+)", pierwsza)
    return m.group(2) if m and m.group(1) == zestaw else None


def z_naglowkami(cialа, zestaw, pochodzenie, fw_od, fw_do, zamrozenie, zrodlo):
    wynik = []
    for rodzaj, c in zip(("progi", "katalog", "teksty"), cialа):
        skrot = hashlib.sha256(c.encode("utf-8")).hexdigest()
        wynik.append("#cpap-eink %s 1 zestaw=%s pochodzenie=%s firmware=%s..%s tryby=%s zamrozenie=%s zrodlo=%s "
                     "skrot=sha256:%s\n%s" % (rodzaj, zestaw, pochodzenie, fw_od, fw_do, TRYBY, zamrozenie, zrodlo,
                                              skrot, c))
    return wynik


def naglowek_cpp(pliki):
    nazwy = ("PROGI", "KATALOG", "TEKSTY")
    # REUSE-IgnoreStart (znacznik SPDX jako tekst w kodzie, nie licencja tego pliku)
    out = ["// SPDX-License-Identifier: GPL-3.0-or-later",
           "// WYGENEROWANE przez tools/zestaw/buduj_zestaw.py ze spec funkcjonalnej, docs/spec/progi.csv",
           "// i docs/spec/katalog/ (spec techniczna 9.3, ADR-0023). Nie edytuj ręcznie: zmień źródła i uruchom skrypt.",
           "// Zestaw wbudowany w firmware; parsuje go reguly::zestawWbudowany() przy pierwszym wywołaniu.",
           "#pragma once", "", "namespace reguly {", "namespace wbudowany {"]
    # REUSE-IgnoreEnd
    for n, p in zip(nazwy, pliki):
        if ")zestaw\"" in p:
            raise BladZestawu("tekst zawiera ogranicznik `)zestaw\"`")
        out.append('static const char %s[] = R"zestaw(%s)zestaw";' % (n, p))
    out += ["}  // namespace wbudowany", "}  // namespace reguly", ""]
    return "\n".join(out)


def main(argv=None):
    a = argparse.ArgumentParser(description="Budowa zestawu danych reguł (spec techniczna 9.3)")
    a.add_argument("--progi", default=os.path.join(REPO, "docs", "spec", "progi.csv"))
    a.add_argument("--katalog", default=os.path.join(REPO, "docs", "spec", "katalog"))
    a.add_argument("--spec", default=os.path.join(REPO, "docs", "spec", "specyfikacja-funkcjonalna.md"))
    a.add_argument("--h", default=os.path.join(REPO, "firmware", "prototyp", "src", "zestaw_wbudowany.h"))
    a.add_argument("--bez-h", action="store_true", help="bez zapisu zestaw_wbudowany.h (sama kontrola)")
    a.add_argument("--wyjscie", default="", help="katalog na progi.csv, katalog.csv, teksty.csv")
    a.add_argument("--pochodzenie", default="wbudowany", choices=["wbudowany", "karta"])
    a.add_argument("--firmware", default="1.0.0")
    a.add_argument("--firmware-do", default="")
    a.add_argument("--zamrozenie", default="brak")
    a.add_argument("--zrodlo", default="")
    a.add_argument("--zestaw", default="", help="identyfikator zestawu (domyślnie ze skrótu treści)")
    a.add_argument("--rodzaj", action="append", default=[], help="dodatkowy znany rodzaj warunku")
    o = a.parse_args(argv)
    try:
        ostrz = []
        cial = buduj(o.progi, o.katalog, o.spec, o.rodzaj, ostrz)
        for x in sorted(set(ostrz)):
            print("OSTRZEŻENIE: %s" % x, file=sys.stderr)
        if not re.fullmatch(r"\d+\.\d+\.\d+", o.firmware):
            raise BladZestawu("--firmware `%s`: wersja w postaci MAJOR.MINOR.PATCH" % o.firmware)
        fw_do = o.firmware_do or (o.firmware.split(".")[0] + ".99.99")
        zestaw = o.zestaw or ("z-" + hashlib.sha256("".join(cial).encode("utf-8")).hexdigest()[:12])
        if not re.fullmatch(r"[A-Za-z0-9._-]+", zestaw):
            raise BladZestawu("--zestaw `%s`: tylko litery, cyfry i . _ -" % zestaw)
        zrodlo = o.zrodlo or (not o.bez_h and zrodlo_z_naglowka(o.h, zestaw)) or zrodlo_git()
        pliki = z_naglowkami(cial, zestaw, o.pochodzenie, o.firmware, fw_do, o.zamrozenie, zrodlo)
    except BladZestawu as e:
        print("BŁĄD zestawu reguł: %s" % e, file=sys.stderr)
        return 1
    if o.wyjscie:
        os.makedirs(o.wyjscie, exist_ok=True)
        for n, p in zip(("progi.csv", "katalog.csv", "teksty.csv"), pliki):
            with open(os.path.join(o.wyjscie, n), "w", encoding="utf-8", newline="") as f:
                f.write(p)
    if not o.bez_h:
        with open(o.h, "w", encoding="utf-8", newline="") as f:
            f.write(naglowek_cpp(pliki))
    print("zestaw %s: %d progów, %d wierszy katalogu, %d tekstów" % (
        zestaw, pliki[0].count("\n") - 2, pliki[1].count("\n") - 2, pliki[2].count("\n") - 2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
