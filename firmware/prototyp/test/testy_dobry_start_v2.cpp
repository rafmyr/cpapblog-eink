// SPDX-License-Identifier: GPL-3.0-or-later
// Testy programu „Dobry start” w wersji 2 (spec 5.8.14, R-DS-54…R-DS-71, ADR-0049, ADR-0051): fazy i doba programu,
// do dwóch porad na dobę w starcie, pochwały z danych nocy, znaczniki danych, kod QR i rząd faktów na ekranie, formularz
// 12 tygodni, migracja pliku stanu wersji 1, kompletność planu; scenariusze 90 dób na nocach z atrapy (zwykły, start
// z kłopotami co noc, same dobre noce, przerwa w starcie, migracja w środku adaptacji, bez nawilżacza, ASV, „tylko na
// telefonie”, QR przy najdłuższym adresie). Uruchomienie: tools/prototyp/testy.sh.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "dobry_start.h"
#include "ekrany.h"
#include "ekrany_kontekst.h"
#include "formularz.h"
#include "magazyn.h"
#include "qr.h"
#include "reguly.h"
#include "sekrety.h"
#include "wspolne.h"

using namespace dobry_start;

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

static const std::string START = "2026-12-01";
static const Katalog& K() { return katalogWbudowany(); }
static Ustawienia wl() {
  Ustawienia u;
  u.program = true;
  return u;
}
static std::string dataDoby(int n) { return dodajDni(START, n - 1); }
static Doba db(int n, std::set<std::string> tok = {}, Tryb t = Tryb::CPAP, bool terapia = true) {
  Doba d;
  d.dzis = dataDoby(n);
  d.tryb = t;
  d.tokeny = tok;
  d.nocZTerapia = terapia;
  return d;
}
static const Porada* P(const std::string& id) { return K().porada(id); }
static std::string id(const WyborPorady& w) { return w.porada ? w.porada->id : ""; }
// rano i wieczorem jednej doby: (ID rano, ID wieczorem)
static std::pair<std::string, std::string> doba2(Stan& s, const Ustawienia& u, const Doba& d) {
  WyborPorady r = wybierz(K(), s, u, d);
  if (r.porada) zapiszPokazanie(K(), s, r, d.dzis, r.tylkoWieczor, r.telefon);
  WyborPorady e = wybierzWieczor(K(), s, u, d);
  std::string w;
  if (e.porada && e.porada->faza == Faza::START_WIECZOR && e.zrodlo == Zrodlo::PLAN && !e.narysowana) {
    zapiszPokazanie(K(), s, e, d.dzis, true, e.telefon);
    w = e.porada->id;
  }
  return {id(r), w};
}
static int progMin(const char* p) {
  const reguly::Prog* x = reguly::zestawWbudowany().prog(p, reguly::Tryb::NIEZNANY);
  return x ? (int)std::lround(x->jednostka == "h" ? x->wartosc * 60 : x->wartosc) : 0;
}
static ProgiPochwal progi() {
  ProgiPochwal p;
  p.terapia = progMin("R-SKI-27.noc_z_terapia_m01");
  p.m01Min = progMin("R-DANE-13.m01_min");
  p.wyciekUwaga = progMin("S-K-10.uwaga_o_masce");
  p.noceProblem = progMin("E-10.noce_problem");
  return p;
}
static int seriaNocy(const std::vector<NocProgramu>& n, const std::string& dzis, int prog) {
  int s = 0;
  for (int i = 0; i < 400; i++) {
    std::string d = dodajDni(dzis, -i);
    bool jest = false;
    for (auto& x : n) jest = jest || (x.data == d && x.m01 >= prog);
    if (!jest) break;
    s++;
  }
  return s;
}
// tokeny poranka z nocy (pochwały, M-20) i dodatkowe tokeny reguł
static std::set<std::string> tokeny(const std::vector<NocProgramu>& n, const std::string& dzis,
                                    std::set<std::string> t = {}) {
  ProgiPochwal p = progi();
  int s = seriaNocy(n, dzis, p.m01Min);
  tokenyPochwal(n, dzis, s, p, t);
  t.insert("M-20/" + std::to_string(s));
  return t;
}

// ---------- R-DS-54 … R-DS-71 ----------

static void testR_DS_54_fazy() {
  SPRAWDZ(K().etap(1) == Etap::START && K().etap(K().koniecStartu) == Etap::START);
  SPRAWDZ(K().etap(K().koniecStartu + 1) == Etap::ADAPTACJA && K().etap(K().koniecAdaptacji) == Etap::ADAPTACJA);
  SPRAWDZ(K().etap(K().koniecAdaptacji + 1) == Etap::UTRWALENIE && K().etap(K().miejsc) == Etap::UTRWALENIE);
  SPRAWDZ(K().etap(K().miejsc + 1) == Etap::KONIEC);
  SPRAWDZ(K().koniecStartu == 10 && K().koniecAdaptacji == 42 && K().miejsc == 84);  // dane z 22.11.2026
  Stan s;
  Ustawienia u = wl();
  std::map<Etap, std::set<Faza>> fazy;
  for (int n = 1; n <= K().miejsc; n++) {
    auto x = doba2(s, u, db(n));
    for (auto& i : {x.first, x.second})
      if (!i.empty() && P(i)->rodzaj == Rodzaj::PLAN) fazy[K().etap(n)].insert(P(i)->faza);
  }
  SPRAWDZ(fazy[Etap::START] == (std::set<Faza>{Faza::START_RANO, Faza::START_WIECZOR}));
  SPRAWDZ(fazy[Etap::ADAPTACJA] == std::set<Faza>{Faza::ADAPTACJA});
  SPRAWDZ(fazy[Etap::UTRWALENIE] == std::set<Faza>{Faza::UTRWALENIE});
  // przerwa 5 dób w dobie 8 → faza start kończy się w dobie 15 od włączenia
  Stan t;
  for (int n = 1; n <= 7; n++) doba2(t, u, db(n));
  doba2(t, u, db(8, {"E-10/linia"}, Tryb::CPAP, false));
  for (int n = 9; n <= 13; n++) doba2(t, u, db(n, {"E-10/linia", "E-10/problem"}, Tryb::CPAP, false));
  for (int n = 14; n <= 16; n++) doba2(t, u, db(n));
  SPRAWDZ(t.przerwaDni == 5);
  SPRAWDZ(K().etap(dobaProgramu(t, dataDoby(15))) == Etap::START && K().etap(dobaProgramu(t, dataDoby(16))) == Etap::ADAPTACJA);
}

