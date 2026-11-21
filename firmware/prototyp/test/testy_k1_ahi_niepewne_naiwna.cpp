// SPDX-License-Identifier: GPL-3.0-or-later
// Naiwna definicja R-KOK-37 kontra budujK1() na losowych historiach. Kontrfakt normy: liczNorme() z tymi samymi
// wyłączeniami co norma stanów, tylko bez nocy niepewnych w WykluczeniaNormy::ahi; kontrfakt trendu: liczenie nocy
// w oknach 7 i 23 od zera. Tryb 0: wejścia spójne (wykluczone normy = czynniki K1); tryb 1: jak dyrygent
// (stany z wykluczoneB16 = czynniki + noce okresów infekcji R-INF-05, K1 z samymi czynnikami).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include "czynniki.h"
#include "ekrany.h"
#include "kokpit.h"
#include "noc.h"
#include "stany.h"
#include "wspolne.h"

static int losI(std::mt19937& g, int a, int b) { return std::uniform_int_distribution<int>(a, b)(g); }
static float los(std::mt19937& g, float a, float b) { return std::uniform_real_distribution<float>(a, b)(g); }

int main(int argc, char** argv) {
  int N = argc > 1 ? atoi(argv[1]) : 3000;
  int trybWej = argc > 2 ? atoi(argv[2]) : 0;
  int przyp = 0, niepewneKod = 0, niepewneNaiw = 0, falszNiep = 0, falszUcze = 0, zlaForma = 0;
  int wgPrzyczyny[4] = {0, 0, 0, 0};
  for (int s = 1; s <= N; s++) {
    std::mt19937 g(s * 7919u);
    std::vector<Noc> h;
    std::vector<WpisCzynnikow> cz;
    std::set<std::string> inf;
    int nocy = losI(g, 8, 110), dzien = 0;
    const char* TR[] = {"AutoSet", "ASVAuto", "CPAP", "ASV"};
    const char* tr = TR[losI(g, 0, 3)];
    double pNiep = los(g, 0, 0.7f);
    int okresWysoki = losI(g, 0, 2) == 0 ? losI(g, 0, nocy) : -1;
    for (int i = 0; i < nocy; i++) {
      dzien += losI(g, 0, 8) == 0 ? losI(g, 2, 6) : 1;
      Noc n;
      n.data = dodajDni("2026-07-01", dzien);
      n.wazna = losI(g, 0, 11) != 0;
      n.m01 = 420;
      n.ahi = losI(g, 0, 14) == 0 ? NAN : los(g, 0.5f, 3.0f) + ((okresWysoki >= 0 && i >= okresWysoki && i < okresWysoki + 12) ? 6.0f : 0.0f);
      bool niep = los(g, 0, 1) < pNiep;
      int jak = losI(g, 0, 2);
      n.wp95 = niep && jak == 0 ? los(g, 15.5f, 40) : los(g, 3, 14);
      n.wmed = niep && jak == 1 ? 32 : 4;
      n.m08 = niep && jak == 2 ? 40 : NAN;
      n.cmed = 9; n.cp95 = 10; n.oa = .3f; n.ca = .2f; n.h = .4f; n.m17 = 0;
      n.tryb = losI(g, 0, 25) == 0 ? "BiLevel" : tr;  // czasem tryb spoza wydania
      n.ustawienia = n.tryb + std::string("/6/12/2");
      if (losI(g, 0, 30) == 0) { n.tryb = ""; n.ustawienia = ""; n.zrodlo = 'A'; }
      h.push_back(n);
      if (losI(g, 0, 15) == 0) { WpisCzynnikow w; w.data = n.data; w.stan = 'O'; w.czynniki = CZ_KATAR; cz.push_back(w); }
      if (trybWej == 1 && losI(g, 0, 8) == 0) inf.insert(n.data);
    }
    std::string ost = h.back().data;
    std::string dzis = losI(g, 0, 3) == 0 ? dodajDni(ost, losI(g, 1, 3)) : ost;
    std::set<std::string> W = wykluczoneZNormy(cz);
    for (auto& d : inf) W.insert(d);
    Stany st = liczStany(h, dzis, W);
    DaneK1 k1 = budujK1(h, dzis, st.norma, cz, &st);
    int r = -1;
    for (int i = 0; i < k1.wierszy; i++) if (std::string(k1.w[i].nazwa) == "AHI") r = i;
    if (r < 0) continue;
    przyp++;
    const WierszK1& w = k1.w[r];
    bool kod = w.slowo == "niepewne";
    // naiwnie
    bool naiw = false; int przyczyna = 0;
    if (!st.ahi.aktywny) {
      if (!st.norma.ahi.jest()) {
        WykluczeniaNormy wm2 = st.wyl;
        for (auto& n : h) if (nocNiepewna(n)) wm2.ahi.erase(n.data);
        Norma n2 = liczNorme(h, dzis, W, &wm2);
        naiw = n2.ahi.jest(); przyczyna = 1;
      } else {
        const Noc* on = nullptr;
        for (auto& n : h) if (n.data <= dzis && (!on || n.data >= on->data)) on = &n;
        std::string koniec = on ? on->data : dzis;
        // tryb nocy jak trybyOkna(): pole tryb albo ustawienia; bez obu: źródło K = ostatni znany, A = obcy
        auto obsl = [](const std::string& t) { return t == "CPAP" || t == "AutoSet" || t == "ASV" || t == "ASVAuto"; };
        int nowe[2] = {0, 0}, stare[2] = {0, 0};
        for (int i = 0; i < 30; i++) {
          std::string d = dodajDni(koniec, i - 29);
          const Noc* n = nullptr;
          for (auto& x : h) if (x.data == d) { n = &x; break; }
          if (!n || std::isnan(n->ahi)) continue;
          std::string t = !n->tryb.empty() ? n->tryb : n->ustawienia.substr(0, n->ustawienia.find('/'));
          if (t.empty() || !obsl(t)) continue;  // w generatorze noce bez trybu mają źródło A
          for (int z = 0; z < 2; z++) {
            if (z == 0 && nocNiepewna(*n)) continue;
            (i >= 23 ? nowe : stare)[z]++;
          }
        }
        bool bez = nowe[0] >= TREND_NOCY_7 && stare[0] >= TREND_NOCY_23;
        bool z = nowe[1] >= TREND_NOCY_7 && stare[1] >= TREND_NOCY_23;
        naiw = !bez && z; przyczyna = 2;
      }
    } else przyczyna = 3;
    niepewneKod += kod; niepewneNaiw += naiw;
    if (kod && !naiw) { falszNiep++; wgPrzyczyny[przyczyna]++; if (falszNiep <= 6) printf("FAŁSZYWE niepewne: ziarno %d, gałąź %d, norma AHI n=%d, stan AHI %d\n", s, przyczyna, st.norma.ahi.n, st.ahi.aktywny); }
    if (!kod && naiw) { falszUcze++; if (falszUcze <= 6) printf("BRAK niepewne: ziarno %d gałąź %d słowo '%s'\n", s, przyczyna, w.slowo.c_str()); }
    if (kod && (w.slowo2 != "duży wyciek" || !w.dopisek.empty() || w.stan || znakOcenyK1(w.slowo) != "?")) zlaForma++;
  }
  printf("R-KOK-37 naiwnie (wejścia %s): przypadków %d, `niepewne` w kodzie %d, naiwnie %d, fałszywe `niepewne` %d (bez normy %d, trend %d, stan %d), fałszywe `uczę się`/inne %d, zła forma %d\n",
         trybWej ? "jak dyrygent, z okresami infekcji" : "spójne", przyp, niepewneKod, niepewneNaiw, falszNiep, wgPrzyczyny[1], wgPrzyczyny[2], wgPrzyczyny[3], falszUcze, zlaForma);
  return (falszNiep || falszUcze || zlaForma) ? 1 : 0;
}
