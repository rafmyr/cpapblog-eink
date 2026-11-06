// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
#include "Arduino.h"
enum { HSPI = 2, MSBFIRST = 1, SPI_MODE0 = 0 };
struct SPISettings {
  SPISettings(uint32_t, uint8_t, uint8_t) {}
};
struct SPIClass {
  explicit SPIClass(int) {}
  void begin(int, int, int, int) {}
};
