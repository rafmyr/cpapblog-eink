// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: stany nocy 5.1.4 (src/stan_nocy.h) jako funkcja czysta
// i w przebiegu dyrygenta na atrapach (doba z nocą w drodze, R-DANE-25 o 23:30, 2 i 3 noce bez danych R-AW-24, E-10
// tylko przy odpowiadającym AirBridge), slot wieczorny i tygodniowy (R-KON-01, R-KON-09, R-KON-17, R-KON-08),
// rytm pełnego ekranu L według faktycznych pokazań (R-PIE-08) i linie kolejki (R-NOR-21, R-SKI-39, R-SKI-33,
// R-SKI-41, R-PIE-09, R-INF-10). Pomocnicze funkcje symulacji doby skopiowane z test/testy_slot_dzienny_dyrygent.cpp.
// Uruchomienie: tools/prototyp/testy.sh (program testy_stany_nocy_sloty_wieczorne).
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
#include "kolejka.h"
#include "nawigacja.h"
#include "ekrany.h"
#include "dzien.h"
#include "sprawy.h"
#include "obrazki.h"
#include "plan.h"
#include "slot_dzienny.h"
#include "slot_wieczorny.h"
#include "stan_nocy.h"
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
static std::string czesciowy(const std::string& r) {
  std::string x = r;
  size_t i = x.find("\"partial\":false");
  if (i != std::string::npos) x.replace(i, 15, "\"partial\":true");
  return x;
}

// ---------- stany nocy 5.1.4: funkcja czysta ----------
static void testStanNocy() {
  using namespace stan_nocy;
  const std::string D = "2026-11-04";
  FazaNocy s;
  Noc rek;
  rek.data = D;
  rek.m01 = 400;
  // nieobecna przed 11:00: lista bez dnia nie daje E-10 (R-DANE-22)
  s = przejdz(s, D, 7 * 60, Obserwacja::LISTA_BEZ_DNIA, nullptr);
  SPRAWDZ(s.data == D && s.stan == Stan::NIEOBECNA && !bezTerapii(s, D));
  // 11:00, AirBridge odpowiada, dnia nie ma: bez terapii (E-10, R-DANE-20)
  s = przejdz(s, D, 11 * 60, Obserwacja::LISTA_BEZ_DNIA, nullptr);
  SPRAWDZ(s.stan == Stan::BEZ_TERAPII && bezTerapii(s, D));
  // dzień pojawia się z partial = true o 12:40: w budowie, E-10 cofnięte (R-DANE-21), wejście po 11:00 (R-DANE-25)
  s = przejdz(s, D, 12 * 60 + 40, Obserwacja::RAPORT_CZESCIOWY, nullptr);
  SPRAWDZ(s.stan == Stan::W_BUDOWIE && s.budowaPo11 && s.naLiscie && !bezTerapii(s, D));
  SPRAWDZ(!czesciowaTeraz(s, D, 13 * 60) && !czesciowaTeraz(s, D, 23 * 60) && czesciowaTeraz(s, D, 23 * 60 + 30));
  SPRAWDZ(!pobranieO2330(s, D, 23 * 60, nullptr) && pobranieO2330(s, D, 23 * 60 + 30, nullptr));
  // rekord kompletny rozstrzyga
  s = przejdz(s, D, 15 * 60, Obserwacja::ZEGAR, &rek);
  SPRAWDZ(s.stan == Stan::KOMPLETNA && !pobranieO2330(s, D, 23 * 60 + 30, &rek));
  // wejście w budowę przed 11:00: rekord „częściowa” od 11:00 (R-DANE-15)
  FazaNocy b;
  b = przejdz(b, D, 7 * 60, Obserwacja::RAPORT_CZESCIOWY, nullptr);
  SPRAWDZ(b.stan == Stan::W_BUDOWIE && !b.budowaPo11);
  SPRAWDZ(!czesciowaTeraz(b, D, 10 * 60 + 30) && czesciowaTeraz(b, D, 11 * 60));
  // 11:00 bez odpowiedzi: E-01 (brak danych), nie E-10; potem lista bez dnia: E-01 znika, E-10
  FazaNocy c;
  c = przejdz(c, D, 11 * 60, Obserwacja::BRAK_ODPOWIEDZI, nullptr);
  SPRAWDZ(c.stan == Stan::BRAK_DANYCH && !bezTerapii(c, D));
  c = przejdz(c, D, 13 * 60 + 30, Obserwacja::LISTA_BEZ_DNIA, nullptr);
  SPRAWDZ(c.stan == Stan::BEZ_TERAPII);
  // przed 11:00 brak odpowiedzi nic nie zmienia (S10, R-EKR-57)
  FazaNocy e;
  e = przejdz(e, D, 9 * 60, Obserwacja::BRAK_ODPOWIEDZI, nullptr);
  SPRAWDZ(e.stan == Stan::NIEOBECNA);
  // R-DANE-35: status w budowie o 11:00, lista pusta: bez E-10 i E-01; o 23:30 nieobecna bez rekordu
  FazaNocy f;
  f = przejdz(f, D, 10 * 60, Obserwacja::STATUS_BUDOWA, nullptr);
  f = przejdz(f, D, 11 * 60, Obserwacja::STATUS_BUDOWA, nullptr);
  SPRAWDZ(f.stan == Stan::W_BUDOWIE_STATUS && !bezTerapii(f, D));
  f = przejdz(f, D, 11 * 60, Obserwacja::BRAK_ODPOWIEDZI, nullptr);
  SPRAWDZ(f.stan == Stan::W_BUDOWIE_STATUS);
  f = przejdz(f, D, 23 * 60 + 30, Obserwacja::ZEGAR, nullptr);
  SPRAWDZ(f.stan == Stan::NIEOBECNA && !bezTerapii(f, D) && !pobranieO2330(f, D, 23 * 60 + 30, nullptr));
  // R-DANE-35: noc bez terapii zostaje, choćby wieczorem rop = 1
  FazaNocy g;
  g = przejdz(g, D, 11 * 60, Obserwacja::LISTA_BEZ_DNIA, nullptr);
  g = przejdz(g, D, 22 * 60, Obserwacja::STATUS_BUDOWA, nullptr);
  SPRAWDZ(g.stan == Stan::BEZ_TERAPII);
  // R-DANE-35: dzień na liście z partial, potem rop = 1: o 23:30 pobranie raportu na rekord „częściowa”
  FazaNocy h;
  h = przejdz(h, D, 9 * 60, Obserwacja::RAPORT_CZESCIOWY, nullptr);
  h = przejdz(h, D, 21 * 60, Obserwacja::STATUS_BUDOWA, nullptr);
  SPRAWDZ(h.stan == Stan::W_BUDOWIE_STATUS && h.naLiscie && pobranieO2330(h, D, 23 * 60 + 30, nullptr));
  // nowa data zaczyna od nowa
  FazaNocy n = przejdz(g, "2026-11-05", 6 * 60, Obserwacja::ZEGAR, nullptr);
  SPRAWDZ(n.data == "2026-11-05" && n.stan == Stan::NIEOBECNA && !bezTerapii(n, "2026-11-05"));
  // zapis w NVS w obie strony; śmieci = stan domyślny
  FazaNocy z = odczytStanu(zapisStanu(h));
  SPRAWDZ(z == h);
  SPRAWDZ(odczytStanu("zle").data.empty() && odczytStanu("2026-11-04;cos;").data.empty());
}

