// SPDX-License-Identifier: GPL-3.0-or-later
// Testy planu wybudzeń.
// Pisane ze spec (2.2, 7.1–7.5, 9.5, T-WYM-20, T-WYM-28, T-WYM-31, T-NF-02,
// T-NF-06, T-TEST-279; spec funkcjonalna R-NF-02, R-NF-03, R-DANE-18 i wymagania, do których odsyłają), bez czytania
// implementacji dyrygenta. Testują plan wybudzenia (src/plan.cpp) i dyrygenta wobec zachowania prototypu
// (budzenie co 30 min, Wi-Fi do snu, czuwanie po każdym wybudzeniu).
//
// (a) Plan wybudzenia (src/plan.h, T-KOMP-24): terminy doby, powody z R-NF-02, Wi-Fi tylko z powodem sieciowym.
// (b) Dyrygent z atrapami (test/atrapy/adaptery_atrapy.h) i adapterami z pomiarem czasu Wi-Fi, rysowania i snu:
//     kolejność kroków 7.1 (T-WYM-20), wybudzenie pomiarowe (T-WYM-31), symulacja doby zwykłej i dób bez danych
//     (T-NF-02, T-TEST-279) w budżecie 8.1.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_plan_wybudzen, na końcu).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "magazyn.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "dzien.h"
#include "nawigacja.h"
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
#define ROWNE(a, b)                                                                                     \
  do {                                                                                                  \
    std::string _a = (a), _b = (b);                                                                     \
    if (_a == _b) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)
#define ROWNE_L(a, b)                                                                                    \
  do {                                                                                                   \
    long long _a = (long long)(a), _b = (long long)(b);                                                  \
    if (_a == _b) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); }  \
  } while (0)

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";  // jak test/testy_wybudzenie.cpp

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
static int sd(int h, int m, int s = 0) { return h * 3600 + m * 60 + s; }

// Stan planu z zegarem: chwila hh:mm:ss (domyślnie 5 s po pełnej minucie, jak po śnie do terminu + ZAPAS_POLGODZINY_S).
static WejsciePlanu wp(int h, int m, int s = 5) {
  WejsciePlanu w;
  w.zegar = true;
  w.sekundaDoby = sd(h, m, s);
  return w;
}

// Najbliższy termin z Wi-Fi: przy E-01 bez nocy slot dzienny od 11:00 dokłada oceny bez Wi-Fi o pełnych
// godzinach (POWOD_SLOT_DZIENNY_BEZ_SIECI, R-KON-41), więc rytm zapytań R-DANE-18 to odstęp do terminu sieciowego.
// Zwraca czas snu od teraz do niego (suma snów po drodze) i jego powody.
static Termin nastepnySieciowy(WejsciePlanu w) {
  uint32_t razem = 0;
  for (int i = 0; i < 100; i++) {
    Termin t = nastepnyTermin(w);
    if (sieciowy(t.powody) || t.sekund == 0 || w.sekundaDoby + (long)t.sekund >= 86400) {
      t.sekund += razem;
      return t;
    }
    razem += t.sekund;
    w.sekundaDoby += (int)t.sekund;
  }
  return Termin();
}

const Powody SIECIOWE = POWOD_OKNO_PORANNE | POWOD_SPRAWDZENIE_11 | POWOD_PONOWIENIE | POWOD_POGODA |
                          POWOD_POCZATEK_NOCY | POWOD_CZAS;

// ============================================================================================================
// (a) Plan wybudzenia
// ============================================================================================================

// T-WYM-28, kolumna „Weryfikacja”: K1 otwarty o 10:12:00 → sen do 10:17:02, nie do 10:30:05; o 02:12 w slocie nocnym
// → sen do 02:15 (zapis czujników, R-DANE-09), bez terminu powrotu i bez wybudzenia z Wi-Fi do 05:00; o 14:05 przy
// nocy kompletnej zapisanej o 07:00 → sen do najbliższego powodu z R-NF-02 (pogoda przed slotem wieczornym), nie do 14:30.
static void testT_WYM_28_przykladySpec() {
  WejsciePlanu a = wp(10, 12, 0);
  a.nocKompletna = true;
  a.ekran = Ekran::K1;
  a.sekundOdNacisniecia = 0;
  Termin ta = nastepnyTermin(a);
  ROWNE_L(ta.sekund, 302);
  ROWNE_L(ta.powody, POWOD_POWROT);

  WejsciePlanu b = wp(2, 12, 0);
  b.ekran = Ekran::K1;
  b.sekundOdNacisniecia = 0;
  Termin tb = nastepnyTermin(b);
  ROWNE_L(tb.sekund, 3 * 60 + 5);  // 02:15:05
  ROWNE_L(tb.powody, POWOD_CZUJNIKI);
  SPRAWDZ(!sieciowy(tb.powody));

  WejsciePlanu c = wp(14, 5, 0);
  c.nocKompletna = true;
  c.sprawdzenie11 = true;
  c.zrodloOtoczenia = true;
  Termin tc = nastepnyTermin(c);
  ROWNE_L(tc.sekund, sd(21, 0) - sd(14, 5) + 5);  // 21:00:05 (pogoda przed slotem wieczornym, R-DANE-16)
  ROWNE_L(tc.powody, POWOD_POGODA | POWOD_SLOT_WIECZORNY);  // 21:00 to też pierwsza ocena slotu (R-KON-01)
  SPRAWDZ(tc.sekund != 25 * 60 + 5);              // nie 14:30:05 jak prototyp
  // bez źródła otoczenia (prototyp nie ma serwisu pogody): (R-KON-01, egzemplarz bez CO2) następny powód to
  // ocena slotu wieczornego bez Wi-Fi o 21:00 (przed zapis czujników o 23:00)
  c.zrodloOtoczenia = false;
  Termin td = nastepnyTermin(c);
  ROWNE_L(td.sekund, sd(21, 0) - sd(14, 5) + 5);
  ROWNE_L(td.powody, POWOD_SLOT_WIECZORNY_BEZ_SIECI);
  SPRAWDZ(!sieciowy(td.powody));
}

// R-DANE-02, R-DANE-19, R-DANE-03: okno poranne 05:00–11:00, co 30 min do nocy kompletnej; 05:00 zawsze.
static void testR_DANE_19_oknoPoranne() {
  ROWNE_L(nastepnyTermin(wp(4, 50)).sekund, 600);
  ROWNE_L(nastepnyTermin(wp(4, 50)).powody, POWOD_OKNO_PORANNE | POWOD_CZUJNIKI);
  ROWNE_L(nastepnyTermin(wp(5, 0)).sekund, 900);  // 05:15 czujniki (okno czujników do 06:00)
  ROWNE_L(nastepnyTermin(wp(5, 0)).powody, POWOD_CZUJNIKI);
  ROWNE_L(nastepnyTermin(wp(5, 15)).powody, POWOD_OKNO_PORANNE | POWOD_CZUJNIKI);
  ROWNE_L(nastepnyTermin(wp(5, 45)).sekund, 900);
  ROWNE_L(nastepnyTermin(wp(5, 45)).powody, POWOD_OKNO_PORANNE);  // 06:00 już poza oknem czujników
  ROWNE_L(nastepnyTermin(wp(6, 0)).sekund, 1800);
  ROWNE_L(nastepnyTermin(wp(6, 0)).powody, POWOD_OKNO_PORANNE);
  ROWNE_L(nastepnyTermin(wp(7, 0)).powody, POWOD_OKNO_PORANNE);
  ROWNE_L(nastepnyTermin(wp(10, 30)).sekund, 1800);
  ROWNE_L(nastepnyTermin(wp(10, 30)).powody, POWOD_SPRAWDZENIE_11);  // 11:00 to już nie okno poranne
  // noc kompletna o 07:00: następny termin o 11:00 (R-DANE-24), bez 07:30…10:30
  WejsciePlanu k = wp(7, 0);
  k.nocKompletna = true;
  ROWNE_L(nastepnyTermin(k).sekund, 4 * 3600);
  ROWNE_L(nastepnyTermin(k).powody, POWOD_SPRAWDZENIE_11);
  // noc kompletna już o 05:00: czujniki do 05:45, potem 11:00
  WejsciePlanu r = wp(5, 0);
  r.nocKompletna = true;
  ROWNE_L(nastepnyTermin(r).powody, POWOD_CZUJNIKI);
  r.sekundaDoby = sd(5, 45, 5);
  ROWNE_L(nastepnyTermin(r).sekund, sd(11, 0) - sd(5, 45));
  ROWNE_L(nastepnyTermin(r).powody, POWOD_SPRAWDZENIE_11);
}

// R-DANE-24, R-DANE-15, R-DANE-20: jedno sprawdzenie o 11:00; potem bez ponowień przy nocy kompletnej.
static void testR_DANE_24_sprawdzenie11() {
  WejsciePlanu w = wp(10, 59, 0);
  w.nocKompletna = true;
  ROWNE_L(nastepnyTermin(w).sekund, 65);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_SPRAWDZENIE_11);
  w = wp(11, 0);
  w.nocKompletna = true;
  w.sprawdzenie11 = true;
  // najbliższy termin to ocena slotu wieczornego bez Wi-Fi o 21:00 (przed czujniki o 23:00)
  ROWNE_L(nastepnyTermin(w).sekund, sd(21, 0) - sd(11, 0));
  ROWNE_L(nastepnyTermin(w).powody, POWOD_SLOT_WIECZORNY_BEZ_SIECI);
  // R-DANE-24 fail-path: AirBridge nie odpowiada o 11:00 przy nocy zapisanej → bez kolejnych prób tego dnia
  w.e01 = true;
  w.nocyBezDanych = 1;
  ROWNE_L(nastepnyTermin(w).powody, POWOD_SLOT_WIECZORNY_BEZ_SIECI);  // bez Wi-Fi, nie próba
  // noc bez terapii albo częściowa po 11:00 (AirBridge odpowiada): ponowienia co 30 min (R-DANE-18, R-DANE-20)
  WejsciePlanu b = wp(11, 0);
  b.sprawdzenie11 = true;
  ROWNE_L(nastepnyTermin(b).sekund, 1800);
  ROWNE_L(nastepnyTermin(b).powody, POWOD_PONOWIENIE);
}

// R-DANE-18: co 30 min do 23:30; od drugiej nocy bez danych z rzędu, po braku odpowiedzi albo
// sieci, co 3 h od 11:00 (14:00, 17:00, 20:00, 23:00); odrzucone logowanie → co 30 min.
static void testR_DANE_18_ponowienia() {
  WejsciePlanu w = wp(11, 0);
  w.sprawdzenie11 = true;
  w.e01 = true;
  w.nocyBezDanych = 1;
  ROWNE_L(nastepnyTermin(w).sekund, 1800);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_PONOWIENIE);
  w.sekundaDoby = sd(22, 30, 5);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_PONOWIENIE | POWOD_CZUJNIKI | POWOD_SLOT_WIECZORNY_BEZ_SIECI);
  w.sekundaDoby = sd(23, 15, 5);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_PONOWIENIE | POWOD_POCZATEK_NOCY | POWOD_CZUJNIKI);  // ostatnie o 23:30
  w.sekundaDoby = sd(23, 30, 5);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_CZUJNIKI);  // 23:45: brak zapytań do 05:00

  // druga noc bez danych: co 3 h (między terminami sieciowymi oceny slotu dziennego bez Wi-Fi o pełnych
  // godzinach, R-KON-41 przy E-01; zapytania do AirBridge dalej 14:00, 17:00, 20:00, 23:00)
  w.nocyBezDanych = 2;
  w.sekundaDoby = sd(11, 0, 5);
  ROWNE_L(nastepnyTermin(w).sekund, 3600);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_SLOT_DZIENNY_BEZ_SIECI);  // 12:00 bez Wi-Fi
  ROWNE_L(nastepnySieciowy(w).sekund, 3 * 3600);
  ROWNE_L(nastepnySieciowy(w).powody, POWOD_PONOWIENIE | POWOD_SLOT_DZIENNY_BEZ_SIECI);
  w.sekundaDoby = sd(11, 30, 5);
  ROWNE_L(nastepnySieciowy(w).sekund, sd(14, 0) - sd(11, 30));
  w.sekundaDoby = sd(14, 0, 5);
  ROWNE_L(nastepnySieciowy(w).sekund, 3 * 3600);
  w.sekundaDoby = sd(17, 0, 5);
  ROWNE_L(nastepnySieciowy(w).sekund, 3 * 3600);
  w.sekundaDoby = sd(20, 0, 5);
  // po 20:00 slot dzienny się kończy (21:00); od 21:00 oceny slotu wieczornego bez Wi-Fi (bez źródła otoczenia)
  ROWNE_L(nastepnyTermin(w).sekund, 3600);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_SLOT_WIECZORNY_BEZ_SIECI);
  ROWNE_L(nastepnySieciowy(w).sekund, 3 * 3600);
  ROWNE_L(nastepnySieciowy(w).powody, POWOD_PONOWIENIE | POWOD_CZUJNIKI | POWOD_SLOT_WIECZORNY_BEZ_SIECI);  // 23:00
  w.sekundaDoby = sd(23, 15, 5);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_POCZATEK_NOCY | POWOD_CZUJNIKI);  // 23:30 bez ponowienia
  w.nocyBezDanych = 3;
  w.sekundaDoby = sd(11, 0, 5);
  ROWNE_L(nastepnySieciowy(w).sekund, 3 * 3600);
  // okno poranne drugiej doby bez zmian (R-DANE-19)
  w.sekundaDoby = sd(5, 15, 5);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_OKNO_PORANNE | POWOD_CZUJNIKI);
  w.sekundaDoby = sd(6, 0, 5);
  ROWNE_L(nastepnyTermin(w).sekund, 1800);

  // odrzucone logowanie (R-AW-34) w drugiej nocy: co 30 min
  w.nocyBezDanych = 2;
  w.logowanieOdrzucone = true;
  w.sekundaDoby = sd(11, 0, 5);
  ROWNE_L(nastepnyTermin(w).sekund, 1800);
  // AirBridge odpowiada (bez E-01), noc bez terapii albo częściowa: co 30 min bez względu na liczbę nocy
  w.logowanieOdrzucone = false;
  w.e01 = false;
  ROWNE_L(nastepnyTermin(w).sekund, 1800);
}

