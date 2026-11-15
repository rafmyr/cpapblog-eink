// SPDX-License-Identifier: GPL-3.0-or-later
// Strona kokpitu „Sypialnia · 30 nocy” (spec 5.5.8, R-KOK-29…R-KOK-33, ADR-0044; decyzja właściciela projektu
// 14.11.2026 „zgoda na 4 ekran”), bez zależności od Arduino. Trzy wykresy liniowe jeden pod drugim (temperatura,
// wilgotność, CO2 z ostatnich 30 nocy, `/dane/sypialnia/{RRRR}.csv`, src/sypialnia.h), przy każdym duża wartość
// ostatniej nocy z jednostką, kropka na ostatniej nocy (jej prawdziwa wartość), linia = średnia krocząca z 7 nocy
// (decyzja 9 z 14.11.2026), dwie przerywane linie zwykłego pasma (P10–P90 normy sypialni, ta sama norma co słowo
// `jak zwykle` w S2c, src/sypialnia.h liczNormeSypialni()) w środkowej trzeciej części pola i słowo z prawej. Strona opisuje
// dane: bez progów zdrowotnych, bez ocen „dobrze/źle” i bez zaleceń (ADR-0002); brak odczytu nigdy nie wygląda jak
// „jak zwykle” (ADR-0029 pkt 7).
#pragma once
#include <set>
#include <string>
#include <vector>
#include "ekrany.h"
#include "sypialnia.h"

const int SYP30_NOCY = 30;  // okno wykresu: noce z datą poranka od dzis − 29 do dzis

// Słowo z prawej strony wykresu (R-KOK-31, R-KOK-32). Kolejność: brak jakiegokolwiek odczytu w 30 nocach (napis
// `brak odczytu` w polu wykresu, bez słowa), brak odczytu ostatniej nocy (`brak odczytu`), pasmo bez minimum nocy
// (`uczę się`), wartość ostatniej nocy wobec pasma (P10 ≤ v ≤ P90 `jak zwykle`, ponad P90 `wyżej niż zwykle`, poniżej
// P10 `niżej niż zwykle`). Przed zamknięciem nocy (przed 06:00, noc z datą poranka `dzis` jeszcze bez wiersza,
// R-KOK-31): NOC_TRWA, napis `noc trwa` w miejscu wartości, bez słowa i bez kropki (wartość poprzedniej nocy
// nie udaje dzisiejszej).
enum class SlowoSyp30 : unsigned char { BEZ_DANYCH, BRAK_OSTATNIEJ, UCZE_SIE, JAK_ZWYKLE, WYZEJ, NIZEJ, NOC_TRWA };
const char* tekstSlowaSyp30(SlowoSyp30 s);  // "" przy BEZ_DANYCH (napis stoi w polu wykresu); `noc trwa` przy NOC_TRWA

struct WykresSyp30 {
  std::string nazwa;      // `TEMPERATURA`, `WILGOTNOŚĆ`, `CO2` (wersalikami, P22B z odstępem liter)
  std::string jednostka;  // `°C`, `%`, `ppm`
  std::string wartosc;    // wartość ostatniej nocy (`19,5`, `38`, `1240`); `–` bez odczytu ostatniej nocy
  float v[SYP30_NOCY];    // od najstarszej nocy okna (indeks 0) do ostatniej (29); NAN = noc bez odczytu (luka)
  float sr[SYP30_NOCY];   // średnia krocząca z 7 nocy (noc i 6 poprzednich, także sprzed okna; tylko noce z odczytem);
                          // liczona tylko w nocach z odczytem, NAN gdzie indziej (R-KOK-33, decyzja 9)
  bool pasmo = false;     // zwykłe pasmo znane (norma sypialni z co najmniej 14 nocami [PRÓG], 4.4)
  double p10 = 0, p90 = 0;
  SlowoSyp30 slowo = SlowoSyp30::BEZ_DANYCH;
  WykresSyp30();
  bool jestNoc(int i) const;  // noc i ma odczyt
  bool jestOstatnia() const { return jestNoc(SYP30_NOCY - 1); }
  bool cokolwiek() const;     // co najmniej jedna noc z odczytem w oknie
};

struct DaneSyp30 {
  std::string zakres;          // środek belki: `16.10–14.11` (daty poranka pierwszej i ostatniej nocy okna)
  WykresSyp30 w[3];            // temperatura, wilgotność, CO2
  bool sprawy = false;         // stopka: przed stroną „Otwarte sprawy” (`◀ sprawy`)
  bool liczby = false;         // stopka: przed stroną „Co znaczą te liczby” (`◀ liczby`) bez spraw
  std::string stopkaWstecz;    // pierwsza strona ciągu otwarta z Portretu: `◀ portret nocy` (R-POR-24); "" = z ciągu
};

