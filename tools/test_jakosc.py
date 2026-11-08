# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy bramek jakości (tools/jakosc/porownaj_tidy.py, tools/jakosc/pokrycie.py)."""

import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "jakosc"))
import porownaj_tidy as pt  # noqa: E402
import pokrycie as pk  # noqa: E402

REPO = pt.REPO
PLIK = os.path.join(REPO, "firmware/prototyp/src/humor.cpp")


def _czytaj(linie):
    return lambda plik: linie.get(plik)


def test_klucz_bez_numeru_linii_i_bez_duplikatu_z_naglowka():
    linie = {"firmware/prototyp/src/a.cpp": ["", "  int x = atoi(s);"],
             "firmware/prototyp/src/a.h": ["struct T {"]}
    wy = "\n".join([
        PLIK.replace("humor.cpp", "a.cpp") + ":2:11: warning: 'atoi' used [cert-err34-c]",
        PLIK.replace("humor.cpp", "a.h") + ":1:8: warning: Excessive padding [clang-analyzer-optin.performance.Padding]",
        PLIK.replace("humor.cpp", "a.h") + ":1:8: warning: Excessive padding [clang-analyzer-optin.performance.Padding]",
        "/usr/include/c++/13/bits/x.h:5:1: warning: systemowe [bugprone-foo]",
        "   12 | kontekst kodu",
    ])
    k = pt.klucze(wy, _czytaj(linie))
    assert k == ["firmware/prototyp/src/a.cpp | cert-err34-c | int x = atoi(s);",
                 "firmware/prototyp/src/a.h | clang-analyzer-optin.performance.Padding | struct T {"]


def test_porownanie_z_baza_liczy_krotnosc():
    baza = ["p | r | a", "p | r | a", "p | s | b"]
    nowe, ust = pt.porownaj(["p | r | a", "p | r | a", "p | r | a"], baza)
    assert nowe == ["p | r | a"] and ust == ["p | s | b"]
    assert pt.porownaj(baza, baza) == ([], [])


def test_przesuniecie_linii_nie_daje_nowego_ostrzezenia():
    przed = pt.klucze(PLIK + ":10:3: warning: w [cert-err34-c]", _czytaj({"firmware/prototyp/src/humor.cpp":
                                                                         [""] * 9 + ["int n = atoi(k);"]}))
    po = pt.klucze(PLIK + ":42:3: warning: w [cert-err34-c]", _czytaj({"firmware/prototyp/src/humor.cpp":
                                                                       [""] * 41 + ["  int n = atoi(k);"]}))
    assert pt.porownaj(po, przed) == ([], [])


def test_goly_nolint(tmp_path):
    p = tmp_path / "x.cpp"
    p.write_text("int a = atoi(s);  // NOLINT\n"
                 "int b = atoi(s);  // NOLINT(cert-err34-c)\n"
                 "int c = atoi(s);  // NOLINT(cert-err34-c): wejście sprawdzone isdigit wyżej\n", encoding="utf-8")
    assert [nr for _, nr, _ in pt.gole_nolint([str(p)])] == [1, 2]


def _gcov(plik, linie, katalog=REPO):
    return json.dumps({"current_working_directory": katalog,
                       "files": [{"file": plik, "lines": [{"line_number": n, "count": c} for n, c in linie]}]})


def test_pokrycie_laczy_naglowek_z_wielu_jednostek_i_pomija_main():
    tekst = "\n".join([
        _gcov("firmware/prototyp/src/a.cpp", [(1, 1), (2, 0)]),
        _gcov(os.path.join(REPO, "firmware/prototyp/src/a.h"), [(5, 0)]),
        _gcov(os.path.join(REPO, "firmware/prototyp/src/a.h"), [(5, 3)]),
        _gcov("firmware/prototyp/src/main.cpp", [(1, 0)]),
        _gcov("/usr/include/c++/13/string", [(1, 0)]),
    ])
    assert pk.suma(pk.linie(tekst)) == (2, 3, 66.67)


