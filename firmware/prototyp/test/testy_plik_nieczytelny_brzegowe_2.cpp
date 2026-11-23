// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe trwale nieczytelnego pliku, część 2 (R-AW-40), 23.11.2026. Scenariusze:
//  C1 cztery sprawy plików spoza oceny zajmują wszystkie miejsca; potem trwale nieczytelne czynniki (plik oceny)
//         nie dostają miejsca, więc ocena nie jest wymuszona niepełna (rdzeń);
//  C2 cały nośnik nieczytelny (każdy plik): po 3 wybudzeniach sprawa pliku oceny jest (dyrygent, jak na panelu);
//  C3 stary roczny plik nocy (2024) trwale nieczytelny, noce 2026 czytelne: plikOceny() uznaje go za plik oceny,
//         więc ocena jest niepełna na zawsze, choć okna oceny (≤ 120 dni) go nie obejmują;
//  C4 trwale pełna NVS: ile prób zapisu `nczyt` w jednym wybudzeniu (każda próba to praca flash);
//  C5 plik trwale nieczytelny czytany co 4 doby (rzadziej niż WYGASA_S): sprawa nie powstaje nigdy (rdzeń).
// Czerwone zostają czerwone z opisem; kod 0, blokujący z BRZEG_BLOKUJE=1.
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "magazyn.h"
#include "nieczytelne.h"
#include "noc.h"
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
static const char* const KLUCZ_NVS = "cpap/nczyt";
static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900, t.tm_mon = mi - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static uint32_t rano(int h, int m) { return ep(2026, 11, 22, h, m); }
static std::vector<std::string> noce(const std::string& od, int n) {
  std::vector<std::string> w;
  for (int i = 0; i < n; i++) {
    Noc x;
    std::string l = dodajDni(od, i) + ";1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;";
    if (!parsujNoc(l, 'A', x)) continue;
    x.zrodlo = 'A';
    w.push_back(wierszNocyV5(x));
  }
  return w;
}
static void przygotuj(Panel& p) {
  p.pliki.montowane = true;
  Magazyn m(p.pliki, nullptr);
  m.zapisz("noce", "/dane/noce/2026.csv", noce("2026-10-25", 28));
  p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  p.pliki.montowane = false;
  p.siec.siecJest = false;
}
// jak na panelu: bez tekstów końcowych
static void wybudz(Panel& p, uint32_t t, bool zanik = false) {
  p.port.log.clear();
  p.zegar.epoch = t;
  p.zegar.msTeraz = 0;
  if (zanik) p.start.stan = StanRtc();
  p.start.przyczynaStartu = zanik ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = zanik ? PowodWybudzenia::START : PowodWybudzenia::ZEGAR;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  KonfiguracjaPanelu k = KONF;
  k.bezTekstowKoncowych = true;
  Dyrygent dy(p.adaptery(), k);
  dy.wybudzenie();
}
static int wLogu(const Panel& p, const std::string& t) {
  int n = 0;
  for (auto& l : p.port.log) n += l.find(t) != std::string::npos;
  return n;
}
static bool sprawaOceny(const nieczytelne::Stan& s) {
  for (auto& x : nieczytelne::sprawy(s))
    if (nieczytelne::plikOceny(x.sciezka)) return true;
  return false;
}
static const std::function<bool(const std::string&)> NIGDY = [](const std::string&) { return false; };
static const std::function<bool(const std::string&)> ZAWSZE_JEST = [](const std::string&) { return true; };

static void testR1_MiejscaZajetePrzezPlikiSpozaOceny() {
  printf("C1 4 sprawy spoza oceny, potem trwale nieczytelne czynniki\n");
  nieczytelne::Stan s{};
  const std::vector<std::string> poza = {"/dane/sypialnia/2026.csv", "/log/wybudzenia.csv", "/dane/humor.csv",
                                         "/dane/prognoza.csv"};
  uint32_t w = 1, t = 1000;
  for (int i = 0; i < 3; i++, w++, t += 600) nieczytelne::aktualizuj(s, w, poza, NIGDY, ZAWSZE_JEST, t);
  const size_t przed = nieczytelne::sprawy(s).size();
  std::vector<std::string> wszystkie = poza;
  wszystkie.push_back("/dane/czynniki.csv");
  for (int i = 0; i < 5; i++, w++, t += 600) nieczytelne::aktualizuj(s, w, wszystkie, NIGDY, ZAWSZE_JEST, t);
  printf("   spraw spoza oceny: %zu; po 5 wybudzeniach z czynnikami: sprawa pliku oceny %s\n", przed,
         sprawaOceny(s) ? "jest" : "BRAK");
  sprawdz(sprawaOceny(s), "C1",
          "przy 4 sprawach spoza oceny trwale nieczytelny plik oceny nie dostaje miejsca: ocena nie jest niepełna");
}

