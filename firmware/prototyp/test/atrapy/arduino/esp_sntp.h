// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa nagłówka ESP-IDF esp_sntp.h (components/lwip/include/apps/esp_sntp.h, ESP-IDF 5.5) tylko do
// `g++ -fsyntax-only` na src/adaptery_esp32.cpp (tools/prototyp/testy.sh).
#pragma once
typedef enum {
  SNTP_SYNC_STATUS_RESET,
  SNTP_SYNC_STATUS_COMPLETED,
  SNTP_SYNC_STATUS_IN_PROGRESS,
} sntp_sync_status_t;
sntp_sync_status_t sntp_get_sync_status(void);
void sntp_set_sync_status(sntp_sync_status_t sync_status);