static void testR_DS_55_dwiePoradyWStarcie() {
  Stan s;
  Ustawienia u = wl();
  doba2(s, u, db(1));
  doba2(s, u, db(2));
  auto d3 = doba2(s, u, db(3));
  SPRAWDZ(d3.first == "DS-D05" && d3.second == "DS-D03");
  int ileP = 0, rano = 0;
  for (auto& x : s.p)
    if (x.data == dataDoby(3)) {
      ileP++;
      rano += x.pora == "dzien";
    }
  SPRAWDZ(ileP == 2 && rano == 1);
  // doba 2 po nocy 1 z 5 h → rano DS-P01, wieczorem DS-S02
  Stan t;
  std::vector<NocProgramu> noce;
  doba2(t, u, db(1));
  noce.push_back({dataDoby(2), 300, 0, 5});
  auto d2 = doba2(t, u, db(2, tokeny(noce, dataDoby(2))));
  SPRAWDZ(d2.first == "DS-P01" && d2.second == "DS-S02");
  // restart wieczorem: ta sama porada wieczorna z dziennika, bez drugiego wpisu
  Stan r = stanZWierszy(wierszeStanu(t), K());
  WyborPorady e = wybierzWieczor(K(), r, u, db(2));
  SPRAWDZ(id(e) == "DS-S02" && e.narysowana);
  // poza startem wieczorem ta sama porada co w bloku
  Stan a;
  for (int n = 1; n <= 12; n++) doba2(a, u, db(n));
  WyborPorady b = wybierz(K(), a, u, db(13));
  SPRAWDZ(id(wybierzWieczor(K(), a, u, db(13))) == id(b));
  // W0 wieczorem w starcie: bez porady, porada wieczorna czeka do następnego wieczoru
  Stan w0;
  doba2(w0, u, db(1));
  Doba d = db(2);
  d.w0 = true;
  SPRAWDZ(id(wybierzWieczor(K(), w0, u, d)).empty());
  SPRAWDZ(doba2(w0, u, db(3)).second == "DS-S02");
}

static void testR_DS_56_koniecFazy() {
  // kolejka adaptacji w tyle (D0) → w dobie 42 DS-D42, potem podstawowa DS-D34, potem DS-U01; DS-D38 i DS-D50 `jeszcze nie`
  Stan s;
  Ustawienia u = wl();
  for (int n = 1; n <= 41; n++) {
    Doba d = db(n);
    d.d0 = n > 20 && n % 3 != 0;
    doba2(s, u, d);
  }
  SPRAWDZ(id(wybierz(K(), s, u, db(42))) == "DS-D42");
  zapiszPokazanie(K(), s, wybierz(K(), s, u, db(42)), dataDoby(42), false, false);
  SPRAWDZ(znacznik(K(), s, *P("DS-D38")) == "jeszcze nie" && znacznik(K(), s, *P("DS-D50")) == "jeszcze nie");
  auto d43 = doba2(s, u, db(43));
  SPRAWDZ(d43.first == "DS-D34");
  auto d44 = doba2(s, u, db(44));
  SPRAWDZ(d44.first == "DS-U01");
  // start z 3 dobami przerwy → doba 11 od włączenia jeszcze start
  Stan t;
  for (int n = 1; n <= 4; n++) doba2(t, u, db(n));
  doba2(t, u, db(5, {"E-10/linia"}, Tryb::CPAP, false));
  for (int n = 6; n <= 8; n++) doba2(t, u, db(n, {"E-10/linia", "E-10/problem"}, Tryb::CPAP, false));
  doba2(t, u, db(9));
  SPRAWDZ(K().etap(dobaProgramu(t, dataDoby(11))) == Etap::START);
}

static void testR_DS_57_napisy() {
  auto napisy = [](const std::string& i, Zrodlo z) {
    WyborPorady w;
    w.porada = P(i);
    w.zrodlo = z;
    w.dataNocy = dataDoby(5);
    return opis(K(), Stan(), w);
  };
  OpisPorady a = napisy("DS-S03", Zrodlo::PLAN);
  SPRAWDZ(a.srodekBelki == "start · dzień 4 z 10" && a.etykieta == "na dziś wieczór" && a.tygodni == 12);
  OpisPorady b = napisy("DS-D22", Zrodlo::PLAN);
  SPRAWDZ(b.srodekBelki == "adaptacja · dzień 25 z 42" && b.etykieta == "tydzień 4 z 6" && b.tydzien == 4);
  OpisPorady c = napisy("DS-C04", Zrodlo::PLAN);
  SPRAWDZ(c.srodekBelki == "utrwalenie · tydzień 10 z 12" && c.etykieta == "porada 7 z 12");
  OpisPorady d = napisy("DS-P01", Zrodlo::POCHWALA);
  SPRAWDZ(d.srodekBelki == "po nocy " + ddmm(dataDoby(5)) && d.etykieta == "za ostatnią noc" && d.tydzien == 0);
  OpisPorady e = napisy("DS-D05", Zrodlo::PLAN);
  SPRAWDZ(e.srodekBelki == "start · dzień 3 z 10" && e.etykieta == "na dziś");
  // najdłuższy środek belki mieści się przed polem baterii (jak inne belki: do x 680)
  SPRAWDZ(szer(F_P24B, NAZWA_BELKI) + 28 + szer(F_P22B, "adaptacja · dzień 42 z 42") < 680);
}

