// SPDX-License-Identifier: GPL-3.0-or-later
#include "str.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "wspolne.h"

bool parsujSesjeStr(const std::string& t, InfoStr& wynik) {
  static const FiltrJson STR_F[] = {{"etag", nullptr}, {"size", nullptr}, {nullptr, nullptr}};
  static const FiltrJson F[] = {{"sessions", TYLKO_KLUCZ}, {"str", STR_F}, {nullptr, nullptr}};
  Json j;
  if (!parsujJson(t, j, F) || j.typ != Json::OBIEKT || !j["sessions"].jest()) return false;
  InfoStr s;
  const Json& st = j["str"];
  if (st.typ == Json::OBIEKT) {
    s.jest = true;
    if (st["etag"].typ == Json::TEKST) s.etag = st["etag"].s;
    double v;
    if (st["size"].liczba(v) && v >= 0) s.rozmiar = v;
  } else if (st.jest() && st.typ != Json::NUL) {
    return false;
  }
  wynik = s;
  return true;
}

// ---------- tabele z tools/kalibracja/noce.py (air10_edf.cpp i settings_defs.h AirBridge) ----------
namespace {

struct TagStr {
  const char* tag;
  const char* etykieta;
};
const TagStr TAG_STR[] = {
    {"MOP", "Mode"},           {"IPC", "S.C.Press"},        {"STP", "S.C.StartPress"},   {"MPA", "S.AS.MaxPress"},
    {"MPI", "S.AS.MinPress"},  {"STU", "S.AS.StartPress"},  {"AFC", "S.AS.Comfort"},     {"IPP", "S.BL.IPAP"},
    {"EPP", "S.BL.EPAP"},      {"MXI", "S.VA.MaxIPAP"},     {"MNE", "S.VA.MinEPAP"},     {"SPT", "S.VA.PS"},
    {"EEP", "S.AV.EPAP"},      {"MNS", "S.AV.MinPS"},       {"MXS", "S.AV.MaxPS"},       {"STE", "S.AV.StartPress"},
    {"EAI", "S.AA.MinEPAP"},   {"EAX", "S.AA.MaxEPAP"},     {"ANS", "S.AA.MinPS"},       {"AXS", "S.AA.MaxPS"},
    {"EAS", "S.AA.StartPress"}, {"RMA", "S.RampEnable"},    {"RMT", "S.RampTime"},       {"EPA", "S.EPR.ClinEnable"},
    {"EPX", "S.EPR.EPREnable"}, {"EPR", "S.EPR.Level"},     {"EPT", "S.EPR.EPRType"},    {"SST", "S.SmartStart"},
    {"MSK", "S.Mask"},         {"TBT", "S.Tube"},           {"CCO", "S.ClimateControl"}, {"HMX", "S.HumEnable"},
    {"HMS", "S.HumLevel"},     {"HTX", "S.TempEnable"},     {"HTS", "S.Temp"},
};

struct Opcje {
  const char* tag;
  const char* lista;  // po przecinku
};
const Opcje OPCJE[] = {
    {"MOP", "CPAP,AutoSet,APAP,S,ST,T,VAuto,ASV,ASVAuto,iVAPS,PAC,AutoSet Her"},
    {"AFC", "Standard,Soft"},
    {"RMA", "Off,On,Auto"},
    {"EPA", "Off,On"},
    {"EPX", "Off,On"},
    {"EPT", "Ramp Only,Full Time"},
    {"SST", "Off,On"},
    {"MSK", "Pillows,Full Face,Nasal,Pediatric"},
    {"TBT", "SlimLine,Standard,3m"},
    {"CCO", "Auto,Manual"},
    {"HMX", "Off,On"},
    {"HTX", "Off,On,Auto"},
};

// UKLAD_TRYBU i UKLAD_KOMFORTU (MODE_LAYOUT, COMFORT_LAYOUT), po numerze trybu `Mode`; nullptr kończy listę
const char* const UKLAD_TRYBU[][12] = {
    {"IPC", nullptr},
    {"MPA", "MPI", nullptr},
    {"MPA", "MPI", nullptr},
    {"IPP", "EPP", "BKR", "EBE", "ITN", "ITX", "RST", "VTS", "VCS", nullptr},
    {"IPP", "EPP", "RRT", "ITN", "ITX", "RST", "VTS", "VCS", nullptr},
    {"IPP", "EPP", "RRT", "ITT", "RST", nullptr},
    {"MXI", "MNE", "SPT", "ITN", "ITX", "VTS", "VCS", nullptr},
    {"EEP", "MNS", "MXS", nullptr},
    {"EAI", "EAX", "ANS", "AXS", nullptr},
    {"PHT", "IBR", "WMV", "EPI", "WPM", "WPA", "ITN", "ITX", "RST", "VTS", "VCS", nullptr},
    {"IPP", "EPP", "RRT", "ITT", "RST", "VTS", nullptr},
    {"MPA", "MPI", nullptr},
};
const char* const UKLAD_KOMFORTU[][3] = {{"STP", nullptr}, {"STU", "AFC", nullptr}, {"STU", "AFC", nullptr},
                                         {"EPS", nullptr}, {"EPS", nullptr},        {"EPS", nullptr},
                                         {"STV", nullptr}, {"STE", nullptr},        {"EAS", nullptr},
                                         {"IVS", nullptr}, {"EPS", nullptr},        {"STU", "AFC", nullptr}};
const int N_TRYBOW = (int)(sizeof(UKLAD_TRYBU) / sizeof(UKLAD_TRYBU[0]));
const char* const TAGI_PO[] = {"RMA", "RMT", "EPR", "EPA", "EPX", "EPT",  // TAGI_EPR
                               "SST",                                     // SmartStart
                               "MSK", "TBT", "CCO", "HMS", "HMX", "HTF", "HTS", "HTX"};  // TAGI_AKCESORIOW

// id kanału, jednostka wyniku (KANALY w noce.py, CHANNELS w edf_report.cpp)
struct KanalStr {
  const char* id;
  const char* jednostka;
};
const KanalStr KANALY_STR[] = {{"Press", "cmH2O"}, {"EprPress", "cmH2O"}, {"MaskPress", "cmH2O"}, {"Leak", "L/min"},
                               {"MinVent", "L/min"}, {"RespRate", "bpm"}, {"TidVol", "mL"},       {"SpO2", "%"},
                               {"Pulse", "bpm"},   {"FlowLim", ""},       {"Snore", ""},          {"TgtVent", "L/min"}};

std::string fmt8g(double v) {  // jak format(v, ".8g") w Pythonie
  char b[40];
  snprintf(b, sizeof b, "%.8g", v);
  return b;
}

Json tekstJ(const std::string& s) {
  Json j;
  j.typ = Json::TEKST;
  j.s = s;
  return j;
}
Json liczbaJ(double v) {
  Json j;
  if (std::isnan(v)) {
    j.typ = Json::NUL;
    return j;
  }
  j.typ = Json::LICZBA;
  j.n = v;
  return j;
}
void dodaj(Json& o, const char* klucz, Json v) {
  o.klucze.push_back(klucz);
  o.el.push_back(std::move(v));
}
Json obiekt() {
  Json j;
  j.typ = Json::OBIEKT;
  return j;
}

const char* etykietaTagu(const std::string& tag) {
  for (auto& t : TAG_STR)
    if (tag == t.tag) return t.etykieta;
  return nullptr;
}

}  // namespace

