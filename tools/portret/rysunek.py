# SPDX-License-Identifier: GPL-3.0-or-later
"""Rysunek Portretu nocy (spec 5.4.16, R-POR-06…R-POR-16): wzorzec wyglądu 800×480, 1 bit.

Geometria liczona w liczbach zmiennoprzecinkowych (math.atan2, math.sqrt) jako niezależny wzorzec; panel
(firmware/prototyp/src/portret_ekran.cpp) liczy ją arytmetyką całkowitą z tablicą kątów, a obrazki porównuje test
w tolerancji pikseli (T-TEST-415). Tekst krojami panelu z fonty.h (kroje.py), więc litery wychodzą piksel w piksel.

Wejście `Dane` (słownik):
  przebieg      wynik przebieg.przebieg_doby() albo przebieg.czytaj_plik()
  tryb          "CPAP", "AutoSet", "ASV" albo "ASVAuto"
  norma_ps10    mediana normy M-28 w 0,1 cmH2O albo None (rozruch metryki: „Wsparcie oddechu: uczę się.”)
  zwykla        zwykła pora startu terapii w minutach od 12:00 dnia `day` (mediana M-18, definicje 5.4.16)
  pierwszy      pierwszy podpis (zdanie o normie z reguł S2a)
  czas, ahi     liczby nocy (`6 h 38`, `1,1`; brak AHI = `–`)
  niepewna      noc niepewna (4.9): liczby `{czas} z maską` bez AHI, pierwszy podpis `Tej nocy nie oceniam: duży wyciek.`
  linia         tekst linii uwagi albo "" (tarcza w skali 0,85, najwyżej 1 podpis po pierwszym)
  naglowek      (lewo, prawo) nagłówka; tylko podgląd, poza obszarem porównania
"""

import math
import os
import sys

_TU = os.path.dirname(os.path.abspath(__file__))
if _TU not in sys.path:
    sys.path.insert(0, _TU)

import kroje  # noqa: E402

W, H = 800, 480
# Układ (spec 5.4.16 „Układ”)
X_TARCZY = 200
GORA_TRESCI = 54
DOL_TRESCI = 434
WYS_LINII = 48            # linia uwagi B6 (ekrany.cpp liniaUwagiEkranu)
SKALA_LINII = 85          # tarcza w skali 0,85 przy linii uwagi [PRÓG], w setnych
R_CYFR = 82
R_KRESKI = (77, 87)
R_PIK_WEWN_MIN = 98       # piki do środka najwyżej do tego promienia (20 px od pierścienia)
R_KROPKI = 110            # kropki zdarzeń ASV (koło 8 px)
R_PIERSCIEN = (118, 134)
R_BIALY = (121, 131)      # wyciek: biały środek, obrys 3 px
R_SLAD = 126
R_PIK_MAX = 170
R_ZNACZNIK = (110, 150)   # znacznik startu okna: kreska od 8 px pod pierścieniem do 16 px nad nim (zmiana 2026-11-12 (2))
ZN_POL_SZER = 2.25        # połowa szerokości kreski (kreska 4–5 px)
ZN_PRZERWA = 4.25         # biała przerwa w pierścieniu do 4,25 px od osi kreski (2 px po każdej stronie)
GROT_CZUBEK = 15.0        # czubek grota 15 px od osi kreski w kierunku ruchu wskazówek zegara, podstawa 14 px
R_ZEWN = 172              # zasięg testu biegunowego (T-WYM-97: od 96 do 172)
R_WEWN = 96
KOL_X, KOL_W = 390, 382
PIK_ZEWN_PX, PIK_ZEWN_MIN_PX, PIK_WEWN_PX = 36.0, 12.0, 20.0
OGRANICZENIE_K = 7
PROG_WSPARCIA10 = 20      # +2 cmH2O [PRÓG]
POL_PODSTAWY_S = 300      # podstawa garbu 10 min (±5 min) [PRÓG]
OKNO_H_MIN = 720
PRZED_STARTEM_MIN = 120
NAJGESCIEJ_MIN, NAJGESCIEJ_PIKOW = 60, 3
PRZERWA_MIN = 10
MAKSIMUM_MIN = 5
MIN_TERAPII = 15          # R-POR-16
PROG_WYCIEKU = 15         # L/min: próg próbki wycieku (pole `wyc_s`, legenda `wyciek od 15 l/min`)
BIALY_WYC_S = 60          # biały odcinek od 60 s próbek ≥ 15 L/min w odcinku [PRÓG] (12.11.2026)

