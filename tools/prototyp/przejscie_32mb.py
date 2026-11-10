#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Przejście egzemplarza z prototypu na układ partycji 32 MB.

Kroki (jednorazowo, z kablem USB przy panelu; dane zdrowotne tylko w katalogu poza repozytorium, WY12):
  1. Zrzut plików na obecnym firmware dwa razy (tools/prototyp/zrzut_plikow.py) do <katalog>/zrzut-a i zrzut-b.
  2. Porównanie skrótów SHA-256 obu zrzutów (wszystkie pliki, także `dane/…` i `log/…` z firmware) i kontrola
     kompletności. Różne skróty albo zrzut bez nocy = koniec bez wgrywania (zrzut do powtórzenia).
     Pliki do wgrania (<katalog>/do-wgrania/): pliki prototypu ze zrzutu, a noce, czynniki, sypialnia, dziennik
     i dziennik wybudzeń z nowych ścieżek (`dane/noce/RRRR.csv` …, wersja 1) przepisane na wiersze prototypu,
     oraz `konfiguracja.txt` ze zrzutu bajt w bajt, gdy jest (wysyłana przed `K`, jak `wgraj_pliki.py --konfiguracja`).
     0 nocy = błąd, nigdy sukces.
  3. Wgranie kablem firmware z nową tablicą partycji (skrypt wypisuje komendę i czeka na Enter).
  4. Restart panelu przez DTR/RTS (domyślnie; --bez-resetu wyłącza) i zapytanie `?` o okno przywracania (U3: bez
     zależności od wiersza wypisanego przed otwarciem portu), potem pliki komendą `W` (wgraj_pliki.py); `K` tylko
     przy komplecie `OK`. Po pierwszej komendzie `W` okno wraca przy każdym starcie zasilania, dopóki przywrócenie
     po `K` przy komplecie się nie uda. Gdy pierwsze okno po wgraniu minęło bez
     plików (znacznik „bez przywracania”), skrypt przywraca okno komendą `W` poza oknem i restartuje panel.
  5. Odczyt wyniku z portu: `przywrócono {n} z {m}` (n = liczba nocy w zrzucie) albo `niezgodna liczba`.

Użycie (pyserial jest w środowisku PlatformIO):
  ~/.platformio/penv/bin/python tools/prototyp/przejscie_32mb.py /dev/cu.usbserial-110 <katalog poza repo>
