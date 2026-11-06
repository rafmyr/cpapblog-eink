#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generator zasobów prototypu panelu (firmware/prototyp/include/).

Robi dwa pliki nagłówkowe dla firmware:
  fonty.h   kroje skali B (Source Serif 4 i Public Sans, SPEC-B i SPEC-KOKPIT) jako czcionki
            Adafruit GFX, 1 bit, z polskimi znakami pod kodami 0x7F i dalej (mapowanie UTF-8 w ekrany.cpp),
            oraz szerokości znaków w 1/16 px (tablice *_adv16), żeby odstępy między literami były jak w przeglądarce
  obrazy.h  wszystkie sceny budzika z assets/budzik/karta-240x205/ (już odbite lustrzanie)
            i tabela SCENY {id, bitmapa, szerokość, wysokość} do wyboru sceny po ID (R-HUM-10, R-HUM-11);
            osobno sceny ekranu nocnego „Dobranoc” (SCENY_NOCY) ze scen źródłowych 800×480
            w assets/budzik/zrodlo/, przycięte do zawartości, przeskalowane i progowane jak szkice A1–A3

Oba pliki nie zależą od Arduino: na komputerze kompilują je testy (firmware/prototyp/test/).

Glify: rysowane w 4-krotnym powiększeniu bez hintingu, zmniejszane uśrednianiem pola i progowane
(PROG_MALY dla krojów do 24 px, PROG_DUZY dla większych). Wcześniej glif szedł z rastra 1:1 progiem 50 %, więc cienka
kreska wypadała albo zostawała zależnie od położenia na siatce pikseli: „m” traciło prawą nóżkę i wyglądało jak
„m ” (zdjęcie panelu 30.10.2026). Szerokość znaku jest ułamkowa (1/16 px), a tekst() w ekrany.cpp sumuje ją bez
zaokrąglania każdego znaku z osobna.

Kroje: pliki TTF z google/fonts (OFL), podane argumentami, nie trzymamy ich w repo.
Użycie:
  python3 tools/prototyp/generuj.py --serif SourceSerif4[opsz,wght].ttf --sans PublicSans[wght].ttf \
      --cond IBMPlexSansCondensed-Medium.ttf
  python3 tools/prototyp/generuj.py --tylko-obrazy     (same sceny, bez krojów)
