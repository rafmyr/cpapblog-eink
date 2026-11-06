#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Plik importu historii dla panelu (ADR-0017, R-DANE-31) z tabeli nocy czytnika z etapu 2.

Wejście: CSV z tools/kalibracja/noce.py (kopia karty SD aparatu).
Wyjście: import.csv w katalogu systemu plików panelu, POZA repozytorium (WY12):
  data_poranka;wazna;m01_min;ahi;cisnienie_med;cisnienie_p95;wyciek_med;wyciek_p95;epap_med;epap_p95;ps_med;ps_p95;ustawienia
Kolejność kolumn to kontrakt z firmware: nowe kolumny tylko na końcu. W ASVAuto ciśnienie = IPAP
(`Press.2s`), EPAP = `EprPress.2s` (M-06, M-06p), PS = wsparcie (M-28, M-28p).
`ustawienia` = wartości tagów MOP/EAI/EAX/ANS/AXS z `ustawienia_tagi`, połączone „/”
(np. ASVAuto/6/10/3/15; brak tagu = pusty fragment, brak wszystkich = puste pole); po zmianie
ustawień norma metryk ciśnienia liczy się od nowa (R-NOR-07).
Puste pole = brak danych. Wgranie na panel:
  PLATFORMIO_DATA_DIR="<katalog>" pio run -t uploadfs   (w firmware/prototyp; nadpisuje cały system plików panelu)
"""
import argparse, csv, os, sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
POLA = [("m02_ahi", 2), ("m05_cisnienie_med", 1), ("m05p_cisnienie_p95", 1),
        ("m07_przeciek_med", 1), ("m07_przeciek_p95", 1),
        ("m06_wydechowe_med", 1), ("m06p_wydechowe_p95", 1), ("m28_wsparcie_med", 1), ("m28p_wsparcie_p95", 1)]
CISNIENIA = (1, 2, 5, 6, 7, 8)     # indeksy w POLA: ciśnienie, EPAP, PS (tylko z plików EDF)
NAGLOWEK = ("data_poranka;wazna;m01_min;ahi;cisnienie_med;cisnienie_p95;wyciek_med;wyciek_p95;"
            "epap_med;epap_p95;ps_med;ps_p95;ustawienia")
TAGI_USTAWIEN = ("MOP", "EAI", "EAX", "ANS", "AXS")


def ustawienia(tekst):
    """`MOP=ASVAuto;EAI=6;…` (kolumna ustawienia_tagi) → `ASVAuto/6/10/3/15`."""
    tagi = dict(p.split("=", 1) for p in (tekst or "").split(";") if "=" in p)
    if not any(t in tagi for t in TAGI_USTAWIEN):
        return ""
    return "/".join(tagi.get(t, "") for t in TAGI_USTAWIEN)


def wewnatrz(sciezka, katalog):
    """Czy ścieżka leży w katalogu; samefile na przodkach (macOS bez rozróżniania wielkości liter)."""
    p = os.path.realpath(sciezka)
    while True:
        if os.path.exists(p) and os.path.samefile(p, katalog):
            return True
        rodzic = os.path.dirname(p)
        if rodzic == p:
            return False
        p = rodzic


def liczba(v, miejsc):
    try:
        return f"{float(v):.{miejsc}f}"
    except (TypeError, ValueError):
        return ""


def main():
    a = argparse.ArgumentParser()
    a.add_argument("noce_csv")
    a.add_argument("katalog_fs", help="katalog systemu plików panelu poza repozytorium")
    a = a.parse_args()
    if wewnatrz(a.katalog_fs, REPO):
        sys.exit("Odmowa: prawdziwe noce nie mogą trafić do repozytorium (WY12).")
    os.makedirs(a.katalog_fs, exist_ok=True)
    wiersze = list(csv.DictReader(open(a.noce_csv, encoding="utf-8")))
    out, pominiete = [], 0
    for w in wiersze:
        if w.get("na_liscie") != "tak" or not w.get("used_seconds"):
            pominiete += 1
            continue
        m01 = round(float(w["used_seconds"]) / 60)
        # noc ważna tylko w trybie obsługiwanym (R-DANE-29); ciśnienie tylko z plików EDF, nie ze STR (R-NOR-08)
        wazna = w.get("wazna") == "tak" and w.get("tryb_obslugiwany") == "tak"
        pola = [liczba(w.get(k), m) for k, m in POLA]
        if w.get("zrodlo_cisnienia") != "edf":
            for i in CISNIENIA:
                pola[i] = ""
        out.append(";".join([w["data_poranka"], "1" if wazna else "0", str(m01)] + pola
                            + [ustawienia(w.get("ustawienia_tagi"))]))
    p = os.path.join(a.katalog_fs, "import.csv")
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(NAGLOWEK + "\n")
        f.write("\n".join(out) + "\n")
    wazne = sum(1 for l in out if l.split(";")[1] == "1")
    print(f"Zapisano {len(out)} nocy ({wazne} ważnych), pominięto {pominiete} dni bez nocy, do {p}")


if __name__ == "__main__":
    main()
