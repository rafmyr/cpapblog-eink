// SPDX-License-Identifier: GPL-3.0-or-later
// Testy alarmu słabej baterii: M-25 z napięcia
// (krzywa odcinkami Li-Po 1S, src/bateria.cpp procentBaterii, [PRÓG]), wygładzenie między wybudzeniami
// (wygladzProcent()), M-25 w sygnałach silnika reguł, E-05 linia ≤ 15% (grupa „awarie” 7.5, przed awarią pamięci),
// S5 przy E-05 krytycznym ≤ 5% (R1, zawsze ekran danych, R-HUM-08), wyjście > 20% (6.4), R-PIE-07 (wzorzec A
// w dniu pełnego ekranu, E-05 krytyczne jako linia-ostrzeżenie R-EKR-52), brak napięcia = „nie oceniam”, nigdy alarm.
// ↻ na S5 baterii = nowy pomiar bez Wi-Fi, stopka `↻ odśwież`; S5 baterii
// zostaje do 05:00 (bez S9); granica odczytu 3,0 V; chwilowy brak odczytu: procent z RTC przez jedno wybudzenie;
// S10 w dniu pełnego ekranu A z linią baterii; następnego dnia po R-PIE-07 S5 baterii z linią R-SKI-22.
// Dane syntetyczne. Uruchomienie: tools/prototyp/testy.sh (program testy_alarm_baterii). T-TEST-359.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "bateria.h"
#include "cykl.h"
#include "dyrygent.h"
#include "dzien.h"
#include "kolejka.h"
#include "nawigacja.h"
#include "sprawy.h"
#include "wspolne.h"
#include "wybudzenie.h"
#include "zestaw_kolejki.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE_L(a, b)                                                                                   \
  do {                                                                                                  \
    long long _a = (long long)(a), _b = (long long)(b);                                                 \
    if (_a == _b) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); } \
  } while (0)
#define ROWNE(a, b)                                                                                                \
  do {                                                                                                             \
    std::string _a = (a), _b = (b);                                                                                \
    if (_a == _b) ok++;                                                                                            \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)

// ---------- krzywa napięcie → procent ----------
static void testKrzywa() {
  ROWNE_L(procentBaterii(4150), 100);
  ROWNE_L(procentBaterii(4200), 100);  // przy ładowaniu po USB napięcie > 4,15 V: 100% (Uproszczenia README)
  ROWNE_L(procentBaterii(3300), 0);
  ROWNE_L(procentBaterii(3200), 0);
  ROWNE_L(procentBaterii(3710), 15);   // punkt krzywej = próg E-05.bateria_linia
  ROWNE_L(procentBaterii(3610), 5);    // punkt krzywej = próg E-05.bateria_krytyczna
  ROWNE_L(procentBaterii(3730), 20);   // punkt krzywej = E-05.bateria_koniec
  ROWNE_L(procentBaterii(3705), 14);
  ROWNE_L(procentBaterii(3548), 4);
  ROWNE_L(procentBaterii(3735), 21);
  ROWNE_L(procentBaterii(3950), 70);   // napięcie atrapy czujników (3,95 V)
  ROWNE_L(procentBaterii(0), -1);      // brak odczytu
  ROWNE_L(procentBaterii(2400), -1);   // poza zakresem ogniwa
  ROWNE_L(procentBaterii(2999), -1);   // poniżej 3,0 V błąd pomiaru, nie 0%
  ROWNE_L(procentBaterii(3000), 0);
  ROWNE_L(procentBaterii(3150), 0);
  ROWNE_L(procentBaterii(4600), -1);
  int pop = 0;
  bool mono = true;
  for (int mv = 3300; mv <= 4150; mv++) {
    int p = procentBaterii(mv);
    mono = mono && p >= pop && p >= 0 && p <= 100;
    pop = p;
  }
  SPRAWDZ(mono);  // niemalejąca w całym zakresie
  bool posort = true;
  for (int i = 1; i < KRZYWA_BATERII_N; i++)
    posort = posort && KRZYWA_BATERII[i].mV < KRZYWA_BATERII[i - 1].mV && KRZYWA_BATERII[i].proc < KRZYWA_BATERII[i - 1].proc;
  SPRAWDZ(posort);
  SPRAWDZ(KRZYWA_BATERII[0].mV == 4150 && KRZYWA_BATERII[0].proc == 100);
  SPRAWDZ(KRZYWA_BATERII[KRZYWA_BATERII_N - 1].mV == 3300 && KRZYWA_BATERII[KRZYWA_BATERII_N - 1].proc == 0);
}

