// SPDX-License-Identifier: GPL-3.0-or-later
#include "stany.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "wspolne.h"

// 4.9: duży wyciek jak E-20 (M-08 ≥ 30 min [PRÓG E-20.minuty] albo ≥ 10% czasu terapii [PRÓG E-20.odsetek_czasu],
// od wersji beta) albo mediana wycieku > 30 L/min [PRÓG S-K-12.mediana_przecieku], a także (wariant C)
// M-07 P95 nocy ≥ 15 L/min [PRÓG R-NOR-05.niepewna_p95]: od tego wycieku aparat gorzej rozpoznaje
// bezdechy. Warunek dochodzi jako „albo”, więc noc niepewna przy 24 L/min zostaje niepewna także bez P95. Brak M-08 albo
// brak P95 = tylko pozostałe warunki (brak danych nie znaczy „szczelna”, ADR-0029 pkt 7).
bool nocNiepewna(const Noc& n) {
  if (!std::isnan(n.wp95) && n.wp95 >= NIEPEWNA_P95 - 1e-4f) return true;
  if (warunekE20(n)) return true;
  return !std::isnan(n.wmed) && n.wmed > 30.0f + 1e-4f;
}

bool warunekE20(const Noc& n) {
  if (std::isnan(n.m08)) return false;
  if (n.m08 >= 30.0f - 1e-4f) return true;
  return n.m01 > 0 && n.m08 >= 0.10f * (float)n.m01 - 1e-4f;
}

float poleM01(const Noc& n) { return n.m01 >= 0 ? (float)n.m01 : NAN; }
float poleAhi(const Noc& n) { return n.ahi; }
float poleEmed(const Noc& n) { return epapMed(n); }  // R-NOR-08: bez EPAP docelowego urządzenia (STR)
float poleWp95(const Noc& n) { return n.wp95; }
static float poleEp95(const Noc& n) { return epapP95(n); }
static float poleCa(const Noc& n) { return n.ca; }
static float poleM17(const Noc& n) { return n.m17 >= 0 ? (float)n.m17 : NAN; }
static float poleCp95(const Noc& n) { return n.cp95; }
// 4.11: ciśnienie trybu nocy. CPAP i AutoSet: M-05p; ASV, ASVAuto i noc bez znanego trybu: EPAP P95 (M-06p).
static bool trybCpap(const Noc& n) {
  std::string t = trybNocy(n);
  return t == "CPAP" || t == "AutoSet";
}
static bool trybAsv(const Noc& n) { return !trybCpap(n); }

