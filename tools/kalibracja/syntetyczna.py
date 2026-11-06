#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Syntetyczna karta SD aparatu ResMed do testów loadera nocy (bez prawdziwych danych, WY12).

Tworzy w podanym katalogu `STR.edf` i `DATALOG/RRRRMMDD/*.edf` (BRP, PLD, SAD, EVE, CSL)
z nocami o znanych wartościach. Znane odpowiedzi sprawdza `test_noce.py`.

Noce (katalog DATALOG = dzień terapii `day`; data poranka = day + 1):
  20260110 N1 ASVAuto:00–06:00; zdarzenia OA 3, CA 7, H 4, RERA 2 w sesji,
                OA o 06:00 i 06:30 poza sesją; CSR 10 + 5 min i 15 min przycięte do 5 min;
                przeciek 30 L/min przez 30 min; ciśnienie 12 (60%) i 15 (40%)
  20260111  N2  sesja po północy 00:30–07:00
  20260112  N3  dwie sesje 23:10–23:40 i 00:19–07:00 (pierwszy rekord PLD drugiej sesji
                bez poprawnych próbek); zdarzenie w przerwie między sesjami
  20260113  N4  noc krótsza niż 4 h: 01:00–03:30
  20260114  N5  duży przeciek przez całą noc 23:00–05:00: 39,6 L/min przez 60 min, potem 32,4
  20260115 N6 drzemka 15:00–16:00 i:10–06:40 (przykład z R-DANE-27)
  20260116      dzień bez sesji (tylko rekord STR)
  20260117  N8  tryb ASV (Mode = 7) zamiast ASVAuto, 23:00–06:00
  20260118  N9  brak pliku EVE (noc częściowa, zdarzenia nieznane); zmiana EAX na 11
  20260119  N10 tylko BRP, bez PLD (czas nieznany, ciśnienie ze STR)
  20260120  N11 trzy sesje 17:15:49, 03:40:48, 04:01:16; PLD i SAD o 1 s po BRP;
                EVE i CSL tylko przy pierwszej sesji, prefiks 5 s wcześniej
  20260121  N12 jedna sesja: EVE 02:52:52, BRP 02:52:57, PLD i SAD 02:52:58

Osobna karta `utworz_ps` (wsparcie PS = Press.2s − EprPress.2s, M-06p, M-28, M-28p):
  20260201  P1  stałe IPAP 14 i EPAP 7 przez 300 min → PS 7
  20260202  P2  150 min 12/8 (PS 4), potem 150 min 15/6 (PS 9); EPAP niepoprawne w min 0–59
  20260203  P3  285 min 13/8 (PS 5), potem 15 min 17/7 (PS 10): P95 na granicy rangi
  20260204  P4  tylko BRP (ciśnienie ze STR) → PS puste

