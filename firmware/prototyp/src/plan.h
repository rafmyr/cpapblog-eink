// SPDX-License-Identifier: GPL-3.0-or-later
// Plan wybudzenia (T-KOMP-24, spec techniczna 6.4, 7.1–7.5, 9.5; ADR-0021): kiedy panel ma się obudzić, z jakiego
// powodu z R-NF-02 i czy to wybudzenie włącza Wi-Fi. Spec techniczna 13.1, wiersz 2: koniec stałego budzenia co 30 min przez całą dobę i zapytań do AirBridge 23:30–05:00,
// czuwanie na przycisk tylko po wybudzeniu przyciskiem (2.2, wnioski 3 i 4), rzadsze ponawianie od drugiej doby bez
// danych (R-DANE-18). Bez Arduino: czas i stan jako dane, dyrygent (src/dyrygent.h) tylko wykonuje wynik.
//
// Zakres: okno poranne, sprawdzenie o 11:00, ponowienia R-DANE-18, pogoda przed
// slotem wieczornym (stała chwila 21:00 z R-KON-17, tylko ze źródłem otoczenia), początek slotu nocnego,
// zapis czujników co 15 min w oknie nocy sypialni prototypu (23:00–06:00, src/sypialnia.h oknoSypialni(); okno slotu
// wieczornego z M-18 i plik serii T-DANE-02 są poza planem), powrót R-PRZ-02, termin odpowiedzi R-PIE-13, brak czasu
// R-AW-20. Slot wieczorny co 30 min (R-KON-01), slot tygodniowy (R-KON-08) i sygnał (R-BUZ-02b) są poza tym planem.
//
// Implementacja: src/plan.cpp; testy: test/testy_plan_wybudzen.cpp, dyrygent: test/testy_dyrygenta.cpp;
// slot dzienny: test/testy_slot_dzienny_w_planie.cpp.
#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "czynniki.h"
#include "nawigacja.h"
#include "noc.h"
#include "wybudzenie.h"

// Typ zbioru powodów: 32 bity, bo 16 bitów zajęły już powody kroku 2.
using Powody = uint32_t;
// Powody wybudzenia z R-NF-02 jako bity (jeden termin może mieć kilka powodów naraz, np. 05:00 = okno poranne
// i zapis czujników). Nazwa w nawiasie to tekst w logu i w polu `powod` pliku /bateria.csv (nazwyPowodow()).
const Powody POWOD_OKNO_PORANNE = 1u << 0;   // „okno_poranne”: R-DANE-02, R-DANE-19, R-DANE-03; z Wi-Fi
const Powody POWOD_SPRAWDZENIE_11 = 1u << 1; // „sprawdzenie_11”: R-DANE-15, R-DANE-20, R-DANE-24; z Wi-Fi
const Powody POWOD_PONOWIENIE = 1u << 2;     // „ponowienie”: R-DANE-18 (11:00–23:30); z Wi-Fi
const Powody POWOD_POGODA = 1u << 3;         // „pogoda”: R-DANE-16 przed slotem wieczornym; z Wi-Fi
const Powody POWOD_POCZATEK_NOCY = 1u << 4;  // „poczatek_nocy”: R-KON-31, R-KON-33, R-DANE-25 (23:30); z Wi-Fi
const Powody POWOD_CZUJNIKI = 1u << 5;       // „czujniki”: R-DANE-09 (wybudzenie pomiarowe, T-WYM-31); bez Wi-Fi
const Powody POWOD_POWROT = 1u << 6;         // „powrot”: R-PRZ-02 (5 min bez naciśnięcia); bez Wi-Fi
const Powody POWOD_PYTANIE = 1u << 7;        // „pytanie”: R-PIE-13, R-CZY-03 (termin odpowiedzi); bez Wi-Fi
const Powody POWOD_CZAS = 1u << 8;           // „czas”: R-AW-20 (brak czasu ważnego, co 30 min); z Wi-Fi
const Powody POWOD_ODSWIEZENIE = 1u << 9;    // „odswiezenie”: R-EKR-60 (przerysowanie ekranu dnia co 2 h); bez Wi-Fi
// Slot dzienny: terminy i powody wybudzeń; wybór ekranu
// dnia S-K-33…S-K-38, drzemka i prognoza godzinowa z serwisu to (do tego czasu dyrygent przy tych powodach
// przerysowuje ekran dnia jak przy R-EKR-60). Testy: test/testy_slot_dzienny_w_planie.cpp.
const Powody POWOD_SLOT_DZIENNY_START = 1u << 10;    // „slot_dzienny”: R-KON-41 (start slotu dziennego); z Wi-Fi
                                                       // tylko na `/api/status` (R-KON-42, R-KON-56)
const Powody POWOD_SLOT_DZIENNY_GODZINA = 1u << 11;  // „slot_dzienny_godzina”: R-KON-42 (ocena o pełnej godzinie
                                                       // slotu dziennego); z Wi-Fi tylko na `/api/status`
const Powody POWOD_PROGNOZA_GODZINOWA = 1u << 12;    // „prognoza_godzinowa”: R-KON-52 (prognoza godzinowa przy
                                                       // starcie slotu dziennego); z Wi-Fi (serwis pogody)
