# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy podziału programów testów rdzenia na części CI (tools/jakosc/podzial.py)."""
import os
import subprocess
import sys

import pytest

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TU)
sys.path.insert(0, os.path.join(TU, "jakosc"))
import podzial  # noqa: E402


def lista_testy():
    fw = os.path.join(REPO, "firmware", "prototyp")
    r = subprocess.run(["bash", "-c", 'source tools/prototyp/zrodla.sh; printf "%s\\n" "${TESTY[@]}"'],
                       cwd=REPO, env=dict(os.environ, FW=fw, REPO=REPO), capture_output=True, text=True, check=True)
    return r.stdout.split()


@pytest.mark.parametrize("krok", podzial.KROKI)
@pytest.mark.parametrize("n", range(1, 9))
def test_kazdy_program_testy_dokladnie_raz_w_sumie_czesci(krok, n):
    testy = lista_testy()
    assert len(testy) > 100
    czasy = podzial.wczytaj_czasy()[krok]
    czesci, sumy = podzial.podziel(testy, czasy, n)
    assert len(czesci) == n and all(czesci)
    assert sorted(p for c in czesci for p in c) == sorted(testy)
    assert podzial.sprawdz(testy, czesci) == []
    for c in czesci:  # w części kolejność jak w TESTY
        assert c == [p for p in testy if p in c]


def test_cli_czesci_skladaja_sie_na_testy():
    testy = lista_testy()
    wynik = []
    for k in range(1, 5):
        r = subprocess.run([sys.executable, os.path.join(TU, "jakosc", "podzial.py"), "czesc", "--krok", "sanitizery",
                            "--czesc", "%d/4" % k, "--"] + testy, capture_output=True, text=True, check=True)
        wynik += r.stdout.split()
    assert sorted(wynik) == sorted(testy)


def test_najdluzszy_sam_w_czesci_i_rowne_reszty():
    czasy = {"a": 100, "b": 30, "c": 30, "d": 30, "e": 30, "f": 30, "g": 30}
    czesci, sumy = podzial.podziel(list(czasy), czasy, 3)
    assert ["a"] in czesci
    assert sorted(sumy) == [90, 90, 100]


def test_program_bez_czasu_dostaje_mediane():
    czasy = {"a": 10, "b": 20, "c": 30}
    czesci, sumy = podzial.podziel(["a", "b", "c", "nowy"], czasy, 1)
    assert sumy == [80]


def test_podzial_deterministyczny():
    czasy = {"x%d" % i: 5 for i in range(20)}
    assert podzial.podziel(list(czasy), czasy, 3) == podzial.podziel(list(czasy), czasy, 3)


def test_sprawdz_wykrywa_brak_powtorzenie_i_obcy():
    bledy = podzial.sprawdz(["/a/testy_a.cpp", "/a/testy_b.cpp", "/a/testy_c.cpp"],
                            [["testy_a", "testy_b"], ["testy_b", "testy_obcy"]])
    assert "testy_b wykonany 2 razy" in bledy
    assert "testy_c wykonany 0 razy" in bledy
    assert "testy_obcy spoza listy TESTY" in bledy


def test_sprawdz_cli(tmp_path):
    (tmp_path / "lista").write_text("/x/testy_a.cpp\n/x/testy_b.cpp\n")
    (tmp_path / "c1").write_text("testy_a\n")
    (tmp_path / "c2").write_text("testy_b\n")
    pd = os.path.join(TU, "jakosc", "podzial.py")
    ok = subprocess.run([sys.executable, pd, "sprawdz", "--lista", str(tmp_path / "lista"), str(tmp_path / "c1"),
                         str(tmp_path / "c2")], capture_output=True, text=True)
    assert ok.returncode == 0, ok.stdout
    zle = subprocess.run([sys.executable, pd, "sprawdz", "--lista", str(tmp_path / "lista"), str(tmp_path / "c1")],
                         capture_output=True, text=True)
    assert zle.returncode == 1 and "testy_b wykonany 0 razy" in zle.stdout


def test_zla_czesc():
    for zla in ("0/4", "5/4", "1", "a/b", "1/0"):
        with pytest.raises(ValueError):
            podzial.czesc(zla)


def test_czasy_z_logu_i_zapis(tmp_path):
    log = ("2026-11-23T10:00:00.1Z czas: sanitizery testy_a 12 s\n"
           "czas: pokrycie testy_a 3 s\nczas: sanitizery testy_b 1062 s\n")
    assert podzial.z_logu([log], "sanitizery") == {"testy_a": 12.0, "testy_b": 1062.0}
    assert podzial.z_logu([log, "czas: sanitizery testy_a 20 s\nczas: sanitizery testy_b 598 s"],
                          "sanitizery") == {"testy_a": 20.0, "testy_b": 1062.0}  # najdłuższy z przebiegów
    plik = tmp_path / "czasy.txt"
    czasy = {"sanitizery": {"testy_a": 12.0}, "pokrycie": {"testy_a": 3.0}}
    podzial.zapisz_czasy(czasy, str(plik))
    assert podzial.wczytaj_czasy(str(plik)) == czasy


def test_plik_czasow_w_repo_czytelny_i_bez_programow_spoza_testy():
    czasy = podzial.wczytaj_czasy()
    nazwy = {podzial.nazwa(p) for p in lista_testy()}
    for krok in podzial.KROKI:
        assert czasy[krok], krok
        # program usunięty z TESTY: wiersz do usunięcia z czasy-testow.txt (inaczej podział liczy nieistniejący czas)
        assert set(czasy[krok]) <= nazwy, sorted(set(czasy[krok]) - nazwy)


def test_sprawdz_sh_odrzuca_zla_czesc():
    r = subprocess.run(["bash", os.path.join(TU, "jakosc", "sprawdz.sh"), "sanitizery", "--czesc", "x"],
                       capture_output=True, text=True)
    assert r.returncode == 2 and "k/n" in r.stdout
