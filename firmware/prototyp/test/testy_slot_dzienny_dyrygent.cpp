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
#include <map>
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
    // co godzinę inaczej, 18–21°: poniżej progu trybu letniego (24°), więc bez okna wietrzenia (od 16.11.2026 bez bloku
    // obrazka okno zasłaniałoby wygaszacz, R-KON-75)
    if (zmiennaTemp) s.p.czujniki.o.temp = 18.0f + (float)((minutaLok(t) / 60) % 4);
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

// Doba 4.11.2026: noc o 07:00, slot od 08:00, prognoza pobrana raz przy starcie slotu. Od 16.11.2026 bez bloku
// obrazka od startu slotu (R-KON-50 wycofane); obrazek S-K-38 jako wygaszacz po 3 h bez naciśnięcia (R-KON-73):
// 08:00…10:00 S-K-37 (D5), od 11:00 S-K-38 do końca slotu dziennego (21:00).
// T-TEST-461
// wymaganie: R-KON-73
static void testSlotDziennyZObrazkiem() {
  PanelSym s;
  s.p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
  s.p.pliki.pliki["/obrazki/002_syntetyczny.pbm"] = pbm(300, 200);
  s.p.pliki.pliki["/obrazki/obrazki.json"] =
      "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\",\"dni\":[\"sr\"]},{\"plik\":\"002_syntetyczny.pbm\"}]}";
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 22, 30), odpowiedziB9, false, konf(true));
  ROWNE_L(ileZadan(v, "/v1/forecast"), 1);      // R-KON-52: przy starcie slotu, prognoza świeża przez resztę dnia
  ROWNE_L(ileZadan(v, "/v1/air-quality"), 1);
  SPRAWDZ(logMa(v, "prognoza godzinowa: pobrana", 8 * 60, 8 * 60 + 1));
  SPRAWDZ(s.p.pliki.pliki.count("/dane/prognoza.csv") == 1);
  // od startu slotu zwykła kolejność (S-K-37), wygaszacz dopiero po 3 h bez naciśnięcia, przy ocenie o 11:00
  SPRAWDZ(logMa(v, "slot dzienny: S-K-37", 8 * 60, 8 * 60 + 1));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-37", 10 * 60, 10 * 60 + 1));
  SPRAWDZ(!logMa(v, "slot dzienny: S-K-38", 0, 11 * 60));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-38 /obrazki/001_syntetyczny.pbm (pula k)", 11 * 60, 11 * 60 + 1));  // środa: `sr`
  SPRAWDZ(logMa(v, "slot dzienny: S-K-38", 20 * 60, 20 * 60 + 1));
  SPRAWDZ(!logMa(v, "slot dzienny: S-K-37", 11 * 60, 24 * 60));
  // koniec slotu dziennego: wieczorem bez wygaszacza (slot wieczorny, R-KON-41)
  SPRAWDZ(!logMa(v, "slot dzienny: S-K-38", 21 * 60, 24 * 60));
  // R-KON-61: jeden wpis w dzienniku na dzień, mimo ocen o każdej pełnej godzinie
  std::string dz = s.p.pliki.pliki["/dane/obrazki.csv"];
  size_t pierwszy = dz.find("2026-11-04;001_syntetyczny.pbm;k");
  SPRAWDZ(pierwszy != std::string::npos && dz.find("2026-11-04;", pierwszy + 1) == std::string::npos);
  // R-KON-42: oceny bez zmiany klatki nie odświeżają e-ink (wygaszacz o 12:00 i 13:00 bez odświeżenia)
  int odswiezen = 0;
  for (auto& w : v)
    if (w.minuta > 11 * 60 && w.minuta < 14 * 60) odswiezen += w.m.odswiezen;
  ROWNE_L(odswiezen, 0);
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