namespace {

struct Def {
  float (*wart)(const Noc&);
  NormaMetryki Norma::*norma;
  std::set<std::string> WykluczeniaNormy::*wyl;
  StanMetryki Stany::*stan;
  bool wGore;        // kierunek alarmu (4.5)
  double delta;      // Δ [PRÓG]
  bool zdrowotna;    // 4.7
  bool bezNiepewnych;  // R-NOR-05, R-NOR-19 (AHI)
  bool (*wTrybie)(const Noc&);  // noc liczy się do tej metryki (4.11); nullptr = każda
};

const Def DEFS[] = {
    {poleM01, &Norma::m01, &WykluczeniaNormy::m01, &Stany::m01, false, 90.0, false, false, nullptr},  // −1,5 h
    {poleAhi, &Norma::ahi, &WykluczeniaNormy::ahi, &Stany::ahi, true, 3.0, true, true, nullptr},
    {poleWp95, &Norma::wp95, &WykluczeniaNormy::wp95, &Stany::wp95, true, 10.0, false, false, nullptr},
    {poleEp95, &Norma::ep95, &WykluczeniaNormy::ep95, &Stany::ep95, true, 2.0, true, false, trybAsv},  // ASV (4.11)
    // M-03 CA/h (w górę, Δ +1 / h [PRÓG R-NOR-12.delta.M-03], zdrowotna, bez nocy niepewnych:
    // R-NOR-05, R-NOR-19) i M-17 zdjęcia maski (w górę, Δ +2 [PRÓG R-NOR-12.delta.M-17], techniczna), 4.5 i 4.7
    {poleCa, &Norma::ca, &WykluczeniaNormy::ca, &Stany::ca, true, 1.0, true, true, nullptr},
    {poleM17, &Norma::m17, &WykluczeniaNormy::m17, &Stany::m17, true, 2.0, false, false, nullptr},
    // przegląd M-05p (w górę, Δ +2 cmH2O [PRÓG R-NOR-12.delta.M-05p], zdrowotna), tryby CPAP i AutoSet (4.11)
    {poleCp95, &Norma::cp95, &WykluczeniaNormy::cp95, &Stany::cp95, true, 2.0, true, false, trybCpap},
};
const int N_DEFS = sizeof(DEFS) / sizeof(DEFS[0]);

bool odchylenie(double v, const NormaMetryki& nm, const Def& d) {
  if (!nm.jest() || std::isnan(v)) return false;
  if (d.wGore) return v > nm.p90 && v - nm.med >= d.delta - 1e-6;
  return v < nm.p10 && nm.med - v >= d.delta - 1e-6;
}

struct Bieg {  // stan liczenia jednej metryki
  std::vector<bool> ost5;            // odchylenia ostatnich nocy ważnych z wartością (najnowsze na końcu)
  std::vector<std::string> nocStanu; // noce bieżącego stanu (do zwrotu przy nowym poziomie)
  NormaMetryki zamrozona;            // R-NOR-22
  int spokojne = 0;
};

}  // namespace

