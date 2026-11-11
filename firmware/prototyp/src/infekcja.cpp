// SPDX-License-Identifier: GPL-3.0-or-later
// Moduł INFEKCJA (spec 5.10): opis w src/infekcja.h.
#include "infekcja.h"
#include <algorithm>
#include <cmath>
#include "wspolne.h"

namespace infekcja {

const char* const LINIA = "Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij pomoc.";

namespace {

std::vector<const Noc*> wazne(const std::vector<Noc>& h, const std::string& dzis) {
  std::vector<const Noc*> v;
  for (auto& n : h)
    if (n.data <= dzis && n.wazna) v.push_back(&n);
  return v;
}

}  // namespace

WynikSK30 ocenSK30(const std::vector<Noc>& h, const Norma& nm, const std::string& dzis, bool asv) {
  WynikSK30 r;
  if (!nm.m13.jest()) { r.dlaczego = "M-13 bez normy (R-NOR-11)"; return r; }
  std::vector<const Noc*> v = wazne(h, dzis);
  for (size_t i = v.size() > 14 ? v.size() - 14 : 0; i < v.size(); i++) r.linia.push_back(v[i]->m13);
  r.norma = (float)nm.m13.med;
  if (v.size() < (size_t)SK30_NOCE || v.back()->data != dzis) { r.dlaczego = "brak ważnej nocy z dziś"; return r; }
  for (size_t i = v.size() - (size_t)SK30_NOCE; i < v.size(); i++)
    if (std::isnan(v[i]->m13) || !(v[i]->m13 >= nm.m13.med + SK30_WZROST)) { r.dlaczego = "M-13 w normie"; return r; }
  const Noc& o = *v.back();
  // sygnał towarzyszący w ostatniej nocy (5.10.2): wentylacja minutowa M-15, ciśnienie (M-05p, w ASV M-06p); sygnał
  // bez normy albo bez wartości pomijany (R-INF-01); puls nie jest zbierany
  const bool went = nm.m15.jest() && nm.m15.med > 0 && !std::isnan(o.m15) &&
                    o.m15 >= nm.m15.med * (1.0 + SK30_WENTYLACJA / 100.0) - 1e-6;  // Δ M-15 = 15% mediany normy (4.5)
  const NormaMetryki& nc = asv ? nm.ep95 : nm.cp95;
  float c = asv ? epapP95(o) : o.cp95;
  const bool cisn = nc.jest() && !std::isnan(c) && c >= nc.med + (asv ? SK30_EPAP : SK30_CISNIENIE) - 1e-6;
  if (!went && !cisn) { r.dlaczego = "bez sygnału towarzyszącego"; return r; }
  r.warunek = true;
  r.sygnal = went ? "wentylacja też wyższa" : asv ? "EPAP też wyższe" : "ciśnienie też wyższe";
  r.pierwszaNoc = v[v.size() - (size_t)SK30_NOCE]->data;
  r.x = o.m13;
  return r;
}

std::vector<Okres> okresy(const std::vector<Noc>& h, const std::vector<std::string>& tak,
                          const std::set<std::string>& wykluczone, const std::string& dzis, bool asv) {
  std::vector<Okres> wy;
  std::set<std::string> wyk = wykluczone;
  std::vector<std::string> t = tak;
  std::sort(t.begin(), t.end());
  for (auto& d : t) {
    if (!dataOk(d) || d > dzis) continue;
    if (nocWOkresie(wy, d)) continue;  // R-INF-09: w okresie nie ma drugiego S-K-30
    if (karencja(wy, d)) continue;     // po okresie z limitem 14 nocy nowe „tak” dopiero po karencji
    Norma nm = liczNorme(h, d, wyk);
    WynikSK30 s = ocenSK30(h, nm, d, asv);
    Okres o;
    o.odpowiedz = d;
    o.od = s.warunek ? s.pierwszaNoc : d;
    o.normaM13 = (float)nm.m13.med;
    std::string maks = dodajDni(o.od, MAKS_NOCE - 1);  // 14 nocy: od … od + 13
    o.koniec = maks;
    int wNormie = 0;
    for (auto* n : wazne(h, dzis)) {
      if (n->data <= d || n->data > maks) continue;
      if (!std::isnan(n->m13) && !std::isnan(o.normaM13) && n->m13 < o.normaM13 + KONIEC_WZROST) wNormie++;
      else wNormie = 0;
      if (wNormie >= KONIEC_NOCE) {
        o.koniec = n->data;
        o.zakonczony = o.koniecPoprawa = true;
        break;
      }
    }
    if (!o.zakonczony && dzis > maks) o.zakonczony = true;
    for (std::string x = o.od; x <= o.koniec && !x.empty(); x = dodajDni(x, 1)) wyk.insert(x);
    wy.push_back(o);
  }
  return wy;
}

bool karencja(const std::vector<Okres>& o, const std::string& dzis) { return !koniecKarencji(o, dzis).empty(); }

std::string koniecKarencji(const std::vector<Okres>& o, const std::string& dzis) {
  std::string k;
  for (auto& x : o)
    if (x.zakonczony && !x.koniecPoprawa && dzis > x.koniec && dniMiedzy(x.koniec, dzis) <= KARENCJA_NOCE) {
      std::string d = dodajDni(x.koniec, KARENCJA_NOCE);
      if (d > k) k = d;
    }
  return k;
}

bool nocWOkresie(const std::vector<Okres>& o, const std::string& data) {
  for (auto& x : o)
    if (data >= x.od && data <= x.koniec) return true;
  return false;
}

std::set<std::string> noceOkresow(const std::vector<Okres>& o, const std::string& dzis) {
  std::set<std::string> s;
  for (auto& x : o)
    for (std::string d = x.od; !d.empty() && d <= x.koniec && d <= dzis; d = dodajDni(d, 1)) s.insert(d);
  return s;
}

const Okres* okresDzis(const std::vector<Okres>& o, const std::string& dzis) {
  for (auto& x : o) {
    if (dzis < x.od || dzis > x.koniec) continue;
    if (x.koniecPoprawa && dzis == x.koniec) continue;  // R-INF-06: rano po nocy końca reguły działają
    return &x;
  }
  return nullptr;
}

bool pytanieGorzej(const Okres& o, const std::vector<Noc>& h, const std::vector<std::string>& pyt,
                   const std::string& dzis) {
  if (dniMiedzy(o.od, dzis) + 1 < GORZEJ_OD_NOCY) return false;
  std::string ost;
  for (auto& p : pyt)
    if (p >= o.od && p < dzis && p > ost) ost = p;
  if (ost.empty()) return true;  // (a)
  if (dniMiedzy(ost, dzis) < GORZEJ_CO_DNI) return false;
  float m = NAN;
  for (auto& n : h)
    if (n.data <= dzis && !std::isnan(n.m13)) m = n.m13;
  return !std::isnan(m) && !std::isnan(o.normaM13) && m >= o.normaM13 + GORZEJ_WZROST;  // (b)
}

bool liniaDzis(const Okres& o, const std::string& dzis, const std::string& ost) {
  if (dzis < o.odpowiedz) return false;
  if (ost.empty() || ost < o.odpowiedz) return true;  // pierwszy dzień okresu (albo pierwszy z pokazaniem)
  return ost != dzis ? dniMiedzy(ost, dzis) >= LINIA_CO_DNI : true;
}

}  // namespace infekcja
