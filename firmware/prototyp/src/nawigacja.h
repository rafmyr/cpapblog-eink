// SPDX-License-Identifier: GPL-3.0-or-later
// Mapa przycisków (spec 5.6.1, 5.6.2, R-PRZ-*) w zakresie prototypu, bez zależności od Arduino.
// Przyciski reTerminal E1001 według oznaczeń na obudowie: ◀ = `<`, ▶ = `>`, ↻ = zielony (adaptery_esp32.cpp).
// Przytrzymanie = co najmniej 2 s [PRÓG] (R-PRZ-08).
//
// (jedna reguła dla całego panelu):
// - ▶ dalej / więcej / „tak”, ◀ wstecz / „nie”, ↻ dom (ekran dnia); na ekranie dnia ↻ nic, a na ekranie, który czeka
//   na dane (S5, S10), ponawia pobranie;
// - przytrzymanie tylko zapisuje („wiem”, „omówione” z potwierdzeniem, „wymienione dziś” na S-K-31 i S-K-35, cofnięcie,
//   odpowiedź „Co było inaczej?”) albo uruchamia usługę z Wi-Fi (raport ogólny ▶ 2 s w K2, sieć panelu ◀ 2 s na U-01
//   i U-05); nigdy nie otwiera strony do przeglądania;
// - kokpit to ciąg stron (5.5.7, ADR-0041, R-KOK-19): ekran dnia ▶ K1 (1/3) ▶ K2 (2/3) ▶ „Co znaczą te liczby” (bez
//   numeru, tylko w profilu publicznym) ▶ Otwarte sprawy (bez numeru, tylko przy co najmniej jednej sprawie)
//   ▶ Ustawienia U-01 (3/3) ▶ 2 s formularz U-03 ▶ 2 s test U-05; ◀ cofa o jedną stronę, z K1 do ekranu dnia; na
//   „Otwartych sprawach” ▶ i ◀ przesuwają po kartach, a za ich końcem przechodzą na sąsiednią stronę; S8 „Twoja norma”
//   i „Sprzęt” nie są stronami kokpitu (S8 zostaje kamieniem milowym ekranu dnia, daty wymiany w formularzu U-03);
// - wyjątek: pytanie „Co było inaczej?” (siatka): ◀ ▶ kursor, ↻ zaznacz, ↻ 2 s zapisz, ◀ 2 s pomiń;
// - ukryty skrót ◀ + ▶ razem 2 s: test U-05 z oknem wgrywania (R-PRZ-21, dyrygent przytrzymanieLubSkrot()).
#pragma once

#include <string>

enum class Ekran : unsigned char { DZIEN = 0, K1 = 1, K2 = 2, PYTANIE = 3, TEST = 4, OMOWIONE = 5, SPRAWY = 6,
                                   PRZENIESIONE = 7, ZAPISANO = 8, RAPORT = 9,
                                   // NORMA (S8) i SPRZET to dawne strony kokpitu, wycofane w ADR-0041: wartości zostają,
                                   // żeby numery dalszych ekranów w pamięci RTC się nie przesunęły (ekranZRtc → DZIEN)
                                   NORMA = 10, LICZBY = 11,  // NORMA wycofane; „Co znaczą te liczby” (R-KOK-22)
                                   SPRZET = 12, SPRZET_ZAPISANO = 13,  // SPRZET wycofane; `Wymiana zapisana.` (R-MAS-01, R-MAS-18)
                                   USTAWIENIA = 14, FORMULARZ = 15,  // U-01 „Ustawienia”, U-03 formularz z kodem QR
                                   SIEC_PANELU = 16,  // U-02, sieć panelu z formularzem (tylko ręcznie)
                                   AKTUALIZACJA = 17,  // U-07 „Aktualizacja” (R-KONF-43, ↻ 2 s na U-01)
                                   PORTRET = 18 };  // Portret nocy jako strona pod ▶ z ekranu problemu (R-POR-24);
                                                    // przejścia w src/dyrygent.cpp przejsciePortretu()
