// SPDX-License-Identifier: GPL-3.0-or-later
// Generator losowych historii dla testu test/testy_norma_tymczasowa_naiwna.cpp (ADR-0048). Ustawienia syntetyczne.
#pragma once
#include <cmath>
#include <random>
#include <set>
#include <string>
#include <vector>
#include "noc.h"
#include "wspolne.h"

struct Przypadek {
  std::vector<Noc> h;
  std::string dzis;
  std::set<std::string> wyk;
  WykluczeniaNormy wm;
  bool zWm = false;
};

inline float los(std::mt19937& g, float a, float b) { return std::uniform_real_distribution<float>(a, b)(g); }
inline int losI(std::mt19937& g, int a, int b) { return std::uniform_int_distribution<int>(a, b)(g); }

inline Przypadek generuj(unsigned ziarno) {
  std::mt19937 g(ziarno);
  Przypadek p;
  static const char* TRYBY[] = {"AutoSet", "CPAP", "ASV", "ASVAuto"};
  int trybStartowy = losI(g, 0, 3);
  bool zmianaTrybu = losI(g, 0, 9) == 0;
  // pula ustawień: 1…5 zestawów; czasem powrót do wcześniejszych
  int pula = losI(g, 1, 5);
  std::vector<std::string> ust;
  for (int k = 0; k < pula; k++) ust.push_back("/" + std::to_string(7 + losI(g, 0, 3)) + "/" + std::to_string(12 + k) + "/3");
  int nocy = losI(g, 1, 160);
  std::string start = "2026-05-01";
  int dzien = 0, blok = losI(g, 0, pula - 1), zostalo = losI(g, 1, 20);
  int tryb = trybStartowy;
  float bazaW = los(g, 5, 30), bazaC = los(g, 6, 14);
  int zbiorPustych = losI(g, 0, 3);  // 0: bez ustawień od początku przez kilka nocy
  for (int i = 0; i < nocy; i++) {
    dzien += losI(g, 0, 6) == 0 ? losI(g, 2, 5) : 1;  // luki
    if (--zostalo <= 0) {
      blok = losI(g, 0, pula - 1);
      zostalo = losI(g, 1, 25);
      bazaW = los(g, 5, 30);
      bazaC = los(g, 6, 14);
      if (zmianaTrybu && losI(g, 0, 3) == 0) tryb = losI(g, 0, 3);
    }
    Noc n;
    n.data = dodajDni(start, dzien);
    n.wazna = losI(g, 0, 9) != 0;
    n.m01 = losI(g, 0, 19) == 0 ? -1 : losI(g, 60, 520);
    auto v = [&](float b, float r) { return losI(g, 0, 11) == 0 ? NAN : b + los(g, -r, r); };
    n.ahi = v(2, 1.5);
    n.cmed = v(bazaC, 1);
    n.cp95 = v(bazaC + 2, 1);
    n.wmed = v(bazaW / 2, 2);
    n.wp95 = v(bazaW, 5);
    bool asv = tryb >= 2;
    n.emed = asv ? v(bazaC - 2, 1) : NAN;
    n.ep95 = asv ? v(bazaC - 1, 1) : NAN;
    n.psmed = asv ? v(4, 1) : NAN;
    n.psp95 = asv ? v(7, 1) : NAN;
    n.epapStr = asv && losI(g, 0, 7) == 0;
    n.oa = v(0.3, 0.2);
    n.ca = v(0.5, 0.4);
    n.h = v(0.6, 0.4);
    n.csr = v(2, 2);
    n.m13 = v(15, 2);
    n.m14 = v(450, 50);
    n.m15 = v(7, 1);
    n.m10p = v(0.2, 0.1);
    n.m17 = losI(g, 0, 9) == 0 ? -1 : losI(g, 0, 3);
    int z = losI(g, 0, 9);
    n.zrodlo = z < 6 ? 'A' : z < 9 ? 'K' : 'S';
    n.tryb = TRYBY[tryb];
    bool pustePocz = zbiorPustych == 0 && i < 10;
    bool brakOdczytu = losI(g, 0, 7) == 0;
    n.ustawienia = (pustePocz || brakOdczytu) ? "" : std::string(TRYBY[tryb]) + ust[blok];
    if (losI(g, 0, 15) == 0) n.tryb = "";  // tryb z ustawień
    p.h.push_back(n);
    if (losI(g, 0, 40) == 0) p.h.push_back(n);  // duplikat daty
  }
  if (nocy > 3 && losI(g, 0, 1)) std::swap(p.h[0], p.h[nocy / 2]);  // kolejność nieposortowana
  int r = losI(g, 0, 5);
  std::string ost = p.h.empty() ? start : dodajDni(start, dzien);
  p.dzis = r == 0 ? dodajDni(ost, losI(g, 1, 100)) : r == 1 ? dodajDni(start, losI(g, 0, dzien)) : ost;
  if (losI(g, 0, 3) == 0)
    for (auto& n : p.h) if (losI(g, 0, 8) == 0) p.wyk.insert(n.data);
  if (losI(g, 0, 2) == 0) {
    p.zWm = true;
    std::set<std::string>* pola[] = {&p.wm.m01, &p.wm.ahi, &p.wm.wp95, &p.wm.ep95, &p.wm.m03, &p.wm.ca, &p.wm.m17,
                                     &p.wm.cp95, &p.wm.ps, &p.wm.m13, &p.wm.csr, &p.wm.m10, &p.wm.m14, &p.wm.m15};
    for (auto* s : pola)
      for (auto& n : p.h) if (losI(g, 0, 10) == 0) s->insert(n.data);
  }
  return p;
}
