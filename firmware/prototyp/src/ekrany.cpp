// SPDX-License-Identifier: GPL-3.0-or-later
#include "ekrany.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "czynniki.h"
#include "fonty.h"
#include "nawigacja.h"
#include "obrazy.h"
#include "wspolne.h"

const GFXfont* const F_S140 = &S140;
const GFXfont* const F_S120 = &S120;
const GFXfont* const F_S110 = &S110;
const GFXfont* const F_SN96 = &SN96;
const GFXfont* const F_SN56 = &SN56;
const GFXfont* const F_P34B = &P34B;
const GFXfont* const F_P28B = &P28B;
const GFXfont* const F_P24B = &P24B;
const GFXfont* const F_P24S = &P24S;
const GFXfont* const F_SB44 = &SB44;
const GFXfont* const F_SB24 = &SB24;
const GFXfont* const F_P22B = &P22B;
const GFXfont* const F_P20 = &P20;
const GFXfont* const F_P20B = &P20B;
const GFXfont* const F_P16 = &P16;
const GFXfont* const F_P18B = &P18B;

// ---------- prymitywy ----------
void Plotno::prostokat(int x, int y, int w, int h, bool c) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) piksel(xx, yy, c);
}

void Plotno::ramka(int x, int y, int w, int h, int g, bool c) {
  prostokat(x, y, w, g, c);
  prostokat(x, y + h - g, w, g, c);
  prostokat(x, y, g, h, c);
  prostokat(x + w - g, y, g, h, c);
}

void Plotno::linia(int x0, int y0, int x1, int y1, bool c) {
  // bez abs(): w części rdzeni Arduino abs to makro
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1,
      e = dx + dy;
  for (;;) {
    piksel(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 >= dy) { e += dy; x0 += sx; }
    if (e2 <= dx) { e += dx; y0 += sy; }
  }
}

void Plotno::kolo(int cx, int cy, int r, bool wyp, bool c) {
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++) {
      int d = x * x + y * y;
      if (d <= r * r + r && (wyp || d >= (r - 1) * (r - 1) + (r - 1))) piksel(cx + x, cy + y, c);
    }
}

void Plotno::trojkat(int x0, int y0, int x1, int y1, int x2, int y2, bool c) {
  int minx = std::min({x0, x1, x2}), maxx = std::max({x0, x1, x2});
  int miny = std::min({y0, y1, y2}), maxy = std::max({y0, y1, y2});
  auto kr = [](int ax, int ay, int bx, int by, int px, int py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); };
  for (int y = miny; y <= maxy; y++)
    for (int x = minx; x <= maxx; x++) {
      int a = kr(x0, y0, x1, y1, x, y), b = kr(x1, y1, x2, y2, x, y), d = kr(x2, y2, x0, y0, x, y);
      if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) piksel(x, y, c);
    }
}

void Plotno::bitmapa(const uint8_t* b, int x, int y, int w, int h) {
  int bw = (w + 7) / 8;
  for (int yy = 0; yy < h; yy++)
    for (int xx = 0; xx < w; xx++)
      if (b[yy * bw + xx / 8] & (0x80 >> (xx & 7))) piksel(x + xx, y + yy, true);
}

// ---------- tekst ----------
namespace {

enum : std::int8_t { IK_LEWO = -1, IK_PRAWO = -2, IK_ODSWIEZ = -3, IK_ROMB = -4, IK_OBIE = -5, IK_GORA = -6, IK_DOL = -7 };

uint32_t utf8(const char*& s) {
  uint8_t c = (uint8_t)*s++;
  if (c < 0x80) return c;
  int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : 1;
  uint32_t cp = c & (0x3F >> n);
  while (n-- && *s) cp = (cp << 6) | ((uint8_t)*s++ & 0x3F);
  return cp;
}

uint32_t extraCp[64];
int extraN = -1;

int kod(uint32_t cp) {  // kod glifu w kroju albo ikona (ujemne)
  if (extraN < 0) {
    extraN = 0;
    const char* s = FONT_EXTRA;
    while (*s && extraN < 64) extraCp[extraN++] = utf8(s);
  }
  if (cp == 0xA0) return ' ';
  if (cp >= 0x20 && cp < 0x7F) return (int)cp;
  if (cp == 0x25C0) return IK_LEWO;
  if (cp == 0x25B6) return IK_PRAWO;
  if (cp == 0x21BB) return IK_ODSWIEZ;
  if (cp == 0x25C6) return IK_ROMB;
  if (cp == 0x2194) return IK_OBIE;  // ↔ ↑ ↓: strzałki rysowane prymitywami (SPEC-B 1: z kroju wychodziły cienkie)
  if (cp == 0x2191) return IK_GORA;
  if (cp == 0x2193) return IK_DOL;
  for (int i = 0; i < extraN; i++)
    if (extraCp[i] == cp) return 0x7F + i;
  return '?';
}

const GFXglyph* glif(const GFXfont* f, int c) {
  if (c < f->first || c > f->last) return nullptr;
  return &f->glyph[c - f->first];
}

int wersalik(const GFXfont* f) {  // wysokość wielkiej litery
  const GFXglyph* g = glif(f, 'H');
  return g && g->height ? g->height : 12;
}

// Rozmiar kroju w px (do strzałek: wysokość = round(px × 0,70), grubość = px / 11, jak makiety skali B).
int pxKroju(const GFXfont* f) {
  struct { const GFXfont* f; int px; } t[] = {{&P22B, 22}, {&P24B, 24}, {&P24S, 24}, {&P28B, 28}, {&P34B, 34}, {&SB24, 24}, {&SB44, 44}};
  for (auto& e : t)
    if (e.f == f) return e.px;
  return (int)std::lround(wersalik(f) / 0.70);
}

// Ikona przycisku: kwadrat o boku max(wersalik + dod, min), po nim odstęp adv. Ekrany dnia: wersalik + 6, odstęp 6
// (makiety SPEC-B); stopki z akcjami: wersalik + 8, min. 24 px, odstęp 7 (SPEC-KOKPIT 2).
struct StylIkony { int dod = 6, min = 0, adv = 6; };
StylIkony styl;
struct StylStopki {  // na czas rysowania stopki
  StylIkony stary;
  StylStopki() : stary(styl) { styl = StylIkony{8, 24, 7}; }
  ~StylStopki() { styl = stary; }
};

int bokIkony(const GFXfont* f) { return std::max(wersalik(f) + styl.dod, styl.min); }

int capStrzalki(const GFXfont* f) { return (int)std::lround(pxKroju(f) * 0.70); }

int szerIkony(const GFXfont* f, int ik) {
  if (ik == IK_ROMB) return wersalik(f) + 6;
  int c = capStrzalki(f);
  if (ik == IK_OBIE) return (int)std::lround(c * 1.25) + 4;
  if (ik == IK_GORA || ik == IK_DOL) return (int)std::lround(c * 0.75) + 4;
  return bokIkony(f) + styl.adv;
}

void ikona(Plotno& p, const GFXfont* f, int x, int y, int ik, bool cz) {
  int cap = wersalik(f);
  if (ik == IK_ROMB) {
    int d = cap / 2, cx = x + d + 1, cy = y - cap / 2;
    p.trojkat(cx - d, cy, cx + d, cy, cx, cy - d, cz);
    p.trojkat(cx - d, cy, cx + d, cy, cx, cy + d, cz);
    return;
  }
  if (ik == IK_OBIE || ik == IK_GORA || ik == IK_DOL) {  // strzałki skali B (SPEC-B 1)
    int c = capStrzalki(f), g = std::max(2, (int)std::lround(pxKroju(f) / 11.0));
    if (ik == IK_OBIE) {
      int w = (int)std::lround(c * 1.25), h = (int)std::lround(c * 0.36), sr = y - (int)std::lround(c / 2.0);
      p.prostokat(x + h, sr - g / 2, w - 2 * h, g, cz);
      p.trojkat(x, sr, x + h + 1, sr - h, x + h + 1, sr + h, cz);
      p.trojkat(x + w, sr, x + w - h - 1, sr - h, x + w - h - 1, sr + h, cz);
    } else {
      int w = (int)std::lround(c * 0.75), h = (int)std::lround(w / 2.0), cx = x + h, top = y - c;
      p.prostokat(cx - g / 2, top + (ik == IK_GORA ? h : 0), g, c - h, cz);
      if (ik == IK_GORA) p.trojkat(cx, top, cx - h, top + h + 1, cx + h, top + h + 1, cz);
      else p.trojkat(cx, y, cx - h, y - h - 1, cx + h, y - h - 1, cz);
    }
    return;
  }
  int s = bokIkony(f), top = (int)std::lround(y - cap / 2.0 - s / 2.0);
  p.prostokat(x, top, s, s, cz);
  bool bl = !cz;
  double m = top + s / 2.0;
  auto X_ = [&](double v) { return (int)std::lround(x + s * v); };
  auto Y_ = [&](double v) { return (int)std::lround(top + s * v); };
  if (ik == IK_LEWO) p.trojkat(X_(0.28), (int)std::lround(m), X_(0.72), Y_(0.22), X_(0.72), Y_(0.78), bl);
  if (ik == IK_PRAWO) p.trojkat(X_(0.72), (int)std::lround(m), X_(0.28), Y_(0.22), X_(0.28), Y_(0.78), bl);
  if (ik == IK_ODSWIEZ) {  // łuk 2,4 px z przerwą u góry po prawej, grot w prawo (SPEC-KOKPIT 2)
    const double PI_ = 3.14159265358979;
    double r = s * 0.30, cx = x + s / 2.0, cy = m;
    for (int yy = top; yy < top + s; yy++)
      for (int xx = x; xx < x + s; xx++) {
        double dx = xx + 0.5 - cx, dy = yy + 0.5 - cy, d = std::sqrt(dx * dx + dy * dy);
        double kat = std::atan2(dy, dx) * 180 / PI_;  // w dół dodatni (jak łuk w PIL)
        if (d >= r - 1.2 && d <= r + 1.2 && !(kat > -90 && kat < -60)) p.piksel(xx, yy, bl);
      }
    double ax = cx + r * 0.5, ay = cy - r * 0.87;
    p.trojkat((int)std::lround(ax - 1.5), (int)std::lround(ay - 4), (int)std::lround(ax - 1.5), (int)std::lround(ay + 4),
              (int)std::lround(ax + 4.5), (int)std::lround(ay), bl);
  }
}

// szerokość znaku w 1/16 px (tablice *_adv16 z generuj.py); zapas: xAdvance z glifu
const uint16_t* tablicaAdv(const GFXfont* f) {
  static const GFXfont* ostatni = nullptr;
  static const uint16_t* tab = nullptr;
  if (f == ostatni) return tab;
  ostatni = f;
  tab = nullptr;
  for (int i = 0; i < KROJE_N; i++)
    if (KROJE_ADV16[i].f == f) tab = KROJE_ADV16[i].adv16;
  return tab;
}

int adv16(const GFXfont* f, int c) {
  if (c < (int)f->first || c > (int)f->last) return 0;
  const uint16_t* t = tablicaAdv(f);
  return t ? t[c - f->first] : f->glyph[c - f->first].xAdvance * 16;
}

}  // namespace

