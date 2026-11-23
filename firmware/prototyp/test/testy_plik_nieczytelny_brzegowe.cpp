// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe trwale nieczytelnego pliku (R-AW-40, T-WYM-19), 22.11.2026. Scenariusze spoza T-TEST-520:
//  B1 dziennik baterii (plik tylko dopisywany, czytany raz na dobę): udane dopisanie nie przerywa serii, więc
//        3 NIEKOLEJNE usterki chwilowe w jednej dobie robią sprawę (wymaganie: „3 kolejne wybudzenia”);
//  B2 plik sypialni z zeszłego roku: od roku granicy 120 dni panel go nie czyta, więc sprawa (i wymuszona ocena
//        niepełna) zostaje na zawsze, choć panel tych danych już nie używa;
//  B3 plik zniknął z nośnika (np. formatowanie): sprawa „nieczytelny” zostaje, dopóki coś tego pliku nie zapisze;
//  B4 cztery stare wpisy z licznikiem 1 (ścieżki, których panel już nie czyta) zajmują wszystkie miejsca i nowa
//        trwała usterka innego pliku nigdy nie robi sprawy;
//  B5 nieudany zapis NVS (pełna NVS): skrót ustawiony mimo błędu, więc bez ponowienia; po zaniku zasilania sprawa
//        znika i liczy się od nowa (wymaganie: stan przetrwa zanik zasilania, data „od” bez zmiany);
//  B6 zdrowy panel, 30 dób z wybudzeniami zegarem, przyciskiem (kokpit, U-05) i zanikiem zasilania: ani jednej
//        odmowy zapisu pliku niewczytanego, ani wpisu NVS `nczyt`, ani wiersza logu „pliki nieczytelne” (ma być zielony).
// Testy czerwone zostają czerwone z opisem: program kończy się kodem 0, a liczba czerwonych jest
// w ostatnim wierszu; tryb blokujący: BRZEG_BLOKUJE=1.
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "dzien.h"
#include "ekrany.h"
#include "magazyn.h"
#include "nieczytelne.h"
#include "noc.h"
#include "pliki_pamiec.h"
#include "przebiegi.h"
#include "sprawy.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
static std::vector<std::string> czerwone;
static void sprawdz(bool w, const char* id, const char* opis) {
  if (w) { ok++; printf("  %s: zielony\n", id); return; }
  zle++;
  czerwone.push_back(id);
  printf("  CZERWONY %s: %s\n", id, opis);
}

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* const NOCE = "/dane/noce/2026.csv";
static const char* const BAT = "/log/wybudzenia.csv";
static const char* const SYP = "/dane/sypialnia/2026.csv";
static const char* const KLUCZ_NVS = "cpap/nczyt";

static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900, t.tm_mon = mi - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static uint32_t rano(int h, int m) { return ep(2026, 11, 22, h, m); }

static void przygotuj(Panel& p) {
  p.pliki.montowane = true;
  Magazyn m(p.pliki, nullptr);
  std::vector<std::string> w;
  for (int i = 0; i < 28; i++) {
    Noc x;
    std::string l = dodajDni("2026-10-25", i) + ";1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;";
    if (!parsujNoc(l, 'A', x)) continue;
    x.zrodlo = 'A';
    w.push_back(wierszNocyV5(x));
  }
  m.zapisz("noce", NOCE, w);
  p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  p.pliki.montowane = false;
  p.siec.siecJest = false;
}
// Jak na panelu (src/main.cpp: bezTekstowKoncowych = true): bez tekstów końcowych po śnie. Teksty końcowe
// (Przebieg::tekstyKoncowe(), tylko testy) liczą U-05 po spij(), czytają przy tym dziennik baterii
// (liniePomiaruBaterii()) i wołają odswiezNieczytelne() jeszcze raz, więc zmieniają StanRtc::nieczytelne: wynik testu
// z tekstami nie musi być wynikiem panelu.
static KonfiguracjaPanelu konfPanelu() {
  KonfiguracjaPanelu k = KONF;
  k.bezTekstowKoncowych = true;
  return k;
}
static void wybudzJakPanel(Panel& p, uint32_t t) {
  p.port.log.clear();
  p.zegar.epoch = t;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent dy(p.adaptery(), konfPanelu());
  dy.wybudzenie();
}
static TekstyWybudzenia wybudz(Panel& p, uint32_t t, bool zanik = false) {
  p.port.log.clear();
  p.zegar.epoch = t;
  p.zegar.msTeraz = 0;
  if (zanik) p.start.stan = StanRtc();
  p.start.przyczynaStartu = zanik ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = zanik ? PowodWybudzenia::START : PowodWybudzenia::ZEGAR;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent dy(p.adaptery(), KONF);
  dy.wybudzenie();
  return dy.teksty();
}
static Ekran nacisnij(Panel& p, uint32_t t, Ekran e, uint8_t bit) {
  p.port.log.clear();
  p.zegar.epoch = t;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = {{0, bit}, {300, 0}};
  p.start.stan.ekran = (uint8_t)e;
  p.start.stan.ekranOd = t - 10;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  p.start.budzace = 0;
  p.przyciski.skrypt.clear();
  return (Ekran)p.start.stan.ekran;
}
static std::string wiersz(const TekstyWybudzenia& t, const std::string& nazwa) {
  std::string w;
  for (auto& l : t.linieTestu)
    if (l.compare(0, nazwa.size() + 2, nazwa + ": ") == 0) w = l.substr(nazwa.size() + 2);
  return w;
}
static bool wLogu(const Panel& p, const std::string& t) {
  for (auto& l : p.port.log)
    if (l.find(t) != std::string::npos) return true;
  return false;
}
static bool nvsJest(const Panel& p) { return p.nvs.teksty.count(KLUCZ_NVS) > 0; }
static bool nieczytelneU05(const std::string& v) { return v.compare(0, 11, "nieczytelne") == 0; }

