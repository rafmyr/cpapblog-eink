// SPDX-License-Identifier: GPL-3.0-or-later
// Pomiar baterii prototypu, bez zależności od Arduino. Narzędzie
// pomiarowe do oceny energii (spec 8.1, R-NF-01), nie funkcja dla użytkownika: wynik tylko na ekranie testu
// i w pliku /bateria.csv.
//
// Wiersz pliku na każde wybudzenie: `epoch;mV;wifi_ms;aktywny_ms;powod`
//   epoch       chwila wybudzenia (czas panelu), mV napięcie baterii (średnia kilku próbek przed włączeniem Wi-Fi),
//   wifi_ms     łączny czas z włączonym Wi-Fi w tym wybudzeniu,
//   aktywny_ms  czas od startu do zaśnięcia POPRZEDNIEGO wybudzenia (trzymany w RTC, -1 = nieznany, np. po starcie),
// powod start / przycisk / zegar:{powody} (od kroku 2 : powody z R-NF-02 z src/plan.h
//               nazwyPowodow(), np. zegar:okno_poranne+czujniki; „zegar:” bez nazwy = wybudzenie bez powodu).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct WpisBaterii {
  uint32_t ep = 0;
  int mV = 0;
  long wifiMs = -1, aktywnyMs = -1;
  std::string powod;
};
bool parsujBaterie(const std::string& linia, WpisBaterii& w);
std::string zapiszBaterie(const WpisBaterii& w);

const int BAT_DNI = 30;                  // plik przycinany do tylu dni (jak /humor.csv: 120 dni) [stała prototypu]
const int BAT_DNI_ZAPAS = 32;            // przycinanie dopiero, gdy najstarszy wpis jest starszy (bez przepisywania co dzień)
const int BAT_SKOK_LADOWANIA_MV = 60;    // wzrost netto względem minimum z BAT_OKNO_LADOWANIA wpisów = ładowanie [PRÓG]
const int BAT_OKNO_LADOWANIA = 12;       // ile poprzednich wpisów (ok. 6 h co 30 min) [stała prototypu]
const int BAT_PROG_ROZLADOWANIA_MV = 3500;  // szacunek dni do tego napięcia [PRÓG]
const uint32_t BAT_MIN_OKNO_S = 24 * 3600;  // trend dopiero z co najmniej 24 h danych bez ładowania
// Gdy najstarszy wpis jest starszy niż BAT_DNI_ZAPAS dni: zostają wpisy nie starsze niż BAT_DNI dni (bez zegara: bez
// zmian). true, gdy coś usunięto (wtedy plik do przepisania).
bool przytnijBaterie(std::vector<WpisBaterii>& w, uint32_t teraz);
// Czy plik wymaga przycięcia, po samym pierwszym (najstarszym) wpisie; zwykłe wybudzenie tylko dopisuje wiersz.
bool wymagaPrzyciecia(uint32_t najstarszy, uint32_t teraz);

struct TrendBaterii {
  bool jest = false;      // co najmniej 24 h od ostatniego ładowania
  double v = 0;           // ostatnie napięcie, V
  double vNaDobe = 0;     // nachylenie regresji liniowej, V na dobę (ujemne = spadek)
  double doby = 0;        // długość okna danych, doby
  double dniDoProgu = -1; // szacunek dni do BAT_PROG_ROZLADOWANIA_MV; -1 = brak spadku
  double godzinZbierania = 0;
  int n = 0;
};
// Regresja liniowa na napięciu wygładzonym medianą z 3 sąsiednich wpisów (pojedynczy odstający odczyt nie resetuje
// okna). Ładowanie = wzrost netto ≥ BAT_SKOK_LADOWANIA_MV względem minimum z BAT_OKNO_LADOWANIA poprzednich wpisów
// (także powolne); okno zaczyna się od pierwszego spadku po maksimum po ostatnim ładowaniu (bez fazy ładowania i stałego
// napięcia przy kablu). Bez wykrytego ładowania okno od pierwszego spadku albo, przy stałym napięciu, od początku.
TrendBaterii trendBaterii(std::vector<WpisBaterii> w);
// `Bateria 3,95 V · −0,04 V/dobę z 2,1 doby · ok. 25 dni` (1 dzień, 2 dni) albo `Bateria 3,95 V · zbieram dane (5 h)`;
// napięcie to bieżący odczyt `vTeraz` (NAN: ostatni wpis z pliku, bez wpisów `–`), trend z pliku.
std::string liniaBaterii(const TrendBaterii& t, double vTeraz);
// Średni czas Wi-Fi i aktywności z wpisów z ostatniej doby: `Wi-Fi śr. 8,2 s · aktywny 31 s`; "" bez danych.
std::string sredniaDoby(const std::vector<WpisBaterii>& w, uint32_t teraz);

