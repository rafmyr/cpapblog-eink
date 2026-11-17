// SPDX-License-Identifier: GPL-3.0-or-later
// Rodzaj odświeżenia e-ink (R-NF-25, T-INT-27, T-KOMP-17, T-TEST-439): ekran dnia i odświeżenia z własnej inicjatywy
// panelu pełne, widoki z przycisku częściowe, co N-te częściowe pełne, N z klucza `eink.pelne_co`, pierwsze
// rysowanie po głębokim śnie i po starcie pełne, sterownik bez częściowego = same pełne, klatka bez zmian bez
// odświeżenia i bez zmiany licznika. Pisane przed kodem z kryteriów R-NF-25 (16.11.2026).
// Atrapa wyświetlacza zapisuje rodzaj każdego odświeżenia (AtrapaWyswietlacza::rodzaje).
// Uruchomienie: tools/prototyp/testy.sh (program testy_odswiezanie_czesciowe).
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "nawigacja.h"
#include "odswiezanie.h"
#include "slot_dzienny.h"
#include "sprawy.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

using odswiezanie::Wejscie;
using odswiezanie::rodzajOdswiezenia;
static const Odswiezenie P = Odswiezenie::PELNE, C = Odswiezenie::CZESCIOWE;

static std::string tekst(const std::vector<Odswiezenie>& v) {
  std::string s;
  for (Odswiezenie r : v) s += r == C ? 'C' : 'P';
  return s;
}

// ---------- czysta funkcja ----------
// wymaganie: R-NF-25
static void testR_NF_25_regula() {
  Wejscie w;
  w.ekran = Ekran::K1;
  w.poPrzycisku = true;
  w.obrazZnany = true;
  w.pelneCo = 5;
  SPRAWDZ(rodzajOdswiezenia(w) == C);
  // widoki z przycisku: kokpit, pytanie, potwierdzenia, U-01, Portret
  for (Ekran e : {Ekran::K1, Ekran::K2, Ekran::PYTANIE, Ekran::OMOWIONE, Ekran::SPRAWY, Ekran::PRZENIESIONE,
                  Ekran::ZAPISANO, Ekran::LICZBY, Ekran::SPRZET_ZAPISANO, Ekran::USTAWIENIA, Ekran::PORTRET,
                  Ekran::SYPIALNIA}) {
    w.ekran = e;
    SPRAWDZ(odswiezanie::ekranCzesciowy(e) && rodzajOdswiezenia(w) == C);
  }
  // ekran dnia (S9), test, kody QR, U-07, wycofane: zawsze pełne
  for (Ekran e : {Ekran::DZIEN, Ekran::TEST, Ekran::RAPORT, Ekran::FORMULARZ, Ekran::SIEC_PANELU, Ekran::AKTUALIZACJA,
                  Ekran::NORMA, Ekran::SPRZET}) {
    w.ekran = e;
    SPRAWDZ(!odswiezanie::ekranCzesciowy(e) && rodzajOdswiezenia(w) == P);
  }
  w.ekran = Ekran::K2;
  Wejscie x = w;
  x.poPrzycisku = false;  // z własnej inicjatywy panelu
  SPRAWDZ(rodzajOdswiezenia(x) == P);
  x = w;
  x.obrazZnany = false;  // pierwsze w przebiegu (po głębokim śnie, po starcie)
  SPRAWDZ(rodzajOdswiezenia(x) == P);
  x = w;
  x.zawszePelne = true;
  SPRAWDZ(rodzajOdswiezenia(x) == P);
  x = w;
  x.pelneCo = 0;  // częściowe wyłączone
  SPRAWDZ(rodzajOdswiezenia(x) == P);
  // co N-te: przy N = 5 częściowe przy liczniku 0…4, pełne przy 5
  for (int n = 0; n < 5; n++) {
    x = w;
    x.czesciowychZRzedu = n;
    SPRAWDZ(rodzajOdswiezenia(x) == C);
  }
  x.czesciowychZRzedu = 5;
  SPRAWDZ(rodzajOdswiezenia(x) == P);
  x.czesciowychZRzedu = 200;
  SPRAWDZ(rodzajOdswiezenia(x) == P);
  // licznik
  SPRAWDZ(odswiezanie::licznikPo(C, 0) == 1 && odswiezanie::licznikPo(C, 4) == 5 && odswiezanie::licznikPo(P, 4) == 0);
  SPRAWDZ(odswiezanie::licznikPo(C, 255) == 255 && odswiezanie::licznikPo(C, -3) == 1);
  SPRAWDZ(std::string(odswiezanie::nazwa(C)) == "czesciowe" && std::string(odswiezanie::nazwa(P)) == "pelne");
}

