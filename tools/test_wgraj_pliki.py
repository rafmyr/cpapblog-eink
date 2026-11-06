# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy narzędzi przejścia egzemplarza na układ 32 MB (spec techniczna 8.9).

Kontrakt, który testy zakładają (do implementacji w; pyserial importowany dopiero w main, żeby moduły
dało się importować bez niego):

tools/prototyp/wgraj_pliki.py
  PLIKI_ZRZUTU: lista nazw plików zrzutu bez ukośnika (noce.csv, czynniki.csv, humor.csv, sypialnia.csv,
                sypialnia-co2.csv, bateria.csv; 8.9 krok 1).
  komenda(nazwa: str, tresc: bytes) -> bytes
      b"W {nazwa} {len(tresc)} {sha256 hex}\\n" + tresc (R8).
  wgraj(port, katalog: str, zakoncz: bool = True) -> dict[str, str]
      Dla każdego pliku z PLIKI_ZRZUTU, który jest w `katalog`, wysyła komenda() przez port.write(bytes) i czyta
      wiersze port.readline() (bytes) do odpowiedzi `OK {nazwa}` albo `BLAD {nazwa} {powód}`; inne wiersze (log
      panelu) pomija. Wynik: nazwa → "OK" albo powód (np. "skrot"). Przy zakoncz=True na końcu wysyła b"K".

tools/prototyp/przejscie_32mb.py
  sprawdz_zrzuty(katalog_a: str, katalog_b: str) -> list[str]
      Lista problemów (pusta = zrzuty zgodne i kompletne): plik z PLIKI_ZRZUTU o różnym skrócie SHA-256 w obu
      zrzutach albo obecny tylko w jednym (nazwa pliku w opisie problemu), brak `noce.csv` w zrzucie (nazwa w opisie).
