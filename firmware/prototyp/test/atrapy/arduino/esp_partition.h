// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Sygnatury jak w ESP-IDF 5.5 (esp_partition.h).
#pragma once
#include <cstddef>
#include <cstdint>
#include "esp_system.h"
typedef enum {
  ESP_PARTITION_TYPE_APP = 0x00,
  ESP_PARTITION_TYPE_DATA = 0x01,
  ESP_PARTITION_TYPE_ANY = 0xff,
} esp_partition_type_t;
typedef enum {
  ESP_PARTITION_SUBTYPE_DATA_SPIFFS = 0x82,
  ESP_PARTITION_SUBTYPE_ANY = 0xff,
} esp_partition_subtype_t;
typedef struct esp_partition_t {
  const void* flash_chip;
  esp_partition_type_t type;
  esp_partition_subtype_t subtype;
  uint32_t address;
  uint32_t size;
  uint32_t erase_size;
  char label[17];
  bool encrypted;
  bool readonly;
} esp_partition_t;
typedef struct esp_partition_iterator_opaque_* esp_partition_iterator_t;
esp_partition_iterator_t esp_partition_find(esp_partition_type_t type, esp_partition_subtype_t subtype,
                                            const char* label);
const esp_partition_t* esp_partition_find_first(esp_partition_type_t type, esp_partition_subtype_t subtype,
                                                const char* label);
const esp_partition_t* esp_partition_get(esp_partition_iterator_t iterator);
esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t iterator);
void esp_partition_iterator_release(esp_partition_iterator_t iterator);
esp_err_t esp_partition_read(const esp_partition_t* partition, size_t src_offset, void* dst, size_t size);
