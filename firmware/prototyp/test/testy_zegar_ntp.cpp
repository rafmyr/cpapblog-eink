// SPDX-License-Identifier: GPL-3.0-or-later
// zegar nowego panelu (R-AW-08, R-AW-18, R-AW-20, T-WYM-80). Panel po `erase` nie ma czasu:
// NTP dostaje 3 dłuższe próby, potem czas z `esp_time` w `/api/status` i ten sam przebieg pobiera noce i rysuje dane.
// Zegar znany: jedna próba 2 s, `esp_time` ignorowany. Dane syntetyczne.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "magazyn.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
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

static std::string pole(const char* tag, const std::string& v, int opcja = -1) {
  std::string s = std::string("{\"tag\":\"") + tag + "\",\"label\":\"x\",\"value\":\"" + v + "\",\"unit\":\"\"";
  if (opcja >= 0) s += ",\"option\":" + std::to_string(opcja);
  return s + "}";
}
static std::string ustAutoSet(const std::string& mpi, const std::string& mpa, const std::string& epr) {
  return pole("MOP", "AutoSet", 1) + "," + pole("MPI", mpi) + "," + pole("MPA", mpa) + "," + pole("EPR", epr) + "," +
         pole("RMA", "Off") + "," + pole("HMS", "3");
}
static std::string kanal(const char* id, const char* j, double med, double p95) {
  char b[256];
  snprintf(b, sizeof b,
           "{\"id\":\"%s\",\"label\":\"x\",\"unit\":\"%s\",\"count\":12600,\"coverage_seconds\":25200,\"min\":1,"
           "\"mean\":%g,\"median\":%g,\"p95\":%g,\"max\":%g,\"source\":\"edf\"}",
           id, j, med, med, p95, p95 + 1);
  return b;
}
// Raport dnia aparatu AirSense: bez EAI, EAX, ANS, AXS, EEP, MNS, MXS; `ustawienia` = zawartość settings.fields.
static std::string raport(long day, double pMed, double pP95, const std::string& ustawienia, int sekund = 25200,
                          double eprMed = NAN, double eprP95 = NAN) {
  std::string d = std::to_string(day);
  unsigned long long k = (unsigned long long)(day * 86400 + 86400 + 5 * 3600), s = k - (unsigned long long)sekund;
  if (std::isnan(eprMed)) { eprMed = pMed - 2; eprP95 = pP95 - 2; }
  return "{\"source\":\"edf\",\"revision\":1,\"view\":\"day\",\"day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":" + std::to_string(s * 1000ULL) + ",\"end_ms\":" + std::to_string(k * 1000ULL) +
         "}],\"present\":true,\"partial\":false,\"used_seconds\":" + std::to_string(sekund) +
         ",\"rdi\":2,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":60,\"session_count\":1,\"ahi\":1.4,"
         "\"events_known\":true,\"counts\":{\"oa\":3,\"ca\":2,\"ua\":0,\"h\":5,\"rera\":1},\"channels\":[" +
         kanal("Press", "cmH2O", pMed, pP95) + "," + kanal("EprPress", "cmH2O", eprMed, eprP95) + "," +
         kanal("Leak", "L/min", 2, 9) + "," + kanal("MinVent", "L/min", 7, 10) + "," + kanal("RespRate", "/min", 14, 18) +
         "," + kanal("TidVol", "L", 0.5, 0.7) + "," + kanal("FlowLim", "", 0.02, 0.1) +
         "],\"settings\":{\"day\":" + d + ",\"available\":" + (ustawienia.empty() ? "false" : "true") + ",\"fields\":[" +
         ustawienia + "]}}";
}

static bool logMa(const Panel& p, const std::string& t) {
  for (auto& l : p.port.log)
    if (l.find(t) != std::string::npos) return true;
  return false;
}

static std::string status(const std::string& czas) {
  std::string s = "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirSense_10_AutoSet\",\"srn\":\"22222222222\","
                  "\"mode\":\"AutoSet\",\"system\":\"IDLE\",\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\","
                  "\"report_state\":\"idle\"";
  if (!czas.empty()) s += ",\"esp_time\":\"" + czas + "\"";
  return s + "}";
}