// ---------- wygaszacz w przebiegu doby z naciśnięciami (R-KON-73…R-KON-77) ----------
struct Nac {
  uint32_t t;
  uint8_t bit;
};
struct DobaWyg {
  std::vector<Wyb> v;
  std::vector<std::pair<uint32_t, uint8_t>> ekranPoPrzycisku;  // chwila naciśnięcia → ekran po nim (Ekran)
};
// Wybudzenia jak symuluj(), a naciśnięcie z `nac` wypada zamiast wybudzenia zegarem, gdy jest wcześniej.
static DobaWyg dobaZNacisnieciami(PanelSym& s, uint32_t od, uint32_t doT, std::vector<Nac> nac,
                                  const std::string& plikKonf = "", bool obrazki = true) {
  if (obrazki) {
    s.p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
    s.p.pliki.pliki["/obrazki/obrazki.json"] = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\"}]}";
  }
  if (!plikKonf.empty()) s.p.pliki.pliki["/konfiguracja.txt"] = plikKonf;
  DobaWyg d;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  size_t n = 0;
  const KonfiguracjaPanelu k = konf(true);
  for (int i = 0; i < 800 && t < doT; i++) {
    uint8_t bit = 0;
    if (pw != PowodWybudzenia::START && n < nac.size() && nac[n].t <= t) {
      t = nac[n].t;
      bit = nac[n].bit;
      n++;
      pw = PowodWybudzenia::PRZYCISK;
      s.p.start.budzace = bit;
      s.p.przyciski.skrypt = {{0, bit}, {300, 0}};
    }
    odpowiedziB9(s, t);
    Wyb w = wybudz(s, t, pw, k);
    s.p.start.budzace = 0;
    s.p.przyciski.skrypt.clear();
    if (bit) d.ekranPoPrzycisku.push_back({t, s.p.start.stan.ekran});
    d.v.push_back(w);
    if (!w.spal) break;
    uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
    if (snu == 0) snu = 1;
    t += w.m.msPrzySnie / 1000 + snu;
    pw = PowodWybudzenia::ZEGAR;
  }
  return d;
}
static bool ekranO(const std::vector<Wyb>& v, const char* id, int godz) {
  return logMa(v, std::string("slot dzienny: ") + id, godz * 60, godz * 60 + 1);
}

// Naciśnięcie zeruje licznik bezczynności; ↻ na wygaszaczu: ekran dnia; ▶ na wygaszaczu: kokpit (R-KON-76)
// T-TEST-461
// wymaganie: R-KON-76
static void testR_KON_76_przyciskNaWygaszaczu() {
  PanelSym s;
  DobaWyg d = dobaZNacisnieciami(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 20, 30),
                                 {{ep(2026, 11, 4, 11, 20), BIT_SRODEK}, {ep(2026, 11, 4, 15, 30), BIT_PRAWY}});
  SPRAWDZ(ekranO(d.v, "S-K-38", 11));
  // ↻ o 11:20: od razu ekran dnia (S-K-37), wygaszacz znów dopiero po 3 h, przy ocenie o 15:00 (14:20 + do pełnej godziny)
  SPRAWDZ(logMa(d.v, "slot dzienny: S-K-37", 11 * 60 + 20, 11 * 60 + 21));
  SPRAWDZ(d.ekranPoPrzycisku.size() == 2 && d.ekranPoPrzycisku[0].second == (uint8_t)Ekran::DZIEN);
  SPRAWDZ(ekranO(d.v, "S-K-37", 12) && ekranO(d.v, "S-K-37", 14));
  SPRAWDZ(!logMa(d.v, "slot dzienny: S-K-38", 11 * 60 + 20, 15 * 60));
  SPRAWDZ(ekranO(d.v, "S-K-38", 15));
  // ▶ o 15:30 na wygaszaczu: pierwsza strona kokpitu (K1), bez kroku „zdejmij wygaszacz”
  SPRAWDZ(d.ekranPoPrzycisku.size() == 2 && d.ekranPoPrzycisku[1].second == (uint8_t)Ekran::K1);
  // po powrocie do ekranu dnia (R-PRZ-02) licznik od 15:30: S-K-37 o 16:00…18:00, wygaszacz o 19:00
  SPRAWDZ(ekranO(d.v, "S-K-37", 16) && ekranO(d.v, "S-K-37", 18));
  SPRAWDZ(!logMa(d.v, "slot dzienny: S-K-38", 15 * 60 + 30, 19 * 60));
  SPRAWDZ(ekranO(d.v, "S-K-38", 19));
}

