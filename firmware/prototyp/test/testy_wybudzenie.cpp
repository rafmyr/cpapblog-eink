// SPDX-License-Identifier: GPL-3.0-or-later
// Testy wybudzenia (rdzeń: src/wybudzenie.cpp, src/start.cpp, src/dyrygent.cpp).
//
// (a) Testy charakteryzujące decyzji wybudzenia w rdzeniu (src/wybudzenie.h). Utrwalają obecne zachowanie,
//     nie spec docelową; gdzie się różnią, komentarz testu podaje krok 13.1, w którym to się zmienia.
// (b) Testy siatki aktualizacji (src/start.h): licznik startów po błędzie (T-WYM-82), tryb awaryjny U-06 (T-WYM-83,
//     R-AW-39), samosprawdzenie (T-WYM-41), strażnik zadania (T-WYM-81), log sterty i stosu.
// (c) Testy dyrygenta z atrapami adapterów (test/atrapy/adaptery_atrapy.h; T-TEST-272, T-TEST-273 w zakresie PC).
//
// Uruchomienie: tools/prototyp/testy.sh (osobny program testy_wybudzenie, po testach prototypu).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dobranoc.h"
#include "dyrygent.h"
#include "dzien.h"
#include "nawigacja.h"
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

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";  // strefa ustawiana na panelu przy starcie

// Czas unix chwili lokalnej (strefa panelu ustawiona w main()).
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
static int mdoby(int h, int m) { return h * 60 + m; }

// ============================================================================================================
// (a) Testy charakteryzujące decyzji wybudzenia
// ============================================================================================================

// Zegar znany: t > 1700000000
static void testZegarZnany() {
  SPRAWDZ(!zegarZnany(0));
  SPRAWDZ(!zegarZnany(1700000000));
  SPRAWDZ(zegarZnany(1700000001));
  SPRAWDZ(zegarZnany(ep(2026, 11, 4, 7, 0)));
}

// T-WYM-24: wybudzenie przyciskiem bez Wi-Fi, poza ↻ na S5 (R-PRZ-06) i ekranem testu.
// Odczyt przy każdym wybudzeniu poza przyciskiem (z poranną decyzją o pytaniu); po naciśnięciu: ↻ ponawia
// odczyt z poranną decyzją, wejście na ekran testu daje odczyt przerywalny (raz, także zaraz po wybudzeniu).
static void testT_WYM_24_kiedyPobierac() {
  Pobranie z = pobranieNaStarcie(PowodWybudzenia::ZEGAR);
  SPRAWDZ(z.pobierz && !z.przerywalne && z.poranekCzynnikow);
  Pobranie s = pobranieNaStarcie(PowodWybudzenia::START);
  SPRAWDZ(s.pobierz && !s.przerywalne && s.poranekCzynnikow);
  Pobranie p = pobranieNaStarcie(PowodWybudzenia::PRZYCISK);
  SPRAWDZ(!p.pobierz && !p.przerywalne && !p.poranekCzynnikow);

  KontekstNaw k;  // ekran dnia z danymi
  // ▶ na ekranie dnia → K1 bez pobrania (spec T-WYM-24: „Wybudzenie ▶ na ekranie dnia → K1 narysowany, brak WiFi.begin”)
  Przejscie pr = nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, k);
  SPRAWDZ(pr.ekran == Ekran::K1);
  SPRAWDZ(!pobraniePoNacisnieciu(pr, Ekran::DZIEN, true).pobierz);
  SPRAWDZ(!pobraniePoNacisnieciu(pr, Ekran::DZIEN, false).pobierz);
  // ◀ na ekranie dnia (nic), ↻ w K1 → ekran dnia: bez pobrania
  SPRAWDZ(!pobraniePoNacisnieciu(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, false, k), Ekran::DZIEN, true).pobierz);
  SPRAWDZ(!pobraniePoNacisnieciu(nacisniecie(Ekran::K1, Przycisk::SRODEK, false, k), Ekran::K1, true).pobierz);
  // ▶ 2 s na U-03 (ciąg U-01 ▶ 2 s U-03 ▶ 2 s U-05; ▶ na `Test sprzętu` w U-01, wcześniej ◀ 2 s w K2) → ekran testu:
  // pobranie przerywalne, bez porannego pytania (przy przycisku budzącym i w czuwaniu)
  Przejscie test = nacisniecie(Ekran::FORMULARZ, Przycisk::PRAWY, true, k);  // ▶ 2 s
  SPRAWDZ(test.ekran == Ekran::TEST);
  Pobranie t1 = pobraniePoNacisnieciu(test, Ekran::FORMULARZ, true);
  SPRAWDZ(t1.pobierz && t1.przerywalne && !t1.poranekCzynnikow);
  Pobranie t2 = pobraniePoNacisnieciu(test, Ekran::FORMULARZ, false);
  SPRAWDZ(t2.pobierz && t2.przerywalne && !t2.poranekCzynnikow);
  // test już na ekranie: przy przycisku budzącym pobiera (setup() po obsluz()), w czuwaniu nie (`!bylTest`)
  Przejscie zostaje = {Ekran::TEST, Akcja::NIC};
  SPRAWDZ(pobraniePoNacisnieciu(zostaje, Ekran::TEST, true).pobierz);
  SPRAWDZ(!pobraniePoNacisnieciu(zostaje, Ekran::TEST, false).pobierz);
}

// R-PRZ-06: ↻ na S5 ponawia pobranie i poranne pytanie.
static void testR_PRZ_06_ponowNaS5() {
  KontekstNaw k;
  k.s5 = true;
  Przejscie pr = nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, k);
  SPRAWDZ(pr.akcja == Akcja::PONOW_POBRANIE);
  Pobranie b = pobraniePoNacisnieciu(pr, Ekran::DZIEN, true);
  SPRAWDZ(b.pobierz && b.przerywalne && b.poranekCzynnikow);
  Pobranie c = pobraniePoNacisnieciu(pr, Ekran::DZIEN, false);
  SPRAWDZ(c.pobierz && c.przerywalne && c.poranekCzynnikow);
  // ▶ na ekranie testu w czuwaniu: to samo (Akcja::PONOW_POBRANIE na TEST; ↻ to ekran dnia)
  Przejscie t = nacisniecie(Ekran::TEST, Przycisk::PRAWY, false, KontekstNaw());
  SPRAWDZ(t.akcja == Akcja::PONOW_POBRANIE);
  SPRAWDZ(pobraniePoNacisnieciu(t, Ekran::TEST, false).pobierz);
}

// R-PRZ-02 i R-PRZ-18: powrót do ekranu dnia po 5 min bez naciśnięcia, poza slotem nocnym.
// START → ekran dnia; po upływie czasu powrotu (sekundDoPowrotu() == 0) ekran dnia; ekran testu po wybudzeniu
// zegarem → ekran dnia.
static void testR_PRZ_02_ekranPoStarcie() {
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::START, Ekran::K1, true, 10, mdoby(10, 0)) == Ekran::DZIEN);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::START, Ekran::TEST, false, 0, 12 * 60) == Ekran::DZIEN);
  // K1 po 5 min: wraca; po 4 min 59 s: zostaje
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::K1, true, 300, mdoby(10, 17)) == Ekran::DZIEN);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::K1, true, 299, mdoby(10, 16)) == Ekran::K1);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::K2, true, 1000, mdoby(14, 0)) == Ekran::DZIEN);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::PYTANIE, true, 300, mdoby(10, 0)) == Ekran::DZIEN);
  // bez zegara: bez powrotu
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::K1, false, 1000, 12 * 60) == Ekran::K1);
  // przycisk: ekran z nawigacji, bez powrotu w tym miejscu
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::PRZYCISK, Ekran::K1, true, 1000, mdoby(10, 0)) == Ekran::K1);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::PRZYCISK, Ekran::TEST, true, 1000, mdoby(10, 0)) == Ekran::TEST);
  // ekran testu po zegarze zawsze do ekranu dnia
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::TEST, true, 10, mdoby(10, 0)) == Ekran::DZIEN);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::TEST, false, 10, 12 * 60) == Ekran::DZIEN);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::TEST, true, 10, mdoby(2, 0)) == Ekran::DZIEN);
}

