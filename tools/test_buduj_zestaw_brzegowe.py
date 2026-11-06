# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy przypadków brzegowych (skrypt tools/zestaw/buduj_zestaw.py, granica ADR-0002 w 9.3).

Testy usterek padają do poprawki; testy `test_trzyma_*` przechodzą. Uruchomienie: python3 -m pytest -q tools/test_buduj_zestaw_brzegowe.py
"""

import os
import subprocess
import sys

TU = os.path.dirname(os.path.abspath(__file__))
SKRYPT = os.path.join(TU, "zestaw", "buduj_zestaw.py")
sys.path.insert(0, TU)
from test_buduj_zestaw import E30, POLA, PROGI, SPEC, TEKSTY, przygotuj  # noqa: E402

# Spec testowa jak prawdziwa: zakazane przykłady z ADR-0002 stoją w niej dosłownie (spec funkcjonalna je cytuje,
# np. „Obniż EPR o 1”, „Ustaw minimum na 7 cmH2O”: grep w docs/spec/specyfikacja-funkcjonalna.md).
SPEC_Z_ZAKAZANYMI = SPEC + """
Granica ADR-0002: zakazane `Obniż EPR o 1`, `Ustaw minimum na 7 cmH2O`.
"""


def uruchom(argv):
    r = subprocess.run([sys.executable, SKRYPT] + argv, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def z_tekstem(tekst, id_t="E-30.krok"):
    return TEKSTY.replace("E-30.krok;krok;;→ Sprawdź wyciek i obserwuj kolejne noce.", "%s;krok;;%s" % (id_t, tekst))


def test_zakazany_tekst_cytowany_w_spec_przechodzi(tmp_path):
    # „tekst dosłownie w spec” nie jest granicą ADR-0002, bo spec cytuje zakazane przykłady; słowa
    # granicy dają tylko ostrzeżenie. Spec techniczna 9.3 / tabela mechanizmów: tekst z wartością albo drogą zmiany
    # ustawień „zatrzymuje skrypt budowy”.
    for zakazany in ("Obniż EPR o 1", "Ustaw minimum na 7 cmH2O"):
        kod, out = uruchom(przygotuj(tmp_path, teksty=z_tekstem(zakazany), spec=SPEC_Z_ZAKAZANYMI))
        assert kod == 1, "`%s` przeszło do zestawu:\n%s" % (zakazany, out)


def test_tekst_spoza_katalogu_bez_kontroli(tmp_path):
    # tekst w teksty.csv, którego nie wskazuje żaden wiersz katalogu, nie przechodzi żadnej kontroli
    # (spec, liczby, limit), a trafia do zestawu; firmware pobiera teksty po id (tekst(), teksty U-01…U-05, T-WYM-64).
    teksty = TEKSTY + "U-01.krok;krok;;Zmień ciśnienie na 9 cmH2O w menu Komfort.\n"
    kod, out = uruchom(przygotuj(tmp_path, teksty=teksty))
    wyj = (tmp_path / "wyj" / "teksty.csv").read_text(encoding="utf-8") if kod == 0 else ""
    assert kod == 1 and "9 cmH2O" not in wyj, "tekst spoza spec w zestawie:\n%s" % out


def test_zmienna_pasuje_do_calej_spec(tmp_path):
    # `{zmienna}` zamienia się na `.*?` bez granic, więc tekst sklejony z kawałków spec odległych
    # o całe sekcje przechodzi jako „dosłownie w spec”.
    spec = SPEC + "\nDużo dalej, w opisie zakazów: zmień ciśnienie na 9 cmH2O.\n"
    tekst = "→ Sprawdź wyciek {x} zmień ciśnienie na 9 cmH2O."
    kod, out = uruchom(przygotuj(tmp_path, teksty=z_tekstem(tekst), spec=spec))
    assert kod == 1, "tekst sklejony przez {x} przeszedł:\n%s" % out


def test_bom_w_progi_bez_komunikatu(tmp_path):
    # progi.csv z BOM (zapis z Excela) kończy się wyjątkiem Pythona (KeyError), nie komunikatem BŁĄD.
    argv = przygotuj(tmp_path)
    (tmp_path / "progi.csv").write_text("\ufeff" + PROGI, encoding="utf-8")
    kod, out = uruchom(argv)
    assert kod == 1 and "BŁĄD zestawu reguł" in out and "Traceback" not in out, out


def test_nieznana_zmienna_w_tekscie(tmp_path):
    # zmienna, której rodzaj nie podaje, przechodzi; na ekranie stanie `–` (tekst() w reguly.cpp).
    tekst = "→ Sprawdź wyciek i obserwuj kolejne noce."
    spec = SPEC.replace(tekst, "→ Sprawdź wyciek {zmienna_bez_rodzaju} i obserwuj kolejne noce.")
    kod, out = uruchom(przygotuj(tmp_path, teksty=z_tekstem("→ Sprawdź wyciek {nieznana} i obserwuj kolejne noce."),
                                 spec=spec))
    assert kod == 1, "{nieznana} przeszła:\n%s" % out


def test_trzyma_liczba_progu_bez_prog(tmp_path):
    teksty = TEKSTY.replace("Noce z AHI powyżej {prog:E-30.ahi}", "Noce z AHI powyżej 5")
    kod, out = uruchom(przygotuj(tmp_path, teksty=teksty))
    assert kod == 1 and "{prog:" in out


def test_trzyma_tekst_spoza_spec_uzyty_w_katalogu(tmp_path):
    kod, out = uruchom(przygotuj(tmp_path, teksty=z_tekstem("Zmień ciśnienie na 9 cmH2O.")))
    assert kod == 1 and "nie stoi dosłownie" in out


def test_trzyma_cykl_i_kolejnosc(tmp_path):
    a = "A-1;;zawsze;;;;problem;R5b;rano;ASV;nie;tak;;A-2;;;;aktywna;;t"
    b = "A-2;;zawsze;;;;problem;R5b;rano;ASV;nie;tak;;A-1;;;;aktywna;;t"
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=(E30, a, b)))
    assert kod == 1 and "cykl" in out
    c = "A-3;;zlozenie;op=i,skladniki=A-4+E-30;;;problem;R5b;rano;ASV;nie;tak;;;;;;aktywna;;t"
    d = "A-4;;zawsze;;;;problem;R5b;rano;ASV;nie;tak;;;;;;aktywna;;t"
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=(c, E30, d)))
    assert kod == 0, out
    kat = (tmp_path / "wyj" / "katalog.csv").read_text(encoding="utf-8").split("\n")[2:]
    ids = [x.split(";")[0] for x in kat if x]
    assert ids.index("A-4") < ids.index("A-3") and ids.index("E-30") < ids.index("A-3")


def test_trzyma_deterministycznie(tmp_path):
    argv = przygotuj(tmp_path)
    assert uruchom(argv)[0] == 0
    h1 = (tmp_path / "zestaw.h").read_bytes()
    assert uruchom(argv)[0] == 0
    assert (tmp_path / "zestaw.h").read_bytes() == h1


def test_trzyma_pole_z_srednikiem_w_katalogu(tmp_path):
    zly = E30.replace("E-30.tytul,E-30.powod", "E-30.tytul;E-30.powod")
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=(zly,)))
    assert kod == 1, out


def test_trzyma_parametr_liczbowy_w_katalogu(tmp_path):
    zly = E30.replace("prog=E-30.ahi", "prog=5")
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=(zly,)))
    assert kod == 1, out


assert POLA  # import użyty w testach pomocniczych