// dziennik baterii czytany raz na dobę, dopisywany w każdym wybudzeniu. Usterka chwilowa dopisania w
// wybudzeniach 2, 4 i 6 tej samej doby (w 3 i 5 dopisanie udane): to nie są „3 kolejne wybudzenia z nieudanym
// odczytem”, a sprawa powstaje, bo udane dopisanie nie jest „udanym odczytem” (Magazyn::wczytanyWTymWybudzeniu()).
static void testR1_DziennikBateriiNiekolejne() {
  printf("B1 dziennik baterii, usterki chwilowe niekolejne (wybudzenia 2, 4, 6 jednej doby)\n");
  Panel p;
  przygotuj(p);
  wybudz(p, rano(6, 0), true);
  wybudz(p, rano(6, 10));
  const bool jest = p.pliki.pliki.count(BAT) > 0;
  printf("   dziennik baterii na nośniku po 2 wybudzeniach: %s\n", jest ? "tak" : "nie");
  int n = 0;
  for (int i = 0; i < 6; i++) {
    const bool usterka = i % 2 == 0;
    if (usterka) p.pliki.nieczytelne.insert(BAT);
    else p.pliki.nieczytelne.erase(BAT);
    wybudzJakPanel(p, rano(7, 10 * i));  // jak na panelu: bez tekstów końcowych (te czytają dziennik baterii)
    n += usterka;
  }
  p.pliki.nieczytelne.erase(BAT);
  const size_t spraw = nieczytelne::sprawy(p.start.stan.nieczytelne).size();
  printf("   po %d niekolejnych usterkach (jak na panelu): spraw w RTC %zu; NVS %s\n", n, spraw,
         nvsJest(p) ? p.nvs.teksty[KLUCZ_NVS].c_str() : "brak");
  sprawdz(jest, "B1a", "atrapa nie zapisała dziennika baterii (scenariusz bez sensu)");
  sprawdz(spraw == 0 && !nvsJest(p), "B1",
          "3 niekolejne usterki chwilowe dopisania (z udanymi dopisaniami między nimi) zrobiły sprawę "
          "`nieczytelne od …` i wymusiły ocenę niepełną");
}

// plik sypialni roku 2026 trwale nieczytelny; w czerwcu 2027 panel czyta sypialnię od roku 2027 (granica
// 120 dni), więc pliku 2026 nie czyta wcale, a sprawa (z wymuszoną oceną niepełną) zostaje.
static void testR2_StaryRokNaZawsze() {
  printf("B2 plik sypialni z zeszłego roku nieczytelny, potem rok bez czytania tego pliku\n");
  Panel p;
  przygotuj(p);
  p.pliki.pliki[SYP] = "treść nieistotna: plik od początku nieczytelny\n";
  p.pliki.nieczytelne.insert(SYP);
  wybudz(p, rano(6, 0), true);
  TekstyWybudzenia t;
  for (int i = 0; i < 3; i++) t = wybudz(p, rano(7, 10 * i));
  const std::string listopad = wiersz(t, "Pamięć panelu");
  t = wybudz(p, ep(2027, 6, 15, 7, 0));
  t = wybudz(p, ep(2027, 6, 15, 7, 10));
  const std::string czerwiec = wiersz(t, "Pamięć panelu");
  printf("   22.11.2026: Pamięć panelu: %s; 15.06.2027: Pamięć panelu: %s\n", listopad.c_str(), czerwiec.c_str());
  sprawdz(nieczytelneU05(listopad), "B2a", "sprawa sypialni 2026 nie powstała (scenariusz bez sensu)");
  sprawdz(!nieczytelneU05(czerwiec), "B2",
          "sprawa pliku, którego panel już nie czyta (sypialnia zeszłego roku), zostaje na zawsze i na zawsze "
          "zdejmuje „W normie”, zdania uspokajające i karty pochwalne");
}