"""
import argparse
import glob
import os
import re
from PIL import Image, ImageDraw, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WYJ = os.path.join(REPO, "firmware", "prototyp", "include")

# Znaki spoza ASCII, w tej kolejności dostają kody 0x7F, 0x80, ...
EXTRA = "ąćęłńóśźżĄĆĘŁŃÓŚŹŻ„”°·→↓↑–’…≥↔₂−"

# nazwa, krój, rozmiar px, waga[, tylko te znaki]
# Krój: serif (Source Serif 4), sans (Public Sans); cond (IBM Plex Sans Condensed 500) tylko jako zapas dla znaków spoza
# Source Serif i Public Sans. W skali B bez kursyw, bez Plexa i bez krojów poniżej 22 px: kursywa i cienka
# kreska znikały w 1 bicie (zdjęcie panelu 30.10.2026). Kursywy (--serif-kursywa, --sans-kursywa) i --cond-b są opcjonalne.
# Duże kroje mają tylko potrzebne znaki (pozostałe kody to puste glify), żeby oszczędzić flash:
# S140 godzina założenia maski (A3), S110 napis `Dobranoc.` w A3 bez godziny, S120 numer serii (S2-seria).
FONTY = [
    ("S140", "serif", 140, 400, "0123456789:"),
    ("S120", "serif", 120, 400, "0123456789."),
    ("S110", "serif", 110, 400, "Dobranc."),
    # skala B (docs/design/czytelnosc-b/SPEC-B.md i SPEC-KOKPIT.md, decyzja 30.10.2026): B1 SN96, B2 S120,
    # B3 SB44, B4 P34B, B5 P28B, B6 P24B, B7 P22B; w kokpicie dodatkowo SN56 (4 liczby K1)
    ("SN96", "serif", 96, 600, "0123456789,h–:"),  # liczby ostatniej nocy, duża liczba kamienia
    ("SN56", "serif", 56, 600, "0123456789,:–h"),  # 4 liczby ostatniej nocy w K1
    ("P34B", "sans", 34, 700),      # zdanie o tygodniu, `noc z rzędu`, `nie oceniam: duży wyciek`
    ("P28B", "sans", 28, 700),      # wartości paska, powód, krok, linia wzorca A; mediany K1, fakty kart i S9
    ("P24B", "sans", 24, 700),      # nagłówek, podpisy liczb, nazwy kolumn, linia uwagi; nazwy wierszy K1
    ("P24S", "sans", 24, 600),      # B6 600: wartości wierszy `Ocena:`, `Ty:`, `Dalej:`, `Koniec:` „Otwartych spraw”
    ("SB44", "serif", 44, 700),     # tytuł ekranu (S5, S6), karty, pytania (F08), S9 A1 i A2, testu
    ("SB24", "serif", 24, 700),     # zdanie K2 (F07)
    ("P22B", "sans", 22, 600),      # minimum (etykiety, legenda, stopki, cała tabela K2, pytanie)
    # U-05 „Test sprzętu” według kanwy `design/kanwa/U05-test-sprzetu.dc.html` (decyzja 04.11.2026
    # „wszystko według docelowej kanwy”): ekran serwisowy, poza skalą B ekranów dnia (minimum 22 px jej nie dotyczy).
    ("P20", "sans", 20, 400),       # nazwy wierszy, prawa część nagłówka i ramki
    ("P20B", "sans", 20, 600),      # wartości wierszy, lewa część nagłówka i ramki (dowolne znaki: nazwa sieci Wi-Fi)
    ("P16", "sans", 16, 400, "ABCDEFGHIJKLMNOPQRSTUVWXYZĄĆĘŁŃÓŚŹŻ"),  # nazwy kolumn wersalikami
    ("P18B", "sans", 18, 600, "abcdefghijklmnopqrstuvwxyząćęłńóśźż0123456789·"),  # stopka (ikony ◀ ↻ rysuje ekrany.cpp)
]
# kolejność zapasowych krojów dla znaków, których brak w kroju głównym (→ w Public Sans z szeryfu; ↔ ↑ ↓ rysuje ekrany.cpp)
ZAPAS = {"serif": ["sans", "cond"], "kursywa": ["serif", "sans", "cond"], "sans": ["serif", "cond"],
         "sans-kursywa": ["sans", "serif", "cond"],
         "cond": ["sans", "serif"], "cond-b": ["cond", "sans", "serif"]}
SS = 4            # powiększenie rastra glifu
PROG_MALY = 100   # próg czerni (0–255) dla krojów do 24 px: kreska 1,6 px nie znika na siatce pikseli
PROG_DUZY = 128

KATALOG_SCEN = os.path.join(REPO, "assets", "budzik", "karta-240x205")
KATALOG_ZRODEL = os.path.join(REPO, "assets", "budzik", "zrodlo")

# Ekran nocny „Dobranoc” (ADR-0018): wariant, scena, ramka (szer., wys.), lustro. Jak w szkicach A1–A3:
# A1 jak karta (240×205, odbite), A2 duża scena bez lustra (do 600×295: wyższa nachodzi na `Dobranoc.`),
# A3 ok. 230×250 odbite.
SCENY_NOCY = [
    ("A1", "W12", 240, 205, True),
    ("A1", "G14", 240, 205, True),
    ("A1", "G11", 240, 205, True),
    ("A2", "G10", 600, 295, False),
    ("A2", "G13", 600, 295, False),
    ("A2", "G15", 600, 295, False),
    ("A3", "G12", 230, 250, True),
    ("A3", "W12", 230, 250, True),
]

# Pierwsze linie generowanych nagłówków: źródło i licencja (NOTICE pkt 1 i 2). Test tools/test_licencje.py pilnuje,
# że pliki w repo zaczynają się od tych linii.
# REUSE-IgnoreStart (znacznik SPDX jako tekst w kodzie, nie licencja tego pliku)
LICENCJA_FONTY = ("// SPDX-License-Identifier: OFL-1.1\n"
                  "// Copyright 2014 The Source Serif 4 Project Authors (https://github.com/adobe-fonts/source-serif)\n"
                  "// Copyright 2015 The Public Sans Project Authors (https://github.com/uswds/public-sans)\n"
                  "// Copyright © 2017 IBM Corp. with Reserved Font Name \"Plex\"\n"
                  "// Wygenerowane przez tools/prototyp/generuj.py z krojów Source Serif 4, Public Sans i IBM Plex Sans\n"
                  "// Condensed; bitmapy krojów na licencji SIL Open Font License 1.1 (LICENSES/OFL-1.1.txt, NOTICE pkt 2).\n"
                  "// Nazwy krojów w tym pliku nie zawierają zastrzeżonej nazwy „Plex”.\n"
                  "// Nie edytuj ręcznie: zmień generator albo kroje i uruchom tools/prototyp/generuj.py.")
LICENCJA_FONTY_SLOT = ("// SPDX-License-Identifier: OFL-1.1\n"
                       "// Copyright 2014 The Source Serif 4 Project Authors (https://github.com/adobe-fonts/source-serif)\n"
                       "// Copyright 2015 The Public Sans Project Authors (https://github.com/uswds/public-sans)\n"
                       "// Wygenerowane przez tools/prototyp/generuj_slot.py z krojów Source Serif 4 i Public Sans;\n"
                       "// bitmapy krojów na licencji SIL Open Font License 1.1 (LICENSES/OFL-1.1.txt, NOTICE pkt 2).\n"
                       "// Nie edytuj ręcznie: zmień generator albo kroje i uruchom tools/prototyp/generuj_slot.py.")
LICENCJA_OBRAZY = ("// SPDX-License-Identifier: LicenseRef-cpapblog-eink-budzik\n"
                   "// SPDX-FileCopyrightText: 2026 Rafał Myrta\n"
                   "// Wygenerowane przez tools/prototyp/generuj.py ze scen z assets/budzik/. Grafiki budzika są poza\n"
                   "// licencją GPL: warunki w LICENSES/LicenseRef-cpapblog-eink-budzik.txt (NOTICE pkt 1).\n"
                   "// Nie edytuj ręcznie: zmień sceny w assets/budzik/ i uruchom tools/prototyp/generuj.py --tylko-obrazy.")
# REUSE-IgnoreEnd

NAGLOWEK = """#pragma once
#include <stdint.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif
#ifndef PROGMEM
#define PROGMEM
#endif"""


