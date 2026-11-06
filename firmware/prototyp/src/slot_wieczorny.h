// SPDX-License-Identifier: GPL-3.0-or-later
// Okno slotu wieczornego i początek slotu tygodniowego (spec 5.8.2, R-KON-09, R-KON-17, R-KON-08),
// bez zależności od Arduino. Plan wybudzeń (src/plan.h) bierze stąd chwile ocen co 30 min (R-KON-01), a dyrygent
// (src/dyrygent.cpp rysujKontekstB9) okno, w którym ocenia ekrany wieczorne S-K-01 i S-K-02.
//
// R-KON-09: okno od 90 min [PRÓG] przed medianą M-18 (start terapii, minuty od 12:00 dnia wieczoru) z ostatnich 14
// [PRÓG] nocy z terapią, z co najmniej 3 [PRÓG] takich nocy, nie wcześniej niż 19:00 [PRÓG], do 23:30 [PRÓG]; mniej
// niż 3 noce ze znanym startem: okno stałe 21:00–23:30 [PRÓG] (R-KON-17). M-18 jest w rekordzie nocy po
// przeglądu (Noc::m18, `noce` wersja 3, z `sessions[].start_ms` raportu AirBridge); noce z karty go nie mają.
#pragma once
#include <string>
#include <vector>

#include "noc.h"

namespace slot_wieczorny {

const int OKNO_STALE_OD = 21 * 60;     // [PRÓG] R-KON-17
const int OKNO_NAJWCZESNIEJ = 19 * 60;  // [PRÓG] R-KON-09
const int OKNO_DO = 23 * 60 + 30;       // [PRÓG] R-KON-09: początek slotu nocnego
const int PRZED_MEDIANA_MIN = 90;       // [PRÓG] R-KON-09
const int NOCY_OKNA = 14;               // [PRÓG] R-KON-09
const int NOCY_MIN = 3;                 // [PRÓG] R-KON-09, R-KON-17
const int TYDZIEN_OD = 17 * 60, TYDZIEN_DO = 19 * 60;  // [PRÓG] 5.8.2: niedziela 17:00–19:00 (R-KON-08)

// Początek okna (minuta doby) z listy startów terapii (minuty od 12:00 dnia wieczoru, M-18; -1 = start nieznany,
// pominięty, fail-path R-KON-09) ostatnich nocy z terapią, od najstarszej. -1 = okno puste (początek po 23:30).
int oknoZeStartow(const std::vector<int>& startyOd12);
// Początek okna dla dnia `dzis` z historii nocy: 14 ostatnich nocy z terapią (M-01 ≥ 15 min) do `dzis`.
int oknoZHistorii(const std::vector<Noc>& hist, const std::string& dzis);
// Czy minuta `m` leży w oknie slotu wieczornego zaczynającym się o `od` (do 23:30).
bool wOknie(int od, int m);
// Slot tygodniowy: niedziela (data poranka) 17:00–19:00.
bool slotTygodniowy(const std::string& dzis, int m);

}  // namespace slot_wieczorny
