// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: program „Dobry start” (spec 5.8.14, R-DS-01…R-DS-53, ADR-0049): katalog porad z zestawu wbudowanego, wybór
// porady dnia, limity, stopnie, przerwa, koniec programu, ankieta, plik stanu, teksty ekranu S-K-48, w wersji 2 programu
// (12 tygodni w trzech fazach; testy wymagań zmienionych w wersji 2 oznaczone „wersja 2”; nowe R-DS-54…R-DS-71:
// test/testy_dobry_start_v2.cpp). Na atrapach, bez dyrygenta (dyrygent i ekran: test/testy_dobry_start_brzegowe.cpp).
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "dobry_start.h"
#include "ekrany.h"
#include "magazyn.h"
#include "reguly.h"
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

static Ustawienia wlaczony() {
  Ustawienia u;
  u.program = true;
  return u;
}
static Doba doba_(int n, Tryb t = Tryb::CPAP, std::set<std::string> tok = {}, bool terapia = true) {
  Doba d;
  d.dzis = dodajDni(START, n - 1);
  d.tryb = t;
  d.tokeny = tok;
  d.nocZTerapia = terapia;
  return d;
}
// jedna doba: wybór i narysowanie (w bloku albo wieczorem); zwraca ID porady albo ""
// jedna doba: porada dnia (blok albo wieczór przy D0) i, w fazie start, porada wieczorna (R-DS-55); zwraca ID porady
// dnia albo ""
static std::string dzien(Stan& s, const Ustawienia& u, const Doba& d, bool narysuj = true) {
  WyborPorady w = wybierz(K(), s, u, d);
  if (narysuj && w.porada) zapiszPokazanie(K(), s, w, d.dzis, w.tylkoWieczor, w.telefon);
  if (narysuj) {
    WyborPorady e = wybierzWieczor(K(), s, u, d);
    if (e.porada && e.porada->faza == Faza::START_WIECZOR && e.zrodlo == Zrodlo::PLAN) zapiszPokazanie(K(), s, e, d.dzis, true, e.telefon);
  }
  return w.porada ? w.porada->id : "";
}
static int wpisowP(const Stan& s, const std::string& dzis) {  // wpisy porady dnia (bez porady wieczornej startu)
  int n = 0;
  for (auto& x : s.p) {
    const Porada* p = katalogWbudowany().porada(x.id);
    n += x.data == dzis && !(p && p->faza == Faza::START_WIECZOR && x.zrodlo == "plan");
  }
  return n;
}
static const Porada* koncowa() {
  for (auto& p : K().porady)
    if (p.ma(FL_KONCOWA)) return &p;
  return nullptr;
}

static const Porada* plan(const std::string& id) {
  const Porada* p = K().porada(id);
  return p && p->rodzaj == Rodzaj::PLAN ? p : nullptr;
}
static bool rodzaj(const std::string& id, Rodzaj r) { return K().porada(id) && K().porada(id)->rodzaj == r; }

static void testKatalogWbudowany() {
  SPRAWDZ(K().jest());
  SPRAWDZ(K().log.empty());
  // wersja 2 (R-DS-54): trzy fazy, koniec programu = ostatnia doba nominalna, 12 tygodni
  SPRAWDZ(K().koniecStartu > 0 && K().koniecStartu < K().koniecAdaptacji && K().koniecAdaptacji < K().miejsc);
  SPRAWDZ(K().tygodni() == (K().miejsc + 6) / 7 && K().utrwalenieIle > 0);
  SPRAWDZ(!K().pytania.empty());
  SPRAWDZ(koncowa() && koncowa()->dzien == K().miejsc);
  // R-DS-71: każda doba nominalna każdej fazy w każdym trybie i stanie nawilżacza ma dokładnie jedną poradę
  for (auto& p : K().porady) {
    if (p.rodzaj != Rodzaj::PLAN) continue;
    for (Tryb t : {Tryb::CPAP, Tryb::AUTOSET, Tryb::ASV})
      for (int n : {1, 0}) SPRAWDZ(wariantDoby(K(), p.faza, p.dzien, t, n) != nullptr);
  }
}

static void testR_DS_01_wlacznik() {
  Stan s;
  Ustawienia u;  // bez wiersza: wyłączony
  for (int n = 1; n <= 30; n++) SPRAWDZ(dzien(s, u, doba_(n)).empty());
  SPRAWDZ(s.start.empty() && s.p.empty());
  std::string b;
  Ustawienia z;
  SPRAWDZ(ustawienie(z, "start.program", "moze", b) && !z.program && !b.empty());
  SPRAWDZ(!ustawienie(z, "dzien.ukryte", "porada", b));  // nie klucz programu (R-DS-22)
  SPRAWDZ(ustawienie(z, "start.ponad", "tak", b) && !b.empty());  // R-DS-67: klucz wycofany, wpis w logu
  Stan s2;
  std::string id = dzien(s2, wlaczony(), doba_(1));
  SPRAWDZ(plan(id) && plan(id)->faza == Faza::START_RANO && plan(id)->dzien == 1);
  SPRAWDZ(s2.start == START && dobaProgramu(s2, START) == 1);
}

static void testR_DS_02_wariantTrybu() {
  // doba z trzema wariantami trybu daje trzy różne porady, doba 1 startu w ASV inną niż w AutoSet
  const Porada* m3 = nullptr;
  for (auto& p : K().porady)
    if (p.rodzaj == Rodzaj::PLAN && wariantDoby(K(), p.faza, p.dzien, Tryb::CPAP, -1) !=
                                        wariantDoby(K(), p.faza, p.dzien, Tryb::AUTOSET, -1))
      m3 = &p;
  SPRAWDZ(m3 != nullptr);
  if (!m3) return;
  const Porada* a = wariantDoby(K(), m3->faza, m3->dzien, Tryb::CPAP, -1);
  const Porada* b = wariantDoby(K(), m3->faza, m3->dzien, Tryb::AUTOSET, -1);
  const Porada* c = wariantDoby(K(), m3->faza, m3->dzien, Tryb::ASV, -1);
  SPRAWDZ(a && b && c && a != b && b != c && a != c);
  SPRAWDZ(a && c && a->baza == c->baza);
  SPRAWDZ(wariantDoby(K(), Faza::START_RANO, 1, Tryb::ASV, -1) != wariantDoby(K(), Faza::START_RANO, 1, Tryb::AUTOSET, -1));
  SPRAWDZ(trybZNazwy("ASVAuto") == Tryb::ASV && trybZNazwy("VAuto") == Tryb::NIEZNANY);
}

