// SPDX-License-Identifier: GPL-3.0-or-later
// Ekrany zdarzeń E w tonie „problem” (stan S3, spec 5.4.2, katalog 6.4). Bez zależności od
// Arduino. Zdarzenie z poziomem „problem” (grupy R5b, R5c, R5d) przychodzi jako sprawy::SprawaL z tonem PROBLEM
// (sprawy::sprawyOtwarte()); wybór pełnego ekranu, rytm R-KON-14, „wiem” i linia `Wciąż: {tytuł}.` są w src/sprawy.*
// i src/kolejka.*, a ten plik składa treść ekranu.
//
// Ekran (szablon L09-wyciek w skali B, negatyw bez ramki, 5.4.3): nagłówek dnia, strefa linii uwagi (gdy kolejka ma
// linię), tytuł B3, powód (≤ 2 linie), mini wykres metryki z 30 ostatnich nocy (5.4.2 S3) z linią przerywaną progu
// zdarzenia (parametr `prog` katalogu) albo mediany normy (zdarzenia ze stanu odchylenia), krok B5, stopka `↻ 2 s wiem`
// (mapa 5.6.2: problem poranny). Tytuł, powód i krok wyłącznie z zestawu reguł (teksty 6.4, reguly::tekst); etykiety
// i podpisów osi spec dla zdarzeń E nie podaje, więc ekran ich nie ma (do decyzji, raport). Zawsze ekran danych,
// bez budzika (tabela 5.7.3: „zdarzenia E w tonie problem”).
#pragma once
#include <string>

#include "dzien.h"
#include "ekrany.h"
#include "sprawy.h"

namespace zdarzenia {

const int NOCY_WYKRESU = 30;  // 5.4.2, S3: „mini wykres metryki z 30 nocy”

// Metryka wykresu zdarzenia w trybie bieżącym (4.11: E-36 w ASV i ASVAuto M-06p, w CPAP i AutoSet M-05p); "" = brak.
std::string metrykaWykresu(const sprawy::SprawaL& s, reguly::Tryb tryb);
// Mediana normy metryki (linia przerywana przy zdarzeniu ze stanu odchylenia); NAN, gdy norma nie jest gotowa.
double medianaNormy(const Norma& nm, const std::string& metryka);
// Treść S3 dla sprawy, która dziś jest ekranem dnia; linia uwagi z `d.linia` (kolejka::ustawLinie przed wywołaniem).
DaneWzorca trescZdarzenia(const sprawy::SprawaL& s, const DaneDnia& d);

}  // namespace zdarzenia
