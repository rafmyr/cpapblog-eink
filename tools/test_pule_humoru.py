# SPDX-License-Identifier: GPL-3.0-or-later
"""Pule zdań budzika w firmware dosłownie ze spec funkcjonalnej 5.7.5 (R-HUM-12) i twarde zasady zdań.

Czyta tablicę PULE z firmware/prototyp/src/humor.cpp, pulę „noc” (ZDANIA_NOCY) z src/dobranoc.cpp, listę USPOKAJAJACE
i tabele „Pule”, „Pula „noc”” oraz „Zdania scen przerywników” ze spec: każde zdanie panelu ma być w spec w tej samej
kolumnie (przyjęte albo propozycje) i w tej samej kolejności (numer zdania to identyfikator w `/humor.csv`). Zasady 5.7.5
sprawdzane dla wszystkich zdań: ≤ 45 znaków, bez „masz” i „wiemy” (1.4 pkt 6), bez słów o alkoholu, THC, lekach,
senności, objawach i lekarzu, a na ekranach w tonie „problem” (S-K-10, S-K-16, S-K-31) bez zwrotu do użytkownika
w 2. osobie. Teksty spoza spec nie wchodzą (granica ADR-0002, zasady 5.7.5).
"""
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = os.path.join(REPO, "docs", "spec", "specyfikacja-funkcjonalna.md")
HUMOR = os.path.join(REPO, "firmware", "prototyp", "src", "humor.cpp")
DOBRANOC = os.path.join(REPO, "firmware", "prototyp", "src", "dobranoc.cpp")

WIERSZE = {"dobra noc": "dobra_noc", "seria": "seria", "kamień milowy": "kamien", "synchronizacja": "synchronizacja",
           "tydzień": "tydzien", "powrót do rytmu": "powrot", "wyciek": "wyciek", "przerwy w nocy": "przerwy",
           "sprzęt: zużycie": "zuzycie", "sprzęt: po wymianie": "po_wymianie", "wieczór: wietrzenie": "wietrzenie",
           "wieczór: smog": "smog", "wieczór: chłód": "chlod", "wieczór: suche powietrze": "suche_powietrze",
           "wieczór: upał": "upal", "wieczór: pyłek": "pylek", "wieczór: stała pora": "stala_pora",
           "wieczór: dziś znowu z maską": "z_maska", "wieczór: weekend": "weekend", "odkrycie": "odkrycie",
           "brak danych": "brak_danych", "przerywnik": "przerywnik"}


def pule_spec():
    s = open(SPEC, encoding="utf-8").read()
    a = s.index("| Typ ekranu | Ekrany | Przyjęte")
    b = s.index("**Pula „noc”", a)
    w = {}
    for linia in s[a:b].splitlines()[2:]:
        c = [x.strip() for x in linia.strip().strip("|").split("|")]
        if len(c) >= 4 and c[0] in WIERSZE:
            w[WIERSZE[c[0]]] = (re.findall(r"`([^`]+)`", c[2]), re.findall(r"`([^`]+)`", c[3]))
    w["przerywnik_sceny"] = ([z for _, z in sceny_g_spec()], [])
    return w


def sceny_g_spec():
    """Wiersze tabeli „Zdania scen przerywników”: (etykieta sceny `G04`, zdanie)."""
    s = open(SPEC, encoding="utf-8").read()
    a = s.index("| Scena | Zdanie budzika (przyjęte")
    g = []
    for linia in s[a:].splitlines()[2:]:
        if not linia.startswith("| G"):
            break
        c = linia.split("|")
        g.append((c[1].strip().split()[0], re.findall(r"`([^`]+)`", c[2])[0]))
    return g


def sceny_g_firmware():
    s = open(HUMOR, encoding="utf-8").read()
    a = s.index("static const std::vector<const char*> SCENY_G = {")
    return re.findall(r'"(G\d\d)"', s[a:s.index("};", a)])


def noc_spec():
    s = open(SPEC, encoding="utf-8").read()
    a = s.index("| Scena S9 | Opis sceny | Zdanie budzika")
    w = []
    for linia in s[a:].splitlines()[2:]:
        if not linia.startswith("| "):
            break
        c = [x.strip() for x in linia.strip().strip("|").split("|")]
        w.append((c[0].split()[0], re.findall(r"`([^`]+)`", c[2])[0], int(c[3])))
    return w


