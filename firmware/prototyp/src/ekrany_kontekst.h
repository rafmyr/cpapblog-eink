// SPDX-License-Identifier: GPL-3.0-or-later
// Rysowanie ekranów (slot dzienny S-K-33…S-K-39, obrazek dnia, ekrany CO2 S-K-01, S-K-02, S-K-13) według
// kanwy „interfejs docelowy v1” (design/kanwa/SK33-*.dc.html … SK39-*.dc.html, współrzędne 1:1 z SVG artboardów)
// i skali B (docs/design/czytelnosc-b/SPEC-B.md). S-K-01, S-K-02 i S-K-13 nie mają artboardu w skali B (kanwa ma ich
// starszy szablon `Ekran`), więc stoją w szablonie A ekranów S-K-34…S-K-36 z linią CO2 jak w artboardach SK01, SK13.
// Karta S-K-01 (K05) idzie przez rysujKarte() z src/ekrany.h (scena W01, budzik zawsze na bieli, ADR-0009).
#pragma once
#include "ekrany.h"
#include "kontekst.h"
#include "obrazki.h"
#include "slot_dzienny.h"

// Ekran slotu dziennego (S-K-33…S-K-37, S-K-39): nagłówek `{dzień tyg.} {dd.mm}` / `prognoza {hh:mm}` (S-K-39:
// `drzemka {hh:mm}–{hh:mm}`), treść według wyboru, stopka dzienna (R-KON-51). S-K-39 w negatywie (ton „problem”).
// Nagłówek ekranów slotu dziennego (bez belki, ADR-0043): lewo data, prawo `prognoza {hh:mm}` albo `drzemka …`.
struct NaglowekSlotu {
  std::string lewo, prawo;
};
void rysujSlotDzienny(Plotno& p, const NaglowekSlotu& n, const WyborSlotu& w, const TekstySlotu& t, const WejscieSlotu& we,
                      const StopkaDzienna& s);
// S-K-38: obraz wyśrodkowany w polu 800×434 na białym tle, bez nagłówka, ze stopką dzienną (R-KON-55).
void rysujObrazekDnia(Plotno& p, const ObrazPbm& o, const StopkaDzienna& s);
// Ekran danych S-K-01, S-K-02, S-K-13 (szablon A, ton „spokój”).
void rysujEkranKontekstu(Plotno& p, const Belka& n, const TrescKontekstu& k, const std::string& stopkaLewo,
                         const std::string& stopkaPrawo);
// Podpis godziny na osi wykresów slotu dziennego (S-K-33 słupki i linia temperatury, S-K-36, pas doby S-K-37):
// godzina zegarowa `HH:00` (`20:00`, `07:00`; g modulo 24), żeby nie myliła się z wartością na osi (R-KON-63).
std::string godzinaOsiSlotu(int g);
// Nagłówek ekranów slotu dziennego: `wt 03.11`.
std::string naglowekSlotu(const std::string& dzis);

// ---------- S-K-48 „Porada dnia” programu „Dobry start” (ADR-0049, spec 5.8.14.3.3) ----------
// Ekran danych z belką (`Dobry start`, środek z OpisPorady, prawe pole jak na innych belkach), etykieta P22B na y 78,
// pasek tygodni (porady planu) z prawej, tytuł SB44 w 1 linii na y 142, treść P24S do 5 linii po 34 px od y 192,
// krok P24B na y 408, stopka dzienna (R-KON-51). Karta (`karta` i scena): treść P22B do 4 linii po 30 px w 490 px,
// krok P22B, budzik z prawym dolnym rogiem w (774, 425), stopka tylko z temperaturami (`stopka.lewo` puste).
// Z linią uwagi (7.5): linia pod belką, bez etykiety i paska, tytuł i treść przy linii 60 px o 12 px niżej (R-DS-18).
// Przy „tylko na telefonie” OpisPorady ma tytuł `Nowa porada na telefonie.` bez treści i kroku (R-DS-50).
struct DanePorady {
  Belka belka;
  dobry_start::OpisPorady o;
  std::string scena;  // "" = ekran danych; inaczej karta z tą sceną
  StopkaDzienna stopka;
  std::string linia;
  bool liniaA = false, liniaOstrzezenie = false;
};
const int PORADA_X = 28, PORADA_KOLUMNA = 748, PORADA_KOLUMNA_KARTY = 490;
const int PORADA_LINIE = 5, PORADA_LINIE_KARTY = 4;
const int PORADA_TYTUL_Y = 142, PORADA_TRESC_Y = 192;  // linie bazowe tytułu i pierwszej linii treści (spec 5.8.14.5)
const int PORADA_PASEK_PRAWO = 774, PORADA_PASEK_Y = 62, PORADA_PASEK_W = 20, PORADA_PASEK_H = 13, PORADA_PASEK_KROK = 26;
void rysujPorade(Plotno& p, const DanePorady& d);
