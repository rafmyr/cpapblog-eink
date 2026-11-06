# SPDX-License-Identifier: GPL-3.0-or-later
"""Katalog ekranów kontekstowych docs/spec/katalog/SK.csv i progi w kodzie.

Sprawdza: każdy niewycofany S-K z tabeli 5.8.3 ma wiersz; teksty wierszy są w teksty.csv; `zablokowana` ma powód;
rodzaj jest zadeklarowany w rodzaje-sk.csv; implementacja wskazuje istniejący plik; stałe `// próg: id = wartość`
w src/sprzet.h, src/infekcja.h, src/kontekst_sk.h są równe wartościom w docs/spec/progi.csv.
"""
import csv
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KAT = os.path.join(REPO, "docs", "spec", "katalog")
SRC = os.path.join(REPO, "firmware", "prototyp", "src")


def wiersze(nazwa):
    with open(os.path.join(KAT, nazwa), encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter=";"))


def test_kazdy_sk_z_katalogu_spec_ma_wiersz():
    with open(os.path.join(REPO, "docs", "spec", "specyfikacja-funkcjonalna.md"), encoding="utf-8") as f:
        spec = f.read()
    sekcja = spec[spec.index("#### 5.8.3"):spec.index("#### 5.8.4")]
    ids = set(re.findall(r"^\| (S-K-\d\d) \|", sekcja, re.M))
    assert len(ids) >= 30
    jest = set(w["id"] for w in wiersze("SK.csv"))
    assert ids - jest == set()


def test_teksty_rodzaje_stan_implementacja():
    teksty = set(w["id_tekstu"] for w in wiersze("teksty.csv"))
    rodzaje = set(w["nazwa"] for w in wiersze("rodzaje-sk.csv")) | {"kamien"}
    for w in wiersze("SK.csv"):
        g = "%s/%s" % (w["id"], w["wariant"])
        for t in filter(None, w["teksty"].split(",")):
            assert t in teksty, "%s: tekst %s nie w teksty.csv" % (g, t)
        assert w["rodzaj"] in rodzaje, g
        assert w["stan"] in ("aktywna", "tylko_raport", "zablokowana"), g
        if w["stan"] == "zablokowana":
            assert w["powod_blokady"].strip(), "%s: blokada bez powodu" % g
        else:
            plik = w["implementacja"].split(" ")[0]
            assert os.path.exists(os.path.join(REPO, "firmware", "prototyp", plik)), g


def progi():
    with open(os.path.join(REPO, "docs", "spec", "progi.csv"), encoding="utf-8", newline="") as f:
        p = {w["id_progu"]: w["wartosc"] for w in csv.DictReader(f)}

    def wartosc(i, glebokosc=0):
        v = p[i].strip()
        if v.startswith("=") and glebokosc < 5:
            return wartosc(v[1:], glebokosc + 1)
        return v
    return p, wartosc


def test_progi_w_kodzie_rowne_progi_csv():
    p, wartosc = progi()
    n = 0
    for plik in ("sprzet.h", "infekcja.h", "kontekst_sk.h"):
        with open(os.path.join(SRC, plik), encoding="utf-8") as f:
            for m in re.finditer(r"// próg: (\S+) = (\S+)", f.read()):
                idp, w = m.group(1), m.group(2)
                assert idp in p, "%s: brak progu %s w progi.csv" % (plik, idp)
                v = wartosc(idp)
                if "/" in v:
                    assert v == w, (idp, v, w)
                else:
                    assert abs(float(v) - float(w)) < 1e-9, (idp, v, w)
                n += 1
    assert n >= 50
