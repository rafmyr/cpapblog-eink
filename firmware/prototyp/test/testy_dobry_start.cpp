// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: program „Dobry start” (spec 5.8.14, R-DS-01…R-DS-52, ADR-0049): katalog porad z zestawu wbudowanego, wybór
// porady dnia, limity, stopnie, przerwa, koniec programu, ankieta, plik stanu, teksty ekranu S-K-48. Scenariusze 60 dób
// na atrapach (bez dyrygenta; dyrygent i ekran: test/testy_dobry_start_brzegowe.cpp). Uruchomienie: tools/prototyp/testy.sh.
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
static std::string dzien(Stan& s, const Ustawienia& u, const Doba& d, bool narysuj = true) {
  WyborPorady w = wybierz(K(), s, u, d);
  if (!w.porada) return "";
  if (narysuj) zapiszPokazanie(K(), s, w, d.dzis, w.tylkoWieczor, w.telefon);
  return w.porada->id;
}
static int wpisowP(const Stan& s, const std::string& dzis) {
  int n = 0;
  for (auto& x : s.p) n += x.data == dzis;
  return n;
}
static const Porada* koncowa() {
  for (auto& p : K().porady)
    if (p.ma(FL_KONCOWA)) return &p;
  return nullptr;
}

static void testKatalogWbudowany() {
  SPRAWDZ(K().jest());
  SPRAWDZ(K().log.empty());
  SPRAWDZ(K().miejsc >= 7 && K().miejsc % 7 == 0);
  SPRAWDZ(K().tygodni() == K().miejsc / 7);
  SPRAWDZ(K().ponadIle > 0);
  SPRAWDZ(!K().pytania.empty());
  SPRAWDZ(koncowa() && koncowa()->dzien == K().miejsc);
  // każde miejsce planu w każdym trybie i stanie nawilżacza ma dokładnie jedną poradę (kontrola buduj_zestaw.py)
  for (Tryb t : {Tryb::CPAP, Tryb::AUTOSET, Tryb::ASV})
    for (int n : {1, 0})
      for (int m = 1; m <= K().miejsc; m++) SPRAWDZ(wariantMiejsca(K(), m, t, n) != nullptr);
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
  Stan s2;
  std::string id = dzien(s2, wlaczony(), doba_(1));
  SPRAWDZ(!id.empty() && K().porada(id)->rodzaj == Rodzaj::PLAN && K().porada(id)->dzien == 1);
  SPRAWDZ(s2.start == START && s2.kolejka == 2);
}

static void testR_DS_02_wariantTrybu() {
  // miejsce z trzema wariantami trybu daje trzy różne porady, miejsce 1 w ASV inną niż w AutoSet
  int m3 = 0;
  for (int m = 1; m <= K().miejsc; m++)
    if (wariantMiejsca(K(), m, Tryb::CPAP, -1) != wariantMiejsca(K(), m, Tryb::AUTOSET, -1)) m3 = m;
  SPRAWDZ(m3 > 0);
  const Porada* a = wariantMiejsca(K(), m3, Tryb::CPAP, -1);
  const Porada* b = wariantMiejsca(K(), m3, Tryb::AUTOSET, -1);
  const Porada* c = wariantMiejsca(K(), m3, Tryb::ASV, -1);
  SPRAWDZ(a && b && c && a != b && b != c && a != c);
  SPRAWDZ(a && a->baza == c->baza);
  SPRAWDZ(wariantMiejsca(K(), 1, Tryb::ASV, -1) != wariantMiejsca(K(), 1, Tryb::AUTOSET, -1));
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
  SPRAWDZ(!a.empty() && K().porada(a)->rodzaj == Rodzaj::ZDARZENIE);
  SPRAWDZ(s.z.size() == 1);
  std::string b = dzien(s, u, doba_(3));
  SPRAWDZ(!b.empty() && K().porada(b)->rodzaj == Rodzaj::ZDARZENIE && b != a);  // zaległa następnego dnia
  // zaległa przepada po 2 dniach: limit 2 / 7 wyczerpany, więc czeka, a 3. dnia znika
  Stan t;
  dzien(t, u, doba_(1));
  dzien(t, u, doba_(2, Tryb::CPAP, {"E-11"}));
  dzien(t, u, doba_(3, Tryb::CPAP, {"S-K-14"}));
  dzien(t, u, doba_(4, Tryb::CPAP, {"E-14"}));  // limit: zaległa
  SPRAWDZ(t.z.size() == 1);
  dzien(t, u, doba_(5));
  dzien(t, u, doba_(6));
  SPRAWDZ(t.z.size() == 1);
  dzien(t, u, doba_(7));
  SPRAWDZ(t.z.empty());
}

