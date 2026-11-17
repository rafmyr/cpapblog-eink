// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe belki bez napisu `kabel` (decyzja właściciela projektu 16.11.2026: „usuń napis kabel przy
// ładowaniu”; spec 5.4.17 R-EKR-66, ADR-0043 „Zmiana 2026-11-16”). Testy pisane z kryteriów
// zmiany, nie z kodu:
//  (1) belki wszystkich rodzajów (ekran dnia, Portret, K1, K2, „Sypialnia · 30 nocy”, belki ogólne U-01, U-05, U-07,
//      Otwarte sprawy, Pytanie, Raport) × dane nieaktualne × klepsydra × nowa wersja × oceny × romb, najdłuższe środki:
//      na kablu w prawym polu dokładnie ikona z kreską przy x 735…771, za nią nic, przed nią odstęp; tekst środka bez
//      podwójnej spacji i bez wiszącego ` ·`; kabel nigdy nie zabiera środkowi ani znakom więcej niż `100%`;
//  (2) pełne ekrany: na kablu treść pod belką (y 44…479) piksel w piksel jak przy `100%` (układ się nie rozjeżdża);
//  (3) liczby ze spec 5.4.17 (tabela „Miejsce na środek”, liczniki ustępowania) jako twarde sprawdzenia, `100%`
//      najciaśniejszym stanem baterii (testy „ciasno” po przestawieniu z kabla nadal sprawdzają najgorszy przypadek);
//  (4) dyrygent z atrapami: kabel ↔ bateria ↔ stan nieznany między klatkami, flagi baterii w RTC, liczba odświeżeń.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_belka_bez_kabla_brzegowe).
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "aktualizacja.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "dzien.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "kokpit.h"
#include "noc.h"
#include "start.h"
#include "sypialnia_ekran.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
#define ROWNE_L(a, b)                                                                                    \
  do {                                                                                                   \
    long long _a = (long long)(a), _b = (long long)(b);                                                  \
    if (_a == _b) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); }  \
  } while (0)

static const char* STREFA = "CET-1CEST,M3.5.0,M10.5.0/3";

struct PlotnoR : Plotno {
  std::vector<unsigned char> px = std::vector<unsigned char>(800 * 480, 0);
  int poza = 0;
  void piksel(int x, int y, bool c) override {
    if (x < 0 || y < 0 || x >= 800 || y >= 480) { poza++; return; }
    px[(size_t)(y * 800 + x)] = c ? 1 : 0;
  }
  bool cz(int x, int y) const { return px[(size_t)(y * 800 + x)] != 0; }
  int czarne(int x0, int y0, int x1, int y1) const {  // [x0, x1) × [y0, y1)
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[(size_t)(y * 800 + x)];
    return n;
  }
};
static BateriaEkranu bat(int p, bool kabel = false) {
  BateriaEkranu b;
  b.procent = p;
  b.kabel = kabel;
  return b;
}
static const int IKONA = BATERIA_W + BATERIA_NOSEK_W;  // 37 px
static const int X_IKONY_KABLA = BELKA_XP - IKONA;     // 735

// Wzorzec: sama ikona „na kablu” z lewym górnym rogiem obrysu w (735, 12) na jasnym tle.
static const PlotnoR& wzorIkonyKabla() {
  static PlotnoR w;
  static bool gotowy = false;
  if (!gotowy) {
    ikonaBaterii(w, X_IKONY_KABLA, BELKA_BAZA - BATERIA_H, 0, true);
    gotowy = true;
  }
  return w;
}
// Pas x 735…799, y 0…40 dokładnie taki jak sama ikona kabla (nic nie wchodzi na ikonę, nic za noskiem).
static bool ikonaKablaSama(const PlotnoR& p) {
  const PlotnoR& w = wzorIkonyKabla();
  for (int y = 0; y < BELKA_LINIA_Y; y++)
    for (int x = X_IKONY_KABLA; x < 800; x++)
      if (p.cz(x, y) != w.cz(x, y)) return false;
  return true;
}
// To samo na buforze klatki z atrapy wyświetlacza (1 bit na piksel, 100 B na wiersz, 1 = czarny).
static bool czK(const std::vector<uint8_t>& k, int x, int y) {
  return (k[(size_t)(y * 100 + x / 8)] & (uint8_t)(0x80 >> (x & 7))) != 0;
}
// Klatka z belką: wiersze 41…43 czarne na całej szerokości, wiersz 40 jasny przy lewym marginesie (S9 i slot
// dzienny belki nie mają: linia na 46 albo 52).
static bool klatkaZBelka(const std::vector<uint8_t>& k) {
  for (int y = BELKA_LINIA_Y; y < BELKA_LINIA_Y + BELKA_LINIA_GRUBOSC; y++)
    for (int x = 0; x < 800; x++)
      if (!czK(k, x, y)) return false;
  for (int x = 0; x < BELKA_X; x++)
    if (czK(k, x, BELKA_LINIA_Y - 1)) return false;
  return true;
}
static bool klatkaZIkonaKabla(const std::vector<uint8_t>& k) {
  const PlotnoR& w = wzorIkonyKabla();
  for (int y = 0; y < BELKA_LINIA_Y; y++)
    for (int x = X_IKONY_KABLA; x < 800; x++)
      if (czK(k, x, y) != w.cz(x, y)) return false;
  return true;
}
// Tekst bez podwójnej spacji, bez spacji i separatora na brzegach.
static bool tekstCzysty(const std::string& t) {
  if (t.empty()) return true;
  if (t.find("  ") != std::string::npos) return false;
  if (t.front() == ' ' || t.back() == ' ') return false;
  const std::string kropka = "·";
  if (t.size() >= kropka.size() && t.compare(t.size() - kropka.size(), kropka.size(), kropka) == 0) return false;
  if (t.compare(0, kropka.size(), kropka) == 0) return false;
  return true;
}
// Pozycja wybranego tekstu na liście ustępowania (0 = pełny środek); -1 = skrócony, spoza listy.
static int pozycjaUstepowania(const Belka& b, const std::string& t) {
  if (t == b.srodek) return 0;
  for (size_t i = 0; i < b.ustepuje.size(); i++)
    if (b.ustepuje[i] == t) return (int)i + 1;
  return -1;
}
static std::vector<Noc> nocDo(const std::string& ostatnia, int n = 3) {
  std::vector<Noc> h;
  for (int i = n - 1; i >= 0; i--) {
    Noc x;
    x.data = dodajDni(ostatnia, -i);
    x.wazna = true;
    x.m01 = 420;
    x.ahi = 2.0f;
    h.push_back(x);
  }
  return h;
}