def sceny():
    """ID sceny (B01, M01, ...) -> ścieżka pliku; ID to początek nazwy pliku (np. M01_medal-konfetti-240x205.png)."""
    wynik = {}
    for p in sorted(glob.glob(os.path.join(KATALOG_SCEN, "*-240x205.png"))):
        m = re.match(r"([A-Z]\d\d)", os.path.basename(p))
        if m:
            assert m.group(1) not in wynik, "dwa pliki tej samej sceny: " + m.group(1)
            wynik[m.group(1)] = p
    return wynik


def wczytaj(sciezka, px, waga, opsz=None):
    """Krój w rozmiarze px; opsz = rozmiar optyczny (oś opsz Source Serif 4), domyślnie px."""
    f = ImageFont.truetype(sciezka, px)
    try:
        osie = f.get_variation_axes()
    except OSError:  # krój statyczny (IBM Plex Sans Condensed)
        return f
    wart = []
    for o in osie:
        n = o["name"].decode() if isinstance(o["name"], bytes) else o["name"]
        if n.lower().startswith("weight"):
            wart.append(waga)
        elif n.lower().startswith("optical"):
            wart.append(max(o["minimum"], min(o["maximum"], opsz or px)))
        else:
            wart.append(o["default"])
    f.set_variation_by_axes(wart)
    return f


