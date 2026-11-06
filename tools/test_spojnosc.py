# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy skryptu spójności: python3 -m unittest tools/test_spojnosc.py"""

import csv
import io
import json
import os
import sys
import tempfile
import unittest
from collections import OrderedDict
from contextlib import redirect_stdout

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import spojnosc as s  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = os.path.join(REPO, "docs/spec/specyfikacja-funkcjonalna.md")
WYCOFANE = os.path.join(REPO, "tools/wycofane.csv")


def wyc(**kw):
    """wyc(E_32='wycofane') -> słownik jak z wczytaj_wycofane."""
    out = OrderedDict()
    for k, v in kw.items():
        out[k.replace("_", "-").replace("SK-", "S-K-")] = {"status": v, "nastepca": "X", "zrodlo": "test"}
    return out


def ids(tekst):
    return sorted({i for i, _ in s.znajdz_id(tekst)})


def wpisy(wynik, sprawdzenie, poziom=None):
    return [w for w in wynik.wpisy if w["sprawdzenie"] == sprawdzenie
            and (poziom is None or w["poziom"] == poziom)]


KATALOG = """# Spec

| ID | Opis |
|---|---|
| E-01 | brak danych |
| S-K-01 | ekran |
| L-01 | lekarz |

| ID | Wzorzec | Wymaganie | Weryfikacja |
|---|---|---|---|
| R-KON-01 | Event | WHEN coś the Panel shall pokazać S-K-01 i L-01. | Test E-01 → wynik. |
"""


# ---------------------------------------------------------------------------
class TestGraniceIRodziny(unittest.TestCase):
    def test_e40_nie_lapie_e40w(self):
        self.assertEqual(ids("E-40w i E-40."), ["E-40", "E-40w"])
        self.assertEqual(ids("tylko E-40w"), ["E-40w"])
        self.assertEqual(ids("tylko E-40, nic więcej"), ["E-40"])

    def test_l01_nie_lapie_l010(self):
        self.assertEqual(ids("L-010 to nie ID"), [])
        self.assertEqual(ids("L-01."), ["L-01"])

    def test_id_wewnatrz_innego_id(self):
        # E-01 nie może się złapać w R-PIE-01, K-22 w S-K-22.
        self.assertEqual(ids("R-PIE-01"), ["R-PIE-01"])
        self.assertEqual(ids("S-K-22"), ["S-K-22"])

    def test_sufiksy(self):
        self.assertEqual(ids("M-05p, R-LEK-03b, L-06(b)"),
                         ["L-06", "M-05p", "R-LEK-03b"])

    def test_sam_prefiks_to_nie_id(self):
        self.assertEqual(ids("wg R-BUZ)"), [])


class TestZakresyISkroty(unittest.TestCase):
    def test_zakres_do_pelny(self):
        self.assertEqual(ids("S-K-22 do S-K-25"), ["S-K-22", "S-K-23", "S-K-24", "S-K-25"])

    def test_zakres_do_skrocony(self):
        self.assertEqual(ids("(S-K-22 do 25, E-30)"), ["E-30", "S-K-22", "S-K-23", "S-K-24", "S-K-25"])

    def test_zakres_wielokropek(self):
        self.assertEqual(ids("S-K-22…25"), ["S-K-22", "S-K-23", "S-K-24", "S-K-25"])
        self.assertEqual(len(ids("L-01…L-12")), 12)
        self.assertEqual(ids("H-01...H-03"), ["H-01", "H-02", "H-03"])

    def test_zakres_polpauza(self):
        self.assertEqual(ids("E-40–E-42"), ["E-40", "E-41", "E-42"])

    def test_skrot_ukosnik(self):
        self.assertEqual(ids("E-30/32/34/38)"), ["E-30", "E-32", "E-34", "E-38"])
        self.assertEqual(ids("S-K-31/32 i"), ["S-K-31", "S-K-32"])
        self.assertEqual(ids("L-01/03/04 tylko"), ["L-01", "L-03", "L-04"])

    def test_skrot_przecinki(self):
        self.assertEqual(ids("S-K-01, 02, 04."), ["S-K-01", "S-K-02", "S-K-04"])

    def test_skrot_przecinki_z_zakresem(self):
        self.assertEqual(ids("S-K-07, 09, 13, 15, 19 do 21, 26)"),
                         ["S-K-07", "S-K-09", "S-K-13", "S-K-15", "S-K-19", "S-K-20", "S-K-21", "S-K-26"])

    def test_liczba_z_jednostka_to_nie_skrot(self):
        self.assertEqual(ids("M-17, 10 nocy"), ["M-17"])
        self.assertEqual(ids("E-30, 3 z 7"), ["E-30"])
        self.assertEqual(ids("M-01 do 14 nocy"), ["M-01"])
        self.assertEqual(ids("L-01, 1,5 h"), ["L-01"])

    def test_trzy_cyfry_to_nie_id(self):
        # każda rodzina ma numer dwucyfrowy
        self.assertEqual(ids("L-010 i E-100"), [])

    def test_zakres_malejacy_nie_rozwija(self):
        self.assertEqual(ids("S-K-25 do S-K-22"), ["S-K-22", "S-K-25"])


