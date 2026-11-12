// SPDX-License-Identifier: GPL-3.0-or-later
// Testy szybszego wyniku nocy rano (decyzja właściciela projektu 10.11.2026: A + B + D).
// Pisane z kryteriów akceptacji i wymagań R-DANE-61 (gęste okno), R-DANE-62 (powtórka przy przetwarzaniu),
// R-PRZ-22 (pobranie rano po przycisku), R-DANE-18 (brak zapytań 23:30–05:00), R-NF-02.
//
// (a) Plan wybudzenia (src/plan.h): zwykła pora końca terapii, gęste okno, powtórka, pobranie po przycisku, powody.
// (b) Symulacje poranków planem (bez dyrygenta): czas od końca terapii do pierwszego zapytania z wynikiem.
// (c) Dyrygent z atrapami: poranki z kryteriów akceptacji i wybudzenie przyciskiem (D).
// (d) Budżet energii doby (spec techniczna 8.1): liczba wybudzeń i wybudzeń z Wi-Fi przed zmianą i po niej.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_poranek_szybciej). LOG_PORANKA=1 drukuje przebiegi z (c) i (d).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "noc.h"
#include "plan.h"
#include "start.h"
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

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";
static bool logPoranka() { return getenv("LOG_PORANKA") != nullptr; }

static int sd(int h, int m, int s = 0) { return h * 3600 + m * 60 + s; }
static WejsciePlanu wp(int h, int m, int s = 5) {
  WejsciePlanu w;
  w.zegar = true;
  w.sekundaDoby = sd(h, m, s);
  return w;
}
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

// Noc z poranka `data` z terapią od `startOd12` (minuty od 12:00 wieczoru, M-18) przez `minut` (M-01).
static Noc noc(const std::string& data, int startOd12, int minut) {
  Noc n;
  n.data = data;
  n.wazna = true;
  n.m01 = minut;
  n.m18 = startOd12;
  n.ahi = 1.5f;
  n.tryb = "ASVAuto";
  return n;
}
// Historia `ile` nocy przed 4.11.2026 (poranki 3.11, 2.11, …) z końcem terapii o `koniec` (minuta doby):
// start 23:00 (M-18 = 660), M-01 = koniec + 24 h − 23:00.
static std::vector<Noc> historia(int ile, int koniec = 7 * 60) {
  std::vector<Noc> h;
  for (int i = ile; i >= 1; i--) h.push_back(noc(dodajDni("2026-11-04", -i), 660, koniec + 24 * 60 - 23 * 60));
  return h;
}

// ============================================================================================================
// (a) Plan wybudzenia
// ============================================================================================================

// R-DANE-61: koniec terapii nocy = M-18 + M-01 (przybliżenie z rekordu), mediana z ostatnich 14 nocy, co najmniej 3.
static void testR_DANE_61_zwyklaPora() {
  ROWNE_L(koniecTerapiiNocy(noc("2026-11-03", 660, 480)), 1140);  // 23:00 + 8 h = 07:00 (1140 min od 12:00)
  ROWNE_L(koniecTerapiiNocy(noc("2026-11-03", -1, 480)), -1);     // start nieznany (noc z karty)
  ROWNE_L(koniecTerapiiNocy(noc("2026-11-03", 660, 10)), -1);     // poniżej 15 min: bez terapii
  Noc cz = noc("2026-11-03", 660, 480);
  cz.czesciowa = true;
  ROWNE_L(koniecTerapiiNocy(cz), -1);
  // mniej niż 3 noce: bez gęstego okna (zachowanie jak przed zmianą)
  ROWNE_L(zwyklyKoniecTerapii(historia(2), "2026-11-04"), -1);
  ROWNE_L(zwyklyKoniecTerapii({}, "2026-11-04"), -1);
  ROWNE_L(zwyklyKoniecTerapii(historia(3), "2026-11-04"), 7 * 60);
  // mediana: 06:50, 07:00, 07:40 → 07:00; parzysta liczba: średnia dwóch środkowych
  std::vector<Noc> h = {noc("2026-11-01", 660, 470), noc("2026-11-02", 660, 480), noc("2026-11-03", 660, 520)};
  ROWNE_L(zwyklyKoniecTerapii(h, "2026-11-04"), 7 * 60);
  h.push_back(noc("2026-10-31", 660, 500));  // kolejność w historii nie ma znaczenia dla mediany
  ROWNE_L(zwyklyKoniecTerapii(h, "2026-11-04"), 7 * 60 + 10);
  // noc z poranka dziś (i późniejsza) się nie liczy; noc bez znanego końca pominięta
  std::vector<Noc> h2 = historia(2);
  h2.push_back(noc("2026-11-04", 660, 300));
  h2.push_back(noc("2026-11-03", -1, 480));
  ROWNE_L(zwyklyKoniecTerapii(h2, "2026-11-04"), -1);
  // tylko 14 ostatnich nocy: 14 nocy z końcem 08:00 przesłania starsze z 06:00
  std::vector<Noc> h3;
  for (int i = 30; i >= 15; i--) h3.push_back(noc(dodajDni("2026-11-04", -i), 660, 6 * 60 + 60));
  for (int i = 14; i >= 1; i--) h3.push_back(noc(dodajDni("2026-11-04", -i), 660, 8 * 60 + 60));
  ROWNE_L(zwyklyKoniecTerapii(h3, "2026-11-04"), 8 * 60);
  // koniec przed północą (terapia 20:00–23:00) albo po 12:00: poza oknem porannym, bez minuty poranka
  std::vector<Noc> h4 = {noc("2026-11-01", 480, 180), noc("2026-11-02", 480, 180), noc("2026-11-03", 480, 180)};
  ROWNE_L(zwyklyKoniecTerapii(h4, "2026-11-04"), -1);
  ROWNE_L(zwyklyKoniecTerapii(historia(3), "zła data"), -1);
}