static void testR_PRZ_18_bezPowrotuWNocy() {
  // slot nocny 23:30–05:00: K1 i K2 zostają mimo upływu 5 min
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::K1, true, 1000, mdoby(2, 0)) == Ekran::K1);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::K2, true, 1000, mdoby(23, 30)) == Ekran::K2);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::K1, true, 1000, mdoby(4, 59)) == Ekran::K1);
  // pierwsze odświeżenie o 05:00 wraca (licznik dawno przekroczony)
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::K1, true, 20000, mdoby(5, 0)) == Ekran::DZIEN);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::K2, true, 1000, mdoby(23, 29)) == Ekran::DZIEN);
}

// R-KON-37 i U1: w slocie nocnym bez błysku e-ink. Zegar poza terminem przerysowania: według decyzji nocy;
// przycisk bez zmiany w slocie nocnym: bez rysowania; w pętli czuwania rysunek przy zmianie, bez zegara albo poza
// slotem nocnym.
static void testR_KON_37_rysowanieWNocy() {
  // start zasilania rysuje zawsze
  SPRAWDZ(rysowacPoWybudzeniu(PowodWybudzenia::START, false, true, mdoby(2, 0), false));
  // zegar w dzień: tak; w nocy tylko pierwsza decyzja S9
  SPRAWDZ(rysowacPoWybudzeniu(PowodWybudzenia::ZEGAR, false, true, mdoby(10, 0), false));
  SPRAWDZ(!rysowacPoWybudzeniu(PowodWybudzenia::ZEGAR, false, true, mdoby(2, 0), false));
  SPRAWDZ(rysowacPoWybudzeniu(PowodWybudzenia::ZEGAR, false, true, mdoby(23, 30), true));
  SPRAWDZ(!rysowacPoWybudzeniu(PowodWybudzenia::ZEGAR, false, true, mdoby(23, 30), false));
  SPRAWDZ(rysowacPoWybudzeniu(PowodWybudzenia::ZEGAR, false, true, mdoby(23, 29), false));
  SPRAWDZ(!rysowacPoWybudzeniu(PowodWybudzenia::ZEGAR, false, true, mdoby(4, 59), false));
  SPRAWDZ(rysowacPoWybudzeniu(PowodWybudzenia::ZEGAR, false, true, mdoby(5, 0), false));
  SPRAWDZ(rysowacPoWybudzeniu(PowodWybudzenia::ZEGAR, false, false, mdoby(2, 0), false));  // bez zegara: rysuje
  // przycisk: bez przerysowania tylko naciśnięcie bez skutku w nocy (np. przytrzymanie na S9)
  SPRAWDZ(!rysowacPoWybudzeniu(PowodWybudzenia::PRZYCISK, false, true, mdoby(2, 0), false));
  SPRAWDZ(rysowacPoWybudzeniu(PowodWybudzenia::PRZYCISK, true, true, mdoby(2, 0), false));
  SPRAWDZ(rysowacPoWybudzeniu(PowodWybudzenia::PRZYCISK, false, true, mdoby(10, 0), false));
  SPRAWDZ(rysowacPoWybudzeniu(PowodWybudzenia::PRZYCISK, false, true, mdoby(2, 0), true));
  SPRAWDZ(rysowacPoWybudzeniu(PowodWybudzenia::PRZYCISK, false, false, mdoby(2, 0), false));
  // naciśnięcie w czuwaniu
  SPRAWDZ(!rysowacPoNacisnieciu(false, true, mdoby(2, 0)));
  SPRAWDZ(rysowacPoNacisnieciu(true, true, mdoby(2, 0)));
  SPRAWDZ(rysowacPoNacisnieciu(false, false, mdoby(2, 0)));
  SPRAWDZ(rysowacPoNacisnieciu(false, true, mdoby(10, 0)));
  SPRAWDZ(rysowacPoNacisnieciu(false, true, mdoby(23, 29)));
  SPRAWDZ(!rysowacPoNacisnieciu(false, true, mdoby(23, 30)));
  SPRAWDZ(!rysowacPoNacisnieciu(false, true, mdoby(4, 59)));
  SPRAWDZ(rysowacPoNacisnieciu(false, true, mdoby(5, 0)));
}

// Okno serwisowe tylko na ekranie testu, a ten po zegarze wraca do ekranu dnia, więc OTA tylko po przycisku
// albo starcie.
static void testOknoSerwisoweTylkoPoPrzycisku() {
  SPRAWDZ(otworzycOknoSerwisowe(Ekran::TEST));
  SPRAWDZ(!otworzycOknoSerwisowe(Ekran::DZIEN));
  SPRAWDZ(!otworzycOknoSerwisowe(Ekran::K2));
  SPRAWDZ(!otworzycOknoSerwisowe(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::TEST, true, 5, mdoby(10, 0))));
  SPRAWDZ(otworzycOknoSerwisowe(ekranPoStarcie(PowodWybudzenia::PRZYCISK, Ekran::TEST, true, 5, mdoby(10, 0))));
  SPRAWDZ(czasCzuwaniaMs(true) == 600000);
  SPRAWDZ(czasCzuwaniaMs(false) == 20000);  // także po wybudzeniu zegarem (prototyp; krok 2 to zmienia)
}

// R-KON-35 przy pierwszej decyzji nocy: ekran dnia, znany wieczór, noc bez decyzji i ekran S5.
static void testEkranDniaTrwa() {
  StanNocy nowa;  // brak decyzji
  StanNocy juz{"2026-11-03", true};
  SPRAWDZ(ekranDniaTrwa(Ekran::DZIEN, "2026-11-03", nowa, true));
  SPRAWDZ(!ekranDniaTrwa(Ekran::DZIEN, "2026-11-03", nowa, false));
  SPRAWDZ(!ekranDniaTrwa(Ekran::DZIEN, "2026-11-03", juz, true));
  SPRAWDZ(!ekranDniaTrwa(Ekran::K1, "2026-11-03", nowa, true));
  SPRAWDZ(!ekranDniaTrwa(Ekran::DZIEN, "", nowa, true));
  SPRAWDZ(ekranDniaTrwa(Ekran::DZIEN, "2026-11-04", juz, true));
}

// R-PRZ-02, R-PRZ-18, T-WYM-29: czas snu:
//   s = OKRES_SNU_S; if (zegar) s = OKRES_SNU_S − teraz % OKRES_SNU_S + 5;
//   powrot = sekundDoPowrotu(ekran, teraz − ekranOd, minuta); if (zegar && powrot >= 0 && powrot + 2 < s)
//   s = powrot + 2; maska ext1 = przyciski w stanie HIGH (niewciśnięte).
static void testR_PRZ_02_czasSnu() {
  uint32_t t = ep(2026, 11, 4, 10, 12, 0);  // 09:12 UTC: 720 s po pełnej godzinie
  PlanSnu d = planSnu(true, t, Ekran::DZIEN, 0, mdoby(10, 12), 0);
  SPRAWDZ(d.budzZegarem);
  ROWNE_L(d.sekund, 1085);  // do 10:30:05
  SPRAWDZ(d.przyciski == WSZYSTKIE_PRZYCISKI);
  // K1 otwarty o 10:12:00 → sen do 10:17:02 (T-WYM-28, pierwszy przykład; zgodne z prototypem)
  ROWNE_L(planSnu(true, t, Ekran::K1, t, mdoby(10, 12), 0).sekund, 302);
  // K1 otwarty 29 min temu: powrót już (0 + 2 s)
  uint32_t t2 = ep(2026, 11, 4, 10, 29, 0);
  ROWNE_L(planSnu(true, t2, Ekran::K1, t2 - 29 * 60, mdoby(10, 29), 0).sekund, 2);
  // K1 otwarty 2 min temu o 10:28: powrót za 180 s, ale półgodzina za 125 s → 125
  uint32_t t3 = ep(2026, 11, 4, 10, 28, 0);
  ROWNE_L(planSnu(true, t3, Ekran::K1, t3 - 120, mdoby(10, 28), 0).sekund, 125);
  // równo o 10:30:00 → 1805
  ROWNE_L(planSnu(true, ep(2026, 11, 4, 10, 30, 0), Ekran::DZIEN, 0, mdoby(10, 30), 0).sekund, 1805);
  // bez zegara: 30 min równo, bez powrotu
  ROWNE_L(planSnu(false, 5000, Ekran::K1, 4990, 12 * 60, 0).sekund, 1800);
  ROWNE_L(planSnu(false, 5000, Ekran::DZIEN, 0, 12 * 60, 0).sekund, 1800);
}