# ---------------------------------------------------------------------------
class TestA_Definicje(unittest.TestCase):
    def test_przechodzi(self):
        w, _, _ = s.sprawdz(KATALOG, OrderedDict(), None)
        self.assertEqual(wpisy(w, "a", "blad"), [])

    def test_podwojna_definicja(self):
        tekst = KATALOG + "\n| ID | Inne |\n|---|---|\n| E-01 | znowu |\n| E-02 | nowe |\n"
        w, _, _ = s.sprawdz(tekst, OrderedDict(), None)
        bl = wpisy(w, "a", "blad")
        self.assertEqual(len(bl), 1)
        self.assertEqual(bl[0]["id"], "E-01")

    def test_tabela_uzupelniajaca_to_ostrzezenie(self):
        tekst = KATALOG + "\n| ID | Tytuł |\n|---|---|\n| L-01 | tekst |\n"
        w, _, _ = s.sprawdz(tekst, OrderedDict(), None)
        self.assertEqual(wpisy(w, "a", "blad"), [])
        self.assertEqual(len(wpisy(w, "a", "ostrzezenie")), 1)

    def test_definicja_z_dopiskiem_w_nawiasie(self):
        self.assertEqual(s.id_definicji("E-20 (od v0.3: zastąpione)"), "E-20")
        self.assertEqual(s.id_definicji("**S-K-01**"), "S-K-01")
        self.assertIsNone(s.id_definicji("L-07, L-08, L-10"))
        self.assertIsNone(s.id_definicji("M-01 Czas na masce"))

    def test_tabela_bez_naglowka_id_nie_definiuje(self):
        tekst = "| Dotyczy | Pytanie |\n|---|---|\n| E-05 | x |\n"
        w, _, _ = s.sprawdz(tekst, OrderedDict(), None)
        self.assertEqual([x["id"] for x in wpisy(w, "b", "blad")], ["E-05"])


class TestB_UzyteZdefiniowane(unittest.TestCase):
    def test_przechodzi(self):
        w, _, _ = s.sprawdz(KATALOG, OrderedDict(), None)
        self.assertEqual(wpisy(w, "b"), [])

    def test_uzycie_bez_definicji(self):
        w, _, _ = s.sprawdz(KATALOG + "\nPatrz E-99 i S-K-01.\n", OrderedDict(), None)
        bl = wpisy(w, "b", "blad")
        self.assertEqual([x["id"] for x in bl], ["E-99"])
        self.assertEqual(bl[0]["linia"], 13)

    def test_wycofane_nie_jest_bledem_b(self):
        w, _, _ = s.sprawdz(KATALOG + "\nDawniej E-99.\n", wyc(E_99="wycofane"), None)
        self.assertEqual(wpisy(w, "b"), [])


