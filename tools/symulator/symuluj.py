#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Symulator doby panelu cpap-eink na zrzucie plików z panelu.

Kompiluje firmware/prototyp/test/symulator.cpp razem ze wszystkimi plikami firmware/prototyp/src/*.cpp (poza
adapterami ESP32 i main.cpp), uruchamia go na kopii zrzutu i zapisuje obrazki ekranów, które panel by narysował,
oraz log wybudzeń. Instrukcja krok po kroku: tools/symulator/README.md.

Użycie:
  python3 tools/symulator/symuluj.py --dane <katalog zrzutu> --od 2026-11-04T05:00 --do 2026-11-04T23:59 \
      [--przyciski plik] [--wyjscie katalog] [--bez-sieci] [--zegar-znany] [--miasto Katowice] [--prognoza plik.json] \
      [--bez-prognozy] [--obrazki katalog] [--scenariusz NAZWA …]
  python3 tools/symulator/symuluj.py --dane <katalog zrzutu> --pokaz-wszystko [--obrazki katalog]

Atrapa prognozy (domyślnie Katowice, plik prognoza-katowice-listopad.json), obrazki dnia, scenariusze
z nocami syntetycznymi (tools/symulator/scenariusze.py) i przegląd wszystkich scenariuszy (--pokaz-wszystko).
Wynik (domyślnie ~/cpap-eink-symulator/<okres>/): NNN-RRRRMMDD-HHMM-{ekran}.png, log.txt, log-panelu.txt, spis.txt.
Zrzut i wynik muszą leżeć poza repozytorium: to dane zdrowotne (WY12).
"""
import argparse
import concurrent.futures
import datetime as dt
import glob
import os
import platform
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scenariusze as scen  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FW = os.path.join(REPO, "firmware", "prototyp")
# build/ jest w .gitignore; osobny katalog na system i procesor (repo bywa widziane z Maca i z maszyny wirtualnej)
BUDOWA = os.path.join(REPO, "build", "symulator", f"{sys.platform}-{platform.machine()}")


def zrodla_rdzenia(fw=None):
    """Pliki src/*.cpp rdzenia: wszystkie poza main.cpp i *_esp32.cpp (tylko na panelu: Arduino, ESP-IDF). Ta sama
    reguła obejmuje listę ZRODLA z tools/prototyp/zrodla.sh (test w tools/test_symulator.py), więc nowy plik rdzenia
    wchodzi do symulatora bez dopisywania, a nowy adapter ESP32 go nie psuje."""
    return sorted(p for p in glob.glob(os.path.join(fw or FW, "src", "*.cpp"))
                  if os.path.basename(p) != "main.cpp" and not os.path.basename(p).endswith("_esp32.cpp"))
PRZYCISKI = {"lewy": "lewy", "◀": "lewy", "srodkowy": "srodkowy", "środkowy": "srodkowy", "↻": "srodkowy",
             "prawy": "prawy", "▶": "prawy"}
DLUGO = {"2s", "2 s", "dlugo", "długo"}
PROGNOZA_DOMYSLNA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "prognoza-katowice-listopad.json")
# Współrzędne miast do atrapy prognozy i S-K-37 (na panelu POGODA_LAT, POGODA_LON w secrets.h); inne: --wspolrzedne
MIASTA = {"katowice": (50.2597, 19.0211), "warszawa": (52.2297, 21.0122), "krakow": (50.0647, 19.9450),
          "kraków": (50.0647, 19.9450), "wroclaw": (51.1079, 17.0385), "wrocław": (51.1079, 17.0385),
          "poznan": (52.4064, 16.9252), "poznań": (52.4064, 16.9252), "gdansk": (54.3520, 18.6466),
          "gdańsk": (54.3520, 18.6466), "lodz": (51.7592, 19.4560), "łódź": (51.7592, 19.4560),
          "gliwice": (50.2945, 18.6714), "tychy": (50.1218, 18.9966), "sosnowiec": (50.2863, 19.1041),
          "bytom": (50.3480, 18.9157), "zabrze": (50.3249, 18.7857), "chorzow": (50.2975, 18.9545),
          "chorzów": (50.2975, 18.9545), "ruda slaska": (50.2558, 18.8556), "ruda śląska": (50.2558, 18.8556),
          "nysa": (50.4746, 17.3343)}


class Blad(Exception):
    """Błąd z czytelnym komunikatem dla użytkownika (bez śladu stosu)."""


def konfiguracja_gita(katalog):
    """Treść pliku config repozytorium, którego korzeniem jest `katalog` (`.git` katalog albo plik `gitdir:` drzewa roboczego);
    "" gdy to nie korzeń repozytorium albo config jest nieczytelny."""
    g = os.path.join(katalog, ".git")
    try:
        if os.path.isfile(g):
            with open(g, encoding="utf-8") as f:
                gd = f.read().strip()
            if not gd.startswith("gitdir:"):
                return ""
            gd = os.path.join(katalog, gd[7:].strip())
            wspolny = os.path.join(gd, "commondir")
            if os.path.isfile(wspolny):
                with open(wspolny, encoding="utf-8") as f:
                    gd = os.path.join(gd, f.read().strip())
            g = gd
        if not os.path.isdir(g):
            return ""
        with open(os.path.join(g, "config"), encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def w_repo(sciezka):
    """true, gdy ścieżka leży w tym repozytorium albo w dowolnym klonie lub drzewie roboczym cpap-eink (WY12)."""
    p = os.path.realpath(sciezka)
    r = os.path.realpath(REPO)
    if p == r or p.startswith(r + os.sep):
        return True
    k = p
    while True:
        if os.path.exists(os.path.join(k, ".git")) and "cpap-eink" in konfiguracja_gita(k):
            return True
        nad = os.path.dirname(k)
        if nad == k:
            return False
        k = nad


def czas(tekst, opcja):
    for wzor in ("%Y-%m-%dT%H:%M", "%Y-%m-%dT%H:%M:%S", "%Y-%m-%d %H:%M"):
        try:
            return dt.datetime.strptime(tekst.strip(), wzor)
        except ValueError:
            pass
    raise Blad(f"{opcja}: nie rozumiem daty „{tekst}”. Wzór: 2026-11-05T07:15")


def przyciski(plik):
    """Sprawdza plik przycisków i zwraca wiersze w postaci dla symulatora: `RRRR-MM-DDTHH:MM:SS przycisk [2s]`."""
    if not os.path.isfile(plik):
        raise Blad(f"nie ma pliku przycisków: {plik}")
    wynik = []
    try:
        with open(plik, encoding="utf-8-sig") as f:
            tresc = f.read()
    except UnicodeDecodeError:
        raise Blad(f"plik przycisków {plik} nie jest zwykłym tekstem w UTF-8. W TextEdit: Format → Zmień na zwykły "
                   "tekst, potem Zachowaj z kodowaniem „Unicode (UTF-8)”") from None
    except OSError as e:
        raise Blad(f"nie mogę przeczytać pliku przycisków {plik}: {e.strerror}") from None
    if tresc.lstrip().startswith("{\\rtf"):
        raise Blad(f"plik przycisków {plik} jest zapisany jako dokument RTF, nie zwykły tekst. W TextEdit: "
                   "Format → Zmień na zwykły tekst, potem zapisz jeszcze raz")
    for nr, wiersz in enumerate(tresc.splitlines(), 1):
        w = wiersz.split("#", 1)[0].strip()
        if not w:
            continue
        czesci = w.split()
        if len(czesci) < 2:
            raise Blad(f"plik przycisków, wiersz {nr}: „{w}” — brak przycisku. Wzór: 2026-11-05T07:15 srodkowy")
        kiedy = czas(czesci[0], f"plik przycisków, wiersz {nr}")
        ktory = PRZYCISKI.get(czesci[1].lower())
        if not ktory:
            raise Blad(f"plik przycisków, wiersz {nr}: nieznany przycisk „{czesci[1]}”. "
                       "Do wyboru: lewy, srodkowy, prawy")
        reszta = " ".join(czesci[2:]).lower()
        if reszta.startswith("do:") and len(reszta) > 3 and " " not in reszta:  # powtarzaj aż do ekranu
            wynik.append(f"{kiedy:%Y-%m-%dT%H:%M:%S} {ktory} {czesci[2]}")
            continue
        if reszta and reszta not in DLUGO:
            raise Blad(f"plik przycisków, wiersz {nr}: „{reszta}” — po nazwie przycisku może być tylko 2s "
                       "(przytrzymanie) albo do:{ekran}, np. do:ustawienia")
        wynik.append(f"{kiedy:%Y-%m-%dT%H:%M:%S} {ktory}" + (" 2s" if reszta else ""))
    return wynik


def ile(n, jeden, kilka, wiele):
    """Liczba z rzeczownikiem w polskiej odmianie: 1 obrazek, 2 obrazki, 5 obrazków."""
    d, s = n % 10, n % 100
    return f"{n} " + (jeden if n == 1 else kilka if 2 <= d <= 4 and not 12 <= s <= 14 else wiele)


def ma_noce(k):
    return os.path.isfile(os.path.join(k, "noce.csv")) or bool(glob.glob(os.path.join(k, "dane", "noce", "*.csv")))


BRAK_NARZEDZI = ("nie znalazłem działającego kompilatora C++. Na Macu brakuje narzędzi programisty Apple: wklej "
                 "w Terminalu xcode-select --install, zatwierdź instalację i powtórz komendę")


def kompilator():
    """Pierwszy działający kompilator (CXX, g++, clang++, c++) i pierwszy wiersz jego wersji. Na Macu bez narzędzi
    Apple /usr/bin/g++ istnieje, ale to shim xcrun, który kończy się błędem: wtedy komunikat o xcode-select."""
    for k in (os.environ.get("CXX"), "g++", "clang++", "c++"):
        if not k or not shutil.which(k):
            continue
        try:
            r = subprocess.run([k, "--version"], capture_output=True, text=True, timeout=60)
        except (OSError, subprocess.TimeoutExpired):
            continue
        if r.returncode == 0 and r.stdout.strip():
            return k, r.stdout.splitlines()[0]
        if "xcrun" in r.stderr or "xcode-select" in r.stderr or "CommandLineTools" in r.stderr:
            raise Blad(BRAK_NARZEDZI)
    raise Blad(BRAK_NARZEDZI)


def mtime(p):
    try:
        return os.path.getmtime(p)
    except OSError:
        return 0.0


def zbuduj(cicho=False):
    """Kompiluje symulator (pliki obiektowe w build/symulator/, tylko zmienione). Zwraca ścieżkę programu."""
    kx, wersja = kompilator()
    os.makedirs(BUDOWA, exist_ok=True)
    zrodla = zrodla_rdzenia()
    naglowki = (glob.glob(os.path.join(FW, "src", "*.h")) + glob.glob(os.path.join(FW, "include", "*.h"))
                + glob.glob(os.path.join(FW, "test", "atrapy", "**", "*.h"), recursive=True))
    najnowszy_naglowek = max((mtime(h) for h in naglowki), default=0.0)
    flagi = ["-std=c++17", "-O1", "-I" + os.path.join(FW, "src"), "-I" + os.path.join(FW, "include"),
             "-I" + os.path.join(FW, "test", "atrapy")]
    znacznik = os.path.join(BUDOWA, "kompilator.txt")
    opis = kx + " " + wersja + " " + " ".join(flagi)
    stary_kompilator = not os.path.exists(znacznik) or open(znacznik, encoding="utf-8").read() != opis

    def obiekt(z):
        return os.path.join(BUDOWA, os.path.basename(z)[:-4] + ".o")

    do_zrobienia = [z for z in zrodla if stary_kompilator or mtime(obiekt(z)) < max(mtime(z), najnowszy_naglowek)]
    if do_zrobienia and not cicho:
        print(f"kompiluję firmware do symulatora ({len(do_zrobienia)} z {len(zrodla)} plików, {kx}); "
              "pierwszy raz trwa do minuty…", flush=True)

    def kompiluj(z):
        r = subprocess.run([kx] + flagi + ["-c", z, "-o", obiekt(z)], capture_output=True, text=True)
        return z, r

    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as pula:
        for z, r in pula.map(kompiluj, do_zrobienia):
            if r.returncode:
                raise Blad(f"kompilacja {os.path.relpath(z, REPO)} się nie udała:\n{r.stderr[-3000:]}")
    program = os.path.join(BUDOWA, "symulator")
    glowny = os.path.join(FW, "test", "symulator.cpp")
    obiekty = [obiekt(z) for z in zrodla]
    if do_zrobienia or mtime(program) < max([mtime(glowny), najnowszy_naglowek] + [mtime(o) for o in obiekty]):
        r = subprocess.run([kx] + flagi + [glowny] + obiekty + ["-o", program], capture_output=True, text=True)
        if r.returncode:
            raise Blad(f"kompilacja test/symulator.cpp się nie udała:\n{r.stderr[-3000:]}")
    with open(znacznik, "w", encoding="utf-8") as f:
        f.write(opis)
    return program


def png(sciezka, dane, w=800, h=480):
    """PNG 1 bit z bufora panelu (bit 1 = czarny), bez Pillow (jak tools/prototyp/podglad.py)."""
    wiersz = w // 8
    surowe = b"".join(b"\x00" + bytes(255 - b for b in dane[y * wiersz:(y + 1) * wiersz]) for y in range(h))

    def blok(typ, d):
        return struct.pack(">I", len(d)) + typ + d + struct.pack(">I", zlib.crc32(typ + d) & 0xFFFFFFFF)

    with open(sciezka, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + blok(b"IHDR", struct.pack(">IIBBBBB", w, h, 1, 0, 0, 0, 0))
                + blok(b"IDAT", zlib.compress(surowe, 9)) + blok(b"IEND", b""))


def pbm_na_png(pbm, cel):
    with open(pbm, "rb") as f:
        d = f.read()
    naglowek = b"P4\n800 480\n"
    if not d.startswith(naglowek) or len(d) != len(naglowek) + 48000:
        raise Blad(f"zły plik klatki z symulatora: {pbm}")
    png(cel, d[len(naglowek):])


def katalog_wyjscia(od, do, nad=None, przedrostek=""):
    """Nowy (nieistniejący albo pusty) katalog wyniku `{nad}/{przedrostek}{okres}`, z przyrostkiem -2, -3… gdy zajęty.
    Przebieg scenariusza: przedrostek `scenariusz-{nazwy}-`."""
    nad = nad or os.path.join(os.path.expanduser("~"), "cpap-eink-symulator")
    baza = os.path.join(nad, f"{przedrostek}{od:%Y-%m-%d_%H%M}--{do:%Y-%m-%d_%H%M}")
    k, n = baza, 2
    while os.path.exists(k) and os.listdir(k):
        k, n = f"{baza}-{n}", n + 1
    return k


def wspolrzedne_miasta(miasto=None, wspolrzedne=None):
    """(lat, lon) z --wspolrzedne `50.26,19.02` albo z nazwy miasta (tabela MIASTA)."""
    if wspolrzedne:
        try:
            lat, lon = (float(x) for x in str(wspolrzedne).replace(" ", "").split(","))
        except ValueError:
            raise Blad(f"--wspolrzedne: nie rozumiem „{wspolrzedne}”. Wzór: 50.2597,19.0211") from None
        if not (-90 <= lat <= 90 and -180 <= lon <= 180):
            raise Blad(f"--wspolrzedne poza zakresem: {wspolrzedne}")
        return lat, lon
    m = (miasto or "Katowice").strip().lower()
    if m not in MIASTA:
        znane = sorted({k for k in MIASTA if k.isascii()})
        raise Blad(f"nie znam współrzędnych miasta „{miasto}”. Znane: {', '.join(znane)}; inne podaj jako "
                   "--wspolrzedne 50.2597,19.0211")
    return MIASTA[m]


def profil_prognozy(plik, cel):
    """Odpowiedź Open-Meteo z pliku JSON → 48 wierszy dla symulatora (godzina 0…47 od pierwszej godziny pliku).
    Plik: {"forecast": <odpowiedź /v1/forecast>, "air_quality": <odpowiedź /v1/air-quality>} albo sama odpowiedź
    /v1/forecast (wtedy bez PM2.5 i pyłków, chyba że jej blok `hourly` je ma). Daty z pliku nie mają znaczenia."""
    import json
    plik = os.path.abspath(os.path.expanduser(plik))
    try:
        with open(plik, encoding="utf-8") as f:
            j = json.load(f)
    except OSError as e:
        raise Blad(f"nie mogę przeczytać pliku prognozy {plik}: {e.strerror}") from None
    except ValueError:
        raise Blad(f"plik prognozy {plik} nie jest poprawnym JSON-em (odpowiedź serwisu Open-Meteo)") from None
    bloki = [j.get("forecast"), j.get("air_quality")] if isinstance(j, dict) and "forecast" in j else [j]
    godz = {}
    for b in bloki:
        if not isinstance(b, dict):
            continue
        # panel pyta z timezone=Europe/Warsaw (src/slot_dzienny.cpp urlPrognozy()), więc godziny są lokalne
        if b.get("timezone") not in (None, "Europe/Warsaw"):
            raise Blad(f"plik prognozy {plik}: strefa „{b.get('timezone')}”, a panel pyta z timezone=Europe/Warsaw; "
                       "pobierz prognozę z &timezone=Europe%2FWarsaw")
        if isinstance(b.get("hourly"), dict):
            h = b["hourly"]
            t0 = h.get("time", [None])[0] if isinstance(h.get("time"), list) and h.get("time") else None
            if t0 is not None and not str(t0).endswith("T00:00"):
                raise Blad(f"plik prognozy {plik}: pierwsza godzina {t0}, a symulator potrzebuje 48 godzin od północy "
                           "czasu lokalnego (odpowiedź z timezone=Europe/Warsaw i forecast_days=2)")
            godz.update({k: v for k, v in h.items() if isinstance(v, list)})
    pola = ("temperature_2m", "precipitation_probability", "cloud_cover", "wind_gusts_10m", "pm2_5", "birch_pollen",
            "grass_pollen", "alder_pollen", "mugwort_pollen")
    if "temperature_2m" not in godz or len(godz["temperature_2m"]) < 48:
        raise Blad(f"plik prognozy {plik}: brak 48 godzin `hourly.temperature_2m` (odpowiedź Open-Meteo z "
                   "forecast_days=2)")
    with open(cel, "w", encoding="utf-8") as f:
        for g in range(48):
            w = []
            for p in pola:
                v = godz.get(p, [None] * 48)
                v = v[g] if g < len(v) else None
                w.append("" if v is None or isinstance(v, bool) or not isinstance(v, (int, float)) else repr(float(v)))
            f.write(f"{g};" + ";".join(w) + "\n")
    return cel


OBRAZY = (".png", ".jpg", ".jpeg", ".bmp", ".gif")


def obrazki_panelu(katalog, tmp, cicho=False):
    """Katalog z plikami panelu (.pbm i obrazki.json) z --obrazki: sam katalog, gdy ma pliki .pbm; jego podkatalog
    `panel/` z plikami .pbm (układ assets/obrazki-dzienne-prywatne/); inaczej obrazy z katalogu (albo z jego
    `obrazki-1bit/`) przepuszczone przez tools/obrazki/przygotuj.py do katalogu tymczasowego, z manifestem
    obrazki.json z katalogu albo katalogu nadrzędnego."""
    k = os.path.abspath(os.path.expanduser(katalog))
    if not os.path.isdir(k):
        raise Blad(f"nie ma katalogu obrazków: {k}")

    def pliki(d, konc):
        return [n for n in os.listdir(d) if n.lower().endswith(konc) and not n.startswith("._")] if os.path.isdir(d) else []

    if pliki(k, (".pbm",)):
        return k
    if pliki(os.path.join(k, "panel"), (".pbm",)):
        return os.path.join(k, "panel")
    zrodlo = k if pliki(k, OBRAZY) else os.path.join(k, "obrazki-1bit") if pliki(os.path.join(k, "obrazki-1bit"), OBRAZY) else None
    if not zrodlo:
        raise Blad(f"w katalogu {k} nie ma obrazków (.pbm, .png, .jpg) ani podkatalogu panel/ z plikami .pbm")
    manifest = next((m for m in (os.path.join(zrodlo, "obrazki.json"), os.path.join(os.path.dirname(zrodlo), "obrazki.json"))
                     if os.path.isfile(m)), None)
    sys.path.insert(0, os.path.join(REPO, "tools", "obrazki"))
    try:
        import przygotuj
    except ImportError:
        raise Blad("obrazki trzeba zamienić na format panelu, a do tego potrzebna jest biblioteka Pillow, której ten "
                   "Python nie ma. Wskaż katalog z gotowymi plikami .pbm (np. assets/obrazki-dzienne-prywatne/panel) albo "
                   "zainstaluj Pillow: python3 -m pip install --user Pillow") from None
    cel = os.path.join(tmp, "obrazki-panelu")
    w = przygotuj.przygotuj(zrodlo, manifest, cel)
    if not cicho:
        print(f"obrazki dnia: {len(w)} z {zrodlo} zamienione na format panelu (tymczasowo)", flush=True)
    return cel


def symuluj(dane, od=None, do=None, plik_przyciskow=None, wyjscie=None, bez_sieci=False, cicho=False, zegar_znany=False,
            miasto="Katowice", wspolrzedne=None, prognoza=None, bez_prognozy=False, obrazki=None, scenariusze=(),
            profil_publiczny=False, bez_ntp=False):
    """Cały przebieg. Zwraca katalog wyniku. Rzuca Blad z komunikatem dla użytkownika. `scenariusze`: nazwy
    z tools/symulator/scenariusze.py (noce syntetyczne po zrzucie); bez `od` i `do` okres bierze się ze scenariusza.
    `profil_publiczny`: wiersz `profil=publiczny` w /konfiguracja.txt panelu (5.6.4), bez zmiany kodu panelu."""
    dane = os.path.abspath(os.path.expanduser(dane))  # `~` w cudzysłowie Terminal zostawia bez rozwinięcia
    if plik_przyciskow:
        plik_przyciskow = os.path.abspath(os.path.expanduser(plik_przyciskow))
    if not os.path.isdir(dane):
        raise Blad(f"nie ma katalogu zrzutu: {dane}")
    if w_repo(dane):
        raise Blad("zrzut leży w repozytorium. To dane zdrowotne: przenieś katalog poza repozytorium "
                   "(np. do ~/cpap-eink-dane/) i podaj nową ścieżkę (WY12)")
    if not ma_noce(dane):
        pod = sorted(d for d in os.listdir(dane) if os.path.isdir(os.path.join(dane, d)) and ma_noce(os.path.join(dane, d)))
        if pod:
            raise Blad(f"w katalogu {dane} nie ma pliku nocy, są za to zrzuty w podkatalogach: {', '.join(pod)}. "
                       f"Podaj jeden z nich, np. --dane \"{os.path.join(dane, pod[0])}\"")
        raise Blad(f"w katalogu {dane} nie ma pliku nocy (noce.csv albo dane/noce/RRRR.csv). "
                   "Podaj katalog, do którego zapisał pliki tools/prototyp/zrzut_plikow.py")
    plan = None
    if scenariusze:
        try:
            plan = scen.plan(dane, list(scenariusze), obrazki=bool(obrazki))
        except scen.BladScenariusza as e:
            raise Blad(str(e)) from None
        if od and czas(od, "--od") != plan.od and not cicho:
            print(f"UWAGA: scenariusz pokazuje swój ekran od {plan.od:%Y-%m-%dT%H:%M}, a podano --od {od}; "
                  "przy innym początku ekranu scenariusza może nie być w wyniku", flush=True)
        od = od or f"{plan.od:%Y-%m-%dT%H:%M}"
        do = do or f"{plan.do:%Y-%m-%dT%H:%M}"
    if not od or not do:
        raise Blad("podaj --od i --do (albo --scenariusz, który sam ustawia okres)")
    t_od, t_do = czas(od, "--od"), czas(do, "--do")
    if t_do <= t_od:
        raise Blad("--do musi być później niż --od")
    if t_do - t_od > dt.timedelta(days=14) and not cicho:
        print("uwaga: okres dłuższy niż 14 dni, symulacja i log będą długie", flush=True)
    if wyjscie:
        wyjscie = os.path.abspath(os.path.expanduser(wyjscie))
        if os.path.exists(wyjscie) and not os.path.isdir(wyjscie):
            raise Blad(f"--wyjscie wskazuje plik, a nie katalog: {wyjscie}")
        if os.path.isdir(wyjscie) and os.listdir(wyjscie):  # nigdy nie mieszamy z poprzednim wynikiem
            wyjscie = katalog_wyjscia(t_od, t_do, wyjscie, przedrostek_scenariusza(scenariusze))
    else:
        wyjscie = katalog_wyjscia(t_od, t_do, przedrostek=przedrostek_scenariusza(scenariusze))
    rd, rw = os.path.realpath(dane), os.path.realpath(wyjscie)
    if rw == rd or rw.startswith(rd + os.sep):
        raise Blad("katalog wyniku leży w katalogu zrzutu: obrazki i logi weszłyby przy następnym przebiegu do "
                   "danych panelu. Wybierz katalog poza zrzutem albo pomiń --wyjscie")
    if w_repo(wyjscie):
        raise Blad("katalog wyniku leży w repozytorium. Obrazki pokazują dane zdrowotne: wybierz katalog poza "
                   "repozytorium albo pomiń --wyjscie (domyślnie ~/cpap-eink-symulator/) (WY12)")
    lat_lon = None if bez_prognozy else wspolrzedne_miasta(miasto, wspolrzedne)
    program = zbuduj(cicho)
    with tempfile.TemporaryDirectory(prefix="cpap-eink-sym-") as tmp:
        argumenty = [program, "--dane", os.path.abspath(dane), "--od", f"{t_od:%Y-%m-%dT%H:%M}",
                     "--do", f"{t_do:%Y-%m-%dT%H:%M}", "--wyjscie", tmp]
        wiersze_przyc = przyciski(plik_przyciskow) if plik_przyciskow else []
        if plan:
            wiersze_przyc = sorted(wiersze_przyc + plan.przyciski)
            p = os.path.join(tmp, "noce-scenariusza.txt")
            with open(p, "w", encoding="utf-8") as f:
                f.write("".join(x + "\n" for x in plan.noce))
            if plan.noce:
                argumenty += ["--noce-scenariusza", p]
            if plan.otoczenie:
                p = os.path.join(tmp, "otoczenie.txt")
                with open(p, "w", encoding="utf-8") as f:
                    f.write("".join(f"{h};{c}\n" for h, c in plan.otoczenie))
                argumenty += ["--otoczenie", p]
                if plan.otoczenie_od:
                    argumenty += ["--otoczenie-od", f"{plan.otoczenie_od:%Y-%m-%dT%H:%M}"]
                if plan.otoczenie_do:
                    argumenty += ["--otoczenie-do", f"{plan.otoczenie_do:%Y-%m-%dT%H:%M}"]
            if plan.milczy_od:
                argumenty += ["--airbridge-milczy-od", f"{plan.milczy_od:%Y-%m-%dT%H:%M}"]
            if plan.aparat:  # model i tryb aparatu w atrapie AirBridge zgodne z nocami scenariusza
                argumenty += ["--aparat", f"{plan.aparat[0]},{plan.aparat[1]}"]
            for d_, g_ in sorted(plan.opoznione.items()):  # noc dochodzi później niż 04:30
                argumenty += ["--noc-opozniona", f"{d_},{g_}"]
            for od_, do_ in plan.przerwy:  # wyjazd (AirBridge milczy, Wi-Fi jest)
                argumenty += ["--airbridge-przerwa", f"{od_:%Y-%m-%dT%H:%M},{do_:%Y-%m-%dT%H:%M}"]
            # ustawienia sprzętu (/konfiguracja.txt) i odpowiedzi „Co było inaczej?” scenariusza
            for wiersze_, opcja, nazwa in ((plan.czynniki, "--czynniki-scenariusza", "czynniki-scenariusza.txt"),):
                if wiersze_:
                    p = os.path.join(tmp, nazwa)
                    with open(p, "w", encoding="utf-8") as f:
                        f.write("".join(x + "\n" for x in wiersze_))
                    argumenty += [opcja, p]
            for o in plan.opis:
                argumenty += ["--opis", o]
        konf = (list(plan.konfiguracja) if plan else []) + (["profil=publiczny"] if profil_publiczny else [])
        if konf:  # ustawienia sprzętu scenariusza; profil publiczny (--profil-publiczny)
            p = os.path.join(tmp, "konfiguracja-scenariusza.txt")
            with open(p, "w", encoding="utf-8") as f:
                f.write("".join(x + "\n" for x in konf))
            argumenty += ["--konfiguracja", p]
        if wiersze_przyc:
            p = os.path.join(tmp, "przyciski-sprawdzone.txt")
            with open(p, "w", encoding="utf-8") as f:
                f.write("".join(x + "\n" for x in wiersze_przyc))
            argumenty += ["--przyciski", p]
        if lat_lon:
            argumenty += ["--lat", repr(lat_lon[0]), "--lon", repr(lat_lon[1]),
                          "--prognoza", profil_prognozy(prognoza or PROGNOZA_DOMYSLNA, os.path.join(tmp, "prognoza.txt"))]
        if obrazki:
            argumenty += ["--obrazki", obrazki_panelu(obrazki, tmp, cicho)]
        if bez_sieci:
            argumenty.append("--bez-sieci")
        if zegar_znany:
            argumenty.append("--zegar-znany")
        if bez_ntp:
            argumenty.append("--bez-ntp")
        r = subprocess.run(argumenty, capture_output=True, text=True)
        if r.returncode:
            tekst = (r.stderr or r.stdout).strip()[-3000:]
            raise Blad("symulator zakończył się błędem: " + tekst.replace("BŁĄD: ", "", 1))
        try:
            os.makedirs(wyjscie, exist_ok=True)
        except OSError as e:
            raise Blad(f"nie mogę utworzyć katalogu wyniku {wyjscie}: {e.strerror}") from None
        for pbm in sorted(glob.glob(os.path.join(tmp, "*.pbm"))):
            pbm_na_png(pbm, os.path.join(wyjscie, os.path.basename(pbm)[:-4] + ".png"))
        for pdf in sorted(glob.glob(os.path.join(tmp, "*raport-*.pdf"))):  # raport A4 z pamięci panelu
            with open(pdf, "rb") as f:
                d = f.read()
            if plan and plan.noce:  # noce wymyślone, znak na każdej stronie i w polu „Pacjent”
                d = oznacz_pdf(d)
            with open(os.path.join(wyjscie, os.path.basename(pdf)), "wb") as f:
                f.write(d)
        for nazwa in ("log.txt", "log-panelu.txt"):
            shutil.copyfile(os.path.join(tmp, nazwa), os.path.join(wyjscie, nazwa))
        if plik_przyciskow:
            shutil.copyfile(plik_przyciskow, os.path.join(wyjscie, "przyciski.txt"))
        if plan and plan.przyciski:
            with open(os.path.join(wyjscie, "przyciski-scenariusza.txt"), "w", encoding="utf-8") as f:
                f.write("# naciśnięcia dołożone przez scenariusz\n" + "".join(x + "\n" for x in plan.przyciski))
        if plan and plan.noce:
            with open(os.path.join(wyjscie, UWAGA_SYN), "w", encoding="utf-8") as f:
                f.write(TEKST_UWAGI_SYN.format(nazwy=", ".join(scenariusze), od=plan.noce[0][:10], do=plan.noce[-1][:10]))
        if r.stderr and not cicho:
            print(r.stderr.strip())
    spis = klatki(wyjscie)
    with open(os.path.join(wyjscie, "spis.txt"), "w", encoding="utf-8") as f:
        f.write(tekst_spisu(spis, plan, skroty=skroty_png(wyjscie), przyciski=przyciski_klatek(wyjscie)))
    return wyjscie


# ---------- oznaczenie nocy syntetycznych ----------
# raport PDF z nocami wymyślonymi. Kodowanie fontów panelu (src/raport_pdf.cpp): WinAnsi z Differences
# 192… (\300 ą, \302 ę, \305 ś), \267 kropka środkowa.
ZNAK_PDF = (b"q 0 g 0 806 595.3 36 re f 1 g BT /F2 13 Tf 36 818.5 Td (DANE SYNTETYCZNE \267 noce wymy\305lone przez "
            b"symulator \267 to nie jest raport pacjenta) Tj ET Q\n"
            b"q 1 g 340 772 219.3 14 re f 0 g BT /F2 8.6 Tf 343.6 777.7 Td (Pacjent: DANE SYNTETYCZNE \\(symulator\\)) Tj "
            b"ET Q\n"
            b"q 0 g 0 0 595.3 24 re f 1 g BT /F2 11 Tf 36 8 Td (DANE SYNTETYCZNE \267 nie drukowa\301 dla lekarza) Tj ET Q\n")


def oznacz_pdf(d):
    """Przyrostowa aktualizacja PDF (dopisana na końcu, treść panelu bez zmian): strumień ze znakiem DANE SYNTETYCZNE
    (pas u góry i u dołu strony, pole „Pacjent” zasłonięte) dołożony do /Contents każdej strony."""
    import re
    rozm = re.search(rb"/Size (\d+)", d[d.rfind(b"trailer"):])
    sx = re.search(rb"startxref\s+(\d+)", d[d.rfind(b"startxref"):])
    korzen = re.search(rb"/Root (\d+ \d+ R)", d[d.rfind(b"trailer"):])
    info = re.search(rb"/Info (\d+ \d+ R)", d[d.rfind(b"trailer"):])
    if not (rozm and sx and korzen):
        raise Blad("raport PDF z panelu w nieznanym układzie: nie mogę dołożyć oznaczenia DANE SYNTETYCZNE")
    nr = int(rozm.group(1))
    obiekty = [(nr, b"<< /Length %d >>\nstream\n" % len(ZNAK_PDF) + ZNAK_PDF + b"endstream")]
    strony = 0
    for m in re.finditer(rb"(\d+) 0 obj\s*(<<\s*/Type\s*/Page\b.*?)\s*endobj", d, re.S):
        sl = m.group(2)
        c = re.search(rb"/Contents\s*(\[[^\]]*\]|\d+ 0 R)", sl)
        if not c:
            continue
        stare = c.group(1)[1:-1] if c.group(1).startswith(b"[") else c.group(1)
        sl = sl[:c.start()] + b"/Contents [" + stare + b" %d 0 R]" % nr + sl[c.end():]
        obiekty.append((int(m.group(1)), sl))
        strony += 1
    if not strony:
        raise Blad("raport PDF z panelu bez stron: nie mogę dołożyć oznaczenia DANE SYNTETYCZNE")
    w = bytearray(d if d.endswith(b"\n") else d + b"\n")
    xref = b"xref\n0 1\n0000000000 65535 f \n"
    for n, tresc in obiekty:
        xref += b"%d 1\n%010d 00000 n \n" % (n, len(w))
        w += b"%d 0 obj\n" % n + tresc + b"\nendobj\n"
    start = len(w)
    w += xref + b"trailer\n<< /Size %d /Root %s%s /Prev %s >>\nstartxref\n%d\n%%%%EOF\n" % (
        nr + 1, korzen.group(1), b" /Info " + info.group(1) if info else b"", sx.group(1), start)
    return bytes(w)

