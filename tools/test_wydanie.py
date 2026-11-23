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


def test_opis_z_nowosciami_jak_firmware():
    """Pole `nowosci` (R-KONF-56) na końcu, z ucieczką jak aktualizacja::zapiszOpis(); bez linii pola nie ma."""
    obraz = b"\xe9" + b"x" * 99
    d = wydaj.opis("1.0.1", obraz, 0x1A2B3C4D, nowosci=["Portret nocy: łagodniejsze piki", 'Cudzysłów " i ukośnik \\'])
    t = wydaj.tekst_opisu(d)
    assert t.endswith(',"plik":"cpap-eink-1.0.1.bin","nowosci":["Portret nocy: łagodniejsze piki",'
                      '"Cudzysłów \\" i ukośnik \\\\"]}\n')
    assert json.loads(t)["nowosci"][1] == 'Cudzysłów " i ukośnik \\'
    assert "nowosci" not in wydaj.opis("1.0.1", obraz, 0x1A2B3C4D)
    assert "nowosci" not in wydaj.opis("1.0.1", obraz, 0x1A2B3C4D, nowosci=[])


@pytest.mark.parametrize("linie", [
    [],
    ["Portret nocy: łagodniejsze piki", "Znak na belce, gdy dane są nieaktualne", "Ustawienia w nowym układzie"],
    ["x" * 40],
    ["Nowy wykres ciśnienia w kokpicie"],   # o panelu, bez wartości i bez czynności na aparacie
])
def test_nowosci_dobre(linie):
    assert wydaj.bledy_nowosci(linie) == []


@pytest.mark.parametrize("linie,fraza", [
    (["a", "b", "c", "d"], "najwyżej 3"),
    (["x" * 41], "41 znaków"),
    ([""], "pusta"),
    (["Emoji 😀"], "nie wyświetli"),
    (["Strzałka ▶ w środku"], "nie wyświetli"),
    (["Ciśnienie 7 cmH2O"], "ADR-0002"),
    (["Obniż EPR o 1"], "ADR-0002"),
    (["Ustaw minimum ciśnienia na 7"], "ADR-0002"),
])
def test_nowosci_zle(linie, fraza):
    b = wydaj.bledy_nowosci(linie)
    assert b and any(fraza in x for x in b)


def test_main_nowosci(tmp_path, monkeypatch, capsys):
    """--sprawdz-nowosci kończy bez budowy; zła linia zatrzymuje wydanie przed budową (kod 2)."""
    monkeypatch.setattr(wydaj.subprocess, "run", lambda *a, **k: (_ for _ in ()).throw(AssertionError("bez budowy")))
    assert wydaj.main(["--sprawdz-nowosci", "--nowosc", "Portret nocy: łagodniejsze piki"]) == 0
    assert wydaj.main(["1.0.1", "--nowosc", "Obniż EPR o 1"]) == 2
    assert "ADR-0002" in capsys.readouterr().err


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
    for fraza in ('= "main" ]', "git status --porcelain", "check-runs", '[ "$ODP" = "tak" ]', "--publikuj",
                  "--sprawdz-nowosci", '${NOWOSCI[@]+"${NOWOSCI[@]}"}'):
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


# ---------- kanał wczesny: wydanie jako pre-release (R-KONF-62, ADR-0032 „Zmiana 2026-11-23”) ----------
def test_polecenie_publikacji_wczesne():
    """--wczesne dokłada `--prerelease`; bez niego polecenie jak dotąd (bez flagi)."""
    zwykle = wydaj.polecenie_publikacji("1.0.1", "a/b", ["x.bin", "wydanie.json"], "nota.md")
    assert "--prerelease" not in zwykle
    wczesne = wydaj.polecenie_publikacji("1.1.0-beta.1", "a/b", ["x.bin", "wydanie.json"], "nota.md", True)
    assert wczesne == wydaj.polecenie_publikacji("1.1.0-beta.1", "a/b", ["x.bin", "wydanie.json"], "nota.md") + \
        ["--prerelease"]


