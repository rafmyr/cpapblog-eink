// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe historii nocy ze STR.edf (ADR-0035): czytnik EDF na uszkodzonych plikach (także pod
// -fsanitize=address,undefined,float-cast-overflow), data poranka z sygnału `Date`, brak wartości (0xFFFF) jako NAN,
// jednostki, tryby aparatu, pierwszeństwo nocy A, K i S w obie strony, zapis i odczyt wiersza nocy `S`, migracja
// dwóch plików rocznych 3 → 4. Dane wyłącznie syntetyczne. Każdy błąd ma w komunikacie nazwę grupy przypadków.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "edf.h"
#include "magazyn.h"
#include "noc.h"
#include "plan.h"
#include "str.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(nr, w)                                                            \
  do {                                                                            \
    if (w) ok++;                                                                  \
    else { zle++; printf("BŁĄD [%s] %s:%d: %s\n", nr, __FILE__, __LINE__, #w); } \
  } while (0)

// ---------- budowa EDF (niezależna od testów podstawowych) ----------
struct SygR {
  std::string e, j;
  std::string fmin, fmax, cmin, cmax, pr;  // pola nagłówka jako tekst (do psucia)
};
static std::string pole(const std::string& t, size_t n) {
  std::string x = t.substr(0, n);
  x.resize(n, ' ');
  return x;
}
static SygR syg(const char* e, const char* j, double fmin, double fmax, long cmin, long cmax, int pr = 1) {
  char a[5][32];
  snprintf(a[0], 32, "%g", fmin);
  snprintf(a[1], 32, "%g", fmax);
  snprintf(a[2], 32, "%ld", cmin);
  snprintf(a[3], 32, "%ld", cmax);
  snprintf(a[4], 32, "%d", pr);
  return {e, j, a[0], a[1], a[2], a[3], a[4]};
}
struct NagEdfR {
  std::string wersja = "0", data = "01.11.25", godz = "12.00.00", rekordy, czas = "86400", ns, bajty;
};
// probki[r][s][k] cyfrowe
static std::string edfBajty(const std::vector<SygR>& s, const std::vector<std::vector<std::vector<long>>>& probki,
                            NagEdfR n = NagEdfR()) {
  size_t ns = s.size();
  if (n.rekordy.empty()) n.rekordy = std::to_string(probki.size());
  if (n.ns.empty()) n.ns = std::to_string(ns);
  if (n.bajty.empty()) n.bajty = std::to_string(256 + 256 * ns);
  std::string h = pole(n.wersja, 8) + pole("X", 80) + pole("Startdate X", 80) + pole(n.data, 8) + pole(n.godz, 8) +
                  pole(n.bajty, 8) + pole("", 44) + pole(n.rekordy, 8) + pole(n.czas, 8) + pole(n.ns, 4);
  for (auto& x : s) h += pole(x.e, 16);
  for (size_t i = 0; i < ns; i++) h += pole("", 80);
  for (auto& x : s) h += pole(x.j, 8);
  for (auto& x : s) h += pole(x.fmin, 8);
  for (auto& x : s) h += pole(x.fmax, 8);
  for (auto& x : s) h += pole(x.cmin, 8);
  for (auto& x : s) h += pole(x.cmax, 8);
  for (size_t i = 0; i < ns; i++) h += pole("", 80);
  for (auto& x : s) h += pole(x.pr, 8);
  for (size_t i = 0; i < ns; i++) h += pole("", 32);
  for (auto& r : probki)
    for (auto& sy : r)
      for (long v : sy) {
        uint16_t u = (uint16_t)(v & 0xFFFF);
        h += (char)(u & 0xFF);
        h += (char)(u >> 8);
      }
  return h;
}

// STR: sygnały po jednej próbce; wartości cyfrowe = fizyczne × 10 dla skali 0.1 itd. (zakresy dobrane tak, że
// cyfrowa = fizyczna × mnożnik)
struct DefSR {
  const char* e;
  const char* j;
  double mnoznik;  // fizyczna = cyfrowa / mnoznik
};
static const DefSR DEF[] = {
    {"Date", "", 1},          {"Mode", "", 1},           {"Duration", "min", 1},   {"AHI", "", 10},
    {"OAI", "", 10},          {"CAI", "", 10},           {"HI", "", 10},           {"UAI", "", 10},
    {"S.C.Press", "cmH2O", 50}, {"S.AS.MaxPress", "cmH2O", 50}, {"S.AS.MinPress", "cmH2O", 50},
    {"S.EPR.Level", "", 1},   {"S.AV.EPAP", "cmH2O", 50}, {"S.AV.MinPS", "cmH2O", 50}, {"S.AV.MaxPS", "cmH2O", 50},
    {"S.AA.MinEPAP", "cmH2O", 50}, {"S.AA.MaxEPAP", "cmH2O", 50}, {"S.AA.MinPS", "cmH2O", 50},
    {"S.AA.MaxPS", "cmH2O", 50}, {"TgtIPAP.50", "cmH2O", 50}, {"TgtIPAP.95", "cmH2O", 50},
    {"TgtEPAP.50", "cmH2O", 50}, {"TgtEPAP.95", "cmH2O", 50}, {"Leak.50", "L/s", 100}, {"Leak.95", "L/s", 100},
    {"RespRate.50", "bpm", 5},
};
using DobaR = std::map<std::string, double>;  // brak klucza = 0xFFFF
static std::vector<SygR> sygnalyStr(const std::map<std::string, std::string>& jednostki = {}) {
  std::vector<SygR> s;
  for (auto& d : DEF) {
    std::string j = jednostki.count(d.e) ? jednostki.at(d.e) : d.j;
    s.push_back(syg(d.e, j.c_str(), 0, 32000.0 / d.mnoznik, 0, 32000));
  }
  return s;
}
static std::string strPlik(const std::vector<DobaR>& doby, const std::map<std::string, std::string>& jedn = {}) {
  std::vector<std::vector<std::vector<long>>> r;
  for (auto& d : doby) {
    std::vector<std::vector<long>> rek;
    for (auto& x : DEF) {
      auto i = d.find(x.e);
      rek.push_back({i == d.end() ? 0xFFFF : std::lround(i->second * x.mnoznik)});
    }
    r.push_back(rek);
  }
  return edfBajty(sygnalyStr(jedn), r);
}
static const long D0 = edf::dniOdEpoki(2026, 10, 1);  // doba nocy z 01 na 02.10.2026
static DobaR asvAuto(long dzien, double czas = 420, double ahi = 3.2) {
  return {{"Date", (double)dzien}, {"Mode", 8},          {"Duration", czas},      {"AHI", ahi},
          {"HI", 1.0},             {"UAI", 0.4},          {"S.AA.MinEPAP", 6},     {"S.AA.MaxEPAP", 10},
          {"S.AA.MinPS", 3},       {"S.AA.MaxPS", 15},    {"TgtIPAP.50", 12},      {"TgtIPAP.95", 14},
          {"TgtEPAP.50", 7},       {"TgtEPAP.95", 8},     {"Leak.50", 0.1},        {"Leak.95", 0.4},
          {"RespRate.50", 15}};
}
static bool czytajNoc(const std::string& b, long dzien, Noc& n, Str* wynik = nullptr) {
  Str s;
  if (czytajStr(b, s) != edf::Blad::BRAK) return false;
  bool r = nocZeStr(s, dzien, n);
  if (wynik) *wynik = std::move(s);
  return r;
}

// ============================ uszkodzone nagłówki i dane: błąd, nie wyjście poza bufor ============================
static void testUszkodzone() {
  std::vector<SygR> s = {syg("Date", "", 0, 32000, 0, 32000), syg("Duration", "min", 0, 32000, 0, 32000, 3)};
  std::vector<std::vector<std::vector<long>>> r = {{{100}, {1, 2, 3}}, {{101}, {4, 5, 6}}};
  std::string dobry = edfBajty(s, r);
  edf::Plik p;
  SPRAWDZ("uszkodzony plik", edf::czytaj((const uint8_t*)dobry.data(), dobry.size(), p) == edf::Blad::BRAK);
  SPRAWDZ("uszkodzony plik", p.sygnaly.size() == 2 && p.bajtyRekordu == 8 && p.rekordy == 2);
  // wiele próbek: wartoscStr = NAN (nie pierwsza próbka)
  Str st;
  SPRAWDZ("uszkodzony plik", czytajStr(dobry, st) == edf::Blad::BRAK && std::isnan(wartoscStr(st, 0, "Duration")));
  SPRAWDZ("uszkodzony plik", st.plik.surowa(st.plik.sygnaly[1], 1, 2) == 6 && st.plik.surowa(st.plik.sygnaly[1], 1, 3) == 0xFFFF);
  SPRAWDZ("uszkodzony plik", st.plik.surowa(st.plik.sygnaly[1], 2, 0) == 0xFFFF && st.plik.surowa(st.plik.sygnaly[1], -1, 0) == 0xFFFF);

  struct Przyp {
    const char* opis;
    NagEdfR n;
    std::vector<SygR> s;
  };
  std::vector<Przyp> zle_ = {
      {"rekordy -1", [] { NagEdfR n; n.rekordy = "-1"; return n; }(), s},
      {"rekordy ogromne", [] { NagEdfR n; n.rekordy = "99999999"; return n; }(), s},
      {"rekordy 1e300", [] { NagEdfR n; n.rekordy = "1e300"; return n; }(), s},
      {"rekordy nan", [] { NagEdfR n; n.rekordy = "nan"; return n; }(), s},
      {"rekordy inf", [] { NagEdfR n; n.rekordy = "inf"; return n; }(), s},
      {"rekordy tekst", [] { NagEdfR n; n.rekordy = "abc"; return n; }(), s},
      {"rekordy 3 (ucięty)", [] { NagEdfR n; n.rekordy = "3"; return n; }(), s},
      {"ns 0", [] { NagEdfR n; n.ns = "0"; n.bajty = "256"; return n; }(), s},
      {"ns 9999", [] { NagEdfR n; n.ns = "9999"; return n; }(), s},
      {"ns -1", [] { NagEdfR n; n.ns = "-1"; return n; }(), s},
      {"ns 3 przy 2 opisach", [] { NagEdfR n; n.ns = "3"; n.bajty = "1024"; return n; }(), s},
      {"bajty nagłówka złe", [] { NagEdfR n; n.bajty = "1024"; return n; }(), s},
      {"wersja 1", [] { NagEdfR n; n.wersja = "1"; return n; }(), s},
      {"data 31.02", [] { NagEdfR n; n.data = "31.02.25"; return n; }(), s},
      {"godzina 25", [] { NagEdfR n; n.godz = "25.00.00"; return n; }(), s},
      {"próbki 0", NagEdfR(), {s[0], syg("Duration", "min", 0, 32000, 0, 32000, 0)}},
      {"próbki -5", NagEdfR(), {s[0], syg("Duration", "min", 0, 32000, 0, 32000, -5)}},
      {"próbki 2e9", NagEdfR(), {s[0], syg("Duration", "min", 0, 32000, 0, 32000, 2000000000)}},
      {"cyf_min = cyf_max", NagEdfR(), {s[0], syg("Duration", "min", 0, 1, 5, 5, 3)}},
      {"fiz_min = fiz_max", NagEdfR(), {s[0], syg("Duration", "min", 1, 1, 0, 5, 3)}},
  };
  for (auto& z : zle_) {
    std::string b = edfBajty(z.s, r, z.n);
    edf::Plik q;
    edf::Blad e = edf::czytaj((const uint8_t*)b.data(), b.size(), q);
    if (e == edf::Blad::BRAK) {
      zle++;
      printf("BŁĄD [uszkodzony plik] plik „%s” przyjęty jako poprawny\n", z.opis);
    } else {
      ok++;
    }
    Str x;
    (void)czytajStr(b, x);
  }
  // ucięcie w każdym miejscu: nigdy BRAK, gdy ucięte dane rekordu
  for (size_t n = 0; n < dobry.size(); n++) {
    edf::Plik q;
    edf::Blad e = edf::czytaj((const uint8_t*)dobry.data(), n, q);
    if (e == edf::Blad::BRAK) {
      zle++;
      printf("BŁĄD [uszkodzony plik] plik ucięty do %zu B przyjęty\n", n);
      break;
    }
  }
  ok++;
  SPRAWDZ("uszkodzony plik", edf::czytaj(nullptr, 0, p) == edf::Blad::KROTKI_NAGLOWEK);
  // nadmiarowe bajty na końcu: przyjęte (jak edf.py), rekordy z nagłówka
  std::string dl = dobry + std::string(5, '\x7f');
  SPRAWDZ("uszkodzony plik", edf::czytaj((const uint8_t*)dl.data(), dl.size(), p) == edf::Blad::BRAK && p.rekordy == 2);
  // zakres cyfrowy ze znakiem: -100..100, surowa 0xFF9C = -100 → fiz_min
  std::vector<SygR> zs = {syg("Date", "", 0, 32000, 0, 32000), syg("AHI", "", -10, 10, -100, 100)};
  std::string b = edfBajty(zs, {{{100}, {-100}}, {{101}, {100}}, {{102}, {0x7FFF}}});
  Str z;
  SPRAWDZ("uszkodzony plik", czytajStr(b, z) == edf::Blad::BRAK);
  SPRAWDZ("uszkodzony plik", std::fabs(wartoscStr(z, 0, "AHI") + 10) < 1e-9 && std::fabs(wartoscStr(z, 1, "AHI") - 10) < 1e-9);
  SPRAWDZ("uszkodzony plik", std::isnan(wartoscStr(z, 2, "AHI")));  // poza zakresem cyfrowym = brak
}

// losowe uszkodzenia prawdziwie wyglądającego STR (pod sanitizerami: brak odczytu poza buforem, brak UB)
static void testLosowe() {
  std::vector<DobaR> d;
  for (int i = 0; i < 20; i++) d.push_back(asvAuto(D0 + i));
  std::string wzor = strPlik(d);
  uint32_t ziarno = 12345;
  auto los = [&]() { return ziarno = ziarno * 1103515245u + 12345u, (ziarno >> 8); };
  int przyjete = 0;
  for (int it = 0; it < 4000; it++) {
    std::string b = wzor;
    int zmian = 1 + (int)(los() % 8);
    for (int k = 0; k < zmian; k++) {
      size_t i = los() % b.size();
      // połowa zmian w nagłówku (pola liczbowe), połowa w danych
      if (k % 2 == 0) i = los() % std::min<size_t>(b.size(), 256 + 256 * sizeof(DEF) / sizeof(DEF[0]));
      static const char ZN[] = "0123456789-+.eE nN\xff\x00";
      b[i] = ZN[los() % (sizeof ZN - 1)];
    }
    if (los() % 5 == 0) b.resize(los() % b.size());
    Str s;
    if (czytajStr(b, s) != edf::Blad::BRAK) continue;
    przyjete++;
    std::vector<Noc> noce = noceZeStr(s, "2027-01-01");
    std::vector<Noc> hist;
    for (auto& n : noce) {
      wpiszNocS(hist, n);
      SPRAWDZ("uszkodzony plik", dataOk(n.data) && n.zrodlo == 'S' && n.m01 > 0 && n.m01 <= 1440);
      // pola liczbowe: skończone albo NAN, nigdy nieskończone (zapis do pliku)
      for (float v : {n.ahi, n.oa, n.ca, n.h, n.ua, n.wmed, n.wp95, n.emed, n.ep95, n.m13})
        if (std::isinf(v)) { zle++; printf("BŁĄD [uszkodzony plik] nieskończona wartość w nocy %s\n", n.data.c_str()); }
      Noc z;
      SPRAWDZ("uszkodzony plik", parsujNocV3(wierszNocyV3(n), z) && z.data == n.data);
    }
  }
  printf("  losowe uszkodzenia: %d z 4000 przyjętych jako czytelne\n", przyjete);
}

// Date jako liczba spoza zakresu `long` (pod -fsanitize=float-cast-overflow), ujemna i bardzo duża
static void testDataSkrajna() {
  std::vector<SygR> s = {syg("Date", "", -1e300, 1e300, -32000, 32000), syg("Duration", "min", 0, 32000, 0, 32000)};
  std::string b = edfBajty(s, {{{32000}, {420}}, {{-32000}, {420}}});
  Str st;
  SPRAWDZ("data skrajna", czytajStr(b, st) == edf::Blad::BRAK);
  std::vector<Noc> n = noceZeStr(st, "");
  for (auto& x : n) SPRAWDZ("data skrajna", dataOk(x.data));
  // Date 1e7 (rok ~29000): bez nocy z niepoprawną datą
  std::vector<SygR> s2 = {syg("Date", "", 0, 1e7, 0, 32000), syg("Duration", "min", 0, 32000, 0, 32000)};
  std::string b2 = edfBajty(s2, {{{32000}, {420}}});
  Str st2;
  SPRAWDZ("data skrajna", czytajStr(b2, st2) == edf::Blad::BRAK);
  for (auto& x : noceZeStr(st2, "")) SPRAWDZ("data skrajna", dataOk(x.data) && x.data.size() == 10);
}

// ============================ data poranka = Date + 1, epoka 1970-01-01 ============================
static void testDataPoranka() {
  SPRAWDZ("data poranka", nrDnia("1970-01-02") == 1 && edf::dniOdEpoki(1970, 1, 1) == 0);
  Noc n;
  long d = edf::dniOdEpoki(2026, 11, 8);  // noc z 08 na 09.11
  SPRAWDZ("data poranka", czytajNoc(strPlik({asvAuto(d)}), d, n) && n.data == "2026-11-09");
  // przełom roku i 29.02
  long s = edf::dniOdEpoki(2025, 12, 31), l = edf::dniOdEpoki(2028, 2, 28);
  SPRAWDZ("data poranka", czytajNoc(strPlik({asvAuto(s)}), s, n) && n.data == "2026-01-01");
  SPRAWDZ("data poranka", czytajNoc(strPlik({asvAuto(l)}), l, n) && n.data == "2028-02-29");
  // Date pusta → dzień startu pliku (01.11.2025, start 12:00) + numer rekordu
  DobaR bez = asvAuto(0);
  bez.erase("Date");
  Str st;
  SPRAWDZ("data poranka", czytajStr(strPlik({asvAuto(edf::dniOdEpoki(2025, 11, 1)), bez}), st) == edf::Blad::BRAK);
  std::vector<Noc> w = noceZeStr(st, "");
  SPRAWDZ("data poranka", w.size() == 2 && w[0].data == "2025-11-02" && w[1].data == "2025-11-03");
  // ta sama doba dwa razy: późniejszy rekord wygrywa
  Str dup;
  SPRAWDZ("data poranka", czytajStr(strPlik({asvAuto(D0, 400), asvAuto(D0, 300)}), dup) == edf::Blad::BRAK);
  SPRAWDZ("data poranka", nocZeStr(dup, D0, n) && n.m01 == 300);
  // filtr `dzis`: noc z datą po dziś pominięta, dzisiejsza zostaje
  Str f;
  SPRAWDZ("data poranka", czytajStr(strPlik({asvAuto(D0), asvAuto(D0 + 1)}), f) == edf::Blad::BRAK);
  std::vector<Noc> fw = noceZeStr(f, dataZNr(D0 + 1));
  SPRAWDZ("data poranka", fw.size() == 1 && fw[0].data == dataZNr(D0 + 1));
}

// ============================ brak danych = NAN, nie 0; ciśnienie wdechowe poza rekordem ============================
static void testBrakDanych() {
  Noc n;
  DobaR d = asvAuto(D0);  // OAI i CAI bez wartości (jak AirCurve)
  SPRAWDZ("brak danych", czytajNoc(strPlik({d}), D0, n));
  SPRAWDZ("brak danych", n.zrodlo == 'S' && n.m01 == 420 && std::fabs(n.ahi - 3.2f) < 1e-4 && n.wazna && !n.czesciowa);
  SPRAWDZ("brak danych", std::isnan(n.oa) && std::isnan(n.ca) && std::fabs(n.h - 1.0f) < 1e-4 && std::fabs(n.ua - 0.4f) < 1e-4);
  SPRAWDZ("brak danych", std::isnan(n.cmed) && std::isnan(n.cp95));  // R-NOR-08: TgtIPAP nie trafia do rekordu
  SPRAWDZ("brak danych", std::isnan(n.psmed) && std::isnan(n.psp95) && std::isnan(n.csr) && std::isnan(n.m08));
  SPRAWDZ("brak danych", n.m17 == -1 && n.m18 == -1 && std::isnan(n.m10) && std::isnan(n.m10p));
  SPRAWDZ("brak danych", n.epapStr && std::fabs(n.emed - 7) < 1e-4 && std::fabs(n.ep95 - 8) < 1e-4);
  SPRAWDZ("brak danych", std::isnan(epapMed(n)));  // EPAP ze STR poza normą i regułami (epapMed() pomija epapStr)
  SPRAWDZ("brak danych", !rekordPelny(n));
  // wiersz pliku: pola bez danych puste (nie „0”), odczyt odtwarza NAN
  std::vector<std::string> f;
  std::string w = wierszNocyV3(n);
  size_t a = 0;
  for (;;) {
    size_t b = w.find(';', a);
    f.push_back(w.substr(a, b == std::string::npos ? b : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  SPRAWDZ("brak danych", f.size() == POLA_NOCY_V3 && f[13] == "S" && f[4].empty() && f[5].empty() && f[16].empty() &&
                       f[17].empty());
  Noc z;
  SPRAWDZ("brak danych", parsujNocV3(w, z) && z.zrodlo == 'S' && std::isnan(z.oa) && std::isnan(z.cmed) && z.epapStr &&
                       z.wazna && z.m01 == 420);
  // AHI puste → noc zapisana, nieważna, AHI NAN (nie 0)
  DobaR bezAhi = asvAuto(D0);
  bezAhi.erase("AHI");
  SPRAWDZ("brak danych", czytajNoc(strPlik({bezAhi}), D0, n) && std::isnan(n.ahi) && !n.wazna);
  // AHI = 0 to wartość: noc ważna z AHI 0
  SPRAWDZ("brak danych", czytajNoc(strPlik({asvAuto(D0, 420, 0)}), D0, n) && n.ahi == 0 && n.wazna);
  // Duration puste, 0, ujemne (poza zakresem cyfrowym), 1441 min, 0,4 min → bez nocy
  DobaR bezCzasu = asvAuto(D0);
  bezCzasu.erase("Duration");
  SPRAWDZ("brak danych", !czytajNoc(strPlik({bezCzasu}), D0, n));
  SPRAWDZ("brak danych", !czytajNoc(strPlik({asvAuto(D0, 0)}), D0, n));
  SPRAWDZ("brak danych", !czytajNoc(strPlik({asvAuto(D0, 1441)}), D0, n));
  // próg 4 h: 239 min nieważna, 240 ważna
  SPRAWDZ("brak danych", czytajNoc(strPlik({asvAuto(D0, 239)}), D0, n) && !n.wazna);
  SPRAWDZ("brak danych", czytajNoc(strPlik({asvAuto(D0, 240)}), D0, n) && n.wazna);
}

// ============================ jednostki przecieku i oddechów ============================
static void testJednostki() {
  Noc n;
  SPRAWDZ("jednostki", czytajNoc(strPlik({asvAuto(D0)}), D0, n) && std::fabs(n.wmed - 6) < 1e-3 && std::fabs(n.wp95 - 24) < 1e-3);
  SPRAWDZ("jednostki", std::fabs(n.m13 - 15) < 1e-4);
  // Leak w L/min (bez przeliczenia) i w nieznanej jednostce (brak, bez zgadywania)
  SPRAWDZ("jednostki", czytajNoc(strPlik({asvAuto(D0)}, {{"Leak.50", "L/min"}, {"Leak.95", "L/min"}}), D0, n) &&
                       std::fabs(n.wmed - 0.1f) < 1e-4);
  SPRAWDZ("jednostki", czytajNoc(strPlik({asvAuto(D0)}, {{"Leak.50", "ml/s"}, {"Leak.95", "ml/s"}}), D0, n) &&
                       std::isnan(n.wmed) && std::isnan(n.wp95));
}

// ============================ tryby aparatu i ustawienia ============================
static void testTryby() {
  Noc n;
  DobaR c = {{"Date", (double)D0}, {"Mode", 0}, {"Duration", 400}, {"AHI", 1}, {"S.C.Press", 9.4}, {"S.EPR.Level", 2}};
  SPRAWDZ("tryby aparatu", czytajNoc(strPlik({c}), D0, n) && n.tryb == "CPAP" && n.wazna);
  if (n.ustawienia != "CPAP/9.4/2") { zle++; printf("BŁĄD [tryby aparatu] CPAP ustawienia „%s”\n", n.ustawienia.c_str()); } else ok++;
  SPRAWDZ("tryby aparatu", std::fabs(n.ipc - 9.4f) < 1e-4 && std::fabs(n.epr - 2) < 1e-4);
  DobaR a = {{"Date", (double)D0}, {"Mode", 1}, {"Duration", 400}, {"AHI", 1}, {"S.AS.MaxPress", 15},
            {"S.AS.MinPress", 6.2}, {"S.EPR.Level", 3}};
  SPRAWDZ("tryby aparatu", czytajNoc(strPlik({a}), D0, n) && n.tryb == "AutoSet" && n.wazna);
  if (n.ustawienia != "AutoSet/6.2/15/3") { zle++; printf("BŁĄD [tryby aparatu] AutoSet ustawienia „%s”\n", n.ustawienia.c_str()); } else ok++;
  SPRAWDZ("tryby aparatu", std::fabs(n.mpa - 15) < 1e-4 && std::fabs(n.mpi - 6.2f) < 1e-4);
  DobaR v = {{"Date", (double)D0}, {"Mode", 7}, {"Duration", 400}, {"AHI", 1}, {"S.AV.EPAP", 5}, {"S.AV.MinPS", 3},
            {"S.AV.MaxPS", 12}};
  SPRAWDZ("tryby aparatu", czytajNoc(strPlik({v}), D0, n) && n.tryb == "ASV" && n.wazna);
  if (n.ustawienia != "ASV/5/3/12") { zle++; printf("BŁĄD [tryby aparatu] ASV ustawienia „%s”\n", n.ustawienia.c_str()); } else ok++;
  SPRAWDZ("tryby aparatu", czytajNoc(strPlik({asvAuto(D0)}), D0, n) && n.tryb == "ASVAuto" && n.ustawienia == "ASVAuto/6/10/3/15");
  SPRAWDZ("tryby aparatu", std::fabs(n.eai - 6) < 1e-4 && std::fabs(n.axs - 15) < 1e-4);
  DobaR va = asvAuto(D0);
  va["Mode"] = 6;  // VAuto: poza wydaniem
  SPRAWDZ("tryby aparatu", czytajNoc(strPlik({va}), D0, n) && n.tryb == "VAuto" && !n.wazna);
  DobaR bezTrybu = asvAuto(D0);
  bezTrybu.erase("Mode");
  SPRAWDZ("tryby aparatu", czytajNoc(strPlik({bezTrybu}), D0, n) && n.tryb.empty() && !n.wazna);
  DobaR zlyTryb = asvAuto(D0);
  zlyTryb["Mode"] = 40;
  SPRAWDZ("tryby aparatu", czytajNoc(strPlik({zlyTryb}), D0, n) && !n.wazna);
}

// ============================ pierwszeństwo A, K, S w obie strony ============================
static Noc nS(const std::string& d, int m01 = 420) {
  Noc n;
  n.data = d;
  n.zrodlo = 'S';
  n.m01 = m01;
  n.ahi = 2;
  n.h = 1.5f;
  n.emed = 7;
  n.epapStr = true;
  n.wazna = true;
  return n;
}
static Noc nA(const std::string& d, bool pelna, int m01, bool czesciowa = false) {
  Noc n;
  n.data = d;
  n.zrodlo = 'A';
  n.m01 = m01;
  n.ahi = m01 >= 0 ? 1.1f : NAN;
  if (pelna) n.cmed = 11;
  n.czesciowa = czesciowa;
  n.wazna = pelna && !czesciowa;
  return n;
}
static Noc nK(const std::string& d) {
  Noc n;
  n.data = d;
  n.zrodlo = 'K';
  n.m01 = 450;
  n.ahi = 0.9f;
  n.cmed = 10;
  n.wazna = true;
  return n;
}
static void testPierwszenstwo() {
  const std::string d = "2026-11-02";
  // S potem pełna A: A wygrywa; potem S nic nie zmienia
  {
    std::vector<Noc> h;
    SPRAWDZ("pierwszeństwo nocy", wpiszNocS(h, nS(d)));
    SPRAWDZ("pierwszeństwo nocy", wpiszNocAB(h, nA(d, true, 400), false, true) && h.size() == 1 && h[0].zrodlo == 'A');
    SPRAWDZ("pierwszeństwo nocy", !h[0].epapStr && std::isnan(h[0].emed));  // EPAP ze STR nie przechodzi do nocy A
    SPRAWDZ("pierwszeństwo nocy", !wpiszNocS(h, nS(d)) && h[0].zrodlo == 'A' && h[0].m01 == 400);
  }
  // pełna A potem S: A zostaje
  {
    std::vector<Noc> h;
    SPRAWDZ("pierwszeństwo nocy", wpiszNocAB(h, nA(d, true, 400), false, true));
    SPRAWDZ("pierwszeństwo nocy", !wpiszNocS(h, nS(d)) && h[0].zrodlo == 'A');
  }
  // K potem S i S potem K
  {
    std::vector<Noc> h = {nK(d)};
    SPRAWDZ("pierwszeństwo nocy", !wpiszNocS(h, nS(d)) && h[0].zrodlo == 'K');
    std::vector<Noc> h2;
    wpiszNocS(h2, nS(d));
    SPRAWDZ("pierwszeństwo nocy", wpiszImport(h2, nK(d)) == WynikImportu::ZASTAPIONA && h2[0].zrodlo == 'K' && h2[0].m01 == 450);
    SPRAWDZ("pierwszeństwo nocy", std::fabs(h2[0].h - 1.5f) < 1e-4);  // H/h ze STR dopisane do nocy z karty
  }
  // A niepełna bez czasu (doba tylko ze STR w raporcie JSON) potem S: S zastępuje; odwrotnie: S zostaje
  {
    std::vector<Noc> h;
    wpiszNocAB(h, nA(d, false, -1), false, false);
    SPRAWDZ("pierwszeństwo nocy", wpiszNocS(h, nS(d)) && h[0].zrodlo == 'S' && h[0].m01 == 420);
    bool zost = false;
    SPRAWDZ("pierwszeństwo nocy", !wpiszNocAB(h, nA(d, false, -1), true, false, &zost) && zost && h[0].zrodlo == 'S');
  }
  // A „częściowa” z czasem potem S: A zostaje; S potem A „częściowa”: S zostaje
  {
    std::vector<Noc> h;
    wpiszNocAB(h, nA(d, false, 200, true), false, false);
    SPRAWDZ("pierwszeństwo nocy", !wpiszNocS(h, nS(d)) && h[0].zrodlo == 'A' && h[0].czesciowa);
    std::vector<Noc> h2;
    wpiszNocS(h2, nS(d));
    bool zost = false;
    SPRAWDZ("pierwszeństwo nocy", !wpiszNocAB(h2, nA(d, false, 200, true), false, false, &zost) && h2[0].zrodlo == 'S');
  }
  // S na S: inna treść zastępuje, ta sama nie; brak pna w nowej nie kasuje starej
  {
    std::vector<Noc> h;
    Noc a = nS(d);
    a.pna = "Model";
    wpiszNocS(h, a);
    SPRAWDZ("pierwszeństwo nocy", !wpiszNocS(h, nS(d)) && h[0].pna == "Model");
    SPRAWDZ("pierwszeństwo nocy", wpiszNocS(h, nS(d, 300)) && h[0].m01 == 300 && h[0].pna == "Model");
  }
  // noc S nie jest „kompletną nocą z AirBridge”: plan nadal pyta AirBridge o pełną noc dziś
  {
    std::vector<Noc> h = {nS(d)};
    SPRAWDZ("pierwszeństwo nocy", !nocKompletnaDzis(h, d, {}));
  }
  // wpiszNocS odrzuca rekord bez źródła S i ze złą datą
  {
    std::vector<Noc> h;
    Noc x = nS("2026-02-30");
    SPRAWDZ("pierwszeństwo nocy", !wpiszNocS(h, x) && h.empty());
    Noc y = nS(d);
    y.zrodlo = 'A';
    SPRAWDZ("pierwszeństwo nocy", !wpiszNocS(h, y) && h.empty());
  }
}

// ============================ migracja dwóch plików rocznych 3 → 4 ============================
static const char* POLA3 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb;m18";
static void testMigracja() {
  Dziennik d;
  AtrapaPlikow pl(d);
  pl.montujeSie = true;
  pl.montowane = true;
  std::string w25 = wierszNocyV3(nA("2025-12-30", true, 400)), w26 = wierszNocyV3(nA("2026-01-02", true, 410));
  std::string w26s = wierszNocyV3(nS("2026-01-03"));
  pl.pliki["/dane/noce/2025.csv"] = std::string("#cpap-eink noce 3\n") + POLA3 + "\n" + w25 + "\n#koniec 1\n";
  // drugi plik już w wersji 4 (restart w trakcie migracji po pierwszym pliku)
  pl.pliki["/dane/noce/2026.csv"] = std::string("#cpap-eink noce 4\n") + POLA3 + "\n" + w26 + "\n" + w26s + "\n#koniec 2\n";
  std::vector<std::string> log;
  Magazyn m(pl, [&](const std::string& s) { log.push_back(s); });
  SPRAWDZ("migracja plików", m.migrujWMiejscu());
  SPRAWDZ("migracja plików", pl.tresc("/dane/noce/2025.csv").compare(0, 18, "#cpap-eink noce 4\n") == 0);
  SPRAWDZ("migracja plików", pl.istnieje("/dane/noce/2025.csv.v3") && !pl.istnieje("/dane/noce/2026.csv.v3"));
  OdczytPliku a = m.czytaj("noce", "/dane/noce/2025.csv"), b = m.czytaj("noce", "/dane/noce/2026.csv");
  SPRAWDZ("migracja plików", a.stan == StanPliku::DOBRY && a.wiersze.size() == 1 && a.wiersze[0] == w25);
  SPRAWDZ("migracja plików", b.stan == StanPliku::DOBRY && b.wiersze.size() == 2 && b.wiersze[1] == w26s);
  // plik wersji 3 z wierszem `S` (nie powinien istnieć, ale np. po ręcznym wgraniu): wiersz nie ginie po migracji
  pl.pliki["/dane/noce/2027.csv"] = std::string("#cpap-eink noce 3\n") + POLA3 + "\n" + wierszNocyV3(nS("2027-01-01")) + "\n#koniec 1\n";
  SPRAWDZ("migracja plików", m.migrujWMiejscu());
  OdczytPliku c = m.czytaj("noce", "/dane/noce/2027.csv");
  Noc z;
  SPRAWDZ("migracja plików", c.stan == StanPliku::DOBRY && c.wiersze.size() == 1 && parsujNocV3(c.wiersze[0], z) && z.zrodlo == 'S');
}


// ============================ doba z AirBridge tylko ze STR zastąpiona nocą S wraca do planu pobrań ============================
// Wcześniej rekord A takiej doby (bez czasu z maską) liczył się jako „kompletny z AirBridge” i nie był pobierany
// ponownie. Po zastąpieniu nocą S plan pobiera ją jeszcze raz (raport niepełny → `zostawiony` → doba „pominięta”),
// czyli dodatkowe pobranie raportu na każdą taką dobę i licznik zaległych dób zawyżony do tego czasu.
static void testPlanPoNocyS() {
  const long dzien = D0;
  const std::string d = dataPoranka(dzien), dzis = dataZNr(D0 + 30);
  std::vector<DzienListy> lista(1);
  lista[0].day = dzien;
  std::vector<Noc> przed;
  wpiszNocAB(przed, nA(d, false, -1), false, false);
  SPRAWDZ("doba ze STR", planPobrania(lista, przed, {}, dzis, false, 10, nullptr, 0).empty());  // stan przed zmianą
  std::vector<Noc> po = przed;
  wpiszNocS(po, nS(d));
  if (!planPobrania(lista, po, {}, dzis, false, 10, nullptr, 0).empty()) {
    zle++;
    printf("BŁĄD [doba ze STR] doba %s (A tylko ze STR zastąpiona nocą S) znowu w planie pobrań\n", d.c_str());
  } else {
    ok++;
  }
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  testUszkodzone();
  testLosowe();
  testDataPoranka();
  testBrakDanych();
  testJednostki();
  testTryby();
  testPierwszenstwo();
  testMigracja();
  testPlanPoNocyS();
  testDataSkrajna();  // ostatni: pod -fsanitize=float-cast-overflow bez odzyskiwania przerywa program
  printf("testy_str_edf_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
