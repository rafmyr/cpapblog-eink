// SPDX-License-Identifier: GPL-3.0-or-later
// Scenariusze z dyrygentem na atrapach (slot dzienny w przebiegu wybudzeń): prognoza godzinowa z serwisu
// pogody pobrana przy starcie slotu (R-KON-52, atrapa odpowiedzi Open-Meteo), wybór ekranu S-K-33…S-K-38 o pełnych
// godzinach (R-KON-42), obrazek dnia z pamięci panelu i dziennik /obrazki.csv (R-KON-61), S-K-37 bez obrazka
// (R-KON-54), S-K-01 wieczorem z CO2 z lokalnego źródła otoczenia, bez współrzędnych bez zapytań do serwisu
// (R-KON-53). Pomocnicze funkcje symulacji doby skopiowane z test/testy_slot_dzienny_w_planie.cpp (tam statyczne).
// Uruchomienie: tools/prototyp/testy.sh (program testy_slot_dzienny_dyrygent).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "obrazki.h"
#include "plan.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE_L(a, b)                                                                                          \
  do {                                                                                                         \
    long a_ = (long)(a), b_ = (long)(b);                                                                       \
    if (a_ == b_) ok++;                                                                                        \
    else { zle++; printf("BŁĄD %s:%d: %s = %ld, ma być %ld\n", __FILE__, __LINE__, #a, a_, b_); }            \
  } while (0)

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";  // jak test/testy_plan_wybudzen.cpp

static uint32_t ep(int r, int mi, int d, int h, int m, int s = 0) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_BLOKADA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";

static const long DOBA_A = nrDnia("2026-11-03");  // noc z poranka 4.11.2026

// Raport doby jak w test/testy_plan_wybudzen.cpp raport(), z sesją od `startEp` do `koniecEp` (czas unix): koniec terapii
// ostatniej nocy jako `end_ms` ostatniej sesji (Załącznik A).
static std::string raport(long day, uint32_t startEp, uint32_t koniecEp) {
  std::string d = std::to_string(day);
  std::string s = std::to_string((unsigned long long)startEp * 1000ULL);
  std::string k = std::to_string((unsigned long long)koniecEp * 1000ULL);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":" + s + ",\"end_ms\":" + k + "}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25200,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Press\",\"label\":\"C\",\"unit\":\"cmH2O\",\"count\":12600,"
         "\"coverage_seconds\":25200,\"min\":6.2,\"mean\":10.1,\"median\":9.8,\"p95\":13.6,\"max\":16.0,"
         "\"source\":\"edf\"},{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12600,"
         "\"coverage_seconds\":25200,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}

// Pomiar jednego wybudzenia (jak test/testy_plan_wybudzen.cpp Pomiar): czas z radiem, rysowanie przy radiu, chwila snu.
struct Pomiar {
  bool radio = false;
  uint32_t radioOdMs = 0, wifiMs = 0, msPrzySnie = 0;
  int polaczen = 0, zadan = 0, rysowanZRadiem = 0, odswiezen = 0;
  bool radioPrzySnie = false;
  void zeruj() { *this = Pomiar(); }
  void zamknijRadio(uint32_t teraz) {
    if (!radio) return;
    wifiMs += teraz - radioOdMs;
    radio = false;
  }
};

class SiecPomiar : public AtrapaSieci {
 public:
  SiecPomiar(Dziennik& d, AtrapaZegara& z, Pomiar& m) : AtrapaSieci(d, z), z_(z), m_(m) {}
  bool polacz(uint32_t limitMs, const std::function<bool()>& przerwij, bool& przerwane) override {
    if (!m_.radio) { m_.radio = true; m_.radioOdMs = z_.msTeraz; }
    m_.polaczen++;
    return AtrapaSieci::polacz(limitMs, przerwij, przerwane);
  }
  OdpowiedzHttp get(const ZadanieHttp& z) override {
    m_.zadan++;
    return AtrapaSieci::get(z);
  }
  void wylacz() override {
    m_.zamknijRadio(z_.msTeraz);
    AtrapaSieci::wylacz();
  }
 private:
  AtrapaZegara& z_;
  Pomiar& m_;
};

class EkranPomiar : public AtrapaWyswietlacza {
 public:
  EkranPomiar(Dziennik& d, Pomiar& m) : AtrapaWyswietlacza(d), m_(m) {}
  void odswiez() override {
    m_.odswiezen++;
    if (m_.radio) m_.rysowanZRadiem++;
    AtrapaWyswietlacza::odswiez();
  }
 private:
  Pomiar& m_;
};

class SenPomiar : public AtrapaSnu {
 public:
  SenPomiar(Dziennik& d, AtrapaUruchomienia& u, AtrapaStraznika& s, AtrapaZegara& z, Pomiar& m)
      : AtrapaSnu(d, u, s), z_(z), m_(m) {}
  void spij(const PlanSnu& p) override {
    m_.msPrzySnie = z_.msTeraz;
    m_.radioPrzySnie = m_.radio;
    m_.zamknijRadio(z_.msTeraz);
    AtrapaSnu::spij(p);
  }
 private:
  AtrapaZegara& z_;
  Pomiar& m_;
};

struct PanelSym {
  Panel p;
  Pomiar m;
  SiecPomiar siec{p.d, p.zegar, m};
  EkranPomiar ekran{p.d, m};
  SenPomiar sen{p.d, p.start, p.straznik, p.zegar, m};
  Adaptery adaptery() {
    return Adaptery{p.zegar, p.nvs, p.pliki, siec, ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik, sen,
                    p.port, p.diag};
  }
};

struct Wyb {
  uint32_t ep = 0;
  PowodWybudzenia powod = PowodWybudzenia::ZEGAR;
  int minuta = 0;
  std::string data;
  Pomiar m;
  bool spal = false;
  std::vector<std::string> zdarzenia;
};
static int minutaLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  return l.tm_hour * 60 + l.tm_min;
}
static std::string dataLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  char b[16];
  strftime(b, sizeof b, "%Y-%m-%d", &l);
  return b;
}

static Wyb wybudz(PanelSym& s, uint32_t t, PowodWybudzenia pw, const KonfiguracjaPanelu& k) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.siec.polaczonaTeraz = false;
  s.siec.okno = false;
  s.p.d.z.clear();
  s.m.zeruj();
  int snow = s.sen.snow;
  Dyrygent(s.adaptery(), k).wybudzenie();
  Wyb w;
  w.ep = t;
  w.powod = pw;
  w.minuta = minutaLok(t);
  w.data = dataLok(t);
  w.m = s.m;
  w.spal = s.sen.snow == snow + 1;
  w.zdarzenia = s.p.d.z;
  return w;
}

