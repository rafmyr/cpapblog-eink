#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Bramki jakości rdzenia lokalnie, tak samo jak w CI (.github/workflows/testy.yml).
# Uruchom przed każdym pushem. Wymaga: g++ (9+), gcov, clang-tidy (18), python3.
#
# Użycie: tools/jakosc/sprawdz.sh [krok …] [--zapisz]
#   testy       tools/prototyp/testy.sh (flagi -Werror i dodatkowe ostrzeżenia, podgląd ekranów)
#   standard    bez wyjątków i RTTI w src/ (grep: throw, try, catch, dynamic_cast, typeid)
#   sanitizery  testy rdzenia z ASan i UBSan; każde zgłoszenie przerywa
#   tidy        clang-tidy z firmware/prototyp/.clang-tidy; zero nowych ostrzeżeń wobec tools/jakosc/clang-tidy-baza.txt
#   pokrycie    pokrycie linii rdzenia (gcov); nie niżej niż tools/jakosc/pokrycie-baza.txt
#   bazy        bazy clang-tidy i pokrycia nie gorsze niż na gałęzi bazowej (merge-base; BAZA_GALAZ, domyślnie
#               origin/main) albo linia „zmiana bazy:” w opisie commitu lub w CHANGELOG dopisanym w gałęzi
#   spojnosc    skrypty spójności spec i pytest narzędzi
#   (bez kroku) wszystko po kolei
#   --zapisz    przy tidy i pokrycie: zapisz obecny wynik jako nową bazę (tylko świadomie, z powodem w PR)
set -euo pipefail
if (( BASH_VERSINFO[0] < 4 )); then
  echo "sprawdz.sh wymaga bash 4 albo nowszego (mapfile); na macOS: brew install bash, potem \"\$(brew --prefix)/bin/bash\" tools/jakosc/sprawdz.sh" >&2
  exit 2
fi
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
FW="$REPO/firmware/prototyp"
source "$REPO/tools/prototyp/zrodla.sh"
JOBY="$(nproc 2>/dev/null || echo 2)"
BUD="$FW/.pio/jakosc"
mkdir -p "$BUD"

ZAPISZ=""
KROKI=()
for a in "$@"; do
  case "$a" in
    --zapisz) ZAPISZ="--zapisz" ;;
    testy|standard|sanitizery|tidy|pokrycie|bazy|spojnosc) KROKI+=("$a") ;;
    *) echo "nieznany krok: $a"; exit 2 ;;
  esac