class TestC_Wycofane(unittest.TestCase):
    def test_przechodzi(self):
        w, _, _ = s.sprawdz(KATALOG, wyc(E_32="wycofane"), None)
        self.assertEqual(wpisy(w, "c"), [])

    def test_odwolanie_do_wycofanego(self):
        w, _, _ = s.sprawdz(KATALOG + "\nE-32 wyłącza E-33.\n", wyc(E_32="wycofane"), None)
        bl = wpisy(w, "c", "blad")
        self.assertEqual([(x["id"], x["linia"]) for x in bl], [("E-32", 13)])

    def test_odwolanie_przez_zakres(self):
        w, _, _ = s.sprawdz("Rano: S-K-20 do S-K-24.\n", wyc(SK_22="wycofane"), None)
        self.assertEqual([x["id"] for x in wpisy(w, "c", "blad")], ["S-K-22"])

    def test_sekcja_wycofane_jest_zwolniona(self):
        tekst = KATALOG + "\n## 10. Wycofane ID\n\n| ID | Następca |\n|---|---|\n| E-32 | L-01 |\n\n## 11. Dalej\n\nE-32 tu nie wolno.\n"
        w, _, _ = s.sprawdz(tekst, wyc(E_32="wycofane"), None)
        bl = wpisy(w, "c", "blad")
        self.assertEqual(len(bl), 1)
        self.assertEqual(bl[0]["linia"], tekst.split("\n").index("E-32 tu nie wolno.") + 1)

    def _csv(self, tresc):
        d = tempfile.mkdtemp()
        p = os.path.join(d, "wycofane.csv")
        with open(p, "w", encoding="utf-8", newline="") as f:
            f.write(tresc)
        return p

    def test_powtorzone_id_w_csv_to_ostrzezenie(self):
        # drugi wiersz tego samego ID przykrywał pierwszy bez śladu (tu „wycofane” przez „zmienione”)
        p = self._csv('id,status,nastepca,zrodlo\nE-32,wycofane,L-01,a\nE-01,zmienione,x,b\nE-32,zmienione,y,c\n')
        wy = s.wczytaj_wycofane(p)
        self.assertEqual(dict(wy.powtorzone), {"E-32": [2, 4]})
        w, _, _ = s.sprawdz(KATALOG, wy, None)
        ostrz = wpisy(w, "c", "ostrzezenie")
        self.assertEqual([(x["id"], x["linia"]) for x in ostrz], [("E-32", 0)])
        self.assertIn("l. 2, 4", ostrz[0]["tresc"])

    def test_bez_powtorzen_bez_ostrzezenia(self):
        p = self._csv('id,status,nastepca,zrodlo\nE-32,wycofane,L-01,a\nE-01,zmienione,"x; y","b; c"\n')
        wy = s.wczytaj_wycofane(p)
        self.assertEqual(dict(wy.powtorzone), {})
        w, _, _ = s.sprawdz(KATALOG, wy, None)
        self.assertEqual(wpisy(w, "c", "ostrzezenie"), [])

    @unittest.skipUnless(os.path.exists(WYCOFANE), "brak wycofane.csv")
    def test_repo_wycofane_jeden_wiersz_na_id(self):
        self.assertEqual(dict(s.wczytaj_wycofane(WYCOFANE).powtorzone), {})

    def test_zmienione_to_informacja(self):
        w, _, _ = s.sprawdz(KATALOG + "\nE-01 inaczej.\n", wyc(E_01="zmienione"), None)
        self.assertEqual(wpisy(w, "c", "blad"), [])
        self.assertTrue(wpisy(w, "c", "info"))

    def test_s4_tylko_samodzielne_slowo(self):
        w, _, _ = s.sprawdz("Stan S4 i S3/S4.\nS45, S4a, xS4, S-S4 to nie stan.\n", wyc(S4="wycofane"), None)
        self.assertEqual(sorted({x["linia"] for x in wpisy(w, "c", "blad")}), [1])

    def test_s4_w_zakresie_stanow_to_informacja(self):
        w, _, _ = s.sprawdz("brak S3–S5\n", wyc(S4="wycofane"), None)
        self.assertEqual(wpisy(w, "c", "blad"), [])
        self.assertEqual(len(wpisy(w, "c", "info")), 1)


class TestD_Weryfikacja(unittest.TestCase):
    def test_przechodzi(self):
        w, _, _ = s.sprawdz(KATALOG, OrderedDict(), None)
        self.assertEqual(wpisy(w, "d"), [])

    def test_pusta_weryfikacja(self):
        tekst = KATALOG + "| R-KON-02 | Event | WHEN x the Panel shall y. |  |\n| R-KON-03 | Event | WHEN z the Panel shall y. | \u2014 |\n"
        w, _, _ = s.sprawdz(tekst, OrderedDict(), None)
        self.assertEqual([x["id"] for x in wpisy(w, "d", "blad")], ["R-KON-02", "R-KON-03"])

    def test_brak_kolumny(self):
        tekst = "| ID | Wzorzec | Wymaganie |\n|---|---|---|\n| R-X-01 | Event | y |\n"
        w, _, _ = s.sprawdz(tekst, OrderedDict(), None)
        self.assertEqual(len(wpisy(w, "d", "blad")), 1)


class TestE_EkranMaWymaganie(unittest.TestCase):
    def test_przechodzi(self):
        w, _, _ = s.sprawdz(KATALOG, OrderedDict(), None, strict=True)
        self.assertEqual(wpisy(w, "e"), [])

    def test_ekran_bez_wymagania(self):
        tekst = KATALOG.replace("| L-01 | lekarz |", "| L-01 | lekarz |\n| L-02 | inny |")
        w, _, _ = s.sprawdz(tekst, OrderedDict(), None)
        self.assertEqual([(x["id"], x["poziom"]) for x in wpisy(w, "e")], [("L-02", "ostrzezenie")])
        w, _, _ = s.sprawdz(tekst, OrderedDict(), None, strict=True)
        self.assertEqual([(x["id"], x["poziom"]) for x in wpisy(w, "e")], [("L-02", "blad")])

    def test_wymaganie_przez_zakres(self):
        tekst = KATALOG.replace("| L-01 | lekarz |", "| L-01 | lekarz |\n| L-02 | inny |").replace(
            "pokazać S-K-01 i L-01.", "pokazać S-K-01 i L-01…L-02.")
        w, _, _ = s.sprawdz(tekst, OrderedDict(), None, strict=True)
        self.assertEqual(wpisy(w, "e"), [])

    def test_ekran_z_kanwy_bez_wymagania(self):
        w, _, _ = s.sprawdz(KATALOG, OrderedDict(), {"S-K-01": "SK01.dc.html", "L-01": "L01.dc.html",
                                                    "S-K-09": "SK09-x.dc.html"})
        self.assertIn("S-K-09", [x["id"] for x in wpisy(w, "e")])