// ============================================================================================================
// (1) Belki × znaki × kabel / 100%
// ============================================================================================================
static Belka belkaPrzypadku(int ekr, bool nieakt, int ocena, bool romb, const std::string& dt, const char* dzT) {
  static std::vector<Noc> h;
  const std::string dzis = "2026-11-14";
  h = nieakt ? nocDo("2026-11-13") : nocDo("2026-11-14");
  if (ocena == 2 && !nieakt) h.back().wp95 = 30.0f;  // noc niepewna: ocena ograniczona
  DaneDnia dd;
  dd.dzis = dzis;
  dd.hist = &h;
  dd.godzinaImportu = "08:08";
  switch (ekr) {
    case 0: return belkaDnia(dd);
    case 1: return belkaPortretu(dd);
    case 2: {
      DaneK1 k;
      k.dataNocy = dt;
      k.dzienNocy = dzT;
      k.ubiegla = !nieakt;
      k.godzinaDanych = nieakt ? "" : "08:08";
      k.romb = romb;
      k.ocena = ocena == 1 ? "ocena częściowa" : ocena == 2 ? "ocena ograniczona" : "";
      return belkaK1(k);
    }
    case 3: {
      DaneK2 k;
      k.zakres = "28.10–03.11";
      k.godzinaDanych = "08:08";
      return belkaK2(k);
    }
    case 4: {
      DaneSyp30 k;
      k.zakres = "16.10–14.11";
      return belkaSyp30(k);
    }
    case 5: return Belka{"Ustawienia 3 / 3", ""};
    case 6: return Belka{"Test sprzętu", "sprawdzono 28.08 08:08"};
    case 7: return Belka{"Aktualizacja", "nie udało się: za słaba bateria albo przerwane połączenie z serwerem"};
    case 8: return Belka{"Otwarte sprawy", "3 sprawy · najstarsza z 28.08"};
    case 9: return Belka{"Pytanie", "śr 28.08 · odczyt 08:08"};
    default: return Belka{"Raport", "28.08–30.12 · odczyt 08:08 · ocena ograniczona · dane z 27.08"};
  }
}