const Powody POWOD_SLOT_DZIENNY_BEZ_SIECI = 1u << 13;  // „slot_dzienny_bez_sieci”: R-KON-41 przy E-01 (start albo
                                                         // ocena o pełnej godzinie bez Wi-Fi; zapytania do AirBridge
                                                         // tylko w rytmie R-DANE-18); bez Wi-Fi
// slot wieczorny i tygodniowy (5.8.2, src/slot_wieczorny.h)
const Powody POWOD_SLOT_WIECZORNY = 1u << 14;   // „slot_wieczorny”: R-KON-01, ocena co 30 min w oknie (R-KON-09,
                                                  // R-KON-17) do 23:30; z Wi-Fi tylko na `/api/status` i źródło
                                                  // otoczenia (CO2 S-K-01, S-K-02), tylko przy źródle otoczenia
const Powody POWOD_SLOT_TYGODNIOWY = 1u << 15;  // „slot_tygodniowy”: R-KON-08, niedziela 17:00 (pytanie o senność
                                                  // R-SKI-16, linie R-SKI-12, R-SKI-33); bez Wi-Fi
// (R-KON-01, wariant bez CO2): ocena slotu wieczornego co 30 min w oknie bez Wi-Fi, gdy nie ma
// POWOD_SLOT_WIECZORNY (egzemplarz bez ekranów CO2 albo bez źródła otoczenia); S-K-03…S-K-09 z danych w pamięci
// (noce, prognoza z POWOD_POGODA), e-ink tylko przy innej klatce (pokaz(): skrót klatki)
const Powody POWOD_SLOT_WIECZORNY_BEZ_SIECI = 1u << 16;  // „slot_wieczorny_bez_sieci”: R-KON-01; bez Wi-Fi
// Szybszy wynik nocy rano (decyzja właściciela projektu 10.11.2026, A + B):
// gęste okno co 5 min wokół zwykłej pory końca terapii (A) i powtórka po 2 min, gdy AirBridge przetwarza pliki po
// terapii (B). Oba z pełnym pobraniem jak okno poranne (status, lista nocy, raport; przy terapii sam status).
const Powody POWOD_OKNO_GESTE = 1u << 17;   // „okno_geste”: R-DANE-61 (co 5 min wokół zwykłej pory końca terapii); z Wi-Fi
const Powody POWOD_POWTORKA = 1u << 18;     // „powtorka_przetwarzania”: R-DANE-62 (2 min po statusie
                                            // „przetwarza”, najwyżej 5 na dobę); z Wi-Fi

// Stałe planu kroku 2 (wszystkie [PRÓG] ze spec, minuty doby czasu lokalnego).
const int PLAN_OKNO_OD = 5 * 60;            // R-DANE-02: okno poranne 05:00–11:00
const int PLAN_OKNO_DO = 11 * 60;           // R-DANE-15, R-DANE-24: sprawdzenie o 11:00
const int PLAN_NOC_OD = 23 * 60 + 30;       // R-KON-31, R-DANE-18: początek slotu nocnego, koniec ponowień
const int PLAN_POGODA_WIECZOR = 21 * 60;    // R-DANE-16 przed slotem wieczornym; krok 2: okno stałe z R-KON-17
const int PLAN_CO_ILE_MIN = 30;             // R-DANE-19, R-DANE-03, R-DANE-18, R-AW-20
const int PLAN_CO_ILE_RZADKO_MIN = 180;     // R-DANE-18 od drugiej nocy bez danych (14:00, 17:00, 20:00, 23:00)
const int PLAN_CZUJNIKI_CO_MIN = 15;        // R-DANE-09
const int PLAN_ODSWIEZENIE_OD = 11 * 60;     // R-EKR-60: przerysowanie ekranu dnia najwcześniej o 11:00
const int PLAN_ODSWIEZENIE_CO_MIN = 120;    // R-EKR-60: 2 h po ostatnim narysowaniu ekranu dnia
const int PLAN_SLOT_DZIENNY_PO_S = 60 * 60;  // R-KON-41: 60 min [PRÓG] po późniejszej z chwil koniec terapii, import
const int PLAN_SLOT_DZIENNY_DO = PLAN_POGODA_WIECZOR;  // okno stałe R-KON-17 (21:00); koniec slotu: koniecSlotuDziennego()
const int PLAN_PROGNOZA_WAZNA_S = 3 * 3600;  // R-KON-52: prognoza godzinowa starsza niż 3 h [PRÓG] → pobranie przy starcie
const uint32_t TOLERANCJA_TERMINU_S = 60;  // powodyTeraz() bez znanej chwili obsłużonej: termin z ostatniej minuty
const uint32_t ZALEGLY_TERMIN_S = 1;        // nastepnyTermin(): sen przed terminem, który wypadł w trakcie wybudzenia
// Start slotu dziennego później niż 60 s przed pełną godziną (np. 08:59:30) idzie razem
// z oceną o tej godzinie (jedno wybudzenie z Wi-Fi zamiast dwóch w minucie)
const int PLAN_START_Z_OCENA_S = 60;
// Ocena slotu dziennego czyta `/api/status` jedną próbą z tym limitem odpowiedzi [PRÓG]
// (bez ponowień R-AW-01, bo brak odpowiedzi to zwykła ocena, R-KON-56); 3 s to kilkakrotność czasu odpowiedzi
// AirBridge w sieci domowej (odczyt egzemplarza: ok. 100–300 ms) [DOMYSŁ, do kalibracji w]
const uint32_t LIMIT_STATUSU_SLOTU_MS = 3000;
// Gęste okno (A, R-DANE-61; bez niego przy E-01: ostatnia próba bez odpowiedzi AirBridge albo Wi-Fi, decyzja właściciela
// projektu 10.11.2026, także z odrzuconym logowaniem): zwykła pora końca terapii = mediana końca terapii
// (koniecTerapiiNocy()) z ostatnich PLAN_GESTE_NOCY nocy z terapią, z co najmniej PLAN_GESTE_NOCY_MIN takich nocy
// (mniej: bez gęstego okna, jak przed 10.11.2026); okno od PLAN_GESTE_PRZED_MIN przed do PLAN_GESTE_PO_MIN po niej, przycięte do okna porannego 05:00–11:00;
// w nim zapytania co PLAN_GESTE_CO_MIN, dopóki noc nie jest kompletna. Wszystkie [PRÓG] (decyzja właściciela projektu 10.11.2026,
// bez źródła; docs/spec/progi-poza-zakresem.csv).
const int PLAN_GESTE_CO_MIN = 5;        // [PRÓG] R-DANE-61
const int PLAN_GESTE_PRZED_MIN = 45;    // [PRÓG] R-DANE-61
const int PLAN_GESTE_PO_MIN = 45;       // [PRÓG] R-DANE-61
const int PLAN_GESTE_NOCY = 14;         // [PRÓG] R-DANE-61 (jak R-KON-09)
const int PLAN_GESTE_NOCY_MIN = 3;      // [PRÓG] R-DANE-61 (jak R-KON-09)
// Powtórka przy przetwarzaniu (B, R-DANE-62): następne wybudzenie PLAN_POWTORKA_S po odczycie `/api/status`
// z przetwarzaniem plików po terapii (src/airbridge.h modulPrzetwarza()), najwyżej PLAN_POWTORKI_MAKS na dobę.
const uint32_t PLAN_POWTORKA_S = 2 * 60;  // [PRÓG] R-DANE-62
const int PLAN_POWTORKI_MAKS = 5;         // [PRÓG] R-DANE-62
// Pobranie przy wybudzeniu przyciskiem rano bez nocy (D, R-PRZ-22): najwyżej raz na tyle sekund.
const long PLAN_POBRANIE_PRZYCISKIEM_CO_S = 120L;  // [PRÓG] R-PRZ-22: 2 min
// Ile sekund ostatnia obsłużona chwila albo start slotu może leżeć „w przyszłości” po
// cofnięciu zegara przez NTP w tym samym wybudzeniu (zegar panelu spieszy się) [PRÓG]; dalej: jak nieznana
const uint32_t KOREKTA_NTP_S = 15 * 60;