UWAGA_SYN = "UWAGA-NOCE-SYNTETYCZNE.txt"
TEKST_UWAGI_SYN = (
    "UWAGA: NOCE SYNTETYCZNE\n\n"
    "Ten katalog pokazuje scenariusz symulatora ({nazwy}). Noce z poranków {od} – {do} są WYMYŚLONE przez scenariusz,\n"
    "żeby pokazać ekrany (np. „pokaż lekarzowi”). To nie są Twoje noce i nie są oceną Twojej terapii.\n"
    "Każdy obrazek z tymi nocami ma w nazwie przedrostek SYN- i napis DANE SYNTETYCZNE w prawym górnym rogu.\n")


def przedrostek_scenariusza(nazwy):
    return f"scenariusz-{'+'.join(nazwy)}-" if nazwy else ""


# ---------- spis obrazków (spis.txt) ----------
OPISY = {"S5": "brak danych z aparatu (S5, E-01)", "S10": "noc jeszcze w drodze, synchronizacja (S10)",
         "SK33": "slot dzienny: okno na wietrzenie (S-K-33)", "SK34": "slot dzienny: jutro rano (S-K-34)",
         "SK35": "slot dzienny: wymiana sprzętu (S-K-35)", "SK36": "slot dzienny: pyłek (S-K-36)",
         "SK37": "slot dzienny: dzień i noc, wschód i zachód słońca (S-K-37)", "SK38": "slot dzienny: obrazek dnia (S-K-38)",
         "SK39": "slot dzienny: po drzemce (S-K-39)", "SK13": "rano po nocy z wysokim CO2 (S-K-13)",
         # ekrany
         "S1": "odkrycie dnia (S1)", "S7": "podsumowanie miesiąca (S7)", "S8": "„Twoja norma” (S8), raz po 30 nocach",
         "SK30": "„Oddychasz szybciej niż zwykle” (S-K-30)", "SK31": "„Maska się zużywa” (S-K-31)",
         "SK32": "„Nowa maska działa” (S-K-32)", "SK17": "oddechy szybsze (S-K-17)", "S2": "ekran dnia (S2)",
         "S2A": "ekran dnia (S2A)", "S3": "problem poranny (S3)", "S9": "ekran nocny (S9)"}


