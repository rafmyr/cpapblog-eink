// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
// Podgląd ekranów na komputerze: ten sam kod rysowania co na panelu (src/ekrany.cpp), dane syntetyczne
// (nie prawdziwe noce, WY12). Zapisuje pliki PBM 800×480 do katalogu z argumentu; tools/prototyp/testy.sh
// zamienia je na PNG.
#include <cstdio>
#include <regex>
#include <string>
#include <vector>
#include "czynniki.h"
#include "dzien.h"
#include "ekrany.h"
#include "humor.h"
#include "kolejka.h"
#include "sprawy.h"
#include "zestaw_kolejki.h"
#include "kokpit.h"
#include "stany.h"
#include "sypialnia.h"
#include "otoczenie.h"
#include "ekrany_kontekst.h"
#include "kontekst.h"
#include "obrazki.h"
#include "slot_dzienny.h"
#include "wspolne.h"
#include "odkrycia.h"
#include "podsumowanie.h"
#include "raport_ekran.h"
#include "ekrany_konfiguracji.h"
#include "aktualizacja.h"
#include "kontekst_sk.h"
#include "sprzet.h"
#include "ekrany_zdarzen.h"
#include "zdarzenia_ekrany_e.h"
#include "portret.h"
#include "portret_ekran.h"

struct PlotnoPamiec : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[y * 800 + x] = c;
  }
  bool zapisz(const std::string& sciezka) {
    FILE* f = fopen(sciezka.c_str(), "wb");
    if (!f) return false;
    fprintf(f, "P4\n800 480\n");
    for (int y = 0; y < 480; y++)
      for (int x = 0; x < 800; x += 8) {
        uint8_t b = 0;
        for (int k = 0; k < 8; k++) b = (uint8_t)(b << 1) | px[y * 800 + x + k];
        fputc(b, f);
      }
    fclose(f);
    return true;
  }
};

using Wynik_ = reguly::Wynik;
static std::string kat;
static std::string scenaK22, scenaS5, zdanieS5;  // sceny kart z podglądu (makiety kart mają te same, 1:1)
static void zapisz(PlotnoPamiec& p, const char* n) {
  std::string s = kat + "/" + n + ".pbm";
  printf("  %s\n", s.c_str());
  p.zapisz(s);
}

// syntetyczna historia: 120 nocy, prosty generator liczb (stałe ziarno). wariant: 0 = spokojny tydzień,
// 1 = noc z odchyleniem AHI i noce z dużym wyciekiem (stan odchylenia wycieku od 9 nocy), 2 = ostatnia noc niepewna
static std::vector<Noc> historia(const std::string& dzis, int wariant) {
  std::vector<Noc> h;
  uint32_t z = 7;
  auto los = [&]() { z = z * 1103515245u + 12345u; return (z >> 16) % 1000 / 1000.0; };
  for (int i = 119; i >= 0; i--) {
    if (i == 9 || i == 40) continue;  // dwie noce bez rekordu
    Noc n;
    n.data = dodajDni(dzis, -i);
    n.m01 = 380 + (int)(los() * 80);
    if (i == 17) n.m01 = 150;
    n.wazna = n.m01 >= 240;
    n.ahi = (float)(0.8 + los() * 1.4);
    n.cmed = (float)(9.0 + los() * 1.0);
    n.cp95 = (float)(10.2 + los() * 1.2);
    n.wmed = (float)(2 + los() * 4);
    n.wp95 = (float)(6 + los() * 6);
    n.emed = (float)(7.0 + los() * 1.2);  // ASV: EPAP i wsparcie PS
    n.ep95 = n.emed + (float)(0.5 + los() * 0.8);
    n.psmed = (float)(6.0 + los() * 1.6);
    n.psp95 = n.psmed + (float)(1.5 + los() * 1.5);
    n.cmed = n.emed + n.psmed;
    n.ustawienia = i <= 40 ? "ASVAuto/6/10/3/15" : "ASVAuto/6/10/2/15";  // zmiana ustawień 41 nocy temu
    n.zrodlo = 'K';
    if (i < 60) {  // ostatnie 60 nocy z AirBridge: M-03 (OA, CA, H na godzinę) i M-09; noce z karty bez nich (R-DANE-31)
      n.zrodlo = 'A';
      n.oa = (float)(0.1 + los() * 0.5);
      n.ca = (float)(0.2 + los() * 0.6);
      n.h = std::max(0.0f, n.ahi - n.oa - n.ca);
      n.csr = 0;
    }
    if (wariant == 1) {
      if (i <= 10) n.wp95 = (float)(27 + los() * 7);  // wyciek od 11 nocy: stan odchylenia od 9. nocy (R-NOR-03)
      if (i == 1) n.ahi = 5.4f;
      if (i == 0) { n.ahi = 4.6f; n.m01 = 368; }
    }
    if (wariant == 2 && i == 0) { n.wmed = 34; n.wp95 = 52; }  // mediana wycieku > 30 L/min: noc niepewna (4.9)
    h.push_back(n);
  }
  return h;
}

// ---------- stany z makiet skali B (docs/design/czytelnosc-b/makiety) ----------
// Dane jak w generatorach makiet (gen_b.py, gen.py), w tym najgorsze przypadki: `12 h 59`, AHI `12,4`, wyciek `112`,
// EPAP `14,5`, najdłuższe linie uwagi, E-01 i E-03. Struktury ekranu budowane wprost (teksty spoza logiki prototypu,
// np. linia wzorca A, E-01 trzeciej nocy, rocznica), żeby porównać rysowanie piksel w piksel. Pliki `mak-<stan>.png`.

// R-EKR-08 jak twarde() w gen_b.py: twarda spacja między liczbą a jednostką i po jednoliterowych słowach
static std::string twarde(const std::string& t) {
  static const std::string NB = "\xC2\xA0";
  std::string r = std::regex_replace(t, std::regex("([0-9]) (h|min|l/min|nocy|nocach|tyg\\.|s)\\b"), "$1" + NB + "$2");
  r = std::regex_replace(r, std::regex("([0-9])\xC2\xA0h ([0-9])"), "$1" + NB + "h" + NB + "$2");
  r = std::regex_replace(r, std::regex("(^| )([ozwiaujOZWIAUJ]) "), "$1$2" + NB);
  return r;
}

static Kolumna kol(const std::string& n, const std::string& w, const std::string& s = "↔", const std::string& sl = "",
                   bool poza = false) {
  Kolumna k;
  k.et = n;
  k.w = w;
  k.s = s;
  k.p = sl;
  k.poza = poza;
  return k;
}

