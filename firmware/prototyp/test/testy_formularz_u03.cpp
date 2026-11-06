// SPDX-License-Identifier: GPL-3.0-or-later
// formularz ustawień z telefonu (U-03) uzupełniony o ustawienia obiecane w spec 5.6.4:
// ekrany CO2 (tylko ze źródłem otoczenia), miasto do pogody i wschodu słońca (tabela miast albo własne współrzędne,
// pierwszeństwo przed POGODA_LAT i POGODA_LON z secrets.h), humor, dźwięk, cisza nocna, podpowiedź „co do poprawy”,
// czynniki w raporcie dla lekarza. Testy: parsowanie i zapis każdego klucza, brak wiersza = domyślne, zła wartość
// odrzucona z komunikatem przy polu, pole CO2 tylko ze źródłem, tabela miast, HTML bez zewnętrznych odwołań i z ucieczką
// znaków, rozmiar strony, pierwszeństwo współrzędnych w dyrygencie (adres prognozy). Scenariusz dyrygenta: pomocnicze
// funkcje symulacji skopiowane z test/testy_slot_dzienny_dyrygent.cpp (tam statyczne). Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_formularz_u03). Strona do obejrzenia: /tmp/b32/formularz.html.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <fstream>
#include <map>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "czynniki.h"
#include "dyrygent.h"
#include "formularz.h"
#include "miasta.h"
#include "slot_dzienny.h"
#include "obrazki.h"
#include "plan.h"
#include "raport.h"
#include "sekrety.h"
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
static std::string raportDoby(long day, uint32_t startEp, uint32_t koniecEp) {
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
    s.siec.dodaj(sciezkaNocy(DOBA_A), 200, raportDoby(DOBA_A, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40)));
  } else {
    s.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  }
  s.siec.dodaj("/v1/forecast", 200, jsonPrognozy("2026-11-04"));
  s.siec.dodaj("/v1/air-quality", 200, jsonPowietrza("2026-11-04"));
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":1250},\"ostrzezenia\":[]}");
}
static int ileZadan(const std::vector<Wyb>& v, const std::string& p) {
  int n = 0;
  for (auto& w : v) n += zadanZeSciezka(w, p);
  return n;
}

// ---------- pomocnicze ----------
static bool zawiera(const std::string& s, const std::string& w) { return s.find(w) != std::string::npos; }
static size_t ile(const std::string& s, const std::string& w) {
  size_t n = 0;
  for (size_t p = s.find(w); p != std::string::npos; p = s.find(w, p + 1)) n++;
  return n;
}
static std::string dzis() { return "2026-11-05"; }
static formularz::Wynik waliduj(const std::string& cialo) {
  return formularz::waliduj(formularz::parsujCialo(cialo), sekrety::Dane(), dzis());
}
static std::string plik(const formularz::Wynik& w, const char* k) {
  auto i = w.plik.find(k);
  return i == w.plik.end() ? "<brak>" : i->second;
}

// ---------- tabela miast ----------
static void testTabelaMiast() {
  // dokładnie 18 miast wojewódzkich, w tym oba w kujawsko-pomorskim i lubuskim
  SPRAWDZ(miasta::MIASTA_N == 18);
  for (const char* n : {"Białystok", "Bydgoszcz", "Gdańsk", "Gorzów Wielkopolski", "Katowice", "Kielce", "Kraków", "Lublin", "Łódź",
                        "Olsztyn", "Opole", "Poznań", "Rzeszów", "Szczecin", "Toruń", "Warszawa", "Wrocław", "Zielona Góra"})
    SPRAWDZ(miasta::znajdz(n) != nullptr);
  std::vector<std::string> nazwy;
  for (int i = 0; i < miasta::MIASTA_N; i++) {
    const miasta::Miasto& m = miasta::MIASTA[i];
    SPRAWDZ(miasta::wPolsce(m.szer, m.dl));   // wszystkie współrzędne w Polsce
    SPRAWDZ(m.nazwa[0] != '\0' && std::string(m.nazwa).find_first_of("<>&\"'=%+") == std::string::npos);
    nazwy.push_back(m.nazwa);
    SPRAWDZ(miasta::znajdz(m.nazwa) == &m);
    SPRAWDZ(std::fabs(m.szer * 100 - std::round(m.szer * 100)) < 0.01 && std::fabs(m.dl * 100 - std::round(m.dl * 100)) < 0.01);  // 0,01°
  }
  std::vector<std::string> uniq = nazwy;
  std::sort(uniq.begin(), uniq.end());
  SPRAWDZ(std::adjacent_find(uniq.begin(), uniq.end()) == uniq.end());  // bez duplikatów
  SPRAWDZ(miasta::znajdz("Pcim") == nullptr && miasta::znajdz("") == nullptr && miasta::znajdz("katowice") == nullptr);
  const miasta::Miasto* k = miasta::znajdz("Katowice");
  SPRAWDZ(k && std::fabs(k->szer - 50.26) < 1e-4 && std::fabs(k->dl - 19.02) < 1e-4);
  const miasta::Miasto* w = miasta::znajdz("Warszawa");
  SPRAWDZ(w && std::fabs(w->szer - 52.23) < 1e-4 && std::fabs(w->dl - 21.01) < 1e-4);
  SPRAWDZ(miasta::wPolsce(52.0, 19.0) && !miasta::wPolsce(48.0, 19.0) && !miasta::wPolsce(52.0, 30.0) &&
          !miasta::wPolsce(60.0, 19.0) && !miasta::wPolsce(52.0, 10.0));
}

