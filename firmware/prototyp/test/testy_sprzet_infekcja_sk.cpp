// SPDX-License-Identifier: GPL-3.0-or-later
// Testy „Moduły SPRZĘT i INFEKCJA, pozostałe ekrany kontekstowe S-K” (spec 5.8.3, 5.8.4, 5.9, 5.10, 7.2–7.4). Dane wyłącznie syntetyczne. Uruchomienie: tools/prototyp/testy.sh.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "infekcja.h"
#include "kontekst_sk.h"
#include "sprzet.h"
#include "stany.h"
#include "wspolne.h"
#include "wzorce.h"
#include <ctime>
#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "nawigacja.h"
#include "start.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE(a, b)                                                                                      \
  do {                                                                                                   \
    std::string a_ = (a), b_ = (b);                                                                      \
    if (a_ == b_) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s\n  jest:    '%s'\n  ma być:  '%s'\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } \
  } while (0)

static const std::string D0 = "2026-09-01";
static std::string d(int i) { return dodajDni(D0, i); }

// noc ważna z czasem 7 h, wyciekiem 9 (pomijalny: P95 10 przy domyślnym „Uwaga o masce od” 10 to już
// `Wyciek widoczny`, R-KONF-42), oddechami 14, ciśnieniem 10, bez przerw
static Noc noc(int i, int m01 = 420, float wp95 = 9) {
  Noc n;
  n.data = d(i);
  n.wazna = m01 >= 240;
  n.m01 = m01;
  n.ahi = 1.0f;
  n.wmed = 4;
  n.wp95 = wp95;
  n.cp95 = 10;
  n.m13 = 14;
  n.m17 = 0;
  n.m08 = 0;
  n.ustawienia = "AutoSet/5/12";
  return n;
}
static std::vector<Noc> historia(int ile, int m01 = 420, float wp95 = 9) {
  std::vector<Noc> h;
  for (int i = 0; i < ile; i++) h.push_back(noc(i, m01, wp95));
  return h;
}
static NormaMetryki nm(double med, double p10, double p90) {
  NormaMetryki x;
  x.n = 30;
  x.med = med;
  x.p10 = p10;
  x.p90 = p90;
  return x;
}
static Norma norma() {
  Norma n;
  n.m01 = nm(420, 360, 470);
  n.wp95 = nm(10, 6, 14);
  n.cp95 = nm(10, 9, 11);
  n.ep95 = nm(8, 7, 9);
  n.m13 = nm(14, 13, 15);
  n.m17 = nm(1, 0, 2);
  return n;
}
static bool jest(const std::vector<sk::Tresc>& v, const std::string& id, const std::string& war = "") {
  for (auto& t : v)
    if (t.id == id && (war.empty() || t.wariant == war)) return true;
  return false;
}
// kopia kandydata (wektor z sk::rano() jest tymczasowy)
static sk::Tresc kopia_;
static const sk::Tresc* znajdz(const std::vector<sk::Tresc>& v, const std::string& id) {
  for (auto& t : v)
    if (t.id == id) { kopia_ = t; return &kopia_; }
  return nullptr;
}
static sk::Wejscie wejscie(const std::vector<Noc>& h, const Norma& n, int dzien) {
  sk::Wejscie w;
  w.dzis = d(dzien);
  w.hist = &h;
  w.norma = &n;
  w.trybProbny = false;
  return w;
}

// ---------- teksty z zestawu (sprawdzone skryptem ze spec) ----------
static void testTeksty() {
  ROWNE(sk::tekst("S-K-03.tytul"), "Chłodna sypialnia");
  ROWNE(sk::tekst("S-K-10.powod", {{"próg", "24"}, {"min", "95"}}), "Wyciek ponad 24 l/min przez 95 min. AHI mniej pewne.");
  ROWNE(sk::tekst("S-K-31.krok"), "→ Wymień i przytrzymaj ◀.");
  ROWNE(sk::tekst("R-INF-10.linia"), infekcja::LINIA);
  ROWNE(sk::tekst("nie-ma-takiego"), "");
}

// ---------- wieczór ----------
static void testWieczor() {
  std::vector<Noc> h = historia(30);
  Norma n = norma();
  std::vector<WpisHumoru> dz;
  sk::Wejscie w = wejscie(h, n, 29);
  w.dziennik = &dz;
  // S-K-03: na progu 16 °C tak, 16,1 nie; tag nawilżania „wyłączone” wyklucza
  w.tempTeraz = 16;
  SPRAWDZ(jest(sk::wieczor(w), "S-K-03"));
  w.tempTeraz = 16.1f;
  SPRAWDZ(!jest(sk::wieczor(w), "S-K-03"));
  w.tempTeraz = 15;
  w.tagNawilzania = true;
  w.nawilzanie = false;
  SPRAWDZ(!jest(sk::wieczor(w), "S-K-03"));
  w.tagNawilzania = false;
  const sk::Tresc* t = znajdz(sk::wieczor(w), "S-K-03");
  SPRAWDZ(t && t->t.powod == "W sypialni 15 °C. W zimnej rurze para skrapla się w wodę.");
  // S-K-04: < 35% (35 nie)
  w.tempTeraz = 21;
  w.wilgTeraz = 35;
  SPRAWDZ(!jest(sk::wieczor(w), "S-K-04"));
  w.wilgTeraz = 34;
  SPRAWDZ(jest(sk::wieczor(w), "S-K-04"));
  // S-K-05: sypialnia ≥ 26 (bez prognozy wariant powodu z sypialnią) i pierwszeństwo W1 przed S-K-04
  w.tempTeraz = 26;
  std::vector<sk::Tresc> v = sk::wieczor(w);
  SPRAWDZ(!v.empty() && v[0].id == "S-K-05");
  SPRAWDZ(v[0].t.powod == "W sypialni 26 °C. W upały noce z maską bywają krótsze.");
  w.tempTeraz = 25.9f;
  SPRAWDZ(!jest(sk::wieczor(w), "S-K-05"));
  // limit „ten sam ekran wieczorny raz na 3 dni” (7.4) poza sezonem grzewczym (wrzesień)
  w.wilgTeraz = 30;
  dz.push_back({d(27), "SK04", "S-K-04#1", 'K', "", ""});
  SPRAWDZ(!jest(sk::wieczor(w), "S-K-04"));
  dz[0].data = d(26);
  SPRAWDZ(jest(sk::wieczor(w), "S-K-04"));
  dz.clear();
  // R-KON-29: w sezonie grzewczym raz na 7 dni, po 4 pokazaniach w sezonie brak (związku nie ustalono)
  std::vector<Noc> hz;
  for (int i = 0; i < 60; i++) { Noc x = noc(0); x.data = dodajDni("2026-10-01", i); hz.push_back(x); }
  sk::Wejscie wz = w;
  wz.hist = &hz;
  wz.dzis = "2026-11-20";
  wz.tempTeraz = 21;
  wz.wilgTeraz = 30;
  dz = {{"2026-11-15", "SK04", "S-K-04#1", 'D', "", ""}};
  SPRAWDZ(!jest(sk::wieczor(wz), "S-K-04"));
  dz[0].data = "2026-11-13";
  SPRAWDZ(jest(sk::wieczor(wz), "S-K-04"));
  for (const char* x : {"2026-10-05", "2026-10-12", "2026-10-19"}) dz.push_back({x, "SK04", "S-K-04", 'D', "", ""});
  SPRAWDZ(!jest(sk::wieczor(wz), "S-K-04"));
  dz.clear();
  // R-PIE-10 i R-PIE-12: prośba rano albo 4 wieczory z krokiem → bez ekranu wieczornego
  w.prosbaDzis = true;
  SPRAWDZ(sk::wieczor(w).empty());
  w.prosbaDzis = false;
  w.wieczorowZKrokiem7 = 4;
  SPRAWDZ(sk::wieczor(w).empty());
  w.wieczorowZKrokiem7 = 0;
  // tryb próbny (R-KON-05, R-KON-24): S-K-04 do logu, S-K-08 na ekranie
  w.trybProbny = true;
  t = znajdz(sk::wieczor(w), "S-K-04");
  SPRAWDZ(t && t->doLogu);
  w.trybProbny = false;
  // S-K-08: ostatnia noc < 30 min (także brak rekordu), przy działającym AirBridge
  h.back().m01 = 29;
  v = sk::wieczor(w);
  t = znajdz(v, "S-K-08");
  SPRAWDZ(t && t->kropki.size() == 7 && t->kropki.back() == 0 && t->kropki.front() == 1);
  w.trybProbny = true;
  t = znajdz(sk::wieczor(w), "S-K-08");
  SPRAWDZ(t && !t->doLogu);
  w.trybProbny = false;
  h.back().m01 = 30;
  SPRAWDZ(!jest(sk::wieczor(w), "S-K-08"));
  h.pop_back();  // noc bez rekordu (E-10)
  SPRAWDZ(jest(sk::wieczor(w), "S-K-08"));
  w.airbridge = false;
  SPRAWDZ(!jest(sk::wieczor(w), "S-K-08"));
  w.airbridge = true;
  h.push_back(noc(29));
  // limit 3 różnych S-K w dobie
  dz = {{w.dzis, "SK10", "x", 'D', "", ""}, {w.dzis, "SK13", "x", 'D', "", ""}, {w.dzis, "SK01", "x", 'D', "", ""}};
  SPRAWDZ(!jest(sk::wieczor(w), "S-K-04"));
  dz.clear();
}

