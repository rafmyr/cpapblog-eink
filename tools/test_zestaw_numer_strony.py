# SPDX-License-Identifier: GPL-3.0-or-later
"""Budowa zestawu: kontrola „liczba progu wpisana bez {prog:…}” pomija tylko numer strony kokpitu `n / 6` po słowie
„raport” albo „strona”; próg wpisany na sztywno w zapisie ułamka (`3 / 7 nocy`) dalej jest błędem.
"""
from test_buduj_zestaw import PROGI, E30, TEKSTY, SPEC, przygotuj, uruchom


def test_prog_w_ulamku_nie_jest_numerem_strony(tmp_path):
    progi = PROGI + "E-30.trzy,E-30,,3,noce,opis,brak,spec,wszystkie,do kalibracji\n"
    e = E30.replace("n=E-30.okno", "n=E-30.okno,delta=E-30.trzy")
    e2 = e.replace("E-30.krok;aktywna", "E-30.krok,E-30.linia;aktywna")
    spec = SPEC + "\n`Wyżej w 3 / 7 nocy.`\n"
    t = TEKSTY + "E-30.linia;linia;;Wyżej w 3 / 7 nocy.\n"
    kod, out = uruchom(przygotuj(tmp_path, progi=progi, katalog_e=(e2,), teksty=t, spec=spec))
    # oczekiwane: błąd jak przy `Od 3 nocy.`; numer strony to tylko `n / 6` po „raport” albo „strona”
    assert kod == 1 and "przez {prog:" in out, "próg 3 w `3 / 7 nocy` przeszedł kontrolę: " + out