def glif(f4, ch, prog):
    """f4: krój w rozmiarze SS × px. Zwraca (bity, w, h, xOff, yOff) względem linii bazowej, po przycięciu pustych
    brzegów. Raster w powiększeniu SS, zmniejszenie uśrednianiem pola (jak wygładzanie w przeglądarce), próg `prog`."""
    l, t, r, b = f4.getbbox(ch, anchor="ls")
    if r <= l or b <= t:
        return [], 0, 0, 0, 0
    xl, xr = l // SS - 1, -(-r // SS) + 1
    yt, yb = t // SS - 1, -(-b // SS) + 1
    im = Image.new("L", ((xr - xl) * SS, (yb - yt) * SS), 0)
    ImageDraw.Draw(im).text((-xl * SS, -yt * SS), ch, font=f4, fill=255, anchor="ls")
    im = im.resize((xr - xl, yb - yt), Image.BOX)
    w, h = im.size
    px = im.load()
    pelne = [(x, y) for y in range(h) for x in range(w) if px[x, y] >= prog]
    if not pelne:
        return [], 0, 0, 0, 0
    x0, x1 = min(p[0] for p in pelne), max(p[0] for p in pelne)
    y0, y1 = min(p[1] for p in pelne), max(p[1] for p in pelne)
    bity = [1 if px[x, y] >= prog else 0 for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)]
    return bity, x1 - x0 + 1, y1 - y0 + 1, xl + x0, yt + y0


def cmap(sciezka):
    from fontTools.ttLib import TTFont
    return TTFont(sciezka).getBestCmap()


def font_c(nazwa, kroje, mapy, px, prog, tylko=None):
    """kroje: lista (krój 1x, krój SS×) w kolejności: główny, potem zapasowe (dla znaków, których brak w głównym);
    mapy: zestawy kodów tych krojów. tylko: jeśli podane, glify tylko dla tych znaków (i spacji), reszta to puste
    glify (kody bez zmian)."""
    znaki = [chr(c) for c in range(0x20, 0x7F)] + list(EXTRA)
    f1 = kroje[0][0]
    bajty, glify, adv16 = [], [], []
    for ch in znaki:
        zrodlo = None
        for (k1, k4), m in zip(kroje, mapy):
            if ord(ch) in m:
                zrodlo = (k1, k4)
                break
        if tylko is not None and ch not in tylko and ch != " ":
            zrodlo = None
        if zrodlo is None:
            # brakujący znak: pusty glif o szerokości spacji, żeby kody się nie przesunęły
            a = f1.getlength(" ")
            glify.append((len(bajty), 0, 0, round(a), 0, 0, ch))
            adv16.append(round(a * 16))
            continue
        a = zrodlo[1].getlength(ch) / SS
        bity, w, h, xo, yo = glif(zrodlo[1], ch, prog)
        off = len(bajty)
        for i in range(0, len(bity), 8):
            chunk = bity[i:i + 8] + [0] * (8 - len(bity[i:i + 8]))
            bajty.append(int("".join(map(str, chunk)), 2))
        glify.append((off, w, h, round(a), xo, yo, ch))
        adv16.append(round(a * 16))
    assert len(bajty) < 65536, nazwa
    asc = -min(g[5] for g in glify if g[2])
    desc = max(g[5] + g[2] for g in glify if g[2])
    lh = round((asc + desc) * 1.15)
    out = [f"// {nazwa}: wygenerowane przez tools/prototyp/generuj.py, nie edytować ręcznie ({px} px, próg {prog})"]
    out.append(f"const uint8_t {nazwa}_bmp[] PROGMEM = {{")
    for i in range(0, len(bajty), 24):
        out.append("  " + ",".join(f"0x{b:02X}" for b in bajty[i:i + 24]) + ",")
    out.append("};")
    out.append(f"const GFXglyph {nazwa}_gl[] PROGMEM = {{")
    for off, w, h, adv, xo, yo, ch in glify:
        kom = ch if ch not in "\\" else "backslash"
        out.append(f"  {{{off},{w},{h},{adv},{xo},{yo}}}, // {kom}")
    out.append("};")
    last = 0x20 + len(glify) - 1
    out.append(f"const GFXfont {nazwa} PROGMEM = {{(uint8_t*){nazwa}_bmp,(GFXglyph*){nazwa}_gl,0x20,0x{last:02X},{lh}}};")
    out.append(f"// szerokość znaków w 1/16 px (bez zaokrąglania każdego znaku do piksela)")
    out.append(f"const uint16_t {nazwa}_adv16[] PROGMEM = {{")
    for i in range(0, len(adv16), 24):
        out.append("  " + ",".join(str(v) for v in adv16[i:i + 24]) + ",")
    out.append("};")
    brak = [g[6] for g in glify if g[1] == 0 and g[6] != " " and (tylko is None or g[6] in tylko)]
    return "\n".join(out), brak, len(bajty)


