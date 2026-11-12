// SPDX-License-Identifier: GPL-3.0-or-later
// Slot dzienny: opis w src/slot_dzienny.h (spec 5.8.12, ADR-0027). Bez wyjątków i RTTI (liczby przez strtod).
#include "slot_dzienny.h"
#include "noc_pliki.h"
#include "miasta.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "ekrany.h"
#include "wspolne.h"

const char* const PYLEK_DOPELNIACZ[PYL_N] = {"brzozy", "traw", "olchy", "bylicy"};
const char* const NAZWY_ELEMENTU[EL_N] = {"maska", "filtr"};

namespace {

const double PI_ = 3.14159265358979323846;

std::string liczba4(double v) {
  char b[32];
  snprintf(b, sizeof b, "%.4f", v);
  return b;
}

// ---------- minimalny czytnik JSON Open-Meteo (tylko to, czego potrzebuje parser) ----------
// Pozycja tablicy `"klucz":[` w bloku `hourly` (po `"hourly":{`, bo `hourly_units` ma te same klucze bez tablic).
size_t tablica(const std::string& j, size_t od, const char* klucz) {
  std::string k = std::string("\"") + klucz + "\"";
  size_t p = od;
  for (;;) {
    p = j.find(k, p);
    if (p == std::string::npos) return p;
    size_t q = p + k.size();
    while (q < j.size() && (j[q] == ' ' || j[q] == '\n' || j[q] == '\r' || j[q] == '\t')) q++;
    if (q < j.size() && j[q] == ':') {
      q++;
      while (q < j.size() && (j[q] == ' ' || j[q] == '\n' || j[q] == '\r' || j[q] == '\t')) q++;
      if (q < j.size() && j[q] == '[') return q + 1;
    }
    p = q;
  }
}

// Elementy tablicy od `p` (po `[`) do `]`: teksty bez cudzysłowów, liczby jako tekst, `null` jako "".
bool elementy(const std::string& j, size_t p, std::vector<std::string>& w, size_t maks = 400) {
  w.clear();
  while (p < j.size()) {
    char c = j[p];
    if (c == ']') return true;
    if (c == ',' || c == ' ' || c == '\n' || c == '\r' || c == '\t') { p++; continue; }
    if (w.size() >= maks) return false;
    if (c == '"') {
      size_t q = j.find('"', p + 1);
      if (q == std::string::npos) return false;
      w.push_back(j.substr(p + 1, q - p - 1));
      p = q + 1;
      continue;
    }
    size_t q = p;
    while (q < j.size() && j[q] != ',' && j[q] != ']' && j[q] != ' ' && j[q] != '\n') q++;
    std::string t = j.substr(p, q - p);
    w.push_back(t == "null" ? std::string() : t);
    p = q;
  }
  return false;
}

float liczbaZ(const std::string& t, float min, float max) {
  if (t.empty()) return NAN;
  char* k = nullptr;
  double v = strtod(t.c_str(), &k);
  if (!k || *k != 0 || std::isnan(v) || v < min || v > max) return NAN;
  return (float)v;
}

// indeks godziny 0…47 względem `data` z `RRRR-MM-DDTHH:MM`; -1 poza zakresem
int indeksGodziny(const std::string& data, const std::string& t) {
  if (t.size() < 13 || t[10] != 'T') return -1;
  std::string d = t.substr(0, 10);
  if (!dataOk(d)) return -1;
  int g = 0;
  if (!indeksZTekstu(t.substr(11, 2).c_str(), 24, g)) return -1;
  int dn = dniMiedzy(data, d);
  if (dn < 0 || dn > 1) return -1;
  return dn * 24 + g;
}

size_t blokHourly(const std::string& j) {
  size_t p = j.find("\"hourly\"");
  while (p != std::string::npos) {
    size_t q = p + 8;
    while (q < j.size() && (j[q] == ' ' || j[q] == ':')) q++;
    if (q < j.size() && j[q] == '{') return q;
    p = j.find("\"hourly\"", p + 1);
  }
  return std::string::npos;
}

bool czytajPola(const std::string& j, Prognoza& p, bool ustalDate,
                const std::vector<std::pair<const char*, float GodzinaPrognozy::*>>& pola, bool pylki) {
  size_t h = blokHourly(j);
  if (h == std::string::npos) return false;
  size_t tp = tablica(j, h, "time");
  if (tp == std::string::npos) return false;
  std::vector<std::string> czasy;
  if (!elementy(j, tp, czasy) || czasy.empty()) return false;
  if (ustalDate) {
    std::string d = czasy[0].substr(0, 10);
    if (!dataOk(d)) return false;
    p.data = d;
    p.h.assign(PROG_GODZIN, GodzinaPrognozy());
  }
  if (p.data.empty()) return false;
  std::vector<std::string> w;
  for (auto& pole : pola) {
    size_t ap = tablica(j, h, pole.first);
    if (ap == std::string::npos || !elementy(j, ap, w)) continue;  // brak pola = brak wartości
    float lo = -80, hi = 2000;
    for (size_t i = 0; i < czasy.size() && i < w.size(); i++) {
      int g = indeksGodziny(p.data, czasy[i]);
      if (g < 0) continue;
      p.h[g].*(pole.second) = liczbaZ(w[i], lo, hi);
    }
  }
  if (pylki) {
    const char* nazwy[PYL_N] = {"birch_pollen", "grass_pollen", "alder_pollen", "mugwort_pollen"};
    for (int k = 0; k < PYL_N; k++) {
      size_t ap = tablica(j, h, nazwy[k]);
      if (ap == std::string::npos || !elementy(j, ap, w)) continue;
      for (size_t i = 0; i < czasy.size() && i < w.size(); i++) {
        int g = indeksGodziny(p.data, czasy[i]);
        if (g >= 0) p.h[g].pylek[k] = liczbaZ(w[i], 0, 100000);
      }
    }
  }
  return true;
}

std::string pole(float v) {
  if (std::isnan(v)) return "";
  char b[24];
  snprintf(b, sizeof b, "%.1f", v);
  return b;
}

std::vector<std::string> podziel(const std::string& s, char sep) {
  std::vector<std::string> w;
  size_t a = 0;
  for (;;) {
    size_t b = s.find(sep, a);
    w.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) return w;
    a = b + 1;
  }
}

std::string stopnie(float t) {  // `−4°`, `21°` (minus typograficzny U+2212)
  if (std::isnan(t)) return "–";
  long v = std::lround(t);
  if (v == 0) return "0°";
  return (v < 0 ? std::string("−") + std::to_string(-v) : std::to_string(v)) + "°";
}

}  // namespace

// ---------- prognoza ----------
const GodzinaPrognozy* Prognoza::godzina(const std::string& dzien, int g) const {
  if (data.empty() || g < 0 || g > 23) return nullptr;
  int dn = dniMiedzy(data, dzien);
  if (dn < 0 || dn > 1) return nullptr;
  return &h[dn * 24 + g];
}

std::string urlPrognozy(double lat, double lon) {
  return "http://api.open-meteo.com/v1/forecast?latitude=" + liczba4(lat) + "&longitude=" + liczba4(lon) +
         "&hourly=temperature_2m,precipitation_probability,cloud_cover,wind_gusts_10m&timezone=Europe%2FWarsaw"
         "&forecast_days=2";
}
std::string urlPowietrza(double lat, double lon) {
  return "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + liczba4(lat) + "&longitude=" +
         liczba4(lon) + "&hourly=pm2_5,birch_pollen,grass_pollen,alder_pollen,mugwort_pollen"
         "&timezone=Europe%2FWarsaw&forecast_days=2";
}
std::string urlGeokodowania(const std::string& miasto) {
  std::string e;  // kodowanie procentowe UTF-8 (polskie znaki w nazwie miasta)
  const char* hex = "0123456789ABCDEF";
  for (unsigned char c : miasto) {
    if (isalnum(c) || c == '-' || c == '.' || c == '_') e += (char)c;
    else if (c == ' ') e += "%20";
    else { e += '%'; e += hex[c >> 4]; e += hex[c & 15]; }
  }
  return "http://geocoding-api.open-meteo.com/v1/search?name=" + e + "&count=1&language=pl&format=json";
}

