// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe (T-TEST-519, ADR-0047 zmiana 2026-11-22): źródło otoczenia po DŁUGIM czekaniu na kablu,
// gdy surowy zegar procesora (millis() panelu, uint32) jest tuż przed przepełnieniem i tuż za nim (ok. 49,7 doby
// pracy na kablu bez restartu, realne od ADR-0047).
//  R1. budzetCialaMinal() na granicy przepełnienia (jednostkowo);
//  R2. przemiatanie chwili przepełnienia względem przebiegu: odpowiedź 200 w 50 ms po czekaniu daje 200 i nowe dane,
//      bez -400, a AirBridge /api/status w tym samym przebiegu odpowiada 200;
//  R3. odpowiedź wolniejsza niż cały budżet po przepełnieniu nadal daje -400;
// Atrapa zegara ma czas unix liczony z 32-bitowego msTeraz, więc przy przepełnieniu cofnęłaby datę o 49,7 doby;
// tu opakowanie ZegarCiagly liczy czas unix z licznika 64-bitowego (jak time() panelu), a ms() i surowy zegar
// zostają 32-bitowe z AtrapaZegara.
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
  t.tm_year = r - 1900; t.tm_mon = mi - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = m; t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}

class ZegarCiagly : public Zegar {
 public:
  explicit ZegarCiagly(AtrapaZegara& z) : z_(z), ost_(z.msTeraz) {}
  uint32_t epoch0 = 0;  // czas unix przy surowym64 == 0
  uint64_t surowe64() {
    if (z_.msTeraz < ost_) obiegi_++;
    ost_ = z_.msTeraz;
    return (obiegi_ << 32) | z_.msTeraz;
  }
  uint32_t teraz() override { return epoch0 + (uint32_t)(surowe64() / 1000); }
  uint32_t ms() override { surowe64(); return z_.ms(); }
  void czekaj(uint32_t m) override { surowe64(); z_.czekaj(m); surowe64(); }
  bool lokalny(uint32_t t, std::tm& w) override { return z_.lokalny(t, w); }
  uint32_t zLokalnego(const std::tm& l) override { return z_.zLokalnego(l); }
  bool ustaw(uint32_t t) override { epoch0 = t - (uint32_t)(surowe64() / 1000); return true; }
 private:
  AtrapaZegara& z_;
  uint32_t ost_;
  uint64_t obiegi_ = 0;
};

static const char* STATUS_BLOKADA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static std::string oto(int co2) {
  return "{\"wersja\":1,\"sypialnia\":{\"co2\":" + std::to_string(co2) + "},\"ostrzezenia\":[]}";
}
static int wLogu(const Panel& p, size_t od, const std::string& f) {
  int n = 0;
  for (size_t i = od; i < p.port.log.size(); i++) n += p.port.log[i].find(f) != std::string::npos;
  return n;
}
static void drukuj(const Panel& p, size_t od) {
  for (size_t i = od; i < p.port.log.size(); i++)
    if (p.port.log[i].find("otoczenie") != std::string::npos || p.port.log[i].find("AirBridge /api") != std::string::npos)
      printf("  log: %s\n", p.port.log[i].c_str());
}

// Panel na kablu jak w T-TEST-519, z surowym zegarem startującym `doPrzepelnienia` ms przed przepełnieniem uint32.
struct KabelDlugo {
  Panel p;
  ZegarCiagly zc{p.zegar};
  PamiecPrzebiegow pp;
  KonfiguracjaPanelu k;
  bool przepelnienieWPrzebiegu = false;
  uint32_t poczatekOstatniegoSurowy = 0, koniecPrzebieguSurowy = 0;  // surowy zegar przepełnił się w trakcie ostatniego przebiegu
  explicit KabelDlugo(uint32_t doPrzepelnienia) : k{"192.168.1.20", "cpap", "haslo", "", "", true} {
    k.otoUrl = "http://192.168.1.60:8080/otoczenie";
    k.otoToken = "t";
    k.bezTekstowKoncowych = true;
    p.zegar.msTeraz = (uint32_t)(0u - doPrzepelnienia);
    p.zegar.poczatekPrzebiegu = p.zegar.msTeraz;  // pierwszy przebieg „programu” (po długiej pracy) od 0
    zc.surowe64();
    zc.ustaw(ep(2026, 11, 22, 7, 0));
    p.zegar.epoch = 1;  // nieużywane przez rdzeń (rdzeń widzi zc); != 0 dla ewentualnych odwołań atrap
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.czujniki.zas = Zasilanie::ZEWNETRZNE;
    p.sen.czekanieWlaczone = true;
    p.ekran.pamietaObraz = true;
    p.sen.wCzekaniu = [this]() {
      if (p.zegar.msTeraz < p.zegar.poczatekPrzebiegu) przepelnienieWPrzebiegu = true;
      koniecPrzebieguSurowy = p.zegar.msTeraz;
      zc.surowe64();
      p.zegar.msTeraz += p.sen.plan.sekund * 1000u;
      zc.surowe64();
      p.zegar.poczatekPrzebiegu = p.zegar.msTeraz;
    };
  }
  Adaptery ad() {
    return Adaptery{zc, p.nvs, p.pliki, p.siec, p.ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik,
                    p.sen, p.port, p.diag, &p.wydania};
  }
  bool przebieg() {
    przepelnienieWPrzebiegu = false;
    poczatekOstatniegoSurowy = p.zegar.msTeraz;
    p.siec.polaczonaTeraz = false;
    bool czekal = Dyrygent(ad(), k, &pp).wybudzenie();
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    p.start.budzace = 0;
    return czekal;
  }
  size_t doOdczytuPoCzekaniu(int maks = 16) {
    for (int i = 0; i < maks; i++) {
      size_t od = p.port.log.size();
      bool czekal = przebieg();
      if (wLogu(p, od, "otoczenie: HTTP")) return od;
      if (!czekal) return (size_t)-1;
    }
    return (size_t)-1;
  }
};

