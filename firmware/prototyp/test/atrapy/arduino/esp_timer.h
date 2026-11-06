// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Sygnatury jak w ESP-IDF 5.5 (esp_timer.h).
#pragma once
#include <stdint.h>
#include "esp_system.h"
typedef struct esp_timer* esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void* arg);
typedef struct {
  esp_timer_cb_t callback;
  void* arg;
  int dispatch_method;
  const char* name;
  bool skip_unhandled_events;
} esp_timer_create_args_t;
int64_t esp_timer_get_time(void);
esp_err_t esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* out);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t period_us);