// Wygaszacz × odświeżenie częściowe (R-NF-25, 16.11.2026): każde
// odświeżenie z inicjatywy panelu (wybudzenie zegarem i start), w tym wejście wygaszacza o pełnej godzinie, jest pełne;
// ▶ budzący na wygaszaczu → K1 według reguły R-NF-25 bez jej zmiany (po głębokim śnie obraz nieznany: pełne), w czuwaniu
// ▶ → K2 częściowe, ↻ → ekran dnia pełne; sam ↻ budzący na wygaszaczu → ekran dnia (S-K-37) pełne.
// T-TEST-461
// wymaganie: R-NF-25
static void testR_NF_25_wygaszaczOdswiezenie() {
  struct Wynik {
    int odswiezen = 0, niepelnych = 0;
    bool wyg11Pelne = false;
  };
  auto doba = [](PanelSym& s, uint32_t doT, Wynik& w) {
    s.p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
    s.p.pliki.pliki["/obrazki/obrazki.json"] = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\"}]}";
    zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
    uint32_t t = ep(2026, 11, 4, 0, 5);
    PowodWybudzenia pw = PowodWybudzenia::START;
    const KonfiguracjaPanelu k = konf(true);
    for (int i = 0; i < 800 && t < doT; i++) {
      odpowiedziB9(s, t);
      const size_t przed = s.ekran.rodzaje.size();
      Wyb x = wybudz(s, t, pw, k);
      for (size_t j = przed; j < s.ekran.rodzaje.size(); j++) {
        w.odswiezen++;
        if (s.ekran.rodzaje[j] != Odswiezenie::PELNE) w.niepelnych++;
      }
      if (logMa({x}, "slot dzienny: S-K-38", 11 * 60, 11 * 60 + 1) && s.ekran.rodzaje.size() == przed + 1 &&
          s.ekran.rodzaje.back() == Odswiezenie::PELNE)
        w.wyg11Pelne = true;
      if (!x.spal) break;
      uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
      if (snu == 0) snu = 1;
      t += x.m.msPrzySnie / 1000 + snu;
      pw = PowodWybudzenia::ZEGAR;
    }
  };
  auto przyciskiem = [](PanelSym& s, uint32_t t, std::vector<std::pair<uint32_t, uint8_t>> skrypt) {
    odpowiedziB9(s, t);
    s.p.start.budzace = skrypt.front().second;
    s.p.przyciski.skrypt = std::move(skrypt);
    const size_t przed = s.ekran.rodzaje.size();
    Wyb x = wybudz(s, t, PowodWybudzenia::PRZYCISK, konf(true));
    s.p.start.budzace = 0;
    s.p.przyciski.skrypt.clear();
    std::string r;
    for (size_t j = przed; j < s.ekran.rodzaje.size(); j++) r += s.ekran.rodzaje[j] == Odswiezenie::PELNE ? 'P' : 'C';
    return std::make_pair(r, x);
  };
  // doba bez naciśnięć do 20:30: wszystkie odświeżenia z inicjatywy panelu pełne, wygaszacz o 11:00 pełnym
  PanelSym s;
  Wynik w;
  doba(s, ep(2026, 11, 4, 20, 30), w);
  printf("  R-NF-25 × wygaszacz, doba bez naciśnięć: %d odświeżeń, niepełnych %d, wygaszacz 11:00 pełne %d\n",
         w.odswiezen, w.niepelnych, w.wyg11Pelne);
  SPRAWDZ(w.odswiezen > 0 && w.niepelnych == 0 && w.wyg11Pelne);
  // ▶ budzący o 11:20 na wygaszaczu, w czuwaniu ▶ i ↻: K1 pełne (obraz nieznany po głębokim śnie), K2 częściowe,
  // ekran dnia pełne
  PanelSym a;
  Wynik wa;
  doba(a, ep(2026, 11, 4, 11, 20), wa);
  SPRAWDZ(wa.wyg11Pelne && wa.niepelnych == 0);
  auto ra = przyciskiem(a, ep(2026, 11, 4, 11, 20),
                        {{0, BIT_PRAWY}, {300, 0}, {3300, BIT_PRAWY}, {3600, 0}, {6600, BIT_SRODEK}, {6900, 0}});
  printf("  R-NF-25 × wygaszacz: ▶ budzący, ▶, ↻ w czuwaniu: %s (ekran na końcu %d)\n", ra.first.c_str(),
         (int)a.p.start.stan.ekran);
  SPRAWDZ(ra.first == "PCP");
  SPRAWDZ(a.p.start.stan.ekran == (uint8_t)Ekran::DZIEN);
  // ↻ budzący o 11:20 na wygaszaczu: ekran dnia S-K-37, pełne
  PanelSym b;
  Wynik wb;
  doba(b, ep(2026, 11, 4, 11, 20), wb);
  auto rb = przyciskiem(b, ep(2026, 11, 4, 11, 20), {{0, BIT_SRODEK}, {300, 0}});
  printf("  R-NF-25 × wygaszacz: ↻ budzący: %s\n", rb.first.c_str());
  SPRAWDZ(rb.first == "P");
  SPRAWDZ(logMa({rb.second}, "slot dzienny: S-K-37"));
}

