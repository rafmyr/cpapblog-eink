// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (SPRZĘT, INFEKCJA, pozostałe S-K). Każdy test usterki pada z numerem R16-nn,
// dopóki usterka nie zostanie poprawiona. Testy „trzyma”
// (bez numeru, makro TRZYMA) pilnują rzeczy sprawdzonych w przeglądu. Dane wyłącznie syntetyczne.
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "humor.h"
#include "infekcja.h"
#include "kontekst_sk.h"
#include "sprzet.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define PRZYPADEK(nr, w)                                                                  \
  do {                                                                                     \
    if (w) ok++;                                                                           \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); }    \
  } while (0)
#define TRZYMA(w)                                                                          \
  do {                                                                                     \
    if (w) ok++;                                                                           \
    else { zle++; printf("REGRES (%s:%d): %s\n", __FILE__, __LINE__, #w); }               \
  } while (0)

static const std::string D0 = "2026-09-01";
static std::string d(int i) { return dodajDni(D0, i); }

static Noc noc(int i, float wp95 = 10) {
  Noc n;
  n.data = d(i);
  n.wazna = true;
  n.m01 = 420;
  n.ahi = 1.0f;
  n.wmed = 4;
  n.wp95 = wp95;
  n.cp95 = 10;
  n.ep95 = 8;
  n.m13 = 14;
  n.m17 = 0;
  n.m08 = 0;
  n.ustawienia = "AutoSet/5/12";
  return n;
}
static std::vector<Noc> historia(int ile, float wp95 = 10) {
  std::vector<Noc> h;
  for (int i = 0; i < ile; i++) h.push_back(noc(i, wp95));
  return h;
}
static bool jest(const std::vector<sk::Tresc>& v, const std::string& id) {
  for (auto& t : v)
    if (t.id == id) return true;
  return false;
}
static WpisHumoru wpis(const std::string& data, const std::string& typ, const std::string& klucz) {
  WpisHumoru w;
  w.data = data;
  w.typ = typ;
  w.klucz = klucz;
  w.tryb = 'D';
  return w;
}

// okres infekcji kończy się po 14 nocach (R-INF-04), ale następnego ranka S-K-30 wraca (oddechy i
// ciśnienie nadal wyżej niż norma bez nocy okresu) i wyklucza S-K-17 (R5a); kolejne „tak” otwiera nowy okres, więc
// L-03, L-09, E-20…E-22, S-K-10, S-K-14, S-K-16 i S-K-31 są wstrzymane dłużej niż 14 nocy (5.10.3: „wstrzymanie trwa
// najwyżej 14 nocy”; R-INF-04: „od nocy 15 S-K-17 i L-05 działają”).
static void testR16_01() {
  std::vector<Noc> h = historia(70);
  for (int i = 30; i < 70; i++) {
    h[(size_t)i].m13 = 17;
    h[(size_t)i].cp95 = 12;
  }
  std::vector<infekcja::Okres> o1 = infekcja::okresy(h, {d(31)}, {}, d(45), false);
  TRZYMA(o1.size() == 1 && o1[0].koniec == d(43) && o1[0].zakonczony);  // R-INF-04: koniec po 14 nocach
  TRZYMA(infekcja::okresDzis(o1, d(45)) == nullptr);
  // rano po nocy 45 (dwie noce po końcu okresu): wejście jak w dyrygencie (norma bez nocy okresu)
  Norma nm = liczNorme(h, d(45), infekcja::noceOkresow(o1, d(45)));
  sk::Wejscie w;
  w.dzis = d(45);
  w.hist = &h;
  w.norma = &nm;
  w.trybProbny = false;
  w.okresy = &o1;
  w.okresInfekcji = false;
  std::vector<sk::Tresc> v = sk::rano(w);
  PRZYPADEK("okres infekcji kończy się po 14 nocach", !jest(v, "S-K-30"));  // po okresie z podwyższonymi oddechami: zwykła ścieżka S-K-17 i L-05
  // łańcuch: „tak” na S-K-30 dzień po końcu okresu otwiera następny okres
  std::vector<infekcja::Okres> o2 = infekcja::okresy(h, {d(31), d(45)}, {}, d(69), false);
  int wstrzymane = 0;
  for (int i = 30; i < 70; i++)
    if (infekcja::okresDzis(o2, d(i))) wstrzymane++;
  printf("  poranków z okresem infekcji w nocach 30…69 przy dwóch „tak”: %d (najwyżej 14)\n", wstrzymane);
  PRZYPADEK("okres infekcji kończy się po 14 nocach", wstrzymane <= 14);
}

// przy poduszce i masce jednocześnie po terminie S-K-31 dotyczy poduszki (R-MAS-07), więc licznik
// pokazań maski nigdy nie rośnie, a blokadaL09 trzyma L-09 bez końca mimo 4 pokazań (R-MAS-10,: „Blokada nie
// trwa w nieskończoność”). To zwykły przypadek: formularz U-02 zapisuje obie daty z jednego pola (R-KONF-34).
static void testR16_02() {
  std::vector<Noc> h = historia(80, 12);
  Norma nm = liczNorme(h, d(79), {});
  sprzet::Elementy el;
  // poduszka i maska to jeden element, więc remis (R-MAS-07) nie istnieje; test zostaje jako
  // kontrola, że 4 pokazania S-K-31 maski zwalniają L-09 (jeden licznik)
  el.el[EL_MASKA] = {"2026-01-01", 6, false};
  std::string spr = sprzet::sprawaElementu(EL_MASKA, el.el[EL_MASKA]);
  std::vector<WpisHumoru> dz = {wpis(d(50), "SK31", spr), wpis(d(57), "SK31", spr), wpis(d(64), "SK31", spr),
                                wpis(d(71), "SK31", spr)};
  sprzet::WejscieSprzetu ws;
  ws.dzis = d(79);
  ws.hist = &h;
  ws.norma = &nm;
  ws.el = &el;
  ws.dziennik = &dz;
  TRZYMA(sprzet::pokazaniaSK31(dz, spr) == 4);
  PRZYPADEK("przy poduszce i masce jednocześnie po terminie S-K-31", !sprzet::blokadaL09(ws));
}

// wysoki, ale płaski wyciek (L-09 ma warunek: E-22 albo S-K-10 „od kilku nocy”, bez trendu)
// przy poduszce po terminie: S-K-31 nie wchodzi (warunek (3) „rośnie”), licznik stoi na 0, a L-09 jest wstrzymane bez
// końca. Fail-path R-MAS-03 w spec zakłada, że „L-09 i tak nie ma warunku (wyciek nie trwa)”, co dla płaskiego wycieku
// 30 L/min nie jest prawdą. To samo przy M-07 bez normy (zmiana ustawień, nowy model maski) i w okresie infekcji.
static void testR16_03() {
  std::vector<Noc> h = historia(80, 30);
  Norma nm = liczNorme(h, d(79), {});
  nm.wp95.med = 12;  // norma sprzed pogorszenia (wyciek stoi wysoko od dawna)
  sprzet::Elementy el;
  el.el[EL_MASKA] = {"2026-01-01", 6, false};
  std::vector<WpisHumoru> dz;
  sprzet::WejscieSprzetu ws;
  ws.dzis = d(79);
  ws.hist = &h;
  ws.norma = &nm;
  ws.el = &el;
  ws.dziennik = &dz;
  sprzet::WynikSK31 r = sprzet::ocenSK31(ws);
  TRZYMA(!r.warunek);  // płaski wyciek: S-K-31 milczy (zgodnie z 5.9.4)
  PRZYPADEK("wysoki, ale płaski wyciek", !sprzet::blokadaL09(ws));  // … a L-09 („Wyciek nie ustępuje”) wstrzymane na zawsze
}

// pytanie `Wymieniona poduszka?` liczy spadek bez nocy okresu infekcji (R-MAS-12), a zapis daty po „tak”
// (sprzet::zOdpowiedziami) liczy ten sam spadek z nocami okresu. Gdy noce okresu leżą w oknie 14 nocy, „tak”
// przepada bez śladu: data wymiany nie powstaje, S-K-32 i R-MAS-08 nie ruszają.
static void testR16_05() {
  std::vector<Noc> h = historia(40, 26);
  std::vector<std::string> inf;
  for (int i = 22; i < 30; i++) {
    h[(size_t)i].wp95 = 10;
    inf.push_back(d(i));
  }
  for (int i = 37; i < 40; i++) h[(size_t)i].wp95 = 15;
  Norma nm = liczNorme(h, d(39), {});
  sprzet::Elementy el;
  sprzet::WejscieSprzetu ws;
  ws.dzis = d(39);
  ws.hist = &h;
  ws.norma = &nm;
  ws.el = &el;
  sprzet::WynikPytaniaPoduszki q = sprzet::ocenPytaniePoduszki(ws, {}, inf, "");
  TRZYMA(q.jest && q.odNocy == d(37));
  ElementWymiany baza[EL_N];
  sprzet::Elementy po = sprzet::zOdpowiedziami(baza, h, {d(39)});
  PRZYPADEK("pytanie `Wymieniona poduszka?` liczy spadek bez nocy okresu", po.el[EL_MASKA].wymieniono == d(37));
}

// koniec miesiąca. Wymiana 31.08, okres 6 mies. → termin 28.02 (przycięty), więc 28.02 element jest po
// terminie, a wiek liczy się jako 5 pełnych miesięcy: S-K-31 `Ma 5 mies.` i negatyw w „Sprzęt” przy `5 mies.` /
// `6 mies.` (sprzeczne dla użytkownika).
static void testR16_06() {
  ElementWymiany e;
  e.wymieniono = "2026-08-31";
  e.okres = 6;
  bool po = sprzet::poTerminie(e, "2027-02-28");
  int m = sprzet::pelneMiesiace(e.wymieniono, "2027-02-28");
  PRZYPADEK("koniec miesiąca", !po || m >= e.okres);
}

// ---------- trzyma ----------
static void testTrzyma() {
  // zerowanie normy M-07 (nowy model, R-MAS-11) nie chowa wycieku ponad próg bezwzględny: S-K-10 (24 L/min) działa
  std::vector<Noc> h = historia(40, 10);
  for (int i = 33; i < 40; i++) h[(size_t)i].wp95 = 30;
  Norma nm = liczNorme(h, d(39), {});
  nm.wp95 = NormaMetryki();  // norma M-07 pusta
  sk::Wejscie w;
  w.dzis = d(39);
  w.hist = &h;
  w.norma = &nm;
  w.trybProbny = false;
  TRZYMA(jest(sk::rano(w), "S-K-10"));
  // … ale w okresie infekcji S-K-10 i S-K-31 milczą (R-INF-02)
  w.okresInfekcji = true;
  TRZYMA(!jest(sk::rano(w), "S-K-10"));
  // S-K-30 w trybie ASV bierze M-06p (EPAP), nie M-05p (4.11)
  std::vector<Noc> a = historia(40);
  for (int i : {38, 39}) a[(size_t)i].m13 = 17;
  a[39].cp95 = 14;  // ciśnienie (M-05p) wyżej, EPAP bez zmian
  Norma na = liczNorme(a, d(39), {});
  TRZYMA(!infekcja::ocenSK30(a, na, d(39), true).warunek);
  TRZYMA(infekcja::ocenSK30(a, na, d(39), false).warunek);
  a[39].ep95 = 11;
  TRZYMA(infekcja::ocenSK30(a, na, d(39), true).warunek);
  // jeden okres przy oddechach podwyższonych bez końca kończy się po 14 nocach kalendarzowych (R-INF-04)
  std::vector<Noc> b = historia(70);
  for (int i = 30; i < 70; i++) {
    b[(size_t)i].m13 = 17;
    b[(size_t)i].cp95 = 12;
    if (i % 3 == 0) b[(size_t)i].wazna = false;  // noce nieważne nie przedłużają okresu
  }
  std::vector<infekcja::Okres> o = infekcja::okresy(b, {d(31)}, {}, d(69), false);
  TRZYMA(!o.empty() && o[0].koniec == dodajDni(o[0].od, 13));
  // cofnięcie „tak” (R-PRZ-13) = brak odpowiedzi w pliku: okresu nie ma, bez osobnego stanu
  TRZYMA(infekcja::okresy(b, {}, {}, d(69), false).empty());
  // „wymienione dziś” dwa razy tego samego dnia: jeden wiersz (fail-path R-MAS-01); reszta pliku bez zmian
  std::string k = sprzet::zapiszWymiane("a=1\r\nwymiana.maska = 2026-01-01\n", EL_MASKA, "2026-11-04");
  k = sprzet::zapiszWymiane(k, EL_MASKA, "2026-11-04");
  TRZYMA(k == "a=1\r\nwymiana.maska=2026-11-04\n");
}

int main() {
  testR16_01();
  testR16_02();
  testR16_03();
  testR16_05();
  testR16_06();
  testTrzyma();
  printf("testy_sprzet_infekcja_sk_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
