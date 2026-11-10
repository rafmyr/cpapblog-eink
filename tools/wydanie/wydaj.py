#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Wydanie firmware do aktualizacji z GitHub (ADR-0032, ADR-0038).

Buduje obraz bez sekretów (`pio run -e e1001_wydanie`, flaga BEZ_SEKRETOW, numer wersji z argumentu), sprawdza, że
w obrazie nie ma wartości z lokalnego include/secrets.h, liczy SHA-256, pisze `wydanie.json` i nazywa obraz
`cpap-eink-{wersja}.bin`. Z flagą --publikuj tworzy wydanie `v{wersja}` w repozytorium wydań (`gh release create`).
Bez --publikuj niczego nie wysyła.

Użycie:
  python3 tools/wydanie/wydaj.py 1.0.1                       # budowa i pliki w build/wydanie-1.0.1/
  python3 tools/wydanie/wydaj.py 1.0.1 --obraz firmware.bin  # bez budowy, z gotowego obrazu bez sekretów
  python3 tools/wydanie/wydaj.py 1.0.1 --publikuj            # jak wyżej i wydanie na GitHubie (tylko na wyraźne polecenie)
"""
import argparse
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FW = os.path.join(REPO, "firmware", "prototyp")
REPO_WYDAN = "rafmyr/cpapblog-eink"       # wartość domyślna jak REPO_WYDAN w src/aktualizacja.h (ADR-0038 pkt 5)
PLYTKA = "reterminal_e1001"               # PLYTKA_WYDANIA w src/aktualizacja.h
SLOT = 0x400000                           # slot aplikacji 4 MB (firmware/partycje-e1001.csv, spec techniczna 8.2)
TABLICA = os.path.join(REPO, "firmware", "partycje-e1001.csv")

# SemVer jak aktualizacja::parsujWersje(): liczby bez zer wiodących (najwyżej 6 cyfr), identyfikatory [0-9A-Za-z-]
_LICZBA = r"(?:0|[1-9][0-9]{0,5})"
_ID = r"(?:0|[1-9][0-9]{0,5}|[0-9]*[A-Za-z-][0-9A-Za-z-]*)"
WERSJA_RE = re.compile(r"^%s\.%s\.%s(?:-%s(?:\.%s)*)?$" % (_LICZBA, _LICZBA, _LICZBA, _ID, _ID))


def wersja_ok(w):
    return len(w) <= 64 and bool(WERSJA_RE.match(w))


def nazwa_obrazu(wersja):
    return "cpap-eink-%s.bin" % wersja


def uklad(tablica=TABLICA):
    """Identyfikator tablicy partycji tym samym algorytmem co firmware (firmware/prototyp/uklad_partycji.py)."""
    spec = importlib.util.spec_from_file_location("uklad_partycji", os.path.join(FW, "uklad_partycji.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m.identyfikator_pliku(tablica)


def opis(wersja, obraz, uklad_id, plytka=PLYTKA):
    """Słownik `wydanie.json` w kolejności pól aktualizacja::zapiszOpis()."""
    return {
        "wersja": wersja,
        "plytka": plytka,
        "uklad": "0x%08x" % uklad_id,
        "rozmiar": len(obraz),
        "sha256": hashlib.sha256(obraz).hexdigest(),
        "plik": nazwa_obrazu(wersja),
    }


def tekst_opisu(o):
    """Treść pliku jak aktualizacja::zapiszOpis(): JSON bez spacji, jedna linia, znak nowej linii na końcu."""
    return json.dumps(o, ensure_ascii=False, separators=(",", ":")) + "\n"


def sekrety_lokalne(sciezka=os.path.join(FW, "include", "secrets.h")):
    """Wartości tekstowe z `#define` w lokalnym secrets.h (co najmniej 6 znaków); [] bez pliku."""
    if not os.path.exists(sciezka):
        return []
    w = []
    with open(sciezka, encoding="utf-8") as f:
        for linia in f:
            m = re.match(r'\s*#define\s+\w+\s+"([^"]{6,})"', linia)
            if m:
                w.append(m.group(1))
    return w


# Znacznik wkompilowany tylko z -DBEZ_SEKRETOW (src/main.cpp ZNACZNIK_OBRAZU_WYDANIA): obraz bez niego
# nie jest wydawany, także gdy na tej maszynie nie ma include/secrets.h do porównania.
ZNACZNIK = b"cpap-eink:obraz-bez-sekretow:v1"
# Wartości przykładowe ze wzoru include/secrets.example.h i atrapy testów: obraz z nimi to obraz ze wzoru, nie wydanie.
WZORZEC = ["nazwa-sieci", "haslo-wifi", "haslo-airbridge", "wpisz-wlasne-losowe-haslo", "token-zrodla-otoczenia",
           "atrapa-hasla-okna-wgrywania"]


def sprawdz_obraz(obraz, sekrety):
    """Lista błędów: obraz pusty, większy niż slot, bez nagłówka obrazu ESP32 (0xE9), bez znacznika obrazu wydania
    (zbudowany bez BEZ_SEKRETOW), z wartością przykładową ze wzoru albo z wartością z lokalnego secrets.h."""
    b = []
    if obraz and ZNACZNIK not in obraz:
        b.append("brak znacznika obrazu wydania: obraz nie jest zbudowany z -DBEZ_SEKRETOW (pio run -e e1001_wydanie)")
    for w in WZORZEC:
        if w.encode("utf-8") in obraz:
            b.append("obraz zawiera wartość przykładową ze wzoru secrets.example.h")
            break
    if not obraz:
        b.append("obraz pusty")
    if len(obraz) > SLOT:
        b.append("obraz %d B większy niż slot %d B" % (len(obraz), SLOT))
    if obraz and obraz[0] != 0xE9:
        b.append("brak nagłówka obrazu ESP32 (pierwszy bajt 0xE9)")
    for s in sekrety:
        if s.encode("utf-8") in obraz:
            b.append("obraz zawiera wartość z include/secrets.h (%d znaków): wydanie musi być bez sekretów" % len(s))
    return b


def polecenie_budowy(wersja, repo):
    """Polecenie i środowisko budowy obrazu bez sekretów."""
    env = dict(os.environ)
    env["PLATFORMIO_BUILD_FLAGS"] = '-DWERSJA_WYDANIA=\\"%s\\" -DREPO_WYDAN=\\"%s\\"' % (wersja, repo)
    return ["pio", "run", "-e", "e1001_wydanie"], env


def polecenie_publikacji(wersja, repo, pliki, nota):
    return ["gh", "release", "create", "v" + wersja, *pliki, "--repo", repo, "--title", "cpap-eink " + wersja,
            "--notes-file", nota]


def main(argv=None):
    a = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    a.add_argument("wersja", help="numer wersji bez `v`, np. 1.0.1 (wyższy od wersji na panelach)")
    a.add_argument("--repo", default=REPO_WYDAN, help="repozytorium wydań (domyślnie %s)" % REPO_WYDAN)
    a.add_argument("--obraz", help="gotowy obraz zbudowany z BEZ_SEKRETOW (bez budowy)")
    a.add_argument("--katalog", help="katalog wynikowy (domyślnie build/wydanie-{wersja})")
    a.add_argument("--nota", default="", help="krótka nota wydania")
    a.add_argument("--publikuj", action="store_true", help="utwórz wydanie na GitHubie (gh release create)")
    o = a.parse_args(argv)
    if not wersja_ok(o.wersja):
        print("BŁĄD: wersja %r nie jest w zapisie MAJOR.MINOR.PATCH[-pre]" % o.wersja, file=sys.stderr)
        return 2
    if o.obraz:
        sciezka = o.obraz
    else:
        cmd, env = polecenie_budowy(o.wersja, o.repo)
        print("budowa:", " ".join(cmd))
        if subprocess.run(cmd, cwd=FW, env=env).returncode != 0:
            print("BŁĄD: budowa nieudana", file=sys.stderr)
            return 1
        sciezka = os.path.join(FW, ".pio", "build", "e1001_wydanie", "firmware.bin")
    with open(sciezka, "rb") as f:
        obraz = f.read()
    lokalne = sekrety_lokalne()
    if not lokalne:
        print("UWAGA: brak include/secrets.h na tej maszynie: porównanie z lokalnymi hasłami pominięte; "
              "obraz sprawdzony znacznikiem BEZ_SEKRETOW i wartościami wzoru", file=sys.stderr)
    bledy = sprawdz_obraz(obraz, lokalne)
    if bledy:
        for b in bledy:
            print("BŁĄD:", b, file=sys.stderr)
        return 1
    kat = o.katalog or os.path.join(REPO, "build", "wydanie-" + o.wersja)
    os.makedirs(kat, exist_ok=True)
    d = opis(o.wersja, obraz, uklad())
    plik_obrazu = os.path.join(kat, nazwa_obrazu(o.wersja))
    shutil.copyfile(sciezka, plik_obrazu)
    plik_opisu = os.path.join(kat, "wydanie.json")
    with open(plik_opisu, "w", encoding="utf-8") as f:
        f.write(tekst_opisu(d))
    plik_noty = os.path.join(kat, "nota.md")
    with open(plik_noty, "w", encoding="utf-8") as f:
        f.write("cpap-eink %s\n\n%s\n" % (o.wersja, o.nota or "Wydanie firmware panelu (obraz bez sekretów)."))
    print("obraz %s, %d B, SHA-256 %s" % (plik_obrazu, d["rozmiar"], d["sha256"]))
    print("UWAGA: obraz wydania nie ma haseł ani adresów. Wolno go instalować tylko na panelu, który ma już w "
          "pamięci sieć, AirBridge, hasło okna serwisowego, źródło otoczenia i współrzędne z wersji z aktualizatorem "
          "wgranej z secrets.h kablem albo oknem serwisowym.", file=sys.stderr)
    if not o.publikuj:
        print("bez --publikuj: nic nie wysłano")
        return 0
    cmd = polecenie_publikacji(o.wersja, o.repo, [plik_obrazu, plik_opisu], plik_noty)
    print("publikacja:", " ".join(cmd))
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main())
