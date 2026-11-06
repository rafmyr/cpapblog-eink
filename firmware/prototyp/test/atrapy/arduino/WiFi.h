// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
#include "Arduino.h"
#include <functional>
#include "esp_wifi.h"

struct IPAddress {
  IPAddress() {}
  bool fromString(const char*) { return true; }
  String toString() const { return String("0.0.0.0"); }
  bool operator==(const IPAddress&) const { return true; }
  operator uint32_t() const { return 0; }
};

typedef int WiFiEvent_t;
struct WiFiEventInfo_t {
  struct { int reason; } wifi_sta_disconnected;
};
enum { ARDUINO_EVENT_WIFI_STA_DISCONNECTED = 1 };
enum wl_status_t { WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum { WIFI_STA = 1, WIFI_OFF = 0, WIFI_AP = 2 };

struct WiFiClass {
  void onEvent(std::function<void(WiFiEvent_t, WiFiEventInfo_t)>, int) {}
  bool mode(int) { return true; }
  int getMode() { return WIFI_STA; }
  void begin(const char*, const char*) {}
  wl_status_t status() { return WL_CONNECTED; }
  bool disconnect(bool = false) { return true; }
  int scanNetworks() { return 0; }
  String SSID(int) { return String(""); }
  String SSID() const { return String(""); }
  int channel(int) { return 0; }
  int RSSI(int = 0) { return 0; }
  int encryptionType(int) { return 0; }
  IPAddress localIP() { return IPAddress(); }
  void persistent(bool) {}
  bool softAP(const char*, const char* = nullptr, int = 1, int = 0, int = 4) { return true; }
  bool softAPdisconnect(bool = false) { return true; }
  IPAddress softAPIP() { return IPAddress(); }
};
extern WiFiClass WiFi;
