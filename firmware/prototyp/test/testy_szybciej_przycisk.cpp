// SPDX-License-Identifier: GPL-3.0-or-later
// Szybsza zmiana ekranu po przycisku (pomiar na panelu 16.11.2026, CHANGELOG): liczniki kosztownych przebiegów na
// ścieżce „przycisk → rysowanie” z historią 366 nocy i równoważność klatek oraz stanu pamięci z pamięcią podręczną
// i bez niej (szybkosc::bezPamieci). Wymaganie: T-WYM-99 (spec techniczna), budżet R-NF czasu reakcji bez zmian.
//
// Liczniki (atrapy, bez zegara procesora): odczyty NVS `cpap/sp_model`, wywołania i obliczenia stanyZPamieci(),
// przebiegi silnika reguł (wiersz `silnik reguł` w logu). Czas na PC tylko wypisywany (informacyjnie).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "czynniki.h"
#include "dyrygent.h"
#include "magazyn.h"
#include "nawigacja.h"
#include "noc.h"
#ifndef BAZA
#include "liczba_pol.h"
#include "reguly.h"
#include "reguly_pomoc.h"
#include "reguly_sygnaly.h"
#endif
#include "reguly_pamiec.h"
#include "sprawy.h"
#include "stany.h"
#include "start.h"
#ifndef BAZA
#include "szybkosc.h"
#else
// -DBAZA: ten sam test zbudowany na kodzie sprzed zmiany (porównanie skrótów, tylko ręcznie): liczniki puste
namespace szybkosc {
static bool bezPamieci = false;
static unsigned long przebiegiSilnika = 0;
}  // namespace szybkosc
struct LicznikiStanowBaza {
  unsigned long wywolan = 0, obliczen = 0;
};
static LicznikiStanowBaza& licznikiStanow() {
  static LicznikiStanowBaza l;
  return l;
}
#endif
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)

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

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";  // jak test/testy_plan_wybudzen.cpp
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";

// Raport doby (jak test/testy_dyrygenta.cpp raport()), z kanałami do pól wersji 5.
static std::string raport(long day) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":4.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":25,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Press\",\"label\":\"C\",\"unit\":\"cmH2O\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":6.2,\"mean\":10.1,\"median\":9.8,\"p95\":13.6,\"max\":16.0,"
         "\"source\":\"edf\"},{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":19.6,\"max\":30.2,"
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

// Historia syntetyczna (stałe ziarno): starsza część z karty (K), nowsza z AirBridge (A) z polami wersji 5,
// tryb ASVAuto; kilka nocy nieważnych i częściowych, wahania wokół normy, żeby reguły miały co liczyć.
static uint32_t ziarno = 20261116u;
static float los(float a, float b) {
  ziarno = ziarno * 1664525u + 1013904223u;
  return a + (b - a) * (float)((ziarno >> 8) & 0xFFFF) / 65535.0f;
}
static Noc nocLos(const std::string& d, int i, int n) {
  Noc r;
  r.data = d;
  r.wazna = i % 37 != 5;
  r.m01 = (int)los(300, 480);
  r.ahi = los(0.6f, i > n - 20 ? 9.0f : 4.0f);
  r.cmed = los(9.0f, 10.8f);
  r.cp95 = los(12.0f, 13.8f);
  r.wmed = los(1.5f, 4.5f);
  r.wp95 = los(7.0f, i > n - 10 ? 30.0f : 14.0f);
  r.emed = los(6.0f, 7.2f);
  r.ep95 = los(7.5f, 8.8f);
  r.psmed = los(3.0f, 5.0f);
  r.psp95 = los(6.0f, 8.0f);
  r.zrodlo = i < n / 3 ? 'K' : 'A';
  r.oa = los(0.1f, 1.0f);
  r.ca = los(0.5f, i > n - 15 ? 8.0f : 3.0f);
  r.h = los(0.2f, 2.0f);
  r.csr = los(0.0f, 20.0f);
  if (r.zrodlo == 'A') {
    r.import = ep(2026, 1, 1, 7, 0) + (uint32_t)i * 86400u;
    r.m13 = los(12.0f, 17.0f);
    r.m17 = (int)los(0, 3);
    r.pna = "AirCurve_10_CS_PaceWave";
    r.srn = "23222345678";
    r.m08 = los(0.0f, 30.0f);
    r.eai = 5.0f;
    r.eax = 10.0f;
    r.ans = 3.0f;
    r.axs = 10.0f;
    r.ua = los(0.0f, 1.0f);
    r.m10 = los(0.0f, 0.1f);
    r.m10p = los(0.1f, 0.3f);
    r.tryb = "ASVAuto";
    r.ustawienia = "ASVAuto/EAI5/EAX10/ANS3/AXS10";
    r.m18 = (int)los(600, 760);
    r.m14 = los(400.0f, 600.0f);
    r.m15 = los(6.0f, 9.0f);
  }
  return r;
}

