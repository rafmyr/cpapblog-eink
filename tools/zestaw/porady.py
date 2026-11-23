#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Porady programu „Dobry start” w zestawie danych (ADR-0049, ADR-0023, ADR-0024, R-DS-36…R-DS-41, R-DS-52).

Wejście: docs/spec/katalog/porady.csv i porady-ankieta.csv (separator `;`), kroje panelu z
firmware/prototyp/include/fonty.h. Wyjście: dwa ciała plików w formacie panelu (bez wiersza nagłówka
`#cpap-eink …`, który dokłada buduj_zestaw.py): porady bez kolumn źródeł (`ksiazka`, `blog`, `zalecenia`, `poziom`,
`werdykt`, `wyzwalacz` i `tekst_zmieniony_po_recenzji` nie idą do firmware) i pytania ankiety.

Kontrole (błąd = BladPorad z ID porady):
  - unikalne ID, znany `rodzaj`, `tryb`, `wymaga`, `ekran`, `dzien` liczbą przy planie;
  - każde miejsce planu 1…N obsadzone dla każdego trybu (CPAP, AutoSet, ASV) i obu stanów nawilżacza, najwyżej jedna
    porada na miejsce, tryb i stan nawilżacza; porada z flagą `koncowa` na ostatnim miejscu;
  - odwołania (`zamiast`, `przy`, `wymaga_porady`, `stopien_wyzej`) do istniejących ID;
  - długość każdego tekstu krojami panelu (R-DS-39, spec 5.8.14.3.3, ikony przycisków jak szerIkony()
    w src/ekrany.cpp, R-DS-52): tytuł 1 linia SB44 ≤ 748 px; treść ≤ 5 linii P24S w 748 px, na karcie ≤ 4 linie P22B
    w 490 px; krok 1 linia (P24B w 748 px, na karcie P22B w 490 px);
  - znaczniki `{prog:id}` (R-DS-53): próg musi istnieć w progi.csv; długość liczona z najdłuższą wartością;
  - test ADR-0002 z wyjątkiem ADR-0050 (R-DS-41): tools/zestaw/adr2.py (samotest i wzorce) i adr2_wyjatki.csv obok;
    trafienie spoza wyjątków i wyjątek nieużywany = błąd.

