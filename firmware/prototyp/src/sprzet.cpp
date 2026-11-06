// SPDX-License-Identifier: GPL-3.0-or-later
// Moduł SPRZĘT (spec 5.9): opis w src/sprzet.h.
#include "sprzet.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "nawigacja.h"
#include "wspolne.h"

namespace sprzet {

namespace {

const char* const KLUCZ_EL[EL_N] = {"maska", "filtr"};

// noce ważne z M-07 P95 do `dzis` włącznie (po dacie rosnąco)
std::vector<const Noc*> waznezWyciekiem(const std::vector<Noc>& h, const std::string& dzis) {
  std::vector<const Noc*> v;
  for (auto& n : h)
    if (n.data <= dzis && n.wazna && !std::isnan(n.wp95)) v.push_back(&n);
  return v;
}

double medianaWycieku(const std::vector<const Noc*>& v, size_t od, size_t ile) {
  std::vector<double> x;
  for (size_t i = od; i < od + ile && i < v.size(); i++) x.push_back(v[i]->wp95);
  return mediana(x);
}

int dniZWpisem(const std::vector<WpisHumoru>& d, const char* typ, const std::string& sprawa) {
  std::vector<std::string> dni;
  for (auto& x : d) {
    if (x.typ != typ) continue;
    if (x.klucz != sprawa && x.klucz.compare(0, sprawa.size() + 1, sprawa + "#") != 0) continue;
    if (std::find(dni.begin(), dni.end(), x.data) == dni.end()) dni.push_back(x.data);
  }
  return (int)dni.size();
}

std::vector<std::string> wiersze(const std::string& t) {
  std::vector<std::string> l;
  size_t a = 0;
  while (a <= t.size()) {
    size_t b = t.find('\n', a);
    if (b == std::string::npos) b = t.size();
    l.push_back(t.substr(a, b - a));
    a = b + 1;
  }
  return l;
}

}  // namespace

bool okresMaska(int e) { return e == EL_MASKA; }

int okresElementu(const ElementWymiany& e) { return e.okres; }

bool poTerminie(const ElementWymiany& e, const std::string& dzis) {
  std::string t = terminWymiany(e);
  return !t.empty() && dataOk(dzis) && dzis >= t;
}

int pelneMiesiace(const std::string& od, const std::string& dzis) {
  if (!dataOk(od) || !dataOk(dzis) || dzis < od) return -1;
  int r1 = liczbaCalk(od.substr(0, 4).c_str()), m1 = liczbaCalk(od.substr(5, 2).c_str()), d1 = liczbaCalk(od.substr(8, 2).c_str());
  int r2 = liczbaCalk(dzis.substr(0, 4).c_str()), m2 = liczbaCalk(dzis.substr(5, 2).c_str()), d2 = liczbaCalk(dzis.substr(8, 2).c_str());
  bool ostatni = dodajDni(dzis, 1).substr(5, 2) != dzis.substr(5, 2);  // jak termin przycięty do końca miesiąca
  return (r2 - r1) * 12 + (m2 - m1) - (d2 < d1 && !ostatni ? 1 : 0);
}

std::string sprawaElementu(int element, const ElementWymiany& e) {
  if (element < 0 || element >= EL_N) return "";
  return std::string(KLUCZ_EL[element]) + "@" + e.wymieniono;
}

// jeden licznik pokazań S-K-31 dla maski. Wpisy sprzed (sprawa `poduszka@{data}`) liczą się, gdy data wymiany
// jest ta sama co dziś w `maska@{data}` (migracja: maska = późniejsza z dat poduszki i maski).
static int pokazaniaMaski(const std::vector<WpisHumoru>& d, const ElementWymiany& e) {
  return dniZWpisem(d, "SK31", "maska@" + e.wymieniono) + dniZWpisem(d, "SK31", "poduszka@" + e.wymieniono);
}

double nachylenie(const std::vector<double>& v) {
  size_t n = v.size();
  if (n < 2) return NAN;
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (size_t i = 0; i < n; i++) {
    double x = (double)i;
    sx += x;
    sy += v[i];
    sxx += x * x;
    sxy += x * v[i];
  }
  double m = (double)n * sxx - sx * sx;
  if (m == 0) return NAN;
  return ((double)n * sxy - sx * sy) / m;
}

// Wykrycie spadku wycieku z dnia `dzis` (R-MAS-12): 3 ostatnie noce ważne wobec 14 przed nimi (bez nocy infekcji).
static bool spadek(const std::vector<Noc>& h, const std::string& dzis, const std::vector<std::string>& infekcja,
                   std::string& odNocy, float& przed, float& po) {
  std::vector<const Noc*> v;
  for (auto* n : waznezWyciekiem(h, dzis))
    if (std::find(infekcja.begin(), infekcja.end(), n->data) == infekcja.end()) v.push_back(n);
  const size_t k = (size_t)PYT_NOCE_SPADKU, b = (size_t)SK32_NOCE_PRZED;
  if (v.size() < k + b) return false;
  size_t s = v.size() - k;
  double mPo = medianaWycieku(v, s, k), mPrzed = medianaWycieku(v, s - b, b);
  if (std::isnan(mPo) || std::isnan(mPrzed) || mPrzed - mPo < PYT_SPADEK) return false;
  odNocy = v[s]->data;
  przed = (float)mPrzed;
  po = (float)mPo;
  return true;
}

Elementy zOdpowiedziami(const ElementWymiany (&el)[EL_N], const std::vector<Noc>& hist,
                        const std::vector<std::string>& tak, const std::vector<std::string>& noceInfekcji) {
  Elementy e;
  for (int i = 0; i < EL_N; i++) e.el[i] = el[i];
  // „tak” to odpowiedź na pytanie, które już stwierdziło spadek (R-MAS-12), więc data = pierwsza z 3 ostatnich
  // nocy ważnych z dnia pytania, tą samą metodą co pytanie (bez nocy okresu infekcji), bez ponownego liczenia spadku
  for (auto& d : tak) {
    std::vector<const Noc*> v;
    for (auto* n : waznezWyciekiem(hist, d))
      if (std::find(noceInfekcji.begin(), noceInfekcji.end(), n->data) == noceInfekcji.end()) v.push_back(n);
    if (v.size() < (size_t)PYT_NOCE_SPADKU) continue;
    std::string od = v[v.size() - (size_t)PYT_NOCE_SPADKU]->data;
    if (od > e.el[EL_MASKA].wymieniono) e.el[EL_MASKA].wymieniono = od;
  }
  return e;
}

WynikSK31 ocenSK31(const WejscieSprzetu& w) {
  WynikSK31 r;
  if (!w.hist || !w.el || !dataOk(w.dzis)) { r.dlaczego = "brak danych"; return r; }
  // (1) maska po terminie (poduszka i maska to jeden element, remis R-MAS-07 wycofany)
  if (poTerminie(w.el->el[EL_MASKA], w.dzis)) r.element = EL_MASKA;
  if (r.element < 0) { r.dlaczego = "maska w terminie albo bez daty"; return r; }
  if (w.okresInfekcji) { r.dlaczego = "okres infekcji (R-INF-02)"; return r; }  // (4)
  if (!w.norma || !w.norma->wp95.jest()) { r.dlaczego = "M-07 bez normy (R-NOR-11)"; return r; }
  std::vector<const Noc*> v = waznezWyciekiem(*w.hist, w.dzis);
  if (v.size() < (size_t)SK31_NOCE_TRENDU) { r.dlaczego = "za mało nocy ważnych"; return r; }
  // (2) mediana 7 ostatnich nocy ważnych ≥ mediana normy + Δ
  double m7 = medianaWycieku(v, v.size() - 7, 7);
  if (!(m7 >= w.norma->wp95.med + DELTA_M07)) { r.dlaczego = "wyciek 7 nocy poniżej normy + Δ"; return r; }
  // (3) nachylenie z 14 ostatnich nocy ważnych > 0, bez zmiany ustawień w oknie (4.8)
  auto okno = [&](size_t ile, bool& ust) {
    std::vector<double> x;
    ust = true;
    const std::string& u0 = v[v.size() - ile]->ustawienia;
    for (size_t i = v.size() - ile; i < v.size(); i++) {
      x.push_back(v[i]->wp95);
      if (v[i]->ustawienia != u0) ust = false;
    }
    return nachylenie(x);
  };
  bool bezZmian = false;
  double n14 = okno((size_t)SK31_NOCE_TRENDU, bezZmian);
  if (!bezZmian) { r.dlaczego = "zmiana ustawień w oknie 14 nocy"; return r; }
  if (!(n14 > 0)) { r.dlaczego = "wyciek nie rośnie"; return r; }
  r.tygodni = 2;
  for (size_t t : {(size_t)21, (size_t)28}) {
    if (v.size() < t) break;
    bool bz = false;
    if (okno(t, bz) > 0 && bz) r.tygodni = (int)(t / 7);
  }
  r.warunek = true;
  r.miesiecy = std::max(0, pelneMiesiace(w.el->el[r.element].wymieniono, w.dzis));
  r.sprawa = sprawaElementu(r.element, w.el->el[r.element]);
  for (size_t i = v.size() > 28 ? v.size() - 28 : 0; i < v.size(); i++) r.linia.push_back(v[i]->wp95);
  if (w.dziennik) {  // raz na 7 dni (7.4); dzień z wpisem zostaje przy swoim ekranie
    std::string ost;
    for (auto& x : *w.dziennik)
      if (x.typ == "SK31" && x.data < w.dzis && x.data > ost) ost = x.data;
    if (!ost.empty() && dniMiedzy(ost, w.dzis) < SK31_CO_DNI) { r.limit = true; r.dlaczego = "limit raz na 7 dni"; }
  }
  return r;
}

int pokazaniaSK31(const std::vector<WpisHumoru>& d, const std::string& sprawa) { return dniZWpisem(d, "SK31", sprawa); }

bool blokadaL09(const WejscieSprzetu& w, const char** powod) {
  const char* p = "";
  bool b = false;
  if (w.el && w.hist && dataOk(w.dzis)) {
    // element po terminie blokuje L-09 tylko, gdy S-K-31 może się pokazać (wyciek rośnie wobec normy
    // M-07, norma jest, poza okresem infekcji); płaski wysoki wyciek albo brak normy: L-09 oceniany normalnie
    // (fail-path R-MAS-03). poduszka i maska to jeden element, więc jeden licznik pokazań (R-MAS-10).
    bool moze = ocenSK31(w).warunek;
    const ElementWymiany& x = w.el->el[EL_MASKA];
    if (poTerminie(x, w.dzis)) {
      // R-MAS-10: jeden licznik pokazań maski
      int k = w.dziennik ? pokazaniaMaski(*w.dziennik, x) : 0;
      if (moze && k < SK31_POKAZAN_BLOKADY) { b = true; p = "element po terminie, S-K-31 pokazane mniej niż 4 razy (R-MAS-03)"; }  // tekst logu bez zmiany (symulator)
    } else if (dataOk(x.wymieniono) && x.wymieniono <= w.dzis) {
      int n = 0;
      for (auto* nc : waznezWyciekiem(*w.hist, w.dzis))
        if (nc->data > x.wymieniono) n++;
      if (n < L09_NOCE_PO_WYMIANIE) { b = true; p = "mniej niż 14 nocy ważnych od wymiany (R-MAS-08)"; }
    }
  }
  if (powod) *powod = p;
  return b;
}

WynikSK32 ocenSK32(const WejscieSprzetu& w) {
  WynikSK32 r;
  if (!w.hist || !w.el || !dataOk(w.dzis)) { r.dlaczego = "brak danych"; return r; }
  std::vector<const Noc*> v = waznezWyciekiem(*w.hist, w.dzis);
  const int el = EL_MASKA;
  {
    const std::string& d = w.el->el[el].wymieniono;
    if (!(dataOk(d) && d <= w.dzis)) { r.dlaczego = "brak zapisanej wymiany"; return r; }
  }
  const std::string& wym = w.el->el[el].wymieniono;
  std::vector<double> przed, po;
  for (auto* n : v) {
    if (n->data <= wym) przed.push_back(n->wp95);
    else po.push_back(n->wp95);
  }
  if ((int)po.size() < SK32_NOCE_PO) { r.dlaczego = "mniej niż 7 nocy ważnych po wymianie"; return r; }
  po.resize((size_t)SK32_NOCE_PO);  // pierwsze 7 nocy ważnych po wymianie
  if (przed.size() > (size_t)SK32_NOCE_PRZED) przed.erase(przed.begin(), przed.end() - SK32_NOCE_PRZED);
  if ((int)przed.size() < SK32_GRUPA_MIN || (int)po.size() < SK32_GRUPA_MIN) { r.dlaczego = "za mało nocy (R-MAS-09)"; return r; }
  double a = mediana(przed), b = mediana(po);
  r.element = el;
  r.przed = (float)a;
  r.po = (float)b;
  r.okresMies = w.el->el[el].okres;
  r.sprawa = sprawaElementu(el, w.el->el[el]);
  if (!(a - b >= DELTA_M07)) { r.dlaczego = "bez poprawy o Δ"; return r; }
  if (w.dziennik && dniZWpisem(*w.dziennik, "SK32", r.sprawa) > 0) {
    bool dzis = false;
    for (auto& x : *w.dziennik)
      if (x.typ == "SK32" && x.data == w.dzis) dzis = true;
    if (!dzis) { r.dlaczego = "już pokazane dla tej wymiany"; return r; }
  }
  r.warunek = true;
  return r;
}

WynikPytaniaPoduszki ocenPytaniePoduszki(const WejscieSprzetu& w, const std::vector<std::string>& pytania,
                                         const std::vector<std::string>& infekcja, const std::string& zmianaMaski) {
  WynikPytaniaPoduszki r;
  if (!w.hist || !w.el || !dataOk(w.dzis)) return r;
  for (auto& d : pytania)
    if (dataOk(d) && d < w.dzis && dniMiedzy(d, w.dzis) < PYT_CO_DNI) return r;  // R-MAS-12: raz na 30 dni
  std::string od;
  float a = NAN, b = NAN;
  if (!spadek(*w.hist, w.dzis, infekcja, od, a, b)) return r;
  // w tych 17 nocach bez zapisu wymiany maski, nowego modelu ani zmiany `MSK`
  std::vector<const Noc*> v;
  for (auto* n : waznezWyciekiem(*w.hist, w.dzis))
    if (std::find(infekcja.begin(), infekcja.end(), n->data) == infekcja.end()) v.push_back(n);
  const std::string& pocz = v[v.size() - (size_t)(PYT_NOCE_SPADKU + SK32_NOCE_PRZED)]->data;
  {
    const std::string& d = w.el->el[EL_MASKA].wymieniono;
    if (dataOk(d) && d >= pocz && d <= w.dzis) return r;
  }
  if (dataOk(zmianaMaski) && zmianaMaski >= pocz && zmianaMaski <= w.dzis) return r;
  r.jest = true;
  r.odNocy = od;
  r.przed = a;
  r.po = b;
  return r;
}

std::string zapiszWymiane(const std::string& t, int element, const std::string& data) {
  if (element < 0 || element >= EL_N || !dataOk(data)) return t;
  std::string klucz = std::string("wymiana.") + KLUCZ_EL[element];
  std::string wiersz = klucz + "=" + data;
  std::vector<std::string> l = wiersze(t);
  if (!l.empty() && l.back().empty()) l.pop_back();
  bool jest = false;
  for (auto& x : l) {
    std::string y = x;
    if (!y.empty() && y.back() == '\r') y.pop_back();
    size_t r = y.find('=');
    if (r == std::string::npos) continue;
    std::string k = y.substr(0, r);
    while (!k.empty() && k.back() == ' ') k.pop_back();
    if (k == klucz) { x = wiersz; jest = true; }
  }
  if (!jest) l.push_back(wiersz);
  std::string o;
  for (auto& x : l) o += x + "\n";
  return o;
}

std::string przywrocWymiane(const std::string& t, int element, const std::string& data) {
  if (element < 0 || element >= EL_N) return t;
  if (!data.empty()) return zapiszWymiane(t, element, data);
  std::string klucz = std::string("wymiana.") + KLUCZ_EL[element];
  std::vector<std::string> l = wiersze(t);
  if (!l.empty() && l.back().empty()) l.pop_back();
  std::string o;
  for (auto& x : l) {
    std::string y = x;
    if (!y.empty() && y.back() == '\r') y.pop_back();
    size_t r = y.find('=');
    std::string k = r == std::string::npos ? "" : y.substr(0, r);
    while (!k.empty() && k.back() == ' ') k.pop_back();
    if (k != klucz) o += x + "\n";
  }
  return o;
}

std::vector<WierszSprzetu> wierszeSprzetu(const Elementy& e, const std::string& dzis) {
  static const char* const NAZWA[EL_N] = {"Maska", "Filtr"};
  std::vector<WierszSprzetu> w;
  for (int i = 0; i < EL_N; i++) {
    WierszSprzetu x;
    x.nazwa = NAZWA[i];
    x.wiek = wiekElementu(e.el[i], dzis);
    x.okres = okresElementuTekst(e.el[i]);
    x.poTerminie = poTerminie(e.el[i], dzis);
    w.push_back(x);
  }
  return w;
}

// (5.9.3, mapa 5.6.2): ◀ ▶ przesuwają kursor, przed pierwszym wierszem ◀ wraca na poprzednią stronę, za
// ostatnim ▶ idzie do U-01; ◀ 2 s „wymienione dziś” (element pod kursorem), ↻ ekran dnia, ↻ 2 s nowy model (maska).
std::string stopkaSprzetu(int kursor) {
  std::string l = kursor <= 0 ? "◀ wstecz" : "◀ wyżej";
  std::string r = kursor >= EL_N - 1 ? "▶ ustawienia" : "▶ niżej";
  return l + " · 2 s wymienione   " + r + "   ↻ ekran dnia" + (okresMaska(kursor) ? " · 2 s nowy model" : "");
}

// Kanwa `Sprzet` (design/kanwa/Sprzet.dc.html) w języku reszty panelu (
// „dopasuj do całości”): nagłówek i stopka jak w K1 i „Otwartych sprawach” (naglowekEkranu,
// stopkaEkranu), nazwy kolumn jak etykiety (P22B wersalikami, rozstrzelenie 2 px), wiersze 44 px co 50 px od y 96 na
// szerokości wierszy K1 (12…788, tekst od marginesu 28), nazwy i wartości w P24S (600, jak wartości „Otwartych spraw”;
// kanwa ma jedną wagę dla całego wiersza), kreska 1 px między wierszami, kursor: ramka 3 px z odstępem 3 px, negatyw =
// po terminie. Podpis w P22B: w jednej linii zajmuje 856 px przy 744 px miejsca (minimum B7, SPEC-B), więc idzie w
// dwóch liniach nad stopką, a miejsce na nie daje niższy wiersz (44 zamiast 50 px), nie krótszy tekst.
void rysujSprzet(Plotno& p, const std::vector<WierszSprzetu>& w, int kursor) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekEkranu(p, {"Sprzęt", numerStronyTekst(Ekran::SPRZET)});  // `5 / 6`
  const int X1 = 28, X2 = 462, X3 = 600, W = 744, Y0 = 96, H = 44, KROK = 50;
  tekst(p, F_P22B, X1, 84, wersaliki("element"), true, 2);
  tekst(p, F_P22B, X2, 84, wersaliki("wiek"), true, 2);
  tekst(p, F_P22B, X3, 84, wersaliki("wymiana co"), true, 2);
  const int n = (int)std::min<size_t>(w.size(), 5);
  for (int i = 0; i < n; i++) {
    int y = Y0 + KROK * i;
    bool neg = w[(size_t)i].poTerminie;
    if (neg) p.prostokat(12, y, 776, H, true);
    // kreska w połowie odstępu, bez kreski przy kursorze (ramka) i pod ostatnim wierszem
    else if (i < n - 1 && i != kursor && i + 1 != kursor) p.prostokat(12, y + H + 2, 776, 1, true);
    if (i == kursor) p.ramka(6, y - 6, 788, H + 12, 3, true);
    tekst(p, F_P24S, X1, y + 30, w[(size_t)i].nazwa, !neg);
    tekst(p, F_P24S, X2, y + 30, w[(size_t)i].wiek, !neg);
    tekst(p, F_P24S, X3, y + 30, w[(size_t)i].okres, !neg);
  }
  auto l = zawin(F_P22B, "Negatyw = po terminie. Ekran o wymianie pojawi się dopiero, gdy rośnie też wyciek.", W);
  for (size_t i = 0; i < l.size() && i < 2; i++) tekst(p, F_P22B, X1, (l.size() > 1 ? 380 : 406) + 26 * (int)i, l[i]);
  // stopka 5.9.3: wszystkie grupy z tekstu stopki (oddzielone trzema spacjami) po kolei od lewej: ◀ ▶ ↻
  std::vector<std::string> grupy;
  std::string s = stopkaSprzetu(kursor);
  for (size_t a = 0; a < s.size();) {
    size_t b = s.find("   ", a);
    grupy.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
    a = b == std::string::npos ? s.size() : b + 3;
  }
  stopkaEkranu(p, grupy, "");
}

}  // namespace sprzet