// Jedno wybudzenie. epoch0 = 0: zegar nieznany (nowy panel po erase); NTP: ntpJest, czasNtp.
struct Sym {
  Panel p;
  long dzien = nrDnia("2026-11-06");
  void wybudz(uint32_t epoch0, PowodWybudzenia pw, const std::string& czasAb) {
    p.siec.odpowiedzi.clear();
    p.siec.siecJest = true;
    p.siec.dodaj("/api/status", 200, status(czasAb));
    p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[{\"day\":" + std::to_string(dzien) + ",\"session_count\":1,\"stale\":false}]}");
    p.siec.dodaj(sciezkaNocy(dzien), 200, raport(dzien, 9.5, 12, ustAutoSet("6.0", "14.0", "3")));
    p.zegar.epoch = epoch0;
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = pw;
    p.siec.polaczonaTeraz = false;
    p.siec.okno = false;
    p.port.log.clear();
    p.d.z.clear();
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
  }
  void log() const {
    if (getenv("ZEGAR_LOG")) for (auto& l : p.port.log) printf("  | %s\n", l.c_str());
  }
};

static void parser() {
  std::tm t{};
  SPRAWDZ(parsujCzasAb("2026-11-01 21:09", t) && t.tm_year == 126 && t.tm_mon == 10 && t.tm_mday == 1 && t.tm_hour == 21 && t.tm_min == 9);
  for (const char* z : {"--", "", "2026-11-01", "2026-11-01 21:09:30", "2026-11-01T21:09", "2026-13-01 21:09", "2026-02-30 10:00",
                        "2026-11-01 24:00", "2026-11-01 21:60", "2026-11-01 2l:09", "abcd-ef-gh ij:kl", " 2026-11-01 21:09"})
    SPRAWDZ(!parsujCzasAb(z, t));
  StatusAB st;
  SPRAWDZ(parsujStatus(status("2026-11-01 21:09"), st) && st.czas == "2026-11-01 21:09");
  SPRAWDZ(parsujStatus(status(""), st) && st.czas.empty());
  SPRAWDZ(parsujStatus("{\"version\":\"v2.2.1\",\"esp_time\":5}", st) && st.czas.empty());
}

// (a) panel bez zegara, NTP milczy, status ma esp_time: zegar, log, noce i ekran z danymi w tym samym wybudzeniu
static void a_bezNtpZAirBridge() {
  Sym s;
  s.p.siec.ntpJest = false;
  s.wybudz(0, PowodWybudzenia::START, "2026-11-07 06:30");
  s.log();
  SPRAWDZ(s.p.d.ile("siec.ntp") == 3);                       // R-AW-18: 3 próby
  SPRAWDZ(s.p.zegar.ustawienia == 1);
  SPRAWDZ(logMa(s.p, "źródło czasu: AirBridge"));
  SPRAWDZ(s.p.zegar.teraz() > CZAS_ZNANY_OD);
  // zegar ustawiony na czas lokalny z esp_time (06:30), potem tylko czas wybudzenia (okno przywracania ok. 10 min)
  SPRAWDZ(s.p.zegar.teraz() >= ep(2026, 11, 7, 6, 30) && s.p.zegar.teraz() < ep(2026, 11, 7, 6, 30) + 3600);
  std::string pl = s.p.pliki.tresc("/dane/noce/2026.csv");
  SPRAWDZ(pl.find("2026-11-07;1;420;") != std::string::npos);  // noc pobrana w tym samym wybudzeniu
  SPRAWDZ(s.p.ekran.odswiezen >= 1);                           // ekran z danymi
  SPRAWDZ(!logMa(s.p, "następny termin: czas"));
  SPRAWDZ(logMa(s.p, "NTP: brak odpowiedzi"));
  SPRAWDZ(logMa(s.p, "ostatnia synchronizacja brak"));         // zegar z AirBridge to nie synchronizacja NTP
  // limit 3 prób: 3 × 5 s (zegar nieznany)
  SPRAWDZ(s.p.zegar.ms() >= 15000);
}

// (b) NTP odpowiada za 2. próbą: AirBridge ignorowany, zegar z NTP
struct SiecDruga : AtrapaSieci {
  SiecDruga(Dziennik& d, AtrapaZegara& z) : AtrapaSieci(d, z) {}
  int proby = 0;
  bool czasZSieci(uint32_t limitMs) override {
    ntpJest = ++proby >= 2;
    czasNtp = ep(2026, 11, 7, 5, 45);
    return AtrapaSieci::czasZSieci(limitMs);
  }
};
static void b_ntpZaDrugaProba() {
  Dziennik d;
  Sym s;
  // podmiana sieci: komplet atrap z własną siecią
  AtrapaZegara& z = s.p.zegar;
  SiecDruga siec(s.p.d, z);
  Adaptery ad{z, s.p.nvs, s.p.pliki, siec, s.p.ekran, s.p.przyciski, s.p.czujniki, s.p.dzwiek, s.p.start, s.p.straznik, s.p.sen, s.p.port, s.p.diag};
  siec.dodaj("/api/status", 200, status("2026-11-07 06:30"));
  siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[]}");
  z.epoch = 0;
  s.p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  s.p.start.powodWybudzenia = PowodWybudzenia::START;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  Dyrygent(ad, KONF).wybudzenie();
  SPRAWDZ(siec.proby == 2);
  SPRAWDZ(z.ustawienia == 0);                                  // esp_time nieużyty
  SPRAWDZ(!logMa(s.p, "źródło czasu: AirBridge"));
  SPRAWDZ(logMa(s.p, "NTP: zsynchronizowany"));
  SPRAWDZ(logMa(s.p, "NTP: próba 1 z 3 bez odpowiedzi"));
  SPRAWDZ(z.teraz() >= ep(2026, 11, 7, 5, 45) && z.teraz() < ep(2026, 11, 7, 5, 45) + 3600);
}

