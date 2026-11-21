#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Testy logiki prototypu panelu bez Arduino (zwykłe g++) i podgląd ekranów w PNG.
# Użycie: tools/prototyp/testy.sh [katalog na podgląd PNG, domyślnie /tmp/cpap-eink-podglad]
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"

# Środowisko: GNU g++ (nie Apple clang), GNU grep z -P, GNU xargs i nproc. Na macOS systemowe narzędzia są inne
# (clang jako g++, BSD grep bez -P, BSD xargs z limitem 255 B dla -I), więc skrypt kończy się tu z opisem,
# zamiast paść później z mylącym komunikatem.
braki=()
if ! command -v g++ >/dev/null 2>&1; then
  braki+=("brak g++")
elif g++ -dM -E -x c++ /dev/null 2>/dev/null | grep -q '__clang__'; then
  braki+=("g++ to clang ($(command -v g++)), potrzebny GNU g++")
fi
if ! grep --version 2>/dev/null | head -1 | grep -q 'GNU' || ! echo a | grep -qP 'a' 2>/dev/null; then
  braki+=("grep bez -P, potrzebny GNU grep")
fi
if ! xargs --version 2>/dev/null | head -1 | grep -q 'GNU'; then
  braki+=("xargs nie jest GNU xargs")
fi
if ! command -v nproc >/dev/null 2>&1; then
  braki+=("brak nproc (GNU coreutils)")
fi
if [ "${#braki[@]}" -gt 0 ]; then
  echo "BŁĄD: środowisko testów niepełne:" >&2
  for b in "${braki[@]}"; do echo "  - $b" >&2; done
  cat >&2 <<'OPIS'
macOS (Homebrew):
  brew install gcc grep findutils coreutils
  eval "$(bash tools/prototyp/srodowisko-macos.sh)"   # katalog z dowiązaniami g++, gcc, grep, xargs, nproc na PATH
Linux (Debian, Ubuntu):
  sudo apt-get install g++ grep findutils coreutils
OPIS
  exit 1
fi

FW="$REPO/firmware/prototyp"
BUD="$FW/.pio/testy"
PODGLAD="${1:-/tmp/cpap-eink-podglad}"
mkdir -p "$BUD" "$PODGLAD"
source "$REPO/tools/prototyp/zrodla.sh"
FLAGI=(-std=c++17 -O1 "${INCLUDY[@]}")
JOBY="$(nproc 2>/dev/null || echo 2)"

