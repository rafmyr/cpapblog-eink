// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych: (A) ocena slotu wieczornego bez
// Wi-Fi u egzemplarza bez CO2, (B) obrazki dnia komendą `W`. Każdy test podaje numer R19-nn; „OCZEKUJE”
// = przypadek otwarty. Dane i obrazki syntetyczne.
// Pomocnicze funkcje symulacji skopiowane z test/testy_wieczor_bez_wifi_obrazki.cpp.
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

#include "magazyn.h"

#include "magazyn.h"

#define ROWNE_S(a, b)                                                                               \
  do {                                                                                              \
    std::string a_ = (a), b_ = (b);                                                                 \
    if (a_ == b_) ok++;                                                                             \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", ma być \"%s\"\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } \
  } while (0)

static int oczekuje = 0;
#define OCZEKUJE(id, w)                                                               \
  do {                                                                                \
    if (w) ok++;                                                                      \
    else { oczekuje++; printf("OCZEKUJE %s (%s:%d): %s\n", id, __FILE__, __LINE__, #w); } \
  } while (0)

static void odpKrotka(PanelSym& s, uint32_t) {
  std::string r = raport(DOBA_A, ep(2026, 11, 4, 5, 0), ep(2026, 11, 4, 5, 20));
  size_t i = r.find("\"used_seconds\":25200");
  r.replace(i, 20, "\"used_seconds\":1200");
  s.siec.odpowiedzi.clear();
  s.siec.siecJest = true;
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
  s.siec.dodaj(sciezkaNocy(DOBA_A), 200, r);
}
static void odpPelna(PanelSym& s, uint32_t) {
  s.siec.odpowiedzi.clear();
  s.siec.siecJest = true;
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
  s.siec.dodaj(sciezkaNocy(DOBA_A), 200, raport(DOBA_A, ep(2026, 11, 3, 23, 0), ep(2026, 11, 4, 6, 0)));
}

static void odpSpokojna(PanelSym& s, uint32_t t) {  // czujnik zmienny bez progów S-K-03…S-K-05
  odpPelna(s, t);
  s.p.czujniki.o.temp = 19.0f + (float)(minutaLok(t) / 60 % 4);
  s.p.czujniki.o.wilg = 45.0f + (float)(minutaLok(t) / 60 % 4);
}

struct Wieczor {
  int wybudzen = 0, zWifi = 0, odswiezen = 0, bezSieci = 0;
};
static Wieczor wieczor(const std::vector<Wyb>& v, int od = 21 * 60, int doM = 23 * 60 + 30) {
  Wieczor r;
  for (auto& w : v) {
    if (w.powod != PowodWybudzenia::ZEGAR || w.minuta < od || w.minuta >= doM) continue;
    r.wybudzen++;
    r.zWifi += w.m.polaczen > 0;
    r.odswiezen += w.m.odswiezen;
    for (auto& z : w.zdarzenia) r.bezSieci += z.find("slot_wieczorny_bez_sieci") != std::string::npos && z.find("wybudzenie") != std::string::npos;
  }
  return r;
}

// ---------- A ----------

// (A, bateria): egzemplarz testowy, doba bez naciśnięcia, czujnik w obudowie co godzinę inny (stopka „przy łóżku”).
// R-KON-01: „z odświeżeniem e-ink tylko przy zmianie ekranu”; R-EKR-60 ogranicza przerysowania samej stopki do
// co 2 h. Oczekiwane: w oknie 21:00–23:30 tyle odświeżeń, ile zmian ekranu (S-K-08 o 21:00: 1), nie co 30 min.
static void odswiezeniaPrzyZmiennejStopce() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 6, 0), ep(2026, 11, 4, 23, 20), odpKrotka, true);
  Wieczor w = wieczor(v);
  printf("  egzemplarz testowy, stopka zmienna: wieczorem %d wybudzeń, %d z Wi-Fi, %d odświeżeń e-ink\n", w.wybudzen, w.zWifi,
         w.odswiezen);
  SPRAWDZ(w.zWifi == 0);
  OCZEKUJE("egzemplarz testowy, doba bez naciśnięcia, czujnik w obudowie co godzinę", w.odswiezen <= 1);
  // ta sama doba ze spokojną nocą (bez S-K wieczorem): ile odświeżeń samej stopki. Po poprawce czujnik co
  // godzinę inny, ale w zakresie spokojnym (19–22 °C, 45–48%); przy `zmiennaTemp` z testu 15 °C + godzina wieczorem jest
  // 36 °C, czyli S-K-05 (wejście ekranu o 21:00 to prawdziwa zmiana, sprawdza ją pierwsza asercja wyżej)
  PanelSym q;
  std::vector<Wyb> u = symuluj(q, ep(2026, 11, 4, 6, 0), ep(2026, 11, 4, 23, 20), odpSpokojna);
  Wieczor x = wieczor(u);
  printf("  egzemplarz testowy, noc pełna, stopka zmienna: wieczorem %d wybudzeń, %d odświeżeń e-ink\n", x.wybudzen,
         x.odswiezen);
  SPRAWDZ(x.zWifi == 0);
  printf("  odświeżenia (noc pełna) o:");
  for (auto& y : u)
    if (y.minuta >= 19 * 60 && y.m.odswiezen) printf(" %02d:%02d", y.minuta / 60, y.minuta % 60);
  printf("\n");
  OCZEKUJE("egzemplarz testowy, doba bez naciśnięcia, czujnik w obudowie co godzinę", x.odswiezen == 0);
}

