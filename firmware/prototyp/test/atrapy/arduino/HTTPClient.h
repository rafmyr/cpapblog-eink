// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
#include "Arduino.h"
struct Stream {
  virtual ~Stream() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t*, size_t) = 0;
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual void flush() = 0;
};
enum followRedirects_t { HTTPC_DISABLE_FOLLOW_REDIRECTS, HTTPC_STRICT_FOLLOW_REDIRECTS, HTTPC_FORCE_FOLLOW_REDIRECTS };
struct HTTPClient {
  void setFollowRedirects(followRedirects_t) {}
  void setTimeout(int) {}
  void setConnectTimeout(int32_t) {}
  int getSize() { return -1; }
  int writeToStream(Stream*) { return 0; }
  bool begin(const String&) { return true; }
  void setAuthorization(const char*, const char*) {}
  void collectHeaders(const char* [], size_t) {}
  void addHeader(const String&, const String&) {}
  String header(const char*) { return String(""); }
  int GET() { return 200; }
  String getString() { return String(""); }
  void end() {}
};
