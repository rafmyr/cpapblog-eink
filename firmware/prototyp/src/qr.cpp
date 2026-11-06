// SPDX-License-Identifier: GPL-3.0-or-later AND MIT
// SPDX-FileCopyrightText: 2026 Rafał Myrta
// SPDX-FileCopyrightText: Project Nayuki
// Koder QR (src/qr.h) napisany według referencyjnej implementacji Project Nayuki (QR Code generator library);
// części wyrażeń i tabel odpowiadają tamtej implementacji, dlatego plik zawiera jej notę licencyjną MIT:
//
// Copyright (c) Project Nayuki. (MIT License)
// https://www.nayuki.io/page/qr-code-generator-library
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
// - The above copyright notice and this permission notice shall be included in
//   all copies or substantial portions of the Software.
// - The Software is provided "as is", without warranty of any kind, express or
//   implied, including but not limited to the warranties of merchantability,
//   fitness for a particular purpose and noninfringement. In no event shall the
//   authors or copyright holders be liable for any claim, damages or other
//   liability, whether in an action of contract, tort or otherwise, arising from,
//   out of or in connection with the Software or the use or other dealings in the
//   Software.
#include "qr.h"

#include <algorithm>
#include <cstdlib>

namespace qr {
namespace {

// Tabele normy dla poziomu M, indeks = wersja (0 nieużywany)
const int ECC_NA_BLOK[11] = {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26};
const int BLOKOW[11] = {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5};
const int BITY_FORMATU_M = 0;  // poziom M w polu formatu: 00

int modulyDanych(int w) {  // moduły na dane i korekcję (bez wzorców funkcyjnych)
  int r = (16 * w + 128) * w + 64;
  if (w >= 2) {
    int n = w / 7 + 2;
    r -= (25 * n - 10) * n - 55;
    if (w >= 7) r -= 36;
  }
  return r;
}
int slowDanych(int w) { return modulyDanych(w) / 8 - ECC_NA_BLOK[w] * BLOKOW[w]; }

uint8_t mnozGf(uint8_t x, uint8_t y) {  // mnożenie w GF(2^8) modulo 0x11D
  int z = 0;
  for (int i = 7; i >= 0; i--) {
    z = (z << 1) ^ ((z >> 7) * 0x11D);
    z ^= ((y >> i) & 1) * x;
  }
  return (uint8_t)z;
}
std::vector<uint8_t> dzielnikRs(int stopien) {
  std::vector<uint8_t> r((size_t)stopien, 0);
  r[(size_t)stopien - 1] = 1;
  uint8_t pierw = 1;
  for (int i = 0; i < stopien; i++) {
    for (size_t j = 0; j < r.size(); j++) {
      r[j] = mnozGf(r[j], pierw);
      if (j + 1 < r.size()) r[j] ^= r[j + 1];
    }
    pierw = mnozGf(pierw, 0x02);
  }
  return r;
}
std::vector<uint8_t> resztaRs(const std::vector<uint8_t>& dane, const std::vector<uint8_t>& dz) {
  std::vector<uint8_t> r(dz.size(), 0);
  for (uint8_t b : dane) {
    uint8_t c = (uint8_t)(b ^ r[0]);
    r.erase(r.begin());
    r.push_back(0);
    for (size_t i = 0; i < r.size(); i++) r[i] ^= mnozGf(dz[i], c);
  }
  return r;
}

struct Siatka {
  int n;
  std::vector<uint8_t> m, f;  // moduł ciemny, moduł funkcyjny
  explicit Siatka(int r) : n(r), m((size_t)r * (size_t)r, 0), f((size_t)r * (size_t)r, 0) {}
  size_t i(int x, int y) const { return (size_t)y * (size_t)n + (size_t)x; }
  void ustawF(int x, int y, bool c) {
    m[i(x, y)] = c ? 1 : 0;
    f[i(x, y)] = 1;
  }
};

void wzorzecWykrywania(Siatka& s, int cx, int cy) {
  for (int dy = -4; dy <= 4; dy++)
    for (int dx = -4; dx <= 4; dx++) {
      int x = cx + dx, y = cy + dy;
      if (x < 0 || y < 0 || x >= s.n || y >= s.n) continue;
      int d = std::max(std::abs(dx), std::abs(dy));
      s.ustawF(x, y, d != 2 && d != 4);
    }
}
void wzorzecWyrownania(Siatka& s, int cx, int cy) {
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++) s.ustawF(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
}
std::vector<int> pozycjeWyrownania(int w, int n) {
  if (w == 1) return {};
  int ile = w / 7 + 2;
  int krok = (w * 4 + ile * 2 + 1) / (ile * 2 - 2) * 2;
  std::vector<int> r;
  r.push_back(6);
  for (int k = ile - 1; k >= 1; k--) r.push_back(n - 7 - (k - 1) * krok);
  std::sort(r.begin() + 1, r.end());
  return r;
}
void bityFormatu(Siatka& s, int maska) {
  int dane = BITY_FORMATU_M << 3 | maska;
  int r = dane;
  for (int i = 0; i < 10; i++) r = (r << 1) ^ ((r >> 9) * 0x537);
  int b = (dane << 10 | r) ^ 0x5412;
  auto bit = [b](int i) { return ((b >> i) & 1) != 0; };
  for (int i = 0; i <= 5; i++) s.ustawF(8, i, bit(i));
  s.ustawF(8, 7, bit(6));
  s.ustawF(8, 8, bit(7));
  s.ustawF(7, 8, bit(8));
  for (int i = 9; i < 15; i++) s.ustawF(14 - i, 8, bit(i));
  for (int i = 0; i < 8; i++) s.ustawF(s.n - 1 - i, 8, bit(i));
  for (int i = 8; i < 15; i++) s.ustawF(8, s.n - 15 + i, bit(i));
  s.ustawF(8, s.n - 8, true);  // moduł zawsze ciemny
}
void bityWersji(Siatka& s, int w) {
  if (w < 7) return;
  int r = w;
  for (int i = 0; i < 12; i++) r = (r << 1) ^ ((r >> 11) * 0x1F25);
  long b = (long)w << 12 | r;
  for (int i = 0; i < 18; i++) {
    bool c = ((b >> i) & 1) != 0;
    int a = s.n - 11 + i % 3, k = i / 3;
    s.ustawF(a, k, c);
    s.ustawF(k, a, c);
  }
}
void wzorceFunkcyjne(Siatka& s, int w) {
  for (int i = 0; i < s.n; i++) {
    s.ustawF(6, i, i % 2 == 0);
    s.ustawF(i, 6, i % 2 == 0);
  }
  wzorzecWykrywania(s, 3, 3);
  wzorzecWykrywania(s, s.n - 4, 3);
  wzorzecWykrywania(s, 3, s.n - 4);
  std::vector<int> p = pozycjeWyrownania(w, s.n);
  size_t k = p.size();
  for (size_t a = 0; a < k; a++)
    for (size_t b = 0; b < k; b++) {
      if ((a == 0 && b == 0) || (a == 0 && b == k - 1) || (a == k - 1 && b == 0)) continue;
      wzorzecWyrownania(s, p[a], p[b]);
    }
  bityFormatu(s, 0);  // rezerwacja miejsc; właściwe bity po wyborze maski
  bityWersji(s, w);
}
void slowa(Siatka& s, const std::vector<uint8_t>& d) {
  size_t i = 0;
  for (int prawo = s.n - 1; prawo >= 1; prawo -= 2) {
    if (prawo == 6) prawo = 5;
    for (int v = 0; v < s.n; v++)
      for (int j = 0; j < 2; j++) {
        int x = prawo - j;
        bool wGore = ((prawo + 1) & 2) == 0;
        int y = wGore ? s.n - 1 - v : v;
        if (!s.f[s.i(x, y)] && i < d.size() * 8) {
          s.m[s.i(x, y)] = (uint8_t)((d[i >> 3] >> (7 - (i & 7))) & 1);
          i++;
        }
      }
  }
}
bool maskaBit(int maska, int x, int y) {
  switch (maska) {
    case 0: return (x + y) % 2 == 0;
    case 1: return y % 2 == 0;
    case 2: return x % 3 == 0;
    case 3: return (x + y) % 3 == 0;
    case 4: return (x / 3 + y / 2) % 2 == 0;
    case 5: return x * y % 2 + x * y % 3 == 0;
    case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
    default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
  }
}
void nalozMaske(Siatka& s, int maska) {
  for (int y = 0; y < s.n; y++)
    for (int x = 0; x < s.n; x++)
      if (!s.f[s.i(x, y)] && maskaBit(maska, x, y)) s.m[s.i(x, y)] ^= 1;
}
long kara(const Siatka& s) {
  long k = 0;
  for (int pion = 0; pion < 2; pion++)  // N1: serie ≥ 5 w wierszach i kolumnach
    for (int a = 0; a < s.n; a++) {
      int dl = 0;
      uint8_t pop = 2;
      for (int b = 0; b < s.n; b++) {
        uint8_t c = pion ? s.m[s.i(a, b)] : s.m[s.i(b, a)];
        if (c == pop) {
          dl++;
        } else {
          if (dl >= 5) k += 3 + (dl - 5);
          dl = 1;
          pop = c;
        }
      }
      if (dl >= 5) k += 3 + (dl - 5);
    }
  for (int y = 0; y + 1 < s.n; y++)  // N2: bloki 2×2
    for (int x = 0; x + 1 < s.n; x++) {
      uint8_t c = s.m[s.i(x, y)];
      if (c == s.m[s.i(x + 1, y)] && c == s.m[s.i(x, y + 1)] && c == s.m[s.i(x + 1, y + 1)]) k += 3;
    }
  long ciemne = 0;  // N4: udział ciemnych
  for (uint8_t c : s.m) ciemne += c;
  long calosc = (long)s.n * s.n;
  long odch = std::labs(ciemne * 20 - calosc * 10);
  k += ((odch + calosc - 1) / calosc - 1) * 10;
  return k;
}

}  // namespace

Kod koduj(const std::string& tresc, int maskaWymuszona) {
  Kod wynik;
  int w = 1;
  size_t bityTresci = 0;
  for (; w <= 10; w++) {
    int bityLicznika = w <= 9 ? 8 : 16;
    bityTresci = 4 + (size_t)bityLicznika + tresc.size() * 8;
    if (bityTresci <= (size_t)slowDanych(w) * 8 && tresc.size() < (1u << bityLicznika)) break;
  }
  if (w > 10) return wynik;
  // strumień bitów: tryb bajtowy 0100, licznik, treść, terminator, dopełnienie
  std::vector<uint8_t> bity;
  auto dopisz = [&bity](unsigned long v, int ile) {
    for (int i = ile - 1; i >= 0; i--) bity.push_back((uint8_t)((v >> i) & 1));
  };
  dopisz(4, 4);
  dopisz(tresc.size(), w <= 9 ? 8 : 16);
  for (unsigned char c : tresc) dopisz(c, 8);
  size_t pojemnosc = (size_t)slowDanych(w) * 8;
  for (int i = 0; i < 4 && bity.size() < pojemnosc; i++) bity.push_back(0);
  while (bity.size() % 8) bity.push_back(0);
  for (uint8_t p = 0xEC; bity.size() < pojemnosc; p = (uint8_t)(p == 0xEC ? 0x11 : 0xEC)) dopisz(p, 8);
  std::vector<uint8_t> dane(bity.size() / 8, 0);
  for (size_t i = 0; i < bity.size(); i++) dane[i >> 3] = (uint8_t)(dane[i >> 3] | bity[i] << (7 - (i & 7)));
  // bloki, korekcja Reeda-Solomona, przeplot
  int nb = BLOKOW[w], ecc = ECC_NA_BLOK[w];
  int surowe = modulyDanych(w) / 8;
  int krotkich = nb - surowe % nb;
  int dlKrotkiego = surowe / nb;
  std::vector<std::vector<uint8_t>> bloki;
  std::vector<uint8_t> dz = dzielnikRs(ecc);
  size_t k = 0;
  for (int i = 0; i < nb; i++) {
    size_t dl = (size_t)(dlKrotkiego - ecc) + (i < krotkich ? 0U : 1U);
    std::vector<uint8_t> b(dane.begin() + (long)k, dane.begin() + (long)(k + dl));
    k += dl;
    std::vector<uint8_t> e = resztaRs(b, dz);
    if (i < krotkich) b.push_back(0);
    b.insert(b.end(), e.begin(), e.end());
    bloki.push_back(b);
  }
  std::vector<uint8_t> slowaWyj;
  const size_t dlBloku = bloki.empty() ? 0 : bloki[0].size();
  for (size_t i = 0; i < dlBloku; i++)
    for (size_t j = 0; j < bloki.size(); j++)
      if (i != (size_t)(dlKrotkiego - ecc) || (int)j >= krotkich) slowaWyj.push_back(bloki[j][i]);
  int n = 17 + 4 * w;
  Siatka s(n);
  wzorceFunkcyjne(s, w);
  slowa(s, slowaWyj);
  int maska = maskaWymuszona;
  if (maska < 0 || maska > 7) {
    long najm = -1;
    for (int m = 0; m < 8; m++) {
      Siatka t = s;
      nalozMaske(t, m);
      bityFormatu(t, m);
      long kk = kara(t);
      if (najm < 0 || kk < najm) {
        najm = kk;
        maska = m;
      }
    }
  }
  nalozMaske(s, maska);
  bityFormatu(s, maska);
  wynik.rozmiar = n;
  wynik.wersja = w;
  wynik.maska = maska;
  wynik.moduly = s.m;
  return wynik;
}

}  // namespace qr
