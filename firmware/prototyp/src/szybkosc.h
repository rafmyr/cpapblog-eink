// SPDX-License-Identifier: GPL-3.0-or-later
// Szybkość ścieżki „przycisk → rysowanie” (pomiar na panelu 16.11.2026, T-WYM-99): przełącznik pamięci podręcznej
// dla testów równoważności i punkty czasu faz dla obrazu pomiarowego (`e1001_pomiar`). Rdzeń bez Arduino.
#pragma once

namespace szybkosc {
// true = każda pamięć podręczna z T-WYM-99 liczy od nowa (stany, `sp_model`); tylko testy równoważności
// (test/testy_szybciej_przycisk.cpp). Panel zawsze false.
extern bool bezPamieci;
// Liczba przebiegów reguly::ocen() z wbudowanym zestawem (bez trafień w pamięć wyniku); tylko rośnie, do testów.
extern unsigned long przebiegiSilnika;
// Haczyk punktu czasu fazy: ustawia go tylko adapter ESP32 w obrazie z POMIAR_CZASU (src/adaptery_esp32.cpp);
// nullptr = nic. `nazwa` to stały napis (`historia`, `reguly`, `strona` …).
extern void (*haczykFazy)(const char* nazwa);
// Punkt końca fazy `nazwa`: woła haczyk, gdy ustawiony; w zwykłych obrazach jedno porównanie wskaźnika.
inline void faza(const char* nazwa) {
  if (haczykFazy) haczykFazy(nazwa);
}
}  // namespace szybkosc
