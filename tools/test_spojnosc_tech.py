# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy skryptu spójności specyfikacji technicznej: python3 -m pytest tools/test_spojnosc_tech.py"""

import csv
import io
import os
import sys
import tempfile
import unittest
from collections import OrderedDict
from contextlib import redirect_stdout

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import spojnosc_tech as t  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SPEC = """# Spec

## 5.1 DANE

| ID | Wzorzec | Wymaganie | Weryfikacja | Źródło |
|---|---|---|---|---|
| R-DANE-01 | Ubiquitous | The Panel shall a. | x → y | nowe |
| R-DANE-02 | Ubiquitous | The Panel shall b. | x → y | nowe |
| R-DANE-03 | Ubiquitous | The Panel shall c. | x → y | nowe |
| R-DANE-04 | Ubiquitous | The Panel shall d. | x → y | nowe |
"""

GLOWA = """# Tech

| | |
|---|---|
| Odwołanie | specyfikacja funkcjonalna na `main`, commit `468480d` |

## Stan sekcji

| Sekcja | Stan |
|---|---|
| 2. Jakość | szkic |
| 6. Widok bloków | szkic |
| 9. Koncepcje | szkic |
| 11. Testy | szkic |
"""

DOBRY = GLOWA + """
## 2. Jakość

| Nr | Co mierzymy | Jak |
|---|---|---|
| 4 | odświeżenie e-ink | znacznik w logu |
| 5 | czasy reakcji | log portu serwisowego |

## 6. Widok bloków

| ID | Komponent | Realizuje |
|---|---|---|
| T-KOMP-01 | pobieranie | R-DANE-01…R-DANE-02, T-WYM-01 |
| T-KOMP-02 | zapis | R-DANE-03 |

### 6.9 Wymagania bez komponentu

| Wymaganie | Powód |
|---|---|
| R-DANE-04 | zablokowane do pomiaru 4 |

## 9. Koncepcje

| ID | Wzorzec | Wymaganie | Weryfikacja |
|---|---|---|---|
| T-WYM-01 | Event | WHEN plik ma inną wersję the Panel shall migrować. | plik v1 → plik v2 |

Czas odświeżenia: [do zmierzenia] (pomiar 4).

## 11. Testy

| ID | Poziom | Sprawdza |
|---|---|---|
| T-TEST-001 | PC | R-DANE-01, 02 |
| T-TEST-002 | PC | R-DANE-03, T-WYM-01 |
"""


def uruchom(tech, spec=SPEC, wycofane=None, koniec=False):
    return t.sprawdz(tech, t.definicje_spec(spec), wycofane or OrderedDict(), koniec)


def bledy(wynik, spr=None):
    return [w for w in wynik.wpisy if w["poziom"] == "blad" and (spr is None or w["sprawdzenie"] == spr)]


class TestSchemat(unittest.TestCase):
    def test_rozpoznaje_tylko_poprawne(self):
        self.assertEqual(t.T_RE.findall("T-KOMP-01 T-TEST-001 T-NF-12 T-SOUP-03"),
                         ["T-KOMP-01", "T-TEST-001", "T-NF-12", "T-SOUP-03"])
        self.assertEqual(t.T_RE.findall("T-TEST-01 T-KOMP-001 T-FOO-01 XT-KOMP-01 T-KOMP-01a"), [])

    def test_nie_koliduje_z_rodzinami_spec(self):
        # R-NF-01 i S-K-01 nie są ID T-, a T-NF-01 nie jest ID R.
        self.assertEqual(t.T_RE.findall("R-NF-01 S-K-01"), [])
        self.assertEqual(t.ids_r("T-NF-01 T-DANE-02"), [])


class TestDobry(unittest.TestCase):
    def test_zero_bledow_i_pelne_pokrycie(self):
        wynik, stat, macierz = uruchom(DOBRY, koniec=True)
        self.assertEqual(bledy(wynik), [], bledy(wynik))
        self.assertEqual(stat["pokryte"], 5)  # 4 × R + T-WYM-01
        self.assertEqual(stat["wymagania"], 5)
        wiersze = {m[0]: m for m in macierz}
        self.assertEqual(wiersze["R-DANE-02"][2:4], ("T-KOMP-01", "T-TEST-001"))  # zakres i skrót
        self.assertEqual(wiersze["R-DANE-04"][4], "zablokowane do pomiaru 4")
        self.assertEqual(wiersze["T-WYM-01"][2:4], ("T-KOMP-01", "T-TEST-002"))


