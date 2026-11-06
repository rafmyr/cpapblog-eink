// SPDX-License-Identifier: GPL-3.0-or-later
// Testy (T-TEST-320…T-TEST-323): (A) wieczorna ocena S-K bez Wi-Fi na egzemplarzu bez CO2
// (R-KON-01, POWOD_SLOT_WIECZORNY_BEZ_SIECI), (B) obrazki dnia przez komendę `W` portu serwisowego (spec techniczna
// 8.6, src/magazyn.h nazwaObrazkaW()). Pomocnicze funkcje symulacji doby skopiowane z test/testy_ekrany_co2.cpp.
// Dane i obrazki syntetyczne. Uruchomienie: tools/prototyp/testy.sh (program testy_wieczor_bez_wifi_obrazki).
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

#include "magazyn.h"

#define ROWNE_S(a, b)                                                                               \
  do {                                                                                              \
    std::string a_ = (a), b_ = (b);                                                                 \
    if (a_ == b_) ok++;                                                                             \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", ma być \"%s\"\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } \
  } while (0)

// ---------- A. wieczór bez Wi-Fi (T-TEST-320, T-TEST-321) ----------

struct WieczorPlanu {
  std::vector<int> zWifi, bezWifi;  // minuty wybudzeń 17:00…23:30 (bez 23:30): z Wi-Fi i z oceną bez Wi-Fi
};
// Plan doby z nocą kompletną od 17:00 (jak testy_ekrany_co2.cpp wieczorneZWifi()), okno slotu od `okno`.
static WieczorPlanu wieczorPlanu(bool zrodlo, bool co2, int okno = 21 * 60) {
  WieczorPlanu r;
  WejsciePlanu q;
  q.zegar = true;
  q.nocKompletna = true;
  q.sprawdzenie11 = true;
  q.zrodloOtoczenia = zrodlo;
  q.co2 = co2;
  q.wieczorOd = okno;
  q.slotDzienny = false;
  q.sekundaDoby = 17 * 3600;
  q.obsluzoneZnane = true;
  q.obsluzoneDo = q.sekundaDoby;
  for (int i = 0; i < 100; i++) {
    Termin t = nastepnyTermin(q);
    int m = (q.sekundaDoby + (int)t.sekund) / 60;
    if (m >= 23 * 60 + 30 || t.sekund == 0) break;
    if (sieciowy(t.powody)) r.zWifi.push_back(m);
    if (t.powody & POWOD_SLOT_WIECZORNY_BEZ_SIECI) {
      r.bezWifi.push_back(m);
      SPRAWDZ((t.powody & POWOD_SLOT_WIECZORNY) == 0);  // nigdy oba warianty naraz
    }
    q.sekundaDoby = m * 60;
    q.obsluzoneDo = m * 60;
    // powody w chwili wybudzenia te same co w planie (dziennik wybudzeń, /log/wybudzenia.csv)
    SPRAWDZ((powodyTeraz(q) & POWOD_SLOT_WIECZORNY_BEZ_SIECI) == (t.powody & POWOD_SLOT_WIECZORNY_BEZ_SIECI));
  }
  return r;
}
static void testPlanWieczoruBezSieci() {
  const std::vector<int> CO30 = {21 * 60, 21 * 60 + 30, 22 * 60, 22 * 60 + 30, 23 * 60};
  // Egzemplarz ze źródłem (CO2): bez zmian, ocena z Wi-Fi co 30 min, bez wariantu bez sieci
  WieczorPlanu zeZrodlem = wieczorPlanu(true, true);
  SPRAWDZ(zeZrodlem.zWifi == CO30);
  SPRAWDZ(zeZrodlem.bezWifi.empty());
  // egzemplarz testowy (bez źródła): 0 wybudzeń z Wi-Fi, ocena bez Wi-Fi co 30 min od początku okna (przed żadnej)
  WieczorPlanu tester = wieczorPlanu(false, false);
  SPRAWDZ(tester.zWifi.empty());
  SPRAWDZ(tester.bezWifi == CO30);
  SPRAWDZ(wieczorPlanu(false, true).bezWifi == CO30);  // co2 bez źródła nic nie włącza (co2Wlaczone())
  // źródło z `co2=nie`: pogoda o 21:00 z Wi-Fi (prognoza do oceny S-K w pamięci), oceny bez Wi-Fi
  WieczorPlanu bezCo2 = wieczorPlanu(true, false);
  SPRAWDZ(bezCo2.zWifi == std::vector<int>({21 * 60}));
  SPRAWDZ(bezCo2.bezWifi == CO30);
  // okno z R-KON-09 od 21:10: oceny 21:10, 21:40, …, 23:10
  SPRAWDZ(wieczorPlanu(false, false, 21 * 60 + 10).bezWifi ==
          std::vector<int>({21 * 60 + 10, 21 * 60 + 40, 22 * 60 + 10, 22 * 60 + 40, 23 * 60 + 10}));
  // okno puste (-1): bez ocen
  SPRAWDZ(wieczorPlanu(false, false, -1).bezWifi.empty());
  // powód bez Wi-Fi: bez pobrania, nie pomiarowy (rysuje), nazwa w dzienniku
  SPRAWDZ(!sieciowy(POWOD_SLOT_WIECZORNY_BEZ_SIECI));
  SPRAWDZ(!pobranieZegarem(POWOD_SLOT_WIECZORNY_BEZ_SIECI).pobierz);
  SPRAWDZ(!wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, POWOD_SLOT_WIECZORNY_BEZ_SIECI | POWOD_CZUJNIKI));
  ROWNE_S(nazwyPowodow(POWOD_SLOT_WIECZORNY_BEZ_SIECI), "slot_wieczorny_bez_sieci");
  ROWNE_S(powodWpisuBaterii(PowodWybudzenia::ZEGAR, POWOD_CZUJNIKI | POWOD_SLOT_WIECZORNY_BEZ_SIECI),
          "zegar:czujniki+slot_wieczorny_bez_sieci");
}

