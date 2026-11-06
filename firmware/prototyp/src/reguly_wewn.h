// SPDX-License-Identifier: GPL-3.0-or-later
// Deklaracje wewnętrzne silnika reguł, wspólne dla src/reguly.cpp, src/reguly_parser.cpp,
// src/reguly_rodzaje.cpp i src/reguly_pamiec.cpp. Nie dla innych paczek: one używają
// src/reguly.h i src/reguly_pomoc.h.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "reguly.h"

namespace reguly {
namespace wewn {

std::vector<std::string> dziel(const std::string& s, char sep);
std::string przytnij(const std::string& s);
std::vector<std::string> lista(const std::string& s);  // po przecinku, bez pustych
std::string male(const std::string& s);
bool liczbaZ(const std::string& t, double& v);  // liczba z kropką albo przecinkiem, cała treść pola
inline constexpr char KLUCZ_TRYBU[] = "\x1f";  // rozdziela id_progu i tryb w kluczu Zestaw::progi
const char* nazwaTrybu(Tryb t);
bool trybNaLiscie(const std::string& lista, Tryb t);  // "wszystkie"/puste = cztery tryby; NIEZNANY nigdy

std::map<std::string, FunkcjaRodzaju>& rejestr();
std::map<std::string, FunkcjaRodzaju> rdzen();  // rodzaje rdzenia (src/reguly_rodzaje.cpp)

// Noce historii do `dzis` po dacie rosnąco, jedna na datę (przy powtórzonej dacie rekord pełniejszy:
// kompletny przed częściowym, ważny przed nieważnym, potem dłuższy czas z maską), z trybem każdej nocy (ustawienia
// puste = ostatnie znane wcześniej, R-NOR-07). `znane` = w historii są jakiekolwiek ustawienia.
struct NocTrybu {
  const Noc* n;
  Tryb tryb;
};
std::vector<NocTrybu> nocePosortowane(const Kontekst& k, bool& znane);
bool nocRodzaju(const Kontekst& k, const Noc& n, const std::string& liczone);
const Noc* nocDzis(const Kontekst& k, const std::string& liczone, Tryb& tryb);

enum class Trzy : unsigned char { FALSZ, PRAWDA, NIEZNANE };
Trzy stanReguly(const Kontekst& k, const std::string& ref, std::string& powod);
bool regulaPominieta(const Kontekst& k, const std::string& ref);
bool regulaZnana(const Kontekst& k, const std::string& ref);
bool pasujeDoRef(const Wynik& w, const std::string& ref);
std::string jakoKlucz(const WierszKatalogu& w);

// Test permutacyjny `porownanie_grup` (R-NF-24, propozycja spec technicznej 9.3, „[etap 3]”, do potwierdzenia w WY5):
const int PRZETASOWANIA = 2000;
const double POZIOM_ISTOTNOSCI = 0.05;  // łącznie dla wierszy `porownanie_grup` jednego przebiegu, poprawka Holma

}  // namespace wewn
}  // namespace reguly