static void testR_PRZ_18_snemWNocy() {
  // K1 otwarty o 02:12 w slocie nocnym: bez terminu powrotu, sen do 02:30:05 (prototyp; krok 2: 02:15 z R-DANE-09)
  uint32_t t = ep(2026, 11, 4, 2, 12, 0);
  ROWNE_L(planSnu(true, t, Ekran::K1, t, mdoby(2, 12), 0).sekund, 1085);
  ROWNE_L(planSnu(true, t, Ekran::K2, t - 3000, mdoby(2, 12), 0).sekund, 1085);
}

static void testT_WYM_29_zaciety() {
  // przycisk wciśnięty w chwili zasypiania nie budzi (U4)
  uint32_t t = ep(2026, 11, 4, 10, 12, 0);
  SPRAWDZ(planSnu(true, t, Ekran::DZIEN, 0, mdoby(10, 12), BIT_SRODEK).przyciski == (BIT_LEWY | BIT_PRAWY));
  SPRAWDZ(planSnu(true, t, Ekran::DZIEN, 0, mdoby(10, 12), BIT_LEWY | BIT_PRAWY).przyciski == BIT_SRODEK);
  SPRAWDZ(planSnu(true, t, Ekran::DZIEN, 0, mdoby(10, 12), WSZYSTKIE_PRZYCISKI).przyciski == 0);
  SPRAWDZ(planSnu(true, t, Ekran::DZIEN, 0, mdoby(10, 12), WSZYSTKIE_PRZYCISKI).budzZegarem);
  // kolejność LEWY, SRODEK, PRAWY; puszczony zdejmuje bit zaciętego; zacięty pominięty
  uint8_t z = 0;
  SPRAWDZ(wcisnietyPrzycisk(0, z) == Przycisk::BRAK && z == 0);
  SPRAWDZ(wcisnietyPrzycisk(BIT_SRODEK, z) == Przycisk::SRODEK && z == 0);
  z = BIT_SRODEK;
  SPRAWDZ(wcisnietyPrzycisk(BIT_SRODEK, z) == Przycisk::BRAK && z == BIT_SRODEK);
  SPRAWDZ(wcisnietyPrzycisk(0, z) == Przycisk::BRAK && z == 0);  // puszczony: znowu działa
  z = BIT_LEWY;
  SPRAWDZ(wcisnietyPrzycisk(BIT_LEWY | BIT_PRAWY, z) == Przycisk::PRAWY && z == BIT_LEWY);
  // wynik przerywa sprawdzanie: bity zaciętych dalszych przycisków zostają, nawet gdy są puszczone
  z = BIT_SRODEK | BIT_PRAWY;
  SPRAWDZ(wcisnietyPrzycisk(BIT_LEWY, z) == Przycisk::LEWY && z == (BIT_SRODEK | BIT_PRAWY));
  // po 10 s nadal wciśnięty → zacięty
  SPRAWDZ(ZACIECIE_MS == 10000);
  SPRAWDZ(zacietePoCzekaniu(Przycisk::SRODEK, 0) == BIT_SRODEK);
  SPRAWDZ(zacietePoCzekaniu(Przycisk::PRAWY, BIT_LEWY) == (BIT_LEWY | BIT_PRAWY));
  SPRAWDZ(zacietePoCzekaniu(Przycisk::BRAK, BIT_LEWY) == BIT_LEWY);
}

// T-WYM-21: przycisk budzący z maski ext1 i przytrzymanie od startu programu.
static void testT_WYM_21_przyciskBudzacy() {
  SPRAWDZ(przyciskBudzacy(BIT_LEWY) == Przycisk::LEWY);
  SPRAWDZ(przyciskBudzacy(BIT_LEWY | BIT_PRAWY) == Przycisk::LEWY);
  SPRAWDZ(przyciskBudzacy(BIT_SRODEK | BIT_PRAWY) == Przycisk::SRODEK);
  SPRAWDZ(przyciskBudzacy(BIT_PRAWY) == Przycisk::PRAWY);
  SPRAWDZ(przyciskBudzacy(0) == Przycisk::PRAWY);  // prototyp: domyślnie ▶
  // ◀ przytrzymany 2,5 s → przytrzymanie; puszczony po 0,5 s → krótkie (weryfikacja T-WYM-21)
  SPRAWDZ(przytrzymanie(0, 2500, true) == Przytrzymanie::DLUGO);
  SPRAWDZ(przytrzymanie(0, 500, false) == Przytrzymanie::KROTKO);
  SPRAWDZ(przytrzymanie(0, 1999, true) == Przytrzymanie::TRWA);
  SPRAWDZ(przytrzymanie(0, 2000, true) == Przytrzymanie::DLUGO);
  SPRAWDZ(przytrzymanie(1000, 2999, true) == Przytrzymanie::TRWA);
  SPRAWDZ(przytrzymanie(1000, 3000, true) == Przytrzymanie::DLUGO);
  SPRAWDZ(przytrzymanie(1000, 3000, false) == Przytrzymanie::KROTKO);
  SPRAWDZ(PROG_PRZYTRZYMANIA_MS == 2000);
}

// Decyzje po `/api/status`.
static void testPoStatusie() {
  StatusAB st;
  SPRAWDZ(poStatusie(401, "", st) == PoStatusie::BRAK_ODPOWIEDZI);
  SPRAWDZ(poStatusie(-1, "", st) == PoStatusie::BRAK_ODPOWIEDZI);
  SPRAWDZ(poStatusie(500, "{}", st) == PoStatusie::BRAK_ODPOWIEDZI);
  SPRAWDZ(poStatusie(AB_KOD_URWANE, "", st) == PoStatusie::BRAK_ODPOWIEDZI);
  SPRAWDZ(poStatusie(200, "nie json", st) == PoStatusie::E03);
  SPRAWDZ(poStatusie(200, "{\"version\":\"v2.1.9\",\"rop\":0}", st) == PoStatusie::E03);  // wersja spoza zakresu
  SPRAWDZ(poStatusie(200, "{\"version\":\"v2.2.1-25-gebbd145\",\"rop\":1,\"report_state\":\"blocked\"}", st) ==
          PoStatusie::ZABLOKOWANE);
  ROWNE(st.wersja, "v2.2.1-25-gebbd145");
  SPRAWDZ(poStatusie(200, "{\"version\":\"v2.2.1\",\"rop\":0,\"report_state\":\"blocked\"}", st) ==
          PoStatusie::ZABLOKOWANE);
  SPRAWDZ(poStatusie(200, "{\"version\":\"v2.2.1\",\"rop\":0,\"edf\":\"post-processing\"}", st) ==
          PoStatusie::PRZETWARZA);
  SPRAWDZ(poStatusie(200, "{\"version\":\"v2.3.0\",\"rop\":0,\"report_state\":\"storage_busy\"}", st) ==
          PoStatusie::PRZETWARZA);
  // blokada przed przetwarzaniem (kolejność sprawdzeń)
  SPRAWDZ(poStatusie(200, "{\"version\":\"v2.2.1\",\"rop\":1,\"edf\":\"post-processing\"}", st) ==
          PoStatusie::ZABLOKOWANE);
  SPRAWDZ(poStatusie(200, "{\"version\":\"v2.2.1\",\"rop\":0,\"report_state\":\"idle\"}", st) == PoStatusie::DALEJ);
  // wersja nieczytelna: dalej (o E-03 przesądzi raport)
  SPRAWDZ(poStatusie(200, "{\"version\":\"dev\",\"rop\":0}", st) == PoStatusie::DALEJ);
  SPRAWDZ(poStatusie(200, "{}", st) == PoStatusie::DALEJ);
}

