# SPDX-License-Identifier: GPL-3.0-or-later
"""Przypadki brzegowe „Dobry start” wersja 2 (R-DS-39, R-DS-60…R-DS-62, R-DS-71): wstrzyknięcia w kopii katalogu
w katalogu tymczasowym mają zatrzymać budowę zestawu błędem z ID porady. Test oznaczony `przypadek n` to przypadek
dawnej usterki narzędzia. Dekodowanie kodów
QR z zrzutów ekranu (firmware/prototyp/test/testy_dobry_start_v2_brzegowe.cpp, katalog BRZEG2_QR_ZRZUT) własnym
dekoderem zxing-cpp: pominięte bez biblioteki albo bez zrzutów."""

import csv
import glob
import io
import os
import shutil
import sys

import pytest

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TU)
sys.path.insert(0, os.path.join(TU, "zestaw"))
import porady  # noqa: E402

KATALOG = os.path.join(REPO, "docs", "spec", "katalog")


def _wczytaj():
    with open(os.path.join(KATALOG, "porady.csv"), encoding="utf-8", newline="") as f:
        r = csv.DictReader(io.StringIO(f.read()), delimiter=";")
        return r.fieldnames, list(r)


def _zbuduj(tmp_path, zmiana):
    pola, wiersze = _wczytaj()
    wiersze = zmiana(wiersze) or wiersze
    d = tmp_path / "katalog"
    d.mkdir()
    shutil.copy(os.path.join(KATALOG, "porady-ankieta.csv"), d / "porady-ankieta.csv")
    with open(d / "porady.csv", "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=pola, delimiter=";", lineterminator="\n")
        w.writeheader()
        w.writerows(wiersze)
    return porady.buduj_porady(str(d))


def _por(w, i):
    return next(p for p in w if p["id"] == i)


def test_krok_przy_kodzie_qr_o_1_px_za_szeroki(tmp_path):
    kroje = porady.wczytaj_kroje()
    t = "→ Krok"
    while kroje["P24B"].szer(porady.twarde(t + "l")) <= porady.KOLUMNA_QR:
        t += "l"
    (tmp_path / "a").mkdir()
    (tmp_path / "b").mkdir()
    _zbuduj(tmp_path / "a", lambda w: _por(w, "DS-S07").update(krok=t))
    with pytest.raises(porady.BladPorad, match="DS-S07: krok"):
        _zbuduj(tmp_path / "b", lambda w: _por(w, "DS-S07").update(krok=t + "l"))


def test_tresc_przy_kodzie_qr_szosta_linia(tmp_path):
    kroje = porady.wczytaj_kroje()
    _, w = _wczytaj()
    t = _por(w, "DS-S07")["tresc"]
    while len(porady.zawin(kroje["P24S"], porady.twarde(t), porady.KOLUMNA_QR)) <= 5:
        t += " słowo"
    with pytest.raises(porady.BladPorad, match=r"DS-S07: treść 6 linii > 5"):
        _zbuduj(tmp_path, lambda x: _por(x, "DS-S07").update(tresc=t))


@pytest.mark.parametrize("zmiana,wzor", [
    (lambda w: _por(w, "DS-P01").update(tresc="Ta noc: {czasy} z maską."), "DS-P01"),
    (lambda w: _por(w, "DS-S07").update(qr="https://cpapblog.pl/?p=" + "1" * 20), "DS-S07: adres QR"),
    (lambda w: _por(w, "DS-D07").update(qr="https://cpapblog.pl/?p=1"), "DS-D07: kod QR na karcie"),
    (lambda w: _por(w, "DS-P05").update(tresc=_por(w, "DS-P05")["tresc"] + " AHI też."), "DS-P05: AHI"),
    (lambda w: [x for x in w if x["id"] != "DS-S08.asv"], "start_wieczor doba 10 w trybie ASV"),
    (lambda w: _por(w, "DS-P12").update(warunek=""), "DS-P12: pochwała bez flagi `pochwala` albo bez warunku"),
    (lambda w: _por(w, "DS-D28").update(fakty="{noce_z_maska}::AHI pod kontrolą"), "DS-D28: AHI"),
])
def test_wstrzykniecia_koncza_budowe_bledem(tmp_path, zmiana, wzor):
    with pytest.raises(porady.BladPorad, match=wzor):
        _zbuduj(tmp_path, zmiana)


def test_qr_42_bajty_przechodzi(tmp_path):
    a = "https://cpapblog.pl/?p=" + "1" * (42 - len("https://cpapblog.pl/?p="))
    assert len(a) == 42
    _zbuduj(tmp_path, lambda w: _por(w, "DS-S07").update(qr=a))


# przypadek 5: znacznik danych spoza wzorca `{[a-z_0-9]+}` (wielka litera, spacja) przechodzi budowę, a panel
# (podstawZnaczniki) zostawia go na ekranie jako tekst `{Czas}`
@pytest.mark.parametrize("zly", ["{Czas}", "{czas }", "{o-ile-dluzej}"])
def test_przypadek5_znacznik_spoza_wzorca(tmp_path, zly):
    with pytest.raises(porady.BladPorad):
        _zbuduj(tmp_path, lambda w: _por(w, "DS-P01").update(tresc="Ta noc: %s z maską." % zly))


# przypadek 6: token warunku pochwały z literówką (pochwała nigdy nie stanie) przechodzi budowę
def test_przypadek6_nieznany_token_pochwaly(tmp_path):
    with pytest.raises(porady.BladPorad):
        _zbuduj(tmp_path, lambda w: _por(w, "DS-P01").update(warunek="pochwala/pierwsza_nco|!ekran/problem"))


def test_kody_qr_dekoduja_sie_wlasnym_dekoderem():
    zxing = pytest.importorskip("zxingcpp")
    Image = pytest.importorskip("PIL.Image")
    kat = os.environ.get("BRZEG2_QR_ZRZUT", "/tmp/brzeg2-qr")
    pliki = sorted(glob.glob(os.path.join(kat, "*.pbm")))
    if not pliki:
        pytest.skip("brak zrzutów PBM (BRZEG2_QR_ZRZUT)")
    _, w = _wczytaj()
    qr = {p["id"]: p["qr"] for p in w if p["qr"]}
    wynik = {}
    for f in pliki:
        im = Image.open(f).convert("L")
        wynik[os.path.basename(f)[:-4]] = [b.text for b in zxing.read_barcodes(im)]
    assert set(wynik) == set(qr) and len(qr) == 27
    assert all(wynik[i] == [qr[i]] for i in qr)
