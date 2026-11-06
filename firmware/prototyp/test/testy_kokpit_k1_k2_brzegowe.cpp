// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (kokpit K1 i K2, słowa `rośnie`/`spada`, R-KOK-16).
// Testy „trzyma” sprawdzają przypadki brzegowe, których testy podstawowe nie mają.
// Uruchomienie: tools/prototyp/testy.sh (program testy_kokpit_k1_k2_brzegowe).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "kokpit.h"
#include "noc.h"
#include "stany.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define ROWNE(id, a, b)                                                                                          \
  do {                                                                                                           \
    std::string _a = (a), _b = (b);                                                                              \
    if (_a == _b) ok++;                                                                                          \
    else { zle++; printf("BŁĄD %s (%s:%d): %s = \"%s\", oczekiwane \"%s\"\n", id, __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)
#define ROZNE(id, a, b)                                                                                          \
  do {                                                                                                           \
    std::string _a = (a), _b = (b);                                                                              \
    if (_a != _b) ok++;                                                                                          \
    else { zle++; printf("BŁĄD %s (%s:%d): %s = \"%s\", nie powinno być \"%s\"\n", id, __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)

static const std::string DZIS = "2026-11-12";
static const char* const U_ASVAUTO = "ASVAuto/6/10/3/15";
static const char* const U_AUTOSET = "AutoSet/5/13//";

static Noc noc(const std::string& d, const char* ust) {
  Noc n;
  n.data = d;
  n.m01 = 420;
  n.wazna = true;
  n.ahi = 1.0f;
  n.cmed = 9.0f;
  n.cp95 = 10.5f;
  n.wmed = 4.0f;
  n.wp95 = 10.0f;
  n.emed = 7.0f;
  n.ep95 = 8.0f;
  n.psmed = 5.0f;
  n.psp95 = 7.0f;
  n.ustawienia = ust;
  n.zrodlo = 'A';
  return n;
}
static std::vector<Noc> okno(const char* ust) {
  std::vector<Noc> h;
  for (int i = 0; i < 30; i++) h.push_back(noc(dodajDni(DZIS, i - 29), ust));
  return h;
}
static NormaMetryki nmJest(double med, double p10, double p90) {
  NormaMetryki m;
  m.n = 30;
  m.med = med;
  m.p10 = p10;
  m.p90 = p90;
  return m;
}
static Norma normaPelna() {
  Norma nm;
  nm.zegar = true;
  nm.n = 30;
  nm.m01 = nmJest(420, 380, 460);
  nm.ahi = nmJest(1.0, 0.5, 2.0);
  nm.cmed = nmJest(9.0, 8.5, 9.5);
  nm.cp95 = nmJest(10.5, 10.0, 11.0);
  nm.wp95 = nmJest(10, 8, 12);
  nm.emed = nmJest(7.0, 6.5, 7.5);
  nm.ep95 = nmJest(8.0, 7.5, 8.5);
  nm.psmed = nmJest(5.0, 4.5, 5.5);
  nm.psp95 = nmJest(7.0, 6.5, 7.5);
  return nm;
}
static std::string slowo(const std::vector<Noc>& h, const std::string& nazwa) {
  DaneK1 d = budujK1(h, DZIS, normaPelna(), {});
  for (int i = 0; i < d.wierszy; i++)
    if (nazwa == d.w[i].nazwa) return d.w[i].slowo;
  return "(brak wiersza)";
}

// wiersz `Wsparcie` (M-28) z normą pokazuje `stabilnie` bez żadnego porównania, także gdy PS w ostatnich
// 7 nocach jest dwa razy wyższe. Słowo `stabilnie` twierdzi coś o danych, czego panel nie sprawdził (ADR-0002: słowa
// opisują dane); reguła R-KOK-16 nie obejmuje metryki bez Δ. Oczekiwane: słowo neutralne (nie `stabilnie`),
// do decyzji w otwartych pytaniach.
static void wsparcieStabilnie() {
  std::vector<Noc> h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].psmed = 10.0f;
  ROZNE("wiersz `Wsparcie`", slowo(h, "Wsparcie"), "stabilnie");
  // rampa PS 5,0 → 7,9 (jak testAsv w test/testy.cpp): EPAP o tej samej rampie `rośnie`, PS `stabilnie`
  h = okno(U_ASVAUTO);
  for (int i = 0; i < 30; i++) {
    h[i].psmed = 5.0f + 0.1f * i;
    h[i].emed = 7.0f + 0.1f * i;
  }
  ROWNE("wiersz `Wsparcie`-kontrola", slowo(h, "EPAP"), "rośnie");
  ROZNE("wiersz `Wsparcie`", slowo(h, "Wsparcie"), "stabilnie");
}

// noce z AirBridge bez żadnego wcześniejszego odczytu trybu (`MOP`) to według fail-path R-DANE-29 tryb
// nieznany, „noc jak w trybie spoza wydania”, czyli poza oknami reguł. Trend R-KOK-16 wyłącza tylko T_OBCY, więc
// takie noce liczą się do okna 23 i dają `rośnie`. Oczekiwane: `uczę się` (0 nocy ze znanym trybem w oknie 23).
static void trybNieznanyNaPoczatku() {
  std::vector<Noc> h = okno(U_ASVAUTO);
  for (int i = 0; i < 23; i++) {
    h[i].ustawienia = "";
    h[i].wazna = false;  // tak zapisuje ją airbridge.cpp (tryb spoza AB_TRYB)
  }
  for (int i = 23; i < 30; i++) h[i].ahi = 2.5f;
  ROWNE("noce z AirBridge bez żadnego wcześniejszego odczytu trybu", slowo(h, "AHI"), "uczę się");
}

// ---------- trzyma: przypadki brzegowe spoza testów ----------
static void trzyma() {
  // granica ½ Δ na wartościach niedokładnych w float (1,1 → 2,6; 9,1 → 10,1)
  std::vector<Noc> h = okno(U_ASVAUTO);
  for (int i = 0; i < 30; i++) h[i].ahi = i < 23 ? 1.1f : 2.6f;
  ROWNE("T1", slowo(h, "AHI"), "rośnie");
  h = okno(U_AUTOSET);
  for (int i = 0; i < 30; i++) h[i].cmed = i < 23 ? 9.1f : 10.1f;
  ROWNE("T1", slowo(h, "Ciśnienie"), "rośnie");
  for (int i = 0; i < 30; i++) h[i].cmed = i < 23 ? 10.1f : 9.1f;
  ROWNE("T1", slowo(h, "Ciśnienie"), "spada");
  // mediana parzystej liczby nocy w oknie 7 (4 noce): (2,0 + 3,0) / 2 = 2,5 → różnica 1,5 → `rośnie`
  h = okno(U_ASVAUTO);
  h.erase(h.begin() + 23, h.begin() + 26);  // zostają 26…29
  h[h.size() - 4].ahi = 2.0f;
  h[h.size() - 3].ahi = 2.0f;
  h[h.size() - 2].ahi = 3.0f;
  h[h.size() - 1].ahi = 3.0f;
  ROWNE("T2", slowo(h, "AHI"), "rośnie");
  h.back().ahi = 2.9f;  // (2,0 + 2,9) / 2 = 2,45 → `stabilnie`
  ROWNE("T2", slowo(h, "AHI"), "stabilnie");
  // noce VAuto w oknie 23: 10 z 23 → 13 → `uczę się`, także w wierszu niezależnym od trybu (wyciek)
  h = okno(U_ASVAUTO);
  for (int i = 0; i < 10; i++) h[i].ustawienia = "VAuto/4/20//";
  for (int i = 23; i < 30; i++) h[i].wp95 = 20.0f;
  ROWNE("T3", slowo(h, "Wyciek"), "uczę się");
  h[9].ustawienia = U_ASVAUTO;  // 14
  ROWNE("T3", slowo(h, "Wyciek"), "rośnie");
  // noc z pustymi ustawieniami po nocach VAuto ma tryb VAuto (przeniesienie, R-NOR-07) i też wypada
  h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].ahi = 2.5f;
  h[25].ustawienia = "VAuto/4/20//";
  for (int i = 26; i < 30; i++) h[i].ustawienia = "";
  ROWNE("T4", slowo(h, "AHI"), "uczę się");  // 1 noc ASVAuto (23, 24 → 2) w oknie 7
  // AutoSet po ASVAuto w oknie 23: ciśnienie z 7 nocy AutoSet, w oknie 23 tylko 16 nocy AutoSet → słowo trendu
  h = okno(U_AUTOSET);
  for (int i = 0; i < 7; i++) h[i].ustawienia = U_ASVAUTO;
  for (int i = 0; i < 7; i++) h[i].cmed = 15.0f;
  for (int i = 23; i < 30; i++) h[i].cmed = 10.0f;
  ROWNE("T5", slowo(h, "Ciśnienie"), "rośnie");  // 9,0 → 10,0 z nocy AutoSet; IPAP ASVAuto 15 nie wchodzi
  for (int i = 7; i < 10; i++) h[i].ustawienia = U_ASVAUTO;  // 13 nocy AutoSet w oknie 23
  ROWNE("T5", slowo(h, "Ciśnienie"), "uczę się");
}

int main() {
  wsparcieStabilnie();
  trybNieznanyNaPoczatku();
  trzyma();
  printf("testy_kokpit_k1_k2_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