// ---------- wygładzenie (histereza pomiaru) ----------
static void testWygladzenie() {
  StanProcentuBaterii s;
  ROWNE_L(wygladzProcent(s, 50), 50);  // pierwszy pomiar od startu zasilania
  ROWNE_L(wygladzProcent(s, 49), 49);  // mały spadek od razu
  ROWNE_L(wygladzProcent(s, 51), 49);  // mały wzrost nie
  ROWNE_L(wygladzProcent(s, 30), 49);  // jeden odstający spadek nic nie zmienia
  ROWNE_L(wygladzProcent(s, 49), 49);
  ROWNE_L(wygladzProcent(s, 40), 49);  // spadek potwierdzony drugim pomiarem: do bliższej wartości
  ROWNE_L(wygladzProcent(s, 38), 40);
  ROWNE_L(wygladzProcent(s, 90), 40);  // jeden skok w górę (np. po paczce Wi-Fi albo szum) nie
  ROWNE_L(wygladzProcent(s, 40), 40);
  ROWNE_L(wygladzProcent(s, 95), 40);  // ładowanie: dwa kolejne pomiary wyżej
  ROWNE_L(wygladzProcent(s, 92), 92);
  // chwilowy brak odczytu (jedno wybudzenie): ostatni procent z RTC, E-05 bez zmian; drugi kolejny: brak M-25
  ROWNE_L(wygladzProcent(s, -1), 92);
  ROWNE_L(s.proc, 92);
  ROWNE_L(wygladzProcent(s, -1), -1);
  ROWNE_L(s.proc, 92);
  ROWNE_L(wygladzProcent(s, 91), 91);
  ROWNE_L(wygladzProcent(s, -1), 91);  // po udanym pomiarze znowu jedno wybudzenie z RTC
  ROWNE_L(wygladzProcent(s, 60), 91);  // duży spadek po braku odczytu: niepotwierdzony (brak to nie pomiar)
  ROWNE_L(wygladzProcent(s, 60), 60);
  StanProcentuBaterii u;               // po starcie zasilania (RTC bez procentu): brak odczytu = brak M-25
  ROWNE_L(wygladzProcent(u, -1), -1);
  // szum ±3 pkt wokół progu E-05: opublikowany procent nie skacze (nie włącza i wyłącza linii co wybudzenie)
  StanProcentuBaterii t;
  wygladzProcent(t, 14);
  bool stale = true;
  for (int i = 0; i < 20; i++) stale = stale && wygladzProcent(t, i % 2 ? 17 : 14) == 14;
  SPRAWDZ(stale);
}

// ---------- kolejka 7.5: E-05 w grupie „awarie” przed awarią pamięci; krytyczna jako ostrzeżenie ----------
static void testKolejka() {
  std::vector<sprawy::SprawaL> sp;
  std::vector<sprawy::LiniaZdarzenia> le = {{"E-04", "dane", "Noc zapisana częściowo."},
                                            {"E-05/linia", "awarie", "Ekran: bateria 14%, naładuj w tym tygodniu."}};
  kolejka::WejscieKolejki k;
  k.dzis = "2026-11-05";
  k.sprawy = &sp;
  k.linieE = &le;
  k.awaria = "Pamięć panelu: błąd zapisu.";
  std::vector<kolejka::LiniaUwagi> l = kolejka::kolejkaLinii(k);
  SPRAWDZ(l.size() == 3);
  if (l.size() == 3) {
    ROWNE(l[0].zrodlo, "E-05/linia");  // R-EKR-12: E-05 wygrywa z E-04,: przed pamięcią
    ROWNE(l[1].zrodlo, "awaria");
    ROWNE(l[2].zrodlo, "E-04");
    SPRAWDZ(!l[0].zdrowie());  // linia E-05 to informacja: biały pasek (R-EKR-52)
  }
  k.bateriaKrytyczna = "Naładuj ekran: bateria 4%.";
  l = kolejka::kolejkaLinii(k);
  SPRAWDZ(!l.empty() && l[0].zrodlo == "E-05/krytyczne" && l[0].zdrowie());  // R-EKR-52: czarny pasek z `!`
}