static void testR_DS_07_limit() {
  Stan s;
  Ustawienia u = wlaczony();
  dzien(s, u, doba_(1));
  SPRAWDZ(K().porada(dzien(s, u, doba_(2, Tryb::CPAP, {"E-11"})))->rodzaj == Rodzaj::ZDARZENIE);
  SPRAWDZ(K().porada(dzien(s, u, doba_(4, Tryb::CPAP, {"S-K-14"})))->rodzaj == Rodzaj::ZDARZENIE);
  std::string c = dzien(s, u, doba_(6, Tryb::CPAP, {"E-14"}));
  SPRAWDZ(K().porada(c)->rodzaj == Rodzaj::PLAN);  // trzecia w 7 dniach: plan
  // po 28. dobie limit 1 / 7
  Stan t;
  for (int n = 1; n <= 30; n++) dzien(t, u, doba_(n));
  SPRAWDZ(K().porada(dzien(t, u, doba_(31, Tryb::CPAP, {"E-11"})))->rodzaj == Rodzaj::ZDARZENIE);
  SPRAWDZ(K().porada(dzien(t, u, doba_(33, Tryb::CPAP, {"E-14"})))->rodzaj == Rodzaj::PLAN);
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
  SPRAWDZ(K().porada(pokazane[6])->rodzaj != Rodzaj::ZDARZENIE);  // po dwóch najwyższych nic nowego (plan albo ponad)
  // ta sama porada najwyżej raz na 7 dni
  Stan t;
  dzien(t, u, doba_(1));
  std::string a = dzien(t, u, doba_(3, Tryb::CPAP, {"E-11"}));
  std::string b = dzien(t, u, doba_(6, Tryb::CPAP, {"E-11"}));
  SPRAWDZ(K().porada(a)->rodzaj == Rodzaj::ZDARZENIE && b != a);
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
  for (int n = 3; n <= miejsce + 3; n++) miejsca.insert(K().porada(dzien(s, u, doba_(n)))->dzien);
  SPRAWDZ(!miejsca.count(miejsce));
  SPRAWDZ(znacznik(K(), s, *K().porada(z->zamiast)) == "pokazana " + ddmm(doba_(2).dzis));
}

static void testR_DS_10_kolejkaPoNarysowaniu() {
  Stan s;
  Ustawienia u = wlaczony();
  dzien(s, u, doba_(1));
  SPRAWDZ(s.kolejka == 2);
  dzien(s, u, doba_(2), false);  // wybrana, nienarysowana
  SPRAWDZ(s.kolejka == 2 && wpisowP(s, doba_(2).dzis) == 0);
  dzien(s, u, doba_(3));
  SPRAWDZ(s.kolejka == 3);  // nowa doba: ta sama porada planu (miejsce 2)
  Doba d4 = doba_(4);
  d4.w0 = true;  // S5 trzyma dzień i wieczór
  SPRAWDZ(dzien(s, u, d4).empty() && s.kolejka == 3);
}

static void testR_DS_11_przerwa() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 9; n++) dzien(s, u, doba_(n));
  int kol = s.kolejka;
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
  SPRAWDZ(s.stan == "przerwa" && s.kolejka == kol);
  std::string id = dzien(s, u, doba_(19, Tryb::CPAP, {}, true));
  SPRAWDZ(s.stan == "aktywny" && K().porada(id)->rodzaj == Rodzaj::PLAN && K().porada(id)->dzien == kol);
  SPRAWDZ(s.przerwaDni == 8);
}

static void testR_DS_12_koniecProgramu() {
  // przy limitach i D0 kolejka zostaje w tyle: w dobie 56 ostatnia porada planu
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 55; n++) {
    Doba d = doba_(n);
    d.d0 = n % 3 != 0;  // dwie doby z D0, jedna bez: porada planu tylko co trzecią dobę
    dzien(s, u, d);
  }
  SPRAWDZ(!s.planKoniec);
  SPRAWDZ(dzien(s, u, doba_(56)) == koncowa()->id);
  SPRAWDZ(s.planKoniec);
  // pozostałe miejsca: `jeszcze nie`
  int nie = 0;
  for (auto& p : K().porady)
    if (p.rodzaj == Rodzaj::PLAN && znacznik(K(), s, p) == "jeszcze nie") nie++;
  SPRAWDZ(nie > 0);
  SPRAWDZ(K().porada(dzien(s, u, doba_(57)))->rodzaj == Rodzaj::PONAD);
}