// R-DANE-61: gęste okno 45 min przed i po zwykłej porze, przycięte do okna porannego (bez 05:00 i 11:00).
static void testR_DANE_61_gesteOkno() {
  WejsciePlanu w = wp(5, 0);
  SPRAWDZ(!wGestymOknie(w, 7 * 60));  // bez zwykłej pory: bez okna
  w.zwyklyKoniec = 7 * 60;
  SPRAWDZ(!wGestymOknie(w, 6 * 60 + 14));
  SPRAWDZ(wGestymOknie(w, 6 * 60 + 15));
  SPRAWDZ(wGestymOknie(w, 7 * 60 + 45));
  SPRAWDZ(!wGestymOknie(w, 7 * 60 + 46));
  w.zwyklyKoniec = 5 * 60 + 20;  // okno 04:35–06:05 przycięte do 05:05–06:05
  SPRAWDZ(!wGestymOknie(w, 4 * 60 + 40));
  SPRAWDZ(!wGestymOknie(w, 5 * 60));
  SPRAWDZ(wGestymOknie(w, 5 * 60 + 5));
  w.zwyklyKoniec = 10 * 60 + 40;  // 09:55–11:25 przycięte do 09:55–10:59
  SPRAWDZ(wGestymOknie(w, 10 * 60 + 55));
  SPRAWDZ(!wGestymOknie(w, 11 * 60));
  w.zwyklyKoniec = 4 * 60;  // koniec terapii poza oknem porannym: brak gęstego okna
  for (int m = 0; m < 24 * 60; m++) SPRAWDZ(!wGestymOknie(w, m));
  w.zwyklyKoniec = 12 * 60;
  for (int m = 0; m < 24 * 60; m++) SPRAWDZ(!wGestymOknie(w, m));
}

// R-DANE-61, R-NF-02: terminy planu w gęstym oknie co 5 min, siatka 30 min bez zmian, koniec przy nocy kompletnej.
static void testR_DANE_61_terminy() {
  WejsciePlanu w = wp(6, 10);
  w.zwyklyKoniec = 7 * 60;
  Termin t = nastepnyTermin(w);  // 06:10:05 → 06:15:05 (gęste okno)
  ROWNE_L(t.sekund, 5 * 60);
  ROWNE_L(t.powody, POWOD_OKNO_GESTE);
  w.sekundaDoby = sd(6, 25, 5);  // 06:30 zostaje terminem okna porannego (bez bitu gęstego okna)
  t = nastepnyTermin(w);
  ROWNE_L(t.powody, POWOD_OKNO_PORANNE);
  w.sekundaDoby = sd(7, 45, 5);  // po końcu gęstego okna: siatka 30 min
  t = nastepnyTermin(w);
  ROWNE_L(t.sekund, 15 * 60);
  ROWNE_L(t.powody, POWOD_OKNO_PORANNE);
  // powodyTeraz w gęstym oknie
  WejsciePlanu v = wp(6, 35);
  v.zwyklyKoniec = 7 * 60;
  ROWNE_L(powodyTeraz(v), POWOD_OKNO_GESTE);
  // noc kompletna: bez gęstego okna (koniec dla doby)
  w.sekundaDoby = sd(6, 20, 5);
  w.nocKompletna = true;
  t = nastepnyTermin(w);
  SPRAWDZ(!(t.powody & POWOD_OKNO_GESTE));
  SPRAWDZ(t.sekund > 30 * 60);
  // ostatnia próba bez odpowiedzi AirBridge albo Wi-Fi (E-01, decyzja właściciela projektu 10.11.2026): bez gęstego okna, siatka 30 min
  WejsciePlanu e = wp(6, 10);
  e.zwyklyKoniec = 7 * 60;
  e.e01 = true;
  t = nastepnyTermin(e);
  ROWNE_L(t.sekund, 20 * 60);
  ROWNE_L(t.powody, POWOD_OKNO_PORANNE);
  e.logowanieOdrzucone = true;  // odrzucone logowanie: noc nie przyjdzie do poprawy hasła, też bez
  ROWNE_L(nastepnyTermin(e).powody, POWOD_OKNO_PORANNE);
  e.logowanieOdrzucone = false;
  e.e01 = false;  // pierwsza odpowiedź: gęste okno wraca
  ROWNE_L(nastepnyTermin(e).powody, POWOD_OKNO_GESTE);
  // bez zwykłej pory (mniej niż 3 noce): jak przed zmianą
  WejsciePlanu b = wp(6, 10);
  t = nastepnyTermin(b);
  ROWNE_L(t.sekund, 20 * 60);
  ROWNE_L(t.powody, POWOD_OKNO_PORANNE);
  // pełne pobranie (status, lista, raport), z Wi-Fi; nie „tylko status” slotu dziennego
  SPRAWDZ(sieciowy(POWOD_OKNO_GESTE) && sieciowy(POWOD_POWTORKA));
  SPRAWDZ(!tylkoStatus(POWOD_OKNO_GESTE) && !tylkoStatus(POWOD_POWTORKA));
  SPRAWDZ(!przerysowanieDnia(POWOD_OKNO_GESTE));
  SPRAWDZ(pobranieZegarem(POWOD_OKNO_GESTE).pobierz);
  SPRAWDZ(nazwyPowodow(POWOD_OKNO_GESTE) == "okno_geste");
  SPRAWDZ(nazwyPowodow(POWOD_POWTORKA | POWOD_CZUJNIKI) == "czujniki+powtorka_przetwarzania");
  SPRAWDZ(powodWpisuBaterii(PowodWybudzenia::ZEGAR, POWOD_OKNO_GESTE) == "zegar:okno_geste");
}