static void testSK07iSK09() {
  std::vector<Noc> h = historia(40);
  Norma n = norma();
  sk::Wejscie w = wejscie(h, n, 39);
  // S-K-07 bez M-18 milczy
  SPRAWDZ(!jest(sk::wieczor(w), "S-K-07"));
  // rozrzut startu: 7 nocy ±90 min wokół 23:00 (660 min od 12:00)
  int st[7] = {570, 750, 570, 750, 570, 750, 660};
  for (int i = 0; i < 7; i++) w.startyM18[d(33 + i)] = st[i];
  const sk::Tresc* t = znajdz(sk::wieczor(w), "S-K-07");
  SPRAWDZ(t != nullptr);
  int st2[7] = {640, 680, 640, 680, 640, 680, 660};
  for (int i = 0; i < 7; i++) w.startyM18[d(33 + i)] = st2[i];
  SPRAWDZ(!jest(sk::wieczor(w), "S-K-07"));
  // S-K-09: piątek, weekendy krótsze o ≥ 30 min w 4 tygodniach
  std::vector<Noc> h2;
  std::string pt = "2026-10-30";  // piątek
  for (int i = 27; i >= 0; i--) {
    Noc x = noc(0);
    x.data = dodajDni(pt, -i);
    int dt = dzienTygodnia(x.data);
    x.m01 = dt == 6 || dt == 0 ? 360 : 420;
    h2.push_back(x);
  }
  sk::Wejscie w2 = wejscie(h2, n, 0);
  w2.dzis = pt;
  t = znajdz(sk::wieczor(w2), "S-K-09");
  SPRAWDZ(t && t->t.powod == "W weekendy masz maskę średnio o 60 min krócej.");
  w2.dzis = dodajDni(pt, -1);  // czwartek
  SPRAWDZ(!jest(sk::wieczor(w2), "S-K-09"));
  for (auto& x : h2) if (x.m01 == 360) x.m01 = 391;  // różnica 29 min
  w2.dzis = pt;
  SPRAWDZ(!jest(sk::wieczor(w2), "S-K-09"));
}

// ---------- rano ----------
static void testRano() {
  std::vector<Noc> h = historia(30);
  Norma n = norma();
  std::vector<WpisHumoru> dz;
  sk::Wejscie w = wejscie(h, n, 29);
  w.dziennik = &dz;
  std::vector<sk::Tresc> v0 = sk::rano(w);  // 30 nocy po 7 h: tylko kamień S-K-27 (pierwszy raz 70% w 30 dniach)
  SPRAWDZ(v0.size() == 1 && v0[0].id == "S-K-27");
  dz.push_back({d(1), "SK27", "x", 'K', "", ""});
  // S-K-12: mediana wycieku > 30 albo terapia 15 min…2 h
  h.back().wmed = 31;
  SPRAWDZ(jest(sk::rano(w), "S-K-12"));
  h.back().wmed = 30;
  SPRAWDZ(!jest(sk::rano(w), "S-K-12"));
  h.back().m01 = 119;
  h.back().wazna = false;
  SPRAWDZ(jest(sk::rano(w), "S-K-12"));
  h.back().m01 = 120;
  SPRAWDZ(!jest(sk::rano(w), "S-K-12"));
  h.back() = noc(29);
  // S-K-10: ton „problem” od P95 15 (model pasm wycieku, testSk10Opisowy); P95 24 → problem z powodem
  // bez minut, > 24 → powód z minutami ponad 24; „od kilku nocy” w 3 z 7 (R-KON-13)
  h.back().wp95 = 24;
  SPRAWDZ(znajdz(sk::rano(w), "S-K-10") && znajdz(sk::rano(w), "S-K-10")->problem &&
          znajdz(sk::rano(w), "S-K-10")->t.powod == "W części nocy wyciek 24 l/min. AHI mniej pewne.");
  h.back().wp95 = 25;
  h.back().m08 = 95;
  const sk::Tresc* t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->wariant.empty() && t->problem && t->t.powod == "Wyciek ponad 24 l/min przez 95 min. AHI mniej pewne.");
  h[27].wp95 = 28;
  h[28].wp95 = 31;
  t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->wariant == "kilka" && t->t.tytul == "Maska przecieka od 3 nocy");
  // E-23 wyklucza wariant jednonocny, nie „od kilku nocy”
  w.e23 = true;
  SPRAWDZ(jest(sk::rano(w), "S-K-10", "kilka"));
  h[27].wp95 = 9;
  h[28].wp95 = 9;
  SPRAWDZ(!jest(sk::rano(w), "S-K-10"));
  w.e23 = false;
  // R-KON-14: jednonocny pokazany 2 razy w 7 dniach → przerwa
  dz = {{d(27), "SK10", "S-K-10#1", 'D', "", ""}, {d(28), "SK10", "S-K-10#2", 'K', "", ""}};
  SPRAWDZ(!jest(sk::rano(w), "S-K-10"));
  dz[0].data = d(22);
  SPRAWDZ(jest(sk::rano(w), "S-K-10"));
  dz.clear();
  // okres infekcji wstrzymuje S-K-10, S-K-14, S-K-16, S-K-31 (R-INF-02)
  w.okresInfekcji = true;
  SPRAWDZ(!jest(sk::rano(w), "S-K-10"));
  w.okresInfekcji = false;
  // S-K-14: wilgotność < 35 i P95 > 24; E-21 i „poza domem” wykluczają; R5c: S-K-14 przed S-K-10
  std::vector<NocSypialni> syp = {NocSypialni(d(29), 22, 34)};
  w.sypialnia = &syp;
  std::vector<sk::Tresc> v = sk::rano(w);
  SPRAWDZ(jest(v, "S-K-14") && jest(v, "S-K-10"));
  SPRAWDZ(!v.empty() && v[0].id == "S-K-14");
  t = znajdz(v, "S-K-14");
  SPRAWDZ(t && t->t.powod == "W nocy wilgotność 34%, wyciek wyższy niż zwykle. Suche usta?");  // 25 − 10 ≥ Δ
  Norma n2 = n;
  n2.wp95 = nm(20, 15, 25);
  w.norma = &n2;
  t = znajdz(sk::rano(w), "S-K-14");
  SPRAWDZ(t && t->t.powod == "W nocy wilgotność 34%, wyciek 25 l/min. Suche usta?");  // R-KON-28
  w.norma = &n;
  w.e21 = true;
  SPRAWDZ(!jest(sk::rano(w), "S-K-14"));
  w.e21 = false;
  w.pozaDomem.insert(d(29));
  SPRAWDZ(!jest(sk::rano(w), "S-K-14"));
  w.pozaDomem.clear();
  syp[0].w = 35;
  SPRAWDZ(!jest(sk::rano(w), "S-K-14"));
  h.back() = noc(29);
  // S-K-16: M-17 ≥ 3; E-14 wyklucza; powód z normą
  h.back().m17 = 3;
  t = znajdz(sk::rano(w), "S-K-16");
  SPRAWDZ(t && t->t.tytul == "Maska zdejmowana 3 razy" && t->t.powod == "Zwykle 1. Coś mogło przeszkadzać w nocy.");
  w.e14 = true;
  SPRAWDZ(!jest(sk::rano(w), "S-K-16"));
  w.e14 = false;
  h.back().m17 = 2;
  SPRAWDZ(!jest(sk::rano(w), "S-K-16"));
  h.back() = noc(29);
  // S-K-15: sypialnia ≥ 26 i M-01 < 4 h; wariant powodu z normą (6 h 30 → krótsza o ≥ 1,5 h)
  syp[0] = NocSypialni(d(29), 27, 45);
  h.back().m01 = 230;
  h.back().wazna = false;
  t = znajdz(sk::rano(w), "S-K-15");
  SPRAWDZ(t && t->t.powod == "W sypialni 27°C, maska 3 h 10 krócej niż zwykle. W upał to częste.");
  Norma n3 = n;
  n3.m01 = nm(220, 200, 260);
  w.norma = &n3;
  t = znajdz(sk::rano(w), "S-K-15");
  SPRAWDZ(t && t->t.powod == "W sypialni 27°C, maska tylko 3 h 50. W upał to częste.");
  w.norma = &n;
  h.back().m01 = 240;
  SPRAWDZ(!jest(sk::rano(w), "S-K-15"));
  syp[0].t = 25.9f;
  h.back().m01 = 200;
  SPRAWDZ(!jest(sk::rano(w), "S-K-15"));
  h.back() = noc(29);
  w.sypialnia = nullptr;
}

