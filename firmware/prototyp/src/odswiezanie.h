// SPDX-License-Identifier: GPL-3.0-or-later
// Rodzaj odświeżenia e-ink (R-NF-25, T-INT-27, T-KOMP-17): pełne (z błyskiem) albo częściowe (szybki przebieg
// całej powierzchni ekranu bez błysku, sterownik UC8179 przez GxEPD2: `refresh(0, 0, 800, 480)` z tablicą szybkiego
// odświeżenia z OTP). Rdzeń (dyrygent) decyduje o rodzaju funkcją rodzajOdswiezenia(), adapter ekranu wykonuje
// (src/adaptery.h Wyswietlacz::odswiezJako()), atrapa zapisuje rodzaj do testów.
//
// Reguła (R-NF-25): częściowe tylko dla widoku otwartego przyciskiem (kokpit, pytania, potwierdzenia, strony Portretu)
// i tylko wtedy, gdy sterownik ma wiarygodny poprzedni obraz (ten sam przebieg programu już odświeżył e-ink; po
// głębokim śnie i po starcie zawsze pełne). Pełne zawsze: ekran dnia (w tym S9 i informacja U-07 na jego
// miejscu), każde odświeżenie z własnej inicjatywy panelu (zegar, powrót po 5 min, drugi ekran poranka), ekran testu,
// samosprawdzenie, rysowanie bez zegara, ekrany z kodem QR i serwisowe (U-02, U-03, U-07, raport), co N-te
// odświeżenie częściowe z rzędu i N = 0 (częściowe wyłączone).
#pragma once

#include <cstdint>
#include <string>

#include "nawigacja.h"

enum class Odswiezenie : uint8_t { PELNE = 0, CZESCIOWE = 1 };

namespace odswiezanie {

// [PRÓG] (R-NF-25; wiersz `R-NF-25` w docs/spec/progi-poza-zakresem.csv, parametr techniczny, nie `id_progu`
// z progi.csv) po tylu odświeżeniach częściowych z rzędu następne jest pełne; 0 = częściowe wyłączone
// (wszystko pełne, jak przed 16.11.2026). Klucz `eink.pelne_co` w /konfiguracja.txt (bez pola w formularzu U-03,
// przełącznik do prób na panelu), liczba całkowita 0…PELNE_CO_MAX; zła wartość = wiersz pominięty, wartość domyślna.
const int PELNE_CO_DOMYSLNIE = 5;
const int PELNE_CO_MAX = 20;
extern const char* const KLUCZ_PELNE_CO;  // `eink.pelne_co`

struct Wejscie {
  Ekran ekran = Ekran::DZIEN;   // ekran rysowany teraz
  bool poPrzycisku = false;     // rysowanie po akcji przycisku (budzącego, w czuwaniu albo z kolejki naciśnięć)
  bool obrazZnany = false;      // ten przebieg programu już odświeżył e-ink (sterownik i adapter znają poprzedni obraz)
  bool zawszePelne = false;     // ekran testu, samosprawdzenie wersji, rysowanie bez zegara (Przebieg::pokaz())
  // na płótnie ekran dnia, choć ekran() mówi co innego (potwierdzenie albo „Otwarte sprawy” bez danych rysuje
  // w pokaz() ekran dnia, `!pokazB5()` → `rysujDzien()`): pełne
  bool naPlotnieDzien = false;
  int pelneCo = PELNE_CO_DOMYSLNIE;  // N; 0 = częściowe wyłączone
  int czesciowychZRzedu = 0;    // licznik z pamięci RTC (StanRtc::czesciowychZRzedu)
};

// Ekran, który po przycisku może dostać odświeżenie częściowe: strony kokpitu, pytanie, potwierdzenia, U-01,
// strony Portretu. Ekran dnia, test U-05, ekrany z kodem QR (U-02, U-03, raport) i U-07: zawsze pełne.
bool ekranCzesciowy(Ekran e);

// Rodzaj odświeżenia według R-NF-25 (opis reguły wyżej).
Odswiezenie rodzajOdswiezenia(const Wejscie& w);

// Licznik po wykonanym odświeżeniu: częściowe +1 (najwyżej 255), pełne 0.
uint8_t licznikPo(Odswiezenie wykonane, int przed);

// `pelne` / `czesciowe` (log, wiersz POMIAR).
const char* nazwa(Odswiezenie r);

// Wartość klucza `eink.pelne_co`: true i `n` przy liczbie całkowitej 0…PELNE_CO_MAX, inaczej false.
bool pelneCoZTekstu(const std::string& v, int& n);

}  // namespace odswiezanie
