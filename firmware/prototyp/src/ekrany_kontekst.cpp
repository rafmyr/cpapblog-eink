// SPDX-License-Identifier: GPL-3.0-or-later
// Rysowanie ekranów opis w src/ekrany_kontekst.h. Współrzędne z SVG artboardów kanwy (design/kanwa/SK3x-*.dc.html).
#include "ekrany_kontekst.h"
#include <algorithm>
#include <cmath>
#include "../include/fonty_slot.h"
#include "wspolne.h"

std::string godzinaOsiSlotu(int g) {
  char b[8];
  snprintf(b, sizeof b, "%02d:00", ((g % 24) + 24) % 24);
  return b;
}

namespace {

const int X = 28, XP = 772, W = XP - X;
const GFXfont* const F_SN72 = &SN72;  // liczby faktów S-K-34 i S-K-39 (Source Serif 4, 72 px, 600)

void etykieta(Plotno& p, int y, const std::string& u, bool cz) { tekst(p, F_P22B, X, y, wersaliki(u), cz, 2); }

void naglowek(Plotno& p, const Naglowek& n, bool cz) {
  tekst(p, F_P24B, X, 35, n.lewo, cz);
  if (!n.prawo.empty()) tekstPrawo(p, F_P24B, XP, 35, n.prawo, cz);
  p.prostokat(0, 52, 800, 2, cz);
}

void stopka(Plotno& p, const StopkaDzienna& s) {
  p.prostokat(0, 436, 800, 44, false);  // stopka biała także pod negatywem S-K-39
  p.prostokat(0, 434, 800, 2, true);
  std::string prawo = s.prawo, lewo = s.lewo;
  if (ikonaAktualizacjiWlaczona()) {  // R-EKR-61: pełne słowa przed ikoną
    prawo = skrocDoSlow(F_P22B, s.prawo, prawyBrzegStopki() - X);
    lewo = skrocDoSlow(F_P22B, s.lewo, prawyBrzegStopki() - X - (prawo.empty() ? 0 : szer(F_P22B, prawo) + 26));
  }
  if (!lewo.empty()) tekst(p, F_P22B, X, 465, lewo);
  if (!prawo.empty()) tekstPrawo(p, F_P22B, prawyBrzegStopki(), 465, prawo);
}

// linia przerywana pozioma (kreska `k` px, przerwa `q` px, grubość `g`)
void przerywanaPoz(Plotno& p, int x0, int x1, int y, int k, int q, int g) {
  for (int x = x0; x < x1; x += k + q) p.prostokat(x, y - g / 2, std::min(k, x1 - x), g, true);
}
void przerywanaPion(Plotno& p, int x, int y0, int y1, int k, int q, int g) {
  for (int y = y0; y < y1; y += k + q) p.prostokat(x - g / 2, y, g, std::min(k, y1 - y), true);
}
void gruba(Plotno& p, int x0, int y0, int x1, int y1, int g) {
  for (int d = -(g / 2); d < g - g / 2; d++) {
    p.linia(x0, y0 + d, x1, y1 + d, true);
  }
}

// Powód obok godziny B1 w dwóch równych liniach (kanwa: `Potem smog` / `za oknem.`, `Dzień krótszy o 19 min` /
// `niż tydzień temu.`): podział przy spacji (nie przy twardej spacji) z najmniejszą szerokością dłuższej linii.
std::vector<std::string> dwieLinie(const GFXfont* f, const std::string& u, int maxW) {
  std::vector<std::string> naj = zawin(f, u, maxW);
  int najW = 1 << 30;
  for (size_t i = 0; i < u.size(); i++) {
    if (u[i] != ' ') continue;
    std::string a = u.substr(0, i), b = u.substr(i + 1);
    int wa = szer(f, a), wb = szer(f, b);
    if (wa > maxW || wb > maxW) continue;
    if (std::max(wa, wb) < najW) { najW = std::max(wa, wb); naj = {a, b}; }
  }
  return naj;
}

const int WYS_CIASNA = 66;  // najwyższy słupek przy linii uwagi 48 px (od y 286; godzina B1 kończy się na 270)
// Linia uwagi wzorca A ma 60 px zamiast 48, więc blok godziny i powód stoją o tyle niżej; o tyle samo niższy
// jest ciasny wykres (oś i podpisy zostają na miejscu, nad krokiem).
int przesuniecieLinii(int dy) { return dy > 48 ? dy - 48 : 0; }

// blok `podpis` B6 nad godziną B1, obok powód B4 w dwóch liniach (kanwa SK33-okno, SK37-slonce); dy = linia uwagi
void blokGodziny(Plotno& p, const TekstySlotu& t, bool cz, int dy) {
  tekst(p, F_P24B, X, 132 + dy, t.podpis, cz);
  tekst(p, F_SN96, 22, 222 + dy, t.godzina, cz);
  auto l = dwieLinie(F_P34B, t.powod, XP - 330);
  for (size_t i = 0; i < l.size() && i < 2; i++) tekst(p, F_P34B, 330, 168 + dy + 42 * (int)i, l[i], cz);
}
// tytuł B3 i powód (kanwa SK34, SK35, SK36, SK39)
void blokTytulu(Plotno& p, const TekstySlotu& t, bool cz, int dy) {
  tekst(p, F_SB44, X, 145 + dy, t.tytul, cz);
  auto l = zawin(F_P28B, t.powod, W);
  for (size_t i = 0; i < l.size() && i < 2; i++) tekst(p, F_P28B, X, 195 + dy + 34 * (int)i, l[i], cz);
}

// słupki godzina po godzinie (kanwa SK33-okno, SK36-pylek): oś 2 px na 352, słupki do hMax px (80, przy linii uwagi
// WYS_CIASNA), biały w ramce 2 px albo czarny; znacznik `teraz` przerywany na x 30 od 14 px (przy linii uwagi 6 px)
// nad najwyższym słupkiem;
// podpisy osi co 3 h jako godziny `HH:00` (bez kolizji z podpisem z prawej)
void slupki(Plotno& p, const std::vector<float>& v, const std::vector<bool>& czarny, int godz0, const std::string& prawy,
            float skalaMin, int hMax = 80) {
  int n = (int)v.size();
  if (n <= 0) return;
  double krok = (double)W / n;
  int sz = std::max(6, (int)std::lround(krok * 0.774));
  float mx = skalaMin;
  for (float x : v)
    if (!std::isnan(x)) mx = std::max(mx, x);
  int prawyX = XP - szer(F_P22B, prawy);
  for (int i = 0; i < n; i++) {
    int x = X + (int)std::lround(i * krok + (krok - sz) / 2.0);
    if (!std::isnan(v[i])) {
      int h = std::max(6, (int)std::lround((double)v[i] / mx * hMax));
      if (czarny[i]) p.prostokat(x, 352 - h, sz, h, true);
      else p.ramka(x + 1, 351 - h, sz - 2, h, 2);
    }
    if (i > 0 && i % 3 == 0) {  // podpisy co 3 h od `teraz` (kanwa: teraz 11 → 14, 17)
      std::string l = godzinaOsiSlotu(godz0 + i);
      if (x + szer(F_P22B, l) < prawyX - 12) tekst(p, F_P22B, x, 376, l);
    }
  }
  p.prostokat(X, 352, W, 2, true);
  przerywanaPion(p, 30, 352 - hMax - (hMax < 80 ? 6 : 14), 356, 6, 4, 3);  // przy linii uwagi od 280
  tekst(p, F_P22B, X, 376, "teraz");
  tekstPrawo(p, F_P22B, XP, 376, prawy);
}

// klamra nad słupkami okna (kanwa: ścieżka 3 px od y 320 do 312)
void klamra(Plotno& p, int x0, int x1, int yGora, int yDol) {
  p.prostokat(x0, yGora - 1, x1 - x0, 3, true);
  int y0 = std::min(yGora, yDol), y1 = std::max(yGora, yDol);
  p.prostokat(x0 - 1, y0, 3, y1 - y0, true);
  p.prostokat(x1 - 1, y0, 3, y1 - y0, true);
}

void wizualizacjaOkna(Plotno& p, const WynikWietrzenia& o, int godz, float przyLozku, bool ciasno, int przes = 0) {
  int n = (int)o.pm25.size();
  if (n <= 0) return;
  bool lato = o.tryb == TrybWietrzenia::LATO && !std::isnan(przyLozku);
  if (!lato) {
    std::vector<bool> cz(n);
    for (int i = 0; i < n; i++) cz[i] = !o.czyste[i];
    slupki(p, o.pm25, cz, godz, "PM2.5 za oknem", 50, ciasno ? WYS_CIASNA - przes : 80);
    if (o.od >= 0) {  // klamra nad oknem
      double krok = (double)W / n;
      int a = o.od - godz, b = o.do_ - godz;
      int x0 = X + (int)std::lround(a * krok) + 7, x1 = X + (int)std::lround(b * krok) - 7;
      klamra(p, x0, x1, 312, 320);
    }
    return;
  }
  // tryb letni i upał: linia temperatury za oknem (4 px), przerywana temperatura przy łóżku, klamra pod oknem
  float lo = przyLozku, hi = przyLozku;
  for (float t : o.temp)
    if (!std::isnan(t)) { lo = std::min(lo, t); hi = std::max(hi, t); }
  if (hi - lo < 4) { hi += 2; lo -= 2; }
  const int hw = ciasno ? WYS_CIASNA - 8 - przes : 68;  // przy linii uwagi wykres niższy (od 282 zamiast 272)
  auto py = [&](float t) { return 340 - (int)std::lround((double)(t - lo) / (hi - lo) * hw); };
  auto px = [&](int i) { return n == 1 ? X : X + (int)std::lround(i * (double)W / (n - 1)); };
  for (int i = 1; i < n; i++)
    if (!std::isnan(o.temp[i - 1]) && !std::isnan(o.temp[i])) gruba(p, px(i - 1), py(o.temp[i - 1]), px(i), py(o.temp[i]), 4);
  int yl = py(przyLozku);
  przerywanaPoz(p, X, XP, yl, 8, 6, 2);
  std::string l = "przy łóżku " + std::to_string((int)std::lround(przyLozku)) + "°";
  tekstPrawo(p, F_P22B, XP, yl - 8, l);
  p.prostokat(X, 352, W, 2, true);
  przerywanaPion(p, 30, ciasno ? 352 - WYS_CIASNA - 6 + przes : 258, 356, 6, 4, 3);
  tekst(p, F_P22B, 26, 376, "teraz");
  std::string prawy = "°C za oknem";
  int prawyX = XP - szer(F_P22B, prawy);
  for (int i = 1; i < n; i++)
    if (i % 3 == 0) {
      std::string e = godzinaOsiSlotu(godz + i);
      int x = px(i) - szer(F_P22B, e) / 2;
      if (x + szer(F_P22B, e) < prawyX - 12) tekst(p, F_P22B, x, 376, e);
    }
  tekstPrawo(p, F_P22B, XP, 376, prawy);
  if (o.od >= 0) {
    int x0 = px(o.od - godz), x1 = px(std::min(n - 1, o.do_ - godz));
    if (o.do_ - godz >= n) x1 = XP;
    klamra(p, x0, x1, 354, 346);
  }
}

void fakt72(Plotno& p, int x, const std::string& w, const std::string& podpis, bool cz) {
  tekst(p, F_SN72, x, 300, w, cz);
  tekst(p, F_P24B, x, 340, podpis, cz);
}

std::string stopnieTxt(float t) {
  if (std::isnan(t)) return "–";
  long v = std::lround(t);
  return (v < 0 ? std::string("−") + std::to_string(-v) : std::to_string(v)) + "°";
}

void listaSprzetu(Plotno& p, const WejscieSlotu& we, const WynikWymiany& m) {
  static const char* const NAZWY[EL_N] = {"Maska", "Filtr"};
  for (int i = 0; i < EL_N; i++) {
    int kol = i % 2, wiersz = i / 2;
    int cx = kol ? 419 : 39, cy = 244 + 50 * wiersz;
    const ElementWymiany* e = we.sprzet ? &(*we.sprzet)[i] : nullptr;
    std::string nazwa = NAZWY[i];
    std::string wart = e ? wiekElementu(*e, we.dzis) : "–";
    bool doWymiany = i == m.element;
    if (doWymiany) {
      p.kolo(cx, cy, 10, true, true);
      p.kolo(cx, cy, 7, true, false);  // kółko puste, kreska 3 px
      wart = std::string(m.poTerminie ? "po " : "do ") + ddmm(m.termin);
    } else {
      p.kolo(cx, cy, 11, true, true);
    }
    tekst(p, F_P24B, cx + 23, cy + 8, nazwa);
    tekstPrawo(p, F_P24B, kol ? 768 : 388, cy + 8, wart);
  }
}

// Pas doby 00–24: zwykle od y 262, 50 px; przy linii uwagi (ciasno) od 294, 30 px, `teraz` nad pasem na 288,
// podpisy godzin na 348 (bloki nad nim przesunięte o wysokość linii), wiersz `wschód` / `dzień` bez zmian na 392.
void osDniaINocy(Plotno& p, const WynikSlonca& s, int minuta, bool ciasno, int przes = 0) {
  double k = W / 1440.0;
  // przy linii wzorca A (przes = 12) pas, `teraz` i podpisy godzin o tyle niżej; wiersz `wschód` zostaje na 392
  const int y0 = ciasno ? 294 + przes : 262, h = ciasno ? 30 : 50, yTeraz = ciasno ? 288 + przes : 252,
            yPodp = ciasno ? 348 + przes : 338;
  int xw = X + (int)std::lround(s.wschod * k), xz = X + (int)std::lround(s.zachod * k);
  p.prostokat(X, y0, xw - X, h, true);
  p.ramka(xw, y0 + 1, xz - xw, h - 2, 2);
  p.prostokat(xz, y0, XP - xz, h, true);
  if (minuta >= 0) {
    int xt = X + (int)std::lround(minuta * k);
    przerywanaPion(p, xt, ciasno ? 272 + przes : 236, y0 + h, 6, 4, 3);
    // podpis `teraz` z prawej znacznika, a przy prawym brzegu z lewej (nie wychodzi poza margines)
    int wt = szer(F_P22B, "teraz");
    tekst(p, F_P22B, xt + 8 + wt > XP ? xt - 8 - wt : xt + 8, yTeraz, "teraz");
  }
  tekst(p, F_P22B, X, yPodp, "00:00");
  for (int g : {6, 12, 18}) {
    std::string e = godzinaOsiSlotu(g);
    int x = X + (int)std::lround(g * 60 * k);
    tekst(p, F_P22B, x - szer(F_P22B, e) / 2, yPodp, e);
  }
  tekstPrawo(p, F_P22B, XP, yPodp, "24:00");
  char b[16];
  snprintf(b, sizeof b, "%d:%02d", s.wschod / 60, s.wschod % 60);
  tekst(p, F_P24B, X, 392, std::string("wschód ") + b);
  tekstPrawo(p, F_P24B, XP, 392, "dzień " + godzH(s.dlugosc));
}

// linia CO2 nocy (artboardy SK01, SK13 w szablonie A): y 262–335, przerywana na 1000 ppm, podpisy pod osią
void liniaCo2(Plotno& p, const TrescKontekstu& k) {
  const std::vector<int>& v = k.linia;
  if (v.size() < 2) return;
  int lo = CO2_PROG, hi = CO2_PROG;
  for (int x : v) { lo = std::min(lo, x); hi = std::max(hi, x); }
  lo -= 100;
  hi += 100;
  auto py = [&](int x) { return 335 - (int)std::lround((double)(x - lo) / (hi - lo) * 73); };
  auto px = [&](size_t i) { return X + (int)std::lround((double)i * W / (double)(v.size() - 1)); };
  for (size_t i = 1; i < v.size(); i++) gruba(p, px(i - 1), py(v[i - 1]), px(i), py(v[i]), 3);
  przerywanaPoz(p, X, XP, py(CO2_PROG), 8, 6, 2);
  tekst(p, F_P22B, X, 376, k.podpisLewo);
  tekstPrawo(p, F_P22B, XP, 376, k.podpisPrawo);
}

}  // namespace

