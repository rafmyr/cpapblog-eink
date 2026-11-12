// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: podpisy w stopkach ekranów są w kolejności
// przycisków na obudowie, `◀` (`<`), `▶` (`>`), `↻` (zielony). Znaczenie przycisków się nie zmienia, tylko miejsce
// podpisu. Test sprawdza źródła tekstów stopek i porządkowanie przy rysowaniu. Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_kolejnosc_przyciskow_stopki).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "kokpit.h"
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
  // 5.5.7 „Stopki stron” (ADR-0041): K2, sprawy, U-01 i „Co znaczą te liczby” we wszystkich wariantach celu
  for (bool spr : {false, true})
    for (bool lb : {false, true}) {
      SPRAWDZ(kolejnoscOk(stopkaK2(spr, lb)) && kolejnoscOk(stopkaU01(spr, lb)));
      SPRAWDZ(szerStopki(stopkaK2(spr, lb)) <= 744 && szerStopki(stopkaU01(spr, lb)) <= 744);
      for (int i = 1; i <= 3; i++) SPRAWDZ(kolejnoscOk(stopkaSpraw(i, 3, lb)));
    }
  for (bool spr : {false, true}) SPRAWDZ(kolejnoscOk(podsumowanie::budujLiczby(reguly::Tryb::ASV, true, spr).stopkaLewo));
  SPRAWDZ(szerStopki(stopkaK1(true)) <= 744 && szerStopki(stopkaK2()) <= 744 && szerStopki(stopkaPytania()) <= 744);
}

// Płótno w pamięci 800×480 (bit 1 = czarny)
struct PlotnoT : Plotno {
  std::vector<uint8_t> b = std::vector<uint8_t>(800 / 8 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x < 0 || y < 0 || x >= 800 || y >= 480) return;
    uint8_t m = (uint8_t)(0x80 >> (x & 7));
    if (c) b[(size_t)(y * 100 + x / 8)] |= m;
    else b[(size_t)(y * 100 + x / 8)] &= (uint8_t)~m;
  }
};
// pierwszy wiersz, w którym klatki się różnią; 480 = takie same
static int pierwszaRoznica(const PlotnoT& a, const PlotnoT& b) {
  for (int y = 0; y < 480; y++)
    for (int i = 0; i < 100; i++)
      if (a.b[(size_t)(y * 100 + i)] != b.b[(size_t)(y * 100 + i)]) return y;
  return 480;
}

// R-POR-24, R-PRZ-04: podpis ▶ na ekranie L rano z Portretem pod ▶ i podpis ◀ w K1 otwartym
// z Portretu mówią, dokąd prowadzi przycisk; kolejność ◀ ▶ ↻, szerokość w 744 px (L z ramką: w kolumnie treści), a
// rysunek różni się od zwykłego tylko w pasie stopki (od y 434).
static void testStopkiPortretu() {
  const std::string dalej = "▶ portret nocy", wstecz = "◀ portret nocy";
  for (bool lek : {false, true}) {
    std::vector<std::string> g = stopkaWzorca(lek, dalej);
    SPRAWDZ(kolejnoscOk(g) && szerStopki(g) <= szerTresciWzorca(lek) && szerStopki(g) <= 744);
    SPRAWDZ(g[lek ? 1 : 0] == (lek ? dalej + " · 2 s raport" : dalej));
    SPRAWDZ(stopkaWzorca(lek, "") == stopkaWzorca(lek));  // bez Portretu jak dotąd: `▶ kokpit`
  }
  for (bool noc : {false, true}) {
    std::vector<std::string> g = stopkaK1(noc, wstecz);
    SPRAWDZ(kolejnoscOk(g) && szerStopki(g) <= 744 && g[0] == wstecz);
    SPRAWDZ(stopkaK1(noc, "")[0] == "◀ ekran dnia");
  }
  DaneK1 k;
  k.naglowek = "Ostatnia noc · śr 04.11";
  PlotnoT a, b;
  rysujK1(a, k);
  k.stopkaWstecz = wstecz;
  rysujK1(b, k);
  int y = pierwszaRoznica(a, b);
  SPRAWDZ(y >= 434 && y < 480);
  DaneWzorca w;
  w.naglowek = {"Ostatnia noc", ""};
  w.lekarz = false;
  w.tytul = "Wyciek wysoki";
  PlotnoT c, e;
  rysujWzorzec(c, w);
  w.stopkaDalej = dalej;
  rysujWzorzec(e, w);
  y = pierwszaRoznica(c, e);
  SPRAWDZ(y >= 434 && y < 480);
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
  testStopkiPortretu();
  printf("testy_kolejnosc_przyciskow_stopki: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
