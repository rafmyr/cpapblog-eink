// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h: odczyt NVS z ESP-IDF (NvsEsp32::liczbaUStan, ADR-0046).
#pragma once
#include <cstddef>
#include <cstdint>
#include "esp_system.h"
typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;
#ifndef ESP_ERR_NVS_NOT_FOUND
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#endif
inline esp_err_t nvs_open(const char*, nvs_open_mode_t, nvs_handle_t*) { return ESP_ERR_NVS_NOT_FOUND; }
inline esp_err_t nvs_get_u32(nvs_handle_t, const char*, uint32_t*) { return ESP_ERR_NVS_NOT_FOUND; }
inline void nvs_close(nvs_handle_t) {}
// zajętość partycji NVS do wiersza `nvs start` w logu startu (miejsce-i-nvs)
typedef struct {
  size_t used_entries;
  size_t free_entries;
  size_t available_entries;
  size_t total_entries;
  size_t namespace_count;
} nvs_stats_t;
inline esp_err_t nvs_get_stats(const char*, nvs_stats_t* s) {
  *s = nvs_stats_t{};
  return ESP_OK;
}
