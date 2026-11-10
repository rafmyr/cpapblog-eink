// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (ekrany CO2 per egzemplarz, przerywniki S2d). Przypadki
// R17C-nn padają z numerem do poprawki; testy „trzyma” pilnują zachowań potwierdzonych. Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "dzien.h"
#include "humor.h"
#include "noc.h"
#include "slot_dzienny.h"
#include "sprawy.h"
#include "start.h"
#include "wspolne.h"
#include "atrapy/zestaw_kolejki.h"

static int ok = 0, zle = 0;
#define PRZYPADEK(id, w)                                                       \
  do {                                                                          \
    if (w) ok++;                                                                \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", id, __FILE__, __LINE__, #w); } \
  } while (0)
#define TRZYMA(w)                                                               \
  do {                                                                          \
    if (w) ok++;                                                                \
    else { zle++; printf("REGRES (%s:%d): %s\n", __FILE__, __LINE__, #w); }     \
  } while (0)

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";
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
static uint32_t epDnia(const std::string& data, int h, int m) {
  return ep(atoi(data.substr(0, 4).c_str()), atoi(data.substr(5, 2).c_str()), atoi(data.substr(8, 2).c_str()), h, m);
}
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";

// Raport nocy jak test/testy_ekrany_co2.cpp raport(): spokojna noc 7 h, AHI 1,4.
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
         "\"option\":8},{\"id\":\"Leak\",\"label\":\"Leak\",\"unit\":\"L/min\",\"count\":12600,"
         "\"coverage_seconds\":25200,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":20.0,\"option\":8}]}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}

// ---------- atrapa silnika: wzorzec L-11 (pilność B) aktywny albo nic ----------
static reguly::Zestaw ZESTAW = atrapa_b5::zestaw();
static std::vector<std::string> KLUCZE;
static std::vector<reguly::Wynik> wyniki(const reguly::Kontekst&) {
  std::vector<reguly::Wynik> w;
  for (auto& k : ZESTAW.katalog) {
    std::string kl = k.wariant.empty() ? k.id : k.id + "/" + k.wariant;
    bool sp = std::find(KLUCZE.begin(), KLUCZE.end(), kl) != KLUCZE.end();
    w.push_back(atrapa_b5::wynik(ZESTAW, k.id, k.wariant, k.stan != reguly::StanReguly::ZABLOKOWANA, sp,
                                 {{"x", "1,4"}, {"n", "3"}, {"min", "42"}, {"m", "27"}}));
  }
  return w;
}

// Ekran dnia z logu panelu: „ekran dnia S2A (3): karta G10” → scena przerywnika (G..) albo "".
struct Rano {
  std::string data;
  int minuta = 0;
  std::string scena;  // przerywnik G.. na ekranie dnia
  bool ekranL = false;
};
static std::vector<Rano> przebieg(int dni, bool wzorzecB, std::vector<std::string>* log = nullptr) {
  KLUCZE = wzorzecB ? std::vector<std::string>{"L-11"} : std::vector<std::string>{};
  Panel p;
  const std::string start = "2026-11-06";  // piątek: początek pracy egzemplarza testowego
  const long pierwsza = nrDnia("2026-09-20");
  std::vector<Rano> r;
  bool pierwszy = true;
  for (int d = 0; d < dni; d++) {
    std::string dzis = dodajDni(start, d);
    long doba = nrDnia(dzis) - 1;
    for (int h : {6, 7, 9}) {
      p.siec.odpowiedzi.clear();
      p.siec.siecJest = true;
      p.siec.dodaj("/api/status", 200, STATUS);
      p.siec.dodaj(sciezkaListy(), 200, lista(pierwsza, doba));
      for (long x = pierwsza; x <= doba; x++) {
        std::string dx = dataZNr(x);
        p.siec.dodaj(sciezkaNocy(x), 200, raport(x, epDnia(dx, 23, 0), epDnia(dodajDni(dx, 1), 5, 30)));
      }
      p.zegar.epoch = epDnia(dzis, h, 0);
      p.zegar.msTeraz = 0;
      p.start.przyczynaStartu = pierwszy ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
      p.start.powodWybudzenia = pierwszy ? PowodWybudzenia::START : PowodWybudzenia::ZEGAR;
      pierwszy = false;
      zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
      p.port.log.clear();
      Dyrygent(p.adaptery(), KONF).wybudzenie();
      Rano x;
      x.data = dzis;
      x.minuta = h * 60;
      for (auto& l : p.port.log) {
        if (log) log->push_back(l);
        size_t k = l.find("ekran dnia ");
        size_t g = l.find(": karta G");
        if (k != std::string::npos && g != std::string::npos) x.scena = l.substr(g + 8, 3);
        if (l.find("ekran dnia: wzorzec L-11") != std::string::npos) x.ekranL = true;
      }
      r.push_back(x);
    }
  }
  return r;
}

// ---------- R-HUM-26 w dyrygencie (przy aktywnym wzorcu B bez przerywnika) ----------
static void testWzorzecB() {
  sprawy::zrodloTestowe.wyniki = wyniki;
  sprawy::zrodloTestowe.zestaw = &ZESTAW;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW);
  // kontrola: bez wzorca przerywnik jest na panelu (tylko rano, jeden dzień na 7)
  std::vector<Rano> bez = przebieg(16, false);
  std::set<std::string> dniPrz;
  for (auto& x : bez)
    if (!x.scena.empty()) dniPrz.insert(x.data);
  TRZYMA(!dniPrz.empty());
  std::string pop;
  for (auto& d : dniPrz) {
    if (!pop.empty()) TRZYMA(nrDnia(d) - nrDnia(pop) >= 7);  // R-HUM-14
    pop = d;
  }
  // wzorzec L-11 (pilność B, ADR-0029) aktywny przez cały przebieg: R-HUM-26 = ani jednej karty przerywnika
  std::vector<std::string> log;
  std::vector<Rano> zB = przebieg(16, true, &log);
  bool ekranL = false;
  std::set<std::string> przyB;
  for (auto& x : zB) {
    ekranL = ekranL || x.ekranL;
    if (!x.scena.empty()) przyB.insert(x.data + " " + x.scena);
  }
  TRZYMA(ekranL);  // wzorzec naprawdę wszedł (ekran L)
  for (auto& s : przyB) printf("  przerywnik %s przy aktywnym wzorcu B (L-11)\n", s.c_str());
  PRZYPADEK("R-HUM-26 w dyrygencie", przyB.empty());
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow{};
}

// ---------- etykieta `Spokojny tydzień` dzień po ekranie problemu / „pokaż lekarzowi” ----------
static KontekstHumoru kontekst(const std::string& dzis) {
  KontekstHumoru k;
  k.dzis = dzis;
  k.sceny = sceny();
  k.zPropozycjami = true;
  return k;
}
static void testEtykieta() {
  UstawieniaPrzerywnikow u = ustawieniaPrzerywnikowBety();
  // wczoraj ekran zdarzenia S3 (problem), przedwczoraj ekran L; dziś pierwszy dzień dowodu spokoju
  std::vector<WpisHumoru> log = {{"2026-11-10", "L", "L-11", 'D', "", ""}, {"2026-11-11", "S3", "E-23", 'D', "", ""}};
  Przerywnik p = wybierzPrzerywnik(EkranDnia{"S2A", "1", false, false}, log, kontekst("2026-11-12"), u);
  TRZYMA(p.jest);
  TekstPrzerywnika t = tekstPrzerywnika(p.scena, p.warunkowa);
  // bez danych spoza ostatniej nocy, ADR-0029 pkt 7: brak oceny ≠ „w porządku”; `Spokojny tydzień` ocenia
  // 7 dni, w których panel pokazał problem i „pokaż lekarzowi”
  // karta przerywnika nie ma etykiety, więc `Spokojny tydzień` nie może wrócić w tytule
  printf("  %s, tytuł „%s” po S3 wczoraj i ekranie L przedwczoraj\n", p.scena.c_str(), t.tytul.c_str());
  PRZYPADEK("etykieta `Spokojny tydzień` dzień po ekranie problemu /", !t.tytul.empty() && t.tytul.find("Spokojny tydzień") == std::string::npos && !p.warunkowa);
}

// ---------- trzyma: ustawienie co2, przerywnik tylko w dowodzie spokoju i bez linii ----------
static void testTrzyma() {
  // zła pisownia klucza: pominięta z wpisem w `bledy`, decyduje źródło
  UstawieniaDnia a = parsujUstawieniaDnia("co2=Nie\n");
  TRZYMA(a.co2 == -1 && a.bledy.size() == 1 && co2Wlaczone(true, a.co2));
  UstawieniaDnia b = parsujUstawieniaDnia("co2=\n");
  TRZYMA(b.co2 == -1 && b.bledy.size() == 1);
  TRZYMA(!co2Wlaczone(false, parsujUstawieniaDnia("co2=tak\n").co2));
  // ostatni wiersz wygrywa (dwa wiersze w pliku)
  TRZYMA(parsujUstawieniaDnia("co2=tak\nco2=nie\n").co2 == 0);
  UstawieniaPrzerywnikow u = ustawieniaPrzerywnikowBety();
  std::vector<WpisHumoru> pusty;
  // każdy typ spoza dowodu spokoju: bez przerywnika (nie wypiera innego ekranu, 5.7.6)
  for (const char* t : {"S1", "S3", "S5", "S10", "L", "KM", "SK10", "SK16", "SK31", "SK18", "SK01", "SK13", "S7", "S8"})
    TRZYMA(!wybierzPrzerywnik(EkranDnia{t, "x", false, false}, pusty, kontekst("2026-11-12"), u).jest);
  for (const char* t : {"S2A", "S2B", "S2C"})
    TRZYMA(wybierzPrzerywnik(EkranDnia{t, "1", false, false}, pusty, kontekst("2026-11-12"), u).jest);
  // Wielkanoc 2027 (28.03, razem z wiosenną zmianą czasu) → G01 (niższe ID niż G06); 2028: 16.04 → G06; 1.01 → G04
  TRZYMA(okazjaDnia("2027-03-28") == "G01" && okazjaDnia("2028-03-26") == "G01" && okazjaDnia("2028-04-16") == "G06" && okazjaDnia("2027-01-01") == "G04");
  TRZYMA(okazjaDnia("2026-10-25") == "G01" && okazjaDnia("2026-10-18") == "");
  // fakty przerywnika: tylko ostatnia noc, brak = „–”
  std::vector<Noc> h(1);
  h[0].data = "2026-11-12";
  h[0].m01 = -1;
  DaneDnia d;
  d.dzis = "2026-11-12";
  d.hist = &h;
  std::vector<Fakt> f = faktyPrzerywnika(d);
  TRZYMA(f.size() == 2 && f[0].w == "–" && f[1].w == "–");
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testTrzyma();
  testEtykieta();
  testWzorzecB();
  printf("testy_ekrany_co2_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