// Stan, z którego plan liczy terminy (T-INT-11: wejście wybudzenia). Wypełnia dyrygent z RTC, NVS i plików po
// pobraniu (na koniec wybudzenia) albo przed nim (powodyTeraz()). „Dziś” = data lokalna chwili `sekundaDoby`.
struct WejsciePlanu {
  bool zegar = false;              // czas ważny (zegarZnany()); bez czasu tylko POWOD_CZAS co 30 min (R-AW-20)
  int sekundaDoby = 0;             // czas lokalny teraz, sekundy od północy (0…86399)
  bool nocKompletna = false;       // ostatnia noc (data poranka dziś) ma rekord kompletny (R-DANE-04)
  bool sprawdzenie11 = false;      // sprawdzenie o 11:00 tej doby już było (R-DANE-24, R-DANE-15; prototyp: NVS ab_11)
  bool e01 = false;                // AirBridge nie odpowiada od ostatniego udanego odczytu (StanAB::bladOd != 0)
  int nocyBezDanych = 0;           // noce bez danych z rzędu (nocyBezDanych(), liczenie jak ekran S5, R-AW-32)
  bool logowanieOdrzucone = false; // ostatnia próba skończyła się odrzuconym logowaniem (R-AW-34)
  bool zrodloOtoczenia = false;    // lokalne źródło otoczenia skonfigurowane (pogoda przed slotem, R-DANE-16, R-DANE-33)
  // (5.8.8): ekrany CO2 włączone w tym egzemplarzu (src/slot_dzienny.h
  // co2Wlaczone(): źródło otoczenia i brak `co2=nie`); ocena slotu wieczornego z Wi-Fi (POWOD_SLOT_WIECZORNY) tylko przy
  // zrodloOtoczenia && co2, więc egzemplarz bez CO2 nie ma wieczorem ani jednego wybudzenia z Wi-Fi więcej
  // bez tego (egzemplarz bez CO2) ocena co 30 min w tym samym oknie bez Wi-Fi (POWOD_SLOT_WIECZORNY_BEZ_SIECI)
  bool co2 = true;
  Ekran ekran = Ekran::DZIEN;      // ekran na panelu (powrót R-PRZ-02, src/nawigacja.h sekundDoPowrotu())
  long sekundOdNacisniecia = 0;    // od ostatniego naciśnięcia (R-PRZ-02)
  int terminPytania = -1;          // minuta doby terminu odpowiedzi na pytanie poranne dziś (R-PIE-13), -1 = brak
  // Ostatnia obsłużona chwila terminów: terminy siatki z chwilą ≤ obsluzoneDo
  // wykonało już jakieś wybudzenie (zegarem albo start z pobraniem). Dyrygent trzyma ją w RTC (StanRtc::obsluzoneDo).
  bool obsluzoneZnane = false;     // false (pierwszy start bez zegara, cofnięty zegar): tolerancja TOLERANCJA_TERMINU_S
  int obsluzoneDo = -1;            // sekunda doby dziś (0…86399); -1 = przed dzisiejszą północą
  // R-EKR-60 : sekunda doby dziś ostatniego narysowania ekranu dnia ze stopką R-EKR-10 (nie S5, S10, S9);
  // -1 = dziś nie rysowany albo ostatnio narysowany ekran dnia bez przerysowań (S5, S10, ekran nocny).
  int dzienRysowany = -1;
  // Slot dzienny (R-KON-41, R-KON-42, R-KON-52; 5.8.12). Chwile to sekundy doby dziś (0…86399); -1 = brak dziś.
  // Wartości domyślne (wszystko -1, bez miasta) dają plan bez slotu dziennego, czyli plan bez slotu dziennego (wariant porównawczy).
  // koniecTerapii: koniec ostatniej sesji ostatniej nocy z raportu, gdy wypadł dziś; -1 = nieznany albo przed
  // dzisiejszą północą (wtedy start slotu liczy się od samego importu).
  int koniecTerapii = -1;
  // importNocy: chwila zapisu rekordu kompletnego ostatniej nocy (data poranka dziś, R-DANE-04); -1 = dziś nie było.
  int importNocy = -1;
  // nacisniecieRano: pierwsze naciśnięcie dowolnego przycisku na ekranie dnia slotu porannego po imporcie nocy
  // (R-KON-41: slot dzienny od tej chwili); -1 = nie było. Naciśnięcie wcześniejsze niż importNocy się nie liczy.
  int nacisniecieRano = -1;
  // slotDziennyOd: chwila, w której slot dzienny tej doby już wystartował (wybudzenie z POWOD_SLOT_DZIENNY_START albo
  // naciśnięcie z R-KON-41), zapisana przez dyrygenta; -1 = jeszcze nie. Slot startuje najwyżej raz na dobę.
  int slotDziennyOd = -1;
  // miasto: miasto do pogody ustawione (R-KONF-24); bez niego bez zapytań o prognozę godzinową (R-KON-52, R-KON-53).
  bool miasto = false;
  // prognozaGodzinowa: chwila ostatniego udanego pobrania prognozy godzinowej (M-26, M-27) dziś, z pobrania porannego
  // R-DANE-16 albo z R-KON-52; -1 = dziś nie było (prognoza z wczoraj jest zawsze starsza niż 3 h w slocie dziennym).
  int prognozaGodzinowa = -1;
  // nocBezTerapii: AirBridge odpowiada, a ostatniej nocy nie ma na liście nocy (E-10, R-DANE-20); z e01 (E-01) daje
  // slot dzienny od 11:00 bez importu (R-KON-41, przypadek graniczny „noc bez terapii albo brak nocy”).
  bool nocBezTerapii = false;
  // slotDzienny: false = plan bez slotu dziennego (wariant porównawczy). Tylko jawny wariant testowy symulatora;
  // na panelu zawsze true.
  bool slotDzienny = true;
  // początek okna slotu wieczornego (minuta doby, slot_wieczorny::oknoZHistorii; -1 = okno puste) i niedziela
  // (data poranka dziś) dla slotu tygodniowego
  int wieczorOd = 21 * 60;
  bool niedziela = false;
  // Szybszy wynik nocy rano (R-DANE-61, R-DANE-62). zwyklyKoniec: zwykła pora końca terapii (minuta doby,
  // zwyklyKoniecTerapii()); -1 = mniej niż 3 noce ze znanym końcem, czyli bez gęstego okna.
  int zwyklyKoniec = -1;
  // przetwarzaOd: chwila (sekunda doby dziś) ostatniego odczytu `/api/status` z przetwarzaniem plików po terapii
  // (StanRtc::przetwarzaEp); -1 = ostatni odczyt bez przetwarzania albo nie dziś.
  int przetwarzaOd = -1;
  // powtorekDzis: powtórki R-DANE-62 wykonane dziś (StanRtc::powtorki z dobą powtorkiDoba).
  int powtorekDzis = 0;
};

