#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Wydanie firmware jednym poleceniem, na Macu właściciela (ADR-0032, ADR-0038; tools/wydanie/README.md).
#
# Użycie (z dowolnego miejsca w repo):
#   tools/wydanie/wydaj.sh            # następny numer po ostatnim wydaniu (np. 1.0.0-beta.2 → 1.0.0-beta.3)
#   tools/wydanie/wydaj.sh 1.0.1      # numer podany ręcznie
#   tools/wydanie/wydaj.sh --wczesne 1.1.0-beta.1   # wersja próbna (pre-release, flaga w dowolnym miejscu): tylko panele z kanałem wczesnym
#                                                   # (`aktualizacja_wczesna=tak`, R-KONF-62, od 23.11.2026)
#
# Po kolei: repo na aktualnym `main` bez niezapisanych zmian, CI na tym commicie zielone, numer wersji, linie „Co nowego”
# (od 14.11.2026, opcjonalne), pytanie
# „tak”, budowa obrazu bez sekretów i jego kontrola (wydaj.py), publikacja w repozytorium wydań przez `gh`
# i sprawdzenie, że najnowsze wydanie (przy --wczesne: wydanie z tagu, oznaczone jako pre-release) ma obraz i wydanie.json.
# Każda odmowa kończy bez publikacji.
# Wymaga: PlatformIO (~/.platformio), `gh` zalogowane na konto z prawem zapisu do repozytorium wydań.
# Zgodne z bash 3.2 (macOS).
set -euo pipefail

REPO_KODU="rafmyr/cpapblog-eink"
REPO_WYDAN="rafmyr/cpapblog-eink"   # jak REPO_WYDAN w src/aktualizacja.h i tools/wydanie/wydaj.py
KORZEN="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$KORZEN"
export PATH="$HOME/.platformio/penv/bin:$PATH"
# Budowa poza folderem projektu: iCloud dorabia w .pio kopie „plik 2.cpp” i budowa pada (10.11.2026)
export PLATFORMIO_WORKSPACE_DIR="${PLATFORMIO_WORKSPACE_DIR:-$HOME/.cache/cpap-eink-pio-wydanie}"

blad() { echo "STOP: $*" >&2; exit 1; }

# Argumenty: `--wczesne` (R-KONF-62: wydanie oznaczone jako pre-release) w dowolnym miejscu i najwyżej jeden numer;
# inny przełącznik albo drugi numer = odmowa bez publikacji (flaga po numerze nie może dać zwykłego wydania)
WCZESNE=()
NUMER=""
for ARG in "$@"; do
  case "$ARG" in
    --wczesne) WCZESNE=(--wczesne) ;;
    -*) blad "nieznany przełącznik $ARG (dozwolony tylko --wczesne)" ;;
    *)
      [ -z "$NUMER" ] || blad "za dużo argumentów: $NUMER i $ARG (podaj jeden numer wersji)"
      NUMER="$ARG"
      ;;
  esac
done

command -v gh >/dev/null || blad "brak narzędzia gh (brew install gh, potem gh auth login)"
command -v pio >/dev/null || blad "brak PlatformIO (pio) w ~/.platformio/penv/bin"
gh auth status >/dev/null 2>&1 || blad "gh niezalogowane (gh auth login)"

# 1. Kod: gałąź main, bez zmian, równo z GitHubem
[ "$(git rev-parse --abbrev-ref HEAD)" = "main" ] || blad "repo nie jest na gałęzi main (git checkout main)"
[ -z "$(git status --porcelain)" ] || blad "są niezapisane zmiany w repo (git status); wydanie tylko z czystego main"
git fetch -q origin main
LOKALNY="$(git rev-parse HEAD)"
ZDALNY="$(git rev-parse origin/main)"
if [ "$LOKALNY" != "$ZDALNY" ]; then
  if git merge-base --is-ancestor HEAD origin/main; then
    echo "Pobieram najnowszy main…"
    git merge -q --ff-only origin/main
    LOKALNY="$(git rev-parse HEAD)"
  else
    blad "lokalny main różni się od GitHuba (lokalne commity); wydanie tylko z main z GitHuba"
  fi
fi
KROTKI="$(git rev-parse --short HEAD)"