// R-DANE-18 „brak zapytań 23:30–05:00”, R-PRZ-18: od 23:30 do 05:00 tylko wybudzenia pomiarowe co 15 min.
static void testR_DANE_18_brakZapytanWNocy() {
  WejsciePlanu w = wp(23, 30);
  w.e01 = true;
  w.nocyBezDanych = 1;
  w.ekran = Ekran::K1;          // ekran otwarty przyciskiem: w nocy bez powrotu
  w.sekundOdNacisniecia = 4000;
  long teraz = sd(23, 30, 5);
  int pomiarowych = 0, zlych = 0;
  for (int i = 0; i < 60 && teraz < 86400 + sd(5, 0); i++) {
    w.sekundaDoby = (int)(teraz % 86400);
    w.sekundOdNacisniecia = 4000 + (teraz - sd(23, 30, 5));
    Termin t = nastepnyTermin(w);
    if (t.sekund == 0) break;
    teraz += t.sekund;
    if (teraz >= 86400 + sd(5, 0)) {
      ROWNE_L(teraz, 86400 + sd(5, 0, 5));
      ROWNE_L(t.powody, POWOD_OKNO_PORANNE | POWOD_CZUJNIKI);
      break;
    }
    if (t.powody == POWOD_CZUJNIKI && t.sekund == 900) pomiarowych++;
    else zlych++;
  }
  ROWNE_L(pomiarowych, 21);  // 23:45, 00:00, …, 04:45
  ROWNE_L(zlych, 0);
}

// R-KON-31, R-KON-33: początek slotu nocnego 23:30 z /api/status (Wi-Fi), także przy nocy kompletnej.
static void testR_KON_31_poczatekNocy() {
  WejsciePlanu w = wp(23, 20);
  w.nocKompletna = true;
  w.sprawdzenie11 = true;
  ROWNE_L(nastepnyTermin(w).sekund, 600);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_POCZATEK_NOCY | POWOD_CZUJNIKI);
  SPRAWDZ(sieciowy(POWOD_POCZATEK_NOCY | POWOD_CZUJNIKI));
}

// T-WYM-31, R-DANE-09 (okno nocy sypialni prototypu 23:00–06:00): co 15 min, bez Wi-Fi.
static void testT_WYM_31_terminyCzujnikow() {
  WejsciePlanu w = wp(22, 50);
  w.nocKompletna = true;
  w.sprawdzenie11 = true;
  ROWNE_L(nastepnyTermin(w).sekund, 600);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_CZUJNIKI | POWOD_SLOT_WIECZORNY_BEZ_SIECI);  // 23:00 też ocena bez Wi-Fi
  // cała noc od 22:50 do 06:00: 28 terminów z czujnikami (23:00…05:45), żaden inny bez sieci
  long teraz = sd(22, 50, 5);
  int czujnikow = 0;
  for (int i = 0; i < 60 && teraz < 86400 + sd(6, 0); i++) {
    w.sekundaDoby = (int)(teraz % 86400);
    if (teraz >= 86400) { w.nocKompletna = false; w.sprawdzenie11 = false; }  // nowa doba
    Termin t = nastepnyTermin(w);
    if (t.sekund == 0) break;
    teraz += t.sekund;
    if (teraz < 86400 + sd(6, 0) && (t.powody & POWOD_CZUJNIKI)) czujnikow++;
  }
  ROWNE_L(czujnikow, 28);
  WejsciePlanu teraz215 = wp(2, 15);
  ROWNE_L(powodyTeraz(teraz215), POWOD_CZUJNIKI);
  SPRAWDZ(!pobranieZegarem(powodyTeraz(teraz215)).pobierz);
  ROWNE_L(powodyTeraz(wp(6, 0)), POWOD_OKNO_PORANNE);  // 06:00: bez czujników
}

// R-PRZ-02: powrót po 5 min bez naciśnięcia, sen do powrotu + 2 s, gdy wypada przed innym terminem.
static void testR_PRZ_02_powrot() {
  WejsciePlanu w = wp(10, 12, 0);
  w.ekran = Ekran::K1;
  w.sekundOdNacisniecia = 299;
  ROWNE_L(nastepnyTermin(w).sekund, 3);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_POWROT);
  w.ekran = Ekran::DZIEN;  // ekran dnia nie wraca
  ROWNE_L(nastepnyTermin(w).powody, POWOD_OKNO_PORANNE);
  // 10:28, K1 otwarty 2 min temu: okno poranne o 10:30 (125 s) przed powrotem (182 s)
  w = wp(10, 28, 0);
  w.ekran = Ekran::K1;
  w.sekundOdNacisniecia = 120;
  ROWNE_L(nastepnyTermin(w).sekund, 125);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_OKNO_PORANNE);
  // 10:29, K1 otwarty 290 s temu: powrót za 10 s + 2 s przed oknem o 10:30
  w = wp(10, 29, 0);
  w.ekran = Ekran::K1;
  w.sekundOdNacisniecia = 290;
  ROWNE_L(nastepnyTermin(w).sekund, 12);
  // chwila powrotu: powód bieżącego wybudzenia, bez Wi-Fi
  WejsciePlanu p = wp(10, 17, 2);
  p.nocKompletna = true;
  p.ekran = Ekran::K1;
  p.sekundOdNacisniecia = 302;
  SPRAWDZ((powodyTeraz(p) & POWOD_POWROT) != 0);
  SPRAWDZ(!sieciowy(powodyTeraz(p)));
}

// R-AW-20: bez czasu ważnego ponowienie co 30 min (Wi-Fi: NTP i czas z AirBridge).
static void testR_AW_20_bezZegara() {
  WejsciePlanu w;
  w.zegar = false;
  w.sekundaDoby = 0;
  ROWNE_L(nastepnyTermin(w).sekund, OKRES_SNU_S);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_CZAS);
  ROWNE_L(powodyTeraz(w), POWOD_CZAS);
  SPRAWDZ(sieciowy(POWOD_CZAS));
}

// R-PIE-13, R-CZY-03, R-NF-02 (weryfikacja): import o 13:00, pytanie bez odpowiedzi → wybudzenie o 15:00 bez Wi-Fi.
static void testR_PIE_13_terminPytania() {
  WejsciePlanu w = wp(13, 0);
  w.nocKompletna = true;
  w.sprawdzenie11 = true;
  w.terminPytania = 15 * 60;
  ROWNE_L(nastepnyTermin(w).sekund, 7200);
  ROWNE_L(nastepnyTermin(w).powody, POWOD_PYTANIE);
  w.sekundaDoby = sd(15, 0, 5);
  ROWNE_L(powodyTeraz(w), POWOD_PYTANIE);
  SPRAWDZ(!sieciowy(POWOD_PYTANIE));
  ROWNE_L(nastepnyTermin(w).powody, POWOD_SLOT_WIECZORNY_BEZ_SIECI);  // termin minął: dalej 21:00 (przed: 23:00)
}

// R-NF-02: powody bieżącego wybudzenia zegarem (dziennik wybudzeń), z tolerancją 60 s po terminie.
static void testR_NF_02_powodyTeraz() {
  ROWNE_L(powodyTeraz(wp(5, 0)), POWOD_OKNO_PORANNE | POWOD_CZUJNIKI);
  ROWNE_L(powodyTeraz(wp(5, 0, 0)), POWOD_OKNO_PORANNE | POWOD_CZUJNIKI);
  ROWNE_L(powodyTeraz(wp(5, 1, 0)), POWOD_OKNO_PORANNE | POWOD_CZUJNIKI);  // 60 s po terminie
  ROWNE_L(powodyTeraz(wp(5, 1, 10)), 0);                                   // 70 s po: bez powodu
  ROWNE_L(powodyTeraz(wp(7, 30)), POWOD_OKNO_PORANNE);
  WejsciePlanu k = wp(7, 30);
  k.nocKompletna = true;
  ROWNE_L(powodyTeraz(k), 0);  // noc kompletna: 07:30 nie jest terminem
  ROWNE_L(powodyTeraz(wp(11, 0)), POWOD_SPRAWDZENIE_11);
  WejsciePlanu n = wp(23, 30);
  n.nocKompletna = true;
  n.sprawdzenie11 = true;
  ROWNE_L(powodyTeraz(n), POWOD_POCZATEK_NOCY | POWOD_CZUJNIKI);
  n.nocKompletna = false;
  ROWNE_L(powodyTeraz(n), POWOD_PONOWIENIE | POWOD_POCZATEK_NOCY | POWOD_CZUJNIKI);
  WejsciePlanu p = wp(21, 0);
  p.nocKompletna = true;
  p.sprawdzenie11 = true;
  ROWNE_L(powodyTeraz(p), POWOD_SLOT_WIECZORNY_BEZ_SIECI);  // (R-KON-01): bez źródła ocena bez Wi-Fi (przed: 0)
  p.zrodloOtoczenia = true;
  ROWNE_L(powodyTeraz(p), POWOD_POGODA | POWOD_SLOT_WIECZORNY);  // R-KON-01
}

// Log doby (13.1, wiersz 2): nazwa każdego powodu w logu i w polu `powod` pliku /bateria.csv.
static void testR_NF_02_nazwyPowodow() {
  ROWNE(nazwyPowodow(POWOD_OKNO_PORANNE), "okno_poranne");
  ROWNE(nazwyPowodow(POWOD_SPRAWDZENIE_11), "sprawdzenie_11");
  ROWNE(nazwyPowodow(POWOD_PONOWIENIE), "ponowienie");
  ROWNE(nazwyPowodow(POWOD_POGODA), "pogoda");
  ROWNE(nazwyPowodow(POWOD_POCZATEK_NOCY), "poczatek_nocy");
  ROWNE(nazwyPowodow(POWOD_CZUJNIKI), "czujniki");
  ROWNE(nazwyPowodow(POWOD_POWROT), "powrot");
  ROWNE(nazwyPowodow(POWOD_PYTANIE), "pytanie");
  ROWNE(nazwyPowodow(POWOD_CZAS), "czas");
  ROWNE(nazwyPowodow(POWOD_OKNO_PORANNE | POWOD_CZUJNIKI), "okno_poranne+czujniki");
  ROWNE(nazwyPowodow(POWOD_PONOWIENIE | POWOD_POCZATEK_NOCY | POWOD_CZUJNIKI), "ponowienie+poczatek_nocy+czujniki");
  ROWNE(nazwyPowodow(0), "");
  ROWNE(powodWpisuBaterii(PowodWybudzenia::ZEGAR, POWOD_CZUJNIKI), "zegar:czujniki");
  ROWNE(powodWpisuBaterii(PowodWybudzenia::ZEGAR, 0), "zegar:");
  ROWNE(powodWpisuBaterii(PowodWybudzenia::PRZYCISK, POWOD_CZUJNIKI), "przycisk");
  ROWNE(powodWpisuBaterii(PowodWybudzenia::START, 0), "start");
}

// R-NF-03, T-WYM-24, T-WYM-31: Wi-Fi tylko przy powodach sieciowych.
static void testR_NF_03_sieciowy() {
  SPRAWDZ(sieciowy(POWOD_OKNO_PORANNE));
  SPRAWDZ(sieciowy(POWOD_SPRAWDZENIE_11));
  SPRAWDZ(sieciowy(POWOD_PONOWIENIE));
  SPRAWDZ(sieciowy(POWOD_POGODA));
  SPRAWDZ(sieciowy(POWOD_POCZATEK_NOCY));
  SPRAWDZ(sieciowy(POWOD_CZAS));
  SPRAWDZ(!sieciowy(POWOD_CZUJNIKI));
  SPRAWDZ(!sieciowy(POWOD_POWROT));
  SPRAWDZ(!sieciowy(POWOD_PYTANIE));
  SPRAWDZ(!sieciowy(POWOD_CZUJNIKI | POWOD_POWROT | POWOD_PYTANIE));
  SPRAWDZ(!sieciowy(0));
  SPRAWDZ(sieciowy(POWOD_CZUJNIKI | POWOD_OKNO_PORANNE));
  Pobranie a = pobranieZegarem(POWOD_OKNO_PORANNE | POWOD_CZUJNIKI);
  SPRAWDZ(a.pobierz && !a.przerywalne && a.poranekCzynnikow);
  Pobranie b = pobranieZegarem(POWOD_CZUJNIKI);
  SPRAWDZ(!b.pobierz && !b.przerywalne && !b.poranekCzynnikow);
  SPRAWDZ(!pobranieZegarem(0).pobierz);
  SPRAWDZ(!pobranieZegarem(POWOD_POWROT).pobierz);
}

