# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy loadera nocy z karty SD na syntetycznej karcie ze znanymi odpowiedziami.

Uruchomienie:
  python3 -m unittest discover -s tools/kalibracja -p 'test_*.py'

Syntetyczna karta powstaje w katalogu tymczasowym; w repozytorium nie ma żadnych
plików nocy (WY12).
"""

import datetime as dt
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

_TU = os.path.dirname(os.path.abspath(__file__))
if _TU not in sys.path:
    sys.path.insert(0, _TU)

import edf  # noqa: E402
import noce  # noqa: E402
import syntetyczna  # noqa: E402

REPO = os.path.dirname(os.path.dirname(_TU))


def _stan_katalogu(k):
    out = {}
    for d, _, pliki in os.walk(k):
        for p in pliki:
            s = os.stat(os.path.join(d, p))
            out[os.path.join(d, p)] = (s.st_size, s.st_mtime_ns)
    return out


class KartaSyntetyczna(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="karta-syn-")
        cls.karta = os.path.join(cls.tmp, "karta")
        syntetyczna.utworz(cls.karta)
        cls.par = noce.Parametry()
        cls.lista = noce.noce(cls.karta, cls.par)
        cls.n = {r["data_poranka"]: r for r in cls.lista}

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def m(self, data):
        return self.n[data]["metryki"]

    # --- N1: pełna noc ze znanymi zdarzeniami -------------------------------
    def test_n1_pola_raportu(self):
        r = self.n["2026-01-11"]
        self.assertEqual(r["day"], noce.dzien_z_daty(dt.date(2026, 1, 10)))
        self.assertTrue(r["present"])
        self.assertFalse(r["partial"])
        self.assertTrue(r["events_known"])
        self.assertEqual(r["used_seconds"], 7 * 3600)
        # OA o 06:00 (koniec sesji, przedział półotwarty) i 06:30 (poza sesją) się nie liczą
        self.assertEqual(r["counts"], {"oa": 3, "ca": 7, "ua": 0, "h": 4, "rera": 2})
        self.assertAlmostEqual(r["ahi"], 14 / 7.0)
        self.assertAlmostEqual(r["rdi"], 16 / 7.0)
        # CSR: 10 min + 5 min + 15 min przycięte do końca:00, czyli 5 min
        self.assertAlmostEqual(r["csr_seconds"], 20 * 60)
        self.assertEqual(r["csr_zrodlo"], "CSL")
        self.assertAlmostEqual(r["leak_high_seconds"], 30 * 60)
        self.assertEqual(r["leak_27_seconds"], 30 * 60)  # 30 L/min > 27 przez 30 min (E-23)
        self.assertEqual(r["session_count"], 1)
        self.assertEqual(r["adnotacje_inne"], {"Recording starts": 1})

    def test_n1_kanaly(self):
        ch = self.n["2026-01-11"]["channels"]
        self.assertAlmostEqual(ch["Press"]["median"], 12.0)
        self.assertAlmostEqual(ch["Press"]["p95"], 15.0)
        self.assertEqual(ch["Press"]["source"], "edf")
        self.assertAlmostEqual(ch["EprPress"]["median"], 8.0)
        self.assertAlmostEqual(ch["Leak"]["median"], 6.0)       # 0,1 L/s → L/min
        self.assertAlmostEqual(ch["Leak"]["p95"], 30.0)         # 30 z 420 min > 5%
        self.assertAlmostEqual(ch["TidVol"]["median"], 460.0)   # 0,46 L → mL
        self.assertAlmostEqual(ch["MinVent"]["median"], 7.5)
        self.assertAlmostEqual(ch["RespRate"]["median"], 15.0)
        self.assertAlmostEqual(ch["FlowLim"]["p95"], 0.05)
        self.assertAlmostEqual(ch["Snore"]["p95"], 0.1)
        self.assertAlmostEqual(ch["SpO2"]["median"], 95.0)
        self.assertAlmostEqual(ch["Pulse"]["median"], 60.0)
        self.assertAlmostEqual(ch["TgtVent"]["median"], 6.0)

    def test_n1_metryki(self):
        m = self.m("2026-01-11")
        self.assertAlmostEqual(m["m01_h"], 7.0)
        self.assertAlmostEqual(m["m03_ca_h"], 1.0)
        self.assertAlmostEqual(m["m03_obturacyjne_h"], 1.0)      # (3 + 0 + 4) / 7 h
        self.assertAlmostEqual(m["m04_rera_h"], 2 / 7.0)
        self.assertAlmostEqual(m["m08_przeciek_wysoki_min"], 30.0)
        self.assertAlmostEqual(m["m08_przeciek_wysoki_proc"], 100 * 30 / 420.0)
        self.assertAlmostEqual(m["m09_csr_min"], 20.0)
        self.assertAlmostEqual(m["m09_csr_proc"], 100 * 20 / 420.0)
        self.assertEqual(m["m17_przerwy"], 0)
        self.assertEqual(m["m18_start_min"], 660)
        self.assertEqual(m["m18_start"], "23:00")
        self.assertTrue(m["wazna"])
        self.assertEqual(m["tryb"], "ASVAuto")
        self.assertTrue(m["tryb_obslugiwany"])
        self.assertEqual(m["m21_ustawienia"], "ASVAuto · EPAP 6–10 · PS 3–15")
        self.assertAlmostEqual(m["cisnienie_max"], 25.0)          # EAX + AXS (4.11)
        self.assertAlmostEqual(m["cisnienie_min"], 6.0)           # EAI
        self.assertEqual(m["maska"], "Nasal")

    def test_n1_ustawienia_kolejnosc_tagow(self):
        pola = self.n["2026-01-11"]["settings"]["fields"]
        self.assertTrue(self.n["2026-01-11"]["settings"]["available"])
        self.assertEqual([p["tag"] for p in pola[:5]], ["MOP", "EAI", "EAX", "ANS", "AXS"])
        self.assertEqual(pola[0]["value"], "ASVAuto")
        self.assertEqual(pola[0]["option"], 8)
        self.assertEqual(pola[1]["value"], "6")
        self.assertNotIn("EEP", [p["tag"] for p in pola])

    # --- sesje i dzień ------------------------------------------------------
    def test_n2_sesja_po_polnocy_w_dniu_katalogu(self):
        r = self.n["2026-01-12"]
        self.assertEqual(r["day"], noce.dzien_z_daty(dt.date(2026, 1, 11)))
        m = r["metryki"]
        self.assertEqual(m["m18_start_min"], 750)                 # 00:30 = 750 min od 12:00
        self.assertEqual(m["m18_start"], "00:30")
        self.assertAlmostEqual(m["m01_h"], 6.5)

    def test_n3_dwie_sesje(self):
        r = self.n["2026-01-13"]
        self.assertEqual(r["session_count"], 2)
        # druga sesja: plik od 00:19, pierwszy rekord bez poprawnych próbek → zapis od 00:20
        self.assertEqual(r["used_seconds"], 30 * 60 + 400 * 60)
        self.assertEqual(r["counts"]["ca"], 2)
        self.assertEqual(r["counts"]["oa"], 0)                    # zdarzenie w przerwie
        m = r["metryki"]
        self.assertEqual(m["m17_przerwy"], 1)
        self.assertEqual(m["m18_start_min"], 739)                 # start najdłuższej sesji (początek pliku)

    def test_n4_noc_krotka(self):
        m = self.m("2026-01-14")
        self.assertAlmostEqual(m["m01_h"], 2.5)
        self.assertFalse(m["wazna"])
        self.assertAlmostEqual(self.n["2026-01-14"]["ahi"], 0.4)

    def test_n5_duzy_przeciek(self):
        r = self.n["2026-01-15"]
        self.assertAlmostEqual(r["leak_high_seconds"], 6 * 3600)
        self.assertAlmostEqual(r["leak_27_seconds"], 6 * 3600)  # cała noc powyżej 27 L/min
        self.assertAlmostEqual(r["channels"]["Leak"]["median"], 32.4)
        self.assertAlmostEqual(r["channels"]["Leak"]["p95"], 39.6)
        m = r["metryki"]
        self.assertAlmostEqual(m["m08_przeciek_wysoki_proc"], 100.0)
        self.assertAlmostEqual(m["m08_27_min"], 360.0)

    def test_n6_drzemka_nie_jest_przerwa(self):
        r = self.n["2026-01-16"]
        m = r["metryki"]
        self.assertAlmostEqual(m["m01_h"], 8.5)                    # R-DANE-27: 1 h + 7 h 30
        self.assertEqual(m["m17_przerwy"], 0)
        self.assertEqual(m["m18_start"], "23:10")
        self.assertEqual(r["counts"]["h"], 1)                      # EVE z prefiksem drzemki

    def test_dzien_bez_sesji_nie_jest_na_liscie(self):
        self.assertNotIn("2026-01-17", self.n)
        kal = noce.kalendarz(self.lista, dt.date(2026, 1, 16), dt.date(2026, 1, 18))
        self.assertEqual([k["data_poranka"] for k in kal], ["2026-01-16", "2026-01-17", "2026-01-18"])
        self.assertEqual([k["na_liscie"] for k in kal], [True, False, True])

    def test_n8_tryb_spoza_wydania(self):
        m = self.m("2026-01-18")
        self.assertEqual(m["tryb"], "ASV")
        self.assertFalse(m["tryb_obslugiwany"])
        self.assertEqual(m["m21_ustawienia"], "ASV · EPAP 8 · PS 3–15")
        self.assertAlmostEqual(m["cisnienie_max"], 23.0)
        self.assertIsNone(m["cisnienie_min"])

    def test_n9_brak_eve(self):
        r = self.n["2026-01-19"]
        self.assertTrue(r["partial"])
        self.assertFalse(r["events_known"])
        self.assertIsNone(r["ahi"])
        self.assertIsNone(r["counts"]["oa"])
        self.assertIsNone(r["csr_seconds"])
        m = r["metryki"]
        self.assertFalse(m["wazna"])
        self.assertIsNone(m["m03_ca_h"])
        self.assertEqual(m["m21_ustawienia"], "ASVAuto · EPAP 6–11 · PS 3–15")

    def test_n10_bez_pld_cisnienie_ze_str(self):
        r = self.n["2026-01-20"]
        self.assertTrue(r["partial"])
        self.assertIsNone(r["used_seconds"])
        self.assertFalse(r["events_known"])
        self.assertEqual(r["session_count"], 1)
        self.assertEqual(r["channels"]["Press"]["source"], "str")
        self.assertAlmostEqual(r["channels"]["Press"]["median"], 12.5)
        self.assertAlmostEqual(r["channels"]["Press"]["p95"], 15.0)
        self.assertAlmostEqual(r["channels"]["Leak"]["median"], 6.0)
        self.assertEqual(r["metryki"]["zrodlo_cisnienia"], "str")
        self.assertIsNone(r["metryki"]["m01_h"])

    # --- wzorce nazw z prawdziwej karty --------------------------------------
    def test_n11_trzy_sesje_przesuniete_prefiksy_eve_tylko_dla_pierwszej(self):
        r = self.n["2026-01-21"]
        self.assertEqual(r["session_count"], 3)
        self.assertFalse(r["partial"], r["problemy"])
        self.assertTrue(r["events_known"])
        self.assertEqual(r["used_seconds"], (120 + 20 + 180) * 60)
        self.assertEqual(r["counts"], {"oa": 1, "ca": 1, "ua": 0, "h": 1, "rera": 0})
        self.assertEqual(r["csr_seconds"], 0)
        self.assertEqual([noce._hhmm_ms(s["start_ms"]) for s in r["sessions"]], ["17:15", "03:40", "04:01"])
        m = r["metryki"]
        self.assertEqual(m["m17_przerwy"], 1)                     #:15 przed 18:00 to nie przerwa
        self.assertEqual(m["m18_start"], "04:01")                 # najdłuższa sesja od 18:00
        self.assertTrue(m["wazna"])

    def test_n12_jedna_sesja_z_prefiksami_rozjechanymi_o_sekundy(self):
        r = self.n["2026-01-22"]
        self.assertEqual(r["session_count"], 1)
        self.assertFalse(r["partial"], r["problemy"])
        self.assertTrue(r["events_known"])
        self.assertEqual(r["used_seconds"], 360 * 60)
        self.assertEqual(r["counts"]["oa"], 1)
        self.assertEqual(r["sessions"][0]["start_ms"] // 1000 % 86400, 2 * 3600 + 52 * 60 + 57)  # start BRP
        self.assertEqual(r["metryki"]["m18_start"], "02:52")
        self.assertTrue(r["metryki"]["wazna"])

    def test_sprawdz_str(self):
        w = noce.sprawdz_str(self.karta, self.lista)
        self.assertEqual((w["noce_4h"], w["dni_str_4h"]), (9, 10))      # 19.01 bez PLD: brak used_seconds
        self.assertTrue(w["a_zgodne"])
        self.assertEqual((w["zgodne_5min"], w["dni_str_z_terapia"]), (10, 11))
        self.assertFalse(w["b_zgodne"])
        w = noce.sprawdz_str(self.karta, self.lista, dt.date(2026, 1, 11), dt.date(2026, 1, 17))
        self.assertEqual((w["zgodne_5min"], w["dni_str_z_terapia"]), (6, 6))
        self.assertTrue(w["a_zgodne"] and w["b_zgodne"])

    def test_cli_sprawdz_str(self):
        wyj = os.path.join(self.tmp, "noce-str.csv")
        p = self._cli(self.karta, "--wyjscie", wyj, "--sprawdz-str")
        self.assertEqual(p.returncode, 1)
        self.assertIn("(a): noce ≥ 4 h z DATALOG 9, dni STR z Duration ≥ 240 min 10: ZGODNE", p.stdout)
        self.assertIn("(b): czas terapii zgodny z Duration (±5 min) w 10 z 11 dni (90.9%): NIEZGODNE", p.stdout)
        p = self._cli(self.karta, "--od", "2026-01-11", "--do", "2026-01-17", "--wyjscie", wyj, "--sprawdz-str")
        self.assertEqual(p.returncode, 0, p.stdout)

    def test_m19_m20(self):
        self.assertEqual((self.m("2026-01-11")["m19_noce"], self.m("2026-01-11")["m19_dni"]), (1, 1))
        self.assertEqual(self.m("2026-01-13")["m20_seria"], 3)
        self.assertEqual(self.m("2026-01-14")["m20_seria"], 0)
        self.assertEqual(self.m("2026-01-16")["m20_seria"], 2)
        self.assertEqual(self.m("2026-01-19")["m20_seria"], 2)    # 17.01 bez nocy przerywa serię
        m = self.m("2026-01-20")
        self.assertEqual((m["m19_noce"], m["m19_dni"]), (7, 10))
        self.assertAlmostEqual(m["m19_proc"], 70.0)
        self.assertEqual(m["m20_seria"], 0)

    # --- CLI ----------------------------------------------------------------
    def _cli(self, *arg):
        return subprocess.run([sys.executable, "-I", os.path.join(_TU, "noce.py")] + list(arg),
                              capture_output=True, text=True)

    def test_cli_zapis_poza_karta_i_karta_bez_zmian(self):
        przed = _stan_katalogu(self.karta)
        wyj = os.path.join(self.tmp, "noce.csv")
        p = self._cli(self.karta, "--od", "2026-01-11", "--do", "2026-01-20", "--wyjscie", wyj)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(_stan_katalogu(self.karta), przed)
        with open(wyj, encoding="utf-8") as f:
            linie = f.read().splitlines()
        self.assertEqual(linie[0].split(",")[:3], ["data_poranka", "day", "na_liscie"])
        self.assertEqual(len(linie), 1 + 10)
        self.assertTrue(linie[7].startswith("2026-01-17,") and ",nie" in linie[7])

    def test_cli_odmawia_zapisu_do_karty(self):
        wyj = os.path.join(self.karta, "noce.csv")
        p = self._cli(self.karta, "--wyjscie", wyj)
        self.assertEqual(p.returncode, 2)
        self.assertFalse(os.path.exists(wyj))

    def test_cli_odmawia_zapisu_do_repo(self):
        wyj = os.path.join(REPO, "noce-test.csv")
        p = self._cli(self.karta, "--wyjscie", wyj)
        self.assertEqual(p.returncode, 2)
        self.assertFalse(os.path.exists(wyj))


class SesjeNocne(unittest.TestCase):
    """R-DANE-27 na przykładach z weryfikacji wymagania."""

    def setUp(self):
        self.par = noce.Parametry()
        self.dzien = noce.dzien_z_daty(dt.date(2026, 1, 10))

    def _rek(self, sesje):
        def ms(g, mi, nast=False):
            d = dt.datetime(2026, 1, 11 if nast else 10, g, mi)
            return int((d - edf.EPOKA).total_seconds() * 1000)
        out = []
        for s in sesje:
            a = ms(*s[0])
            b = None if s[1] is None else ms(*s[1])
            out.append({"start_ms": a, "end_ms": b})
        return {"day": self.dzien, "sessions": out, "session_count": len(out)}

    def _m17_m18(self, sesje):
        m17, m18 = noce.m17_m18(self._rek(sesje), self.par)
        return m17, noce._godzina(m18)

    def test_przyklady(self):
        self.assertEqual(self._m17_m18([((15, 0), (16, 0)), ((23, 10), (6, 40, True))]), (0, "23:10"))
        self.assertEqual(self._m17_m18([((23, 10), (23, 40)), ((0, 20, True), (7, 0, True))]), (1, "00:20"))
        self.assertEqual(self._m17_m18([((23, 10), None), ((0, 20, True), None)]), (1, "23:10"))
        self.assertEqual(self._m17_m18([((13, 0), (15, 30))]), (0, None))
        self.assertEqual(self._m17_m18([((13, 0), (20, 0))]), (0, "13:00"))
        # Wczesny sen bez sesji ≥ 4 h po 18:00::30–21:40 i krótka 22:00–23:00
        self.assertEqual(self._m17_m18([((17, 30), (21, 40)), ((22, 0), (23, 0))]), (1, "17:30"))

    def test_wczesny_sen_rozbieznosc_w_spec(self):
        # Przykład z weryfikacji R-DANE-27 („17:30–01:00 i 01:20–05:30 → M-17 = 1, M-18 = 17:30”)
        # przeczy treści wymagania::20–05:30 trwa 4 h 10 min i zaczyna się po 18:00,
        # więc wyjątek wczesnego snu nie powinien działać. Skrypt stosuje treść wymagania;
        # rozbieżność zgłoszona do rozstrzygnięcia (README, „Niejasności w specyfikacji”).
        self.assertEqual(self._m17_m18([((17, 30), (1, 0, True)), ((1, 20, True), (5, 30, True))]), (0, "01:20"))


class CzytnikEDF(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="edf-")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_percentyl_najblizsza_ranga(self):
        v = list(range(1, 11))
        self.assertEqual(edf.percentyl(v, 50), 5)
        self.assertEqual(edf.percentyl(v, 95), 10)
        self.assertEqual(edf.percentyl_z_licznikow([(1, 5), (2, 5)], 50), 1)
        self.assertEqual(edf.percentyl_z_licznikow([(1, 94), (2, 6)], 95), 2)
        self.assertEqual(edf.percentyl_z_licznikow([(1, 95), (2, 5)], 95), 1)

    def test_adnotacje(self):
        p = os.path.join(self.tmp, "x_EVE.edf")
        s = dt.datetime(2026, 1, 10, 23, 0)
        syntetyczna.adnotacje(p, s, [(s + dt.timedelta(seconds=90), 12.5, "Hypopnea"),
                                     (s + dt.timedelta(seconds=100), None, "CSR Start")])
        f = edf.PlikEDF(p)
        self.assertTrue(f.edf_plus)
        self.assertEqual(f.adnotacje(), [(90.0, 12.5, "Hypopnea"), (100.0, None, "CSR Start")])
        self.assertEqual(f.start, s)

    def _karta_jednej_nocy(self, profil, minuty=300):
        karta = os.path.join(self.tmp, "k")
        k = os.path.join(karta, "DATALOG", "20260110")
        os.makedirs(k)
        s = dt.datetime(2026, 1, 10, 23, 0)
        prefiks = syntetyczna._sesja(k, s, minuty, profil)
        syntetyczna.adnotacje(os.path.join(k, prefiks + "_EVE.edf"), s, [])
        syntetyczna.str_edf(os.path.join(karta, "STR.edf"), dt.date(2026, 1, 10),
                            [dict(syntetyczna.ASVAUTO, Duration=minuty)])
        return karta, os.path.join(k, prefiks + "_PLD.edf")

    def test_przeciek_rowny_progowi_nie_jest_duzy(self):
        # 0,4 L/s = 24 L/min dokładnie: AirBridge liczy tylko > 24 L/min
        karta, _ = self._karta_jednej_nocy(lambda i: {"Leak.2s": 0.4 if i < 100 else 0.42})
        r = noce.noce(karta)[0]
        self.assertAlmostEqual(r["leak_high_seconds"], 200 * 60)

    def test_plik_niepelny_daje_noc_czesciowa(self):
        karta, pld = self._karta_jednej_nocy(None)
        with open(pld, "r+b") as f:
            f.truncate(os.path.getsize(pld) - 100)
        r = noce.noce(karta)[0]
        self.assertTrue(r["partial"])
        self.assertTrue(any("niepełny" in p for p in r["problemy"]))
        self.assertFalse(r["metryki"]["wazna"])

    def test_brak_strefy_czasu(self):
        # godzina z nagłówka jest czasem lokalnym aparatu, bez przeliczania strefy
        karta, pld = self._karta_jednej_nocy(None)
        self.assertEqual(edf.PlikEDF(pld).start_s % 86400, 23 * 3600)


class PrzypadkiKarty(unittest.TestCase):
    """Przypadki graniczne formatu karty."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="karta-przyp-")
        self.karta = os.path.join(self.tmp, "k")
        self.dzien = os.path.join(self.karta, "DATALOG", "20260110")
        os.makedirs(self.dzien)
        self.s = dt.datetime(2026, 1, 10, 23, 0)
        syntetyczna.str_edf(os.path.join(self.karta, "STR.edf"), dt.date(2026, 1, 10),
                            [dict(syntetyczna.ASVAUTO, Duration=300)])

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def _eve(self, start, zdarzenia, rodzaj="EVE"):
        syntetyczna.adnotacje(os.path.join(self.dzien, start.strftime("%Y%m%d_%H%M%S") + "_%s.edf" % rodzaj),
                              start, zdarzenia)

    def _noc(self):
        return noce.noce(self.karta)[0]

    def test_aparat_bez_podzialu_bezdechow_i_bez_csr(self):
        # wzorzec z prawdziwej karty AirCurve: tylko „Recording starts”, „Apnea”, „Hypopnea”, brak CSL
        syntetyczna._sesja(self.dzien, self.s, 300)
        self._eve(self.s - dt.timedelta(seconds=5), [
            (self.s - dt.timedelta(seconds=5), None, "Recording starts"),
            (self.s + dt.timedelta(minutes=30), 12, "Apnea"),
            (self.s + dt.timedelta(minutes=60), 12, "Apnea"),
            (self.s + dt.timedelta(minutes=90), 15, "Hypopnea")])
        r = self._noc()
        self.assertTrue(r["events_known"])
        self.assertFalse(r["partial"])
        self.assertFalse(r["klasyfikacja_bezdechow"])
        self.assertFalse(r["csr_dostepne"])
        self.assertEqual(r["counts"], {"oa": None, "ca": None, "ua": 2, "h": 1, "rera": 0})
        self.assertAlmostEqual(r["ahi"], 3 / 5.0)
        self.assertIsNone(r["csr_seconds"])
        m = r["metryki"]
        for k in ("m03_oa_h", "m03_ca_h", "m03_obturacyjne_h", "m09_csr_min", "m09_csr_proc"):
            self.assertIsNone(m[k], k)
        self.assertAlmostEqual(m["m03_ua_h"], 2 / 5.0)
        w = noce.wiersz(r)
        self.assertEqual([noce._formatuj(w[k]) for k in ("oa", "ca", "m03_oa_h", "m03_ca_h", "csr_seconds")],
                         ["", "", "", "", ""])
        self.assertEqual((noce._formatuj(w["klasyfikacja_bezdechow"]), noce._formatuj(w["csr_dostepne"])),
                         ("nie", "nie"))

    def test_z_klasyfikacja_zero_centralnych_to_zero(self):
        syntetyczna._sesja(self.dzien, self.s, 300)
        self._eve(self.s, [(self.s + dt.timedelta(minutes=30), 12, "Obstructive Apnea")])
        r = self._noc()
        self.assertTrue(r["klasyfikacja_bezdechow"])
        self.assertEqual((r["counts"]["oa"], r["counts"]["ca"]), (1, 0))
        self.assertEqual(r["metryki"]["m03_ca_h"], 0)

    def test_csr_z_eve_bez_csl(self):
        syntetyczna._sesja(self.dzien, self.s, 300)
        self._eve(self.s, [(self.s + dt.timedelta(minutes=60), None, "CSR Start"),
                           (self.s + dt.timedelta(minutes=72), None, "CSR End")])
        r = self._noc()
        self.assertEqual(r["csr_zrodlo"], "EVE")
        self.assertTrue(r["csr_dostepne"])
        self.assertAlmostEqual(r["csr_seconds"], 12 * 60)

    def test_dwa_eve_z_tym_samym_zdarzeniem_liczone_raz(self):
        syntetyczna._sesja(self.dzien, self.s, 300)
        z = [(self.s + dt.timedelta(minutes=30), 12, "Central Apnea")]
        self._eve(self.s, z)
        self._eve(self.s + dt.timedelta(minutes=1), z)
        self.assertEqual(self._noc()["counts"]["ca"], 1)

    def test_sad_z_innym_prefiksem_dolaczony_po_czasie(self):
        syntetyczna._sesja(self.dzien, self.s, 300, przesuniecia={"SAD": 40})   # poza tolerancją 10 s
        self._eve(self.s, [])
        r = self._noc()
        self.assertEqual(r["session_count"], 1)
        self.assertFalse(r["partial"], r["problemy"])
        self.assertAlmostEqual(r["channels"]["SpO2"]["median"], 95.0)
        self.assertGreater(r["sessions"][0]["end_ms"] - r["sessions"][0]["start_ms"], 300 * 60 * 1000)

    def test_ujemny_onset_przypisany_po_czasie(self):
        syntetyczna._sesja(self.dzien, self.s, 300)
        e = self.s + dt.timedelta(minutes=10)
        self._eve(e, [(self.s + dt.timedelta(minutes=5), 12, "Central Apnea")])  # onset −300 s
        r = self._noc()
        self.assertFalse(r["partial"])
        self.assertEqual(r["counts"]["ca"], 1)

    def test_ujemny_czas_trwania_jak_airbridge(self):
        syntetyczna._sesja(self.dzien, self.s, 300)
        self._eve(self.s, [(self.s + dt.timedelta(minutes=30), "-5", "Obstructive Apnea")])
        r = self._noc()
        self.assertTrue(r["partial"])
        self.assertFalse(r["events_known"])
        self.assertIsNone(r["ahi"])
        self.assertTrue(any("ujemny czas trwania" in p for p in r["problemy"]))

    def test_edf_plus_d_z_sygnalami_daje_noc_czesciowa(self):
        syntetyczna._sesja(self.dzien, self.s, 300)
        self._eve(self.s, [])
        pld = os.path.join(self.dzien, self.s.strftime("%Y%m%d_%H%M%S") + "_PLD.edf")
        with open(pld, "r+b") as f:
            f.seek(192)
            f.write(b"EDF+D".ljust(44))
        r = self._noc()
        self.assertTrue(r["partial"])
        self.assertTrue(any("EDF+D" in p for p in r["problemy"]))