static void bezLogu(const std::string&) {}

// Panel po wielu dobach: `nocy` nocy do `ostatnia` włącznie w plikach rocznych (Magazyn, wersja 5).
static void przygotuj(Panel& p, const std::string& ostatnia, int nocy = 366) {
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  ziarno = 20261116u;
  p.pliki.montowane = true;  // zapis przez Magazyn jak na zamontowanej partycji; dyrygent montuje sam
  Magazyn mag(p.pliki, bezLogu);
  std::map<std::string, std::vector<std::string>> lata;
  for (int i = 0; i < nocy; i++) {
    std::string d = dodajDni(ostatnia, i - (nocy - 1));
    lata[d.substr(0, 4)].push_back(wierszNocyV5(nocLos(d, i, nocy)));
  }
  for (auto& kv : lata) SPRAWDZ(mag.zapisz("noce", mag.sciezkaRoku("noce", kv.first), kv.second));
  p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  p.pliki.montowane = false;
  p.d.z.clear();
}

struct Liczniki {
  int spModel = 0;                       // odczyty NVS `cpap/sp_model`
  unsigned long stanyWyw = 0, stanyObl = 0;  // stanyZPamieci(): wywołania, przebiegi liczStany()
  int silnik = 0;                        // przebiegi silnika reguł (szybkosc::przebiegiSilnika)
  int wierszeSilnika = 0;                // wiersze `silnik reguł` w logu (zostają jak przed zmianą)
  int odswiezen = 0;                     // odświeżenia e-ink w tym wybudzeniu
  double ms = 0;                         // czas wybudzenia na PC (informacyjnie)
  void dodaj(const Liczniki& x) {
    spModel += x.spModel;
    stanyWyw += x.stanyWyw;
    stanyObl += x.stanyObl;
    silnik += x.silnik;
    wierszeSilnika += x.wierszeSilnika;
    odswiezen += x.odswiezen;
    ms += x.ms;
  }
};

static int wierszySilnika(const Panel& p) {
  int n = 0;
  for (auto& l : p.port.log) n += l.find("silnik reguł ") != std::string::npos;
  return n;
}

// Jedno wybudzenie: przycisk (budzący ▶ i skrypt dalszych naciśnięć) albo zegar; liczniki z różnicy przed i po.
static Liczniki wybudz(Panel& p, uint32_t epoch, PowodWybudzenia w, uint8_t budzace = 0,
                       std::vector<std::pair<uint32_t, uint8_t>> skrypt = {},
                       std::vector<std::pair<int, uint32_t>> kolejka = {}) {
  p.zegar.epoch = epoch;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = w == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = w;
  p.start.budzace = budzace;
  p.przyciski.skrypt = skrypt;
  p.przyciski.kolejka = kolejka;
  Liczniki l0;
  l0.spModel = p.nvs.odczyty["cpap/sp_model"];
  l0.stanyWyw = licznikiStanow().wywolan;
  l0.stanyObl = licznikiStanow().obliczen;
  l0.silnik = (int)szybkosc::przebiegiSilnika;
  l0.wierszeSilnika = wierszySilnika(p);
  l0.odswiezen = p.ekran.odswiezen;
  auto t0 = std::chrono::steady_clock::now();
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  auto t1 = std::chrono::steady_clock::now();
  Liczniki l;
  l.spModel = p.nvs.odczyty["cpap/sp_model"] - l0.spModel;
  l.stanyWyw = licznikiStanow().wywolan - l0.stanyWyw;
  l.stanyObl = licznikiStanow().obliczen - l0.stanyObl;
  l.silnik = (int)szybkosc::przebiegiSilnika - l0.silnik;
  l.wierszeSilnika = wierszySilnika(p) - l0.wierszeSilnika;
  l.odswiezen = p.ekran.odswiezen - l0.odswiezen;
  l.ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  return l;
}

