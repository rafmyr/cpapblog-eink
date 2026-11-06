// SPDX-License-Identifier: GPL-3.0-or-later
// Sekrety panelu poza obrazem firmware.
// Sieć domowa (nazwa, hasło), adres, konto i hasło AirBridge są w NVS, w osobnej przestrzeni `sekrety` (poza `cpap`
// i `noc`, więc poza zrzutem plików `Z` i poza kopią plików). `include/secrets.h` daje tylko wartość początkową:
//  - NVS pusty, secrets.h ma sieć i adres AirBridge → kopia do NVS (źródło „k”, z kompilacji, ze skrótem wartości);
//  - NVS ze źródłem „k”, a secrets.h ma inne wartości (nowe wgranie z nową siecią, np. egzemplarz testowy) → kopia od nowa;
//  - NVS ze źródłem „f” (zapis z formularza na telefonie, etap 2–4) → NVS wygrywa nad secrets.h;
//  - NVS ze źródłem „z” (zapis przerwany restartem) → kopia ostatnich zatwierdzonych danych;
//  - NVS pusty i secrets.h bez sieci albo bez adresu → stan „pierwsza konfiguracja” (U-02, R-KONF-07), nic nie zapisane.
// Hasło OTA zostaje w secrets.h (okno serwisowe, R-NF-29). Wartości haseł nigdy w logu, na ekranie, w zrzucie ani
// w odpowiedzi serwera (R-KONF-38); do logu tylko opis() bez haseł.
// Szyfrowanie kluczem egzemplarza (R-NF-22, „postać nieczytelna”) jest otwarte: NVS bez szyfrowania flash; wartości
// nie trafiają do plików ani do kopii na karcie, więc wyjęcie karty ich nie daje (do decyzji).
// Bez Arduino: rdzeń z testami na komputerze (test/testy_konfiguracja_z_telefonu.cpp).
#pragma once
#include <cstdint>
#include <string>

#include "adaptery.h"

namespace sekrety {

const char* const PRZESTRZEN = "sekrety";

struct Dane {
  std::string ssid, hasloWifi;          // sieć domowa (2,4 GHz)
  std::string abAdres, abKonto, abHaslo;  // AirBridge: adres (IP albo `.local`, z portem), konto Digest, hasło
};
// Panel ma czym pracować: sieć domowa i adres AirBridge (hasła mogą być puste, ADR-0011 adnotacja).
bool skonfigurowany(const Dane& d);

enum class Zrodlo : unsigned char { BRAK, KOMPILACJA, FORMULARZ };
struct Wynik {
  Dane d;
  Zrodlo zrodlo = Zrodlo::BRAK;  // BRAK = pierwsza konfiguracja
  bool zapisanoPoczatkowe = false;  // w tym starcie kopia z secrets.h do NVS
  bool pierwszaKonfiguracja() const { return zrodlo == Zrodlo::BRAK; }
};

// Przy starcie, przed dyrygentem (src/main.cpp). `zKompilacji`: wartości z secrets.h ("" bez definicji).
Wynik wczytaj(Nvs& nvs, const Dane& zKompilacji);
// Zapis z formularza (etapy 2–4): NVS ze źródłem „f”. Zwraca false, gdy dane niekompletne albo zapis NVS się nie
// udał (wynik ustawTekst() i odczyt kontrolny pięciu kluczy); wtedy w NVS zostają poprzednie dane. Przed
// stanem „z” kopia ostatnich zatwierdzonych danych (klucze `p_*`): restart w trakcie wraca do niej, nie do
// secrets.h.
bool zapiszZFormularza(Nvs& nvs, const Dane& d);
// Opis do logu bez haseł: `sieć {ssid}, AirBridge {adres}, konto {konto}, hasło AirBridge ustawione|puste, źródło …`.
std::string opis(const Wynik& w);
const char* nazwaZrodla(Zrodlo z);

}  // namespace sekrety