static void testR_DS_13_14_przebiegZwykly() {
  Stan s;
  Ustawienia u = wlaczony();
  int m = K().miejsc;
  for (int n = 1; n <= m; n++) {
    WyborPorady w = wybierz(K(), s, u, doba_(n));
    SPRAWDZ(w.porada && w.porada->dzien == n);
    OpisPorady o = opis(K(), s, w);
    SPRAWDZ(o.srodekBelki == "dzień " + std::to_string(n) + " z " + std::to_string(m));
    SPRAWDZ(o.etykieta == "tydzień " + std::to_string((n - 1) / 7 + 1) + " z " + std::to_string(m / 7));
    SPRAWDZ(o.tydzien == (n - 1) / 7 + 1 && o.wTygodniu == (n - 1) % 7 + 1);
    zapiszPokazanie(K(), s, w, doba_(n).dzis, false, false);
  }
  SPRAWDZ(s.planKoniec);
  for (int i = 1; i <= K().ponadIle; i++) {
    WyborPorady w = wybierz(K(), s, u, doba_(m + i));
    SPRAWDZ(w.porada && w.porada->rodzaj == Rodzaj::PONAD);
    OpisPorady o = opis(K(), s, w);
    SPRAWDZ(o.srodekBelki == "ponad program" && o.tydzien == 0);
    SPRAWDZ(o.etykieta == "ponad program · " + std::to_string(i) + " z " + std::to_string(K().ponadIle));
    zapiszPokazanie(K(), s, w, doba_(m + i).dzis, false, false);
  }
  SPRAWDZ(dzien(s, u, doba_(m + K().ponadIle + 1)).empty() && s.stan == "zakonczony");
  // start.ponad=nie: po ostatniej poradzie planu koniec
  Stan t;
  Ustawienia bez = wlaczony();
  bez.ponad = false;
  for (int n = 1; n <= m; n++) dzien(t, bez, doba_(n));
  SPRAWDZ(dzien(t, bez, doba_(m + 1)).empty() && t.stan == "zakonczony");
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
  SPRAWDZ(K().porada(a)->rodzaj == Rodzaj::ANKIETA && K().porada(b)->rodzaj == Rodzaj::ANKIETA && a != b);
  WyborPorady w = wybierz(K(), s, u, doba_(6));
  SPRAWDZ(opis(K(), s, w).srodekBelki == "z twojej ankiety" && opis(K(), s, w).etykieta == "porada do ankiety");
  // odpowiedź wyłączna odznacza pozostałe; nic nie daje porady (R-DS-48)
  zapiszAnkiete(K(), s, doba_(11).dzis, {{wiele->id, {1, wiele->wylaczna}}});
  SPRAWDZ(s.o.back().kody == std::to_string(wiele->wylaczna));
  SPRAWDZ(K().porada(dzien(s, u, doba_(12)))->rodzaj == Rodzaj::PLAN);
  // pytanie z flagą „dwa razy”: ta sama odpowiedź w dwóch kolejnych wypełnionych ankietach
  const Porada* dwa = nullptr;
  for (auto& p : K().porady)
    if (p.ma(FL_DWA_RAZY)) dwa = &p;
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
  // miejsce z wariantem `bez nawilżacza`
  int m = 0;
  for (auto& p : K().porady)
    if (p.rodzaj == Rodzaj::PLAN && p.nawilzacz == -1) m = p.dzien;
  SPRAWDZ(m > 0);
  SPRAWDZ(wariantMiejsca(K(), m, Tryb::CPAP, 0)->nawilzacz == -1);
  SPRAWDZ(wariantMiejsca(K(), m, Tryb::CPAP, -1)->nawilzacz == 1);
  // porady wyzwalane wymagające nawilżacza nie stoją
  for (auto& p : K().porady)
    if (p.rodzaj == Rodzaj::ZDARZENIE && p.nawilzacz == 1) {
      Stan s;
      dzien(s, u, doba_(1));
      std::string id = dzien(s, u, doba_(2, Tryb::CPAP, {p.warunek[0]}));
      SPRAWDZ(id != p.id);
    }
}