using Odpowiedzi = void (*)(PanelSym&, uint32_t);

// Kolejne wybudzenia od startu zasilania o `od` do `doT`, każde po śnie z planu dyrygenta. `zmiennaTemp`: czujnik
// w obudowie co godzinę inny (stopka `przy łóżku` się zmienia), inaczej stały odczyt atrapy.
static std::vector<Wyb> symuluj(PanelSym& s, uint32_t od, uint32_t doT, Odpowiedzi odp, bool zmiennaTemp = false,
                                const KonfiguracjaPanelu& k = KONF) {
  std::vector<Wyb> v;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 600 && t < doT; i++) {
    odp(s, t);
    if (zmiennaTemp) s.p.czujniki.o.temp = 15.0f + (float)(minutaLok(t) / 60);
    Wyb w = wybudz(s, t, pw, k);
    v.push_back(w);
    if (!w.spal) break;
    uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
    if (snu == 0) snu = 1;
    t += w.m.msPrzySnie / 1000 + snu;
    pw = PowodWybudzenia::ZEGAR;
  }
  return v;
}

static std::vector<std::string> zadaniaWyb(const Wyb& w) {
  std::vector<std::string> u;
  for (auto& z : w.zdarzenia)
    if (z.compare(0, 9, "siec.get ") == 0) u.push_back(z.substr(9));
  return u;
}
static int zadanZeSciezka(const Wyb& w, const std::string& p) {
  int n = 0;
  for (auto& u : zadaniaWyb(w)) n += AtrapaSieci::sciezka(u).compare(0, p.size(), p) == 0;
  return n;
}


