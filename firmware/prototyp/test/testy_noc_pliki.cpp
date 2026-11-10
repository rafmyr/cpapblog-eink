// SPDX-License-Identifier: GPL-3.0-or-later
// Testy nocy liczonej na panelu z plików sesji (src/noc_pliki.h, ADR-0036):
//  (a) zgodność z tools/kalibracja/noce.py (`noc_z_katalogu`, `metryki`) na kartach syntetycznych
//      z tools/kalibracja/syntetyczna.py i wzorcu tools/kalibracja/noc_wzorzec.py (katalog z testy.sh);
//  (b) tryb panelu (bez BRP i SAD) wobec pełnego odczytu;
//  (c) przypadki brzegowe: nazwy, `.part`, brak plików, pliki uszkodzone i ucięte (pod ASan/UBSan), wiele sesji, sesja
//      przez północ, doba od południa, R-DANE-27, lista sesji, ścieżki, metryki z JSON, klucze konfiguracji;
//  (d) dyrygent z atrapami: noc z plików przy `noc_zrodlo=plik`, zapas JSON (404, 503, `.part`, plik zniknął, plik
//      nieczytelny, doba bez sesji), `noc_zrodlo=json` bez plików, sloty statusowe bez pobrań, pierwszeństwo nad nocami S i K,
//      `noc_json`, stronicowanie listy sesji.
// Dane wyłącznie syntetyczne (bez związku z żadną osobą).
// Uruchomienie: tools/prototyp/testy.sh (program testy_noc_pliki z katalogiem kart i wzorców; bez argumentu części (a)
// i (b) są pominięte, reszta działa, np. pod sanitizerami z tools/jakosc/sprawdz.sh).
#include <dirent.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "edf.h"
#include "magazyn.h"
#include "noc.h"
#include "noc_pliki.h"
#include "slot_dzienny.h"
#include "str.h"
#include "wspolne.h"
#include "wybudzenie.h"

using namespace noc_pliki;

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
    if (std::fabs(_a - _b) < 1e-3 || (std::isnan(_a) && std::isnan(_b))) ok++;                               \
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
static std::vector<std::string> pliki(const std::string& kat) {
  std::vector<std::string> w;
  DIR* d = opendir(kat.c_str());
  if (!d) return w;
  while (dirent* e = readdir(d))
    if (e->d_name[0] != '.') w.push_back(e->d_name);
  closedir(d);
  return w;
}
static bool rowneLiczby(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return std::isnan(a) && std::isnan(b);
  return std::fabs(a - b) <= 1e-6 * std::max(1.0, std::fabs(b));
}
static double liczbaWz(const std::string& t) { return t == "brak" ? NAN : std::strtod(t.c_str(), nullptr); }

// ============================================================================================================
// budowa plików EDF w teście (jak tools/kalibracja/syntetyczna.py)
// ============================================================================================================
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
// start: sekundy cywilne; rekordy[r][s] = surowe bajty sygnału s w rekordzie r (dopełniane zerami)
static std::string budujEdf(long long start, double czasRekordu, const std::vector<SygT>& syg,
                            const std::vector<std::vector<std::string>>& rekordy, const std::string& zastrz = "") {
  long dzien = (long)(start / 86400);
  long sek = (long)(start % 86400);
  std::string d = dataZNr(dzien);
  char g[16];
  snprintf(g, sizeof g, "%02ld.%02ld.%02ld", sek / 3600, sek / 60 % 60, sek % 60);
  size_t ns = syg.size();
  std::string h = pole("0", 8) + pole("X", 80) + pole("Startdate X", 80) +
                  pole(d.substr(8, 2) + "." + d.substr(5, 2) + "." + d.substr(2, 2), 8) + pole(g, 8) +
                  pole(std::to_string(256 + 256 * ns), 8) + pole(zastrz, 44) + pole(std::to_string(rekordy.size()), 8) +
                  pole(num(czasRekordu), 8) + pole(std::to_string(ns), 4);
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
    for (size_t i = 0; i < ns; i++) {
      std::string b = i < r.size() ? r[i] : "";
      b.resize((size_t)syg[i].probki * 2, '\0');
      h += b;
    }
  return h;
}
static std::string probki(const std::vector<long>& v) {
  std::string b;
  for (long x : v) {
    uint16_t u = (uint16_t)(x & 0xFFFF);
    b += (char)(u & 0xFF);
    b += (char)(u >> 8);
  }
  return b;
}
static long cyf(const SygT& s, double v) {
  if (std::isnan(v)) return -1;  // poza zakresem cyfrowym: próbka niepoprawna
  return std::lround((double)s.cyfMin + (v - s.fizMin) * (double)(s.cyfMax - s.cyfMin) / (s.fizMax - s.fizMin));
}
static const SygT CRC = {"Crc16", "", -32768, 32767, -32768, 32767, 1};
static const std::vector<SygT> PLD_SYG = {
    {"MaskPress.2s", "cmH2O", 0, 30, 0, 1500, 30}, {"Press.2s", "cmH2O", 0, 30, 0, 1500, 30},
    {"EprPress.2s", "cmH2O", 0, 30, 0, 1500, 30},  {"Leak.2s", "L/s", 0, 2, 0, 100, 30},
    {"RespRate.2s", "bpm", 0, 50, 0, 250, 30},     {"TidVol.2s", "L", 0, 4, 0, 200, 30},
    {"MinVent.2s", "L/min", 0, 30, 0, 240, 30},    {"Snore.2s", "", 0, 5, 0, 250, 30},
    {"FlowLim.2s", "", 0, 1, 0, 100, 30},          {"TgtVent.2s", "L/min", 0, 30, 0, 240, 30},
    CRC,
};
using Wartosci = std::map<std::string, double>;
static Wartosci domyslne() {
  return {{"MaskPress.2s", 12}, {"Press.2s", 12}, {"EprPress.2s", 8}, {"Leak.2s", 0.1}, {"RespRate.2s", 15},
          {"TidVol.2s", 0.46}, {"MinVent.2s", 7.5}, {"Snore.2s", 0.1}, {"FlowLim.2s", 0.05}, {"TgtVent.2s", 6}};
}
// PLD: rekordy 60 s, 30 próbek; profil(minuta) nadpisuje wartości fizyczne (NAN = próbka niepoprawna)
static std::string pld(long long start, int minuty, std::function<void(int, Wartosci&)> profil = nullptr) {
  std::vector<std::vector<std::string>> rek;
  for (int i = 0; i < minuty; i++) {
    Wartosci w = domyslne();
    if (profil) profil(i, w);
    std::vector<std::string> r;
    for (auto& s : PLD_SYG) r.push_back(s.etykieta == "Crc16" ? probki({0}) : probki(std::vector<long>(30, cyf(s, w[s.etykieta]))));
    rek.push_back(r);
  }
  return budujEdf(start, 60, PLD_SYG, rek);
}
// Plik adnotacji (EVE, CSL): jedno zdarzenie na rekord; trwanie < 0 = bez pola trwania
struct Zd {
  long long t;
  double trwanie;
  std::string tekst;
};
static std::string adnotacje(long long start, const std::vector<Zd>& z) {
  std::vector<SygT> syg = {{"EDF Annotations", "", 0, 1, -32768, 32767, 40}, CRC};
  std::vector<std::vector<std::string>> rek;
  for (auto& e : z) {
    long long on = e.t - start;
    std::string tal = std::string("+0\x14\x14", 4) + std::string(1, '\0') + (on >= 0 ? "+" : "-") +
                      std::to_string(on >= 0 ? on : -on);
    if (e.trwanie >= 0) tal += "\x15" + num(e.trwanie);
    tal += "\x14" + e.tekst + "\x14" + std::string(1, '\0');
    rek.push_back({tal, probki({0})});
  }
  return budujEdf(start, 0, syg, rek, "EDF+D");
}
static long long cyw(int r, int mi, int d, int h, int m, int s = 0) {
  char b[16];
  snprintf(b, sizeof b, "%04d-%02d-%02d", r, mi, d);
  return (long long)nrDnia(b) * 86400LL + h * 3600 + m * 60 + s;
}
static std::string prefiks(long long t) {
  std::string d = dataZNr((long)(t / 86400));
  long s = (long)(t % 86400);
  char b[32];
  snprintf(b, sizeof b, "%s%s%s_%02ld%02ld%02ld", d.substr(0, 4).c_str(), d.substr(5, 2).c_str(), d.substr(8, 2).c_str(),
           s / 3600, s / 60 % 60, s % 60);
  return b;
}
using PlikiDoby = std::map<std::string, std::string>;  // nazwa → treść
static WynikNocy policz(long dzien, const PlikiDoby& p, const std::set<std::string>& bezTresci = {},
                        LicznikNocy* zewn = nullptr) {
  LicznikNocy l(dzien);
  LicznikNocy& L = zewn ? *zewn : l;
  for (auto& x : p) L.dodajNazwe(x.first);
  for (auto& n : L.kolejnosc(true, true)) {
    if (bezTresci.count(n)) continue;
    auto it = p.find(n);
    if (it != p.end()) L.dodajPlik(n, (const uint8_t*)it->second.data(), it->second.size());
  }
  return L.wynik();
}

