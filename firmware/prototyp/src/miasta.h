// SPDX-License-Identifier: GPL-3.0-or-later
// Miasta do pogody i wschodu słońca (spec 5.6.4 „Miasto do pogody”, 5.6.3 U-03): wbudowana tabela nazwa →
// szerokość i długość geograficzna, żeby wybór miasta w formularzu nie wymagał żadnego zapytania do internetu
// (ADR-0001, R-NF-07). Bez Arduino. Wybór wygrywa z `POGODA_LAT` i `POGODA_LON` z `secrets.h` (dyrygent).
#pragma once
#include <string>

namespace miasta {

struct Miasto {
  const char* nazwa;
  float szer, dl;  // stopnie, dokładność 0,01° (ok. 1 km: dla prognozy godzinowej i wschodu słońca wystarcza)
};
extern const Miasto MIASTA[];  // alfabetycznie (kolejność polska), 18 miast wojewódzkich (województwa kujawsko-pomorskie i lubuskie mają po dwa)
extern const int MIASTA_N;
// Miasto o tej nazwie (dokładnie jak w tabeli) albo nullptr.
const Miasto* znajdz(const std::string& nazwa);

// Kontrola tabeli: Polska leży w 49,0…54,9°N i 14,1…24,2°E (z zapasem na granicę 48,9…55,0 i 14,0…24,3).
bool wPolsce(double szer, double dl);

}  // namespace miasta