static KonfiguracjaPanelu konf(bool wspolrzedne) {
  KonfiguracjaPanelu k = KONF;
  if (wspolrzedne) { k.pogodaLat = 50.2597; k.pogodaLon = 19.0217; }
  return k;
}
// Open-Meteo: dziś i jutro co godzinę; za oknem 15° (przy zmiennej: 5° + godzina), PM2.5 10, opad 0 (bez trybu
// wietrzenia przy łóżku 15–20°)
static std::string jsonPrognozy(const std::string& d, bool zmiennaTemp = false) {
  std::string t = "\"time\":[", a = "\"temperature_2m\":[", o = "\"precipitation_probability\":[", c = "\"cloud_cover\":[",
              w = "\"wind_gusts_10m\":[";
  for (int i = 0; i < 48; i++) {
    std::string dz = i < 24 ? d : dodajDni(d, 1);
    char b[32];
    snprintf(b, sizeof b, "\"%sT%02d:00\"", dz.c_str(), i % 24);
    const char* s = i ? "," : "";
    t += s + std::string(b);
    a += s + (zmiennaTemp ? std::to_string(5 + i % 24) : std::string("15"));
    o += s + std::string("0");
    c += s + std::string("40");
    w += s + std::string("20");
  }
  return "{\"hourly\":{" + t + "]," + a + "]," + o + "]," + c + "]," + w + "]}}";
}
static std::string jsonPowietrza(const std::string& d) {
  std::string t = "\"time\":[", p = "\"pm2_5\":[";
  for (int i = 0; i < 48; i++) {
    std::string dz = i < 24 ? d : dodajDni(d, 1);
    char b[32];
    snprintf(b, sizeof b, "\"%sT%02d:00\"", dz.c_str(), i % 24);
    t += (i ? "," : "") + std::string(b);
    p += (i ? "," : "") + std::string("10");
  }
  return "{\"hourly\":{" + t + "]," + p + "]}}";
}
static void odpowiedziB9(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  if (t >= ep(2026, 11, 4, 6, 40) && t < ep(2026, 11, 5, 0, 0)) {
    s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
    s.siec.dodaj(sciezkaNocy(DOBA_A), 200, raport(DOBA_A, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40)));
  } else {
    s.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  }
  s.siec.dodaj("/v1/forecast", 200, jsonPrognozy("2026-11-04"));
  s.siec.dodaj("/v1/air-quality", 200, jsonPowietrza("2026-11-04"));
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":1250},\"ostrzezenia\":[]}");
}
static void odpowiedziB9ZmiennaTemp(PanelSym& s, uint32_t t) {
  odpowiedziB9(s, t);
  s.siec.odpowiedzi.erase("/v1/forecast");
  s.siec.dodaj("/v1/forecast", 200, jsonPrognozy("2026-11-04", true));
}
static bool logMa(const std::vector<Wyb>& v, const std::string& t, int odMin = 0, int doMin = 24 * 60) {
  for (auto& w : v)
    if (w.minuta >= odMin && w.minuta < doMin)
      for (auto& z : w.zdarzenia)
        if (z.find(t) != std::string::npos) return true;
  return false;
}
static int ileZadan(const std::vector<Wyb>& v, const std::string& p) {
  int n = 0;
  for (auto& w : v) n += zadanZeSciezka(w, p);
  return n;
}
static std::string pbm(int w, int h) {
  return "P4\n" + std::to_string(w) + " " + std::to_string(h) + "\n" + std::string((size_t)((w + 7) / 8 * h), '\x0F');
}

