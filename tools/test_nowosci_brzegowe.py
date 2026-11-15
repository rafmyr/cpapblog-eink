# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy brzegowe „Co nowego” (14.11.2026): próby obejścia filtra ADR-0002 w wydaj.bledy_nowosci().

Każdy przypadek z listy OBEJSCIA to linia, która podaje wartość albo drogę zmiany ustawienia aparatu; przed poprawką
 filtr ją przepuszczał (xfail), od poprawki odrzuca (zwykły test, bramka). PODOBNE: kolejne obejścia tego samego
rodzaju. PRZEPUSZCZA: zwykłe opisy zmian panelu, których filtr nie może odrzucać (fałszywe odrzucenia)."""
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "wydanie"))
import wydaj  # noqa: E402

OBEJSCIA = [
    "Obniz EPR o 1",                 # bez polskich znaków
    "Zwieksz cisnienie do 9",        # bez polskich znaków
    "OBNIZ EPR",                     # wielkie litery bez ogonków
    "Ustaw 7 cm",                    # jednostka bez H2O
    "Ciśnienie 7 cm H₂O",            # indeks dolny ₂ jest w krojach panelu
    "EPR na 2 zamiast 3",            # wartość bez czasownika
    "Min. ciśnienie 7",              # wartość bez czasownika
    "Wyłącz rampę",                  # czasownik spoza listy, odmiana „rampę”
    "Przestaw EPR na 2",             # czasownik z przedrostkiem
    "Włącz EPR3",                    # liczba przyklejona do skrótu
    "Ustaw nawilżanie na 4",         # nawilżacz to też ustawienie aparatu
    "Ustaw tryb CPAP",               # CPAP spoza listy słów aparatu
]


PODOBNE = [
    "Ciśnienie 7 cmH2O",
    "9 cmH₂O wystarczy",
    "Ustaw minimum ciśnienia na 7",
    "Zmniejsz ciśnienie",
    "Przełącz na APAP",
    "Ustaw temperaturę na 27",
    "Temperatura rury 27",
    "Rampa 20 min",
    "ASV: EPAP 6",
    "IPAP=12",
    "EPR 3 zamiast 2",
    "Zwiększ nawilżacz",
]

PRZEPUSZCZA = [
    "Portret nocy: łagodniejsze piki",
    "Znak na belce, gdy dane są nieaktualne",
    "Ustawienia w nowym układzie",
    "Ekran sypialni: wykres CO2 z 30 nocy",
    "Nowy wykres ciśnienia w kokpicie",
    "Sypialnia: temperatura 22° na belce",
    "Wykres min i max z 30 nocy",
    "Wyłącz dźwięk panelu w nocy",
    "Kropki ASV na portrecie nocy",
    "Aktualizacja do 1.2.0 w 4 krokach",
    "Włącz automatyczne aktualizacje",
    "Test sprzętu: słowa zamiast kodów",
    "Belka: bateria 70% na zasilaniu",
    "Zmiana ciśnienia w nocy na wykresie",
]


@pytest.mark.parametrize("linia", OBEJSCIA + PODOBNE)
def test_filtr_adr0002_odrzuca(linia):
    assert any("ADR-0002" in b for b in wydaj.bledy_nowosci([linia]))


@pytest.mark.parametrize("linia", PRZEPUSZCZA)
def test_filtr_adr0002_przepuszcza_opisy_panelu(linia):
    assert wydaj.bledy_nowosci([linia]) == []


def test_znaki_narzedzia_sa_w_krojach_panelu():
    """Znak dozwolony w narzędziu, którego panel nie ma w krojach, znika na ekranie bez ostrzeżenia."""
    import re
    sciezka = os.path.join(os.path.dirname(__file__), "..", "firmware", "prototyp", "include", "fonty.h")
    m = re.search(r'FONT_EXTRA\[\]\s*=\s*"([^"]*)"', open(sciezka, encoding="utf-8").read())
    assert m
    panel = set(chr(c) for c in range(0x20, 0x7F)) | set(m.group(1))
    assert wydaj.ZNAKI_PANELU - panel == set()
