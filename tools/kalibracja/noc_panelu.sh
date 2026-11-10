#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Noc liczona kodem panelu z plików sesji wobec raportu nocy AirBridge, tolerancje WY9 (ADR-0036 pkt 5).
# Kompiluje tools/kalibracja/noc_panelu.cpp z plikami firmware/prototyp/src (GNU g++ albo clang++, C++17) do
# katalogu tymczasowego i uruchamia go. Pliki nocy i raporty zostają poza repozytorium (dane prywatne).
#
# Użycie:
#   bash tools/kalibracja/noc_panelu.sh [--pelny] KATALOG_DOBY RAPORT.json [STR.edf]
#     KATALOG_DOBY  pliki jednej doby terapii (np. kopia karty DATALOG/20261108 albo pliki pobrane z AirBridge:
#                   /api/storage/download?path=/DATALOG/20261108/…); wystarczą PLD, EVE, CSL (BRP i SAD tylko z --pelny)
#     RAPORT.json   odpowiedź /api/report?source=edf&day=N dla tej doby (N = dni od 1970-01-01, doba od południa)
#     STR.edf       opcjonalnie: ustawienia doby (M-21); bez niego wiersz M-21 jest pominięty
#   Kod wyjścia: 0 w tolerancji, 1 rozjazd, 2 błąd wejścia.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$REPO/firmware/prototyp/src"
CXX="${CXX:-g++}"
BUD="${TMPDIR:-/tmp}/cpap-eink-noc-panelu"
mkdir -p "$BUD"
PROG="$BUD/noc_panelu"
ZRODLA=("$REPO/tools/kalibracja/noc_panelu.cpp" "$SRC/noc_pliki.cpp" "$SRC/edf.cpp" "$SRC/str.cpp" "$SRC/airbridge.cpp"
        "$SRC/noc.cpp" "$SRC/wspolne.cpp" "$SRC/nawigacja.cpp" "$SRC/porownanie.cpp")
nowszy=0
for z in "${ZRODLA[@]}" "$SRC"/*.h; do [ "$z" -nt "$PROG" ] && nowszy=1; done
if [ ! -x "$PROG" ] || [ "$nowszy" = 1 ]; then
  "$CXX" -std=c++17 -O1 -I"$SRC" "${ZRODLA[@]}" -o "$PROG"
fi
exec "$PROG" "$@"
