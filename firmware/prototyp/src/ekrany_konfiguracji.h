// SPDX-License-Identifier: GPL-3.0-or-later
// Ekrany konfiguracji (konfiguracja z telefonu; spec 5.6.3): U-01 „Ustawienia” (lista pozycji z kursorem) i U-03 formularz
// z kodem QR (jak ekran raportu dla lekarza, 5.13.6). Ekrany danych bez budzika; hasło nigdy na ekranie (R-KONF-38).
// Artboardów brak (5.6.3: „Ekrany nie mają jeszcze artboardów na kanwie”): układ według ekranu „Sprzęt” (U-01)
// i „Raport dla lekarza” (U-03), skala B.
#pragma once
#include <string>
#include <vector>

#include "ekrany.h"

// U-01: pozycje (nazwy z 5.6.3) i kursor; zdanie stałe `Te ustawienia dotyczą tylko panelu, nie aparatu.`
void rysujUstawienia(Plotno& p, const std::vector<std::string>& pozycje, int kursor);

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
