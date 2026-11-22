// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe (22.11.2026) rdzenia src/wysylka_obrazu.h, niezależnie
// od test/testy_wysylka_obrazu.cpp. Model odniesienia: najprostsza możliwa pętla „bajt po bajcie” (jak `_transfer()`
// w GxEPD2 1.6.9), bez dzielenia na kawałki. Sprawdza: długości 0, 1, 3, 511, 512, 513, 47 999, 48 000 przy buforze
// 512 B (KAWALEK_B) i przy pojemnościach nie będących wielokrotnością 4; wzorce bitów; że każdy kawałek zaczyna się
// na początku bufora (wyrównanie dla `spiWriteNL()`), że żaden nie przekracza pojemności, że przy obrazie 48 000 B
// każdy kawałek ma długość podzielną przez 4; że źródło się nie zmienia; że bajty za pojemnością bufora nie są
// ruszane (strażnik). Uruchomienie: tools/prototyp/testy.sh (program testy_wysylka_obrazu_brzegowe).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "wysylka_obrazu.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

struct Zapis {
  const uint8_t* bufor = nullptr;  // oczekiwany początek każdego kawałka
  std::vector<uint8_t> bajty;
  std::vector<size_t> dl;
  bool poczatekZawszeBufor = true;
};
static void pisz(void* k, const uint8_t* d, size_t n) {
  Zapis* z = static_cast<Zapis*>(k);
  if (d != z->bufor) z->poczatekZawszeBufor = false;
  z->bajty.insert(z->bajty.end(), d, d + n);
  z->dl.push_back(n);
}

// wzorce: stałe, naprzemienne, pojedyncze bity, rampa
static std::vector<uint8_t> wzorzec(int rodzaj, size_t n) {
  std::vector<uint8_t> v(n);
  for (size_t i = 0; i < n; i++) {
    switch (rodzaj) {
      case 0: v[i] = 0x00; break;
      case 1: v[i] = 0xFF; break;
      case 2: v[i] = 0xAA; break;
      case 3: v[i] = 0x55; break;
      case 4: v[i] = 0x01; break;
      case 5: v[i] = 0x80; break;
      default: v[i] = static_cast<uint8_t>(i * 131u + (i >> 8)); break;  // rampa z przeniesieniem
    }
  }
  return v;
}

static void testDlugosciIWzorce() {
  const size_t dlugosci[] = {0, 1, 3, 511, 512, 513, 47999, 48000};
  for (size_t poj : {wysylka_obrazu::KAWALEK_B, (size_t)1, (size_t)3, (size_t)5, (size_t)509, (size_t)513}) {
    for (size_t n : dlugosci) {
      for (int r = 0; r <= 6; r++) {
        const std::vector<uint8_t> zr = wzorzec(r, n);
        const std::vector<uint8_t> kopia = zr;
        std::vector<uint8_t> bufor(poj + 16, 0xC3);  // 16 B strażnika za pojemnością
        Zapis z;
        z.bufor = bufor.data();
        size_t w = wysylka_obrazu::wyslijOdwrocone(n ? zr.data() : nullptr, n, bufor.data(), poj, pisz, &z);
        if (n == 0) {
          SPRAWDZ(w == 0 && z.bajty.empty());
          continue;
        }
        // model: bajt po bajcie, ~bajt
        bool zgodne = z.bajty.size() == n;
        for (size_t i = 0; zgodne && i < n; i++) zgodne = z.bajty[i] == static_cast<uint8_t>(~zr[i]);
        SPRAWDZ(zgodne);
        SPRAWDZ(w == (n + poj - 1) / poj);
        SPRAWDZ(z.dl.size() == w);
        SPRAWDZ(z.poczatekZawszeBufor);
        bool dlDobre = true;
        for (size_t i = 0; i < z.dl.size(); i++)
          if (z.dl[i] == 0 || z.dl[i] > poj || (i + 1 < z.dl.size() && z.dl[i] != poj)) dlDobre = false;
        SPRAWDZ(dlDobre);
        SPRAWDZ(zr == kopia);  // źródło nietknięte
        bool straznik = true;
        for (size_t i = poj; i < bufor.size(); i++) straznik = straznik && bufor[i] == 0xC3;
        SPRAWDZ(straznik);
      }
    }
  }
}

