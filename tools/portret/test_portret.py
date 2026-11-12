# SPDX-License-Identifier: GPL-3.0-or-later
"""T-TEST-413: wzorzec Portretu nocy na syntetycznej karcie ze znanymi nocami (karta_portretu.py).

Uruchomienie: python3 -m pytest -q tools/portret
"""

import math
import os
import shutil
import sys
import tempfile

import pytest

_TU = os.path.dirname(os.path.abspath(__file__))
_KAL = os.path.join(os.path.dirname(_TU), "kalibracja")
for _p in (_TU, _KAL):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import karta_portretu  # noqa: E402
import kroje  # noqa: E402
import noce  # noqa: E402
import portret  # noqa: E402
import przebieg  # noqa: E402
import rysunek  # noqa: E402

CX = rysunek.X_TARCZY
CY = (rysunek.GORA_TRESCI + rysunek.DOL_TRESCI) // 2


@pytest.fixture(scope="module")
def karta():
    k = tempfile.mkdtemp(prefix="portret-")
    karta_portretu.utworz(os.path.join(k, "karta"))
    lista = noce.noce(os.path.join(k, "karta"))
    for r in lista:
        r["_katalog"] = os.path.join(k, "karta", "DATALOG", noce.data_z_dnia(r["day"]).strftime("%Y%m%d"))
    yield lista
    shutil.rmtree(k)


def _noc(lista, rano, **kw):
    i = next(i for i, r in enumerate(lista) if noce.data_z_dnia(r["day"] + 1).isoformat() == rano)
    return portret.dane_nocy(lista, i, **kw)


def _u(g, m):
    """Godzina zegarowa → minuty od 12:00."""
    return (g * 60 + m - 720) % 1440


def _punkt(u_s, r):
    a = (u_s % 43200) / 43200.0 * 2 * math.pi
    return CX + int(math.floor(r * math.sin(a) + 0.5)), CY - int(math.floor(r * math.cos(a) + 0.5))