static void testSK17iSK30() {
  std::vector<Noc> h = historia(30);
  Norma n = norma();
  sk::Wejscie w = wejscie(h, n, 29);
  // S-K-17: 2 kolejne noce z ≥ 2 sygnałami odchylenia (M-13 + 2, M-07 + 10); S-K-30 bez sygnału ciśnienia
  for (int i : {28, 29}) {
    h[(size_t)i].m13 = 17;
    h[(size_t)i].wp95 = 26;  // iloraz wycieku 1,6 > oddechów 1,5: wyciek pierwszy
  }
  std::vector<sk::Tresc> v = sk::rano(w);
  const sk::Tresc* t = znajdz(v, "S-K-17");
  SPRAWDZ(t && t->t.powod == "Wyciek wyższy niż zwykle, oddech szybszy o 3,0 na minutę.");
  SPRAWDZ(!jest(v, "S-K-30"));
  // (T-TEST-335): w okresie infekcji S-K-17 wstrzymany (R-INF-02), po okresie działa
  w.okresInfekcji = true;
  SPRAWDZ(!jest(sk::rano(w), "S-K-17"));
  w.okresInfekcji = false;
  SPRAWDZ(jest(sk::rano(w), "S-K-17"));
  h[28].wp95 = 10;  // jedna noc z jednym sygnałem
  SPRAWDZ(!jest(sk::rano(w), "S-K-17"));
  // S-K-30: M-13 ≥ mediana + 2 w 2 nocach i ciśnienie ≥ mediana + 2 w ostatniej → S-K-30 wyklucza S-K-17
  h[28].m13 = 16;
  h[29].m13 = 16;
  h[29].cp95 = 12;
  h[28].wp95 = 25;
  v = sk::rano(w);
  t = znajdz(v, "S-K-30");
  SPRAWDZ(t && t->t.powod == "Oddechy 16,0 / min, zwykle 14,0. Organizm mógł być obciążony." && t->pytanie == "Katar albo infekcja?");
  SPRAWDZ(!jest(v, "S-K-17"));
  SPRAWDZ(!v.empty() && v[0].id == "S-K-30");  // R5a na czele problemów porannych
  w.trybProbny = true;
  t = znajdz(sk::rano(w), "S-K-30");
  SPRAWDZ(t && !t->doLogu);  // R-KON-24
  w.trybProbny = false;
  h[28].m13 = 15.9f;  // na progu: 15,9 < 16
  SPRAWDZ(!jest(sk::rano(w), "S-K-30"));
  h[28].m13 = 16;
  h[29].cp95 = 11.9f;
  SPRAWDZ(!jest(sk::rano(w), "S-K-30"));
  // tryb ASV: sygnał M-06p (EPAP)
  h[29].ep95 = 10;
  w.tryb = reguly::Tryb::ASVAUTO;
  t = znajdz(sk::rano(w), "S-K-30");
  SPRAWDZ(t && t->stopkaPrawo == "EPAP też wyższe");
  w.tryb = reguly::Tryb::NIEZNANY;
  // rozruch globalny: bez S-K-30 i S-K-17 (wymagają normy)
  std::vector<Noc> krotka(h.begin() + 20, h.end());
  w.hist = &krotka;
  SPRAWDZ(!jest(sk::rano(w), "S-K-30"));
}

static void testKamienie() {
  std::vector<Noc> h;
  for (int i = 0; i < 30; i++) h.push_back(noc(i, i < 9 ? 100 : 300));  // 21 z 30 nocy ≥ 4 h = 70%
  Norma n = norma();
  std::vector<WpisHumoru> dz;
  sk::Wejscie w = wejscie(h, n, 29);
  w.dziennik = &dz;
  const sk::Tresc* t = znajdz(sk::rano(w), "S-K-27");
  SPRAWDZ(t && t->t.powod == "Maska 4 h lub dłużej na 21 z 30 nocy.");
  dz.push_back({d(10), "SK27", "x", 'K', "", ""});  // raz (R-KON-16)
  SPRAWDZ(!jest(sk::rano(w), "S-K-27"));
  dz.clear();
  h[9].m01 = 100;  // 20 z 30
  SPRAWDZ(!jest(sk::rano(w), "S-K-27"));
  // S-K-29: noc ≥ 4 h po ≥ 3 nocach bez terapii
  std::vector<Noc> h2 = historia(20);
  h2.erase(h2.begin() + 16, h2.begin() + 19);  // noce 16, 17, 18 bez rekordu
  sk::Wejscie w2 = wejscie(h2, n, 19);
  t = znajdz(sk::rano(w2), "S-K-29");
  SPRAWDZ(t && t->t.powod == "Po 3 nocach przerwy dziś 7 h 00 z maską.");
  h2.insert(h2.begin() + 16, noc(18, 30));  // noc 18 z terapią 30 min: przerwa 2 noce
  SPRAWDZ(!jest(sk::rano(w2), "S-K-29"));
}

static void testTydzien() {
  std::vector<Noc> h = historia(30);
  Norma n = norma();
  std::string nd = "2026-09-27";  // niedziela (d(26))
  sk::Wejscie w = wejscie(h, n, 26);
  SPRAWDZ(w.dzis == nd);
  sk::Tresc t;
  w.uspokojenie = true;
  SPRAWDZ(sk::tydzien(w, t));
  ROWNE(t.t.tytul, "7 z 7 nocy ponad 4 h");
  ROWNE(t.t.powod, "Noc z maską: średnio 7 h 00.");
  ROWNE(t.t.krok, "→ Nic do zrobienia. Tak trzymaj.");
  // pora (5.8.4, zmiana 08.11.2026): rozrzut startu > 60 min, ale 7 z 7 nocy ponad 4 h → bez kroku pory
  for (int i = 0; i < 7; i++) w.startyM18[dodajDni(nd, -i)] = (i % 2) ? 22 * 60 : 24 * 60 + 90;
  sk::tydzien(w, t);
  ROWNE(t.t.krok, "→ Nic do zrobienia. Tak trzymaj.");
  h[25].m01 = 200;  // jedna noc < 4 h: 6 z 7 → krok pory
  sk::tydzien(w, t);
  ROWNE(t.t.tytul, "6 z 7 nocy ponad 4 h");
  ROWNE(t.t.krok, "→ Stała pora snu, też w weekend.");
  h[25].m01 = 420;
  w.startyM18.clear();
  // słaby tydzień (2 z 7 ponad 4 h, reszta rekordów bez terapii) → krok czasu, nie „Tak trzymaj”
  {
    std::vector<Noc> hs = h;
    for (int i = 20; i <= 24; i++) hs[(size_t)i].m01 = 0;  // mediana nocy z terapią 7 h: bez tej reguły byłoby „Tak trzymaj”
    sk::Wejscie ws = w;
    ws.hist = &hs;
    sk::Tresc ts;
    SPRAWDZ(sk::tydzien(ws, ts));
    ROWNE(ts.t.tytul, "2 z 7 nocy ponad 4 h");
    ROWNE(ts.t.krok, "→ Każda godzina z maską się liczy.");
    for (int i = 20; i <= 26; i++) hs[(size_t)i].m01 = 0;  // 7 rekordów bez terapii
    SPRAWDZ(sk::tydzien(ws, ts));
    ROWNE(ts.t.powod, "W tym tygodniu bez nocy z maską.");
    ROWNE(ts.t.krok, "→ Każda godzina z maską się liczy.");
  }
  w.uspokojenie = false;
  sk::tydzien(w, t);
  ROWNE(t.t.krok, "→ Otwarte sprawy: strona 3 / 6.");  // numer strony kokpitu
  w.liniaZdrowiaNaEkranie = true;
  sk::tydzien(w, t);
  ROWNE(t.t.krok, "→ Sprawa z linii wyżej wciąż otwarta.");
  for (int i = 20; i <= 26; i++) h[(size_t)i].m01 = 320;  // mediana 5 h 20 → krok czasu
  sk::tydzien(w, t);
  ROWNE(t.t.krok, "→ Każda godzina z maską się liczy.");
  for (int i : {20, 22, 24, 26}) h[(size_t)i].wp95 = 15;  // > P90 normy w 4 z 7
  sk::tydzien(w, t);
  ROWNE(t.t.krok, "→ Sprawdź szczelność maski.");
  w.sennoscTak = true;
  sk::tydzien(w, t);
  ROWNE(t.t.krok, "→ Za kierownicą nie ryzykuj.");
  // < 5 rekordów w 7 nocach
  std::vector<Noc> h2(h.begin(), h.begin() + 24);
  w.hist = &h2;
  SPRAWDZ(!sk::tydzien(w, t));
  // S-K-21 wariant weekend w poniedziałek (R8), odkrycie poza rozruchem
  std::vector<Noc> h3;
  for (int i = 0; i < 60; i++) {
    Noc x = noc(i);
    int dt = dzienTygodnia(x.data);
    if (dt == 6 || dt == 0) x.m01 = 360;
    h3.push_back(x);
  }
  sk::Wejscie w3 = wejscie(h3, n, 0);
  std::string pon = d(59);
  while (dzienTygodnia(pon) != 1) pon = dodajDni(pon, -1);
  w3.dzis = pon;
  std::vector<sk::Tresc> v = sk::rano(w3);
  const sk::Tresc* s = znajdz(v, "S-K-21");
  SPRAWDZ(s && s->wariant == "weekend" && s->t.tytul == "Odkrycie: twoje noce i weekend");
  SPRAWDZ(s && s->t.powod == "W weekendy maska krócej o 60 min.");
  w3.dzis = dodajDni(pon, 1);  // wtorek: bez trendów tygodnia
  SPRAWDZ(!jest(sk::rano(w3), "S-K-21"));
  // S-K-19 wariant tymczasowy: mediana M-10 P95 z 7 nocy ≥ P90 normy i ≥ mediana + 0,1
  for (auto& x : h3) x.m10p = 0.12f;
  for (int i = 0; i < 60; i++) if (i % 5 == 0) h3[(size_t)i].m10p = 0.20f;
  std::string nd2 = dodajDni(pon, -1);
  for (auto& x : h3) if (x.data > dodajDni(nd2, -7) && x.data <= nd2) x.m10p = 0.24f;
  w3.dzis = pon;
  SPRAWDZ(jest(sk::rano(w3), "S-K-19"));
  for (auto& x : h3) if (x.data > dodajDni(nd2, -7) && x.data <= nd2) x.m10p = 0.21f;
  SPRAWDZ(!jest(sk::rano(w3), "S-K-19"));
  // S-K-20 tylko przy L-07 aktywnym („tylko raport”, więc bez ekranu)
  for (auto& x : h3) if (x.data > dodajDni(nd2, -7) && x.data <= nd2) x.cp95 = 13;
  SPRAWDZ(!jest(sk::rano(w3), "S-K-20"));
  w3.l07Aktywne = true;
  const sk::Tresc* s20 = znajdz(sk::rano(w3), "S-K-20");
  SPRAWDZ(s20 && s20->t.powod == "Urządzenie dobiera średnio o 3,0 cmH2O więcej niż zwykle.");
}

