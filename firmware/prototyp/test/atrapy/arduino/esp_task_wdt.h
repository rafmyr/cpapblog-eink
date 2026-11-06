// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Sygnatury jak w ESP-IDF 5.5 (esp_task_wdt.h).
#pragma once
#include <stdint.h>
#include "esp_system.h"
#include "freertos/task.h"
typedef struct {
  uint32_t timeout_ms;
  uint32_t idle_core_mask;
  bool trigger_panic;
} esp_task_wdt_config_t;
esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t* config);
esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t* config);
esp_err_t esp_task_wdt_add(TaskHandle_t task_handle);
esp_err_t esp_task_wdt_reset(void);
#define CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0 1