// ---------- cykl: S5 z E-05 krytycznym ----------
static void testCykl() {
  std::vector<Noc> h;
  Noc n;
  n.data = "2026-11-05";
  n.m01 = 420;
  h.push_back(n);
  WejscieDnia w;
  w.dzis = "2026-11-05";
  w.minuta = 8 * 60;
  w.hist = &h;
  w.odczytOk = true;
  SPRAWDZ(!brakDanych(w));
  w.e05Krytyczne = true;
  SPRAWDZ(brakDanych(w) && !brakDanychAirBridge(w));
  StanDnia s = stanDnia(w);
  SPRAWDZ(s.brakDanych && !s.kartaBraku);  // R-HUM-08: zawsze ekran danych
  ROWNE(s.kluczBraku, "E-05");
  // E-05 krytyczne bez nocy z dziś przed 11:00: S5 baterii zamiast S10 (bateria grozi wyłączeniem panelu)
  WejscieDnia v = w;
  v.hist = nullptr;
  v.odczytOk = false;
  SPRAWDZ(brakDanych(v) && !synchronizacja(v));
  v.e05Krytyczne = false;
  SPRAWDZ(synchronizacja(v));
}

// ---------- dyrygent z atrapami: M-25 → silnik reguł → linia, S5, R-PIE-07 ----------
static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static std::string raportNocy(long day) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static uint32_t EP_TERAZ = 0;  // chwila ostatniego wybudzenia (ocenaPoPlanie())
// Wybudzenie dnia `dzien` listopada o h:m z napięciem v; bezNocy: AirBridge bez nocy z dziś (S10 przed 11:00);
// przycisk: wybudzenie przyciskiem (bit budzący i skrypt naciśnięć)
static void wybudzD(Panel& p, int dzien, int h, int m, float v, bool bezNocy = false,
                    PowodWybudzenia pw = PowodWybudzenia::ZEGAR, uint8_t bit = 0,
                    std::vector<std::pair<uint32_t, uint8_t>> skrypt = {}) {
  p.zegar.epoch = ep(2026, 11, dzien, h, m);
  EP_TERAZ = p.zegar.epoch;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = pw;
  p.start.budzace = bit;
  p.przyciski.skrypt = skrypt;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  char d[11];
  snprintf(d, sizeof d, "2026-11-%02d", dzien);
  long doba = nrDnia(d) - 1;
  p.siec.dodaj("/api/status", 200, STATUS);
  p.siec.dodaj(sciezkaListy(), 200, bezNocy ? std::string("{\"source\":\"edf\",\"days\":[]}")
                                            : "{\"source\":\"edf\",\"days\":[{\"day\":" + std::to_string(doba) +
                                                  ",\"session_count\":1,\"stale\":false}]}");
  if (!bezNocy) p.siec.dodaj(sciezkaNocy(doba), 200, raportNocy(doba));
  p.czujniki.o.bateriaV = v;
  p.port.log.clear();
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static void wybudz(Panel& p, int h, int m, float v) { wybudzD(p, 5, h, m, v); }
static bool wLogu(const Panel& p, const std::string& f) {
  for (auto& l : p.port.log)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static void zrzutLogu(const Panel& p) {
  if (getenv("BAT_LOG"))
    for (auto& l : p.port.log) printf("    %s\n", l.c_str());
}

static reguly::Sygnaly SYG;
static void lapSygnaly(const reguly::Sygnaly& s) { SYG = s; }
static int m25() {
  auto it = SYG.odczyty.find("M-25");
  return it == SYG.odczyty.end() ? -1 : (int)std::lround(it->second);
}

// Wybudzenia zegarem w terminach planu (log `sen na N s`) z napięciem `v`, aż silnik reguł policzy się dwa razy;
// zwraca M-25 z ostatniej oceny (-1 = brak).
static int ocenaPoPlanie(Panel& p, float v) {
  int ocen = 0;
  for (int i = 0; i < 20 && ocen < 2; i++) {
    uint32_t sen = 0;
    for (auto& l : p.port.log)
      if (l.compare(0, 7, "sen na ") == 0) sen = (uint32_t)atol(l.c_str() + 7);
    EP_TERAZ += sen ? sen : 1800;
    p.zegar.epoch = EP_TERAZ;
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.czujniki.o.bateriaV = v;
    p.port.log.clear();
    SYG = reguly::Sygnaly();
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    if (wLogu(p, "silnik reguł")) ocen++;
  }
  return m25();
}

static void testDyrygentZestawWbudowany() {
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
  {  // 14%: linia E-05 w kolejce (grupa „awarie”), ekran dnia bez S5
    Panel p;
    wybudz(p, 9, 0, 3.705f);
    zrzutLogu(p);
    SPRAWDZ(wLogu(p, "linia uwagi: Ekran: bateria 14%, naładuj w tym tygodniu. (E-05/linia)"));
    SPRAWDZ(!wLogu(p, "S5:"));
    SPRAWDZ(wLogu(p, "spełnione 1"));  // jedyna spełniona reguła tej historii: E-05/linia
    // histereza 6.4 w kolejnych wybudzeniach według planu (wybudzenia pomiarowe bez rysowania też mierzą baterię
    // i liczą się do wygładzenia): przy 18% linia trwa, znika dopiero > 20%, po wyjściu 18% jej nie przywraca
    sprawy::zrodloTestowe.sygnaly = lapSygnaly;
    ROWNE_L(ocenaPoPlanie(p, 3.72f), 18);
    SPRAWDZ(wLogu(p, "spełnione 1"));  // 18% ≤ 20%: linia dalej (pamięć reguł)
    ROWNE_L(ocenaPoPlanie(p, 3.745f), 24);
    SPRAWDZ(wLogu(p, "spełnione 0"));  // 24% > 20%: E-05 znika
    ROWNE_L(ocenaPoPlanie(p, 3.72f), 18);
    SPRAWDZ(wLogu(p, "spełnione 0"));  // wejście dopiero ≤ 15%
    ROWNE_L(ocenaPoPlanie(p, 3.705f), 14);
    SPRAWDZ(wLogu(p, "spełnione 1"));
    sprawy::zrodloTestowe.sygnaly = nullptr;
  }
  {  // 4%: E-05 krytyczne → S5 `Naładuj ekran` / `Bateria 4%.` jako ekran danych, linia E-05 wykluczona
    Panel p;
    wybudz(p, 9, 0, 3.548f);
    zrzutLogu(p);
    SPRAWDZ(wLogu(p, "S5: Naładuj ekran / Bateria 4%."));
    SPRAWDZ(!wLogu(p, "Ekran: bateria"));
  }
  {  // 21%: nic
    Panel p;
    wybudz(p, 9, 0, 3.735f);
    SPRAWDZ(!wLogu(p, "Ekran: bateria") && !wLogu(p, "Naładuj"));
  }
  {  // 2,9 V to błąd pomiaru, nie bateria 0%: brak M-25, „nie oceniam”, bez S5
    Panel p;
    wybudz(p, 9, 0, 2.9f);
    SPRAWDZ(!wLogu(p, "Naładuj") && !wLogu(p, "S5:"));
    SPRAWDZ(wLogu(p, "E-05/krytyczne (brak odczytu M-25)"));
  }
  {  // brak napięcia: brak M-25, E-05 „nie oceniam” (nigdy alarm ani „w porządku”)
    Panel p;
    wybudz(p, 9, 0, NAN);
    zrzutLogu(p);
    SPRAWDZ(!wLogu(p, "Ekran: bateria") && !wLogu(p, "Naładuj") && !wLogu(p, "S5:"));
    SPRAWDZ(wLogu(p, "E-05/krytyczne (brak odczytu M-25)") && wLogu(p, "E-05/linia (brak odczytu M-25)"));
  }
}

// R-PIE-07 na atrapie wyników (L-06(b) z pilnością A w dniu wejścia) i E-05 krytycznym z wynikiem dopisanym
static reguly::Zestaw ZESTAW_A = atrapa_b5::zestaw();
static bool Z_A = true;
static std::vector<reguly::Wynik> wynikiA(const reguly::Kontekst&) {
  std::vector<reguly::Wynik> w;
  for (auto& k : ZESTAW_A.katalog) {
    std::string kl = k.wariant.empty() ? k.id : k.id + "/" + k.wariant;
    w.push_back(atrapa_b5::wynik(ZESTAW_A, k.id, k.wariant, k.stan != reguly::StanReguly::ZABLOKOWANA,
                                 Z_A && kl == "L-06/b", {{"x", "12,4"}, {"n", "3"}}));
  }
  reguly::Wynik e;
  e.id = "E-05";
  e.wariant = "krytyczne";
  e.oceniona = e.spelniona = true;
  e.poziom = "brak_danych";
  e.grupa = "R1";
  e.zmienne["x"] = "4";
  w.push_back(e);
  return w;
}
static void testRPie07() {
  sprawy::zrodloTestowe.wyniki = wynikiA;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_A;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_A);
  {  // dzień pełnego ekranu A: ekran L-06(b), linia `Naładuj ekran: bateria 4%.`, bez S5
    Z_A = true;
    Panel p;
    wybudz(p, 9, 0, 3.548f);
    zrzutLogu(p);
    SPRAWDZ(wLogu(p, "ekran dnia: wzorzec L-06/b (lekarz)"));
    SPRAWDZ(wLogu(p, "linia uwagi: Naładuj ekran: bateria 4%. (E-05/krytyczne)"));
    SPRAWDZ(!wLogu(p, "S5:"));
  }
  {  // bez wzorca A: S5 baterii z tekstami z zestawu wbudowanego (atrapa ich nie ma)
    Z_A = false;
    Panel p;
    wybudz(p, 9, 0, 3.548f);
    zrzutLogu(p);
    SPRAWDZ(wLogu(p, "S5: Naładuj ekran / Bateria 4%."));
    SPRAWDZ(!wLogu(p, "ekran dnia: wzorzec"));
  }
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

// ---------- ↻ na S5 baterii ----------
static void testNawigacjaS5Baterii() {
  KontekstNaw k;
  k.s5 = true;
  Przejscie pr = nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, k);
  SPRAWDZ(pr.ekran == Ekran::DZIEN && pr.akcja == Akcja::PONOW_POBRANIE);  // S5 z E-01, E-03: bez zmian (R-PRZ-06)
  k.s5Bateria = true;
  pr = nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, k);
  SPRAWDZ(pr.ekran == Ekran::DZIEN && pr.akcja == Akcja::POMIAR_BATERII);
  SPRAWDZ(!pobraniePoNacisnieciu(pr, Ekran::DZIEN, true).pobierz && !pobraniePoNacisnieciu(pr, Ekran::DZIEN, false).pobierz);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, true, k).akcja == Akcja::NIC);  // ↻ 2 s bez zaproszenia: nic
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, k).ekran == Ekran::K1);   // ▶ kokpit bez zmian
  // stopka: S5 baterii `↻ odśwież` (nie obiecuje pobrania), S5 z E-01 dalej `↻ ponów`; kolejność ◀ ▶ ↻
  DaneDnia d;
  d.dzis = "2026-11-05";
  d.e05Tytul = "Naładuj ekran";
  d.e05Powod = "Bateria 4%.";
  d.e05Krok = "→ Podłącz kabel USB-C.";
  TrescEkranu t = trescDnia("S5", "E-05", d);
  ROWNE(t.stopkaLewo, "▶ kokpit");
  ROWNE(t.stopkaPrawo, "↻ odśwież");
  DaneDnia e01;
  e01.dzis = "2026-11-05";
  ROWNE(trescDnia("S5", "123", e01).stopkaPrawo, "↻ ponów");
}