// Ekran zapisany w pamięci RTC (StanRtc::ekran) z kontrolą zakresu: bajt spoza listy (zepsuta pamięć RTC,
// stary obraz z innym wyliczeniem) daje ekran dnia zamiast wartości spoza `Ekran`. Przykład: 9 → RAPORT, 200 → DZIEN.
// Dawne strony S8 i „Sprzęt” (obraz sprzed ADR-0041) też dają ekran dnia (5.5.7 fail-path R-KOK-19).
inline Ekran ekranZRtc(unsigned char b) {
  if (b == static_cast<unsigned char>(Ekran::NORMA) || b == static_cast<unsigned char>(Ekran::SPRZET)) return Ekran::DZIEN;
  return b <= static_cast<unsigned char>(Ekran::PORTRET) ? static_cast<Ekran>(b) : Ekran::DZIEN;
}
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
  SPRZET_WYMIENIONE, SPRZET_COFNIJ,  // ◀ 2 s na S-K-31 i S-K-35 (R-MAS-01, R-MAS-18), ◀ 2 s cofnij na `Wymiana zapisana.`
  RAPORT_STARSZE,  // ↻ 2 s na ekranie raportu (R-LEK-33)
  SPRAWY_OD_KONCA,  // ◀ z U-01 do „Otwartych spraw”: ostatnia karta (R-KOK-20)
  SIEC_PANELU_START,    // ◀ 2 s na U-01 i U-05 (i na ekranie pierwszego uruchomienia), ↻ 2 s na U-02
  SIEC_PANELU_ANULUJ,   // ◀ na U-02 (R-KONF-29): poprzednia sieć, powrót tam, skąd uruchomiono; ↻ dom
  POMIAR_BATERII,       // ↻ na S5 z E-05 krytycznym: nowy pomiar baterii i nowy wybór ekranu dnia, bez Wi-Fi
  AKT_SPRAWDZ,          // ↻ 2 s na U-01 (R-KONF-43): U-07, Wi-Fi i sprawdzenie najnowszego wydania
  AKT_INSTALUJ,         // ▶ na U-07 w stanie „jest nowsza” (R-KONF-44)
  AKT_PONOW,            // ▶ na U-07 po nieudanej próbie: ta sama czynność jeszcze raz
};

// numer strony kokpitu w nagłówku (`n / 3`, R-KOK-19); 0 = ekran bez numeru. „Co znaczą te liczby” i „Otwarte sprawy”
// są w ciągu stron, ale bez numeru (stronaKokpitu() true, numerStrony() 0).
const int STRON_KOKPITU = 3;
int numerStrony(Ekran e);
std::string numerStronyTekst(Ekran e);  // `n / 3` albo pusty
bool stronaKokpitu(Ekran e);  // K1, K2, „Co znaczą te liczby”, „Otwarte sprawy”, U-01
// Kolejka naciśnięć (spec 5.6.1): naciśnięcie zapamiętane w czasie rysowania wykonuje się bez pokazania ekranu
// pośredniego, więc tylko czysta nawigacja: krótkie naciśnięcie na ekranie dnia albo na stronie kokpitu.
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
  bool sk31 = false;            // ekran dnia to S-K-31 albo S-K-35: ◀ 2 s = „wymienione dziś” (R-MAS-01, R-MAS-18)
  // (R-KOK-19, R-KOK-20): co najmniej jedna sprawa z listy R-EKR-48 w chwili naciśnięcia: strona „Otwarte sprawy”
  // w ciągu; bez spraw (także bez wyników silnika) strona pominięta w obie strony
  bool sprawyDostepne = false;
  bool profilPubliczny = false; // ustawienie „Profil publiczny” (5.6.4): „Co znaczą te liczby” po K2 (R-KOK-22)
  Ekran zrodlo = Ekran::DZIEN;  // raport (QR) i U-02: ekran, z którego otwarto (◀ tam wraca, ▶ dalej od niego)
  // potwierdzenie „Omówione z lekarzem?” w wariancie blokady (R-LEK-26, `Najpierw nowy raport: ▶ na ekranie L.`):
  // ▶ niczego nie zapisuje i nic nie robi, ◀ wraca, ↻ anuluje (mapa 5.6.2)
  bool omowienieZablokowane = false;
  // (R-KONF-07): panel bez sieci domowej albo adresu AirBridge; ekran dnia to instrukcja U-02,
  // a ◀ 2 s na nim uruchamia sieć panelu (tylko ręcznie, nigdy sama)
  bool bezKonfiguracji = false;
  // U-07 (R-KONF-44): 1 = „jest nowsza” (▶ instaluje), 2 = po nieudanej próbie (▶ spróbuj ponownie), 0 = ▶ nic
  int aktualizacja = 0;
};

struct Przejscie {
  Ekran ekran;
  Akcja akcja;
};

Przejscie nacisniecie(Ekran e, Przycisk p, bool przytrzymanie, const KontekstNaw& k);

// Slot nocny 23:30–05:00 (R-PRZ-18): ekrany otwarte przyciskiem nie wracają same.
bool slotNocny(int minutaDoby);
// (R-PRZ-19): w slocie nocnym ekran potwierdzenia z cofnięciem („Przeniesione” 30 s, „Zapisano” 60 s,
// `Wymiana zapisana.` 30 s [PRÓG]) zostaje do 05:00, ale po swoim czasie panel budzi się raz i rysuje go bez
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
