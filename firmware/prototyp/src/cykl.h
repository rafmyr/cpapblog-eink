// SPDX-License-Identifier: GPL-3.0-or-later
// Decyzje jednego wybudzenia panelu (bez Arduino): czy noc jest znana, czy jest brak danych (S5), czy wolno
// wybrać ekran dnia i przerysować ekran, oraz poranna decyzja o pytaniu „Co było inaczej?”.
// Dyrygent (src/dyrygent.cpp) zbiera wejście (zegar, odczyt AirBridge, pliki) i wykonuje wynik; test: test/testy.cpp (doba).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "czynniki.h"
#include "humor.h"
#include "noc.h"

struct WejscieDnia {
  std::string dzis;               // data poranka (dziś); "" = bez zegara
  int minuta = 12 * 60;           // minuta doby, czas lokalny
  const std::vector<Noc>* hist = nullptr;
  bool odczytOk = false;          // ostatnie pobranie z AirBridge się udało
  std::string abData;             // data ostatniego udanego odczytu AirBridge ("" = brak)
  int abMinuta = -1;              // minuta doby ostatniego udanego odczytu
  std::string kluczBraku;         // epizod braku danych (R-HUM-08): czas ostatniego udanego odczytu
  bool kartaBraku = true;         // false: brak Wi-Fi albo nieczytelny raport, S5 zawsze ekranem danych (A-59)
  bool e03 = false;               // ostatnie pobranie dało E-03 (raport ostatniej nocy albo lista nieczytelne, wersja
                                  // AirBridge spoza zakresu, R-AW-02): S5 z E-03 także przy nocy „znanej” ze statusu
  bool wBudowie = false;          // AirBridge odpowiada, ostatnia noc w budowie (R-DANE-35, R-AW-15, `partial`)
  bool terapia = false;           // `rop` = 1: terapia trwa, noc się jeszcze nie skończyła
  bool maNorme = false;
  bool sypialnia = false;         // S2c możliwe (StanDnia::sypialnia)
  // E-05 krytyczne (bateria ekranu ≤ 5%, 6.4): S5 po E-01 i E-03 (R1), zawsze ekranem danych (R-HUM-08), klucz
  // braku `E-05`. Ustawia dyrygent z wyniku silnika reguł w jednym miejscu (wejscieDnia()): wybór ekranu dnia,
  // także w slocie nocnym (S5 baterii zostaje do 05:00), decyzja o S9 i nawigacja (↻ = nowy pomiar); nie
  // w planie pobierania i nie w dniu pełnego ekranu wzorca A (R-PIE-07: wtedy linia `Naładuj ekran: bateria {x}%.`).
  bool e05Krytyczne = false;
};

// Okno poranne: wybór ekranu dnia i decyzje poranne od 05:00 (poza slotem nocnym 23:30–05:00, R-PRZ-18).
bool oknoDnia(int minuta);
// U3: ostatnia noc jest znana, gdy jest w historii albo AirBridge odpowiedział dziś po 05:00 (odczyt przed 05:00
// może dotyczyć jeszcze nocy sprzed doby albo trwającej).
bool nocZnana(const WejscieDnia& w);
// S5: ostatni odczyt nieudany i noc nieznana.
// S5: nieudany odczyt przy nieznanej ostatniej nocy albo E-03 (R-AW-02), gdy ostatniej nocy nie ma w historii (zapisany
// rekord zostaje na ekranie, R-DANE-24 fail-path).
// E-01 dopiero od 11:00 [PRÓG] (R-DANE-20, 6.4): w oknie dnia przed 11:00 nieudany odczyt to „dane w drodze” (S10).
bool brakDanych(const WejscieDnia& w);
// S5 z przyczyny po stronie AirBridge (E-01, E-03), bez E-05 krytycznego; brakDanych() = ta albo e05Krytyczne.
bool brakDanychAirBridge(const WejscieDnia& w);
// S10 „Synchronizuję się z aparatem” (R-EKR-57): okno dnia, ostatniej nocy nie ma w historii, bez E-01 i E-03,
// terapia nie trwa, a noc może jeszcze przyjść: przed 11:00 albo noc w budowie przy odpowiadającym AirBridge.
bool synchronizacja(const WejscieDnia& w);
StanDnia stanDnia(const WejscieDnia& w);

struct DecyzjaDnia {
  bool nowyWybor = false;  // false w slocie nocnym: ekran z ostatniego wpisu dziennika, bez nowego wpisu
  bool zapisz = true;      // false: wybór tymczasowy przed nocą z dziś (do 11:00), ekran danych bez wpisu
  EkranDnia e;
  std::string dataEkranu;  // data, dla której liczyć treść (w nocy: data ostatniego wpisu)
  const WpisHumoru* wpis = nullptr;  // wpis dziennika tego ekranu, gdy już był
};
DecyzjaDnia decyzjaDnia(const WejscieDnia& w, const std::vector<WpisHumoru>& log);

// Odświeżenie po zegarze: w slocie nocnym ekranu nie przerysowujemy (R-PRZ-18, błysk e-ink w sypialni).
bool przerysowacPoZegarze(int minuta);

// Poranna decyzja o pytaniu (R-CZY-01, R-CZY-02, R-CZY-03, R-PIE-13): zamyka wpisy po terminie, a w oknie dnia
// dla nocy z dziś bez wpisu dodaje zaproszenie z terminem od chwili pierwszego zobaczenia nocy. Zwraca true przy zmianie.
bool poranekCzynnikow(std::vector<WpisCzynnikow>& cz, const std::vector<Noc>& hist, const std::string& dzis, int minuta,
                      const Norma& norma, uint32_t ziarno, const OkolicznosciRanka& ok = OkolicznosciRanka());