// Próg z telefonu (`dzien.wygaszacz_min`, R-KON-74) i wyłącznik (`obrazek` w `dzien.ukryte`, R-KON-77) w przebiegu doby
// T-TEST-461
// wymaganie: R-KON-74
static void testR_KON_74_progZKonfiguracji() {
  PanelSym a;
  DobaWyg d60 = dobaZNacisnieciami(a, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 12, 30), {}, "dzien.wygaszacz_min=60\n");
  SPRAWDZ(ekranO(d60.v, "S-K-37", 8) && ekranO(d60.v, "S-K-38", 9));
  PanelSym b;  // zła wartość → domyślne 180 min (wiersz pominięty z wpisem w logu)
  DobaWyg dz = dobaZNacisnieciami(b, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 12, 30), {}, "dzien.wygaszacz_min=10\n");
  SPRAWDZ(ekranO(dz.v, "S-K-37", 10) && ekranO(dz.v, "S-K-38", 11));
  PanelSym c;  // 720 min: przez cały slot (08:00–21:00) bez wygaszacza, bo 20:00 to dopiero 12 h po starcie
  DobaWyg d720 = dobaZNacisnieciami(c, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 20, 30), {}, "dzien.wygaszacz_min=720\n");
  SPRAWDZ(ekranO(d720.v, "S-K-37", 19) && !logMa(d720.v, "slot dzienny: S-K-38", 0, 20 * 60));
}
// Wyłącznik wygaszacza z telefonu (`dzien.wygaszacz=nie`, R-KON-77) i ukrycie obrazka (`obrazek` w `dzien.ukryte`,
// R-KON-71); zasilanie z kabla nie zmienia wygaszacza
// T-TEST-461
// wymaganie: R-KON-77
static void testR_KON_77_wylaczonyZTelefonu() {
  PanelSym w;
  DobaWyg dw = dobaZNacisnieciami(w, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 20, 30), {}, "dzien.wygaszacz=nie\n");
  SPRAWDZ(ekranO(dw.v, "S-K-37", 11) && ekranO(dw.v, "S-K-37", 20) && !logMa(dw.v, "slot dzienny: S-K-38"));
  PanelSym k;  // na kablu tak samo jak na baterii: wygaszacz o 11:00
  k.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
  DobaWyg dk = dobaZNacisnieciami(k, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 12, 30), {});
  SPRAWDZ(ekranO(dk.v, "S-K-37", 10) && ekranO(dk.v, "S-K-38", 11));
  PanelSym a;
  DobaWyg d = dobaZNacisnieciami(a, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 20, 30), {}, "dzien.ukryte=obrazek\n");
  SPRAWDZ(ekranO(d.v, "S-K-37", 11) && ekranO(d.v, "S-K-37", 20) && !logMa(d.v, "slot dzienny: S-K-38"));
  PanelSym b;  // brak obrazków: bez wygaszacza, bez pustego ekranu (R-KON-54)
  DobaWyg bez = dobaZNacisnieciami(b, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 20, 30), {}, "", false);
  SPRAWDZ(ekranO(bez.v, "S-K-37", 11) && !logMa(bez.v, "slot dzienny: S-K-38"));
}