def test_zapadka_pokrycia(tmp_path):
    raport = tmp_path / "g.json"
    raport.write_text(_gcov("firmware/prototyp/src/a.cpp", [(1, 1), (2, 1), (3, 0), (4, 1)]), encoding="utf-8")
    baza = tmp_path / "baza.txt"
    baza.write_text("3 4 75.00\n", encoding="utf-8")
    assert pk.main([str(raport), "--baza", str(baza)]) == 0
    baza.write_text("4 4 75.01\n", encoding="utf-8")
    assert pk.main([str(raport), "--baza", str(baza)]) == 1
    assert pk.main([str(raport), "--baza", str(baza), "--zapisz"]) == 0
    assert baza.read_text(encoding="utf-8") == "3 4 75.00\n"


import budzet_pio as bp  # noqa: E402

LOG_PIO = """Linking .pio/build/e1001/firmware.elf
Checking size .pio/build/e1001/firmware.elf
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [===       ]  32.1% (used 105192 bytes from 327680 bytes)
Flash: [=====     ]  52.0% (used 1738000 bytes from 3342336 bytes)
"""


def test_budzet_odczyt_i_progi(tmp_path):
    w = bp.odczyt(LOG_PIO)
    assert w["RAM"][1:] == (105192, 327680) and abs(w["Flash"][0] - 52.0) < 0.1
    log = tmp_path / "pio.log"
    log.write_text(LOG_PIO, encoding="utf-8")
    assert bp.main([str(log)]) == 0
    assert bp.main([str(log), "--flash", "50"]) == 1
    assert bp.main([str(log), "--ram", "30"]) == 1
    log.write_text("Linking\n", encoding="utf-8")
    assert bp.main([str(log)]) == 2


import subprocess  # noqa: E402

SPRAWDZ_SH = os.path.join(REPO, "tools", "jakosc", "sprawdz.sh")


def test_pusty_wynik_tidy_przy_niepustej_bazie_to_blad(tmp_path):
    wy = tmp_path / "tidy.txt"
    wy.write_text("", encoding="utf-8")
    baza = tmp_path / "baza.txt"
    baza.write_text("# baza\np | r | a\n", encoding="utf-8")
    assert pt.main([str(wy), "--baza", str(baza)]) == 1
    baza.write_text("# baza\n", encoding="utf-8")
    assert pt.main([str(wy), "--baza", str(baza)]) == 0


def _bash4():
    """Bash 4 albo nowszy (sprawdz.sh używa mapfile); na macOS systemowy bash to 3.2."""
    import shutil
    for nazwa in ("bash", "/opt/homebrew/bin/bash", "/usr/local/bin/bash"):
        sciezka = shutil.which(nazwa)
        if sciezka and subprocess.run([sciezka, "-c", "(( BASH_VERSINFO[0] >= 4 ))"]).returncode == 0:
            return sciezka
    return None


def _tidy(clang_tidy):
    import pytest
    bash = _bash4()
    if bash is None:
        pytest.skip("sprawdz.sh wymaga bash 4 albo nowszego")
    env = dict(os.environ, CLANG_TIDY=clang_tidy)
    return subprocess.run([bash, SPRAWDZ_SH, "tidy"], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          universal_newlines=True, timeout=120)


def test_bramka_tidy_bez_narzedzia_jest_czerwona(tmp_path):
    w = _tidy(str(tmp_path / "nie-ma-clang-tidy"))
    assert w.returncode == 1
    assert "nie ma clang-tidy" in w.stdout


def test_bramka_tidy_przy_padnieciu_narzedzia_jest_czerwona(tmp_path):
    pada = tmp_path / "clang-tidy"
    pada.write_text("#!/bin/sh\necho 'Segmentation fault' >&2\nexit 139\n", encoding="utf-8")
    pada.chmod(0o755)
    w = _tidy(str(pada))
    assert w.returncode == 1
    assert "padł" in w.stdout


import porownaj_bazy as pb  # noqa: E402


def test_zapadka_pokrycia_bez_tolerancji_zaokraglenia(tmp_path):
    # 2913/3123 = 93,276…% i 2914/3124 = 93,278…%: oba dają 93,28 po zaokrągleniu, a drugie jest wyżej
    raport = tmp_path / "g.json"
    raport.write_text(_gcov("firmware/prototyp/src/a.cpp", [(i, 1 if i <= 2913 else 0) for i in range(1, 3124)]),
                      encoding="utf-8")
    baza = tmp_path / "baza.txt"
    baza.write_text("2914 3124 93.28\n", encoding="utf-8")
    assert pk.main([str(raport), "--baza", str(baza)]) == 1