// zakres dób STR [stała techniczna]: 01.01.1990 (7305) … 01.01.2100 (47482), dni od 1970-01-01
static const long DZIEN_STR_OD = 7305, DZIEN_STR_DO = 47482;

edf::Blad czytajStr(std::string bajty, Str& wynik) {
  Str s;
  s.bajty = std::move(bajty);
  edf::Blad b = edf::czytaj((const uint8_t*)s.bajty.data(), s.bajty.size(), s.plik);
  if (b != edf::Blad::BRAK) return b;
  long startDnia = (long)(s.plik.startS >= 0 ? s.plik.startS / 86400 : -((-s.plik.startS + 86399) / 86400));
  for (long r = 0; r < s.plik.rekordy; r++) {
    double data = wartoscStr(s, r, "Date");
    long dzien = startDnia + r;
    if (!std::isnan(data)) {
      // dzień spoza 1990–2099 (uszkodzony opis sygnału) pominięty przed rzutowaniem na long
      if (!(data >= (double)DZIEN_STR_OD && data < (double)DZIEN_STR_DO)) continue;
      dzien = (long)std::nearbyint(data);
    }
    if (dzien < DZIEN_STR_OD || dzien >= DZIEN_STR_DO) continue;
    bool obecny = !std::isnan(wartoscStr(s, r, "Mode")) || !std::isnan(wartoscStr(s, r, "Duration"));
    if (obecny) s.doby[dzien] = r;
  }
  wynik = std::move(s);
  return edf::Blad::BRAK;
}