bool znakJestWKroju(uint32_t cp) {
  if (cp == 0xA0) return true;
  if (cp < 0x20 || cp == 0x7F) return false;
  return kod(cp) != '?' || cp == '?';
}

int szer(const GFXfont* f, const std::string& u, int odstep) {
  int w16 = 0;
  const char* s = u.c_str();
  while (*s) {
    int c = kod(utf8(s));
    if (c < 0) w16 += (szerIkony(f, c) + odstep) * 16;
    else if (glif(f, c)) w16 += adv16(f, c) + odstep * 16;
  }
  if (odstep && w16) w16 -= odstep * 16;  // bez odstępu po ostatnim znaku
  return (w16 + 8) >> 4;
}

int tekst(Plotno& p, const GFXfont* f, int x, int y, const std::string& u, bool cz, int odstep) {
  int x16 = x * 16;
  const char* s = u.c_str();
  while (*s) {
    int c = kod(utf8(s));
    int x0 = (x16 + 8) >> 4;
    if (c < 0) {
      ikona(p, f, x0, y, c, cz);
      x16 += (szerIkony(f, c) + odstep) * 16;
      continue;
    }
    const GFXglyph* g = glif(f, c);
    if (!g) continue;
    const uint8_t* b = f->bitmap + g->bitmapOffset;
    int bit = 0;
    uint8_t bits = 0;
    for (int yy = 0; yy < g->height; yy++)
      for (int xx = 0; xx < g->width; xx++) {
        if (!(bit++ & 7)) bits = *b++;
        if (bits & 0x80) p.piksel(x0 + g->xOffset + xx, y + g->yOffset + yy, cz);
        bits <<= 1;
      }
    x16 += adv16(f, c) + odstep * 16;
  }
  return (x16 + 8) >> 4;
}

void tekstPrawo(Plotno& p, const GFXfont* f, int xp, int y, const std::string& u, bool cz, int odstep) {
  tekst(p, f, xp - szer(f, u, odstep), y, u, cz, odstep);
}

void tekstSrodek(Plotno& p, const GFXfont* f, int xs, int y, const std::string& u, bool cz) {
  tekst(p, f, xs - szer(f, u) / 2, y, u, cz);
}

std::vector<std::string> zawin(const GFXfont* f, const std::string& u, int maxW) {
  std::vector<std::string> wiersze;
  std::string linia, slowo;
  for (size_t i = 0; i <= u.size(); i++) {
    char c = i < u.size() ? u[i] : ' ';
    if (c != ' ') { slowo += c; continue; }
    if (slowo.empty()) continue;
    std::string proba = slowo;
    if (!linia.empty()) { proba = linia; proba += " "; proba += slowo; }
    if (szer(f, proba) > maxW && !linia.empty()) {
      wiersze.push_back(linia);
      linia = slowo;
    } else {
      linia = proba;
    }
    slowo.clear();
  }
  if (!linia.empty()) wiersze.push_back(linia);
  return wiersze;
}

// wersaliki ASCII i polskich liter (UTF-8 dwubajtowe: małe ą ć ę ł ń ó ś ź ż leżą 1 punkt kodowy za wielkimi)
std::string wersaliki(const std::string& u) {
  std::string r;
  for (size_t i = 0; i < u.size(); i++) {
    unsigned char c = (unsigned char)u[i];
    if (c >= 'a' && c <= 'z') { r += (char)(c - 32); continue; }
    if ((c == 0xC4 || c == 0xC5 || c == 0xC3) && i + 1 < u.size()) {
      unsigned char d = (unsigned char)u[i + 1];
      bool male = (c == 0xC3 && d == 0xB3) ||                                          // ó → Ó (C3 93)
                  (c == 0xC4 && (d == 0x85 || d == 0x87 || d == 0x99)) ||               // ą ć ę
                  (c == 0xC5 && (d == 0x82 || d == 0x84 || d == 0x9B || d == 0xBA || d == 0xBC));  // ł ń ś ź ż
      r += (char)c;
      r += (char)(male ? (c == 0xC3 ? 0x93 : d - 1) : d);
      i++;
      continue;
    }
    r += (char)c;
  }
  return r;
}

// ---------- elementy wspólne ----------
namespace {

const int X = 28;              // lewy margines ekranu danych i kokpitu
const int XP = 772;            // prawy margines
const int W = XP - X;          // 744 px treści
const int XK = 26;             // lewy margines treści karty i S9 (ADR-0009)
const int KOL_TEKST = 474;     // kolumna tekstu karty: 800 − 26 − 250 − 24 − 26 (ADR-0009)

// etykieta: wersaliki P22B, rozstrzelenie 2 px (SPEC-B 2, SPEC-KOKPIT 2)
void etykieta(Plotno& p, int x, int y, const std::string& u, bool cz = true) { tekst(p, F_P22B, x, y, u, cz, 2); }

void kreska(Plotno& p, int y, int g = 2, int x0 = 0, int x1 = 800) { p.prostokat(x0, y, x1 - x0, g, true); }

// nagłówek ekranu danych: 0–52, linia 2 px na 52, linia bazowa 35. Ekrany dnia: obie strony (P24B); K1 i test:
// prawa P22B (SPEC-KOKPIT 2).
void naglowekDanych(Plotno& p, const Naglowek& n, const GFXfont* prawy = F_P24B) {
  tekst(p, F_P24B, X, 35, n.lewo);
  if (!n.prawo.empty()) tekstPrawo(p, prawy, XP, 35, n.prawo);
  kreska(p, 52);
}

// nagłówek karty, S9 i pytania (ADR-0009): linia na 46, linia bazowa 32 (pytanie 33), lewo P24B, prawo P22B
void naglowekKarty(Plotno& p, const Naglowek& n, int b = 32) {
  tekst(p, F_P24B, X, b, n.lewo);
  if (!n.prawo.empty()) tekstPrawo(p, F_P22B, XP, b, n.prawo);
  kreska(p, 46);
}

// Stopka (SPEC-B 3, SPEC-KOKPIT 2.1): linia 2 px na 434, P22B na 465. lewo: pogoda albo zdanie budzika w cudzysłowie;
// grupy: akcje przycisków (z prawej, gdy jest `lewo`) albo prawa część (odczyt czujnika `przy łóżku …`).
void stopkaAkcji(Plotno& p, const std::string& lewo, std::vector<std::string> grupy) {
  uporzadkujPrzyciski(grupy);
  StylStopki st;
  kreska(p, 434);
  int w = szerStopki(grupy);
  int x = X;
  if (!lewo.empty()) {
    tekst(p, F_P22B, X, 465, lewo);
    x = XP - w;
  }
  for (auto& g : grupy) x = tekst(p, F_P22B, x, 465, g) + 26;
}

// stopka ekranu dnia: znika, gdy obie strony są puste (reguła ustępowania 0, SPEC-B 3); lewo pogoda, prawo `przy łóżku …`
// albo akcja (`↻ ponów`)
bool stopkaDnia(Plotno& p, const std::string& lewo, const std::string& prawo) {
  if (lewo.empty() && prawo.empty()) return false;
  StylStopki st;
  kreska(p, 434);
  if (!lewo.empty()) tekst(p, F_P22B, X, 465, lewo);
  if (!prawo.empty()) tekstPrawo(p, F_P22B, XP, 465, prawo);
  return true;
}

void przerywana(Plotno& p, int x0, int x1, int y, int krok = 3, bool cz = true) {
  for (int x = x0; x < x1; x += 2 * krok) p.prostokat(x, y, std::min(krok, x1 - x), 1, cz);
}

void ramkaPrzerywana(Plotno& p, int x, int y, int w, int h) {
  for (int i = 0; i < w; i += 12) {
    p.prostokat(x + i, y, std::min(6, w - i), 2, true);
    p.prostokat(x + i, y + h - 2, std::min(6, w - i), 2, true);
  }
  for (int i = 0; i < h; i += 12) {
    p.prostokat(x, y + i, 2, std::min(6, h - i), true);
    p.prostokat(x + w - 2, y + i, 2, std::min(6, h - i), true);
  }
}

// rząd faktów karty i S9 (SPEC-KOKPIT 7): liczba P28B nad podpisem P22B (+26), odstęp 22 px
void fakty(Plotno& p, int x, int y, const std::vector<Fakt>& f) {
  for (auto& e : f) {
    tekst(p, F_P28B, x, y, e.w);
    tekst(p, F_P22B, x, y + 26, e.p);
    x += std::max(szer(F_P28B, e.w), szer(F_P22B, e.p)) + 22;
  }
}

// Linia 30 nocy (K1): przerwy w nocach bez wartości (R-KOK-02), przerywana mediana normy (3 px kreska, 3 px przerwy).
void linia30(Plotno& p, int x, int y, int w, int h, const Linia30& l, int grubosc, bool kropka, bool cz = true) {
  double mn = 1e9, mx = -1e9;
  for (int i = 0; i < 30; i++)
    if (l.ma[i]) { mn = std::min(mn, (double)l.v[i]); mx = std::max(mx, (double)l.v[i]); }
  if (mn > mx) return;
  if (!std::isnan(l.medNormy)) { mn = std::min(mn, l.medNormy); mx = std::max(mx, l.medNormy); }
  if (mx - mn < 1e-6) { mn -= 1; mx += 1; }
  auto px = [&](int i) { return x + (int)std::lround(i * (w - 1) / 29.0); };
  auto py = [&](double v) { return y + h - 1 - (int)std::lround((v - mn) / (mx - mn) * (h - 1)); };
  if (!std::isnan(l.medNormy)) przerywana(p, x, x + w, py(l.medNormy), 3, cz);
  for (int i = 1; i < 30; i++)
    if (l.ma[i] && l.ma[i - 1])
      for (int g = 0; g < grubosc; g++) p.linia(px(i - 1), py(l.v[i - 1]) + g - grubosc / 2, px(i), py(l.v[i]) + g - grubosc / 2, cz);
  for (int i = 0; i < 30; i++)  // pojedyncza noc między przerwami: kropka, żeby nie zniknęła
    if (l.ma[i] && (i == 0 || !l.ma[i - 1]) && (i == 29 || !l.ma[i + 1])) p.prostokat(px(i) - 1, py(l.v[i]) - 1, 3, 3, cz);
  if (kropka && l.ma[29]) p.kolo(px(29), py(l.v[29]), 3, true, cz);
}

}  // namespace

