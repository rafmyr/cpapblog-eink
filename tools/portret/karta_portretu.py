#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Syntetyczna karta do testów Portretu nocy (T-TEST-413, T-TEST-414): noce ze znanymi odpowiedziami.

Pliki jak na karcie aparatu (`DATALOG/RRRRMMDD/*_PLD.edf`, `*_EVE.edf`, `STR.edf`), zapis plikami
tools/kalibracja/syntetyczna.py; PLD z próbkami 2 s, które mogą się różnić w obrębie minuty. Doby (katalog = doba
terapii `day`, poranek = day + 1):

  20261101  P1  AutoSet (MPA 15) 23:00–07:00; 5 spłyceń 04:05–04:09 (pik k = 5 na odcinku 04:05), 1 OA 01:30
                (pik 12,6 px), 2 CA 02:00 i 02:02 (pik do środka k = 2, 10,8 px), RERA 03:40 (bez piku); wyciek
                18 L/min 03:00–03:30 (6 białych odcinków), 6 próbek (12 s) wycieku 18 L/min o 05:41 (P95 odcinka
                niski, ale 16 s czasu wycieku); ciśnienie 14,6 05:00–05:20 (20 min na maksimum)
  20261102  P2  AutoSet: drzemka 14:00–15:00, sesje 22:30–01:00 i 01:20–06:40 (przerwa 01:00–01:20), EVE tylko
                „Apnea” (bez rodzaju bezdechu: `R`)
  20261103  P3  ASVAuto (EAX 10) 23:30–06:30, EPAP 8, IPAP 13 (PS 5,0); PS 7,0 02:00–02:05, PS 11,0 03:00–03:05,
                PS 6,9 04:00–04:05; EPAP 9,6 06:00–06:10 (10 min na maksimum); zdarzenia 01:10 (Apnea), 03:01
                (Hypopnea) → kropki
  20261104  P4  AutoSet 23:00–05:00, PLD bez kanału `Leak`, bez pliku EVE (`L`, `E`)
  20261105  P5  ASVAuto 23:00–05:00, EPAP niepoprawne całą noc (brak par PS: `P`)
  20261106  P6  AutoSet, jedna sesja 23:00–23:10 (noc bez terapii: brak Portretu, R-POR-16)
  20261107  P7  AutoSet 23:00–06:00; odcinek 02:00 z 20 poprawnymi próbkami ciśnienia (mniej niż 30: pusty) i
                wyciekiem 30 L/min w tych próbkach (bez białego odcinka)
  20261108  P8  AutoSet bez MPA w STR (`M`: `Maksimum ciśnienia: nieznane.`); dwie sesje 23:00–02:12 (wyciek 6 L/min)
                i 02:13–06:00 (wyciek 30 L/min), odcinek 02:10 dzielony przez dwa pliki (P95 z obu: 30)

