# SPDX-License-Identifier: GPL-3.0-or-later
"""Narzędzie porównania nocy panelu z raportem AirBridge (noc_panelu.sh, ADR-0036 pkt 5) i karta nocy panelu.

Na syntetycznej karcie z syntetyczna.utworz_noce_panelu() raport „AirBridge” składa ten test z wyniku noce.py
(ten sam układ pól co raport `/api/report?source=edf&day=N`, Załącznik A), więc narzędzie ma wyjść w tolerancji;
raport z celowo zmienionym AHI daje rozjazd i kod 1. Bez kompilatora C++ testy narzędzia są pominięte.

Uruchomienie: python3 -m pytest -q tools/kalibracja/test_noc_panelu.py
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

_TU = os.path.dirname(os.path.abspath(__file__))
if _TU not in sys.path:
    sys.path.insert(0, _TU)

import noc_wzorzec  # noqa: E402
import noce  # noqa: E402
import syntetyczna  # noqa: E402

SKRYPT = os.path.join(_TU, "noc_panelu.sh")
KOMPILATOR = shutil.which(os.environ.get("CXX", "g++"))


def raport_z_noce(r):
    """Raport nocy w układzie AirBridge (Załącznik A) z rekordu noce.py."""
    kanaly = []
    for id_k, unit in ((k[0], k[3]) for k in noce.KANALY):
        ch = r["channels"][id_k]
        if ch["source"]:
            kanaly.append({"id": id_k, "unit": unit, "median": ch["median"], "p95": ch["p95"],
                           "source": ch["source"]})
    return {
        "day": r["day"], "present": r["present"], "partial": r["partial"], "used_seconds": r["used_seconds"],
        "ahi": r["ahi"], "events_known": r["events_known"], "session_count": r["session_count"],
        "counts": r["counts"], "csr_seconds": r["csr_seconds"], "leak_high_seconds": r["leak_high_seconds"],
        "sessions": [{"start_ms": s["start_ms"], "end_ms": s["end_ms"]} for s in r["sessions"]],
        "channels": kanaly,
        "settings": {"available": r["settings"]["available"],
                     "fields": [{"tag": p["tag"], "value": p["value"], "option": p["option"]}
                                for p in r["settings"]["fields"]]},
    }


class KartaNocyPanelu(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="noc-panelu-")
        cls.karta = os.path.join(cls.tmp, "karta")
        syntetyczna.utworz_noce_panelu(cls.karta)
        cls.lista = {r["data_poranka"]: r for r in noce.noce(cls.karta)}

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_znane_wartosci(self):
        z1 = self.lista["2026-11-02"]
        self.assertFalse(z1["partial"])
        self.assertEqual(z1["used_seconds"], 480 * 60)
        self.assertEqual(z1["csr_zrodlo"], "CSL")
        self.assertIsNotNone(z1["wsparcie"]["median"])
        self.assertGreater(z1["leak_high_seconds"], 0)
        z2 = self.lista["2026-11-03"]
        self.assertIsNone(z2["counts"]["oa"])          # EVE bez rodzaju bezdechu
        self.assertEqual(z2["csr_zrodlo"], "EVE")
        self.assertEqual(self.lista["2026-11-04"]["metryki"]["m17_przerwy"], 0)   # drzemka poza M-17
        self.assertEqual(self.lista["2026-11-05"]["metryki"]["m17_przerwy"], 1)   # wczesny sen
        self.assertIsNone(self.lista["2026-11-06"]["csr_seconds"])                # CSR Start bez End

    def test_wzorzec(self):
        w = noc_wzorzec.wzorzec(self.karta)
        self.assertTrue(all(x.startswith("N;") and x.count(";") == 3 for x in w))
        self.assertIn("N;%d;partial;0" % self.lista["2026-11-02"]["day"], w)

    def _uruchom(self, data, zmiana=None):
        r = self.lista[data]
        rap = raport_z_noce(r)
        if zmiana:
            zmiana(rap)
        sciezka = os.path.join(self.tmp, "raport-%s.json" % data)
        with open(sciezka, "w", encoding="utf-8") as f:
            json.dump(rap, f)
        katalog = os.path.join(self.karta, "DATALOG", noce.data_z_dnia(r["day"]).strftime("%Y%m%d"))
        return subprocess.run(["bash", SKRYPT, katalog, sciezka, os.path.join(self.karta, "STR.edf")],
                              capture_output=True, text=True, timeout=300)

    @unittest.skipIf(KOMPILATOR is None, "brak kompilatora C++")
    def test_narzedzie_w_tolerancji(self):
        for data in sorted(self.lista):
            p = self._uruchom(data)
            self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
            self.assertIn("metryka;panel;raport;różnica;tolerancja;wynik", p.stdout)
            self.assertIn("w tolerancji WY9", p.stdout)

    @unittest.skipIf(KOMPILATOR is None, "brak kompilatora C++")
    def test_narzedzie_rozjazd(self):
        def zmien(rap):
            rap["ahi"] = rap["ahi"] + 1.0
        p = self._uruchom("2026-11-02", zmien)
        self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
        self.assertIn("M-02 AHI", p.stdout)
        self.assertIn("ROZJAZD", p.stdout)

    @unittest.skipIf(KOMPILATOR is None, "brak kompilatora C++")
    def test_narzedzie_te_same_reguly_co_panel(self):
        # porownanie::porownaj() (firmware/prototyp/src/porownanie.cpp), ta sama funkcja co tryb panelu
        # `noc_zrodlo=porownanie`: OA/CA nieznane w plikach (EVE tylko `Apnea`) przy 0 w raporcie = pominięte,
        # PS tylko w plikach, metryka tylko w raporcie = rozjazd
        def zera(rap):  # kopia: `counts` raportu to ten sam słownik co w rekordzie noce.py klasy
            rap["counts"] = dict(rap["counts"], oa=0, ca=0)
        p = self._uruchom("2026-11-03", zera)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn("M-03 OA/h;–;0.00;–;0.50;pominięte: aparat nie rozróżnia", p.stdout)
        p = self._uruchom("2026-11-02")
        self.assertIn("tylko pliki", p.stdout)

        def wiecej(rap):
            rap["counts"] = dict(rap["counts"], oa=2, ca=0)
        p = self._uruchom("2026-11-03", wiecej)
        self.assertEqual(p.returncode, 1, p.stdout + p.stderr)
        self.assertIn("M-03 OA/h;–;", p.stdout)
        self.assertIn("brak w plikach", p.stdout)

    @unittest.skipIf(KOMPILATOR is None, "brak kompilatora C++")
    def test_narzedzie_bledne_wejscie(self):
        p = subprocess.run(["bash", SKRYPT, os.path.join(self.tmp, "nie-ma"), os.path.join(self.tmp, "nie-ma.json")],
                           capture_output=True, text=True, timeout=300)
        self.assertEqual(p.returncode, 2)


if __name__ == "__main__":
    unittest.main()
