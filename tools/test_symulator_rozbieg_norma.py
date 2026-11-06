# SPDX-License-Identifier: GPL-3.0-or-later
"""Symulator doby, poprawka po integracji rozbieg scenariuszy w normie osobistej zrzutu.

Regresja: na zrzucie z historią inną niż stałe wartości rozbiegu (np. dłuższy czas z maską, wyższy EPAP) rozbieg dawał
stan odchylenia (4.6), pełny ekran S3 (np. E-13) zasłaniał zapowiedziane ekrany (`krotkie-noce`: ekran dnia
z krótką nocą i z nocą bez terapii), a prośba S3 (R-PIE-10) blokowała S-K-01 (`co2`). Test odtwarza to na zrzucie
syntetycznym z przesuniętą normą i przechodzi `--pokaz-wszystko` dla tych scenariuszy. Bez prawdziwych danych.
"""
import glob
import importlib.util
import os

import pytest

TU = os.path.dirname(os.path.abspath(__file__))


def modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa + "_rozbieg", os.path.join(TU, "symulator", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


sym = modul("symuluj")
gen = modul("generuj_zrzut")
scen = sym.scen


@pytest.fixture(scope="module")
def zrzut_inna_norma(tmp_path_factory):
    """Zrzut syntetyczny (wersja 2, 120 nocy) z czasem z maską ok. 8–10 h i EPAP wyższym o 2–2,5 cmH2O."""
    k = str(tmp_path_factory.mktemp("zrzut_inna_norma"))
    gen.generuj(k, wersja=2, nocy=120)
    for p in glob.glob(os.path.join(k, "dane", "noce", "*.csv")):
        with open(p, encoding="utf-8") as f:
            w = f.read().splitlines()
        out = []
        for l in w:
            c = l.split(";")
            if len(c) > 20 and c[0][:2] == "20":
                c[2] = str(int(c[2]) + 150)
                c[8] = "%.2f" % (float(c[8]) + 2.0)
                c[9] = "%.2f" % (float(c[9]) + 2.5)
            out.append(";".join(c))
        with open(p, "w", encoding="utf-8") as f:
            f.write("\n".join(out) + "\n")
    return k


def test_rozbieg_w_normie_zrzutu(zrzut_inna_norma):
    w = scen.wzor(zrzut_inna_norma)
    plan = scen.plan(zrzut_inna_norma, ["co2"])
    rozbieg = [dict(zip(scen.POLA, r.split(";"))) for r in plan.noce[:scen.ROZBIEG]]
    for k in ("m01_min", "emed", "ep95"):
        med = w["typowe"][k]
        for d in rozbieg:
            assert abs(float(d[k]) - med) <= 0.04 * med, (k, d[k], med)
    for d in rozbieg:  # bezdechy i wyciek nie wyżej niż zdrowy zakres stały
        assert float(d["ca"]) <= 0.8 and float(d["wp95"]) <= 13.0


def test_pokaz_wszystko_bez_brakow_przy_innej_normie(zrzut_inna_norma, tmp_path):
    k, braki = sym.pokaz_wszystko(zrzut_inna_norma, str(tmp_path), cicho=True, nazwy=["krotkie-noce", "co2"])
    assert braki == [], braki
    with open(os.path.join(k, "spis.txt"), encoding="utf-8") as f:
        spis = f.read()
    assert "zdarzenie-E-13" not in spis and "zdarzenie-E-36" not in spis  # rozbieg bez stanu odchylenia