// Skrót FNV-1a stanu panelu po wybudzeniu: każda klatka e-ink, NVS, pliki, log portu, ekran i chwila ekranu w RTC.
struct Skrot {
  uint64_t k = 1469598103934665603ULL;
  void bajty(const void* d, size_t n) {
    const unsigned char* b = (const unsigned char*)d;
    for (size_t i = 0; i < n; i++) { k ^= b[i]; k *= 1099511628211ULL; }
    k ^= 0xFF; k *= 1099511628211ULL;
  }
  void tekst(const std::string& t) { bajty(t.data(), t.size()); }
};
static uint64_t skrotKlatek(const Panel& p) {
  Skrot s;
  for (auto& k : p.ekran.klatki) s.bajty(k.data(), k.size());
  return s.k;
}
static uint64_t skrotStanu(const Panel& p) {
  Skrot s;
  for (auto& kv : p.nvs.teksty) { s.tekst(kv.first); s.tekst(kv.second); }
  for (auto& kv : p.nvs.liczbyU) { s.tekst(kv.first); s.bajty(&kv.second, sizeof kv.second); }
  for (auto& kv : p.nvs.liczby) { s.tekst(kv.first); s.bajty(&kv.second, sizeof kv.second); }
  for (auto& kv : p.nvs.flagi) { s.tekst(kv.first); s.bajty(&kv.second, sizeof kv.second); }
  for (auto& kv : p.nvs.bajtyMap) { s.tekst(kv.first); s.tekst(kv.second); }
  for (auto& kv : p.pliki.pliki) { s.tekst(kv.first); s.tekst(kv.second); }
  for (auto& l : p.port.log) s.tekst(l);
  s.bajty(&p.start.stan.ekran, sizeof p.start.stan.ekran);
  s.bajty(&p.start.stan.ekranOd, sizeof p.start.stan.ekranOd);
  return s.k;
}

static std::vector<std::pair<uint32_t, uint8_t>> naciski(const std::vector<uint8_t>& kolejne, uint32_t odstep = 3000) {
  std::vector<std::pair<uint32_t, uint8_t>> s = {{0, BIT_PRAWY}, {300, 0}};
  uint32_t t = odstep;
  for (uint8_t b : kolejne) {
    s.push_back({t, b});
    s.push_back({t + 200, 0});
    t += odstep;
  }
  return s;
}
static const long DOBA_0311 = nrDnia("2026-11-03");  // dataPoranka() = 2026-11-04

// Wynik scenariusza: skrót klatek i stanu po każdym wybudzeniu oraz liczniki.
struct WynikScen {
  std::vector<uint64_t> klatki, stan;
  std::vector<Liczniki> liczniki;
  void po(const Panel& p, const Liczniki& l) {
    klatki.push_back(skrotKlatek(p));
    stan.push_back(skrotStanu(p));
    liczniki.push_back(l);
  }
};

static const std::vector<uint8_t> KOKPIT_TAM_I_Z_POWROTEM = {BIT_PRAWY, BIT_PRAWY, BIT_PRAWY, BIT_PRAWY, BIT_PRAWY,
                                                             BIT_PRAWY, BIT_LEWY,  BIT_LEWY,  BIT_LEWY};
static const int SCENARIUSZY = 7;
static const char* NAZWA_SCEN[SCENARIUSZY] = {
    "poranek z nową nocą, potem przycisk", "przycisk bez nowych danych: kokpit ▶ i ◀",
    "kolejka 3 naciśnięć w czasie rysowania", "zmiana dnia między wybudzeniami",
    "zmiana konfiguracji między wybudzeniami", "odpowiedź, czynnik i nowy model maski między wybudzeniami",
    "„wiem” i „Omówione” w pamięci reguł między wybudzeniami"};

