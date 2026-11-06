# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy tools/pomiary/analiza.py na danych o znanej odpowiedzi (rachunki w komentarzach).

Uruchomienie: python3 -m pytest -q tools/pomiary
Dane syntetyczne powstają w katalogu tymczasowym pytest; w repozytorium nie ma surowych plików z panelu (WY12).
"""

import datetime as dt
import os
import subprocess
import sys

import pytest

_TU = os.path.dirname(os.path.abspath(__file__))
if _TU not in sys.path:
    sys.path.insert(0, _TU)

import analiza as A  # noqa: E402

NAGLOWEK = A.NAGLOWEK_DRYFU
# Prawdziwe pierwsze 5 wierszy z panelu (02.11.2026, pomiar 12).
PIEC = """wybudzenie;ntp_ms;od_startu_h;panel_minus_ntp_ms;pcf_minus_ntp_ms;rtt_ms;pcf_vl
1;1793611907027;0.000;-6;906;16;0
2;1793613297333;0.386;13613;917;15;0
3;1793613900292;0.554;13619;926;23;0
4;1793615380552;0.965;27476;934;13;0
5;1793616660857;1.321;36045;945;19;0
"""


def uruchom(tmp_path, *argi, tresc=None, nazwa="plik.csv"):
    sciezka = tmp_path / nazwa
    if tresc is not None:
        sciezka.write_text(tresc, encoding="utf-8")
    p = subprocess.run([sys.executable, os.path.join(_TU, "analiza.py"), argi[0], str(sciezka)] + list(argi[1:]),
                       capture_output=True, text=True)
    return p.returncode, p.stdout, p.stderr


# ---------------------------------------------------------------- liczby

def test_liczba_po_polsku():
    assert A.liczba(13613, 0) == "13 613"
    assert A.liczba(-0.05, 2) == "−0,05"
    assert A.liczba(-0.0001, 2) == "0,00"
    assert A.liczba(None) == "–"


def test_percentyl_najblizsza_ranga():
    # 10 wartości 1…10: P90 = wartość o randze ceil(0,9·10) = 9
    assert A.percentyl(list(range(1, 11)), 90) == 9
    # 5 wartości: ranga ceil(4,5) = 5 → max
    assert A.percentyl([5, 1, 4, 2, 3], 90) == 5


# ---------------------------------------------------------------- dryf

def test_dryf_piec_wierszy_tempo_odcinka_3_4():
    w, k, pom = A.parsuj_dryf(PIEC)
    assert len(w) == 5 and not pom
    r = A.analiza_dryfu(w, k)
    o = {(x["od"], x["do"]): x for x in r["odcinki"]}
    # 3→4: (27 476 − 13 619) ms / (1 793 615 380 552 − 1 793 613 900 292) ms = 13 857 / 1 480 260 = 0,9361 %
    #      → 0,009361 × 86 400 = 808,8 s/dobę
    assert o[(3, 4)]["tempo"] * 100 == pytest.approx(0.9361, abs=1e-4)
    assert o[(3, 4)]["tempo"] * 86400 == pytest.approx(808.8, abs=0.1)
    # 1→2: (13 613 − (−6)) / 1 390 306 = 0,9796 %; 4→5: 8 569 / 1 280 305 = 0,6693 %
    assert o[(1, 2)]["tempo"] * 100 == pytest.approx(0.9796, abs=1e-4)
    assert o[(4, 5)]["tempo"] * 100 == pytest.approx(0.6693, abs=1e-4)
    # 2→3: 6 / 602 959 = 0,001 %: okno serwisowe, panel nie spał. Mediana 4 odcinków = (0,6693 + 0,9361)/2 =
    # 0,8027 %, 25 % z niej = 0,2007 %, więc tylko 2→3 poza statystyką.
    assert o[(2, 3)]["bez_snu"] and not o[(3, 4)]["bez_snu"]
    t = r["tempo_snu"]
    assert t["n"] == 3
    assert t["min"] * 100 == pytest.approx(0.6693, abs=1e-4)
    assert t["mediana"] * 100 == pytest.approx(0.9361, abs=1e-4)
    assert t["max"] * 100 == pytest.approx(0.9796, abs=1e-4)
    assert r["bezpiecznik"] == []
    # całkowity czas: (1 793 616 660 857 − 1 793 611 907 027) / 3 600 000 = 1,3205 h
    assert r["czas_h"] == pytest.approx(1.3205, abs=1e-4)


def test_dryf_piec_wierszy_pcf_regresja():
    w, k, _ = A.parsuj_dryf(PIEC)
    r = A.analiza_dryfu(w, k)["pcf"]
    # x [h] = 0; 0,38620; 0,55368; 0,96487; 1,32051 (z ntp_ms), y [ms] = 906; 917; 926; 934; 945
    # średnie: x̄ = 0,64505, ȳ = 925,6; Σ(x−x̄)(y−ȳ) = 30,623; Σ(x−x̄)² = 1,04997
    # nachylenie b = 30,623 / 1,04997 = 29,166 ms/h → 29,166 / 3,6 = 8,10 ppm (0,700 s/dobę)
    # wyraz wolny a = 925,6 − 29,166 · 0,64505 = 906,79 ms
    assert r["b"] == pytest.approx(29.166, abs=1e-3)
    assert r["b"] / 3.6 == pytest.approx(8.10, abs=0.01)
    assert r["a"] == pytest.approx(906.79, abs=0.01)
    # błąd w 72 h z dryfu: 29,166 × 72 = 2 100 ms
    assert r["b"] * 72 == pytest.approx(2100.0, abs=1)


def test_dryf_raport_i_wniosek(tmp_path):
    kod, out, err = uruchom(tmp_path, "dryf", tresc=PIEC)
    assert kod == 0, err
    assert "| 3 → 4 | 0,411 h | 13 857 ms | 0,936% | 809 |  |" in out
    assert "8,10 ppm" in out
    # zegar panelu 0,9361 % × 72 h × 3600 s = 2 426 s = 40,4 min > 5 min; PCF 2,10 s + 0,907 s = 3,0 s
    assert "ok. 40,4 min w 72 h" in out and "niespełnione" in out
    assert "w 72 h ok. 3,0 s" in out
    assert "brać czas z PCF8563" in out
    assert "krócej niż 72 h" in out


def test_dryf_bezpiecznik_faza_2_i_brak_odczytu_pcf():
    # 0 h: panel 0, PCF 900; 1 h: panel 36 000 (1 %); 2 h: 72 000; 3,5 h: 126 000 ≥ 120 000 → bezpiecznik.
    # Potem komentarze firmware i faza 2: panel − NTP = dryf od synchronizacji w poprzednim wybudzeniu:
    # 4,0 h: 18 000 ms po 0,5 h = 1 %; 5,0 h: brak odczytu PCF (puste pole); 6,0 h: PCF z bitem VL = 1.
    # PCF: 900 + 36 ms/h (10 ppm) w punktach 0; 1; 2; 3,5; 4 h → nachylenie 36 ms/h dokładnie.
    h = 3600000
    t0 = 1793611907027
    tekst = NAGLOWEK + "\n" + "\n".join([
        "1;{};0.000;0;900;16;0".format(t0),
        "2;{};1.000;36000;936;16;0".format(t0 + h),
        "3;{};2.000;72000;972;16;0".format(t0 + 2 * h),
        "4;{};3.500;126000;1026;16;0".format(t0 + int(3.5 * h)),
        "# koniec po 3.50 h: panel 864.00 s/dobę",
        "# zegar panelu: bezpiecznik, dalej tylko PCF8563 do 72 h",
        "# v2: zegar panelu z NTP, dalej tylko PCF8563 do 72 h",
        "5;{};4.000;18000;1044;16;0".format(t0 + 4 * h),
        "6;{};5.000;-3;;16;0".format(t0 + 5 * h),
        "7;{};6.000;5;5000;16;1".format(t0 + 6 * h),
    ]) + "\n"
    w, k, pom = A.parsuj_dryf(tekst)
    assert [x.faza for x in w] == [1, 1, 1, 1, 2, 2, 2]
    r = A.analiza_dryfu(w, k)
    assert r["bezpiecznik"] == [{"wybudzenie": 4, "h": pytest.approx(3.5), "panel_ms": 126000}]
    # 3 odcinki fazy 1, każdy 1 % (36 000 / 3 600 000; 54 000 / 5 400 000)
    assert [round(x["tempo"], 6) for x in r["odcinki"]] == [0.01, 0.01, 0.01]
    # faza 2: 18 000 / 1 800 000 = 1 %; −3 / 3 600 000; 5 / 3 600 000
    assert r["tempo_fazy2"]["n"] == 3
    assert r["tempo_fazy2"]["max"] == pytest.approx(0.01)
    assert r["pcf_bez_odczytu"] == 1 and r["pcf_vl"] == 1 and r["pcf_n"] == 5
    assert r["pcf"]["b"] == pytest.approx(36.0, abs=1e-9)
    assert r["pcf"]["b"] / 3.6 == pytest.approx(10.0)
    raport = A.raport_dryfu(r, "dryf.csv")
    assert "wybudzenie 4, 3,50 h od startu, panel − NTP 126,0 s" in raport
    assert "Bezpiecznik 2 min zadziałał po 3,50 h" in raport


def test_dryf_bezpiecznik_bez_komentarza():
    # komentarz o bezpieczniku urwany: faza 2 i tak od wiersza po |panel − NTP| ≥ 2 min
    tekst = NAGLOWEK + "\n1;1000000;0.000;0;900;16;0\n2;8200000;2.000;130000;970;16;0\n3;9000000;2.222;100;980;16;0\n"
    w, _, _ = A.parsuj_dryf(tekst)
    assert [x.faza for x in w] == [1, 1, 2]


def test_dryf_pcf_zero_to_brak_odczytu():
    # firmware zapisuje różnicę 0 jako 1 (main.cpp `x ? x : 1`), więc 0 w pliku = brak odczytu
    w, _, _ = A.parsuj_dryf(NAGLOWEK + "\n1;1000000;0.000;0;0;16;0\n")
    assert w[0].pcf_ms is None


def test_dryf_urwany_wiersz_i_puste_linie(tmp_path):
    tekst = "\n" + PIEC + "\n\n6;1793617;1.4"  # ostatni wiersz urwany w trakcie zapisu
    w, _, pom = A.parsuj_dryf(tekst)
    assert len(w) == 5 and len(pom) == 1 and "3 pól" in pom[0]
    kod, out, _ = uruchom(tmp_path, "dryf", tresc=tekst)
    assert kod == 0 and "Pominięte linie (1)" in out


def test_dryf_zly_plik(tmp_path):
    kod, out, err = uruchom(tmp_path, "dryf", tresc="1793611907;3950;0;-1;start\n")
    assert kod == 2 and "nagłówka" in err and out == ""
    kod, _, err = uruchom(tmp_path, "dryf", tresc=NAGLOWEK + "\n")
    assert kod == 2 and "żadnego poprawnego wiersza" in err
    kod, _, err = uruchom(tmp_path, "dryf", nazwa="nie-ma.csv")
    assert kod == 2 and "Nie ma pliku" in err
    (tmp_path / "bin.csv").write_bytes(b"\xff\xfe\x00\x81")
    kod, _, err = uruchom(tmp_path, "dryf", nazwa="bin.csv")
    assert kod == 2 and "UTF-8" in err


# ---------------------------------------------------------------- bateria

def ep(rok, mies, dzien, godz, minuta=0):
    """Chwila w UTC jako epoch."""
    return int(dt.datetime(rok, mies, dzien, godz, minuta, tzinfo=dt.timezone.utc).timestamp())


def test_parser_baterii_jak_cpp():
    assert A.parsuj_linie_baterii("1793611907;3950;6000;1500;zegar:okno_poranne+czujniki").mV == 3950
    assert A.parsuj_linie_baterii("1793611907;3950;0;-1;start\r").aktywny_ms == -1
    for zla in ["1793611907;3950;0;-1", "1793611907;3950;0;-1;", "0;3950;0;0;zegar", "1;0;0;0;zegar",
                "1;6001;0;0;zegar", "1;3950;x;0;zegar", "1;3950;1.5;0;zegar", "1;3950;0;0;zegar;x", "1; 3950;0;0;z"]:
        assert A.parsuj_linie_baterii(zla) is None, zla


def test_bateria_doba_zmiany_czasu_25h():
    tz = A.strefa()
    # 25.10.2026: 00:00 CEST = 24.10 22:00 UTC, 26.10 00:00 CET = 25.10 23:00 UTC → 25 h
    assert A.godzin_doby(dt.date(2026, 10, 25), tz) == 25
    assert A.godzin_doby(dt.date(2026, 3, 29), tz) == 23
    # wpisy co 30 min od 24.10 22:00 UTC do 25.10 22:30 UTC = 50 wpisów w dobie lokalnej 25.10;
    # wpis 24.10 21:59 UTC = 24.10 23:59 CEST, wpis 25.10 23:00 UTC = 26.10 00:00 CET
    wiersze = ["{};4000;0;1000;zegar:czujniki".format(ep(2026, 10, 24, 21, 59))]
    t = ep(2026, 10, 24, 22)
    for i in range(50):
        wiersze.append("{};4000;{};1000;zegar:czujniki".format(t + i * 1800, 5000 if i < 4 else 0))
    wiersze.append("{};4000;0;1000;zegar:".format(ep(2026, 10, 25, 23)))
    wpisy, pom = A.parsuj_baterie("\n".join(wiersze))
    r = A.analiza_baterii(wpisy, None, None)
    d = r["doby"]
    assert sorted(d) == [dt.date(2026, 10, 24), dt.date(2026, 10, 25), dt.date(2026, 10, 26)]
    s = d[dt.date(2026, 10, 25)]
    assert s["n"] == 50 and s["godzin"] == 25 and not s["niepelna"]
    # 4 wybudzenia × 5 000 ms = 20 000 ms = 0,333 min
    assert s["z_wifi"] == 4 and s["wifi_ms"] == 20000
    assert d[dt.date(2026, 10, 26)]["powody"] == {"zegar:": 1}
    raport = A.raport_baterii(r, "bateria.csv", len(pom), None, None)
    assert "| 25.10.2026 | 25 | 50 | 4 | 0,3 (w budżecie) | zegar:czujniki 50 |" in raport
    assert "zegar: (bez powodu) 1" in raport


def test_bateria_aktywnosc_z_nastepnego_wiersza_i_wifi():
    # wiersz i+1 niesie aktywny_ms wybudzenia i (bateria.h). 11 wybudzeń: parzyste z Wi-Fi 1 000…5 000 ms,
    # nieparzyste bez Wi-Fi; aktywność: bez Wi-Fi 1 500 ms, z Wi-Fi 9 000 ms; ostatni wiersz bez następnika.
    t = ep(2026, 11, 3, 12)
    wiersze = []
    for i in range(11):
        wifi = (i // 2 + 1) * 1000 if i % 2 == 0 else 0
        ak = -1 if i == 0 else (9000 if (i - 1) % 2 == 0 else 1500)
        powod = "start" if i == 0 else ("przycisk" if i == 3 else ("zegar:okno_poranne" if wifi else "zegar:czujniki"))
        wiersze.append("{};4000;{};{};{}".format(t + i * 900, wifi, ak, powod))
    wpisy, _ = A.parsuj_baterie("\n".join(wiersze))
    r = A.analiza_baterii(wpisy, None, None)
    a = r["aktywnosc"]
    # bez Wi-Fi: i = 1, 3, 5, 7, 9; i = 3 to przycisk → zegar bez Wi-Fi: 4 × 1,5 s
    assert a["zegar"]["n"] == 4 and a["zegar"]["mediana"] == 1.5
    assert a["przycisk"]["n"] == 1
    # z Wi-Fi: i = 0, 2, 4, 6, 8 mają następnika (i = 10 nie) → 5 × 9 s
    assert a["wifi"]["n"] == 5 and a["wifi"]["max"] == 9.0
    # Wi-Fi na wybudzenie: i = 0…10 parzyste: 1, 2, 3, 4, 5, 6 s; mediana 3,5; P90 ranga ceil(5,4) = 6 → 6 s
    w = r["wifi"]
    assert (w["n"], w["min"], w["mediana"], w["p90"], w["max"]) == (6, 1.0, 3.5, 6.0, 6.0)
    assert r["wifi_ponad"] == 0 and r["bez_wifi_ponad"] == 0


def test_bateria_regresja_bez_ladowania():
    # 4,100 V na starcie, spadek 0,050 V/dobę, wpis co 30 min przez 3 doby (145 wpisów).
    # Ostatnie napięcie 4,100 − 3 × 0,050 = 3,950 V → (3,950 − 3,5) / 0,05 = 9,0 dni do 3,5 V;
    # razem 3 + 9 = 12 dni wobec 30.
    t = ep(2026, 11, 1, 0)
    wiersze = ["{};{};0;1000;zegar:czujniki".format(t + i * 1800, round(4100 - 50 * i / 48)) for i in range(145)]
    wpisy, _ = A.parsuj_baterie("\n".join(wiersze))
    tr = A.trend_baterii(wpisy)
    assert tr["jest"] and not tr["ladowania"]
    assert tr["v_na_dobe"] == pytest.approx(-0.050, abs=0.0005)
    assert tr["v"] == pytest.approx(3.950)
    assert tr["dni_do_progu"] == pytest.approx(9.0, abs=0.1)
    assert tr["doby"] + tr["dni_do_progu"] == pytest.approx(12.0, abs=0.1)


def test_bateria_ladowanie_okno_od_spadku_po_szczycie(tmp_path):
    # Doba spadku z 3,900 V (0,1 V/dobę, co 30 min, 48 wpisów), potem ładowanie przy kablu: +20 mV co wpis do
    # 4,180 V (wzrost netto ≥ 60 mV wobec minimum z 12 wpisów → ładowanie), potem 2 doby spadku 0,040 V/dobę.
    # Okno trendu zaczyna się od pierwszego spadku po szczycie 4,180 V, nachylenie −0,040 V/dobę.
    t = ep(2026, 11, 1, 0)
    mv = []
    for i in range(48):
        mv.append(round(3900 - 100 * i / 48))
    dol = mv[-1]
    while dol < 4180:
        dol = min(4180, dol + 20)
        mv.append(dol)
    for i in range(1, 97):
        mv.append(round(4180 - 40 * i / 48))
    wiersze = ["{};{};0;1000;zegar:czujniki".format(t + i * 1800, v) for i, v in enumerate(mv)]
    tresc = "\n".join(wiersze) + "\n"
    wpisy, _ = A.parsuj_baterie(tresc)
    tr = A.trend_baterii(wpisy)
    assert len(tr["ladowania"]) == 1
    lad = tr["ladowania"][0]
    # napięcie wygładzone medianą z 3 (jak bateria.cpp): przy szczycie surowe 4 162, 4 180, 4 179 → 4 179 mV
    # (wpis nr 67); ładowanie wykryte na wpisie nr 51: mediana 3 882 − minimum z 12 poprzednich 3 804 = 78 ≥ 60 mV
    assert lad["do_mV"] == 4179 and lad["od_mV"] == 3804
    assert lad["ep"] == t + 51 * 1800 and lad["szczyt_ep"] == t + 67 * 1800
    assert tr["jest"]
    assert tr["v_na_dobe"] == pytest.approx(-0.040, abs=0.001)
    # wpis nr 68 ma medianę równą szczytowi (4 180, 4 179, 4 178 → 4 179), więc okno od pierwszego spadku po nim:
    # wpisy 68…162 = 95 wpisów, 94 × 30 min = 1,958 doby; ostatnie 4,100 V → (4,100 − 3,5) / 0,040 = 15,0 dni
    assert tr["n"] == 95
    assert tr["od_ep"] == t + 68 * 1800
    assert tr["doby"] == pytest.approx(94 / 48, abs=1e-6)
    assert tr["dni_do_progu"] == pytest.approx(15.0, abs=0.05)
    kod, out, err = uruchom(tmp_path, "bateria", tresc=tresc)
    assert kod == 0, err
    assert "| Ładowanie wykryte |" in out and "4,179 V" in out and "`[szacunek]`" in out
    assert "krzywa napięcia baterii litowo-jonowej nie jest liniowa" in out


def test_bateria_za_krotko_na_trend():
    t = ep(2026, 11, 1, 0)
    wpisy, _ = A.parsuj_baterie("\n".join("{};4000;0;1;zegar".format(t + i * 1800) for i in range(10)))
    tr = A.trend_baterii(wpisy)
    assert not tr["jest"] and tr["godzin"] == pytest.approx(4.5)


def test_bateria_filtr_od_do_i_urwany_wiersz(tmp_path):
    # 3 doby lokalne po 2 wpisy (12:00 UTC = 13:00 CET), plus urwany ostatni wiersz i pusta linia
    wiersze = []
    for d in (1, 2, 3):
        wiersze += ["{};4000;0;1000;zegar:czujniki".format(ep(2026, 11, d, 12)),
                    "{};4000;3000;1000;zegar:okno_poranne".format(ep(2026, 11, d, 13))]
    tresc = "\n".join(wiersze) + "\n\n" + "1793900000;39"
    kod, out, err = uruchom(tmp_path, "bateria", "--od", "2026-11-02", "--do", "2026-11-02", tresc=tresc)
    assert kod == 0, err
    assert "| 02.11.2026 | 24 | 2 | 1 | 0,1 (w budżecie) |" in out
    assert "01.11.2026" not in out.split("### Doby")[1].split("###")[0]
    assert "pominięte złe wiersze w całym pliku: 1" in out
    kod, _, err = uruchom(tmp_path, "bateria", "--od", "2026-12-01", tresc=tresc)
    assert kod == 2 and "nie ma żadnego wpisu" in err
    kod, _, err = uruchom(tmp_path, "bateria", "--od", "2026-11-03", "--do", "2026-11-01", tresc=tresc)
    assert kod == 2 and "późniejsza" in err
    kod, _, err = uruchom(tmp_path, "bateria", "--od", "03.11.2026", tresc=tresc)
    assert kod == 2 and "RRRR-MM-DD" in err


def test_bateria_zly_plik(tmp_path):
    kod, out, err = uruchom(tmp_path, "bateria", tresc=PIEC)
    assert kod == 2 and "bateria.csv" in err and out == ""


def test_bateria_wifi_ponad_budzet():
    # 12 wybudzeń po 60 s Wi-Fi = 12 min > 10 min budżetu, ≤ 45 min limitu doby bez danych
    t = ep(2026, 11, 2, 6)
    wpisy, _ = A.parsuj_baterie("\n".join("{};4000;60000;1000;zegar:ponowienie".format(t + i * 1800)
                                          for i in range(12)))
    r = A.analiza_baterii(wpisy, None, None)
    raport = A.raport_baterii(r, "b.csv", 0, None, None)
    assert "12,0 (ponad budżet, w limicie doby bez danych)" in raport
    assert r["wifi_ponad"] == 12
