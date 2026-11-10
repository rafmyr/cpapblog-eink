// SPDX-License-Identifier: GPL-3.0-or-later
// Historia nocy z pliku STR.edf aparatu (podsumowanie dób), pobieranego z AirBridge od v2.3.1; bez zależności od
// Arduino. Odczyt dób jak klasa `Str` w tools/kalibracja/noce.py, kanały jak `HistoricalStr::statistic` w AirBridge
// (`src/edf_report.cpp`), rekord nocy (źródło 'S') przez te same reguły kanałów i ustawień co raport AirBridge
// (src/airbridge.h kanalyRaportu(), ustawieniaRaportu()).
//
// Protokół (AirBridge v2.3.1, spec Załącznik A):
//   GET /api/sessions?order=desc&limit=1        → {"sessions":[…], "str":{"path","size","modified","etag"}|null,
//                                                   "next_cursor":…}; 404 = AirBridge starszy niż 2.3.1;
//                                                   503 `catalog_busy` / `catalog_unavailable` = później
//   GET /api/storage/download?path=/STR.edf     → cały plik (application/octet-stream, bez Range); w trakcie terapii
//                                                   zablokowany
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "airbridge.h"
#include "edf.h"
#include "noc.h"

// ---------- /api/sessions (ścieżki i limity ciała: src/airbridge.h) ----------
// NVS `cpap`: `{etag}|{liczba nocy 'S' w historii po zapisie}` ostatniego STR.edf, który panel przetworzył i zapisał
// do historii ("" = żaden); etag z liczbą > 0 przy historii bez nocy 'S' traktowany jak nieznany; nowy klucz
// z wartością domyślną „brak”, bez migracji (ADR-0022, zmiana 04.11.2026).
const char* const NVS_KLUCZ_STR_ETAG = "str_etag";

struct InfoStr {
  bool jest = false;     // blok `str` obecny i nie null
  std::string etag;      // `str.etag` (CRC32 całego pliku, w cudzysłowie jak w nagłówku ETag); "" = brak
  double rozmiar = -1;   // `str.size`; -1 = brak
};
// false: tekst nie jest obiektem JSON z kluczem `sessions` (tablica) albo `str` ma zły typ. `str` = null albo brak
// klucza: true i jest = false.
bool parsujSesjeStr(const std::string& tresc, InfoStr& s);

// ---------- STR.edf ----------
struct Str {
  std::string bajty;  // cały plik (na panelu w PSRAM); `plik` wskazuje na te bajty
  edf::Plik plik;
  std::map<long, long> doby;  // numer dnia (dni od 1970-01-01, doba od południa) → rekord; późniejszy rekord wygrywa
  Str() = default;
  Str(const Str&) = delete;
  Str& operator=(const Str&) = delete;
  Str(Str&& o) noexcept { *this = std::move(o); }
  Str& operator=(Str&& o) noexcept {
    bajty = std::move(o.bajty);
    plik = std::move(o.plik);
    doby = std::move(o.doby);
    plik.dane = (const uint8_t*)bajty.data();  // std::string przy przeniesieniu krótkiego tekstu zmienia adres
    plik.rozmiar = bajty.size();
    return *this;
  }
};
// Doba z `Date` (albo dniem z nagłówka) spoza 1990–2099 jest pominięta (uszkodzony plik, nie błąd całego pliku).
// Odczyt pliku; `bajty` przechodzą na własność `s` (std::move bez kopii), więc widok `plik` żyje tyle co `s`. Błąd:
// edf::Blad inny niż BRAK, `s` bez zmian. Plik bez żadnej doby (sygnału `Mode` ani `Duration` z wartością) to nie
// błąd: pusta lista. `s` jest tylko do przenoszenia (kopia zostawiłaby widok na bajty oryginału).
edf::Blad czytajStr(std::string bajty, Str& s);
// Wartość fizyczna sygnału `etykieta` w rekordzie `rekord`; NAN: brak sygnału, sygnał z więcej niż jedną próbką na
// rekord, surowa próbka 0xFFFF (brak wartości) albo próbka spoza zakresu cyfrowego.
double wartoscStr(const Str& s, long rekord, const std::string& etykieta);
// `settings` jak w raporcie AirBridge dla doby (Str.ustawienia() w noce.py): {"available": true, "fields": [{"tag",
// "value" (tekst: liczba w zapisie %.8g albo nazwa opcji), "option" (numer opcji albo null)}]} w kolejności
// MOP, układ trybu, komfort, EPR, SST, akcesoria; tylko tagi z wartością.
Json ustawieniaStr(const Str& s, long rekord);
// Statystyka kanału ze STR (`HistoricalStr::statistic`): etykieta = (Press → TgtIPAP, EprPress → TgtEPAP, inne → id)
// + "." + przyrostek ("50", "95", "Max"); Leak w L/s × 60 → L/min, TidVol w L × 1000 → mL; inna jednostka niż
// oczekiwana (bez tych przeliczeń) albo brak wartości = NAN.
double statystykaStr(const Str& s, long rekord, const std::string& idKanalu, const std::string& przyrostek);
// `channels[]` doby jak raport AirBridge dla doby tylko ze STR: dla każdego kanału z wartością `.50` albo `.95`
// obiekt {"id", "unit" (jednostka wyniku), "median", "p95", "source": "str"}.
Json kanalyStr(const Str& s, long rekord);

// Doba STR → rekord nocy (zrodlo 'S', ADR-0035 pkt 3). false: doba bez czasu terapii (`Duration` brak, ≤ 0 albo
// ponad 1440 min) albo bez daty. Data poranka = dzień + 1. M-01 = round(Duration); AHI = `AHI`; OA/h, CA/h, H/h, UA/h
// = `OAI`, `CAI`, `HI`, `UAI`; kanały i ustawienia przez kanalyRaportu() i ustawieniaRaportu() (ciśnienie wdechowe
// ze STR pominięte, R-NOR-08; EPAP z flagą epapStr); M-08, M-09, M-17, M-18 i PS: brak. Ważna jak noc z AirBridge
// (≥ 240 min, znane AHI, tryb obsługiwany), nigdy „częściowa”. `pna`, `srn`: z `/api/status` tego wybudzenia.
bool nocZeStr(const Str& s, long dzien, Noc& n, const std::string& pna = "", const std::string& srn = "");
// Wszystkie noce ze STR z datą poranka ≤ `dzis` ("" = bez filtra), rosnąco po dacie.
std::vector<Noc> noceZeStr(const Str& s, const std::string& dzis, const std::string& pna = "",
                           const std::string& srn = "");

// Wpis nocy ze STR do historii (ADR-0035 pkt 4): data bez rekordu → dopisana; rekord 'S' tej daty → zastąpiony, gdy
// treść inna; rekord z AirBridge, który nie jest pełny (rekordPelny() false) i nie ma czasu z maską (m01 < 0) →
// zastąpiony (pola, których STR nie ma, zostają puste); rekord z karty ('K'), pełny z AirBridge albo z AirBridge
// z czasem z maską → bez zmian. Zwraca true, gdy historia się zmieniła.
bool wpiszNocS(std::vector<Noc>& hist, const Noc& n);
