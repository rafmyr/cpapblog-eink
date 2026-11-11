// SPDX-License-Identifier: GPL-3.0-or-later
// Stany odchylenia (spec 4.6), noc niepewna (4.9), warunek uspokojenia (5.4.13) i porównanie tygodnia z normą
// (S2a, 5.4.2), bez zależności od Arduino. Wynik idzie na ekran dnia (dzien.cpp), do K1 i K2 (kokpit.cpp).
//
// Metryki ze stanem odchylenia, które są w danych panelu (tabela 4.5):
//   M-01 czas z maską (w dół, Δ −1,5 h, techniczna), M-02 AHI (w górę, Δ +3 / h, zdrowotna),
//   M-07 wyciek P95 (w górę, Δ +10 L/min, techniczna), M-06p EPAP P95 (tryb ASV, w górę, Δ +2 cmH2O, zdrowotna).
// Od także M-03 CA/h (w górę, Δ +1 / h, zdrowotna, bez nocy niepewnych) i M-17 zdjęcia maski (w górę,
// Δ +2, techniczna), dla E-33 i E-14. M-13 jest w rekordzie bez stanu; M-14, M-15 i M-11 nie mają pola. Warunek
// uspokojenia jest oceniany niepełnie (Stany::ocenaNiepelna; punkt „stan odchylenia M-03” przełącza).
//
// Przebieg (noc po nocy, od najstarszej do `dzis`, A-10): norma nocy n z nocy ważnych przed n (4.4), bez nocy, w których
// ta metryka była w stanie odchylenia (R-NOR-09), a dla AHI bez nocy niepewnych (R-NOR-19). Odchylenie w nocy (4.5):
// poza P90 (P10 dla czasu z maską) i co najmniej Δ od mediany. Wejście w stan: odchylenie w ≥ 3 z 5 ostatnich nocy
// ważnych z wartością (R-NOR-03); noc wejścia jest pierwszą nocą stanu. Wyjście: 3 kolejne noce ważne w granicy Δ/2 od
// mediany normy (R-NOR-04). Metryka zdrowotna w stanie trzyma normę z chwili wejścia (R-NOR-22). Metryka techniczna po
// 21 nocach ważnych w stanie kończy stan, a jej noce wracają do normy (nowy poziom, R-NOR-06; linii R-NOR-13 panel
// jeszcze nie pokazuje). Noc niepewna nie liczy się do reguł AHI (R-NOR-05).
#pragma once
#include <set>
#include <string>
#include <vector>
#include "noc.h"

// 4.9: duży wyciek M-08 ≥ 30 min albo ≥ 10% czasu terapii (M-01) [PRÓG] (od wersji beta; noc bez M-08, np. z karty,
// tylko drugi warunek) albo mediana wycieku nocy > 30 L/min [PRÓG], albo (wariant C) M-07 P95
// nocy ≥ NIEPEWNA_P95 L/min [PRÓG R-NOR-05.niepewna_p95]; brak P95 = tylko pozostałe warunki.
const float NIEPEWNA_P95 = 15.0f;
// E-30 (6.4, katalog E.csv): mediana AHI z E30_OKNO ostatnich nocy ważnych bez niepewnych co najmniej E30_AHI
// [PRÓG E-30.okno], [PRÓG E-30.ahi]; ta sama definicja co w silniku reguł (warunek uspokojenia 5.4.13 pkt 2)
const int E30_OKNO = 7;
const float E30_AHI = 5.0f;
// R-SKI-24: linia `Noce mało czytelne: bezdechów nie oceniam.` przy SKI24_NOCE z SKI24_OKNO ostatnich nocy z terapią
// [PRÓG R-SKI-24.noce_nieczytelne] (noc z terapią: M-01 ≥ 15 min [PRÓG R-SKI-27.noc_z_terapia_m01])
const int SKI24_NOCE = 4, SKI24_OKNO = 7, NOC_Z_TERAPIA_MIN = 15;
// brak ocen z powodu wycieku (5.4.13 pkt 7): okno i liczba nocy jak L-03 [PRÓG L-03.okno], [PRÓG L-03.min_noce]
const int WYCIEK_OKNO = 14, WYCIEK_MIN_NOCE = 10;
bool nocNiepewna(const Noc& n);
// Warunek E-20 nocy (6.4): M-08 ≥ 30 min [PRÓG E-20.minuty] albo ≥ 10% czasu terapii [PRÓG E-20.odsetek_czasu];
// brak M-08 = false (wariant opisowy S-K-10 ustępuje E-20 tego samego dnia, R-KONF-42).
bool warunekE20(const Noc& n);

