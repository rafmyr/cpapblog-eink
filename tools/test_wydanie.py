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
    monkeypatch.setattr(wydaj, "sekrety_z_nazwami", lambda *a: [])
    kat = tmp_path / "wyn"
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(kat)]) == 0
    assert wywolania == []  # bez --publikuj żadnego polecenia (ani budowy, ani gh)
    assert (kat / "cpap-eink-1.0.1.bin").read_bytes() == obraz.read_bytes()
    o = json.loads((kat / "wydanie.json").read_text(encoding="utf-8"))
    assert o["sha256"] == hashlib.sha256(obraz.read_bytes()).hexdigest() and o["plik"] == "cpap-eink-1.0.1.bin"


def test_main_odrzuca_obraz_z_sekretem(tmp_path, monkeypatch):
    obraz = tmp_path / "firmware.bin"
    obraz.write_bytes(b"\xe9 ... MojaSiecDomowa ..." + wydaj.ZNACZNIK)
    monkeypatch.setattr(wydaj, "sekrety_z_nazwami", lambda *a: [("WIFI_SSID", "MojaSiecDomowa")])
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(tmp_path / "w")]) == 1
    assert not (tmp_path / "w").exists()


def test_main_zla_wersja():
    assert wydaj.main(["1.0"]) == 2


def test_main_publikuje_tylko_z_flaga(tmp_path, monkeypatch):
    obraz = tmp_path / "firmware.bin"
    obraz.write_bytes(b"\xe9abc" + wydaj.ZNACZNIK)
    wywolania = []
    monkeypatch.setattr(subprocess, "run", lambda *a, **k: wywolania.append(a[0]) or subprocess.CompletedProcess(a, 0))
    monkeypatch.setattr(wydaj, "sekrety_z_nazwami", lambda *a: [])
    monkeypatch.setattr(wydaj, "sprawdz_opublikowane", lambda *a: [])
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(tmp_path / "w"), "--publikuj"]) == 0
    assert len(wywolania) == 1 and wywolania[0][:3] == ["gh", "release", "create"]
    monkeypatch.setattr(wydaj, "sprawdz_opublikowane", lambda *a: ["brak pliku wydanie.json w wydaniu"])
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(tmp_path / "w"), "--publikuj"]) == 1


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


def test_certyfikaty_obejmuja_wystawcow_github():
    """Wystawcy łańcuchów sprawdzeni 10.11.2026: api.github.com przez Sectigo E46 →
    USERTrust ECC, release-assets.githubusercontent.com przez Let's Encrypt YR1 → ISRG Root YR (krzyżowo ISRG
    Root X1). Brak któregoś korzenia = panel odrzuca serwer i pokazuje „brak połączenia z internetem”."""
    for e in ("USERTrust ECC Certification Authority", "Sectigo Public Server Authentication Root E46",
              "ISRG Root X1"):
        assert e in certyfikaty.ETYKIETY


def test_main_odrzuca_obraz_bez_znacznika(tmp_path, monkeypatch):
    """obraz zbudowany z secrets.h (bez -DBEZ_SEKRETOW) odrzucony także bez lokalnego secrets.h."""
    obraz = tmp_path / "firmware.bin"
    obraz.write_bytes(b"\xe9" + bytes(range(100)))
    monkeypatch.setattr(wydaj, "sekrety_z_nazwami", lambda *a: [])
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(tmp_path / "w")]) == 1


def test_znacznik_jak_firmware():
    with open(os.path.join(wydaj.FW, "src", "main.cpp"), encoding="utf-8") as f:
        assert wydaj.ZNACZNIK.decode() in f.read()


def test_nastepna_wersja():
    """wydaj.sh bierze następny numer po najwyższym tagu (SemVer, nie kolejność na liście)."""
    nw = wydaj.nastepna_wersja
    assert nw(["v1.0.0-beta.2"]) == "1.0.0-beta.3"
    assert nw(["v1.0.0-beta.2", "v1.0.0-beta.10", "v1.0.0-beta.9"]) == "1.0.0-beta.11"
    assert nw(["v1.0.0-beta.3", "v1.0.0"]) == "1.0.1"
    assert nw(["v1.0.1"]) == "1.0.2"
    assert nw(["v1.0.0-rc"]) == "1.0.0-rc.1"
    assert nw(["zly", "v01.0.0"]) is None and nw([]) is None
    assert wydaj.klucz_wersji("1.0.0-beta.2") < wydaj.klucz_wersji("1.0.0-beta.10") < wydaj.klucz_wersji("1.0.0")