def bity_c(nazwa, opis, im):
    """Obraz 1-bit (0 = czarny) → tablica C, wiersze dopełnione do bajtu, bit 1 = czarny."""
    w, h = im.size
    px = im.load()
    bajty = []
    for y in range(h):
        for x0 in range(0, w, 8):
            b = 0
            for k in range(8):
                x = x0 + k
                czarny = x < w and px[x, y] == 0
                b = (b << 1) | (1 if czarny else 0)
            bajty.append(b)
    out = [f"// {opis}, bit 1 = czarny"]
    out.append(f"const uint16_t {nazwa}_W = {w}, {nazwa}_H = {h};")
    out.append(f"const uint8_t {nazwa}[] PROGMEM = {{")
    for i in range(0, len(bajty), 24):
        out.append("  " + ",".join(f"0x{b:02X}" for b in bajty[i:i + 24]) + ",")
    out.append("};")
    return "\n".join(out), len(bajty)


def obraz_c(n, p):
    im = Image.open(p).convert("1")
    assert im.size[0] <= 240 and im.size[1] <= 205, f"{n}: {im.size} większe niż slot karty 240x205"
    return bity_c(f"BUDZIK_{n}", f"scena {n} z assets/budzik/karta-240x205/{os.path.basename(p)}", im)


def zrodlo_sceny(n):
    pliki = [p for p in sorted(glob.glob(os.path.join(KATALOG_ZRODEL, n + "*.png")))
             if re.match(n + r"[._]", os.path.basename(p))]
    assert len(pliki) == 1, f"scena źródłowa {n}: {pliki}"
    return pliki[0]


def sprite(p, bw, bh, lustro):
    """Jak szkice A1–A3: przycięcie do zawartości (wiersze i kolumny z więcej niż 2 czarnymi pikselami),
    lustro, skalowanie do ramki bw×bh z zachowaniem proporcji (LANCZOS), próg 140."""
    import numpy as np
    from PIL import ImageOps
    a = np.array(Image.open(p).convert("L")) < 128
    wiersze = np.where(a.sum(1) > 2)[0]
    kolumny = np.where(a.sum(0) > 2)[0]
    assert len(wiersze) and len(kolumny), f"{p}: brak czarnych pikseli (pusty albo jasny obraz), nie ma czego przyciąć"
    c = Image.fromarray(((~a[wiersze[0]:wiersze[-1] + 1, kolumny[0]:kolumny[-1] + 1]) * 255).astype("uint8"))
    if lustro:
        c = ImageOps.mirror(c)
    s = min(bw / c.width, bh / c.height)
    c = c.resize((round(c.width * s), round(c.height * s)), Image.LANCZOS)
    return c.point(lambda v: 255 if v >= 140 else 0).convert("1")


