// SPDX-License-Identifier: GPL-3.0-or-later
// Kolejność i widoczność ekranów z telefonu (ADR-0045, R-KON-71, T-TEST-436): dyrygent przekazuje `dzien.ukryte` z pliku
// konfiguracji do slotu dziennego (WejscieSlotu::uklad). Test z przeglądu: mutacja
// `we.uklad = us.ekrany.dzien` → brak przeżywała wszystkie wcześniejsze testy. Harness z test/testy_slot_dzienny_dyrygent.cpp
// (dołączony, jego main() przemianowany); blokujący w tools/prototyp/testy.sh.
#define main main_testy_slot_dzienny_dyrygent
#include "testy_slot_dzienny_dyrygent.cpp"
#undef main

// wymaganie: R-KON-71
int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  const std::string kf = "wymiana.filtr=2026-10-07\nokres.filtr_tyg=4\n";  // termin 4.11: S-K-35 w slocie dziennym
  DobaBaterii jest = dobaBaterii(false, stanStaly(V78, Zasilanie::BATERIA), kf, ep(2026, 11, 4, 12, 30));
  DobaBaterii ukryty =
      dobaBaterii(false, stanStaly(V78, Zasilanie::BATERIA), kf + "dzien.ukryte=wymiana\n", ep(2026, 11, 4, 12, 30));
  bool a = logMa(jest.v, "slot dzienny: S-K-35"), b = logMa(ukryty.v, "slot dzienny: S-K-35");
  printf("  dyrygent: `dzien.ukryte=wymiana` wyłącza S-K-35 w slocie dziennym (bez: %s, z: %s)\n", a ? "S-K-35" : "brak",
         b ? "S-K-35" : "brak");
  SPRAWDZ(a && !b);
  printf("testy_kolejnosc_slot_dyrygent: %d błędów\n", zle);
  return zle ? 1 : 0;
}
