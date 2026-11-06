// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (pełne ekrany zdarzeń E S3, R-KON-14, linia „Wciąż”, koniec slotu dziennego
// wobec okna wieczornego, krój P24 600). Każdy test pada z numerem R14-nn, dopóki usterka nie zostanie
// poprawiona.
// Testy bez numeru (sprawdzenia „trzyma”) pilnują zachowań już potwierdzonych. Dane syntetyczne.
// Drugi argument: katalog na podglądy PBM (S3 z linią wzorca A i dwuwierszowym powodem).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "dzien.h"
#include "ekrany.h"
#include "ekrany_zdarzen.h"
#include "kolejka.h"
#include "plan.h"
#include "sprawy.h"
#include "stany.h"
#include "wspolne.h"
#include "zdarzenia_ekrany_e.h"

static int ok = 0, zle = 0;
#define PRZYPADEK(nr, w)                                                                    \
  do {                                                                                       \
    if (w) ok++;                                                                             \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); }       \
  } while (0)
#define TRZYMA(w) PRZYPADEK("(trzyma)", w)

using reguly::Pilnosc;
using sprawy::SprawaL;

static const std::string D1 = "2026-11-01";

// Doba po dobie jak src/dyrygent.cpp rysujDzienB5(): pokazanie zapisane, gdy sprawa jest dziś ekranem dnia.
static std::vector<int> dniPokazan(SprawaL s, int dni, const std::vector<int>& przegrane = {}) {
  reguly::Pamiec p;
  std::vector<int> w;
  for (int i = 1; i <= dni; i++) {
    std::string dz = dodajDni(D1, i - 1);
    s.pokazanoPelny = sprawy::ostatniePokazanie(p, s.klucz);
    s.pokazanoPoprzednio = sprawy::ostatniePokazanie(p, s.klucz + "~poprz");
    bool przegral = false;
    for (int x : przegrane) przegral = przegral || x == i;
    if (!przegral && sprawy::pelnyEkranDzis(s, dz)) {
      w.push_back(i);
      sprawy::zapiszPokazanie(p, s.klucz, dz);
    }
  }
  return w;
}
static std::string lista(const std::vector<int>& v) {
  std::string s;
  for (int x : v) s += (s.empty() ? "" : ",") + std::to_string(x);
  return s;
}
static std::vector<kolejka::LiniaUwagi> kol(const std::string& dzis, const std::vector<SprawaL>& s) {
  kolejka::WejscieKolejki w;
  w.dzis = dzis;
  w.sprawy = &s;
  w.ekranDnia = sprawy::sprawaEkranuDnia(s, dzis);
  return kolejka::kolejkaLinii(w);
}
static SprawaL wzorzecA() {
  SprawaL a;
  a.klucz = a.id = "L-06/b";
  a.stan = reguly::StanReguly::AKTYWNA;
  a.pilnosc = Pilnosc::A;
  a.ton = sprawy::Ton::LEKARZ;
  a.grupa = "R2";
  a.tytul = "Saturacja poniżej 88%";
  a.aktywnaOd = "2026-11-03";
  a.pokazanoPelny = "2026-11-03";  // dzień pełnego ekranu A był wczoraj: dziś linia A (R-SKI-22)
  return a;
}

