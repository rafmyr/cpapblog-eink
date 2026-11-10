// SPDX-License-Identifier: GPL-3.0-or-later
// Testy historii nocy ze STR.edf (src/edf.h, src/str.h, ADR-0035): czytnik EDF z bufora (nagłówek, próbki, 0xFFFF,
// plik ucięty i uszkodzony), zgodność z tools/kalibracja/noce.py (klasa Str) na syntetycznym STR z
// tools/kalibracja/syntetyczna.py --str-panelu i wzorcu tools/kalibracja/str_wzorzec.py, doba STR → rekord nocy 'S',
// pierwszeństwo nocy A, K i S, plik `noce` w wersji 4 (ADR-0022), odpowiedź `/api/sessions` i pobieranie STR
// w dyrygencie (etag, 404, 503, plik nieczytelny, zapis historii).
// Dane wyłącznie syntetyczne (bez związku z żadną osobą).
// Uruchomienie: tools/prototyp/testy.sh (program testy_str_edf z katalogiem, w którym są STR.edf i wzorzec.txt).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "edf.h"
#include "magazyn.h"
#include "noc.h"
#include "str.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
#define ROWNE(a, b)                                                                                     \
  do {                                                                                                  \
    std::string _a = (a), _b = (b);                                                                     \
    if (_a == _b) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)
#define ROWNE_L(a, b)                                                                                    \
  do {                                                                                                   \
    long long _a = (long long)(a), _b = (long long)(b);                                                  \
    if (_a == _b) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); }  \
  } while (0)