// Restart panelu w trakcie wygaszacza (zanik zasilania o 12:30, prąd wraca o 13:10): pamięć RTC (start slotu,
// ostatnie naciśnięcie) przepada, slot zaczyna się od startu zasilania, więc licznik bezczynności też: S-K-37 od 13:10,
// wygaszacz o 17:00 (13:10 + 3 h, najbliższa ocena)
// T-TEST-461
// wymaganie: R-KON-73
static void testR_KON_73_restartWTrakcie() {
  PanelSym s;
  DobaWyg d = dobaZNacisnieciami(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 12, 30), {});
  SPRAWDZ(ekranO(d.v, "S-K-38", 12));
  PanelSym q;
  q.p.pliki.pliki = s.p.pliki.pliki;
  q.p.nvs.teksty = s.p.nvs.teksty;
  q.p.nvs.liczbyU = s.p.nvs.liczbyU;
  q.p.nvs.liczby = s.p.nvs.liczby;
  q.p.nvs.flagi = s.p.nvs.flagi;
  q.p.nvs.bajtyMap = s.p.nvs.bajtyMap;
  DobaWyg r = dobaZNacisnieciami(q, ep(2026, 11, 4, 13, 10), ep(2026, 11, 4, 18, 30), {});
  SPRAWDZ(logMa(r.v, "slot dzienny: S-K-37", 13 * 60 + 10, 13 * 60 + 11));
  SPRAWDZ(!logMa(r.v, "slot dzienny: S-K-38", 13 * 60, 17 * 60));
  SPRAWDZ(ekranO(r.v, "S-K-38", 17));
  // ten sam obrazek co przed restartem i bez drugiego wpisu w dzienniku (R-KON-61)
  std::string dz = q.p.pliki.pliki["/dane/obrazki.csv"];
  size_t p0 = dz.find("2026-11-04;");
  SPRAWDZ(p0 != std::string::npos && dz.find("2026-11-04;", p0 + 1) == std::string::npos);
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
    auto ms = [](uint32_t e) { return std::to_string(cywilneMs(e)); };
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
// z prognozy godzinowej i przy łóżku), więc sama zmiana prognozy i czujnika nie odświeża e-ink przy tym samym ekranie:
// od 16.11.2026 sprawdzane na wygaszaczu (S-K-38 o 11:00, potem 12:00, 13:00, 14:00 bez odświeżenia; S-K-37 zmienia się
// co godzinę przez znacznik `teraz`). Przy zmianie ekranu (11:00) jedno odświeżenie.
// wymaganie: R-KON-42
static void testR_KON_42_stopkaZKlatki() {
  PanelSym s;
  s.p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
  s.p.pliki.pliki["/obrazki/obrazki.json"] = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\"}]}";
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 14, 30), odpowiedziB9ZmiennaTemp, true,
                               konf(true));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-37", 8 * 60, 8 * 60 + 1));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-38", 11 * 60, 11 * 60 + 1));
  SPRAWDZ(logMa(v, "slot dzienny: S-K-38", 13 * 60, 13 * 60 + 1));
  int o11 = 0, potem = 0;
  for (auto& w : v) {
    if (w.minuta == 11 * 60) o11 += w.m.odswiezen;
    if (w.minuta > 11 * 60) potem += w.m.odswiezen;
  }
  ROWNE_L(o11, 1);    // inny ekran (wygaszacz po 3 h): jedno odświeżenie, już ze świeżymi odczytami
  ROWNE_L(potem, 0);  // 12:00…14:00: za oknem i przy łóżku co godzinę inaczej, bez odświeżenia
  SPRAWDZ(logMa(v, "bez zmian: klatka jak wyświetlona", 12 * 60, 12 * 60 + 1));
}

// ---------- belka informacyjna i bateria w slocie dziennym (spec 5.4.17, R-EKR-66, R-KON-69) ----------
// Doba 4.11.2026 od 00:05 do 20:30 jak wyżej; `ustaw(s, t, nr)` ustawia przed każdym wybudzeniem napięcie baterii
// i stan zasilania, który „widzi” panel.
struct DobaBaterii {
  int odswiezen = 0, wybudzen = 0;
  std::vector<Wyb> v;
  std::vector<uint8_t> klatka;                 // ostatnia wyświetlona klatka
  std::map<std::string, std::string> pliki;    // pliki panelu po dobie
  std::map<std::string, std::string> nvsTeksty;
  std::map<std::string, uint32_t> nvsLiczbyU;
  std::map<std::string, int32_t> nvsLiczby;
};
static bool czarnyK(const std::vector<uint8_t>& k, int x, int y) {
  return k.size() == 48000 && (k[(size_t)(y * 100 + x / 8)] & (0x80 >> (x & 7))) != 0;
}
// ikona baterii w stopce slotu dziennego: górna krawędź obrysu 34 px na y 447 od x 28 i nosek na x 62…64
static bool ikonaWStopce(const std::vector<uint8_t>& k) {
  for (int x = 28; x < 62; x++)
    if (!czarnyK(k, x, 447) || !czarnyK(k, x, 464)) return false;
  return czarnyK(k, 62, 455) && czarnyK(k, 64, 455);
}
template <typename F>
static DobaBaterii dobaBaterii(bool obrazki, F ustaw, const std::string& konfiguracja = "",
                               uint32_t doT = 0) {
  PanelSym s;
  if (obrazki) {
    s.p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
    s.p.pliki.pliki["/obrazki/obrazki.json"] = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\"}]}";
  }
  if (!konfiguracja.empty()) s.p.pliki.pliki["/konfiguracja.txt"] = konfiguracja;
  DobaBaterii d;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  uint32_t t = ep(2026, 11, 4, 0, 5);
  if (!doT) doT = ep(2026, 11, 4, 20, 30);
  PowodWybudzenia pw = PowodWybudzenia::START;
  const KonfiguracjaPanelu k = konf(true);
  for (int i = 0; i < 600 && t < doT; i++) {
    odpowiedziB9(s, t);
    ustaw(s, t, i);
    Wyb w = wybudz(s, t, pw, k);
    d.v.push_back(w);
    d.odswiezen += w.m.odswiezen;
    d.wybudzen++;
    if (w.m.odswiezen) d.klatka.assign(s.ekran.bufor(), s.ekran.bufor() + 48000);
    if (!w.spal) break;
    uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
    if (snu == 0) snu = 1;
    t += w.m.msPrzySnie / 1000 + snu;
    pw = PowodWybudzenia::ZEGAR;
  }
  d.pliki = s.p.pliki.pliki;
  d.nvsTeksty = s.p.nvs.teksty;
  d.nvsLiczbyU = s.p.nvs.liczbyU;
  d.nvsLiczby = s.p.nvs.liczby;
  return d;
}
static int odswiezenOd(const DobaBaterii& d, int minuta) {
  int n = 0;
  for (auto& w : d.v)
    if (w.minuta >= minuta) n += w.m.odswiezen;
  return n;
}
static auto stanStaly(float v, Zasilanie z) {
  return [=](PanelSym& s, uint32_t, int) {
    s.p.czujniki.o.bateriaV = v;
    s.p.czujniki.zas = z;
  };
}
static const float V78 = 4.002f, V18 = 3.722f, V15 = 3.710f, V4 = 3.550f;  // krzywa M-25 (src/bateria.cpp)