def opis_ekranu(nazwa):
    """Nazwa ekranu z nazwy obrazka (`dzien-S2A`, `K1`, `dzien-wzorzec-L-03_ge10` …) → opis po polsku."""
    stale = {"K1": "kokpit K1, strona 1 / 3", "K2": "kokpit K2 (trendy), strona 2 / 3",
             "sprawy": "Otwarte sprawy (karta), bez numeru strony",
             "zapisano": "potwierdzenie „Zapisano”", "omowione": "„Omówione z lekarzem?”",
             "przeniesione": "„wiem”: przeniesione do Otwartych spraw", "raport": "raport dla lekarza",
             "pytanie": "pytanie o czynniki nocy", "test": "ekran testu", "Dobranoc": "ekran nocny „Dobranoc”",
             "dzien-nieznany": "ekran dnia (typ nieznany)",
             # ekrany i otwierane przyciskami
             # strony kokpitu w ciągu ▶ (ADR-0041: `1 / 3` … `3 / 3`, bez S8 i „Sprzętu”)
             "liczby": "„Co znaczą te liczby”, bez numeru strony (profil publiczny)",
             "sprzet-zapisano": "`Wymiana zapisana.` z ◀ 2 s cofnij przez 30 s (R-MAS-01)",
             "ustawienia": "„Ustawienia panelu” (U-01), strona 3 / 3", "formularz": "formularz na telefonie (U-03)",
             "siec-panelu": "sieć panelu (U-02)"}
    if nazwa in stale:
        return stale[nazwa]
    if nazwa.startswith("dzien-wzorzec-"):
        return f"ekran wzorca {nazwa[14:].replace('_', '/')}: „pokaż lekarzowi” albo problem"
    if nazwa.startswith("dzien-odkrycie-"):
        return f"odkrycie dnia {nazwa[15:].replace('_', '/')} (S1)"
    if nazwa.startswith("dzien-zdarzenie-"):
        return f"ekran zdarzenia {nazwa[16:].replace('_', '/')}: problem poranny (S3)"
    if nazwa.startswith("dzien-pytanie-"):
        return {"sennosc": "pytanie o senność (tak/nie)", "gorzej": "pytanie „gorzej niż zwykle?” (tak/nie)",
                "przerwa": "pytanie o przerwę planowaną (tak/nie)",
                "infekcja": "„Oddychasz szybciej niż zwykle” z pytaniem „Katar albo infekcja?” (S-K-30, tak/nie)",
                "poduszka": "pytanie „Wymieniona maska?” (tak/nie, R-MAS-12)",
                "wyjazd": "pytanie „Spałeś poza domem?” po przerwie odczytów (R-CZY-22, tak/nie)"
                }.get(nazwa[14:], "pytanie tak/nie " + nazwa[14:])
    if nazwa.startswith("dzien-wieczor-"):
        k = nazwa[14:]
        return {"SK01": "wieczorem: przewietrz sypialnię, CO2 ponad próg (S-K-01)",
                "SK02": "wieczorem: CO2 wysokie, ale smog za oknem (S-K-02)"}.get(k, "ekran wieczorny " + k)
    if nazwa.startswith("dzien-S2D"):  # przerywnik S2d (5.7.6) ze sceną G01…G18, `G05-warunkowa|neutralna`
        if not nazwa.startswith("dzien-S2D-"):
            return "karta z budzikiem: przerywnik S2d"
        scena, _, wariant = nazwa[10:].partition("-")
        return f"karta z budzikiem: przerywnik S2d {scena} ({wariant})" if wariant else \
            f"karta z budzikiem: przerywnik S2d {scena}"
    if nazwa.startswith("dzien-"):
        t = nazwa[6:]
        return OPISY.get(t, f"ekran dnia {t}")
    return nazwa


