// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (stany nocy, sloty wieczorny i tygodniowy). Testy niczego w kodzie nie naprawiają. Pomocnicze funkcje
// symulacji doby skopiowane z test/testy_stany_nocy_sloty_wieczorne.cpp, żeby nie zależały od jego testów.
// Uruchomienie: tools/prototyp/testy.sh (program testy_stany_nocy_sloty_wieczorne_brzegowe).
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
#include "slot_wieczorny.h"
#include "stan_nocy.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ_N(n, w)                                                  \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s (linia %d): %s\n", n, __LINE__, #w); } \
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

static sprawy::SprawaL sprawaL(const std::string& kl, reguly::Pilnosc p, const std::string& od, const std::string& tytul) {
  sprawy::SprawaL x;
  x.klucz = x.id = kl;
  x.stan = reguly::StanReguly::AKTYWNA;
  x.pilnosc = p;
  x.ton = sprawy::Ton::LEKARZ;
  x.tytul = tytul;
  x.linia = kl + ": linia C. Na kontroli: raport 2 / 3.";
  x.aktywnaOd = od;
  return x;
}
static std::vector<kolejka::LiniaUwagi> kol(const std::string& dzis, const std::vector<sprawy::SprawaL>& s,
                                          const reguly::Pamiec* p, int minuta) {
  kolejka::WejscieKolejki w;
  w.dzis = dzis;
  w.sprawy = &s;
  w.pamiec = p;
  w.minuta = minuta;
  return kolejka::kolejkaLinii(w);
}
static bool maLinie(const std::vector<kolejka::LiniaUwagi>& k, const std::string& t) {
  for (auto& l : k)
    if (l.tekst == t) return true;
  return false;
}

// (R-SKI-41, R-SKI-12): linia C wybrana w niedzielę po pierwszym pokazaniu przeskakuje przy kolejnym
// przeliczeniu kolejki tej samej niedzieli na wzorzec „nigdy niepokazany”, więc obie linie C stają w jedną niedzielę
// i rotacja z przykładu R-SKI-41 (niedziela 1: L-08, niedziela 2: L-07) się rozsypuje.
static void rotacjaCTejSamejNiedzieli() {
  using reguly::Pilnosc;
  std::vector<sprawy::SprawaL> s = {sprawaL("L-08", Pilnosc::C, "2026-10-01", "Przerwy w nocy"),
                                    sprawaL("L-07", Pilnosc::C, "2026-10-01", "Ciśnienie w szczycie")};
  reguly::Pamiec p;
  const std::string N1 = "2026-11-01";
  auto k = kol(N1, s, &p, 17 * 60);
  SPRAWDZ_N("linia C wybrana w niedzielę po pierwszym pokazaniu", k.size() == 1 && k[0].zrodlo == "L-08");
  if (k.empty()) return;
  sprawy::zapiszPokazanie(p, kolejka::kluczPokazaniaLinii(k[0]), N1);  // linia stanęła na ekranie o 17:00
  k = kol(N1, s, &p, 18 * 60);  // ta sama niedziela, ocena slotu dziennego o 18:00 przerysowuje ekran dnia
  SPRAWDZ_N("linia C wybrana w niedzielę po pierwszym pokazaniu", k.size() == 1 && k[0].zrodlo == "L-08");
  // gdy panel jednak pokaże L-07 tego samego dnia, obie są „pokazane” w N1 i w N2 znów wygrywa L-08 (remis),
  // zamiast L-07 z przykładu R-SKI-41
  sprawy::zapiszPokazanie(p, "linia:L-07", N1);
  k = kol("2026-11-08", s, &p, 17 * 60);
  SPRAWDZ_N("linia C wybrana w niedzielę po pierwszym pokazaniu", k.size() == 1 && k[0].zrodlo == "L-07");
}

// (R-LEK-24, R-LEK-27, R-PIE-08): linia `Wciąż: {tytuł}. Omówione {data}.` liczona jako
// dniMiedzy(omowione) % 3 == 0, a nie od rzeczywistego pokazania. Przykład R-LEK-24: „Omówione” 10.11 → linia 11.11,
// 14.11, 17.11; kod daje 13.11, 16.11. Linia, która przegra 14.11, ma wrócić 15.11 (R-PIE-08), a nie 16.11.
static void rytmLiniiPoOmowieniu() {
  using reguly::Pilnosc;
  sprawy::SprawaL a = sprawaL("L-06/b", Pilnosc::A, "2026-11-01", "Saturacja poniżej 88%");
  a.omowione = "2026-11-10";
  a.stanWzorca = "omowione";
  std::vector<sprawy::SprawaL> s = {a};
  const std::string L = "Wciąż: Saturacja poniżej 88%. Omówione 10.11.";
  reguly::Pamiec p;
  SPRAWDZ_N("rytm linii „Wciąż” po omówieniu", maLinie(kol("2026-11-11", s, &p, 9 * 60), L));   // przykład R-LEK-24: 11.11
  sprawy::zapiszPokazanie(p, "linia:L-06/b", "2026-11-11");
  SPRAWDZ_N("rytm linii „Wciąż” po omówieniu", !maLinie(kol("2026-11-13", s, &p, 9 * 60), L));  // 13.11 bez linii
  SPRAWDZ_N("rytm linii „Wciąż” po omówieniu", maLinie(kol("2026-11-14", s, &p, 9 * 60), L));   // 14.11
  // 14.11 linia przegrała (nie stanęła na ekranie) → 15.11 ma wrócić (R-PIE-08)
  SPRAWDZ_N("rytm linii „Wciąż” po omówieniu", maLinie(kol("2026-11-15", s, &p, 9 * 60), L));
}

