// SPDX-License-Identifier: GPL-3.0-or-later
#include "stany.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "wspolne.h"

// 4.9: duży wyciek jak E-20 (M-08 ≥ 30 min [PRÓG E-20.minuty] albo ≥ 10% czasu terapii [PRÓG E-20.odsetek_czasu],
// od wersji beta) albo mediana wycieku > 30 L/min [PRÓG S-K-12.mediana_przecieku], a także (wariant C)
// M-07 P95 nocy ≥ 15 L/min [PRÓG R-NOR-05.niepewna_p95]: ostrożny próg wiarygodności AHI (bez źródła; za duży wyciek
// producent aparatu uznaje 24 L/min). Warunek dochodzi jako „albo”, więc noc niepewna przy 24 L/min zostaje niepewna także bez P95. Brak M-08 albo
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

static float poleM28(const Noc& n) { return n.psmed; }
static float poleM13(const Noc& n) { return n.m13; }
static float poleCsrp(const Noc& n) { return n.m01 > 0 && !std::isnan(n.csr) ? n.csr / (float)n.m01 * 100.0f : NAN; }
static float poleM10p(const Noc& n) { return n.m10p; }
static float poleM14(const Noc& n) { return n.m14; }
static float poleM15(const Noc& n) { return n.m15; }

struct Def {
  float (*wart)(const Noc&);
  NormaMetryki Norma::*norma;
  std::set<std::string> WykluczeniaNormy::*wyl;  // nullptr: noce stanu nie wypadają z normy (dobra zmiana)
  StanMetryki Stany::*stan;
  bool wGore;        // kierunek alarmu (4.5)
  double delta;      // Δ [PRÓG], wartość bezwzględna
  bool zdrowotna;    // 4.7
  bool bezNiepewnych;  // R-NOR-05, R-NOR-19 (AHI)
  bool (*wTrybie)(const Noc&);  // noc liczy się do tej metryki (4.11); nullptr = każda
  // Δ względne (komplet progów 11.11.2026): `delta` to ułamek mediany
  // normy (M-14, M-15: 0,15); mediana ≤ 0 = brak Δ, metryka bez odchylenia (sygnał pomijany, R-INF-01)
  bool deltaWzgledna;
  // R-NOR-07 (R-NOR-22 fail-path): noc z innymi ustawieniami niż poprzednia noc ważna zeruje normę tego sygnału, więc
  // stan (także w kierunku dobrej zmiany) i licznik 3 z 5 zaczynają się od nowa; nullptr = bez zerowania w tym liczeniu
  // (od 11.11.2026 także M-05p, M-06p i M-07, jak w fail-path R-NOR-22)
  bool (*zeruj)(const Noc&);
};

static bool zawsze(const Noc&) { return true; }
// R-NOR-07 od 11.11.2026: M-13 (i M-14, M-15) zerowane tylko w trybach ASV i ASVAuto
static bool trybAsvJawny(const Noc& n) {
  std::string t = trybNocy(n);
  return t == "ASV" || t == "ASVAuto";
}
// Komplet progów Δ przyjęty 11.11.2026 (decyzja właściciela projektu), wszystkie
// [PRÓG]: domysł z przeglądu literatury 11.11.2026 i obserwacja własna, 1 osoba.
const double DELTA_M10P = 0.15;  // R-NOR-12.delta.M-10p (FlowLim P95)
const double DELTA_M28 = 1.0;    // R-NOR-12.delta.M-28 (PS mediana, cmH2O)
const double DELTA_CSR = 5.0;    // R-NOR-12.delta.M-09p (punkty procentowe nocy)
const double DELTA_M06P = 1.5;   // R-NOR-12.delta.M-06p (EPAP P95, cmH2O; było 2)
const double DELTA_WZGL = 0.15;  // R-NOR-12.delta.M-14, R-NOR-12.delta.M-15 (15% mediany normy)

