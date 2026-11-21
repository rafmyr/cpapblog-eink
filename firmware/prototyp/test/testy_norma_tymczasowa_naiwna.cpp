// SPDX-License-Identifier: GPL-3.0-or-later
// Niezależna (naiwna) implementacja definicji R-NOR-27 (tylko noce w trybie najnowszej nocy) kontra liczNorme()
// na losowych historiach; do tego norma właściwa bit w bit taka sama z normą tymczasową i bez niej.
// Uruchomienie: tools/prototyp/testy.sh (program testy_norma_tymczasowa_naiwna); argument: liczba historii.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include "norma_tymczasowa_gen.h"

static double medN(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  size_t n = v.size();
  return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}
static double pctN(std::vector<double> v, int p) {  // najbliższa ranga: ceil(n·p/100), co najmniej 1
  std::sort(v.begin(), v.end());
  long n = (long)v.size();
  long r = (n * p + 99) / 100;
  r = std::max(1L, std::min(n, r));
  return v[(size_t)(r - 1)];
}
struct Wzor { bool pokaz; int nocy, nowych; double med, p10, p90; };

static std::string pierwszyCzlon(const std::string& u) { return u.substr(0, u.find('/')); }

// Definicja R-NOR-27 (ADR-0048 pkt 1) od zera: noce w [dzis−90, dzis), ważne, nie w `wyk`; ustawienia
// z przeniesieniem ostatnich znanych (R-NOR-15) po historii posortowanej stabilnie; tryb nocy: pole `tryb`, potem
// pierwszy człon jej ustawień, potem przeniesionych; noc sprzed zmiany: znane ustawienia inne niż najnowszej nocy
// i ten sam tryb co najnowsza noc.
static std::map<std::string, Wzor> naiwnie(const Przypadek& p) {
  std::vector<const Noc*> s;
  for (auto& n : p.h) s.push_back(&n);
  std::stable_sort(s.begin(), s.end(), [](const Noc* a, const Noc* b) { return a->data < b->data; });
  std::vector<std::string> ust(s.size()), tryb(s.size());
  std::string znane;
  for (size_t i = 0; i < s.size(); i++) {
    if (!s[i]->ustawienia.empty()) znane = s[i]->ustawienia;
    ust[i] = znane;
    tryb[i] = !s[i]->tryb.empty() ? s[i]->tryb
              : !s[i]->ustawienia.empty() ? pierwszyCzlon(s[i]->ustawienia) : pierwszyCzlon(ust[i]);
  }
  std::string S, T;
  int ostatnia = -1;
  for (size_t i = 0; i < s.size(); i++) if (s[i]->data <= p.dzis) ostatnia = (int)i;
  if (ostatnia >= 0) { S = ust[(size_t)ostatnia]; T = tryb[(size_t)ostatnia]; }
  std::string gr = dodajDni(p.dzis, -90);
  const WykluczeniaNormy* wm = p.zWm ? &p.wm : nullptr;
  auto bez = [&](const std::set<std::string>* pole, const std::string& d) { return wm && pole && pole->count(d); };
  struct Def { const char* k; float (*f)(const Noc&); const std::set<std::string>* wyl; };
  std::vector<Def> defs = {
      {"cmed", [](const Noc& n) { return n.cmed; }, nullptr},
      {"cp95", [](const Noc& n) { return n.cp95; }, &p.wm.cp95},
      {"wp95", [](const Noc& n) { return n.wp95; }, &p.wm.wp95},
      {"emed", [](const Noc& n) { return n.epapStr ? NAN : n.emed; }, nullptr},
      {"ep95", [](const Noc& n) { return n.epapStr ? NAN : n.ep95; }, &p.wm.ep95},
      {"psmed", [](const Noc& n) { return n.psmed; }, &p.wm.ps},
  };
  std::map<std::string, Wzor> w;
  for (auto& d : defs) {
    std::vector<double> nowe, stare;
    for (int i = (int)s.size() - 1; i >= 0; i--) {
      const Noc& n = *s[(size_t)i];
      if (n.data >= p.dzis || n.data < gr) continue;
      if (!n.wazna || p.wyk.count(n.data)) continue;
      if (bez(d.wyl, n.data)) continue;
      float v = d.f(n);
      if (std::isnan(v)) continue;
      bool tak = S.empty() || ust[(size_t)i] == S;
      bool sprzed = !S.empty() && ust[(size_t)i] != S && !ust[(size_t)i].empty() && tryb[(size_t)i] == T;
      if (tak && nowe.size() < 30) nowe.push_back(v);
      if (sprzed && stare.size() < 30) stare.push_back(v);
    }
    Wzor z{};
    z.nowych = (int)nowe.size();
    z.nocy = (int)stare.size();
    z.pokaz = z.nowych < 14 && z.nocy >= 14;
    z.med = z.p10 = z.p90 = NAN;
    if (z.pokaz) { z.med = medN(stare); z.p10 = pctN(stare, 10); z.p90 = pctN(stare, 90); }
    w[d.k] = z;
  }
  return w;
}