// Noc 3/4.11 z 20 min terapii (M-01 < 30 min: S-K-08, R-KON-24, poza trybem próbnym), AirBridge odpowiada.
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
// T-TEST-321: egzemplarz testowy bez źródła otoczenia, doba bez naciśnięcia: S-K-08 wieczorem z wybudzenia zegarem bez Wi-Fi,
// wybudzenia co 30 min w oknie, e-ink tylko przy innej klatce.
static void testDobaTesteraS_K_08() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 6, 0), ep(2026, 11, 4, 23, 20), odpKrotka);
  std::vector<int> bez, zWifi;
  int odswiezen = 0, sk08 = -1;
  for (auto& w : v) {
    if (w.powod != PowodWybudzenia::ZEGAR || w.minuta < 21 * 60 || w.minuta >= 23 * 60 + 30) continue;
    if (w.m.polaczen > 0) zWifi.push_back(w.minuta);
    bool b = false;
    for (auto& z : w.zdarzenia) b = b || z.find("slot_wieczorny_bez_sieci") != std::string::npos;
    if (b) bez.push_back(w.minuta);
    odswiezen += w.m.odswiezen;
    for (auto& z : w.zdarzenia)
      if (sk08 < 0 && z.find("S-K-08") != std::string::npos) sk08 = w.minuta;
  }
  SPRAWDZ(zWifi.empty());  // egzemplarz testowy bez Wi-Fi wieczorem
  SPRAWDZ(bez == std::vector<int>({21 * 60, 21 * 60 + 30, 22 * 60, 22 * 60 + 30, 23 * 60}));
  ROWNE_L(sk08, 21 * 60);  // S-K-08 o 21:00 bez naciśnięcia (dopiero po naciśnięciu)
  ROWNE_L(odswiezen, 1);   // jedno odświeżenie e-ink (21:00), potem klatka bez zmian
  if (sk08 != 21 * 60 || odswiezen != 1)
    for (auto& w : v)
      if (w.minuta >= 20 * 60)
        for (auto& z : w.zdarzenia)
          if (z.find("ekran") != std::string::npos || z.find("wybudzenie") != std::string::npos)
            printf("  %02d:%02d %s\n", w.minuta / 60, w.minuta % 60, z.c_str());
}

