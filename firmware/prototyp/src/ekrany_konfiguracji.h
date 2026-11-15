// SPDX-License-Identifier: GPL-3.0-or-later
// Ekrany konfiguracji (konfiguracja z telefonu; spec 5.6.3): U-01 „Ustawienia” (lista pozycji z kursorem) i U-03 formularz
// z kodem QR (jak ekran raportu dla lekarza, 5.13.6). Ekrany danych bez budzika; hasło nigdy na ekranie (R-KONF-38).
// Artboardów brak (5.6.3: „Ekrany nie mają jeszcze artboardów na kanwie”): układ według ekranu „Sprzęt” (U-01)
// i „Raport dla lekarza” (U-03), skala B.
#pragma once
#include <string>
#include <vector>

#include "aktualizacja.h"
#include "ekrany.h"

// U-01 (5.6.3, R-KONF-55; układ z makiety 14.11.2026 projekt ekranu, decyzja 2): bieżące ustawienia panelu do
// odczytu w dwóch kolumnach (`POŁĄCZENIA`, `PANEL`), bez kursora i bez przełączników; pod nimi trzy kafelki
// `PRZYTRZYMAJ 2 S` (◀ sieć panelu, ▶ więcej na telefonie, ↻ oprogramowanie ze stanem) i zdanie granicy ADR-0002.
// Hasła (sieci domowej, AirBridge, okna serwisowego), konto AirBridge i token źródła otoczenia nigdy nie stoją na
// ekranie (R-KONF-38); struktura ich nie ma. Wartości ustawień aparatu też nie (ADR-0002).
// U-01, wiersz „Miasto pogody” bez miasta z listy, gdy pogoda idzie ze współrzędnych wgranych z panelem (secrets.h)
extern const char* const MIASTO_ZE_WSPOLRZEDNYCH;  // `współrzędne z wgrania`

struct DaneUstawien {
  std::string ssid;                 // sieć domowa; "" = `nie ustawiona`
  std::string abAdres;              // adres AirBridge z portem; "" = `nie ustawiony`
  std::string miasto;               // miasto do pogody (src/miasta.h); "" = `nie wybrano`; MIASTO_ZE_WSPOLRZEDNYCH
  bool profilPubliczny = false;     // `publiczny` / `prywatny`
  int humor = 2;                    // 0 `wyłączony`, 1 `oszczędny`, 2 `normalny`
  bool dzwiek = true;               // buzzer
  int ciszaOd = 22 * 60, ciszaDo = 8 * 60;  // cisza nocna, minuty doby
  bool podpowiedz = true;           // podpowiedź „co do poprawy”
  int co2 = -1;                     // -1 panel bez czujnika (źródła otoczenia), 0 ekrany CO2 wyłączone, 1 włączone
  std::string wersja;               // wersja oprogramowania (jak U-05 `Oprogramowanie`, bez slotu); kafelek ↻
  bool jestAktualizacja = false;    // panel zna wersję nowszą od własnej (do 14.11.2026 `jest aktualizacja ·` w belce;
                                    // od 14.11.2026 znak nowej wersji z znakiBelki(), R-EKR-69); do logu i testów
  std::string aktualizacja;         // aktualizacja::wierszU01() do logu (stan wersji i droga do U-07); na ekranie kafelek
  std::string stanOprogramowania;   // aktualizacja::kafelekU01().stan; "" = `sprawdź aktualizację`
  bool stanOdwrocony = false;       // kafelekU01().odwrocony: `jest {X} · zainstaluj` w odwróconym polu
  bool sprawy = false;              // przed U-01 „Otwarte sprawy” (co najmniej jedna sprawa, R-KOK-20): `◀ sprawy`
};
struct WierszUstawien {
  std::string nazwa, wartosc;
};
// Wiersze U-01 w kolejności z 5.6.3: kolumna `POŁĄCZENIA` (U01_WIERSZY_LEWA pierwszych: `Sieć Wi-Fi`, `AirBridge`,
// `Miasto pogody`, `Czujnik CO2`), potem kolumna `PANEL` (`Profil`, `Humor`, `Dźwięk`, `Podpowiedź „co do poprawy”`).
// Wersja oprogramowania stoi tylko w kafelku ↻ (raz na ekranie). Wartość w pierwszym wariancie, który mieści się obok
// nazwy (P20 / P20B, src/ekrany.h dopasujWartosc()), inaczej skrócona jawnie znakiem `…`.
std::vector<WierszUstawien> wierszeUstawien(const DaneUstawien& d);
const int U01_WIERSZY_LEWA = 4;
const int U01_KOLUMNA_L = 348, U01_KOLUMNA_P = 384;   // szerokości kolumn (744 = 348 + 12 + 384)
// Pionowa kreska 2 px na środku 12 px odstępu między kolumnami (odstęp 24 px się nie mieści, `Gorzów
// Wielkopolski` potrzebuje 347 px, `Podpowiedź „co do poprawy” wyłączona` 382 px).
const int U01_KRESKA_X = 28 + U01_KOLUMNA_L + 5, U01_KRESKA_W = 2;
// Kafelki `PRZYTRZYMAJ 2 S` (szerokości 204, 244, 276, odstęp 10 px; ramka 2 px, wcięcie 8 px): tytuł P20B z ikoną
// przycisku, pod nim do dwóch linii P20.
const int U01_KAFELEK_W[3] = {204, 244, 276};
extern const char* const U01_KAFELEK_TYTUL[2];        // `◀ Sieć panelu`, `▶ Więcej na telefonie`
extern const char* const U01_KAFELEK_OPIS[2][2];      // linie pod tytułem ◀ i ▶
extern const char* const U01_GRANICA;                 // `Te ustawienia dotyczą tylko panelu, nie aparatu.`
// Linie kafelka ↻: `↻ Oprogramowanie {wersja}` (gdy się nie mieści: `↻ Oprogramowanie` i wersja w drugiej linii) i stan
// (do dwóch linii, w odwróconym polu przy `stanOdwrocony`). Zwraca linie bez tytułu (do testów szerokości).
std::vector<std::string> linieKafelkaOprogramowania(const DaneUstawien& d, std::string& tytul);
void rysujUstawienia(Plotno& p, const DaneUstawien& d);
// Stopka U-01 (mapa 5.6.2, 5.5.7 „Stopki stron”, od 14.11.2026 tylko krótkie naciśnięcia; przytrzymania w kafelkach):
// ◀ podpisany stroną przed U-01: `◀ sypialnia` przy stronie „Sypialnia · 30 nocy” w ciągu (R-KOK-29, ADR-0044),
// inaczej `◀ sprawy` przy sprawach, w profilu publicznym bez spraw `◀ liczby`, inaczej `◀ 7 nocy`; dalej
// `↻ ekran dnia` (krótkie ▶ na U-01 nic nie robi, R31-04).
std::vector<std::string> stopkaU01(bool sprawy = false, bool liczby = false, bool sypialnia = sypialniaWKokpicie());  // ◀ sypialnia (R-KOK-29)

