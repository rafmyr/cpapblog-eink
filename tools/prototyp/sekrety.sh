#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Tworzy firmware/prototyp/include/secrets.h z Pęku kluczy macOS. Hasła nie trafiają do repo ani do historii powłoki.
# Hasło Wi-Fi: macOS pokaże okno z prośbą o hasło administratora (dostęp do „AirPort network password”).
# Hasło wgrywania przez Wi-Fi (OTA): przy pierwszym uruchomieniu losowe, zapisane w Pęku kluczy
# (usługa cpap-eink-ota, konto panel); potem zawsze to samo, bo panel zna je z firmware.
# AirBridge (od v2.2.0): adres stały z drugiego argumentu albo ze zmiennej AB_IP (np. AB_IP=192.168.1.50 przed
# skryptem); bez adresu w pliku zostaje znacznik WPISZ_ADRES_AIRBRIDGE do ręcznego uzupełnienia (jak hasło Wi-Fi).
# Hasło konta admin z Pęku kluczy: usługa cpap-eink-airbridge (konto admin).
# Dodanie hasła: security add-generic-password -s cpap-eink-airbridge -a admin -l "cpap-eink: AirBridge" -w
# Lokalne źródło danych otoczenia (opcja, ADR-0019): adres z trzeciego argumentu albo ze zmiennej OTO_URL
# (np. OTO_URL=http://192.168.1.60:8080/otoczenie); bez adresu funkcja zostaje wyłączona (bez domyślnego adresu).
# Token z Pęku kluczy: usługa cpap-eink-otoczenie, konto panel; bez niego znacznik WPISZ_TOKEN_OTOCZENIA do ręcznego
# wpisania (gdy źródło nie sprawdza tokenu, usuń linię OTO_TOKEN z pliku).
# Dodanie tokenu: security add-generic-password -s cpap-eink-otoczenie -a panel -l "cpap-eink: źródło otoczenia" -w
set -euo pipefail
cd "$(dirname "$0")/../../firmware/prototyp"
OUT=include/secrets.h
SSID="${1:-$(networksetup -getairportnetwork en0 2>/dev/null | sed -n 's/^Current Wi-Fi Network: //p')}"
[ -n "$SSID" ] || { echo "Nie znam nazwy sieci. Uruchom: AB_IP=<adres AirBridge> $0 \"nazwa-sieci\""; exit 1; }
# Hasło Wi-Fi z iCloud Pęku kluczy bywa niedostępne dla `security`; wtedy wpisujesz je ręcznie w pliku.
WIFI=$(security find-generic-password -D "AirPort network password" -a "$SSID" -w 2>/dev/null) || WIFI="WPISZ_HASLO_WIFI"
AB=$(security find-generic-password -s cpap-eink-airbridge -a admin -w 2>/dev/null) || AB="WPISZ_HASLO_AIRBRIDGE"
OTA=$(security find-generic-password -s cpap-eink-ota -a panel -w 2>/dev/null) || {
  OTA=$(openssl rand -hex 16)
  security add-generic-password -s cpap-eink-ota -a panel -l "cpap-eink: wgrywanie przez Wi-Fi" -w "$OTA"
  echo "Nowe hasło OTA zapisane w Pęku kluczy (cpap-eink-ota / panel)."
}
IP="${2:-${AB_IP:-WPISZ_ADRES_AIRBRIDGE}}"
OTO="${3:-${OTO_URL:-}}"
TOKEN=""
if [ -n "$OTO" ]; then
  TOKEN=$(security find-generic-password -s cpap-eink-otoczenie -a panel -w 2>/dev/null) || TOKEN="WPISZ_TOKEN_OTOCZENIA"
fi
esc() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }
umask 077
rm -f "$OUT"
cat > "$OUT" <<H
#pragma once
#define WIFI_SSID "$(esc "$SSID")"
#define WIFI_PASS "$(esc "$WIFI")"
#define AB_IP "$(esc "$IP")"
#define AB_USER "admin"
#define AB_PASS "$(esc "$AB")"
#define OTA_PASS "$(esc "$OTA")"
H
if [ -n "$OTO" ]; then
  cat >> "$OUT" <<H
#define OTO_URL "$(esc "$OTO")"
#define OTO_TOKEN "$(esc "$TOKEN")"
H
else
  echo "// źródło otoczenia wyłączone; włączenie: OTO_URL=<adres> $0 (README prototypu, „Źródło danych otoczenia”)" >> "$OUT"
fi
BRAKI=""
[ "$WIFI" = "WPISZ_HASLO_WIFI" ] && BRAKI="hasło Wi-Fi (WPISZ_HASLO_WIFI)"
[ "$AB" = "WPISZ_HASLO_AIRBRIDGE" ] && BRAKI="${BRAKI:+$BRAKI i }hasło AirBridge (WPISZ_HASLO_AIRBRIDGE)"
[ "$IP" = "WPISZ_ADRES_AIRBRIDGE" ] && BRAKI="${BRAKI:+$BRAKI i }adres AirBridge (WPISZ_ADRES_AIRBRIDGE)"
[ "$TOKEN" = "WPISZ_TOKEN_OTOCZENIA" ] && BRAKI="${BRAKI:+$BRAKI i }token źródła otoczenia (WPISZ_TOKEN_OTOCZENIA; bez tokenu usuń linię OTO_TOKEN)"
if [ -n "$BRAKI" ]; then
  echo "Brak: $BRAKI. Otwieram $OUT, wpisz wartość w miejsce znacznika i zapisz."
  open -e "$OUT"
fi
echo "Zapisano $OUT (sieć: $SSID, AirBridge: $IP, źródło otoczenia: ${OTO:-wyłączone})"