// ---------- B. obrazki dnia komendą `W` (T-TEST-322, T-TEST-323) ----------

static std::string pbmSyntetyczny(int w, int h) {
  std::string d = "P4\n" + std::to_string(w) + " " + std::to_string(h) + "\n";
  for (int y = 0; y < h; y++)
    for (int x = 0; x < (w + 7) / 8; x++) d += (char)((x + y) % 2 ? 0xAA : 0x55);
  return d;
}
static std::string komendaW(const std::string& nazwa, const std::string& tresc) {
  return "W " + nazwa + " " + std::to_string(tresc.size()) + " " + sha256Hex(tresc) + "\n" + tresc;
}
static const char* MANIFEST = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_las.pbm\",\"dni\":[\"pn\"],\"pogoda\":[],"
                              "\"sezon\":[],\"inne\":[]}]}";

static void testNazwyIObrazki() {
  SPRAWDZ(nazwaObrazkaW("obrazki/001_las.pbm"));
  SPRAWDZ(nazwaObrazkaW("obrazki/a-b_9.pbm"));
  SPRAWDZ(nazwaObrazkaW("obrazki/obrazki.json"));
  for (const char* z : {"obrazki/../noce.csv", "obrazki/../x.pbm", "obrazki/a/b.pbm", "obrazki/Las.pbm",
                        "obrazki/las.PBM", "obrazki/.pbm", "obrazki/las.png", "obrazki/inny.json", "obrazki/las pbm",
                        "obrazki/ł.pbm", "/obrazki/las.pbm", "obrazki/", "noce.csv", "obrazki/las.pbm.tmp"})
    SPRAWDZ(!nazwaObrazkaW(z));
  SPRAWDZ(nazwaObrazkaW("obrazki/" + std::string(MAKS_NAZWY_OBRAZKA_W, 'a') + ".pbm"));
  SPRAWDZ(!nazwaObrazkaW("obrazki/" + std::string(MAKS_NAZWY_OBRAZKA_W + 1, 'a') + ".pbm"));
  // pliki zrzutu bez zmian: obrazki nie są na liście przywracania
  SPRAWDZ(sprawdzPlikW(KomendaW{"obrazki/001_las.pbm", true, 1, sha256Hex("x")}, "x") == "nazwa");
  std::string p = pbmSyntetyczny(800, 434);
  KomendaW k;
  SPRAWDZ(parsujKomendeW("W obrazki/001_las.pbm " + std::to_string(p.size()) + " " + sha256Hex(p), k));
  ROWNE_S(sprawdzObrazekW(k, p), "");
  std::string zly = p;
  zly[100] ^= 1;
  ROWNE_S(sprawdzObrazekW(k, zly), "skrot");
  std::string duzy = pbmSyntetyczny(801, 434);
  SPRAWDZ(parsujKomendeW("W obrazki/x.pbm " + std::to_string(duzy.size()) + " " + sha256Hex(duzy), k));
  ROWNE_S(sprawdzObrazekW(k, duzy), "tresc");  // ponad 800×434 (R-KON-55)
  std::string nie = "nie obrazek";
  SPRAWDZ(parsujKomendeW("W obrazki/x.pbm " + std::to_string(nie.size()) + " " + sha256Hex(nie), k));
  ROWNE_S(sprawdzObrazekW(k, nie), "tresc");
  std::string m = MANIFEST;
  SPRAWDZ(parsujKomendeW("W obrazki/obrazki.json " + std::to_string(m.size()) + " " + sha256Hex(m), k));
  ROWNE_S(sprawdzObrazekW(k, m), "");
  SPRAWDZ(parsujKomendeW("W obrazki/obrazki.json 3 " + sha256Hex("{x}"), k));
  ROWNE_S(sprawdzObrazekW(k, "{x}"), "tresc");
  std::string wielki(MAKS_OBRAZKA_W + 1, 'a');
  SPRAWDZ(parsujKomendeW("W obrazki/x.pbm " + std::to_string(wielki.size()) + " " + sha256Hex(wielki), k));
  ROWNE_S(sprawdzObrazekW(k, wielki), "rozmiar");
}