// ---------- parser konfiguracji (parsujUstawieniaDnia) ----------
static void testParser() {
  UstawieniaDnia d0 = parsujUstawieniaDnia("");  // brak wiersza = domyślne
  SPRAWDZ(d0.humor == -1 && d0.buzzer == -1 && d0.ciszaOd == -1 && d0.ciszaDo == -1 && d0.podpowiedz == -1 &&
          d0.miasto.empty() && std::isnan(d0.pogodaLat) && std::isnan(d0.pogodaLon) && d0.bledy.empty());
  UstawieniaDnia u = parsujUstawieniaDnia("humor=oszczedny\nbuzzer=nie\ncisza=23:00-07:30\npodpowiedz=nie\nmiasto=Gdańsk\n");
  SPRAWDZ(u.bledy.empty() && u.humor == 1 && u.buzzer == 0 && u.ciszaOd == 23 * 60 && u.ciszaDo == 7 * 60 + 30 &&
          u.podpowiedz == 0 && u.miasto == "Gdańsk");
  SPRAWDZ(std::fabs(u.pogodaLat - 54.35) < 1e-4 && std::fabs(u.pogodaLon - 18.65) < 1e-4);  // z tabeli, bez pary
  SPRAWDZ(parsujUstawieniaDnia("humor=wylaczony").humor == 0 && parsujUstawieniaDnia("humor=wyłączony").humor == 0 &&
          parsujUstawieniaDnia("humor=oszczędny").humor == 1 && parsujUstawieniaDnia("humor=normalny").humor == 2);
  SPRAWDZ(parsujUstawieniaDnia("buzzer=tak").buzzer == 1 && parsujUstawieniaDnia("podpowiedz=tak").podpowiedz == 1);
  SPRAWDZ(parsujUstawieniaDnia("cisza=22:00–10:00").ciszaDo == 10 * 60);  // myślnik półpauza też
  // zła wartość: wiersz pominięty z wpisem w `bledy` (R-KONF-21), wartość domyślna
  for (const char* zle_ : {"humor=wesoły", "humor=", "buzzer=może", "buzzer=Tak", "podpowiedz=1", "cisza=22:00", "cisza=22:00-22:00",
                           "cisza=24:00-08:00", "cisza=22:60-08:00", "cisza=8:00-10:00", "cisza=22:00-08:00:00", "cisza=",
                           "miasto=Pcim", "miasto=", "miasto=katowice", "pogoda.lat=50.2\npogoda.lon=19.1"}) {
    UstawieniaDnia z = parsujUstawieniaDnia(zle_);
    SPRAWDZ(!z.bledy.empty());
    SPRAWDZ(z.humor == -1 && z.buzzer == -1 && z.ciszaOd == -1 && z.podpowiedz == -1 && z.miasto.empty() &&
            std::isnan(z.pogodaLat));
  }
  // BOM, \r i spacje na końcu także dla nowych kluczy
  UstawieniaDnia b = parsujUstawieniaDnia("\xEF\xBB\xBFhumor=oszczedny\r\nbuzzer=nie \t\r\n");
  SPRAWDZ(b.humor == 1 && b.buzzer == 0 && b.bledy.empty());
  int od = 0, doo = 0;
  SPRAWDZ(ciszaZTekstu("00:00-23:59", od, doo) && od == 0 && doo == 23 * 60 + 59);
  SPRAWDZ(!ciszaZTekstu("", od, doo) && !ciszaZTekstu("-", od, doo) && !ciszaZTekstu("22:00-", od, doo));
  // stare klucze bez zmian
  UstawieniaDnia s = parsujUstawieniaDnia("co2=nie\nprofil=publiczny\nwymiana.filtr=2026-09-10\nokres.maska=6\nraport.czynniki=K\n");
  SPRAWDZ(s.co2 == 0 && s.profilPubliczny && s.bledy.empty());
  // R-SKI-08 (przegląd kliniczny): „Początek leczenia” tylko z pliku K, zła data pominięta z wpisem
  SPRAWDZ(parsujUstawieniaDnia("poczatek_leczenia=2026-09-10\n").poczatekLeczenia == "2026-09-10");
  SPRAWDZ(d0.poczatekLeczenia.empty());
  UstawieniaDnia pl = parsujUstawieniaDnia("poczatek_leczenia=10.09.2026\n");
  SPRAWDZ(pl.poczatekLeczenia.empty() && pl.bledy.size() == 1);
}

