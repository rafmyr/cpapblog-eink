// SPDX-License-Identifier: GPL-3.0-or-later
// R-KONF-61 w slocie dziennym, runda poprawek po przeglądzie 15.11.2026):
// M9 (ekran slotu z linią uwagi E-08 i uzbrojona informacja), (liczby odświeżeń e-ink w slocie z informacją
// i bez), (znak nowej wersji wraca po narysowaniu informacji). Harness z test/testy_slot_dzienny_dyrygent.cpp
// (main przemianowany), jak test/testy_karta_slot_brzegowe.cpp.
#define main main_testy_slot_dzienny_dyrygent
#include "testy_slot_dzienny_dyrygent.cpp"
#undef main

#include "aktualizacja.h"
#include "ekrany.h"
#include "sekrety.h"

static void uzbrojS(PanelSym& s) {
  aktualizacja::Informacja i;
  i.rodzaj = aktualizacja::RodzajInformacji::UDANA;
  i.wlasna = i.nowa = "1.0.0";
  s.p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, aktualizacja::zapisInformacji(i));
}
static aktualizacja::Informacja infS(PanelSym& s) {
  return aktualizacja::odczytInformacji(s.p.nvs.tekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, ""));
}
static bool maW(const Wyb& w, const std::string& t) {
  for (auto& z : w.zdarzenia)
    if (z.find(t) != std::string::npos) return true;
  return false;
}
static const char* const INF_S = "ekran dnia: informacja po aktualizacji samodzielnej";

// E-08 (`co2_milczy` od 10:00) jako linia uwagi na ekranach slotu od 10:00: wcześniej informacja stoi na ekranie slotu,
// od pojawienia się linii ustępuje przy najbliższym rysowaniu i nie wraca, dopóki linia stoi.
static void odpE08(PanelSym& s, uint32_t t) {
  odpowiedziB9(s, t);
  s.siec.odpowiedzi.erase("/otoczenie");
  std::string ost = t >= ep(2026, 11, 4, 10, 0) ? "[{\"kod\":\"co2_milczy\",\"od\":\"2026-11-04T10:00:00+01:00\"}]" : "[]";
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":600},\"ostrzezenia\":" + ost + "}");
}
// wymaganie: R-KONF-61
static void testR_KONF_61_slotZLiniaUwagi() {
  KonfiguracjaPanelu k = konf(true);
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  PanelSym s;
  uzbrojS(s);
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 14, 30), odpE08, false, k);
  int zLinia = 0, zLiniaIInf = 0, przedInf = 0;
  for (auto& w : v) {
    if (!maW(w, "slot dzienny: S-K-")) continue;
    bool linia = maW(w, "linia uwagi:") || maW(w, "E-08");
    if (w.minuta < 10 * 60) przedInf += maW(w, INF_S);
    if (w.minuta >= 10 * 60 && linia) {
      zLinia++;
      zLiniaIInf += maW(w, INF_S);
    }
  }
  printf("  R-KONF-61 M9: przed 10:00 informacja na ekranie slotu %d; od 10:00 ekranów slotu z linią E-08 %d, z "
         "informacją %d\n", przedInf, zLinia, zLiniaIInf);
  SPRAWDZ(przedInf > 0);
  SPRAWDZ(zLinia > 0);
  SPRAWDZ(zLiniaIInf == 0);
  SPRAWDZ(infS(s).dzien == "2026-11-04");  // pokazana rano w slocie, z końcem doby przepada
}

// odświeżenia e-ink w slocie dziennym (od pierwszego ekranu slotu do 21:00) w dobie
// z informacją uzbrojoną rano i bez niej; informacja stoi do przycisku albo do końca doby (decyzja 11), więc pogoda,
// wietrzenie i „jutro rano” pod nią się nie zmieniają. Bez przycisku, CO2 600 ppm (bez `Przewietrz teraz.`).
static void odpBezCo2(PanelSym& s, uint32_t t) {
  odpowiedziB9(s, t);
  s.siec.odpowiedzi.erase("/otoczenie");
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":600},\"ostrzezenia\":[]}");
}
// wymaganie: R-KONF-61
static void testR_KONF_61_liczbySlotu() {
  KonfiguracjaPanelu k = konf(true);
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  int odsw[2] = {0, 0}, slotow[2] = {0, 0}, zInf = 0, od = 24 * 60;
  std::vector<Wyb> v[2];
  for (int z = 0; z < 2; z++) {
    PanelSym s;
    if (z == 0) uzbrojS(s);
    v[z] = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 21, 0), odpBezCo2, false, k);
  }
  for (auto& w : v[1])
    if (maW(w, "slot dzienny: S-K-") && w.minuta < od) od = w.minuta;
  for (int z = 0; z < 2; z++)
    for (auto& w : v[z]) {
      if (w.minuta < od || w.minuta >= 21 * 60) continue;
      odsw[z] += w.m.odswiezen;
      slotow[z] += maW(w, "slot dzienny: S-K-");
      if (z == 0) zInf += maW(w, INF_S);
    }
  printf("  R-KONF-61 slot dzienny %02d:%02d–21:00: rysowań slotu %d (bez informacji %d), z informacją %d; "
         "odświeżenia e-ink z informacją %d, bez %d\n", od / 60, od % 60, slotow[0], slotow[1], zInf, odsw[0], odsw[1]);
  SPRAWDZ(zInf > 0 && zInf == slotow[0]);  // od pierwszego ekranu slotu informacja stoi do końca slotu
  SPRAWDZ(odsw[0] == 1);                   // jedno odświeżenie: pierwsze pokazanie, potem klatka bez zmian
  SPRAWDZ(odsw[1] > odsw[0]);
}