static void doPortu(Panel& p, const std::string& b) {
  for (char c : b) p.port.znaki.push_back((unsigned char)c);
}
static bool portMa(const Panel& p, const std::string& t) {
  for (auto& l : p.port.log)
    if (l == t) return true;
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
// T-TEST-322: wybudzenie zegarem (jeden obieg czuwania): `?` z narzędzia trzyma czuwanie, kolejne komendy `W` z obrazkami
// zapisane w `/obrazki/`; manifest na końcu; dane nocy, znaczniki przywracania i lista odrzuconych bez zmian.
static void testWgranieObrazkow() {
  Panel p;
  const std::string noce = "2026-11-03;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;\n";
  p.pliki.pliki["/noce.csv"] = noce;
  p.pliki.pliki["/obrazki/001_las.pbm"] = "stary";
  std::string a = pbmSyntetyczny(800, 434), b = pbmSyntetyczny(320, 200), m = MANIFEST;
  std::string zly = komendaW("obrazki/003_x.pbm", b);
  zly[zly.size() - 3] ^= 1;
  doPortu(p, "?" + komendaW("obrazki/001_las.pbm", a) + komendaW("obrazki/002_morze.pbm", b) + zly +
                 komendaW("obrazki/Zla_Nazwa.pbm", b) + komendaW("obrazki/../noce.csv", "x") +
                 komendaW("obrazki/obrazki.json", m) + "W obrazki/004.pbm 9999999 " + sha256Hex("") + "\n");
  wybudzZegarem(p, 14, 0);
  SPRAWDZ(portMa(p, "przywracanie: poza oknem"));
  SPRAWDZ(portMa(p, "OK obrazki/001_las.pbm"));
  SPRAWDZ(portMa(p, "OK obrazki/002_morze.pbm"));
  SPRAWDZ(portMa(p, "BLAD obrazki/003_x.pbm skrot"));
  SPRAWDZ(portMa(p, "BLAD obrazki/Zla_Nazwa.pbm nazwa"));
  SPRAWDZ(portMa(p, "BLAD obrazki/../noce.csv nazwa"));
  SPRAWDZ(portMa(p, "OK obrazki/obrazki.json"));
  SPRAWDZ(portMa(p, "BLAD obrazki/004.pbm rozmiar"));
  SPRAWDZ(p.pliki.tresc("/obrazki/001_las.pbm") == a);  // nadpisany (zapis atomowy przez .tmp)
  SPRAWDZ(p.pliki.tresc("/obrazki/002_morze.pbm") == b);
  SPRAWDZ(p.pliki.tresc("/obrazki/obrazki.json") == m);
  SPRAWDZ(!p.pliki.pliki.count("/obrazki/003_x.pbm") && !p.pliki.pliki.count("/obrazki/Zla_Nazwa.pbm"));
  SPRAWDZ(!p.pliki.pliki.count("/obrazki/001_las.pbm.tmp"));
  SPRAWDZ(p.pliki.tresc("/noce.csv") == noce);
  for (auto& kv : p.pliki.pliki) SPRAWDZ(kv.first.compare(0, 14, "/przywracanie/") != 0);
  SPRAWDZ(p.nvs.tekst("cpap", NVS_KLUCZ_PRZYWR, "") == "" && !p.nvs.flaga("cpap", NVS_KLUCZ_PRZYWR_PROBA, false));
  SPRAWDZ(p.sen.snow == 1);
  // ten sam obrazek widzi dobór S-K-38 (parsujPbm na treści z pamięci)
  ObrazPbm o;
  std::string pw;
  SPRAWDZ(parsujPbm(p.pliki.tresc("/obrazki/002_morze.pbm"), o, pw) && o.w == 320 && o.h == 200);
  // bez `?` i bez `W` wybudzenie zegarem nie czeka (czuwanie zegarem 0 s); samo `?` trzyma czuwanie CZUWANIE_MS
  Panel q, q2;
  wybudzZegarem(q, 14, 0);
  doPortu(q2, "?");
  wybudzZegarem(q2, 14, 0);
  SPRAWDZ(q2.zegar.msTeraz >= q.zegar.msTeraz + CZUWANIE_MS - 1000);
  SPRAWDZ(q2.zegar.msTeraz <= q.zegar.msTeraz + CZUWANIE_MS + 1000);
  // znacznik „bez przywracania” zostaje: obrazki to nie próba przywracania (dotyczy plików zrzutu)
  Panel r;
  r.nvs.ustawTekst("cpap", NVS_KLUCZ_PRZYWR, std::string(ZNACZNIK_BEZ_PRZYWRACANIA) + " 2026-11-01");
  doPortu(r, komendaW("obrazki/002_morze.pbm", b));
  wybudzZegarem(r, 14, 0);
  SPRAWDZ(portMa(r, "OK obrazki/002_morze.pbm"));
  SPRAWDZ(r.nvs.tekst("cpap", NVS_KLUCZ_PRZYWR, "").compare(0, strlen(ZNACZNIK_BEZ_PRZYWRACANIA),
                                                            ZNACZNIK_BEZ_PRZYWRACANIA) == 0);
}


static void odpSpokojna(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  s.siec.siecJest = true;
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
  s.siec.dodaj(sciezkaNocy(DOBA_A), 200, raport(DOBA_A, ep(2026, 11, 3, 23, 0), ep(2026, 11, 4, 6, 0)));
  s.p.czujniki.o.temp = 19.0f + (float)(minutaLok(t) / 60 % 4);  // co godzinę inny, bez progów S-K-03…S-K-05
  s.p.czujniki.o.wilg = 45.0f + (float)(minutaLok(t) / 60 % 4);
}
// spokojny wieczór ze zmiennym czujnikiem: 0 odświeżeń e-ink (stopka z poprzedniej klatki); przejście przez
// próg (S-K-03 przy ≤ 16 °C o 22:00) to zmiana ekranu: 1 odświeżenie, ze świeżym odczytem w treści.
static void odpChlodno(PanelSym& s, uint32_t t) {
  odpSpokojna(s, t);
  if (minutaLok(t) >= 22 * 60) s.p.czujniki.o.temp = 15.0f;
}
static void testStopkaZPamieci() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 6, 0), ep(2026, 11, 4, 23, 20), odpSpokojna);
  int odsw = 0, ocen = 0;
  for (auto& w : v) {
    if (w.powod != PowodWybudzenia::ZEGAR || w.minuta < 21 * 60 || w.minuta >= 23 * 60 + 30) continue;
    odsw += w.m.odswiezen;
    for (auto& z : w.zdarzenia) ocen += z.find("bez zmian: klatka") != std::string::npos;
  }
  ROWNE_L(odsw, 0);
  SPRAWDZ(ocen >= 5);  // ekran oceniony przy każdym wybudzeniu, klatka ta sama
  PanelSym c;
  std::vector<Wyb> u = symuluj(c, ep(2026, 11, 4, 6, 0), ep(2026, 11, 4, 23, 20), odpChlodno);
  int przed = 0, po = 0;
  for (auto& w : u) {
    if (w.powod != PowodWybudzenia::ZEGAR || w.minuta < 21 * 60 || w.minuta >= 23 * 60 + 30) continue;
    (w.minuta < 22 * 60 ? przed : po) += w.m.odswiezen;
  }
  ROWNE_L(przed, 0);
  ROWNE_L(po, 1);  // S-K-03 o 22:00, potem ta sama klatka
}