static std::string opisMak;  // opis nocy na makiecie pytania, z logiki (main)
static void makiety(PlotnoPamiec& p) {
  const std::vector<std::string> STOPKA = {"Katowice 12° · deszcz po 15:00", "przy łóżku 21° · 45%"};
  const std::vector<std::string> STOPKA_MAX = {"Katowice −12° · śnieg po 15:00", "przy łóżku 35° · 99%"};
  const std::string N_MAX = "cz 30.11", P_MAX = "dane 23:59 · ocena ograniczona";
  const std::string ROZNICE = "Tydzień: różnice zaznaczone niżej.";
  const std::vector<Kolumna> SPOKOJ = {kol("czas z maską", "9:07"), kol("AHI", "0,0"), kol("EPAP", "7,7"),
                                       kol("wyciek l/min", "8")};
  const std::vector<Kolumna> NAJGORSZE = {kol("czas z maską", "12:59", "↑", "lepiej niż zwykle"),
                                          kol("AHI", "12,4", "↑", "wyżej niż zwykle", true),
                                          kol("EPAP", "14,5", "↓", "niżej niż zwykle", true),
                                          kol("wyciek l/min", "112", "↑", "wyżej niż zwykle", true)};
  const std::vector<Kolumna> ODCH = {kol("czas z maską", "6:52"), kol("AHI", "3,4", "↑", "wyżej niż zwykle", true),
                                     kol("EPAP", "7,9"), kol("wyciek l/min", "4", "↓", "lepiej niż zwykle")};
  auto rysuj = [&](const std::string& nazwa, const Naglowek& n, TrescEkranu t) {
    t.powod = twarde(t.powod);
    t.krok = twarde(t.krok);
    t.tytul = twarde(t.tytul);
    rysujEkranDanych(p, n, t);
    zapisz(p, ("mak-" + nazwa).c_str());
  };
  auto tydz = [&](const std::string& czas, const std::string& ahi, std::vector<Kolumna> k, const std::string& zdanie,
                  const std::vector<std::string>& st) {
    TrescEkranu t;
    t.uklad = Uklad::TYDZIEN;
    t.podpisNocy = "Ostatnia noc";
    t.tytulPaska = "Ostatnie 7 nocy · mediana";
    t.czas = czas;
    t.ahi = ahi;
    t.kolumny = k;
    t.powod = zdanie;
    t.stopkaLewo = st[0];
    t.stopkaPrawo = st[1];
    return t;
  };
  TrescEkranu t;
  rysuj("s2a-zwykly", {"pt 30.10", "dane 05:42"}, tydz("9 h 09", "0,1", SPOKOJ, "Tydzień jak zwykle.", STOPKA));
  rysuj("s2a-najgorsze-wartosci", {N_MAX, P_MAX}, tydz("12 h 59", "12,4", NAJGORSZE, ROZNICE, STOPKA_MAX));
  {  // ikona aktualizacji w stopce ekranu dnia (R-EKR-61): najdłuższa stopka skrócona do pełnych słów przed ikoną
    IkonaStopki ik(true);
    TrescEkranu ti = tydz("12 h 59", "12,4", NAJGORSZE, ROZNICE, STOPKA_MAX);
    rysujEkranDanych(p, {N_MAX, P_MAX}, ti);
    ikonaAktualizacji(p, false);
    zapisz(p, "mak-s2a-ikona-aktualizacji");
  }
  rysuj("s2a-odchylenie", {"wt 27.10", "dane 05:38"}, tydz("6 h 38", "4,1", ODCH, ROZNICE, STOPKA));
  t = tydz("6 h 38", "", SPOKOJ, "Tydzień jak zwykle.", STOPKA);
  t.ahiNiepewne = true;
  rysuj("s2a-niepewna", {"śr 28.10", "dane 07:12 · ocena ograniczona"}, t);
  t.linia = "Wczoraj duży wyciek: 999 min. Bezdechy mniej pewne.";  // E-20, grupa 6: informacja (biały pasek)
  rysuj("s2a-niepewna-linia", {"śr 28.10", "dane 07:12 · ocena ograniczona"}, t);
  rysuj("s2a-bez-czasu", {"cz 29.10", "dane 06:05"},
        tydz("–", "1,1", {kol("czas z maską", "–", ""), kol("AHI", "1,1"), kol("EPAP", "7,8", "", "uczę się"),
                          kol("wyciek l/min", "9")}, "Tydzień jak zwykle.", STOPKA));
  rysuj("s2a-w-normie", {"pt 30.10", "dane 05:42"}, tydz("9 h 09", "0,1", SPOKOJ, "Noc w normie. Tydzień jak zwykle.", STOPKA));
  rysuj("s2a-w-normie-najgorsze", {N_MAX, "dane 23:59"},
        tydz("12 h 59", "12,4", {NAJGORSZE[0], kol("AHI", "12,4", "↓", "lepiej niż zwykle"), kol("EPAP", "14,5"),
                                 kol("wyciek l/min", "112", "↓", "lepiej niż zwykle")}, "Noc w normie. " + ROZNICE, STOPKA_MAX));
  t = tydz("9 h 09", "0,1", SPOKOJ, "Tydzień jak zwykle.", STOPKA);
  t.podpisNocy = "Noc 29.10";
  rysuj("s2a-dane-z", {"pt 30.10", "dane z 29.10"}, t);
  t = tydz("12 h 59", "12,4", NAJGORSZE, ROZNICE, STOPKA_MAX);
  t.linia = "Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij pomoc.";  // R-INF-10, grupa „zdrowie”
  t.liniaOstrzezenie = true;
  rysuj("s2a-linia-najdluzsza", {N_MAX, P_MAX}, t);
  t = tydz("6 h 38", "4,1", ODCH, ROZNICE, STOPKA);
  t.linia = "Co było inaczej? ↻ 2 s";
  rysuj("s2a-linia-zaproszenia", {"wt 27.10", "dane 05:38"}, t);
  t = tydz("12 h 59", "12,4", NAJGORSZE, ROZNICE, STOPKA_MAX);
  t.linia = "Wciąż: Ciśnienie przy górnej granicy. Omówione 30.11.";
  t.liniaA = true;
  rysuj("s2a-linia-wzorca-A", {N_MAX, P_MAX}, t);
  t = tydz("12 h 59", "12,4", {kol("temperatura", "35,5°", "↑", "zwykle 21,5°", true), kol("wilgotność", "99%", "↑", "zwykle 45%", true),
                               kol("CO2 ppm", "2400", "↑", "zwykle 820", true), kol("PM2.5", "999", "↔")},
           "Przy łóżku: różnice niżej.", STOPKA_MAX);
  t.tytulPaska = "Przy łóżku · ostatnia noc";
  rysuj("s2c-sypialnia", {N_MAX, "dane 23:59"}, t);
  t = tydz("7 h 02", "0,6", {kol("temperatura", "20,5°"), kol("wilgotność", "48%")}, "Dobre warunki do snu.",
           {"Katowice 9° · pogodnie", ""});
  t.tytulPaska = "Przy łóżku · ostatnia noc";
  rysuj("s2c-bez-qingping", {"sb 31.10", "dane 05:40"}, t);
  // S2b
  t = TrescEkranu();
  t.uklad = Uklad::SERIA;
  t.seria = 1000;
  for (bool& k : t.kropka) k = true;
  t.stopkaLewo = STOPKA_MAX[0];
  t.stopkaPrawo = STOPKA_MAX[1];
  rysuj("s2b-seria", {N_MAX, P_MAX}, t);
  t.seria = 12;
  const int kr[19] = {1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1};
  for (int i = 0; i < 30; i++) t.kropka[i] = i < 19 ? kr[i] : 1;
  t.linia = "Noc 30.11 zapisana częściowo, liczby mogą być niepełne.";
  t.stopkaLewo = STOPKA[0];
  t.stopkaPrawo = STOPKA[1];
  rysuj("s2b-seria-linia", {"pn 26.10", "dane 05:38"}, t);
  // S6
  auto s6 = [&](const std::string& powod, int n, const std::string& l, const std::string& pr, const std::vector<std::string>& st) {
    TrescEkranu r;
    r.uklad = Uklad::ROZRUCH;
    r.etykieta = "PIERWSZE DNI";
    r.tytul = "Uczę się twojej normy.";
    r.powod = powod;
    r.postep = n;
    r.postepLewo = l;
    r.postepPrawo = pr;
    r.stopkaLewo = st[0];
    r.stopkaPrawo = st[1];
    return r;
  };
  rysuj("s6-rozruch-dzien1", {"pn 2.11", "dane 05:40"},
        s6("Do tego czasu pokazuję tylko sprawy, które nie wymagają normy.", 1, "noc 1 z 14", "pełna norma od ok. 15.11", STOPKA));
  rysuj("s6-rozruch-najdluzszy", {N_MAX, P_MAX},
        s6("Ostatnia noc: 12 h 59 z maską, oddechy na minutę: 99,9.", 13, "noc 13 z 14", "pełna norma od ok. 30.11", STOPKA_MAX));
  rysuj("s6-brak-nowych", {"pn 16.11", "dane z 2.11"},
        s6("Do tego czasu pokazuję tylko sprawy, które nie wymagają normy.", 0, "brak nowych nocy", "norma po 14 nocach", STOPKA));
  // kamień milowy jako ekran danych (dzień z linią, R-HUM-17) i rocznica
  t = TrescEkranu();
  t.etykieta = "KAMIEŃ MILOWY";
  t.duza = "365";
  t.podpisDuzej = "nocy z maską";
  t.powod = "Pierwsza noc: 30.10.2025. Każda się liczy.";
  t.krok = "→ Następny cel: kolejny rok.";
  t.linia = "Ekran: bateria 15%, naładuj w tym tygodniu.";
  t.stopkaLewo = STOPKA_MAX[0];
  t.stopkaPrawo = STOPKA_MAX[1];
  rysuj("kamien-linia", {N_MAX, P_MAX}, t);
  t.duza.clear();
  t.linia.clear();
  t.tytul = "Rok z maską";
  t.powod = "Rok temu pierwsza noc. Od tej pory 999 nocy.";
  t.stopkaLewo = STOPKA[0];
  t.stopkaPrawo = STOPKA[1];
  rysuj("kamien-rocznica", {"sb 31.10", "dane 05:40"}, t);
  // S5
  auto s5 = [&](const std::string& tyt, const std::string& pow, const std::string& fakt, const std::string& krok) {
    TrescEkranu r;
    r.ramkaPrzerywana = true;
    r.etykieta = "BRAK DANYCH Z NOCY";
    r.tytul = tyt;
    r.powod = pow;
    r.fakt = fakt;
    r.krok = krok;
    r.stopkaPrawo = "↻ ponów";
    return r;
  };
  rysuj("s5-e01", {"cz 30.11", "dane z 29.11"},
        s5("Brak danych z nocy 30.11", "AirBridge nie odpowiada od 29.11 23:59. Ponawiam co\xC2\xA0" "30 min.", "",
           "→ Sprawdź zasilanie AirBridge."));  // jedna godzina: w powodzie
  rysuj("s5-e01-3noc", {"cz 30.11", "dane z 27.11"},
        s5("Brak danych od 999 nocy", "AirBridge nie odpowiada od 30.11. Ponawiam co\xC2\xA0" "30 min.", "",
           "→ Sprawdź AirBridge i sieć. Ustawienia: strona 3 / 3."));
  rysuj("s5-e03", {"cz 30.11", "dane z 29.11"},
        s5("Nie rozumiem danych AirBridge", "Format raportu zmienił się (AirBridge v1.12.10).", "ostatnie dane: 29.11 23:59",
           "→ Daj znać osobie, która instalowała panel."));

  // ---------- kokpit, pytanie, S9, test, karty (docs/design/czytelnosc-b/makiety) ----------
  uint32_t z = 7;
  auto los = [&]() { z = z * 1103515245u + 12345u; return (z >> 16) % 1000 / 1000.0; };
  auto seria = [&](double baza, double rozrzut, double trend, double med) {
    Linia30 l;
    for (int i = 0; i < 30; i++) {
      l.ma[i] = i != 10 && i != 11;
      l.v[i] = (float)(baza + (los() * 2 - 1) * rozrzut + trend * i);
    }
    l.medNormy = med;
    return l;
  };
  DaneK1 k1;
  k1.naglowek = "Ostatnia noc · cz 12.11";
  k1.romb = true;
  k1.jestNoc = true;
  const char* liczby[4] = {"10:59", "12,4", "14,5", "112"};
  const char* opisy[4] = {"czas z maską", "AHI", "EPAP (urządzenie)", "wyciek l/min"};
  for (int i = 0; i < 4; i++) { k1.liczby[i] = liczby[i]; k1.opisy[i] = opisy[i]; }
  struct { const char* n; double b, r, tr, med; const char* md; const char* s1; const char* s2; bool neg; } w1[5] = {
      {"AHI", 5, 2, 0.25, 6, "12,4", "wyżej niż zwykle", "od 12 nocy", true},
      {"Czas z maską", 9.5, 0.8, 0, 9.5, "10:59", "stabilnie", "", false},
      {"EPAP", 13, 0.8, 0, 13, "14,5", "rośnie", "", false},
      {"Wsparcie", 9, 0.8, 0, 9, "10,0", "uczę się", "", false},
      {"Wyciek", 60, 25, -0.8, 60, "112", "spada", "wyciek widoczny", false}};  // R-KOK-16, R-EKR-59
  for (int i = 0; i < 5; i++) {
    k1.w[i].nazwa = w1[i].n;
    k1.w[i].linia = seria(w1[i].b, w1[i].r, w1[i].tr, w1[i].med);
    k1.w[i].mediana = w1[i].md;
    k1.w[i].slowo = w1[i].s1;
    k1.w[i].slowo2 = w1[i].s2;
    k1.w[i].stan = w1[i].neg;
  }
  rysujK1(p, k1);
  zapisz(p, "mak-k1");
  {  // CPAP i AutoSet (R-KOK-15, ADR-0030): 4 wiersze, `ciśnienie` (M-05 mediana) zamiast EPAP i wsparcia
    DaneK1 kc = k1;
    kc.wierszy = 4;
    kc.liczby[2] = "9,6";
    kc.opisy[2] = "ciśnienie";
    kc.w[2].nazwa = "Ciśnienie";
    kc.w[2].linia = seria(9.4, 0.4, 0.04, 9.4);
    kc.w[2].mediana = "9,8";
    kc.w[2].slowo = "rośnie";
    kc.w[3] = k1.w[4];
    rysujK1(p, kc);
    zapisz(p, "mak-k1-cpap");
  }

  DaneK2 k2;
  k2.zakres = "6–12 listopada";
  k2.zdanie = "Tydzień w normie, poza maską z trzech ostatnich nocy.";  // tylko makieta; w panelu `{n} z 7 nocy…`
  const char* noce[7] = {"pt 6", "sb 7", "nd 8", "pn 9", "wt 10", "śr 11", "cz 12"};
  for (int i = 0; i < 7; i++) k2.kol[i] = noce[i];
  k2.romb[3] = true;
  struct { const char* e; bool gr, wc; const char* v[7]; const char* norma; int prog, poza; bool neg; } w2[10] = {
      {"AHI", false, false, {"1,5", "1,9", "5,4", "2,1", "1,1", "8,8", "12,4"}, "10,5–12,3", 0x64, 0x1B, false},
      {"obturacyjne", false, true, {"0,3", "0,2", "4,1", "0,4", "0,4", "10,8", "8,9"}, "0,1–0,5", 0, 0x64, false},
      {"centralne", false, true, {"0,7", "0,4", "0,5", "0,6", "0,5", "0,3", "10,2"}, "0,3–0,7", 0, 0x40, false},
      {"spłycenia", false, true, {"0,6", "1,3", "0,9", "1,1", "0,1", "0,7", "11,0"}, "0,0–1,2", 0, 0x42, false},
      {"Czas z maską", true, false, {"10:59", "9:18", "8:42", "6:57", "10:38", "9:31", "–"}, "9:48–10:59", 0, 0x08, false},
      {"Zdjęcia maski", false, false, {"1", "0", "2", "0", "1", "12", "–"}, "0–2", 0, 0x20, false},
      {"Wyciek l/min", true, false, {"11", "7", "10", "31", "112", "28", "–"}, "6–11", 0x38, 0, true},
      {"EPAP P95", false, false, {"14,5", "8,7", "8,4", "7,7", "8,5", "8,7", "–"}, "7,9–9,0", 0, 0x01, false},
      {"Wsparcie PS", false, false, {"10,0", "6,5", "7,6", "6,6", "7,5", "6,6", "–"}, "uczę się", 0, 0, false},
      {"Oddechy /min", false, false, {"14", "15", "–", "16", "14", "22", "–"}, "13–16", 0, 0x20, false}};
  for (int r = 0; r < 10; r++) {
    WierszK2& w = k2.w[r];
    w.etykieta = w2[r].e;
    w.grupa = w2[r].gr;
    w.wciecie = w2[r].wc;
    w.norma = w2[r].norma;
    w.stan = w2[r].neg;
    for (int i = 0; i < 7; i++) {
      w.k[i].tekst = w2[r].v[i];
      w.k[i].ponadProg = w2[r].prog >> i & 1;
      w.k[i].poza = w2[r].poza >> i & 1;
    }
  }
  rysujK2(p, k2);
  zapisz(p, "mak-k2");
  {  // CPAP i AutoSet: 9 wierszy, `Ciśnienie P95` (M-05p) zamiast `EPAP P95` i `Wsparcie PS`
    DaneK2 kc = k2;
    kc.wierszy = 9;
    kc.w[7].etykieta = "Ciśnienie P95";
    const char* c[7] = {"11,2", "10,8", "11,0", "12,4", "10,9", "11,1", "–"};
    for (int i = 0; i < 7; i++) {
      kc.w[7].k[i] = KomorkaK2();
      kc.w[7].k[i].tekst = c[i];
    }
    kc.w[7].k[3].poza = true;
    kc.w[7].norma = "10,4–11,6";
    kc.w[8] = k2.w[9];
    rysujK2(p, kc);
    zapisz(p, "mak-k2-cpap");
    kc.krokPasma = "→ Pasmo AHI wysokie. Obserwuj kolejne noce.";  // R-KOK-24 (wariant b
    rysujK2(p, kc);
    zapisz(p, "mak-k2-cpap-pasmo-ahi");
  }

  DanePytania q;
  q.dataPoranka = "2026-11-30";
  q.innaNoc = true;
  q.opisNocy = opisMak;  // tekst z logiki (opisNocyPytania), najgorsza para: AHI i czas z maską
  q.kursor = POZYCJA_NIC;
  q.zaznaczone = CZ_ALKOHOL | CZ_LEKI;
  q.termin = 23 * 60 + 30;
  rysujPytanie(p, q);
  zapisz(p, "mak-pytanie");

  DaneNocy dn;
  dn.wieczor = "2026-10-30";
  dn.temp = 21.0f;
  dn.wilg = 45.0f;
  dn.zwyklySen = 10 * 60 + 59;
  dn.maskaOd = 22 * 60 + 47;
  dn.wybor = wyborNocy(0);  // A1-W12
  dn.wybor.zdanie = "Maska schodziła ze sceny częściej niż trzeba.";  // najdłuższe zdanie budzika (45 zn.)
  rysujDobranoc(p, dn);
  zapisz(p, "mak-noc-a1");
  dn.maskaOd = -1;
  rysujDobranoc(p, dn);
  zapisz(p, "mak-noc-a1-bez-godziny");
  dn.maskaOd = 22 * 60 + 47;
  dn.wybor = wyborNocy(4);  // A2-G13, `Księżyc gra na zwłokę. Ja na sen.`
  rysujDobranoc(p, dn);
  zapisz(p, "mak-noc-a2");
  dn.wieczor = "2026-10-26";  // poniedziałek: najdłuższy nagłówek A3
  dn.wybor = wyborNocy(2);   // A3-W12, `Dobranoc. Skargi przyjmę we śnie.`
  rysujDobranoc(p, dn);
  zapisz(p, "mak-noc-a3");
  dn.maskaOd = -1;
  rysujDobranoc(p, dn);
  zapisz(p, "mak-noc-a3-bez-godziny");

  {  // U-05 z wartościami kanwy (design/kanwa/U05-test-sprzetu.dc.html), do porównania z design/png/U05-test-sprzetu.png
    DaneTestu u;
    u.lewa = {{"Oprogramowanie", "1.0.1 (app1)"}, {"Zestaw reguł", "2026-12-01"}, {"Pamięć panelu", "wolne 18 MB"},
              {"Karta", "kopia 03.11 05:40"}, {"Źródło otoczenia", "OK 05:31"}};
    u.prawa = {{"Panel", "192.168.30.51"}, {"Wi-Fi", "Dom −61 dBm"}, {"AirBridge", "192.168.20.205 · v2.3.0"},
               {"Ostatnie pobranie", "03.11 05:32"}, {"Bateria", "64%"}};
    u.ramkaLewo = "Wgrywanie przez Wi-Fi do 10:14";
    u.ramkaPrawo = "hasła i dane terapii nie są pokazywane";
    rysujTest(p, u);
    zapisz(p, "mak-test");
  }

  // karty: zmieniają się tylko kroje (SPEC-KOKPIT 7); sceny jak w podglądzie
  TrescEkranu k;
  k.etykietaKarty = "OSTATNIA NOC";
  k.tytul = "Noc: 12 h 59 z maską.";  // czas w tytule (R-NF-23), w faktach bez powtórzenia
  k.powod = "Tydzień: inaczej niż zwykle.";
  k.fakty = {{"12,4", "AHI"}, {"112", "wyciek l/min"}};
  k.stopkaPrawo = "przy łóżku 21° · 46%";
  rysujKarte(p, {"cz 12.11", "dane 05:46"}, k, scenaK22);
  zapisz(p, "mak-karta-k22");
  {  // ta sama karta z ikoną aktualizacji w stopce (R-EKR-61)
    IkonaStopki ik(true);
    rysujKarte(p, {"cz 12.11", "dane 05:46"}, k, scenaK22);
    ikonaAktualizacji(p, false);
    zapisz(p, "mak-karta-k22-ikona-aktualizacji");
  }
  k = TrescEkranu();
  k.etykieta = "BRAK DANYCH Z NOCY";
  k.tytul = "Brak danych z nocy 12.11";
  k.powod = "AirBridge nie odpowiada od 11.11 05:42. Ponawiam co\xC2\xA0" "30\xC2\xA0" "min.";
  k.krok = "→ Sprawdź zasilanie AirBridge.";
  k.stopkaPrawo = "↻ ponów";
  rysujKarte(p, {"cz 12.11", "dane z 11.11"}, k, scenaS5);
  zapisz(p, "mak-karta-s5");
}

