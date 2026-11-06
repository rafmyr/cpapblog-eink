# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy plików progów docs/spec/progi.csv i docs/spec/progi-poza-zakresem.csv (WY16 (a)).

Każdy wiersz specyfikacji z `[PRÓG]` (poza sekcjami „Wycofane”) dostaje klucz miejsca:
- wiersz tabeli, którego pierwsza komórka zaczyna się od ID (E-20, R-CZY-19, M-17, WY16…): to ID;
- inny wiersz tabeli: „sekcja|pierwsza komórka” (np. `5.9.5|Warunek`, do 40 znaków);
- akapit poza tabelą: „§sekcja” (np. `§4.6`).

Sprawdza:
- kolumny i dozwolone wartości pól obu plików, unikalne `id_progu`;
- każdy klucz miejsca z `[PRÓG]` jest pokryty: ID reguły albo wpis w kolumnie `miejsce`
  w progi.csv, albo wiersz w progi-poza-zakresem.csv z powodem;
- każde ID reguły z progi.csv jest zdefiniowane w spec i nie jest wycofane;
- liczby z `wartosc` występują w wierszach spec danej reguły albo jej miejsc
  (wyjątki z powodem w WYJATKI_WARTOSCI), a także w każdym miejscu z kolumny
  `miejsce` osobno (wyjątki z powodem w WYJATKI_MIEJSC);
- odwołania `=id_progu` wskazują istniejący wiersz kanoniczny;
- w drugą stronę: liczba albo godzina tuż przed `[PRÓG]` w wierszu,
  którego klucz ma wiersz w progi.csv, stoi w wartościach progi.csv dla tego klucza albo w `wartosc`
  wiersza progi-poza-zakresem.csv z tym kluczem;
- definicje z 4.11 i WY16 są w pliku.

