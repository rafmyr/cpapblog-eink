// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
#include "Arduino.h"
struct JsonVariant {
  const char* operator|(const char* d) const { return d; }
  int operator|(int d) const { return d; }
};
struct JsonObject {
  JsonVariant operator[](const char*) const { return JsonVariant(); }
};
struct JsonArray {
  const JsonObject* begin() const { return nullptr; }
  const JsonObject* end() const { return nullptr; }
};
struct JsonDocument {
  template <typename T> bool is() const { return true; }
  template <typename T> T as() const { return T(); }
};
struct DeserializationError {
  explicit operator bool() const { return false; }
};
inline DeserializationError deserializeJson(JsonDocument&, const String&) { return DeserializationError(); }