ASV = ("ASV", "ASVAuto")

LEGENDA = {
    "maska": "maska na twarzy",
    "wyciek": "wyciek od 15 l/min",
    "wsparcie": "większe wsparcie oddechu",
    "zdarzenia": "zdarzenia w 5 minutach",
    "centralne": "zdarzenia centralne",
    "kropka": "pojedyncze zdarzenie",
}


class Obraz:
    def __init__(self, w=W, h=H):
        self.w, self.h = w, h
        self.px = bytearray(w * h)  # 1 = czarny

    def piksel(self, x, y, czarny=True):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y * self.w + x] = 1 if czarny else 0

    def czarny(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h and self.px[y * self.w + x] == 1

    def prostokat(self, x, y, w, h, czarny=True):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.piksel(xx, yy, czarny)

    def linia(self, x0, y0, x1, y1, czarny=True):  # Bresenham jak Plotno::linia()
        dx = abs(x1 - x0)
        sx = 1 if x0 < x1 else -1
        dy = -abs(y1 - y0)
        sy = 1 if y0 < y1 else -1
        e = dx + dy
        while True:
            self.piksel(x0, y0, czarny)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * e
            if e2 >= dy:
                e += dy
                x0 += sx
            if e2 <= dx:
                e += dx
                y0 += sy

    def kolo(self, cx, cy, r, czarny=True):  # wypełnione, jak Plotno::kolo()
        for y in range(-r, r + 1):
            for x in range(-r, r + 1):
                if x * x + y * y <= r * r + r:
                    self.piksel(cx + x, cy + y, czarny)

    def trojkat(self, x0, y0, x1, y1, x2, y2, czarny=True):
        def kr(ax, ay, bx, by, px, py):
            return (bx - ax) * (py - ay) - (by - ay) * (px - ax)
        for y in range(min(y0, y1, y2), max(y0, y1, y2) + 1):
            for x in range(min(x0, x1, x2), max(x0, x1, x2) + 1):
                a, b, d = kr(x0, y0, x1, y1, x, y), kr(x1, y1, x2, y2, x, y), kr(x2, y2, x0, y0, x, y)
                if (a >= 0 and b >= 0 and d >= 0) or (a <= 0 and b <= 0 and d <= 0):
                    self.piksel(x, y, czarny)

    def pbm(self):
        rzad = (self.w + 7) // 8
        out = bytearray(b"P4\n%d %d\n" % (self.w, self.h))
        for y in range(self.h):
            b = bytearray(rzad)
            for x in range(self.w):
                if self.px[y * self.w + x]:
                    b[x >> 3] |= 0x80 >> (x & 7)
            out += b
        return bytes(out)

    def zapisz_pbm(self, sciezka):
        with open(sciezka, "wb") as f:
            f.write(self.pbm())

    def zapisz_png(self, sciezka):
        from PIL import Image  # Pillow tylko do podglądu (requirements-dev.txt)
        im = Image.new("1", (self.w, self.h), 1)
        im.putdata([0 if v else 1 for v in self.px])
        im.save(sciezka)


def czytaj_pbm(sciezka):
    with open(sciezka, "rb") as f:
        t = f.read()
    czesci = t.split(b"\n", 2)
    w, h = (int(x) for x in czesci[1].split())
    dane = czesci[2]
    o = Obraz(w, h)
    rzad = (w + 7) // 8
    for y in range(h):
        for x in range(w):
            if dane[y * rzad + (x >> 3)] & (0x80 >> (x & 7)):
                o.px[y * w + x] = 1
    return o


# ---------------------------------------------------------------------------
# Liczby Portretu (te same reguły co src/portret.cpp)
# ---------------------------------------------------------------------------
def hhmm(u):
    """Minuty od 12:00 → `hh:mm` czasu zegarowego."""
    c = (u + 720) % 1440
    return "%02d:%02d" % (c // 60, c % 60)


def okno(dane):
    """Początek okna tarczy w minutach od 12:00 (wielokrotność 5). gdy najdłuższa sesja nie mieści się w oknie
    w całości, okno od jej startu minus 2 h (koniec sesji w oknie); sesja dłuższa niż 12 h: od jej startu."""
    w = (dane["zwykla"] - PRZED_STARTEM_MIN) // 5 * 5
    sesje = dane["przebieg"]["sesje"]
    if sesje:
        najd = None
        for s in sesje:  # pierwsza najdłuższa (jak panel)
            if najd is None or s[1] - s[0] > najd[1] - najd[0]:
                najd = s
        if najd[0] < w or najd[1] > w + OKNO_H_MIN:
            w = (najd[0] - PRZED_STARTEM_MIN) // 5 * 5
            if w + OKNO_H_MIN < najd[1]:
                w = -((-(najd[1] - OKNO_H_MIN)) // 5 * 5)
            if w > najd[0]:
                w = najd[0] // 5 * 5
    return w


def minuty_terapii(pr):
    return sum(max(0, b - a) for a, b in pr["sesje"])


def jest_portret(pr):
    """R-POR-16: noc bez terapii (sesje razem < 15 min) nie ma Portretu."""
    return minuty_terapii(pr) >= MIN_TERAPII


def _w_oknie(k, w):
    return w <= k * 5 < w + OKNO_H_MIN


def bialy_odcinek(x):
    """odcinek biały, gdy liczy się do wycieku (co najmniej 30 poprawnych próbek, w pliku `wyciek95` niepuste)
    i ma co najmniej 60 s próbek `Leak` ≥ 15 L/min (pole `wyc_s`)."""
    return x["wyciek95"] is not None and x["wyc_s"] is not None and x["wyc_s"] >= BIALY_WYC_S


def _sesja_w_odcinku(pr, od):
    return any(a < od + 5 and b > od for a, b in pr["sesje"])


def bialy_na_rysunku(pr, k, x, w):
    """Odcinek biały widoczny na tarczy: w oknie i z częścią w sesji (ten sam warunek: rysunek, podpis, legenda)."""
    return _w_oknie(k, w) and bialy_odcinek(x) and _sesja_w_odcinku(pr, x["od"])


def wysokosc_zewn(dane, x):
    """Wysokość piku na zewnątrz odcinka w px (skala 1) albo 0."""
    if dane["tryb"] in ASV:
        if dane["norma_ps10"] is None or x["ps"] is None or "P" in dane["przebieg"]["braki"]:
            return 0.0
        d10 = x["ps"] - dane["norma_ps10"]
        if d10 < PROG_WSPARCIA10:
            return 0.0
        return PIK_ZEWN_PX * min(1.0, 0.45 + 0.55 * (d10 / 10.0 - 2.0) / 4.0)
    k = x["zew"] or 0
    if k <= 0:
        return 0.0
    return max(PIK_ZEWN_MIN_PX, PIK_ZEWN_PX * (0.35 + 0.65 * (min(k, OGRANICZENIE_K) - 1) / 6.0))


def wysokosc_wewn(dane, x):
    if dane["tryb"] in ASV:
        return 0.0
    k = x["centr"] or 0
    if k <= 0:
        return 0.0
    return PIK_WEWN_PX * (0.45 + 0.55 * (min(k, OGRANICZENIE_K) - 1) / 6.0)


def pierwszy_podpis(dane):
    """R-POR-25: po nocy niepewnej opis tej nocy zamiast zdania o tygodniu."""
    return "Tej nocy nie oceniam: duży wyciek." if dane["niepewna"] else dane["pierwszy"]


def braki(dane):
    """Podpisy o brakującym pomiarze („B”) w kolejności tabeli „Podpisy”: (pełny podpis, krótka nazwa)."""
    br = dane["przebieg"]["braki"]
    tryb = dane["tryb"]
    b = []
    if tryb in ASV:
        if "P" in br:
            b.append(("Wsparcie oddechu: brak odczytu.", "wsparcie"))
        elif dane["norma_ps10"] is None:
            b.append(("Wsparcie oddechu: uczę się.", "norma wsparcia"))
    if "L" in br:
        b.append(("Wyciek: brak odczytu.", "wyciek"))
    if "E" in br:
        b.append(("Zdarzenia: brak odczytu.", "zdarzenia"))
    if tryb not in ASV and "R" in br:
        b.append(("Rodzaj bezdechów: brak zapisu.", "rodzaj bezdechów"))
    if "M" in br and tryb in ("AutoSet", "ASVAuto"):
        b.append(("Maksimum ciśnienia: nieznane." if tryb == "AutoSet" else "Maksimum EPAP: nieznane.", "maksimum"))
    return b


def podpisy_brakow(dane, miejsc):
    """(R-POR-27): podpisy „B” na `miejsc` miejsc; przy większej liczbie braków pierwsze miejsc − 1 pełnym podpisem,
    reszta w podpisie zbiorczym `Nieznane: {lista}.` (zawsze nazwy, w kolumnie najwyżej 2 linie po 382 px)."""
    b = braki(dane)
    if len(b) <= miejsc:
        return [x[0] for x in b]
    w = [x[0] for x in b[:miejsc - 1]]
    reszta = b[miejsc - 1:]
    return w + ["Nieznane: %s." % ", ".join(x[1] for x in reszta)]


def podpisy(dane):
    """Lista podpisów (pierwszy, „B”, 2–7), przycięta do 4 (z linią uwagi do 2)."""
    pr = dane["przebieg"]
    tryb = dane["tryb"]
    w = okno(dane)
    br = pr["braki"]
    limit = 2 if dane.get("linia") else 4
    p = [pierwszy_podpis(dane)] + podpisy_brakow(dane, limit - 1)
    odc = pr["odcinki"]
    piki = sorted(k for k, x in odc.items() if _w_oknie(k, w) and wysokosc_zewn(dane, x) > 0)
    najl, najs = 0, None
    for j in range(w, w + OKNO_H_MIN - NAJGESCIEJ_MIN + 5, 5):
        n = sum(1 for k in piki if j <= k * 5 < j + NAJGESCIEJ_MIN)
        if n > najl:
            najl, najs = n, j
    if najl >= NAJGESCIEJ_PIKOW:
        p.append("Najgęściej %s–%s" % (hhmm(najs), hhmm(najs + NAJGESCIEJ_MIN)))
    if "L" not in br:
        s = sum(x["wyc_s"] for k, x in odc.items() if bialy_na_rysunku(pr, k, x, w))  # tylko odcinki białe
        m = int(math.floor(s / 60.0 + 0.5))
        if m >= 1:
            p.append("Wyciek przez %d min" % m)
    ses = sorted((max(a, w), min(b, w + OKNO_H_MIN)) for a, b in pr["sesje"] if b > w and a < w + OKNO_H_MIN)
    przerwy = [(ses[i][1], ses[i + 1][0]) for i in range(len(ses) - 1) if ses[i + 1][0] - ses[i][1] >= PRZERWA_MIN]
    if len(przerwy) == 1:
        p.append("Przerwa %s–%s" % (hhmm(przerwy[0][0]), hhmm(przerwy[0][1])))
    elif przerwy:
        p.append("Przerwy: %d, najdłuższa %d min" % (len(przerwy), max(b - a for a, b in przerwy)))
    if tryb not in ASV and "E" not in br:
        c = sum(x["centr"] or 0 for k, x in odc.items() if _w_oknie(k, w))
        if c >= 1:
            p.append("Centralne: %d" % c)
    if tryb in ("AutoSet", "ASVAuto"):
        mk = sum(x["maks"] or 0 for k, x in odc.items() if _w_oknie(k, w))
        if mk >= MAKSIMUM_MIN:
            p.append("%s na maksimum %d min" % ("Ciśnienie" if tryb == "AutoSet" else "EPAP", mk))
    poza = [s for s in pr["sesje"] if s[1] <= w or s[0] >= w + OKNO_H_MIN]
    czesc = [s for s in pr["sesje"] if s not in poza and (s[0] < w or s[1] > w + OKNO_H_MIN)]
    if czesc:
        p.append("Część nocy poza tarczą")
    if poza:
        p.append("Drzemka poza tarczą")
    return p[:limit]


def legenda(dane):
    """Klucze znaków obecnych na rysunku, w kolejności „Język rysunku”."""
    pr = dane["przebieg"]
    w = okno(dane)
    odc = [x for k, x in pr["odcinki"].items() if _w_oknie(k, w)]
    l = ["maska"]
    if any(bialy_na_rysunku(pr, k, x, w) for k, x in pr["odcinki"].items()):
        l.append("wyciek")
    if any(wysokosc_zewn(dane, x) > 0 for x in odc):
        l.append("wsparcie" if dane["tryb"] in ASV else "zdarzenia")
    if any(wysokosc_wewn(dane, x) > 0 for x in odc):
        l.append("centralne")
    if dane["tryb"] in ASV and any(w <= z[0] < w + OKNO_H_MIN for z in pr["zdarzenia"]):
        l.append("kropka")
    return l


def _w_sesji(pr, u_s):
    return any(a * 60 <= u_s < b * 60 for a, b in pr["sesje"])


# ---------------------------------------------------------------------------
# Rysowanie
# ---------------------------------------------------------------------------
def _r(v, skala):
    return (v * skala + 50) // 100


def rysuj_tarcze(o, dane, cy, skala):
    # pozycje punktów zaokrąglane połówką w górę z zapasem 1e-9 (sin 330° w liczbach zmiennoprzecinkowych daje
    # −0,50000000000000001 zamiast −0,5; panel liczy tablicą całkowitą)
    pr = dane["przebieg"]
    w = okno(dane)
    s = skala / 100.0
    r_in, r_out = _r(R_PIERSCIEN[0], skala), _r(R_PIERSCIEN[1], skala)
    b_in, b_out = _r(R_BIALY[0], skala), _r(R_BIALY[1], skala)
    r_min, r_max = _r(R_WEWN, skala), _r(R_ZEWN, skala)
    cx = X_TARCZY
    odc = pr["odcinki"]
    h_zewn = {k: wysokosc_zewn(dane, x) * s for k, x in odc.items() if _w_oknie(k, w)}
    h_wewn = {k: wysokosc_wewn(dane, x) * s for k, x in odc.items() if _w_oknie(k, w)}
    biale = {k for k, x in odc.items() if bialy_odcinek(x)}

    def obwiednia(h, u):
        k0 = int(u // 300)
        m = 0.0
        for k in (k0 - 1, k0, k0 + 1):
            hk = h.get(k, 0.0)
            if hk <= 0:
                continue
            d = abs(u - (k * 300 + 150))
            if d < POL_PODSTAWY_S:
                m = max(m, hk * math.sqrt(1.0 - (d / POL_PODSTAWY_S) ** 2))
        return m

    # 1. ślad tarczy: kropki 2×2 co 2°
    rs = R_SLAD * s
    for j in range(180):
        a = math.radians(2 * j)
        px, py = cx + int(math.floor(rs * math.sin(a) + 0.5 + 1e-9)), cy - int(math.floor(rs * math.cos(a) + 0.5 + 1e-9))
        o.prostokat(px - 1, py - 1, 2, 2, True)
    # 2–5. pierścień, wyciek, piki: test biegunowy w pierścieniu r_min…r_max
    for y in range(cy - r_max, cy + r_max + 1):
        for x in range(cx - r_max, cx + r_max + 1):
            dx, dy = x - cx, y - cy
            r2 = dx * dx + dy * dy
            if r2 < r_min * r_min or r2 >= r_max * r_max:
                continue
            r = math.sqrt(r2)
            q = (math.atan2(dx, -dy) % (2 * math.pi)) / (2 * math.pi) * 43200.0
            baza = w * 60
            u = baza + ((q - baza) % 43200.0)
            sesja = _w_sesji(pr, u)
            if r_in <= r < r_out and sesja:
                o.piksel(x, y, True)
                if b_in <= r < b_out and int(u // 300) in biale:
                    o.piksel(x, y, False)
            if r >= r_out and r < r_out + obwiednia(h_zewn, u):
                o.piksel(x, y, True)
            if r < r_in and r >= r_in - obwiednia(h_wewn, u):
                o.piksel(x, y, True)
    # 6. znacznik startu okna (nad pierścieniem i pikami, pod kropkami ASV i cyframi)
    # wyciek w pierwszym albo ostatnim odcinku okna → znacznik omija tor pierścienia (przerwa < 5 min łuku)
    przy_starcie = any(k in odc and bialy_na_rysunku(pr, k, odc[k], w) for k in (w // 5, (w + OKNO_H_MIN - 5) // 5))
    rysuj_znacznik(o, w, cy, skala, bez_toru=przy_starcie)
    # 7. kropki ASV
    if dane["tryb"] in ASV:
        rk = R_KROPKI * s
        for m, _, _ in pr["zdarzenia"]:
            if not (w <= m < w + OKNO_H_MIN):
                continue
            a = math.radians((m % 720) * 0.5)
            o.kolo(cx + int(math.floor(rk * math.sin(a) + 0.5 + 1e-9)), cy - int(math.floor(rk * math.cos(a) + 0.5 + 1e-9)), 4)
    # 8. kreski i cyfry; środek tarczy pusty (od 12.11.2026 bez napisu okna, R-POR-06)
    for g in (1, 2, 4, 5, 7, 8, 10, 11):
        a = math.radians(30 * g)
        x1, y1 = cx + int(math.floor(R_KRESKI[0] * s * math.sin(a) + 0.5 + 1e-9)), cy - int(math.floor(R_KRESKI[0] * s * math.cos(a) + 0.5 + 1e-9))
        x2, y2 = cx + int(math.floor(R_KRESKI[1] * s * math.sin(a) + 0.5 + 1e-9)), cy - int(math.floor(R_KRESKI[1] * s * math.cos(a) + 0.5 + 1e-9))
        o.linia(x1, y1, x2, y2)
        if abs(y2 - y1) > abs(x2 - x1):
            o.linia(x1 + 1, y1, x2 + 1, y2)
        else:
            o.linia(x1, y1 + 1, x2, y2 + 1)
    k24 = kroje.kroj("P24B")
    cap24 = kroje.wersalik(k24)
    for g, t in ((0, "12"), (3, "3"), (6, "6"), (9, "9")):
        a = math.radians(30 * g)
        sx = cx + int(math.floor(R_CYFR * s * math.sin(a) + 0.5 + 1e-9))
        sy = cy - int(math.floor(R_CYFR * s * math.cos(a) + 0.5 + 1e-9))
        kroje.tekst(o, k24, sx - kroje.szer(k24, t) // 2, sy + cap24 // 2, t)


def kat_znacznika(w):
    """Kąt znacznika startu okna w radianach od godziny 12 zgodnie ze wskazówkami (minuta tarczy = pół stopnia)."""
    return math.radians((w % OKNO_H_MIN) * 0.5)


def rysuj_znacznik(o, w, cy, skala, bez_toru=False):
    """Znacznik startu okna tarczy (R-POR-06, zmiana 2026-11-12 (2)): kreska przez tor pierścienia, czarna, w obrębie
    pierścienia z białą przerwą po obu stronach (czytelna na czarnym łuku maski), i grot na zewnątrz pierścienia
    wskazujący kierunek ruchu wskazówek zegara. Bez liczb i bez napisu. Granice w ćwiartkach i połówkach piksela,
    żeby piksel nie leżał dokładnie na granicy (panel liczy to samo tablicą sinusów). `bez_toru` (wyciek w pierwszym
    albo ostatnim odcinku okna): kreska i przerwa nie wchodzą w tor pierścienia, biały odcinek z obrysem zostaje nietknięty."""
    cx = X_TARCZY
    k = kat_znacznika(w)
    sn, cs = math.sin(k), math.cos(k)
    r1, r2 = _r(R_ZNACZNIK[0], skala), _r(R_ZNACZNIK[1], skala)
    r_in, r_out = _r(R_PIERSCIEN[0], skala), _r(R_PIERSCIEN[1], skala)
    z = r2 + int(GROT_CZUBEK) + 2
    for y in range(cy - z, cy + z + 1):
        for x in range(cx - z, cx + z + 1):
            X, Y = x - cx, cy - y
            a = X * sn + Y * cs      # wzdłuż promienia znacznika
            t = X * cs - Y * sn      # w poprzek, dodatnie w kierunku ruchu wskazówek zegara
            if r1 - 0.5 <= a < r2 - 0.5:
                tor = r_in * r_in <= X * X + Y * Y < r_out * r_out
                if bez_toru and tor:
                    continue
                if abs(t) < ZN_POL_SZER:
                    o.piksel(x, y, True)
                    continue
                if abs(t) < ZN_PRZERWA and tor:
                    o.piksel(x, y, False)
                    continue
            if ZN_POL_SZER <= t < GROT_CZUBEK and abs(a - (r2 - 7.5)) <= (GROT_CZUBEK - t) * 28.0 / 51.0:
                o.piksel(x, y, True)


def znak_legendy(o, klucz, x, y):
    """Znak legendy 30×20 px od (x, y)."""
    if klucz == "maska":
        o.prostokat(x, y + 6, 30, 8)
    elif klucz == "wyciek":
        o.prostokat(x, y + 6, 30, 8)
        o.prostokat(x + 3, y + 8, 24, 4, False)
    elif klucz in ("wsparcie", "zdarzenia"):
        o.prostokat(x, y + 14, 30, 6)
        o.trojkat(x + 7, y + 14, x + 15, y, x + 23, y + 14)
    elif klucz == "centralne":
        o.prostokat(x, y, 30, 6)
        o.trojkat(x + 7, y + 6, x + 15, y + 20, x + 23, y + 6)
    elif klucz == "kropka":
        o.prostokat(x, y + 2, 30, 6)
        o.kolo(x + 15, y + 15, 4)


def rysuj_kolumne(o, dane, gora):
    k28, k24, k22 = kroje.kroj("P28B"), kroje.kroj("P24B"), kroje.kroj("P22B")
    y = gora + 8
    if dane["niepewna"]:  # sam czas z maską, jeden komunikat o nocy niepewnej w pierwszym podpisie (R-POR-15)
        kroje.tekst(o, k28, KOL_X, y + 26, dane["czas"] + " z maską")
    else:
        kroje.tekst(o, k28, KOL_X, y + 26, dane["czas"] + " z maską · AHI " + (dane["ahi"] or "–"))
    y += 34
    y += 10
    p = podpisy(dane)
    for i, t in enumerate(p):
        if i == 0:
            for l in kroje.zawin(k28, t, KOL_W)[:2]:
                kroje.tekst(o, k28, KOL_X, y + 26, l)
                y += 34
        else:  # podpis zbiorczy braków najwyżej w 2 liniach (R-POR-27)
            for l in kroje.zawin(k24, t, KOL_W)[:2]:
                kroje.tekst(o, k24, KOL_X, y + 24, l)
                y += 32
    y += 8
    o.prostokat(KOL_X, y, KOL_W, 1)
    y += 9
    for klucz in legenda(dane):
        znak_legendy(o, klucz, KOL_X, y + 4)
        kroje.tekst(o, k22, KOL_X + 42, y + 21, LEGENDA[klucz])
        y += 28
    return y


def rysuj(dane):
    """Cały ekran 800×480 (nagłówek i stopka uproszczone, tylko do podglądu). None: noc bez Portretu (R-POR-16)."""
    if not jest_portret(dane["przebieg"]):
        return None
    o = Obraz()
    k24, k22 = kroje.kroj("P24B"), kroje.kroj("P22B")
    lewo, prawo = dane.get("naglowek", ("", ""))
    kroje.tekst(o, k24, 28, 35, lewo)
    kroje.tekst_prawo(o, k22, 772, 35, prawo)
    o.prostokat(0, 50, 800, 2)
    gora = GORA_TRESCI
    skala = 100
    if dane.get("linia"):
        o.prostokat(0, 54, 800, WYS_LINII)
        o.prostokat(2, 56, 796, WYS_LINII - 4, False)
        kroje.tekst(o, k24, 28, 87, dane["linia"])
        gora += WYS_LINII
        skala = SKALA_LINII
    cy = (gora + DOL_TRESCI) // 2
    rysuj_tarcze(o, dane, cy, skala)
    rysuj_kolumne(o, dane, gora)
    o.prostokat(0, DOL_TRESCI, 800, 2)
    kroje.tekst(o, k22, 28, 465, dane.get("stopka", "przy łóżku"))
    return o


def obszar_porownania(dane):
    """(y0, y1) strefy treści bez linii uwagi: tu obrazek panelu ma być równy wzorcowi (T-TEST-415)."""
    return (GORA_TRESCI + (WYS_LINII if dane.get("linia") else 0), DOL_TRESCI)