const Def DEFS[] = {
    {poleM01, &Norma::m01, &WykluczeniaNormy::m01, &Stany::m01, false, 90.0, false, false, nullptr, false, nullptr},  // −1,5 h
    {poleAhi, &Norma::ahi, &WykluczeniaNormy::ahi, &Stany::ahi, true, 3.0, true, true, nullptr, false, nullptr},
    {poleWp95, &Norma::wp95, &WykluczeniaNormy::wp95, &Stany::wp95, true, 10.0, false, false, nullptr, false, zawsze},
    // M-06p EPAP P95: Δ +1,5 cmH2O [PRÓG R-NOR-12.delta.M-06p] od 11.11.2026 (komplet progów; wcześniej 2)
    {poleEp95, &Norma::ep95, &WykluczeniaNormy::ep95, &Stany::ep95, true, DELTA_M06P, true, false, trybAsv, false, zawsze},  // ASV
    // M-03 CA/h (w górę, Δ +1 / h [PRÓG R-NOR-12.delta.M-03], zdrowotna, bez nocy niepewnych:
    // R-NOR-05, R-NOR-19) i M-17 zdjęcia maski (w górę, Δ +2 [PRÓG R-NOR-12.delta.M-17], techniczna), 4.5 i 4.7
    {poleCa, &Norma::ca, &WykluczeniaNormy::ca, &Stany::ca, true, 1.0, true, true, nullptr, false, nullptr},
    {poleM17, &Norma::m17, &WykluczeniaNormy::m17, &Stany::m17, true, 2.0, false, false, nullptr, false, nullptr},
    // przegląd M-05p (w górę, Δ +2 cmH2O [PRÓG R-NOR-12.delta.M-05p], zdrowotna), tryby CPAP i AutoSet (4.11)
    {poleCp95, &Norma::cp95, &WykluczeniaNormy::cp95, &Stany::cp95, true, 2.0, true, false, trybCpap, false, zawsze},
    // ADR-0039 (R-KON-64): kierunki z kartą S-K-40…S-K-47, metryki zdrowotne (4.7)
    {poleM28, &Norma::psmed, &WykluczeniaNormy::ps, &Stany::ps, true, DELTA_M28, true, false, trybAsv, false, zawsze},  // S-K-40
    {poleM13, &Norma::m13, &WykluczeniaNormy::m13, &Stany::m13, true, 2.0, true, false, nullptr, false, trybAsvJawny},  // S-K-45
    {poleCsrp, &Norma::csrp, &WykluczeniaNormy::csr, &Stany::csr, true, DELTA_CSR, true, false, nullptr, false, nullptr},  // S-K-46
    {poleM10p, &Norma::m10p, &WykluczeniaNormy::m10, &Stany::m10, true, DELTA_M10P, true, false, nullptr, false, zawsze},  // S-K-47
    // M-15 i M-14: Δ ±15% mediany normy [PRÓG R-NOR-12.delta.M-15, .M-14] (komplet progów 11.11.2026); obie strony
    // dzielą zbiór nocy wyłączonych z normy sygnału (stan po jednej stronie mediany wyklucza drugą)
    {poleM15, &Norma::m15, &WykluczeniaNormy::m15, &Stany::m15g, true, DELTA_WZGL, true, false, nullptr, true, trybAsvJawny},  // S-K-41
    {poleM15, &Norma::m15, &WykluczeniaNormy::m15, &Stany::m15d, false, DELTA_WZGL, true, false, nullptr, true, trybAsvJawny},  // S-K-42
    {poleM14, &Norma::m14, &WykluczeniaNormy::m14, &Stany::m14d, false, DELTA_WZGL, true, false, nullptr, true, trybAsvJawny},  // S-K-43
    {poleM14, &Norma::m14, &WykluczeniaNormy::m14, &Stany::m14g, true, DELTA_WZGL, true, false, nullptr, true, trybAsvJawny},  // S-K-44
    // ADR-0039 pkt 5 (R-KON-68): dobra zmiana; Δ kierunku przeciwnego z 4.5 (PS: jak S-K-40), bez wyłączeń z normy
    {poleAhi, &Norma::ahi, nullptr, &Stany::dobraAhi, false, 3.0, false, true, nullptr, false, nullptr},
    {poleM28, &Norma::psmed, nullptr, &Stany::dobraPs, false, DELTA_M28, false, false, trybAsv, false, zawsze},
    {poleWp95, &Norma::wp95, nullptr, &Stany::dobraWp95, false, 10.0, false, false, nullptr, false, zawsze},
    {poleM17, &Norma::m17, nullptr, &Stany::dobraM17, false, 2.0, false, false, nullptr, false, nullptr},
    {poleM10p, &Norma::m10p, nullptr, &Stany::dobraM10, false, DELTA_M10P, false, false, nullptr, false, zawsze},
    {poleM01, &Norma::m01, nullptr, &Stany::dobraM01, true, 90.0, false, false, nullptr, false, nullptr},  // czas z maską ↑
};
const int N_DEFS = sizeof(DEFS) / sizeof(DEFS[0]);