// Noc „w budowie”: data znana, przed 12:00 i brak nocy z dziś przy blokadzie; data znana i brak nocy przy
// przetwarzaniu. Granica 12:00 to prototyp (13.2; R-DANE-35
// ma 23:30, zmiana w kroku 4).
static void testGranicaWBudowie12() {
  SPRAWDZ(wBudowiePrzyBlokadzie(true, mdoby(11, 59), false));
  SPRAWDZ(!wBudowiePrzyBlokadzie(true, mdoby(12, 0), false));
  SPRAWDZ(!wBudowiePrzyBlokadzie(true, mdoby(6, 0), true));
  SPRAWDZ(!wBudowiePrzyBlokadzie(false, mdoby(6, 0), false));
  SPRAWDZ(wBudowiePrzyBlokadzie(true, 0, false));
  SPRAWDZ(wBudowiePrzyPrzetwarzaniu(true, false));
  SPRAWDZ(!wBudowiePrzyPrzetwarzaniu(true, true));
  SPRAWDZ(!wBudowiePrzyPrzetwarzaniu(false, false));
}

// Po liście i dobie 200 z ciałem nieczytelnym to AB_KOD_URWANE, inne kody bez zmian; koniec odpytywania joba,
// gdy następne odpytanie nie mieści się w limicie raportu albo w budżecie wybudzenia.
static void testKodyIOdpytywanie() {
  SPRAWDZ(kodPoNieudanej(200) == AB_KOD_URWANE);
  SPRAWDZ(kodPoNieudanej(404) == 404);
  SPRAWDZ(kodPoNieudanej(-1) == -1);
  SPRAWDZ(kodPoNieudanej(KOD_E03) == KOD_E03);
  SPRAWDZ(!koniecOdpytywania(0, AB_LIMIT_JOBA_MS, 120000));
  SPRAWDZ(!koniecOdpytywania(114999, AB_LIMIT_JOBA_MS, 100000));
  SPRAWDZ(koniecOdpytywania(115000, AB_LIMIT_JOBA_MS, 100000));
  SPRAWDZ(koniecOdpytywania(0, AB_LIMIT_JOBA_MS, 5000));
  SPRAWDZ(!koniecOdpytywania(0, AB_LIMIT_JOBA_MS, 5001));
  SPRAWDZ(koniecOdpytywania(0, AB_LIMIT_JOBA_MS, -10));
}

// Zaległe doby tylko po zegarze albo starcie, poza ekranem testu i poza slotem nocnym, tylko po
// udanym odczycie i liście, bez blokady raportów, z działającymi plikami i Wi-Fi, przy znanej dacie.
static void testZalegleDobyPozaSlotemNocnym() {
  WarunkiZaleglych w;
  w.powod = PowodWybudzenia::ZEGAR;
  w.ekran = Ekran::DZIEN;
  w.odczytUdany = w.listaOk = w.plikiOk = w.wifi = w.dataZnana = true;
  w.zablokowane = false;
  w.minuta = mdoby(10, 0);
  SPRAWDZ(dociagacZalegle(w));
  WarunkiZaleglych x = w;
  x.powod = PowodWybudzenia::START;
  SPRAWDZ(dociagacZalegle(x));
  x = w; x.powod = PowodWybudzenia::PRZYCISK; SPRAWDZ(!dociagacZalegle(x));
  x = w; x.ekran = Ekran::TEST; SPRAWDZ(!dociagacZalegle(x));
  x = w; x.ekran = Ekran::K1; SPRAWDZ(dociagacZalegle(x));
  x = w; x.odczytUdany = false; SPRAWDZ(!dociagacZalegle(x));
  x = w; x.listaOk = false; SPRAWDZ(!dociagacZalegle(x));
  x = w; x.zablokowane = true; SPRAWDZ(!dociagacZalegle(x));
  x = w; x.plikiOk = false; SPRAWDZ(!dociagacZalegle(x));
  x = w; x.wifi = false; SPRAWDZ(!dociagacZalegle(x));
  x = w; x.dataZnana = false; SPRAWDZ(!dociagacZalegle(x));
  x = w; x.minuta = mdoby(23, 30); SPRAWDZ(!dociagacZalegle(x));
  x = w; x.minuta = mdoby(2, 0); SPRAWDZ(!dociagacZalegle(x));
  x = w; x.minuta = mdoby(23, 29); SPRAWDZ(dociagacZalegle(x));
  x = w; x.minuta = mdoby(5, 0); SPRAWDZ(dociagacZalegle(x));
  // plan: bez doby z dziś, najwyżej 4
  long d = nrDnia("2026-11-03");  // dataPoranka(d) = 2026-11-04
  std::string dzis = dataPoranka(d);
  ROWNE(dzis, "2026-11-04");
  std::vector<long> plan = {d, d - 1, d - 2, d - 3, d - 4, d - 5};
  std::vector<long> wynik = planZaleglych(plan, dzis);
  SPRAWDZ(wynik == (std::vector<long>{d - 1, d - 2, d - 3, d - 4}));
  SPRAWDZ(planZaleglych({d - 1, d - 2}, dzis) == (std::vector<long>{d - 1, d - 2}));
  SPRAWDZ(planZaleglych({d}, dzis).empty());
  SPRAWDZ(planZaleglych({}, dzis).empty());
  SPRAWDZ(zaczacRaportZaleglej(20000));
  SPRAWDZ(!zaczacRaportZaleglej(19999));
}

// Początek błędu: udany odczyt zeruje, pierwszy nieudany przy znanym zegarze ustawia; S5 kartą poza KOD_WIFI,
// KOD_E03 i 401; terapia tylko z udanego odczytu i `rop` = 1; ziarno z NVS albo losowe nieparzyste.
static void testDrobneDecyzje() {
  uint32_t t = ep(2026, 11, 4, 7, 0);
  SPRAWDZ(bladOdPoOdczycie(true, 123, true, t) == 0);
  SPRAWDZ(bladOdPoOdczycie(false, 0, true, t) == t);
  SPRAWDZ(bladOdPoOdczycie(false, 999, true, t) == 999);
  SPRAWDZ(bladOdPoOdczycie(false, 0, false, 5) == 0);
  SPRAWDZ(!kartaBraku(KOD_WIFI));
  SPRAWDZ(!kartaBraku(KOD_E03));
  SPRAWDZ(!kartaBraku(401));
  SPRAWDZ(kartaBraku(200));
  SPRAWDZ(kartaBraku(-1));
  SPRAWDZ(kartaBraku(500));
  SPRAWDZ(kartaBraku(AB_KOD_URWANE));
  SPRAWDZ(terapiaTrwa(true, 1));
  SPRAWDZ(!terapiaTrwa(false, 1));
  SPRAWDZ(!terapiaTrwa(true, 0));
  SPRAWDZ(ziarnoPanelu(77, 5) == 77);
  SPRAWDZ(ziarnoPanelu(0, 4) == 5);
  SPRAWDZ(ziarnoPanelu(0, 0) == 1);
}

