// SPDX-License-Identifier: GPL-3.0-or-later
// Moduł SPRZĘT (spec 5.9; elementy tylko maska i filtr): wiek elementów i termin wymiany, zapis wymiany, S-K-31 „Maska
// się zużywa”, S-K-32 „Nowa maska działa”, pytanie `Wymieniona maska?` (R-MAS-12, R-MAS-13), licznik pokazań
// S-K-31 i blokada L-09 przy elemencie po terminie (R-MAS-03, R-MAS-08, R-MAS-10), termin wymiany do formularza U-03
// (R-MAS-17; ekran „Sprzęt” 5.9.3 wycofany w ADR-0041).
// Bez Arduino.
//
// Dane: daty wymiany i okresy z `/konfiguracja.txt` (src/slot_dzienny.h UstawieniaDnia; zapis wymiany dopisuje
// tam wiersz `wymiana.{element}=RRRR-MM-DD`, R-MAS-01: „data w pliku konfiguracji w pamięci panelu”), odpowiedź „tak”
// na `Wymieniona maska?` z pliku odpowiedzi (src/sprawy.h, Pytanie::PODUSZKA), pokazania S-K-31 z dziennika ekranów
// `/humor.csv` (typ `SK31`, sprawa `{element}@{data wymiany}`): licznik pokazań startuje od przekroczenia okresu
// (pokazania są możliwe dopiero po nim) i zeruje się przy nowej dacie wymiany (nowa sprawa). Bez osobnego pliku stanu.
#pragma once
#include <string>
#include <vector>
#include "ekrany.h"
#include "humor.h"
#include "noc.h"
#include "slot_dzienny.h"

