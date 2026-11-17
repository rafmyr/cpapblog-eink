// SPDX-License-Identifier: GPL-3.0-or-later
// Układ ekranów z telefonu (ADR-0045, decyzja właściciela projektu 14.11.2026, spec 5.6.6 „Ekrany”, R-KOK-34…R-KOK-36,
// R-KON-70…R-KON-72, R-KONF-60): kolejność stron kokpitu z numerem i ukrycie strony sypialni, kolejność i włączenie
// ekranów dziennych slotu dziennego. Bez Arduino; klucze w `/konfiguracja.txt` (5.6.4):
//   kokpit.kolejnosc = ostatnia_noc,7_nocy,sypialnia      kolejność trzech stron z numerem (U-01 zawsze ostatnia)
//   kokpit.ukryte    = sypialnia                          strony ukryte; wolno tylko `sypialnia`
//   dzien.kolejnosc  = wietrzenie,jutro_rano,wymiana,pylek pierwszeństwo czterech ekranów dziennych D1…D4
//   dzien.ukryte     = wietrzenie,jutro_rano,wymiana,pylek,dzien_i_noc,obrazek   ekrany dzienne wyłączone
// Brak wiersza = układ domyślny (zachowanie sprzed 14.11.2026); formularz przy układzie domyślnym usuwa wiersz
// (R-KONF-20). Wersja formatu: nazwa klucza (jak `okres.filtr_tyg`): inne znaczenie wartości dostanie nowy klucz.
// Wartość uszkodzona: nazwy nieznane, powtórzone albo ekranu zawsze włączonego pominięte z wpisem w `bledy`, reszta
// działa; brakujące w kolejności strony i ekrany dochodzą na końcu w kolejności domyślnej (nigdy pusty kokpit, nigdy
// ukryty ekran ostrzegawczy). Ekranów zawsze włączonych (ekrany problemu, lekarz, brak danych, synchronizacja,
// bateria, błędy E, pytania, `Przewietrz teraz.`, drzemka z wyciekiem, K1, K2, sprawy, U-01) nie ma czym wyłączyć.
#pragma once
#include <string>
#include <vector>

#include "nawigacja.h"

// Ekrany dzienne z przełącznikiem (7.2 część „Dzień”). Pierwsze RODZAJOW_PRZESTAWNYCH mają kolejność (D1…D4);
// S-K-37 i S-K-38 zostają na swoich miejscach: D5 i zastępstwo D5 (R-KON-54); od 16.11.2026 bez pierwszego bloku slotu
// (R-KON-50 wycofane), a S-K-38 jest też wygaszaczem (R-KON-73). `obrazek` w `dzien.ukryte` ukrywa obie role; sam
// wygaszacz wyłącza osobny klucz `dzien.wygaszacz=nie` (R-KON-77, src/slot_dzienny.h).
enum class RodzajDzienny : unsigned char { WIETRZENIE = 0, JUTRO_RANO = 1, WYMIANA = 2, PYLEK = 3, DZIEN_I_NOC = 4,
                                           OBRAZEK = 5 };
const int RODZAJOW_DZIENNYCH = 6;
const int RODZAJOW_PRZESTAWNYCH = 4;

struct UkladDnia {
  RodzajDzienny kolejnosc[RODZAJOW_PRZESTAWNYCH] = {RodzajDzienny::WIETRZENIE, RodzajDzienny::JUTRO_RANO,
                                                     RodzajDzienny::WYMIANA, RodzajDzienny::PYLEK};
  bool pokaz[RODZAJOW_DZIENNYCH] = {true, true, true, true, true, true};
  bool pokazuj(RodzajDzienny r) const { return pokaz[static_cast<int>(r)]; }
  bool domyslny() const;
  int wlaczonych() const;  // liczba ekranów dziennych z przełącznikiem „pokazuj” włączonym
};

struct UkladEkranow {
  UkladKokpitu kokpit;
  UkladDnia dzien;
};

// identyfikatory w pliku i nazwy w formularzu
extern const char* const ID_STRONY[STRON_PRZESTAWNYCH];         // `ostatnia_noc`, `7_nocy`, `sypialnia`
extern const char* const NAZWA_STRONY[STRON_PRZESTAWNYCH];      // `Ostatnia noc`, `7 nocy`, `Sypialnia · 30 nocy`
extern const char* const ID_DZIENNY[RODZAJOW_DZIENNYCH];        // `wietrzenie`, `jutro_rano`, `wymiana`, `pylek`, …
extern const char* const NAZWA_DZIENNEGO[RODZAJOW_DZIENNYCH];   // `Wietrzenie`, `Jutro rano`, …
extern const char* const KLUCZ_KOKPIT_KOLEJNOSC;  // `kokpit.kolejnosc`
extern const char* const KLUCZ_KOKPIT_UKRYTE;     // `kokpit.ukryte`
extern const char* const KLUCZ_DZIEN_KOLEJNOSC;   // `dzien.kolejnosc`
extern const char* const KLUCZ_DZIEN_UKRYTE;      // `dzien.ukryte`

// Wiersz `klucz=wartość` z pliku konfiguracji: true, gdy klucz należy do układu (wartość zastosowana w `u`, części
// złe pominięte z jednym wpisem na klucz w `bledy`: `kokpit.ukryte: 7_nocy zawsze włączone`, przy kilku nazwach liczba
// i najwyżej 3 przykłady (`kokpit.ukryte: pominięte 3 nazwy: ostatnia_noc zawsze włączone, …`); wartość czytana
// najwyżej do 160 B; false = inny klucz, `u` bez zmian.
bool wierszUkladu(const std::string& klucz, const std::string& wartosc, UkladEkranow& u, std::vector<std::string>& bledy);
// Wartości do zapisu w pliku; "" = wartość domyślna (wiersz usuwany, R-KONF-20).
std::string kolejnoscKokpituTekst(const UkladKokpitu& u);
std::string ukryteKokpituTekst(const UkladKokpitu& u);
std::string kolejnoscDniaTekst(const UkladDnia& u);
std::string ukryteDniaTekst(const UkladDnia& u);
