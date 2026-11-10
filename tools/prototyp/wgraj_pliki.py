#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Wgranie zrzutu plików do panelu przez port serwisowy, komendą `W` (odwrotność zrzut_plikow.py).

Spec techniczna 8.9 krok 4, T-WYM-87. Panel przyjmuje pliki tylko
w oknie przywracania: 10 min po starcie zasilania (włączenie, restart przez DTR/RTS z --reset; nie po restarcie
z aktualizacji ani po błędzie programu) i po pierwszym formatowaniu partycji plików, dopóki przywracanie czeka. Przywracanie kończy się tylko po `K` przy
komplecie plików i udanym przywróceniu; okno bez żadnej komendy `W` (panel bez zrzutu) zamyka je znacznikiem „bez
przywracania”, który zdejmuje komenda `W` poza oknem (ten skrypt robi to sam, przywroc_okno()). Na U-05 (test
sprzętu) jest wtedy w ramce `Okno przywracania do {hh:mm}`, a w logu portu `okno przywracania do {hh:mm} (T-WYM-87) …`.

Protokół (R8): wiersz `W {nazwa} {rozmiar} {sha256}\\n`, potem dokładnie {rozmiar} bajtów treści; panel odpowiada
`OK {nazwa}` albo `BLAD {nazwa} {powód}` (skrot, rozmiar, nazwa, poza oknem, zapis). Znak `K` kończy okno przed
czasem; potem panel przywraca dane (T-WYM-62) i pisze w logu `przywrócono {n} z {m}` albo `niezgodna liczba`.
Znak `?` to zapytanie o stan: w oknie panel odpowiada wierszem okna (`okno przywracania do {hh:mm} …` albo
`okno przywracania: {m} min …`), poza oknem w czuwaniu `przywracanie: poza oknem`, a przy
znaczniku „bez przywracania” `przywracanie: poza oknem, bez przywracania …`.
Po `BLAD` skrypt ponawia plik raz (skrót, zapis) i nie wysyła `K`, dopóki któryś plik nie ma `OK`: okno trwa, a panel
bez `K` przy komplecie nie stawia znacznika udanego przywrócenia, więc okno wróci przy następnym starcie zasilania.
Pusty plik nocy (0 nocy) skrypt odrzuca bez wysyłania, a bez pliku nocy nie wysyła `K` (bez niego nie ma
kompletu).

Użycie (pyserial jest w środowisku PlatformIO):
  ~/.platformio/penv/bin/python tools/prototyp/wgraj_pliki.py /dev/cu.usbserial-110 <katalog zrzutu poza repo>
  ~/.platformio/penv/bin/python tools/prototyp/wgraj_pliki.py /dev/cu.usbserial-110 --obrazki <wyjście przygotuj.py>
  ~/.platformio/penv/bin/python tools/prototyp/wgraj_pliki.py /dev/cu.usbserial-110 --konfiguracja <konfiguracja.txt>

