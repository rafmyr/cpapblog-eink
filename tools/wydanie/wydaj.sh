#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Wydanie firmware jednym poleceniem, na Macu właściciela (ADR-0032, ADR-0038; tools/wydanie/README.md).
#
# Użycie (z dowolnego miejsca w repo):
#   tools/wydanie/wydaj.sh            # następny numer po ostatnim wydaniu (np. 1.0.0-beta.2 → 1.0.0-beta.3)
#   tools/wydanie/wydaj.sh 1.0.1      # numer podany ręcznie
#
# Po kolei: repo na aktualnym `main` bez niezapisanych zmian, CI na tym commicie zielone, numer wersji, pytanie
# „tak”, budowa obrazu bez sekretów i jego kontrola (wydaj.py), publikacja w repozytorium wydań przez `gh`
# i sprawdzenie, że najnowsze wydanie na GitHubie ma obraz i wydanie.json. Każda odmowa kończy bez publikacji.
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

# 2. CI na tym commicie zielone (wszystkie sprawdzenia zakończone sukcesem)
STANY="$(gh api "repos/$REPO_KODU/commits/$LOKALNY/check-runs" --jq '.check_runs[] | "\(.name) \(.status) \(.conclusion)"')"
[ -n "$STANY" ] || blad "CI nie ma jeszcze wyniku dla $KROTKI; spróbuj za kilka minut"
ZLE="$(printf '%s\n' "$STANY" | awk '$2 != "completed" || $3 != "success"')"
if [ -n "$ZLE" ]; then
  printf 'CI dla %s nie jest w pełni zielone:\n%s\n' "$KROTKI" "$ZLE" >&2
  blad "poczekaj na zielone CI (w toku) albo popraw błąd (failure)"
fi

# 3. Numer wersji
if [ $# -ge 1 ]; then
  WERSJA="$1"
else
  WERSJA="$(python3 tools/wydanie/wydaj.py --nastepna --repo "$REPO_WYDAN")" || blad "nie znam następnego numeru; podaj go: tools/wydanie/wydaj.sh 1.0.1"
fi
if gh api "repos/$REPO_WYDAN/releases/tags/v$WERSJA" >/dev/null 2>&1; then
  blad "wydanie v$WERSJA już jest w $REPO_WYDAN"
fi

echo
echo "Wydanie:   $WERSJA"
echo "Z commitu: $KROTKI $(git log -1 --format=%s)"
echo "Do:        https://github.com/$REPO_WYDAN (panele z kluczem zobaczą je przy następnym sprawdzeniu)"
echo
printf 'Wpisz „tak”, żeby zbudować i opublikować: '
read -r ODP
[ "$ODP" = "tak" ] || blad "przerwane, nic nie opublikowano"

# 4. Budowa, kontrola obrazu, publikacja i sprawdzenie na GitHubie (wydaj.py)
python3 tools/wydanie/wydaj.py "$WERSJA" --repo "$REPO_WYDAN" --publikuj \
  --nota "Zbudowane z main @ $KROTKI ($(git log -1 --format=%s)). Obraz bez sekretów."