// ---------- SPRZĘT ----------
static void testSprzet() {
  ElementWymiany el[EL_N];
  // elementy to maska (6 mies.) i filtr (4 tyg.)
  el[EL_MASKA] = {"2026-03-01", 6, false};
  el[EL_FILTR] = {"2026-10-10", 4, true};
  sprzet::Elementy e = sprzet::zOdpowiedziami(el, {}, {});
  SPRAWDZ(sprzet::poTerminie(e.el[EL_MASKA], "2026-09-01"));
  SPRAWDZ(!sprzet::poTerminie(e.el[EL_MASKA], "2026-08-31"));
  SPRAWDZ(!sprzet::poTerminie(ElementWymiany{"", 6, false}, "2030-01-01"));  // bez daty nigdy (R-MAS-06)
  std::vector<sprzet::WierszSprzetu> ws = sprzet::wierszeSprzetu(e, "2026-10-31");
  SPRAWDZ(ws.size() == 2 && ws[0].nazwa == "Maska" && ws[0].wiek == "7 mies." && ws[0].okres == "6 mies." && ws[0].poTerminie);
  SPRAWDZ(ws[1].nazwa == "Filtr" && ws[1].wiek == "3 tyg." && ws[1].okres == "4 tyg." && !ws[1].poTerminie);
  // zapis wymiany (R-MAS-01): zastąpienie albo dopisanie wiersza
  ROWNE(sprzet::zapiszWymiane("# k\nwymiana.filtr=2026-01-01\npylki=brzoza\n", EL_FILTR, "2026-11-04"),
        "# k\nwymiana.filtr=2026-11-04\npylki=brzoza\n");
  ROWNE(sprzet::zapiszWymiane("pylki=brzoza", EL_MASKA, "2026-11-04"), "pylki=brzoza\nwymiana.maska=2026-11-04\n");
  // ◀ ▶ kursor (za końcem listy sąsiednia strona), ◀ 2 s wymienione, ↻ ekran dnia, ↻ 2 s nowy model (maska)
  ROWNE(sprzet::stopkaSprzetu(EL_MASKA), "◀ wstecz · 2 s wymienione   ▶ niżej   ↻ ekran dnia · 2 s nowy model");
  ROWNE(sprzet::stopkaSprzetu(EL_FILTR), "◀ wyżej · 2 s wymienione   ▶ ustawienia   ↻ ekran dnia");
  // S-K-31: maska po terminie, wyciek 7 nocy ≥ norma + 10, rośnie od 3 tygodni
  std::vector<Noc> h;
  std::string koniec = "2026-10-31";
  for (int i = 40; i >= 0; i--) {
    Noc x = noc(0);
    x.data = dodajDni(koniec, -i);
    x.wp95 = i >= 21 ? 10.0f : 10.0f + (float)(21 - i);  // ostatnie 21 nocy: 11 … 31 (nachylenie > 0 także w 28)
    h.push_back(x);
  }
  Norma n = norma();
  std::vector<WpisHumoru> dz;
  sprzet::WejscieSprzetu w;
  w.dzis = koniec;
  w.hist = &h;
  w.norma = &n;
  w.el = &e;
  w.dziennik = &dz;
  sprzet::WynikSK31 r = sprzet::ocenSK31(w);
  SPRAWDZ(r.warunek && r.element == EL_MASKA && r.miesiecy == 7 && r.tygodni == 4 && r.linia.size() == 28);
  w.okresInfekcji = true;
  SPRAWDZ(!sprzet::ocenSK31(w).warunek);
  w.okresInfekcji = false;
  // jeden element, maska (R-MAS-07 wygasa); tytuł `Maska się zużywa` dla każdej wymiany
  e.el[EL_MASKA].wymieniono = "2026-02-01";
  SPRAWDZ(sprzet::ocenSK31(w).element == EL_MASKA);
  sk::Wejscie wk = wejscie(h, n, 0);
  wk.dzis = koniec;
  wk.sprzet = &e;
  wk.dziennik = &dz;
  const sk::Tresc* t = znajdz(sk::rano(wk), "S-K-31");
  SPRAWDZ(t && t->t.tytul == "Maska się zużywa" && t->t.powod == "Ma 8 mies., wyciek rośnie od 4 tyg.");
  e.el[EL_MASKA].wymieniono = "2026-03-01";
  // wyciek stały → brak S-K-31 (R-MAS-02)
  std::vector<Noc> hs = historia(40, 420, 30);
  w.hist = &hs;
  w.dzis = hs.back().data;
  SPRAWDZ(!sprzet::ocenSK31(w).warunek);
  w.hist = &h;
  w.dzis = koniec;
  // limit raz na 7 dni i licznik pokazań; blokada L-09 do 4. pokazania (R-MAS-03, R-MAS-10)
  std::string sp = sprzet::sprawaElementu(EL_MASKA, e.el[EL_MASKA]);
  ROWNE(sp, "maska@2026-03-01");
  SPRAWDZ(sprzet::blokadaL09(w));
  dz = {{"2026-10-27", "SK31", sp + "#1", 'D', "", ""}};
  SPRAWDZ(sprzet::ocenSK31(w).limit);
  dz[0].data = "2026-10-24";
  SPRAWDZ(!sprzet::ocenSK31(w).limit);
  for (const char* x : {"2026-10-03", "2026-10-10", "2026-10-17"}) dz.push_back({x, "SK31", sp + "#2", 'K', "", ""});
  SPRAWDZ(sprzet::pokazaniaSK31(dz, sp) == 4);
  SPRAWDZ(!sprzet::blokadaL09(w));
  // zapis wymiany zeruje licznik (nowa sprawa) i blokuje L-09 na 14 nocy ważnych (R-MAS-08)
  e.el[EL_MASKA].wymieniono = "2026-10-25";
  SPRAWDZ(sprzet::pokazaniaSK31(dz, sprzet::sprawaElementu(EL_MASKA, e.el[EL_MASKA])) == 0);
  SPRAWDZ(sprzet::blokadaL09(w));
  e.el[EL_MASKA].wymieniono = "2026-10-16";  // 15 nocy ważnych po wymianie
  SPRAWDZ(!sprzet::blokadaL09(w));
  // S-K-32: po 7 nocach ważnych od wymiany, wyciek niższy o ≥ Δ (17 → 6), jednorazowo
  std::vector<Noc> h2;
  for (int i = 0; i < 21; i++) {
    Noc x = noc(i, 420, i < 14 ? 17.0f : 6.0f);
    h2.push_back(x);
  }
  e.el[EL_MASKA].wymieniono = d(13);
  w.hist = &h2;
  w.dzis = d(20);
  dz.clear();
  sprzet::WynikSK32 r32 = sprzet::ocenSK32(w);
  SPRAWDZ(r32.warunek && r32.przed == 17 && r32.po == 6 && r32.okresMies == 6);
  w.dzis = d(19);  // 6 nocy po wymianie
  SPRAWDZ(!sprzet::ocenSK32(w).warunek);
  w.dzis = d(20);
  dz.push_back({d(20), "SK32", r32.sprawa, 'K', "", ""});
  SPRAWDZ(sprzet::ocenSK32(w).warunek);  // ten sam dzień: ten sam ekran
  w.dzis = d(21);
  SPRAWDZ(!sprzet::ocenSK32(w).warunek);  // jednorazowe
  dz.clear();
  for (auto& x : h2) if (x.data > d(13)) x.wp95 = 9;  // 17 → 9: różnica 8 < Δ
  w.dzis = d(20);
  SPRAWDZ(!sprzet::ocenSK32(w).warunek);
  // R-MAS-09: mniej niż 5 nocy przed wymianą
  std::vector<Noc> h3(h2.begin() + 10, h2.end());
  w.hist = &h3;
  SPRAWDZ(!sprzet::ocenSK32(w).warunek);
  // pytanie `Wymieniona maska?` (R-MAS-12): 14 nocy po 26, potem 3 po 15
  std::vector<Noc> h4;
  for (int i = 0; i < 17; i++) h4.push_back(noc(i, 420, i < 14 ? 26.0f : 15.0f));
  e.el[EL_MASKA].wymieniono = "";
  w.hist = &h4;
  w.dzis = d(16);
  sprzet::WynikPytaniaPoduszki q = sprzet::ocenPytaniePoduszki(w, {}, {}, "");
  SPRAWDZ(q.jest && q.odNocy == d(14) && q.przed == 26 && q.po == 15);
  SPRAWDZ(!sprzet::ocenPytaniePoduszki(w, {d(0)}, {}, "").jest);  // pytanie 16 dni temu
  SPRAWDZ(!sprzet::ocenPytaniePoduszki(w, {}, {}, d(10)).jest);   // zmiana `MSK` w oknie
  for (int i = 14; i < 17; i++) h4[(size_t)i].wp95 = 20;  // spadek 6
  SPRAWDZ(!sprzet::ocenPytaniePoduszki(w, {}, {}, "").jest);
  for (int i = 14; i < 17; i++) h4[(size_t)i].wp95 = 15;
  // „tak” (R-MAS-13): data wymiany = data poranka pierwszej z 3 nocy spadku
  sprzet::Elementy e2 = sprzet::zOdpowiedziami(el, h4, {d(16)});
  ROWNE(e2.el[EL_MASKA].wymieniono, d(14));
  // nachylenie
  SPRAWDZ(sprzet::nachylenie({1, 2, 3}) > 0.99 && sprzet::nachylenie({3, 2, 1}) < 0);
  SPRAWDZ(std::isnan(sprzet::nachylenie({1})));
}