// ---------- formularz: wartości do pokazania ----------
static void testPola() {
  sekrety::Dane ob;
  formularz::Pola d = formularz::zPliku("", ob);  // brak wiersza = domyślne ze spec
  SPRAWDZ(d.humor == "normalny" && d.buzzer == "tak" && d.ciszaOd == "22:00" && d.ciszaDo == "08:00" &&
          d.podpowiedz == "tak" && d.miasto.empty() && d.co2 == "tak" &&
          d.czynniki == "KL" && !d.zrodloOto);
  formularz::Pola z = formularz::zPliku("humor=oszczedny\nbuzzer=nie\ncisza=23:00-07:00\npodpowiedz=nie\nco2=nie\nmiasto=Gdańsk\n"
                                        "raport.czynniki=AKT\n", ob, true);
  SPRAWDZ(z.humor == "oszczedny" && z.buzzer == "nie" && z.ciszaOd == "23:00" && z.ciszaDo == "07:00" &&
          z.podpowiedz == "nie" && z.co2 == "nie" && z.miasto == "Gdańsk" && z.czynniki == "AKT" && z.zrodloOto);
  SPRAWDZ(formularz::zPliku("miasto=Gdańsk\n", ob).miasto == "Gdańsk");
  // bez `miasto=`: współrzędne z secrets.h (50.2597, 19.0217) w granicy 0,05° od Katowic zaznaczają Katowice, inaczej „nie wybrano”
  SPRAWDZ(formularz::zPliku("", ob, false, 50.2597, 19.0217).miasto == "Katowice");
  SPRAWDZ(formularz::zPliku("", ob, false, 50.30, 19.05).miasto == "Katowice");  // 0,04° i 0,03°
  SPRAWDZ(formularz::zPliku("", ob, false, 50.32, 19.02).miasto.empty());        // 0,06°
  SPRAWDZ(formularz::zPliku("", ob, false, 50.0, 10.0).miasto.empty() && formularz::zPliku("", ob).miasto.empty());
  SPRAWDZ(formularz::zPliku("miasto=Gdańsk\n", ob, false, 50.2597, 19.0217).miasto == "Gdańsk");  // wpis wygrywa z secrets.h
  SPRAWDZ(formularz::zPliku("raport.czynniki=N\n", ob).czynniki.empty());  // wiersz bez czynników (żaden)
  SPRAWDZ(formularz::zPliku("raport.czynniki=\n", ob).czynniki.empty());
  SPRAWDZ(formularz::zPliku("humor=wesoły\nbuzzer=?\ncisza=zle\n", ob).humor == "normalny");  // zła wartość: domyślne
}

