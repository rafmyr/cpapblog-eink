// SPDX-License-Identifier: GPL-3.0-or-later
#include "sypialnia_ekran.h"
#include <algorithm>
#include <cmath>
#include "nawigacja.h"
#include "wspolne.h"

namespace {
const int X = 28, XP = 772;
const char* const NAZWY[3] = {"TEMPERATURA", "WILGOTNOŚĆ", "CO2"};
const char* const JEDNOSTKI[3] = {"°C", "%", "ppm"};

float wartoscNocy(const NocSypialni& n, int k) { return k == 0 ? n.t : k == 1 ? n.w : n.co2; }
std::string tekstWartosci(int k, double v) { return k == 0 ? przecinek1(v) : calkowita(v); }

// linia z grubością (kwadraty g × g wzdłuż odcinka, Bresenham): pełna, bez szarości
void liniaGruba(Plotno& p, int x0, int y0, int x1, int y1, int g) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1,
      e = dx + dy;
  for (;;) {
    p.prostokat(x0 - g / 2, y0 - g / 2, g, g, true);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 >= dy) {
      e += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      e += dx;
      y0 += sy;
    }
  }
}

void przerywana2(Plotno& p, int x0, int x1, int y) {
  for (int x = x0; x < x1; x += 2 * SYP30_PASMO_KRESKA)
    p.prostokat(x, y - SYP30_PASMO_GRUBOSC / 2, std::min(SYP30_PASMO_KRESKA, x1 - x), SYP30_PASMO_GRUBOSC, true);
}

void etykietaSyp(Plotno& p, int x, int y, const std::string& t) { tekst(p, F_P22B, x, y, t, true, 2); }

// pełny trójkąt „poza skalą”: wierzchołek (x, y), podstawa SYP30_STRZALKA_W px o SYP30_STRZALKA_H px dalej w stronę
// `kier` (+1: strzałka w górę, podstawa niżej; −1: w dół)
void strzalkaSyp(Plotno& p, int x, int y, int kier) {
  for (int k = 0; k < SYP30_STRZALKA_H; k++) {
    int pol = k * (SYP30_STRZALKA_W / 2) / (SYP30_STRZALKA_H - 1);
    p.prostokat(x - pol, y + kier * k, 2 * pol + 1, 1, true);
  }
}

int goraPola(int r) { return SYP30_RZAD_Y0 + r * SYP30_RZAD_H + SYP30_WYKRES_GORA; }
}  // namespace

const char* tekstSlowaSyp30(SlowoSyp30 s) {
  switch (s) {
    case SlowoSyp30::BEZ_DANYCH: return "";
    case SlowoSyp30::BRAK_OSTATNIEJ: return "brak odczytu";
    case SlowoSyp30::UCZE_SIE: return "uczę się";
    case SlowoSyp30::JAK_ZWYKLE: return "jak zwykle";
    case SlowoSyp30::WYZEJ: return "wyżej niż zwykle";
    case SlowoSyp30::NIZEJ: return "niżej niż zwykle";
    case SlowoSyp30::NOC_TRWA: return "noc trwa";
  }
  return "";
}

WykresSyp30::WykresSyp30() {
  for (auto& x : v) x = NAN;
  for (auto& x : sr) x = NAN;
}
bool WykresSyp30::jestNoc(int i) const { return i >= 0 && i < SYP30_NOCY && !std::isnan(v[i]); }
bool WykresSyp30::cokolwiek() const {
  for (int i = 0; i < SYP30_NOCY; i++)
    if (jestNoc(i)) return true;
  return false;
}

bool stronaSypialniJest(const std::vector<NocSypialni>& s, const std::string& dzis) {
  if (!dataOk(dzis)) return false;
  const std::string od = dodajDni(dzis, -(SYP30_NOCY - 1));
  for (auto& n : s)
    if (n.data >= od && n.data <= dzis && dataOk(n.data) &&
        (std::isfinite(n.t) || std::isfinite(n.w) || std::isfinite(n.co2)))  // `inf` to nie odczyt
      return true;
  return false;
}

