// SPDX-License-Identifier: GPL-3.0-or-later
// Odczyt lokalnego źródła otoczenia (ADR-0019) po czekaniu na kablu (ADR-0047), T-TEST-519.
// Usterka z panelu 22.11.2026 18:00 (panel na kablu, przebieg ok. 27 min po starcie programu): w logu
// `otoczenie: HTTP -400 w 100 ms, 0 B`. Rdzeń liczy termin końca odczytu ciała na Zegar::ms(), który od ADR-0047
// liczy od końca czekania, a adapter porównywał ten termin z własnym millis() od startu programu: po każdym czekaniu
// pierwszy bajt ciała był „po terminie”. Atrapa zegara ma tu dwie podstawy czasu jak panel: surową (millis(), od
// startu programu, ciągłą przez czekanie) i rdzenia (ms(), od początku przebiegu); atrapa sieci ocenia odczyt ciała
// na surowej, jak adapter.
//  A. przebieg po czekaniu na kablu, źródło odpowiada 200 w 50 ms: kod 200, nowe dane otoczenia (stan OK, nowe CO2,
//     chwila pobrania z tego przebiegu), bez -400 w logu;
//  B. ten sam przebieg, źródło odpowiada dłużej niż cały budżet otoczenia: dalej -400 (budżet działa po czekaniu);
//  C. pierwszy przebieg programu (jedna podstawa czasu) bez zmian: 200 i dane.
// Uruchomienie: tools/prototyp/testy.sh (program testy_otoczenie_zegar_kabel).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "otoczenie.h"
#include "przebiegi.h"
#include "start.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)

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

static const char* STATUS_BLOKADA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static std::string oto(int co2) {
  return "{\"wersja\":1,\"sypialnia\":{\"co2\":" + std::to_string(co2) + "},\"ostrzezenia\":[]}";
}
static KonfiguracjaPanelu konf() {
  KonfiguracjaPanelu k = {"192.168.1.20", "cpap", "haslo", "", "", true};
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  k.otoToken = "t";
  k.bezTekstowKoncowych = true;  // jak src/main.cpp
  return k;
}

// Wiersze logu od `od` zawierające `f`.
static int wLogu(const Panel& p, size_t od, const std::string& f) {
  int n = 0;
  for (size_t i = od; i < p.port.log.size(); i++) n += p.port.log[i].find(f) != std::string::npos;
  return n;
}
static void drukujOtoczenie(const Panel& p, size_t od) {
  for (size_t i = od; i < p.port.log.size(); i++)
    if (p.port.log[i].find("otoczenie") != std::string::npos || p.port.log[i].find("czekanie") != std::string::npos)
      printf("  log: %s\n", p.port.log[i].c_str());
}

// Panel na kablu z czekaniem jak src/main.cpp: jeden program, kolejne przebiegi nowym Dyrygentem z tą samą pamięcią
// przebiegów. Czekanie jak SenEsp32::czekaj(): surowy zegar biegnie przez cały czas czekania (plan.sekund), a na
// końcu ms() rdzenia liczy od nowa (czekanieEsp32::poczatekMs = millis()).
struct Kabel {
  Panel p;
  PamiecPrzebiegow pp;
  KonfiguracjaPanelu k = konf();
  uint32_t czekanoMs = 0;  // surowy czas ostatniego czekania
  Kabel() {
    p.zegar.epoch = ep(2026, 11, 22, 7, 0);
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.czujniki.zas = Zasilanie::ZEWNETRZNE;
    p.sen.czekanieWlaczone = true;
    p.ekran.pamietaObraz = true;
    p.sen.wCzekaniu = [this]() {
      czekanoMs = p.sen.plan.sekund * 1000u;
      p.zegar.msTeraz += czekanoMs;
      p.zegar.poczatekPrzebiegu = p.zegar.msTeraz;
    };
  }
  // Jeden przebieg; true = skończył się czekaniem (następny przebieg w tym samym programie).
  bool przebieg() {
    p.siec.polaczonaTeraz = false;
    bool czekal = Dyrygent(p.adaptery(), k, &pp).wybudzenie();
    // następny przebieg jak po wybudzeniu z głębokiego snu zegarem (UruchomienieEsp32 po czekaniu)
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    p.start.budzace = 0;
    return czekal;
  }
};