// ---------- formularz: walidacja i zapis ----------
static void testZapis() {
  // domyślne = usunięcie wiersza (brak wiersza = domyślne), zapis każdego klucza
  formularz::Wynik w = waliduj("humor=wylaczony&buzzer=nie&podpowiedz=nie&co2=nie&cisza_od=23%3A00&cisza_do=07%3A30");
  SPRAWDZ(w.ok() && plik(w, "humor") == "wylaczony" && plik(w, "buzzer") == "nie" && plik(w, "podpowiedz") == "nie" &&
          plik(w, "co2") == "nie" && plik(w, "cisza") == "23:00-07:30");
  w = waliduj("humor=oszczedny");
  SPRAWDZ(w.ok() && plik(w, "humor") == "oszczedny");
  w = waliduj("humor=normalny&buzzer=tak&podpowiedz=tak&co2=tak&cisza_od=22%3A00&cisza_do=08%3A00");
  SPRAWDZ(w.ok() && plik(w, "humor").empty() && plik(w, "buzzer").empty() && plik(w, "podpowiedz").empty() &&
          plik(w, "co2").empty() && plik(w, "cisza").empty());  // wartość domyślna: "" = wiersz usunięty
  w = waliduj("ssid=&humor=normalny");  // pola, których nie ma w ciele, zostają bez zmian
  SPRAWDZ(!w.plik.count("buzzer") && !w.plik.count("cisza") && !w.plik.count("miasto") && !w.plik.count("raport.czynniki"));
  // zła wartość: odrzucona z komunikatem przy polu, nic nie trafia do pliku
  for (const char* c : {"humor=wesoly", "buzzer=moze", "podpowiedz=1", "co2=Tak", "humor=", "co2="}) {
    w = waliduj(c);
    SPRAWDZ(w.bledy.size() == 1 && w.plik.empty());
    SPRAWDZ(!w.bledy.begin()->second.empty());
  }
  w = waliduj("cisza_od=24%3A00&cisza_do=08%3A00");
  SPRAWDZ(w.bledy.count("cisza_od") && !w.bledy.count("cisza_do") && !w.plik.count("cisza"));
  w = waliduj("cisza_od=22%3A00&cisza_do=abc");
  SPRAWDZ(w.bledy.count("cisza_do") && !w.plik.count("cisza"));
  w = waliduj("cisza_od=22%3A00");  // druga godzina nie przyszła
  SPRAWDZ(w.bledy.count("cisza_do") && !w.plik.count("cisza"));
  w = waliduj("cisza_od=22%3A00&cisza_do=22%3A00");  // pusta albo całodobowa cisza (R-BUZ-02)
  SPRAWDZ(w.bledy.count("cisza_do") && !w.plik.count("cisza"));
  w = waliduj("cisza_od=22%3A00%3A30&cisza_do=08%3A00");
  SPRAWDZ(w.bledy.count("cisza_od"));
  // dobra i zła wartość razem: poprawne zapisane (R-KONF-18)
  w = waliduj("humor=oszczedny&buzzer=moze");
  SPRAWDZ(plik(w, "humor") == "oszczedny" && w.bledy.count("buzzer") && !w.plik.count("buzzer"));
  // miasto z listy: nazwa i współrzędne z tabeli, bez żadnego zapytania do internetu
  w = waliduj("miasto=" + formularz::koduj("Łódź"));
  SPRAWDZ(w.ok() && plik(w, "miasto") == "Łódź" && w.plik.size() == 1);  // tylko `miasto=`, bez współrzędnych
  for (int i = 0; i < miasta::MIASTA_N; i++) {  // każde miasto z tabeli przechodzi i wraca z pliku jako to samo miasto
    w = waliduj("miasto=" + formularz::koduj(miasta::MIASTA[i].nazwa));
    SPRAWDZ(w.ok() && plik(w, "miasto") == miasta::MIASTA[i].nazwa);
    std::string nowy = formularz::scal("", w.plik);
    SPRAWDZ(formularz::zPliku(nowy, sekrety::Dane()).miasto == miasta::MIASTA[i].nazwa);
    UstawieniaDnia u = parsujUstawieniaDnia(nowy);
    SPRAWDZ(u.bledy.empty() && std::fabs(u.pogodaLat - miasta::MIASTA[i].szer) < 1e-6 &&
            std::fabs(u.pogodaLon - miasta::MIASTA[i].dl) < 1e-6);
  }
  w = waliduj("miasto=Pcim");
  SPRAWDZ(w.bledy.count("miasto") && w.plik.empty());
  w = waliduj("miasto=inne&pog_szer=50.1&pog_dl=19.5");  // pola własnych współrzędnych nie istnieją
  SPRAWDZ(w.bledy.count("miasto") && w.plik.empty());
  w = waliduj("miasto=Katowice&pog_szer=1&pogoda.lat=1");
  SPRAWDZ(w.ok() && w.plik.size() == 1 && plik(w, "miasto") == "Katowice");
  w = waliduj("miasto=");  // bez wyboru: wpisy z konfiguracji znikają, panel wraca do secrets.h
  SPRAWDZ(w.ok() && plik(w, "miasto").empty() && w.plik.size() == 1);
  std::string stary = "miasto=Gdańsk\nhumor=oszczedny\n";
  SPRAWDZ(formularz::scal(stary, w.plik) == "humor=oszczedny\n");
  // czynniki: zmiana tylko przy innym wyborze niż pokazany (cz_start)
  w = waliduj("cz_start=KL&cz_K=1&cz_L=1");
  SPRAWDZ(w.ok() && !w.plik.count("raport.czynniki"));
  w = waliduj("cz_start=KL&cz_K=1&cz_L=1&cz_A=1&cz_P=1");
  SPRAWDZ(w.ok() && plik(w, "raport.czynniki") == "AKLP");
  w = waliduj("cz_start=KL");  // odznaczone wszystkie
  SPRAWDZ(w.ok() && plik(w, "raport.czynniki") == "N");
  SPRAWDZ(waliduj("cz_start=&cz_K=1").plik.at("raport.czynniki") == "K");
  SPRAWDZ(!waliduj("cz_start=").plik.count("raport.czynniki"));
  w = waliduj("cz_start=X&cz_K=1");
  SPRAWDZ(w.bledy.count("cz_start") && w.plik.empty());
  w = waliduj("cz_start=K&cz_N=1&cz_Z=1");  // N i nieznane kody nie są czynnikami
  SPRAWDZ(w.ok() && plik(w, "raport.czynniki") == "N");
  SPRAWDZ(!waliduj("cz_K=1").plik.count("raport.czynniki"));  // bez cz_start nic nie ruszamy
  // pełny obieg: zapis → plik → parser panelu → te same wartości
  w = waliduj("humor=oszczedny&buzzer=nie&podpowiedz=nie&co2=nie&cisza_od=23%3A15&cisza_do=06%3A45&miasto=" +
              formularz::koduj("Opole") + "&cz_start=KL&cz_A=1&cz_T=1");
  std::string nowy = formularz::scal("# komentarz\nprofil=publiczny\nokres.rura=3\n", w.plik);
  UstawieniaDnia u = parsujUstawieniaDnia(nowy);
  SPRAWDZ(u.bledy.empty() && u.humor == 1 && u.buzzer == 0 && u.podpowiedz == 0 && u.co2 == 0 && u.ciszaOd == 23 * 60 + 15 &&
          u.ciszaDo == 6 * 60 + 45 && u.miasto == "Opole" && u.profilPubliczny);
  bool ust = false;
  SPRAWDZ(raport::czynnikiZKonfiguracji(nowy, ust) == (CZ_ALKOHOL | CZ_THC) && ust);
  SPRAWDZ(zawiera(nowy, "# komentarz\n") && zawiera(nowy, "okres.rura=3\n"));  // cudze wiersze bez zmian
  // „Początek leczenia” nie ma w formularzu: wiersz zostaje w pliku po zapisie
  SPRAWDZ(zawiera(formularz::scal("poczatek_leczenia=2026-09-10\n", w.plik), "poczatek_leczenia=2026-09-10\n"));
  // ponowny zapis tego samego: plik bez zmian (bez zbędnego zapisu we flash)
  formularz::Pola pp = formularz::zPliku(nowy, sekrety::Dane());
  w = waliduj("humor=" + pp.humor + "&buzzer=" + pp.buzzer + "&cisza_od=" + formularz::koduj(pp.ciszaOd) + "&cisza_do=" +
              formularz::koduj(pp.ciszaDo) + "&miasto=" + formularz::koduj(pp.miasto) + "&cz_start=" + pp.czynniki + "&cz_A=1&cz_T=1");
  SPRAWDZ(formularz::scal(nowy, w.plik) == nowy);
}

