// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Sygnatury jak w ESP-IDF 5.5 (esp_ota_ops.h).
#pragma once
#include "esp_system.h"
#include "esp_partition.h"
typedef enum {
  ESP_OTA_IMG_NEW, ESP_OTA_IMG_PENDING_VERIFY, ESP_OTA_IMG_VALID, ESP_OTA_IMG_INVALID, ESP_OTA_IMG_ABORTED,
  ESP_OTA_IMG_UNDEFINED,
} esp_ota_img_states_t;
const esp_partition_t* esp_ota_get_running_partition(void);
esp_err_t esp_ota_get_state_partition(const esp_partition_t* partition, esp_ota_img_states_t* ota_state);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void);