static void testR_DS_05_jednaNaDobe60() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 60; n++) {
    std::set<std::string> t;
    if (n % 3 == 0) t.insert("E-11");
    if (n % 5 == 0) t.insert("S-K-10/problem");
    if (n % 7 == 0) t.insert("E-14");
    dzien(s, u, doba_(n, Tryb::CPAP, t));
    dzien(s, u, doba_(n, Tryb::CPAP, t));  // druga ocena tej samej doby: bez nowego wpisu
    SPRAWDZ(wpisowP(s, doba_(n).dzis) <= 1);
  }
}

static void testR_DS_06_zalegla2Dni() {
  Stan s;
  Ustawienia u = wlaczony();
  dzien(s, u, doba_(1));
  // dwa warunki tej samej nocy: jedna porada, druga zaległa
  std::string a = dzien(s, u, doba_(2, Tryb::CPAP, {"E-11", "E-14"}));
  SPRAWDZ(rodzaj(a, Rodzaj::ZDARZENIE));
  SPRAWDZ(s.z.size() == 1);
  std::string b = dzien(s, u, doba_(3));
  SPRAWDZ(rodzaj(b, Rodzaj::ZDARZENIE) && b != a);  // zaległa następnego dnia
  // zaległa przepada po 2 dniach: w adaptacji limit 2 / 7 wyczerpany, więc czeka, a 3. dnia znika (wersja 2: w starcie
  // bez limitu, R-DS-68, więc przykład w dobach 14…19)
  Stan t;
  for (int n = 1; n <= 13; n++) dzien(t, u, doba_(n));
  dzien(t, u, doba_(14, Tryb::CPAP, {"E-11"}));
  dzien(t, u, doba_(15, Tryb::CPAP, {"S-K-14"}));
  dzien(t, u, doba_(16, Tryb::CPAP, {"E-14"}));  // limit: zaległa
  SPRAWDZ(t.z.size() == 1);
  dzien(t, u, doba_(17));
  dzien(t, u, doba_(18));
  SPRAWDZ(t.z.size() == 1);
  dzien(t, u, doba_(19));
  SPRAWDZ(t.z.empty());
}

static void testR_DS_07_limit() {
  Ustawienia u = wlaczony();
  {  // R-DS-68: w starcie bez limitu 2 na 7 (najwyżej jedna rano)
    Stan s;
    dzien(s, u, doba_(1));
    SPRAWDZ(rodzaj(dzien(s, u, doba_(2, Tryb::CPAP, {"E-11"})), Rodzaj::ZDARZENIE));
    SPRAWDZ(rodzaj(dzien(s, u, doba_(3, Tryb::CPAP, {"S-K-14"})), Rodzaj::ZDARZENIE));
    SPRAWDZ(rodzaj(dzien(s, u, doba_(4, Tryb::CPAP, {"E-14"})), Rodzaj::ZDARZENIE));
  }
  Stan s;  // od doby 11: 2 na 7 dni
  for (int n = 1; n <= 13; n++) dzien(s, u, doba_(n));
  SPRAWDZ(rodzaj(dzien(s, u, doba_(14, Tryb::CPAP, {"E-11"})), Rodzaj::ZDARZENIE));
  SPRAWDZ(rodzaj(dzien(s, u, doba_(16, Tryb::CPAP, {"S-K-14"})), Rodzaj::ZDARZENIE));
  SPRAWDZ(rodzaj(dzien(s, u, doba_(18, Tryb::CPAP, {"E-14"})), Rodzaj::PLAN));  // trzecia w 7 dniach: plan
  // po 28. dobie limit 1 / 7
  Stan t;
  for (int n = 1; n <= 30; n++) dzien(t, u, doba_(n));
  SPRAWDZ(rodzaj(dzien(t, u, doba_(31, Tryb::CPAP, {"E-11"})), Rodzaj::ZDARZENIE));
  SPRAWDZ(rodzaj(dzien(t, u, doba_(33, Tryb::CPAP, {"E-14"})), Rodzaj::PLAN));
}
static void testR_DS_08_stopnieWycieku() {
  Stan s;
  Ustawienia u = wlaczony();
  std::vector<std::string> pokazane;
  for (int n = 1; n <= 56; n++) {
    std::set<std::string> t;
    if (n == 3 || n == 11 || n == 20 || n == 29 || n == 38 || n == 47 || n == 55) t.insert("S-K-10/problem");
    std::string id = dzien(s, u, doba_(n, Tryb::CPAP, t));
    if (!t.empty()) pokazane.push_back(id);
  }
  const Porada* z1 = nullptr;
  for (auto& p : K().porady)
    if (p.ma(FL_WYCIEK) && !p.stopienWyzej.empty() && p.kol > 0) {
      bool niz = false;
      for (auto& q : K().porady)
        if (q.stopienWyzej == p.id) niz = true;
      if (!niz) z1 = &p;
    }
  SPRAWDZ(z1 != nullptr);
  if (!z1) return;
  const Porada* z2 = K().porada(z1->stopienWyzej);
  const Porada* z3 = z2 ? K().porada(z2->stopienWyzej) : nullptr;
  SPRAWDZ(z2 && z3);
  if (!z2 || !z3) return;
  SPRAWDZ(pokazane.size() == 7);
  SPRAWDZ(pokazane[0] == z1->id && pokazane[1] == z1->id && pokazane[2] == z2->id && pokazane[3] == z2->id);
  SPRAWDZ(pokazane[4] == z3->id && pokazane[5] == z3->id);
  SPRAWDZ(!rodzaj(pokazane[6], Rodzaj::ZDARZENIE));  // po dwóch najwyższych nic nowego
  // ta sama porada najwyżej raz na 7 dni
  Stan t;
  dzien(t, u, doba_(1));
  std::string a = dzien(t, u, doba_(3, Tryb::CPAP, {"E-11"}));
  std::string b = dzien(t, u, doba_(6, Tryb::CPAP, {"E-11"}));
  SPRAWDZ(rodzaj(a, Rodzaj::ZDARZENIE) && b != a);
}