double deltaDef(const NormaMetryki& nm, const Def& d) {
  if (!d.deltaWzgledna) return d.delta;
  return nm.med > 0 ? d.delta * nm.med : NAN;
}

bool odchylenie(double v, const NormaMetryki& nm, const Def& d) {
  if (!nm.jest() || std::isnan(v)) return false;
  double delta = deltaDef(nm, d);
  if (std::isnan(delta)) return false;
  if (d.wGore) return v > nm.p90 && v - nm.med >= delta - 1e-6;
  return v < nm.p10 && nm.med - v >= delta - 1e-6;
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
  std::vector<float> ahi7;
  std::vector<bool> ep95odch;
  const std::string* ustPoprz = nullptr;  // ustawienia poprzedniej nocy ważnej (z przeniesieniem znanych, R-NOR-15)
  for (size_t in = 0; in < noce.size(); in++) {
    const Noc* n = noce[in];
    bool niepewna = nocNiepewna(*n);
    if (niepewna) {  // R-NOR-19
      s.wyl.ahi.insert(n->data);
      s.wyl.m03.insert(n->data);
    }
    // Od 11.11.2026: spadek czasu z maską (M-01 w dół) liczy 3 z 5 z nocy z terapią (M-01 ≥ 15 min,
    // 5.12.3), także krótszych niż 4 h; noc ważna (4.2) wymaga 4 h, więc sam spadek wypadałby z oceny. Norma M-01 i
    // pozostałe metryki bez zmian (noce ważne).
    bool krotkaZTerapia = !n->wazna && !n->czesciowa && n->m01 >= NOC_Z_TERAPIA_MIN &&
                          (n->tryb.empty() || trybObslugiwany(n->tryb));
    if (!n->wazna && !krotkaZTerapia) continue;
    if (!n->wazna) {
      Norma nmK = liczNorme(hp, n->data, wykluczone, &s.wyl);
      const Def& d = DEFS[0];  // M-01 w dół
      StanMetryki& st = s.*(d.stan);
      Bieg& b = bieg[0];
      const NormaMetryki& norma = nmK.*(d.norma);
      bool odch = odchylenie(d.wart(*n), norma, d);
      if (n == noce.back()) s.odchylenie.m01 = odch;
      b.ost5.push_back(odch);
      if (b.ost5.size() > 5) b.ost5.erase(b.ost5.begin());
      if (!st.aktywny) {
        if ((int)std::count(b.ost5.begin(), b.ost5.end(), true) >= 3 && norma.jest()) {  // R-NOR-03
          st.aktywny = true;
          st.wyzej = d.wGore;
          st.od = n->data;
          st.odPierwszej = (int)(b.ost5.end() - std::find(b.ost5.begin(), b.ost5.end(), true)) - 1;
          st.nocy = 0;
          b.zamrozona = norma;
          b.spokojne = 0;
          b.nocStanu.clear();
        }
      } else {
        b.spokojne = 0;  // noc krótsza niż 4 h nie jest „w połowie Δ” od mediany czasu z maską
      }
      if (st.aktywny) {
        st.nocy++;
        st.odPierwszej++;
        b.nocStanu.push_back(n->data);
      }
      continue;
    }
    // R-NOR-07: zmiana ustawień między nocami ważnymi (noce `noce` to początek `hp.noce`, więc indeks się zgadza)
    const std::string* ust = hp.ust[in];
    bool zmianaUst = ustPoprz && !ustPoprz->empty() && !ust->empty() && *ustPoprz != *ust;
    ustPoprz = ust;
    Norma nm = liczNorme(hp, n->data, wykluczone, &s.wyl);
    bool ostatnia = n == noce.back();
    for (int i = 0; i < N_DEFS; i++) {
      const Def& d = DEFS[i];
      StanMetryki& st = s.*(d.stan);
      Bieg& b = bieg[i];
      // R-NOR-07, R-NOR-22 (fail-path): norma od nowa, stan i licznik 3 z 5 też; przed pominięciem nocy bez wartości,
      // nocy niepewnej i nocy spoza trybu, bo inaczej stan z normą zamrożoną trwałby po zmianie bez końca
      if (zmianaUst && d.zeruj && d.zeruj(*n)) {
        st = StanMetryki();
        b = Bieg();
      }
      if (d.bezNiepewnych && niepewna) continue;  // R-NOR-05
      if (d.wTrybie && !d.wTrybie(*n)) continue;  // 4.11: metryka ciśnienia trybu nocy
      double v = d.wart(*n);
      if (std::isnan(v)) continue;
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
          // R-KON-68: noce od pierwszej z odchyleniem w oknie wejścia (bez bieżącej, liczonej niżej)
          st.odPierwszej = (int)(b.ost5.end() - std::find(b.ost5.begin(), b.ost5.end(), true)) - 1;
          b.zamrozona = norma;
          if (d.zdrowotna) st.zamrozona = norma;  // R-NOR-22: do kart S-K-40…S-K-47 (R-KON-64)
          b.spokojne = 0;
          b.nocStanu.clear();
        }
      } else if (std::fabs(v - norma.med) <= deltaDef(norma, d) / 2 + 1e-6) {
        b.spokojne++;
      } else {
        b.spokojne = 0;
      }
      if (st.aktywny) {
        st.nocy++;
        st.odPierwszej++;
        if (d.wyl) (s.wyl.*(d.wyl)).insert(n->data);  // R-NOR-09
        b.nocStanu.push_back(n->data);
        if (b.spokojne >= 3) {  // R-NOR-04
          st.aktywny = false;
          b.ost5.clear();
        } else if (!d.zdrowotna && st.nocy >= 21) {  // nowy poziom metryki technicznej (R-NOR-06)
          st.aktywny = false;
          if (d.wyl)
            for (auto& x : b.nocStanu) (s.wyl.*(d.wyl)).erase(x);
          b.ost5.clear();
        }
        if (!st.aktywny) {
          st.od.clear();
          st.nocy = 0;
          st.odPierwszej = 0;
          st.zamrozona = NormaMetryki();
          b.nocStanu.clear();
        }
      }
    }
    if (!niepewna && !std::isnan(n->ahi)) {  // E-30: AHI nocy ważnych bez niepewnych, ostatnie E30_OKNO
      ahi7.push_back(n->ahi);
      if (ahi7.size() > (size_t)E30_OKNO) ahi7.erase(ahi7.begin());
    }
  }
  auto ostatnie = [](const std::vector<bool>& v, int k) {
    int c = 0;
    for (int i = (int)v.size() - 1, j = 0; i >= 0 && j < k; i--, j++) c += v[i];
    return c;
  };
  // E-30 jak w katalogu reguł (E.csv: stat=mediana, n=E-30.okno, prog=E-30.ahi): mediana AHI z 7 ostatnich nocy
  // ważnych bez niepewnych ≥ 5 / h; mniej niż 7 takich nocy = warunek niespełniony (silnik: „nie oceniam”, pkt 7)
  if (ahi7.size() == (size_t)E30_OKNO) {
    std::vector<float> v = ahi7;
    std::sort(v.begin(), v.end());
    s.e30 = v[E30_OKNO / 2] >= E30_AHI - 1e-4f;
  }
  s.e36 = ostatnie(ep95odch, 7) >= 5;
  // R-SKI-24 i brak ocen z powodu wycieku: noce z terapią od najnowszej
  {
    int zTerapia = 0, nieczytelne = 0, wazne = 0, pewne = 0;
    for (auto it = noce.rbegin(); it != noce.rend() && zTerapia < WYCIEK_OKNO; ++it) {
      const Noc& n = **it;
      if (n.m01 < NOC_Z_TERAPIA_MIN) continue;
      bool niep = nocNiepewna(n);
      if (zTerapia < SKI24_OKNO && (!n.wazna || niep)) nieczytelne++;
      if (n.wazna) wazne++;
      if (n.wazna && !niep) pewne++;
      zTerapia++;
    }
    s.nocyNieczytelne = nieczytelne >= SKI24_NOCE;
    s.bezOcenZWycieku = wazne >= WYCIEK_MIN_NOCE && pewne < WYCIEK_MIN_NOCE;
  }
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
    {"pkt 3: stan odchylenia M-03, M-11, M-13, M-14, M-15, M-28, M-09 (% nocy), M-10 P95", false},
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
