// SPDX-License-Identifier: GPL-3.0-or-later
// Obrazek dnia S-K-38 (spec 5.8.12, ADR-0027 pkt 4, 10 i 11, R-KON-54, R-KON-55, R-KON-59…R-KON-62), bez Arduino.
//
// Format na panelu (propozycja wpisana do spec technicznej 8.6): katalog `/obrazki/` w pamięci panelu
// (LittleFS, przez magazyn; karta microSD jako przyszłe źródło tego samego katalogu), w nim:
//   - pliki obrazków w formacie PBM binarnym (`P4`): nagłówek tekstowy `P4\n{szer} {wys}\n`, potem wiersze bitów
//     od lewej, bit 1 = czarny, wiersz dopełniony do pełnego bajtu; szerokość i wysokość do 800×434 (R-KON-55,
//     bez skalowania na panelu); PBM to standard (czytają go GIMP, ImageMagick, PIL), więc obrazek da się obejrzeć
//     na komputerze, a parser na panelu ma kilkanaście linii;
//   - manifest `obrazki.json` (`{"wersja":1,"obrazki":[{"plik":"001_x.pbm","dni":[…],"pogoda":[…],"sezon":[…],
//     "inne":[…]}, …]}`); panel czyta tylko `plik` i cztery tagi, resztę pól ignoruje (5.8.12).
// Pliki przygotowuje poza panelem skrypt tools/obrazki/przygotuj.py z obrazków użytkownika; obrazki i manifest nie
// trafiają do repozytorium (.gitignore), w testach są tylko obrazki syntetyczne.
// Dziennik `/obrazki.csv` (R-KON-61): `data;plik;pula` (pula `k` kontekstowa, `n` neutralna), jeden wiersz na dzień
// z S-K-38.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "slot_dzienny.h"

const int OBRAZEK_MAX_W = 800, OBRAZEK_MAX_H = 434;  // pole nad stopką dzienną (R-KON-55)
extern const char* const KATALOG_OBRAZKOW;           // "/obrazki"
extern const char* const MANIFEST_OBRAZKOW;          // "/obrazki/obrazki.json"
extern const char* const DZIENNIK_OBRAZKOW;          // "/obrazki.csv"

struct PozycjaManifestu {
  std::string plik;
  std::vector<std::string> dni, pogoda, sezon, inne;
};
// Manifest → pozycje (z nazwą pliku; pozycje bez `plik` pominięte). false: brak listy `obrazki` albo JSON nieczytelny.
bool parsujManifest(const std::string& json, std::vector<PozycjaManifestu>& wynik);

// Kontekst doboru: tylko kalendarz i pogoda, nigdy dane terapii (R-KON-59). Prognoza nullptr = tagi pogody nie pasują.
struct KontekstObrazka {
  std::string dzis;
  int godzina = 0;                     // horyzont pogody od tej godziny do 22 (jak reguła wietrzenia)
  const Prognoza* prognoza = nullptr;
};
// Wartości tagów, które pasują dziś (np. {"wt", "listopad", "deszcz"}); znane wartości z tabeli 5.8.12.
std::vector<std::string> tagiDzis(const KontekstObrazka& k);
// Czy pozycja ma choć jeden znany tag kontekstu (nieznane wartości, np. `sen`, `mgla`, są ignorowane: R-KON-62).
bool maTagKontekstu(const PozycjaManifestu& p);
bool pasujeDzis(const PozycjaManifestu& p, const std::vector<std::string>& tagi);

struct WpisObrazka {
  std::string data, plik;
  char pula = 'n';
};
bool parsujWpisObrazka(const std::string& linia, WpisObrazka& w);
std::string zapiszWpisObrazka(const WpisObrazka& w);

struct WyborObrazka {
  std::string plik;   // pełna ścieżka pliku w katalogu (`/obrazki/005_x.pbm`); "" = brak obrazka (R-KON-54)
  char pula = 'n';
  bool nowy = false;  // true: wybór dziś pierwszy raz, dopisać WpisObrazka do dziennika
  std::vector<std::string> log;  // `obrazki: bez manifestu`, `obrazki: pominięty {plik}: {powód}`
};
// pliki: pełne ścieżki plików z katalogu (Pliki::listuj); manifest: nullptr przy braku albo nieczytelnym (R-KON-62:
// wtedy wszystkie pliki `.pbm` jako jedna pula neutralna). czytelny(plik): czy plik da się pokazać (nagłówek PBM,
// rozmiar do 800×434); kandydat nieczytelny jest pomijany, z wpisem w logu. Ten sam dzień z tym samym katalogiem
// i dziennikiem daje ten sam plik (R-KON-59, R-KON-61): wpis z dziś w dzienniku wygrywa; inaczej pula według licznika
// dni S-K-38 w dzienniku (parzysty: kontekstowa, nieparzysty: neutralna, pusta pula: druga), w puli obrazki
// z najdawniejszym ostatnim pokazaniem (najpierw nigdy niepokazane), z nich pozycja z numeru dnia.
WyborObrazka wybierzObrazek(const std::vector<std::string>& pliki, const std::vector<PozycjaManifestu>* manifest,
                            const std::vector<WpisObrazka>& dziennik, const KontekstObrazka& k,
                            const std::function<bool(const std::string&)>& czytelny);

// PBM `P4` → bitmapa (bit 1 = czarny, wiersz co (w + 7) / 8 bajtów). false z powodem przy złym nagłówku, rozmiarze
// ponad 800×434 albo za krótkich danych.
struct ObrazPbm {
  int w = 0, h = 0;
  std::vector<uint8_t> bity;
};
bool parsujPbm(const std::string& dane, ObrazPbm& o, std::string& powod);