static void testR_DS_09_zamiast() {
  // porada z danych z polem `zamiast` przed miejscem planu: miejsce zaliczone i pominięte
  const Porada* z = nullptr;
  for (auto& p : K().porady)
    if (p.rodzaj == Rodzaj::ZDARZENIE && !p.zamiast.empty() && !p.warunek.empty() && p.wymagaPorady.empty() &&
        K().porada(p.zamiast) && K().porada(p.zamiast)->dzien > 2 && p.weta.empty() && !p.ma(FL_PRZERWA))
      if (!z || K().porada(p.zamiast)->dzien < K().porada(z->zamiast)->dzien) z = &p;
  SPRAWDZ(z != nullptr);
  if (!z) return;
  int miejsce = K().porada(z->zamiast)->dzien;
  Stan s;
  Ustawienia u = wlaczony();
  dzien(s, u, doba_(1));
  SPRAWDZ(dzien(s, u, doba_(2, Tryb::CPAP, {z->warunek[0]})) == z->id);
  std::set<int> miejsca;
  for (int n = 3; n <= miejsce + 3; n++) {
    const Porada* p = K().porada(dzien(s, u, doba_(n)));
    if (p && p->faza == K().porada(z->zamiast)->faza) miejsca.insert(p->dzien);
  }
  SPRAWDZ(!miejsca.count(miejsce));
  SPRAWDZ(znacznik(K(), s, *K().porada(z->zamiast)) == "pokazana " + ddmm(doba_(2).dzis));
}

static void testR_DS_10_kolejkaPoNarysowaniu() {
  Stan s;
  Ustawienia u = wlaczony();
  std::string d1 = dzien(s, u, doba_(1));
  std::string d2 = dzien(s, u, doba_(2), false);  // wybrana, nienarysowana
  SPRAWDZ(wpisowP(s, doba_(2).dzis) == 0 && plan(d2) && plan(d2)->dzien == 2);
  SPRAWDZ(dzien(s, u, doba_(3)) == d2);  // nowa doba: ta sama porada planu (doba nominalna 2) czeka w kolejce
  Doba d4 = doba_(4);
  d4.w0 = true;  // S5 trzyma dzień i wieczór
  SPRAWDZ(dzien(s, u, d4).empty());
  SPRAWDZ(plan(dzien(s, u, doba_(5))) && plan(dzien(s, u, doba_(5)))->dzien == 3);
}

static void testR_DS_11_przerwa() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 9; n++) dzien(s, u, doba_(n));
  const Porada* z5 = nullptr;
  const Porada* z6 = nullptr;
  for (auto& p : K().porady) {
    if (p.ma(FL_PRZERWA)) z6 = &p;
    if (p.ma(FL_POZA_LIMITEM) && !p.ma(FL_PRZERWA)) z5 = &p;
  }
  SPRAWDZ(z5 && z6);
  if (!z5 || !z6) return;
  SPRAWDZ(dzien(s, u, doba_(10, Tryb::CPAP, {"E-10/linia"}, false)) == z5->id);
  for (int n = 11; n <= 18; n++)
    SPRAWDZ(dzien(s, u, doba_(n, Tryb::CPAP, {"E-10/linia", "E-10/problem"}, false)) == z6->id);
  int wpisyZ6 = 0;
  for (auto& x : s.p) wpisyZ6 += x.id == z6->id;
  SPRAWDZ(wpisyZ6 == 2);  // 11. i 18. doba (co 7 dni)
  SPRAWDZ(s.stan == "przerwa" && dobaProgramu(s, doba_(18).dzis) == 10);
  // wersja 2 (R-DS-56): przerwa wydłuża fazę; doba programu 11 = adaptacja, porada startu z doby 10 (zajętej przez
  // DS-Z05) nie jest podstawowa, więc nie przechodzi do adaptacji
  std::string id = dzien(s, u, doba_(19, Tryb::CPAP, {}, true));
  SPRAWDZ(s.stan == "aktywny" && plan(id) && plan(id)->faza == Faza::ADAPTACJA && plan(id)->dzien == 11);
  SPRAWDZ(s.przerwaDni == 8);
}

static void testR_DS_12_koniecProgramu() {
  // wersja 2 (R-DS-12 wycofane, zob. R-DS-56, R-DS-69): przy D0 kolejka adaptacji zostaje w tyle, w ostatniej dobie
  // adaptacji porada `koncowa_fazy`, potem niepokazane podstawowe, a po poradzie `koncowa` program się kończy
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= K().koniecAdaptacji - 1; n++) {
    Doba d = doba_(n);
    d.d0 = n > K().koniecStartu && n % 3 != 0;  // dwie doby z D0, jedna bez: porada planu co trzecią dobę
    dzien(s, u, d);
  }
  const Porada* kf = nullptr;
  for (auto& p : K().porady)
    if (p.ma(FL_KONCOWA_FAZY)) kf = &p;
  SPRAWDZ(kf && dzien(s, u, doba_(K().koniecAdaptacji)) == kf->id);
  int nie = 0;
  for (auto& p : K().porady)
    if (p.rodzaj == Rodzaj::PLAN && p.faza == Faza::ADAPTACJA && znacznik(K(), s, p) == "jeszcze nie") nie++;
  SPRAWDZ(nie > 0);
  WyborPorady w = wybierz(K(), s, u, doba_(K().koniecAdaptacji + 1));
  SPRAWDZ(w.porada && (w.zrodlo == Zrodlo::PODSTAWOWA || w.porada->faza == Faza::UTRWALENIE));
  SPRAWDZ(w.zrodlo != Zrodlo::PODSTAWOWA || w.porada->ma(FL_PODSTAWOWA));
}