static void testBelkiKabel() {
  printf("(1) belki × znaki × kabel / 100%%:\n");
  const char* dniT[] = {"pn", "wt", "śr", "cz", "pt", "sb", "nd"};
  const std::vector<std::string> daty = {"28.08", "03.03", "11.11", "30.12", "08.08"};
  long przyp = 0, zlaIkona = 0, brakOdstepu = 0, brudnyTekst = 0, kabelGorzej = 0, lewaRozna = 0, wylazi = 0;
  long kabelLepiej = 0;
  for (int ekr = 0; ekr <= 10; ekr++)
    for (int nieakt = 0; nieakt < 2; nieakt++)
      for (int ocena = 0; ocena < 3; ocena++)
        for (int romb = 0; romb < 2; romb++)
          for (int kl = 0; kl < 2; kl++)
            for (int wer = 0; wer < 2; wer++)
              for (const std::string& dt : daty)
                for (int dz = 0; dz < 7; dz++) {
                  if (ekr != 2 && (romb || ocena == 1)) continue;
                  if (ekr != 2 && dz > 0) continue;
                  ZnakiBelki z;
                  z.nieaktualne = nieakt;
                  z.daneZ = dt;
                  z.pobieram = kl;
                  z.nowaWersja = wer;
                  ustawZnakiBelki(z);
                  // stan 100%
                  ustawBaterieEkranu(bat(100));
                  const Belka b100 = belkaPrzypadku(ekr, nieakt, ocena, romb, dt, dniT[dz]);
                  const UkladSrodka u100 = ukladSrodkaBelki(b100);
                  const bool zn100 = znakWersjiNaBelce(b100);
                  const int xp100 = xPrawegoPolaBelki(b100);
                  PlotnoR p100;
                  belka(p100, b100);
                  // stan kabel
                  ustawBaterieEkranu(bat(60, true));
                  const Belka bk = belkaPrzypadku(ekr, nieakt, ocena, romb, dt, dniT[dz]);
                  const UkladSrodka uk = ukladSrodkaBelki(bk);
                  const bool znk = znakWersjiNaBelce(bk);
                  const int xpk = xPrawegoPolaBelki(bk);
                  PlotnoR pk;
                  belka(pk, bk);
                  przyp++;
                  if (pk.poza) wylazi++;
                  // prawe pole: dokładnie ikona kabla przy 735…771, nic za noskiem, nic na ikonie
                  if (!ikonaKablaSama(pk)) {
                    if (zlaIkona < 3) printf("  zła ikona: %s / %s\n", bk.nazwa.c_str(), uk.tekst.c_str());
                    zlaIkona++;
                  }
                  // przed prawym polem 16 px pustych; przy znaku wersji 10 px między znakiem a ikoną
                  if (pk.czarne(xpk - BELKA_ODSTEP_PRAWEGO, 0, xpk, BELKA_LINIA_Y) != 0) brakOdstepu++;
                  if (znk && pk.czarne(X_IKONY_KABLA - BELKA_ODSTEP_ZNAKU, 0, X_IKONY_KABLA, BELKA_LINIA_Y) != 0)
                    brakOdstepu++;
                  if (znk && pk.czarne(X_IKONY_KABLA - BELKA_ODSTEP_ZNAKU - ZNAK_WERSJI_D, 0,
                                       X_IKONY_KABLA - BELKA_ODSTEP_ZNAKU, BELKA_LINIA_Y) == 0)
                    brakOdstepu++;  // znak miał być, a go nie ma
                  if (!znk && xpk != X_IKONY_KABLA) brakOdstepu++;  // bez znaku prawe pole zaczyna się na ikonie
                  // teksty bez podwójnej spacji i bez wiszącego separatora
                  if (!tekstCzysty(uk.tekst) || !tekstCzysty(bk.poPolu) || !tekstCzysty(bk.alarm)) {
                    if (brudnyTekst < 3) printf("  tekst: „%s” / „%s”\n", uk.tekst.c_str(), bk.poPolu.c_str());
                    brudnyTekst++;
                  }
                  // kabel (węższe prawe pole) nigdy nie daje mniej niż `100%`
                  const int pk_ = pozycjaUstepowania(bk, uk.tekst), p1_ = pozycjaUstepowania(b100, u100.tekst);
                  bool gorzej = (u100.klepsydra && !uk.klepsydra) || (u100.slowo && !uk.slowo) || (zn100 && !znk) ||
                                (pk_ >= 0 && p1_ >= 0 && pk_ > p1_) || (pk_ < 0 && p1_ >= 0);
                  if (gorzej) {
                    if (kabelGorzej < 3) printf("  kabel gorzej niż 100%%: %s „%s”\n", bk.nazwa.c_str(), uk.tekst.c_str());
                    kabelGorzej++;
                  }
                  if (!gorzej && (uk.tekst != u100.tekst || uk.klepsydra != u100.klepsydra || uk.slowo != u100.slowo ||
                                  znk != zn100))
                    kabelLepiej++;
                  // ten sam układ środka: lewa część belki piksel w piksel jak przy 100%
                  if (uk.tekst == u100.tekst && uk.klepsydra == u100.klepsydra && uk.slowo == u100.slowo) {
                    for (int y = 0; y < 44; y++)
                      for (int x = 0; x < xp100 - BELKA_ODSTEP_PRAWEGO; x++)
                        if (pk.cz(x, y) != p100.cz(x, y)) { lewaRozna++; y = 44; break; }
                  }
                }
  printf("  przypadków %ld: zła ikona %ld, brak odstępu %ld, tekst z podwójną spacją albo wiszącym ` ·` %ld, kabel "
         "gorzej niż 100%% %ld, kabel lepiej niż 100%% %ld, lewa część różna przy tym samym układzie %ld, piksele poza "
         "ekranem %ld\n",
         przyp, zlaIkona, brakOdstepu, brudnyTekst, kabelGorzej, kabelLepiej, lewaRozna, wylazi);
  SPRAWDZ(przyp > 1000);
  ROWNE_L(zlaIkona, 0);
  ROWNE_L(brakOdstepu, 0);
  ROWNE_L(brudnyTekst, 0);
  ROWNE_L(kabelGorzej, 0);
  ROWNE_L(lewaRozna, 0);
  ROWNE_L(wylazi, 0);
  ustawZnakiBelki(ZnakiBelki());
  ustawBaterieEkranu(BateriaEkranu());
}

// tekstBaterii / tekstNiskiejBaterii na kablu: brak wiszącego separatora tam, gdzie tekst może się pojawić.
static void testTekstyBaterii() {
  printf("(1b) teksty baterii:\n");
  SPRAWDZ(tekstBaterii(bat(60, true)).empty());
  SPRAWDZ(tekstBaterii(bat(-1, true)).empty());
  SPRAWDZ(tekstBaterii(bat(100, true)).empty());
  ROWNE_L(szerPolaBaterii(bat(-1, true)), IKONA);
  // bez zmian poza kablem
  SPRAWDZ(tekstBaterii(bat(78)) == "78%" && tekstBaterii(bat(-1)) == "–" && tekstBaterii(bat(150)) == "100%");
  SPRAWDZ(tekstNiskiejBaterii(bat(18)) == "18% · podłącz kabel");
  // Stan „niska” na kablu nie występuje (bateriaNiska() zwraca false), więc stopka R-KON-69 go nie rysuje. Gdyby
  // jednak doszło do takiego wywołania, tekst zaczynałby się od wiszącego separatora: tylko informacja (P3).
  SPRAWDZ(!bateriaNiska(5, true, true) && !bateriaNiska(-1, true, true));
  const std::string t = tekstNiskiejBaterii(bat(18, true));
  printf("  tekstNiskiejBaterii na kablu (stan nieosiągalny): „%s”%s\n", t.c_str(),
         tekstCzysty(t) ? "" : "  <-- wiszący separator, gdyby stan był osiągalny");
}

