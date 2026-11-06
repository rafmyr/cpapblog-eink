// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: podpisy w stopkach ekranów są w kolejności
// przycisków na obudowie, `◀` (`<`), `▶` (`>`), `↻` (zielony). Znaczenie przycisków się nie zmienia, tylko miejsce
// podpisu. Test sprawdza źródła tekstów stopek i porządkowanie przy rysowaniu. Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_kolejnosc_przyciskow_stopki).
#include <cstdio>
#include <string>
#include <vector>

#include "ekrany.h"
#include "podsumowanie.h"
#include "sprzet.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

// kolejność ◀ ▶ ↻: po `↻` nie ma już `▶` ani `◀`, po `▶` nie ma już `◀`
static bool kolejnoscOk(const std::string& s) {
  size_t o = s.rfind("◀"), p = s.find("▶"), r = s.find("↻");
  if (o != std::string::npos && p != std::string::npos && o > p) return false;
  if (p != std::string::npos && r != std::string::npos && r < s.rfind("▶")) return false;
  if (o != std::string::npos && r != std::string::npos && r < o) return false;
  return true;
}
static bool kolejnoscOk(const std::vector<std::string>& g) {
  std::string s;
  for (auto& x : g) s += x + "\n";
  return kolejnoscOk(s);
}

static void testZrodlaStopek() {
  SPRAWDZ(kolejnoscOk(stopkaK1(false)) && kolejnoscOk(stopkaK1(true)));
  SPRAWDZ(kolejnoscOk(stopkaK2()));
  SPRAWDZ(kolejnoscOk(stopkaPytania()));
  SPRAWDZ(kolejnoscOk(stopkaWzorca(true)) && kolejnoscOk(stopkaWzorca(false)));
  for (bool aw : {false, true}) SPRAWDZ(kolejnoscOk(stopkaTestu(aw)));
  for (int n : {0, 1, 3, 12})
    for (int i = 0; i <= n; i++) SPRAWDZ(kolejnoscOk(stopkaSpraw(i, n)));
  for (int k = 0; k < 6; k++) SPRAWDZ(kolejnoscOk(sprzet::stopkaSprzetu(k)));
  SPRAWDZ(kolejnoscOk(podsumowanie::DaneS8().stopkaLewo));
  SPRAWDZ(szerStopki(stopkaK1(true)) <= 744 && szerStopki(stopkaK2()) <= 744 && szerStopki(stopkaPytania()) <= 744);
  std::string s = sprzet::stopkaSprzetu(0);
  SPRAWDZ(szerStopki({s}) <= 744);
}

static void testPorzadkowanie() {
  std::vector<std::string> g = {"↻ b", "◀ a", "▶ c", "◀ d", "bez ikony", "↻ e", "▶ f"};
  uporzadkujPrzyciski(g);
  // stabilnie według pierwszego znaku; grupa bez ikony zostaje przy poprzedniej
  SPRAWDZ((g == std::vector<std::string>{"◀ a", "◀ d", "bez ikony", "▶ c", "▶ f", "↻ b", "↻ e"}));
  std::vector<std::string> p;
  uporzadkujPrzyciski(p);
  SPRAWDZ(p.empty());
  std::vector<std::string> j = {"↻ ekran dnia", "▶ raport", "◀ wróć"};
  uporzadkujPrzyciski(j);
  SPRAWDZ(kolejnoscOk(j) && j[0] == "◀ wróć");
}

int main() {
  testZrodlaStopek();
  testPorzadkowanie();
  printf("testy_kolejnosc_przyciskow_stopki: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