Użycie: python3 -I tools/kalibracja/syntetyczna.py KATALOG
Tylko biblioteka standardowa Pythona 3.
"""

import datetime as dt
import os
import struct
import sys
from array import array

D = dt.datetime


# ---------------------------------------------------------------------------
# Zapis EDF
# ---------------------------------------------------------------------------
def _pole(tekst, szer):
    b = str(tekst).encode("latin-1")
    if len(b) > szer:
        raise ValueError("pole za długie: %r" % tekst)
    return b.ljust(szer, b" ")


def _num(v):
    t = ("%d" % v) if float(v) == int(v) else ("%g" % v)
    return t


def zapisz_edf(sciezka, start, czas_rekordu, sygnaly, rekordy, zastrzezone=""):
    """sygnaly: lista (etykieta, jednostka, fiz_min, fiz_max, cyf_min, cyf_max, probki).
    rekordy: lista rekordów; rekord = lista wartości na sygnał: lista liczb cyfrowych
    albo bytes (sygnał adnotacji, dopełniany zerami)."""
    ns = len(sygnaly)
    nag = b"".join([
        _pole("0", 8), _pole("X X X X", 80), _pole("Startdate X X X X", 80),
        _pole(start.strftime("%d.%m.%y"), 8), _pole(start.strftime("%H.%M.%S"), 8),
        _pole(256 + 256 * ns, 8), _pole(zastrzezone, 44), _pole(len(rekordy), 8),
        _pole(_num(czas_rekordu), 8), _pole(ns, 4),
    ])
    for kolumna, szer in ((0, 16), (None, 80), (1, 8), (2, 8), (3, 8), (4, 8), (5, 8), (None, 80),
                          (6, 8), (None, 32)):
        for s in sygnaly:
            nag += _pole("" if kolumna is None else (s[kolumna] if kolumna < 2 else _num(s[kolumna])), szer)
    with open(sciezka, "wb") as f:
        f.write(nag)
        for rek in rekordy:
            for s, wart in zip(sygnaly, rek):
                if isinstance(wart, (bytes, bytearray)):
                    if len(wart) > s[6] * 2:
                        raise ValueError("adnotacja za długa")
                    f.write(bytes(wart).ljust(s[6] * 2, b"\x00"))
                else:
                    if len(wart) != s[6]:
                        raise ValueError("zła liczba próbek %s" % s[0])
                    f.write(array("h", wart).tobytes() if sys.byteorder == "little"
                            else struct.pack("<%dh" % len(wart), *wart))


CRC = ("Crc16", "", -32768, 32767, -32768, 32767, 1)

# Sygnały PLD: etykieta, jednostka, fiz_min, fiz_max, cyf_min, cyf_max, próbki na 60 s
PLD_SYG = [
    ("MaskPress.2s", "cmH2O", 0, 30, 0, 1500, 30),
    ("Press.2s", "cmH2O", 0, 30, 0, 1500, 30),
    ("EprPress.2s", "cmH2O", 0, 30, 0, 1500, 30),
    ("Leak.2s", "L/s", 0, 2, 0, 100, 30),
    ("RespRate.2s", "bpm", 0, 50, 0, 250, 30),
    ("TidVol.2s", "L", 0, 4, 0, 200, 30),
    ("MinVent.2s", "L/min", 0, 30, 0, 240, 30),
    ("Snore.2s", "", 0, 5, 0, 250, 30),
    ("FlowLim.2s", "", 0, 1, 0, 100, 30),
    ("TgtVent.2s", "L/min", 0, 30, 0, 240, 30),
    CRC,
]
SAD_SYG = [("Pulse.1s", "bpm", 0, 300, 0, 300, 60), ("SpO2.1s", "%", 0, 100, 0, 100, 60), CRC]
BRP_SYG = [("Flow.40ms", "L/s", -2, 2, -1000, 1000, 1500), ("Press.40ms", "cmH2O", 0, 30, 0, 1500, 1500), CRC]
ANN_SYG = [("EDF Annotations", "", 0, 1, -32768, 32767, 40), CRC]

DOMYSLNE = {"MaskPress.2s": 12.0, "Press.2s": 12.0, "EprPress.2s": 8.0, "Leak.2s": 0.1,
            "RespRate.2s": 15.0, "TidVol.2s": 0.46, "MinVent.2s": 7.5, "Snore.2s": 0.1,
            "FlowLim.2s": 0.05, "TgtVent.2s": 6.0}


def _cyfrowa(s, v):
    if v is None:
        return -1                                   # poza zakresem cyfrowym: próbka niepoprawna
    return int(round(s[4] + (v - s[2]) * (s[5] - s[4]) / (s[3] - s[2])))


def pld(sciezka, start, minuty, profil=None):
    """profil(minuta) -> dict nadpisań wartości fizycznych; None w wartości = próbka niepoprawna."""
    rek = []
    for i in range(minuty):
        w = dict(DOMYSLNE)
        if profil:
            w.update(profil(i))
        rek.append([[_cyfrowa(s, w[s[0]])] * s[6] if s is not CRC else [0] for s in PLD_SYG])
    zapisz_edf(sciezka, start, 60, PLD_SYG, rek)


def sad(sciezka, start, minuty, spo2=95, puls=60):
    rek = [[[puls] * 60, [spo2] * 60, [0]] for _ in range(minuty)]
    zapisz_edf(sciezka, start, 60, SAD_SYG, rek)


def brp(sciezka, start, minuty):
    zero = [0] * 1500
    p12 = [600] * 1500
    zapisz_edf(sciezka, start, 60, BRP_SYG, [[zero, p12, [0]] for _ in range(minuty)])


def adnotacje(sciezka, start, zdarzenia):
    """zdarzenia: lista (datetime, czas_trwania_s albo None, tekst); czas trwania jako tekst
    trafia do pliku bez zmian (do testów złych adnotacji). Onset może być ujemny."""
    rek = []
    for t, dur, tekst in zdarzenia:
        onset = (t - start).total_seconds()
        znak = b"+" if onset >= 0 else b"-"
        tal = b"+0\x14\x14\x00" + znak + _num(abs(onset)).encode()
        if dur is not None:
            tal += b"\x15" + (dur.encode() if isinstance(dur, str) else _num(dur).encode())
        tal += b"\x14" + tekst.encode("latin-1") + b"\x14\x00"
        rek.append([tal, [0]])
    zapisz_edf(sciezka, start, 0, ANN_SYG, rek, zastrzezone="EDF+D")


# ---------------------------------------------------------------------------
# STR.edf
# ---------------------------------------------------------------------------
STR_SYG = [
    ("Date", "", 0, 24836, 0, 24836, 1),
    ("MaskOn", "min.", 0, 1440, 0, 1440, 10),
    ("MaskOff", "min.", 0, 1440, 0, 1440, 10),
    ("Duration", "min.", 0, 1440, 0, 1440, 1),
    ("Mode", "", 0, 16, 0, 16, 1),
    ("S.AV.EPAP", "cmH2O", 1, 30, 50, 1500, 1),
    ("S.AV.MaxPS", "cmH2O", 0, 30, 0, 1500, 1),
    ("S.AV.MinPS", "cmH2O", 0, 30, 0, 1500, 1),
    ("S.AA.MaxEPAP", "cmH2O", 1, 30, 50, 1500, 1),
    ("S.AA.MinEPAP", "cmH2O", 1, 30, 50, 1500, 1),
    ("S.AA.MaxPS", "cmH2O", 0, 30, 0, 1500, 1),
    ("S.AA.MinPS", "cmH2O", 0, 30, 0, 1500, 1),
    ("S.Mask", "", 0, 16, 0, 16, 1),
    ("S.HumLevel", "", 1, 8, 1, 8, 1),
    ("TgtIPAP.50", "cmH2O", 0, 30, 0, 1500, 1),
    ("TgtIPAP.95", "cmH2O", 0, 30, 0, 1500, 1),
    ("TgtEPAP.50", "cmH2O", 0, 30, 0, 1500, 1),
    ("Leak.50", "L/s", 0, 2, 0, 100, 1),
    ("Leak.95", "L/s", 0, 2, 0, 100, 1),
    ("AHI", "", 0, 240, 0, 2400, 1),
    CRC,
]

ASVAUTO = {"Mode": 8, "S.AA.MinEPAP": 6, "S.AA.MaxEPAP": 10, "S.AA.MinPS": 3, "S.AA.MaxPS": 15,
           "S.AV.EPAP": None, "S.AV.MinPS": None, "S.AV.MaxPS": None, "S.Mask": 2, "S.HumLevel": 4}
ASV = {"Mode": 7, "S.AV.EPAP": 8, "S.AV.MinPS": 3, "S.AV.MaxPS": 15,
       "S.AA.MinEPAP": None, "S.AA.MaxEPAP": None, "S.AA.MinPS": None, "S.AA.MaxPS": None,
       "S.Mask": 2, "S.HumLevel": 4}


def str_edf(sciezka, pierwszy_dzien, dni):
    """dni: lista dict wartości fizycznych na kolejne dni od `pierwszy_dzien` (date)."""
    rek = []
    for i, w in enumerate(dni):
        dzien = (pierwszy_dzien - dt.date(1970, 1, 1)).days + i
        w = dict(w)
        w["Date"] = dzien
        r = []
        for s in STR_SYG:
            if s is CRC:
                r.append([0])
            elif s[6] == 10:
                r.append([-1] * 10)
            else:
                r.append([_cyfrowa(s, w.get(s[0]))])
        rek.append(r)
    zapisz_edf(sciezka, D.combine(pierwszy_dzien, dt.time(12, 0)), 86400, STR_SYG, rek)


# ---------------------------------------------------------------------------
# Karta
# ---------------------------------------------------------------------------
def _sesja(katalog, start, minuty, profil=None, pliki=("BRP", "PLD", "SAD"), przesuniecia=None):
    """Pliki jednej sesji. `przesuniecia`: {rodzaj: sekundy} startu (i prefiksu) pliku względem
    `start`, jak na prawdziwej karcie (BRP 02:52:57, PLD i SAD 02:52:58)."""
    przes = przesuniecia or {}
    prefiks = start.strftime("%Y%m%d_%H%M%S")
    for rodzaj, f in (("BRP", brp), ("PLD", pld), ("SAD", sad)):
        if rodzaj not in pliki:
            continue
        t = start + dt.timedelta(seconds=przes.get(rodzaj, 0))
        sciezka = os.path.join(katalog, t.strftime("%Y%m%d_%H%M%S") + "_%s.edf" % rodzaj)
        if rodzaj == "PLD":
            f(sciezka, t, minuty, profil)
        else:
            f(sciezka, t, minuty)
    return prefiks


def _dzien(karta, nazwa):
    k = os.path.join(karta, "DATALOG", nazwa)
    os.makedirs(k, exist_ok=True)
    return k


def utworz(karta):
    """Tworzy syntetyczną kartę w katalogu `karta` (katalog musi być pusty albo nie istnieć)."""
    if os.path.exists(karta) and os.listdir(karta):
        raise SystemExit("Katalog nie jest pusty: %s" % karta)
    os.makedirs(karta, exist_ok=True)

    # N1: 20260110:00–06:00 (420 min)
    k = _dzien(karta, "20260110")
    s = D(2026, 1, 10, 23, 0)

    def profil_n1(i):
        w = {"Press.2s": 12.0 if i < 252 else 15.0}       # 60% po 12, 40% po 15
        if 60 <= i < 90:
            w["Leak.2s"] = 0.5                              # 30 L/min przez 30 min
        return w
    p = _sesja(k, s, 420, profil_n1)
    zd = ([(s + dt.timedelta(minutes=10 + 20 * j), 10, "Obstructive Apnea") for j in range(3)]
          + [(s + dt.timedelta(minutes=30 + 30 * j), 12, "Central Apnea") for j in range(7)]
          + [(s + dt.timedelta(minutes=40 + 45 * j), 15, "Hypopnea") for j in range(4)]
          + [(s + dt.timedelta(minutes=100), 8, "Arousal"), (s + dt.timedelta(minutes=200), 8, "RERA")]
          + [(D(2026, 1, 11, 6, 0), 10, "Obstructive Apnea"),       # koniec sesji: nie liczy się
             (D(2026, 1, 11, 6, 30), 10, "Obstructive Apnea"),      # poza sesją
             (s + dt.timedelta(minutes=5), None, "Recording starts")])
    adnotacje(os.path.join(k, p + "_EVE.edf"), s, zd)
    adnotacje(os.path.join(k, p + "_CSL.edf"), s, [
        (D(2026, 1, 11, 1, 0), None, "CSR Start"), (D(2026, 1, 11, 1, 10), None, "CSR End"),
        (D(2026, 1, 11, 3, 0), None, "CSR Start"), (D(2026, 1, 11, 3, 5), None, "CSR End"),
        (D(2026, 1, 11, 5, 55), None, "CSR Start"), (D(2026, 1, 11, 6, 10), None, "CSR End"),
    ])

    # N2: 20260111, sesja po północy 00:30–07:00 (390 min)
    k = _dzien(karta, "20260111")
    s = D(2026, 1, 12, 0, 30)
    p = _sesja(k, s, 390)
    adnotacje(os.path.join(k, p + "_EVE.edf"), s, [(s + dt.timedelta(minutes=60), 10, "Hypopnea")])
    adnotacje(os.path.join(k, p + "_CSL.edf"), s, [])

    # N3: 20260112, sesje 23:10–23:40 i 00:19–07:00 (pierwszy rekord bez próbek)
    k = _dzien(karta, "20260112")
    s1 = D(2026, 1, 12, 23, 10)
    p1 = _sesja(k, s1, 30)
    s2 = D(2026, 1, 13, 0, 19)
    _sesja(k, s2, 401, lambda i: {"Press.2s": None, "EprPress.2s": None, "MaskPress.2s": None} if i == 0 else {})
    adnotacje(os.path.join(k, p1 + "_EVE.edf"), s1, [
        (D(2026, 1, 12, 23, 20), 12, "Central Apnea"),
        (D(2026, 1, 12, 23, 50), 12, "Obstructive Apnea"),    # przerwa między sesjami
        (D(2026, 1, 13, 1, 0), 12, "Central Apnea"),
    ])
    adnotacje(os.path.join(k, p1 + "_CSL.edf"), s1, [])

    # N4: 20260113, noc krótka 01:00–03:30 (150 min)
    k = _dzien(karta, "20260113")
    s = D(2026, 1, 14, 1, 0)
    p = _sesja(k, s, 150)
    adnotacje(os.path.join(k, p + "_EVE.edf"), s, [(s + dt.timedelta(minutes=30), 10, "Obstructive Apnea")])
    adnotacje(os.path.join(k, p + "_CSL.edf"), s, [])

    # N5: 20260114, duży przeciek 23:00–05:00 (360 min)
    k = _dzien(karta, "20260114")
    s = D(2026, 1, 14, 23, 0)
    p = _sesja(k, s, 360, lambda i: {"Leak.2s": 0.66 if i < 60 else 0.54})
    adnotacje(os.path.join(k, p + "_EVE.edf"), s, [])
    adnotacje(os.path.join(k, p + "_CSL.edf"), s, [])

    # N6: 20260115, drzemka 15:00–16:00 i:10–06:40 (450 min)
    k = _dzien(karta, "20260115")
    sd = D(2026, 1, 15, 15, 0)
    pd = _sesja(k, sd, 60)
    s = D(2026, 1, 15, 23, 10)
    _sesja(k, s, 450)
    adnotacje(os.path.join(k, pd + "_EVE.edf"), sd, [(s + dt.timedelta(minutes=100), 10, "Hypopnea")])
    adnotacje(os.path.join(k, pd + "_CSL.edf"), sd, [])

    # N8: 20260117, tryb ASV, 23:00–06:00
    k = _dzien(karta, "20260117")
    s = D(2026, 1, 17, 23, 0)
    p = _sesja(k, s, 420)
    adnotacje(os.path.join(k, p + "_EVE.edf"), s, [])
    adnotacje(os.path.join(k, p + "_CSL.edf"), s, [])

    # N9: 20260118, bez EVE, 23:00–05:00
    k = _dzien(karta, "20260118")
    _sesja(k, D(2026, 1, 18, 23, 0), 360)

    # N10: 20260119, tylko BRP
    k = _dzien(karta, "20260119")
    s = D(2026, 1, 19, 23, 0)
    p = _sesja(k, s, 300, pliki=("BRP",))
    adnotacje(os.path.join(k, p + "_EVE.edf"), s, [])

    # N11: 20260120, wzorzec z prawdziwej karty: trzy sesje w jednym dniu (wieczorna 17:15),
    # BRP z prefiksem sesji, PLD i SAD o 1 s później, EVE i CSL tylko dla pierwszej sesji, o 5 s wcześniej
    k = _dzien(karta, "20260120")
    p1 = D(2026, 1, 20, 17, 15, 49)
    przes = {"BRP": 0, "PLD": 1, "SAD": 1}
    _sesja(k, p1, 120, przesuniecia=przes)
    _sesja(k, D(2026, 1, 21, 3, 40, 48), 20, przesuniecia=przes)
    _sesja(k, D(2026, 1, 21, 4, 1, 16), 180, przesuniecia=przes)
    e = p1 - dt.timedelta(seconds=5)
    nazwa = e.strftime("%Y%m%d_%H%M%S")
    adnotacje(os.path.join(k, nazwa + "_EVE.edf"), e, [
        (e, None, "Recording starts"),
        (D(2026, 1, 20, 17, 30), 12, "Central Apnea"),
        (D(2026, 1, 21, 3, 50), 12, "Obstructive Apnea"),
        (D(2026, 1, 21, 5, 0), 12, "Hypopnea"),
    ])
    adnotacje(os.path.join(k, nazwa + "_CSL.edf"), e, [])

    # N12: 20260121, jedna sesja jak 24/25.10.2026: EVE 02:52:52, BRP 02:52:57, PLD i SAD 02:52:58
    k = _dzien(karta, "20260121")
    s = D(2026, 1, 22, 2, 52, 57)
    _sesja(k, s, 360, przesuniecia=przes)
    e = D(2026, 1, 22, 2, 52, 52)
    adnotacje(os.path.join(k, e.strftime("%Y%m%d_%H%M%S") + "_EVE.edf"), e,
              [(D(2026, 1, 22, 3, 30), 10, "Obstructive Apnea")])

    asvauto_b = dict(ASVAUTO, **{"S.AA.MaxEPAP": 11})
    zwykly = {"Duration": 420, "TgtIPAP.50": 12.5, "TgtIPAP.95": 15.0, "TgtEPAP.50": 8.0,
              "Leak.50": 0.1, "Leak.95": 0.2, "AHI": 2.0}
    # Duration = rzeczywisty czas terapii w minutach (sprawdzian --sprawdz-str)
    czasy = [420, 390, 430, 150, 360, 510]
    dni = [dict(ASVAUTO, **dict(zwykly, Duration=c)) for c in czasy]   # 10–15.01
    dni.append(dict(ASVAUTO, Duration=0))                       # 16.01: bez terapii
    dni.append(dict(ASV, **dict(zwykly, Duration=420)))         # 17.01
    dni.append(dict(asvauto_b, **dict(zwykly, Duration=360)))   # 18.01
    dni.append(dict(asvauto_b, **dict(zwykly, Duration=300)))   # 19.01 (bez PLD)
    dni.append(dict(asvauto_b, **dict(zwykly, Duration=320)))   # 20.01: trzy sesje
    dni.append(dict(asvauto_b, **dict(zwykly, Duration=360)))   # 21.01
    str_edf(os.path.join(karta, "STR.edf"), dt.date(2026, 1, 10), dni)
    return karta



def utworz_ps(karta):
    """Karta z nocami do M-06p, M-28 i M-28p (wsparcie PS w ASVAuto). Znane odpowiedzi:

    P1: PS 7 w każdej próbce → M-28 = M-28p = 7; EPAP P95 = 7.
    P2: pary poprawne: PS 4 przez 90 min (min 60–149), PS 9 przez 150 min; n = 7200 próbek,
        ranga 50% = 3600 > 2700 → M-28 = 9; ranga 95% = 6840 → M-28p = 9. Różnica median
        kanałów dałaby 12 − 6 = 6. EPAP z poprawnych próbek: 6 (150 min) i 8 (90 min) →
        mediana 6, P95 8.
    P3: n = 9000, ranga 95% = 8550 = 285 min × 30 → M-28p = 5 (interpolacja dałaby 5,25); M-28 = 5.
    P4: bez PLD → źródło STR, M-28 i M-28p puste.
    """
    if os.path.exists(karta) and os.listdir(karta):
        raise SystemExit("Katalog nie jest pusty: %s" % karta)
    os.makedirs(karta, exist_ok=True)

    def noc(nazwa, start, minuty, profil=None, pliki=("BRP", "PLD", "SAD")):
        k = _dzien(karta, nazwa)
        p = _sesja(k, start, minuty, profil, pliki=pliki)
        adnotacje(os.path.join(k, p + "_EVE.edf"), start, [])

    noc("20260201", D(2026, 2, 1, 23, 0), 300, lambda i: {"Press.2s": 14.0, "EprPress.2s": 7.0})
    noc("20260202", D(2026, 2, 2, 23, 0), 300,
        lambda i: {"Press.2s": 12.0 if i < 150 else 15.0,
                   "EprPress.2s": None if i < 60 else (8.0 if i < 150 else 6.0)})
    noc("20260203", D(2026, 2, 3, 23, 0), 300,
        lambda i: {"Press.2s": 13.0, "EprPress.2s": 8.0} if i < 285 else {"Press.2s": 17.0, "EprPress.2s": 7.0})
    noc("20260204", D(2026, 2, 4, 23, 0), 300, pliki=("BRP",))
    zwykly = {"Duration": 300, "TgtIPAP.50": 12.5, "TgtIPAP.95": 15.0, "TgtEPAP.50": 8.0,
              "Leak.50": 0.1, "Leak.95": 0.2, "AHI": 2.0}
    str_edf(os.path.join(karta, "STR.edf"), dt.date(2026, 2, 1), [dict(ASVAUTO, **zwykly)] * 4)
    return karta


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    utworz(sys.argv[1])
    print("Syntetyczna karta: %s" % sys.argv[1])