// Budżety sterty i stosu w logu wybudzenia (progi [PRÓG] 64 KB i 4 KB).
static void testLogPamieci() {
  SPRAWDZ(PROG_STERTY_B == 65536 && PROG_STOSU_B == 4096);
  SPRAWDZ(pamiecWBudzecie({65536, 4096}));
  SPRAWDZ(!pamiecWBudzecie({65535, 4096}));
  SPRAWDZ(!pamiecWBudzecie({70000, 4095}));
  ROWNE(wierszPamieci({70000, 5000}), "pamiec sterta_min=70000 stos_zapas=5000");
  ROWNE(wierszPamieci({60000, 5000}), "pamiec sterta_min=60000 stos_zapas=5000 poza_budzetem");
  ROWNE(wierszPamieci({70000, 1000}), "pamiec sterta_min=70000 stos_zapas=1000 poza_budzetem");
}

// ============================================================================================================
// (b) Siatka aktualizacji
// ============================================================================================================

static const uint32_t W1 = identyfikatorWersji("proto-1.0");
static const uint32_t W2 = identyfikatorWersji("proto-1.1");

// Licznik w stanie po włączeniu zasilania: przypadkowa zawartość pamięci RTC.
static LicznikStartow smieci() {
  LicznikStartow l;
  memset(static_cast<void*>(&l), 0xA5, sizeof l);
  return l;
}

static void testT_WYM_82_identyfikatorWersji() {
  SPRAWDZ(W1 != 0 && W2 != 0);
  SPRAWDZ(W1 != W2);
  SPRAWDZ(identyfikatorWersji("proto-1.0") == W1);
}

// T-WYM-82, R-AW-39: licznik startów po nieprawidłowym zakończeniu (weryfikacja T-WYM-82, T-TEST-272).
static void testT_WYM_82_licznikStartow() {
  ZnacznikAwaryjny brak;
  // włączenie zasilania, potem 3 starty z rzędu po błędzie programu bez zaśnięcia → przy 3. tryb awaryjny
  LicznikStartow l = smieci();
  DecyzjaStartu d = decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 0);
  d = decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 1 && !d.zapiszZnacznik);
  d = decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 2 && !d.zapiszZnacznik);
  d = decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  SPRAWDZ(d.awaryjny && d.poBledzie == 3);
  SPRAWDZ(d.zapiszZnacznik && d.znacznik.jest && d.znacznik.wersja == W1);
  SPRAWDZ(d.znacznik.przyczyna == PrzyczynaStartu::PANIKA && d.znacznik.u06Narysowane);
  SPRAWDZ(d.rysujU06 && !d.usunZnacznik);

  // wszystkie cztery rodzaje strażnika i błędu liczą się tak samo
  l = smieci();
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  SPRAWDZ(!decyzjaStartu(l, PrzyczynaStartu::TASK_WDT, W1, brak).awaryjny);
  SPRAWDZ(!decyzjaStartu(l, PrzyczynaStartu::INT_WDT, W1, brak).awaryjny);
  d = decyzjaStartu(l, PrzyczynaStartu::WDT, W1, brak);
  SPRAWDZ(d.awaryjny && d.poBledzie == 3);

  // 2 starty po strażniku, zaśnięcie, 2 kolejne → bez trybu awaryjnego
  l = smieci();
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::TASK_WDT, W1, brak);
  SPRAWDZ(decyzjaStartu(l, PrzyczynaStartu::TASK_WDT, W1, brak).poBledzie == 2);
  zerujPrzedSnem(l, W1);
  d = decyzjaStartu(l, PrzyczynaStartu::USPIENIE, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 0);
  decyzjaStartu(l, PrzyczynaStartu::TASK_WDT, W1, brak);
  d = decyzjaStartu(l, PrzyczynaStartu::TASK_WDT, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 2);

  // nieudana aktualizacja: restart po wgraniu i jawny powrót (ESP_RST_SW dwa razy) → licznik bez zmian
  l = smieci();
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::PROGRAMOWY, W1, brak);
  d = decyzjaStartu(l, PrzyczynaStartu::PROGRAMOWY, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 1);

  // 5 startów po spadku napięcia → licznik bez zmian, licznik spadków 5
  l = smieci();
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  for (int i = 0; i < 4; i++) decyzjaStartu(l, PrzyczynaStartu::SPADEK, W1, brak);
  d = decyzjaStartu(l, PrzyczynaStartu::SPADEK, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 0 && d.spadki == 5 && d.spadekNapiecia);
  // spadki napięcia przerywające serię błędów nie wpychają w tryb awaryjny
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::SPADEK, W1, brak);
  d = decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 2 && !d.spadekNapiecia);

  // 10 wybudzeń z uśpienia → licznik 0
  l = smieci();
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  for (int i = 0; i < 10; i++) {
    zerujPrzedSnem(l, W1);
    d = decyzjaStartu(l, PrzyczynaStartu::USPIENIE, W1, brak);
  }
  SPRAWDZ(!d.awaryjny && d.poBledzie == 0);

  // pamięć RTC z magią innej wersji firmware → licznik 0
  l = smieci();
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  d = decyzjaStartu(l, PrzyczynaStartu::PROGRAMOWY, W2, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 0);

  // włączenie zasilania zeruje licznik także przy przypadkowo ważnej zawartości
  l = smieci();
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  d = decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 0);
  d = decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 1);

  // uszkodzona suma kontrolna (jedno pole zmienione bez sumy) → licznik nieważny, od zera
  l = smieci();
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  l.poBledzie = 7;
  d = decyzjaStartu(l, PrzyczynaStartu::PROGRAMOWY, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 0);

  // zerowanie tuż przed snem także po wejściu w tryb awaryjny (tryb trzyma znacznik w NVS, nie licznik)
  l = smieci();
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  for (int i = 0; i < 3; i++) d = decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  SPRAWDZ(d.awaryjny);
  zerujPrzedSnem(l, W1);
  d = decyzjaStartu(l, PrzyczynaStartu::USPIENIE, W1, brak);
  SPRAWDZ(d.poBledzie == 0);
}

