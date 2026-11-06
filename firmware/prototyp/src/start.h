// SPDX-License-Identifier: GPL-3.0-or-later
// Siatka aktualizacji (krok 1, spec techniczna 7.8 „Powrót do poprzedniej wersji” i „Pętla restartów
// i zawieszenie”, 13.1): licznik startów po nieprawidłowym zakończeniu (T-WYM-82), tryb awaryjny U-06 (T-WYM-83,
// R-AW-39), samosprawdzenie nowej wersji przed pierwszym snem (T-WYM-41) i ustawienia strażnika zadania (T-WYM-81).
// Bez Arduino i bez ESP-IDF: przyczynę startu, stan wersji i pamięć RTC podaje adapter (src/adaptery.h).
// Testy: test/testy_wybudzenie.cpp.
#pragma once
#include <cstdint>
#include <string>
#include "nawigacja.h"

// Przyczyna startu z esp_reset_reason() (adapter mapuje esp_reset_reason_t jeden do jednego, reszta INNA).
enum class PrzyczynaStartu : uint8_t {
  NIEZNANA,     // ESP_RST_UNKNOWN
  ZASILANIE,    // ESP_RST_POWERON
  ZEWNETRZNY,   // ESP_RST_EXT
  PROGRAMOWY,   // ESP_RST_SW: restart po wgraniu nowej wersji i jawny powrót do poprzedniej (T-WYM-41)
  PANIKA,       // ESP_RST_PANIC: błąd programu
  INT_WDT,      // ESP_RST_INT_WDT: strażnik przerwań
  TASK_WDT,     // ESP_RST_TASK_WDT: strażnik zadania (T-WYM-81)
  WDT,          // ESP_RST_WDT: inny strażnik
  USPIENIE,     // ESP_RST_DEEPSLEEP: wybudzenie z głębokiego uśpienia
  SPADEK,       // ESP_RST_BROWNOUT: spadek napięcia
  INNA,         // ESP_RST_SDIO, ESP_RST_USB, ESP_RST_JTAG, ESP_RST_EFUSE, ESP_RST_PWR_GLITCH, ESP_RST_CPU_LOCKUP i inne
};

// Liczba startów po nieprawidłowym zakończeniu, przy której panel przechodzi w tryb awaryjny (R-AW-39) [PRÓG].
const uint8_t STARTOW_DO_AWARYJNEGO = 3;

// Licznik w pamięci RTC, która przeżywa restart programu (RTC_NOINIT_ATTR). Po włączeniu zasilania zawartość jest
// przypadkowa: ważność daje magia, wersja firmware i suma kontrolna. Pola ustawia tylko rdzeń (decyzjaStartu(),
// zerujPrzedSnem(), wyjscieZAwaryjnego()); adapter trzyma blok i niczego w nim nie liczy. Typ prosty, bez
// inicjalizatorów pól: zmienna RTC_NOINIT_ATTR nie może mieć konstruktora, który nadpisze ją przy starcie.
struct LicznikStartow {
  uint32_t magia;
  uint32_t wersja;      // identyfikator wersji firmware (skrót napisu wersji)
  uint8_t poBledzie;    // kolejne starty po nieprawidłowym zakończeniu bez zaśnięcia pomiędzy
  uint16_t spadki;      // starty po spadku napięcia (tylko log i ekran testu)
  uint32_t suma;        // suma kontrolna pól powyżej
};

// Znacznik trybu awaryjnego w NVS (przeżywa zanik zasilania, T-WYM-83). Przestrzeń i klucz w NVS: NVS_PRZESTRZEN,
// NVS_KLUCZ_AWARYJNY; wartość jako tekst z zapiszZnacznik().
const char* const NVS_PRZESTRZEN = "cpap";
const char* const NVS_KLUCZ_AWARYJNY = "awaryjny";
struct ZnacznikAwaryjny {
  bool jest = false;
  uint32_t wersja = 0;                                   // wersja firmware, która weszła w tryb
  PrzyczynaStartu przyczyna = PrzyczynaStartu::NIEZNANA;  // przyczyna startu przy wejściu
  bool u06Narysowane = false;                            // U-06 narysowane (zapis przed rysowaniem)
};
// Zapis znacznika do tekstu NVS. Zwraca tekst, który parsujZnacznik() czyta z powrotem bez straty.
std::string zapiszZnacznik(const ZnacznikAwaryjny& z);
// Odczyt znacznika z tekstu NVS. Pusty albo nieczytelny tekst → znacznik bez `jest` (zwraca false).
bool parsujZnacznik(const std::string& tekst, ZnacznikAwaryjny& z);