// ---------- slot dzienny S-K-33…S-K-39, obrazek dnia, S-K-01, S-K-02, S-K-13 (dane syntetyczne) ----------
static Prognoza prognozaB9(const std::string& d, float t, float pm) {
  Prognoza p;
  p.data = d;
  p.pobrano = 1;
  p.godzinaPobrania = "07:55";
  for (auto& g : p.h) { g.temp = t; g.pm25 = pm; g.opad = 0; g.chmury = 30; g.porywy = 20; }
  return p;
}
static void slotB9(PlotnoPamiec& p, const char* plik, const WejscieSlotu& we, const Noc& noc, float zaOknem,
                   int drzemka = 0) {
  WyborSlotu w = wybierzSlot(we);
  TekstySlotu t = tekstySlotu(w, we);
  Naglowek n{naglowekSlotu(we.dzis), we.prognoza && w.ekran != EkranSlotu::SK35 && w.ekran != EkranSlotu::SK37
                                         ? "prognoza " + we.prognoza->godzinaPobrania : ""};
  if (w.ekran == EkranSlotu::SK39) n.prawo = "drzemka 15:10–16:20";
  StopkaDzienna s = stopkaDzienna(&noc, true, drzemka, zaOknem, we.przyLozku);
  rysujSlotDzienny(p, n, w, t, we, s);
  printf("  [%s] %s\n", idEkranuSlotu(w.ekran), plik);
  zapisz(p, plik);
}