class TestBledy(unittest.TestCase):
    def test_a_podwojna_definicja(self):
        tech = DOBRY + "\n| ID | Komponent | Realizuje |\n|---|---|---|\n| T-KOMP-01 | znowu | R-DANE-01 |\n"
        self.assertEqual(len(bledy(uruchom(tech)[0], "a")), 1)

    def test_b_bez_definicji_i_zly_format(self):
        w = uruchom(DOBRY + "\nZob. T-KOMP-09 i T-TEST-01.\n")[0]
        tresci = " ".join(x["tresc"] for x in bledy(w, "b"))
        self.assertIn("T-KOMP-09", tresci)
        self.assertIn("T-TEST-01", tresci)

    def test_c_wycofane(self):
        wyc = OrderedDict([("T-KOMP-02", {"status": "wycofane", "nastepca": "T-KOMP-01", "zrodlo": "test"})])
        w = uruchom(DOBRY, wycofane=wyc)[0]
        self.assertTrue(bledy(w, "c"))
        # w sekcji „Wycofane” wolno
        tech = DOBRY.replace("| T-KOMP-02 | zapis | R-DANE-03 |\n", "") + \
            "\n## Wycofane ID\n\nT-KOMP-02 zastąpione przez T-KOMP-01.\n"
        self.assertEqual(bledy(uruchom(tech, wycofane=wyc)[0], "c"), [])

    def test_d_nieistniejace_r_i_pusta_kolumna(self):
        tech = DOBRY.replace("| T-KOMP-02 | zapis | R-DANE-03 |", "| T-KOMP-02 | zapis | R-DANE-77 |")
        self.assertIn("R-DANE-77", " ".join(x["tresc"] for x in bledy(uruchom(tech)[0], "d")))
        tech = DOBRY.replace("| T-TEST-002 | PC | R-DANE-03, T-WYM-01 |", "| T-TEST-002 | PC |  |")
        self.assertTrue(bledy(uruchom(tech)[0], "d"))

    def test_d_wycofane_r(self):
        wyc = OrderedDict([("R-DANE-03", {"status": "wycofane", "nastepca": "R-DANE-01", "zrodlo": "test"})])
        self.assertIn("wycofanego wymagania R-DANE-03", " ".join(x["tresc"] for x in bledy(uruchom(DOBRY, wycofane=wyc)[0], "d")))

    def test_e_ears_i_weryfikacja(self):
        tech = DOBRY.replace("| Event | WHEN plik", "| State | WHEN plik")
        self.assertTrue(bledy(uruchom(tech)[0], "d"))  # sprawdz_ears z spojnosc.py zgłasza jako „d”
        tech = DOBRY.replace("| plik v1 → plik v2 |", "| plik v1 i plik v2 |")
        self.assertTrue(bledy(uruchom(tech)[0], "d"))

    def test_f_do_zmierzenia(self):
        w = uruchom(DOBRY.replace("[do zmierzenia] (pomiar 4)", "[do zmierzenia]"))[0]
        self.assertEqual(len(bledy(w, "f")), 1)
        w = uruchom(DOBRY.replace("[do zmierzenia] (pomiar 4)", "[do zmierzenia] (pomiar 99)"))[0]
        self.assertIn("Pomiar 99", bledy(w, "f")[0]["tresc"])
        # wzmianka w odwrotnych apostrofach nie jest znacznikiem
        w = uruchom(DOBRY + "\nKonwencja: `[do zmierzenia]` z numerem pomiaru.\n")[0]
        self.assertEqual(bledy(w, "f"), [])

    def test_f_kilka_numerow_i_bez_tabeli_pomiarow(self):
        self.assertEqual(bledy(uruchom(DOBRY + "\nProfil: [do zmierzenia] (pomiary 4 i 5).\n")[0], "f"), [])
        self.assertTrue(bledy(uruchom(DOBRY + "\nProfil: [do zmierzenia] (pomiary 4 i 7).\n")[0], "f"))
        # bez tabeli „Pomiary” numer nie jest sprawdzany, ale opis pomiaru nadal jest wymagany
        bez_tabeli = DOBRY.replace("| Nr | Co mierzymy | Jak |", "| Nr | Temat | Jak |")
        self.assertEqual(bledy(uruchom(bez_tabeli.replace("(pomiar 4)", "(pomiar 99)"))[0], "f"), [])
        self.assertTrue(bledy(uruchom(bez_tabeli.replace("(pomiar 4)", ""))[0], "f"))

    def test_g_stan_sekcji(self):
        self.assertTrue(bledy(uruchom(DOBRY.replace("| szkic |", "| prawie |"))[0], "g"))
        tech = DOBRY.replace("| 6. Widok bloków | szkic |", "| 6. Widok bloków | do napisania |")
        self.assertEqual(bledy(uruchom(tech)[0], "g"), [])
        self.assertTrue(bledy(uruchom(tech, koniec=True)[0], "g"))

    def test_g_kompletnosc_i_duplikat(self):
        tech = DOBRY.replace("| 11. Testy | szkic |\n", "")
        self.assertIn("Sekcja 11.", bledy(uruchom(tech)[0], "g")[0]["tresc"])
        tech = DOBRY.replace("| 11. Testy | szkic |\n", "| 11. Testy | szkic |\n| 11. Testy | szkic |\n")
        self.assertIn("dwa razy", bledy(uruchom(tech)[0], "g")[0]["tresc"])
        tech = DOBRY.replace("| 11. Testy | szkic |\n", "| 11. Testy | szkic |\n| 12. Wydania | szkic |\n")
        self.assertTrue(bledy(uruchom(tech)[0], "g"))

    def test_h_tylko_wiersz_odwolanie_i_git(self):
        tech = DOBRY.replace("| Odwołanie | specyfikacja funkcjonalna na `main`, commit `468480d` |",
                             "| Uwagi | nic |") + "\nSpecyfikacja funkcjonalna `deadbee` w treści.\n"
        self.assertTrue(bledy(uruchom(tech)[0], "h"))
        w = t.sprawdz(DOBRY, t.definicje_spec(SPEC), OrderedDict(), ref_istnieje=lambda r: False)[0]
        self.assertEqual([x["poziom"] for x in w.wpisy if x["sprawdzenie"] == "h"], ["ostrzezenie"])

    def test_h_wersja_spec(self):
        tech = DOBRY.replace("commit `468480d`", "commit z dziś")
        self.assertTrue(bledy(uruchom(tech)[0], "h"))

    def test_i_pokrycie(self):
        tech = DOBRY.replace("| T-TEST-002 | PC | R-DANE-03, T-WYM-01 |", "| T-TEST-002 | PC | T-WYM-01 |")
        w, stat, _ = uruchom(tech)
        self.assertEqual(bledy(w, "i"), [])  # bez --koniec-etapu tylko informacja
        self.assertEqual(stat["pokryte"], 4)
        w = uruchom(tech, koniec=True)[0]
        self.assertEqual([x["id"] for x in bledy(w, "i")], ["R-DANE-03"])

    def test_i_bez_powodu(self):
        w = uruchom(DOBRY.replace("| R-DANE-04 | zablokowane do pomiaru 4 |", "| R-DANE-04 |  |"))[0]
        self.assertTrue(bledy(w, "i"))