// ---------- doba z nocą w drodze (R-DANE-20, R-DANE-21, R-DANE-25) w dyrygencie ----------
static const long DOBA_B = nrDnia("2026-11-03");  // noc z poranka 4.11
static void odpNocWDrodze(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  s.siec.siecJest = true;
  uint32_t pojawia = ep(2026, 11, 4, 12, 40);
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaNocy(DOBA_B - 1), 200, raport(DOBA_B - 1, ep(2026, 11, 2, 23, 0), ep(2026, 11, 3, 6, 30)));
  if (t < pojawia) {
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_B - 1, DOBA_B - 1));
  } else {
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_B - 1, DOBA_B));
    s.siec.dodaj(sciezkaNocy(DOBA_B), 200, czesciowy(raport(DOBA_B, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40))));
  }
}
static void testNocWDrodze() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 5, 0), ep(2026, 11, 4, 23, 50), odpNocWDrodze);
  // przed 11:00 noc „nieobecna”: S10, bez E-10
  SPRAWDZ(logMa(v, "ekran dnia S10", 5 * 60, 11 * 60));
  SPRAWDZ(!logMa(v, "bez_terapii", 5 * 60, 11 * 60));
  // 11:00: AirBridge odpowiada, dnia nie ma → bez terapii (E-10)
  SPRAWDZ(logMa(v, "stan nocy 2026-11-04: nieobecna → bez_terapii", 11 * 60, 11 * 60 + 5));
  // 12:40: dzień z partial → w budowie (cofnięcie E-10), bez rekordu „częściowa” do 23:30 (R-DANE-25)
  SPRAWDZ(logMa(v, "bez_terapii → w_budowie", 12 * 60 + 40, 13 * 60 + 15));
  SPRAWDZ(logMa(v, "rekord „częściowa” o 23:30 (R-DANE-25)", 13 * 60, 23 * 60 + 30));
  SPRAWDZ(!logMa(v, "(częściowa)", 0, 23 * 60 + 30));
  // 23:30: rekord „częściowa”
  SPRAWDZ(logMa(v, "23:30, raport nocy w budowie (R-DANE-25)", 23 * 60 + 30, 23 * 60 + 35));
  SPRAWDZ(logMa(v, "(częściowa)", 23 * 60 + 30, 23 * 60 + 35));
  std::string noce = s.p.pliki.pliki["/dane/noce/2026.csv"];
  SPRAWDZ(noce.find("2026-11-04;0;") != std::string::npos);  // rekord nieważny (4.2)
}

