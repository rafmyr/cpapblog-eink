# SPDX-License-Identifier: GPL-3.0-or-later
"""Konfiguracja panelu w zrzucie `Z` i przy przywracaniu (T-TEST-392; spec techniczna 8.6, 8.9, T-WYM-92).

tools/prototyp/zrzut_plikow.py składa `/konfiguracja.txt` z sekcji zrzutu bajt w bajt i sprawdza ją skrótem z wiersza
`konfiguracja w zrzucie: {n} B, sha256 {skrót}`; `--tylko-konfiguracja` zapisuje tylko ją i wysyła do panelu wyłącznie
`Z`. tools/prototyp/wgraj_pliki.py <katalog> wysyła `konfiguracja.txt` ze zrzutu przed `K`, a stary zrzut bez niej
przechodzi jak dotąd. Dane syntetyczne, bez danych zdrowotnych.
"""
import hashlib
import importlib.util
import os
import sys
import types

import pytest

TU = os.path.dirname(os.path.abspath(__file__))
PLIKI = ["noce.csv", "czynniki.csv", "humor.csv", "sypialnia.csv", "sypialnia-co2.csv", "bateria.csv"]
KONF = b"profil=prywatny\n\n# komentarz z \xc5\xbc\nwymiana.maska=2026-10-01\nPLIKI=1\nnoc_zrodlo=porownanie"


def modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa, os.path.join(TU, "prototyp", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def wiersze_zrzutu(konf=KONF, skrot=None, noce=b"2026-11-03;1;420\n"):
    """Wyjście panelu na `Z` jak src/dyrygent.cpp zrzutPlikow(): Serial.println, więc każdy wiersz z `\\r\\n`."""
    w = [b"ekran 0 narysowany w 10 ms", b"PLIKI_START", b"PLIK /noce.csv"]
    w += [x for x in noce.split(b"\n") if x] + [b"PLIK_KONIEC"]
    if konf is not None:
        if skrot is None:
            skrot = hashlib.sha256(konf).hexdigest()
        w.append(b"konfiguracja w zrzucie: %d B, sha256 %s" % (len(konf), skrot.encode()))
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


class PortZrzutu:
    """Panel w czuwaniu: na pierwsze `Z` wypisuje zrzut; zapisuje wszystko, co dostał."""

    def __init__(self, wiersze):
        self.wiersze, self.wyslane, self.wyjscie = wiersze, b"", []

    def write(self, b):
        self.wyslane += b
        if b == b"Z" and not self.wyjscie:
            self.wyjscie, self.wiersze = self.wiersze, []
        return len(b)

    def readline(self):
        return self.wyjscie.pop(0) if self.wyjscie else b""


def zrzut_do(tmp_path, port, tylko=False):
    z = modul("zrzut_plikow")
    logi = []
    wynik = z.czytaj_zrzut(port, 2, log=logi.append, co_s=0.01)
    assert wynik is not None
    stan = z.zapisz_zrzut(str(tmp_path), wynik[0], wynik[1], tylko, log=logi.append)
    return z, stan, logi


@pytest.mark.parametrize("konf", [KONF, KONF + b"\n", b"", b"\n\n", b"a=1\n\n"])
def test_zrzut_zapisuje_konfiguracje_bajt_w_bajt(tmp_path, konf):
    port = PortZrzutu(wiersze_zrzutu(konf))
    z, stan, logi = zrzut_do(tmp_path, port)
    assert stan == "zgodny"
    assert (tmp_path / "konfiguracja.txt").read_bytes() == konf
    assert (tmp_path / "noce.csv").read_bytes() == b"2026-11-03;1;420\n"
    assert set(port.wyslane) == {ord("Z")}  # do panelu tylko znak zrzutu
    # treść konfiguracji nie trafia do logu narzędzia (najwyżej rozmiar i skrót)
    assert not any("wymiana.maska" in l for l in logi)


def test_zrzut_bez_konfiguracji_na_panelu(tmp_path):
    port = PortZrzutu(wiersze_zrzutu(None))
    z, stan, _ = zrzut_do(tmp_path, port)
    assert stan == "brak"
    assert not (tmp_path / "konfiguracja.txt").exists()
    assert (tmp_path / "noce.csv").exists()
    assert z.podsumowanie(str(tmp_path), stan, False, "port") is None  # zwykły zrzut bez błędu


def test_zrzut_niezgodny_skrot_nie_daje_pliku_do_przywracania(tmp_path):
    (tmp_path / "konfiguracja.txt").write_bytes(b"stara=1\n")
    port = PortZrzutu(wiersze_zrzutu(KONF, skrot="0" * 64))
    z, stan, logi = zrzut_do(tmp_path, port)
    assert stan == "niezgodny"
    assert not (tmp_path / "konfiguracja.txt").exists()  # ani uszkodzona, ani stara treść do wysłania
    kopie = [p for p in tmp_path.iterdir() if p.name.startswith("konfiguracja.txt.poprzedni-")]
    assert len(kopie) == 1 and kopie[0].read_bytes() == b"stara=1\n"
    assert (tmp_path / "konfiguracja.txt.niezgodny").exists()
    assert any("UWAGA" in l for l in logi)
    assert z.podsumowanie(str(tmp_path), stan, False, "port")  # kod wyjścia różny od 0
    assert z.podsumowanie(str(tmp_path), stan, True, "port")


def test_wiersz_protokolu_w_tresci_czytany_jako_tresc(tmp_path):
    """Wiersze-znaczniki w treści (`PLIK_KONIEC`, `PLIKI_KONIEC`, `PLIK …`): sekcja czytana według rozmiaru z wiersza
    skrótu, więc treść wierna, a zrzut kompletny."""
    konf = b"a=1\nPLIK_KONIEC\nPLIKI_KONIEC\nPLIK /noce.csv\nb=2\n"
    port = PortZrzutu(wiersze_zrzutu(konf))
    _, stan, _ = zrzut_do(tmp_path, port)
    assert stan == "zgodny" and (tmp_path / "konfiguracja.txt").read_bytes() == konf
    assert (tmp_path / "noce.csv").read_bytes() == b"2026-11-03;1;420\n"


def test_bez_wiersza_skrotu_jak_niezgodny(tmp_path):
    w = [x for x in wiersze_zrzutu(b"a=1\n") if not x.startswith(b"konfiguracja w zrzucie")]
    z, stan, _ = zrzut_do(tmp_path, PortZrzutu(w))
    assert stan == "bez skrotu" and not (tmp_path / "konfiguracja.txt").exists()
    assert (tmp_path / "konfiguracja.txt.niezgodny").read_bytes() == b"a=1\n"
    assert z.podsumowanie(str(tmp_path), stan, True, "port") and z.podsumowanie(str(tmp_path), stan, False, "port")


def test_kopie_z_data_nic_nie_ginie(tmp_path):
    """Kolejne odczyty: każda wcześniejsza wersja zostaje w kopii z datą i godziną; stary `.niezgodny` znika z miejsca
    po udanym odczycie."""
    for t in (b"v1\n", b"v2\n", b"v3\n"):
        zrzut_do(tmp_path, PortZrzutu(wiersze_zrzutu(t)), tylko=True)
    zrzut_do(tmp_path, PortZrzutu(wiersze_zrzutu(b"zle\n", skrot="0" * 64)), tylko=True)
    assert (tmp_path / "konfiguracja.txt.niezgodny").exists()
    zrzut_do(tmp_path, PortZrzutu(wiersze_zrzutu(b"v4\n")), tylko=True)
    assert (tmp_path / "konfiguracja.txt").read_bytes() == b"v4\n"
    assert not (tmp_path / "konfiguracja.txt.niezgodny").exists()
    tresci = {p.read_bytes() for p in tmp_path.iterdir()}
    assert {b"v1\n", b"v2\n", b"v3\n", b"v4\n", b"zle\n"} <= tresci
    assert all(p.name.startswith("konfiguracja.txt") for p in tmp_path.iterdir())


def test_wgraj_konfiguracje_ze_znacznikiem_nie_wysyla(tmp_path):
    w = modul("wgraj_pliki")
    for t in (b"a=1\nPLIK_KONIEC\n", b"PLIKI_START\r\n", b"PLIK x", b"a=1\nPLIKI_KONIEC"):
        p = tmp_path / "k.txt"
        p.write_bytes(t)
        port = PortW()
        assert w.wgraj_konfiguracje(port, str(p)) == {"k.txt": "znacznik"} and port.wyslane == b""
    p.write_bytes(b"# PLIK_KONIEC\nPLIKI=1\n")
    assert w.wgraj_konfiguracje(PortW(), str(p)) == {"k.txt": "OK"}
    assert "znacznik" in w.OPISY_POWODOW


def test_tylko_konfiguracja_main(tmp_path, monkeypatch, capsys):
    """`--tylko-konfiguracja` przez main(): w katalogu tylko konfiguracja.txt, do portu tylko `Z`, bez treści na wyjściu."""
    z = modul("zrzut_plikow")
    port = PortZrzutu(wiersze_zrzutu(KONF))
    atrapa = types.ModuleType("serial")
    atrapa.Serial = lambda *a, **k: port
    monkeypatch.setitem(sys.modules, "serial", atrapa)
    with pytest.raises(SystemExit) as e:
        z.main(["port", str(tmp_path / "odczyt"), "--tylko-konfiguracja", "--czas", "2"])
    assert e.value.code is None
    assert sorted(os.listdir(tmp_path / "odczyt")) == ["konfiguracja.txt"]
    assert (tmp_path / "odczyt" / "konfiguracja.txt").read_bytes() == KONF
    assert set(port.wyslane) == {ord("Z")}
    out = capsys.readouterr().out
    assert "--konfiguracja" in out and "wymiana.maska" not in out


def test_tylko_konfiguracja_bez_pliku_na_panelu(tmp_path, monkeypatch):
    z = modul("zrzut_plikow")
    port = PortZrzutu(wiersze_zrzutu(None))
    atrapa = types.ModuleType("serial")
    atrapa.Serial = lambda *a, **k: port
    monkeypatch.setitem(sys.modules, "serial", atrapa)
    with pytest.raises(SystemExit) as e:
        z.main(["port", str(tmp_path), "--tylko-konfiguracja", "--czas", "2"])
    assert e.value.code and "nie ma /konfiguracja.txt" in e.value.code
    assert os.listdir(tmp_path) == []
    assert set(port.wyslane) == {ord("Z")}


class PortW:
    """Panel na komendę `W`: odpowiada `OK` albo `BLAD` z `bledy`; zapisuje kolejność nazw."""

    def __init__(self, bledy=None):
        self.wyslane, self.odp, self.bledy, self.nazwy = b"", [], bledy or {}, []

    def write(self, b):
        self.wyslane += b
        if b.startswith(b"W "):
            n = b.split(b"\n", 1)[0].split()[1].decode()
            self.nazwy.append(n)
            powod = self.bledy.get(n)
            if powod == "milczy":
                return len(b)
            self.odp.append((f"BLAD {n} {powod}" if powod else f"OK {n}").encode() + b"\n")
        return len(b)

    def readline(self):
        return self.odp.pop(0) if self.odp else b""


def katalog_zrzutu(k, konf=KONF):
    os.makedirs(k, exist_ok=True)
    for n in PLIKI:
        with open(os.path.join(k, n), "wb") as f:
            f.write(b"2026-11-03;1;420\n" if n == "noce.csv" else f"{n};1\n".encode())
    if konf is not None:
        with open(os.path.join(k, "konfiguracja.txt"), "wb") as f:
            f.write(konf)


def test_przywracanie_wysyla_konfiguracje_przed_k(tmp_path):
    w = modul("wgraj_pliki")
    katalog_zrzutu(str(tmp_path))
    port = PortW()
    wynik = w.wgraj(port, str(tmp_path))
    assert wynik == dict({n: "OK" for n in PLIKI}, **{"konfiguracja.txt": "OK"})
    assert port.nazwy == PLIKI + ["konfiguracja.txt"]
    k = w.komenda("konfiguracja.txt", KONF)
    assert k in port.wyslane and port.wyslane.endswith(k + b"K")  # ta sama komenda co --konfiguracja, potem K


def test_stary_zrzut_bez_konfiguracji_jak_dotad(tmp_path):
    w = modul("wgraj_pliki")
    katalog_zrzutu(str(tmp_path), konf=None)
    port = PortW()
    assert w.wgraj(port, str(tmp_path)) == {n: "OK" for n in PLIKI}
    assert port.nazwy == PLIKI and port.wyslane.endswith(b"K")


def test_odrzucona_konfiguracja_nie_wstrzymuje_k(tmp_path):
    w = modul("wgraj_pliki")
    katalog_zrzutu(str(tmp_path))
    port = PortW(bledy={"konfiguracja.txt": "zapis"})
    wynik = w.wgraj(port, str(tmp_path))
    assert wynik["konfiguracja.txt"] == "zapis" and port.nazwy.count("konfiguracja.txt") == 2  # ponowienie
    assert w.pliki_nocy_ok(wynik) and port.wyslane.endswith(b"K")


def test_brak_odpowiedzi_na_konfiguracje_bez_k(tmp_path, monkeypatch):
    w = modul("wgraj_pliki")
    katalog_zrzutu(str(tmp_path))
    t = [0.0]

    def monotonic():
        t[0] += 5.0
        return t[0]

    monkeypatch.setattr(w.time, "monotonic", monotonic)
    port = PortW(bledy={"konfiguracja.txt": "milczy"})
    wynik = w.wgraj(port, str(tmp_path))
    assert wynik["konfiguracja.txt"] == "brak odpowiedzi"
    assert not port.wyslane.endswith(b"K")


def test_odrzucony_plik_nocy_bez_k_konfiguracja_i_tak_wyslana(tmp_path):
    w = modul("wgraj_pliki")
    katalog_zrzutu(str(tmp_path))
    port = PortW(bledy={"czynniki.csv": "skrot"})
    wynik = w.wgraj(port, str(tmp_path))
    assert wynik["konfiguracja.txt"] == "OK"
    assert not w.pliki_nocy_ok(wynik) and not port.wyslane.endswith(b"K")


def test_odczyt_zmiana_linii_i_wgranie(tmp_path):
    """Procedura właściciela: odczyt → zmiana jednej linii → `--konfiguracja`; reszta pliku bajt w bajt."""
    port = PortZrzutu(wiersze_zrzutu(KONF))
    zrzut_do(tmp_path, port, tylko=True)
    p = tmp_path / "konfiguracja.txt"
    nowa = p.read_bytes().replace(b"noc_zrodlo=porownanie", b"noc_zrodlo=plik")
    p.write_bytes(nowa)
    w = modul("wgraj_pliki")
    pw = PortW()
    assert w.wgraj_konfiguracje(pw, str(p)) == {"konfiguracja.txt": "OK"}
    assert pw.wyslane == w.komenda("konfiguracja.txt", KONF.replace(b"=porownanie", b"=plik"))


def test_przejscie_przenosi_konfiguracje(tmp_path, capsys):
    p = modul("przejscie_32mb")
    for k in ("zrzut-a", "zrzut-b"):
        katalog_zrzutu(str(tmp_path / k))
    panele = []

    class Atrapa(p.AtrapaPanelu):
        def __init__(self):
            super().__init__()
            panele.append(self)

    p.AtrapaPanelu = Atrapa
    assert p.main(["port", str(tmp_path), "--symulacja"]) == 0
    assert (tmp_path / "do-wgrania" / "konfiguracja.txt").read_bytes() == KONF
    assert panele and panele[0].konfiguracja == KONF
    assert "konfiguracja.txt OK" in capsys.readouterr().out
