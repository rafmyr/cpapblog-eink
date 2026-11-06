// SPDX-License-Identifier: GPL-3.0-or-later
// Sypialnia z czujnika w obudowie panelu (M-22: temperatura i wilgotność, mediana nocy), bez zależności od Arduino.
// Używają jej: prawa część stopki ekranu dnia (R-EKR-10) i ekran S2c „Dowód spokoju: sypialnia” (5.4.2).
//
// Zapis (R-DANE-09 w uproszczeniu): od kroku 2 panel budzi się w oknie nocy co 15 min (wybudzenie
// pomiarowe bez Wi-Fi, T-WYM-31, src/plan.h), a odczyt dochodzi z każdego wybudzenia w oknie. Odczyty
// z okna nocy 23:00–06:00 (podpis artboardu S2-sypialnia `mediana nocy 23:00–06:00`) trafiają do bufora w pamięci RTC;
// pierwsze wybudzenie od 06:00 liczy medianę nocy i dopisuje wiersz do /sypialnia.csv (`data;temp;wilg`, data poranka).
// Brak odczytu = brak pomiaru, nie zero (R-AW-13); noc bez żadnego odczytu nie ma wiersza.
// CO2 sypialni (M-23) z lokalnego źródła otoczenia (ADR-0019, src/otoczenie.h): odczyt z wybudzenia z udaną odpowiedzią
// źródła trafia do tego samego bufora nocy (osobny licznik), a mediana CO2 nocy do osobnego pliku /sypialnia-co2.csv
// (`data;co2`, data poranka nocy); /sypialnia.csv zostaje w układzie `data;temp;wilg`, który czyta też starszy firmware.
// Wiersze 4-polowe z wczesnej wersji formatu są czytane i przenoszone (migracja w
// src/dyrygent.cpp). Qingping nie jest podłączony; PM2.5 sypialni nie ma w kontrakcie wersji 1, więc S2c pokazuje
// najwyżej 3 kolumny (R-EKR-17).
#pragma once
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "noc.h"

struct NocSypialni {
  // konstruktor zamiast agregatu (pola wersji 2 bez zmiany miejsc `NocSypialni{data, t, w}` w kodzie i testach)
  NocSypialni(std::string d = "", float t_ = NAN, float w_ = NAN, float c = NAN) : data(std::move(d)), t(t_), w(w_), co2(c) {}
  std::string data;  // data poranka
  float t = NAN, w = NAN;
  float co2 = NAN;  // mediana nocy CO2 (ppm) z lokalnego źródła otoczenia; NAN = brak
  // (format `sypialnia` wersja 2, S-K-01, S-K-13): minuty serii nocy z CO2 > 1000 ppm (każdy odczyt liczy się do
  // następnego, najwyżej 30 min), maksimum nocy i seria odczytów do wizualizacji (`620,700,…`); -1 / "" = brak
  int co2Ponad = -1, co2Maks = -1;
  std::string co2Seria;
};
// `data;temp;wilg` (także `data;temp;wilg;co2` i wersja 2 `data;temp;wilg;co2;co2_ponad;co2_maks;co2_seria`)
bool parsujSypialnie(const std::string& linia, NocSypialni& n);
// Pola wersji 2 po `co2`: `;{co2_ponad};{co2_maks};{co2_seria}`; puste przy braku.
std::string polaCo2V2(const NocSypialni& n);
const int CO2_PROBKA_MAX_MIN = 30;  // odczyt reprezentuje najwyżej 30 min serii (stała prototypu)
// Statystyka CO2 bufora nocy (minuty ponad CO2_PROG_NOCY, maksimum, seria) bez zamykania bufora (rano przed 06:00).
void co2ZBufora(const struct BuforSypialni& b, NocSypialni& n);
std::string zapiszSypialnie(const NocSypialni& n);  // zawsze 3 pola
// /sypialnia-co2.csv: `data;co2` (ppm, całkowita); zapis "" bez CO2
bool parsujCo2Nocy(const std::string& linia, std::string& data, float& co2);
std::string zapiszCo2Nocy(const NocSypialni& n);
// Próg CO2 nocy do zdania i oznaczenia S2c: ten sam co S-K-01 (5.8.3, 1000 ppm [PRÓG]). Mediana nocy na progu albo
// wyżej nigdy nie daje `Dobre warunki do snu.`, a kolumna CO2 dostaje czarne pole „ponad próg” (jedna legenda panelu,
// także bez normy).
const int CO2_PROG_NOCY = 1000;

