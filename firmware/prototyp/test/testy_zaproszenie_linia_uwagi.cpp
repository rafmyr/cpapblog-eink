// SPDX-License-Identifier: GPL-3.0-or-later
// Testy linii zaproszenia według ekranu, który faktycznie stoi rano, drogi do „Sprzętu” z kroku S-K-35, linii uwagi
// na białym tle i kolumny paska S2a. Dane wyłącznie syntetyczne. Uruchomienie: tools/prototyp/testy.sh.
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "czynniki.h"
#include "dyrygent.h"
#include "ekrany.h"
#include "kontekst_sk.h"
#include "nawigacja.h"
#include "slot_dzienny.h"
#include "sprzet.h"
#include "humor.h"
#include "start.h"
#include "stany.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(nr, w)                                                                 \
  do {                                                                                 \
    if (w) ok++;                                                                       \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); }       \
  } while (0)

static Noc noc0() {
  Noc n;
  n.wazna = true;
  n.m01 = 420;
  n.ahi = 1.0f;
  n.wmed = 4;
  n.wp95 = 9;
  n.cp95 = 10;
  n.m13 = 14;
  n.m17 = 0;
  n.m08 = 0;
  n.ustawienia = "AutoSet/5/12";
  n.zrodlo = 'A';
  return n;
}
// sk::problemNaEkranieRano() (prosbaSkRano() w dyrygencie): S-K-31 jest pierwszym kandydatem (to on staje na ekranie
// dnia), a S-K-10 w tonie „problem” stoi niżej na liście sk::rano() tego samego ranka. Na S-K-31 linia zaproszenia jest
// dozwolona (7.4, 5.11.3, mapa 5.6.2: ↻ 2 s → pytanie), więc niższy S-K-10 nie może jej zgasić.
static NormaMetryki nmx(double med, double p10, double p90) {
  NormaMetryki x;
  x.n = 30;
  x.med = med;
  x.p10 = p10;
  x.p90 = p90;
  return x;
}
static void testZaproszenieNaSK31MimoSK10Nizej() {
  ElementWymiany el[EL_N];
  el[EL_MASKA] = {"2026-02-01", 6, false};
  el[EL_FILTR] = {"2026-10-10", 4, true};
  sprzet::Elementy e = sprzet::zOdpowiedziami(el, {}, {});
  std::vector<Noc> h;
  std::string koniec = "2026-10-31";
  for (int i = 40; i >= 0; i--) {
    Noc x = noc0();
    x.data = dodajDni(koniec, -i);
    x.wp95 = i >= 21 ? 10.0f : 10.0f + (float)(21 - i);  // ostatnie 21 nocy 11 … 31, jak testSprzet()
    h.push_back(x);
  }
  Norma n;
  n.m01 = nmx(420, 360, 470);
  n.wp95 = nmx(10, 6, 14);
  n.cp95 = nmx(10, 9, 11);
  n.ep95 = nmx(8, 7, 9);
  n.m13 = nmx(14, 13, 15);
  n.m17 = nmx(1, 0, 2);
  std::vector<WpisHumoru> dz;
  sk::Wejscie w;
  w.dzis = koniec;
  w.hist = &h;
  w.norma = &n;
  w.trybProbny = false;
  w.sprzet = &e;
  w.dziennik = &dz;
  std::vector<sk::Tresc> v = sk::rano(w);
  std::string pierwszy, lista;
  bool predykat = sk::problemNaEkranieRano(v);  // prosbaSkRano() z src/dyrygent.cpp po poprawce
  bool s10 = false;
  for (auto& t : v) {
    lista += t.id + " ";
    s10 = s10 || (t.id == "S-K-10" && t.problem);
    if (t.doLogu || t.id == "S-K-30") continue;
    if (pierwszy.empty()) pierwszy = t.id;
  }
  SPRAWDZ("warunek wstępny: S-K-10 w tonie „problem” niżej", s10);
  printf("  kandydaci rano: %s\n", lista.c_str());
  SPRAWDZ("warunek wstępny", pierwszy == "S-K-31");
  SPRAWDZ("zaproszenie na S-K-31", !predykat);  // ekran dnia to S-K-31, więc zaproszenie nie powinno przepaść
}