// R-KON-69 na dyrygencie: ostrzeżenie w stopce slotu dziennego tylko w paśmie między progiem linii E-05 (15%)
// a 20%; przy aktywnym E-05 o baterii mówi linia uwagi i stopka zostaje z wynikiem nocy; na kablu bez ostrzeżenia.
// wymaganie: R-KON-69
static void testR_KON_69_stopkaSlotuZDyrygenta() {
  DobaBaterii b78 = dobaBaterii(true, stanStaly(V78, Zasilanie::BATERIA));
  DobaBaterii b18 = dobaBaterii(true, stanStaly(V18, Zasilanie::BATERIA));
  DobaBaterii b15 = dobaBaterii(true, stanStaly(V15, Zasilanie::BATERIA));
  DobaBaterii k18 = dobaBaterii(true, stanStaly(V18, Zasilanie::ZEWNETRZNE));
  SPRAWDZ(logMa(b18.v, "slot dzienny: S-K-3", 8 * 60, 21 * 60) && logMa(b78.v, "slot dzienny: S-K-3", 8 * 60, 21 * 60));
  SPRAWDZ(!ikonaWStopce(b78.klatka));  // 78%: wynik nocy
  SPRAWDZ(ikonaWStopce(b18.klatka));   // 18%: ikona baterii i `18% · podłącz kabel`
  SPRAWDZ(!ikonaWStopce(b15.klatka));  // 15%: linia E-05 aktywna, stopka z wynikiem nocy (bez dublowania)
  SPRAWDZ(logMa(b15.v, "E-05") || logMa(b15.v, "bateria"));
  SPRAWDZ(!ikonaWStopce(k18.klatka));  // 18% na kablu: bez ostrzeżenia
  // slot dzienny nie ma belki: wiersze 41…43 nie są linią belki na żadnej z klatek końca dnia
  for (const DobaBaterii* d : {&b78, &b18, &b15}) {
    bool linia = true;
    for (int x = 0; x < 800; x++) linia = linia && czarnyK(d->klatka, x, 42);
    SPRAWDZ(!linia);
  }
  printf("  slot dzienny, e-ink w dobie: 78%% %d, 18%% %d, 15%% %d, 18%% na kablu %d odświeżeń\n", b78.odswiezen,
         b18.odswiezen, b15.odswiezen, k18.odswiezen);
  // od decyzji 16.11.2026 22:08 (R-KON-75, W3): wygaszacz także przy stanie „niska bateria”; obrazek rysuje tę samą
  // stopkę z ikoną baterii i `{x}% · podłącz kabel` (ostatnia klatka dnia to S-K-38, wyżej ikonaWStopce(b18.klatka)),
  // a liczba odświeżeń doby przy 18% jest taka sama jak przy 78% (stały stan stopki nie dodaje odświeżeń). Na kablu
  // od startu slotu (08:00) tyle samo co przy 78% (rano kabel ma swój rytm sprawdzania nocy, R-DANE-62)
  SPRAWDZ(logMa(b18.v, "slot dzienny: S-K-38", 11 * 60, 11 * 60 + 1) && logMa(b18.v, "slot dzienny: S-K-38", 20 * 60, 20 * 60 + 1));
  SPRAWDZ(logMa(b78.v, "slot dzienny: S-K-38", 11 * 60, 11 * 60 + 1) && logMa(k18.v, "slot dzienny: S-K-38", 11 * 60, 11 * 60 + 1));
  ROWNE_L(b18.odswiezen, b78.odswiezen);
  ROWNE_L(odswiezenOd(k18, 8 * 60), odswiezenOd(b78, 8 * 60));
  ROWNE_L(odswiezenOd(b78, 11 * 60 + 1), 0);
}