// R-DANE-62: powtórka 2 min po statusie „przetwarza”, najwyżej 5 na dobę, tylko w oknie porannym przy nocy niekompletnej.
static void testR_DANE_62_powtorka() {
  WejsciePlanu w = wp(8, 41, 20);
  w.przetwarzaOd = sd(8, 41, 10);
  ROWNE_L(terminPowtorki(w), sd(8, 43, 10));
  Termin t = nastepnyTermin(w);
  ROWNE_L(t.sekund, 110 + ZAPAS_POLGODZINY_S);
  ROWNE_L(t.powody, POWOD_POWTORKA);
  WejsciePlanu po = w;  // wybudzenie o 08:43:15: powód powtórki
  po.sekundaDoby = sd(8, 43, 15);
  po.obsluzoneZnane = true;
  po.obsluzoneDo = sd(8, 41, 10);
  ROWNE_L(powodyTeraz(po), POWOD_POWTORKA);
  // limit 5 na dobę: dalej zwykła siatka
  w.powtorekDzis = PLAN_POWTORKI_MAKS;
  ROWNE_L(terminPowtorki(w), -1);
  t = nastepnyTermin(w);
  ROWNE_L(t.powody, POWOD_OKNO_PORANNE);  // 09:00
  w.powtorekDzis = PLAN_POWTORKI_MAKS - 1;
  ROWNE_L(terminPowtorki(w), sd(8, 43, 10));
  // noc kompletna, poza oknem porannym, bez zegara: bez powtórki
  WejsciePlanu k = w;
  k.nocKompletna = true;
  ROWNE_L(terminPowtorki(k), -1);
  WejsciePlanu p = wp(10, 59);
  p.przetwarzaOd = sd(10, 58, 30);  // 11:00:30 już poza oknem porannym
  ROWNE_L(terminPowtorki(p), -1);
  p.przetwarzaOd = sd(4, 59, 0);    // przed 05:00 (slot nocny)
  ROWNE_L(terminPowtorki(p), -1);
  WejsciePlanu bz = w;
  bz.zegar = false;
  ROWNE_L(terminPowtorki(bz), -1);
  // gęste okno: termin siatki wcześniejszy niż powtórka wygrywa (powtórka nie zużyta)
  WejsciePlanu g = wp(7, 3, 20);
  g.zwyklyKoniec = 7 * 60;
  g.przetwarzaOd = sd(7, 3, 10);
  t = nastepnyTermin(g);
  ROWNE_L(t.powody, POWOD_OKNO_GESTE);  // 07:05:00 przed 07:05:10
  ROWNE_L(t.sekund, 100 + ZAPAS_POLGODZINY_S);
  g.przetwarzaOd = sd(7, 2, 0);  // powtórka 07:04:00 przed 07:05
  t = nastepnyTermin(g);
  ROWNE_L(t.powody, POWOD_POWTORKA);
  // powtórka, która minęła w trakcie wybudzenia (czuwanie po przycisku): zaraz, z tym powodem
  WejsciePlanu z = wp(8, 5);
  z.przetwarzaOd = sd(8, 0);
  t = nastepnyTermin(z);
  ROWNE_L(t.sekund, ZALEGLY_TERMIN_S);
  ROWNE_L(t.powody, POWOD_POWTORKA);
  // nowa doba: po północy powtórek i przetwarzania z wczoraj nie ma (terminy jutra bez POWTORKA)
  WejsciePlanu j = wp(23, 50);
  j.przetwarzaOd = sd(10, 0);
  j.powtorekDzis = 3;
  t = nastepnyTermin(j);
  SPRAWDZ(!(t.powody & POWOD_POWTORKA));
}

// R-PRZ-22: pobranie po przycisku rano bez nocy, najwyżej raz na 2 min.
static void testR_PRZ_22_warunek() {
  WejsciePlanu w = wp(6, 10);
  SPRAWDZ(pobranieRanoPrzyciskiem(w, -1));
  SPRAWDZ(!pobranieRanoPrzyciskiem(w, 60));
  SPRAWDZ(!pobranieRanoPrzyciskiem(w, 119));
  SPRAWDZ(pobranieRanoPrzyciskiem(w, 120));
  WejsciePlanu k = w;
  k.nocKompletna = true;
  SPRAWDZ(!pobranieRanoPrzyciskiem(k, -1));
  SPRAWDZ(!pobranieRanoPrzyciskiem(wp(11, 0, 0), -1));  // od 11:00 bez zmian względem stanu przed zmianą
  SPRAWDZ(!pobranieRanoPrzyciskiem(wp(4, 59, 59), -1));  // slot nocny
  SPRAWDZ(pobranieRanoPrzyciskiem(wp(5, 0, 0), -1));
  SPRAWDZ(pobranieRanoPrzyciskiem(wp(10, 59, 59), -1));
  WejsciePlanu b = w;
  b.zegar = false;
  SPRAWDZ(!pobranieRanoPrzyciskiem(b, -1));
  WejsciePlanu e = w;  // E-01 (ostatnia próba bez odpowiedzi albo odrzucone logowanie): bez pobrania
  e.e01 = true;
  SPRAWDZ(!pobranieRanoPrzyciskiem(e, -1));
  e.logowanieOdrzucone = true;
  SPRAWDZ(!pobranieRanoPrzyciskiem(e, -1));
}

// ============================================================================================================
// (b) Symulacje poranków samym planem: dyrygent zastąpiony regułą „zapytanie widzi stan AirBridge w tej chwili”
// ============================================================================================================

struct Poranek {
  int zwyklyKoniec = 7 * 60;  // -1 = mniej niż 3 noce
  int koniecTerapii = 7 * 60 + 2;  // sekundy nie mają znaczenia: minuta doby
  int koniecPrzetwarzania = -1;    // minuta doby; -1 = bez przetwarzania
  bool e01 = false;                // AirBridge milczy cały dzień (każde zapytanie bez odpowiedzi)
};
struct WynikPoranka {
  int wynik = -1;  // sekunda doby zapytania, które dało noc
  int zapytan = 0, powtorek = 0, nocnychZapytan = 0, wybudzen = 0;
  std::vector<int> minuty;  // minuty zapytań (z Wi-Fi)
};
// Doba od 00:00:05 do 23:59 planem; noc jest na AirBridge od końca terapii (i przetwarzania).
static WynikPoranka symulujPlan(const Poranek& p) {
  WynikPoranka r;
  WejsciePlanu w = wp(0, 0);
  w.zwyklyKoniec = p.zwyklyKoniec;
  long teraz = 5;
  for (int i = 0; i < 400 && teraz < 86400 - 60; i++) {
    w.sekundaDoby = (int)teraz;
    Termin t = nastepnyTermin(w);
    w.obsluzoneZnane = true;
    w.obsluzoneDo = (int)teraz;
    teraz += t.sekund;
    if (teraz >= 86400) break;
    w.sekundaDoby = (int)teraz;
    Powody pw = powodyTeraz(w);
    r.wybudzen++;
    if (pw & POWOD_POWTORKA) {
      r.powtorek++;
      w.powtorekDzis++;
      w.przetwarzaOd = -1;
    }
    if (pw & POWOD_SPRAWDZENIE_11) w.sprawdzenie11 = true;
    if (!sieciowy(pw)) continue;
    int m = (int)(teraz / 60);
    r.zapytan++;
    r.minuty.push_back(m);
    if (m > 23 * 60 + 30 || m < 5 * 60) r.nocnychZapytan++;
    if (p.e01) {  // zapytanie bez odpowiedzi: E-01 od tej chwili (StanAB::bladOd)
      w.e01 = true;
      continue;
    }
    if (w.nocKompletna) continue;
    bool terapia = m < p.koniecTerapii && m >= 5 * 60;
    bool przetwarza = !terapia && p.koniecPrzetwarzania >= 0 && m < p.koniecPrzetwarzania;
    w.przetwarzaOd = przetwarza ? (int)teraz : -1;
    if (!terapia && !przetwarza && m >= 5 * 60) {
      w.nocKompletna = true;
      r.wynik = (int)teraz;
    }
  }
  return r;
}

