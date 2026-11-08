// SPDX-License-Identifier: GPL-3.0-or-later
// Wspólne narzędzia bez zależności od Arduino: daty "RRRR-MM-DD" i formatowanie liczb.
// Kompiluje się na panelu i na komputerze (testy: firmware/prototyp/test/).
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

// ---------- daty ----------
// Numer dnia = dni od 1970-01-01 (kalendarz gregoriański, algorytm H. Hinnanta, bez time.h i strefy).
bool dataOk(const std::string& d);                  // RRRR-MM-DD, dzień w granicach miesiąca (29.02 w roku przestępnym)
long nrDnia(const std::string& d);                  // -1 dla złej daty
std::string dataZNr(long n);
std::string dodajDni(const std::string& d, int k);  // "" dla złej daty
int dzienTygodnia(const std::string& d);            // 0 = niedziela … 6 = sobota, -1 dla złej daty
int dniMiedzy(const std::string& od, const std::string& do_);  // do_ − od w dniach

extern const char* const DNI_KROTKO[7];   // "nd", "pn", "wt", "śr", "cz", "pt", "sb" (jednoznaczne, też w K2)
extern const char* const MIESIACE_D[12];  // dopełniacz: "stycznia" … "grudnia"
extern const char* const MIESIACE_DW[12]; // dopełniacz wersalikami (etykiety): "STYCZNIA" …

std::string ddmm(const std::string& d);        // "2026-10-29" → "29.10"
std::string dataZRokiem(const std::string& d); // "2024-03-06" → "6.03.2024"
std::string dzienMiesiac(const std::string& d); // "2026-10-07" → "7 października"

// ---------- liczby ----------
// Czytanie liczb z tekstu bez atoi/atof (clang-tidy cert-err34-c). Zachowanie IDENTYCZNE z atoi/atol/atof:
// puste i śmieci dają 0, prefiks „12abc” daje 12, spacje na początku są pomijane, nie ma sygnalizacji błędu.
// atoi/atol/atof w bibliotece C to dokładnie te wywołania strtol/strtod (test: test/testy_parsowanie_liczb.cpp).
inline int liczbaCalk(const char* s) { return static_cast<int>(std::strtol(s, nullptr, 10)); }
inline long liczbaDluga(const char* s) { return std::strtol(s, nullptr, 10); }
inline double liczbaRzecz(const char* s) { return std::strtod(s, nullptr); }

// Zaokrąglenie „połowa w górę” z poprawką 1e-4: wartości z pliku mają 1–2 miejsca po przecinku,
// a float gubi ostatni bit (12,7 → 12,6999…).
std::string przecinek1(double v);   // 13,55 → "13,6"; NAN → "–"
std::string calkowita(double v);    // 8,5 → "9"; NAN → "–"
std::string godzMin(double minuty); // 414 → "6:54"; NAN albo < 0 → "–"
std::string godzH(int minuty);      // 549 → "9 h 09"
bool brak(double v);                // NAN
// CRC-32 (IEEE 802.3, wielomian 0xEDB88320, jak zlib crc32()) z `n` bajtów od `d`; n = 0 albo d = nullptr → 0.
// Skrót klatki ekranu (src/dyrygent.cpp Przebieg::pokaz()). Przykład: "123456789" → 0xCBF43926.
uint32_t crc32(const uint8_t* d, size_t n);
// odmiana po liczebniku: 1 → "noc", 2–4, 22–24 … (bez 12–14) → "noce", reszta → "nocy" (S2b: `7 nocy z rzędu`)
const char* nocyZRzedu(int n);
// Indeks z tekstu z zewnątrz (plik, HTTP, NVS) przed użyciem w tablicy.
// Przyjmuje: tekst `t` i granicę `ile` (liczba elementów tablicy). Zwraca true i `wynik` w 0…ile−1 tylko
// wtedy, gdy `t` to same cyfry dziesiętne (bez znaku, spacji i reszty) o wartości mniejszej niż `ile`;
// przy błędzie false i `wynik` bez zmian. Przykład: "3", 5 → true, 3; "-1", "1x", "", "99999999999" → false.
bool indeksZTekstu(const char* t, int ile, int& wynik);
// Ile porannych nocy minęło bez udanego odczytu AirBridge (licznik E-01, R-AW-24, R-AW-32): odczyt z dnia D o 05:00
// albo później zna noc z D, więc pierwsza brakująca to D+1; odczyt przed 05:00 jeszcze jej nie zna. 0 = brak odczytu.
int nocyOdOdczytu(const std::string& dzis, const std::string& abData, int abMinuta);

// plik konfiguracji z BOM UTF-8 na początku (Notatnik) czytany jak bez niego; wiersz bez `\r`, spacji
// i tabulatorów na końcu (ręczna edycja). Dla każdego klucza `/konfiguracja.txt`.
std::string bezBom(const std::string& t);
void obetnijKoniec(std::string& l);