static void testR_DS_58_pochwaly() {
  Ustawienia u = wl();
  Stan s;
  std::vector<NocProgramu> noce;
  doba2(s, u, db(1));
  noce.push_back({dataDoby(2), 300, 1, 12});  // noc 1: 5 h, zdjęcia maski, wyciek ponad próg uwagi
  SPRAWDZ(doba2(s, u, db(2, tokeny(noce, dataDoby(2)))).first == "DS-P01");
  noce.push_back({dataDoby(3), 270, 1, 12});  // noc 2: 4 h 30 → DS-P02 (P01 już była)
  SPRAWDZ(doba2(s, u, db(3, tokeny(noce, dataDoby(3)))).first == "DS-P02");
  for (int n = 4; n <= 8; n++) {
    noce.push_back({dataDoby(n), 300, 1, 12});
    doba2(s, u, db(n, tokeny(noce, dataDoby(n))));
  }
  noce.push_back({dataDoby(9), 300, 1, 12});  // 8. noc ≥ 4 h z rzędu (M-20 = 8) → DS-P04 była przy M-20 = 7
  int p04 = 0;
  for (auto& x : s.p) p04 += x.id == "DS-P04";
  SPRAWDZ(p04 == 1 && P("DS-P04")->karta);
  // DS-P03 najwyżej 3 razy i tylko w starcie
  Stan t;
  for (int n = 1; n <= 6; n++) doba2(t, u, db(n, {"pochwala/dluzej"}));
  int p03 = 0;
  for (auto& x : t.p) p03 += x.id == "DS-P03";
  SPRAWDZ(p03 == 3);
  Stan a;
  for (int n = 1; n <= 12; n++) doba2(a, u, db(n));
  SPRAWDZ(id(wybierz(K(), a, u, db(13, {"pochwala/dluzej"}))) != "DS-P03");
  // tokeny z danych nocy
  std::set<std::string> tk;
  tokenyPochwal({{dataDoby(1), 120, 0, 20}, {dataDoby(2), 400, 0, 4}}, dataDoby(2), 2, progi(), tk);
  SPRAWDZ(tk.count("pochwala/dluzej") && tk.count("pochwala/cala_noc") && tk.count("pochwala/szczelniej") &&
          tk.count("pochwala/pierwsze_4h") && !tk.count("pochwala/powrot"));
  std::set<std::string> tp;  // powrót po 2 nocach bez terapii
  tokenyPochwal({{dataDoby(1), 300, 0, 4}, {dataDoby(4), 300, 0, 4}}, dataDoby(4), 1, progi(), tp);
  SPRAWDZ(tp.count("pochwala/powrot"));
}

static void testR_DS_59_jednaPochwala() {
  Ustawienia u = wl();
  Stan s;  // noc 7 spełnia DS-P04 i DS-P07 → DS-P04, DS-P07 następnego ranka, potem przepada
  for (int n = 1; n <= 7; n++) doba2(s, u, db(n));
  SPRAWDZ(doba2(s, u, db(8, {"M-20/7", "pochwala/szczelna"})).first == "DS-P04");
  // powtarzalna pochwała za noc czeka 1 dobę tylko, gdy ta noc też ją spełnia (nie przeczy ostatniej nocy)
  Stan s2 = s;
  SPRAWDZ(doba2(s, u, db(9, {"pochwala/szczelna"})).first == "DS-P07");
  SPRAWDZ(doba2(s2, u, db(9)).first != "DS-P07");
  Stan t;
  for (int n = 1; n <= 7; n++) doba2(t, u, db(n));
  doba2(t, u, db(8, {"M-20/7", "pochwala/szczelna"}));
  SPRAWDZ(P(doba2(t, u, db(9, {"E-11"})).first)->rodzaj == Rodzaj::ZDARZENIE);  // rano porada z danych, pochwała czeka
  SPRAWDZ(doba2(t, u, db(10)).first != "DS-P07");                              // po 1 dobie przepada
  // adaptacja: DS-P07 w dobie 20, DS-P08 w 23 nie staje (1 na 7), DS-P09 w 24 staje (karta serii)
  Stan a;
  for (int n = 1; n <= 19; n++) doba2(a, u, db(n));
  SPRAWDZ(doba2(a, u, db(20, {"pochwala/szczelna"})).first == "DS-P07");
  SPRAWDZ(doba2(a, u, db(23, {"pochwala/szczelniej"})).first != "DS-P08");
  SPRAWDZ(doba2(a, u, db(24, {"M-20/14"})).first == "DS-P09");
  SPRAWDZ(doba2(a, u, db(25)).first != "DS-P08");
}

static void testR_DS_60_problemIZnaczniki() {
  Ustawienia u = wl();
  Stan s;
  doba2(s, u, db(1));
  SPRAWDZ(doba2(s, u, db(2, {"pochwala/pierwsza_noc", "ekran/problem"})).first != "DS-P01");
  SPRAWDZ(doba2(s, u, db(3)).first != "DS-P01");  // nie czeka
  Stan t;
  doba2(t, u, db(1));
  SPRAWDZ(doba2(t, u, db(2, {"pochwala/szczelna", "noc_niepewna"})).first != "DS-P07");
  Znaczniki z;
  z.czas = 312;
  SPRAWDZ(podstawZnaczniki("Ta noc: {czas} z maską.", z) == "Ta noc: 5 h 12 z maską.");
  SPRAWDZ(podstawZnaczniki("{srednio} · {o_ile_dluzej} · {nieznany}", z) == "– · – · {nieznany}");
  z.oIleDluzej = 25;
  SPRAWDZ(podstawZnaczniki("{o_ile_dluzej}", z) == "25 min");
  Znaczniki pusty = znacznikiNocy({}, dataDoby(5), START, 0, progi());
  SPRAWDZ(pusty.srednio < 0 && pusty.nocyZMaska < 0 && pusty.czas < 0);
  std::vector<NocProgramu> noce;
  for (int n = 1; n <= 7; n++) noce.push_back({dataDoby(n), n == 3 ? 0 : 340, 0, 6});
  Znaczniki w = znacznikiNocy(noce, dataDoby(7), START, 4, progi());
  SPRAWDZ(w.nocyZMaska == 6 && w.nocy4h == 6 && w.srednio == 340 && w.czas == 340 && w.wyciek == 6);
}

static void testR_DS_61_bezAhi() {
  for (auto& p : K().porady)
    if (p.karta || p.rodzaj == Rodzaj::POCHWALA) {
      std::string t = K().tekst(p.tytul) + K().tekst(p.tresc) + K().tekst(p.krok) + K().tekst(p.fakty);
      SPRAWDZ(t.find("AHI") == std::string::npos && t.find("w porządku") == std::string::npos);
    }
}

