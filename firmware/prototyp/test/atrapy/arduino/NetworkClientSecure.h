// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Klient TLS rdzenia Arduino-ESP32 3.x (NetworkClientSecure).
#pragma once
#include "Arduino.h"
struct NetworkClient {
  virtual ~NetworkClient() {}
  int available() { return 0; }
  int read(uint8_t*, size_t) { return 0; }
  uint8_t connected() { return 0; }
  void setTimeout(uint32_t) {}
};
struct NetworkClientSecure : NetworkClient {
  void setCACert(const char*) {}
  void setHandshakeTimeout(unsigned long) {}
  int lastError(char*, const size_t) { return 0; }
};