// R-KOK-29: strona wchodzi do ciągu kokpitu, gdy w oknie 30 nocy (daty poranka dzis − 29 … dzis) jest co najmniej
// jedna noc z co najmniej jednym odczytem (temperatura, wilgotność albo CO2). Bez daty (`dzis` niepoprawne) false.
bool stronaSypialniJest(const std::vector<NocSypialni>& s, const std::string& dzis);
// Dane strony (R-KOK-31, R-KOK-32): okno 30 nocy, wartości ostatniej nocy (noc z datą poranka `dzis`), pasmo P10–P90
// z liczNormeSypialni(s, dzis, wykluczone) (30 ostatnich nocy z odczytem przed `dzis` w 90 dniach, bez nocy poza
// domem, minimum 14 [PRÓG]) i słowo. Noc spoza okna, z datą w przyszłości albo zła data: pomijana.
DaneSyp30 budujSyp30(const std::vector<NocSypialni>& s, const std::string& dzis,
                     const std::set<std::string>& wykluczone, bool nocTrwa = false);
// Jeden wiersz na datę: przy powtórzonej dacie liczy się PIERWSZY wiersz pliku, jak w S2c (sypialniaZDnia()).
// nocTrwa (R-KOK-31): dyrygent przekazuje true przed 06:00 (noc z datą poranka `dzis` jeszcze się nie zamknęła);
// metryka bez odczytu ostatniej nocy dostaje wtedy NOC_TRWA zamiast BRAK_OSTATNIEJ.
// Stopka (5.6.1 pkt 6, kolejność ◀ ▶ ↻): `◀ sprawy` / `◀ liczby` / `◀ 7 nocy`, `▶ ustawienia`, `↻ ekran dnia`.
// `wstecz` "" = podpis strony przed sypialnią w ciągu; inaczej podpis ◀ (pierwsza strona z Portretu).
std::vector<std::string> stopkaSyp30(bool sprawy, bool liczby, const std::string& wstecz = "");
// Belka (5.4.17): nazwa `Sypialnia 3 / 4` (numer z nawigacji), środek `30 nocy · 16.10–14.11`, przy braku miejsca
// `30 nocy`.
Belka belkaSyp30(const DaneSyp30& d);

// Geometria (piksele, ekran 800 × 480). Trzy rzędy pod belką, legenda nad stopką (kreska stopki na 434).
const int SYP30_RZAD_Y0 = 48, SYP30_RZAD_H = 116;    // rzędy 48…163, 164…279, 280…395
const int SYP30_X_WYKRESU = 228, SYP30_W_WYKRESU = 352;  // pole wykresu x 228…579
const int SYP30_X_SLOWA_MIN = 596;                        // słowo do prawej na 772, nie wcześniej niż od 596
const int SYP30_WYKRES_GORA = 14, SYP30_WYKRES_H = 88;    // pole wykresu w rzędzie: y + 14 … y + 101
const int SYP30_MARGINES = 7;                             // od brzegu pola do skrajnej wartości (kropka r = 6)
const int SYP30_LINIA_GRUBOSC = 3;                        // linia nocy (co najmniej 2 px)
const int SYP30_PASMO_GRUBOSC = 2;                        // przerywane linie pasma: 2 px, kreska 6, przerwa 6
const int SYP30_PASMO_KRESKA = 6;
const int SYP30_KROPKA_R = 6;                             // kropka ostatniej nocy
const int SYP30_LEGENDA_BAZA = 424;                       // linia bazowa legendy (P22B), kreska stopki na 434
// Najmniejszy zakres osi Y wykresu (wartość stała i pasmo o zerowej szerokości nie dzielą przez zero; mała zmiana nie
// wygląda jak skok przez całą wysokość): parametry rysunku, nie progi oceny.
const double SYP30_ZAKRES_MIN[3] = {2.0, 6.0, 200.0};     // °C, %, ppm  [PRÓG]
// Średnia krocząca linii (decyzja 9): noc i 6 poprzednich; luka dłuższa niż SYP30_LUKA_MAX nocy bez odczytu przerywa
// linię (krótsza jest mostkowana), żeby linia nie udawała ciągłości. Parametry rysunku, nie progi oceny.
const int SYP30_SREDNIA_NOCY = 7;
const int SYP30_LUKA_MAX = 2;                             // [PRÓG]
const int SYP30_STRZALKA_W = 9, SYP30_STRZALKA_H = 5;     // znak „poza skalą” przy krawędzi pola

// Oś Y (decyzja 9a): przy znanym paśmie oś = pasmo rozszerzone o swoją szerokość w górę i w dół (pasmo w środkowej
// trzeciej części pola), zakres co najmniej SYP30_ZAKRES_MIN (wtedy rozszerzony symetrycznie, pasmo nadal w środku);
// bez pasma („uczę się”) od najmniejszej do największej wartości okna, co najmniej SYP30_ZAKRES_MIN.
void zakresOsiSyp30(const WykresSyp30& w, int r, double& mn, double& mx);
// Pozycja nocy i wartości w polu wykresu rzędu `r` (0…2); do testów i rysunku. Wartość poza osią: przycięta do
// krawędzi pola (najwyższa / najniższa pozycja), pozaOsiaSyp30() mówi, w którą stronę (+1 ponad, −1 poniżej, 0 w osi).
int xNocySyp30(int i);
int yWartosciSyp30(const WykresSyp30& w, int r, double v);
int pozaOsiaSyp30(const WykresSyp30& w, int r, double v);
void rysujSyp30(Plotno& p, const DaneSyp30& d);