// Doba 4.11.2026 (S-K-38 w pierwszym bloku slotu każdego dnia): noc o 07:00, slot od 08:00, prognoza pobrana raz przy starcie slotu
static void testSlotDziennyZObrazkiem() {
  PanelSym s;
  s.p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
  s.p.pliki.pliki["/obrazki/002_syntetyczny.pbm"] = pbm(300, 200);
  s.p.pliki.pliki["/obrazki/obrazki.json"] =
      "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\",\"dni\":[\"sr\"]},{\"plik\":\"002_syntetyczny.pbm\"}]}";
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 20, 30), odpowiedziB9, false, konf(true));
  ROWNE_L(ileZadan(v, "/v1/forecast"), 1);      // R-KON-52: przy starcie slotu, prognoza świeża przez resztę dnia
  ROWNE_L(ileZadan(v, "/v1/air-quality"), 1);
  SPRAWDZ(logMa(v, "prognoza godzinowa: pobrana", 8 * 60, 8 * 60 + 1));
  SPRAWDZ(s.p.pliki.pliki.count("/dane/prognoza.csv") == 1);
  // przed startem slotu: ekran z rana; od startu S-K-38 (środa: pula kontekstowa z `sr`)
  SPRAWDZ(!logMa(v, "slot dzienny: S-K-38", 0, 8 * 60));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-38 /obrazki/001_syntetyczny.pbm (pula k)", 8 * 60, 8 * 60 + 1));
  // (R-KON-50): obrazek w pierwszym 2-godzinnym bloku slotu (08:00–10:00), potem D5: S-K-37 (T-TEST-341)
  SPRAWDZ(logMa(v, "slot dzienny: S-K-38", 9 * 60, 9 * 60 + 1));  // 09:00: ten sam obrazek (odświeżeń 0, niżej)
  SPRAWDZ(logMa(v, "slot dzienny: S-K-37", 10 * 60, 10 * 60 + 1));
  SPRAWDZ(!logMa(v, "slot dzienny: S-K-38", 10 * 60, 21 * 60));
  // R-KON-61: jeden wpis w dzienniku na dzień, mimo ocen o każdej pełnej godzinie
  std::string dz = s.p.pliki.pliki["/dane/obrazki.csv"];
  size_t pierwszy = dz.find("2026-11-04;001_syntetyczny.pbm;k");
  SPRAWDZ(pierwszy != std::string::npos && dz.find("2026-11-04;", pierwszy + 1) == std::string::npos);
  // R-KON-42: oceny bez zmiany klatki nie odświeżają e-ink
  int odswiezen = 0;
  for (auto& w : v)
    if (w.minuta > 8 * 60 && w.minuta < 10 * 60) odswiezen += w.m.odswiezen;
  ROWNE_L(odswiezen, 0);  // w bloku obrazka żadnego odświeżenia; zmiana dopiero o 10:00 (R-KON-42, R-KON-50)
  // po 21:00: S-K-01 z CO2 1250 ppm z lokalnego źródła (traktowanego jak Qingping)
  KonfiguracjaPanelu k = konf(true);
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  PanelSym q;
  std::vector<Wyb> u = symuluj(q, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 22, 0), odpowiedziB9, false, k);
  SPRAWDZ(logMa(u, "ekran wieczorny SK01", 21 * 60, 21 * 60 + 5));
  SPRAWDZ(q.p.pliki.pliki["/humor.csv"].find(";SK01;") != std::string::npos ||
          q.p.pliki.pliki["/dane/humor.csv"].find(";SK01;") != std::string::npos);
  SPRAWDZ(q.p.pliki.pliki.count("/co2-nocy.csv") == 0);  // r08: bez osobnego pliku serii CO2
}

// bez obrazków → S-K-37 (R-KON-54); bez współrzędnych → bez zapytań do serwisu i bez S-K-37 (R-KON-53)
static void testSlotBezObrazkowIBezMiasta() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 12, 30), odpowiedziB9, false, konf(true));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-37", 8 * 60, 8 * 60 + 1));
  PanelSym q;
  std::vector<Wyb> u = symuluj(q, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 12, 30), odpowiedziB9, false, konf(false));
  ROWNE_L(ileZadan(u, "/v1/"), 0);
  SPRAWDZ(!logMa(u, "slot dzienny: S-K-"));  // bez współrzędnych i obrazka: ekran z rana (W4)
}