static void testR_DS_29_miejsceBezNawilzacza() {
  // porada z `miejsce_bez` stoi w planie bez nawilżacza i wypada wtedy z „ponad program”
  const Porada* z = nullptr;
  for (auto& p : K().porady)
    if (p.miejsceBez > 0) z = &p;
  SPRAWDZ(z && z->rodzaj == Rodzaj::PONAD);
  if (!z) return;
  SPRAWDZ(wariantMiejsca(K(), z->miejsceBez, Tryb::CPAP, 0) == z);
  SPRAWDZ(wariantMiejsca(K(), z->miejsceBez, Tryb::CPAP, 1) != z);
  Ustawienia u = wlaczony();
  u.nawilzacz = 0;
  Stan s;
  for (int n = 1; n <= z->miejsceBez; n++) dzien(s, u, doba_(n));
  WyborPorady w = wybierz(K(), s, u, doba_(z->miejsceBez));
  SPRAWDZ(w.porada == z && opis(K(), s, w).srodekBelki == "dzień " + std::to_string(z->miejsceBez) + " z " +
                                                             std::to_string(K().miejsc));
  SPRAWDZ((int)bazyPonad(K(), s, 0).size() == K().ponadIle - 1);
  Stan t;
  SPRAWDZ((int)bazyPonad(K(), t, 1).size() == K().ponadIle);
  for (int n = 1; n <= K().miejsc; n++) dzien(t, wlaczony(), doba_(n));
  WyborPorady p1 = wybierz(K(), t, wlaczony(), doba_(K().miejsc + 1));
  SPRAWDZ(opis(K(), t, p1).etykieta == "ponad program · 1 z " + std::to_string(K().ponadIle));
}

static void testR_DS_32_33_wylaczenieIOdNowa() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 16; n++) dzien(s, u, doba_(n));
  SPRAWDZ(s.kolejka == 17);
  Ustawienia wyl;
  SPRAWDZ(dzien(s, wyl, doba_(17)).empty() && s.stan == "zatrzymany");
  SPRAWDZ(dzien(s, wyl, doba_(18)).empty());
  std::string id = dzien(s, u, doba_(20));
  SPRAWDZ(s.stan == "aktywny" && K().porada(id)->dzien == 17);
  Stan n = nowyStan(u, K());
  SPRAWDZ(n.kolejka == 1 && n.p.empty() && n.start.empty());
}

static void testR_DS_34_startOdDnia15() {
  Ustawienia u = wlaczony();
  u.dzien = 15;
  Stan s;
  std::vector<const Porada*> podst;
  for (auto& p : K().porady)
    if (p.ma(FL_PODSTAWOWA) && p.dzien < 15 && p.nawilzacz >= 0) podst.push_back(&p);
  std::sort(podst.begin(), podst.end(), [](const Porada* a, const Porada* b) { return a->kol < b->kol; });
  for (size_t i = 0; i < podst.size(); i++) {
    WyborPorady w = wybierz(K(), s, u, doba_((int)i + 1));
    SPRAWDZ(w.porada == podst[i] && w.zrodlo == Zrodlo::PODSTAWOWA);
    SPRAWDZ(opis(K(), s, w).srodekBelki == "dzień " + std::to_string(podst[i]->dzien) + " z " + std::to_string(K().miejsc));
    zapiszPokazanie(K(), s, w, doba_((int)i + 1).dzis, false, false);
  }
  SPRAWDZ(K().porada(dzien(s, u, doba_((int)podst.size() + 1)))->dzien == 15);
  std::string b;
  Ustawienia z;
  SPRAWDZ(ustawienie(z, "start.dzien", "500", b) && z.dzien == 1 && !b.empty());
}

