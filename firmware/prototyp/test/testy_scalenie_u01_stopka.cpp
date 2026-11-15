// SPDX-License-Identifier: GPL-3.0-or-later
// U-01 po połączeniu listy ustawień do odczytu (R-KONF-55) ze stanem oprogramowania (R-KONF-43, R-KONF-46, R-NF-33):
// dyrygent na atrapach rysuje na U-01 właściwy kafelek ↻ we wszystkich trzech stanach (token i nowsza wersja, token
// bez nowszej, bez tokenu; od 14.11.2026 kafelek zamiast wiersza pod listą), a tekst U-07 bez tokenu nie odsyła do
// elementu, którego na U-01 nie ma. Szerokości kafelka: test/testy_kokpit_legenda_u01_sprzet.cpp. Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_scalenie_u01_stopka).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "aktualizacja.h"
#include "dyrygent.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "nawigacja.h"
#include "sekrety.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* const TOKEN = "github_pat_ZNACZNIKTOKENU_0123456789abcdef";
static const uint8_t B_LEWY = 1u << (int)Przycisk::LEWY;

// wybudzenie przyciskiem ◀ na U-07: powrót na U-01 („nie teraz”), U-01 narysowane przez dyrygenta
static void naU01(Panel& p) {
  p.zegar.epoch = ep(2026, 11, 11, 9, 0);
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = B_LEWY;
  p.przyciski.skrypt = {{0, B_LEWY}, {200, 0}};
  p.start.stan.ekran = (uint8_t)Ekran::AKTUALIZACJA;
  p.start.stan.ekranOd = p.zegar.epoch - 10;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent d(p.adaptery(), KONF);
  d.wybudzenie();
}

// prostokąt bajtów bx0…bx1 (po 8 px) i y0…y1 bufora 1 bit (100 bajtów na wiersz)
static bool polaRowne(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int bx0, int bx1, int y0, int y1) {
  for (int y = y0; y < y1; y++)
    for (int x = bx0; x < bx1; x++)
      if (a[(size_t)(y * 100 + x)] != b[(size_t)(y * 100 + x)]) return false;
  return true;
}

// ---------- dyrygent: kafelek ↻ oprogramowania na U-01 we wszystkich trzech stanach ----------
// (od 14.11.2026 kafelek zamiast wiersza pod listą, decyzja 2; wiersz zostaje w logu `U-01 · …`)
// wymaganie: R-KONF-55, R-KONF-43, R-NF-33
static void testDyrygentKafelekU01() {
  struct Przypadek {
    bool token;
    std::string znana;
  };
  for (const Przypadek& c : {Przypadek{true, "1.0.1"}, Przypadek{true, ""}, Przypadek{false, ""}}) {
    Panel p;
    if (c.token) sekrety::zapiszKluczWydan(p.nvs, TOKEN);
    aktualizacja::Stan s = aktualizacja::wczytaj(p.nvs);
    s.znana = c.znana;
    aktualizacja::zapisz(p.nvs, s);
    naU01(p);
    SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::USTAWIENIA);
    SPRAWDZ(!p.ekran.klatki.empty());
    if (p.ekran.klatki.empty()) continue;
    const std::vector<uint8_t>& klatka = p.ekran.klatki.back();
    std::string wlasna = aktualizacja::bezBudowy(wersjaWydania(p.start.napisWersji));
    aktualizacja::KafelekOprogramowania k = aktualizacja::kafelekU01(c.token, c.znana, wlasna, "");
    // wzorzec: U-01 z tą samą wersją i stanem; porównany sam kafelek ↻ (x 496…772, y 254…350)
    DaneUstawien d;
    d.wersja = wlasna;
    d.stanOprogramowania = k.stan;
    d.stanOdwrocony = k.odwrocony;
    PlotnoAtrapy wz;
    rysujUstawienia(wz, d);
    bool rowny = polaRowne(klatka, wz.b, 496 / 8, 776 / 8, 254, 351);
    if (!rowny) printf("  U-01 z dyrygenta: kafelek ↻ różny od `%s`\n", k.stan.c_str());
    SPRAWDZ(rowny);
    SPRAWDZ(k.odwrocony == (c.token && !c.znana.empty()));
    // token nigdy w klatce jako tekst: stan kafelka i wiersz logu bez znacznika tokenu
    std::string w = aktualizacja::wierszU01(c.token, c.znana, wlasna) + k.stan;
    SPRAWDZ(w.find("ZNACZNIKTOKENU") == std::string::npos && w.find("github_pat") == std::string::npos);
  }
}

// ---------- tekst stanu nie odsyła do pozycji, której na U-01 nie ma ----------
// wymaganie: R-KONF-55, R-NF-33
static void testOdeslanieDoIstniejacegoElementu() {
  // bez tokenu U-07 mówi `Ustaw je w „Więcej na telefonie”.`; ta nazwa musi stać na U-01 (kafelek ▶),
  // inaczej tekst odsyła do elementu, którego na ekranie nie ma
  std::string t = aktualizacja::wierszU01(false, "", "1.0.0");
  const std::string cel = "Więcej na telefonie";
  if (t.find(cel) == std::string::npos) {
    ok++;
    return;
  }
  bool jest = false;
  DaneUstawien d;
  for (auto& w : wierszeUstawien(d)) jest = jest || w.nazwa.find(cel) != std::string::npos;
  for (const char* x : {U01_KAFELEK_TYTUL[0], U01_KAFELEK_TYTUL[1], U01_GRANICA})
    jest = jest || std::string(x).find(cel) != std::string::npos;
  for (auto& s : stopkaU01()) jest = jest || s.find(cel) != std::string::npos;
  if (!jest) printf("  U-01: `%s` odsyła do „%s”, którego na U-01 ani na formularzu już nie ma\n", t.c_str(), cel.c_str());
  SPRAWDZ(jest);
}

int main() {
  testDyrygentKafelekU01();
  testOdeslanieDoIstniejacegoElementu();
  printf("testy_scalenie_u01_stopka: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
