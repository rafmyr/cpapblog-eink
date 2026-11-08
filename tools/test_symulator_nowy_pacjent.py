# SPDX-License-Identifier: GPL-3.0-or-later
"""Symulator doby: nowy pacjent bez historii, dni 1–30 (ADR-0034, T-TEST-383).

Trzy scenariusze `nowy-…` (tools/symulator/scenariusze.py, NOWY_PACJENT) na zrzucie syntetycznym z jedną nocą bez
terapii (jak panel u nowego użytkownika), tryb AutoSet, 30 nocy ze stałymi wartościami:
- `nowy-ahi12`: AHI 12/h, wyciek P95 9 L/min;
- `nowy-wyciek18`: AHI 12/h, wyciek P95 18 L/min (pasmo 15–24: każda noc niepewna, AHI nie oceniane);
- `nowy-ahi7`: AHI 7/h, wyciek P95 9 L/min (świadomy koszt ADR-0029: E-30 bez końca, bez L-03).
Dla każdego poranka (obrazek z 05:00) sprawdza ekran dnia i linię uwagi z listą oczekiwaną niżej. Listy spisane
z przebiegu 08.11.2026 i przejrzane wobec spec: S6 (rozruch) w nocach 1–6 i dłużej, E-30 od 7. nocy (mediana z 7 nocy),
limit problemu porannego 2 razy w 7 dniach (R-KON-14) z linią `Wciąż: …` (R-EKR-40), L-03 od 10. nocy (≥ 10 nocy
ważnych w 14), L-09 dopiero od 14. nocy, bo warunek „10 z 14” liczy się z pełnego okna 14 nocy z terapią,
linia R-SKI-24 od 4. nocy niepewnej. Dane wyłącznie syntetyczne.
"""
import importlib.util
import os
import re

import pytest

TU = os.path.dirname(os.path.abspath(__file__))


def modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa + "_nowy", os.path.join(TU, "symulator", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


sym = modul("symuluj")
gen = modul("generuj_zrzut")
scen = sym.scen

NIECZYTELNE = "Noce mało czytelne: bezdechów nie oceniam."
WCIAZ_E30 = "Wciąż: Noce z AHI powyżej 5."
WCIAZ_L03 = "Wciąż: Bezdechy utrzymują się wyżej."
BEZ_SENN = "Bez odpowiedzi o senność nie oceniam części spraw."

AHI12 = [  # 05:00, poranek 1…30: (ekran, linia uwagi)
    ("S6", ""),  # 1
    ("S6", ""),  # 2
    ("S6", ""),  # 3
    ("S6", ""),  # 4
    ("S6", ""),  # 5
    ("S6", ""),  # 6
    ("zdarzenie-E-30", ""),  # 7
    ("zdarzenie-E-30", ""),  # 8
    ("S6", WCIAZ_E30),  # 9
    ("wzorzec-L-03_ge10", ""),  # 10
    ("S6", WCIAZ_E30),  # 11
    ("S6", WCIAZ_E30),  # 12
    ("S6", WCIAZ_L03),  # 13
    ("zdarzenie-E-30", ""),  # 14
    ("zdarzenie-E-30", ""),  # 15
    ("S2B", WCIAZ_L03),  # 16
    ("wzorzec-L-03_ge10", ""),  # 17
    ("S2B", WCIAZ_E30),  # 18
    ("S2A", WCIAZ_L03),  # 19
    ("S2B", WCIAZ_E30),  # 20
    ("zdarzenie-E-30", ""),  # 21
    ("zdarzenie-E-30", WCIAZ_L03),  # 22
    ("S2A", WCIAZ_E30),  # 23
    ("wzorzec-L-03_ge10", ""),  # 24
    ("S2B", WCIAZ_L03),  # 25
    ("S2A", WCIAZ_E30),  # 26
    ("S7", WCIAZ_E30),  # 27
    ("zdarzenie-E-30", WCIAZ_L03),  # 28
    ("zdarzenie-E-30", BEZ_SENN),  # 29
    ("KM", WCIAZ_E30),  # 30
]

WYCIEK18 = [  # 05:00, poranek 1…30: (ekran, linia uwagi)
    ("SK10", ""),  # 1
    ("SK10", ""),  # 2
    ("S6", ""),  # 3
    ("S6", NIECZYTELNE),  # 4
    ("S6", NIECZYTELNE),  # 5
    ("S6", NIECZYTELNE),  # 6
    ("KM", NIECZYTELNE),  # 7
    ("SK10", NIECZYTELNE),  # 8
    ("SK10", NIECZYTELNE),  # 9
    ("S6", NIECZYTELNE),  # 10
    ("S6", NIECZYTELNE),  # 11
    ("S6", NIECZYTELNE),  # 12
    ("S6", NIECZYTELNE),  # 13
    ("wzorzec-L-09", NIECZYTELNE),  # 14
    ("wzorzec-L-09", NIECZYTELNE),  # 15
    ("SK10", NIECZYTELNE),  # 16
    ("SK10", NIECZYTELNE),  # 17
    ("S2B", NIECZYTELNE),  # 18
    ("S2A", NIECZYTELNE),  # 19
    ("S2B", NIECZYTELNE),  # 20
    ("wzorzec-L-09", NIECZYTELNE),  # 21
    ("wzorzec-L-09", NIECZYTELNE),  # 22
    ("SK10", NIECZYTELNE),  # 23
    ("SK10", NIECZYTELNE),  # 24
    ("S2B", NIECZYTELNE),  # 25
    ("S2A", NIECZYTELNE),  # 26
    ("S7", NIECZYTELNE),  # 27
    ("wzorzec-L-09", NIECZYTELNE),  # 28
    ("wzorzec-L-09", NIECZYTELNE),  # 29
    ("SK10", NIECZYTELNE),  # 30
]

AHI7 = [  # 05:00, poranek 1…30: (ekran, linia uwagi)
    ("S6", ""),  # 1
    ("S6", ""),  # 2
    ("S6", ""),  # 3
    ("S6", ""),  # 4
    ("S6", ""),  # 5
    ("S6", ""),  # 6
    ("zdarzenie-E-30", ""),  # 7
    ("zdarzenie-E-30", ""),  # 8
    ("S6", WCIAZ_E30),  # 9
    ("S6", WCIAZ_E30),  # 10
    ("S6", WCIAZ_E30),  # 11
    ("S6", WCIAZ_E30),  # 12
    ("S6", WCIAZ_E30),  # 13
    ("zdarzenie-E-30", ""),  # 14
    ("zdarzenie-E-30", ""),  # 15
    ("S2B", WCIAZ_E30),  # 16
    ("S2A", WCIAZ_E30),  # 17
    ("S2B", WCIAZ_E30),  # 18
    ("S2A", WCIAZ_E30),  # 19
    ("S2B", WCIAZ_E30),  # 20
    ("zdarzenie-E-30", ""),  # 21
    ("zdarzenie-E-30", ""),  # 22
    ("S2A", WCIAZ_E30),  # 23
    ("S2B", WCIAZ_E30),  # 24
    ("S2B", WCIAZ_E30),  # 25
    ("S2A", WCIAZ_E30),  # 26
    ("S7", WCIAZ_E30),  # 27
    ("zdarzenie-E-30", BEZ_SENN),  # 28
    ("zdarzenie-E-30", BEZ_SENN),  # 29
    ("KM", WCIAZ_E30),  # 30
]

OCZEKIWANE = {"nowy-ahi12": AHI12, "nowy-wyciek18": WYCIEK18, "nowy-ahi7": AHI7}


@pytest.fixture(scope="module")
def zrzut(tmp_path_factory):
    """Zrzut z jedną nocą bez terapii w trybie AutoSet (nowy pacjent: brak nocy do normy i reguł)."""
    k = str(tmp_path_factory.mktemp("zrzut_nowy"))
    gen.generuj(k, wersja=0, nocy=1)
    p = os.path.join(k, "noce.csv")
    with open(p, encoding="utf-8") as f:
        w = f.read().splitlines()
    out = []
    for l in w:
        c = l.split(";")
        if c[0][:2] == "20":
            c[1], c[2], c[12] = "0", "0", "AutoSet/7/15/2"
            for i in (3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 18, 19):
                c[i] = ""
        out.append(";".join(c))
    with open(p, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    return k


@pytest.fixture(scope="module")
def przebiegi(zrzut, tmp_path_factory):
    return {n: sym.symuluj(zrzut, wyjscie=str(tmp_path_factory.mktemp("s-" + n)), cicho=True, scenariusze=[n])
            for n in scen.NOWY_PACJENT}


def poranki(k):
    """[(ekran bez przedrostka `dzien-`, linia uwagi bez źródła, reguły)] obrazków z 05:00, po kolei."""
    w = []
    for x in sym.klatki_pelne(k):
        if re.search(r"-\d{8}-0500-dzien-", x["plik"]):
            w.append((x["nazwa"][6:], re.sub(r" \([^()]*\)$", "", x["linia"].strip()), x["reguly"]))
    return w


def test_poza_pokaz_wszystko():
    assert set(scen.NOWY_PACJENT) <= set(scen.SCENARIUSZE) and not set(scen.NOWY_PACJENT) & set(scen.KOLEJNOSC)


@pytest.mark.parametrize("nazwa", scen.NOWY_PACJENT)
def test_dzien_po_dniu(przebiegi, nazwa):
    jest = [(e, l) for e, l, _ in poranki(przebiegi[nazwa])]
    oczek = OCZEKIWANE[nazwa]
    assert len(jest) == scen.NOWY_PACJENT_NOCE
    for i, (j, o) in enumerate(zip(jest, oczek), 1):
        assert j == o, f"{nazwa}, poranek {i}: jest {j}, oczekiwane {o}"


@pytest.mark.parametrize("nazwa", scen.NOWY_PACJENT)
def test_zapowiedzi_scenariusza(przebiegi, zrzut, nazwa):
    wynik = sym.sprawdz_oczekiwane(przebiegi[nazwa], scen.plan(zrzut, [nazwa]))
    assert wynik and all(j for _, j in wynik), [o for o, j in wynik if not j]


def test_wyciek18_bez_uspokojenia_i_z_dostawca(przebiegi):
    k = przebiegi["nowy-wyciek18"]
    p = poranki(k)
    # L-09 (dostawca sprzętu) od 14. poranka do końca; bezdechy nigdy nie ocenione (bez E-30, L-03)
    for i, (_, _, r) in enumerate(p, 1):
        assert sym.regula_w(r, "L-09") == (i >= 14), (i, r)
        assert not sym.regula_w(r, "L-03") and not sym.regula_w(r, "E-30"), (i, r)
    # od 4. poranka linia R-SKI-24 stoi w kolejce każdego ranka (ostrzeżenie: „nie oceniam”, nie „w porządku”)
    assert all(l == NIECZYTELNE for _, l, _ in p[3:])
    # nigdy karta pochwalna ani przedrostek „Noc w normie” (uspokojenie niespełnione przez linię i L-09)
    nazwy = [x["nazwa"] for x in sym.klatki_pelne(k)]
    assert not [n for n in nazwy if n.startswith("dzien-S2D")]
    with open(os.path.join(k, "log-panelu.txt"), encoding="utf-8") as f:
        assert "Noc w normie" not in f.read()


def test_ahi7_e30_bez_l03(przebiegi):
    p = poranki(przebiegi["nowy-ahi7"])
    for i, (_, _, r) in enumerate(p, 1):
        assert sym.regula_w(r, "E-30") == (i >= 7), (i, r)
        assert "L-03/ge10" not in r  # mediana 7/h: bez ekranu „pokaż lekarzowi”; L-03/5_10 tylko raport
    assert "wzorzec-L-03_ge10" not in [e for e, _, _ in p]