static WynikScen scenariusz(int nr) {
  WynikScen w;
  Panel p;
  const uint8_t P = BIT_PRAWY;
  if (nr == 0) {
    przygotuj(p, "2026-11-03");
    p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    p.siec.dodaj(sciezkaListy(), 200, lista(DOBA_0311 - 3, DOBA_0311));
    for (long d = DOBA_0311 - 3; d <= DOBA_0311; d++) p.siec.dodaj(sciezkaNocy(d), 200, raport(d));
    w.po(p, wybudz(p, ep(2026, 11, 4, 7, 0), PowodWybudzenia::ZEGAR));
    w.po(p, wybudz(p, ep(2026, 11, 4, 7, 10), PowodWybudzenia::PRZYCISK, P, naciski({P, P})));
    return w;
  }
  przygotuj(p, "2026-11-04");
  if (nr == 1) {
    w.po(p, wybudz(p, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK, P, naciski(KOKPIT_TAM_I_Z_POWROTEM)));
  } else if (nr == 2) {
    w.po(p, wybudz(p, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK, P, naciski({}),
                   {{(int)Przycisk::PRAWY, 120}, {(int)Przycisk::PRAWY, 120}, {(int)Przycisk::PRAWY, 120}}));
  } else if (nr == 3) {
    w.po(p, wybudz(p, ep(2026, 11, 4, 23, 50), PowodWybudzenia::PRZYCISK, P, naciski({P})));
    w.po(p, wybudz(p, ep(2026, 11, 5, 0, 10), PowodWybudzenia::PRZYCISK, P, naciski({P})));
    w.po(p, wybudz(p, ep(2026, 11, 5, 9, 0), PowodWybudzenia::PRZYCISK, P, naciski({P})));
  } else if (nr == 4) {
    w.po(p, wybudz(p, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK, P, naciski({P})));
    p.pliki.pliki["/konfiguracja.txt"] =
        "noc_zrodlo=json\nwymiana.filtr=2026-10-07\nokres.filtr_tyg=4\nkokpit.kolejnosc=k2,k1\n";
    w.po(p, wybudz(p, ep(2026, 11, 4, 12, 10), PowodWybudzenia::PRZYCISK, P, naciski({P, P})));
  } else if (nr == 5) {
    w.po(p, wybudz(p, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK, P, naciski({P})));
    p.pliki.montowane = true;
    Magazyn mag(p.pliki, bezLogu);
    sprawy::Odpowiedz o;
    o.data = "2026-11-02";
    o.pytanie = sprawy::Pytanie::INFEKCJA;
    o.odp = 't';
    SPRAWDZ(mag.zapisz("odpowiedzi", "/dane/odpowiedzi.csv", {sprawy::wierszOdpowiedzi(o)}));
    w.po(p, wybudz(p, ep(2026, 11, 4, 12, 10), PowodWybudzenia::PRZYCISK, P, naciski({P})));
    WpisCzynnikow c;
    c.data = "2026-11-03";
    c.czynniki = CZ_ALKOHOL;
    p.pliki.montowane = true;
    SPRAWDZ(mag.zapisz("czynniki", "/dane/czynniki.csv", {zapiszWpis(c)}));
    w.po(p, wybudz(p, ep(2026, 11, 4, 12, 20), PowodWybudzenia::PRZYCISK, P, naciski({P})));
    p.nvs.teksty["cpap/sp_model"] = "2026-10-20";
    w.po(p, wybudz(p, ep(2026, 11, 4, 12, 30), PowodWybudzenia::PRZYCISK, P, naciski({P})));
  } else if (nr == 6) {
    w.po(p, wybudz(p, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK, P, naciski({P})));
    p.pliki.montowane = true;
    Magazyn mag(p.pliki, bezLogu);
    reguly::Pamiec pm = reguly::czytajPamiec(mag);
    int k = 0;
    for (auto& kv : pm) {
      if (k == 0) kv.second.wiem = "2026-11-04";
      if (k == 1) kv.second.omowione = "2026-11-04";
      k++;
    }
    SPRAWDZ(pm.size() >= 2 && reguly::zapiszPamiec(mag, pm));
    printf("  (pamięć reguł: %d wpisów, „wiem” i „Omówione” na dwóch pierwszych)\n", (int)pm.size());
    w.po(p, wybudz(p, ep(2026, 11, 4, 12, 10), PowodWybudzenia::PRZYCISK, P, naciski({P})));
  }
  if (getenv("SZYB_LOG")) {
    for (auto& l : p.port.log) printf("      | %s\n", l.c_str());
    for (auto& kv : p.pliki.pliki) printf("      plik %s (%d B)\n", kv.first.c_str(), (int)kv.second.size());
  }
  return w;
}

static void wypisz(const char* co, const Liczniki& l) {
  printf("    %-28s odczyty sp_model %3d, stany() %4lu wywołań / %2lu obliczeń, "
         "silnik %d (wierszy w logu %d), odświeżeń %2d, %7.1f ms na PC\n",
         co, l.spModel, l.stanyWyw, l.stanyObl, l.silnik, l.wierszeSilnika, l.odswiezen, l.ms);
}