// po narysowaniu informacji znak nowej wersji wraca do stanu sprzed niej (informacja nie zeruje go na resztę
// wybudzenia): panel zna nowszą wersję 1.0.1 (klucz wydań, sprawdzenie dziś), informacja dla 1.0.0 na ekranie slotu.
// wymaganie: R-KONF-61
static void testR_KONF_61_znakWersji() {
  KonfiguracjaPanelu k = konf(true);
  for (int z = 0; z < 2; z++) {
    PanelSym s;
    sekrety::zapiszKluczWydan(s.p.nvs, "github_pat_ZNACZNIKTOKENU_0123456789abcdef");
    aktualizacja::Stan st;
    st.znana = "1.0.1";
    st.sprawdzonoDnia = "2026-11-04";
    st.sprawdzonoEp = ep(2026, 11, 4, 0, 1);
    aktualizacja::zapisz(s.p.nvs, st);
    if (z == 0) uzbrojS(s);
    std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 9, 30), odpBezCo2, false, k);
    bool inf = false;
    for (auto& w : v) inf = inf || maW(w, INF_S);
    bool znak = znakiBelki().nowaWersja;
    printf("  R-KONF-61 %s: informacja %d, znak nowej wersji po wybudzeniu %d\n",
           z == 0 ? "z informacją" : "bez (baza)", inf, znak);
    SPRAWDZ(inf == (z == 0));
    SPRAWDZ(znak);
  }
}

// ekran przykryty informacją nie zużywa limitu: obrazek dnia S-K-38 (dziennik obrazków) nigdy nie stoi pod
// informacją, więc w bloku obrazka (08:00–10:00) obrazek jest na ekranie i wpisany raz, a informacja staje dopiero
// na następnym ekranie slotu bez zużycia (S-K-37 od 10:00). Przerywnik S2d: test/testy_karta_po_aktualizacji.cpp.
// wymaganie: R-KONF-61
static void testR_KONF_61_obrazekNieZuzyty() {
  PanelSym s;
  s.p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
  s.p.pliki.pliki["/obrazki/obrazki.json"] = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\"}]}";
  uzbrojS(s);
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 12, 30), odpowiedziB9, false, konf(true));
  int sk38 = 0, sk38Inf = 0, infPo = 0;
  for (auto& w : v) {
    bool o = maW(w, "slot dzienny: S-K-38");
    sk38 += o;
    sk38Inf += o && maW(w, INF_S);
    infPo += !o && maW(w, INF_S);
  }
  std::string dz = s.p.pliki.pliki["/dane/obrazki.csv"] + s.p.pliki.pliki["/obrazki.csv"];
  size_t wpisow = 0;
  for (size_t q = dz.find("001_syntetyczny.pbm"); q != std::string::npos; q = dz.find("001_syntetyczny.pbm", q + 1)) wpisow++;
  printf("  R-KONF-61 S-K-38 narysowany %d razy (pod informacją %d), wpisów w dzienniku obrazków %zu; informacja "
         "później na innym ekranie slotu %d\n", sk38, sk38Inf, wpisow, infPo);
  SPRAWDZ(sk38 > 0 && sk38Inf == 0);
  SPRAWDZ(wpisow == 1);
  SPRAWDZ(infPo > 0);
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testR_KONF_61_slotZLiniaUwagi();
  testR_KONF_61_liczbySlotu();
  testR_KONF_61_znakWersji();
  testR_KONF_61_obrazekNieZuzyty();
  printf("testy_karta_po_aktualizacji_slot: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
