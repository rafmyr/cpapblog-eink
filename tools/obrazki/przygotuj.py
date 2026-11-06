#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Obrazki dnia S-K-38 w formacie panelu (spec techniczna 8.6; spec funkcjonalna 5.8.12, R-KON-55, R-KON-60).

Wejście: katalog obrazków użytkownika panelu (np. `obrazki-1bit/` z gotowymi plikami 1-bit albo `obrazki/` w szarości)
i manifest `obrazki.json` użytkownika (lista `obrazki[]` z polem `plik` i tagami `dni`, `pogoda`, `sezon`, `inne`).
Wyjście: katalog na pamięć panelu (`/obrazki/` w LittleFS, później karta microSD) z plikami PBM `P4` (bit 1 = czarny)
o rozmiarze do 800×434 i manifestem `obrazki.json` tylko z polami, które czyta panel (`plik` i tagi), bez opisów,
tekstów i źródeł użytkownika. Nazwy PBM w formie, którą przyjmuje panel (`[a-z0-9_-]{1,40}.pbm`, nazwa_panelu()).
Wgranie na panel bez kasowania pamięci: `tools/prototyp/wgraj_pliki.py <port> --obrazki <wyjście>` (komenda `W`).

Obrazki i manifest użytkownika nie trafiają do repozytorium (ADR-0027 pkt 11, .gitignore): skrypt odmawia zapisu
do katalogu wewnątrz repozytorium. Obrazek większy niż 800×434 jest zmniejszany z zachowaniem proporcji (panel nie
skaluje); obraz w szarości progowany (domyślnie 128) albo z rozpraszaniem Floyda–Steinberga (`--rozpraszanie`).

Użycie:
  python3 tools/obrazki/przygotuj.py --wejscie ~/…/obrazki-dzienne-prywatne/obrazki-1bit \\
      --manifest ~/…/obrazki-dzienne-prywatne/obrazki.json --wyjscie ~/…/panel-obrazki