// (R-KON-42 i zakres slotu wieczornego „Wi-Fi tylko na /api/status i źródło otoczenia”): ocena slotu
// wieczornego idzie gałęzią „tylko status” slotu dziennego i woła drzemkaB9(): krótka sesja wieczorna przed snem
// (maska 22:05–22:20, o 22:30 `rop` = 0) daje dodatkowe żądanie `/api/report` i zapis „drzemki” (R-KON-57 to slot
// dzienny), choć wieczorem nie ma drzemki.
static const long DOBA_W = nrDnia("2026-11-03");  // noc z poranka 4.11
static void odpWieczorSesja(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  s.siec.siecJest = true;
  if (t >= ep(2026, 11, 4, 22, 25))
    s.siec.dodaj("/api/status", 200,
                 "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\","
                 "\"system\":\"IDLE\",\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"20261104_220500\","
                 "\"report_state\":\"idle\"}");
  else
    s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_W, DOBA_W));
  s.siec.dodaj(sciezkaNocy(DOBA_W), 200, raport(DOBA_W, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40)));
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":700},\"ostrzezenia\":[]}");
}
static void wieczorBezDrzemki() {
  KonfiguracjaPanelu k = KONF;
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 6, 0), ep(2026, 11, 4, 23, 20), odpWieczorSesja, false, k);
  int raporty = 0, ocen = 0;
  for (auto& w : v)
    if (w.minuta > 21 * 60 + 15 && w.minuta < 23 * 60 + 25) {
      raporty += zadanZeSciezka(w, "/api/report");
      ocen++;
    }
  SPRAWDZ_N("ocena slotu", ocen >= 3);  // oceny 21:30, 22:00, 22:30, 23:00 są
  SPRAWDZ_N("ocena slotu", raporty == 0);
  SPRAWDZ_N("ocena slotu", !logMa(v, "drzemka:", 21 * 60, 24 * 60));
}

// (R-KON-01, przykład: okno 21:10–23:30 → oceny 21:10, 21:40, …): powód `slot_wieczorny` stoi na siatce
// :00/:30, a nie co 30 min od początku okna. Dziś bez skutku (okno zawsze 21:00, rekord bez M-18), ale plan.h
// obiecuje okno z oknoZeStartow().
static void rytmOdPoczatkuOkna() {
  WejsciePlanu w;
  w.zegar = true;
  w.nocKompletna = true;
  w.sprawdzenie11 = true;
  w.zrodloOtoczenia = true;
  w.wieczorOd = 21 * 60 + 10;
  for (int m : {21 * 60 + 10, 21 * 60 + 40, 22 * 60 + 10}) {
    w.sekundaDoby = m * 60;
    w.obsluzoneZnane = true;
    w.obsluzoneDo = m * 60 - 1;
    SPRAWDZ_N("powód `slot_wieczorny` stoi na siatce", (powodyTeraz(w) & POWOD_SLOT_WIECZORNY) != 0);
  }
}

// Sprawdzenie „trzyma” (bez numeru znaleziska): noc w drodze z zanikiem zasilania o 14:05 (START, RTC wyzerowane,
// NVS i pliki zostają) dalej dostaje rekord „częściowa” dopiero o 23:30 (R-DANE-25, NVS `sn_noc`).
static void odpNocWDrodze(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  s.siec.siecJest = true;
  uint32_t pojawia = ep(2026, 11, 4, 12, 40);
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaNocy(DOBA_W - 1), 200, raport(DOBA_W - 1, ep(2026, 11, 2, 23, 0), ep(2026, 11, 3, 6, 30)));
  if (t < pojawia) {
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_W - 1, DOBA_W - 1));
  } else {
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_W - 1, DOBA_W));
    s.siec.dodaj(sciezkaNocy(DOBA_W), 200, czesciowy(raport(DOBA_W, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40))));
  }
}
static void trzyma_restartWBudowie() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 5, 0), ep(2026, 11, 4, 14, 0), odpNocWDrodze);
  std::vector<Wyb> v2 = symuluj(s, ep(2026, 11, 4, 14, 5), ep(2026, 11, 4, 23, 50), odpNocWDrodze);
  SPRAWDZ_N("trzyma: restart", logMa(v, "bez_terapii → w_budowie", 12 * 60 + 40, 13 * 60 + 15));
  SPRAWDZ_N("trzyma: restart", !logMa(v2, "(częściowa)", 0, 23 * 60 + 30));
  SPRAWDZ_N("trzyma: restart", logMa(v2, "(częściowa)", 23 * 60 + 30, 23 * 60 + 35));
  // (R-DANE-20, „E-10 o 11:00”): stan „bez terapii” daje sygnał `noc_bez_rekordu` i E-10/linia jest
  // w silniku spełniona, ale kolejka (src/kolejka.cpp) nie ma linii zdarzeń grupy „noc”: o 11:00 ekran dnia S6 bez
  // linii `Noc … bez terapii.`, więc pacjent nie widzi, że noc była bez terapii
  SPRAWDZ_N("trzyma_restartWBudowie", logMa(v, "linia uwagi: Noc", 11 * 60, 11 * 60 + 5));
}

int main() {
  (void)STATUS_BLOKADA;
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  rotacjaCTejSamejNiedzieli();
  rytmLiniiPoOmowieniu();
  wieczorBezDrzemki();
  rytmOdPoczatkuOkna();
  trzyma_restartWBudowie();
  printf("testy_stany_nocy_sloty_wieczorne_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
