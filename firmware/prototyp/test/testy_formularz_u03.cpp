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
#include <cctype>
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
  // grupy z nagłówkami: od 14.11.2026 sekcje zwijane w kolejności z makiety (R-KONF-59)
  for (const char* g : {"Sieć domowa", "AirBridge", "Pogoda", "Panel", "Dźwięk i humor", "Sprzęt", "Raport dla lekarza"})
    SPRAWDZ(zawiera(sb, std::string("<summary><b>") + g));
  size_t poz = 0;
  for (const char* g : {"Sieć domowa", "AirBridge", "Pogoda", "Panel", "Dźwięk i humor", "Sprzęt", "Raport dla lekarza"}) {
    size_t n = sb.find(std::string("<summary><b>") + g, poz);
    SPRAWDZ(n != std::string::npos);
    if (n != std::string::npos) poz = n;
  }
  // pola, przycisk, tryb jasny, hasła nie wracają
  for (const char* n : {"humor", "buzzer", "cisza_od", "cisza_do", "podpowiedz", "miasto", "cz_start", "cz_K", "cz_L"})
    SPRAWDZ(zawiera(sb, std::string("name=\"") + n + "\""));
  SPRAWDZ(!zawiera(sb, "pog_szer") && !zawiera(sb, "pog_dl") && !zawiera(sb, "inne") && !zawiera(sb, "współrzędne (wpisz"));
  SPRAWDZ(!zawiera(sb, "name=\"cz_N\"") && zawiera(sb, "type=\"time\"") && zawiera(sb, "type=\"checkbox\" name=\"cz_A\""));
  SPRAWDZ(zawiera(sb, "<button type=\"submit\">Zapisz na panelu</button>") && zawiera(sb, "background:#000;color:#fff") &&
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
  // od 14.11.2026 jeden własny skrypt w stronie (licznik zmian, R-KONF-59): wartości z ucieczką nie dokładają drugiego
  SPRAWDZ(ile(se, "<script") == 1 && !zawiera(se, "<b>&") && zawiera(se, "Zły &lt;czas&gt;") &&
          zawiera(se, "&#39;a&#39;&amp;b") && zawiera(se, "&quot;x&quot;") && !zawiera(se, "K\"><b>"));
  SPRAWDZ(!zawiera(sb, "http://") && !zawiera(sb, "https://") && !zawiera(sb, "src=") && ile(sb, "<script") == 1 &&
          !zawiera(sb, "<script src") && !zawiera(sb, "<link rel=\"stylesheet") && !zawiera(sb, "@import") &&
          !zawiera(sb, "url(") && !zawiera(sb, "fonts."));
  SPRAWDZ(!zawiera(sz, "http://") && !zawiera(sz, "https://") && ile(sz, "<script") == 1);
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
  SPRAWDZ(st.find("name=\"uwaga_o_masce\"") > st.find("<summary><b>Sprzęt"));
  std::string se = formularz::strona(formularz::zPliku("", ob), "/u/a/", "", {{"uwaga_o_masce", "Liczba od 10 do 15."}});
  SPRAWDZ(zawiera(se, "Liczba od 10 do 15."));
  // zapis z formularza czyta panel tym samym parserem
  std::string nowa = formularz::scal("humor=oszczedny\n", waliduj("uwaga_o_masce=14").plik);
  SPRAWDZ(parsujUstawieniaDnia(nowa).uwagaOMasce == 14 && parsujUstawieniaDnia(nowa).humor == 1);
}

