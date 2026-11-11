// SPDX-License-Identifier: GPL-3.0-or-later
// Porównanie nocy liczonej na panelu z plików sesji (src/noc_pliki.h) z nocą z raportu JSON AirBridge tej samej doby,
// w tolerancjach WY9 (spec 9.1). Jedna funkcja porównania dla panelu (tryb `noc_zrodlo=porownanie`, ADR-0036
// „Zmiana 2026-11-09”) i dla narzędzia na komputerze (tools/kalibracja/noc_panelu.cpp), bez zależności od Arduino.
//
// Panel nigdy nie zapisuje nocy z porównania: noc w historii jest z raportu JSON, a wynik porównania trafia tylko do
// pliku `/dane/porownanie.csv` (ostatnie 30 dób) i do wiersza `Noce z plików` na ekranie testu sprzętu (U-05).
// Wynik porównania nie wpływa na ekrany pacjenta i nigdy nie jest oceną nocy (ADR-0029 pkt 7).
#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "airbridge.h"

namespace porownanie {

// Ocena jednego wiersza porównania.
enum class Ocena : uint8_t {
  TAK,               // obie strony w tolerancji albo obie bez wartości
  NIE,               // różnica poza tolerancją (rozjazd)
  BRAK_W_PLIKACH,    // raport ma wartość, noc z plików nie (rozjazd: po przełączeniu metryka by zniknęła)
  BRAK_W_RAPORCIE,   // noc z plików ma wartość, raport nie (bez wpływu na werdykt)
  POMINIETE_APARAT,  // OA/h albo CA/h: pliki „nieznane” (EVE tylko `Apnea`), raport 0 (aparat nie rozróżnia)
  TYLKO_PLIKI,       // PS (M-28, M-28p): raport JSON go nie ma
  BEZ_STR,           // M-21 bez STR.edf (tylko narzędzie; panel liczy noc z plików zawsze ze STR)
};
const char* opisOceny(Ocena o);    // `tak`, `NIE`, `brak w plikach`, … (kolumna `wynik` narzędzia)
bool rozjazd(Ocena o);             // NIE albo BRAK_W_PLIKACH
bool pominieta(Ocena o);           // POMINIETE_APARAT, TYLKO_PLIKI, BEZ_STR: zapisywane, poza werdyktem

struct Wiersz {
  std::string id;    // m01, m02, m03oa, …, m28p; flagi: czesciowa, pelna, wazna, sesje
  std::string opis;  // nazwa w tabeli narzędzia, np. `M-05 IPAP mediana`
  bool tekst = false;
  double panel = 0, raport = 0, tolerancja = 0;  // NAN = brak wartości (wiersze liczbowe)
  std::string tPanel, tRaport;                    // wiersze tekstowe (flagi, M-21)
  Ocena ocena = Ocena::TAK;
};
struct Wynik {
  std::vector<Wiersz> wiersze;
  int rozjazdy = 0;  // wiersze z rozjazd(ocena)
};
// Noc z plików `panel` (nocZWyniku()) wobec nocy z raportu JSON `raport` (nocZRaportu()) tej samej doby. Tolerancje
// WY9 [PRÓG] jak w narzędziu porównania: M-01 1 min, AHI i M-03 0,5/h, ciśnienia 0,5 cmH2O, wyciek 2 L/min, M-08 i M-09
// 2 min, M-10 0,02, M-13 0,5, M-14 20 mL, M-15 0,3 L/min, M-17 0, M-18 5 min, flagi i M-21 ten sam tekst; granica
// z marginesem względnym 1e-6 (wartości z float). `zUstawieniami` = false: M-21 jako
// BEZ_STR (narzędzie bez STR.edf).
Wynik porownaj(const NocAB& panel, const NocAB& raport, bool zUstawieniami = true);
std::string liczba(double v, int miejsca = 2);  // "–" dla NAN

// Porównanie biegnie na samym końcu wybudzenia (po raportach JSON wszystkich dób, zapisie historii, rysowaniu
// i czuwaniu) i korzysta tylko z budżetu żądań, który ZOSTAŁ po raportach i zaległych dobach (AB_LIMIT_WYBUDZENIA_MS
// albo AB_LIMIT_ZALEGLYCH_MS w src/dyrygent.cpp, bez nowego budżetu). Najmniej tyle z niego na start porównania doby
// [stała techniczna]: lista sesji, STR.edf i pliki sesji; w trakcie każde żądanie dalej wymaga AB_LIMIT_ODPOWIEDZI_MS
// (twarde przerwanie: wpis `brak czasu`). Mniej na starcie = wpis `brak czasu` bez Wi-Fi, ponowienie w kolejnym
// wybudzeniu; noc z raportu JSON jest wtedy już dawno zapisana.
const uint32_t CZAS_POROWNANIA_MS = 3 * AB_LIMIT_ODPOWIEDZI_MS;
// Najwyżej tyle dób porównania na wybudzenie, od najnowszej [PRÓG] techniczny: koszt Wi-Fi jednego porównania
// (ok. 0,5 MB, na ESP32 szacunek 2–5 s) ponad czas wybudzenia przy `json`.
const int POROWNAN_NA_WYBUDZENIE = 1;
// Ponawiane na końcu kolejnych wybudzeń (z AirBridge i udanym odczytem) są tylko doby z przyczyną przejściową
// (`czeka`, `brak czasu`, `sesja niezamknięta`) z ostatnich DOB_PONAWIANIA dób [stała techniczna]; pozostałe
// przyczyny czekają na ponowne pobranie raportu tej doby (R-DANE-24, `stale`).
const int DOB_PONAWIANIA = 7;
bool czekaPonowienia(const std::string& przyczyna);

// ---------- wynik doby i plik /dane/porownanie.csv ----------
enum class Werdykt : uint8_t { ZGODNA, ROZJAZD, BRAK };
const char* nazwaWerdyktu(Werdykt w);  // zgodna, rozjazd, brak
// Przyczyna braku porównania (noc z plików nie powstała, R-DANE-44): krótki tekst do pliku, logu i U-05.
enum class Przyczyna : uint8_t {
  BRAK,              // porównanie zrobione
  STARY_AIRBRIDGE,   // /api/sessions 404: AirBridge < 2.3.1
  BLAD_LISTY,        // lista sesji: inny kod, nieczytelna, przerwa w sieci
  BEZ_SESJI,         // doba bez sesji na liście
  ZA_DUZO_SESJI,     // ponad STRON_NA_DOBE stron listy
  NIEZAMKNIETA,      // sesja niezamknięta albo `.part`
  BEZ_STR,           // STR.edf niepobrany albo nieczytelny
  STR_BEZ_DOBY,      // STR.edf jeszcze bez tej doby
  PLIK_NIEPOBRANY,   // plik zniknął, inny kod, ponad limit
  PLIK_NIECZYTELNY,  // plik uszkodzony albo ucięty
  CZESCIOWA,         // noc z plików częściowa
  BRAK_CZASU,        // za mało budżetu żądań po raportach i zaległych dobach
  CZEKA,             // raport JSON kompletny, porównanie czeka na koniec wybudzenia (albo na kolejne)
};
const char* opisPrzyczyny(Przyczyna p);  // np. `AirBridge < 2.3.1`, `sesja niezamknięta`, `brak plików`, `brak czasu`

struct Wpis {
  std::string data;  // data poranka RRRR-MM-DD
  Werdykt werdykt = Werdykt::BRAK;
  std::string rozjazd;    // `m05=14.20~13.10|wazna=tak~nie` (id=panel~raport), "" = brak
  std::string pominiete;  // `m03oa:aparat|m28:pliki`
  std::string przyczyna;  // przy BRAK: opisPrzyczyny()
};
// Pola formatu `porownanie` wersji 1 (src/magazyn.cpp rejestrFormatow()).
const char* const POLA = "data;werdykt;rozjazd;pominiete;przyczyna";
const int MAKS_WPISOW = 30;  // ostatnie 30 dób [stała techniczna]
Wpis wpisZWyniku(const std::string& data, const Wynik& w);
Wpis wpisBraku(const std::string& data, Przyczyna p);
std::string wierszWpisu(const Wpis& w);                // bez '\n'
bool parsujWpis(const std::string& wiersz, Wpis& w);  // false: zła data, werdykt albo liczba pól
std::vector<Wpis> parsujWpisy(const std::vector<std::string>& wiersze);  // złe wiersze pominięte, posortowane po dacie
// Wpis doby zastępuje wpis tej samej daty; wynik posortowany, najwyżej MAKS_WPISOW najnowszych.
void dopisz(std::vector<Wpis>& wpisy, const Wpis& w);
// Porównanie doby już jest (zgodna albo rozjazd): pliki doby nie są pobierane drugi raz.
bool gotowe(const std::vector<Wpis>& wpisy, const std::string& data);

// ---------- wiersz U-05 ----------
const int DOB_DO_PRZELACZENIA = 7;  // tyle kolejnych dób z porównaniem zgodnych = dopisek „można przełączyć” [PRÓG]
struct Licznik {
  int porownane = 0, zgodne = 0;  // z ostatnich DOB_DO_PRZELACZENIA dób z porównaniem
  std::vector<std::string> metryki;  // M-05, M-02, … z rozjazdu w tych dobach (kolejność pierwszego wystąpienia)
  std::string przyczyna;           // przyczyna braku najnowszego wpisu BRAK ("" = najnowszy wpis to porównanie)
  bool wpisy = false;              // jakikolwiek wpis
  bool moznaPrzelaczyc() const { return porownane == DOB_DO_PRZELACZENIA && zgodne == porownane; }
};
Licznik policz(const std::vector<Wpis>& wpisy);
// Źródło nocy z konfiguracji (`noc_zrodlo`), jak src/slot_dzienny.h UstawieniaDnia::nocZrodlo.
enum class Zrodlo : uint8_t { POROWNANIE, PLIK, JSON };
// Warianty wartości wiersza `Noce z plików` od najdłuższego (dopasujWartosc() w src/ekrany.h wybiera pierwszy, który
// się mieści): `zgodne 7 z 7, można przełączyć na pliki`, `rozjazd 1 z 7 (M-05)`, `brak porównania: AirBridge < 2.3.1`,
// `jeszcze bez porównania`, `wyłączone (json)`, `włączone (plik)`.
std::vector<std::string> wartosciU05(const Licznik& l, Zrodlo z);
// Linia diagnostyki U-05 w logu portu: tryb, licznik, ostatni rozjazd z wartościami, ostatnia przyczyna braku.
std::string liniaDiagnostyki(const std::vector<Wpis>& wpisy, Zrodlo z);

}  // namespace porownanie
