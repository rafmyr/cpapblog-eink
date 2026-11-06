// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: ekrany CO2 per egzemplarz (`co2=tak|nie` w /konfiguracja.txt, domyślnie według lokalnego źródła
// otoczenia; 5.8.8, R-KON-01), liczba wybudzeń z Wi-Fi wieczorem w egzemplarzu ze źródłem otoczenia i bez niego,
// przerywniki S2d losowane w wersji beta z tekstami G01–G18 (5.7.6, R-HUM-10, R-HUM-14, R-HUM-21, R-HUM-22).
// Pomocnicze funkcje symulacji doby skopiowane z test/testy_stany_nocy_sloty_wieczorne.cpp. Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_ekrany_co2).
#include <algorithm>
#include <set>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "kolejka.h"
#include "nawigacja.h"
#include "ekrany.h"
#include "dzien.h"
#include "sprawy.h"
#include "obrazki.h"
#include "plan.h"
#include "slot_wieczorny.h"
#include "stan_nocy.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"
#include "humor.h"
#include "kontekst_sk.h"
#include "slot_dzienny.h"
#include "otoczenie.h"

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


static bool logMa(const std::vector<Wyb>& v, const std::string& t, int odMin = 0, int doMin = 24 * 60,
                  const std::string& data = "") {
  for (auto& w : v)
    if (w.minuta >= odMin && w.minuta < doMin && (data.empty() || w.data == data))
      for (auto& z : w.zdarzenia)
        if (z.find(t) != std::string::npos) return true;
  return false;
}
// ---------- A. ekrany CO2 per egzemplarz ----------
static void testUstawienieCo2() {
  SPRAWDZ(parsujUstawieniaDnia("").co2 == -1);
  SPRAWDZ(parsujUstawieniaDnia("co2=tak\n").co2 == 1);
  SPRAWDZ(parsujUstawieniaDnia("pylki=brzoza\nco2 = nie\n").co2 == 0);
  UstawieniaDnia z = parsujUstawieniaDnia("co2=moze\n");
  SPRAWDZ(z.co2 == -1 && z.bledy.size() == 1);
  // domyślnie według źródła: egzemplarz z adresem źródła tak, bez źródła nie; `tak` bez źródła nic nie włącza
  SPRAWDZ(co2Wlaczone(true, -1) && co2Wlaczone(true, 1) && !co2Wlaczone(true, 0));
  SPRAWDZ(!co2Wlaczone(false, -1) && !co2Wlaczone(false, 1) && !co2Wlaczone(false, 0));
}

// Minuty wybudzeń z Wi-Fi od 17:00 do 23:30 (bez 23:30) w planie doby z nocą kompletną i oknem 21:00 (R-KON-17).
static std::vector<int> wieczorneZWifi(bool zrodlo, bool co2, std::vector<int>* wszystkie = nullptr) {
  std::vector<int> r;
  WejsciePlanu q;
  q.zegar = true;
  q.nocKompletna = true;
  q.sprawdzenie11 = true;
  q.zrodloOtoczenia = zrodlo;
  q.co2 = co2;
  q.wieczorOd = 21 * 60;
  q.slotDzienny = false;
  q.sekundaDoby = 17 * 3600;
  q.obsluzoneZnane = true;
  q.obsluzoneDo = q.sekundaDoby;
  for (int i = 0; i < 100; i++) {
    Termin t = nastepnyTermin(q);
    int m = (q.sekundaDoby + (int)t.sekund) / 60;
    if (m >= 23 * 60 + 30 || t.sekund == 0) break;
    if (wszystkie) wszystkie->push_back(m);
    if (sieciowy(t.powody)) r.push_back(m);
    SPRAWDZ(co2 && zrodlo ? true : (t.powody & POWOD_SLOT_WIECZORNY) == 0);
    q.sekundaDoby = m * 60;
    q.obsluzoneDo = m * 60;
  }
  return r;
}
// linia E-08 `co2_milczy` tylko przy włączonych ekranach CO2 (5.8.8)
static void testE08ZCo2() {
  Otoczenie o;
  wyczyscOtoczenie(o, true, 1);
  uint32_t t = ep(2026, 11, 4, 7, 0);
  o.stan = (int8_t)StanOto::OK;
  o.pobrano = t - 60;
  o.co2Milczy = true;
  o.co2MilczyOd = t - 3600;
  StanE08 a, b;
  SPRAWDZ(liniaDnia(false, "S2A", o, t, "2026-11-04", a, true).e08);
  SPRAWDZ(!liniaDnia(false, "S2A", o, t, "2026-11-04", b, false).e08);
}
static void testPlanWieczoru() {
  // Egzemplarz ze źródłem: pogoda 21:00 i ocena co 30 min z Wi-Fi (R-KON-01): 5 wybudzeń z Wi-Fi
  SPRAWDZ(wieczorneZWifi(true, true) == std::vector<int>({21 * 60, 21 * 60 + 30, 22 * 60, 22 * 60 + 30, 23 * 60}));
  // egzemplarz testowy (bez źródła): ani jednego wybudzenia z Wi-Fi wieczorem (budżet baterii 8.1)
  std::vector<int> wsz;
  SPRAWDZ(wieczorneZWifi(false, true, &wsz).empty());
  SPRAWDZ(wieczorneZWifi(false, false).empty());
  SPRAWDZ(!wsz.empty());  // wybudzenia pomiarowe czujnika bez Wi-Fi zostają (R-DANE-09)
  // źródło jest, ale `co2=nie`: zostaje tylko pogoda o 21:00 (R-DANE-16), bez oceny slotu z Wi-Fi
  SPRAWDZ(wieczorneZWifi(true, false) == std::vector<int>({21 * 60}));
}