DaneSyp30 budujSyp30(const std::vector<NocSypialni>& s, const std::string& dzis,
                     const std::set<std::string>& wykluczone, bool nocTrwa) {
  DaneSyp30 d;
  for (int k = 0; k < 3; k++) {
    d.w[k].nazwa = NAZWY[k];
    d.w[k].jednostka = JEDNOSTKI[k];
    d.w[k].wartosc = "–";
  }
  if (!dataOk(dzis)) return d;
  const std::string od = dodajDni(dzis, -(SYP30_NOCY - 1));
  d.zakres = ddmm(od) + "–" + ddmm(dzis);
  // okno średniej sięga 6 nocy przed okno wykresu; jeden wiersz na datę: pierwszy w pliku, jak S2c
  const int PRZED = SYP30_SREDNIA_NOCY - 1, N = SYP30_NOCY + PRZED;
  const std::string odSr = dodajDni(dzis, -(N - 1));
  float wart[3][SYP30_NOCY + SYP30_SREDNIA_NOCY - 1];
  bool zajete[SYP30_NOCY + SYP30_SREDNIA_NOCY - 1] = {};
  for (auto& x : wart)
    for (auto& y : x) y = NAN;
  for (auto& n : s) {
    if (!dataOk(n.data) || n.data < odSr || n.data > dzis) continue;
    int i = N - 1 - dniMiedzy(n.data, dzis);
    if (i < 0 || i >= N || zajete[i]) continue;
    zajete[i] = true;
    for (int k = 0; k < 3; k++) {
      float v = wartoscNocy(n, k);
      if (std::isfinite(v)) wart[k][i] = v;
    }
  }
  for (int k = 0; k < 3; k++)
    for (int i = 0; i < SYP30_NOCY; i++) {
      d.w[k].v[i] = wart[k][i + PRZED];
      if (std::isnan(d.w[k].v[i])) continue;
      double suma = 0;
      int ile = 0;
      for (int j = i; j <= i + PRZED; j++)  // noc i + PRZED w tablicy wart = noc i okna i 6 poprzednich
        if (!std::isnan(wart[k][j])) {
          suma += wart[k][j];
          ile++;
        }
      d.w[k].sr[i] = (float)(suma / ile);
    }
  NormaSypialni nm = liczNormeSypialni(s, dzis, wykluczone);
  const NormaMetryki* norma[3] = {&nm.t, &nm.w, &nm.co2};
  for (int k = 0; k < 3; k++) {
    WykresSyp30& w = d.w[k];
    if (norma[k]->jest()) {
      w.pasmo = true;
      w.p10 = norma[k]->p10;
      w.p90 = norma[k]->p90;
    }
    if (w.jestOstatnia()) w.wartosc = tekstWartosci(k, w.v[SYP30_NOCY - 1]);
    if (!w.jestOstatnia() && nocTrwa) w.slowo = SlowoSyp30::NOC_TRWA;  // przed 06:00
    else if (!w.cokolwiek()) w.slowo = SlowoSyp30::BEZ_DANYCH;
    else if (!w.jestOstatnia()) w.slowo = SlowoSyp30::BRAK_OSTATNIEJ;
    else if (!w.pasmo) w.slowo = SlowoSyp30::UCZE_SIE;
    else {
      // ta sama reguła co słowo S2c (src/sypialnia.h slowoSypialni()): P10 ≤ v ≤ P90 `jak zwykle`, żeby obie strony
      // nie mówiły o tej samej nocy różnie
      double v = w.v[SYP30_NOCY - 1];
      w.slowo = v > w.p90 + 1e-6 ? SlowoSyp30::WYZEJ : v < w.p10 - 1e-6 ? SlowoSyp30::NIZEJ : SlowoSyp30::JAK_ZWYKLE;
    }
  }
  return d;
}

std::vector<std::string> stopkaSyp30(bool sprawy, bool liczby, const std::string& wstecz) {
  // ◀ strona przed sypialnią (R-KOK-29): w układzie domyślnym „Otwarte sprawy” (ostatnia karta), „Co znaczą te
  // liczby” albo K2; ▶ U-01; przy innej kolejności (R-KOK-34) sąsiedzi z nawigacji (podpisCelu(), R-KOK-35)
  const KontekstNaw k = kontekstStopki(sprawy, liczby, true);
  return {wstecz.empty() ? "◀ " + podpisCelu(Ekran::SYPIALNIA, false, k) : wstecz,
          "▶ " + podpisCelu(Ekran::SYPIALNIA, true, k), "↻ ekran dnia"};
}