double wartoscStr(const Str& s, long rekord, const std::string& etykieta) {
  const edf::Sygnal* g = s.plik.sygnal(etykieta);
  if (!g || g->probki != 1 || g->adnotacje()) return NAN;
  uint16_t raw = s.plik.surowa(*g, rekord, 0);
  if (raw == 0xFFFF) return NAN;  // brak wartości (AirBridge)
  long cyf = (g->cyfMin < 0 && raw >= 32768) ? (long)raw - 65536 : (long)raw;
  return g->poprawna(cyf) ? g->fizyczna(cyf) : NAN;
}

Json ustawieniaStr(const Str& s, long rekord) {
  Json u = obiekt();
  Json pola;
  pola.typ = Json::TABLICA;
  auto dodajTag = [&](const char* tag) {
    const char* e = etykietaTagu(tag);
    if (!e) return;
    double v = wartoscStr(s, rekord, e);
    if (std::isnan(v)) return;
    Json f = obiekt();
    std::string tekst = fmt8g(v);
    double opcja = NAN;
    for (auto& o : OPCJE) {
      if (std::strcmp(o.tag, tag) != 0 || v < 0 || v != std::floor(v)) continue;
      long nr = (long)v, i = 0;
      const char* a = o.lista;
      while (i < nr && a) {
        a = std::strchr(a, ',');
        if (a) a++;
        i++;
      }
      if (a) {
        const char* k = std::strchr(a, ',');
        tekst = k ? std::string(a, (size_t)(k - a)) : std::string(a);
        opcja = (double)nr;
      }
    }
    dodaj(f, "tag", tekstJ(tag));
    dodaj(f, "value", tekstJ(tekst));
    dodaj(f, "option", liczbaJ(opcja));
    pola.el.push_back(std::move(f));
  };
  dodajTag("MOP");
  double tryb = wartoscStr(s, rekord, "Mode");
  if (!std::isnan(tryb) && tryb == std::floor(tryb) && tryb >= 0 && tryb < N_TRYBOW) {
    for (const char* t : UKLAD_TRYBU[(int)tryb]) {
      if (!t) break;
      dodajTag(t);
    }
    for (const char* t : UKLAD_KOMFORTU[(int)tryb]) {
      if (!t) break;
      dodajTag(t);
    }
  }
  for (const char* t : TAGI_PO) dodajTag(t);
  dodaj(u, "available", [] {
    Json b;
    b.typ = Json::BOOL;
    b.b = true;
    return b;
  }());
  dodaj(u, "fields", std::move(pola));
  return u;
}

static const char* jednostkaKanalu(const std::string& id) {
  for (auto& k : KANALY_STR)
    if (id == k.id) return k.jednostka;
  return nullptr;
}

double statystykaStr(const Str& s, long rekord, const std::string& id, const std::string& przyrostek) {
  const char* oczek = jednostkaKanalu(id);
  if (!oczek) return NAN;
  std::string baza = id == "Press" ? "TgtIPAP" : id == "EprPress" ? "TgtEPAP" : id;
  std::string e = baza + "." + przyrostek;
  const edf::Sygnal* g = s.plik.sygnal(e);
  double v = wartoscStr(s, rekord, e);
  if (!g || std::isnan(v)) return NAN;
  double skala = 1;
  if (id == "Leak" && g->jednostka == "L/s") skala = 60;
  if (id == "TidVol" && g->jednostka == "L") skala = 1000;
  // `ml` i `mL` to ta sama jednostka (jak M-14 z raportu JSON, src/airbridge.cpp kanalyRaportu())
  bool zgodna = g->jednostka == oczek || (id == "TidVol" && g->jednostka == "ml");
  if (skala == 1 && !zgodna) return NAN;  // jednostka inna niż oczekiwana: brak, bez zgadywania skali
  return v * skala;
}

