// SPDX-License-Identifier: GPL-3.0-or-later
// Testy
// podpisy osi na pełnym ekranie S3 zdarzeń E (pkt 1), `Ocena: częściowa` na pustym ekranie „Otwarte sprawy”
// (pkt 2), słowo trendu wsparcia PS w K1 (pkt 12), tytuły L-07 w CPAP i AutoSet (pkt 10)
// i O-09 (pkt 11). Dane syntetyczne. Uruchomienie: tools/prototyp/testy.sh (program testy_trend_ps_podpisy_osi).
#include <cmath>
#include <functional>
#include <cstdio>
#include <string>
#include <vector>

#include "dzien.h"
#include "ekrany.h"
#include "ekrany_zdarzen.h"
#include "infekcja.h"
#include "kokpit.h"
#include "noc.h"
#include "reguly.h"
#include "stany.h"
#include "sprawy.h"
#include "wspolne.h"
#include "zdarzenia_ekrany_e.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE(a, b)                                                                                              \
  do {                                                                                                           \
    std::string a_ = (a), b_ = (b);                                                                              \
    if (a_ == b_) ok++;                                                                                          \
    else { zle++; printf("BŁĄD %s:%d: %s = „%s”, ma być „%s”\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } \
  } while (0)

using sprawy::SprawaL;

static const char* const E_EKRANY[] = {"E-30", "E-31/obserwuj", "E-31/lekarz", "E-33", "E-36/obserwuj", "E-36/lekarz",
                                       "E-23", "E-22", "E-21", "E-10/problem", "E-12", "E-13", "E-14"};

// ---------- pkt 1,: podpisy osi S3 (6.4, tabela „Podpisy osi na pełnym ekranie S3”) ----------
static void testPodpisyOsi() {
  for (const char* kl : E_EKRANY) {
    for (bool dl : {false, true}) {
      SprawaL s = b14::sprawaE(kl, dl);
      bool jest = !s.podpisLewo.empty() && !s.podpisPrawo.empty();
      SPRAWDZ(jest);
      if (!jest) printf("    %s bez podpisów osi\n", kl);
      // oba podpisy w kroju (22 px, 600) obok siebie w 744 px z odstępem co najmniej 24 px (5.4.3)
      int w = szer(F_P22B, s.podpisLewo) + 24 + szer(F_P22B, s.podpisPrawo);
      SPRAWDZ(w <= 744);
      if (w > 744) printf("    %s: podpisy %d px > 744 px\n", kl, w);
      // podpis opisuje pomiar: zawsze liczba nocy wykresu i znaczenie linii przerywanej (bez oceny)
      SPRAWDZ(s.podpisLewo.find("30 nocy") != std::string::npos);
      SPRAWDZ(s.podpisPrawo.compare(0, 19, "linia przerywana = ") == 0);
    }
  }
  // próg z progi.csv (przez `{prog:…}`), norma z danych zdarzenia
  ROWNE(b14::sprawaE("E-30").podpisLewo, "AHI · 30 nocy");
  ROWNE(b14::sprawaE("E-30").podpisPrawo, "linia przerywana = 5 / h");
  ROWNE(b14::sprawaE("E-31/lekarz").podpisPrawo, "linia przerywana = zwykle 1,2");
  ROWNE(b14::sprawaE("E-33").podpisLewo, "bezdechy centralne / h · 30 nocy");
  ROWNE(b14::sprawaE("E-23").podpisPrawo, "linia przerywana = 27 l/min");
  ROWNE(b14::sprawaE("E-22").podpisLewo, "duży wyciek, min na noc · 30 nocy");
  ROWNE(b14::sprawaE("E-22").podpisPrawo, "linia przerywana = 30 min");
  ROWNE(b14::sprawaE("E-21").podpisLewo, "wyciek l/min · 30 nocy");
  ROWNE(b14::sprawaE("E-10/problem").podpisPrawo, "linia przerywana = 15 min");
  ROWNE(b14::sprawaE("E-12").podpisPrawo, "linia przerywana = 4 h");
  ROWNE(b14::sprawaE("E-13", true).podpisPrawo, "linia przerywana = zwykle 12 h 59");
  ROWNE(b14::sprawaE("E-14").podpisLewo, "przerwy w nocy · 30 nocy");
  // E-36: słowo sygnału ciśnienia trybu (4.11)
  ROWNE(b14::sprawaE("E-36/obserwuj").podpisLewo, "EPAP w szczytach nocy · 30 nocy");
  ROWNE(b14::sprawaE("E-36/obserwuj", false, reguly::Tryb::CPAP).podpisLewo, "Ciśnienie w szczytach nocy · 30 nocy");
  // pełny ekran S3 dostaje podpisy z zestawu
  DaneDnia d;
  DaneWzorca w = zdarzenia::trescZdarzenia(b14::sprawaE("E-23"), d);
  ROWNE(w.podpisLewo, "wyciek l/min · 30 nocy");
  ROWNE(w.podpisPrawo, "linia przerywana = 27 l/min");
}

// ---------- pkt 2, pusty ekran „Otwarte sprawy” (R-EKR-50) ----------
static void testOcenaCzesciowa() {
  sprawy::WejscieSpraw ws;
  ws.dzis = "2026-11-05";
  sprawy::KartaSprawy k = sprawy::kartaPusta(ws);
  ROWNE(k.ocena, "częściowa");  // „nie oceniam” nie znaczy „w porządku” (ADR-0029 pkt 7)
  ROWNE(k.dopisek, "Części rzeczy jeszcze nie oceniam.");
  // przy ocenie niepełnej tytuł i powód nie twierdzą, że spraw nie ma
  ROWNE(k.tytul, "Nie widzę otwartych spraw.");
  ROWNE(k.powod, "W tym, co oceniam, nic nie jest w toku.");
  DaneSpraw d = trescSpraw({}, 1, ws);
  ROWNE(d.ocena, "częściowa");
  ROWNE(d.dopisek, "Części rzeczy jeszcze nie oceniam.");
  SPRAWDZ(d.n == 0);
  ws.ocenaPelna = true;
  k = sprawy::kartaPusta(ws);
  ROWNE(k.ocena, "pełna");
  SPRAWDZ(k.dopisek.empty());
  ROWNE(k.tytul, "Brak otwartych spraw.");
  ROWNE(k.powod, "Nie ma spraw w toku.");
  ws.ostatniaNiepewna = true;
  k = sprawy::kartaPusta(ws);
  ROWNE(k.ocena, "ograniczona: ostatnia noc z dużym wyciekiem");
  SPRAWDZ(k.dopisek.empty());
  ROWNE(k.tytul, "Nie widzę otwartych spraw.");
  // dopisek mieści się w szerokości treści (744 px) w kroju wiersza wartości (P24 600)
  SPRAWDZ(szer(F_P24S, "Części rzeczy jeszcze nie oceniam.") <= 744);
}

// ---------- pkt 12,: słowo trendu wsparcia PS w K1 (R-KOK-16, Δ 2 cmH2O jak EPAP) ----------
static std::vector<Noc> historiaPs(float stare, float nowe) {
  std::vector<Noc> h;
  for (int i = 0; i < 30; i++) {
    Noc n;
    n.data = dodajDni("2026-10-06", i);
    n.m01 = 420;
    n.wazna = true;
    n.ahi = 1.0f;
    n.wp95 = 10;
    n.emed = 7.0f;
    n.ep95 = 8.0f;
    n.psmed = i < 23 ? stare : nowe;
    n.psp95 = n.psmed + 1;
    n.zrodlo = 'K';
    h.push_back(n);
  }
  return h;
}

static std::string slowoPs(float stare, float nowe) {
  std::vector<Noc> h = historiaPs(stare, nowe);
  Norma nm = liczNorme(h, h.back().data, {});
  DaneK1 k = budujK1(h, h.back().data, nm, {});
  for (int i = 0; i < k.wierszy; i++)
    if (std::string(k.w[i].nazwa) == "Wsparcie") return k.w[i].slowo;
  return "brak wiersza";
}

static void testTrendPs() {
  ROWNE(slowoPs(5.0f, 6.0f), "rośnie");  // różnica 1,0 = 0,5 × 2 cmH2O
  ROWNE(slowoPs(5.0f, 5.9f), "stabilnie");
  ROWNE(slowoPs(6.0f, 5.0f), "spada");
  ROWNE(slowoPs(5.0f, 10.0f), "rośnie");
  SPRAWDZ(std::fabs(TREND_DELTA_PS - 2.0) < 1e-9);  // R-KOK-16.delta_ps w progi.csv
}

// ---------- pkt 10 i 11: tytuły L-07 (CPAP, AutoSet) i O-09 w zestawie ----------
static void testTytuly() {
  const reguly::Zestaw& z = b14::zestaw();
  auto t = z.teksty.find("L-07.tytul");
  SPRAWDZ(t != z.teksty.end());
  if (t != z.teksty.end()) ROWNE(t->second.tekst, "Ciśnienie rośnie od tygodni");
  bool wiersz = false;
  for (auto& k : z.katalog)
    if (k.id == "L-07" && k.wariant == "cpap")
      for (auto& id : k.teksty) wiersz = wiersz || id == "L-07.tytul";
  SPRAWDZ(wiersz);
  auto o = z.teksty.find("O-09.tytul");
  SPRAWDZ(o != z.teksty.end());
  if (o != z.teksty.end()) ROWNE(o->second.tekst, "Pora startu a twoje noce");
}

// ---------- koniec karencji R-INF-04.karencja do logu panelu ----------
static void testKoniecKarencji() {
  infekcja::Okres o;
  o.od = "2026-11-26";
  o.koniec = "2026-12-09";  // limit 14 nocy (R-INF-04)
  o.zakonczony = true;
  std::vector<infekcja::Okres> v = {o};
  ROWNE(infekcja::koniecKarencji(v, "2026-12-09"), "");  // ostatni dzień okresu: jeszcze okres, nie karencja
  ROWNE(infekcja::koniecKarencji(v, "2026-12-10"), "2026-12-23");
  ROWNE(infekcja::koniecKarencji(v, "2026-12-23"), "2026-12-23");
  ROWNE(infekcja::koniecKarencji(v, "2026-12-24"), "");
  SPRAWDZ(infekcja::karencja(v, "2026-12-15") && !infekcja::karencja(v, "2026-12-24"));
  v[0].koniecPoprawa = true;  // koniec poprawą (R-INF-06): bez karencji
  ROWNE(infekcja::koniecKarencji(v, "2026-12-10"), "");
  v[0].koniecPoprawa = false;
  v[0].zakonczony = false;  // okres trwa
  ROWNE(infekcja::koniecKarencji(v, "2026-12-10"), "");
}

// ---------- teksty E-13, E-12, E-10, E-14 z prawdziwego silnika reguł ----------
// (jak test/testy_trend_ps_podpisy_osi_brzegowe.cpp: historia 90 nocy AutoSet, ostatnie 10 nocy zmienia `f`; ocen() → sprawy →
// trescZdarzenia(), bez podstawianych zmiennych)
static std::vector<Noc> historiaSilnika(const std::function<void(Noc&, int)>& f) {
  std::vector<Noc> h;
  for (int i = 0; i < 90; i++) {
    Noc n;
    n.data = dodajDni("2026-08-07", i);
    n.wazna = true;
    double j = (i % 5) * 0.1;
    n.m01 = 420 + (i % 5) * 6;
    n.ahi = (float)(1.0 + j);
    n.cmed = 9;
    n.cp95 = (float)(10 + j);
    n.wmed = 4;
    n.wp95 = (float)(10 + 2 * j);
    n.oa = 0.3f;
    n.ca = (float)(0.5 + j);
    n.h = 0.3f;
    n.m13 = (float)(15 + j);
    n.m17 = 1;
    n.m08 = 2;
    n.ustawienia = "AutoSet/5/12";
    n.tryb = "AutoSet";
    if (i >= 80) f(n, i - 80);
    h.push_back(n);
  }
  return h;
}

struct EkranSilnika {
  bool jest = false;
  SprawaL s;
  DaneWzorca w;
};

static EkranSilnika ekranSilnika(const std::string& klucz, const std::function<void(Noc&, int)>& f) {
  static std::vector<Noc> h;
  static Stany st;
  h = historiaSilnika(f);
  std::string dzis = h.back().data;
  const reguly::Zestaw& z = b14::zestaw();
  st = liczStany(h, dzis, {});
  reguly::Kontekst k;
  k.historia = &h;
  k.dzis = dzis;
  k.norma = &st.norma;
  k.stany = &st;
  k.zestaw = &z;
  k.trybBiezacy = reguly::Tryb::AUTOSET;
  std::vector<reguly::Wynik> w = reguly::ocen(z, k);
  reguly::Pamiec p;
  EkranSilnika e;
  for (auto& s : sprawy::sprawyOtwarte(z, w, p, b14::funkcjaTekstu(reguly::Tryb::AUTOSET))) {
    if (s.klucz != klucz || s.wykluczona) continue;
    DaneDnia d;
    d.dzis = dzis;
    d.hist = &h;
    d.stany = &st;
    d.norma = st.norma;
    e.jest = true;
    e.s = s;
    e.w = zdarzenia::trescZdarzenia(s, d);
  }
  if (!e.jest) printf("    %s: brak ekranu S3 z silnika\n", klucz.c_str());
  return e;
}

static void testTekstySilnika() {
  // E-13: czas z maską `7 h 12` (mediana normy 432 min), `{x}` = średnia z 5 ostatnich nocy ważnych (6.4)
  EkranSilnika e13 = ekranSilnika("E-13", [](Noc& n, int k) { n.m01 = k < 8 ? 280 : 300; });
  SPRAWDZ(e13.jest);
  ROWNE(e13.s.powod, "Średnio 4 h 48, zwykle 7 h 12 (ostatnie 5 nocy).");  // (280·3 + 300·2) / 5 = 288 min
  ROWNE(e13.w.podpisPrawo, "linia przerywana = zwykle 7 h 12");
  // E-14: przerwy jako liczba całkowita
  EkranSilnika e14 = ekranSilnika("E-14", [](Noc& n, int) { n.m17 = 6; });
  SPRAWDZ(e14.jest);
  ROWNE(e14.s.powod, "Przerwy w nocy: 6, zwykle 1.");
  ROWNE(e14.w.podpisPrawo, "linia przerywana = zwykle 1");
  // E-12: 10 krótkich nocy z 30 (20 ponad 4 h, cel 21)
  EkranSilnika e12 = ekranSilnika("E-12", [](Noc& n, int) { n.m01 = 200; n.wazna = false; });
  SPRAWDZ(e12.jest);
  SPRAWDZ(e12.s.powod.find(",0") == std::string::npos && e12.s.powod.find("Ponad 4 h tylko w ") == 0);
  ROWNE(e12.w.podpisPrawo, "linia przerywana = 4 h");
  printf("  E-12 z silnika: %s\n", e12.s.powod.c_str());
  // E-10: 2 noce bez terapii
  EkranSilnika e10 = ekranSilnika("E-10/problem", [](Noc& n, int k) {
    if (k >= 8) { n.m01 = 5; n.wazna = false; }
  });
  SPRAWDZ(e10.jest);
  ROWNE(e10.s.tytul, "Noce bez terapii: 2");
  SPRAWDZ(e10.s.powod.find(",0") == std::string::npos);
  ROWNE(e10.w.podpisPrawo, "linia przerywana = 15 min");
  printf("  E-10 z silnika: %s / %s\n", e10.s.tytul.c_str(), e10.s.powod.c_str());
}

int main() {
  testPodpisyOsi();
  testOcenaCzesciowa();
  testTrendPs();
  testTytuly();
  testKoniecKarencji();
  testTekstySilnika();
  printf("testy_trend_ps_podpisy_osi: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