def test_bledy_wydania_wczesnego():
    dobre = {"tag_name": "v1.1.0-beta.1", "draft": False, "prerelease": True,
             "assets": [{"name": "cpap-eink-1.1.0-beta.1.bin", "state": "uploaded"},
                        {"name": "wydanie.json", "state": "uploaded"}]}
    assert wydaj.bledy_wydania_wczesnego(dobre, "1.1.0-beta.1") == []
    assert any("pre-release" in b for b in wydaj.bledy_wydania_wczesnego(dict(dobre, prerelease=False), "1.1.0-beta.1"))
    assert any("robocze" in b for b in wydaj.bledy_wydania_wczesnego(dict(dobre, draft=True), "1.1.0-beta.1"))
    assert len(wydaj.bledy_wydania_wczesnego(dict(dobre, assets=[]), "1.1.0-beta.1")) == 2
    assert wydaj.bledy_wydania_wczesnego(dobre, "1.1.0-beta.2")
    # dla zwykłych wydań dzisiejsza kontrola zostaje: pre-release to błąd (panel bez kanału go nie zobaczy)
    assert wydaj.bledy_wydania(dobre, "1.1.0-beta.1")


def test_sprawdz_opublikowane_adres(monkeypatch):
    """Zwykłe: `releases/latest` jak dotąd; wczesne: wydanie z tagu (latest pomija pre-release)."""
    adresy = []
    w = {"tag_name": "v1.1.0-beta.1", "draft": False, "prerelease": True,
         "assets": [{"name": "cpap-eink-1.1.0-beta.1.bin", "state": "uploaded"},
                    {"name": "wydanie.json", "state": "uploaded"}]}

    def run(cmd, **k):
        adresy.append(cmd[2])
        return subprocess.CompletedProcess(cmd, 0, stdout=json.dumps(w), stderr="")
    monkeypatch.setattr(subprocess, "run", run)
    assert wydaj.sprawdz_opublikowane("a/b", "1.1.0-beta.1", True) == []
    assert wydaj.sprawdz_opublikowane("a/b", "1.1.0-beta.1")  # zwykła kontrola: pre-release = błąd
    assert adresy == ["repos/a/b/releases/tags/v1.1.0-beta.1", "repos/a/b/releases/latest"]


def test_main_wczesne_publikuje_prerelease_z_ostrzezeniem(tmp_path, monkeypatch, capsys):
    obraz = tmp_path / "firmware.bin"
    obraz.write_bytes(b"\xe9abc" + wydaj.ZNACZNIK)
    wywolania, sprawdzenia = [], []
    monkeypatch.setattr(subprocess, "run", lambda *a, **k: wywolania.append(a[0]) or subprocess.CompletedProcess(a, 0))
    monkeypatch.setattr(wydaj, "sekrety_z_nazwami", lambda *a: [])
    monkeypatch.setattr(wydaj, "sprawdz_opublikowane", lambda *a: sprawdzenia.append(a) or [])
    assert wydaj.main(["1.1.0-beta.1", "--obraz", str(obraz), "--katalog", str(tmp_path / "w"), "--publikuj",
                       "--wczesne"]) == 0
    assert len(wywolania) == 1 and wywolania[0][:3] == ["gh", "release", "create"] and "--prerelease" in wywolania[0]
    assert sprawdzenia == [(wydaj.REPO_WYDAN, "1.1.0-beta.1", True)]
    err = capsys.readouterr().err
    assert "nie zobaczy" in err and "aktualizacja_wczesna=tak" in err
    # bez --wczesne: bez `--prerelease`, kontrola zwykła, bez ostrzeżenia
    wywolania.clear()
    sprawdzenia.clear()
    assert wydaj.main(["1.0.1", "--obraz", str(obraz), "--katalog", str(tmp_path / "w2"), "--publikuj"]) == 0
    assert "--prerelease" not in wywolania[0] and sprawdzenia == [(wydaj.REPO_WYDAN, "1.0.1", False)]
    assert "nie zobaczy" not in capsys.readouterr().err