Json kanalyStr(const Str& s, long rekord) {
  Json k;
  k.typ = Json::TABLICA;
  for (auto& x : KANALY_STR) {
    double med = statystykaStr(s, rekord, x.id, "50"), p95 = statystykaStr(s, rekord, x.id, "95");
    if (std::isnan(med) && std::isnan(p95)) continue;
    Json c = obiekt();
    dodaj(c, "id", tekstJ(x.id));
    dodaj(c, "unit", tekstJ(x.jednostka));
    dodaj(c, "median", liczbaJ(med));
    dodaj(c, "p95", liczbaJ(p95));
    dodaj(c, "source", tekstJ("str"));
    k.el.push_back(std::move(c));
  }
  return k;
}

static float naFloatStr(double v) { return std::isfinite(v) && std::fabs(v) <= 3.0e38 ? (float)v : NAN; }

bool nocZeStr(const Str& s, long dzien, Noc& wynik, const std::string& pna, const std::string& srn) {
  auto i = s.doby.find(dzien);
  if (i == s.doby.end()) return false;
  long r = i->second;
  double czas = wartoscStr(s, r, "Duration");
  if (std::isnan(czas) || czas <= 0 || czas > 1440) return false;
  std::string data = dataPoranka(dzien);
  if (!dataOk(data)) return false;
  Noc n;
  n.data = data;
  n.zrodlo = 'S';
  n.m01 = (int)std::lround(czas);
  if (n.m01 <= 0) return false;  // poniżej pół minuty: jak brak terapii
  n.ahi = naFloatStr(wartoscStr(s, r, "AHI"));
  auto naGodzine = [&](const char* e) {
    double v = wartoscStr(s, r, e);
    return v >= 0 ? naFloatStr(v) : NAN;
  };
  n.oa = naGodzine("OAI");
  n.ca = naGodzine("CAI");
  n.h = naGodzine("HI");
  n.ua = naGodzine("UAI");
  bool kanalEdf = false;
  kanalyRaportu(n, kanalyStr(s, r), kanalEdf);
  bool maMop = false;
  std::string tryb = ustawieniaRaportu(n, ustawieniaStr(s, r), maMop);
  n.tryb = czystePole(tryb);
  n.pna = czystePole(pna);
  n.srn = czystePole(srn);
  // ważność jak noc z AirBridge (4.2, R-DANE-13, R-DANE-29): ≥ 240 min, znane AHI (odpowiednik `events_known`),
  // tryb obsługiwany; noc ze STR nie bywa „częściowa”
  n.wazna = n.m01 >= 240 && !std::isnan(n.ahi) && trybObslugiwany(tryb);
  wynik = n;
  return true;
}

std::vector<Noc> noceZeStr(const Str& s, const std::string& dzis, const std::string& pna, const std::string& srn) {
  std::vector<Noc> w;
  for (auto& d : s.doby) {
    Noc n;
    if (!nocZeStr(s, d.first, n, pna, srn)) continue;
    if (!dzis.empty() && n.data > dzis) continue;
    w.push_back(n);
  }
  return w;
}

bool wpiszNocS(std::vector<Noc>& hist, const Noc& n) {
  if (!dataOk(n.data) || n.zrodlo != 'S') return false;
  for (auto& h : hist) {
    if (h.data != n.data) continue;
    if (h.zrodlo == 'S') {
      if (trescNocy(h) == trescNocy(n)) return false;
      Noc m = n;
      if (m.pna.empty()) m.pna = h.pna;  // brak odczytu aparatu nie jest zmianą (R-NOR-25)
      if (m.srn.empty()) m.srn = h.srn;
      if (trescNocy(h) == trescNocy(m)) return false;
      h = m;
      return true;
    }
    if (h.zrodlo != 'A' || rekordPelny(h) || h.m01 >= 0) return false;  // K, pełna A, A z czasem z maską
    h = n;
    return true;
  }
  hist.push_back(n);
  return true;
}
