// SPDX-License-Identifier: GPL-3.0-or-later
// Ekrany konfiguracji (konfiguracja z telefonu; spec 5.6.3): U-01 „Ustawienia” (lista pozycji z kursorem) i U-03 formularz
// z kodem QR (jak ekran raportu dla lekarza, 5.13.6). Ekrany danych bez budzika; hasło nigdy na ekranie (R-KONF-38).
// Artboardów brak (5.6.3: „Ekrany nie mają jeszcze artboardów na kanwie”): układ według ekranu „Sprzęt” (U-01)
// i „Raport dla lekarza” (U-03), skala B.
#pragma once
#include <string>
#include <vector>

#include "ekrany.h"

// U-01 (5.6.3, R-KONF-55): lista do odczytu bieżących ustawień panelu, bez kursora i bez przełączników. Hasła
// (sieci domowej, AirBridge, okna serwisowego), konto AirBridge i token źródła otoczenia nigdy nie stoją na ekranie
// (R-KONF-38); struktura ich nie ma. Wartości ustawień aparatu też nie (ADR-0002).
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
  std::string wersja;               // wersja oprogramowania (jak U-05 `Oprogramowanie`, bez slotu)
  std::string aktualizacja;         // linia pod listą: aktualizacja::wierszU01() (stan wersji i droga do U-07); "" = bez
  bool sprawy = false;              // przed U-01 „Otwarte sprawy” (co najmniej jedna sprawa, R-KOK-20): `◀ sprawy`
};
struct WierszUstawien {
  std::string nazwa, wartosc;
};
// Wiersze U-01 w kolejności z 5.6.3: `Sieć Wi-Fi`, `AirBridge`, `Miasto pogody`, `Profil`, `Humor`, `Dźwięk`,
// `Podpowiedź „co do poprawy”`, `Czujnik CO2`, `Oprogramowanie`. Wartość dłuższa niż kolumna wartości
// (U01_SZER_WARTOSCI px w P24B) skrócona jawnie znakiem `…`.
std::vector<WierszUstawien> wierszeUstawien(const DaneUstawien& d);
const int U01_X_WARTOSCI = 350;                       // kolumna wartości od x 350 do 772
const int U01_SZER_WARTOSCI = 772 - U01_X_WARTOSCI;
// Pod listą: linia aktualizacji (DaneUstawien::aktualizacja), podpowiedź (dwie linie P22B) i zdanie stałe granicy
// ADR-0002. Stopka z stopkaU01().
extern const char* const U01_PODPOWIEDZ[2];
extern const char* const U01_GRANICA;                 // `Te ustawienia dotyczą tylko panelu, nie aparatu.`
void rysujUstawienia(Plotno& p, const DaneUstawien& d);
// Stopka U-01 (mapa 5.6.2, 5.5.7 „Stopki stron”): ◀ podpisany stroną przed U-01: `◀ sprawy · 2 s sieć` przy
// sprawach, w profilu publicznym bez spraw `◀ liczby · 2 s sieć`, inaczej `◀ 7 nocy · 2 s sieć`; dalej `▶ 2 s telefon`,
// `↻ ekran dnia · 2 s aktualizacja`.
std::vector<std::string> stopkaU01(bool sprawy = false, bool liczby = false);

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

// U-07 „Aktualizacja” (R-KONF-43, R-KONF-44): ekran danych bez budzika, nagłówek
// `Aktualizacja`, zdania z aktualizacja::trescU07() (pierwsze P28B, kolejne P24, zawijane do 744 px), stopka z grup
// przycisków (pusta = bez stopki: pobieranie i instalacja). Bez danych terapii, ustawień aparatu i tokenu.
void rysujAktualizacje(Plotno& p, const std::vector<std::string>& linie, const std::vector<std::string>& stopka);