// wymaganie: T-WYM-99
static void testT_WYM_99_licznikiIRownowaznosc() {
  for (int nr = 0; nr < SCENARIUSZY; nr++) {
    szybkosc::bezPamieci = true;
    WynikScen bez = scenariusz(nr);
    szybkosc::bezPamieci = false;
    WynikScen z = scenariusz(nr);
    printf("  scenariusz %d: %s\n", nr, NAZWA_SCEN[nr]);
    Liczniki sb, sz;
    for (auto& l : bez.liczniki) sb.dodaj(l);
    for (auto& l : z.liczniki) sz.dodaj(l);
    wypisz("bez pamięci podręcznej:", sb);
    wypisz("z pamięcią podręczną:", sz);
    if (getenv("SZYB_SKROTY"))
      for (size_t i = 0; i < z.klatki.size(); i++)
        printf("    SKROT %d.%d klatki %016llx stan %016llx\n", nr, (int)i, (unsigned long long)z.klatki[i],
               (unsigned long long)z.stan[i]);
    bool rowneKlatki = bez.klatki == z.klatki, rownyStan = bez.stan == z.stan;
    printf("    klatki %s, stan pamięci %s (%d wybudzeń, %d klatek)\n", rowneKlatki ? "równe" : "RÓŻNE",
           rownyStan ? "równy" : "RÓŻNY", (int)z.klatki.size(), sz.odswiezen);
    SPRAWDZ(rowneKlatki);
    SPRAWDZ(rownyStan);
    SPRAWDZ(sz.odswiezen >= 1);
    SPRAWDZ(sz.wierszeSilnika == sb.wierszeSilnika);  // log silnika jak przed zmianą
    for (size_t i = 0; i < z.liczniki.size(); i++) {
      const Liczniki& l = z.liczniki[i];
      // górne limity na jedno wybudzenie (niezależnie od liczby stron i naciśnięć)
      SPRAWDZ(l.spModel <= 1);       // odczyt NVS `sp_model` raz na wybudzenie
      // silnik reguł: pierwsza ocena i najwyżej jedna po zmianie pamięci reguł przez tę ocenę (aktualizujPamiec,
      // wzorce::aktualizuj: silnik czyta pamięć, więc wynik może być inny; tak było przed zmianą); nie rośnie
      // z liczbą stron ani naciśnięć
      SPRAWDZ(l.silnik <= 2);
      SPRAWDZ(l.stanyObl <= 2);      // liczStany(): raz, drugi raz tylko po zmianie danych w wybudzeniu
    }
  }
  // ścieżka „kolejna strona w czuwaniu” i kolejka: liczba wywołań stanów nie zależy od liczby stron
  szybkosc::bezPamieci = false;
  WynikScen jedna = scenariusz(2), kokpit = scenariusz(1);
  printf("  na wybudzenie: kolejka 3 naciśnięć: stany() %lu, silnik %d; kokpit 9 stron: stany() %lu, silnik %d\n",
         jedna.liczniki[0].stanyWyw, jedna.liczniki[0].silnik, kokpit.liczniki[0].stanyWyw, kokpit.liczniki[0].silnik);
  SPRAWDZ(kokpit.liczniki[0].silnik == jedna.liczniki[0].silnik);
  SPRAWDZ(kokpit.liczniki[0].spModel == 1 && jedna.liczniki[0].spModel == 1);
  SPRAWDZ(kokpit.liczniki[0].stanyObl <= 1);
}