// T-WYM-83, R-AW-39: znacznik trybu awaryjnego w NVS, wyjście przy innej wersji i po ↻ 2 s na U-06.
static void testT_WYM_83_znacznik() {
  ZnacznikAwaryjny z;
  z.jest = true;
  z.wersja = 0xDEADBEEF;
  z.przyczyna = PrzyczynaStartu::TASK_WDT;
  z.u06Narysowane = true;
  ZnacznikAwaryjny w;
  SPRAWDZ(parsujZnacznik(zapiszZnacznik(z), w));
  SPRAWDZ(w.jest && w.wersja == 0xDEADBEEF && w.przyczyna == PrzyczynaStartu::TASK_WDT && w.u06Narysowane);
  SPRAWDZ(!parsujZnacznik("", w) && !w.jest);
  SPRAWDZ(!parsujZnacznik("śmieci;;", w) && !w.jest);
  // po czwartym polu koniec tekstu, nic więcej
  SPRAWDZ(parsujZnacznik("v1;deadbeef;1;1", w) && w.jest);
  SPRAWDZ(!parsujZnacznik("v1;deadbeef;1;1;", w) && !w.jest);
  SPRAWDZ(!parsujZnacznik("v1;deadbeef;1;1;smieci", w) && !w.jest);
  SPRAWDZ(!parsujZnacznik("v1;deadbeef;1;1;a;b", w) && !w.jest);
  SPRAWDZ(!parsujZnacznik("v1;deadbeef;1", w) && !w.jest);

  ZnacznikAwaryjny tenSam;
  tenSam.jest = true;
  tenSam.wersja = W1;
  tenSam.przyczyna = PrzyczynaStartu::PANIKA;
  tenSam.u06Narysowane = true;
  // zanik zasilania w trybie awaryjnym → po starcie znowu tryb awaryjny (znacznik w NVS), bez drugiego zapisu
  LicznikStartow l = smieci();
  DecyzjaStartu d = decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, tenSam);
  SPRAWDZ(d.awaryjny && !d.zapiszZnacznik && !d.usunZnacznik);
  // wybudzenie przyciskiem w trybie awaryjnym → dalej tryb awaryjny
  zerujPrzedSnem(l, W1);
  d = decyzjaStartu(l, PrzyczynaStartu::USPIENIE, W1, tenSam);
  SPRAWDZ(d.awaryjny && !d.zapiszZnacznik);
  // błąd programu w trakcie rysowania U-06 (znacznik „narysowane”) → bez rysowania, od razu sen
  d = decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, tenSam);
  SPRAWDZ(d.awaryjny && !d.rysujU06);
  d = decyzjaStartu(l, PrzyczynaStartu::TASK_WDT, W1, tenSam);
  SPRAWDZ(d.awaryjny && !d.rysujU06);
  // start innej wersji firmware → znacznik usunięty, zwykły start z licznikiem 0
  d = decyzjaStartu(l, PrzyczynaStartu::PROGRAMOWY, W2, tenSam);
  SPRAWDZ(!d.awaryjny && d.usunZnacznik && d.poBledzie == 0);

  // mapa przycisków U-06 i U-05 w trybie awaryjnym (spec funkcjonalna 5.6.2)
  SPRAWDZ(nacisniecieAwaryjne(EkranAwaryjny::U06, Przycisk::PRAWY, false) == AkcjaAwaryjna::TEST_SPRZETU);
  SPRAWDZ(nacisniecieAwaryjne(EkranAwaryjny::U06, Przycisk::SRODEK, true) == AkcjaAwaryjna::WYJDZ);
  SPRAWDZ(nacisniecieAwaryjne(EkranAwaryjny::U06, Przycisk::SRODEK, false) == AkcjaAwaryjna::NIC);
  SPRAWDZ(nacisniecieAwaryjne(EkranAwaryjny::U06, Przycisk::LEWY, false) == AkcjaAwaryjna::NIC);
  SPRAWDZ(nacisniecieAwaryjne(EkranAwaryjny::U06, Przycisk::LEWY, true) == AkcjaAwaryjna::NIC);
  SPRAWDZ(nacisniecieAwaryjne(EkranAwaryjny::U05, Przycisk::LEWY, false) == AkcjaAwaryjna::WROC);
  SPRAWDZ(nacisniecieAwaryjne(EkranAwaryjny::U05, Przycisk::SRODEK, true) == AkcjaAwaryjna::NIC);  // bez pakietu
  SPRAWDZ(nacisniecieAwaryjne(EkranAwaryjny::U05, Przycisk::PRAWY, false) == AkcjaAwaryjna::NIC);

  // ↻ 2 s na U-06 → zwykły start z licznikiem 0; przy tym samym błędzie po 3 startach znowu tryb awaryjny
  ZnacznikAwaryjny brak;
  l = smieci();
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, W1, brak);
  for (int i = 0; i < 3; i++) d = decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  SPRAWDZ(d.awaryjny);
  wyjscieZAwaryjnego(l, W1);
  d = decyzjaStartu(l, PrzyczynaStartu::PROGRAMOWY, W1, brak);
  SPRAWDZ(!d.awaryjny && d.poBledzie == 0);
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  d = decyzjaStartu(l, PrzyczynaStartu::PANIKA, W1, brak);
  SPRAWDZ(d.awaryjny);
}

// R-AW-39: teksty U-06 wbudowane w firmware (spec funkcjonalna 6.2).
static void testR_AW_39_tekstyU06() {
  TekstyU06 t = tekstyU06();
  ROWNE(t.tytul, "Panel wymaga serwisu");
  ROWNE(t.powod, "Oprogramowanie panelu nie uruchamia się poprawnie. Nocy nie oceniam.");
  ROWNE(t.krok, "→ Daj znać osobie, która instalowała panel.");
  ROWNE(t.stopka, "▶ test sprzętu · ↻ 2 s spróbuj ponownie");
  SPRAWDZ(STARTOW_DO_AWARYJNEGO == 3);
}

// T-WYM-41: samosprawdzenie nowej wersji przed pierwszym snem, oznaczenie albo powrót.
static void testT_WYM_41_samosprawdzenie() {
  WynikSamosprawdzenia dobry;
  dobry.plikiMontuja = dobry.zestawWczytany = dobry.migracjeOk = dobry.ekranRysuje = dobry.wifiLaczy = true;
  SPRAWDZ(decyzjaSamosprawdzenia(StanWersji::DO_SPRAWDZENIA, dobry) == PoSamosprawdzeniu::OZNACZ_DOBRA);
  SPRAWDZ(decyzjaSamosprawdzenia(StanWersji::DOBRA, dobry) == PoSamosprawdzeniu::NIC);
  SPRAWDZ(decyzjaSamosprawdzenia(StanWersji::DOBRA, WynikSamosprawdzenia()) == PoSamosprawdzeniu::NIC);
  WynikSamosprawdzenia w = dobry;
  w.wifiLaczy = false;  // obraz testowy, który nie łączy się z Wi-Fi → powrót
  SPRAWDZ(decyzjaSamosprawdzenia(StanWersji::DO_SPRAWDZENIA, w) == PoSamosprawdzeniu::WROC_DO_POPRZEDNIEJ);
  w = dobry; w.plikiMontuja = false;
  SPRAWDZ(decyzjaSamosprawdzenia(StanWersji::DO_SPRAWDZENIA, w) == PoSamosprawdzeniu::WROC_DO_POPRZEDNIEJ);
  w = dobry; w.zestawWczytany = false;
  SPRAWDZ(decyzjaSamosprawdzenia(StanWersji::DO_SPRAWDZENIA, w) == PoSamosprawdzeniu::WROC_DO_POPRZEDNIEJ);
  w = dobry; w.migracjeOk = false;  // atrapa pamięci odmawia zapisu nowego pliku migracji
  SPRAWDZ(decyzjaSamosprawdzenia(StanWersji::DO_SPRAWDZENIA, w) == PoSamosprawdzeniu::WROC_DO_POPRZEDNIEJ);
  w = dobry; w.ekranRysuje = false;
  SPRAWDZ(decyzjaSamosprawdzenia(StanWersji::DO_SPRAWDZENIA, w) == PoSamosprawdzeniu::WROC_DO_POPRZEDNIEJ);
  SPRAWDZ(decyzjaSamosprawdzenia(StanWersji::DO_SPRAWDZENIA, WynikSamosprawdzenia()) ==
          PoSamosprawdzeniu::WROC_DO_POPRZEDNIEJ);
  // wersja niesprawdzona nie zasypia (7.8: po wybudzeniu z uśpienia bootloader nie wraca sam)
  SPRAWDZ(!wolnoZasnac(StanWersji::DO_SPRAWDZENIA));
  SPRAWDZ(wolnoZasnac(StanWersji::DOBRA));
  SPRAWDZ(samosprawdzenieWymagane(StanWersji::DO_SPRAWDZENIA));
  SPRAWDZ(!samosprawdzenieWymagane(StanWersji::DOBRA));
}

// T-WYM-81: strażnik zadania 150 s, przekroczenie kończy się błędem programu i restartem.
static void testT_WYM_81_ustawieniaStraznika() {
  UstawieniaStraznika u = ustawieniaStraznika();
  SPRAWDZ(u.limitS == 150 && u.panika);
  SPRAWDZ(u.limitS * 1000 > AB_LIMIT_JOBA_MS);  // dłużej niż najdłuższy krok 7.1 (120 s AirBridge)
}

