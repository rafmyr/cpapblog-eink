// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
#include <stdint.h>
typedef enum { ESP_SLEEP_WAKEUP_UNDEFINED = 0, ESP_SLEEP_WAKEUP_EXT1 = 3, ESP_SLEEP_WAKEUP_TIMER = 4 } esp_sleep_wakeup_cause_t;
enum { ESP_EXT1_WAKEUP_ANY_LOW = 2 };
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause();
uint64_t esp_sleep_get_ext1_wakeup_status();
int esp_sleep_enable_timer_wakeup(uint64_t);
int esp_sleep_enable_ext1_wakeup(uint64_t, int);
void esp_deep_sleep_start();