#ifndef BAZA
// Mutacje sterowane formatem pliku: każda kolumna wierszNocyV5() zmieniona osobno i sparsowana;
// jeśli rekord po parsowaniu jest inny, skrotNocy() też musi być inny. Każde pole Noc ma kolumnę, więc liczba kolumn
// zmieniających rekord = liczba pól Noc (liczba_pol.h): nowe pole bez kolumny albo bez wpisu w skrócie wywraca test.
// wymaganie: T-WYM-99
static void testT_WYM_99_kolumnyNocy() {
  ziarno = 7u;
  Noc wz = nocLos("2026-10-15", 300, 366);
  wz.epr = 2.0f;
  wz.mpa = 1.5f; wz.mpi = 2.5f; wz.ipc = 3.5f; wz.eep = 4.5f; wz.mns = 5.5f; wz.mxs = 6.5f;
  Noc baza;
  SPRAWDZ(parsujNocV5(wierszNocyV5(wz), baza));
  const std::string wb = wierszNocyV5(baza);
  const uint64_t kb = skrotNocy(SKROT_POCZATEK, baza);
  std::vector<std::string> kol;
  for (size_t i = 0, j; i <= wb.size(); i = j + 1) {
    j = wb.find(';', i);
    if (j == std::string::npos) j = wb.size();
    kol.push_back(wb.substr(i, j - i));
  }
  int zmienione = 0, widziane = 0;
  std::string bez;
  for (size_t i = 0; i < kol.size(); i++) {
    std::vector<std::string> kand = {"7", "1", "0", "2", "2026-10-20", "K", "A", "S", "ASV", "13.5", "1790000123",
                                     kol[i] + "1", "X" + kol[i]};
    bool zmiana = false;
    for (auto& c : kand) {
      if (c == kol[i]) continue;
      std::string w;
      for (size_t k = 0; k < kol.size(); k++) w += (k ? ";" : "") + (k == i ? c : kol[k]);
      Noc n;
      if (!parsujNocV5(w, n) || wierszNocyV5(n) == wb) continue;
      zmiana = true;
      if (skrotNocy(SKROT_POCZATEK, n) != kb) widziane++;
      else printf("BŁĄD: kolumna %zu (%s → %s) zmienia rekord, a skrotNocy() nie\n", i, kol[i].c_str(), c.c_str());
      break;
    }
    if (zmiana) zmienione++;
    else bez += " " + std::to_string(i);
  }
  printf("  kolumny wierszNocyV5(): %zu, zmieniają rekord %d, skrót widzi %d, pola Noc %zu%s%s\n", kol.size(),
         zmienione, widziane, liczba_pol::policz<Noc>(), bez.empty() ? "" : "; bez zmiany rekordu:", bez.c_str());
  SPRAWDZ(widziane == zmienione);
  SPRAWDZ((size_t)zmienione == liczba_pol::policz<Noc>());
}