Użycie: python3 -I tools/portret/karta_portretu.py KATALOG
"""

import datetime as dt
import os
import sys

_TU = os.path.dirname(os.path.abspath(__file__))
_KAL = os.path.join(os.path.dirname(_TU), "kalibracja")
for _p in (_TU, _KAL):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import syntetyczna as syn  # noqa: E402

D = dt.datetime
PIERWSZA = dt.date(2026, 11, 1)
AUTOSET = {"Mode": 1, "S.AS.MaxPress": 15, "S.AS.MinPress": 6, "S.AS.StartPress": 5, "S.EPR.Level": 2,
           "S.Mask": 1, "S.HumLevel": 4, "Duration": 420, "AHI": 2.0}
ASVAUTO = dict(syn.ASVAUTO_PANELU, Duration=420, AHI=2.0)
STALE = {"MaskPress.2s": 12.0, "Press.2s": 12.0, "EprPress.2s": 12.0, "Leak.2s": 0.1, "RespRate.2s": 15.0,
         "TidVol.2s": 0.46, "MinVent.2s": 7.5, "Snore.2s": 0.1, "FlowLim.2s": 0.05, "TgtVent.2s": 6.0}


def pld(sciezka, start, minuty, profil=None, bez=()):
    """PLD: profil(minuta) → {etykieta: wartość albo lista 30 wartości}; None = próbka niepoprawna; `bez`: kanały
    pominięte w pliku."""
    syg = [s for s in syn.PLD_SYG if s[0] not in bez]
    rek = []
    for i in range(minuty):
        w = dict(STALE)
        if profil:
            w.update(profil(i))
        r = []
        for s in syg:
            if s is syn.CRC:
                r.append([0])
                continue
            v = w[s[0]]
            r.append([syn._cyfrowa(s, x) for x in v] if isinstance(v, (list, tuple)) else [syn._cyfrowa(s, v)] * s[6])
        rek.append(r)
    syn.zapisz_edf(sciezka, start, 60, syg, rek)


def _noc(karta, dzien, start, minuty, profil=None, zdarzenia=(), eve=True, bez=()):
    k = os.path.join(karta, "DATALOG", dzien)
    os.makedirs(k, exist_ok=True)
    pref = start.strftime("%Y%m%d_%H%M%S")
    pld(os.path.join(k, pref + "_PLD.edf"), start, minuty, profil, bez)
    if eve:
        syn.adnotacje(os.path.join(k, pref + "_EVE.edf"), start, list(zdarzenia))
    return k


def _m(start, h, mi, s=0, dzien=1):
    """Chwila h:mi:s nocy zaczętej `start` (godziny < 12 następnego dnia)."""
    d = start.date() + dt.timedelta(days=1 if h < 12 else 0)
    return D(d.year, d.month, d.day, h, mi, s)


def utworz(karta):
    if os.path.exists(karta) and os.listdir(karta):
        raise SystemExit("Katalog nie jest pusty: %s" % karta)
    os.makedirs(karta, exist_ok=True)

    # P1
    s = D(2026, 11, 1, 23, 0)

    def p1(i):
        g = (s + dt.timedelta(minutes=i)).time()
        w = {}
        if dt.time(3, 0) <= g < dt.time(3, 30):
            w["Leak.2s"] = 0.3                        # 18 L/min
        if g == dt.time(5, 41):
            w["Leak.2s"] = [0.3] * 6 + [0.1] * 24      # 12 s wycieku
        if dt.time(5, 0) <= g < dt.time(5, 20):
            w["Press.2s"] = 14.6
        return w
    zd = [(_m(s, 4, 5 + j, 10), 12, "Hypopnea") for j in range(5)]
    zd += [(_m(s, 1, 30, 20), 11, "Obstructive Apnea"), (_m(s, 2, 0, 30), 15, "Central Apnea"),
           (_m(s, 2, 2, 40), 14, "Central Apnea"), (_m(s, 3, 40, 5), 10, "RERA")]
    _noc(karta, "20261101", s, 480, p1, zd)

    # P2
    k = os.path.join(karta, "DATALOG", "20261102")
    _noc(karta, "20261102", D(2026, 11, 2, 14, 0), 60, None, [])
    s1, s2 = D(2026, 11, 2, 22, 30), D(2026, 11, 3, 1, 20)
    pld(os.path.join(k, s1.strftime("%Y%m%d_%H%M%S") + "_PLD.edf"), s1, 150)
    pld(os.path.join(k, s2.strftime("%Y%m%d_%H%M%S") + "_PLD.edf"), s2, 320)
    syn.adnotacje(os.path.join(k, s1.strftime("%Y%m%d_%H%M%S") + "_EVE.edf"), s1,
                  [(_m(s1, 0, 15), 12, "Apnea"), (_m(s1, 3, 0), 12, "Apnea"), (_m(s1, 4, 0), 12, "Hypopnea")])

    # P3
    s = D(2026, 11, 3, 23, 30)

    def p3(i):
        g = (s + dt.timedelta(minutes=i)).time()
        w = {"Press.2s": 13.0, "EprPress.2s": 8.0, "MaskPress.2s": 12.8}
        if dt.time(2, 0) <= g < dt.time(2, 5):
            w["Press.2s"] = 15.0
        if dt.time(3, 0) <= g < dt.time(3, 5):
            w["Press.2s"] = 19.0
        if dt.time(4, 0) <= g < dt.time(4, 5):
            w["Press.2s"] = 14.9
        if dt.time(6, 0) <= g < dt.time(6, 10):
            w["EprPress.2s"], w["Press.2s"] = 9.6, 14.6
        return w
    _noc(karta, "20261103", s, 420, p3, [(_m(s, 1, 10, 30), 12, "Apnea"), (_m(s, 3, 1, 15), 14, "Hypopnea")])

    # P4
    _noc(karta, "20261104", D(2026, 11, 4, 23, 0), 360, None, eve=False, bez=("Leak.2s",))
    # P5
    _noc(karta, "20261105", D(2026, 11, 5, 23, 0), 360, lambda i: {"EprPress.2s": None}, [])
    # P6
    _noc(karta, "20261106", D(2026, 11, 6, 23, 0), 10, None, [])
    # P7
    s = D(2026, 11, 7, 23, 0)

    def p7(i):
        g = (s + dt.timedelta(minutes=i)).time()
        if dt.time(2, 0) <= g < dt.time(2, 5):
            ok = [12.0] * 20 + [None] * 10 if g == dt.time(2, 0) else [None] * 30
            return {"Press.2s": ok, "EprPress.2s": ok, "MaskPress.2s": ok, "Leak.2s": 0.5}
        return {}
    _noc(karta, "20261107", s, 420, p7, [])

    # P8
    k = os.path.join(karta, "DATALOG", "20261108")
    os.makedirs(k, exist_ok=True)
    s1, s2 = D(2026, 11, 8, 23, 0), D(2026, 11, 9, 2, 13)
    pld(os.path.join(k, s1.strftime("%Y%m%d_%H%M%S") + "_PLD.edf"), s1, 192)
    pld(os.path.join(k, s2.strftime("%Y%m%d_%H%M%S") + "_PLD.edf"), s2, 227, lambda i: {"Leak.2s": 0.5})
    syn.adnotacje(os.path.join(k, s1.strftime("%Y%m%d_%H%M%S") + "_EVE.edf"), s1, [])

    bez_mpa = dict(AUTOSET, **{"S.AS.MaxPress": None})
    dni = [AUTOSET, AUTOSET, ASVAUTO, AUTOSET, ASVAUTO, AUTOSET, AUTOSET, bez_mpa]
    syn.str_panelu(os.path.join(karta, "STR.edf"), PIERWSZA, dni)
    return karta


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    utworz(sys.argv[1])
    print("Syntetyczna karta Portretu: %s" % sys.argv[1])
