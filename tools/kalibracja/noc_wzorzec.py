#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Wzorzec nocy z plików sesji dla testu zgodności panelu (firmware/prototyp/test/testy_noc_pliki.cpp, ADR-0036).

Liczy noce karty `noc_z_katalogu()` i `metryki()` z noce.py i wypisuje na standardowe wyjście wiersze `;`:
  N;dzień;klucz;wartość
Klucze: partial, events_known, present, klasyfikacja, used_seconds, oa, ca, ua, h, rera, ahi, csr_seconds,
csr_zrodlo, leak_high_seconds, leak_27_seconds, session_count, sesja.{i}.start_ms, sesja.{i}.end_ms,
kanal.{id}.{median|p95|min|max|source}, ps.median, ps.p95, m17, m18_start_min, tryb, m21, wazna,
ustawienie.{tag}. Brak wartości = `brak`; liczby w zapisie repr; prawda/fałsz jako 1/0.

Użycie: python3 -I tools/kalibracja/noc_wzorzec.py KARTA > wzorzec.txt
Tylko na kartach syntetycznych (tools/kalibracja/syntetyczna.py); wynik z prawdziwej karty zostaje poza repo.
"""

import os
import sys

_TU = os.path.dirname(os.path.abspath(__file__))
if _TU not in sys.path:          # python3 -I nie dodaje katalogu skryptu do sys.path
    sys.path.insert(0, _TU)

import noce  # noqa: E402


def _w(v):
    if v is None:
        return "brak"
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, float):
        return repr(v)
    return str(v)


def wzorzec(karta):
    w = []
    for r in noce.noce(karta):
        d = r["day"]
        m = r["metryki"]

        def dodaj(k, v):
            w.append("N;%d;%s;%s" % (d, k, _w(v)))
        for k in ("partial", "events_known", "present", "klasyfikacja_bezdechow", "used_seconds", "ahi",
                  "csr_seconds", "csr_zrodlo", "leak_high_seconds", "leak_27_seconds", "session_count"):
            dodaj(k, r[k])
        for k in ("oa", "ca", "ua", "h", "rera"):
            dodaj(k, r["counts"][k])
        for i, s in enumerate(r["sessions"]):
            dodaj("sesja.%d.start_ms" % i, s["start_ms"])
            dodaj("sesja.%d.end_ms" % i, s["end_ms"])
        for id_k, ch in sorted(r["channels"].items()):
            for p in ("median", "p95", "min", "max", "source"):
                dodaj("kanal.%s.%s" % (id_k, p), ch[p] if ch[p] != "" else None)
        dodaj("ps.median", r["wsparcie"]["median"])
        dodaj("ps.p95", r["wsparcie"]["p95"])
        dodaj("m17", m["m17_przerwy"])
        dodaj("m18_start_min", m["m18_start_min"])
        dodaj("tryb", m["tryb"])
        dodaj("wazna", m["wazna"])
        for p in r["settings"]["fields"]:
            dodaj("ustawienie.%s" % p["tag"], p["value"])
    return w


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    print("\n".join(wzorzec(sys.argv[1])))
