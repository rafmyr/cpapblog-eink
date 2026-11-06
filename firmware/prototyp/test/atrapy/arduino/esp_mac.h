// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
#include <stdint.h>
typedef enum { ESP_MAC_WIFI_STA = 0, ESP_MAC_WIFI_SOFTAP = 1 } esp_mac_type_t;
inline int esp_read_mac(uint8_t* mac, esp_mac_type_t) { (void)mac; return 0; }
