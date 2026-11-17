// SPDX-License-Identifier: GPL-3.0-or-later
// Rodzaj odświeżenia e-ink (R-NF-25): opis w src/odswiezanie.h.
#include "odswiezanie.h"

namespace odswiezanie {

const char* const KLUCZ_PELNE_CO = "eink.pelne_co";

bool ekranCzesciowy(Ekran e) {
  switch (e) {
    case Ekran::K1:
    case Ekran::K2:
    case Ekran::PYTANIE:
    case Ekran::OMOWIONE:
    case Ekran::SPRAWY:
    case Ekran::PRZENIESIONE:
    case Ekran::ZAPISANO:
    case Ekran::LICZBY:
    case Ekran::SPRZET_ZAPISANO:
    case Ekran::USTAWIENIA:
    case Ekran::PORTRET:
    case Ekran::SYPIALNIA:
      return true;
    default:  // DZIEN (w tym S9), TEST, RAPORT, FORMULARZ, SIEC_PANELU, AKTUALIZACJA, wycofane NORMA i SPRZET
      return false;
  }
}

Odswiezenie rodzajOdswiezenia(const Wejscie& w) {
  if (w.pelneCo <= 0 || w.zawszePelne || w.naPlotnieDzien || !w.poPrzycisku || !w.obrazZnany ||
      !ekranCzesciowy(w.ekran))
    return Odswiezenie::PELNE;
  if (w.czesciowychZRzedu >= w.pelneCo) return Odswiezenie::PELNE;  // co N-te częściowe: pełne
  return Odswiezenie::CZESCIOWE;
}

uint8_t licznikPo(Odswiezenie wykonane, int przed) {
  if (wykonane != Odswiezenie::CZESCIOWE) return 0;
  if (przed < 0) przed = 0;
  return przed >= 255 ? (uint8_t)255 : (uint8_t)(przed + 1);
}

const char* nazwa(Odswiezenie r) { return r == Odswiezenie::CZESCIOWE ? "czesciowe" : "pelne"; }

bool pelneCoZTekstu(const std::string& v, int& n) {
  if (v.empty() || v.size() > 2) return false;
  int x = 0;
  for (char c : v) {
    if (c < '0' || c > '9') return false;
    x = x * 10 + (c - '0');
  }
  if (x > PELNE_CO_MAX) return false;
  n = x;
  return true;
}

}  // namespace odswiezanie
