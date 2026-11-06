// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
#include "WiFi.h"
struct MDNSResponder {
  bool begin(const char*) { return true; }
  void end() {}
  IPAddress queryHost(const char*, int) { return IPAddress(); }
};
extern MDNSResponder MDNS;