static bool rowne(double a, double b) { return (std::isnan(a) && std::isnan(b)) || a == b; }
static bool rowneM(const NormaMetryki& a, const NormaMetryki& b) {
  return a.n == b.n && rowne(a.med, b.med) && rowne(a.p10, b.p10) && rowne(a.p90, b.p90);
}

int main(int argc, char** argv) {
  int N = argc > 1 ? atoi(argv[1]) : 3000;
  int porownan = 0, rozjazdow = 0, pokazanych = 0, wlasciwaInna = 0, zInnymTrybem = 0;
  for (int s = 1; s <= N; s++) {
    Przypadek p = generuj((unsigned)s);
    const WykluczeniaNormy* wm = p.zWm ? &p.wm : nullptr;
    Norma nm = liczNorme(p.h, p.dzis, p.wyk, wm, ZakresNormy::Z_TYMCZASOWA);
    Norma bez = liczNorme(p.h, p.dzis, p.wyk, wm);
    // norma właściwa ta sama; domyślnie bez normy tymczasowej
    const NormaMetryki* a[] = {&nm.m01, &nm.ahi, &nm.cmed, &nm.cp95, &nm.wp95, &nm.emed, &nm.ep95, &nm.psmed, &nm.psp95,
                               &nm.oa, &nm.ca, &nm.h, &nm.m17, &nm.m13, &nm.m14, &nm.m15, &nm.csrp, &nm.m10p};
    const NormaMetryki* b[] = {&bez.m01, &bez.ahi, &bez.cmed, &bez.cp95, &bez.wp95, &bez.emed, &bez.ep95, &bez.psmed,
                               &bez.psp95, &bez.oa, &bez.ca, &bez.h, &bez.m17, &bez.m13, &bez.m14, &bez.m15, &bez.csrp,
                               &bez.m10p};
    bool ta = nm.n == bez.n && nm.zKarty == bez.zKarty && nm.ustawienia == bez.ustawienia && nm.zegar == bez.zegar;
    for (size_t k = 0; k < sizeof a / sizeof a[0]; k++) ta = ta && rowneM(*a[k], *b[k]);
    const PasmoSprzedZmiany* pb[] = {&bez.sprzedZmiany.cmed, &bez.sprzedZmiany.cp95, &bez.sprzedZmiany.wp95,
                                     &bez.sprzedZmiany.emed, &bez.sprzedZmiany.ep95, &bez.sprzedZmiany.psmed};
    for (auto* x : pb) ta = ta && x->nocy == 0 && !x->pokaz();
    if (!ta) {
      wlasciwaInna++;
      if (wlasciwaInna <= 5) printf("BŁĄD ziarno %d: norma właściwa albo domyślna inna z normą tymczasową\n", s);
    }
    auto w = naiwnie(p);
    const NormaSprzedZmiany& t = nm.sprzedZmiany;
    std::map<std::string, const PasmoSprzedZmiany*> k = {{"cmed", &t.cmed}, {"cp95", &t.cp95}, {"wp95", &t.wp95},
                                                         {"emed", &t.emed}, {"ep95", &t.ep95}, {"psmed", &t.psmed}};
    for (auto& [nazwa, ps] : k) {
      const Wzor& z = w[nazwa];
      porownan++;
      bool ok = ps->pokaz() == z.pokaz && ps->nocyNowych == z.nowych && ps->nocy == z.nocy && rowne(ps->med, z.med) &&
                rowne(ps->p10, z.p10) && rowne(ps->p90, z.p90);
      if (ps->pokaz()) pokazanych++;
      if (!ok) {
        rozjazdow++;
        if (rozjazdow <= 10)
          printf("BŁĄD ziarno %d %s: kod pokaz=%d nocy=%d nowych=%d med=%g | naiwnie pokaz=%d nocy=%d nowych=%d med=%g\n", s,
                 nazwa.c_str(), ps->pokaz(), ps->nocy, ps->nocyNowych, ps->med, z.pokaz, z.nocy, z.nowych, z.med);
      }
    }
    for (auto& n : p.h)
      if (!n.tryb.empty() && n.tryb != p.h.front().tryb) { zInnymTrybem++; break; }
  }
  printf("testy_norma_tymczasowa_naiwna: historii %d (z kilkoma trybami %d), porównań pasm %d, do pokazania %d, "
         "rozjazdów %d, norma właściwa inna %d\n", N, zInnymTrybem, porownan, pokazanych, rozjazdow, wlasciwaInna);
  // test musi coś sprawdzać: pasma do pokazania i historie z kilkoma trybami są
  bool sens = pokazanych > 100 && zInnymTrybem > 50;
  if (!sens) printf("BŁĄD: za mało przypadków z normą tymczasową albo ze zmianą trybu\n");
  return rozjazdow || wlasciwaInna || !sens ? 1 : 0;
}