// ============================================================================================================
// (2) Pełne ekrany na kablu: belka z samą ikoną, treść pod belką jak przy 100%
// ============================================================================================================
static void testPelneEkrany() {
  printf("(2) pełne ekrany na kablu:\n");
  struct Ekr {
    const char* nazwa;
    std::function<void(Plotno&)> rysuj;
  };
  static std::vector<Noc> h = nocDo("2026-11-13", 30);
  DaneDnia dd;
  dd.dzis = "2026-11-14";
  dd.hist = &h;
  dd.godzinaImportu = "08:08";
  std::vector<Ekr> ekrany = {
      {"ekran dnia S2a", [&](Plotno& p) { rysujEkranDanych(p, belkaDnia(dd), trescDnia("S2a", "", dd)); }},
      {"ekran dnia S3 (negatyw)", [&](Plotno& p) { rysujEkranDanych(p, belkaDnia(dd), trescDnia("S3", "", dd)); }},
      {"karta", [&](Plotno& p) {
         TrescEkranu k;
         k.etykieta = "OSTATNIA NOC";
         k.tytul = "Noc: 7 h 30 z maską.";
         rysujKarte(p, belkaDnia(dd), k, "");
       }},
      {"K1", [&](Plotno& p) {
         DaneK1 k;
         k.dataNocy = "28.08";
         k.dzienNocy = "śr";
         k.ubiegla = true;
         k.godzinaDanych = "08:08";
         k.romb = true;
         k.ocena = "ocena ograniczona";
         for (auto& w : k.w) {  // Linia30 nie ma wartości domyślnych (src/kokpit.h): bez tego linia trendu losowa
           std::fill(w.linia.v, w.linia.v + 30, 0.0f);
           std::fill(w.linia.ma, w.linia.ma + 30, false);
         }
         rysujK1(p, k);
       }},
      {"K2", [&](Plotno& p) {
         DaneK2 k;
         k.zakres = "28.10–03.11";
         k.godzinaDanych = "08:08";
         rysujK2(p, k);
       }},
      {"Sypialnia · 30 nocy", [&](Plotno& p) {
         DaneSyp30 k;
         k.zakres = "16.10–14.11";
         rysujSyp30(p, k);
       }},
      {"U-01", [&](Plotno& p) { rysujUstawienia(p, DaneUstawien()); }},
      {"U-05", [&](Plotno& p) { rysujTest(p, DaneTestu()); }},
      {"U-07", [&](Plotno& p) { rysujAktualizacje(p, aktualizacja::TresciU07()); }},
      {"Otwarte sprawy", [&](Plotno& p) { rysujSprawy(p, DaneSpraw()); }},
      {"Pytanie", [&](Plotno& p) { rysujPytanie(p, DanePytania()); }},
      {"Raport", [&](Plotno& p) { rysujRaport(p, DaneRaportu()); }},
  };
  for (int zn = 0; zn < 4; zn++) {  // 0 bez znaków, 1 nieaktualne, 2 nowa wersja, 3 wszystkie (z klepsydrą)
    ZnakiBelki z;
    z.nieaktualne = zn == 1 || zn == 3;
    z.daneZ = "13.11";
    z.nowaWersja = zn >= 2;
    z.pobieram = zn == 3;
    for (auto& e : ekrany) {
      ustawZnakiBelki(z);
      ustawBaterieEkranu(bat(100));
      PlotnoR a;
      e.rysuj(a);
      ustawZnakiBelki(z);
      ustawBaterieEkranu(bat(60, true));
      PlotnoR k;
      e.rysuj(k);
      int rozne = 0;
      for (int y = BELKA_WYSOKOSC; y < 480; y++)
        for (int x = 0; x < 800; x++) rozne += a.cz(x, y) != k.cz(x, y);
      const bool ikona = ikonaKablaSama(k);
      if (rozne || !ikona || k.poza) {
        int x0 = 800, x1 = -1, y0 = 480, y1 = -1;
        for (int y = BELKA_WYSOKOSC; y < 480; y++)
          for (int x = 0; x < 800; x++)
            if (a.cz(x, y) != k.cz(x, y)) {
              x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
            }
        printf("  %s (znaki %d): pod belką różnych pikseli %d (x %d…%d, y %d…%d; czarnych 100%%: %d, kabel: %d), "
               "ikona kabla sama %d, poza ekranem %d\n", e.nazwa, zn, rozne, x0, x1, y0, y1,
               a.czarne(x0 < 800 ? x0 : 0, y0 < 480 ? y0 : 0, x1 + 1, y1 + 1),
               k.czarne(x0 < 800 ? x0 : 0, y0 < 480 ? y0 : 0, x1 + 1, y1 + 1), (int)ikona, k.poza);
      }
      ROWNE_L(rozne, 0);
      SPRAWDZ(ikona);
      ROWNE_L(k.poza, 0);
    }
  }
  ustawZnakiBelki(ZnakiBelki());
  ustawBaterieEkranu(BateriaEkranu());
}