// wymaganie: R-NF-25
static void testR_NF_25_klucz() {
  int n = -1;
  SPRAWDZ(odswiezanie::pelneCoZTekstu("0", n) && n == 0);
  SPRAWDZ(odswiezanie::pelneCoZTekstu("5", n) && n == 5);
  SPRAWDZ(odswiezanie::pelneCoZTekstu("20", n) && n == 20);
  for (const char* zly : {"", "21", "-1", "abc", "5x", " 5", "100", "05 "}) SPRAWDZ(!odswiezanie::pelneCoZTekstu(zly, n));
  SPRAWDZ(parsujUstawieniaDnia("").pelneCo == -1);
  SPRAWDZ(parsujUstawieniaDnia("eink.pelne_co=3\n").pelneCo == 3);
  SPRAWDZ(parsujUstawieniaDnia("eink.pelne_co = 0\n").pelneCo == 0);
  UstawieniaDnia z = parsujUstawieniaDnia("eink.pelne_co=21\n");
  SPRAWDZ(z.pelneCo == -1 && z.bledy.size() == 1);
  z = parsujUstawieniaDnia("eink.pelne_co=abc\n");
  SPRAWDZ(z.pelneCo == -1 && z.bledy.size() == 1);
}

// ---------- dyrygent z atrapami ----------
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t ep(int h, int m, int s = 0) {
  struct tm t = {};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 10;
  t.tm_mday = 4;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  return (uint32_t)timegm(&t);
}
struct Nacisk {
  uint8_t bit;
  uint32_t trzymaj;  // ms
};
// Wybudzenie z głębokiego snu przyciskiem `pierwszy`, potem w czuwaniu kolejne naciśnięcia co 3 s.
static void wybudzPrzyciskiem(Panel& p, uint32_t kiedy, const std::vector<Nacisk>& n) {
  p.zegar.epoch = kiedy;
  p.zegar.msTeraz = 0;
  p.d.z.clear();
  p.sen.snow = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = n.front().bit;
  p.przyciski.skrypt.clear();
  uint32_t t = 0;
  for (const Nacisk& x : n) {
    p.przyciski.skrypt.push_back({t, x.bit});
    p.przyciski.skrypt.push_back({t + x.trzymaj, 0});
    t += x.trzymaj + 3000;
  }
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static void wybudzZegarem(Panel& p, uint32_t kiedy) {
  p.zegar.epoch = kiedy;
  p.zegar.msTeraz = 0;
  p.d.z.clear();
  p.sen.snow = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  p.start.budzace = 0;
  p.przyciski.skrypt.clear();
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static void przygotuj(Panel& p, const std::string& konf = "") {
  p.zegar.epoch = ep(10, 0);
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.pliki.pliki["/konfiguracja.txt"] = konf;
  p.start.stan.ekran = (uint8_t)Ekran::DZIEN;
}
static Ekran ekran(const Panel& p) { return (Ekran)p.start.stan.ekran; }
static bool logMa(const Panel& p, const std::string& s) {
  for (auto& w : p.port.log)
    if (w.find(s) != std::string::npos) return true;
  return false;
}
static const Nacisk PR{BIT_PRAWY, 300}, LE{BIT_LEWY, 300}, SR{BIT_SRODEK, 300};

// ▶ → K1 (przycisk budzący, po głębokim śnie: pełne), w czuwaniu ▶ → K2, ▶ → dalej, ↻ → ekran dnia:
// częściowe, częściowe, pełne (R-NF-25)
// wymaganie: R-NF-25
static void testR_NF_25_ciagWCzuwaniu() {
  Panel p;
  przygotuj(p);
  wybudzPrzyciskiem(p, ep(10, 0), {PR, PR, PR, SR});
  printf("  ▶ ▶ ▶ ↻ w jednym wybudzeniu: %s (ekran na końcu %d)\n", tekst(p.ekran.rodzaje).c_str(), (int)ekran(p));
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PCCP");
  SPRAWDZ(ekran(p) == Ekran::DZIEN);
  SPRAWDZ(p.start.stan.czesciowychZRzedu == 0);  // pełne na ekranie dnia zeruje
  SPRAWDZ(logMa(p, "odświeżenie częściowe 1 z 5"));
  SPRAWDZ(logMa(p, "(odświeżenie pełne)"));
}

// co N-te częściowe pełne i zerowanie licznika; N z `eink.pelne_co`
// wymaganie: R-NF-25
static void testR_NF_25_coNte() {
  Panel p;
  przygotuj(p, "eink.pelne_co=2\n");
  wybudzPrzyciskiem(p, ep(10, 0), {PR, PR, LE, PR, LE, PR});
  printf("  N = 2, ▶ ▶ ◀ ▶ ◀ ▶: %s\n", tekst(p.ekran.rodzaje).c_str());
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PCCPCC");
  SPRAWDZ(p.start.stan.czesciowychZRzedu == 2);
  Panel q;
  przygotuj(q, "eink.pelne_co=1\n");
  wybudzPrzyciskiem(q, ep(10, 0), {PR, PR, LE, PR, LE});
  SPRAWDZ(tekst(q.ekran.rodzaje) == "PCPCP");
}

// N = 0: same pełne, jak przed zmianą
// wymaganie: R-NF-25
static void testR_NF_25_wylaczone() {
  Panel p;
  przygotuj(p, "eink.pelne_co=0\n");
  wybudzPrzyciskiem(p, ep(10, 0), {PR, PR, LE, PR, SR});
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PPPPP");
  SPRAWDZ(p.start.stan.czesciowychZRzedu == 0);
}

// zła wartość klucza: wartość domyślna 5 i wpis w logu
// wymaganie: R-NF-25
static void testR_NF_25_zlaWartosc() {
  for (const char* zly : {"eink.pelne_co=abc\n", "eink.pelne_co=21\n", "eink.pelne_co=-1\n"}) {
    Panel p;
    przygotuj(p, zly);
    wybudzPrzyciskiem(p, ep(10, 0), {PR, PR, LE, PR, LE, PR, LE, PR});
    SPRAWDZ(tekst(p.ekran.rodzaje) == "PCCCCCPC");
    SPRAWDZ(logMa(p, "konfiguracja: pominięty wiersz 'eink.pelne_co"));
  }
}

// sterownik bez częściowego: wszystko pełne, licznik bez zmian, wpis w logu
// wymaganie: R-NF-25
static void testR_NF_25_sterownikBez() {
  Panel p;
  przygotuj(p);
  p.ekran.czesciowePozwala = false;
  wybudzPrzyciskiem(p, ep(10, 0), {PR, PR, LE, SR});
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PPPP");
  SPRAWDZ(p.start.stan.czesciowychZRzedu == 0);
  SPRAWDZ(logMa(p, "odświeżenie częściowe niedostępne"));
}

// kolejka naciśnięć z czasu rysowania: kilka ▶ = jedno odświeżenie, częściowe (obraz w tym przebiegu już był)
// wymaganie: R-NF-25
static void testR_NF_25_kolejka() {
  Panel p;
  przygotuj(p);
  p.przyciski.kolejka.assign(2, {(int)Przycisk::PRAWY, 150u});
  wybudzPrzyciskiem(p, ep(10, 0), {PR});
  printf("  ▶ budzący i 2 ▶ z kolejki: %s\n", tekst(p.ekran.rodzaje).c_str());
  SPRAWDZ(p.ekran.odswiezen == 2);
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PC");
}

// licznik w pamięci RTC przez głęboki sen; pierwsze rysowanie po wybudzeniu pełne, potem częściowe
// wymaganie: R-NF-25
static void testR_NF_25_glebokiSen() {
  Panel p;
  przygotuj(p);
  wybudzPrzyciskiem(p, ep(10, 0), {PR, PR});  // K1 pełne, K2 częściowe
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PC");
  SPRAWDZ(p.start.stan.czesciowychZRzedu == 1);  // w RTC po zaśnięciu
  p.ekran.rodzaje.clear();
  wybudzPrzyciskiem(p, ep(10, 1), {LE, PR});  // drugie wybudzenie przyciskiem pod rząd: ◀ K1 pełne, ▶ K2 częściowe
  printf("  drugie wybudzenie przyciskiem: %s, licznik %d\n", tekst(p.ekran.rodzaje).c_str(),
         (int)p.start.stan.czesciowychZRzedu);
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PC");
  SPRAWDZ(p.start.stan.czesciowychZRzedu == 1);
}

// restart (start zasilania): pierwsze rysowanie pełne bez względu na licznik w pamięci
// wymaganie: R-NF-25
static void testR_NF_25_restart() {
  Panel p;
  przygotuj(p);
  p.start.stan.czesciowychZRzedu = 3;
  p.start.stan.ekran = (uint8_t)Ekran::K1;
  p.zegar.epoch = ep(10, 0);
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  p.start.budzace = 0;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(!p.ekran.rodzaje.empty());
  for (Odswiezenie r : p.ekran.rodzaje) SPRAWDZ(r == P);
  SPRAWDZ(p.start.stan.czesciowychZRzedu == 0);
}

// odświeżenie z własnej inicjatywy panelu (zegar): zawsze pełne, także gdy na ekranie strona kokpitu
// wymaganie: R-NF-25
static void testR_NF_25_zegar() {
  Panel p;
  przygotuj(p);
  wybudzPrzyciskiem(p, ep(10, 0), {PR, PR});  // K2 stoi, licznik 1
  p.ekran.rodzaje.clear();
  wybudzZegarem(p, ep(10, 30));  // po 5 min powrót na ekran dnia (R-PRZ-02)
  printf("  wybudzenie zegarem po stronie kokpitu: %s (ekran %d)\n", tekst(p.ekran.rodzaje).c_str(), (int)ekran(p));
  SPRAWDZ(!p.ekran.rodzaje.empty());
  for (Odswiezenie r : p.ekran.rodzaje) SPRAWDZ(r == P);
  SPRAWDZ(p.start.stan.czesciowychZRzedu == 0);
}

// noc: S9 na ekranie dnia pełne; ▶ w nocy na K1 i ↻ z powrotem: bez dodatkowych odświeżeń, ekran dnia pełny
// wymaganie: R-NF-25
static void testR_NF_25_noc() {
  Panel p;
  przygotuj(p);
  wybudzPrzyciskiem(p, ep(1, 0), {PR, SR, PR});  // 02:00 czasu panelu (UTC+1): ▶ K1, ↻ S9, ▶ K1
  printf("  noc ▶ ↻ ▶: %s\n", tekst(p.ekran.rodzaje).c_str());
  SPRAWDZ(p.ekran.odswiezen == 3);  // tyle samo odświeżeń co przed zmianą (jedno na zmianę ekranu)
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PPC");
}

// potwierdzenie (◀ 2 s „wymienione dziś” na ekranie dnia → `Wymiana zapisana.`) przyciskiem budzącym: pełne (po
// głębokim śnie), licznik zerowany. Potwierdzenie i pytanie otwarte w czuwaniu dostają
// częściowe według ekranCzesciowy() (testR_NF_25_regula()); w atrapie bez historii ekran dnia to S5, z którego
// ◀ 2 s w czuwaniu nic nie zapisuje.
// wymaganie: R-NF-25
static void testR_NF_25_potwierdzenie() {
  Panel p;
  przygotuj(p, "wymiana.filtr=2026-10-01\n");
  p.nvs.liczbyU["cpap/b16_sk31"] = EL_FILTR + 1;  // ekran dnia S-K-35 „Wymiana: filtr.” (R-MAS-18)
  wybudzPrzyciskiem(p, ep(10, 0), {{BIT_LEWY, 2500}});
  printf("  ◀ 2 s budzący (potwierdzenie): %s (ekran %d)\n", tekst(p.ekran.rodzaje).c_str(), (int)ekran(p));
  SPRAWDZ(ekran(p) == Ekran::SPRZET_ZAPISANO);
  SPRAWDZ(tekst(p.ekran.rodzaje) == "P");
  SPRAWDZ(p.start.stan.czesciowychZRzedu == 0);
}

// klatka bez zmian (◀ 2 s na K1 nic nie robi): bez odświeżenia, licznik bez zmiany
// wymaganie: R-NF-25
static void testR_NF_25_klatkaBezZmian() {
  Panel p;
  przygotuj(p);
  wybudzPrzyciskiem(p, ep(10, 0), {PR, PR, LE, {BIT_LEWY, 2500}});
  printf("  ▶ ▶ ◀ ◀ 2 s: %s, licznik %d\n", tekst(p.ekran.rodzaje).c_str(), (int)p.start.stan.czesciowychZRzedu);
  SPRAWDZ(ekran(p) == Ekran::K1);
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PCC");
  SPRAWDZ(p.start.stan.czesciowychZRzedu == 2);
  SPRAWDZ(logMa(p, "bez zmian: klatka jak wyświetlona"));
}

// ---------- pytanie i potwierdzenia ze sprawą L w czuwaniu ----------
// Panel z 15 nocami AHI 12 / h: rano ekran dnia L-03 (jak testDyrygentL03() w test/testy_wzorce_l_hipotezy_h.cpp).
static const char* STATUS_L =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static std::string raportNocyL(long day) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":12.0,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static bool panelZeSprawaL(Panel& p) {
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();  // zestaw wbudowany i reguly::ocen()
  p.pliki.pliki["/konfiguracja.txt"] = "";
  const int NOCY = 15;
  const long ost = nrDnia("2026-11-04");
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = ost - NOCY + 1; x <= ost; x++)
    l += std::string(x > ost - NOCY + 1 ? "," : "") + "{\"day\":" + std::to_string(x) +
         ",\"session_count\":1,\"stale\":false}";
  l += "]}";
  bool jest = false;
  for (int h = 0; h < 12 && !jest; h++) {  // ostatnia noc i zaległe doby partiami
    p.port.log.clear();
    p.siec.odpowiedzi.clear();
    p.zegar.epoch = ep(6, 0) + (uint32_t)(h * 1800);
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.siec.dodaj("/api/status", 200, STATUS_L);
    p.siec.dodaj(sciezkaListy(), 200, l);
    for (long x = ost - NOCY + 1; x <= ost; x++) p.siec.dodaj(sciezkaNocy(x), 200, raportNocyL(x));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    jest = logMa(p, "ekran dnia: wzorzec L-03");
  }
  p.siec.odpowiedzi.clear();
  p.ekran.rodzaje.clear();
  return jest;
}
static std::vector<Ekran> ekranyOdswiezen(const Panel& p) {  // r.ekran przy każdym wpisie „narysowany”
  std::vector<Ekran> e;
  for (auto& w : p.port.log) {
    int n = -1;
    if (w.find(" narysowany w ") != std::string::npos && sscanf(w.c_str(), "ekran %d", &n) == 1) e.push_back((Ekran)n);
  }
  return e;
}

