// SPDX-License-Identifier: GPL-3.0-or-later
// Wysyłka obrazu do sterownika e-ink kawałkami (opis w src/wysylka_obrazu.h).
#include "wysylka_obrazu.h"

#include <cstring>

namespace wysylka_obrazu {

size_t wyslijOdwrocone(const uint8_t* zrodlo, size_t n, uint8_t* bufor, size_t pojemnosc, Pisz pisz,
                       void* kontekst) {
  if (!zrodlo || !bufor || pojemnosc == 0 || !pisz || n == 0) return 0;
  size_t wywolan = 0;
  for (size_t od = 0; od < n; od += pojemnosc) {
    const size_t dl = n - od < pojemnosc ? n - od : pojemnosc;
    for (size_t i = 0; i < dl; i++) bufor[i] = static_cast<uint8_t>(~zrodlo[od + i]);
    pisz(kontekst, bufor, dl);
    wywolan++;
  }
  return wywolan;
}

size_t wyslijWartosc(uint8_t wartosc, size_t n, uint8_t* bufor, size_t pojemnosc, Pisz pisz, void* kontekst) {
  if (!bufor || pojemnosc == 0 || !pisz || n == 0) return 0;
  // bufor wypełniony raz: każdy kawałek ma te same bajty
  memset(bufor, wartosc, n < pojemnosc ? n : pojemnosc);
  size_t wywolan = 0;
  for (size_t od = 0; od < n; od += pojemnosc) {
    pisz(kontekst, bufor, n - od < pojemnosc ? n - od : pojemnosc);
    wywolan++;
  }
  return wywolan;
}

}  // namespace wysylka_obrazu