// ============================================================================================================
// (a) zgodność z noce.py
// ============================================================================================================
static void testZgodnosc(const std::string& karta, const std::string& plikWzorca) {
  std::map<long, std::map<std::string, std::string>> wz;
  for (auto& l : dziel(czytajPlik(plikWzorca), '\n')) {
    std::vector<std::string> f = dziel(l, ';');
    if (f.size() == 4 && f[0] == "N") wz[std::atol(f[1].c_str())][f[2]] = f[3];
  }
  SPRAWDZ(!wz.empty());
  Str st;
  SPRAWDZ(czytajStr(czytajPlik(karta + "/STR.edf"), st) == edf::Blad::BRAK);
  int nocy = 0;
  for (auto& d : wz) {
    long dzien = d.first;
    auto& w = d.second;
    std::string kat = karta + "/DATALOG/" + dzienAB(dzien);
    PlikiDoby p;
    for (auto& n : pliki(kat)) p[n] = czytajPlik(kat + "/" + n);
    WynikNocy r = policz(dzien, p);
    nocy++;
    auto bool01 = [](bool b) { return std::string(b ? "1" : "0"); };
    auto liczba = [&](const std::string& k, double v) {
      if (!rowneLiczby(v, liczbaWz(w[k]))) {
        zle++;
        printf("BŁĄD zgodność %s doba %ld %s: panel %.9g, noce.py %s\n", karta.c_str(), dzien, k.c_str(), v, w[k].c_str());
      } else {
        ok++;
      }
    };
    auto calk = [&](const std::string& k, long v) { liczba(k, v < 0 ? NAN : (double)v); };
    std::string ctx = "doba " + std::to_string(dzien) + " ";
    ROWNE(ctx + bool01(r.czesciowa), ctx + w["partial"]);
    ROWNE(ctx + bool01(r.zdarzeniaZnane), ctx + w["events_known"]);
    ROWNE(ctx + bool01(r.sesjeObecne || st.doby.count(dzien)), ctx + w["present"]);
    ROWNE(ctx + (r.zdarzeniaZnane ? bool01(r.klasyfikacja) : "brak"), ctx + w["klasyfikacja_bezdechow"]);
    liczba("used_seconds", r.sekundy);
    liczba("ahi", r.ahi);
    liczba("csr_seconds", r.csrS);
    ROWNE(ctx + (r.csrZrodlo.empty() ? "brak" : r.csrZrodlo), ctx + w["csr_zrodlo"]);
    liczba("leak_high_seconds", r.przeciekWysokiS);
    liczba("leak_27_seconds", r.przeciek27S);
    calk("session_count", (long)r.sesje.size());
    calk("oa", r.oa);
    calk("ca", r.ca);
    calk("ua", r.ua);
    calk("h", r.h);
    calk("rera", r.rera);
    for (size_t i = 0; i < r.sesje.size(); i++) {
      const SesjaNocy& s = r.sesje[i];
      std::string k = "sesja." + std::to_string(i) + ".";
      liczba(k + "start_ms", s.maZakres ? (double)(long long)(s.startS * 1000) : NAN);
      liczba(k + "end_ms", s.maZakres ? (double)(long long)(s.koniecS * 1000) : NAN);
    }
    auto ds = st.doby.find(dzien);
    for (const char* id : {"Press", "EprPress", "MaskPress", "Leak", "MinVent", "RespRate", "TidVol", "SpO2", "Pulse",
                           "FlowLim", "Snore", "TgtVent"}) {
      std::string k = std::string("kanal.") + id + ".";
      auto it = r.kanaly.find(id);
      if (it != r.kanaly.end()) {
        ROWNE(ctx + id + " edf", ctx + id + " " + w[k + "source"]);
        liczba(k + "median", it->second.med);
        liczba(k + "p95", it->second.p95);
        liczba(k + "min", it->second.min);
        liczba(k + "max", it->second.max);
      } else {
        double med = NAN, p95 = NAN, mx = NAN;
        if (ds != st.doby.end()) {
          med = statystykaStr(st, ds->second, id, "50");
          p95 = statystykaStr(st, ds->second, id, "95");
          mx = statystykaStr(st, ds->second, id, "Max");
        }
        bool zStr = !std::isnan(med) || !std::isnan(p95) || !std::isnan(mx);
        ROWNE(ctx + id + (zStr ? " str" : " brak"), ctx + id + " " + w[k + "source"]);
        liczba(k + "median", med);
        liczba(k + "p95", p95);
        liczba(k + "max", mx);
      }
    }
    bool pressEdf = r.kanaly.count("Press") > 0;
    liczba("ps.median", r.ps.jest ? r.ps.med : NAN);
    liczba("ps.p95", r.ps.jest ? r.ps.p95 : NAN);
    SPRAWDZ(pressEdf || w["ps.median"] == "brak");
    int m17 = -1;
    double m18 = NAN;
    m17m18(r, Parametry(), m17, m18);
    calk("m17", m17);
    liczba("m18_start_min", m18);
    // rekord nocy panelu
    NocAB n = nocZWyniku(r, &st, "");
    ROWNE(ctx + (n.tryb.empty() ? "brak" : n.tryb), ctx + w["tryb"]);
    ROWNE(ctx + bool01(n.noc.wazna), ctx + w["wazna"]);
    ROWNE_L(n.noc.m01, std::isnan(r.sekundy) ? -1 : std::lround(r.sekundy / 60.0));
    if (pressEdf && r.ps.jest) BLISKO(n.noc.psmed, r.ps.med);
    SPRAWDZ(!n.noc.epapStr || !r.kanaly.count("EprPress"));
    BLISKO(n.noc.m18, std::isnan(m18) ? -1 : std::floor(m18));
    ROWNE_L(n.noc.m17, m17);
    // ustawienia ze STR tej doby
    if (ds != st.doby.end()) {
      Json u = ustawieniaStr(st, ds->second);
      for (auto& f : u["fields"].el) ROWNE(ctx + f["value"].s, ctx + w["ustawienie." + f["tag"].s]);
    }
    // (b) tryb panelu: bez BRP i SAD; te same metryki nocy (poza SpO2, tętnem i granicami sesji o sekundy)
    std::set<std::string> bez;
    for (auto& x : p)
      if (rodzajPliku(x.first) == Rodzaj::BRP || rodzajPliku(x.first) == Rodzaj::SAD) bez.insert(x.first);
    WynikNocy q = policz(dzien, p, bez);
    ROWNE(ctx + bool01(q.czesciowa), ctx + bool01(r.czesciowa));
    BLISKO(q.sekundy, r.sekundy);
    BLISKO(q.ahi, r.ahi);
    BLISKO(q.csrS, r.csrS);
    BLISKO(q.przeciekWysokiS, r.przeciekWysokiS);
    ROWNE_L(q.sesje.size(), r.sesje.size());
    for (const char* id : {"Press", "EprPress", "Leak", "RespRate", "FlowLim"}) {
      SPRAWDZ(q.kanaly.count(id) == r.kanaly.count(id));
      if (q.kanaly.count(id) && r.kanaly.count(id)) BLISKO(q.kanaly[id].p95, r.kanaly[id].p95);
    }
    BLISKO(q.ps.med, r.ps.med);
    for (size_t i = 0; i < q.sesje.size() && i < r.sesje.size(); i++)
      if (q.sesje[i].maZakres && r.sesje[i].maZakres) SPRAWDZ(std::fabs(q.sesje[i].startS - r.sesje[i].startS) <= 30);
    NocAB nq = nocZWyniku(q, &st, "");
    ROWNE_L(nq.noc.m01, n.noc.m01);
    ROWNE_L(nq.noc.m17, n.noc.m17);
    SPRAWDZ(r.czesciowa || std::abs(nq.noc.m18 - n.noc.m18) <= 1);  // sesja z samym BRP: bez granic w trybie panelu
  }
  printf("  zgodność z noce.py: %s, %d dób\n", karta.c_str(), nocy);
}

