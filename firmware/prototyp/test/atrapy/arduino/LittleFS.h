// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Sygnatury jak w FS.h z arduino-esp32.
#pragma once
#include "Arduino.h"
struct File {
  explicit operator bool() const { return true; }
  int available() { return 0; }
  String readStringUntil(char) { return String(""); }
  size_t println(const char*) { return 0; }
  size_t print(const char*) { return 0; }
  size_t write(const uint8_t*, size_t n) { return n; }
  size_t size() { return 0; }
  bool seek(uint32_t) { return true; }
  int read() { return -1; }
  size_t read(uint8_t*, size_t n) { return n; }
  bool isDirectory() { return false; }
  File openNextFile(const char* = "r") { return File(); }
  const char* name() const { return ""; }
  const char* path() const { return ""; }
  int printf(const char*, ...) { return 0; }
  void close() {}
};
struct LittleFSFS {
  bool begin(bool = false) { return true; }
  bool format() { return true; }
  void end() {}
  bool exists(const char*) { return false; }
  bool exists(const String&) { return false; }
  File open(const char*, const char*) { return File(); }
  File open(const String&, const char*) { return File(); }
  bool rename(const char*, const char*) { return true; }
  bool rename(const String&, const String&) { return true; }
  bool remove(const char*) { return true; }
  bool remove(const String&) { return true; }
  bool mkdir(const char*) { return true; }
  bool mkdir(const String&) { return true; }
  size_t totalBytes() { return 0; }
  size_t usedBytes() { return 0; }
};
extern LittleFSFS LittleFS;