// ---------- formularz: strona ----------
static void testStrona(bool zapisz) {
  sekrety::Dane ob;
  ob.ssid = "Dom";
  ob.hasloWifi = "TajneHaslo123";
  formularz::Pola bez = formularz::zPliku("", ob, false);
  formularz::Pola ze = formularz::zPliku("", ob, true);
  std::string sb = formularz::strona(bez, "/u/abc/", "", {});
  std::string sz = formularz::strona(ze, "/u/abc/", "", {});
  // pole CO2 tylko ze źródłem; bez źródła krótki tekst
  SPRAWDZ(!zawiera(sb, "name=\"co2\"") && zawiera(sb, "Ten panel nie ma czujnika CO2."));
  SPRAWDZ(zawiera(sz, "name=\"co2\"") && !zawiera(sz, "Ten panel nie ma czujnika CO2."));
  // grupy z nagłówkami
  for (const char* g : {"Sieć domowa", "AirBridge", "Panel", "Pogoda", "Dźwięk i humor", "Raport dla lekarza", "Sprzęt"})
    SPRAWDZ(zawiera(sb, std::string("<legend>") + g));
  size_t poz = 0;
  for (const char* g : {"Sieć domowa", "AirBridge", "Panel", "Pogoda", "Dźwięk i humor", "Raport dla lekarza", "Sprzęt"}) {
    size_t n = sb.find(std::string("<legend>") + g, poz);
    SPRAWDZ(n != std::string::npos);
    if (n != std::string::npos) poz = n;
  }
  // pola, przycisk, tryb jasny, hasła nie wracają
  for (const char* n : {"humor", "buzzer", "cisza_od", "cisza_do", "podpowiedz", "miasto", "cz_start", "cz_K", "cz_L"})
    SPRAWDZ(zawiera(sb, std::string("name=\"") + n + "\""));
  SPRAWDZ(!zawiera(sb, "pog_szer") && !zawiera(sb, "pog_dl") && !zawiera(sb, "inne") && !zawiera(sb, "współrzędne (wpisz"));
  SPRAWDZ(!zawiera(sb, "name=\"cz_N\"") && zawiera(sb, "type=\"time\"") && zawiera(sb, "type=\"checkbox\" name=\"cz_A\""));
  SPRAWDZ(zawiera(sb, "<button type=\"submit\">Zapisz</button>") && zawiera(sb, "background:#000;color:#fff") &&
          zawiera(sb, "color-scheme:light"));
  SPRAWDZ(!zawiera(sb, "TajneHaslo123"));
  // lista miast: wszystkie w stronie, domyślnie „bez wyboru”, miasto z pliku zaznaczone
  SPRAWDZ(ile(sb, "<option") >= (size_t)miasta::MIASTA_N + 1);
  for (int i = 0; i < miasta::MIASTA_N; i++) SPRAWDZ(zawiera(sb, std::string(">") + miasta::MIASTA[i].nazwa + "</option>"));
  SPRAWDZ(zawiera(sb, "<option value=\"\" selected>"));
  formularz::Pola g = formularz::zPliku("miasto=Gdańsk\n", ob);
  std::string sg = formularz::strona(g, "/u/abc/", "", {});
  SPRAWDZ(zawiera(sg, "<option selected>Gdańsk</option>") && !zawiera(sg, "<option value=\"\" selected>"));
  // dźwięk: zdanie ostrzegawcze zawsze, ostrzeżenie o pilności A przy wyłączonym
  SPRAWDZ(zawiera(sb, "Bez dźwięku nie usłyszysz sygnału o sprawie do lekarza.") && !zawiera(sb, "pilność A"));
  std::string sc = formularz::strona(formularz::zPliku("buzzer=nie\n", ob), "/u/abc/", "", {});
  SPRAWDZ(zawiera(sc, "pilność A") && zawiera(sc, "<option value=\"nie\" selected>wyłączony</option>"));
  // zaznaczenia wartości z pliku
  std::string sw = formularz::strona(formularz::zPliku("humor=wylaczony\npodpowiedz=nie\ncisza=23:00-07:00\nraport.czynniki=AT\n", ob), "/u/a/", "", {});
  SPRAWDZ(zawiera(sw, "<option value=\"wylaczony\" selected>") && zawiera(sw, "<option value=\"nie\" selected>wyłączona") &&
          zawiera(sw, "name=\"cisza_od\" id=\"cisza_od\" value=\"23:00\"") && zawiera(sw, "name=\"cz_A\" value=\"1\" checked") &&
          zawiera(sw, "name=\"cz_T\" value=\"1\" checked") && !zawiera(sw, "name=\"cz_K\" value=\"1\" checked") &&
          zawiera(sw, "name=\"cz_start\" value=\"AT\""));
  // błędy przy polach, ucieczka znaków, bez zewnętrznych odwołań
  formularz::Pola zly = ze;
  zly.czynniki = "K\"><b>";
  std::map<std::string, std::string> bl = {{"cisza_do", "Zły <czas>"}, {"humor", "\"x\""}, {"miasto", "'a'&b"}};
  std::string se = formularz::strona(zly, "/u/\"x\"/", "Komunikat <b>", bl);
  SPRAWDZ(!zawiera(se, "<script>") && !zawiera(se, "<b>") && zawiera(se, "Zły &lt;czas&gt;") && zawiera(se, "&#39;a&#39;&amp;b") &&
          zawiera(se, "&quot;x&quot;"));
  SPRAWDZ(!zawiera(sb, "http://") && !zawiera(sb, "https://") && !zawiera(sb, "src=") && !zawiera(sb, "<script") &&
          !zawiera(sb, "<link rel=\"stylesheet") && !zawiera(sb, "@import") && !zawiera(sb, "url("));
  SPRAWDZ(!zawiera(sz, "http://") && !zawiera(sz, "https://") && !zawiera(sz, "<script"));
  // rozmiar strony (pamięć panelu): strona jest składana raz i wysyłana w jednym kawałku, bez żadnego zewnętrznego zasobu
  printf("rozmiar strony formularza: %zu B bez źródła CO2, %zu B ze źródłem CO2, %zu B z błędami i komunikatem\n", sb.size(),
         sz.size(), se.size());
  SPRAWDZ(sb.size() < 16 * 1024 && sz.size() < 16 * 1024 && se.size() < 16 * 1024);
  if (zapisz) {
    std::ofstream o("/tmp/b32/formularz.html", std::ios::binary);
    o << formularz::strona(formularz::zPliku("humor=oszczedny\ncisza=23:00-07:00\nmiasto=Gdańsk\nraport.czynniki=AK\n", ob, true),
                           "/u/abc/", "Zapisano o 21:14.", {{"cisza_do", "Początek i koniec ciszy muszą być różne."}});
  }
}