done
[ ${#KROKI[@]} -eq 0 ] && KROKI=(testy standard sanitizery tidy pokrycie bazy spojnosc)

# kompiluje rdzeń plik po pliku (równolegle) do katalogu $1 z flagami z dalszych argumentów
kompiluj_rdzen() {
  local kat="$1"; shift
  mkdir -p "$kat"; rm -f "$kat"/*.o "$kat"/*.gcda "$kat"/*.gcno
  printf '%s\n' "${ZRODLA[@]}" | xargs -P "$JOBY" -I{} sh -c 'g++ "$@" -c "{}" -o "'"$kat"'/$(basename "{}" .cpp).o"' _ "$@"
}
obiekty() { for z in "${ZRODLA[@]}"; do printf '%s\n' "$1/$(basename "$z" .cpp).o"; done; }

krok_testy() {
  echo "== testy (tools/prototyp/testy.sh)"
  bash "$REPO/tools/prototyp/testy.sh" "$BUD/podglad" | grep -v '^  /' || return 1
}

krok_standard() {
  echo "== standard: bez wyjątków i RTTI w src/"
  # słowa kluczowe C++ poza komentarzami i tekstami w cudzysłowie (np. „tryb” nie pasuje dzięki \b)
  if grep -nE '^[^/"]*(\b(throw|dynamic_cast|typeid)\b|\btry[[:space:]]*\{|\bcatch[[:space:]]*\()' "$FW"/src/*.cpp "$FW"/src/*.h; then
    echo "  BŁĄD: wyjątek albo RTTI w src/"; return 1
  fi
  echo "  standard: ok"
}

krok_sanitizery() {
  echo "== sanitizery: ASan + UBSan na testach rdzenia"
  local kat="$BUD/asan" fl=(-std=c++17 -O1 -g -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer
                            -fno-sanitize-recover=all "${INCLUDY[@]}")
  kompiluj_rdzen "$kat" "${fl[@]}"
  mapfile -t ob < <(obiekty "$kat")
  local t n
  for t in "${TESTY[@]}"; do  # programy testów rdzenia z zrodla.sh
    n="$(basename "$t" .cpp)"
    g++ "${fl[@]}" "$t" "${ob[@]}" -o "$kat/$n"
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
      "$kat/$n" > "$kat/wynik-$n.txt" 2>&1 || { tail -40 "$kat/wynik-$n.txt"; echo "  BŁĄD: sanitizery albo $n"; return 1; }
    if grep -qE 'runtime error|AddressSanitizer|LeakSanitizer' "$kat/wynik-$n.txt"; then
      grep -E 'runtime error|Sanitizer' "$kat/wynik-$n.txt" | head -20; echo "  BŁĄD: zgłoszenie sanitizera ($n)"; return 1
    fi
    grep -E '^testy' "$kat/wynik-$n.txt" || true
  done
  echo "  sanitizery: 0 zgłoszeń"
}

krok_tidy() {
  echo "== clang-tidy (profil firmware/prototyp/.clang-tidy)"
  local wy="$BUD/tidy.txt" ct="${CLANG_TIDY:-clang-tidy}"
  # brak narzędzia to błąd, nie zielona bramka
  if ! command -v "$ct" > /dev/null 2>&1; then
    echo "  BŁĄD: nie ma clang-tidy ($ct); zainstaluj clang-tidy 18 albo wskaż program zmienną CLANG_TIDY"; return 1
  fi
  # bez main.cpp: to tylko adaptery ESP32 z nagłówkami Arduino; dyrygent jest w ZRODLA
  # clang-tidy zwraca 0 przy samych ostrzeżeniach, więc kod ≠ 0 (także z xargs) to awaria albo błąd kompilacji
  if ! (cd "$FW" && printf '%s\n' "${ZRODLA[@]}" | xargs -P "$JOBY" -I{} "$ct" --quiet {} -- -std=c++17 "${INCLUDY[@]}") \
       > "$wy" 2> "$BUD/tidy-stderr.txt"; then
    grep -hE "error:|Error|No such file" "$wy" "$BUD/tidy-stderr.txt" | head; echo "  BŁĄD: clang-tidy padł na którymś pliku"; return 1
  fi
  if grep -q "error:" "$wy"; then grep "error:" "$wy" | head; echo "  BŁĄD: clang-tidy nie skompilował pliku"; return 1; fi
  python3 "$REPO/tools/jakosc/porownaj_tidy.py" "$wy" $ZAPISZ
}

krok_pokrycie() {
  echo "== pokrycie linii rdzenia (gcov)"
  local kat="$BUD/pokrycie" fl=(-std=c++17 -O0 --coverage "${INCLUDY[@]}")
  kompiluj_rdzen "$kat" "${fl[@]}"
  rm -f "$kat"/testy*.gcda "$kat"/testy*.gcno
  mapfile -t ob < <(obiekty "$kat")
  local t n gcda=()
  for t in "${TESTY[@]}"; do  # liczniki .gcda rdzenia sumują się ze wszystkich programów testów
    n="$(basename "$t" .cpp)"
    g++ "${fl[@]}" "$t" "${ob[@]}" -o "$kat/$n"
    (cd "$kat" && "./$n" > "wynik-$n.txt") || { tail -20 "$kat/wynik-$n.txt"; echo "  BŁĄD: $n z --coverage"; return 1; }
    gcda+=("$kat/$n-$n.gcda")
  done
  # także jednostki testów: linie z nagłówków src/*.h wykonane tylko w kodzie testów
  (cd "$kat" && gcov --json-format --stdout -o "$kat" "${ob[@]}" "${gcda[@]}" > "$kat/gcov.json" 2> /dev/null)
  python3 "$REPO/tools/jakosc/pokrycie.py" "$kat/gcov.json" $ZAPISZ
}

krok_bazy() {
  echo "== bazy bramek wobec ${BAZA_GALAZ:-origin/main} (merge-base)"
  python3 "$REPO/tools/jakosc/porownaj_bazy.py"  # gałąź bazowa ze zmiennej BAZA_GALAZ
}

krok_spojnosc() {
  echo "== spójność spec i testy narzędzi"
  python3 "$REPO/tools/spojnosc.py" --strict > "$BUD/spojnosc.txt" || { tail -20 "$BUD/spojnosc.txt"; return 1; }
  grep -m1 "^Wynik" "$BUD/spojnosc.txt"
  python3 "$REPO/tools/spojnosc_tech.py" > "$BUD/spojnosc_tech.txt" || { grep -E "BŁĄD|Wynik" "$BUD/spojnosc_tech.txt"; return 1; }
  grep -E "^Wynik" "$BUD/spojnosc_tech.txt"
  python3 -m pytest -q "$REPO/tools" | tail -1
}

BLEDY=()
for k in "${KROKI[@]}"; do
  start=$(date +%s)
  if "krok_$k"; then echo "  [$k: ok, $(( $(date +%s) - start )) s]"; else BLEDY+=("$k"); echo "  [$k: BŁĄD]"; fi
done
if [ ${#BLEDY[@]} -gt 0 ]; then echo "BRAMKI NIEZALICZONE: ${BLEDY[*]}"; exit 1; fi
echo "wszystkie bramki zielone: ${KROKI[*]}"