// Port z przerwą: `a` od razu, `b` dopiero od `odMs` (zegar atrapy).
class PortZPrzerwa : public AtrapaPortu {
 public:
  PortZPrzerwa(Dziennik& d, AtrapaZegara& z, const std::string& a, const std::string& b, uint32_t odMs)
      : AtrapaPortu(d), z_(z), a_(a), b_(b), od_(odMs) {}
  int znak() override {
    if (i_ < a_.size()) return (unsigned char)a_[i_++];
    if (z_.msTeraz >= od_ && j_ < b_.size()) return (unsigned char)b_[j_++];
    return -1;
  }
 private:
  AtrapaZegara& z_;
  std::string a_, b_;
  uint32_t od_;
  size_t i_ = 0, j_ = 0;
};
// treść z bajtami komend nigdy nie idzie do pętli: rozmiar nieczytelny i ponad 4 MB (do ciszy), ponad
// 64 KB (wyrzucone), przerwa ponad 5 s w środku pliku (reszta do ciszy).
static void testTrescNieJestKomenda() {
  const std::string zla = "Z O\nK Z O";
  std::string komendy;
  for (int i = 0; i < 50; i++) komendy += zla;
  for (const std::string& rozmiar : {std::string("abc"), std::string("9999999")}) {
    Panel p;
    doPortu(p, "?W obrazki/x.pbm " + rozmiar + " " + sha256Hex("") + "\n" + komendy);
    wybudzZegarem(p, 14, 0);
    SPRAWDZ(portMa(p, "BLAD obrazki/x.pbm rozmiar"));
    SPRAWDZ(!portMa(p, "PLIKI_START") && p.d.ile("siec.okno") == 0);
    SPRAWDZ(p.sen.snow == 1);
  }
  {
    Panel p;
    std::string duzy(MAKS_OBRAZKA_W + 100, 'Z');
    doPortu(p, "?" + komendaW("obrazki/x.pbm", duzy) + komendaW("obrazki/y.pbm", pbmSyntetyczny(8, 1)));
    wybudzZegarem(p, 14, 0);
    SPRAWDZ(portMa(p, "BLAD obrazki/x.pbm rozmiar"));
    SPRAWDZ(portMa(p, "OK obrazki/y.pbm"));  // port zsynchronizowany: następna komenda przyjęta
    SPRAWDZ(!portMa(p, "PLIKI_START"));
  }
  {
    Panel p;
    std::string t = pbmSyntetyczny(320, 200);
    std::string k = komendaW("obrazki/a.pbm", t);
    PortZPrzerwa port(p.d, p.zegar, "?" + k.substr(0, k.size() - t.size() + 100), komendy, 6000);
    Adaptery ad{p.zegar, p.nvs, p.pliki, p.siec, p.ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik, p.sen,
                port, p.diag};
    p.zegar.epoch = ep(2026, 11, 4, 14, 0);
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Dyrygent(ad, KONF).wybudzenie();
    bool bladRozmiar = false, zrzut = false;
    for (auto& l : port.log) {
      bladRozmiar = bladRozmiar || l == "BLAD obrazki/a.pbm rozmiar";
      zrzut = zrzut || l.find("PLIKI_START") != std::string::npos;
    }
    SPRAWDZ(bladRozmiar);
    SPRAWDZ(!zrzut && p.d.ile("siec.okno") == 0);
    SPRAWDZ(!p.pliki.pliki.count("/obrazki/a.pbm"));
    SPRAWDZ(p.sen.snow == 1);
  }
}