def test_bazy_wobec_main():
    T, P = pb.TIDY, pb.POKRYCIE
    main_ = {T: "# x\na | r | 1\nb | r | 2\n", P: "2914 3123 93.31\n"}
    # lepiej: mniej ostrzeżeń, wyższe pokrycie
    assert pb.sprawdz({T: "a | r | 1\n", P: "2920 3123 93.50\n"}, main_, [])[0] == []
    # gorzej bez wpisu: nowe ostrzeżenie i niższe pokrycie
    b, _ = pb.sprawdz({T: "a | r | 1\nb | r | 2\nc | r | 3\n", P: "2914 3124 93.28\n"}, main_, [])
    assert len(b) == 2
    # ta sama baza z inną kolejnością i komentarzem: bez zmian
    assert pb.sprawdz({T: "b | r | 2\na | r | 1\n", P: "2914 3123 93.31\n"}, main_, [])[0] == []
    # gorzej z uzasadnieniem w opisie commitu
    opis = ("zmiana bazy: tools/jakosc/clang-tidy-baza.txt — nowy adapter, ostrzeżenie zgłoszone w PR\n"
            "zmiana bazy: `pokrycie-baza.txt`: kod sprzętu bez atrapy\n")
    b, info = pb.sprawdz({T: "a | r | 1\nb | r | 2\nc | r | 3\n", P: "2914 3124 93.28\n"}, main_, [opis])
    assert b == [] and len(info) == 2
    # wpis bez powodu albo dla innego pliku nie wystarcza
    for zly in ("zmiana bazy: tools/jakosc/clang-tidy-baza.txt —\n", "zmiana bazy: pokrycie-baza.txt — powód długi\n"):
        assert len(pb.sprawdz({T: "a | r | 1\nb | r | 2\nc | r | 3\n", P: main_[P]}, main_, [zly])[0]) == 1, zly
    # bazy jeszcze nie ma na main: bez porównania; brak bazy w gałęzi: błąd
    assert pb.sprawdz({T: "z | r | 9\n", P: "1 2 50.00\n"}, {T: None, P: None}, [])[0] == []
    assert len(pb.sprawdz({T: None, P: main_[P]}, main_, [])[0]) == 1


def test_bazy_galaz_bazowa_i_wpis_z_changelog():
    # gałąź bazowa: argument, potem BAZA_GALAZ, potem origin/main
    assert pb.galaz_bazowa(None, {}) == "origin/main"
    assert pb.galaz_bazowa(None, {"BAZA_GALAZ": "origin/integracja"}) == "origin/integracja"
    assert pb.galaz_bazowa("origin/x", {"BAZA_GALAZ": "origin/integracja"}) == "origin/x"
    assert pb.galaz_bazowa(None, {"BAZA_GALAZ": ""}) == "origin/main"
    # tylko linie dopisane w diffie CHANGELOG liczą się jako wpis (stare wpisy nie zwalniają nowych pogorszeń)
    diff = ("--- a/CHANGELOG.md\n+++ b/CHANGELOG.md\n@@ -1,2 +1,3 @@\n # Historia zmian\n"
            "+- zmiana bazy: tools/jakosc/clang-tidy-baza.txt — performance-* bez wpływu na działanie\n"
            "-- zmiana bazy: pokrycie-baza.txt — stary wpis usunięty\n")
    tekst = pb.dopisane_linie(diff)
    assert "clang-tidy-baza.txt" in tekst and "pokrycie-baza" not in tekst and "+++" not in tekst
    assert pb.dopisane_linie(None) == ""
    T, P = pb.TIDY, pb.POKRYCIE
    baza = {T: "a | r | 1\n", P: "10 20 50.00\n"}
    b, info = pb.sprawdz({T: "a | r | 1\nc | r | 3\n", P: baza[P]}, baza, [tekst], "origin/integracja")
    assert b == [] and "origin/integracja" in info[0]
    b, _ = pb.sprawdz({T: baza[T], P: "9 20 45.00\n"}, baza, [tekst], "origin/integracja")
    assert len(b) == 1 and "origin/integracja" in b[0]