static const long DOBA_C = nrDnia("2026-11-03");  // noc z poranka 4.11
static void odpCo2(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  s.siec.siecJest = true;
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_C, DOBA_C));
  s.siec.dodaj(sciezkaNocy(DOBA_C), 200, raport(DOBA_C, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40)));
  int c = t >= ep(2026, 11, 4, 21, 50) ? 1300 : 700;
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":" + std::to_string(c) + "},\"ostrzezenia\":[]}");
}
struct WynikDoby {
  int wifiWieczorem = 0;   // wybudzenia zegarem z połączeniem Wi-Fi w oknie slotu wieczornego 21:00–23:30 (R-KON-17:
                           // bez M-18 okno stałe; wcześniej trwa slot dzienny z oceną o pełnej godzinie, R-KON-42)
  int ocenZWifi = 0;       // wybudzenia z powodem `slot_wieczorny`
  bool sk01 = false;
};
static WynikDoby doba(bool zrodlo, const char* konfiguracja) {
  KonfiguracjaPanelu k = KONF;
  if (zrodlo) k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  PanelSym s;
  if (konfiguracja) s.p.pliki.pliki["/konfiguracja.txt"] = konfiguracja;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 6, 0), ep(2026, 11, 4, 23, 20), odpCo2, false, k);
  WynikDoby r;
  for (auto& w : v) {
    if (w.powod != PowodWybudzenia::ZEGAR || w.minuta < 21 * 60 || w.minuta >= 23 * 60 + 30) continue;
    r.wifiWieczorem += w.m.polaczen > 0;
    // `slot_wieczorny_bez_sieci` (ocena bez Wi-Fi u egzemplarza bez CO2) to nie ocena z Wi-Fi
    for (auto& z : w.zdarzenia) {
      size_t i = z.find("slot_wieczorny");
      r.ocenZWifi += i != std::string::npos && z.compare(i, 24, "slot_wieczorny_bez_sieci") != 0;
    }
  }
  r.sk01 = logMa(v, "ekran wieczorny SK01", 21 * 60, 24 * 60);
  return r;
}
static void testDobaEgzemplarzy() {
  WynikDoby zeZrodlem = doba(true, nullptr);
  SPRAWDZ(zeZrodlem.sk01);                 // ekrany CO2 bez trybu próbnego
  SPRAWDZ(zeZrodlem.ocenZWifi >= 4);       // 21:00 … 23:00 co 30 min (R-KON-01)
  SPRAWDZ(zeZrodlem.wifiWieczorem >= 4);
  WynikDoby jawnie = doba(true, "co2=tak\n");
  SPRAWDZ(jawnie.sk01 && jawnie.ocenZWifi >= 4);
  WynikDoby wyl = doba(true, "co2=nie\n");
  SPRAWDZ(!wyl.sk01 && wyl.ocenZWifi == 0);
  SPRAWDZ(wyl.wifiWieczorem <= 1);     // tylko pogoda o 21:00 (R-DANE-16)
  WynikDoby tester = doba(false, nullptr);
  SPRAWDZ(!tester.sk01 && tester.ocenZWifi == 0);
  ROWNE_L(tester.wifiWieczorem, 0);    // egzemplarz testowy bez wybudzeń z Wi-Fi wieczorem
  WynikDoby testerTak = doba(false, "co2=tak\n");
  SPRAWDZ(!testerTak.sk01 && testerTak.wifiWieczorem == 0);
}