// Karta, rozdz. 4: koniec terapii o zwykłej porze → wynik najpóźniej ok. 5 min po końcu terapii bez przetwarzania,
// najpóźniej ok. 2 min po końcu przetwarzania; bez zapytań 23:30–05:00; po nocy kompletnej bez gęstego okna.
static void testKryteria_symulacjePlanem() {
  for (int koniec = 6 * 60 + 15; koniec <= 7 * 60 + 45; koniec++) {  // każda minuta gęstego okna
    Poranek p;
    p.koniecTerapii = koniec;
    WynikPoranka r = symulujPlan(p);
    SPRAWDZ(r.wynik >= 0 && r.wynik - koniec * 60 <= 5 * 60 + 5);
    ROWNE_L(r.nocnychZapytan, 0);
    // po nocy kompletnej: następne zapytanie dopiero o 11:00 (R-DANE-24) i o 23:30
    std::vector<int> po;
    for (int m : r.minuty)
      if (m * 60 > r.wynik) po.push_back(m);
    SPRAWDZ(po.size() == 2 && po[0] == 11 * 60 && po[1] == 23 * 60 + 30);
  }
  // przetwarzanie 10 min po terapii do 07:12 → wynik najpóźniej ok. 2 min po końcu przetwarzania
  for (int kp = 7 * 60 + 3; kp <= 7 * 60 + 12; kp++) {
    Poranek p;
    p.koniecTerapii = 7 * 60 + 2;
    p.koniecPrzetwarzania = kp;
    WynikPoranka r = symulujPlan(p);
    SPRAWDZ(r.wynik >= 0 && r.wynik - kp * 60 <= 2 * 60 + 5);
  }
  // koniec terapii poza gęstym oknem (09:20) → siatka 30 min jak dziś, a przy przetwarzaniu powtórki (B)
  {
    Poranek p;
    p.koniecTerapii = 9 * 60 + 20;
    WynikPoranka r = symulujPlan(p);
    ROWNE_L(r.wynik, sd(9, 30, 5));
    p.koniecPrzetwarzania = 9 * 60 + 35;
    r = symulujPlan(p);
    SPRAWDZ(r.wynik > 0 && r.wynik - (9 * 60 + 35) * 60 <= 2 * 60 + 5);
    SPRAWDZ(r.powtorek >= 1 && r.powtorek <= PLAN_POWTORKI_MAKS);
  }
  // mniej niż 3 noce historii → siatka 30 min jak dziś, plus powtórki B
  {
    Poranek p;
    p.zwyklyKoniec = -1;
    p.koniecTerapii = 7 * 60 + 2;
    WynikPoranka r = symulujPlan(p);
    ROWNE_L(r.wynik, sd(7, 30, 5));
    std::vector<int> rano;
    for (int m : r.minuty)
      if (m < 11 * 60) rano.push_back(m);
    SPRAWDZ((rano == std::vector<int>{300, 330, 360, 390, 420, 450}));
    p.koniecPrzetwarzania = 7 * 60 + 40;  // 07:30 przetwarza → 07:32, 07:34, …, 07:40 → wynik 07:40 albo 07:42
    r = symulujPlan(p);
    SPRAWDZ(r.wynik > 0 && r.wynik - (7 * 60 + 40) * 60 <= 2 * 60 + 5);
  }
  // przetwarzanie dłuższe niż 5 powtórek (07:50–08:40, gęste okno już minęło): 08:00, 5 powtórek do 08:10, potem
  // siatka 30 min (08:30 przetwarza, 09:00 noc)
  {
    Poranek p;
    p.koniecTerapii = 7 * 60 + 50;
    p.koniecPrzetwarzania = 8 * 60 + 40;
    WynikPoranka r = symulujPlan(p);
    ROWNE_L(r.powtorek, PLAN_POWTORKI_MAKS);
    ROWNE_L(r.wynik, sd(9, 0, 5));
    std::vector<int> rano;
    for (int m : r.minuty)
      if (m >= 8 * 60 && m < 11 * 60) rano.push_back(m);
    SPRAWDZ((rano == std::vector<int>{480, 482, 484, 486, 488, 490, 510, 540}));
    ROWNE_L(r.nocnychZapytan, 0);
  }
  // terapia trwa do końca gęstego okna (07:45) i 5 powtórek: najgorsza doba (8.1)
  {
    Poranek p;
    p.koniecTerapii = 7 * 60 + 46;
    p.koniecPrzetwarzania = 9 * 60 + 15;
    WynikPoranka r = symulujPlan(p);
    ROWNE_L(r.powtorek, PLAN_POWTORKI_MAKS);
    ROWNE_L(r.nocnychZapytan, 0);
  }
}

// ============================================================================================================
// (c) Dyrygent z atrapami
// ============================================================================================================

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_TERAPIA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* STATUS_PRZETWARZA =
    "{\"version\":\"v2.2.1\",\"mode\":\"ASVAuto\",\"rop\":0,\"edf\":\"post-processing\",\"edf_prefix\":\"\","
    "\"report_state\":\"idle\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const char* POLA_V3 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb;m18";
static const long DOBA = nrDnia("2026-11-03");  // noc z poranka 4.11.2026

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

struct PanelP {
  Panel p;
  explicit PanelP(int nocyHistorii, int koniec = 7 * 60) {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    std::string w;
    for (auto& n : historia(nocyHistorii, koniec)) w += wierszNocyV3(n) + "\n";
    p.pliki.pliki["/dane/noce/2026.csv"] = std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n" + w + "#koniec " +
                                           std::to_string(nocyHistorii) + "\n";
    p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";  // bez trybu porównawczego (drugie połączenie)
  }
  bool maNoc() const {
    auto it = p.pliki.pliki.find("/dane/noce/2026.csv");
    return it != p.pliki.pliki.end() && it->second.find("\n2026-11-04;") != std::string::npos;
  }
};