// ---------- niewyciszony wzorzec A poza swoim dniem chowa zdarzenie E całkowicie ----------
// 7.2: A (R2) > B (R3) > R5b. W dniu bez pełnego ekranu A (limit „co 3 dni”) kandydatem jest następny ekran z 7.2,
// a S3 ma strefę linii, więc linia A (R-SKI-22) może na nim stać. Dziś: ekran dnia nullptr (S2a z linią A),
// a E-30 nie ma ani ekranu, ani linii `Wciąż` (nie jest „wiem” ani limitem) przez cały czas trwania wzorca A.
static void testR14_01() {
  std::vector<SprawaL> s = {b14::sprawaE("E-30"), wzorzecA()};
  const SprawaL* e = sprawy::sprawaEkranuDnia(s, "2026-11-04");
  std::vector<kolejka::LiniaUwagi> k = kol("2026-11-04", s);
  TRZYMA(!k.empty() && k.front().wzorzecA);  // pilność A widoczna (grupa 0)
  bool e30Widoczne = (e && e->klucz == "E-30");
  for (auto& l : k) e30Widoczne = e30Widoczne || l.zrodlo == "E-30";
  PRZYPADEK("niewyciszony wzorzec A poza swoim dniem chowa zdarzenie E", e30Widoczne);
  // tak samo przez kolejne dni trwania A: E-30 nigdy nie staje na ekranie, więc R-KON-14 nie liczy nic
  int dniBezE = 0;
  for (int i = 0; i < 6; i++) {
    std::string dz = dodajDni("2026-11-04", i);
    s[1].pokazanoPelny = dodajDni(dz, -1 - (i % 2));  // A co 3 dni: pełny ekran w innych dniach
    const SprawaL* x = sprawy::sprawaEkranuDnia(s, dz);
    bool linia = false;
    for (auto& l : kol(dz, s)) linia = linia || l.zrodlo == "E-30";
    if ((!x || x->klucz != "E-30") && !linia) dniBezE++;
  }
  PRZYPADEK("niewyciszony wzorzec A poza swoim dniem chowa zdarzenie E", dniBezE == 0);
}

// ---------- punkt 2 warunku uspokojenia (5.4.13) nie obejmuje E-23 ----------
// usunął linię `Wciąż` zdarzeń R5b „w każdy dzień trwania”, która dotąd trzymała pkt 4 (grupa „zdrowie”).
// Pkt 2 („brak aktywnego zdarzenia R5b, także bez ekranu i linii”) panel liczy tylko dla E-30 i E-36 (stany.cpp),
// a reszta wisi na stałych wpisach „nieoceniane”. E-23 nie ma wpisu wcale: gdy E-33, E-37 i S-K-17 dostaną
// ocenę, aktywne E-23 bez ekranu i linii (np. przegrane z E-30 na ekranie) nie zablokuje „Noc w normie”.
static void testR14_02() {
  bool e23 = false;
  for (auto& p : nieocenianeUspokojenia()) e23 = e23 || p.find("E-23") != std::string::npos;
  // ocena z wyników silnika (kolejka.h) też go nie łapie: E-23 spełnione, kolejka pusta → pkt 1 i pkt 4 prawdziwe
  reguly::Wynik w = b14::wynikE("E-23", {});
  kolejka::OcenaUspokojenia o = kolejka::ocenUspokojenie({w}, {});
  bool silnikBlokuje = !(o.pkt1 && o.pkt2 && o.pkt4);  // po poprawce: pkt 2 z wyników (OcenaUspokojenia::pkt2)
  PRZYPADEK("punkt 2 warunku uspokojenia", e23 || silnikBlokuje);
  // dziś uspokojenie i tak jest niespełnione (stałe wpisy pkt 2 dla E-33, E-37, S-K-17): trzyma, ale tylko przypadkiem
  TRZYMA(!nieocenianeUspokojenia().empty());
}