static void testR2_CalyNosnik() {
  printf("C2 cały nośnik nieczytelny\n");
  Panel p;
  przygotuj(p);
  wybudz(p, rano(6, 0), true);
  wybudz(p, rano(6, 10));
  for (auto& kv : p.pliki.pliki)
    if (kv.first != "/konfiguracja.txt") p.pliki.nieczytelne.insert(kv.first);
  for (int i = 0; i < 3; i++) wybudz(p, rano(7, 10 * i));
  std::string l;
  for (auto& x : nieczytelne::sprawy(p.start.stan.nieczytelne)) l += x.sciezka + " ";
  printf("   plików nieczytelnych %zu; sprawy: %s\n", p.pliki.nieczytelne.size(), l.c_str());
  sprawdz(sprawaOceny(p.start.stan.nieczytelne), "C2", "przy całym nośniku nieczytelnym brak sprawy pliku oceny");
}

static void testR3_StaryRokNocy() {
  printf("C3 noce 2024 trwale nieczytelne, noce 2026 czytelne\n");
  Panel p;
  przygotuj(p);
  {
    p.pliki.montowane = true;
    Magazyn m(p.pliki, nullptr);
    m.zapisz("noce", "/dane/noce/2024.csv", noce("2024-03-01", 20));
    p.pliki.montowane = false;
  }
  wybudz(p, rano(6, 0), true);
  p.pliki.nieczytelne.insert("/dane/noce/2024.csv");
  for (int i = 0; i < 3; i++) wybudz(p, rano(7, 10 * i));
  const bool sprawa = sprawaOceny(p.start.stan.nieczytelne);
  wybudz(p, rano(8, 0));
  const int wymuszona = wLogu(p, "ocena: niepełna, nieczytelny plik oceny");
  printf("   sprawa noce 2024 jako plik oceny: %s; wiersz „ocena: niepełna” w wybudzeniu: %d\n", sprawa ? "tak" : "nie",
         wymuszona);
  // Od 23.11.2026 oczekiwanie odwrócone. Okna reguł silnika (reguly.cpp noceOkna(): ostatnie N nocy danego
  // rodzaju bez granicy dat) i S-K-19 (kontekst_sk.cpp: 90 dni przed siódmą od końca nocą) sięgają po noce z dowolnego
  // roku, gdy historia jest rzadka, więc stary rok nocy jest plikiem oceny i ocena niepełna jest poprawna (ADR-0029 pkt 7).
  sprawdz(sprawa && wymuszona > 0, "C3",
          "stary rok nocy powinien być plikiem oceny (okna reguł bez granicy dat), a ocena przy nim niepełna");
}

static void testR4_NvsPelnaNaStale() {
  printf("C4 trwale pełna NVS: próby zapisu `nczyt` na wybudzenie\n");
  Panel p;
  przygotuj(p);
  wybudz(p, rano(6, 0), true);
  p.pliki.nieczytelne.insert("/dane/noce/2026.csv");
  p.nvs.nieudaneTeksty.insert(KLUCZ_NVS);
  int maks = 0, suma = 0;
  for (int i = 0; i < 6; i++) {
    wybudz(p, rano(7, 10 * i));
    int n = wLogu(p, "zapis NVS nieudany");
    suma += n;
    if (n > maks) maks = n;
  }
  printf("   6 wybudzeń: prób nieudanych łącznie %d, najwięcej w jednym %d\n", suma, maks);
  sprawdz(maks <= 1, "C4", "przy pełnej NVS więcej niż jedna próba zapisu `nczyt` w jednym wybudzeniu");
}

static void testR5_RzadkiOdczyt() {
  printf("C5 plik trwale nieczytelny, czytany co 4 doby (inne wybudzenia co 8 h)\n");
  nieczytelne::Stan s{};
  const std::vector<std::string> u = {"/dane/ab-proby.csv"};
  bool kiedykolwiek = false;
  uint32_t w = 1;
  for (int h = 0; h < 40 * 24; h += 8, w++) {
    const bool czyta = h % 96 == 0;
    nieczytelne::aktualizuj(s, w, czyta ? u : std::vector<std::string>{}, NIGDY, ZAWSZE_JEST, 1000000 + h * 3600u);
    kiedykolwiek = kiedykolwiek || !nieczytelne::sprawy(s).empty();
  }
  printf("   40 dób: sprawa %s\n", kiedykolwiek ? "była" : "NIGDY");
  sprawdz(kiedykolwiek, "C5", "plik czytany rzadziej niż co 3 doby: trwała usterka nigdy nie staje się sprawą");
}