// Stan AirBridge w chwili t: terapia do `terapiaDo`, przetwarzanie do `przetwarzaDo`, potem noc na liście.
struct ScenariuszAB {
  uint32_t terapiaDo = 0, przetwarzaDo = 0;
};
static void odpowiedzi(PanelP& s, const ScenariuszAB& sc, uint32_t t) {
  s.p.siec.odpowiedzi.clear();
  if (t < sc.terapiaDo) {
    s.p.siec.dodaj("/api/status", 200, STATUS_TERAPIA);
  } else if (t < sc.przetwarzaDo) {
    s.p.siec.dodaj("/api/status", 200, STATUS_PRZETWARZA);
  } else {
    s.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.p.siec.dodaj(sciezkaListy(), 200, lista(DOBA, DOBA));
    s.p.siec.dodaj(sciezkaNocy(DOBA), 200, raport(DOBA));
  }
}

struct WybP {
  uint32_t ep = 0;
  int minuta = 0;
  PowodWybudzenia powod = PowodWybudzenia::ZEGAR;
  int polaczen = 0, odswiezen = 0;
  bool spal = false, noc = false;
  std::string powody;  // pole `powod` wiersza dziennika wybudzeń (z logu)
};
static WybP wybudz(PanelP& s, uint32_t t, PowodWybudzenia pw) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.p.siec.polaczonaTeraz = false;
  s.p.d.z.clear();
  int snow = s.p.sen.snow;
  Dyrygent(s.p.adaptery(), KONF).wybudzenie();
  WybP w;
  w.ep = t;
  w.minuta = minutaLok(t);
  w.powod = pw;
  w.spal = s.p.sen.snow == snow + 1;
  w.noc = s.maNoc();
  for (auto& z : s.p.d.z) {
    w.polaczen += z == "siec.polacz";
    w.odswiezen += z == "ekran.odswiez";
    size_t a = z.find("wybudzenie ");
    if (w.powody.empty() && a != std::string::npos && z.find("cpap-eink") != std::string::npos) {
      size_t b = z.find(',', a);
      w.powody = z.substr(a + 11, b == std::string::npos ? std::string::npos : b - a - 11);
    }
  }
  return w;
}
// Wybudzenia zegarem od startu zasilania o `od` do `doT`.
static std::vector<WybP> symuluj(PanelP& s, const ScenariuszAB& sc, uint32_t od, uint32_t doT) {
  std::vector<WybP> v;
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 400 && t < doT; i++) {
    odpowiedzi(s, sc, t);
    WybP w = wybudz(s, t, pw);
    v.push_back(w);
    if (!w.spal) break;
    uint32_t snu = s.p.sen.plan.budzZegarem ? s.p.sen.plan.sekund : OKRES_SNU_S;
    t += s.p.zegar.msTeraz / 1000 + (snu ? snu : 1);
    pw = PowodWybudzenia::ZEGAR;
  }
  if (logPoranka())
    for (auto& w : v)
      printf("  %s %-9s Wi-Fi %d, e-ink %d, noc %d, %s\n", hhmm(w.minuta).c_str(),
             w.powod == PowodWybudzenia::START ? "start" : "zegar", w.polaczen, w.odswiezen, (int)w.noc,
             w.powody.c_str());
  return v;
}
// Chwila (czas unix) pierwszego wybudzenia z nocą w historii, albo 0.
static uint32_t chwilaNocy(const std::vector<WybP>& v) {
  for (auto& w : v)
    if (w.noc) return w.ep;
  return 0;
}
static int zapytanWNocy(const std::vector<WybP>& v) {
  int n = 0;
  for (auto& w : v)
    if (w.powod == PowodWybudzenia::ZEGAR && w.polaczen > 0 && (w.minuta > 23 * 60 + 30 || w.minuta < 5 * 60)) n++;
  return n;
}
static int zPowodem(const std::vector<WybP>& v, const std::string& nazwa) {
  int n = 0;
  for (auto& w : v) n += w.powody.find(nazwa) != std::string::npos;
  return n;
}

// Karta, rozdz. 4, na dyrygencie: koniec terapii 07:02 o zwykłej porze (historia 5 nocy z końcem 07:00).
static void testKryteria_dyrygentZwyklaPora() {
  PanelP s(5);
  ScenariuszAB sc;
  sc.terapiaDo = ep(2026, 11, 4, 7, 2);
  sc.przetwarzaDo = sc.terapiaDo;
  std::vector<WybP> v = symuluj(s, sc, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 12, 0));
  uint32_t n = chwilaNocy(v);
  SPRAWDZ(n > 0 && n >= sc.terapiaDo && n - sc.terapiaDo <= 5 * 60 + 30);
  ROWNE_L(zapytanWNocy(v), 0);
  SPRAWDZ(zPowodem(v, "okno_geste") >= 9);  // 06:15…07:05 bez 06:30 i 07:00
  // po nocy kompletnej: bez gęstego okna i powtórek do 11:00
  int poNocy = 0;
  for (auto& w : v)
    if (w.ep > n && w.minuta < 11 * 60 && w.powody.find("okno_") != std::string::npos) poNocy++;
  ROWNE_L(poNocy, 0);
  // przy terapii w gęstym oknie tylko `/api/status` (bez listy i raportów)
  for (auto& w : v)
    if (w.ep < sc.terapiaDo && w.powody.find("okno_geste") != std::string::npos) ROWNE_L(w.polaczen, 1);
}

// Przetwarzanie 07:02–07:12: wynik najpóźniej ok. 2 min po końcu przetwarzania (B w gęstym oknie).
static void testKryteria_dyrygentPrzetwarzanie() {
  PanelP s(5);
  ScenariuszAB sc;
  sc.terapiaDo = ep(2026, 11, 4, 7, 2);
  sc.przetwarzaDo = ep(2026, 11, 4, 7, 12);
  std::vector<WybP> v = symuluj(s, sc, ep(2026, 11, 4, 4, 0), ep(2026, 11, 4, 12, 0));
  uint32_t n = chwilaNocy(v);
  SPRAWDZ(n > 0 && n - sc.przetwarzaDo <= 2 * 60 + 30);
  SPRAWDZ(zPowodem(v, "powtorka_przetwarzania") >= 1);
  ROWNE_L(zapytanWNocy(v), 0);
}

