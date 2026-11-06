# SPDX-License-Identifier: GPL-3.0-or-later
"""Symulator doby, scenariusze z nocami syntetycznymi, atrapa prognozy Open-Meteo, obrazki
dnia, --pokaz-wszystko. Wszystko na zrzucie syntetycznym (tools/symulator/generuj_zrzut.py) i obrazkach syntetycznych;
bez prawdziwych danych zdrowotnych i bez obrazków użytkownika.

Każdy scenariusz ma dać ekrany i linie, które zapowiada (Plan.oczekiwane w tools/symulator/scenariusze.py), po nazwie
z wiersza `EKRAN …` w log-panelu.txt, linii uwagi z logu dyrygenta albo reguły z pamięci panelu (/dane/reguly.csv).
"""
import datetime as dt
import hashlib
import importlib.util
import json
import os
import re

import pytest

TU = os.path.dirname(os.path.abspath(__file__))


def modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa + "_b12", os.path.join(TU, "symulator", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


sym = modul("symuluj")
gen = modul("generuj_zrzut")
scen = sym.scen


def png(k):
    return sorted(x for x in os.listdir(k) if x.endswith(".png"))


def w_spisie(spis, pliki):
    """Obrazki opisane w spis.txt: wprost albo w zwiniętym wierszu `pierwszy … ostatni — opis (N obrazków …)`."""
    jest = {p for p in pliki if p in spis}
    for a, b in re.findall(r"(\S+\.png) … (\S+\.png) — ", spis):
        jest |= {p for p in pliki if a <= p <= b}
    return jest


def ekrany(k):
    """Nazwy ekranów z wierszy `EKRAN …` log-panelu.txt."""
    return [n for _, n, _, _ in sym.klatki(k)]


def obrazki_syntetyczne(k, n=3, w=400, h=300, z_manifestem=True):
    os.makedirs(k, exist_ok=True)
    for i in range(n):
        wiersze = bytearray()
        for y in range(h):
            for x in range(0, w, 8):
                wiersze.append(0xFF if ((x // 40 + y // 40 + i) % 2) else 0x00)
        with open(os.path.join(k, f"00{i}_szachownica.pbm"), "wb") as f:
            f.write(f"P4\n{w} {h}\n".encode() + bytes(wiersze))
    if z_manifestem:
        with open(os.path.join(k, "obrazki.json"), "w", encoding="utf-8") as f:
            json.dump({"wersja": 1, "obrazki": [{"plik": f"00{i}_szachownica.pbm"} for i in range(n)]}, f)
    return k


def skrot(k):
    h = hashlib.sha256()
    for p, _, pliki in sorted(os.walk(k)):
        for n in sorted(pliki):
            h.update(n.encode())
            with open(os.path.join(p, n), "rb") as f:
                h.update(f.read())
    return h.hexdigest()


@pytest.fixture(scope="module")
def zrzut(tmp_path_factory):
    k = str(tmp_path_factory.mktemp("zrzut_b12"))
    gen.generuj(k, wersja=0, nocy=60)
    return k


@pytest.fixture(scope="module")
def obrazki(tmp_path_factory):
    return obrazki_syntetyczne(str(tmp_path_factory.mktemp("obrazki_b12")))


@pytest.fixture(scope="module")
def przebiegi(zrzut, obrazki, tmp_path_factory):
    """Każdy scenariusz raz (moduł): nazwa → katalog wyniku."""
    przed = skrot(zrzut)
    w = {n: sym.symuluj(zrzut, wyjscie=str(tmp_path_factory.mktemp("s-" + n)), cicho=True, obrazki=obrazki,
                        scenariusze=[n]) for n in scen.KOLEJNOSC}
    assert skrot(zrzut) == przed, "zrzut na dysku zmieniony przez scenariusz"
    return w


@pytest.mark.parametrize("nazwa", scen.KOLEJNOSC)
def test_scenariusz_daje_zapowiedziane_ekrany(przebiegi, zrzut, nazwa):
    k = przebiegi[nazwa]
    wynik = sym.sprawdz_oczekiwane(k, scen.plan(zrzut, [nazwa]))
    assert wynik and all(j for _, j in wynik), (nazwa, [o for o, j in wynik if not j], ekrany(k))
    assert len(png(k)) == len(ekrany(k)) > 0
    with open(os.path.join(k, "spis.txt"), encoding="utf-8") as f:
        spis = f.read()
    assert set(png(k)) <= w_spisie(spis, png(k))  # spis.txt: każdy obrazek z opisem (powtórzenia zwinięte)


@pytest.mark.parametrize("nazwa", [n for n in scen.KOLEJNOSC if n != "zwykly-dzien"])
def test_noce_scenariusza_oznaczone_jako_syntetyczne(przebiegi, zrzut, nazwa):
    with open(os.path.join(przebiegi[nazwa], "log.txt"), encoding="utf-8") as f:
        log = f.read()
    plan = scen.plan(zrzut, [nazwa])
    assert "Noce syntetyczne (wymyślone przez scenariusz, nie z aparatu)" in log
    assert f"Scenariusz {nazwa} od poranka" in log and "Rozbieg:" in log
    # noce po --od dochodzą o 04:30 swojego poranka, z oznaczeniem w wierszu wybudzenia
    if any(r[:10] > f"{plan.od:%Y-%m-%d}" for r in plan.noce):
        assert "[doszła noc syntetyczna z poranka " in log


def test_okres_domyslny_scenariusza(przebiegi, zrzut):
    for n in ("wyciek", "ahi-rosnie", "brak-danych"):
        plan = scen.plan(zrzut, [n])
        pierwszy = png(przebiegi[n])[0]  # SYN-001-RRRRMMDD-HHMM-…: pierwszy obrazek tego poranka, od 05:00
        assert pierwszy[8:16] == f"{plan.od:%Y%m%d}" and pierwszy[17:19] == "05", (n, pierwszy)


def test_noce_scenariusza_maja_pelne_metryki_i_ustawienia_ze_zrzutu(zrzut):
    plan = scen.plan(zrzut, ["centralne"])
    w = scen.wzor(zrzut)
    for r in plan.noce:
        p = r.split(";")
        assert len(p) == len(scen.POLA) == 41
        d = dict(zip(scen.POLA, p))
        for k in ("oa", "ca", "h", "m08", "m13", "m17", "tryb"):
            assert d[k] != "", (k, r)
        assert d["zrodlo"] == "A" and d["ustawienia"] == w["ustawienia"] and d["tryb"] == "ASVAuto"
    pierwsza = dt.date.fromisoformat(plan.noce[0][:10])
    assert pierwsza == w["data"] + dt.timedelta(days=1)  # od dnia po ostatniej nocy zrzutu
    dni = [dt.date.fromisoformat(r[:10]) for r in plan.noce]
    assert dni == sorted(set(dni)) and (dni[-1] - dni[0]).days == len(dni) - 1  # bez dziur i powtórzeń


def test_zmiana_trybu_noce_autoset(zrzut):
    d = [dict(zip(scen.POLA, r.split(";"))) for r in scen.plan(zrzut, ["zmiana-trybu"]).noce]
    assert d[-1]["tryb"] == "AutoSet" and d[-1]["psmed"] == "" and d[-1]["mpi"] != ""
    assert d[0]["tryb"] == "ASVAuto"  # rozbieg w trybie ze zrzutu


def test_kilka_scenariuszy_po_kolei(zrzut, tmp_path):
    plan = scen.plan(zrzut, ["wyciek", "co2"])
    assert plan.od == scen.plan(zrzut, ["wyciek"]).od and plan.do > plan.od + dt.timedelta(days=1)
    k = sym.symuluj(zrzut, wyjscie=str(tmp_path / "w"), cicho=True, scenariusze=["wyciek", "co2"])
    wynik = sym.sprawdz_oczekiwane(k, plan)
    assert all(j for _, j in wynik), wynik


def test_bledy_scenariuszy_po_polsku(zrzut, tmp_path):
    with pytest.raises(sym.Blad, match="nieznany scenariusz: xyz"):
        sym.symuluj(zrzut, cicho=True, scenariusze=["xyz"], wyjscie=str(tmp_path / "a"))
    with pytest.raises(sym.Blad, match="zwykly-dzien działa tylko sam"):
        sym.symuluj(zrzut, cicho=True, scenariusze=["wyciek", "zwykly-dzien"], wyjscie=str(tmp_path / "b"))
    with pytest.raises(sym.Blad, match="brak-danych działa tylko sam"):
        sym.symuluj(zrzut, cicho=True, scenariusze=["brak-danych", "co2"], wyjscie=str(tmp_path / "c"))
    with pytest.raises(sym.Blad, match="podaj --od i --do"):
        sym.symuluj(zrzut, cicho=True, wyjscie=str(tmp_path / "d"))


def test_inne_od_niz_scenariusz_daje_ostrzezenie(zrzut, tmp_path, capsys):
    sym.symuluj(zrzut, "2026-11-26T09:00", None, wyjscie=str(tmp_path / "w"), scenariusze=["wyciek"])
    assert "UWAGA: scenariusz pokazuje swój ekran od 2026-11-26T05:00" in capsys.readouterr().out


# ---------- prognoza w atrapie ----------
def test_slot_dzienny_z_atrapa_prognozy(zrzut, tmp_path):
    k = sym.symuluj(zrzut, "2026-11-04T05:00", "2026-11-04T23:59", wyjscie=str(tmp_path / "z"), cicho=True)
    e = ekrany(k)
    assert any(n.startswith("dzien-SK33") for n in e) and any(n.startswith("dzien-SK3") and n != "dzien-SK33" for n in e), e
    with open(os.path.join(k, "log.txt"), encoding="utf-8") as f:
        assert "Współrzędne miasta: 50.2597, 19.0211" in f.read()
    with open(os.path.join(k, "log-panelu.txt"), encoding="utf-8") as f:
        assert "prognoza godzinowa: pobrana" in f.read()
    b = sym.symuluj(zrzut, "2026-11-04T05:00", "2026-11-04T23:59", wyjscie=str(tmp_path / "b"), cicho=True,
                    bez_prognozy=True)
    # bez prognozy żadnego ekranu slotu dziennego S-K-33…S-K-39; poranny S-K-10 `Wyciek widoczny` (R-KONF-42, od
    # 06.11.2026 od P95 10 L/min) nie zależy od prognozy
    assert not any(n.startswith("dzien-SK3") for n in ekrany(b)), ekrany(b)


def test_prognoza_z_wlasnego_pliku_i_bledy(zrzut, tmp_path):
    # sama odpowiedź /v1/forecast (bez PM2.5): daty dowolne, symulator podstawia dzień wybudzenia
    f = tmp_path / "prognoza.json"
    f.write_text(json.dumps({"hourly": {"time": [f"2020-01-01T{g % 24:02d}:00" for g in range(48)],
                                        "temperature_2m": [5.0] * 48, "precipitation_probability": [10] * 48,
                                        "cloud_cover": [50] * 48, "wind_gusts_10m": [20.0] * 48}}), encoding="utf-8")
    k = sym.symuluj(zrzut, "2026-11-04T05:00", "2026-11-04T12:00", wyjscie=str(tmp_path / "w"), cicho=True,
                    prognoza=str(f), miasto="Gliwice")
    assert any(n.startswith("dzien-SK") for n in ekrany(k))
    zly = tmp_path / "zly.json"
    zly.write_text("{nie json", encoding="utf-8")
    with pytest.raises(sym.Blad, match="nie jest poprawnym JSON-em"):
        sym.symuluj(zrzut, "2026-11-04T05:00", "2026-11-04T06:00", wyjscie=str(tmp_path / "x"), cicho=True,
                    prognoza=str(zly))
    krotki = tmp_path / "krotki.json"
    krotki.write_text(json.dumps({"hourly": {"temperature_2m": [1.0] * 10}}), encoding="utf-8")
    with pytest.raises(sym.Blad, match="brak 48 godzin"):
        sym.symuluj(zrzut, "2026-11-04T05:00", "2026-11-04T06:00", wyjscie=str(tmp_path / "y"), cicho=True,
                    prognoza=str(krotki))


def test_wspolrzedne_miasta():
    assert sym.wspolrzedne_miasta("Katowice") == (50.2597, 19.0211)
    assert sym.wspolrzedne_miasta("kraków") == sym.wspolrzedne_miasta("Krakow")
    assert sym.wspolrzedne_miasta(wspolrzedne="50.1, 19.2") == (50.1, 19.2)
    with pytest.raises(sym.Blad, match="nie znam współrzędnych miasta „Pcim”"):
        sym.wspolrzedne_miasta("Pcim")
    with pytest.raises(sym.Blad, match="--wspolrzedne"):
        sym.wspolrzedne_miasta(wspolrzedne="pięćdziesiąt")


def test_domyslny_plik_prognozy_w_ksztalcie_open_meteo(tmp_path):
    with open(sym.PROGNOZA_DOMYSLNA, encoding="utf-8") as f:
        j = json.load(f)
    assert len(j["forecast"]["hourly"]["time"]) == 48 and len(j["air_quality"]["hourly"]["pm2_5"]) == 48
    w = open(sym.profil_prognozy(sym.PROGNOZA_DOMYSLNA, str(tmp_path / "p.txt")), encoding="utf-8").read().splitlines()
    assert len(w) == 48 and all(len(x.split(";")) == 10 for x in w)


# ---------- obrazki dnia ----------
def test_obrazek_dnia_z_katalogu_pbm(przebiegi):
    e = ekrany(przebiegi["zwykly-dzien"])
    assert "dzien-SK38" in e, e
    with open(os.path.join(przebiegi["zwykly-dzien"], "log.txt"), encoding="utf-8") as f:
        assert "Obrazki dnia w pamięci panelu (/obrazki/): 3 obrazki." in f.read()


def test_obrazki_z_podkatalogu_panel_i_ze_zrodla(zrzut, tmp_path):
    # układ assets/obrazki-dzienne-prywatne/: gotowe pliki panelu w panel/
    obrazki_syntetyczne(str(tmp_path / "prywatne" / "panel"))
    assert sym.obrazki_panelu(str(tmp_path / "prywatne"), str(tmp_path)) == str(tmp_path / "prywatne" / "panel")
    os.makedirs(tmp_path / "pusty")
    with pytest.raises(sym.Blad, match="nie ma obrazków"):
        sym.obrazki_panelu(str(tmp_path / "pusty"), str(tmp_path))
    with pytest.raises(sym.Blad, match="nie ma katalogu obrazków"):
        sym.obrazki_panelu(str(tmp_path / "brak"), str(tmp_path))
    # katalog źródłowy (PNG) → przygotuj.py do katalogu tymczasowego
    PIL = pytest.importorskip("PIL.Image")
    src = tmp_path / "zrodlo"
    src.mkdir()
    for i in range(2):
        PIL.new("L", (1200, 900), 255 if i else 0).save(src / f"00{i}_x.png")
    cel = sym.obrazki_panelu(str(src), str(tmp_path / "tmp"))
    assert sorted(os.listdir(cel)) == ["000_x.pbm", "001_x.pbm", "obrazki.json"]
    k = sym.symuluj(zrzut, "2026-11-04T05:00", "2026-11-04T23:59", wyjscie=str(tmp_path / "w"), cicho=True,
                    obrazki=str(src))
    assert "dzien-SK38" in ekrany(k)


# ---------- przegląd wszystkiego ----------
def test_pokaz_wszystko(zrzut, obrazki, tmp_path):
    k, braki = sym.pokaz_wszystko(zrzut, str(tmp_path), obrazki=obrazki, cicho=True)
    assert braki == []
    assert sorted(os.listdir(k)) == sorted(["spis.txt"] + [f"scenariusz-{n}-{scen.plan(zrzut, [n]).od:%Y-%m-%d}"
                                                            for n in scen.KOLEJNOSC])
    with open(os.path.join(k, "spis.txt"), encoding="utf-8") as f:
        spis = f.read()
    for n in scen.KOLEJNOSC:
        assert f"== scenariusz-{n} (" in spis
        kat = os.path.join(k, f"scenariusz-{n}-{scen.plan(zrzut, [n]).od:%Y-%m-%d}")
        assert set(png(kat)) <= w_spisie(spis, png(kat))  # powtórzenia zwinięte
        assert os.path.isfile(os.path.join(kat, sym.UWAGA_SYN)) == (n != "zwykly-dzien")
    assert "obrazek dnia z --obrazki wczytany" in spis
    assert "BRAK:" not in spis
    # drugi przegląd nie miesza się z pierwszym
    k2, _ = sym.pokaz_wszystko(zrzut, str(tmp_path), obrazki=obrazki, cicho=True, nazwy=["wyciek"])
    assert k2 == k + "-2"


def test_pokaz_wszystko_odmawia_wyniku_w_repo(zrzut):
    with pytest.raises(sym.Blad, match="repozytorium"):
        sym.pokaz_wszystko(zrzut, os.path.join(sym.REPO, "x"), cicho=True, nazwy=["wyciek"])


def test_nazwy_ekranow_bety(przebiegi):
    """Nazwy nowych ekranów (test/symulator.cpp nazwaEkranu()) z wierszy dyrygenta: wzorzec, pytanie, slot, wieczór."""
    wszystkie = {n for k in przebiegi.values() for n in ekrany(k)}
    for oczekiwana in ("dzien-wzorzec-L-03_ge10", "dzien-wzorzec-L-11_2tyg", "dzien-pytanie-sennosc", "dzien-SK33",
                       "dzien-SK38", "dzien-wieczor-SK01", "dzien-S5", "dzien-S10", "sprawy", "zapisano", "K1", "K2",
                       "dzien-zdarzenie-E-23", "dzien-zdarzenie-E-30", "dzien-zdarzenie-E-33",
                       "dzien-zdarzenie-E-10_problem"):  # pełne ekrany zdarzeń E (S3)
        assert oczekiwana in wszystkie, (oczekiwana, sorted(wszystkie))
    assert "dzien-nieznany" not in wszystkie


# ---------- przypadki dodatkowe ----------
def test_oznaczenie_syntetycznych_przedrostek_i_znak_wodny(przebiegi, zrzut):
    from PIL import Image
    syn = przebiegi["wyciek"]
    assert all(p.startswith("SYN-") for p in png(syn)) and os.path.isfile(os.path.join(syn, sym.UWAGA_SYN))
    assert os.path.basename(syn).startswith("scenariusz-wyciek-") or "s-wyciek" in syn
    zw = przebiegi["zwykly-dzien"]
    assert not any(p.startswith("SYN-") for p in png(zw)) and not os.path.exists(os.path.join(zw, sym.UWAGA_SYN))
    # znak wodny: czarne pole w prawym górnym rogu każdego obrazka z nocami syntetycznymi (także K1 i K2)
    for p in png(syn):
        im = Image.open(os.path.join(syn, p)).convert("L")
        assert im.getpixel((795, 3)) == 0 and im.getpixel((795, 20)) == 0, p


def test_domyslny_katalog_scenariusza_z_nazwa(zrzut, tmp_path, monkeypatch):
    monkeypatch.setenv("HOME", str(tmp_path))
    k = sym.symuluj(zrzut, cicho=True, scenariusze=["wyciek"])
    assert os.path.basename(k).startswith("scenariusz-wyciek-2026-11-26_0500--")


def test_naglowek_log_rozdziela_zrzut_i_noce_syntetyczne(przebiegi):
    with open(os.path.join(przebiegi["wyciek"], "log.txt"), encoding="utf-8") as f:
        log = f.read()
    assert "Ostatnia noc z AirBridge w zrzucie: rano 2026-11-04 (prawdziwa, ze zrzutu)." in log
    assert "Noce syntetyczne (wymyślone przez scenariusz, nie z aparatu): 22 noce, poranki 2026-11-05 – 2026-11-26" in log


def test_reguly_spelnione_bez_hipotez_bez_wzorca_i_bez_wierszy_pomocniczych(przebiegi):
    for n, k in przebiegi.items():
        for x in sym.reguly_spelnione(k, wszystkie=True).split(" | "):
            poz = sym._pozycje_regul(x)
            assert not any(y in poz for y in ("E-33/mediana", "E-23/bez_sk10_kilka")), (n, x)
            for y in poz:
                if y.startswith("H-"):
                    assert sym.regula_w(poz, y.split("/", 1)[1].split("~")[0]), (n, x)


def test_co2_tylko_w_dniach_scenariusza_co2(zrzut, tmp_path):
    k = sym.symuluj(zrzut, wyjscie=str(tmp_path / "w"), cicho=True, scenariusze=["wyciek", "co2"])
    with open(os.path.join(k, "log-panelu.txt"), encoding="utf-8") as f:
        t = f.read()
    assert "CO2 650" in t and "CO2 1250" in t


def test_prognoza_musi_zaczynac_sie_o_polnocy_czasu_polskiego(zrzut, tmp_path):
    f = tmp_path / "utc.json"
    f.write_text(json.dumps({"timezone": "GMT", "hourly": {"time": [f"2020-01-01T{g % 24:02d}:00" for g in range(48)],
                                                           "temperature_2m": [1.0] * 48}}), encoding="utf-8")
    with pytest.raises(sym.Blad, match="timezone=Europe/Warsaw"):
        sym.profil_prognozy(str(f), str(tmp_path / "p.txt"))
    f.write_text(json.dumps({"hourly": {"time": [f"2020-01-01T{(g + 3) % 24:02d}:00" for g in range(48)],
                                        "temperature_2m": [1.0] * 48}}), encoding="utf-8")
    with pytest.raises(sym.Blad, match="od północy"):
        sym.profil_prognozy(str(f), str(tmp_path / "p.txt"))


def test_pokaz_wszystko_idzie_dalej_po_bledzie(zrzut, tmp_path, monkeypatch):
    stary = scen.SCENARIUSZE["wyciek"]

    def zly(p, s, w):
        raise scen.BladScenariusza("celowy błąd testu")
    monkeypatch.setitem(scen.SCENARIUSZE, "wyciek", (zly,) + stary[1:])
    k, braki = sym.pokaz_wszystko(zrzut, str(tmp_path), cicho=True, nazwy=["wyciek", "co2"])
    assert braki == ["wyciek: błąd: celowy błąd testu"]
    with open(os.path.join(k, "spis.txt"), encoding="utf-8") as f:
        spis = f.read()
    assert "BŁĄD: celowy błąd testu" in spis and "jest: wieczorem" in spis


def test_noc_syntetyczna_importowana_o_0430(zrzut):
    w = scen.wzor(zrzut)
    d = dt.date(2026, 11, 26)
    imp = int(scen.noc(d, w).split(";")[scen.POLA.index("import")])
    assert imp == scen._import_ep(d) and dt.datetime.fromtimestamp(imp, dt.timezone(dt.timedelta(hours=1))).hour == 4


def test_opcja_profil_publiczny(zrzut, tmp_path):
    # --profil-publiczny = `profil=publiczny` w /konfiguracja.txt panelu (5.6.4), bez zmiany kodu panelu;
    # bez opcji profil prywatny (egzemplarz użytkownika)
    k = sym.symuluj(zrzut, wyjscie=str(tmp_path / "pub"), cicho=True, scenariusze=["zwykly-dzien"],
                    profil_publiczny=True)
    assert sym._w_logu_panelu(k, "konfiguracja: profil publiczny")
    k = sym.symuluj(zrzut, wyjscie=str(tmp_path / "pryw"), cicho=True, scenariusze=["zwykly-dzien"])
    assert not sym._w_logu_panelu(k, "konfiguracja: profil publiczny")


def test_odkrycia_bez_znaleziska_r_ekr_51(zrzut):
    # wpis „NIE DA SIĘ WYWOŁAĆ” dla R-EKR-51 usunięty; ekran pokazuje scenariusz profil-publiczny
    assert not [z for z in scen.plan(zrzut, ["odkrycia"]).znaleziska if "R-EKR-51" in z]
    assert "profil=publiczny" in scen.plan(zrzut, ["profil-publiczny"]).konfiguracja


def test_pokaz_wszystko_z_profilem_publicznym(zrzut, tmp_path):
    # --profil-publiczny z --pokaz-wszystko włącza profil w każdym scenariuszu
    nazwy = ["zwykly-dzien", "wyciek"]
    k, _ = sym.pokaz_wszystko(zrzut, str(tmp_path), cicho=True, nazwy=nazwy, profil_publiczny=True)
    kat = [os.path.join(k, d) for d in os.listdir(k) if d.startswith("scenariusz-")]
    assert len(kat) == len(nazwy)
    assert all(sym._w_logu_panelu(x, "konfiguracja: profil publiczny") for x in kat)
    with open(os.path.join(k, "spis.txt"), encoding="utf-8") as f:
        assert "Profil publiczny we wszystkich scenariuszach" in f.read()


def test_przerywniki_maja_karte_z_warunkiem_w_dniu_okazji(przebiegi, zrzut):
    """Zapowiedź karty z warunkiem nie zależy od losowania sceny: dzień okazji ze stałą datą wymusza scenę."""
    plan = scen.plan(zrzut, ["przerywniki"])
    okazja, scena = scen._najblizsza_okazja(plan.od.date() + dt.timedelta(days=27))
    wynik = dict(sym.sprawdz_oczekiwane(przebiegi["przerywniki"], plan))
    assert any(scena in o and j for o, j in wynik.items()), (scena, wynik)
    assert okazja >= plan.od.date() + dt.timedelta(days=27)
