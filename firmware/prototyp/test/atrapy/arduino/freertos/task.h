// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Sygnatury jak w FreeRTOS z ESP-IDF 5.5.
#pragma once
#include "FreeRTOS.h"
typedef void* TaskHandle_t;
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask);