Belka belkaSyp30(const DaneSyp30& d) {
  Belka b;
  b.nazwa = nazwaStronyKokpitu("Sypialnia", numerStronyTekst(Ekran::SYPIALNIA));
  b.srodek = d.zakres.empty() ? "30 nocy" : "30 nocy · " + d.zakres;
  b.ustepuje = {"30 nocy"};
  return b;
}

int xNocySyp30(int i) {
  const int x0 = SYP30_X_WYKRESU + SYP30_MARGINES, x1 = SYP30_X_WYKRESU + SYP30_W_WYKRESU - 1 - SYP30_MARGINES;
  return x0 + (int)std::lround(i * (double)(x1 - x0) / (SYP30_NOCY - 1));
}

void zakresOsiSyp30(const WykresSyp30& w, int r, double& mn, double& mx) {
  if (r < 0 || r > 2) r = 0;
  if (w.pasmo) {  // decyzja 9a: pasmo w środkowej trzeciej części
    double sz = w.p90 - w.p10;
    mn = w.p10 - sz;
    mx = w.p90 + sz;
  } else {
    mn = 1e18;
    mx = -1e18;
    for (int i = 0; i < SYP30_NOCY; i++)
      if (w.jestNoc(i)) {
        mn = std::min(mn, (double)w.v[i]);
        mx = std::max(mx, (double)w.v[i]);
      }
    if (mn > mx) mn = mx = 0;
  }
  double brak = SYP30_ZAKRES_MIN[r] - (mx - mn);
  if (brak > 0) {
    mn -= brak / 2;
    mx += brak / 2;
  }
}

int pozaOsiaSyp30(const WykresSyp30& w, int r, double v) {
  double mn, mx;
  zakresOsiSyp30(w, r, mn, mx);
  return v > mx + 1e-9 ? 1 : v < mn - 1e-9 ? -1 : 0;
}

int yWartosciSyp30(const WykresSyp30& w, int r, double v) {
  double mn, mx;
  zakresOsiSyp30(w, r, mn, mx);
  if (r < 0 || r > 2) r = 0;
  const int gora = goraPola(r) + SYP30_MARGINES;
  const int dol = goraPola(r) + SYP30_WYKRES_H - 1 - SYP30_MARGINES;
  double f = mx - mn < 1e-9 ? 0.5 : (v - mn) / (mx - mn);
  f = std::max(0.0, std::min(1.0, f));  // poza osią: przycięte do krawędzi (decyzja 9a)
  return dol - (int)std::lround(f * (dol - gora));
}

