# SPDX-License-Identifier: GPL-3.0-or-later
"""Przypadki brzegowe konfiguracji w zrzucie panelu (T-WYM-92, spec 8.6, 8.9). Wszystkie testy blokują.
Dane syntetyczne, bez danych zdrowotnych."""
import hashlib
import importlib.util
import os
import sys
import types

import pytest

TU = os.path.dirname(os.path.abspath(__file__))


def modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa + "_brzegowe", os.path.join(TU, "prototyp", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def panel_zrzut(konf, noce=b"2026-11-03;1;420\n", skrot=None, wiersz_skrotu=True):
    """Wyjście panelu na `Z` jak src/dyrygent.cpp zrzutPlikow()+zrzutKonfiguracji() (Serial.println: `\\r\\n`)."""
    w = [b"PLIKI_START", b"PLIK /noce.csv"] + [x for x in noce.split(b"\n") if x] + [b"PLIK_KONIEC"]
    if konf is not None:
        if wiersz_skrotu:
            s = skrot or hashlib.sha256(konf).hexdigest()
            w.append(b"konfiguracja w zrzucie: %d B, sha256 %s" % (len(konf), s.encode()))
        w.append(b"PLIK /konfiguracja.txt")
        p = 0
        while p < len(konf):
            e = konf.find(b"\n", p)
            e = len(konf) if e < 0 else e
            w.append(konf[p:e])
            p = e + 1
        w.append(b"PLIK_KONIEC")
    w.append(b"PLIKI_KONIEC")
    return [x + b"\r\n" for x in w]


class Port:
    def __init__(self, wiersze, urwij_po=None):
        self.wiersze, self.wyslane, self.wyjscie = wiersze, b"", []
        if urwij_po is not None:
            self.wiersze = wiersze[:urwij_po]

    def write(self, b):
        self.wyslane += b
        if b == b"Z" and not self.wyjscie:
            self.wyjscie, self.wiersze = self.wiersze, []
        return len(b)

    def readline(self):
        return self.wyjscie.pop(0) if self.wyjscie else b""


def zrzut(kat, wiersze, tylko=False, limit=1.0):
    z = modul("zrzut_plikow")
    logi = []
    w = z.czytaj_zrzut(Port(wiersze), limit, log=logi.append, co_s=0.01)
    if w is None:
        return z, None, logi
    return z, z.zapisz_zrzut(str(kat), w[0], w[1], tylko, log=logi.append), logi


DUZY = b"".join(b"k%d=w\r\n" % i for i in range(3000))[: 16 * 1024 - 1] + b"y"
ZAWARTOSCI = [
    b"a=1\r\nb=2\r\n", b"a=1\r\nb=2", b"\r\n\r\n", b"\n", b"\n\n\n", b"", b"x\r",
    "maska.model=AirFit F20 żółć\n\tk=\tw\t\n".encode(),
    b"PLIK /noce.csv\nkonfiguracja w zrzucie: 1 B, sha256 " + b"a" * 64 + b"\n",
    DUZY, DUZY + b"z" * 5000 + b"\n",
]


@pytest.mark.parametrize("konf", ZAWARTOSCI)
def test_wiernosc_bajt_w_bajt(tmp_path, konf):
    _, stan, _ = zrzut(tmp_path, panel_zrzut(konf))
    assert stan == "zgodny"
    assert (tmp_path / "konfiguracja.txt").read_bytes() == konf


def test_pelny_obieg_zmiana_jednej_linii(tmp_path):
    """zrzut → zmiana jednej linii → --konfiguracja (bajty komendy W) → nowy zrzut: różni się tylko ta linia."""
    w = modul("wgraj_pliki")
    konf = "profil=prywatny\r\n\n# ż\nwymiana.maska=2026-10-01\nco2=tak".encode()
    os.makedirs(tmp_path / "a")
    zrzut(tmp_path / "a", panel_zrzut(konf), tylko=True)
    p = tmp_path / "a" / "konfiguracja.txt"
    nowa = p.read_bytes().replace(b"wymiana.maska=2026-10-01", b"wymiana.maska=2026-11-09")
    p.write_bytes(nowa)

    class PortW:
        def __init__(self):
            self.wyslane = b""

        def write(self, b):
            self.wyslane += b

        def readline(self):
            return b"OK konfiguracja.txt\r\n"

    pw = PortW()
    assert w.wgraj_konfiguracje(pw, str(p)) == {"konfiguracja.txt": "OK"}
    naglowek, tresc = pw.wyslane.split(b"\n", 1)
    assert tresc == nowa and naglowek.split()[3].decode() == hashlib.sha256(nowa).hexdigest()
    os.makedirs(tmp_path / "b")
    zrzut(tmp_path / "b", panel_zrzut(tresc), tylko=True)
    a, b = konf.split(b"\n"), (tmp_path / "b" / "konfiguracja.txt").read_bytes().split(b"\n")
    assert len(a) == len(b) and [i for i in range(len(a)) if a[i] != b[i]] == [3]


def test_tylko_konfiguracja_wysyla_tylko_z_i_nic_innego(tmp_path, monkeypatch):
    z = modul("zrzut_plikow")
    port = Port(panel_zrzut(b"a=1\n"))
    monkeypatch.setitem(sys.modules, "serial", types.SimpleNamespace(Serial=lambda *a, **k: port))
    with pytest.raises(SystemExit) as e:
        z.main(["/dev/x", str(tmp_path), "--tylko-konfiguracja", "--czas", "1"])
    assert e.value.code is None
    assert set(port.wyslane) == {ord("Z")}
    assert sorted(os.listdir(tmp_path)) == ["konfiguracja.txt"]


def test_przerwane_w_polowie_sekcji_nie_rusza_dobrego_pliku(tmp_path):
    (tmp_path / "konfiguracja.txt").write_bytes(b"dobry=1\n")
    w = panel_zrzut(b"a=1\nb=2\nc=3\n")
    _, stan, _ = zrzut(tmp_path, w[: w.index(b"b=2\r\n")], tylko=True, limit=0.3)
    assert stan is None
    assert (tmp_path / "konfiguracja.txt").read_bytes() == b"dobry=1\n"


def test_niezgodny_skrot_nie_gubi_starszej_kopii(tmp_path):
    (tmp_path / "konfiguracja.txt").write_bytes(b"edycja-wlasciciela=1\n")
    (tmp_path / "konfiguracja.txt.poprzedni").write_bytes(b"starsza=1\n")
    _, stan, _ = zrzut(tmp_path, panel_zrzut(b"a=1\n", skrot="0" * 64), tylko=True)
    assert stan == "niezgodny"
    zostalo = {p.read_bytes() for p in tmp_path.iterdir()}
    assert b"edycja-wlasciciela=1\n" in zostalo and b"starsza=1\n" in zostalo


def test_stary_niezgodny_blokuje_przejscie(tmp_path):
    prz = modul("przejscie_32mb")
    za, zb = tmp_path / "zrzut-a", tmp_path / "zrzut-b"
    za.mkdir(), zb.mkdir()
    zrzut(za, panel_zrzut(b"a=1\n", skrot="0" * 64))  # pierwsza próba: błąd przesyłu
    zrzut(za, panel_zrzut(b"a=1\n"))  # ponowna próba: dobra
    zrzut(zb, panel_zrzut(b"a=1\n"))
    assert prz.sprawdz_zrzuty(str(za), str(zb)) == []


def test_wiersz_znacznika_w_tresci_nie_psuje_innych_plikow(tmp_path):
    noce = b"2026-11-03;1;420\n"
    konf = b"a=1\nPLIK_KONIEC\nPLIK /noce.csv\nnie-noc\n"
    _, stan, logi = zrzut(tmp_path, panel_zrzut(konf, noce=noce))
    # po poprawce sekcja czytana według rozmiaru z wiersza skrótu: treść wierna, zrzut nienaruszony
    assert stan == "zgodny" and (tmp_path / "konfiguracja.txt").read_bytes() == konf
    assert (tmp_path / "noce.csv").read_bytes() == noce


def test_bez_skrotu_to_blad_nie_sukces(tmp_path):
    z, stan, _ = zrzut(tmp_path, panel_zrzut(b"a=1", wiersz_skrotu=False), tylko=True)
    assert z.podsumowanie(str(tmp_path), stan, True, "/dev/x") is not None


def test_przywracanie_kolejnosc_noce_konfiguracja_k(tmp_path):
    w = modul("wgraj_pliki")
    (tmp_path / "noce.csv").write_bytes(b"2026-11-03;1;420\n")
    (tmp_path / "konfiguracja.txt").write_bytes(b"a=1")

    class P:
        def __init__(self, odp):
            self.wyslane, self.odp = [], odp

        def write(self, b):
            self.wyslane.append(b[:30])

        def readline(self):
            return self.odp.pop(0) if self.odp else b""

    p = P([b"OK noce.csv\n", b"BLAD konfiguracja.txt skrot\n", b"BLAD konfiguracja.txt skrot\n"])
    wynik = w.wgraj(p, str(tmp_path))
    kol = [x[:3] for x in p.wyslane]
    assert kol == [b"W n", b"W k", b"W k", b"K"]  # odrzucona konfiguracja: K i tak, po dwóch próbach
    assert wynik["konfiguracja.txt"] == "skrot"


def test_pliki_start_w_tresci_zeruje_zrzut_po_cichu(tmp_path):
    z, stan, _ = zrzut(tmp_path, panel_zrzut(b"a=1\nPLIKI_START\nb=2\n"))
    assert (tmp_path / "noce.csv").exists() and stan != "brak"
    assert (tmp_path / "konfiguracja.txt").read_bytes() == b"a=1\nPLIKI_START\nb=2\n"
    assert z.podsumowanie(str(tmp_path), stan, False, "/dev/x") is not None or stan == "zgodny"