static std::string plikPorad(const std::string& cialo) {
  return "#cpap-eink porady 1 zestaw=t pochodzenie=test skrot=sha256:" + sha256Hex(cialo) + "\n" + cialo;
}
static const char* POLA =
    "id;rodzaj;dzien;tryb;wymaga;ekran;przy;zamiast;warunek;kol;kol_ankiety;flagi;wymaga_porady;stopien_wyzej;miejsce_bez;tytul;"
    "tresc;krok\n";

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
  std::string c = std::string(POLA) + "DS-D01;plan;1;wszystkie;;dane;;;;;;;;;;Tytuł 1;Treść 1;→ Krok.\n" +
                  "DS-D02;plan;2;wszystkie;;dane;;;;;;;;;;Tytuł 2;;→ Krok.\n" +
                  "DS-D03;plan;3;wszystkie;;dane;;;;;;koncowa;;;;Tytuł 3;Treść 3;\n" +
                  "DS-D03;plan;3;wszystkie;;dane;;;;;;;;;;Tytuł 3b;Treść 3b;\n" + "DS-D04;plan;x;wszystkie;;dane\n";
  std::string t = plikPorad(c);
  Katalog k;
  std::string p;
  SPRAWDZ(wczytajKatalog(t.c_str(), "", k, p, true));
  SPRAWDZ(k.porady.size() == 2 && k.log.size() == 3);
  SPRAWDZ(std::find(k.log.begin(), k.log.end(), "DS-D02: brak treści") != k.log.end());
  SPRAWDZ(k.tekst(k.porady[1].tytul) == "Tytuł 3" && k.tekst(k.porady[1].tresc) == "Treść 3");
  Katalog kk = k;  // kopia z własnym tekstem
  SPRAWDZ(kk.tekst(kk.porady[0].krok) == "→ Krok.");
  // miejsce z uszkodzoną poradą przesuwa się bez rysowania
  Stan s;
  Ustawienia u = wlaczony();
  WyborPorady w1 = wybierz(kk, s, u, doba_(1));
  zapiszPokazanie(kk, s, w1, doba_(1).dzis, false, false);
  WyborPorady w2 = wybierz(kk, s, u, doba_(2));
  SPRAWDZ(w2.porada && w2.porada->id == "DS-D03");
}

static void testR_DS_38_trybNieznany() {
  Stan s;
  Ustawienia u = wlaczony();
  int m3 = 0;  // pierwsze miejsce bez porady `wszystkie`
  for (int m = K().miejsc; m >= 2; m--)
    if (!wariantMiejsca(K(), m, Tryb::NIEZNANY, -1)) m3 = m;
  SPRAWDZ(m3 > 1);
  for (int n = 1; n < m3; n++) dzien(s, u, doba_(n));
  std::string id = dzien(s, u, doba_(m3, Tryb::NIEZNANY));
  SPRAWDZ(K().porada(id)->tryby & TR_WSZYSTKIE);
  SPRAWDZ(K().porada(id)->dzien > m3);
}

static void testR_DS_39_dlugoscNaKrojachPanelu() {
  // te same granice co tools/zestaw/porady.py, policzone krojami i zawijaniem panelu (zawin(), szer())
  auto najdl = [](const std::string& t) {  // R-DS-53: najdłuższa wartość progu (ustawienie R-KONF-42 do 15)
    return podstawProgi(t, [](const std::string& id) -> std::string {
      const reguly::Prog* p = reguly::zestawWbudowany().prog(id == "S-K-10.uwaga_o_masce" ? "R-KONF-42.zakres_max" : id,
                                                            reguly::Tryb::NIEZNANY);
      return p ? calkowita(p->wartosc) : "";
    });
  };
  for (auto& p : K().porady) {
    std::string ty = najdl(K().tekst(p.tytul)), tr = najdl(K().tekst(p.tresc)), kr = najdl(K().tekst(p.krok));
    SPRAWDZ(tr.find("{prog:") == std::string::npos);
    SPRAWDZ(szer(F_SB44, ty) <= 748);
    if (p.karta) {
      SPRAWDZ(zawin(F_P22B, tr, 490).size() <= 4);
      SPRAWDZ(szer(F_P22B, kr) <= 490);
    } else {
      SPRAWDZ(zawin(F_P24S, tr, 748).size() <= 5);
      SPRAWDZ(szer(F_P24B, kr) <= 748);
    }
    if (szer(F_SB44, ty) > 748) printf("  %s: tytuł %d px\n", p.id.c_str(), szer(F_SB44, ty));
  }
}

