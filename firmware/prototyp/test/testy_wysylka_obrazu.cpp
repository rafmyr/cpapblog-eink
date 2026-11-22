// SPDX-License-Identifier: GPL-3.0-or-later
// Wysyłka obrazu do sterownika e-ink kawałkami (src/wysylka_obrazu.h, T-KOMP-17, 22.11.2026): bajty wysłane blokami
// mają być bajt w bajt takie, jak z pętli GxEPD2 1.6.9 (`GxEPD2_750_GDEY075T7::_writeImage()`, przepisana niżej jako
// model) w obu dotychczasowych drogach adaptera:
//  - częściowe: `writeImageToPrevious()` / `writeImage()` z kanwy z `invert = true`;
//  - pełne: stronicowanie `GxEPD2_BW` (`fillScreen(GxEPD_WHITE)`, `drawBitmap(kanwa, GxEPD_BLACK)`, potem
//    `writeImageForFullRefresh()` bufora biblioteki bez odwracania; model `drawPixel()` dla obrotu 0 niżej);
//  - czyszczenie pamięci sterownika przy pierwszym zapisie (`_writeScreenBuffer()`: 48 000 × 0xFF albo 0x00).
// Do tego podział na kawałki (pojemność, reszta, przypadki puste). Sprzętu (SPI, CS, DC) test nie dotyka.
// Uruchomienie: tools/prototyp/testy.sh (program testy_wysylka_obrazu).
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include "wysylka_obrazu.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

static const int SZER = 800, WYS = 480;
static const size_t BAJTOW = SZER / 8 * WYS;  // 48 000

struct Zapis {
  std::vector<uint8_t> bajty;
  std::vector<size_t> dlugosci;
};
static void piszDo(void* k, const uint8_t* d, size_t n) {
  Zapis* z = static_cast<Zapis*>(k);
  z->bajty.insert(z->bajty.end(), d, d + n);
  z->dlugosci.push_back(n);
}

// Model pętli danych GxEPD2 1.6.9 `GxEPD2_750_GDEY075T7::_writeImage()` (bez komend, pgm = false).
static std::vector<uint8_t> petlaBiblioteki(const uint8_t* bitmap, int16_t x, int16_t y, int16_t w, int16_t h,
                                            bool invert, bool mirror_y) {
  std::vector<uint8_t> out;
  uint16_t wb = (uint16_t)((w + 7) / 8);
  x = (int16_t)(x - x % 8);
  w = (int16_t)(wb * 8);
  int16_t x1 = x < 0 ? 0 : x;
  int16_t y1 = y < 0 ? 0 : y;
  int16_t w1 = x + w < int16_t(SZER) ? w : (int16_t)(int16_t(SZER) - x);
  int16_t h1 = y + h < int16_t(WYS) ? h : (int16_t)(int16_t(WYS) - y);
  int16_t dx = (int16_t)(x1 - x);
  int16_t dy = (int16_t)(y1 - y);
  w1 = (int16_t)(w1 - dx);
  h1 = (int16_t)(h1 - dy);
  if ((w1 <= 0) || (h1 <= 0)) return out;
  for (int16_t i = 0; i < h1; i++) {
    for (int16_t j = 0; j < w1 / 8; j++) {
      uint16_t idx = mirror_y ? (uint16_t)(j + dx / 8 + uint16_t((h - 1 - (i + dy))) * wb)
                              : (uint16_t)(j + dx / 8 + uint16_t(i + dy) * wb);
      uint8_t data = bitmap[idx];
      if (invert) data = (uint8_t)~data;
      out.push_back(data);
    }
  }
  return out;
}

