// SPDX-License-Identifier: GPL-3.0-or-later
// R-KONF-61, runda poprawek po przeglądzie 15.11.2026): pytanie tak/nie
// rano (R0, S-K-30 „infekcja”) czeka na odpowiedź, a informacja po aktualizacji samodzielnej jest uzbrojona: dopóki
// pytanie czeka (do 11:00), informacja go nie zasłania, także gdy slot dzienny już trwa (pytanie zostaje ekranem dnia,
// slot ustępuje), a wpis czeka bez doby. Harness z test/testy_sprzet_infekcja_sk.cpp (main przemianowany).
#define main main_testy_sprzet_infekcja_sk
#include "testy_sprzet_infekcja_sk.cpp"
#undef main

#include "aktualizacja.h"

static aktualizacja::Informacja infP(const Panel& p) {
  auto i = p.nvs.teksty.find("aktual/u07_inf");
  return aktualizacja::odczytInformacji(i == p.nvs.teksty.end() ? "" : i->second);
}

// wymaganie: R-KONF-61
static void testR_KONF_61_pytanieR0() {
  KonfiguracjaPanelu km = KONF;  // z miastem: slot dzienny ma S-K-37, gdy pytanie nie stoi
  km.pogodaLat = 50.2597;
  km.pogodaLon = 19.0217;
  std::vector<Noc> h;
  for (int i = 40; i >= 0; i--) {  // jak testDyrygentSK30(): odchylenie M-13 dwóch ostatnich nocy → S-K-30
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
  aktualizacja::Informacja i;
  i.rodzaj = aktualizacja::RodzajInformacji::UDANA;
  i.wlasna = i.nowa = "1.0.0";
  p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, aktualizacja::zapisInformacji(i));
  p.zegar.epoch = ep(2026, 11, 4, 7, 30);
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), km).wybudzenie();
  drukujLog(p);
  SPRAWDZ(wLogu(p, "ekran dnia: pytanie infekcja"));
  int pytan = 1, informacji = wLogu(p, "informacja po aktualizacji samodzielnej (") ? 1 : 0, slotow = 0;
  // pytanie bez odpowiedzi do 11:00 (R-CZY-…: potem `brak odpowiedzi`); co 30 min rysowanie ekranu dnia od nowa
  // (start zasilania), a slot dzienny „trwa” od 07:00 (chwila startu w pamięci RTC): pytanie i tak zostaje ekranem dnia
  for (int m = 8 * 60; m <= 10 * 60 + 30; m += 30) {
    p.zegar.epoch = ep(2026, 11, 4, m / 60, m % 60);
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    p.start.stan.slotDziennyEp = ep(2026, 11, 4, 7, 0);
    p.przyciski.skrypt.clear();
    p.port.log.clear();
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Dyrygent(p.adaptery(), km).wybudzenie();
    drukujLog(p);
    pytan += wLogu(p, "ekran dnia: pytanie infekcja");
    slotow += wLogu(p, "slot dzienny: S-K-");
    informacji += wLogu(p, "informacja po aktualizacji samodzielnej (");
  }
  printf("  R-KONF-61 pytanie R0 bez odpowiedzi 07:30–10:30: rysowań z pytaniem %d z 7, ekranów slotu %d, "
         "z informacją %d\n", pytan, slotow, informacji);
  SPRAWDZ(pytan == 7);
  SPRAWDZ(slotow == 0);
  SPRAWDZ(informacji == 0);
  SPRAWDZ(infP(p).rodzaj == aktualizacja::RodzajInformacji::UDANA && infP(p).dzien.empty());
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testR_KONF_61_pytanieR0();
  printf("testy_karta_po_aktualizacji_pytanie: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