static void testR_DS_42_zegarCofniety() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 10; n++) dzien(s, u, doba_(n));
  int kol = s.kolejka;
  SPRAWDZ(dzien(s, u, doba_(8)).empty() && dzien(s, u, doba_(9)).empty());
  SPRAWDZ(!dzien(s, u, doba_(10)).empty() && s.kolejka == kol);  // porada dnia z dziennika
  SPRAWDZ(!dzien(s, u, doba_(11)).empty() && s.kolejka == kol + 1);
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
  SPRAWDZ(w.porada && w.porada->id == id && w.narysowana && r.kolejka == s.kolejka);
  // plik bez wiersza kolejki: kolejka z dziennika
  std::vector<std::string> bez;
  for (auto& x : wierszeStanu(s))
    if (x.compare(0, 10, "S;kolejka;") != 0) bez.push_back(x);
  Stan b = stanZWierszy(bez, K());
  SPRAWDZ(b.kolejka == 15 && !b.bledy.empty());
  // wybór zapamiętany przed narysowaniem przeżywa restart
  Stan c = s;
  dzien(c, u, doba_(15), false);
  Stan cr = stanZWierszy(wierszeStanu(c), K());
  SPRAWDZ(wybierz(K(), cr, u, doba_(15)).porada == wybierz(K(), c, u, doba_(15)).porada);
}

static void testR_DS_44_bezNocy() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 3; n++) {
    Doba d = doba_(n, Tryb::NIEZNANY, {}, false);
    d.bezNocy = true;
    SPRAWDZ(K().porada(dzien(s, u, d))->dzien == n);
  }
  SPRAWDZ(s.stan == "aktywny" && s.przerwaDni == 0);
}

static void testR_DS_45_zmianaTrybu() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 27; n++) dzien(s, u, doba_(n, Tryb::AUTOSET));
  std::vector<std::string> w;
  for (int n = 28; n <= 40; n++) {
    WyborPorady x = wybierz(K(), s, u, doba_(n, Tryb::ASV));
    if (x.zrodlo == Zrodlo::TRYB) w.push_back(x.porada->id);
    zapiszPokazanie(K(), s, x, doba_(n).dzis, false, false);
  }
  int ocz = 0;
  for (int m = 1; m <= 27; m++) {
    const Porada* a = wariantMiejsca(K(), m, Tryb::AUTOSET, -1);
    const Porada* b = wariantMiejsca(K(), m, Tryb::ASV, -1);
    if (a != b) ocz++;
  }
  SPRAWDZ((int)w.size() == ocz && ocz > 0);
  for (auto& id : w) SPRAWDZ(K().porada(id)->tryby & TR_ASV);
}

static void testR_DS_47_siedemNocy() {
  const Porada* z14 = nullptr;
  for (auto& p : K().porady)
    for (auto& t : p.warunek)
      if (t == "M-20/7") z14 = &p;
  SPRAWDZ(z14 && z14->karta && !z14->weta.empty());
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
  SPRAWDZ(obj && obj->dzien > 5);
  if (!obj) return;
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 4; n++) dzien(s, u, doba_(n));
  WyborPorady w = wybierz(K(), s, u, doba_(5, Tryb::CPAP, {z->warunek[0]}));
  SPRAWDZ(w.porada == obj);
  SPRAWDZ(opis(K(), s, w).srodekBelki == "dzień " + std::to_string(obj->dzien) + " z " + std::to_string(K().miejsc));
  zapiszPokazanie(K(), s, w, doba_(5).dzis, false, false);
  SPRAWDZ(dzien(s, u, doba_(6)) == z->id);  // następnego dnia zaległa porada
  for (int n = 7; n <= obj->dzien + 3; n++) SPRAWDZ(dzien(s, u, doba_(n)) != obj->id);  // miejsce pominięte
}

static void testR_DS_50_telefon() {
  Ustawienia u = wlaczony();
  u.telefon = true;
  Stan s;
  dzien(s, u, doba_(1));
  WyborPorady w = wybierz(K(), s, u, doba_(2, Tryb::CPAP, {"E-11"}));
  SPRAWDZ(w.porada && w.porada->ma(FL_OBJAW) && w.telefon);
  OpisPorady o = opis(K(), s, w);
  SPRAWDZ(o.tytul == TYTUL_TELEFON && o.tresc.empty() && o.krok.empty() && o.tydzien == 0);
  zapiszPokazanie(K(), s, w, doba_(2).dzis, false, w.telefon);
  SPRAWDZ(znacznik(K(), s, *w.porada) == "na telefonie " + ddmm(doba_(2).dzis));
  // porada planu bez flagi `objaw`: pełna
  WyborPorady p = wybierz(K(), s, u, doba_(3));
  SPRAWDZ(p.porada && !p.porada->ma(FL_OBJAW) && !p.telefon);
}

