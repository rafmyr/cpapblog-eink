# SPDX-License-Identifier: GPL-3.0-or-later
"""Symulator doby: doba na zrzucie syntetycznym wersji 0 i 1, przyciski, odmowy WY12.

Kompiluje firmware/prototyp/test/symulator.cpp z src/*.cpp (cache w build/symulator/, pierwszy raz kilkanaście
sekund). Dane syntetyczne z tools/symulator/generuj_zrzut.py, bez prawdziwych danych zdrowotnych.
"""
import importlib.util
import os
import re

import pytest

TU = os.path.dirname(os.path.abspath(__file__))


def modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa + "_t", os.path.join(TU, "symulator", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


sym = modul("symuluj")
gen = modul("generuj_zrzut")
WZOR = re.compile(r"^\d{3}-\d{8}-\d{4}-[A-Za-z0-9_-]+\.png$")


def png(k):
    return sorted(x for x in os.listdir(k) if x.endswith(".png"))


@pytest.fixture(scope="module")
def zrzut_v0(tmp_path_factory):
    k = str(tmp_path_factory.mktemp("zrzut_v0"))
    gen.generuj(k, wersja=0, nocy=60)
    return k


@pytest.fixture(scope="module")
def doba_v0(zrzut_v0, tmp_path_factory):
    przyc = tmp_path_factory.mktemp("p") / "przyciski.txt"
    # ▶ z ekranu dnia → K1 (wcześniej ↻)
    przyc.write_text("# poranek: ekran dnia, potem K1\n2026-11-04T07:15 prawy\n", encoding="utf-8")
    przed = {n: open(os.path.join(zrzut_v0, n), "rb").read() for n in os.listdir(zrzut_v0)}
    w = sym.symuluj(zrzut_v0, "2026-11-04T05:00", "2026-11-04T23:59", str(przyc),
                    str(tmp_path_factory.mktemp("wynik_v0")), cicho=True)
    return w, przed


def test_doba_v0_ekran_dnia_rano_K1_i_dobranoc(doba_v0):
    w, _ = doba_v0
    p = png(w)
    assert p and all(WZOR.match(x) for x in p), p
    assert p[0].startswith("001-20261104-0500-dzien-")  # ekran dnia z nocą ze zrzutu
    # prawy o 07:15 → K1 (przed nim są obrazki slotu dziennego z atrapą prognozy, więc numer dowolny)
    assert any(x[3:].startswith("-20261104-0715-K1") for x in p)
    dobranoc = [x for x in p if x.endswith("-Dobranoc.png")]
    assert dobranoc and dobranoc[0].split("-")[2] in ("2325", "2330", "2335")
    log = open(os.path.join(w, "log.txt"), encoding="utf-8").read()
    assert "wersja 0" in log and "przycisk ▶ prawy" in log
    assert "zegar:poczatek_nocy" in log
    panel = open(os.path.join(w, "log-panelu.txt"), encoding="utf-8").read()
    # migracja 0 → wersja znana firmware (noce w wersji 2, reszta 1)
    assert re.search(r"migracja noce\.csv: wersja 0 → \d, 60 rekordów", panel), panel[:800]
    for x in p:  # PNG 800×480
        d = open(os.path.join(w, x), "rb").read(24)
        assert d[:8] == b"\x89PNG\r\n\x1a\n" and int.from_bytes(d[16:20], "big") == 800


def test_zrzut_nietkniety(doba_v0, zrzut_v0):
    _, przed = doba_v0
    assert {n: open(os.path.join(zrzut_v0, n), "rb").read() for n in os.listdir(zrzut_v0)} == przed


def test_zrzut_v1_i_bez_sieci(tmp_path):
    z = str(tmp_path / "z1")
    gen.generuj(z, wersja=1, nocy=60)
    w = sym.symuluj(z, "2026-11-04T05:00", "2026-11-04T12:00", wyjscie=str(tmp_path / "w1"), bez_sieci=True,
                    cicho=True)
    p = png(w)
    assert p[0].startswith("001-20261104-0500-dzien-")
    log = open(os.path.join(w, "log.txt"), encoding="utf-8").read()
    assert "wersja 1" in log and "bez Wi-Fi" in log and "Wi-Fi tak" not in log


def test_odmowa_danych_i_wyniku_w_repo(zrzut_v0, tmp_path):
    with pytest.raises(sym.Blad, match="WY12"):
        sym.symuluj(os.path.join(sym.REPO, "docs"), "2026-11-04T05:00", "2026-11-04T06:00", cicho=True)
    with pytest.raises(sym.Blad, match="WY12"):
        sym.symuluj(zrzut_v0, "2026-11-04T05:00", "2026-11-04T06:00", wyjscie=os.path.join(sym.REPO, "x"),
                    cicho=True)


def test_bledy_po_polsku(zrzut_v0, tmp_path):
    with pytest.raises(sym.Blad, match="nie rozumiem daty"):
        sym.symuluj(zrzut_v0, "5 listopada", "2026-11-04T06:00", cicho=True)
    with pytest.raises(sym.Blad, match="później"):
        sym.symuluj(zrzut_v0, "2026-11-04T06:00", "2026-11-04T05:00", cicho=True)
    zly = tmp_path / "p.txt"
    zly.write_text("2026-11-04T07:15 gorny\n", encoding="utf-8")
    with pytest.raises(sym.Blad, match="wiersz 1: nieznany przycisk"):
        sym.przyciski(str(zly))
    zly.write_text("2026-11-04T07:15 lewy 5s\n", encoding="utf-8")
    with pytest.raises(sym.Blad, match="tylko 2s"):
        sym.przyciski(str(zly))
    with pytest.raises(sym.Blad, match="nie ma katalogu"):
        sym.symuluj(str(tmp_path / "brak"), "2026-11-04T05:00", "2026-11-04T06:00", cicho=True)


def test_przyciski_postac_dla_symulatora(tmp_path):
    p = tmp_path / "p.txt"
    p.write_text("2026-11-04T07:15 środkowy\n2026-11-04T08:00:30 ◀ 2s  # przytrzymanie\n\n", encoding="utf-8")
    assert sym.przyciski(str(p)) == ["2026-11-04T07:15:00 srodkowy", "2026-11-04T08:00:30 lewy 2s"]


def test_katalog_z_podkatalogami_zrzutu(tmp_path):
    gen.generuj(str(tmp_path / "zrzut" / "a"))
    gen.generuj(str(tmp_path / "zrzut" / "b"))
    with pytest.raises(sym.Blad, match="podkatalogach: a, b"):
        sym.symuluj(str(tmp_path / "zrzut"), "2026-11-04T05:00", "2026-11-04T06:00", cicho=True)


def test_znaczniki_nazw_ekranow_zgodne_z_dyrygentem(doba_v0):
    # Nazwa ekranu dnia w PNG pochodzi z wierszy logu dyrygenta (test/symulator.cpp ZNACZNIK_*); gdy dyrygent zmieni
    # ich format, ten test pada, zamiast żeby obrazki dostały cicho złe nazwy.
    with open(os.path.join(sym.FW, "src", "dyrygent.cpp"), encoding="utf-8") as f:
        d = f.read()
    for wzor in ('log("ekran nocny (%s)', 'log("ekran dnia %s (%s)', 'log("S5: %s'):
        assert wzor in d, wzor
    with open(os.path.join(sym.FW, "test", "symulator.cpp"), encoding="utf-8") as f:
        s = f.read()
    for znacznik in ('ZNACZNIK_NOCNY = "ekran nocny ("', 'ZNACZNIK_DNIA = "ekran dnia "', 'ZNACZNIK_S5 = "S5: "'):
        assert znacznik in s, znacznik
    w, _ = doba_v0
    assert not [x for x in png(w) if "nieznany" in x], png(w)
    panel = open(os.path.join(w, "log-panelu.txt"), encoding="utf-8").read()
    assert len(re.findall(r"  EKRAN \S+ \d{3}-", panel)) == len(png(w))


def test_plik_przyciskow_rtf(tmp_path):
    p = tmp_path / "p.txt"
    p.write_text("{\\rtf1\\ansi 2026-11-04T07:15 srodkowy}", encoding="utf-8")
    with pytest.raises(sym.Blad, match="RTF"):
        sym.przyciski(str(p))


def test_lista_zrodel_z_jednej_reguly(zrzut_v0, tmp_path, monkeypatch):
    # nowy adapter ESP32 (z Arduino.h) nie wchodzi do symulatora, nowy plik rdzenia wchodzi bez dopisywania
    import shutil
    fw = tmp_path / "fw"
    shutil.copytree(sym.FW, fw, ignore=shutil.ignore_patterns(".pio"))
    (fw / "src" / "ota_esp32.cpp").write_text("#include <Arduino.h>\nvoid ota() {}\n")
    (fw / "src" / "nowy_rdzen.cpp").write_text("int nowyRdzen() { return 1; }\n")
    monkeypatch.setattr(sym, "FW", str(fw))
    monkeypatch.setattr(sym, "BUDOWA", str(tmp_path / "budowa"))
    sym.symuluj(zrzut_v0, "2026-11-04T05:00", "2026-11-04T06:00", wyjscie=str(tmp_path / "w"), cicho=True)
    assert (tmp_path / "budowa" / "nowy_rdzen.o").exists()
    assert not (tmp_path / "budowa" / "ota_esp32.o").exists()


def test_zrodla_sh_w_regule_symulatora():
    # każdy plik ZRODLA z tools/prototyp/zrodla.sh (testy.sh) jest też w symulatorze
    with open(os.path.join(TU, "prototyp", "zrodla.sh"), encoding="utf-8") as f:
        t = f.read()
    blok = t[t.index("ZRODLA=("):t.index(")", t.index("ZRODLA=("))]
    pliki = set(re.findall(r"\$FW/src/(\w+\.cpp)", blok))
    assert pliki and pliki <= {os.path.basename(p) for p in sym.zrodla_rdzenia()}


def test_log_z_data_strefa_i_odmiana(zrzut_v0, tmp_path):
    w = sym.symuluj(zrzut_v0, "2026-11-04T05:00", "2026-11-04T05:10", wyjscie=str(tmp_path / "w"), cicho=True)
    t = open(os.path.join(w, "log.txt"), encoding="utf-8").read()
    assert re.search(r"sen do 2026-11-04 05:\d\d:\d\d CET · strefa CET", t), t
    assert "6 plików" in t and "1 narysowany ekran." in t, t
    assert [sym.ile(n, "obrazek", "obrazki", "obrazków") for n in (1, 2, 5, 12, 22, 25)] == [
        "1 obrazek", "2 obrazki", "5 obrazków", "12 obrazków", "22 obrazki", "25 obrazków"]


def test_zmiana_czasu_wiersze_odroznialne(tmp_path):
    import datetime as dt
    z = str(tmp_path / "z")
    gen.generuj(z, ostatni=dt.date(2026, 10, 24))
    w = sym.symuluj(z, "2026-10-24T20:00", "2026-10-25T09:00", wyjscie=str(tmp_path / "w"), cicho=True)
    t = open(os.path.join(w, "log.txt"), encoding="utf-8").read().splitlines()
    dwa = [x for x in t if x.startswith("2026-10-25 02:00:05")]
    assert len(dwa) == 2 and {x.rsplit("strefa ", 1)[1] for x in dwa} == {"CEST", "CET"}, dwa


def test_blad_bez_podwojnego_przedrostka(tmp_path):
    z = str(tmp_path / "z")
    gen.generuj(z)
    open(os.path.join(z, "noce.csv"), "w").close()
    with pytest.raises(sym.Blad) as e:
        sym.symuluj(z, "2026-11-04T05:00", "2026-11-04T06:00", wyjscie=str(tmp_path / "w"), cicho=True)
    assert "BŁĄD" not in str(e.value), str(e.value)


def test_tylda_i_wynik_w_zrzucie(tmp_path, monkeypatch):
    monkeypatch.setenv("HOME", str(tmp_path))
    gen.generuj(str(tmp_path / "zrzut"))
    w = sym.symuluj("~/zrzut", "2026-11-04T05:00", "2026-11-04T06:00", cicho=True)
    assert w.startswith(str(tmp_path / "cpap-eink-symulator")) and png(w)
    with pytest.raises(sym.Blad, match="w katalogu zrzutu"):
        sym.symuluj("~/zrzut", "2026-11-04T05:00", "2026-11-04T06:00", wyjscie="~/zrzut/wynik", cicho=True)


def test_zegar_nieznany_bez_sieci_i_opcja_zegar_znany(zrzut_v0, tmp_path):
    w = sym.symuluj(zrzut_v0, "2026-11-04T05:00", "2026-11-04T07:00", wyjscie=str(tmp_path / "a"), bez_sieci=True,
                    cicho=True)
    t = open(os.path.join(w, "log.txt"), encoding="utf-8").read()
    assert "[zegar panelu nieznany]" in t and "nieznany po starcie zasilania" in t
    w = sym.symuluj(zrzut_v0, "2026-11-04T05:00", "2026-11-04T07:00", wyjscie=str(tmp_path / "b"), bez_sieci=True,
                    zegar_znany=True, cicho=True)
    t = open(os.path.join(w, "log.txt"), encoding="utf-8").read()
    assert "[zegar panelu nieznany]" not in t and "--zegar-znany" in t
    w = sym.symuluj(zrzut_v0, "2026-11-04T05:00", "2026-11-04T07:00", wyjscie=str(tmp_path / "c"), cicho=True)
    assert "[zegar panelu nieznany]" not in open(os.path.join(w, "log.txt"), encoding="utf-8").read()


def test_bez_ntp_czas_z_airbridge(zrzut_v0, tmp_path):
    w = sym.symuluj(zrzut_v0, "2026-11-04T05:00", "2026-11-04T07:00", wyjscie=str(tmp_path / "a"), bez_ntp=True, cicho=True)
    t = open(os.path.join(w, "log-panelu.txt"), encoding="utf-8").read()
    assert "źródło czasu: AirBridge" in t and "NTP: próba 3 z 3 bez odpowiedzi" in t
    assert "[zegar panelu nieznany]" not in open(os.path.join(w, "log.txt"), encoding="utf-8").read()
    assert png(w)


@pytest.mark.parametrize("wersja", [1, 2])
def test_zrzut_v1_v2_doba(tmp_path, wersja):
    z = str(tmp_path / "z")
    gen.generuj(z, wersja=wersja, nocy=60)
    with open(os.path.join(z, "dane", "noce", "2026.csv"), encoding="utf-8") as f:
        w = f.read().splitlines()
    assert w[0] == f"#cpap-eink noce {wersja}" and w[2].count(";") == (20 if wersja == 1 else 40)
    k = sym.symuluj(z, "2026-11-04T05:00", "2026-11-04T23:59", wyjscie=str(tmp_path / "w"), cicho=True)
    p = png(k)
    assert p[0].startswith("001-20261104-0500-dzien-") and any(x.endswith("-Dobranoc.png") for x in p), p
    t = open(os.path.join(k, "log.txt"), encoding="utf-8").read()
    assert " 60 nocy " in t and "wersja 1 lub nowsza" in t
    assert "HTTP 404" not in open(os.path.join(k, "log-panelu.txt"), encoding="utf-8").read()


def test_drugi_ekran_poranka_sk_bez_podwojnego_podpisu_portretu(tmp_path):
    """(R-POR-24): zrzut syntetyczny z przebiegiem ostatniej nocy
    (`/dane/portret.csv`): o 05:00 Portret (S11), po 30 min drugi ekran poranka S-K-10 jako ekran danych, rysowany przez
    rysujB16 → pokazB16(), który drugi raz woła prawoZPortretem() na odczycie czujnika z danych dnia. Stopka ma
    `▶ portret` dokładnie raz (bez zabezpieczenia w prawoZPortretem() byłoby `▶ portret     ▶ portret     przy łóżku`)."""
    spec = importlib.util.spec_from_file_location("przebieg_t", os.path.join(TU, "portret", "przebieg.py"))
    przebieg = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(przebieg)
    zr = str(tmp_path / "zrzut")
    gen.generuj(zr, wersja=1, nocy=60)
    odc = {m // 5: {"od": m, "do": 5, "wyciek95": 5, "wyc_s": 0, "zew": 0, "centr": 0, "ps": 60, "maks": 0}
           for m in range(660, 1080, 5)}
    pr = {"data": "2026-11-04", "sesje": [(660, 1080)], "odcinki": odc, "zdarzenia": [], "braki": ""}
    with open(os.path.join(zr, "dane", "portret.csv"), "w", encoding="utf-8") as f:
        f.write(przebieg.tekst_pliku(pr))
    w = sym.symuluj(zr, "2026-11-04T05:00", "2026-11-04T06:00", None, str(tmp_path / "wynik"), cicho=True,
                    zegar_znany=True)
    p = png(w)
    assert any("-0500-dzien-S11" in x for x in p), p
    log = open(os.path.join(w, "log-panelu.txt"), encoding="utf-8").read()
    assert "portret: drugi ekran poranka, Portret pod ▶ (R-POR-24)" in log
    assert re.search(r"ekran S-K-\d+ \(R\w+\): ekran danych", log), "drugi ekran nie jest ekranem danych S-K"
    stopki = re.findall(r"stopka z odczytem: „([^”]*)”", log)
    assert stopki and all(s.count("▶ portret") == 1 for s in stopki), stopki