class TestU_EkranyKonfiguracji(unittest.TestCase):
    """Rodzina U-xx (ekrany konfiguracji, ADR-0011): definicja, ekran w e, bez kanwy w f."""
    TEKST = KATALOG.replace("| L-01 | lekarz |", "| L-01 | lekarz |\n| U-01 | Ustawienia |\n| U-02 | Pierwsza konfiguracja |")

    def test_rodzina_i_granice(self):
        self.assertEqual(s.rodzina("U-01"), "U")
        self.assertEqual(ids("U-01…U-03 i UA"), ["U-01", "U-02", "U-03"])
        self.assertEqual(ids("XU-01"), [])

    def test_ekran_bez_wymagania_e(self):
        tekst = self.TEKST.replace("pokazać S-K-01 i L-01.", "pokazać S-K-01, L-01 i U-01.")
        w, _, _ = s.sprawdz(tekst, OrderedDict(), None)
        self.assertEqual([x["id"] for x in wpisy(w, "e")], ["U-02"])

    def test_brak_artboardu_nie_daje_f(self):
        w, _, _ = s.sprawdz(self.TEKST, OrderedDict(), {"S-K-01": "SK01-a.dc.html", "L-01": "L01-b.dc.html"})
        self.assertEqual(wpisy(w, "f"), [])

    def test_uzycie_bez_definicji_b(self):
        w, _, _ = s.sprawdz(KATALOG + "\nZob. U-07.\n", OrderedDict(), None)
        self.assertIn("U-07", [x["id"] for x in wpisy(w, "b")])


class TestF_Kanwa(unittest.TestCase):
    def test_przechodzi(self):
        w, _, _ = s.sprawdz(KATALOG, OrderedDict(), {"S-K-01": "SK01-a.dc.html", "L-01": "L01-b.dc.html"})
        self.assertEqual(wpisy(w, "f"), [])

    def test_niezgodnosc_w_obie_strony(self):
        w, _, _ = s.sprawdz(KATALOG, OrderedDict(), {"S-K-01": "SK01-a.dc.html", "S-K-30": "SK30-x.dc.html"})
        f = sorted(x["id"] for x in wpisy(w, "f", "ostrzezenie"))
        self.assertEqual(f, ["L-01", "S-K-30"])

    def test_wzorzec_bez_pelnego_ekranu_bez_ostrzezenia(self):
        # przegląd 31.10.2026: L-07 ma tylko pilność C, artboard nie jest potrzebny
        tekst = KATALOG + "\n| Artboard | Co poprawić |\n|---|---|\n| nie dotyczy (L-01; L-02 w pilności C) | brak pełnego ekranu |\n"
        w, _, _ = s.sprawdz(tekst, OrderedDict(), {"S-K-01": "SK01-a.dc.html"})
        self.assertEqual(wpisy(w, "f"), [])
        w, _, _ = s.sprawdz(KATALOG, OrderedDict(), {"S-K-01": "SK01-a.dc.html"})
        self.assertEqual([x["id"] for x in wpisy(w, "f", "ostrzezenie")], ["L-01"])

    def test_nazwy_artboardow(self):
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as f:
            json.dump({"boards": {"SK10-przeciek.dc.html": {}, "L01-centralne.dc.html": {},
                                  "K09-przeciek.dc.html": {}, "Main.dc.html": {}}}, f)
        try:
            self.assertEqual(list(s.wczytaj_kanwe(f.name).items()),
                             [("S-K-10", "SK10-przeciek.dc.html"), ("L-01", "L01-centralne.dc.html")])
        finally:
            os.unlink(f.name)


