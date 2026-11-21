// SPDX-License-Identifier: GPL-3.0-or-later
// Scenariusze normy tymczasowej w Stany::normaKokpitu (ADR-0048): tryb najnowszej nocy, wymiana aparatu, noc bez
// trybu; R-KOK-37 przy wywołaniu jak w dyrygencie
// (stany z okresami infekcji R-INF-05 i z historią R-MAS-16, K1 z samymi czynnikami i surową historią).
#include <cmath>
#include <cstdio>
#include "czynniki.h"
#include "kokpit.h"
#include "noc.h"
#include "stany.h"
#include "wspolne.h"
static int ok = 0, zle = 0;
#define S(w) do { if (w) ok++; else { zle++; printf("BŁĄD %d: %s\n", __LINE__, #w); } } while (0)
static Noc noc(int i, const std::string& u, float w, float c, const std::string& tryb) {
  Noc n; n.data = dodajDni("2026-08-01", i); n.wazna = true; n.m01 = 420; n.ahi = 1.5f; n.wp95 = w + (float)(i % 3);
  n.wmed = 4; n.cmed = c; n.cp95 = c + 1; n.oa = .3f; n.ca = .2f; n.h = .4f; n.m17 = 0; n.tryb = tryb; n.ustawienia = u;
  if (tryb.rfind("ASV", 0) == 0) { n.emed = c - 2; n.ep95 = c - 1; n.psmed = 4; n.psp95 = 7; }
  return n;
}
static int wiersz(const DaneK1& d, const char* n) { for (int i = 0; i < d.wierszy; i++) if (std::string(d.w[i].nazwa) == n) return i; return -1; }
static int wierszK2(const DaneK2& d, const char* n) { for (int i = 0; i < d.wierszy; i++) if (std::string(d.w[i].etykieta) == n) return i; return -1; }
int main() {
  {  // CPAP → AutoSet
    std::vector<Noc> h; for (int i = 0; i < 25; i++) h.push_back(noc(i, "CPAP/10", 10, 10, "CPAP"));
    for (int i = 25; i < 30; i++) h.push_back(noc(i, "AutoSet/6/12/2", 12, 8, "AutoSet"));
    std::string dz = h.back().data; Stany st = liczStany(h, dz, {});
    DaneK1 k1 = budujK1(h, dz, st.norma, {}, &st); DaneK2 k2 = budujK2(h, dz, st.norma, {}, &st);
    int c = wiersz(k1, "Ciśnienie"), c2 = wierszK2(k2, "Ciśnienie P95");
    printf("CPAP→AutoSet: cmed pokaz=%d wp95 pokaz=%d | K1 Ciśnienie '%s' sprzed=%d | K2 Ciśnienie P95 '%s' sprzed=%d\n", st.normaKokpitu.sprzedZmiany.cmed.pokaz(), st.normaKokpitu.sprzedZmiany.wp95.pokaz(),
           c >= 0 ? k1.w[c].slowo.c_str() : "?", c >= 0 && k1.w[c].sprzedZmiany, c2 >= 0 ? k2.w[c2].norma.c_str() : "?", c2 >= 0 && k2.w[c2].sprzedZmiany);
    S(!st.normaKokpitu.sprzedZmiany.cmed.pokaz() && !st.normaKokpitu.sprzedZmiany.wp95.pokaz() && !st.normaKokpitu.sprzedZmiany.cp95.pokaz());
  }
  {  // AutoSet → ASVAuto, nowy aparat
    std::vector<Noc> h; for (int i = 0; i < 25; i++) { Noc n = noc(i, "AutoSet/6/12/2", 10, 10, "AutoSet"); n.srn = "A1"; h.push_back(n); }
    for (int i = 25; i < 30; i++) { Noc n = noc(i, "ASVAuto/6/10/3/15", 30, 10, "ASVAuto"); n.srn = "B2"; h.push_back(n); }
    Stany st = liczStany(h, h.back().data, {});
    printf("AutoSet→ASVAuto (nowy aparat): wp95 pokaz=%d\n", st.normaKokpitu.sprzedZmiany.wp95.pokaz()); S(!st.normaKokpitu.sprzedZmiany.wp95.pokaz());
  }
  {  // nowy aparat, ten sam tryb, inne ustawienia (R-NOR-25 niezaimplementowane): norma tymczasowa ze starego aparatu
    std::vector<Noc> h; for (int i = 0; i < 25; i++) { Noc n = noc(i, "AutoSet/6/12/2", 10, 10, "AutoSet"); n.srn = "A1"; h.push_back(n); }
    for (int i = 25; i < 30; i++) { Noc n = noc(i, "AutoSet/6/13/2", 30, 10, "AutoSet"); n.srn = "B2"; h.push_back(n); }
    Stany st = liczStany(h, h.back().data, {});
    printf("nowy aparat ten sam tryb, inne ustawienia: wp95 pokaz=%d nocy=%d (stary aparat)\n", st.normaKokpitu.sprzedZmiany.wp95.pokaz(), st.normaKokpitu.sprzedZmiany.wp95.nocy);
  }
  {  // noc bez trybu i bez ustawień (najnowsza): tryb z przeniesionych ustawień
    std::vector<Noc> h; for (int i = 0; i < 25; i++) h.push_back(noc(i, "AutoSet/6/12/2", 10, 10, "AutoSet"));
    for (int i = 25; i < 29; i++) h.push_back(noc(i, "AutoSet/6/13/2", 20, 10, "AutoSet"));
    Noc x = noc(29, "", 20, 10, ""); x.zrodlo = 'K'; h.push_back(x);
    Stany st = liczStany(h, h.back().data, {});
    printf("najnowsza noc bez trybu (karta): wp95 pokaz=%d nocy=%d nowych=%d\n", st.normaKokpitu.sprzedZmiany.wp95.pokaz(), st.normaKokpitu.sprzedZmiany.wp95.nocy, st.normaKokpitu.sprzedZmiany.wp95.nocyNowych);
    S(st.normaKokpitu.sprzedZmiany.wp95.pokaz());
    // stare noce bez pola tryb, tylko z ustawieniami (tryb z pierwszego członu)
    for (int i = 0; i < 25; i++) h[i].tryb = "";
    Stany st2 = liczStany(h, h.back().data, {});
    S(st2.normaKokpitu.sprzedZmiany.wp95.pokaz());
    // stare noce bez trybu i bez ustawień, ale ustawienia przeniesione z wcześniejszej nocy CPAP → inny tryb
    std::vector<Noc> g; g.push_back(noc(0, "CPAP/10", 10, 10, "CPAP"));
    for (int i = 1; i < 25; i++) g.push_back(noc(i, "", 10, 10, ""));
    for (int i = 25; i < 30; i++) g.push_back(noc(i, "AutoSet/6/13/2", 20, 10, "AutoSet"));
    Stany st3 = liczStany(g, g.back().data, {});
    printf("stare noce bez trybu z przeniesionym CPAP: wp95 pokaz=%d\n", st3.normaKokpitu.sprzedZmiany.wp95.pokaz()); S(!st3.normaKokpitu.sprzedZmiany.wp95.pokaz());
  }
  {  // R-KOK-37 z okresem infekcji (dyrygent: stany z wykluczoneB16, K1 z czynnikami bez okresów)
    std::vector<Noc> h; std::set<std::string> infekcja;
    for (int i = 0; i < 10; i++) { Noc n = noc(i, "AutoSet/6/12/2", 22, 10, "AutoSet"); h.push_back(n); infekcja.insert(n.data); }  // niepewne, w okresie infekcji
    for (int i = 10; i < 20; i++) h.push_back(noc(i, "AutoSet/6/12/2", 8, 10, "AutoSet"));
    std::string dz = dodajDni(h.back().data, 1);
    Stany st = liczStany(h, dz, infekcja);  // jak stany(dzis): wykluczoneB16 = czynniki + noce okresów infekcji
    DaneK1 k1 = budujK1(h, dz, st.norma, {}, &st);  // jak dyrygent: czynniki (tu brak), bez okresów infekcji
    int a = wiersz(k1, "AHI");
    WykluczeniaNormy wm2 = st.wyl; for (auto& n : h) if (nocNiepewna(n)) wm2.ahi.erase(n.data);
    Norma n2 = liczNorme(h, dz, infekcja, &wm2);
    printf("R-KOK-37 infekcja: norma AHI n=%d, z nocami niepewnymi n=%d (jest=%d) → K1 AHI '%s' / '%s'\n", st.norma.ahi.n, n2.ahi.n, n2.ahi.jest(), k1.w[a].slowo.c_str(), k1.w[a].slowo2.c_str());
    S(k1.w[a].slowo == "uczę się");
  }
  {  // R-KOK-37 po nowym modelu maski (R-MAS-16: stany z historią bez wycieku sprzed modelu, K1 z surową)
    std::vector<Noc> h;
    for (int i = 0; i < 10; i++) h.push_back(noc(i, "AutoSet/6/12/2", 22, 10, "AutoSet"));  // stara maska, duży wyciek
    for (int i = 10; i < 13; i++) h.push_back(noc(i, "AutoSet/6/12/2", 8, 10, "AutoSet"));  // nowy model maski
    std::string m = h[10].data, dz = dodajDni(h.back().data, 1);
    std::vector<Noc> hb = h; for (auto& n : hb) if (n.data < m) n.wmed = n.wp95 = NAN;  // jak histNormyB16()
    Stany st = liczStany(hb, dz, {});
    DaneK1 k1 = budujK1(h, dz, st.norma, {}, &st);  // dyrygent.cpp:5616: budujK1(hist, …, norma(dzis), …, &stany(dzis))
    int a = wiersz(k1, "AHI");
    printf("R-KOK-37 nowy model maski: norma AHI n=%d (noce sprzed modelu w normie, bo bez wycieku nie są niepewne) → K1 AHI '%s' / '%s'\n", st.norma.ahi.n, k1.w[a].slowo.c_str(), k1.w[a].slowo2.c_str());
    S(k1.w[a].slowo == "uczę się");
  }
  printf("scenariusze normy tymczasowej i R-KOK-37: %d ok, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
