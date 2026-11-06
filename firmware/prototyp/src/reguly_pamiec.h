// SPDX-License-Identifier: GPL-3.0-or-later
// Pamięć reguł między przebiegami (src/reguly.h PamiecReguly; rodzaje `trwanie`, `od_zdarzenia`, stany wzorca 5.12.12).
// Plik /dane/reguly.csv, format `reguly` wersji 1 w rejestrze magazynu (src/magazyn.cpp), przepisywany atomowo:
//   id;wariant;aktywna_od;ostatnio;licznik;stan_wzorca;wiem;omowione
// Pola `stan_wzorca` oraz `wiem` i `omowione` aktualizujPamiec zostawia bez zmian.
#pragma once
#include <string>
#include <vector>

#include "magazyn.h"
#include "reguly.h"

namespace reguly {

const char* const FORMAT_PAMIECI = "reguly";
const char* const SCIEZKA_PAMIECI = "/dane/reguly.csv";

// Klucz pamięci: "id" albo "id/wariant" (ten sam co klucz wyniku).
std::string kluczPamieci(const std::string& id, const std::string& wariant);
// Odczyt pliku; brak, plik uszkodzony albo wiersz nieczytelny (licznik nie jest liczbą, za mało pól) → pominięty.
Pamiec czytajPamiec(Magazyn& m);
// Zapis atomowy całej pamięci (wiersze po kluczu); false przy błędzie magazynu.
bool zapiszPamiec(Magazyn& m, const Pamiec& p);
// Po ocenie poranka `dzis`:
//  - wynik oceniony i spełniony: `aktywna_od` = dzis, jeśli puste; `ostatnio` = dzis; `licznik` + 1 raz na dzień;
//  - wynik oceniony i niespełniony: `aktywna_od` i `licznik` skasowane (`ostatnio` zostaje: ostatnie spełnienie);
//  - „nie oceniam”: bez zmian (R-DANE-28: brak oceny nie kończy stanu).
void aktualizujPamiec(Pamiec& p, const std::vector<Wynik>& wyniki, const std::string& dzis);

}  // namespace reguly