// AirBridge milczy od 2.11 08:00: noce 2.11, 3.11, 4.11 bez danych; teksty S5 według R-AW-24 i R-AW-32, bez E-10
static void odpMilczy(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  if (t >= ep(2026, 11, 2, 8, 0)) {
    s.siec.siecJest = true;  // Wi-Fi jest, AirBridge nie odpowiada (atrapa bez odpowiedzi = błąd połączenia)
    return;
  }
  long d = nrDnia("2026-11-01");
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(d - 1, d));
  s.siec.dodaj(sciezkaNocy(d - 1), 200, raport(d - 1, ep(2026, 10, 31, 23, 0), ep(2026, 11, 1, 6, 30)));
  s.siec.dodaj(sciezkaNocy(d), 200, raport(d, ep(2026, 11, 1, 23, 0), ep(2026, 11, 2, 6, 30)));
}
static void testNoceBezDanych() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 2, 6, 50), ep(2026, 11, 4, 12, 10), odpMilczy);
  // 2.11 ma noc (pobrana 06:50); 3.11 o 11:00 pierwsza noc bez danych: `Czekam na dane`, `Pytam co 30 min.`
  SPRAWDZ(logMa(v, "S5: Noc 03.11 jeszcze w drodze. / AirBridge milczy od", 11 * 60, 12 * 60, "2026-11-03"));
  // 4.11: druga noc bez danych z rzędu (R-AW-24)
  SPRAWDZ(logMa(v, "S5: Brak danych od 2 nocy / Wyjazd z aparatem? Dane wrócą same. Sprawdzam co", 11 * 60, 12 * 60,
                "2026-11-04"));
  // E-01, nigdy E-10 (R-DANE-20)
  SPRAWDZ(!logMa(v, "bez_terapii"));
  SPRAWDZ(logMa(v, "stan nocy 2026-11-03: nieobecna → brak_danych", 11 * 60, 11 * 60 + 5, "2026-11-03"));
  // przed 11:00 S10, nie S5 (E-01 dopiero od 11:00) w pierwszej nocy bez danych
  SPRAWDZ(logMa(v, "ekran dnia S10", 5 * 60, 11 * 60, "2026-11-03"));
}
static void testTrzeciaNoc() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 2, 6, 50), ep(2026, 11, 5, 11, 40), odpMilczy);
  SPRAWDZ(logMa(v, "S5: Brak danych od 3 nocy / AirBridge nie odpowiada od 02.11. Sprawdzam co", 11 * 60, 12 * 60,
                "2026-11-05"));
}

// ---------- R-PIE-08: rytm pełnego ekranu L od rzeczywistego pokazania ----------
static sprawy::SprawaL sprawaL(const std::string& kl, reguly::Pilnosc p, const std::string& od) {
  sprawy::SprawaL x;
  x.klucz = x.id = kl;
  x.stan = reguly::StanReguly::AKTYWNA;
  x.pilnosc = p;
  x.ton = sprawy::Ton::LEKARZ;
  x.tytul = kl == "L-11" ? "Senność mimo terapii" : "Saturacja: sprawa dla lekarza";
  x.linia = kl + ": linia C. Na kontroli: raport 2 / 3.";
  x.aktywnaOd = od;
  return x;
}
static void testRytmEkranuL() {
  using reguly::Pilnosc;
  // L-06(b) (A) wchodzi 1.11, dzień 1 przegrywa z S5 (nie stanął na ekranie): 2.11 pełny ekran (R-PIE-08)
  sprawy::SprawaL a = sprawaL("L-06/b", Pilnosc::A, "2026-11-01");
  SPRAWDZ(sprawy::pelnyEkranDzis(a, "2026-11-01") && sprawy::pelnyEkranDzis(a, "2026-11-02"));
  reguly::Pamiec p;
  SPRAWDZ(sprawy::zapiszPokazanie(p, "L-06/b", "2026-11-02"));
  SPRAWDZ(!sprawy::zapiszPokazanie(p, "L-06/b", "2026-11-02"));  // drugie pokazanie tego dnia nic nie zmienia
  a.pokazanoPelny = sprawy::ostatniePokazanie(p, "L-06/b");
  SPRAWDZ(sprawy::pelnyEkranDzis(a, "2026-11-02"));   // cały dzień pokazania
  SPRAWDZ(!sprawy::pelnyEkranDzis(a, "2026-11-03") && !sprawy::pelnyEkranDzis(a, "2026-11-04"));
  SPRAWDZ(sprawy::pelnyEkranDzis(a, "2026-11-05"));   // 3 dni od pokazania, nie od wejścia (od wejścia: 4.11)
  // B co 7 dni od pokazania
  sprawy::SprawaL b = sprawaL("L-11", Pilnosc::B, "2026-11-01");
  b.pokazanoPelny = "2026-11-03";
  SPRAWDZ(!sprawy::pelnyEkranDzis(b, "2026-11-08") && sprawy::pelnyEkranDzis(b, "2026-11-10"));
  // pokazanie sprzed ponownego wejścia się nie liczy
  b.aktywnaOd = "2026-11-20";
  SPRAWDZ(sprawy::pelnyEkranDzis(b, "2026-11-21"));
  // wygaszony B (R-SKI-09), wstrzymany (R-INF-02), karencja (R-SKI-08): bez pełnego ekranu
  b.pokazanoPelny.clear();
  for (const char* st : {"wygaszony_b", "wstrzymany", "karencja"}) {
    b.stanWzorca = st;
    SPRAWDZ(!sprawy::pelnyEkranDzis(b, "2026-11-21"));
  }
  // wpis pokazania nie myli się z regułą: klucz pomocniczy, plik `reguly` w obie strony bez zmian formatu
  SPRAWDZ(sprawy::kluczPomocniczy(sprawy::kluczPokazania("L-03/ge10")) && !sprawy::kluczPomocniczy("L-03/ge10"));
}

