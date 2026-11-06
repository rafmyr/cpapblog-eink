# SPDX-License-Identifier: GPL-3.0-or-later
"""Teksty przerywników S2d (decyzja 05.11.2026; spec 5.7.6, R-HUM-21, R-HUM-28).

Plik źródłowy autora `docs/design/przerywniki-teksty.md` jest prawdą: 18 kart z warunkiem i 18 neutralnych.
Sprawdza, że `docs/spec/katalog/teksty.csv` ma każdy tekst dosłownie (tytuł = pierwsze zdanie, powód = reszta), że spec
funkcjonalna ma je w tabelach 5.7.6 i że warunki z pliku autora są w tabeli „Warunki kart”.

Uruchomienie: python3 -m pytest -q tools/test_przerywniki_s2d_teksty.py
"""
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLIK = os.path.join(REPO, "docs", "design", "przerywniki-teksty.md")
TEKSTY = os.path.join(REPO, "docs", "spec", "katalog", "teksty.csv")
SPEC = os.path.join(REPO, "docs", "spec", "specyfikacja-funkcjonalna.md")


def _plik():
    warunek, neutralna, nazwa = {}, {}, {}
    sekcja = ""
    with open(PLIK, encoding="utf-8") as f:
        for l in f.read().split("\n"):
            if l.startswith("## Karty z warunkiem"):
                sekcja = "w"
            elif l.startswith("## Karty neutralne"):
                sekcja = "n"
            elif re.match(r"^G\d\d ", l):
                if sekcja == "w":
                    scena, nazwa_sceny, war, tekst = [x.strip() for x in l.split(" | ")]
                    warunek[scena[:3]] = (war, tekst)
                    nazwa[scena[:3]] = nazwa_sceny
                else:
                    neutralna[l[:3]] = l[4:]
    return warunek, neutralna, nazwa


def _teksty_csv():
    t = {}
    with open(TEKSTY, encoding="utf-8") as f:
        for l in f.read().split("\n")[1:]:
            if l.startswith("S2D."):
                idt, _rola, _lim, tekst = l.split(";", 3)
                t[idt] = tekst
    return t


def _norm(s):
    return re.sub(r"\s+", " ", s)


def test_plik_autora_ma_36_tekstow():
    w, n, _ = _plik()
    assert sorted(w) == sorted(n) == ["G%02d" % i for i in range(1, 19)]
    assert all(t for _, t in w.values()) and all(n.values())


def test_teksty_csv_dosłownie_jak_w_pliku_autora():
    w, n, _ = _plik()
    t = _teksty_csv()
    assert len(t) == 71  # 37 tytułów (w tym G01 neutralna na marzec) i 34 powody (G05 neutralna i G16 z warunkiem mają cały tekst w tytule)
    for g in w:
        for nazwa, tekst in (("warunek", w[g][1]), ("neutralna", n[g])):
            tyt = t["S2D.%s.%s.tytul" % (g, nazwa)]
            pow_ = t.get("S2D.%s.%s.powod" % (g, nazwa), "")
            assert (tyt + " " + pow_).strip() == tekst, (g, nazwa)
            assert tyt.endswith(".") and ". " not in tyt  # tytuł = jedno, pierwsze zdanie


def test_g01_marzec_dosłownie_jak_w_pliku_autora():
    # decyzja 05.11.2026: osobna neutralna G01 na wiosenną zmianę czasu, jedno zdanie, bez powodu
    m = re.search(r"^Marzec: (.+)$", open(PLIK, encoding="utf-8").read(), re.M).group(1)
    t = _teksty_csv()
    assert m == "Dzisiaj śpisz godzinę krócej." == t["S2D.G01.neutralna_marzec.tytul"]
    assert "S2D.G01.neutralna_marzec.powod" not in t


def test_spec_ma_teksty_w_tabelach_5_7_6():
    w, n, _ = _plik()
    t = _teksty_csv()
    spec = _norm(open(SPEC, encoding="utf-8").read())
    for idt, tekst in t.items():
        assert "`%s`" % tekst in spec, idt


def test_spec_ma_warunki_z_pliku_autora():
    w, _, _ = _plik()
    spec = open(SPEC, encoding="utf-8").read()
    for g, (war, _t) in w.items():
        assert war in spec, (g, war)
        # wiersz tabeli kart z warunkiem: scena, warunek, tytuł
        assert re.search(r"\| %s [^|]+\| %s \|" % (g, re.escape(war)), spec), g


def test_stary_komplet_wycofany():
    t = _teksty_csv()
    assert not [k for k in t if re.fullmatch(r"S2D\.G\d\d\.(tytul|powod)", k)]
    spec = open(SPEC, encoding="utf-8").read()
    assert "Budzik wkłada kapcie" not in spec and "Na dzień dobry` | `Budzik" not in spec
