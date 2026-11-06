// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
#include <stdint.h>
typedef int esp_err_t;
#ifndef ESP_OK
#define ESP_OK 0
#endif
typedef enum { WIFI_IF_STA = 0, WIFI_IF_AP = 1 } wifi_interface_t;
typedef enum { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 3 } wifi_auth_mode_t;
typedef enum { WIFI_CIPHER_TYPE_CCMP = 4 } wifi_cipher_type_t;
typedef struct {
  uint8_t ssid[32];
  uint8_t password[64];
  uint8_t ssid_len, channel;
  wifi_auth_mode_t authmode;
  uint8_t ssid_hidden, max_connection;
  uint16_t beacon_interval;
  wifi_cipher_type_t pairwise_cipher;
} wifi_ap_config_t;
typedef union { wifi_ap_config_t ap; } wifi_config_t;
inline esp_err_t esp_wifi_stop(void) { return ESP_OK; }
inline esp_err_t esp_wifi_start(void) { return ESP_OK; }
inline esp_err_t esp_wifi_set_config(wifi_interface_t, wifi_config_t*) { return ESP_OK; }
#define WIFI_AP_DEFAULT_AUTH_MODE WIFI_AUTH_WPA2_PSK
#define WIFI_AP_DEFAULT_CIPHER WIFI_CIPHER_TYPE_CCMP
