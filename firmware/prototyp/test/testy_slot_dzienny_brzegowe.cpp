// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (slot dzienny, obrazki dnia, „Przewietrz sypialnię”).
// Każdy test odpowiada przypadkowi z numerem rNN w komunikacie. Dane wyłącznie syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_slot_dzienny_brzegowe).
// Pomocnicze funkcje symulacji doby skopiowane z test/testy_slot_dzienny_dyrygent.cpp (tam statyczne).
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
static const char* NR = "";
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s %s:%d: %s\n", NR, __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE_L(a, b)                                                                                          \
  do {                                                                                                         \
    long a_ = (long)(a), b_ = (long)(b);                                                                       \
    if (a_ == b_) ok++;                                                                                        \
    else { zle++; printf("BŁĄD %s %s:%d: %s = %ld, ma być %ld\n", NR, __FILE__, __LINE__, #a, a_, b_); }            \
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
  std::string s = std::to_string(cywilneMs(startEp));
  std::string k = std::to_string(cywilneMs(koniecEp));
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



static KonfiguracjaPanelu konf(bool wspolrzedne) {
  KonfiguracjaPanelu k = KONF;
  if (wspolrzedne) { k.pogodaLat = 50.2597; k.pogodaLon = 19.0217; }
  return k;
}
// Open-Meteo: dziś i jutro co godzinę; za oknem 15°, PM2.5 10, opad 0 (bez trybu wietrzenia przy łóżku 15–20°)
static std::string jsonPrognozy(const std::string& d) {
  std::string t = "\"time\":[", a = "\"temperature_2m\":[", o = "\"precipitation_probability\":[", c = "\"cloud_cover\":[",
              w = "\"wind_gusts_10m\":[";
  for (int i = 0; i < 48; i++) {
    std::string dz = i < 24 ? d : dodajDni(d, 1);
    char b[32];
    snprintf(b, sizeof b, "\"%sT%02d:00\"", dz.c_str(), i % 24);
    const char* s = i ? "," : "";
    t += s + std::string(b);
    a += s + std::string("15");
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
static bool logMa(const std::vector<Wyb>& v, const std::string& t, int odMin = 0, int doMin = 24 * 60) {
  for (auto& w : v)
    if (w.minuta >= odMin && w.minuta < doMin)
      for (auto& z : w.zdarzenia)
        if (z.find(t) != std::string::npos) return true;
  return false;
}

#include "kontekst.h"
#include "slot_dzienny.h"

// ---------- S-K-01 wyzwolony odczytem bieżącym, po powrocie z K1 powód mówi o zeszłej nocy ----------
// 21:00 CO2 1250 ppm z lokalnego źródła → S-K-01 `Powietrze w sypialni jest teraz zastałe.` (1250 w nagłówku i fakcie); 22:00 naciśnięcie ▶,
// 22:05 powrót (R-PRZ-02): odczyt z 21:00 ma już > 30 min, ocenWieczor() daje BRAK z zOdczytu = false, dyrygent
// rysuje S-K-01 ponownie z trescSK01() i powodem `Zeszłej nocy CO2 przez {h} h ponad 1000 ppm.` (fałsz: zeszła noc
// nie spełniła warunku 2 h; przy nocy bez odczytów `0 h`). Oczekiwane: ten sam powód co o 21:00.
static void odpR01(PanelSym& s, uint32_t t) {
  odpowiedziB9(s, t);
  s.siec.odpowiedzi.erase("/otoczenie");
  int c = t >= ep(2026, 11, 4, 21, 10) ? 900 : (t >= ep(2026, 11, 4, 20, 0) ? 1250 : 800);
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":" + std::to_string(c) + "},\"ostrzezenia\":[]}");
}
static bool rzedyRowne(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int y0, int y1) {
  for (int y = y0; y < y1; y++)
    for (int x = 0; x < 100; x++)
      if (a[(size_t)(y * 100 + x)] != b[(size_t)(y * 100 + x)]) return false;
  return true;
}
static void powodSK01PoPowrocie() {
  NR = "r01";
  KonfiguracjaPanelu k = konf(true);
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  PanelSym q;
  std::vector<Wyb> u = symuluj(q, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 21, 50), odpR01, false, k);
  SPRAWDZ(logMa(u, "ekran wieczorny SK01", 21 * 60, 21 * 60 + 5));
  SPRAWDZ(!q.ekran.klatki.empty());
  if (q.ekran.klatki.empty()) return;
  std::vector<uint8_t> o21 = q.ekran.klatki.back();
  size_t przed = q.ekran.klatki.size();
  uint32_t t = ep(2026, 11, 4, 22, 0);
  odpR01(q, t);
  q.p.start.budzace = BIT_PRAWY;
  q.p.przyciski.skrypt = {{0, BIT_PRAWY}, {300, 0}};
  wybudz(q, t, PowodWybudzenia::PRZYCISK, k);
  q.p.start.budzace = 0;
  q.p.przyciski.skrypt.clear();
  // od slotu wieczornego (R-KON-01) plan budzi panel także o 22:00 na ocenę (K1 zostaje), więc do powrotu z K1
  // (~22:05) może być kilka wybudzeń zegarem
  for (int i = 0; i < 4; i++) {
    uint32_t t2 = q.p.zegar.teraz() + (q.sen.plan.budzZegarem ? q.sen.plan.sekund : OKRES_SNU_S);
    odpR01(q, t2);
    wybudz(q, t2, PowodWybudzenia::ZEGAR, k);  // powrót z K1 (~22:05)
    if (t2 >= t + 5 * 60) break;
  }
  SPRAWDZ(q.ekran.klatki.size() >= przed + 2);
  if (q.ekran.klatki.size() < przed + 2) return;
  // pas tytułu i powodu (y 90–190 karty K05): ten sam tekst co o 21:00
  SPRAWDZ(rzedyRowne(o21, q.ekran.klatki.back(), 90, 190));
  // ta sama ścieżka na poziomie logiki: odczyt nieświeży, noc bez warunku → powód nie może mówić o zeszłej nocy
  Co2Nocy n = statystykaCo2({{1000000, 1250}, {1003600, 1250}});  // 1 h ponad próg
  WejscieWieczoru w;
  w.dzis = "2026-11-04";
  w.noc = &n;
  w.co2Teraz = -1;
  WynikWieczoru wy = ocenWieczor(w);
  SPRAWDZ(wy.ekran == EkranWieczoru::BRAK);
  TrescKontekstu tk = trescSK01(w, wy);  // tak rysuje dyrygent.cpp rysujKontekstB9() przy wpisie SK01 z dziś
  SPRAWDZ(tk.t.powod.rfind("Zeszłej nocy", 0) != 0);
}