// 2.2 wniosek 4, 7.1 krok 13, T-WYM-20: czuwanie na przycisk tylko po wybudzeniu przyciskiem.
static void testT_WYM_20_czuwanieTylkoPoPrzycisku() {
  ROWNE_L(czuwanieMs(PowodWybudzenia::ZEGAR, false), 0);
  ROWNE_L(czuwanieMs(PowodWybudzenia::START, false), 0);
  ROWNE_L(czuwanieMs(PowodWybudzenia::PRZYCISK, false), CZUWANIE_MS);
  ROWNE_L(czuwanieMs(PowodWybudzenia::PRZYCISK, true), OKNO_SERWISOWE_MS);
}

// T-WYM-28, T-WYM-29: sen z terminu, budzą przyciski niewciśnięte.
static void testT_WYM_29_planSnuZTerminu() {
  Termin t;
  t.sekund = 185;
  t.powody = POWOD_CZUJNIKI;
  PlanSnu a = planSnuZTerminu(t, 0);
  SPRAWDZ(a.budzZegarem);
  ROWNE_L(a.sekund, 185);
  ROWNE_L(a.przyciski, WSZYSTKIE_PRZYCISKI);
  ROWNE_L(planSnuZTerminu(t, BIT_SRODEK).przyciski, BIT_LEWY | BIT_PRAWY);
  ROWNE_L(planSnuZTerminu(t, WSZYSTKIE_PRZYCISKI).przyciski, 0);
  SPRAWDZ(planSnuZTerminu(t, WSZYSTKIE_PRZYCISKI).budzZegarem);
}

// R-DANE-18 z liczeniem nocy jak R-AW-32 (od ostatniego udanego odczytu AirBridge).
static void testR_DANE_18_nocyBezDanych() {
  ROWNE_L(nocyBezDanych(false, "2026-10-23", "2026-10-21", "2026-10-21"), 0);
  // AirBridge milczy od 21.10 06:00 (ostatni udany odczyt 21.10 rano)
  ROWNE_L(nocyBezDanych(true, "2026-10-22", "2026-10-21", "2026-10-21"), 1);
  ROWNE_L(nocyBezDanych(true, "2026-10-23", "2026-10-21", "2026-10-21"), 2);
  ROWNE_L(nocyBezDanych(true, "2026-10-24", "2026-10-21", "2026-10-21"), 3);
  ROWNE_L(nocyBezDanych(true, "2026-10-22", "2026-10-22", "2026-10-22"), 0);  // udany odczyt dziś
  // bez udanego odczytu: od dnia pierwszej nieudanej próby
  ROWNE_L(nocyBezDanych(true, "2026-10-22", "", "2026-10-22"), 1);
  ROWNE_L(nocyBezDanych(true, "2026-10-23", "", "2026-10-22"), 2);
  ROWNE_L(nocyBezDanych(true, "zła data", "2026-10-21", "2026-10-21"), 1);
}

// Przejście planem przez dobę od 00:00:05 do 00:00:05 dnia następnego. `import` = minuta doby, od której pierwszy
// termin z oknem porannym daje noc kompletną (-1 = noc nie przychodzi). Zwraca minuty terminów i ich powody.
struct TerminDoby {
  int minuta;
  Powody powody;
};
static std::vector<TerminDoby> planDoby(WejsciePlanu w, int import) {
  std::vector<TerminDoby> v;
  long teraz = 5;
  for (int i = 0; i < 200 && teraz < 86400; i++) {
    w.sekundaDoby = (int)teraz;
    Termin t = nastepnyTermin(w);
    if (t.sekund == 0) break;
    teraz += t.sekund;
    int minuta = (int)((teraz % 86400) / 60);
    v.push_back({teraz >= 86400 ? minuta + 24 * 60 : minuta, t.powody});
    if (import >= 0 && (t.powody & POWOD_OKNO_PORANNE) && minuta >= import) w.nocKompletna = true;
    if (t.powody & POWOD_SPRAWDZENIE_11) w.sprawdzenie11 = true;
  }
  return v;
}
static std::vector<int> sieciowe(const std::vector<TerminDoby>& v) {
  std::vector<int> m;
  for (auto& t : v)
    if (sieciowy(t.powody)) m.push_back(t.minuta);
  return m;
}
static std::vector<int> co30(int od, int doMin) {
  std::vector<int> m;
  for (int x = od; x <= doMin; x += 30) m.push_back(x);
  return m;
}

// T-WYM-28, R-NF-02, spec techniczna 2.2 („zwykła doba według planu”): noc zapisana o 07:00, ze źródłem otoczenia.
static void testT_WYM_28_planDobyZwyklej() {
  WejsciePlanu w = wp(0, 0);
  w.zrodloOtoczenia = true;
  std::vector<TerminDoby> v = planDoby(w, 7 * 60);
  // slot wieczorny co 30 min w oknie 21:00–23:30 przy źródle otoczenia (R-KON-01, R-KON-17): 21:30, 22:00,
  // 22:30, 23:00 z Wi-Fi tylko na `/api/status` i źródło otoczenia
  std::vector<int> oczekiwane = {5 * 60,  5 * 60 + 30,  6 * 60,  6 * 60 + 30, 7 * 60,      11 * 60,
                                 21 * 60, 21 * 60 + 30, 22 * 60, 22 * 60 + 30, 23 * 60, 23 * 60 + 30};
  SPRAWDZ(sieciowe(v) == oczekiwane);
  // 23 czujniki 00:15…05:45, 4 czujniki 23:00…23:45, 00:00 następnej doby, 06:00, 06:30, 07:00, 11:00, 21:00,
  // 21:30, 22:00, 22:30 (23:00 razem z czujnikami)
  ROWNE_L(v.size(), 36);
  SPRAWDZ(v.size() <= 80);  // budżet 8.1
  int bezPowodu = 0;
  for (auto& t : v) bezPowodu += t.powody == 0;
  ROWNE_L(bezPowodu, 0);
  // bez źródła otoczenia: bez 21:00
  w.zrodloOtoczenia = false;
  ROWNE_L(sieciowe(planDoby(w, 7 * 60)).size(), 7);
}

// T-NF-02, R-DANE-18, spec techniczna 2.2 („doba bez danych”): 38 wybudzeń z siecią w pierwszej dobie, 18 w drugiej.
static void testT_NF_02_planDobyBezDanych() {
  WejsciePlanu w = wp(0, 0);
  w.e01 = true;
  w.nocyBezDanych = 1;
  std::vector<TerminDoby> v = planDoby(w, -1);
  std::vector<int> pierwsza = co30(5 * 60, 23 * 60 + 30);  // 05:00…23:30 co 30 min
  SPRAWDZ(sieciowe(v) == pierwsza);
  ROWNE_L(sieciowe(v).size(), 38);
  ROWNE_L(v.size(), 62);
  SPRAWDZ(v.size() <= 110);
  SPRAWDZ(38 * 68 <= 45 * 60);  // Wi-Fi ok. 68 s na wybudzenie (2.2) w limicie 45 min

  w.nocyBezDanych = 2;
  v = planDoby(w, -1);
  std::vector<int> druga = co30(5 * 60, 11 * 60);
  for (int m : {14 * 60, 17 * 60, 20 * 60, 23 * 60, 23 * 60 + 30}) druga.push_back(m);
  SPRAWDZ(sieciowe(v) == druga);
  ROWNE_L(sieciowe(v).size(), 18);
  // 42 terminy kroku 2 i 6 ocen slotu dziennego bez Wi-Fi (12:00, 13:00, 15:00, 16:00, 18:00, 19:00; R-KON-41
  // przy E-01); o 11:00, 14:00, 17:00 i 20:00 ocena w tym samym wybudzeniu co zapytanie
  // (R-KON-01, bez źródła otoczenia): 4 oceny slotu wieczornego bez Wi-Fi (21:00, 21:30, 22:00, 22:30; 23:00 razem
  // z ponowieniem i czujnikami)
  ROWNE_L(v.size(), 52);
}

// ============================================================================================================
// (b) Dyrygent z atrapami i pomiarem
// ============================================================================================================

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_BLOKADA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const char* WYZWANIE = "Digest realm=\"AirBridge\", qop=\"auth\", nonce=\"abc\", opaque=\"xyz\"";

// Raport doby jak w test/testy_dyrygenta.cpp raport() (Załącznik A).
static std::string raport(long day) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Press\",\"label\":\"C\",\"unit\":\"cmH2O\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":6.2,\"mean\":10.1,\"median\":9.8,\"p95\":13.6,\"max\":16.0,"
         "\"source\":\"edf\"},{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}

// Pomiar jednego wybudzenia: czas z włączonym radiem, rysowanie i odczyt przycisków przy radiu, chwila snu.
struct Pomiar {
  bool radio = false;
  uint32_t radioOdMs = 0, wifiMs = 0, koniecSieciMs = 0, przerwaPoSieciMs = 0;
  int polaczen = 0, zadan = 0, rysowanZRadiem = 0, odswiezen = 0, odczytowZRadiemPoRysowaniu = 0;
  uint32_t ostatnieRysowanieMs = 0, wylaczenieMs = 0, msPrzySnie = 0;
  bool radioPrzySnie = false;
  void zeruj() { *this = Pomiar(); }
  void zamknijRadio(uint32_t teraz) {
    if (!radio) return;
    wifiMs += teraz - radioOdMs;
    if (teraz - koniecSieciMs > przerwaPoSieciMs) przerwaPoSieciMs = teraz - koniecSieciMs;
    radio = false;
  }
};

// Slot dzienny, R-KON-41: jawny wariant symulacji bez slotu dziennego. Gdy ustawione, każde wybudzenie
// dostaje konfigurację z KonfiguracjaPanelu::bezSlotuDziennego = true (flaga tylko dla symulatora; src/main.cpp jej nie
// ustawia), więc plan liczy dobę bez slotu dziennego, a przebieg idzie przez ten sam kod dyrygenta i planu (przypadek
// zastępczy z R-EKR-60 „przed wersją ze slotem dziennym w symulatorze”). Nie zeruje
// StanRtc::importNocyEp, bo i tak dawałoby slot (zastępnik z NVS).
static bool wariantBezSlotu = false;
struct BezSlotuDziennego {
  BezSlotuDziennego() { wariantBezSlotu = true; }
  ~BezSlotuDziennego() { wariantBezSlotu = false; }
};

class SiecPomiar : public AtrapaSieci {
 public:
  SiecPomiar(Dziennik& d, AtrapaZegara& z, Pomiar& m) : AtrapaSieci(d, z), z_(z), m_(m) {}
  bool polacz(uint32_t limitMs, const std::function<bool()>& przerwij, bool& przerwane) override {
    if (!m_.radio) { m_.radio = true; m_.radioOdMs = z_.msTeraz; }
    m_.polaczen++;
    bool w = AtrapaSieci::polacz(limitMs, przerwij, przerwane);
    m_.koniecSieciMs = z_.msTeraz;
    return w;
  }
  bool czasZSieci(uint32_t limitMs) override {
    bool w = AtrapaSieci::czasZSieci(limitMs);
    m_.koniecSieciMs = z_.msTeraz;
    return w;
  }
  OdpowiedzHttp get(const ZadanieHttp& z) override {
    OdpowiedzHttp o = AtrapaSieci::get(z);
    m_.zadan++;
    m_.koniecSieciMs = z_.msTeraz;
    return o;
  }
  void wylacz() override {
    m_.zamknijRadio(z_.msTeraz);
    m_.wylaczenieMs = z_.msTeraz;
    AtrapaSieci::wylacz();
  }
 private:
  AtrapaZegara& z_;
  Pomiar& m_;
};

class EkranPomiar : public AtrapaWyswietlacza {
 public:
  EkranPomiar(Dziennik& d, AtrapaZegara& z, Pomiar& m) : AtrapaWyswietlacza(d), z_(z), m_(m) {}
  void odswiez() override {
    m_.odswiezen++;
    if (m_.radio) m_.rysowanZRadiem++;
    m_.ostatnieRysowanieMs = z_.msTeraz;
    AtrapaWyswietlacza::odswiez();
  }
 private:
  AtrapaZegara& z_;
  Pomiar& m_;
};

class PrzyciskiPomiar : public AtrapaPrzyciskow {
 public:
  PrzyciskiPomiar(AtrapaZegara& z, Pomiar& m) : AtrapaPrzyciskow(z), m_(m) {}
  uint8_t wcisniete() override {
    if (m_.odswiezen > 0 && m_.radio) m_.odczytowZRadiemPoRysowaniu++;
    return AtrapaPrzyciskow::wcisniete();
  }
 private:
  Pomiar& m_;
};