# 2. CI na tym commicie zielone: sprawdzenie „wynik: pełne CI” z wynikiem success (reguła „zielone CI”,
#    od 23.11.2026), żadne sprawdzenie w toku ani z wynikiem innym niż success albo skipped
#    (zadania galerii są na push do main celowo pomijane). Pola rozdzielone tabulatorem, bo nazwy mają spacje
#    („sanitizery (4)”, „wynik: pełne CI”).
#    Tylko sprawdzenia GitHub Actions (inna aplikacja z tą samą nazwą nie daje sygnału), wszystkie strony wyników.
STANY="$(gh api --paginate "repos/$REPO_KODU/commits/$LOKALNY/check-runs?per_page=100" --jq '.check_runs[] | select(.app.slug == "github-actions") | "\(.name)\t\(.status)\t\(.conclusion)"')"
[ -n "$STANY" ] || blad "CI nie ma jeszcze wyniku dla $KROTKI; spróbuj za kilka minut (jeśli przebieg na main został anulowany przez nowszy push, wynik się nie pojawi: wydaj z nowszego commitu)"
ZLE="$(printf '%s\n' "$STANY" | awk -F '\t' '$2 != "completed" || ($3 != "success" && $3 != "skipped")')"
if [ -n "$ZLE" ]; then
  printf 'CI dla %s nie jest w pełni zielone:\n%s\n' "$KROTKI" "$ZLE" >&2
  blad "poczekaj na zielone CI (w toku) albo popraw błąd (failure)"
fi
PELNE="$(printf '%s\n' "$STANY" | awk -F '\t' '$1 == "wynik: pełne CI" && $2 == "completed" && $3 == "success"')"
[ -n "$PELNE" ] || blad "brak zielonego sprawdzenia „wynik: pełne CI” dla $KROTKI (pełne CI nie przeszło na tym commicie)"

# 3. Numer wersji
if [ -n "$NUMER" ]; then
  WERSJA="$NUMER"
else
  WERSJA="$(python3 tools/wydanie/wydaj.py --nastepna --repo "$REPO_WYDAN")" || blad "nie znam następnego numeru; podaj go: tools/wydanie/wydaj.sh 1.0.1"
fi
if gh api "repos/$REPO_WYDAN/releases/tags/v$WERSJA" >/dev/null 2>&1; then
  blad "wydanie v$WERSJA już jest w $REPO_WYDAN"
fi

# 3a. „Co nowego” na ekranie aktualizacji panelu (R-KONF-56): do 3 linii po najwyżej 40 znaków, o panelu, nie
# o ustawieniach aparatu (ADR-0002); pusta linia kończy, bez linii wydanie bez pola (panel pomija blok)
echo
echo "Co nowego w tej wersji (na ekranie aktualizacji panelu; do 3 linii, po najwyżej 40 znaków; Enter kończy):"
NOWOSCI=()
for i in 1 2 3; do
  printf '  %s. ' "$i"
  read -r LINIA || LINIA=""
  [ -n "$LINIA" ] || break
  NOWOSCI+=(--nowosc "$LINIA")
done
python3 tools/wydanie/wydaj.py --sprawdz-nowosci ${NOWOSCI[@]+"${NOWOSCI[@]}"} || blad "popraw linie „Co nowego” i uruchom ponownie"

echo
echo "Wydanie:   $WERSJA"
echo "Z commitu: $KROTKI $(git log -1 --format=%s)"
if [ "${#WCZESNE[@]}" -gt 0 ]; then
  echo "Rodzaj:    wersja próbna (pre-release)"
  echo "Do:        https://github.com/$REPO_WYDAN"
  echo "UWAGA: zobaczą ją tylko panele z kanałem wczesnym („Dostawaj wersje próbne przed innymi” = tak)."
  echo "       Panel bez kanału wczesnego tego wydania nie zobaczy i zostaje przy ostatniej zwykłej wersji."
else
  echo "Do:        https://github.com/$REPO_WYDAN (panele z kluczem zobaczą je przy następnym sprawdzeniu)"
  case "$WERSJA" in
    *-beta*|*-rc*)
      echo "UWAGA: numer $WERSJA wygląda na wersję próbną, a wydanie będzie ZWYKŁE: dostaną je wszystkie panele."
      echo "       Wersja próbna tylko dla kanału wczesnego: przerwij i uruchom tools/wydanie/wydaj.sh --wczesne $WERSJA"
      ;;
  esac
fi
echo
printf 'Wpisz „tak”, żeby zbudować i opublikować: '
read -r ODP
[ "$ODP" = "tak" ] || blad "przerwane, nic nie opublikowano"

# 4. Budowa, kontrola obrazu, publikacja i sprawdzenie na GitHubie (wydaj.py)
python3 tools/wydanie/wydaj.py "$WERSJA" --repo "$REPO_WYDAN" --publikuj ${WCZESNE[@]+"${WCZESNE[@]}"} \
  ${NOWOSCI[@]+"${NOWOSCI[@]}"} \
  --nota "Zbudowane z main @ $KROTKI ($(git log -1 --format=%s)). Obraz bez sekretów."
