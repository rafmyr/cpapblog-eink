// SPDX-License-Identifier: GPL-3.0-or-later
// Kokpit K1 i K2 (moduł KOKPIT 5.5, R-KOK-*): agregacje z historii nocy, bez zależności od Arduino.
// Wynik to gotowe teksty i liczby do rysowania (ekrany.cpp), żeby dało się je sprawdzić testami.
//
// Wiersze według trybu bieżącego (4.11, ADR-0030, R-KOK-15): tryb bieżący = tryb (tag `MOP`, pierwsze pole
// Noc::ustawienia) ostatniej nocy w trybie obsługiwanym (CPAP, AutoSet, ASV, ASVAuto) do ostatniej zapisanej nocy;
// tryb nocy z trybNocy() (src/noc.h); noc z karty bez odczytu trybu ma tryb ostatniej znanej wcześniej nocy (jak
// R-NOR-07), noc z AirBridge bez odczytu trybu to tryb nieznany, poza oknami (R-DANE-29).
// - CPAP, AutoSet: K1 liczba i wiersz `ciśnienie` / `Ciśnienie` (M-05 mediana), 4 wiersze; K2 wiersz `Ciśnienie P95`
//   (M-05p), 9 wierszy;
// - ASV, ASVAuto: K1 `EPAP` (M-06 mediana) i dodatkowy wiersz `Wsparcie` (M-28 mediana), 5 wierszy; K2 `EPAP P95`
//   (M-06p) i `Wsparcie PS` (M-28 mediana), 10 wierszy; M-05 i M-05p bez wiersza;
// - brak nocy ze znanym trybem (np. same noce ze starego importu bez ustawień): układ ASV bez filtra trybu, jak
//   przed ADR-0030 (tryb egzemplarza referencyjnego).
// Wiersze zależne od trybu (ciśnienie, EPAP, wsparcie) biorą tylko noce w trybie bieżącym: w K1 linia 30 nocy,
// mediana i słowo oceny, w K2 komórka nocy w innym trybie to `–` (fail-path R-KOK-15).
//
// Słowa oceny K1 (5.5.2, R-KOK-16) i oznaczenia K2 (5.5.3) ze stanów odchylenia (src/stany.h, 4.6), kolejność:
// 1. metryka bez normy: `uczę się`, bez ramek (R-KOK-05);
// 2. metryka w stanie odchylenia: wiersz K1 w ramce ze słowem `wyżej niż zwykle` / `od {n} nocy` albo
//    `niżej niż zwykle` / `od {n} nocy` w 2 liniach (R-KOK-03, T3), etykieta wiersza K2 w ramce (R-KOK-09);
// 3. trend (R-KOK-16, wariant A): mediana ostatnich 7 nocy okna 30 nocy wobec mediany
//    23 wcześniejszych; różnica ≥ 0,5 × Δ metryki (4.5; ciśnienie: Δ M-05p, EPAP: Δ M-06p) w górę `rośnie`, w dół
//    `spada`, mniejsza `stabilnie`; mniej niż 4 noce w oknie 7 albo mniej niż 14 w oknie 23: `uczę się`. Noce jak
//    w linii 30 nocy, bez nocy w trybie spoza wydania (R-DANE-29), dla AHI bez nocy niepewnych (R-NOR-19);
// - `Wsparcie` (M-28) nie ma Δ w 4.5; do słowa trendu Δ 2 cmH2O jak EPAP (TREND_DELTA_PS);
// - jedna legenda panelu: ramka = poza twoją normą, czarne pole = ponad próg (tylko K2).
// Wiersz `Wyciek` bez stanu odchylenia z medianą 30 nocy (w dokładności ekranu) powyżej 10 L/min [PRÓG]: w drugiej
// linii oceny `wyciek widoczny`, bez ramki (R-EKR-59).
// Stan odchylenia mają w danych panelu: AHI (M-02), czas z maską (M-01), wyciek P95 (M-07), EPAP P95 (M-06p, wiersz K2).
// Wiersze K1 ciśnienia (M-05 mediana), `EPAP` (M-06 mediana) i `Wsparcie` (M-28) nie mają stanu (tabela 4.5).
//
// Zostaje z wcześniejszych pakietów (do decyzji):
// - brak znaczników wzorców L i „P95 przy maksimum” (R-KOK-12, R-KOK-13): wygląd znaczników;
// - zdanie podsumowania K2 bez oceny tygodnia (`{n} z 7 nocy z maską ponad 4 h.`): reguła zdania czeka na decyzję;
// - wiersze SpO2 i puls (R-KOK-11): rekord nocy nie ma oksymetru, układ 11 wierszy;
// - `Zdjęcia maski` (M-17) i `Oddechy /min` (M-13) z rekordu nocy od wersji beta (tylko noce z AirBridge); M-17 ma normę
//   i stan odchylenia, M-13 nie ma normy w panelu (src/noc.h), więc kolumna `norma` `–` i bez ramek;
// - OA, CA, H (M-03) z raportu AirBridge; noce z karty mają tam `–` (R-DANE-31, 4.11); wiersz bez żadnej
//   wartości w 30 nocach jak wiersz bez danych (`–` także w kolumnie `norma`); CA/h ze stanem odchylenia od wersji beta;
// - okno 7 i 30 nocy kończy się na ostatniej zapisanej nocy (tej z nagłówka K1), nie na dzisiejszej dacie;
// - odkrycie O-12 pod nagłówkiem K1 (5.5.2): w tej wersji panel odkryć tam nie pokazuje.
#pragma once
#include <string>
#include <vector>
#include "czynniki.h"
#include "noc.h"
#include "reguly.h"
#include "stany.h"

