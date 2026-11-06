// SPDX-License-Identifier: GPL-3.0-or-later
// Moduł INFEKCJA (spec 5.10): S-K-30 „Oddychasz szybciej niż zwykle” (R-INF-01), okres
// infekcji po odpowiedzi „tak” (R-INF-02…R-INF-06, R-INF-09), pytanie „gorzej” w okresie (R-INF-07) i linia
// `Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij pomoc.` (R-INF-10). Bez Arduino. Panel niczego nie
// diagnozuje: opisuje pomiar, pyta i zapisuje odpowiedź (5.10.1, ADR-0002).
//
// Stan bez osobnego pliku: odpowiedź na `Katar albo infekcja?` leży w pliku odpowiedzi (src/sprawy.h,
// Pytanie::INFEKCJA, data poranka pytania); okres liczy się z niej i z historii nocy, więc cofnięcie „tak” (R-PRZ-13)
// cofa okres i czynnik bez dodatkowego zapisu. Norma okresu (koniec, pytanie „gorzej”) to norma z dnia odpowiedzi,
// liczona bez nocy okresu (zamrożona na czas okresu, żeby okres nie zmieniał własnej normy).
// Sygnały towarzyszące (5.10.2): w rekordzie nocy są tylko M-05p i M-06p (w trybie ASV, 4.11); M-15 (wentylacja
// minutowa) i puls (M-24, oksymetr) nie są zbierane, więc te sygnały są pomijane jak sygnał bez normy (fail-path
// R-INF-01).
#pragma once
#include <set>
#include <string>
#include <vector>
#include "noc.h"

namespace infekcja {

// próg: S-K-30.wzrost = 2
const float SK30_WZROST = 2;        // M-13 ≥ mediana normy + 2 / min [PRÓG]
// próg: S-K-30.noce = 2
const int SK30_NOCE = 2;            // w 2 kolejnych nocach ważnych [PRÓG]
// próg: S-K-30.m05p = 2
const float SK30_CISNIENIE = 2;     // sygnał towarzyszący M-05p (w ASV M-06p) ≥ mediana normy + Δ [PRÓG]
// próg: R-INF-06.koniec_wzrost = 1
const float KONIEC_WZROST = 1;      // koniec: M-13 < mediana normy + 1 / min [PRÓG]
// próg: R-INF-06.koniec_noce = 2
const int KONIEC_NOCE = 2;          // … przez 2 kolejne noce ważne [PRÓG]
// próg: R-INF-04.maks_noce = 14
const int MAKS_NOCE = 14;           // albo po 14 nocach [PRÓG]
// próg: R-INF-04.karencja = 14
const int KARENCJA_NOCE = 14;       // po okresie zakończonym limitem 14 nocy: bez S-K-30 i bez nowego okresu [PRÓG]
// próg: R-INF-07.wzrost = 3
const float GORZEJ_WZROST = 3;      // pytanie „gorzej” ponawiane przy M-13 ≥ mediana normy + 3 / min [PRÓG]
// próg: R-INF-07.od_nocy = 3
const int GORZEJ_OD_NOCY = 3;       // od 3. nocy okresu [PRÓG]
// próg: R-INF-07.co_dni = 3
const int GORZEJ_CO_DNI = 3;        // co 3 dni [PRÓG]
// próg: R-INF-10.linia_co_dni = 3
const int LINIA_CO_DNI = 3;         // linia okresu najwyżej raz na 3 dni [PRÓG]
extern const char* const LINIA;     // tekst linii R-INF-10 (7.5, grupa „wzorce L”)

struct WynikSK30 {
  bool warunek = false;
  std::string pierwszaNoc;     // pierwsza z 2 nocy wyzwalacza (początek okresu, R-INF-05)
  float x = NAN, norma = NAN;  // `{x}` (ostatnia noc), `{norma}` (mediana normy M-13)
  std::string sygnal;          // stopka prawa: sygnał towarzyszący, np. `ciśnienie też wyższe`
  std::vector<float> linia;    // M-13 z 14 ostatnich nocy (wizualizacja), NAN = brak
  const char* dlaczego = "";
};
// `asv`: tryb ASV albo ASVAuto (sygnał M-06p zamiast M-05p, 4.11). Ostatnia noc ważna musi mieć datę `dzis`.
WynikSK30 ocenSK30(const std::vector<Noc>& hist, const Norma& norma, const std::string& dzis, bool asv);

struct Okres {
  std::string odpowiedz;  // data poranka odpowiedzi „tak”
  std::string od;         // pierwsza noc okresu (data poranka)
  std::string koniec;     // ostatnia noc okresu (włącznie); przy trwającym okresie: od + 13 (najpóźniej)
  bool zakonczony = false;  // koniec znany: 2 noce z M-13 w normie albo 14 nocy (do `dzis`)
  bool koniecPoprawa = false;  // zakończony warunkiem R-INF-06 (rano po nocy końca reguły już działają)
  float normaM13 = NAN;   // mediana normy M-13 z dnia odpowiedzi
};
// Okresy z odpowiedzi „tak” (daty poranka, rosnąco), historii do `dzis` i nocy wyłączonych czynnikami (R-CZY-05).
std::vector<Okres> okresy(const std::vector<Noc>& hist, const std::vector<std::string>& odpowiedziTak,
                          const std::set<std::string>& wykluczone, const std::string& dzis, bool asv);
// Noc `data` w okresie infekcji (czynnik „katar/infekcja”, wypada z normy: R-INF-05, R-CZY-05).
bool nocWOkresie(const std::vector<Okres>& o, const std::string& data);
std::set<std::string> noceOkresow(const std::vector<Okres>& o, const std::string& dzis);
// po okresie zakończonym limitem 14 nocy (R-INF-04, oddechy wciąż wyżej) przez KARENCJA_NOCE dni
// działa zwykła ścieżka S-K-17 i L-05 (5.10.3 „Po 14 nocach”): S-K-30 nie wraca, „tak” nie otwiera nowego okresu.
bool karencja(const std::vector<Okres>& o, const std::string& dzis);
// ostatni dzień karencji trwającej w `dzis` (koniec okresu + KARENCJA_NOCE), "" = bez karencji; do logu.
std::string koniecKarencji(const std::vector<Okres>& o, const std::string& dzis);
// Bramka „okres infekcji” (7.3) dla oceny z rana `dzis` (noc `dzis`). nullptr = brak okresu.
const Okres* okresDzis(const std::vector<Okres>& o, const std::string& dzis);

// R-INF-07: pytanie „gorzej” w okresie od jego 3. nocy, gdy (a) nie padło w tym okresie albo (b) od ostatniego minęły
// ≥ 3 dni i M-13 ostatniej nocy z wartością ≥ mediana normy + 3 / min. `pytaniaGorzej` = daty poranka pytań „gorzej”.
bool pytanieGorzej(const Okres& o, const std::vector<Noc>& hist, const std::vector<std::string>& pytaniaGorzej,
                   const std::string& dzis);
// R-INF-10: linia w pierwszym dniu okresu, potem najwyżej raz na 3 dni od rzeczywistego pokazania (`ostatniePokazanie`).
bool liniaDzis(const Okres& o, const std::string& dzis, const std::string& ostatniePokazanie);

}  // namespace infekcja