static void testR_DS_13_14_przebiegZwykly() {
  // wersja 2 (R-DS-13 zmienione, R-DS-14 wycofane): napisy faz z R-DS-57, w utrwaleniu porady planu tylko w ich doby
  Stan s;
  Ustawienia u = wlaczony();
  int pokazanych = 0;
  for (int n = 1; n <= K().miejsc; n++) {
    WyborPorady w = wybierz(K(), s, u, doba_(n));
    Etap e = K().etap(n);
    if (e != Etap::UTRWALENIE) SPRAWDZ(w.porada && w.porada->dzien == n);
    if (!w.porada) continue;
    OpisPorady o = opis(K(), s, w);
    std::string d = std::to_string(w.porada->dzien);
    if (e == Etap::START) SPRAWDZ(o.srodekBelki == "start · dzień " + d + " z " + std::to_string(K().koniecStartu) &&
                                  o.etykieta == "na dziś");
    if (e == Etap::ADAPTACJA)
      SPRAWDZ(o.srodekBelki == "adaptacja · dzień " + d + " z " + std::to_string(K().koniecAdaptacji) &&
              o.etykieta == "tydzień " + std::to_string((w.porada->dzien - 1) / 7 + 1) + " z " +
                                std::to_string((K().koniecAdaptacji + 6) / 7));
    if (e == Etap::UTRWALENIE) {
      SPRAWDZ(w.porada->dzien == n && w.porada->faza == Faza::UTRWALENIE);
      SPRAWDZ(o.etykieta.compare(0, 7, "porada ") == 0);
    }
    SPRAWDZ(o.tydzien == (w.porada->dzien - 1) / 7 + 1 && o.tygodni == K().tygodni());
    zapiszPokazanie(K(), s, w, doba_(n).dzis, false, false);
    WyborPorady we = wybierzWieczor(K(), s, u, doba_(n));  // w starcie porada wieczorna (R-DS-55)
    if (we.porada && we.porada->faza == Faza::START_WIECZOR && we.zrodlo == Zrodlo::PLAN) zapiszPokazanie(K(), s, we, doba_(n).dzis, true, false);
    pokazanych++;
  }
  SPRAWDZ(s.planKoniec);
  SPRAWDZ(dzien(s, u, doba_(K().miejsc + 1)).empty() && s.stan == "zakonczony");
  printf("  przebieg zwykły: %d porad rano w %d dobach\n", pokazanych, K().miejsc);
}

static void testR_DS_13_belkaWyzwalanej() {
  Stan s;
  Ustawienia u = wlaczony();
  dzien(s, u, doba_(1));
  WyborPorady w = wybierz(K(), s, u, doba_(3, Tryb::CPAP, {"E-11"}));
  OpisPorady o = opis(K(), s, w);
  SPRAWDZ(o.srodekBelki == "po nocy " + ddmm(doba_(3).dzis) && o.etykieta == "porada do ostatniej nocy");
  SPRAWDZ(o.tydzien == 0 && !o.tytul.empty() && !o.tresc.empty());
}
static void testR_DS_26_27_28_ankieta() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 4; n++) dzien(s, u, doba_(n));
  SPRAWDZ(s.ankietaOd == doba_(4).dzis);  // po 3. nocy z terapią od dnia włączenia
  SPRAWDZ(tydzienAnkiety(s, doba_(4).dzis) == "t1" && tydzienAnkiety(s, doba_(11).dzis) == "t2");
  SPRAWDZ(ankietaCzeka(s, doba_(4).dzis));
  const Pytanie* wiele = nullptr;
  for (auto& q : K().pytania)
    if (q.typ == "wiele") wiele = &q;
  SPRAWDZ(wiele && wiele->wylaczna > 0);
  if (!wiele) return;
  // odpowiedzi 2 i 3 → dwie porady z ankiety w kolejnych dobach (w limicie 2 / 7)
  zapiszAnkiete(K(), s, doba_(4).dzis, {{wiele->id, {2, 3}}});
  SPRAWDZ(!ankietaCzeka(s, doba_(4).dzis));
  std::string a = dzien(s, u, doba_(5));
  std::string b = dzien(s, u, doba_(6));
  SPRAWDZ(rodzaj(a, Rodzaj::ANKIETA) && rodzaj(b, Rodzaj::ANKIETA) && a != b);
  WyborPorady w = wybierz(K(), s, u, doba_(6));
  SPRAWDZ(opis(K(), s, w).srodekBelki == "z twojej ankiety" && opis(K(), s, w).etykieta == "porada do ankiety");
  // odpowiedź wyłączna odznacza pozostałe; nic nie daje porady (R-DS-48)
  zapiszAnkiete(K(), s, doba_(11).dzis, {{wiele->id, {1, wiele->wylaczna}}});
  SPRAWDZ(s.o.back().kody == std::to_string(wiele->wylaczna));
  SPRAWDZ(rodzaj(dzien(s, u, doba_(12)), Rodzaj::PLAN));
  // pytanie z flagą „dwa razy”: ta sama odpowiedź w dwóch kolejnych wypełnionych ankietach
  const Porada* dwa = nullptr;
  for (auto& p : K().porady)
    if (p.ma(FL_DWA_RAZY) && p.rodzaj == Rodzaj::ANKIETA) dwa = &p;  // u pochwał `dwa_razy` to limit (R-DS-58)
  SPRAWDZ(dwa && !dwa->warunek.empty());
  if (!dwa) return;
  std::string pyt = dwa->warunek[0].substr(0, dwa->warunek[0].find('='));
  int kod = liczbaCalk(dwa->warunek[0].substr(dwa->warunek[0].find('=') + 1).c_str());
  zapiszAnkiete(K(), s, doba_(18).dzis, {{pyt, {kod}}});
  SPRAWDZ(dzien(s, u, doba_(19)) != dwa->id);  // pierwszy raz: nic
  zapiszAnkiete(K(), s, doba_(32).dzis, {{pyt, {kod}}});  // t3 bez zapisu tego pytania, t5 znowu
  SPRAWDZ(dzien(s, u, doba_(33)) == dwa->id);
}

static void testR_DS_29_bezNawilzacza() {
  Ustawienia u = wlaczony();
  u.nawilzacz = 0;
  const Porada* m = nullptr;  // doba z wariantem `bez nawilżacza`
  for (auto& p : K().porady)
    if (p.rodzaj == Rodzaj::PLAN && p.nawilzacz == -1) m = &p;
  SPRAWDZ(m != nullptr);
  if (!m) return;
  SPRAWDZ(wariantDoby(K(), m->faza, m->dzien, Tryb::CPAP, 0)->nawilzacz == -1);
  SPRAWDZ(wariantDoby(K(), m->faza, m->dzien, Tryb::CPAP, -1)->nawilzacz == 1);
  for (auto& p : K().porady)  // porady wyzwalane wymagające nawilżacza nie stoją
    if (p.rodzaj == Rodzaj::ZDARZENIE && p.nawilzacz == 1) {
      Stan s;
      dzien(s, u, doba_(1));
      SPRAWDZ(dzien(s, u, doba_(2, Tryb::CPAP, {p.warunek[0]})) != p.id);
    }
}

