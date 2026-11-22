#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Test różnicowy normy tymczasowej (ADR-0048):
# symulator doby przez DNI dni po zmianie ustawień (ASVAuto i AutoSet), raz z normą tymczasową, raz z nią wymuszenie
# wyłączoną (-DNORMA_BEZ_TYMCZASOWEJ w src/noc.cpp). Klatki mogą się różnić tylko na K1, K2 i „Co znaczą te liczby”;
# pliki PDF raportu (▶ 2 s w K2 w południe) i logi muszą być identyczne.
# Użycie: tools/prototyp/norma_tymczasowa_roznicowy.sh <katalog obiektów rdzenia z testy.sh> [katalog roboczy] [dni]
# Wołane z tools/prototyp/testy.sh (blokujące).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
FW="$REPO/firmware/prototyp"
OBJ="$1"; W="${2:-/tmp/cpap-norma-tymczasowa-roznicowy}"; DNI="${3:-10}"
T="$REPO/tools/prototyp/norma_tymczasowa"
source "$REPO/tools/prototyp/zrodla.sh"
rm -rf "$W"; mkdir -p "$W/bez"
OB=(); OBB=()
for z in "${ZRODLA[@]}"; do
  o="$OBJ/$(basename "$z" .cpp).o"; OB+=("$o")
  if [ "$(basename "$z")" = noc.cpp ]; then OBB+=("$W/bez/noc.o"); else OBB+=("$o"); fi
done
g++ -std=c++17 -O1 -DNORMA_BEZ_TYMCZASOWEJ "${INCLUDY[@]}" -c "$FW/src/noc.cpp" -o "$W/bez/noc.o"
g++ -std=c++17 -O1 "${INCLUDY[@]}" "$FW/test/symulator.cpp" "${OB[@]}" -o "$W/sym-gal"
g++ -std=c++17 -O1 "${INCLUDY[@]}" "$FW/test/symulator.cpp" "${OBB[@]}" -o "$W/sym-bez"
OD=2026-11-05
: > "$W/przyciski.txt"
for i in $(seq 0 $((DNI - 1))); do d=$(date -d "$OD +$i day" +%F)
  printf "${d}T07:15 prawy\n${d}T07:16 prawy\n${d}T07:17 prawy\n${d}T07:18 prawy\n${d}T07:19 prawy\n${d}T07:20 srodkowy\n" >> "$W/przyciski.txt"
  # w południe K1, K2 i raport (▶ 2 s w K2), żeby porównać też pliki PDF raportu
  printf "${d}T12:00 prawy\n${d}T12:01 prawy\n${d}T12:01:30 prawy 2s\n${d}T12:02 srodkowy\n" >> "$W/przyciski.txt"; done
KONIEC=$(date -d "$OD +$((DNI - 1)) day" +%F)
for tr in ASVAuto AutoSet; do
  python3 "$REPO/tools/symulator/generuj_zrzut.py" "$W/z-$tr" --nocy 80 >/dev/null
  [ $tr = AutoSet ] && python3 "$T/przerob.py" "$W/z-$tr/noce.csv" 0 AutoSet
  python3 "$T/scen_multi.py" $tr "$W/noce-$tr.txt"
  ap=(); [ $tr = AutoSet ] && ap=(--aparat AirSense_10_AutoSet,AutoSet)
  for b in gal bez; do
    "$W/sym-$b" --dane "$W/z-$tr" --od 2026-11-04T05:00 --do "${KONIEC}T23:59" --przyciski "$W/przyciski.txt" \
      --noce-scenariusza "$W/noce-$tr.txt" ${ap[@]+"${ap[@]}"} --wyjscie "$W/w-$b-$tr" >/dev/null
  done
  echo "== $tr: z normą tymczasową kontra wymuszony brak normy tymczasowej"
  python3 "$T/porownaj.py" "$W/w-gal-$tr" "$W/w-bez-$tr"
done
echo "  norma tymczasowa różnicowo: ok"
