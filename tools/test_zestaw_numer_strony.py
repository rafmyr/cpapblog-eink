# SPDX-License-Identifier: GPL-3.0-or-later
"""Budowa zestawu: kontrola „liczba progu wpisana bez {prog:…}” pomija tylko numer strony kokpitu `n / 3` po słowie
„raport”, „strona” albo „stroną” (5.5.7); próg wpisany na sztywno w zapisie ułamka (`3 / 7 nocy`) dalej jest błędem.
"""
from test_buduj_zestaw import PROGI, E30, TEKSTY, SPEC, przygotuj, uruchom


def test_prog_w_ulamku_nie_jest_numerem_strony(tmp_path):
    progi = PROGI + "E-30.trzy,E-30,,3,noce,opis,brak,spec,wszystkie,do kalibracji\n"
    e = E30.replace("n=E-30.okno", "n=E-30.okno,delta=E-30.trzy")
    e2 = e.replace("E-30.krok;aktywna", "E-30.krok,E-30.linia;aktywna")
    spec = SPEC + "\n`Wyżej w 3 / 7 nocy.`\n"
    t = TEKSTY + "E-30.linia;linia;;Wyżej w 3 / 7 nocy.\n"
    kod, out = uruchom(przygotuj(tmp_path, progi=progi, katalog_e=(e2,), teksty=t, spec=spec))
    # oczekiwane: błąd jak przy `Od 3 nocy.`; numer strony to tylko `n / 3` po „raport”, „strona” albo „stroną”
    assert kod == 1 and "przez {prog:" in out, "próg 3 w `3 / 7 nocy` przeszedł kontrolę: " + out


def test_numer_strony_bez_slowa_strony_odrzucony(tmp_path):
    # R-KOK-30, `{1|2|3} / 3` poza słowem strony (ułamek, numer bez słowa) = błąd budowy, także z twardą spacją
    for tekst in ("Wyżej w 2 / 3 nocy.", "Ustawienia 3 / 3.", "Oddechy 1 / 3 nocy."):
        e2 = E30.replace("E-30.krok;aktywna", "E-30.krok,E-30.linia;aktywna")
        spec = SPEC + "\n`%s`\n" % tekst
        t = TEKSTY + "E-30.linia;linia;;%s\n" % tekst
        kod, out = uruchom(przygotuj(tmp_path, katalog_e=(e2,), teksty=t, spec=spec))
        assert kod == 1 and "bez słowa strony" in out, tekst + ": " + out


def test_numer_strony_po_slowie_przechodzi(tmp_path):
    for tekst in ("Na kontroli: raport 2 / 3.", "Lista na stronie 1 / 3.", "Ustawienia: strona 3 / 3."):
        e2 = E30.replace("E-30.krok;aktywna", "E-30.krok,E-30.linia;aktywna")
        spec = SPEC + "\n`%s`\n" % tekst
        t = TEKSTY + "E-30.linia;linia;;%s\n" % tekst
        kod, out = uruchom(przygotuj(tmp_path, katalog_e=(e2,), teksty=t, spec=spec))
        assert "bez słowa strony" not in out, tekst + ": " + out


def test_numer_strony_niekanoniczny_odrzucony(tmp_path):
    # R-KOK-36, ADR-0045: zestaw pisze tylko `{1|2|3} / 3`; panel przelicza na bieżący ciąg i kolejność, więc
    # `raport 2 / 4` (już przeliczony) albo `strona 4 / 4` zostałyby na ekranie bez przeliczenia
    for tekst in ("Na kontroli: raport 2 / 4.", "Ustawienia: strona 4 / 4.", "Lista na stronie 1 / 5.",
                  "Ustawienia: strona\u00a04 / 4."):
        e2 = E30.replace("E-30.krok;aktywna", "E-30.krok,E-30.linia;aktywna")
        spec = SPEC + "\n`%s`\n" % tekst
        t = TEKSTY + "E-30.linia;linia;;%s\n" % tekst
        kod, out = uruchom(przygotuj(tmp_path, katalog_e=(e2,), teksty=t, spec=spec))
        assert kod == 1 and "inny niż" in out, tekst + ": " + out
