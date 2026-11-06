#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Środowisko testów na macOS: katalog z dowiązaniami g++, gcc, grep, xargs i nproc do narzędzi GNU z Homebrew
# (tools/prototyp/testy.sh potrzebuje GNU g++, GNU grep z -P, GNU xargs i nproc). Na standardowe wyjście wypisuje
# jedną linię `export PATH=...`, więc wystarczy:
#   brew install gcc grep findutils coreutils
#   eval "$(bash tools/prototyp/srodowisko-macos.sh)"
# Użycie: tools/prototyp/srodowisko-macos.sh [katalog, domyślnie ~/.cache/cpap-eink/bin]
# Wersja GCC: najnowsza g++-N z Homebrew, a GCC_WERSJA=12 wybiera konkretną (np. jak lokalnie, gdy CI ma inną).
set -euo pipefail
KAT="${1:-$HOME/.cache/cpap-eink/bin}"

if ! command -v brew >/dev/null 2>&1; then
  echo "BŁĄD: brak Homebrew (https://brew.sh); potem: brew install gcc grep findutils coreutils" >&2
  exit 1
fi
BIN="$(brew --prefix)/bin"

if [ -n "${GCC_WERSJA:-}" ]; then
  GPP="$BIN/g++-$GCC_WERSJA"
else
  GPP="$(ls "$BIN"/g++-[0-9]* 2>/dev/null | sed 's/.*g++-//' | sort -n | tail -1)"
  GPP="${GPP:+$BIN/g++-$GPP}"
fi
if [ -z "$GPP" ] || [ ! -x "$GPP" ]; then
  echo "BŁĄD: brak GNU g++ z Homebrew (g++-N w $BIN): brew install gcc" >&2
  exit 1
fi
GCC="${GPP/g++-/gcc-}"

braki=()
for p in "$GCC" "$BIN/ggrep" "$BIN/gxargs" "$BIN/gnproc"; do
  [ -x "$p" ] || braki+=("$p")
done
if [ "${#braki[@]}" -gt 0 ]; then
  echo "BŁĄD: brak: ${braki[*]}" >&2
  echo "  brew install gcc grep findutils coreutils" >&2
  exit 1
fi

mkdir -p "$KAT"
ln -sf "$GPP" "$KAT/g++"
ln -sf "$GCC" "$KAT/gcc"
ln -sf "$BIN/ggrep" "$KAT/grep"
ln -sf "$BIN/gxargs" "$KAT/xargs"
ln -sf "$BIN/gnproc" "$KAT/nproc"
echo "środowisko testów: $KAT (g++ -> $(basename "$GPP"))" >&2
printf 'export PATH="%s:$PATH"\n' "$KAT"
