// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa WebServer.h (Arduino-ESP32) do sprawdzenia składni src/serwer_raportu_esp32.cpp na komputerze.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include "Arduino.h"
enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
class WebServer {
 public:
  explicit WebServer(int) {}
  void onNotFound(std::function<void()>) {}
  void begin() {}
  void stop() {}
  void handleClient() {}
  HTTPMethod method() { return HTTP_GET; }
  String uri() { return String(""); }
  int args() { return 0; }
  String argName(int) { return String(""); }
  String arg(int) { return String(""); }
  bool hasArg(const char*) { return false; }
  String arg(const char*) { return String(""); }
  void sendHeader(const String&, const String&) {}
  void setContentLength(size_t) {}
  void send(int, const char*, const char*) {}
  void sendContent(const char*, size_t) {}
};
