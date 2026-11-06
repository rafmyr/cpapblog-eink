#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Zrzut syntetyczny plików panelu do symulatora doby.

Dane wymyślone (stałe ziarno losowania), bez prawdziwych danych zdrowotnych: można je commitować i z nich robić
obrazki do dokumentacji. Starsza połowa nocy ma źródło K (import z karty), nowsza A (AirBridge), tryb ASVAuto.

Wersja 0 (jak zrzut z panelu sprzed płaskie pliki, ten sam komplet co zrzut_plikow.py): noce.csv (20 pól),
czynniki.csv, humor.csv, bateria.csv oraz puste sypialnia.csv i sypialnia-co2.csv (jak na panelu bez czujnika CO2).
Wersja 2: jak wersja 1, ale dane/noce/RRRR.csv w wersji 2 (35 pól, pola wersji 2 puste).
Wersja 1 (jak): dane/noce/RRRR.csv z nagłówkiem `#cpap-eink noce 1`, nazwami pól i `#koniec n`, dane/czynniki.csv,
log/wybudzenia.csv.

Użycie:
  python3 tools/symulator/generuj_zrzut.py <katalog> [--wersja 0|1|2] [--nocy 60] [--ostatni-poranek 2026-11-04]
"""
import argparse
import datetime as dt
import os
import random
import sys

POLA_NOCE = ("data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;"
             "oa;ca;h;csr")
POLA_CZYNNIKI = "data;stan;kody;warstwa;termin;pokazano"
POLA_WYBUDZENIA = "epoch;mV;wifi_ms;aktywny_ms;powod"


def noce(nocy, ostatni):
    """Wiersze 20 pól (format zapiszNoc() z src/noc.cpp), od najstarszej nocy; data = data poranka."""
    los = random.Random(20261104)
    w = []
    for i in range(nocy):
        d = ostatni - dt.timedelta(days=nocy - 1 - i)
        zrodlo = "K" if i < nocy // 2 else "A"
        m01 = los.randint(330, 470)
        pola = [d.isoformat(), "1", str(m01), "%.2f" % los.uniform(0.8, 3.2),
                "%.2f" % los.uniform(9.0, 10.8), "%.2f" % los.uniform(12.0, 13.8),
                "%.2f" % los.uniform(1.5, 4.5), "%.2f" % los.uniform(7.0, 14.0),
                "%.2f" % los.uniform(6.0, 7.2), "%.2f" % los.uniform(7.5, 8.8),
                "%.2f" % los.uniform(3.0, 4.5), "%.2f" % los.uniform(6.0, 8.5),
                "ASVAuto/6/10/3/15", zrodlo, "0", "0",
                "%.2f" % los.uniform(0.1, 0.6), "%.2f" % los.uniform(0.3, 1.5),
                "%.2f" % los.uniform(0.2, 1.1), "%.2f" % los.uniform(0.0, 4.0)]
        w.append(";".join(pola))
    return w


def czynniki(ostatni):
    d = ostatni - dt.timedelta(days=3)
    return [f"{d.isoformat()};O;K;K;-1;1"]


def humor(ostatni):
    """Dziennik ekranów dnia (data;typ;klucz;tryb;scena;zdanie) z trzech poprzednich poranków."""
    return [f"{ostatni - dt.timedelta(days=k)};S2A;;D;;" for k in range(3, 0, -1)]


def wybudzenia(ostatni):
    t = int(dt.datetime(ostatni.year, ostatni.month, ostatni.day, 7, 0).timestamp())
    return [f"{t - 86400 * k};3950;5200;9100;zegar:okno_poranne" for k in range(3, 0, -1)]


def pisz(sciezka, wiersze):
    os.makedirs(os.path.dirname(sciezka), exist_ok=True)
    with open(sciezka, "w", encoding="utf-8") as f:
        f.write("".join(x + "\n" for x in wiersze))


# Pola `noce` dopisane w wersji 2 (src/magazyn.cpp rejestrFormatow(), src/noc.h wierszNocyV2()).
POLA_NOCE_V2 = "m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb"


def z_naglowkiem(nazwa, pola, wiersze, koniec=True, wersja=1):
    w = [f"#cpap-eink {nazwa} {wersja}", pola] + wiersze
    return w + ([f"#koniec {len(wiersze)}"] if koniec else [])


def generuj(katalog, wersja=0, nocy=60, ostatni=dt.date(2026, 11, 4)):
    n = noce(nocy, ostatni)
    if wersja == 0:
        pisz(os.path.join(katalog, "noce.csv"), n)
        pisz(os.path.join(katalog, "czynniki.csv"), czynniki(ostatni))
        pisz(os.path.join(katalog, "humor.csv"), humor(ostatni))
        pisz(os.path.join(katalog, "bateria.csv"), wybudzenia(ostatni))
        pisz(os.path.join(katalog, "sypialnia.csv"), [])
        pisz(os.path.join(katalog, "sypialnia-co2.csv"), [])
        return
    # wersja 1: jak panel (pole `import` puste: chwila importu nieznana); wersja 2: jak panel, pola wersji 2
    # puste (brak danych, tak jak noce przeniesione migracją 1 → 2)
    lata = {}
    for w in n:
        lata.setdefault(w[:4], []).append(w + ";" + (";" * POLA_NOCE_V2.count(";") + ";" if wersja == 2 else ""))
    pola = POLA_NOCE + ";import" + (";" + POLA_NOCE_V2 if wersja == 2 else "")
    for rok, w in lata.items():
        pisz(os.path.join(katalog, "dane", "noce", rok + ".csv"), z_naglowkiem("noce", pola, w, wersja=min(wersja, 2)))
    pisz(os.path.join(katalog, "dane", "czynniki.csv"), z_naglowkiem("czynniki", POLA_CZYNNIKI, czynniki(ostatni)))
    pisz(os.path.join(katalog, "dane", "humor.csv"),
         z_naglowkiem("humor", "data;typ;klucz;tryb;scena;zdanie", humor(ostatni), koniec=False))
    pisz(os.path.join(katalog, "log", "wybudzenia.csv"),
         z_naglowkiem("wybudzenia", POLA_WYBUDZENIA, wybudzenia(ostatni), koniec=False))


def main():
    a = argparse.ArgumentParser(description="Zrzut syntetyczny plików panelu do symulatora doby")
    a.add_argument("katalog")
    a.add_argument("--wersja", type=int, choices=(0, 1, 2), default=0,
                   help="0: płaskie pliki prototypu, 1: pliki wersji 1, 2: noce w wersji 2")
    a.add_argument("--nocy", type=int, default=60)
    a.add_argument("--ostatni-poranek", default="2026-11-04", help="data poranka ostatniej nocy (RRRR-MM-DD)")
    a = a.parse_args()
    try:
        ostatni = dt.date.fromisoformat(a.ostatni_poranek)
    except ValueError:
        sys.exit("zła data --ostatni-poranek (wzór 2026-11-04)")
    generuj(a.katalog, a.wersja, a.nocy, ostatni)
    print(f"zrzut syntetyczny wersji {a.wersja}: {a.nocy} nocy do poranka {ostatni} w {a.katalog}")


if __name__ == "__main__":
    main()