// (R-KON-41, R-EKR-60, R-KON-09): koniec slotu dziennego i przerysowań R-EKR-60 (minuta doby) = początek okna
// slotu wieczornego `wieczorOd` (WejsciePlanu::wieczorOd, slot_wieczorny::oknoZHistorii()); okno przed 21:00 kończy
// slot wcześniej (okno od 19:00 → koniec 19:00, przerysowania 13:00, 15:00, 17:00), okno od 21:10 → 21:10; okno puste
// (-1) albo poza dobą: 23:30, początek slotu nocnego (R-EKR-60: „przy pustym oknie do 23:30”). PLAN_SLOT_DZIENNY_DO
// zostaje jako okno stałe R-KON-17 (wartość domyślna wieczorOd). Niedziela 17:00 (slot tygodniowy).
int koniecSlotuDziennego(int wieczorOd);

struct Termin {
  uint32_t sekund = 0;   // czas snu od teraz do terminu, z zapasem (ZAPAS_POLGODZINY_S albo ZAPAS_POWROTU_S)
  Powody powody = 0;   // wszystkie powody, które przypadają na tę chwilę (bity POWOD_*)
};

// Najbliższy termin wybudzenia po chwili `w.sekundaDoby` (T-WYM-28, R-NF-02). Przyjmuje stan planu. Zwraca czas snu
// i powody. Gdy obsluzoneZnane i obsluzoneDo leży po teraz (zegar cofnięty przez NTP w tym wybudzeniu, najwyżej
// KOREKTA_NTP_S), terminy do obsluzoneDo włącznie są już wykonane i się nie liczą. Terminy są chwilami siatki (pełna minuta doby, terminy z dnia następnego po północy); termin równy albo
// wcześniejszy niż teraz się nie liczy. Czas snu do chwili siatki to różnica + ZAPAS_POLGODZINY_S (5 s), do powrotu
// sekundDoPowrotu() + ZAPAS_POWROTU_S (2 s). Powody tej samej chwili łączy bitowo. Terminy (minuty doby):
//  - POWOD_OKNO_PORANNE: 05:00 zawsze (R-DANE-02, R-EKR-37); 05:30…10:30 co 30 min, dopóki !nocKompletna;
//  - POWOD_SPRAWDZENIE_11: 11:00, dopóki !sprawdzenie11;
//  - POWOD_PONOWIENIE: po 11:00 do 23:30 włącznie co 30 min, dopóki !nocKompletna; gdy e01 i nocyBezDanych >= 2
//    i !logowanieOdrzucone, tylko 14:00, 17:00, 20:00, 23:00 (R-DANE-18; 23:30 wtedy bez tego powodu);
//  - POWOD_POGODA: 21:00, gdy zrodloOtoczenia;
//  - POWOD_POCZATEK_NOCY: 23:30 zawsze;
//  - POWOD_CZUJNIKI: co 15 min w oknie 23:00–06:00 (23:00, 23:15, …, 05:45; oknoSypialni());
//  - POWOD_POWROT: gdy sekundDoPowrotu(ekran, sekundOdNacisniecia, minuta) >= 0 (w slocie nocnym nigdy, R-PRZ-18);
//  - POWOD_PYTANIE: terminPytania, gdy jest później niż teraz.
//  - POWOD_SLOT_WIECZORNY: co 30 min od wieczorOd do 23:30 (bez), gdy zrodloOtoczenia i co2 (z Wi-Fi);
//    POWOD_SLOT_WIECZORNY_BEZ_SIECI: te same minuty bez Wi-Fi, gdy nie ma POWOD_SLOT_WIECZORNY (R-KON-01);
//  - POWOD_ODSWIEZENIE: gdy ekran == DZIEN i dzienRysowany >= 0: minuta rysowania + 120, najwcześniej 11:00, gdy
// wypada przed koniecSlotuDziennego(wieczorOd) (R-EKR-60, początek okna wieczornego, puste okno 23:30) i w tej
// minucie nie ma powodu sieciowego; tylko poza slotem dziennym (od poczatekSlotuDziennego do
//    koniecSlotuDziennego() bez tego powodu, R-EKR-60);
// - POWOD_SLOT_DZIENNY_START: minuta, w której wypada poczatekSlotuDziennego(w) (sekundy pomijane), gdy slot
//    jeszcze nie wystartował (slotDziennyOd < 0 albo slot bez nocy sprzed importu) i start nie wynika z naciśnięcia
//    (R-KON-41); start później niż PLAN_START_Z_OCENA_S przed pełną godziną: w minucie tej godziny, razem z oceną;
// - POWOD_SLOT_DZIENNY_GODZINA: każda pełna godzina po początku slotu dziennego, przed koniecSlotuDziennego
//    (np. start 07:55 → 08:00…20:00; R-KON-42);
// - slot bez nocy (R-KON-41, przypadek graniczny): bez importu i bez nocy kompletnej, przy nocBezTerapii (E-10) albo e01 (E-01) start
//    o 11:00 (PLAN_OKNO_DO); przy E-10 START i GODZINA jak wyżej (z `/api/status`); przy E-01 zamiast nich
//    POWOD_SLOT_DZIENNY_BEZ_SIECI (start i każda pełna godzina bez Wi-Fi), a Wi-Fi tylko z PONOWIENIE (R-DANE-18);
// - POWOD_PROGNOZA_GODZINOWA: w minucie POWOD_SLOT_DZIENNY_START, gdy miasto i prognozaGodzinowa < 0 albo jest
//    starsza niż PLAN_PROGNOZA_WAZNA_S w chwili startu; także przy lokalnym źródle otoczenia (R-KON-52); przy starcie
//    naciśnięciem (wybudzenie przyciskiem bez Wi-Fi) w pierwszej ocenie POWOD_SLOT_DZIENNY_GODZINA po naciśnięciu.
// Bez zegara: {OKRES_SNU_S, POWOD_CZAS}. Stan doby (nocKompletna, sprawdzenie11, e01, dzienRysowany) dotyczy dziś; terminy po
// północy liczy jak dla nowej doby bez danych (05:00 i czujniki). Zmiana czasu letniego w czasie snu: bez korekty.
// Termin zaległy: gdy powodyZalegle(w) != 0, czyli termin z powodem do wykonania wypadł po ostatniej
// obsłużonej chwili, a przed teraz (np. w trakcie czuwania po przycisku albo okna serwisowego), zwraca
// {ZALEGLY_TERMIN_S, powodyZalegle(w)}: następne wybudzenie zegarem zaraz, z tymi powodami w powodyTeraz().
Termin nastepnyTermin(const WejsciePlanu& w);

