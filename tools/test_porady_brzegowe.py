# SPDX-License-Identifier: GPL-3.0-or-later
"""Przypadki brzegowe porad „Dobry start” (R-DS-36…R-DS-41, R-DS-52, R-DS-53): każde zepsucie katalogu w kopii
w katalogu tymczasowym ma zatrzymać budowę zestawu błędem z ID porady; dwie budowy dają ten sam wynik; teksty w
zestawie wbudowanym firmware są bajt w bajt tekstami z porady.csv (po twardych spacjach)."""

import csv
import io
import os
import random
import re
import shutil
import sys

import pytest

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TU)
sys.path.insert(0, os.path.join(TU, "zestaw"))
import porady  # noqa: E402

KATALOG = os.path.join(REPO, "docs", "spec", "katalog")
NAGLOWEK = os.path.join(REPO, "firmware", "prototyp", "src", "zestaw_wbudowany.h")


def _wczytaj():
    with open(os.path.join(KATALOG, "porady.csv"), encoding="utf-8", newline="") as f:
        t = f.read()
    r = csv.DictReader(io.StringIO(t), delimiter=";")
    return r.fieldnames, list(r)


def _zbuduj(tmp_path, zmiana):
    pola, wiersze = _wczytaj()
    wiersze = zmiana(wiersze) or wiersze
    d = tmp_path / "katalog"
    d.mkdir()
    shutil.copy(os.path.join(KATALOG, "porady-ankieta.csv"), d / "porady-ankieta.csv")
    with open(d / "porady.csv", "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=pola, delimiter=";", lineterminator="\n")
        w.writeheader()
        w.writerows(wiersze)
    return porady.buduj_porady(str(d))


def _por(wiersze, i):
    return next(p for p in wiersze if p["id"] == i)


def test_kopia_bez_zmian_przechodzi(tmp_path):
    assert _zbuduj(tmp_path, lambda w: None) == porady.buduj_porady(KATALOG)


def test_dwie_budowy_identyczne():
    assert porady.buduj_porady(KATALOG) == porady.buduj_porady(KATALOG)


def test_tytul_na_granicy_szerokosci(tmp_path):
    kroje = porady.wczytaj_kroje()
    t = "Tytuł"
    while kroje["SB44"].szer(t + "l") <= porady.KOLUMNA:
        t += "l"
    assert kroje["SB44"].szer(t) <= porady.KOLUMNA < kroje["SB44"].szer(t + "l")
    _zbuduj(tmp_path / "a", lambda w: _por(w, "DS-D02").update(tytul=t))  # na granicy: przechodzi
    with pytest.raises(porady.BladPorad, match="DS-D02: tytuł"):
        _zbuduj(tmp_path / "b", lambda w: _por(w, "DS-D02").update(tytul=t + "l"))


@pytest.fixture
def tmp_path(tmp_path_factory, request):
    p = tmp_path_factory.mktemp(re.sub(r"\W", "_", request.node.name)[:40])
    (p / "a").mkdir()
    (p / "b").mkdir()
    return p


def test_tresc_szesc_linii(tmp_path):
    def z(w):
        p = _por(w, "DS-D02")
        p["tresc"] = " ".join([p["tresc"]] * 4)
    with pytest.raises(porady.BladPorad, match=r"DS-D02: treść \d+ linii > 5"):
        _zbuduj(tmp_path, z)


def test_karta_piec_linii(tmp_path):
    def z(w):
        p = _por(w, "DS-D07")
        p["tresc"] = " ".join(["Słowo"] * 80)
    # wersja 2 (R-DS-64): karta z rzędem faktów ma treść do 2 linii
    with pytest.raises(porady.BladPorad, match=r"DS-D07: treść \d+ linii > 2"):
        _zbuduj(tmp_path, z)


def test_krok_z_ikonami_za_szeroki(tmp_path):
    with pytest.raises(porady.BladPorad, match="DS-D26: krok"):
        _zbuduj(tmp_path, lambda w: _por(w, "DS-D26").update(krok="→ " + "▶ " * 30))


def test_nieznany_prog(tmp_path):
    with pytest.raises(porady.BladPorad, match=r"DS-D22: .*X-99\.brak"):
        _zbuduj(tmp_path, lambda w: _por(w, "DS-D22").update(tresc=_por(w, "DS-D22")["tresc"] + " {prog:X-99.brak}"))


def test_powtorzone_id(tmp_path):
    with pytest.raises(porady.BladPorad, match="powtórzone ID"):
        _zbuduj(tmp_path, lambda w: w + [dict(_por(w, "DS-Z04"))])


def test_dziura_w_planie_asv(tmp_path):
    with pytest.raises(porady.BladPorad, match="start_rano doba 1 w trybie ASV"):  # wersja 2: doby faz (R-DS-71)
        _zbuduj(tmp_path, lambda w: [p for p in w if p["id"] != "DS-D01.asv"])


def test_dwie_porady_na_miejscu_wszystkie_i_cpap(tmp_path):
    def z(w):
        n = dict(_por(w, "DS-D02"))
        n.update(id="DS-D99", tryb="CPAP")
        return w + [n]
    with pytest.raises(porady.BladPorad, match="start_rano doba 2 w trybie CPAP"):  # wersja 2: doby faz (R-DS-71)
        _zbuduj(tmp_path, z)


@pytest.mark.parametrize("tekst", ["Ustaw EPR 2.", "Możesz sam obniżyć ciśnienie.", "Ustaw minimum na 7 cmH2O.",
                                   "Wejdź w menu aparatu i zmień tryb."])
def test_adr2_odrzuca(tmp_path, tekst):
    with pytest.raises(porady.BladPorad, match="DS-D02: test ADR-0002"):
        _zbuduj(tmp_path, lambda w: _por(w, "DS-D02").update(tresc=tekst))


def test_znak_spoza_krojow(tmp_path):
    with pytest.raises(porady.BladPorad, match="znak spoza"):
        _zbuduj(tmp_path, lambda w: _por(w, "DS-D02").update(tytul="Uśmiech ☺"))


def _wbudowane():
    t = open(NAGLOWEK, encoding="utf-8").read()
    m = re.search(r'static const char PORADY\[\] = R"zestaw\((.*?)\)zestaw";', t, re.S)
    assert m
    linie = m.group(1).split("\n")
    pola = linie[1].split(";")
    return {w.split(";")[0]: dict(zip(pola, w.split(";"))) for w in linie[2:] if w}


def test_teksty_panelu_bajt_w_bajt_z_csv():
    _, wiersze = _wczytaj()
    wb = _wbudowane()
    assert len(wb) == len(wiersze) == 128  # wersja 2: 128 porad (spec 5.8.14, tabela 5.4; DS-D40.asv i DS-D40.asvbez)
    los = random.Random(22112026).sample(wiersze, 10) + [p for p in wiersze if "." in p["id"]]
    for p in los:
        for pole in ("tytul", "tresc", "krok"):
            assert wb[p["id"]][pole] == porady.twarde(p[pole].strip()), (p["id"], pole)
        for pole in ("rodzaj", "dzien", "tryb", "wymaga", "ekran", "warunek", "flagi", "zamiast", "wymaga_porady"):
            assert wb[p["id"]][pole] == p[pole], (p["id"], pole)