// ============================================================================================================
// (c) Dyrygent z atrapami adapterów
// ============================================================================================================

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", ""};
static const char* STATUS_TERAPIA =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"system\":\"THERAPY\",\"rop\":1,"
    "\"edf\":\"recording\",\"edf_prefix\":\"20261104_000000\",\"report_state\":\"blocked\",\"report_error\":\"not_idle\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"system\":\"IDLE\",\"rop\":0,"
    "\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";

// Zwykłe wybudzenie zegarem o 07:00 4.11.2026, AirBridge w terapii (bez listy i raportów), licznik po uśpieniu.
static void zwyklyPanel(Panel& p) {
  p.zegar.epoch = ep(2026, 11, 4, 7, 0);
  p.siec.czasNtp = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.siec.dodaj("/api/status", 200, STATUS_TERAPIA);
}

// Liczba zdarzeń z przedrostkiem `p` w dzienniku przed indeksem `do_`.
static int przed(const Dziennik& d, const std::string& p, int do_) {
  int n = 0;
  for (int i = 0; i < do_ && i < (int)d.z.size(); i++)
    if (d.z[(size_t)i].compare(0, p.size(), p) == 0) n++;
  return n;
}

// T-WYM-81: strażnik zapisany od początku, przed każdym innym adapterem, z limitem 150 s i restartem.
static void testT_WYM_81_straznikNaPoczatku() {
  Panel p;
  zwyklyPanel(p);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.sen.snow == 1);
  SPRAWDZ(p.straznik.zapisany && p.straznik.limitS == 150 && p.straznik.panika);
  SPRAWDZ(p.d.pierwsze("straznik.zapisz") == 0);
  SPRAWDZ(p.straznik.odnowien >= 3);  // granice kroków 7.1: co najmniej po pobraniu, rysowaniu i czuwaniu
  SPRAWDZ(p.straznik.przekroczen == 0);
}

// T-WYM-81: odnowienie w każdym obiegu pętli odpytywania joba (AirBridge buduje raport przez cały limit 120 s).
static void testT_WYM_81_odnowienieWPetliJoba() {
  Panel p;
  zwyklyPanel(p);
  p.siec.odpowiedzi.clear();
  p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  p.siec.dodaj("/api/report", 202, "{\"job\":\"j1\",\"pending\":true}");
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.sen.snow == 1);
  int zadan = p.d.ile("siec.get");
  SPRAWDZ(zadan >= 10);  // status, lista i odpytywanie joba co 5 s
  // między każdymi dwoma żądaniami co najmniej jedno odnowienie strażnika
  int ostatnieGet = -1;
  bool wszystkie = zadan > 0;
  for (int i = 0; i < (int)p.d.z.size(); i++) {
    if (p.d.z[(size_t)i].compare(0, 8, "siec.get") != 0) continue;
    if (ostatnieGet >= 0 && przed(p.d, "straznik.odnow", i) - przed(p.d, "straznik.odnow", ostatnieGet) < 1)
      wszystkie = false;
    ostatnieGet = i;
  }
  SPRAWDZ(wszystkie);
  SPRAWDZ(p.straznik.przekroczen == 0);
  SPRAWDZ(p.straznik.najdluzszaPrzerwaMs < 150000);
}

// T-WYM-81, T-TEST-272: wywołanie, które nie wraca w limicie (atrapa HTTP 200 s) → przekroczenie strażnika.
static void testT_WYM_81_wywolanieBezPowrotu() {
  Panel p;
  zwyklyPanel(p);
  p.siec.odpowiedzi.clear();
  p.siec.dodaj("/api/status", 200, STATUS_TERAPIA, 200000);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.straznik.zapisany);
  SPRAWDZ(p.straznik.przekroczen >= 1);
}

// T-WYM-82: licznik zerowany tuż przed snem, start po błędzie bez progu R-AW-39 to zwykłe wybudzenie.
static void testT_WYM_82_dyrygentLicznik() {
  uint32_t w = identyfikatorWersji(WERSJA_ATRAPY);
  ZnacznikAwaryjny brak;
  {
    Panel p;
    zwyklyPanel(p);
    p.start.l = smieci();
    decyzjaStartu(p.start.l, PrzyczynaStartu::ZASILANIE, w, brak);
    decyzjaStartu(p.start.l, PrzyczynaStartu::PANIKA, w, brak);  // 1 start po błędzie
    p.start.przyczynaStartu = PrzyczynaStartu::PANIKA;          // ten start: 2.
    p.start.powodWybudzenia = PowodWybudzenia::START;
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.sen.snow == 1);
    SPRAWDZ(p.sen.plan.budzZegarem);  // zwykłe wybudzenie
    SPRAWDZ(p.siec.zadania.size() >= 1);
    SPRAWDZ(p.sen.licznikPrzySnie == 0);
    SPRAWDZ(p.nvs.teksty.count("cpap/awaryjny") == 0);
  }
  {
    // restart po wgraniu (ESP_RST_SW) przy liczniku 2: bez zmian, zwykłe wybudzenie, przed snem 0
    Panel p;
    zwyklyPanel(p);
    p.start.l = smieci();
    decyzjaStartu(p.start.l, PrzyczynaStartu::ZASILANIE, w, brak);
    decyzjaStartu(p.start.l, PrzyczynaStartu::PANIKA, w, brak);
    decyzjaStartu(p.start.l, PrzyczynaStartu::PANIKA, w, brak);
    p.start.przyczynaStartu = PrzyczynaStartu::PROGRAMOWY;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.sen.snow == 1 && p.sen.plan.budzZegarem);
    SPRAWDZ(p.sen.licznikPrzySnie == 0);
  }
}

// T-WYM-83, R-AW-39, T-TEST-273 (zakres PC): tryb awaryjny bez zapisu danych i bez sieci.
static void testT_WYM_83_trybAwaryjny() {
  uint32_t w = identyfikatorWersji(WERSJA_ATRAPY);
  ZnacznikAwaryjny brak;
  Panel p;
  zwyklyPanel(p);
  p.start.l = smieci();
  decyzjaStartu(p.start.l, PrzyczynaStartu::ZASILANIE, w, brak);
  decyzjaStartu(p.start.l, PrzyczynaStartu::PANIKA, w, brak);
  decyzjaStartu(p.start.l, PrzyczynaStartu::PANIKA, w, brak);
  p.start.przyczynaStartu = PrzyczynaStartu::PANIKA;  // 3. start po błędzie
  p.start.powodWybudzenia = PowodWybudzenia::START;
  p.pliki.pliki["/noce.csv"] = "2026-11-02;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;\n";
  p.pliki.pliki["/humor.csv"] = "2026-11-03;S2A;;D;;\n";
  auto przedPliki = p.pliki.pliki;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.sen.snow == 1);
  SPRAWDZ(p.pliki.zapisy == 0);
  SPRAWDZ(p.pliki.pliki == przedPliki);  // pliki danych bajt w bajt jak przed
  SPRAWDZ(p.siec.wywolania == 0);
  SPRAWDZ(p.ekran.odswiezen == 1);  // U-06 raz
  SPRAWDZ(!p.sen.plan.budzZegarem);
  SPRAWDZ(p.sen.plan.przyciski == WSZYSTKIE_PRZYCISKI);
  SPRAWDZ(p.sen.licznikPrzySnie == 0);
  ZnacznikAwaryjny z;
  SPRAWDZ(p.nvs.teksty.count("cpap/awaryjny") == 1);
  SPRAWDZ(parsujZnacznik(p.nvs.teksty["cpap/awaryjny"], z) && z.jest && z.wersja == w && z.u06Narysowane);
  // znacznik zapisany przed rysowaniem U-06
  SPRAWDZ(p.d.pierwsze("nvs.zapis cpap/awaryjny") >= 0 &&
          p.d.pierwsze("nvs.zapis cpap/awaryjny") < p.d.pierwsze("ekran.odswiez"));

  // restart zasilania w trybie awaryjnym → dalej bez sieci i bez zapisu plików, sen bez zegara
  p.d.z.clear();
  p.sen.snow = 0;
  p.siec.wywolania = 0;
  p.start.l = smieci();
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  p.zegar.msTeraz = 0;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.sen.snow == 1 && !p.sen.plan.budzZegarem);
  SPRAWDZ(p.siec.wywolania == 0 && p.pliki.zapisy == 0);

  // ▶ na U-06 → U-05 z oknem serwisowym: Wi-Fi tylko do okna, bez AirBridge, bez zapisu plików
  p.d.z.clear();
  p.sen.snow = 0;
  p.siec.zadania.clear();
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = BIT_PRAWY;
  p.zegar.msTeraz = 0;
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {200, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.d.ile("siec.okno") == 1);
  SPRAWDZ(p.siec.zadania.empty());
  SPRAWDZ(p.pliki.zapisy == 0);
  SPRAWDZ(p.sen.snow == 1 && !p.sen.plan.budzZegarem);

  // start innej wersji firmware → znacznik usunięty, zwykłe wybudzenie z AirBridge
  p.d.z.clear();
  p.sen.snow = 0;
  p.siec.zadania.clear();
  p.start.napisWersji = "proto-1.1";
  p.start.przyczynaStartu = PrzyczynaStartu::PROGRAMOWY;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  p.start.budzace = 0;
  p.przyciski.skrypt.clear();
  p.zegar.msTeraz = 0;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.nvs.teksty.count("cpap/awaryjny") == 0);
  SPRAWDZ(p.siec.zadania.size() >= 1);
  SPRAWDZ(p.sen.snow == 1 && p.sen.plan.budzZegarem);
}