static void testR_DS_29_dobaBezNawilzacza() {
  // wersja 2 (R-DS-29 zmienione, bez `miejsce_bez`): każda doba planu bez nawilżacza ma poradę bez wymagania
  // nawilżacza; przebieg 84 dób bez nawilżacza bez porady `wymaga=nawilżacz`
  for (auto& p : K().porady)
    if (p.rodzaj == Rodzaj::PLAN)
      for (Tryb t : {Tryb::CPAP, Tryb::AUTOSET, Tryb::ASV}) {
        const Porada* v = wariantDoby(K(), p.faza, p.dzien, t, 0);
        SPRAWDZ(v && v->nawilzacz != 1);
      }
  Ustawienia u = wlaczony();
  u.nawilzacz = 0;
  Stan s;
  for (int n = 1; n <= K().miejsc; n++) {
    std::string id = dzien(s, u, doba_(n));
    SPRAWDZ(id.empty() || K().porada(id)->nawilzacz != 1);
  }
}

static void testR_DS_32_33_wylaczenieIOdNowa() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 16; n++) dzien(s, u, doba_(n));
  Ustawienia wyl;
  SPRAWDZ(dzien(s, wyl, doba_(17)).empty() && s.stan == "zatrzymany");
  SPRAWDZ(dzien(s, wyl, doba_(18)).empty());
  std::string id = dzien(s, u, doba_(20));  // doby 17…19 bez programu nie liczą się (R-DS-56)
  SPRAWDZ(s.stan == "aktywny" && plan(id) && plan(id)->faza == Faza::ADAPTACJA && plan(id)->dzien == 17);
  Stan n = nowyStan(u, K());
  SPRAWDZ(n.p.empty() && n.start.empty() && n.dzienUstawiony == 1);
}

static void testR_DS_34_startOdDnia15() {
  // wersja 2 (R-DS-34 wycofane, zob. R-DS-65): podstawowe z dób < 15 w kolejności `kol`, potem adaptacja od doby 15
  Ustawienia u = wlaczony();
  u.dzien = 15;
  Stan s;
  std::vector<const Porada*> podst;
  std::set<std::string> bazy;
  for (auto& p : K().porady)
    if (p.ma(FL_PODSTAWOWA) && p.dzien < 15 && p.nawilzacz >= 0 && (p.tryby & (TR_CPAP | TR_WSZYSTKIE)) &&
        bazy.insert(p.baza).second)
      podst.push_back(&p);
  std::sort(podst.begin(), podst.end(), [](const Porada* a, const Porada* b) { return a->kol < b->kol; });
  SPRAWDZ(!podst.empty());
  for (size_t i = 0; i < podst.size(); i++) {
    WyborPorady w = wybierz(K(), s, u, doba_((int)i + 1));
    SPRAWDZ(w.porada == podst[i] && w.zrodlo == Zrodlo::PODSTAWOWA);
    zapiszPokazanie(K(), s, w, doba_((int)i + 1).dzis, false, false);
  }
  std::string d = dzien(s, u, doba_((int)podst.size() + 1));
  SPRAWDZ(plan(d) && plan(d)->dzien >= 15 && plan(d)->faza == Faza::ADAPTACJA);
  std::string b;
  Ustawienia z;
  SPRAWDZ(ustawienie(z, "start.dzien", "500", b) && z.dzien == 1 && !b.empty());
}

static std::string plikPorad(const std::string& cialo) {
  return "#cpap-eink porady 1 zestaw=t pochodzenie=test skrot=sha256:" + sha256Hex(cialo) + "\n" + cialo;
}
static const char* POLA =
    "id;rodzaj;dzien;tryb;wymaga;ekran;zamiast;warunek;kol;kol_ankiety;flagi;wymaga_porady;stopien_wyzej;tytul;tresc;krok;faza;"
    "qr;fakty\n";

static void testR_DS_36_brakPliku() {
  Katalog k;
  std::string p;
  SPRAWDZ(!wczytajKatalog("", "", k, p) && !p.empty());
  SPRAWDZ(!wczytajKatalog("#cpap-eink porady 1 skrot=sha256:00\nid\n", "", k, p) && p.find("skrót") != std::string::npos);
  Stan s;
  WyborPorady w = wybierz(Katalog(), s, wlaczony(), doba_(1));
  SPRAWDZ(!w.porada && s.start.empty() && !w.log.empty());
}

static void testR_DS_37_uszkodzonyWiersz() {
  std::string c = std::string(POLA) + "DS-D01;plan;1;wszystkie;;dane;;;;;;;;Tytuł 1;Treść 1;→ Krok.;adaptacja;;\n" +
                  "DS-D02;plan;2;wszystkie;;dane;;;;;;;;Tytuł 2;;→ Krok.;adaptacja;;\n" +
                  "DS-D03;plan;3;wszystkie;;dane;;;;;koncowa;;;Tytuł 3;Treść 3;;adaptacja;;\n" +
                  "DS-D03;plan;3;wszystkie;;dane;;;;;;;;Tytuł 3b;Treść 3b;;adaptacja;;\n" + "DS-D04;plan;x;wszystkie;;dane\n";
  std::string t = plikPorad(c);
  Katalog k;
  std::string p;
  SPRAWDZ(wczytajKatalog(t.c_str(), "", k, p, true));
  SPRAWDZ(k.porady.size() == 2 && k.log.size() == 3);
  SPRAWDZ(std::find(k.log.begin(), k.log.end(), "DS-D02: brak treści") != k.log.end());
  SPRAWDZ(k.tekst(k.porady[1].tytul) == "Tytuł 3" && k.tekst(k.porady[1].tresc) == "Treść 3");
  Katalog kk = k;  // kopia z własnym tekstem
  SPRAWDZ(kk.tekst(kk.porady[0].krok) == "→ Krok.");
  // doba z uszkodzoną poradą bez porady planu, kolejna porada w swojej dobie nominalnej
  Stan s;
  Ustawienia u = wlaczony();
  WyborPorady w1 = wybierz(kk, s, u, doba_(1));
  zapiszPokazanie(kk, s, w1, doba_(1).dzis, false, false);
  SPRAWDZ(!wybierz(kk, s, u, doba_(2)).porada);
  WyborPorady w3 = wybierz(kk, s, u, doba_(3));
  SPRAWDZ(w3.porada && w3.porada->id == "DS-D03");
}

