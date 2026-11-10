// SPDX-License-Identifier: GPL-3.0-or-later
// U-01 po połączeniu listy ustawień do odczytu (R-KONF-55) z wierszem stanu oprogramowania (R-KONF-43, R-KONF-46,
// R-NF-33): dyrygent na atrapach rysuje na U-01 właściwy wiersz aktualizacji we wszystkich trzech stanach (token
// i nowsza wersja, token bez nowszej, bez tokenu), wiersz mieści się w 744 px także przy długiej własnej wersji,
// a tekst wiersza nie odsyła do elementu, którego na U-01 nie ma. Dane syntetyczne.
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

// pas pikseli y0…y1 bufora 1 bit (100 bajtów na wiersz)
static bool pasRowny(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int y0, int y1) {
  for (int y = y0; y < y1; y++)
    for (int x = 0; x < 100; x++)
      if (a[(size_t)(y * 100 + x)] != b[(size_t)(y * 100 + x)]) return false;
  return true;
}

// ---------- dyrygent: wiersz aktualizacji pod listą U-01 we wszystkich trzech stanach ----------
// wymaganie: R-KONF-55, R-KONF-43, R-NF-33
static void testDyrygentWierszAktualizacjiU01() {
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
    std::string oczek = aktualizacja::wierszU01(c.token, c.znana, wlasna);
    // wzorzec: sam wiersz w miejscu z rysujUstawienia() (P22B, x 28, linia bazowa 338) na białym tle
    PlotnoAtrapy wz;
    tekst(wz, F_P22B, 28, 338, oczek);
    bool rowny = pasRowny(klatka, wz.b, 318, 345);
    if (!rowny) printf("  U-01 z dyrygenta: pas wiersza aktualizacji różny od `%s`\n", oczek.c_str());
    SPRAWDZ(rowny);
    // token nigdy w klatce jako tekst: wiersz ze wzorca nie zawiera znacznika tokenu
    SPRAWDZ(oczek.find("ZNACZNIKTOKENU") == std::string::npos && oczek.find("github_pat") == std::string::npos);
  }
}

// ---------- szerokość wiersza aktualizacji przy długich wersjach (P22B, 744 px) ----------
// wymaganie: R-KONF-55
static void testSzerokoscWierszaPrzyDlugiejWersji() {
  // własna wersja długa (wydanie kandydujące), bez nowszej: `Oprogramowanie {wersja} · ↻ 2 s Sprawdź aktualizację`
  std::string t = aktualizacja::wierszU01(true, "", "1.12.3-rc.12");
  if (szer(F_P22B, t) > 744) printf("  %d px: %s\n", szer(F_P22B, t), t.c_str());
  SPRAWDZ(szer(F_P22B, t) <= 744);
  // obie wersje długie: `Jest wersja {X} · ↻ 2 s Aktualizacja`
  t = aktualizacja::wierszU01(true, "12.12.12-rc.12", "1.12.3-rc.12");
  SPRAWDZ(szer(F_P22B, t) <= 744);
}

// ---------- tekst wiersza nie odsyła do pozycji, której na U-01 nie ma ----------
// wymaganie: R-KONF-55, R-NF-33
static void testOdeslanieDoIstniejacegoElementu() {
  // bez tokenu wiersz mówi `Ustaw je w „Więcej na telefonie”.`; ta nazwa musi stać na U-01 (podpowiedź pod listą),
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
  for (const char* x : {U01_PODPOWIEDZ[0], U01_PODPOWIEDZ[1], U01_GRANICA}) jest = jest || std::string(x).find(cel) != std::string::npos;
  for (auto& s : stopkaU01()) jest = jest || s.find(cel) != std::string::npos;
  if (!jest) printf("  U-01: `%s` odsyła do „%s”, którego na U-01 ani na formularzu już nie ma\n", t.c_str(), cel.c_str());
  SPRAWDZ(jest);
}

int main() {
  testDyrygentWierszAktualizacjiU01();
  testSzerokoscWierszaPrzyDlugiejWersji();
  testOdeslanieDoIstniejacegoElementu();
  printf("testy_scalenie_u01_stopka: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