// (T-WYM-87, 8.6): `/konfiguracja.txt` komendą `W konfiguracja.txt` jak obrazki: zapis atomowy, limit, skrót, nazwa;
// noce, NVS i `/przywracanie/` bez zmian; po zapisie panel czyta nowe ustawienia (profil=publiczny).
static void testWgranieKonfiguracji() {
  KomendaW k;
  const std::string t = "profil=publiczny\nco2=nie\n";
  SPRAWDZ(parsujKomendeW("W konfiguracja.txt " + std::to_string(t.size()) + " " + sha256Hex(t), k));
  ROWNE_S(sprawdzKonfiguracjeW(k, t), "");
  ROWNE_S(sprawdzKonfiguracjeW(k, "profil=publiczny\n"), "rozmiar");  // treść krótsza niż w komendzie
  ROWNE_S(sprawdzKonfiguracjeW(k, "profil=publiczny\nco2=tak\n"), "skrot");  // ta sama długość, inna treść
  std::string z(1, 'a');
  z += '\0';
  SPRAWDZ(parsujKomendeW("W konfiguracja.txt 2 " + sha256Hex(z), k));
  ROWNE_S(sprawdzKonfiguracjeW(k, z), "tresc");
  std::string wielki(MAKS_KONFIGURACJI_W + 1, 'a');
  SPRAWDZ(parsujKomendeW("W konfiguracja.txt " + std::to_string(wielki.size()) + " " + sha256Hex(wielki), k));
  ROWNE_S(sprawdzKonfiguracjeW(k, wielki), "rozmiar");
  for (const char* n : {"konfiguracja.txt.bak", "Konfiguracja.txt", "../konfiguracja.txt", "/konfiguracja.txt", "obrazki/konfiguracja.txt"}) {
    k = KomendaW{n, true, 1, sha256Hex("x")};
    ROWNE_S(sprawdzKonfiguracjeW(k, "x"), "nazwa");
  }
  Panel p;
  const std::string noce = "2026-11-03;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;\n";
  p.pliki.pliki["/noce.csv"] = noce;
  p.pliki.pliki["/konfiguracja.txt"] = "profil=prywatny\nco2=tak\n";
  std::string zlySkrot = komendaW("konfiguracja.txt", "a=1\n");
  zlySkrot[zlySkrot.size() - 2] ^= 1;
  std::string duzy(MAKS_KONFIGURACJI_W + 50, 'Z');
  doPortu(p, "?" + zlySkrot + komendaW("konfiguracja.txt.bak", "a=1\n") + komendaW("konfiguracja.txt", duzy) +
                 komendaW("konfiguracja.txt", t));
  wybudzZegarem(p, 14, 0);
  SPRAWDZ(portMa(p, "przywracanie: poza oknem"));
  SPRAWDZ(portMa(p, "BLAD konfiguracja.txt skrot"));
  SPRAWDZ(portMa(p, "BLAD konfiguracja.txt.bak poza oknem"));  // inna nazwa: zwykła ścieżka zrzutu, w czuwaniu bez zapisu
  SPRAWDZ(portMa(p, "BLAD konfiguracja.txt rozmiar"));
  SPRAWDZ(portMa(p, "OK konfiguracja.txt"));  // ostatnia komenda po odrzuconych: port zsynchronizowany
  SPRAWDZ(p.pliki.tresc("/konfiguracja.txt") == t);
  SPRAWDZ(!p.pliki.pliki.count("/konfiguracja.txt.tmp") && !p.pliki.pliki.count("/konfiguracja.txt.bak"));
  SPRAWDZ(p.pliki.tresc("/noce.csv") == noce);
  for (auto& kv : p.pliki.pliki) SPRAWDZ(kv.first.compare(0, 14, "/przywracanie/") != 0);
  SPRAWDZ(p.nvs.tekst("cpap", NVS_KLUCZ_PRZYWR, "") == "" && !p.nvs.flaga("cpap", NVS_KLUCZ_PRZYWR_PROBA, false));
  SPRAWDZ(!portMa(p, "PLIKI_START") && p.d.ile("siec.okno") == 0);
  SPRAWDZ(p.sen.snow == 1);
  // zła treść (bajt 0) nie zastępuje dobrego pliku
  Panel q;
  q.pliki.pliki["/konfiguracja.txt"] = t;
  doPortu(q, "?" + komendaW("konfiguracja.txt", z));
  wybudzZegarem(q, 14, 0);
  SPRAWDZ(portMa(q, "BLAD konfiguracja.txt tresc"));
  SPRAWDZ(q.pliki.tresc("/konfiguracja.txt") == t);
}

int main() {
  (void)zadanZeSciezka;
  (void)STATUS_BLOKADA;
  (void)logMa;
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testPlanWieczoruBezSieci();
  testDobaTesteraS_K_08();
  testNazwyIObrazki();
  testWgranieObrazkow();
  testStopkaZPamieci();
  testTrescNieJestKomenda();
  testWgranieKonfiguracji();
  printf("testy_wieczor_bez_wifi_obrazki: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