Obrazki dnia (spec techniczna 8.6): `--obrazki <katalog>` wysyła komendą `W` pliki `*.pbm` i na końcu
`obrazki.json` z wyjścia tools/obrazki/przygotuj.py pod nazwami `obrazki/{plik}`; panel zapisuje je do `/obrazki/`
atomowo, w oknie przywracania i poza nim, bez ruszania danych nocy (zamiast `uploadfs`, który kasuje całą pamięć).
Nazwy tylko `[a-z0-9_-]{1,40}.pbm` i `obrazki.json`, plik do 64 KB (src/magazyn.h MAKS_OBRAZKA_W); inne pliki dostają
wynik "nazwa", "rozmiar" albo "dowiazanie" i nie są wysyłane. Panel słucha portu po starcie i przez 20 s po naciśnięciu przycisku
albo po komendzie `?`/`W`, więc skrypt najpierw pyta `?` (naciśnij dowolny przycisk panelu, jeśli milczy) i nie wysyła `K`.
Po `brak odpowiedzi` skrypt przerywa: panel zasnął, naciśnij przycisk panelu i uruchom ponownie.
Zrzut z `konfiguracja.txt` (zrzut_plikow.py zapisuje ją w korzeniu katalogu zrzutu): przy przywracaniu z katalogu
skrypt wysyła ją po plikach zrzutu, a przed `K`, tą samą drogą co `--konfiguracja`; zrzut bez niej (starszy) przechodzi
jak dotąd, a jej odrzucenie nie wstrzymuje `K` (skrypt kończy się wtedy błędem z poleceniem `--konfiguracja`).
Konfiguracja (spec techniczna 8.6): `--konfiguracja <plik>` wysyła komendą `W` jeden plik pod nazwą
`konfiguracja.txt` (panel zapisuje go atomowo do `/konfiguracja.txt`, w oknie i poza nim, bez ruszania nocy, NVS i
`/przywracanie/`), zamiast `uploadfs`. Plik do 16 KB (src/magazyn.h MAKS_KONFIGURACJI_W), tekst bez bajtu 0, nazwa pliku
na dysku dowolna (po stronie panelu zawsze `konfiguracja.txt`). Protokół i komunikaty jak przy `--obrazki`
(najpierw `?`, bez `K`, po `brak odpowiedzi` stop).
Opcje: --bez-czekania (nie pytaj panelu o okno), --bez-k (nie kończ okna), --reset (restart panelu przed pytaniem:
okno otwiera się po starcie zasilania, dopóki przywracanie czeka). pyserial jest importowany dopiero w main(), więc moduł działa
w testach bez niego (tools/test_wgraj_pliki.py).
"""
import argparse
import hashlib
import json
import os
import re
import sys
import time

# Pliki zrzutu prototypu (8.9 krok 1), ta sama lista co src/magazyn.cpp plikiZrzutu().
PLIKI_ZRZUTU = ("noce.csv", "czynniki.csv", "humor.csv", "sypialnia.csv", "sypialnia-co2.csv", "bateria.csv")
MAKS_PLIKU = 4 * 1024 * 1024  # [PRÓG] R8, src/magazyn.h MAKS_PLIKU_W
ZNACZNIK_OKNA = "okno przywracania"
# Wiersz otwartego okna (log przy otwarciu i odpowiedź na `?`); nie łapie `okno przywracania: koniec czasu` itp.
WZOR_OKNA = re.compile(r"okno przywracania( do \d{1,2}:\d{2}|: \d+ min)")
POZA_OKNEM = "przywracanie: poza oknem"
BEZ_PRZYWRACANIA = "bez przywracania"  # dopisek odpowiedzi na `?` przy znaczniku „bez przywracania”
PONAWIANE = ("skrot", "zapis")  # powody, po których plik idzie jeszcze raz (zakłócenie na kablu, chwilowy błąd zapisu)
BRAK_ODPOWIEDZI = "brak odpowiedzi"  # panel zasnął albo kabel odpadł: dalsze bajty przerwane
ZASNAL = "panel zasnął: naciśnij przycisk panelu i uruchom ponownie"
# obrazki dnia komendą `W` (src/magazyn.h nazwaObrazkaW, MAKS_OBRAZKA_W)
PREFIKS_OBRAZKOW = "obrazki/"
MANIFEST_OBRAZKOW = "obrazki.json"
NAZWA_OBRAZKA = re.compile(r"[a-z0-9_-]{1,40}\.pbm\Z")
MAKS_OBRAZKA = 64 * 1024
# /konfiguracja.txt komendą `W` (src/magazyn.h NAZWA_KONFIGURACJI_W, MAKS_KONFIGURACJI_W)
NAZWA_KONFIGURACJI = "konfiguracja.txt"
MAKS_KONFIGURACJI = 16 * 1024
# wiersze, których panel nie przyjmuje w konfiguracji (src/magazyn.cpp sprawdzKonfiguracjeW(), powód "znacznik"):
# znaczniki zrzutu `Z`, w którym plik wraca
ZNACZNIKI_ZRZUTU = ("PLIK_KONIEC", "PLIKI_KONIEC", "PLIKI_START")
OPISY_POWODOW = {
    "znacznik": "wiersz równy znacznikowi zrzutu (PLIK_KONIEC, PLIKI_KONIEC, PLIKI_START albo zaczynający się "
                "od „PLIK ”): usuń go albo zmień",
    "tresc": "bajt 0 w pliku: to nie jest plik tekstowy",
    "rozmiar": "plik ponad 16 KB albo ucięty w drodze",
    "skrot": "treść zmieniona w drodze (zakłócenie na kablu)",
}


def ma_znacznik(tresc):
    """True, gdy któryś wiersz `tresc` (bytes, bez `\r` na końcu) to znacznik zrzutu albo zaczyna się od `PLIK `."""
    for w in tresc.split(b"\n"):
        w = w[:-1] if w.endswith(b"\r") else w
        if w.decode("utf-8", "replace") in ZNACZNIKI_ZRZUTU or w.startswith(b"PLIK "):
            return True
    return False


def komenda(nazwa, tresc):
    """Bajty komendy `W` dla pliku `nazwa` o treści `tresc` (bytes)."""
    return ("W %s %d %s\n" % (nazwa, len(tresc), hashlib.sha256(tresc).hexdigest())).encode("ascii") + tresc


def _odpowiedz(port, nazwa, limit_s, log=None):
    """Wiersz `OK {nazwa}` albo `BLAD {nazwa} …` z portu: "OK", powód albo "brak odpowiedzi" po limit_s sekundach."""
    koniec = time.monotonic() + limit_s
    while time.monotonic() < koniec:
        w = port.readline()
        if not w:
            continue
        t = w.decode("utf-8", "replace").rstrip("\r\n")
        if t == "OK " + nazwa:
            return "OK"
        if t.startswith("BLAD " + nazwa + " "):
            return t[len("BLAD " + nazwa + " "):]
        if log:
            log(t)
    return BRAK_ODPOWIEDZI


def wgraj(port, katalog, zakoncz=True, log=None, ponowien=1):
    """Wysyła komendą `W` każdy plik z PLIKI_ZRZUTU obecny w `katalog`; wynik: nazwa → "OK" albo powód.

    `port` ma write(bytes) i readline() → bytes (pusty przy braku danych w limicie portu). Po plikach zrzutu, a przed
    `K`, wysyła `konfiguracja.txt` z katalogu, jeśli jest (zrzut_plikow.py zapisuje ją tam), przez wgraj_konfiguracje()
    (wynik pod kluczem "konfiguracja.txt"); brak pliku nic nie zmienia (starsze zrzuty go nie mają), a jej odrzucenie
    nie wstrzymuje `K` (pliki_nocy_ok()). Plik ponad 4 MB nie jest
    wysyłany (wynik "rozmiar"). Plik nocy bez żadnej nocy nie jest wysyłany, a gdy
    pliku nocy w katalogu nie ma, wynik "brak pliku" (bez niego panel nie ma kompletu). Plik z `BLAD … skrot` albo
    `BLAD … zapis` idzie jeszcze `ponowien` razy. Przy zakoncz=True na końcu wysyła b"K" (koniec okna, potem
    przywrócenie na panelu), ale tylko gdy każdy plik ma "OK" (po `BLAD` okno trwa, żeby plik
    dało się wysłać jeszcze raz; panel stawia znacznik tylko po `K` przy komplecie).
    """
    wynik = {}
    for nazwa in PLIKI_ZRZUTU:
        sciezka = os.path.join(katalog, nazwa)
        if not os.path.isfile(sciezka):
            if nazwa == "noce.csv" and any(os.path.isfile(os.path.join(katalog, n)) for n in PLIKI_ZRZUTU):
                wynik[nazwa] = "brak pliku"
            continue
        with open(sciezka, "rb") as f:
            tresc = f.read()
        if len(tresc) > MAKS_PLIKU:
            wynik[nazwa] = "rozmiar"
            continue
        if nazwa == "noce.csv" and not any(w.strip(b"\r") for w in tresc.split(b"\n")):
            wynik[nazwa] = "pusty"
            continue
        for proba in range(ponowien + 1):
            port.write(komenda(nazwa, tresc))
            # 115 200 b/s ≈ 11 KB/s; zapas na zapis atomowy w panelu
            wynik[nazwa] = _odpowiedz(port, nazwa, 30 + len(tresc) / 5000.0, log)
            if wynik[nazwa] not in PONAWIANE:
                break
            if log and proba < ponowien:
                log("%s: BLAD %s, wysyłam jeszcze raz" % (nazwa, wynik[nazwa]))
        if wynik[nazwa] == BRAK_ODPOWIEDZI:  # nic więcej do portu; panel wybudzony czytałby treść jako komendy
            break
    konf = os.path.join(katalog, NAZWA_KONFIGURACJI)
    if BRAK_ODPOWIEDZI not in wynik.values() and (os.path.isfile(konf) or os.path.islink(konf)):
        # przed `K`: po `K` panel przywraca dane i zasypia; ta sama droga co --konfiguracja (w oknie i poza nim)
        wynik[NAZWA_KONFIGURACJI] = wgraj_konfiguracje(port, konf, log=log, ponowien=ponowien)[NAZWA_KONFIGURACJI]
    if zakoncz and pliki_nocy_ok(wynik) and BRAK_ODPOWIEDZI not in wynik.values():
        port.write(b"K")
    return wynik


def pliki_nocy_ok(wynik):
    """True, gdy wynik wgraj() ma co najmniej jeden plik zrzutu i każdy plik zrzutu ma "OK" (wtedy idzie `K`).
    Wynik konfiguracji się nie liczy: panel przyjmuje ją także poza oknem, a przywrócenie nocy od niej nie zależy."""
    noce = [w for n, w in wynik.items() if n in PLIKI_ZRZUTU]
    return bool(noce) and all(w == "OK" for w in noce)


def wgraj_obrazki(port, katalog, log=None, ponowien=1):
    """Wysyła komendą `W` obrazki dnia z `katalog` (wyjście tools/obrazki/przygotuj.py): najpierw pliki `.pbm` po nazwie,
    na końcu manifest `obrazki.json`, pod nazwami `obrazki/{plik}`; wynik: plik → "OK" albo powód.

    Plik o nazwie spoza `[a-z0-9_-]{1,40}.pbm` i `obrazki.json` nie jest wysyłany (wynik "nazwa"), plik ponad 64 KB
    też nie ("rozmiar"), dowiązanie symboliczne i plik spoza katalogu po rozwiązaniu ścieżki też nie
    ("dowiazanie"); pliki ukryte (`.DS_Store`) są pomijane. Po pierwszym "brak odpowiedzi" (panel zasnął) nic więcej nie idzie
    do portu. Bez `K`: obrazki nie kończą okna przywracania. Manifest bez pozycji dla wysyłanych plików:
    ostrzeżenie przez `log` (panel ich nie pokaże, w logu panelu `obrazki: manifest bez pozycji z plikiem`).
    """
    wynik = {}
    kat = os.path.realpath(katalog)
    nazwy = sorted(n for n in os.listdir(katalog) if not n.startswith(".") and os.path.isfile(os.path.join(katalog, n)))
    nazwy = [n for n in nazwy if n != MANIFEST_OBRAZKOW] + [n for n in nazwy if n == MANIFEST_OBRAZKOW]
    for n in nazwy:
        sciezka = os.path.join(katalog, n)
        if os.path.islink(sciezka) or os.path.dirname(os.path.realpath(sciezka)) != kat:
            wynik[n] = "dowiazanie"
            continue
        if n != MANIFEST_OBRAZKOW and not NAZWA_OBRAZKA.match(n):
            wynik[n] = "nazwa"
            continue
        with open(sciezka, "rb") as f:
            tresc = f.read()
        if len(tresc) > MAKS_OBRAZKA:
            wynik[n] = "rozmiar"
            continue
        if n == MANIFEST_OBRAZKOW and log:
            brakujace = pliki_bez_pozycji(tresc, [x for x, w in wynik.items() if w == "OK"])
            if brakujace:
                log("UWAGA: manifest nie ma pozycji dla %d plików (np. %s): panel ich nie pokaże; zrób katalog jeszcze raz "
                    "przez przygotuj.py" % (len(brakujace), brakujace[0]))
        nazwa = PREFIKS_OBRAZKOW + n
        for proba in range(ponowien + 1):
            port.write(komenda(nazwa, tresc))
            wynik[n] = _odpowiedz(port, nazwa, 30 + len(tresc) / 5000.0, log)
            if wynik[n] not in PONAWIANE:
                break
            if log and proba < ponowien:
                log("%s: BLAD %s, wysyłam jeszcze raz" % (n, wynik[n]))
        if wynik[n] == BRAK_ODPOWIEDZI:
            break
    return wynik


def wgraj_konfiguracje(port, plik, log=None, ponowien=1):
    """Wysyła komendą `W` plik `plik` pod nazwą `konfiguracja.txt`; wynik: nazwa pliku → "OK" albo powód.

    Plik ponad 16 KB ("rozmiar"), z bajtem 0 ("tresc"), z wierszem-znacznikiem zrzutu ("znacznik", ma_znacznik();
    panel i tak by go odrzucił), dowiązanie symboliczne ("dowiazanie") albo nie-plik ("brak") nie jest wysyłany. Po "skrot" i "zapis" jedna ponowna próba, po "brak odpowiedzi" stop (jak wgraj_obrazki()).
    """
    n = os.path.basename(plik)
    if os.path.islink(plik):
        return {n: "dowiazanie"}
    if not os.path.isfile(plik):
        return {n: "brak"}
    with open(plik, "rb") as f:
        tresc = f.read()
    if len(tresc) > MAKS_KONFIGURACJI:
        return {n: "rozmiar"}
    if b"\0" in tresc:
        return {n: "tresc"}
    if ma_znacznik(tresc):
        return {n: "znacznik"}
    wynik = {}
    for proba in range(ponowien + 1):
        port.write(komenda(NAZWA_KONFIGURACJI, tresc))
        wynik[n] = _odpowiedz(port, NAZWA_KONFIGURACJI, 30 + len(tresc) / 5000.0, log)
        if wynik[n] not in PONAWIANE:
            break
        if log and proba < ponowien:
            log("%s: BLAD %s, wysyłam jeszcze raz" % (n, wynik[n]))
    return wynik


def pliki_bez_pozycji(manifest, pliki):
    """Pliki z `pliki`, których nie ma w pozycjach manifestu (bytes JSON); nieczytelny manifest: pusta lista (panel odpowie
    `BLAD … tresc`)."""
    try:
        pozycje = json.loads(manifest.decode("utf-8")).get("obrazki", [])
        w_manifescie = {str(p.get("plik", "")) for p in pozycje}
    except (ValueError, AttributeError):
        return []
    return [p for p in pliki if p not in w_manifescie]


def czekaj_na_okno(port, limit_s, log=None, pytaj_co_s=2.0):
    """Stan okna przywracania z odpowiedzi panelu na zapytanie `?` (wysyłane co `pytaj_co_s` s) albo z wiersza logu
    przy otwarciu okna: "otwarte", "poza oknem", "bez przywracania" (poza oknem ze znacznikiem
    „bez przywracania”) albo None po limit_s sekundach (panel milczy, np. jeszcze formatuje).
    Nie zależy od wiersza wypisanego, zanim skrypt otworzył port."""
    koniec = time.monotonic() + limit_s
    ostatnie = None
    while True:
        teraz = time.monotonic()
        if teraz >= koniec:
            return None
        if ostatnie is None or teraz - ostatnie >= pytaj_co_s:
            port.write(b"?")
            ostatnie = teraz
        w = port.readline()
        if not w:
            continue
        t = w.decode("utf-8", "replace").rstrip("\r\n")
        if log:
            log(t)
        if "narysowany" in t:
            try:
                port.narysowany = True  # dla czekaj_na_rysowanie()
            except AttributeError:
                pass
        if WZOR_OKNA.search(t):
            return "otwarte"
        if POZA_OKNEM in t:
            return "bez przywracania" if BEZ_PRZYWRACANIA in t else "poza oknem"


def przywroc_okno(port, limit_s, log=None, co_s=2.0):
    """Komenda `W` pustego pliku nocy poza oknem (co `co_s` s, do odpowiedzi `BLAD noce.csv poza oknem`): panel zdejmuje
    znacznik „bez przywracania” i okno wraca przy następnym starcie zasilania. Panel czyta port
    w jednym obiegu po starcie albo przez 20 s po naciśnięciu przycisku, więc komenda idzie kilka razy. True, gdy
    panel odpowiedział."""
    koniec = time.monotonic() + limit_s
    ostatnie = None
    while time.monotonic() < koniec:
        if ostatnie is None or time.monotonic() - ostatnie >= co_s:
            port.write(komenda("noce.csv", b""))
            ostatnie = time.monotonic()
        w = port.readline()
        if not w:
            continue
        t = w.decode("utf-8", "replace").rstrip("\r\n")
        if log:
            log(t)
        if t.startswith("BLAD noce.csv poza oknem"):
            return True
    return False


def czekaj_na_rysowanie(port, limit_s=20, log=None, pytaj_co_s=2.0):
    """Po restarcie panel odpowiada na `?` także przed rysowaniem ekranu, a w czasie rysowania (ok. 4 s) nie czyta
    portu: bajty pliku wysłane wtedy przepełniają bufor i plik dochodzi ucięty (wynik "rozmiar"). Wraca od razu, gdy
    wiersz `… narysowany …` był już w logu od restartu (`port.narysowany`, ustawia czekaj_na_okno()); inaczej czyta log
    do tego wiersza, najwyżej limit_s s, i co pytaj_co_s s wysyła `?`, bo zapytanie trzyma panel w czuwaniu (brak
    wiersza nie jest błędem). Zwraca True, gdy ekran jest narysowany."""
    if getattr(port, "narysowany", False):
        return True
    koniec = time.monotonic() + limit_s
    ostatnie = time.monotonic()
    while time.monotonic() < koniec:
        if time.monotonic() - ostatnie >= pytaj_co_s:
            port.write(b"?")
            ostatnie = time.monotonic()
        w = port.readline()
        if not w:
            continue
        t = w.decode("utf-8", "replace").rstrip("\r\n")
        if log:
            log(t)
        if "narysowany" in t:
            return True
    return False


def resetuj(port):
    """Restart panelu przez DTR/RTS (jak zrzut_plikow.py --reset): start zasilania otwiera okno, dopóki przywracanie
    czeka (U2 wariant A)."""
    try:
        port.narysowany = False  # nowy start: ekran jeszcze nie narysowany
    except AttributeError:
        pass
    port.dtr = False
    port.rts = True
    time.sleep(0.1)
    port.rts = False


def czekaj_na_wiersz(port, znaczniki, limit_s, log=None):
    """Pierwszy wiersz logu panelu zawierający któryś z `znaczniki` (albo None po limit_s sekundach)."""
    koniec = time.monotonic() + limit_s
    while time.monotonic() < koniec:
        w = port.readline()
        if not w:
            continue
        t = w.decode("utf-8", "replace").rstrip("\r\n")
        if log:
            log(t)
        if any(z in t for z in znaczniki):
            return t
    return None


def main(argv=None):
    a = argparse.ArgumentParser(description="Wgranie zrzutu do panelu komendą W (okno przywracania, T-WYM-87).")
    a.add_argument("port")
    a.add_argument("katalog", nargs="?", help="katalog zrzutu z zrzut_plikow.py (poza repozytorium)")
    a.add_argument("--obrazki", metavar="KATALOG",
                   help="zamiast zrzutu: obrazki dnia z wyjścia tools/obrazki/przygotuj.py do /obrazki/")
    a.add_argument("--konfiguracja", metavar="PLIK",
                   help="zamiast zrzutu: plik ustawień do /konfiguracja.txt")
    a.add_argument("--bez-czekania", action="store_true", help="nie czekaj na wiersz okna przywracania w logu")
    a.add_argument("--bez-k", action="store_true", help="nie wysyłaj K (okno trwa do końca czasu)")
    a.add_argument("--reset", action="store_true", help="zrestartuj panel przed czekaniem na okno")
    a.add_argument("--czas", type=int, default=180, help="ile sekund czekać na okno przywracania")
    a = a.parse_args(argv)
    if bool(a.katalog) + bool(a.obrazki) + bool(a.konfiguracja) != 1:
        sys.exit("podaj katalog zrzutu albo --obrazki <katalog> albo --konfiguracja <plik>, tylko jedno naraz")
    import serial  # leniwie: testy bez pyserial

    try:
        s = serial.Serial(a.port, 115200, timeout=0.5)
    except serial.SerialException:  # bez śladu stosu, inny numer portu to częsty przypadek
        sys.exit("nie mogę otworzyć portu %s: sprawdź kabel, a nazwę portu znajdź poleceniem ls /dev/cu.*" % a.port)
    if a.reset:
        resetuj(s)
    if a.obrazki:
        sys.exit(obrazki_main(s, a))
    if a.konfiguracja:
        sys.exit(konfiguracja_main(s, a))
    if not a.bez_czekania:
        print("pytam panel o okno przywracania…", flush=True)
        stan = czekaj_na_okno(s, a.czas, log=lambda t: print(t, flush=True))
        if stan == "bez przywracania":  # okno minęło bez komendy W; komenda W poza oknem je przywraca
            print("panel: okno przywracania minęło bez plików; przywracam okno (jeśli panel nie odpowiada, naciśnij "
                  "dowolny przycisk panelu)…", flush=True)
            if not przywroc_okno(s, a.czas, log=lambda t: print(t, flush=True)):
                sys.exit("panel nie odpowiedział na komendę W poza oknem; naciśnij przycisk panelu i uruchom ponownie")
            if not a.reset:
                sys.exit("okno wróci przy następnym starcie zasilania: uruchom skrypt ponownie z --reset")
            resetuj(s)
            stan = czekaj_na_okno(s, a.czas, log=lambda t: print(t, flush=True))
        if stan in ("poza oknem", "bez przywracania"):
            sys.exit("panel: okno przywracania zamknięte (przywrócenie już udane albo panel z historią bez przejścia); "
                     "nic nie wysyłam")
        if stan is None:
            print("nie widzę okna przywracania; wysyłam mimo to (poza oknem panel odpowie `BLAD … poza oknem`)")
        elif a.reset:
            czekaj_na_rysowanie(s, log=lambda t: print(t, flush=True))
    wynik = wgraj(s, a.katalog, zakoncz=not a.bez_k, log=lambda t: print(t, flush=True))
    for n, w in wynik.items():
        print("%-18s %s" % (n, w))
    if not wynik:
        sys.exit("w katalogu nie ma plików zrzutu: " + ", ".join(PLIKI_ZRZUTU + (NAZWA_KONFIGURACJI,)))
    if BRAK_ODPOWIEDZI in wynik.values():
        sys.exit(ZASNAL)
    noce = {n: w for n, w in wynik.items() if n in PLIKI_ZRZUTU}
    if any(w != "OK" for w in noce.values()):
        sys.exit("nie wszystkie pliki przyjęte, K nie wysłane; popraw plik (pusty albo brak pliku nocy: inny zrzut) "
                 "i uruchom skrypt jeszcze raz w tym oknie albo z --reset: po próbie wgrania okno wraca przy każdym "
                 "starcie zasilania, dopóki przywrócenie nie zakończy się komendą K przy komplecie plików")
    if noce and not a.bez_k:
        t = czekaj_na_wiersz(s, ["przywrócono", "niezgodna liczba"], 60, log=lambda t: print(t, flush=True))
        print("wynik przywrócenia: " + (t or "brak wiersza w logu w 60 s"))
    elif not noce:
        print("w katalogu bez plików zrzutu prototypu: wysłana tylko konfiguracja, K nie wysłane")
    konf = wynik.get(NAZWA_KONFIGURACJI)
    if konf not in (None, "OK"):
        sys.exit("konfiguracja nie przyjęta (%s); wyślij ją osobno (w oknie i poza nim): tools/prototyp/wgraj_pliki.py "
                 "%s --konfiguracja %s" % (konf, a.port, os.path.join(a.katalog, NAZWA_KONFIGURACJI)))


def obrazki_main(s, a):
    """--obrazki: panel musi słuchać portu (odpowiedź na `?`), potem wgraj_obrazki(); kod wyjścia jak sys.exit()."""
    if not os.path.isdir(a.obrazki):
        return "nie ma katalogu obrazków: " + a.obrazki
    print("pytam panel (naciśnij dowolny przycisk panelu, jeśli nic się nie dzieje)…", flush=True)
    stan = czekaj_na_okno(s, a.czas, log=lambda t: print(t, flush=True))
    if stan is None:
        return "panel nie odpowiada na `?`: naciśnij dowolny przycisk panelu i uruchom skrypt jeszcze raz"
    if getattr(a, "reset", False):
        czekaj_na_rysowanie(s, log=lambda t: print(t, flush=True))
    wynik = wgraj_obrazki(s, a.obrazki, log=lambda t: print(t, flush=True))
    for n, w in wynik.items():
        print("%-44s %s" % (n, w))
    if not wynik:
        return "w katalogu nie ma plików obrazków (*.pbm, obrazki.json)"
    if BRAK_ODPOWIEDZI in wynik.values():  # pliki są dobre, to panel przestał słuchać
        return ZASNAL
    zle = [n for n, w in wynik.items() if w != "OK"]
    if zle:
        return "nie wszystkie pliki przyjęte (%s); popraw je i uruchom skrypt jeszcze raz" % ", ".join(zle)
    print("obrazki wgrane: %d plików w /obrazki/; panel pokaże je przy następnym ekranie dnia S-K-38" % len(wynik))
    if stan == "otwarte":  # nowy panel; skrypt obrazków nie wysyła `K`
        print("okno przywracania otwarte: ekran testu zostaje do 10 min, dowolny przycisk panelu kończy okno")
    return None


def konfiguracja_main(s, a):
    """--konfiguracja: jak obrazki_main(), jeden plik; kod wyjścia jak sys.exit()."""
    if not os.path.isfile(a.konfiguracja):
        return "nie ma pliku konfiguracji: " + a.konfiguracja
    print("pytam panel (naciśnij dowolny przycisk panelu, jeśli nic się nie dzieje)…", flush=True)
    stan = czekaj_na_okno(s, a.czas, log=lambda t: print(t, flush=True))
    if stan is None:
        return "panel nie odpowiada na `?`: naciśnij dowolny przycisk panelu i uruchom skrypt jeszcze raz"
    if getattr(a, "reset", False):
        czekaj_na_rysowanie(s, log=lambda t: print(t, flush=True))
    wynik = wgraj_konfiguracje(s, a.konfiguracja, log=lambda t: print(t, flush=True))
    for n, w in wynik.items():
        print("%-44s %s" % (n, w))
    if BRAK_ODPOWIEDZI in wynik.values():
        return ZASNAL
    zle = ["%s: %s" % (w, OPISY_POWODOW[w]) if w in OPISY_POWODOW else w for w in wynik.values() if w != "OK"]
    if zle:
        return "plik nie przyjęty (%s); popraw go i uruchom skrypt jeszcze raz" % ", ".join(zle)
    print("konfiguracja wgrana: /konfiguracja.txt; panel czyta ją przy następnym wybudzeniu")
    if stan == "otwarte":
        print("okno przywracania otwarte: ekran testu zostaje do 10 min, dowolny przycisk panelu kończy okno")
    return None


if __name__ == "__main__":
    main()
