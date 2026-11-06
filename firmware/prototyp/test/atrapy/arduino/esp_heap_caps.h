// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Sygnatury jak w ESP-IDF 5.5 (esp_heap_caps.h).
#pragma once
#include <stddef.h>
#include <stdint.h>
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_INTERNAL (1 << 11)
size_t heap_caps_get_minimum_free_size(uint32_t caps);
