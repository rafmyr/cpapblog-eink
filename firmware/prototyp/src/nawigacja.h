// SPDX-License-Identifier: GPL-3.0-or-later
// Mapa przycisków (spec 5.6.1, 5.6.2, R-PRZ-*) w zakresie prototypu, bez zależności od Arduino.
// Przyciski reTerminal E1001 według oznaczeń na obudowie: ◀ = `<`, ▶ = `>`, ↻ = zielony (adaptery_esp32.cpp).
// Przytrzymanie = co najmniej 2 s [PRÓG] (R-PRZ-08).
//
// (jedna reguła dla całego panelu):
// - ▶ dalej / więcej / „tak”, ◀ wstecz / „nie”, ↻ dom (ekran dnia); na ekranie dnia ↻ nic, a na ekranie, który czeka
//   na dane (S5, S10), ponawia pobranie;
// - przytrzymanie tylko zapisuje („wiem”, „omówione” z potwierdzeniem, „wymienione dziś”, nowy model maski, cofnięcie,
//   odpowiedź „Co było inaczej?”) albo uruchamia usługę z Wi-Fi (raport ogólny ▶ 2 s w K2, sieć panelu ◀ 2 s na U-01
//   i U-05); nigdy nie otwiera strony do przeglądania;
// - kokpit to ciąg stron: ekran dnia ▶ K1 (1/6) ▶ K2 (2/6) ▶ Otwarte sprawy (3/6) ▶ Twoja norma S8 (4/6, w profilu
//   publicznym dalej „Co znaczą te liczby”) ▶ Sprzęt (5/6) ▶ Ustawienia U-01 (6/6) ▶ formularz U-03 ▶ test U-05; ◀ cofa
//   o jedną stronę, z K1 do ekranu dnia; na stronach z listą (sprawy, sprzęt) ▶ i ◀ przesuwają po liście, a za jej końcem
//   przechodzą na sąsiednią stronę;
// - wyjątek: pytanie „Co było inaczej?” (siatka): ◀ ▶ kursor, ↻ zaznacz, ↻ 2 s zapisz, ◀ 2 s pomiń;
// - ukryty skrót ◀ + ▶ razem 2 s: test U-05 z oknem wgrywania (R-PRZ-21, dyrygent przytrzymanieLubSkrot()).
#pragma once

#include <string>

enum class Ekran : unsigned char { DZIEN = 0, K1 = 1, K2 = 2, PYTANIE = 3, TEST = 4, OMOWIONE = 5, SPRAWY = 6,
                                   PRZENIESIONE = 7, ZAPISANO = 8, RAPORT = 9,
                                   NORMA = 10, LICZBY = 11,  // S8 „Twoja norma”, „Co znaczą te liczby”
                                   SPRZET = 12, SPRZET_ZAPISANO = 13,  // „Sprzęt” (5.9.3), `Nowy model maski zapisany.` (R-MAS-14)
                                   USTAWIENIA = 14, FORMULARZ = 15,  // U-01 „Ustawienia”, U-03 formularz z kodem QR
                                   SIEC_PANELU = 16 };  // U-02, sieć panelu z formularzem (tylko ręcznie)
enum class Przycisk : unsigned char { BRAK = 0, LEWY = 1, SRODEK = 2, PRAWY = 3 };