Opcje: --bez-zrzutu (zrzut-a i zrzut-b już są w katalogu), --bez-resetu (krok 4 bez restartu panelu), --symulacja
(bez panelu: kroki 1, 3 i 5 na atrapie portu, która sprawdza komendy `W` jak panel; do testu skryptu,
tools/test_przejscie_32mb.py).
Test tego samego przejścia na symulatorze panelu (C++): CPAP_ZRZUT=<katalog>/zrzut-a bash tools/prototyp/testy.sh
"""
import argparse
import glob
import hashlib
import os
import shutil
import subprocess
import sys

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(TU))
sys.path.insert(0, TU)
import wgraj_pliki  # noqa: E402

PLIKI_ZRZUTU = wgraj_pliki.PLIKI_ZRZUTU
KOMENDA_KABEL = "cd firmware/prototyp && pio run -e e1001 -t upload"


def skrot(sciezka):
    with open(sciezka, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def pliki_zrzutu(katalog):
    """Ścieżki względne wszystkich plików zrzutu (bez kopii `.poprzedni` z zrzut_plikow.py i bez kopii konfiguracji
    `konfiguracja.txt.*`: poprzednie wersje z datą i nieudane odczyty), posortowane."""
    v = []
    for kat, _, nazwy in os.walk(katalog):
        for n in nazwy:
            if n.endswith(".poprzedni") or n.startswith(wgraj_pliki.NAZWA_KONFIGURACJI + "."):
                continue
            v.append(os.path.relpath(os.path.join(kat, n), katalog))
    return sorted(v)


def ma_noce(katalog):
    """Zrzut ma plik nocy: noce.csv (prototyp) albo dane/noce/RRRR.csv (firmware)."""
    return os.path.isfile(os.path.join(katalog, "noce.csv")) or bool(glob.glob(os.path.join(katalog, "dane", "noce",
                                                                                              "[0-9]" * 4 + ".csv")))


def sprawdz_zrzuty(katalog_a, katalog_b):
    """Problemy dwóch zrzutów (pusta lista = zgodne i kompletne): różny skrót, plik tylko w jednym, brak pliku nocy."""
    problemy = []
    for k in (katalog_a, katalog_b):
        if not ma_noce(k):
            problemy.append("brak noce.csv (ani dane/noce/RRRR.csv) w zrzucie " + k)
    if problemy:
        return problemy
    pa, pb = set(pliki_zrzutu(katalog_a)), set(pliki_zrzutu(katalog_b))
    for nazwa in sorted(pa | pb):
        if (nazwa in pa) != (nazwa in pb):
            problemy.append("%s tylko w zrzucie %s" % (nazwa, katalog_a if nazwa in pa else katalog_b))
        elif skrot(os.path.join(katalog_a, nazwa)) != skrot(os.path.join(katalog_b, nazwa)):
            problemy.append("%s: różne skróty SHA-256 w obu zrzutach" % nazwa)
    return problemy


def liczba_nocy(katalog):
    """Niepuste wiersze noce.csv zrzutu (z tym panel porównuje przywrócenie, T-WYM-62)."""
    with open(os.path.join(katalog, "noce.csv"), "rb") as f:
        return sum(1 for w in f.read().split(b"\n") if w.strip(b"\r"))


# Pliki magazynu (src/magazyn.cpp rejestrFormatow, wersja 1): format, wzór ścieżki w zrzucie, plik prototypu,
# przepisywany (z `#koniec {n}`), liczba pól wersji 1 przenoszonych do wiersza prototypu (noce: 20 bez `import`).
POLA_V1 = {
    "noce": "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;"
            "ca;h;csr;import",
    "czynniki": "data;stan;kody;warstwa;termin;pokazano",
    "sypialnia": "data;temp;wilg;co2",
    "humor": "data;typ;klucz;tryb;scena;zdanie",
    "wybudzenia": "epoch;mV;wifi_ms;aktywny_ms;powod",
}
NOWE_SCIEZKI = (
    ("noce", os.path.join("dane", "noce", "[0-9]" * 4 + ".csv"), "noce.csv", True, 20),
    ("czynniki", os.path.join("dane", "czynniki.csv"), "czynniki.csv", True, 6),
    ("sypialnia", os.path.join("dane", "sypialnia", "[0-9]" * 4 + ".csv"), "sypialnia.csv", True, 4),
    ("humor", os.path.join("dane", "humor.csv"), "humor.csv", False, 6),
    ("wybudzenia", os.path.join("log", "wybudzenia.csv"), "bateria.csv", False, 5),
)


def _wiersze(sciezka):
    with open(sciezka, "rb") as f:
        t = f.read().decode("utf-8")
    return [w.rstrip("\r") for w in t.split("\n") if w.strip("\r")]


def wiersze_wersji_1(sciezka, nazwa, przepisywany, pol):
    """Wiersze danych pliku magazynu (nagłówek `#cpap-eink {nazwa} {n}`, nazwy pól, przy przepisywanych `#koniec`)
    jako wiersze prototypu: pierwsze `pol` pól. ValueError przy nagłówku, nazwach pól albo `#koniec` niezgodnych."""
    w = _wiersze(sciezka)
    if len(w) < 2:
        raise ValueError("%s: brak nagłówka" % sciezka)
    p = w[0].split(" ")
    if len(p) != 3 or p[0] != "#cpap-eink" or p[1] != nazwa or not p[2].isdigit() or int(p[2]) < 1:
        raise ValueError("%s: nagłówek `%s`" % (sciezka, w[0]))
    znane = POLA_V1[nazwa]
    if not (w[1] == znane or (int(p[2]) > 1 and w[1].startswith(znane + ";"))):
        raise ValueError("%s: nazwy pól `%s` inne niż w wersji 1" % (sciezka, w[1]))
    dane = w[2:]
    if przepisywany:
        if not dane or not dane[-1].startswith("#koniec ") or dane[-1][8:] != str(len(dane) - 1):
            raise ValueError("%s: bez poprawnego `#koniec` (plik uszkodzony)" % sciezka)
        dane = dane[:-1]
    return [";".join(x.split(";")[:pol]) for x in dane]


def _klucz(w):
    k = w.split(";", 1)[0]
    return (len(k), k)


def _scal(stare, nowe):
    """Wiersze po kluczu (pierwsze pole): klucz z `nowe` zastępuje ten sam klucz ze `stare`; wynik po kluczu."""
    grupy = {}
    for w in stare:
        grupy.setdefault(_klucz(w), ([], []))[0].append(w)
    for w in nowe:
        grupy.setdefault(_klucz(w), ([], []))[1].append(w)
    return [x for k in sorted(grupy) for x in (grupy[k][1] or grupy[k][0])]


def pliki_do_wgrania(zrzut, cel):
    """Pliki prototypu do komendy `W` w katalogu `cel` ze zrzutu `zrzut`; zwraca liczbę nocy w `cel/noce.csv`.

    Zrzut z prototypu: pliki skopiowane bajt w bajt. Zrzut z firmware (U10): wiersze z `dane/noce/RRRR.csv`,
    `dane/czynniki.csv`, `dane/sypialnia/RRRR.csv`, `dane/humor.csv`, `log/wybudzenia.csv` przepisane na wiersze
    prototypu i scalone z plikami prototypu (ta sama data: nowa ścieżka). ValueError przy pliku nieczytelnym."""
    if os.path.isdir(cel):
        shutil.rmtree(cel)
    os.makedirs(cel)
    nowe = {}
    for nazwa, wzor, v0, przepisywany, pol in NOWE_SCIEZKI:
        for s in sorted(glob.glob(os.path.join(zrzut, wzor))):
            nowe.setdefault(v0, []).extend(wiersze_wersji_1(s, nazwa, przepisywany, pol))
    for v0 in PLIKI_ZRZUTU:
        z = os.path.join(zrzut, v0)
        if v0 not in nowe and v0 != "sypialnia-co2.csv":
            if os.path.isfile(z):
                shutil.copyfile(z, os.path.join(cel, v0))
            continue
        stare = _wiersze(z) if os.path.isfile(z) else []
        if v0 == "sypialnia-co2.csv":  # CO2 z plików rocznych sypialni ma pierwszeństwo przed starym plikiem CO2
            daty = {_klucz(w) for w in nowe.get("sypialnia.csv", [])}
            stare = [w for w in stare if _klucz(w) not in daty]
            if not stare and not os.path.isfile(z):
                continue
            wiersze = stare
        else:
            wiersze = _scal(stare, nowe[v0])
        with open(os.path.join(cel, v0), "wb") as f:
            f.write("".join(w + "\n" for w in wiersze).encode("utf-8"))
    konf = os.path.join(zrzut, wgraj_pliki.NAZWA_KONFIGURACJI)
    if os.path.isfile(konf) and not os.path.islink(konf):  # ustawienia panelu bajt w bajt, wgraj() wysyła je przed `K`
        shutil.copyfile(konf, os.path.join(cel, wgraj_pliki.NAZWA_KONFIGURACJI))
    return liczba_nocy(cel) if os.path.isfile(os.path.join(cel, "noce.csv")) else 0


class AtrapaPanelu:
    """Port panelu w oknie przywracania (tryb --symulacja): sprawdza komendy `W` jak src/magazyn.cpp sprawdzPlikW(),
    po `K` odpowiada wynikiem przywrócenia z liczbą niepustych wierszy przyjętego pliku nocy."""

    WIERSZ_OKNA = "okno przywracania do 07:10 (T-WYM-87): port serwisowy, komenda W, K kończy; bez Wi-Fi\n".encode()

    def __init__(self):
        self.pliki = {}
        self.konfiguracja = None
        self.wyjscie = [self.WIERSZ_OKNA]
        self._bufor = b""

    def write(self, b):
        self._bufor += b
        while self._bufor:
            if self._bufor.startswith(b"?"):  # U3: zapytanie o stan okna
                self._bufor = self._bufor[1:]
                self.wyjscie.append(self.WIERSZ_OKNA)
                continue
            if self._bufor.startswith(b"K"):
                self._bufor = self._bufor[1:]
                n = sum(1 for w in self.pliki.get("noce.csv", b"").split(b"\n") if w.strip(b"\r"))
                self.wyjscie.append(("przywrócono %d z %d nocy z /przywracanie/\n" % (n, n)).encode())
                continue
            if b"\n" not in self._bufor:
                break
            naglowek, reszta = self._bufor.split(b"\n", 1)
            p = naglowek.decode().split()
            nazwa, rozmiar, sha = p[1], int(p[2]), p[3]
            if len(reszta) < rozmiar:
                break
            tresc, self._bufor = reszta[:rozmiar], reszta[rozmiar:]
            if nazwa == wgraj_pliki.NAZWA_KONFIGURACJI and hashlib.sha256(tresc).hexdigest() == sha:
                self.konfiguracja = tresc  # jak panel: zapis wprost do /konfiguracja.txt, poza plikami przywracania
                self.wyjscie.append(("OK %s\n" % nazwa).encode())
            elif nazwa not in PLIKI_ZRZUTU:
                self.wyjscie.append(("BLAD %s nazwa\n" % nazwa).encode())
            elif hashlib.sha256(tresc).hexdigest() != sha:
                self.wyjscie.append(("BLAD %s skrot\n" % nazwa).encode())
            else:
                self.pliki[nazwa] = tresc
                self.wyjscie.append(("OK %s\n" % nazwa).encode())
        return len(b)

    def readline(self):
        return self.wyjscie.pop(0) if self.wyjscie else b""


def zrob_zrzut(port, katalog):
    """Zrzut przez istniejący zrzut_plikow.py (z restartem panelu, okno czuwania po rysowaniu)."""
    r = subprocess.run([sys.executable, os.path.join(TU, "zrzut_plikow.py"), port, katalog, "--reset"])
    return r.returncode == 0


def main(argv=None, wejscie=input):
    a = argparse.ArgumentParser(description="Przejście egzemplarza na układ partycji 32 MB (spec 8.9).")
    a.add_argument("port", help="port szeregowy panelu (w trybie --symulacja dowolny tekst)")
    a.add_argument("katalog", help="katalog danych poza repozytorium (WY12): zrzut-a/, zrzut-b/")
    a.add_argument("--bez-zrzutu", action="store_true", help="zrzut-a i zrzut-b już są w katalogu")
    a.add_argument("--bez-resetu", action="store_true", help="krok 4 bez restartu panelu przez DTR/RTS")
    a.add_argument("--symulacja", action="store_true", help="bez panelu: atrapa portu w oknie przywracania")
    a = a.parse_args(argv)
    kat = os.path.abspath(a.katalog)
    if kat == REPO or kat.startswith(REPO + os.sep):
        print("katalog danych musi być poza repozytorium (WY12)")
        return 2
    za, zb = os.path.join(kat, "zrzut-a"), os.path.join(kat, "zrzut-b")
    # krok 1: zrzut dwa razy
    if not a.bez_zrzutu and not a.symulacja:
        for k in (za, zb):
            print("krok 1: zrzut do " + k, flush=True)
            if not zrob_zrzut(a.port, k):
                print("zrzut nieudany: obudź panel przyciskiem i uruchom ponownie")
                return 1
    # krok 2: porównanie skrótów i kompletność
    problemy = sprawdz_zrzuty(za, zb)
    if problemy:
        print("krok 2: zrzuty niezgodne albo niepełne, bez wgrywania:")
        for p in problemy:
            print("  - " + p)
        return 1
    dw = os.path.join(kat, "do-wgrania")
    try:
        n = pliki_do_wgrania(za, dw)
    except (ValueError, UnicodeDecodeError) as e:
        print("krok 2: zrzut nieczytelny, bez wgrywania: %s" % e)
        return 1
    if n == 0:
        print("krok 2: zrzut bez nocy (0 wierszy nocy w noce.csv i dane/noce/), bez wgrywania; zrób zrzut ponownie")
        return 1
    obecne = [x for x in PLIKI_ZRZUTU if os.path.isfile(os.path.join(dw, x))]
    print("krok 2: zrzuty zgodne, plików do wgrania %d z %d (%s), nocy %d" % (len(obecne), len(PLIKI_ZRZUTU), dw, n))
    # krok 3: wgranie kablem nowej tablicy
    print("krok 3: wgraj kablem firmware z tablicą partycji 32 MB (firmware/partycje-e1001.csv):")
    print("  " + KOMENDA_KABEL)
    print("  (przez Wi-Fi się nie da: e1001_ota jest zablokowane do przejścia, karta P03 R7)")
    if a.symulacja:
        port = AtrapaPanelu()
    else:
        wejscie("po wgraniu i restarcie panelu naciśnij Enter… ")
        import serial  # leniwie: testy bez pyserial

        port = serial.Serial(a.port, 115200, timeout=0.5)
        if not a.bez_resetu:
            wgraj_pliki.resetuj(port)  # U3: start zasilania otwiera okno, dopóki przywracanie czeka
    # krok 4: okno przywracania i komenda W
    log = lambda t: print("  panel: " + t, flush=True)  # noqa: E731
    stan = wgraj_pliki.czekaj_na_okno(port, 180, log)
    if stan == "bez przywracania":  # pierwsze okno po wgraniu minęło bez plików (np. Enter po ponad 10 min)
        print("krok 4: okno minęło bez plików; przywracam je komendą W poza oknem (gdy panel milczy, naciśnij "
              "przycisk panelu)", flush=True)
        if not wgraj_pliki.przywroc_okno(port, 180, log):
            print("krok 4: panel nie odpowiada na komendę W poza oknem; uruchom ponownie z --bez-zrzutu")
            return 1
        wgraj_pliki.resetuj(port)
        stan = wgraj_pliki.czekaj_na_okno(port, 180, log)
    if stan in ("poza oknem", "bez przywracania"):
        print("krok 4: panel odpowiada `przywracanie: poza oknem` (przywrócenie już udane?); sprawdź U-05 "
              "(wiersz Pamięć panelu)")
        return 1
    if stan is None:
        print("krok 4: panel nie odpowiada na `?`; wysyłam pliki mimo to (poza oknem panel odpowie `poza oknem`)")
    wynik = wgraj_pliki.wgraj(port, dw, zakoncz=True, log=log)
    print("krok 4: " + ", ".join("%s %s" % kv for kv in wynik.items()))
    konf = wynik.get(wgraj_pliki.NAZWA_KONFIGURACJI)
    if not wgraj_pliki.pliki_nocy_ok(wynik) or konf == wgraj_pliki.BRAK_ODPOWIEDZI:
        print("nie wszystkie pliki przyjęte, K nie wysłane. Po próbie wgrania okno wraca przy każdym starcie zasilania, "
              "dopóki przywrócenie po K przy komplecie się nie uda; powtórz krok 4 (restart panelu przez DTR/RTS):")
        print("  tools/prototyp/wgraj_pliki.py %s %s --reset" % (a.port, dw))
        if any(w in ("rozmiar", "brak odpowiedzi") for w in wynik.values()):
            print("  (port mógł się rozsynchronizować: uruchom ponownie z --reset, nie wysyłaj dalej w tym oknie)")
        return 1
    # krok 5: wynik przywrócenia z logu
    t = wgraj_pliki.czekaj_na_wiersz(port, ["przywrócono", "niezgodna liczba"], 60, log)
    oczekiwany = "przywrócono %d z %d" % (n, n)
    if konf not in (None, "OK"):  # noce idą dalej (`K` wysłane); ustawienia panelu do wysłania osobno
        print("krok 4: konfiguracja nie przyjęta (%s); po przejściu wyślij ją: tools/prototyp/wgraj_pliki.py %s "
              "--konfiguracja %s" % (konf, a.port, os.path.join(dw, wgraj_pliki.NAZWA_KONFIGURACJI)))
    if t and oczekiwany in t:
        print("krok 5: %s; sprawdź U-05 (Pamięć panelu: przywrócono %d…) i ekran dnia" % (oczekiwany, n))
        return 0 if konf in (None, "OK") else 1
    print("krok 5: przywrócenie nieudane: %s (oczekiwane `%s`); zrzut zostaje w %s, okno wróci przy następnym "
          "starcie zasilania" % (t or "brak wiersza", oczekiwany, za))
    return 1


if __name__ == "__main__":
    sys.exit(main())