class TestPrzypadkiBrzegowe(unittest.TestCase):
    """Przypadki brzegowe sprawdzeń (a) do (j)."""

    def test_r_poza_kolumnami(self):
        tech = DOBRY + "\n| ID | Scenariusz | R-NF |\n|---|---|---|\n| T-NF-01 | energia | R-NF-99 |\n\nZob. R-XYZ-01.\n"
        tresci = " ".join(x["tresc"] for x in bledy(uruchom(tech)[0], "d"))
        self.assertIn("R-NF-99", tresci)
        self.assertIn("R-XYZ-01", tresci)

    def test_wycofane_r_w_sekcji_wycofane_wolno(self):
        wyc = OrderedDict([("R-DANE-03", {"status": "wycofane", "nastepca": "R-DANE-01", "zrodlo": "t"})])
        tech = DOBRY.replace("R-DANE-03", "R-DANE-01") + "\n## Wycofane\n\nR-DANE-03 zastąpione.\n"
        self.assertEqual(bledy(uruchom(tech, wycofane=wyc)[0], "d"), [])

    def test_furtka_brak_z_powodem(self):
        tech = DOBRY.replace("| T-KOMP-02 | zapis | R-DANE-03 |", "| T-KOMP-02 | zapis | do ustalenia, brakuje danych |")
        self.assertTrue(bledy(uruchom(tech)[0], "d"))
        tech = DOBRY.replace("| T-KOMP-02 | zapis | R-DANE-03 |", "| T-KOMP-02 | zapis | patrz niżej |")
        self.assertTrue(bledy(uruchom(tech)[0], "d"))
        tech = DOBRY.replace("| T-KOMP-02 | zapis | R-DANE-03 |", "| T-KOMP-02 | zapis | brak (konfiguracja) |")
        self.assertEqual(bledy(uruchom(tech)[0], "d"), [])

    def test_sekcja_bez_komponentu_po_tytule(self):
        # tytuł z podciągiem nie zwalnia z pokrycia
        tech = DOBRY.replace("### 6.9 Wymagania bez komponentu", "### 13.2 Rozjazdy: elementy bez komponentu")
        w, stat, _ = uruchom(tech)
        self.assertEqual(stat["pokryte"], 4)
        # podsekcja pod właściwym nagłówkiem działa
        tech = DOBRY.replace("### 6.9 Wymagania bez komponentu\n", "### 6.9 Wymagania bez komponentu\n\n#### 6.9.1 Lista\n")
        self.assertEqual(uruchom(tech)[1]["pokryte"], 5)

    def test_definicje_w_wycofanych(self):
        wyc = OrderedDict([("T-KOMP-09", {"status": "wycofane", "nastepca": "T-KOMP-01", "zrodlo": "t"})])
        tech = DOBRY + "\n## Wycofane\n\n| ID | Następca |\n|---|---|\n| T-KOMP-09 | T-KOMP-01 |\n"
        self.assertEqual(bledy(uruchom(tech, wycofane=wyc)[0]), [])

    def test_zmienione_musi_miec_definicje(self):
        wyc = OrderedDict([("T-KOMP-09", {"status": "zmienione", "nastepca": "x", "zrodlo": "t"})])
        self.assertTrue(bledy(uruchom(DOBRY + "\nZob. T-KOMP-09.\n", wycofane=wyc)[0], "b"))

    def test_test_bez_komponentu_nie_ostrzega(self):
        # wymaganie z testem, ale bez komponentu, stoi w „Wymagania bez komponentu” zgodnie z tabelą
        tech = DOBRY.replace("| T-TEST-002 | PC | R-DANE-03, T-WYM-01 |", "| T-TEST-002 | PC | R-DANE-03, R-DANE-04, T-WYM-01 |")
        w = uruchom(tech)[0]
        self.assertEqual([x for x in w.wpisy if x["poziom"] == "ostrzezenie" and x["sprawdzenie"] == "i"], [])

    def test_definicja_tylko_w_tabeli_id(self):
        tech = DOBRY.replace("| ID | Poziom | Sprawdza |", "| Nr | Poziom | Sprawdza |")
        self.assertTrue(bledy(uruchom(tech)[0], "b"))

    def test_realizuje_bez_t_nf_i_sprzecznosc(self):
        tech = DOBRY + "\n| ID | Scenariusz |\n|---|---|\n| T-NF-01 | energia |\n"
        tech = tech.replace("| T-KOMP-02 | zapis | R-DANE-03 |", "| T-KOMP-02 | zapis | R-DANE-03, T-NF-01 |")
        self.assertTrue(bledy(uruchom(tech)[0], "d"))
        tech = DOBRY.replace("| T-KOMP-02 | zapis | R-DANE-03 |", "| T-KOMP-02 | zapis | R-DANE-03, R-DANE-04 |")
        w = uruchom(tech)[0]
        self.assertTrue([x for x in w.wpisy if x["poziom"] == "ostrzezenie" and x["sprawdzenie"] == "i"])

    def test_znaczniki_wielkosc_liter_i_liczba(self):
        self.assertTrue(bledy(uruchom(DOBRY + "\n[Do zmierzenia] bez opisu.\n")[0], "f"))
        self.assertTrue(bledy(uruchom(DOBRY + "\n[do zmierzenia] i [do zmierzenia] (pomiar 4).\n")[0], "f"))

    def test_dlugi_zakres_ostrzega(self):
        w = uruchom(DOBRY + "\nR-DANE-01…R-DANE-79\n")[0]
        self.assertTrue([x for x in w.wpisy if x["poziom"] == "ostrzezenie" and "60" in x["tresc"]])

    def test_kod_wyjscia_2(self):
        with redirect_stdout(io.StringIO()), contextlib_stderr():
            self.assertEqual(t.main(["/nie/ma/takiego.md"]), 2)
        with tempfile.NamedTemporaryFile("wb", suffix=".md", delete=False) as f:
            f.write(b"\xff\xfe zly")
        try:
            with redirect_stdout(io.StringIO()), contextlib_stderr():
                self.assertEqual(t.main([f.name]), 2)
        finally:
            os.unlink(f.name)