// plik nocy nieczytelny (sprawa), potem znika z nośnika (formatowanie, ręczne usunięcie); bez sieci nic go nie
// tworzy od nowa, więc ekran dalej mówi „nieczytelne”, choć pliku nie ma.
static void testR3_PlikZniknal() {
  printf("B3 plik nocy znika z nośnika przy otwartej sprawie\n");
  Panel p;
  przygotuj(p);
  wybudz(p, rano(6, 0), true);
  p.pliki.nieczytelne.insert(NOCE);
  for (int i = 0; i < 3; i++) wybudz(p, rano(7, 10 * i));
  p.pliki.pliki.erase(NOCE);
  p.pliki.nieczytelne.clear();
  wybudz(p, rano(8, 0));
  TekstyWybudzenia t = wybudz(p, rano(8, 10));
  const std::string w = wiersz(t, "Pamięć panelu");
  printf("   plik usunięty, 2 wybudzenia: Pamięć panelu: %s; NVS %s\n", w.c_str(), nvsJest(p) ? "jest" : "brak");
  sprawdz(!nieczytelneU05(w), "B3", "sprawa `nieczytelne` zostaje, gdy pliku już nie ma (pliku brak ≠ nieczytelny)");
}

// rdzeń. Cztery ścieżki z pojedynczą usterką, których panel potem nie czyta ani nie zapisuje (licznik 1 na
// zawsze), zajmują wszystkie MIEJSC; piąta ścieżka z trwałą usterką nie dostaje miejsca i sprawy nie ma nigdy.
static void testR4_ZajeteMiejsca() {
  printf("B4 cztery stare wpisy z licznikiem 1 blokują nową trwałą usterkę\n");
  nieczytelne::Stan s{};
  auto nigdy = [](const std::string&) { return false; };
  const char* stare[] = {"/dane/sypialnia/2023.csv", "/dane/sypialnia/2024.csv", "/dane/sypialnia/2025.csv",
                         "/dane/noce/2024.csv"};
  uint32_t w = 1;
  for (const char* x : stare) {
    nieczytelne::aktualizuj(s, w, {x}, nigdy, 100 * w);
    w++;
  }
  for (int i = 0; i < 5; i++) {
    nieczytelne::aktualizuj(s, w, {"/dane/czynniki.csv"}, nigdy, 100 * w);
    w++;
  }
  std::vector<nieczytelne::Sprawa> sp = nieczytelne::sprawy(s);
  printf("   spraw po 5 kolejnych usterkach czynników: %zu\n", sp.size());
  sprawdz(sp.size() == 1 && sp[0].sciezka == "/dane/czynniki.csv", "B4",
          "wpisy z licznikiem poniżej progu nie wygasają; przy 4 takich trwała usterka piątego pliku jest niewidoczna");
}

// pełna NVS przy powstaniu sprawy. odswiezNieczytelne() ustawia skrotNvs mimo nieudanego ustawTekst(), więc
// nie ponawia zapisu; po zaniku zasilania sprawy nie ma przez kolejne 3 wybudzenia, a data „od” się przesuwa.
static void testR5_NvsPelna() {
  printf("B5 nieudany zapis NVS przy powstaniu sprawy, potem NVS wolna i zanik zasilania\n");
  Panel p;
  przygotuj(p);
  wybudz(p, rano(6, 0), true);
  p.pliki.nieczytelne.insert(NOCE);
  p.nvs.nieudaneTeksty.insert(KLUCZ_NVS);
  for (int i = 0; i < 3; i++) wybudz(p, rano(7, 10 * i));
  p.nvs.nieudaneTeksty.clear();  // miejsce w NVS się zwolniło
  wybudz(p, rano(7, 40));
  const bool ponowiony = nvsJest(p);
  TekstyWybudzenia t = wybudz(p, rano(7, 50), true);
  const std::string w = wiersz(t, "Pamięć panelu");
  printf("   zapis NVS ponowiony: %s; po zaniku zasilania: Pamięć panelu: %s\n", ponowiony ? "tak" : "nie", w.c_str());
  sprawdz(ponowiony, "B5a", "nieudany zapis `nczyt` nie jest ponawiany (skrotNvs ustawiony mimo błędu)");
  sprawdz(w == "nieczytelne od 22.11", "B5",
          "po nieudanym zapisie NVS i zaniku zasilania sprawa znika (wymaganie: przetrwa zanik, data bez zmian)");
}