bool parsujPrognoze(const std::string& json, Prognoza& p) {
  return czytajPola(json, p, true,
                    {{"temperature_2m", &GodzinaPrognozy::temp},
                     {"precipitation_probability", &GodzinaPrognozy::opad},
                     {"cloud_cover", &GodzinaPrognozy::chmury},
                     {"wind_gusts_10m", &GodzinaPrognozy::porywy}},
                    false);
}
bool parsujPowietrze(const std::string& json, Prognoza& p) {
  return czytajPola(json, p, false, {{"pm2_5", &GodzinaPrognozy::pm25}}, true);
}

bool parsujGeokodowanie(const std::string& json, double& lat, double& lon) {
  size_t r = json.find("\"results\"");
  if (r == std::string::npos) return false;
  auto liczbaPola = [&](const char* k, double& v) {
    size_t p = json.find(std::string("\"") + k + "\"", r);
    if (p == std::string::npos) return false;
    p = json.find(':', p);
    if (p == std::string::npos) return false;
    char* e = nullptr;
    v = strtod(json.c_str() + p + 1, &e);
    return e && e != json.c_str() + p + 1;
  };
  double a = 0, b = 0;
  if (!liczbaPola("latitude", a) || !liczbaPola("longitude", b)) return false;
  if (a < -90 || a > 90 || b < -180 || b > 180) return false;
  lat = a;
  lon = b;
  return true;
}

std::vector<std::string> wierszePrognozy(const Prognoza& p) {
  std::vector<std::string> w;
  if (!p.jest()) return w;
  for (int i = 0; i < PROG_GODZIN; i++) {
    const GodzinaPrognozy& g = p.h[(size_t)i];
    std::string s = (i < 24 ? p.data : dodajDni(p.data, 1)) + ";" + std::to_string(i % 24) + ";" + pole(g.temp) + ";" +
                    pole(g.opad) + ";" + pole(g.chmury) + ";" + pole(g.porywy) + ";" + pole(g.pm25);
    for (int k = 0; k < PYL_N; k++) s += ";" + pole(g.pylek[k]);
    w.push_back(s + ";" + std::to_string(p.pobrano));
  }
  return w;
}

bool prognozaZWierszy(const std::vector<std::string>& w, Prognoza& p) {
  p = Prognoza();
  if (w.size() != (size_t)PROG_GODZIN) return false;
  Prognoza q;
  for (int i = 0; i < PROG_GODZIN; i++) {
    std::vector<std::string> f = podziel(w[(size_t)i], ';');
    if (f.size() != 12 || !dataOk(f[0])) return false;
    if (i == 0) {
      q.data = f[0];
      char* e = nullptr;
      q.pobrano = (uint32_t)strtoul(f[11].c_str(), &e, 10);
    }
    int g = 0;
    if (!indeksZTekstu(f[1].c_str(), 24, g) || g != i % 24 || f[0] != (i < 24 ? q.data : dodajDni(q.data, 1))) return false;
    GodzinaPrognozy& x = q.h[(size_t)i];
    x.temp = liczbaZ(f[2], -80, 70);
    x.opad = liczbaZ(f[3], 0, 100);
    x.chmury = liczbaZ(f[4], 0, 100);
    x.porywy = liczbaZ(f[5], 0, 400);
    x.pm25 = liczbaZ(f[6], 0, 2000);
    for (int k = 0; k < PYL_N; k++) x.pylek[k] = liczbaZ(f[7 + (size_t)k], 0, 100000);
  }
  p = q;
  return true;
}

bool prognozaWazna(const Prognoza& p, uint32_t teraz) {
  if (!p.jest() || p.pobrano == 0 || teraz < p.pobrano) return false;
  return teraz - p.pobrano <= PROGNOZA_WAZNA_S;
}

// ---------- reguła wietrzenia ----------
bool PylkiDnia::jakikolwiek() const {
  for (int k = 0; k < PYL_N; k++)
    if (zaznaczony[k]) return true;
  return false;
}

static bool pylekWysoko(const GodzinaPrognozy& g, const PylkiDnia& p, int k) {
  return p.zaznaczony[k] && !std::isnan(p.prog[k]) && !std::isnan(g.pylek[k]) && g.pylek[k] >= p.prog[k];
}

bool godzinaCzysta(const GodzinaPrognozy& g, const PylkiDnia& pylki) {
  if (std::isnan(g.pm25) || std::isnan(g.temp)) return false;  // godzina bez danych nie jest czysta (5.8.10)
  if (g.pm25 > WIETRZ_PM25_MAX) return false;
  for (int k = 0; k < PYL_N; k++)
    if (pylekWysoko(g, pylki, k)) return false;  // R-KON-45; pyłek bez prognozy (poza sezonem) nie brudzi godziny
  return true;
}

namespace {
struct Okno {
  int od = -1, do_ = -1;
};
// Okno w godzinach [a, b) według trybu; tylko godziny z prognozy dnia `dzis`.
Okno oknoTrybu(const Prognoza& p, const std::string& dzis, int a, int b, TrybWietrzenia tryb, float przyLozku,
               const PylkiDnia& pylki) {
  Okno o;
  auto g = [&](int h) { return p.godzina(dzis, h); };
  auto czysta = [&](int h) { const GodzinaPrognozy* x = g(h); return x && godzinaCzysta(*x, pylki); };
  if (tryb == TrybWietrzenia::ZIMA) {
    int hm = -1;
    for (int h = a; h < b; h++)
      if (czysta(h) && (hm < 0 || g(h)->temp > g(hm)->temp)) hm = h;
    if (hm < 0) return o;
    float prog = g(hm)->temp - WIETRZ_ZIMA_PASMO;
    int l = hm, r = hm;
    while (l - 1 >= a && czysta(l - 1) && g(l - 1)->temp >= prog) l--;
    while (r + 1 < b && czysta(r + 1) && g(r + 1)->temp >= prog) r++;
    o.od = l;
    o.do_ = r + 1;
  } else if (tryb == TrybWietrzenia::LATO) {
    auto pasuje = [&](int h) { return czysta(h) && g(h)->temp <= przyLozku - WIETRZ_LATO_ROZNICA; };
    for (int h = a; h < b; h++)
      if (pasuje(h)) {
        int r = h;
        while (r + 1 < b && pasuje(r + 1)) r++;
        o.od = h;
        o.do_ = r + 1;
        break;
      }
  }
  return o;
}
}  // namespace

