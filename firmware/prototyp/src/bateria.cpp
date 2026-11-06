// SPDX-License-Identifier: GPL-3.0-or-later
#include "bateria.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "wspolne.h"

bool parsujBaterie(const std::string& l0, WpisBaterii& w) {
  std::string l = l0;
  if (!l.empty() && l.back() == '\r') l.pop_back();
  std::vector<std::string> p;
  size_t a = 0;
  for (;;) {
    size_t b = l.find(';', a);
    p.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  if (p.size() != 5) return false;
  auto liczba = [](const std::string& s, long& v) {
    if (s.empty()) return false;
    char* e;
    v = strtol(s.c_str(), &e, 10);
    return *e == 0;
  };
  long ep, mv, wf, ak;
  if (!liczba(p[0], ep) || !liczba(p[1], mv) || !liczba(p[2], wf) || !liczba(p[3], ak)) return false;
  if (ep <= 0 || mv <= 0 || mv > 6000 || p[4].empty()) return false;
  w.ep = (uint32_t)ep;
  w.mV = (int)mv;
  w.wifiMs = wf;
  w.aktywnyMs = ak;
  w.powod = p[4];
  return true;
}

std::string zapiszBaterie(const WpisBaterii& w) {
  char b[192];  // pole powod do ok. 90 znaków (wszystkie powody z src/plan.h naraz)
  snprintf(b, sizeof b, "%lu;%d;%ld;%ld;%s", (unsigned long)w.ep, w.mV, w.wifiMs, w.aktywnyMs, w.powod.c_str());
  return b;
}

bool wymagaPrzyciecia(uint32_t najstarszy, uint32_t teraz) {
  return teraz >= 1700000000u && najstarszy && najstarszy + BAT_DNI_ZAPAS * 86400u < teraz;
}

bool przytnijBaterie(std::vector<WpisBaterii>& w, uint32_t teraz) {
  uint32_t najstarszy = 0;
  for (auto& x : w)
    if (!najstarszy || x.ep < najstarszy) najstarszy = x.ep;
  if (!wymagaPrzyciecia(najstarszy, teraz)) return false;
  uint32_t granica = teraz - BAT_DNI * 86400u;
  size_t n = w.size();
  w.erase(std::remove_if(w.begin(), w.end(), [&](const WpisBaterii& x) { return x.ep < granica; }), w.end());
  return w.size() != n;
}

TrendBaterii trendBaterii(std::vector<WpisBaterii> w) {
  TrendBaterii t;
  if (w.empty()) return t;
  std::stable_sort(w.begin(), w.end(), [](const WpisBaterii& a, const WpisBaterii& b) { return a.ep < b.ep; });
  size_t n = w.size();
  std::vector<int> m(n);  // mediana z 3 sąsiednich (na krańcach sam odczyt)
  for (size_t i = 0; i < n; i++) {
    if (i == 0 || i + 1 == n) { m[i] = w[i].mV; continue; }
    int a = w[i - 1].mV, b = w[i].mV, c = w[i + 1].mV;
    m[i] = std::max(std::min(a, b), std::min(std::max(a, b), c));
  }
  long lad = -1;  // ostatni wpis z wzrostem netto ≥ próg względem minimum z poprzednich wpisów
  for (size_t i = 1; i < n; i++) {
    int mn = m[i - 1];
    for (size_t k = i - 1, ile = 0; ile < (size_t)BAT_OKNO_LADOWANIA; k--, ile++) {
      mn = std::min(mn, m[k]);
      if (k == 0) break;
    }
    if (m[i] > m[i - 1] && m[i] - mn >= BAT_SKOK_LADOWANIA_MV) lad = (long)i;  // tylko w fazie wzrostu
  }
  // od ładowania (albo od początku) idziemy po napięciu niemalejącym; okno od pierwszego spadku
  size_t k = lad < 0 ? 0 : (size_t)lad;
  while (k + 1 < n && m[k + 1] >= m[k]) k++;
  size_t od;
  if (k + 1 >= n) od = lad < 0 ? 0 : n - 1;  // stałe napięcie bez ładowania: całe dane; po ładowaniu bez spadku: nic
  else if (lad < 0 && k == 0) od = 0;         // dane od razu w fazie spadku: od pierwszego wpisu
  else od = k + 1;                            // od pierwszego spadku po maksimum (ładowanie, stałe napięcie przy kablu)
  t.v = w.back().mV / 1000.0;
  t.n = (int)(n - od);
  double okno = (double)(w.back().ep - w[od].ep);
  t.godzinZbierania = okno / 3600.0;
  if (okno < BAT_MIN_OKNO_S || t.n < 3) return t;
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (size_t i = od; i < n; i++) {
    double x = (w[i].ep - w[od].ep) / 86400.0, y = m[i] / 1000.0;
    sx += x; sy += y; sxx += x * x; sxy += x * y;
  }
  double nn = t.n, mian = nn * sxx - sx * sx;
  if (mian <= 1e-12) return t;
  t.jest = true;
  t.vNaDobe = (nn * sxy - sx * sy) / mian;
  t.doby = okno / 86400.0;
  double nad = t.v - BAT_PROG_ROZLADOWANIA_MV / 1000.0;
  t.dniDoProgu = t.vNaDobe < -1e-4 ? (nad > 0 ? nad / -t.vNaDobe : 0) : -1;
  return t;
}

static std::string dwa(double v) {  // 2 miejsca po przecinku, minus U+2212
  char b[32];
  snprintf(b, sizeof b, "%.2f", std::fabs(v) < 0.005 ? 0.0 : v);
  std::string s = b;
  for (auto& c : s) if (c == '.') c = ',';
  if (!s.empty() && s[0] == '-') s = "\xE2\x88\x92" + s.substr(1);
  return s;
}

std::string liniaBaterii(const TrendBaterii& t, double vTeraz) {
  double v = !std::isnan(vTeraz) ? vTeraz : t.n > 0 ? t.v : NAN;
  std::string l = "Bateria " + (std::isnan(v) ? std::string("–") : dwa(v)) + " V";
  if (t.n == 0) return l;
  if (!t.jest) return l + " · zbieram dane (" + std::to_string((int)t.godzinZbierania) + " h)";
  l += " · " + dwa(t.vNaDobe) + " V/dobę z " + przecinek1(t.doby) + " doby";
  if (t.dniDoProgu < 0) return l + " · bez spadku";
  if (t.dniDoProgu > 999) return l + " · ok. 999+ dni";
  std::string d = calkowita(t.dniDoProgu);
  return l + " · ok. " + d + (d == "1" ? " dzień" : " dni");
}

std::string sredniaDoby(const std::vector<WpisBaterii>& w, uint32_t teraz) {
  double sw = 0, sa = 0;
  int nw = 0, na = 0;
  for (auto& x : w) {
    if (teraz < 1700000000u || x.ep + 86400u < teraz || x.ep > teraz + 600) continue;
    if (x.wifiMs >= 0) { sw += (double)x.wifiMs; nw++; }  // jawnie: -Wconversion
    if (x.aktywnyMs >= 0) { sa += (double)x.aktywnyMs; na++; }
  }
  if (!nw && !na) return "";
  std::string l = "Wi-Fi śr. " + (nw ? przecinek1(sw / nw / 1000.0) : std::string("–")) + " s";
  l += " · aktywny " + (na ? calkowita(sa / na / 1000.0) : std::string("–")) + " s";
  return l;
}

// ---------- M-25 ----------
const PunktKrzywej KRZYWA_BATERII[] = {
    {4150, 100}, {4110, 90}, {4020, 80}, {3950, 70}, {3870, 60}, {3840, 50}, {3800, 40},
    {3770, 30},  {3730, 20}, {3710, 15}, {3690, 10}, {3610, 5},  {3300, 0},
};
const int KRZYWA_BATERII_N = (int)(sizeof KRZYWA_BATERII / sizeof KRZYWA_BATERII[0]);

int procentBaterii(int mV) {
  if (mV < BAT_MV_MIN || mV > BAT_MV_MAX) return -1;
  if (mV >= KRZYWA_BATERII[0].mV) return 100;
  for (int i = 1; i < KRZYWA_BATERII_N; i++) {
    const PunktKrzywej& g = KRZYWA_BATERII[i - 1];
    const PunktKrzywej& d = KRZYWA_BATERII[i];
    if (mV >= d.mV) {
      double t = (double)(mV - d.mV) / (double)(g.mV - d.mV);
      return (int)std::lround(d.proc + t * (g.proc - d.proc));
    }
  }
  return 0;
}

int wygladzProcent(StanProcentuBaterii& s, int surowy) {
  if (surowy < 0 || surowy > 100) {  // chwilowy brak odczytu, E-05 bez zmian przez jedno wybudzenie
    bool drugiBrak = s.surowy == BAT_SUROWY_BRAK;
    s.surowy = BAT_SUROWY_BRAK;
    return !drugiBrak && s.proc >= 0 && s.proc <= 100 ? s.proc : -1;
  }
  int p = s.proc, poprz = s.surowy;
  s.surowy = (int8_t)surowy;
  if (p < 0 || p > 100) {  // pierwszy pomiar od startu zasilania
    s.proc = (int8_t)surowy;
    return surowy;
  }
  if (surowy <= p && surowy >= p - BAT_PROC_SZUM) p = surowy;  // mały spadek od razu
  else if (surowy < p - BAT_PROC_SZUM && poprz >= 0 && poprz < p - BAT_PROC_SZUM) p = std::max(surowy, poprz);
  else if (surowy > p + BAT_PROC_SZUM && poprz > p + BAT_PROC_SZUM) p = std::min(surowy, poprz);
  s.proc = (int8_t)p;
  return p;
}