class SenPomiar : public AtrapaSnu {
 public:
  SenPomiar(Dziennik& d, AtrapaUruchomienia& u, AtrapaStraznika& s, AtrapaZegara& z, Pomiar& m)
      : AtrapaSnu(d, u, s), z_(z), m_(m) {}
  void spij(const PlanSnu& p) override {
    m_.msPrzySnie = z_.msTeraz;
    m_.radioPrzySnie = m_.radio;
    m_.zamknijRadio(z_.msTeraz);
    AtrapaSnu::spij(p);
  }
 private:
  AtrapaZegara& z_;
  Pomiar& m_;
};

// Panel z atrapami z pomiarem; sieć, ekran, przyciski i sen to obiekty z pomiarem (nie p.siec, p.ekran, …).
// Plik nocy wersji 1 bez rekordów (nagłówek, nazwy pól z rejestru magazynu, `#koniec 0`).
static std::string plikNocyBezRekordow() {
  const FormatPliku* f = formatPliku(rejestrFormatow(), "noce");
  std::string p;
  for (auto& x : f->pola[0]) p += (p.empty() ? "" : ";") + x;
  return "#cpap-eink noce 1\n" + p + "\n#koniec 0\n";
}

struct PanelSym {
  Panel p;
  Pomiar m;
  SiecPomiar siec{p.d, p.zegar, m};
  EkranPomiar ekran{p.d, p.zegar, m};
  PrzyciskiPomiar przyciski{p.zegar, m};
  SenPomiar sen{p.d, p.start, p.straznik, p.zegar, m};
  // implementacja plik nocy bez rekordów, żeby start zasilania symulacji
  // nie otwierał okna przywracania T-WYM-87 (partycja bez plików nocy); historia dalej pusta jak przed 
  PanelSym() { p.pliki.pliki["/dane/noce/2025.csv"] = plikNocyBezRekordow(); }
  Adaptery adaptery() {
    return Adaptery{p.zegar, p.nvs, p.pliki, siec, ekran, przyciski, p.czujniki, p.dzwiek, p.start, p.straznik, sen,
                    p.port, p.diag};
  }
};

struct Wyb {
  uint32_t ep = 0;
  PowodWybudzenia powod = PowodWybudzenia::ZEGAR;
  int minuta = 0;      // minuta doby czasu lokalnego
  std::string data;    // data lokalna RRRR-MM-DD
  Pomiar m;
  bool spal = false;
  std::vector<std::string> zdarzenia;  // dziennik zdarzeń tego wybudzenia
};
static int minutaLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  return l.tm_hour * 60 + l.tm_min;
}
static std::string dataLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  char b[16];
  strftime(b, sizeof b, "%Y-%m-%d", &l);
  return b;
}

// Jedno wybudzenie o chwili `t`: stan RTC, NVS i pliki zostają z poprzedniego (jak po głębokim śnie).
static Wyb wybudz(PanelSym& s, uint32_t t, PowodWybudzenia pw, const KonfiguracjaPanelu& k = KONF) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.siec.polaczonaTeraz = false;
  s.siec.okno = false;
  s.p.d.z.clear();
  s.m.zeruj();
  int snow = s.sen.snow;
  KonfiguracjaPanelu kk = k;
  kk.bezSlotuDziennego = kk.bezSlotuDziennego || wariantBezSlotu;
  Dyrygent(s.adaptery(), kk).wybudzenie();
  Wyb w;
  w.ep = t;
  w.powod = pw;
  w.minuta = minutaLok(t);
  w.data = dataLok(t);
  w.m = s.m;
  w.spal = s.sen.snow == snow + 1;
  w.zdarzenia = s.p.d.z;
  return w;
}

using Odpowiedzi = void (*)(PanelSym&, uint32_t);

// Kolejne wybudzenia od startu zasilania o `od` do `doT`, każde po śnie z planu dyrygenta.
static std::vector<Wyb> symuluj(PanelSym& s, uint32_t od, uint32_t doT, Odpowiedzi odp,
                                const KonfiguracjaPanelu& k = KONF) {
  std::vector<Wyb> v;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 600 && t < doT; i++) {
    odp(s, t);
    Wyb w = wybudz(s, t, pw, k);
    v.push_back(w);
    if (!w.spal) break;
    uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
    if (snu == 0) snu = 1;
    t += w.m.msPrzySnie / 1000 + snu;
    pw = PowodWybudzenia::ZEGAR;
  }
  return v;
}

// (magazyn danych): treść pliku bez nagłówka `#cpap-eink {nazwa} {wersja}` i wiersza nazw pól.
static std::string bezNaglowka(const std::string& t) {
  if (t.compare(0, 11, "#cpap-eink ") != 0) return t;
  size_t a = t.find('\n');
  a = a == std::string::npos ? t.size() : t.find('\n', a + 1);
  return a == std::string::npos ? std::string() : t.substr(a + 1);
}
static int ileWierszy(const std::string& t, const std::string& poczatek) {
  int n = 0;
  size_t a = 0;
  while (a < t.size()) {
    size_t b = t.find('\n', a);
    if (b == std::string::npos) b = t.size();
    if (t.compare(a, poczatek.size(), poczatek) == 0) n++;
    a = b + 1;
  }
  return n;
}

// Pole `powod` każdego wiersza /bateria.csv: „start”, „przycisk” albo „zegar:” z nazwami powodów z R-NF-02.
static int wierszeBateriiBezPowodu(const std::string& plik) {
  // „odswiezenie” (R-EKR-60) i powody slotu dziennego (R-KON-41, R-KON-42, R-KON-52)
  static const std::set<std::string> NAZWY = {"okno_poranne", "sprawdzenie_11", "ponowienie", "pogoda",
                                              "poczatek_nocy", "czujniki", "powrot", "pytanie", "czas",
                                              "odswiezenie", "slot_dzienny", "slot_dzienny_godzina",
                                              "prognoza_godzinowa", "slot_dzienny_bez_sieci",
                                              "slot_wieczorny_bez_sieci"};  // (R-KON-01)
  int zlych = 0;
  size_t a = 0;
  while (a < plik.size()) {
    size_t b = plik.find('\n', a);
    if (b == std::string::npos) b = plik.size();
    std::string w = plik.substr(a, b - a);
    a = b + 1;
    if (w.empty()) continue;
    std::vector<std::string> pola;
    size_t x = 0;
    while (true) {
      size_t y = w.find(';', x);
      pola.push_back(w.substr(x, y == std::string::npos ? std::string::npos : y - x));
      if (y == std::string::npos) break;
      x = y + 1;
    }
    if (pola.size() < 5) { zlych++; continue; }
    const std::string& p = pola[4];
    if (p == "start" || p == "przycisk") continue;
    if (p.compare(0, 6, "zegar:") != 0 || p.size() == 6) { zlych++; continue; }
    std::string reszta = p.substr(6);
    size_t q = 0;
    while (q <= reszta.size()) {
      size_t r = reszta.find('+', q);
      std::string n = reszta.substr(q, r == std::string::npos ? std::string::npos : r - q);
      if (!NAZWY.count(n)) { zlych++; break; }
      if (r == std::string::npos) break;
      q = r + 1;
    }
  }
  return zlych;
}

static const long DOBA_A = nrDnia("2026-11-03");  // noc z poranka 4.11.2026

// T-WYM-20, kolumna „Weryfikacja”: wybudzenie zegarem o 07:00 → kolejność 7.1, Wi-Fi wyłączone przed pierwszym
// zapisem i rysowaniem, brak czuwania przed snem; sen do 11:00 (R-DANE-24); powód w /bateria.csv.
// sen do 11:00 w wariancie bez slotu dziennego (jawna flaga testowa, plan bez slotu); bez niej sen do startu slotu
// dziennego o 08:00 (import 07:00 + 60 min, R-KON-41), sprawdzone na końcu.
static void testT_WYM_20_kolejnoscKrokow() {
  {
  BezSlotuDziennego bezSlotu;
  PanelSym s;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
  s.siec.dodaj(sciezkaNocy(DOBA_A), 200, raport(DOBA_A));
  uint32_t t = ep(2026, 11, 4, 7, 0, 5);
  Wyb w = wybudz(s, t, PowodWybudzenia::ZEGAR);
  const Dziennik& d = s.p.d;
  SPRAWDZ(w.spal);
  ROWNE_L(w.m.polaczen, 1);
  int wyl = d.pierwsze("siec.wylacz");
  SPRAWDZ(wyl >= 0);
  SPRAWDZ(d.pierwsze("pliki.nazwa /dane/noce/2026.csv.tmp") > wyl);
  SPRAWDZ(d.pierwsze("ekran.odswiez") > wyl);
  int getPo = 0;
  for (size_t i = (size_t)(wyl < 0 ? 0 : wyl); i < d.z.size(); i++) getPo += d.z[i].compare(0, 8, "siec.get") == 0;
  ROWNE_L(getPo, 0);
  ROWNE_L(w.m.rysowanZRadiem, 0);
  SPRAWDZ(!w.m.radioPrzySnie);
  SPRAWDZ(w.m.msPrzySnie - w.m.ostatnieRysowanieMs < 1000);  // bez czuwania 20 s po zegarze
  // sen do 11:00:05 (noc kompletna o 07:00, R-DANE-24), nie do 07:30:05
  long koniec = (long)t + (long)(w.m.msPrzySnie / 1000) + (long)s.sen.plan.sekund;
  SPRAWDZ(labs(koniec - (long)ep(2026, 11, 4, 11, 0, 5)) <= 1);
  SPRAWDZ(bezNaglowka(s.p.pliki.pliki["/log/wybudzenia.csv"]).find(";zegar:okno_poranne") != std::string::npos);
  }
  // (R-KON-41): plan zna chwilę importu (07:00:05) → sen do startu slotu dziennego o 08:00:05
  PanelSym s;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
  s.siec.dodaj(sciezkaNocy(DOBA_A), 200, raport(DOBA_A));
  uint32_t t = ep(2026, 11, 4, 7, 0, 5);
  Wyb w = wybudz(s, t, PowodWybudzenia::ZEGAR);
  SPRAWDZ(w.spal);
  long koniec = (long)t + (long)(w.m.msPrzySnie / 1000) + (long)s.sen.plan.sekund;
  SPRAWDZ(labs(koniec - (long)ep(2026, 11, 4, 8, 0, 5)) <= 1);
  bool slot = false;
  for (auto& z : w.zdarzenia) slot = slot || z.find("następny termin: slot_dzienny") != std::string::npos;
  SPRAWDZ(slot);
}

// T-WYM-31: wybudzenie pomiarowe o 02:15 (slot nocny) → odczyt do bufora nocy, bez Wi-Fi, bez rysowania, od razu sen.
static void testT_WYM_31_wybudzeniePomiarowe() {
  PanelSym s;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  s.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  uint32_t t = ep(2026, 11, 4, 2, 15, 5);
  int przed = s.p.start.stan.buforSyp.n;
  Wyb w = wybudz(s, t, PowodWybudzenia::ZEGAR);
  SPRAWDZ(w.spal);
  ROWNE_L(w.m.polaczen, 0);
  ROWNE_L(w.m.zadan, 0);
  ROWNE_L(w.m.odswiezen, 0);
  SPRAWDZ(w.m.msPrzySnie < 2000);
  ROWNE_L(s.p.start.stan.buforSyp.n, przed + 1);
  ROWNE(s.p.start.stan.buforSyp.data, "2026-11-04");
  long koniec = (long)t + (long)(w.m.msPrzySnie / 1000) + (long)s.sen.plan.sekund;
  SPRAWDZ(labs(koniec - (long)ep(2026, 11, 4, 2, 30, 5)) <= 1);  // następny zapis czujników
  SPRAWDZ(bezNaglowka(s.p.pliki.pliki["/log/wybudzenia.csv"]).find(";zegar:czujniki") != std::string::npos);
}

// T-WYM-20: czuwanie 20 s bez Wi-Fi tylko po wybudzeniu przyciskiem; po zegarze sen zaraz po rysowaniu.
static void testT_WYM_20_czuwanieDyrygent() {
  {
    PanelSym s;
    zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
    s.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
    Wyb w = wybudz(s, ep(2026, 11, 4, 10, 0, 5), PowodWybudzenia::ZEGAR);  // okno poranne, noc w budowie
    SPRAWDZ(w.spal && w.m.polaczen == 1 && w.m.odswiezen == 1);
    SPRAWDZ(w.m.msPrzySnie - w.m.ostatnieRysowanieMs < 1000);
  }
  {
    PanelSym s;
    zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
    s.p.start.budzace = BIT_PRAWY;
    s.p.start.stan.ekran = (uint8_t)Ekran::DZIEN;
    s.przyciski.skrypt = {{0, BIT_PRAWY}, {300, 0}};
    Wyb w = wybudz(s, ep(2026, 11, 4, 10, 5, 0), PowodWybudzenia::PRZYCISK);
    SPRAWDZ(w.spal && w.m.polaczen == 0 && w.m.odswiezen == 1);
    SPRAWDZ(w.m.msPrzySnie - w.m.ostatnieRysowanieMs >= CZUWANIE_MS);
    ROWNE_L(w.m.wifiMs, 0);
    SPRAWDZ(bezNaglowka(s.p.pliki.pliki["/log/wybudzenia.csv"]).find(";przycisk") != std::string::npos);
  }
}

