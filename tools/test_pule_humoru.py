# SPDX-License-Identifier: GPL-3.0-or-later
"""Pule zdań budzika w firmware dosłownie ze spec funkcjonalnej 5.7.5 (R-HUM-12).

Czyta tablicę PULE z firmware/prototyp/src/humor.cpp i tabele „Pule” oraz „Zdania scen przerywników” ze spec:
każde zdanie panelu ma być w spec w tej samej kolumnie (kanwa albo propozycje) i w tej samej kolejności (numer zdania
to identyfikator w `/humor.csv`), każde ≤ 45 znaków. Teksty spoza spec nie wchodzą (granica ADR-0002, zasady 5.7.5).
"""
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = os.path.join(REPO, "docs", "spec", "specyfikacja-funkcjonalna.md")
HUMOR = os.path.join(REPO, "firmware", "prototyp", "src", "humor.cpp")

WIERSZE = {"dobra noc": "dobra_noc", "seria": "seria", "kamień milowy": "kamien", "synchronizacja": "synchronizacja",
           "tydzień": "tydzien", "powrót do rytmu": "powrot", "wyciek": "wyciek", "przerwy w nocy": "przerwy",
           "sprzęt: zużycie": "zuzycie", "sprzęt: po wymianie": "po_wymianie", "wieczór: wietrzenie": "wietrzenie",
           "wieczór: smog": "smog", "wieczór: chłód": "chlod", "wieczór: suche powietrze": "suche_powietrze",
           "wieczór: upał": "upal", "wieczór: pyłek": "pylek", "wieczór: stała pora": "stala_pora",
           "wieczór: dziś znowu z maską": "z_maska", "wieczór: weekend": "weekend", "odkrycie": "odkrycie",
           "brak danych": "brak_danych", "przerywnik": "przerywnik"}


def pule_spec():
    s = open(SPEC, encoding="utf-8").read()
    a = s.index("| Typ ekranu | Ekrany | Z v0.6c i kanwy |")
    b = s.index("**Pula „noc”", a)
    w = {}
    for linia in s[a:b].splitlines()[2:]:
        c = [x.strip() for x in linia.strip().strip("|").split("|")]
        if len(c) >= 4 and c[0] in WIERSZE:
            w[WIERSZE[c[0]]] = (re.findall(r"`([^`]+)`", c[2]), re.findall(r"`([^`]+)`", c[3]))
    a = s.index("| Scena | Zdanie budzika (propozycja")
    g = []
    for linia in s[a:].splitlines()[2:]:
        if not linia.startswith("| G"):
            break
        g.append(re.findall(r"`([^`]+)`", linia.split("|")[2])[0])
    w["przerywnik_sceny"] = ([], g)
    return w


def pule_firmware():
    s = open(HUMOR, encoding="utf-8").read()
    a = s.index("static const Pula PULE[] = {")
    b = s.index("\n};", a)
    tresc = re.sub(r"//[^\n]*", "", s[a:b])
    w = {}
    for m in re.finditer(r'\{"(\w+)",\s*\{(.*?)\},\s*\{(.*?)\}\}', tresc, re.S):
        w[m.group(1)] = (re.findall(r'"([^"]*)"', m.group(2)), re.findall(r'"([^"]*)"', m.group(3)))
    return w


def test_kazda_pula_dosłownie_ze_spec():
    spec, fw = pule_spec(), pule_firmware()
    assert set(fw) == set(spec), (sorted(set(fw) ^ set(spec)))
    for id_, (k, p) in fw.items():
        assert k == spec[id_][0], id_ + ": kanwa"
        assert p == spec[id_][1], id_ + ": propozycje"


def test_zdania_w_limicie_45_znakow():
    for id_, (k, p) in pule_firmware().items():
        for z in k + p:
            assert len(z) <= 45, (id_, z)


def test_przerywniki_maja_zdanie_dla_kazdej_sceny_g():
    assert len(pule_firmware()["przerywnik_sceny"][1]) == 18