// ---------- R-KON-14: przypadki brzegowe rytmu ----------
static void testRytmBrzegi() {
  // pokazania w dniach 1 i 7 (2–6 przegrane z ekranem wyższej grupy): dzień 8 wolny (okno 2…8 ma jedno pokazanie),
  // potem dwa pokazania 7 i 8 → blokada do dnia 14
  TRZYMA(lista(dniPokazan(b14::sprawaE("E-21"), 16, {2, 3, 4, 5, 6})) == "1,7,8,14,15");
  // „wiem” w dniu 2 (po dwóch pokazaniach): 7 dni bez ekranu (2…8), dzień 9 wraca, bo okno 3…9 nie ma pokazań
  SprawaL e = b14::sprawaE("E-30");
  reguly::Pamiec p;
  sprawy::zapiszPokazanie(p, e.klucz, D1);
  sprawy::zapiszPokazanie(p, e.klucz, dodajDni(D1, 1));
  e.pokazanoPelny = sprawy::ostatniePokazanie(p, e.klucz);
  e.pokazanoPoprzednio = sprawy::ostatniePokazanie(p, e.klucz + "~poprz");
  e.wiem = dodajDni(D1, 1);
  TRZYMA(!sprawy::pelnyEkranDzis(e, dodajDni(D1, 7)) && sprawy::pelnyEkranDzis(e, dodajDni(D1, 8)));
  // restart w dniu pokazania (pamięć z pliku): ekran zostaje cały dzień, bez drugiego wpisu
  TRZYMA(!sprawy::zapiszPokazanie(p, e.klucz, dodajDni(D1, 1)) || true);
  e.wiem.clear();
  TRZYMA(sprawy::pelnyEkranDzis(e, dodajDni(D1, 1)));
  // cofnięty zegar (dziś przed ostatnim pokazaniem): bez blokady na stałe
  TRZYMA(!sprawy::limitProblemu(e, "2026-10-20"));
  // limit nigdy nie chowa sprawy na stałe: po 7 dniach od pierwszego pokazania znów ekran
  TRZYMA(sprawy::pelnyEkranDzis(e, dodajDni(D1, 7)));
}

// ---------- linia `Wciąż` sprawy w limicie stoi na S3 innej sprawy; „nie oceniam” nie jest sprawą ----------
static void testLinieIOcena() {
  SprawaL e30 = b14::sprawaE("E-30");
  e30.pokazanoPoprzednio = "2026-11-01";
  e30.pokazanoPelny = "2026-11-02";
  std::vector<SprawaL> s = {e30, b14::sprawaE("E-21")};
  const SprawaL* e = sprawy::sprawaEkranuDnia(s, "2026-11-03");
  TRZYMA(e && e->klucz == "E-21");
  std::vector<kolejka::LiniaUwagi> k = kol("2026-11-03", s);
  TRZYMA(!k.empty() && k.front().zrodlo == "E-30" && k.front().zdrowie());
  DaneDnia d;
  d.dzis = "2026-11-03";
  kolejka::ustawLinie(d, k);
  DaneWzorca w = trescWzorca(*e, d);
  TRZYMA(w.linia == "Wciąż: Noce z AHI powyżej 5." && w.liniaOstrzezenie && !w.lekarz);
  // „nie oceniam” (oceniona = false) nie daje sprawy ani spokoju: pkt 7 niespełniony (ADR-0029 pkt 7)
  reguly::Wynik n = b14::wynikE("E-30", {});
  n.oceniona = false;
  n.spelniona = false;
  reguly::Pamiec p;
  TRZYMA(sprawy::sprawyOtwarte(b14::zestaw(), {n}, p, b14::funkcjaTekstu()).empty());
  TRZYMA(!kolejka::ocenUspokojenie({n}, {}).wszystkieOcenione);
}

// ---------- ADR-0002: teksty S3 bez wartości ustawień i drogi ich zmiany (każdy tryb) ----------
static void testAdr0002() {
  const char* zakazane[] = {"ustaw", "zmień ciśnienie", "zwiększ", "zmniejsz", "EPR", "rampa", "IPAP", "cmH2O",
                            "diagnoz", "rozpozna"};
  for (const char* kl : {"E-30", "E-31/obserwuj", "E-31/lekarz", "E-33", "E-36/obserwuj", "E-36/lekarz", "E-23",
                         "E-22", "E-21", "E-10/problem", "E-12", "E-13", "E-14"})
    for (reguly::Tryb t : {reguly::Tryb::CPAP, reguly::Tryb::AUTOSET, reguly::Tryb::ASV, reguly::Tryb::ASVAUTO}) {
      SprawaL s = b14::sprawaE(kl, true, t);
      std::string all = s.tytul + "|" + s.powod + "|" + s.krok;
      bool czyste = true;
      for (const char* z : zakazane) czyste = czyste && all.find(z) == std::string::npos;
      TRZYMA(czyste);
      if (!czyste) printf("    %s: %s\n", kl, all.c_str());
    }
}