// ============================================================================================================
// (c) przypadki brzegowe
// ============================================================================================================
static const long D1 = nrDnia("2026-11-02") - 1;  // doba od południa 1.11, noc z poranka 2.11
static PlikiDoby sesja(long long start, int minuty, std::function<void(int, Wartosci&)> profil = nullptr,
                       const std::vector<Zd>& zd = {}, bool csl = true) {
  PlikiDoby p;
  std::string pr = prefiks(start);
  p[pr + "_PLD.edf"] = pld(start, minuty, profil);
  p[pr + "_EVE.edf"] = adnotacje(start, zd);
  if (csl) p[pr + "_CSL.edf"] = adnotacje(start, {});
  return p;
}

static void testNazwyIPercentyl() {
  std::string pr;
  long long t = 0;
  SPRAWDZ(rodzajPliku("20261109_010635_PLD.edf", &pr, &t) == Rodzaj::PLD && pr == "20261109_010635");
  ROWNE_L(t, cyw(2026, 11, 9, 1, 6, 35));
  SPRAWDZ(rodzajPliku("20261109_010635_pld.EDF") == Rodzaj::PLD);
  SPRAWDZ(rodzajPliku("20261109_010635_EVE.edf") == Rodzaj::EVE);
  SPRAWDZ(rodzajPliku("20261109_010635_CSL.edf") == Rodzaj::CSL);
  SPRAWDZ(rodzajPliku("20261109_010635_BRP.edf") == Rodzaj::BRP);
  SPRAWDZ(rodzajPliku("20261109_010635_SAD.edf") == Rodzaj::SAD);
  SPRAWDZ(rodzajPliku("20261109_010635_PLD.edf.part") == Rodzaj::INNY);
  SPRAWDZ(rodzajPliku("20261109_010635_XYZ.edf") == Rodzaj::INNY);
  SPRAWDZ(rodzajPliku("2026110_010635_PLD.edf") == Rodzaj::INNY);
  SPRAWDZ(rodzajPliku("STR.edf") == Rodzaj::INNY);
  SPRAWDZ(rodzajPliku("") == Rodzaj::INNY);
  SPRAWDZ(rodzajPliku("20261340_010635_PLD.edf", nullptr, &t) == Rodzaj::PLD && t == -1);  // zła data
  ROWNE(nazwaZeSciezki("/DATALOG/20261108/20261109_010635_PLD.edf"), "20261109_010635_PLD.edf");
  ROWNE(nazwaZeSciezki("x.edf"), "x.edf");
  std::map<double, uint64_t> m = {{1, 1}, {2, 1}, {3, 1}, {4, 1}};
  BLISKO(percentylZLicznikow(m, 50), 2);
  BLISKO(percentylZLicznikow(m, 95), 4);
  SPRAWDZ(std::isnan(percentylZLicznikow({}, 50)));
  // P95 na granicy rangi (karta P3 z noce.py): 8550 × 5, 450 × 10 → ranga 8550 → 5
  BLISKO(percentylZLicznikow({{5, 8550}, {10, 450}}, 95), 5);
}

static void testJednaSesja() {
  long long s = cyw(2026, 11, 1, 23, 0);
  std::vector<Zd> zd = {{s + 600, 10, "Obstructive Apnea"}, {s + 1200, 12, "Central Apnea"},
                        {s + 1800, 15, "Hypopnea"},          {s + 2400, 10, "Apnea"},
                        {s + 3000, 8, "RERA"},               {s + 480 * 60, 10, "Obstructive Apnea"},  // na końcu
                        {s + 3600, -1, "Recording starts"}};
  PlikiDoby p = sesja(s, 480,
                      [](int i, Wartosci& w) {
                        w["Press.2s"] = i < 288 ? 12 : 15;  // 60% i 40%
                        w["EprPress.2s"] = 7;
                        if (60 <= i && i < 90) w["Leak.2s"] = 0.5;  // 30 L/min przez 30 min
                      },
                      zd);
  WynikNocy r = policz(D1, p);
  SPRAWDZ(!r.czesciowa && r.zdarzeniaZnane && r.klasyfikacja);
  BLISKO(r.sekundy, 480 * 60);
  ROWNE_L(r.oa, 1);
  ROWNE_L(r.ca, 1);
  ROWNE_L(r.h, 1);
  ROWNE_L(r.ua, 1);
  ROWNE_L(r.rera, 1);
  BLISKO(r.ahi, 4 * 3600.0 / (480 * 60));
  BLISKO(r.przeciekWysokiS, 30 * 60);
  BLISKO(r.kanaly["Press"].med, 12);
  BLISKO(r.kanaly["Press"].p95, 15);
  BLISKO(r.ps.med, 5);
  BLISKO(r.ps.p95, 8);
  ROWNE_L(r.adnotacjeInne["Recording starts"], 1);
  NocAB n = nocZWyniku(r, nullptr, "ASVAuto", "AirCurve", "SYN1");
  SPRAWDZ(n.stan == StanRaportu::KOMPLETNA && n.pelna && n.noc.wazna);
  ROWNE(n.noc.data, "2026-11-02");
  ROWNE_L(n.noc.m01, 480);
  BLISKO(n.noc.cmed, 12);
  BLISKO(n.noc.emed, 7);
  BLISKO(n.noc.psmed, 5);
  BLISKO(n.noc.psp95, 8);
  BLISKO(n.noc.m08, 30);
  BLISKO(n.noc.wmed, 6);   // 0,1 L/s × 60
  BLISKO(n.noc.wp95, 30);  // 30 z 480 min (6%) po 30 L/min
  BLISKO(n.noc.oa, 1.0 / 8);
  ROWNE_L(n.noc.m17, 0);
  ROWNE_L(n.noc.m18, 11 * 60);  // 23:00 = 660 min od 12:00
  ROWNE(n.noc.tryb, "ASVAuto");  // bez STR: tryb ze statusu
  ROWNE(n.noc.ustawienia, "");
  ROWNE(n.noc.pna, "AirCurve");
  SPRAWDZ(n.noc.zrodlo == 'A' && !n.noc.epapStr);
  BLISKO(n.noc.csr, 0);  // CSL bez par: 0 min (znane)
}