void rysujSyp30(Plotno& p, const DaneSyp30& d) {
  p.prostokat(0, 0, 800, 480, false);
  belka(p, belkaSyp30(d));
  bool jestPasmo = false, jestKropka = false, jestLinia = false;
  for (int r = 0; r < 3; r++) {
    const WykresSyp30& w = d.w[r];
    const int y0 = SYP30_RZAD_Y0 + r * SYP30_RZAD_H;
    if (r > 0) p.prostokat(X, y0 - 2, XP - X, 2, true);  // kreska między rzędami (2 px)
    // lewa kolumna: nazwa wersalikami nad wartością ostatniej nocy z jednostką
    etykietaSyp(p, X, y0 + 34, w.nazwa);
    if (w.slowo == SlowoSyp30::NOC_TRWA) {  // przed 06:00: noc się nie zamknęła, nie brak danych
      tekst(p, F_P22B, X, y0 + 74, tekstSlowaSyp30(w.slowo));
    } else {
      int xw = tekst(p, F_P34B, X, y0 + 80, w.wartosc);
      if (w.jestOstatnia()) tekst(p, F_P22B, xw + 6, y0 + 80, w.jednostka);
    }
    const int gy = y0 + SYP30_WYKRES_GORA, gx = SYP30_X_WYKRESU;
    if (!w.cokolwiek()) {  // R-KOK-31: żadnego odczytu w 30 nocach: pole wykresu z napisem, nie puste
      tekstSrodek(p, F_P22B, gx + SYP30_W_WYKRESU / 2, gy + SYP30_WYKRES_H / 2 + 8, "brak odczytu");
      continue;
    }
    if (w.pasmo) {  // dwie przerywane linie zwykłego pasma (R-KOK-32)
      przerywana2(p, gx, gx + SYP30_W_WYKRESU, yWartosciSyp30(w, r, w.p10));
      przerywana2(p, gx, gx + SYP30_W_WYKRESU, yWartosciSyp30(w, r, w.p90));
      jestPasmo = true;
    }
    // linia: średnia krocząca z 7 nocy (decyzja 9b); punkty tylko w nocach z odczytem; luka dłuższa niż
    // SYP30_LUKA_MAX nocy przerywa linię (R-KOK-33); punkt bez sąsiada: kwadrat 5 px, żeby nie zniknął
    int poprz = -1, poprzPoza = 0;
    for (int i = 0; i < SYP30_NOCY; i++) {
      if (std::isnan(w.sr[i])) continue;
      const int xi = xNocySyp30(i), yi = yWartosciSyp30(w, r, w.sr[i]), poza = pozaOsiaSyp30(w, r, w.sr[i]);
      bool laczy = poprz >= 0 && i - poprz - 1 <= SYP30_LUKA_MAX;
      if (laczy) {
        liniaGruba(p, xNocySyp30(poprz), yWartosciSyp30(w, r, w.sr[poprz]), xi, yi, SYP30_LINIA_GRUBOSC);
        jestLinia = true;
      }
      int nast = i + 1;
      while (nast < SYP30_NOCY && std::isnan(w.sr[nast])) nast++;
      bool laczyDalej = nast < SYP30_NOCY && nast - i - 1 <= SYP30_LUKA_MAX;
      if (!laczy && !laczyDalej) {
        p.prostokat(xi - 2, yi - 2, 5, 5, true);
        jestLinia = true;
      }
      // poza osią: strzałka przy krawędzi pola na początku każdego odcinka poza skalą (decyzja 9a)
      if (poza != 0 && !(laczy && poprzPoza == poza))
        strzalkaSyp(p, xi, poza > 0 ? gy : gy + SYP30_WYKRES_H - 1, poza);
      poprz = i;
      poprzPoza = poza;
    }
    if (w.jestOstatnia()) {  // kropka: prawdziwa wartość ostatniej nocy (może leżeć poza linią średniej)
      const double vo = w.v[SYP30_NOCY - 1];
      const int xo = xNocySyp30(SYP30_NOCY - 1), yo = yWartosciSyp30(w, r, vo), poza = pozaOsiaSyp30(w, r, vo);
      p.kolo(xo, yo, SYP30_KROPKA_R, true);
      if (poza != 0)  // poza skalą: strzałka tuż obok kropki, w stronę wartości
        strzalkaSyp(p, xo + SYP30_KROPKA_R + 3 + SYP30_STRZALKA_W / 2, poza > 0 ? yo - 2 : yo + 2, poza);
      jestKropka = true;
    }
    if (w.slowo != SlowoSyp30::NOC_TRWA)
    tekstPrawo(p, F_P22B, XP, y0 + 64, tekstSlowaSyp30(w.slowo));
  }
  // legenda jednej linii, mówi wprost, co jest czym (decyzja 9c); tylko elementy, które są na ekranie
  int x = X;
  if (jestKropka) {
    p.kolo(x + SYP30_KROPKA_R, SYP30_LEGENDA_BAZA - 8, SYP30_KROPKA_R, true);
    x = tekst(p, F_P22B, x + 2 * SYP30_KROPKA_R + 10, SYP30_LEGENDA_BAZA, "ostatnia noc") + 26;
  }
  if (jestLinia) {
    p.prostokat(x, SYP30_LEGENDA_BAZA - 8 - SYP30_LINIA_GRUBOSC / 2, 30, SYP30_LINIA_GRUBOSC, true);
    x = tekst(p, F_P22B, x + 40, SYP30_LEGENDA_BAZA, "średnia z 7 nocy") + 26;
  }
  if (jestPasmo) {
    przerywana2(p, x, x + 30, SYP30_LEGENDA_BAZA - 8);
    tekst(p, F_P22B, x + 40, SYP30_LEGENDA_BAZA, "zwykłe pasmo");
  }
  stopkaEkranu(p, stopkaSyp30(d.sprawy, d.liczby, d.stopkaWstecz), "");
}