class WsparcieCisnieniowe(unittest.TestCase):
    """M-06p (EPAP P95), M-28 i M-28p (wsparcie PS = Press.2s − EprPress.2s, próbka po próbce)."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="karta-ps-")
        cls.karta = os.path.join(cls.tmp, "karta")
        syntetyczna.utworz_ps(cls.karta)
        cls.n = {r["data_poranka"]: r for r in noce.noce(cls.karta)}

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def m(self, data):
        return self.n[data]["metryki"]

    def test_p1_stale_ipap_14_epap_7(self):
        m = self.m("2026-02-02")
        self.assertAlmostEqual(m["m05_cisnienie_med"], 14.0)
        self.assertAlmostEqual(m["m06_wydechowe_med"], 7.0)
        self.assertAlmostEqual(m["m06p_wydechowe_p95"], 7.0)
        self.assertAlmostEqual(m["m28_wsparcie_med"], 7.0)
        self.assertAlmostEqual(m["m28p_wsparcie_p95"], 7.0)
        self.assertEqual(self.n["2026-02-02"]["wsparcie"]["source"], "edf")

    def test_p2_przerwa_w_kanale_epap(self):
        m = self.m("2026-02-03")
        # pary tylko z chwil, gdy obie próbki poprawne: PS 4 × 90 min, PS 9 × 150 min
        self.assertAlmostEqual(m["m28_wsparcie_med"], 9.0)
        self.assertAlmostEqual(m["m28p_wsparcie_p95"], 9.0)
        # różnica median kanałów dałaby co innego (12 − 6)
        self.assertAlmostEqual(m["m05_cisnienie_med"] - m["m06_wydechowe_med"], 6.0)
        self.assertAlmostEqual(m["m06_wydechowe_med"], 6.0)
        self.assertAlmostEqual(m["m06p_wydechowe_p95"], 8.0)
        self.assertAlmostEqual(m["m05p_cisnienie_p95"], 15.0)

    def test_p3_p95_najblizsza_ranga(self):
        m = self.m("2026-02-04")
        self.assertAlmostEqual(m["m28_wsparcie_med"], 5.0)
        self.assertAlmostEqual(m["m28p_wsparcie_p95"], 5.0)     # 285 z 300 min = dokładnie 95%
        self.assertAlmostEqual(m["m05p_cisnienie_p95"], 13.0)

    def test_p4_tylko_str_puste(self):
        r = self.n["2026-02-05"]
        m = r["metryki"]
        self.assertEqual(m["zrodlo_cisnienia"], "str")
        self.assertIsNone(m["m28_wsparcie_med"])
        self.assertIsNone(m["m28p_wsparcie_p95"])
        self.assertIsNone(m["m06p_wydechowe_p95"])               # STR karty bez TgtEPAP.95
        w = noce.wiersz(r)
        self.assertEqual([noce._formatuj(w[k]) for k in ("m28_wsparcie_med", "m28p_wsparcie_p95")], ["", ""])

    def test_kolumny_csv(self):
        i = noce.KOLUMNY.index("m06_wydechowe_med")
        self.assertEqual(noce.KOLUMNY[i + 1:i + 4], ["m06p_wydechowe_p95", "m28_wsparcie_med", "m28p_wsparcie_p95"])
        self.assertEqual(len(noce.KOLUMNY), len(set(noce.KOLUMNY)))
        w = noce.wiersz(self.n["2026-02-02"])
        self.assertEqual([noce._formatuj(w[k]) for k in ("m06p_wydechowe_p95", "m28_wsparcie_med",
                                                          "m28p_wsparcie_p95")], ["7", "7", "7"])

    def test_n1_karty_glownej(self):
        # karta główna, N1: 60% 12/8 (PS 4), 40% 15/8 (PS 7)
        tmp = tempfile.mkdtemp(prefix="karta-syn-ps-")
        try:
            k = os.path.join(tmp, "k")
            syntetyczna.utworz(k)
            n = {r["data_poranka"]: r["metryki"] for r in noce.noce(k)}
            self.assertEqual((n["2026-01-11"]["m28_wsparcie_med"], n["2026-01-11"]["m28p_wsparcie_p95"]), (4.0, 7.0))
            self.assertEqual(n["2026-01-11"]["m06p_wydechowe_p95"], 8.0)
            self.assertIsNone(n["2026-01-20"]["m28_wsparcie_med"])     # N10: bez PLD
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


class Historia(unittest.TestCase):
    """tools/prototyp/historia.py: import.csv dla panelu, 4 nowe kolumny na końcu."""

    def test_import_csv(self):
        tmp = tempfile.mkdtemp(prefix="historia-")
        try:
            k = os.path.join(tmp, "k")
            syntetyczna.utworz_ps(k)
            tab = os.path.join(tmp, "noce.csv")
            fs = os.path.join(tmp, "fs")
            subprocess.run([sys.executable, "-I", os.path.join(_TU, "noce.py"), k, "--wyjscie", tab],
                           check=True, capture_output=True)
            subprocess.run([sys.executable, "-I", os.path.join(REPO, "tools", "prototyp", "historia.py"), tab, fs],
                           check=True, capture_output=True)
            with open(os.path.join(fs, "import.csv"), encoding="utf-8") as f:
                linie = f.read().splitlines()
            self.assertEqual(linie[0], "data_poranka;wazna;m01_min;ahi;cisnienie_med;cisnienie_p95;"
                                       "wyciek_med;wyciek_p95;epap_med;epap_p95;ps_med;ps_p95;ustawienia")
            w = {l.split(";")[0]: l.split(";") for l in linie[1:]}
            self.assertEqual(w["2026-02-02"][4:12], ["14.0", "14.0", "6.0", "6.0", "7.0", "7.0", "7.0", "7.0"])
            self.assertEqual(w["2026-02-03"][8:12], ["6.0", "8.0", "9.0", "9.0"])
            self.assertEqual(w["2026-02-02"][12], "ASVAuto/6/10/3/15")       # ustawienia z STR.edf
            self.assertNotIn("2026-02-05", w)          # bez PLD nie ma czasu terapii: dzień pominięty
            self.assertTrue(all(len(x) == 13 for x in w.values()))
            # wiersz z czasem terapii, ale ciśnieniem ze STR: puste ciśnienia, EPAP i PS; wyciek zostaje
            tab2 = os.path.join(tmp, "noce2.csv")
            with open(tab2, "w", encoding="utf-8") as f:
                f.write("data_poranka,na_liscie,used_seconds,wazna,tryb_obslugiwany,m02_ahi,zrodlo_cisnienia,"
                        "m05_cisnienie_med,m05p_cisnienie_p95,m07_przeciek_med,m07_przeciek_p95,"
                        "m06_wydechowe_med,m06p_wydechowe_p95,m28_wsparcie_med,m28p_wsparcie_p95,ustawienia_tagi\n"
                        "2026-03-01,tak,25200,tak,tak,1.234,str,12.5,15,6,12,8,9,,,\n"
                        "2026-03-02,tak,25200,tak,tak,1,edf,12,15,6,12,8,9,4,7,"
                        "MOP=ASVAuto;EAI=6;EAX=10;ANS=3;AXS=15;RMA=Off\n"
                        "2026-03-03,tak,25200,tak,tak,1,edf,12,15,6,12,8,9,4,7,MOP=ASVAuto;EAX=10;MSK=Nasal\n")
            fs2 = os.path.join(tmp, "fs2")
            subprocess.run([sys.executable, "-I", os.path.join(REPO, "tools", "prototyp", "historia.py"), tab2, fs2],
                           check=True, capture_output=True)
            with open(os.path.join(fs2, "import.csv"), encoding="utf-8") as f:
                l2 = f.read().splitlines()
            self.assertEqual(l2[1], "2026-03-01;1;420;1.23;;;6.0;12.0;;;;;")          # bez tagów: puste pole
            self.assertEqual(l2[2].split(";")[12], "ASVAuto/6/10/3/15")
            self.assertEqual(l2[3].split(";")[12], "ASVAuto//10//")                # brak tagu: pusty fragment
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


class OchronaRepo(unittest.TestCase):
    def _cli(self, *arg):
        return subprocess.run([sys.executable, "-I", os.path.join(_TU, "noce.py")] + list(arg),
                              capture_output=True, text=True)

    def test_karta_w_repo_odrzucona(self):
        k = tempfile.mkdtemp(prefix="tmp-karta-", dir=REPO)
        try:
            syntetyczna.str_edf(os.path.join(k, "STR.edf"), dt.date(2026, 1, 10), [dict(syntetyczna.ASVAUTO)])
            wyj = os.path.join(tempfile.gettempdir(), "noce-test-repo.csv")
            p = self._cli(k, "--wyjscie", wyj)
            self.assertEqual(p.returncode, 2)
            self.assertIn("repozytorium", p.stderr)
            self.assertFalse(os.path.exists(wyj))
        finally:
            shutil.rmtree(k, ignore_errors=True)

    def test_wyjscie_przez_dowiazanie_do_repo_odrzucone(self):
        tmp = tempfile.mkdtemp(prefix="dow-")
        try:
            os.symlink(REPO, os.path.join(tmp, "repo"))
            self.assertTrue(noce._wewnatrz(os.path.join(tmp, "repo", "x", "noce.csv"), REPO))
            self.assertFalse(noce._wewnatrz(os.path.join(tmp, "noce.csv"), REPO))
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


class BezPrawdziwychNocyWRepo(unittest.TestCase):
    def test_brak_plikow_edf_w_repo(self):
        znalezione = []
        for d, katalogi, pliki in os.walk(REPO):
            katalogi[:] = [k for k in katalogi if k != ".git"]
            for p in pliki:
                sciezka = os.path.join(d, p)
                if p.lower().endswith((".edf", ".crc")):
                    znalezione.append(sciezka)
                elif p.lower().endswith(".csv"):
                    with open(sciezka, encoding="utf-8", errors="replace") as f:
                        if f.readline().startswith("data_poranka,day,na_liscie"):
                            znalezione.append(sciezka)          # tabela nocy z noce.py
        self.assertEqual(znalezione, [], "pliki z karty SD albo tabele nocy w repozytorium (WY12)")

    def test_brak_plikow_edf_w_historii_git(self):
        p = subprocess.run(["git", "-C", REPO, "log", "--all", "--name-only", "--pretty=format:"],
                           capture_output=True, text=True)
        if p.returncode != 0:
            self.skipTest("brak historii git")
        pliki = [l for l in p.stdout.splitlines() if l.lower().endswith((".edf", ".crc"))]
        self.assertEqual(pliki, [], "pliki z karty SD w historii git (WY12)")


if __name__ == "__main__":
    unittest.main()
