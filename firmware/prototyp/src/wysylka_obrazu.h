// SPDX-License-Identifier: GPL-3.0-or-later
// Wysyłka obrazu do sterownika e-ink kawałkami (T-KOMP-17, od 22.11.2026): część bez Arduino, testowana na PC
// (test/testy_wysylka_obrazu.cpp). Adapter ekranu (src/adaptery_esp32.cpp, klasa SterownikBlokiem) wysyła obraz
// całej powierzchni jednym blokiem SPI (`SPIClass::writeBytes`) zamiast bajt po bajcie (`transfer(bajt)` w GxEPD2);
// kanwa ma 1 = czarny, sterownik UC8179 1 = biały, więc bajty idą odwrócone (`~`), jak w GxEPD2 z `invert = true`.
// Odwracanie i podział na kawałki są tutaj: wynik ma być bajt w bajt taki, jak pętla biblioteki
// (GxEPD2 1.6.9 `GxEPD2_750_GDEY075T7::_writeImage()` dla x = 0, y = 0, w = 800, h = 480: bajty 0…47 999 po kolei).
#pragma once

#include <cstddef>
#include <cstdint>

namespace wysylka_obrazu {

// Bufor roboczy adaptera: kilkaset bajtów w pamięci wewnętrznej, nie 48 KB (06.11.2026 zabrakło pamięci
// wewnętrznej). Wielokrotność 4 B: `spiWriteNL()` z Arduino ESP32 czyta dane słowami 32-bitowymi.
constexpr size_t KAWALEK_B = 512;

// Odbiorca kawałka: `dane` ważne tylko w czasie wywołania, `dl` od 1 do pojemności bufora.
using Pisz = void (*)(void* kontekst, const uint8_t* dane, size_t dl);

// Wysyła `n` bajtów `zrodlo` z odwróconymi bitami (`~bajt`) kawałkami po najwyżej `pojemnosc` B przez `bufor`.
// Zwraca liczbę wywołań `pisz`; 0 i nic nie wysłane, gdy brak bufora, zerowa pojemność, brak źródła albo `n` = 0.
size_t wyslijOdwrocone(const uint8_t* zrodlo, size_t n, uint8_t* bufor, size_t pojemnosc, Pisz pisz,
                       void* kontekst);

// Wysyła `n` razy bajt `wartosc` (GxEPD2 `_writeScreenBuffer()`: czyszczenie pamięci sterownika) tak samo kawałkami.
size_t wyslijWartosc(uint8_t wartosc, size_t n, uint8_t* bufor, size_t pojemnosc, Pisz pisz, void* kontekst);

}  // namespace wysylka_obrazu
