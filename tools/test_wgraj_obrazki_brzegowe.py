# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy przypadków brzegowych (wgraj_pliki.py --obrazki); bez xfail, więc blokują.

Dane syntetyczne; bez portu szeregowego (atrapa portu, atrapa modułu `serial`).
"""
import hashlib
import importlib.util
import os
import sys
import types

import pytest

KATALOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "prototyp")


def modul():
    spec = importlib.util.spec_from_file_location("wgraj_pliki_r19", os.path.join(KATALOG, "wgraj_pliki.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def pbm(w=16, h=2):
    return b"P4\n%d %d\n" % (w, h) + b"\x55" * ((w + 7) // 8 * h)


class Port:
    """Odpowiada `OK`, chyba że plik w `milczy` (panel zasnął: brak odpowiedzi) albo w `bledy`."""

    def __init__(self, milczy=(), bledy=None, stan=b"przywracanie: poza oknem\n"):
        self.wyslane = b""
        self.odp = []
        self.milczy = set(milczy)
        self.bledy = bledy or {}
        self.stan = stan
        self.zasnal = False

    def write(self, b):
        self.wyslane += b
        if b == b"?":
            self.odp.append(self.stan)
        elif b.startswith(b"W "):
            n = b.split(b"\n", 1)[0].split()[1].decode()
            if n in self.milczy or self.zasnal:
                self.zasnal = True
                return len(b)
            p = self.bledy.get(n)
            self.odp.append((f"BLAD {n} {p}" if p else f"OK {n}").encode() + b"\n")
        return len(b)

    def readline(self):
        return self.odp.pop(0) if self.odp else b""


def katalog(tmp_path, nazwy=("001_a.pbm", "002_b.pbm", "003_c.pbm")):
    k = tmp_path / "obrazki"
    k.mkdir()
    for n in nazwy:
        (k / n).write_bytes(pbm())
    (k / "obrazki.json").write_bytes(b'{"wersja":1,"obrazki":[]}')
    return k


def test_dowiazanie_poza_katalog(tmp_path):
    w = modul()
    k = katalog(tmp_path, ())
    poza = tmp_path / "prywatne.pbm"
    poza.write_bytes(pbm(8, 1) + b"TAJNE")
    os.symlink(poza, k / "009_link.pbm")
    port = Port()
    w.wgraj_obrazki(port, str(k))
    assert b"TAJNE" not in port.wyslane


def test_stop_po_braku_odpowiedzi(tmp_path, monkeypatch):
    w = modul()
    k = katalog(tmp_path)
    monkeypatch.setattr(w.time, "monotonic", iter(range(0, 10**7, 50)).__next__)  # limit odpowiedzi mija od razu
    port = Port(milczy={"obrazki/001_a.pbm"})
    wynik = w.wgraj_obrazki(port, str(k))
    assert wynik["001_a.pbm"] == "brak odpowiedzi"
    assert port.wyslane.count(b"W obrazki/") == 1, "po pierwszym braku odpowiedzi nie wysyłać kolejnych plików"


def test_zly_port_komunikat(tmp_path, monkeypatch, capsys):
    w = modul()
    k = katalog(tmp_path)
    fake = types.ModuleType("serial")

    class SerialException(OSError):
        pass

    def Serial(*a, **kw):
        raise SerialException("[Errno 2] could not open port /dev/cu.usbserial-999: No such file or directory")

    fake.SerialException = SerialException
    fake.Serial = Serial
    monkeypatch.setitem(sys.modules, "serial", fake)
    with pytest.raises(SystemExit) as e:  # oczekiwane: sys.exit("nie ma portu …, sprawdź ls /dev/cu.*")
        w.main(["/dev/cu.usbserial-999", "--obrazki", str(k)])
    assert "port" in str(e.value.code)


def test_komunikat_brak_odpowiedzi(tmp_path, monkeypatch):
    w = modul()
    k = katalog(tmp_path)
    monkeypatch.setattr(w.time, "monotonic", iter(range(0, 10**7, 50)).__next__)
    a = types.SimpleNamespace(obrazki=str(k), czas=5)
    port = Port(milczy={"obrazki/001_a.pbm"})
    monkeypatch.setattr(w, "czekaj_na_okno", lambda *x, **y: "poza oknem")
    kod = w.obrazki_main(port, a)
    assert kod and "popraw" not in kod and "przycisk" in kod


def test_okno_przywracania_rada(tmp_path, monkeypatch, capsys):
    w = modul()
    k = katalog(tmp_path)
    a = types.SimpleNamespace(obrazki=str(k), czas=5)
    monkeypatch.setattr(w, "czekaj_na_okno", lambda *x, **y: "otwarte")
    assert w.obrazki_main(Port(), a) is None
    out = capsys.readouterr().out
    assert "okno przywracania" in out and "przycisk" in out


def test_trzyma_nazwy_i_limit(tmp_path):
    """Trzyma: pliki spoza wzoru i ponad 64 KB nie wychodzą z komputera; podkatalogi i ukryte pomijane."""
    w = modul()
    k = katalog(tmp_path, ("001_a.pbm",))
    (k / "Duze.PBM").write_bytes(pbm())
    (k / "za_duzy.pbm").write_bytes(b"P4\n8 1\n" + b"\0" * (64 * 1024))
    (k / "pod").mkdir()
    (k / "pod" / "x.pbm").write_bytes(pbm())
    (k / ".ukryty.pbm").write_bytes(pbm())
    (k / "..pbm").write_bytes(pbm())
    port = Port()
    wynik = w.wgraj_obrazki(port, str(k))
    assert wynik["Duze.PBM"] == "nazwa" and wynik["za_duzy.pbm"] == "rozmiar"
    assert port.wyslane.count(b"W obrazki/") == 2  # 001_a.pbm i obrazki.json
    assert b"obrazki/pod" not in port.wyslane and b".ukryty" not in port.wyslane
    naglowek = w.komenda("obrazki/001_a.pbm", pbm()).split(b"\n", 1)[0]
    assert naglowek.endswith(hashlib.sha256(pbm()).hexdigest().encode())