// ---------- INFEKCJA ----------
static void testInfekcja() {
  std::vector<Noc> h = historia(40);
  // wyzwalacz w nocach 30 i 31: M-13 17, ciśnienie 12
  for (int i : {30, 31}) h[(size_t)i].m13 = 17;
  h[31].cp95 = 12;
  for (int i = 32; i < 40; i++) h[(size_t)i].m13 = 17;  // dalej podwyższone
  Norma n = liczNorme(h, d(31), {});
  infekcja::WynikSK30 r = infekcja::ocenSK30(h, n, d(31), false);
  SPRAWDZ(r.warunek && r.pierwszaNoc == d(30));
  // R-INF-01: sam M-13 bez sygnału towarzyszącego → brak
  h[31].cp95 = 10;
  SPRAWDZ(!infekcja::ocenSK30(h, n, d(31), false).warunek);
  h[31].cp95 = 12;
  // okres od „tak” rano po nocy 31: od nocy 30, 14 nocy (30…43)
  std::vector<infekcja::Okres> o = infekcja::okresy(h, {d(31)}, {}, d(39), false);
  SPRAWDZ(o.size() == 1 && o[0].od == d(30) && o[0].koniec == d(43) && !o[0].zakonczony);
  SPRAWDZ(infekcja::okresDzis(o, d(39)) != nullptr && infekcja::okresDzis(o, d(29)) == nullptr);
  SPRAWDZ(infekcja::noceOkresow(o, d(39)).size() == 10);
  // R-INF-06: M-13 w normie przez 2 noce (36, 37) → koniec po nocy 37
  h[36].m13 = 14;
  h[37].m13 = 14.5f;
  o = infekcja::okresy(h, {d(31)}, {}, d(39), false);
  SPRAWDZ(o[0].koniec == d(37) && o[0].zakonczony && o[0].koniecPoprawa);
  SPRAWDZ(infekcja::okresDzis(o, d(36)) != nullptr && infekcja::okresDzis(o, d(37)) == nullptr);
  SPRAWDZ(!infekcja::nocWOkresie(o, d(38)));
  h[36].m13 = 17;
  h[37].m13 = 17;
  // R-INF-04: po 14 nocach koniec
  std::vector<Noc> hd = historia(50);
  for (int i = 30; i < 50; i++) hd[(size_t)i].m13 = 17;
  hd[31].cp95 = 12;
  o = infekcja::okresy(hd, {d(31)}, {}, d(49), false);
  SPRAWDZ(o[0].koniec == d(43) && o[0].zakonczony);
  SPRAWDZ(infekcja::okresDzis(o, d(44)) == nullptr);
  // bez „tak” nic (R-INF-08)
  SPRAWDZ(infekcja::okresy(h, {}, {}, d(39), false).empty());
  // R-INF-07: pytanie „gorzej” od 3. nocy okresu, potem co 3 dni przy M-13 ≥ mediana + 3
  o = infekcja::okresy(h, {d(31)}, {}, d(39), false);
  SPRAWDZ(!infekcja::pytanieGorzej(o[0], h, {}, d(31)));
  SPRAWDZ(infekcja::pytanieGorzej(o[0], h, {}, d(32)));
  SPRAWDZ(!infekcja::pytanieGorzej(o[0], h, {d(32)}, d(34)));
  SPRAWDZ(infekcja::pytanieGorzej(o[0], h, {d(32)}, d(35)));  // 17 ≥ 14 + 3
  h[35].m13 = 16.9f;
  SPRAWDZ(!infekcja::pytanieGorzej(o[0], h, {d(32)}, d(35)));
  // R-INF-10: linia w pierwszym dniu okresu (dzień odpowiedzi), potem co 3 dni od pokazania
  SPRAWDZ(infekcja::liniaDzis(o[0], d(31), ""));
  SPRAWDZ(!infekcja::liniaDzis(o[0], d(33), d(31)));
  SPRAWDZ(infekcja::liniaDzis(o[0], d(34), d(31)));
  // wpływ na normę: noce okresu wyłączone (R-INF-05, R-CZY-05)
  std::set<std::string> wyk = infekcja::noceOkresow(o, d(39));
  Norma bez = liczNorme(h, d(40), wyk), z = liczNorme(h, d(40), {});
  SPRAWDZ(bez.m13.p90 < z.m13.p90 && bez.m13.n == z.m13.n);
  // wpływ na L-03 i L-09: wstrzymanie w wzorce::Okolicznosci i blokada sprzętu
  wzorce::Okolicznosci ow;
  SPRAWDZ(!ow.okresInfekcji && !ow.l09Sprzet);
}

// ---------- rysowanie (bez awarii, negatyw tonu „problem”) ----------
struct PlotnoTest : Plotno {
  int czarne = 0;
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480 && c) czarne++;
  }
};
static void testRysowanie() {
  std::vector<Noc> h = historia(30);
  h.back().wp95 = 26;
  Norma n = norma();
  sk::Wejscie w = wejscie(h, n, 29);
  const sk::Tresc* t = nullptr;
  std::vector<sk::Tresc> v = sk::rano(w);
  t = znajdz(v, "S-K-10");
  SPRAWDZ(t != nullptr);
  if (t) {
    PlotnoTest p;
    sk::rysuj(p, Naglowek{"pt 30.09", "dane 05:44"}, *t, "", "");
    SPRAWDZ(p.czarne > 800 * 480 / 2);  // negatyw
  }
  ElementWymiany el[EL_N];
  sprzet::Elementy e = sprzet::zOdpowiedziami(el, {}, {});
  PlotnoTest p2;
  sprzet::rysujSprzet(p2, sprzet::wierszeSprzetu(e, "2026-11-04"), 0);
  SPRAWDZ(p2.czarne > 1000 && p2.czarne < 800 * 480 / 2);
}


