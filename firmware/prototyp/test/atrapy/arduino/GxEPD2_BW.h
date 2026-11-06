// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Sygnatury jak w GxEPD2 i Adafruit GFX.
#pragma once
#include "Arduino.h"
#include "SPI.h"
#include <gfxfont.h>
enum { GxEPD_WHITE = 0xFFFF, GxEPD_BLACK = 0x0000 };

struct GFXcanvas1 {
  GFXcanvas1(uint16_t, uint16_t) {}
  void drawPixel(int16_t, int16_t, uint16_t) {}
  void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  uint8_t* getBuffer() const { return nullptr; }
};

struct GxEPD2_750_GDEY075T7 {
  static const uint16_t HEIGHT = 480;
  GxEPD2_750_GDEY075T7(int, int, int, int) {}
  void selectSPI(SPIClass&, SPISettings) {}
};

template <typename Driver, uint16_t page_height>
struct GxEPD2_BW {
  Driver epd2;
  explicit GxEPD2_BW(Driver d) : epd2(d) {}
  void init(uint32_t) {}
  void setFullWindow() {}
  void firstPage() {}
  bool nextPage() { return false; }
  void fillScreen(uint16_t) {}
  void drawBitmap(int16_t, int16_t, const uint8_t*, int16_t, int16_t, uint16_t) {}
  void hibernate() {}
};