// Kolejność podpisów przycisków w stopce jest zawsze taka jak przyciski na obudowie: `<` ◀, `>` ▶, zielony ↻.
// Dlaczego: kolejność ◀ ↻ ▶ mylił użytkownika przy łóżku, bo ręka szuka przycisku w tym
// samym rzędzie, w którym czyta podpis. Porządkuje grupy stabilnie według pierwszego znaku grupy (◀, ▶, ↻); grupa bez
// ikony zostaje przy poprzedniej. Znaczenie przycisków się nie zmienia, tylko miejsce podpisu na ekranie.
static int rzadPrzycisku(const std::string& g) {
  if (g.rfind("◀", 0) == 0) return 0;
  if (g.rfind("▶", 0) == 0) return 1;
  if (g.rfind("↻", 0) == 0) return 2;
  return -1;
}
void uporzadkujPrzyciski(std::vector<std::string>& grupy) {
  std::vector<std::pair<int, std::string>> w;
  int ostatni = 0;
  for (auto& g : grupy) {
    int r = rzadPrzycisku(g);
    if (r >= 0) ostatni = r;
    w.push_back({ostatni, g});
  }
  std::stable_sort(w.begin(), w.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (size_t i = 0; i < w.size(); i++) grupy[i] = w[i].second;
}

// (5.6.1 pkt 3): na stronach kokpitu stopka podpisuje każdy przycisk, także ◀ wstecz, ▶ dalej i ↻ ekran dnia,
// krótko nazwą strony, do której prowadzi; kolejność ◀ ▶ ↻.
std::vector<std::string> stopkaK1(bool jestNoc) {
  // ◀ ekran dnia, ▶ K2, ↻ ekran dnia, ↻ 2 s „Co było inaczej?” dla ostatniej nocy (R-CZY-08)
  return {"◀ ekran dnia", "▶ 7 nocy", jestNoc ? "↻ ekran dnia · 2 s co było inaczej" : "↻ ekran dnia"};
}
std::vector<std::string> stopkaK2(bool sprawy) {
  // ◀ K1, ▶ „Otwarte sprawy” (bez wyników silnika S8), ▶ 2 s raport ogólny (R-LEK-02, usługa z Wi-Fi), ↻ ekran dnia
  return {"◀ ostatnia noc", sprawy ? "▶ sprawy · 2 s raport" : "▶ norma · 2 s raport", "↻ ekran dnia"};
}
std::vector<std::string> stopkaPytania() {
  // bez liczby zaznaczonych w nawiasie (`zapisz (2)` czytało się jak „zapisz za 2 s”); ile jest
  // zaznaczone, widać na siatce
  return {"◀ wstecz · 2 s pomiń", "▶ dalej", "↻ zaznacz · 2 s zapisz"};
}
std::vector<std::string> stopkaTestu(bool awaryjny) {
  // ◀ 2 s sieć panelu (U-02), poza trybem awaryjnym; ◀ U-01, ▶ odśwież, ↻ ekran dnia
  if (awaryjny) return {"◀ wróć"};
  return {"◀ ustawienia · 2 s sieć panelu", "▶ odśwież", "↻ ekran dnia"};
}
int szerStopki(const std::vector<std::string>& grupy) {
  StylStopki st;
  int w = 0;
  for (auto& g : grupy) w += szer(F_P22B, g);
  return w + 26 * std::max(0, (int)grupy.size() - 1);
}

// ---------- sceny ----------
static const SceneBudzika* scena(const std::string& id) {
  for (int i = 0; i < SCENY_N; i++)
    if (id == SCENY[i].id) return &SCENY[i];
  return nullptr;
}

bool scenaJest(const std::string& id) { return scena(id) != nullptr; }

std::vector<std::string> sceny() {
  std::vector<std::string> v;
  v.reserve(SCENY_N);
  for (int i = 0; i < SCENY_N; i++) v.push_back(SCENY[i].id);
  return v;
}

// ---------- karta (ADR-0009, 5.7.2, artboard K22; kroje SPEC-KOKPIT 7) ----------
bool kartaMiesci(const TrescEkranu& t) {
  if (zawin(F_SB44, t.tytul, 748).size() > 2) return false;
  if (zawin(F_P22B, t.powodKarty.empty() ? t.powod : t.powodKarty, KOL_TEKST).size() > 2) return false;
  if (!t.krok.empty() && szer(F_P22B, t.krok) > KOL_TEKST) return false;
  return true;
}

// S2d: tytuł w 2 liniach SB44 albo, gdy pierwsze zdanie jest dłuższe, w 2 liniach P34B; powód
// do 3 linii P22B w kolumnie tekstu; bez kroku. Teksty autora nie są skracane, więc układ się dopasowuje (nie odwrotnie).
bool kartaPrzerywnikaMiesci(const TrescEkranu& t) {
  if (zawin(F_SB44, t.tytul, 748).size() > 2 && zawin(F_P34B, t.tytul, 748).size() > 2) return false;
  return zawin(F_P22B, t.powodKarty.empty() ? t.powod : t.powodKarty, KOL_TEKST).size() <= 3;
}

// wspólna anatomia karty i S9 A1: etykieta, tytuł SB44, powód P22B, fakty, krok P22B na 412
static void trescKarty(Plotno& p, const std::string& et, const std::string& tytul, const std::vector<std::string>& powod,
                       const std::vector<Fakt>& f, const std::string& krok, bool problem = false, bool bezEtykiety = false) {
  if (bezEtykiety) {  // S2d: bez etykiety (ADR-0009), tytuł 116 albo, w P34B, 112; powód 38 pod ostatnią linią tytułu
    const GFXfont* ft = F_SB44;
    auto tyt = zawin(ft, tytul, 748);
    int b0 = 116, krok_t = 48, odstep = 38;
    if (tyt.size() > 2) { ft = F_P34B; tyt = zawin(ft, tytul, 748); b0 = 112; krok_t = 42; odstep = 36; }
    int y = b0;
    for (auto& l : tyt) { tekst(p, ft, XK, y, l); y += krok_t; }
    y = b0 + ((int)tyt.size() - 1) * krok_t + odstep;
    for (auto& l : powod) { tekst(p, F_P22B, XK, y, l); y += 29; }
    fakty(p, XK, y + 44, f);
    return;
  }
  if (problem && !et.empty()) {  // ton „problem” (R-HUM-24, kanwa K09): etykieta biała w czarnym polu, 10 px wcięcia
    p.prostokat(XK, 82 - 22, szer(F_P22B, et, 2) + 20, 30, true);
    etykieta(p, XK + 10, 82, et, false);
  } else {
    etykieta(p, XK, 82, et);
  }
  auto tyt = zawin(F_SB44, tytul, 748);
  int y = 134;
  for (auto& l : tyt) { tekst(p, F_SB44, XK, y, l); y += 48; }
  y = 172 + ((int)tyt.size() - 1) * 48;
  for (auto& l : powod) { tekst(p, F_P22B, XK, y, l); y += 29; }
  fakty(p, XK, y + 44, f);
  if (!krok.empty()) tekst(p, F_P22B, XK, 412, krok);
}

void rysujKarte(Plotno& p, const Naglowek& n, const TrescEkranu& t, const std::string& sc, const std::string& zdanie) {
  p.prostokat(0, 0, 800, 480, false);
  if (t.tonProblem) {  // ton „problem” (R-HUM-24, kanwa K09): czarny pasek nagłówka z białym tekstem; tło jasne (ADR-0009)
    p.prostokat(0, 0, 800, 48, true);
    tekst(p, F_P24B, X, 32, n.lewo, false);
    if (!n.prawo.empty()) tekstPrawo(p, F_P22B, XP, 32, n.prawo, false);
  } else {
    naglowekKarty(p, n);
  }
  trescKarty(p, t.etykietaKarty.empty() ? t.etykieta : t.etykietaKarty, t.tytul,
             zawin(F_P22B, t.powodKarty.empty() ? t.powod : t.powodKarty, KOL_TEKST),
             t.fakty, t.krok, t.tonProblem, t.kartaBezEtykiety);
  const SceneBudzika* s = scena(sc);
  if (s) p.bitmapa(s->bmp, 800 - 26 - 14 - 125 - s->w / 2, 418 - s->h, s->w, s->h);  // budzik zawsze na bieli (ADR-0009)
  // zdanie budzika w cudzysłowie, bez kursywy (T16); prawa część: `przy łóżku …` albo akcja (`↻ ponów`)
  std::string lewo = zdanie.empty() ? std::string() : "„" + zdanie + "”";
  std::string prawo = t.stopkaPrawo;
  // odczyt czujnika (nie akcja) ustępuje zdaniu budzika, gdy oba nie mieszczą się w 744 px (zdanie do 45 znaków)
  bool akcja = prawo.rfind("↻", 0) == 0 || prawo.rfind("▶", 0) == 0 || prawo.rfind("◀", 0) == 0;
  // (najpierw ustępuje dopisek CO2 sypialni, potem cały odczyt)
  auto zaDlugo = [&]() { return !akcja && !prawo.empty() && !lewo.empty() && szer(F_P22B, lewo) + szerStopki({prawo}) + 24 > W; };
  if (zaDlugo() && prawo.find(" · CO2 ") != std::string::npos) prawo = prawo.substr(0, prawo.find(" · CO2 "));
  if (zaDlugo()) prawo.clear();
  stopkaAkcji(p, lewo, prawo.empty() ? std::vector<std::string>{} : std::vector<std::string>{prawo});
}

// ---------- ekran danych (5.4.3, wariant B: docs/design/czytelnosc-b/SPEC-B.md 3–5) ----------
namespace {
const int PASEK_SLOWA = 142, PASEK_CIASNY = 128, PASEK_BEZ = 114;  // wysokość paska: pełny, zagęszczony, bez słów
int wysLinii(const TrescEkranu& t) { return t.linia.empty() ? 0 : (t.liniaA ? 60 : 48); }
bool maStopke(const TrescEkranu& t) { return !t.stopkaLewo.empty() || !t.stopkaPrawo.empty(); }
int dolStrefy(const TrescEkranu& t) { return maStopke(t) ? 422 : 466; }
int bazaLiczb(const TrescEkranu& t) { return 54 + wysLinii(t) + 117; }  // podpis na top + 34, liczba B1 o 83 niżej
int dolZdania(const TrescEkranu& t) { return bazaLiczb(t) + 50 + 8; }  // zdanie B4 na liczba + 50, ogonki 8 px
bool jestSlowo(const TrescEkranu& t) {
  for (size_t i = 0; i < t.kolumny.size() && i < 4; i++)
    if (!t.kolumny[i].p.empty()) return true;
  return false;
}
}  // namespace

int wysokoscPaska(const TrescEkranu& t) {
  if (dolStrefy(t) - PASEK_SLOWA >= dolZdania(t) + 16) return PASEK_SLOWA;  // reguła 1: wolne miejsce nad paskiem
  return jestSlowo(t) ? PASEK_CIASNY : PASEK_BEZ;  // reguła 2: słowa zostają, pasek się zagęszcza
}
bool slowaWPasku(const TrescEkranu& t) { return wysokoscPaska(t) != PASEK_BEZ; }
int gornaPaska(const TrescEkranu& t) { return dolStrefy(t) - wysokoscPaska(t); }

static void ekranTydzien(Plotno& p, const TrescEkranu& t, int top) {
  // blok liczb ostatniej nocy: podpisy B6 nad liczbami B1; AHI od x = max(328, koniec czasu + 56)
  int yp = top + 34, yl = top + 117;
  tekst(p, F_P24B, X, yp, t.podpisNocy);
  int x1 = tekst(p, F_SN96, X, yl, t.czas);
  int x2 = std::max(328, x1 + 56);
  tekst(p, F_P24B, x2, yp, "AHI");
  if (t.ahiNiepewne) {  // noc niepewna (R-EKR-46): górna linia na wersaliku B1
    tekst(p, F_P34B, x2, yl - 67 + 24 + 2, "nie oceniam:");
    tekst(p, F_P34B, x2, yl, "duży wyciek");  // od 15 L/min wyciek jest duży (noc niepewna, R-NOR-05)
  } else {
    tekst(p, F_SN96, x2, yl, t.ahi);
  }
  tekst(p, F_P34B, X, yl + 50, (t.wNormie ? "Noc w normie. " : "") + t.powod);  // 5.4.2, R-EKR-41
  // pasek przyklejony do dołu strefy treści (reguła 1); rząd słów ustępuje przy braku miejsca (reguła 2)
  int hp = wysokoscPaska(t);
  bool slowa = hp != PASEK_BEZ, ciasny = hp == PASEK_CIASNY;
  int ptop = gornaPaska(t), dol = ptop + hp;
  kreska(p, ptop, 2, X, XP);
  // pełny: tytuł +30, nazwa +38, wartość +36, słowo +28 (do 142); zagęszczony: +27, +34, +33, +25 (do 128)
  int yt = ptop + (ciasny ? 27 : 30), yn = yt + (ciasny ? 34 : 38), yw = yn + (ciasny ? 33 : 36),
      ys = yw + (ciasny ? 25 : 28);
  etykieta(p, X, yt, wersaliki(t.tytulPaska));
  tekstPrawo(p, F_P22B, XP, yt, "↔ jak zwykle");
  for (size_t i = 0; i < t.kolumny.size() && i < 4; i++) {
    const Kolumna& k = t.kolumny[i];
    int kx = X + (int)i * 186;
    // poza normą: ramka (jedna legenda panelu: ramka = poza twoją normą, czarne pole = ponad próg)
    // 2 px jak w K1 i K2; 184 px od kx − 5 (słowo `wyżej niż zwykle` ma 173 px), dół 2 px pod paskiem (ogonki słów)
    if (k.poza) p.ramka(kx - 5, yn - 26, 184, dol + 2 - (yn - 26), 2);
    bool cz = !k.ponadProg;  // ponad próg: czarne pole tej samej wielkości co ramka, tekst biały
    if (k.ponadProg) p.prostokat(kx - 5, yn - 26, 184, dol + 2 - (yn - 26), true);
    tekst(p, F_P24B, kx, yn, k.et, cz);
    int x = tekst(p, F_P28B, kx, yw, k.w, cz);
    if (!k.s.empty()) tekst(p, F_P28B, x + 8, yw, k.s, cz);
    if (slowa && !k.p.empty()) tekst(p, F_P22B, kx, ys, k.p, cz);
  }
}

static void ekranSeria(Plotno& p, const TrescEkranu& t, int top, bool stopka) {
  etykieta(p, X, top + 40, "SERIA");
  int b = top + 152;
  int x = tekst(p, F_S120, X, b, std::to_string(t.seria));  // liczba nocy, nie numer porządkowy
  tekst(p, F_P34B, x + 20, b - 84 + 24, std::string(nocyZRzedu(t.seria)) + " z rzędu");  // górna linia na wersaliku
  tekst(p, F_P34B, x + 20, b, "z maską");
  int ky = std::max(b + 44, stopka ? 372 : 410);
  for (int i = 0; i < 30; i++) {
    int cx = X + 10 + (int)std::lround(i * (W - 20) / 29.0);
    p.kolo(cx, ky, 10, true);
    if (!t.kropka[i]) p.kolo(cx, ky, 7, true, false);  // pusta kropka: obwódka 3 px
  }
  tekst(p, F_P22B, X, ky + 40, "30 nocy temu");
  tekstPrawo(p, F_P22B, XP, ky + 40, "dziś");
}

// szablon A (S5, S6, kamień): etykieta, tytuł B3 albo duża liczba B1, powód (≤ 2 linie), postęp S6, fakt B7, krok B5
static void ekranOgolny(Plotno& p, const TrescEkranu& t, int h) {
  int y = 54 + h;
  if (t.ramkaPrzerywana) ramkaPrzerywana(p, 12, 62 + h, 776, 368 - h);
  int xi = t.ramkaPrzerywana ? X + 8 : X, wi = t.ramkaPrzerywana ? W - 16 : W;
  y += 40;
  etykieta(p, X, y, t.etykieta);
  if (!t.duza.empty()) {
    y += 22 + 67;
    int x = tekst(p, F_SN96, xi, y, t.duza);
    tekst(p, F_P34B, x + 20, y, t.podpisDuzej);
  } else {
    bool pierwsza = true;
    for (auto& l : zawin(F_SB44, t.tytul, wi)) {  // tytuł w 2 liniach tylko wyjątkowo (reguła 5)
      y += (pierwsza ? 20 : 12) + 31;
      tekst(p, F_SB44, xi, y, l);
      pierwsza = false;
    }
  }
  y += 14;
  auto pow = zawin(F_P28B, t.powod, wi);  // reguła 4: powód zawija się na 2 linie co 36
  if (pow.size() > 2) {  // jawne skrócenie zamiast cichego ucięcia; testy pilnują 2 linii
    pow.resize(2);
    while (!pow[1].empty() && szer(F_P28B, pow[1] + "…") > wi) {
      size_t k = pow[1].find_last_of(' ');
      pow[1] = k == std::string::npos ? std::string() : pow[1].substr(0, k);
    }
    pow[1] += "…";
  }
  for (size_t i = 0; i < pow.size(); i++) { y += 36; tekst(p, F_P28B, xi, y, pow[i]); }
  if (t.postep >= 0) {  // S6: 14 pól postępu (ramka 3 px / pełne), podpisy B6
    int n = std::max(0, std::min(t.postep, 14));
    y = 336;
    for (int i = 0; i < 14; i++) {
      int x0 = X + (int)std::lround(i * 750 / 14.0), x1 = X + (int)std::lround((i + 1) * 750 / 14.0) - 6;
      if (i < n) p.prostokat(x0, y, x1 - x0, 32, true);
      else p.ramka(x0, y, x1 - x0, 32, 3);
    }
    y += 62;
    tekst(p, F_P24B, X, y, t.postepLewo);
    tekstPrawo(p, F_P24B, XP, y, t.postepPrawo);
  }
  if (!t.fakt.empty()) { y += 34; tekst(p, F_P22B, xi, y, t.fakt); }
  if (!t.krok.empty()) tekst(p, F_P28B, xi, t.ramkaPrzerywana ? 408 : 410, t.krok);
}

void rysujEkranDanych(Plotno& p, const Naglowek& n, const TrescEkranu& t) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekDanych(p, {t.naglowekLewo.empty() ? n.lewo : t.naglowekLewo, n.prawo});
  int h = wysLinii(t);
  if (h) {  // linia uwagi (zwykła B6 na 87, wzorca A B5 na 94)
    const GFXfont* f = t.liniaA ? F_P28B : F_P24B;
    int y = t.liniaA ? 94 : 87;
    if (t.liniaA || t.liniaOstrzezenie) {  // ostrzeżenie (grupa „zdrowie”): czarny pasek, `!` i tekst biały
      p.prostokat(0, 54, 800, h, true);
      int x = tekst(p, f, X, y, "!", false);
      tekst(p, f, x + 12, y, t.linia, false);
    } else {  // informacja albo prośba: biały pasek w ramce 2 px, bez `!`
      p.ramka(12, 57, 776, h - 6, 2);
      tekst(p, f, X, y, t.linia);
    }
  }
  switch (t.uklad) {
    case Uklad::TYDZIEN: ekranTydzien(p, t, 54 + h); break;
    case Uklad::SERIA: ekranSeria(p, t, 54 + h, maStopke(t)); break;
    case Uklad::ROZRUCH:
    case Uklad::OGOLNY: ekranOgolny(p, t, h); break;
  }
  stopkaDnia(p, t.stopkaLewo, t.stopkaPrawo);
}

