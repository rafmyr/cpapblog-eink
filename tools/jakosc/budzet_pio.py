#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Budżet rozmiaru obrazu firmware z logu `pio run`.

Co robi
-------
Czyta log kompilacji PlatformIO i znajduje dwie linie podsumowania:

  RAM:   [===       ]  32.1% (used 105192 bytes from 327680 bytes)
  Flash: [====      ]  42.6% (used 1785585 bytes from 4194304 bytes)

Flash to slot aplikacji z tablicy partycji (od firmware/partycje-e1001.csv: 4 MB, 4 194 304 B; PlatformIO bierze
rozmiar z tablicy, skrypt czyta go z logu, bez stałej),
RAM to statyczna pamięć (.data + .bss) wewnętrznego SRAM. Porównuje
z budżetem: Flash ≤ 80% slotu, RAM ≤ 60% (progi `[PRÓG]` z sekcji 5 metody;
zmiana progu = zmiana tu i w metodzie). Wypisuje liczby także do
$GITHUB_STEP_SUMMARY, gdy zmienna jest ustawiona.

Użycie
------
  pio run -e e1001 | tee pio.log
  python3 tools/jakosc/budzet_pio.py pio.log [--flash 80] [--ram 60]

Kod wyjścia: 0 = w budżecie, 1 = przekroczony, 2 = brak linii w logu.
"""

import argparse
import os
import re
import sys

FLASH_PROG = 80.0  # [PRÓG] % slotu aplikacji
RAM_PROG = 60.0    # [PRÓG] % statycznego RAM
LINIA_RE = re.compile(r"^(RAM|Flash):\s*\[[^\]]*\]\s*([\d.]+)%\s*\(used (\d+) bytes from (\d+) bytes\)", re.M)


def odczyt(log):
    """{"RAM": (procent, użyte, z), "Flash": (…)}; ostatnie wystąpienie każdej linii."""
    wynik = {}
    for m in LINIA_RE.finditer(log):
        uzyte, z = int(m.group(3)), int(m.group(4))
        wynik[m.group(1)] = (100.0 * uzyte / z if z else 0.0, uzyte, z)
    return wynik


def main(argv=None):
    ap = argparse.ArgumentParser(description="Budżet flash i RAM z logu pio run.")
    ap.add_argument("log")
    ap.add_argument("--flash", type=float, default=FLASH_PROG)
    ap.add_argument("--ram", type=float, default=RAM_PROG)
    a = ap.parse_args(argv)
    with open(a.log, encoding="utf-8", errors="replace") as f:
        w = odczyt(f.read())
    if "RAM" not in w or "Flash" not in w:
        print("Budżet: w logu nie ma linii RAM i Flash z podsumowania pio run.", file=sys.stderr)
        return 2
    przekroczone = []
    wiersze = []
    for nazwa, prog in (("Flash", a.flash), ("RAM", a.ram)):
        proc, uzyte, z = w[nazwa]
        ok = proc <= prog
        if not ok:
            przekroczone.append(nazwa)
        wiersze.append("| %s | %d B z %d B | %.1f%% | ≤ %.0f%% | %s |" % (nazwa, uzyte, z, proc, prog,
                                                                        "w budżecie" if ok else "PRZEKROCZONY"))
    tabela = "| Budżet | Użyte | Udział | Próg | Wynik |\n|---|---|---|---|---|\n" + "\n".join(wiersze)
    print(tabela)
    podsumowanie = os.environ.get("GITHUB_STEP_SUMMARY")
    if podsumowanie:
        with open(podsumowanie, "a", encoding="utf-8") as f:
            f.write("### Rozmiar firmware (e1001)\n\n" + tabela + "\n")
    if przekroczone:
        print("BŁĄD: budżet przekroczony: " + ", ".join(przekroczone))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
