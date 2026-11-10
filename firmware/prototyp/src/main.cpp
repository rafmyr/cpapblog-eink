// SPDX-License-Identifier: GPL-3.0-or-later
// Firmware panelu cpap-eink (wydanie 1.0.0-beta.1) na reTerminal E1001: ESP32-S3, e-ink 800×480 1-bit.
// Rano ekran dnia z highlightami terapii, pod przyciskiem kokpit K1/K2, pytanie „Co było inaczej?” i karta z budzikiem.
// Czyta AirBridge od v2.2.0 (raport nocy z Załącznika A, logowanie Digest, tylko odczyt; pełna historia nocy z modułu)
// i historię nocy z importu karty aparatu (noce, których AirBridge nie ma).
// Granice: tylko żądania odczytu do AirBridge (R-LEK-08), przetwarzanie na panelu (ADR-0001),
// żadnych wskazówek o ustawieniach aparatu i żadnej oceny zdrowia na ekranie (ADR-0002),
// karta z budzikiem albo ekran danych, bez mieszania (ADR-0007), budzik nigdy na czarnym tle (ADR-0009).
//
// Ten plik tylko buduje adaptery ESP32 (src/adaptery_esp32.h) i woła dyrygenta wybudzenia (src/dyrygent.h).
// Cała logika jest w plikach bez Arduino z testami na komputerze: tools/prototyp/testy.sh.

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
// Obraz wydania (`-DBEZ_SEKRETOW`, tools/wydanie/): bez secrets.h, więc bez żadnej wartości początkowej. Panel
// traktuje puste wartości jak brak i zostaje przy tym, co ma w pamięci (src/sekrety.h).
#ifndef BEZ_SEKRETOW
#include "secrets.h"
static const bool OBRAZ_Z_SEKRETAMI = true;
#else
static const bool OBRAZ_Z_SEKRETAMI = false;
// Znacznik obrazu wydania sprawdzany przez tools/wydanie/wydaj.py: obraz bez niego nie jest wydawany.
extern "C" __attribute__((used)) const char ZNACZNIK_OBRAZU_WYDANIA[] = "cpap-eink:obraz-bez-sekretow:v1";
#endif
#include "adaptery_esp32.h"
#include "dyrygent.h"
#include "sekrety.h"
#include "start.h"

// Stos zadania loop: domyślne 8 KB nie wystarcza na rysowanie ekranu dnia (snprintf w głębokim wywołaniu,
// panika „Unhandled debug exception”). 24 KB nie wystarczało przy kilkumiesięcznej historii: ekran dnia z S8
// (silnik reguł, stany z pamięci, zapis nocy przez snprintf) przekraczał stos („Stack canary watchpoint
// triggered (loopTask)”, 06.11.2026), więc 48 KB (zużycie ok. 24 KB).
SET_LOOP_TASK_STACK_SIZE(48 * 1024);

// Numer docelowego wydania (spec techniczna 8.7: przy wydaniu wersja z tagu git). Numer wydania nie zawiera przedrostka „proto”.
// Narzędzie wydania (tools/wydanie/) podaje numer z tagu: -DWERSJA_WYDANIA=\"1.0.1\". Flaga ma inną nazwę niż
// stała WERSJA, bo flaga z wiersza poleceń obejmuje wszystkie pliki, a `WERSJA` jest też nazwą wartości
// `edf::Blad::WERSJA` (src/edf.h): `-DWERSJA=…` psuło kompilację obrazu wydania po scaleniu z EDF.
#ifndef WERSJA_WYDANIA
#define WERSJA_WYDANIA "1.0.0-beta.1"
#endif
#define WERSJA WERSJA_WYDANIA
// Skrót commitu z wersja_budowy.py (platformio.ini, extra_scripts); bez gita albo poza PlatformIO „nieznany”.
#ifndef WERSJA_BUDOWY
#define WERSJA_BUDOWY "nieznany"
#endif
// Identyfikator wersji `{WERSJA}+{skrót commitu}`: log (także diagnostyka U-05), licznik startów i znacznik trybu
// awaryjnego (src/start.h). Inny przy każdym commicie, więc naprawa wgrana z trybu awaryjnego nie jest „tą samą
// wersją” co zepsuta. Na U-05 tylko numer wydania i slot (src/start.h wersjaWydania, kanwa).
static const char* const IDENTYFIKATOR = WERSJA "+" WERSJA_BUDOWY;

