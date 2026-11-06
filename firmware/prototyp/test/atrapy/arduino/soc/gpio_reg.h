// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Nazwy jak w ESP-IDF 5.5 (soc/gpio_reg.h, soc/soc.h).
#pragma once
#include <stdint.h>
#define GPIO_IN_REG 0x6000403C
#define REG_READ(r) (*(volatile uint32_t*)(uintptr_t)(r))