static void testPrzyciskS5Baterii() {
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
  Panel p;
  wybudz(p, 9, 0, 3.548f);
  SPRAWDZ(wLogu(p, "S5: Naładuj ekran / Bateria 4%."));
  size_t zad = p.siec.zadania.size();
  // ładowanie po USB: ↻ budzi panel (pomiar z tego wybudzenia), drugie ↻ w czuwaniu mierzy od nowa; dwa pomiary
  // wyżej potwierdzają wzrost (wygladzProcent()), więc po drugim S5 baterii znika; bez Wi-Fi i bez pobrania
  wybudzD(p, 5, 9, 10, 3.80f, false, PowodWybudzenia::PRZYCISK, BIT_SRODEK,
          {{0, BIT_SRODEK}, {300, 0}, {3000, BIT_SRODEK}, {3300, 0}});
  zrzutLogu(p);
  SPRAWDZ(wLogu(p, "bateria: ↻ na S5, pomiar 4%"));
  SPRAWDZ(wLogu(p, "bateria: ↻ na S5, pomiar 40%"));
  SPRAWDZ(!wLogu(p, "Pobieram") && !wLogu(p, "ponawiam"));
  ROWNE_L(p.siec.zadania.size(), zad);  // bez żadnego żądania do AirBridge
  // ostatni ekran po drugim ↻: nowy wybór bez S5 baterii
  std::string ost;
  for (auto& l : p.port.log)
    if (l.compare(0, 4, "S5: ") == 0 || l.compare(0, 11, "ekran dnia ") == 0 || l.find("bateria: ↻") == 0) ost = l;
  SPRAWDZ(ost.find("bateria: ↻ na S5, pomiar 40%") == 0 || ost.compare(0, 4, "S5: ") != 0);
}