static void testBrakiIUszkodzenia() {
  long long s = cyw(2026, 11, 1, 23, 0);
  PlikiDoby p = sesja(s, 300, nullptr, {{s + 600, 10, "Hypopnea"}});
  std::string pr = prefiks(s);
  // brak EVE w ogóle: noc częściowa (zdarzenia nieznane)
  PlikiDoby b = p;
  b.erase(pr + "_EVE.edf");
  WynikNocy r = policz(D1, b);
  SPRAWDZ(r.czesciowa && !r.zdarzeniaZnane && std::isnan(r.ahi));
  SPRAWDZ(nocZWyniku(r, nullptr, "ASVAuto").stan == StanRaportu::CZESCIOWA);
  // EVE na liście, ale bez treści (np. zniknął): częściowa
  r = policz(D1, p, {pr + "_EVE.edf"});
  SPRAWDZ(r.czesciowa && zawiera(r.problemy, "nie przeczytany"));
  // brakPliku(): opis w problemach
  {
    LicznikNocy l(D1);
    for (auto& x : p) l.dodajNazwe(x.first);
    l.brakPliku(pr + "_PLD.edf", "HTTP 404");
    for (auto& n : l.kolejnosc(false, false))
      if (n != pr + "_PLD.edf") l.dodajPlik(n, (const uint8_t*)p[n].data(), p[n].size());
    WynikNocy w = l.wynik();
    SPRAWDZ(w.czesciowa && zawiera(w.problemy, "HTTP 404") && std::isnan(w.sekundy));
  }
  // brak CSL: CSR z EVE (tu bez par) i nie częściowa
  b = p;
  b.erase(pr + "_CSL.edf");
  r = policz(D1, b);
  SPRAWDZ(!r.czesciowa && r.csrZrodlo.empty() && std::isnan(r.csrS));
  // tylko BRP (nazwa bez treści): sesja bez PLD → częściowa, czas nieznany
  b.clear();
  b[pr + "_BRP.edf"] = "";
  b[pr + "_EVE.edf"] = p[pr + "_EVE.edf"];
  r = policz(D1, b, {pr + "_BRP.edf"});
  SPRAWDZ(r.czesciowa && std::isnan(r.sekundy) && r.sesje.size() == 1);
  // `.part` i nazwy obce nie wchodzą
  {
    LicznikNocy l(D1);
    l.dodajNazwe(pr + "_PLD.edf.part");
    l.dodajNazwe("STR.edf");
    SPRAWDZ(l.kolejnosc(true, true).empty());
    WynikNocy w = l.wynik();
    SPRAWDZ(!w.sesjeObecne && !w.czesciowa && std::isnan(w.sekundy));
    SPRAWDZ(!l.dodajPlik("STR.edf", nullptr, 0));
  }
  // ten sam plik drugi raz nie liczy się podwójnie
  {
    LicznikNocy l(D1);
    for (auto& x : p) l.dodajNazwe(x.first);
    for (auto& n : l.kolejnosc(false, false)) SPRAWDZ(l.dodajPlik(n, (const uint8_t*)p[n].data(), p[n].size()));
    SPRAWDZ(!l.dodajPlik(pr + "_PLD.edf", (const uint8_t*)p[pr + "_PLD.edf"].data(), p[pr + "_PLD.edf"].size()));
    BLISKO(l.wynik().sekundy, 300 * 60);
    SPRAWDZ(l.bajtyLicznikow() > 0);
  }
  // zła adnotacja (czas bez znaku) i ujemny czas trwania: problem, zdarzenia nieznane
  for (const std::string& zly : {std::string("12\x14x\x14", 5), std::string("+12\x15-3\x14x\x14", 9)}) {
    std::vector<SygT> syg = {{"EDF Annotations", "", 0, 1, -32768, 32767, 40}, CRC};
    b = p;
    b[pr + "_EVE.edf"] = budujEdf(s, 0, syg, {{zly, probki({0})}}, "EDF+D");
    r = policz(D1, b);
    SPRAWDZ(r.czesciowa && !r.zdarzeniaZnane && zawiera(r.problemy, "zła adnotacja"));
  }
  // PLD ucięty w każdym miejscu, z losowo zmienionymi bajtami: bez wyjścia poza bufor (ASan), częściowa przy błędzie
  const std::string& pl = p[pr + "_PLD.edf"];
  for (size_t n = 0; n < pl.size(); n += 997) {
    b = p;
    b[pr + "_PLD.edf"] = pl.substr(0, n);
    r = policz(D1, b);
    SPRAWDZ(r.czesciowa);
  }
  unsigned los = 12345;
  for (int i = 0; i < 300; i++) {
    std::string x = pl;
    for (int k = 0; k < 8; k++) {
      los = los * 1103515245u + 12345u;
      x[(los >> 8) % x.size()] = (char)(los >> 16);
    }
    b = p;
    b[pr + "_PLD.edf"] = x;
    policz(D1, b);  // tylko brak awarii pod sanitizerami
    std::string e = p[pr + "_EVE.edf"];
    for (int k = 0; k < 4; k++) {
      los = los * 1103515245u + 12345u;
      e[(los >> 8) % e.size()] = (char)(los >> 16);
    }
    b[pr + "_EVE.edf"] = e;
    policz(D1, b);
  }
  // PLD z liczbą rekordów -1 (plik w trakcie zapisu): błąd, częściowa
  b = p;
  std::string x = pl;
  x.replace(236, 8, pole("-1", 8));
  b[pr + "_PLD.edf"] = x;
  SPRAWDZ(policz(D1, b).czesciowa);
  // PLD bez poprawnych próbek ciśnienia: czas zapisu nieznany
  b = p;
  b[pr + "_PLD.edf"] = pld(s, 30, [](int, Wartosci& w) {
    w["Press.2s"] = NAN;
    w["EprPress.2s"] = NAN;
    w["MaskPress.2s"] = NAN;
  });
  r = policz(D1, b);
  SPRAWDZ(std::isnan(r.sekundy) && !r.zdarzeniaZnane);
  // PLD z 0 rekordami: użycie znane, ale 0 s → czas nieznany
  b = p;
  b[pr + "_PLD.edf"] = pld(s, 0);
  r = policz(D1, b);
  SPRAWDZ(std::isnan(r.sekundy));
}