def noc_firmware():
    s = open(DOBRANOC, encoding="utf-8").read()
    a = s.index("const std::vector<ZdanieSceny> ZDANIA_NOCY = {")
    b = s.index("\n};", a)
    return re.findall(r'\{"(\w+)", "([^"]+)"\}', s[a:b])


def uspokajajace_firmware():
    s = open(HUMOR, encoding="utf-8").read()
    a = s.index("static const char* const USPOKAJAJACE[] = {")
    b = s.index("};", a)
    return re.findall(r'"([^"]*)"', re.sub(r"//[^\n]*", "", s[a:b]))


def uspokajajace_spec():
    s = open(SPEC, encoding="utf-8").read()
    a = s.index("**Zdania uspokajające**")
    return re.findall(r"`([^`]+)`", s[a:s.index(". Trzy zdania", a)])


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
    assert len(pule_firmware()["przerywnik_sceny"][0]) == 18


def test_etykiety_scen_g_zgodne_z_kolejnoscia_scen_g():
    # zdanie numer i w `przerywnik_sceny` jest przypięte do SCENY_G[i] (zdanieSceny()), więc wiersz i tabeli
    # w spec musi mieć etykietę tej sceny; SCENY_G to G01…G18 po kolei
    sg = sceny_g_firmware()
    assert sg == ["G%02d" % (i + 1) for i in range(18)]
    spec = sceny_g_spec()
    assert len(spec) == len(sg)
    for i, (et, _) in enumerate(spec):
        assert et == sg[i], (i, et, sg[i])


def test_pula_noc_dosłownie_ze_spec():
    spec = noc_spec()
    assert [(sc, z) for sc, z, _ in spec] == noc_firmware()
    for sc, z, n in spec:
        assert n == len(z), (sc, z, n)


def test_uspokajajace_dosłownie_ze_spec_i_w_pulach():
    fw = uspokajajace_firmware()
    assert fw == uspokajajace_spec()
    wszystkie = {z for k, p in pule_firmware().values() for z in k + p}
    for z in fw:
        assert z in wszystkie, z


def wszystkie_zdania():
    for id_, (k, p) in pule_firmware().items():
        for z in k + p:
            yield id_, z
    for sc, z in noc_firmware():
        yield "noc:" + sc, z


ZAKAZANE = [r"masz", r"wiemy", r"alkohol\w*", r"piw\w*", r"wódk\w*", r"wino", r"THC", r"lek(i|ów|ami|u)?", r"tabletk\w*",
            r"senn\w*", r"bezsenn\w*", r"ziew\w*", r"lekarz\w*", r"objaw\w*"]
# 2. osoba liczby pojedynczej (zaimki i typowe końcówki czasowników) na pulach ekranów w tonie „problem”
DRUGA_OSOBA = [r"ty", r"cię", r"ciebie", r"tobie", r"tobą", r"ci", r"twó[jm]\w*", r"twoj\w*", r"\w+(esz|isz|ysz|asz)",
               r"\w+(łeś|łaś)", r"zrób", r"sprawdź", r"popraw", r"zmień", r"pamiętaj"]
PULE_PROBLEM = ["wyciek", "przerwy", "zuzycie"]


def test_zasady_zdan_budzika():
    zdania = list(wszystkie_zdania())
    assert len(zdania) >= 150
    for id_, z in zdania:
        assert len(z) <= 45, (id_, z)
        for w in ZAKAZANE:
            assert not re.search(r"(?<!\w)" + w + r"(?!\w)", z, re.I), (id_, z, w)
    for id_ in PULE_PROBLEM:
        k, p = pule_firmware()[id_]
        assert k + p, id_
        for z in k + p:
            for w in DRUGA_OSOBA:
                assert not re.search(r"(?<!\w)" + w + r"(?!\w)", z, re.I), (id_, z, w)


def test_po_przyjeciu_puli_propozycji_brak():
    # pula przyjęta 08.11.2026: wszystkie zdania w liście przyjętych, listy propozycji puste
    for id_, (k, p) in pule_firmware().items():
        assert p == [] and len(k) >= 5, id_