// Stan „nie wiem” nie przełącza stanu „niska bateria” i nie odświeża e-ink: zasilanie na zmianę „zewnętrzne”
// i „nieznane” przy 18% (odczyt ładowarki raz się udaje, raz nie) daje tyle odświeżeń co stałe „zewnętrzne”;
// brak pomiaru napięcia w co trzecim wybudzeniu przy 18% tyle co stały pomiar.
// wymaganie: R-EKR-66
static void testR_EKR_66_stanNieznanyBezOdswiezen() {
  DobaBaterii staleKabel = dobaBaterii(true, stanStaly(V18, Zasilanie::ZEWNETRZNE));
  DobaBaterii miga = dobaBaterii(true, [](PanelSym& s, uint32_t, int nr) {
    s.p.czujniki.o.bateriaV = V18;
    s.p.czujniki.zas = nr % 2 ? Zasilanie::NIEZNANE : Zasilanie::ZEWNETRZNE;
  });
  DobaBaterii staleBat = dobaBaterii(true, stanStaly(V18, Zasilanie::BATERIA));
  DobaBaterii brakPomiaru = dobaBaterii(true, [](PanelSym& s, uint32_t, int nr) {
    s.p.czujniki.o.bateriaV = nr % 3 == 2 ? NAN : V18;
    s.p.czujniki.zas = Zasilanie::BATERIA;
  });
  DobaBaterii dwaBraki = dobaBaterii(true, [](PanelSym& s, uint32_t, int nr) {  // dwa wybudzenia z rzędu bez pomiaru
    s.p.czujniki.o.bateriaV = nr % 4 >= 2 ? NAN : V18;
    s.p.czujniki.zas = Zasilanie::BATERIA;
  });
  printf("  e-ink w dobie przy 18%%: kabel stale %d, kabel / nieznane na zmianę %d; bateria stale %d, brak pomiaru co "
         "trzecie wybudzenie %d, dwa braki z rzędu co cztery %d (wybudzeń %d)\n",
         staleKabel.odswiezen, miga.odswiezen, staleBat.odswiezen, brakPomiaru.odswiezen, dwaBraki.odswiezen,
         staleBat.wybudzen);
  SPRAWDZ(staleKabel.odswiezen > 0);
  // od startu slotu (rano na kablu inny rytm sprawdzania nocy, R-DANE-62, niezależny od stanu baterii)
  ROWNE_L(odswiezenOd(miga, 8 * 60), odswiezenOd(staleKabel, 8 * 60));
  ROWNE_L(brakPomiaru.odswiezen, staleBat.odswiezen);
  ROWNE_L(dwaBraki.odswiezen, staleBat.odswiezen);
  SPRAWDZ(!ikonaWStopce(miga.klatka));        // ostatni znany stan to kabel: bez ostrzeżenia
  SPRAWDZ(ikonaWStopce(brakPomiaru.klatka));  // ostrzeżenie zostaje mimo wybudzeń bez pomiaru
}

