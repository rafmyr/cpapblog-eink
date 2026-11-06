#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Wgrywa firmware na panel przez Wi-Fi (ArduinoOTA, środowisko e1001_ota w platformio.ini).
# Użycie:
#   tools/prototyp/wgraj_wifi.sh                 firmware
#   tools/prototyp/wgraj_wifi.sh --fs <katalog>  pamięć plików (uploadfs) z katalogu danych poza repo;
#                                                KASUJE historię, czynniki i dziennik na panelu
# Przedtem: panel na ekranie „Test sprzętu” (K2, przytrzymaj ◀) z linią „Wgrywanie przez Wi-Fi: gotowe”.
# Hasło OTA czyta z Pęku kluczy macOS (usługa cpap-eink-ota, konto panel; tworzy je sekrety.sh).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
CPAP_OTA_PASS=$(security find-generic-password -s cpap-eink-ota -a panel -w 2>/dev/null) || {
  echo "Brak hasła OTA w Pęku kluczy. Uruchom tools/prototyp/sekrety.sh i wgraj firmware raz kablem."
  exit 1
}
export CPAP_OTA_PASS
PIO="$(command -v pio || echo "$HOME/.platformio/penv/bin/pio")"
CEL=upload
if [ "${1:-}" = "--fs" ]; then
  DANE="${2:-}"
  [ -n "$DANE" ] && [ -d "$DANE" ] || { echo "Podaj katalog danych: $0 --fs <katalog poza repo>"; exit 1; }
  DANE="$(cd "$DANE" && pwd)"
  case "$DANE/" in "$REPO"/*) echo "Odmowa: dane nocy nie mogą leżeć w repozytorium (WY12)."; exit 1 ;; esac
  echo "uploadfs nadpisze CAŁĄ pamięć plików panelu (historia, czynniki, dziennik ekranów) zawartością $DANE."
  echo "Najpierw zrzut: tools/prototyp/zrzut_plikow.py (kablem USB), README prototypu, „Pliki w pamięci panelu”."
  read -r -p "Wpisz „tak”, żeby wgrać: " OK
  [ "$OK" = "tak" ] || { echo "Przerwane."; exit 1; }
  export PLATFORMIO_DATA_DIR="$DANE"
  CEL=uploadfs
fi
cd "$REPO/firmware/prototyp"
echo "Wgrywam ($CEL) na cpap-eink.local przez Wi-Fi…"
exec "$PIO" run -e e1001_ota -t "$CEL"