class TestG_StatystykaIMacierz(unittest.TestCase):
    def test_statystyka(self):
        tekst = KATALOG + "\nx [PRÓG] y [PRÓG]\nz [DANE?]\n"
        _, st, _ = s.sprawdz(tekst, OrderedDict(), None)
        self.assertEqual(st["prog_wystapienia"], 2)
        self.assertEqual(st["dane_wystapienia"], 1)
        self.assertEqual(st["wymagania_na_prefiks"], {"R-KON": 1})

    def test_macierz(self):
        _, _, m = s.sprawdz(KATALOG, OrderedDict(), None)
        self.assertEqual(sorted(m), [("R-KON-01", "E-01"), ("R-KON-01", "L-01"), ("R-KON-01", "S-K-01")])

    def test_wiersze_macierzy_kolumna_ekranu_i_sekcji(self):
        tekst = KATALOG + "\n## Moduł X\n\n| ID | Wzorzec | Wymaganie | Weryfikacja |\n|---|---|---|---|\n| R-X-01 | Event | WHEN M-01 the Panel shall U-01 i S4. | Test L-01 → wynik. |\n"
        _, st, m = s.sprawdz(tekst, wyc(S4="wycofane"), None)
        w = s.wiersze_macierzy(m, st)
        self.assertIn(["R-X-01", "U-01", "U", "tak", "Moduł X", "wymaganie", "weryfikacja R-X-01"], w)
        self.assertIn(["R-X-01", "M-01", "M", "nie", "Moduł X", "wymaganie", "weryfikacja R-X-01"], w)
        self.assertIn(["R-X-01", "S4", "inne", "nie", "Moduł X", "wymaganie", "weryfikacja R-X-01"], w)
        self.assertIn(["R-X-01", "L-01", "L", "tak", "Moduł X", "wymaganie", "weryfikacja R-X-01"], w)

    def test_main_kod_wyjscia_i_plik_macierzy(self):
        with tempfile.TemporaryDirectory() as d:
            spec = os.path.join(d, "spec.md")
            with open(spec, "w", encoding="utf-8") as f:
                f.write(KATALOG)
            mac = os.path.join(d, "m.csv")
            with redirect_stdout(io.StringIO()):
                self.assertEqual(s.main([spec, "--wycofane", "", "--kanwa", "", "--macierz", mac]), 0)
            with open(mac, encoding="utf-8") as f:
                wiersze = list(csv.reader(f))
            self.assertEqual(wiersze[0], ["wymaganie", "id_odwolania", "rodzina_odwolania", "ekran", "sekcja_wymagania",
                                          "zrodlo_powiazania", "przypadek_testowy"])
            self.assertIn(["R-KON-01", "S-K-01", "S-K", "tak", "Spec", "wymaganie", "weryfikacja R-KON-01"], wiersze)
            # Od kandydata 2: zdarzenia E są ekranami (sprawdzenie e).
            self.assertIn(["R-KON-01", "E-01", "E", "tak", "Spec", "wymaganie", "weryfikacja R-KON-01"], wiersze)
            with open(spec, "a", encoding="utf-8") as f:
                f.write("\nE-77\n")
            with redirect_stdout(io.StringIO()):
                self.assertEqual(s.main([spec, "--wycofane", "", "--kanwa", ""]), 1)


class TestMacierzWymaganieBezPowiazan(unittest.TestCase):
    # przegląd 31.10.2026: wymaganie bez odwołań nie znika z macierzy
    def test_wiersz_brak(self):
        tekst = KATALOG + "\n| ID | Wzorzec | Wymaganie | Weryfikacja |\n|---|---|---|---|\n| R-X-02 | Ubiquitous | The Panel shall coś robić. | Test → wynik. |\n"
        w, st, m = s.sprawdz(tekst, OrderedDict(), None)
        wiersze = s.wiersze_macierzy(m, st)
        self.assertIn(["R-X-02", "", "brak", "nie", "Spec", "brak powiązania", "weryfikacja R-X-02"], wiersze)
        self.assertNotIn("R-KON-01", [r[0] for r in wiersze if r[2] == "brak"])
        self.assertIn("R-X-02", [x["id"] for x in wpisy(w, "e", "info")])