// Bufor odczytów jednej nocy; zwykła struktura bez wskaźników, żeby mogła leżeć w pamięci RTC (tylko RTC, bez kopii
// w NVS, więc rozmiar wolno zmieniać). Odczyt czujnika panelu co 15 min w oknie
// 23:00–06:00 (T-WYM-31) daje 28 odczytów na noc; pojemność 32 zostawia zapas na wybudzenia przyciskiem.
// CO2 sypialni ma odczyty tylko z wybudzeń z Wi-Fi (23:30, od 05:00), więc zostaje 24.
const int BUFOR_SYPIALNI_ODCZYTOW = 32;
const int BUFOR_SYPIALNI_CO2 = 24;
struct BuforSypialni {
  char data[11] = {0};  // data poranka nocy, której odczyty są w buforze
  uint8_t n = 0;
  int16_t t10[BUFOR_SYPIALNI_ODCZYTOW];  // temperatura × 10
  int16_t w10[BUFOR_SYPIALNI_ODCZYTOW];  // wilgotność × 10
  uint8_t nCo2 = 0;
  int16_t co2[BUFOR_SYPIALNI_CO2];       // CO2 sypialni, ppm (lokalne źródło otoczenia)
  int16_t co2Min[BUFOR_SYPIALNI_CO2];    // minuta od 12:00 wieczoru odczytu CO2 (seria nocy S-K-01, S-K-13)
};
// Okno nocy: od 23:00 do 06:00 (minuty doby ≥ 1380 albo < 360).
bool oknoSypialni(int minuta);
// Odczyt w oknie nocy: dopisuje do bufora nocy z datą poranka `dzis` (przed północą: jutro). Odczyt NAN pomija.
void dodajOdczyt(BuforSypialni& b, const std::string& dzis, int minuta, float t, float w);
// To samo dla CO2 sypialni (ppm); co2 < 0 = brak odczytu (pomijany, nie zero).
void dodajCo2(BuforSypialni& b, const std::string& dzis, int minuta, int co2);
// Od 06:00: gdy bufor ma noc z datą poranka `dzis` (albo starszą), zwraca jej medianę i czyści bufor.
bool zamknijNoc(BuforSypialni& b, const std::string& dzis, int minuta, NocSypialni& wynik);

// Norma sypialni (4.4, wiersz „sypialnia”): mediana, P10 i P90 z 30 ostatnich nocy z odczytem w 90 dniach przed `dzis`,
// bez nocy z czynnikiem „spanie poza domem” (5.11.5); minimum 14.
struct NormaSypialni {
  NormaMetryki t, w, co2;
};
NormaSypialni liczNormeSypialni(const std::vector<NocSypialni>& s, const std::string& dzis,
                                const std::set<std::string>& wykluczone);
const NocSypialni* sypialniaZDnia(const std::vector<NocSypialni>& s, const std::string& data);

// Prawa część stopki (R-EKR-10): `przy łóżku 21° · 46%` z odczytu czujnika panelu w tym
// wybudzeniu (czujnik stoi przy łóżku, nie jest czujnikiem sypialni); bez odczytu "" (R-AW-13). Podpis `sypialnia`
// zostaje dla danych z czujnika sypialni (Qingping, M-23), jeszcze niepodłączonego.
// Z CO2 sypialni (lokalne źródło otoczenia, świeże, R-EKR-10): `przy łóżku 21° · 46% · CO2 770`; bez odczytu czujnika
// panelu "" także przy CO2 (R-AW-13).
std::string stopkaCzujnika(float t, float w, int co2 = -1);
// Słowo w wierszu S2c (artboard S2-sypialnia): w paśmie P10–P90 `↔ jak zwykle`, poza nim `↑ zwykle {mediana}` albo
// `↓ zwykle {mediana}`; bez normy `uczę się`. spokojne = `jak zwykle`. Jednostka "" (CO2): mediana jako liczba całkowita.
std::string slowoSypialni(double v, const NormaMetryki& nm, bool& spokojne, const char* jednostka);