// (A): egzemplarz ze źródłem (CO2) w każdej minucie doby i przy różnych oknach: bez bitu bez sieci; tester: bit tylko
// w oknie, co 30 min, nigdy sieciowy; zbiory powodów sieciowych identyczne z i bez źródła poza bitami CO2/pogody.
static void planBezRegresuBezZrodla() {
  int zle = 0, poza = 0;
  for (int okno : {19 * 60, 20 * 60 + 15, 21 * 60, 21 * 60 + 10, 22 * 60 + 50, -1})
    for (int m = 0; m < 24 * 60; m++) {
      WejsciePlanu q;
      q.zegar = true;
      q.nocKompletna = true;
      q.sprawdzenie11 = true;
      q.wieczorOd = okno;
      q.sekundaDoby = m * 60;
      q.obsluzoneZnane = true;
      q.obsluzoneDo = q.sekundaDoby;
      q.zrodloOtoczenia = true;
      q.co2 = true;
      zle += (powodyTeraz(q) & POWOD_SLOT_WIECZORNY_BEZ_SIECI) != 0;
      q.zrodloOtoczenia = false;
      q.co2 = false;
      Powody p = powodyTeraz(q);
      if (p & POWOD_SLOT_WIECZORNY_BEZ_SIECI) {
        bool wOknie = okno >= 0 && m >= okno && m <= 23 * 60 + 30 && (m - okno) % 30 <= 1;  // tolerancja 60 s (R-NF-02)
        poza += !wOknie;
        poza += sieciowy(POWOD_SLOT_WIECZORNY_BEZ_SIECI);
      }
    }
  SPRAWDZ(zle == 0);
  SPRAWDZ(poza == 0);
}

