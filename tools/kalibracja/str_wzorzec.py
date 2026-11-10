#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Wzorzec odczytu STR.edf dla testu zgodności panelu (firmware/prototyp/test/testy_str_edf.cpp).

Czyta STR.edf klasą `Str` z noce.py i wypisuje na standardowe wyjście wiersze tekstu (separator `;`):
  D;dzień;etykieta;wartość     wartość fizyczna każdego sygnału doby (jedna próbka na rekord); `brak` = None
  U;dzień;tag;value;option     pola `settings.fields[]` z Str.ustawienia(); option `brak` = None
  K;dzień;id;przyrostek;wartość statystyka kanału ze STR (Str.statystyka(); przyrostki 50, 95, Max)
Liczby w zapisie repr (najkrótszy, dokładny dla double). Dni w kolejności rosnącej, etykiety w kolejności sygnałów.

Użycie: python3 -I tools/kalibracja/str_wzorzec.py STR.edf > wzorzec.txt
Tylko na plikach syntetycznych w repo (tools/kalibracja/syntetyczna.py --str-panelu); wynik z prawdziwego STR
zostaje poza repozytorium.
"""

import os
import sys

_TU = os.path.dirname(os.path.abspath(__file__))
if _TU not in sys.path:          # python3 -I nie dodaje katalogu skryptu do sys.path
    sys.path.insert(0, _TU)

import noce  # noqa: E402


def _l(v):
    return "brak" if v is None else repr(float(v))


def wzorzec(sciezka):
    s = noce.Str(sciezka)
    if s.blad:
        raise SystemExit(s.blad)
    w = []
    for dzien in sorted(s.dni):
        rek = s.dni[dzien]
        for etykieta, v in rek.items():
            w.append("D;%d;%s;%s" % (dzien, etykieta, _l(v)))
        _, pola = s.ustawienia(dzien)
        for p in pola:
            w.append("U;%d;%s;%s;%s" % (dzien, p["tag"], p["value"],
                                         "brak" if p["option"] is None else p["option"]))
        for id_kanalu in noce.ID_KANALOW:
            for przyrostek in ("50", "95", "Max"):
                w.append("K;%d;%s;%s;%s" % (dzien, id_kanalu, przyrostek,
                                            _l(s.statystyka(dzien, id_kanalu, przyrostek))))
    return w


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    print("\n".join(wzorzec(sys.argv[1])))