static void testWieleSesjiIDoba() {
  // dwie sesje przez północ 22:30–01:00 i 01:20–06:40, zdarzenie w przerwie się nie liczy
  long long a = cyw(2026, 11, 1, 22, 30), b = cyw(2026, 11, 2, 1, 20);
  PlikiDoby p = sesja(a, 150, nullptr, {{a + 600, 10, "Hypopnea"}, {a + 160 * 60, 10, "Hypopnea"}, {b + 60, 10, "Hypopnea"}});
  PlikiDoby q = sesja(b, 320, nullptr, {}, false);
  q.erase(prefiks(b) + "_EVE.edf");
  p.insert(q.begin(), q.end());
  WynikNocy r = policz(D1, p);
  SPRAWDZ(!r.czesciowa && r.sesje.size() == 2);
  BLISKO(r.sekundy, (150 + 320) * 60);
  ROWNE_L(r.h, 2);
  NocAB n = nocZWyniku(r, nullptr, "ASVAuto");
  ROWNE_L(n.noc.m01, 470);
  ROWNE_L(n.noc.m17, 1);
  ROWNE_L(n.noc.m18, 13 * 60 + 20);  // najdłuższa sesja 01:20 = 800 min od 12:00
  ROWNE_L(n.sesje, 2);
  // doba od południa: drzemka 14:00–15:30 i noc 23:30–06:00 → M-17 0, M-18 23:30; M-01 obie
  long long dz = cyw(2026, 11, 1, 14, 0), nc = cyw(2026, 11, 1, 23, 30);
  p = sesja(dz, 90);
  q = sesja(nc, 390, nullptr, {}, false);
  q.erase(prefiks(nc) + "_EVE.edf");
  p.insert(q.begin(), q.end());
  r = policz(D1, p);
  n = nocZWyniku(r, nullptr, "ASVAuto");
  ROWNE_L(n.noc.m01, 480);
  ROWNE_L(n.noc.m17, 0);
  ROWNE_L(n.noc.m18, 11 * 60 + 30);
  // wczesny sen 16:00–22:00 i 23:00–23:30 → M-17 1, M-18 16:00
  long long ws = cyw(2026, 11, 1, 16, 0), pz = cyw(2026, 11, 1, 23, 0);
  p = sesja(ws, 360);
  q = sesja(pz, 30, nullptr, {}, false);
  q.erase(prefiks(pz) + "_EVE.edf");
  p.insert(q.begin(), q.end());
  n = nocZWyniku(policz(D1, p), nullptr, "ASVAuto");
  ROWNE_L(n.noc.m17, 1);
  ROWNE_L(n.noc.m18, 4 * 60);
  // pliki PLD i EVE z prefiksami różnymi o sekundy (PLD 1 s po BRP): jedna sesja
  long long t = cyw(2026, 11, 1, 23, 0, 5);
  p = sesja(t, 60);
  p[prefiks(t - 1) + "_BRP.edf"] = "";
  r = policz(D1, p, {prefiks(t - 1) + "_BRP.edf"});
  ROWNE_L(r.sesje.size(), 1);
  SPRAWDZ(!r.czesciowa);
  // PLD 11 s po BRP: dwie sesje, ta z samym BRP bez PLD → częściowa
  p = sesja(t, 60);
  p[prefiks(t - 11) + "_BRP.edf"] = "";
  r = policz(D1, p, {prefiks(t - 11) + "_BRP.edf"});
  ROWNE_L(r.sesje.size(), 2);
  SPRAWDZ(r.czesciowa);
  // EVE tylko z „Apnea”: OA i CA nieznane, UA znane
  p = sesja(t, 300, nullptr, {{t + 60, 10, "Apnea"}});
  r = policz(D1, p);
  SPRAWDZ(r.oa < 0 && r.ca < 0 && r.ua == 1 && !r.klasyfikacja);
  n = nocZWyniku(r, nullptr, "ASVAuto");
  SPRAWDZ(std::isnan(n.noc.oa) && std::isnan(n.noc.ca));
  BLISKO(n.noc.ua, 0.2);
  // CSR w CSL przycięty do zapisu; niesparowany start = CSR nieznany
  p = sesja(t, 300);
  p[prefiks(t) + "_CSL.edf"] = adnotacje(t, {{t + 600, -1, "CSR Start"}, {t + 1200, -1, "CSR End"},
                                             {t + 299 * 60, -1, "CSR Start"}, {t + 310 * 60, -1, "CSR End"}});
  r = policz(D1, p);
  BLISKO(r.csrS, 600 + 60);
  p[prefiks(t) + "_CSL.edf"] = adnotacje(t, {{t + 600, -1, "CSR Start"}});
  r = policz(D1, p);
  SPRAWDZ(std::isnan(r.csrS) && r.csrZrodlo == "CSL" && !r.czesciowa);
  // noc krótsza niż 4 h: zapisana, nieważna
  n = nocZWyniku(policz(D1, sesja(t, 200)), nullptr, "ASVAuto");
  SPRAWDZ(n.stan == StanRaportu::KOMPLETNA && !n.noc.wazna && n.noc.m01 == 200);
  // tryb spoza wydania: nieważna
  n = nocZWyniku(policz(D1, sesja(t, 300)), nullptr, "VAuto");
  SPRAWDZ(!n.noc.wazna);
}

