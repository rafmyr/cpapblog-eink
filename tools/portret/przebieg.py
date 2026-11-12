#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Przebieg ostatniej nocy w odcinkach 5 min (Portret nocy, spec 5.4.16, T-DANE-38): wzorzec dla panelu.

Z plików sesji jednej doby (`PLD`, `EVE`; jak panel w firmware/prototyp/src/noc_pliki.cpp, bez BRP, SAD i CSL)
liczy to, co panel zapisuje w `/dane/portret.csv` (wersja 1):

  S  sesja: `od`, `do` = granice zapisu z `PLD` (pierwszy i ostatni rekord z poprawną próbką ciśnienia) w minutach
     od 12:00 dnia `day`, zaokrąglone do najbliższej minuty
  O  odcinek 5 min (siatka od 12:00, czyli od pełnej godziny) z co najmniej jedną poprawną próbką ciśnienia albo
     zdarzeniem: `od` = minuta początku, `do` = minuty z próbką (0–5), `wyciek95` = P95 `Leak` w L/min całkowitych
     (w dół), `wyc_s` = sekundy z `Leak` ≥ 15 L/min, `zew` = zdarzenia na zewnątrz (OA, `Apnea`, H), `centr` = CA,
     `ps` = mediana PS (`Press` − `EprPress`) w 0,1 cmH2O, `maks` = minuty z próbką ciśnienia na maksimum
     (AutoSet: `Press` ≥ MPA − 0,5; ASVAuto: `EprPress` ≥ EAX − 0,5). Odcinek z mniej niż 30 poprawnymi próbkami
     ciśnienia nie liczy się do pików, wycieku i maksimum: `wyciek95`, `zew`, `centr`, `ps`, `maks` puste (`wyc_s`
     zostaje: czas wycieku liczy się z próbek, nie z odcinków)
  Z  zdarzenie (najwyżej 500, od najwcześniejszego): `od` = minuta, `zew` = 1 albo `centr` = 1
  B  jeden wiersz nocy: `braki` = litery `L` (brak kanału `Leak`), `P` (brak poprawnych par PS), `E` (zdarzenia
     nieznane), `R` (bezdechy bez rodzaju), `M` (AutoSet albo ASVAuto bez maksimum z ustawień)