// ---------- pozostałe S-K (wieczór, rano, tydzień, poniedziałek), S-K-30, S-K-31, „Sprzęt”, pytanie o poduszkę,
// `Nowy model maski zapisany.` (dane syntetyczne, kanwa design/png: SKnn, Sprzet, Q-poduszka, P-nowy-model) ----------
static Noc nocB16(const std::string& data, float wp95 = 10) {
  Noc n;
  n.data = data;
  n.wazna = true;
  n.m01 = 420;
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
static NormaMetryki nmB16(double med, double p10, double p90) {
  NormaMetryki x;
  x.n = 30;
  x.med = med;
  x.p10 = p10;
  x.p90 = p90;
  return x;
}
static void skB16(PlotnoPamiec& p, const std::vector<sk::Tresc>& v, const char* id, const char* plik,
                  const Naglowek& n, const std::string& stL, const std::string& stP) {
  for (auto& t : v)
    if (t.id == id) {
      sk::rysuj(p, n, t, stL, stP);
      zapisz(p, plik);
      return;
    }
  printf("podglad: brak %s (%s)\n", id, plik);
}
static void podgladB16(PlotnoPamiec& p) {
  Norma nm;
  nm.m01 = nmB16(420, 360, 470);
  nm.wp95 = nmB16(10, 6, 14);
  nm.cp95 = nmB16(10, 9, 11);
  nm.ep95 = nmB16(8, 7, 9);
  nm.m13 = nmB16(14, 13, 15);
  nm.m17 = nmB16(1, 0, 2);
  std::string koniec = "2026-11-04";
  std::vector<Noc> h;
  for (int i = 29; i >= 0; i--) h.push_back(nocB16(dodajDni(koniec, -i)));
  std::vector<WpisHumoru> dz;
  sk::Wejscie w;
  w.dzis = koniec;
  w.hist = &h;
  w.norma = &nm;
  w.trybProbny = false;
  w.dziennik = &dz;
  Naglowek wiecz{"wt 03.11 · wieczór", "sypialnia 21:30"};
  w.tempTeraz = 15;
  skB16(p, sk::wieczor(w), "S-K-03", "b16-sk03-chlod", wiecz, "Katowice 2° · bezchmurnie", stopkaCzujnika(15, 52));
  w.tempTeraz = 21;
  w.wilgTeraz = 30;
  skB16(p, sk::wieczor(w), "S-K-04", "b16-sk04-sucho", wiecz, "Katowice 1° · mróz", stopkaCzujnika(21, 30));
  w.wilgTeraz = 45;
  w.tempTeraz = 27;
  skB16(p, sk::wieczor(w), "S-K-05", "b16-sk05-upal", wiecz, "Katowice 24° · bezchmurnie", stopkaCzujnika(27, 45));
  w.tempTeraz = 21;
  Naglowek rano{"śr 04.11", "dane 05:44"};
  {  // S-K-10: wyciek ostatniej nocy ponad próg (ton „problem”)
    h.back().wp95 = 26;
    skB16(p, sk::rano(w), "S-K-10", "b16-sk10-przeciek", rano, "Katowice 3° · mgła", stopkaCzujnika(21, 44));
    // ten sam ekran w negatywie z linią uwagi (ostrzeżenie: biały pasek z `!`) i z ikoną aktualizacji w stopce
    for (auto& t : sk::rano(w))
      if (t.id == "S-K-10") {
        sk::Tresc tl = t;
        tl.t.linia = "Noce mało czytelne: bezdechów nie oceniam.";
        tl.t.liniaOstrzezenie = true;
        IkonaStopki ik(true);
        sk::rysuj(p, rano, tl, "Katowice 3° · mgła", stopkaCzujnika(21, 44));
        ikonaAktualizacji(p, true);
        zapisz(p, "b16-sk10-przeciek-linia-ikona");
      }
    h.back().wp95 = 10;
  }
  {  // S-K-10 wariant opisowy (R-KONF-42): P95 18 przy ustawieniu 15, ton „informacja”, ekran danych
    h.back().wp95 = 18;
    skB16(p, sk::rano(w), "S-K-10", "b16-sk10-widoczny", rano, "Katowice 3° · mgła", stopkaCzujnika(21, 44));
    h.back().wp95 = 10;
  }
  {  // S-K-30: oddechy szybciej w 2 nocach, ciśnienie też wyższe; noce wcześniej wokół mediany (linia
     // przerywana normy odróżnia się od danych, jak na kanwie SK30-oddech)
    const float zm[7] = {13.2f, 14.6f, 13.6f, 14.9f, 13.0f, 14.3f, 13.8f};
    for (size_t i = 0; i + 2 < h.size(); i++) h[i].m13 = zm[i % 7];
    h[28].m13 = 16.6f;
    h[29].m13 = 17.4f;
    h[29].cp95 = 12;
    std::vector<sk::Tresc> v = sk::rano(w);
    skB16(p, v, "S-K-30", "b16-sk30-oddech", rano, "", "");
    for (auto& x : h) x.m13 = 14;
    h[29].cp95 = 10;
  }
  {  // S-K-40 i S-K-43 (ADR-0039): stan odchylenia PS w górę i objętości oddechowej w dół, linia normy, ton „informacja”
    std::vector<Noc> ho;
    for (int i = 34; i >= 0; i--) {
      Noc x = nocB16(dodajDni(koniec, -i));
      x.ustawienia = "ASVAuto/6/10/3/15";
      x.tryb = "ASVAuto";
      int k = (34 - i) % 5;
      x.psmed = i < 5 ? 8.4f - 0.1f * (float)k : 6.0f + 0.1f * (float)k;
      x.m14 = 460.0f + 5.0f * (float)k;
      ho.push_back(x);
    }
    Stany so = liczStany(ho, koniec, {});
    sk::Wejscie wo = w;
    wo.hist = &ho;
    wo.norma = &so.norma;
    wo.stany = &so;
    wo.tryb = reguly::Tryb::ASVAUTO;
    skB16(p, sk::rano(wo), "S-K-40", "b16-sk40-ps", rano, "Katowice 3° · mgła", stopkaCzujnika(21, 44));
    for (int i = 30; i < 35; i++) ho[(size_t)i].psmed = 6.2f;
    for (int i = 30; i < 35; i++) ho[(size_t)i].m14 = 385.0f + 3.0f * (float)i - 90.0f;
    so = liczStany(ho, koniec, {});
    wo.norma = &so.norma;
    wo.stany = &so;
    skB16(p, sk::rano(wo), "S-K-43", "b16-sk43-objetosc", rano, "Katowice 3° · mgła", stopkaCzujnika(21, 44));
  }
  {  // S-K-31: maska po terminie, wyciek rośnie od 4 tygodni
    std::vector<Noc> h31;
    for (int i = 40; i >= 0; i--) {
      Noc x = nocB16(dodajDni(koniec, -i));
      x.wp95 = i >= 21 ? 10.0f : 10.0f + (float)(21 - i);
      h31.push_back(x);
    }
    ElementWymiany el[EL_N];
    el[EL_MASKA].wymieniono = "2026-03-01";
    el[EL_MASKA].okres = 6;
    sprzet::Elementy e = sprzet::zOdpowiedziami(el, h31, {});
    sk::Wejscie w31 = w;
    w31.hist = &h31;
    w31.sprzet = &e;
    skB16(p, sk::rano(w31), "S-K-31", "b16-sk31-poduszka", rano, "◀ 2 s: wymienione dziś", "▶ kokpit");
  }
  {  // S-K-18 w niedzielę (T2)
    sk::Wejscie wt = w;
    wt.dzis = "2026-11-01";
    std::vector<Noc> ht(h.begin(), h.begin() + 27);
    wt.hist = &ht;
    wt.uspokojenie = true;
    sk::Tresc t;
    if (sk::tydzien(wt, t)) {
      sk::rysuj(p, Naglowek{"nd 01.11 · tydzień", "dane 17:05"}, t, "", "");
      zapisz(p, "b16-sk18-tydzien");
    } else {
      printf("podglad: brak S-K-18\n");
    }
  }
  {  // S-K-21 wariant weekend w poniedziałek (T3, R8)
    std::vector<Noc> h3;
    for (int i = 59; i >= 0; i--) {
      Noc x = nocB16(dodajDni("2026-11-02", -i));
      int dt = dzienTygodnia(x.data);
      if (dt == 6 || dt == 0) x.m01 = 360;
      h3.push_back(x);
    }
    sk::Wejscie w3 = w;
    w3.dzis = "2026-11-02";
    w3.hist = &h3;
    skB16(p, sk::rano(w3), "S-K-21", "b16-sk21-weekend", Naglowek{"pn 02.11", "dane 05:44"}, "", "");
  }
  {  // pytanie `Wymieniona poduszka?` (R-MAS-12, artboard Q-poduszka)
    DaneDnia dd;
    dd.dzis = koniec;
    DanePytaniaTN q = trescPytaniaTN(sprawy::Pytanie::PODUSZKA, dd, 11 * 60, 0);
    q.opis = sk::tekst("R-MAS-12.opis", {{"y", "26"}, {"x", "15"}, {"data", "01.11"}});
    rysujPytanieTN(p, q);
    zapisz(p, "b16-pytanie-poduszka");
  }
  {  // `Wymiana zapisana.` (R-MAS-01, R-MAS-18; od ADR-0041 jedyny ekran zapisu wymiany, artboard P-nowy-model)
    DanePotwierdzenia d;
    d.naglowek = {"Sprzęt", ""};
    d.tytul = sk::tekst("R-MAS-01.tytul");
    d.powod = sk::tekst("R-MAS-01.filtr");
    d.stopka = {"◀ 2 s cofnij"};
    rysujPotwierdzenie(p, d);
    zapisz(p, "b16-wymiana-zapisana");
    d.stopka.clear();
    rysujPotwierdzenie(p, d);
    zapisz(p, "b16-wymiana-zapisana-noc");
  }
}
static void podgladB9(PlotnoPamiec& p) {
  Noc noc;
  noc.data = "2026-11-03";
  noc.m01 = 7 * 60 + 52;
  noc.ahi = 1.2f;
  noc.wp95 = 4;
  auto we = [](const std::string& d, int minuta, const Prognoza* pr, float przy) {
    WejscieSlotu w;
    w.dzis = d;
    w.minuta = minuta;
    w.prognoza = pr;
    w.przyLozku = przy;
    w.wspolrzedne = true;
    w.lat = 50.2597;
    w.lon = 19.0217;
    w.obrazekJest = true;
    return w;
  };
  const std::string d = "2026-11-03";
  // S-K-33 zimą: okno 11:00–14:00, od 15:00 smog (kanwa SK33-okno)
  Prognoza z = prognozaB9(d, 2, 22);
  const float pmZ[24] = {40, 42, 45, 44, 40, 38, 36, 30, 26, 22, 18, 12, 13, 14, 18, 40, 52, 63, 68, 66, 58, 54, 48, 44};
  const float tZ[24] = {1, 1, 0, 0, 0, 0, 1, 1, 2, 3, 4, 5, 6, 6, 5, 4, 3, 3, 2, 2, 2, 1, 1, 1};
  for (int h = 0; h < 24; h++) { z.h[h].pm25 = pmZ[h]; z.h[h].temp = tZ[h]; }
  slotB9(p, "sk33-okno", we(d, 11 * 60 + 5, &z, 21), noc, 6);
  slotB9(p, "sk33-okno-pozniej", we(d, 8 * 60 + 5, &z, 21), noc, 2);
  {  // wariant CO2
    Prognoza c = prognozaB9(d, 10, 14);
    WejscieSlotu w = we(d, 14 * 60 + 2, &c, 21);
    w.co2 = 1150;
    slotB9(p, "sk33-okno-co2", w, noc, 10);
  }
  {  // smog cały dzień
    Prognoza s = prognozaB9(d, 1, 70);
    for (int h = 0; h < 24; h++) s.h[h].pm25 = (float)(60 + (h * 7) % 30);
    slotB9(p, "sk33-okno-smog", we(d, 9 * 60 + 5, &s, 21), noc, 1);
  }
  {  // upał 17.07 (kanwa SK33-okno-upal): przy łóżku 26°, za oknem 31° do 19:00, chłodniej od 20:00
    Prognoza u = prognozaB9("2026-07-17", 31, 10);
    const float tU[24] = {20, 19, 19, 18, 18, 19, 21, 24, 26, 28, 30, 31, 31, 32, 31, 30, 29, 28, 27, 25, 23.5f, 23, 22, 21};
    for (int h = 0; h < 24; h++) u.h[h].temp = tU[h];
    u.godzinaPobrania = "06:10";
    Noc nl = noc;
    nl.data = "2026-07-17";
    slotB9(p, "sk33-okno-upal", we("2026-07-17", 12 * 60 + 5, &u, 26), nl, 31);
  }
  {  // S-K-34 jutro rano mróz
    Prognoza j = prognozaB9(d, 8, 60);
    for (int h = 24 + 6; h < 24 + 9; h++) { j.h[h].temp = h == 31 ? -4.f : -3.f; j.h[h].opad = 10; j.h[h].porywy = 20; }
    j.godzinaPobrania = "16:00";
    slotB9(p, "sk34-jutro", we(d, 16 * 60 + 5, &j, 21), noc, 3);
  }
  {  // S-K-35 filtr
    static ElementWymiany el[EL_N];
    el[EL_MASKA] = {"2026-07-01", 6, false};
    el[EL_FILTR] = {"2026-10-08", 4, true};
    WejscieSlotu w = we(d, 12 * 60, nullptr, 21);
    w.sprzet = &el;
    slotB9(p, "sk35-wymiana", w, noc, 6);
  }
  {  // S-K-36 pyłek brzozy 13.04
    Prognoza py = prognozaB9("2026-04-13", 14, 10);
    const float b[24] = {10, 10, 10, 10, 10, 10, 20, 40, 60, 70, 60, 90, 180, 200, 205, 190, 100, 80, 65, 55, 45, 35, 30, 25};
    for (int h = 0; h < 24; h++) py.h[h].pylek[PYL_BRZOZA] = b[h];
    py.godzinaPobrania = "07:20";
    WejscieSlotu w = we("2026-04-13", 10 * 60 + 5, &py, 21);
    w.pylki.zaznaczony[PYL_BRZOZA] = true;
    w.pylki.prog[PYL_BRZOZA] = 150;
    Noc nw = noc;
    nw.data = "2026-04-13";
    slotB9(p, "sk36-pylek", w, nw, 14);
  }
  slotB9(p, "sk37-slonce", we(d, 12 * 60, nullptr, 21), noc, 6);  // n = 20760, parzyste
  {  // S-K-38: obrazek syntetyczny 434×434 (koła i paski), bez prawdziwych obrazków dnia
    ObrazPbm o;
    o.w = 434;
    o.h = 434;
    int wb = (o.w + 7) / 8;
    o.bity.assign((size_t)wb * (size_t)o.h, 0);
    for (int y = 0; y < o.h; y++)
      for (int x = 0; x < o.w; x++) {
        int dx = x - 217, dy = y - 200, r2 = dx * dx + dy * dy;
        bool cz = (r2 < 150 * 150 && r2 > 140 * 140) || (r2 < 40 * 40) || (y > 380 && y < 392) ||
                  (x < 6 || x > 427 || y < 6 || y > 427) || (std::abs(dx) < 4 && dy > 40 && dy < 140);
        if (cz) o.bity[(size_t)y * (size_t)wb + (size_t)(x / 8)] |= (uint8_t)(0x80 >> (x % 8));
      }
    StopkaDzienna s = stopkaDzienna(&noc, true, 0, 6, 21);
    rysujObrazekDnia(p, o, s);
    zapisz(p, "sk38-obrazek");
  }
  {  // S-K-39 po drzemce
    Prognoza c = prognozaB9(d, 10, 14);
    WejscieSlotu w = we(d, 17 * 60, &c, 21);
    w.drzemka.minut = 70;
    w.drzemka.wyciekP95 = 31;
    slotB9(p, "sk39-drzemka-wyciek", w, noc, NAN, 70);
  }
  // S-K-01 wieczorem: ekran danych (zeszła noc 5 h ponad progiem) i karta K05 (scena W01)
  std::vector<ProbkaCo2> pr;
  const int co2[15] = {620, 700, 820, 940, 1050, 1160, 1260, 1340, 1420, 1480, 1520, 1550, 1560, 1540, 1500};
  for (int i = 0; i < 15; i++) pr.push_back({1793660000u + (uint32_t)i * 1800u, co2[i]});
  Co2Nocy n = statystykaCo2(pr);
  std::vector<WpisHumoru> dz;
  WejscieWieczoru ww;
  ww.dzis = "2026-10-21";
  ww.noc = &n;
  ww.co2Teraz = 1240;
  ww.pm25ZaOknem = 9;
  ww.dziennik = &dz;
  WynikWieczoru rw = ocenWieczor(ww);
  TrescKontekstu k = trescSK01(ww, rw);
  Naglowek nw{naglowekSlotu(ww.dzis) + " · wieczór", "sypialnia 21:30 · CO2 1240"};
  rysujEkranKontekstu(p, nw, k, "Katowice 7° · zachmurzenie", stopkaCzujnika(21, 44, 1240));
  zapisz(p, "sk01-przewietrz");
  {
    TrescEkranu t = k.t;
    t.tytul += ".";  // na karcie tytuł z kropką (5.7.2), etykieta wersalikami (K05)
    t.etykieta = wersaliki(t.etykieta);
    t.stopkaPrawo = stopkaCzujnika(21, 44);
    Naglowek nk{naglowekSlotu(ww.dzis) + " · wieczór", "sypialnia 21:40"};
    rysujKarte(p, nk, t, "W01");
    zapisz(p, "sk01-przewietrz-karta");
  }
  {  // wariant z odczytu bieżącego, bez serii nocy
    WejscieWieczoru w2 = ww;
    w2.noc = nullptr;
    WynikWieczoru r2 = ocenWieczor(w2);
    rysujEkranKontekstu(p, nw, trescSK01(w2, r2), "Katowice 7° · zachmurzenie", stopkaCzujnika(21, 44, 1240));
    zapisz(p, "sk01-przewietrz-teraz");
  }
  {  // S-K-02 (wymaga PM2.5 sypialni: tu syntetyczne, w kontrakcie wersji 1 go nie ma)
    WejscieWieczoru w2 = ww;
    w2.pm25ZaOknem = 48;
    w2.pm25Sypialni = 11;
    rysujEkranKontekstu(p, nw, trescSK02(w2), "Katowice 3° · mgła", stopkaCzujnika(21, 44, 1240));
    zapisz(p, "sk02-smog");
  }
  {  // S-K-13 rano
    std::vector<ProbkaCo2> r13;
    const int c13[14] = {700, 850, 1050, 1250, 1420, 1580, 1720, 1850, 1950, 2030, 2080, 2100, 2090, 2060};
    for (int i = 0; i < 14; i++) r13.push_back({1793660000u + (uint32_t)i * 1800u, c13[i]});
    Co2Nocy n13 = statystykaCo2(r13);
    Naglowek nr{"nd 15.11", "dane 05:55"};
    rysujEkranKontekstu(p, nr, trescSK13(n13), "Katowice 2° · mgła", stopkaCzujnika(22, 45));
    zapisz(p, "sk13-zaduch");
  }
}


// ---------- S7, S8, „Co znaczą te liczby”, odkrycia S1, pytanie o wyjazd (dane syntetyczne) ----------
static void podgladB15(PlotnoPamiec& p) {
  odkrycia::zarejestrujRodzajeO();
  Naglowek nag{"wt 01.12", "dane 05:49"};
  std::vector<Noc> h;
  std::string d = "2026-10-01";
  for (int i = 0; i < 61; i++, d = dodajDni(d, 1)) {
    Noc n;
    n.data = d;
    n.m01 = i % 9 == 0 ? 200 : 412;
    n.wazna = n.m01 >= 240;
    n.ahi = i < 31 ? 1.2f : 1.0f;
    n.wp95 = i < 31 ? 14.0f : 2.0f;
    n.ustawienia = "ASVAuto/6/10/3/15";
    h.push_back(n);
  }
  podsumowanie::DaneS7 s7 = podsumowanie::budujS7("2026-12-01", h, reguly::Tryb::ASVAUTO, false);
  TrescEkranu t = podsumowanie::trescS7(s7);
  t.stopkaLewo = "Katowice 0° · szron";
  t.stopkaPrawo = "przy łóżku 20° · 40% · CO2 850";
  podsumowanie::rysujS7(p, nag, s7, t);
  zapisz(p, "b15-s7-miesiac");
  Norma n;
  n.ahi.n = n.wp95.n = n.m01.n = n.ca.n = 30;
  n.ahi.p10 = 0.8; n.ahi.p90 = 2.1; n.wp95.p10 = 6; n.wp95.p90 = 12; n.m01.p10 = 372; n.m01.p90 = 456;
  n.ca.p10 = 0.2; n.ca.p90 = 0.9;
  podsumowanie::WejscieS8 w;
  w.norma = &n;
  w.tryb = reguly::Tryb::ASVAUTO;
  Naglowek n8{"wt 03.11", "dane 05:44"};  // S8 tylko jako ekran dnia (kamień milowy R7, ADR-0041)
  podsumowanie::DaneS8 s8 = podsumowanie::budujS8(w);
  podsumowanie::rysujS8(p, n8, s8, podsumowanie::trescS8(s8));
  zapisz(p, "b15-s8-norma");
  n.ahi.p10 = 1.2; n.ahi.p90 = 5.4;
  s8 = podsumowanie::budujS8(w);
  t = podsumowanie::trescS8(s8);
  t.stopkaLewo = "Katowice 4° · pochmurno";
  t.stopkaPrawo = "przy łóżku 21° · 45%";
  podsumowanie::rysujS8(p, Naglowek{"wt 03.11", "dane 05:44"}, s8, t);  // S8 jako ekran dnia: data i tytuł
  zapisz(p, "b15-s8-informacja");
  n.ahi.n = 0;
  n.ca.n = 0;
  w.profilPubliczny = true;
  s8 = podsumowanie::budujS8(w);
  podsumowanie::rysujS8(p, n8, s8, podsumowanie::trescS8(s8));
  zapisz(p, "b15-s8-publiczny-uczesie");
  podsumowanie::rysujLiczby(p, Naglowek{"Co znaczą te liczby", ""}, podsumowanie::budujLiczby(reguly::Tryb::AUTOSET, true, true));
  zapisz(p, "b15-co-znacza-liczby");
  podsumowanie::rysujLiczby(p, Naglowek{"Co znaczą te liczby", ""}, podsumowanie::budujLiczby(reguly::Tryb::ASV, true));
  zapisz(p, "b15-co-znacza-liczby-asv");
  // odkrycia: O-12 (alkohol) z archiwum z wagami R-CZY-24, O-08 (weekend), O-03 (linia 30 nocy)
  std::vector<Noc> ho;
  std::vector<WpisCzynnikow> cz;
  d = "2026-05-01";
  auto dodaj = [&](int ile, char war, uint8_t c, float ahi) {
    for (int i = 0; i < ile; i++, d = dodajDni(d, 1)) {
      Noc x;
      x.data = d;
      x.m01 = 420;
      x.wazna = true;
      x.ahi = ahi;
      x.wp95 = 8;
      x.ustawienia = "ASVAuto/6/10/3/15";
      ho.push_back(x);
      WpisCzynnikow e;
      e.data = d;
      e.stan = 'O';
      e.czynniki = c;
      e.warstwa = war;
      e.pokazano = true;
      cz.push_back(e);
    }
  };
  dodaj(4, 'O', CZ_ALKOHOL, 4.9f);
  dodaj(5, 'K', CZ_ALKOHOL, 4.9f);
  dodaj(10, 'O', CZ_NIC, 1.8f);
  dodaj(31, 'K', CZ_NIC, 1.8f);
  dodaj(1, 'K', CZ_ALKOHOL, 4.9f);
  dodaj(1, 'O', CZ_ALKOHOL, 4.9f);
  reguly::Kontekst k;
  k.historia = &ho;
  k.dzis = ho.back().data;
  k.trybBiezacy = reguly::Tryb::ASVAUTO;
  k.zestaw = &reguly::zestawWbudowany();
  odkrycia::CechyNocy c;
  c.tryb = odkrycia::TrybOceny::REWALIDACJA;
  c.czynniki = &cz;
  std::vector<reguly::Wynik> wy = odkrycia::ocenOdkrycia(reguly::zestawWbudowany(), k, c);
  Naglowek no{"pn 15.02", "dane 05:51"};
  for (auto& x : wy) {
    if (odkrycia::klucz(x) != "O-12/A.M-02" || !x.spelniona) continue;
    odkrycia::UstawieniaTekstow u;
    u.czynnikiWrazliwe = true;
    odkrycia::TrescOdkrycia o = odkrycia::trescOdkrycia(reguly::zestawWbudowany(), x, reguly::Tryb::ASVAUTO, u);
    TrescEkranu ts = podsumowanie::trescS1(o);
    ts.stopkaLewo = "Katowice 2° · mgła";
    ts.stopkaPrawo = "przy łóżku 20° · 44%";
    podsumowanie::rysujOdkrycie(p, no, o, ts);
    zapisz(p, "b15-s1-o12-alkohol");
    u.czynnikiWrazliwe = false;
    o = odkrycia::trescOdkrycia(reguly::zestawWbudowany(), x, reguly::Tryb::ASVAUTO, u);
    ts = podsumowanie::trescS1(o);
    ts.stopkaLewo = "Katowice 2° · mgła";
    ts.stopkaPrawo = "przy łóżku 20° · 44%";
    podsumowanie::rysujOdkrycie(p, no, o, ts);
    zapisz(p, "b15-s1-o12-ukryty");
    {  // K1 otwarty ▶ z ekranu O-12 z ukrytym czynnikiem: pełny tytuł i powód pod nagłówkiem (5.5.2, R-CZY-20)
      std::string dz = ho.back().data;
      Stany st = stanyZPamieci(ho, dz, wykluczoneZNormy(cz));
      DaneK1 k1 = budujK1(ho, dz, st.norma, cz, &st);
      k1.odkrycieTytul = o.tytulK1;
      k1.odkryciePowod = o.powodK1;
      k1.w[0].stan = true;  // najciaśniejszy wiersz: ramka i ocena w dwóch liniach
      k1.w[0].slowo = "wyżej niż zwykle";
      k1.w[0].slowo2 = "od 3 nocy";
      rysujK1(p, k1);
      zapisz(p, "b15-k1-odkrycie-czynnik");
      k1.odkryciePowod = "Centralne 0,9 / h po lekach nasennych, 0,4 / h bez. Z 9/41 nocy. Może to przypadek.";
      rysujK1(p, k1);
      zapisz(p, "b15-k1-odkrycie-czynnik-dlugi");
      // K1 z odkryciem w ASV z 5 wierszami i krótkim powodem (wiersz 45 px, ocena
      // w jednej linii P22B) oraz prawdziwy K1 trybu AutoSet (tryb egzemplarza testowego) z tych samych nocy: 4 wiersze
      // (AHI, czas z maską, ciśnienie, wyciek; R-KOK-15), też z oceną w jednej linii
      k1.odkryciePowod = o.powodK1;
      rysujK1(p, k1);
      zapisz(p, "b17-k1-odkrycie-asv-5-wierszy");
      std::vector<Noc> ha = ho;
      for (auto& na : ha) {
        na.ustawienia = "AutoSet/5/12";
        na.tryb = "AutoSet";
        na.cmed = 9.0f + 0.1f * (float)(na.data.back() - '0');  // ciśnienie z AutoSet (syntetyczne)
        na.cp95 = na.cmed + 1.5f;
      }
      Stany sa = stanyZPamieci(ha, dz, wykluczoneZNormy(cz));
      DaneK1 ka = budujK1(ha, dz, sa.norma, cz, &sa);
      ka.odkrycieTytul = o.tytulK1;
      ka.odkryciePowod = o.powodK1;
      ka.w[0].stan = true;
      ka.w[0].slowo = "wyżej niż zwykle";
      ka.w[0].slowo2 = "od 3 nocy";
      rysujK1(p, ka);
      zapisz(p, "b17-k1-odkrycie-4-wiersze");  // AutoSet
    }
    rysujKarte(p, no, ts, scenaJest("O01") ? "O01" : "");
    zapisz(p, "b15-s1-o12-karta");
  }
  std::vector<Noc> hw;
  d = "2026-08-01";
  for (int i = 0; i < 90; i++, d = dodajDni(d, 1)) {
    Noc x;
    x.data = d;
    x.m01 = 420;
    x.wazna = true;
    int dt = dzienTygodnia(d);
    x.ahi = i >= 83 ? 0.6f : (dt == 0 || dt == 6 ? 4.8f : 1.2f);
    x.wp95 = 8;
    x.ustawienia = "ASVAuto/6/10/3/15";
    hw.push_back(x);
  }
  k.historia = &hw;
  k.dzis = hw.back().data;
  c.tryb = odkrycia::TrybOceny::PRZEGLAD;
  wy = odkrycia::ocenOdkrycia(reguly::zestawWbudowany(), k, c);
  for (auto& x : wy) {
    std::string kl = odkrycia::klucz(x);
    if (!x.spelniona || (kl != "O-08/M-02" && kl != "O-03/M-02")) continue;
    odkrycia::TrescOdkrycia o = odkrycia::trescOdkrycia(reguly::zestawWbudowany(), x, reguly::Tryb::ASVAUTO, {}, &hw);
    TrescEkranu ts = podsumowanie::trescS1(o);
    ts.stopkaLewo = "Katowice 9° · deszcz";
    ts.stopkaPrawo = "przy łóżku 21° · 46%";
    podsumowanie::rysujOdkrycie(p, no, o, ts);
    zapisz(p, kl == "O-08/M-02" ? "b15-s1-o08-weekend" : "b15-s1-o03-rekord");
  }
  // pytanie o wyjazd R-CZY-22 (układ pytania tak/nie 5.12.8)
  std::vector<WpisCzynnikow> v;
  oznaczPozaDomem(v, {"2026-10-22", "2026-10-23", "2026-10-24", "2026-10-25"}, "2026-10-21", 360, "2026-10-25", 840);
  TekstyWyjazdu tw = tekstyWyjazdu(okresyWyjazdu(v)[0], 11 * 60);
  DanePytaniaTN q;
  q.naglowek = {"pn 26.10", "dane 05:42"};
  q.etykieta = tw.etykieta;
  q.tytul = tw.tytul;
  q.opis = tw.opis;
  q.stopka = tw.stopka;
  rysujPytanieTN(p, q);
  zapisz(p, "b15-pytanie-wyjazd");
}

// Portret nocy S11 (5.4.16, ADR-0040): pełny ekran (nagłówek, tarcza, kolumna, stopka) na
// przebiegach zbudowanych w kodzie (bez plików zewnętrznych): AutoSet z pikami, centralnymi i wyciekiem; ASVAuto
// z kropkami i wsparciem; noc niepewna; noc spokojna; wariant z linią uwagi (z podpisem zbiorczym braków); strona
// pod ▶ ze stopką `◀ ekran dnia   ▶ ostatnia noc   ↻ ekran dnia` (R-POR-24); start okna na pierścieniu (znacznik startu
// okna przez czarny łuk maski, R-POR-06, od 12.11.2026) i wyciek na starcie okna.
static portret::Odcinek odcinekS11(int od) {
  portret::Odcinek o;
  o.od = od;
  o.minuty = 5;
  o.wyciek95 = 6;
  o.wycS = 0;
  o.zew = 0;
  o.centr = 0;
  return o;
}
// noc 23:10–06:55 z przerwą 03:05–03:20 (minuty od 12:00); `asv`: PS w odcinkach i kropki zdarzeń
static portret::Przebieg przebiegS11(bool asv, bool wyciek, bool zdarzenia) {
  portret::Przebieg pr;
  pr.data = "2026-11-12";
  pr.sesje = {{670, 905}, {920, 1135}};
  for (int m = 670; m < 1135; m += 5) {
    if (m >= 905 && m < 920) continue;
    portret::Odcinek o = odcinekS11(m);
    if (asv) o.ps = 50;
    pr.odcinki.push_back(o);
  }
  auto odc = [&](int m) -> portret::Odcinek& {
    for (auto& o : pr.odcinki)
      if (o.od == m) return o;
    return pr.odcinki.front();
  };
  if (wyciek)  // 04:00–04:20 białe (300 s), 05:40 rozproszony 20 s (bez białego)
    for (int m = 960; m < 980; m += 5) odc(m).wycS = 300, odc(m).wyciek95 = 22;
  if (wyciek) odc(1060).wycS = 20;
  if (zdarzenia && !asv) {
    const int zew[][2] = {{780, 1}, {840, 3}, {845, 7}, {850, 5}, {855, 2}, {1010, 4}, {1015, 2}, {1090, 1}};
    for (auto& z : zew) odc(z[0]).zew = z[1];
    odc(870).centr = 2;
    odc(875).centr = 1;
    odc(1020).centr = 3;
  }
  if (asv) {
    odc(840).ps = 70;
    odc(845).ps = 110;
    odc(850).ps = 90;
    odc(1000).ps = 75;
    for (int m : {700, 842, 846, 851, 930, 1003, 1080}) pr.zdarzenia.push_back({m, true, false});
  }
  return pr;
}
static void portretS11(PlotnoPamiec& p, const char* plik, const DanePortretu& d, const std::string& linia,
                       const std::string& prawo = "dane 07:12") {
  TrescEkranu t;
  t.uklad = Uklad::PORTRET;
  t.portret = &d;
  t.linia = linia;
  t.stopkaStandardowa = true;
  t.stopkaLewo = "Katowice 8° · pochmurno";
  t.stopkaPrawo = "przy łóżku 21° · 45%";
  rysujEkranDanych(p, Naglowek{"cz 12.11", prawo}, t);
  zapisz(p, plik);
}
static void podgladPortretu(PlotnoPamiec& p) {
  DanePortretu d;
  d.tryb = "AutoSet";
  d.zwykla = 11 * 60 + 30;  // 23:30, okno 21:30–09:30
  d.czas = "7 h 30";
  d.ahi = "3,4";
  d.pierwszy = "Tydzień: AHI wyżej niż zwykle.";
  portret::Przebieg autoset = przebiegS11(false, true, true);
  d.przebieg = &autoset;
  portretS11(p, "s11-autoset-piki-centralne-wyciek", d, "");
  portret::Przebieg asv = przebiegS11(true, true, false);
  DanePortretu a = d;
  a.tryb = "ASVAuto";
  a.przebieg = &asv;
  a.normaPs10 = 50;
  a.ahi = "1,1";
  a.pierwszy = "Noc w normie. Tydzień jak zwykle.";
  portretS11(p, "s11-asvauto-kropki-wsparcie", a, "");
  DanePortretu n = d;  // noc niepewna: sam czas z maską, jeden komunikat, nagłówek `· ocena ograniczona`
  n.niepewna = true;
  portretS11(p, "s11-noc-niepewna", n, "", "dane 07:12 · ocena ograniczona");
  portret::Przebieg spok = przebiegS11(false, false, false);
  spok.sesje = {{670, 1135}};
  spok.odcinki.clear();
  for (int m = 670; m < 1135; m += 5) spok.odcinki.push_back(odcinekS11(m));
  DanePortretu c = d;
  c.przebieg = &spok;
  c.ahi = "0,4";
  c.pierwszy = "Noc w normie. Tydzień jak zwykle.";
  portretS11(p, "s11-noc-spokojna", c, "");
  DanePortretu l = d;  // linia uwagi: tarcza 0,85, 1 podpis po pierwszym; braki L i E w podpisie zbiorczym
  portret::Przebieg brakiLE = przebiegS11(false, false, false);  // bez `Leak` i bez `EVE`: bez białych i bez pików
  brakiLE.braki = "LE";
  for (auto& o : brakiLE.odcinki) o.wycS = o.wyciek95 = o.zew = o.centr = portret::BRAK;
  l.przebieg = &brakiLE;
  portretS11(p, "s11-linia-uwagi-braki", l, "Co było inaczej? ↻ 2 s");
  portretS11(p, "s11-linia-uwagi", d, "Co było inaczej? ↻ 2 s");
  {  // 3 braki przy linii uwagi (AutoSet: bez `Leak`, bezdechy bez rodzaju, maksimum nieznane): podpis zbiorczy w 2 liniach
    portret::Przebieg b3 = przebiegS11(false, false, true);
    b3.braki = "LRM";
    for (auto& o : b3.odcinki) o.wycS = o.wyciek95 = o.centr = portret::BRAK;
    DanePortretu l3 = d;
    l3.przebieg = &b3;
    portretS11(p, "s11-linia-uwagi-3-braki", l3, "Co było inaczej? ↻ 2 s");
  }
  {  // znacznik startu okna na pierścieniu (R-POR-06, zmiana 2026-11-12 (2)): noc zaczęta wcześniej niż zwykle, pierwsza
     // sesja 21:20–22:00 przez start okna 21:30 (zwykła pora 23:30), więc znacznik przecina czarny łuk maski
    portret::Przebieg wcz = przebiegS11(false, false, true);
    wcz.sesje.insert(wcz.sesje.begin(), portret::Sesja{560, 600});
    std::vector<portret::Odcinek> pierwsze;
    for (int m = 560; m < 600; m += 5) pierwsze.push_back(odcinekS11(m));
    wcz.odcinki.insert(wcz.odcinki.begin(), pierwsze.begin(), pierwsze.end());
    DanePortretu e = d;
    e.przebieg = &wcz;
    e.czas = "8 h 10";
    portretS11(p, "s11-start-okna-na-pierscieniu", e, "");
    // wyciek w pierwszym odcinku okna (21:30–21:35, 300 s): biały odcinek z obrysem nietknięty, znacznik tylko
    // pod pierścieniem, nad nim i grot
    portret::Przebieg wyc = wcz;
    for (auto& o : wyc.odcinki)
      if (o.od == 570) o.wycS = 300, o.wyciek95 = 22;
    DanePortretu f = e;
    f.przebieg = &wyc;
    portretS11(p, "s11-start-okna-wyciek", f, "");
  }
  {  // strona pod ▶ (R-POR-24): bez linii, stopka z akcjami jak w src/dyrygent.cpp rysujPortretStrony()
    TrescEkranu t;
    t.uklad = Uklad::PORTRET;
    t.portret = &d;
    rysujEkranDanych(p, Naglowek{"cz 12.11", "dane 07:12"}, t);
    p.prostokat(0, 434, 800, 46, false);
    stopkaEkranu(p, {"◀ ekran dnia", "▶ ostatnia noc", "↻ ekran dnia"}, "");
    zapisz(p, "s11-strona-pod-prawym");
  }
}

int main(int argc, char** argv) {
  kat = argc > 1 ? argv[1] : ".";
  const std::string dzis = "2026-11-12";
  std::vector<Noc> h = historia(dzis, 0), hw = historia(dzis, 1), hn = historia(dzis, 2);
  std::vector<WpisCzynnikow> cz;
  zapiszOdpowiedz(cz, dodajDni(dzis, -3), CZ_ALKOHOL);
  Stany st = liczStany(h, dzis, wykluczoneZNormy(cz)), stw = liczStany(hw, dzis, wykluczoneZNormy(cz)),
        stn = liczStany(hn, dzis, wykluczoneZNormy(cz));
  const Norma& nm = st.norma;
  PlotnoPamiec p;

  rysujK1(p, budujK1(h, dzis, nm, cz, &st));
  zapisz(p, "k1");
  rysujK1(p, budujK1(hw, dzis, stw.norma, cz, &stw));
  zapisz(p, "k1-stan-wycieku");
  rysujK2(p, budujK2(h, dzis, nm, cz, &st));
  zapisz(p, "k2");
  rysujK2(p, budujK2(hw, dzis, stw.norma, cz, &stw));
  zapisz(p, "k2-stan-wycieku");
  {  // R-KOK-15 fail-path: EPAP ostatniej nocy tylko ze STR (docelowe urządzenia) → `(urządzenie)` w K1, `–` w K2
    std::vector<Noc> hs = h;
    hs.back().epapStr = true;
    Stany sts = liczStany(hs, dzis, wykluczoneZNormy(cz));
    rysujK1(p, budujK1(hs, dzis, sts.norma, cz, &sts));
    zapisz(p, "k1-epap-urzadzenie");
  }
  // (R-KOK-16): słowa `rośnie` i `spada` z danych: AHI i wyciek wyżej w ostatnich 7 nocach, czas z maską i EPAP
  // niżej; noce syntetyczne, bez stanu odchylenia (różnica mniejsza niż Δ od mediany normy)
  {
    std::vector<Noc> hr = h, hsp = h;
    for (int i = 0; i < 7; i++) {
      Noc& r = hr[hr.size() - 1 - i];
      r.ahi += 1.6f;
      r.wp95 += 5.5f;
      Noc& s = hsp[hsp.size() - 1 - i];
      s.m01 = 370;  // mediana 7 nocy 6:10 wobec 6:59: spadek ≥ 45 min, ale mniej niż Δ 1,5 h od normy (bez stanu)
      s.emed -= 1.5f;
      s.ep95 -= 1.5f;
    }
    Stany sr = liczStany(hr, dzis, wykluczoneZNormy(cz)), ss = liczStany(hsp, dzis, wykluczoneZNormy(cz));
    rysujK1(p, budujK1(hr, dzis, sr.norma, cz, &sr));
    zapisz(p, "k1-rosnie");
    rysujK1(p, budujK1(hsp, dzis, ss.norma, cz, &ss));
    zapisz(p, "k1-spada");
  }
  // (R-KOK-15, ADR-0030): te same noce w trybie AutoSet i CPAP (ciśnienie M-05, M-05p zamiast EPAP i wsparcia);
  // w AutoSet ostatnie 7 nocy z ciśnieniem wyższym o 1,2 cmH2O (`rośnie`)
  for (const char* tryb : {"AutoSet", "CPAP"}) {
    std::vector<Noc> ha = h;
    for (size_t i = 0; i < ha.size(); i++) {
      Noc& r = ha[i];
      bool autoset = std::string(tryb) == "AutoSet";
      r.ustawienia = autoset ? "AutoSet/5/13//" : "CPAP/9///";
      r.cmed = autoset ? (float)(8.6 + (double)(i * 7 % 10) / 10.0) : 9.0f;
      r.cp95 = r.cmed + (autoset ? 1.8f : 0.0f);
      if (autoset && i + 7 >= ha.size()) { r.cmed += 1.2f; r.cp95 += 1.2f; }
      r.emed = r.ep95 = r.psmed = r.psp95 = NAN;
      if (r.zrodlo == 'A') {  // pola z AirBridge: zdjęcia maski (M-17), oddechy (M-13)
        r.m17 = (int)(i % 5 == 0);
        r.m13 = (float)(13.5 + (double)(i % 4) * 0.5);
      }
    }
    Stany sa = liczStany(ha, dzis, wykluczoneZNormy(cz));
    std::string nazwa = std::string(tryb) == "AutoSet" ? "autoset" : "cpap";
    rysujK1(p, budujK1(ha, dzis, sa.norma, cz, &sa));
    zapisz(p, ("k1-" + nazwa).c_str());
    rysujK2(p, budujK2(ha, dzis, sa.norma, cz, &sa));
    zapisz(p, ("k2-" + nazwa).c_str());
  }

  DanePytania q;
  q.dataPoranka = dzis;
  q.innaNoc = true;
  q.opisNocy = opisNocyPytania(hw.back(), stw.norma, true);  // tekst z logiki
  {
    Noc o = hw.back();
    o.m01 = 192;
    o.ahi = 12.4f;
    opisMak = opisNocyPytania(o, stw.norma, true);
  }
  q.kursor = 2;
  q.zaznaczone = CZ_ALKOHOL | CZ_KOLACJA;
  q.termin = 660;
  rysujPytanie(p, q);
  zapisz(p, "pytanie");
  q.innaNoc = false;
  q.opisNocy = "6:08 · AHI 4,6";
  q.kursor = POZYCJA_NIC;
  q.zaznaczone = CZ_LEKI;
  q.termin = -1;
  rysujPytanie(p, q);
  zapisz(p, "pytanie-na-zadanie");

  // sypialnia: 30 nocy z czujnika panelu (mediany nocy), syntetyczne
  std::vector<NocSypialni> syp;
  for (int i = 30; i >= 0; i--) {
    NocSypialni s;
    s.data = dodajDni(dzis, -i);
    s.t = (float)(20.0 + (i * 7 % 10) / 10.0);
    s.w = (float)(43 + (i * 3 % 7));
    if (i == 0) { s.t = 20.5f; s.w = 46; }
    syp.push_back(s);
  }

  DaneDnia d;
  d.dzis = dzis;
  d.hist = &h;
  d.stany = &st;
  d.norma = nm;
  d.godzinaDanych = "05:46";
  d.sypNoc = sypialniaZDnia(syp, dzis);
  d.sypNorma = liczNormeSypialni(syp, dzis, {});
  d.przyLozku = stopkaCzujnika(21.4f, 45.6f);  // odczyt czujnika panelu w tym wybudzeniu
  Naglowek n{naglowekDniaLewo(d), naglowekDniaPrawo(d)};
  KontekstHumoru kh;
  kh.dzis = dzis;
  kh.sceny = sceny();
  kh.uspokojenie = st.uspokojenie;

  auto karta = [&](const std::string& typ, const std::string& klucz, const char* plik, bool propozycje) {
    EkranDnia e;
    e.typ = typ;
    e.klucz = klucz;
    e.kartaZTabeli = true;
    kh.zPropozycjami = propozycje;
    TrescEkranu t = trescDnia(typ, klucz, d);
    KontekstHumoru khk = kh;
    if (typ == "S2A") khk.uspokojenie = true;  // K22 ze zdaniem z pełnej puli „dobra noc” (także uspokajające)
    Wybor w = wybierzTryb(e, kartaMiesci(t), {}, khk);
    if (std::string(plik) == "karta-k22-propozycje") scenaK22 = w.scena;
    if (w.tryb == 'K') rysujKarte(p, n, t, w.scena);
    else { printf("  %s: ekran danych (%s)\n", plik, w.dlaczego); rysujEkranDanych(p, n, t); }
    zapisz(p, plik);
  };
  karta("S2B", "", "karta-s2b-seria", true);  // pula „seria” przyjęta: 7 zdań przyjętych
  karta("KM", "100", "karta-km-100", false);
  karta("S2A", "3", "karta-k22-propozycje", true);
  karta("KM", "365", "karta-km-365", false);  // M05 w pamięci (rocznica)
  {  // karty ekranów S-K z tabeli 5.7.3; teksty dosłownie z 5.8.4, liczby syntetyczne (ekrany S-K rysuje
     // dopiero pakiet ekranów; tu tylko karta z budzikiem: scena i zdanie z humoru, pula przyjęta)
    auto kartaSK = [&](const std::string& typ, const std::string& sprawa, const char* plik, const char* et,
                       const char* tytul, const char* powod, const char* krok) {
      TrescEkranu t;
      t.etykieta = et;
      t.tytul = tytul;
      t.powod = powod;
      t.krok = krok;
      t.stopkaPrawo = d.przyLozku;
      kh.zPropozycjami = true;
      EkranDnia e = ekranKarty(typ, sprawa, dzis, {}, krok);
      Wybor w = wybierzTryb(e, kartaMiesci(t), {}, kh);
      if (w.tryb == 'K') rysujKarte(p, n, t, w.scena);
      else { printf("  %s: ekran danych (%s)\n", plik, w.dlaczego); rysujEkranDanych(p, n, t); }
      zapisz(p, plik);
    };
    kartaSK("SK01", dzis, "karta-sk01-przewietrz", "PRZED SNEM", "Przewietrz sypialnię",
            "Zeszłej nocy CO2 przez 3\xC2\xA0h ponad 1000\xC2\xA0ppm.", "→ Uchyl okno na noc.");
    kartaSK("SK29", dzis, "karta-sk29-powrot", "KAMIEŃ MILOWY", "Wracasz do rytmu",
            "Po 3 nocach przerwy dziś 6\xC2\xA0h\xC2\xA0" "40 z maską.", "→ Dziś wieczorem znowu z maską.");
    kartaSK("SK32", dzis, "karta-sk32-nowa-poduszka", "SPRZĘT", "Nowa poduszka działa",
            "Wyciek 8\xC2\xA0l/min, przed wymianą 21.", "→ Wymiana co 3\xC2\xA0mies. się opłaca.");
  }
  {  // przerywnik S2d (5.7.6) z mechanizmu humoru; wszystkie 18 kart G01–G18 z tekstami z zestawu
     // (teksty właściciela projektu), etykieta, tytuł i powód jak na panelu, bez kroku (R-HUM-21), fakty
     // ostatniej nocy z dowodu spokoju i zdanie przypięte do sceny (5.7.5)
    UstawieniaPrzerywnikow u = ustawieniaPrzerywnikowBety();
    kh.zPropozycjami = true;
    EkranDnia dowod{"S2A", "1", false, false};
    Przerywnik pr = wybierzPrzerywnik(dowod, {}, kh, u);
    if (!pr.jest) printf("  przerywnik: brak (%s)\n", pr.dlaczego);
    TrescEkranu baza = trescDnia("S2A", "3", d);
    for (int g = 1; g <= 18; g++) {
      char sc[32];
      snprintf(sc, sizeof sc, "G%02d", g);
      for (bool war : {true, false}) {  // dwa komplety tekstów, karta bez etykiety (ADR-0009)
        TekstPrzerywnika tp = tekstPrzerywnika(sc, war);
        TrescEkranu t = baza;
        trescPrzerywnika(t, tp.tytul, tp.powod, d);
        t.stopkaPrawo = d.przyLozku;
        if (!kartaPrzerywnikaMiesci(t)) printf("  UWAGA: %s %s poza układem S2d\n", sc, war ? "warunek" : "neutralna");
        rysujKarte(p, n, t, sc);
        zapisz(p, (std::string("karta-s2d-") + sc + (war ? "-warunkowa" : "-neutralna")).c_str());
      }
    }
  }
  {  // neutralna G01 na marcową zmianę czasu (jedno zdanie, bez powodu)
    TekstPrzerywnika tp = tekstPrzerywnika("G01", false, "2027-03-28");
    TrescEkranu t = trescDnia("S2A", "3", d);
    trescPrzerywnika(t, tp.tytul, tp.powod, d);
    t.stopkaPrawo = d.przyLozku;
    if (!kartaPrzerywnikaMiesci(t)) printf("  UWAGA: G01 marzec poza układem S2d\n");
    rysujKarte(p, n, t, "G01");
    zapisz(p, "karta-s2d-G01-neutralna-marzec");
  }
  rysujEkranDanych(p, n, trescDnia("S2A", "3", d));
  zapisz(p, "dane-s2a");
  rysujEkranDanych(p, n, trescDnia("S2B", "", d));
  zapisz(p, "dane-s2b");
  rysujEkranDanych(p, n, trescDnia("S2C", "4", d));
  zapisz(p, "dane-s2c");
  rysujEkranDanych(p, n, trescDnia("KM", "100", d));
  zapisz(p, "dane-km-100");
  {  // noc z odchyleniem i stan wycieku: tytuł `Noc: …`, powód `Tydzień: szczegóły…`
    DaneDnia w = d;
    w.hist = &hw;
    w.stany = &stw;
    w.norma = stw.norma;
    rysujEkranDanych(p, Naglowek{naglowekDniaLewo(w), naglowekDniaPrawo(w)}, trescDnia("S2A", "3", w));
    zapisz(p, "dane-s2a-odchylenie");
    w.linia = "Co było inaczej? ↻ 2 s";
    rysujEkranDanych(p, Naglowek{naglowekDniaLewo(w), naglowekDniaPrawo(w)}, trescDnia("S2A", "4", w));
    zapisz(p, "dane-s2a-linia-zaproszenia");
    rysujEkranDanych(p, Naglowek{naglowekDniaLewo(w), naglowekDniaPrawo(w)}, trescDnia("S2B", "", w));
    zapisz(p, "dane-s2b-linia");
  }
  {  // noc niepewna (4.9): `Noc: {czas}.` i `· ocena ograniczona` (R-EKR-46)
    DaneDnia w = d;
    w.hist = &hn;
    w.stany = &stn;
    w.norma = stn.norma;
    rysujEkranDanych(p, Naglowek{naglowekDniaLewo(w), naglowekDniaPrawo(w)}, trescDnia("S2A", "3", w));
    zapisz(p, "dane-s2a-niepewna");
  }
  {  // raport bez `used_seconds` (R-DANE-07, R-AW-22): czas `–`, tytuł bez czasu, nigdy `0 h 00`
    std::vector<Noc> hb = h;
    hb.back().m01 = -1;
    hb.back().wazna = false;
    Stany sb = liczStany(hb, dzis, {});
    DaneDnia w = d;
    w.hist = &hb;
    w.stany = &sb;
    w.norma = sb.norma;
    rysujEkranDanych(p, Naglowek{naglowekDniaLewo(w), naglowekDniaPrawo(w)}, trescDnia("S2A", "3", w));
    zapisz(p, "dane-s2a-bez-czasu");
  }
  {  // S5 po odrzuconym logowaniu (401) bez wcześniejszego odczytu: tekst E-01 bez kodu, `od` = pierwsza nieudana próba
    std::vector<Noc> stara(h.begin(), h.end() - 1);
    DaneDnia b = d;
    b.hist = &stara;
    Stany sb = liczStany(stara, dzis, {});
    b.stany = &sb;
    b.norma = sb.norma;
    b.httpKod = 401;
    b.bladOd = "05:30";
    rysujEkranDanych(p, Naglowek{naglowekDniaLewo(b), naglowekDniaPrawo(b)}, trescDnia("S5", "1", b));
    zapisz(p, "dane-s5-401");
  }
  {  // brak danych: ostatnia noc z 11.11, odczyt z wczoraj
    std::vector<Noc> stara(h.begin(), h.end() - 1);
    DaneDnia b = d;
    b.hist = &stara;
    Stany sb = liczStany(stara, dzis, {});
    b.stany = &sb;
    b.norma = sb.norma;
    b.godzinaDanych = "05:42";
    b.ostatnieDane = kiedyOdczyt("2026-11-11", 5 * 60 + 42, dzis);  // odczyt z wczoraj: z datą (U7)
    b.httpKod = -1;
    Naglowek n5{naglowekDniaLewo(b), naglowekDniaPrawo(b)};
    TrescEkranu t = trescDnia("S5", "1", b);
    EkranDnia e{"S5", "1", true, false};
    Wybor w = wybierzTryb(e, kartaMiesci(t), {}, kh);
    rysujKarte(p, n5, t, w.scena);
    scenaS5 = w.scena;
    zapisz(p, "karta-s5");
    rysujEkranDanych(p, n5, t);
    zapisz(p, "dane-s5");
  }
  {  // S10 „Synchronizuję się z aparatem” (R-EKR-57) i S5 od 3. nocy bez danych („awaria” z krokiem, R-AW-24)
    std::vector<Noc> stara(h.begin(), h.end() - 1);
    DaneDnia b = d;
    b.hist = &stara;
    Stany sb = liczStany(stara, dzis, {});
    b.stany = &sb;
    b.norma = sb.norma;
    b.godzinaDanych = "";
    Naglowek n10{naglowekDniaLewo(b), naglowekDniaPrawo(b)};
    TrescEkranu t = trescDnia("S10", "", b);
    rysujKarte(p, n10, t, "B05");
    zapisz(p, "karta-s10");
    rysujEkranDanych(p, n10, t);
    zapisz(p, "dane-s10");
    std::vector<Noc> trzy;
    for (auto& x : h)
      if (x.data < dodajDni(dzis, -2)) trzy.push_back(x);
    b.hist = &trzy;
    rysujEkranDanych(p, n10, trescDnia("S5", "1", b));
    zapisz(p, "dane-s5-3-noce");
    // S5 w 2. nocy bez danych (R-AW-24): `Sprawdzam co 3 h.`, a przy odrzuconym logowaniu `Ponawiam co 30 min.`
    std::vector<Noc> dwie;
    for (auto& x : h)
      if (x.data < dodajDni(dzis, -1)) dwie.push_back(x);
    b.hist = &dwie;
    rysujEkranDanych(p, n10, trescDnia("S5", "1", b));
    zapisz(p, "dane-s5-2-noce");
    b.logowanieOdrzucone = true;
    rysujEkranDanych(p, n10, trescDnia("S5", "1", b));
    zapisz(p, "dane-s5-2-noce-401");
  }
  {
    std::vector<Noc> krotka(h.end() - 9, h.end());
    DaneDnia r = d;
    r.hist = &krotka;
    Stany sr = liczStany(krotka, dzis, {});
    r.stany = &sr;
    r.norma = sr.norma;
    rysujEkranDanych(p, Naglowek{naglowekDniaLewo(r), naglowekDniaPrawo(r)}, trescDnia("S6", "", r));
    zapisz(p, "dane-s6-rozruch");
  }
  {  // U-05 według kanwy z wartościami, które firmware ma dziś: zestaw wbudowany, bez karty, bez % baterii
    DaneTestu u;
    u.lewa = {{"Oprogramowanie", "1.0.0 (app0)"}, {"Zestaw reguł", "wbudowany"}, {"Pamięć panelu", "wolne 23 MB"},
              {"Karta", "brak karty"}, {"Źródło otoczenia", "OK 05:46"}};
    u.prawa = {{"Panel", "192.168.0.10"}, {"Wi-Fi", "Dom −60 dBm"}, {"AirBridge", "192.168.0.20 · v2.3.0"},
               {"Ostatnie pobranie", "12.11 05:32"}, {"Bateria", "–"}};
    // wiersz trybu porównawczego (ADR-0036 „Zmiana 2026-11-09”) pod kolumnami: 7 z 7 zgodnych
    u.dol = {{"Noce z plików", "zgodne 7 z 7, można przełączyć na pliki"}};
    u.ramkaLewo = "Wgrywanie przez Wi-Fi do 23:59";
    u.ramkaPrawo = "hasła i dane terapii nie są pokazywane";
    rysujTest(p, u);
    zapisz(p, "test");
    u.dol = {{"Noce z plików", "rozjazd 1 z 7 (M-05, M-02)"}};
    rysujTest(p, u);
    zapisz(p, "test-porownanie-rozjazd");
    // najdłuższe wartości: warianty skrócone (dopasujWartosc) i bez połączenia z Wi-Fi (ostatnie z godziną)
    Otoczenie o;
    wyczyscOtoczenie(o, true);
    nieudanaOtoczenia(o, 401, 1793395800u);
    u.lewa[2].wartosc = dopasujWartosc("Pamięć panelu", {"dane z nowszej wersji: wybudzenia.csv", "dane z nowszej wersji"});
    u.lewa[4].wartosc = dopasujWartosc("Źródło otoczenia", wartosciTestuOtoczenia(o, "05:16"));
    u.prawa[0].wartosc = "192.168.100.200 (11.11 23:59)";
    u.prawa[1].wartosc = dopasujWartosc("Wi-Fi", {"Bardzo-Dluga-Nazwa-Sieci-Domowej −90 dBm (11.11 23:59)"});
    u.prawa[2].wartosc = dopasujWartosc("AirBridge", {"brak odpowiedzi od 11.11 23:40", "brak odp. od 11.11 23:40"});
    u.dol = {{"Noce z plików", "brak porównania: AirBridge < 2.3.1"}};
    u.ramkaLewo = "Wgrywanie przez Wi-Fi: wyłączone";
    rysujTest(p, u);
    zapisz(p, "test-najdluzsze");
    // tryb awaryjny (R-AW-39, T-WYM-83): tylko wersja ze slotem, przyczyna i okno; stopka `◀ wróć`
    DaneTestu aw;
    aw.awaryjny = true;
    aw.lewa = {{"Oprogramowanie", "1.0.0 (app1)"}, {"Tryb awaryjny", "błąd programu"}};
    aw.ramkaLewo = "Wgrywanie przez Wi-Fi do 23:59";
    aw.ramkaPrawo = "hasła i dane terapii nie są pokazywane";
    rysujTest(p, aw);
    zapisz(p, "test-awaryjny");
  }

  // lokalne źródło otoczenia (ADR-0019): pogoda w stopce z lewej, CO2 sypialni dopisane do odczytu
  // czujnika panelu z prawej; S2c z kolumną CO2 (mediana nocy), E-08 jako linia „dane” (biały pasek)
  {
    std::vector<NocSypialni> sc = syp;
    for (size_t i = 0; i < sc.size(); i++) sc[i].co2 = (float)(760 + (i * 37 % 9) * 10);  // 30 nocy z CO2
    sc.back().co2 = 1180;  // ostatnia noc: zaduch (poza pasmem normy)
    DaneDnia o = d;
    o.sypNoc = sypialniaZDnia(sc, dzis);
    o.sypNorma = liczNormeSypialni(sc, dzis, {});
    o.pogoda = "Katowice 8° · deszcz";
    o.przyLozku = stopkaCzujnika(21.4f, 45.6f, 770);
    rysujEkranDanych(p, n, trescDnia("S2A", "3", o));
    zapisz(p, "oto-s2a-pogoda-co2");
    rysujEkranDanych(p, n, trescDnia("S2C", "4", o));
    zapisz(p, "oto-s2c-co2");
    {  // karta: lewa część stopki to zdanie budzika, odczyt przy łóżku z CO2 z prawej (ustępuje przy długim zdaniu)
      DaneDnia k = o;
      std::swap(d, k);
      karta("S2B", "", "oto-karta-s2b", true);
      std::swap(d, k);
    }
    o.pogoda = "Katowice −12° · śnieg";
    o.przyLozku = stopkaCzujnika(35, 99, 4999);
    o.linia = tekstE08("2026-11-11", 14 * 60 + 20, dzis);
    o.liniaOstrzezenie = liniaJestOstrzezeniem(GrupaLinii::DANE);
    rysujEkranDanych(p, n, trescDnia("S2A", "3", o));
    zapisz(p, "oto-s2a-najdluzsza-stopka-e08");
    rysujEkranDanych(p, n, trescDnia("S2C", "4", o));
    zapisz(p, "oto-s2c-e08");
    DaneDnia b = o;  // źródło bez danych sypialni (blok null): stopka bez CO2, S2c z dwiema kolumnami
    b.linia.clear();
    b.przyLozku = stopkaCzujnika(21.4f, 45.6f, -1);
    b.sypNoc = sypialniaZDnia(syp, dzis);
    rysujEkranDanych(p, n, trescDnia("S2C", "4", b));
    zapisz(p, "oto-s2c-bez-co2");
  }

  // ekran nocny „Dobranoc”: każdy wariant i każda scena (n = 0…8 obejmuje wszystkie), z godziną maski (`edf_prefix`
  // przy `rop` = 1) i bez niej (bez terapii albo bez prefiksu); wymyślone wartości: maska od 22:47, czujnik 21,4°
  // i 44,6%, zwykły czas z maską 7 h 25
  DaneNocy dn;
  dn.wieczor = "2026-10-30";
  dn.temp = 21.4f;
  dn.wilg = 44.6f;
  dn.zwyklySen = 7 * 60 + 25;
  for (long nr = 0; nr < 9; nr++) {
    if (nr == 8) continue;  // A3-W12 jak nr 2
    dn.wybor = wyborNocy(nr);
    dn.maskaOd = 22 * 60 + 47;
    rysujDobranoc(p, dn);
    std::string plik = "noc-" + std::to_string(nr) + "-" + dn.wybor.scena;
    zapisz(p, plik.c_str());
    if (nr < 3) {
      dn.maskaOd = -1;
      rysujDobranoc(p, dn);
      zapisz(p, (plik + "-bez-godziny").c_str());
    }
  }
  dn.maskaOd = -1;  // S9 ze sceną bez zdania budzika (zdanie spoza puli, R-KON-40); od 08.11.2026 flaga propozycji zdania nie zabiera
  for (long nr = 0; nr < 3; nr++) {
    dn.wybor = wyborNocy(nr, false);
    dn.wybor.zdanie.clear();
    rysujDobranoc(p, dn);
    zapisz(p, ("noc-" + std::to_string(nr) + "-" + dn.wybor.scena + "-bez-zdania").c_str());
  }
  podgladB9(p);
  {  // ekrany z wyników silnika na atrapie zestawu (test/atrapy/zestaw_kolejki.h), dane syntetyczne
    reguly::Zestaw z = atrapa_b5::zestaw();
    auto f = atrapa_b5::funkcja(z);
    std::map<std::string, std::string> zm = {{"x", "12,4"}, {"n", "3"}, {"min", "42"}, {"m", "27"}};
    auto wyn = [&](const char* id, const char* war, bool sp) { return atrapa_b5::wynik(z, id, war, true, sp, zm); };
    auto dzienZ = [&](const std::vector<Wynik_>& w, reguly::Pamiec pam, const std::string& data, const char* plik,
                      const reguly::Odpowiedzi* odp) {
      std::vector<sprawy::SprawaL> sp = sprawy::sprawyOtwarte(z, w, pam, f);
      kolejka::WejscieKolejki k;
      k.dzis = data;
      k.sprawy = &sp;
      k.odpowiedzi = odp;
      k.ekranDnia = sprawy::sprawaEkranuDnia(sp, data);
      DaneDnia w2 = d;
      w2.dzis = data;
      kolejka::ustawLinie(w2, kolejka::kolejkaLinii(k));
      if (k.ekranDnia) {
        rysujWzorzec(p, trescWzorca(*k.ekranDnia, w2));
      } else {
        rysujEkranDanych(p, Naglowek{naglowekDniaLewo(w2), naglowekDniaPrawo(w2)}, trescDnia("S2A", "3", w2));
      }
      zapisz(p, plik);
    };
    reguly::Pamiec pam;
    pam["L-06/b"].aktywnaOd = dodajDni(dzis, -1);  // A od wczoraj: dziś linia grupy 0, bez pełnego ekranu
    dzienZ({wyn("L-06", "b", true)}, pam, dzis, "b5-dzien-linia-A", nullptr);
    pam.clear();
    pam["L-11"].aktywnaOd = dodajDni(dzis, -3);  // B: linia co 3 dni w dni bez pełnego ekranu (R-SKI-37)
    dzienZ({wyn("L-11", "", true)}, pam, dzis, "b5-dzien-linia-B", nullptr);
    pam.clear();
    pam["L-08"].aktywnaOd = "2026-10-10";  // C: linia w niedzielę (R-SKI-12)
    dzienZ({wyn("L-08", "", true)}, pam, "2026-11-15", "b5-dzien-linia-C", nullptr);
    pam.clear();
    dzienZ({wyn("L-11", "", true)}, pam, dzis, "b5-wzorzec-L11", nullptr);  // B w dniu wejścia: pełny ekran
    dzienZ({wyn("L-06", "b", true)}, pam, dzis, "b5-wzorzec-A", nullptr);
    dzienZ({wyn("L-03", "10", true)}, pam, dzis, "b5-wzorzec-L03", nullptr);
    dzienZ({wyn("L-09", "", true)}, pam, dzis, "b5-wzorzec-L09-problem", nullptr);
    {  // E-05 (bateria ekranu, M-25; teksty jak w docs/spec/katalog/teksty.csv): linia ≤ 15% (grupa „awarie”, biała),
       // S5 przy ≤ 5% i R-PIE-07: wzorzec A w dniu pełnego ekranu z linią krytyczną (czarny pasek z `!`, R-EKR-52)
      std::vector<sprawy::LiniaZdarzenia> le = {{"E-05/linia", "awarie", "Ekran: bateria 14%, naładuj w tym tygodniu."}};
      std::vector<sprawy::SprawaL> sp;
      kolejka::WejscieKolejki k;
      k.dzis = dzis;
      k.sprawy = &sp;
      k.linieE = &le;
      DaneDnia w2 = d;
      w2.dzis = dzis;
      kolejka::ustawLinie(w2, kolejka::kolejkaLinii(k));
      rysujEkranDanych(p, Naglowek{naglowekDniaLewo(w2), naglowekDniaPrawo(w2)}, trescDnia("S2A", "3", w2));
      zapisz(p, "bateria-e05-linia");
      DaneDnia w5 = d;
      w5.dzis = dzis;
      w5.e05Tytul = "Naładuj ekran";
      w5.e05Powod = "Bateria 4%.";
      w5.e05Krok = "→ Podłącz kabel USB-C.";
      rysujEkranDanych(p, Naglowek{naglowekDniaLewo(w5), naglowekDniaPrawo(w5)}, trescDnia("S5", "E-05", w5));
      zapisz(p, "bateria-e05-s5");
      std::vector<sprawy::SprawaL> spA = sprawy::sprawyOtwarte(z, {wyn("L-06", "b", true)}, reguly::Pamiec(), f);
      const sprawy::SprawaL* a = sprawy::sprawaEkranuDnia(spA, dzis);
      if (a) {
        kolejka::WejscieKolejki ka;
        ka.dzis = dzis;
        ka.sprawy = &spA;
        ka.ekranDnia = a;
        ka.bateriaKrytyczna = "Naładuj ekran: bateria 4%.";
        std::vector<kolejka::LiniaUwagi> kl = kolejka::kolejkaLinii(ka);
        DaneWzorca dw = trescWzorca(*a, w2);
        for (auto& l : kl)
          if (l.zrodlo == "E-05/krytyczne") {
            dw.linia = l.tekst;
            dw.liniaOstrzezenie = l.zdrowie();
          }
        rysujWzorzec(p, dw);
        zapisz(p, "bateria-e05-wzorzec-A");
      }
    }
    {  // „Otwarte sprawy”: L-11 po „wiem”, L-08 (C), L-02 „tylko raport”, metryka AHI
      std::vector<Wynik_> w = {wyn("L-11", "", true), wyn("L-08", "", true), wyn("L-02", "a", true)};
      reguly::Pamiec pm;
      pm["L-11"].aktywnaOd = dodajDni(dzis, -4);
      pm["L-11"].wiem = dodajDni(dzis, -1);
      pm["L-08"].aktywnaOd = "2026-10-01";
      std::vector<sprawy::SprawaL> sp = sprawy::sprawyOtwarte(z, w, pm, f);
      std::vector<sprawy::SprawaMetryki> m = {{"AHI", 9, "6,2", "0,8", "2,1", "6,2", "M-02", ""}};
      sprawy::WejscieSpraw ws;
      ws.dzis = dzis;
      ws.sprawy = &sp;
      ws.metryki = &m;
      auto karty = sprawy::kartySpraw(ws);
      rysujSprawy(p, trescSpraw(karty, 1, ws));
      zapisz(p, "b5-sprawy-pierwsza");
      rysujSprawy(p, trescSpraw(karty, 3, ws));
      zapisz(p, "b5-sprawy-w-raporcie");
      rysujSprawy(p, trescSpraw(karty, (int)karty.size(), ws));
      zapisz(p, "b5-sprawy-ostatnia");
      sprawy::WejscieSpraw pusto;
      pusto.dzis = dzis;
      rysujSprawy(p, trescSpraw({}, 1, pusto));
      zapisz(p, "b5-sprawy-pusto");
      rysujOmowione(p, trescOmowione(sp[0], dzis));
      zapisz(p, "b5-omowione");
      rysujPotwierdzenie(p, trescPrzeniesione(sp[0], d, true));
      zapisz(p, "b5-przeniesione");
      rysujRaport(p, trescRaportu(sp[0], d));
      zapisz(p, "b5-raport");
      TrescEkranu kp;  // karta w tonie „problem” (R-HUM-24), tekst z artboardu K09 (syntetyczny)
      kp.etykieta = "MASKA · TRZECIA NOC";
      kp.tytul = "Maska przecieka.";
      kp.powod = "Trzecią noc z rzędu ponad próg 24.";
      kp.tonProblem = true;
      rysujKarte(p, {"pn 09.11", "dane 05:44"}, kp, scenaJest("B04") ? "B04" : "");
      zapisz(p, "b5-karta-problem");
    }
    DaneDnia nd = d;
    nd.dzis = "2026-11-15";
    rysujPytanieTN(p, trescPytaniaTN(sprawy::Pytanie::SENNOSC, nd, 11 * 60, 0));
    zapisz(p, "b5-pytanie-sennosc");
    rysujPytanieTN(p, trescPytaniaTN(sprawy::Pytanie::GORZEJ, d, 11 * 60, 0));
    zapisz(p, "b5-pytanie-gorzej");
    rysujPytanieTN(p, trescPytaniaTN(sprawy::Pytanie::PRZERWA, d, 13 * 60 + 5, 4));
    zapisz(p, "b5-pytanie-przerwa");
    rysujPotwierdzenie(p, trescZapisano(sprawy::Pytanie::GORZEJ, true, d, true));
    zapisz(p, "b5-zapisano-tak");
    dzienZ({wyn("L-08", "szybki", true)}, pam, dzis, "b5-wzorzec-L08", nullptr);
  }
  {  // ekran „Raport dla lekarza” z kodem QR (R-raport-qr), po pobraniu, po ↻ 2 s, ściąga (R-sciaga), bez Wi-Fi
    DaneEkranuRaportu e;
    e.naglowek = {"śr 04.11 · raport gotowy", "Wi-Fi włączone"};
    e.temat = "Bezdechy centralne utrwalone";
    e.okres = "Okres: 07.08–04.11 · 1 strona A4";
    e.adresQr = "http://192.168.1.47/r/f1b384krart14y2p6g97gnhm28/";
    e.adresReczny = "192.168.1.47/r/f1b384krart14y2p6g97gnhm28";
    e.kodWazny = "Kod ważny do 07:25.";
    rysujRaportQr(p, e);
    zapisz(p, "b13-raport-qr");
    e.pobrano = "Pobrano o 07:12.";
    rysujRaportQr(p, e);
    zapisz(p, "b13-raport-pobrano");
    e.stan = "Starsze raporty widoczne do 07:25.";
    e.starsze = true;
    e.temat = "Raport ogólny, 90 nocy";
    rysujRaportQr(p, e);
    zapisz(p, "b13-raport-ogolny-starsze");
    // U-01 „Ustawienia” i U-03 formularz z kodem QR (stany: kod, zapisano, wynik sprawdzenia, bez Wi-Fi)
    // U-01 jako strona 3 / 3, bez kursora: lista bieżących ustawień do odczytu (R-KONF-55)
    {
      DaneUstawien u;
      u.ssid = "Dom-2.4GHz";
      u.abAdres = "192.168.1.40";
      u.miasto = "Katowice";
      u.co2 = 1;
      u.wersja = "1.0.0";
      u.aktualizacja = aktualizacja::wierszU01(true, "", "1.0.0");
      rysujUstawienia(p, u);
      zapisz(p, "p07-u01-ustawienia");
      // najdłuższe wartości i znana nowsza wersja: wiersz `Jest wersja … · ↻ 2 s Aktualizacja` pod listą
      u.ssid = "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
      u.abAdres = "bardzo-dluga-nazwa-airbridge-w-sieci.local:65535";
      u.miasto = MIASTO_ZE_WSPOLRZEDNYCH;
      u.aktualizacja = aktualizacja::wierszU01(true, "1.12.3-rc.12", "1.0.0");
      rysujUstawienia(p, u);
      zapisz(p, "p07-u01-ustawienia-nowa-wersja");
      u.aktualizacja = aktualizacja::wierszU01(false, "", "1.0.0");
      rysujUstawienia(p, u);
      zapisz(p, "p07-u01-ustawienia-bez-klucza");
    }
    {  // U-07 „Aktualizacja”: jest nowsza wersja
      aktualizacja::DaneU07 d7;
      d7.stan = aktualizacja::StanU07::NOWSZA;
      d7.wlasna = "1.0.0";
      d7.nowa = "1.1.0";
      d7.sprawdzono = "10.11 09:12";
      aktualizacja::TresciU07 t7 = aktualizacja::trescU07(d7);
      rysujAktualizacje(p, t7.linie, t7.stopka);
      zapisz(p, "akt-u07-jest-nowsza");
    }
    DaneFormularzaQr f;
    f.adresQr = "http://192.168.1.47/u/f1b384krart14y2p6g97gnhm28/";
    f.adresReczny = "192.168.1.47/u/f1b384krart14y2p6g97gnhm28";
    f.kodWazny = "Kod ważny do 21:29.";
    rysujFormularzQr(p, f);
    zapisz(p, "p07-u03-formularz");
    f.zapisano = "Zapisano o 21:14.";
    f.stan = "AirBridge pod 192.168.1.21 odrzucił logowanie. Sprawdź konto i hasło. Poprzednie ustawienia zostają.";
    rysujFormularzQr(p, f);
    zapisz(p, "p07-u03-odrzucone");
    f.stan = "Nie łączę się z siecią Mieszkanie-Nowe-5G-2,4. Sprawdź hasło. Wracam do sieci Dom.";
    rysujFormularzQr(p, f);
    zapisz(p, "p07-u03-siec-odrzucona");
    DaneFormularzaQr bez;
    bez.bezWifi = true;
    rysujFormularzQr(p, bez);
    zapisz(p, "p07-u03-bez-wifi");
    DaneFormularzaQr ok;
    ok.zapisano = "Zapisano o 21:16.";
    ok.stan = "Połączono z siecią Mieszkanie. AirBridge odpowiada pod 192.168.1.21. Zapisano.";
    rysujFormularzQr(p, ok);
    zapisz(p, "p07-u03-siec-zapisana");
    // U-02 sieć panelu (dwa kody), po nieudanej próbie, łączenie, pierwsze uruchomienie, bez radia
    DaneSieciPanelu sp;
    sp.nazwa = "cpap-eink-A1B2";
    sp.haslo = "k7m3xq9hv2tp";
    sp.adresQr = "http://192.168.4.1/u/f1b384krart14y2p6g97gnhm28/";
    sp.adresReczny = "192.168.4.1/u/f1b384krart14y2p6g97gnhm28";
    sp.wazne = "Ważne do 21:29.";
    rysujSiecPanelu(p, sp);
    zapisz(p, "p07-u02-siec-panelu");
    sp.stan = "Nie łączę się z siecią Mieszkanie-Nowe-5G-2,4. Sprawdź hasło. Poprzednie ustawienia zostają.";
    rysujSiecPanelu(p, sp);
    zapisz(p, "p07-u02-siec-odrzucona");
    sp.stan = "AirBridge pod 192.168.100.200:8080 odrzucił logowanie. Sprawdź konto i hasło. Poprzednie ustawienia "
              "zostają.";
    sp.wazne = "Ważne do 21:29. Zapisano o 21:16.";
    rysujSiecPanelu(p, sp);
    zapisz(p, "p07-u02-airbridge-odrzucony");
    DaneSieciPanelu la;
    la.stan = "Łączę z siecią Mieszkanie…";
    rysujSiecPanelu(p, la);
    zapisz(p, "p07-u02-laczenie");
    DaneSieciPanelu pi;
    pi.instrukcja = true;
    rysujSiecPanelu(p, pi);
    zapisz(p, "p07-u02-pierwsze-uruchomienie");
    DaneSieciPanelu br;
    br.bezRadia = true;
    rysujSiecPanelu(p, br);
    zapisz(p, "p07-u02-bez-radia");
    DaneSciagi s;
    s.naglowek = e.naglowek;
    s.temat = "Bezdechy centralne utrwalone";
    s.pytanie = "Bezdechy centralne utrwalone: czy to wymaga korekty leczenia albo dalszej diagnostyki?";
    const char* l[3] = {"2,0", "5,4", "6,2"};
    const char* et[3] = {"30 nocy przed", "od początku wzorca", "ostatnie 7"};
    for (int i = 0; i < 3; i++) {
      s.liczby[i] = l[i];
      s.etykiety[i] = et[i];
    }
    s.podpis = "Centralne / h · okres 07.08–04.11.2026";
    s.adresQr = "http://192.168.1.47/r/f1b384krart14y2p6g97gnhm28/edf";
    rysujSciage(p, s);
    zapisz(p, "b13-sciaga");
    rysujRaportBezWifi(p, {"śr 04.11", ""}, "Bezdechy centralne utrwalone", "");
    zapisz(p, "b13-raport-bez-wifi");
  }
  podgladB16(p);
  {  // pełny ekran S3 każdego zdarzenia E w tonie „problem” (6.4, R5b–R5d) na zestawie wbudowanym, teksty
     // z teksty.csv; wykres 30 nocy z historii z wyciekami (hw); wariant `-najdluzsze` z najdłuższymi zmiennymi
     // (5.4.4), wariant z linią `Wciąż: …` (R-EKR-40) i ekran dnia S2A z liniami E-04 (dane) i E-10 (noc), 7.5
    DaneDnia e = d;
    e.hist = &hw;
    e.stany = &stw;
    e.norma = stw.norma;
    for (auto& kv : b14::zmienneE(false)) {
      std::string nazwa = kv.first;
      std::replace(nazwa.begin(), nazwa.end(), '/', '-');
      // przegląd historia z danymi metryki zdarzenia (M-08 dla E-22, 2 noce bez terapii dla E-10, zgodnie
      // z `Ostatnia terapia: 10.11.` przy nagłówku 12.11)
      std::vector<Noc> hz = hw;
      for (size_t i = 0; i < hz.size(); i++) {
        size_t odKonca = hz.size() - 1 - i;
        if (kv.first == "E-22") hz[i].m08 = odKonca < 5 && odKonca != 2 ? (float)(40 + 6 * odKonca) : (float)(i % 4 * 3);
        if (kv.first == "E-10/problem" && odKonca < 2) hz[i].m01 = 5;
      }
      DaneDnia ez = e;
      ez.hist = &hz;
      rysujWzorzec(p, zdarzenia::trescZdarzenia(b14::sprawaE(kv.first), ez));
      zapisz(p, ("b14-s3-" + nazwa).c_str());
      rysujWzorzec(p, zdarzenia::trescZdarzenia(b14::sprawaE(kv.first, true), ez));
      zapisz(p, ("b14-s3-" + nazwa + "-najdluzsze").c_str());
    }
    DaneDnia l = e;
    l.linia = "Wciąż: " + b14::sprawaE("E-30").tytul + ".";
    l.liniaOstrzezenie = true;
    rysujWzorzec(p, zdarzenia::trescZdarzenia(b14::sprawaE("E-23"), l));
    zapisz(p, "b14-s3-E-23-linia-wciaz");
    std::vector<Wynik_> wl = {b14::wynikE("E-04", {{"data", "12.11"}}), b14::wynikE("E-10/linia", {{"data", "11.11"}})};
    std::vector<sprawy::LiniaZdarzenia> le = sprawy::linieZdarzen(b14::zestaw(), wl, b14::funkcjaTekstu());
    kolejka::WejscieKolejki k;
    k.dzis = dzis;
    k.linieE = &le;
    kolejka::ustawLinie(e, kolejka::kolejkaLinii(k));
    rysujEkranDanych(p, Naglowek{naglowekDniaLewo(e), naglowekDniaPrawo(e)}, trescDnia("S2A", "3", e));
    zapisz(p, "b14-s2a-linia-E-04");
    le.erase(le.begin());
    kolejka::ustawLinie(e, kolejka::kolejkaLinii(k));
    rysujEkranDanych(p, Naglowek{naglowekDniaLewo(e), naglowekDniaPrawo(e)}, trescDnia("S2A", "3", e));
    zapisz(p, "b14-s2a-linia-E-10");
  }
  makiety(p);
  podgladB15(p);
  podgladPortretu(p);
  return 0;
}
