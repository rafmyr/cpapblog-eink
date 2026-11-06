// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (profil publiczny: `profil=publiczny|prywatny` w /konfiguracja.txt, teksty 5.4.14
// na ekranach dnia, R-EKR-43, R-EKR-51, propozycje). Każdy test pada z numerem R18-nn, dopóki usterka
// nie zostanie poprawiona. Testy „trzyma” (SPRAWDZ) potwierdzają
// miejsca, w których moduł działa. Dane wyłącznie syntetyczne. Blokują bramkę w testy.sh.
// AHI i PM2.5 zostają w profilu publicznym (5.4.14). Profil publiczny nie zmienia tekstów
// ekranów dnia, więc znaleziska sprawdzają teraz, że AHI stoi wszędzie jak w profilu prywatnym,
// a wywołania z flagą profilu (której funkcje tekstów już nie mają) zastąpiło jedno wywołanie.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "dzien.h"
#include "ekrany.h"
#include "kolejka.h"
#include "noc.h"
#include "raport.h"
#include "reguly.h"
#include "slot_dzienny.h"
#include "sprawy.h"
#include "sprzet.h"
#include "stany.h"
#include "wspolne.h"
#include "zdarzenia_ekrany_e.h"

static int ok = 0, zle = 0, trzyma = 0;
#define PRZYPADEK(nr, w)                                                              \
  do {                                                                                 \
    if (w) ok++;                                                                       \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); } \
  } while (0)