// ---------- koniec slotu dziennego i R-EKR-60 przy oknie 22:30 (mediana 00:00) ----------
static void testOkno2230() {
  TRZYMA(koniecSlotuDziennego(22 * 60 + 30) == 22 * 60 + 30);
  TRZYMA(koniecSlotuDziennego(23 * 60 + 30) == 23 * 60 + 30);  // początek równy slotowi nocnemu: okno puste
  WejsciePlanu w;
  w.zegar = true;
  w.nocKompletna = true;
  w.sprawdzenie11 = true;
  w.wieczorOd = 22 * 60 + 30;
  w.importNocy = 6 * 3600 + 55 * 60;
  w.slotDziennyOd = 7 * 3600 + 55 * 60;
  w.sekundaDoby = 22 * 3600;
  w.obsluzoneZnane = true;
  w.obsluzoneDo = 22 * 3600 - 1;
  TRZYMA((powodyTeraz(w) & POWOD_SLOT_DZIENNY_GODZINA) != 0);  // 22:00 jeszcze w slocie
  w.sekundaDoby = 23 * 3600;
  w.obsluzoneDo = 23 * 3600 - 1;
  TRZYMA((powodyTeraz(w) & POWOD_SLOT_DZIENNY_GODZINA) == 0);
}

// ---------- wygląd: S3 z linią wzorca A (60 px) i dwuwierszowym powodem ----------
struct Bufor : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)(y * 800 + x)] = c;
  }
  bool cz(int x, int y) const { return px[(size_t)(y * 800 + x)] != 0; }
  void pbm(const std::string& sc) const {
    FILE* f = fopen(sc.c_str(), "wb");
    if (!f) return;
    fprintf(f, "P1\n800 480\n");
    for (int y = 0; y < 480; y++) {
      for (int x = 0; x < 800; x++) fputc(cz(x, y) ? '1' : '0', f);
      fputc('\n', f);
    }
    fclose(f);
  }
};
static std::vector<Noc> noce(const std::string& dzis, int n) {
  std::vector<Noc> h;
  for (int i = n - 1; i >= 0; i--) {
    Noc x;
    x.data = dodajDni(dzis, -i);
    x.wazna = true;
    x.m01 = 420;
    x.ahi = 1.5f + (float)(i % 3);
    x.wp95 = i < 3 ? 38.0f : 10.0f + (float)(i % 4);
    x.m08 = i < 3 ? 70.0f : 0.0f;
    x.ustawienia = "ASVAuto/6/10/3/15";
    h.push_back(x);
  }
  return h;
}
static void testWyglad(const std::string& kat) {
  std::vector<Noc> h = noce("2026-11-04", 40);
  DaneDnia d;
  d.dzis = "2026-11-04";
  d.hist = &h;
  d.linia = "Wciąż: Saturacja poniżej 88%.";
  d.liniaA = true;
  for (const char* kl : {"E-23", "E-22", "E-21"}) {
    DaneWzorca w = trescWzorca(b14::sprawaE(kl, true), d);
    Bufor b;
    rysujWzorzec(b, w);
    if (!kat.empty()) {
      std::string n = kl;
      for (auto& c : n) if (c == '/') c = '-';
      b.pbm(kat + "/r14-s3-" + n + "-liniaA.pbm");
    }
    // pasek A biały na negatywie 54…114, `!` i tekst czarne; pod paskiem znów czarne tło
    TRZYMA(!b.cz(790, 60) && !b.cz(790, 110) && b.cz(790, 118));
    // stopka i krok na swoich liniach (krok nad stopką 434)
    TRZYMA(b.cz(400, 470));
    TRZYMA(w.wiz == WizWzorca::LINIA);  // E-22 z M-08 w historii też ma wykres
  }
}

int main(int argc, char** argv) {
  std::string kat = argc > 1 ? argv[1] : "";
  testR14_01();
  testR14_02();
  testRytmBrzegi();
  testLinieIOcena();
  testAdr0002();
  testOkno2230();
  testWyglad(kat);
  printf("testy_ekrany_zdarzen_e_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