static void testR_DS_38_trybNieznany() {
  // tryb nieznany: tylko porady `wszystkie`; doba startu bez takiej porady zostaje bez porady planu
  Stan s;
  Ustawienia u = wlaczony();
  const Porada* bez = nullptr;
  for (int d = 1; d <= K().koniecStartu && !bez; d++)
    if (!wariantDoby(K(), Faza::START_RANO, d, Tryb::NIEZNANY, -1)) bez = wariantDoby(K(), Faza::START_RANO, d, Tryb::CPAP, -1);
  SPRAWDZ(bez != nullptr);
  for (int n = 1; n <= K().koniecStartu; n++) {
    std::string id = dzien(s, u, doba_(n, Tryb::NIEZNANY));
    if (!id.empty()) SPRAWDZ(K().porada(id)->tryby & TR_WSZYSTKIE);
  }
  SPRAWDZ(bez && znacznik(K(), s, *bez) == "jeszcze nie");
}

static void testR_DS_39_dlugoscNaKrojachPanelu() {
  // te same granice co tools/zestaw/porady.py, policzone krojami i zawijaniem panelu (zawin(), szer()); wersja 2:
  // kolumna 513 px przy kodzie QR, karta z faktami do 2 linii, znaczniki danych z najdłuższą wartością (R-DS-39)
  Znaczniki zmax;
  zmax.czas = zmax.oIleDluzej = zmax.srednio = zmax.srednioProg = 23 * 60 + 59;
  zmax.nocyZRzedu = zmax.nocyZMaskaProg = 999;
  zmax.wyciek = 99;
  zmax.nocyZMaska = zmax.nocy4h = 7;
  auto najdl = [&](const std::string& t) {  // R-DS-53: najdłuższa wartość progu (ustawienie R-KONF-42 do 15)
    return podstawProgi(podstawZnaczniki(t, zmax), [](const std::string& id) -> std::string {
      const reguly::Prog* p = reguly::zestawWbudowany().prog(id == "S-K-10.uwaga_o_masce" ? "R-KONF-42.zakres_max" : id,
                                                            reguly::Tryb::NIEZNANY);
      return p ? calkowita(p->wartosc) : "";
    });
  };
  for (auto& p : K().porady) {
    std::string ty = najdl(K().tekst(p.tytul)), tr = najdl(K().tekst(p.tresc)), kr = najdl(K().tekst(p.krok));
    SPRAWDZ(tr.find("{prog:") == std::string::npos && tr.find('{') == std::string::npos);
    SPRAWDZ(szer(F_SB44, ty) <= 748);
    if (p.karta) {
      SPRAWDZ(zawin(F_P22B, tr, 490).size() <= (p.fakty.dl ? 2u : 4u));
      SPRAWDZ(szer(F_P22B, kr) <= 490);
    } else {
      int kol = p.qr.dl ? 513 : 748;
      SPRAWDZ(zawin(F_P24S, tr, kol).size() <= 5);
      SPRAWDZ(szer(F_P24B, kr) <= kol);
    }
    if (szer(F_SB44, ty) > 748) printf("  %s: tytuł %d px\n", p.id.c_str(), szer(F_SB44, ty));
  }
}

static void testR_DS_42_zegarCofniety() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 10; n++) dzien(s, u, doba_(n));
  size_t ile = s.p.size();
  std::string d10;
  for (auto& x : s.p)
    if (x.data == doba_(10).dzis && K().porada(x.id)->faza != Faza::START_WIECZOR) d10 = x.id;
  SPRAWDZ(dzien(s, u, doba_(8)).empty() && dzien(s, u, doba_(9)).empty());
  SPRAWDZ(dzien(s, u, doba_(10)) == d10 && s.p.size() == ile);  // porada dnia z dziennika
  SPRAWDZ(!dzien(s, u, doba_(11)).empty() && s.p.size() == ile + 1);  // adaptacja: jedna porada na dobę
  Doba bez;
  SPRAWDZ(dzien(s, u, bez).empty());  // U-04
}

static void testR_DS_43_restart() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 13; n++) dzien(s, u, doba_(n));
  std::string id = dzien(s, u, doba_(14));
  Stan r = stanZWierszy(wierszeStanu(s), K());
  SPRAWDZ(r.bledy.empty());
  SPRAWDZ(wierszeStanu(r) == wierszeStanu(s));
  WyborPorady w = wybierz(K(), r, u, doba_(14));
  SPRAWDZ(w.porada && w.porada->id == id && w.narysowana);
  // wersja 2 (R-DS-67): wiersze wersji 1 (`S;kolejka`, `S;ponad`, `K;{miejsce}`) pominięte z jednym wpisem w logu,
  // kolejki z dziennika P
  std::vector<std::string> v1 = wierszeStanu(s);
  v1.push_back("S;kolejka;15");
  v1.push_back("S;ponad;0");
  v1.push_back("K;6;" + doba_(2).dzis);
  Stan b = stanZWierszy(v1, K());
  SPRAWDZ(b.bledy.size() == 1 && b.p.size() == s.p.size() && b.zaliczone.size() == s.zaliczone.size());
  Stan s15 = s;
  SPRAWDZ(wybierz(K(), b, u, doba_(15)).porada == wybierz(K(), s15, u, doba_(15)).porada);
  // wybór zapamiętany przed narysowaniem przeżywa restart
  Stan c = s;
  dzien(c, u, doba_(16), false);
  Stan cr = stanZWierszy(wierszeStanu(c), K());
  SPRAWDZ(wybierz(K(), cr, u, doba_(16)).porada == wybierz(K(), c, u, doba_(16)).porada);
}

static void testR_DS_44_bezNocy() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 3; n++) {
    Doba d = doba_(n, Tryb::NIEZNANY, {}, false);
    d.bezNocy = true;
    std::string id = dzien(s, u, d);
    SPRAWDZ(plan(id) && plan(id)->dzien == n);
  }
  SPRAWDZ(s.stan == "aktywny" && s.przerwaDni == 0);
}

