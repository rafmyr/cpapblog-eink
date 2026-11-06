// SPDX-License-Identifier: GPL-3.0-or-later AND MIT
// SPDX-FileCopyrightText: 2026 Rafał Myrta
// SPDX-FileCopyrightText: Project Nayuki
// Implementacja w src/qr.cpp według Project Nayuki (MIT), nota licencyjna w src/qr.cpp i LICENSES/MIT.txt.
// Kod QR (ISO/IEC 18004) do ekranu „Raport dla lekarza” i ekranu-ściągi (5.13.6, R-LEK-06, R-LEK-18).
// Własna minimalna implementacja bez biblioteki zewnętrznej: tryb bajtowy, poziom korekcji M (ok. 15% słów), wersje
// 1–10 (21×21 do 57×57 modułów, do 213 bajtów treści), maska wybrana regułami kar normy (N1, N2, N4; N3 pominięta,
// każda maska jest poprawna dla czytnika). Algorytm według opisu normy i referencyjnej implementacji Project Nayuki
// (MIT), napisany od nowa. Bez wyjątków i bez alokacji poza std::vector. Bez zależności od Arduino.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace qr {

struct Kod {
  int rozmiar = 0;                // modułów w boku (17 + 4 × wersja); 0 = treść za długa
  int wersja = 0;
  int maska = -1;
  std::vector<uint8_t> moduly;    // rozmiar × rozmiar, wiersz po wierszu, 1 = moduł ciemny
  bool ciemny(int x, int y) const {
    return x >= 0 && y >= 0 && x < rozmiar && y < rozmiar && moduly[(size_t)y * (size_t)rozmiar + (size_t)x] != 0;
  }
};

// Najmniejsza wersja 1–10, w której mieści się `tresc` (bajty, np. adres URL w ASCII); rozmiar 0, gdy się nie mieści.
// `maska` 0–7 wymusza maskę (testy), -1 = wybór według kar.
Kod koduj(const std::string& tresc, int maska = -1);

// Strefa ciszy wymagana przy rysowaniu (R-LEK-06): co najmniej 4 moduły z każdej strony.
const int STREFA_CISZY = 4;

}  // namespace qr