KOD = {
    "src/main.cpp": "static const uint32_t CZUWANIE_MS = 20000;\nstatic void spij() {\n  esp_deep_sleep_start();\n}\n"
                    "void setup() {\n  spij();\n}\n",
    "src/noc.h": "#pragma once\n// format pliku\nbool parsujNoc(const std::string& l, Noc& r);\n",
}


def uruchom_kod(linia):
    tech = DOBRY + "\n" + linia + "\n"
    return t.sprawdz(tech, t.definicje_spec(SPEC), OrderedDict(), czytaj_kod=KOD.get)[0]


class TestKod(unittest.TestCase):
    """(j) Odwołania do kodu po nazwie funkcji (przegląd architektury 01.11.2026, praktyk W4)."""

    def test_dobre_odwolania(self):
        for linia in ("Sen: `src/main.cpp` `spij()`.",
                      "Start: `src/main.cpp` `setup()`, `spij()` i `CZUWANIE_MS`.",
                      "Format: `src/noc.h:2@c518470`, `parsujNoc()`.",
                      "Deklaracja w nagłówku: `src/noc.h` `parsujNoc()`."):
            with self.subTest(linia=linia):
                self.assertEqual(bledy(uruchom_kod(linia), "j"), [])

    def test_brak_funkcji_w_pliku(self):
        b = bledy(uruchom_kod("Sen: `src/main.cpp` `spij()`, `uspij()`."), "j")
        self.assertEqual(len(b), 1)
        self.assertIn("uspij()", b[0]["tresc"])
        # wywołanie wewnątrz funkcji to nie definicja
        self.assertTrue(bledy(uruchom_kod("`src/main.cpp` `esp_deep_sleep_start()`"), "j"))

    def test_brak_nazwy_i_pliku(self):
        self.assertTrue(bledy(uruchom_kod("`src/main.cpp` `CZUWANIE_S`"), "j"))
        self.assertIn("nie istnieje", bledy(uruchom_kod("`src/brak.cpp` `spij()`"), "j")[0]["tresc"])

    def test_linia_bez_commitu_to_blad(self):
        b = bledy(uruchom_kod("Sen: `src/main.cpp:1631`."), "j")
        self.assertEqual(len(b), 1)
        self.assertIn("bez commitu", b[0]["tresc"])
        self.assertTrue(bledy(uruchom_kod("Zakres: `src/main.cpp:1631-1650`."), "j"))
        self.assertEqual(bledy(uruchom_kod("Zakres: `src/main.cpp:1631-1650@c518470`."), "j"), [])

    def test_szesc_form(self):
        """Formy zapisu, których pierwsza wersja sprawdzenia nie łapała."""
        commity = {("c518470", "src/main.cpp"): KOD["src/main.cpp"]}

        def czytaj_commit(h, plik):
            return commity.get((h, plik))

        def j(linia):
            tech = DOBRY + "\n" + linia + "\n"
            w = t.sprawdz(tech, t.definicje_spec(SPEC), OrderedDict(), czytaj_kod=KOD.get,
                          czytaj_commit=czytaj_commit)[0]
            return bledy(w, "j")
        for zla in ("Nawias: `src/main.cpp` (`nieMa()`).",
                    "Dwukropek: `src/main.cpp`: `nieMa()`.",
                    "Klasa: `src/main.cpp` `Klasa::nieMa()`.",
                    "Bez apostrofów: src/main.cpp:406.",
                    "Pełna ścieżka: `firmware/prototyp/src/main.cpp:408`.",
                    "Commit, którego nie ma: `src/main.cpp:4@deadbee`.",
                    "Linia poza plikiem: `src/main.cpp:400@c518470`."):
            with self.subTest(zla=zla):
                self.assertTrue(j(zla), zla)
        for dobra in ("Nawias: `src/main.cpp` (`spij()`).", "Dwukropek: `src/main.cpp`: `setup()`.",
                      "Commit: `src/main.cpp:2-3@c518470`.", "`src/main.cpp` `spij()` (`esp_deep_sleep_start`)"):
            with self.subTest(dobra=dobra):
                self.assertEqual(j(dobra), [], dobra)

    def test_nazwa_kwalifikowana(self):
        kod = "class Ekran {\n public:\n  void rysuj(int x);\n};\nvoid Ekran::czysc() {\n}\n"
        self.assertTrue(t.ma_definicje(kod, "Ekran::rysuj"))
        self.assertTrue(t.ma_definicje(kod, "Ekran::czysc"))
        self.assertFalse(t.ma_definicje(kod, "Ekran::brak"))
        self.assertFalse(t.ma_definicje(kod, "Inna::rysuj"))

    def test_bez_czytnika_bez_sprawdzenia(self):
        w = t.sprawdz(DOBRY + "\n`src/main.cpp:12` `nic()`\n", t.definicje_spec(SPEC), OrderedDict())[0]
        self.assertEqual(bledy(w, "j"), [])

    def test_repo_odwolania_istnieja(self):
        czytaj = t.czytnik_kodu(os.path.join(REPO, "firmware/prototyp"))
        self.assertIsNotNone(czytaj("src/main.cpp"))
        self.assertTrue(t.ma_definicje(czytaj("src/main.cpp"), "setup"))  # spij() jest w src/dyrygent.cpp
        self.assertTrue(t.ma_definicje(czytaj("src/dyrygent.cpp"), "Przebieg::spij"))


def contextlib_stderr():
    from contextlib import redirect_stderr
    return redirect_stderr(io.StringIO())


class TestRepo(unittest.TestCase):
    """Prawdziwe pliki repo: skrypt przechodzi, a macierz na dysku jest aktualna."""

    def test_repo_bez_bledow_i_macierz_aktualna(self):
        with tempfile.TemporaryDirectory() as d:
            sciezka = os.path.join(d, "m.csv")
            with redirect_stdout(io.StringIO()):
                kod = t.main(["--macierz", sciezka])
            self.assertEqual(kod, 0)
            with open(sciezka, encoding="utf-8") as f:
                nowa = f.read()
        with open(os.path.join(REPO, "docs/spec/macierz-techniczna.csv"), encoding="utf-8") as f:
            self.assertEqual(f.read(), nowa, "macierz-techniczna.csv nieaktualna: wygeneruj skryptem")
        wiersze = list(csv.reader(io.StringIO(nowa)))
        self.assertEqual(wiersze[0], ["wymaganie", "sekcja_spec", "komponenty", "testy", "powod_braku"])
        self.assertGreater(len(wiersze), 400)


if __name__ == "__main__":
    unittest.main()