// krok S-K-35 jest prawdziwy w nawigacji: z S8 ▶ wchodzi na „Sprzęt” z kursorem na masce (pierwszy
// wiersz), ◀ 2 s zapisuje wymianę elementu pod kursorem, ↻ 2 s na masce to nowy model (nie wymiana).
// Dla filtra potrzebne jest jeszcze ▶ (ramka niżej); krok mówi to słowami `ramka na filtrze`.
static void testDrogaDoSprzetu() {
  KontekstNaw k;
  k.wierszySprzetu = EL_N;
  k.kursorSprzetu = 0;
  Przejscie z8 = nacisniecie(Ekran::NORMA, Przycisk::PRAWY, false, k);
  SPRAWDZ("droga do „Sprzętu”", z8.ekran == Ekran::SPRZET);
  Przejscie w = nacisniecie(Ekran::SPRZET, Przycisk::LEWY, true, k);
  SPRAWDZ("droga do „Sprzętu”", w.akcja == Akcja::SPRZET_WYMIENIONE);
  Przejscie m = nacisniecie(Ekran::SPRZET, Przycisk::SRODEK, true, k);
  SPRAWDZ("droga do „Sprzętu”", m.akcja == Akcja::SPRZET_NOWY_MODEL);  // dlatego ↻ 2 s nie może stać w kroku S-K-35
  // ◀ 2 s na ekranie dnia (S-K-35 nie jest S-K-31) nic nie zapisuje
  Przejscie d = nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k);
  SPRAWDZ("droga do „Sprzętu”", d.akcja == Akcja::NIC);
}

// Kolumna paska S2a: mediana czasu z maską w zapisie `12:59` (celowo innym niż noc `12 h 59` nad paskiem, 5.4.2) ze
// strzałką i ramką (pole 184 px od kx − 5, kolumny co 186 px) nie wchodzi na sąsiednią kolumnę.
static void testKolumnaPaskaS2a() {
  int w = szer(F_P28B, "12:59") + 8 + szer(F_P28B, "↑");
  SPRAWDZ("kolumna paska S2a", w <= 172);
  SPRAWDZ("kolumna paska S2a", szer(F_P28B, "– ") <= 172);
}

// linia uwagi na białym tle rysuje się piksel w piksel jak przed wspólną funkcją liniaUwagiEkranu() (kopia starego kodu
// rysujEkranDanych()), dla ostrzeżenia, informacji i linii wzorca A.
struct PlotnoP : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)(y * 800 + x)] = c;
  }
};
static void staraLinia(Plotno& p, const std::string& t, bool a, bool ostrz) {
  int h = a ? 60 : 48;
  const GFXfont* f = a ? F_P28B : F_P24B;
  int y = a ? 94 : 87;
  if (a || ostrz) {
    p.prostokat(0, 54, 800, h, true);
    int x = tekst(p, f, 28, y, "!", false);
    tekst(p, f, x + 12, y, t, false);
  } else {
    p.ramka(12, 57, 776, h - 6, 2);
    tekst(p, f, 28, y, t);
  }
}
static void testLiniaUwagiBialeTlo() {
  for (int a = 0; a < 2; a++)
    for (int o = 0; o < 2; o++) {
      PlotnoP s, n;
      s.prostokat(0, 0, 800, 480, false);
      n.prostokat(0, 0, 800, 480, false);
      staraLinia(s, "Wciąż: Noce z AHI powyżej 5.", a, o);
      int h = liniaUwagiEkranu(n, "Wciąż: Noce z AHI powyżej 5.", a, o, false);
      SPRAWDZ("linia uwagi na białym tle", h == (a ? 60 : 48));
      SPRAWDZ("linia uwagi na białym tle", s.px == n.px);
    }
}

int main() {
  testZaproszenieNaSK31MimoSK10Nizej();
  testDrogaDoSprzetu();
  testKolumnaPaskaS2a();
  testLiniaUwagiBialeTlo();
  printf("testy_zaproszenie_linia_uwagi: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