// Powrót do poprzedniej wersji (spec techniczna 7.8, T-WYM-41): rdzeń Arduino nie oznacza nowej wersji jako dobrej
// przy starcie; robi to dyrygent po samosprawdzeniu przed pierwszym snem (Uruchomienie::oznaczWersjeDobra()).
extern "C" bool verifyRollbackLater() { return true; }

#ifdef OTA_PASS
// puste albo krótkie hasło otwierałoby okno wgrywania dla każdego w sieci domowej (sizeof liczy też znak końca)
static_assert(sizeof(OTA_PASS) > HASLO_OKNA_MIN, "OTA_PASS: co najmniej 16 znaków (np. openssl rand -hex 16)");
static const char* const HASLO_OKNA_Z_KOMPILACJI = OTA_PASS;
#else
static const char* const HASLO_OKNA_Z_KOMPILACJI = "";
#endif

// (src/sekrety.h): sieć i AirBridge z secrets.h tylko jako wartość początkowa dla NVS; brak definicji
// = "" (panel bez sieci w secrets.h startuje w stanie „pierwsza konfiguracja”, R-KONF-07).
static sekrety::Dane zKompilacji() {
  sekrety::Dane d;
#ifdef WIFI_SSID
  d.ssid = WIFI_SSID;
#endif
#ifdef WIFI_PASS
  d.hasloWifi = WIFI_PASS;
#endif
#ifdef AB_IP
  d.abAdres = AB_IP;
#endif
#ifdef AB_USER
  d.abKonto = AB_USER;
#else
  d.abKonto = "admin";  // domyślne konto AirBridge (5.6.4)
#endif
#ifdef AB_PASS
  d.abHaslo = AB_PASS;
#endif
  return d;
}

// Adres i token źródła otoczenia oraz współrzędne pogody z secrets.h tylko jako wartość początkowa dla NVS.
static sekrety::Dodatki dodatkiZKompilacji() {
  sekrety::Dodatki d;
#ifdef OTO_URL
  d.otoUrl = OTO_URL;
#endif
#ifdef OTO_TOKEN
  d.otoToken = OTO_TOKEN;
#endif
#if defined(POGODA_LAT) && defined(POGODA_LON)
  char b[24];
  snprintf(b, sizeof b, "%.6f", (double)POGODA_LAT);
  d.pogodaLat = b;
  snprintf(b, sizeof b, "%.6f", (double)POGODA_LON);
  d.pogodaLon = b;
#endif
  return d;
}

static double wspolrzedna(const std::string& t, double granica) {
  if (t.empty()) return NAN;
  char* e = nullptr;
  double v = strtod(t.c_str(), &e);
  return (e && *e == 0 && v >= -granica && v <= granica) ? v : NAN;
}

static KonfiguracjaPanelu konfiguracja(const sekrety::Wynik& s, const sekrety::Dodatki& dd, bool oknoZHaslem) {
  KonfiguracjaPanelu k;
  k.abAdres = s.d.abAdres;
  k.abKonto = s.d.abKonto;
  k.abHaslo = s.d.abHaslo;
  k.ssid = s.d.ssid;
  k.hasloWifi = s.d.hasloWifi;
  k.pierwszaKonfiguracja = s.pierwszaKonfiguracja();
  k.opisSekretow = sekrety::opis(s);
  k.otoUrl = dd.otoUrl;  // lokalne źródło otoczenia (ADR-0019); puste = wyłączone (R-AW-07)
  k.otoToken = dd.otoToken;
  double lat = wspolrzedna(dd.pogodaLat, 90), lon = wspolrzedna(dd.pogodaLon, 180);
  if (!std::isnan(lat) && !std::isnan(lon)) {  // prognoza godzinowa Open-Meteo i S-K-37 (R-KON-52, R-KON-49)
    k.pogodaLat = lat;
    k.pogodaLon = lon;
  }
  k.oknoZHaslem = oknoZHaslem;
  return k;
}