struct PlotnoT : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)(y * 800 + x)] = c;
  }
  int obszar(int x0, int y0, int x1, int y1) const {
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[(size_t)(y * 800 + x)];
    return n;
  }
};
static DanePorady dane(const std::string& i, Zrodlo z = Zrodlo::PLAN) {
  WyborPorady w;
  w.porada = P(i);
  w.zrodlo = z;
  w.dataNocy = dataDoby(3);
  DanePorady d;
  d.o = opis(K(), Stan(), w);
  d.belka = Belka(NAZWA_BELKI, d.o.srodekBelki);
  return d;
}

static void testR_DS_62_63_kodQr() {
  int z = 0, maks = 0;
  for (auto& p : K().porady) {
    if (!p.qr.dl) continue;
    z++;
    maks = std::max(maks, (int)p.qr.dl);
    DanePorady d = dane(p.id, p.rodzaj == Rodzaj::POCHWALA ? Zrodlo::POCHWALA : Zrodlo::PLAN);
    PlotnoT pl;
    SPRAWDZ(rysujPorade(pl, d));  // każdy adres z danych mieści się (wersja ≤ 3)
    SPRAWDZ(pl.obszar(569, 182, 772, 386) > 2000);   // kod w rogu pola treści
    SPRAWDZ(pl.obszar(541, 155, 569, 182) == 0);     // strefa ciszy nad i obok kodu biała
    SPRAWDZ(pl.obszar(600, 415, 772, 434) > 50);     // podpis pod kodem
  }
  printf("  R-DS-62: %d porad z kodem QR, najdłuższy adres %d B\n", z, maks);
  SPRAWDZ(z == 27 && maks <= 42);
  // R-DS-63: adres nie do zakodowania w strefie → bez kodu, treść w 748 px
  DanePorady d = dane("DS-S07");
  d.o.qr = "https://cpapblog.pl/" + std::string(280, 'a');
  PlotnoT pl;
  SPRAWDZ(!rysujPorade(pl, d) && pl.obszar(600, 415, 772, 434) == 0);
  // „tylko na telefonie”: bez kodu
  WyborPorady w;
  w.porada = P("DS-S07");
  w.telefon = true;
  SPRAWDZ(opis(K(), Stan(), w).qr.empty());
}

static void testR_DS_64_fakty() {
  DanePorady d = dane("DS-D07");
  SPRAWDZ(d.o.fakty.size() == 3 && d.o.fakty[0].second == "nocy z maską");
  Znaczniki z;
  z.nocyZMaska = 7;
  z.srednio = 340;
  z.nocy4h = 6;
  for (auto& f : d.o.fakty) f.first = podstawZnaczniki(f.first, z);
  SPRAWDZ(d.o.fakty[0].first == "7 z 7" && d.o.fakty[1].first == "5 h 40" && d.o.fakty[2].first == "6 z 7");
  d.scena = "M06";
  PlotnoT k;
  rysujPorade(k, d);
  SPRAWDZ(k.obszar(28, 266, 518, 296) > 100 && k.obszar(28, 300, 518, 326) > 100);  // liczby nad podpisami
  d.scena.clear();  // karta jako ekran danych: fakty jedną linią pod treścią
  PlotnoT e;
  rysujPorade(e, d);
  SPRAWDZ(e.obszar(28, 192, 776, 400) > 0);
}

static void testR_DS_65_startOdDnia() {
  Ustawienia u = wl();
  u.dzien = 15;
  Stan s;
  std::vector<std::string> v;
  for (int n = 1; n <= 6; n++) v.push_back(doba2(s, u, db(n)).first);
  SPRAWDZ(v == (std::vector<std::string>{"DS-D37", "DS-S07", "DS-D05", "DS-D06", "DS-D09", "DS-D49"}));
  u.dzien = 50;
  Stan t;
  std::string ost;
  for (int n = 1; n <= 12; n++) {
    std::string x = doba2(t, u, db(n)).first;
    if (!x.empty()) ost = x;
    if (x == "DS-C03") break;
  }
  SPRAWDZ(ost == "DS-C03");
  formularz::Wynik w = formularz::waliduj({{"start_dzien", "90"}}, sekrety::Dane(), START);
  SPRAWDZ(w.bledy.count("start_dzien") && w.bledy["start_dzien"] == "Wpisz liczbę od 1 do 84.");
  SPRAWDZ(formularz::waliduj({{"start_dzien", "84"}}, sekrety::Dane(), START).plik["start.dzien"] == "84");
}

static void testR_DS_66_formularz() {
  formularz::Pola p;
  p.start = wl();
  p.startMiejsc = K().miejsc;
  p.startNaglowek = naglowekSekcji(K(), Stan(), p.start, START);
  std::string h = formularz::strona(p, "/u/abc/", "", {});
  SPRAWDZ(h.find("Dobry start: program na pierwsze 12 tygodni") != std::string::npos);
  SPRAWDZ(h.find("Przez pierwsze 10 dni panel pokaże poradę rano i wieczorem, do 6. tygodnia jedną dziennie, a potem dwie "
                 "w tygodniu.") != std::string::npos);
  SPRAWDZ(h.find("start_ponad") == std::string::npos && h.find("max=\"84\"") != std::string::npos);
  SPRAWDZ(h.find("start · dzień 1 z 10") != std::string::npos);
  SPRAWDZ(h.size() <= formularz::REZERWACJA_STRONY);
  // strony „Wszystkie porady”: każda w budżecie, każda porada na którejś stronie
  size_t maks = 0;
  for (Tryb t : {Tryb::CPAP, Tryb::AUTOSET, Tryb::ASV})
    for (int n : {1, 0}) {
      Ustawienia us = wl();
      us.nawilzacz = n;
      std::string wszystko;
      for (int i = 1; i <= formularz::zakladekPorad(K()); i++) {
        std::string st = formularz::stronaPorad(K(), Stan(), us, t, i, "/u/abc/", nullptr, nullptr);
        maks = std::max(maks, st.size());
        wszystko += st;
      }
      for (auto& x : K().porady)
        if (wariant(K(), x.baza, t, n) == &x) SPRAWDZ(wszystko.find(formularz::html(K().tekst(x.tytul))) != std::string::npos);
    }
  printf("  R-DS-66: najdłuższa strona „Wszystkie porady” %zu B (próg %zu B), zakładek %d\n", maks,
         formularz::MAKS_STRONY_PORAD, formularz::zakladekPorad(K()));
  SPRAWDZ(maks <= formularz::MAKS_STRONY_PORAD);
  std::string sp = formularz::stronaPorad(K(), Stan(), wl(), Tryb::CPAP, 2, "/u/abc/", nullptr, nullptr);
  SPRAWDZ(sp.find("Więcej: <a href=\"https://cpapblog.pl/") != std::string::npos && sp.find("<b>Start, wieczór</b>") != std::string::npos);
  SPRAWDZ(naglowekSekcji(K(), Stan(), Ustawienia(), START) == "wyłączony");
}

