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


# ---------- geometria S-K-48 (spec robocza dobry-start.md 3.3) ----------
KOLUMNA, KOLUMNA_KARTY = 748, 490
LINIE_TRESCI, LINIE_TRESCI_KARTY = 5, 4
BELKA_NAZWA = "Dobry start"


def sprawdz_dlugosc(p, kroje):
    """Lista błędów długości jednej porady (pusta = mieści się)."""
    karta = p["ekran"] == "karta"
    b = []
    if kroje["SB44"].szer(p["tytul"]) > KOLUMNA:
        b.append("tytuł %d px > %d" % (kroje["SB44"].szer(p["tytul"]), KOLUMNA))
    f, kol, lim = (kroje["P22B"], KOLUMNA_KARTY, LINIE_TRESCI_KARTY) if karta else (kroje["P24S"], KOLUMNA, LINIE_TRESCI)
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
RODZAJE = ("plan", "zdarzenie", "ankieta", "ponad_program")
TRYBY_KOL = ("CPAP+AutoSet", "CPAP", "AutoSet", "ASV", "wszystkie")
TRYBY_APARATU = {"CPAP": ("CPAP+AutoSet", "CPAP", "wszystkie"), "AutoSet": ("CPAP+AutoSet", "AutoSet", "wszystkie"),
                 "ASV": ("ASV", "wszystkie")}
WYMAGA = ("", "nawilżacz", "bez nawilżacza")
EKRANY = ("dane", "karta")
FLAGI = ("przerwa", "poza_limitem", "bez_limitu", "wyciek", "bez_wycieku_7", "pierwsza_odpowiedz", "dwa_razy",
         "podstawowa", "koncowa", "objaw")
POLA_PANELU = ["id", "rodzaj", "dzien", "tryb", "wymaga", "ekran", "przy", "zamiast", "warunek", "kol", "kol_ankiety",
               "flagi", "wymaga_porady", "stopien_wyzej", "miejsce_bez", "tytul", "tresc", "krok"]
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
            raise BladPorad("%s: porada planu bez miejsca w kolejce" % g)
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
        maks = max(int(p["dzien"]) for p in plan)
        for tryb, kol in TRYBY_APARATU.items():
            for nawilzacz in (True, False):
                for d in range(1, maks + 1):
                    m = [p for p in plan if int(p["dzien"]) == d and p["tryb"] in kol and
                         (p["wymaga"] == "" or (p["wymaga"] == "nawilżacz") == nawilzacz)]
                    if not m and not nawilzacz:  # R-DS-29: bez nawilżacza porada spoza planu z `miejsce_bez`
                        m = [p for p in porady if p.get("miejsce_bez") == str(d) and p["tryb"] in kol]
                    if len(m) != 1:
                        raise BladPorad("porady.csv: miejsce planu %d w trybie %s %s nawilżacza: %d porad (ma być 1)"
                                        % (d, tryb, "z" if nawilzacz else "bez", len(m)))
        konc = [p for p in plan if "koncowa" in p.get("flagi", "")]
        if not konc or any(int(p["dzien"]) != maks for p in konc):
            raise BladPorad("porady.csv: porada z flagą `koncowa` musi stać na ostatnim miejscu planu (%d)" % maks)
    bledy = []
    for p in porady:
        try:
            q = dict(p)
            for pole in ("tytul", "tresc", "krok"):
                q[pole] = podstaw_progi(p[pole], progi, najdluzsze=True)
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
                traf = a.trafienia(p.get("_" + pole, p[pole]), lambda t: podstaw_progi(t, progi))
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
