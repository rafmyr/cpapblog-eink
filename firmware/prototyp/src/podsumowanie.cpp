// SPDX-License-Identifier: GPL-3.0-or-later
// S7, S8, „Co znaczą te liczby” i ekran danych S1; opis w src/podsumowanie.h.
#include "podsumowanie.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "wspolne.h"

namespace podsumowanie {

const char* const MIESIACE_M[12] = {"styczeń", "luty",   "marzec",   "kwiecień",    "maj",      "czerwiec",
                                    "lipiec",  "sierpień", "wrzesień", "październik", "listopad", "grudzień"};
const char* const MIESIACE_MS[12] = {"styczniu", "lutym",   "marcu",    "kwietniu",     "maju",       "czerwcu",
                                     "lipcu",    "sierpniu", "wrześniu", "październiku", "listopadzie", "grudniu"};

namespace {

const int X = 28, XP = 772;

int miesiac(const std::string& d) { return liczbaCalk(d.substr(5, 2).c_str()); }
std::string poprzedniMiesiac(const std::string& dzis, int ile) {  // `RRRR-MM` miesiąca o `ile` wcześniej
  int r = liczbaCalk(dzis.substr(0, 4).c_str()), m = miesiac(dzis) - ile;
  while (m < 1) { m += 12; r--; }
  char b[32];
  snprintf(b, sizeof b, "%04d-%02d", r, m);
  return b;
}
int dniMiesiaca(const std::string& rm) {
  std::string pierwszy = rm + "-01";
  int r = liczbaCalk(rm.substr(0, 4).c_str()), m = liczbaCalk(rm.substr(5, 2).c_str());
  char b[32];
  snprintf(b, sizeof b, "%04d-%02d-01", m == 12 ? r + 1 : r, m == 12 ? 1 : m + 1);
  return dniMiedzy(pierwszy, b);
}
std::vector<const Noc*> noceMiesiaca(const std::vector<Noc>& h, const std::string& rm) {
  std::vector<const Noc*> v;
  for (auto& n : h)
    if (n.data.compare(0, 7, rm) == 0) v.push_back(&n);
  return v;
}
int wazneMiesiaca(const std::vector<Noc>& h, const std::string& rm) {
  int k = 0;
  for (auto* n : noceMiesiaca(h, rm)) k += n->wazna;
  return k;
}
double medianaMetryki(const std::vector<Noc>& h, const std::string& rm, const std::string& m) {
  std::vector<double> v;
  for (auto* n : noceMiesiaca(h, rm)) {
    if (!n->wazna) continue;
    double x = reguly::metryka(*n, m);
    if (!std::isnan(x)) v.push_back(x);
  }
  return mediana(v);
}
double odchStd(const std::vector<double>& v) {
  if (v.size() < 2) return NAN;
  double s = 0, q = 0;
  for (double x : v) s += x;
  s /= (double)v.size();
  for (double x : v) q += (x - s) * (x - s);
  return std::sqrt(q / (double)(v.size() - 1));
}
double delta(const char* id, double zapas, reguly::Tryb t) {
  const reguly::Prog* p = reguly::zestawWbudowany().prog(id, t);
  if (!p || !p->tekst.empty()) return zapas;
  return std::fabs(p->jednostka == "h" ? p->wartosc * 60 : p->wartosc);
}

struct MetrykaS7 {
  const char *id, *nazwa, *delta;
  char rodzaj;  // m, n, p (męski, nijaki, mnogi) do przymiotnika
  double zapas;
};

}  // namespace

// ---------- S7 ----------
bool kandydatS7(const std::string& dzis, const std::vector<Noc>& hist, int minNocy) {
  if (!dataOk(dzis) || dzis.substr(8, 2) != "01") return false;
  return wazneMiesiaca(hist, poprzedniMiesiac(dzis, 1)) >= minNocy;
}

// profil publiczny nie zmienia S7: AHI w obu profilach (5.4.14)
DaneS7 budujS7(const std::string& dzis, const std::vector<Noc>& hist, reguly::Tryb tryb, bool /*profilPubliczny*/,
               int minNocy) {
  DaneS7 d;
  if (!dataOk(dzis)) return d;
  std::string rm = poprzedniMiesiac(dzis, 1), rp = poprzedniMiesiac(dzis, 2);
  int mi = liczbaCalk(rm.substr(5, 2).c_str());
  d.etykieta = std::string("Twój ") + MIESIACE_M[mi - 1];
  int ponad4 = 0, minut = 0;
  for (auto* n : noceMiesiaca(hist, rm)) {
    if (n->m01 >= 240) ponad4++;
    if (n->m01 > 0) minut += n->m01;
  }
  d.noce = std::to_string(ponad4);
  d.z = "/" + std::to_string(dniMiesiaca(rm));
  d.podpisNocy = "nocy ponad 4 h";
  d.ahi = przecinek1(medianaMetryki(hist, rm, "M-02"));
  d.podpisAhi = "AHI, mediana";
  d.godziny = std::to_string((int)std::lround(minut / 60.0));
  d.podpisGodzin = "h z maską";
  // zdanie porównania (R-EKR-19, R-EKR-20): oba miesiące ≥ 20 nocy ważnych
  if (wazneMiesiaca(hist, rm) < minNocy || wazneMiesiaca(hist, rp) < minNocy) return d;
  bool asv = tryb == reguly::Tryb::ASV || tryb == reguly::Tryb::ASVAUTO;
  const MetrykaS7 M[] = {
      {"M-01", "Czas na masce", "R-NOR-12.delta.M-01", 'm', 90},
      {"M-02", "AHI", "R-NOR-12.delta.M-02", 'n', 3},
      {"M-03ca", "Bezdechy centralne", "R-NOR-12.delta.M-03", 'p', 1},
      {asv ? "M-06p" : "M-05p", asv ? "EPAP w szczycie" : "Ciśnienie w szczycie",
       asv ? "R-NOR-12.delta.M-06p" : "R-NOR-12.delta.M-05p", 'n', 2},
      {"M-07", "Wyciek", "R-NOR-12.delta.M-07", 'm', 10},
      {"M-13", "Oddechy na minutę", "R-NOR-12.delta.M-13", 'p', 2},
      {"M-17", "Przerwy w nocy", "R-NOR-12.delta.M-17", 'p', 2},
  };
  int pm = liczbaCalk(rp.substr(5, 2).c_str());
  std::string wPop = MIESIACE_MS[pm - 1];
  double najw = 0;
  const MetrykaS7* wyb = nullptr;
  double zm = 0;
  for (auto& m : M) {
    double a = medianaMetryki(hist, rm, m.id), b = medianaMetryki(hist, rp, m.id);
    if (std::isnan(a) || std::isnan(b)) continue;
    double il = std::fabs(a - b) / delta(m.delta, m.zapas, tryb);
    if (il >= 1 - 1e-9 && il > najw + 1e-12) {
      najw = il;
      wyb = &m;
      zm = a - b;
    }
  }
  if (wyb) {
    d.metrykaZdania = wyb->id;
    bool w = zm > 0;
    const char* przym = wyb->rodzaj == 'm' ? (w ? "wyższy" : "niższy") : (w ? "wyższe" : "niższe");
    d.zdanie = std::string(wyb->nazwa) + " " + przym + " niż w " + wPop + ".";
    return d;
  }
  // bez różnicy ≥ Δ: najstabilniejszy miesiąc (≥ 3 miesiące historii, najmniejsze odchylenie standardowe AHI)
  std::map<std::string, std::vector<double>> ahi;
  for (auto& n : hist)
    if (n.wazna && !std::isnan(n.ahi) && n.data.substr(0, 7) <= rm) ahi[n.data.substr(0, 7)].push_back(n.ahi);
  bool stab = ahi.size() >= 3 && ahi.count(rm);
  double sd = stab ? odchStd(ahi[rm]) : NAN;
  for (auto& kv : ahi)
    if (stab && kv.first != rm && !std::isnan(odchStd(kv.second)) && !(sd < odchStd(kv.second))) stab = false;
  d.zdanie = stab && !std::isnan(sd) ? "Najstabilniejszy miesiąc od początku pomiarów." : "Podobnie jak w " + wPop + ".";
  return d;
}

TrescEkranu trescS7(const DaneS7& d) {
  TrescEkranu t;
  t.etykieta = wersaliki(d.etykieta);  // etykieta wersalikami jak na kanwie (SPEC-B 2)
  t.stopkaStandardowa = true;
  return t;
}

void rysujS7(Plotno& p, const Naglowek& n, const DaneS7& d, const TrescEkranu& t) {
  rysujEkranDanych(p, n, t);
  int h = t.linia.empty() ? 0 : (t.liniaA ? 60 : 48);
  int y = 234 + h / 2;  // linia bazowa liczb (kanwa S7-miesiac: 235)
  const int kol[3] = {X, 284, 540};
  int x = tekst(p, F_SN96, kol[0], y, d.noce);
  tekst(p, F_P34B, x + 2, y, d.z);
  tekst(p, F_SN96, kol[1], y, d.ahi);
  x = tekst(p, F_SN96, kol[2], y, d.godziny);
  tekst(p, F_SN96, x + 18, y, "h");
  tekst(p, F_P22B, kol[0], y + 34, d.podpisNocy);
  tekst(p, F_P22B, kol[1], y + 34, d.podpisAhi);
  tekst(p, F_P22B, kol[2], y + 34, d.podpisGodzin);
  if (!d.zdanie.empty()) {
    p.prostokat(X, 348, XP - X, 2, true);
    const GFXfont* f = szer(F_SB44, d.zdanie) <= XP - X ? F_SB44 : F_P34B;
    tekst(p, f, X, 404, d.zdanie);
  }
}

// ---------- S8 ----------
static std::string pasmo(const NormaMetryki& m, int rodzaj) {  // 0: 1 miejsce, 1: całkowite, 2: czas
  if (rodzaj == 0) return przecinek1(m.p10) + "–" + przecinek1(m.p90);
  if (rodzaj == 1) return calkowita(m.p10) + "–" + calkowita(m.p90);
  return godzH((int)std::lround(m.p10)) + "–" + godzH((int)std::lround(m.p90));
}

bool kandydatS8(const Norma& n, bool pokazane) { return !pokazane && n.ahi.n >= 30 && n.m01.n >= 30; }

DaneS8 budujS8(const WejscieS8& w) {
  DaneS8 d;
  Norma pusta;
  const Norma& n = w.norma ? *w.norma : pusta;
  std::string ahi = "AHI";  // w obu profilach
  d.wiersze.push_back(n.ahi.jest() ? ahi + " " + pasmo(n.ahi, 0) + " / h" : ahi + ": uczę się");
  d.wiersze.push_back(n.wp95.jest() ? "Wyciek " + pasmo(n.wp95, 1) + " l/min" : "Wyciek: uczę się");
  d.wiersze.push_back(n.m01.jest() ? "Czas " + pasmo(n.m01, 2) : "Czas: uczę się");
  if (n.ca.jest()) {
    d.wiersze.push_back("Centralne " + pasmo(n.ca, 0) + " / h");
    d.centralne = true;
  }
  bool asv = w.tryb == reguly::Tryb::ASV || w.tryb == reguly::Tryb::ASVAUTO;
  double pAhi = reguly::zestawWbudowany().prog("R-EKR-36.p90_ahi", w.tryb) ? reguly::zestawWbudowany().prog("R-EKR-36.p90_ahi", w.tryb)->wartosc : 5;
  double pCa = reguly::zestawWbudowany().prog("R-EKR-36.p90_ca", w.tryb) ? reguly::zestawWbudowany().prog("R-EKR-36.p90_ca", w.tryb)->wartosc : 2;
  if (n.ahi.jest() && n.ahi.p90 >= pAhi - 1e-9) {
    d.krok = "→ Pasmo AHI wysokie. Obserwuj kolejne noce.";  // R-EKR-36 (brzmienie po przeglądzie klinicznym)
    d.informacja = true;
  } else if (n.ca.jest() && n.ca.p90 >= pCa - 1e-9 && !asv && w.p199) {
    d.krok = "→ Pasmo centralnych wysokie. Obserwuj kolejne noce.";
    d.informacja = true;
  } else if (w.wzorzecL) {
    d.krok = "→ Otwarte sprawy: strona 3 / 6.";  // numer strony kokpitu
    d.informacja = true;
  } else {
    d.krok = "→ Odchylenia liczę od tego.";
  }
  if (w.zK1)  // strona 4 / 6, ◀ strona 3 (albo K2), ▶ „Co znaczą te liczby” (profil publiczny) albo „Sprzęt”
    d.stopkaLewo = std::string(w.sprawy ? "◀ sprawy" : "◀ 7 nocy") +  // grupy co 3 spacje jak w „Sprzęt”
                   (w.profilPubliczny ? "   ▶ co znaczą liczby   ↻ ekran dnia" : "   ▶ sprzęt   ↻ ekran dnia");
  return d;
}

TrescEkranu trescS8(const DaneS8& d) {
  TrescEkranu t;
  t.etykieta = wersaliki(d.etykieta);
  t.tytul = d.tytul;
  t.krok = d.informacja ? "" : d.krok;  // w tonie „informacja” krok w ramce rysuje rysujS8()
  if (!d.stopkaLewo.empty()) t.stopkaLewo = d.stopkaLewo;
  else t.stopkaStandardowa = true;
  return t;
}

void rysujS8(Plotno& p, const Naglowek& n, const DaneS8& d, const TrescEkranu& t) {
  rysujEkranDanych(p, n, t);
  int h = t.linia.empty() ? 0 : (t.liniaA ? 60 : 48);
  int krok = d.wiersze.size() > 3 && h ? 42 : 46;
  for (size_t i = 0; i < d.wiersze.size(); i++) tekst(p, F_P34B, X, 196 + h + (int)i * krok, d.wiersze[i]);
  if (d.informacja) {  // ton „informacja” (5.4.3, kanwa S8-norma-informacja): krok B5 w ramce 2 px
    p.ramka(16, 378, 768, 48, 2);
    tekst(p, F_P28B, X, 412, d.krok);
  }
}

// ---------- „Co znaczą te liczby” ----------
DaneLiczb budujLiczby(reguly::Tryb tryb, bool centralne) {
  DaneLiczb d;
  d.wiersze.push_back({"Bezdechy na godzinę (AHI):", " przerwy i spłycenia oddechu."});
  d.wiersze.push_back({"Czas z maską:", " ile godzin aparat pracował."});
  if (tryb == reguly::Tryb::ASV || tryb == reguly::Tryb::ASVAUTO) {
    d.wiersze.push_back({"EPAP:", " ciśnienie z aparatu przy wydechu."});
    d.wiersze.push_back({"Wsparcie:", " ile aparat dokłada przy wdechu."});
  } else {
    d.wiersze.push_back({"Ciśnienie:", " siła powietrza z aparatu."});
  }
  d.wiersze.push_back({"Wyciek:", " powietrze, które ucieka obok maski."});
  d.wiersze.push_back({"Obturacyjne:", " bezdechy z zamkniętym gardłem."});
  if (centralne) d.wiersze.push_back({"Centralne:", " bezdechy bez próby wdechu."});
  return d;
}

void rysujLiczby(Plotno& p, const Naglowek& n, const DaneLiczb& d) {
  TrescEkranu t;
  t.etykieta = wersaliki(d.etykieta);
  t.krok = d.zdanie;
  t.stopkaLewo = d.stopkaLewo;
  rysujEkranDanych(p, n, t);
  int krok = d.wiersze.size() > 6 ? 38 : 44;
  for (size_t i = 0; i < d.wiersze.size(); i++) {
    int y = 133 + (int)i * krok;
    int x = tekst(p, F_P24B, X, y, d.wiersze[i].nazwa);
    tekst(p, F_P22B, x, y, d.wiersze[i].opis);
  }
}

// ---------- S1 ----------
TrescEkranu trescS1(const odkrycia::TrescOdkrycia& o) {
  TrescEkranu t;
  t.etykieta = wersaliki(o.etykieta);
  t.tytul = o.tytul;
  t.powod = o.powod;
  t.krok = o.krok;
  t.stopkaStandardowa = true;
  if (!o.linia && !o.wartA.empty()) t.fakty = {{o.wartA, o.etA}, {o.wartB, o.etB}};
  return t;
}

void rysujOdkrycie(Plotno& p, const Naglowek& n, const odkrycia::TrescOdkrycia& o, const TrescEkranu& t) {
  TrescEkranu bez = t;
  bez.fakty.clear();
  rysujEkranDanych(p, n, bez);
  int h = t.linia.empty() ? 0 : (t.liniaA ? 60 : 48);
  int linie = (int)std::min<size_t>(2, zawin(F_P28B, t.powod, XP - X).size());
  int gora = 159 + h + 36 * linie + 14, dol = 338;  // strefa wizualizacji między powodem a podpisem osi
  if (dol - gora < 40) gora = dol - 40;
  if (o.linia && !o.wartosci30.empty()) {  // O-03: linia 30 nocy
    double mn = 1e9, mx = -1e9;
    for (double v : o.wartosci30)
      if (!std::isnan(v)) { mn = std::min(mn, v); mx = std::max(mx, v); }
    if (mn <= mx) {
      if (mx - mn < 1e-6) { mn -= 1; mx += 1; }
      int nn = (int)o.wartosci30.size();
      auto px = [&](int i) { return X + (int)std::lround(i * (XP - X - 1) / std::max(1.0, nn - 1.0)); };
      auto py = [&](double v) { return dol - (int)std::lround((v - mn) / (mx - mn) * (dol - gora)); };
      for (int i = 1; i < nn; i++)
        if (!std::isnan(o.wartosci30[(size_t)i]) && !std::isnan(o.wartosci30[(size_t)i - 1]))
          for (int g = -1; g <= 1; g++)
            p.linia(px(i - 1), py(o.wartosci30[(size_t)i - 1]) + g, px(i), py(o.wartosci30[(size_t)i]) + g);
      if (!std::isnan(o.wartosci30.back())) p.kolo(px(nn - 1), py(o.wartosci30.back()), 4, true);
    }
  } else if (!o.wartA.empty()) {  // 2 słupki (kanwa O12-alkohol): grupa z cechą czarna, porównawcza w ramce
    double mx = std::max(std::fabs(o.a), std::fabs(o.b));
    int maxH = dol - gora;
    auto wys = [&](double v) { return mx > 0 ? std::max(6, (int)std::lround(std::fabs(v) / mx * maxH)) : 6; };
    int ha = wys(o.a), hb = wys(o.b);
    p.prostokat(X, dol - ha, 62, ha, true);
    int xb = std::max(226, X + 74 + std::max(szer(F_P22B, o.etA), szer(F_P28B, o.wartA)) + 24);
    p.ramka(xb, dol - hb, 62, hb, 3);
    tekst(p, F_P28B, X + 74, dol - 26, o.wartA);
    tekst(p, F_P22B, X + 74, dol, o.etA);
    tekst(p, F_P28B, xb + 74, dol - 26, o.wartB);
    tekst(p, F_P22B, xb + 74, dol, o.etB);
  }
  if (!o.podpisOsi.empty()) tekst(p, F_P22B, X, 366, o.podpisOsi);
}

}  // namespace podsumowanie
