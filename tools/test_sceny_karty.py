# SPDX-License-Identifier: GPL-3.0-or-later
"""Pakiet scen karty (spec funkcjonalna 5.7.2 „Przygotowanie scen”, 5.7.4, R-HUM-11).

Każda scena źródłowa ma bitmapę karty ≤ 240×205, 1-bit; nowe bitmapy wychodzą z tools/prototyp/sceny_karty.py bit
w bit; include/obrazy.h ma wszystkie sceny karty.
"""
import importlib.util
import os
import re

import pytest

PIL = pytest.importorskip("PIL")
pytest.importorskip("numpy")
from PIL import Image  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def modul():
    p = os.path.join(REPO, "tools", "prototyp", "sceny_karty.py")
    spec = importlib.util.spec_from_file_location("sceny_karty", p)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def test_komplet_70_scen():
    m = modul()
    assert len(m.zrodla()) == 70
    assert m.braki() == []


def test_bitmapy_w_slocie_karty_i_1_bit():
    m = modul()
    for n, p in m.generuj.sceny().items():
        im = Image.open(p)
        assert im.size[0] <= 240 and im.size[1] <= 205, n
        assert set(im.convert("L").tobytes()) <= {0, 255}, n


def test_bitmapa_z_metody_przygotowania():
    m = modul()
    # sceny dodane (D01 z K22, M05 z kamienia 365, G13 z przerywnika) = wynik sprite() bit w bit
    for n in ("D01", "M05", "G13"):
        zr = m.zrodla()[n]
        assert m.karta(zr).tobytes() == Image.open(m.plik_karty(zr)).convert("1").tobytes(), n


def test_obrazy_h_ma_wszystkie_sceny():
    h = open(os.path.join(REPO, "firmware", "prototyp", "include", "obrazy.h"), encoding="utf-8").read()
    a = h.index("static const SceneBudzika SCENY[] = {")
    ids = re.findall(r'\{"([A-Z]\d\d)", BUDZIK_', h[a:h.index("};", a)])
    assert len(ids) == 70 and sorted(ids) == sorted(modul().zrodla())