// ---------- linie kolejki ----------
static std::vector<kolejka::LiniaUwagi> kol(const std::string& dzis, const std::vector<sprawy::SprawaL>& s,
                                          const reguly::Pamiec* p, int minuta = -1,
                                          const std::vector<sprawy::SprawaMetryki>* m = nullptr,
                                          const std::vector<sprawy::Odpowiedz>* o = nullptr,
                                          const std::string& awaria = "", const std::string& infekcja = "") {
  kolejka::WejscieKolejki w;
  w.dzis = dzis;
  w.sprawy = &s;
  w.pamiec = p;
  w.minuta = minuta;
  w.metryki = m;
  w.odpowiedziPelne = o;
  w.awaria = awaria;
  w.okresInfekcjiOd = infekcja;
  return kolejka::kolejkaLinii(w);
}
static bool maLinie(const std::vector<kolejka::LiniaUwagi>& k, const std::string& t) {
  for (auto& l : k)
    if (l.tekst == t) return true;
  return false;
}
static void testLinieB11() {
  using reguly::Pilnosc;
  // R-SKI-41: rotacja linii C w niedziele według ostatniego pokazania; R-SKI-12: od 17:00 (slot tygodniowy)
  std::vector<sprawy::SprawaL> s = {sprawaL("L-08", Pilnosc::C, "2026-10-01"), sprawaL("L-07", Pilnosc::C, "2026-10-01")};
  reguly::Pamiec p;
  const std::string N1 = "2026-11-01", N2 = "2026-11-08", N3 = "2026-11-15";  // niedziele
  SPRAWDZ(kol(N1, s, &p, 10 * 60).empty());  // niedziela rano: slot tygodniowy jeszcze nie trwa
  auto k = kol(N1, s, &p, 17 * 60);
  SPRAWDZ(k.size() == 1 && k[0].zrodlo == "L-08");
  sprawy::zapiszPokazanie(p, kolejka::kluczPokazaniaLinii(k[0]), N1);
  k = kol(N2, s, &p, 17 * 60);
  SPRAWDZ(k.size() == 1 && k[0].zrodlo == "L-07");
  // niedziela 2 z S5: linia nie stanęła na ekranie, więc niedziela 3 dalej L-07
  k = kol(N3, s, &p, 17 * 60);
  SPRAWDZ(k.size() == 1 && k[0].zrodlo == "L-07");
  // R-SKI-33: wygaszony B w slocie tygodniowym przed linią C; R-SKI-13: 1. dnia miesiąca
  sprawy::SprawaL b = sprawaL("L-11", Pilnosc::B, "2026-10-01");
  b.stanWzorca = "wygaszony_b";
  s.insert(s.begin(), b);
  k = kol(N3, s, &p, 17 * 60);
  SPRAWDZ(k.size() == 2 && k[0].tekst == "Wciąż: Senność mimo terapii. Pokaż na kontroli.");
  k = kol("2026-12-01", s, &p, 9 * 60);
  SPRAWDZ(k.size() == 1 && k[0].tekst == "Wciąż: Senność mimo terapii. Pokaż na kontroli.");
  SPRAWDZ(kol("2026-11-17", s, &p, 9 * 60).empty());  // zwykły dzień: wygaszony B bez linii co 3 dni
  // R-NOR-21: metryka od 21 nocy, raz na 7 dni od pokazania
  std::vector<sprawy::SprawaMetryki> m = {{"AHI", 21, "6,0", "0,8", "2,1", "6,2", "M-02", ""}};
  std::vector<sprawy::SprawaL> pusto;
  reguly::Pamiec q;
  k = kol("2026-11-10", pusto, &q, 9 * 60, &m);
  SPRAWDZ(k.size() == 1 && k[0].tekst == "Od 3 tyg.: AHI 6,2. Liczby w raporcie." && k[0].zdrowie());
  SPRAWDZ(k.size() == 1 && k[0].tekst.size() <= 60 + 10);  // ≤ 60 znaków (UTF-8: litery polskie po 2 bajty)
  sprawy::zapiszPokazanie(q, kolejka::kluczPokazaniaLinii(k[0]), "2026-11-10");
  for (int d = 1; d < 7; d++) SPRAWDZ(kol(dodajDni("2026-11-10", d), pusto, &q, 9 * 60, &m).empty());
  m[0].nocy = 28;
  k = kol("2026-11-17", pusto, &q, 9 * 60, &m);
  SPRAWDZ(k.size() == 1 && k[0].tekst == "Od 4 tyg.: AHI 6,2. Liczby w raporcie.");
  m[0].nocy = 20;
  SPRAWDZ(kol("2026-11-24", pusto, &q, 9 * 60, &m).empty());
  // R-SKI-39: 4 ostatnie pokazane pytania o senność bez odpowiedzi
  std::vector<sprawy::Odpowiedz> o;
  for (const char* d : {"2026-10-11", "2026-10-18", "2026-10-25"}) o.push_back({d, sprawy::Pytanie::SENNOSC, 'b'});
  SPRAWDZ(!maLinie(kol("2026-11-01", pusto, nullptr, 18 * 60, nullptr, &o), "Bez odpowiedzi o senność nie oceniam części spraw."));
  o.push_back({"2026-11-01", sprawy::Pytanie::SENNOSC, 'b'});
  SPRAWDZ(maLinie(kol("2026-11-01", pusto, nullptr, 18 * 60, nullptr, &o), "Bez odpowiedzi o senność nie oceniam części spraw."));
  o.back().odp = 'n';
  SPRAWDZ(!maLinie(kol("2026-11-01", pusto, nullptr, 18 * 60, nullptr, &o), "Bez odpowiedzi o senność nie oceniam części spraw."));
  // R-PIE-09: trwała awaria była wczoraj na ekranie, czeka linia wzorca L → dziś linia L, jutro awaria
  std::vector<sprawy::SprawaL> s2 = {sprawaL("L-11", Pilnosc::B, "2026-11-01")};  // 4.11: 3. dzień → linia co 3 dni
  reguly::Pamiec r;
  sprawy::zapiszPokazanie(r, "linia:awaria", "2026-11-03");
  k = kol("2026-11-04", s2, &r, 9 * 60, nullptr, nullptr, "Pamięć panelu: awaria.");
  SPRAWDZ(k.size() == 2 && k[0].zrodlo == "L-11" && k[1].zrodlo == "awaria");
  sprawy::zapiszPokazanie(r, kolejka::kluczPokazaniaLinii(k[0]), "2026-11-04");
  k = kol("2026-11-07", s2, &r, 9 * 60, nullptr, nullptr, "Pamięć panelu: awaria.");
  SPRAWDZ(k.size() == 2 && k[0].zrodlo == "awaria");  // awarii wczoraj nie było na ekranie
  // R-INF-10: pierwszy dzień okresu, potem najwyżej raz na 3 dni od pokazania
  reguly::Pamiec f;
  const std::string INF = "Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij pomoc.";
  k = kol("2026-11-10", pusto, &f, 9 * 60, nullptr, nullptr, "", "2026-11-10");
  SPRAWDZ(maLinie(k, INF));
  sprawy::zapiszPokazanie(f, "linia:R-INF-10", "2026-11-10");
  SPRAWDZ(!maLinie(kol("2026-11-11", pusto, &f, 9 * 60, nullptr, nullptr, "", "2026-11-10"), INF));
  SPRAWDZ(maLinie(kol("2026-11-13", pusto, &f, 9 * 60, nullptr, nullptr, "", "2026-11-10"), INF));
  SPRAWDZ(!maLinie(kol("2026-11-13", pusto, &f, 9 * 60), INF));  // bez okresu infekcji linii nie ma
}

