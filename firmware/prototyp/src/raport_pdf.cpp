// SPDX-License-Identifier: GPL-3.0-or-later
// Składanie strony A4 PDF raportu dla lekarza (5.13.3, R-LEK-12). Bez biblioteki: PDF 1.4 z krojami
// standardowymi (Helvetica, Helvetica-Bold, Symbol), kodowanie WinAnsi z /Differences dla polskich liter, treść bez
// kompresji. Wykresy wektorowo; pasmo normy kreskowaniem (tylko czerń i biel, bez szarości).
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "raport.h"
#include "wspolne.h"

namespace raport {
namespace {

const double STR_W = 595.276, STR_H = 841.89;  // A4 w punktach
const double MARG = 36, SZER = STR_W - 2 * MARG;

// Szerokości glifów w 1/1000 em (AFM Adobe Core14) dla ASCII 32–126: Helvetica i Helvetica-Bold.
const short W_REG[95] = {278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278, 556, 556, 556,
                         556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556, 1015, 667, 667, 722, 722, 667,
                         611, 778, 722, 278, 500, 667, 556, 833, 722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667,
                         667, 611, 278, 278, 278, 469, 556, 333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500,
                         222, 833, 556, 556, 556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584};
const short W_BOLD[95] = {278, 333, 474, 556, 556, 889, 722, 238, 333, 333, 389, 584, 278, 333, 278, 278, 556, 556, 556,
                          556, 556, 556, 556, 556, 556, 556, 333, 333, 584, 584, 584, 611, 975, 722, 722, 722, 722, 667,
                          611, 778, 722, 278, 556, 722, 611, 833, 722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667,
                          667, 611, 333, 278, 333, 584, 556, 333, 556, 611, 556, 611, 556, 333, 611, 611, 278, 278, 556,
                          278, 889, 611, 611, 611, 611, 389, 556, 333, 611, 556, 778, 556, 556, 500, 389, 280, 389, 584};

// Znak poza ASCII: kod w kodowaniu kroju i szerokości (zwykły, gruby). Kody 0xC0–0xCF zajmuje /Differences.
struct Glif {
  uint32_t cp;
  unsigned char kod;
  short wr, wb;
  bool symbol;
};
const Glif GLIFY[] = {
    {0x105, 0xC0, 556, 556, false}, {0x107, 0xC1, 500, 556, false}, {0x119, 0xC2, 556, 556, false},
    {0x142, 0xC3, 222, 278, false}, {0x144, 0xC4, 556, 611, false}, {0x15B, 0xC5, 500, 556, false},
    {0x17A, 0xC6, 500, 500, false}, {0x17C, 0xC7, 500, 500, false}, {0x104, 0xC8, 667, 722, false},
    {0x106, 0xC9, 722, 722, false}, {0x118, 0xCA, 667, 667, false}, {0x141, 0xCB, 556, 611, false},
    {0x143, 0xCC, 722, 722, false}, {0x15A, 0xCD, 667, 667, false}, {0x179, 0xCE, 611, 611, false},
    {0x17B, 0xCF, 611, 611, false}, {0xF3, 0xF3, 556, 611, false},  {0xD3, 0xD3, 778, 778, false},
    {0xB7, 0xB7, 278, 278, false},  {0x2013, 0x96, 556, 556, false}, {0x2014, 0x97, 1000, 1000, false},
    {0x201E, 0x84, 333, 500, false}, {0x201D, 0x94, 333, 500, false}, {0x201C, 0x93, 333, 500, false},
    {0x2026, 0x85, 1000, 1000, false}, {0xD7, 0xD7, 584, 584, false}, {0xB0, 0xB0, 400, 400, false},
    {0xA0, 0xA0, 278, 278, false},  {0xB2, 0xB2, 333, 333, false},  {0xE9, 0xE9, 556, 556, false},
    {0x2265, 0xB3, 549, 549, true}, {0x2264, 0xA3, 549, 549, true}, {0x2192, 0xAE, 987, 987, true},
};

uint32_t nastepny(const std::string& s, size_t& i) {  // UTF-8 → punkt kodowy; błędny bajt → '?'
  unsigned char c = (unsigned char)s[i++];
  if (c < 0x80) return c;
  int ile = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
  if (!ile) return '?';
  uint32_t cp = c & (0x3F >> ile);
  for (int k = 0; k < ile && i < s.size(); k++) cp = cp << 6 | ((unsigned char)s[i++] & 0x3F);
  return cp;
}
const Glif* glif(uint32_t cp) {
  for (auto& g : GLIFY)
    if (g.cp == cp) return &g;
  return nullptr;
}
double szerokosc(const std::string& s, double pt, bool gruby) {
  double w = 0;
  for (size_t i = 0; i < s.size();) {
    uint32_t cp = nastepny(s, i);
    if (cp >= 32 && cp < 127) {
      w += (gruby ? W_BOLD : W_REG)[cp - 32];
    } else if (const Glif* g = glif(cp)) {
      w += gruby ? g->wb : g->wr;
    } else {
      w += 556;
    }
  }
  return w * pt / 1000.0;
}

std::string num(double v) {
  char b[32];
  snprintf(b, sizeof b, "%.1f", v);
  std::string s = b;
  while (s.size() > 1 && s.back() == '0') s.pop_back();
  if (s.back() == '.') s.pop_back();
  return s;
}

class Strona {
 public:
  std::string t;  // strumień treści
  double y = MARG;  // kursor od góry strony
  double pt = 8;    // krój treści
  bool przepelniona() const { return y > STR_H - MARG + 0.01; }