// T-NF-06, R-NF-03: Wi-Fi wyłączone zaraz po ostatnim żądaniu, przed rysowaniem i czuwaniem.
static void testT_NF_06_wifiPrzedRysowaniem() {
  PanelSym s;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  s.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  Wyb w = wybudz(s, ep(2026, 11, 4, 9, 30, 5), PowodWybudzenia::ZEGAR);
  SPRAWDZ(w.spal && w.m.polaczen == 1 && w.m.zadan >= 1);
  ROWNE_L(w.m.rysowanZRadiem, 0);
  ROWNE_L(w.m.odczytowZRadiemPoRysowaniu, 0);
  SPRAWDZ(w.m.przerwaPoSieciMs < 1000);  // w logu radia brak okresów Wi-Fi bez żądania
  SPRAWDZ(w.m.wifiMs <= 8000);           // T-NF-05: wybudzenie bez nowych danych ≤ 8 s Wi-Fi
  SPRAWDZ(!w.m.radioPrzySnie);
}

// Zwykła doba (spec techniczna 2.2, 8.1): start zasilania 4.11 00:05, terapia do 06:40, noc w raporcie od 06:40.
static void odpowiedziZwyklej(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  if (t >= ep(2026, 11, 4, 6, 40) && t < ep(2026, 11, 5, 0, 0)) {
    s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
    s.siec.dodaj(sciezkaNocy(DOBA_A), 200, raport(DOBA_A));
  } else {
    s.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  }
}

// R-NF-02, R-NF-03, T-WYM-28, T-NF-06 na dobie z atrapami: każde wybudzenie z powodem, Wi-Fi ≤ 10 min, bez zapytań
// 23:30–05:00, bez czuwania po zegarze, Wi-Fi wyłączone przed rysowaniem; log doby w /bateria.csv.
// Wariant bez slotu dziennego (BezSlotuDziennego, kryterium „Wi-Fi < 10 min na dobę bez slotu
// dziennego”); doba ze slotem dziennym: test/testy_slot_dzienny_w_planie.cpp testR_NF_02_symulacjaDobyZeSlotemDziennym. Przy
// zmiennej środowiska LOG_DOBY=1 drukuje log obu dób.
static void testR_NF_02_symulacjaZwyklejDoby() {
  BezSlotuDziennego bezSlotu;
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 5), odpowiedziZwyklej);
  SPRAWDZ(!v.empty() && v.size() <= 80);
  uint32_t wifi = 0;
  int nocneZSiecia = 0, czuwanZegarem = 0, rysowanZRadiem = 0, radioWeSnie = 0, przerw = 0;
  std::vector<int> sieciowe;
  for (size_t i = 0; i < v.size(); i++) {
    const Wyb& w = v[i];
    wifi += w.m.wifiMs;
    rysowanZRadiem += w.m.rysowanZRadiem;
    radioWeSnie += w.m.radioPrzySnie;
    przerw += w.m.przerwaPoSieciMs >= 1000;
    if (w.powod != PowodWybudzenia::ZEGAR) continue;
    if (w.m.polaczen) sieciowe.push_back(w.minuta);
    if (w.m.polaczen && slotNocny(w.minuta) && w.minuta != 23 * 60 + 30) nocneZSiecia++;
    uint32_t koniecPracy = w.m.ostatnieRysowanieMs > w.m.koniecSieciMs ? w.m.ostatnieRysowanieMs : w.m.koniecSieciMs;
    if (w.m.msPrzySnie - koniecPracy >= 1000) czuwanZegarem++;
  }
  SPRAWDZ(wifi <= 10 * 60 * 1000);
  ROWNE_L(nocneZSiecia, 0);
  ROWNE_L(czuwanZegarem, 0);
  ROWNE_L(rysowanZRadiem, 0);
  ROWNE_L(radioWeSnie, 0);
  ROWNE_L(przerw, 0);
  std::vector<int> oczekiwane = {5 * 60, 5 * 60 + 30, 6 * 60, 6 * 60 + 30, 7 * 60, 11 * 60, 23 * 60 + 30};
  SPRAWDZ(sieciowe == oczekiwane);
  ROWNE_L(ileWierszy(s.p.pliki.pliki["/dane/noce/2026.csv"], "2026-11-04"), 1);
  ROWNE_L(ileWierszy(bezNaglowka(s.p.pliki.pliki["/log/wybudzenia.csv"]), ""), v.size());
  ROWNE_L(wierszeBateriiBezPowodu(bezNaglowka(s.p.pliki.pliki["/log/wybudzenia.csv"])), 0);
  // wybudzenia pomiarowe od 00:05 do 05:45: odczyt przy starcie i 23 terminy co 15 min w buforze nocy
  SPRAWDZ(ileWierszy(s.p.pliki.pliki["/dane/sypialnia/2026.csv"], "2026-11-04") == 1);
}

// Doby bez danych (T-TEST-279): AirBridge odpowiada 21.10 do 06:00, potem milczy; wraca 24.10 o 14:10.
static const long DOBA_21 = nrDnia("2026-10-20");  // noc z poranka 21.10
static const long DOBA_24 = nrDnia("2026-10-23");  // noc z poranka 24.10
static void odpowiedziBezDanych(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  if (t < ep(2026, 10, 21, 6, 0)) {
    s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_21, DOBA_21));
    s.siec.dodaj(sciezkaNocy(DOBA_21), 200, raport(DOBA_21));
  } else if (t >= ep(2026, 10, 24, 14, 10)) {
    s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_21, DOBA_24));
    for (long d = DOBA_21; d <= DOBA_24; d++) s.siec.dodaj(sciezkaNocy(d), 200, raport(d));
  }
  // między 21.10 06:00 a 24.10 14:10 brak odpowiedzi (atrapa: limit żądania, kod błędu połączenia)
}
static void odpowiedziLogowanieOdrzucone(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  if (t < ep(2026, 10, 21, 6, 0)) {
    s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_21, DOBA_21));
    s.siec.dodaj(sciezkaNocy(DOBA_21), 200, raport(DOBA_21));
  } else {
    s.siec.dodaj("/api/status", 401, "");
    s.siec.odpowiedzi["/api/status"].back().o.wwwAuthenticate = WYZWANIE;
  }
}

static std::vector<int> sieciowePo11(const std::vector<Wyb>& v, const std::string& data) {
  std::vector<int> m;
  for (auto& w : v)
    if (w.data == data && w.powod == PowodWybudzenia::ZEGAR && w.m.polaczen && w.minuta > 11 * 60) m.push_back(w.minuta);
  return m;
}
static int siecioweRano(const std::vector<Wyb>& v, const std::string& data) {
  int n = 0;
  for (auto& w : v)
    n += w.data == data && w.m.polaczen && w.minuta >= 5 * 60 && w.minuta <= 11 * 60;
  return n;
}
static uint32_t wifiDoby(const std::vector<Wyb>& v, const std::string& data) {
  uint32_t s = 0;
  for (auto& w : v)
    if (w.data == data) s += w.m.wifiMs;
  return s;
}
static int wybudzenDoby(const std::vector<Wyb>& v, const std::string& data) {
  int n = 0;
  for (auto& w : v) n += w.data == data;
  return n;
}
static int nocnychZSiecia(const std::vector<Wyb>& v) {
  int n = 0;
  for (auto& w : v)
    n += w.powod == PowodWybudzenia::ZEGAR && w.m.polaczen && slotNocny(w.minuta) && w.minuta != 23 * 60 + 30;
  return n;
}

// T-TEST-279, R-DANE-18, T-NF-02: 22.10 po 11:00 co 30 min do 23:30; 23.10 i 24.10 okno poranne co 30 min, po 11:00
// tylko 14:00, 17:00, 20:00, 23:00 (i początek slotu nocnego 23:30); AirBridge wraca 24.10 o 14:10 → noc pobrana
// o 17:00, E-01 znika; budżet doby bez danych z 8.1 (≤ 110 wybudzeń, Wi-Fi ≤ 45 min), druga doba ok. 21 min.
static void testR_DANE_18_symulacjaBezDanych() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 10, 21, 5, 10), ep(2026, 10, 24, 18, 0), odpowiedziBezDanych);
  SPRAWDZ(v.size() > 100);
  ROWNE_L(ileWierszy(s.p.pliki.pliki["/dane/noce/2026.csv"], "2026-10-21"), 1);
  SPRAWDZ(sieciowePo11(v, "2026-10-22") == co30(11 * 60 + 30, 23 * 60 + 30));
  ROWNE_L(siecioweRano(v, "2026-10-23"), 13);  // 05:00…11:00 co 30 min
  std::vector<int> rzadko = {14 * 60, 17 * 60, 20 * 60, 23 * 60, 23 * 60 + 30};
  SPRAWDZ(sieciowePo11(v, "2026-10-23") == rzadko);
  ROWNE_L(nocnychZSiecia(v), 0);
  for (const char* d : {"2026-10-22", "2026-10-23"}) {
    SPRAWDZ(wybudzenDoby(v, d) <= 110);
    SPRAWDZ(wifiDoby(v, d) <= 45u * 60 * 1000);
  }
  SPRAWDZ(wifiDoby(v, "2026-10-23") <= 25u * 60 * 1000);  // ok. 21 min (2.2)
  // powrót AirBridge 24.10 o 14:10: pierwsze wybudzenie z siecią po tej chwili o 17:00, noc pobrana, E-01 znika
  int pierwszePowrocie = -1;
  for (auto& w : v)
    if (w.ep >= ep(2026, 10, 24, 14, 10) && w.m.polaczen) { pierwszePowrocie = w.minuta; break; }
  ROWNE_L(pierwszePowrocie, 17 * 60);
  ROWNE_L(ileWierszy(s.p.pliki.pliki["/dane/noce/2026.csv"], "2026-10-24"), 1);
  ROWNE_L(s.p.start.stan.ab.bladOd, 0);
  // cała noc 21/22.10 w oknie czujników: 28 odczytów (23:00…05:45) w buforze nocy przed zamknięciem o 06:00
  int maks = 0;
  {
    PanelSym q;
    std::vector<Wyb> u = symuluj(q, ep(2026, 10, 21, 22, 50), ep(2026, 10, 22, 5, 50), odpowiedziBezDanych);
    maks = q.p.start.stan.buforSyp.n;
    SPRAWDZ(!u.empty());
  }
  ROWNE_L(maks, 28);
}

// R-DANE-18, R-AW-34: odrzucone logowanie także w drugiej nocy → ponawianie co 30 min (nie co 3 h).
static void testR_DANE_18_symulacjaLogowanieOdrzucone() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 10, 21, 5, 10), ep(2026, 10, 23, 23, 50), odpowiedziLogowanieOdrzucone);
  SPRAWDZ(sieciowePo11(v, "2026-10-23") == co30(11 * 60 + 30, 23 * 60 + 30));
  ROWNE_L(nocnychZSiecia(v), 0);
  ROWNE_L(wierszeBateriiBezPowodu(bezNaglowka(s.p.pliki.pliki["/log/wybudzenia.csv"])), 0);
}

// ============================================================================================================
// (c) Przerysowanie ekranu dnia co 2 h i powody wybudzeń (R-EKR-60, R-NF-02)
// ============================================================================================================

static const std::string CO3H = "Sprawdzam co\xC2\xA0" "3\xC2\xA0" "h.";
static const std::string CO30 = "Ponawiam co\xC2\xA0" "30\xC2\xA0" "min.";