// ---------- S5 baterii zostaje do 05:00 ----------
static void testNocS5Baterii() {
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
  {  // 21:00 S5 baterii → 23:35 bez S9 „Dobranoc”
    Panel p;
    wybudz(p, 21, 0, 3.548f);
    SPRAWDZ(wLogu(p, "S5: Naładuj ekran / Bateria 4%."));
    wybudz(p, 23, 35, 3.548f);
    zrzutLogu(p);
    SPRAWDZ(wLogu(p, "bez ekranu nocnego (R-KON-35)"));
    SPRAWDZ(!wLogu(p, "ekran nocny ("));
    ROWNE_L(p.start.stan.ekran, (int)Ekran::DZIEN);
  }
  {  // kontrola: bateria 70% → o 23:35 S9
    Panel p;
    wybudz(p, 21, 0, 3.95f);
    wybudz(p, 23, 35, 3.95f);
    SPRAWDZ(wLogu(p, "ekran nocny ("));
  }
  {  // cykl: w slocie nocnym E-05 krytyczne = S5 baterii, nawet gdy w dzienniku stoi inny ekran
    std::vector<WpisHumoru> dz(1);
    dz[0].data = "2026-11-05";
    dz[0].typ = "S2A";
    dz[0].tryb = 'K';
    WejscieDnia w;
    w.dzis = "2026-11-05";
    w.minuta = 23 * 60 + 35;
    w.odczytOk = true;
    DecyzjaDnia d = decyzjaDnia(w, dz);
    ROWNE(d.e.typ, "S2A");
    w.e05Krytyczne = true;
    d = decyzjaDnia(w, dz);
    ROWNE(d.e.typ, "S5");
    ROWNE(d.e.klucz, "E-05");
    SPRAWDZ(!d.wpis && !d.nowyWybor && !d.zapisz);
  }
}