#define BLISKO(a, b)                                                                                          \
  do {                                                                                                        \
    double _a = (double)(a), _b = (double)(b);                                                                \
    if (std::fabs(_a - _b) < 1e-3) ok++;                                                                      \
    else { zle++; printf("BŁĄD %s:%d: %s = %.6f, oczekiwane %.6f\n", __FILE__, __LINE__, #a, _a, _b); }       \
  } while (0)

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";

static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}

static std::vector<std::string> dziel(const std::string& s, char z) {
  std::vector<std::string> v;
  size_t a = 0;
  for (;;) {
    size_t b = s.find(z, a);
    v.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  return v;
}
static bool zawiera(const std::vector<std::string>& v, const std::string& p) {
  for (auto& s : v)
    if (s.find(p) != std::string::npos) return true;
  return false;
}
static std::string czytajPlik(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream b;
  b << f.rdbuf();
  return b.str();
}

// ---------- budowa pliku EDF w teście ----------
struct SygT {
  std::string etykieta, jednostka;
  double fizMin, fizMax;
  long cyfMin, cyfMax;
  int probki;
};
static std::string pole(const std::string& t, size_t szer) {
  std::string x = t.substr(0, szer);
  x.resize(szer, ' ');
  return x;
}
static std::string num(double v) {
  char b[32];
  if (v == std::floor(v) && std::fabs(v) < 1e9) snprintf(b, sizeof b, "%ld", (long)v);
  else snprintf(b, sizeof b, "%g", v);
  return b;
}
// rekordy[r][s] = próbki sygnału s w rekordzie r (wartości cyfrowe; zapis jako 16 bitów little-endian)
static std::string budujEdf(const std::string& data, const std::string& godz, const std::vector<SygT>& syg,
                            const std::vector<std::vector<std::vector<long>>>& rekordy, long liczbaRekordow = -2) {
  size_t ns = syg.size();
  std::string h = pole("0", 8) + pole("X", 80) + pole("Startdate X", 80) + pole(data, 8) + pole(godz, 8) +
                  pole(std::to_string(256 + 256 * ns), 8) + pole("", 44) +
                  pole(std::to_string(liczbaRekordow == -2 ? (long)rekordy.size() : liczbaRekordow), 8) +
                  pole("86400", 8) + pole(std::to_string(ns), 4);
  for (auto& s : syg) h += pole(s.etykieta, 16);
  for (size_t i = 0; i < ns; i++) h += pole("", 80);
  for (auto& s : syg) h += pole(s.jednostka, 8);
  for (auto& s : syg) h += pole(num(s.fizMin), 8);
  for (auto& s : syg) h += pole(num(s.fizMax), 8);
  for (auto& s : syg) h += pole(num((double)s.cyfMin), 8);
  for (auto& s : syg) h += pole(num((double)s.cyfMax), 8);
  for (size_t i = 0; i < ns; i++) h += pole("", 80);
  for (auto& s : syg) h += pole(std::to_string(s.probki), 8);
  for (size_t i = 0; i < ns; i++) h += pole("", 32);
  for (auto& r : rekordy)
    for (size_t i = 0; i < ns; i++)
      for (int k = 0; k < syg[i].probki; k++) {
        long v = k < (int)r[i].size() ? r[i][(size_t)k] : -1;
        uint16_t u = (uint16_t)(v & 0xFFFF);
        h += (char)(u & 0xFF);
        h += (char)(u >> 8);
      }
  return h;
}
static long cyf(const SygT& s, double v) {  // NAN → 0xFFFF (brak)
  if (std::isnan(v)) return -1;
  return std::lround((double)s.cyfMin + (v - s.fizMin) * (double)(s.cyfMax - s.cyfMin) / (s.fizMax - s.fizMin));
}

// STR testowy: doby od `pierwszyDzien` (numer dnia); wartości fizyczne po etykiecie (brak klucza = brak wartości).
static const std::vector<SygT> SYG_STR = {
    {"Date", "", 0, 32767, 0, 32767, 1},          {"MaskOn", "min.", 0, 1440, 0, 1440, 10},
    {"Duration", "min.", 0, 1440, 0, 1440, 1},    {"Mode", "", 0, 16, 0, 16, 1},
    {"S.AA.MinEPAP", "cmH2O", 1, 30, 50, 1500, 1}, {"S.AA.MaxEPAP", "cmH2O", 1, 30, 50, 1500, 1},
    {"S.AA.MinPS", "cmH2O", 0, 30, 0, 1500, 1},   {"S.AA.MaxPS", "cmH2O", 0, 30, 0, 1500, 1},
    {"AHI", "", 0, 240, 0, 2400, 1},              {"HI", "", 0, 240, 0, 2400, 1},
    {"OAI", "", 0, 240, 0, 2400, 1},              {"TgtIPAP.50", "cmH2O", 0, 30, 0, 1500, 1},
    {"TgtEPAP.50", "cmH2O", 0, 30, 0, 1500, 1},   {"TgtEPAP.95", "cmH2O", 0, 30, 0, 1500, 1},
    {"Leak.50", "L/s", 0, 2, 0, 100, 1},          {"Leak.95", "L/s", 0, 2, 0, 100, 1},
    {"RespRate.50", "bpm", 0, 50, 0, 250, 1},     {"Crc16", "", -32768, 32767, -32768, 32767, 1},
};
using DobaT = std::map<std::string, double>;
static DobaT dobaAsvAuto(double czas, double ahi = 2.0) {
  return {{"Duration", czas}, {"Mode", 8},          {"S.AA.MinEPAP", 6},  {"S.AA.MaxEPAP", 10}, {"S.AA.MinPS", 3},
          {"S.AA.MaxPS", 15}, {"AHI", ahi},         {"HI", 1.5},          {"TgtIPAP.50", 12.5}, {"TgtEPAP.50", 7},
          {"TgtEPAP.95", 8},  {"Leak.50", 0.1},     {"Leak.95", 0.3},     {"RespRate.50", 15}};
}
static std::string strTestowy(long pierwszyDzien, const std::vector<DobaT>& doby,
                              const std::vector<SygT>& syg = SYG_STR) {
  std::vector<std::vector<std::vector<long>>> rek;
  for (size_t i = 0; i < doby.size(); i++) {
    DobaT d = doby[i];
    if (!d.count("Date")) d["Date"] = (double)(pierwszyDzien + (long)i);
    std::vector<std::vector<long>> r;
    for (auto& s : syg) {
      if (s.probki != 1) {
        r.push_back(std::vector<long>((size_t)s.probki, -1));
        continue;
      }
      auto it = d.find(s.etykieta);
      r.push_back({it == d.end() ? -1 : cyf(s, it->second)});
    }
    rek.push_back(r);
  }
  // start pliku: południe pierwszego dnia
  std::string data = dataZNr(pierwszyDzien);  // RRRR-MM-DD
  std::string dd = data.substr(8, 2) + "." + data.substr(5, 2) + "." + data.substr(2, 2);
  return budujEdf(dd, "12.00.00", syg, rek);
}

// ============================================================================================================
// (a) czytnik EDF
// ============================================================================================================
static void testNaglowekIProbki() {
  std::vector<SygT> s = {{"A", "u", -10, 10, -100, 100, 2}, {"B", "", 0, 1, 0, 1, 1}};
  std::string b = budujEdf("09.11.26", "12.30.15", s, {{{-50, 50}, {1}}, {{0, -1}, {0}}});
  edf::Plik p;
  SPRAWDZ(edf::czytaj((const uint8_t*)b.data(), b.size(), p) == edf::Blad::BRAK);
  ROWNE_L(p.rekordy, 2);
  ROWNE_L(p.sygnaly.size(), 2);
  ROWNE_L(p.bajtyNaglowka, 256 + 512);
  ROWNE_L(p.bajtyRekordu, 6);
  ROWNE_L(p.startS, (long long)nrDnia("2026-11-10") * 86400 - 86400 + 12 * 3600 + 30 * 60 + 15);
  ROWNE_L(p.startS / 86400, edf::dniOdEpoki(2026, 11, 9));
  const edf::Sygnal* a = p.sygnal("A");
  SPRAWDZ(a && a->probki == 2 && a->offset == 0);
  if (a) {
    ROWNE_L(p.surowa(*a, 0, 0), 0xFFCE);  // -50
    BLISKO(a->fizyczna(-50), -5.0);
    ROWNE_L(p.surowa(*a, 1, 1), 0xFFFF);
    ROWNE_L(p.surowa(*a, 2, 0), 0xFFFF);  // poza zakresem rekordów
    SPRAWDZ(a->poprawna(100) && !a->poprawna(101));
  }
  SPRAWDZ(p.sygnal("C") == nullptr);
  ROWNE_L(edf::dniOdEpoki(1970, 1, 1), 0);
  ROWNE_L(edf::dniOdEpoki(2026, 11, 8), 20765);
  ROWNE_L(edf::dniOdEpoki(1969, 12, 31), -1);
}

static void testBledyEdf() {
  std::vector<SygT> s = {{"A", "u", 0, 10, 0, 100, 2}};
  std::string b = budujEdf("09.11.26", "12.00.00", s, {{{1, 2}}, {{3, 4}}, {{5, 6}}});
  edf::Plik p;
  auto czyt = [&](const std::string& x) { return edf::czytaj((const uint8_t*)x.data(), x.size(), p); };
  SPRAWDZ(czyt(b) == edf::Blad::BRAK);
  SPRAWDZ(edf::czytaj(nullptr, 0, p) == edf::Blad::KROTKI_NAGLOWEK);
  SPRAWDZ(czyt("") == edf::Blad::KROTKI_NAGLOWEK);
  SPRAWDZ(czyt(b.substr(0, 255)) == edf::Blad::KROTKI_NAGLOWEK);
  SPRAWDZ(czyt(b.substr(0, 300)) == edf::Blad::KROTKI_NAGLOWEK);  // bez całego opisu sygnałów
  // ucięty plik: każda długość krótsza od całości to błąd, nigdy odczyt części
  int bledy = 0;
  for (size_t n = 0; n < b.size(); n++) bledy += czyt(b.substr(0, n)) != edf::Blad::BRAK;
  ROWNE_L(bledy, b.size());
  SPRAWDZ(czyt(b.substr(0, b.size() - 1)) == edf::Blad::UCIETY);
  SPRAWDZ(czyt(b + "xyz") == edf::Blad::BRAK);  // nadmiar bajtów za rekordami: pominięty
  std::string w = b;
  w[0] = '1';
  SPRAWDZ(czyt(w) == edf::Blad::WERSJA);
  // zła liczba rekordów: więcej niż w pliku, -1 (plik w zapisie), śmieci
  SPRAWDZ(czyt(budujEdf("09.11.26", "12.00.00", s, {{{1, 2}}}, 5)) == edf::Blad::UCIETY);
  SPRAWDZ(czyt(budujEdf("09.11.26", "12.00.00", s, {{{1, 2}}}, -1)) == edf::Blad::REKORDY);
  w = b;
  w.replace(236, 8, pole("abc", 8));
  SPRAWDZ(czyt(w) == edf::Blad::NAGLOWEK);
  w = b;
  w.replace(252, 4, pole("0", 4));
  SPRAWDZ(czyt(w) == edf::Blad::NAGLOWEK);
  w = b;
  w.replace(184, 8, pole("1024", 8));  // długość nagłówka niezgodna z liczbą sygnałów
  SPRAWDZ(czyt(w) == edf::Blad::NAGLOWEK);
  SPRAWDZ(czyt(budujEdf("31.02.26", "12.00.00", s, {{{1, 2}}})) == edf::Blad::NAGLOWEK);
  SPRAWDZ(czyt(budujEdf("09.11.26", "25.00.00", s, {{{1, 2}}})) == edf::Blad::NAGLOWEK);
  SPRAWDZ(czyt(budujEdf("09.11.26", "12.00.00", {{"A", "", 0, 10, 100, 100, 1}}, {{{1}}})) == edf::Blad::SYGNAL);
  SPRAWDZ(czyt(budujEdf("09.11.26", "12.00.00", {{"A", "", 5, 5, 0, 100, 1}}, {{{1}}})) == edf::Blad::SYGNAL);
  SPRAWDZ(czyt(budujEdf("09.11.26", "12.00.00", {{"A", "", 0, 10, 0, 100, 0}}, {})) == edf::Blad::SYGNAL);
  // zakres cyfrowy poza [-32768, 65535] (16 bitów, ze znakiem tylko przy cyf_min < 0)
  SPRAWDZ(czyt(budujEdf("09.11.26", "12.00.00", {{"A", "", 0, 10, 0, 65536, 1}}, {{{1}}})) == edf::Blad::SYGNAL);
  SPRAWDZ(czyt(budujEdf("09.11.26", "12.00.00", {{"A", "", 0, 10, -40000, 0, 1}}, {{{1}}})) == edf::Blad::SYGNAL);
  // regresja: sygnał bez znaku 0…65534 jak `PatientHours` w STR AirCurve; próbka powyżej 32767 czytana bez znaku
  {
    std::vector<SygT> u = SYG_STR;
    u.push_back({"PatientHours", "h", 0, 65534, 0, 65534, 1});
    DobaT d = dobaAsvAuto(400);
    d["PatientHours"] = 24337;
    DobaT d2 = dobaAsvAuto(410);
    d2["PatientHours"] = 40000;
    Str su;
    SPRAWDZ(czytajStr(strTestowy(nrDnia("2026-11-01") - 1, {d, d2}, u), su) == edf::Blad::BRAK);
    ROWNE_L(su.doby.size(), 2);
    if (su.doby.size() == 2) {
      BLISKO(wartoscStr(su, su.doby.begin()->second, "PatientHours"), 24337);
      BLISKO(wartoscStr(su, su.doby.rbegin()->second, "PatientHours"), 40000);
      Noc nu;
      SPRAWDZ(nocZeStr(su, su.doby.begin()->first, nu) && nu.m01 == 400);
    }
  }
  SPRAWDZ(czyt(budujEdf("09.11.26", "12.00.00", {{"A", "", 0, 10, -32768, 32767, 1}}, {{{1}}})) == edf::Blad::BRAK);
  // śmieci: losowe zmiany bajtów nagłówka i ucięcia nie wywracają czytnika (wynik dowolny, bez awarii)
  uint32_t ziarno = 12345;
  int przeszly = 0;
  for (int i = 0; i < 3000; i++) {
    std::string x = b;
    for (int k = 0; k < 4; k++) {
      ziarno = ziarno * 1103515245u + 12345u;
      x[(ziarno >> 8) % 512 % x.size()] = (char)(ziarno >> 20);
    }
    Str st;
    if (czytajStr(x, st) == edf::Blad::BRAK) {
      przeszly++;
      for (auto& d : st.doby) {
        Noc n;
        nocZeStr(st, d.first, n);
      }
    }
  }
  printf("  śmieci w nagłówku: 3000 plików bez awarii, %d czytelnych\n", przeszly);
  ROWNE(edf::opisBledu(edf::Blad::UCIETY), "plik ucięty");
}

// ============================================================================================================
// (b) STR: zgodność z noce.py (Str) na syntetycznym pliku
// ============================================================================================================
static double liczbaWz(const std::string& t) { return t == "brak" ? NAN : std::strtod(t.c_str(), nullptr); }
static bool rowneLiczby(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return std::isnan(a) && std::isnan(b);
  return std::fabs(a - b) <= 1e-9 * std::max(1.0, std::fabs(b));
}

static void testZgodnoscZNoce(const std::string& kat) {
  std::string bajty = czytajPlik(kat + "/STR.edf"), wz = czytajPlik(kat + "/wzorzec.txt");
  SPRAWDZ(!bajty.empty() && !wz.empty());
  if (bajty.empty() || wz.empty()) {
    printf("  brak plików wzorca w %s (testy.sh tworzy je skryptami Pythona)\n", kat.c_str());
    return;
  }
  Str s;
  SPRAWDZ(czytajStr(bajty, s) == edf::Blad::BRAK);
  std::map<long, bool> dniWz;
  std::map<long, std::vector<std::string>> polaWz;  // dzień → "tag;value;option" w kolejności
  int d = 0, k = 0, rozne = 0;
  for (auto& l : dziel(wz, '\n')) {
    if (l.empty()) continue;
    std::vector<std::string> f = dziel(l, ';');
    long dzien = std::strtol(f[1].c_str(), nullptr, 10);
    dniWz[dzien] = true;
    auto r = s.doby.find(dzien);
    if (r == s.doby.end()) {
      rozne++;
      continue;
    }
    if (f[0] == "D" && f.size() == 4) {
      d++;
      if (!rowneLiczby(wartoscStr(s, r->second, f[2]), liczbaWz(f[3]))) {
        rozne++;
        printf("  różnica D %s\n", l.c_str());
      }
    } else if (f[0] == "U" && f.size() == 5) {
      polaWz[dzien].push_back(f[2] + ";" + f[3] + ";" + f[4]);
    } else if (f[0] == "K" && f.size() == 5) {
      k++;
      if (!rowneLiczby(statystykaStr(s, r->second, f[2], f[3]), liczbaWz(f[4]))) {
        rozne++;
        printf("  różnica K %s\n", l.c_str());
      }
    }
  }
  ROWNE_L(rozne, 0);
  SPRAWDZ(d > 200 && k > 100);
  ROWNE_L(s.doby.size(), dniWz.size());  // te same doby (bez doby bez Mode i Duration)
  for (auto& x : s.doby) SPRAWDZ(dniWz.count(x.first));
  // ustawienia: ta sama lista pól w tej samej kolejności
  for (auto& x : s.doby) {
    Json u = ustawieniaStr(s, x.second);
    std::vector<std::string> c;
    for (auto& p : u["fields"].el) {
      double o;
      std::string opc = p["option"].liczba(o) ? std::to_string((long)o) : "brak";
      c.push_back(p["tag"].s + ";" + p["value"].s + ";" + opc);
    }
    SPRAWDZ(c == polaWz[x.first]);
    if (c != polaWz[x.first])
      for (auto& z : c) printf("  C++ U;%ld;%s\n", x.first, z.c_str());
  }
}

// Noce z syntetycznego STR (tools/kalibracja/syntetyczna.py dni_str_panelu()): od 28.10.2026 (doba) = poranek 29.10.
static void testNoceZSyntetycznego(const std::string& kat) {
  std::string bajty = czytajPlik(kat + "/STR.edf");
  if (bajty.empty()) return;
  Str s;
  SPRAWDZ(czytajStr(bajty, s) == edf::Blad::BRAK);
  std::vector<Noc> v = noceZeStr(s, "", "AirCurve_10_CS_PaceWave", "SYN00000001");
  std::map<std::string, Noc> m;
  for (auto& n : v) m[n.data] = n;
  ROWNE_L(v.size(), 6);  // doby 0, 2, 3, 4, 6, 7 (1: bez terapii, 5: bez doby)
  SPRAWDZ(!m.count("2026-10-30") && !m.count("2026-11-03"));
  const Noc& a = m["2026-10-29"];
  SPRAWDZ(a.zrodlo == 'S' && a.wazna && !a.czesciowa);
  ROWNE_L(a.m01, 432);
  BLISKO(a.ahi, 2.3);
  SPRAWDZ(std::isnan(a.oa) && std::isnan(a.ca));  // OAI, CAI puste (0xFFFF)
  BLISKO(a.h, 1.2);
  BLISKO(a.ua, 0.4);
  SPRAWDZ(std::isnan(a.cmed) && std::isnan(a.cp95));  // ciśnienie docelowe ze STR poza rekordem (R-NOR-08)
  BLISKO(a.emed, 7.2);
  BLISKO(a.ep95, 8.4);
  SPRAWDZ(a.epapStr && std::isnan(epapMed(a)));
  BLISKO(a.wmed, 6.0);  // 0,1 L/s × 60
  BLISKO(a.wp95, 24.0);
  BLISKO(a.m13, 15.6);
  SPRAWDZ(std::isnan(a.m10) && std::isnan(a.m08) && std::isnan(a.csr) && std::isnan(a.psmed));
  ROWNE_L(a.m17, -1);
  ROWNE_L(a.m18, -1);
  ROWNE(a.ustawienia, "ASVAuto/6/10/3/15");
  ROWNE(a.tryb, "ASVAuto");
  BLISKO(a.eai, 6);
  BLISKO(a.axs, 15);
  SPRAWDZ(std::isnan(a.epr) && std::isnan(a.mpa));
  ROWNE(a.pna, "AirCurve_10_CS_PaceWave");
  ROWNE(a.srn, "SYN00000001");
  ROWNE_L(a.import, 0);
  SPRAWDZ(!rekordPelny(a));
  const Noc& krotka = m["2026-10-31"];
  ROWNE_L(krotka.m01, 200);
  SPRAWDZ(!krotka.wazna);
  const Noc& bezAhi = m["2026-11-01"];
  SPRAWDZ(std::isnan(bezAhi.ahi) && !bezAhi.wazna && bezAhi.m01 == 432);
  const Noc& asv = m["2026-11-02"];
  ROWNE(asv.ustawienia, "ASV/8/3/15");
  ROWNE(asv.tryb, "ASV");
  BLISKO(asv.oa, 0.7);
  BLISKO(asv.ca, 1.9);
  BLISKO(asv.eep, 8);
  SPRAWDZ(asv.wazna);
  const Noc& as = m["2026-11-04"];
  ROWNE(as.ustawienia, "AutoSet/6.5/13/2");
  ROWNE(as.tryb, "AutoSet");
  BLISKO(as.mpi, 6.5);
  BLISKO(as.mpa, 13);
  BLISKO(as.epr, 2);
  const Noc& bezDaty = m["2026-11-05"];  // Date puste: dzień startu + numer rekordu (7)
  ROWNE_L(bezDaty.m01, 432);
  // filtr daty: noce z datą poranka po `dzis` pominięte
  ROWNE_L(noceZeStr(s, "2026-11-02").size(), 4);
  // wiersz pliku nocy z `S` czyta się z powrotem bez zmian
  Noc z;
  SPRAWDZ(parsujNocV3(wierszNocyV3(a), z));
  ROWNE(wierszNocyV3(z), wierszNocyV3(a));
  SPRAWDZ(z.zrodlo == 'S');
}

// ============================================================================================================
// (c) STR: brak wartości, jednostki, data poranka, czas terapii
// ============================================================================================================
static void testWartosciIJednostki() {
  const long D0 = nrDnia("2026-11-01") - 1;  // doba 31.10 → poranek 1.11
  DobaT a = dobaAsvAuto(400);
  a.erase("HI");             // HI brak (0xFFFF)
  a["OAI"] = 300;            // poza zakresem fizycznym → cyfrowa poza [cyf_min, cyf_max]: brak
  std::string b = strTestowy(D0, {a});
  Str s;
  SPRAWDZ(czytajStr(b, s) == edf::Blad::BRAK);
  ROWNE_L(s.doby.size(), 1);
  long r = s.doby.begin()->second;
  SPRAWDZ(std::isnan(wartoscStr(s, r, "HI")));
  SPRAWDZ(std::isnan(wartoscStr(s, r, "OAI")));
  SPRAWDZ(std::isnan(wartoscStr(s, r, "MaskOn")));  // 10 próbek na rekord: poza odczytem dób (jak noce.py)
  SPRAWDZ(std::isnan(wartoscStr(s, r, "Brak")));
  BLISKO(wartoscStr(s, r, "Duration"), 400);
  BLISKO(statystykaStr(s, r, "Leak", "50"), 6.0);
  SPRAWDZ(std::isnan(statystykaStr(s, r, "Leak", "Max")));
  BLISKO(statystykaStr(s, r, "Press", "50"), 12.5);
  BLISKO(statystykaStr(s, r, "EprPress", "95"), 8);
  SPRAWDZ(std::isnan(statystykaStr(s, r, "Nieznany", "50")));
  Noc n;
  SPRAWDZ(nocZeStr(s, D0, n));
  ROWNE(n.data, "2026-11-01");
  SPRAWDZ(std::isnan(n.h) && std::isnan(n.oa));
  // jednostki: Leak w L/min bez mnożnika, Leak w L/h i RespRate w innej jednostce = brak; TidVol w L × 1000
  std::vector<SygT> syg = SYG_STR;
  for (auto& x : syg) {
    if (x.etykieta == "Leak.50") x = {"Leak.50", "L/min", 0, 120, 0, 1200, 1};
    if (x.etykieta == "Leak.95") x = {"Leak.95", "L/h", 0, 120, 0, 1200, 1};
    if (x.etykieta == "RespRate.50") x = {"RespRate.50", "1/s", 0, 50, 0, 250, 1};
  }
  syg.push_back({"TidVol.50", "L", 0, 4, 0, 200, 1});
  DobaT c = dobaAsvAuto(400);
  c["Leak.50"] = 30;
  c["Leak.95"] = 40;
  c["TidVol.50"] = 0.5;
  SPRAWDZ(czytajStr(strTestowy(D0, {c}, syg), s) == edf::Blad::BRAK);
  r = s.doby.begin()->second;
  BLISKO(statystykaStr(s, r, "Leak", "50"), 30);
  SPRAWDZ(std::isnan(statystykaStr(s, r, "Leak", "95")));
  SPRAWDZ(std::isnan(statystykaStr(s, r, "RespRate", "50")));
  BLISKO(statystykaStr(s, r, "TidVol", "50"), 500);
  SPRAWDZ(nocZeStr(s, D0, n));
  BLISKO(n.wmed, 30);
  SPRAWDZ(std::isnan(n.wp95) && std::isnan(n.m13));
  // czas terapii: 0, brak i ponad dobę = bez rekordu; doba bez Mode i Duration = brak doby
  DobaT zero = dobaAsvAuto(0), brak = dobaAsvAuto(0), duzo = dobaAsvAuto(400), pusta;
  brak.erase("Duration");
  SPRAWDZ(czytajStr(strTestowy(D0, {zero, brak, duzo, pusta}), s) == edf::Blad::BRAK);
  ROWNE_L(s.doby.size(), 3);
  SPRAWDZ(!nocZeStr(s, D0, n) && !nocZeStr(s, D0 + 1, n) && nocZeStr(s, D0 + 2, n) && !nocZeStr(s, D0 + 3, n));
  ROWNE_L(noceZeStr(s, "").size(), 1);
  // ten sam Date w dwóch rekordach: późniejszy wygrywa (jak słownik dób w noce.py)
  DobaT p1 = dobaAsvAuto(300), p2 = dobaAsvAuto(350);
  p1["Date"] = (double)D0;
  p2["Date"] = (double)D0;
  SPRAWDZ(czytajStr(strTestowy(D0, {p1, p2}), s) == edf::Blad::BRAK);
  SPRAWDZ(nocZeStr(s, D0, n));
  ROWNE_L(n.m01, 350);
  // Date poza 1990–2099 (np. uszkodzony opis sygnału): doba pominięta, reszta pliku czytana
  DobaT stara = dobaAsvAuto(300), dobra = dobaAsvAuto(310);
  stara["Date"] = 5000;  // 1983
  SPRAWDZ(czytajStr(strTestowy(D0, {stara, dobra}), s) == edf::Blad::BRAK);
  ROWNE_L(s.doby.size(), 1);
  SPRAWDZ(s.doby.count(D0 + 1) == 1);
  // tryb nieobsługiwany (VAuto) i brak trybu: noc zapisana, nieważna
  DobaT va = dobaAsvAuto(400), bm = dobaAsvAuto(400);
  va["Mode"] = 6;
  bm.erase("Mode");
  SPRAWDZ(czytajStr(strTestowy(D0, {va, bm}), s) == edf::Blad::BRAK);
  SPRAWDZ(nocZeStr(s, D0, n) && !n.wazna && n.tryb == "VAuto");
  SPRAWDZ(nocZeStr(s, D0 + 1, n) && !n.wazna && n.tryb.empty() && n.ustawienia.empty());
  // granica ważności: 240 min ważna, 239 nie
  SPRAWDZ(czytajStr(strTestowy(D0, {dobaAsvAuto(240), dobaAsvAuto(239)}), s) == edf::Blad::BRAK);
  SPRAWDZ(nocZeStr(s, D0, n) && n.wazna);
  SPRAWDZ(nocZeStr(s, D0 + 1, n) && !n.wazna);
}

// ============================================================================================================
// (d) pierwszeństwo A, K i S (ADR-0035 pkt 4)
// ============================================================================================================
static Noc nocS(const std::string& data, int m01 = 420) {
  Noc n;
  n.data = data;
  n.zrodlo = 'S';
  n.m01 = m01;
  n.ahi = 2;
  n.wazna = true;
  n.emed = 7;
  n.epapStr = true;
  n.tryb = "ASVAuto";
  return n;
}
static Noc nocA(const std::string& data, bool pelna, int m01) {
  Noc n;
  n.data = data;
  n.zrodlo = 'A';
  n.m01 = m01;
  n.ahi = m01 >= 0 ? 1.0f : NAN;
  if (pelna) n.cmed = 11;
  return n;
}
static void testPierwszenstwo() {
  std::vector<Noc> h;
  // pusta data
  SPRAWDZ(wpiszNocS(h, nocS("2026-11-01")));
  ROWNE_L(h.size(), 1);
  // ta sama noc S drugi raz: bez zmian; zmieniona: zastąpiona
  SPRAWDZ(!wpiszNocS(h, nocS("2026-11-01")));
  SPRAWDZ(wpiszNocS(h, nocS("2026-11-01", 430)) && h[0].m01 == 430);
  // A niepełna bez M-01 (doba tylko z STR w raporcie JSON) → S ją zastępuje
  h = {nocA("2026-11-01", false, -1)};
  SPRAWDZ(wpiszNocS(h, nocS("2026-11-01")) && h[0].zrodlo == 'S');
  // A niepełna z M-01, A pełna, K → S nie wchodzi
  h = {nocA("2026-11-01", false, 300)};
  SPRAWDZ(!wpiszNocS(h, nocS("2026-11-01")) && h[0].zrodlo == 'A');
  h = {nocA("2026-11-01", true, 300)};
  SPRAWDZ(!wpiszNocS(h, nocS("2026-11-01")) && h[0].zrodlo == 'A');
  Noc k = nocA("2026-11-01", false, 300);
  k.zrodlo = 'K';
  h = {k};
  SPRAWDZ(!wpiszNocS(h, nocS("2026-11-01")) && h[0].zrodlo == 'K');
  Noc kBez = k;
  kBez.m01 = -1;
  h = {kBez};
  SPRAWDZ(!wpiszNocS(h, nocS("2026-11-01")) && h[0].zrodlo == 'K');
  // A częściowa z M-01 zostaje (S tylko w miejsce A bez czasu z maską)
  Noc cz = nocA("2026-11-01", true, 200);
  cz.czesciowa = true;
  h = {cz};
  SPRAWDZ(!wpiszNocS(h, nocS("2026-11-01")) && h[0].zrodlo == 'A');
  // zła data albo inne źródło: nic
  h.clear();
  Noc zla = nocS("2026-13-01");
  SPRAWDZ(!wpiszNocS(h, zla) && h.empty());
  Noc a = nocS("2026-11-01");
  a.zrodlo = 'A';
  SPRAWDZ(!wpiszNocS(h, a) && h.empty());
  // w drugą stronę: pełna A wypiera S, niepełna A i A częściowa nie
  h = {nocS("2026-11-01")};
  bool zost = false;
  SPRAWDZ(!wpiszNocAB(h, nocA("2026-11-01", false, -1), false, false, &zost) && zost && h[0].zrodlo == 'S');
  zost = false;
  SPRAWDZ(!wpiszNocAB(h, nocA("2026-11-01", false, 300), false, false, &zost) && zost && h[0].zrodlo == 'S');
  Noc czA = nocA("2026-11-01", true, 100);
  czA.czesciowa = true;
  zost = false;
  SPRAWDZ(!wpiszNocAB(h, czA, false, true, &zost) && zost && h[0].zrodlo == 'S');
  SPRAWDZ(wpiszNocAB(h, nocA("2026-11-01", true, 300), false, true, &zost) && !zost && h[0].zrodlo == 'A');
  SPRAWDZ(!h[0].epapStr && std::isnan(h[0].emed));  // pola S nie przechodzą do nocy z AirBridge
  // karta wypiera S i dostaje jej pola, których karta nie ma (jak przy niepełnej A)
  h = {nocS("2026-11-01")};
  h[0].h = 1.5f;
  Noc kk = k;
  kk.emed = 8;
  SPRAWDZ(wpiszImport(h, kk) == WynikImportu::ZASTAPIONA && h[0].zrodlo == 'K');
  BLISKO(h[0].h, 1.5);
  BLISKO(h[0].emed, 8);
  SPRAWDZ(!h[0].epapStr);
  // S przyjęta, potem pełna A tej daty z dyrygenta (pierwszeństwo w obie strony), potem S znowu nie wchodzi
  h.clear();
  SPRAWDZ(wpiszNocS(h, nocS("2026-11-02")));
  SPRAWDZ(wpiszNocAB(h, nocA("2026-11-02", true, 400), false, true));
  SPRAWDZ(!wpiszNocS(h, nocS("2026-11-02")) && h[0].zrodlo == 'A');
  // plan pobrań przy nocy S: starsza doba bez sesji na liście nie wraca; z sesjami wraca (pełny raport EDF wygrywa);
  // ostatnia noc zawsze; `stale` raz jeszcze
  const long d1 = nrDnia("2026-11-01") - 1;
  std::vector<Noc> hs = {nocS("2026-11-01"), nocS("2026-11-05")};
  std::vector<DzienListy> l(1);
  l[0].day = d1;
  l[0].sesje = 0;
  SPRAWDZ(planPobrania(l, hs, {}, "2026-11-05", false, 10).empty());
  l[0].sesje = -1;
  SPRAWDZ(planPobrania(l, hs, {}, "2026-11-05", false, 10).empty());
  l[0].sesje = 2;
  ROWNE_L(planPobrania(l, hs, {}, "2026-11-05", false, 10).size(), 1);
  l[0].sesje = 0;
  l[0].stale = true;
  ProbyDob pd;
  ROWNE_L(planPobrania(l, hs, {}, "2026-11-05", false, 10, &pd, 0).size(), 1);
  std::vector<DzienListy> ost(1);
  ost[0].day = nrDnia("2026-11-05") - 1;
  ost[0].sesje = 0;
  ROWNE_L(planPobrania(ost, hs, {}, "2026-11-05", false, 10).size(), 1);
}

// ============================================================================================================
// (e) plik nocy: `S` w polu źródła, wersja 4 (ADR-0022)
// ============================================================================================================
static const char* POLA_V3 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb;m18";
static void testFormatPliku() {
  Noc n;
  SPRAWDZ(parsujNoc("2026-11-01;1;420;2.00;;;6.00;24.00;7.00;8.00;;;ASVAuto/6/10/3/15;S", 'A', n) && n.zrodlo == 'S');
  SPRAWDZ(!parsujNoc("2026-11-01;1;420;2.00;;;6.00;24.00;7.00;8.00;;;ASVAuto/6/10/3/15;X", 'A', n));
  const FormatPliku* f = formatPliku(rejestrFormatow(), "noce");
  SPRAWDZ(f && f->wersja() == WERSJA_NOCY && WERSJA_NOCY == 4);
  if (f && f->wersja() >= 4) ROWNE_L(f->pola[3].size(), POLA_NOCY_V3);  // wersja 4 bez nowych pól
  // migracja 3 → 4: kopia `.v3`, nagłówek 4, wiersze bez zmian
  Dziennik d;
  AtrapaPlikow pl(d);
  pl.montujeSie = true;
  pl.montowane = true;
  Noc a = nocS("2026-11-01");
  std::string w = wierszNocyV3(a);
  std::string v3 = std::string("#cpap-eink noce 3\n") + POLA_V3 + "\n" + w + "\n#koniec 1\n";
  pl.pliki["/dane/noce/2026.csv"] = v3;
  std::vector<std::string> log;
  {
    Magazyn m(pl, [&](const std::string& s) { log.push_back(s); });
    SPRAWDZ(m.migrujWMiejscu());
    ROWNE(pl.tresc("/dane/noce/2026.csv"), std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n" + w + "\n#koniec 1\n");
    ROWNE(pl.tresc("/dane/noce/2026.csv.v3"), v3);
  }
  // starszy firmware (zna wersję 3) widzi plik wersji 4 jako nowszy: tylko do odczytu, bez zapisu (ADR-0022 pkt 4)
  std::vector<FormatPliku> stary = rejestrFormatow();
  for (auto& x : stary)
    if (x.nazwa == "noce") x.pola.resize(3);
  Magazyn ms(pl, [&](const std::string& s) { log.push_back(s); }, stary);
  OdczytPliku o = ms.czytaj("noce", "/dane/noce/2026.csv");
  SPRAWDZ(o.stan == StanPliku::NOWSZY && o.wersja == 4);
  SPRAWDZ(ms.tylkoDoOdczytu("/dane/noce/2026.csv"));
  SPRAWDZ(!ms.zapisz("noce", "/dane/noce/2026.csv", {w}));
  Noc z;
  SPRAWDZ(o.wiersze.size() == 1 && parsujNocV3(o.wiersze[0], z) && z.zrodlo == 'S');
}

// ============================================================================================================
// (f) /api/sessions, ścieżki i limity
// ============================================================================================================
static void testSesje() {
  InfoStr i;
  SPRAWDZ(parsujSesjeStr("{\"sessions\":[{\"day\":\"20261108\",\"start\":\"2026-11-09T01:06:35\",\"end\":\"2026-11-09T09:24:35\","
                         "\"closed\":true,\"files\":[{\"path\":\"/DATALOG/20261108/20261109_010635_BRP.edf\",\"size\":2990020}]}],"
                         "\"str\":{\"path\":\"/STR.edf\",\"size\":134200,\"modified\":1794212706,\"etag\":\"\\\"8ee9f250\\\"\"},"
                         "\"next_cursor\":null}",
                         i));
  SPRAWDZ(i.jest);
  ROWNE(i.etag, "\"8ee9f250\"");
  BLISKO(i.rozmiar, 134200);
  SPRAWDZ(parsujSesjeStr("{\"sessions\":[],\"str\":null,\"next_cursor\":null}", i) && !i.jest && i.etag.empty());
  SPRAWDZ(parsujSesjeStr("{\"sessions\":[]}", i) && !i.jest);
  SPRAWDZ(parsujSesjeStr("{\"sessions\":[],\"str\":{\"size\":10}}", i) && i.jest && i.etag.empty());
  SPRAWDZ(!parsujSesjeStr("{\"str\":{\"etag\":\"x\"}}", i));  // bez `sessions`
  SPRAWDZ(!parsujSesjeStr("{\"sessions\":[],\"str\":\"x\"}", i));
  SPRAWDZ(!parsujSesjeStr("{\"error\":\"catalog_busy\"}", i));
  SPRAWDZ(!parsujSesjeStr("{\"sessions\":[", i));
  SPRAWDZ(!parsujSesjeStr("", i));
  ROWNE(sciezkaSesjiStr(), "/api/sessions?order=desc&limit=1");
  ROWNE(sciezkaPobraniaStr(), "/api/storage/download?path=/STR.edf");
  ROWNE_L(limitOdpowiedziAB(sciezkaPobraniaStr()), 1024 * 1024);
  ROWNE_L(limitOdpowiedziAB(sciezkaSesjiStr()), 16 * 1024);
  ROWNE_L(limitOdpowiedziAB(sciezkaNocy(20000)), AB_MAKS_BAJTOW_RAPORTU);
}

// ============================================================================================================
// (g) dyrygent: pobieranie STR w wybudzeniu z listą nocy
// ============================================================================================================
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS =
    "{\"version\":\"v2.3.1\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"srn\":\"SYN00000001\",\"mode\":\"ASVAuto\","
    "\"system\":\"IDLE\",\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static std::string raport(long day) {
  return "{\"day\":" + std::to_string(day) +
         ",\"present\":true,\"partial\":false,\"used_seconds\":25200,\"ahi\":1.4,\"events_known\":true,"
         "\"session_count\":1,\"counts\":{\"oa\":1,\"ca\":5,\"h\":4},\"channels\":[{\"id\":\"Press\",\"unit\":\"cmH2O\","
         "\"median\":9.8,\"p95\":13.6,\"source\":\"edf\"},{\"id\":\"Leak\",\"unit\":\"L/min\",\"median\":2.4,\"p95\":9.6,"
         "\"source\":\"edf\"}],\"settings\":{\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"value\":\"ASVAuto\","
         "\"option\":8}]}}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}
static std::string sesje(const std::string& etag) {
  return "{\"sessions\":[],\"str\":{\"path\":\"/STR.edf\",\"size\":1,\"modified\":1,\"etag\":\"\\\"" + etag +
         "\\\"\"},\"next_cursor\":null}";
}
struct PanelS {
  Panel p;
  PanelS() {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.pliki.montujeSie = true;
    p.pliki.montowane = true;
    p.pliki.pliki["/dane/noce/2026.csv"] = std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n#koniec 0\n";
    // historia ze STR bez trybu porównawczego (domyślny od ADR-0036 „Zmiana 2026-11-09” pyta też o listę sesji doby;
    // STR wspólny z nocą z plików: test/testy_porownanie.cpp)
    p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  }
};
static void wybudz(PanelS& s, uint32_t t, PowodWybudzenia pw) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  if (pw == PowodWybudzenia::START) s.p.start.stan = StanRtc();
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.p.siec.polaczonaTeraz = false;
  s.p.siec.okno = false;
  s.p.siec.zadania.clear();
  s.p.d.z.clear();
  s.p.port.log.clear();
  Dyrygent dy(s.p.adaptery(), KONF);
  dy.wybudzenie();
}
static int zadan(const PanelS& s, const std::string& sc) {
  int n = 0;
  for (auto& u : s.p.siec.zadania) n += AtrapaSieci::sciezka(u).compare(0, sc.size(), sc) == 0;
  return n;
}
static int nocyS(const PanelS& s) {
  int n = 0;
  for (auto& l : dziel(s.p.pliki.tresc("/dane/noce/2026.csv"), '\n')) {
    std::vector<std::string> f = dziel(l, ';');
    n += f.size() > 14 && f[13] == "S";
  }
  return n;
}
// etag z zapisu NVS `{etag}|{liczba nocy S}`
static std::string etag(PanelS& s) {
  std::string t = s.p.nvs.tekst("cpap", NVS_KLUCZ_STR_ETAG, "");
  return t.substr(0, t.rfind('|'));
}
static const long DZIS4 = nrDnia("2026-11-04") - 1;  // doba nocy z poranka 4.11
// STR z 10 dobami do nocy z poranka 4.11 (pierwsza noc 26.10)
static std::string strDziesiec() {
  std::vector<DobaT> d;
  for (int i = 0; i < 10; i++) d.push_back(dobaAsvAuto(400 + i));
  return strTestowy(DZIS4 - 9, d);
}
static void odpowiedzi(PanelS& s, long dzien, int kodSesji, const std::string& etagSesji, const std::string& str) {
  s.p.siec.odpowiedzi.clear();
  s.p.siec.dodaj("/api/status", 200, STATUS);
  s.p.siec.dodaj(sciezkaListy(), 200, lista(dzien, dzien));
  s.p.siec.dodaj(sciezkaNocy(dzien), 200, raport(dzien));
  s.p.siec.dodaj("/api/sessions", kodSesji, kodSesji == 200 ? sesje(etagSesji) : "{\"error\":\"catalog_busy\"}");
  s.p.siec.dodaj("/api/storage/download", 200, str);
}

static void testDyrygentPobieraStr() {
  PanelS s;
  odpowiedzi(s, DZIS4, 200, "aaaa0001", strDziesiec());
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE_L(zadan(s, "/api/sessions?order=desc&limit=1"), 1);
  ROWNE_L(zadan(s, "/api/storage/download?path=/STR.edf"), 1);
  ROWNE_L(nocyS(s), 9);  // 10 dób: 9 nocy 'S', noc z poranka 4.11 z AirBridge (pełna) wygrywa
  ROWNE(etag(s), "\"aaaa0001\"");
  SPRAWDZ(zawiera(s.p.port.log, "STR: "));
  std::string t = s.p.pliki.tresc("/dane/noce/2026.csv");
  SPRAWDZ(t.compare(0, 18, "#cpap-eink noce 4\n") == 0);
  SPRAWDZ(t.find("2026-10-26;1;400;") != std::string::npos);
  SPRAWDZ(t.find("2026-11-04;1;420;") != std::string::npos);  // noc z AirBridge, nie ze STR (409 min)
  // następny ranek, ten sam etag: tylko /api/sessions, bez pobrania pliku
  odpowiedzi(s, DZIS4 + 1, 200, "aaaa0001", strDziesiec());
  wybudz(s, ep(2026, 11, 5, 7, 0), PowodWybudzenia::ZEGAR);
  ROWNE_L(zadan(s, "/api/sessions"), 1);
  ROWNE_L(zadan(s, "/api/storage"), 0);
  ROWNE_L(nocyS(s), 9);
  ROWNE(s.p.nvs.tekst("cpap", NVS_KLUCZ_STR_ETAG, ""), "\"aaaa0001\"|9");
  // kolejny ranek, nowy etag: pobranie; noce bez zmian → historia bez zmian, nowy etag zapisany
  odpowiedzi(s, DZIS4 + 2, 200, "aaaa0002", strDziesiec());
  wybudz(s, ep(2026, 11, 6, 7, 0), PowodWybudzenia::ZEGAR);
  ROWNE_L(zadan(s, sciezkaListy()), 1);
  ROWNE_L(zadan(s, "/api/storage"), 1);
  ROWNE(etag(s), "\"aaaa0002\"");
  ROWNE_L(nocyS(s), 9);
  // pliki nocy zgubione (historia bez nocy S), etag ten sam → plik od nowa i noce S wracają
  s.p.pliki.pliki["/dane/noce/2026.csv"] = std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n#koniec 0\n";
  odpowiedzi(s, DZIS4 + 3, 200, "aaaa0002", strDziesiec());
  wybudz(s, ep(2026, 11, 7, 7, 0), PowodWybudzenia::START);
  ROWNE_L(zadan(s, "/api/storage"), 1);
  ROWNE_L(nocyS(s), 10);  // także noc 4.11, bo jej rekord z AirBridge zginął razem z plikiem
  SPRAWDZ(zawiera(s.p.port.log, "nie ma nocy ze STR"));
  // STR bez żadnej nocy z terapią: zapis z liczbą 0, ten sam etag nie pobiera pliku drugi raz
  PanelS z;
  std::vector<DobaT> bez = {dobaAsvAuto(0), dobaAsvAuto(0)};
  odpowiedzi(z, DZIS4, 200, "ffff0001", strTestowy(DZIS4 - 5, bez));
  wybudz(z, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE(z.p.nvs.tekst("cpap", NVS_KLUCZ_STR_ETAG, ""), "\"ffff0001\"|0");
  odpowiedzi(z, DZIS4 + 1, 200, "ffff0001", strTestowy(DZIS4 - 5, bez));
  wybudz(z, ep(2026, 11, 5, 7, 0), PowodWybudzenia::ZEGAR);
  ROWNE_L(zadan(z, "/api/storage"), 0);
}

static void testDyrygentBledyStr() {
  // 404: AirBridge < 2.3.1, bez pobrania, bez etagu, noc z AirBridge zapisana, bez E-03
  {
    PanelS s;
    odpowiedzi(s, DZIS4, 404, "", strDziesiec());
    wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    ROWNE_L(zadan(s, "/api/sessions"), 1);
    ROWNE_L(zadan(s, "/api/storage"), 0);
    ROWNE(etag(s), "");
    ROWNE_L(nocyS(s), 0);
    SPRAWDZ(zawiera(s.p.port.log, "STR: /api/sessions 404"));
    SPRAWDZ(!zawiera(s.p.port.log, "E-03"));
    SPRAWDZ(s.p.pliki.tresc("/dane/noce/2026.csv").find("2026-11-04;1;420;") != std::string::npos);
  }
  // 503 (katalog zajęty): bez pobrania; w następnym wybudzeniu z listą nocy próba od nowa i pobranie
  {
    PanelS s;
    odpowiedzi(s, DZIS4, 503, "", strDziesiec());
    wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    ROWNE_L(zadan(s, "/api/sessions"), 1);  // jedna próba, bez ponowień R-AW-01
    ROWNE_L(zadan(s, "/api/storage"), 0);
    ROWNE(etag(s), "");
    odpowiedzi(s, DZIS4 + 1, 200, "bbbb0001", strDziesiec());
    wybudz(s, ep(2026, 11, 5, 7, 0), PowodWybudzenia::ZEGAR);
    ROWNE_L(zadan(s, "/api/storage"), 1);
    ROWNE(etag(s), "\"bbbb0001\"");
    ROWNE_L(nocyS(s), 9);
  }
  // plik nieczytelny (ucięty): etag nie zapisany, historia bez nocy S; następnym razem pobranie od nowa
  {
    PanelS s;
    std::string b = strDziesiec();
    odpowiedzi(s, DZIS4, 200, "cccc0001", b.substr(0, b.size() - 7));
    wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    ROWNE_L(zadan(s, "/api/storage"), 1);
    ROWNE(etag(s), "");
    ROWNE_L(nocyS(s), 0);
    SPRAWDZ(zawiera(s.p.port.log, "plik ucięty"));
    odpowiedzi(s, DZIS4 + 1, 200, "cccc0001", b);
    wybudz(s, ep(2026, 11, 5, 7, 0), PowodWybudzenia::ZEGAR);
    ROWNE_L(zadan(s, "/api/storage"), 1);
    ROWNE(etag(s), "\"cccc0001\"");
  }
  // pobranie STR nieudane (HTTP 500) i STR ponad limit: bez etagu
  {
    PanelS s;
    odpowiedzi(s, DZIS4, 200, "dddd0001", "");
    s.p.siec.odpowiedzi["/api/storage/download"].clear();
    s.p.siec.dodaj("/api/storage/download", 500, "busy");
    wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    ROWNE(etag(s), "");
    odpowiedzi(s, DZIS4 + 1, 200, "dddd0001", std::string(AB_MAKS_BAJTOW_STR + 1, '0'));
    wybudz(s, ep(2026, 11, 5, 7, 0), PowodWybudzenia::ZEGAR);
    ROWNE_L(zadan(s, "/api/storage"), 1);
    ROWNE(etag(s), "");
  }
  // zapis historii odrzucony (pełna pamięć): etag nie zapisany, więc STR wraca w następnym wybudzeniu
  {
    PanelS s;
    odpowiedzi(s, DZIS4, 200, "eeee0001", strDziesiec());
    s.p.pliki.odmawiaZapisu = true;
    wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    ROWNE(etag(s), "");
  }
  // `str` = null (AirBridge bez pliku STR): bez pobrania
  {
    PanelS s;
    odpowiedzi(s, DZIS4, 200, "", strDziesiec());
    s.p.siec.odpowiedzi["/api/sessions"].clear();
    s.p.siec.dodaj("/api/sessions", 200, "{\"sessions\":[],\"str\":null,\"next_cursor\":null}");
    wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    ROWNE_L(zadan(s, "/api/sessions"), 1);
    ROWNE_L(zadan(s, "/api/storage"), 0);
  }
}

int main(int argc, char** argv) {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  std::string kat = argc > 1 ? argv[1] : "";
  testNaglowekIProbki();
  testBledyEdf();
  if (!kat.empty()) {
    testZgodnoscZNoce(kat);
    testNoceZSyntetycznego(kat);
  } else {
    // tools/jakosc/sprawdz.sh (sanitizery, pokrycie) woła programy bez argumentów; testy.sh podaje katalog zawsze
    printf("  zgodność z noce.py pominięta: brak katalogu ze STR.edf i wzorcem (testy.sh go podaje)\n");
  }
  testWartosciIJednostki();
  testPierwszenstwo();
  testFormatPliku();
  testSesje();
  testDyrygentPobieraStr();
  testDyrygentBledyStr();
  printf("testy_str_edf: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