// (R-DANE-61, R-DANE-62) w nastepnyTermin():
//  - POWOD_OKNO_GESTE: co PLAN_GESTE_CO_MIN w gęstym oknie (wGestymOknie()), dopóki !nocKompletna, w minutach bez
//    POWOD_OKNO_PORANNE (siatka 30 min zostaje jak była); nie przy e01 (ostatnia próba bez odpowiedzi AirBridge albo
//    Wi-Fi, decyzja właściciela projektu 10.11.2026; także odrzucone logowanie), wraca po pierwszym udanym odczycie;
//  - POWOD_POWTORKA: chwila terminPowtorki() (sekunda, nie minuta siatki), gdy wypada przed najbliższą chwilą siatki;
//    termin powtórki, który już minął (np. w czuwaniu po przycisku), daje sen ZALEGLY_TERMIN_S z tym powodem;
//    powodyTeraz() nie daje POWOD_POWTORKA, gdy w tym samym przedziale jest POWOD_OKNO_PORANNE albo POWOD_OKNO_GESTE
//    (powtórka nie zużywa wtedy licznika).
// Powody bieżącego wybudzenia zegarem (dziennik wybudzeń, decyzja o Wi-Fi). Przyjmuje stan planu z chwili wybudzenia
// (przed pobraniem). Zwraca bity tych terminów z reguł nastepnyTermin(), których chwila wypada w przedziale
// (obsluzoneDo, teraz] (przy obsluzoneZnane i obsluzoneDo < teraz; po północy od 00:00), inaczej
// [teraz − TOLERANCJA_TERMINU_S, teraz], oraz POWOD_POWROT, gdy sekundDoPowrotu() == 0. Bez zegara: POWOD_CZAS.
// 0 = wybudzenie bez powodu (do logu; dyrygent nie włącza wtedy Wi-Fi i zasypia do nastepnyTermin()).
// (R-KON-41, przypadek „panel uśpiony w chwili startu”): POWOD_SLOT_DZIENNY_START także wtedy, gdy chwila startu
// minęła poza tym przedziałem, slot nie wystartował (slotDziennyOd < 0), a teraz jest przed koniecSlotuDziennego().
Powody powodyTeraz(const WejsciePlanu& w);