// Identyfikator wersji firmware do magii licznika i znacznika: skrót napisu wersji (np. "1.0.0-beta.1"). Ten sam
// napis daje ten sam wynik, różne napisy praktycznie zawsze różny; nigdy 0.
uint32_t identyfikatorWersji(const char* wersja);
// Numer wydania z identyfikatora wersji (`{WERSJA}+{skrót commitu}`, spec techniczna 8.7): część przed `+`, np.
// "1.0.0+abc1234" → "1.0.0" (na U-05; skrót commitu tylko w logu). Bez `+` cały napis; nullptr → "".
std::string wersjaWydania(const char* identyfikator);

// Stan bieżącej wersji z esp_ota_get_state_partition() (adapter): DOBRA = ESP_OTA_IMG_VALID albo bez danych OTA
// (wgranie kablem), DO_SPRAWDZENIA = ESP_OTA_IMG_PENDING_VERIFY.
enum class StanWersji : uint8_t { DOBRA, DO_SPRAWDZENIA };

struct DecyzjaStartu {
  bool awaryjny = false;        // ten start idzie w trybie awaryjnym (T-WYM-83), nie zwykłe wybudzenie
  bool rysujU06 = false;        // narysować U-06 (raz; nie przy błędzie programu po już narysowanym U-06)
  bool zapiszZnacznik = false;  // wejście w tryb: jeden zapis znacznika w NVS przed rysowaniem (wartość w
                                // `znacznik`, z u06Narysowane = true: zapis przy wejściu i znacznik „U-06
                                // narysowane” to ten sam jeden zapis)
  bool usunZnacznik = false;    // znacznik innej wersji firmware: usunąć, zwykły start
  ZnacznikAwaryjny znacznik;    // znacznik do zapisu przy zapiszZnacznik
  uint8_t poBledzie = 0;        // licznik po tej decyzji (do logu)
  uint16_t spadki = 0;          // licznik spadków napięcia (do logu i ekranu testu)
  bool spadekNapiecia = false;  // ten start jest po spadku napięcia (wpis w logu)
};

// Decyzja na samym początku startu, przed montowaniem plików, migracjami i Wi-Fi (T-WYM-82). Przyjmuje licznik
// z pamięci RTC (zmieniany na miejscu), przyczynę startu, identyfikator bieżącej wersji i znacznik z NVS.
// Licznik: nieważny (magia, suma) albo innej wersji, a także start po włączeniu zasilania → od zera; +1 tylko przy
// PANIKA, TASK_WDT, INT_WDT, WDT; bez zmian przy PROGRAMOWY, USPIENIE i pozostałych; SPADEK bez zmian licznika,
// +1 w `spadki`. Tryb awaryjny: licznik osiąga STARTOW_DO_AWARYJNEGO (wtedy zapis znacznika) albo znacznik tej
// samej wersji jest w NVS; znacznik innej wersji → usunąć i zwykły start (R-AW-39: wyjście przy innej wersji).
// rysujU06: przy wejściu w tryb tak; przy znaczniku z u06Narysowane i starcie po błędzie programu (PANIKA, TASK_WDT,
// INT_WDT, WDT) nie, żeby błąd w rysowaniu nie dawał pętli (T-WYM-83).
DecyzjaStartu decyzjaStartu(LicznikStartow& l, PrzyczynaStartu p, uint32_t wersja, const ZnacznikAwaryjny& z);
// To samo ze stanem bieżącej wersji: wersja „do sprawdzenia” to nowe wgranie, także
// naprawa wgrana przez okno serwisowe U-05 albo kablem, więc znacznik trybu awaryjnego traktuje jak znacznik innej
// wersji (usunąć, zwykły start), żeby naprawa nie wróciła przez tryb awaryjny do zepsutej wersji. Wersja 4-argumentowa
// = StanWersji::DOBRA.
DecyzjaStartu decyzjaStartu(LicznikStartow& l, PrzyczynaStartu p, uint32_t wersja, const ZnacznikAwaryjny& z,
                            StanWersji s);

// Tuż przed każdym zaśnięciem, także w trybie awaryjnym i po przekroczeniu limitu wybudzenia (T-WYM-82).
// Ustawia licznik startów po błędzie na 0 (ważny, z bieżącą wersją); licznik spadków zostaje.
void zerujPrzedSnem(LicznikStartow& l, uint32_t wersja);