// POST z nowymi polami przechodzi przez sesję do zapisu, a ciało nie przekracza limitu przy wszystkich polach naraz
static void testSesja() {
  formularz::Sesja ses;
  ses.otworz("tok", 1000, "192.168.1.9");
  std::string cialo = "ssid=Dom&wifi_haslo=" + std::string(63, 'x') + "&ab_adres=192.168.1.20&ab_konto=admin&ab_haslo=" +
                      std::string(64, 'y') + "&profil=publiczny&co2=nie&humor=oszczedny&buzzer=nie&cisza_od=23%3A00&cisza_do=07%3A00"
                      "&podpowiedz=nie&miasto=Opole&cz_start=KL&cz_A=1&cz_K=1&cz_S=1&cz_J=1&cz_L=1&cz_T=1&cz_P=1"
                      "&wymiana.maska=2026-09-01&okres.maska=6&wymiana.filtr=2026-10-01&okres.filtr_tyg=4";
  SPRAWDZ(cialo.size() < formularz::MAKS_CIALA / 2);
  formularz::Odpowiedz o = ses.obsluz("POST", "/u/tok/", cialo, 2000, "");
  SPRAWDZ(o.zapis && o.pola.at("humor") == "oszczedny" && o.pola.at("cz_start") == "KL");
  formularz::Wynik w = formularz::waliduj(o.pola, sekrety::Dane(), dzis());
  SPRAWDZ(w.ok() && plik(w, "humor") == "oszczedny" && plik(w, "cisza") == "23:00-07:00" && plik(w, "miasto") == "Opole" &&
          plik(w, "raport.czynniki") == "AKSJLTP");
  // klucz spoza 5.6.4 (np. ustawienie aparatu) nie trafia do pliku (R-KONF-01, R-NF-10)
  w = waliduj("epr=8&mode=ASV&api_settings=1&miasto=Pcim");
  SPRAWDZ(!w.plik.count("epr") && !w.plik.count("mode") && !w.plik.count("api_settings"));
}