// plik stanu wersji 1 z kolejką 17 i 16 wpisami P (miejsca 1…16 wersji 1, ID porad z wersji 1)
static std::vector<std::string> stanV1(const std::string& stan = "aktywny") {
  std::vector<std::string> w = {"S;start;" + START, "S;kolejka;17", "S;stan;" + stan, "S;przerwa_dni;0", "S;d0_z_rzedu;0",
                                "S;tryb;CPAP", "S;ponad;0", "S;noce_terapii;15", "S;ankieta_od;" + dataDoby(4),
                                "S;doba;" + dataDoby(16), "S;dzien_ust;1", "S;plan_koniec;0", "K;6;" + dataDoby(3)};
  const char* ids[] = {"DS-D01", "DS-D02", "DS-D03", "DS-D04", "DS-D05", "DS-D06", "DS-D07", "DS-D08",
                       "DS-D09", "DS-D10", "DS-D11", "DS-D12", "DS-D13", "DS-D14", "DS-D45", "DS-D16"};
  for (int i = 0; i < 16; i++) w.push_back(std::string("P;") + dataDoby(i + 1) + ";" + ids[i] + ";plan;;dzien;panel");
  return w;
}

static void testR_DS_67_migracjaV1() {
  // magazyn: plik w wersji 1 → kopia `.v1` bajt w bajt, nagłówek wersji 2, te same wiersze
  Dziennik dz;
  AtrapaPlikow pl(dz);
  pl.montujeSie = pl.montowane = true;
  std::string t = "#cpap-eink dobry_start 1\nrodzaj;a;b;c;d;e;f\n";
  std::vector<std::string> v1 = stanV1();
  for (auto& x : v1) {
    std::string r = x;
    int pol = 1;
    for (char ch : r) pol += ch == ';';
    while (pol++ < 7) r += ";";
    t += r + "\n";
  }
  t += "#koniec " + std::to_string(v1.size()) + "\n";
  pl.pliki["/dane/dobry_start.csv"] = t;
  std::vector<std::string> log;
  Magazyn m(pl, [&](const std::string& l) { log.push_back(l); });
  OdczytPliku o = m.czytaj("dobry_start", "/dane/dobry_start.csv");
  SPRAWDZ(o.stan == StanPliku::DOBRY && o.wersja == 1);
  SPRAWDZ(m.migrujWMiejscu());
  SPRAWDZ(pl.pliki["/dane/dobry_start.csv"].compare(0, 25, "#cpap-eink dobry_start 2\n") == 0);
  bool kopia = false;
  for (auto& kv : pl.pliki) kopia = kopia || kv.first.find("/dane/dobry_start.csv.v1") == 0;
  SPRAWDZ(kopia);  // kopia `.v1` zawsze przy migracji 1 → 2
  OdczytPliku o2 = m.czytaj("dobry_start", "/dane/dobry_start.csv");
  SPRAWDZ(o2.stan == StanPliku::DOBRY && o2.wersja == 2 && o2.wiersze.size() == v1.size());
  // silnik: porady z P pokazane, doba programu z daty startu, faza adaptacja, `start.ponad` pominięty
  Stan s = stanZWierszy(o2.wiersze, K());
  SPRAWDZ(s.bledy.size() == 1 && s.p.size() == 16 && s.start == START);
  Ustawienia u = wl();
  std::string r = doba2(s, u, db(17)).first;
  SPRAWDZ(P(r) && K().etap(dobaProgramu(s, dataDoby(17))) == Etap::ADAPTACJA);
  for (auto& x : s.p)
    if (x.data < dataDoby(17)) SPRAWDZ(x.id != r);
  Stan zak = stanZWierszy(stanV1("zakonczony"), K());
  SPRAWDZ(doba2(zak, u, db(17)).first.empty() && zak.stan == "zakonczony");
  std::string b;
  Ustawienia us;
  SPRAWDZ(ustawienie(us, "start.ponad", "nie", b) && b.find("wycofany") != std::string::npos);
}

static void testR_DS_68_start() {
  Ustawienia u = wl();
  Stan s;
  doba2(s, u, db(1));
  SPRAWDZ(doba2(s, u, db(2, {"S-K-10/opisowy"})).first == "DS-Z01");
  SPRAWDZ(doba2(s, u, db(3, {"S-K-10/opisowy"})).first != "DS-Z01");  // raz na 7 dni (R-DS-08)
  Stan t;  // trzy poranki z rzędu z poradami z danych: trzy porady (bez limitu 2 na 7)
  doba2(t, u, db(1));
  int z = 0;
  for (int n = 2; n <= 4; n++)
    z += P(doba2(t, u, db(n, {n == 2 ? "E-11" : n == 3 ? "S-K-14" : "E-14"})).first)->rodzaj == Rodzaj::ZDARZENIE;
  SPRAWDZ(z == 3);
  Stan a;  // E-30 w dobie 9 → brak DS-Z08; w dobie 12 → najpierw DS-D09 (objaśnienie), potem DS-Z08
  for (int n = 1; n <= 8; n++) doba2(a, u, db(n));
  std::string d9 = doba2(a, u, db(9, {"E-30"})).first;
  SPRAWDZ(d9 != "DS-Z08" && d9 != "DS-D09");
  doba2(a, u, db(10));
  doba2(a, u, db(11));
  SPRAWDZ(doba2(a, u, db(12, {"E-30"})).first == "DS-D09");
  SPRAWDZ(doba2(a, u, db(13)).first == "DS-Z08");
  // DS-S10/opisowy poza startem nie działa
  Stan b;
  for (int n = 1; n <= 12; n++) doba2(b, u, db(n));
  SPRAWDZ(doba2(b, u, db(13, {"S-K-10/opisowy"})).first != "DS-Z01");
}

