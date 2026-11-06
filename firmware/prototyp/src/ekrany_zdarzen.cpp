// SPDX-License-Identifier: GPL-3.0-or-later
// Ekrany zdarzeń E w tonie „problem” (S3), src/ekrany_zdarzen.h.
#include "ekrany_zdarzen.h"

#include <cmath>

#include "kokpit.h"
#include "reguly.h"
#include "wspolne.h"

namespace zdarzenia {

namespace {
bool rodzinaAsv(reguly::Tryb t) { return t == reguly::Tryb::ASV || t == reguly::Tryb::ASVAUTO || t == reguly::Tryb::NIEZNANY; }
}  // namespace

std::string metrykaWykresu(const sprawy::SprawaL& s, reguly::Tryb tryb) {
  // bez znanego trybu jak K1 i K2 (układ ASV)
  return rodzinaAsv(tryb) ? s.metrykaWiz : s.metrykaWizCpap;
}

double medianaNormy(const Norma& nm, const std::string& m) {
  const NormaMetryki* x = nullptr;
  if (m == "M-01") x = &nm.m01;
  else if (m == "M-02") x = &nm.ahi;
  else if (m == "M-03ca") x = &nm.ca;
  else if (m == "M-05p") x = &nm.cp95;
  else if (m == "M-06p") x = &nm.ep95;
  else if (m == "M-07") x = &nm.wp95;
  else if (m == "M-13") x = &nm.m13;
  else if (m == "M-17") x = &nm.m17;
  return x && x->jest() ? x->med : NAN;
}

DaneWzorca trescZdarzenia(const sprawy::SprawaL& s, const DaneDnia& d) {
  DaneWzorca w;
  w.naglowek = {naglowekDniaLewo(d), naglowekDniaPrawo(d)};
  w.lekarz = false;  // ton „problem”: negatyw bez ramki i bez znaku ostrzeżenia (5.4.3)
  w.tytul = s.tytul;
  w.powod = s.powod;
  w.krok = s.krok;
  w.podpisLewo = s.podpisLewo;  // ról podpisów spec dla E nie ma: puste (zestaw może je kiedyś dodać)
  w.podpisPrawo = s.podpisPrawo;
  w.linia = d.linia;
  w.liniaA = d.liniaA;
  w.liniaOstrzezenie = d.liniaOstrzezenie;
  if (!d.hist || !dataOk(d.dzis)) return w;
  std::string m = metrykaWykresu(s, trybBiezacy(*d.hist, d.dzis));
  if (m.empty()) return w;
  bool jest = false;
  for (int i = 0; i < NOCY_WYKRESU; i++) {
    const Noc* n = ::nocZDnia(*d.hist, dodajDni(d.dzis, i - (NOCY_WYKRESU - 1)));
    float v = n ? (float)reguly::metryka(*n, m) : NAN;
    jest = jest || !std::isnan(v);
    w.wartosci.push_back(v);
  }
  if (!jest) {
    w.wartosci.clear();
    return w;
  }
  w.wiz = WizWzorca::LINIA;
  // linia przerywana: próg zdarzenia (E-30 5 / h, E-23 27 l/min, E-22 30 min, E-12 4 h), inaczej mediana normy
  // (E-13, E-14, E-21, E-31, E-33, E-36: zdarzenia ze stanu odchylenia, 4.6)
  double prog = s.progLinii;
  if (std::isnan(prog)) prog = medianaNormy(d.stany ? d.stany->norma : d.norma, m);
  w.prog = (float)prog;
  return w;
}

}  // namespace zdarzenia
