#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Portret nocy z kopii karty aparatu albo z plików doby z AirBridge: PNG, PBM i `portret.csv` (wzorzec).

Użycie:
  python3 -I tools/portret/portret.py KARTA --rano RRRR-MM-DD --wyjscie KATALOG [--norma-ps 5.0] [--zwykla 23:47]
          [--pierwszy "Tydzień jak zwykle."] [--linia "Co było inaczej? ↻ 2 s"] [--wszystkie]

KARTA: katalog z `STR.edf` i `DATALOG/RRRRMMDD/` (kopia karty albo pliki z AirBridge ułożone tak samo). Bez
`--zwykla` zwykła pora startu to mediana M-18 z 14 wcześniejszych nocy z kartą (co najmniej 3), inaczej M-18 tej
nocy. Bez `--norma-ps` (tryb ASV) mediana M-28 z 30 wcześniejszych nocy ważnych, gdy jest ich co najmniej 14
(R-NOR-07), inaczej „uczę się”. Liczby nocy (czas z maską, AHI) z noce.py. `--wszystkie`: każdy poranek karty.

Dane prawdziwe: katalog wyniku musi leżeć poza repozytorium (narzędzie odmawia zapisu do repo).
"""

import argparse
import datetime as dt
import os
import statistics
import sys

_TU = os.path.dirname(os.path.abspath(__file__))
_KAL = os.path.join(os.path.dirname(_TU), "kalibracja")
for _p in (_TU, _KAL):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import noce  # noqa: E402
import przebieg  # noqa: E402
import rysunek  # noqa: E402

REPO = os.path.dirname(os.path.dirname(_TU))
DNI = ("pn", "wt", "śr", "cz", "pt", "sb", "nd")


def _czas(sek):
    if sek is None:
        return "–"
    m = int(round(sek / 60.0))
    return "%d h %02d" % (m // 60, m % 60)


def _ahi(v):
    return "–" if v is None else ("%.1f" % v).replace(".", ",")


def _ustawienia(rek):
    w = {}
    for p in rek["settings"]["fields"]:
        if p.get("liczba") is not None:
            w[p["tag"]] = p["liczba"]
    return w


def dane_nocy(lista, i, norma_ps=None, zwykla=None, pierwszy="Tydzień jak zwykle.", linia=""):
    """Dane rysunku dla nocy lista[i] (rekordy noce.noce()); katalog doby z rekordu."""
    rek = lista[i]
    m = rek["metryki"]
    tryb = m["tryb"]
    katalog = rek["_katalog"]
    pr = przebieg.przebieg_doby(katalog, rek["day"], przebieg.prog_maksimum(tryb, _ustawienia(rek)),
                                tryb in ("AutoSet", "ASVAuto"))
    if zwykla is None:
        poprz = [x["metryki"]["m18_start_min"] for x in lista[max(0, i - 14):i]
                 if x["metryki"]["m18_start_min"] is not None]
        if len(poprz) >= 3:
            zwykla = int(statistics.median_low(poprz))
        elif m["m18_start_min"] is not None:
            zwykla = int(m["m18_start_min"])
        else:
            zwykla = pr["sesje"][0][0] if pr["sesje"] else 11 * 60
    if norma_ps is None and tryb in rysunek.ASV:
        ps = [x["metryki"]["m28_wsparcie_med"] for x in lista[max(0, i - 30):i]
              if x["metryki"]["wazna"] and x["metryki"]["m28_wsparcie_med"] is not None]
        if len(ps) >= 14:
            norma_ps = statistics.median_low(ps)
    rano = dt.date.fromisoformat(pr["data"])
    p95 = (rek["channels"].get("Leak") or {}).get("p95")
    return {
        "przebieg": pr,
        "tryb": tryb,
        "norma_ps10": None if norma_ps is None else int(round(norma_ps * 10)),
        "zwykla": zwykla,
        "pierwszy": pierwszy,
        "czas": _czas(rek["used_seconds"]),
        "ahi": _ahi(rek["ahi"]),
        "niepewna": p95 is not None and p95 >= 15,
        "linia": linia,
        "naglowek": ("%s %s" % (DNI[rano.weekday()], rano.strftime("%d.%m")), "dane 07:12"),
    }


def _wewnatrz_repo(sciezka):
    r = os.path.realpath(REPO)
    s = os.path.realpath(sciezka)
    return s == r or s.startswith(r + os.sep)


def main(argv=None):
    a = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    a.add_argument("karta")
    a.add_argument("--rano")
    a.add_argument("--wszystkie", action="store_true")
    a.add_argument("--wyjscie", required=True)
    a.add_argument("--norma-ps", type=float)
    a.add_argument("--zwykla", help="hh:mm")
    a.add_argument("--pierwszy", default="Tydzień jak zwykle.")
    a.add_argument("--linia", default="")
    a.add_argument("--syntetyczna", action="store_true", help="karta syntetyczna: wolno pisać do repozytorium")
    x = a.parse_args(argv)
    if _wewnatrz_repo(x.wyjscie) and not x.syntetyczna:
        print("Odmowa: wynik z prawdziwej karty nie może trafić do repozytorium (%s)" % x.wyjscie, file=sys.stderr)
        return 2
    lista = noce.noce(x.karta)
    for r in lista:
        r["_katalog"] = os.path.join(x.karta, "DATALOG", noce.data_z_dnia(r["day"]).strftime("%Y%m%d"))
    zw = None
    if x.zwykla:
        g, mi = (int(v) for v in x.zwykla.split(":"))
        zw = (g * 60 + mi - 720) % 1440
    os.makedirs(x.wyjscie, exist_ok=True)
    zrobione = 0
    for i, r in enumerate(lista):
        rano = noce.data_z_dnia(r["day"] + 1).isoformat()
        if not x.wszystkie and rano != x.rano:
            continue
        d = dane_nocy(lista, i, x.norma_ps, zw, x.pierwszy, x.linia)
        baza = os.path.join(x.wyjscie, "portret-" + rano)
        with open(baza + ".csv", "w", encoding="utf-8") as f:
            f.write(przebieg.tekst_pliku(d["przebieg"]))
        o = rysunek.rysuj(d)
        if o is None:
            print("%s: noc bez terapii (sesje razem < 15 min), bez Portretu" % rano)
            continue
        with open(baza + ".txt", "w", encoding="utf-8") as f:  # wejście rysunku dla testu panelu (T-TEST-415)
            f.write("tryb=%s\nnorma_ps10=%s\nzwykla=%d\npierwszy=%s\nczas=%s\nahi=%s\nniepewna=%d\nlinia=%s\n"
                    % (d["tryb"], "" if d["norma_ps10"] is None else d["norma_ps10"], d["zwykla"], d["pierwszy"],
                       d["czas"], d["ahi"], 1 if d["niepewna"] else 0, d["linia"]))
            f.write("podpisy=%s\nlegenda=%s\n" % ("|".join(rysunek.podpisy(d)), "|".join(rysunek.legenda(d))))
        o.zapisz_pbm(baza + ".pbm")
        try:
            o.zapisz_png(baza + ".png")
        except ImportError:
            pass
        print("%s: %s; okno %s; podpisy: %s" % (rano, d["tryb"], rysunek.hhmm(rysunek.okno(d)),
                                                " | ".join(rysunek.podpisy(d))))
        zrobione += 1
    return 0 if zrobione or x.wszystkie else 1


if __name__ == "__main__":
    sys.exit(main())