// ---------- K1 (5.5.2, SPEC-KOKPIT 3) ----------
void rysujK1(Plotno& p, const DaneK1& d) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekDanych(p, {d.naglowek + (d.romb ? " ◆" : ""), numerStronyTekst(Ekran::K1)}, F_P22B);  // `1 / 6`
  // R-CZY-20 (5.5.2 „Odkrycie o czynniku”): tytuł i powód O-12 pod nagłówkiem, nad 4 liczbami ostatniej nocy
  // (liczby zostają). Wygląd („dopasuj do całości”):
  // wariant to zwykły K1 z dodatkowym pasem. Pas: tytuł P24B na linii bazowej opisów liczb (82), powód P22B (≤ 2 linie
  // co 26), kreska 1 px; pod nim blok liczb i tabela 30 nocy z tymi samymi krojami i odstępami co w K1, przesunięte
  // w dół. Miejsce bierze tylko wysokość wierszy 30 nocy (z 50 px do (432 − góra) / wierszy); kroje bez zmian, nic
  // poniżej (SPEC-B). Bez odkrycia układ bez zmian.
  const bool odk = !d.odkrycieTytul.empty();
  int g = 0;  // przesunięcie bloku liczb i tabeli względem zwykłego K1 (pas odkrycia)
  if (odk) {
    tekst(p, F_P24B, X, 82, d.odkrycieTytul);
    std::vector<std::string> l = zawin(F_P22B, d.odkryciePowod, W);
    if (l.size() > 2) {  // jawne skrócenie jak w ekranOgolny (testy pilnują 2 linii)
      l.resize(2);
      while (!l[1].empty() && szer(F_P22B, l[1] + "…") > W) {
        size_t k = l[1].find_last_of(' ');
        l[1] = k == std::string::npos ? std::string() : l[1].substr(0, k);
      }
      l[1] += "…";
    }
    int y = 82;
    for (auto& s : l) tekst(p, F_P22B, X, y += 26, s);
    p.prostokat(12, y + 12, 776, 1, true);  // dół pasa: kreska 1 px jak między wierszami tabeli
    g = y + 13 - 54;                         // blok liczb zaczyna się pod pasem, jak w K1 pod nagłówkiem (54)
  }
  // 4 liczby ostatniej nocy: opis P22B nad liczbą SN56; kolumny nierówne (`EPAP (urządzenie)` ma 190 px)
  const int kolx[4] = {X, 222, 398, 608};
  const int yOpis = 82 + g, yLiczba = 140 + g, yKreska = 154 + g;
  for (int i = 0; i < 4; i++) {
    tekst(p, F_P22B, kolx[i], yOpis, d.opisy[i]);
    tekst(p, F_SN56, kolx[i], yLiczba, d.liczby[i]);
  }
  kreska(p, yKreska);
  // 5 wierszy po 50 px (ASV: EPAP i wsparcie zamiast ciśnienia, R-KOK-15): 182…432, stopka od 434 (R-PRZ-04).
  // Ramka wiersza w stanie odchylenia: krawędzie 2 px na y − 1 i y + 49 (sąsiednie ramki dzielą krawędź), wnętrze
  // y + 1 … y + 48 (48 px). Tekst P22B zajmuje od −17 do +3 px wokół linii bazowej (21 px), więc dwie linie oceny
  // na y + 20 i y + 43 mają po 2 px odstępu od ramki i między sobą.
  // Z odkryciem wiersze mają (432 − góra) / wierszy px. Ocena w dwóch liniach P22B mieści się od 46 px; niżej idzie
  // w jednej linii P22B (`wyżej niż zwykle od 3 nocy`, `stabilnie · wyciek widoczny`, do 296 px), a linia trendu
  // i mediana przesuwają się w lewo o 36 px, żeby zmieścić ją do prawego marginesu.
  const int top = yKreska + 28;
  const int hw = odk ? std::min(50, (432 - top) / std::max(1, d.wierszy)) : 50;
  const bool jednaLinia = hw < 46;
  const int xm = jednaLinia ? 470 : 506, wl = jednaLinia ? 174 : 210;
  const int yKol = top - 6;  // nagłówki kolumn: noc u góry, mediana 30 nocy niżej (T2)
  tekst(p, F_P22B, X, yKol, "Ostatnie 30 nocy");
  tekstPrawo(p, F_P22B, xm, yKol, "mediana");
  tekst(p, F_P22B, xm + 16, yKol, "ocena");
  for (int i = 0; i < d.wierszy; i++) {  // 4 wiersze w CPAP i AutoSet, 5 w ASV (R-KOK-15)
    const WierszK1& w = d.w[i];
    int y = top + i * hw, sr = y + hw / 2;
    if (i < d.wierszy - 1 && !w.stan) p.prostokat(12, y + hw - 1, 776, 1, true);
    if (w.stan) p.ramka(12, y - 1, 776, hw + 2, 2);  // R-KOK-03: wiersz w ramce (legenda: ramka = poza twoją normą)
    tekst(p, F_P24B, X, sr + 9, w.nazwa);
    int hl = hw >= 50 ? 32 : hw - 14;  // linia trendu niższa przy niższym wierszu
    linia30(p, 196, sr - hl / 2, wl, hl, w.linia, 3, false);
    tekstPrawo(p, F_P28B, xm, sr + 10, w.mediana);
    if (w.slowo2.empty()) {
      tekst(p, F_P22B, xm + 16, sr + 8, w.slowo);
    } else if (jednaLinia) {  // `od {n} nocy` czyta się dalej zdania, inne dopiski po `·` (separator panelu)
      tekst(p, F_P22B, xm + 16, sr + 8, w.slowo + (w.slowo2.rfind("od ", 0) == 0 ? " " : " · ") + w.slowo2);
    } else {  // T3: `wyżej niż zwykle` / `od {n} nocy` w 2 liniach; przy wierszu 46…49 px o 1 px ciaśniej
      tekst(p, F_P22B, xm + 16, y + (hw >= 50 ? 20 : 19), w.slowo);
      tekst(p, F_P22B, xm + 16, y + hw - (hw >= 50 ? 7 : 6), w.slowo2);
    }
  }
  stopkaAkcji(p, "", stopkaK1(d.jestNoc));
}