def _pozycje_regul(tekst):
    """`E-33 od 2026-11-28, L-01/cpap od …` → {"E-33", "L-01/cpap"}."""
    if not tekst or tekst == "brak":
        return set()
    return {x.split(" od ")[0].strip() for x in tekst.split(", ") if x.strip()}


def _stan_nocy(tekst):
    """`rano 2026-11-26, tryb AutoSet, M-01 459 min, syntetyczna tak` → słownik."""
    d = {}
    for cz in tekst.split(", "):
        if cz.startswith("rano "):
            d["data"] = cz[5:]
        elif cz.startswith("tryb "):
            d["tryb"] = cz[5:]
        elif cz.startswith("M-01 "):
            v = cz[5:].split()[0]
            d["m01"] = int(v) if v.isdigit() else None
        elif cz.startswith("syntetyczna "):
            d["syntetyczna"] = cz[12:] == "tak"
    return d


def klatki_pelne(katalog):
    """Obrazki wyniku w kolejności z log-panelu.txt z kontekstem: słowniki `plik`, `nazwa`, `linia` (tylko ekran dnia),
    `buzzer`, `log` (wiersze tego wybudzenia przed klatką), `reguly` (zbiór reguł spełnionych po tym wybudzeniu), `noc`
    (ostatnia noc w pamięci panelu: data, tryb, m01, syntetyczna). Wiersze `SYMULATOR …` symulator pisze po wierszach
    panelu, a przed wierszami `EKRAN …` tego samego wybudzenia, więc klatka widzi stan z końca swojego wybudzenia."""
    w = []
    linia = buzzer = ""
    wyb, reguly, noc = [], set(), {}
    try:
        with open(os.path.join(katalog, "log-panelu.txt"), encoding="utf-8") as f:
            for l in f:
                t = l.rstrip("\n")[21:]
                if t.startswith("SYMULATOR reguły spełnione"):
                    reguly = _pozycje_regul(t.split(": ", 1)[1].strip())
                    continue
                if t.startswith("SYMULATOR ostatnia noc w pamięci panelu: "):
                    noc = _stan_nocy(t.split(": ", 1)[1].strip())
                    continue
                if t.startswith("cpap-eink "):  # nowe wybudzenie: linia z poprzedniego nie dotyczy tej klatki
                    linia = buzzer = ""
                    wyb = []
                if t.startswith("linia uwagi: "):
                    linia = t[13:]
                elif t.startswith("buzzer"):
                    buzzer = t
                if t.startswith("EKRAN "):
                    cz = t.split()
                    if len(cz) >= 3:
                        w.append({"plik": cz[2], "nazwa": cz[1], "linia": linia if cz[1].startswith("dzien-") else "",
                                  "buzzer": buzzer, "log": list(wyb), "reguly": set(reguly), "noc": dict(noc)})
                    linia = buzzer = ""
                    continue
                wyb.append(t)
    except OSError:
        pass
    return w