// ============================================================================================================
// (3) Liczby ze spec 5.4.17 jako twarde sprawdzenia; `100%` najciaśniejszym stanem baterii
// ============================================================================================================
static void testLiczbySpec() {
  printf("(3) liczby ze spec 5.4.17:\n");
  // tabela „Miejsce na środek przy 100% / 78% / 5% / – / kabel”
  const BateriaEkranu st[] = {bat(100), bat(78), bat(5), bat(-1), bat(60, true)};
  const char* nazwy[] = {"Ostatnia noc 1 / 3", "Poranek", "Portret nocy"};
  const int spec[3][5] = {{401, 410, 423, 445, 466}, {503, 512, 525, 547, 568}, {455, 464, 477, 499, 520}};
  for (int e = 0; e < 3; e++) {
    printf("  %-20s", nazwy[e]);
    for (int s = 0; s < 5; s++) {
      ustawBaterieEkranu(st[s]);
      const int m = miejsceSrodkaBelki(Belka{nazwy[e], ""});
      printf(" %d", m);
      ROWNE_L(m, spec[e][s]);
    }
    printf("\n");
  }
  // CHANGELOG i ADR: środek ma na kablu „o 66 px więcej” niż dawny `kabel` (400 / 502 / 454)
  ustawBaterieEkranu(bat(60, true));
  ROWNE_L(miejsceSrodkaBelki(Belka{nazwy[0], ""}) - 400, 66);
  // najciaśniej przy `100%`: każdy procent, `–` i kabel zostawiają środkowi co najmniej tyle samo miejsca
  ustawBaterieEkranu(bat(100));
  const int m100 = miejsceSrodkaBelki(Belka{nazwy[0], ""});
  int ciasniej = 0;
  for (int pr = -1; pr <= 100; pr++) {
    ustawBaterieEkranu(bat(pr));
    ciasniej += miejsceSrodkaBelki(Belka{nazwy[0], ""}) < m100;
  }
  ustawBaterieEkranu(bat(60, true));
  ciasniej += miejsceSrodkaBelki(Belka{nazwy[0], ""}) < m100;
  ROWNE_L(ciasniej, 0);
  // fail-path R-EKR-67: `! dane z 03.03` (157 px) + `◆ · ocena ograniczona` (232 px) w najciaśniejszym K1
  ROWNE_L(szerPolaAlarmu("dane z 03.03"), 157);
  ROWNE_L(szer(F_P22B, "◆ · ocena ograniczona"), 232);
  SPRAWDZ(157 + BELKA_ODSTEP_POLA + 232 <= m100);

  // licznik R-EKR-65 (K1, 365 dat × 53 godziny × 3 oceny × romb × 5 stanów baterii): 292 390 / 133 983 / 153 977 / 0
  const BateriaEkranu stany[] = {bat(5), bat(78), bat(100), bat(100, true), bat(-1)};
  const char* oceny[] = {"", "ocena częściowa", "ocena ograniczona"};
  std::vector<std::string> godziny;
  for (int m = 0; m < 1440; m += 30) {
    char g[8];
    snprintf(g, sizeof g, "%02d:%02d", m / 60, m % 60);
    godziny.push_back(g);
  }
  for (const char* g : {"08:08", "05:55", "00:00", "20:08", "23:59"}) godziny.push_back(g);
  long razem = 0, pelny = 0, bezDnia = 0, bezDaty = 0, bezGodziny = 0, pelnyKabel = 0;
  for (const BateriaEkranu& s : stany) {
    ustawBaterieEkranu(s);
    std::string data = "2026-01-01";
    for (int d = 0; d < 365; d++, data = dodajDni(data, 1))
      for (const std::string& godz : godziny)
        for (const char* oc : oceny)
          for (bool romb : {false, true}) {
            DaneK1 x;
            x.ubiegla = true;
            x.dzienNocy = DNI_KROTKO[dzienTygodnia(data)];
            x.dataNocy = ddmm(data);
            x.godzinaDanych = godz;
            x.ocena = oc;
            x.romb = romb;
            const Belka bx = belkaK1(x);
            const std::string sr = srodekBelki(bx);
            const bool jest = sr.find("odczyt " + godz) != std::string::npos;
            razem++;
            if (sr == bx.srodek) { pelny++; pelnyKabel += s.kabel; }
            else if (!bx.ustepuje.empty() && sr == bx.ustepuje[0]) bezDnia++;
            else if (jest) bezDaty++;
            else bezGodziny++;
          }
  }
  printf("  R-EKR-65 K1: %ld przypadków, pełny %ld (na kablu %ld), bez dnia %ld, bez daty %ld, bez godziny %ld\n", razem,
         pelny, pelnyKabel, bezDnia, bezDaty, bezGodziny);
  ROWNE_L(razem, 580350);
  ROWNE_L(pelny, 292390);
  ROWNE_L(bezDnia, 133983);
  ROWNE_L(bezDaty, 153977);
  ROWNE_L(bezGodziny, 0);

  // liczniki „Ustępowanie przy znakach”: K1 z klepsydrą i znakiem wersji (365 × 5 × 2 × 3): znak 2190, klepsydra 365
  {
    int n = 0, bezZn = 0, bezKl = 0;
    const BateriaEkranu sz[] = {bat(5), bat(78), bat(100), bat(100, true), BateriaEkranu()};
    for (int i = 0; i < 365; i++) {
      const std::string dz = dodajDni("2026-01-01", i);
      for (const BateriaEkranu& bb : sz)
        for (int romb = 0; romb < 2; romb++)
          for (const char* oc : oceny) {
            ZnakiBelki zz;
            zz.pobieram = zz.nowaWersja = true;
            ustawZnakiBelki(zz);
            ustawBaterieEkranu(bb);
            DaneK1 kk;
            kk.ubiegla = true;
            kk.dzienNocy = DNI_KROTKO[dzienTygodnia(dz)];
            kk.dataNocy = ddmm(dz);
            kk.godzinaDanych = "08:08";
            kk.romb = romb;
            kk.ocena = oc;
            Belka bk = belkaK1(kk);
            n++;
            bezKl += !ukladSrodkaBelki(bk).klepsydra;
            bezZn += !znakWersjiNaBelce(bk);
            if (bb.kabel) SPRAWDZ(ukladSrodkaBelki(bk).klepsydra && znakWersjiNaBelce(bk));  // spec: na kablu się mieszczą
          }
    }
    printf("  znaki przy K1: %d przypadków, znak wersji ustąpił %d, klepsydra %d\n", n, bezZn, bezKl);
    ROWNE_L(n, 10950);
    ROWNE_L(bezZn, 2190);
    ROWNE_L(bezKl, 365);
  }
  // dane nieaktualne (K1 z rombem i oceną ograniczoną): znak wersji 2465 z 3650, klepsydra 1742 z 3650
  {
    int bezZn = 0, bezKl = 0;
    const BateriaEkranu sz[] = {bat(5), bat(78), bat(100), bat(100, true), BateriaEkranu()};
    for (int i = 0; i < 365; i++) {
      const std::string dz = dodajDni("2026-01-01", i), wcz = dodajDni(dz, -1);
      for (const BateriaEkranu& s : sz)
        for (int pob = 0; pob < 2; pob++)
          for (int wer = 0; wer < 2; wer++) {
            ZnakiBelki zz;
            zz.nieaktualne = true;
            zz.daneZ = ddmm(wcz);
            zz.pobieram = pob;
            zz.nowaWersja = wer;
            ustawZnakiBelki(zz);
            ustawBaterieEkranu(s);
            DaneK1 kk;
            kk.dataNocy = ddmm(wcz);
            kk.romb = true;
            kk.ocena = "ocena ograniczona";
            Belka bb = belkaK1(kk);
            if (pob && !ukladSrodkaBelki(bb).klepsydra) bezKl++;
            if (wer && !znakWersjiNaBelce(bb)) bezZn++;
          }
    }
    printf("  dane nieaktualne K1: znak wersji ustąpił %d z 3650, klepsydra %d z 3650\n", bezZn, bezKl);
    ROWNE_L(bezZn, 2465);
    ROWNE_L(bezKl, 1742);
  }
  ustawZnakiBelki(ZnakiBelki());
  ustawBaterieEkranu(BateriaEkranu());
}