// ---------- K2 (5.5.3, SPEC-KOKPIT 4, opcja A) ----------
void rysujK2(Plotno& p, const DaneK2& d) {
  p.prostokat(0, 0, 800, 480, false);
  // nagłówek jak w K1 (`… · 1 / 2`): co to za strona i jej numer
  tekst(p, F_P24B, X, 33, d.zakres.empty() ? std::string("Ostatnie 7 nocy") : "Ostatnie 7 nocy · " + d.zakres);
  tekstPrawo(p, F_P22B, XP, 33, numerStronyTekst(Ekran::K2));  // `2 / 6`
  kreska(p, 44, 2, X, XP);
  tekst(p, F_SB24, X, 68, d.zdanie);
  // tabela na marginesach 20 px: etykieta 20–164, 7 nocy co 68 px od 168, norma do prawej na 780; bez kolumny 30 nocy
  const int TX = 20, TXP = 780, xKol = 168, krok = 68, top = 119, h = 28;
  for (int i = 0; i < 7; i++) {
    // skróty dni 2-literowe (`pn 30`): wszystkie nagłówki P22B, ostatnią noc wyróżnia pionowa
    // linia; R-KOK-14: romb 12 px nad środkiem nagłówka swojej kolumny, więc nie przylega do sąsiedniej
    int xp = xKol + (i + 1) * krok - 7;
    tekstPrawo(p, F_P22B, xp, top - 9, d.kol[i]);
    if (d.romb[i]) {
      int cx = xp - szer(F_P22B, d.kol[i]) / 2, cy = 85, r = 6;
      p.trojkat(cx - r, cy, cx + r, cy, cx, cy - r);
      p.trojkat(cx - r, cy, cx + r, cy, cx, cy + r);
    }
  }
  tekstPrawo(p, F_P22B, TXP, top - 9, "norma");
  kreska(p, top - 2, 3, TX, TXP);
  int y = top + 1;
  for (int r = 0; r < d.wierszy; r++) {  // 9 wierszy w CPAP i AutoSet, 10 w ASV (R-KOK-15)
    const WierszK2& w = d.w[r];
    if (w.grupa) {  // linie grup: przed `Czas z maską` i przed `Wyciek l/min`
      kreska(p, y, 1, TX, TXP);
      y += 2;
    }
    int b = y + h / 2 + 8;  // wersalik P22B wyśrodkowany w wierszu
    if (w.stan) {  // R-KOK-09: etykieta wiersza w ramce (stan odchylenia = poza twoją normą)
      // długie etykiety (`Zdjęcia maski`, `Ciśnienie P95`, 143 px) z węższym marginesem, żeby ramka kończyła się
      // co najmniej 2 px przed 1. komórką nocy (x 171), czyli najdalej na x 169
      int sz = szer(F_P22B, w.etykieta), pad = std::max(2, std::min(6, (xKol + 1 - TX - sz) / 2));
      p.ramka(TX, y + 1, sz + 2 * pad, h - 2, 2);
      tekst(p, F_P22B, TX + pad, b, w.etykieta);
    } else {
      tekst(p, F_P22B, TX + (w.wciecie ? 14 : 0), b, w.etykieta);
    }
    for (int i = 0; i < 7; i++) {
      const KomorkaK2& k = w.k[i];
      int x0 = xKol + i * krok + 3, x1 = xKol + (i + 1) * krok - 1;
      if (k.tekst == "–") {  // T7: brak danych na środku komórki (przy prawej krawędzi czyta się jak minus)
        tekstSrodek(p, F_P22B, (x0 + x1) / 2, b, k.tekst);
        continue;
      }
      if (k.ponadProg) {  // R-KOK-07: negatyw
        p.prostokat(x0, y + 1, x1 - x0, h - 2, true);
        tekstPrawo(p, F_P22B, x1 - 5, b, k.tekst, false);
      } else {
        if (k.poza) p.ramka(x0, y + 1, x1 - x0, h - 2, 2);  // R-KOK-08: ramka
        tekstPrawo(p, F_P22B, x1 - 5, b, k.tekst);
      }
    }
    tekstPrawo(p, F_P22B, TXP, b, w.norma);
    y += h;
  }
  // pionowe linie 1 px: przed ostatnią nocą (wyróżnienie bez znaczenia kolorystycznego) i przed normą
  p.prostokat(xKol + 6 * krok + 1, top + 1, 1, y - top - 1, true);
  p.prostokat(xKol + 7 * krok + 3, top + 1, 1, y - top - 1, true);
  // legenda (T6), jedna dla całego panelu: czarny prostokąt `ponad próg`, ramka `poza twoją normą`, P22B prosto
  int ly = y + 21;
  p.prostokat(X, ly - 15, 26, 16, true);
  int x = tekst(p, F_P22B, X + 34, ly, "ponad próg");
  p.ramka(x + 26, ly - 16, 26, 18, 2);
  tekst(p, F_P22B, x + 60, ly, "poza twoją normą");
  stopkaAkcji(p, "", stopkaK2(d.sprawy));
}