// R-AW-24, E-01 (6.4),: od drugiej nocy bez odpowiedzi AirBridge powód S5 kończy się `Sprawdzam co 3 h.`
// (rzadsze ponawianie z R-DANE-18); pierwsza noc bez zmian (`Pytam co 30 min.`, R-AW-32); przy odrzuconym logowaniu
// panel dalej ponawia co 30 min (R-AW-34, R-DANE-18), więc zdanie zostaje `Ponawiam co 30 min.` (R-AW-35).
static void testR_AW_24_sprawdzamCo3h() {
  std::vector<Noc> h;
  for (int i = 1; i <= 20; i++) {
    Noc n;
    n.data = dodajDni("2026-10-21", -i + 1);  // noce do 21.10 włącznie
    h.push_back(n);
  }
  DaneDnia d;
  d.hist = &h;
  d.abData = "2026-10-21";
  d.abMinuta = 6 * 60;
  d.ostatnieDane = "21.10 06:00";
  d.httpKod = -1;
  d.dzis = "2026-10-22";  // 1. noc bez danych
  TrescEkranu t1 = trescDnia("S5", "1", d);
  ROWNE(t1.powod, "AirBridge milczy od 21.10 06:00. Pytam co\xC2\xA0" "30\xC2\xA0" "min.");
  d.dzis = "2026-10-23";  // 2. noc
  TrescEkranu t2 = trescDnia("S5", "1", d);
  ROWNE(t2.tytul, "Brak danych od 2 nocy");
  ROWNE(t2.powod, "Wyjazd z aparatem? Dane wrócą same. " + CO3H);  // 53 znaki (R-AW-24)
  SPRAWDZ(t2.krok.empty());
  d.dzis = "2026-10-24";  // 3. noc
  TrescEkranu t3 = trescDnia("S5", "1", d);
  ROWNE(t3.tytul, "Brak danych od 3 nocy");
  ROWNE(t3.powod, "AirBridge nie odpowiada od 21.10. " + CO3H);  // 50 znaków (R-AW-24)
  ROWNE(t3.krok, "→ Sprawdź AirBridge i sieć. Ustawienia: strona 6 / 6.");
  // odrzucone logowanie (R-AW-34): ponawianie co 30 min także od 2. nocy, więc bez `co 3 h`
  d.logowanieOdrzucone = true;
  d.dzis = "2026-10-22";
  ROWNE(trescDnia("S5", "1", d).powod, t1.powod);
  d.dzis = "2026-10-23";
  SPRAWDZ(trescDnia("S5", "1", d).powod.find(CO30) != std::string::npos);
  SPRAWDZ(trescDnia("S5", "1", d).powod.find(CO3H) == std::string::npos);
  d.dzis = "2026-10-24";
  SPRAWDZ(trescDnia("S5", "1", d).powod.find(CO30) != std::string::npos);
  SPRAWDZ(trescDnia("S5", "1", d).powod.find(CO3H) == std::string::npos);
  // E-03 bez zmian
  d.logowanieOdrzucone = false;
  d.httpKod = KOD_E03;
  SPRAWDZ(trescDnia("S5", "1", d).powod.find("Sprawdzam") == std::string::npos);
}

static bool wZdarzeniach(const Wyb& w, const std::string& f) {
  for (auto& z : w.zdarzenia)
    if (z.find(f) != std::string::npos) return true;
  return false;
}
static const Wyb* wybudzenieO(const std::vector<Wyb>& v, const std::string& data, int minuta) {
  for (auto& w : v)
    if (w.data == data && w.minuta == minuta && w.powod == PowodWybudzenia::ZEGAR) return &w;
  return nullptr;
}

// R-AW-24, z dyrygentem (T-TEST-279): ekran S5 rysowany w dobach bez danych ma powód z 6.4 według nocy
// i rytmu ponawiania; port serwisowy podaje powód S5 (`S5: {tytuł} / {powód}`).
static void testR_AW_24_symulacjaTekstS5() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 10, 21, 5, 10), ep(2026, 10, 24, 12, 0), odpowiedziBezDanych);
  const Wyb* a = wybudzenieO(v, "2026-10-22", 11 * 60 + 30);
  SPRAWDZ(a && wZdarzeniach(*a, "S5: Noc 22.10 jeszcze w drodze. / AirBridge milczy od 21.10"));
  const Wyb* b = wybudzenieO(v, "2026-10-23", 14 * 60);
  SPRAWDZ(b && wZdarzeniach(*b, "S5: Brak danych od 2 nocy / Wyjazd z aparatem? Dane wrócą same. " + CO3H));
  const Wyb* c = wybudzenieO(v, "2026-10-24", 11 * 60);
  SPRAWDZ(c && wZdarzeniach(*c, "S5: Brak danych od 3 nocy / AirBridge nie odpowiada od 21.10. " + CO3H));
  // odrzucone logowanie: w 2. nocy co 30 min i zdanie `Ponawiam co 30 min.`
  PanelSym q;
  std::vector<Wyb> u = symuluj(q, ep(2026, 10, 21, 5, 10), ep(2026, 10, 23, 15, 0), odpowiedziLogowanieOdrzucone);
  const Wyb* e = wybudzenieO(u, "2026-10-23", 14 * 60 + 30);
  SPRAWDZ(e && wZdarzeniach(*e, "S5: Brak danych od 2 nocy / Wyjazd z aparatem? Dane wrócą same. " + CO30));
  bool co3h = false;
  for (auto& w : u) co3h = co3h || wZdarzeniach(w, CO3H);
  SPRAWDZ(!co3h);
}

// R-EKR-60 (wariant A): plan przerysowania ekranu dnia bez Wi-Fi 2 h po ostatnim narysowaniu, od 11:00 do okna
// slotu wieczornego (krok 2: stałe 21:00 z R-KON-17), tylko przy ekranie dnia ze stopką (dzienRysowany >= 0).
static WejsciePlanu dzienPo(int h, int m, int s, int rysH, int rysM, int rysS = 8) {
  WejsciePlanu w = wp(h, m, s);
  w.nocKompletna = true;
  w.sprawdzenie11 = true;
  w.dzienRysowany = rysH < 0 ? -1 : sd(rysH, rysM, rysS);
  return w;
}
static void testR_EKR_60_planOdswiezenia() {
  SPRAWDZ(!sieciowy(POWOD_ODSWIEZENIE));
  SPRAWDZ(!pobranieZegarem(POWOD_ODSWIEZENIE).pobierz && !pobranieZegarem(POWOD_ODSWIEZENIE).poranekCzynnikow);
  SPRAWDZ(!wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, POWOD_ODSWIEZENIE));  // z rysowaniem i odczytem czujnika
  ROWNE(nazwyPowodow(POWOD_ODSWIEZENIE), "odswiezenie");
  ROWNE(powodWpisuBaterii(PowodWybudzenia::ZEGAR, POWOD_ODSWIEZENIE), "zegar:odswiezenie");
  // rysowanie o 11:00:08 → 13:00, 15:00, 17:00, 19:00; po 19:00 nie (21:00 to początek okna slotu wieczornego)
  Termin t = nastepnyTermin(dzienPo(11, 0, 8, 11, 0));
  ROWNE_L(t.powody, POWOD_ODSWIEZENIE);
  ROWNE_L(t.sekund, sd(13, 0, 5) - sd(11, 0, 8));
  for (int h : {13, 15, 17}) {
    Termin n = nastepnyTermin(dzienPo(h, 0, 8, h, 0));
    ROWNE_L(n.powody, POWOD_ODSWIEZENIE);
    ROWNE_L(n.sekund, sd(h + 2, 0, 5) - sd(h, 0, 8));
  }
  ROWNE_L(nastepnyTermin(dzienPo(19, 0, 8, 19, 0)).powody, POWOD_SLOT_WIECZORNY_BEZ_SIECI);  // bez źródła: 21:00
  WejsciePlanu z = dzienPo(19, 0, 8, 19, 0);
  z.zrodloOtoczenia = true;
  ROWNE_L(nastepnyTermin(z).powody, POWOD_POGODA | POWOD_SLOT_WIECZORNY);  // 21:00 pogoda i ocena slotu
  ROWNE_L(nastepnyTermin(z).sekund, sd(21, 0, 5) - sd(19, 0, 8));
  // rysowanie o 18:30 → 20:30 (przed 21:00)
  ROWNE_L(nastepnyTermin(dzienPo(18, 30, 8, 18, 30)).sekund, sd(20, 30, 5) - sd(18, 30, 8));
  // 21:01 już w oknie wieczornym: bez R-EKR-60, ocena bez Wi-Fi o 21:00
  ROWNE_L(nastepnyTermin(dzienPo(19, 1, 8, 19, 1)).powody, POWOD_SLOT_WIECZORNY_BEZ_SIECI);
  // nie przed 11:00: rysowanie o 07:00 → 11:00 (gdy 11:00 nie ma sprawdzenia), a ze sprawdzeniem 11:00 bez bitu
  WejsciePlanu r = dzienPo(7, 0, 8, 7, 0);
  ROWNE_L(nastepnyTermin(r).powody, POWOD_ODSWIEZENIE);
  ROWNE_L(nastepnyTermin(r).sekund, sd(11, 0, 5) - sd(7, 0, 8));
  r.sprawdzenie11 = false;
  ROWNE_L(nastepnyTermin(r).powody, POWOD_SPRAWDZENIE_11);  // wybudzenie o 11:00 i tak rysuje ekran dnia
  // ekran dnia bez przerysowań (S5, S10, ekran nocny) albo dziś nierysowany: bez terminu
  ROWNE_L(nastepnyTermin(dzienPo(11, 0, 8, -1, 0)).powody, POWOD_SLOT_WIECZORNY_BEZ_SIECI);  // 21:00
  // ekran otwarty przyciskiem: powrót R-PRZ-02, nie przerysowanie
  WejsciePlanu k = dzienPo(14, 10, 0, 13, 0);
  k.ekran = Ekran::K1;
  k.sekundOdNacisniecia = 0;
  ROWNE_L(nastepnyTermin(k).powody, POWOD_POWROT);
  ROWNE_L(nastepnyTermin(dzienPo(14, 15, 2, 14, 15, 2)).sekund, sd(16, 15, 5) - sd(14, 15, 2));
  // bez czasu: tylko POWOD_CZAS (R-AW-20)
  WejsciePlanu bez = dzienPo(13, 0, 5, 11, 0);
  bez.zegar = false;
  ROWNE_L(nastepnyTermin(bez).powody, POWOD_CZAS);
  ROWNE_L(powodyTeraz(bez), POWOD_CZAS);
  // powód bieżącego wybudzenia o 13:00:05 po obsłużonym 11:00:05
  WejsciePlanu b = dzienPo(13, 0, 5, 11, 0);
  b.obsluzoneZnane = true;
  b.obsluzoneDo = sd(11, 0, 5);
  ROWNE_L(powodyTeraz(b), POWOD_ODSWIEZENIE);
  // termin przerysowania w trakcie wybudzenia (przycisk o 12:59:50, czuwanie do 13:00:30): bez snu 1 s
  WejsciePlanu c = dzienPo(13, 0, 30, 11, 0);
  c.obsluzoneZnane = true;
  c.obsluzoneDo = sd(12, 59, 50);
  ROWNE_L(powodyZalegle(c), 0);
  SPRAWDZ(nastepnyTermin(c).sekund > ZALEGLY_TERMIN_S);
  // po północy (nowa doba) bez przerysowań z wczoraj
  WejsciePlanu n = dzienPo(23, 50, 5, 19, 0);
  ROWNE_L(nastepnyTermin(n).powody, POWOD_CZUJNIKI);
}

// T-WYM-28, kolumna „Weryfikacja”: o 14:05 przy nocy kompletnej i ekranie dnia narysowanym o 13:00
// → sen do 15:00:05 (R-EKR-60), nie do 14:30; o 19:05 po przerysowaniu o 19:00 → pogoda o 21:00 (ze źródłem).
static void testT_WYM_28_przykladOdswiezenia() {
  Termin a = nastepnyTermin(dzienPo(14, 5, 5, 13, 0));
  ROWNE_L(a.powody, POWOD_ODSWIEZENIE);
  ROWNE_L(a.sekund, sd(15, 0, 5) - sd(14, 5, 5));
  WejsciePlanu b = dzienPo(19, 5, 5, 19, 0);
  b.zrodloOtoczenia = true;
  ROWNE_L(nastepnyTermin(b).powody, POWOD_POGODA | POWOD_SLOT_WIECZORNY);  // R-KON-01
}

// Symulacja jak symuluj(), z jednym naciśnięciem ▶ o chwili `przycisk` (0 = bez) i numerem pierwszej klatki ekranu
// każdego wybudzenia; czujnik w obudowie podaje co godzinę inną temperaturę (stopka `przy łóżku` się zmienia).
static std::vector<Wyb> symulujZPrzyciskiem(PanelSym& s, uint32_t od, uint32_t doT, Odpowiedzi odp, uint32_t przycisk,
                                          std::vector<size_t>& klatkaOd) {
  std::vector<Wyb> v;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 600 && t < doT; i++) {
    if (przycisk && t > przycisk) {
      t = przycisk;
      pw = PowodWybudzenia::PRZYCISK;
      przycisk = 0;
      s.p.start.budzace = BIT_PRAWY;
      s.przyciski.skrypt = {{0, BIT_PRAWY}, {300, 0}};
    }
    odp(s, t);
    s.p.czujniki.o.temp = 15.0f + (float)(minutaLok(t) / 60);
    klatkaOd.push_back(s.ekran.klatki.size());
    Wyb w = wybudz(s, t, pw);
    v.push_back(w);
    s.p.start.budzace = 0;
    s.przyciski.skrypt.clear();
    if (!w.spal) break;
    uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
    if (snu == 0) snu = 1;
    t += w.m.msPrzySnie / 1000 + snu;
    pw = PowodWybudzenia::ZEGAR;
  }
  return v;
}
static std::vector<int> minutyZPowodem(const std::vector<Wyb>& v, const std::string& powod) {
  std::vector<int> m;
  for (auto& w : v)
    for (auto& z : w.zdarzenia)
      if (z.find("wybudzenie " + powod + ",") != std::string::npos) { m.push_back(w.minuta); break; }
  return m;
}

