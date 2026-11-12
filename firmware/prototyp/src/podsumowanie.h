// SPDX-License-Identifier: GPL-3.0-or-later
// Podsumowanie miesiąca S7 (5.4.7), „Twoja norma” S8 (5.4.8), „Co znaczą te liczby” (5.4.14) i ekran danych odkrycia
// S1 (6.5) według kanwy „interfejs docelowy v1” (S7-miesiac, S8-norma*, X-co-znacza-liczby*, O12-alkohol) i skali B.
// Bez zależności od Arduino. Rysowanie: wspólny szablon ekranu danych (rysujEkranDanych:
// nagłówek, etykieta, tytuł, krok, stopka) i elementy ekranu (liczby, wiersze, słupki) dorysowane tutaj.
#pragma once
#include <string>
#include <vector>

#include "ekrany.h"
#include "noc.h"
#include "odkrycia.h"
#include "reguly.h"

namespace podsumowanie {

extern const char* const MIESIACE_M[12];   // mianownik: „styczeń” … „grudzień” (`Twój listopad`)
extern const char* const MIESIACE_MS[12];  // miejscownik: „styczniu” … „grudniu” (`niż w październiku`)

// ---------- S7 (5.4.7, R-EKR-18…R-EKR-20) ----------
// Kandydat 1. dnia miesiąca, gdy poprzedni miesiąc ma co najmniej `minNocy` [PRÓG 20] nocy ważnych.
bool kandydatS7(const std::string& dzis, const std::vector<Noc>& hist, int minNocy = 20);
struct DaneS7 {
  std::string etykieta;                      // `Twój listopad`
  std::string noce, z, podpisNocy;           // `29`, `/30`, `nocy ponad 4 h`
  std::string ahi, podpisAhi;                // `1,0`, `AHI, mediana` (w obu profilach, bez wariantu publicznego)
  std::string godziny, podpisGodzin;         // `198`, `z maską` (`h` przy liczbie)
  std::string zdanie;                        // zdanie porównania albo "" (R-EKR-20)
  std::string metrykaZdania;                 // do logu: metryka wybrana (R-EKR-19)
};
// `tryb` bieżący: w ASV i ASVAuto ciśnienie w szczycie to EPAP (M-06p, 4.11).
DaneS7 budujS7(const std::string& dzis, const std::vector<Noc>& hist, reguly::Tryb tryb, bool profilPubliczny,
               int minNocy = 20);
TrescEkranu trescS7(const DaneS7& d);  // etykieta i stopka; liczby i zdanie dorysowuje rysujS7()
void rysujS7(Plotno& p, const Naglowek& n, const DaneS7& d, const TrescEkranu& t);

// ---------- S8 (5.4.8, R-EKR-33…R-EKR-36) ----------
struct WejscieS8 {
  const Norma* norma = nullptr;
  reguly::Tryb tryb = reguly::Tryb::NIEZNANY;
  bool profilPubliczny = false;
  bool p199 = false;      // test zaliczony (krok o paśmie centralnych poza ASV); dziś nie
  bool wzorzecL = false;  // aktywny wzorzec L (także wyciszony) albo „tylko raport” ze spełnionym warunkiem (R-SKI-44)
};
struct DaneS8 {
  std::string etykieta = "Norma osobista", tytul = "Twoja norma";
  std::vector<std::string> wiersze;
  bool centralne = false;    // wiersz `Centralne` (CA/h z normą)
  std::string krok;
  bool informacja = false;   // ton „informacja” (R-EKR-36): krok w ramce
};
DaneS8 budujS8(const WejscieS8& w);
// Kandydat R7 jednorazowo (R-EKR-33): panel ma pierwszy raz 30 nocy ważnych w oknie normy i S8 nie było pokazane.
bool kandydatS8(const Norma& n, bool pokazane);
TrescEkranu trescS8(const DaneS8& d);
void rysujS8(Plotno& p, const Naglowek& n, const DaneS8& d, const TrescEkranu& t);

// ---------- „Co znaczą te liczby” (5.4.14, R-EKR-51) ----------
struct WierszLiczb { std::string nazwa, opis; };  // `Czas z maską:` + ` ile godzin aparat pracował.`
struct DaneLiczb {
  // strona kokpitu bez numeru po K2 (R-KOK-22): nazwa strony w nagłówku, bez powtórzenia jej etykietą w treści
  std::string naglowek = "Co znaczą te liczby";
  std::string etykieta;
  std::vector<WierszLiczb> wiersze;
  std::string zdanie = "Co jest dobre dla ciebie, powie lekarz.";
  std::string stopkaLewo = "◀ 7 nocy   ▶ ustawienia   ↻ ekran dnia";  // 5.5.7 „Stopki stron”
};
// `sprawy`: za tą stroną stoją „Otwarte sprawy” (▶ sprawy), inaczej U-01 (▶ ustawienia), R-KOK-22.
DaneLiczb budujLiczby(reguly::Tryb tryb, bool centralne, bool sprawy = false);
void rysujLiczby(Plotno& p, const Naglowek& n, const DaneLiczb& d);

// ---------- S1: odkrycie jako ekran danych (6.5, R-HUM-13) ----------
TrescEkranu trescS1(const odkrycia::TrescOdkrycia& o);  // tytuł, powód, krok, fakty karty (2 słupki)
void rysujOdkrycie(Plotno& p, const Naglowek& n, const odkrycia::TrescOdkrycia& o, const TrescEkranu& t);

}  // namespace podsumowanie