// Przerwanie długiego pobierania (↻ na S5, partie historii) naciśnięciem przycisku: liczy się tylko naciśnięcie po
// chwili, w której żaden przycisk nie był wciśnięty, więc przycisk, który uruchomił pobieranie i jest jeszcze
// trzymany, niczego nie przerywa. krok(): stan przycisków w tej chwili → true = przerwać.
struct PrzerwaniePrzyciskiem {
  bool puszczone = false;
  bool krok(bool wcisniety) {
    if (!wcisniety) {
      puszczone = true;
      return false;
    }
    return puszczone;
  }
};
enum class Akcja : unsigned char {
  NIC, POKAZ, PONOW_POBRANIE, OTWORZ_PYTANIE,
  PYT_ZAZNACZ, PYT_ZAPISZ, PYT_POMIN, PYT_WSTECZ, PYT_DALEJ,
  WIEM, OMOWIONE_TAK, SPRAWY_OTWORZ, SPRAWY_DALEJ, SPRAWY_WSTECZ, ODP_TAK, ODP_NIE, ODP_POMIN,
  COFNIJ_WIEM, COFNIJ_ODP,
  SPRZET_DALEJ, SPRZET_WYMIENIONE, SPRZET_NOWY_MODEL, SPRZET_COFNIJ,  // „Sprzęt” (5.9.3), ◀ 2 s na S-K-31 (R-MAS-01), R-MAS-14
  RAPORT_STARSZE,  // ↻ 2 s na ekranie raportu (R-LEK-33)
  SPRAWY_OD_KONCA,  // ◀ z S8 do „Otwartych spraw”: ostatnia karta
  SPRZET_WSTECZ, SPRZET_OTWORZ, SPRZET_OD_KONCA,  // kursor „Sprzęt” w górę, od pierwszego, od ostatniego wiersza
  SIEC_PANELU_START,    // ◀ 2 s na U-01 i U-05 (i na ekranie pierwszego uruchomienia), ↻ 2 s na U-02
  SIEC_PANELU_ANULUJ,   // ◀ na U-02 (R-KONF-29): poprzednia sieć, powrót tam, skąd uruchomiono; ↻ dom
  POMIAR_BATERII,       // ↻ na S5 z E-05 krytycznym: nowy pomiar baterii i nowy wybór ekranu dnia, bez Wi-Fi
};

// numer strony kokpitu w nagłówku (`n / 6`); 0 = ekran poza ciągiem stron. „Co znaczą te liczby” ma numer S8.
const int STRON_KOKPITU = 6;
int numerStrony(Ekran e);
std::string numerStronyTekst(Ekran e);  // `n / 6` albo pusty
// Kolejka naciśnięć (spec 5.6.1): naciśnięcie zapamiętane w czasie rysowania wykonuje się bez pokazania ekranu
// pośredniego, więc tylko czysta nawigacja: krótkie naciśnięcie na ekranie dnia albo na stronie kokpitu `n / 6`.
// Pytanie, potwierdzenie, ekran poza ciągiem stron albo przytrzymanie (zapis) = false: reszta kolejki przepada.
bool zKolejkiWolno(Ekran e, bool dlugo);
// karta „Otwartych spraw” z NVS przycięta do liczby kart (min. 1), żeby ◀ nie cofało niewidocznych kart
int przytnijKarte(int karta, int liczba);