// R-EKR-60, R-NF-02, T-TEST-281 z dyrygentem: zwykła doba 4.11 → przerysowania bez Wi-Fi o 13:00, 15:00, 17:00,
// 19:00, każde z odczytem czujnika i jednym odświeżeniem; budżet doby (≤ 80 wybudzeń, Wi-Fi bez zmian, ≤ 30 odświeżeń).
// wariant bez slotu dziennego (R-EKR-60, przypadek zastępczy „przed wersją ze slotem dziennym w symulatorze”);
// w slocie dziennym R-EKR-60 milczy (test/testy_slot_dzienny_w_planie.cpp testR_EKR_60_milczyWSlocieDziennym).
static void testR_EKR_60_symulacjaDoby() {
  BezSlotuDziennego bezSlotu;
  PanelSym s;
  std::vector<size_t> k;
  std::vector<Wyb> v = symulujZPrzyciskiem(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 5), odpowiedziZwyklej, 0, k);
  std::vector<int> oczekiwane = {13 * 60, 15 * 60, 17 * 60, 19 * 60};
  SPRAWDZ(minutyZPowodem(v, "zegar:odswiezenie") == oczekiwane);
  int odswiezen = 0, sieciowych = 0, krotkich = 0;
  for (size_t i = 0; i < v.size(); i++) {
    const Wyb& w = v[i];
    odswiezen += w.m.odswiezen;
    sieciowych += w.m.polaczen > 0;
    if (i > 0 && w.ep - v[i - 1].ep < 60) krotkich++;
    if (std::find(oczekiwane.begin(), oczekiwane.end(), w.minuta) == oczekiwane.end()) continue;
    if (w.powod != PowodWybudzenia::ZEGAR) continue;
    SPRAWDZ(w.m.polaczen == 0 && w.m.zadan == 0 && w.m.wifiMs == 0);
    ROWNE_L(w.m.odswiezen, 1);
    // stopka z odczytem tej godziny: klatka inna niż poprzednia narysowana
    SPRAWDZ(k[i] >= 1 && k[i] < s.ekran.klatki.size() && s.ekran.klatki[k[i]] != s.ekran.klatki[k[i] - 1]);
  }
  SPRAWDZ(v.size() <= 80);
  ROWNE_L(v.size(), 41);           // 33 wybudzenia doby kroku 2, 4 przerysowania, 4 oceny wieczoru bez Wi-Fi
  ROWNE_L(sieciowych, 8);          // bez zmian: start, 05:00…07:00, 11:00, 23:30
  SPRAWDZ(odswiezen <= 30);
  ROWNE_L(krotkich, 0);            // bez snu 1 s
  ROWNE_L(ileWierszy(bezNaglowka(s.p.pliki.pliki["/log/wybudzenia.csv"]), ""), v.size());
  ROWNE_L(wierszeBateriiBezPowodu(bezNaglowka(s.p.pliki.pliki["/log/wybudzenia.csv"])), 0);
  // ▶ o 14:10 → K1, powrót o 14:15 rysuje ekran dnia → następne przerysowania 16:15, 18:15, 20:15
  PanelSym q;
  std::vector<size_t> kq;
  std::vector<Wyb> u = symulujZPrzyciskiem(q, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 5), odpowiedziZwyklej,
                                           ep(2026, 11, 4, 14, 10), kq);
  std::vector<int> poPrzycisku = {13 * 60, 16 * 60 + 15, 18 * 60 + 15, 20 * 60 + 15};
  SPRAWDZ(minutyZPowodem(u, "zegar:odswiezenie") == poPrzycisku);
  SPRAWDZ(minutyZPowodem(u, "zegar:powrot") == std::vector<int>{14 * 60 + 15});
  // T-TEST-281 odczyt czujnika bez zmian → przerysowania o tych samych
  // godzinach, ale klatka identyczna z wyświetloną, więc bez odświeżenia e-ink
  PanelSym c;
  std::vector<Wyb> st = symuluj(c, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 5), odpowiedziZwyklej);
  SPRAWDZ(minutyZPowodem(st, "zegar:odswiezenie") == oczekiwane);
  for (auto& w : st) {
    if (std::find(oczekiwane.begin(), oczekiwane.end(), w.minuta) == oczekiwane.end()) continue;
    ROWNE_L(w.m.odswiezen, 0);
    bool bezZmian = false;
    for (auto& z : w.zdarzenia) bezZmian = bezZmian || z.find("bez zmian: klatka jak wyświetlona") != std::string::npos;
    SPRAWDZ(bezZmian);
  }
}

// R-EKR-60 w dobach bez danych (T-TEST-279): S5 na ekranie → bez przerysowań; ekran nocny i slot nocny → bez.
static void testR_EKR_60_bezPrzerysowanWS5() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 10, 21, 5, 10), ep(2026, 10, 24, 12, 0), odpowiedziBezDanych);
  int w22 = 0, wNocy = 0;
  for (auto& w : v) {
    bool odsw = false;
    for (auto& z : w.zdarzenia) odsw = odsw || z.find("wybudzenie zegar:odswiezenie") != std::string::npos;
    if (!odsw) continue;
    if (w.data == "2026-10-22" || w.data == "2026-10-23") w22++;
    if (slotNocny(w.minuta) || w.minuta < 11 * 60 || w.minuta >= 21 * 60) wNocy++;
  }
  ROWNE_L(w22, 0);
  ROWNE_L(wNocy, 0);
}

// ============================================================================================================
// (d) scenariusze A, C, F
// ============================================================================================================

// Pełne odświeżenia e-ink na dobę i ile z nich dało klatkę identyczną z poprzednią wyświetloną (zbędne odświeżenie).
struct OdswiezeniaDoby {
  int odswiezen = 0, identycznych = 0;
};
static std::map<std::string, OdswiezeniaDoby> odswiezeniaNaDobe(const std::vector<Wyb>& v, const std::vector<size_t>& k,
                                                               const AtrapaWyswietlacza& e) {
  std::map<std::string, OdswiezeniaDoby> m;
  for (size_t i = 0; i < v.size(); i++) {
    size_t koniec = i + 1 < v.size() ? k[i + 1] : e.klatki.size();
    for (size_t j = k[i]; j < koniec; j++) {
      m[v[i].data].odswiezen++;
      if (j > 0 && e.klatki[j] == e.klatki[j - 1]) m[v[i].data].identycznych++;
    }
  }
  return m;
}

// Noc z poranka 4.11 bez terapii: AirBridge działa cały dzień, ale tej nocy nie ma na liście (R-DANE-20).
static const long DOBA_BEZ_TERAPII = nrDnia("2026-11-03");
static void odpowiedziBezTerapii(PanelSym& s, uint32_t) {
  s.siec.odpowiedzi.clear();
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_BEZ_TERAPII - 3, DOBA_BEZ_TERAPII - 1));
  for (long d = DOBA_BEZ_TERAPII - 3; d < DOBA_BEZ_TERAPII; d++) s.siec.dodaj(sciezkaNocy(d), 200, raport(d));
}

// scenariusz A: budżet pełnych odświeżeń ≤ 30 na dobę (spec funkcjonalna 8.1) także w dobach bez
// danych (T-TEST-279), przy odrzuconym logowaniu i w dniu bez terapii; ponowienie bez zmiany klatki nie odświeża e-ink
// (wariant A: skrót klatki w RTC). Bez naciśnięć, więc żadna klatka nie może być identyczna z poprzednią wyświetloną.
// wymaganie: R-NF-01
static void testR_NF_01_odswiezeniaDobBezDanych() {
  struct Przypadek {
    const char* nazwa;
    Odpowiedzi odp;
    uint32_t od, doT;
  };
  const Przypadek pp[] = {
      {"bez danych", odpowiedziBezDanych, ep(2026, 10, 21, 5, 10), ep(2026, 10, 25, 0, 5)},
      {"odrzucone logowanie", odpowiedziLogowanieOdrzucone, ep(2026, 10, 21, 5, 10), ep(2026, 10, 25, 0, 5)},
      {"bez terapii", odpowiedziBezTerapii, ep(2026, 11, 3, 12, 0), ep(2026, 11, 5, 0, 5)},
  };
  for (const Przypadek& p : pp) {
    PanelSym s;
    std::vector<size_t> k;
    std::vector<Wyb> v = symulujZPrzyciskiem(s, p.od, p.doT, p.odp, 0, k);
    for (auto& x : odswiezeniaNaDobe(v, k, s.ekran)) {
      printf("  %s %s: odświeżeń %d, identycznych z poprzednią klatką %d\n", p.nazwa, x.first.c_str(),
             x.second.odswiezen, x.second.identycznych);
      SPRAWDZ(x.second.odswiezen <= 30);
      ROWNE_L(x.second.identycznych, 0);
    }
  }
}

// scenariusz C: ekran S5 i plan wybudzeń liczą noce bez danych tak samo (R-AW-32 z granicą 05:00
// jak R-EKR-57: odczyt przed 05:00 nie zna nocy z tego dnia). Ostatni udany odczyt 21.10 o 03:00, potem AirBridge
// milczy → 22.10 to druga noc bez danych: S5 `Sprawdzam co 3 h.` i ponowienia po 11:00 tylko 14:00, 17:00, 20:00,
// 23:00 (R-DANE-18); tekst ekranu zawsze zgodny z tym, jak często panel naprawdę pyta.
static const long DOBA_C3 = nrDnia("2026-10-20");  // noc z poranka 21.10
static void odpowiedziDo0330(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  if (t < ep(2026, 10, 21, 3, 30)) {
    s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_C3 - 5, DOBA_C3 - 1));
    for (long d = DOBA_C3 - 5; d < DOBA_C3; d++) s.siec.dodaj(sciezkaNocy(d), 200, raport(d));
  }
}
static void sprawdzS5IPonowienia22(const std::vector<Wyb>& v, const char* nazwa) {
  const std::string co3h = "Sprawdzam co\xC2\xA0" "3\xC2\xA0" "h.", co30 = "co\xC2\xA0" "30\xC2\xA0" "min.";
  int z3h = 0, z30 = 0;
  for (auto& w : v)
    if (w.data == "2026-10-22" && w.minuta >= 11 * 60)
      for (auto& z : w.zdarzenia)
        if (z.find("S5: ") != std::string::npos) {
          z3h += z.find(co3h) != std::string::npos;
          z30 += z.find(co30) != std::string::npos;
        }
  std::vector<int> siec = sieciowePo11(v, "2026-10-22");
  std::vector<int> co3 = {14 * 60, 17 * 60, 20 * 60, 23 * 60, 23 * 60 + 30};
  printf("  %s: 22.10 S5 `co 3 h` %d, `co 30 min` %d, wybudzeń z Wi-Fi po 11:00 %zu\n", nazwa, z3h, z30, siec.size());
  SPRAWDZ(z3h > 0 && z30 == 0);  // druga noc bez danych
  SPRAWDZ(siec == co3);          // plan liczy tak samo jak ekran
}
static void testR_AW_32_nocyBezDanychEkranIPlan() {
  {
    // nowy panel: start 21.10 03:00, jedyny udany odczyt przed 05:00
    PanelSym s;
    std::vector<Wyb> v = symuluj(s, ep(2026, 10, 21, 3, 0), ep(2026, 10, 23, 0, 5), odpowiedziDo0330);
    sprawdzS5IPonowienia22(v, "start 21.10 03:00");
  }
  {
    // panel z historią od 20.10, restart 21.10 03:00 (np. wgranie), ostatni udany odczyt 03:00, potem cisza
    PanelSym s;
    std::vector<Wyb> v;
    zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
    uint32_t t = ep(2026, 10, 20, 5, 10), reset = ep(2026, 10, 21, 3, 0), doT = ep(2026, 10, 23, 0, 5);
    PowodWybudzenia pw = PowodWybudzenia::START;
    for (int i = 0; i < 600 && t < doT; i++) {
      if (reset && t > reset) {
        t = reset;
        pw = PowodWybudzenia::START;
        reset = 0;
      }
      odpowiedziDo0330(s, t);
      Wyb w = wybudz(s, t, pw);
      v.push_back(w);
      if (!w.spal) break;
      uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
      if (snu == 0) snu = 1;
      t += w.m.msPrzySnie / 1000 + snu;
      pw = PowodWybudzenia::ZEGAR;
    }
    sprawdzS5IPonowienia22(v, "historia i restart 21.10 03:00");
  }
}

// scenariusz F: przerysowanie R-EKR-60 bez pobrania zachowuje tryb i
// linię uwagi z ostatniego narysowania; zmienia tylko stopkę. Źródło otoczenia zgłasza `co2_milczy`: o 11:00 linia
// E-08 i ekran danych; o 13:00 ostrzeżenie jest już starsze niż 60 min (bez pobrania), a ekran nad stopką ma zostać
// taki jak o 11:00.
static const char* OTO_CO2_MILCZY =
    "{\"wersja\":1,\"ostrzezenia\":[{\"kod\":\"co2_milczy\",\"od\":\"2026-11-04T05:00:00+01:00\"}]}";