"""
import hashlib
import importlib.util
import os

import pytest

KATALOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "prototyp")
PLIKI = ["noce.csv", "czynniki.csv", "humor.csv", "sypialnia.csv", "sypialnia-co2.csv", "bateria.csv"]


def modul(nazwa):
    """Moduł tools/prototyp/{nazwa}.py albo czytelny błąd testu (bez błędu zbierania całego pakietu)."""
    sciezka = os.path.join(KATALOG, nazwa + ".py")
    if not os.path.exists(sciezka):
        pytest.fail(f"brak tools/prototyp/{nazwa}.py")
    spec = importlib.util.spec_from_file_location(nazwa, sciezka)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def zrzut(katalog, noce=b"2026-11-03;1;420;1.4\n"):
    """Zrzut syntetyczny (bez danych zdrowotnych)."""
    os.makedirs(katalog, exist_ok=True)
    for n in PLIKI:
        tresc = noce if n == "noce.csv" else f"{n};1\n".encode()
        with open(os.path.join(katalog, n), "wb") as f:
            f.write(tresc)


class AtrapaPortu:
    """Port szeregowy panelu: zbiera bajty, na każdą komendę `W` odpowiada wierszem logu i wynikiem."""

    def __init__(self, bledy=None):
        self.wyslane = b""
        self.odpowiedzi = []
        self.bledy = bledy or {}

    def write(self, b):
        self.wyslane += b
        if b.startswith(b"W "):
            naglowek = b.split(b"\n", 1)[0].decode()
            nazwa = naglowek.split()[1]
            self.odpowiedzi.append(b"ekran 0 narysowany w 10 ms\n")
            powod = self.bledy.get(nazwa)
            self.odpowiedzi.append((f"BLAD {nazwa} {powod}" if powod else f"OK {nazwa}").encode() + b"\n")
        return len(b)

    def readline(self):
        return self.odpowiedzi.pop(0) if self.odpowiedzi else b""


def test_wgraj_komenda_skrot_i_rozmiar():
    w = modul("wgraj_pliki")
    tresc = b"2026-11-03;1;420\n2026-11-04;1;400\n"
    k = w.komenda("noce.csv", tresc)
    naglowek, reszta = k.split(b"\n", 1)
    assert naglowek.decode() == f"W noce.csv {len(tresc)} {hashlib.sha256(tresc).hexdigest()}"
    assert reszta == tresc
    assert list(w.PLIKI_ZRZUTU) == PLIKI


def test_wgraj_wysyla_wszystkie_pliki_i_konczy(tmp_path):
    w = modul("wgraj_pliki")
    zrzut(str(tmp_path))
    port = AtrapaPortu()
    wynik = w.wgraj(port, str(tmp_path))
    assert wynik == {n: "OK" for n in PLIKI}
    for n in PLIKI:
        tresc = (tmp_path / n).read_bytes()
        assert w.komenda(n, tresc) in port.wyslane
    assert port.wyslane.endswith(b"K")


def test_wgraj_reaguje_na_blad(tmp_path):
    w = modul("wgraj_pliki")
    zrzut(str(tmp_path))
    port = AtrapaPortu(bledy={"czynniki.csv": "skrot"})
    wynik = w.wgraj(port, str(tmp_path))
    assert wynik["czynniki.csv"] == "skrot"
    assert all(wynik[n] == "OK" for n in PLIKI if n != "czynniki.csv")


def test_przejscie_zgodne_zrzuty(tmp_path):
    p = modul("przejscie_32mb")
    zrzut(str(tmp_path / "a"))
    zrzut(str(tmp_path / "b"))
    assert p.sprawdz_zrzuty(str(tmp_path / "a"), str(tmp_path / "b")) == []


def test_przejscie_odrzuca_rozne_skroty(tmp_path):
    p = modul("przejscie_32mb")
    zrzut(str(tmp_path / "a"))
    zrzut(str(tmp_path / "b"))
    (tmp_path / "b" / "czynniki.csv").write_bytes(b"czynniki.csv;2\n")
    problemy = p.sprawdz_zrzuty(str(tmp_path / "a"), str(tmp_path / "b"))
    assert problemy and any("czynniki.csv" in x for x in problemy)


def test_przejscie_odrzuca_zrzut_bez_nocy(tmp_path):
    p = modul("przejscie_32mb")
    zrzut(str(tmp_path / "a"))
    zrzut(str(tmp_path / "b"))
    os.remove(tmp_path / "a" / "noce.csv")
    os.remove(tmp_path / "b" / "noce.csv")
    problemy = p.sprawdz_zrzuty(str(tmp_path / "a"), str(tmp_path / "b"))
    assert problemy and any("noce.csv" in x for x in problemy)


# ---------- obrazki dnia komendą `W` (wgraj_pliki.py --obrazki) ----------

def obrazki(katalog):
    """Wyjście przygotuj.py, syntetyczne: dwa PBM, manifest i pliki, których panel nie przyjmie."""
    os.makedirs(katalog, exist_ok=True)
    pliki = {"002_morze.pbm": b"P4\n8 1\n\x55", "001_las.pbm": b"P4\n8 1\n\xaa",
             "obrazki.json": b'{"wersja":1,"obrazki":[{"plik":"001_las.pbm"}]}', "Zly Plik.pbm": b"P4\n8 1\n\x00",
             "duzy.pbm": b"P4\n8 1\n" + b"\x00" * (64 * 1024), ".DS_Store": b"x"}
    for n, t in pliki.items():
        with open(os.path.join(katalog, n), "wb") as f:
            f.write(t)
    return pliki


def test_wgraj_obrazki_nazwy_kolejnosc_i_limit(tmp_path):
    w = modul("wgraj_pliki")
    kat = str(tmp_path / "obrazki")
    pliki = obrazki(kat)
    port = AtrapaPortu()
    wynik = w.wgraj_obrazki(port, kat)
    assert wynik == {"001_las.pbm": "OK", "002_morze.pbm": "OK", "Zly Plik.pbm": "nazwa", "duzy.pbm": "rozmiar",
                     "obrazki.json": "OK"}
    import re
    naglowki = [n.decode() for n in re.findall(rb"W (obrazki/\S+) \d+ [0-9a-f]{64}\n", port.wyslane)]
    assert naglowki == ["obrazki/001_las.pbm", "obrazki/002_morze.pbm", "obrazki/obrazki.json"]  # manifest na końcu
    assert w.komenda("obrazki/001_las.pbm", pliki["001_las.pbm"]) in port.wyslane
    assert not port.wyslane.endswith(b"K")
    assert b".DS_Store" not in port.wyslane and b"Zly" not in port.wyslane


def test_wgraj_obrazki_ponowienie_po_skrocie(tmp_path):
    w = modul("wgraj_pliki")
    kat = str(tmp_path / "obrazki")
    obrazki(kat)

    class RazZly(AtrapaPortu):
        def write(self, b):
            if b.startswith(b"W obrazki/002_morze.pbm") and not getattr(self, "byl", False):
                self.byl = True
                self.wyslane += b
                self.odpowiedzi.append(b"BLAD obrazki/002_morze.pbm skrot\n")
                return len(b)
            return super().write(b)

    port = RazZly()
    wynik = w.wgraj_obrazki(port, kat)
    assert wynik["002_morze.pbm"] == "OK"
    assert port.wyslane.count(b"W obrazki/002_morze.pbm") == 2


def test_obrazki_nazwy_zgodne_z_panelem():
    """Wzór nazwy w narzędziu jak src/magazyn.cpp nazwaObrazkaW() (MAKS_NAZWY_OBRAZKA_W = 40) i limit 64 KB."""
    w = modul("wgraj_pliki")
    for n in ("001_las.pbm", "a-b_9.pbm", "a" * 40 + ".pbm"):
        assert w.NAZWA_OBRAZKA.match(n), n
    for n in ("Las.pbm", "las.PBM", ".pbm", "a" * 41 + ".pbm", "../x.pbm", "a/b.pbm", "las.pbm\n", "ł.pbm"):
        assert not w.NAZWA_OBRAZKA.match(n), n
    assert w.MAKS_OBRAZKA == 64 * 1024
    zrodlo = open(os.path.join(os.path.dirname(KATALOG), "..", "firmware", "prototyp", "src", "magazyn.h"),
                  encoding="utf-8").read()
    assert "MAKS_NAZWY_OBRAZKA_W = 40" in zrodlo and "MAKS_OBRAZKA_W = (size_t)64 * 1024" in zrodlo


def test_obrazki_main_wymaga_katalogu_albo_obrazkow():
    w = modul("wgraj_pliki")
    with pytest.raises(SystemExit):
        w.main(["/dev/null"])
    with pytest.raises(SystemExit):
        w.main(["/dev/null", "zrzut", "--obrazki", "obrazki"])


def test_obrazki_manifest_bez_pozycji_ostrzega(tmp_path):
    """manifest bez pozycji dla wysłanych PBM: ostrzeżenie (panel ich nie pokaże), bez przerwania."""
    w = modul("wgraj_pliki")
    kat = str(tmp_path / "obrazki")
    os.makedirs(kat)
    with open(os.path.join(kat, "001_las.pbm"), "wb") as f:
        f.write(b"P4\n8 1\n\xaa")
    with open(os.path.join(kat, "obrazki.json"), "wb") as f:
        f.write(b'{"wersja":1,"obrazki":[]}')
    logi = []
    assert w.wgraj_obrazki(AtrapaPortu(), kat, log=logi.append) == {"001_las.pbm": "OK", "obrazki.json": "OK"}
    assert any("UWAGA" in l and "001_las.pbm" in l for l in logi)
    assert w.pliki_bez_pozycji(b'{"obrazki":[{"plik":"a.pbm"}]}', ["a.pbm"]) == []
    assert w.pliki_bez_pozycji(b"nie json", ["a.pbm"]) == []


def test_obrazki_zrzut_przerywa_po_braku_odpowiedzi(tmp_path, monkeypatch):
    """w trybie zrzutu: po `brak odpowiedzi` żaden następny plik nie idzie do portu i nie ma `K`."""
    w = modul("wgraj_pliki")
    kat = str(tmp_path / "zrzut")
    zrzut(kat)
    monkeypatch.setattr(w.time, "monotonic", iter(range(0, 10**7, 50)).__next__)

    class Milczy(AtrapaPortu):
        def write(self, b):
            self.wyslane += b
            return len(b)

    port = Milczy()
    wynik = w.wgraj(port, kat)
    assert list(wynik.values()) == ["brak odpowiedzi"]
    assert port.wyslane.count(b"W ") == 1 and not port.wyslane.endswith(b"K")


def test_wgraj_konfiguracje_nazwa_i_ponowienie(tmp_path):
    w = modul("wgraj_pliki")
    plik = tmp_path / "moje ustawienia.txt"
    tresc = b"profil=publiczny\nco2=nie\n# komentarz\n"
    plik.write_bytes(tresc)

    class RazZly(AtrapaPortu):
        def write(self, b):
            if b.startswith(b"W konfiguracja.txt") and not getattr(self, "byl", False):
                self.byl = True
                self.wyslane += b
                self.odpowiedzi.append(b"BLAD konfiguracja.txt skrot\n")
                return len(b)
            return super().write(b)

    port = RazZly()
    assert w.wgraj_konfiguracje(port, str(plik)) == {"moje ustawienia.txt": "OK"}
    assert port.wyslane.count(w.komenda("konfiguracja.txt", tresc)) == 2  # pod nazwą panelu, nie z dysku
    assert not port.wyslane.endswith(b"K")


def test_wgraj_konfiguracje_odrzuca_przed_wyslaniem(tmp_path):
    w = modul("wgraj_pliki")
    duzy = tmp_path / "duzy.txt"
    duzy.write_bytes(b"a" * (16 * 1024 + 1))
    zero = tmp_path / "zero.txt"
    zero.write_bytes(b"a=1\0b")
    ok = tmp_path / "ok.txt"
    ok.write_bytes(b"a" * (16 * 1024))
    link = tmp_path / "link.txt"
    link.symlink_to(ok)
    port = AtrapaPortu()
    assert w.wgraj_konfiguracje(port, str(duzy)) == {"duzy.txt": "rozmiar"}
    assert w.wgraj_konfiguracje(port, str(zero)) == {"zero.txt": "tresc"}
    assert w.wgraj_konfiguracje(port, str(link)) == {"link.txt": "dowiazanie"}
    assert w.wgraj_konfiguracje(port, str(tmp_path / "nie.txt")) == {"nie.txt": "brak"}
    assert port.wyslane == b""
    assert w.wgraj_konfiguracje(port, str(ok)) == {"ok.txt": "OK"}  # dokładnie 16 KB przechodzi


def test_limit_konfiguracji_zgodny_z_panelem():
    w = modul("wgraj_pliki")
    zrodlo = open(os.path.join(os.path.dirname(KATALOG), "..", "firmware", "prototyp", "src", "magazyn.h"),
                  encoding="utf-8").read()
    assert w.NAZWA_KONFIGURACJI == "konfiguracja.txt" and w.MAKS_KONFIGURACJI == 16 * 1024
    assert 'NAZWA_KONFIGURACJI_W = "konfiguracja.txt"' in zrodlo
    assert "MAKS_KONFIGURACJI_W = (size_t)16 * 1024" in zrodlo


def test_main_konfiguracja_wylacznie_z_innymi():
    w = modul("wgraj_pliki")
    with pytest.raises(SystemExit):
        w.main(["/dev/null", "--konfiguracja", "k.txt", "--obrazki", "obrazki"])
    with pytest.raises(SystemExit):
        w.main(["/dev/null", "zrzut", "--konfiguracja", "k.txt"])
