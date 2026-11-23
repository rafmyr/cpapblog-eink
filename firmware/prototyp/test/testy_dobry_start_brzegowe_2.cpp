// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe programu „Dobry start” (spec 5.8.14, ADR-0049): silnik wyboru porady (src/dobry_start.cpp)
// na zestawie wbudowanym, przypadki z tabel R-DS-xx i z fail-path 5.8.14.10.1, sprawdzane niezależnie od
// test/testy_dobry_start.cpp. Testy oznaczone CZERWONY to przypadki dawnych usterek (przypadek 1…9);
// liczone osobno (`czerwone: n` w ostatniej linii) i blokujące, jak pozostałe, dopóki usterka nie jest poprawiona.
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "dobry_start.h"
#include "ekrany.h"
#include "ekrany_kontekst.h"
#include "formularz.h"
#include "magazyn.h"
#include "sekrety.h"
#include "slot_dzienny.h"
#include "wspolne.h"
#include "zestaw_wbudowany.h"

using namespace dobry_start;

static int ok = 0, zle = 0, czerwone = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
// przypadek dawnej usterki: liczony osobno (`czerwone: n`)
#define CZERWONY(rd, w)                                                                   \
  do {                                                                                    \
    if (w) ok++;                                                                          \
    else { czerwone++; printf("CZERWONY %s (%s:%d): %s\n", rd, __FILE__, __LINE__, #w); } \
  } while (0)

static const std::string START = "2026-12-01";
static const Katalog& K() { return katalogWbudowany(); }
static Ustawienia wl() {
  Ustawienia u;
  u.program = true;
  return u;
}
static Doba db(int n, std::set<std::string> tok = {}, Tryb t = Tryb::CPAP, bool terapia = true) {
  Doba d;
  d.dzis = dodajDni(START, n - 1);
  d.tryb = t;
  d.tokeny = tok;
  d.nocZTerapia = terapia;
  return d;
}
static std::string dzien(Stan& s, const Ustawienia& u, const Doba& d, bool rysuj = true, WyborPorady* wy = nullptr) {
  WyborPorady w = wybierz(K(), s, u, d);
  if (wy) *wy = w;
  if (rysuj && w.porada) zapiszPokazanie(K(), s, w, d.dzis, w.tylkoWieczor, w.telefon);
  if (rysuj) {  // wersja 2 (R-DS-55): w fazie start także porada wieczorna
    WyborPorady e = wybierzWieczor(K(), s, u, d);
    if (e.porada && e.porada->faza == Faza::START_WIECZOR && e.zrodlo == Zrodlo::PLAN && !e.narysowana)
      zapiszPokazanie(K(), s, e, d.dzis, true, e.telefon);
  }
  return w.porada ? w.porada->id : "";
}
// wersja 2: porada planu rano w dobie nominalnej m (faza z doby: start, adaptacja, utrwalenie)
static std::string idMiejsca(int m, Tryb t = Tryb::CPAP, int n = -1) {
  Etap e = K().etap(m);
  Faza f = e == Etap::START ? Faza::START_RANO : e == Etap::ADAPTACJA ? Faza::ADAPTACJA : Faza::UTRWALENIE;
  const Porada* p = wariantDoby(K(), f, m, t, n);
  return p ? p->id : "";
}
static int wpisy(const Stan& s, const std::string& baza) {
  int n = 0;
  for (auto& x : s.p) n += x.id.substr(0, x.id.find('.')) == baza;
  return n;
}
static int wpisowDnia(const Stan& s, const std::string& data) {  // wpisy porady dnia (bez wieczornej startu)
  int n = 0;
  for (auto& x : s.p) n += x.data == data && !(K().porada(x.id) && K().porada(x.id)->faza == Faza::START_WIECZOR && x.zrodlo == "plan");
  return n;
}

// ---------- zgodność z przykładami z tabeli R-DS ----------

// R-DS-07: okno kroczące 7 dni, granica 28. doby; zaległa wraca po zwolnieniu limitu
static void brz_R_DS_07_oknoIGranica() {
  Stan s;
  Ustawienia u = wl();
  // wersja 2 (R-DS-68): w fazie start bez limitu 2 na 7, więc przykład przesunięty o 10 dób (adaptacja)
  for (int n = 1; n <= 12; n++) dzien(s, u, db(n));
  SPRAWDZ(dzien(s, u, db(13, {"E-11"})) == "DS-Z04");
  SPRAWDZ(dzien(s, u, db(15, {"E-14"})) == "DS-Z07");
  // doba 19: dwie wyzwalane w oknie (13, 15) → porada planu, DS-Z09 zaległa
  std::string d9 = dzien(s, u, db(19, {"S-K-14"}));
  SPRAWDZ(K().porada(d9) && K().porada(d9)->rodzaj == Rodzaj::PLAN);
  bool zal = false;
  for (auto& z : s.z) zal = zal || z.id == "DS-Z09";
  SPRAWDZ(zal);
  // doba 20: wpis z doby 13 wypada z okna (20 − 13 = 7) → zaległa DS-Z09
  SPRAWDZ(dzien(s, u, db(20)) == "DS-Z09");
  // po 28. dobie limit 1: DS-Z04 w 30., DS-Z13? nie: noc_niepewna wymaga DS-D09 → bierzemy E-14 (DS-Z07) w 36.
  Stan t;
  for (int n = 1; n <= 29; n++) dzien(t, u, db(n));
  SPRAWDZ(dzien(t, u, db(30, {"E-11"})) == "DS-Z04");
  std::string d36 = dzien(t, u, db(36, {"E-14"}));
  SPRAWDZ(K().porada(d36) && K().porada(d36)->rodzaj != Rodzaj::ZDARZENIE);  // 36 − 30 = 6 < 7: limit 1
  SPRAWDZ(dzien(t, u, db(37)) == "DS-Z07");                                   // zaległa z 36., okno zwolnione
  printf("  R-DS-07: okno 7 dni i granica 28. doby zgodne z przykładem\n");
}

// R-DS-08: raz na 7 dni, 2 razy w programie, stopnie Z01 → Z02 → Z03 bez warunku stopnia
static void brz_R_DS_08_stopnie() {
  Stan s;
  Ustawienia u = wl();
  std::map<int, std::string> noce = {{3, "DS-Z01"}, {11, "DS-Z01"}, {20, "DS-Z02"}, {29, "DS-Z02"}, {38, "DS-Z03"}, {47, "DS-Z03"}};
  for (int n = 1; n <= 55; n++) {
    bool wyc = noce.count(n) || n == 55;
    std::string id = dzien(s, u, db(n, wyc ? std::set<std::string>{"S-K-10/problem"} : std::set<std::string>{}));
    if (noce.count(n)) SPRAWDZ(id == noce[n]);
    if (n == 55) SPRAWDZ(!K().porada(id) || K().porada(id)->rodzaj != Rodzaj::ZDARZENIE);  // po dwóch Z03 nic nowego (wersja 2: w utrwaleniu doba może być bez porady)
  }
  // E-11 w nocach 3 i 6: DS-Z04 tylko raz w tych 7 dniach (bez zaległej)
  Stan t;
  for (int n = 1; n <= 2; n++) dzien(t, u, db(n));
  SPRAWDZ(dzien(t, u, db(3, {"E-11"})) == "DS-Z04");
  for (int n = 4; n <= 5; n++) dzien(t, u, db(n));
  SPRAWDZ(dzien(t, u, db(6, {"E-11"})) != "DS-Z04" && dzien(t, u, db(7)) != "DS-Z04");
  // `zamiast`: DS-Z01 przy kolejce 3 zalicza DS-D05 (miejsce 6), przy kolejce 9 nic
  Stan z;
  for (int n = 1; n <= 2; n++) dzien(z, u, db(n));
  dzien(z, u, db(3, {"S-K-10/problem"}));
  std::vector<std::string> dalej;
  for (int n = 4; n <= 7; n++) dalej.push_back(dzien(z, u, db(n)));
  // wersja 2: DS-D05 stoi rano na dobie 3, więc po niej kolejka od doby 4
  SPRAWDZ(dalej[0] == idMiejsca(4) && dalej[1] == idMiejsca(5) && dalej[2] == idMiejsca(6) && dalej[3] == idMiejsca(7));
  SPRAWDZ(znacznik(K(), z, *K().porada("DS-D05")) == "pokazana " + ddmm(db(3).dzis));
  printf("  R-DS-08/09: stopnie Z01→Z02→Z03, okno 7 dni, `zamiast` zgodne z przykładem\n");
}

// R-DS-11: przykład z tabeli (noce 10…20 bez terapii, noc 20 z terapią)
static void brz_R_DS_11_przerwa() {
  Stan s;
  Ustawienia u = wl();
  for (int n = 1; n <= 10; n++) dzien(s, u, db(n));
  SPRAWDZ(dzien(s, u, db(11, {"E-10/linia"}, Tryb::CPAP, false)) == "DS-Z05");
  for (int n = 12; n <= 20; n++) {
    std::string id = dzien(s, u, db(n, {"E-10/linia", "E-10/problem"}, Tryb::CPAP, false));
    SPRAWDZ(id == "DS-Z06");
  }
  std::vector<std::string> daty;
  for (auto& x : s.p)
    if (x.id == "DS-Z06") daty.push_back(x.data);
  SPRAWDZ(daty.size() == 2 && daty[0] == db(12).dzis && daty[1] == db(19).dzis);
  std::string id = dzien(s, u, db(21));
  SPRAWDZ(id == idMiejsca(11) && s.stan == "aktywny");  // wersja 2: doba programu 12, adaptacja od doby 11
  SPRAWDZ(s.przerwaDni == 9);  // doba programu − 9 dób
  printf("  R-DS-11: przerwa zgodna z przykładem (DS-Z06 %zu wpisy, przerwa %d dób)\n", daty.size(), s.przerwaDni);
}

// wersja 2 (R-DS-14 wycofane, zob. R-DS-69): „ponad program” w utrwaleniu: 12 porad z etykietą `porada n z 12`,
// także bez nawilżacza (DS-D29 i DS-D33.bez), potem `zakonczony`
static void brz_R_DS_14_ponad() {
  for (int nawilz : {-1, 0}) {
    Stan s;
    Ustawienia u = wl();
    u.nawilzacz = nawilz;
    std::vector<std::string> etykiety;
    for (int n = 1; n <= 90; n++) {
      WyborPorady w;
      dzien(s, u, db(n), false, &w);
      if (!w.porada) continue;
      if (w.porada->faza == Faza::UTRWALENIE) etykiety.push_back(opis(K(), s, w).etykieta);
      zapiszPokazanie(K(), s, w, db(n).dzis, false, false);
      WyborPorady e = wybierzWieczor(K(), s, u, db(n));
      if (e.porada && e.porada->faza == Faza::START_WIECZOR && e.zrodlo == Zrodlo::PLAN) zapiszPokazanie(K(), s, e, db(n).dzis, true, false);
    }
    SPRAWDZ(etykiety.size() == 12 && s.stan == "zakonczony");
    SPRAWDZ(!etykiety.empty() && etykiety.front() == "porada 1 z 12" && etykiety.back() == "porada 12 z 12");
    SPRAWDZ(wpisy(s, "DS-D29") == 1);
  }
  printf("  R-DS-69: utrwalenie 12 porad z nawilżaczem i bez\n");
}

// R-DS-34: start od dnia 15: podstawowe w kolejności, potem miejsce 15; belka według miejsca porady
static void brz_R_DS_34_start15() {
  Stan s;
  Ustawienia u = wl();
  u.dzien = 15;
  std::vector<std::string> v;
  std::vector<std::string> belki;
  for (int n = 1; n <= 6; n++) {
    WyborPorady w;
    v.push_back(dzien(s, u, db(n), false, &w));
    belki.push_back(opis(K(), s, w).srodekBelki);
    zapiszPokazanie(K(), s, w, db(n).dzis, false, false);
  }
  // wersja 2 (R-DS-34 wycofane, zob. R-DS-65): przykład z R-DS-65
  SPRAWDZ(v == (std::vector<std::string>{"DS-D37", "DS-S07", "DS-D05", "DS-D06", "DS-D09", idMiejsca(15)}));
  SPRAWDZ(belki[0] == "start · dzień 9 z 10" && belki[5] == "adaptacja · dzień 15 z 42");
  printf("  R-DS-34: start od 15: %s … %s\n", v.front().c_str(), v.back().c_str());
}

// R-DS-49: E-30 przy kolejce 5 → DS-D09 (belka `dzień 11 z 42`), następnego dnia DS-Z08; kolejka 11 → DS-D37
static void brz_R_DS_49_objasnienie() {
  Stan s;
  Ustawienia u = wl();
  // wersja 2 (R-DS-68): DS-Z08 nie w fazie start (`!faza/start`), więc przykład w adaptacji
  for (int n = 1; n <= 10; n++) dzien(s, u, db(n));
  WyborPorady w;
  SPRAWDZ(dzien(s, u, db(11, {"E-30"}), true, &w) == "DS-D09");
  SPRAWDZ(opis(K(), s, w).srodekBelki == "adaptacja · dzień 13 z 42");
  SPRAWDZ(dzien(s, u, db(12)) == "DS-Z08");
  std::vector<std::string> v;
  for (int n = 13; n <= 20; n++) v.push_back(dzien(s, u, db(n)));
  SPRAWDZ(std::find(v.begin(), v.end(), "DS-D09") == v.end());
  SPRAWDZ(std::find(v.begin(), v.end(), idMiejsca(11)) != v.end());
  printf("  R-DS-49: objaśnienie DS-D09 przed DS-Z08, potem pominięte w kolejce\n");
}

// R-DS-42: zegar cofnięty z 10.12 na 8.12: 8.12 i 9.12 bez nowej porady, 10.12 z dziennika, 11.12 nowa
static void brz_R_DS_42_zegar() {
  Stan s;
  Ustawienia u = wl();
  for (int n = 1; n <= 10; n++) dzien(s, u, db(n));
  std::string d10;
  for (auto& x : s.p)
    if (x.data == db(10).dzis && x.pora == "dzien") d10 = x.id;
  size_t przed = s.p.size();
  SPRAWDZ(dzien(s, u, db(8)).empty() && dzien(s, u, db(9)).empty());
  SPRAWDZ(dzien(s, u, db(10)) == d10 && s.p.size() == przed);
  SPRAWDZ(!dzien(s, u, db(11)).empty() && s.p.size() == przed + 1);
  // dwa wybudzenia tej samej doby przed narysowaniem: ten sam wybór, jeden wpis
  Stan t;
  std::string a = dzien(t, u, db(1), false), b = dzien(t, u, db(1));
  SPRAWDZ(a == b && wpisowDnia(t, db(1).dzis) == 1 && dzien(t, u, db(1)) == a && wpisowDnia(t, db(1).dzis) == 1);
  printf("  R-DS-42/43: zegar cofnięty i dwa wybudzenia w tej samej dobie bez drugiej porady\n");
}

// R-DS-43 i fail-path pliku stanu: śmieci, ucięte wiersze, bardzo długi wiersz, zły rodzaj; bez kolejki → z dziennika
static void brz_plikStanuUszkodzony() {
  std::vector<std::string> w = {"S;start;2026-12-01", "S;kolejka;abc", "S;stan;dziwny", "P;2026-12-0", "P;2026-12-01;DS-D01;plan;;dzien;panel",
                                "P;2026-12-02;DS-D02;plan;;dzien;panel", std::string(100000, 'x'), "\xff\xfe\x00\x01", "", ";;;;",
                                "Q;1;2;3", "S", "D;2026-12-03;NIE-MA;plan;;0;0", "K;0;2026-12-01", "K;999;2026-12-01"};
  Stan s = stanZWierszy(w, K());
  SPRAWDZ(s.start == START && s.p.size() == 2 && s.stan == "aktywny" && s.bledy.size() >= 8);
  SPRAWDZ(s.zaliczone.empty() && s.wDzis.empty());
  // wersja 2 (R-DS-67): wiersz `S;kolejka` (wersja 1) pominięty, kolejki z dziennika P
  printf("  plik stanu z śmieciami: błędów %zu\n", s.bledy.size());
  Stan b = stanZWierszy({"S;start;2026-12-01", "P;2026-12-01;DS-D01;plan;;dzien;panel", "P;2026-12-02;DS-D02;plan;;dzien;panel"}, K());
  Ustawienia ub = wl();
  SPRAWDZ(dzien(b, ub, db(3)) == idMiejsca(3));
  // pusty stan i plik 0 B: program zaczyna się od nowa bez wyjątku
  Stan p = stanZWierszy({}, K());
  SPRAWDZ(p.start.empty() && p.p.empty());
  // obieg zapis → odczyt bez strat
  Stan r;
  Ustawienia u = wl();
  for (int n = 1; n <= 40; n++) dzien(r, u, db(n, n % 6 == 0 ? std::set<std::string>{"E-11"} : std::set<std::string>{}));
  Stan r2 = stanZWierszy(wierszeStanu(r), K());
  SPRAWDZ(wierszeStanu(r2) == wierszeStanu(r));
}

// ---------- usterki (CZERWONE) ----------

// (R-DS-06, R-DS-10, R-DS-51): porada z danych wybrana w dobie, ale nienarysowana (blok i wieczór zajęte przez
// ekrany z działaniem albo W1…W3), znika: następnego dnia nie jest zaległa, choć jej warunek był spełniony.
static void brz_nienarysowanaZDanychGinie() {
  Stan s;
  Ustawienia u = wl();
  for (int n = 1; n <= 4; n++) dzien(s, u, db(n));
  SPRAWDZ(dzien(s, u, db(5, {"E-11"}), false) == "DS-Z04");  // wybrana, nie narysowana (S-K-01 do 23:30)
  std::string d6 = dzien(s, u, db(6));
  printf("  przypadek 1: doba 5 DS-Z04 nienarysowana, doba 6: %s (oczekiwane DS-Z04 jako zaległa, R-DS-06)\n", d6.c_str());
  CZERWONY("przypadek 1", d6 == "DS-Z04");
}

// (R-DS-12, fail-path „panel wyłączony dłużej niż do terminu programu”): spec każe przy pierwszym starcie stan
// `zakonczony` bez porad; silnik pokazuje DS-D42.
static void brz_poTerminiePanelWylaczony() {
  Stan s;
  Ustawienia u = wl();
  for (int n = 1; n <= 10; n++) dzien(s, u, db(n));
  WyborPorady w;
  // wersja 2: koniec programu po dobie 84 (R-DS-69); po skoku daty o > SKOK_DNI najpierw `wstrzymany`, nowa data
  // przyjęta po SKOK_POTWIERDZ kolejnych dobach
  for (int n = 95 - SKOK_POTWIERDZ + 1; n < 95; n++) SPRAWDZ(dzien(s, u, db(n)).empty() && s.stan == "wstrzymany");
  std::string id = dzien(s, u, db(95), false, &w);
  printf("  przypadek 2: pierwszy start w 95. dobie: porada %s, stan %s (oczekiwane: brak porady, zakonczony)\n",
         id.empty() ? "-" : id.c_str(), s.stan.c_str());
  CZERWONY("przypadek 2", id.empty() && s.stan == "zakonczony");
}

// (R-DS-05, R-DS-33): „Zacznij od nowa” w dobie, w której porada już stała, daje drugą nową poradę tej samej
// doby (formularz mówi „od najbliższego dnia”).
static void brz_odNowaDrugaPoradaTejDoby() {
  Stan s;
  Ustawienia u = wl();
  for (int n = 1; n <= 6; n++) dzien(s, u, db(n));
  std::string przed = s.p.back().id;
  Stan nowy = nowyStan(u, K());  // to robi odNowaB49() w dyrygencie
  std::string po = dzien(nowy, u, db(6));
  printf("  przypadek 3: doba 6 przed „od nowa” %s, po „od nowa” tej samej doby %s (oczekiwane: nic do jutra)\n", przed.c_str(),
         po.empty() ? "-" : po.c_str());
  CZERWONY("przypadek 3", po.empty());
}

// (fail-path R-DS-29), wersja 2 (bez DS-D45 i `miejsce_bez`): `start.nawilzacz` zmieniony z `tak` na `nie` po
// DS-D06 → DS-D06.bez się nie pokazuje (doba już pokazana), dalsze porady w wariancie bez nawilżacza
static void brz_D29GinieZmianaNawilzacza() {
  Stan s;
  Ustawienia u = wl();
  u.nawilzacz = 1;
  for (int n = 1; n <= 12; n++) dzien(s, u, db(n));
  SPRAWDZ(wpisy(s, "DS-D06") == 1);
  u.nawilzacz = 0;
  for (int n = 13; n <= 90; n++) dzien(s, u, db(n));
  int bez = 0;
  for (auto& x : s.p) bez += x.id == "DS-D06.bez";
  printf("  przypadek 4: DS-D06.bez pokazana %d razy, DS-D19.bez %d (oczekiwane 0 i 1)\n", bez, wpisy(s, "DS-D19"));
  CZERWONY("przypadek 4", bez == 0 && wpisy(s, "DS-D29") == 1);
  for (auto& x : s.p)
    if (x.data >= db(13).dzis) SPRAWDZ(K().porada(x.id)->nawilzacz != 1);
}

// (R-DS-37): dwa wiersze planu na tym samym miejscu i w tym samym wariancie, gdy jeden ma `tryb=wszystkie`,
// a drugi `CPAP`: maska TR_WSZYSTKIE nie ma wspólnych bitów z TR_CPAP, więc wiersz nie jest odrzucony.
static void brz_wszystkieIKonkretnyTryb() {
  std::string pelny = reguly::wbudowany::PORADY;
  size_t nl = pelny.find('\n');
  std::string cialo = pelny.substr(nl + 1);
  const Porada* d2 = K().porada("DS-D02");
  SPRAWDZ(d2 && (d2->tryby & TR_WSZYSTKIE));
  cialo += "DS-X99;plan;2;CPAP;;dane;;;;;;;;;;Tytuł;Treść;;start_rano;rano;;\n";
  std::string plik = "#cpap-eink porady 1 zestaw=z-test skrot=sha256:" + sha256Hex(cialo) + "\n" + cialo;
  Katalog k;
  std::string powod;
  SPRAWDZ(wczytajKatalog(plik.c_str(), nullptr, k, powod, true));
  bool pominiety = !k.porada("DS-X99");
  printf("  przypadek 5: wiersz CPAP na miejscu 2 obok `wszystkie`: %s\n", pominiety ? "pominięty" : "przyjęty");
  CZERWONY("przypadek 5", pominiety);
  // te same wejścia z błędnymi wierszami (R-DS-37): ucięty wiersz, nieznany tryb, śmieci, bardzo długi wiersz
  std::string c2 = pelny.substr(nl + 1) + "DS-X98;plan;43;CPAP\n" + "DS-X97;plan;43;VAuto;;dane;;;;;;;;T;X;;utrwalenie;;\n" +
                   std::string("\x01\x02\xff;") + "\n" + std::string(70000, 'a') + "\n" + "DS-D02;plan;43;wszystkie;;dane;;;;;;;;T;X;;utrwalenie;;\n";
  std::string p2 = "#cpap-eink porady 1 zestaw=z-test skrot=sha256:" + sha256Hex(c2) + "\n" + c2;
  Katalog k2;
  SPRAWDZ(wczytajKatalog(p2.c_str(), nullptr, k2, powod, true));
  SPRAWDZ(k2.porady.size() == K().porady.size() && k2.log.size() == 5 && k2.miejsc == K().miejsc);
  // skrót niezgodny, zła wersja, 0 B: odrzucone w całości
  Katalog k3;
  SPRAWDZ(!wczytajKatalog((p2 + "x").c_str(), nullptr, k3, powod, true));
  SPRAWDZ(!wczytajKatalog(("#cpap-eink porady 2 skrot=sha256:" + sha256Hex(c2) + "\n" + c2).c_str(), nullptr, k3, powod, true));
  SPRAWDZ(!wczytajKatalog("", nullptr, k3, powod, true) && !wczytajKatalog(nullptr, nullptr, k3, powod, true));
}

// (R-DS-45): przykład z tabeli: AutoSet do miejsca 29, od nocy 32 ASV z DS-Z12 → DS-Z12, potem DS-D01.asv,
// DS-D46.asv, DS-D23.asv, DS-D24.asv, potem miejsce 30. Silnik najpierw pokazuje DS-D25 (objaśnienie DS-Z12, R-DS-49),
// więc kolejność odbiega od przykładu: sprzeczność R-DS-45 z R-DS-49 w spec albo usterka.
static void brz_zmianaTrybuPrzyklad() {
  Stan s;
  Ustawienia u = wl();
  int n = 1;
  while (n <= 29) dzien(s, u, db(n++, {}, Tryb::AUTOSET));  // wersja 2: AutoSet do doby 29 (rano i wieczorem w starcie)
  // kolejność wariantów ASV z dziennika P (porady planu pokazane w AutoSet, bez `wszystkie`, R-DS-45)
  std::vector<std::string> ocz;
  for (auto& x : s.p) {
    const Porada* p = K().porada(x.id);
    if (!p || p->rodzaj != Rodzaj::PLAN || (p->tryby & TR_WSZYSTKIE)) continue;
    const Porada* a = wariantDoby(K(), p->faza, p->dzien, Tryb::ASV, u.nawilzacz);
    if (a && a->id != p->id && std::find(ocz.begin(), ocz.end(), a->id) == ocz.end()) ocz.push_back(a->id);
  }
  std::vector<std::string> v;
  v.push_back(dzien(s, u, db(n++, {"M-21"}, Tryb::ASV)));
  for (int i = 0; i < 5; i++) v.push_back(dzien(s, u, db(n++, {}, Tryb::ASV)));
  std::string sv;
  for (auto& x : v) sv += x + " ";
  printf("  przypadek 6: po zmianie AutoSet → ASV: %s\n", sv.c_str());
  // oczekiwanie według poprawionego przykładu R-DS-45 (rozstrzygnięcie przypadku 6: kod zgodny z R-DS-49, poprawiony przykład
  // w spec): DS-D25 (miejsce 30, objaśnia DS-Z12) przed DS-Z12, potem warianty nowego trybu
  // wersja 2: DS-D25 (objaśnienie DS-Z12) stoi już w dobie 29, więc najpierw DS-Z12, potem po jednej na dobę warianty
  // ASV porad pokazanych w AutoSet (R-DS-45) w kolejności dziennika
  bool dobre = v.size() == 6 && v[0] == "DS-Z12" && ocz.size() >= 5;
  for (size_t i = 1; dobre && i < v.size(); i++) dobre = v[i] == ocz[i - 1];
  CZERWONY("przypadek 6", dobre);
}

// Formularz (R-DS-31…R-DS-35, R-KONF-13): złe wejście w polach sekcji i na podstronach, kodowanie HTML, bez skryptu
static void brz_formularzZleWejscie() {
  for (const char* d : {"0", "85", "-1", "abc", "9999999999", " 5", "5 ", "+5", "4.0", "１"}) {
    formularz::Wynik w = formularz::waliduj({{"start_dzien", d}}, sekrety::Dane(), "2026-12-01");
    SPRAWDZ(w.bledy.count("start_dzien") && !w.plik.count("start.dzien"));
  }
  formularz::Wynik pusty = formularz::waliduj({{"start_dzien", ""}}, sekrety::Dane(), "2026-12-01");
  SPRAWDZ(pusty.ok() && pusty.plik.count("start.dzien") && pusty.plik["start.dzien"].empty());
  formularz::Wynik d42 = formularz::waliduj({{"start_dzien", "42"}}, sekrety::Dane(), "2026-12-01");
  SPRAWDZ(d42.ok() && d42.plik["start.dzien"] == "42");
  for (const char* pole : {"start_program", "start_nawilzacz", "start_objawy"}) {  // wersja 2: bez `start_ponad` (R-DS-66)
    formularz::Wynik w = formularz::waliduj({{pole, "<script>"}}, sekrety::Dane(), "2026-12-01");
    SPRAWDZ(w.bledy.count(pole) == 1);
  }
  // klucz `start.*` z pliku ze złą wartością: domyślna i błąd w logu ustawień
  UstawieniaDnia ud = parsujUstawieniaDnia("start.program=moze\nstart.dzien=abc\nstart.nawilzacz=\nstart.zle=1\n");
  SPRAWDZ(!ud.start.program && ud.start.dzien == 1 && ud.start.nawilzacz == -1 && ud.bledy.size() == 4);
  // podstrony: zły token, ukośnik na końcu, inne nazwy → 404 i licznik złych żądań; zakładka spoza zakresu → tydzień 1
  formularz::Sesja s;
  s.otworz("tok", 0, "1.2.3.4");
  SPRAWDZ(s.obsluz("GET", "/u/zly/ankieta", "", 1, "").kod == 404);
  SPRAWDZ(s.obsluz("GET", "/u/tok/ankieta/", "", 1, "").kod == 404);
  SPRAWDZ(s.obsluz("GET", "/u/tok/../ankieta", "", 1, "").kod == 404);
  formularz::Odpowiedz z = s.obsluz("GET", "/u/tok/porady?t=99999999999", "", 1, "");
  SPRAWDZ(z.kod == 200 && z.podstrona == "porady");
  std::string h = formularz::stronaPorad(K(), Stan(), wl(), Tryb::CPAP, z.zakladka, "/u/tok/", nullptr, nullptr);
  SPRAWDZ(h.find("<b>Start, rano</b>") != std::string::npos && h.find("<script") == std::string::npos);  // wersja 2: zakładki faz
  // kodowanie HTML znaków z tekstów porad
  SPRAWDZ(formularz::html("<a & \"b\"> → ▶") == "&lt;a &amp; &quot;b&quot;&gt; → ▶");
  // wszystkie strony porad w każdym trybie i stanie nawilżacza w budżecie 8 KB, każda porada na jakiejś stronie
  size_t maks = 0;
  for (Tryb t : {Tryb::CPAP, Tryb::AUTOSET, Tryb::ASV, Tryb::NIEZNANY})
    for (int n : {-1, 0, 1}) {
      Ustawienia u = wl();
      u.nawilzacz = n;
      for (int i = 1; i <= K().tygodni() + 3; i++)
        maks = std::max(maks, formularz::stronaPorad(K(), Stan(), u, t, i, "/u/tok/", nullptr, nullptr).size());
    }
  printf("  formularz: najdłuższa strona „Wszystkie porady” we wszystkich trybach %zu B (próg %zu B)\n", maks,
         formularz::MAKS_STRONY_PORAD);
  SPRAWDZ(maks <= formularz::MAKS_STRONY_PORAD);
  // ankieta: `nic z tych rzeczy` odznacza pozostałe bez skryptu; odpowiedź spoza listy odrzucona
  Stan a;
  a.start = START;
  const Pytanie* q4 = K().pytanieId("DS-Q04");
  SPRAWDZ(q4 && q4->wylaczna > 0);
  if (q4) {
    zapiszAnkiete(K(), a, db(5).dzis, {{"DS-Q04", {1, q4->wylaczna, 99, -3}}, {"DS-QXX", {1}}});
    SPRAWDZ(a.o.size() == 1 && a.o[0].kody == std::to_string(q4->wylaczna));
  }
  auto odp = formularz::odpowiedziAnkiety(K(), {{"q_DS-Q03", "7"}, {"q_DS-Q03_1", "1"}, {"ankieta_DS-Q03", "1"}});
  SPRAWDZ(odp.size() == 1 && odp[0].second.empty());
}

// (R-DS-18): linia uwagi pod belką nachodzi na tytuł S-K-48: czarny pasek linii (48 px, przy wzorcu A 60 px)
// kończy się niżej niż górne krawędzie liter tytułu (SB44, linia bazowa 132, a przy 60 px 144), więc wydłużenia i
// znaki diakrytyczne (Ś, Ł, l, d, b) giną w pasku. Spec: „bez nakładania linii na tytuł”, „0 nakładań w renderze z linią”.
struct PlotnoR : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)(y * 800 + x)] = c;
  }
  int wiersz(int y, int x0 = 0, int x1 = 800) const {
    int n = 0;
    for (int x = x0; x < x1; x++) n += px[(size_t)(y * 800 + x)];
    return n;
  }
};
static void brz_liniaUwagiNaTytule() {
  int nakladan = 0, porad = 0, maksNakladania = 0;
  std::string przyklad;
  for (auto& x : K().porady) {
    if (x.karta) continue;
    for (bool a : {false, true}) {
      WyborPorady w;
      w.porada = &x;
      w.zrodlo = x.rodzaj == Rodzaj::PLAN ? Zrodlo::PLAN : Zrodlo::DANE;
      w.dataNocy = START;
      DanePorady bez;
      bez.o = opis(K(), Stan(), w);
      bez.o.etykieta.clear();  // sam tytuł nad treścią: górna krawędź liter bez etykiety i paska
      bez.o.tydzien = 0;
      bez.belka = Belka(NAZWA_BELKI, bez.o.srodekBelki);
      PlotnoR p0;
      rysujPorade(p0, bez);
      int gora = -1;
      for (int y = 44; y < 182 && gora < 0; y++)
        if (p0.wiersz(y, 28, 776)) gora = y;
      DanePorady z = bez;
      z.linia = "Bateria 15%. Podłącz kabel.";
      z.liniaA = a;
      z.liniaOstrzezenie = true;
      PlotnoR p1;
      rysujPorade(p1, z);
      int dol = -1;
      for (int y = 44; y < 182; y++)
        if (p1.wiersz(y) == 800) dol = y;
      int dy = a ? 12 : 0;
      porad++;
      if (gora >= 0 && dol >= gora + dy) {
        nakladan++;
        maksNakladania = std::max(maksNakladania, dol - (gora + dy) + 1);
        if (przyklad.empty()) przyklad = x.id + (a ? " (wzorzec A)" : "") + ": pasek do y " + std::to_string(dol) +
                                         ", tytuł od y " + std::to_string(gora + dy);
      }
    }
  }
  printf("  przypadek 9: linia uwagi nachodzi na tytuł w %d z %d renderów (do %d px), np. %s\n", nakladan, porad, maksNakladania,
         przyklad.c_str());
  CZERWONY("przypadek 9", nakladan == 0);
}

int main() {
  brz_liniaUwagiNaTytule();
  brz_formularzZleWejscie();
  brz_R_DS_07_oknoIGranica();
  brz_R_DS_08_stopnie();
  brz_R_DS_11_przerwa();
  brz_R_DS_14_ponad();
  brz_R_DS_34_start15();
  brz_R_DS_49_objasnienie();
  brz_R_DS_42_zegar();
  brz_plikStanuUszkodzony();
  brz_nienarysowanaZDanychGinie();
  brz_poTerminiePanelWylaczony();
  brz_odNowaDrugaPoradaTejDoby();
  brz_D29GinieZmianaNawilzacza();
  brz_wszystkieIKonkretnyTryb();
  brz_zmianaTrybuPrzyklad();
  printf("testy_dobry_start_brzegowe_2: %d ok, %d błędów, czerwone: %d\n", ok, zle, czerwone);
  return (zle || czerwone) ? 1 : 0;  // czerwone = przypadki dawnych usterek: blokują, dopóki nie są poprawione
}
