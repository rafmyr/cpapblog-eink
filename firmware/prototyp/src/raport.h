// SPDX-License-Identifier: GPL-3.0-or-later
// Raport A4 dla lekarza (moduł RAPORT, spec funkcjonalna 5.13; komponent T-KOMP-20 spec technicznej).
// Trzy kroki, każdy bez zależności od Arduino i sieci:
//   1. zbuduj(): z danych panelu (historia nocy, norma, wyniki reguł, sprawy, „tylko raport”, odpowiedzi tak/nie,
//      czynniki według konfiguracji, tryb terapii) treść stref 1–10 z tabeli 5.13.3, teksty dosłownie ze spec;
//   2. pdf(): jedna strona A4 PDF (595 × 842 pt), czarno-biała, ≤ 250 kB (R-LEK-12), strefy w kolejności z akapitu
//      „Pierwsza połowa strony”: 1, 2, 4, 3, 3b, 5, 6 i 8 (lewa kolumna) obok 7, 9, 10 (układ artboardu Raport-A4-L01);
// 3. sciaga: treść ekranu-ściągi (R-LEK-18), gdy wydanie nie ma PDF (warunek ADR-0005).
// Format: PDF 1.4 składany na panelu bez biblioteki: tekst krojem standardowym Helvetica (bez osadzania, kodowanie
// WinAnsi z tablicą /Differences dla polskich liter) i Symbol (≥, ≤, →), wykresy wektorowo. Strumień treści bez
// kompresji (ok. 20–40 kB), więc bez zlib na panelu. Odchylenie od 5.13.3 („Noto Sans … w podzbiorze”): krój nie
// jest osadzony; do decyzji (raport).
// Granice: ADR-0002 (strefa 6 to jedyne miejsce z wartościami ustawień, jako opis stanu; nigdzie wartości docelowych
// ani drogi do zmiany, R-LEK-07), ADR-0001 (nic nie wychodzi poza panel), R-LEK-15 (bez danych osobowych).
#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "czynniki.h"
#include "noc.h"
#include "reguly.h"
#include "sprawy.h"
#include "stany.h"