Twarda spacja (U+00A0) po jednoliterowych spójnikach i przyimkach oraz między liczbą a jednostką (twarde()) trafia do
tekstu panelu, tak jak w renderze z 21.11.2026: zawijanie na panelu (zawin()) jej nie łamie.
"""

import csv
import io
import os
import re
import sys

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(TU))
FONTY = os.path.join(REPO, "firmware", "prototyp", "include", "fonty.h")


class BladPorad(Exception):
    pass


# ---------- kroje panelu (te same tablice co src/ekrany.cpp: szerokości w 1/16 px, glif `H` jako wersalik) ----------
class Kroj:
    def __init__(self, nazwa, gl, adv16, extra):
        self.nazwa, self.gl, self.adv16, self.extra = nazwa, gl, adv16, extra

    def wersalik(self):
        g = self.gl[ord("H") - 0x20]
        return g[2] if g[2] else 12

    def szer_ikony(self):
        # ikona przycisku (◀ ▶ ↻) w zwykłym tekście: bok = wersalik + 6, po nim odstęp 6 px (StylIkony w ekrany.cpp)
        return self.wersalik() + 6 + 6

    def szer(self, tekst):
        w16 = 0
        for ch in tekst:
            o = ord(ch)
            if o in (0x25C0, 0x25B6, 0x21BB):
                w16 += self.szer_ikony() * 16
                continue
            if ch == "\u00a0":
                o = 0x20
            elif not 0x20 <= o < 0x7F:
                i = self.extra.find(ch)
                if i < 0:
                    raise BladPorad("znak spoza krojów panelu: %r (U+%04X)" % (ch, o))
                o = 0x7F + i
            w16 += self.adv16[o - 0x20]
        return (w16 + 8) >> 4


def wczytaj_kroje(sciezka=FONTY):
    t = open(sciezka, encoding="utf-8").read()
    m = re.search(r'FONT_EXTRA\[\] = "([^"]*)"', t)
    extra = m.group(1) if m else ""
    kroje = {}
    for m in re.finditer(r"const GFXfont (\w+) PROGMEM = \{", t):
        n = m.group(1)
        gl = re.search(r"const GFXglyph %s_gl\[\] PROGMEM = \{(.*?)\};" % n, t, re.S)
        adv = re.search(r"const uint16_t %s_adv16\[\] PROGMEM = \{(.*?)\};" % n, t, re.S)
        if not gl or not adv:
            continue
        g = [tuple(int(v) for v in x.split(",")) for x in re.findall(r"\{\s*([-\d,\s]+?)\s*\}", gl.group(1))]
        kroje[n] = Kroj(n, g, [int(v) for v in re.findall(r"\d+", adv.group(1))], extra)
    return kroje


def zawin(kroj, tekst, maks):
    """Jak zawin() w src/ekrany.cpp: słowa po zwykłej spacji, pierwsze słowo linii zawsze w niej zostaje."""
    linie, akt = [], ""
    for slowo in tekst.split(" "):
        if not slowo:
            continue
        proba = slowo if not akt else akt + " " + slowo
        if kroj.szer(proba) > maks and akt:
            linie.append(akt)
            akt = slowo
        else:
            akt = proba
    if akt:
        linie.append(akt)
    return linie


SIEROTY = re.compile(r"(?<![^\s„(])([aiouwzAIOUWZ]) ")


def twarde(tekst):
    t = SIEROTY.sub(lambda m: m.group(1) + "\u00a0", tekst)
    return re.sub(r"(\d) (h|min|L/min|tyg\.|dni|nocy|noce|razy|%|/ h)", "\\1\u00a0\\2", t)


# ---------- geometria S-K-48 (spec 5.8.14: 3.3 wersji 1, rozdz. 7 i 8 wersji 2) ----------
KOLUMNA, KOLUMNA_KARTY = 748, 490
KOLUMNA_QR = 513          # treść i krok obok kodu QR w wersji 3 (R-DS-62)
LINIE_TRESCI, LINIE_TRESCI_KARTY, LINIE_TRESCI_FAKTY = 5, 4, 2
QR_MAKS_BAJTOW = 42       # wersja 3, poziom M (R-DS-62)
ODSTEP_FAKTOW = 24        # odstęp między parami rzędu faktów (rozdz. 7)
BELKA_NAZWA = "Dobry start"

# Znaczniki danych nocy (R-DS-60): najdłuższa wartość do testu długości (spec 5.8.14, tabela 4.3).
ZNACZNIKI = {"czas": "23 h 59", "o_ile_dluzej": "23 h 59", "noce_z_rzedu": "999", "wyciek": "99", "noce_z_maska": "7",
             "noce_4h": "7", "srednio": "23 h 59", "noce_z_maska_prog": "999", "srednio_prog": "23 h 59"}


# Tokeny warunku liczone przez panel (dobry_start::tokenyPochwal(), tokenyRegulB49(), tokenyFazy()): literówka w tokenie
# z tych rodzin = porada, która nigdy nie stanie. Inne rodziny (E-, L-, S-K-, DS-Q…) sprawdza katalog reguł.
TOKENY_POCHWAL = {"pierwsza_noc", "pierwsze_4h", "cala_noc", "dluzej", "szczelna", "szczelniej", "tydzien_co_noc",
                  "powrot"}
TOKEN_RODZINY = re.compile(r"(pochwala|M-20|ekran|faza)/")


def sprawdz_token(tok):
    """None albo opis błędu tokenu warunku z rodzin `pochwala/`, `M-20/`, `ekran/`, `faza/` (sufiks `@start` dozwolony)."""
    t = tok.lstrip("!")
    if t.endswith("@start"):
        t = t[:-len("@start")]
    m = TOKEN_RODZINY.match(t)
    if not m:
        return None
    reszta = t[m.end():]
    ok = {"pochwala": reszta in TOKENY_POCHWAL, "M-20": reszta.isdigit(),
          "ekran": re.fullmatch(r"problem|S[0-9A-Z]{1,3}|SK[0-9]{2}", reszta) is not None,
          "faza": reszta == "start"}[m.group(1)]
    return None if ok else "nieznany token warunku %s" % tok


def sprawdz_nawiasy(tekst):
    """None albo opis błędu: `{…}` poza wzorcami `{prog:id}` i `{nazwa}` (panel zostawiłby go na ekranie)."""
    t = re.sub(r"\{prog:[^{}]+\}", "", tekst)
    t = re.sub(r"\{[a-z_0-9]+\}", "", t)  # nazwę spoza ZNACZNIKI zgłasza podstaw_znaczniki()
    if "{" in t or "}" in t:
        return "znacznik spoza wzorca albo nieznany: %s" % (re.search(r"\{[^}]*\}?|\}", t).group(0))
    return None


def podstaw_znaczniki(tekst):
    """Znaczniki `{nazwa}` danych nocy z najdłuższą wartością; nieznany znacznik = BladPorad."""
    def jedna(m):
        if m.group(1) not in ZNACZNIKI:
            raise BladPorad("nieznany znacznik danych {%s}" % m.group(1))
        return ZNACZNIKI[m.group(1)]
    return re.sub(r"\{([a-z_0-9]+)\}", jedna, tekst)


def pary_faktow(fakty):
    """`liczba::podpis | liczba::podpis` → lista par (R-DS-64)."""
    pary = []
    for x in filter(None, (y.strip() for y in fakty.split(" | "))):
        if x.count("::") != 1:
            raise BladPorad("fakty: para bez `::`: %s" % x)
        pary.append(tuple(x.split("::")))
    return pary


def sprawdz_dlugosc(p, kroje):
    """Lista błędów długości jednej porady (pusta = mieści się)."""
    karta = p["ekran"] == "karta"
    qr = bool(p.get("qr")) and not karta
    b = []
    if kroje["SB44"].szer(p["tytul"]) > KOLUMNA:
        b.append("tytuł %d px > %d" % (kroje["SB44"].szer(p["tytul"]), KOLUMNA))
    if karta:
        f, kol, lim = kroje["P22B"], KOLUMNA_KARTY, LINIE_TRESCI_FAKTY if p.get("fakty") else LINIE_TRESCI_KARTY
    else:
        f, kol, lim = kroje["P24S"], KOLUMNA_QR if qr else KOLUMNA, LINIE_TRESCI
    if p.get("fakty"):
        pary = pary_faktow(p["fakty"])
        if len(pary) > 3:
            b.append("fakty: %d par > 3" % len(pary))
        suma = sum(max(kroje["P28B"].szer(l), kroje["P22B"].szer(t)) for l, t in pary) + ODSTEP_FAKTOW * (len(pary) - 1)
        if suma > KOLUMNA_KARTY:
            b.append("rząd faktów %d px > %d" % (suma, KOLUMNA_KARTY))
    for slowo in p["tresc"].replace("\u00a0", " ").split(" "):
        if slowo and f.szer(slowo) > kol:
            b.append("słowo szersze niż kolumna: %s" % slowo)
    n = len(zawin(f, p["tresc"], kol))
    if n > lim:
        b.append("treść %d linii > %d" % (n, lim))
    if p["krok"]:
        fk = kroje["P22B"] if karta else kroje["P24B"]
        if fk.szer(p["krok"]) > kol:
            b.append("krok %d px > %d" % (fk.szer(p["krok"]), kol))
    return b


# ---------- test ADR-0002 z wyjątkiem ADR-0050 (R-DS-41) ----------
# Jedno źródło wzorców i wyjątków: tools/zestaw/adr2.py i adr2_wyjatki.csv (samotest na przykładach przed testem
# treści); narzędzia materiałów źródłowych porad korzystają z tych samych plików.


def adr2():
    if TU not in sys.path:
        sys.path.insert(0, TU)
    import adr2 as a  # noqa: E402
    return a


def wyjatki_adr2():
    with open(os.path.join(TU, "adr2_wyjatki.csv"), encoding="utf-8") as f:
        return {(w["id"], w["regula"], w["trafienie"]): w["uzasadnienie"] for w in csv.DictReader(f, delimiter=";")}


def baza(i):
    return i.split(".", 1)[0]


# ---------- znaczniki progów w treści (R-DS-53) ----------
# Panel wstawia bieżącą wartość progu z zestawu; `S-K-10.uwaga_o_masce` to ustawienie „Uwaga o masce od” (R-KONF-42),
# więc długość liczona z górną granicą jego zakresu.
NAJDLUZSZE_PROGI = {"S-K-10.uwaga_o_masce": "R-KONF-42.zakres_max"}


def podstaw_progi(tekst, progi, najdluzsze=False):
    def jedna(m):
        idp = m.group(1)
        if najdluzsze and idp in NAJDLUZSZE_PROGI:
            idp = NAJDLUZSZE_PROGI[idp]
        if idp not in progi:
            raise BladPorad("znacznik {prog:%s}: nie ma takiego progu" % m.group(1))
        return progi[idp]
    return re.sub(r"\{prog:([^}]+)\}", jedna, tekst)


def wczytaj_progi(sciezka=os.path.join(REPO, "docs", "spec", "progi.csv")):
    with open(sciezka, encoding="utf-8") as f:
        wart = {w["id_progu"]: w["wartosc"] for w in csv.DictReader(f)}
    for k in list(wart):
        v, n = wart[k], 0
        while v.startswith("=") and n < 10:
            v, n = wart.get(v[1:], ""), n + 1
        wart[k] = v
    return wart


# ---------- pliki ----------
RODZAJE = ("plan", "zdarzenie", "ankieta", "pochwala")
FAZY_PLANU = ("start_rano", "start_wieczor", "adaptacja", "utrwalenie")  # kolumna `faza` porad planu (R-DS-54)
FAZA_RODZAJU = {"zdarzenie": "zdarzenie", "ankieta": "ankieta", "pochwala": "pochwala"}
PORY = {"start_rano": "rano", "start_wieczor": "wieczor", "pochwala": "rano"}  # kolumna `pora` (R-DS-55)
TRYBY_KOL = ("CPAP+AutoSet", "CPAP", "AutoSet", "ASV", "wszystkie")
TRYBY_APARATU = {"CPAP": ("CPAP+AutoSet", "CPAP", "wszystkie"), "AutoSet": ("CPAP+AutoSet", "AutoSet", "wszystkie"),
                 "ASV": ("ASV", "wszystkie")}
WYMAGA = ("", "nawilżacz", "bez nawilżacza")
EKRANY = ("dane", "karta")
FLAGI = ("przerwa", "poza_limitem", "bez_limitu", "wyciek", "bez_wycieku_7", "pierwsza_odpowiedz", "dwa_razy",
         "podstawowa", "koncowa", "objaw", "pochwala", "raz", "koncowa_fazy")
# kolumny pliku porad w zestawie panelu: tylko czytane przez firmware (`przy`, `miejsce_bez` i `pora` zostają w
# porady.csv do kontroli, panel ich nie czyta; obraz mniejszy)
POLA_PANELU = ["id", "rodzaj", "dzien", "tryb", "wymaga", "ekran", "zamiast", "warunek", "kol", "kol_ankiety",
               "flagi", "wymaga_porady", "stopien_wyzej", "tytul", "tresc", "krok", "faza", "qr", "fakty"]
POLA_ANKIETY = ["id", "typ", "od_tygodnia", "wylaczna", "klucz", "wartosci", "pytanie", "odpowiedzi"]


def czytaj(sciezka):
    with open(sciezka, encoding="utf-8", newline="") as f:
        t = f.read()
    if t.startswith("\ufeff"):
        raise BladPorad("%s: znacznik BOM" % os.path.basename(sciezka))
    return list(csv.DictReader(io.StringIO(t), delimiter=";"))


def sprawdz(porady, ankieta, kroje, progi):
    ids = set()
    for p in porady:
        i = p["id"]
        g = "porady.csv %s" % i
        if not re.fullmatch(r"DS-[A-Z]\d{2}(\.[a-z]+)?", i or ""):
            raise BladPorad("%s: złe ID" % g)
        if i in ids:
            raise BladPorad("%s: powtórzone ID" % g)
        ids.add(i)
        if p["rodzaj"] not in RODZAJE:
            raise BladPorad("%s: nieznany rodzaj %s" % (g, p["rodzaj"]))
        if p["tryb"] not in TRYBY_KOL:
            raise BladPorad("%s: nieznany tryb %s" % (g, p["tryb"]))
        if p["wymaga"] not in WYMAGA:
            raise BladPorad("%s: nieznane `wymaga` %s" % (g, p["wymaga"]))
        if p["ekran"] not in EKRANY:
            raise BladPorad("%s: nieznany ekran %s" % (g, p["ekran"]))
        if p["rodzaj"] == "plan" and not p["dzien"].isdigit():
            raise BladPorad("%s: porada planu bez doby nominalnej" % g)
        faza = p.get("faza", "")
        if (p["rodzaj"] == "plan" and faza not in FAZY_PLANU) or (p["rodzaj"] != "plan" and faza != FAZA_RODZAJU[p["rodzaj"]]):
            raise BladPorad("%s: zła faza %s" % (g, faza))
        if p.get("pora", "") != PORY.get(faza, ""):
            raise BladPorad("%s: zła pora %s dla fazy %s" % (g, p.get("pora", ""), faza))
        if p["rodzaj"] == "pochwala" and ("pochwala" not in p.get("flagi", "") or not p.get("warunek")):
            raise BladPorad("%s: pochwała bez flagi `pochwala` albo bez warunku" % g)
        if p.get("qr"):
            if p["ekran"] == "karta":
                raise BladPorad("%s: kod QR na karcie z budzikiem" % g)
            if not re.fullmatch(r"https?://[!-~]+", p["qr"]) or len(p["qr"].encode()) > QR_MAKS_BAJTOW:
                raise BladPorad("%s: adres QR nie ASCII albo dłuższy niż %d B" % (g, QR_MAKS_BAJTOW))
        if p.get("fakty") and p["ekran"] != "karta":
            raise BladPorad("%s: fakty poza kartą" % g)
        tekst = " ".join(p.get(k, "") for k in ("tytul", "tresc", "krok", "fakty"))
        if "AHI" in tekst and (p["ekran"] == "karta" or p["rodzaj"] == "pochwala"):
            raise BladPorad("%s: AHI na karcie albo w pochwale (R-DS-61)" % g)
        for pole in ("tytul", "tresc", "krok", "fakty"):
            b = sprawdz_nawiasy(p.get(pole, ""))
            if b:
                raise BladPorad("%s: pole %s: %s" % (g, pole, b))
        for tok in filter(None, p.get("warunek", "").split("|")):
            b = sprawdz_token(tok)
            if b:
                raise BladPorad("%s: %s" % (g, b))
        for f in filter(None, p.get("flagi", "").split(",")):
            if f not in FLAGI:
                raise BladPorad("%s: nieznana flaga %s" % (g, f))
        for k in ("kol", "kol_ankiety"):
            if p.get(k) and not p[k].isdigit():
                raise BladPorad("%s: %s nie jest liczbą" % (g, k))
        for pole in ("tytul", "tresc"):
            if not p[pole].strip():
                raise BladPorad("%s: brak pola %s" % (g, pole))
        for pole in POLA_PANELU:
            if ";" in p.get(pole, "") or "\n" in p.get(pole, ""):
                raise BladPorad("%s: średnik albo koniec wiersza w polu %s" % (g, pole))
        if p["rodzaj"] != "plan" and "podstawowa" in p.get("flagi", ""):
            raise BladPorad("%s: flaga `podstawowa` poza planem" % g)
    for p in porady:
        for pole in ("zamiast", "przy", "wymaga_porady", "stopien_wyzej"):
            if p.get(pole) and p[pole] not in ids:
                raise BladPorad("porady.csv %s: %s %s: nie ma takiej porady" % (p["id"], pole, p[pole]))
    plan = [p for p in porady if p["rodzaj"] == "plan"]
    for p in porady:
        if p.get("miejsce_bez") and (not p["miejsce_bez"].isdigit() or p["rodzaj"] == "plan"):
            raise BladPorad("porady.csv %s: miejsce_bez tylko liczbą i poza planem" % p["id"])
    if plan:
        sprawdz_fazy(porady, plan)
    bledy = []
    for p in porady:
        try:
            q = dict(p)
            for pole in ("tytul", "tresc", "krok", "fakty"):
                q[pole] = podstaw_znaczniki(podstaw_progi(p.get(pole, ""), progi, najdluzsze=True))
            for b in sprawdz_dlugosc(q, kroje):
                bledy.append("%s: %s" % (p["id"], b))
        except BladPorad as e:
            bledy.append("%s: %s" % (p["id"], e))
    a = adr2()
    for b in a.samotest():
        bledy.append("test ADR-0002, samotest: %s" % b)
    wyj, uzyte = wyjatki_adr2(), set()
    for p in porady:
        for pole in ("tytul", "tresc", "krok"):
            try:
                traf = a.trafienia(p.get("_" + pole, p[pole]), lambda t: podstaw_znaczniki(podstaw_progi(t, progi)))
            except (KeyError, BladPorad):  # nieznany znacznik progu: błąd zgłoszony wyżej
                continue
            for nazwa, frag in traf:
                k = (p["id"], nazwa, frag)
                if k in wyj:
                    uzyte.add(k)
                else:
                    bledy.append("%s: test ADR-0002, %s: «%s»" % (p["id"], nazwa, frag))
    ids_por = {p["id"] for p in porady}
    for k in sorted(set(wyj) - uzyte):
        if k[0] in ids_por:  # wyjątek porady, której tekst się zmienił: lista ma być aktualna
            bledy.append("%s: test ADR-0002, wyjątek nieużywany: %s «%s»" % (k[0], k[1], k[2]))
    if bledy:
        raise BladPorad("; ".join(bledy))
    qids = set()
    for q in ankieta:
        if q["id"] in qids or not q["id"]:
            raise BladPorad("porady-ankieta.csv %s: puste albo powtórzone ID" % q["id"])
        qids.add(q["id"])
        if q["typ"] not in ("jedna", "wiele"):
            raise BladPorad("porady-ankieta.csv %s: nieznany typ %s" % (q["id"], q["typ"]))
        if not q.get("od_tygodnia", "").isdigit():
            raise BladPorad("porady-ankieta.csv %s: od_tygodnia nie jest liczbą" % q["id"])
        n = len(q["odpowiedzi"].split(" | "))
        if q.get("wylaczna") and not (q["wylaczna"].isdigit() and 1 <= int(q["wylaczna"]) <= n):
            raise BladPorad("porady-ankieta.csv %s: zła odpowiedź wyłączna" % q["id"])
        if q.get("wartosci") and len(q["wartosci"].split("|")) != n:
            raise BladPorad("porady-ankieta.csv %s: liczba wartości klucza różna od liczby odpowiedzi" % q["id"])
    for p in porady:
        for tok in filter(None, p.get("warunek", "").split("|")):
            m = re.fullmatch(r"!?(DS-Q\d{2})=(\d+)", tok)
            if m and m.group(1) not in qids:
                raise BladPorad("porady.csv %s: warunek %s: nie ma takiego pytania" % (p["id"], tok))


def pasuje(p, kol, nawilzacz):
    return p["tryb"] in kol and (p["wymaga"] == "" or (p["wymaga"] == "nawilżacz") == nawilzacz)


def sprawdz_fazy(porady, plan):
    """R-DS-71: każda faza planu ma dla każdego trybu i stanu nawilżacza dokładnie jedną poradę na każdą swoją dobę
    nominalną (doby fazy = doby jej porad w pliku); fazy po kolei (start, adaptacja, utrwalenie) bez nakładania; porada
    `koncowa` na ostatniej dobie programu, `koncowa_fazy` na ostatniej dobie swojej fazy; porady wyzwalane, z ankiety
    i pochwały mają wariant dla każdej osoby (bez wariantu tylko porada `wymaga=nawilżacz`)."""
    doby = {f: sorted({int(p["dzien"]) for p in plan if p["faza"] == f}) for f in FAZY_PLANU}
    for f in FAZY_PLANU:
        if not doby[f]:
            raise BladPorad("porady.csv: faza %s bez porad planu" % f)
    granice = [(min(doby["start_rano"] + doby["start_wieczor"]), max(doby["start_rano"] + doby["start_wieczor"])),
               (min(doby["adaptacja"]), max(doby["adaptacja"])), (min(doby["utrwalenie"]), max(doby["utrwalenie"]))]
    for (a0, a1), (b0, b1) in zip(granice, granice[1:]):
        if b0 <= a1:
            raise BladPorad("porady.csv: fazy planu nachodzą na siebie (doby %d…%d i %d…%d)" % (a0, a1, b0, b1))
    for tryb, kol in TRYBY_APARATU.items():
        for nawilzacz in (True, False):
            for f in FAZY_PLANU:
                for d in doby[f]:
                    m = [p for p in plan if p["faza"] == f and int(p["dzien"]) == d and pasuje(p, kol, nawilzacz)]
                    if len(m) != 1:
                        raise BladPorad("porady.csv: %s doba %d w trybie %s %s nawilżacza: %d porad (ma być 1)"
                                        % (f, d, tryb, "z" if nawilzacz else "bez", len(m)))
            for p in porady:
                if p["rodzaj"] == "plan" or p["wymaga"] == "nawilżacz":
                    continue
                if not [q for q in porady if baza(q["id"]) == baza(p["id"]) and pasuje(q, kol, nawilzacz)]:
                    raise BladPorad("porady.csv %s: brak wariantu w trybie %s %s nawilżacza"
                                    % (p["id"], tryb, "z" if nawilzacz else "bez"))
    maks = granice[-1][1]
    konc = [p for p in plan if "koncowa" in p.get("flagi", "").split(",")]
    if not konc or any(int(p["dzien"]) != maks for p in konc):
        raise BladPorad("porady.csv: porada z flagą `koncowa` musi stać na ostatniej dobie programu (%d)" % maks)
    for p in plan:
        if "koncowa_fazy" in p.get("flagi", "").split(",") and int(p["dzien"]) != max(doby[p["faza"]]):
            raise BladPorad("porady.csv %s: `koncowa_fazy` nie na ostatniej dobie fazy %s" % (p["id"], p["faza"]))


def buduj_porady(katalog_dir, kroje=None, progi=None):
    """Ciała plików `porady` i `ankieta` (bez nagłówka) albo None, gdy katalog nie ma porady.csv."""
    sp = os.path.join(katalog_dir, "porady.csv")
    sa = os.path.join(katalog_dir, "porady-ankieta.csv")
    if not os.path.exists(sp):
        return None
    porady = czytaj(sp)
    ankieta = czytaj(sa) if os.path.exists(sa) else []
    for p in porady:
        for pole in ("tytul", "tresc", "krok"):
            p["_" + pole] = p.get(pole, "").strip()  # tekst przed twardymi spacjami do testu ADR-0002
            p[pole] = twarde(p["_" + pole])
    sprawdz(porady, ankieta, kroje or wczytaj_kroje(), progi if progi is not None else wczytaj_progi())
    pb = [";".join(POLA_PANELU)] + [";".join(p.get(k, "") for k in POLA_PANELU) for p in porady]
    ab = [";".join(POLA_ANKIETY)] + [";".join(q.get(k, "") for k in POLA_ANKIETY) for q in ankieta]
    return ["\n".join(pb) + "\n", "\n".join(ab) + "\n"]
