# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy przypadków brzegowych: testy narzędzi przejścia egzemplarza (spec 8.9),
które pokazują znalezione błędy; każdy test pilnuje, żeby błąd nie wrócił.
Dane syntetyczne, bez prawdziwych danych zdrowotnych.
"""
import importlib.util
import os

TU = os.path.dirname(os.path.abspath(__file__))


def modul(sciezka, nazwa):
    spec = importlib.util.spec_from_file_location(nazwa, sciezka)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def przejscie():
    return modul(os.path.join(TU, "prototyp", "przejscie_32mb.py"), "przejscie_32mb_r")


def wgraj_mod():
    return modul(os.path.join(TU, "prototyp", "wgraj_pliki.py"), "wgraj_pliki_r")


def zrzut_v0(katalog, nocy=5):
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


class PortPanelu:
    """Port panelu w oknie przywracania: na `W` odpowiada OK albo BLAD skrot (pierwszy plik z listy `zle`)."""

    def __init__(self, zle=()):
        self.zle = set(zle)
        self.wyslane = b""
        self.wyjscie = []

    def write(self, b):
        self.wyslane += b
        while True:
            if b"\n" not in self.wyslane:
                break
            naglowek, reszta = self.wyslane.split(b"\n", 1)
            p = naglowek.decode().split()
            if not p or p[0] != "W" or len(reszta) < int(p[2]):
                break
            self.wyslane = reszta[int(p[2]):]
            self.wyjscie.append(("BLAD %s skrot\n" % p[1] if p[1] in self.zle else "OK %s\n" % p[1]).encode())
        return len(b)

    def readline(self):
        return self.wyjscie.pop(0) if self.wyjscie else b""


def test_K_po_odrzuconym_pliku(tmp_path):
    """8.9 krok 4: po `BLAD` któregoś pliku wgraj() i tak wysyła `K`, więc panel przywraca niepełny zestaw, noce
    trafiają na partycję i okno przywracania już się nie otworzy (test C++ r04). Skrypt każe potem „powtórzyć krok 4”,
    czego firmware nie przyjmie. `K` po błędzie powinno nie pójść (okno trwa, można wysłać plik jeszcze raz)."""
    w = wgraj_mod()
    zrzut_v0(str(tmp_path))
    port = PortPanelu(zle={"czynniki.csv"})
    wynik = w.wgraj(port, str(tmp_path), zakoncz=True)
    assert wynik["czynniki.csv"] == "skrot"
    assert not port.wyslane.endswith(b"K"), "K wysłane mimo odrzuconego pliku: przywrócenie niepełnego zrzutu"


def test_okno_otwarte_przed_otwarciem_portu(tmp_path, monkeypatch, capsys):
    """8.9 krok 3–4 [hipoteza co do mostka USB-UART]: `pio run -t upload` resetuje panel; panel formatuje partycję i
    wypisuje `okno przywracania …` raz, zanim użytkownik naciśnie Enter i skrypt otworzy port, więc wiersz przepada.
    przejscie_32mb.py czeka tylko na ten wiersz (180 s) i kończy krok 4 bez wysłania plików, choć okno trwa jeszcze
    kilka minut; nie ma też opcji --reset. Po 10 min panel pobiera noce z AirBridge i okno nie wróci (test C++ r03)."""
    p = przejscie()
    zrzut_v0(str(tmp_path / "zrzut-a"), 7)
    zrzut_v0(str(tmp_path / "zrzut-b"), 7)
    port = PortPanelu()  # okno otwarte, ale jego wiersz logu był przed otwarciem portu

    monkeypatch.setattr(p, "AtrapaPanelu", lambda: port)
    czas = [0.0]

    def monotonic():
        czas[0] += 5.0
        return czas[0]

    monkeypatch.setattr(p.wgraj_pliki.time, "monotonic", monotonic)
    p.main(["port", str(tmp_path), "--symulacja"])
    out = capsys.readouterr().out
    assert b"W noce.csv" in port.wyslane or port.wyjscie, "krok 4 przerwany bez wysłania plików do otwartego okna:\n" + out


def test_zrzut_z_panelu_po_przejsciu_bez_nocy_w_v0(tmp_path, capsys):
    """8.9 jako droga zmiany wariantu tablicy (R6, komentarz w firmware/partycje-e1001-16mb.csv: „przejście między
    wariantami to też wgranie kablem i 8.9”): zrzut `Z` panelu z ma puste sekcje plików prototypu (`/noce.csv`
    nie istnieje) i noce w `dane/noce/RRRR.csv`. zrzut_plikow.py zapisuje pusty `noce.csv`, sprawdz_zrzuty() go
    przyjmuje, liczba nocy = 0, a skrypt kończy się sukcesem `przywrócono 0 z 0`: historia zostaje tylko na komputerze,
    a panel po pobraniu z AirBridge już jej nie przyjmie."""
    p = przejscie()
    for k in ("zrzut-a", "zrzut-b"):
        d = tmp_path / k
        os.makedirs(d / "dane" / "noce", exist_ok=True)
        for n in p.PLIKI_ZRZUTU:  # puste sekcje PLIK /noce.csv … PLIK_KONIEC
            (d / n).write_bytes(b"")
        (d / "dane" / "noce" / "2026.csv").write_bytes(
            b"#cpap-eink noce 1\ndata;wazna\n" + b"".join(b"2026-10-%02d;1\n" % i for i in range(1, 31)) + b"#koniec 30\n")
    kod = p.main(["port", str(tmp_path), "--symulacja"])
    out = capsys.readouterr().out
    assert kod != 0, "przejście zgłosiło sukces z 0 nocy, choć zrzut ma 30 nocy w dane/noce/:\n" + out
