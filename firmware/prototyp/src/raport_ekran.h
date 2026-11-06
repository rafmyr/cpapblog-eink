// SPDX-License-Identifier: GPL-3.0-or-later
// Ekrany raportu dla lekarza (5.13.6, R-LEK-18), „Raport dla lekarza” z kodem QR (artboard
// R-raport-qr) i ekran-ściąga (artboard R-sciaga). Ekrany danych w tonie „spokój”, bez budzika; kod QR czarny na
// białym polu ze strefą ciszy ≥ 4 moduły (R-LEK-06). Stan błędu R-LEK-05 rysuje dalej rysujRaport() z src/ekrany.h.
#pragma once
#include <string>

#include "ekrany.h"

struct DaneEkranuRaportu {
  Naglowek naglowek;      // `{dzień tyg.} {data} · raport gotowy` / `Wi-Fi włączone`
  std::string temat;      // tytuł wzorca L albo `Raport ogólny, 90 nocy`
  std::string okres;      // `Okres: {od}–{do} · 1 strona A4`
  std::string adresQr;    // treść kodu QR (adres z tokenem)
  std::string adresReczny;  // `albo wpisz: {adres}`
  std::string kodWazny;   // `Kod ważny do {hh:mm}.`
  std::string stan;       // `Pobrano o {hh:mm}.` albo `Starsze raporty widoczne do {hh:mm}.` ("" = bez)
  std::string pobrano;     // `Pobrano o {hh:mm}.` (R-LEK-21), pod `Kod ważny do …`; "" = bez
  bool starsze = false;   // po ↻ 2 s (R-LEK-33): akcja `↻ 2 s starsze` już wykonana, znika ze stopki
};
// Ekran z kodem QR. Odchylenie od kanwy z powodu długości tokenu (128 bitów, T-NF-27): adres do wpisania stoi
// w osobnym wierszu nad stopką, a stopka z lewej ma stan (R-LEK-21, R-LEK-33). Zwraca false, gdy adresu nie da się
// zakodować (rysuje wtedy tylko ramkę).
bool rysujRaportQr(Plotno& p, const DaneEkranuRaportu& d);

struct DaneSciagi {
  Naglowek naglowek;
  std::string temat, pytanie;
  std::string liczby[3];  // 30 nocy przed · od początku wzorca · ostatnie 7
  std::string etykiety[3];
  std::string podpis;     // `{parametr} · okres {od}–{do}`
  std::string adresQr;    // EDF z serwera panelu ("" = bez kodu)
};
bool rysujSciage(Plotno& p, const DaneSciagi& d);

// Kod QR w polu x, y, bok (białe pole, moduły czarne, strefa ciszy ≥ 4 moduły, moduł całkowitą liczbą pikseli).
// Zwraca rozmiar modułu w px (0 = nie zmieścił się albo treść za długa).
int rysujKodQr(Plotno& p, int x, int y, int bok, const std::string& tresc);

// R-LEK-22: `Nie mogę włączyć Wi-Fi` (raport w pamięci panelu); ścieżka pliku na karcie tylko, gdy jest karta (panel
// w wersji beta nie obsługuje karty microSD, więc `sciezkaKarty` pusta). Stopka `◀ wróć` · `▶ dalej` · `↻ ekran dnia`.
void rysujRaportBezWifi(Plotno& p, const Naglowek& n, const std::string& temat, const std::string& sciezkaKarty);
