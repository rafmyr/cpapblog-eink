# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy brzegowe piku „dzwon” (14.11.2026): własne sprawdzenia kształtu piku „dzwon” (R-POR-11).

Nie jest testem pytest (nazwa bez `test_`): uruchamiany ręcznie, dane wyłącznie syntetyczne.

  python3 -I tools/portret/dzwon_brzegowe.py generuj KATALOG [--n 400] [--ziarno 1411]
      losowe noce (CPAP, AutoSet, ASV, ASVAuto; przerwy, piki przy starcie okna, linia uwagi) w formacie wzorca
      T-TEST-415 (portret-RRRR-MM-DD.csv/.txt/.pbm), do porównania z panelem: dzwon_brzegowe.cpp porownaj KATALOG
  python3 -I tools/portret/dzwon_brzegowe.py garby PLIK
      porównanie garb16() wzorca z wartościami panelu (plik z: dzwon_brzegowe.cpp garby PLIK), wszystkie h 0…432,
      d −1300…1300 s; do tego właściwości: odchyłka od krzywej Gaussa, maksimum zamiast sumy (obwiednia16)
  python3 -I tools/portret/dzwon_brzegowe.py png KATALOG
      każdy .pbm w KATALOG → .png (podgląd)
"""
import datetime as dt
import math
import os
import random
import sys

_TU = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _TU)
import przebieg  # noqa: E402
import rysunek  # noqa: E402

TRYBY = ("CPAP", "AutoSet", "ASV", "ASVAuto")


def noc_losowa(rnd, i):
    tryb = rnd.choice(TRYBY)
    zwykla = rnd.randrange(480, 900)            # 20:00–03:00
    w0 = (zwykla - 120) // 5 * 5
    sesje = []
    t = zwykla + rnd.randrange(-150, 60)
    for _ in range(rnd.randrange(1, 4)):
        dl = rnd.randrange(15, 400)
        a, b = t, min(1439, t + dl)
        if b > a:
            sesje.append((a, b))
        t = b + rnd.choice([3, 7, 12, 20, 35, 60, 120])
    if not sesje:
        sesje = [(zwykla, zwykla + 60)]
    odc = {}
    zdarz = []
    skupisko = rnd.random() < 0.5
    for a, b in sesje:
        for m in range(a // 5 * 5, b, 5):
            k = m // 5
            p = 0.08
            if skupisko and abs(m - (a + b) // 2) < 40:
                p = 0.6
            if m in (w0, w0 + 715, a // 5 * 5, (b - 1) // 5 * 5):   # piki przy starcie okna i brzegach sesji
                p = 0.5
            zew = rnd.randrange(1, 13) if rnd.random() < p else 0
            centr = rnd.randrange(1, 10) if rnd.random() < p / 2 else 0
            odc[k] = {"od": m, "do": 5, "wyciek95": rnd.randrange(0, 30), "wyc_s": rnd.choice([0, 0, 0, 30, 60, 300]),
                      "zew": zew, "centr": centr, "ps": rnd.randrange(40, 130), "maks": rnd.randrange(0, 6)}
            if tryb in ("ASV", "ASVAuto") and rnd.random() < 0.15:
                zdarz.append((m + rnd.randrange(5), 1, 0))
    data = (dt.date(2026, 11, 1) + dt.timedelta(days=i % 300)).isoformat()
    pr = {"data": data, "sesje": sesje, "odcinki": odc, "zdarzenia": zdarz, "braki": ""}
    linia = rnd.choice(["", "", "Co było inaczej? ↻ 2 s"])
    return {"przebieg": pr, "tryb": tryb, "norma_ps10": rnd.choice([50, 50, 70, None]), "zwykla": zwykla,
            "pierwszy": "Tydzień jak zwykle.", "czas": "6 h 00", "ahi": "1,0", "niepewna": False, "linia": linia,
            "naglowek": ("", "")}


def generuj(katalog, n, ziarno):
    rnd = random.Random(ziarno)
    os.makedirs(katalog, exist_ok=True)
    zrobione = 0
    for i in range(n):
        d = noc_losowa(rnd, i)
        o = rysunek.rysuj(d)
        if o is None:
            continue
        # data poranka w nazwie: rozne noce → rozne pliki; T-TEST-415 czyta nazwę `portret-RRRR-MM-DD`
        rano = (dt.date(2026, 1, 1) + dt.timedelta(days=i)).isoformat()
        baza = os.path.join(katalog, "portret-" + rano)
        with open(baza + ".csv", "w", encoding="utf-8") as f:
            f.write(przebieg.tekst_pliku(d["przebieg"]))
        with open(baza + ".txt", "w", encoding="utf-8") as f:
            f.write("tryb=%s\nnorma_ps10=%s\nzwykla=%d\npierwszy=%s\nczas=%s\nahi=%s\nniepewna=%d\nlinia=%s\n"
                    % (d["tryb"], "" if d["norma_ps10"] is None else d["norma_ps10"], d["zwykla"], d["pierwszy"],
                       d["czas"], d["ahi"], 0, d["linia"]))
            f.write("podpisy=%s\nlegenda=%s\n" % ("|".join(rysunek.podpisy(d)), "|".join(rysunek.legenda(d))))
        o.zapisz_pbm(baza + ".pbm")
        zrobione += 1
    print("noce: %d" % zrobione)


def garby(plik):
    """Plik panelu: linie `h d g` (garb16(h, d)); porównanie 1:1 ze wzorcem i z krzywą Gaussa."""
    rozne = 0
    n = 0
    maks_odch = 0.0
    with open(plik, encoding="utf-8") as f:
        for l in f:
            h, d, g = (int(v) for v in l.split())
            n += 1
            if rysunek.garb16(h, d) != g:
                rozne += 1
                if rozne < 10:
                    print("  różnica h=%d d=%d panel %d wzorzec %d" % (h, d, g, rysunek.garb16(h, d)))
            if h > 0 and abs(d) < rysunek.ZASIEG_S:
                maks_odch = max(maks_odch, abs(g - h * math.exp(-d * d / (2.0 * 420 * 420))))
    print("garb16: %d wartości, różnych panel/wzorzec %d, największa odchyłka od Gaussa %.3f/16 px" % (n, rozne, maks_odch))
    # obwiednia: maksimum (nie suma) wobec brutalnego maksimum po wszystkich odcinkach, losowe sąsiedztwa
    rnd = random.Random(7)
    zle = 0
    for _ in range(3000):
        h = {k: rnd.choice([0, 0, 0, 122, 144, 151, 194, 240, 338, 367, 432]) for k in range(100, 120)}
        u = rnd.randrange(100 * 300 - 1500, 120 * 300 + 1500)
        brut = max(rysunek.garb16(v, u - (k * 300 + 150)) for k, v in h.items())
        brut = brut if brut >= rysunek.PROG_GARBU16 else 0
        if rysunek.obwiednia16(h, u) != brut or brut > max(h.values()):
            zle += 1
    print("obwiednia16 = maksimum garbów (3000 losowych): niezgodnych %d" % zle)
    return 1 if rozne or zle else 0


def png(katalog):
    for p in sorted(os.listdir(katalog)):
        if p.endswith(".pbm"):
            rysunek.czytaj_pbm(os.path.join(katalog, p)).zapisz_png(os.path.join(katalog, p[:-4] + ".png"))


def main(a):
    if len(a) >= 2 and a[0] == "generuj":
        n = int(a[a.index("--n") + 1]) if "--n" in a else 400
        z = int(a[a.index("--ziarno") + 1]) if "--ziarno" in a else 1411
        generuj(a[1], n, z)
        return 0
    if len(a) == 2 and a[0] == "garby":
        return garby(a[1])
    if len(a) == 2 and a[0] == "png":
        png(a[1])
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
