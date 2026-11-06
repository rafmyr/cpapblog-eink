#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Eksport artboardów kanwy do statycznych PNG 800×480, 1 bit.

Po co
-----
Pliki `design/kanwa/*.dc.html` są źródłami z kanwy projektowej. 42 z nich to
szablony zależne od `support.js` środowiska kanwy, którego nie ma w repo,
więc w zwykłej przeglądarce renderują się puste albo z surowymi znacznikami.
Ten skrypt robi z każdego artboardu obraz, który obca osoba otworzy bez
kanwy: `design/png/<nazwa artboardu>.png`, 800×480 px, czerń i biel (1 bit),
tak jak na panelu e-ink (R-NF-13).

Jak działa
----------
1. `tools/mockdc.js` (Node) zamienia artboard na statyczny HTML: liczy
   wartości ze skryptu artboardu i wstawia szablon z `dc-import`.
2. Chromium (Playwright) robi zrzut obszaru 800×480.
3. Pillow zamienia zrzut na 1 bit progiem 50% jasności.

Kroje (Public Sans, Source Serif 4) przychodzą z Google Fonts; bez sieci
przeglądarka bierze kroje zastępcze (Arial, Georgia), a skrypt mówi o tym
w podsumowaniu. Obraz jest wtedy poglądowy: układ i teksty się zgadzają,
szerokość liter nie.

Użycie
------
  python3 tools/eksport_png.py                # wszystkie artboardy
  python3 tools/eksport_png.py K10 SK16       # tylko pliki zaczynające się od tych nazw
  python3 tools/eksport_png.py --wyjscie /tmp/png

Wymaga: Node, Python 3 z pakietami `playwright` (z zainstalowanym Chromium)
i `Pillow`. Kod wyjścia: 0 = wszystkie wyeksportowane, 1 = część się nie
udała, 2 = brak narzędzi.
"""

import argparse
import glob
import json
import os
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TU)
KANWA = os.path.join(REPO, "design", "kanwa")
MOCK = os.path.join(TU, "mockdc.js")
SZER, WYS = 800, 480
PROG = 128  # jasność 0–255; ciemniejsze piksele są czarne


def artboardy(filtry):
    """Pliki artboardów w kolejności z canvas.json (gdy jest), potem pozostałe."""
    pliki = sorted(glob.glob(os.path.join(KANWA, "*.dc.html")))
    sciezka_json = os.path.join(KANWA, "canvas.json")
    if os.path.exists(sciezka_json):
        with open(sciezka_json, encoding="utf-8") as f:
            kolejnosc = list(json.load(f).get("boards", {}))
        wg_nazwy = {os.path.basename(p): p for p in pliki}
        pliki = [wg_nazwy[n] for n in kolejnosc if n in wg_nazwy] + \
                [p for p in pliki if os.path.basename(p) not in kolejnosc]
    if filtry:
        pliki = [p for p in pliki if any(os.path.basename(p).startswith(f) for f in filtry)]
    return pliki


def na_html(plik, katalog_tmp):
    nazwa = os.path.basename(plik)[:-len(".dc.html")]
    wyj = os.path.join(katalog_tmp, nazwa + ".html")
    subprocess.run(["node", MOCK, plik, wyj], check=True, capture_output=True, text=True)
    return nazwa, wyj


def na_1bit(png_rgb, png_wyj):
    from PIL import Image
    obraz = Image.open(png_rgb).convert("L").crop((0, 0, SZER, WYS))
    obraz.point(lambda p: 255 if p >= PROG else 0).convert("1").save(png_wyj, optimize=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Eksport artboardów kanwy do PNG 800×480, 1 bit.")
    ap.add_argument("filtry", nargs="*", help="początki nazw plików artboardów, np. K10 SK16")
    ap.add_argument("--wyjscie", default=os.path.join(REPO, "design", "png"))
    a = ap.parse_args(argv)

    try:
        from playwright.sync_api import sync_playwright
        import PIL  # noqa: F401
    except ImportError as e:
        print("Brak narzędzia: %s (potrzebne: playwright z Chromium, Pillow)." % e, file=sys.stderr)
        return 2

    os.makedirs(a.wyjscie, exist_ok=True)
    pliki = artboardy(a.filtry)
    bledy, kroje_zastepcze = [], []
    with tempfile.TemporaryDirectory() as tmp, sync_playwright() as p:
        przegladarka = p.chromium.launch()
        strona = przegladarka.new_page(viewport={"width": SZER, "height": WYS})
        for plik in pliki:
            try:
                nazwa, html = na_html(plik, tmp)
                strona.goto("file://" + html, wait_until="networkidle", timeout=20000)
                # Zrzut dopiero po wczytaniu krojów, inaczej litery mają szerokość kroju zastępczego.
                if not strona.evaluate("document.fonts.ready.then(() => document.fonts.check('16px \"Public Sans\"'))"):
                    kroje_zastepcze.append(nazwa)
                surowy = os.path.join(tmp, nazwa + ".png")
                strona.screenshot(path=surowy, clip={"x": 0, "y": 0, "width": SZER, "height": WYS})
                na_1bit(surowy, os.path.join(a.wyjscie, nazwa + ".png"))
            except Exception as e:  # jeden zły artboard nie zatrzymuje eksportu
                bledy.append((os.path.basename(plik), str(e).splitlines()[0][:120]))
        przegladarka.close()

    print("Wyeksportowano %d z %d artboardów do %s." % (len(pliki) - len(bledy), len(pliki), a.wyjscie))
    if kroje_zastepcze:
        print("Uwaga: bez kroju Public Sans (brak sieci albo artboard go nie wczytuje), obraz poglądowy: %s"
              % ", ".join(kroje_zastepcze))
    for nazwa, opis in bledy:
        print("  błąd: %s: %s" % (nazwa, opis))
    return 1 if bledy else 0


if __name__ == "__main__":
    sys.exit(main())