// ---------- pytanie „Co było inaczej?” (5.11.4, SPEC-KOKPIT 5) ----------
static const char* const DNI_PELNE[7] = {"niedziela", "poniedziałek", "wtorek", "środa", "czwartek", "piątek", "sobota"};

void rysujPytanie(Plotno& p, const DanePytania& d) {
  p.prostokat(0, 0, 800, 480, false);
  int dt = dzienTygodnia(d.dataPoranka);
  std::string prawo = dt >= 0 ? std::string(DNI_PELNE[dt]) + ", " + std::to_string(liczbaCalk(d.dataPoranka.substr(8, 2).c_str())) +
                                    " " + MIESIACE_D[liczbaCalk(d.dataPoranka.substr(5, 2).c_str()) - 1]
                              : std::string();
  naglowekKarty(p, {"Pytanie poranne", prawo}, 33);
  std::string et = d.innaNoc ? "INNA NOC" : "NOC " + ddmm(d.dataPoranka);
  int we = szer(F_P22B, et, 1);
  p.ramka(X, 60, we + 18, 30, 2);
  tekst(p, F_P22B, X + 9, 82, et, true, 1);
  if (d.termin >= 0) {  // T9: pominięcie jest w stopce; podpowiedź w rzędzie etykiety, z prawej
    char b[64];
    snprintf(b, sizeof b, "Bez odpowiedzi pytanie zniknie o %02d:%02d.", d.termin / 60, d.termin % 60);
    tekstPrawo(p, F_P22B, XP, 82, b);
  }
  // opis nocy pod etykietą na pełną szerokość 744 px, najwyżej 2 linie (obok etykiety
  // wychodził poza ekran)
  auto op = zawin(F_P22B, d.opisNocy, W);
  int y = 114;
  for (size_t i = 0; i < op.size() && i < 2; i++, y += 26) tekst(p, F_P22B, X, y, op[i]);
  y += 48 - 26;
  tekst(p, F_SB44, X, y, "Co było wczoraj inaczej?");
  tekst(p, F_P22B, X, y + 30, "Zaznacz wszystko, co pasuje.");  // T8
  int y0 = y + 66;
  for (int i = 0; i < 8; i++) {
    int kol = i % 2, wiersz = i / 2;
    int x = kol ? 412 : X, cy = y0 + wiersz * 48, wk = kol ? XP - 412 : 376;
    bool zazn = d.zaznaczone & POZYCJE[i].bit;
    if (i == d.kursor) p.ramka(x, cy - 23, wk, 46, 3);  // kursor: ramka 3 px (5.11.4)
    int bx = x + 10, by = cy - 14;
    if (zazn) {  // zaznaczone: negatyw z białym ptaszkiem 4 px
      p.prostokat(bx, by, 28, 28, true);
      for (int g = -2; g < 2; g++) {
        p.linia(bx + 6, by + 14 + g, bx + 12, by + 21 + g, false);
        p.linia(bx + 12, by + 21 + g, bx + 23, by + 7 + g, false);
      }
    } else {
      p.ramka(bx, by, 28, 28, 3);
    }
    std::string e = POZYCJE[i].etykieta;
    int maxW = wk - 58;
    if (szer(F_P22B, e) <= maxW) {
      tekst(p, F_P22B, x + 50, cy + 8, e);
    } else {  // długa etykieta leków: dwie linie tym samym krojem
      auto l = zawin(F_P22B, e, maxW);
      for (size_t k = 0; k < l.size() && k < 2; k++) tekst(p, F_P22B, x + 50, cy - 4 + (int)k * 24, l[k]);
    }
  }
  stopkaAkcji(p, "", stopkaPytania());
}

// ---------- U-05 „Test sprzętu” (kanwa design/kanwa/U05-test-sprzetu.dc.html) ----------
// Wymiary z kanwy (podgląd design/png/U05-test-sprzetu.png): nagłówek 0–48 i kreska 2 px na 48, linia bazowa 31,
// marginesy 26 px; kolumny od 26 i 414 po 360 px; nazwa kolumny P16 wersalikami (rozstrzelenie 1 px) na 79, kreska 2 px
// na 89; wiersze co 47 px (46 + kreska 1 px) od 91, linia bazowa 30 px od góry wiersza; ramka 2 px 26..774 × 378..422,
// tekst na 407, wcięcie 16 px (2 + 14); stopka: kreska 2 px na 436, P18B na 466, od 20 i do 780.
namespace {
const int T_X = 26, T_XP = 774, T_KOL2 = 414;
const int T_NAGLOWEK = 48, T_BAZA_NAGLOWKA = 31;
const int T_BAZA_KOLUMNY = 79, T_KRESKA_KOLUMNY = 89, T_WIERSZ0 = 91, T_WIERSZ = 47, T_BAZA_WIERSZA = 30;
const int T_RAMKA_Y = 378, T_RAMKA_H = 44, T_BAZA_RAMKI = 407, T_WCIECIE_RAMKI = 16;
const int T_STOPKA = 436, T_BAZA_STOPKI = 466, T_STOPKA_X = 20, T_STOPKA_XP = 780;

// Ikony stopki U-05 jak na kanwie: znaki kroju bez kwadratu (◀ pełny trójkąt, ↻ okrąg z grotem), czarne na białym.
// Wymiary zmierzone z podglądu kanwy dla 18 px: ◀ 14 × 17 px, tekst 20 px od początku ikony; ↻ okrąg o średnicy 13 px
// (kreska 2 px) z przerwą w prawej górnej ćwiartce i grotem w prawo u góry, tekst 21 px od początku ikony.
const int IK_U05_LEWO = 20, IK_U05_ODSWIEZ = 21;  // przesunięcie tekstu za ikoną
void ikonaU05(Plotno& p, int ik, int x, int yb) {
  if (ik == IK_LEWO || ik == IK_PRAWO) {  // ▶ jako lustro ◀ (stopka U-05 `▶ odśwież`)
    int yc = yb - 7;
    if (ik == IK_LEWO) p.trojkat(x, yc, x + 13, yc - 8, x + 13, yc + 8);
    else p.trojkat(x + 13, yc, x, yc - 8, x, yc + 8);
    return;
  }
  // ↻: środek okręgu (x + 8,5; yb − 6,5), promień 4,5–6,5 px; bez łuku dla kąta z (−90°, 0°) (oś y w dół)
  double cx = x + 8.5, cy = yb - 6.5;
  for (int yy = yb - 14; yy <= yb; yy++)
    for (int xx = x + 1; xx <= x + 16; xx++) {
      double dx = xx + 0.5 - cx, dy = yy + 0.5 - cy, d = std::sqrt(dx * dx + dy * dy);
      if (d < 4.5 || d > 6.5) continue;
      if (dx > 0 && dy < 0) continue;
      p.piksel(xx, yy, true);
    }
  p.trojkat(x + 8, yb - 16, x + 8, yb - 9, x + 11, yb - 13);
}
// Grupa stopki U-05 zawsze zaczyna się ikoną (stopkaTestu()): `◀ `, `▶ ` albo `↻ `; zwraca ikonę i początek tekstu.
int kodIkonyU05(const std::string& g, size_t& reszta) {
  for (auto& [z, ik] : {std::pair<const char*, int>{"◀ ", IK_LEWO}, {"▶ ", IK_PRAWO}})
    if (g.rfind(z, 0) == 0) {
      reszta = std::string(z).size();
      return ik;
    }
  reszta = std::string("↻ ").size();
  return IK_ODSWIEZ;
}
int przesuniecieU05(int ik) { return ik == IK_ODSWIEZ ? IK_U05_ODSWIEZ : IK_U05_LEWO; }
int szerGrupyU05(const std::string& g) {
  size_t r;
  int ik = kodIkonyU05(g, r);
  return przesuniecieU05(ik) + szer(F_P18B, g.substr(r));
}
void grupaU05(Plotno& p, int x, const std::string& g) {
  size_t r;
  int ik = kodIkonyU05(g, r);
  ikonaU05(p, ik, x, T_BAZA_STOPKI);
  tekst(p, F_P18B, x + przesuniecieU05(ik), T_BAZA_STOPKI, g.substr(r));
}

void kolumnaU05(Plotno& p, int x0, const std::string& nazwa, const std::vector<WierszTestu>& w) {
  if (w.empty()) return;
  tekst(p, F_P16, x0, T_BAZA_KOLUMNY, wersaliki(nazwa), true, 1);
  p.prostokat(x0, T_KRESKA_KOLUMNY, TEST_KOLUMNA, 2, true);
  for (size_t i = 0; i < w.size() && i < 5; i++) {
    int y0 = T_WIERSZ0 + (int)i * T_WIERSZ;
    tekst(p, F_P20, x0, y0 + T_BAZA_WIERSZA, w[i].nazwa);
    tekstPrawo(p, F_P20B, x0 + TEST_KOLUMNA, y0 + T_BAZA_WIERSZA, dopasujWartosc(w[i].nazwa, {w[i].wartosc}));
    p.prostokat(x0, y0 + T_WIERSZ - 1, TEST_KOLUMNA, 1, true);
  }
}
}  // namespace

int miejsceNaWartosc(const std::string& nazwa) { return TEST_KOLUMNA - szer(F_P20, nazwa) - TEST_ODSTEP; }

std::string dopasujWartosc(const std::string& nazwa, const std::vector<std::string>& warianty) {
  if (warianty.empty()) return "";
  int m = miejsceNaWartosc(nazwa);
  for (auto& w : warianty)
    if (szer(F_P20B, w) <= m) return w;
  std::string w = warianty.back();
  while (!w.empty() && szer(F_P20B, w + "…") > m) {
    size_t k = w.size() - 1;
    while (k > 0 && ((unsigned char)w[k] & 0xC0) == 0x80) k--;  // cały znak UTF-8
    w.erase(k);
  }
  return w + "…";
}