namespace sprzet {

// Progi (docs/spec/progi.csv; wartości sprawdza tools/test_katalog_sk.py, wiersze `próg:` nad stałymi)
// próg: S-K-31.delta = 10
const float DELTA_M07 = 10;          // S-K-31 warunek (2), S-K-32 (Δ M-07, 4.5) [PRÓG]
// próg: S-K-31.noce_wzrostu = 14
const int SK31_NOCE_TRENDU = 14;     // S-K-31 warunek (3): wyciek rośnie od ≥ 14 nocy ważnych [PRÓG]
// próg: S-K-31.limit_dni = 7
const int SK31_CO_DNI = 7;           // S-K-31: raz na 7 dni, dopóki warunek trwa (7.4) [PRÓG]
// próg: R-MAS-03.pokazania = 4
const int SK31_POKAZAN_BLOKADY = 4;  // R-MAS-03, R-MAS-10: L-09 wraca po 4. pokazaniu bez zapisu wymiany [PRÓG]
// próg: R-MAS-08.po_wymianie = 14
const int L09_NOCE_PO_WYMIANIE = 14; // R-MAS-08: blokada L-09 jeszcze 14 nocy ważnych po wymianie [PRÓG]
// spec 5.9.5 „Kiedy” (bez osobnego progu w progi.csv)
const int SK32_NOCE_PO = 7;          // 5.9.5: po 7 nocach ważnych od wymiany
// spec 5.9.5 „Warunek”
const int SK32_NOCE_PRZED = 14;      // … wobec mediany z 14 nocy ważnych przed wymianą
// próg: R-MAS-09.grupa_min = 5
const int SK32_GRUPA_MIN = 5;        // R-MAS-09, R-EKR-26: co najmniej 5 nocy ważnych w każdej grupie [PRÓG]
// próg: R-MAS-12.noce = 3
const int PYT_NOCE_SPADKU = 3;       // R-MAS-12: 3 ostatnie noce ważne [PRÓG]
// próg: R-MAS-12.spadek = 8
const float PYT_SPADEK = 8;          // … niższe o ≥ 8 L/min [PRÓG]
// próg: R-MAS-12.odstep = 30
const int PYT_CO_DNI = 30;           // … kolejne pytanie najwcześniej po 30 dniach [PRÓG]

bool okresMaska(int element);                       // maska (z poduszką): S-K-31, S-K-32, L-09
int okresElementu(const ElementWymiany& e);         // okres: miesiące (maska) albo tygodnie (filtr), 5.9.2
// Po terminie: zapisana data i dziś ≥ data + okres (R-MAS-06: bez daty nigdy).
bool poTerminie(const ElementWymiany& e, const std::string& dzis);
int pelneMiesiace(const std::string& od, const std::string& dzis);  // wiek w pełnych miesiącach (`{m}`), -1 bez daty
std::string sprawaElementu(int element, const ElementWymiany& e);   // `maska@2026-04-01` (klucz dziennika)

// Daty wymiany po uwzględnieniu odpowiedzi „tak” na `Wymieniona maska?` (R-MAS-13): data poranka pierwszej z 3
// nocy spadku, gdy późniejsza od zapisanej. `odpowiedziTak` = daty poranka pytań z odpowiedzią „tak”.
struct Elementy {
  ElementWymiany el[EL_N];
};
Elementy zOdpowiedziami(const ElementWymiany (&el)[EL_N], const std::vector<Noc>& hist,
                        const std::vector<std::string>& odpowiedziTak,
                        const std::vector<std::string>& noceInfekcji = {});  // jak pytanie R-MAS-12

// ---------- S-K-31 (5.9.4, R-MAS-02) ----------
struct WejscieSprzetu {
  std::string dzis;                             // data poranka
  const std::vector<Noc>* hist = nullptr;       // po dacie rosnąco
  const Norma* norma = nullptr;                 // norma osobista (M-07 P95: wp95)
  const Elementy* el = nullptr;
  bool okresInfekcji = false;                   // warunek (4), R-INF-02
  const std::vector<WpisHumoru>* dziennik = nullptr;
};
struct WynikSK31 {
  bool warunek = false;      // warunki (1)–(4) spełnione (kandydat, zanim limit)
  bool limit = false;        // pokazany w ostatnich 7 dniach (7.4): warunek trwa, ekranu nie ma
  int element = -1;          // EL_MASKA
  int miesiecy = 0;          // `{m}`
  int tygodni = 0;           // `{t}`: 2, 3 albo 4
  std::vector<float> linia;  // M-07 P95 z 28 ostatnich nocy (wizualizacja), NAN = brak
  std::string sprawa;
  const char* dlaczego = "";
};
WynikSK31 ocenSK31(const WejscieSprzetu& w);
// Licznik pokazań S-K-31 elementu (`licznik_pokazan`): dni z wpisem `SK31` sprawy elementu od przekroczenia okresu.
int pokazaniaSK31(const std::vector<WpisHumoru>& dziennik, const std::string& sprawa);
// Blokada L-09 (bramka „maska po terminie”, 7.3): maska po terminie z mniej niż 4 pokazaniami S-K-31 (jeden licznik,
// R-MAS-03, R-MAS-10) albo mniej niż 14 nocy ważnych od zapisanej wymiany maski (R-MAS-08).
bool blokadaL09(const WejscieSprzetu& w, const char** powod = nullptr);
// Nachylenie prostej (L/min na noc) dopasowanej do wartości w kolejności; NAN przy < 2 wartościach.
double nachylenie(const std::vector<double>& v);

// ---------- S-K-32 (5.9.5, R-MAS-04, R-MAS-09) ----------
struct WynikSK32 {
  bool warunek = false;
  int element = -1;
  float przed = NAN, po = NAN;  // `{y}`, `{x}`
  int okresMies = 0;            // `{m}`
  std::string sprawa;
  const char* dlaczego = "";
};
// Jednorazowe dla każdej wymiany: wpis `SK32` z tą sprawą w dzienniku = już pokazane. Odkrycie: w rozruchu wyłączone
// (rozruch sprawdza wołający, 7.3).
WynikSK32 ocenSK32(const WejscieSprzetu& w);

// ---------- pytanie `Wymieniona maska?` (R-MAS-12) ----------
struct WynikPytaniaPoduszki {
  bool jest = false;
  std::string odNocy;     // data poranka pierwszej z 3 nocy spadku (R-MAS-13)
  float przed = NAN, po = NAN;  // `{y}`, `{x}`
};
// `pytania` = daty poranka wcześniejszych pytań (z każdą odpowiedzią), `noceInfekcji` = noce okresu infekcji (okno
// 14 nocy sięga wtedy dalej wstecz), `zmianaMaski` = data zmiany typu maski `MSK` albo nowego modelu (""= brak).
WynikPytaniaPoduszki ocenPytaniePoduszki(const WejscieSprzetu& w, const std::vector<std::string>& pytania,
                                         const std::vector<std::string>& noceInfekcji, const std::string& zmianaMaski);

// ---------- zapis wymiany (R-MAS-01) ----------
// Nowa treść `/konfiguracja.txt`: wiersz `wymiana.{element}=data` zastąpiony albo dopisany; reszta bez zmian.
std::string zapiszWymiane(const std::string& konfiguracja, int element, const std::string& data);
// Cofnięcie (R-MAS-01): data sprzed zapisu; "" = wiersz `wymiana.{element}` usunięty (element znów bez daty, R-MAS-06).
std::string przywrocWymiane(const std::string& konfiguracja, int element, const std::string& data);

// ---------- termin wymiany w formularzu U-03 (R-MAS-17) ----------
// Od ADR-0041 bez strony „Sprzęt” w kokpicie: termin następnej wymiany (data wymiany + okres z 5.9.2) stoi przy polu
// daty w formularzu: `następna wymiana {DD.MM.RRRR}`, po terminie `po terminie od {DD.MM.RRRR}`, bez daty
// `brak daty wymiany` (R-MAS-06).
std::string terminFormularza(const ElementWymiany& e, const std::string& dzis);
// Stopka ekranów z zapisem „wymienione dziś” (S-K-31, R-MAS-01; S-K-35, R-MAS-18): ta sama akcja, ten sam podpis,
// kolejność ◀ ▶ (5.6.1 pkt 6, R-PRZ-04).
extern const char* const STOPKA_WYMIANY_LEWO;   // `◀ 2 s: wymienione dziś`
extern const char* const STOPKA_WYMIANY_PRAWO;  // `▶ kokpit`

}  // namespace sprzet