// (A, R-DANE-18): egzemplarz testowy z nocą bez danych (ponowienia co 30 min do 23:30): wieczorne oceny bez Wi-Fi nie
// dokładają wybudzeń między ponowieniami (ten sam rytm), a 23:30 bez oceny.
static void ponowieniaIWieczor() {
  WejsciePlanu q;
  q.zegar = true;
  q.nocKompletna = false;
  q.sprawdzenie11 = true;
  q.nocyBezDanych = 1;
  q.zrodloOtoczenia = false;
  q.co2 = false;
  q.wieczorOd = 21 * 60 + 10;  // okno z R-KON-09 nie na pełnej półgodzinie
  q.sekundaDoby = 20 * 3600 + 5;
  q.obsluzoneZnane = true;
  q.obsluzoneDo = q.sekundaDoby;
  std::vector<int> mins, tylkoBez;
  for (int i = 0; i < 40; i++) {
    Termin t = nastepnyTermin(q);
    int m = (int)((q.sekundaDoby + t.sekund) / 60);
    if (m > 23 * 60 + 30 || t.sekund <= 0) break;
    mins.push_back(m);
    if (t.powody == POWOD_SLOT_WIECZORNY_BEZ_SIECI) tylkoBez.push_back(m);
    SPRAWDZ(!(m == 23 * 60 + 30 && (t.powody & POWOD_SLOT_WIECZORNY_BEZ_SIECI)));
    q.sekundaDoby = m * 60 + 5;
    q.obsluzoneDo = q.sekundaDoby;
  }
  printf("  egzemplarz testowy z ponowieniami, okno od 21:10: %zu wybudzeń 20:00–23:30, w tym %zu tylko dla oceny bez Wi-Fi:",
         mins.size(), tylkoBez.size());
  for (int m : mins) printf(" %02d:%02d", m / 60, m % 60);
  printf("\n");
  // informacyjnie: okno R-KON-09 przesunięte względem ponowień podwaja wybudzenia (koszt baterii poza 8.1?)
  SPRAWDZ(mins.size() <= 20);
}

// ---------- B ----------

