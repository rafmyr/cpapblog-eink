// SPDX-License-Identifier: GPL-3.0-or-later
// Testy po przeglądzie czytelności ekranów (10.11.2026): K2 z wierszami bez danych zwiniętymi do jednej
// linii (R-KOK-18), podpis S7 bez podwójnego „h” i nagłówek strony „Co znaczą te liczby”.
// Uruchomienie: tools/prototyp/testy.sh (program testy_k2_zwijanie_s7_liczby).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "dzien.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "kokpit.h"
#include "noc.h"
#include "podsumowanie.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                                     \
  do {                                                                                 \
    if (w) ok++;                                                                       \
    else { zle++; printf("BŁĄD (%s:%d): %s\n", __FILE__, __LINE__, #w); }              \
  } while (0)
#define ROWNE(a, b)                                                                                              \
  do {                                                                                                           \
    std::string _a = (a), _b = (b);                                                                              \
    if (_a == _b) ok++;                                                                                          \
    else { zle++; printf("BŁĄD (%s:%d): %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)

struct PlotnoTest : Plotno {
  std::vector<unsigned char> px = std::vector<unsigned char>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)y * 800 + (size_t)x] = c;
  }
  int czarne(int x0, int y0, int x1, int y1) const {
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[(size_t)y * 800 + (size_t)x];
    return n;
  }
  // najniższy wiersz z czarnym pikselem w pasie y0…y1 (−1 = brak)
  int dol(int y0, int y1) const {
    for (int y = y1 - 1; y >= y0; y--)
      if (czarne(0, y, 800, y + 1)) return y;
    return -1;
  }
};

static const std::string DZIS = "2026-11-12";
static const char* const TRYBY[4] = {"CPAP/9///", "AutoSet/5/13//", "ASV/8///", "ASVAuto/6/10/3/15"};

// noc z kompletem metryk z AirBridge (każdy wiersz K2 ma wartość)
static Noc pelna(const std::string& d, const char* ust) {
  Noc n;
  n.data = d;
  n.m01 = 420;
  n.wazna = true;
  n.ahi = 1.0f;
  n.oa = 0.4f;
  n.ca = 0.3f;
  n.h = 0.3f;
  n.cmed = 9.0f;
  n.cp95 = 10.5f;
  n.wmed = 4.0f;
  n.wp95 = 10.0f;
  n.emed = 7.0f;
  n.ep95 = 8.0f;
  n.psmed = 5.0f;
  n.psp95 = 7.0f;
  n.m13 = 14.0f;
  n.m17 = 1;
  n.ustawienia = ust;
  n.zrodlo = 'A';
  return n;
}
static std::vector<Noc> okno(const char* ust) {
  std::vector<Noc> h;
  for (int i = 0; i < 30; i++) h.push_back(pelna(dodajDni(DZIS, i - 29), ust));
  return h;
}
static int wiersz(const DaneK2& d, const std::string& e) {
  for (int r = 0; r < d.wierszy; r++)
    if (e == d.w[r].etykieta) return r;
  return -1;
}
static std::string linia(const DaneK2& d) {
  bool zwin[DaneK2::WIERSZE] = {};
  return zwinieteK2(d, zwin);
}
static int zwinietych(const DaneK2& d) {
  bool zwin[DaneK2::WIERSZE] = {};
  zwinieteK2(d, zwin);
  int n = 0;
  for (bool z : zwin) n += z;
  return n;
}

// wymaganie: R-KOK-18
static void testR_KOK_18_zwiniecie() {
  for (const char* ust : TRYBY) {
    const bool asv = std::string(ust).compare(0, 3, "ASV") == 0;
    // 0 zwiniętych: każdy wiersz ma wartość
    std::vector<Noc> h = okno(ust);
    DaneK2 d = budujK2(h, DZIS, Norma{}, {});
    ROWNE(linia(d), "");
    SPRAWDZ(zwinietych(d) == 0);
    // 1: oddechy w żadnej z 30 nocy (np. aparat bez kanału oddechu)
    for (Noc& n : h) n.m13 = NAN;
    d = budujK2(h, DZIS, Norma{}, {});
    ROWNE(linia(d), "brak danych: oddechy");
    SPRAWDZ(zwinietych(d) == 1 && d.w[wiersz(d, "Oddechy /min")].pusty);
    // 2: oddechy i zdjęcia maski, kolejność jak w tabeli
    for (Noc& n : h) n.m17 = -1;
    d = budujK2(h, DZIS, Norma{}, {});
    ROWNE(linia(d), "brak danych: zdjęcia maski, oddechy");
    SPRAWDZ(zwinietych(d) == 2);
    // więcej: noce z karty bez M-03, M-13, M-17 (5 wierszy); w ASV także bez PS
    for (Noc& n : h) n.oa = n.ca = n.h = NAN;
    if (asv)
      for (Noc& n : h) n.psmed = NAN;
    d = budujK2(h, DZIS, Norma{}, {});
    // w ASV `wsparcie PS` nie mieści się już w 744 px (773 px): ten wiersz zostaje w tabeli z kreskami,
    // a krótsze `oddechy` za nim wchodzą do linii (kolejność zwiniętych jak w tabeli)
    ROWNE(linia(d), "brak danych: obturacyjne, centralne, spłycenia, zdjęcia maski, oddechy");
    SPRAWDZ(zwinietych(d) == 5);
    if (asv) {
      bool zwin[DaneK2::WIERSZE] = {};
      zwinieteK2(d, zwin);
      SPRAWDZ(d.w[wiersz(d, "Wsparcie PS")].pusty && !zwin[wiersz(d, "Wsparcie PS")]);
    }
    // jedna wartość w tygodniu wystarcza, żeby wiersz został
    h.back().m13 = 15.0f;
    d = budujK2(h, DZIS, Norma{}, {});
    SPRAWDZ(!d.w[wiersz(d, "Oddechy /min")].pusty);
    SPRAWDZ(linia(d).find("oddechy") == std::string::npos);
    // rysunek: linia nad legendą, legenda i tabela nad stopką (434), wiersz `Oddechy /min` dalej narysowany
    PlotnoTest p;
    rysujK2(p, d);
    SPRAWDZ(p.dol(380, 434) < 434);
    SPRAWDZ(p.czarne(0, 0, 800, 434) > 0);
  }
  // bez żadnej nocy w tygodniu: pełna tabela z kreskami (zdanie `Brak zapisanych nocy…` mówi, czego brak)
  std::vector<Noc> stare = okno("AutoSet/5/13//");
  for (Noc& n : stare) n.data = dodajDni(n.data, -40);
  DaneK2 d = budujK2(stare, DZIS, Norma{}, {});
  ROWNE(linia(d), "");
  // za długa lista: wiersze, które się nie zmieszczą w 744 px, zostają w tabeli
  DaneK2 m;
  m.wierszy = DaneK2::WIERSZE;
  static const char* const DLUGIE[DaneK2::WIERSZE] = {"obturacyjne", "centralne", "spłycenia", "czas z maską", "zdjęcia maski",
                                                     "wyciek", "EPAP", "wsparcie PS", "oddechy", "zdjęcia maski"};
  for (int i = 0; i < DaneK2::WIERSZE; i++) {
    m.w[i].pusty = true;
    m.w[i].nazwaBraku = DLUGIE[i];
  }
  // składowe AHI z wartością w tygodniu: AHI bez wartości zostaje w tabeli nad nimi (wcięcie ma wiersz nadrzędny)
  {
    std::vector<Noc> ha = okno("AutoSet/5/13//");
    for (Noc& n : ha) { n.ahi = NAN; n.m13 = NAN; }
    DaneK2 da = budujK2(ha, DZIS, Norma{}, {});
    SPRAWDZ(!da.w[0].pusty && da.w[1].wciecie && !da.w[1].pusty);
    ROWNE(linia(da), "brak danych: oddechy");
    // AHI i wszystkie składowe bez wartości: zwinięte razem
    for (Noc& n : ha) n.oa = n.ca = n.h = NAN;
    da = budujK2(ha, DZIS, Norma{}, {});
    ROWNE(linia(da), "brak danych: AHI, obturacyjne, centralne, spłycenia, oddechy");
  }
  // AHI mieści się w linii, a składowa już nie: AHI wraca do tabeli
  DaneK2 s2;
  s2.wierszy = 5;
  static const char* const N2[5] = {"AHI", "bardzo długa nazwa składowej, która się nie zmieści w linii brak danych nawet sama", "x", "y", "z"};
  for (int i = 0; i < 5; i++) { s2.w[i].pusty = i < 2; s2.w[i].wciecie = i >= 1 && i <= 3; s2.w[i].nazwaBraku = N2[i]; }
  bool z2[DaneK2::WIERSZE] = {};
  ROWNE(zwinieteK2(s2, z2), "");
  SPRAWDZ(!z2[0] && !z2[1]);
  bool zwin[DaneK2::WIERSZE] = {};
  std::string l = zwinieteK2(m, zwin);
  SPRAWDZ(!l.empty() && szer(F_P22B, l) <= 744);
  int n = 0;
  for (bool z : zwin) n += z;
  SPRAWDZ(n > 0 && n < DaneK2::WIERSZE);
}

// S7: jednostka `h` raz (przy dużej liczbie), podpis bez niej; „Co znaczą te liczby”: nazwa strony w nagłówku
static void testS7ILiczby() {
  std::vector<Noc> h;
  std::string d = "2026-10-01";
  for (int i = 0; i < 61; i++, d = dodajDni(d, 1)) h.push_back(pelna(d, "AutoSet/5/13//"));
  podsumowanie::DaneS7 s = podsumowanie::budujS7("2026-12-01", h, reguly::Tryb::AUTOSET, false);
  ROWNE(s.podpisGodzin, "z maską");
  SPRAWDZ(s.podpisGodzin.find('h') == std::string::npos && !s.godziny.empty());
  for (reguly::Tryb t : {reguly::Tryb::AUTOSET, reguly::Tryb::ASV}) {
    podsumowanie::DaneLiczb l = podsumowanie::budujLiczby(t, true);
    ROWNE(l.naglowek, "Co znaczą te liczby");
    ROWNE(l.etykieta, "");  // bez powtórzenia nazwy w treści
    // nazwa strony obok numeru `4 / 6` w nagłówku (P24B z lewej od 28, P24B z prawej do 772)
    SPRAWDZ(28 + szer(F_P24B, l.naglowek) + 24 <= 772 - szer(F_P24B, "4 / 6"));
    PlotnoTest p, q;
    podsumowanie::rysujLiczby(p, Naglowek{l.naglowek, "4 / 6"}, l);
    SPRAWDZ(p.czarne(0, 0, 800, 50) > 0);
    // pas etykiety (pod nagłówkiem, nad pierwszym wierszem na 133) pusty
    SPRAWDZ(p.czarne(0, 60, 800, 104) == 0);
    l.etykieta = "Co znaczą te liczby";
    podsumowanie::rysujLiczby(q, Naglowek{l.naglowek, "4 / 6"}, l);
    SPRAWDZ(q.czarne(0, 60, 800, 104) > 0);
  }
}

// stopka karty z pogodą (po zdjęciu zdania budzika): ustępuje jak na ekranie danych, nie cała lewa część (R-EKR-10)
static void testStopkaKartyIU01() {
  std::string lewo = "Bielsko-Biała 12° · przelotne opady deszczu ze śniegiem", prawo = "przy łóżku 21° · 45% · CO2 1200";
  SPRAWDZ(szer(F_P22B, lewo) + szer(F_P22B, prawo) + 24 > 744);
  dopasujStopke(lewo, prawo);
  SPRAWDZ(!lewo.empty() && szer(F_P22B, lewo) + szer(F_P22B, prawo) + 24 <= 744);
  ROWNE(lewo, "Bielsko-Biała 12°");  // najpierw ustępuje opis pogody
  ROWNE(prawo, "przy łóżku 21° · 45% · CO2 1200");
  TrescEkranu t;
  t.tytul = "Dobra noc";
  t.stopkaLewo = lewo;
  t.stopkaPrawo = prawo;
  PlotnoTest p;
  rysujKarte(p, {"cz 30.11", ""}, t, "");
  SPRAWDZ(p.czarne(26, 444, 26 + szer(F_P22B, lewo), 472) > 50);  // pogoda z lewej narysowana
  // U-01: pogoda ze współrzędnych wgranych z panelem, bez miasta z listy
  DaneUstawien d;
  d.miasto = MIASTO_ZE_WSPOLRZEDNYCH;
  std::vector<WierszUstawien> w = wierszeUstawien(d);
  bool jest = false;
  for (auto& x : w) jest = jest || (x.nazwa == "Miasto pogody" && x.wartosc == "współrzędne z wgrania");
  SPRAWDZ(jest);
}

int main() {
  testR_KOK_18_zwiniecie();
  testStopkaKartyIU01();
  testS7ILiczby();
  printf("testy_k2_zwijanie_s7_liczby: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