Stany liczStany(const std::vector<Noc>& hist, const std::string& dzis, const std::set<std::string>& wykluczone) {
  Stany s;
  if (!dataOk(dzis)) {
    s.norma = liczNorme(hist, dzis, wykluczone);
    return s;
  }
  // historia posortowana raz; norma każdej nocy z okna 90 dni (4.4), więc całość jest liniowa w liczbie nocy
  HistoriaPos hp = posortujHistorie(hist);
  std::vector<const Noc*> noce;
  for (const Noc* r : hp.noce)
    if (r->data <= dzis) noce.push_back(r);
  Bieg bieg[N_DEFS];
  // ostatnie noce ważne do E-30 (AHI, bez niepewnych) i E-36 (odchylenie EPAP P95)
  std::vector<bool> ahi5, ep95odch;
  for (const Noc* n : noce) {
    bool niepewna = nocNiepewna(*n);
    if (niepewna) {  // R-NOR-19
      s.wyl.ahi.insert(n->data);
      s.wyl.m03.insert(n->data);
    }
    if (!n->wazna) continue;
    Norma nm = liczNorme(hp, n->data, wykluczone, &s.wyl);
    bool ostatnia = n == noce.back();
    for (int i = 0; i < N_DEFS; i++) {
      const Def& d = DEFS[i];
      if (d.bezNiepewnych && niepewna) continue;  // R-NOR-05
      if (d.wTrybie && !d.wTrybie(*n)) continue;  // 4.11: metryka ciśnienia trybu nocy
      double v = d.wart(*n);
      if (std::isnan(v)) continue;
      StanMetryki& st = s.*(d.stan);
      Bieg& b = bieg[i];
      const NormaMetryki& norma = st.aktywny && d.zdrowotna ? b.zamrozona : nm.*(d.norma);
      bool odch = odchylenie(v, norma, d);
      if (ostatnia) {
        if (i == 0) s.odchylenie.m01 = odch;
        if (i == 1) s.odchylenie.ahi = odch;
        if (i == 2) s.odchylenie.wp95 = odch;
        if (i == 3) s.odchylenie.ep95 = odch;
        if (i == 4) s.odchylenie.ca = odch;
        if (i == 5) s.odchylenie.m17 = odch;
        if (i == 6) s.odchylenie.cp95 = odch;
      }
      if (i == 3 || i == 6) ep95odch.push_back(odch);  // E-36: ciśnienie trybu nocy (4.11)
      b.ost5.push_back(odch);
      if (b.ost5.size() > 5) b.ost5.erase(b.ost5.begin());
      if ((i == 3 || i == 6) && ep95odch.size() > 7) ep95odch.erase(ep95odch.begin());
      if (!st.aktywny) {
        int k = (int)std::count(b.ost5.begin(), b.ost5.end(), true);
        if (k >= 3 && norma.jest()) {  // R-NOR-03
          st.aktywny = true;
          st.wyzej = d.wGore;
          st.od = n->data;
          st.nocy = 0;
          b.zamrozona = norma;
          b.spokojne = 0;
          b.nocStanu.clear();
        }
      } else if (std::fabs(v - norma.med) <= d.delta / 2 + 1e-6) {
        b.spokojne++;
      } else {
        b.spokojne = 0;
      }
      if (st.aktywny) {
        st.nocy++;
        (s.wyl.*(d.wyl)).insert(n->data);  // R-NOR-09
        b.nocStanu.push_back(n->data);
        if (b.spokojne >= 3) {  // R-NOR-04
          st.aktywny = false;
          b.ost5.clear();
        } else if (!d.zdrowotna && st.nocy >= 21) {  // nowy poziom metryki technicznej (R-NOR-06)
          st.aktywny = false;
          for (auto& x : b.nocStanu) (s.wyl.*(d.wyl)).erase(x);
          b.ost5.clear();
        }
        if (!st.aktywny) {
          st.od.clear();
          st.nocy = 0;
          b.nocStanu.clear();
        }
      }
    }
    if (!niepewna && !std::isnan(n->ahi)) {
      ahi5.push_back(n->ahi >= 5.0f - 1e-4f);
      if (ahi5.size() > 7) ahi5.erase(ahi5.begin());
    }
  }
  auto ostatnie = [](const std::vector<bool>& v, int k) {
    int c = 0;
    for (int i = (int)v.size() - 1, j = 0; i >= 0 && j < k; i--, j++) c += v[i];
    return c;
  };
  s.e30 = ostatnie(ahi5, 7) >= 3;
  s.e36 = ostatnie(ep95odch, 7) >= 5;
  s.norma = liczNorme(hp, dzis, wykluczone, &s.wyl);
  if (!noce.empty()) {
    s.ostatnia = noce.back()->data;
    s.ostatniaNiepewna = nocNiepewna(*noce.back());
  }
  // 5.4.13: pkt 2 i 3 w zakresie liczonym przez panel; całość spełniona tylko, gdy żaden punkt nie jest nieoceniany
  s.nieoceniane = nieocenianeUspokojenia();
  s.ocenaNiepelna = !s.nieoceniane.empty();
  s.uspokojenieOcenione = !s.e30 && !s.e36 && !s.ahi.aktywny && !s.ep95.aktywny && !s.cp95.aktywny;
  s.uspokojenie = !s.ocenaNiepelna && s.uspokojenieOcenione;
  s.niespokoj = s.uspokojenie ? Stany::Niespokoj::BRAK : Stany::Niespokoj::SPRAWA;  // bez silnika: SPRAWA
  return s;
}