Uruchomienie: python3 -m unittest discover -s tools -p 'test_*.py'
"""

import csv
import os
import re
import sys
import unittest

TU = os.path.dirname(os.path.abspath(__file__))
if TU not in sys.path:
    sys.path.insert(0, TU)

import spojnosc  # noqa: E402

REPO = os.path.dirname(TU)
SPEC = os.path.join(REPO, "docs", "spec", "specyfikacja-funkcjonalna.md")
PROGI = os.path.join(REPO, "docs", "spec", "progi.csv")
POZA = os.path.join(REPO, "docs", "spec", "progi-poza-zakresem.csv")

KOLUMNY = ["id_progu", "regula", "miejsce", "wartosc", "jednostka", "opis", "zrodlo", "pochodzenie",
           "tryb", "status"]
KOLUMNY_POZA = ["miejsce", "wartosc", "powod", "opis"]
TRYBY = ("wszystkie", "CPAP", "AutoSet", "ASV", "ASVAuto")  # ADR-0030; wiele trybów rozdzielonych „;”
STATUSY = ("do kalibracji", "definicja", "stała AirBridge", "do zadeklarowania", "kryterium")
# Powody wyłączenia: progi bez wpływu na ocenę nocy i na liczniki WY3, WY4, WY6. Cztery ostatnie
# nie są progami (powtórzenie wartości z progi.csv, historia, pytanie otwarte, opis zasady);
# uzasadnienie w tools/kalibracja/README.md.
POWODY = ("czas interfejsu", "limit tekstu", "wygląd ekranu", "stała AirBridge", "parametr techniczny",
          "powtórzenie", "historia", "pytanie otwarte", "opis")

# Wiersze progi.csv, których liczba nie stoi dosłownie w tekście spec (z powodem).
WYJATKI_WARTOSCI = {
    "R-NOR-04.wyjscie_ulamek_delta": "spec pisze słowami „w połowie Δ”",
    "WY16.data_zamrozenia": "wartość wpisywana przy zamrożeniu",
    "WY16.zrodlo_nocy": "wartość deklarowana przy zamrożeniu",
    "WY16.skrypt_commit": "wartość deklarowana przy zamrożeniu",
    "M-21.maksimum_cisnienia": "wzór z tagów (4.11)",
    "L-04.wariant": "nazwa wybranego wariantu z tabeli wariantów 4.11 (decyzja)",
    "R-DANE-29.tryb_obslugiwany": "lista tagów `MOP` z 4.11, w spec rozdzielona przecinkami (ADR-0030)",
    "R-CZY-24.warstwa_min": "spec pisze słowami „noce z obu warstw w każdej grupie” (co najmniej 1)",
}

# Miejsca (kolumna `miejsce`), w których próg stoi inaczej niż w wierszu reguły: słowami, jako
# zakres albo bez jednostki zaraz po liczbie (31.10.2026). Klucz: (id_progu, miejsce).
WYJATKI_MIEJSC = {
    ("R-NOR-07.zmiana_ustawien", "4.11|zmiana ustawień (4.8, R-HIP-10)"): "wiersz tabeli 4.11 wymienia tagi zakresów, tag `MOP` ma osobny wiersz",
    ("R-NOR-07.zmiana_ustawien", "4.11|zmiana trybu (MOP)"): "wiersz tabeli 4.11 o samym trybie (`MOP`), tagi zakresów w wierszu wyżej",
    ("O-12.efektywna_min", "§5.11.6"): "akapit podaje próg jako zakres „5–9 nocach w grupie”",
    ("R-INF-07.wzrost", "5.12.8|„Gorzej”"): "wiersz pisze słowami „przy podwyższonym M-13 (R-INF-07)”",
    ("R-PIE-10.prosby_dziennie", "7.4|prośby: pytania i ekrany z krokiem (defi"): "liczba stoi przed „dziennie”, jednostka w nazwie wiersza",
    ("R-PIE-12.wieczory", "7.4|wieczory z krokiem (ekran W1–W3 na ekran"): "liczba stoi przed „w oknie”, jednostka w nazwie wiersza",
    ("R-HUM-27.pula_min", "§5.7.3"): "akapit pisze „krótszą niż 3 [PRÓG]” bez słowa „zdania” po liczbie",
}

DEFINICJE_WYMAGANE = ["R-DANE-29.tryb_obslugiwany", "M-21.pola_ustawien", "M-21.maksimum_cisnienia",
                      "H-01.minimum_cisnienia", "R-NOR-07.zmiana_ustawien", "L-04.wariant",
                      "WY16.data_zamrozenia", "WY16.zrodlo_nocy", "WY16.dlugosc_okresu_nocy",
                      "WY16.skrypt_commit"]

_ID_NA_POCZATKU = re.compile(r"(" + "|".join(spojnosc.RODZINY.values()) + r"|WY\d+)(?![\w-])")
_SEKCJA = re.compile(r"([\dA-Z](?:[\d.]*\d)?)\.?\s")


def _dokument():
    with open(SPEC, encoding="utf-8") as f:
        return spojnosc.Dokument(f.read())


def klucz_miejsca(dok, nr, wiersze_tabel):
    """Klucz miejsca dla linii `nr` (numeracja od 1)."""
    sek = dok.sekcje[nr - 1]
    m = _SEKCJA.match(sek)
    sek = m.group(1) if m else sek
    if nr in wiersze_tabel:
        c = spojnosc._czysc(wiersze_tabel[nr][0]) if wiersze_tabel[nr] else ""
        m = _ID_NA_POCZATKU.match(c)
        if m:
            return m.group(1)
        return "%s|%s" % (sek, c[:40])
    return "§" + sek


def wiersze_z_progiem(dok=None):
    """Lista (nr linii, klucz miejsca, tekst) dla linii spec z `[PRÓG]` poza sekcjami „Wycofane”."""
    dok = dok or _dokument()
    wt = {nr: kom for t in dok.tabele for nr, kom in t["wiersze"]}
    out = []
    for nr, linia in enumerate(dok.linie, start=1):
        if "[PRÓG]" in linia and not dok.w_wycofanych[nr - 1]:
            out.append((nr, klucz_miejsca(dok, nr, wt), linia))
    return out


def teksty_wedlug_klucza(dok=None):
    """{klucz: [teksty linii]} dla wszystkich wierszy tabel i akapitów (do kontroli wartości)."""
    dok = dok or _dokument()
    wt = {nr: kom for t in dok.tabele for nr, kom in t["wiersze"]}
    out = {}
    for nr, linia in enumerate(dok.linie, start=1):
        if dok.w_wycofanych[nr - 1] or not linia.strip():
            continue
        out.setdefault(klucz_miejsca(dok, nr, wt), []).append(linia)
    return out


def definicje_spec(dok=None):
    """{ID: (nr linii, czy w sekcji wycofanych)} dla pierwszej definicji ID (tabele „ID” i „#”)."""
    dok = dok or _dokument()
    defs = {}
    for t in dok.tabele:
        nag = t["naglowek"][0] if t["naglowek"] else ""
        for nr, kom in t["wiersze"]:
            if not kom:
                continue
            c = spojnosc._czysc(kom[0])
            if nag == "ID":
                id_ = spojnosc.id_definicji(kom[0])
            elif nag == "#" and re.fullmatch(r"WY\d+", c):
                id_ = c
            else:
                id_ = None
            if id_ and id_ not in defs:
                defs[id_] = (nr, dok.w_wycofanych[nr - 1])
    return defs


# Kierunek spec → CSV. Liczba albo godzina tuż przed `[PRÓG]`
# (w ostatnich 30 znakach przed znacznikiem, po zastąpieniu ID przez „ID”). Pomijane: numery
# rozdziałów (liczba z kropką, np. „4.5”, „7b.5”) i liczebniki porządkowe („1. nocy”).
_TOK_PRZED = re.compile(r"(?<![\w.,:])(\d{1,2}:\d{2}|\d+(?:,\d+)?)(?![\w.,:]|\.\S)(?!\.\s)")
# Wyjątki kierunku spec → CSV: (klucz miejsca, wartość) z powodem.
WYJATKI_SPEC_CSV = {
}


def _norm_wartosc(s):
    return s if ":" in s else repr(float(s.replace(",", ".")))


def _liczby_wartosci(tekst):
    return {_norm_wartosc(m.group(1)) for m in re.finditer(r"(\d{1,2}:\d{2}|\d+(?:[.,]\d+)?)", tekst)}


def progi_spec_bez_csv(dok, progi, poza):
    """Lista (nr linii, klucz, wartość, kontekst) progów spec bez wartości w plikach progów."""
    po_id = {w["id_progu"]: w for w in progi}
    wart, klucze_progi = {}, set()
    for w in progi:
        v = w["wartosc"]
        if v.startswith("="):
            v = po_id[v[1:]]["wartosc"]
        for k in [w["regula"]] + [m for m in w["miejsce"].split(";") if m]:
            klucze_progi.add(k)
            wart.setdefault(k, set()).update(_liczby_wartosci(v))
    for w in poza:
        wart.setdefault(w["miejsce"], set()).update(_liczby_wartosci(w["wartosc"]))
    out = []
    for nr, klucz, linia in wiersze_z_progiem(dok):
        if klucz not in klucze_progi:
            continue
        pos = 0
        for m in re.finditer(r"\[PRÓG\]", linia):
            przed, pos = linia[pos:m.start()], m.end()
            przed = _ID_RE.sub(" ID ", przed.replace("`", "").replace("\u00a0", " ").replace("−", "-"))[-30:]
            toks = list(_TOK_PRZED.finditer(przed))
            if not toks:
                continue
            v = toks[-1].group(1)
            if _norm_wartosc(v) in wart.get(klucz, set()) or (klucz, v) in WYJATKI_SPEC_CSV:
                continue
            out.append((nr, klucz, v, przed.strip()))
    return out


def _czytaj(sciezka):
    """CSV z pominięciem linii komentarza na początku („# …”)."""
    with open(sciezka, encoding="utf-8", newline="") as f:
        linie = [l for l in f if not l.startswith("#")]
    r = csv.DictReader(linie)
    return r.fieldnames, list(r)


# Kontekst jednostki: słowo, które w spec stoi zaraz po liczbie (po pominięciu „[PRÓG]”).
_NOC = r"noc|nocy|noce|nocach|nocą"
_DNI = r"dni|dzień|dnia|doby|dób|dobę|dniach"
KONTEKST = {
    "noce": _NOC, "noce (k z n)": _NOC, "noc trwania": _NOC, "noc okresu": _NOC,
    "dni": _DNI, "doby": _DNI, "pokazania w dniach": _DNI,
    "tygodnie": r"tyg", "tygodnie (k z n)": r"tyg", "weekendy": r"weekend",
    "h": r"h\b|godz", "min": r"min", "s": r"s\b|sek", "%": r"%|pkt proc|proc",
    "% AHI": r"%", "% tła": r"%", "% górnych wartości sezonu": r"%",
    "/h": r"/ ?h\b", "/min": r"/ ?min", "L/min": r"L/min|l/min", "cmH2O": r"cmH2O",
    "mL": r"mL", "ppm": r"ppm", "°C": r"°C", "µg/m³": r"µg", "pkt proc.": r"pkt", "pkt proc./s": r"pkt",
    "pokazania": r"raz|razy|pokaz", "pytania": r"pyta|\(R-SKI-16\)|pokazane",
    "miesiące": r"miesi", "zdania": r"zda", "karty na dobę": r"kart", "odkrycia": r"odkry",
    "ekrany": r"fałszyw|ekran", "kandydaci": r"odrzucon|kandyd", "przypadki": r"\]|:|przypad",
    "wieczory": r"\]|wiecz", "prośby": r"prośb", "wyświetlenia": r"wyświetl",
    "wyświetlenia (k z n)": r"wyświetl", "karty na dobę": r"kart", "wyświetlenia (minimum w max)": r"\)",
}
_ID_RE = re.compile(r"(?<![\w-])(?:" + "|".join(spojnosc.RODZINY.values()) + r"|WY\d+)(?![\w-])")
_LICZBA = re.compile(r"(?<![\w.,])(\d+(?:[.,]\d+)?)(?![\d])(?![.,]\d)")
# Jednostki wartości „k z n”: wartość musi mieć postać k/n.
K_Z_N = ("noce (k z n)", "tygodnie (k z n)", "sygnały (k z n)", "pokazania w dniach", "wyświetlenia (k z n)")
# Słowo przed liczbą, gdy jednostka stoi przed nią („AHI ≥ 5”).
PRZED = {"/h": r"(?:AHI|CA/h|CAI|CA|obturacyjn\w*|zdarze\w*)\s*[≥≤<>=]*\s*$"}


def _kontekst(kont, po):
    """Jednostka zaraz po liczbie, po najwyżej 3 słowach bez cyfr („3 kolejne noce”)."""
    return re.match(r"\s*(?:[^\s\d]+\s+){0,3}?(?:" + kont + ")", po) is not None


def _norm(tekst):
    t = tekst.replace("[PRÓG]", " ").replace("`", "").replace("**", "").replace("\u00a0", " ")
    t = t.replace("−", "-")
    return _ID_RE.sub(" ID ", t)


def wystapienia(tekst):
    """Liczby w tekście spec: lista (wartość, tekst po liczbie, tekst przed liczbą)."""
    t = _norm(tekst)
    return [(float(m.group(1).replace(",", ".")), t[m.end():m.end() + 80], t[max(0, m.start() - 30):m.start()])
            for m in _LICZBA.finditer(t)]


def _liczba(x):
    return float(x.replace(",", "."))


def wartosc_w_spec(wart, jednostka, tekst):
    """Czy wartość z pliku stoi w tekście spec w tym samym kontekście (pusty napis = tak,
    inaczej powód niezgodności)."""
    norm = _norm(tekst)
    czesci = wart.split(";")
    if len(czesci) > 1:
        braki = [c for c in czesci if wartosc_w_spec(c, "", tekst)]
        return "" if not braki else "brak %s" % braki
    if not re.fullmatch(r"[=+-]?\d+(?:[.,]\d+)?(?:/\d+)?", wart):
        return "" if wart in tekst or wart in norm else "brak tekstu %r" % wart
    wyst = wystapienia(tekst)
    m = re.fullmatch(r"(\d+(?:[.,]\d+)?)/(\d+)", wart)
    kont = KONTEKST.get(jednostka)
    if jednostka in K_Z_N and not m:
        return "jednostka %r wymaga wartości k/n" % jednostka
    if m:
        k, n = _liczba(m.group(1)), _liczba(m.group(2))
        for v, po, _ in wyst:
            # para k … n (albo n … k: „w 7 nocach … ≥ 5”) bez innej liczby między nimi, do 60 znaków
            dalej = wystapienia(po[:60])
            if not dalej:
                continue
            nast, po_nast = dalej[0][0], dalej[0][1]
            if (v, nast) == (k, n) and (kont is None or _kontekst(kont, po_nast)):
                return ""
            if (v, nast) == (n, k) and (kont is None or _kontekst(kont, po)):
                return ""
        return "brak „%s” jako k z n" % wart
    v = abs(_liczba(re.sub(r"^[=+-]", "", wart)))
    for x, po, przed in wyst:
        if x != v:
            continue
        if kont is None or _kontekst(kont, po) or (jednostka in PRZED and re.search(PRZED[jednostka], przed)):
            return ""
    return "brak %s z kontekstem %r" % (wart, kont)


class PlikiProgow(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dok = _dokument()
        cls.kolumny, cls.progi = _czytaj(PROGI)
        cls.kolumny_poza, cls.poza = _czytaj(POZA)
        cls.defs = definicje_spec(cls.dok)
        cls.po_id = {w["id_progu"]: w for w in cls.progi}

    def test_kolumny(self):
        self.assertEqual(self.kolumny, KOLUMNY)
        self.assertEqual(self.kolumny_poza, KOLUMNY_POZA)

    def test_pola_progi(self):
        # od wersji beta (spec techniczna 9.3): jeden id_progu może mieć kilka wierszy z różnym `tryb`
        ids = [(w["id_progu"], w["tryb"]) for w in self.progi]
        self.assertEqual(len(ids), len(set(ids)), "powtórzone id_progu z tym samym trybem")
        for w in self.progi:
            with self.subTest(id=w["id_progu"]):
                self.assertTrue(w["id_progu"].startswith(w["regula"] + "."))
                for k in KOLUMNY:
                    if k != "miejsce":
                        self.assertTrue(w[k].strip(), "puste pole %s" % k)
                for t in w["tryb"].split(";"):
                    self.assertIn(t, TRYBY)
                self.assertIn(w["status"], STATUSY)
                self.assertIn(w["pochodzenie"], ("spec 1.2",))
                self.assertNotIn("—", w["opis"])

    def test_powtorzenia_wskazuja_wiersze_progow(self):
        for w in self.poza:
            if w["powod"] == "powtórzenie":
                with self.subTest(miejsce=w["miejsce"]):
                    ids = re.findall(r"[A-Z][\w.-]*\.[\w.-]+", w["opis"])
                    self.assertTrue(ids, "powtórzenie bez wskazania wiersza progi.csv")
                    for id_ in ids:
                        self.assertIn(id_, self.po_id)

    def test_komentarz_o_zamrozeniu(self):
        with open(POZA, encoding="utf-8") as f:
            pierwsza = f.readline()
        self.assertTrue(pierwsza.startswith("#") and "progi-RRRRMMDD" in pierwsza)

    def test_pola_poza(self):
        for w in self.poza:
            with self.subTest(miejsce=w["miejsce"]):
                self.assertIn(w["powod"], POWODY)
                self.assertTrue(w["miejsce"].strip() and w["wartosc"].strip())

    def test_kazdy_prog_spec_jest_pokryty(self):
        pokryte = {w["regula"] for w in self.progi}
        for w in self.progi:
            pokryte |= {m for m in w["miejsce"].split(";") if m}
        pokryte |= {w["miejsce"] for w in self.poza}
        brak = ["%s (l. %d)" % (k, nr) for nr, k, _ in wiersze_z_progiem(self.dok) if k not in pokryte]
        self.assertEqual(brak, [], "[PRÓG] bez wiersza w progi.csv ani w progi-poza-zakresem.csv")

    def test_miejsca_istnieja(self):
        klucze = {k for _, k, _ in wiersze_z_progiem(self.dok)}
        wszystkie = set(teksty_wedlug_klucza(self.dok))
        for w in self.poza:
            with self.subTest(miejsce=w["miejsce"]):
                self.assertIn(w["miejsce"], klucze, "miejsce bez [PRÓG] w spec (nieaktualny wpis)")
        for w in self.progi:
            for m in w["miejsce"].split(";"):
                if m:
                    with self.subTest(id=w["id_progu"], miejsce=m):
                        self.assertIn(m, wszystkie)

    def test_regula_istnieje_w_spec(self):
        for w in self.progi:
            with self.subTest(id=w["id_progu"]):
                self.assertIn(w["regula"], self.defs, "brak definicji %s w specyfikacji" % w["regula"])
                self.assertFalse(self.defs[w["regula"]][1], "%s jest wycofane" % w["regula"])

    def test_odwolania(self):
        for w in self.progi:
            if w["wartosc"].startswith("="):
                with self.subTest(id=w["id_progu"]):
                    cel = self.po_id.get(w["wartosc"][1:])
                    self.assertIsNotNone(cel, "odwołanie do nieistniejącego wiersza")
                    self.assertFalse(cel["wartosc"].startswith("="), "odwołanie do odwołania")
                    self.assertEqual(cel["jednostka"], w["jednostka"])

    def test_wartosc_wystepuje_w_spec(self):
        teksty = teksty_wedlug_klucza(self.dok)
        for w in self.progi:
            if w["id_progu"] in WYJATKI_WARTOSCI:
                continue
            wart = w["wartosc"]
            if wart.startswith("="):
                wart = self.po_id[wart[1:]]["wartosc"]
            klucze = [w["regula"]] + [m for m in w["miejsce"].split(";") if m]
            tekst = "\n".join(l for k in klucze for l in teksty.get(k, []))
            with self.subTest(id=w["id_progu"], wartosc=wart):
                self.assertEqual(wartosc_w_spec(wart, w["jednostka"], tekst), "", "wiersze %s" % klucze)
            # wartość także w każdym miejscu osobno, żeby rozjazd liczby w jednym
            # powtórzeniu nie przechodził dzięki sumie tekstów
            for m in klucze[1:]:
                if (w["id_progu"], m) in WYJATKI_MIEJSC:
                    continue
                with self.subTest(id=w["id_progu"], miejsce=m):
                    self.assertEqual(wartosc_w_spec(wart, w["jednostka"], "\n".join(teksty.get(m, []))), "",
                                     "miejsce %s" % m)

    def test_kontrola_miejsca_osobno_wykrywa_zmiane(self):
        # test negatywny: 70% w L-06 zmienione na 75% musi wywrócić sprawdzenie miejsca L-06
        teksty = teksty_wedlug_klucza(self.dok)
        w = self.po_id["R-SKI-20.pokrycie_spo2"]
        dobry = "\n".join(teksty.get("L-06", []))
        self.assertEqual(wartosc_w_spec(w["wartosc"], w["jednostka"], dobry), "")
        self.assertNotEqual(wartosc_w_spec(w["wartosc"], w["jednostka"], dobry.replace("70%", "75%")), "")

    def test_prog_spec_ma_wartosc_w_csv(self):
        brak = progi_spec_bez_csv(self.dok, self.progi, self.poza)
        self.assertEqual(brak, [], "[PRÓG] z liczbą spoza progi.csv i progi-poza-zakresem.csv")

    def test_kierunek_spec_csv_wykrywa_zmiane(self):
        # test negatywny na kopii w pamięci: 27 L/min w E-23 zmienione na 28
        # (do 01.11.2026 test zmieniał 36 L/min w R-DANE-23; próg maski twarzowej usunięty)
        with open(SPEC, encoding="utf-8") as f:
            tekst = f.read()
        linie = [l for l in tekst.split("\n") if l.startswith("| E-23 | skuteczność") and "27 L/min [PRÓG]" in l]
        self.assertEqual(len(linie), 1)
        zly = spojnosc.Dokument(tekst.replace(linie[0], linie[0].replace("27 L/min [PRÓG]", "28 L/min [PRÓG]")))
        brak = progi_spec_bez_csv(zly, self.progi, self.poza)
        self.assertIn("E-23", [k for _, k, _, _ in brak])
        self.assertIn("28", [v for _, _, v, _ in brak])

    def test_kontrola_wartosci_wykrywa_zmiane(self):
        # test negatywny na kopii w pamięci: R-KON-14 = 2 razy w 7 dniach
        teksty = teksty_wedlug_klucza(self.dok)
        w = dict(self.po_id["R-KON-14.pokazania"])
        tekst = "\n".join(teksty.get("R-KON-14", []))
        self.assertEqual(wartosc_w_spec(w["wartosc"], w["jednostka"], tekst), "")
        for zla in ("5/7", "4", "2/14", "7/2"):
            with self.subTest(zla=zla):
                self.assertNotEqual(wartosc_w_spec(zla, w["jednostka"], tekst), "")
        # od 04.11.2026 E-30 liczy medianę (konsultacja lekarska), więc próbka „k z n” to L-06(a)
        w = self.po_id["L-06.a_noce"]
        tekst = "\n".join(teksty.get("L-06", []))
        self.assertEqual(wartosc_w_spec("3/7", w["jednostka"], tekst), "")
        self.assertNotEqual(wartosc_w_spec("4/7", w["jednostka"], tekst), "")
        # ta sama liczba w innym kontekście nie wystarcza
        self.assertNotEqual(wartosc_w_spec("10", "noce", "mediana AHI ≥ 10 / h"), "")
        self.assertEqual(wartosc_w_spec("10", "/h", "mediana AHI ≥ 10 / h"), "")

    def test_definicje_kazdego_trybu(self):
        # ADR-0030: każdy tryb obsługiwany ma pola ustawień i definicję zmiany ustawień
        tryby = self.po_id["R-DANE-29.tryb_obslugiwany"]["wartosc"].split(";")
        for tryb in tryby:
            for baza in ("M-21.pola_ustawien", "R-NOR-07.zmiana_ustawien"):
                with self.subTest(tryb=tryb, baza=baza):
                    self.assertTrue(any(w["id_progu"].startswith(baza) and tryb in w["tryb"].split(";")
                                        for w in self.progi))

    def test_wyjatki_wartosci_istnieja(self):
        for id_ in WYJATKI_WARTOSCI:
            self.assertIn(id_, self.po_id)

    def test_definicje_wymagane(self):
        for id_ in DEFINICJE_WYMAGANE:
            with self.subTest(id=id_):
                self.assertIn(id_, self.po_id)
        self.assertEqual(self.po_id["R-DANE-29.tryb_obslugiwany"]["wartosc"], "CPAP;AutoSet;ASV;ASVAuto")
        self.assertEqual(self.po_id["R-DANE-29.tryb_obslugiwany"]["tryb"], "wszystkie")
        self.assertEqual(self.po_id["L-04.wariant"]["wartosc"], "EPAP (wariant 2)")
        self.assertEqual(self.po_id["WY16.dlugosc_okresu_nocy"]["wartosc"], "30")
        # wariant ASVAuto progów L-04 to wiersz z tym samym id_progu i trybem ASVAuto
        # (wcześniej sufiks `.asvauto.`); każdy próg L-04 ma wiersz AutoSet i wiersz ASVAuto
        l04 = {}
        for w in self.progi:
            if w["regula"] == "L-04" and w["id_progu"] != "L-04.wariant":
                self.assertIn(w["tryb"], ("AutoSet", "ASVAuto"), w["id_progu"])
                l04.setdefault(w["id_progu"], set()).add(w["tryb"])
        for id_, t in l04.items():
            with self.subTest(id=id_):
                if id_ in ("L-04.margines_max", "L-04.noce", "L-04.zdarzenia_obturacyjne", "L-04.wyjscie_noce",
                           "L-04.wyjscie_zdarzenia"):
                    self.assertEqual(t, {"AutoSet", "ASVAuto"})


if __name__ == "__main__":
    unittest.main()