// Początek slotu dziennego dziś (R-KON-41, 5.8.12). Przyjmuje stan planu. Zwraca -1 przy !slotDzienny. Zwraca
// sekundę doby: slotDziennyOd, gdy slot już wystartował (slot bez nocy z E-01 sprzed importu nocy się nie liczy: po
// imporcie slot startuje od nowa 60 min po nim); inaczej nacisniecieRano, gdy jest i nie jest wcześniejsze niż importNocy; inaczej
// późniejszą z chwil koniecTerapii, importNocy plus PLAN_SLOT_DZIENNY_PO_S (bez końca terapii albo z końcem spoza
// 0…86399: importNocy + 60 min).
// Bez importu, przy !nocKompletna i nocBezTerapii albo e01 (noc bez terapii albo brak nocy): PLAN_OKNO_DO * 60
// (11:00); noc kompletna bez znanej chwili importu (np. po zaniku zasilania) nie daje slotu. Zwraca -1 bez zegara,
// bez importu nocy dziś (poza E-01 i E-10), przy chwilach spoza 0…86399 i gdy start wypadałby w
// koniecSlotuDziennego() albo później (slot dzienny trwa tylko do początku slotu wieczornego).
int poczatekSlotuDziennego(const WejsciePlanu& w);

// Terminy zaległe: powody terminów siatki z chwilą w (obsluzoneDo, teraz], które trzeba wykonać
// mimo spóźnienia: sieciowe (sieciowy()) i POWOD_PYTANIE; bez CZUJNIKI (odczyt dochodzi w kolejnym terminie) i bez
// POWROT, bez ODSWIEZENIE (przerysowanie, które przepadło, zastępuje najbliższe rysowanie ekranu dnia; bez pętli
// snu 1 s). Przyjmuje stan planu z chwili zasypiania i obsłużoną chwilę tego wybudzenia. Zwraca 0 bez zegara, bez
// obsluzoneZnane albo przy obsluzoneDo >= teraz (cofnięty zegar). Każdy termin wychodzi najwyżej raz, bo następne
// wybudzenie zegarem przesuwa obsłużoną chwilę na swój początek.
Powody powodyZalegle(const WejsciePlanu& w);

// Czy powody wymagają Wi-Fi (R-NF-03): OKNO_PORANNE, SPRAWDZENIE_11, PONOWIENIE, POGODA, POCZATEK_NOCY, CZAS,
// OKNO_GESTE i POWTORKA (R-DANE-61, R-DANE-62);
// także SLOT_DZIENNY_START i SLOT_DZIENNY_GODZINA (`/api/status`, R-KON-42) i PROGNOZA_GODZINOWA (R-KON-52).
// CZUJNIKI, POWROT, PYTANIE, ODSWIEZENIE i SLOT_DZIENNY_BEZ_SIECI same nie włączają Wi-Fi (R-DANE-33, T-WYM-31, T-WYM-24, R-EKR-60).
bool sieciowy(Powody powody);

