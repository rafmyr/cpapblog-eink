// SPDX-License-Identifier: GPL-3.0-or-later
// Ekrany kontekstowe CO2 sypialni (spec 5.8.3, 5.8.4, 5.8.5): S-K-01 „Przewietrz sypialnię” (wieczór, W1), S-K-02
// „Smog za oknem” (wieczór, W1), S-K-13 „Zaduch w sypialni w nocy” (rano, R8); limity R-KON-04, R-KON-20, R-KON-29,
// tryb próbny R-KON-05. Bez Arduino.
//
// Źródło CO2 (M-23): lokalne źródło otoczenia (ADR-0019; w przykładowej instalacji Qingping przez serwer domowy) traktowane jak
// Qingping; przy obu źródłach pierwszeństwo ma Qingping (ADR-0019), tu jest jedno. Seria nocy: odczyty CO2 z chwilą
// (odczyt nie starszy niż 30 min z wybudzeń z Wi-Fi, R-DANE-33) w oknie 23:00–06:00; każdy odczyt liczy się do
// następnego, najwyżej CO2_PROBKA_MAX_S, więc rzadka seria (w nocy tylko 23:30 i od 05:00) nie daje „2 h ponad
// próg” z jednego odczytu.
// PM2.5 sypialni (S-K-01: „PM2.5 za oknem nie wyższe niż w sypialni”, S-K-02): kontrakt wersji 1 źródła go nie ma;
// do rozstrzygnięcia zamiast porównania z sypialnią panel bierze próg PM25_ZA_OKNEM_ZASTEPCZY (35 µg/m³,
// ten sam co czysta godzina S-K-33), oznaczony jako zastępczy [PRÓG] i wypisany w raporcie do decyzji.
//
// Przeniesienie warunków do katalogu reguł silnika (ADR-0023) to zadanie na później; tu własny kod z testami.
#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "ekrany.h"
#include "humor.h"
#include "sypialnia.h"

const int CO2_PROG = 1000;                 // S-K-01, S-K-13 [PRÓG]
const int SK01_MIN_PONAD = 120;            // S-K-01: ≥ 2 h [PRÓG] zeszłej nocy
const int SK13_MIN_PONAD = 240;            // S-K-13: ≥ 4 h [PRÓG]
const int SK13_MAKS = 2000;                // S-K-13: maksimum > 2000 ppm [PRÓG]
const float SK02_PM25_ZA_OKNEM = 15;       // S-K-02: PM2.5 za oknem > 15 µg/m³ (WHO, W)
const float PM25_ZA_OKNEM_ZASTEPCZY = 35;  // zastępstwo porównania z PM2.5 sypialni [PRÓG, do decyzji]
const uint32_t CO2_PROBKA_MAX_S = 30 * 60;  // odczyt reprezentuje najwyżej 30 min serii (stała prototypu)
// Tryb próbny (5.8.8, R-KON-05): spec: w egzemplarzu produkcyjnym domyślnie włączony (S-K-01, S-K-02, S-K-13 tylko
// w logu przez 2–3 miesiące). Prototyp nie miał trybu próbnego; tu jest wyłączony, żeby ekrany było widać od
// początku (do decyzji użytkownika).
const bool TRYB_PROBNY_KONTEKSTU = false;

struct ProbkaCo2 {
  uint32_t ts = 0;  // czas unix pomiaru
  int ppm = -1;
};
struct Co2Nocy {
  int odczytow = 0;
  int minutPonad = 0;  // minuty serii z CO2 > CO2_PROG
  int maks = -1;
  std::vector<int> wartosci;  // do wizualizacji (linia CO2 nocy)
};
// Odczyty w kolejności czasu (inne są sortowane); odczyt NAN/-1 pominięty.
Co2Nocy statystykaCo2(std::vector<ProbkaCo2> p);
// Seria nocy z rekordu sypialni (format `sypialnia` wersja 2, src/sypialnia.h; r08 bez osobnego pliku).
Co2Nocy co2ZNocy(const NocSypialni* n);