  double Y(double yGora) const { return STR_H - yGora; }
  void tekst(double x, double yLinii, const std::string& s, double rozm, bool gruby) {
    // odcinki jednym krojem: Helvetica (F1, F2) albo Symbol (F3); x każdego odcinka z szerokości poprzednich
    std::string odc;
    bool sym = false;
    double xx = x, wOdc = 0;
    auto wypisz = [&]() {
      if (!odc.empty())
        t += "BT /" + std::string(sym ? "F3" : gruby ? "F2" : "F1") + " " + num(rozm) + " Tf " + num(xx) + " " +
             num(Y(yLinii)) + " Td (" + odc + ") Tj ET\n";
      xx += wOdc * rozm / 1000.0;
      odc.clear();
      wOdc = 0;
    };
    for (size_t i = 0; i < s.size();) {
      uint32_t cp = nastepny(s, i);
      bool ascii = cp >= 32 && cp < 127;
      const Glif* g = ascii ? nullptr : glif(cp);
      bool s2 = g && g->symbol;
      if (s2 != sym) {
        wypisz();
        sym = s2;
      }
      unsigned char k = ascii ? (unsigned char)cp : g ? g->kod : (unsigned char)'?';
      wOdc += ascii ? (gruby ? W_BOLD : W_REG)[cp - 32] : g ? (gruby ? g->wb : g->wr) : 556;
      if (k == '(' || k == ')' || k == '\\') odc += '\\';
      if (k >= 0x80) {
        char b[8];
        snprintf(b, sizeof b, "\\%03o", k);
        odc += b;
      } else {
        odc += (char)k;
      }
    }
    wypisz();
  }
  // tekst z łamaniem wierszy na szerokość; zwraca liczbę wierszy
  int akapit(double x, double szer, const std::string& s, double rozm, bool gruby, double interlinia) {
    std::vector<std::string> wiersze;
    std::string w, slowo;
    for (size_t i = 0; i <= s.size(); i++) {
      if (i == s.size() || s[i] == ' ') {
        std::string pr = slowo;
        if (!w.empty()) { pr = w; pr += " "; pr += slowo; }
        if (!w.empty() && szerokosc(pr, rozm, gruby) > szer) {
          wiersze.push_back(w);
          w = slowo;
        } else {
          w = pr;
        }
        slowo.clear();
      } else {
        slowo += s[i];
      }
    }
    if (!w.empty()) wiersze.push_back(w);
    for (auto& l : wiersze) {
      y += interlinia;
      tekst(x, y, l, rozm, gruby);
    }
    return (int)wiersze.size();
  }
  void szerLinii(double w) { t += num(w) + " w\n"; }
  void kreska(double x0, double y0, double x1, double y1, double w = 0.5, bool przerywana = false) {
    szerLinii(w);
    if (przerywana) t += "[2 2] 0 d\n";
    t += num(x0) + " " + num(Y(y0)) + " m " + num(x1) + " " + num(Y(y1)) + " l S\n";
    if (przerywana) t += "[] 0 d\n";
  }
  void kolo(double cx, double cy, double r, bool pelne) {
    if (pelne) {  // kropka: odcinek zerowej długości z okrągłym zakończeniem (krótszy zapis niż krzywe)
      t += num(2 * r) + " w 1 J " + num(cx) + " " + num(Y(cy)) + " m " + num(cx) + " " + num(Y(cy)) + " l S 0 J\n";
      return;
    }
    const double k = 0.5523 * r;
    double Yc = Y(cy);
    szerLinii(0.4);
    t += num(cx + r) + " " + num(Yc) + " m " + num(cx + r) + " " + num(Yc + k) + " " + num(cx + k) + " " + num(Yc + r) +
         " " + num(cx) + " " + num(Yc + r) + " c " + num(cx - k) + " " + num(Yc + r) + " " + num(cx - r) + " " +
         num(Yc + k) + " " + num(cx - r) + " " + num(Yc) + " c " + num(cx - r) + " " + num(Yc - k) + " " + num(cx - k) +
         " " + num(Yc - r) + " " + num(cx) + " " + num(Yc - r) + " c " + num(cx + k) + " " + num(Yc - r) + " " +
         num(cx + r) + " " + num(Yc - k) + " " + num(cx + r) + " " + num(Yc) + " c S\n";
  }
  void krzyzyk(double cx, double cy, double r) {
    kreska(cx - r, cy - r, cx + r, cy + r, 0.5);
    kreska(cx - r, cy + r, cx + r, cy - r, 0.5);
  }
  void kreskowanie(double x, double yG, double w, double h) {  // pasmo normy: ukośne kreski w prostokącie
    t += "q " + num(x) + " " + num(Y(yG + h)) + " " + num(w) + " " + num(h) + " re W n\n";
    szerLinii(0.3);
    double d = -h;  // pętla while, nie for: licznik zmiennoprzecinkowy (clang-tidy FloatLoopCounter), te same kroki
    while (d < w) {
      t += num(x + d) + " " + num(Y(yG + h)) + " m " + num(x + d + h) + " " + num(Y(yG)) + " l\n";
      d += 3;
    }
    t += "S Q\n";
  }
};

const double INTER = 1.3;  // interlinia jako krotność kroju

void etykieta(Strona& s, const std::string& t) {  // nagłówek strefy: wersaliki, gruby, mały
  s.y += s.pt * 1.6;
  s.tekst(MARG, s.y, t, s.pt * 0.85, true);
  s.y += 2;
}
void wykres(Strona& s, const Wykres& k, double x, double w, double h, double rKropki, bool podpisyDat) {
  double yG = s.y;
  if (k.brakPomiaru) {
    s.tekst(x, yG + h / 2, "brak pomiaru", s.pt, false);
    s.y = yG + h;
    return;
  }
  float mx = 0, mn = 0;
  bool jest = false;
  auto obejmij = [&](float v) {
    if (std::isnan(v)) return;
    mx = jest ? std::max(mx, v) : v;
    mn = jest ? std::min(mn, v) : std::min(0.0f, v);
    jest = true;
  };
  for (float v : k.w) obejmij(v);
  for (float v : k.w2) obejmij(v);
  for (float v : k.linie) obejmij(v);
  obejmij(k.pasmoDo);
  if (!jest || mx <= mn) mx = mn + 1;
  mx = mx + (mx - mn) * 0.08f;
  size_t n = k.daty.size();
  auto X = [&](size_t i) { return x + 14 + (n > 1 ? (w - 16) * (double)i / (double)(n - 1) : 0); };
  auto Yv = [&](double v) { return yG + h - (v - mn) / (mx - mn) * h; };
  s.kreska(x + 12, yG, x + 12, yG + h, 0.4);
  s.kreska(x + 12, yG + h, x + w, yG + h, 0.4);
  if (!k.bezLiczb) {
    s.tekst(x, Yv(mx) + s.pt * 0.6, num(std::round(mx)), s.pt * 0.75, false);
    s.tekst(x, yG + h, "0", s.pt * 0.75, false);
  }
  if (!std::isnan(k.pasmoOd) && !std::isnan(k.pasmoDo))
    s.kreskowanie(x + 13, Yv(k.pasmoDo), w - 13, Yv(k.pasmoOd) - Yv(k.pasmoDo));
  for (float v : k.linie) {
    s.kreska(x + 12, Yv(v), x + w, Yv(v), 0.5, true);
    std::string e = num(v);
    if (!k.bezLiczb) s.tekst(x + w - szerokosc(e, s.pt * 0.7, false), Yv(v) - 1.5, e, s.pt * 0.7, false);
  }
  for (int z : k.zmiany) {
    s.kreska(X((size_t)z), yG, X((size_t)z), yG + h, 0.6);
    s.tekst(X((size_t)z) + 1.5, yG + s.pt * 0.8, "zmiana ustawień", s.pt * 0.7, false);
  }
  if (k.poczatek >= 0) {
    s.kreska(X((size_t)k.poczatek), yG, X((size_t)k.poczatek), yG + h, 0.6, true);
    s.tekst(X((size_t)k.poczatek) - 52, yG + s.pt * 1.8, "początek wzorca", s.pt * 0.7, false);
  }
  // linia wartości (przerwy przy braku), znaczniki nocy
  bool otwarta = false;
  std::string sc;
  for (size_t i = 0; i < n; i++) {
    float v = i < k.w.size() ? k.w[i] : NAN;
    if (std::isnan(v)) {
      otwarta = false;
      continue;
    }
    sc += num(X(i)) + " " + num(s.Y(Yv(v))) + (otwarta ? " l\n" : " m\n");
    otwarta = true;
  }
  if (!sc.empty()) {
    s.szerLinii(0.6);
    s.t += sc + "S\n";
  }
  for (size_t i = 0; i < n; i++) {
    float v = i < k.w.size() ? k.w[i] : NAN;
    if (!std::isnan(v)) {
      char z = i < k.znak.size() ? k.znak[i] : 'p';
      if (z == 'x')
        s.krzyzyk(X(i), Yv(v), rKropki);
      else
        s.kolo(X(i), Yv(v), rKropki, z != 'o');
    }
    float v2 = i < k.w2.size() ? k.w2[i] : NAN;
    if (!std::isnan(v2)) s.kolo(X(i), Yv(v2), rKropki * 0.45, true);
  }
  if (podpisyDat && n > 0) {
    s.tekst(x + 14, yG + h + s.pt, ddmm(k.daty.front()), s.pt * 0.75, false);
    std::string o = ddmm(k.daty.back());
    s.tekst(x + w - szerokosc(o, s.pt * 0.75, false), yG + h + s.pt, o, s.pt * 0.75, false);
  }
  s.y = yG + h + (podpisyDat ? s.pt * 1.1 : 0);
}
void tabela(Strona& s, const Tabela& t, double x, const std::vector<double>& kol, double rozm) {
  double lin = rozm * INTER + 1;
  if (!t.kolumny.empty()) {
    s.y += lin;
    double xx = x;
    for (size_t i = 0; i < t.kolumny.size() && i < kol.size(); i++) {
      s.tekst(xx, s.y, t.kolumny[i], rozm, true);
      xx += kol[i];
    }
    double szer = 0;
    for (double k : kol) szer += k;
    s.kreska(x, s.y + 2.5, x + szer, s.y + 2.5, 0.4);
  }
  for (auto& w : t.wiersze) {  // komórka dłuższa niż kolumna łamie się na wiersze
    double y0 = s.y, yMax = s.y;
    double xx = x;
    for (size_t i = 0; i < w.size() && i < kol.size(); i++) {
      s.y = y0;
      s.akapit(xx, kol[i] - 3, w[i], rozm, false, lin);
      yMax = std::max(yMax, s.y);
      xx += kol[i];
    }
    s.y = yMax;
  }
}

std::string strona(const Raport& r, double pt, bool& przepelniona) {
  Strona s;
  s.pt = pt;
  double L = pt * INTER;
  // 1
  s.y += 14;
  s.tekst(MARG, s.y, r.s1tytul, 14, true);
  s.y += L + 3;
  s.tekst(MARG, s.y, r.s1okres, pt, false);
  s.tekst(MARG + SZER - szerokosc(r.s1pacjent, pt, false), s.y, r.s1pacjent, pt, false);
  s.y += 4;
  s.kreska(MARG, s.y, MARG + SZER, s.y, 0.8);
  // 2
  etykieta(s, "DLACZEGO TEN RAPORT");
  for (size_t i = 0; i < r.s2.size(); i++) s.akapit(MARG, SZER, r.s2[i], pt, i == 0 && r.rodzaj != Rodzaj::OGOLNY, L);
  if (r.s2.empty()) s.akapit(MARG, SZER, "–", pt, false, L);
  s.y += 3;
  s.kreska(MARG, s.y, MARG + SZER, s.y, 0.5);
  // 4
  s.y += L + 2;
  s.tekst(MARG, s.y, r.s4.tytul, pt, true);
  s.tekst(MARG + SZER - szerokosc(r.s4.podpis, pt * 0.8, false), s.y, r.s4.podpis, pt * 0.8, false);
  s.y += 4;
  wykres(s, r.s4, MARG, SZER, 135, 1.4, true);
  tabela(s, r.s4tab, MARG, {120, 130, 130, 130}, pt);
  s.y += 2;
  s.akapit(MARG, SZER, r.s4legenda, pt * 0.8, false, pt * 0.8 * INTER);
  // 3 i 3b
  s.kreska(MARG, s.y + 3, MARG + SZER, s.y + 3, 0.5);
  etykieta(s, "PYTANIE DO LEKARZA");
  if (!r.s3pytanie.empty()) s.akapit(MARG, SZER, r.s3pytanie, pt * 1.1, true, pt * 1.1 * INTER);
  s.akapit(MARG, SZER, r.s3dopisek, pt * 0.85, false, pt * 0.85 * INTER);
  if (r.s3bJest) {
    etykieta(s, "CO NAJPEWNIEJ DO POPRAWY");
    for (auto& h : r.s3b) s.akapit(MARG, SZER, h, pt, false, L);
    s.akapit(MARG, SZER, r.s3bDopisek, pt * 0.85, false, pt * 0.85 * INTER);
  }
  // 5
  s.kreska(MARG, s.y + 3, MARG + SZER, s.y + 3, 0.5);
  etykieta(s, "KONTEKST, TE SAME 90 NOCY");
  {
    size_t n = r.s5.size();
    double odstep = 10, w = n ? (SZER - odstep * (double)(n - 1)) / (double)n : SZER;
    double y0 = s.y, yMax = y0;
    for (size_t i = 0; i < n; i++) {
      s.y = y0;
      double x = MARG + (double)i * (w + odstep);
      s.y += pt;
      s.tekst(x, s.y, r.s5[i].tytul, pt * 0.85, true);
      s.y += 3;
      wykres(s, r.s5[i], x, w, 62, 0.8, false);
      s.akapit(x, w, r.s5[i].podpis, pt * 0.7, false, pt * 0.7 * INTER);
      yMax = std::max(yMax, s.y);
    }
    s.y = yMax + 2;
    std::string z;
    for (size_t i = 0; i < r.s5zdania.size(); i++) z += (i ? " · " : "") + r.s5zdania[i];
    s.akapit(MARG, SZER, z, pt, false, L);
  }
  // 6 i 8 w lewej kolumnie, 7 w prawej (układ artboardu Raport-A4-L01)
  s.kreska(MARG, s.y + 3, MARG + SZER, s.y + 3, 0.5);
  double yKol = s.y, kolW = SZER * 0.45 - 8, xP = MARG + SZER * 0.45 + 8, kolP = SZER * 0.55 - 8;
  etykieta(s, "USTAWIENIA TERAZ");
  for (auto& u : r.s6) s.akapit(MARG, kolW, u, pt, false, L);
  tabela(s, r.s6zmiany, MARG, {40, kolW - 40}, pt * 0.9);
  etykieta(s, "JAKOŚĆ DANYCH");
  for (auto& q : r.s8) s.akapit(MARG, kolW, q, pt * 0.9, false, pt * 0.9 * INTER);
  double yL = s.y;
  s.y = yKol;
  s.y += pt * 1.6;
  s.tekst(xP, s.y, "REGULARNOŚĆ I PODSUMOWANIE", pt * 0.85, true);
  s.y += 2;
  tabela(s, r.s7, xP, {110, (kolP - 110) / 2, (kolP - 110) / 2}, pt * 0.9);
  s.akapit(xP, kolP, r.s7kryterium, pt * 0.9, true, pt * 0.9 * INTER);
  s.akapit(xP, kolP, r.s7dopisek, pt * 0.8, false, pt * 0.8 * INTER);
  s.y = std::max(s.y, yL);
  // 9
  s.kreska(MARG, s.y + 3, MARG + SZER, s.y + 3, 0.5);
  etykieta(s, "CZEGO TE DANE NIE MÓWIĄ");
  std::string z9;
  for (size_t i = 0; i < r.s9.size(); i++) z9 += (i ? " " : "") + r.s9[i];
  s.akapit(MARG, SZER, z9, pt * 0.85, false, pt * 0.85 * INTER);
  // 10
  s.y += 4;
  s.kreska(MARG, s.y, MARG + SZER, s.y, 0.5);
  for (auto& l : r.s10) s.akapit(MARG, SZER, l, pt * 0.8, false, pt * 0.8 * INTER);
  przepelniona = s.przepelniona();
  return s.t;
}

}  // namespace

std::string pdf(const Raport& r, std::string& blad) {
  blad.clear();
  bool przep = true;
  std::string tresc;
  for (double pt : {8.6, 8.0, 7.4, 6.8}) {  // mniejszy krój, gdy strona się nie mieści
    tresc = strona(r, pt, przep);
    if (!przep) break;
  }
  if (przep) {
    blad = KOD_STRONA;
    return "";
  }
  std::vector<std::string> ob;
  ob.push_back("<< /Type /Catalog /Pages 2 0 R >>");
  ob.push_back("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  ob.push_back("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + num(STR_W) + " " + num(STR_H) +
               "] /Resources << /Font << /F1 5 0 R /F2 6 0 R /F3 7 0 R >> >> /Contents 4 0 R >>");
  ob.push_back("<< /Length " + std::to_string(tresc.size()) + " >>\nstream\n" + tresc + "endstream");
  const char* kodowanie =
      "/Encoding << /Type /Encoding /BaseEncoding /WinAnsiEncoding /Differences [192 /aogonek /cacute /eogonek "
      "/lslash /nacute /sacute /zacute /zdotaccent /Aogonek /Cacute /Eogonek /Lslash /Nacute /Sacute /Zacute "
      "/Zdotaccent] >>";
  ob.push_back(std::string("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica ") + kodowanie + " >>");
  ob.push_back(std::string("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold ") + kodowanie + " >>");
  ob.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Symbol >>");
  ob.push_back("<< /Title (Raport dla lekarza) /Producer (cpap-eink) >>");
  std::string p = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
  std::vector<size_t> off;
  for (size_t i = 0; i < ob.size(); i++) {
    off.push_back(p.size());
    p += std::to_string(i + 1) + " 0 obj\n" + ob[i] + "\nendobj\n";
  }
  size_t xref = p.size();
  p += "xref\n0 " + std::to_string(ob.size() + 1) + "\n0000000000 65535 f \n";
  for (size_t o : off) {
    char b[24];
    snprintf(b, sizeof b, "%010lu 00000 n \n", (unsigned long)o);
    p += b;
  }
  p += "trailer\n<< /Size " + std::to_string(ob.size() + 1) + " /Root 1 0 R /Info " + std::to_string(ob.size()) +
       " 0 R >>\nstartxref\n" + std::to_string(xref) + "\n%%EOF\n";
  if (p.size() > PDF_MAKS) {
    blad = KOD_ROZMIAR;
    return "";
  }
  return p;
}

}  // namespace raport