"""
import argparse
import json
import os
import sys

from PIL import Image

MAX_W, MAX_H = 800, 434
TAGI = ("dni", "pogoda", "sezon", "inne")
# nazwa pliku na panelu jak przyjmuje komenda `W` (src/magazyn.h nazwaObrazkaW, wgraj_pliki.py --obrazki)
MAKS_NAZWY = 40
POLSKIE = str.maketrans("ąćęłńóśźż", "acelnoszz")
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def w_repo(sciezka):
    s = os.path.realpath(sciezka)
    r = os.path.realpath(REPO)
    return s == r or s.startswith(r + os.sep)


def nazwa_panelu(plik, zajete=()):
    """Nazwa PBM na panelu z nazwy obrazu użytkownika: małe litery, bez polskich znaków, inne znaki niż [a-z0-9_-] → `_`,
    rdzeń do 40 znaków; przy powtórzeniu (`zajete`) dopisek `_2`, `_3`…"""
    rdzen = os.path.splitext(os.path.basename(plik))[0].lower().translate(POLSKIE)
    rdzen = "".join(c if c in "abcdefghijklmnopqrstuvwxyz0123456789_-" else "_" for c in rdzen)[:MAKS_NAZWY] or "obrazek"
    nazwa, i = rdzen + ".pbm", 2
    while nazwa in zajete:
        dopisek = "_%d" % i
        nazwa, i = rdzen[:MAKS_NAZWY - len(dopisek)] + dopisek + ".pbm", i + 1
    return nazwa


def do_1bit(im, prog=128, rozpraszanie=False):
    """Obraz → tryb '1' (0 = czarny w PIL), zmniejszony do 800×434 z zachowaniem proporcji."""
    if im.mode in ("RGBA", "LA", "P"):
        tlo = Image.new("RGBA", im.size, (255, 255, 255, 255))
        tlo.alpha_composite(im.convert("RGBA"))
        im = tlo
    if im.width > MAX_W or im.height > MAX_H:
        k = min(MAX_W / im.width, MAX_H / im.height)
        im = im.convert("L").resize((max(1, int(im.width * k)), max(1, int(im.height * k))), Image.LANCZOS)
    if im.mode == "1":
        return im
    szary = im.convert("L")
    if rozpraszanie:
        return szary.convert("1", dither=Image.FLOYDSTEINBERG)
    return szary.point(lambda v: 255 if v >= prog else 0).convert("1", dither=Image.NONE)


def pbm(im):
    """Obraz trybu '1' → bajty PBM P4 (bit 1 = czarny, wiersz dopełniony do bajtu)."""
    w, h = im.size
    px = im.load()
    wiersze = bytearray()
    for y in range(h):
        bajt, n = 0, 0
        for x in range(w):
            bajt = (bajt << 1) | (1 if px[x, y] == 0 else 0)
            n += 1
            if n == 8:
                wiersze.append(bajt)
                bajt, n = 0, 0
        if n:
            wiersze.append(bajt << (8 - n))
    return f"P4\n{w} {h}\n".encode() + bytes(wiersze)


def przygotuj(wejscie, manifest, wyjscie, prog=128, rozpraszanie=False):
    """Zwraca listę pozycji manifestu panelu. Brak manifestu: wszystkie pliki obrazów jako pozycje bez tagów."""
    if w_repo(wyjscie):
        raise SystemExit("odmowa: katalog wyjścia leży w repozytorium (obrazki użytkownika poza repo)")
    os.makedirs(wyjscie, exist_ok=True)
    if manifest:
        with open(manifest, encoding="utf-8") as f:
            pozycje = json.load(f).get("obrazki", [])
    else:
        pozycje = [{"plik": n} for n in sorted(os.listdir(wejscie))
                   if n.lower().endswith((".png", ".jpg", ".jpeg", ".bmp", ".gif", ".pbm"))]
    wynik = []
    for poz in pozycje:
        plik = poz.get("plik")
        if not plik:
            continue
        zrodlo = os.path.join(wejscie, plik)
        if not os.path.isfile(zrodlo):
            print(f"pominięty {plik}: brak pliku", file=sys.stderr)
            continue
        im = do_1bit(Image.open(zrodlo), prog, rozpraszanie)
        nazwa = nazwa_panelu(plik, {p["plik"] for p in wynik})
        with open(os.path.join(wyjscie, nazwa), "wb") as f:
            f.write(pbm(im))
        p = {"plik": nazwa}
        for t in TAGI:
            p[t] = [str(v) for v in poz.get(t, []) if isinstance(v, str)]
        wynik.append(p)
    with open(os.path.join(wyjscie, "obrazki.json"), "w", encoding="utf-8") as f:
        json.dump({"wersja": 1, "obrazki": wynik}, f, ensure_ascii=False, indent=1)
    return wynik


def main():
    a = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    a.add_argument("--wejscie", required=True, help="katalog obrazków użytkownika")
    a.add_argument("--manifest", help="obrazki.json użytkownika (bez: wszystkie obrazy bez tagów)")
    a.add_argument("--wyjscie", required=True, help="katalog na pamięć panelu, poza repozytorium")
    a.add_argument("--prog", type=int, default=128, help="próg czerni dla obrazów w szarości (0–255)")
    a.add_argument("--rozpraszanie", action="store_true", help="rozpraszanie Floyda–Steinberga zamiast progu")
    a = a.parse_args()
    w = przygotuj(a.wejscie, a.manifest, a.wyjscie, a.prog, a.rozpraszanie)
    razem = sum(os.path.getsize(os.path.join(a.wyjscie, p["plik"])) for p in w)
    print(f"zapisano {len(w)} obrazków i obrazki.json w {a.wyjscie} ({razem / 1024:.0f} KB)")


if __name__ == "__main__":
    main()
