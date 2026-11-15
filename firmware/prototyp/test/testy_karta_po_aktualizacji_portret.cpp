// SPDX-License-Identifier: GPL-3.0-or-later
// R-KONF-61: Portret nocy rano i ekran problemu idą przed informacją po
// aktualizacji samodzielnej; informacja czeka i staje dopiero na ekranie slotu dziennego bez sprawy (decyzja 12).
// Harness z test/testy_noc_pliki.cpp (dołączony, jego main() przemianowany), jak test/testy_kolejnosc_portret.cpp.
#define main main_testy_noc_pliki
#include "testy_noc_pliki.cpp"
#undef main

#include "aktualizacja.h"

static void uzbrojInf(PanelN& s) {
  aktualizacja::Informacja i;
  i.rodzaj = aktualizacja::RodzajInformacji::UDANA;
  i.wlasna = i.nowa = "1.0.0";
  s.p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, aktualizacja::zapisInformacji(i));
}
static aktualizacja::Informacja inf(PanelN& s) {
  return aktualizacja::odczytInformacji(s.p.nvs.tekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, ""));
}
static bool informacjaWLogu(PanelN& s) { return wLogu(s, "ekran dnia: informacja po aktualizacji samodzielnej"); }

static void wybudzZegarem(PanelN& s, uint32_t t, const KonfiguracjaPanelu& k) {
  s.p.siec.zadania.clear();
  s.p.port.log.clear();
  s.p.d.z.clear();
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  s.p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  s.p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  Dyrygent(s.p.adaptery(), k).wybudzenie();
}

// wymaganie: R-KONF-61 (decyzja 12 z 15.11.2026, rano zawsze wynik nocy)
// Portret rano (S11) i drugi ekran poranka o 07:31 bez informacji; informacja dopiero na ekranie slotu dziennego
// (współrzędne miasta: S-K-37), ▶ na niej: pierwsza strona kokpitu (nie strona Portretu), informacja zamknięta.
static void testR_KONF_61_portretPierwszy() {
  KonfiguracjaPanelu km = KONF;
  km.pogodaLat = 50.2597;
  km.pogodaLon = 19.0217;
  DobaAB d = dobaAB(DZIS4);
  PanelN s;
  uzbrojInf(s);
  odpowiedzi(s, d);
  wybudz(s, ep(2026, 11, 4, 7, 0));
  SPRAWDZ(wLogu(s, "ekran dnia S11 (Portret nocy"));
  SPRAWDZ(!informacjaWLogu(s) && inf(s).dzien.empty());
  // 07:31: drugi ekran poranka (wynik nocy) → bez informacji, dalej czeka
  wybudzZegarem(s, ep(2026, 11, 4, 7, 31), km);
  SPRAWDZ(wLogu(s, "portret: po 30 min drugi ekran poranka"));
  SPRAWDZ(!informacjaWLogu(s) && inf(s).dzien.empty());
  // slot dzienny (start po drugim ekranie poranka, R-POR-04): pierwszy ekran slotu bez sprawy → informacja
  bool jest = false;
  for (int h = 9; h <= 12 && !jest; h++) {
    wybudzZegarem(s, ep(2026, 11, 4, h, 0), km);
    jest = informacjaWLogu(s);
  }
  SPRAWDZ(jest && inf(s).dzien == "2026-11-04");
  SPRAWDZ(wLogu(s, "slot dzienny: S-K-"));
  // ▶ na informacji: pierwsza strona kokpitu, informacja zamknięta
  wybudzPrzyciskiem(s, ep(2026, 11, 4, 12, 35), Ekran::DZIEN, BIT_PRAWY);
  SPRAWDZ(s.p.start.stan.ekran == (uint8_t)Ekran::K1 && inf(s).rodzaj == aktualizacja::RodzajInformacji::BRAK);
}

// wymaganie: R-KONF-61
static void testR_KONF_61_ekranProblemu() {
  DobaAB d = dobaZWyciekiem(DZIS4, 0.5);  // ekran problemu S3 (E-23)
  PanelN s;
  uzbrojInf(s);
  odpowiedzi(s, d);
  wybudz(s, ep(2026, 11, 4, 7, 0));
  SPRAWDZ(wLogu(s, "stopka: ▶ portret nocy (R-POR-24)"));  // ekran problemu z Portretem pod ▶
  SPRAWDZ(!informacjaWLogu(s) && inf(s).dzien.empty());
  for (int m : {31, 60, 120}) {  // problem zostaje ekranem dnia: informacja dalej czeka
    s.p.siec.zadania.clear();
    s.p.port.log.clear();
    s.p.zegar.epoch = ep(2026, 11, 4, 7, 0) + (uint32_t)m * 60;
    s.p.zegar.msTeraz = 0;
    s.p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    s.p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    Dyrygent(s.p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(!informacjaWLogu(s));
  }
  SPRAWDZ(inf(s).dzien.empty());
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testR_KONF_61_portretPierwszy();
  testR_KONF_61_ekranProblemu();
  printf("testy_karta_po_aktualizacji_portret: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