def test_wydaj_sh_wczesne():
    """wydaj.sh --wczesne: przekazuje flagę do wydaj.py i ostrzega przed pytaniem „tak”."""
    sciezka = os.path.join(os.path.dirname(__file__), "wydanie", "wydaj.sh")
    with open(sciezka, encoding="utf-8") as f:
        t = f.read()
    assert '--wczesne) WCZESNE=(--wczesne)' in t and '${WCZESNE[@]+"${WCZESNE[@]}"}' in t
    assert t.index("Panel bez kanału wczesnego tego wydania nie zobaczy") < t.index('[ "$ODP" = "tak" ]')


def test_kanal_wczesny_klucz_jak_firmware():
    """Ostrzeżenie podaje ten sam klucz konfiguracji co firmware (KLUCZ_KANALU_WCZESNEGO) i adres listy wydań."""
    with open(os.path.join(wydaj.FW, "src", "aktualizacja.h"), encoding="utf-8") as f:
        t = f.read()
    assert 'KLUCZ_KANALU_WCZESNEGO = "aktualizacja_wczesna"' in t
    assert "aktualizacja_wczesna=tak" in wydaj.OSTRZEZENIE_WCZESNE


# ---------- testy brzegowe kanału wczesnego: argumenty wydaj.sh ----------
def _uruchom_wydaj_sh(tmp_path, argumenty, checki=None):
    """wydaj.sh na atrapach (git z `origin`, gh, pio, wydaj.py zapisujący argumenty); zwraca (kod, argumenty wydaj.py
    przy publikacji albo None, wyjście)."""
    import shutil
    korzen = tmp_path / "repo"
    (korzen / "tools" / "wydanie").mkdir(parents=True)
    shutil.copy(os.path.join(os.path.dirname(__file__), "wydanie", "wydaj.sh"), korzen / "tools" / "wydanie" / "wydaj.sh")
    zapis = tmp_path / "argumenty.json"
    (korzen / "tools" / "wydanie" / "wydaj.py").write_text(
        "import json, sys\n"
        "if '--sprawdz-nowosci' in sys.argv or '--nastepna' in sys.argv:\n"
        "    print('1.0.2') if '--nastepna' in sys.argv else None\n"
        "    sys.exit(0)\n"
        "json.dump(sys.argv[1:], open(%r, 'w'))\n" % str(zapis), encoding="utf-8")
    bin_ = tmp_path / "bin"
    bin_.mkdir()
    (bin_ / "gh").write_text(
        "#!/bin/sh\n"
        "case \"$*\" in\n"
        "  *check-runs*) printf '%b' \"$CHECKI\";;\n"
        "  *releases/tags*) exit 1;;\n"
        "esac\n"
        "exit 0\n")
    (bin_ / "pio").write_text("#!/bin/sh\nexit 0\n")
    for n in ("gh", "pio"):
        os.chmod(bin_ / n, 0o755)
    if checki is None:
        checki = "wynik: pełne CI\tcompleted\tsuccess\nsanitizery (4)\tcompleted\tsuccess\ngaleria\tcompleted\tskipped\n"
    env = dict(os.environ, PATH=str(bin_) + os.pathsep + os.environ["PATH"], HOME=str(tmp_path), CHECKI=checki,
               GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
    zdalne = tmp_path / "zdalne.git"
    subprocess.run(["git", "init", "-q", "--bare", str(zdalne)], check=True, env=env)
    for c in (["git", "init", "-q", "-b", "main"], ["git", "add", "tools"], ["git", "commit", "-q", "-m", "x"],
              ["git", "remote", "add", "origin", str(zdalne)], ["git", "push", "-q", "origin", "main"]):
        subprocess.run(c, cwd=korzen, check=True, env=env, capture_output=True)
    r = subprocess.run(["bash", str(korzen / "tools" / "wydanie" / "wydaj.sh"), *argumenty], cwd=korzen, env=env,
                       input="\ntak\n", capture_output=True, text=True)
    arg = json.loads(zapis.read_text()) if zapis.exists() else None
    return r.returncode, arg, r.stdout + r.stderr


def test_brzeg_wydaj_sh_wczesne_na_poczatku(tmp_path):
    kod, arg, _ = _uruchom_wydaj_sh(tmp_path, ["--wczesne", "1.1.0-beta.1"])
    assert kod == 0 and arg is not None and "--wczesne" in arg and arg[0] == "1.1.0-beta.1"


def test_brzeg_wydaj_sh_zwykle_bez_flagi(tmp_path):
    kod, arg, wyj = _uruchom_wydaj_sh(tmp_path, ["1.0.2"])
    assert kod == 0 and arg is not None and "--wczesne" not in arg and "wersja próbna" not in wyj


def test_brzeg_wydaj_sh_flaga_po_numerze_nie_daje_zwyklego(tmp_path):
    kod, arg, _ = _uruchom_wydaj_sh(tmp_path, ["1.1.0-beta.1", "--wczesne"])
    # dopuszczalne: odmowa bez publikacji albo publikacja jako pre-release; niedopuszczalne: zwykłe wydanie
    assert arg is None or "--wczesne" in arg


def test_wydaj_sh_argumenty_wczesne(tmp_path):
    """Flaga po numerze daje pre-release; nieznany przełącznik i drugi numer = odmowa bez
    publikacji; zwykłe wydanie z numerem `-beta`/`-rc` ostrzega przed „tak”."""
    for n in "abcde":
        (tmp_path / n).mkdir()
    kod, arg, _ = _uruchom_wydaj_sh(tmp_path / "a", ["1.1.0-beta.1", "--wczesne"])
    assert kod == 0 and arg is not None and "--wczesne" in arg and arg[0] == "1.1.0-beta.1"
    kod, arg, wyj = _uruchom_wydaj_sh(tmp_path / "b", ["1.1.0-beta.1", "--wczesna"])
    assert kod != 0 and arg is None and "nieznany przełącznik" in wyj
    kod, arg, wyj = _uruchom_wydaj_sh(tmp_path / "c", ["1.1.0", "1.1.1"])
    assert kod != 0 and arg is None and "za dużo argumentów" in wyj
    kod, arg, wyj = _uruchom_wydaj_sh(tmp_path / "d", ["1.1.0-rc.1"])
    assert kod == 0 and arg is not None and "--wczesne" not in arg and "wygląda na wersję próbną" in wyj
    kod, arg, wyj = _uruchom_wydaj_sh(tmp_path / "e", ["--wczesne", "1.1.0-beta.2"])
    assert "wygląda na wersję próbną" not in wyj


def test_wydaj_sh_zielone_ci_wynik_pelne(tmp_path):
    """Reguła „zielone CI”: nazwy ze spacjami i pominięte zadania galerii nie blokują wydania;
    brak „wynik: pełne CI”, zadanie w toku albo failure blokują je bez publikacji."""
    kod, arg, wyj = _uruchom_wydaj_sh(tmp_path / "a", ["1.0.2"])
    assert kod == 0 and arg is not None, wyj
    przypadki = {
        "b": "testy\tcompleted\tsuccess\n",
        "c": "wynik: pełne CI\tcompleted\tsuccess\nsanitizery (2)\tin_progress\t\n",
        "d": "wynik: pełne CI\tcompleted\tsuccess\ngaleria\tcompleted\tfailure\n",
        "e": "wynik: pełne CI\tcompleted\tfailure\n",
        "f": "wynik: szybka bramka (nie do scalenia)\tcompleted\tsuccess\n",
        "g": "wynik: pełne CI\tcompleted\tsuccess\nsanitizery (1)\tin_progress\tnull\n",
        "h": "wynik: pełne CI\tcompleted\tsuccess\ntesty\tcompleted\tcancelled\n",
        "i": "wynik: pełne CI\tcompleted\tskipped\n",
        "j": "",
    }
    for k, checki in przypadki.items():
        (tmp_path / k).mkdir()
        kod, arg, wyj = _uruchom_wydaj_sh(tmp_path / k, ["1.0.2"], checki)
        assert kod != 0 and arg is None, (k, wyj)