class TestE_EkranySpozaRodzinIKatalog(unittest.TestCase):
    """Rozszerzone e: stany, kokpit i ekrany nazwane z katalogu; powiązanie przez kolumnę „Wymagania”."""
    KAT = KATALOG + """
| Ekran | Nazwa | Wymagania |
|---|---|---|
| S5 | Brak danych | R-KON-01 |
| K2 | Kokpit | |
| „Sprzęt” | Wiek elementów | R-KON-01 |
| O-03…O-04 | Odkrycia | R-KON-01 |

| ID | Rodzaj |
|---|---|
| O-03 | a |
| O-04 | b |

| # | Sytuacja | Oczekiwany ekran | Gdzie |
|---|---|---|---|
| 1 | x | S5 | R-KON-01 |
"""

    def test_katalog_daje_powiazanie(self):
        w, st, m = s.sprawdz(self.KAT, OrderedDict(), None, strict=True)
        self.assertIn(("R-KON-01", "S5"), m)
        self.assertIn(("R-KON-01", "„Sprzęt”"), m)
        # Wiersz zbiorczy (O-03…O-04) nie wiąże wymagania, które tych ekranów nie wymienia.
        self.assertNotIn(("R-KON-01", "O-04"), m)
        self.assertEqual(st["zrodla_powiazan"]["R-KON-01|S5"], "katalog")
        self.assertEqual(st["przypadki"]["R-KON-01"], ["1"])

    def test_stan_z_katalogu_bez_wymagania(self):
        w, _, _ = s.sprawdz(self.KAT, OrderedDict(), None, strict=True)
        self.assertEqual([(x["id"], x["poziom"]) for x in wpisy(w, "e")],
                         [("O-03", "blad"), ("O-04", "blad"), ("K2", "blad")])

    def test_kolumna_przypadku_testowego(self):
        _, st, m = s.sprawdz(self.KAT, OrderedDict(), None)
        w = s.wiersze_macierzy(m, st)
        self.assertIn(["R-KON-01", "S5", "stan", "tak", "Spec", "katalog", "weryfikacja R-KON-01; przypadki 7.8: 1"], w)
        self.assertIn(["R-KON-01", "„Sprzęt”", "nazwany", "tak", "Spec", "katalog", "weryfikacja R-KON-01; przypadki 7.8: 1"], w)


class TestI_TytulyUnikalne(unittest.TestCase):
    def sprawdz(self, tekst):
        w, _, _ = s.sprawdz("# Spec\n\n" + tekst, OrderedDict(), None)
        return [x["tresc"] for x in wpisy(w, "i")]

    def test_rozne_tytuly(self):
        tab = ("| Wzorzec | Tytuł |\n|---|---|\n| L-01 | `Bezdechy centralne utrwalone` |\n"
               "| L-05 | `Zmiana oddechu od kilku nocy` |\n")
        self.assertEqual(self.sprawdz(tab), [])

    def test_ten_sam_tytul_dwa_ekrany(self):
        tab = ("| Wzorzec | Tytuł |\n|---|---|\n| L-01 | `Więcej bezdechów centralnych` |\n\n"
               "| ID | Poziom | Komunikat |\n|---|---|---|\n"
               "| E-33 | problem | `Więcej bezdechów centralnych` / `{x} / h, zwykle {norma}.` / `→ Obserwuj kolejne noce.` |\n")
        bledy = self.sprawdz(tab)
        self.assertEqual(len(bledy), 1)
        self.assertIn("E-33, L-01", bledy[0])

    def test_ten_sam_ekran_dwa_razy(self):
        tab = ("| Wzorzec | Tytuł |\n|---|---|\n| L-06 (a), (b) | `Saturacja poniżej 88%` |\n\n"
               "| Ekran | Tytuł |\n|---|---|\n| L-06 | `Saturacja poniżej 88%` |\n")
        self.assertEqual(self.sprawdz(tab), [])

    def test_obecna_specyfikacja(self):
        with open(SPEC, encoding="utf-8") as f:
            self.assertEqual(self.sprawdz(f.read()), [])


