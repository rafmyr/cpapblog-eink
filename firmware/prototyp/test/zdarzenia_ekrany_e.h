// SPDX-License-Identifier: GPL-3.0-or-later
// Wspólne dla test/testy_ekrany_zdarzen_e.cpp i test/podglad.cpp: wyniki zdarzeń E z poziomem „problem”
// (6.4, grupy R5b–R5d) z przykładowymi zmiennymi tekstów (dane syntetyczne, WY12) na zestawie wbudowanym. Rodzaje
// spoza rdzenia rejestrowane przed pierwszym reguly::zestawWbudowany(), jak w src/dyrygent.cpp.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "reguly.h"
#include "sprawy.h"
#include "wzorce.h"

namespace reguly {
void zarejestrujRodzajeE();  // src/rodzaje_e.cpp
}

namespace b14 {

inline const reguly::Zestaw& zestaw() {
  static bool rodzaje = false;
  if (!rodzaje) {
    reguly::zarejestrujRodzajeE();
    wzorce::zarejestrujRodzajeL();
    rodzaje = true;
  }
  return reguly::zestawWbudowany();
}

// Zdarzenie E z ekranem S3 (6.4): klucz wyniku → zmienne tekstów. `najdluzsze` = największe wartości, jakie dają dane
// (liczby nocy dwucyfrowe, wartości z przecinkiem, czas `12 h 59`), do testu limitów 5.4.4.
inline std::map<std::string, std::map<std::string, std::string>> zmienneE(bool najdluzsze) {
  if (najdluzsze)
    return {{"E-30", {{"n", "7"}, {"x", "99,9"}}},
            {"E-31/obserwuj", {{"x", "99,9"}, {"norma", "99,9"}}},
            {"E-31/lekarz", {{"x", "99,9"}, {"norma", "99,9"}}},
            {"E-33", {{"x", "99,9"}, {"norma", "99,9"}}},
            {"E-36/obserwuj", {{"x", "99,9"}, {"norma", "99,9"}}},
            {"E-36/lekarz", {{"x", "99,9"}, {"norma", "99,9"}}},
            {"E-23", {}},
            {"E-22", {{"n", "5"}, {"min", "999"}}},
            {"E-21", {{"n", "99"}, {"x", "99,9"}, {"norma", "99,9"}}},
            {"E-10/problem", {{"n", "99"}, {"data", "30.12"}}},
            {"E-12", {{"n", "29"}, {"d", "30"}, {"ceil(0,7·d)", "21"}}},
            {"E-13", {{"x", "12 h 59"}, {"norma", "12 h 59"}}},
            {"E-14", {{"x", "99"}, {"norma", "99"}}}};
  return {{"E-30", {{"n", "6"}, {"x", "14,2"}}},
          {"E-31/obserwuj", {{"x", "9,8"}, {"norma", "1,2"}}},
          {"E-31/lekarz", {{"x", "9,8"}, {"norma", "1,2"}}},
          {"E-33", {{"x", "7,5"}, {"norma", "0,7"}}},
          {"E-36/obserwuj", {{"x", "12,4"}, {"norma", "9,1"}}},
          {"E-36/lekarz", {{"x", "12,4"}, {"norma", "9,1"}}},
          {"E-23", {}},
          {"E-22", {{"n", "4"}, {"min", "64"}}},
          {"E-21", {{"n", "12"}, {"x", "31"}, {"norma", "9"}}},
          {"E-10/problem", {{"n", "2"}, {"data", "10.11"}}},
          {"E-12", {{"n", "12"}, {"d", "30"}, {"ceil(0,7·d)", "21"}}},
          {"E-13", {{"x", "5 h 10"}, {"norma", "7 h 20"}}},
          {"E-14", {{"x", "4"}, {"norma", "1"}}}};
}

// Wynik spełniony z polami wiersza katalogu (poziom, grupa, stan); `zm` = zmienne tekstów.
inline reguly::Wynik wynikE(const std::string& kl, const std::map<std::string, std::string>& zm) {
  reguly::Wynik w;
  size_t u = kl.find('/');
  w.id = kl.substr(0, u);
  w.wariant = u == std::string::npos ? "" : kl.substr(u + 1);
  w.oceniona = w.spelniona = true;
  for (auto& k : zestaw().katalog)
    if (k.id == w.id && k.wariant == w.wariant) {
      w.poziom = k.poziom;
      w.grupa = k.grupa;
    }
  w.zmienne = zm;
  return w;
}

inline sprawy::FunkcjaTekstu funkcjaTekstu(reguly::Tryb t = reguly::Tryb::ASVAUTO) {
  return [t](const std::string& id, const reguly::Wynik& w) { return reguly::tekst(zestaw(), id, w, t); };
}

// Sprawa S3 jednego zdarzenia (sprawy::sprawyOtwarte na jednym wyniku).
inline sprawy::SprawaL sprawaE(const std::string& kl, bool najdluzsze = false,
                               reguly::Tryb t = reguly::Tryb::ASVAUTO) {
  std::vector<reguly::Wynik> w = {wynikE(kl, zmienneE(najdluzsze)[kl])};
  reguly::Pamiec p;
  std::vector<sprawy::SprawaL> s = sprawy::sprawyOtwarte(zestaw(), w, p, funkcjaTekstu(t));
  return s.empty() ? sprawy::SprawaL() : s.front();
}

}  // namespace b14
