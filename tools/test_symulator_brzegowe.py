# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy przypadków brzegowych symulatora doby (tools/symulator/symuluj.py).

Testy z komentarzem „usterka” opisują poprawiony błąd i blokują jego powrót. Pozostałe
potwierdzają, że dany punkt listy kontrolnej trzyma. Dane wyłącznie syntetyczne (tools/symulator/generuj_zrzut.py).
"""
import datetime as dt
import importlib.util
import os
import re
import shutil
import stat
import subprocess

import pytest

TU = os.path.dirname(os.path.abspath(__file__))


def modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa + "_r", os.path.join(TU, "symulator", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


sym = modul("symuluj")
gen = modul("generuj_zrzut")


def pngi(k):
    return sorted(x for x in os.listdir(k) if x.endswith(".png"))


def log(k, nazwa="log.txt"):
    with open(os.path.join(k, nazwa), encoding="utf-8") as f:
        return f.read()


@pytest.fixture(scope="module")
def zrzut(tmp_path_factory):
    k = str(tmp_path_factory.mktemp("zrzut_r"))
    gen.generuj(k)
    return k


def uruchom(zrzut_k, od, do, tmp_path, nazwa="w", przyciski=None, **kw):
    p = None
    if przyciski is not None:
        p = str(tmp_path / (nazwa + "-przyciski.txt"))
        with open(p, "w", encoding="utf-8") as f:
            f.write(przyciski)
    return sym.symuluj(zrzut_k, od, do, p, wyjscie=str(tmp_path / nazwa), cicho=True, **kw)


# ---------- usterki (mają padać do poprawki) ----------

def test_noce_z_przyszlosci_wobec_od_sa_zglaszane(tmp_path):
    # usterka: zrzut ma noce po --od (np. powtórka dnia sprzed zrzutu); panel widzi „noce z przyszłości”,
    # co 30 min rysuje od nowa i szuka nocy; log.txt nie mówi, że wynik nie odpowiada żadnej realnej dobie.
    z = str(tmp_path / "z")
    gen.generuj(z, ostatni=dt.date(2026, 11, 10))
    w = uruchom(z, "2026-11-04T05:00", "2026-11-04T12:00", tmp_path)
    assert re.search(r"(po początku okresu|z przyszłości|późniejsze niż --od)", log(w)), log(w)[:600]


def test_wynik_w_niepustym_katalogu_nie_miesza_obrazkow(zrzut, tmp_path):
    # usterka: --wyjscie do katalogu z poprzednim wynikiem zostawia stare PNG obok nowych; log mówi 1 ekran,
    # w katalogu są 3 obrazki.
    w = uruchom(zrzut, "2026-11-04T05:00", "2026-11-04T23:59", tmp_path, "staly")
    uruchom(zrzut, "2026-11-04T05:00", "2026-11-04T06:00", tmp_path, "staly")
    # odmiana liczebników po R12 (np. „23 wybudzenia, 3 narysowane ekrany”): wzór przyjmuje każdą formę
    m = re.search(r"Razem: \d+ wybudze\w+, (\d+) narysowan", log(w))
    assert len(pngi(w)) == int(m.group(1)), pngi(w)


def test_przycisk_po_koncu_okresu_jest_w_logu(zrzut, tmp_path):
    # usterka: naciśnięcie po --do (np. literówka w dacie) znika bez słowa; przed --od jest „pominięty”.
    w = uruchom(zrzut, "2026-11-04T05:00", "2026-11-04T12:00", tmp_path, przyciski="2026-11-05T07:15 srodkowy\n")
    assert "2026-11-05 07:15" in log(w), log(w)[-400:]


def test_brak_narzedzi_apple_daje_komunikat_xcode_select(zrzut, tmp_path, monkeypatch):
    # usterka: na Macu bez Command Line Tools /usr/bin/g++ istnieje (to ten sam shim xcrun co python3),
    # więc kompilator() go znajduje, a użytkownik dostaje „kompilacja … się nie udała: xcrun: error …” zamiast
    # obiecanego w README „nie znalazłem kompilatora … xcode-select --install”.
    shim = tmp_path / "g++"
    shim.write_text("#!/bin/sh\necho 'xcrun: error: invalid active developer path (/Library/Developer/CommandLineTools),"
                    " missing xcrun at: /Library/Developer/CommandLineTools/usr/bin/xcrun' >&2\nexit 1\n")
    shim.chmod(shim.stat().st_mode | stat.S_IEXEC)
    monkeypatch.setenv("CXX", str(shim))
    monkeypatch.setattr(sym, "BUDOWA", str(tmp_path / "budowa"))
    with pytest.raises(sym.Blad, match="xcode-select --install"):
        uruchom(zrzut, "2026-11-04T05:00", "2026-11-04T06:00", tmp_path)


def test_R5a_plik_przyciskow_z_textedit_utf16_daje_blad_po_polsku(tmp_path):
    # usterka: plik przycisków nie w UTF-8 (TextEdit, „Unicode UTF-16”) kończy się śladem stosu
    # UnicodeDecodeError zamiast komunikatu.
    p = tmp_path / "przyciski.txt"
    p.write_bytes("2026-11-04T07:15 srodkowy\n".encode("utf-16"))
    with pytest.raises(sym.Blad):
        sym.przyciski(str(p))


def test_R5b_wyjscie_bedace_plikiem_daje_blad_po_polsku(zrzut, tmp_path):
    # usterka: --wyjscie wskazujące plik kończy się śladem stosu FileExistsError.
    plik = tmp_path / "plik"
    plik.write_text("x")
    with pytest.raises(sym.Blad):
        sym.symuluj(zrzut, "2026-11-04T05:00", "2026-11-04T06:00", wyjscie=str(plik), cicho=True)


def test_zrzut_w_innym_klonie_repozytorium_odrzucony(tmp_path):
    # WY12 sprawdza tylko repo, z którego uruchomiono skrypt. Zrzut w innym klonie albo drzewie roboczym
    # cpap-eink przechodzi bez odmowy.
    klon = tmp_path / "klon"
    subprocess.run(["git", "init", "-q", str(klon)], check=True)
    subprocess.run(["git", "-C", str(klon), "remote", "add", "origin", "https://github.com/rafmyr/cpap-eink"], check=True)
    gen.generuj(str(klon / "zrzut"))
    with pytest.raises(sym.Blad, match="WY12"):
        sym.symuluj(str(klon / "zrzut"), "2026-11-04T05:00", "2026-11-04T06:00", wyjscie=str(tmp_path / "w"),
                    cicho=True)


def test_pominiete_wiersze_nocy_sa_w_log_txt(tmp_path):
    # usterka: uszkodzone wiersze noce.csv są tylko w log-panelu.txt; log.txt podaje pełną liczbę nocy.
    z = str(tmp_path / "z")
    gen.generuj(z)
    with open(os.path.join(z, "noce.csv"), encoding="utf-8") as f:
        w = f.read().splitlines()
    w.insert(10, "2026-10-0;xx;;;")
    with open(os.path.join(z, "noce.csv"), "w", encoding="utf-8") as f:
        f.write("\n".join(w) + "\n")
    k = uruchom(z, "2026-11-04T05:00", "2026-11-04T06:00", tmp_path)
    assert "pominięte" in log(k, "log-panelu.txt")
    assert "pominięte" in log(k) or "nieczyteln" in log(k), log(k)[:500]


def test_noce_csv_z_bom_nie_gubi_pierwszej_nocy(tmp_path):
    # usterka: noce.csv zapisany z BOM (Numbers/Excel) gubi pierwszą noc bez śladu w log.txt.
    z = str(tmp_path / "z")
    gen.generuj(z)
    p = os.path.join(z, "noce.csv")
    with open(p, encoding="utf-8") as f:
        t = f.read()
    with open(p, "w", encoding="utf-8") as f:
        f.write("﻿" + t)
    k = uruchom(z, "2026-11-04T05:00", "2026-11-04T06:00", tmp_path)
    assert " 60 nocy " in log(k), log(k).splitlines()[1]


# ---------- trzyma (dowody do raportu) ----------

def test_trzyma_sciezki_ze_spacjami_i_polskimi_znakami(tmp_path):
    z = str(tmp_path / "z z" / "zrzut ą ę")
    gen.generuj(z)
    w = sym.symuluj(z, "2026-11-04T05:00", "2026-11-04T06:00", wyjscie=str(tmp_path / "w y" / "wynik ł"), cicho=True)
    assert pngi(w)


def test_trzyma_symlink_i_sciezka_wzgledna_do_repo_odrzucone(zrzut, tmp_path, monkeypatch):
    link = tmp_path / "link-repo"
    os.symlink(sym.REPO, link)
    with pytest.raises(sym.Blad, match="WY12"):
        sym.symuluj(str(link / "docs"), "2026-11-04T05:00", "2026-11-04T06:00", cicho=True)
    with pytest.raises(sym.Blad, match="WY12"):
        sym.symuluj(zrzut, "2026-11-04T05:00", "2026-11-04T06:00", wyjscie=str(link / "wynik"), cicho=True)
    monkeypatch.chdir(os.path.join(sym.REPO, "tools"))
    with pytest.raises(sym.Blad, match="WY12"):
        sym.symuluj(zrzut, "2026-11-04T05:00", "2026-11-04T06:00", wyjscie="../wynik", cicho=True)


def test_trzyma_pusty_noce_csv_czytelny_blad(tmp_path):
    z = str(tmp_path / "z")
    gen.generuj(z)
    open(os.path.join(z, "noce.csv"), "w").close()
    with pytest.raises(sym.Blad, match="nie ma żadnej nocy"):
        uruchom(z, "2026-11-04T05:00", "2026-11-04T06:00", tmp_path)


# „mieszanka” (pliki wersji 0 obok dane/noce w wersji 1) padała z przyczyny w src/, nie w symulatorze:
# firmware znający `noce` w wersji 2 przy starcie scala zmieniony /noce.csv z plikiem nowej ścieżki jeszcze w wersji 1,
# zapis pliku czekającego na migrację w miejscu jest odrzucany (T-WYM-55), log „scalenie z plikami nowej ścieżki
# nieudane (plik noce/2026.csv w wersji 1)”, a w tym wybudzeniu panel liczy na 30 nocach z /noce.csv zamiast 60
# (pobiera „zaległe” doby, rysuje KM o 05:01). Następne wybudzenia mają 60 nocy. Naprawione (kolejność migracji
# w src/magazyn.cpp, 04.11.2026), znacznik xfail zdjęty.
@pytest.mark.parametrize("wariant", ["sam_noce", "400", "mieszanka"])
def test_trzyma_warianty_zrzutu(tmp_path, wariant):
    z = str(tmp_path / "z")
    if wariant == "sam_noce":
        gen.generuj(z)
        for n in os.listdir(z):
            if n != "noce.csv":
                os.remove(os.path.join(z, n))
    elif wariant == "400":
        gen.generuj(z, nocy=400)
    else:
        gen.generuj(z, wersja=1)
        gen.generuj(z, wersja=0, nocy=30)
    w = uruchom(z, "2026-11-04T05:00", "2026-11-04T06:00", tmp_path)
    assert pngi(w)[0].startswith("001-20261104-0500-dzien-")
    if wariant == "400":
        assert " 400 nocy " in log(w)
    if wariant == "mieszanka":
        assert "wersja 1" in log(w) and "wersja 0" in log(w)


def test_trzyma_przyciski_ta_sama_minuta_2s_i_powrot_po_5_min(zrzut, tmp_path):
    w = uruchom(zrzut, "2026-11-04T05:00", "2026-11-04T08:00", tmp_path,
                # ▶ K1, ◀ ekran dnia, ▶ K1 (trzy klatki w tej samej minucie), o 07:30 ▶ K2, ◀ 2 s w K2 (nic),
                # więc powrót do ekranu dnia 5 min po 07:31
                przyciski="2026-11-04T07:15 prawy\n2026-11-04T07:15 lewy\n2026-11-04T07:15 prawy\n"
                          "2026-11-04T07:30 prawy\n2026-11-04T07:31 lewy 2s\n")
    p = pngi(w)
    assert sum("-0715-" in x for x in p) == 3, p
    t = log(w)
    assert "przycisk ◀ lewy 2 s" in t
    assert re.search(r"2026-11-04 07:36:0\d · zegar:powrot", t), t  # 5 min po ostatnim naciśnięciu (07:31)


def test_trzyma_zla_godzina_i_przycisk_przed_okresem(zrzut, tmp_path):
    p = tmp_path / "p.txt"
    p.write_text("2026-11-04T25:00 lewy\n", encoding="utf-8")
    with pytest.raises(sym.Blad, match="wiersz 1: nie rozumiem daty"):
        sym.przyciski(str(p))
    w = uruchom(zrzut, "2026-11-04T05:00", "2026-11-04T06:00", tmp_path, przyciski="2026-11-03T07:00 lewy\n")
    assert "przed początkiem okresu: pominięty" in log(w)


@pytest.mark.parametrize("dzien,godziny", [(dt.date(2026, 10, 25), "02"), (dt.date(2026, 3, 29), None)])
def test_trzyma_zmiana_czasu(tmp_path, dzien, godziny):
    z = str(tmp_path / "z")
    gen.generuj(z, ostatni=dzien - dt.timedelta(days=1))
    w = uruchom(z, f"{dzien - dt.timedelta(days=1)}T20:00", f"{dzien}T09:00", tmp_path)
    t = log(w)
    wiersze = [x for x in t.splitlines() if x.startswith(f"{dzien} 02:")]
    if godziny:  # 25.10: godzina 02:xx dwa razy
        assert len([x for x in wiersze if x.startswith(f"{dzien} 02:00:05")]) == 2, wiersze
    else:  # 29.03: 02:xx nie istnieje
        assert not wiersze, wiersze
    assert "okno_poranne" in t and "-Dobranoc.png" in t


def test_trzyma_30_dni(zrzut, tmp_path):
    import time
    t0 = time.time()
    w = uruchom(zrzut, "2026-11-04T05:00", "2026-12-04T05:00", tmp_path)
    assert time.time() - t0 < 60
    assert "zapętlenie" not in log(w)


def test_trzyma_nowy_plik_src_wchodzi_do_kompilacji(zrzut, tmp_path, monkeypatch):
    fw = tmp_path / "fw"
    shutil.copytree(sym.FW, fw, ignore=shutil.ignore_patterns(".pio"))
    (fw / "src" / "reguly_recenzja.cpp").write_text("int regulyRecenzja() { return 1; }\n")
    monkeypatch.setattr(sym, "FW", str(fw))
    monkeypatch.setattr(sym, "BUDOWA", str(tmp_path / "budowa"))
    uruchom(zrzut, "2026-11-04T05:00", "2026-11-04T06:00", tmp_path)
    assert (tmp_path / "budowa" / "reguly_recenzja.o").exists()