namespace raport {

enum class Rodzaj : unsigned char { TEMATYCZNY, OGOLNY, PRZED_PO };
// Wariant wydania (ADR-0005): PDF przez kod QR z lokalnego serwera albo ekran-ściąga (R-LEK-18).
enum class Wariant : unsigned char { PDF, SCIAGA };
// Ustawienie `raport=pdf|sciaga` w `/konfiguracja.txt` (wiersze `klucz=wartość`, `#` komentarz). Domyślnie PDF:
// R-LEK-18 dotyczy wydania bez generatora PDF, a ta wersja go ma; rozstrzygnięcie wydania: WY8.
Wariant wariantZKonfiguracji(const std::string& trescKonfiguracji);
// Czynniki nocy włączone do raportu (5.11.7, R-CZY-06): ustawienie `raport.czynniki=K,L,…` (kody z src/czynniki.h);
// puste ustawienie albo jego brak = domyślne 5.13.7 (katar/infekcja i leki tylko przy L-01). Zwraca bity Czynnik;
// `ustawione` = czy ustawienie było w pliku.
uint8_t czynnikiZKonfiguracji(const std::string& trescKonfiguracji, bool& ustawione);

struct Wejscie {
  std::string dzis;    // data poranka (ostatnia noc ≤ dzis)
  std::string czas;    // `hh:mm` wygenerowania; "" = nieznany
  const std::vector<Noc>* historia = nullptr;          // po dacie rosnąco
  const std::set<std::string>* wykluczone = nullptr;   // noce wyłączone czynnikami (R-CZY-05)
  const Stany* stany = nullptr;                        // stany(dzis): metryki w stanie odchylenia
  const sprawy::SprawaL* temat = nullptr;              // nullptr = raport ogólny (▶ w K2)
  const std::vector<sprawy::SprawaL>* sprawy = nullptr;
  const std::vector<reguly::Wynik>* wyniki = nullptr;
  const reguly::Pamiec* pamiec = nullptr;
  const reguly::Zestaw* zestaw = nullptr;
  sprawy::FunkcjaTekstu tekst;                         // tekst z zestawu po podstawieniu zmiennych
  const std::vector<sprawy::Odpowiedz>* odpowiedzi = nullptr;
  const std::vector<WpisCzynnikow>* czynniki = nullptr;
  uint8_t czynnikiWlaczone = 0;  // bity Czynnik
  bool czynnikiUstawione = false;  // false = domyślne 5.13.7
  bool podpowiedziH = true;        // R-HIP-07: wyłączona podpowiedź = bez strefy 3b i pytanie ogólne wzorca
  std::string model;               // `pna` aparatu (np. `AirCurve_10_CS_PaceWave`); "" = nieznany
  std::string wersjaPanelu, wersjaAirBridge;
  std::set<std::string> niewalidowane;  // `Metryki niewalidowane` (R-LEK-19), np. "M-02"
};

// Wykres nocy (strefy 4 i 5): jedna wartość na datę poranka od najstarszej; NAN = brak (luka, R-KOK-02).
struct Wykres {
  std::string tytul, podpis;            // nad wykresem i pod nim (legenda)
  std::vector<std::string> daty;
  std::vector<float> w, w2;             // w2: druga seria (np. P95, CA/h kropkami); puste = brak
  std::vector<char> znak;               // 'p' noc ważna (pełne kółko), 'o' niepewna (puste kółko), 'x' nieważna, ' ' brak
  float pasmoOd = NAN, pasmoDo = NAN;   // P10–P90 normy sprzed wzorca
  std::vector<float> linie;             // poziome linie przerywane (próg, minimum i maksimum ustawień)
  std::vector<int> zmiany;              // indeksy nocy ze zmianą ustawień
  int poczatek = -1;                    // indeks nocy początku wzorca
  bool brakPomiaru = false;             // `brak pomiaru`, bez wykresu (fail-path R-LEK-13)
  bool bezLiczb = false;                // metryka niewalidowana: bez liczb osi i linii (R-LEK-19)
};
struct Tabela {
  std::vector<std::string> kolumny;              // pierwsza = nazwa wiersza (zwykle "")
  std::vector<std::vector<std::string>> wiersze;
};

struct Raport {
  Rodzaj rodzaj = Rodzaj::OGOLNY;
  std::string temat;         // tytuł wzorca albo `Raport ogólny, 90 nocy`
  std::string klucz, idL;    // klucz sprawy ("" w raporcie ogólnym)
  std::string od, do_;       // okres (daty poranka)
  int nocy = 0, waznych = 0;
  // 1 nagłówek
  std::string s1tytul, s1okres, s1pacjent;
  // 2 dlaczego ten raport: zdania w kolejności z 5.13.3 (powód, objawy, daty zmian ustawień, reszta)
  std::vector<std::string> s2;
  // 4 wykres kluczowy i tabelka (30 nocy przed · od początku wzorca · ostatnie 7)
  Wykres s4;
  Tabela s4tab;
  std::string s4legenda;
  // 3 i 3b
  std::string s3pytanie, s3dopisek;
  bool s3bJest = false;
  std::vector<std::string> s3b;
  std::string s3bDopisek;
  // 5 kontekst: małe wykresy i zdania R-LEK-35
  std::vector<Wykres> s5;
  std::vector<std::string> s5zdania;
  // 6 ustawienia teraz i tabela zmian
  std::vector<std::string> s6;
  Tabela s6zmiany;
  std::string s6bezZmian;
  // 7 regularność i podsumowanie
  Tabela s7;
  std::string s7kryterium, s7dopisek;
  // 8, 9, 10
  std::vector<std::string> s8, s9, s10;
  // ekran-ściąga (R-LEK-18)
  std::string sciagaPytanie, sciagaPodpis;
  std::string sciagaLiczby[3];
};

// Treść stref; zawsze się udaje (brak danych = `–` i zdania fail-path), błąd daje dopiero składanie PDF.
Raport zbuduj(const Wejscie& w);

// Kody błędów R-LEK-05 (`Nie udało się przygotować raportu ({kod})`).
const char* const KOD_STRONA = "R2";    // treść nie mieści się na jednej stronie A4 nawet mniejszym krojem
const char* const KOD_ROZMIAR = "R3";   // PDF większy niż 250 kB (R-LEK-12)
const char* const KOD_ZAPIS = "R4";     // zapis w `/raporty/` nieudany (R-LEK-14, R-LEK-23)
const char* const KOD_BRAK_DANYCH = "R5";  // brak historii nocy (nie ma z czego złożyć raportu)
const size_t PDF_MAKS = static_cast<size_t>(250) * 1024;
// PDF jednej strony A4; "" i `blad` = kod przy błędzie.
std::string pdf(const Raport& r, std::string& blad);
// Nazwa pliku w pamięci panelu (T-DANE-25): `/raporty/RRRR-MM-DD-{temat}.pdf`, temat = klucz wzorca bez `/`
// (np. `L-03-ge10`), `ogolny` albo `przed-po`; kolejny raport tego samego dnia i tematu dostaje `-2`, `-3` (R-LEK-14:
// stary plik zostaje).
std::string nazwaPliku(const Raport& r, const std::string& dzis, const std::vector<std::string>& istniejace);

// Tekst całej strony w kolejności druku (testy: przegląd procedurą 9.3, R-LEK-07, R-LEK-15; kolejność R-LEK-12).
std::vector<std::string> wierszeTekstu(const Raport& r);

// Pomocnicze do testów.
std::string liczba(double v);  // jak przecinek1(), NAN → `–`
const char* const ZDANIE_R_LEK_28 = "Panel nie ostrzega o nagłych stanach. Przy duszności lub bólu w klatce wezwij pomoc.";
const char* const ZDANIE_R_LEK_34 =
    "Wskazanie i bezpieczeństwo leczenia ASV, w tym ocenę chorób serca, weryfikuje lekarz prowadzący.";
const char* const ZDANIE_STOPKI = "Zestawienie danych z aparatu, nie dokumentacja medyczna. Ocena należy do lekarza.";
const char* const PYTANIE_SENNOSC =
    "Czy w ostatnim tygodniu zdarzyło Ci się zasnąć w dzień mimo woli, w sytuacji, w której chciałeś pozostać "
    "przytomny?";

}  // namespace raport