// Punkty warunku uspokojenia (5.4.13), których panel nie ocenia, bo nie ma reguły albo danych. Wpis znika, gdy reguła
// powstanie i zacznie być liczona w liczStany (wtedy wpisać tu `true` w kolumnie `oceniany`); dopóki lista nie jest
// pusta, Stany::uspokojenie = false, a panel nie mówi `W normie` ani zdań uspokajających.
struct PunktUspokojenia {
  const char* co;
  bool oceniany;
};
static const PunktUspokojenia PUNKTY_USPOKOJENIA[] = {
    // pkt 1, 4 i 7 ocenia kolejka::zastosujUspokojenie z wyników silnika: schodzą z tej listy tylko wtedy,
    // gdy każda reguła aktywna i „tylko raport” jest oceniona; „nie oceniam” którejkolwiek = punkt zostaje nieoceniany,
    // a uspokojenie niespełnione (ADR-0029 pkt 3 i 7). Bez wyników silnika zostają tu jak dotąd.
    {"pkt 1: wzorce L (L-01…L-11)", false},  // u pacjenta ASV m.in. L-01 (bezdechy centralne)
    {"pkt 2: E-30", true},
    {"pkt 2: E-31 (stan odchylenia AHI)", true},
    {"pkt 2: E-33 (stan odchylenia CA/h, M-03)", true},  // z silnika (kolejka OcenaUspokojenia::pkt2, R5b)
    {"pkt 2: E-36", true},
    {"pkt 2: E-37 (GI, M-11)", false},
    {"pkt 2: S-K-17 (M-13, M-14, M-15)", true},  // z silnika (grupa R5b, OcenaUspokojenia::pkt2)
    {"pkt 3: stan odchylenia AHI i EPAP P95", true},
    {"pkt 3: stan odchylenia M-03, M-11, M-13, M-14, M-15", false},
    {"pkt 4: linie grupy „zdrowie” (7.5)", false},  // kolejka linii z wyników: src/kolejka.cpp
    {"pkt 5: lista `Metryki niewalidowane` (R-DANE-28)", false},  // walidacji metryk (WY9) w panelu jeszcze nie ma
    {"pkt 7: każda reguła zestawu policzona (R-AW-31)", false},     // z wyników silnika
};

std::vector<std::string> nieocenianeUspokojenia() {
  std::vector<std::string> w;
  for (auto& p : PUNKTY_USPOKOJENIA)
    if (!p.oceniany) w.push_back(p.co);
  return w;
}

const Stany& stanyZPamieci(const std::vector<Noc>& hist, const std::string& dzis, const std::set<std::string>& wykluczone) {
  static Stany pamiec;
  static uint64_t klucz = 0;
  static bool jest = false;
  // skrót FNV-1a z pełnej treści (nadpisanie nocy, R-AW-03, też zmienia wynik) zamiast kopii całej historii w RAM
  uint64_t k = 1469598103934665603ULL;
  auto dopisz = [&k](const std::string& t) {
    for (unsigned char c : t) { k ^= c; k *= 1099511628211ULL; }
    k ^= 0xFF; k *= 1099511628211ULL;  // separator
  };
  dopisz(dzis);
  for (auto& r : hist) dopisz(trescNocy(r));  // z polami M-08, M-17 (noc niepewna, stan M-17)
  for (auto& w : wykluczone) dopisz(w);
  if (!jest || k != klucz) {
    pamiec = liczStany(hist, dzis, wykluczone);
    klucz = k;
    jest = true;
  }
  return pamiec;
}

// ---------- porównanie tygodnia ----------
std::vector<double> wartosci7(const std::vector<Noc>& hist, const std::string& dzis, float (*pole)(const Noc&)) {
  std::vector<double> v;
  std::string od = dodajDni(dzis, -6);
  for (auto& r : hist)
    if (r.data >= od && r.data <= dzis && !std::isnan(pole(r))) v.push_back(pole(r));
  return v;
}

Porownanie porownajTydzien(const std::vector<double>& w, const NormaMetryki& nm, Strona s) {
  Porownanie p;
  p.mediana = mediana(w);
  if (!nm.jest()) {
    p.slowo = "uczę się";
    return p;
  }
  if (std::isnan(p.mediana)) {
    p.slowo = "–";
    return p;
  }
  bool wyzej = p.mediana > nm.p90 + 1e-6, nizej = p.mediana < nm.p10 - 1e-6;
  if (!wyzej && !nizej) {
    p.slowo = "↔ jak zwykle";
    p.spokojne = true;
  } else if ((wyzej && s == Strona::WYZEJ_LEPIEJ) || (nizej && s == Strona::NIZEJ_LEPIEJ)) {
    p.slowo = wyzej ? "↑ lepiej niż zwykle" : "↓ lepiej niż zwykle";
    p.spokojne = true;
  } else {
    p.slowo = wyzej ? "↑ wyżej niż zwykle" : "↓ niżej niż zwykle";
  }
  return p;
}
