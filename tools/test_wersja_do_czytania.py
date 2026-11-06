# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy wersji do czytania: python3 -m unittest tools/test_wersja_do_czytania.py"""
import os
import re
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import wersja_do_czytania as w  # noqa: E402

SPEC = os.path.join(os.path.dirname(__file__), "..", "docs", "spec", "specyfikacja-funkcjonalna.md")
DEF_RE = re.compile(r"^\| (R-[A-Z]+-\d+b?) \| (?:Event|State|Ubiquitous|IF/THEN|Optional)", re.M)


class TestMale(unittest.TestCase):
    def test_kolumna_zrodla(self):
        t = "| ID | Wymaganie | Źródło |\n|---|---|---|\n| R-X-01 | tekst `a | b` | audyt A-1 |\n"
        wynik, r = w.przetworz(t)
        self.assertIn("| R-X-01 | tekst `a | b` |", wynik)
        self.assertNotIn("| Źródło |", wynik)
        self.assertEqual(r["tabele"], 1)

    def test_kolumna_prog_zostaje(self):
        t = "| ID | Próg: źródło i poziom dowodu |\n|---|---|\n| E-01 | ATS 2013, W |\n"
        wynik, _ = w.przetworz(t)
        self.assertIn("ATS 2013, W", wynik)

    def test_nawiasy(self):
        wynik, _ = w.przetworz("Tekst (audyt A-57). Inny (R-EKR-42; audyt A-48). Treść (audyt A-76: bez pary).")
        self.assertIn("Tekst.", wynik)
        self.assertIn("Inny (R-EKR-42).", wynik)
        self.assertIn("(audyt A-76: bez pary)", wynik)

    def test_kod_i_poczatek_wiersza(self):
        wynik, _ = w.przetworz("Tekst `Ekran (audyt A-1) koniec` dalej.\n(recenzja 1.1) Początek.")
        self.assertIn("`Ekran (audyt A-1) koniec`", wynik)
        self.assertIn("\nPoczątek.", wynik)

    def test_status_blokady(self):
        t = "| ID | Wymaganie | Źródło |\n|---|---|---|\n| R-GI-01 | tekst | v0.6c; status zablokowane do pomiaru 21 |\n"
        wynik, _ = w.przetworz(t)
        self.assertIn("tekst (status: zablokowane do pomiaru 21)", wynik)

    def test_zrodlo_dowodu_zostaje(self):
        # źródło dowodu progu z usuwanej kolumny „Źródło” przechodzi do treści
        t = "| ID | Wymaganie | Źródło |\n|---|---|---|\n| R-X-01 | tekst | nowe, audyt A-1; próg: ATS 2013, W |\n"
        wynik, _ = w.przetworz(t)
        self.assertIn("tekst (źródło: ATS 2013, W)", wynik)

    def test_id_z_usuwanego_nawiasu_zostaje(self):
        # „(audyt A-46, M-08 zmienione)” traci historię, nie ID
        wynik, _ = w.przetworz("Tekst (audyt A-46, M-08 zmienione; R-HUM-27).")
        self.assertIn("M-08", wynik)
        self.assertIn("R-HUM-27", wynik)
        self.assertNotIn("audyt A-46", wynik)

    def test_decyzja_wlasciciela_jako_historia(self):
        # decyzja z datą to historia, ID zostaje
        wynik, _ = w.przetworz("Tekst (decyzja właściciela projektu 30.10.2026). Inny (audyt UX 30.10.2026; R-EKR-52).")
        self.assertIn("Tekst.", wynik)
        self.assertIn("Inny (R-EKR-52).", wynik)

    def test_pierwsza_kolumna_inna_niz_zrodlo_zostaje(self):
        t = "| Źródło danych | Brak |\n|---|---|\n| AirBridge: lista nocy | E-01 |\n"
        wynik, _ = w.przetworz(t)
        self.assertIn("AirBridge: lista nocy", wynik)

    def test_niespojna_tabela_bez_zmian(self):
        t = "| ID | Źródło |\n|---|---|\n| R-X-01 | a | b |\n"
        wynik, r = w.przetworz(t)
        self.assertIn("| R-X-01 | a | b |", wynik)
        self.assertEqual(len(r["ostrzezenia"]), 1)


class TestSpecyfikacja(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.zrodlo = open(SPEC, encoding="utf-8").read()
        cls.wynik, cls.raport = w.przetworz(cls.zrodlo)

    def test_te_same_wymagania(self):
        przed = sorted(DEF_RE.findall(self.zrodlo))
        po = sorted(DEF_RE.findall(self.wynik))
        self.assertEqual(przed, po)
        self.assertGreater(len(przed), 400)

    def test_bez_historii(self):
        self.assertNotIn("### 10.3 Historia wersji", self.wynik)
        self.assertNotIn("| Źródło |", self.wynik)
        self.assertIn("## Załącznik A", self.wynik)
        self.assertIn("### 10.1 Otwarte pytania", self.wynik)

    def test_te_same_id_poza_historia(self):
        # zbiór wszystkich ID poza 10.2 i 10.3 przed i po jest równy
        import spojnosc as s
        def ids(t):
            d = s.Dokument(t)
            return {i for n, l in enumerate(d.linie) if not d.sekcje[n].startswith(("10.2", "10.3"))
                    and "Wycofane" not in d.sekcje[n] for i, _ in s.znajdz_id(l)}
        self.assertEqual(ids(self.zrodlo) - ids(self.wynik), set())

    def test_zrodla_dowodu_i_etykiety(self):
        self.assertIn("ATS 2013", self.wynik)
        self.assertIn("AirBridge: lista nocy i raport", self.wynik)

    def test_bez_ostrzezen(self):
        self.assertEqual(self.raport["ostrzezenia"], [])

    def test_tabele_spojne(self):
        linie = self.wynik.split("\n")
        i = 0
        while i < len(linie):
            if w.czy_wiersz_tabeli(linie[i]):
                n = len(w.podziel_wiersz(linie[i]))
                j = i
                while j < len(linie) and w.czy_wiersz_tabeli(linie[j]):
                    self.assertEqual(len(w.podziel_wiersz(linie[j])), n, "l. %d" % (j + 1))
                    j += 1
                i = j
            else:
                i += 1


if __name__ == "__main__":
    unittest.main()