// Model dotychczasowej drogi pełnej: bufor GxEPD2_BW po fillScreen(GxEPD_WHITE) i drawBitmap(kanwa, GxEPD_BLACK)
// (Adafruit GFX: bit 1 kanwy = piksel czarny, bit 0 = nic; GxEPD2_BW::drawPixel() obrót 0, pełne okno, jedna strona).
static std::vector<uint8_t> buforBibliotekiPoDrawBitmap(const uint8_t* kanwa) {
  std::vector<uint8_t> b(BAJTOW, 0xFF);
  const int bajtowWiersza = (SZER + 7) / 8;
  for (int y = 0; y < WYS; y++)
    for (int x = 0; x < SZER; x++) {
      const bool czarny = kanwa[y * bajtowWiersza + x / 8] & (0x80 >> (x & 7));
      if (!czarny) continue;
      const uint32_t i = (uint32_t)(x / 8) + (uint32_t)y * (uint32_t)(SZER / 8);
      b[i] = (uint8_t)(b[i] & (0xFF ^ (1 << (7 - x % 8))));  // GxEPD_BLACK = 0: bit zerowany
    }
  return b;
}

static std::vector<uint8_t> losowyObraz(unsigned ziarno) {
  std::mt19937 g(ziarno);
  std::vector<uint8_t> v(BAJTOW);
  for (auto& b : v) b = (uint8_t)(g() & 0xFF);
  return v;
}

// Droga częściowa: writeImageToPrevious()/writeImage(kanwa, 0, 0, 800, 480, invert = true) ≡ wysyłka blokiem.
static void testCzesciowe_bajtWBajt() {
  for (unsigned z = 1; z <= 5; z++) {
    const std::vector<uint8_t> kanwa = losowyObraz(z);
    const std::vector<uint8_t> wzor = petlaBiblioteki(kanwa.data(), 0, 0, SZER, WYS, true, false);
    SPRAWDZ(wzor.size() == BAJTOW);
    uint8_t bufor[wysylka_obrazu::KAWALEK_B];
    Zapis zap;
    size_t n = wysylka_obrazu::wyslijOdwrocone(kanwa.data(), BAJTOW, bufor, sizeof bufor, piszDo, &zap);
    SPRAWDZ(zap.bajty == wzor);
    SPRAWDZ(n == (BAJTOW + wysylka_obrazu::KAWALEK_B - 1) / wysylka_obrazu::KAWALEK_B);  // 94 kawałki
    SPRAWDZ(zap.dlugosci.size() == n);
  }
  // obrazy skrajne: cały biały (0x00 na kanwie) i cały czarny (0xFF)
  for (uint8_t v : {(uint8_t)0x00, (uint8_t)0xFF, (uint8_t)0x5A}) {
    std::vector<uint8_t> kanwa(BAJTOW, v);
    uint8_t bufor[wysylka_obrazu::KAWALEK_B];
    Zapis zap;
    wysylka_obrazu::wyslijOdwrocone(kanwa.data(), BAJTOW, bufor, sizeof bufor, piszDo, &zap);
    SPRAWDZ(zap.bajty == petlaBiblioteki(kanwa.data(), 0, 0, SZER, WYS, true, false));
  }
}

// Droga pełna: bufor GxEPD2_BW po drawBitmap() wysłany bez odwracania ≡ kanwa wysłana z odwróceniem.
static void testPelne_bajtWBajt() {
  for (unsigned z = 11; z <= 13; z++) {
    const std::vector<uint8_t> kanwa = losowyObraz(z);
    const std::vector<uint8_t> buf = buforBibliotekiPoDrawBitmap(kanwa.data());
    const std::vector<uint8_t> wzor = petlaBiblioteki(buf.data(), 0, 0, SZER, WYS, false, false);
    uint8_t bufor[wysylka_obrazu::KAWALEK_B];
    Zapis zap;
    wysylka_obrazu::wyslijOdwrocone(kanwa.data(), BAJTOW, bufor, sizeof bufor, piszDo, &zap);
    SPRAWDZ(zap.bajty == wzor);
  }
}