#define SPRAWDZ(w)                                                             \
  do {                                                                         \
    if (w) trzyma++;                                                           \
    else { zle++; printf("BŁĄD (trzyma?) %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

static bool ma(const std::string& s, const char* x) { return s.find(x) != std::string::npos; }

static Noc noc(const std::string& d, double ahi, int m01 = 420, double wp95 = 8) {
  Noc n;
  n.data = d;
  n.wazna = m01 >= 240;
  n.m01 = m01;
  n.ahi = (float)ahi;
  n.wp95 = (float)wp95;
  n.wmed = 4;
  n.ustawienia = "ASVAuto/6/10/3/15";
  return n;
}

// raport dla lekarza w profilu publicznym dostaje teksty publiczne. Dyrygent daje raportowi
// `we.sprawy = &b5.sprawy`, a te powstają z `sprawyOtwarte(…, profilB18())`; strefa 2 raportu ogólnego bierze
// `s.powod` spraw „tylko raport” (L-12 `{metryka} {x}, przed zmianą …`), temat raportu `t->tytul`. R-EKR-43 i 5.4.14:
// „K1, K2 i raport dla lekarza zostają bez zmian, z AHI”. Spec techniczna twierdzi „raport … bierze tekst zwykły”.
static void testRaport() {
  reguly::Wynik w;
  w.id = "L-12";
  w.oceniona = w.spelniona = true;
  w.poziom = "lekarz";
  w.grupa = "R3";
  w.stan = reguly::StanReguly::TYLKO_RAPORT;
  w.zmienne = {{"metryka", "AHI"}, {"x", "6,2"}, {"y", "2,1"}, {"data", "12.10"}};
  reguly::Pamiec pm;
  std::vector<Noc> h;
  for (int i = 0; i < 40; i++) h.push_back(noc(dodajDni("2026-11-05", i - 39), 2.0));
  {
    std::vector<sprawy::SprawaL> s = sprawy::sprawyOtwarte(b14::zestaw(), {w}, pm, b14::funkcjaTekstu());
    SPRAWDZ(s.size() == 1);
    raport::Wejscie we;
    we.dzis = "2026-11-05";
    we.historia = &h;
    we.sprawy = &s;
    we.zestaw = &b14::zestaw();
    we.tekst = b14::funkcjaTekstu();
    raport::Raport r = raport::zbuduj(we);
    std::string s2;
    for (auto& z : r.s2) s2 += z + "\n";
    PRZYPADEK("raport dla lekarza w profilu publicznym dostaje teksty", ma(s2, "AHI 6,2, przed zmianą 2,1") && !ma(s2, "Bezdechy"));
    if (!ma(s2, "AHI 6,2")) printf("    raport, strefa 2: %s", s2.c_str());
  }
}

// Po poprawce (wariant A, do potwierdzenia): `{metryka}` przed
// liczbą bez jednostki = `bezdechy / h`, więc oczekiwanie zmienione z `bezdechy 6,0` na `bezdechy / h 6,0`.
// linia R-NOR-21 ma szablon `Od {n} tyg.: {metryka} {x}. Liczby w raporcie.`, a 5.4.14 daje dla `{metryka}`
// M-02 `bezdechy` (małą literą). Kod wstawia nazwę karty sprawy `Bezdechy` z wielkiej litery w środku zdania.
static void testLiniaNor21() {
  std::vector<Noc> h;
  for (int i = 0; i < 40; i++) h.push_back(noc(dodajDni("2026-11-05", i - 39), 6.0));
  Stany st;
  st.ahi.aktywny = true;
  st.ahi.nocy = 21;
  st.ahi.od = dodajDni("2026-11-05", -20);
  st.norma.ahi.p10 = 0.8;
  st.norma.ahi.p90 = 2.1;
  {
    std::vector<sprawy::SprawaMetryki> m = sprawy::sprawyMetryk(h, st, "2026-11-05");
    std::vector<sprawy::SprawaL> brak;
    kolejka::WejscieKolejki wk;
    wk.dzis = "2026-11-05";
    wk.sprawy = &brak;
    wk.metryki = &m;
    std::string l;
    for (auto& x : kolejka::kolejkaLinii(wk))
      if (x.zrodlo.rfind("R-NOR-21", 0) == 0) l = x.tekst;
    PRZYPADEK("linia R-NOR-21 ma szablon `Od {n} tyg", l == "Od 3 tyg.: AHI 6,0. Liczby w raporcie.");  // po decyzji: AHI w obu profilach
    if (l != "Od 3 tyg.: AHI 6,0. Liczby w raporcie.") printf("    R-NOR-21: „%s”\n", l.c_str());
  }
}

// AHI brak (`–`, R-DANE-07) w profilu publicznym daje `bezdechy – / h` (stopka slotu, pytanie „Co było
// inaczej?”), czyli jednostkę przy kresce; prywatnie `AHI –`.
static void testBrakAhi() {
  Noc n = noc("2026-11-05", NAN, 400);
  StopkaDzienna s = stopkaDzienna(&n, true, 0, NAN, NAN);
  PRZYPADEK("AHI brak", !ma(s.lewo, "– / h") && ma(s.lewo, "AHI –"));
  if (ma(s.lewo, "– / h")) printf("    stopka: „%s”\n", s.lewo.c_str());
  Norma nm;
  std::string p = opisNocyPytania(n, nm, false);
  PRZYPADEK("AHI brak", !ma(p, "– / h"));
  if (ma(p, "– / h")) printf("    pytanie: „%s”\n", p.c_str());
}

// Trzyma: limity i szerokości przy najdłuższych wartościach w profilu publicznym.
static void testDlugosci() {
  // blok liczb S2a/S2c: AHI od x = max(328, koniec czasu + 56); najdłuższy czas `12 h 59` (SN96) + podpis P24B
  int x1 = 28 + szer(F_SN96, "12 h 59");
  int x2 = std::max(328, x1 + 56);
  SPRAWDZ(x2 + szer(F_P24B, "AHI") <= 772);
  SPRAWDZ(x2 + szer(F_SN96, "99,9") <= 772);
  // pytanie „Co było inaczej?”: dwie metryki, najdłuższe wartości, najwyżej 2 linie po 744 px
  Norma nm;
  nm.ahi.p10 = 10.1f;
  nm.ahi.p90 = 12.0f;
  nm.ahi.n = 30;
  nm.ahi.med = 11;
  nm.m01.p10 = 100;
  nm.m01.p90 = 120;
  nm.m01.n = 30;
  nm.m01.med = 110;
  Noc n = noc("2026-11-05", 99.9, 779, 120);
  std::string p = opisNocyPytania(n, nm, true);
  SPRAWDZ(zawin(F_P22B, p, 744).size() <= 2);
  // stopka slotu z datą, czasem 12 h 59, AHI 99,9, wyciek 120, pogoda i przy łóżku: lewa część nie przekracza 744 px
  Noc d = noc("2026-11-03", 99.9, 779, 120);
  StopkaDzienna s = stopkaDzienna(&d, false, 0, -25, 21);
  SPRAWDZ(szer(F_P22B, s.lewo) + (s.prawo.empty() ? 0 : 24 + szer(F_P22B, s.prawo)) <= 744);
  SPRAWDZ(ma(s.lewo, "AHI 99,9"));
  // odwrócone: E-30 z `AHI` w tytule, powodzie i podpisie osi w każdym profilu
  sprawy::SprawaL e = b14::sprawaE("E-30", true, reguly::Tryb::ASVAUTO);
  PRZYPADEK("testDlugosci", e.tytul == "Noce z AHI powyżej 5" && ma(e.powod, "AHI") && e.podpisLewo == "AHI · 30 nocy");
}

// Trzyma: przełącznik w pliku konfiguracji.
static void testKlucz() {
  SPRAWDZ(!parsujUstawieniaDnia("profil=PUBLICZNY\n").profilPubliczny);  // zła wartość: prywatny + wiersz w logu
  SPRAWDZ(parsujUstawieniaDnia("profil=PUBLICZNY\n").bledy.size() == 1);
  SPRAWDZ(!parsujUstawieniaDnia("# profil=publiczny\n").profilPubliczny);
  SPRAWDZ(!parsujUstawieniaDnia("profil=publiczny\nprofil=prywatny\n").profilPubliczny);  // ostatni wygrywa
  // spacja na końcu wartości (ręczna edycja) i BOM na początku pliku (Notatnik) dają po cichu prywatny
  PRZYPADEK("testKlucz", parsujUstawieniaDnia("profil=publiczny \n").profilPubliczny);
  PRZYPADEK("testKlucz", parsujUstawieniaDnia("\xEF\xBB\xBFprofil=publiczny\n").profilPubliczny);
  // zapis wymiany sprzętu (R-MAS-01, ◀ 2 s w „Sprzęt”) nie gubi wiersza profilu
  std::string t = sprzet::zapiszWymiane("profil=publiczny\nco2=nie\n", EL_FILTR, "2026-11-05");
  SPRAWDZ(parsujUstawieniaDnia(t).profilPubliczny);
}

// Trzyma: profil prywatny (domyślny) bez zmian w tekstach katalogu: wariant `_publiczny` nie wygrywa bez flagi.
static void testPrywatny() {
  for (const char* kl : {"E-30", "E-31/obserwuj", "E-31/lekarz"}) {
    sprawy::SprawaL a = b14::sprawaE(kl, false, reguly::Tryb::ASVAUTO);
    sprawy::SprawaL b = b14::sprawaE(kl, false, reguly::Tryb::ASVAUTO);
    SPRAWDZ(a.tytul == b.tytul && a.powod == b.powod && a.podpisLewo == b.podpisLewo);
    SPRAWDZ(ma(a.podpisLewo, "AHI"));
  }
}

int main() {
  testRaport();
  testLiniaNor21();
  testBrakAhi();
  testDlugosci();
  testKlucz();
  testPrywatny();
  printf("testy_profil_publiczny_brzegowe: %d trzyma, %d poprawionych, %d błędów\n", trzyma, ok, zle);
  return zle ? 1 : 0;
}