// ---------- dyrygent: pierwszeństwo współrzędnych z konfiguracji przed secrets.h ----------
static std::string adresPrognozy(const std::string& konfiguracja, bool wspolrzedneZSecrets, int* zapytan = nullptr) {
  PanelSym s;
  if (!konfiguracja.empty()) s.p.pliki.pliki["/konfiguracja.txt"] = konfiguracja;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 9, 0), odpowiedziB9, false, konf(wspolrzedneZSecrets));
  if (zapytan) *zapytan = ileZadan(v, "/v1/forecast");
  for (auto& w : v)
    for (auto& z : w.zdarzenia)
      if (z.compare(0, 9, "siec.get ") == 0 && z.find("/v1/forecast") != std::string::npos) return z.substr(9);
  return "";
}
static void testPogodaWDyrygencie() {
  int n = 0;
  // jak dziś: współrzędne z secrets.h (konf(true): 50.2597, 19.0217), bez wiersza w konfiguracji
  std::string u0 = adresPrognozy("", true, &n);
  SPRAWDZ(zawiera(u0, "latitude=50.2597&longitude=19.0217") && n == 1);
  // miasto z formularza ma pierwszeństwo przed secrets.h
  std::string u1 = adresPrognozy("miasto=Warszawa\n", true, &n);
  SPRAWDZ(zawiera(u1, "latitude=52.2300&longitude=21.0100") && !zawiera(u1, "50.2597") && n == 1);
  std::string u2 = adresPrognozy("miasto=Gdańsk\n", false);  // bez secrets.h też
  SPRAWDZ(zawiera(u2, "latitude=54.3500&longitude=18.6500"));
  // bez secrets.h i bez wpisu: jak dotąd brak zapytań do serwisu pogody (R-KON-53, R-KONF-24)
  std::string u4 = adresPrognozy("", false, &n);
  SPRAWDZ(u4.empty() && n == 0);
  // zła wartość w konfiguracji: wiersz pominięty, współrzędne z secrets.h; wiersze pogoda.* nie są już kluczami
  std::string u5 = adresPrognozy("miasto=Pcim\npogoda.lat=52.23\npogoda.lon=21.01\n", true);
  SPRAWDZ(zawiera(u5, "latitude=50.2597&longitude=19.0217"));
}