static void testR_DS_69_utrwalenie() {
  Ustawienia u = wl();
  Stan s;
  for (int n = 1; n <= 42; n++) doba2(s, u, db(n));
  std::vector<int> z;
  for (int n = 43; n <= 49; n++)
    if (!doba2(s, u, db(n)).first.empty()) z.push_back(n);
  SPRAWDZ(z == (std::vector<int>{43, 46}));
  for (int n = 50; n <= 83; n++) doba2(s, u, db(n));
  SPRAWDZ(doba2(s, u, db(84)).first == "DS-U02" && s.planKoniec);
  SPRAWDZ(doba2(s, u, db(85)).first.empty() && s.stan == "zakonczony");
  SPRAWDZ(naglowekSekcji(K(), s, u, dataDoby(85)) == "zakończony");
  // doba nominalna zajęta przez poradę wyzwalaną: porada planu na następną dobę bez porady
  Stan t;
  for (int n = 1; n <= 45; n++) doba2(t, u, db(n));
  SPRAWDZ(P(doba2(t, u, db(46, {"E-11"})).first)->rodzaj == Rodzaj::ZDARZENIE);
  SPRAWDZ(doba2(t, u, db(47)).first == "DS-D36");
}

static void testR_DS_70_wariantTrybu() {
  Ustawienia u = wl();
  Stan s;
  for (int n = 1; n <= 9; n++) doba2(s, u, db(n, {}, Tryb::ASV));
  SPRAWDZ(doba2(s, u, db(10, {}, Tryb::ASV)).second == "DS-S08.asv");
  Stan c;
  for (int n = 1; n <= 9; n++) doba2(c, u, db(n));
  SPRAWDZ(doba2(c, u, db(10)).second == "DS-S08");
  Stan x;  // tryb nieznany: doba bez porady wieczornej z wariantem trybu
  for (int n = 1; n <= 9; n++) doba2(x, u, db(n, {}, Tryb::NIEZNANY));
  std::string w = doba2(x, u, db(10, {}, Tryb::NIEZNANY)).second;
  SPRAWDZ(w.empty() || (P(w)->tryby & TR_WSZYSTKIE));
}

static void testR_DS_71_kompletnosc() {
  int planu = 0;
  std::set<std::pair<int, int>> doby;
  for (auto& p : K().porady)
    if (p.rodzaj == Rodzaj::PLAN) doby.insert({(int)p.faza, p.dzien});
  for (Tryb t : {Tryb::CPAP, Tryb::AUTOSET, Tryb::ASV})
    for (int n : {1, 0})
      for (auto& d : doby) {
        int ile = 0;
        for (auto& p : K().porady)
          if (p.rodzaj == Rodzaj::PLAN && (int)p.faza == d.first && p.dzien == d.second && wariantDoby(K(), p.faza, p.dzien, t, n) == &p) ile++;
        SPRAWDZ(ile == 1);
        planu++;
      }
  SPRAWDZ(doby.size() == 64 && planu == 6 * 64);
  for (auto& p : K().porady)  // pochwały i porady wyzwalane: wariant dla każdej osoby (poza `wymaga=nawilżacz`)
    if (p.rodzaj != Rodzaj::PLAN && p.nawilzacz != 1)
      for (Tryb t : {Tryb::CPAP, Tryb::AUTOSET, Tryb::ASV})
        for (int n : {1, 0}) SPRAWDZ(wariant(K(), p.baza, t, n) != nullptr);
}

// ---------- scenariusze 90 dób ----------
struct Wynik90 {
  int porad = 0, pochwal = 0, planu = 0, wieczornych = 0, maksDzien = 0, telefon = 0, zlyTryb = 0, zNawilzaczem = 0;
  std::string stanKoniec;
};
// noc(n) → noc z poranka doby n (m01 < 0 = brak nocy); dodatkowe tokeny reguł z tok(n)
template <class FN, class FT>
static Wynik90 scenariusz(Stan& s, const Ustawienia& u, Tryb tryb, FN noc, FT tok, int od = 1) {
  Wynik90 w;
  std::vector<NocProgramu> noce;
  ProgiPochwal pp = progi();
  int okno = -100;  // ostatnia pochwała (nie karta) poza startem
  for (int n = od; n <= 90; n++) {
    NocProgramu x = noc(n);
    x.data = dataDoby(n);
    if (x.m01 >= 0) noce.push_back(x);
    Doba d = db(n, tokeny(noce, dataDoby(n), tok(n)), tryb, x.m01 >= pp.terapia);
    auto r = doba2(s, u, d);
    int dzis = 0;
    for (auto& i : {r.first, r.second}) {
      if (i.empty()) continue;
      const Porada* p = P(i);
      dzis++;
      w.porad++;
      if (p->rodzaj == Rodzaj::POCHWALA) {
        w.pochwal++;
        if (!p->karta && K().etap(dobaProgramu(s, d.dzis)) != Etap::START) {
          SPRAWDZ(n - okno >= POCHWALA_OKNO_DNI);
          okno = n;
        }
      }
      if (p->rodzaj == Rodzaj::PLAN) w.planu++;
      if (p->faza == Faza::START_WIECZOR) w.wieczornych++;
      if (!(p->tryby & TR_WSZYSTKIE) && !wariant(K(), p->baza, tryb, u.nawilzacz)) w.zlyTryb++;
      if (p->nawilzacz == 1) w.zNawilzaczem++;
    }
    for (auto& x2 : s.p) w.telefon += x2.data == d.dzis && x2.gdzie == "telefon";
    w.maksDzien = std::max(w.maksDzien, dzis);
    if (dzis > (K().etap(dobaProgramu(s, d.dzis)) == Etap::START ? 2 : 1)) printf("  doba %d: %s / %s\n", n, r.first.c_str(), r.second.c_str());
    SPRAWDZ(dzis <= (K().etap(dobaProgramu(s, d.dzis)) == Etap::START ? 2 : 1));
  }
  w.stanKoniec = s.stan;
  return w;
}
static NocProgramu dobra(int) { return {"", 400, 0, 4}; }
static std::set<std::string> bez(int) { return {}; }

