// SPDX-License-Identifier: GPL-3.0-or-later
// Kontrakt silnika reguł.
// Zasada ADR-0023 i spec techniczna 9.3: reguła to wiersz katalogu (co, kiedy, jak pilne, jakim tekstem),
// rodzaj warunku to kod (jak liczyć). Ten plik ustala typy i funkcje, na których pracują równolegle.
// Implementacja: src/reguly.cpp. Zmiana tego pliku zmienia kontrakt silnika dla wszystkich rodzajów warunków.
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include "noc.h"
#include "stany.h"

namespace reguly {

struct Wynik;

// Stan reguły w zestawie (ADR-0029 pkt 3): AKTYWNA ma ekran, pilność i linię uwagi; TYLKO_RAPORT liczy się,
// trafia do logu, K2 i „Otwartych spraw” z wierszem `w raporcie, bez pilności`, blokuje uspokojenie, ale nie ma
// ekranu, pilności, buzzera ani linii; ZABLOKOWANA (np. L-06) nie liczy się wcale.
enum class StanReguly : unsigned char { AKTYWNA, TYLKO_RAPORT, ZABLOKOWANA };

// Pilność wzorca L (5.12.2). BRAK dla zdarzeń E, hipotez H i reguł „tylko raport”.
enum class Pilnosc : unsigned char { BRAK, A, B, C };

// Tryb terapii nocy albo bieżący (4.11, ADR-0030). ocenia cztery tryby (CPAP, AutoSet, ASV, ASVAuto;
// wycofane); tryb nierozpoznany (np. `VAuto`, przypadek 7.8 nr 50) = NIEZNANY → „nie oceniam”.
enum class Tryb : unsigned char { NIEZNANY, CPAP, AUTOSET, ASV, ASVAUTO };
Tryb trybZUstawien(const std::string& ustawienia);  // pole Noc::ustawienia, np. "ASVAuto/6/10/3/15"

// Wiersz pliku progów (9.3): wartość liczbowa albo godzina jako tekst; tryb "wszystkie" albo lista po przecinku.
struct Prog {
  std::string id, jednostka, tryb;
  double wartosc = 0;
  std::string tekst;  // gdy wartość nie jest liczbą (np. "18:00")
};

// Wiersz katalogu reguł (9.3, pola w tej kolejności). Pola z listą trzymają surowy tekst; parametry rozbite.
struct WierszKatalogu {
  std::string id, wariant, rodzaj;
  std::map<std::string, std::string> parametry;  // nazwa → id_progu albo stała słowna (metryka=M-02)
  std::string liczoneNoce, okno, poziom, grupa, slot, tryby;
  bool wymagaNormy = false, wRozruchu = false;
  std::vector<std::string> wyklucza, wymaga;
  std::string bramki, limit;
  std::vector<std::string> teksty;  // id_tekstu
  StanReguly stan = StanReguly::AKTYWNA;  // pole: z kolumny `stan` (ADR-0029); brak kolumny = AKTYWNA
  Pilnosc pilnosc = Pilnosc::BRAK;        // pole: pilność docelowa wzorca L przy stanie AKTYWNA
};

struct Tekst {
  std::string id, rola, tekst;
  int limit = 0;
};

// Zestaw: trzy pliki z jednym identyfikatorem (T-WYM-64). Zestaw wbudowany generuje tools/zestaw/buduj_zestaw.py
// do src/zestaw_wbudowany.h (nikt go nie pisze ręcznie).
struct Zestaw {
  std::string identyfikator;
  std::map<std::string, Prog> progi;
  std::vector<WierszKatalogu> katalog;
  std::map<std::string, Tekst> teksty;
  const Prog* prog(const std::string& id, Tryb tryb) const;  // wiersz trybu, inaczej "wszystkie", inaczej nullptr
};
// Parsuje trzy pliki (z nagłówkiem #cpap-eink … skrot=sha256:…); false i powód przy złym nagłówku, skrócie,
// nieznanym rodzaju warunku albo id progu/tekstu, którego nie ma.
bool wczytajZestaw(const std::string& progi, const std::string& katalog, const std::string& teksty, Zestaw& z,
                   std::string& powod);
const Zestaw& zestawWbudowany();

// Odpowiedzi tak/nie (5.12.8): senność, „gorzej”, przerwa planowana; data poranka → odpowiedź.
struct Odpowiedzi {
  std::map<std::string, bool> sennosc, gorzej;
  std::set<std::string> przerwaPlanowana;
};

// Pamięć reguł między przebiegami (rodzaje `trwanie`, `od_zdarzenia`, stany wzorca 5.12.12). Plik /dane/reguly.csv
// (format w rejestrze magazynu): id;wariant;aktywna_od;ostatnio;licznik;stan_wzorca;wiem;omowione
struct PamiecReguly {
  std::string aktywnaOd, ostatnio;  // data poranka, "" = nigdy
  int licznik = 0;
  std::string stanWzorca;           // nazwa stanu z 5.12.12 (znaczenia: src/wzorce.h)
  std::string wiem, omowione;       // data poranka naciśnięcia „wiem” / „Omówione z lekarzem” (zapis: src/sprawy.h)
};
using Pamiec = std::map<std::string, PamiecReguly>;  // klucz: "id" albo "id/wariant"

// Wejście jednej oceny: wszystko, czego rodzaj warunku może potrzebować. Rodzaje nie czytają plików ani sieci.
struct Kontekst {
  const std::vector<Noc>* historia = nullptr;  // po dacie rosnąco
  std::string dzis;                            // data poranka bieżącej oceny
  const Norma* norma = nullptr;
  const Stany* stany = nullptr;
  Tryb trybBiezacy = Tryb::NIEZNANY;
  const Odpowiedzi* odpowiedzi = nullptr;
  const Pamiec* pamiec = nullptr;
  const Zestaw* zestaw = nullptr;
  // wyniki reguł już ocenionych w tym przebiegu (złożenie, `wymaga`, `wyklucza`, `trwanie`)
  const std::map<std::string, Wynik>* wczesniejsze = nullptr;
  // daty poranków nocy wyłączonych z norm czynnikami nocy (R-CZY-05: infekcja, spanie poza domem); ten sam zbiór,
  // który dostaje liczNorme i Stany. Rodzaje liczące własną normę muszą go uwzględnić.
  const std::set<std::string>* wykluczoneNoce = nullptr;
  // R-SKI-08 (przegląd kliniczny): data rozpoczęcia leczenia z ustawienia „Początek leczenia”
  // (klucz `poczatek_leczenia=RRRR-MM-DD` w konfiguracji, 5.6.4); "" = nieznana, wtedy L-01 bez karencji.
  std::string poczatekLeczenia;
};

// Wynik reguły. `oceniona == false` = „nie oceniam” (R-DANE-28, ADR-0029 pkt 7): za mało nocy, brak metryki, zły tryb.
// Nigdy nie traktuj „nie oceniam” jak „w porządku”.
struct Wynik {
  std::string id, wariant;
  bool oceniona = false, spelniona = false;
  std::string powodNieoceny;                    // do logu, np. "za malo nocy 9/14"
  StanReguly stan = StanReguly::AKTYWNA;
  Pilnosc pilnosc = Pilnosc::BRAK;
  std::string poziom, grupa;                    // z katalogu
  std::map<std::string, std::string> zmienne;   // do tekstów: {data}, {n}, {wartosc}, …
};

// Rodzaj warunku: czysta funkcja wiersza i kontekstu. Rejestr nazw = pole `rodzaj` katalogu (tabela 9.3).
using FunkcjaRodzaju = Wynik (*)(const WierszKatalogu&, const Kontekst&);
void zarejestrujRodzaj(const std::string& nazwa, FunkcjaRodzaju f);
bool znanyRodzaj(const std::string& nazwa);

// Ocena całego zestawu w kolejności katalogu z wykluczeniami i `wymaga` (9.3, wiersz `zlozenie`).
std::vector<Wynik> ocen(const Zestaw& z, const Kontekst& k);
// Tekst z podstawionymi {zmienna} i {prog:id}; "" gdy tekstu nie ma.
std::string tekst(const Zestaw& z, const std::string& idTekstu, const Wynik& w, Tryb tryb);

// Pomocnicze wspólne dla rodzajów: wartość metryki M-xx z rekordu nocy (NAN = brak), noce danego rodzaju
// z pola `liczone_noce` (5.12.3) w oknie do `dzis` włącznie, mediana jedną metodą dla norm i okien.
double metryka(const Noc& n, const std::string& idMetryki);
std::vector<const Noc*> noceOkna(const Kontekst& k, const std::string& liczoneNoce, int ileNocy);
double mediana(std::vector<double> v);

}  // namespace reguly
