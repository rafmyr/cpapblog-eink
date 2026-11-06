# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy przypadków brzegowych (symulator: scenariusze, prognoza, obrazki dnia). Każdy test sprawdza jedną
usterkę i blokuje jej powrót. Tylko zrzut syntetyczny
(tools/symulator/generuj_zrzut.py), bez prawdziwych danych zdrowotnych i bez obrazków użytkownika.
"""
import datetime as dt
import importlib.util
import os

import pytest

TU = os.path.dirname(os.path.abspath(__file__))


def modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa + "_r12", os.path.join(TU, "symulator", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


sym = modul("symuluj")
gen = modul("generuj_zrzut")
scen = sym.scen


@pytest.fixture(scope="module")
def zrzut(tmp_path_factory):
    k = str(tmp_path_factory.mktemp("zrzut_r12"))
    gen.generuj(k, wersja=0, nocy=60)
    return k


def jeden(zrzut, tmp_path, nazwy, **kw):
    return sym.symuluj(zrzut, wyjscie=str(tmp_path / "wy"), cicho=True, scenariusze=nazwy, **kw)


def statusy(k, nazwa):
    """{opis: 'jest'|'BRAK'} z wspólnego spis.txt --pokaz-wszystko dla scenariusza `nazwa`."""
    w, moj = {}, False
    with open(os.path.join(k, "spis.txt"), encoding="utf-8") as f:
        for l in f:
            if l.startswith("== "):
                moj = l.startswith(f"== scenariusz-{nazwa} ")
            elif moj and l.strip().startswith(("jest: ", "BRAK: ")):
                s, o = l.strip().split(": ", 1)
                w[o] = s
    return w


# `regula` sprawdza podłańcuch w zlepionej liście reguł, więc wiersz hipotezy „H-08/L-01” (H-08 przy
# L-01 po zmianie ustawień) daje „jest: L-01 spełnione”, choć L-01 nie jest spełnione.
def test_regula_nie_jest_podlancuchem(tmp_path):
    k = tmp_path / "wynik"
    k.mkdir()
    (k / "log-panelu.txt").write_text(
        "2026-11-30 05:00:05  SYMULATOR reguły spełnione (pamięć panelu): H-08/L-01 od 2026-11-30\n", encoding="utf-8")
    p = scen.Plan()
    p.oczekiwane = [("regula", "L-01", "L-01 spełnione")]
    assert sym.sprawdz_oczekiwane(str(k), p) == [("L-01 spełnione", False)], \
        "„H-08/L-01” zaliczone jako L-01 spełnione"


# status „jest” przy ekranach sprawdza tylko przedrostek nazwy obrazka, nie treść. Scenariusz z celowo
# zepsutą nocą (wyciek zdrowy) dalej pisze „jest: Otwarte sprawy z kartą E-23”; zmiana-trybu bez zmiany trybu dalej
# pisze „jest: K1 z wierszami trybu AutoSet”.
def test_karta_e23_bez_e23_to_brak(zrzut, tmp_path, monkeypatch):
    stary = scen.SCENARIUSZE["wyciek"]

    def zepsuty(p, s, w):
        stary[0](p, s, w)
        p.noce = [scen.noc(s, w)]  # zdrowa noc zamiast wycieku: E-23 nie ma prawa się pojawić
    monkeypatch.setitem(scen.SCENARIUSZE, "wyciek", (zepsuty,) + stary[1:])
    k, _ = sym.pokaz_wszystko(zrzut, str(tmp_path), cicho=True, nazwy=["wyciek"])
    st = statusy(k, "wyciek")
    assert any("E-23" in o and s == "BRAK" for o, s in st.items() if "ekran" in o), st  # pełny ekran S3: kontrola działa
    assert st.get("Otwarte sprawy z kartą E-23") == "BRAK", f"karta E-23 „jest” bez E-23: {st}"


def test_k1_autoset_bez_autoset_to_brak(zrzut, tmp_path, monkeypatch):
    monkeypatch.setattr(scen, "_autoset", lambda w: dict(w))  # tryb zostaje ze zrzutu (ASVAuto)
    k, _ = sym.pokaz_wszystko(zrzut, str(tmp_path), cicho=True, nazwy=["zmiana-trybu"])
    st = statusy(k, "zmiana-trybu")
    assert st.get("K1 z wierszami trybu AutoSet") == "BRAK", f"K1 AutoSet „jest” w trybie ASVAuto: {st}"


# wiersz hipotezy H-08 z warunkiem `zawsze` (tylko_raport) trafia do „reguły spełnione” jako „H-08/L-02”
# w KAŻDYM przebiegu, także w zwykly-dzien na prawdziwym zrzucie, choć L-02 (oddech okresowy) nie jest spełnione.
# Użytkownik czyta to jako wzorzec L-02 w swoich danych.
def test_hipoteza_bez_wzorca_nie_jest_spelniona(zrzut, tmp_path):
    k = jeden(zrzut, tmp_path, ["zwykly-dzien"])
    reg = sym.reguly_spelnione(k, wszystkie=True)
    pozycje = [x.split(" od ")[0].strip() for s in reg.split(" | ") for x in s.split(", ")]
    for x in pozycje:
        if x.startswith("H-") and "/L-" in x:
            wz = x.split("/", 1)[1].split("~")[0]
            assert any(y == wz or y.startswith(wz + "/") for y in pozycje), \
                f"„{x}” na liście spełnionych bez wzorca {wz}: {reg}"


# przebieg jednego scenariusza ląduje w ~/cpap-eink-symulator/<daty>/ z nazwami jak prawdziwa doba
# (np. …-dzien-wzorzec-L-03_ge10.png). Ani katalog, ani nazwy obrazków nie mówią, że to noce wymyślone; obrazek
# „pokaż lekarzowi” wyjęty z katalogu wygląda jak ocena danych użytkownika.
def test_wynik_scenariusza_oznaczony_w_nazwie(zrzut, tmp_path, monkeypatch):
    monkeypatch.setenv("HOME", str(tmp_path))
    k = sym.symuluj(zrzut, cicho=True, scenariusze=["wyciek"])
    pngi = [n for n in os.listdir(k) if n.endswith(".png")]
    oznaczone = "scenariusz" in os.path.basename(k) or "syntet" in os.path.basename(k)
    assert oznaczone or all("scen" in n or "syntet" in n for n in pngi), \
        f"brak oznaczenia nocy syntetycznych w {os.path.basename(k)} / {pngi[:2]}"


# nagłówek log.txt pisze „Ostatnia noc z AirBridge w zrzucie: rano 2026-11-26”, a to noc syntetyczna
# scenariusza (zrzut kończy się 04.11).
def test_naglowek_nie_przypisuje_zrzutowi_nocy_syntetycznej(zrzut, tmp_path):
    k = jeden(zrzut, tmp_path, ["wyciek"])
    with open(os.path.join(k, "log.txt"), encoding="utf-8") as f:
        l = [x for x in f if x.startswith("Ostatnia noc z AirBridge w zrzucie")]
    assert l and "2026-11-04" in l[0], f"{l}"


# przy kilku scenariuszach profil CO2 scenariusza `co2` działa od początku okresu, także w dniach
# wcześniejszego scenariusza (wyciek 26.11 dostaje CO2 1350 ppm), więc opis „o 21:00 drugiego dnia” przestaje się zgadzać.
def test_co2_tylko_w_dniach_swojego_scenariusza(zrzut, tmp_path):
    k = jeden(zrzut, tmp_path, ["wyciek", "co2"])
    with open(os.path.join(k, "log-panelu.txt"), encoding="utf-8") as f:
        zle = [x for x in f if x.startswith("2026-11-26") and "CO2 1350" in x]
    assert not zle, f"CO2 scenariusza co2 w dniu scenariusza wyciek: {zle[:1]}"


# noc syntetyczna ma chwilę importu 07:00, a dochodzi do pamięci o 04:30; o 05:15 panel widzi import
# „z przyszłości” i idzie ścieżką zastępnika po zaniku zasilania („chwila importu nieznana”), nie zwykłą.
def test_import_nocy_nie_pozniej_niz_jej_przyjscie():
    d = dt.date(2026, 11, 26)
    w = {"tryb": "ASVAuto", "ustawienia": "ASVAuto/6/10/3/15", "pna": "x", "srn": ""}
    pole = scen.noc(d, w).split(";")[scen.POLA.index("import")]
    przyjscie = int(dt.datetime(2026, 11, 26, 4, 30, tzinfo=dt.timezone(dt.timedelta(hours=1))).timestamp())
    assert int(pole) <= przyjscie, "import nocy syntetycznej później niż jej przyjście (04:30)"


# Trzyma (regresja na przyszłość): trzy formy --obrazki, wynik poza repo, brak Pillow → czytelny komunikat.
def test_trzyma_brak_pillow_czytelny_blad(tmp_path, monkeypatch):
    zr = tmp_path / "obr"
    zr.mkdir()
    (zr / "a.png").write_bytes(b"\x89PNG\r\n\x1a\n")
    import builtins
    imp = builtins.__import__

    def bez_pil(n, *a, **k):
        if n == "przygotuj" or n.startswith("PIL"):
            raise ImportError(n)
        return imp(n, *a, **k)
    monkeypatch.setattr(builtins, "__import__", bez_pil)
    with pytest.raises(sym.Blad, match="Pillow"):
        sym.obrazki_panelu(str(zr), str(tmp_path / "t"), cicho=True)
