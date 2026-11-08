// SPDX-License-Identifier: GPL-3.0-or-later
// Wzór. Prawdziwy plik secrets.h robi skrypt tools/prototyp/sekrety.sh z Pęku kluczy macOS albo kopia tego pliku
// z odkomentowanymi liniami. secrets.h jest w .gitignore i nigdy nie trafia do repo (WY12).
//
// Kopia bez zmian (tak buduje CI) daje działający firmware bez sieci i bez wgrywania przez Wi-Fi: panel startuje
// w stanie „pierwsza konfiguracja” (R-KONF-07), a sieć i AirBridge ustawia się z telefonu (U-02, U-03). Wartości
// poniżej to przykłady: odkomentuj tylko te, których potrzebujesz, i wpisz własne.
#pragma once
// Sieć domowa i AirBridge jako wartość początkowa dla pamięci panelu (src/sekrety.h); bez nich: pierwsza konfiguracja.
// #define WIFI_SSID   "nazwa-sieci"
// #define WIFI_PASS   "haslo-wifi"
// #define AB_IP       "192.168.1.50"     // AirBridge od v2.2.0: adres stały (bez mDNS); przykład, wpisz swój
// #define AB_USER     "admin"            // logowanie HTTP Digest; bez tej linii konto „admin”
// #define AB_PASS     "haslo-airbridge"
// Wgrywanie przez Wi-Fi (okno serwisowe U-05). Bez tej linii jest wyłączone. Wpisz własne długie, losowe hasło
// (np. `openssl rand -hex 16`, 32 znaki), nigdy przykładowe. Hasło krótsze niż 16 znaków (także puste) zatrzymuje
// kompilację (src/main.cpp static_assert), a okno z takim hasłem i tak się nie otwiera (src/adaptery_esp32.cpp).
// #define OTA_PASS    "wpisz-wlasne-losowe-haslo"
// Lokalne źródło danych otoczenia (opcja, ADR-0019): dowolne urządzenie w sieci domowej, które podaje JSON według
// spec Załącznik C (np. Home Assistant, Node-RED albo własny skrypt na serwerze domowym). Bez OTO_URL funkcja jest
// wyłączona i panel działa jak bez źródła. OTO_TOKEN tylko, gdy źródło sprawdza nagłówek X-Token.
// #define OTO_URL     "http://192.168.1.60:8080/otoczenie"  // przykład, wpisz swój
// #define OTO_TOKEN   "token-zrodla-otoczenia"
// Prognoza godzinowa z serwisu pogody Open-Meteo (slot dzienny, ADR-0027) i wschód słońca (S-K-37): współrzędne miasta.
// Bez nich panel nie pyta serwisu, a w slocie dziennym pokazuje sprzęt, słońce albo obrazek (R-KON-53).
// #define POGODA_LAT  50.2597   // przykład: Katowice
// #define POGODA_LON  19.0217
