// SPDX-License-Identifier: GPL-3.0-or-later
#include "sypialnia.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "wspolne.h"

bool parsujSypialnie(const std::string& l, NocSypialni& n) {
  // `data;temp;wilg`, `data;temp;wilg;co2`, wersja 2 `…;co2;co2_ponad;co2_maks;co2_seria`
  std::vector<std::string> p;
  size_t a = 0;
  for (;;) {
    size_t b = l.find(';', a);
    p.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  if (!p.empty() && !p.back().empty() && p.back().back() == '\r') p.back().pop_back();
  if (p.size() != 3 && p.size() != 4 && p.size() != 7) return false;
  NocSypialni r;
  r.data = p[0];
  if (!dataOk(r.data)) return false;
  auto liczba = [](const std::string& x, float& out) {
    if (x.empty()) { out = NAN; return true; }
    char* e;
    out = strtof(x.c_str(), &e);
    return e != x.c_str() && *e == 0;
  };
  if (!liczba(p[1], r.t) || !liczba(p[2], r.w)) return false;
  if (p.size() >= 4 && !liczba(p[3], r.co2)) return false;
  if (p.size() == 7) {
    float a5 = NAN, a6 = NAN;
    if (!liczba(p[4], a5) || !liczba(p[5], a6)) return false;
    r.co2Ponad = std::isnan(a5) ? -1 : (int)a5;
    r.co2Maks = std::isnan(a6) ? -1 : (int)a6;
    for (char c : p[6])
      if (!(c == ',' || (c >= '0' && c <= '9'))) return false;
    r.co2Seria = p[6];
  }
  n = r;
  return true;
}

std::string polaCo2V2(const NocSypialni& n) {
  return ";" + (n.co2Ponad < 0 ? std::string() : std::to_string(n.co2Ponad)) + ";" +
         (n.co2Maks < 0 ? std::string() : std::to_string(n.co2Maks)) + ";" + n.co2Seria;
}

std::string zapiszSypialnie(const NocSypialni& n) {
  char b[48];
  auto pole = [](float v, char* o, size_t k) {
    if (std::isnan(v)) o[0] = 0;
    else snprintf(o, k, "%.1f", v);
  };
  char t[16], w[16];
  pole(n.t, t, sizeof t);
  pole(n.w, w, sizeof w);
  snprintf(b, sizeof b, "%s;%s;%s", n.data.c_str(), t, w);
  return b;
}

bool parsujCo2Nocy(const std::string& l, std::string& data, float& co2) {
  size_t a = l.find(';');
  if (a == std::string::npos || l.find(';', a + 1) != std::string::npos) return false;
  std::string d = l.substr(0, a), v = l.substr(a + 1);
  if (!v.empty() && v.back() == '\r') v.pop_back();
  if (!dataOk(d) || v.empty()) return false;
  char* e;
  float x = strtof(v.c_str(), &e);
  if (e == v.c_str() || *e || std::isnan(x) || x <= 0) return false;
  data = d;
  co2 = x;
  return true;
}

std::string zapiszCo2Nocy(const NocSypialni& n) {
  if (std::isnan(n.co2)) return "";
  return n.data + ";" + std::to_string((long)std::lround(n.co2));
}

bool oknoSypialni(int m) { return m >= 23 * 60 || m < 6 * 60; }

// Bufor na noc z datą poranka odczytu; inna noc: stary bufor bez zamknięcia przepada (np. panel spał do następnej nocy).
static bool buforNocy(BuforSypialni& b, const std::string& dzis, int minuta) {
  if (!dataOk(dzis) || !oknoSypialni(minuta)) return false;
  std::string noc = minuta >= 23 * 60 ? dodajDni(dzis, 1) : dzis;
  if (noc != b.data) {
    snprintf(b.data, sizeof b.data, "%s", noc.c_str());
    b.n = 0;
    b.nCo2 = 0;
  }
  return true;
}

void dodajOdczyt(BuforSypialni& b, const std::string& dzis, int minuta, float t, float w) {
  if (std::isnan(t) || std::isnan(w) || !buforNocy(b, dzis, minuta)) return;
  if (b.n >= BUFOR_SYPIALNI_ODCZYTOW) return;
  b.t10[b.n] = (int16_t)std::lround(t * 10);
  b.w10[b.n] = (int16_t)std::lround(w * 10);
  b.n++;
}

void dodajCo2(BuforSypialni& b, const std::string& dzis, int minuta, int co2) {
  if (co2 < 0 || !buforNocy(b, dzis, minuta) || b.nCo2 >= BUFOR_SYPIALNI_CO2) return;
  b.co2Min[b.nCo2] = (int16_t)(minuta >= 12 * 60 ? minuta - 12 * 60 : minuta + 12 * 60);
  b.co2[b.nCo2++] = (int16_t)co2;
}

void co2ZBufora(const BuforSypialni& b, NocSypialni& n) {
  n.co2Ponad = -1;
  n.co2Maks = -1;
  n.co2Seria.clear();
  if (b.nCo2 == 0) return;
  n.co2Ponad = 0;
  for (int i = 0; i < b.nCo2; i++) {
    n.co2Maks = std::max(n.co2Maks, (int)b.co2[i]);
    n.co2Seria += (i ? "," : "") + std::to_string(b.co2[i]);
    int dt = i + 1 < b.nCo2 ? b.co2Min[i + 1] - b.co2Min[i] : CO2_PROBKA_MAX_MIN;
    if (dt < 0) dt = 0;
    if (dt > CO2_PROBKA_MAX_MIN) dt = CO2_PROBKA_MAX_MIN;
    if (b.co2[i] > CO2_PROG_NOCY) n.co2Ponad += dt;
  }
}

bool zamknijNoc(BuforSypialni& b, const std::string& dzis, int minuta, NocSypialni& wynik) {
  if (!dataOk(dzis) || oknoSypialni(minuta) || !b.data[0] || std::string(b.data) > dzis) return false;
  bool jest = b.n > 0 || b.nCo2 > 0;
  if (jest) {
    std::vector<double> t, w, c;
    for (int i = 0; i < b.n; i++) {
      t.push_back(b.t10[i] / 10.0);
      w.push_back(b.w10[i] / 10.0);
    }
    for (int i = 0; i < b.nCo2; i++) c.push_back(b.co2[i]);
    wynik = NocSypialni();
    wynik.data = b.data;
    wynik.t = t.empty() ? NAN : (float)mediana(t);
    wynik.w = w.empty() ? NAN : (float)mediana(w);
    wynik.co2 = c.empty() ? NAN : (float)mediana(c);
    co2ZBufora(b, wynik);
  }
  b.data[0] = 0;
  b.n = 0;
  b.nCo2 = 0;
  return jest;
}

NormaSypialni liczNormeSypialni(const std::vector<NocSypialni>& s, const std::string& dzis,
                                const std::set<std::string>& wykluczone) {
  NormaSypialni nm;
  if (!dataOk(dzis)) return nm;
  std::string granica = dodajDni(dzis, -90);
  std::vector<const NocSypialni*> p;
  p.reserve(s.size());
  for (auto& r : s) p.push_back(&r);
  std::sort(p.begin(), p.end(), [](const NocSypialni* a, const NocSypialni* b) { return a->data > b->data; });
  std::vector<double> t, w, c;
  for (auto* r : p) {
    if (r->data >= dzis || r->data < granica || wykluczone.count(r->data)) continue;
    if (!std::isnan(r->t) && t.size() < 30) t.push_back(r->t);
    if (!std::isnan(r->w) && w.size() < 30) w.push_back(r->w);
    if (!std::isnan(r->co2) && c.size() < 30) c.push_back(r->co2);  // CO2: osobne 30 nocy z odczytem (4.4)
  }
  auto zbierz = [](const std::vector<double>& v) {
    NormaMetryki m;
    m.n = (int)v.size();
    if (m.n >= 14) {
      m.med = mediana(v);
      m.p10 = percentyl(v, 10);
      m.p90 = percentyl(v, 90);
    }
    return m;
  };
  nm.t = zbierz(t);
  nm.w = zbierz(w);
  nm.co2 = zbierz(c);
  return nm;
}

const NocSypialni* sypialniaZDnia(const std::vector<NocSypialni>& s, const std::string& data) {
  for (auto& r : s)
    if (r.data == data) return &r;
  return nullptr;
}

std::string stopkaCzujnika(float t, float w, int co2) {
  // bez odczytu czujnika panelu prawa część pusta, także przy CO2 (R-AW-13)
  if (std::isnan(t) || std::isnan(w)) return "";
  std::string s = "przy łóżku " + calkowita(t) + "° · " + calkowita(w) + "%";
  if (co2 >= 0) s += " · CO2 " + std::to_string(co2);
  return s;
}

std::string slowoSypialni(double v, const NormaMetryki& nm, bool& spokojne, const char* jednostka) {
  spokojne = false;
  if (!nm.jest()) return "uczę się";
  if (std::isnan(v)) return "–";
  if (v <= nm.p90 + 1e-6 && v >= nm.p10 - 1e-6) {
    spokojne = true;
    return "↔ jak zwykle";
  }
  std::string med = !jednostka[0]          ? calkowita(nm.med)
                    : jednostka[0] == '%' ? calkowita(nm.med) + "%"
                                          : przecinek1(nm.med) + jednostka;
  return std::string(v > nm.p90 ? "↑" : "↓") + " zwykle " + med;
}