struct KontekstNaw {
  bool s5 = false;              // ekran dnia to brak danych (↻ ponawia pobranie, R-PRZ-06)
  // ten brak danych to S5 z E-05 krytycznym (bateria ekranu, klucz `E-05`): ↻ mierzy baterię od nowa i rysuje
  // ekran dnia z nowym wyborem, bez Wi-Fi i bez ponowienia pobrania (pobranie nic by tu nie naprawiło); stopka `↻ odśwież`
  bool s5Bateria = false;
  bool zaproszenie = false;     // linia `Co było inaczej? ↻ 2 s` do terminu odpowiedzi (R-CZY-17)
  bool ostatniaNoc = false;     // jest noc, o którą można zapytać z K1 (R-CZY-08)
  bool nocny = false;           // ekran dnia to ekran nocny „Dobranoc”: ▶ → K1, reszta nic
  bool wzorzec = false;         // ekran dnia to ekran wzorca L (5.12.5): ↻ 2 s wiem, ◀ 2 s omówione, ▶ raport
  bool problem = false;         // … w tonie „problem” (L-09, S3): ◀ 2 s nic, ▶ K1 (mapa 5.6.2)
  bool pytanieTN = false;       // ekran dnia to pytanie tak/nie (5.12.8): ◀ nie, ▶ tak, ↻ pomiń, przytrzymania nic
  int sprawa = 1, spraw = 0;    // „Otwarte sprawy”: bieżąca karta (od 1) i liczba kart (0 = stan pusty)
  bool cofniecie = true;        // „Przeniesione”, „Zapisano”: cofnięcie jeszcze działa (w nocy tylko przez swój czas, R-PRZ-19)
  bool pytanieRuszone = false;  // w siatce „Co było inaczej?” zmieniono albo wybrano pozycję: ↻ 2 s zapisuje
  bool sk31 = false;            // ekran dnia to S-K-31: ◀ 2 s = „wymienione dziś” (R-MAS-01)
  int kursorSprzetu = 0;        // „Sprzęt”: wiersz kursora (0 maska: ↻ 2 s = nowy model, R-MAS-11; 1 filtr)
  int wierszySprzetu = 2;       // liczba wierszy „Sprzęt” (sprzet::EL_N): ▶ na ostatnim → U-01
  bool sprawyDostepne = false;  // dyrygent ma wyniki silnika : strona „Otwarte sprawy”; inaczej pominięta
  bool profilPubliczny = false; // ustawienie „Profil publiczny” (5.6.4): ▶ na S8 → „Co znaczą te liczby” (R-EKR-51)
  Ekran zrodlo = Ekran::DZIEN;  // raport (QR) i U-02: ekran, z którego otwarto (◀ tam wraca, ▶ dalej od niego)
  // potwierdzenie „Omówione z lekarzem?” w wariancie blokady (R-LEK-26, `Najpierw nowy raport: ▶ na ekranie L.`):
  // ▶ niczego nie zapisuje i nic nie robi, ◀ wraca, ↻ anuluje (mapa 5.6.2)
  bool omowienieZablokowane = false;
  // (R-KONF-07): panel bez sieci domowej albo adresu AirBridge; ekran dnia to instrukcja U-02,
  // a ◀ 2 s na nim uruchamia sieć panelu (tylko ręcznie, nigdy sama)
  bool bezKonfiguracji = false;
};

struct Przejscie {
  Ekran ekran;
  Akcja akcja;
};

Przejscie nacisniecie(Ekran e, Przycisk p, bool przytrzymanie, const KontekstNaw& k);

// Slot nocny 23:30–05:00 (R-PRZ-18): ekrany otwarte przyciskiem nie wracają same.
bool slotNocny(int minutaDoby);
// (R-PRZ-19): w slocie nocnym ekran potwierdzenia z cofnięciem („Przeniesione” 30 s, „Zapisano” 60 s,
// `Nowy model maski zapisany.` 30 s [PRÓG]) zostaje do 05:00, ale po swoim czasie panel budzi się raz i rysuje go bez
// podpisu cofnięcia. sekundDoKoncaCofniecia(): sekundy do tego wybudzenia, -1 poza nocą, na innych ekranach i po
// upływie; koniecCofnieciaTeraz(): chwila `sek` leży w KONIEC_COFNIECIA_TOL_S po upływie (wybudzenie z tego powodu).
const long KONIEC_COFNIECIA_TOL_S = 30;
long sekundDoKoncaCofniecia(Ekran e, long sek, int minutaDoby);
bool koniecCofnieciaTeraz(Ekran e, long sek, int minutaDoby);
// Powrót do ekranu dnia po 5 min [PRÓG] bez naciśnięcia (R-PRZ-02, R-CZY-16). Zwraca liczbę sekund do powrotu
// (0 = już), albo -1, gdy ekran nie wraca sam (ekran dnia, slot nocny). Potwierdzenie „Omówione z lekarzem?” wraca po
// 60 s [PRÓG] bez zapisu, także w nocy (R-PRZ-05, 5.6.2 „W nocy”). „Przeniesione” wraca po 30 s [PRÓG] (R-PRZ-17),
// „Zapisano” po 60 s [PRÓG] (R-PRZ-12); w nocy oba zostają do 05:00 (R-PRZ-19).
long sekundDoPowrotu(Ekran e, long sekundOdNacisniecia, int minutaDoby);