static void testR1() {
  SPRAWDZ(!budzetCialaMinal(0, 0xFFFFFF00u, 0x00001000u));          // 0 = bez limitu
  SPRAWDZ(!budzetCialaMinal(6000, 0xFFFFFF00u, 0x00000100u));       // 512 ms przez przepełnienie
  SPRAWDZ(budzetCialaMinal(500, 0xFFFFFF00u, 0x00000100u));
  SPRAWDZ(budzetCialaMinal(6000, 0xFFFFF000u, 0x00000F00u));        // dokładnie 7936 ms
  SPRAWDZ(budzetCialaMinal(1, 0xFFFFFFFFu, 0x00000000u));           // 1 ms przez granicę
  SPRAWDZ(!budzetCialaMinal(1, 0xFFFFFFFFu, 0xFFFFFFFFu));
}

// R2: wiele położeń przepełnienia względem przebiegów (w 1. przebiegu, w czekaniu, w 2. przebiegu przed i w trakcie GET)
static int przypadkowR2 = 0, zlychR2 = 0, przepelnionychWPrzebiegu = 0;
static void jedenR2(uint32_t doPrzepelnienia) {
  KabelDlugo c(doPrzepelnienia);
  c.p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  c.p.siec.dodaj("/otoczenie", 200, oto(700), 50);
  c.p.siec.dodaj("/otoczenie", 200, oto(1234), 50);
  int przed = zle;
  SPRAWDZ(c.przebieg());
  SPRAWDZ(wLogu(c.p, 0, "otoczenie: HTTP 200") == 1);
  uint32_t surowePrzed = c.p.zegar.msTeraz;
  size_t od = c.doOdczytuPoCzekaniu();
  SPRAWDZ(od != (size_t)-1);
  if (od != (size_t)-1) {
    SPRAWDZ(wLogu(c.p, od, "otoczenie: HTTP 200") == 1);
    SPRAWDZ(wLogu(c.p, od, "otoczenie: HTTP -400") == 0);
    SPRAWDZ(c.p.start.stan.oto.stan == (int8_t)StanOto::OK && c.p.start.stan.oto.co2 == 1234);
    SPRAWDZ(wLogu(c.p, od, "AirBridge /api/status: HTTP 200") >= 1);
    if (c.przepelnienieWPrzebiegu) przepelnionychWPrzebiegu++;
  }
  (void)surowePrzed;
  przypadkowR2++;
  if (zle != przed) {
    zlychR2++;
    printf("  R2 doPrzepelnienia=%u ms: błąd\n", doPrzepelnienia);
    if (od != (size_t)-1) drukuj(c.p, od);
  }
}

static void testR3() {
  KabelDlugo c(30u * 60u * 1000u);  // przepełnienie w czekaniu
  c.p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  c.p.siec.dodaj("/otoczenie", 200, oto(700), 50);
  c.p.siec.dodaj("/otoczenie", 200, oto(1234), OTO_BUDZET_MS + 1000);
  SPRAWDZ(c.przebieg());
  size_t od = c.doOdczytuPoCzekaniu();
  SPRAWDZ(od != (size_t)-1);
  if (od == (size_t)-1) return;
  SPRAWDZ(c.p.zegar.msTeraz < 0x80000000u);  // naprawdę za przepełnieniem
  SPRAWDZ(wLogu(c.p, od, "otoczenie: HTTP -400") >= 1);
  SPRAWDZ(wLogu(c.p, od, "otoczenie: HTTP 200") == 0);
  SPRAWDZ(c.p.start.stan.oto.co2 == 700);
  if (zle) drukuj(c.p, od);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testR1();
  for (uint32_t d = 500; d <= 40000; d += 250) jedenR2(d);
  {
    // sonda: ile surowego czasu od startu do początku przebiegu z odczytem (S) i ile trwa ten przebieg (T2)
    const uint32_t start = 0x10000000u;
    KabelDlugo c(0u - start);
    c.p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
    c.p.siec.dodaj("/otoczenie", 200, oto(700), 50);
    c.przebieg();
    c.doOdczytuPoCzekaniu();
    uint32_t S = c.poczatekOstatniegoSurowy - start, T2 = c.koniecPrzebieguSurowy - c.poczatekOstatniegoSurowy;
    printf("sonda: S=%u ms, T2=%u ms\n", S, T2);
    for (uint32_t x = 0; x <= T2 + 200; x += 10) jedenR2(S + x);  // przepełnienie x ms po starcie przebiegu z odczytem
  }
  const uint32_t duze[] = {60000u, 5u * 60000u, 15u * 60000u, 30u * 60000u, 60u * 60000u, 3u * 3600000u};
  for (uint32_t d : duze) jedenR2(d);
  printf("R2: %d położeń przepełnienia, %d złych, %d z przepełnieniem w przebiegu z odczytem\n", przypadkowR2, zlychR2,
         przepelnionychWPrzebiegu);
  testR3();
  printf("testy_otoczenie_zegar_kabel_brzegowe: %d OK, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
