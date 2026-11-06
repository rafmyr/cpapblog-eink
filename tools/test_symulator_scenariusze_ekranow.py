# SPDX-License-Identifier: GPL-3.0-or-later
"""Symulator doby, scenariusze ekranów (sprzet, infekcja, wyjazd, wyciek-maski,
regularnosc, odkrycia, autoset, cpap) i zwinięte powtórzenia w spis.txt. Na zrzucie syntetycznym
(tools/symulator/generuj_zrzut.py); zapowiedzi każdego scenariusza sprawdza tools/test_symulator_scenariusze.py
(parametryzacja po scen.KOLEJNOSC), tu: mechanizmy dołożone.
"""
import importlib.util
import os

import pytest

TU = os.path.dirname(os.path.abspath(__file__))


def modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa + "_b7", os.path.join(TU, "symulator", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


sym = modul("symuluj")
gen = modul("generuj_zrzut")
scen = sym.scen

NOWE = ("sprzet", "infekcja", "wyjazd", "wyciek-maski", "regularnosc", "odkrycia", "autoset", "cpap")


@pytest.fixture(scope="module")
def zrzut(tmp_path_factory):
    k = str(tmp_path_factory.mktemp("zrzut_b7"))
    gen.generuj(k, wersja=0, nocy=60)
    return k


def test_nowe_scenariusze_w_pokaz_wszystko():
    for n in NOWE:
        assert n in scen.KOLEJNOSC
        f, opis, rozbieg, *_ = scen.SCENARIUSZE[n]
        assert opis and rozbieg


def test_plany_nowych_scenariuszy(zrzut):
    for n in NOWE:
        p = scen.plan(zrzut, [n])
        assert p.oczekiwane and p.od < p.do and p.noce, n
        # noce scenariusza po ostatniej nocy zrzutu, czynniki tylko dla nocy sprzed początku okresu (przed startem)
        assert all(c[:10] < f"{p.od:%Y-%m-%d}" for c in p.czynniki), n
    s = scen.plan(zrzut, ["sprzet"])
    assert any(k.startswith("wymiana.poduszka=") for k in s.konfiguracja)
    w = scen.plan(zrzut, ["wyjazd"])
    assert len(w.przerwy) == 2 and all(d - o > scen.dt.timedelta(days=2) for o, d in w.przerwy)
    o = scen.plan(zrzut, ["odkrycia"])
    # „Co znaczą te liczby” (R-EKR-51) da się wywołać (scenariusz profil-publiczny), więc bez znaleziska
    assert len(o.czynniki) == len(scen.ODKRYCIA_HISTORIA) and not [z for z in o.znaleziska if "R-EKR-51" in z]


def test_tryb_scenariusza_w_rozbiegu(zrzut):
    # autoset i cpap: rozbieg też w swoim trybie (wzór ze SCENARIUSZE), więc K1 i K2 bez wierszy ASV
    for n, tryb in (("autoset", "AutoSet"), ("cpap", "CPAP")):
        p = scen.plan(zrzut, [n])
        assert {x.split(";")[40] for x in p.noce} == {tryb}, n


def test_spis_zwija_przerysowania():
    # zwijane tylko obrazki o identycznej treści (skrót bajtów PNG) i tylko w obrębie jednego dnia
    spis = [(f"SYN-{i:03d}-20261129-{5 + i:02d}00-dzien-SK33.png", "dzien-SK33", "", "") for i in range(1, 6)]
    spis += [("SYN-006-20261129-1100-K1.png", "K1", "", ""), ("SYN-007-20261129-1100-K1.png", "K1", "", "")]
    sk = {x[0]: "a" for x in spis}
    t = sym.tekst_spisu(spis, skroty=sk)
    assert "SYN-001-20261129-0600-dzien-SK33.png … SYN-005-20261129-1000-dzien-SK33.png" in t
    assert "5 obrazków o identycznej treści" in t and "29.11 06:00 – 29.11 10:00" in t
    assert t.count("kokpit K1") == 2  # ekrany z przycisków nie są zwijane
    assert " … " not in sym.tekst_spisu(spis[:2], skroty=sk)
    assert " … " not in sym.tekst_spisu(spis)  # bez skrótów nic
    sk2 = dict(sk, **{spis[2][0]: "b"})  # inna treść w środku: zwinięte tylko 001…002? nie (2 < ZWIN_OD), 004…005 też
    assert " … " not in sym.tekst_spisu(spis, skroty=sk2)
    dni = [(f"SYN-00{i}-2026112{7 + i // 2}-{11 + i:02d}01-dzien-S5.png", "dzien-S5", "", "") for i in range(4)]
    assert "20261127-1201-dzien-S5.png … " not in sym.tekst_spisu(dni, skroty={x[0]: "a" for x in dni})


def test_zwiniete_w_przebiegu_maja_te_sama_tresc(zrzut, tmp_path):
    import re
    k = sym.symuluj(zrzut, wyjscie=str(tmp_path / "z"), cicho=True, scenariusze=["wyciek-maski"])
    sk = sym.skroty_png(k)
    with open(os.path.join(k, "spis.txt"), encoding="utf-8") as f:
        spis = f.read()
    pliki = sorted(sk)
    zakresy = re.findall(r"(\S+\.png) … (\S+\.png) — ", spis)
    for a, b in zakresy:
        assert len({sk[p] for p in pliki if a <= p <= b}) == 1, (a, b)
        assert a.split("-")[2] == b.split("-")[2]  # ten sam dzień


def test_autoset_raport_pdf_i_konfiguracja(zrzut, tmp_path):
    k = sym.symuluj(zrzut, wyjscie=str(tmp_path / "a"), cicho=True, scenariusze=["autoset"])
    pdf = [x for x in os.listdir(k) if x.endswith(".pdf")]
    assert pdf and all(x.startswith("SYN-raport-") for x in pdf)
    with open(os.path.join(k, pdf[0]), "rb") as f:
        assert f.read(5) == b"%PDF-"
    with open(os.path.join(k, "log.txt"), encoding="utf-8") as f:
        assert "Raport dla lekarza z pamięci panelu" in f.read()
    k2 = sym.symuluj(zrzut, wyjscie=str(tmp_path / "s"), cicho=True, scenariusze=["sprzet"], bez_prognozy=True)
    with open(os.path.join(k2, "log.txt"), encoding="utf-8") as f:
        assert "Konfiguracja panelu ze scenariusza (/konfiguracja.txt): wymiana.poduszka=" in f.read()


def test_nie_ma_i_dzien_z(tmp_path):
    k = str(tmp_path)
    with open(os.path.join(k, "log-panelu.txt"), "w", encoding="utf-8") as f:
        for nazwa, plik in (("dzien-odkrycie-O-12_A", "SYN-001-20261201-0500-dzien-odkrycie-O-12_A.png"),
                            ("K1", "SYN-002-20261201-0510-K1.png"), ("K1", "SYN-003-20261202-0510-K1.png"),
                            ("dzien-pytanie-infekcja", "SYN-004-20261203-0500-dzien-pytanie-infekcja.png")):
            f.write(f"2026-12-01 05:00:00  cpap-eink 1.0, wybudzenie zegar, licznik 1\n"
                    f"2026-12-01 05:00:00  EKRAN {nazwa} {plik}\n")
    p = scen.Plan()
    p.oczekiwane = [("ekran", "K1", "K1 po odkryciu", (("dzien_z", "dzien-odkrycie-O-12"),)),
                    ("nie_ma", "dzien-pytanie-infekcja", "bez S-K-30 od 4.12", (("od", "20261204"),)),
                    ("nie_ma", "dzien-pytanie-infekcja", "bez S-K-30 od 3.12", (("od", "20261203"),))]
    assert [j for _, j in sym.sprawdz_oczekiwane(k, p)] == [True, True, False]


def test_odkrycia_rekord_ponizej_historii_zrzutu(tmp_path):
    # zrzut z bardzo niskim AHI w historii (0,25/h): tydzień rekordu O-03 i tak niższy, więc O-03 jest
    k = str(tmp_path / "zrzut")
    gen.generuj(k, wersja=0, nocy=60)
    p = os.path.join(k, "noce.csv")
    with open(p, encoding="utf-8") as f:
        w = f.read().splitlines()
    nowe = []
    for l in w:
        c = l.split(";")
        if len(c) > 3 and c[0][:2] == "20" and c[3]:
            c[3] = "0.25"
        nowe.append(";".join(c))
    with open(p, "w", encoding="utf-8") as f:
        f.write("\n".join(nowe) + "\n")
    plan = scen.plan(k, ["odkrycia"])
    rekord = float(plan.noce[-1].split(";")[3])
    assert 0 <= rekord < 0.25 and not [z for z in plan.znaleziska if z.startswith("O-03")]
    wy = sym.symuluj(k, wyjscie=str(tmp_path / "w"), cicho=True, scenariusze=["odkrycia"], bez_prognozy=True)
    wynik = dict(sym.sprawdz_oczekiwane(wy, plan))
    assert [j for o, j in wynik.items() if "O-03" in o] == [True]
    assert scen.min_mediana7([1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 0]) == 0
