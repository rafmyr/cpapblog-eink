// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Sygnatury jak w ArduinoOTA.h z arduino-esp32 3.x.
#pragma once
#include "Arduino.h"
#include <functional>
#define U_FLASH 0
#define U_SPIFFS 100
typedef enum { OTA_AUTH_ERROR, OTA_BEGIN_ERROR, OTA_CONNECT_ERROR, OTA_RECEIVE_ERROR, OTA_END_ERROR } ota_error_t;
struct ArduinoOTAClass {
  ArduinoOTAClass& setHostname(const char*) { return *this; }
  ArduinoOTAClass& setPassword(const char*) { return *this; }
  ArduinoOTAClass& setRebootOnSuccess(bool) { return *this; }
  ArduinoOTAClass& onStart(std::function<void(void)>) { return *this; }
  ArduinoOTAClass& onEnd(std::function<void(void)>) { return *this; }
  ArduinoOTAClass& onError(std::function<void(ota_error_t)>) { return *this; }
  ArduinoOTAClass& onProgress(std::function<void(unsigned int, unsigned int)>) { return *this; }
  void end() {}
  void begin() {}
  void handle() {}
  int getCommand() { return U_FLASH; }
};
extern ArduinoOTAClass ArduinoOTA;
