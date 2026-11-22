// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h. Sygnatury jak w GxEPD2 1.6.9 i Adafruit GFX; od 22.11.2026 z klasą
// bazową GxEPD2_EPD (chronione pola i funkcje, których używa SterownikBlokiem w src/adaptery_esp32.cpp) i stałymi
// GxEPD2_750_GDEY075T7 z src/gdey/GxEPD2_750_GDEY075T7.h.
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

class GxEPD2_EPD {
 public:
  void selectSPI(SPIClass& spi, SPISettings spi_settings) {
    _pSPIx = &spi;
    (void)spi_settings;
  }
  void init(uint32_t) {}  // adapter woła go wprost przy wysyłce blokiem (bez GxEPD2_BW)

 protected:
  void _reset() {}
  void _waitWhileBusy(const char* comment = 0, uint16_t busy_time = 5000) {
    (void)comment;
    (void)busy_time;
  }
  void _writeCommand(uint8_t) {}
  void _writeData(uint8_t) {}
  void _startTransfer() {}
  void _transfer(uint8_t) {}
  void _endTransfer() {}
  SPIClass* _pSPIx = nullptr;
  bool _initial_write = true, _initial_refresh = true;
  bool _power_is_on = false, _using_partial_mode = false, _hibernating = false;
};

class GxEPD2_750_GDEY075T7 : public GxEPD2_EPD {
 public:
  static const uint16_t WIDTH = 800;
  static const uint16_t HEIGHT = 480;
  static const bool hasFastPartialUpdate = true;  // jak GxEPD2 1.6.9 src/gdey/GxEPD2_750_GDEY075T7.h
  static const bool useFastPartialUpdateFromOTP = true;
  static const uint16_t power_on_time = 140;
  GxEPD2_750_GDEY075T7(int16_t, int16_t, int16_t, int16_t) {}
  // odświeżenie częściowe całej powierzchni (R-NF-25): obraz poprzedni (0x10), nowy (0x13), refresh okna
  void writeImageToPrevious(const uint8_t*, int16_t, int16_t, int16_t, int16_t, bool, bool, bool) {}
  void writeImage(const uint8_t*, int16_t, int16_t, int16_t, int16_t, bool, bool, bool) {}
  void refresh(bool partial_update_mode = false) { (void)partial_update_mode; }
  void refresh(int16_t, int16_t, int16_t, int16_t) {}
  void powerOff() {}
  void hibernate() {}
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