// ============================================================================================================
// (4) Dyrygent: kabel ↔ bateria ↔ stan nieznany między klatkami (harness jak test/testy_zasilanie_rytm.cpp)
// ============================================================================================================
static uint32_t ep(int r, int mi, int d, int h, int m, int s = 0) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static int minutaLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  return l.tm_hour * 60 + l.tm_min;
}
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_TERAPIA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const char* POLA_V3 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb;m18";
static long dobaTestu() { return nrDnia("2026-11-03"); }
static std::string raport(long day) {
  std::string d = std::to_string(day);
  return "{\"day\":" + d + ",\"present\":true,\"partial\":false,\"used_seconds\":28920,\"ahi\":1.4,"
         "\"events_known\":true,\"session_count\":1,\"counts\":{\"oa\":1,\"ca\":5,\"h\":4},\"channels\":[{\"id\":"
         "\"Press\",\"unit\":\"cmH2O\",\"median\":9.8,\"p95\":13.6,\"source\":\"edf\"},{\"id\":\"Leak\",\"unit\":"
         "\"L/min\",\"median\":2.4,\"p95\":9.6,\"source\":\"edf\"}],\"settings\":{\"available\":true,\"fields\":"
         "[{\"tag\":\"MOP\",\"value\":\"ASVAuto\",\"option\":8}]}}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}
struct PanelK {
  Panel p;
  PanelK() {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    std::string w;
    for (int i = 5; i >= 1; i--) {
      Noc n;
      n.data = dodajDni("2026-11-04", -i);
      n.wazna = true;
      n.m01 = 480;
      n.m18 = 660;
      n.ahi = 1.5f;
      n.tryb = "ASVAuto";
      w += wierszNocyV3(n) + "\n";
    }
    p.pliki.pliki["/dane/noce/2026.csv"] =
        std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n" + w + "#koniec 5\n";
    p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  }
};
struct WybK {
  uint32_t ep = 0;
  int odswiezen = 0;
  bool spal = false;
  Zasilanie zas = Zasilanie::BATERIA;
};
// Przebieg zegarem od `od` do `doT`; noc gotowa od `nocOd`. `zas(t)` i `volt(t)` ustawiają panel przed wybudzeniem.
// Po każdym wybudzeniu z odświeżeniem sprawdza ostatnią klatkę: ikona kabla ⇔ `kabelOczek(t)`; zawsze: flaga
// klatki „kabel” w RTC (bit 1) zgodna z tym, co stoi na e-inku.
static int gZBelka = 0, gBezBelki = 0;
// Wybudzenia przyciskiem ▶ o 05:30, 06:30, 08:00, 09:30, 13:00 (zmiana ekranu: świeży stan baterii, R-EKR-66).
static std::vector<uint32_t> chwilePrzyciskow() {
  return {ep(2026, 11, 4, 5, 30), ep(2026, 11, 4, 6, 30), ep(2026, 11, 4, 8, 0), ep(2026, 11, 4, 9, 30),
          ep(2026, 11, 4, 13, 0)};
}
template <typename FZ, typename FV, typename FK>
static std::vector<WybK> przebieg(const char* nazwa, uint32_t od, uint32_t doT, uint32_t nocOd, FZ zas, FV volt,
                                  FK kabelOczek, int* zleKlatki, int* zleFlagi) {
  PanelK s;
  std::vector<WybK> v;
  bool naEkranieKabel = false, cos = false;
  auto jedno = [&](uint32_t t, PowodWybudzenia pw) {
    s.p.czujniki.zas = zas(t);
    s.p.czujniki.o.bateriaV = volt(t);
    s.p.siec.odpowiedzi.clear();
    if (t < nocOd) {
      s.p.siec.dodaj("/api/status", 200, STATUS_TERAPIA);
    } else {
      s.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
      s.p.siec.dodaj(sciezkaListy(), 200, lista(dobaTestu(), dobaTestu()));
      s.p.siec.dodaj(sciezkaNocy(dobaTestu()), 200, raport(dobaTestu()));
    }
    s.p.zegar.epoch = t;
    s.p.zegar.msTeraz = 0;
    s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
    s.p.start.powodWybudzenia = pw;
    if (pw == PowodWybudzenia::PRZYCISK) {
      s.p.start.budzace = BIT_PRAWY;
      s.p.przyciski.skrypt = {{0, BIT_PRAWY}, {150, 0}};
    } else {
      s.p.start.budzace = 0;
      s.p.przyciski.skrypt.clear();
    }
    s.p.siec.polaczonaTeraz = false;
    s.p.d.z.clear();
    const int snow = s.p.sen.snow;
    const size_t klatek = s.p.ekran.klatki.size();
    Dyrygent(s.p.adaptery(), KONF).wybudzenie();
    WybK w;
    w.ep = t;
    w.zas = zas(t);
    w.spal = s.p.sen.snow == snow + 1;
    w.odswiezen = (int)(s.p.ekran.klatki.size() - klatek);
    if (getenv("LOG_KABEL_MIN") && minutaLok(t) == atoi(getenv("LOG_KABEL_MIN"))) {
      printf("   [%s %02d:%02d, odświeżeń %d, RTC bateria %d, flagi %d]\n", nazwa, minutaLok(t) / 60, minutaLok(t) % 60,
             w.odswiezen, (int)s.p.start.stan.klatkaBatProc, (int)s.p.start.stan.klatkaBatFlagi);
      for (auto& z : s.p.d.z)
        if (z.find("ekran") != std::string::npos || z.find("bater") != std::string::npos ||
            z.find("belka") != std::string::npos || z.find("wybudzenie") != std::string::npos)
          printf("     %s\n", z.c_str());
      if (w.odswiezen && s.p.ekran.klatki.size() >= 2) {
        const auto& a = s.p.ekran.klatki[s.p.ekran.klatki.size() - 2];
        const auto& b = s.p.ekran.klatki.back();
        int x0 = 800, x1 = -1, y0 = 480, y1 = -1, n = 0;
        for (int y = 0; y < 480; y++)
          for (int x = 0; x < 800; x++)
            if (czK(a, x, y) != czK(b, x, y)) {
              n++; x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
            }
        printf("     różnica z poprzednią klatką: %d px, x %d…%d, y %d…%d\n", n, x0, x1, y0, y1);
      }
    }
    if (w.odswiezen && !klatkaZBelka(s.p.ekran.klatki.back())) {
      cos = false;  // klatka bez belki (S9, slot dzienny): bateria niewidoczna, flaga klatki bez odniesienia
      gBezBelki++;
    } else if (w.odswiezen) {
      naEkranieKabel = klatkaZIkonaKabla(s.p.ekran.klatki.back());
      cos = true;
      gZBelka++;
      if (naEkranieKabel != kabelOczek(t)) {
        if (*zleKlatki < 3)
          printf("  %s %02d:%02d%s: na e-inku %s, oczekiwane %s\n", nazwa, minutaLok(t) / 60, minutaLok(t) % 60,
                 pw == PowodWybudzenia::PRZYCISK ? " (przycisk)" : "", naEkranieKabel ? "ikona kabla" : "bateria",
                 kabelOczek(t) ? "ikona kabla" : "bateria");
        (*zleKlatki)++;
      }
    }
    if (cos && ((s.p.start.stan.klatkaBatFlagi & 2) != 0) != naEkranieKabel) (*zleFlagi)++;
    v.push_back(w);
    return w;
  };
  const std::vector<uint32_t> przyc = chwilePrzyciskow();
  size_t ip = 0;
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 1500 && t < doT; i++) {
    while (ip < przyc.size() && przyc[ip] < t) {  // przycisk w czasie snu: osobne wybudzenie, potem plan dalej
      jedno(przyc[ip], PowodWybudzenia::PRZYCISK);
      ip++;
    }
    WybK w = jedno(t, pw);
    if (!w.spal) break;
    uint32_t snu = s.p.sen.plan.budzZegarem ? s.p.sen.plan.sekund : OKRES_SNU_S;
    t += s.p.zegar.msTeraz / 1000 + (snu ? snu : 1);
    pw = PowodWybudzenia::ZEGAR;
  }
  return v;
}
static int odswiezen(const std::vector<WybK>& v) {
  int n = 0;
  for (auto& w : v) n += w.odswiezen;
  if (getenv("LOG_KABEL")) {
    printf("   odświeżenia:");
    for (auto& w : v)
      if (w.odswiezen) printf(" %02d:%02d", minutaLok(w.ep) / 60, minutaLok(w.ep) % 60);
    printf("\n");
  }
  return n;
}

