// SPDX-License-Identifier: GPL-3.0-or-later
// Odkrycia O-03…O-12; opis w src/odkrycia.h.
#include "odkrycia.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

#include "reguly_wewn.h"  // tylko stałe testu permutacyjnego (PRZETASOWANIA, POZIOM_ISTOTNOSCI), jedno źródło
#include "wspolne.h"

namespace odkrycia {

using reguly::Kontekst;
using reguly::Wynik;
using reguly::WierszKatalogu;

// ---------- cechy nocy ----------
static const CechyNocy* gCechy = nullptr;
Zakres::Zakres(const CechyNocy* c) : poprz_(gCechy) { gCechy = c; }
Zakres::~Zakres() { gCechy = poprz_; }
const CechyNocy* cechyOceny() { return gCechy; }

double cecha(const std::string& nazwa, const std::string& data) {
  if (nazwa == "weekend") {
    int d = dzienTygodnia(data);
    return d < 0 ? NAN : (d == 0 || d == 6 ? 1.0 : 0.0);
  }
  if (!gCechy) return NAN;
  auto c = gCechy->wartosci.find(nazwa);
  if (c == gCechy->wartosci.end()) return NAN;
  auto v = c->second.find(data);
  return v == c->second.end() ? NAN : v->second;
}

bool wierszOdkrycia(const std::string& id) { return id.size() > 2 && id.compare(0, 2, "O-") == 0; }

std::string klucz(const Wynik& w) { return w.wariant.empty() ? w.id : w.id + "/" + w.wariant; }

namespace {

// ---------- pomocnicze ----------
// Wartość progu z parametru (liczba); NAN przy braku. Próg w godzinach dla M-01 (minuty) przeliczony (Δ M-01 = 1,5 h).
double progLiczba(const WierszKatalogu& w, const Kontekst& k, const char* nazwa, std::string& powod) {
  const reguly::Prog* p = reguly::progParametru(w, k, nazwa, powod);
  if (!p) return NAN;
  if (!p->tekst.empty()) {
    // ułamek `1/7` (R-CZY-02.grupa_kontrolna)
    size_t s = p->tekst.find('/');
    if (s != std::string::npos) {
      double a = liczbaRzecz(p->tekst.substr(0, s).c_str()), b = liczbaRzecz(p->tekst.substr(s + 1).c_str());
      if (b > 0) return a / b;
    }
    powod = "prog " + p->id + " nie jest liczba";
    return NAN;
  }
  return p->jednostka == "h" ? p->wartosc * 60.0 : p->wartosc;
}

std::string param(const WierszKatalogu& w, const char* nazwa) {
  const std::string* p = reguly::parametr(w, nazwa);
  return p ? *p : "";
}

// Noce ważne okna (tryb bieżący) z wartością metryki, najstarsza pierwsza; n = 0: wszystkie.
struct Probka {
  std::string data;
  double v;
};
std::vector<Probka> probki(const Kontekst& k, const std::string& m, int n) {
  std::vector<Probka> r;
  for (const Noc* x : reguly::noceOkna(k, "wazne", 0)) {
    double v = reguly::metryka(*x, m);
    if (!std::isnan(v)) r.push_back({x->data, v});
  }
  if (n > 0 && (int)r.size() > n) r.erase(r.begin(), r.end() - n);
  return r;
}

double med(std::vector<double> v) { return mediana(std::move(v)); }

// Mediana ważona (R-CZY-24): najmniejsza wartość, przy której suma wag wartości nie większych osiąga połowę sumy wag.
struct Wazona {
  double v, w;
  char war = 0;  // warstwa R-CZY-24 (O, K) do przetasowania w obrębie warstwy (R-NF-24); 0 = jedna warstwa
};
double medianaWazona(std::vector<Wazona> v) {
  if (v.empty()) return NAN;
  std::sort(v.begin(), v.end(), [](const Wazona& a, const Wazona& b) { return a.v < b.v; });
  double s = 0;
  for (auto& x : v) s += x.w;
  double c = 0;
  for (auto& x : v) {
    c += x.w;
    if (c >= s / 2 - 1e-9) return x.v;
  }
  return v.back().v;
}
double efektywna(const std::vector<Wazona>& v) {
  double s = 0, q = 0;
  for (auto& x : v) {
    s += x.w;
    q += x.w * x.w;
  }
  return q > 0 ? s * s / q : 0;
}

// Generator testu: własny Fisher–Yates na mt19937 (std::shuffle i rozkłady zależą od biblioteki; wynik ma być ten sam
// na panelu i na komputerze).
uint32_t ziarnoKlucza(const std::string& dzis, const std::string& kl) {
  uint32_t h = 2166136261u;
  for (char c : kl) { h ^= (uint8_t)c; h *= 16777619u; }
  return (uint32_t)nrDnia(dzis) ^ h;
}
// Test permutacyjny różnicy (ważonych) median dwóch grup: p = (liczba przetasowań z |różnicą| ≥ obserwowanej + 1) /
// (PRZETASOWANIA + 1); kier = 0 dwustronnie, +1/−1 jednostronnie (grupa A wyżej / niżej). R-NF-24: etykiety
// grup tasowane osobno w każdej warstwie (`Wazona::war`), więc skład warstw grupy zostaje jak w danych. noce
// posortowane raz, przetasowanie zmienia tylko etykiety, mediany ważone jednym przejściem bez alokacji.
double testPermutacyjny(const std::vector<Wazona>& a, const std::vector<Wazona>& b, int kier, uint32_t ziarno) {
  std::vector<Wazona> all = a;
  all.insert(all.end(), b.begin(), b.end());
  std::vector<char> lab(all.size(), 0);
  for (size_t i = 0; i < a.size(); i++) lab[i] = 1;
  std::vector<size_t> ix(all.size());
  for (size_t i = 0; i < ix.size(); i++) ix[i] = i;
  std::stable_sort(ix.begin(), ix.end(), [&](size_t x, size_t y) { return all[x].v < all[y].v; });
  std::vector<Wazona> s(all.size());
  std::vector<char> l(all.size());
  for (size_t i = 0; i < ix.size(); i++) s[i] = all[ix[i]], l[i] = lab[ix[i]];
  double sa = 0, sb = 0;
  for (size_t i = 0; i < s.size(); i++) (l[i] ? sa : sb) += s[i].w;
  auto stat = [&]() {  // mediany ważone obu grup z posortowanych nocy (jak medianaWazona())
    double ca = 0, cb = 0, ma = NAN, mb = NAN;
    for (size_t i = 0; i < s.size() && (std::isnan(ma) || std::isnan(mb)); i++) {
      if (l[i]) {
        ca += s[i].w;
        if (std::isnan(ma) && ca >= sa / 2 - 1e-9) ma = s[i].v;
      } else {
        cb += s[i].w;
        if (std::isnan(mb) && cb >= sb / 2 - 1e-9) mb = s[i].v;
      }
    }
    double d = ma - mb;
    return kier == 0 ? std::fabs(d) : kier * d;
  };
  double obs = stat();
  // warstwy: pozycje (w kolejności posortowanej) każdej warstwy
  std::vector<std::vector<size_t>> warstwy;
  std::vector<char> kody;
  for (size_t i = 0; i < s.size(); i++) {
    size_t k = 0;
    while (k < kody.size() && kody[k] != s[i].war) k++;
    if (k == kody.size()) {
      kody.push_back(s[i].war);
      warstwy.emplace_back();
    }
    warstwy[k].push_back(i);
  }
  std::mt19937 los(ziarno);
  int wiecej = 0;
  for (int t = 0; t < reguly::wewn::PRZETASOWANIA; t++) {
    for (auto& wp : warstwy)  // Fisher–Yates etykiet w warstwie; sumy wag grup zmieniają się razem z etykietami
      for (size_t i = wp.size(); i > 1; i--) std::swap(l[wp[i - 1]], l[wp[los() % i]]);
    sa = sb = 0;
    for (size_t i = 0; i < s.size(); i++) (l[i] ? sa : sb) += s[i].w;
    if (stat() >= obs - 1e-12) wiecej++;
  }
  return (wiecej + 1.0) / (reguly::wewn::PRZETASOWANIA + 1.0);
}

// Nazwa metryki w tekstach odkryć (4.3). Od AHI ma jedną nazwę w obu profilach (5.4.14), więc bez wariantu publicznego.
std::string nazwaMetryki(const std::string& m, bool wielka) {
  if (m == "M-02") return "AHI";
  if (m == "M-07") return wielka ? "Wyciek" : "wyciek";
  if (m == "M-01") return wielka ? "Czas z maską" : "czas z maską";
  if (m == "M-03ca") return wielka ? "Centralne" : "centralne";
  if (m == "M-17") return wielka ? "Przerwy" : "przerwy";
  return m;
}
// Wartość z jednostką jak na ekranach dnia (4.3, bez P95, R-EKR-53).
std::string wartoscZJednostka(const std::string& m, double v) {
  if (std::isnan(v)) return "–";
  if (m == "M-02" || m == "M-03ca") return przecinek1(v) + " / h";
  if (m == "M-07") return calkowita(v) + " l/min";
  if (m == "M-01") return godzH((int)std::lround(v));
  return calkowita(v);
}
std::string bezJednostki(const std::string& m, double v) {
  if (m == "M-02" || m == "M-03ca") return przecinek1(v);
  if (m == "M-01") return godzH((int)std::lround(v));
  return calkowita(v);
}
// `wyższe o 1,2 / h`, `krótszy o 0 h 40` (O-08, O-09: `{różnica}`)
std::string roznica(const std::string& m, double d) {
  bool w = d > 0;
  std::string o = " o " + wartoscZJednostka(m, std::fabs(d));
  if (m == "M-02" || m == "M-03ca") return (w ? "wyższe" : "niższe") + o;
  if (m == "M-07") return (w ? "wyższy" : "niższy") + o;
  if (m == "M-01") return (w ? "dłuższy" : "krótszy") + o;
  if (m == "M-17") return (w ? "częstsze" : "rzadsze") + o;
  return o;
}
// `{zmiana}` tytułów O-06 i O-12 (6.5, reguła tytułu O-12)
std::string zmiana(const std::string& m, double d) {
  bool w = d > 0;
  if (m == "M-02") return w ? "wyższe AHI" : "niższe AHI";
  if (m == "M-07") return w ? "wyższy wyciek" : "niższy wyciek";
  if (m == "M-01") return w ? "dłuższa maska" : "krótsza maska";
  if (m == "M-03ca") return w ? "więcej centralnych" : "mniej centralnych";
  if (m == "M-17") return w ? "więcej przerw" : "mniej przerw";
  return "";
}

std::string liczbaSurowa(double v) {
  char b[32];
  snprintf(b, sizeof b, "%.6g", v);
  return b;
}

void wspolne(Wynik& r, const std::string& m, double wart, double delta) {
  r.zmienne["metryka_id"] = m;
  r.zmienne["Metryka"] = nazwaMetryki(m, true);
  r.zmienne["metryka"] = nazwaMetryki(m, false);
  r.zmienne["wartosc_num"] = liczbaSurowa(wart);
  r.zmienne["delta_num"] = liczbaSurowa(std::fabs(delta));
}

// ---------- rodzaje ----------
//  rekord_tygodnia   metryka (M-02, M-07), n (noce okna, O-03.noce), min (dni historii, O-03.historia), delta (Δ 4.5,
//                    do R-EKR-27: powrót po 30 dniach albo zmianie o Δ). Mediana n ostatnich nocy ważnych niższa
//                    (ściśle) od każdej kroczącej mediany n nocy sprzed tego okna. Historia: ≥ min dni i ≥ min nocy
// ważnych (obie skale okna „30”, linia 30 nocy pełna). Zmienne: x, miesiąc (dopełniacz
//                    miesiąca pierwszej nocy, z rokiem, gdy inny niż dziś), n.
Wynik rRekordTygodnia(const WierszKatalogu& w, const Kontekst& k) {
  std::string m = param(w, "metryka"), powod;
  if (m.empty()) return reguly::nieoceniona(w, "brak parametru metryka");
  double n = progLiczba(w, k, "n", powod), dni = progLiczba(w, k, "min", powod);
  if (std::isnan(n) || std::isnan(dni)) return reguly::nieoceniona(w, powod);
  std::vector<Probka> v = probki(k, m, 0);
  int nn = (int)std::lround(n);
  if (v.empty() || dniMiedzy(v.front().data, k.dzis) + 1 < (int)std::lround(dni))
    return reguly::nieoceniona(w, "historia krotsza niz " + std::to_string((int)dni) + " dni");
  if ((int)v.size() < std::max(nn + 1, (int)std::lround(dni)))  // także ≥ min nocy ważnych
    return reguly::nieoceniona(w, "za malo nocy " + std::to_string(v.size()));
  if (v.back().data != k.dzis) return reguly::oceniona(w, false);  // bez nowej nocy ważnej: bez odkrycia
  auto medOkna = [&](size_t kon) {  // okno kończące się przed indeksem kon
    std::vector<double> x;
    for (size_t i = kon - (size_t)nn; i < kon; i++) x.push_back(v[i].v);
    return med(x);
  };
  double teraz = medOkna(v.size());
  // okna kończące się przed początkiem bieżącego (okna nakładające się na nie dzielą jego noce i remisowałyby z nim)
  bool rekord = true;
  for (size_t kon = (size_t)nn; kon + (size_t)nn <= v.size(); kon++)
    if (!(teraz < medOkna(kon) - 1e-9)) rekord = false;
  Wynik r = reguly::oceniona(w, rekord);
  double d = NAN;
  if (reguly::parametr(w, "delta")) d = progLiczba(w, k, "delta", powod);  // Δ do R-EKR-27
  wspolne(r, m, teraz, d);
  r.zmienne["x"] = bezJednostki(m, teraz);
  int mies = liczbaCalk(v.front().data.substr(5, 2).c_str());
  std::string mc = mies >= 1 && mies <= 12 ? MIESIACE_D[mies - 1] : "";
  if (v.front().data.substr(0, 4) != k.dzis.substr(0, 4)) mc += " " + v.front().data.substr(0, 4);
  r.zmienne["miesiąc"] = mc;
  r.zmienne["n"] = std::to_string(nn);
  return r;
}

//  tydzien_miesiac   metryka, okno_a (7), okno_b (30), delta (Δ 4.5), por (le: niżej lepiej, ge: wyżej lepiej). Mediana
//                    okna a lepsza o co najmniej |Δ| od mediany okna b (noce ważne). Zmienne: Metryka, x, y, iloraz.
Wynik rTydzienMiesiac(const WierszKatalogu& w, const Kontekst& k) {
  std::string m = param(w, "metryka"), powod, por = param(w, "por");
  if (m.empty()) return reguly::nieoceniona(w, "brak parametru metryka");
  double a = progLiczba(w, k, "okno_a", powod), b = progLiczba(w, k, "okno_b", powod), d = progLiczba(w, k, "delta", powod);
  if (std::isnan(a) || std::isnan(b) || std::isnan(d)) return reguly::nieoceniona(w, powod);
  std::vector<Probka> v = probki(k, m, (int)std::lround(b));
  if ((int)v.size() < (int)std::lround(b)) return reguly::nieoceniona(w, "za malo nocy " + std::to_string(v.size()));
  std::vector<double> va, vb;
  for (size_t i = 0; i < v.size(); i++) {
    vb.push_back(v[i].v);
    if (i + (size_t)std::lround(a) >= v.size()) va.push_back(v[i].v);
  }
  double ma = med(va), mb = med(vb);
  double lepiej = por == "ge" ? ma - mb : mb - ma;
  Wynik r = reguly::oceniona(w, lepiej >= std::fabs(d) - 1e-9);
  wspolne(r, m, ma, d);
  r.zmienne["x"] = wartoscZJednostka(m, ma);
  r.zmienne["y"] = wartoscZJednostka(m, mb);
  r.zmienne["a_num"] = liczbaSurowa(ma);
  r.zmienne["b_num"] = liczbaSurowa(mb);
  r.zmienne["iloraz"] = liczbaSurowa(lepiej / std::fabs(d));
  return r;
}

// Klasyfikacja nocy cechą: 1 = grupa cechy, 0 = grupa porównawcza, -1 = nieznana.
int grupaCechy(const WierszKatalogu& w, const Kontekst& k, const std::string& c, const std::string& data, bool& blad,
               std::string& powod) {
  double v = cecha(c, data);
  if (std::isnan(v)) return -1;
  const std::string* g = reguly::parametr(w, "granica");
  if (!g) return v != 0 ? 1 : 0;
  const reguly::Prog* p = reguly::progParametru(w, k, "granica", powod);
  if (!p) {
    blad = true;
    return -1;
  }
  double gr = p->wartosc;
  if (!p->tekst.empty()) {  // godzina `00:30` na skali M-18: minuty od 18:00
    int h = liczbaCalk(p->tekst.c_str()), mi = liczbaCalk(p->tekst.c_str() + (p->tekst.size() > 3 ? 3 : 0));
    gr = (double)(((h * 60 + mi) - 18 * 60 + 1440) % 1440);
  }
  std::string pc = param(w, "por_cechy");
  return reguly::porownaj(v, pc.empty() ? "gt" : pc, gr) ? 1 : 0;
}

//  grupy_nocy        metryka, cecha (`co2`, `wilgotnosc`, `weekend`, `start`, `gi`), granica (id_progu; bez niej cecha
//                    ≠ 0), por_cechy (gt domyślnie), n (okno nocy ważnych), min (nocy w grupie, R-EKR-26), delta (Δ),
//                    por (opcjonalnie: ge = grupa cechy wyżej, le = niżej; bez = w obie strony). Mediany grup; test
//                    permutacyjny tylko w przeglądzie (zmienne `p`, `holm`). Zmienne: Metryka, metryka, x, y, n,
//                    różnica, zmiana, dx (|różnica| bez jednostki).
Wynik rGrupyNocy(const WierszKatalogu& w, const Kontekst& k) {
  const CechyNocy* c = cechyOceny();
  if (!c || c->tryb == TrybOceny::DZIENNY) return reguly::nieoceniona(w, "porownanie grup: przeglad tygodnia (R-KON-08)");
  Wynik pusty;
  pusty.id = w.id;
  pusty.wariant = w.wariant;
  if (c->pokazane.count(klucz(pusty))) return reguly::nieoceniona(w, "para pokazana: poza przegladami (9.3)");
  std::string m = param(w, "metryka"), ce = param(w, "cecha"), powod, por = param(w, "por");
  if (m.empty() || ce.empty()) return reguly::nieoceniona(w, "brak parametru metryka albo cecha");
  double n = progLiczba(w, k, "n", powod), mn = progLiczba(w, k, "min", powod), d = progLiczba(w, k, "delta", powod);
  if (std::isnan(n) || std::isnan(mn) || std::isnan(d)) return reguly::nieoceniona(w, powod);
  std::vector<Wazona> a, b;
  bool blad = false;
  for (auto& x : probki(k, m, (int)std::lround(n))) {
    int g = grupaCechy(w, k, ce, x.data, blad, powod);
    if (blad) return reguly::nieoceniona(w, powod);
    if (g == 1) a.push_back({x.v, 1});
    if (g == 0) b.push_back({x.v, 1});
  }
  if ((double)a.size() < mn || (double)b.size() < mn)
    return reguly::nieoceniona(w, "grupy " + std::to_string(a.size()) + " i " + std::to_string(b.size()) + " nocy (" + ce + ")");
  double ma = medianaWazona(a), mb = medianaWazona(b), roz = ma - mb;
  int kier = por == "ge" ? 1 : por == "le" ? -1 : 0;
  bool kand = kier == 0 ? std::fabs(roz) >= std::fabs(d) - 1e-9 : kier * roz >= std::fabs(d) - 1e-9;
  Wynik r = reguly::oceniona(w, kand);
  wspolne(r, m, roz, d);
  r.zmienne["x"] = wartoscZJednostka(m, ma);
  r.zmienne["y"] = wartoscZJednostka(m, mb);
  r.zmienne["a_num"] = liczbaSurowa(ma);
  r.zmienne["b_num"] = liczbaSurowa(mb);
  r.zmienne["dx"] = bezJednostki(m, std::fabs(roz));
  r.zmienne["n"] = std::to_string(a.size() + b.size());
  r.zmienne["na"] = std::to_string(a.size());
  r.zmienne["nb"] = std::to_string(b.size());
  r.zmienne["różnica"] = roznica(m, roz);
  r.zmienne["zmiana"] = zmiana(m, roz);
  r.zmienne["kierunek"] = roz > 0 ? "1" : "-1";
  if (c->tryb == TrybOceny::PRZEGLAD) {
    r.zmienne["p"] = liczbaSurowa(testPermutacyjny(a, b, kier, ziarnoKlucza(k.dzis, klucz(r))));
    r.zmienne["holm"] = "tak";
  }
  return r;
}

struct OpisCzynnika {
  char kod;
  uint8_t bit;
  const char *odmiana, *odmianaW, *mianownik;
};
const OpisCzynnika CZYNNIKI_O12[] = {
    {'A', CZ_ALKOHOL, "po alkoholu", "Po alkoholu", "alkohol"},
    {'K', CZ_KATAR, "przy katarze", "Przy katarze", "katar lub infekcja"},
    {'S', CZ_STRES, "w stresie", "W stresie", "stres"},
    {'J', CZ_KOLACJA, "po późnej kolacji", "Po późnej kolacji", "późna kolacja"},
    {'L', CZ_LEKI, "po lekach nasennych", "Po lekach nasennych", "leki nasenne"},
    {'T', CZ_THC, "po THC", "Po THC", "THC"},
    {'P', CZ_POZA_DOMEM, "poza domem", "Poza domem", "spanie poza domem"},
};
const OpisCzynnika* opisCzynnika(char kod) {
  for (auto& o : CZYNNIKI_O12)
    if (o.kod == kod) return &o;
  return nullptr;
}

//  czynnik_nocy      czynnik (kod A, K, S, J, L, T, P), metryka (M-02, M-07, M-01, M-03ca), delta (Δ), min (nocy w grupie,
//                    O-12.grupa_min), efektywna (O-12.efektywna_min), warstwa_min (R-CZY-24.warstwa_min), mala
//                    (R-CZY-21.mala_grupa), okno (R-CZY-24.okno_odpowiedzi, dni), waga (R-CZY-24.waga_kontrolna),
//                    n (R-EKR-26.okno: ostatnie noce ważne).
//                    Noce z odpowiedzią z warstw O i K (R-CZY-04), mediany ważone 1 / (p × r) (R-CZY-24); test
//                    permutacyjny w przeglądzie. Zmienne: Metryka, x, y, odmiana, Odmiana, czynnik, zmiana, n, m, kod,
//                    mala (`tak` przy grupie < 10).
Wynik rCzynnikNocy(const WierszKatalogu& w, const Kontekst& k) {
  const CechyNocy* c = cechyOceny();
  if (!c || c->tryb == TrybOceny::DZIENNY) return reguly::nieoceniona(w, "O-12: przeglad tygodnia (R-KON-08)");
  Wynik pusty;
  pusty.id = w.id;
  pusty.wariant = w.wariant;
  if (c->pokazane.count(klucz(pusty))) return reguly::nieoceniona(w, "para czynnik-metryka pokazana (9.3)");
  if (!c->czynniki) return reguly::nieoceniona(w, "O-12: brak odpowiedzi czynnikow");
  std::string m = param(w, "metryka"), kod = param(w, "czynnik"), powod;
  const OpisCzynnika* oc = kod.size() == 1 ? opisCzynnika(kod[0]) : nullptr;
  if (m.empty() || !oc) return reguly::nieoceniona(w, "O-12: zly czynnik albo metryka");
  double d = progLiczba(w, k, "delta", powod), mn = progLiczba(w, k, "min", powod), ef = progLiczba(w, k, "efektywna", powod),
         wm = progLiczba(w, k, "warstwa_min", powod), mala = progLiczba(w, k, "mala", powod),
         okno = progLiczba(w, k, "okno", powod), pk = progLiczba(w, k, "waga", powod);
  for (double x : {d, mn, ef, wm, mala, okno, pk})
    if (std::isnan(x)) return reguly::nieoceniona(w, powod);
  double rO = odsetekOdpowiedzi(*c->czynniki, k.dzis, 'O', (int)std::lround(okno));
  double rK = odsetekOdpowiedzi(*c->czynniki, k.dzis, 'K', (int)std::lround(okno));
  // (9.3 „do 90 nocy”): noce z ostatnich R-EKR-26.okno nocy ważnych, jak porównania grup O-06…O-10
  double nOkna = reguly::parametr(w, "n") ? progLiczba(w, k, "n", powod) : 0;
  if (std::isnan(nOkna)) return reguly::nieoceniona(w, powod);
  std::map<std::string, double> wart;
  for (auto& x : probki(k, m, (int)std::lround(nOkna))) wart[x.data] = x.v;
  std::vector<Wazona> a, b;
  int aO = 0, aK = 0, bO = 0, bK = 0;
  for (auto& e : *c->czynniki) {
    if (!wWarstwieO12(e) || e.data > k.dzis) continue;
    auto it = wart.find(e.data);
    if (it == wart.end()) continue;  // noc nieważna albo bez metryki
    bool fa = (e.czynniki & oc->bit) != 0, nic = e.czynniki == CZ_NIC;
    if (!fa && !nic) continue;
    bool warO = e.warstwa == 'O';
    double r = warO ? rO : rK;
    if (std::isnan(r) || r <= 0) return reguly::nieoceniona(w, "O-12: waga nieokreslona (r w warstwie " + std::string(1, e.warstwa) + ")");
    double waga = 1.0 / ((warO ? 1.0 : pk) * r);
    (fa ? a : b).push_back({it->second, waga, e.warstwa});
    (fa ? (warO ? aO : aK) : (warO ? bO : bK))++;
  }
  if ((double)a.size() < mn || (double)b.size() < mn)
    return reguly::nieoceniona(w, "O-12: grupy " + std::to_string(a.size()) + " i " + std::to_string(b.size()) + " nocy");
  if (aO < wm || aK < wm || bO < wm || bK < wm) return reguly::nieoceniona(w, "O-12: grupa bez nocy w warstwie");
  if (efektywna(a) < ef - 1e-9 || efektywna(b) < ef - 1e-9)
    return reguly::nieoceniona(w, "O-12: efektywna liczebnosc " + przecinek1(efektywna(a)) + " i " + przecinek1(efektywna(b)));
  double x = medianaWazona(a), y = medianaWazona(b), roz = x - y;
  Wynik r = reguly::oceniona(w, std::fabs(roz) >= std::fabs(d) - 1e-9);
  wspolne(r, m, roz, d);
  r.zmienne["x"] = wartoscZJednostka(m, x);
  r.zmienne["y"] = wartoscZJednostka(m, y);
  r.zmienne["a_num"] = liczbaSurowa(x);
  r.zmienne["b_num"] = liczbaSurowa(y);
  r.zmienne["odmiana"] = oc->odmiana;
  r.zmienne["Odmiana"] = oc->odmianaW;
  r.zmienne["czynnik"] = oc->mianownik;
  r.zmienne["zmiana"] = zmiana(m, roz);
  r.zmienne["kierunek"] = roz > 0 ? "1" : "-1";
  r.zmienne["n"] = std::to_string(a.size());
  r.zmienne["m"] = std::to_string(b.size());
  r.zmienne["kod"] = kod;
  r.zmienne["mala"] = (double)a.size() < mala || (double)b.size() < mala ? "tak" : "nie";
  if (c->tryb == TrybOceny::PRZEGLAD) {
    r.zmienne["p"] = liczbaSurowa(testPermutacyjny(a, b, 0, ziarnoKlucza(k.dzis, klucz(r))));
    r.zmienne["holm"] = "tak";
  }
  return r;
}

// rozklad_w_nocy (9.3) O-05: czasy zdarzeń w nocy; rekord nocy ich nie ma → „nie oceniam”.
Wynik rRozkladWNocy(const WierszKatalogu& w, const Kontekst&) {
  return reguly::nieoceniona(w, "brak czasow zdarzen w rekordzie nocy");
}
// zmiana_cechy_gi O-11: dominująca cecha GI; moduł GI zablokowany → „nie oceniam”.
Wynik rZmianaCechyGi(const WierszKatalogu& w, const Kontekst&) {
  return reguly::nieoceniona(w, "modul GI zablokowany");
}

}  // namespace

void zarejestrujRodzajeO() {
  reguly::zarejestrujRodzaj("rekord_tygodnia", rRekordTygodnia);
  reguly::zarejestrujRodzaj("tydzien_miesiac", rTydzienMiesiac);
  reguly::zarejestrujRodzaj("grupy_nocy", rGrupyNocy);
  reguly::zarejestrujRodzaj("czynnik_nocy", rCzynnikNocy);
  reguly::zarejestrujRodzaj("zmiana_cechy_gi", rZmianaCechyGi);
  if (!reguly::znanyRodzaj("rozklad_w_nocy")) reguly::zarejestrujRodzaj("rozklad_w_nocy", rRozkladWNocy);
}

// ---------- podzestawy ----------
// podzestaw ma tylko wiersze katalogu i progi (ocen() czyta z niego `R-NOR-10.min_noce`; progi parametrów idą
// przez Kontekst::zestaw = pełny zestaw), bez kopii tekstów (teksty bierze sprawy::/odkrycia:: z pełnego zestawu).
static const reguly::Zestaw& podzestaw(const reguly::Zestaw& z, bool odkrycia) {
  static std::map<std::pair<const reguly::Zestaw*, bool>, reguly::Zestaw> kopie;
  auto kl = std::make_pair(&z, odkrycia);
  auto it = kopie.find(kl);
  if (it != kopie.end() && it->second.identyfikator == z.identyfikator && it->second.progi.size() == z.progi.size())
    return it->second;
  reguly::Zestaw k;
  k.identyfikator = z.identyfikator;
  k.progi = z.progi;
  for (auto& w : z.katalog)
    if (wierszOdkrycia(w.id) == odkrycia) k.katalog.push_back(w);
  kopie[kl] = k;
  return kopie[kl];
}
const reguly::Zestaw& bezOdkryc(const reguly::Zestaw& z) { return podzestaw(z, false); }
const reguly::Zestaw& tylkoOdkrycia(const reguly::Zestaw& z) { return podzestaw(z, true); }

std::vector<Wynik> ocenOdkrycia(const reguly::Zestaw& z, const Kontekst& k, const CechyNocy& c,
                                const reguly::OpcjeOceny& o) {
  Zakres zk(&c);
  reguly::Kontekst kx = k;
  kx.zestaw = &z;
  return reguly::ocen(tylkoOdkrycia(z), kx, o);
}

bool przegladDzis(const std::string& dzis, const std::string& ostatni) {
  if (!dataOk(dzis)) return false;
  if (ostatni == dzis) return false;
  if (dzienTygodnia(dzis) == 0) return true;
  return !dataOk(ostatni) || dniMiedzy(ostatni, dzis) > 7;
}

// ---------- pokazania i kolejka ----------
std::string zapiszPokazania(const std::vector<Pokazanie>& p) {
  std::string s;
  for (auto& x : p) s += (s.empty() ? "" : "|") + x.klucz + "@" + x.data + "@" + liczbaSurowa(x.wartosc);
  return s;
}
std::vector<Pokazanie> parsujPokazania(const std::string& s) {
  std::vector<Pokazanie> r;
  for (auto& e : reguly::wewn::dziel(s, '|')) {
    std::vector<std::string> f = reguly::wewn::dziel(e, '@');
    if (f.size() != 3 || !dataOk(f[1]) || f[0].empty()) continue;
    r.push_back({f[0], f[1], liczbaRzecz(f[2].c_str())});
  }
  return r;
}
std::string zapiszKolejke(const Kolejka& k) {
  std::string s = k.przeglad + ";";
  for (size_t i = 0; i < k.klucze.size(); i++) s += (i ? "|" : "") + k.klucze[i];
  return s;
}
Kolejka parsujKolejke(const std::string& s) {
  Kolejka k;
  size_t p = s.find(';');
  if (p == std::string::npos || !dataOk(s.substr(0, p))) return k;
  k.przeglad = s.substr(0, p);
  for (auto& x : reguly::wewn::dziel(s.substr(p + 1), '|'))
    if (!x.empty()) k.klucze.push_back(x);
  return k;
}
bool porownanieGrup(const std::string& kl) {
  if (kl.size() < 4) return false;
  int n = liczbaCalk(kl.c_str() + 2);
  return kl.compare(0, 2, "O-") == 0 && n >= 6 && n <= 12 && n != 11;
}
std::set<std::string> paryPokazane(const std::vector<Pokazanie>& p) {
  std::set<std::string> s;
  for (auto& x : p)
    if (porownanieGrup(x.klucz)) s.insert(x.klucz);
  return s;
}

double wartoscWyniku(const Wynik& w) {
  auto it = w.zmienne.find("wartosc_num");
  return it == w.zmienne.end() ? NAN : liczbaRzecz(it->second.c_str());
}
static double zmiennaLiczba(const Wynik& w, const char* n) {
  auto it = w.zmienne.find(n);
  return it == w.zmienne.end() ? NAN : liczbaRzecz(it->second.c_str());
}
static int numer(const std::string& id) { return liczbaCalk(id.c_str() + 2); }

const Wynik* wybierz(const WejscieWyboru& w, std::string& powod) {
  std::vector<const Wynik*> kand;
  if (w.dzienne)
    for (auto& r : *w.dzienne)
      if (r.oceniona && r.spelniona && r.stan == reguly::StanReguly::AKTYWNA) kand.push_back(&r);
  std::set<std::string> pary = w.pokazane ? paryPokazane(*w.pokazane) : std::set<std::string>();
  if (w.kolejka && w.kol && w.dzis > w.kol->przeglad)  // D4: z przeglądu (niedziela) od następnego dnia
    for (auto& r : *w.kolejka) {
      std::string kl = klucz(r);
      if (!r.oceniona || !r.spelniona || r.stan != reguly::StanReguly::AKTYWNA) continue;  // R-EKR-30
      if (std::find(w.kol->klucze.begin(), w.kol->klucze.end(), kl) == w.kol->klucze.end()) continue;
      bool dzis = false;  // pokazana dziś zostaje do końca dnia (R-EKR-31)
      if (w.pokazane)
        for (auto& p : *w.pokazane) dzis = dzis || (p.klucz == kl && p.data == w.dzis);
      if (pary.count(kl) && !dzis) continue;
      kand.push_back(&r);
    }
  // R-EKR-31: pokazane dziś zostaje do końca dnia
  if (w.pokazane)
    for (auto& p : *w.pokazane)
      if (p.data == w.dzis)
        for (auto* r : kand)
          if (klucz(*r) == p.klucz) return r;
  if (w.rozruch) { powod = "rozruch globalny (R-EKR-28)"; return nullptr; }
  std::string od = dodajDni(w.dzis, -(w.limitDni - 1));
  auto wOknie = [&](const std::string& d) { return d >= od && d < w.dzis; };
  for (auto& d : w.pokazaniaWspolne)
    if (wOknie(d)) { powod = "limit 1 odkrycie na 7 dni (R-EKR-25): " + d; return nullptr; }
  if (w.pokazane)
    for (auto& p : *w.pokazane)
      if (wOknie(p.data)) { powod = "limit 1 odkrycie na 7 dni (R-EKR-25): " + p.data; return nullptr; }
  std::string odPowrotu = dodajDni(w.dzis, -(w.powrotDni - 1));
  std::vector<const Wynik*> ok;
  for (auto* r : kand) {
    bool zablok = false;
    if (w.pokazane)
      for (auto& p : *w.pokazane)
        if (p.klucz == klucz(*r) && p.data >= odPowrotu && p.data < w.dzis &&
            std::fabs(wartoscWyniku(*r) - p.wartosc) < zmiennaLiczba(*r, "delta_num") - 1e-9)
          zablok = true;  // R-EKR-27
    if (!zablok) ok.push_back(r);
  }
  if (ok.empty()) { powod = kand.empty() ? "brak odkrycia" : "to samo odkrycie w 30 dniach (R-EKR-27)"; return nullptr; }
  std::stable_sort(ok.begin(), ok.end(), [](const Wynik* a, const Wynik* b) {
    if (numer(a->id) != numer(b->id)) return numer(a->id) < numer(b->id);  // R-PIE-03
    double ia = zmiennaLiczba(*a, "iloraz"), ib = zmiennaLiczba(*b, "iloraz");
    return (std::isnan(ia) ? 0 : ia) > (std::isnan(ib) ? 0 : ib);
  });
  return ok.front();
}

// ---------- teksty S1 ----------
static const char* rodzajO(const std::string& id) {
  static const char* const R[] = {"najlepszy tydzień", "tydzień a miesiąc", "pora zdarzeń w nocy", "sypialnia a terapia",
                                  "wilgotność a wyciek", "weekend a dni robocze", "pora startu a noc", "GI a ciśnienie",
                                  "zmiana cechy GI", "czynniki nocy"};
  int n = numer(id);
  return n >= 3 && n <= 12 ? R[n - 3] : "";
}
static std::string tekstRoli(const reguly::Zestaw& z, const reguly::WierszKatalogu* k, const Wynik& w,
                             const std::string& sufiks, reguly::Tryb tryb) {
  if (!k) return "";
  for (auto& id : k->teksty) {
    size_t p = id.rfind('.');
    if (p != std::string::npos && id.substr(p + 1) == sufiks) return reguly::tekst(z, id, w, tryb);
  }
  return "";
}

std::string kluczHumoru(const Wynik& w) {
  if (w.id == "O-12") {
    auto it = w.zmienne.find("kod");
    return "O-12/" + (it == w.zmienne.end() ? std::string() : it->second);
  }
  return w.id;
}

TrescOdkrycia trescOdkrycia(const reguly::Zestaw& z, const Wynik& w0, reguly::Tryb tryb, const UstawieniaTekstow& u,
                            const std::vector<Noc>* hist) {
  Wynik w = w0;
  TrescOdkrycia t;
  t.id = w.id;
  t.klucz = klucz(w);
  t.etykieta = std::string("Odkrycie · ") + rodzajO(w.id);
  const reguly::WierszKatalogu* k = nullptr;
  for (auto& x : z.katalog)
    if (x.id == w.id && x.wariant == w.wariant) k = &x;
  std::string m = w.zmienne["metryka_id"];
  // AHI bez zamiany w obu profilach (5.4.14), więc bez wariantu publicznego
  t.tytul = tekstRoli(z, k, w, "tytul", tryb);
  t.powod = tekstRoli(z, k, w, "powod", tryb);
  t.krok = tekstRoli(z, k, w, "krok", tryb);
  if (w.id == "O-12") {
    t.kodCzynnika = w.zmienne["kod"];
    if (t.kodCzynnika == "L") t.krok = tekstRoli(z, k, w, "krok_leki", tryb);
    if ((int)t.tytul.size() > 0 && (int)std::count_if(t.tytul.begin(), t.tytul.end(), [](char c) { return (c & 0xC0) != 0x80; }) > 30)
      t.tytul = tekstRoli(z, k, w, "tytul_skrot", tryb);  // reguła tytułu O-12: ponad 30 znaków
    std::string przyp = w.zmienne["mala"] == "tak" ? tekstRoli(z, k, w, "przypadek", tryb) : "";
    auto znaki = [](const std::string& s) {
      return (int)std::count_if(s.begin(), s.end(), [](char c) { return (c & 0xC0) != 0x80; });
    };
    t.ukryty = !u.czynnikiWrazliwe && (t.kodCzynnika == "A" || t.kodCzynnika == "T" || t.kodCzynnika == "L");
    if (t.ukryty) {  // R-CZY-20: ekran dnia bez nazwy czynnika, szczegół w K1
      t.tytulK1 = t.tytul;
      t.powodK1 = przyp.empty() ? t.powod : t.powod + " " + przyp;
      t.tytul = tekstRoli(z, k, w, "tytul_ukryty", tryb);
      t.powod = tekstRoli(z, k, w, "powod_ukryty", tryb);
      t.podpisOsi = przyp;  // R-CZY-21: przy czynniku ukrytym zdanie w podpisie osi
    } else if (!przyp.empty()) {
      if (znaki(t.powod + " " + przyp) <= 70) t.powod += " " + przyp;
      else t.podpisOsi = przyp;  // R-CZY-21: powód z dopiskiem ponad 70 znaków
    }
    t.etA = w.zmienne["odmiana"];
    t.etB = "nic szczególnego";
    if (t.ukryty) t.etA = "z czynnikiem";
  }
  if (w.id == "O-03") {
    // krok przy braku ocen z wycieku z katalogu tekstów (ten sam klucz co S-K-18), zapas: ten sam literał
    auto zKatalogu = [&](const char* id, const char* zapas) {
      auto it = z.teksty.find(id);
      return it == z.teksty.end() ? std::string(zapas) : it->second.tekst;
    };
    if (!u.uspokojenie)  // R-EKR-32;: bez kroku na pusty ekran „Otwarte sprawy” (5.4.13 pkt 5 i 7)
      t.krok = u.liniaZdrowia          ? "→ Sprawa z linii wyżej wciąż otwarta."
               : u.tylkoRegulyBezOceny ? "→ Część reguł nie działa: zobacz Test sprzętu."
               : u.tylkoOcenyWylaczone ? "→ Część ocen wyłączona: zobacz ▶."
               : u.brakDanychWyciek    ? zKatalogu("S-K-18.krok_dane_wyciek", "→ Bezdechów nie oceniam: noce z wyciekiem.")
               : u.tylkoBrakDanych     ? "→ Część ocen jeszcze nie działa: za mało nocy."  //
               : u.bezKartSpraw        ? (u.bezSilnika ? "→ Część reguł nie działa: zobacz Test sprzętu."
                                                       : "→ Obserwuj kolejne noce.")  // R-EKR-32, R-KOK-19
                                       : "→ Otwarte sprawy: za stroną 2 / 3.";
    t.linia = true;
    t.podpisOsi = "ostatnie 30 nocy";
    if (hist) {
      std::vector<const Noc*> h;
      for (auto& n : *hist) h.push_back(&n);
      std::sort(h.begin(), h.end(), [](const Noc* a, const Noc* b) { return a->data < b->data; });
      std::string dzis = h.empty() ? "" : h.back()->data;
      for (int i = 29; i >= 0; i--) {
        std::string d = dodajDni(dzis, -i);
        double v = NAN;
        for (auto* n : h)
          if (n->data == d && n->wazna) v = reguly::metryka(*n, m);
        t.wartosci30.push_back(v);
      }
    }
  }
  if (w.zmienne.count("a_num") && w.zmienne.count("b_num")) {
    t.a = liczbaRzecz(w.zmienne["a_num"].c_str());
    t.b = liczbaRzecz(w.zmienne["b_num"].c_str());
    t.wartA = bezJednostki(m, t.a);
    t.wartB = bezJednostki(m, t.b);
    if (w.id == "O-04") { t.etA = "ostatnie 7 nocy"; t.etB = "ostatnie 30 nocy"; }
    if (w.id == "O-06") { t.etA = "CO2 ponad próg"; t.etB = "niżej"; }
    if (w.id == "O-07") { t.etA = "suche noce"; t.etB = "pozostałe"; }
    if (w.id == "O-08") { t.etA = "weekend · " + w.zmienne["na"] + " nocy"; t.etB = "dni robocze · " + w.zmienne["nb"] + " nocy"; }
    if (w.id == "O-06" || w.id == "O-07" || w.id == "O-09") {
      t.etA += " · " + w.zmienne["na"] + " nocy";
      t.etB += " · " + w.zmienne["nb"] + " nocy";
    }
    if (w.id == "O-09") { t.etA = "później · " + w.zmienne["na"] + " nocy"; t.etB = "wcześniej · " + w.zmienne["nb"] + " nocy"; }
  }
  t.doLekarza = t.krok.find("lekarz") != std::string::npos || t.krok.find("kontroli") != std::string::npos;
  return t;
}

}  // namespace odkrycia