static void odpowiedziZwyklejZOtoczeniem(PanelSym& s, uint32_t t) {
  odpowiedziZwyklej(s, t);
  s.siec.dodaj("/otoczenie", 200, OTO_CO2_MILCZY);
}
static void testR_EKR_60_przerysowanieZachowujeTrybILinie() {
  BezSlotuDziennego bezSlotu;  // przerysowanie R-EKR-60 o 13:00 tylko poza slotem dziennym
  KonfiguracjaPanelu k = KONF;
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  k.otoToken = "t";
  PanelSym s;
  std::vector<Wyb> v;
  std::vector<size_t> kl;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  uint32_t t = ep(2026, 11, 4, 0, 5), doT = ep(2026, 11, 4, 16, 0);
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 300 && t < doT; i++) {
    odpowiedziZwyklejZOtoczeniem(s, t);
    s.p.czujniki.o.temp = 15.0f + (float)(minutaLok(t) / 60);
    kl.push_back(s.ekran.klatki.size());
    Wyb w = wybudz(s, t, pw, k);
    v.push_back(w);
    if (!w.spal) break;
    uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
    if (snu == 0) snu = 1;
    t += w.m.msPrzySnie / 1000 + snu;
    pw = PowodWybudzenia::ZEGAR;
  }
  size_t i11 = v.size(), i13 = v.size();
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i].minuta == 11 * 60) i11 = i;
    if (v[i].minuta == 13 * 60) i13 = i;
  }
  SPRAWDZ(i11 < v.size() && i13 < v.size());
  if (i11 >= v.size() || i13 >= v.size()) return;
  SPRAWDZ(v[i11].m.odswiezen == 1 && v[i13].m.odswiezen == 1 && v[i13].m.polaczen == 0);
  if (kl[i11] >= s.ekran.klatki.size() || kl[i13] >= s.ekran.klatki.size()) return;
  const auto& a = s.ekran.klatki[kl[i11]];
  const auto& b = s.ekran.klatki[kl[i13]];
  int rozneGora = 0, rozneDol = 0;
  for (int y = 0; y < 480; y++)
    for (int x = 0; x < 100; x++)
      if (a[(size_t)(y * 100 + x)] != b[(size_t)(y * 100 + x)]) (y < 430 ? rozneGora : rozneDol)++;
  printf("  przerysowanie 13:00 wobec 11:00: bajtów różnych nad stopką %d, w stopce %d\n", rozneGora, rozneDol);
  ROWNE_L(rozneGora, 0);
  SPRAWDZ(rozneDol > 0);
}

// jedno liczenie nocy bez danych dla ekranu i planu (src/noc.h nocyBezDanychZRzedu()).
static void testR_AW_32_jednoLiczenie() {
  std::vector<Noc> h;
  for (int i = 0; i < 10; i++) {
    Noc n;
    n.data = dodajDni("2026-10-20", -i);
    h.push_back(n);
  }
  DaneDnia d;
  d.hist = &h;
  d.dzis = "2026-10-22";
  d.abData = "2026-10-21";
  d.abMinuta = 3 * 60;  // odczyt 21.10 03:00: noc z 21.10 jeszcze nieznana (granica 05:00)
  ROWNE_L(nocyBezDanych(d), 2);
  ROWNE_L(nocyBezDanych(true, "2026-10-22", "2026-10-21", 3 * 60, "2026-10-21", &h), 2);
  d.abMinuta = 6 * 60;  // odczyt po 05:00 zna noc z 21.10
  ROWNE_L(nocyBezDanych(d), 1);
  ROWNE_L(nocyBezDanych(true, "2026-10-22", "2026-10-21", 6 * 60, "2026-10-21", &h), 1);
  // bez odczytu: od pierwszej nieudanej próby, ograniczone luką w historii (noc 20.10 jest)
  d.abData.clear();
  d.abMinuta = -1;
  d.bladData = "2026-10-19";
  ROWNE_L(nocyBezDanych(d), 2);
  ROWNE_L(nocyBezDanych(true, "2026-10-22", "", -1, "2026-10-19", &h), 2);
  ROWNE_L(nocyBezDanych(false, "2026-10-22", "", -1, "2026-10-19", &h), 0);
  // nowy panel bez historii i bez odczytu, bez daty próby: 1. noc
  std::vector<Noc> pusta;
  ROWNE_L(nocyBezDanychZRzedu("2026-10-22", "", -1, "", &pusta), 1);
  ROWNE_L(nocyBezDanychZRzedu("zła", "2026-10-21", 0, "", nullptr), 1);
  ROWNE_L(nocyBezDanychZRzedu("2026-10-22", "2026-10-25", 0, "", nullptr), 0);  // odczyt „z przyszłości”
}

// (wariant A): klatka identyczna z wyświetloną bez odświeżenia e-ink; po 24 h, po starcie innym niż
// wybudzenie z uśpienia zawsze odświeżenie. Odczyt czujnika stały, więc ekran dnia się nie zmienia.
// wymaganie: R-NF-01
static void testR_NF_01_klatkaBezZmian() {
  const uint8_t wzor[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  SPRAWDZ(crc32(wzor, sizeof wzor) == 0xCBF43926u);
  SPRAWDZ(crc32(nullptr, 5) == 0 && crc32(wzor, 0) == 0);
  BezSlotuDziennego bezSlotu;  // przerysowania R-EKR-60 o 13:00, 15:00, 17:00 tylko poza slotem dziennym
  PanelSym s;
  symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 12, 0), odpowiedziZwyklej);  // do snu po 11:00
  uint32_t t13 = ep(2026, 11, 4, 13, 0, 5);
  odpowiedziZwyklej(s, t13);
  Wyb a = wybudz(s, t13, PowodWybudzenia::ZEGAR);
  ROWNE_L(a.m.odswiezen, 0);  // przerysowanie R-EKR-60, ta sama klatka
  SPRAWDZ(s.p.start.stan.klatkaOd != 0 && s.p.start.stan.klatkaOd < t13);
  uint32_t t15 = ep(2026, 11, 4, 15, 0, 5);
  s.p.start.stan.klatkaOd = t15 - KLATKA_NAJDLUZEJ_S;  // ostatnie pełne odświeżenie 24 h temu
  odpowiedziZwyklej(s, t15);
  Wyb b = wybudz(s, t15, PowodWybudzenia::ZEGAR);
  ROWNE_L(b.m.odswiezen, 1);
  ROWNE_L(s.p.start.stan.klatkaOd, t15);
  uint32_t t17 = ep(2026, 11, 4, 17, 0, 5);
  odpowiedziZwyklej(s, t17);
  ROWNE_L(wybudz(s, t17, PowodWybudzenia::ZEGAR).m.odswiezen, 0);
  // start po zaniku zasilania (RTC zostaje w atrapie): pierwsze rysowanie zawsze
  uint32_t t18 = ep(2026, 11, 4, 18, 0, 5);
  odpowiedziZwyklej(s, t18);
  ROWNE_L(wybudz(s, t18, PowodWybudzenia::START).m.odswiezen, 1);
}

// Log doby z symulatora: zwykła doba 4.11.2026
// (odpowiedziZwyklej, odczyt czujnika stały) w dwóch wariantach, bez slotu dziennego i ze slotem dziennym. Jeden
// wiersz na wybudzenie: godzina, pole `powod` z /bateria.csv, Wi-Fi tak/nie, czas Wi-Fi atrapy; na końcu suma.
// Wynik to plik log-doby.txt (komenda w jego nagłówku); dane do, nie pomiar.
static void drukujLogDoby(const char* tytul, const std::vector<Wyb>& v, const std::string& bateria) {
  std::vector<std::string> powody;
  size_t a = 0;
  while (a < bateria.size()) {
    size_t b = bateria.find('\n', a);
    if (b == std::string::npos) b = bateria.size();
    std::string w = bateria.substr(a, b - a);
    a = b + 1;
    size_t x = std::string::npos;  // pole `powod` za czwartym średnikiem (epoch;mV;wifi_ms;aktywny_ms;powod)
    for (int i = 0; i < 4; i++) {
      x = w.find(';', x == std::string::npos ? 0 : x + 1);
      if (x == std::string::npos) break;
    }
    powody.push_back(x == std::string::npos ? "?" : w.substr(x + 1));
  }
  printf("\n## %s\n\n", tytul);
  printf("| godzina | powód (`/bateria.csv`) | Wi-Fi | czas Wi-Fi atrapy [ms] |\n|---|---|---|---|\n");
  uint32_t razem = 0;
  int zWifi = 0;
  for (size_t i = 0; i < v.size(); i++) {
    const Wyb& w = v[i];
    razem += w.m.wifiMs;
    zWifi += w.m.polaczen > 0;
    printf("| %s %02d:%02d | %s | %s | %u |\n", w.data.c_str(), w.minuta / 60, w.minuta % 60,
           i < powody.size() ? powody[i].c_str() : "?", w.m.polaczen > 0 ? "tak" : "nie", w.m.wifiMs);
  }
  unsigned dziesiate = (razem + 3000) / 6000;  // minuty z jednym miejscem po przecinku
  printf("\nSuma: wybudzeń %zu, z Wi-Fi %d, czas Wi-Fi atrapy %u ms (%u,%u min).\n", v.size(), zWifi, razem,
         dziesiate / 10, dziesiate % 10);
}
static void logDoby() {
  printf("# Log doby z symulatora (plan wybudzeń i energia)\n\n");
  printf("Plik generowany, nie edytować ręcznie. Odtworzenie (z katalogu repozytorium):\n\n");
  printf("    bash tools/prototyp/testy.sh >/dev/null && "
         "LOG_DOBY=1 firmware/prototyp/.pio/testy/testy_plan_wybudzen > log-doby.txt\n\n");
  printf("Doba 4.11.2026 od startu zasilania o 00:05 do 00:05 dnia następnego, atrapy z test/atrapy/ "
         "(test/testy_plan_wybudzen.cpp, odpowiedziZwyklej: terapia do 06:40, noc w raporcie od 06:40, odczyt czujnika "
         "stały, bez źródła otoczenia). Czas Wi-Fi to czas atrapy (łączenie 1,5 s), nie pomiar panelu; na panelu "
         "więcej. Wariant bez slotu dziennego: plan bez slotu dziennego (jawna flaga testowa "
         "KonfiguracjaPanelu::bezSlotuDziennego), kryterium: Wi-Fi < 10 min. Wariant ze slotem dziennym: import 07:00 → start slotu "
         "dziennego 08:00 (R-KON-41), oceny 09:00…20:00 (R-KON-42) z Wi-Fi tylko na `/api/status`.\n");
  {
    BezSlotuDziennego bezSlotu;
    PanelSym s;
    std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 5), odpowiedziZwyklej);
    drukujLogDoby("Doba bez slotu dziennego", v, bezNaglowka(s.p.pliki.pliki["/log/wybudzenia.csv"]));
  }
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 5), odpowiedziZwyklej);
  drukujLogDoby("Doba ze slotem dziennym", v, bezNaglowka(s.p.pliki.pliki["/log/wybudzenia.csv"]));
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  if (getenv("LOG_DOBY")) {  // tylko log doby, bez testów
    logDoby();
    return 0;
  }
  // (a)
  testT_WYM_28_przykladySpec();
  testR_DANE_19_oknoPoranne();
  testR_DANE_24_sprawdzenie11();
  testR_DANE_18_ponowienia();
  testR_DANE_18_brakZapytanWNocy();
  testR_KON_31_poczatekNocy();
  testT_WYM_31_terminyCzujnikow();
  testR_PRZ_02_powrot();
  testR_AW_20_bezZegara();
  testR_PIE_13_terminPytania();
  testR_NF_02_powodyTeraz();
  testR_NF_02_nazwyPowodow();
  testR_NF_03_sieciowy();
  testT_WYM_20_czuwanieTylkoPoPrzycisku();
  testT_WYM_29_planSnuZTerminu();
  testR_DANE_18_nocyBezDanych();
  testT_WYM_28_planDobyZwyklej();
  testT_NF_02_planDobyBezDanych();
  // (b)
  testT_WYM_20_kolejnoscKrokow();
  testT_WYM_31_wybudzeniePomiarowe();
  testT_WYM_20_czuwanieDyrygent();
  testT_NF_06_wifiPrzedRysowaniem();
  testR_NF_02_symulacjaZwyklejDoby();
  testR_DANE_18_symulacjaBezDanych();
  testR_DANE_18_symulacjaLogowanieOdrzucone();
  // (c)
  testR_AW_24_sprawdzamCo3h();
  testR_AW_24_symulacjaTekstS5();
  testR_EKR_60_planOdswiezenia();
  testT_WYM_28_przykladOdswiezenia();
  testR_EKR_60_symulacjaDoby();
  testR_EKR_60_bezPrzerysowanWS5();
  // (d)
  testR_NF_01_odswiezeniaDobBezDanych();
  testR_AW_32_nocyBezDanychEkranIPlan();
  testR_EKR_60_przerysowanieZachowujeTrybILinie();
  testR_AW_32_jednoLiczenie();
  testR_NF_01_klatkaBezZmian();
  printf("testy_plan_wybudzen: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