struct StanMetryki {
  bool aktywny = false;
  bool wyzej = true;     // `wyżej niż zwykle` (false: `niżej niż zwykle`, czas z maską)
  std::string od;        // data poranka pierwszej nocy stanu
  int nocy = 0;          // noce ważne w stanie do `dzis` włącznie (`od {n} nocy`, R-KOK-03)
  // ADR-0039 (linia „dobra zmiana”, R-KON-68): noce ważne z wartością od pierwszej nocy z odchyleniem w oknie wejścia
  // (3 z 5) do `dzis` włącznie; przy wejściu co najmniej 3
  int odPierwszej = 0;
  // R-NOR-22: norma zapisana przy wejściu w stan metryki zdrowotnej (pusta poza stanem i dla metryk technicznych);
  // karty S-K-40…S-K-47 porównują z nią, bo dzisiejsza norma bez nocy stanu (R-NOR-09) po ok. 76 nocach znika
  NormaMetryki zamrozona;
};

struct OdchylenieNocy {  // 4.5 dla jednej nocy (z normą tej nocy)
  bool m01 = false, ahi = false, wp95 = false, ep95 = false;
  bool ca = false, m17 = false;  // M-03ca, M-17
  bool cp95 = false;             // M-05p, noc w trybie CPAP albo AutoSet
  bool jakiekolwiek() const { return m01 || ahi || wp95 || ep95 || ca || m17 || cp95; }
};

struct Stany {
  Norma norma;                 // norma dla nocy `dzis` z wyłączeniami 4.4
  WykluczeniaNormy wyl;        // noce wyłączone z normy (stany odchylenia, noce niepewne)
  StanMetryki m01, ahi, wp95, ep95;
  StanMetryki ca, m17;         // M-03ca (CA/h, zdrowotna) i M-17 (techniczna), dla E-33 i E-14
  // M-05p ciśnienie P95 (w górę, Δ +2 cmH2O, zdrowotna) z nocy w trybie CPAP albo AutoSet;
  // EPAP P95 (`ep95`) tylko z nocy w trybie ASV albo ASVAuto i z nocy bez znanego trybu (zachowanie), 4.11
  StanMetryki cp95;
  // ADR-0039 (R-KON-64): kierunki bez własnej karty, każdy z kartą S-K-40…S-K-47 (sk::odchylenia, src/kontekst_odchylenia.cpp);
  // metryki zdrowotne (4.7): norma zamrożona przy wejściu (R-NOR-22), noce stanu poza normą tego sygnału (R-NOR-09)
  StanMetryki ps;     // M-28 wsparcie PS w górę (tryb ASV, Δ +1 cmH2O [PRÓG]), S-K-40
  StanMetryki m13;    // M-13 oddechy na minutę w górę (Δ +2 / min), S-K-45
  StanMetryki csr;    // M-09 oddychanie okresowe, % nocy, w górę (Δ +5 pp [PRÓG]), S-K-46
  StanMetryki m10;    // M-10 P95 ograniczenie przepływu w górę (Δ +0,15 [PRÓG]), S-K-47
  StanMetryki m15g;   // M-15 wentylacja minutowa w górę (Δ +15% mediany normy [PRÓG]), S-K-41
  StanMetryki m15d;   // M-15 w dół (Δ −15%), S-K-42
  StanMetryki m14d;   // M-14 objętość oddechowa w dół (Δ −15% mediany normy [PRÓG]), S-K-43
  StanMetryki m14g;   // M-14 w górę (Δ +15%), S-K-44
  // „Dobra zmiana” (ADR-0039 pkt 5, R-KON-68): ten sam warunek 3 z 5 w kierunku pożądanym, liczony na tej samej normie;
  // te noce nie wypadają z normy (to nie stan odchylenia z 4.6) i norma nie jest zamrażana
  StanMetryki dobraAhi, dobraPs, dobraWp95, dobraM17, dobraM10, dobraM01;
  std::string ostatnia;        // data poranka ostatniej nocy ≤ dzis ("" = brak)
  bool ostatniaNiepewna = false;
  OdchylenieNocy odchylenie;   // ostatniej nocy, z jej normą
  // zdarzenia z grupy R5b, które da się sprawdzić na danych panelu (6.4)
  bool e30 = false;            // mediana AHI z 7 ostatnich nocy ważnych bez niepewnych ≥ 5 / h (jak E-30 w katalogu)
  bool e36 = false;            // odchylenie ciśnienia trybu (ASV: EPAP P95, CPAP i AutoSet: M-05p) w ≥ 5 z 7 nocy ważnych
  // R-SKI-24: co najmniej 4 z 7 ostatnich nocy z terapią (M-01 ≥ 15 min) nie są nocami ważnymi dla L (nieważne albo
  // niepewne, 5.12.3); przy mniej niż 7 nocach z terapią z dostępnych (linia dopiero przy 4 takich nocach)
  bool nocyNieczytelne = false;
  // 5.4.13 pkt 7: brak ocen bezdechów z powodu wycieku, nie liczby nocy. W ostatnich 14 nocach z terapią co najmniej
  // 10 nocy ważnych (tyle wymaga L-03), ale mniej niż 10 ważnych bez niepewnych: bez nocy niepewnych (wyciek, 4.9)
  // reguła AHI miałaby z czego liczyć. Krok zamienny `S-K-18.krok_dane_wyciek` zamiast `… za mało nocy.`
  bool bezOcenZWycieku = false;
  // 5.4.13: warunek uspokojenia. Panel liczy pkt 2 w części (E-30, E-31, E-36) i pkt 3 w części (stan AHI, EPAP P95).
  // Punkty, których nie ocenia (wzorce L, E-33, E-37, S-K-17, stany M-03, M-11, M-13–M-15, linie „zdrowie”, lista
  // `Metryki niewalidowane`), są w `nieoceniane` (nieocenianeUspokojenia()); dopóki lista nie jest pusta, ocena jest
  // niepełna i uspokojenie = false: panel nie mówi „w normie” o tym, czego nie ocenia (5.4.13 pkt 5).
  std::vector<std::string> nieoceniane;
  bool ocenaNiepelna = true;
  bool uspokojenieOcenione = false;  // punkty liczone przez panel spełnione (bez punktów nieocenianych)
  bool uspokojenie = false;          // = !ocenaNiepelna && uspokojenieOcenione
  // brak otwartej sprawy według punktów, które panel liczy (wzorce L, zdarzenia R5b, stan AHI i EPAP, linie
  // „zdrowie”), bez wymogu, by każda reguła była oceniona (pkt 7: „nie oceniam” z braku danych albo awaria reguły) i bez
  // punktów, których panel nie liczy. Ustawia kolejka::zastosujUspokojenie(); do kart pochwalnych S2d.
  bool bezSpraw = false;
  // (R-EKR-32, 5.4.13 pkt 5 i 7): dlaczego uspokojenie jest niespełnione, do kroku zamiennego. SPRAWA =
  // sprawa, linia „zdrowie” albo ocena bez silnika (`→ Otwarte sprawy: strona 3 / 6.`); REGULY = jakaś reguła
  // „nie oceniam”, a z pozostałych co najwyżej punkty, których panel nie liczy (`→ Część reguł nie działa: zobacz
  // Test sprzętu.`, tylko przy awarii zestawu, R-AW-31); OCENY = tylko punkty, których panel nie liczy (pkt 3 w części, pkt 5: `→ Część ocen wyłączona:
  // zobacz ▶.`). Ustawia kolejka::zastosujUspokojenie(); bez wyników silnika SPRAWA.
  // DANE = jakaś reguła „nie oceniam” z braku danych (za mało nocy, brak metryki), bez
  // awarii zestawu: `→ Część ocen jeszcze nie działa: za mało nocy.` (przy bezOcenZWycieku `→ Bezdechów nie oceniam: noce
  // z wyciekiem.`); REGULY = awaria zestawu albo powód nierozpoznany.
  enum class Niespokoj : unsigned char { BRAK, SPRAWA, REGULY, OCENY, DANE };
  Niespokoj niespokoj = Niespokoj::SPRAWA;
};
// Punkty warunku uspokojenia bez reguły w panelu (tabela PUNKTY_USPOKOJENIA w stany.cpp).
std::vector<std::string> nieocenianeUspokojenia();