// Czyszczenie pamięci sterownika (_writeScreenBuffer): 48 000 × wartość.
static void testWartosc() {
  for (uint8_t v : {(uint8_t)0xFF, (uint8_t)0x00}) {
    uint8_t bufor[wysylka_obrazu::KAWALEK_B];
    Zapis zap;
    size_t n = wysylka_obrazu::wyslijWartosc(v, BAJTOW, bufor, sizeof bufor, piszDo, &zap);
    SPRAWDZ(zap.bajty == std::vector<uint8_t>(BAJTOW, v));
    SPRAWDZ(n == zap.dlugosci.size());
    SPRAWDZ(zap.dlugosci.back() == BAJTOW % wysylka_obrazu::KAWALEK_B);  // reszta 384 B
  }
}

// Podział: każdy kawałek ≤ pojemność, tylko ostatni krótszy, suma = n; przypadki puste bez wywołań.
static void testPodzial() {
  const std::vector<uint8_t> kanwa = losowyObraz(7);
  for (size_t poj : {(size_t)1, (size_t)3, (size_t)64, (size_t)500, (size_t)512, (size_t)48000, (size_t)60000}) {
    std::vector<uint8_t> bufor(poj);
    for (size_t n : {(size_t)1, (size_t)511, (size_t)512, (size_t)513, (size_t)1001, BAJTOW}) {
      Zapis zap;
      size_t w = wysylka_obrazu::wyslijOdwrocone(kanwa.data(), n, bufor.data(), poj, piszDo, &zap);
      SPRAWDZ(w == (n + poj - 1) / poj);
      SPRAWDZ(zap.bajty.size() == n);
      bool dobrze = true;
      for (size_t i = 0; i < zap.dlugosci.size(); i++) {
        if (zap.dlugosci[i] == 0 || zap.dlugosci[i] > poj) dobrze = false;
        if (i + 1 < zap.dlugosci.size() && zap.dlugosci[i] != poj) dobrze = false;
      }
      SPRAWDZ(dobrze);
      bool bajty = true;
      for (size_t i = 0; i < n; i++)
        if (zap.bajty[i] != (uint8_t)~kanwa[i]) bajty = false;
      SPRAWDZ(bajty);
    }
  }
  uint8_t b[8];
  Zapis zap;
  SPRAWDZ(wysylka_obrazu::wyslijOdwrocone(kanwa.data(), 0, b, sizeof b, piszDo, &zap) == 0);
  SPRAWDZ(wysylka_obrazu::wyslijOdwrocone(nullptr, 10, b, sizeof b, piszDo, &zap) == 0);
  SPRAWDZ(wysylka_obrazu::wyslijOdwrocone(kanwa.data(), 10, nullptr, sizeof b, piszDo, &zap) == 0);
  SPRAWDZ(wysylka_obrazu::wyslijOdwrocone(kanwa.data(), 10, b, 0, piszDo, &zap) == 0);
  SPRAWDZ(wysylka_obrazu::wyslijOdwrocone(kanwa.data(), 10, b, sizeof b, nullptr, &zap) == 0);
  SPRAWDZ(wysylka_obrazu::wyslijWartosc(0xFF, 0, b, sizeof b, piszDo, &zap) == 0);
  SPRAWDZ(wysylka_obrazu::wyslijWartosc(0xFF, 10, nullptr, sizeof b, piszDo, &zap) == 0);
  SPRAWDZ(wysylka_obrazu::wyslijWartosc(0xFF, 10, b, 0, piszDo, &zap) == 0);
  SPRAWDZ(zap.bajty.empty() && zap.dlugosci.empty());
  // bufor krótszy niż n przy wartości: memset tylko do pojemności, bajty dalej poprawne
  Zapis z2;
  SPRAWDZ(wysylka_obrazu::wyslijWartosc(0xA5, 5, b, sizeof b, piszDo, &z2) == 1);
  SPRAWDZ(z2.bajty == std::vector<uint8_t>(5, 0xA5));
}

int main() {
  testCzesciowe_bajtWBajt();
  testPelne_bajtWBajt();
  testWartosc();
  testPodzial();
  printf("testy_wysylka_obrazu: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
