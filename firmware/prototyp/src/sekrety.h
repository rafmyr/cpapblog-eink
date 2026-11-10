// SPDX-License-Identifier: GPL-3.0-or-later
// Sekrety panelu poza obrazem firmware.
// Sieć domowa (nazwa, hasło), adres, konto i hasło AirBridge są w NVS, w osobnej przestrzeni `sekrety` (poza `cpap`
// i `noc`, więc poza zrzutem plików `Z` i poza kopią plików). `include/secrets.h` daje tylko wartość początkową:
//  - NVS pusty, secrets.h ma sieć i adres AirBridge → kopia do NVS (źródło „k”, z kompilacji, ze skrótem wartości);
//  - NVS ze źródłem „k”, a secrets.h ma inne wartości (nowe wgranie z nową siecią, np. egzemplarz testowy) → kopia od nowa;
//  - NVS ze źródłem „f” (zapis z formularza na telefonie, etap 2–4) → NVS wygrywa nad secrets.h;
//  - NVS ze źródłem „z” (zapis przerwany restartem) → kopia ostatnich zatwierdzonych danych;
//  - NVS pusty i secrets.h bez sieci albo bez adresu → stan „pierwsza konfiguracja” (U-02, R-KONF-07), nic nie zapisane.
// Hasło okna serwisowego (R-NF-29) też w NVS (`ota_h`): secrets.h `OTA_PASS` tylko jako wartość początkowa,
// formularz U-03 je zmienia; token wydań GitHub (ADR-0038, `gh_tok`) tylko z formularza. Adres i token lokalnego źródła
// otoczenia oraz współrzędne pogody (wcześniej tylko z secrets.h) mają kopię w NVS, żeby obraz wydania bez sekretów
// (`-DBEZ_SEKRETOW`) niczego panelowi nie odbierał. Wartości haseł nigdy w logu, na ekranie, w zrzucie ani
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

// ---------- token wydań (ADR-0038 pkt 4, R-NF-36) ----------
// Token z NVS (`gh_tok`); "" = brak (panel nie pyta GitHuba, R-NF-33). Nigdy do logu, na ekran ani do zrzutu.
std::string kluczWydan(Nvs& nvs);
// Zapis tokenu z formularza; "" usuwa. false, gdy zapis albo odczyt kontrolny się nie udał (poprzedni zostaje).
bool zapiszKluczWydan(Nvs& nvs, const std::string& t);

// ---------- hasło okna serwisowego (R-NF-29) ----------
// Reguła pierwszeństwa: `obrazZSekretami` = obraz zbudowany z secrets.h (wgranie kablem albo oknem
// serwisowym, bez -DBEZ_SEKRETOW). Taki obraz jest źródłem prawdy dla tego, co ma w secrets.h: wartość trafia do NVS,
// a brak definicji czyści NVS (np. usunięte `OTA_PASS` wyłącza okno). Wyjątek: hasło okna z formularza U-03 (źródło
// „f”) zostaje. Obraz wydania (`obrazZSekretami` = false) niczego w NVS nie zmienia i bierze wartości z NVS.
// `zKompilacji`: OTA_PASS z secrets.h ("" bez definicji). "" w wyniku = okno wyłączone (T-WYM-45).
std::string wczytajHasloOkna(Nvs& nvs, const std::string& zKompilacji, bool obrazZSekretami = true);
// Zapis z formularza (najmniej HASLO_OKNA_MIN_ZNAKOW znaków, bez znaków sterujących); false przy złym haśle albo zapisie.
const size_t HASLO_OKNA_MIN_ZNAKOW = 16;
bool zapiszHasloOkna(Nvs& nvs, const std::string& h);
bool hasloOknaUstawione(Nvs& nvs);

// ---------- dodatki z secrets.h z kopią w NVS ----------
struct Dodatki {
  std::string otoUrl, otoToken;  // OTO_URL, OTO_TOKEN
  std::string pogodaLat, pogodaLon;  // POGODA_LAT, POGODA_LON jako tekst ("" = brak)
};
// Ta sama reguła pierwszeństwa co hasło okna: obraz z secrets.h zapisuje każde pole do NVS (puste czyści),
// obraz wydania bierze wszystko z NVS bez zmian. Współrzędne zawsze parą.
Dodatki wczytajDodatki(Nvs& nvs, const Dodatki& zKompilacji, bool obrazZSekretami = true);

}  // namespace sekrety