static void testScenariusze90() {
  Ustawienia u = wl();
  {  // zwykły: dobre noce, pochwały nie zalewają, plan pokazany, koniec po 84 dobach
    Stan s;
    Wynik90 w = scenariusz(s, u, Tryb::CPAP, dobra, bez);
    printf("  90 dób, dobre noce: %d porad, %d pochwał, %d planu, %d wieczornych\n", w.porad, w.pochwal, w.planu,
           w.wieczornych);
    SPRAWDZ(w.stanKoniec == "zakonczony" && s.planKoniec);
    SPRAWDZ(w.pochwal >= 4 && w.pochwal <= 17 && w.wieczornych == 10 && w.planu >= 54);
  }
  {  // start z kłopotami co noc (wyciek, krótka noc): rano porady z danych, wieczorem zawsze porada wieczorna
    Stan s;
    Wynik90 w = scenariusz(s, u, Tryb::CPAP, [](int n) { return NocProgramu{"", n <= 10 ? 150 : 400, 2, n <= 10 ? 30 : 5}; },
                           [](int n) { return n <= 10 ? std::set<std::string>{"S-K-10/problem", "E-11", "ekran/problem"}
                                                     : std::set<std::string>{}; });
    SPRAWDZ(w.wieczornych == 10 && w.stanKoniec == "zakonczony");
  }
  {  // przerwa w starcie (noce 5…9 bez maski): faza start wydłużona, po przerwie pochwała powrotu
    Stan s;
    Wynik90 w = scenariusz(s, u, Tryb::CPAP, [](int n) { return n >= 5 && n <= 9 ? NocProgramu{"", 0, -1, -1} : dobra(n); },
                           [](int n) { return n == 5 ? std::set<std::string>{"E-10/linia"}
                                              : n >= 6 && n <= 9 ? std::set<std::string>{"E-10/linia", "E-10/problem"}
                                                                 : std::set<std::string>{}; });
    bool p11 = false;
    for (auto& x : s.p) p11 = p11 || x.id == "DS-P11";
    SPRAWDZ(p11 && s.przerwaDni == 4 && w.wieczornych == 10);
  }
  {  // migracja z wersji 1 w środku adaptacji: bez powtórzeń porad z dziennika wersji 1
    Stan s = stanZWierszy(stanV1(), K());
    std::set<std::string> v1;
    for (auto& x : s.p) v1.insert(x.id);
    Wynik90 w = scenariusz(s, u, Tryb::CPAP, dobra, bez, 17);
    int powt = 0;
    for (auto& x : s.p)
      if (x.data >= dataDoby(17) && v1.count(x.id)) powt++;
    SPRAWDZ(powt == 0 && w.porad > 20 && w.stanKoniec == "zakonczony");
  }
  {  // bez nawilżacza
    Stan s;
    Ustawienia b = wl();
    b.nawilzacz = 0;
    Wynik90 w = scenariusz(s, b, Tryb::CPAP, dobra, [](int n) { return n == 20 ? std::set<std::string>{"S-K-03"} : std::set<std::string>{}; });
    SPRAWDZ(w.zNawilzaczem == 0 && w.stanKoniec == "zakonczony");
  }
  {  // ASV: wszystkie porady w wariancie ASV albo `wszystkie`
    Stan s;
    Wynik90 w = scenariusz(s, u, Tryb::ASV, dobra, bez);
    SPRAWDZ(w.zlyTryb == 0 && w.wieczornych == 10);
    bool s08 = false;
    for (auto& x : s.p) s08 = s08 || x.id == "DS-S08.asv";
    SPRAWDZ(s08);
  }
  {  // „tylko na telefonie”: porady o objawach jako „Nowa porada na telefonie.”
    Stan s;
    Ustawienia t = wl();
    t.telefon = true;
    Wynik90 w = scenariusz(s, t, Tryb::CPAP, dobra, [](int n) { return n % 9 == 0 ? std::set<std::string>{"E-11"} : std::set<std::string>{}; });
    SPRAWDZ(w.telefon > 0);
    for (auto& x : s.p) SPRAWDZ((x.gdzie == "telefon") == (P(x.id)->ma(FL_OBJAW) && !P(x.id)->karta));
  }
  {  // QR przy najdłuższym adresie w danych: koduje się w wersji ≤ 3 i mieści w rogu
    const Porada* naj = nullptr;
    for (auto& p : K().porady)
      if (p.qr.dl && (!naj || p.qr.dl > naj->qr.dl)) naj = &p;
    SPRAWDZ(naj != nullptr);
    if (naj) {
      DanePorady d = dane(naj->id, naj->rodzaj == Rodzaj::POCHWALA ? Zrodlo::POCHWALA : Zrodlo::PLAN);
      PlotnoT pl;
      SPRAWDZ(rysujPorade(pl, d));
      SPRAWDZ(qr::koduj(K().tekst(naj->qr)).wersja <= 3);
      printf("  QR: najdłuższy adres %u B (%s), wersja %d\n", (unsigned)naj->qr.dl, naj->id.c_str(),
             qr::koduj(K().tekst(naj->qr)).wersja);
    }
  }
}