// ---------- M-25: procent baterii ekranu (spec 4.3 M-25, E-05 w 6.4, R-KONF-41) ----------
// Napięcie bez obciążenia (przed ekranem i Wi-Fi, adapter średnią z 8 próbek) → procent z krzywej odcinkami ogniwa
// Li-Po 1S. Krzywa [PRÓG] do kalibracji na egzemplarzu (pomiar 10 ze spec technicznej 2.4): punkty z typowej
// krzywej spoczynkowej Li-Po 1S z tabel producentów ogniw i ładowarek modelarskich (4,20 V pełna, ok. 3,7 V przy
// 15–20%, kolano poniżej 3,65 V), przesunięte tak, że 4,15 V = 100% (ogniwo po ładowaniu i krótkim spoczynku nie
// trzyma 4,20 V) i 3,30 V = 0% (spec techniczna 3.1: ok. 3,3 V niska). Stała w kodzie, nie w progi.csv: to
// kalibracja pomiaru sprzętu (adapter T-KOMP-19), nie próg reguły z zestawu.
struct PunktKrzywej {
  int mV, proc;
};
extern const PunktKrzywej KRZYWA_BATERII[];  // malejąco po mV, od 100% do 0%
extern const int KRZYWA_BATERII_N;
// Poza [BAT_MV_MIN, BAT_MV_MAX] odczyt nie jest napięciem ogniwa: brak M-25. Dolna granica 3,0 V: ogniwo Li-Po 1S z zabezpieczeniem odcina się ok. 3,0 V, a panel z ESP32-S3 nie pracuje stabilnie poniżej,
// więc niższy odczyt to błąd pomiaru (dzielnik, ADC), nie bateria 0%; „nie oceniam”, nie S5.
const int BAT_MV_MIN = 3000, BAT_MV_MAX = 4500;
// Procent 0…100 z napięcia w mV (interpolacja liniowa między punktami, powyżej 4,15 V 100%, od 3,00 do 3,30 V 0%);
// -1 = brak odczytu (mV poza [BAT_MV_MIN, BAT_MV_MAX], np. NAN zamieniony na 0): E-05 „nie oceniam”, nigdy alarm.
int procentBaterii(int mV);

// Wygładzenie procentu między wybudzeniami (stan w RTC, StanRtc::bateriaProc i bateriaSurowy). Zmiana o więcej niż
// BAT_PROC_SZUM punktów w górę albo w dół wchodzi dopiero, gdy potwierdzi ją drugi kolejny pomiar (jeden odstający
// odczyt, np. tuż po paczce Wi-Fi poprzedniego wybudzenia albo przy szumie ADC, niczego nie zmienia); wtedy do
// wartości bliższej poprzedniej. Mały spadek (≤ BAT_PROC_SZUM) wchodzi od razu, mały wzrost nie: procent nie skacze
// w górę i w dół wokół progu E-05 przy każdym wybudzeniu, a błąd idzie w stronę ostrożną (niżej). Histerezę wejścia
// i wyjścia E-05 (≤ 15% wchodzi, znika > 20%; krytyczne ≤ 5%) robi reguła z zestawu (prog_biezacy_wyjscie).
// Zapadka w dół (mały spadek od razu, mały wzrost nigdy) i BAT_PROC_SZUM do kalibracji na egzemplarzu.
const int BAT_PROC_SZUM = 2;  // punkty procentowe [PRÓG] do kalibracji (pomiar 10)
const int8_t BAT_SUROWY_BRAK = -2;  // poprzedni pomiar w tym cyklu RTC był bez odczytu napięcia
struct StanProcentuBaterii {
  int8_t proc = -1;     // opublikowany procent (M-25); -1 = jeszcze nie było (start zasilania)
  int8_t surowy = -1;   // surowy procent poprzedniego pomiaru; -1 = brak, BAT_SUROWY_BRAK = poprzedni bez odczytu
};
// Przyjmuje stan i surowy procent tego wybudzenia (procentBaterii()); zwraca M-25. Brak odczytu teraz (surowy -1,
// R-KONF-41 „E-05 bez zmian”): w pierwszym takim wybudzeniu ostatni opublikowany procent z RTC (stan
// RTC ginie przy zaniku zasilania, więc to zawsze ten sam cykl RTC), a bez niego albo w drugim kolejnym wybudzeniu
// bez odczytu -1 (brak M-25: sygnał nie trafia do reguł, E-05 „nie oceniam”). Opublikowany procent się nie zmienia.
int wygladzProcent(StanProcentuBaterii& s, int surowy);