// ---------- dyrygent z atrapami: „Sprzęt” (5.9.3), R-MAS-01, R-MAS-11, R-MAS-14, S-K-31 na ekranie dnia ----------
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t ep(int r, int mi, int dz, int h, int m, int s = 0) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = dz;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static bool wLogu(const Panel& p, const std::string& f) {
  for (auto& l : p.port.log)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static void drukujLog(const Panel& p) {
  if (getenv("SPRZET_LOG"))
    for (auto& l : p.port.log) printf("    %s\n", l.c_str());
}
// Wybudzenie przyciskiem 4.11.2026 o h:m na ekranie `e`: przycisk `bit` krótko albo ≥ 2 s
static void nacisnij(Panel& p, int h, int m, Ekran e, uint8_t bit, bool dlugo, int s = 0) {
  p.zegar.epoch = ep(2026, 11, 4, h, m, s);
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.start.stan.ekran = (uint8_t)e;
  p.start.stan.ekranOd = p.zegar.epoch;
  p.przyciski.skrypt = {{0, bit}, {dlugo ? 2500u : 300u, 0}};
  p.port.log.clear();
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static std::string konf(const Panel& p) {
  auto i = p.pliki.pliki.find("/konfiguracja.txt");
  return i == p.pliki.pliki.end() ? "" : i->second;
}
static void testDyrygentSprzet() {
  Panel p;
  p.pliki.pliki["/konfiguracja.txt"] = "wymiana.poduszka=2026-03-01\nwymiana.filtr=2026-10-01\n";
  // S8 ▶ → „Sprzęt” (strona 5 / 6), kursor od pierwszego wiersza
  p.nvs.ustawLiczbeU("cpap", "sp_kur", 1);
  nacisnij(p, 12, 0, Ekran::NORMA, BIT_PRAWY, false);
  drukujLog(p);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::SPRZET && p.nvs.liczbaU("cpap", "sp_kur", 9) == 0);
  // dwa elementy. ▶ → kursor na filtrze (NVS sp_kur); ◀ 2 s → wymiana filtra dziś (R-MAS-01), reszta pliku bez zmian
  nacisnij(p, 12, 1, Ekran::SPRZET, BIT_PRAWY, false);
  SPRAWDZ(p.nvs.liczbaU("cpap", "sp_kur", 9) == 1 && p.start.stan.ekran == (uint8_t)Ekran::SPRZET);
  nacisnij(p, 12, 3, Ekran::SPRZET, BIT_LEWY, true);
  drukujLog(p);
  SPRAWDZ(konf(p).find("wymiana.filtr=2026-11-04") != std::string::npos);
  SPRAWDZ(konf(p).find("wymiana.poduszka=2026-03-01") != std::string::npos);
  SPRAWDZ(wLogu(p, "sprzęt: wymiana filtr 2026-11-04 (R-MAS-01)"));
  // ↻ 2 s przy filtrze → nic (R-MAS-11 tylko maska)
  nacisnij(p, 12, 4, Ekran::SPRZET, BIT_SRODEK, true);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::SPRZET && p.nvs.tekst("cpap", "sp_model", "") == "");
  // kursor na masce (◀), ↻ 2 s → `Nowy model maski zapisany.`, data maski dziś (stary wiersz poduszki zostaje, ale
  // jest wcześniejszy), sp_model
  nacisnij(p, 12, 5, Ekran::SPRZET, BIT_LEWY, false);
  SPRAWDZ(p.nvs.liczbaU("cpap", "sp_kur", 9) == 0);
  nacisnij(p, 12, 10, Ekran::SPRZET, BIT_SRODEK, true);
  drukujLog(p);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::SPRZET_ZAPISANO);
  SPRAWDZ(konf(p).find("wymiana.maska=2026-11-04") != std::string::npos);
  SPRAWDZ(p.nvs.tekst("cpap", "sp_model", "") == "2026-11-04");
  SPRAWDZ(wLogu(p, "nowy model maski 2026-11-04"));
  // ◀ 2 s po 10 s → cofnięcie: data maski sprzed przytrzymania (3.03 ze starego `wymiana.poduszka`), bez
  // sp_model, powrót do „Sprzęt”
  nacisnij(p, 12, 10, Ekran::SPRZET_ZAPISANO, BIT_LEWY, true, 10);
  drukujLog(p);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::SPRZET);
  SPRAWDZ(konf(p).find("wymiana.poduszka=2026-03-01") != std::string::npos);
  SPRAWDZ(konf(p).find("wymiana.maska=2026-03-01") != std::string::npos);
  SPRAWDZ(konf(p).find("wymiana.filtr=2026-11-04") != std::string::npos);
  SPRAWDZ(p.nvs.tekst("cpap", "sp_model", "-") == "-");
  // drugi raz, potem ▶ (inny przycisk): zapis zostaje, powrót do „Sprzęt”; ↻ = ekran dnia
  nacisnij(p, 12, 20, Ekran::SPRZET, BIT_SRODEK, true);
  nacisnij(p, 12, 20, Ekran::SPRZET_ZAPISANO, BIT_PRAWY, false, 5);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::SPRZET && p.nvs.tekst("cpap", "sp_model", "") == "2026-11-04");
  // upływ 30 s poza nocą → „Sprzęt” (R-MAS-14), nie ekran dnia
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::SPRZET_ZAPISANO, true, 31, 12 * 60) == Ekran::SPRZET);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::SPRZET_ZAPISANO, true, 20, 12 * 60) == Ekran::SPRZET_ZAPISANO);
  SPRAWDZ(sekundDoPowrotu(Ekran::SPRZET_ZAPISANO, 10, 12 * 60) == 20);
  SPRAWDZ(sekundDoPowrotu(Ekran::SPRZET_ZAPISANO, 10, 2 * 60) == -1);  // 02:10: do 05:00 (R-PRZ-18)
  // ◀ krótko w „Sprzęt” na pierwszym wierszu → S8 (poprzednia strona); ▶ na ostatnim → U-01
  nacisnij(p, 12, 30, Ekran::SPRZET, BIT_LEWY, false);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::NORMA);
  p.nvs.ustawLiczbeU("cpap", "sp_kur", 1);
  nacisnij(p, 12, 31, Ekran::SPRZET, BIT_PRAWY, false);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::USTAWIENIA);
  // ◀ z U-01 → „Sprzęt” z kursorem na ostatnim wierszu
  nacisnij(p, 12, 32, Ekran::USTAWIENIA, BIT_LEWY, false);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::SPRZET && p.nvs.liczbaU("cpap", "sp_kur", 9) == 1);
  // przywrocWymiane: data wraca, "" usuwa wiersz
  ROWNE(sprzet::przywrocWymiane("a=1\nwymiana.maska=2026-11-04\n", EL_MASKA, ""), "a=1\n");
  ROWNE(sprzet::przywrocWymiane("wymiana.maska=2026-11-04\n", EL_MASKA, "2026-01-01"), "wymiana.maska=2026-01-01\n");
}

// S-K-31 jako ekran dnia z historii w pamięci panelu, ◀ 2 s na nim zapisuje wymianę maski (R-MAS-01)
static const char* POLA_V3 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb;m18";
static std::string plikNocy(const std::vector<Noc>& h) {
  std::string t = std::string("#cpap-eink noce 3\n") + POLA_V3 + "\n";
  for (auto& n : h) t += wierszNocyV3(n) + "\n";
  return t + "#koniec " + std::to_string(h.size()) + "\n";
}
static void testDyrygentSK31() {
  std::vector<Noc> h;
  for (int i = 80; i >= 0; i--) {
    Noc x = noc(0);
    x.data = dodajDni("2026-11-04", -i);
    x.zrodlo = 'A';
    x.wp95 = i >= 21 ? 9.0f + (float)(i % 3) : 10.0f + 0.8f * (float)(21 - i);  // poniżej progu E-23 (27, R5b przed R5c)
    h.push_back(x);
  }
  Panel p;
  p.pliki.pliki["/dane/noce/2026.csv"] = plikNocy(h);
  p.pliki.pliki["/konfiguracja.txt"] = "wymiana.poduszka=2026-03-01\n";
  p.zegar.epoch = ep(2026, 11, 4, 7, 30);
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  drukujLog(p);
  SPRAWDZ(wLogu(p, "ekran S-K-31"));
  SPRAWDZ(p.nvs.liczbaU("cpap", "b16_sk31", 0) == EL_MASKA + 1);
  // ◀ 2 s na S-K-31 → data wymiany maski dziś
  nacisnij(p, 7, 31, Ekran::DZIEN, BIT_LEWY, true);
  drukujLog(p);
  SPRAWDZ(konf(p).find("wymiana.maska=2026-11-04") != std::string::npos);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::SPRZET_ZAPISANO);  // ekran z cofnięciem, nie cichy zapis
  // po wymianie S-K-31 znika, L-09 wstrzymane jeszcze 14 nocy (R-MAS-08); ↻ z ekranu potwierdzenia do ekranu dnia
  nacisnij(p, 7, 32, Ekran::SPRZET_ZAPISANO, BIT_SRODEK, false);
  SPRAWDZ(!wLogu(p, "ekran S-K-31") && p.nvs.liczbaU("cpap", "b16_sk31", 9) == 0);
}

// R-KONF-42 w dyrygencie: wariant opisowy S-K-10 jako ekran danych rano (R8 przed S-K-13), wartość z
// `/konfiguracja.txt` (`uwaga_o_masce`; model pasm wycieku): bez wiersza (10) P95 12 → opisowy;
// `uwaga_o_masce=13` przy P95 12 → bez S-K-10; P95 18 → S-K-10 w tonie „problem” (R5c), nie opisowy;
// `uwaga_o_masce=20` (spoza 10–15) odrzucone → 10, P95 12 → opisowy
static void testDyrygentSK10Opisowy() {
  for (int war = 0; war < 4; war++) {
    std::vector<Noc> h;
    for (int i = 80; i >= 0; i--) {
      Noc x = noc(0);
      x.data = dodajDni("2026-11-04", -i);
      x.zrodlo = 'A';
      x.wp95 = i == 0 ? (war == 2 ? 18.0f : 12.0f) : 8.0f + 0.5f * (float)(i % 3);
      h.push_back(x);
    }
    Panel p;
    p.pliki.pliki["/dane/noce/2026.csv"] = plikNocy(h);
    p.pliki.pliki["/konfiguracja.txt"] = war == 1 ? "uwaga_o_masce=13\n" : war == 3 ? "uwaga_o_masce=20\n" : "";
    p.zegar.epoch = ep(2026, 11, 4, 7, 30);
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    drukujLog(p);
    if (war == 0 || war == 3) SPRAWDZ(wLogu(p, "ekran S-K-10 (R8): ekran danych"));
    else if (war == 1) SPRAWDZ(!wLogu(p, "ekran S-K-10"));
    else SPRAWDZ(wLogu(p, "ekran S-K-10") && !wLogu(p, "ekran S-K-10 (R8)"));
  }
}