// ---------- slot wieczorny i tygodniowy (R-KON-01, R-KON-08, R-KON-09, R-KON-17) ----------
static void testOknoWieczoru() {
  using namespace slot_wieczorny;
  ROWNE_L(oknoZeStartow({}), 21 * 60);              // R-KON-17: bez startów okno stałe
  ROWNE_L(oknoZeStartow({670, 670}), 21 * 60);      // 2 noce: dalej okno stałe
  ROWNE_L(oknoZeStartow({670, 670, 670}), 21 * 60 + 40);  // mediana 23:10 → 21:40
  ROWNE_L(oknoZeStartow({700, 710, 720, 730, 750}), 22 * 60 + 30);  // mediana 00:00 → 22:30 (przykład R-KON-09)
  ROWNE_L(oknoZeStartow({750, 750, 750}), 23 * 60);  // mediana 00:30 → 23:00
  ROWNE_L(oknoZeStartow({810, 810, 810}), -1);        // mediana 01:30 → okno puste
  ROWNE_L(oknoZeStartow({480, 480, 480}), 19 * 60);   // mediana 20:00 → od 19:00
  ROWNE_L(oknoZeStartow({600, -1, 600, -1, 600}), 20 * 60 + 30);  // start nieznany pominięty (fail-path)
  SPRAWDZ(wOknie(21 * 60, 23 * 60 + 10) && !wOknie(21 * 60, 23 * 60 + 30) && !wOknie(-1, 22 * 60));
  SPRAWDZ(slotTygodniowy("2026-11-08", 17 * 60) && !slotTygodniowy("2026-11-08", 19 * 60) &&
          !slotTygodniowy("2026-11-07", 17 * 60));
  // plan: ocena co 30 min w oknie do 23:30, tylko przy źródle otoczenia; niedziela 17:00 bez Wi-Fi
  WejsciePlanu w;
  w.zegar = true;
  w.nocKompletna = true;
  w.sprawdzenie11 = true;
  w.zrodloOtoczenia = true;
  w.wieczorOd = 21 * 60;
  for (int m = 20 * 60; m < 24 * 60; m += 30) {
    w.sekundaDoby = m * 60;
    w.obsluzoneZnane = true;
    w.obsluzoneDo = m * 60 - 1;
    bool ocena = (powodyTeraz(w) & POWOD_SLOT_WIECZORNY) != 0;
    SPRAWDZ(ocena == (m >= 21 * 60 && m < 23 * 60 + 30));
  }
  w.sekundaDoby = 22 * 60 * 60;
  w.obsluzoneDo = w.sekundaDoby - 1;
  SPRAWDZ(sieciowy(powodyTeraz(w)) && tylkoStatus(powodyTeraz(w)));  // Wi-Fi tylko na status i otoczenie
  w.zrodloOtoczenia = false;
  SPRAWDZ((powodyTeraz(w) & POWOD_SLOT_WIECZORNY) == 0);
  w.niedziela = true;
  w.sekundaDoby = 17 * 3600;
  w.obsluzoneDo = w.sekundaDoby - 1;
  SPRAWDZ((powodyTeraz(w) & POWOD_SLOT_TYGODNIOWY) != 0 && !sieciowy(POWOD_SLOT_TYGODNIOWY));
  SPRAWDZ(nazwyPowodow(POWOD_SLOT_WIECZORNY | POWOD_SLOT_TYGODNIOWY) == "slot_wieczorny+slot_tygodniowy");
}