struct Linia30 {  // linia 30 nocy w wierszu K1 (K2 w opcji A jej nie ma)
  float v[30];
  bool ma[30];
  double medNormy = NAN;  // przerywana mediana normy
};

struct WierszK1 {
  const char* nazwa = "";
  Linia30 linia;
  std::string mediana;  // mediana 30 nocy (z nocy z wartością w oknie)
  std::string slowo;    // `stabilnie`, `rośnie`, `spada`, `uczę się`, `wyżej niż zwykle` / `niżej niż zwykle`
  std::string slowo2;   // druga linia oceny (T3): `od {n} nocy` albo `wyciek widoczny`; "" = ocena w jednej linii
  bool stan = false;    // R-KOK-03: metryka w stanie odchylenia, wiersz w ramce
};

struct DaneK1 {
  std::string naglowek;  // `Ostatnia noc · czw 12.11` albo `Noc 18.10` (R-KOK-04)
  bool romb = false;     // R-KOK-14
  bool jestNoc = false;  // jest noc, o którą można zapytać (↻ 2 s, R-CZY-08)
  // (5.5.2 „Odkrycie o czynniku”, R-CZY-20): K1 otwarty z ekranu dnia O-12 z ukrytym czynnikiem: pod nagłówkiem
  // pełny tytuł i powód O-12 zamiast 4 liczb (brak kanwy tego wariantu: do decyzji); "" = zwykły K1
  std::string odkrycieTytul, odkryciePowod;
  std::string liczby[4], opisy[4];
  static const int WIERSZE = 5;  // najwięcej (ASV, ASVAuto); CPAP i AutoSet: 4
  int wierszy = WIERSZE;          // wiersze do narysowania: w[0] … w[wierszy − 1]
  WierszK1 w[WIERSZE];
};

struct KomorkaK2 {
  std::string tekst = "–";
  bool ponadProg = false;  // negatyw (R-KOK-07)
  bool poza = false;       // ramka (R-KOK-08)
};

// K2 w opcji A (SPEC-KOKPIT 4): jedna strona, 10 wierszy, bez kolumny `30 nocy`
// (trend 30 nocy jest w K1); jednostka w etykiecie (`Wyciek l/min`, T5).
struct WierszK2 {
  const char* etykieta = "";
  bool grupa = false;     // linia 1 px nad wierszem (początek grupy: `Czas z maską`, `Wyciek l/min`)
  bool wciecie = false;   // składowe AHI
  KomorkaK2 k[7];
  std::string norma = "–";
  bool stan = false;  // R-KOK-09: metryka w stanie odchylenia, etykieta wiersza w ramce
};

struct DaneK2 {
  std::string zakres;  // `3–9 listopada` albo `28.10–3.11` (T4: małymi literami); nagłówek `Ostatnie 7 nocy · {zakres}`
  std::string zdanie;
  std::string kol[7];
  bool romb[7] = {};
  static const int WIERSZE = 10;  // najwięcej (ASV, ASVAuto); CPAP i AutoSet: 9
  int wierszy = WIERSZE;           // wiersze do narysowania: w[0] … w[wierszy − 1]
  WierszK2 w[WIERSZE];
  bool sprawy = true;  // stopka `▶ sprawy` (strona 3) albo, bez wyników silnika, `▶ norma`
};

// Tryb bieżący (4.11, ADR-0030, R-KOK-15): tryb ostatniej nocy w trybie obsługiwanym do `koniec` włącznie (reguła
// trybu nocy wyżej); NIEZNANY = żadnej takiej nocy (układ ASV jak przed ADR-0030). Ta sama funkcja dla K1, K2 i S2a.
reguly::Tryb trybBiezacy(const std::vector<Noc>& hist, const std::string& koniec);

// st: stany odchylenia dla `dzis` (liczStany); nullptr = bez stanów (same `stabilnie` i `uczę się`).
// trybStatusu: `mode` z `/api/status`; tylko gdy historia nie ma żadnej nocy, wtedy układ wierszy według niego;
// "" albo tryb spoza wydania = układ ASV jak dotąd.
DaneK1 budujK1(const std::vector<Noc>& hist, const std::string& dzis, const Norma& nm,
               const std::vector<WpisCzynnikow>& cz, const Stany* st = nullptr, const std::string& trybStatusu = "");
DaneK2 budujK2(const std::vector<Noc>& hist, const std::string& dzis, const Norma& nm,
               const std::vector<WpisCzynnikow>& cz, const Stany* st = nullptr, const std::string& trybStatusu = "");

// Progi „ponad próg” w K2 (R-KOK-07): AHI ≥ 5 / h (E-30); wyciek P95 > 24 L/min (S-K-10; bez tagu MSK
// próg maski nosowej, R-KON-12). Typu maski prototyp nie czyta.
const double PROG_AHI = 5.0, PROG_WYCIEK = 24.0;

// R-KOK-16 [PRÓG] (docs/spec/progi.csv): próg różnicy median jako część Δ metryki (
// wariant A) i najmniejsza liczba nocy z wartością w oknie 7 i w oknie 23 nocy (propozycja, do kalibracji).
const double TREND_CZESC_DELTY = 0.5;
const int TREND_NOCY_7 = 4, TREND_NOCY_23 = 14;
// R-KOK-16.delta_ps [PRÓG]: Δ wiersza `Wsparcie` (M-28) do słowa trendu, ta sama co EPAP.
const double TREND_DELTA_PS = 2.0;
// R-EKR-59 [PRÓG]: mediana wycieku P95 z 30 nocy powyżej tylu L/min daje `wyciek widoczny` w K1.
const double WYCIEK_WIDOCZNY = 10.0;