void rysujTest(Plotno& p, const DaneTestu& d) {
  p.prostokat(0, 0, 800, 480, false);
  tekst(p, F_P20B, T_X, T_BAZA_NAGLOWKA, "Test sprzętu");
  tekstPrawo(p, F_P20, T_XP, T_BAZA_NAGLOWKA, "Ustawienia → Test sprzętu");
  kreska(p, T_NAGLOWEK);
  kolumnaU05(p, T_X, "Oprogramowanie i pamięć", d.lewa);
  kolumnaU05(p, T_KOL2, "Łączność i bateria", d.prawa);
  p.ramka(T_X, T_RAMKA_Y, T_XP - T_X, T_RAMKA_H, 2);
  tekst(p, F_P20B, T_X + T_WCIECIE_RAMKI, T_BAZA_RAMKI, d.ramkaLewo);
  tekstPrawo(p, F_P20, T_XP - T_WCIECIE_RAMKI, T_BAZA_RAMKI, d.ramkaPrawo);
  kreska(p, T_STOPKA);
  std::vector<std::string> s = stopkaTestu(d.awaryjny);
  if (s.size() == 1) {  // awaryjny: sam `◀ wróć` jak dotąd z prawej
    grupaU05(p, T_STOPKA_XP - szerGrupyU05(s[0]), s[0]);
  } else {  // grupy od lewej w kolejności ◀ ▶ ↻, odstęp 26 px
    int x = T_STOPKA_X;
    for (auto& g : s) {
      grupaU05(p, x, g);
      x += szerGrupyU05(g) + 26;
    }
  }
}

// ---------- ekran nocny „Dobranoc” (ADR-0018, SPEC-KOKPIT 6) ----------
static const SceneBudzika* scenaNocy(const std::string& id) {
  for (int i = 0; i < SCENY_NOCY_N; i++)
    if (id == SCENY_NOCY[i].id) return &SCENY_NOCY[i];
  return nullptr;
}

bool scenaNocyJest(const std::string& id) { return scenaNocy(id) != nullptr; }

void rysujDobranoc(Plotno& p, const DaneNocy& d) {
  p.prostokat(0, 0, 800, 480, false);  // budzik zawsze na bieli (ADR-0009)
  TekstyNocy t = tekstyNocy(d);
  const SceneBudzika* s = scenaNocy(d.wybor.scena);
  auto stopkaNocy = [&]() {
    stopkaAkcji(p, t.stopkaLewo, t.stopkaPrawo.empty() ? std::vector<std::string>{} : std::vector<std::string>{t.stopkaPrawo});
  };
  switch (d.wybor.wariant) {
    case WariantNocy::A1: {  // karta według anatomii ADR-0009
      naglowekKarty(p, {t.naglowekLewo, t.naglowekPrawo});
      std::vector<Fakt> f;
      f.reserve(t.fakty.size());
      for (auto& e : t.fakty) f.push_back({e.first, e.second});
      trescKarty(p, t.etykieta, t.tytul, t.linie, f, t.krok);
      if (s) p.bitmapa(s->bmp, 800 - 26 - 14 - 125 - s->w / 2, 418 - s->h, s->w, s->h);
      stopkaNocy();
      break;
    }
    case WariantNocy::A2:  // duża scena, minimum tekstu, bez nagłówka i stopki
      if (s) p.bitmapa(s->bmp, (800 - s->w) / 2, 40, s->w, s->h);
      tekstSrodek(p, F_SB44, 400, t.zdanieA2.empty() ? 395 : 382, t.tytul);
      if (!t.zdanieA2.empty()) tekstSrodek(p, F_P22B, 400, 418, t.zdanieA2);
      tekstSrodek(p, F_P22B, 400, t.zdanieA2.empty() ? 440 : 452, t.liniaA2);
      break;
    case WariantNocy::A3:  // typograficzny: godzina założenia maski
      naglowekKarty(p, {t.naglowekLewo, t.naglowekPrawo});
      etykieta(p, XK, 92, t.etykieta);
      if (t.tytulGodzina) tekst(p, F_S140, 20, 240, t.tytul);
      else tekst(p, F_S110, 20, 222, t.tytul);
      if (t.linie.size() > 0) tekst(p, F_P28B, XK, 290, t.linie[0]);
      if (t.linie.size() > 1) tekst(p, F_P22B, XK, 324, t.linie[1]);
      if (s) p.bitmapa(s->bmp, 800 - 30 - s->w, 418 - s->h, s->w, s->h);
      stopkaNocy();
      break;
  }
}

// ---------- ekran wzorca L, „Omówione”, „Otwarte sprawy”, pytania tak/nie ----------
const char* const ZDANIE_R_LEK_28 = "Duszność albo ból w klatce? Wezwij pomoc.";

namespace {
const int ZNAK_W = 44, ZNAK_ODSTEP = 14;  // znak ostrzeżenia przed tytułem ekranu L (artboard L01-centralne)

// trójkąt ostrzeżenia z `!` w kolorze `cz`, podstawa na linii bazowej y
void znakOstrzezenia(Plotno& p, int x, int y, bool cz) {  // kanwa: trójkąt 44 × 40 z obrysem i `!`
  int h = 38, w = ZNAK_W;
  p.trojkat(x, y, x + w, y, x + w / 2, y - h, cz);
  p.trojkat(x + 7, y - 4, x + w - 7, y - 4, x + w / 2, y - h + 9, !cz);
  p.prostokat(x + w / 2 - 2, y - 26, 4, 12, cz);
  p.prostokat(x + w / 2 - 2, y - 11, 4, 4, cz);
}

void grupyStopki(Plotno& p, int x, int y, const std::vector<std::string>& g, bool cz) {
  StylStopki st;
  for (auto& e : g) x = tekst(p, F_P22B, x, y, e, cz) + 26;
}

// pole odpowiedzi: ramka (3 px albo przerywana), w środku ikona przycisku i słowo
void poleOdpowiedzi(Plotno& p, int x, int y, int w, int h, const std::string& ik, const std::string& slowo,
                    const GFXfont* f, bool przerywane) {
  if (przerywane) ramkaPrzerywana(p, x, y, w, h);
  else p.ramka(x, y, w, h, 3);
  int wi = szer(f, ik), ws = szer(f, slowo), x0 = x + (w - (wi + 8 + ws)) / 2;
  int b = y + h / 2 + wersalik(f) / 2;
  tekst(p, f, x0, b, ik);
  tekst(p, f, x0 + wi + 8, b, slowo);
}
}  // namespace

int szerTresciWzorca(bool) { return W; }
int szerTytuluWzorca(bool lekarz) { return lekarz ? W - ZNAK_W - ZNAK_ODSTEP : W; }

std::vector<std::string> stopkaWzorca(bool lekarz) {
  // kanwa: `◀ 2 s omówione` · `↻ 2 s wiem` · `▶ raport`, w panelu kolejność ◀ ▶ ↻ jak przyciski;
  // ▶ = kokpit jak na każdym ekranie dnia, „Raport dla lekarza” otwiera ▶ 2 s (do w stanie R-LEK-05);
  // L-09 (problem poranny): ▶ kokpit (K1) i `↻ 2 s wiem` (mapa 5.6.2, podpis każdej akcji)
  if (lekarz) return {"◀ 2 s omówione", "▶ kokpit · 2 s raport", "↻ 2 s wiem"};
  return {"▶ kokpit", "↻ 2 s wiem"};
}

void rysujWzorzec(Plotno& p, const DaneWzorca& d) {
  const bool B = false;  // treść na negatywie: biała
  p.prostokat(0, 0, 800, 480, true);
  if (d.lekarz) p.ramka(4, 4, 792, 472, 6, B);
  int x0 = d.lekarz ? 10 : 0, x1 = d.lekarz ? 790 : 800;  // kreski nagłówka i stopki w ramce
  tekst(p, F_P24B, X, 41, d.naglowek.lewo, B);
  if (!d.naglowek.prawo.empty()) tekstPrawo(p, F_P24B, XP, 41, d.naglowek.prawo, B);
  p.prostokat(x0, 52, x1 - x0, 2, B);
  // strefa linii uwagi na ekranie „problem” (S3); bez etykiety tytuł wchodzi w jej miejsce (36 px), więc treść
  // przesuwa się o wysokość linii minus to miejsce, a wizualizacja traci resztę (krok i stopka zostają na swoich liniach)
  // R-PIE-07: na ekranie wzorca A (ton „lekarz”) jedyną linią jest E-05 krytyczne (dyrygent podaje ją tylko wtedy);
  // pasek w ramce ekranu, treść przesuwa się jak na ekranie „problem”
  int hl = !d.linia.empty() ? (d.liniaA ? 60 : 48) : 0;
  if (hl) {
    const GFXfont* f = d.liniaA ? F_P28B : F_P24B;
    int yb = d.liniaA ? 94 : 87;
    if (d.liniaA || d.liniaOstrzezenie) {  // ostrzeżenie: na negatywie biały pasek, `!` i tekst czarny
      p.prostokat(x0, 54, x1 - x0, hl, B);
      int x = tekst(p, f, X, yb, "!", !B);
      tekst(p, f, x + 12, yb, d.linia, !B);
    } else {  // informacja albo prośba: biała ramka 2 px, tekst biały, bez `!`
      p.ramka(12, 57, 776, hl - 6, 2, B);
      tekst(p, f, X, yb, d.linia, B);
    }
  }
  int sz = hl ? hl - (d.etykieta.empty() ? 36 : 0) : 0;
  if (sz < 0) sz = 0;
  if (!d.etykieta.empty()) etykieta(p, X, 88 + sz, wersaliki(d.etykieta), B);
  int xt = X;
  if (d.lekarz) {
    znakOstrzezenia(p, X, 147 + sz, B);
    xt = X + ZNAK_W + ZNAK_ODSTEP;
  }
  std::vector<std::string> t = zawin(F_SB44, d.tytul, XP - xt);
  if (!t.empty()) tekst(p, F_SB44, xt, 143 + sz, t[0], B);  // tytuł ≤ 30 znaków: jedna linia (test szerokości)
  auto pow = zawin(F_P28B, d.powod, W);
  int y = 188 + sz;
  for (size_t i = 0; i < pow.size() && i < 2; i++, y += 36) tekst(p, F_P28B, X, y, pow[i], B);
  int dod = pow.size() > 1 ? 36 : 0;                  // drugi wiersz powodu zabiera miejsce wizualizacji
  int hyp = d.hipoteza.empty() ? 0 : 44;              // ramka hipotezy nad krokiem (kanwa L09)
  int vTop = 208 + dod + sz, vDol = 311 - hyp, bPod = 336 - hyp;
  if (d.wiz == WizWzorca::LINIA && d.wartosci.size() >= 2 && vDol - vTop >= 24) {
    double mn = 1e9, mx = -1e9;
    for (float v : d.wartosci)
      if (!std::isnan(v)) { mn = std::min(mn, (double)v); mx = std::max(mx, (double)v); }
    if (!std::isnan(d.prog)) { mn = std::min(mn, (double)d.prog); mx = std::max(mx, (double)d.prog); }
    if (mn <= mx) {
      if (mx - mn < 1e-6) { mn -= 1; mx += 1; }
      mn = std::min(mn, 0.0);  // oś od zera (kreska podstawy = 0)
      // 4 px odstępu między punktem wartości 0 a kreską podstawy, żeby 0 nie zlewało się z osią
      int h = vDol - vTop - 12, n = (int)d.wartosci.size();
      auto px = [&](int i) { return X + 4 + (int)std::lround(i * (W - 8) / (double)(n - 1)); };
      auto py = [&](double v) { return vTop + 4 + h - (int)std::lround((v - mn) / (mx - mn) * h); };
      p.prostokat(X, vDol, W, 2, B);
      if (!std::isnan(d.prog))
        for (int xx = X; xx < XP; xx += 14) p.prostokat(xx, py(d.prog), std::min(8, XP - xx), 2, B);
      for (int i = 1; i < n; i++)
        if (!std::isnan(d.wartosci[(size_t)i]) && !std::isnan(d.wartosci[(size_t)(i - 1)]))
          for (int g = -1; g <= 1; g++)
            p.linia(px(i - 1), py(d.wartosci[(size_t)(i - 1)]) + g, px(i), py(d.wartosci[(size_t)i]) + g, B);
      for (int i = 0; i < n; i++)
        if (!std::isnan(d.wartosci[(size_t)i])) p.prostokat(px(i) - 4, py(d.wartosci[(size_t)i]) - 4, 8, 8, B);
    }
  } else if (d.wiz == WizWzorca::RZEDY) {  // L-08: 2 × 30 kółek 20 px od x = 248, co 17,5 px (kanwa)
    const bool* r[2] = {d.rzad1, d.rzad2};
    const std::string* et[2] = {&d.etRzad1, &d.etRzad2};
    for (int k = 0; k < 2; k++) {
      int b = vTop + 38 + 44 * k;
      tekst(p, F_P22B, X, b, *et[k], B);
      for (int i = 0; i < 30; i++) {
        int cx = 258 + (int)std::lround(i * 17.4), cy = b - 8;
        p.kolo(cx, cy, 8, true, B);
        if (!r[k][i]) p.kolo(cx, cy, 6, true, !B);
      }
    }
  }
  if (d.wiz != WizWzorca::BRAK) {
    tekst(p, F_P22B, X, bPod, d.podpisLewo, B);
    tekstPrawo(p, F_P22B, XP, bPod, d.podpisPrawo, B);
  }
  if (hyp) {
    p.ramka(X, 336, W, 40, 2, B);
    tekst(p, F_P22B, X + 14, 364, d.hipoteza, B);
  }
  if (d.pilnoscA) tekst(p, F_P22B, X, hyp ? 388 : 372, ZDANIE_R_LEK_28, B);
  tekst(p, F_P28B, X, 418, d.krok, B);
  p.prostokat(x0, 434, x1 - x0, 2, B);
  grupyStopki(p, X, 462, stopkaWzorca(d.lekarz), B);
}