// Drugie rysowanie ze świeżym stanem baterii (gdy klatka i tak się zmienia) nie może niczego liczyć dwa razy:
// po dobie ze slotem dziennym pliki panelu (dzienniki ekranów i obrazków, czynniki, reguły, noce) i NVS są takie
// same przy napięciu stałym i przy spadającym (70% → 40%, każda zmiana klatki rysuje wtedy dwa razy). Poza
// porównaniem: dziennik wybudzeń i plik baterii, które zapisują samo napięcie.
// wymaganie: R-EKR-66
static void testR_EKR_66_drugieRysowanieNieLiczyDwaRazy() {
  const uint32_t od = ep(2026, 11, 4, 0, 5), doT = ep(2026, 11, 4, 22, 0);
  DobaBaterii stale = dobaBaterii(true, stanStaly(3.950f, Zasilanie::BATERIA), "", doT);
  DobaBaterii spada = dobaBaterii(true, [=](PanelSym& s, uint32_t t, int) {
    s.p.czujniki.o.bateriaV = 3.950f - 0.150f * (float)(t - od) / (float)(doT - od);
    s.p.czujniki.zas = Zasilanie::BATERIA;
  }, "", doT);
  int dwaRazy = 0;
  for (auto& w : spada.v)
    for (auto& z : w.zdarzenia) dwaRazy += z.find("slot dzienny: S-K-3") != std::string::npos;
  int raz = 0;
  for (auto& w : stale.v)
    for (auto& z : w.zdarzenia) raz += z.find("slot dzienny: S-K-3") != std::string::npos;
  auto bezNapiecia = [](std::map<std::string, std::string> m) {
    m.erase("/log/wybudzenia.csv");
    m.erase("/bateria.csv");
    return m;
  };
  std::map<std::string, std::string> a = bezNapiecia(stale.pliki), b = bezNapiecia(spada.pliki);
  int rozne = 0;
  for (auto& x : a)
    if (!b.count(x.first) || b[x.first] != x.second) { rozne++; printf("  różny plik: %s\n", x.first.c_str()); }
  for (auto& x : b)
    if (!a.count(x.first)) { rozne++; printf("  plik tylko przy spadku: %s\n", x.first.c_str()); }
  printf("  drugie rysowanie: rysowań slotu przy napięciu stałym %d, przy spadającym %d; plików porównanych %zu, "
         "różnych %d; e-ink %d i %d\n", raz, dwaRazy, a.size(), rozne, stale.odswiezen, spada.odswiezen);
  SPRAWDZ(dwaRazy > raz);  // scenariusz naprawdę rysuje drugi raz
  ROWNE_L(rozne, 0);
  SPRAWDZ(stale.nvsTeksty == spada.nvsTeksty && stale.nvsLiczbyU == spada.nvsLiczbyU && stale.nvsLiczby == spada.nvsLiczby);
  ROWNE_L(spada.odswiezen, stale.odswiezen);
  SPRAWDZ(a.count("/dane/obrazki.csv") == 1 && a.count("/dane/humor.csv") + a.count("/humor.csv") >= 1);
}

// S-K-35 z zapisem wymiany przytrzymaniem ◀ ma w stopce podpisy przycisków (R-PRZ-04, R-MAS-18): przy niskiej
// baterii zostają, ostrzeżenie ich nie zastępuje.
// wymaganie: R-KON-69
static void testR_KON_69_SK35ZostajeZPodpisami() {
  const std::string kf = "wymiana.filtr=2026-10-07\nokres.filtr_tyg=4\n";  // termin 4.11: S-K-35 w slocie dziennym
  DobaBaterii b78 = dobaBaterii(false, stanStaly(V78, Zasilanie::BATERIA), kf, ep(2026, 11, 4, 12, 30));
  DobaBaterii b18 = dobaBaterii(false, stanStaly(V18, Zasilanie::BATERIA), kf, ep(2026, 11, 4, 12, 30));
  SPRAWDZ(logMa(b78.v, "slot dzienny: S-K-35") && logMa(b18.v, "slot dzienny: S-K-35"));
  SPRAWDZ(logMa(b18.v, "◀ 2 s zapisuje wymianę (R-MAS-18)"));
  SPRAWDZ(!ikonaWStopce(b18.klatka));
  // stopka (wiersze 436…479) identyczna przy 78% i 18%
  bool taSama = b78.klatka.size() == 48000 && b18.klatka.size() == 48000;
  for (int y = 436; taSama && y < 480; y++)
    for (int x = 0; x < 800; x++)
      if (czarnyK(b78.klatka, x, y) != czarnyK(b18.klatka, x, y)) { taSama = false; break; }
  SPRAWDZ(taSama);
}

int main() {
  testR_KON_42_stopkaZKlatki();
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testSlotDziennyZObrazkiem();
  testSlotBezObrazkowIBezMiasta();
  testR_KON_76_przyciskNaWygaszaczu();
  testR_NF_25_wygaszaczOdswiezenie();
  testR_KON_74_progZKonfiguracji();
  testR_KON_77_wylaczonyZTelefonu();
  testR_KON_73_restartWTrakcie();
  testDrzemka();
  testR_KON_69_stopkaSlotuZDyrygenta();
  testR_EKR_66_stanNieznanyBezOdswiezen();
  testR_EKR_66_drugieRysowanieNieLiczyDwaRazy();
  testR_KON_69_SK35ZostajeZPodpisami();
  printf("testy_slot_dzienny_dyrygent: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