// S-K-30 jako pytanie dnia, ▶ = „tak” otwiera okres infekcji: noce okresu poza normą, log bramki 7.3 (R-INF-02)
static void testDyrygentSK30() {
  std::vector<Noc> h;
  for (int i = 40; i >= 0; i--) {
    Noc x = noc(0);
    x.data = dodajDni("2026-11-04", -i);
    x.zrodlo = 'A';
    x.m13 = 13.5f + (float)(i % 3) * 0.5f;
    if (i <= 1) {
      x.m13 = 17;
      x.cp95 = 12;
    }
    h.push_back(x);
  }
  Panel p;
  p.pliki.pliki["/dane/noce/2026.csv"] = plikNocy(h);
  p.zegar.epoch = ep(2026, 11, 4, 7, 30);
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  drukujLog(p);
  SPRAWDZ(wLogu(p, "ekran dnia: pytanie infekcja"));
  nacisnij(p, 7, 31, Ekran::DZIEN, BIT_PRAWY, false);
  drukujLog(p);
  SPRAWDZ(p.pliki.pliki["/dane/odpowiedzi.csv"].find("2026-11-04;infekcja;tak") != std::string::npos);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::ZAPISANO);
  nacisnij(p, 7, 33, Ekran::DZIEN, BIT_PRAWY, false);
  drukujLog(p);
  SPRAWDZ(wLogu(p, "okres infekcji od 2026-11-03"));
  SPRAWDZ(!wLogu(p, "ekran dnia: pytanie infekcja"));
}

// karencja po okresie z limitem, koniec miesiąca, blokada L-09
static void testKarencjaKoniecMiesiacaBlokadaL09() {
  infekcja::Okres o;
  o.od = "2026-10-01";
  o.koniec = "2026-10-14";
  o.zakonczony = true;
  std::vector<infekcja::Okres> v = {o};
  SPRAWDZ(infekcja::karencja(v, "2026-10-15") && infekcja::karencja(v, "2026-10-28"));
  SPRAWDZ(!infekcja::karencja(v, "2026-10-29") && !infekcja::karencja(v, "2026-10-14"));
  v[0].koniecPoprawa = true;  // koniec poprawą (R-INF-06): bez karencji
  SPRAWDZ(!infekcja::karencja(v, "2026-10-15"));
  SPRAWDZ(sprzet::pelneMiesiace("2026-08-31", "2027-02-28") == 6 && sprzet::pelneMiesiace("2026-08-31", "2027-02-27") == 5);
  ROWNE(wiekElementu("2026-08-31", "2027-02-28"), "6 mies.");
  ROWNE(wiekElementu("2026-08-31", "2026-09-29"), "4 tyg.");
}

