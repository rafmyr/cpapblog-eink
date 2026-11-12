// SPDX-License-Identifier: GPL-3.0-or-later
// Portret nocy na ekranie (S11, spec 5.4.16, ADR-0040, T-WYM-97): tarcza 12 h z formą „pierścień z pikami” rysowaną
// w buforze 1 bit testem przynależności piksela we współrzędnych biegunowych (tylko w pierścieniu od promienia 96 do
// 172, kwadrat promienia w liczbach całkowitych, kąt z tablicy stałoprzecinkowej), znacznik startu okna (kreska przez
// pierścień i grot zgodnie ze wskazówkami, bez napisu; od 12.11.2026), cyfry, kreski i tekst zwykłymi prymitywami;
// obok liczby nocy, podpisy i legenda. Bez liczb zmiennoprzecinkowych w wyniku: obrazek ten sam w symulatorze
// i na panelu (R-POR-22). Wzorzec wyglądu: tools/portret/rysunek.py (porównanie w T-TEST-415).
#pragma once
#include <string>
#include <utility>
#include <vector>
#include "ekrany.h"
#include "portret.h"

struct DanePortretu {
  const portret::Przebieg* przebieg = nullptr;
  std::string tryb;                  // tryb aparatu ostatniej nocy: CPAP, AutoSet, ASV, ASVAuto
  int normaPs10 = portret::BRAK;     // mediana normy M-28 w 0,1 cmH2O; BRAK = rozruch metryki („uczę się”)
  int zwykla = 11 * 60;              // zwykła pora startu terapii w minutach od 12:00 dnia `day`
  std::string pierwszy;              // pierwszy podpis: zdanie o normie z reguł S2a (R-POR-12)
  std::string czas, ahi;             // `6 h 38`, `1,1` (brak AHI: `–`)
  bool niepewna = false;             // noc niepewna: liczby `{czas} z maską` bez AHI, pierwszy podpis o tej nocy (R-POR-15, R-POR-25)
};

namespace portret {

const int OKNO_MIN = 720;            // tarcza 12 h
const int PRZED_STARTEM_MIN = 120;   // okno od zwykłej pory startu minus 2 h [PRÓG]
const int MIN_TERAPII = 15;          // R-POR-16 [PRÓG]
const int SKALA_LINII = 85;          // tarcza w skali 0,85 przy linii uwagi [PRÓG] (setne)

bool jestPortret(const Przebieg& p);                 // R-POR-16: sesje razem co najmniej 15 min
int minutyTerapii(const Przebieg& p);
// Początek okna tarczy w minutach od 12:00 (wielokrotność 5): zwykła pora startu minus 2 h; gdy najdłuższa sesja nie
// mieści się w nim w całości, od jej startu minus 2 h (koniec sesji w oknie), a sesja dłuższa niż 12 h od jej startu.
int okno(const DanePortretu& d);
std::string hhmmOdPoludnia(int minutyOdPoludnia);     // `hh:mm` czasu zegarowego z minut od 12:00 dnia `day`
bool asv(const std::string& tryb);
// Wysokość piku odcinka w 1/16 px przy skali `skala` (setne): na zewnątrz (R-POR-09, R-POR-10) i do środka.
int pikZewn16(const DanePortretu& d, const Odcinek& o, int skala = 100);
int pikWewn16(const DanePortretu& d, const Odcinek& o, int skala = 100);
// Pierwszy podpis: `pierwszy` (zdanie o normie), po nocy niepewnej opis tej nocy (R-POR-25).
std::string pierwszyPodpis(const DanePortretu& d);
// Podpisy o brakującym pomiarze („B”, R-POR-12) w kolejności tabeli „Podpisy”: (pełny podpis, krótka nazwa).
std::vector<std::pair<std::string, std::string>> braki(const DanePortretu& d);
// Podpisy „B” na `miejsc` miejsc (R-POR-27): gdy braków jest więcej, pierwsze miejsc − 1 pełnym podpisem, reszta
// w jednym podpisie zbiorczym `Nieznane: {lista}.` (zawsze nazwy, w kolumnie najwyżej 2 linie po 382 px)
std::vector<std::string> podpisyBrakow(const DanePortretu& d, size_t miejsc);
// Podpisy w kolejności R-POR-12 (pierwszy, „B”, 2–7), najwyżej 4, z linią uwagi 2; brak pomiaru nigdy nie wypada.
std::vector<std::string> podpisy(const DanePortretu& d, bool linia);
// Klucze legendy (R-POR-13): maska, wyciek, wsparcie, zdarzenia, centralne, kropka; opis: opisLegendy().
std::vector<std::string> legenda(const DanePortretu& d);
std::string opisLegendy(const std::string& klucz);  // tekst S11.leg_{klucz} z zestawu
// Kąt piksela (dx, dy od środka, oś y w dół) w sekundach tarczy 0…43199 od godziny 12 zgodnie ze wskazówkami.
int katTarczy(int dx, int dy);

}  // namespace portret

// Strefa treści Portretu od y = `gora` (54, z linią uwagi 102) do 434: tarcza i kolumna (wołane z rysujEkranDanych()
// przy Uklad::PORTRET; nagłówek, linię uwagi i stopkę rysuje ekran danych).
void rysujCialoPortretu(Plotno& p, const DanePortretu& d, int gora, bool linia);