// Przebiegi po pierwszym, aż któryś przeczyta źródło otoczenia (wybudzenie z samego rytmu zasilania go nie czyta,
// R-DANE-63); zwraca indeks logu początku tego przebiegu albo SIZE_MAX, gdy żaden w limicie.
static size_t doOdczytuPoCzekaniu(Kabel& c, int maks = 16) {
  for (int i = 0; i < maks; i++) {
    size_t od = c.p.port.log.size();
    bool czekal = c.przebieg();
    if (wLogu(c.p, od, "otoczenie: HTTP")) return od;
    if (!czekal) return (size_t)-1;
  }
  return (size_t)-1;
}

// A + C: odpowiedź 200 w 50 ms po czekaniu daje nowe dane.
static void testOdczytPoCzekaniu() {
  Kabel c;
  c.p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  c.p.siec.dodaj("/otoczenie", 200, oto(700), 50);
  c.p.siec.dodaj("/otoczenie", 200, oto(1234), 50);
  // C: pierwszy przebieg programu, jedna podstawa czasu
  SPRAWDZ(c.przebieg());
  SPRAWDZ(wLogu(c.p, 0, "otoczenie: HTTP 200") == 1);
  SPRAWDZ(c.p.start.stan.oto.stan == (int8_t)StanOto::OK && c.p.start.stan.oto.co2 == 700);
  uint32_t pobrano1 = c.p.start.stan.oto.pobrano;
  // A: przebieg po czekaniu (na panelu 22.11.2026: ok. 27 min po starcie programu)
  size_t od = doOdczytuPoCzekaniu(c);
  SPRAWDZ(od != (size_t)-1);
  if (od == (size_t)-1) return;
  SPRAWDZ(c.p.zegar.poczatekPrzebiegu > OTO_BUDZET_MS);  // dwie podstawy czasu naprawdę się rozjechały
  SPRAWDZ(c.p.zegar.surowe() - c.p.zegar.ms() == c.p.zegar.poczatekPrzebiegu);
  SPRAWDZ(wLogu(c.p, od, "otoczenie: HTTP 200") == 1);
  SPRAWDZ(wLogu(c.p, od, "otoczenie: HTTP -400") == 0);
  SPRAWDZ(wLogu(c.p, od, "otoczenie: nieudana próba") == 0);
  SPRAWDZ(c.p.start.stan.oto.stan == (int8_t)StanOto::OK);
  SPRAWDZ(c.p.start.stan.oto.co2 == 1234);  // nowe dane (stopka, ekrany CO2)
  SPRAWDZ(c.p.start.stan.oto.pobrano > pobrano1);
  if (zle) drukujOtoczenie(c.p, od);
}

// B: po czekaniu budżet otoczenia dalej obowiązuje: odpowiedź dłuższa niż cały budżet = -400, zostają stare dane.
static void testBudzetPoCzekaniu() {
  Kabel c;
  c.p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  c.p.siec.dodaj("/otoczenie", 200, oto(700), 50);
  c.p.siec.dodaj("/otoczenie", 200, oto(1234), OTO_BUDZET_MS + 1000);
  SPRAWDZ(c.przebieg());
  size_t od = doOdczytuPoCzekaniu(c);
  SPRAWDZ(od != (size_t)-1);
  if (od == (size_t)-1) return;
  SPRAWDZ(wLogu(c.p, od, "otoczenie: HTTP -400") >= 1);
  SPRAWDZ(wLogu(c.p, od, "otoczenie: HTTP 200") == 0);
  SPRAWDZ(wLogu(c.p, od, "otoczenie: nieudana próba (kod -400") == 1);
  SPRAWDZ(c.p.start.stan.oto.co2 == 700);  // zostają poprzednie dane
}

// B': w pierwszym przebiegu (jedna podstawa czasu) budżet też działa jak dotąd.
static void testBudzetPierwszyPrzebieg() {
  Kabel c;
  c.p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  c.p.siec.dodaj("/otoczenie", 200, oto(700), OTO_BUDZET_MS + 1000);
  c.przebieg();
  SPRAWDZ(wLogu(c.p, 0, "otoczenie: HTTP -400") >= 1);
  SPRAWDZ(c.p.start.stan.oto.stan != (int8_t)StanOto::OK);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testOdczytPoCzekaniu();
  testBudzetPoCzekaniu();
  testBudzetPierwszyPrzebieg();
  printf("testy_otoczenie_zegar_kabel: %d OK, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