// Przypadki brzegowe wersji 2
static bool pochwalaId(const std::string& i) { return P(i) && P(i)->rodzaj == Rodzaj::POCHWALA; }
static void testPoprawkiRecenzjiV2() {
  Ustawienia u = wl();
  // R-DS-60: wspólne weto pochwał: noc częściowa, wariant tokenu (`E-20/…`), ekran problemu, brak danych nocy,
  // ocena niepełna (R-AW-40)
  for (int war = 0; war < 6; war++) {
    Stan s;
    doba2(s, u, db(1));
    Doba d = db(2, {"pochwala/pierwsza_noc", "pochwala/pierwsze_4h"});
    if (war == 0) d.tokeny.insert("noc_czesciowa");
    if (war == 1) d.tokeny.insert("E-20/x");
    if (war == 2) d.tokeny.insert("ekran/problem");
    if (war == 3) d.nocZTerapia = false;
    if (war == 4) d.bezNocy = true;
    if (war == 5) d.tokeny.insert("ocena_niepelna");  // R-AW-40: nieczytelny plik oceny
    SPRAWDZ(!pochwalaId(doba2(s, u, d).first));
    SPRAWDZ(!pochwalaId(doba2(s, u, db(3)).first));  // także jako zaległa następnego dnia
  }
  // fail-path R-DS-60: 0 albo brak nocy w oknie → bez rzędu faktów; odmiana podpisu pod samą liczbą
  std::vector<std::pair<std::string, std::string>> f0 = {{"0 z 7", "nocy z maską"}, {"5 h 40", "średnio z maską"}};
  faktyNaEkran(f0);
  SPRAWDZ(f0.empty());
  std::vector<std::pair<std::string, std::string>> fb = {{"– z 7", "nocy z maską"}};
  faktyNaEkran(fb);
  SPRAWDZ(fb.empty());
  std::vector<std::pair<std::string, std::string>> f7 = {{"7 z 7", "nocy z maską"}};
  faktyNaEkran(f7);
  SPRAWDZ(f7.size() == 1 && f7[0].second == "nocy z maską");  // „z 7 nocy”: dopełniacz zostaje
  for (int n = 1; n <= 31; n++) {
    std::vector<std::pair<std::string, std::string>> f = {{std::to_string(n), "nocy z rzędu"}};
    faktyNaEkran(f);
    const char* oczek = n == 1 ? "noc" : (n % 10 >= 2 && n % 10 <= 4 && !(n >= 12 && n <= 14)) ? "noce" : "nocy";
    SPRAWDZ(f.size() == 1 && f[0].second == std::string(oczek) + " z rzędu");
  }
  // strona „Wszystkie porady”: znaczniki jako opis, bez liczb
  SPRAWDZ(opisZnacznikow("Ta noc: {czas} z maską, {wyciek} L/min.") ==
          "Ta noc: [czas z maską tej nocy] z maską, [wyciek tej nocy] L/min.");
  SPRAWDZ(opisZnacznikow("{prog:S-K-10.uwaga_o_masce} i {nieznany}") == "{prog:S-K-10.uwaga_o_masce} i {nieznany}");
  // fail-path R-DS-65: `start.dzien` spoza 1…84 z pliku → doba 1 z wpisem w logu
  Ustawienia u90 = wl();
  u90.dzien = 90;
  SPRAWDZ(dzienStartu(u90, K()) == 1);
  Stan n90 = nowyStan(u90, K());
  SPRAWDZ(n90.dzienUstawiony == 1 && n90.bledy.size() == 1);
  // fail-path R-DS-69: skok daty o > SKOK_DNI → `wstrzymany` (z zapisem w pliku stanu); powrót daty → dalej
  Stan z;
  for (int n = 1; n <= 5; n++) doba2(z, u, db(n));
  size_t p0 = z.p.size();
  SPRAWDZ(doba2(z, u, db(5 + SKOK_DNI + 170)).first.empty() && z.stan == "wstrzymany" && z.p.size() == p0);
  Stan zr = stanZWierszy(wierszeStanu(z), K());
  SPRAWDZ(zr.stan == "wstrzymany" && zr.skokData == dataDoby(5 + SKOK_DNI + 170) && zr.skokIle == 1 && zr.bledy.empty());
  SPRAWDZ(!doba2(zr, u, db(6)).first.empty() && zr.stan == "aktywny" && zr.skokData.empty());
  // … nowa data przez SKOK_POTWIERDZ kolejne doby → przyjęta; po końcu programu `zakonczony` bez porad
  Stan zk = z;
  for (int i = 1; i < SKOK_POTWIERDZ; i++) SPRAWDZ(doba2(zk, u, db(5 + SKOK_DNI + 170 + i)).first.empty());
  SPRAWDZ(zk.stan == "zakonczony" && zk.p.size() == p0);
  // przerwa w datach (nie po kolei) zeruje licznik dób
  Stan zp = z;
  doba2(zp, u, db(5 + SKOK_DNI + 172));
  SPRAWDZ(zp.stan == "wstrzymany" && zp.skokIle == 1);
  // skok o dokładnie SKOK_DNI dób to zwykła przerwa w porankach (bez wstrzymania)
  Stan zs;
  for (int n = 1; n <= 5; n++) doba2(zs, u, db(n));
  doba2(zs, u, db(5 + SKOK_DNI));
  SPRAWDZ(zs.stan != "wstrzymany");
  // R-DS-63: kod nie stanął pod linią uwagi wzorca A, krok odsyłał do kodu → krok z adresem
  DanePorady d = dane("DS-D47");
  d.linia = "Uwaga testowa";
  d.liniaA = true;
  PlotnoT pl;
  bool qr = rysujPorade(pl, d);
  printf("  DS-D47 pod linią uwagi wzorca A: kod %s\n", qr ? "jest" : "pominięty, krok z adresem");
  // migracja z wersji 1: porada planu zaliczona przez poradę z danych (`zamiast`) z dziennika P
  std::vector<std::string> v1 = stanV1();
  v1.push_back("P;" + dataDoby(17) + ";DS-Z10;dane;S-K-03;dzien;panel");
  Stan m = stanZWierszy(v1, K());
  bool zal = false;
  for (auto& x : m.zaliczone) zal = zal || (x.first == "DS-D19" && x.second == dataDoby(17));
  SPRAWDZ(zal);
}

int main() {
  testR_DS_54_fazy();
  testR_DS_55_dwiePoradyWStarcie();
  testR_DS_56_koniecFazy();
  testR_DS_57_napisy();
  testR_DS_58_pochwaly();
  testR_DS_59_jednaPochwala();
  testR_DS_60_problemIZnaczniki();
  testR_DS_61_bezAhi();
  testR_DS_62_63_kodQr();
  testR_DS_64_fakty();
  testR_DS_65_startOdDnia();
  testR_DS_66_formularz();
  testR_DS_67_migracjaV1();
  testR_DS_68_start();
  testR_DS_69_utrwalenie();
  testR_DS_70_wariantTrybu();
  testR_DS_71_kompletnosc();
  testScenariusze90();
  testPoprawkiRecenzjiV2();
  printf("testy programu Dobry start, wersja 2: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