def obrazy_h():
    sc = sceny()
    czesci, bajty = [LICENCJA_OBRAZY, NAGLOWEK], 0
    for n, p in sc.items():
        c, b = obraz_c(n, p)
        czesci.append(c)
        bajty += b
    czesci.append("struct SceneBudzika { const char* id; const uint8_t* bmp; uint16_t w, h; };")
    czesci.append("static const SceneBudzika SCENY[] = {")
    for n in sc:
        czesci.append(f'  {{"{n}", BUDZIK_{n}, BUDZIK_{n}_W, BUDZIK_{n}_H}},')
    czesci.append("};")
    czesci.append(f"static const int SCENY_N = {len(sc)};")
    # ekran nocny „Dobranoc”: osobna tabela poza rotacją ekranu dnia (sceny() i R-HUM-11 jej nie widzą)
    bajty_nocy = 0
    for war, n, bw, bh, lustro in SCENY_NOCY:
        p = zrodlo_sceny(n)
        opis = f"ekran nocny {war}: scena {n} z assets/budzik/zrodlo/{os.path.basename(p)}, ramka {bw}x{bh}"
        c, b = bity_c(f"NOC_{war}_{n}", opis + (", lustro" if lustro else ""), sprite(p, bw, bh, lustro))
        czesci.append(c)
        bajty_nocy += b
    czesci.append("static const SceneBudzika SCENY_NOCY[] = {")
    for war, n, *_ in SCENY_NOCY:
        czesci.append(f'  {{"{war}-{n}", NOC_{war}_{n}, NOC_{war}_{n}_W, NOC_{war}_{n}_H}},')
    czesci.append("};")
    czesci.append(f"static const int SCENY_NOCY_N = {len(SCENY_NOCY)};")
    open(os.path.join(WYJ, "obrazy.h"), "w").write("\n".join(czesci) + "\n")
    print(f"obrazy.h: {len(sc)} scen, {bajty} B ({bajty / 1024:.0f} KB) w pamięci flash: {', '.join(sc)}")
    print(f"obrazy.h: ekran nocny, {len(SCENY_NOCY)} scen, {bajty_nocy} B ({bajty_nocy / 1024:.0f} KB)")


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--serif")
    a.add_argument("--serif-kursywa")
    a.add_argument("--sans")
    a.add_argument("--sans-kursywa")
    a.add_argument("--cond")
    a.add_argument("--cond-b")
    a.add_argument("--tylko-obrazy", action="store_true", help="tylko obrazy.h, bez krojów (nie trzeba plików TTF)")
    a = a.parse_args()
    os.makedirs(WYJ, exist_ok=True)
    obrazy_h()
    if a.tylko_obrazy:
        return
    pliki = {"serif": a.serif, "kursywa": a.serif_kursywa, "sans": a.sans, "sans-kursywa": a.sans_kursywa,
             "cond": a.cond, "cond-b": a.cond_b}
    potrzebne = {k for _, kroj, *_ in FONTY for k in [kroj] + ZAPAS[kroj]}
    if not all(pliki[k] for k in potrzebne):
        raise SystemExit("podaj pliki krojów: " + ", ".join("--" + k for k in sorted(potrzebne)) + " albo --tylko-obrazy")
    # gfxfont.h z Adafruit GFX (na komputerze: atrapa w firmware/prototyp/test/atrapy/)
    czesci = [LICENCJA_FONTY, NAGLOWEK, "#include <gfxfont.h>",
              f'// znaki dodatkowe od 0x7F: {EXTRA}',
              f'static const char FONT_EXTRA[] = "{EXTRA}";']
    mapy = {k: cmap(v) for k, v in pliki.items() if v}
    razem = 0
    for nazwa, kroj, px, waga, *tylko in FONTY:
        kolejnosc = [kroj] + ZAPAS[kroj]
        kroje = [(wczytaj(pliki[k], px, waga), wczytaj(pliki[k], px * SS, waga, opsz=px)) for k in kolejnosc]
        prog = PROG_MALY if px <= 24 else PROG_DUZY
        c, brak, b = font_c(nazwa, kroje, [mapy[k] for k in kolejnosc], px, prog, tylko[0] if tylko else None)
        if brak:
            print(f"{nazwa}: brak glifów {''.join(brak)} (puste miejsce)")
        czesci.append(c)
        razem += b
    czesci.append("// kroje z szerokościami 1/16 px; ekrany.cpp szuka tu kroju po wskaźniku")
    czesci.append("struct KrojAdv16 { const GFXfont* f; const uint16_t* adv16; };")
    czesci.append("static const KrojAdv16 KROJE_ADV16[] = {")
    for nazwa, *_ in FONTY:
        czesci.append(f"  {{&{nazwa}, {nazwa}_adv16}},")
    czesci.append("};")
    czesci.append(f"static const int KROJE_N = {len(FONTY)};")
    open(os.path.join(WYJ, "fonty.h"), "w").write("\n".join(czesci) + "\n")
    print(f"zapisano {os.path.join(WYJ, 'fonty.h')}: {len(FONTY)} krojów, bitmapy {razem} B ({razem / 1024:.0f} KB)")


if __name__ == "__main__":
    main()