// R-KONF-42: wariant opisowy S-K-10 `Wyciek widoczny` od ustawienia „Uwaga o masce
// od” do 24 L/min, ton „informacja”, R8, ekran danych; problem powyżej 24; nic poniżej progu i bez danych.
static void testSk10Opisowy() {
  std::vector<Noc> h = historia(30);
  Norma n = norma();
  std::vector<WpisHumoru> dz;
  sk::Wejscie w = wejscie(h, n, 29);
  w.dziennik = &dz;
  dz.push_back({d(1), "SK27", "x", 'K', "", ""});  // kamień S-K-27 już był
  // Model pasm wycieku P95: < 10 pomijalny, 10–15 istotny („Wyciek widoczny”, noc pewna),
  // od 15 duży (S-K-10 w tonie „problem”, noc niepewna), > 24 powód z minutami ponad 24
  SPRAWDZ(UWAGA_O_MASCE_MIN == 10 && UWAGA_O_MASCE_MAX == 15 && UWAGA_O_MASCE_DOMYSLNIE == 10);
  SPRAWDZ(std::fabs(sk::SK10_PROBLEM_P95 - NIEPEWNA_P95) < 1e-6 && std::fabs(sk::SK10_PROBLEM_P95 - 15) < 1e-6);
  // 9,9 → brak S-K-10
  h.back().wp95 = 9.9f;
  SPRAWDZ(!jest(sk::rano(w), "S-K-10"));
  // 10 i 14,9 → wariant opisowy, R8, bez tonu „problem”, bez karty, typ SK10W, noc pewna
  h.back().wp95 = 10;
  const sk::Tresc* t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->wariant == "opisowy" && !t->problem && !t->karta && t->grupa == "R8" && t->typ == "SK10W");
  h.back().wp95 = 14.9f;
  t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->wariant == "opisowy" && !nocNiepewna(h.back()));
  h.back().wp95 = 12;
  t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->wariant == "opisowy");
  if (t) {
    ROWNE(t->t.etykieta, "Ostatnia noc");
    ROWNE(t->t.tytul, "Wyciek widoczny");
    ROWNE(t->t.powod, "W części nocy wyciek 12 l/min, bez dużego wycieku.");
    ROWNE(t->t.krok, "→ Obserwuj kolejne noce.");
    ROWNE(t->podpisPrawo, "linia przerywana = 10 l/min");
    SPRAWDZ(t->wiz == sk::Wiz::LINIA && std::fabs(t->prog - 10) < 1e-6 && t->linia.size() == 14);
  }
  // 15, 18 i 24 → ton „problem” `Maska przeciekała`, powód bez minut, linia przerywana 15; noc niepewna
  for (float x : {15.0f, 18.0f, 24.0f}) {
    h.back().wp95 = x;
    t = znajdz(sk::rano(w), "S-K-10");
    SPRAWDZ(t && t->wariant.empty() && t->problem && t->grupa == "R5c" && t->typ == "SK10" && nocNiepewna(h.back()));
    if (t) {
      ROWNE(t->t.tytul, "Maska przeciekała");
      ROWNE(t->t.powod, "W części nocy wyciek " + calkowita(x) + " l/min. AHI mniej pewne.");
      ROWNE(t->podpisPrawo, "linia przerywana = 15 l/min");
    }
  }
  SPRAWDZ(zawin(F_P28B, sk::tekst("S-K-10.powod_p95", {{"x", "24"}}), 744).size() <= 2);
  SPRAWDZ(zawin(F_P28B, sk::tekst("S-K-10.kilka.powod_od", {{"próg", "15"}, {"n", "7"}}), 744).size() <= 2);
  // 25 → powód z minutami ponad 24 jak dotąd, linia przerywana 24
  h.back().wp95 = 25;
  h.back().m08 = 40;
  t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->problem && t->t.powod == "Wyciek ponad 24 l/min przez 40 min. AHI mniej pewne." &&
          t->podpisPrawo == "linia przerywana = 24 l/min");
  h.back().m08 = 0;
  // ustawienie 12: P95 11 → nic, 12 → opisowy (linia 12); ustawienie spoza 10–15 przycięte do zakresu
  w.uwagaOMasce = 12;
  h.back().wp95 = 11;
  SPRAWDZ(!jest(sk::rano(w), "S-K-10"));
  h.back().wp95 = 12;
  t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->wariant == "opisowy" && t->podpisPrawo == "linia przerywana = 12 l/min");
  w.uwagaOMasce = 10;
  // R4: ostatnia noc z warunkiem E-20 (M-08 ≥ 30 min albo ≥ 10% czasu) → bez wariantu
  // opisowego, bo rano jest linia E-20 o tym samym wycieku
  h.back().wp95 = 12;
  h.back().m08 = 30;
  SPRAWDZ(warunekE20(h.back()) && !jest(sk::rano(w), "S-K-10"));
  h.back().m08 = 42;  // 10% z 420 min
  SPRAWDZ(!jest(sk::rano(w), "S-K-10"));
  h.back().m08 = 29;
  SPRAWDZ(!warunekE20(h.back()) && jest(sk::rano(w), "S-K-10", "opisowy"));
  h.back().m08 = NAN;  // brak M-08: wariant opisowy zostaje
  SPRAWDZ(jest(sk::rano(w), "S-K-10", "opisowy"));
  h.back().m08 = 0;
  // brak danych: noc bez P95 albo bez rekordu → brak S-K-10 (nie „szczelna”, ale też nie „wyciek”)
  h.back().wp95 = NAN;
  SPRAWDZ(!jest(sk::rano(w), "S-K-10"));
  sk::Wejscie wb = wejscie(h, n, 31);
  wb.dziennik = &dz;
  SPRAWDZ(!jest(sk::rano(wb), "S-K-10"));
  // P95 12 w 3 z 7 nocy → „Wyciek widoczny od 3 nocy”
  for (size_t i = 0; i + 3 < h.size(); i++) h[i].wp95 = 8;  // reszta nocy poniżej 10
  h.back().wp95 = 12;
  h[28].wp95 = 12;
  h[27].wp95 = 12;
  t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->wariant == "opisowy_kilka" && !t->problem);
  if (t) {
    ROWNE(t->t.etykieta, "Ostatnie noce");
    ROWNE(t->t.tytul, "Wyciek widoczny od 3 nocy");
    ROWNE(t->t.powod, "Wyciek od 10 l/min w 3 z 7 nocy.");
    ROWNE(t->podpisPrawo, "linia przerywana = 10 l/min");
  }
  // P95 16, 18, 17 → „od kilku nocy” w tonie „problem” w paśmie 15–24: `Wyciek powyżej 15 l/min w 3 z 7 nocy.` (tekst:
  // „powyżej” przy liczeniu od 15,0 włącznie to przyjęta nieścisłość)
  h[27].wp95 = 16;
  h[28].wp95 = 18;
  h.back().wp95 = 17;
  t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->wariant == "kilka" && t->problem);
  if (t) {
    ROWNE(t->t.tytul, "Maska przecieka od 3 nocy");
    ROWNE(t->t.powod, "Wyciek powyżej 15 l/min w 3 z 7 nocy.");
    ROWNE(t->sprawa, "S-K-10/15/kilka");
    ROWNE(t->podpisPrawo, "linia przerywana = 15 l/min");
  }
  // … i nie wyklucza E-23 (pasmo 15–24): sk10Kilka() liczy tylko noce > 24
  SPRAWDZ(!sk::sk10Kilka(&h, w.dzis, false));
  // P95 16, 18, 29 → ostatnia > 24; przy E-23 bez S-K-10 (E-23 wyklucza jednonocny i „od kilku nocy” 15–24)
  h.back().wp95 = 29;
  w.e23 = true;
  SPRAWDZ(!jest(sk::rano(w), "S-K-10"));
  w.e23 = false;
  // 3 z 7 nocy > 24: „problem” od kilku nocy jak dotąd (bez limitu R-KON-14, wyklucza E-23)
  h[27].wp95 = 26;
  h[28].wp95 = 25;
  t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->wariant == "kilka" && t->t.powod == "Wyciek ponad 24 l/min w 3 z 7 nocy." && sk::sk10Kilka(&h, w.dzis, false));
  w.e23 = true;
  SPRAWDZ(jest(sk::rano(w), "S-K-10", "kilka"));
  w.e23 = false;
  // opisowy nie liczy się do limitu R-KON-14 wariantu „problem”: dwa pokazania SK10W w 7 dniach nie blokują problemu
  h[27].wp95 = 9;
  h[28].wp95 = 9;
  h.back().wp95 = 25;
  dz = {{d(26), "SK10W", "S-K-10/opisowy", 'D', "", ""}, {d(27), "SK10W", "S-K-10/opisowy", 'D', "", ""}};
  SPRAWDZ(jest(sk::rano(w), "S-K-10", ""));
  t = znajdz(sk::rano(w), "S-K-10");
  SPRAWDZ(t && t->problem);
  // okres infekcji: wstrzymany jak każdy ekran wycieku (R-INF-02)
  h.back().wp95 = 18;
  w.okresInfekcji = true;
  SPRAWDZ(!jest(sk::rano(w), "S-K-10"));
  w.okresInfekcji = false;
  // limit R-KON-14 przy stałym wycieku P95 18 przez 10 nocy: S-K-10 „problem” tylko w dniach 1, 2, 8 i 9 (dni 3+ to
  // wariant „od kilku nocy” w paśmie 15–24, z tym samym limitem); dziennik jak u dyrygenta: wpis w dniu pokazania
  {
    std::vector<Noc> hs = historia(40);
    std::vector<WpisHumoru> ds = {{d(1), "SK27", "x", 'K', "", ""}};
    std::string pokazane;
    for (int dzien = 30; dzien < 40; dzien++) {
      hs[dzien].wp95 = 18;
      sk::Wejscie ws = wejscie(hs, n, dzien);
      ws.dziennik = &ds;
      const sk::Tresc* x = znajdz(sk::rano(ws), "S-K-10");
      if (x && x->problem) {
        pokazane += std::to_string(dzien - 29) + (x->wariant == "kilka" ? "k " : " ");
        ds.push_back({d(dzien), "SK10", x->sprawa, 'D', "", ""});
      }
    }
    ROWNE(pokazane, "1 2 8k 9k ");
  }
  // P95 18, 18, 26 bez E-20 i E-23 → dni 1–2 S-K-10 z pasma 15–24 (klucz
  // `S-K-10/15`), dnia 3 noc > 24 wariantem jednonocnym z minutami, mimo limitu R-KON-14 pasma 15–24 i 3 z 7 nocy ≥ 15;
  // 18, 18, 18 → dnia 3 limit jak dotąd; 26, 26, 26 → „od kilku nocy” > 24 jak przed 55e24b2
  for (int war = 0; war < 3; war++) {
    std::vector<Noc> hs = historia(40);
    std::vector<WpisHumoru> ds = {{d(1), "SK27", "x", 'K', "", ""}};
    std::string pokazane;
    const float P[3][3] = {{18, 18, 26}, {18, 18, 18}, {26, 26, 26}};
    for (int dzien = 30; dzien < 33; dzien++) {
      hs[dzien].wp95 = P[war][dzien - 30];
      hs[dzien].m08 = hs[dzien].wp95 > 24 ? 20 : 0;  // 20 min z 420: bez warunku E-20 (30 min albo 10%)
      SPRAWDZ(!warunekE20(hs[dzien]));
      sk::Wejscie ws = wejscie(hs, n, dzien);
      ws.dziennik = &ds;
      const sk::Tresc* x = znajdz(sk::rano(ws), "S-K-10");
      if (x && x->problem) {
        pokazane += x->sprawa + " ";
        ds.push_back({d(dzien), "SK10", x->sprawa, 'D', "", ""});
        if (dzien == 32 && war == 0) ROWNE(x->t.powod, "Wyciek ponad 24 l/min przez 20 min. AHI mniej pewne.");
        if (dzien == 32 && war == 2) ROWNE(x->t.powod, "Wyciek ponad 24 l/min w 3 z 7 nocy.");
        if (dzien < 32 && war < 2) ROWNE(x->t.powod, "W części nocy wyciek 18 l/min. AHI mniej pewne.");
      } else {
        pokazane += "- ";
      }
    }
    if (war == 0) ROWNE(pokazane, "S-K-10/15 S-K-10/15 S-K-10 ");
    if (war == 1) ROWNE(pokazane, "S-K-10/15 S-K-10/15 - ");
    if (war == 2) ROWNE(pokazane, "S-K-10 S-K-10 S-K-10/kilka ");
  }
  // noc > 24 przy 3 z 7 nocy ≥ 15 i bez limitu: powód z minutami (nie tekst pasma 15), limit nocy > 24 liczy tylko `S-K-10`
  {
    std::vector<Noc> hs = historia(30);
    hs[27].wp95 = 18;
    hs[28].wp95 = 18;
    hs[29].wp95 = 26;
    hs[29].m08 = 20;
    sk::Wejscie ws = wejscie(hs, n, 29);
    std::vector<WpisHumoru> ds = {{d(1), "SK27", "x", 'K', "", ""}};
    ws.dziennik = &ds;
    const sk::Tresc* x = znajdz(sk::rano(ws), "S-K-10");
    SPRAWDZ(x && x->wariant.empty() && x->sprawa == "S-K-10" && x->podpisPrawo == "linia przerywana = 24 l/min");
    if (x) ROWNE(x->t.powod, "Wyciek ponad 24 l/min przez 20 min. AHI mniej pewne.");
    ds.push_back({d(26), "SK10", "S-K-10", 'D', "", ""});
    ds.push_back({d(27), "SK10", "S-K-10", 'D', "", ""});
    SPRAWDZ(!jest(sk::rano(ws), "S-K-10"));  // dwa pokazania nocy > 24 w 7 dniach: limit R-KON-14
  }
  // {x} wariantu opisowego w dół (14,9 → „14”, nie „15 l/min, bez dużego wycieku”)
  {
    std::vector<Noc> hs = historia(30);
    hs[29].wp95 = 14.9f;
    sk::Wejscie ws = wejscie(hs, n, 29);
    std::vector<WpisHumoru> ds = {{d(1), "SK27", "x", 'K', "", ""}};
    ws.dziennik = &ds;
    const sk::Tresc* x = znajdz(sk::rano(ws), "S-K-10");
    SPRAWDZ(x && x->wariant == "opisowy");
    if (x) ROWNE(x->t.powod, "W części nocy wyciek 14 l/min, bez dużego wycieku.");
    // ustawienie 15 = bez tej informacji: P95 14,9 w 1 i w 3 z 7 nocy → brak S-K-10
    ws.uwagaOMasce = 15;
    SPRAWDZ(!jest(sk::rano(ws), "S-K-10"));
    hs[27].wp95 = 14.9f;
    hs[28].wp95 = 14.9f;
    SPRAWDZ(!jest(sk::rano(ws), "S-K-10"));
  }
}

int main() {
  testTeksty();
  testSk10Opisowy();
  testWieczor();
  testSK07iSK09();
  testRano();
  testSK17iSK30();
  testKamienie();
  testTydzien();
  testSprzet();
  testInfekcja();
  testRysowanie();
  testDyrygentSprzet();
  testDyrygentSK31();
  testDyrygentSK10Opisowy();
  testDyrygentSK30();
  testKarencjaKoniecMiesiacaBlokadaL09();
  printf("testy_sprzet_infekcja_sk: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