static void testWartoscBrzegi() {
  for (size_t n : {(size_t)1, (size_t)3, (size_t)511, (size_t)512, (size_t)513, (size_t)47999, (size_t)48000}) {
    for (uint8_t v : {(uint8_t)0x00, (uint8_t)0xFF, (uint8_t)0x5A}) {
      std::vector<uint8_t> bufor(wysylka_obrazu::KAWALEK_B + 16, 0xC3);
      Zapis z;
      z.bufor = bufor.data();
      size_t w = wysylka_obrazu::wyslijWartosc(v, n, bufor.data(), wysylka_obrazu::KAWALEK_B, pisz, &z);
      SPRAWDZ(z.bajty == std::vector<uint8_t>(n, v));
      SPRAWDZ(w == (n + wysylka_obrazu::KAWALEK_B - 1) / wysylka_obrazu::KAWALEK_B);
      SPRAWDZ(z.poczatekZawszeBufor);
      bool straznik = true;
      for (size_t i = wysylka_obrazu::KAWALEK_B; i < bufor.size(); i++) straznik = straznik && bufor[i] == 0xC3;
      SPRAWDZ(straznik);
    }
  }
}

// Obraz panelu 48 000 B z KAWALEK_B: każdy kawałek podzielny przez 4 (spiWriteNL czyta słowami 32-bit; ostatni
// kawałek 384 B), liczba kawałków 94. Gdyby ktoś zmienił KAWALEK_B na wartość niepodzielną przez 4, test to złapie.
static void testObrazPanelu() {
  SPRAWDZ(wysylka_obrazu::KAWALEK_B % 4 == 0);
  const std::vector<uint8_t> zr = wzorzec(6, 48000);
  alignas(4) uint8_t bufor[wysylka_obrazu::KAWALEK_B];
  Zapis z;
  z.bufor = bufor;
  SPRAWDZ(wysylka_obrazu::wyslijOdwrocone(zr.data(), 48000, bufor, sizeof bufor, pisz, &z) == 94);
  bool podzielne = true;
  for (size_t d : z.dl) podzielne = podzielne && d % 4 == 0;
  SPRAWDZ(podzielne);
  SPRAWDZ(z.dl.back() == 384);
  // wyrównanie początku bufora (w adapterze `alignas(4) static`)
  SPRAWDZ((reinterpret_cast<uintptr_t>(bufor) & 3u) == 0);
}

// Dwa wywołania pod rząd na tym samym buforze (adapter: czyszczenie 0x13/0x10, potem obraz): drugie nie zależy
// od zawartości bufora po pierwszym.
static void testBuforWspolny() {
  alignas(4) uint8_t bufor[wysylka_obrazu::KAWALEK_B];
  Zapis a;
  a.bufor = bufor;
  wysylka_obrazu::wyslijWartosc(0x00, 1000, bufor, sizeof bufor, pisz, &a);
  const std::vector<uint8_t> zr = wzorzec(2, 1000);
  Zapis b;
  b.bufor = bufor;
  wysylka_obrazu::wyslijOdwrocone(zr.data(), 1000, bufor, sizeof bufor, pisz, &b);
  SPRAWDZ(b.bajty == std::vector<uint8_t>(1000, 0x55));
  Zapis c;
  c.bufor = bufor;
  wysylka_obrazu::wyslijWartosc(0xFF, 1000, bufor, sizeof bufor, pisz, &c);
  SPRAWDZ(c.bajty == std::vector<uint8_t>(1000, 0xFF));
}

int main() {
  testDlugosciIWzorce();
  testWartoscBrzegi();
  testObrazPanelu();
  testBuforWspolny();
  printf("testy_wysylka_obrazu_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