// Próby złamania zasady „plik oceny zawsze dostaje miejsce” na rdzeniu (nieczytelne::aktualizuj() + ocenaNiepelna()), każda trwała usterka pliku
// oceny ≥ próg ma dawać ocenę niepełną niezależnie od zajętości miejsc i kolejności zdarzeń.
static void testC6_PlikOcenyZMiejscem() {
  printf("C6 plik oceny zawsze z miejscem, na rdzeniu: miejsca, kolejność, powrót wypartej sprawy, zanik zasilania\n");
  auto krok = [](nieczytelne::Stan& s, uint32_t& w, uint32_t& t, const std::vector<std::string>& u) {
    nieczytelne::aktualizuj(s, w, u, NIGDY, ZAWSZE_JEST, t);
    const bool n = nieczytelne::ocenaNiepelna(s, u);
    w++;
    t += 600;
    return n;
  };
  bool zawsze = true;
  // (a) 5 lat nocy nieczytelnych naraz od pierwszego wybudzenia (4 miejsca): piąty rok bez wpisu, ocena od razu niepełna
  {
    nieczytelne::Stan s{};
    uint32_t w = 1, t = 1000;
    const std::vector<std::string> u = {"/dane/noce/2021.csv", "/dane/noce/2022.csv", "/dane/noce/2023.csv",
                                        "/dane/noce/2024.csv", "/dane/noce/2025.csv"};
    for (int i = 0; i < 6; i++) zawsze = krok(s, w, t, u) && zawsze;
    printf("   (a) 5 lat nocy: ocena niepełna w każdym wybudzeniu: %s\n", zawsze ? "tak" : "NIE");
  }
  // (b) 4 sprawy spoza oceny, potem czynniki: wypierają sprawę; wyparta wraca co wybudzenie i nie wypiera czynników
  bool b = true;
  {
    nieczytelne::Stan s{};
    uint32_t w = 1, t = 1000;
    const std::vector<std::string> poza = {"/dane/sypialnia/2026.csv", "/log/wybudzenia.csv", "/dane/humor.csv",
                                           "/dane/prognoza.csv"};
    for (int i = 0; i < 3; i++) krok(s, w, t, poza);
    std::vector<std::string> u = poza;
    u.push_back("/dane/czynniki.csv");
    // wybudzenia 1 i 2 z czynnikami: wpis poniżej progu (usterka może być chwilowa), od 3. ocena ma być niepełna
    std::string seria;
    for (int i = 0; i < 8; i++) {
      const bool n = krok(s, w, t, u);
      seria += n ? "N" : "p";
      if (i >= 2) b = n && b;
    }
    b = b && sprawaOceny(s);
    // (c) zanik zasilania: odtworzenie z tekstu NVS z tą listą spraw, czynniki dalej nieczytelne
    nieczytelne::Stan r{};
    nieczytelne::odtworz(r, nieczytelne::tekstNvs(s));
    seria += " | zanik | ";
    uint32_t w2 = 1, t2 = t;
    for (int i = 0; i < 3; i++) {
      const bool n = krok(r, w2, t2, u);
      seria += n ? "N" : "p";
      b = n && b;
    }
    printf("   (b, c) 4 sprawy spoza oceny + czynniki (N = niepełna, p = pełna): %s; od 3. wybudzenia i po zaniku "
           "zawsze niepełna: %s\n", seria.c_str(), b ? "tak" : "NIE");
  }
  // (d) kolejność odwrotna: czynniki najpierw (sprawa), potem 4 pliki spoza oceny: czynniki zostają
  bool d = true;
  {
    nieczytelne::Stan s{};
    uint32_t w = 1, t = 1000;
    for (int i = 0; i < 3; i++) krok(s, w, t, {"/dane/czynniki.csv"});
    const std::vector<std::string> u = {"/dane/sypialnia/2026.csv", "/log/wybudzenia.csv", "/dane/humor.csv",
                                        "/dane/prognoza.csv", "/dane/czynniki.csv"};
    for (int i = 0; i < 8; i++) d = krok(s, w, t, u) && d;
    printf("   (d) czynniki pierwsze, potem 4 spoza oceny: ocena niepełna zawsze: %s\n", d ? "tak" : "NIE");
  }
  sprawdz(zawsze && b && d, "C6", "trwale nieczytelny plik oceny z oceną pełną w którymś wybudzeniu ");
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  printf("Testy brzegowe trwale nieczytelnego pliku, część 2\n");
  testR1_MiejscaZajetePrzezPlikiSpozaOceny();
  testR2_CalyNosnik();
  testR3_StaryRokNocy();
  testR4_NvsPelnaNaStale();
  testR5_RzadkiOdczyt();
  testC6_PlikOcenyZMiejscem();
  std::string c;
  for (auto& x : czerwone) c += (c.empty() ? "" : ", ") + x;
  printf("testy_plik_nieczytelny_brzegowe_2: %d zielonych, %d czerwonych%s%s\n", ok, zle, zle ? ": " : "", c.c_str());
  const char* b = std::getenv("BRZEG_BLOKUJE");
  return b && std::string(b) == "1" && zle ? 1 : 0;
}