std::string naglowekSlotu(const std::string& dzis) {
  int dt = dzienTygodnia(dzis);
  if (dt < 0) return "";
  return std::string(DNI_KROTKO[dt]) + " " + ddmm(dzis);
}

void rysujSlotDzienny(Plotno& p, const Naglowek& n, const WyborSlotu& w, const TekstySlotu& t, const WejscieSlotu& we,
                      const StopkaDzienna& s) {
  p.prostokat(0, 0, 800, 480, false);
  bool negatyw = w.ekran == EkranSlotu::SK39;
  if (negatyw) p.prostokat(0, 0, 800, 434, true);
  bool cz = !negatyw;
  naglowek(p, n, cz);
  int dy = 0;
  // linia uwagi jak na każdym ekranie danych (liniaUwagiEkranu(), R-EKR-52, R-EKR-62): w negatywie S-K-39 ostrzeżenie
  // na białym pasku z `!`, informacja w białej ramce bez `!`
  dy = liniaUwagiEkranu(p, we.linia, we.liniaA, we.liniaOstrzezenie, negatyw);
  etykieta(p, 94 + dy, t.etykieta, cz);
  if (!t.godzina.empty()) blokGodziny(p, t, cz, dy);
  else blokTytulu(p, t, cz, dy);
  int godz = we.minuta >= 0 ? we.minuta / 60 : 0;
  // Z linią uwagi (R-KON-63) wykresy S-K-33, S-K-36 i pas doby S-K-37 zostają w wersji niższej (słupki do 66 px,
  // pas 30 px), bo pod blokiem godziny albo tytułem z jednolinijkowym powodem jest na nie miejsce nad krokiem;
  // ustępują tylko przy powodzie w 2 liniach pod tytułem B3 (tekst sięgałby pola wykresu). Pozostałe ekrany
  // (S-K-34, S-K-35, S-K-39) mają przy linii uwagi sam tekst, jak dotąd.
  if (dy > 0) {
    bool zmiesci = !t.godzina.empty() || zawin(F_P28B, t.powod, W).size() <= 1;
    const int przes = przesuniecieLinii(dy);  // linia wzorca A: wykres niższy o różnicę wysokości linii
    switch (w.ekran) {
      case EkranSlotu::SK33:
        if (zmiesci) wizualizacjaOkna(p, w.okno, godz, we.przyLozku, true, przes);
        break;
      case EkranSlotu::SK36:
        if (zmiesci) slupki(p, w.pylek.wartosci, w.pylek.wysoko, godz, std::string("pyłek ") + PYLEK_DOPELNIACZ[w.pylek.rodzaj], 1,
                            WYS_CIASNA - przes);
        break;
      case EkranSlotu::SK37:
        if (zmiesci) {
          osDniaINocy(p, w.slonce, we.minuta, true, przes);
        } else {
          char b[16];
          snprintf(b, sizeof b, "%d:%02d", w.slonce.wschod / 60, w.slonce.wschod % 60);
          tekst(p, F_P24B, X, 392, std::string("wschód ") + b);
          tekstPrawo(p, F_P24B, XP, 392, "dzień " + godzH(w.slonce.dlugosc));
        }
        break;
      default: break;
    }
    if (!t.krok.empty()) tekst(p, F_P28B, X, 418, t.krok, cz);
    stopka(p, s);
    return;
  }
  switch (w.ekran) {
    case EkranSlotu::SK33: wizualizacjaOkna(p, w.okno, godz, we.przyLozku, false); break;
    case EkranSlotu::SK34: {
      const WynikJutra& j = w.jutro;
      char o[24];  // zapas na pełny int (gcc na macOS zgłasza tu -Wformat-truncation)
      snprintf(o, sizeof o, "o\xC2\xA0%d:00", j.godzinaTMin >= 0 ? j.godzinaTMin : 7);
      fakt72(p, 28, stopnieTxt(j.tMin), o, true);
      fakt72(p, 276, calkowita(j.opadMax) + "%", "opad 6:00–9:00", true);
      fakt72(p, 524, calkowita(j.porywyMax), "porywy km/h", true);
      break;
    }
    case EkranSlotu::SK35: listaSprzetu(p, we, w.wymiana); break;
    case EkranSlotu::SK36: {
      std::vector<bool> cz2 = w.pylek.wysoko;
      slupki(p, w.pylek.wartosci, cz2, godz, std::string("pyłek ") + PYLEK_DOPELNIACZ[w.pylek.rodzaj], 1);
      break;
    }
    case EkranSlotu::SK37: osDniaINocy(p, w.slonce, we.minuta, false); break;
    case EkranSlotu::SK39:
      fakt72(p, 28, calkowita(we.drzemka.wyciekP95), "wyciek l/min", false);
      fakt72(p, 276, calkowita(WYCIEK_PROG_SK10), "próg", false);
      fakt72(p, 524, godzH(we.drzemka.minut), "drzemka", false);
      break;
    default: break;
  }
  if (!t.krok.empty()) tekst(p, F_P28B, X, 418, t.krok, cz);
  stopka(p, s);
}

