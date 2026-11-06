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
void rysujSlotDzienny(Plotno& p, const Naglowek& n, const WyborSlotu& w, const TekstySlotu& t, const WejscieSlotu& we,
                      const StopkaDzienna& s);
// S-K-38: obraz wyśrodkowany w polu 800×434 na białym tle, bez nagłówka, ze stopką dzienną (R-KON-55).
void rysujObrazekDnia(Plotno& p, const ObrazPbm& o, const StopkaDzienna& s);
// Ekran danych S-K-01, S-K-02, S-K-13 (szablon A, ton „spokój”).
void rysujEkranKontekstu(Plotno& p, const Naglowek& n, const TrescKontekstu& k, const std::string& stopkaLewo,
                         const std::string& stopkaPrawo);
// Nagłówek ekranów slotu dziennego: `wt 03.11`.
std::string naglowekSlotu(const std::string& dzis);