struct DaneFormularzaQr {
  std::string adresQr, adresReczny;  // "" = serwer nie działa (bez kodu)
  std::string kodWazny;              // `Kod ważny do {hh:mm}.`
  std::string zapisano;              // `Zapisano o {hh:mm}.` (R-KONF-17) albo ""
  std::string stan;                  // wynik sprawdzenia sieci albo AirBridge (5.6.3 U-02, R-KONF-37) albo ""
  bool bezWifi = false;              // `Nie mogę włączyć Wi-Fi`
};
// U-03. Zwraca false, gdy adresu nie da się zakodować.
bool rysujFormularzQr(Plotno& p, const DaneFormularzaQr& d);

// U-02 „Sieć panelu” (5.6.3, 5.6.5, R-KONF-07, R-KONF-08, R-KONF-28), uruchamiana tylko ręcznie (◀ 2 s na
// U-01, U-05 albo na ekranie pierwszego uruchomienia). Dwa kody QR: dołączenie do sieci
// panelu (`WIFI:T:WPA;S:…;P:…;;`) i adres formularza z tokenem; pod nimi nazwa sieci, hasło jednorazowe i adres tekstem.
// Hasło sieci panelu jest tylko tu i w kodzie QR (R-KONF-38: hasła sieci domowej i AirBridge nigdy).
struct DaneSieciPanelu {
  std::string nazwa, haslo;          // sieć panelu; "" = nie działa (bez kodów)
  std::string adresQr, adresReczny;  // formularz `http://192.168.4.1/u/{token}/` i ten sam adres bez `http://`
  std::string wazne;                 // `Ważne do {hh:mm}.` albo `Ważne 15 minut.` bez zegara
  std::string stan;                  // `Łączę z siecią {nazwa}…`, wynik nieudanej próby (R-KONF-10, R-KONF-11, R-KONF-37)
  bool instrukcja = false;           // pierwsze uruchomienie bez sieci: tylko instrukcja `Przytrzymaj ◀ 2 s…` (bez E-01)
  bool bezRadia = false;             // `Nie mogę włączyć sieci panelu.`
};
// Treść kodu QR do dołączenia do sieci Wi-Fi (format `WIFI:` z ucieczką `\`, `;`, `,`, `:`, `"`).
std::string kodWifi(const std::string& ssid, const std::string& haslo);
// U-02. Zwraca false, gdy któregoś kodu nie da się zakodować.
bool rysujSiecPanelu(Plotno& p, const DaneSieciPanelu& d);

// U-07 „Aktualizacja” (R-KONF-43, R-KONF-44, R-KONF-57; makieta 14.11.2026, decyzja 3): ekran
// danych bez budzika. Belka `Aktualizacja` ze stanem w środku (TresciU07::stanBelki); po lewej rysunek panelu 1 bit
// (rysujObrazekU07()); po prawej `{własna} → {nowa}` dużym krojem (P34B, gdy się nie mieści P28B, P24B, w końcu dwie
// linie) albo zdanie główne (P28B), pod nim zdania opisu (P24S), „Co nowego” (P16 wersalikami, linie P22B z kwadratem)
// i cztery kroki (koła 36 px, podpisy P20); pas w negatywie nad stopką (`Nie odłączaj zasilania.`), stopka z grup
// przycisków (pusta = bez stopki: pobieranie, instalacja, samosprawdzenie). Bez danych terapii, ustawień aparatu
// i tokenu. Teksty `Co nowego` z wydania: znaki spoza krojów panelu pominięte (znakJestWKroju()), linia dłuższa niż
// miejsce skrócona do pełnych słów z `…`.
void rysujAktualizacje(Plotno& p, const aktualizacja::TresciU07& t);
const int U07_OBRAZEK_X = 28, U07_OBRAZEK_W = 180, U07_OBRAZEK_H = 190;
const int U07_X = 236;                 // lewa krawędź prawej kolumny (do 772)
const int U07_PAS_Y = 392, U07_PAS_H = 40;
// Rysunek w polu U07_OBRAZEK_W × U07_OBRAZEK_H od (x, y).
void rysujObrazekU07(Plotno& p, int x, int y, aktualizacja::ObrazekU07 o);
// Linia „Co nowego” gotowa do rysowania: bez znaków spoza krojów, najwyżej `maxW` px w P22B (pełne słowa i `…`).
std::string liniaCoNowego(const std::string& t, int maxW);
