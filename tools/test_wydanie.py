# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy narzędzia wydania (tools/wydanie/wydaj.py) i generatora certyfikatów (tools/wydanie/certyfikaty.py), część
bez sieci i bez PlatformIO (ADR-0032, ADR-0038)."""
import hashlib
import importlib.util
import json
import os
import subprocess

import pytest

TU = os.path.dirname(os.path.abspath(__file__))


def _modul(nazwa):
    spec = importlib.util.spec_from_file_location(nazwa, os.path.join(TU, "wydanie", nazwa + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


wydaj = _modul("wydaj")
certyfikaty = _modul("certyfikaty")


@pytest.mark.parametrize("w", ["1.0.1", "1.0.0-beta.1", "10.20.30", "1.0.0-rc.1.x-y", "0.0.0"])
def test_wersja_ok(w):
    assert wydaj.wersja_ok(w)


@pytest.mark.parametrize("w", ["", "v1.0.1", "1.0", "01.0.0", "1.0.0-", "1.0.0-01", "1.0.0+abc", "1.0.0-be ta",
                               "1234567.0.0", "1.0.0-ą"])
def test_wersja_zla(w):
    assert not wydaj.wersja_ok(w)


def test_opis_jak_firmware():
    """Ten sam układ pól i zapis co aktualizacja::zapiszOpis() (firmware czyta go aktualizacja::parsujOpis())."""
    obraz = b"\xe9" + b"x" * 99
    d = wydaj.opis("1.0.1", obraz, 0x1A2B3C4D)
    t = wydaj.tekst_opisu(d)
    assert t == ('{"wersja":"1.0.1","plytka":"reterminal_e1001","uklad":"0x1a2b3c4d","rozmiar":100,"sha256":"%s",'
                 '"plik":"cpap-eink-1.0.1.bin"}\n' % hashlib.sha256(obraz).hexdigest())
    assert json.loads(t)["rozmiar"] == 100


def test_uklad_jak_firmware():
    u = wydaj.uklad()
    assert isinstance(u, int) and 0 < u <= 0xFFFFFFFF


Z = wydaj.ZNACZNIK


def test_sprawdz_obraz():
    assert wydaj.sprawdz_obraz(b"\xe9abc" + Z, []) == []
    assert any("pusty" in b for b in wydaj.sprawdz_obraz(b"", []))
    assert any("slot" in b for b in wydaj.sprawdz_obraz(b"\xe9" + Z + b"0" * wydaj.SLOT, []))
    assert any("0xE9" in b for b in wydaj.sprawdz_obraz(b"\x00abc" + Z, []))
    # bez lokalnego secrets.h kontrola dalej działa: znacznik BEZ_SEKRETOW i wartości wzoru
    assert any("znacznika" in b for b in wydaj.sprawdz_obraz(b"\xe9abc", []))
    assert any("wzoru" in b for b in wydaj.sprawdz_obraz(b"\xe9" + Z + b"..haslo-wifi..", []))
    b = wydaj.sprawdz_obraz(b"\xe9..Haslo-Domowe-1.." + Z, ["Haslo-Domowe-1"])
    assert any("secrets.h" in x for x in b) and not any("Haslo-Domowe-1" in x for x in b)  # wartość nie do logu


def test_sekrety_lokalne(tmp_path):
    p = tmp_path / "secrets.h"
    p.write_text('#define WIFI_SSID "MojaSiec"\n// #define AB_PASS "zakomentowane"\n#define AB_USER "adm"\n'
                 '#define POGODA_LAT 50.1\n', encoding="utf-8")
    assert wydaj.sekrety_lokalne(str(p)) == ["MojaSiec"]
    assert wydaj.sekrety_lokalne(str(tmp_path / "brak.h")) == []


def test_polecenia():
    cmd, env = wydaj.polecenie_budowy("1.0.1", "a/b")
    assert cmd == ["pio", "run", "-e", "e1001_wydanie"]
    assert '-DWERSJA_WYDANIA=\\"1.0.1\\"' in env["PLATFORMIO_BUILD_FLAGS"] and 'REPO_WYDAN=\\"a/b\\"' in env["PLATFORMIO_BUILD_FLAGS"]
    p = wydaj.polecenie_publikacji("1.0.1", "a/b", ["x.bin", "wydanie.json"], "nota.md")
    assert p[:4] == ["gh", "release", "create", "v1.0.1"] and "--repo" in p and p[p.index("--repo") + 1] == "a/b"


def test_platformio_ma_srodowisko_wydania():
    with open(os.path.join(wydaj.FW, "platformio.ini"), encoding="utf-8") as f:
        t = f.read()
    blok = t.split("[env:e1001_wydanie]", 1)[1].split("\n[", 1)[0]
    assert "-DBEZ_SEKRETOW" in blok and "extends = env:e1001" in blok


def test_repo_domyslne_jak_firmware():
    with open(os.path.join(wydaj.FW, "src", "aktualizacja.h"), encoding="utf-8") as f:
        t = f.read()
    assert '#define REPO_WYDAN "%s"' % wydaj.REPO_WYDAN in t and '#define PLYTKA_WYDANIA "%s"' % wydaj.PLYTKA in t


def test_main_bez_publikacji(tmp_path, monkeypatch):
    obraz = tmp_path / "firmware.bin"
    obraz.write_bytes(b"\xe9" + bytes(range(200)) + wydaj.ZNACZNIK)
    wywolania = []
    monkeypatch.setattr(subprocess, "run", lambda *a, **k: wywolania.append(a) or subprocess.CompletedProcess(a, 0))
    monkeypatch.setattr(wydaj, "sekrety_lokalne", lambda *a: [])
    kat = tmp_path / "wyn"
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(kat)]) == 0
    assert wywolania == []  # bez --publikuj żadnego polecenia (ani budowy, ani gh)
    assert (kat / "cpap-eink-1.0.1.bin").read_bytes() == obraz.read_bytes()
    o = json.loads((kat / "wydanie.json").read_text(encoding="utf-8"))
    assert o["sha256"] == hashlib.sha256(obraz.read_bytes()).hexdigest() and o["plik"] == "cpap-eink-1.0.1.bin"


def test_main_odrzuca_obraz_z_sekretem(tmp_path, monkeypatch):
    obraz = tmp_path / "firmware.bin"
    obraz.write_bytes(b"\xe9 ... MojaSiecDomowa ..." + wydaj.ZNACZNIK)
    monkeypatch.setattr(wydaj, "sekrety_lokalne", lambda *a: ["MojaSiecDomowa"])
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(tmp_path / "w")]) == 1
    assert not (tmp_path / "w").exists()


def test_main_zla_wersja():
    assert wydaj.main(["1.0"]) == 2


def test_main_publikuje_tylko_z_flaga(tmp_path, monkeypatch):
    obraz = tmp_path / "firmware.bin"
    obraz.write_bytes(b"\xe9abc" + wydaj.ZNACZNIK)
    wywolania = []
    monkeypatch.setattr(subprocess, "run", lambda *a, **k: wywolania.append(a[0]) or subprocess.CompletedProcess(a, 0))
    monkeypatch.setattr(wydaj, "sekrety_lokalne", lambda *a: [])
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(tmp_path / "w"), "--publikuj"]) == 0
    assert len(wywolania) == 1 and wywolania[0][:3] == ["gh", "release", "create"]


PAKIET = """# Label: "Inny"
-----BEGIN CERTIFICATE-----
AAAA
-----END CERTIFICATE-----
""" + "".join('# Label: "%s"\n-----BEGIN CERTIFICATE-----\nQk%d\n-----END CERTIFICATE-----\n' % (e, i)
              for i, e in enumerate(certyfikaty.ETYKIETY))


def test_certyfikaty_wytnij():
    p = certyfikaty.wytnij(PAKIET, certyfikaty.ETYKIETY)
    assert set(p) == set(certyfikaty.ETYKIETY) and "AAAA" not in "".join(p.values())
    h = certyfikaty.naglowek(p)
    assert "static const char CERTYFIKATY_GITHUB[] =" in h and h.rstrip().endswith(";")
    with pytest.raises(ValueError):
        certyfikaty.wytnij(PAKIET, certyfikaty.ETYKIETY + ["Brak"])


def test_certyfikaty_w_repo():
    with open(certyfikaty.CEL, encoding="utf-8") as f:
        t = f.read()
    for e in certyfikaty.ETYKIETY:
        assert "// " + e in t
    assert t.count("-----BEGIN CERTIFICATE-----") == len(certyfikaty.ETYKIETY)


def test_main_odrzuca_obraz_bez_znacznika(tmp_path, monkeypatch):
    """obraz zbudowany z secrets.h (bez -DBEZ_SEKRETOW) odrzucony także bez lokalnego secrets.h."""
    obraz = tmp_path / "firmware.bin"
    obraz.write_bytes(b"\xe9" + bytes(range(100)))
    monkeypatch.setattr(wydaj, "sekrety_lokalne", lambda *a: [])
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(tmp_path / "w")]) == 1


def test_znacznik_jak_firmware():
    with open(os.path.join(wydaj.FW, "src", "main.cpp"), encoding="utf-8") as f:
        assert wydaj.ZNACZNIK.decode() in f.read()