static void testListaSciezkiKonfiguracja() {
  ListaSesji l;
  SPRAWDZ(parsujListeSesji(
      "{\"sessions\":[{\"day\":\"20261108\",\"start\":\"2026-11-09T01:06:35\",\"end\":\"2026-11-09T09:24:35\","
      "\"closed\":true,\"files\":[{\"path\":\"/DATALOG/20261108/20261109_010635_PLD.edf\",\"size\":363140},"
      "{\"path\":\"/DATALOG/20261108/20261109_010635_EVE.edf\",\"size\":800}]},{\"day\":\"20261109\",\"start\":\"x\","
      "\"end\":null,\"closed\":false,\"files\":[{\"path\":\"/DATALOG/20261109/20261110_010000_PLD.edf.part\"}]}],"
      "\"str\":null,\"next_cursor\":\"abc+1\"}",
      l));
  ROWNE_L(l.sesje.size(), 2);
  SPRAWDZ(l.sesje[0].closed && !l.sesje[1].closed);
  ROWNE(l.sesje[0].day, "20261108");
  ROWNE_L(l.sesje[0].pliki.size(), 2);
  BLISKO(l.sesje[0].pliki[0].rozmiar, 363140);
  BLISKO(l.sesje[1].pliki[0].rozmiar, -1);
  ROWNE(l.kursor, "abc+1");
  SPRAWDZ(parsujListeSesji("{\"sessions\":[],\"next_cursor\":null}", l) && l.sesje.empty() && l.kursor.empty());
  SPRAWDZ(!parsujListeSesji("{\"sessions\":[{\"closed\":true}]}", l));
  SPRAWDZ(!parsujListeSesji("{\"sessions\":[{\"day\":\"x\",\"files\":[{\"size\":1}]}]}", l));
  SPRAWDZ(!parsujListeSesji("{\"error\":\"catalog_busy\"}", l));
  SPRAWDZ(!parsujListeSesji("{\"sessions\":[", l));
  SPRAWDZ(!parsujListeSesji("", l));
  ROWNE(dzienAB(nrDnia("2026-11-09") - 1), "20261108");
  ROWNE(sciezkaSesjiDoby(nrDnia("2026-11-09") - 1), "/api/sessions?since=20261108&limit=8&order=asc");
  ROWNE(sciezkaSesjiDoby(nrDnia("2026-11-09") - 1, "a b/+"),
        "/api/sessions?since=20261108&limit=8&order=asc&cursor=a%20b%2F%2B");
  ROWNE(sciezkaPobraniaPliku("/DATALOG/20261108/20261109_010635_PLD.edf"),
        "/api/storage/download?path=/DATALOG/20261108/20261109_010635_PLD.edf");
  ROWNE(sciezkaPobraniaPliku("/a b&c"), "/api/storage/download?path=/a%20b%26c");
  ROWNE_L(limitOdpowiedziAB(sciezkaPobraniaPliku("/DATALOG/x/20261109_010635_PLD.edf")), AB_MAKS_BAJTOW_PLIKU_SESJI);
  ROWNE_L(limitOdpowiedziAB(sciezkaPobraniaStr()), AB_MAKS_BAJTOW_STR);
  ROWNE_L(limitOdpowiedziAB(sciezkaSesjiDoby(20000)), AB_MAKS_BAJTOW_SESJI);
  std::set<std::string> m;
  SPRAWDZ(parsujMetrykiJson("m02, M-08,m28", m) && m.size() == 3 && m.count("m08"));
  SPRAWDZ(parsujMetrykiJson("", m) && m.empty());
  SPRAWDZ(!parsujMetrykiJson("m02,,m08", m));
  SPRAWDZ(!parsujMetrykiJson("m99", m));
  SPRAWDZ(!parsujMetrykiJson("ahi", m));
  UstawieniaDnia u = parsujUstawieniaDnia("noc_zrodlo=plik\nnoc_json=m02,m08\n");
  SPRAWDZ(u.nocZrodlo == 1 && u.nocJson.size() == 2 && u.bledy.empty());
  u = parsujUstawieniaDnia("noc_zrodlo=json\n");
  SPRAWDZ(u.nocZrodlo == 0);
  u = parsujUstawieniaDnia("noc_zrodlo=edf\nnoc_json=ahi\n");
  SPRAWDZ(u.nocZrodlo == -1 && u.nocJson.empty() && u.bledy.size() == 2 && u.nocJsonZly);
  SPRAWDZ(!parsujUstawieniaDnia("noc_json=m02\n").nocJsonZly);
  u = parsujUstawieniaDnia("");
  SPRAWDZ(u.nocZrodlo == -1);
  // scalenie metryk z JSON
  NocAB p, j;
  p.noc.m01 = 300;
  p.noc.ahi = 2;
  p.noc.psmed = 5;
  p.noc.m08 = 10;
  p.noc.wazna = true;
  p.pelna = true;
  p.tryb = "ASVAuto";
  j.noc.m01 = 230;
  j.noc.ahi = 2.4f;
  j.noc.m08 = 12;
  scalZJson(p, j, {"m08"});
  BLISKO(p.noc.m08, 12);
  BLISKO(p.noc.ahi, 2);
  SPRAWDZ(p.noc.wazna);
  scalZJson(p, j, {"m01", "m02"});
  ROWNE_L(p.noc.m01, 230);
  BLISKO(p.noc.ahi, 2.4);
  BLISKO(p.noc.psmed, 5);
  SPRAWDZ(!p.noc.wazna);  // M-01 z raportu < 240 min
  j.noc.m01 = 300;
  j.noc.ahi = NAN;
  scalZJson(p, j, {"m01", "m02"});
  SPRAWDZ(!p.noc.wazna);  // AHI z raportu nieznane
}

// pierwszeństwo: pełna noc z plików jak pełna z raportu (ADR-0036 pkt 3)
static void testPierwszenstwo() {
  long long s = cyw(2026, 11, 1, 23, 0);
  NocAB n = nocZWyniku(policz(D1, sesja(s, 420)), nullptr, "ASVAuto");
  SPRAWDZ(n.pelna);
  Noc ns;
  ns.data = n.noc.data;
  ns.zrodlo = 'S';
  ns.m01 = 400;
  ns.ahi = 3;
  ns.emed = 9;
  ns.epapStr = true;
  std::vector<Noc> h = {ns};
  bool zost = false;
  SPRAWDZ(wpiszNocAB(h, n.noc, false, n.pelna, &zost) && !zost && h[0].zrodlo == 'A' && h[0].m01 == 420);
  SPRAWDZ(!h[0].epapStr);
  Noc nk = ns;
  nk.zrodlo = 'K';
  nk.epapStr = false;
  nk.psmed = 4;
  h = {nk};
  SPRAWDZ(wpiszNocAB(h, n.noc, false, n.pelna, &zost) && h[0].zrodlo == 'A');
  // noc częściowa z plików (gdyby trafiła do historii) nie wypiera S
  NocAB cz = n;
  cz.noc.czesciowa = true;
  cz.pelna = false;
  h = {ns};
  zost = false;
  SPRAWDZ(!wpiszNocAB(h, cz.noc, false, cz.pelna, &zost) && zost && h[0].zrodlo == 'S');
}

// ============================================================================================================
// (d) dyrygent
// ============================================================================================================
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS =
    "{\"version\":\"v2.3.1\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"srn\":\"SYN00000001\",\"mode\":\"ASVAuto\","
    "\"system\":\"IDLE\",\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const char* POLA_V3 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb;m18";