WynikWietrzenia regulaWietrzenia(const WejscieWietrzenia& w) {
  WynikWietrzenia r;
  if (!w.prognoza || !w.prognoza->jest() || w.godzina < 0 || w.godzina >= WIETRZ_HORYZONT_DO) return r;
  const Prognoza& p = *w.prognoza;
  int a = w.godzina, b = WIETRZ_HORYZONT_DO;
  for (int h = a; h < b; h++) {
    const GodzinaPrognozy* g = p.godzina(w.dzis, h);
    r.pm25.push_back(g ? g->pm25 : NAN);
    r.temp.push_back(g ? g->temp : NAN);
    r.czyste.push_back(g && godzinaCzysta(*g, w.pylki));
  }
  const GodzinaPrognozy* teraz = p.godzina(w.dzis, a);
  r.pm25Teraz = teraz ? teraz->pm25 : NAN;
  bool terazCzysta = !r.czyste.empty() && r.czyste[0];
  // wariant CO2 przed każdym innym (R-KON-44); działa też bez trybu i bez czujnika panelu
  if (w.co2 >= CO2_PROG_TERAZ && terazCzysta) {
    r.wariant = Wietrzenie::CO2;
    return r;
  }
  if (std::isnan(w.przyLozku)) return r;  // bez czujnika panelu reguła nie zna temperatury w domu (5.8.10)
  float maxZa = NAN;
  for (float t : r.temp)
    if (!std::isnan(t) && (std::isnan(maxZa) || t > maxZa)) maxZa = t;
  bool lato = false;
  if (w.przyLozku >= WIETRZ_LATO_OD)
    for (int h = a; h < b; h++) {
      const GodzinaPrognozy* g = p.godzina(w.dzis, h);
      if (g && godzinaCzysta(*g, w.pylki) && g->temp <= w.przyLozku - WIETRZ_LATO_ROZNICA) lato = true;
    }
  bool upal = w.przyLozku >= WIETRZ_LATO_OD && teraz && !std::isnan(teraz->temp) && teraz->temp >= w.przyLozku;
  bool zima = !std::isnan(maxZa) && w.przyLozku - maxZa >= WIETRZ_ZIMA_ROZNICA;
  // tryb: przy łóżku ≥ 24 °C najpierw letni (chłodzenie), inaczej zimowy; pora roku nie jest wejściem (5.8.12)
  r.tryb = (lato || upal) ? TrybWietrzenia::LATO : zima ? TrybWietrzenia::ZIMA : TrybWietrzenia::BRAK;
  if (r.tryb == TrybWietrzenia::BRAK) return r;
  Okno o = oknoTrybu(p, w.dzis, a, b, r.tryb, w.przyLozku, w.pylki);
  if (upal && !(o.od == a)) {
    r.wariant = o.od >= 0 ? Wietrzenie::UPAL_DO : Wietrzenie::UPAL_CALY_DZIEN;
    r.od = o.od;
    r.do_ = o.do_;
    return r;
  }
  if (o.od < 0) {
    // okno minęło (było rano w tym trybie) → bez wskazania do końca slotu; inaczej smog cały dzień
    Okno rano = oknoTrybu(p, w.dzis, 6, a, r.tryb, w.przyLozku, w.pylki);
    // `Dziś bez wietrzenia.` mówi o smogu, więc tylko przy zmierzonym smogu w horyzoncie; godziny bez PM2.5 (brak
    // odpowiedzi serwisu jakości powietrza) albo nieczyste tylko przez pyłek nie są smogiem: wtedy S-K-33 milczy,
    // a pyłek pokazuje S-K-36
    bool smog = false;
    for (float x : r.pm25)
      if (!std::isnan(x) && x > WIETRZ_PM25_MAX) smog = true;
    r.wariant = rano.od >= 0 || !smog ? Wietrzenie::BRAK : Wietrzenie::SMOG;
    if (r.wariant == Wietrzenie::BRAK) r.tryb = TrybWietrzenia::BRAK;
    return r;
  }
  r.od = o.od;
  r.do_ = o.do_;
  if (o.od == a) {
    r.wariant = Wietrzenie::TRWA;
    const GodzinaPrognozy* po = p.godzina(w.dzis, o.do_);
    if (o.do_ >= b) r.powod = PowodOkna::CZYSTO_DO_WIECZORA;
    else if (!po || !godzinaCzysta(*po, w.pylki)) r.powod = PowodOkna::SMOG;
    else r.powod = r.tryb == TrybWietrzenia::ZIMA ? PowodOkna::ZIMNIEJ : PowodOkna::CIEPLEJ;
  } else {
    r.wariant = Wietrzenie::POZNIEJ;
    bool smogTeraz = teraz && !std::isnan(teraz->pm25) && teraz->pm25 > WIETRZ_PM25_MAX;
    r.powod = smogTeraz ? PowodOkna::TERAZ_SMOG
                        : (r.tryb == TrybWietrzenia::ZIMA ? PowodOkna::NAJCIEPLEJ : PowodOkna::CHLODNIEJ);
  }
  return r;
}

// ---------- S-K-34 ----------
WynikJutra jutroRano(const Prognoza& p, const std::string& dzis) {
  WynikJutra r;
  std::string jutro = dodajDni(dzis, 1);
  bool cos = false;
  for (int h = JUTRO_OD; h < JUTRO_DO; h++) {
    const GodzinaPrognozy* g = p.godzina(jutro, h);
    if (!g) continue;
    if (!std::isnan(g->temp) && (std::isnan(r.tMin) || g->temp < r.tMin)) { r.tMin = g->temp; r.godzinaTMin = h; }
    if (!std::isnan(g->opad) && (std::isnan(r.opadMax) || g->opad > r.opadMax)) r.opadMax = g->opad;
    if (!std::isnan(g->porywy) && (std::isnan(r.porywyMax) || g->porywy > r.porywyMax)) r.porywyMax = g->porywy;
    cos = true;
  }
  if (!cos) return r;
  bool mroz = !std::isnan(r.tMin) && r.tMin <= 0;
  if (mroz && !std::isnan(r.opadMax) && r.opadMax >= JUTRO_SLISKO_OPAD) r.wariant = Jutro::SLISKO;
  else if (mroz) r.wariant = Jutro::MROZ;
  else if (!std::isnan(r.opadMax) && r.opadMax >= JUTRO_DESZCZ_OPAD) r.wariant = Jutro::DESZCZ;
  else if (!std::isnan(r.porywyMax) && r.porywyMax >= JUTRO_WIATR) r.wariant = Jutro::WIATR;
  return r;
}