def test_bledy_wydania():
    """Sprawdzenie po publikacji: dzisiejsze błędy z 10.11.2026 (wydanie bez plików, brak wydanie.json)."""
    dobre = {"tag_name": "v1.0.0-beta.3", "draft": False, "prerelease": False,
             "assets": [{"name": "cpap-eink-1.0.0-beta.3.bin", "state": "uploaded"},
                        {"name": "wydanie.json", "state": "uploaded"}]}
    assert wydaj.bledy_wydania(dobre, "1.0.0-beta.3") == []
    assert any("wydanie.json" in b for b in wydaj.bledy_wydania(dict(dobre, assets=dobre["assets"][:1]), "1.0.0-beta.3"))
    assert len(wydaj.bledy_wydania(dict(dobre, assets=[]), "1.0.0-beta.3")) == 2
    assert wydaj.bledy_wydania(dict(dobre, prerelease=True), "1.0.0-beta.3")
    assert wydaj.bledy_wydania(dobre, "1.0.0-beta.4")


def test_wydaj_sh_skladnia_i_bramki():
    """wydaj.sh: składnia bash i bramki przed publikacją (main, czyste drzewo, CI, pytanie „tak”)."""
    sciezka = os.path.join(os.path.dirname(__file__), "wydanie", "wydaj.sh")
    assert subprocess.run(["bash", "-n", sciezka]).returncode == 0
    with open(sciezka, encoding="utf-8") as f:
        t = f.read()
    for fraza in ('= "main" ]', "git status --porcelain", "check-runs", '[ "$ODP" = "tak" ]', "--publikuj"):
        assert fraza in t


def test_podziel_sekrety_pomija_wartosci_jawne_w_kodzie():
    """Fałszywy alarm z 10.11.2026: AB_HOST i AB_PASS równe słowu „airbridge”, które stoi w napisach programu."""
    pary = [("AB_PASS", "airbridge"), ("WIFI_PASS", "TajneHaslo123")]
    sprawdzane, pominiete = wydaj.podziel_sekrety(pary, 'log("airbridge: brak odpowiedzi");')
    assert sprawdzane == ["TajneHaslo123"] and pominiete == ["AB_PASS"]
    assert wydaj.sprawdz_obraz(b"\xe9" + wydaj.ZNACZNIK + b"airbridge", sprawdzane) == []
    assert wydaj.sprawdz_obraz(b"\xe9" + wydaj.ZNACZNIK + b"TajneHaslo123", sprawdzane)


def test_main_ostrzega_o_wartosci_jawnej_i_publikuje(tmp_path, monkeypatch, capsys):
    obraz = tmp_path / "firmware.bin"
    obraz.write_bytes(b"\xe9abc" + wydaj.ZNACZNIK + b"airbridge")
    monkeypatch.setattr(wydaj, "sekrety_z_nazwami", lambda *a: [("AB_PASS", "airbridge")])
    monkeypatch.setattr(wydaj, "tekst_kodu", lambda *a: "airbridge")
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(tmp_path / "w")]) == 0
    assert "AB_PASS" in capsys.readouterr().err


def test_tekst_kodu_bez_plikow_sekretow(tmp_path):
    (tmp_path / "a.cpp").write_text("napis airbridge", encoding="utf-8")
    (tmp_path / "secrets.h").write_text('#define WIFI_PASS "TajneHaslo123"', encoding="utf-8")
    t = wydaj.tekst_kodu([str(tmp_path)])
    assert "airbridge" in t and "TajneHaslo123" not in t


def test_katalog_budowy(monkeypatch):
    monkeypatch.setenv("PLATFORMIO_WORKSPACE_DIR", "/tmp/ws")
    assert wydaj.katalog_budowy() == os.path.join("/tmp/ws", "build", "e1001_wydanie")
    monkeypatch.delenv("PLATFORMIO_WORKSPACE_DIR")
    assert wydaj.katalog_budowy().endswith(os.path.join("firmware", "prototyp", ".pio", "build", "e1001_wydanie"))