static const long DZIS4 = nrDnia("2026-11-04") - 1;  // doba nocy z poranka 4.11
static std::string raport(long day) {  // raport JSON: 300 min, bez PS
  return "{\"day\":" + std::to_string(day) +
         ",\"present\":true,\"partial\":false,\"used_seconds\":18000,\"ahi\":1.4,\"events_known\":true,"
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
// STR z dobą `dzien` (ASVAuto 6–10, PS 3–15)
static std::string strDoby(long dzien) {
  std::vector<SygT> syg = {{"Date", "", 0, 32767, 0, 32767, 1},           {"Duration", "min.", 0, 1440, 0, 1440, 1},
                           {"Mode", "", 0, 16, 0, 16, 1},                 {"S.AA.MinEPAP", "cmH2O", 1, 30, 50, 1500, 1},
                           {"S.AA.MaxEPAP", "cmH2O", 1, 30, 50, 1500, 1}, {"S.AA.MinPS", "cmH2O", 0, 30, 0, 1500, 1},
                           {"S.AA.MaxPS", "cmH2O", 0, 30, 0, 1500, 1},    {"AHI", "", 0, 240, 0, 2400, 1},
                           CRC};
  std::vector<double> v = {(double)dzien, 480, 8, 6, 10, 3, 15, 2};
  std::vector<std::string> r;
  for (size_t i = 0; i < v.size(); i++) r.push_back(probki({cyf(syg[i], v[i])}));
  r.push_back(probki({0}));
  return budujEdf((long long)dzien * 86400 + 12 * 3600, 86400, syg, {r});
}
struct DobaAB {
  long dzien;
  PlikiDoby pliki;
  std::string katalog;
};
static DobaAB dobaAB(long dzien, int minuty = 480) {
  DobaAB d;
  d.dzien = dzien;
  d.katalog = "/DATALOG/" + dzienAB(dzien) + "/";
  long long s = (long long)dzien * 86400 + 23 * 3600;
  d.pliki = sesja(s, minuty, [](int, Wartosci& w) { w["Press.2s"] = 14; w["EprPress.2s"] = 7; },
                  {{s + 600, 10, "Hypopnea"}});
  d.pliki[prefiks(s) + "_BRP.edf"] = "BRP nie jest pobierany";
  d.pliki[prefiks(s) + "_SAD.edf"] = "SAD nie jest pobierany";
  return d;
}
static std::string sesjeJson(const DobaAB& d, bool zamknieta = true, bool part = false, const std::string& kursor = "") {
  std::string f;
  for (auto& x : d.pliki)
    f += std::string(f.empty() ? "" : ",") + "{\"path\":\"" + d.katalog + x.first + (part ? ".part" : "") +
         "\",\"size\":" + std::to_string(x.second.size()) + "}";
  return "{\"sessions\":[{\"day\":\"" + dzienAB(d.dzien) + "\",\"start\":\"x\",\"end\":null,\"closed\":" +
         (zamknieta ? "true" : "false") + ",\"files\":[" + f + "]}],\"str\":null,\"next_cursor\":" +
         (kursor.empty() ? "null" : "\"" + kursor + "\"") + "}";
}
struct PanelN {
  Panel p;
  PanelN(const std::string& konf = "noc_zrodlo=plik\n") {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.pliki.montujeSie = true;
    p.pliki.montowane = true;
    p.pliki.pliki["/dane/noce/2026.csv"] = std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n#koniec 0\n";
    if (!konf.empty()) p.pliki.pliki["/konfiguracja.txt"] = konf;
  }
};
static void wybudz(PanelN& s, uint32_t t, PowodWybudzenia pw = PowodWybudzenia::START) {
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
static int zadan(const PanelN& s, const std::string& sc) {
  int n = 0;
  for (auto& u : s.p.siec.zadania) n += AtrapaSieci::sciezka(u).compare(0, sc.size(), sc) == 0;
  return n;
}
static std::vector<std::string> wiersz(const PanelN& s, const std::string& data) {
  for (auto& l : dziel(s.p.pliki.tresc("/dane/noce/2026.csv"), '\n'))
    if (l.compare(0, data.size(), data) == 0) return dziel(l, ';');
  return {};
}
// odpowiedzi atrapy AirBridge dla doby `d`: status, lista dób, raport JSON, lista sesji, STR i pliki
static void odpowiedzi(PanelN& s, const DobaAB& d, int kodSesji = 200, const std::string& sesje = "") {
  s.p.siec.odpowiedzi.clear();
  s.p.siec.dodaj("/api/status", 200, STATUS);
  s.p.siec.dodaj(sciezkaListy(), 200, lista(d.dzien, d.dzien));
  s.p.siec.dodaj(sciezkaNocy(d.dzien), 200, raport(d.dzien));
  s.p.siec.dodaj("/api/sessions?order=desc", 404, "");  // STR z ADR-0035 osobno (tu bez etagu)
  s.p.siec.dodaj("/api/sessions?since=", kodSesji, kodSesji == 200 ? (sesje.empty() ? sesjeJson(d) : sesje) : "{}");
  s.p.siec.dodaj(sciezkaPobraniaStr(), 200, strDoby(d.dzien));
  for (auto& x : d.pliki) s.p.siec.dodaj(sciezkaPobraniaPliku(d.katalog + x.first), 200, x.second);
}
static bool wLogu(const PanelN& s, const std::string& t) { return zawiera(s.p.port.log, t); }

static void testDyrygentZPlikow() {
  DobaAB d = dobaAB(DZIS4);
  PanelN s;
  odpowiedzi(s, d);
  wybudz(s, ep(2026, 11, 4, 7, 0));
  ROWNE_L(zadan(s, "/api/sessions?since=20261103"), 1);
  ROWNE_L(zadan(s, "/api/report?source=edf&day="), 0);  // bez raportu JSON
  ROWNE_L(zadan(s, sciezkaPobraniaStr()), 1);
  ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 3);  // PLD, EVE, CSL
  SPRAWDZ(zadan(s, "/api/storage/download?path=/DATALOG/20261103/20261103_230000_BRP") == 0);
  SPRAWDZ(zadan(s, "/api/storage/download?path=/DATALOG/20261103/20261103_230000_SAD") == 0);
  std::vector<std::string> w = wiersz(s, "2026-11-04");
  SPRAWDZ(w.size() >= 42);
  if (w.size() >= 42) {
    ROWNE(w[1], "1");             // ważna
    ROWNE(w[2], "480");           // M-01 z plików (raport JSON miał 300)
    ROWNE(w[10], "7.00");         // PS na żywo: 14 − 7
    ROWNE(w[12], "ASVAuto/6/10/3/15");  // ustawienia ze STR tej doby
    ROWNE(w[13], "A");
    ROWNE(w[22], "0");            // M-17
    ROWNE(w[41], "660");          // M-18 23:00
  }
  SPRAWDZ(wLogu(s, "z plików sesji: 1 sesji, 3 plików"));
  SPRAWDZ(wLogu(s, "konfiguracja: noc z plików sesji"));
  SPRAWDZ(!wLogu(s, "E-03"));
}

static void testDyrygentZapas() {
  DobaAB d = dobaAB(DZIS4);
  auto zJson = [](PanelN& s) {
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    return w.size() > 10 && w[2] == "300" && w[10].empty();  // noc z raportu JSON: 300 min, bez PS
  };
  // `noc_zrodlo=json`: bez /api/sessions?since (domyślny tryb porównawczy: test/testy_porownanie.cpp)
  for (const char* k : {"noc_zrodlo=json\n"}) {
    PanelN s(k);
    odpowiedzi(s, d);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, "/api/sessions?since="), 0);
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 0);
    SPRAWDZ(zJson(s));
  }
  // 404 (AirBridge < 2.3.1), 503: raport JSON, bez E-03
  for (int kod : {404, 503}) {
    PanelN s;
    odpowiedzi(s, d, kod);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, "/api/sessions?since="), 1);
    ROWNE_L(zadan(s, sciezkaNocy(DZIS4)), 1);
    SPRAWDZ(zJson(s));
    SPRAWDZ(!wLogu(s, "E-03"));
  }
  // sesja niezamknięta albo z plikami `.part`
  for (int war = 0; war < 2; war++) {
    PanelN s;
    odpowiedzi(s, d, 200, sesjeJson(d, war == 1, war == 1));
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 0);
    SPRAWDZ(zJson(s) && wLogu(s, "sesja niezamknięta"));
  }
  // doba bez sesji na liście
  {
    PanelN s;
    odpowiedzi(s, d, 200, "{\"sessions\":[],\"str\":null,\"next_cursor\":null}");
    wybudz(s, ep(2026, 11, 4, 7, 0));
    SPRAWDZ(zJson(s) && wLogu(s, "doba bez sesji"));
  }
  // plik zniknął między listą a pobraniem (404), plik ucięty, plik ponad limit
  std::string pldN;
  for (auto& x : d.pliki)
    if (rodzajPliku(x.first) == Rodzaj::PLD) pldN = x.first;
  for (int war = 0; war < 3; war++) {
    PanelN s;
    odpowiedzi(s, d);
    std::string sc = sciezkaPobraniaPliku(d.katalog + pldN);
    s.p.siec.odpowiedzi[sc].clear();
    if (war == 0) s.p.siec.dodaj(sc, 404, "{\"error\":\"not_found\"}");
    if (war == 1) s.p.siec.dodaj(sc, 200, d.pliki[pldN].substr(0, 1000));
    if (war == 2) s.p.siec.dodaj(sc, 200, std::string(AB_MAKS_BAJTOW_PLIKU_SESJI + 1, 'x'));
    wybudz(s, ep(2026, 11, 4, 7, 0));
    SPRAWDZ(zJson(s));
    SPRAWDZ(war != 0 || wLogu(s, "niepobrany (kod 404)"));
    SPRAWDZ(war != 1 || wLogu(s, "nieczytelny"));
  }
  // STR niedostępny: bez ustawień doby → raport JSON
  {
    PanelN s;
    odpowiedzi(s, d);
    s.p.siec.odpowiedzi[sciezkaPobraniaStr()].clear();
    s.p.siec.dodaj(sciezkaPobraniaStr(), 500, "");
    wybudz(s, ep(2026, 11, 4, 7, 0));
    SPRAWDZ(zJson(s) && wLogu(s, "bez STR.edf"));
  }
  // brak EVE w sesji: noc częściowa z plików → raport JSON
  {
    DobaAB e = d;
    for (auto it = e.pliki.begin(); it != e.pliki.end();)
      it = rodzajPliku(it->first) == Rodzaj::EVE ? e.pliki.erase(it) : std::next(it);
    PanelN s;
    odpowiedzi(s, e);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    SPRAWDZ(zJson(s) && wLogu(s, "częściowa"));
  }
}

