// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
#include "Arduino.h"
struct TwoWire {
  bool begin(int, int) { return true; }
  void beginTransmission(int) {}
  size_t write(uint8_t) { return 1; }
  uint8_t endTransmission() { return 0; }
  uint8_t requestFrom(int, int) { return 0; }
  int read() { return 0; }
};
extern TwoWire Wire;
