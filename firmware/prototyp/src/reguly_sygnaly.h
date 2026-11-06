// SPDX-License-Identifier: GPL-3.0-or-later
// Sygnały komponentów dla rodzajów `sygnal_komponentu` i `prog_biezacy` (spec techniczna 9.3: „warunek liczy komponent źródła danych,
// katalog daje tylko poziom, grupę i teksty”; E-01, E-03, E-04, E-07, E-08).
// Rozszerzenie obok kontraktu src/reguly.h (Kontekst nie ma pola na sygnały, a kontrakt się nie zmienia): sygnały
// podaje się w OpcjeOceny (src/reguly_pomoc.h) do reguly::ocen(); rodzaj czyta je przez reguly::sygnalyOceny().
#pragma once
#include <map>
#include <string>
#include <vector>

namespace reguly {

// Nazwa sygnału = wartość parametru `sygnal` wiersza katalogu (np. `brak_danych`, `nieczytelny_raport`).
// Sygnał nieobecny w mapie = komponent nic nie powiedział → reguła „nie oceniam” (nigdy „w porządku”).
struct Sygnaly {
  std::map<std::string, bool> stan;                 // nazwa → warunek spełniony
  std::map<std::string, std::string> zmienne;       // dodatkowe zmienne tekstów, np. {"n", "3"}; trafiają do Wynik
  // Bieżące odczyty dla rodzaju `prog_biezacy` (9.3: odczyt czujnika albo prognoza wobec progu): nazwa metryki
  // (parametr `metryka`, np. `M-25` bateria w %, `M-23co2`) → wartość; brak klucza albo NAN = „nie oceniam”.
  std::map<std::string, double> odczyty;
  // Wartości sezonu dla rodzaju `percentyl_sezonu` (S-K-06, S-K-21): nazwa (jak w `odczyty`) → wartości z sezonu
  // (np. prognozy pyłku z bieżącego sezonu); podaje komponent otoczenia. Brak = „nie oceniam”.
  std::map<std::string, std::vector<double>> serie;
};

}  // namespace reguly