// Koniec terapii poza gęstym oknem (09:10, przetwarzanie do 09:35): siatka 30 min jak dziś i powtórki B.
static void testKryteria_dyrygentPozaOknem() {
  PanelP s(5);
  ScenariuszAB sc;
  sc.terapiaDo = ep(2026, 11, 4, 9, 10);
  sc.przetwarzaDo = ep(2026, 11, 4, 9, 35);
  std::vector<WybP> v = symuluj(s, sc, ep(2026, 11, 4, 4, 0), ep(2026, 11, 4, 12, 0));
  uint32_t n = chwilaNocy(v);
  SPRAWDZ(n > 0 && n - sc.przetwarzaDo <= 2 * 60 + 30);
  for (auto& w : v)  // po gęstym oknie (07:45) do terminu 09:30 tylko siatka 30 min
    if (w.minuta > 7 * 60 + 45 && w.minuta < 9 * 60 + 30 && w.polaczen > 0) SPRAWDZ(w.minuta % 30 == 0);
}

// Mniej niż 3 noce historii: jak dziś (siatka 30 min) plus B; przetwarzanie dłuższe niż 5 powtórek.
static void testKryteria_dyrygentKrotkaHistoria() {
  PanelP s(2);
  ScenariuszAB sc;
  sc.terapiaDo = ep(2026, 11, 4, 7, 50);
  sc.przetwarzaDo = ep(2026, 11, 4, 8, 40);
  std::vector<WybP> v = symuluj(s, sc, ep(2026, 11, 4, 4, 0), ep(2026, 11, 4, 12, 0));
  ROWNE_L(zPowodem(v, "okno_geste"), 0);
  ROWNE_L(zPowodem(v, "powtorka_przetwarzania"), PLAN_POWTORKI_MAKS);
  std::vector<int> zWifi;
  for (auto& w : v)
    if (w.powod == PowodWybudzenia::ZEGAR && w.polaczen > 0 && w.minuta < 9 * 60 + 30) zWifi.push_back(w.minuta);
  // (od 10:00 start slotu dziennego, R-KON-41) 05:00…08:00 co 30 min, 08:02…08:10 powtórki, 08:30 przetwarza, 09:00 noc
  std::vector<int> oczek = {300, 330, 360, 390, 420, 450, 480, 482, 484, 486, 488, 490, 510, 540};
  SPRAWDZ(zWifi == oczek);
  if (zWifi != oczek)
    for (int m : zWifi) printf("    %s\n", hhmm(m).c_str());
  SPRAWDZ(chwilaNocy(v) > 0 && minutaLok(chwilaNocy(v)) == 9 * 60);
}

// Decyzja właściciela projektu 10.11.2026: bez gęstego okna, gdy ostatnia próba skończyła się brakiem odpowiedzi AirBridge albo
// Wi-Fi; wraca przy pierwszej odpowiedzi. (a) Wi-Fi brak cały poranek → tylko siatka 30 min; (b) Wi-Fi wraca 06:40,
// terapia do 07:20 → 07:00 udane (siatka), od 07:05 znów co 5 min, noc najpóźniej ok. 5 min po końcu terapii.
static void testR_DANE_61_bezOdpowiedzi() {
  {
    PanelP s(5);
    s.p.siec.siecJest = false;
    ScenariuszAB sc{ep(2026, 11, 4, 7, 2), ep(2026, 11, 4, 7, 2)};
    std::vector<WybP> v = symuluj(s, sc, ep(2026, 11, 4, 4, 0), ep(2026, 11, 4, 11, 30));
    ROWNE_L(zPowodem(v, "okno_geste"), 0);
    std::vector<int> zWifi;
    for (auto& w : v)
      if (w.powod == PowodWybudzenia::ZEGAR && w.polaczen > 0 && w.minuta < 11 * 60) zWifi.push_back(w.minuta);
    SPRAWDZ((zWifi == std::vector<int>{300, 330, 360, 390, 420, 450, 480, 510, 540, 570, 600, 630}));
  }
  {
    PanelP s(5);
    s.p.siec.siecJest = false;
    ScenariuszAB sc{ep(2026, 11, 4, 7, 20), ep(2026, 11, 4, 7, 20)};
    std::vector<WybP> v;
    uint32_t t = ep(2026, 11, 4, 4, 0);
    PowodWybudzenia pw = PowodWybudzenia::START;
    for (int i = 0; i < 200 && t < ep(2026, 11, 4, 9, 0); i++) {
      s.p.siec.siecJest = t >= ep(2026, 11, 4, 6, 40);
      odpowiedzi(s, sc, t);
      WybP w = wybudz(s, t, pw);
      v.push_back(w);
      if (!w.spal) break;
      t += s.p.zegar.msTeraz / 1000 + (s.p.sen.plan.sekund ? s.p.sen.plan.sekund : 1);
      pw = PowodWybudzenia::ZEGAR;
    }
    int gestePrzed = 0, gestePo = 0;
    for (auto& w : v) {
      if (w.powody.find("okno_geste") == std::string::npos) continue;
      (w.minuta < 7 * 60 ? gestePrzed : gestePo)++;
    }
    ROWNE_L(gestePrzed, 0);
    SPRAWDZ(gestePo >= 3);  // 07:05, 07:10, 07:15 (terapia), noc 07:20 albo 07:25
    uint32_t n = chwilaNocy(v);
    SPRAWDZ(n > 0 && n - sc.terapiaDo <= 5 * 60 + 30);
  }
}