void setup() {
  piskBudzenia();  // przycisk budzący słychać od razu, przed resztą startu (R-PRZ-20)
  // Zwykłe alokacje od 8 B do PSRAM (8 MB): historia nocy, teksty i stany silnika reguł (głównie małe węzły map
  // i zbiorów) nie zajmują pamięci wewnętrznej, którą potrzebują Wi-Fi, DMA i stos. Przy kilkumiesięcznej historii
  // i stosie 48 KB minimum wolnej pamięci wewnętrznej spadało do 452 B, przy progu 64 B nadal, po tej zmianie
  // 162 KB (06.11.2026). Bufory z wymaganym typem pamięci (DMA, Wi-Fi) idą jak dotąd.
  heap_caps_malloc_extmem_enable(8);
  przygotujPanel();
#ifdef BEZ_SEKRETOW
  Serial.printf("%s\n", ZNACZNIK_OBRAZU_WYDANIA);  // odwołanie trzyma znacznik w obrazie (linker nie usuwa)
#endif
  static PrzyciskiEsp32 przyciski;  // próbkowanie przycisków jak najwcześniej (R-PRZ-20): kolejka od startu
  Serial.printf("pamiec start: PSRAM %u KB (wolne %u KB), wewnetrzna wolna %u KB, najwiekszy blok %u KB\n",
                (unsigned)(ESP.getPsramSize() / 1024), (unsigned)(ESP.getFreePsram() / 1024),
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024),
                (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024));
  static ZegarEsp32 zegar;
  static NvsEsp32 nvs;
  static PlikiEsp32 pliki;
  // sekrety z NVS przed siecią i dyrygentem (secrets.h tylko jako wartość początkowa, src/sekrety.h)
  static const sekrety::Wynik sek = sekrety::wczytaj(nvs, zKompilacji());
  static const sekrety::Dodatki dodatki = sekrety::wczytajDodatki(nvs, dodatkiZKompilacji(), OBRAZ_Z_SEKRETAMI);
  // hasło okna serwisowego z NVS: OTA_PASS z secrets.h tylko jako wartość początkowa, formularz je zmienia
  static const std::string hasloOkna = sekrety::wczytajHasloOkna(nvs, HASLO_OKNA_Z_KOMPILACJI, OBRAZ_Z_SEKRETAMI);
  static SiecEsp32 siec({sek.d.ssid, sek.d.hasloWifi, hasloOkna.empty() ? nullptr : hasloOkna.c_str()}, pliki);
  static WydaniaEsp32 wydania;  // aktualizacja z wydań GitHub (ADR-0032, ADR-0038)
  static WyswietlaczEsp32 ekran;
  static CzujnikiEsp32 czujniki;
  static DzwiekEsp32 dzwiek;
  static UruchomienieEsp32 uruchomienie(IDENTYFIKATOR);
  static StraznikEsp32 straznik;
  static SenEsp32 sen;
  static PortEsp32 port;
  static DiagnostykaEsp32 diagnostyka;
  Adaptery a{zegar, nvs, pliki, siec, ekran, przyciski, czujniki, dzwiek, uruchomienie, straznik, sen, port, diagnostyka,
             &wydania};
  Dyrygent(a, konfiguracja(sek, dodatki, !hasloOkna.empty())).wybudzenie();
  // tu tylko, gdy dyrygent wrócił bez snu (nieudany powrót do poprzedniej wersji): licznik startów od zera jak przed
  // każdym snem (T-WYM-82), potem sen awaryjny
  zerujPrzedSnem(uruchomienie.licznik(), identyfikatorWersji(uruchomienie.wersja()));
  snAwaryjny();
}

void loop() {}
