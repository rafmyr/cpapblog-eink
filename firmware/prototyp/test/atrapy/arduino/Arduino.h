// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapy nagłówków Arduino/ESP32 tylko do `g++ -fsyntax-only` na src/main.cpp (tools/prototyp/testy.sh).
// Nie są implementacją: sprawdzają, czy kod się parsuje i czy typy się zgadzają z tym, czego używa src/main.cpp.
#pragma once
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <math.h>
#include <time.h>
using std::isnan;

bool getLocalTime(struct tm* info, uint32_t ms = 5000);
void configTzTime(const char* tz, const char* server1, const char* server2 = nullptr, const char* server3 = nullptr);

#define SET_LOOP_TASK_STACK_SIZE(n) static const int _stos_loop = (n)
#define RTC_DATA_ATTR
#define RTC_NOINIT_ATTR
#define PROGMEM
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define constrain(x, a, b) ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))

class String {
 public:
  String(const char* s = "") : s_(s) {}
  String(const std::string& s) : s_(s) {}
  explicit String(int v) : s_(std::to_string(v)) {}
  explicit String(unsigned v) : s_(std::to_string(v)) {}
  explicit String(long v) : s_(std::to_string(v)) {}
  explicit String(float v, int = 2) : s_(std::to_string(v)) {}
  const char* c_str() const { return s_.c_str(); }
  unsigned length() const { return (unsigned)s_.size(); }
  void trim() {}
  String operator+(const String& o) const { return String(s_ + o.s_); }
  friend String operator+(const char* a, const String& b) { return String(std::string(a) + b.s_); }
  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  bool operator==(const char* o) const { return s_ == o; }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool startsWith(const char* p) const { return s_.rfind(p, 0) == 0; }
 private:
  std::string s_;
};

struct HardwareSerial {
  size_t setRxBufferSize(size_t n) { return n; }
  void begin(unsigned long) {}
  size_t println(const char*) { return 0; }
  size_t println(const String&) { return 0; }
  int printf(const char*, ...) { return 0; }
  int available() { return 0; }
  int read() { return -1; }
  void flush() {}
};
extern HardwareSerial Serial;

struct EspClass {  // ESP z Esp.h: tylko pola używane w main.cpp
  uint32_t getPsramSize() { return 0; }
  uint32_t getFreePsram() { return 0; }
};
extern EspClass ESP;

void pinMode(int, int);
int digitalRead(int);
void digitalWrite(int, int);
unsigned long millis();
void delay(unsigned long);
void tone(int, unsigned, unsigned long = 0);
uint32_t analogReadMilliVolts(int);
#if !defined(__GLIBC__) || __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 38)
size_t strlcpy(char*, const char*, size_t);
#endif