// Mutacje wejść reguly::kluczOceny() pole po polu. Listy pól mają static_assert z liczbą pól
// struktury (liczba_pol.h): nowe pole w Kontekst, Sygnaly, Odpowiedzi albo PamiecReguly zatrzymuje kompilację testu,
// dopóki nie dostanie tu mutacji (i wpisu w kluczu albo statusu pola pochodnego).
struct WejscieKlucza {
  std::vector<Noc> h;
  reguly::Odpowiedzi odp;
  reguly::Pamiec pam;
  reguly::Sygnaly syg;
  std::set<std::string> wyk;
  Stany st;
  std::map<std::string, reguly::Wynik> wcz;
  reguly::Kontekst k;
  std::string sp = "2026-10-10";
  bool bezSygnalow = false;
  WejscieKlucza() {
    ziarno = 11u;
    for (int i = 0; i < 3; i++) h.push_back(nocLos(dodajDni("2026-11-04", i - 2), 300 + i, 366));
    odp.sennosc["2026-11-01"] = true;
    odp.gorzej["2026-11-02"] = false;
    odp.przerwaPlanowana.insert("2026-11-03");
    reguly::PamiecReguly pr;
    pr.aktywnaOd = "2026-10-01"; pr.ostatnio = "2026-11-03"; pr.licznik = 2; pr.stanWzorca = "aktywny";
    pr.wiem = "2026-11-02"; pr.omowione = "2026-10-20";
    pam["L-01"] = pr;
    syg.stan["po_11"] = true;
    syg.zmienne["czas"] = "07:10";
    syg.odczyty["M-25"] = 55.0;
    syg.serie["pylek"] = {1.0, 2.0};
    wyk.insert("2026-10-01");
    k.dzis = "2026-11-04";
    k.trybBiezacy = reguly::Tryb::ASVAUTO;
    k.zestaw = &reguly::zestawWbudowany();
    k.poczatekLeczenia = "2026-01-01";
    podepnij();
  }
  void podepnij() {
    k.historia = &h;
    k.odpowiedzi = &odp;
    k.pamiec = &pam;
    k.wykluczoneNoce = &wyk;
    k.norma = &st.norma;
    k.stany = &st;
    k.wczesniejsze = &wcz;
  }
  uint64_t klucz() const { return reguly::kluczOceny(k, bezSygnalow ? nullptr : &syg, sp); }
};
struct MutacjaKlucza {
  const char* pole;
  bool wKluczu;  // false = pole pochodne (poza kluczem z założenia): klucz ma się NIE zmienić
  std::function<void(WejscieKlucza&)> zmien;
};
// wymaganie: T-WYM-99
static void testT_WYM_99_mutacjeKluczaOceny() {
  static Stany innyStan;
  static std::map<std::string, reguly::Wynik> inneWcz;
  static reguly::Zestaw innyZestaw;
  const std::vector<MutacjaKlucza> kontekst = {
      {"historia", true, [](WejscieKlucza& w) { w.h[1].ahi += 0.5f; }},
      {"dzis", true, [](WejscieKlucza& w) { w.k.dzis = "2026-11-05"; }},
      {"norma", false, [](WejscieKlucza& w) { w.k.norma = &innyStan.norma; }},
      {"stany", false, [](WejscieKlucza& w) { w.k.stany = &innyStan; }},
      {"trybBiezacy", true, [](WejscieKlucza& w) { w.k.trybBiezacy = reguly::Tryb::CPAP; }},
      {"odpowiedzi", true, [](WejscieKlucza& w) { w.k.odpowiedzi = nullptr; }},
      {"pamiec", true, [](WejscieKlucza& w) { w.k.pamiec = nullptr; }},
      {"zestaw", true, [](WejscieKlucza& w) { w.k.zestaw = &innyZestaw; }},
      {"wczesniejsze", false, [](WejscieKlucza& w) { w.k.wczesniejsze = &inneWcz; }},
      {"wykluczoneNoce", true, [](WejscieKlucza& w) { w.wyk.insert("2026-10-02"); }},
      {"poczatekLeczenia", true, [](WejscieKlucza& w) { w.k.poczatekLeczenia = "2026-01-02"; }},
  };
  const std::vector<MutacjaKlucza> sygnaly = {
      {"Sygnaly::stan", true, [](WejscieKlucza& w) { w.syg.stan["po_11"] = false; }},
      {"Sygnaly::zmienne", true, [](WejscieKlucza& w) { w.syg.zmienne["czas"] = "07:11"; }},
      {"Sygnaly::odczyty", true, [](WejscieKlucza& w) { w.syg.odczyty["M-25"] = std::nextafter(55.0, 100.0); }},
      {"Sygnaly::serie", true, [](WejscieKlucza& w) { w.syg.serie["pylek"][1] = 2.5; }},
  };
  const std::vector<MutacjaKlucza> odpowiedzi = {
      {"Odpowiedzi::sennosc", true, [](WejscieKlucza& w) { w.odp.sennosc["2026-11-01"] = false; }},
      {"Odpowiedzi::gorzej", true, [](WejscieKlucza& w) { w.odp.gorzej["2026-11-02"] = true; }},
      {"Odpowiedzi::przerwaPlanowana", true, [](WejscieKlucza& w) { w.odp.przerwaPlanowana.insert("2026-11-04"); }},
  };
  const std::vector<MutacjaKlucza> pamiec = {
      {"PamiecReguly::aktywnaOd", true, [](WejscieKlucza& w) { w.pam["L-01"].aktywnaOd = "2026-10-02"; }},
      {"PamiecReguly::ostatnio", true, [](WejscieKlucza& w) { w.pam["L-01"].ostatnio = "2026-11-04"; }},
      {"PamiecReguly::licznik", true, [](WejscieKlucza& w) { w.pam["L-01"].licznik++; }},
      {"PamiecReguly::stanWzorca", true, [](WejscieKlucza& w) { w.pam["L-01"].stanWzorca = "wyciszony"; }},
      {"PamiecReguly::wiem", true, [](WejscieKlucza& w) { w.pam["L-01"].wiem = "2026-11-03"; }},
      {"PamiecReguly::omowione", true, [](WejscieKlucza& w) { w.pam["L-01"].omowione = ""; }},
  };
  // listy kompletne: liczba wpisów = liczba pól struktury (nowe pole = błąd kompilacji niżej, po zmianie list)
  static_assert(liczba_pol::policz<reguly::Kontekst>() == 11, "Kontekst: nowe pole, dopisz mutację powyżej");
  static_assert(liczba_pol::policz<reguly::Sygnaly>() == 4, "Sygnaly: nowe pole, dopisz mutację powyżej");
  static_assert(liczba_pol::policz<reguly::Odpowiedzi>() == 3, "Odpowiedzi: nowe pole, dopisz mutację powyżej");
  static_assert(liczba_pol::policz<reguly::PamiecReguly>() == 6, "PamiecReguly: nowe pole, dopisz mutację powyżej");
  SPRAWDZ(kontekst.size() == liczba_pol::policz<reguly::Kontekst>() && sygnaly.size() == liczba_pol::policz<reguly::Sygnaly>() &&
          odpowiedzi.size() == liczba_pol::policz<reguly::Odpowiedzi>() &&
          pamiec.size() == liczba_pol::policz<reguly::PamiecReguly>());
  int n = 0;
  for (auto* lista : {&kontekst, &sygnaly, &odpowiedzi, &pamiec})
    for (auto& m : *lista) {
      WejscieKlucza a, b;
      m.zmien(b);
      bool rozne = a.klucz() != b.klucz();
      n++;
      if (rozne != m.wKluczu) {
        zle++;
        printf("BŁĄD: kluczOceny() %s zmiany pola %s\n", m.wKluczu ? "nie widzi" : "niepotrzebnie widzi", m.pole);
      } else {
        ok++;
      }
    }
  // sygnały nieobecne i `sp_model`
  WejscieKlucza a, b, c;
  b.bezSygnalow = true;
  c.sp = "2026-10-11";
  SPRAWDZ(a.klucz() != b.klucz());
  SPRAWDZ(a.klucz() != c.klucz());
  printf("  kluczOceny(): %d mutacji pól (w tym 3 pochodne poza kluczem), sygnały nieobecne i sp_model: zgodnie\n", n);
}