// pytanie „Co było inaczej?” z K1 (↻ 2 s) i ruch w siatce w czuwaniu: częściowe
// wymaganie: R-NF-25
static void testR_NF_25_pytanieWCzuwaniu() {
  Panel p;
  SPRAWDZ(panelZeSprawaL(p));
  p.port.log.clear();
  wybudzPrzyciskiem(p, ep(10, 0), {PR, {BIT_SRODEK, 2500}, PR, SR});
  std::vector<Ekran> e = ekranyOdswiezen(p);
  printf("  ▶ K1, ↻ 2 s pytanie, ▶ kursor, ↻ zaznacz: %s\n", tekst(p.ekran.rodzaje).c_str());
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PCCC");
  SPRAWDZ(e.size() == 4 && e[0] == Ekran::K1 && e[1] == Ekran::PYTANIE && e[3] == Ekran::PYTANIE);
}

// potwierdzenia ze sprawą L w czuwaniu: ◀ 2 s na L → „Omówione z lekarzem?”, ↻ 2 s → „Przeniesione” („wiem”):
// częściowe; ekran dnia L między nimi pełny
// wymaganie: R-NF-25
static void testR_NF_25_potwierdzeniaWCzuwaniu() {
  Panel p;
  SPRAWDZ(panelZeSprawaL(p));
  p.port.log.clear();
  wybudzPrzyciskiem(p, ep(10, 0), {PR, LE, {BIT_LEWY, 2500}, SR, {BIT_SRODEK, 2500}});
  std::vector<Ekran> e = ekranyOdswiezen(p);
  printf("  ▶ K1, ◀ L-03, ◀ 2 s omówione, ↻ L-03, ↻ 2 s przeniesione: %s\n", tekst(p.ekran.rodzaje).c_str());
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PPCPC");
  SPRAWDZ(e.size() == 5 && e[1] == Ekran::DZIEN && e[2] == Ekran::OMOWIONE && e[3] == Ekran::DZIEN &&
          e[4] == Ekran::PRZENIESIONE);
}