def test_pik_w_znanej_godzinie(karta):
    d = _noc(karta, "2026-11-02")
    x = d["przebieg"]["odcinki"][_u(4, 5) // 5]
    assert x["zew"] == 5
    assert rysunek.wysokosc_zewn(d, x) == pytest.approx(36 * (0.35 + 0.65 * 4 / 6))  # 28,2 px
    o = rysunek.rysuj(d)
    srodek = _u(4, 7) * 60 + 30
    assert o.czarny(*_punkt(srodek, 134 + 25))
    assert not o.czarny(*_punkt(srodek, 134 + 31))
    # 1 OA o 01:30 → pik 12,6 px (≥ 12 px)
    x = d["przebieg"]["odcinki"][_u(1, 30) // 5]
    assert x["zew"] == 1 and rysunek.wysokosc_zewn(d, x) == pytest.approx(12.6)
    # RERA o 03:40 bez piku
    assert d["przebieg"]["odcinki"][_u(3, 40) // 5]["zew"] == 0


def test_pik_do_srodka_nie_schodzi_ponizej_98(karta):
    d = _noc(karta, "2026-11-02")
    x = d["przebieg"]["odcinki"][_u(2, 0) // 5]
    assert x["centr"] == 2 and rysunek.wysokosc_wewn(d, x) == pytest.approx(10.8333, abs=1e-3)
    # skrajny przypadek: 12 centralnych → 20 px, najniżej promień 98
    x["centr"] = 12
    assert rysunek.wysokosc_wewn(d, x) == pytest.approx(20.0)
    o = rysunek.rysuj(d)
    srodek = _u(2, 2) * 60 + 30
    assert o.czarny(*_punkt(srodek, 99))
    for r in (95, 96, 97):
        assert not o.czarny(*_punkt(srodek, r))


def test_bialy_odcinek_w_znanym_przedziale(karta):
    d = _noc(karta, "2026-11-02")
    odc = d["przebieg"]["odcinki"]
    for g in range(0, 30, 5):
        assert odc[_u(3, g) // 5]["wyciek95"] == 18
    assert odc[_u(3, 30) // 5]["wyciek95"] == 6
    o = rysunek.rysuj(d)
    assert not o.czarny(*_punkt(_u(3, 12) * 60, 126))      # biały środek
    assert o.czarny(*_punkt(_u(3, 12) * 60, 119))          # czarny obrys
    assert o.czarny(*_punkt(_u(3, 40) * 60, 126))
    assert "wyciek" in rysunek.legenda(d)


def test_czas_wycieku_z_probek(karta):
    d = _noc(karta, "2026-11-02")
    x = d["przebieg"]["odcinki"][_u(5, 40) // 5]
    assert x["wyc_s"] == 12 and x["wyciek95"] == 6  # 12 s, nie 5 min; odcinek nie jest biały
    # podpis sumuje tylko odcinki białe (03:00–03:30, 6 × 300 s); 12 s z 05:40 nie wchodzi (1812 s dałoby też 30)
    assert "Wyciek przez 30 min" in rysunek.podpisy(d)


def _rozproszony(n, sek, od=660):
    """Noc 23:00–07:00 z wyciekiem rozproszonym: `n` odcinków po `sek` s próbek ≥ 15 L/min, P95 odcinka 20 L/min."""
    odc = [_o(m) for m in range(660, 1140, 5)]
    for i in range(n):
        odc[i * 2]["wyc_s"] = sek
        odc[i * 2]["wyciek95"] = 20
    return _dane(_pr([(660, 1140)], odc))


@pytest.mark.parametrize("n,sek", [(30, 12), (12, 16)])
def test_wyciek_rozproszony_bez_bialego(n, sek):
    """30 × 12 s (6 min) albo 12 × 16 s (3,2 min) w odcinkach z P95 20: bez białego, bez podpisu, bez legendy."""
    d = _rozproszony(n, sek)
    assert not any(rysunek.bialy_odcinek(x) for x in d["przebieg"]["odcinki"].values())
    assert not any(t.startswith("Wyciek przez") for t in rysunek.podpisy(d))
    assert "wyciek" not in rysunek.legenda(d)
    o = rysunek.rysuj(d)
    for i in range(n):  # pierścień czarny w środku odcinków z wyciekiem
        assert o.czarny(*_punkt((660 + i * 10) * 60 + 150, 126))


def test_wyciek_przyklad_r_por_08():
    """R-POR-08: 04:00–04:30 z 300, 120, 60, 59, 0, 300 s → białe 04:00–04:15 i 04:25–04:30, 13 min (780 s)."""
    odc = [_o(m) for m in range(660, 1140, 5)]
    for i, sek in enumerate((300, 120, 60, 59, 0, 300)):
        odc[(_u(4, 0) - 660) // 5 + i]["wyc_s"] = sek
    d = _dane(_pr([(660, 1140)], odc))
    assert [rysunek.bialy_odcinek(odc[(_u(4, 0) - 660) // 5 + i]) for i in range(6)] == [True] * 3 + [False] * 2 + [True]
    assert "Wyciek przez 13 min" in rysunek.podpisy(d)


def test_wyciek_jeden_odcinek_60_s():
    """odcinek 01:00 z 60 s wycieku: biały na rysunku, podpis `Wyciek przez 1 min`, legenda `wyciek`."""
    odc = [_o(m) for m in range(660, 1140, 5)]
    odc[(780 - 660) // 5].update(wyc_s=60, wyciek95=16)
    d = _dane(_pr([(660, 1140)], odc))
    assert "Wyciek przez 1 min" in rysunek.podpisy(d)
    assert "wyciek" in rysunek.legenda(d)
    o = rysunek.rysuj(d)
    assert not o.czarny(*_punkt(782 * 60, 126)) and o.czarny(*_punkt(782 * 60, 119))
    odc[(780 - 660) // 5]["wyc_s"] = 59  # 59 s: czarny, bez podpisu (1 min z zaokrąglenia nie wchodzi)
    assert not any(t.startswith("Wyciek przez") for t in rysunek.podpisy(d)) and "wyciek" not in rysunek.legenda(d)


def test_cisnienie_na_maksimum_i_centralne(karta):
    d = _noc(karta, "2026-11-02")
    p = rysunek.podpisy(d)
    # noc niepewna (P95 wycieku nocy 18 L/min): pierwszy podpis bez zdania o tygodniu (R-POR-25)
    assert d["niepewna"]
    assert p == ["Tej nocy nie oceniam: duży wyciek.", "Wyciek przez 30 min", "Centralne: 2",
                 "Ciśnienie na maksimum 20 min"]


def test_pusta_noc_bez_portretu(karta):
    d = _noc(karta, "2026-11-07")
    assert rysunek.minuty_terapii(d["przebieg"]) == 10
    assert rysunek.rysuj(d) is None


def test_asv_piki_wsparcia_i_kropki(karta):
    d = _noc(karta, "2026-11-04", norma_ps=5.0)
    odc = d["przebieg"]["odcinki"]
    assert odc[_u(2, 0) // 5]["ps"] == 70 and odc[_u(3, 0) // 5]["ps"] == 110 and odc[_u(4, 0) // 5]["ps"] == 69
    assert rysunek.wysokosc_zewn(d, odc[_u(2, 0) // 5]) == pytest.approx(16.2)
    assert rysunek.wysokosc_zewn(d, odc[_u(3, 0) // 5]) == pytest.approx(36.0)
    assert rysunek.wysokosc_zewn(d, odc[_u(4, 0) // 5]) == 0
    assert rysunek.wysokosc_wewn(d, odc[_u(2, 0) // 5]) == 0  # ASV bez pików do środka (ADR-0037)
    o = rysunek.rysuj(d)
    assert o.czarny(*_punkt(_u(3, 1) * 60, 110))   # kropka przy piku w tym samym odcinku
    assert rysunek.legenda(d) == ["maska", "wsparcie", "kropka"]
    assert "EPAP na maksimum 10 min" in rysunek.podpisy(d)


def test_asv_bez_normy_uczy_sie(karta):
    d = _noc(karta, "2026-11-04")  # 1 noc historii: brak normy M-28
    assert d["norma_ps10"] is None
    assert rysunek.podpisy(d)[:2] == ["Tydzień jak zwykle.", "Wsparcie oddechu: uczę się."]
    assert "wsparcie" not in rysunek.legenda(d)
    assert all(rysunek.wysokosc_zewn(d, x) == 0 for x in d["przebieg"]["odcinki"].values())


def test_braki_przed_pozostalymi_podpisami(karta):
    d = _noc(karta, "2026-11-05")
    assert d["przebieg"]["braki"] == "LE"
    assert rysunek.podpisy(d)[:3] == ["Tydzień jak zwykle.", "Wyciek: brak odczytu.", "Zdarzenia: brak odczytu."]
    assert "wyciek" not in rysunek.legenda(d)
    d = _noc(karta, "2026-11-06", norma_ps=5.0)
    assert "P" in d["przebieg"]["braki"]
    assert rysunek.podpisy(d)[1] == "Wsparcie oddechu: brak odczytu."
    d = _noc(karta, "2026-11-03")
    assert d["przebieg"]["braki"] == "R"
    p = rysunek.podpisy(d)
    assert p[1] == "Rodzaj bezdechów: brak zapisu." and "Przerwa 01:00–01:20" in p


def test_drzemka_poza_tarcza(karta):
    d = _noc(karta, "2026-11-03", zwykla=_u(23, 0))
    p = rysunek.podpisy(d)
    assert "Przerwa 01:00–01:20" in p and p[-1] == "Drzemka poza tarczą"


def test_odcinek_z_20_probkami_pusty(karta):
    d = _noc(karta, "2026-11-08")
    x = d["przebieg"]["odcinki"][_u(2, 0) // 5]
    assert x["wyciek95"] is None and x["zew"] is None and x["ps"] is None
    o = rysunek.rysuj(d)
    assert o.czarny(*_punkt(_u(2, 2) * 60, 126))  # bez białego odcinka


def test_linia_uwagi_dwa_podpisy_i_skala(karta):
    d = _noc(karta, "2026-11-02", linia="Co było inaczej? ↻ 2 s")
    assert len(rysunek.podpisy(d)) == 2
    o = rysunek.rysuj(d)
    cy = (rysunek.GORA_TRESCI + rysunek.WYS_LINII + rysunek.DOL_TRESCI) // 2
    assert cy == 268
    for y in range(rysunek.DOL_TRESCI, 480):
        for x in range(30, 371):
            if y == rysunek.DOL_TRESCI or y == rysunek.DOL_TRESCI + 1:
                continue
            assert not o.czarny(x, y) or y >= 440  # stopka tylko tekstem od 440


def test_plik_przebiegu_tam_i_z_powrotem(karta):
    d = _noc(karta, "2026-11-02")
    t = przebieg.tekst_pliku(d["przebieg"])
    assert t.startswith("#cpap-eink portret 1\n" + przebieg.POLA + "\n")
    pr = przebieg.czytaj_plik(t)
    assert pr["sesje"] == d["przebieg"]["sesje"] and pr["odcinki"] == d["przebieg"]["odcinki"]
    assert pr["zdarzenia"] == d["przebieg"]["zdarzenia"] and pr["braki"] == d["przebieg"]["braki"]
    with pytest.raises(ValueError):
        przebieg.czytaj_plik(t.replace("#koniec", "#koniex"))


def test_teksty_w_limitach():
    k28, k24, k22 = kroje.kroj("P28B"), kroje.kroj("P24B"), kroje.kroj("P22B")
    W = rysunek.KOL_W
    assert kroje.szer(k28, "12 h 59 z maską · AHI 99,9") <= W
    l = kroje.zawin(k28, "Noc w normie. Tydzień: 4 wskaźniki inaczej niż zwykle.", W)
    assert len(l) == 2
    for t in ("Wsparcie oddechu: uczę się.", "Wsparcie oddechu: brak odczytu.", "Wyciek: brak odczytu.",
              "Zdarzenia: brak odczytu.", "Rodzaj bezdechów: brak zapisu.", "Najgęściej 23:55–00:55",
              "Wyciek przez 720 min", "Przerwy: 99, najdłuższa 720 min", "Centralne: 999",
              "Ciśnienie na maksimum 720 min", "EPAP na maksimum 720 min", "Część nocy poza tarczą",
              "Drzemka poza tarczą"):
        assert kroje.szer(k24, t) <= W, t
    for t in rysunek.LEGENDA.values():
        assert 42 + kroje.szer(k22, t) <= W
    # bilans pionowy najgorszego przypadku (od 12.11.2026 jeden wiersz liczb także po nocy niepewnej i podpis zbiorczy
    # braków w 2 liniach): 8 + 34 + 10 + 68 + 128 + 17 + 112 = 377 ≤ 380; z linią uwagi 8 + 34 + 10 + 68 + 64 + 17 + 112
    # = 313 ≤ 332
    assert 8 + 34 + 10 + 2 * 34 + 4 * 32 + 17 + 4 * 28 <= rysunek.DOL_TRESCI - rysunek.GORA_TRESCI
    assert 8 + 34 + 10 + 2 * 34 + 2 * 32 + 17 + 4 * 28 <= rysunek.DOL_TRESCI - rysunek.GORA_TRESCI - rysunek.WYS_LINII


def _pr(sesje, odcinki=(), zdarzenia=(), braki=""):
    return {"data": "2026-11-04", "sesje": list(sesje), "zdarzenia": list(zdarzenia), "braki": braki,
            "odcinki": {o["od"] // 5: o for o in odcinki}}


def _o(od, **kw):
    o = {"od": od, "do": 5, "wyciek95": 0, "wyc_s": 0, "zew": 0, "centr": 0, "ps": None, "maks": None}
    o.update(kw)
    return o


def _dane(pr, tryb="AutoSet", zwykla=660, **kw):
    d = {"przebieg": pr, "tryb": tryb, "norma_ps10": 50, "zwykla": zwykla, "pierwszy": "Tydzień jak zwykle.",
         "czas": "8 h 00", "ahi": "1,0", "niepewna": False, "linia": ""}
    d.update(kw)
    return d


def test_okno_na_najdluzszej_sesji():
    """zwykła pora 17:50 (M-18 sprzed 09.11.2026), noc 01:07–09:25: okno 23:05–11:05, cała noc na tarczy."""
    d = _dane(_pr([(22, 122), (787, 1285)]), "ASVAuto", 350)
    w = rysunek.okno(d)
    assert rysunek.hhmm(w) == "23:05" and w <= 787 and w + 720 >= 1285
    p = rysunek.podpisy(d)
    assert "Drzemka poza tarczą" in p and "Część nocy poza tarczą" not in p
    d = _dane(_pr([(10, 60), (600, 1380)]), zwykla=600)
    assert rysunek.hhmm(rysunek.okno(d)) == "22:00"
    p = rysunek.podpisy(d)
    assert p.index("Część nocy poza tarczą") < p.index("Drzemka poza tarczą")


def test_centralne_z_odcinkow_ponad_500_zdarzen():
    """600 spłyceń i 10 centralnych; wiersze `Z` mają 500 pierwszych, podpis liczy wszystkie z odcinków."""
    odc = {k: _o(k * 5) for k in range(132, 228)}
    zd = []
    for i in range(600):
        m = 660 + i * 400 // 600
        zd.append((m, 1, 0))
        odc[m // 5]["zew"] += 1
    for i in range(10):
        odc[(1100 + i) // 5]["centr"] += 1
        zd.append((1100 + i, 0, 1))
    d = _dane(_pr([(660, 1140)], odc.values(), zd[:500]))
    assert "Centralne: 10" in rysunek.podpisy(d)
    assert "centralne" in rysunek.legenda(d)


def test_legenda_wycieku_jak_rysunek():
    """odcinek 03:00 z P95 20 i sesją do 03:02: biały łuk jest, legenda ma wyciek."""
    d = _dane(_pr([(660, 902)], [_o(900, wyciek95=20, wyc_s=120)]))
    assert "wyciek" in rysunek.legenda(d)
    assert "Wyciek przez 2 min" in rysunek.podpisy(d)  # podpis z tego samego odcinka
    d = _dane(_pr([(660, 840)], [_o(900, wyciek95=20, wyc_s=120)]))
    assert "wyciek" not in rysunek.legenda(d)
    assert not any(t.startswith("Wyciek przez") for t in rysunek.podpisy(d))  # odcinek bez sesji: bez podpisu


def test_noc_niepewna_pierwszy_podpis():
    """R-POR-25."""
    d = _dane(_pr([(660, 1140)]), niepewna=True)
    assert rysunek.podpisy(d)[0] == "Tej nocy nie oceniam: duży wyciek."
    assert len(kroje.zawin(kroje.kroj("P28B"), rysunek.podpisy(d)[0], rysunek.KOL_W)) == 2


def test_noc_niepewna_jeden_komunikat():
    """(R-POR-15): liczby `8 h 00 z maską` bez AHI i bez drugiego wiersza; pierwszy podpis od y = 106."""
    d = _dane(_pr([(660, 1140)]), niepewna=True, ahi="4,2")
    o = rysunek.Obraz()
    rysunek.rysuj_kolumne(o, d, rysunek.GORA_TRESCI)
    k28 = kroje.kroj("P28B")
    wz = rysunek.Obraz()
    kroje.tekst(wz, k28, rysunek.KOL_X, 62 + 26, "8 h 00 z maską")
    for i, l in enumerate(kroje.zawin(k28, "Tej nocy nie oceniam: duży wyciek.", rysunek.KOL_W)):
        kroje.tekst(wz, k28, rysunek.KOL_X, 106 + 26 + 34 * i, l)
    for y in range(54, 106 + 68):
        for x in range(rysunek.KOL_X, 772):
            assert o.czarny(x, y) == wz.czarny(x, y), (x, y)


def test_maksimum_nieznane_i_odcinek_dwoch_plikow(karta):
    """(R-POR-26) i odcinek dzielony przez dwa pliki (P95 z obu części)."""
    d = _noc(karta, "2026-11-09")
    pr = d["przebieg"]
    assert "M" in pr["braki"] and all(x["maks"] is None for x in pr["odcinki"].values())
    assert "Maksimum ciśnienia: nieznane." in rysunek.podpisy(d)
    x = pr["odcinki"][_u(2, 10) // 5]
    assert x["wyciek95"] == 30 and x["do"] == 4


def test_zakresy_pliku():
    """sesja od minuty 1440 i `wyc_s` 300 czytelne; zdarzenia poza dobą bez wiersza `Z`."""
    pr = _pr([(1440, 1470)], [_o(1435, wyc_s=300)])
    pr["data"] = "2026-11-04"
    q = przebieg.czytaj_plik(przebieg.tekst_pliku(pr))
    assert q["sesje"] == [(1440, 1470)] and q["odcinki"][287]["wyc_s"] == 300


_NAZWY = {"Wsparcie oddechu: brak odczytu.": "wsparcie", "Wsparcie oddechu: uczę się.": "norma wsparcia",
          "Wyciek: brak odczytu.": "wyciek", "Zdarzenia: brak odczytu.": "zdarzenia",
          "Rodzaj bezdechów: brak zapisu.": "rodzaj bezdechów", "Maksimum ciśnienia: nieznane.": "maksimum",
          "Maksimum EPAP: nieznane.": "maksimum"}


def _noc_pelna(asv):
    """Noc z pikami (Najgęściej), przerwą, białym wyciekiem, centralnymi albo wsparciem i kropkami: legenda 4 pozycji."""
    odc = []
    for m in range(660, 1140, 5):
        if 905 <= m < 920:
            continue
        pik = m in (840, 845, 850)
        odc.append(_o(m, wyciek95=22 if m == 960 else 5, wyc_s=300 if m == 960 else 0, zew=3 if pik else 0,
                      centr=2 if m == 870 else 0, ps=110 if pik else 50))
    zd = [(700, 1, 0), (842, 1, 0), (1000, 1, 0)] if asv else []
    return _pr([(660, 905), (920, 1140)], odc, zd)


def test_braki_nigdy_nie_wypadaja_wszystkie_kombinacje():
    """(R-POR-27): 32 kombinacje liter L, P, E, R, M × tryb × norma PS × linia uwagi × noc niepewna ×
    pierwszy podpis w 1 albo 2 liniach, noc z 4 pozycjami legendy: każdy brak pełnym podpisem albo nazwą w podpisie
    zbiorczym, podpis zbiorczy w najwyżej 2 liniach po 382 px, pozostałe w 1 linii, kolumna nie schodzi poniżej y 434."""
    import itertools
    k24 = kroje.kroj("P24B")
    sprawdzonych = dwie = 0
    for n in range(6):
        for litery in itertools.combinations("LPERM", n):
            for tryb in ("CPAP", "AutoSet", "ASV", "ASVAuto"):
                for norma in (50, None):
                    for linia in ("", "Co było inaczej? ↻ 2 s"):
                        for niepewna in (False, True):
                            for pierwszy in ("Tydzień jak zwykle.",
                                             "Noc w normie. Tydzień: 4 wskaźniki inaczej niż zwykle."):
                                pr = _noc_pelna(tryb in rysunek.ASV)
                                pr["braki"] = "".join(litery)
                                d = _dane(pr, tryb, norma_ps10=norma, linia=linia, niepewna=niepewna,
                                          pierwszy=pierwszy, czas="12 h 59", ahi="99,9")
                                oczek = [x[0] for x in rysunek.braki(d)]
                                p = rysunek.podpisy(d)
                                assert len(p) <= (2 if linia else 4)
                                pelne = [t for t in p[1:] if t in _NAZWY]
                                zb = [t for t in p[1:] if t.startswith("Nieznane: ")]
                                assert len(zb) <= 1
                                reszta = [t for t in oczek if t not in pelne]
                                assert pelne == oczek[:len(pelne)]
                                if reszta:
                                    assert len(reszta) >= 2 and zb, (litery, tryb, norma, linia, p)
                                    assert zb[0] == "Nieznane: %s." % ", ".join(_NAZWY[t] for t in reszta)
                                    assert p.index(zb[0]) == len(pelne) + 1
                                else:
                                    assert not zb
                                for t in p[1:]:
                                    l = kroje.zawin(k24, t, rysunek.KOL_W)
                                    assert len(l) <= (2 if t in zb else 1), t
                                    assert all(kroje.szer(k24, x) <= rysunek.KOL_W for x in l)
                                    dwie += t in zb and len(l) == 2
                                o = rysunek.Obraz()
                                gora = rysunek.GORA_TRESCI + (rysunek.WYS_LINII if linia else 0)
                                y = rysunek.rysuj_kolumne(o, d, gora)
                                assert y <= rysunek.DOL_TRESCI + 4, (y, litery, tryb, linia)
                                assert not any(o.czarny(x, yy) for yy in range(rysunek.DOL_TRESCI, 480)
                                               for x in range(380, 800))
                                sprawdzonych += 1
    assert sprawdzonych == 32 * 4 * 2 * 2 * 2 * 2 and dwie > 0


def test_braki_zbiorczy_znane_odpowiedzi():
    """linia uwagi + braki L i E → `Nieznane: wyciek, zdarzenia.`; ASVAuto P, L, E, M bez linii → 2 pełne
    i `Nieznane: zdarzenia, maksimum.`; z linią → wszystkie 4 nazwy (2 linie); ASV bez normy z linią i L → `norma
    wsparcia`; AutoSet L, R, M z linią → `rodzaj bezdechów`."""
    linia = "Co było inaczej? ↻ 2 s"
    d = _dane(_pr([(660, 1140)], braki="LE"), linia=linia)
    assert rysunek.podpisy(d) == ["Tydzień jak zwykle.", "Nieznane: wyciek, zdarzenia."]
    d = _dane(_pr([(660, 1140)], braki="LPEM"), "ASVAuto")
    assert rysunek.podpisy(d) == ["Tydzień jak zwykle.", "Wsparcie oddechu: brak odczytu.", "Wyciek: brak odczytu.",
                                  "Nieznane: zdarzenia, maksimum."]
    d = _dane(_pr([(660, 1140)], braki="LPEM"), "ASVAuto", linia=linia)
    assert rysunek.podpisy(d) == ["Tydzień jak zwykle.", "Nieznane: wsparcie, wyciek, zdarzenia, maksimum."]
    assert kroje.zawin(kroje.kroj("P24B"), rysunek.podpisy(d)[1], rysunek.KOL_W) == [
        "Nieznane: wsparcie, wyciek,", "zdarzenia, maksimum."]
    d = _dane(_pr([(660, 1140)], braki="LRM"), linia=linia)
    assert rysunek.podpisy(d) == ["Tydzień jak zwykle.", "Nieznane: wyciek, rodzaj bezdechów, maksimum."]
    d = _dane(_pr([(660, 1140)], braki="L"), "ASV", norma_ps10=None, linia=linia)
    assert rysunek.podpisy(d) == ["Tydzień jak zwykle.", "Nieznane: norma wsparcia, wyciek."]
    d = _dane(_pr([(660, 1140)], braki="LE"))  # bez linii: 2 braki na 3 miejscach, pełne podpisy jak dotąd
    assert rysunek.podpisy(d)[:3] == ["Tydzień jak zwykle.", "Wyciek: brak odczytu.", "Zdarzenia: brak odczytu."]


# ---------- znacznik startu okna zamiast napisu okna (R-POR-06, zmiana 2026-11-12 (2), decyzja właściciela projektu 12.11.2026) ----------
def _noc_znacznika(sesje, zwykla):
    odc = {}
    for a, b in sesje:
        for m in range(a // 5 * 5, b, 5):
            odc[m // 5] = {"od": m, "wyciek95": 5, "wyc_s": 0, "zew": 0, "centr": 0, "ps": None, "maks": None}
    pr = {"sesje": list(sesje), "odcinki": odc, "zdarzenia": [], "braki": ""}
    return {"przebieg": pr, "tryb": "AutoSet", "norma_ps10": None, "zwykla": zwykla, "pierwszy": "Tydzień jak zwykle.",
            "czas": "8 h 00", "ahi": "1,0", "niepewna": False, "linia": ""}


def _zn_punkt(w, a, t, cy):
    """Punkt znacznika: `a` wzdłuż promienia pod kątem startu okna, `t` w poprzek (dodatnie zgodnie ze wskazówkami)."""
    k = math.radians((w % 720) * 0.5)
    x = a * math.sin(k) + t * math.cos(k)
    y = a * math.cos(k) - t * math.sin(k)
    return CX + int(math.floor(x + 0.5)), cy - int(math.floor(y + 0.5))


def _sk(r, skala):
    return (r * skala + 50) // 100


def _znacznik_zawsze(o, w, cy, skala):
    """Niezmienniki znacznika: kreska przez pierścień, biała przerwa po obu stronach, grot zgodnie ze wskazówkami."""
    r_in, r_out, r2 = _sk(118, skala), _sk(134, skala), _sk(150, skala)
    A = r2 - 7.5
    bledy = []
    for a in (r_in + 1, (r_in + r_out) / 2, r_out - 1, r2 - 4):
        if not o.czarny(*_zn_punkt(w, a, 0, cy)):
            bledy.append(("kreska", a))
    for a in (r_in + 2, (r_in + r_out) / 2, r_out - 2):
        for t in (-3.2, 3.2):
            if o.czarny(*_zn_punkt(w, a, t, cy)):
                bledy.append(("przerwa", a, t))
    if not o.czarny(*_zn_punkt(w, A, 5.5, cy)):
        bledy.append(("grot",))
    x0, y0 = _zn_punkt(w, A, 13.5, cy)
    if not any(o.czarny(x, y) for x in range(x0 - 2, x0 + 3) for y in range(y0 - 2, y0 + 3)
               if (x - x0) ** 2 + (y - y0) ** 2 <= 4):
        bledy.append(("czubek",))
    return bledy


@pytest.mark.parametrize("linia", ["", "Co było inaczej? ↻ 2 s"])
def test_srodek_tarczy_bez_napisu_okna(linia):
    """R-POR-06: w kole o promieniu 96 (z linią uwagi 82) tylko cyfry i kreski godzin: ten sam obraz przy każdym oknie,
    nic bliżej środka niż 60 px (z linią 51)."""
    skala = 85 if linia else 100
    cy = (rysunek.GORA_TRESCI + (rysunek.WYS_LINII if linia else 0) + rysunek.DOL_TRESCI) // 2
    r_srodka, r_pusty = _sk(96, skala), _sk(60, skala)
    pierwszy = None
    for w in (540, 570, 680, 720, 600, 455):
        d = _noc_znacznika([(w + 150, w + 600)], w + 120)
        d["linia"] = linia
        assert rysunek.okno(d) == w
        o = rysunek.rysuj(d)
        srodek = []
        for y in range(cy - r_srodka, cy + r_srodka + 1):
            for x in range(CX - r_srodka, CX + r_srodka + 1):
                r2 = (x - CX) ** 2 + (y - cy) ** 2
                if r2 >= r_srodka * r_srodka:
                    continue
                srodek.append(o.czarny(x, y))
                if r2 < r_pusty * r_pusty:
                    assert not o.czarny(x, y), (w, x, y)
        assert any(srodek)
        if pierwszy is None:
            pierwszy = srodek
        assert srodek == pierwszy, w


@pytest.mark.parametrize("w", [540, 570, 680, 720])
@pytest.mark.parametrize("linia", ["", "Co było inaczej? ↻ 2 s"])
def test_znacznik_startu_okna(w, linia):
    """R-POR-06: znacznik pod kątem startu okna (21:00, 21:30, 23:20, 00:00), grot zgodnie ze wskazówkami zegara, na
    kropkowanym śladzie i na pierścieniu (sesja przez start okna, noc zaczęta wcześniej niż zwykle)."""
    skala = 85 if linia else 100
    cy = (rysunek.GORA_TRESCI + (rysunek.WYS_LINII if linia else 0) + rysunek.DOL_TRESCI) // 2
    r1, r2, r_out = _sk(110, skala), _sk(150, skala), _sk(134, skala)
    A = r2 - 7.5
    d = _noc_znacznika([(w + 60, w + 600)], w + 120)
    d["linia"] = linia
    assert rysunek.okno(d) == w
    o = rysunek.rysuj(d)
    assert _znacznik_zawsze(o, w, cy, skala) == []
    assert not o.czarny(*_zn_punkt(w, r1 - 4, 0, cy))
    assert not o.czarny(*_zn_punkt(w, r2 + 4, 0, cy))
    assert not o.czarny(*_zn_punkt(w, A, -6, cy)) and not o.czarny(*_zn_punkt(w, A, -11, cy))
    assert not o.czarny(*_zn_punkt(w, A, 19, cy))
    assert not o.czarny(*_zn_punkt(w, r_out + 2, 10, cy))
    assert not o.czarny(*_zn_punkt(w + 60, A, 5.5, cy)) and not o.czarny(*_zn_punkt(w - 60, A, 5.5, cy))
    e = _noc_znacznika([(w - 20, w + 40), (w + 100, w + 600)], w + 120)
    e["linia"] = linia
    assert rysunek.okno(e) == w
    o = rysunek.rysuj(e)
    assert _znacznik_zawsze(o, w, cy, skala) == []
    assert o.czarny(*_zn_punkt(w, _sk(120, skala), 8, cy))
    assert not o.czarny(*_zn_punkt(w, _sk(120, skala), -8, cy))


def test_znacznik_w_strefie_tarczy_kazde_okno():
    """Znacznik przy każdym starcie okna (co 5 min tarczy), z linią uwagi i bez: w strefie tarczy (x < 380, y od góry
    treści do 434), w promieniu najwyżej 152 px (piki sięgają 170), niezmienniki spełnione."""
    for skala, gora in ((100, rysunek.GORA_TRESCI), (85, rysunek.GORA_TRESCI + rysunek.WYS_LINII)):
        cy = (gora + rysunek.DOL_TRESCI) // 2
        for w in range(0, 720, 5):
            o = rysunek.Obraz()
            rysunek.rysuj_znacznik(o, w, cy, skala)
            assert _znacznik_zawsze(o, w, cy, skala) == [], (skala, w)
            for i, v in enumerate(o.px):
                if v:
                    x, y = i % rysunek.W, i // rysunek.W
                    assert x < 380 and gora <= y < rysunek.DOL_TRESCI, (skala, w, x, y)
                    assert (x - CX) ** 2 + (y - cy) ** 2 <= _sk(152, skala) ** 2, (skala, w, x, y)


# ---------- (12.11.2026) ----------
WYCIEK_MIN_PX = 20  # jak test/testy_portret.cpp: pole łuku 5 min w paśmie bieli ok. 55 px (z linią uwagi 37 px)


def _widoczne_wycieku(o, w, cy, skala, od_s=None, do_s=None, scisle=False):
    """Białe piksele w paśmie bieli z czarnym obrysem na tym samym kierunku (promień 119,5 i 132,5 w skali); `scisle`:
    obrys czarny także na kierunkach obróconych o ±1,5 px. `od_s`, `do_s`: kąty w sekundach tarczy od startu okna."""
    b_in, b_out = _sk(121, skala), _sk(131, skala)
    rw, rz = _sk(119, skala) + 0.5, _sk(132, skala) + 0.5
    n = 0
    for y in range(cy - b_out, cy + b_out + 1):
        for x in range(CX - b_out, CX + b_out + 1):
            dx, dy = x - CX, y - cy
            r2 = dx * dx + dy * dy
            if r2 < b_in * b_in or r2 >= b_out * b_out or o.czarny(x, y):
                continue
            if od_s is not None:
                q = int((math.atan2(dx, -dy) % (2 * math.pi)) / (2 * math.pi) * 43200.0)
                u = (q - w * 60) % 43200
                if not (od_s <= u < do_s):
                    continue
            r = math.sqrt(r2)
            ok = True
            for dt in ((-1.5, 0.0, 1.5) if scisle else (0.0,)):
                c, s = math.cos(dt / r), math.sin(dt / r)
                ex, ey = (dx * c - dy * s) / r, (dx * s + dy * c) / r
                ok = ok and o.czarny(CX + int(math.floor(ex * rw + 0.5)), cy + int(math.floor(ey * rw + 0.5))) \
                    and o.czarny(CX + int(math.floor(ex * rz + 0.5)), cy + int(math.floor(ey * rz + 0.5)))
            n += ok
    return n


def _bialy_obrys(o, w, cy, skala, od_s, do_s):
    r_in, r_out, b_in, b_out = _sk(118, skala), _sk(134, skala), _sk(121, skala), _sk(131, skala)
    n = 0
    for y in range(cy - r_out, cy + r_out + 1):
        for x in range(CX - r_out, CX + r_out + 1):
            dx, dy = x - CX, y - cy
            r2 = dx * dx + dy * dy
            if r2 < r_in * r_in or r2 >= r_out * r_out or b_in * b_in <= r2 < b_out * b_out:
                continue
            u = (int((math.atan2(dx, -dy) % (2 * math.pi)) / (2 * math.pi) * 43200.0) - w * 60) % 43200
            if od_s <= u < do_s and not o.czarny(x, y):
                n += 1
    return n


@pytest.mark.parametrize("linia", ["", "Co było inaczej? ↻ 2 s"])
def test_wyciek_na_starcie_okna_widoczny(linia):
    """(R-POR-06, R-POR-08, R-POR-13): 288 startów okna, sesje {w − 20, w + 40}, {w + 100, w + 600}, pierwszy odcinek
    okna z 300 s wycieku: legenda `wyciek`, w pierwszym odcinku co najmniej WYCIEK_MIN_PX widocznych pikseli wycieku,
    obrys bez białego piksela, znacznik pod i nad pierścieniem."""
    skala = 85 if linia else 100
    gora = rysunek.GORA_TRESCI + (rysunek.WYS_LINII if linia else 0)
    cy = (gora + rysunek.DOL_TRESCI) // 2
    katy, n_okien, najmniej = set(), 0, 10 ** 9
    for w in range(0, 1440, 5):
        ses = []
        if w + 40 <= 1439:
            ses.append((max(0, w - 20), w + 40))
        if w + 100 < 1439:
            ses.append((w + 100, min(1439, w + 600)))
        if not ses:
            continue
        d = _noc_znacznika(ses, w + 120)
        d["linia"] = linia
        if w // 5 in d["przebieg"]["odcinki"]:
            d["przebieg"]["odcinki"][w // 5].update(wyc_s=300, wyciek95=22)
        if rysunek.okno(d) != w or "wyciek" not in rysunek.legenda(d):
            continue
        o = rysunek.Obraz()
        rysunek.rysuj_tarcze(o, d, cy, skala)
        n = _widoczne_wycieku(o, w, cy, skala, 10, 290)
        najmniej = min(najmniej, n)
        assert n >= WYCIEK_MIN_PX, (w, n)
        assert _bialy_obrys(o, w, cy, skala, 10, 290) == 0, w
        assert o.czarny(*_zn_punkt(w, _sk(114, skala), 0, cy)) and o.czarny(*_zn_punkt(w, _sk(150, skala) - 7.5, 5.5, cy))
        katy.add((w % 720) // 5)
        n_okien += 1
    assert n_okien >= 250 and len(katy) == 144, (n_okien, len(katy))


def test_legenda_wycieku_zgodna_z_rysunkiem_losowo():
    """300 nocy losowych (wyciek także w pierwszym i ostatnim odcinku okna): legenda `wyciek` ⇒ widoczny biały
    odcinek z obrysem; bez legendy ⇒ żadnego (sprawdzenie ścisłe)."""
    import random
    rnd = random.Random(20261112)
    for _ in range(300):
        w = rnd.randrange(60, 840, 5)
        ses = [(w + rnd.randrange(-30, 30), w + rnd.randrange(40, 120))]
        ses.append((ses[0][1] + rnd.randrange(5, 60), w + rnd.randrange(300, 720)))
        ses = [(a, b) for a, b in ses if b > a]
        d = _noc_znacznika(ses, w + 120)
        for k, x in d["przebieg"]["odcinki"].items():
            if rnd.random() < 0.15 or k * 5 in (w, w + 715):
                x["wyc_s"] = rnd.choice([0, 30, 59, 60, 120, 300])
                x["wyciek95"] = 20
        d["linia"] = rnd.choice(["", "Co było inaczej? ↻ 2 s"])
        skala = 85 if d["linia"] else 100
        cy = (rysunek.GORA_TRESCI + (rysunek.WYS_LINII if d["linia"] else 0) + rysunek.DOL_TRESCI) // 2
        o = rysunek.Obraz()
        rysunek.rysuj_tarcze(o, d, cy, skala)
        wo = rysunek.okno(d)
        if "wyciek" in rysunek.legenda(d):
            assert _widoczne_wycieku(o, wo, cy, skala) > 0, (w, ses)
        else:
            assert _widoczne_wycieku(o, wo, cy, skala, scisle=True) == 0, (w, ses)


@pytest.mark.parametrize("skala", [100, 85])
def test_szerokosc_kreski_znacznika(skala):
    """kreska 4–5 px: przy oknie od 00:00 i 03:00 dokładnie 5 pikseli w poprzek części pod pierścieniem; przy
    każdym z 144 kątów pole części pod pierścieniem / jej długość w 3,5–5,6 px."""
    cy = 244
    r1, r_in = _sk(110, skala), _sk(118, skala)
    rs = (r1 + r_in) // 2
    for w in range(0, 720, 5):
        o = rysunek.Obraz()
        rysunek.rysuj_znacznik(o, w, cy, skala)
        if w == 0:
            assert sum(o.czarny(CX + i, cy - rs) for i in range(-8, 9)) == 5
        if w == 180:
            assert sum(o.czarny(CX + rs, cy + i) for i in range(-8, 9)) == 5
        k = math.radians(w * 0.5)
        pole = 0
        for y in range(cy - r_in, cy + r_in + 1):
            for x in range(CX - r_in, CX + r_in + 1):
                X, Y = x - CX, cy - y
                r = math.hypot(X, Y)
                a = X * math.sin(k) + Y * math.cos(k)
                t = X * math.cos(k) - Y * math.sin(k)
                if r1 + 1 <= r < r_in - 1 and a > 0 and abs(t) < 10 and o.czarny(x, y):
                    pole += 1
        szer = pole / (r_in - 1 - (r1 + 1))
        assert 3.5 <= szer <= 5.6, (w, szer)