// Punkty faz obrazu pomiarowego (POMIAR_CZASU): haczyk dostaje nazwy faz w kolejności przebiegu; bez haczyka nic.
static std::vector<std::string> fazy;
static void zapiszFaze(const char* n) { fazy.push_back(n); }
// wymaganie: T-WYM-99
static void testT_WYM_99_punktyFaz() {
  szybkosc::haczykFazy = zapiszFaze;
  Panel p;
  przygotuj(p, "2026-11-04");
  wybudz(p, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK, BIT_PRAWY, naciski({BIT_PRAWY}));
  szybkosc::haczykFazy = nullptr;
  std::string w;
  for (auto& f : fazy) w += f + " ";
  printf("  fazy w wybudzeniu przyciskiem: %s\n", w.c_str());
  auto ile = [](const char* n) {
    int k = 0;
    for (auto& f : fazy) k += f == n;
    return k;
  };
  SPRAWDZ(!fazy.empty() && fazy.front() == "historia");
  SPRAWDZ(ile("przycisk") == 2 && ile("sprawy") >= 2);  // przycisk budzący i jeden w czuwaniu
  SPRAWDZ(ile("reguly") <= 2);
  size_t przed = fazy.size();
  Panel q;
  przygotuj(q, "2026-11-04");
  wybudz(q, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK, BIT_PRAWY, naciski({}));
  SPRAWDZ(fazy.size() == przed);
}
#endif

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  if (getenv("SZYB_CZAS")) {  // tylko pomiar na PC (informacyjnie): minimum z n przebiegów, także w budowie -DBAZA
    int n = atoi(getenv("SZYB_CZAS"));
    double m1 = 1e9, m2 = 1e9, m3 = 1e9;
    for (int i = 0; i < n; i++) {
      m1 = std::min(m1, scenariusz(1).liczniki[0].ms);
      m2 = std::min(m2, scenariusz(2).liczniki[0].ms);
      WynikScen w0 = scenariusz(0);
      m3 = std::min(m3, w0.liczniki[1].ms);
    }
    printf("CZAS kokpit 9 stron %.1f ms, kolejka 3 naciśnięć %.1f ms, przycisk po poranku %.1f ms\n", m1, m2, m3);
    return 0;
  }
#ifndef BAZA
  if (getenv("SZYB_PETLA")) {  // tylko profil na PC: wybudzenie przyciskiem z 9 stronami w pętli, bez testów
    int n = atoi(getenv("SZYB_PETLA"));
    for (int i = 0; i < n; i++) scenariusz(1);
    return 0;
  }
#endif
  testT_WYM_99_licznikiIRownowaznosc();
#ifndef BAZA
  testT_WYM_99_punktyFaz();
  testT_WYM_99_kolumnyNocy();
  testT_WYM_99_mutacjeKluczaOceny();
#endif
  printf("testy_szybciej_przycisk: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