def klatki(katalog):
    """Obrazki wyniku w kolejności z log-panelu.txt: (plik, nazwa ekranu, linia uwagi albo "", buzzer albo "")."""
    return [(k["plik"], k["nazwa"], k["linia"], k["buzzer"]) for k in klatki_pelne(katalog)]


def reguly_spelnione(katalog, wszystkie=False):
    """Wiersze `SYMULATOR reguły spełnione` z log-panelu.txt (lista z pamięci panelu po wybudzeniu, w którym się
    zmieniła): ostatni albo (wszystkie=True) wszystkie razem; "" bez wierszy."""
    w = []
    try:
        with open(os.path.join(katalog, "log-panelu.txt"), encoding="utf-8") as f:
            for l in f:
                if "SYMULATOR reguły spełnione" in l:
                    w.append(l.split(": ", 1)[1].strip())
    except OSError:
        pass
    return " | ".join(w) if wszystkie else (w[-1] if w else "")


def regula_w(zbior, wzor):
    """Dokładne dopasowanie reguły: `L-01` pasuje do `L-01` i `L-01/wariant`, nie do `H-08/L-01`."""
    return any(x == wzor or x.startswith(wzor + "/") for x in zbior)


ZWIN_OD = 3  # [stała symulatora] tyle i więcej kolejnych obrazków tego samego ekranu spis pokazuje jednym wierszem


