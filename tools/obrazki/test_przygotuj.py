# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy tools/obrazki/przygotuj.py na obrazkach syntetycznych (bez obrazków użytkownika)."""
import json
import os
import sys

import pytest
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import przygotuj as pr  # noqa: E402


def czytaj_pbm(dane):
    assert dane[:3] == b"P4\n"
    naglowek, reszta = dane.split(b"\n", 2)[1], dane.split(b"\n", 2)[2]
    w, h = map(int, naglowek.split())
    return w, h, reszta


def test_pbm_bity_i_dopelnienie():
    im = Image.new("1", (10, 2), 1)
    im.putpixel((0, 0), 0)
    im.putpixel((9, 1), 0)
    w, h, d = czytaj_pbm(pr.pbm(im))
    assert (w, h) == (10, 2)
    assert d == bytes([0x80, 0x00, 0x00, 0x40])


def test_zmniejszanie_i_szarosc(tmp_path):
    wej = tmp_path / "wej"
    wej.mkdir()
    duzy = Image.new("L", (1600, 1600), 255)
    ImageDraw.Draw(duzy).ellipse((400, 400, 1200, 1200), fill=0)
    duzy.save(wej / "001_kolo.png")
    Image.new("1", (430, 430), 1).save(wej / "002_male.png")
    man = tmp_path / "obrazki.json"
    man.write_text(json.dumps({"wersja": 1, "opis": "prywatne", "obrazki": [
        {"plik": "001_kolo.png", "tekst": "tekst użytkownika", "dni": ["pn"], "pogoda": [], "sezon": ["zima"],
         "inne": [], "wulgarne": False},
        {"plik": "002_male.png", "dni": []},
        {"plik": "003_brak.png", "dni": ["wt"]}]}), encoding="utf-8")
    wyj = tmp_path / "panel"
    wynik = pr.przygotuj(str(wej), str(man), str(wyj))
    assert [p["plik"] for p in wynik] == ["001_kolo.pbm", "002_male.pbm"]
    w, h, _ = czytaj_pbm((wyj / "001_kolo.pbm").read_bytes())
    assert w <= 800 and h <= 434 and h == 434
    w2, h2, _ = czytaj_pbm((wyj / "002_male.pbm").read_bytes())
    assert (w2, h2) == (430, 430)
    m = json.loads((wyj / "obrazki.json").read_text(encoding="utf-8"))
    assert m["obrazki"][0] == {"plik": "001_kolo.pbm", "dni": ["pn"], "pogoda": [], "sezon": ["zima"], "inne": []}
    assert "opis" not in m and "tekst" not in json.dumps(m)  # bez pól użytkownika


def test_bez_manifestu(tmp_path):
    wej = tmp_path / "wej"
    wej.mkdir()
    Image.new("1", (20, 20), 0).save(wej / "a.png")
    (wej / "notatka.txt").write_text("x")
    wynik = pr.przygotuj(str(wej), None, str(tmp_path / "wyj"))
    assert [p["plik"] for p in wynik] == ["a.pbm"]
    assert wynik[0]["dni"] == []


def test_odmowa_zapisu_w_repo(tmp_path):
    with pytest.raises(SystemExit):
        pr.przygotuj(str(tmp_path), None, os.path.join(pr.REPO, "tmp-obrazki"))


def test_nazwa_jak_na_panelu():
    """nazwy jak przyjmuje komenda `W` panelu (src/magazyn.h nazwaObrazkaW)."""
    import re
    wzor = re.compile(r"[a-z0-9_-]{1,40}\.pbm\Z")
    assert pr.nazwa_panelu("001_kolo.png") == "001_kolo.pbm"
    assert pr.nazwa_panelu("Żółty Kot (2).JPG") == "zolty_kot__2_.pbm"
    assert pr.nazwa_panelu("a" * 60 + ".png") == "a" * 40 + ".pbm"
    assert pr.nazwa_panelu("..png") == "obrazek.pbm" or wzor.match(pr.nazwa_panelu("..png"))
    assert pr.nazwa_panelu("kot.png", {"kot.pbm"}) == "kot_2.pbm"
    assert pr.nazwa_panelu("kot.jpg", {"kot.pbm", "kot_2.pbm"}) == "kot_3.pbm"
    for n in ("Żółty Kot (2).JPG", "a" * 60 + ".png", "ąę.bmp", "x/y/z.png"):
        assert wzor.match(pr.nazwa_panelu(n)), n