// W czasie pobrania D przytrzymanie może zostać wykryte z opóźnieniem (żądanie HTTP i NTP
// nie sprawdzają przycisków), więc mierzone krócej. Na ekranach, na których D pobiera (ekran dnia bez pytania tak/nie
// i strony kokpitu), krótkie naciśnięcie nigdy nie zapisuje: przytrzymanie uznane za krótkie traci co najwyżej
// swój zapis albo usługę, a nie zapisuje niczego innego. Sprawdzone na mapie przycisków dla wszystkich wariantów.
static bool zapisuje(Akcja a) {
  switch (a) {
    case Akcja::PYT_ZAPISZ: case Akcja::PYT_POMIN: case Akcja::WIEM: case Akcja::OMOWIONE_TAK: case Akcja::ODP_TAK:
    case Akcja::ODP_NIE: case Akcja::ODP_POMIN: case Akcja::COFNIJ_WIEM: case Akcja::COFNIJ_ODP:
    case Akcja::SPRZET_WYMIENIONE: case Akcja::SPRZET_COFNIJ:
    case Akcja::SIEC_PANELU_START: case Akcja::AKT_SPRAWDZ: case Akcja::AKT_INSTALUJ: case Akcja::AKT_PONOW:
      return true;
    default: return false;
  }
}
static void testKrotkieNaEkranachD() {
  std::vector<Ekran> ekrany = {Ekran::DZIEN};
  for (int e = 0; e <= (int)Ekran::AKTUALIZACJA; e++)
    if (numerStrony((Ekran)e) > 0) ekrany.push_back((Ekran)e);
  SPRAWDZ(ekrany.size() >= 1 + STRON_KOKPITU);
  int sprawdzonych = 0;
  for (Ekran e : ekrany)
    for (int war = 0; war < 256; war++) {
      KontekstNaw k;
      k.s5 = war & 1; k.s5Bateria = war & 2; k.zaproszenie = war & 4; k.ostatniaNoc = war & 8; k.nocny = war & 16;
      k.wzorzec = war & 32; k.problem = war & 64; k.sk31 = war & 128;
      k.sprawyDostepne = true; k.spraw = 3; k.sprawa = 2;
      for (Przycisk p : {Przycisk::LEWY, Przycisk::SRODEK, Przycisk::PRAWY}) {
        SPRAWDZ(!zapisuje(nacisniecie(e, p, false, k).akcja));
        sprawdzonych++;
      }
    }
  // dla porównania: pytanie tak/nie na ekranie dnia zapisuje krótkim naciśnięciem, więc tam D nie pobiera
  KontekstNaw tn;
  tn.pytanieTN = true;
  SPRAWDZ(zapisuje(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, tn).akcja));
  SPRAWDZ(sprawdzonych > 0);
}

// D (R-PRZ-22): wybudzenie przyciskiem rano bez nocy → akcja przycisku i pobranie przerywalne; limit raz na 2 min.
static WybP przycisk(PanelP& s, uint32_t t, uint8_t bit, Ekran e = Ekran::DZIEN) {
  s.p.start.budzace = bit;
  s.p.start.stan.ekran = (uint8_t)e;
  s.p.start.stan.ekranOd = t - 600;
  s.p.przyciski.skrypt = {{0, bit}, {150, 0}};
  return wybudz(s, t, PowodWybudzenia::PRZYCISK);
}
static void testR_PRZ_22_dyrygent() {
  ScenariuszAB jest;  // AirBridge ma noc
  {  // ◀ na ekranie dnia (bez akcji): pobranie, noc w historii, ekran dnia narysowany od nowa
    PanelP s(5);
    symuluj(s, ScenariuszAB{ep(2026, 11, 4, 9, 0), ep(2026, 11, 4, 9, 0)}, ep(2026, 11, 4, 4, 0),
            ep(2026, 11, 4, 6, 0));
    SPRAWDZ(!s.maNoc());
    uint32_t t = ep(2026, 11, 4, 6, 10, 30);
    odpowiedzi(s, jest, t);
    WybP w = przycisk(s, t, BIT_LEWY);
    ROWNE_L(w.polaczen, 1);
    SPRAWDZ(w.noc);
    // akcja ◀ bez zmiany (klatka jak wyświetlona, bez odświeżenia e-ink), potem ekran dnia z nową nocą
    bool odNowa = false;
    for (auto& z : s.p.d.z) odNowa = odNowa || z.find("nowa noc po przycisku, ekran dnia od nowa") != std::string::npos;
    SPRAWDZ(odNowa);
    ROWNE_L(w.odswiezen, 1);
    ROWNE_L(s.p.start.stan.ekran, (int)Ekran::DZIEN);
    SPRAWDZ(w.powody == "przycisk");  // dziennik wybudzeń bez zmiany formatu (Wi-Fi w kolumnie wifi_ms)
  }
  {  // ▶ na ekranie dnia: K1, pobranie w tle akcji, bez przerysowania ekranu dnia
    PanelP s(5);
    uint32_t t = ep(2026, 11, 4, 6, 10, 30);
    odpowiedzi(s, jest, t);
    WybP w = przycisk(s, t, BIT_PRAWY);
    ROWNE_L(s.p.start.stan.ekran, (int)Ekran::K1);
    ROWNE_L(w.polaczen, 1);
    SPRAWDZ(w.noc);
    ROWNE_L(w.odswiezen, 1);
  }
  {  // kilka naciśnięć z rzędu: drugie po 60 s bez pobrania, trzecie po 2 min z pobraniem
    PanelP s(5);
    ScenariuszAB terapia{ep(2026, 11, 4, 9, 0), ep(2026, 11, 4, 9, 0)};
    uint32_t t = ep(2026, 11, 4, 6, 10, 30);
    odpowiedzi(s, terapia, t);
    ROWNE_L(przycisk(s, t, BIT_PRAWY).polaczen, 1);
    odpowiedzi(s, terapia, t + 60);
    ROWNE_L(przycisk(s, t + 60, BIT_PRAWY, Ekran::K1).polaczen, 0);
    odpowiedzi(s, terapia, t + 125);
    ROWNE_L(przycisk(s, t + 125, BIT_LEWY, Ekran::K2).polaczen, 1);
  }
  {  // (R-KOK-19, R-PRZ-22): strony kokpitu bez numeru też pobierają, U-01 nie
    PanelP s(5);
    s.p.pliki.pliki["/konfiguracja.txt"] = "profil=publiczny\n";
    uint32_t t = ep(2026, 11, 4, 6, 10, 30);
    odpowiedzi(s, jest, t);
    WybP w = przycisk(s, t, BIT_PRAWY, Ekran::K2);
    ROWNE_L(s.p.start.stan.ekran, (int)Ekran::LICZBY);
    ROWNE_L(w.polaczen, 1);
  }
  {
    PanelP s(5);
    uint32_t t = ep(2026, 11, 4, 6, 10, 30);
    odpowiedzi(s, jest, t);
    WybP w = przycisk(s, t, BIT_LEWY, Ekran::TEST);  // ◀ z U-05 → U-01
    ROWNE_L(s.p.start.stan.ekran, (int)Ekran::USTAWIENIA);
    ROWNE_L(w.polaczen, 0);
  }
  {  // po 11:00 bez nocy: bez zmian względem stanu przed zmianą (bez pobrania)
    PanelP s(5);
    uint32_t t = ep(2026, 11, 4, 12, 0, 30);
    odpowiedzi(s, jest, t);
    WybP w = przycisk(s, t, BIT_PRAWY);
    ROWNE_L(w.polaczen, 0);
    SPRAWDZ(!w.noc);
  }
  {  // przed 05:00 (slot nocny): bez pobrania
    PanelP s(5);
    uint32_t t = ep(2026, 11, 4, 4, 50, 0);
    odpowiedzi(s, jest, t);
    ROWNE_L(przycisk(s, t, BIT_PRAWY).polaczen, 0);
  }
  {  // noc już jest: bez pobrania
    PanelP s(5);
    uint32_t t = ep(2026, 11, 4, 7, 30, 5);
    odpowiedzi(s, jest, t);
    wybudz(s, t, PowodWybudzenia::START);
    SPRAWDZ(s.maNoc());
    odpowiedzi(s, jest, t + 600);
    ROWNE_L(przycisk(s, t + 600, BIT_PRAWY).polaczen, 0);
  }
}