def _godziny(pliki):
    """Godziny z nazw obrazków `NNN-RRRRMMDD-HHMM-…` (z przedrostkiem SYN- albo bez): `05:30, 06:00 …`."""
    w = []
    for p in pliki:
        cz = p[4:].split("-") if p.startswith("SYN-") else p.split("-")
        if len(cz) > 2 and len(cz[2]) == 4 and cz[2].isdigit():
            w.append(f"{cz[1][6:8]}.{cz[1][4:6]} {cz[2][:2]}:{cz[2][2:]}")
    return w


def skroty_png(katalog):
    """{plik: skrót treści} obrazków PNG katalogu wyniku (zwijanie tylko obrazków o identycznej treści)."""
    import hashlib
    w = {}
    for n in os.listdir(katalog):
        if n.endswith(".png"):
            with open(os.path.join(katalog, n), "rb") as f:
                w[n] = hashlib.sha256(f.read()).hexdigest()
    return w


def przyciski_klatek(katalog):
    """{plik obrazka: przycisk, który go dał} z log.txt (`narysowano X.png (po ▶ prawy)`: symulator bierze ostatnie
    naciśnięcie wybudzenia nie późniejsze niż klatka); obrazki bez naciśnięcia (zegar) bez wpisu."""
    w = {}
    try:
        with open(os.path.join(katalog, "log.txt"), encoding="utf-8") as f:
            for l in f:
                if "narysowano " not in l or "przycisk " not in l:
                    continue
                cz = [c.strip() for c in l.split(" · ")]
                for c in cz:  # `narysowano X.png (po ▶ prawy)`: symulator przypisuje obrazek do naciśnięcia (czas)
                    if c.startswith("narysowano ") and " (po " in c:
                        o, po = c[11:].split(" (po ", 1)
                        w[o] = po.rstrip(")").split(" (do ")[0]
    except OSError:
        pass
    return w