// Wpisy dziennika ekranów (/humor.csv, typ `SK01`, `SK02`, `SK13`; klucz = wartość wyzwalacza, ppm albo minuty).
struct WejscieWieczoru {
  std::string dzis;          // data wieczoru (jutro rano będzie data poranka dzis+1)
  const Co2Nocy* noc = nullptr;  // zeszła noc (data poranka dzis)
  int co2Teraz = -1;         // odczyt nie starszy niż 30 min, -1 = brak
  float pm25ZaOknem = NAN;   // M-27 teraz (źródło lokalne albo prognoza), NAN = brak
  float pm25Sypialni = NAN;  // brak w kontrakcie wersji 1
  float tempSypialni = NAN;  // do tabeli konfliktów 5.8.5 (≤ 16 °C: krok z otuliną)
  bool pylekWysoki = false;  // 5.8.5: krok `Wietrz krótko, dużo pyłku.`
  bool trybProbny = TRYB_PROBNY_KONTEKSTU;
  const std::vector<WpisHumoru>* dziennik = nullptr;
  // wartość wyzwalacza przy ostatnim pokazaniu i jego rodzaj (minuty ponad próg nocy albo ppm odczytu), do wyjątku
  // „≥ 25% gorsza” (7.4): porównanie tylko wyzwalaczy tego samego rodzaju; -1 = nieznana (bez wyjątku)
  int poprzedniaWartosc = -1;
  bool poprzedniZOdczytu = false;
  // suma próśb (7.4, R-PIE-10): dziś była już prośba (ekran dnia z krokiem rano, linia zaproszenia na ekranie)
  bool prosbaDzis = false;
  // wieczory z krokiem (W1–W3) w 7 dniach przed dziś (R-PIE-12): od 4 [PRÓG] wieczór zostaje przy ekranie z rana
  int wieczorowZKrokiem7 = 0;
};
enum class EkranWieczoru : uint8_t { BRAK, SK01, SK02 };
struct WynikWieczoru {
  EkranWieczoru ekran = EkranWieczoru::BRAK;
  bool doLogu = false;       // tryb próbny: tylko wpis w logu (R-KON-05)
  bool zOdczytu = false;     // S-K-01 wyzwolony odczytem bieżącym (powód `Teraz CO2 {x} ppm.`)
  int wartosc = 0;           // klucz dziennika: minuty ponad próg (noc) albo ppm (teraz)
  const char* dlaczego = "";
};
bool sezonGrzewczy(const std::string& data);  // 1.10–30.04 [PRÓG]
WynikWieczoru ocenWieczor(const WejscieWieczoru& w);

struct WejscieRanka {
  std::string dzis;              // data poranka
  const Co2Nocy* noc = nullptr;
  bool poprzedniWieczorSK01 = false;  // R-KON-20
  bool pozaDomem = false;        // R-CZY-12
  bool trybProbny = TRYB_PROBNY_KONTEKSTU;
  const std::vector<WpisHumoru>* dziennik = nullptr;
};
struct WynikRanka {
  bool sk13 = false;
  bool doLogu = false;
  const char* dlaczego = "";
};
WynikRanka ocenRano(const WejscieRanka& w);

// Treść ekranu (5.8.4) dla rysujEkranKontekstu() i karty (K05 dla S-K-01, scena W01); `wieczor` z tabelą konfliktów.
struct TrescKontekstu {
  TrescEkranu t;              // etykieta, tytuł, powód, krok; fakty karty
  std::vector<int> linia;     // CO2 nocy (wizualizacja), pusta = bez linii
  std::string podpisLewo, podpisPrawo;  // pod linią
  std::vector<Fakt> pary;     // S-K-02: pary liczb (PM2.5 za oknem, PM2.5 w sypialni, CO2 w sypialni)
};
TrescKontekstu trescSK01(const WejscieWieczoru& w, const WynikWieczoru& r);
TrescKontekstu trescSK02(const WejscieWieczoru& w);
TrescKontekstu trescSK13(const Co2Nocy& n);
// Karta albo ekran danych (5.7.3, R-HUM-07 „1 na 2”): src/humor.h ekranKarty() z typem `SK01`/`SK02` i sprawą `co2`;
// wpis dziennika ma wtedy klucz `co2#{n}`.