echo "kompilacja testów"
# rdzeń plik po pliku, równolegle, z pełnym zestawem ostrzeżeń; potem testy i podgląd z tych samych obiektów
rm -f "$BUD"/*.o
printf '%s\n' "${ZRODLA[@]}" | xargs -P "$JOBY" -I{} sh -c 'g++ "$@" -c "{}" -o "'"$BUD"'/$(basename "{}" .cpp).o"' _ \
  "${FLAGI[@]}" "${OSTRZEZENIA[@]}"
OBIEKTY=()
for z in "${ZRODLA[@]}"; do OBIEKTY+=("$BUD/$(basename "$z" .cpp).o"); done
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy.cpp" "${OBIEKTY[@]}" -o "$BUD/testy"
"$BUD/testy"

echo "teksty ekranów: forma grzecznościowa małą literą w środku zdania, bez P95 na ekranach dnia"
# grep po literałach w kodzie tekstów ekranów dnia, kart i S9: wielka litera tylko na początku zdania (po `. `, `„`, `"`)
if grep -nP '"[^"]*[^.!?„" ]\s(Ciebie|Tobie|Tobą|Twój|Twoja|Twoje|Twojej|Twoją|Twoich|Twoim|Ci|Cię)\b' \
     "$FW/src/humor.cpp" "$FW/src/dobranoc.cpp" "$FW/src/dzien.cpp" "$FW/src/ekrany.cpp" "$FW/src/kokpit.cpp"; then
  echo "BŁĄD: forma grzecznościowa wielką literą w środku zdania"; exit 1
fi
if grep -nP '"[^"]*P95[^"]*"' "$FW/src/dzien.cpp" "$FW/src/humor.cpp" "$FW/src/dobranoc.cpp"; then
  echo "BŁĄD: P95 w tekście ekranu dnia albo karty"; exit 1
fi
echo "  teksty: ok"

echo "składnia main.cpp i adapterów ESP32 z atrapami nagłówków Arduino i ESP-IDF (nie zastępuje kompilacji pio)"
for z in "$FW/src/main.cpp" "$FW/src/adaptery_esp32.cpp"; do
  g++ -std=gnu++17 -fsyntax-only "${OSTRZEZENIA[@]}" -DZRZUT_EKRANU=1 -DARDUINO_ATRAPA -I"$FW/test/atrapy/arduino" \
    -I"$FW/test/atrapy" -I"$FW/src" -I"$FW/include" "$z"
  # obraz pomiarowy `e1001_pomiar` (wiersze POMIAR, historia w NVS; R-NF-25)
  g++ -std=gnu++17 -fsyntax-only "${OSTRZEZENIA[@]}" -DPOMIAR_CZASU=1 -DARDUINO_ATRAPA -I"$FW/test/atrapy/arduino" \
    -I"$FW/test/atrapy" -I"$FW/src" -I"$FW/include" "$z"
done
# obraz wydania bez sekretów (tools/wydanie/): main.cpp bez secrets.h
g++ -std=gnu++17 -fsyntax-only "${OSTRZEZENIA[@]}" -DBEZ_SEKRETOW -DWERSJA_WYDANIA='"9.9.9"' -DARDUINO_ATRAPA \
  -I"$FW/test/atrapy/arduino" -I"$FW/test/atrapy" -I"$FW/src" -I"$FW/include" "$FW/src/main.cpp"
# flagi obrazu wydania obejmują wszystkie pliki (wydaj.py: PLATFORMIO_BUILD_FLAGS), więc żadna nie może zderzyć się
# z nazwą w rdzeniu (po scaleniu z EDF `-DWERSJA` psuło `edf::Blad::WERSJA`); składnia rdzenia z tymi flagami
for z in "${ZRODLA[@]}"; do
  g++ -std=gnu++17 -fsyntax-only "${OSTRZEZENIA[@]}" -DBEZ_SEKRETOW -DWERSJA_WYDANIA='"9.9.9"' \
    -DREPO_WYDAN='"a/b"' "${INCLUDY[@]}" "$z"
done
echo "  składnia main.cpp, adaptery_esp32.cpp i rdzenia z flagami wydania: ok"

echo "podgląd ekranów: $PODGLAD"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/podglad.cpp" "${OBIEKTY[@]}" -o "$BUD/podglad"
"$BUD/podglad" "$PODGLAD"
if python3 -c "import PIL" 2>/dev/null; then
  python3 - "$PODGLAD" <<'PY'
import glob, os, sys
from PIL import Image
for p in sorted(glob.glob(os.path.join(sys.argv[1], "*.pbm"))):
    Image.open(p).convert("1").save(p[:-4] + ".png")
    os.remove(p)
print("PNG gotowe")
PY
fi

echo "testy wybudzenia (test/testy_wybudzenie.cpp: decyzje wybudzenia, siatka aktualizacji, dyrygent z atrapami)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_wybudzenie.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_wybudzenie"
"$BUD/testy_wybudzenie"

echo "scenariusze dyrygenta z atrapami (test/testy_dyrygenta.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_dyrygenta.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_dyrygenta"
"$BUD/testy_dyrygenta"

echo "testy planu wybudzeń (test/testy_plan_wybudzen.cpp: plan wybudzenia, Wi-Fi i czuwanie, symulacja dób z atrapami)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_plan_wybudzen.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_plan_wybudzen"
"$BUD/testy_plan_wybudzen"

echo "testy slotu dziennego w planie wybudzeń (test/testy_slot_dzienny_w_planie.cpp: slot dzienny w planie wybudzeń, R-EKR-60 w slocie)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_slot_dzienny_w_planie.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_slot_dzienny_w_planie"
"$BUD/testy_slot_dzienny_w_planie"

echo "testy szybszego wyniku nocy rano (test/testy_poranek_szybciej.cpp: R-DANE-61 gęste okno, R-DANE-62 powtórka przy przetwarzaniu, R-PRZ-22 pobranie rano po przycisku)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_poranek_szybciej.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_poranek_szybciej"

echo "testy przypadków brzegowych (test/testy_poranek_brzegowe.cpp: szybszy wynik nocy rano)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_poranek_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_poranek_brzegowe"
"$BUD/testy_poranek_brzegowe"
"$BUD/testy_poranek_szybciej"
echo "testy rytmu na zasilaniu zewnętrznym (test/testy_zasilanie_rytm.cpp: R-DANE-63 okno poranne co 5 min, R-DANE-64 gęste okno co 2 min, R-DANE-65 stan zasilania; bateria bez zmian)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_zasilanie_rytm.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_zasilanie_rytm"
"$BUD/testy_zasilanie_rytm"
echo "testy limitu ładowania na kablu (test/testy_ladowarka.cpp: R-DANE-66…R-DANE-68, ADR-0046: rejestry SY6974B na atrapie, twarda granica VREG ≤ 4208 mV i nigdy w górę przy limicie, odczyt z panelu 16.11.2026, formularz, dyrygent)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_ladowarka.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_ladowarka"
"$BUD/testy_ladowarka"
echo "testy brzegowe limitu ładowania (test/testy_ladowarka_brzegowe.cpp: T-TEST-491: własny model SY6974B, błąd na każdym kroku, restart, wpisy nieaktualne)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_ladowarka_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_ladowarka_brzegowe"
"$BUD/testy_ladowarka_brzegowe"
echo "testy brzegowe limitu ładowania, część 2 (test/testy_ladowarka_brzegowe_2.cpp: niezależny model SY6974B i NVS, T-TEST-492: własności zapisu, błąd odczytu NVS i wpis poniżej limitu blokujące, błąd I2C i NVS na każdym kroku; pełny przegląd: BRZEGOWE2_PELNY=1)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_ladowarka_brzegowe_2.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_ladowarka_brzegowe_2"
"$BUD/testy_ladowarka_brzegowe_2"

echo "testy belki informacyjnej (test/testy_belka.cpp: R-EKR-63…R-EKR-66 pola, ustępowanie środka, ikona baterii, ekrany bez belki; R-KON-69 niska bateria w stopce dnia)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_belka.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_belka"
"$BUD/testy_belka"

echo "testy magazynu danych (test/testy_magazyn_danych.cpp: magazyn danych, migracje, układ pamięci 32 MB, przywracanie)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_magazyn_danych.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_magazyn_danych"
"$BUD/testy_magazyn_danych"

echo "testy przypadków brzegowych magazynu (test/testy_magazyn_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_magazyn_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_magazyn_brzegowe"
"$BUD/testy_magazyn_brzegowe"
echo "testy przypadków brzegowych magazynu, część 2 (test/testy_magazyn_brzegowe_2.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_magazyn_brzegowe_2.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_magazyn_brzegowe_2"
"$BUD/testy_magazyn_brzegowe_2"
echo "testy (test/testy_silnik_regul.cpp: silnik reguł, zestaw wbudowany, rodzaje warunków, pamięć reguł)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_silnik_regul.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_silnik_regul"
"$BUD/testy_silnik_regul"

echo "testy (test/testy_zdarzenia_e.cpp: zdarzenia E z katalogu reguł, rodzaje E src/rodzaje_e.cpp)"
# src/rodzaje_e.cpp kompilowany tu, dopóki nie jest w ZRODLA (tools/prototyp/zrodla.sh, poza plikami)
ROD_E_OBJ=()
if [[ " ${ZRODLA[*]} " != *"/rodzaje_e.cpp "* ]]; then
  g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" -c "$FW/src/rodzaje_e.cpp" -o "$BUD/rodzaje_e.o"
  ROD_E_OBJ=("$BUD/rodzaje_e.o")
fi
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_zdarzenia_e.cpp" "${OBIEKTY[@]}" ${ROD_E_OBJ[@]+"${ROD_E_OBJ[@]}"} \
  -o "$BUD/testy_zdarzenia_e"
"$BUD/testy_zdarzenia_e"
echo "testy przypadków brzegowych: zdarzenia E (test/testy_zdarzenia_e_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_zdarzenia_e_brzegowe.cpp" "${OBIEKTY[@]}" ${ROD_E_OBJ[@]+"${ROD_E_OBJ[@]}"} \
  -o "$BUD/testy_zdarzenia_e_brzegowe"
"$BUD/testy_zdarzenia_e_brzegowe" || echo " są otwarte przypadki (wyżej)"
echo "testy (test/testy_wzorce_l_hipotezy_h.cpp: katalog L/H wobec spec, stany wzorca L, „wiem”, „Omówione”, info(), wzorce L z zestawu wbudowanego, dyrygent z ekranem L-03)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_wzorce_l_hipotezy_h.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_wzorce_l_hipotezy_h"
"$BUD/testy_wzorce_l_hipotezy_h" "$REPO"
echo "testy przypadków brzegowych: wzorce L i hipotezy H (test/testy_wzorce_l_hipotezy_h_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_wzorce_l_hipotezy_h_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_wzorce_l_hipotezy_h_brzegowe"
"$BUD/testy_wzorce_l_hipotezy_h_brzegowe"
echo "testy (test/testy_kolejka_linii.cpp: kolejka linii 7.5, ekran L, „Otwarte sprawy”, „wiem” i „Omówione”, pytania tak/nie, buzzer A, uspokojenie, przyciski)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_kolejka_linii.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kolejka_linii"
"$BUD/testy_kolejka_linii"

echo "symulator doby (test/symulator.cpp): pełny zestaw ostrzeżeń, doba na zrzucie syntetycznym"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/symulator.cpp" "${OBIEKTY[@]}" -o "$BUD/symulator"
SYM="$(mktemp -d "${TMPDIR:-/tmp}/cpap-eink-sym-XXXXXX")"
python3 "$REPO/tools/symulator/generuj_zrzut.py" "$SYM/zrzut" >/dev/null
# ▶ z ekranu dnia → K1 rano i drugi raz po sprawdzeniu o 11:00 (R-KOK-26: godzina danych to import nocy, nie ostatni odczyt)
printf '2026-11-04T07:15 prawy\n2026-11-04T07:17 srodkowy\n2026-11-04T13:00 prawy\n' > "$SYM/przyciski.txt"
"$BUD/symulator" --dane "$SYM/zrzut" --od 2026-11-04T05:00 --do 2026-11-04T23:59 --przyciski "$SYM/przyciski.txt" \
  --wyjscie "$SYM/wynik"
ls "$SYM/wynik" | grep -q -- '-0715-K1\.pbm$' || { echo "BŁĄD: symulator bez K1 po ▶ o 07:15"; exit 1; }
GD_RANO="$(sed -n 's/^2026-11-04 07:15:00  K1: godzina odczytu \([0-9:]*\) (R-KOK-26)$/\1/p' "$SYM/wynik/log-panelu.txt" | head -1)"
GD_POL="$(sed -n 's/^2026-11-04 13:00:00  K1: godzina odczytu \([0-9:]*\) (R-KOK-26)$/\1/p' "$SYM/wynik/log-panelu.txt" | head -1)"
if [ -z "$GD_RANO" ] || [ "$GD_RANO" != "$GD_POL" ]; then
  echo "BŁĄD: godzina odczytu K1 rano „$GD_RANO”, o 13:00 „$GD_POL” (R-KOK-26: ma być ta sama chwila importu nocy)"; exit 1
fi
ls "$SYM/wynik" | grep -q -- '-Dobranoc\.pbm$' || { echo "BŁĄD: symulator bez ekranu Dobranoc"; exit 1; }
echo "  symulator: ok ($SYM/wynik)"

echo "testy przypadków brzegowych: kolejka linii, ekran L, granice bezpieczeństwa (test/testy_kolejka_linii_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_kolejka_linii_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kolejka_linii_brzegowe"
"$BUD/testy_kolejka_linii_brzegowe"
echo "testy przypadków brzegowych: silnik reguł (test/testy_silnik_regul_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_silnik_regul_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_silnik_regul_brzegowe"
"$BUD/testy_silnik_regul_brzegowe"

echo "testy (test/testy_rekord_nocy.cpp: rekord nocy M-13, M-17, pna, srn; noce wersja 2)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_rekord_nocy.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_rekord_nocy"
"$BUD/testy_rekord_nocy"
echo "testy (test/testy_str_edf.cpp: czytnik EDF, noce ze STR.edf, zgodność z tools/kalibracja/noce.py, pierwszeństwo A/K/S, noce w wersji 4, STR w dyrygencie)"
STRK="$(mktemp -d "${TMPDIR:-/tmp}/cpap-eink-str-XXXXXX")"
python3 -I "$REPO/tools/kalibracja/syntetyczna.py" --str-panelu "$STRK/STR.edf" >/dev/null
python3 -I "$REPO/tools/kalibracja/str_wzorzec.py" "$STRK/STR.edf" > "$STRK/wzorzec.txt"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_str_edf.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_str_edf"
"$BUD/testy_str_edf" "$STRK"
echo "testy przypadków brzegowych (test/testy_str_edf_brzegowe.cpp: STR.edf)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_str_edf_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_str_edf_brzegowe"
"$BUD/testy_str_edf_brzegowe"  # blokuje
echo "testy (test/testy_noc_pliki.cpp: noc z plików sesji, zgodność z tools/kalibracja/noce.py, brzegowe, dyrygent; ADR-0036)"
NOCK="$(mktemp -d "${TMPDIR:-/tmp}/cpap-eink-noc-XXXXXX")"
python3 -I "$REPO/tools/kalibracja/syntetyczna.py" "$NOCK/karta" >/dev/null
python3 -I "$REPO/tools/kalibracja/syntetyczna.py" --ps "$NOCK/karta_ps" >/dev/null
python3 -I "$REPO/tools/kalibracja/syntetyczna.py" --noce-panelu "$NOCK/karta_panelu" >/dev/null
for k in karta karta_ps karta_panelu; do
  python3 -I "$REPO/tools/kalibracja/noc_wzorzec.py" "$NOCK/$k" > "$NOCK/$k.txt"
done
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_noc_pliki.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_noc_pliki"
"$BUD/testy_noc_pliki" "$NOCK"  # blokuje
echo "testy przypadków brzegowych (test/testy_noc_pliki_brzegowe.cpp: noc z plików sesji)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" -I"$FW/test" "$FW/test/testy_noc_pliki_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_noc_pliki_brzegowe"
"$BUD/testy_noc_pliki_brzegowe"  # blokuje
echo "testy (test/testy_portret.cpp: Portret nocy, odcinki 5 min wobec tools/portret, plik T-DANE-38, rysunek; ADR-0040)"
PORK="$(mktemp -d "${TMPDIR:-/tmp}/cpap-eink-portret-XXXXXX")"
python3 -I "$REPO/tools/portret/karta_portretu.py" "$PORK/karta" >/dev/null
python3 -I "$REPO/tools/portret/portret.py" "$PORK/karta" --wszystkie --syntetyczna --norma-ps 5.0 --wyjscie "$PORK/wzorzec" >/dev/null
python3 -I "$REPO/tools/portret/portret.py" "$PORK/karta" --rano 2026-11-02 --syntetyczna --linia "Co było inaczej? ↻ 2 s" \
  --wyjscie "$PORK/wzorzec-linia" >/dev/null
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_portret.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_portret"
"$BUD/testy_portret" "$PORK"  # blokuje
echo "testy (test/testy_porownanie.cpp: tryb porównawczy noc z plików wobec raportu JSON, U-05, /dane/porownanie.csv)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" -I"$FW/test" "$FW/test/testy_porownanie.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_porownanie"
"$BUD/testy_porownanie"  # blokuje
echo "testy przypadków brzegowych (test/testy_porownanie_brzegowe.cpp: tryb porównawczy)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" -I"$FW/test" "$FW/test/testy_porownanie_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_porownanie_brzegowe"
"$BUD/testy_porownanie_brzegowe"  # blokuje
echo "testy linii zaproszenia według ekranu dnia, drogi do „Sprzętu” i linii uwagi (test/testy_zaproszenie_linia_uwagi.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" -I"$FW/test" "$FW/test/testy_zaproszenie_linia_uwagi.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_zaproszenie_linia_uwagi"
"$BUD/testy_zaproszenie_linia_uwagi"  # wszystkie zielone, więc blokuje
echo "testy (test/testy_karty_z_budzikiem.cpp: karty z budzikiem, komplet scen, limity, przerywnik S2d)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_karty_z_budzikiem.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_karty_z_budzikiem"
"$BUD/testy_karty_z_budzikiem"
echo "testy przypadków brzegowych: karty z budzikiem (test/testy_karty_z_budzikiem_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_karty_z_budzikiem_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_karty_z_budzikiem_brzegowe"
"$BUD/testy_karty_z_budzikiem_brzegowe"  # po poprawkach wszystkie zielone, więc blokuje
python3 "$REPO/tools/prototyp/sceny_karty.py" --sprawdz
echo "testy przypadków brzegowych: rekord nocy (test/testy_rekord_nocy_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_rekord_nocy_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_rekord_nocy_brzegowe"
"$BUD/testy_rekord_nocy_brzegowe"  # wszystkie zielone (wersja 4 pliku nocy), więc blokuje

echo "testy (test/testy_kokpit_k1_k2.cpp: K1 i K2 według trybu, słowa rośnie/spada R-KOK-16)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_kokpit_k1_k2.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kokpit_k1_k2"
"$BUD/testy_kokpit_k1_k2"

echo "testy przypadków brzegowych: kokpit K1 i K2 (test/testy_kokpit_k1_k2_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_kokpit_k1_k2_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kokpit_k1_k2_brzegowe"
"$BUD/testy_kokpit_k1_k2_brzegowe"
echo "testy poprawek czytelności kokpitu, U-01, „Sprzęt”, S-K-01 i slotu dziennego (test/testy_kokpit_legenda_u01_sprzet.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_kokpit_legenda_u01_sprzet.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kokpit_legenda_u01_sprzet"
"$BUD/testy_kokpit_legenda_u01_sprzet"
echo "testy wykresu slotu dziennego przy linii uwagi, U-01, „Sprzętu” i stopki karty (test/testy_slot_ustawienia_sprzet.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_slot_ustawienia_sprzet.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_slot_ustawienia_sprzet"
"$BUD/testy_slot_ustawienia_sprzet"  # wszystkie zielone, więc blokuje

echo "testy K2 z wierszami bez danych w jednej linii, S7, „Co znaczą te liczby” (test/testy_k2_zwijanie_s7_liczby.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_k2_zwijanie_s7_liczby.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_k2_zwijanie_s7_liczby"
"$BUD/testy_k2_zwijanie_s7_liczby"
echo "testy przekrojowe K2, kroku S-K-35, stopki karty, pustych spraw i E-23 (test/testy_kokpit_karty_przekroj.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_kokpit_karty_przekroj.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kokpit_karty_przekroj"
"$BUD/testy_kokpit_karty_przekroj"  # wszystkie zielone, więc blokuje

echo "testy (test/testy_czynniki_odkrycia.cpp: czynniki nocy, odkrycia O-03…O-12 z testem permutacyjnym, S7, S8, Co znaczą te liczby)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_czynniki_odkrycia.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_czynniki_odkrycia"
"$BUD/testy_czynniki_odkrycia"
echo "testy przypadków brzegowych: czynniki nocy i odkrycia (test/testy_czynniki_odkrycia_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_czynniki_odkrycia_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_czynniki_odkrycia_brzegowe"
"$BUD/testy_czynniki_odkrycia_brzegowe"

echo "testy (test/testy_slot_dzienny.cpp: slot dzienny S-K-33…S-K-39, obrazek dnia, S-K-01, S-K-02, S-K-13)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_slot_dzienny.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_slot_dzienny"
"$BUD/testy_slot_dzienny"
echo "scenariusze slotu dziennego z dyrygentem na atrapach (test/testy_slot_dzienny_dyrygent.cpp: prognoza przy starcie slotu, S-K-38, S-K-37, S-K-01)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_slot_dzienny_dyrygent.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_slot_dzienny_dyrygent"
"$BUD/testy_slot_dzienny_dyrygent"
echo "testy brzegowe wygaszacza ekranu (test/testy_wygaszacz_brzegowe.cpp: energia doby, przycisk przed oceną, drzemka, RTC, strona formularza)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_wygaszacz_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_wygaszacz_brzegowe"
"$BUD/testy_wygaszacz_brzegowe"  # blokuje przy błędzie
echo "testy brzegowe wygaszacza, wariant W3 (test/testy_wygaszacz_brzegowe_2.cpp: pełna tabela, stara baza, przejścia okna, bateria, informacja, piksele stopki)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_wygaszacz_brzegowe_2.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_wygaszacz_brzegowe_2"
"$BUD/testy_wygaszacz_brzegowe_2"
echo "testy przypadków brzegowych: slot dzienny i obrazki dnia (test/testy_slot_dzienny_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_slot_dzienny_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_slot_dzienny_brzegowe"
"$BUD/testy_slot_dzienny_brzegowe"  # po poprawkach wszystkie zielone, więc blokuje
echo "testy (test/testy_raport_a4.cpp: raport A4, PDF, kod QR, serwer raportu, ściąga, ▶ na ekranie L i w K2)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_raport_a4.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_raport_a4"
RAPORT_DIR="${PODGLAD%/}-raport"
"$BUD/testy_raport_a4" "$RAPORT_DIR"
echo "testy przypadków brzegowych: raport A4, serwer raportu, kod QR (test/testy_raport_a4_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_raport_a4_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_raport_a4_brzegowe"
mkdir -p "${PODGLAD%/}-raport-a4"
"$BUD/testy_raport_a4_brzegowe" "${PODGLAD%/}-raport-a4"  # po poprawkach wszystkie zielone, więc blokuje
echo "składnia gniazda serwera raportu ESP32 (src/serwer_raportu_esp32.cpp) z atrapą WebServer.h"
g++ -std=gnu++17 -fsyntax-only "${OSTRZEZENIA[@]}" -DARDUINO_ATRAPA -I"$FW/test/atrapy/arduino" -I"$FW/test/atrapy" \
  -I"$FW/src" -I"$FW/include" "$FW/src/serwer_raportu_esp32.cpp"
# kod QR z podglądu czytelny dla dekodera (OpenCV), PDF raportu jako obraz (pdftoppm); oba opcjonalne na komputerze
if python3 -c "import cv2" 2>/dev/null; then
  python3 - "$PODGLAD" <<'PY'
import cv2, os, sys
d = cv2.QRCodeDetector()
for n, oczek in (("b13-raport-qr", "http://192.168.1.47/r/f1b384krart14y2p6g97gnhm28/"),
                 ("b13-sciaga", "http://192.168.1.47/r/f1b384krart14y2p6g97gnhm28/edf")):
    p = os.path.join(sys.argv[1], n + ".png")
    if not os.path.exists(p):
        p = os.path.join(sys.argv[1], n + ".pbm")
    v = d.detectAndDecode(cv2.imread(p, 0))[0]
    if v != oczek:
        print("BŁĄD: kod QR %s czyta się jako %r" % (n, v)); sys.exit(1)
print("  kody QR z podglądu: odczytane (OpenCV)")
PY
fi
if command -v pdftoppm >/dev/null; then
  pdftoppm -r 100 -png "$RAPORT_DIR/raport-L01-ASVAuto.pdf" "$RAPORT_DIR/raport-L01-ASVAuto" && echo "  PDF raportu jako PNG: $RAPORT_DIR"
fi

echo "testy (test/testy_stany_nocy_sloty_wieczorne.cpp: stany nocy 5.1.4, sloty wieczór i tydzień, rytm ekranu L według pokazań, linie kolejki)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_stany_nocy_sloty_wieczorne.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_stany_nocy_sloty_wieczorne"
"$BUD/testy_stany_nocy_sloty_wieczorne"

echo "testy przypadków brzegowych: stany nocy, sloty wieczorny i tygodniowy (test/testy_stany_nocy_sloty_wieczorne_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_stany_nocy_sloty_wieczorne_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_stany_nocy_sloty_wieczorne_brzegowe"
"$BUD/testy_stany_nocy_sloty_wieczorne_brzegowe"  # wszystkie zielone, więc blokuje
echo "testy (test/testy_ekrany_co2.cpp: ekrany CO2 per egzemplarz, wybudzenia z Wi-Fi wieczorem w egzemplarzu ze źródłem otoczenia i bez niego, przerywniki S2d G01–G18)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_ekrany_co2.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_ekrany_co2"

echo "testy (test/testy_przerywniki_s2d.cpp: przerywniki S2d z tekstami autora projektu, karta z warunkiem i neutralna, krok zamienny przy braku danych)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_przerywniki_s2d.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_przerywniki_s2d"
"$BUD/testy_przerywniki_s2d" "$REPO"

echo "testy przypadków brzegowych: przerywniki S2d (test/testy_przerywniki_s2d_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_przerywniki_s2d_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_przerywniki_s2d_brzegowe"
"$BUD/testy_przerywniki_s2d_brzegowe" # wszystkie zielone (oczekujące nie liczą się jako błąd), więc blokuje
"$BUD/testy_ekrany_co2"
echo "testy przypadków brzegowych: ekrany CO2 i przerywniki (test/testy_ekrany_co2_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_ekrany_co2_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_ekrany_co2_brzegowe"
"$BUD/testy_ekrany_co2_brzegowe"  # po poprawkach wszystkie zielone, więc blokuje
echo "testy (test/testy_wieczor_bez_wifi_obrazki.cpp: wieczorna ocena S-K bez Wi-Fi na egzemplarzu bez CO2, obrazki dnia komendą W)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_wieczor_bez_wifi_obrazki.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_wieczor_bez_wifi_obrazki"
"$BUD/testy_wieczor_bez_wifi_obrazki"
echo "testy przypadków brzegowych: ocena wieczorna bez Wi-Fi, obrazki dnia (test/testy_wieczor_bez_wifi_obrazki_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_wieczor_bez_wifi_obrazki_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_wieczor_bez_wifi_obrazki_brzegowe"
"$BUD/testy_wieczor_bez_wifi_obrazki_brzegowe"  # po poprawkach blokuje
echo "testy przypadków brzegowych: konfiguracja w zrzucie Z (test/testy_zrzut_konfiguracja_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_zrzut_konfiguracja_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_zrzut_konfiguracja_brzegowe"
"$BUD/testy_zrzut_konfiguracja_brzegowe"  # blokuje
echo "testy (test/testy_sprzet_infekcja_sk.cpp: SPRZĘT, INFEKCJA, pozostałe ekrany S-K, limity 7.4)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_sprzet_infekcja_sk.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_sprzet_infekcja_sk"
echo "testy przypadków brzegowych: SPRZĘT, INFEKCJA, pozostałe S-K (test/testy_sprzet_infekcja_sk_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_sprzet_infekcja_sk_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_sprzet_infekcja_sk_brzegowe"
"$BUD/testy_sprzet_infekcja_sk_brzegowe"  # po poprawkach wszystkie zielone, więc blokuje
"$BUD/testy_sprzet_infekcja_sk"
echo "testy (test/testy_alerty_odchylen.cpp: karty S-K-40…S-K-47, stany w kierunkach kart, linia „dobra zmiana”, ADR-0039)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_alerty_odchylen.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_alerty_odchylen"
"$BUD/testy_alerty_odchylen"
echo "testy (test/testy_norma_tymczasowa.cpp: norma tymczasowa „sprzed zmiany”, K1, K2, reguły bez zmian, ADR-0048)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_norma_tymczasowa.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_norma_tymczasowa"
"$BUD/testy_norma_tymczasowa"  # blokuje
# R-NOR-29 (ADR-0048, granica bezpieczeństwa): normę tymczasową czytają tylko liczenie normy (noc.*) i kokpit
# (kokpit.*, ekrany.*); każde inne użycie w src/ (reguły, stany, karty, raport, dyrygent) przerywa testy
if grep -lE 'sprzedZmiany|PasmoSprzedZmiany|NormaSprzedZmiany' "$FW"/src/*.cpp "$FW"/src/*.h \
     | grep -vE '/src/(noc|kokpit|ekrany)\.(cpp|h)$'; then
  echo "BŁĄD: norma tymczasowa poza src/noc.*, src/kokpit.*, src/ekrany.* (R-NOR-29)"; exit 1
fi
echo "  norma tymczasowa tylko w noc.*, kokpit.*, ekrany.*: ok"
# norma kokpitu i kontrfakty R-KOK-37 w Stany czytają tylko stany.* (liczy) i kokpit.* (K1, K2)
if grep -lE 'normaKokpitu|kokpitAhiZNiepewnymi|kokpitNoceNiepewne' "$FW"/src/*.cpp "$FW"/src/*.h \
     | grep -vE '/src/(stany|kokpit)\.(cpp|h)$'; then
  echo "BŁĄD: norma kokpitu poza src/stany.*, src/kokpit.* (R-NOR-29)"; exit 1
fi
echo "  norma kokpitu tylko w stany.*, kokpit.*: ok"
echo "testy (test/testy_norma_tymczasowa_naiwna.cpp: naiwna R-NOR-27 na 30000 losowych historiach, norma właściwa bez zmian)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" -I"$FW/test" "$FW/test/testy_norma_tymczasowa_naiwna.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_norma_tymczasowa_naiwna"
"$BUD/testy_norma_tymczasowa_naiwna" 30000  # blokuje
echo "testy (test/testy_k1_ahi_niepewne_scenariusze.cpp, test/testy_k1_ahi_niepewne_naiwna.cpp: tryb nocy, wymiana aparatu, naiwna R-KOK-37)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_k1_ahi_niepewne_scenariusze.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_k1_ahi_niepewne_scenariusze"
"$BUD/testy_k1_ahi_niepewne_scenariusze"  # blokuje
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_k1_ahi_niepewne_naiwna.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_k1_ahi_niepewne_naiwna"
"$BUD/testy_k1_ahi_niepewne_naiwna" 3000 0  # wejścia spójne, blokuje
"$BUD/testy_k1_ahi_niepewne_naiwna" 3000 1  # wejścia jak w dyrygencie (okresy infekcji), blokuje
echo "test różnicowy normy tymczasowej (tools/prototyp/norma_tymczasowa_roznicowy.sh: symulator 10 dni, różnice tylko K1, K2, LICZBY)"
bash "$REPO/tools/prototyp/norma_tymczasowa_roznicowy.sh" "$BUD" "$BUD/norma-tymczasowa-roznicowy" 10  # blokuje
echo "testy (test/testy_k1_ahi_niepewne.cpp: K1 AHI „niepewne” i „duży wyciek” zamiast „uczę się”, R-KOK-37)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_k1_ahi_niepewne.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_k1_ahi_niepewne"
"$BUD/testy_k1_ahi_niepewne"  # blokuje

echo "testy (test/testy_ekrany_zdarzen_e.cpp: pełne ekrany zdarzeń E (S3), wybór 7.2, limit R-KON-14, „wiem” 7 dni, linia „Wciąż”, R-EKR-48, koniec slotu dziennego wobec okna wieczornego)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_ekrany_zdarzen_e.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_ekrany_zdarzen_e"
"$BUD/testy_ekrany_zdarzen_e"

echo "testy (test/testy_trend_ps_podpisy_osi.cpp: decyzje z 04.11.2026: podpisy osi S3, ocena częściowa, trend PS w K1, tytuły L-07 i O-09)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_trend_ps_podpisy_osi.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_trend_ps_podpisy_osi"

echo "testy przypadków brzegowych: podpisy osi S3, ocena częściowa, trend PS, tytuły L-07 i O-09 (test/testy_trend_ps_podpisy_osi_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_trend_ps_podpisy_osi_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_trend_ps_podpisy_osi_brzegowe"
"$BUD/testy_trend_ps_podpisy_osi_brzegowe" "$REPO"  # wszystkie zielone, więc blokuje
"$BUD/testy_trend_ps_podpisy_osi"

echo "testy (test/testy_profil_publiczny.cpp: profil publiczny w /konfiguracja.txt, teksty 5.4.14 na ekranach dnia, R-EKR-51 z fail-path)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_profil_publiczny.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_profil_publiczny"
"$BUD/testy_profil_publiczny"

echo "testy (test/testy_kolejnosc_przyciskow_stopki.cpp: kolejność podpisów w stopkach ◀ ▶ ↻ jak przyciski na obudowie)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_kolejnosc_przyciskow_stopki.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kolejnosc_przyciskow_stopki"
"$BUD/testy_kolejnosc_przyciskow_stopki"

echo "testy przypadków brzegowych: profil publiczny (test/testy_profil_publiczny_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_profil_publiczny_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_profil_publiczny_brzegowe"
"$BUD/testy_profil_publiczny_brzegowe"  # po poprawkach wszystkie zielone, więc blokuje

echo "testy przypadków brzegowych: ekrany zdarzeń E, linia Wciąż (test/testy_ekrany_zdarzen_e_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_ekrany_zdarzen_e_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_ekrany_zdarzen_e_brzegowe"
mkdir -p "${PODGLAD%/}-ekrany-zdarzen-e"
"$BUD/testy_ekrany_zdarzen_e_brzegowe" "${PODGLAD%/}-ekrany-zdarzen-e"  # wszystkie zielone, więc blokuje

echo "testy przypadków brzegowych: aparat AirSense AutoSet, raport z Załącznika A (test/testy_aparat_autoset_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_aparat_autoset_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_aparat_autoset_brzegowe"
"$BUD/testy_aparat_autoset_brzegowe" "$REPO"  # po poprawkach wszystkie zielone, więc blokuje

echo "testy (test/testy_zegar_ntp.cpp: zegar nowego panelu, NTP 3 próby, czas z AirBridge, R-AW-18; T-TEST-327)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_zegar_ntp.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_zegar_ntp"
"$BUD/testy_zegar_ntp"

echo "testy konfiguracji z telefonu (test/testy_konfiguracja_z_telefonu.cpp: sekrety z NVS, secrets.h jako wartość początkowa, ochrona haseł)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_konfiguracja_z_telefonu.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_konfiguracja_z_telefonu"
"$BUD/testy_konfiguracja_z_telefonu"

echo "testy przypadków brzegowych: konfiguracja z telefonu (test/testy_konfiguracja_z_telefonu_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_konfiguracja_z_telefonu_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_konfiguracja_z_telefonu_brzegowe"

echo "testy przypadków brzegowych: sieć panelu U-02 (test/testy_siec_panelu_u02_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_siec_panelu_u02_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_siec_panelu_u02_brzegowe"
"$BUD/testy_siec_panelu_u02_brzegowe"
"$BUD/testy_konfiguracja_z_telefonu_brzegowe"

echo "testy (test/testy_parsowanie_liczb.cpp: liczbaCalk/liczbaDluga/liczbaRzecz równoważne atoi/atol/atof)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_parsowanie_liczb.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_parsowanie_liczb"
echo "testy (test/testy_sprzet_maska_filtr.cpp: SPRZĘT tylko maska i filtr, migracja dat, filtr w tygodniach, formularz bez rury i komory)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_sprzet_maska_filtr.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_sprzet_maska_filtr"
"$BUD/testy_sprzet_maska_filtr"
"$BUD/testy_parsowanie_liczb"
echo "testy (test/testy_nawigacja.cpp: nowa nawigacja, ciąg stron n / 3 (ADR-0041), ↻ ekran dnia, przytrzymanie = zapis, „Omówione” tylko ▶)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_nawigacja.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_nawigacja"
"$BUD/testy_nawigacja"
echo "testy przypadków brzegowych: mapa nawigacji 5.6.2 tabelarycznie (test/testy_nawigacja_mapa_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_nawigacja_mapa_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_nawigacja_mapa_brzegowe"
"$BUD/testy_nawigacja_mapa_brzegowe"
echo "testy strony kokpitu „Sypialnia · 30 nocy” (test/testy_sypialnia_30.cpp: R-KOK-29…R-KOK-33, ciąg n / 4, ADR-0044)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_sypialnia_30.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_sypialnia_30"
echo "testy kolejności i widoczności ekranów z telefonu (test/testy_kolejnosc_ekranow.cpp: R-KOK-34…R-KOK-36, R-KON-70…R-KON-72, R-KONF-60, ADR-0045)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_kolejnosc_ekranow.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kolejnosc_ekranow"
"$BUD/testy_kolejnosc_ekranow"
echo "testy szybkości ścieżki „przycisk → rysowanie” (test/testy_szybciej_przycisk.cpp: T-WYM-99, liczniki i równoważność klatek)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_szybciej_przycisk.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_szybciej_przycisk"
"$BUD/testy_szybciej_przycisk"
echo "testy brzegowe szybkości (test/testy_szybciej_przycisk_brzegowe.cpp: T-WYM-99, mutacje pól kluczy, ocena z PamiecOceny i bez, wybudzenia z wejściami zmiennymi w czuwaniu)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_szybciej_przycisk_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_szybciej_przycisk_brzegowe"
"$BUD/testy_szybciej_przycisk_brzegowe"  # blokuje przy błędzie (w tym strażnik rozmiaru Noc)
echo "testy czekania na kablu zamiast głębokiego snu (test/testy_kabel_bez_snu.cpp: ADR-0047, T-WYM-10…T-WYM-14, T-WYM-16, T-WYM-17, doba i tydzień na jednym żywym stanie wobec głębokiego snu)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_kabel_bez_snu.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kabel_bez_snu"
"$BUD/testy_kabel_bez_snu"
echo "testy brzegowe czekania na kablu (test/testy_kabel_bez_snu_brzegowe.cpp: ADR-0047, przejściowy błąd odczytu a PlikiZPamiecia, wyłącznik w czuwaniu, losowe sekwencje głęboki sen ≡ czekanie)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_kabel_bez_snu_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kabel_bez_snu_brzegowe"
"$BUD/testy_kabel_bez_snu_brzegowe"  # blokuje przy błędzie (PlikiZPamiecia nie pamięta nieudanego odczytu ani braku pliku)
echo "testy brzegowe czekania na kablu, część 2 (test/testy_kabel_bez_snu_brzegowe_2.cpp: ADR-0047 zmiana 2026-11-17, lista katalogu z ukośnikiem a nowy rok, częściowe listuj(), usterki nośnika na obu ścieżkach, naciśnięcie już puszczone)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_kabel_bez_snu_brzegowe_2.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kabel_bez_snu_brzegowe_2"
"$BUD/testy_kabel_bez_snu_brzegowe_2"  # blokuje przy błędzie (PlikiZPamiecia bez pamięci listuj() i pierwszyWiersz(), próg przytrzymania jak po głębokim śnie)
echo "testy brzegowe limitu ładowania z czekaniem na kablu (test/testy_kabel_ladowanie_brzegowe.cpp: ADR-0046 × ADR-0047, losowe sekwencje na dwóch ścieżkach z resetem układu, błędami I2C i NVS; przywrócenie po wyłączeniu w czekaniu; formularz zachowuje kabel_bez_snu; nieczytelna konfiguracja: limit bez zmian, głęboki sen; KL_ZIARNA=n)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_kabel_ladowanie_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kabel_ladowanie_brzegowe"
"$BUD/testy_kabel_ladowanie_brzegowe"  # blokuje przy błędzie, także nieczytelna konfiguracja: ani zdjęcie limitu, ani czekanie
echo "testy kolejności ekranów z dyrygentem (test/testy_kolejnosc_portret.cpp: Portret ▶ na pierwszą stronę ciągu w każdej kolejności; test/testy_kolejnosc_slot_dyrygent.cpp: dzien.ukryte w slocie dziennym)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_kolejnosc_portret.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kolejnosc_portret"
"$BUD/testy_kolejnosc_portret"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_kolejnosc_slot_dyrygent.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_kolejnosc_slot_dyrygent"
"$BUD/testy_kolejnosc_slot_dyrygent"
"$BUD/testy_sypialnia_30"

echo "testy rodzaju odświeżenia e-ink (test/testy_odswiezanie_czesciowe.cpp: R-NF-25, ekran dnia pełne, widoki z przycisku częściowe, co N-te pełne, eink.pelne_co)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_odswiezanie_czesciowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_odswiezanie_czesciowe"
"$BUD/testy_odswiezanie_czesciowe"

echo "testy brzegowe rodzaju odświeżenia e-ink (test/testy_odswiezanie_czesciowe_brzegowe.cpp: R-NF-25 wobec N = 0, licznik w RTC, kolejka, parser)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_odswiezanie_czesciowe_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_odswiezanie_czesciowe_brzegowe"
"$BUD/testy_odswiezanie_czesciowe_brzegowe"

echo "testy (test/testy_formularz_u03.cpp: formularz U-03 uzupełniony: miasto, CO2, humor, dźwięk, cisza, podpowiedź, czynniki)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_formularz_u03.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_formularz_u03"
"$BUD/testy_formularz_u03"

echo "testy alarmu baterii E-05 (test/testy_alarm_baterii.cpp: M-25 z napięcia, wygładzenie, linia ≤ 15%, S5 ≤ 5%, wyjście > 20%, R-PIE-07)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_alarm_baterii.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_alarm_baterii"
"$BUD/testy_alarm_baterii"

echo "testy przypadków brzegowych na styku modułów (test/testy_styki_modulow_brzegowe.cpp, tools/test_styki_modulow_brzegowe.py)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA[@]}" "$FW/test/testy_styki_modulow_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_styki_modulow_brzegowe"
echo "  (linie OCZEKUJE to znane przypadki czekające na decyzję, opisane w komentarzu testu i w firmware/prototyp/README.md;"
echo "  wypisują się dla przejrzystości, nie są błędem i nie blokują testów)"
"$BUD/testy_styki_modulow_brzegowe"  # blokuje (oczekujące nie)
python3 "$REPO/tools/test_styki_modulow_brzegowe.py"

echo "testy (test/testy_aktualizacja.cpp: aktualizacja z wydań GitHub, rdzeń: wersje, opis wydania, SHA-256, decyzje, U-07, ikona, formularz)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_aktualizacja.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_aktualizacja"
"$BUD/testy_aktualizacja"
echo "scenariusze aktualizacji z dyrygentem na atrapach (test/testy_aktualizacja_dyrygent.cpp: R-NF-32…36, R-KONF-43…46, R-EKR-61)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_aktualizacja_dyrygent.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_aktualizacja_dyrygent"
echo "informacja po aktualizacji samodzielnej (test/testy_karta_po_aktualizacji.cpp: R-KONF-61)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_karta_po_aktualizacji.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_karta_po_aktualizacji"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_karta_po_aktualizacji_portret.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_karta_po_aktualizacji_portret"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_karta_po_aktualizacji_pytanie.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_karta_po_aktualizacji_pytanie"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_karta_po_aktualizacji_slot.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_karta_po_aktualizacji_slot"
# testy brzegowe informacji po aktualizacji, od 15.11.2026 blokujące
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_karta_po_aktualizacji_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_karta_po_aktualizacji_brzegowe"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_karta_slot_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_karta_slot_brzegowe"
"$BUD/testy_karta_po_aktualizacji_portret"
"$BUD/testy_karta_po_aktualizacji"
"$BUD/testy_karta_po_aktualizacji_pytanie"
"$BUD/testy_karta_po_aktualizacji_slot"
"$BUD/testy_karta_po_aktualizacji_brzegowe"
"$BUD/testy_karta_slot_brzegowe"
"$BUD/testy_aktualizacja_dyrygent"
echo "testy przypadków brzegowych aktualizacji (test/testy_aktualizacja_brzegowe.cpp)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_aktualizacja_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_aktualizacja_brzegowe"
"$BUD/testy_aktualizacja_brzegowe"
echo "testy U-01 z aktualizacją (test/testy_scalenie_u01_stopka.cpp: kafelek ↻ z dyrygenta, odesłanie do istniejącego elementu)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_scalenie_u01_stopka.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_scalenie_u01_stopka"
"$BUD/testy_scalenie_u01_stopka"
echo "testy brzegowe ekranów serwisowych (test/testy_ekrany_serwisowe_brzegowe.cpp: U-05 werdykt i wiersze, U-07, „Co nowego”, U-01 kafelek; 0 usterek)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" -I"$FW/test" "$FW/test/testy_ekrany_serwisowe_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_ekrany_serwisowe_brzegowe"
(cd "$FW" && "$BUD/testy_ekrany_serwisowe_brzegowe")  # czyta src/dyrygent.cpp (przypadek [i]); blokuje przy usterce
echo "testy brzegowe znaków belki (test/testy_belka_znaki_brzegowe.cpp: dane nieaktualne, klepsydra, znak wersji, U-05 pobieram; 0 błędów)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_belka_znaki_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_belka_znaki_brzegowe"
"$BUD/testy_belka_znaki_brzegowe"  # blokuje przy błędzie
echo "testy brzegowe belki bez napisu kabel (test/testy_belka_bez_kabla_brzegowe.cpp: ikona na kablu na każdej belce i ekranie, liczby ze spec 5.4.17, kabel ↔ bateria w dyrygencie)"
g++ "${FLAGI[@]}" "${OSTRZEZENIA_TESTOW[@]}" "$FW/test/testy_belka_bez_kabla_brzegowe.cpp" "${OBIEKTY[@]}" -o "$BUD/testy_belka_bez_kabla_brzegowe"
"$BUD/testy_belka_bez_kabla_brzegowe"  # blokuje przy błędzie