// ---------- chwilowy brak odczytu w dyrygencie ----------
static void testBrakOdczytuDyrygent() {
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
  sprawy::zrodloTestowe.sygnaly = lapSygnaly;
  Panel p;
  wybudz(p, 9, 0, 3.548f);
  SPRAWDZ(wLogu(p, "S5: Naładuj ekran / Bateria 4%."));
  // wybudzenie przyciskiem ↻ (zegarem o 09:30 panel tylko mierzy, bez silnika): jedno wybudzenie bez napięcia,
  // M-25 = ostatni procent z RTC, E-05 bez zmian (S5 baterii zostaje)
  SYG = reguly::Sygnaly();
  wybudzD(p, 5, 9, 10, NAN, false, PowodWybudzenia::PRZYCISK, BIT_SRODEK, {{0, BIT_SRODEK}, {300, 0}});
  zrzutLogu(p);
  ROWNE_L(m25(), 4);
  SPRAWDZ(wLogu(p, "S5: Naładuj ekran / Bateria 4%."));
  // drugie wybudzenie z rzędu bez napięcia: brak M-25, „nie oceniam”, S5 baterii znika (nigdy alarm z braku odczytu)
  SYG = reguly::Sygnaly();
  wybudzD(p, 5, 9, 12, NAN, false, PowodWybudzenia::PRZYCISK, BIT_SRODEK, {{0, BIT_SRODEK}, {300, 0}});
  ROWNE_L(m25(), -1);
  SPRAWDZ(!wLogu(p, "S5: Naładuj"));
  sprawy::zrodloTestowe.sygnaly = nullptr;
}