Stany liczStany(const std::vector<Noc>& hist, const std::string& dzis, const std::set<std::string>& wykluczone);
// To samo z pamięcią ostatniego wyniku (panel woła to kilka razy w jednym wybudzeniu; liczenie idzie noc po nocy).
const Stany& stanyZPamieci(const std::vector<Noc>& hist, const std::string& dzis, const std::set<std::string>& wykluczone);

// ---------- porównanie tygodnia z normą (S2a, 5.4.2) ----------
// Mediana z nocy z wartością w 7 dniach do `dzis` włącznie, porównana z pasmem P10–P90 normy:
//   w paśmie                          → `↔ jak zwykle`
//   poza pasmem po lepszej stronie    → `↑ lepiej niż zwykle` (czas z maską) albo `↓ lepiej niż zwykle` (AHI, wyciek)
//   poza pasmem po gorszej stronie    → `↑ wyżej niż zwykle` albo `↓ niżej niż zwykle`
//   EPAP (bez strony lepszej, ADR-0002) → `↑ wyżej niż zwykle` albo `↓ niżej niż zwykle`
//   metryka bez normy                 → `uczę się` (jak R-KOK-05)
// Reguła wyboru słowa nie jest zapisana w spec (5.4.2 podaje tylko przykłady słów): propozycja.
enum class Strona : unsigned char { WYZEJ_LEPIEJ, NIZEJ_LEPIEJ, BEZ_OCENY };
struct Porownanie {
  double mediana = NAN;
  std::string slowo;       // z dopiskiem strzałki
  bool spokojne = false;   // `jak zwykle` albo `lepiej niż zwykle` (powód `Tydzień jak zwykle.`)
};
Porownanie porownajTydzien(const std::vector<double>& wartosci7, const NormaMetryki& nm, Strona s);
// Wartości metryki z nocy w 7 dniach do `dzis` włącznie (bez NAN i bez braku czasu z maską).
std::vector<double> wartosci7(const std::vector<Noc>& hist, const std::string& dzis, float (*pole)(const Noc&));
float poleM01(const Noc& n);
float poleAhi(const Noc& n);
float poleEmed(const Noc& n);
float poleWp95(const Noc& n);