static std::string pbmSyntetyczny(int w, int h) {
  std::string d = "P4\n" + std::to_string(w) + " " + std::to_string(h) + "\n";
  for (int y = 0; y < h; y++)
    for (int x = 0; x < (w + 7) / 8; x++) d += (char)((x + y) % 2 ? 0xAA : 0x55);
  return d;
}
static std::string komendaW(const std::string& nazwa, const std::string& tresc) {
  return "W " + nazwa + " " + std::to_string(tresc.size()) + " " + sha256Hex(tresc) + "\n" + tresc;
}
static void doPortu(Panel& p, const std::string& b) {
  for (char c : b) p.port.znaki.push_back((unsigned char)c);
}
static bool portMa(const Panel& p, const std::string& t) {
  for (auto& l : p.port.log)
    if (l == t) return true;
  return false;
}
static bool dzMa(const Panel& p, const std::string& t) {
  for (auto& l : p.d.z)
    if (l.find(t) != std::string::npos) return true;
  return false;
}
static void wybudzZegarem(Panel& p, int h, int m) {
  p.zegar.epoch = ep(2026, 11, 4, h, m);
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static void startZasilania(Panel& p, int h, int m) {
  p.zegar.epoch = ep(2026, 11, 4, h, m);
  p.zegar.msTeraz = 0;
  p.start.stan = StanRtc();
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static int plikowPoza(const Panel& p, const std::set<std::string>& przed) {
  int n = 0;
  for (auto& kv : p.pliki.pliki)
    if (!przed.count(kv.first) && kv.first.compare(0, 9, "/obrazki/") != 0 && kv.first.compare(0, 5, "/log/") != 0 &&
        kv.first.compare(0, 6, "/dane/") != 0)
      n++;
  return n;
}

// (B): nazwy złośliwe: NUL, CR, unicode, podwójny ukośnik, kropka, wielkie litery prefiksu, obcięcie do 64 znaków
// w parsujKomendeW() (k.nazwa.substr(0, 64)) nie robi z nazwy złej nazwy dobrej.
static void nazwyZlosliwe() {
  for (std::string z : {std::string("obrazki/a\0.pbm", 14), std::string("obrazki/a.pbm\r"), std::string("obrazki/\xC5\x82.pbm"),
                        std::string("obrazki//a.pbm"), std::string("obrazki/./a.pbm"), std::string("obrazki/a..pbm"),
                        std::string("OBRAZKI/a.pbm"), std::string("obrazki/a.pbm/"), std::string("obrazki/obrazki.json/"),
                        std::string("obrazki/obrazki.JSON"), std::string("obrazki/..\\noce.csv"),
                        std::string("obrazki/a.pbm\xEF\xBC\x8F"), std::string("obrazki/\xEF\xBD\x81.pbm"),
                        std::string("obrazki/obrazki.json.tmp"), std::string("obrazki/a.pbm.tmp"), std::string("obrazki")})
    SPRAWDZ(!nazwaObrazkaW(z));
  // obcięcie: każda nazwa dłuższa niż 64 znaki po obcięciu jest zła
  int zleObc = 0;
  for (int dl = 1; dl <= 80; dl++)
    for (const char* ogon : {".pbm", ".pbm/../../noce.csv", ".pbmxx", ".json"}) {
      std::string n = "obrazki/" + std::string(dl, 'a') + ogon;
      KomendaW k;
      parsujKomendeW("W " + n + " 1 " + sha256Hex("x"), k);
      bool dobra = nazwaObrazkaW(k.nazwa);
      bool oczek = dl <= 40 && std::string(ogon) == ".pbm";
      zleObc += dobra != oczek;
    }
  SPRAWDZ(zleObc == 0);
  // NUL w wierszu komendy przez port: nazwa odrzucona, nic nie zapisane
  Panel p;
  std::string b = pbmSyntetyczny(8, 1);
  std::string kom = std::string("W obrazki/a\0b.pbm ", 19) + std::to_string(b.size()) + " " + sha256Hex(b) + "\n" + b;
  doPortu(p, "?" + kom);
  wybudzZegarem(p, 14, 0);
  for (auto& kv : p.pliki.pliki) SPRAWDZ(kv.first.find('\0') == std::string::npos);
  SPRAWDZ(!p.pliki.pliki.count("/obrazki/a.pbm") && !p.pliki.pliki.count("/obrazki/ab.pbm"));
}

// (B): zerwany kabel w czuwaniu w trakcie treści 43 KB: `BLAD … rozmiar`, stary plik nietknięty, bez .tmp,
// panel zasypia; następna (pełna) komenda po przerwie przyjęta.
static void zerwanyKabel() {
  Panel p;
  p.pliki.pliki["/obrazki/001_las.pbm"] = "stary";
  std::string a = pbmSyntetyczny(800, 434);
  std::string kom = komendaW("obrazki/001_las.pbm", a);
  doPortu(p, "?" + kom.substr(0, kom.size() / 3));
  wybudzZegarem(p, 14, 0);
  SPRAWDZ(portMa(p, "BLAD obrazki/001_las.pbm rozmiar"));
  SPRAWDZ(p.pliki.tresc("/obrazki/001_las.pbm") == "stary");
  SPRAWDZ(!p.pliki.pliki.count("/obrazki/001_las.pbm.tmp"));
  SPRAWDZ(p.sen.snow == 1);
}

// (B, czuwanie): `?` plus treść bez komendy `O` nie otwiera okna; Wi-Fi o 14:00 z samego planu (ponowienie),
// więc porównanie z wybudzeniem bez portu. Nagłówek z rozmiarem ponad 64 KB (do 4 MB) i treść PBM z bajtami 'O' i 'Z': panel odpowiada
// `BLAD … rozmiar` bez czytania treści, więc treść płynie do pętli czuwania jako komendy. Oczekiwane: bez okna
// serwisowego Wi-Fi ('O'), bez zrzutu ('Z'), bez Wi-Fi.
static void trescPonadLimitJakoKomendy() {
  Panel p;
  std::string t(70000, '\x55');
  for (size_t i = 100; i < t.size(); i += 1000) { t[i] = 'O'; t[i + 1] = 'Z'; t[i + 2] = '\n'; t[i + 3] = 'K'; }
  doPortu(p, "?" + komendaW("obrazki/duzy.pbm", t));
  wybudzZegarem(p, 14, 0);
  SPRAWDZ(portMa(p, "BLAD obrazki/duzy.pbm rozmiar"));
  bool zrzut = portMa(p, "PLIKI_START");
  bool wifi = dzMa(p, "siec.okno") || p.start.stan.ekran == (uint8_t)Ekran::TEST;  // okno serwisowe z bajtu 'O'
  printf("  treść 70 000 B po `BLAD rozmiar` w czuwaniu: zrzut %s, Wi-Fi %s, ekran %s\n", zrzut ? "tak" : "nie",
         wifi ? "tak" : "nie", dzMa(p, "ota") || dzMa(p, "okno serwisowe") ? "okno serwisowe" : "-");
  OCZEKUJE("`?` plus treść bez komendy `O` nie otwiera okna", !zrzut);
  OCZEKUJE("`?` plus treść bez komendy `O` nie otwiera okna", !wifi);
  Panel bezPortu;
  wybudzZegarem(bezPortu, 14, 0);
  ROWNE_L(p.d.ile("siec.polacz"), bezPortu.d.ile("siec.polacz"));  // żadnego połączenia ponad plan tego wybudzenia
}

// (B, okno przywracania): ten sam nagłówek ponad 64 KB w oknie: treść z "\nK" nie może zakończyć okna.
static void trescPonadLimitWOknie() {
  Panel p;
  std::string t(70000, '\x55');
  for (size_t i = 100; i < t.size(); i += 1000) { t[i] = '\n'; t[i + 1] = 'K'; }
  doPortu(p, "?" + komendaW("obrazki/duzy.pbm", t));
  startZasilania(p, 7, 0);
  bool okno = dzMa(p, "okno przywracania do") || dzMa(p, "okno przywracania:");
  SPRAWDZ(okno);
  SPRAWDZ(portMa(p, "BLAD obrazki/duzy.pbm rozmiar"));
  bool k = dzMa(p, "zakończone komendą K");
  printf("  treść ponad 64 KB w oknie: okno zakończone komendą K z treści: %s\n", k ? "tak" : "nie");
  OCZEKUJE("ten sam nagłówek ponad 64 KB w oknie", !k);
}

// (B): zanik zasilania i pełna pamięć w trakcie zapisu: stary obrazek zostaje, nowy nie powstaje częściowy.
static void zanikIPelnaPamiec() {
  std::string a = pbmSyntetyczny(800, 434);
  {
    Panel p;
    p.pliki.pliki["/obrazki/001_las.pbm"] = "stary";
    p.pliki.wolneBajty = 1000;
    doPortu(p, "?" + komendaW("obrazki/001_las.pbm", a));
    wybudzZegarem(p, 14, 0);
    SPRAWDZ(portMa(p, "BLAD obrazki/001_las.pbm zapis"));
    SPRAWDZ(p.pliki.tresc("/obrazki/001_las.pbm") == "stary");
    SPRAWDZ(!p.pliki.pliki.count("/obrazki/001_las.pbm.tmp"));
  }
  {
    Panel p;
    p.pliki.pliki["/obrazki/001_las.pbm"] = "stary";
    p.pliki.zanikPoBajtach = 2000;
    doPortu(p, "?" + komendaW("obrazki/001_las.pbm", a));
    wybudzZegarem(p, 14, 0);
    SPRAWDZ(p.pliki.tresc("/obrazki/001_las.pbm") == "stary");
    // po „restarcie”: resztka .tmp nie jest obrazkiem (wybierzObrazek() bierze tylko `.pbm`)
    std::vector<std::string> l;
    for (auto& kv : p.pliki.pliki)
      if (kv.first.compare(0, 9, "/obrazki/") == 0) l.push_back(kv.first);
    std::vector<WpisObrazka> dz;
    KontekstObrazka ko;
    ko.dzis = "2026-11-04";
    WyborObrazka w = wybierzObrazek(l, nullptr, dz, ko, [](const std::string&) { return true; });
    SPRAWDZ(w.plik.empty() || w.plik == "/obrazki/001_las.pbm");
  }
}

// (B): PBM i manifest na granicy: 800×434 dokładnie, 0×0, 800×435, P1 tekstowy, dane krótsze, manifest pusty,
// manifest z pozycjami bez plików, manifest z `plik` ze ścieżką.
static void trescNaGranicy() {
  auto spr = [](const std::string& n, const std::string& t) {
    KomendaW k;
    parsujKomendeW("W " + n + " " + std::to_string(t.size()) + " " + sha256Hex(t), k);
    return sprawdzObrazekW(k, t);
  };
  ROWNE_S(spr("obrazki/a.pbm", pbmSyntetyczny(800, 434)), "");
  ROWNE_S(spr("obrazki/a.pbm", "P4\n0 0\n"), "tresc");
  ROWNE_S(spr("obrazki/a.pbm", pbmSyntetyczny(800, 435)), "tresc");
  ROWNE_S(spr("obrazki/a.pbm", "P1\n1 1\n1\n"), "tresc");
  std::string krotki = pbmSyntetyczny(800, 434);
  krotki.resize(krotki.size() - 1);
  ROWNE_S(spr("obrazki/a.pbm", krotki), "tresc");
  ROWNE_S(spr("obrazki/a.pbm", ""), "tresc");
  ROWNE_S(spr("obrazki/obrazki.json", ""), "tresc");
  // manifest pusty i manifest wskazujący nieistniejące pliki: panel przyjmuje (OK) …
  std::string pusty = "{\"wersja\":1,\"obrazki\":[]}";
  std::string obcy = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"nie_ma.pbm\",\"dni\":[],\"pogoda\":[],\"sezon\":[],\"inne\":[]},"
                     "{\"plik\":\"../../dane/noce/2026.csv\",\"dni\":[],\"pogoda\":[],\"sezon\":[],\"inne\":[]}]}";
  std::string sp = spr("obrazki/obrazki.json", pusty), so = spr("obrazki/obrazki.json", obcy);
  printf("  manifest pusty → \"%s\", manifest z pozycjami bez plików → \"%s\"\n", sp.c_str(), so.c_str());
  // … i wtedy obrazki z katalogu spoza manifestu nigdy się nie pokażą (R-KON-62 mówi o manifeście nieczytelnym)
  std::vector<std::string> l = {"/obrazki/001_las.pbm", "/obrazki/obrazki.json"};
  std::vector<PozycjaManifestu> m;
  SPRAWDZ(parsujManifest(obcy, m));
  std::vector<WpisObrazka> dz;
  KontekstObrazka ko;
  ko.dzis = "2026-11-04";
  WyborObrazka w = wybierzObrazek(l, &m, dz, ko, [](const std::string&) { return true; });
  printf("  manifest bez pozycji dla 001_las.pbm → wybór \"%s\"\n", w.plik.c_str());
  for (auto& x : w.log) printf("    log: %s\n", x.c_str());
  SPRAWDZ(w.plik.empty() || w.plik.compare(0, 9, "/obrazki/") == 0);  // nigdy poza katalogiem
  OCZEKUJE("PBM i manifest na granicy", !w.plik.empty() || !w.log.empty());  // brak obrazka bez śladu w logu
}

// (B, regresja T-WYM-87): okno przywracania na nowym panelu: obrazki przyjęte, okno trwa (bez `K`),
// po oknie znacznik „bez przywracania” (obrazki to nie próba); plik zrzutu w tym samym oknie dalej liczy się jako próba.
static void oknoPrzywracania() {
  std::string b = pbmSyntetyczny(320, 200);
  {
    Panel p;
    doPortu(p, "?" + komendaW("obrazki/002_morze.pbm", b) + "K");
    startZasilania(p, 7, 0);
    SPRAWDZ(portMa(p, "OK obrazki/002_morze.pbm"));
    SPRAWDZ(p.pliki.tresc("/obrazki/002_morze.pbm") == b);
    SPRAWDZ(p.nvs.tekst("cpap", NVS_KLUCZ_PRZYWR, "").compare(0, strlen(ZNACZNIK_BEZ_PRZYWRACANIA),
                                                              ZNACZNIK_BEZ_PRZYWRACANIA) == 0);
    for (auto& kv : p.pliki.pliki) SPRAWDZ(kv.first.compare(0, 14, "/przywracanie/") != 0);
  }
  {
    Panel p;
    const std::string noce = "2026-11-03;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;\n";
    doPortu(p, "?" + komendaW("obrazki/002_morze.pbm", b) + komendaW("noce.csv", noce) + komendaW("obrazki/obrazki.json", "{\"wersja\":1,\"obrazki\":[]}"));
    startZasilania(p, 7, 0);
    SPRAWDZ(portMa(p, "OK obrazki/002_morze.pbm"));
    SPRAWDZ(portMa(p, "OK noce.csv"));
    SPRAWDZ(portMa(p, "OK obrazki/obrazki.json"));
    SPRAWDZ(p.nvs.flaga("cpap", NVS_KLUCZ_PRZYWR_PROBA, false) || p.nvs.tekst("cpap", NVS_KLUCZ_PRZYWR, "").find(ZNACZNIK_BEZ_PRZYWRACANIA) == std::string::npos);
    SPRAWDZ(!p.pliki.pliki.count("/przywracanie/obrazki/002_morze.pbm"));
  }
  // poza oknem plik zrzutu dalej `BLAD … poza oknem`
  {
    Panel p;
    p.nvs.ustawTekst("cpap", NVS_KLUCZ_PRZYWR, "2026-11-01 40");
    doPortu(p, "?" + komendaW("noce.csv", "x\n") + komendaW("obrazki/002_morze.pbm", b));
    wybudzZegarem(p, 14, 0);
    SPRAWDZ(portMa(p, "BLAD noce.csv poza oknem"));
    SPRAWDZ(portMa(p, "OK obrazki/002_morze.pbm"));
  }
}

// (B): czuwanie przedłużone komendami nie rośnie bez końca bez komend: po ostatniej komendzie najwyżej 20 s
// (bateria), i nie dłużej niż 20 s przy samym `?` co 2 s przez 60 s, gdy nikt nie wysyła `W`.
static void czuwanieZKomendami() {
  Panel p;
  std::string b = pbmSyntetyczny(320, 200);
  doPortu(p, "?" + komendaW("obrazki/a.pbm", b) + komendaW("obrazki/b.pbm", b));
  wybudzZegarem(p, 14, 0);
  SPRAWDZ(portMa(p, "OK obrazki/a.pbm") && portMa(p, "OK obrazki/b.pbm"));
  Panel q;
  wybudzZegarem(q, 14, 0);
  printf("  czuwanie po dwóch komendach W: %u ms, bez komend: %u ms\n", (unsigned)p.zegar.msTeraz,
         (unsigned)q.zegar.msTeraz);
  SPRAWDZ(p.zegar.msTeraz <= q.zegar.msTeraz + CZUWANIE_MS + 2000);
}

int main() {
  (void)STATUS_BLOKADA;
  (void)logMa;
  (void)zadanZeSciezka;
  (void)plikowPoza;
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  odswiezeniaPrzyZmiennejStopce();
  planBezRegresuBezZrodla();
  ponowieniaIWieczor();
  nazwyZlosliwe();
  zerwanyKabel();
  trescPonadLimitJakoKomendy();
  trescPonadLimitWOknie();
  zanikIPelnaPamiec();
  trescNaGranicy();
  oknoPrzywracania();
  czuwanieZKomendami();
  printf("testy_wieczor_bez_wifi_obrazki_brzegowe: %d ok, %d błędów, %d oczekujących\n", ok, zle, oczekuje);
  return zle || oczekuje ? 1 : 0;
}