// ---------- R-KONF-59: układ strony z makiety 14.11.2026 (sekcje zwijane, wartość w nagłówku, przycisk na dole) ----------
static std::string miedzy(const std::string& s, const std::string& od, const std::string& doo, size_t start = 0) {
  size_t a = s.find(od, start);
  if (a == std::string::npos) return "";
  a += od.size();
  size_t b = s.find(doo, a);
  return b == std::string::npos ? "" : s.substr(a, b - a);
}
static double jasnosc(int rgb) {  // WCAG 2.x, względna luminancja koloru #RRGGBB
  double k[3];
  for (int i = 0; i < 3; i++) {
    double c = ((rgb >> (16 - 8 * i)) & 255) / 255.0;
    k[i] = c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
  }
  return 0.2126 * k[0] + 0.7152 * k[1] + 0.0722 * k[2];
}
static double kontrast(int a, int b) {
  double x = jasnosc(a), y = jasnosc(b);
  return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05);
}
static void testR_KONF_59_uklad_strony() {
  sekrety::Dane ob;
  ob.ssid = "Dom-2.4GHz";
  ob.hasloWifi = "TajneHaslo123";
  ob.abAdres = "192.168.1.40";
  ob.abKonto = "kontoXYZ";
  ob.abHaslo = "AbTajne99";
  formularz::Pola p = formularz::zPliku("miasto=Katowice\nwymiana.maska=2026-09-02\n", ob, true);
  p.kluczWydan = true;
  p.hasloOkna = true;
  p.wersja = "1.0.0";
  std::string s = formularz::strona(p, "/u/abc/", "", {});
  // dziewięć sekcji zwijanych (od ADR-0045 także „Ekrany”), każda zamknięta przy pełnej konfiguracji bez błędów
  SPRAWDZ(ile(s, "<details") == 9 && ile(s, "</details>") == 9 && ile(s, "<summary>") == 9 && !zawiera(s, "<details open>"));
  SPRAWDZ(!zawiera(s, "<fieldset") && !zawiera(s, "<legend"));
  // bieżąca wartość w nagłówku sekcji
  SPRAWDZ(zawiera(s, "<summary><b>Sieć domowa</b><span>Dom-2.4GHz</span></summary>"));
  SPRAWDZ(zawiera(s, "<summary><b>AirBridge</b><span>192.168.1.40</span></summary>"));
  SPRAWDZ(zawiera(s, "<summary><b>Pogoda</b><span>Katowice</span></summary>"));
  SPRAWDZ(zawiera(s, "<summary><b>Panel</b><span>profil prywatny</span></summary>"));
  SPRAWDZ(zawiera(s, "<summary><b>Dźwięk i humor</b><span>cisza 22:00–08:00</span></summary>"));
  SPRAWDZ(zawiera(s, "<summary><b>Sprzęt</b><span>maska 02.09.2026</span></summary>"));
  SPRAWDZ(zawiera(s, "<summary><b>Raport dla lekarza</b><span>2 czynniki</span></summary>"));
  SPRAWDZ(zawiera(s, "<summary><b>Aktualizacje</b><span>ręcznie</span></summary>"));
  SPRAWDZ(zawiera(s, "<summary><b>Ekrany</b><span>do 4 stron · 6 ekranów dziennych</span></summary>"));  // ADR-0045
  // nigdy hasło, konto AirBridge ani klucz w nagłówkach; hasła i klucz nigdzie w stronie (R-KONF-38)
  SPRAWDZ(!zawiera(s, "TajneHaslo123") && !zawiera(s, "AbTajne99"));
  size_t z = 0;
  for (int i = 0; i < 9; i++) {
    std::string n = miedzy(s, "<summary>", "</summary>", z);
    z = s.find("</summary>", z) + 1;
    SPRAWDZ(!n.empty() && !zawiera(n, "kontoXYZ") && !zawiera(n, "ustawion") && !zawiera(n, "Tajne"));
  }
  // inne wartości nagłówków: brak sieci i AirBridge (pierwsza konfiguracja) = te sekcje otwarte od razu; wyłączony
  // dźwięk = „Dźwięk i humor” otwarta, ostrzeżenie o sygnale do lekarza widoczne bez dotykania
  formularz::Pola pusty = formularz::zPliku("buzzer=nie\naktualizacja_auto=tak\nraport.czynniki=N\n", sekrety::Dane());
  std::string sp = formularz::strona(pusty, "/u/abc/", "", {});
  SPRAWDZ(zawiera(sp, "<details open><summary><b>Sieć domowa</b><span>nie ustawiona</span>"));
  SPRAWDZ(zawiera(sp, "<details open><summary><b>AirBridge</b><span>nie ustawiony</span>"));
  SPRAWDZ(zawiera(sp, "<details open><summary><b>Dźwięk i humor</b><span>dźwięk wyłączony</span>"));
  SPRAWDZ(ile(sp, "<details open>") == 3);
  SPRAWDZ(zawiera(sp, "<b>Pogoda</b><span>nie wybrano</span>"));
  SPRAWDZ(zawiera(sp, "<b>Sprzęt</b><span>maska: brak daty</span>"));
  {
    formularz::Pola cich = formularz::zPliku("miasto=Katowice\nwymiana.maska=2026-09-02\nbuzzer=nie\n", ob, true);
    std::string sc = formularz::strona(cich, "/u/abc/", "", {});
    size_t o = sc.find("<details open><summary><b>Dźwięk i humor</b><span>dźwięk wyłączony</span>");
    size_t k = sc.find("</details>", o);
    size_t ostrz = sc.find("Dźwięk jest wyłączony: sygnał o sprawie do lekarza (pilność A) nie zadzwoni.");
    SPRAWDZ(ile(sc, "<details open>") == 1 && o != std::string::npos && ostrz > o && ostrz < k);
    SPRAWDZ(!zawiera(s, "Dźwięk jest wyłączony"));  // dźwięk włączony: sekcja zamknięta, bez ostrzeżenia (wyżej)
  }
  SPRAWDZ(zawiera(sp, "<b>Raport dla lekarza</b><span>bez czynników</span>"));
  SPRAWDZ(zawiera(sp, "<b>Aktualizacje</b><span>samodzielnie · bez klucza</span>"));
  // odmiana liczby czynników
  formularz::Pola c = p;
  c.czynniki = "A";
  SPRAWDZ(zawiera(formularz::strona(c, "/u/a/", "", {}), "<span>1 czynnik</span>"));
  c.czynniki = "AKSJL";
  SPRAWDZ(zawiera(formularz::strona(c, "/u/a/", "", {}), "<span>5 czynników</span>"));
  // ucieczka znaków w nagłówku (nazwa sieci od użytkownika)
  formularz::Pola x = p;
  x.ssid = "<i>\"&";
  SPRAWDZ(zawiera(formularz::strona(x, "/u/a/", "", {}), "<span>&lt;i&gt;&quot;&amp;</span>"));
  // sekcja z błędem walidacji otwarta po odesłaniu, pozostałe zamknięte
  std::string sb = formularz::strona(p, "/u/abc/", "", {{"cisza_do", "Początek i koniec ciszy muszą być różne."}});
  SPRAWDZ(ile(sb, "<details open>") == 1 && zawiera(sb, "<details open><summary><b>Dźwięk i humor"));
  struct { const char* pole; const char* sekcja; } bl[] = {
      {"ssid", "Sieć domowa"}, {"wifi_haslo", "Sieć domowa"}, {"ab_adres", "AirBridge"}, {"ab_konto", "AirBridge"},
      {"ab_haslo", "AirBridge"}, {"miasto", "Pogoda"}, {"profil", "Panel"}, {"co2", "Panel"}, {"humor", "Dźwięk i humor"},
      {"buzzer", "Dźwięk i humor"}, {"cisza_od", "Dźwięk i humor"}, {"podpowiedz", "Dźwięk i humor"},
      {"wymiana.maska", "Sprzęt"}, {"okres.maska", "Sprzęt"}, {"wymiana.filtr", "Sprzęt"}, {"okres.filtr_tyg", "Sprzęt"},
      {"nowy_model", "Sprzęt"}, {"uwaga_o_masce", "Sprzęt"}, {"cz_start", "Raport dla lekarza"},
      {"aktualizacja_auto", "Aktualizacje"}, {"klucz_wydan", "Aktualizacje"}, {"haslo_okna", "Aktualizacje"}};
  for (auto& e : bl) {
    std::string se = formularz::strona(p, "/u/abc/", "", {{e.pole, "Błąd testowy."}});
    bool dobrze = ile(se, "<details open>") == 1 && zawiera(se, std::string("<details open><summary><b>") + e.sekcja) &&
                  zawiera(se, "<p class=\"b\">Błąd testowy.</p>");
    if (!dobrze) printf("  sekcja z błędem pola %s nie jest jedyną otwartą\n", e.pole);
    SPRAWDZ(dobrze);
  }
  // błąd pola spoza znanych sekcji: wszystkie sekcje otwarte (błąd nie może zostać ukryty)
  SPRAWDZ(ile(formularz::strona(p, "/u/abc/", "", {{"nieznane", "x"}}), "<details open>") == 9);
  // data filtra z pliku w innej postaci niż RRRR-MM-DD = pole puste (jak po oczyszczeniu przez przeglądarkę), poprawna
  // data bez zmian; zła wartość okresu przycięta do MAKS_OKRESU_Z_PLIKU bajtów bez rozcinania znaku, poprawna bez zmian
  SPRAWDZ(formularz::zPliku("wymiana.filtr=2026-9-1\n", ob).wymiana[1].empty());
  SPRAWDZ(formularz::zPliku("wymiana.filtr=\"><script>\n", ob).wymiana[1].empty());
  SPRAWDZ(formularz::zPliku("wymiana.filtr=2026-10-01\n", ob).wymiana[1] == "2026-10-01");
  SPRAWDZ(formularz::zPliku("okres.maska=6\nokres.filtr_tyg=104\n", ob).okres[0] == "6" &&
          formularz::zPliku("okres.maska=6\nokres.filtr_tyg=104\n", ob).okres[1] == "104");
  SPRAWDZ(formularz::zPliku("okres.filtr_tyg=08\n", ob).okres[1] == "08");
  SPRAWDZ(formularz::zPliku("okres.maska=" + std::string(200, '"') + "\n", ob).okres[0] == std::string(8, '"'));
  SPRAWDZ(formularz::zPliku("okres.filtr_tyg=1234567żżż\n", ob).okres[1] == "1234567");  // `ż` = 2 bajty: bez połówki
  SPRAWDZ(formularz::MAKS_OKRESU_Z_PLIKU > 3);  // przycięta zła wartość zostaje dłuższa niż 3 cyfry, czyli zła
  // okres z pliku poza zakresem pola: „Sprzęt” otwarty (przeglądarka nie zatrzyma zapisu na niewidocznym polu)
  std::string so = formularz::strona(formularz::zPliku("okres.maska=99\n", ob), "/u/abc/", "", {});
  SPRAWDZ(zawiera(so, "<details open><summary><b>Sprzęt") && ile(so, "<details open>") == 1);
  // wszystkie pola (nazwy bez zmian) dokładnie raz, wewnątrz formularza i wewnątrz sekcji
  formularz::Pola w = p;
  w.nowyModelDzis = true;
  std::string sw = formularz::strona(w, "/u/abc/", "", {});
  size_t f0 = sw.find("<form"), f1 = sw.find("</form>");
  std::vector<std::string> nazwy = {"ssid", "wifi_haslo", "wifi_bez", "ab_adres", "ab_konto", "ab_haslo", "ab_bez",
                                    "profil", "co2", "miasto", "humor", "buzzer", "cisza_od", "cisza_do", "podpowiedz",
                                    "cz_start", "wymiana.maska", "okres.maska", "nowy_model", "nowy_model_cofnij",
                                    "wymiana.filtr", "okres.filtr_tyg", "uwaga_o_masce", "aktualizacja_auto",
                                    "klucz_wydan", "klucz_usun", "haslo_okna",
                                    // sekcja „Ekrany” (ADR-0045, R-KONF-60)
                                    "ekrany_start", "kk_start", "dk_start", "kk_ostatnia_noc", "kk_7_nocy", "kk_sypialnia",
                                    "pokaz_sypialnia", "dk_wietrzenie", "dk_jutro_rano", "dk_wymiana", "dk_pylek",
                                    "pokaz_wietrzenie", "pokaz_jutro_rano", "pokaz_wymiana", "pokaz_pylek",
                                    "pokaz_dzien_i_noc", "pokaz_obrazek",
                                    // R-KON-77: wyłącznik wygaszacza i jego czas
                                    "wygaszacz", "wygaszacz_min"};
  for (auto& poz : POZYCJE)
    if (poz.bit != CZ_NIC) nazwy.push_back(std::string("cz_") + poz.kod);
  for (auto& n : nazwy) {
    std::string w1 = "name=\"" + n + "\"";
    size_t at = sw.find(w1);
    size_t d0 = sw.rfind("<details", at), d1 = sw.rfind("</details>", at);
    bool dobrze = ile(sw, w1) == 1 && at > f0 && at < f1 && d0 != std::string::npos && (d1 == std::string::npos || d0 > d1);
    if (!dobrze) printf("  pole %s: brak, powtórzone albo poza sekcją\n", n.c_str());
    SPRAWDZ(dobrze);
  }
  SPRAWDZ(ile(sw.substr(f0, f1 - f0), "name=\"") == nazwy.size());  // żadnego nowego pola wysyłanego do panelu
  // każda etykieta `for` ma pole z tym `id`
  size_t l = 0;
  int etykiet = 0;
  while ((l = sw.find("<label for=\"", l)) != std::string::npos) {
    l += 12;
    std::string id = sw.substr(l, sw.find('"', l) - l);
    etykiet++;
    SPRAWDZ(zawiera(sw, "id=\"" + id + "\""));
  }
  SPRAWDZ(etykiet >= 17);
  // przycisk przyklejony na dole, w formularzu; licznik nad nim ukryty bez skryptu
  SPRAWDZ(zawiera(sw, ".z{position:sticky;bottom:0;"));
  SPRAWDZ(zawiera(sw, "<div class=\"z\"><p id=\"n\" aria-live=\"polite\" hidden></p><button type=\"submit\">Zapisz na panelu</button></div></form>"));
  // po odesłaniu z błędami walidacji licznik oznaczony `data-b` (skrypt pokazuje go dopiero po pierwszej zmianie pola),
  // bez nowych słów na stronie; bez błędów (także z samym komunikatem) bez oznaczenia
  std::string sbl = formularz::strona(p, "/u/abc/", "Popraw pola oznaczone niżej.", {{"ssid", "Wpisz nazwę sieci (najwyżej 32 znaki)."}});
  SPRAWDZ(zawiera(sbl, "<p id=\"n\" aria-live=\"polite\" data-b hidden></p><button type=\"submit\">Zapisz na panelu</button>"));
  SPRAWDZ(!zawiera(formularz::strona(p, "/u/abc/", "Zapisano o 21:14.", {}), " data-b hidden"));
  SPRAWDZ(!zawiera(sw, " data-b hidden"));
  // przyklejony pasek nie zasłania pola z fokusem ani komunikatu błędu pod nim (przewinięcie z zapasem 10em: pasek
  // ok. 7em + dwie linie błędu); sprawdzone w Chrome 390×844 i 390×420 (test przegląd `przegladarka.py`)
  SPRAWDZ(zawiera(sw, "html{scroll-padding-bottom:10em}"));
  SPRAWDZ(ile(sw, "<button") == 1 && ile(sw, "<script>") == 1 && sw.find("<script>") > f1);
  // skrypt liczy zmiany względem stanu pól zapamiętanego przy starcie (nie atrybutów `value`: przeglądarka czyści
  // wartość w innej postaci, a licznik nie może pokazać zmiany, której nie było) i nie wysyła niczego
  std::string js = miedzy(sw, "<script>", "</script>");
  // stan przy samym polu (`_p`, od ADR-0045: przestawienie wierszy strzałkami nie myli licznika)
  SPRAWDZ(zawiera(js, "e[i]._p=w(e[i])") && zawiera(js, "w(x)!==x._p") && !zawiera(js, "defaultValue") &&
          zawiera(js, "hasAttribute('data-b')") && zawiera(js, "n.hidden=h") && zawiera(js, "function u(){h=false;z()}") &&
          zawiera(js, "niezapisana zmiana") && zawiera(js, "niezapisane zmiany") && zawiera(js, "niezapisanych zmian"));
  SPRAWDZ(!zawiera(js, "fetch") && !zawiera(js, "XMLHttpRequest") && !zawiera(js, "action") && !zawiera(js, "submit(") &&
          !zawiera(js, "innerHTML") && !zawiera(js, "http"));
  // wersja w nagłówku strony, raz; bez wersji bez napisu
  SPRAWDZ(ile(sw, "wersja 1.0.0") == 1 && sw.find("<span>wersja 1.0.0</span>") < sw.find("</header>"));
  formularz::Pola bw = p;
  bw.wersja = "";
  SPRAWDZ(!zawiera(formularz::strona(bw, "/u/a/", "", {}), "<span>wersja"));
  bw.wersja = "<x>";
  SPRAWDZ(zawiera(formularz::strona(bw, "/u/a/", "", {}), "<span>wersja &lt;x&gt;</span>"));
  // cele dotyku co najmniej 44 px: nagłówek sekcji 52, przycisk 52, pola 46, pola wyboru w etykiecie 44
  SPRAWDZ(zawiera(sw, "min-height:52px;box-sizing:border-box;padding:.4em 1em") && zawiera(sw, "width:100%;min-height:52px") &&
          zawiera(sw, "input,select{width:100%;min-height:46px") && zawiera(sw, "label.c{display:flex;align-items:center;gap:.6em;min-height:44px"));
  // czarno-biało: w stylu tylko czerń, biel, szary tekst #444 i tło otwartej sekcji #f2f2f2; kontrast tekstu ≥ 4,5:1
  std::string css = miedzy(sw, "<style>", "</style>");
  size_t h = 0;
  int kolorow = 0;
  while ((h = css.find('#', h)) != std::string::npos) {
    size_t k = h + 1;
    while (k < css.size() && isxdigit((unsigned char)css[k])) k++;
    std::string kol = css.substr(h, k - h);
    kolorow++;
    bool znany = kol == "#000" || kol == "#fff" || kol == "#444" || kol == "#f2f2f2";
    if (!znany) printf("  kolor spoza palety: %s\n", kol.c_str());
    SPRAWDZ(znany);
    h = k;
  }
  SPRAWDZ(kolorow > 0);
  SPRAWDZ(kontrast(0x444444, 0xffffff) >= 4.5 && kontrast(0x444444, 0xf2f2f2) >= 4.5 && kontrast(0x000000, 0xf2f2f2) >= 4.5 &&
          kontrast(0xffffff, 0x000000) >= 4.5);
  printf("kontrast: #444 na #fff %.2f:1, #444 na #f2f2f2 %.2f:1\n", kontrast(0x444444, 0xffffff), kontrast(0x444444, 0xf2f2f2));
  // najgorszy przypadek rozmiaru: wszystkie błędy, komunikat, pole cofnięcia, terminy, źródło CO2, długie wartości
  formularz::Pola dl = w;
  dl.ssid = std::string(32, '&');
  dl.abAdres = "airbridge-w-sypialni-na-pietrze.local:65535";
  dl.abKonto = std::string(32, '"');
  dl.termin[0] = "po terminie od 01.01.2026";
  dl.termin[1] = "następna wymiana 01.12.2026";
  dl.czynniki = "AKSJLTP";
  std::map<std::string, std::string> wsz;
  for (auto& e : bl) wsz[e.pole] = "Godzina w postaci GG:MM, np. 22:00.";
  std::string sd = formularz::strona(dl, "/u/0123456789abcdef0123456789abcdef/",
                                     "Panel nie połączył się z AirBridge: sprawdź adres i hasło. Zapisano o 21:14.", wsz);
  SPRAWDZ(sd.size() <= formularz::REZERWACJA_STRONY);
  // najgorszy realny przypadek (scenariusz przegląd): najdłuższe sekrety z samych znaków ucieczki (secrets.h nie
  // przechodzi walidacji formularza), prawdziwe teksty błędów z waliduj() dla każdego pola, najdłuższy komunikat
  // dyrygenta (AirBridge z adresem 64 znaki), pole cofnięcia, oba terminy, wersja z sufiksem; strona mieści się w
  // pierwszej rezerwacji (bez realokacji std::string)
  sekrety::Dane g;
  g.ssid = std::string(32, '"');
  g.hasloWifi = "x";
  g.abAdres = std::string(64, '"');
  g.abKonto = std::string(32, '"');
  g.abHaslo = "x";
  std::map<std::string, std::string> zle_pola = {
      {"ssid", ""}, {"wifi_haslo", "123"}, {"ab_adres", "http://x"}, {"ab_konto", "a\"b"}, {"profil", "zly"},
      {"wymiana.maska", "2099-01-01"}, {"okres.maska", "99"}, {"okres.filtr_tyg", "0"}, {"co2", "x"},
      {"humor", "x"}, {"buzzer", "x"}, {"podpowiedz", "x"}, {"cisza_od", "25:00"}, {"cisza_do", "x"},
      {"miasto", "Atlantyda"}, {"uwaga_o_masce", "9"}, {"aktualizacja_auto", "x"}, {"klucz_wydan", "krotki"},
      {"haslo_okna", "krotkie"}, {"cz_start", "ZZ"},
      // sekcja „Ekrany” (ADR-0045): każde miejsce złe (7 błędów przy polach)
      {"ekrany_start", "1"}, {"kk_start", "ostatnia_noc,7_nocy,sypialnia"}, {"dk_start", "wietrzenie,jutro_rano,wymiana,pylek"},
      {"kk_ostatnia_noc", "0"}, {"kk_7_nocy", "9"}, {"kk_sypialnia", "x"}, {"dk_wietrzenie", "5"}, {"dk_jutro_rano", ""},
      {"dk_wymiana", "-1"}, {"dk_pylek", "<b>"}};
  std::map<std::string, std::string> realne = formularz::waliduj(zle_pola, g, "2026-11-14").bledy;
  realne["wifi_haslo"] = "Nowa sieć: wpisz jej hasło albo zaznacz „bez hasła”.";
  realne["nowy_model"] = "Panel nie zna dzisiejszej daty. Spróbuj po synchronizacji czasu.";
  realne["ab_haslo"] = "Hasło AirBridge: najwyżej 64 znaki.";
  realne["wymiana.filtr"] = "Data w postaci RRRR-MM-DD.";
  realne["okres.filtr_tyg"] = "Liczba tygodni od 1 do 104.";
  formularz::Pola gp = formularz::zPliku(
      "miasto=Gorzów Wielkopolski\nwymiana.maska=2026-09-02\nwymiana.filtr=2026-09-02\nokres.maska=60\n"
      "okres.filtr_tyg=104\nraport.czynniki=KLABCDEFGHIJKLMNOPQRSTUVWXYZ\n",
      g, true);
  gp.hasloWifi = gp.hasloAb = gp.hasloOkna = true;
  gp.nowyModelDzis = true;
  gp.termin[0] = "po terminie od 02.03.2027";
  gp.termin[1] = "po terminie od 02.03.2027";
  gp.wersja = "1.10.100-rc.10+g1234567";
  std::string kom = "AirBridge pod " + std::string(64, '"') +
                    " odrzucił logowanie. Sprawdź konto i hasło. Poprzednie ustawienia zostają. Nie udało się zapisać "
                    "w pamięci panelu.";
  std::string sg = formularz::strona(gp, "/u/ABCDEFGHIJKLMNOPQRSTUVWXYZ234567/", kom, realne);
  SPRAWDZ(ile(sg, "<p class=\"b\">") == realne.size() && realne.size() >= 30 && sg.size() <= formularz::REZERWACJA_STRONY);
  // pliki z wartościami okresu po 200 znaków ucieczki: po przycięciu (MAKS_OKRESU_Z_PLIKU) też w rezerwacji
  formularz::Pola gz = formularz::zPliku("wymiana.filtr=" + std::string(200, '"') + "\nokres.maska=" +
                                             std::string(200, '"') + "\nokres.filtr_tyg=" + std::string(200, '"') + "\n",
                                         g, true);
  std::string sz = formularz::strona(gz, "/u/ABCDEFGHIJKLMNOPQRSTUVWXYZ234567/", kom, realne);
  SPRAWDZ(sz.size() <= formularz::REZERWACJA_STRONY);
  // najgorszy realny przypadek z zapasem co najmniej 1 KB wobec rezerwacji, żeby
  // drobna zmiana tekstów nie dawała realokacji (druga kopia strony na stercie ESP32)
  SPRAWDZ(sg.size() + 1024 <= formularz::REZERWACJA_STRONY);
  // scenariusz przegląd: każde pole formularza z błędem 91 B (najdłuższy tekst błędu, także przy polach, które go nie
  // dają), układ ekranów przestawiony i wszystko ukryte, sekcje otwarte: dalej w rezerwacji (bez realokacji)
  formularz::Pola gr = formularz::zPliku(
      "miasto=Gorzów Wielkopolski\nwymiana.maska=2026-09-02\nwymiana.filtr=2026-09-02\nokres.maska=60\n"
      "okres.filtr_tyg=104\nraport.czynniki=KLABCDEFGHIJKLMNOPQRSTUVWXYZ\nkokpit.kolejnosc=sypialnia,7_nocy\n"
      "kokpit.ukryte=sypialnia\ndzien.ukryte=wietrzenie,jutro_rano,wymiana,pylek,dzien_i_noc,obrazek\n",
      g, true);
  gr.hasloWifi = gr.hasloAb = gr.hasloOkna = true;
  gr.nowyModelDzis = true;
  gr.termin[0] = gr.termin[1] = "po terminie od 02.03.2027";
  gr.wersja = "10.10.100-rc.100+g1234567";
  std::map<std::string, std::string> wszystkie;
  for (const char* pole : {"ssid", "wifi_haslo", "wifi_bez", "ab_adres", "ab_konto", "ab_haslo", "profil", "co2", "miasto",
                           "humor", "buzzer", "cisza_od", "cisza_do", "podpowiedz", "cz_start", "wymiana.maska",
                           "okres.maska", "nowy_model", "nowy_model_cofnij", "wymiana.filtr", "okres.filtr_tyg",
                           "uwaga_o_masce", "aktualizacja_auto", "klucz_wydan", "klucz_usun", "haslo_okna",
                           "kk_ostatnia_noc", "kk_7_nocy", "kk_sypialnia", "dk_wietrzenie", "dk_jutro_rano", "dk_wymiana",
                           "dk_pylek", "ekrany_start"})
    wszystkie[pole] = "Nowa sieć: wpisz jej hasło albo zaznacz „bez hasła”. Panel nie zna dzisiejszej daty.";
  // także błędy przy polach wygaszacza (R-KON-74, R-KON-77) i najdłuższa wartość czasu (720);
  // `bezWyg` = ta sama strona bez błędów przy polach wygaszacza (ile dokładają same błędy tych pól)
  std::string bezWyg = formularz::strona(gr, "/u/ABCDEFGHIJKLMNOPQRSTUVWXYZ234567/", kom, wszystkie);
  for (const char* pole : {"wygaszacz", "wygaszacz_min"})
    wszystkie[pole] = "Nowa sieć: wpisz jej hasło albo zaznacz „bez hasła”. Panel nie zna dzisiejszej daty.";
  gr.wygaszaczMin = "720";
  std::string sr = formularz::strona(gr, "/u/ABCDEFGHIJKLMNOPQRSTUVWXYZ234567/", kom, wszystkie);
  printf("strona formularza, scenariusz brzegowy: z błędami przy polach wygaszacza %zu B, bez nich %zu B (błędy "
         "dokładają %zu B), rezerwacja %zu B, zapas %ld B\n", sr.size(), bezWyg.size(), sr.size() - bezWyg.size(),
         formularz::REZERWACJA_STRONY, (long)formularz::REZERWACJA_STRONY - (long)sr.size());
  SPRAWDZ(sr.size() <= formularz::REZERWACJA_STRONY);
  printf("rozmiar strony formularza (R-KONF-59): %zu B zwykła, %zu B wszystkie błędy, %zu B najgorszy realny (zapas %zu B), "
         "%zu B z przyciętym plikiem, %zu B scenariusz brzegowy (rezerwacja %zu B)\n",
         s.size(), sd.size(), sg.size(), formularz::REZERWACJA_STRONY - sg.size(), sz.size(), sr.size(),
         formularz::REZERWACJA_STRONY);
}

int main() {
  testTabelaMiast();
  testUwagaOMasce();
  testParser();
  testPola();
  testZapis();
  testStrona(true);
  testR_KONF_59_uklad_strony();
  testSesja();
  testPogodaWDyrygencie();
  printf("testy_formularz_u03: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