def tekst_spisu(spis, plan=None, wciecie="", skroty=None, przyciski=None):
    """Spis obrazków: każdy obrazek z opisem ekranu. ZWIN_OD i więcej kolejnych obrazków tego samego ekranu dnia z tą
    samą linią uwagi jednym wierszem `pierwszy … ostatni (N obrazków …)`, ale tylko w obrębie jednego dnia i tylko, gdy
    obrazki mają identyczną treść (`skroty`: plik → skrót bajtów PNG); bez skrótów nic nie jest zwijane. To
    przerysowania tego samego ekranu, panel działa bez zmian."""
    t = []
    if plan:
        t += [wciecie + o for o in plan.opis]
    i = 0
    while i < len(spis):
        plik, nazwa, linia, buzzer = spis[i]
        j = i + 1
        # tylko ekran dnia (przerysowania z zegara); ekrany z przycisków (K1, U-01 …) zostają po jednym wierszu
        while (skroty is not None and nazwa.startswith("dzien-") and j < len(spis) and spis[j][1:3] == (nazwa, linia)
               and not spis[j][3] and not buzzer and _dzien_klatki(spis[j][0]) == _dzien_klatki(plik)
               and skroty.get(spis[j][0]) is not None and skroty.get(spis[j][0]) == skroty.get(plik)):
            j += 1
        if j - i >= ZWIN_OD:
            g = _godziny([x[0] for x in spis[i:j]])
            kiedy = f", {g[0]} – {g[-1]}" if len(g) == j - i else ""
            x = (f"{wciecie}{plik} … {spis[j - 1][0]} — {opis_ekranu(nazwa)} "
                 f"({ile(j - i, 'obrazek', 'obrazki', 'obrazków')} o identycznej treści: ten sam ekran przerysowany{kiedy})")
        else:
            j = i + 1
            x = f"{wciecie}{plik} — {opis_ekranu(nazwa)}"
            if przyciski and plik in przyciski:  # który przycisk dał ten obrazek
                x += f" (po: {przyciski[plik]})"
        if linia:
            x += f"; linia uwagi: {linia}"
        if buzzer:
            x += f"; {buzzer}"
        t.append(x)
        i = j
    if not spis:
        t.append(wciecie + "(żadnego obrazka)")
    return "\n".join(t) + "\n"


def _warunki(k, warunki, sp=()):
    for rodzaj, v in warunki:
        if rodzaj == "dzien_z":  # klatka tuż po klatce z przedrostkiem v, tego samego dnia
            i = next((n for n, x in enumerate(sp) if x is k), -1)
            if i < 1 or not sp[i - 1]["nazwa"].startswith(v) or _dzien_klatki(sp[i - 1]["plik"]) != _dzien_klatki(k["plik"]):
                return False
        if rodzaj == "noc_dnia" and k["noc"].get("data") != v:  # ostatnia noc w pamięci panelu z tego poranka
            return False
        if rodzaj == "regula" and not regula_w(k["reguly"], v):
            return False
        if rodzaj == "log" and not any(v in x for x in k["log"]):
            return False
        if rodzaj == "tryb" and k["noc"].get("tryb") != v:
            return False
        if rodzaj == "m01_ponizej" and not (k["noc"].get("m01") is not None and k["noc"]["m01"] < v):
            return False
        if rodzaj == "syntetyczna" and k["noc"].get("syntetyczna") != v:
            return False
    return True


def _dzien_klatki(plik):
    """`RRRRMMDD` z nazwy obrazka `[SYN-]NNN-RRRRMMDD-HHMM-…`."""
    cz = (plik[4:] if plik.startswith("SYN-") else plik).split("-")
    return cz[1] if len(cz) > 1 else ""


def _w_logu_panelu(katalog, tekst):
    try:
        with open(os.path.join(katalog, "log-panelu.txt"), encoding="utf-8") as f:
            return any(tekst in l for l in f)
    except OSError:
        return False


def sprawdz_oczekiwane(katalog, plan):
    """[(opis, jest)] dla oczekiwanych ekranów planu (Plan.oczekiwane, tools/symulator/scenariusze.py): `ekran` =
    klatka o nazwie z przedrostkiem, `linia` = linia uwagi klatki ekranu dnia, `regula` = reguła w którejkolwiek liście
    spełnionych w okresie (dopasowanie dokładne); warunki z 4. pola muszą zachodzić dla tej samej klatki
    („jest” tylko przy właściwej treści, zepsuty scenariusz daje BRAK)."""
    sp = klatki_pelne(katalog)
    wszystkie = set()
    for x in reguly_spelnione(katalog, wszystkie=True).split(" | "):
        wszystkie |= _pozycje_regul(x)
    wynik = []
    for o in plan.oczekiwane:
        rodzaj, wzor, opis = o[:3]
        warunki = o[3] if len(o) > 3 else ()
        if rodzaj == "ekran":
            jest = any(k["nazwa"].startswith(wzor) and _warunki(k, warunki, sp) for k in sp)
        elif rodzaj == "linia":
            jest = any(wzor in k["linia"] and _warunki(k, warunki) for k in sp)
        elif rodzaj == "nie_ma":  # ekranu z tym przedrostkiem nie ma od dnia ("od", RRRRMMDD) do ("do") albo końca
            od, do = dict(warunki).get("od", ""), dict(warunki).get("do", "99999999")
            jest = not any(k["nazwa"].startswith(wzor) and od <= _dzien_klatki(k["plik"]) <= do for k in sp)
        elif rodzaj == "bez_linii":  # w dniu ("od") żaden ekran dnia nie ma linii uwagi z tym tekstem
            od = dict(warunki).get("od", "")
            jest = not any(wzor in k["linia"] for k in sp if _dzien_klatki(k["plik"]) == od)
        elif rodzaj == "dni_przed":  # tyle dni z ekranem `wzor` przed pierwszym ekranem ("przed", przedrostek)
            wk = dict(warunki)
            pierwszy = next((_dzien_klatki(k["plik"]) for k in sp if k["nazwa"].startswith(wk["przed"])), None)
            dni = {_dzien_klatki(k["plik"]) for k in sp if k["nazwa"].startswith(wzor)
                   and (pierwszy is None or _dzien_klatki(k["plik"]) < pierwszy)}
            jest = pierwszy is not None and len(dni) >= wk["min"]
        elif rodzaj == "plik":  # plik wyniku z tym fragmentem nazwy (raport PDF)
            jest = any(wzor in n and n.endswith(".pdf") for n in os.listdir(katalog))
        elif rodzaj == "log":  # wiersz panelu (log-panelu.txt) z tym tekstem w okresie
            jest = any(wzor in x for k in sp for x in k["log"]) or _w_logu_panelu(katalog, wzor)
        else:
            jest = regula_w(wszystkie, wzor)
        wynik.append((opis, jest))
    return wynik