// R-KON-56…R-KON-58 w przebiegu: drzemka 15:10–16:20 z maską; ocena o 16:00 z `rop` = 1 →
// bez nowego ekranu i bez buzzera, o 17:00 `rop` = 0 i nowy `edf_prefix` → raport doby bez nowego rekordu nocy,
// stopka z drzemką i S-K-39, gdy P95 doby (z drzemką) wyższe niż P95 nocy i ponad 24 L/min.
static void odpDrzemki(PanelSym& s, uint32_t t) {
  odpowiedziB9(s, t);
  s.siec.odpowiedzi.erase("/api/status");
  bool maska = t >= ep(2026, 11, 4, 15, 10) && t < ep(2026, 11, 4, 16, 20);
  bool po = t >= ep(2026, 11, 4, 16, 20);
  std::string st = std::string("{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",") +
                   "\"system\":\"" + (maska ? "THERAPY" : "IDLE") + "\",\"rop\":" + (maska ? "1" : "0") +
                   ",\"edf\":\"idle\",\"edf_prefix\":\"" + (maska || po ? "20261104_151000" : "20261103_232000") +
                   "\",\"report_state\":\"" + (maska ? "blocked" : "idle") + "\"}";
  if (t >= ep(2026, 11, 4, 6, 40)) s.siec.dodaj("/api/status", 200, st);
  else s.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  if (po) {
    std::string d = std::to_string(DOBA_A);
    auto ms = [](uint32_t e) { return std::to_string((unsigned long long)e * 1000ULL); };
    std::string r = raport(DOBA_A, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40));
    // druga sesja (drzemka) i wyższy P95 wycieku doby
    size_t a = r.find("}],\"present\"");
    r.insert(a + 1, ",{\"id\":2,\"day\":" + d + ",\"prefix\":\"Y\",\"start_ms\":" + ms(ep(2026, 11, 4, 15, 10)) +
                        ",\"end_ms\":" + ms(ep(2026, 11, 4, 16, 20)) + "}");
    size_t l = r.find("\"p95\":9.6");
    r.replace(l, 9, "\"p95\":30.0");
    s.siec.odpowiedzi.erase(sciezkaNocy(DOBA_A));
    s.siec.dodaj(sciezkaNocy(DOBA_A), 200, r);
  }
}
static void testDrzemka() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 17, 30), odpDrzemki, false, konf(true));
  SPRAWDZ(logMa(v, "drzemka trwa (rop 1)", 16 * 60, 16 * 60 + 1));
  int odsw16 = 0;
  for (auto& w : v)
    if (w.minuta == 16 * 60) odsw16 += w.m.odswiezen;
  ROWNE_L(odsw16, 0);  // R-KON-56: bez odświeżenia w drzemce
  SPRAWDZ(logMa(v, "drzemka: 70 min", 17 * 60, 17 * 60 + 1));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-39", 17 * 60, 17 * 60 + 1));
  // rekord nocy z raportu doby po drzemce nie powstaje (R-DANE-27): noc 4.11 ma dalej wyciek P95 nocy
  std::string noce = s.p.pliki.pliki["/dane/noce/2026.csv"];
  SPRAWDZ(noce.find("30.0") == std::string::npos);
}

// Od 06.11.2026 (R-KON-42): ocena o pełnej godzinie rysuje stopkę z odczytów klatki (za oknem
// z prognozy godzinowej i przy łóżku), więc sama zmiana prognozy i czujnika nie odświeża e-ink w bloku obrazka;
// przy zmianie ekranu (10:00, S-K-37) jedno odświeżenie ze świeżymi odczytami.
// wymaganie: R-KON-42
static void testR_KON_42_stopkaZKlatki() {
  PanelSym s;
  s.p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
  s.p.pliki.pliki["/obrazki/obrazki.json"] = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\"}]}";
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 11, 30), odpowiedziB9ZmiennaTemp, true,
                               konf(true));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-38", 8 * 60, 8 * 60 + 1));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-38", 9 * 60, 9 * 60 + 1));
  int odswiezen = 0;
  for (auto& w : v)
    if (w.minuta > 8 * 60 && w.minuta < 10 * 60) odswiezen += w.m.odswiezen;
  ROWNE_L(odswiezen, 0);  // 09:00: za oknem 14° zamiast 13°, przy łóżku 24° zamiast 23°: bez odświeżenia
  SPRAWDZ(logMa(v, "bez zmian: klatka jak wyświetlona", 9 * 60, 9 * 60 + 1));
  int o10 = 0;
  for (auto& w : v)
    if (w.minuta == 10 * 60) o10 += w.m.odswiezen;
  ROWNE_L(o10, 1);  // inny ekran (koniec bloku obrazka, R-KON-50): jedno odświeżenie, już ze świeżymi odczytami
  SPRAWDZ(!logMa(v, "slot dzienny: S-K-38", 10 * 60, 10 * 60 + 1));
}

int main() {
  testR_KON_42_stopkaZKlatki();
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testSlotDziennyZObrazkiem();
  testSlotBezObrazkowIBezMiasta();
  testDrzemka();
  printf("testy_slot_dzienny_dyrygent: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