// zdrowy panel, 30 dób (wybudzenia zegarem rano, w dzień i wieczorem, co drugi dzień przejście przyciskiem
// przez kokpit i U-05, zanik zasilania 10. doby, przełom miesiąca). Bez usterek nośnika: ani odmowy zapisu pliku niewczytanego, ani
// sprawy, ani wpisu NVS.
static void testR6_ZdrowyPanel() {
  printf("B6 zdrowy panel, 30 dób, bez usterek nośnika\n");
  Panel p;
  przygotuj(p);
  wybudz(p, ep(2026, 11, 20, 6, 0), true);
  bool odmowa = false, sprawa = false, nvs = false;
  int wybudzen = 0;
  for (int d = 0; d < 30; d++) {
    const std::string dz = dodajDni("2026-11-20", d);
    const int r = std::atoi(dz.substr(0, 4).c_str()), m = std::atoi(dz.substr(5, 2).c_str()),
              dd = std::atoi(dz.substr(8, 2).c_str());
    for (int h : {7, 13, 22}) {
      TekstyWybudzenia t = wybudz(p, ep(r, m, dd, h, 0), d == 10 && h == 7);
      wybudzen++;
      odmowa = odmowa || wLogu(p, "a nie był w tym wybudzeniu przeczytany");
      sprawa = sprawa || wLogu(p, "pliki nieczytelne") || nieczytelneU05(wiersz(t, "Pamięć panelu"));
      nvs = nvs || nvsJest(p);
    }
    if (d % 2 == 0) {
      Ekran e = Ekran::DZIEN;
      for (int i = 0; i < 4; i++) {
        e = nacisnij(p, ep(r, m, dd, 8, 1 + i), e, BIT_PRAWY);
        wybudzen++;
        odmowa = odmowa || wLogu(p, "a nie był w tym wybudzeniu przeczytany");
      }
      nacisnij(p, ep(r, m, dd, 8, 10), Ekran::FORMULARZ, BIT_PRAWY);
      wybudzen++;
      odmowa = odmowa || wLogu(p, "a nie był w tym wybudzeniu przeczytany");
      sprawa = sprawa || wLogu(p, "pliki nieczytelne");
      nvs = nvs || nvsJest(p);
    }
  }
  printf("   %d wybudzeń: odmowa zapisu %s, sprawa %s, NVS nczyt %s\n", wybudzen, odmowa ? "TAK" : "nie",
         sprawa ? "TAK" : "nie", nvs ? "TAK" : "nie");
  sprawdz(!odmowa && !sprawa && !nvs, "B6", "fałszywa sprawa albo odmowa zapisu na zdrowym panelu");
}

// N1/N2 z T-TEST-520 jak na panelu (bez tekstów końcowych): czy sprawa nocy powstaje po 3 wybudzeniach
// także wtedy, gdy nikt nie liczy U-05 po śnie (wynik T-TEST-520 nie zależy od kodu wołanego tylko w testach).
static void testR7_BezTekstowKoncowych() {
  printf("B7 N2 z T-TEST-520 bez tekstów końcowych (jak na panelu)\n");
  Panel p;
  przygotuj(p);
  wybudz(p, rano(6, 50), true);
  p.pliki.nieczytelne.insert(NOCE);
  size_t po[3];
  for (int i = 0; i < 3; i++) {
    wybudzJakPanel(p, rano(7, 10 * i));
    po[i] = nieczytelne::sprawy(p.start.stan.nieczytelne).size();
  }
  printf("   spraw w RTC po wybudzeniach 1, 2, 3: %zu %zu %zu; NVS %s\n", po[0], po[1], po[2], nvsJest(p) ? "jest" : "brak");
  sprawdz(po[0] == 0 && po[1] == 0 && po[2] == 1 && nvsJest(p), "B7", "sprawa nocy nie powstaje po 3 wybudzeniach jak na panelu");
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  printf("Testy brzegowe: scenariusze spoza T-TEST-520\n");
  testR1_DziennikBateriiNiekolejne();
  testR2_StaryRokNaZawsze();
  testR3_PlikZniknal();
  testR4_ZajeteMiejsca();
  testR5_NvsPelna();
  testR6_ZdrowyPanel();
  testR7_BezTekstowKoncowych();
  std::string c;
  for (auto& x : czerwone) c += (c.empty() ? "" : ", ") + x;
  printf("testy_plik_nieczytelny_brzegowe: %d zielonych, %d czerwonych%s%s\n", ok, zle, zle ? ": " : "", c.c_str());
  const char* b = std::getenv("BRZEG_BLOKUJE");
  return b && std::string(b) == "1" && zle ? 1 : 0;
}