static void testDyrygentRozne() {
  DobaAB d = dobaAB(DZIS4);
  // `noc_json=m01`: raport JSON i pliki; M-01 z raportu, PS z plików
  {
    PanelN s("noc_zrodlo=plik\nnoc_json=m01\n");
    odpowiedzi(s, d);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, sciezkaNocy(DZIS4)), 1);
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 3);
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    SPRAWDZ(w.size() > 12 && w[2] == "300" && w[10] == "7.00");
    SPRAWDZ(wLogu(s, "1 metryk z raportu JSON"));
  }
  // noce S i K tej daty ustępują pełnej nocy z plików
  for (char z : {'S', 'K'}) {
    PanelN s;
    Noc n;
    n.data = "2026-11-04";
    n.zrodlo = z;
    n.m01 = 400;
    n.ahi = 3;
    n.wazna = true;
    s.p.pliki.pliki["/dane/noce/2026.csv"] =
        std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n" + wierszNocyV3(n) + "\n#koniec 1\n";
    odpowiedzi(s, d);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    SPRAWDZ(w.size() > 13 && w[13] == "A" && w[2] == "480");
  }
  // stronicowanie: pierwsza strona z kursorem i sesją innej doby wcześniej, druga z sesją doby
  {
    PanelN s;
    DobaAB wcz = dobaAB(DZIS4 - 1);
    odpowiedzi(s, d);
    s.p.siec.odpowiedzi["/api/sessions?since="].clear();
    s.p.siec.dodaj("/api/sessions?since=", 200, sesjeJson(wcz, true, false, "k2"));  // doba wcześniejsza, dalej kursor
    s.p.siec.dodaj("/api/sessions?since=" + dzienAB(DZIS4) + "&limit=8&order=asc&cursor=k2", 200, sesjeJson(d));
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, "/api/sessions?since="), 2);
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    SPRAWDZ(w.size() > 10 && w[2] == "480");
  }
  // slot dzienny (tylko `/api/status`) i ponowny ranek z kompletną nocą: bez list sesji i bez pobrań plików
  {
    PanelN s;
    odpowiedzi(s, d);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    SPRAWDZ(wiersz(s, "2026-11-04").size() > 2);
    s.p.siec.zadania.clear();
    s.p.d.z.clear();
    s.p.port.log.clear();
    s.p.zegar.epoch = ep(2026, 11, 4, 7, 10);
    s.p.zegar.msTeraz = 0;
    s.p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
    s.p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    s.p.start.budzace = BIT_PRAWY;
    s.p.przyciski.skrypt = {{0, BIT_PRAWY}, {300, 0}};
    Dyrygent(s.p.adaptery(), KONF).wybudzenie();
    s.p.start.budzace = 0;
    s.p.przyciski.skrypt.clear();
    s.p.start.stan.ekran = (uint8_t)Ekran::DZIEN;
    s.p.siec.zadania.clear();
    s.p.port.log.clear();
    s.p.zegar.epoch = ep(2026, 11, 4, 8, 0) + 5;
    s.p.zegar.msTeraz = 0;
    s.p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    Dyrygent(s.p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(wLogu(s, "slot dzienny: tylko odczyt stanu aparatu"));
    ROWNE_L(zadan(s, "/api/sessions"), 0);
    ROWNE_L(zadan(s, "/api/storage"), 0);
  }
  // etag STR (ADR-0035) z listy sesji odczytanej przed pobraniem STR do nocy z plików, nie z późniejszego
  // `/api/sessions?order=desc` (plik mógł się zmienić między odczytami: etag nie może być nowszy niż treść)
  {
    PanelN s;
    odpowiedzi(s, d);
    std::string l = sesjeJson(d);
    l.replace(l.find("\"str\":null"), 10, "\"str\":{\"path\":\"/STR.edf\",\"size\":1,\"etag\":\"\\\"e1\\\"\"}");
    s.p.siec.odpowiedzi["/api/sessions?since="].clear();
    s.p.siec.dodaj("/api/sessions?since=", 200, l);
    s.p.siec.odpowiedzi["/api/sessions?order=desc"].clear();
    s.p.siec.dodaj("/api/sessions?order=desc", 200,
                   "{\"sessions\":[],\"str\":{\"path\":\"/STR.edf\",\"size\":1,\"etag\":\"\\\"e2\\\"\"},\"next_cursor\":null}");
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, sciezkaPobraniaStr()), 1);
    ROWNE_L(zadan(s, "/api/sessions?order=desc"), 0);
    std::string e = s.p.nvs.tekst("cpap", NVS_KLUCZ_STR_ETAG, "");
    ROWNE(e.substr(0, e.rfind('|')), "\"e1\"");
    // lista bez bloku `str`: STR przetworzony, etag bez zapisu (następne wybudzenie pobierze plik od nowa)
    PanelN b;
    odpowiedzi(b, d);
    wybudz(b, ep(2026, 11, 4, 7, 0));
    ROWNE(b.p.nvs.tekst("cpap", NVS_KLUCZ_STR_ETAG, ""), "");
    SPRAWDZ(wLogu(b, "etag bez zapisu"));
  }
  // przerwa w sieci przy liście sesji (kod ≤ 0): raport JSON w tym samym wybudzeniu (z ponowieniami R-AW-01)
  {
    PanelN s;
    odpowiedzi(s, d);
    s.p.siec.odpowiedzi["/api/sessions?since="].clear();
    s.p.siec.dodaj("/api/sessions?since=", -1, "");
    wybudz(s, ep(2026, 11, 4, 7, 0));
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    SPRAWDZ(w.size() > 2 && w[2] == "300");
  }
}

int main(int argc, char** argv) {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  if (argc > 1) {
    std::string kat = argv[1];
    for (const char* k : {"karta", "karta_ps", "karta_panelu"}) testZgodnosc(kat + "/" + k, kat + "/" + k + ".txt");
  } else {
    printf("  zgodność z noce.py pominięta: brak katalogu kart i wzorców (testy.sh go podaje)\n");
  }
  testNazwyIPercentyl();
  testJednaSesja();
  testBrakiIUszkodzenia();
  testWieleSesjiIDoba();
  testListaSciezkiKonfiguracja();
  testPierwszenstwo();
  testDyrygentZPlikow();
  testDyrygentZapas();
  testDyrygentRozne();
  printf("testy_noc_pliki: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