// ============================================================================================================
// (d) Budżet energii doby (spec techniczna 8.1): wybudzenia i wybudzenia z Wi-Fi przed zmianą i po niej
// ============================================================================================================

struct Doba {
  int wybudzen = 0, zWifi = 0, powtorek = 0, gestych = 0;
};
static Doba policzDobe(int nocyHistorii, const ScenariuszAB& sc) {
  PanelP s(nocyHistorii);
  std::vector<WybP> v = symuluj(s, sc, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 4));
  Doba d;
  for (auto& w : v) {
    if (w.powod != PowodWybudzenia::ZEGAR) continue;
    d.wybudzen++;
    d.zWifi += w.polaczen > 0;
  }
  d.powtorek = zPowodem(v, "powtorka_przetwarzania");
  d.gestych = zPowodem(v, "okno_geste");
  return d;
}
static void testR_NF_01_budzetDoby() {
  // typowa doba: koniec terapii 07:02, bez przetwarzania; przed zmianą (2 noce historii) i po (5 nocy)
  ScenariuszAB typowa{ep(2026, 11, 4, 7, 2), ep(2026, 11, 4, 7, 2)};
  Doba przed = policzDobe(2, typowa), po = policzDobe(5, typowa);
  // najgorsza: terapia do końca gęstego okna (07:46) i przetwarzanie dłuższe niż 5 powtórek
  ScenariuszAB najgorsza{ep(2026, 11, 4, 7, 46), ep(2026, 11, 4, 9, 15)};
  Doba nPrzed = policzDobe(2, najgorsza), nPo = policzDobe(5, najgorsza);
  printf("  budżet 8.1, wybudzenia zegarem (z Wi-Fi): typowa przed %d (%d), po %d (%d), gęste %d; najgorsza przed %d "
         "(%d), po %d (%d), gęste %d, powtórki %d\n",
         przed.wybudzen, przed.zWifi, po.wybudzen, po.zWifi, po.gestych, nPrzed.wybudzen, nPrzed.zWifi, nPo.wybudzen,
         nPo.zWifi, nPo.gestych, nPo.powtorek);
  SPRAWDZ(po.zWifi > przed.zWifi);
  SPRAWDZ(po.zWifi - przed.zWifi <= 10);  // 06:15…07:05 bez 06:30 i 07:00: 9
  SPRAWDZ(nPo.powtorek == PLAN_POWTORKI_MAKS);
  SPRAWDZ(nPo.zWifi - nPrzed.zWifi <= 16 + PLAN_POWTORKI_MAKS);
  SPRAWDZ(po.wybudzen <= 80);  // budżet 8.1 (bez naciśnięć)
  // doba bez danych (AirBridge milczy, pierwsza doba): zapytania planem przed zmianą i po niej; każde ok. 68 s Wi-Fi
  // (3 próby, R-AW-01). Bez decyzji z 10.11.2026 gęste okno dokładałoby 16 × 68 s ≈ 18 min; z nią (bez gęstego okna
  // po próbie bez odpowiedzi) tylko pierwsze zapytanie dnia jest przed E-01, więc liczba jak przed zmianą
  Poranek bezDanych;
  bezDanych.koniecTerapii = 24 * 60;  // noc nie przychodzi
  bezDanych.zwyklyKoniec = -1;
  int zPrzed = symulujPlan(bezDanych).zapytan;
  bezDanych.zwyklyKoniec = 7 * 60;
  int zBezDecyzji = symulujPlan(bezDanych).zapytan;
  bezDanych.e01 = true;
  int zPo = symulujPlan(bezDanych).zapytan;
  printf("  doba bez danych, zapytania z Wi-Fi: przed %d, bez decyzji z 10.11 %d, po %d (Wi-Fi ok. %d, %d i %d min "
         "przy 68 s)\n", zPrzed, zBezDecyzji, zPo, zPrzed * 68 / 60, zBezDecyzji * 68 / 60, zPo * 68 / 60);
  ROWNE_L(zPrzed, 38);
  ROWNE_L(zBezDecyzji - zPrzed, 16);
  ROWNE_L(zPo, zPrzed);
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  // (a)
  testR_DANE_61_zwyklaPora();
  testR_DANE_61_gesteOkno();
  testR_DANE_61_terminy();
  testR_DANE_62_powtorka();
  testR_PRZ_22_warunek();
  // (b)
  testKryteria_symulacjePlanem();
  // (c)
  testKryteria_dyrygentZwyklaPora();
  testKryteria_dyrygentPrzetwarzanie();
  testKryteria_dyrygentPozaOknem();
  testKryteria_dyrygentKrotkaHistoria();
  testR_DANE_61_bezOdpowiedzi();
  testR_PRZ_22_dyrygent();
  testKrotkieNaEkranachD();
  // (d)
  testR_NF_01_budzetDoby();
  printf("testy_poranek_szybciej: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