// R-KON-01 w dyrygencie: CO2 1200 ppm dopiero od 22:20 → S-K-01 przy ocenie o 22:30, nie wcześniej
static void odpWieczor(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  s.siec.siecJest = true;
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_B, DOBA_B));
  s.siec.dodaj(sciezkaNocy(DOBA_B), 200, raport(DOBA_B, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40)));
  int c = t >= ep(2026, 11, 4, 22, 20) ? 1200 : 700;
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":" + std::to_string(c) + "},\"ostrzezenia\":[]}");
}
static void testWieczorCo30() {
  KonfiguracjaPanelu k = KONF;
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 6, 0), ep(2026, 11, 4, 23, 20), odpWieczor, false, k);
  int ocen = 0;
  for (auto& w : v)
    for (auto& z : w.zdarzenia) ocen += z.find("slot_wieczorny") != std::string::npos && w.minuta >= 21 * 60;
  SPRAWDZ(ocen >= 4);  // 21:00, 21:30, 22:00, 22:30, 23:00
  // ADR-0035 pkt 1: sloty wieczorne tylko ze statusem (R-KON-01) nie pytają o STR.edf
  int strWieczorem = 0;
  for (auto& w : v)
    if (w.minuta >= 21 * 60) strWieczorem += zadanZeSciezka(w, "/api/sessions") + zadanZeSciezka(w, "/api/storage");
  SPRAWDZ(strWieczorem == 0);
  SPRAWDZ(!logMa(v, "ekran wieczorny SK01", 0, 22 * 60 + 30));
  SPRAWDZ(logMa(v, "ekran wieczorny SK01", 22 * 60 + 30, 22 * 60 + 35));
}

// R1: rano (P95 12, domyślne ustawienie 10) wariant opisowy S-K-10 `Wyciek widoczny` (typ SK10W, R8) nie jest
// prośbą dnia (R-PIE-10) i nie „zostaje cały dzień”: grupaRano("SK10W") == 8, a wieczorem S-K-01 wchodzi jak zwykle
static void odpWieczorWyciek(PanelSym& s, uint32_t t) {
  odpWieczor(s, t);
  for (auto& kv : s.siec.odpowiedzi)
    for (auto& a : kv.second) {
      size_t i = a.o.cialo.find("\"p95\":9.6");  // wyciek P95 nocy 12 L/min (od ustawienia 10 do < 15: wariant opisowy)
      if (i == std::string::npos) continue;
      a.o.cialo.replace(i, 9, "\"p95\":12.0");
      a.o.contentLength = a.o.wynikZapisu = (int)a.o.cialo.size();
    }
}
static void testWieczorPoSk10Opisowym() {
  SPRAWDZ(grupaRano("SK10W") == 8 && !zostajeRano("SK10W", false));
  KonfiguracjaPanelu k = KONF;
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 6, 0), ep(2026, 11, 4, 23, 20), odpWieczorWyciek, false, k);
  SPRAWDZ(logMa(v, "ekran S-K-10 (R8): ekran danych", 6 * 60, 12 * 60));
  SPRAWDZ(logMa(v, "ekran wieczorny SK01", 22 * 60 + 30, 22 * 60 + 35));
}

// Niedziela 8.11: o 17:00 wybudzenie slotu tygodniowego i pytanie o senność (R-SKI-16, R-KON-08)
static const long DOBA_N = nrDnia("2026-11-07");  // noc z poranka 8.11
static void odpNiedziela(PanelSym& s, uint32_t t) {
  (void)t;
  s.siec.odpowiedzi.clear();
  s.siec.siecJest = true;
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_N, DOBA_N));
  s.siec.dodaj(sciezkaNocy(DOBA_N), 200, raport(DOBA_N, ep(2026, 11, 7, 23, 20), ep(2026, 11, 8, 6, 40)));
}
static void testNiedziela() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 8, 6, 0), ep(2026, 11, 8, 18, 0), odpNiedziela);
  bool o17 = false;
  for (auto& w : v) o17 = o17 || (w.minuta == 17 * 60 && w.powod == PowodWybudzenia::ZEGAR);
  SPRAWDZ(o17);
  SPRAWDZ(logMa(v, "slot_tygodniowy", 17 * 60, 17 * 60 + 1));
  SPRAWDZ(logMa(v, "ekran dnia: pytanie sennosc", 17 * 60, 17 * 60 + 1));
  SPRAWDZ(!logMa(v, "ekran dnia: pytanie sennosc", 0, 17 * 60));
}