// ---------- B. przerywniki S2d (5.7.6) ----------
static size_t znaki(const std::string& s) {
  size_t n = 0;
  for (unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}
static std::string male(std::string s) {
  for (auto& c : s) c = (char)tolower((unsigned char)c);
  return s;
}
static void testTekstyPrzerywnikow() {
  UstawieniaPrzerywnikow u = ustawieniaPrzerywnikowBety();
  SPRAWDZ(u.tekstyPrzyjete && u.zDatami);
  // teksty autora w dwóch kompletach (z warunkiem i neutralne), bez etykiety, bez limitów
  // 30 i 70 znaków (5.7.6); dosłowną zgodność z plikiem autora i kontrolę ADR-0002 robi test/testy_przerywniki_s2d.cpp
  for (int g = 1; g <= 18; g++) {
    char sc[8];
    snprintf(sc, sizeof sc, "G%02d", g);
    for (bool war : {true, false}) {
      TekstPrzerywnika t = tekstPrzerywnika(sc, war);
      if (t.tytul.empty()) printf("BŁĄD: brak tekstu przerywnika %s\n", sc);
      SPRAWDZ(!t.tytul.empty());
      SPRAWDZ(znaki(t.tytul) <= 90 && znaki(t.powod) <= 100);  // limity katalogu (teksty.csv)
      std::string w = male(t.tytul + " " + t.powod);
      // R-HUM-21: bez kroku;: bez „wiemy”; ADR-0029 pkt 7: bez „w porządku”, „w normie”
      SPRAWDZ(w.find("→") == std::string::npos && w.find("wiemy") == std::string::npos);
      SPRAWDZ(w.find("w porządku") == std::string::npos && w.find("normie") == std::string::npos);
      // ADR-0002: bez ustawień i wartości
      for (const char* z : {"cmh2o", "ciśnieni", "epr", "ustaw"}) SPRAWDZ(w.find(z) == std::string::npos);
    }
  }
  SPRAWDZ(tekstPrzerywnika("B01", false).tytul.empty() && tekstPrzerywnika("", true).tytul.empty());
}

static KontekstHumoru kontekstH(const std::string& dzis) {
  KontekstHumoru k;
  k.dzis = dzis;
  k.sceny = sceny();
  k.zPropozycjami = true;
  return k;
}
// 84 spokojne dni (12 tygodni) od środy 4.11.2026, jak dyrygent: przerywnik zamiast dowodu spokoju (wiersz S2D), inaczej dowód.
static void testLosowanie() {
  UstawieniaPrzerywnikow u = ustawieniaPrzerywnikowBety();
  StanDnia s;
  s.nocDzis = s.nocDzisPonad4h = true;
  s.nocyWaznych = 60;
  s.maNorme = true;
  s.seria = 12;
  s.nocyPonad4h = 50;
  s.sypialnia = true;
  std::vector<WpisHumoru> log;
  std::vector<std::string> dni;
  int okazji = 0;
  std::string poprzedniL;
  std::set<std::string> ogolneSceny, wszystkieSceny;
  for (int d = 0; d < 84; d++) {
    s.dzis = dodajDni("2026-11-04", d);
    KontekstHumoru k = kontekstH(s.dzis);
    EkranDnia e = wybierzEkranDnia(s, log);
    Przerywnik p = wybierzPrzerywnik(e, log, k, u);
    if (p.jest) {
      dni.push_back(s.dzis);
      SPRAWDZ(!tekstPrzerywnika(p.scena, p.warunkowa).tytul.empty() && !p.zdanie.empty());
      // R-HUM-10: scena nie z 6 poprzednich dni
      for (auto& x : log)
        if (x.scena == p.scena && x.tryb == 'K') SPRAWDZ(nrDnia(s.dzis) - nrDnia(x.data) >= 7);
      if (p.klucz == "L") {
        if (!poprzedniL.empty()) SPRAWDZ(nrDnia(s.dzis) - nrDnia(poprzedniL) >= 7);  // R-HUM-14
        poprzedniL = s.dzis;
        // G02 tylko w poniedziałek, G03 tylko w piątek, bez pierwszeństwa (wspólna pula z G09–G18)
        if (p.scena == "G02") SPRAWDZ(dzienTygodnia(s.dzis) == 1);
        if (p.scena == "G03") SPRAWDZ(dzienTygodnia(s.dzis) == 5);
        if (p.scena >= "G09" && d < 56) ogolneSceny.insert(p.scena);
        wszystkieSceny.insert(p.scena);
      } else {
        okazji++;
        SPRAWDZ(p.scena == okazjaDnia(s.dzis));
      }
      // ten sam dzień: ten sam przerywnik (przerysowanie)
      log.push_back({s.dzis, "S2D", p.klucz, 'K', p.scena, p.zdanie});
      Przerywnik znowu = wybierzPrzerywnik(e, log, k, u);
      SPRAWDZ(znowu.jest && znowu.scena == p.scena);
    } else {
      Wybor w = wybierzTryb(e, true, log, k);
      log.push_back({s.dzis, e.typ, e.klucz, w.tryb, w.scena, w.zdanie});
    }
  }
  SPRAWDZ(dni.size() >= 11);      // co 7 dni z 84 spokojnych (1 na 7 dni, R-HUM-14)
  SPRAWDZ(ogolneSceny.size() >= 5);  // w 8 tygodniach co najmniej 5 różnych scen z G09–G18
  SPRAWDZ(wszystkieSceny.size() >= 7);  // losowanie, w 12 tygodniach co najmniej 7 różnych scen
  {  // losowanie powtarzalne: ten sam dzień i ten sam dziennik → ta sama scena
    std::vector<WpisHumoru> l;
    Przerywnik a = wybierzPrzerywnik(EkranDnia{"S2A", "1", false, false}, l, kontekstH("2026-12-02"), u);
    Przerywnik b = wybierzPrzerywnik(EkranDnia{"S2A", "1", false, false}, l, kontekstH("2026-12-02"), u);
    SPRAWDZ(a.jest && a.scena == b.scena);
  }
  (void)okazji;                   // 25.12 w tym przebiegu przepada przez limit kart R-HUM-23 (S2b to zawsze karta)
  {  // okazja ze stałą datą poza limitem 1 na 7 dni, w limitach kart (R-HUM-15): 25.12, przerywnik 3 dni wcześniej
    std::vector<WpisHumoru> l = {{"2026-12-22", "S2D", "L", 'K', "G10", "przerywnik_sceny:9"}};
    Przerywnik o = wybierzPrzerywnik(EkranDnia{"S2A", "1", false, false}, l, kontekstH("2026-12-25"), u);
    SPRAWDZ(o.jest && o.scena == "G05" && o.klucz == "O" && o.okazja == "Boże Narodzenie");
  }
  // dzień z problemem (nie dowód spokoju) i z linią uwagi: bez przerywnika (5.7.6, R-HUM-22)
  std::vector<WpisHumoru> pusty;
  KontekstHumoru k = kontekstH("2026-11-30");
  SPRAWDZ(!wybierzPrzerywnik(EkranDnia{"S3", "E-23", false, false}, pusty, k, u).jest);
  SPRAWDZ(!wybierzPrzerywnik(EkranDnia{"SK10", "s#1", false, false}, pusty, k, u).jest);
  SPRAWDZ(wybierzPrzerywnik(EkranDnia{"S2A", "1", false, false}, pusty, k, u).jest);
  k.liniaUwagi = true;
  SPRAWDZ(!wybierzPrzerywnik(EkranDnia{"S2A", "1", false, false}, pusty, k, u).jest);
  k.liniaUwagi = false;
  k.humor = Humor::OSZCZEDNY;  // R-HUM-16
  SPRAWDZ(!wybierzPrzerywnik(EkranDnia{"S2A", "1", false, false}, pusty, k, u).jest);
}

int main() {
  (void)zadanZeSciezka;
  (void)STATUS_BLOKADA;
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testUstawienieCo2();
  testE08ZCo2();
  testPlanWieczoru();
  testDobaEgzemplarzy();
  testTekstyPrzerywnikow();
  testLosowanie();
  printf("testy_ekrany_co2: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