// T-WYM-24: wybudzenie ▶ na ekranie dnia → K1 narysowany, bez Wi-Fi i bez żądań HTTP.
// Od 10.11.2026 (R-PRZ-22, szybszy wynik nocy rano) przycisk rano bez nocy dodatkowo pobiera noc, więc wybudzenie
// o 12:00 (poza oknem porannym); poranek: test/testy_poranek_szybciej.cpp testR_PRZ_22_dyrygent.
static void testT_WYM_24_dyrygentPrzyciskBezWiFi() {
  Panel p;
  zwyklyPanel(p);
  p.zegar.epoch = ep(2026, 11, 4, 12, 0);
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = BIT_PRAWY;
  p.start.stan.ekran = (uint8_t)Ekran::DZIEN;
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {300, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.sen.snow == 1);
  SPRAWDZ(p.d.ile("siec.polacz") == 0);
  SPRAWDZ(p.siec.zadania.empty());
  SPRAWDZ(p.ekran.odswiezen == 1);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::K1);
  SPRAWDZ(p.sen.plan.budzZegarem);
}

// R-PRZ-06 z dyrygentem: ↻ na S5 (ostatni odczyt nieudany, brak nocy, 12:00) → pobranie z AirBridge.
static void testR_PRZ_06_dyrygentPonowienie() {
  Panel p;
  zwyklyPanel(p);
  p.zegar.epoch = ep(2026, 11, 4, 12, 0);
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = BIT_SRODEK;
  p.start.stan.ekran = (uint8_t)Ekran::DZIEN;
  p.start.stan.ostatniOdczytOk = false;
  p.przyciski.skrypt = {{0, BIT_SRODEK}, {300, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.sen.snow == 1);
  SPRAWDZ(p.d.ile("siec.polacz") == 1);
  SPRAWDZ(p.siec.zadania.size() >= 1 && AtrapaSieci::sciezka(p.siec.zadania[0]) == "/api/status");
}

// T-WYM-41 z dyrygentem: pierwszy start nowej wersji (stan „do sprawdzenia”).
static void testT_WYM_41_dyrygentSamosprawdzenie() {
  {
    // obraz, który nie łączy się z Wi-Fi → powrót do poprzedniej wersji, bez oznaczenia i bez snu
    Panel p;
    zwyklyPanel(p);
    p.start.stanW = StanWersji::DO_SPRAWDZENIA;
    p.start.przyczynaStartu = PrzyczynaStartu::PROGRAMOWY;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    p.siec.siecJest = false;
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.start.powrot);
    SPRAWDZ(!p.start.oznaczona);
    SPRAWDZ(p.sen.snow == 0);
  }
  {
    // partycja plików się nie montuje → powrót
    Panel p;
    zwyklyPanel(p);
    p.start.stanW = StanWersji::DO_SPRAWDZENIA;
    p.start.przyczynaStartu = PrzyczynaStartu::PROGRAMOWY;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    p.pliki.montujeSie = false;
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.start.powrot && !p.start.oznaczona && p.sen.snow == 0);
  }
  {
    // zwykły obraz → oznaczony jako dobry przed pierwszym snem
    Panel p;
    zwyklyPanel(p);
    p.start.stanW = StanWersji::DO_SPRAWDZENIA;
    p.start.przyczynaStartu = PrzyczynaStartu::PROGRAMOWY;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.start.oznaczona && !p.start.powrot);
    SPRAWDZ(p.sen.snow == 1);
    SPRAWDZ(p.d.pierwsze("start.oznacz") >= 0 && p.d.pierwsze("start.oznacz") < p.d.pierwsze("sen"));
  }
  {
    // wersja już dobra: bez oznaczania i bez powrotu
    Panel p;
    zwyklyPanel(p);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(!p.start.oznaczona && !p.start.powrot && p.sen.snow == 1);
  }
}

// Budżety sterty i stosu w logu wybudzenia: wiersz przed snem.
static void testLogPamieciDyrygent() {
  Panel p;
  zwyklyPanel(p);
  p.diag.sterta = 70000;
  p.diag.stos = 5000;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  int wiersz = p.d.pierwsze("port pamiec sterta_min=70000 stos_zapas=5000");
  SPRAWDZ(wiersz >= 0 && wiersz < p.d.pierwsze("sen"));
  Panel q;
  zwyklyPanel(q);
  q.diag.sterta = 60000;
  Dyrygent(q.adaptery(), KONF).wybudzenie();
  SPRAWDZ(q.d.pierwsze("port pamiec sterta_min=60000 stos_zapas=5000 poza_budzetem") >= 0);
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  // (a)
  testZegarZnany();
  testT_WYM_24_kiedyPobierac();
  testR_PRZ_06_ponowNaS5();
  testR_PRZ_02_ekranPoStarcie();
  testR_PRZ_18_bezPowrotuWNocy();
  testR_KON_37_rysowanieWNocy();
  testOknoSerwisoweTylkoPoPrzycisku();
  testEkranDniaTrwa();
  testR_PRZ_02_czasSnu();
  testR_PRZ_18_snemWNocy();
  testT_WYM_29_zaciety();
  testT_WYM_21_przyciskBudzacy();
  testPoStatusie();
  testGranicaWBudowie12();
  testKodyIOdpytywanie();
  testZalegleDobyPozaSlotemNocnym();
  testDrobneDecyzje();
  testLogPamieci();
  // (b)
  testT_WYM_82_identyfikatorWersji();
  testT_WYM_82_licznikStartow();
  testT_WYM_83_znacznik();
  testR_AW_39_tekstyU06();
  testT_WYM_41_samosprawdzenie();
  testT_WYM_81_ustawieniaStraznika();
  // (c)
  testT_WYM_81_straznikNaPoczatku();
  testT_WYM_81_odnowienieWPetliJoba();
  testT_WYM_81_wywolanieBezPowrotu();
  testT_WYM_82_dyrygentLicznik();
  testT_WYM_83_trybAwaryjny();
  testT_WYM_24_dyrygentPrzyciskBezWiFi();
  testR_PRZ_06_dyrygentPonowienie();
  testT_WYM_41_dyrygentSamosprawdzenie();
  testLogPamieciDyrygent();
  printf("testy_wybudzenie: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