// ---------- S-K-35 ----------
std::string terminWymiany(const ElementWymiany& e) {
  if (!dataOk(e.wymieniono) || e.okres <= 0) return "";
  if (e.tygodnie) return dodajDni(e.wymieniono, 7 * e.okres);
  int r = liczbaCalk(e.wymieniono.substr(0, 4).c_str()), m = liczbaCalk(e.wymieniono.substr(5, 2).c_str()),
      d = liczbaCalk(e.wymieniono.substr(8, 2).c_str());
  m += e.okres;
  r += (m - 1) / 12;
  m = (m - 1) % 12 + 1;
  static const int DNI[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int max = DNI[m - 1] + ((m == 2 && ((r % 4 == 0 && r % 100 != 0) || r % 400 == 0)) ? 1 : 0);
  if (d > max) d = max;
  char b[16];
  snprintf(b, sizeof b, "%04d-%02d-%02d", r, m, d);
  return b;
}

std::string wiekElementu(const std::string& od, const std::string& dzis) {
  if (!dataOk(od) || !dataOk(dzis) || dniMiedzy(od, dzis) < 0) return "–";
  int r1 = liczbaCalk(od.substr(0, 4).c_str()), m1 = liczbaCalk(od.substr(5, 2).c_str()), d1 = liczbaCalk(od.substr(8, 2).c_str());
  int r2 = liczbaCalk(dzis.substr(0, 4).c_str()), m2 = liczbaCalk(dzis.substr(5, 2).c_str()), d2 = liczbaCalk(dzis.substr(8, 2).c_str());
  // ostatni dzień krótszego miesiąca liczy się jak dzień wymiany (termin przycięty w terminWymiany)
  bool ostatni = dodajDni(dzis, 1).substr(5, 2) != dzis.substr(5, 2);
  int mies = (r2 - r1) * 12 + (m2 - m1) - (d2 < d1 && !ostatni ? 1 : 0);
  if (mies >= 1) return std::to_string(mies) + " mies.";
  return std::to_string(dniMiedzy(od, dzis) / 7) + " tyg.";
}

std::string wiekElementu(const ElementWymiany& e, const std::string& dzis) {
  if (!e.tygodnie) return wiekElementu(e.wymieniono, dzis);
  if (!dataOk(e.wymieniono) || !dataOk(dzis) || dniMiedzy(e.wymieniono, dzis) < 0) return "–";
  return std::to_string(dniMiedzy(e.wymieniono, dzis) / 7) + " tyg.";
}

std::string okresElementuTekst(const ElementWymiany& e) {
  return std::to_string(e.okres) + (e.tygodnie ? " tyg." : " mies.");
}

WynikWymiany wymianaSprzetu(const ElementWymiany (&el)[EL_N], const std::string& dzis, const std::string& ost) {
  WynikWymiany r;
  if (!dataOk(dzis)) return r;
  if (dataOk(ost) && ost != dzis && dniMiedzy(ost, dzis) >= 0 && dniMiedzy(ost, dzis) < WYMIANA_CO_DNI) return r;
  for (int i = 0; i < EL_N; i++) {
    std::string t = terminWymiany(el[i]);
    if (t.empty()) continue;  // bez zapisanej daty: brak S-K-35 (R-MAS-06)
    if (dniMiedzy(dzis, t) > WYMIANA_PRZED_DNI) continue;
    if (r.element < 0 || t < r.termin) {
      r.element = i;
      r.termin = t;
      r.poTerminie = dniMiedzy(dzis, t) < 0;
    }
  }
  return r;
}

// ---------- S-K-36 ----------
WynikPylku pylekDnia(const Prognoza& p, const std::string& dzis, int godzina, const PylkiDnia& pylki) {
  WynikPylku r;
  if (!pylki.jakikolwiek() || godzina < 0 || godzina >= WIETRZ_HORYZONT_DO) return r;
  double najl = 0;
  for (int k = 0; k < PYL_N; k++) {
    if (!pylki.zaznaczony[k] || std::isnan(pylki.prog[k]) || pylki.prog[k] <= 0) continue;
    for (int h = godzina; h < WIETRZ_HORYZONT_DO; h++) {
      const GodzinaPrognozy* g = p.godzina(dzis, h);
      if (g && pylekWysoko(*g, pylki, k) && g->pylek[k] / pylki.prog[k] > najl) {
        najl = g->pylek[k] / pylki.prog[k];
        r.rodzaj = k;
      }
    }
  }
  if (r.rodzaj < 0) return r;
  for (int h = godzina; h < WIETRZ_HORYZONT_DO; h++) {
    const GodzinaPrognozy* g = p.godzina(dzis, h);
    bool wys = g && pylekWysoko(*g, pylki, r.rodzaj);
    r.wartosci.push_back(g ? g->pylek[r.rodzaj] : NAN);
    r.wysoko.push_back(wys);
    if (wys) {
      if (r.od < 0) r.od = h;
      r.do_ = h + 1;
    }
  }
  return r;
}

// ---------- S-K-37: wschód i zachód (NOAA / równanie wschodu słońca) ----------
int przesuniecieStrefy(const std::string& data) {
  if (!dataOk(data)) return 60;
  int r = liczbaCalk(data.substr(0, 4).c_str());
  auto ostatniaNiedziela = [&](int m) {  // dzień miesiąca ostatniej niedzieli
    char b[40];
    snprintf(b, sizeof b, "%04d-%02d-31", r, m);
    int dt = dzienTygodnia(b);
    return 31 - dt;
  };
  char od[40], do_[40];
  snprintf(od, sizeof od, "%04d-03-%02d", r, ostatniaNiedziela(3));
  snprintf(do_, sizeof do_, "%04d-10-%02d", r, ostatniaNiedziela(10));
  return (data >= std::string(od) && data < std::string(do_)) ? 120 : 60;
}

// Równania arkusza NOAA Solar Calculator (Meeus): wiek juliański, długość i anomalia średnia, równanie środka,
// długość pozorna, nachylenie z poprawką, deklinacja, równanie czasu; kąt godzinny dla 90,833° (refrakcja i promień
// tarczy). Dwa przebiegi: najpierw w południe lokalne, potem w przybliżonej chwili wschodu albo zachodu
// (wcześniejsze uproszczone równanie wschodu słońca myliło się latem o ponad 2 min).
static bool chwilaSlonca(long n0, double lat, double lon, double off, bool wschod, double minuta, double& wynik) {
  const double R = PI_ / 180;
  double jd = (double)n0 + 2440587.5 + (minuta - off) / 1440.0;
  double jc = (jd - 2451545.0) / 36525.0;
  double L0 = std::fmod(280.46646 + jc * (36000.76983 + jc * 0.0003032), 360.0);
  double M = 357.52911 + jc * (35999.05029 - 0.0001537 * jc);
  double e = 0.016708634 - jc * (0.000042037 + 0.0000001267 * jc);
  double C = std::sin(M * R) * (1.914602 - jc * (0.004817 + 0.000014 * jc)) + std::sin(2 * M * R) * (0.019993 - 0.000101 * jc) +
             std::sin(3 * M * R) * 0.000289;
  double om = 125.04 - 1934.136 * jc;
  double lam = L0 + C - 0.00569 - 0.00478 * std::sin(om * R);
  double eps0 = 23 + (26 + (21.448 - jc * (46.815 + jc * (0.00059 - jc * 0.001813))) / 60) / 60;
  double eps = eps0 + 0.00256 * std::cos(om * R);
  double dek = std::asin(std::sin(eps * R) * std::sin(lam * R));
  double y = std::tan(eps * R / 2) * std::tan(eps * R / 2);
  double eqt = 4 / R * (y * std::sin(2 * L0 * R) - 2 * e * std::sin(M * R) + 4 * e * y * std::sin(M * R) * std::cos(2 * L0 * R) -
                        0.5 * y * y * std::sin(4 * L0 * R) - 1.25 * e * e * std::sin(2 * M * R));
  double cosH = std::cos(90.833 * R) / (std::cos(lat * R) * std::cos(dek)) - std::tan(lat * R) * std::tan(dek);
  if (cosH < -1 || cosH > 1) return false;
  double H = std::acos(cosH) / R;
  double poludnie = 720 - 4 * lon - eqt + off;
  wynik = poludnie + (wschod ? -4 * H : 4 * H);
  return true;
}

static bool sloncePrecyzyjnie(const std::string& data, double lat, double lon, double& wschod, double& zachod) {
  long n0 = nrDnia(data);
  if (n0 < 0) return false;
  double off = przesuniecieStrefy(data);
  double w = 0, z = 0;
  if (!chwilaSlonca(n0, lat, lon, off, true, 720, w) || !chwilaSlonca(n0, lat, lon, off, false, 720, z)) return false;
  if (!chwilaSlonca(n0, lat, lon, off, true, w, w) || !chwilaSlonca(n0, lat, lon, off, false, z, z)) return false;
  wschod = w;
  zachod = z;
  return true;
}

bool slonce(const std::string& data, double lat, double lon, int& wschod, int& zachod) {
  double a, b;
  if (!sloncePrecyzyjnie(data, lat, lon, a, b)) return false;
  wschod = (int)std::lround(a);
  zachod = (int)std::lround(b);
  return true;
}

WynikSlonca dzienINoc(const std::string& dzis, double lat, double lon) {
  WynikSlonca r;
  double w0, z0, w1, z1, w2, z2, w7, z7;
  if (std::isnan(lat) || std::isnan(lon) || !sloncePrecyzyjnie(dzis, lat, lon, w0, z0)) return r;
  if (!sloncePrecyzyjnie(dodajDni(dzis, -1), lat, lon, w1, z1) ||
      !sloncePrecyzyjnie(dodajDni(dzis, -2), lat, lon, w2, z2) ||
      !sloncePrecyzyjnie(dodajDni(dzis, -7), lat, lon, w7, z7))
    return r;
  r.jest = true;
  r.wschod = (int)std::lround(w0);
  r.zachod = (int)std::lround(z0);
  double d0 = z0 - w0, d1 = z1 - w1, d2 = z2 - w2, d7 = z7 - w7;
  r.dlugosc = (int)std::lround(d0);
  r.zmiana = (int)std::lround(d0 - d7);
  if (d0 > d1 && d1 <= d2) r.powod = "Najkrótszy dzień za nami.";
  else if (d0 < d1 && d1 >= d2) r.powod = "Najdłuższy dzień za nami.";
  else if (std::fabs(d0 - d7) < 1.0) r.powod = "Dzień tak długi jak tydzień temu.";
  else if (d0 > d7) r.powod = "Dzień dłuższy o " + std::to_string(std::abs(r.zmiana)) + "\xC2\xA0min niż tydzień temu.";
  else r.powod = "Dzień krótszy o " + std::to_string(std::abs(r.zmiana)) + "\xC2\xA0min niż tydzień temu.";
  return r;
}

// ---------- drzemka ----------
Drzemka liczDrzemke(const std::vector<SesjaDoby>& s, uint32_t koniecNocy) {
  Drzemka d;
  for (auto& x : s) {
    if (x.od <= koniecNocy || x.do_ <= x.od) continue;
    if ((int)(x.do_ - x.od) < DRZEMKA_MIN_S) continue;
    d.minut += (int)((x.do_ - x.od) / 60);
    if (!std::isnan(x.wyciekP95) && (std::isnan(d.wyciekP95) || x.wyciekP95 > d.wyciekP95)) d.wyciekP95 = x.wyciekP95;
    if (d.od == 0 || x.od < d.od) d.od = x.od;
    if (x.do_ > d.do_) d.do_ = x.do_;
  }
  return d;
}
bool wyciekDrzemki(const Drzemka& d) { return d.minut > 0 && !std::isnan(d.wyciekP95) && d.wyciekP95 > WYCIEK_PROG_SK10; }

uint32_t chwilaZCywilnego(long long cyw) {
  if (cyw <= 0) return 0;
  time_t t = (time_t)cyw;
  std::tm g{};
  if (!gmtime_r(&t, &g)) return 0;
  g.tm_isdst = -1;  // kalendarz aparatu = czas lokalny strefy panelu (Europe/Warsaw, R-NF-20)
  time_t w = mktime(&g);
  return w > 0 ? (uint32_t)w : 0;
}

bool sesjeZRaportu(const std::string& j, std::vector<SesjaDoby>& s, float& p95) {
  s.clear();
  p95 = NAN;
  size_t a = j.find("\"sessions\"");
  if (a == std::string::npos) return false;
  a = j.find('[', a);
  if (a == std::string::npos) return false;
  int glebokosc = 0;
  size_t b = a;
  for (; b < j.size(); b++) {
    if (j[b] == '[') glebokosc++;
    else if (j[b] == ']' && --glebokosc == 0) break;
  }
  if (b >= j.size()) return false;
  auto liczba = [&](size_t od, size_t do_, const char* k, double& v) {
    size_t q = j.find(std::string("\"") + k + "\"", od);
    if (q == std::string::npos || q > do_) return false;
    q = j.find(':', q);
    if (q == std::string::npos || q > do_) return false;
    char* e = nullptr;
    v = strtod(j.c_str() + q + 1, &e);
    return e && e != j.c_str() + q + 1;
  };
  size_t p = a;
  for (;;) {
    size_t o = j.find('{', p);
    if (o == std::string::npos || o > b) break;
    size_t z = j.find('}', o);
    if (z == std::string::npos || z > b) break;
    double st = 0, en = 0;
    // `start_ms`, `end_ms`: czas cywilny aparatu zakodowany jak UTC (AirBridge, bez strefy) → chwila unix w strefie
    // panelu, żeby porównanie z chwilą importu nocy (czas unix) i godziny `drzemka {hh:mm}` były w tej samej skali
    if (liczba(o, z, "start_ms", st) && liczba(o, z, "end_ms", en) && en > st && st > 0 && en < 4.0e12)
      s.push_back({chwilaZCywilnego((long long)(st / 1000)), chwilaZCywilnego((long long)(en / 1000)), NAN});
    p = z + 1;
  }
  size_t l = j.find("\"id\":\"Leak\"");
  if (l != std::string::npos) {
    size_t z = j.find('}', l);
    double v = 0;
    if (z != std::string::npos && liczba(l, z, "p95", v)) {
      double mn = 1;
      size_t u = j.find("\"unit\"", l);
      if (u != std::string::npos && u < z && j.compare(j.find('"', j.find(':', u) + 1) + 1, 3, "L/s") == 0) mn = 60;
      p95 = (float)(v * mn);
    }
  }
  return true;
}

float wyciekDrzemkiZDoby(float doby, float nocy) {
  if (std::isnan(doby)) return NAN;
  if (std::isnan(nocy) || doby > nocy + 0.5f) return doby;
  return NAN;
}

std::string zapiszDrzemke(const std::string& dzis, const Drzemka& d) {
  long p = std::isnan(d.wyciekP95) ? -1 : std::lround(d.wyciekP95 * 10);
  return dzis + ";" + std::to_string(d.minut) + ";" + std::to_string(p) + ";" + std::to_string(d.od) + ";" +
         std::to_string(d.do_);
}
bool parsujDrzemke(const std::string& t, const std::string& dzis, Drzemka& d) {
  std::vector<std::string> f = podziel(t, ';');
  if (f.size() != 5 || f[0] != dzis) return false;
  Drzemka x;
  x.minut = liczbaCalk(f[1].c_str());
  long p = liczbaDluga(f[2].c_str());
  x.wyciekP95 = p < 0 ? NAN : (float)p / 10.0f;
  x.od = (uint32_t)strtoul(f[3].c_str(), nullptr, 10);
  x.do_ = (uint32_t)strtoul(f[4].c_str(), nullptr, 10);
  if (x.minut < 0 || x.minut > 24 * 60) return false;
  d = x;
  return true;
}
bool prefiksSesji(const std::string& p, std::string& data, int& minuta) {
  if (p.size() < 15 || p[8] != '_') return false;
  for (int i = 0; i < 15; i++)
    if (i != 8 && (p[(size_t)i] < '0' || p[(size_t)i] > '9')) return false;
  std::string d = p.substr(0, 4) + "-" + p.substr(4, 2) + "-" + p.substr(6, 2);
  if (!dataOk(d)) return false;
  int h = liczbaCalk(p.substr(9, 2).c_str()), m = liczbaCalk(p.substr(11, 2).c_str());
  if (h > 23 || m > 59) return false;
  data = d;
  minuta = h * 60 + m;
  return true;
}

// `22:00-08:00` (też `22:00–08:00`): dwie godziny GG:MM, początek różny od końca (pusta i całodobowa cisza: R-BUZ-02)
bool ciszaZTekstu(const std::string& t, int& od, int& doo) {
  size_t r = t.find('-');
  size_t dl = 1;
  if (r == std::string::npos) { r = t.find("–"); dl = 3; }
  if (r == std::string::npos) return false;
  auto godz = [](const std::string& x, int& m) {
    if (x.size() != 5 || x[2] != ':') return false;
    for (size_t i : {0, 1, 3, 4})
      if (x[i] < '0' || x[i] > '9') return false;
    int g = (x[0] - '0') * 10 + (x[1] - '0'), mi = (x[3] - '0') * 10 + (x[4] - '0');
    if (g > 23 || mi > 59) return false;
    m = g * 60 + mi;
    return true;
  };
  int a = 0, b = 0;
  if (!godz(t.substr(0, r), a) || !godz(t.substr(r + dl), b) || a == b) return false;
  od = a;
  doo = b;
  return true;
}

UstawieniaDnia::UstawieniaDnia() {
  sprzet[EL_MASKA].okres = 6;  // 5.9.2: refundacja co 6 miesięcy
  sprzet[EL_FILTR].okres = 4;  // 5.9.2: 4 tygodnie
  sprzet[EL_FILTR].tygodnie = true;
  for (int k = 0; k < PYL_N; k++) pylki.prog[k] = PYLEK_PROG_DOMYSLNY[k];
}

UstawieniaDnia parsujUstawieniaDnia(const std::string& tresc) {
  UstawieniaDnia u;
  static const char* const PY[PYL_N] = {"brzoza", "trawy", "olcha", "bylica"};
  std::string wPoduszka;      // stara data wymiany poduszki, scalana z maską
  bool jestFiltrTyg = false;  // `okres.filtr_tyg` ma pierwszeństwo przed starym `okres.filtr` (miesiące)
  int filtrStaryMies = 0;
  for (std::string l : podziel(bezBom(tresc), '\n')) {
    obetnijKoniec(l);  // `\r`, spacje i tabulatory na końcu
    if (l.empty() || l[0] == '#') continue;
    size_t r = l.find('=');
    if (r == std::string::npos) { u.bledy.push_back(l); continue; }
    std::string k = l.substr(0, r), v = l.substr(r + 1);
    while (!k.empty() && k.back() == ' ') k.pop_back();
    while (!v.empty() && (v[0] == ' ' || v[0] == '\t')) v.erase(0, 1);
    // klucze `raport=` i `raport.czynniki=` czyta src/raport.cpp (wariantZKonfiguracji, czynnikiZKonfiguracji),
    // więc nie są tu „pominiętym wierszem” w logu
    if (k == "raport" || k == "raport.czynniki") continue;
    bool ok = false;
    int m = 0;
    if (k == "wymiana.maska") { ok = dataOk(v); if (ok) u.sprzet[EL_MASKA].wymieniono = v; }
    if (k == "wymiana.poduszka") { ok = dataOk(v); if (ok) wPoduszka = v; }  // migracja do maski
    if (k == "wymiana.filtr") { ok = dataOk(v); if (ok) u.sprzet[EL_FILTR].wymieniono = v; }
    // R-SKI-08 (5.6.4 „Początek leczenia”, tylko plik K): zła data = wiersz pominięty z wpisem w `bledy` (log);
    // data późniejsza niż dziś też bez karencji (sprawdza silnik, reguly_rodzaje.cpp rOdZdarzenia)
    if (k == "poczatek_leczenia") { ok = dataOk(v); if (ok) u.poczatekLeczenia = v; }
    if (k == "uwaga_o_masce") {  // R-KONF-42: 10–15 L/min, liczba całkowita; inaczej wiersz pominięty (5.6.8)
      ok = indeksZTekstu(v.c_str(), UWAGA_O_MASCE_MAX + 1, m) && m >= UWAGA_O_MASCE_MIN;
      if (ok) u.uwagaOMasce = m;
    }
    if (k == "okres.maska") {
      ok = indeksZTekstu(v.c_str(), 61, m) && m > 0;
      if (ok) u.sprzet[EL_MASKA].okres = m;
    }
    if (k == "okres.filtr_tyg") {  // tygodnie, 1…104
      ok = indeksZTekstu(v.c_str(), 105, m) && m > 0;
      if (ok) { u.sprzet[EL_FILTR].okres = m; jestFiltrTyg = true; }
    }
    if (k == "okres.filtr") {  // stary klucz, miesiące bez jednostki: × 4 tygodnie (spec 5.6.4)
      ok = indeksZTekstu(v.c_str(), 61, m) && m > 0;
      if (ok) filtrStaryMies = m;
    }
    // rury i komory panel nie pilnuje, poduszka jest częścią maski: klucze czytane bez błędu i ignorowane
    // (zostają w pliku; formularz ich nie usuwa)
    if (k == "wymiana.rura" || k == "okres.rura" || k == "wymiana.komora" || k == "okres.komora" ||
        k == "okres.poduszka" || k == "poduszka")
      ok = true;
    if (k == "pylki") {
      ok = true;
      for (auto& x : podziel(v, ',')) {
        bool z = false;
        for (int i = 0; i < PYL_N; i++)
          if (x == PY[i]) { u.pylki.zaznaczony[i] = true; z = true; }
        if (!z && !x.empty()) ok = false;
      }
    }
    if (k == "profil") {  // (5.6.4): „Profil publiczny”
      ok = v == "publiczny" || v == "prywatny";
      if (ok) u.profilPubliczny = v == "publiczny";
    }
    for (int i = 0; i < PYL_N; i++)
      if (k == std::string("pylek.") + PY[i]) {
        float f = liczbaZ(v, 0, 100000);
        ok = !std::isnan(f) && f > 0;
        if (ok) u.pylki.prog[i] = f;
      }
    if (k == "noc_zrodlo") {  // ADR-0036 pkt 5 i „Zmiana 2026-11-09”: bramka przed przełączeniem na noc z plików sesji
      ok = v == "plik" || v == "json" || v == "porownanie" || v == "porównanie";
      if (ok) u.nocZrodlo = v == "plik" ? 1 : v == "json" ? 0 : 2;
    }
    if (k == "noc_json") {
      ok = noc_pliki::parsujMetrykiJson(v, u.nocJson);
      u.nocJsonZly = !ok;
    }
    if (k == "co2") {  // przełącznik ekranów CO2 per egzemplarz (5.8.8)
      ok = v == "tak" || v == "nie";
      if (ok) u.co2 = v == "tak" ? 1 : 0;
    }
    // ustawienia z formularza U-03 (5.6.4). Wartości spoza dozwolonych: wiersz pominięty (R-KONF-21).
    if (k == "humor") {
      ok = true;
      if (v == "wylaczony" || v == "wyłączony") u.humor = 0;
      else if (v == "oszczedny" || v == "oszczędny") u.humor = 1;
      else if (v == "normalny") u.humor = 2;
      else ok = false;
    }
    if (k == "buzzer") {
      ok = v == "tak" || v == "nie";
      if (ok) u.buzzer = v == "tak" ? 1 : 0;
    }
    if (k == "podpowiedz") {
      ok = v == "tak" || v == "nie";
      if (ok) u.podpowiedz = v == "tak" ? 1 : 0;
    }
    if (k == "cisza") {
      int od = -1, doo = -1;
      ok = ciszaZTekstu(v, od, doo);
      if (ok) { u.ciszaOd = od; u.ciszaDo = doo; }
    }
    if (k == "miasto") {
      ok = miasta::znajdz(v) != nullptr;
      if (ok) u.miasto = v;
    }
    if (!ok) u.bledy.push_back(l);
  }
  // współrzędne pogody z miasta z listy (`miasto=`); bez wiersza NAN (dyrygent: secrets.h, potem Katowice)
  if (!u.miasto.empty())
    if (const miasta::Miasto* m = miasta::znajdz(u.miasto)) { u.pogodaLat = m->szer; u.pogodaLon = m->dl; }
  // maska = późniejsza z dat `wymiana.maska` i `wymiana.poduszka`; stary `okres.filtr` tylko bez `okres.filtr_tyg`
  if (!wPoduszka.empty() && wPoduszka > u.sprzet[EL_MASKA].wymieniono) u.sprzet[EL_MASKA].wymieniono = wPoduszka;
  if (!jestFiltrTyg && filtrStaryMies > 0) u.sprzet[EL_FILTR].okres = std::min(104, 4 * filtrStaryMies);
  return u;
}

bool co2Wlaczone(bool zrodlo, int ustawienie) { return zrodlo && ustawienie != 0; }

// ---------- stopka dzienna ----------
StopkaDzienna stopkaDzienna(const Noc* noc, bool nocUbiegla, int drzemkaMin, float zaOknem, float przyLozku) {
  std::string czas = "noc –", ahi = "AHI –", wyc = " · wyciek –";
  if (noc) {
    std::string pref = nocUbiegla ? "noc " : "noc " + ddmm(noc->data) + " ";
    czas = noc->m01 >= 0 ? pref + godzH(noc->m01) : (nocUbiegla ? "noc –" : "noc " + ddmm(noc->data));
    ahi = "AHI " + przecinek1(noc->ahi);
    wyc = " · wyciek " + calkowita(noc->wp95);
  }
  // twarde spacje w `7 h 52` (R-EKR-08): stopka nie łamie się, ale szerokość ta sama
  std::string drz = drzemkaMin > 0 ? " · drzemka " + godzH(drzemkaMin) : "";
  std::string zaO = std::isnan(zaOknem) ? "" : "za oknem " + stopnie(zaOknem);
  std::string przy = std::isnan(przyLozku) ? "" : "przy łóżku " + stopnie(przyLozku);
  StopkaDzienna s;
  bool zWyciekiem = drzemkaMin <= 0;  // po drzemce lewo bez wycieku (R-KON-51)
  bool zCzasem = true, zZaOknem = !zaO.empty();
  for (int krok = 0; krok < 4; krok++) {
    s.lewo.clear();
    if (zCzasem) { s.lewo += czas; s.lewo += drz; s.lewo += " · "; }
    else if (!drz.empty()) { s.lewo += drz.substr(3); s.lewo += " · "; }
    s.lewo += ahi;
    if (zWyciekiem) s.lewo += wyc;
    if (zZaOknem && !przy.empty()) { s.prawo = zaO; s.prawo += " · "; s.prawo += przy; }
    else s.prawo = !przy.empty() ? przy : (zZaOknem ? zaO : "");
    if (szer(F_P22B, s.lewo) + (s.prawo.empty() ? 0 : 24 + szer(F_P22B, s.prawo)) <= 744) break;
    if (zZaOknem && !przy.empty()) zZaOknem = false;
    else if (zWyciekiem) zWyciekiem = false;
    else zCzasem = false;
  }
  return s;
}

// ---------- wybór i teksty ----------
// Grupa tabeli 7.2 (część „Rano”) typu ekranu dnia: R0 pytanie, R1 S5, R2/R3 ekran L, R4 S-K-12, R5a–R5d problem
// poranny (zdarzenia E, S-K-10 bez wariantu opisowego SK10W, S-K-11, S-K-14, S-K-16, S-K-17, S-K-30, S-K-31, L-09), R6 S7, R7 kamienie milowe,
// R8 odkrycia i trendy, R9 dowód spokoju i rozruch. D0 = R0…R5d; R5e i R6…R9 ustępują ekranowi dziennemu (ADR-0027 pkt 1).
// R5e (ADR-0039): karty odchyleń S-K-40…S-K-47, ton „informacja”, nie są prośbą (7.4): wynik 6, jak R6.
// Typ nieznany (np. z nowej) zostaje: lepiej nie zasłonić sprawy niż ją ukryć.
int grupaRano(const std::string& typ) {
  static const char* const R6[] = {"S7", "SK40", "SK41", "SK42", "SK43", "SK44", "SK45", "SK46", "SK47"};
  static const char* const R7[] = {"KM", "SK27", "SK28", "SK29"};
  // SK10W: wariant opisowy S-K-10 `Wyciek widoczny` (R-KONF-42, ton „informacja”, R8), nie problem poranny
  static const char* const R8[] = {"S1", "SK10W", "SK13", "SK15", "SK19", "SK20", "SK21", "SK32", "O"};
  static const char* const R9[] = {"S2A", "S2B", "S2C", "S2D", "S6", "S8", "SK18"};
  for (auto t : R6) if (typ == t) return 6;
  for (auto t : R7) if (typ == t) return 7;
  for (auto t : R8) if (typ == t || (typ.size() > 2 && typ.compare(0, 2, "O-") == 0)) return 8;
  for (auto t : R9) if (typ == t) return 9;
  if (typ == "S5" || typ == "S10") return 1;
  return 0;  // L, E, S-K-10…S-K-17 (bez S-K-13, S-K-15), S-K-30, S-K-31 i typy nieznane: zostają (D0)
}
bool zostajeRano(const std::string& typ, bool pytanieR0) {
  if (pytanieR0) return true;
  return grupaRano(typ) <= 5;
}

const char* idEkranuSlotu(EkranSlotu e) {
  switch (e) {
    case EkranSlotu::SK33: return "S-K-33";
    case EkranSlotu::SK34: return "S-K-34";
    case EkranSlotu::SK35: return "S-K-35";
    case EkranSlotu::SK36: return "S-K-36";
    case EkranSlotu::SK37: return "S-K-37";
    case EkranSlotu::SK38: return "S-K-38";
    case EkranSlotu::SK39: return "S-K-39";
    default: return "";
  }
}

bool wBlokuObrazka(int minuta, int slotOd) {
  return slotOd >= 0 && minuta >= slotOd && minuta < slotOd + OBRAZEK_BLOK_MIN;
}

WyborSlotu wybierzSlot(const WejscieSlotu& w) {
  WyborSlotu r;
  int godz = w.minuta >= 0 ? w.minuta / 60 : -1;
  if (wyciekDrzemki(w.drzemka)) {  // D0a (R-KON-58)
    r.ekran = EkranSlotu::SK39;
    return r;
  }
  if (w.prognoza && w.prognoza->jest()) {
    WejscieWietrzenia ww;
    ww.prognoza = w.prognoza;
    ww.dzis = w.dzis;
    ww.godzina = godz;
    ww.przyLozku = w.przyLozku;
    ww.co2 = w.co2;
    ww.pylki = w.pylki;
    r.okno = regulaWietrzenia(ww);
    r.jutro = jutroRano(*w.prognoza, w.dzis);
    r.pylek = pylekDnia(*w.prognoza, w.dzis, godz, w.pylki);
  }
  bool okno = r.okno.wariant != Wietrzenie::BRAK;
  // w pierwszym bloku slotu S-K-38 przed zwykłymi S-K-33…S-K-37 (R-KON-50); przed nim
  // tylko D0a (wyżej) i `Przewietrz teraz.` (R-KON-44), a z linią uwagi (R-KON-54) obrazka nie ma
  if (w.obrazekJest && !w.liniaUwagi && r.okno.wariant != Wietrzenie::CO2 && wBlokuObrazka(w.minuta, w.slotOd)) {
    r.ekran = EkranSlotu::SK38;
    return r;
  }
  bool jutro = r.jutro.wariant != Jutro::BRAK;
  // R-KON-46: po 16:00 S-K-34 przed S-K-33, chyba że okno trwa albo zaczyna się w ciągu 60 min (wariant CO2 to „teraz”)
  bool oknoBliskie = r.okno.wariant == Wietrzenie::TRWA || r.okno.wariant == Wietrzenie::CO2 ||
                     (r.okno.wariant == Wietrzenie::POZNIEJ && r.okno.od - godz <= 1);
  if (jutro && okno && godz >= JUTRO_PRZED_OKNEM_OD && !oknoBliskie) { r.ekran = EkranSlotu::SK34; return r; }
  if (okno) { r.ekran = EkranSlotu::SK33; return r; }
  if (jutro) { r.ekran = EkranSlotu::SK34; return r; }
  if (w.sprzet) {
    r.wymiana = wymianaSprzetu(*w.sprzet, w.dzis, w.ostatnieSK35);
    if (r.wymiana.element >= 0) { r.ekran = EkranSlotu::SK35; return r; }
  }
  if (r.pylek.rodzaj >= 0) { r.ekran = EkranSlotu::SK36; return r; }
  // D5 poza blokiem obrazka (R-KON-50): S-K-37, a bez współrzędnych miasta zastępstwo S-K-38 (R-KON-54)
  if (w.wspolrzedne) r.slonce = dzienINoc(w.dzis, w.lat, w.lon);
  bool sk37 = r.slonce.jest, sk38 = w.obrazekJest && !w.liniaUwagi;
  r.ekran = sk37 ? EkranSlotu::SK37 : sk38 ? EkranSlotu::SK38 : EkranSlotu::BRAK;
  return r;
}

std::string hDoMin(int g) { return hhmm(g * 60); }

TekstySlotu tekstySlotu(const WyborSlotu& w, const WejscieSlotu& we) {
  TekstySlotu t;
  const char* krokZima = "→ Krótko i szeroko, 5–10\xC2\xA0min.";
  auto krokLato = [&](int doGodz) { return "→ Otwórz szeroko, zamknij o " + hDoMin(doGodz) + "."; };
  switch (w.ekran) {
    case EkranSlotu::SK33: {
      const WynikWietrzenia& o = w.okno;
      t.etykieta = "Okno na wietrzenie";
      std::string pm = calkowita(o.pm25Teraz);
      switch (o.wariant) {
        case Wietrzenie::CO2:
          t.tytul = "Przewietrz teraz.";
          t.powod = "CO2 w sypialni " + std::to_string(we.co2) + " ppm, za oknem czysto.";
          t.krok = krokZima;
          break;
        case Wietrzenie::TRWA:
          t.podpis = "wietrz do";
          t.godzina = hDoMin(o.do_);
          t.powod = o.powod == PowodOkna::CZYSTO_DO_WIECZORA ? "Za oknem czysto do wieczora."
                    : o.powod == PowodOkna::SMOG             ? "Potem smog za oknem."
                    : o.powod == PowodOkna::ZIMNIEJ          ? "Potem na dworze zimniej."
                                                             : "Potem na dworze cieplej niż w\xC2\xA0" "domu.";
          t.krok = o.tryb == TrybWietrzenia::LATO ? krokLato(o.do_) : krokZima;
          break;
        case Wietrzenie::POZNIEJ:
          t.podpis = "wietrz od";
          t.godzina = hDoMin(o.od);
          t.powod = o.powod == PowodOkna::TERAZ_SMOG ? "Teraz smog za oknem, PM2.5 " + pm + "."
                    : o.powod == PowodOkna::NAJCIEPLEJ ? "Wtedy na dworze najcieplej i\xC2\xA0" "czysto."
                                                       : "Wtedy na dworze chłodniej niż w\xC2\xA0" "domu.";
          t.krok = o.tryb == TrybWietrzenia::LATO ? krokLato(o.do_) : krokZima;
          break;
        case Wietrzenie::UPAL_DO:
          t.podpis = "okna zamknięte do";
          t.godzina = hDoMin(o.od);
          t.powod = "Na dworze cieplej niż w\xC2\xA0" "domu.";
          t.krok = "→ Zasłoń okna od strony słońca.";
          break;
        case Wietrzenie::UPAL_CALY_DZIEN:
          t.tytul = "Okna zamknięte cały dzień.";
          t.powod = "Na dworze cieplej niż w\xC2\xA0" "domu.";
          t.krok = "→ Zasłoń okna od strony słońca.";
          break;
        case Wietrzenie::SMOG:
          t.tytul = "Dziś bez wietrzenia.";
          if (std::isnan(o.pm25Teraz)) {  // teraz bez pomiaru: najwyższa zmierzona wartość horyzontu
            float mx = NAN;
            for (float x : o.pm25)
              if (!std::isnan(x) && (std::isnan(mx) || x > mx)) mx = x;
            pm = calkowita(mx);
          }
          t.powod = "Smog za oknem do wieczora, PM2.5 " + pm + ".";
          t.krok = "→ Jeśli trzeba, wietrz bardzo krótko.";
          break;
        default: break;
      }
      break;
    }
    case EkranSlotu::SK34: {
      const WynikJutra& j = w.jutro;
      t.etykieta = "Jutro rano";
      std::string tm = stopnie(j.tMin);
      if (j.wariant == Jutro::SLISKO) {
        t.tytul = "Jutro rano ślisko.";
        t.powod = "Rano " + tm + " i\xC2\xA0opady. Na chodnikach może być lód.";
        t.krok = "→ Zostaw rano więcej czasu.";
      } else if (j.wariant == Jutro::MROZ) {
        t.tytul = "Jutro rano mróz, " + tm + ".";
        t.powod = "Szyby i\xC2\xA0" "auta mogą być oszronione.";
        t.krok = "→ Zostaw rano więcej czasu.";
      } else if (j.wariant == Jutro::DESZCZ) {
        t.tytul = "Jutro rano deszcz.";
        t.powod = "Opady " + calkowita(j.opadMax) + "% między 6:00 a\xC2\xA0" "9:00.";
        t.krok = "→ Parasol przy drzwiach.";
      } else if (j.wariant == Jutro::WIATR) {
        t.tytul = "Jutro rano silny wiatr.";
        t.powod = "Porywy do " + calkowita(j.porywyMax) + "\xC2\xA0km/h.";
        t.krok = "→ Zabierz rzeczy z\xC2\xA0" "balkonu.";
      }
      break;
    }
    case EkranSlotu::SK35: {
      const WynikWymiany& m = w.wymiana;
      t.etykieta = "Sprzęt";
      t.tytul = std::string("Wymiana: ") + NAZWY_ELEMENTU[m.element] + ".";
      std::string wiek = we.sprzet ? wiekElementu((*we.sprzet)[m.element], we.dzis) : "–";
      if (m.element == EL_MASKA && m.poTerminie) {
        t.powod = "Możesz już odebrać nową maskę z refundacji.";  // ADR-0002, bez porady terapeutycznej
      } else {
        t.powod = std::string(m.poTerminie ? "Termin minął " : "Termin ") + ddmm(m.termin) + ", ma " + wiek;
        if (t.powod.back() != '.') t.powod += ".";
      }
      // R-MAS-18 (ADR-0041): wymianę zapisuje ◀ 2 s na tym ekranie (element z ekranu, jak S-K-31 R-MAS-01);
      // dawny krok prowadził do usuniętej strony „Sprzęt”
      t.krok = "→ Po wymianie przytrzymaj ◀ 2\xC2\xA0s.";
      break;
    }
    case EkranSlotu::SK36:
      t.etykieta = "Pyłki";
      t.tytul = std::string("Pyłek ") + PYLEK_DOPELNIACZ[w.pylek.rodzaj] + " wysoko.";
      t.powod = "Najwięcej między " + hDoMin(w.pylek.od) + " a\xC2\xA0" + hDoMin(w.pylek.do_) + ".";
      t.krok = "→ W tych godzinach okna zamknięte.";
      break;
    case EkranSlotu::SK37:
      t.etykieta = "Dzień i noc";
      t.podpis = "zachód słońca";
      t.godzina = hhmm(w.slonce.zachod);
      t.powod = w.slonce.powod;
      break;
    case EkranSlotu::SK39: {
      t.etykieta = "Po drzemce";
      t.tytul = "Drzemka: maska przeciekała.";
      t.powod = "Wyciek w\xC2\xA0" "drzemce do " + calkowita(we.drzemka.wyciekP95) + "\xC2\xA0l/min, próg " +
                calkowita(WYCIEK_PROG_SK10) + ".";
      t.krok = "→ Sprawdź poduszkę i\xC2\xA0paski przed nocą.";
      break;
    }
    default: break;
  }
  return t;
}
