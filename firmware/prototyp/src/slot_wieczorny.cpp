// SPDX-License-Identifier: GPL-3.0-or-later
// Okno slotu wieczornego i slot tygodniowy (src/slot_wieczorny.h).
#include "slot_wieczorny.h"

#include <algorithm>

#include "wspolne.h"

namespace slot_wieczorny {

int oknoZeStartow(const std::vector<int>& startyOd12) {
  std::vector<int> s;
  for (size_t i = startyOd12.size(); i-- > 0 && (int)s.size() < NOCY_OKNA;)
    if (startyOd12[i] >= 0) s.push_back(startyOd12[i]);
  if ((int)s.size() < NOCY_MIN) return OKNO_STALE_OD;  // R-KON-17
  std::sort(s.begin(), s.end());
  size_t n = s.size();
  int med = n % 2 ? s[n / 2] : (s[n / 2 - 1] + s[n / 2]) / 2;  // minuty od 12:00 (start po północy > 720)
  int od = 12 * 60 + med - PRZED_MEDIANA_MIN;
  if (od < OKNO_NAJWCZESNIEJ) od = OKNO_NAJWCZESNIEJ;
  return od >= OKNO_DO ? -1 : od;
}

int oknoZHistorii(const std::vector<Noc>& hist, const std::string& dzis) {
  // ostatnie 14 nocy z terapią (M-01 ≥ 15 min, próg E-10 [PRÓG]) do nocy z poranka `dzis` włącznie, od najstarszej;
  // noc bez znanego startu daje -1 i nie liczy się (fail-path R-KON-09, R-KON-17)
  std::vector<int> s;
  for (size_t i = hist.size(); i-- > 0 && (int)s.size() < NOCY_OKNA;) {
    const Noc& n = hist[i];
    if (n.data > dzis || n.czesciowa || n.m01 < 15) continue;
    s.push_back(n.m18);
  }
  std::reverse(s.begin(), s.end());
  return oknoZeStartow(s);
}

bool wOknie(int od, int m) { return od >= 0 && m >= od && m < OKNO_DO; }

bool slotTygodniowy(const std::string& dzis, int m) {
  return dataOk(dzis) && dzienTygodnia(dzis) == 0 && m >= TYDZIEN_OD && m < TYDZIEN_DO;
}

}  // namespace slot_wieczorny
