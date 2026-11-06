# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy: skrypt przejścia egzemplarza w trybie --symulacja, brak odpowiedzi panelu
w wgraj_pliki.py i identyfikator układu partycji w skrypcie budowy (ta sama liczba co test C++
testT_WYM_38_identyfikatorUkladu, więc skrypt budowy i rdzeń liczą tak samo).
"""
import importlib.util
import os

import pytest

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TU)


def modul(sciezka, nazwa):
    spec = importlib.util.spec_from_file_location(nazwa, sciezka)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def przejscie():
    return modul(os.path.join(TU, "prototyp", "przejscie_32mb.py"), "przejscie_32mb")


def zrzut(katalog, nocy=5):
    os.makedirs(katalog, exist_ok=True)
    tresci = {
        "noce.csv": "".join("2026-11-%02d;1;420;1.4\n" % (d + 1) for d in range(nocy)).encode(),
        "czynniki.csv": b"2026-11-01;O;K;K;-1;1\n",
        "humor.csv": b"2026-11-01;S2A;1;D;;\n",
        "sypialnia.csv": b"2026-11-01;20.0;45.0\n",
        "sypialnia-co2.csv": b"2026-11-01;800\n",
        "bateria.csv": b"1793746800;3950;1200;5000;zegar:czujniki\n",
    }
    for n, t in tresci.items():
        with open(os.path.join(katalog, n), "wb") as f:
            f.write(t)


def test_symulacja_przejscia_zgodne(tmp_path, capsys):
    p = przejscie()
    zrzut(str(tmp_path / "zrzut-a"), 7)
    zrzut(str(tmp_path / "zrzut-b"), 7)
    assert p.main(["port", str(tmp_path), "--symulacja"]) == 0
    out = capsys.readouterr().out
    assert "przywrócono 7 z 7" in out
    assert "pio run -e e1001 -t upload" in out


def test_symulacja_rozne_zrzuty_bez_wgrywania(tmp_path, capsys):
    p = przejscie()
    zrzut(str(tmp_path / "zrzut-a"), 7)
    zrzut(str(tmp_path / "zrzut-b"), 6)
    assert p.main(["port", str(tmp_path), "--symulacja"]) == 1
    out = capsys.readouterr().out
    assert "noce.csv: różne skróty" in out
    assert "krok 3" not in out


def test_katalog_w_repo_odrzucony(tmp_path):
    p = przejscie()
    assert p.main(["port", os.path.join(REPO, "tools"), "--symulacja"]) == 2


def test_liczba_nocy_bez_pustych(tmp_path):
    p = przejscie()
    zrzut(str(tmp_path), 4)
    with open(tmp_path / "noce.csv", "ab") as f:
        f.write(b"\n\n")
    assert p.liczba_nocy(str(tmp_path)) == 4


def test_wgraj_brak_odpowiedzi(tmp_path, monkeypatch):
    w = modul(os.path.join(TU, "prototyp", "wgraj_pliki.py"), "wgraj_pliki_b")
    zrzut(str(tmp_path))

    class Cichy:
        wyslane = b""

        def write(self, b):
            self.wyslane += b

        def readline(self):
            return b""

    czas = [0.0]

    def monotonic():
        czas[0] += 10.0
        return czas[0]

    monkeypatch.setattr(w.time, "monotonic", monotonic)
    wynik = w.wgraj(Cichy(), str(tmp_path))
    assert set(wynik.values()) == {"brak odpowiedzi"}


POLA_NOCE_V1 = ("data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;"
                "epap_str;oa;ca;h;csr;import")


def zrzut_po_przejsciu(katalog, nocy=30):
    """Zrzut `Z` firmware: bez pustych sekcji plików prototypu): pliki wersji 1 pod nowymi ścieżkami."""
    os.makedirs(os.path.join(katalog, "dane", "noce"), exist_ok=True)
    os.makedirs(os.path.join(katalog, "dane", "sypialnia"), exist_ok=True)
    os.makedirs(os.path.join(katalog, "log"), exist_ok=True)
    w = ["2026-10-%02d;1;420;1.40;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;;179370000%d" % (d + 1, d % 10)
         for d in range(nocy)]
    pliki = {
        "dane/noce/2026.csv": "#cpap-eink noce 1\n%s\n%s\n#koniec %d\n" % (POLA_NOCE_V1, "\n".join(w), nocy),
        "dane/czynniki.csv": "#cpap-eink czynniki 1\ndata;stan;kody;warstwa;termin;pokazano\n2026-10-02;O;K;K;-1;1\n"
                             "#koniec 1\n",
        "dane/sypialnia/2026.csv": "#cpap-eink sypialnia 1\ndata;temp;wilg;co2\n2026-10-02;20.0;45.0;812\n"
                                   "2026-10-03;20.5;44.0;\n#koniec 2\n",
        "dane/humor.csv": "#cpap-eink humor 1\ndata;typ;klucz;tryb;scena;zdanie\n2026-10-02;S2A;1;D;;\n",
        "log/wybudzenia.csv": "#cpap-eink wybudzenia 1\nepoch;mV;wifi_ms;aktywny_ms;powod\n"
                              "1793746800;3950;1200;5000;zegar:czujniki\n",
    }
    for n, t in pliki.items():
        with open(os.path.join(katalog, n), "w") as f:
            f.write(t)


def test_zrzut_po_p03_przepisany_na_pliki_prototypu(tmp_path, capsys):
    """U10 Testy przypadków brzegowych: zrzut z nowymi ścieżkami → pliki prototypu w do-wgrania/ (noce 20 pól, bez `import`),
    przejście z liczbą nocy z plików rocznych."""
    p = przejscie()
    for k in ("zrzut-a", "zrzut-b"):
        zrzut_po_przejsciu(str(tmp_path / k), 30)
    assert p.main(["port", str(tmp_path), "--symulacja"]) == 0
    assert "przywrócono 30 z 30" in capsys.readouterr().out
    noce = (tmp_path / "do-wgrania" / "noce.csv").read_text().splitlines()
    assert len(noce) == 30 and all(len(w.split(";")) == 20 for w in noce)
    assert (tmp_path / "do-wgrania" / "sypialnia.csv").read_text() == "2026-10-02;20.0;45.0;812\n2026-10-03;20.5;44.0;\n"
    assert (tmp_path / "do-wgrania" / "bateria.csv").read_text() == "1793746800;3950;1200;5000;zegar:czujniki\n"
    assert not (tmp_path / "do-wgrania" / "sypialnia-co2.csv").exists()


def test_zrzut_uszkodzony_albo_bez_nocy(tmp_path, capsys):
    """U10: plik roczny bez poprawnego `#koniec` albo zrzut bez żadnej nocy → błąd, bez wgrywania (nigdy sukces 0 z 0)."""
    p = przejscie()
    for k in ("zrzut-a", "zrzut-b"):
        zrzut_po_przejsciu(str(tmp_path / k), 5)
        sc = tmp_path / k / "dane" / "noce" / "2026.csv"
        sc.write_text(sc.read_text().replace("#koniec 5", "#koniec 4"))
    assert p.main(["port", str(tmp_path), "--symulacja"]) == 1
    assert "nieczytelny" in capsys.readouterr().out
    q = tmp_path / "pusty"
    for k in ("zrzut-a", "zrzut-b"):
        zrzut(str(q / k), 0)
    assert p.main(["port", str(q), "--symulacja"]) == 1
    out = capsys.readouterr().out
    assert "bez nocy" in out and "przywrócono" not in out


def test_wgraj_ponawia_i_bez_K(tmp_path):
    """U2: po `BLAD … skrot` plik idzie jeszcze raz; przyjęty za drugim razem → komplet i `K`; odrzucony dwa razy →
    bez `K`."""
    w = modul(os.path.join(TU, "prototyp", "wgraj_pliki.py"), "wgraj_pliki_s6")
    zrzut(str(tmp_path))

    class Port:
        def __init__(self, zle_razy):
            self.zle = dict(zle_razy)
            self.wyslane = b""
            self.odp = []

        def write(self, b):
            self.wyslane += b
            if b.startswith(b"W "):
                n = b.split(b"\n", 1)[0].decode().split()[1]
                if self.zle.get(n, 0) > 0:
                    self.zle[n] -= 1
                    self.odp.append(("BLAD %s skrot\n" % n).encode())
                else:
                    self.odp.append(("OK %s\n" % n).encode())

        def readline(self):
            return self.odp.pop(0) if self.odp else b""

    raz = Port({"czynniki.csv": 1})
    assert set(w.wgraj(raz, str(tmp_path)).values()) == {"OK"}
    assert raz.wyslane.endswith(b"K") and raz.wyslane.count(b"W czynniki.csv") == 2
    dwa = Port({"czynniki.csv": 2})
    assert w.wgraj(dwa, str(tmp_path))["czynniki.csv"] == "skrot"
    assert not dwa.wyslane.endswith(b"K")


def test_czekaj_na_okno_pyta(monkeypatch):
    """U3: skrypt pyta `?` i rozpoznaje odpowiedź w oknie, odpowiedź poza oknem i milczenie panelu."""
    w = modul(os.path.join(TU, "prototyp", "wgraj_pliki.py"), "wgraj_pliki_s6b")
    czas = [0.0]

    def monotonic():
        czas[0] += 1.0
        return czas[0]

    monkeypatch.setattr(w.time, "monotonic", monotonic)

    class Port:
        def __init__(self, odp):
            self.odp, self.wyslane = odp, b""

        def write(self, b):
            self.wyslane += b

        def readline(self):
            return self.odp.pop(0) if self.odp and self.wyslane else b""

    p = Port([b"okno przywracania: koniec czasu\n", b"okno przywracania: 7 min (T-WYM-87): port serwisowy\n"])
    assert w.czekaj_na_okno(p, 30) == "otwarte" and p.wyslane.startswith(b"?")
    assert w.czekaj_na_okno(Port([b"przywracanie: poza oknem\n"]), 30) == "poza oknem"
    assert w.czekaj_na_okno(Port([]), 10) is None


def test_identyfikator_ukladu_jak_w_rdzeniu():
    u = modul(os.path.join(REPO, "firmware", "prototyp", "uklad_partycji.py"), "uklad_partycji")
    assert u.identyfikator_pliku(os.path.join(REPO, "firmware", "partycje-e1001.csv")) == 0x6FCC622E
    assert u.identyfikator_pliku(os.path.join(REPO, "firmware", "partycje-e1001-16mb.csv")) != 0x6FCC622E
    with pytest.raises(ValueError):
        u.tablica_z_csv("nvs, data, nvs, , 0x5000\n")


def test_pusty_plik_nocy_i_brak_pliku_nocy_bez_K(tmp_path):
    """pusty plik nocy nie jest wysyłany (wynik `pusty`), katalog bez pliku nocy daje wynik
    `brak pliku`; w obu przypadkach bez `K` (panel bez pliku nocy nie ma kompletu)."""
    w = modul(os.path.join(TU, "prototyp", "wgraj_pliki.py"), "wgraj_pliki_r2")

    class Port:
        def __init__(self):
            self.wyslane, self.odp = b"", []

        def write(self, b):
            self.wyslane += b
            if b.startswith(b"W "):
                self.odp.append(("OK %s\n" % b.split(b"\n", 1)[0].decode().split()[1]).encode())

        def readline(self):
            return self.odp.pop(0) if self.odp else b""

    zrzut(str(tmp_path / "a"), nocy=0)
    p = Port()
    wynik = w.wgraj(p, str(tmp_path / "a"))
    assert wynik["noce.csv"] == "pusty" and b"W noce.csv" not in p.wyslane and not p.wyslane.endswith(b"K")
    zrzut(str(tmp_path / "b"))
    os.remove(str(tmp_path / "b" / "noce.csv"))
    p = Port()
    wynik = w.wgraj(p, str(tmp_path / "b"))
    assert wynik["noce.csv"] == "brak pliku" and not p.wyslane.endswith(b"K")
    os.makedirs(str(tmp_path / "c"))
    assert w.wgraj(Port(), str(tmp_path / "c")) == {}  # katalog bez plików zrzutu: bez wyniku `brak pliku`


def test_okno_bez_przywracania_i_przywrocenie_okna(monkeypatch):
    """odpowiedź `poza oknem, bez przywracania` rozpoznana osobno; przywroc_okno wysyła pustą
    komendę `W noce.csv 0 …` do odpowiedzi `BLAD noce.csv poza oknem`."""
    w = modul(os.path.join(TU, "prototyp", "wgraj_pliki.py"), "wgraj_pliki_r2b")
    czas = [0.0]

    def monotonic():
        czas[0] += 1.0
        return czas[0]

    monkeypatch.setattr(w.time, "monotonic", monotonic)

    class Port:
        def __init__(self, odp):
            self.odp, self.wyslane = odp, b""

        def write(self, b):
            self.wyslane += b

        def readline(self):
            return self.odp.pop(0) if self.odp and self.wyslane else b""

    linia = b"przywracanie: poza oknem, bez przywracania (komenda W otworzy okno przy nast\xc4\x99pnym starcie)\n"
    assert w.czekaj_na_okno(Port([linia]), 30) == "bez przywracania"
    p = Port([b"cpap-eink start\n", b"BLAD noce.csv poza oknem\n"])
    assert w.przywroc_okno(p, 30)
    pusty = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    assert p.wyslane.startswith(("W noce.csv 0 %s\n" % pusty).encode())
    assert not w.przywroc_okno(Port([]), 10)