static void testDyrygentKabel() {
  printf("(4) dyrygent: kabel ↔ bateria ↔ stan nieznany:\n");
  const uint32_t od = ep(2026, 11, 4, 4, 0), doT = ep(2026, 11, 4, 14, 0), noc7 = ep(2026, 11, 4, 7, 2);
  const uint32_t t6 = ep(2026, 11, 4, 6, 0);
  auto stale = [](uint32_t) { return 3.95f; };
  int zk = 0, zf = 0;
  // (a) kabel cały czas: każda klatka z samą ikoną kabla
  auto vKabel = przebieg("kabel", od, doT, noc7, [](uint32_t) { return Zasilanie::ZEWNETRZNE; }, stale,
                         [](uint32_t) { return true; }, &zk, &zf);
  // (b) kabel, potem spadające napięcie na kablu: e-ink tyle samo razy (procentu na kablu nie widać)
  auto vKabelSpada = przebieg("kabel, napięcie spada", od, doT, noc7, [](uint32_t) { return Zasilanie::ZEWNETRZNE; },
                              [&](uint32_t t) { return 3.95f - 0.15f * (float)(t - od) / (float)(doT - od); },
                              [](uint32_t) { return true; }, &zk, &zf);
  // (c) kabel do 06:00, potem bateria: od pierwszego odświeżenia po odłączeniu procent (noc o 07:02 zmienia klatkę)
  auto vOdl = przebieg("odłączenie 06:00", od, doT, noc7,
                       [&](uint32_t t) { return t < t6 ? Zasilanie::ZEWNETRZNE : Zasilanie::BATERIA; }, stale,
                       [&](uint32_t t) { return t < t6; }, &zk, &zf);
  // (d) bateria do 06:00, potem kabel
  auto vPodl = przebieg("podłączenie 06:00", od, doT, noc7,
                        [&](uint32_t t) { return t < t6 ? Zasilanie::BATERIA : Zasilanie::ZEWNETRZNE; }, stale,
                        [&](uint32_t t) { return t >= t6; }, &zk, &zf);
  // (e) kabel do 06:00, potem stan „nieznane”: zostaje ostatni znany stan (ikona kabla)
  auto vNiezn = przebieg("kabel, potem nieznane", od, doT, noc7,
                         [&](uint32_t t) { return t < t6 ? Zasilanie::ZEWNETRZNE : Zasilanie::NIEZNANE; }, stale,
                         [](uint32_t) { return true; }, &zk, &zf);
  // (f) od startu tylko „nieznane”: jak bateria
  auto vTylkoNiezn = przebieg("tylko nieznane", od, doT, noc7, [](uint32_t) { return Zasilanie::NIEZNANE; }, stale,
                              [](uint32_t) { return false; }, &zk, &zf);
  // (g) bateria cały czas (odniesienie)
  auto vBat = przebieg("bateria", od, doT, noc7, [](uint32_t) { return Zasilanie::BATERIA; }, stale,
                       [](uint32_t) { return false; }, &zk, &zf);
  printf("  odświeżenia e-ink 04:00–14:00: kabel %d, kabel z opadającym napięciem %d, odłączenie %d, podłączenie %d, "
         "kabel→nieznane %d, nieznane %d, bateria %d; klatek niezgodnych %d, flag RTC niezgodnych %d\n",
         odswiezen(vKabel), odswiezen(vKabelSpada), odswiezen(vOdl), odswiezen(vPodl), odswiezen(vNiezn),
         odswiezen(vTylkoNiezn), odswiezen(vBat), zk, zf);
  printf("  klatek z belką sprawdzonych %d, klatek bez belki (S9, slot dzienny) %d\n", gZBelka, gBezBelki);
  SPRAWDZ(gZBelka >= 14);
  ROWNE_L(zk, 0);
  ROWNE_L(zf, 0);
  SPRAWDZ(odswiezen(vKabel) > 0 && odswiezen(vBat) > 0);
  ROWNE_L(odswiezen(vKabelSpada), odswiezen(vKabel));
  // kabel nie dokłada odświeżeń: najwyżej tyle co bateria (na baterii o 06:00 jest jedno więcej, ale to zmiana treści
  // ekranu y 66…468 z rytmu odczytów, ta sama przy stanie „nieznane”; nie dotyczy belki)
  SPRAWDZ(odswiezen(vKabel) <= odswiezen(vBat));
  SPRAWDZ(odswiezen(vOdl) <= odswiezen(vBat) && odswiezen(vPodl) <= odswiezen(vBat));  // zmiana stanu nie odświeża
  SPRAWDZ(odswiezen(vNiezn) <= odswiezen(vBat) && odswiezen(vTylkoNiezn) <= odswiezen(vBat));
}

int main() {
  setenv("TZ", STREFA, 1);
  tzset();
  testBelkiKabel();
  testTekstyBaterii();
  testPelneEkrany();
  testLiczbySpec();
  testDyrygentKabel();
  printf("testy_belka_bez_kabla_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
