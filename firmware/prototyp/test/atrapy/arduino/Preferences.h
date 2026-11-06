// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
#include "Arduino.h"
struct Preferences {
  bool begin(const char*, bool) { return true; }
  void end() {}
  size_t putBytes(const char*, const void*, size_t n) { return n; }
  size_t getBytes(const char*, void*, size_t n) { return n; }
  size_t getBytesLength(const char*) { return 0; }
  uint32_t getUInt(const char*, uint32_t d) { return d; }
  size_t putUInt(const char*, uint32_t) { return 4; }
  int32_t getInt(const char*, int32_t d) { return d; }
  size_t putInt(const char*, int32_t) { return 4; }
  bool getBool(const char*, bool d) { return d; }
  size_t putBool(const char*, bool) { return 1; }
  bool remove(const char*) { return true; }
  String getString(const char*, const String& d = String()) { return d; }
  size_t putString(const char*, const char* v) { return strlen(v); }
};