// R-KONF-42 (zakres według modelu pasm wycieku): „Uwaga o masce od”
// w `/konfiguracja.txt` (`uwaga_o_masce`, 10–15 L/min, domyślnie 10) i w formularzu U-03 (pole liczbowe w grupie
// „Sprzęt”, walidacja zakresu, domyślne usuwa wiersz).
static void testUwagaOMasce() {
  SPRAWDZ(parsujUstawieniaDnia("").uwagaOMasce == -1);
  SPRAWDZ(parsujUstawieniaDnia("uwaga_o_masce=10\n").uwagaOMasce == 10);
  SPRAWDZ(parsujUstawieniaDnia("uwaga_o_masce=15\n").uwagaOMasce == 15);
  SPRAWDZ(parsujUstawieniaDnia("uwaga_o_masce=12 \n").uwagaOMasce == 12);
  for (const char* zla : {"uwaga_o_masce=9", "uwaga_o_masce=16", "uwaga_o_masce=24", "uwaga_o_masce=abc", "uwaga_o_masce=",
                          "uwaga_o_masce=15.5", "uwaga_o_masce=-12"}) {
    UstawieniaDnia u = parsujUstawieniaDnia(zla);
    SPRAWDZ(u.uwagaOMasce == -1 && u.bledy.size() == 1);
  }
  sekrety::Dane ob;
  SPRAWDZ(formularz::zPliku("", ob).uwagaOMasce == "10");
  SPRAWDZ(formularz::zPliku("uwaga_o_masce=13\n", ob).uwagaOMasce == "13");
  SPRAWDZ(formularz::zPliku("uwaga_o_masce=20\n", ob).uwagaOMasce == "10");  // zła wartość w pliku: domyślne
  formularz::Wynik w = waliduj("uwaga_o_masce=13");
  SPRAWDZ(w.ok() && plik(w, "uwaga_o_masce") == "13");
  w = waliduj("uwaga_o_masce=15");
  SPRAWDZ(w.ok() && plik(w, "uwaga_o_masce") == "15");
  w = waliduj("uwaga_o_masce=10");
  SPRAWDZ(w.ok() && plik(w, "uwaga_o_masce").empty());  // domyślne: wiersz znika (R-KONF-20)
  w = waliduj("uwaga_o_masce=");
  SPRAWDZ(w.ok() && plik(w, "uwaga_o_masce").empty());
  for (const char* c : {"uwaga_o_masce=9", "uwaga_o_masce=16", "uwaga_o_masce=24", "uwaga_o_masce=abc", "uwaga_o_masce=1%2C5"}) {
    w = waliduj(c);
    SPRAWDZ(!w.ok() && w.bledy["uwaga_o_masce"] == "Liczba od 10 do 15." && plik(w, "uwaga_o_masce") == "<brak>");
  }
  SPRAWDZ(waliduj("humor=normalny").plik.count("uwaga_o_masce") == 0);  // bez pola: bez zmiany
  std::string st = formularz::strona(formularz::zPliku("uwaga_o_masce=12\n", ob), "/u/a/", "", {});
  SPRAWDZ(zawiera(st, "name=\"uwaga_o_masce\"") && zawiera(st, "value=\"12\"") && zawiera(st, "min=\"10\" max=\"15\""));
  SPRAWDZ(zawiera(st, "Od tej wartości do 15 l/min panel opisuje wyciek ekranem „Wyciek widoczny”, bez rady o masce. Domyślnie 10, 15 = bez tej informacji."));
  SPRAWDZ(st.find("name=\"uwaga_o_masce\"") > st.find("<legend>Sprzęt"));
  std::string se = formularz::strona(formularz::zPliku("", ob), "/u/a/", "", {{"uwaga_o_masce", "Liczba od 10 do 15."}});
  SPRAWDZ(zawiera(se, "Liczba od 10 do 15."));
  // zapis z formularza czyta panel tym samym parserem
  std::string nowa = formularz::scal("humor=oszczedny\n", waliduj("uwaga_o_masce=14").plik);
  SPRAWDZ(parsujUstawieniaDnia(nowa).uwagaOMasce == 14 && parsujUstawieniaDnia(nowa).humor == 1);
}

int main() {
  testTabelaMiast();
  testUwagaOMasce();
  testParser();
  testPola();
  testZapis();
  testStrona(true);
  testSesja();
  testPogodaWDyrygencie();
  printf("testy_formularz_u03: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
