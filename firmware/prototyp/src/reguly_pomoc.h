// SPDX-License-Identifier: GPL-3.0-or-later
// Pomocnicze silnika reguł dla rodzajów warunków spoza rdzenia (src/rodzaje_e.cpp, src/rodzaje_l.cpp)
// i ocena z logiem i sygnałami. Rozszerzenie obok kontraktu src/reguly.h (bez jego zmiany).
// Opis parametrów rodzajów rdzenia: komentarz na górze src/reguly.cpp.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "reguly.h"
#include "reguly_sygnaly.h"

namespace reguly {

using FunkcjaLogu = std::function<void(const std::string&)>;

struct OpcjeOceny {
  FunkcjaLogu log;                 // jeden wiersz na regułę „nie oceniam”: `reguly: {id}[/{wariant}] nie oceniam: {powód}`
  const Sygnaly* sygnaly = nullptr;  // dla `sygnal_komponentu`; nullptr = żaden komponent nic nie podał
};
// To samo co ocen(z, k) z logiem i sygnałami. ocen(z, k) = ocen(z, k, OpcjeOceny{}).
std::vector<Wynik> ocen(const Zestaw& z, const Kontekst& k, const OpcjeOceny& o);
// Sygnały bieżącej oceny (ważne tylko w czasie wywołania ocen(); poza nim nullptr).
const Sygnaly* sygnalyOceny();
// Historia kontekstu posortowana do liczNorme() (= posortujHistorie(*k.historia)); w czasie jednej oceny ocen()
// liczona raz na historię (T-WYM-99: historia w ocenie jest stała), poza oceną za każdym razem. Pusta przy
// k.historia == nullptr.
HistoriaPos historiaPos(const Kontekst& k);
// T-WYM-99: skrót wszystkich wejść ocen(z, k, o): historia (każde pole każdej nocy, skrotNocy()), data, tryb bieżący,
// odpowiedzi, pamięć reguł (każde pole), noce wyłączone, początek leczenia, adres zestawu, sygnały `s` (każde pole;
// nullptr = brak) i `spModel` (data nowego modelu maski z NVS, od której wołający liczy historię do normy). Poza
// kluczem pola pochodne: `norma` i `stany` (liczone przez wołającego z historii, daty, nocy wyłączonych i `spModel`)
// oraz `wczesniejsze` (ustawia ocen()). Ten sam klucz = ten sam wynik ocen() i te same wiersze logu. Nowe pole
// w Kontekst, Sygnaly, Odpowiedzi albo PamiecReguly zatrzymuje kompilację src/reguly.cpp (liczba_pol.h), dopóki
// nie trafi do klucza albo na listę pól pochodnych; mutacje pole po polu: test/testy_szybciej_przycisk.cpp.
uint64_t kluczOceny(const Kontekst& k, const Sygnaly* s, const std::string& spModel);

// Wartość parametru `nazwa` wiersza albo nullptr.
const std::string* parametr(const WierszKatalogu& w, const char* nazwa);
// Próg wskazany parametrem `nazwa` (id_progu) dla trybu bieżącego kontekstu; nullptr i `powod` przy braku parametru
// albo progu (T-WYM-66: reguła niepoliczalna).
const Prog* progParametru(const WierszKatalogu& w, const Kontekst& k, const char* nazwa, std::string& powod);
// Wynik „nie oceniam” z powodem (id, wariant, poziom, grupa z wiersza).
Wynik nieoceniona(const WierszKatalogu& w, const std::string& powod);
// Wynik oceniony (spełniony albo nie), pola z wiersza.
Wynik oceniona(const WierszKatalogu& w, bool spelniona);
// Porównanie `a {por} b` (`ge` domyślnie, `gt`, `le`, `lt`); false przy NAN.
bool porownaj(double a, const std::string& por, double b);
// Tryb nocy z przeniesieniem ostatnich znanych ustawień (pole puste = jak ostatnie znane wcześniej, R-NOR-07).
Tryb trybNocy(const Kontekst& k, const Noc& n);
// Klucz wyniku: "id" albo "id/wariant".
std::string kluczWyniku(const std::string& id, const std::string& wariant);
// Liczba jak na ekranie: całkowita bez przecinka, inaczej jedno miejsce po przecinku (przecinek1); NAN → "–".
std::string liczbaTekst(double v);
// Wartość progu jak w pliku progów: do 3 miejsc po przecinku bez zer na końcu (1,25 → "1,25", 5 → "5"); przeglądu.
std::string liczbaProgu(double v);
// Nazwy rodzajów rdzenia (rejestrowane przy pierwszym użyciu rejestru).
std::vector<std::string> rodzajeRdzenia();

}  // namespace reguly