// Pobranie ograniczone do odczytu stanu aparatu (R-KON-42, 5.8.12): true, gdy sieciowy(powody), a jedynymi
// powodami sieciowymi są SLOT_DZIENNY_START, SLOT_DZIENNY_GODZINA i PROGNOZA_GODZINOWA. Dyrygent wtedy w sesji Wi-Fi
// robi tylko jedno `GET /api/status` (bez listy nocy, raportów i zaległych dób) i, przy skonfigurowanym źródle
// otoczenia, jego odczyt w tym samym połączeniu (R-DANE-33). Zwraca false bez powodu sieciowego.
bool tylkoStatus(Powody powody);

// Wybudzenie zegarem, które tylko przerysowuje ekran dnia bez nowych danych nocy (R-EKR-60; także ocena slotu
// dziennego R-KON-41, R-KON-42): powody zawierają ODSWIEZENIE, SLOT_DZIENNY_START, SLOT_DZIENNY_GODZINA albo
// SLOT_DZIENNY_BEZ_SIECI i poza nimi najwyżej CZUJNIKI i PROGNOZA_GODZINOWA. Dyrygent bierze wtedy tryb (karta albo
// ekran danych) i linię uwagi z ostatniego narysowania (RTC), a e-ink odświeża tylko przy zmianie klatki. 0 → false.
bool przerysowanieDnia(Powody powody);

// Start slotu dziennego naciśnięciem (R-KON-41): naciśnięcie dowolnego przycisku na ekranie dnia (`przed`, ekran
// przed naciśnięciem) po imporcie nocy dziś. Przyjmuje stan planu z chwili naciśnięcia. Zwraca true przy zegarze,
// przed == DZIEN, importNocy w dobie, slotDziennyOd < 0 (slot jeszcze nie wystartował), teraz ≥ importNocy, teraz
// przed koniecSlotuDziennego() i poza slotem nocnym. Dyrygent zapisuje wtedy chwilę naciśnięcia jako nacisniecieRano
// i slotDziennyOd (StanRtc::nacisniecieRanoEp, slotDziennyEp).
bool startSlotuPrzyciskiem(const WejsciePlanu& w, Ekran przed);

// Gęste okno (R-DANE-61). Przyjmuje stan planu i minutę doby. Zwraca true, gdy zwyklyKoniec >= 0 i minuta leży
// w [zwyklyKoniec − PLAN_GESTE_PRZED_MIN, zwyklyKoniec + PLAN_GESTE_PO_MIN] i w oknie porannym (PLAN_OKNO_OD,
// PLAN_OKNO_DO) bez jego końców (05:00 jest zawsze terminem okna porannego, 11:00 sprawdzeniem R-DANE-24).
// Nie patrzy na nocKompletna (to robi plan przy terminach).
bool wGestymOknie(const WejsciePlanu& w, int minuta);

// Termin powtórki przy przetwarzaniu (R-DANE-62). Przyjmuje stan planu. Zwraca sekundę doby przetwarzaOd +
// PLAN_POWTORKA_S, gdy jest zegar, teraz przed 11:00, przetwarzaOd w oknie porannym (od 05:00), termin przed 11:00,
// noc niekompletna
// i powtorekDzis < PLAN_POWTORKI_MAKS; inaczej -1 (wtedy zwykła siatka: gęste okno albo 30 min).
int terminPowtorki(const WejsciePlanu& w);

// Koniec terapii nocy w minutach od 12:00 dnia wieczoru (skala M-18, R-KON-09) albo -1. Rekord nocy nie ma końca
// ostatniej sesji, więc przybliżenie [DOMYSŁ]: M-18 (start najdłuższej sesji, R-DANE-27) + M-01 (minuty z maską):
// dokładne dla nocy z jedną sesją; przy kilku sesjach przesunięte o (czas sesji przed najdłuższą − przerwy po niej).
// -1: m18 albo m01 nieznane, m01 < 15 min (jak noc z terapią w R-KON-09) albo noc częściowa.
int koniecTerapiiNocy(const Noc& n);

// Zwykła pora końca terapii (R-DANE-61): mediana koniecTerapiiNocy() z ostatnich PLAN_GESTE_NOCY nocy z terapią
// do nocy z poranka `dzis` wyłącznie (ta jeszcze nie przyszła), z co najmniej PLAN_GESTE_NOCY_MIN takich nocy.
// Zwraca minutę doby (0…1439; koniec po 12:00 następnego dnia daje minutę ≥ 12:00, czyli gęste okno puste) albo -1.
int zwyklyKoniecTerapii(const std::vector<Noc>& hist, const std::string& dzis);

// Pobranie przy wybudzeniu przyciskiem rano bez nocy (R-PRZ-22, D). Przyjmuje stan planu i ile sekund temu skończyło
// się poprzednie nieprzerwane takie pobranie (-1 = nie było albo chwila nieznana). Zwraca true przy zegarze, w oknie
// porannym [05:00, 11:00), przy nocy niekompletnej, bez E-01 i odOstatniego < 0 albo
// >= PLAN_POBRANIE_PRZYCISKIEM_CO_S. Dyrygent pobiera wtedy przerywalnie po wykonaniu akcji przycisku, z poranną
// decyzją o pytaniu, tylko gdy po akcji stoi ekran dnia albo strona kokpitu `n / 6`
// i akcja sama nie pobierała, bez pierwszej konfiguracji, sieci panelu, serwera i okna serwisowego.
bool pobranieRanoPrzyciskiem(const WejsciePlanu& w, long odOstatniego);