void rysujObrazekDnia(Plotno& p, const ObrazPbm& o, const StopkaDzienna& s) {
  p.prostokat(0, 0, 800, 480, false);
  int x0 = (800 - o.w) / 2, y0 = (434 - o.h) / 2;
  int wb = (o.w + 7) / 8;
  for (int y = 0; y < o.h; y++)
    for (int x = 0; x < o.w; x++)
      if (o.bity[(size_t)y * wb + x / 8] & (0x80 >> (x % 8))) p.piksel(x0 + x, y0 + y, true);
  stopka(p, s);
}

void rysujEkranKontekstu(Plotno& p, const Naglowek& n, const TrescKontekstu& k, const std::string& sl,
                         const std::string& sp) {
  p.prostokat(0, 0, 800, 480, false);
  naglowek(p, n, true);
  int dy = liniaUwagiEkranu(p, k.t.linia, k.t.liniaA, k.t.liniaOstrzezenie, false);
  etykieta(p, 94 + dy, k.t.etykieta, true);
  tekst(p, F_SB44, X, 145 + dy, k.t.tytul);
  auto l = zawin(F_P28B, k.t.powod, W);
  for (size_t i = 0; i < l.size() && i < 2; i++) tekst(p, F_P28B, X, 195 + dy + 34 * (int)i, l[i]);
  if (dy > 0) {  // z linią uwagi wizualizacja ustępuje
  } else if (!k.linia.empty()) {
    liniaCo2(p, k);
  } else if (!k.pary.empty()) {  // pary liczb (S-K-02) jak fakty S-K-34
    int x = X;
    for (auto& f : k.pary) {
      tekst(p, F_SN72, x, 320, f.w);
      tekst(p, F_P24B, x, 358, f.p);
      x += 248;
    }
  }
  if (!k.t.krok.empty()) tekst(p, F_P28B, X, 418, k.t.krok);
  StopkaDzienna s{sl, sp};
  if (!sl.empty() || !sp.empty()) stopka(p, s);
}