// Ekrany trybu awaryjnego (spec funkcjonalna 6.2, 5.6.2): U-06 i U-05 otwarty z U-06.
enum class EkranAwaryjny : uint8_t { U06, U05 };
enum class AkcjaAwaryjna : uint8_t {
  NIC,
  TEST_SPRZETU,  // U-05 z oknem serwisowym (R-NF-29): jedyne Wi-Fi w trybie awaryjnym
  WROC,          // z U-05 do U-06, okno serwisowe zamknięte
  WYJDZ,         // ↻ ≥ 2 s na U-06: znacznik usunięty, licznik 0, zwykły start od nowa (R-AW-39)
  ODSWIEZ,       // ↻ na U-05: odśwież stan na ekranie (spec funkcjonalna 5.6.2, wiersz U-05)
};
// Mapa przycisków trybu awaryjnego (spec funkcjonalna 5.6.2, wiersze U-05 i U-06). Przyjmuje ekran, przycisk
// i przytrzymanie. Zwraca: U-06 ▶ → TEST_SPRZETU, ↻ 2 s → WYJDZ, reszta NIC; U-05 ◀ → WROC, ↻ krótko → ODSWIEZ,
// reszta NIC (w trybie awaryjnym bez wgrania pakietu z karty).
AkcjaAwaryjna nacisniecieAwaryjne(EkranAwaryjny e, Przycisk p, bool dlugo);

// Wyjście z trybu awaryjnego po WYJDZ: licznik od zera (ważny, bieżąca wersja). Znacznik usuwa wołający w NVS.
void wyjscieZAwaryjnego(LicznikStartow& l, uint32_t wersja);

// Teksty U-06 wkompilowane w firmware, poza zestawem danych reguł (spec funkcjonalna 6.2):
// tryb awaryjny nie wczytuje plików.
struct TekstyU06 {
  const char* tytul;
  const char* powod;
  const char* krok;
  const char* stopka;
};
TekstyU06 tekstyU06();

// ---------- samosprawdzenie i powrót do poprzedniej wersji (T-WYM-41) ----------


struct WynikSamosprawdzenia {
  bool plikiMontuja = false;   // partycja plików się montuje
  bool zestawWczytany = true;  // zestaw danych reguł się wczytuje (do kroku 5 zestawu nie ma: true)
  bool migracjeOk = true;      // migracje na nowe ścieżki z T-WYM-43 (w kroku 1 żadnych: true)
  bool ekranRysuje = false;    // ekran się narysował
  bool wifiLaczy = false;      // panel połączył się z Wi-Fi
};

enum class PoSamosprawdzeniu : uint8_t { NIC, OZNACZ_DOBRA, WROC_DO_POPRZEDNIEJ };
// Decyzja po samosprawdzeniu. Przyjmuje stan wersji i wynik. Zwraca: DOBRA → NIC; DO_SPRAWDZENIA i wszystkie pięć
// warunków spełnione → OZNACZ_DOBRA; DO_SPRAWDZENIA i którykolwiek niespełniony → WROC_DO_POPRZEDNIEJ.
PoSamosprawdzeniu decyzjaSamosprawdzenia(StanWersji s, const WynikSamosprawdzenia& w);

// Czy przy tym stanie wersji wolno zasnąć (7.8: wersja, która zaśnie bez oznaczenia, po wybudzeniu nie wraca sama).
// Zwraca false dla DO_SPRAWDZENIA.
bool wolnoZasnac(StanWersji s);

// Czy samosprawdzenie ma iść w tym wybudzeniu. Zwraca true dla DO_SPRAWDZENIA (także po wybudzeniu przyciskiem).
bool samosprawdzenieWymagane(StanWersji s);

// ---------- strażnik zadania (T-WYM-81) ----------

const uint32_t STRAZNIK_LIMIT_S = 150;  // [szacunek], dłużej niż najdłuższy krok 7.1 (120 s AirBridge)
struct UstawieniaStraznika {
  uint32_t limitS;  // limit bez odnowienia
  bool panika;      // przekroczenie kończy się błędem programu i restartem, nie samym wpisem
};
// Ustawienia strażnika zadania głównego. Zwraca {STRAZNIK_LIMIT_S, true}.
UstawieniaStraznika ustawieniaStraznika();
