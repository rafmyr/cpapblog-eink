# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy przypadków brzegowych (scenariusze symulatora ekranów, AutoSet, CPAP). Każdy test sprawdza jeden przypadek
i blokuje (bez `xfail`).
Tylko zrzuty syntetyczne (tools/symulator/generuj_zrzut.py), bez prawdziwych danych zdrowotnych.
"""
import datetime as dt
import importlib.util
import os
import re
import zlib

import pytest

TU = os.path.dirname(os.path.abspath(__file__))


def modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa + "_scen", os.path.join(TU, "symulator", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


sym = modul("symuluj")
gen = modul("generuj_zrzut")
scen = sym.scen


@pytest.fixture(scope="module")
def zrzut(tmp_path_factory):
    k = str(tmp_path_factory.mktemp("zrzut_r7s"))
    gen.generuj(k, wersja=0, nocy=150)
    return k


def przebieg(zrzut, katalog, nazwa):
    return sym.symuluj(zrzut, wyjscie=str(katalog), cicho=True, scenariusze=[nazwa], bez_prognozy=True)


def tekst_pdf(plik):
    """Teksty operatorów `(…) Tj` ze wszystkich strumieni PDF (rozpakowanych, gdy skompresowane)."""
    with open(plik, "rb") as f:
        d = f.read()
    t = b""
    for m in re.finditer(rb"stream\r?\n(.*?)endstream", d, re.S):
        s = m.group(1)
        try:
            s = zlib.decompress(s)
        except zlib.error:
            pass
        t += s
    return b" ".join(re.findall(rb"\((.*?)\)\s*Tj", t)).decode("latin-1")


# raport A4 z nocami wymyślonymi (rozbieg + scenariusz, na prawdziwym zrzucie zmieszane z prawdziwymi
# nocami) nie ma w treści żadnego oznaczenia „DANE SYNTETYCZNE”; oznaczona jest tylko nazwa pliku. Wydrukowany
# (ściąga panelu każe „wydrukuj lub wyślij”, README „otwórz go podglądem jak każdy PDF”) wygląda jak prawdziwy raport
# dla lekarza. README obiecuje „Noce wymyślone są oznaczone wszędzie”.
def test_r7s_01_pdf_z_nocami_syntetycznymi_oznaczony_w_tresci(zrzut, tmp_path):
    k = przebieg(zrzut, tmp_path / "cpap", "cpap")
    pdf = [x for x in os.listdir(k) if x.endswith(".pdf")]
    assert pdf, "brak PDF raportu"
    t = tekst_pdf(os.path.join(k, pdf[0]))
    assert "SYNTETYCZNE" in t.upper(), "PDF raportu z nocami syntetycznymi bez oznaczenia w treści"


# spis.txt zwija kolejne obrazki o tej samej nazwie i linii uwagi także przez granicę dnia i opisuje je
# „ten sam ekran przerysowany”. Na wyjeździe S5 z 27.11 11:01 („Noc 27.11 jeszcze w drodze”) i z 28.11 05:01
# („Brak danych od 2 nocy”) to różne ekrany w jednym wierszu: użytkownik nie zobaczy drugiego.
def test_r7s_02_spis_nie_zwija_przez_granice_dnia():
    spis = [("SYN-020-20261127-1101-dzien-S5.png", "dzien-S5", "", ""),
            ("SYN-021-20261127-1131-dzien-S5.png", "dzien-S5", "", ""),
            ("SYN-022-20261128-0501-dzien-S5.png", "dzien-S5", "", "")]
    t = sym.tekst_spisu(spis)
    assert "-dzien-S5.png … SYN-022-20261128" not in t, \
        "obrazek następnego dnia schowany w zwiniętym wierszu „ten sam ekran przerysowany”"


# opis scenariusza `sprzet` w spis.txt i --lista-scenariuszy mówi „24 noce z wyciekiem P95 rosnącym od
# 13 do 23 L/min”, a plan ma 14 nocy od 24,5 do 26,6 L/min (README: 14 nocy). Wyciek 13–23 L/min nie dałby S-K-31 ani L-09.
def test_r7s_03_opis_sprzet_zgodny_z_planem(zrzut):
    opis = scen.SCENARIUSZE["sprzet"][1]
    p = scen.plan(zrzut, ["sprzet"])
    wp95 = [float(n.split(";")[7]) for n in p.noce[scen.ROZBIEG:scen.ROZBIEG + scen.SPRZET_ROSNIE]]
    assert min(wp95) > 24
    assert "od 13 do 23" not in opis and "24 noce" not in opis, \
        f"opis `sprzet` niezgodny z nocami ({scen.SPRZET_ROSNIE} nocy {min(wp95):.1f}–{max(wp95):.1f} L/min)"


# `sprzet` zapowiada „L-09 wstrzymane do 4. pokazania S-K-31” (R-MAS-03, R-MAS-10), a w przebiegu L-09
# wychodzi pełnym ekranem po 2 dniach z S-K-31 (bramka spadek wycieku kasuje warunek S-K-31, więc blokada
# przestaje działać). Zapowiedź „jest” sprawdza tylko wiersz `L-09 wstrzymane` w logu; R-MAS-10 nie jest pokazane.
def test_r7s_04_sprzet_l09_po_czwartym_sk31(zrzut, tmp_path):
    k = przebieg(zrzut, tmp_path / "sprzet", "sprzet")
    dni_sk31, pierwszy_l09 = set(), None
    with open(os.path.join(k, "log-panelu.txt"), encoding="utf-8") as f:
        for l in f:
            m = re.search(r"EKRAN (\S+) \S*-(\d{8})-\d{4}-", l)
            if not m:
                continue
            if m.group(1) == "dzien-SK31":
                dni_sk31.add(m.group(2))
            if m.group(1).startswith("dzien-wzorzec-L-09") and pierwszy_l09 is None:
                pierwszy_l09 = m.group(2)
    przed = {d for d in dni_sk31 if pierwszy_l09 is None or d < pierwszy_l09}
    opis = scen.SCENARIUSZE["sprzet"][1]
    assert pierwszy_l09 is None or len(przed) >= 4 or "4. pokazania" not in opis, \
        f"L-09 od {pierwszy_l09} po {len(przed)} dniach z S-K-31, a opis zapowiada „do 4. pokazania S-K-31”"


# `infekcja` zapowiada „w okresie zdarzenia wycieku E-20…E-22 „nie oceniam” (nie „w porządku”)”, ale
# żadna noc scenariusza nie ma wycieku: „jest” daje sam wiersz silnika `nie oceniam: E-20 (okres infekcji …)`, bez
# ekranu, który użytkownik mógłby ocenić (weryfikacja R-INF-02: okres infekcji i P95 wycieku 30 L/min → brak S-K-10
# i linii E-20).
def test_r7s_05_infekcja_ma_noc_z_wyciekiem(zrzut):
    p = scen.plan(zrzut, ["infekcja"])
    scen_noce = p.noce[scen.ROZBIEG:]
    assert any(float(n.split(";")[7]) >= 24 for n in scen_noce), \
        "okres infekcji bez nocy z wyciekiem, zapowiedź R-INF-02 spełniona samym logiem"


# `odkrycia` zapowiada S7 „1. dnia miesiąca”, ale okno pokazu to 11 dni od (ostatnia noc zrzutu + 81 dni):
# dla zrzutu kończącego się 14.11.2026 okno 03.02–13.02.2027 nie ma 1. dnia miesiąca i S7 jest BRAK. Działa tylko
# przypadkiem dat (zrzut z 4.11 albo 5.11).
def test_r7s_06_odkrycia_okno_z_pierwszym_dniem_miesiaca(tmp_path):
    k = str(tmp_path / "z")
    gen.generuj(k, wersja=0, nocy=150, ostatni=dt.date(2026, 11, 14))
    p = scen.plan(k, ["odkrycia"])
    dni = [p.od.date() + dt.timedelta(days=i) for i in range((p.do.date() - p.od.date()).days + 1)]
    assert any(d.day == 1 for d in dni), f"okno odkrycia {p.od:%d.%m}–{p.do:%d.%m} bez 1. dnia miesiąca (S7)"


# `regularnosc` na krótkiej historii (zrzut 20 nocy): L-08 wariant szybki „nie oceniam: odniesienie bez
# 30 dni historii”, wszystkie 6 zapowiedzi BRAK. Scenariusz nie dokłada historii ani nie zgłasza tego w „NIE DA SIĘ
# WYWOŁAĆ”, choć wie, ile nocy ma zrzut.
def test_r7s_07_regularnosc_na_krotkiej_historii(tmp_path):
    k = str(tmp_path / "z20")
    gen.generuj(k, wersja=0, nocy=20)
    p = scen.plan(k, ["regularnosc"])
    if any("L-08" in z for z in p.znaleziska):
        return
    wy = sym.symuluj(k, wyjscie=str(tmp_path / "w"), cicho=True, scenariusze=["regularnosc"], bez_prognozy=True)
    wynik = dict(sym.sprawdz_oczekiwane(wy, p))
    assert all(wynik.values()), f"regularnosc na 20 nocach: BRAK {[o for o, j in wynik.items() if not j]}"


# warunek `dzien_z` (K1 „z pełnym tytułem odkrycia”, R-CZY-20) zalicza każdy K1 tego samego dnia po
# ekranie odkrycia, także K1 o 7:30 po slocie dziennym; treści K1 nie sprawdza nic.
def test_r7s_08_dzien_z_tylko_k1_zaraz_po_odkryciu(tmp_path):
    k = str(tmp_path)
    with open(os.path.join(k, "log-panelu.txt"), "w", encoding="utf-8") as f:
        for nazwa, plik in (("dzien-odkrycie-O-12_A", "SYN-001-20270125-0500-dzien-odkrycie-O-12_A.png"),
                            ("dzien-SK37", "SYN-002-20270125-0510-dzien-SK37.png"),
                            ("K1", "SYN-003-20270125-0730-K1.png")):
            f.write(f"2027-01-25 05:00:00  cpap-eink 1.0, wybudzenie zegar, licznik 1\n"
                    f"2027-01-25 05:00:00  EKRAN {nazwa} {plik}\n")
    p = scen.Plan()
    p.oczekiwane = [("ekran", "K1", "K1 z odkryciem", (("dzien_z", "dzien-odkrycie-O-12"),))]
    assert sym.sprawdz_oczekiwane(k, p) == [("K1 z odkryciem", False)], \
        "K1 o 7:30 (po slocie dziennym, bez odkrycia) zaliczony jako K1 z tytułem odkrycia"