// ---------- „Omówione” według stanu wzorca (5.12.7, R-LEK-24…27) i wariant blokady ----------
static void testOmowione() {
  using reguly::Pilnosc;
  sprawy::SprawaL a = sprawaL("L-06/b", Pilnosc::A, "2026-11-01");
  a.omowione = "2026-11-02";
  SPRAWDZ(sprawy::omowioneAktywne(a, "2026-11-20"));  // bez stanu 56 dób
  a.stanWzorca = "aktywny";  // stan wzorca zakończył wyciszenie (zmiana ustawień, 28 nocy po drugim potwierdzeniu, Δ)
  SPRAWDZ(!sprawy::omowioneAktywne(a, "2026-11-20"));
  SPRAWDZ(sprawy::omowioneAktywne(a, "2026-11-02"));  // w dniu potwierdzenia stan wzorca jeszcze go nie zna
  a.stanWzorca = "omowione";
  SPRAWDZ(sprawy::omowioneAktywne(a, "2026-12-20") == true);  // stan wzorca trwa (koniec liczy src/wzorce.cpp)
  SPRAWDZ(!sprawy::pelnyEkranDzis(a, "2026-11-20"));
  a.pelnyMimoOmowienia = true;  // R-LEK-27: dzień pełnego ekranu co 14 nocy
  SPRAWDZ(sprawy::pelnyEkranDzis(a, "2026-11-20"));
  // „wiem”: powrót przed czasem według stanu wzorca
  sprawy::SprawaL b = sprawaL("L-11", Pilnosc::B, "2026-11-01");
  b.wiem = "2026-11-03";
  SPRAWDZ(sprawy::wiemAktywne(b, "2026-11-05"));
  b.stanWzorca = "aktywny";
  SPRAWDZ(!sprawy::wiemAktywne(b, "2026-11-05") && sprawy::wiemAktywne(b, "2026-11-03"));
  // warianty zdań Q-omowione (5.12.7) i blokada R-LEK-26
  SPRAWDZ(zdaniaOmowienia(1)[0] == "Panel przestanie o tym przypominać na 8 tygodni albo do zmiany ustawień.");
  SPRAWDZ(zdaniaOmowienia(2)[0] == "Pełny ekran zniknie na 8 tygodni, przypomnienie co 3 dni zostanie.");
  SPRAWDZ(zdaniaOmowienia(3)[0] == "Drugi raz bez poprawy: pełny ekran zniknie na 4 tygodnie.");
  SPRAWDZ(zdaniaOmowienia(4)[0] == "Znowu bez poprawy: pełny ekran co 2 tygodnie.");
  SPRAWDZ(zdaniaOmowienia(5).size() == 1 && zdaniaOmowienia(5)[0] == "Najpierw nowy raport: ▶ 2 s na ekranie L.");
  DaneOmowione o = trescOmowione(a, "2026-11-20", 5);
  SPRAWDZ(o.blokada && o.zdania.size() == 1);
  SPRAWDZ(!trescOmowione(a, "2026-11-20", 3).blokada);
  // ADR-0002: żadne zdanie nie podaje wartości ani drogi zmiany ustawień
  for (int w = 1; w <= 5; w++)
    for (auto& z : zdaniaOmowienia(w)) SPRAWDZ(z.find("cmH2O") == std::string::npos && z.find("EPR") == std::string::npos);
  KontekstNaw k;
  k.omowienieZablokowane = true;
  Przejscie pr = nacisniecie(Ekran::OMOWIONE, Przycisk::PRAWY, false, k);
  SPRAWDZ(pr.ekran == Ekran::OMOWIONE && pr.akcja == Akcja::NIC);
  pr = nacisniecie(Ekran::OMOWIONE, Przycisk::LEWY, false, k);
  SPRAWDZ(pr.ekran == Ekran::DZIEN);
  k.omowienieZablokowane = false;
  SPRAWDZ(nacisniecie(Ekran::OMOWIONE, Przycisk::PRAWY, false, k).akcja == Akcja::OMOWIONE_TAK);
  // „Otwarte sprawy”: wiersze `Dalej:` stanów wzorca (R-EKR-49)
  std::vector<sprawy::SprawaL> sp = {sprawaL("L-11", Pilnosc::B, "2026-10-01"), sprawaL("L-03", Pilnosc::B, "2026-10-01"),
                                     sprawaL("L-01", Pilnosc::B, "2026-10-01")};
  sp[0].stanWzorca = "wygaszony_b";
  sp[1].stanWzorca = "wstrzymany";
  sp[2].stanWzorca = "karencja";
  sprawy::WejscieSpraw ws;
  ws.dzis = "2026-11-04";
  ws.sprawy = &sp;
  std::vector<sprawy::KartaSprawy> ks = sprawy::kartySpraw(ws);
  SPRAWDZ(ks.size() == 3 && ks[0].dalej == "przypomnę 1. dnia miesiąca, obserwacja trwa" &&
          ks[1].dalej == "wstrzymane do końca okresu infekcji" && ks[2].dalej == "czekam: ekran po 14 nocach trwania");
}