// ---------- S-K-33 `Dziś bez wietrzenia.` / `Smog za oknem…` bez danych PM2.5 ----------
// Odpowiedź /v1/air-quality nie przyszła (dyrygent zapisuje prognozę bez PM2.5) albo ma same `null`: każda godzina
// jest „nieczysta”, tryb zimowy nie znajduje okna i reguła wybiera wariant SMOG z `PM2.5 –`. Panel twierdzi „smog”
// bez pomiaru. Oczekiwane: bez PM2.5 w horyzoncie brak wariantu SMOG (S-K-33 milczy albo inny wariant bez smogu).
static void smogBezPm25() {
  NR = "r02";
  Prognoza p;
  p.data = "2026-11-04";
  p.pobrano = 1;
  p.godzinaPobrania = "08:00";
  for (auto& g : p.h) { g.temp = 5; g.opad = 0; g.chmury = 20; g.porywy = 20; }  // pm25 = NAN
  WejscieSlotu we;
  we.dzis = "2026-11-04";
  we.minuta = 10 * 60;
  we.prognoza = &p;
  we.przyLozku = 22;
  WyborSlotu w = wybierzSlot(we);
  SPRAWDZ(w.okno.wariant != Wietrzenie::SMOG);
  TekstySlotu t = tekstySlotu(w, we);
  SPRAWDZ(t.powod.find("Smog") == std::string::npos && t.powod.find("smog") == std::string::npos);
}

// ---------- zostajeRano() zna tylko S5 i S10 ----------
// 7.2 D0: ekran z rana zostaje także przy ekranie L (R2, R3), S-K-12 (R4) i problemie porannym (R5a–R5d). Komentarz
// w slot_dzienny.cpp mówi „typy spoza prototypu też zostają”, kod zwraca false dla każdego typu poza S5 i S10, więc
// przy ekranach typów E i L slot dzienny zasłoni je o starcie slotu. Oczekiwane: lista typów,
// które USTĘPUJĄ (S2A, S2B, S2C, S2D, S6, S7, KM, S8, odkrycia), a reszta zostaje.
static void ekranyZostajaRano() {
  NR = "r03";
  SPRAWDZ(zostajeRano("L06", false));
  SPRAWDZ(zostajeRano("E21", false));
  SPRAWDZ(zostajeRano("SK12", false));
  SPRAWDZ(zostajeRano("SK10", false));
  SPRAWDZ(!zostajeRano("S2A", false));
  SPRAWDZ(!zostajeRano("KM", false));
}

// ---------- R-KON-49, błąd wschodu i zachodu ≤ 2 min ----------
// Wzorzec: równania NOAA (arkusz NOAA Solar Calculator, refrakcja 0,833°), Katowice 50,2597 N 19,0217 E, czas
// Europe/Warsaw: 30.08.2026 zachód 19:32,8, 25.08.2026 zachód 19:43,3, 01.10.2026 zachód 18:22,5. Panel liczy
// uproszczonym równaniem wschodu słońca i daje 30.08 zachód 19:35 (30.08 różnica 2,2 min; wzorzec w testy_slot_dzienny.cpp
// to sunrise-sunset.org z minutami obciętymi i tolerancją 2, więc tego nie łapie).
static void slonceDwieMinuty() {
  NR = "r05";
  struct { const char* d; double z; } wz[] = {{"2026-08-30", 19 * 60 + 32.8}, {"2026-08-25", 19 * 60 + 43.3},
                                              {"2026-10-01", 18 * 60 + 22.5}};
  for (auto& e : wz) {
    int w = -1, z = -1;
    SPRAWDZ(slonce(e.d, 50.2597, 19.0217, w, z));
    if (std::fabs(z - e.z) > 2.0) printf("  r05 %s: zachód %02d:%02d, NOAA %.1f min doby\n", e.d, z / 60, z % 60, e.z);
    SPRAWDZ(std::fabs(z - e.z) <= 2.0);
  }
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  powodSK01PoPowrocie();
  smogBezPm25();
  ekranyZostajaRano();
  slonceDwieMinuty();
  printf("testy_slot_dzienny_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