def pokaz_wszystko(dane, wyjscie=None, obrazki=None, miasto="Katowice", wspolrzedne=None, prognoza=None, cicho=False,
                   nazwy=None, profil_publiczny=False):
    """Każdy scenariusz osobno (symulator pracuje na kopii zrzutu w pamięci) do `{wyjscie}/scenariusz-NAZWA-{okres}/`,
    potem `{wyjscie}/spis.txt`: scenariusz → obrazki → co pokazują, i czy pojawiło się to, co scenariusz zapowiada.
    Błąd jednego scenariusza nie przerywa reszty: trafia do spisu i do braków."""
    nad = os.path.abspath(os.path.expanduser(wyjscie)) if wyjscie else os.path.join(os.path.expanduser("~"),
                                                                                   "cpap-eink-symulator")
    baza = os.path.join(nad, "pokaz-wszystko")
    k, n = baza, 2
    while os.path.exists(k) and os.listdir(k):
        k, n = f"{baza}-{n}", n + 1
    if w_repo(k):
        raise Blad("katalog wyniku leży w repozytorium. Obrazki pokazują dane zdrowotne: wybierz katalog poza "
                   "repozytorium albo pomiń --wyjscie (domyślnie ~/cpap-eink-symulator/) (WY12)")
    dane_a = os.path.realpath(os.path.abspath(os.path.expanduser(dane)))
    if os.path.realpath(k).startswith(dane_a + os.sep):
        raise Blad("katalog wyniku leży w katalogu zrzutu; wybierz inny albo pomiń --wyjscie")
    os.makedirs(k, exist_ok=True)
    spis = ["Symulator doby cpap-eink: wszystkie scenariusze na Twoim zrzucie.",
            f"Zrzut: {os.path.abspath(os.path.expanduser(dane))}. Noce scenariuszy są WYMYŚLONE (syntetyczne), "
            "dopisane po ostatniej nocy zrzutu; obrazki z nimi mają przedrostek SYN- i napis DANE SYNTETYCZNE. "
            "Twoje dane na dysku zostają nietknięte.", ""]
    if profil_publiczny:  # ten sam profil we wszystkich scenariuszach
        spis[-1:-1] = ["Profil publiczny we wszystkich scenariuszach (--profil-publiczny, `profil=publiczny`, 5.6.4)."]
    braki, znal = [], []
    lista = nazwy or scen.KOLEJNOSC

    def zapisz():
        with open(os.path.join(k, "spis.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(spis + (["Nie pojawiło się albo błąd (do sprawdzenia):"] + ["  " + b for b in braki]
                                      if braki else [])
                               + (["Ekrany, których nie pozwala wywołać (przyczyna w kodzie panelu):"]
                                  + ["  " + z for z in znal] if znal else [])) + "\n")

    for i, nazwa in enumerate(lista, 1):
        if not cicho:
            print(f"[{i}/{len(lista)}] scenariusz {nazwa}…", flush=True)
        spis.append(f"== scenariusz-{nazwa} ({scen.SCENARIUSZE[nazwa][1] if nazwa in scen.SCENARIUSZE else '?'})")
        try:
            plan = scen.plan(dane, [nazwa], obrazki=bool(obrazki))
            w = symuluj(dane, plik_przyciskow=None, wyjscie=os.path.join(k, f"scenariusz-{nazwa}-{plan.od:%Y-%m-%d}"),
                        cicho=True, miasto=miasto, wspolrzedne=wspolrzedne, prognoza=prognoza, obrazki=obrazki,
                        scenariusze=[nazwa], profil_publiczny=profil_publiczny)
        except (Blad, scen.BladScenariusza) as e:
            spis += [f"   BŁĄD: {e}", ""]
            braki.append(f"{nazwa}: błąd: {e}")
            zapisz()
            continue
        spis.append(f"   katalog {os.path.basename(w)}, okres {plan.od:%Y-%m-%d %H:%M} – {plan.do:%Y-%m-%d %H:%M}")
        for opis, jest in sprawdz_oczekiwane(w, plan):
            spis.append(f"   {'jest' if jest else 'BRAK'}: {opis}")
            if not jest:
                braki.append(f"{nazwa}: {opis}")
        for z in plan.znaleziska:
            spis.append(f"   NIE DA SIĘ WYWOŁAĆ: {z}")
            znal.append(f"{nazwa}: {z}")
        reg = reguly_spelnione(w)
        if reg:
            spis.append(f"   reguły spełnione na końcu okresu (pamięć panelu): {reg}")
        spis.append(tekst_spisu(klatki(w), wciecie="   ", skroty=skroty_png(w), przyciski=przyciski_klatek(w)).rstrip("\n"))
        spis.append("")
        zapisz()
    zapisz()
    return k, braki


def main():
    a = argparse.ArgumentParser(description="Symulator doby panelu cpap-eink na zrzucie plików z panelu")
    a.add_argument("--dane", required=True, help="katalog zrzutu z panelu (poza repozytorium)")
    a.add_argument("--od", help="początek, np. 2026-11-05T05:00 (ze scenariuszem: domyślnie jego pierwszy ekran)")
    a.add_argument("--do", help="koniec, np. 2026-11-05T23:59 (ze scenariuszem: domyślnie koniec scenariusza)")
    a.add_argument("--przyciski", help="plik z naciśnięciami, wiersze: 2026-11-05T07:15 srodkowy")
    a.add_argument("--wyjscie", help="katalog na obrazki i log (domyślnie ~/cpap-eink-symulator/…)")
    a.add_argument("--bez-sieci", action="store_true", help="panel bez Wi-Fi")
    a.add_argument("--zegar-znany", action="store_true",
                   help="panel zna czas od startu (jak panel, który już chodził); domyślnie zegar nieznany do NTP")
    a.add_argument("--bez-ntp", action="store_true",
                   help="internet zablokowany: NTP milczy, czas panelu z AirBridge (pole esp_time, R-AW-18)")
    a.add_argument("--miasto", default="Katowice", help="miasto prognozy i wschodu słońca (domyślnie Katowice)")
    a.add_argument("--wspolrzedne", help="współrzędne zamiast miasta, np. 50.2597,19.0211")
    a.add_argument("--prognoza", help="odpowiedź Open-Meteo w JSON (domyślnie tools/symulator/prognoza-katowice-listopad.json)")
    a.add_argument("--bez-prognozy", action="store_true", help="panel bez współrzędnych miasta: bez prognozy i S-K-37")
    a.add_argument("--obrazki", help="katalog obrazków dnia (pliki .pbm z przygotuj.py albo obrazy źródłowe)")
    a.add_argument("--scenariusz", action="append", default=[],
                   help="noce syntetyczne po zrzucie: " + ", ".join(scen.SCENARIUSZE) + " (można kilka, po kolei; nowy-… tylko osobno, bez --pokaz-wszystko)")
    a.add_argument("--pokaz-wszystko", action="store_true",
                   help="wszystkie scenariusze po kolei, każdy w scenariusz-NAZWA/, plus spis.txt")
    a.add_argument("--lista-scenariuszy", action="store_true", help="wypisz scenariusze i zakończ")
    a.add_argument("--profil-publiczny", action="store_true",
                   help="panel w profilu publicznym (`profil=publiczny` w /konfiguracja.txt, 5.6.4): ▶ w K2 otwiera "
                        "„Co znaczą te liczby” (R-KOK-22); teksty AHI i PM2.5 jak w prywatnym; działa też z "
                        "--pokaz-wszystko")
    a = a.parse_args()
    if a.lista_scenariuszy:
        for n in scen.KOLEJNOSC:
            print(f"{n}: {scen.SCENARIUSZE[n][1]}")
        return
    nazwy = [x.strip() for s in a.scenariusz for x in s.split(",") if x.strip()]
    try:
        if a.pokaz_wszystko:
            if nazwy or a.od or a.do or a.przyciski:
                raise Blad("--pokaz-wszystko sam ustawia scenariusze, okresy i przyciski; pomiń --scenariusz, --od, "
                           "--do i --przyciski")
            k, braki = pokaz_wszystko(a.dane, a.wyjscie, a.obrazki, a.miasto, a.wspolrzedne, a.prognoza,
                                      profil_publiczny=a.profil_publiczny)
            pngi = glob.glob(os.path.join(k, "scenariusz-*", "*.png"))
            print(f"\nGotowe: {ile(len(pngi), 'obrazek', 'obrazki', 'obrazków')} w {len(scen.KOLEJNOSC)} scenariuszach, "
                  f"spis w spis.txt, katalog\n  {k}")
            if braki:
                print("Nie pojawiło się (szczegóły na końcu spis.txt):\n  " + "\n  ".join(braki))
            print(f"Otwórz go w Finderze komendą:\n  open \"{k}\"")
            return
        k = symuluj(a.dane, a.od, a.do, a.przyciski, a.wyjscie, a.bez_sieci, zegar_znany=a.zegar_znany,
                    miasto=a.miasto, wspolrzedne=a.wspolrzedne, prognoza=a.prognoza, bez_prognozy=a.bez_prognozy,
                    obrazki=a.obrazki, scenariusze=nazwy, profil_publiczny=a.profil_publiczny, bez_ntp=a.bez_ntp)
    except Blad as e:
        sys.exit(f"BŁĄD: {e}")
    pngi = sorted(glob.glob(os.path.join(k, "*.png")))
    with open(os.path.join(k, "log.txt"), encoding="utf-8") as f:
        log = f.read().splitlines()
    naglowek = log[:log.index("")] if "" in log else log[:4]
    print("\n".join(naglowek))
    print(f"\nGotowe: {ile(len(pngi), 'obrazek', 'obrazki', 'obrazków')} ekranu i log w katalogu\n  {k}")
    print(f"Otwórz go w Finderze komendą:\n  open \"{k}\"")


if __name__ == "__main__":
    main()