class TestH_DlugoscTekstow(unittest.TestCase):
    def sprawdz(self, tabela):
        w, _, _ = s.sprawdz("# Spec\n\n" + tabela, OrderedDict(), None)
        return [x["tresc"] for x in wpisy(w, "h")]

    def test_w_limicie(self):
        self.assertEqual(self.sprawdz("| Ekran | Tytuł | Powód | Krok | Krok na karcie |\n|---|---|---|---|---|\n"
                                      "| S-K-01 | `Przewietrz sypialnię` | `Zeszłej nocy CO2 przez {h} h.` | `→ Uchyl okno na noc.` | tak |\n"), [])

    def test_tytul_po_wstawieniu_wartosci(self):
        bledy = self.sprawdz("| Ekran | Tytuł |\n|---|---|\n| X | `Bardzo długi tytuł z datą {data} i x` |\n")
        self.assertEqual(len(bledy), 1)
        self.assertIn("limit 30", bledy[0])

    def test_krok_karty(self):
        tab = ("| Ekran | Krok | Krok na karcie |\n|---|---|---|\n"
               "| A | `→ Krok o długości ponad trzydzieści cztery znaki.` | tak |\n"
               "| B | `→ Krok o długości ponad trzydzieści cztery znaki.` | nie dotyczy |\n")
        bledy = self.sprawdz(tab)
        self.assertEqual(len(bledy), 1)
        self.assertIn("limit 34", bledy[0])

    def test_nieznana_zmienna(self):
        bledy = self.sprawdz("| Ekran | Powód |\n|---|---|\n| X | `Coś {nieznana}.` |\n")
        self.assertTrue(any("{nieznana}" in b for b in bledy))

    def test_komunikat_i_poziom_linia(self):
        tab = ("| ID | Poziom | Komunikat |\n|---|---|---|\n"
               "| E-04 | linia | `Noc {data} zapisana częściowo, liczby mogą być niepełne.` |\n"
               "| E-10 | linia; problem | linia: `Noc {data} bez terapii.`; problem: `Noce bez terapii: {n}` / `Ostatnia: {data}.` / `→ Wróć do terapii.` |\n")
        self.assertEqual(self.sprawdz(tab), [])

    def test_linia_w_wymaganiu(self):
        tab = ("| ID | Wzorzec | Wymaganie | Weryfikacja |\n|---|---|---|---|\n"
               "| R-X-01 | Event | WHEN x the Panel shall pokazać linię `" + "a" * 61 + "` | t |\n")
        self.assertEqual(len(self.sprawdz(tab)), 1)

    def test_pomijane_zmienne(self):
        self.assertEqual(self.sprawdz("| Ekran | Tytuł |\n|---|---|\n| O-12 | `{Odmiana}: {zmiana}` |\n"), [])


# ---------------------------------------------------------------------------
# Forma weryfikacji, słowa EARS, wiersze
# zbiorcze katalogu, tabela skrócona 9.2, „\|” w komórce.
# ---------------------------------------------------------------------------
NAG_R = "| ID | Wzorzec | Wymaganie | Weryfikacja |\n|---|---|---|---|\n"


def wym(wiersze, strict=False):
    w, st, m = s.sprawdz("# Spec\n\n" + NAG_R + wiersze, OrderedDict(), None, strict=strict)
    return w, st, m


class TestD_FormaWeryfikacji(unittest.TestCase):
    def test_wejscie_strzalka_wynik_przechodzi(self):
        w, _, _ = wym("| R-X-01 | Event | WHEN a the Panel shall b. | Wejście a → wynik `b`. |\n")
        self.assertEqual(wpisy(w, "d"), [])

    def test_brak_strzalki_ostrzezenie_a_strict_blad(self):
        wiersz = "| R-X-01 | Event | WHEN a the Panel shall b. | Przypadek 6 z 7.8. |\n"
        w, _, _ = wym(wiersz)
        self.assertEqual([(x["id"], x["poziom"]) for x in wpisy(w, "d")], [("R-X-01", "ostrzezenie")])
        w, _, _ = wym(wiersz, strict=True)
        self.assertEqual([(x["id"], x["poziom"]) for x in wpisy(w, "d")], [("R-X-01", "blad")])

    def test_strzalka_tylko_w_tekscie_ekranu_nie_liczy_sie(self):
        w, _, _ = wym("| R-X-01 | Event | WHEN a the Panel shall b. | Ekran z krokiem `→ Pokaż lekarzowi.` |\n",
                      strict=True)
        self.assertEqual(len(wpisy(w, "d", "blad")), 1)

    def test_strzalka_w_tekscie_i_poza_nim(self):
        w, _, _ = wym("| R-X-01 | Event | WHEN a the Panel shall b. | Noc x → krok `→ Odchylenia liczę od tego.` |\n",
                      strict=True)
        self.assertEqual(wpisy(w, "d"), [])

    def test_pusty_wynik_po_strzalce(self):
        w, _, _ = wym("| R-X-01 | Event | WHEN a the Panel shall b. | Wejście a → ; wejście b → wynik. |\n")
        self.assertEqual([x["poziom"] for x in wpisy(w, "d")], ["ostrzezenie"])


class TestD_SlowaEARS(unittest.TestCase):
    def bledy(self, wzorzec, tresc):
        w, _, _ = wym("| R-X-01 | %s | %s | a → b |\n" % (wzorzec, tresc), strict=True)
        return [x["tresc"] for x in wpisy(w, "d", "blad")]

    def test_zgodne(self):
        self.assertEqual(self.bledy("Event", "WHEN a the Panel shall b."), [])
        self.assertEqual(self.bledy("State", "WHILE a the Panel shall b."), [])
        self.assertEqual(self.bledy("IF/THEN", "IF a, THEN the Panel shall b."), [])
        self.assertEqual(self.bledy("Optional", "WHERE a the Panel shall b."), [])
        self.assertEqual(self.bledy("Ubiquitous", "The Panel shall b."), [])
        self.assertEqual(self.bledy("Optional + Event", "WHERE a, WHEN c, the Panel shall b."), [])

    def test_brak_slowa(self):
        self.assertEqual(len(self.bledy("Event", "The Panel shall b, gdy a.")), 1)
        self.assertIn("brak THEN", self.bledy("IF/THEN", "IF a the Panel shall b.")[0])

    def test_nadmiarowe_slowo(self):
        self.assertIn("nadmiarowe WHEN", self.bledy("Ubiquitous", "WHEN a the Panel shall b.")[0])

    def test_nieznany_wzorzec(self):
        self.assertIn("nieznany wzorzec", self.bledy("Complex", "WHEN a the Panel shall b.")[0])