// Potwierdzenie bez sprawy w tym przebiegu (pokazB5() == false) rysuje w pokaz() ekran dnia; odświeżenie pełne
// z wpisem w logu. Uwaga: w atrapie ta droga zachodzi tylko przy pierwszym rysowaniu przebiegu (z ekranu dnia
// nawigacja prowadzi do potwierdzenia tylko ze sprawą, która wtedy jest), więc „pełne” wynika też z pierwszego rysowania po wybudzeniu;
// skutek samego znacznika sprawdza test brzegowy testBrzeg_potwierdzenieRysujeDzien() na regule, a tu wpis
// w logu, że pokaz() go przekazał.
// wymaganie: R-NF-25
static void testR_NF_25_potwierdzenieBezSprawyRysujeDzien() {
  // tylko „Omówione”: ▶ 2 s na nim nic nie robi (5.6.2), a na „Przeniesione” i „Zapisano” każdy inny przycisk
  // prowadzi na ekran dnia, więc tam potwierdzenie nie rysuje się drugi raz
  for (Ekran e : {Ekran::OMOWIONE}) {
    Panel p;
    przygotuj(p);
    p.start.stan.ekran = (uint8_t)e;
    p.start.stan.ekranOd = ep(10, 0) - 5;
    p.port.log.clear();
    wybudzPrzyciskiem(p, ep(10, 0), {{BIT_PRAWY, 2500}});  // ▶ 2 s na potwierdzeniu: bez akcji, ekran zostaje
    char wpis[96];
    snprintf(wpis, sizeof wpis, "na płótnie ekran dnia zamiast ekranu %d, odświeżenie pełne", (int)e);
    printf("  potwierdzenie %d bez sprawy: %s, wpis „na płótnie” %s\n", (int)e, tekst(p.ekran.rodzaje).c_str(),
           logMa(p, wpis) ? "jest" : "brak");
    SPRAWDZ(logMa(p, wpis));
    SPRAWDZ(!p.ekran.rodzaje.empty() && p.ekran.rodzaje[0] == P);
  }
  // ekran dnia na płótnie przy ekranie z listy częściowych → reguła daje pełne
  Wejscie w;
  w.ekran = Ekran::OMOWIONE;
  w.poPrzycisku = true;
  w.obrazZnany = true;
  w.naPlotnieDzien = true;
  SPRAWDZ(rodzajOdswiezenia(w) == P);
  w.naPlotnieDzien = false;
  SPRAWDZ(rodzajOdswiezenia(w) == C);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testR_NF_25_regula();
  testR_NF_25_klucz();
  testR_NF_25_ciagWCzuwaniu();
  testR_NF_25_coNte();
  testR_NF_25_wylaczone();
  testR_NF_25_zlaWartosc();
  testR_NF_25_sterownikBez();
  testR_NF_25_kolejka();
  testR_NF_25_glebokiSen();
  testR_NF_25_restart();
  testR_NF_25_zegar();
  testR_NF_25_noc();
  testR_NF_25_potwierdzenie();
  testR_NF_25_klatkaBezZmian();
  testR_NF_25_pytanieWCzuwaniu();
  testR_NF_25_potwierdzeniaWCzuwaniu();
  testR_NF_25_potwierdzenieBezSprawyRysujeDzien();
  printf("testy_odswiezanie_czesciowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