static void testR_DS_45_zmianaTrybu() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 27; n++) dzien(s, u, doba_(n, Tryb::AUTOSET));
  int ocz = 0;  // porady pokazane w AutoSet, które mają inny wariant ASV
  for (auto& x : s.p) {
    const Porada* a = K().porada(x.id);
    if (!a || a->rodzaj != Rodzaj::PLAN || (a->tryby & TR_WSZYSTKIE)) continue;
    const Porada* b = wariantDoby(K(), a->faza, a->dzien, Tryb::ASV, -1);
    if (b && b != a) ocz++;
  }
  std::vector<std::string> w;
  for (int n = 28; n <= 50; n++) {
    WyborPorady x = wybierz(K(), s, u, doba_(n, Tryb::ASV));
    if (x.zrodlo == Zrodlo::TRYB) w.push_back(x.porada->id);
    zapiszPokazanie(K(), s, x, doba_(n).dzis, false, false);
  }
  SPRAWDZ((int)w.size() == ocz && ocz > 0);
  for (auto& id : w) SPRAWDZ(K().porada(id)->tryby & TR_ASV);
}

static void testR_DS_47_siedemNocy() {
  // wersja 2 (R-DS-47 wycofane, zob. R-DS-58): DS-Z14 zastąpiona pochwałą `M-20/7` (karta serii poza limitem 1 na 7)
  const Porada* z14 = nullptr;
  for (auto& p : K().porady)
    for (auto& t : p.warunek)
      if (t == "M-20/7") z14 = &p;
  SPRAWDZ(z14 && z14->karta && !z14->weta.empty() && z14->rodzaj == Rodzaj::POCHWALA);
  if (!z14) return;
  Stan s;
  Ustawienia u = wlaczony();
  dzien(s, u, doba_(1));
  std::set<std::string> t = {"M-20/7"};
  for (auto& x : z14->weta) t.insert(x);
  SPRAWDZ(dzien(s, u, doba_(8, Tryb::CPAP, t)) != z14->id);
  SPRAWDZ(dzien(s, u, doba_(17, Tryb::CPAP, {"M-20/7"})) == z14->id);
}

static void testR_DS_49_objasnienie() {
  const Porada* z = nullptr;
  for (auto& p : K().porady)
    if (p.rodzaj == Rodzaj::ZDARZENIE && !p.wymagaPorady.empty() && (p.tryby & TR_CPAP) && !p.warunek.empty()) z = &p;
  SPRAWDZ(z != nullptr);
  if (!z) return;
  const Porada* obj = K().porada(z->wymagaPorady);
  SPRAWDZ(obj && obj->dzien > K().koniecStartu + 1);
  if (!obj) return;
  Stan s;
  Ustawienia u = wlaczony();
  int d = K().koniecStartu + 1;  // wersja 2: porada z `!faza/start` dopiero w adaptacji (R-DS-68)
  for (int n = 1; n < d; n++) dzien(s, u, doba_(n));
  WyborPorady w = wybierz(K(), s, u, doba_(d, Tryb::CPAP, {z->warunek[0]}));
  SPRAWDZ(w.porada == obj);
  SPRAWDZ(opis(K(), s, w).srodekBelki == "adaptacja · dzień " + std::to_string(obj->dzien) + " z " +
                                            std::to_string(K().koniecAdaptacji));
  zapiszPokazanie(K(), s, w, doba_(d).dzis, false, false);
  SPRAWDZ(dzien(s, u, doba_(d + 1)) == z->id);  // następnego dnia zaległa porada
  for (int n = d + 2; n <= obj->dzien + 3; n++) SPRAWDZ(dzien(s, u, doba_(n)) != obj->id);  // doba pominięta
  Stan t;  // w starcie ta porada nie staje (`!faza/start`)
  dzien(t, u, doba_(1));
  SPRAWDZ(dzien(t, u, doba_(2, Tryb::CPAP, {z->warunek[0]})) != obj->id);
}

static void testR_DS_50_telefon() {
  Ustawienia u = wlaczony();
  u.telefon = true;
  Stan s;
  dzien(s, u, doba_(1));
  WyborPorady w = wybierz(K(), s, u, doba_(2, Tryb::CPAP, {"E-11"}));
  SPRAWDZ(w.porada && w.porada->ma(FL_OBJAW) && w.telefon);
  OpisPorady o = opis(K(), s, w);
  SPRAWDZ(o.tytul == TYTUL_TELEFON && o.tresc.empty() && o.krok.empty() && o.tydzien == 0 && o.qr.empty());
  zapiszPokazanie(K(), s, w, doba_(2).dzis, false, w.telefon);
  SPRAWDZ(znacznik(K(), s, *w.porada) == "na telefonie " + ddmm(doba_(2).dzis));
  for (int n = 3; n <= 12; n++) {  // porada planu bez flagi `objaw`: pełna; z flagą: „Nowa porada na telefonie.”
    WyborPorady p = wybierz(K(), s, u, doba_(n));
    if (p.porada) SPRAWDZ(p.telefon == (p.porada->ma(FL_OBJAW) && !p.porada->karta));
    zapiszPokazanie(K(), s, p, doba_(n).dzis, false, p.telefon);
  }
}

static void testD0Wieczorem() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 14; n++) dzien(s, u, doba_(n));  // wersja 2: w adaptacji jak w wersji 1
  for (int n = 15; n <= 16; n++) {
    Doba d = doba_(n);
    d.d0 = true;
    SPRAWDZ(dzien(s, u, d).empty());
  }
  Doba d17 = doba_(17);
  d17.d0 = true;
  WyborPorady w = wybierz(K(), s, u, d17);  // trzecia doba z D0: porada planu wieczorem
  SPRAWDZ(w.porada && w.tylkoWieczor && w.porada->dzien == 15);
  Doba d18 = doba_(18, Tryb::CPAP, {"E-11"});
  d18.d0 = true;
  WyborPorady x = wybierz(K(), s, u, d18);
  SPRAWDZ(x.porada && x.zrodlo == Zrodlo::DANE && x.tylkoWieczor);
  // start z D0: rano bez porady, porada z danych czeka, wieczorem porada `start_wieczor` (rozdz. 3 wersji 2)
  Stan t;
  dzien(t, u, doba_(1));
  Doba d2 = doba_(2, Tryb::CPAP, {"E-11"});
  d2.d0 = true;
  SPRAWDZ(!wybierz(K(), t, u, d2).porada && t.z.size() == 1);
  WyborPorady e = wybierzWieczor(K(), t, u, d2);
  SPRAWDZ(e.porada && e.porada->faza == Faza::START_WIECZOR && e.porada->dzien <= 2);
}