// ---------- linia E-10 na ekranie dnia (także S6), wybór C zapamiętany ----------
struct PlotnoPamiec : Plotno {
  std::vector<uint8_t> b = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) b[(size_t)(y * 800 + x)] = c;
  }
};
static void testLiniaE10NaEkranie() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 5, 0), ep(2026, 11, 4, 11, 20), odpNocWDrodze);
  SPRAWDZ(logMa(v, "linia uwagi: Noc 04.11 bez terapii.", 11 * 60, 11 * 60 + 5));
  // ekran danych S6 z tą linią różni się od S6 bez linii w strefie linii (nad stopką), więc linia stoi na ekranie
  std::vector<Noc> h(1);
  h[0].data = "2026-11-03";
  h[0].m01 = 420;
  h[0].wazna = true;
  DaneDnia d;
  d.dzis = "2026-11-04";
  d.hist = &h;
  TrescEkranu bez = trescDnia("S6", "", d);
  d.linia = "Noc 04.11 bez terapii.";
  TrescEkranu z = trescDnia("S6", "", d);
  SPRAWDZ(z.linia == "Noc 04.11 bez terapii." && bez.linia.empty());
  PlotnoPamiec a, b;
  rysujEkranDanych(a, {"śr 04.11", ""}, bez);
  rysujEkranDanych(b, {"śr 04.11", ""}, z);
  int rozne = 0;
  for (int y = 0; y < 480; y++)
    for (int x = 0; x < 800; x++) rozne += a.b[(size_t)(y * 800 + x)] != b.b[(size_t)(y * 800 + x)];
  if (rozne <= 200) printf("rozne %d\n", rozne);
  SPRAWDZ(rozne > 200);
  // kolejka: linia E-10 w grupie „noc”, E-04 w „danych” (7.5)
  std::vector<sprawy::LiniaZdarzenia> le = {{"E-10/linia", "noc", "Noc 04.11 bez terapii."},
                                            {"E-04", "dane", "Noc 04.11 zapisana częściowo, liczby mogą być niepełne."}};
  kolejka::WejscieKolejki w;
  w.dzis = "2026-11-04";
  w.linieE = &le;
  auto k = kolejka::kolejkaLinii(w);
  SPRAWDZ(k.size() == 2 && k[0].grupa == GrupaLinii::DANE && k[1].grupa == GrupaLinii::NOC &&
          k[1].tekst == "Noc 04.11 bez terapii.");
}
static void testWyborC() {
  using reguly::Pilnosc;
  std::vector<sprawy::SprawaL> s = {sprawaL("L-08", Pilnosc::C, "2026-10-01"), sprawaL("L-07", Pilnosc::C, "2026-10-01")};
  reguly::Pamiec p;
  // obie pokazane 1.11, a panel wybrał wtedy L-07 (zapis `linia:C`): 8.11 L-08
  sprawy::zapiszPokazanie(p, "linia:L-08", "2026-11-01");
  sprawy::zapiszPokazanie(p, "linia:L-07", "2026-11-01");
  p[sprawy::kluczPokazania("linia:C")].ostatnio = "2026-11-01";
  p[sprawy::kluczPokazania("linia:C")].stanWzorca = "L-07";
  auto k = kol("2026-11-08", s, &p, 17 * 60);
  SPRAWDZ(k.size() == 1 && k[0].zrodlo == "L-08");
  sprawy::zapiszPokazanie(p, "linia:L-08", "2026-11-08");
  SPRAWDZ(kol("2026-11-08", s, &p, 18 * 60)[0].zrodlo == "L-08");  // ten sam wybór do końca dnia
}

// ---------- M-18 w rekordzie nocy i okno wieczorne z R-KON-09 ----------
static void testM18() {
  // 23:20 wieczoru 3.11 (czas lokalny) dla nocy z poranka 4.11 → 680; 00:30 → 750; 13:00 dnia poranka → poza nocą
  SPRAWDZ(m18ZChwili(ep(2026, 11, 3, 23, 20), "2026-11-04") == 680);
  SPRAWDZ(m18ZChwili(ep(2026, 11, 4, 0, 30), "2026-11-04") == 750);
  SPRAWDZ(m18ZChwili(ep(2026, 11, 4, 13, 0), "2026-11-04") == -1);
  SPRAWDZ(m18ZChwili(0, "2026-11-04") == -1);
  // raport AirBridge: start pierwszej sesji
  NocAB n = nocZRaportu(raport(DOBA_B, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40)), DOBA_B, "ASVAuto");
  SPRAWDZ(n.noc.m18 == 680);
  // wersja 3: zapis i odczyt, wiersz wersji 2 bez M-18
  Noc r;
  SPRAWDZ(parsujNocV3(wierszNocyV3(n.noc), r) && r.m18 == 680);
  { std::string wv = wierszNocyV3(n.noc); SPRAWDZ((size_t)std::count(wv.begin(), wv.end(), ';') + 1 == POLA_NOCY_V3); }
  Noc q;
  SPRAWDZ(parsujNocV3(wierszNocyV2(n.noc), q) && q.m18 == -1);
  // okno z historii: 3 noce z terapią ze startem 23:10 → 21:40; noc bez startu się nie liczy
  std::vector<Noc> h;
  for (int i = 0; i < 4; i++) {
    Noc x;
    x.data = dodajDni("2026-11-04", -3 + i);
    x.m01 = 420;
    x.m18 = i == 0 ? -1 : 670;
    h.push_back(x);
  }
  ROWNE_L(slot_wieczorny::oknoZHistorii(h, "2026-11-04"), 21 * 60 + 40);
  h[1].m18 = -1;  // zostają 2 noce ze startem: okno stałe (R-KON-17)
  ROWNE_L(slot_wieczorny::oknoZHistorii(h, "2026-11-04"), 21 * 60);
  // noc bez terapii (M-01 < 15 min) nie liczy się
  h[1].m18 = 670;
  h[3].m01 = 5;
  ROWNE_L(slot_wieczorny::oknoZHistorii(h, "2026-11-04"), 21 * 60);
}

int main() {
  (void)zadanZeSciezka;
  (void)STATUS_BLOKADA;
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testStanNocy();
  testNocWDrodze();
  testNoceBezDanych();
  testTrzeciaNoc();
  testRytmEkranuL();
  testLinieB11();
  testOknoWieczoru();
  testWieczorCo30();
  testWieczorPoSk10Opisowym();
  testNiedziela();
  testOmowione();
  testLiniaE10NaEkranie();
  testWyborC();
  testM18();
  printf("testy_stany_nocy_sloty_wieczorne: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