void rysujRaport(Plotno& p, const DaneRaportu& d) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekDanych(p, d.naglowek);
  etykieta(p, X, 94, wersaliki("Raport dla lekarza"));
  tekst(p, F_SB44, X, 145, d.temat);
  tekst(p, F_P28B, X, 195, "Nie udało się przygotować raportu (" + d.kod + ")");
  stopkaAkcji(p, "", {"◀ wróć", "▶ dalej", "↻ ekran dnia"});
}

void rysujPotwierdzenie(Plotno& p, const DanePotwierdzenia& d) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekDanych(p, d.naglowek);
  int y = 145;
  if (!d.nad.empty()) {
    tekst(p, F_P28B, X, 97, d.nad);  // kanwa P-zapisano: pytanie od 76 px, tytuł od 123 px
    y = 158;
  }
  tekst(p, F_SB44, X, y, d.tytul);
  auto pow = zawin(F_P28B, d.powod, W);
  int b = y + 50;
  for (size_t i = 0; i < pow.size() && i < 2; i++, b += 36) tekst(p, F_P28B, X, b, pow[i]);
  if (!d.stopka.empty()) stopkaAkcji(p, "", d.stopka);
}

std::vector<std::string> zdaniaOmowione(bool pilnoscA) {
  return {pilnoscA ? "Pełny ekran zniknie na 8 tygodni, przypomnienie co 3 dni zostanie."
                   : "Panel przestanie o tym przypominać na 8 tygodni albo do zmiany ustawień.",
          "Wróci wcześniej, gdy wynik wyraźnie się pogorszy.", "Data rozmowy trafi do następnego raportu."};
}

std::vector<std::string> zdaniaOmowienia(int wariant) {
  // teksty dosłownie z 5.12.7 (artboard Q-omowione); bez wartości i drogi zmiany ustawień (ADR-0002)
  switch (wariant) {
    case 2: return zdaniaOmowione(true);
    case 3:
      return {"Drugi raz bez poprawy: pełny ekran zniknie na 4 tygodnie.", "Wróci wcześniej, gdy wynik wyraźnie się pogorszy.",
              "Data rozmowy trafi do następnego raportu."};
    case 4:
      return {"Znowu bez poprawy: pełny ekran co 2 tygodnie.", "Wróci wcześniej, gdy wynik wyraźnie się pogorszy.",
              "Data rozmowy trafi do następnego raportu."};
    case 5: return {"Najpierw nowy raport: ▶ 2 s na ekranie L."};
    default: return zdaniaOmowione(false);
  }
}

void rysujOmowione(Plotno& p, const DaneOmowione& d) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekDanych(p, d.naglowek, F_P22B);
  etykieta(p, X, 94, wersaliki(d.nazwa));
  tekst(p, F_SB44, X, 145, "Omówione z lekarzem?");
  int y = 186;
  for (auto& z : d.zdania)
    for (auto& l : zawin(F_P22B, z, W)) {
      y += 28;
      tekst(p, F_P22B, X, y, l);
    }
  if (d.blokada) {  // R-LEK-26: ▶ nic nie zapisuje i nie ma podpisu (mapa 5.6.2, R-PRZ-04)
    poleOdpowiedzi(p, X, 326, 360, 92, "◀", "wróć", F_P28B, false);
  } else {
    poleOdpowiedzi(p, X, 326, 360, 92, "◀", "nie, wróć", F_P28B, false);
    poleOdpowiedzi(p, XP - 360, 326, 360, 92, "▶", "tak, omówione", F_P28B, false);
  }
  kreska(p, 434);
  StylStopki st;
  tekst(p, F_P22B, X, 465, "Brak wyboru przez 60 s = nic się nie zmienia.");
  tekstPrawo(p, F_P22B, XP, 465, "↻ anuluj");
}

std::vector<std::string> stopkaSpraw(int i, int n) {
  // ◀ poprzednia karta (przed pierwszą K2), ▶ następna karta (za ostatnią S8), ↻ ekran dnia
  return {i <= 1 || n == 0 ? "◀ 7 nocy" : "◀ poprzednia", n > 0 && i < n ? "▶ następna" : "▶ norma", "↻ ekran dnia"};
}

void rysujSprawy(Plotno& p, const DaneSpraw& d) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekDanych(p, {d.naglowek, numerStronyTekst(Ekran::SPRAWY)});  // `3 / 6`
  if (d.n > 0) etykieta(p, X, 94, wersaliki("Sprawa " + std::to_string(d.i) + " z " + std::to_string(d.n)));
  tekst(p, F_SB44, X, 145, d.tytul);
  int y = 195;
  auto pow = zawin(F_P28B, d.powod, W);
  for (size_t i = 0; i < pow.size() && i < 2; i++, y += 36) tekst(p, F_P28B, X, y, pow[i]);
  kreska(p, 251);
  const char* nazwy[4] = {"Ocena:", "Ty:", "Dalej:", "Koniec:"};
  const std::string* wart[4] = {&d.ocena, &d.ty, &d.dalej, &d.koniec};
  for (int i = 0; i < 4; i++) {
    if (d.n == 0 && i > 0) break;  // stan pusty: tylko `Ocena:` (R-EKR-50)
    if (d.n == 0 && wart[i]->empty()) break;  // bez pewnej oceny stan pusty bez wiersza `Ocena:`
    int b = 283 + 42 * i;
    tekst(p, F_P24B, X, b, nazwy[i]);
    tekst(p, F_P24S, SPRAWY_X_WARTOSCI, b, *wart[i]);  // kanwa: nazwa 700, wartość 600
  }
  if (d.n == 0 && !d.dopisek.empty()) tekst(p, F_P24S, X, 283 + 42, d.dopisek);  // R-EKR-50, ocena częściowa
  stopkaAkcji(p, "", stopkaSpraw(d.i, d.n));
}

void naglowekEkranu(Plotno& p, const Naglowek& n) { naglowekDanych(p, n, F_P22B); }

void stopkaEkranu(Plotno& p, std::vector<std::string> lewo, const std::string& prawo) {
  uporzadkujPrzyciski(lewo);
  StylStopki st;
  kreska(p, 434);
  int x = X;
  for (auto& g : lewo) x = tekst(p, F_P22B, x, 465, g) + 26;
  if (!prawo.empty()) tekstPrawo(p, F_P22B, XP, 465, prawo);
}

void rysujPytanieTN(Plotno& p, const DanePytaniaTN& d) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekDanych(p, d.naglowek);
  etykieta(p, X, 94, wersaliki(d.etykieta));
  tekst(p, F_SB44, X, 145, d.tytul);
  tekst(p, F_P28B, X, 195, d.opis);
  if (!d.nadPolami.empty()) tekst(p, F_P22B, X, 284, d.nadPolami);
  poleOdpowiedzi(p, X, 312, 250, 96, "◀", "nie", F_P34B, false);
  poleOdpowiedzi(p, 300, 312, 250, 96, "▶", "tak", F_P34B, false);  // kolejność pól ◀ ▶ ↻ jak przyciski
  poleOdpowiedzi(p, XP - 200, 312, 200, 96, "↻", "pomiń", F_P28B, true);
  kreska(p, 434);
  tekst(p, F_P22B, X, 465, d.stopka);
}