// Pobranie po wybudzeniu zegarem (zastępuje dla zegara pobranieNaStarcie(ZEGAR) z kroku 1, T-WYM-28, R-NF-03).
// Przyjmuje powodyTeraz(). Zwraca pobierz = sieciowy(powody), nieprzerywalne, poranekCzynnikow = pobierz albo
// POWOD_PYTANIE (termin odpowiedzi bez Wi-Fi: „brak odpowiedzi” zapisany w tym wybudzeniu, R-CZY-03, R-PIE-13).
Pobranie pobranieZegarem(Powody powody);

// Wybudzenie pomiarowe (T-WYM-31, spec techniczna 7.3): wybudzenie zegarem bez innego powodu niż zapis czujników
// (także bez żadnego powodu, powody == 0). Przyjmuje powód wybudzenia i powodyTeraz(). Zwraca true tylko dla ZEGAR
// z powodami ⊆ POWOD_CZUJNIKI: dyrygent wtedy nie włącza Wi-Fi i nie rysuje, tylko zapisuje odczyt i zasypia.
bool wybudzeniePomiarowe(PowodWybudzenia p, Powody powody);

// Czas czuwania na przycisk po rysowaniu (2.2 wniosek 4, 7.1 krok 13, T-WYM-20). Przyjmuje powód wybudzenia i czy
// okno serwisowe jest otwarte. Zwraca OKNO_SERWISOWE_MS przy oknie, CZUWANIE_MS po wybudzeniu przyciskiem, inaczej 0
// (zegar i start: bez czuwania, przycisk budzi panel z uśpienia).
uint32_t czuwanieMs(PowodWybudzenia p, bool oknoSerwisowe);

// Plan snu z terminu (T-WYM-28, T-WYM-29). Przyjmuje termin z nastepnyTermin() i bity przycisków wciśniętych w chwili
// zasypiania. Zwraca budzenie zegarem po t.sekund i budzenie przyciskami niewciśniętymi (wciśnięty = zacięty).
PlanSnu planSnuZTerminu(const Termin& t, uint8_t wcisniete);

// Nazwy powodów do logu i /bateria.csv: nazwy z komentarzy POWOD_* w kolejności bitów, złączone „+”
// (np. „okno_poranne+czujniki”); 0 → "". także nazwy bitów slotu dziennego (np. „slot_dzienny+prognoza_godzinowa”).
std::string nazwyPowodow(Powody powody);

// Pole `powod` wiersza /bateria.csv (src/bateria.h): START → „start”, PRZYCISK → „przycisk”, ZEGAR → „zegar:” +
// nazwyPowodow(powody) (np. „zegar:czujniki”; bez powodu „zegar:”). Log doby: każde wybudzenie z powodem (13.1).
std::string powodWpisuBaterii(PowodWybudzenia p, Powody powody);

// Wejście planu z historii i czynników, żeby dyrygent tylko przepisywał wyniki.
// nocKompletnaDzis(): rekord nocy z datą poranka `dzis` z AirBridge (źródło 'A') i nie częściowy albo doba tej nocy
// (nrDnia(dzis) − 1) pominięta na stałe (`/ab_pominiete.txt`), jak „ostatnia noc jest” w src/airbridge.h
// potrzebnaLista(); zła albo pusta data → false.
bool nocKompletnaDzis(const std::vector<Noc>& hist, const std::string& dzis, const std::set<long>& pominiete);
// terminPytaniaDzis(): termin odpowiedzi (minuta doby, R-PIE-13) wpisu czynników z dziś, gdy wpis czeka (stan 'C')
// i termin jest w dobie (0…1439); inaczej -1 (także bez wpisu).
int terminPytaniaDzis(const WpisCzynnikow* wpis);

// Noce bez danych z rzędu do R-DANE-18, liczone tak samo jak ekran S5 (R-AW-32: od ostatniego udanego odczytu
// AirBridge, odczyt przed 05:00 nie zna nocy z tego dnia; noc bez terapii przy działającym AirBridge się nie liczy):
// jedna funkcja src/noc.h nocyBezDanychZRzedu dla obu. Przyjmuje, czy E-01 trwa, datę dziś,
// datę i minutę doby ostatniego udanego odczytu (StanAB::pobrano; "" = nigdy, minuta -1 = nieznana), datę pierwszej
// nieudanej próby (StanAB::bladOd) i historię nocy (nullptr = bez). Zwraca 0 bez E-01, inaczej nocyBezDanychZRzedu()
// (0…1000; zła data → 1, czyli ostrożnie ponawianie co 30 min).
int nocyBezDanych(bool e01, const std::string& dzis, const std::string& dataOdczytu, int minutaOdczytu,
                  const std::string& dataBledu, const std::vector<Noc>* hist);
// Skrót bez minuty odczytu i bez historii (testy autora testów kroku 2): minuta nieznana, czyli odczyt jak po 05:00.
int nocyBezDanych(bool e01, const std::string& dzis, const std::string& dataOdczytu, const std::string& dataBledu);