Percentyle metodą najbliższej rangi (edf.percentyl, jak percentylZLicznikow() panelu). Puste pole = brak pomiaru.
Tylko biblioteka standardowa Pythona 3.
"""

import math
import os
import re
import sys

_TU = os.path.dirname(os.path.abspath(__file__))
_KAL = os.path.join(os.path.dirname(_TU), "kalibracja")
for _p in (_TU, _KAL):
    if _p not in sys.path:  # python3 -I nie dodaje katalogu skryptu do sys.path
        sys.path.insert(0, _p)

import edf  # noqa: E402

# Progi rysunku i parametry techniczne (docs/spec/progi-poza-zakresem.csv, spec 5.4.16; te same stałe w
# firmware/prototyp/src/portret.h)
ODCINEK_S = 300           # odcinek 5 min [PRÓG]
ODCINKI_DOBY = 288
MIN_PROBEK = 30           # odcinek liczy się od 30 poprawnych próbek (1 min) [PRÓG]
PROG_WYCIEKU = 15.0       # L/min, R-NOR-05.niepewna_p95 (ADR-0034)
MARGINES_MAKS = 0.5       # cmH2O, ciśnienie na maksimum
MAKS_ZDARZEN = 500        # wierszy `Z` [PRÓG]
# zakresy pól pliku (te same w src/portret.h)
SESJA_OD_MAKS, SESJA_DO_MAKS, MINUTA_ZDARZENIA_MAKS, WYC_S_MAKS = 1440, 2880, 1439, 300
EPS = 1e-9

WERSJA = 1
NAZWA = "portret"
POLA = "data;rodzaj;od;do;wyciek95;wyc_s;zew;centr;ps;maks;braki"

PLIK_RE = re.compile(r"^(\d{8}_\d{6})_(PLD|EVE)\.edf$", re.IGNORECASE)
ZEWNETRZNE = ("Obstructive Apnea", "Apnea", "Hypopnea")
CENTRALNE = ("Central Apnea",)
BEZDECHY_Z_RODZAJEM = ("Obstructive Apnea", "Central Apnea")


def prog_maksimum(tryb, ustawienia):
    """(kanał, próg) ciśnienia na maksimum albo None. `ustawienia`: {tag: liczba} (MPA, EAX) z STR doby."""
    if tryb == "AutoSet" and ustawienia.get("MPA") is not None:
        return ("Press.2s", ustawienia["MPA"] - MARGINES_MAKS)
    if tryb == "ASVAuto" and ustawienia.get("EAX") is not None:
        return ("EprPress.2s", ustawienia["EAX"] - MARGINES_MAKS)
    return None


class _Odcinek:
    __slots__ = ("n", "minuty", "leak", "wyc_s", "ps", "maks", "zew", "centr")

    def __init__(self):
        self.n = 0
        self.minuty = set()
        self.leak = []
        self.wyc_s = 0.0
        self.ps = []
        self.maks = set()
        self.zew = 0
        self.centr = 0


def _sek(start_s, t0, r, cr, i, krok):
    # ta sama kolejność działań co panel (src/portret.cpp), żeby granice odcinków wyszły identycznie
    return float(start_s - t0) + r * cr + i * krok


def _granice_zapisu(pld):
    sygn = None
    for nazwa in ("Press.2s", "EprPress.2s", "MaskPress.2s"):
        sygn = pld.sygnal(nazwa)
        if sygn is not None:
            break
    if sygn is None:
        return None, None
    pierwszy = ostatni = None
    for r, probki in enumerate(pld.rekordy_probek(sygn)):
        if any(sygn.poprawna(x) for x in probki):
            if pierwszy is None:
                pierwszy = r
            ostatni = r
    if pierwszy is None:
        return sygn, None
    return sygn, (pld.start_s + pierwszy * pld.czas_rekordu, pld.start_s + (ostatni + 1) * pld.czas_rekordu)


def _zaokr(x):
    return int(math.floor(x + 0.5))


def przebieg_doby(katalog, dzien, maksimum=None, potrzebne=False):
    """Przebieg doby `dzien` (numer dnia od 1970-01-01, doba od południa) z plików w katalogu `katalog`.

    `maksimum`: (etykieta kanału, próg) z prog_maksimum() albo None (pole `maks` puste); `potrzebne`: tryb AutoSet
    albo ASVAuto (bez kanału albo progu brak pomiaru `M`).
    Zwraca słownik: data (data poranka RRRR-MM-DD), sesje [(od, do)], odcinki {indeks: dict pól}, zdarzenia
    [(minuta, zew, centr)], braki (tekst liter). Plik nieczytelny pomija (panel liczy wtedy noc z raportu JSON).
    """
    t0 = dzien * 86400 + 12 * 3600
    nazwy = sorted(n for n in os.listdir(katalog) if PLIK_RE.match(n))
    pld = [n for n in nazwy if PLIK_RE.match(n).group(2).upper() == "PLD"]
    eve = [n for n in nazwy if PLIK_RE.match(n).group(2).upper() == "EVE"]
    odc = {}
    sesje_s = []
    ma_leak = False
    ma_maks = False

    def o(k):
        if k not in odc:
            odc[k] = _Odcinek()
        return odc[k]

    for nazwa in pld:
        try:
            p = edf.PlikEDF(os.path.join(katalog, nazwa))
        except (edf.BladEDF, OSError, ValueError):
            continue
        cr = p.czas_rekordu
        sc, zapis = _granice_zapisu(p)
        if zapis:
            sesje_s.append(zapis)
        if sc is not None:
            krok = cr / sc.probki
            for r, probki in enumerate(p.rekordy_probek(sc)):
                for i, x in enumerate(probki):
                    if not sc.poprawna(x):
                        continue
                    s = _sek(p.start_s, t0, r, cr, i, krok)
                    k = math.floor(s / ODCINEK_S)
                    if 0 <= k < ODCINKI_DOBY:
                        x_ = o(k)
                        x_.n += 1
                        x_.minuty.add(math.floor(s / 60))
        sl = p.sygnal("Leak.2s")
        if sl is not None:
            ma_leak = True
            skala = 60.0 if sl.jednostka == "L/s" else 1.0
            krok = cr / sl.probki
            for r, probki in enumerate(p.rekordy_probek(sl)):
                for i, x in enumerate(probki):
                    if not sl.poprawna(x):
                        continue
                    s = _sek(p.start_s, t0, r, cr, i, krok)
                    k = math.floor(s / ODCINEK_S)
                    if 0 <= k < ODCINKI_DOBY:
                        v = sl.fizyczna(x, skala)
                        x_ = o(k)
                        x_.leak.append(v)
                        if v >= PROG_WYCIEKU - EPS:
                            x_.wyc_s += krok
        sp, se = p.sygnal("Press.2s"), p.sygnal("EprPress.2s")
        if sp is not None and se is not None and sp.probki == se.probki:
            krok = cr / sp.probki
            for r, (rp, re_) in enumerate(zip(p.rekordy_probek(sp), p.rekordy_probek(se))):
                for i, (a, b) in enumerate(zip(rp, re_)):
                    if not (sp.poprawna(a) and se.poprawna(b)):
                        continue
                    s = _sek(p.start_s, t0, r, cr, i, krok)
                    k = math.floor(s / ODCINEK_S)
                    if 0 <= k < ODCINKI_DOBY:
                        o(k).ps.append(round(sp.fizyczna(a) - se.fizyczna(b), 6))
        if maksimum:
            sm = p.sygnal(maksimum[0])
            if sm is not None:
                ma_maks = True
                krok = cr / sm.probki
                for r, probki in enumerate(p.rekordy_probek(sm)):
                    for i, x in enumerate(probki):
                        if not sm.poprawna(x) or sm.fizyczna(x) < maksimum[1] - EPS:
                            continue
                        s = _sek(p.start_s, t0, r, cr, i, krok)
                        k = math.floor(s / ODCINEK_S)
                        if 0 <= k < ODCINKI_DOBY:
                            o(k).maks.add(math.floor(s / 60))

    # zdarzenia: wszystkie EVE bez powtórzeń, liczone w czasie zapisu sesji (jak AHI)
    zdarzenia = set()
    eve_ok = bool(eve) and bool(pld)
    for nazwa in eve:
        try:
            p = edf.PlikEDF(os.path.join(katalog, nazwa))
            for onset, trwanie, tekst in p.adnotacje():
                zdarzenia.add((round(p.start_s + onset, 3), trwanie, tekst))
        except (edf.BladEDF, OSError, ValueError):
            eve_ok = False
    eve_ok = eve_ok and sum(b - a for a, b in sesje_s) > 0
    klasyfikacja = any(t in BEZDECHY_Z_RODZAJEM for _, _, t in zdarzenia)
    lista = []
    bezdechy = 0
    for t, _, tekst in sorted(zdarzenia):
        if tekst not in ZEWNETRZNE and tekst not in CENTRALNE:
            continue
        if not any(a <= t < b for a, b in sesje_s):
            continue
        if tekst in ("Obstructive Apnea", "Central Apnea", "Apnea"):
            bezdechy += 1
        s = t - t0
        if 0 <= math.floor(s / 60) <= MINUTA_ZDARZENIA_MAKS:
            lista.append((math.floor(s / 60), 1 if tekst in ZEWNETRZNE else 0, 1 if tekst in CENTRALNE else 0))
        if eve_ok:
            k = math.floor(s / ODCINEK_S)
            if 0 <= k < ODCINKI_DOBY:
                x_ = o(k)
                if tekst in ZEWNETRZNE:
                    x_.zew += 1
                else:
                    x_.centr += 1

    jest_ps = any(x.ps for x in odc.values())
    braki = ""
    if not ma_leak:
        braki += "L"
    if not jest_ps:
        braki += "P"
    if not eve_ok:
        braki += "E"
    if eve_ok and bezdechy > 0 and not klasyfikacja:
        braki += "R"
    if potrzebne and (maksimum is None or not ma_maks):
        braki += "M"

    odcinki = {}
    for k in sorted(odc):
        x = odc[k]
        liczy = x.n >= MIN_PROBEK
        odcinki[k] = {
            "od": k * 5,
            "do": len(x.minuty),
            "wyciek95": (int(math.floor(edf.percentyl(sorted(x.leak), 95) + EPS)) if liczy and x.leak else None),
            "wyc_s": min(WYC_S_MAKS, _zaokr(x.wyc_s)) if ma_leak else None,
            "zew": x.zew if liczy and eve_ok else None,
            "centr": x.centr if liczy and eve_ok else None,
            "ps": _zaokr(edf.percentyl(sorted(x.ps), 50) * 10) if liczy and x.ps else None,
            "maks": len(x.maks) if liczy and maksimum else None,
        }
    sesje = []
    for a, b in sesje_s:
        od = max(0, min(SESJA_OD_MAKS, _zaokr((a - t0) / 60.0)))
        sesje.append((od, max(od, min(SESJA_DO_MAKS, _zaokr((b - t0) / 60.0)))))
    sesje.sort()
    return {
        "data": _data_poranka(dzien),
        "sesje": sesje,
        "odcinki": odcinki,
        "zdarzenia": lista[:MAKS_ZDARZEN] if eve_ok else [],
        "braki": braki,
    }


def _data_poranka(dzien):
    import datetime as dt
    return (dt.date(1970, 1, 1) + dt.timedelta(days=dzien + 1)).isoformat()


# ---------------------------------------------------------------------------
# Plik `/dane/portret.csv` (T-DANE-38): nagłówek magazynu, nazwy pól, wiersze, `#koniec {n}`
# ---------------------------------------------------------------------------
def _p(v):
    return "" if v is None else str(v)


def wiersze(pr):
    d = pr["data"]
    w = []
    for a, b in pr["sesje"]:
        w.append(";".join([d, "S", str(a), str(b), "", "", "", "", "", "", ""]))
    for k in sorted(pr["odcinki"]):
        x = pr["odcinki"][k]
        w.append(";".join([d, "O", str(x["od"]), str(x["do"]), _p(x["wyciek95"]), _p(x["wyc_s"]), _p(x["zew"]),
                           _p(x["centr"]), _p(x["ps"]), _p(x["maks"]), ""]))
    for m, z, c in pr["zdarzenia"]:
        w.append(";".join([d, "Z", str(m), "", "", "", str(z), str(c), "", "", ""]))
    w.append(";".join([d, "B", "", "", "", "", "", "", "", "", pr["braki"]]))
    return w


def tekst_pliku(pr):
    w = wiersze(pr)
    return "#cpap-eink %s %d\n%s\n%s\n#koniec %d\n" % (NAZWA, WERSJA, POLA, "\n".join(w), len(w))


def czytaj_plik(tekst):
    """Treść pliku → przebieg (jak zapisany); ValueError przy złym pliku."""
    l = tekst.split("\n")
    if l and l[-1] == "":
        l.pop()
    if len(l) < 3 or l[0] != "#cpap-eink %s %d" % (NAZWA, WERSJA) or l[1] != POLA or not l[-1].startswith("#koniec "):
        raise ValueError("zły plik przebiegu")
    dane = l[2:-1]
    if int(l[-1][8:]) != len(dane):
        raise ValueError("zła liczba wierszy")
    pr = {"data": None, "sesje": [], "odcinki": {}, "zdarzenia": [], "braki": ""}

    def n(t):
        return None if t == "" else int(t)
    for w in dane:
        f = w.split(";")
        if len(f) != 11:
            raise ValueError("zły wiersz")
        pr["data"] = f[0]
        if f[1] == "S":
            pr["sesje"].append((int(f[2]), int(f[3])))
        elif f[1] == "O":
            pr["odcinki"][int(f[2]) // 5] = {"od": int(f[2]), "do": int(f[3]), "wyciek95": n(f[4]), "wyc_s": n(f[5]),
                                             "zew": n(f[6]), "centr": n(f[7]), "ps": n(f[8]), "maks": n(f[9])}
        elif f[1] == "Z":
            pr["zdarzenia"].append((int(f[2]), int(f[6]), int(f[7])))
        elif f[1] == "B":
            pr["braki"] = f[10]
    return pr