// Scenariusze wersji 1 w wersji 2 (przerwa 5 nocy, CPAP → ASV w 20. dobie, tydzień z ekranem L).
static void testScenariusze60() {
  Ustawienia u = wlaczony();
  {  // przerwa 5 nocy (doby 10…14 bez terapii): plan stoi, doba programu bez dób przerwy
    Stan s;
    for (int n = 1; n <= 90; n++) {
      bool przerwa = n >= 10 && n <= 14;
      std::set<std::string> t;
      if (przerwa) t.insert(n == 10 ? "E-10/linia" : "E-10/problem");
      dzien(s, u, doba_(n, Tryb::CPAP, t, !przerwa));
      if (n == 14) SPRAWDZ(s.stan == "przerwa");
      SPRAWDZ(wpisowP(s, doba_(n).dzis) <= 1);
    }
    SPRAWDZ(s.przerwaDni == 4 && s.planKoniec);
  }
  {  // CPAP → ASV w 20. dobie: warianty ASV porad pokazanych w CPAP przed planem
    Stan s;
    int tryb = 0;
    for (int n = 1; n <= 60; n++) {
      WyborPorady w = wybierz(K(), s, u, doba_(n, n < 20 ? Tryb::CPAP : Tryb::ASV));
      if (w.zrodlo == Zrodlo::TRYB) tryb++;
      if (w.porada && n >= 20) SPRAWDZ(w.porada->tryby & (TR_ASV | TR_WSZYSTKIE));
      zapiszPokazanie(K(), s, w, doba_(n).dzis, false, false);
    }
    SPRAWDZ(tryb >= 1);
  }
  {  // tydzień z ekranem L (D0 przez 7 dób) w adaptacji: porada planu wieczorem od 3. doby
    Stan s;
    for (int n = 1; n <= 14; n++) dzien(s, u, doba_(n));
    int wieczorem = 0, bez = 0;
    for (int n = 15; n <= 21; n++) {
      Doba d = doba_(n);
      d.d0 = true;
      WyborPorady w = wybierz(K(), s, u, d);
      if (!w.porada) bez++;
      else if (w.tylkoWieczor) wieczorem++;
      zapiszPokazanie(K(), s, w, d.dzis, true, false);
    }
    SPRAWDZ(bez == 2 && wieczorem == 5);
  }
}

// R-DS-56, R-DS-69 (wersja 2, zamiast R-DS-12): doby z wyłączonym programem nie liczą się do doby programu; panel bez
// poranków do po końcu programu → `zakonczony` bez porad, a poranek w ostatniej dobie programu daje poradę końcową
static void testR_DS_12_wylaczonyITermin() {
  Stan s;
  Ustawienia u = wlaczony(), wyl;
  for (int n = 1; n <= 10; n++) dzien(s, u, doba_(n));
  for (int n = 11; n <= 20; n++) SPRAWDZ(dzien(s, wyl, doba_(n)).empty());
  SPRAWDZ(s.stan == "zatrzymany" && s.przerwaDni == 0);
  SPRAWDZ(!dzien(s, u, doba_(21)).empty() && s.przerwaDni == 10 && s.stan == "aktywny");
  SPRAWDZ(dobaProgramu(s, doba_(21).dzis) == 11);
  int m = K().miejsc;
  // fail-path R-DS-69: przerwa w porankach dłuższa niż SKOK_DNI = `wstrzymany`, nowa data przyjęta po
  // SKOK_POTWIERDZ kolejnych dobach; dopiero wtedy koniec programu albo porada końcowa
  auto poSkoku = [&](Stan& x, int n) {
    for (int i = SKOK_POTWIERDZ - 1; i >= 1; i--) SPRAWDZ(dzien(x, u, doba_(n - i)).empty() && x.stan == "wstrzymany");
    return dzien(x, u, doba_(n));
  };
  Stan t2 = s;
  std::string id = poSkoku(t2, m + 10);
  SPRAWDZ(koncowa() && K().porada(id) && K().porada(id)->baza == koncowa()->baza && t2.stan == "aktywny");
  Stan t3 = s;
  SPRAWDZ(poSkoku(t3, m + 11).empty() && t3.stan == "zakonczony");
  Stan t;
  for (int n = 1; n <= 10; n++) dzien(t, u, doba_(n));
  Stan t4 = t;
  std::string km = poSkoku(t4, m);
  SPRAWDZ(K().porada(km) && K().porada(km)->ma(FL_KONCOWA));
  SPRAWDZ(poSkoku(t, m + 1).empty() && t.stan == "zakonczony");
  printf("  R-DS-56: 10 dób z wyłączonym programem → koniec programu w dobie %d; panel wyłączony do doby %d → zakończony\n",
         m + 10, m + 1);
}

int main() {
  testKatalogWbudowany();
  testR_DS_01_wlacznik();
  testR_DS_02_wariantTrybu();
  testR_DS_05_jednaNaDobe60();
  testR_DS_06_zalegla2Dni();
  testR_DS_07_limit();
  testR_DS_08_stopnieWycieku();
  testR_DS_09_zamiast();
  testR_DS_10_kolejkaPoNarysowaniu();
  testR_DS_11_przerwa();
  testR_DS_12_koniecProgramu();
  testR_DS_12_wylaczonyITermin();
  testR_DS_13_14_przebiegZwykly();
  testR_DS_13_belkaWyzwalanej();
  testR_DS_26_27_28_ankieta();
  testR_DS_29_bezNawilzacza();
  testR_DS_29_dobaBezNawilzacza();
  testR_DS_32_33_wylaczenieIOdNowa();
  testR_DS_34_startOdDnia15();
  testR_DS_36_brakPliku();
  testR_DS_37_uszkodzonyWiersz();
  testR_DS_38_trybNieznany();
  testR_DS_39_dlugoscNaKrojachPanelu();
  testR_DS_42_zegarCofniety();
  testR_DS_43_restart();
  testR_DS_44_bezNocy();
  testR_DS_45_zmianaTrybu();
  testR_DS_47_siedemNocy();
  testR_DS_49_objasnienie();
  testR_DS_50_telefon();
  testD0Wieczorem();
  testScenariusze60();
  printf("testy programu Dobry start: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