// (c) esp_time zły albo brak: U-04, bez E-01, następna próba po 30 min (R-AW-20)
static void c_zlyEspTime() {
  for (const char* czas : {"", "--", "nie-czas", "2026-02-30 10:00", "2001-01-01 10:00", "2023-11-14 10:00"}) {
    Sym s;
    s.p.siec.ntpJest = false;
    s.wybudz(0, PowodWybudzenia::START, czas);
    s.log();
    SPRAWDZ(s.p.zegar.ustawienia == 0);
    SPRAWDZ(s.p.zegar.teraz() < CZAS_ZNANY_OD);
    SPRAWDZ(!logMa(s.p, "źródło czasu: AirBridge"));
    SPRAWDZ(logMa(s.p, "czas z AirBridge:") && logMa(s.p, "sen na 1800 s"));
    SPRAWDZ(!logMa(s.p, "E-01"));
    SPRAWDZ(s.p.pliki.tresc("/dane/noce/2026.csv").find("2026-11-07;1;420;") == std::string::npos);
  }
}

// (d) zegar znany: limit 2 s, jedna próba, esp_time ignorowany (R-AW-19, zegar nie cofa się)
static void d_zegarZnany() {
  Sym s;
  s.p.siec.ntpJest = false;
  s.wybudz(ep(2026, 11, 7, 6, 30), PowodWybudzenia::ZEGAR, "2026-11-07 09:00");
  s.log();
  SPRAWDZ(s.p.d.ile("siec.ntp") == 1);
  SPRAWDZ(s.p.zegar.ustawienia == 0);
  SPRAWDZ(!logMa(s.p, "źródło czasu: AirBridge"));
  SPRAWDZ(!logMa(s.p, "czas z AirBridge:"));
  SPRAWDZ(s.p.zegar.teraz() >= ep(2026, 11, 7, 6, 30) && s.p.zegar.teraz() < ep(2026, 11, 7, 7, 30));  // nie 09:00
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  parser();
  a_bezNtpZAirBridge();
  b_ntpZaDrugaProba();
  c_zlyEspTime();
  d_zegarZnany();
  printf("testy_zegar_ntp: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