static void testD0Wieczorem() {
  Stan s;
  Ustawienia u = wlaczony();
  for (int n = 1; n <= 4; n++) dzien(s, u, doba_(n));
  int kol = s.kolejka;
  for (int n = 5; n <= 6; n++) {
    Doba d = doba_(n);
    d.d0 = true;
    SPRAWDZ(dzien(s, u, d).empty() && s.kolejka == kol);
  }
  Doba d7 = doba_(7);
  d7.d0 = true;
  WyborPorady w = wybierz(K(), s, u, d7);  // trzecia doba z D0: porada planu wieczorem
  SPRAWDZ(w.porada && w.tylkoWieczor && w.porada->dzien == kol);
  Doba d8 = doba_(8, Tryb::CPAP, {"E-11"});
  d8.d0 = true;
  WyborPorady x = wybierz(K(), s, u, d8);
  SPRAWDZ(x.porada && x.zrodlo == Zrodlo::DANE && x.tylkoWieczor);
}

// Scenariusze 60 dób: przerwa 5 nocy, zmiana trybu CPAP → ASV w 20. dobie, tydzień z ekranem L (D0).
static void testScenariusze60() {
  Ustawienia u = wlaczony();
  {  // przerwa 5 nocy (doby 10…14 bez terapii): plan stoi, termin programu + dni przerwy
    Stan s;
    int kol = 0;
    for (int n = 1; n <= 60; n++) {
      bool przerwa = n >= 10 && n <= 14;
      std::set<std::string> t;
      if (przerwa) t.insert(n == 10 ? "E-10/linia" : "E-10/problem");
      if (n == 10) kol = s.kolejka;
      dzien(s, u, doba_(n, Tryb::CPAP, t, !przerwa));
      if (n == 14) SPRAWDZ(s.stan == "przerwa" && s.kolejka == kol);
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
  {  // tydzień z ekranem L (D0 przez 7 dób): porada planu wieczorem od 3. doby, wyzwalane wieczorem
    Stan s;
    for (int n = 1; n <= 5; n++) dzien(s, u, doba_(n));
    int wieczorem = 0, bez = 0;
    for (int n = 6; n <= 12; n++) {
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

// R-DS-12: doby z wyłączonym programem wydłużają termin jak przerwa w terapii; panel bez poranków do po terminie →
// `zakonczony` bez porad, a poranek w samej 56. dobie jeszcze daje ostatnią poradę planu
static void testR_DS_12_wylaczonyITermin() {
  Stan s;
  Ustawienia u = wlaczony(), wyl;
  for (int n = 1; n <= 10; n++) dzien(s, u, doba_(n));
  for (int n = 11; n <= 20; n++) SPRAWDZ(dzien(s, wyl, doba_(n)).empty());
  SPRAWDZ(s.stan == "zatrzymany" && s.przerwaDni == 0);
  SPRAWDZ(!dzien(s, u, doba_(21)).empty() && s.przerwaDni == 10 && s.stan == "aktywny");
  Stan t;  // termin 56 + 10: poranek w 66. dobie po długim wyłączeniu panelu to jeszcze nie „po terminie”
  Stan t2 = s;
  std::string id = dzien(t2, u, doba_(66));
  SPRAWDZ(koncowa() && K().porada(id) && K().porada(id)->baza == koncowa()->baza && t2.stan == "aktywny");
  Stan t3 = s;
  SPRAWDZ(dzien(t3, u, doba_(67)).empty() && t3.stan == "zakonczony");
  for (int n = 1; n <= 10; n++) dzien(t, u, doba_(n));
  Stan t4 = t;
  std::string k56 = dzien(t4, u, doba_(56));
  SPRAWDZ(K().porada(k56) && K().porada(k56)->ma(FL_KONCOWA));
  SPRAWDZ(dzien(t, u, doba_(57)).empty() && t.stan == "zakonczony");
  printf("  R-DS-12: 10 dób z wyłączonym programem → termin %d. doba; panel wyłączony do 57. doby → zakończony\n",
         KONIEC_DOBA + s.przerwaDni);
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
  testR_DS_29_miejsceBezNawilzacza();
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