// ---------- S10 w dniu pełnego ekranu A, następny dzień po R-PIE-07 ----------
static void testRPie07Dalej() {
  sprawy::zrodloTestowe.wyniki = wynikiA;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_A;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_A);
  {  // noc z dziś jeszcze w drodze (S10 przed 11:00) w dniu pełnego ekranu A → S10 z linią baterii
    Z_A = true;
    Panel p;
    wybudzD(p, 5, 9, 0, 3.548f, true);
    zrzutLogu(p);
    SPRAWDZ(wLogu(p, "ekran dnia S10"));
    SPRAWDZ(wLogu(p, "linia uwagi: Naładuj ekran: bateria 4%. (E-05/krytyczne)"));
  }
  {  // bez wzorca A: S5 baterii wygrywa z S10
    Z_A = false;
    Panel p;
    wybudzD(p, 5, 9, 0, 3.548f, true);
    SPRAWDZ(wLogu(p, "S5: Naładuj ekran / Bateria 4%."));
    SPRAWDZ(!wLogu(p, "ekran dnia S10"));
  }
  {  // (R-PIE-07, przypadek 13 z 7.8): dzień 1 ekran L-06(b) z linią baterii, dzień 2 S5 baterii z linią
     // R-SKI-22 `Wciąż: …` (pełny ekran A co 3 dni, R-SKI-10)
    Z_A = true;
    Panel p;
    wybudzD(p, 5, 9, 0, 3.548f);
    SPRAWDZ(wLogu(p, "ekran dnia: wzorzec L-06/b (lekarz)"));
    SPRAWDZ(wLogu(p, "linia uwagi: Naładuj ekran: bateria 4%. (E-05/krytyczne)"));
    wybudzD(p, 6, 9, 0, 3.548f);
    zrzutLogu(p);
    SPRAWDZ(wLogu(p, "S5: Naładuj ekran / Bateria 4%."));
    SPRAWDZ(wLogu(p, "linia uwagi: Wciąż: "));
    SPRAWDZ(!wLogu(p, "ekran dnia: wzorzec"));
  }
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

int main() {
  testKrzywa();
  testWygladzenie();
  testKolejka();
  testCykl();
  testDyrygentZestawWbudowany();
  testRPie07();
  testNawigacjaS5Baterii();
  testPrzyciskS5Baterii();
  testNocS5Baterii();
  testBrakOdczytuDyrygent();
  testRPie07Dalej();
  printf("testy baterii E-05: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