class TestKomorkiZeZnakiemPionowym(unittest.TestCase):
    def test_escaped_pipe_nie_dzieli_kolumn(self):
        self.assertEqual(s._komorki("| a | x \\| y \\| z | c |"), ["a", "x \\| y \\| z", "c"])

    def test_weryfikacja_po_komorce_z_pipe(self):
        w, _, _ = wym("| R-X-01 | Ubiquitous | The Panel shall liczyć \\|a\\| / Δ. | Wejście → wynik 1. | źródło |\n",
                      strict=True)
        self.assertEqual(wpisy(w, "d"), [])


class TestMacierzWierszeZbiorcze(unittest.TestCase):
    KAT = """# Spec

| ID | Rodzaj |
|---|---|
| S-K-01 | a |
| S-K-02 | b |
| S-K-03 | c |

| ID | Wzorzec | Wymaganie | Weryfikacja |
|---|---|---|---|
| R-X-01 | Event | WHEN a the Panel shall pokazać S-K-02. | a → b |
| R-X-02 | Event | WHEN a the Panel shall b. | a → b |

| Ekran | Nazwa | Wymagania |
|---|---|---|
| S-K-01…S-K-02 | grupa | R-X-01, R-X-02 |
| S-K-03 | jeden | R-X-02 |

| Ekran albo grupa | Wymagania | Przypadki testowe |
|---|---|---|
| S-K-01 | R-X-02 | brak |
"""

    def test_wiersz_zbiorczy_tylko_z_wymienionym_ekranem(self):
        w, st, m = s.sprawdz(self.KAT, OrderedDict(), None, strict=True)
        self.assertIn(("R-X-01", "S-K-02"), m)
        self.assertNotIn(("R-X-01", "S-K-01"), m)
        self.assertNotIn(("R-X-02", "S-K-02"), m)
        self.assertEqual(st["zrodla_powiazan"]["R-X-01|S-K-02"], "katalog-zbiorczy+wymaganie")

    def test_wiersz_pojedynczy_daje_katalog(self):
        _, st, m = s.sprawdz(self.KAT, OrderedDict(), None)
        self.assertIn(("R-X-02", "S-K-03"), m)
        self.assertEqual(st["zrodla_powiazan"]["R-X-02|S-K-03"], "katalog")

    def test_tabela_skrocona_nie_tworzy_powiazan(self):
        w, _, m = s.sprawdz(self.KAT, OrderedDict(), None, strict=True)
        self.assertNotIn(("R-X-02", "S-K-01"), m)
        self.assertEqual([x["id"] for x in wpisy(w, "e", "blad")], ["S-K-01"])


class TestSpecBiezaca(unittest.TestCase):
    """Bieżąca spec przechodzi nowe sprawdzenia d i e w trybie --strict."""

    def test_spec_bez_bledow_d_i_e(self):
        with open(SPEC, encoding="utf-8") as f:
            tekst = f.read()
        w, _, _ = s.sprawdz(tekst, s.wczytaj_wycofane(WYCOFANE), None, strict=True)
        self.assertEqual([x["tresc"] for x in wpisy(w, "d", "blad") + wpisy(w, "e", "blad")], [])


if __name__ == "__main__":
    unittest.main()


class TestKolumnaS2dTylkoWSekcji576(unittest.TestCase):
    """: kolumna `Tytuł (S2d)` dostaje limit 90/100 tylko w tabelach sekcji 5.7.6."""

    def test_rodzaj_kolumny_zalezy_od_sekcji(self):
        self.assertEqual(s._rodzaj_kolumny("Tytuł (S2d)", "5.7.6 Przerywniki (S2d)"), "tytuł s2d")
        self.assertEqual(s._rodzaj_kolumny("Powód (S2d)", "5.7.6 Przerywniki (S2d)"), "powód s2d")
        self.assertIsNone(s._rodzaj_kolumny("Tytuł (S2d)", "5.4.2 Inna sekcja"))
        self.assertIsNone(s._rodzaj_kolumny("Powód (S2d)"))
