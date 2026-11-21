// SPDX-License-Identifier: GPL-3.0-or-later
// Testy normy tymczasowej „sprzed zmiany” (ADR-0048, decyzja właściciela projektu 21.11.2026): liczenie normy
// tymczasowej w liczNorme() (src/noc.cpp, R-NOR-27), K1 i K2 z normą tymczasową (src/kokpit.cpp, src/ekrany.cpp,
// R-NOR-28) i niezmienność reguł, stanów i kart (R-NOR-29). Dane wyłącznie syntetyczne, także zakresy ustawień
// w scenariuszu częstych zmian ustawień.
// Uruchomienie: tools/prototyp/testy.sh (program testy_norma_tymczasowa).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "ekrany.h"
#include "kokpit.h"
#include "noc.h"
#include "odkrycia.h"
#include "podsumowanie.h"
#include "reguly.h"
#include "stany.h"
#include "wspolne.h"
#include "wzorce.h"

namespace reguly {
void zarejestrujRodzajeE();  // src/rodzaje_e.cpp (jak w src/dyrygent.cpp)
}

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                               \
  do {                                                                           \
    if (w) ok++;                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }         \
  } while (0)
#define ROWNE(a, b)                                                                                              \
  do {                                                                                                           \
    std::string _a = (a), _b = (b);                                                                              \
    if (_a == _b) ok++;                                                                                          \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)

static const std::string D0 = "2026-10-01";
static std::string d(int i) { return dodajDni(D0, i); }

// Noc AutoSet: wyciek i ciśnienie zależą od „epoki” ustawień, żeby norma tymczasowa i właściwa były rozróżnialne.
static Noc nocAutoSet(int i, const std::string& ust, float wyciekBaza, float cisnBaza) {
  Noc n;
  n.data = d(i);
  n.wazna = true;
  n.m01 = 400 + (i % 3) * 10;
  n.ahi = 1.0f + 0.1f * (float)(i % 5);
  n.wmed = wyciekBaza / 2;
  n.wp95 = wyciekBaza + (float)(i % 5);  // rozrzut 0…4
  n.cmed = cisnBaza;
  n.cp95 = cisnBaza + 1.0f + 0.1f * (float)(i % 5);
  n.oa = 0.3f;
  n.ca = 0.2f;
  n.h = 0.5f;
  n.m17 = 0;
  n.m13 = 15;
  n.ustawienia = ust;
  n.tryb = "AutoSet";
  return n;
}

// Scenariusz częstych zmian ustawień (wartości syntetyczne): `6/12/2` × 3, `6/12.4/2` × 2, `6/11.4/2` × 4, `6.4/12.4/2` × 14, potem `6/12/2` × ile.
// Noce sprzed zmiany mają wyciek ok. 10 L/min, noce z ustawieniami ostatniej nocy ok. 20 L/min.
static std::vector<Noc> scenariusz(int ostatnichNowych) {
  std::vector<Noc> h;
  int i = 0;
  auto dodaj = [&](const char* u, int ile, float wyc, float cis) {
    for (int k = 0; k < ile; k++) h.push_back(nocAutoSet(i++, std::string("AutoSet/") + u, wyc, cis));
  };
  dodaj("6/12/2", 3, 20, 9);
  dodaj("6/12.4/2", 2, 10, 10);
  dodaj("6/11.4/2", 4, 10, 10);
  dodaj("6.4/12.4/2", 14, 10, 10);
  dodaj("6/12/2", ostatnichNowych, 20, 9);
  return h;
}

static const std::set<std::string> PUSTE;
// norma z normą tymczasową, jak końcowa Stany::norma (domyślnie liczNorme() jej nie liczy)
static Norma lnt(const std::vector<Noc>& h, const std::string& dzis, const std::set<std::string>& wyk,
                 const WykluczeniaNormy* wm = nullptr) {
  return liczNorme(h, dzis, wyk, wm, ZakresNormy::Z_TYMCZASOWA);
}

// ---------------------------------------------------------------- R-NOR-27: liczenie
static void testR_NOR_27_czeste_zmiany_ustawien() {
  // 11 nocy `6/12/2` po zmianie: noc oceniana to ostatnia (dzis), do normy 3 + 10 = 13 nocy z ustawieniami ostatniej nocy
  std::vector<Noc> h = scenariusz(11);
  std::string dzis = h.back().data;
  Norma nm = lnt(h, dzis, PUSTE);
  ROWNE(nm.ustawienia, "AutoSet/6/12/2");
  SPRAWDZ(!nm.wp95.jest());
  SPRAWDZ(nm.wp95.n == 13);
  SPRAWDZ(nm.sprzedZmiany.wp95.pokaz());
  SPRAWDZ(nm.sprzedZmiany.wp95.nocy == 20);       // 2 + 4 + 14 nocy z innymi ustawieniami
  SPRAWDZ(nm.sprzedZmiany.wp95.nocyNowych == 13);  // ile już zebrane (np. do wariantu „13 z 14”)
  // pasmo z 20 nocy sprzed zmiany (wyciek 10 + i % 5): mediana 12, P10 10, P90 14
  SPRAWDZ(std::fabs(nm.sprzedZmiany.wp95.med - 12) < 1e-9);
  SPRAWDZ(std::fabs(nm.sprzedZmiany.wp95.p10 - 10) < 1e-9);
  SPRAWDZ(std::fabs(nm.sprzedZmiany.wp95.p90 - 14) < 1e-9);
  // te same zasady dla ciśnienia (M-05, M-05p); EPAP i PS bez wartości w AutoSet: brak normy tymczasowej
  SPRAWDZ(nm.sprzedZmiany.cmed.pokaz() && std::fabs(nm.sprzedZmiany.cmed.med - 10) < 1e-9);
  SPRAWDZ(nm.sprzedZmiany.cp95.pokaz());
  SPRAWDZ(!nm.sprzedZmiany.emed.pokaz() && !nm.sprzedZmiany.psmed.pokaz());
  // metryki niezależne od ustawień bez zmian: norma właściwa z wszystkich nocy
  SPRAWDZ(nm.ahi.jest() && nm.m01.jest());
  // M-13 w AutoSet bez filtra ustawień (do sprawdzenia): norma właściwa jest (norma tymczasowa M-13 nie jest liczona)
  SPRAWDZ(nm.m13.jest());
  // domyślnie (normy pośrednie, reguły) bez normy tymczasowej
  Norma bez = liczNorme(h, dzis, PUSTE);
  SPRAWDZ(!bez.sprzedZmiany.wp95.pokaz() && bez.sprzedZmiany.wp95.nocy == 0 && !bez.sprzedZmiany.cmed.pokaz());
  SPRAWDZ(bez.wp95.n == nm.wp95.n && bez.cmed.n == nm.cmed.n);

  // kolejna noc `6/12/2`: 14 nocy z ustawieniami ostatniej nocy → norma właściwa, tymczasowa znika
  std::vector<Noc> h2 = scenariusz(12);
  Norma n2 = lnt(h2, h2.back().data, PUSTE);
  SPRAWDZ(n2.wp95.jest() && n2.wp95.n == 14);
  SPRAWDZ(!n2.sprzedZmiany.wp95.pokaz());
  SPRAWDZ(std::isnan(n2.sprzedZmiany.wp95.med));  // bez liczb, gdy nie do pokazania
  SPRAWDZ(n2.cmed.jest() && !n2.sprzedZmiany.cmed.pokaz());
}

static void testR_NOR_27_norma_wlasciwa_bez_zmian() {
  // Norma właściwa liczona tak jak przed ADR-0048: noce z innymi ustawieniami do niej nie wchodzą
  std::vector<Noc> h = scenariusz(11);
  Norma nm = lnt(h, h.back().data, PUSTE);
  SPRAWDZ(nm.wp95.n == 13 && std::isnan(nm.wp95.med));
  SPRAWDZ(nm.cmed.n == 13 && nm.cp95.n == 13);
  std::vector<Noc> h2 = scenariusz(12);
  Norma n2 = lnt(h2, h2.back().data, PUSTE);
  // 14 nocy z wyciekiem 20 + i % 5: wartości wyłącznie z nocy `6/12/2`, pasmo ≥ 20
  SPRAWDZ(n2.wp95.p10 >= 20 - 1e-9 && n2.wp95.p90 <= 24 + 1e-9);
}

static void testR_NOR_27_ponizej_14_nocy_sprzed_zmiany() {
  // fail-path: 13 nocy sprzed zmiany, 5 po → brak normy tymczasowej (K1, K2 `uczę się`)
  std::vector<Noc> h;
  for (int i = 0; i < 13; i++) h.push_back(nocAutoSet(i, "AutoSet/6/11/2", 10, 9));
  for (int i = 13; i < 18; i++) h.push_back(nocAutoSet(i, "AutoSet/6/12/2", 20, 9));
  Norma nm = lnt(h, h.back().data, PUSTE);
  SPRAWDZ(!nm.wp95.jest());
  SPRAWDZ(nm.sprzedZmiany.wp95.nocy == 13);
  SPRAWDZ(!nm.sprzedZmiany.wp95.pokaz());
  // jedna noc więcej sprzed zmiany: jest
  h.insert(h.begin(), nocAutoSet(-1, "AutoSet/6/11/2", 10, 9));
  nm = lnt(h, h.back().data, PUSTE);
  SPRAWDZ(nm.sprzedZmiany.wp95.nocy == 14 && nm.sprzedZmiany.wp95.pokaz());
}

static void testR_NOR_27_te_same_wylaczenia() {
  // noce sprzed zmiany z czynnikiem (R-CZY-05), nieważne i z EPAP ze STR (R-NOR-08) poza normą tymczasową
  std::vector<Noc> h = scenariusz(11);
  std::set<std::string> wykl = {d(5), d(6)};  // dwie noce `6/11.4/2`
  h[7].wazna = false;
  Norma nm = lnt(h, h.back().data, wykl);
  SPRAWDZ(nm.sprzedZmiany.wp95.nocy == 17);
  SPRAWDZ(nm.sprzedZmiany.wp95.pokaz());
  // stan odchylenia wycieku (R-NOR-09) przez WykluczeniaNormy
  WykluczeniaNormy wm;
  for (int i = 3; i < 10; i++) wm.wp95.insert(d(i));
  nm = lnt(h, h.back().data, wykl, &wm);
  SPRAWDZ(nm.sprzedZmiany.wp95.nocy == 13);  // 17 − 4: noce 3, 4, 8, 9 (5, 6, 7 wyłączone już wcześniej)
  SPRAWDZ(!nm.sprzedZmiany.wp95.pokaz());
  // limit 30 najnowszych: 40 nocy sprzed zmiany → 30
  std::vector<Noc> dl;
  for (int i = 0; i < 40; i++) dl.push_back(nocAutoSet(i, "AutoSet/6/11/2", 10, 9));
  for (int i = 40; i < 45; i++) dl.push_back(nocAutoSet(i, "AutoSet/6/12/2", 20, 9));
  nm = lnt(dl, dl.back().data, PUSTE);
  SPRAWDZ(nm.sprzedZmiany.wp95.nocy == 30);
  // okno 90 dni: noce sprzed zmiany starsze niż 90 dni poza normą tymczasową
  std::vector<Noc> st;
  for (int i = 0; i < 14; i++) st.push_back(nocAutoSet(i, "AutoSet/6/11/2", 10, 9));  // wszystkie przed d(14)
  for (int i = 100; i < 105; i++) st.push_back(nocAutoSet(i, "AutoSet/6/12/2", 20, 9));
  nm = lnt(st, st.back().data, PUSTE);
  SPRAWDZ(nm.sprzedZmiany.wp95.nocy == 0 && !nm.sprzedZmiany.wp95.pokaz());
}

static void testR_NOR_27_bez_odczytu_ustawien() {
  // R-NOR-15: noc bez odczytu ustawień ma ustawienia ostatniej znanej; noce sprzed pierwszych znanych ustawień
  // (puste, bez przeniesienia) nie wchodzą do normy tymczasowej
  std::vector<Noc> h;
  for (int i = 0; i < 16; i++) h.push_back(nocAutoSet(i, "", 10, 9));  // nieznane
  for (int i = 16; i < 20; i++) h.push_back(nocAutoSet(i, "AutoSet/6/12/2", 20, 9));
  Norma nm = lnt(h, h.back().data, PUSTE);
  SPRAWDZ(nm.sprzedZmiany.wp95.nocy == 0 && !nm.sprzedZmiany.wp95.pokaz());
  // znane sprzed zmiany, potem noce bez odczytu (przeniesienie `6/11/2`), potem nowa zmiana
  std::vector<Noc> g;
  for (int i = 0; i < 10; i++) g.push_back(nocAutoSet(i, "AutoSet/6/11/2", 10, 9));
  for (int i = 10; i < 16; i++) g.push_back(nocAutoSet(i, "", 10, 9));
  for (int i = 16; i < 20; i++) g.push_back(nocAutoSet(i, "AutoSet/6/12/2", 20, 9));
  nm = lnt(g, g.back().data, PUSTE);
  SPRAWDZ(nm.sprzedZmiany.wp95.nocy == 16 && nm.sprzedZmiany.wp95.pokaz());
  // noc bez odczytu po nowych ustawieniach liczy się jako nowa (R-NOR-15), nie sprzed zmiany
  g.push_back(nocAutoSet(20, "", 20, 9));
  g.push_back(nocAutoSet(21, "", 20, 9));
  nm = lnt(g, g.back().data, PUSTE);
  SPRAWDZ(nm.wp95.n == 5 && nm.sprzedZmiany.wp95.nocy == 16);
}

static void testR_NOR_27_bez_znanych_ustawien() {
  // bez żadnych znanych ustawień: bez filtra R-NOR-07, więc i bez normy tymczasowej
  std::vector<Noc> h;
  for (int i = 0; i < 20; i++) h.push_back(nocAutoSet(i, "", 10, 9));
  Norma nm = lnt(h, h.back().data, PUSTE);
  SPRAWDZ(nm.wp95.jest());
  SPRAWDZ(!nm.sprzedZmiany.wp95.pokaz() && nm.sprzedZmiany.wp95.nocy == 0);
}

static void testR_NOR_27_asv_oddech() {
  // ASVAuto: M-13, M-14, M-15 i PS zależne od ustawień (R-NOR-07 od 11.11.2026), norma tymczasowa także dla nich
  std::vector<Noc> h;
  for (int i = 0; i < 20; i++) {
    Noc n = nocAutoSet(i, "ASVAuto/6/10/3/15", 10, 9);
    n.tryb = "ASVAuto";
    n.emed = 7;
    n.ep95 = 8;
    n.psmed = 5.0f + 0.1f * (float)(i % 5);
    n.m13 = 14;
    n.m14 = 470;
    n.m15 = 7;
    h.push_back(n);
  }
  for (int i = 20; i < 25; i++) {
    Noc n = h[0];
    n.data = d(i);
    n.ustawienia = "ASVAuto/6/10/4/16";
    h.push_back(n);
  }
  Norma nm = lnt(h, h.back().data, PUSTE);
  SPRAWDZ(!nm.psmed.jest() && nm.sprzedZmiany.psmed.pokaz());
  SPRAWDZ(!nm.emed.jest() && nm.sprzedZmiany.emed.pokaz() && nm.sprzedZmiany.ep95.pokaz());
  SPRAWDZ(!nm.m13.jest());  // M-13 w ASVAuto zależna od ustawień, ale bez kolumny normy na K1 i K2
}

// ADR-0048 pkt 1: norma tymczasowa tylko z nocy w trybie najnowszej nocy
static Noc wTrybie(Noc n, const char* tryb, const std::string& ust) {
  n.tryb = tryb;
  n.ustawienia = ust;
  if (std::string(tryb).rfind("ASV", 0) == 0) {
    n.emed = 7;
    n.ep95 = 8;
    n.psmed = 5;
  }
  return n;
}
static void testR_NOR_27_zmiana_trybu() {
  // CPAP → AutoSet (ten sam aparat): noce CPAP nie są „sprzed zmiany”, K1 i K2 `uczę się`
  std::vector<Noc> h;
  for (int i = 0; i < 25; i++) h.push_back(wTrybie(nocAutoSet(i, "", 10, 10), "CPAP", "CPAP/10"));
  for (int i = 25; i < 30; i++) h.push_back(nocAutoSet(i, "AutoSet/6/12/2", 12, 8));
  Norma nm = lnt(h, h.back().data, PUSTE);
  SPRAWDZ(!nm.wp95.jest() && !nm.sprzedZmiany.wp95.pokaz() && nm.sprzedZmiany.wp95.nocy == 0);
  SPRAWDZ(!nm.sprzedZmiany.cmed.pokaz() && !nm.sprzedZmiany.cp95.pokaz());
  {
    Stany st = liczStany(h, h.back().data, PUSTE);
    DaneK1 k1 = budujK1(h, h.back().data, st.norma, {}, &st);
    DaneK2 k2 = budujK2(h, h.back().data, st.norma, {}, &st);
    for (int i = 0; i < k1.wierszy; i++) SPRAWDZ(!k1.w[i].sprzedZmiany);
    for (int i = 0; i < k2.wierszy; i++) SPRAWDZ(!k2.w[i].sprzedZmiany);
    ROWNE(k2.w[6].norma, "uczę się");  // `Wyciek l/min`
  }
  // AutoSet → ASVAuto (wymiana aparatu z innym trybem; R-NOR-25 nie jest w liczeniu normy): bez normy tymczasowej
  std::vector<Noc> g;
  for (int i = 0; i < 25; i++) {
    Noc n = nocAutoSet(i, "AutoSet/6/12/2", 10, 10);
    n.pna = "aparat A";
    n.srn = "A1";
    g.push_back(n);
  }
  for (int i = 25; i < 30; i++) {
    Noc n = wTrybie(nocAutoSet(i, "", 30, 10), "ASVAuto", "ASVAuto/6/10/3/15");
    n.pna = "aparat B";
    n.srn = "B2";
    g.push_back(n);
  }
  nm = lnt(g, g.back().data, PUSTE);
  SPRAWDZ(!nm.sprzedZmiany.wp95.pokaz() && nm.sprzedZmiany.wp95.nocy == 0 && !nm.sprzedZmiany.psmed.pokaz());
  // nowy aparat w tym samym trybie z innymi ustawieniami: jak zmiana ustawień, norma tymczasowa ze starego aparatu
  // (R-NOR-25 nie zeruje normy w kodzie)
  std::vector<Noc> a;
  for (int i = 0; i < 25; i++) {
    Noc n = nocAutoSet(i, "AutoSet/6/11/2", 10, 10);
    n.srn = "A1";
    a.push_back(n);
  }
  for (int i = 25; i < 30; i++) {
    Noc n = nocAutoSet(i, "AutoSet/6/12/2", 20, 9);
    n.srn = "A2";
    a.push_back(n);
  }
  nm = lnt(a, a.back().data, PUSTE);
  SPRAWDZ(nm.sprzedZmiany.wp95.pokaz() && nm.sprzedZmiany.wp95.nocy == 25);
  // nowy aparat w tym samym trybie i z tymi samymi ustawieniami: nic się nie zmienia (norma właściwa ze wszystkich)
  for (auto& n : a) n.ustawienia = "AutoSet/6/12/2";
  nm = lnt(a, a.back().data, PUSTE);
  SPRAWDZ(nm.wp95.jest() && !nm.sprzedZmiany.wp95.pokaz());
  // noc z pustym polem `tryb`: tryb z jej ustawień, a bez nich z przeniesionych (R-NOR-15)
  std::vector<Noc> b;
  for (int i = 0; i < 20; i++) {
    Noc n = nocAutoSet(i, "AutoSet/6/11/2", 10, 10);
    n.tryb = "";
    b.push_back(n);
  }
  for (int i = 20; i < 25; i++) {
    Noc n = nocAutoSet(i, i == 20 ? "AutoSet/6/12/2" : "", 20, 9);
    n.tryb = "";
    b.push_back(n);
  }
  nm = lnt(b, b.back().data, PUSTE);
  SPRAWDZ(nm.sprzedZmiany.wp95.pokaz() && nm.sprzedZmiany.wp95.nocy == 20);
}

// K1 i K2 dostają normę tymczasową tylko z końcowej normy liczStany(); normy reguł (liczNorme() domyślnie) bez niej
static void testR_NOR_29_tylko_norma_koncowa() {
  std::vector<Noc> h = scenariusz(11);
  Stany st = liczStany(h, h.back().data, PUSTE);
  SPRAWDZ(st.normaKokpitu.sprzedZmiany.wp95.pokaz());
  Norma bez = liczNorme(h, h.back().data, PUSTE, &st.wyl);
  SPRAWDZ(!bez.sprzedZmiany.wp95.pokaz() && bez.sprzedZmiany.wp95.nocy == 0);
  SPRAWDZ(bez.wp95.n == st.norma.wp95.n && bez.cmed.n == st.norma.cmed.n);
}

static void testR_NOR_27_nowy_model_maski() {
  // R-MAS-16: dyrygent podaje historię normy bez wycieku nocy sprzed nowego modelu maski (histNormyB16), więc
  // norma tymczasowa wycieku z tych nocy nie powstaje, także przy jednoczesnej zmianie ustawień
  std::vector<Noc> h = scenariusz(11);
  for (auto& n : h)
    if (n.data < d(23)) n.wmed = n.wp95 = NAN;
  Norma nm = lnt(h, h.back().data, PUSTE);
  SPRAWDZ(!nm.wp95.jest() && !nm.sprzedZmiany.wp95.pokaz() && nm.sprzedZmiany.wp95.nocy == 0);
  SPRAWDZ(nm.sprzedZmiany.cmed.pokaz());  // ciśnienie: zwykła zmiana ustawień
}

// ---------------------------------------------------------------- R-NOR-28: K1 i K2
static const WierszK1* wierszK1(const DaneK1& d1, const char* n) {
  for (int i = 0; i < d1.wierszy; i++)
    if (std::strcmp(d1.w[i].nazwa, n) == 0) return &d1.w[i];
  return nullptr;
}
static const WierszK2* wierszK2(const DaneK2& d2, const char* e) {
  for (int i = 0; i < d2.wierszy; i++)
    if (std::strcmp(d2.w[i].etykieta, e) == 0) return &d2.w[i];
  return nullptr;
}

static void testR_NOR_28_k1_k2_dane() {
  std::vector<Noc> h = scenariusz(11);
  std::string dzis = h.back().data;
  const Stany& st = stanyZPamieci(h, dzis, PUSTE);
  DaneK1 k1 = budujK1(h, dzis, st.norma, {}, &st);
  const WierszK1* w = wierszK1(k1, "Wyciek");
  SPRAWDZ(w && w->sprzedZmiany);
  SPRAWDZ(w && w->slowo != "uczę się" && !w->slowo.empty());  // słowo trendu jak przy normie właściwej
  SPRAWDZ(w && !w->stan);
  SPRAWDZ(w && std::fabs(w->linia.medNormy - 12) < 1e-9);  // przerywana mediana normy tymczasowej
  SPRAWDZ(w && w->nocyNowych == 13 && w->progNocy == 14);
  const WierszK1* c = wierszK1(k1, "Ciśnienie");
  SPRAWDZ(c && c->sprzedZmiany && !c->stan);
  const WierszK1* a = wierszK1(k1, "AHI");
  SPRAWDZ(a && !a->sprzedZmiany);  // AHI niezależne od ustawień
  DaneK2 k2 = budujK2(h, dzis, st.norma, {}, &st);
  const WierszK2* x = wierszK2(k2, "Wyciek l/min");
  SPRAWDZ(x && x->sprzedZmiany);
  ROWNE(x ? x->norma : "", "10–14");
  SPRAWDZ(x && !x->stan);
  bool ramka = false;
  for (int i = 0; x && i < 7; i++) ramka = ramka || x->k[i].poza;
  SPRAWDZ(!ramka);  // bez ramek `poza twoją normą` (R-NOR-28)
  const WierszK2* p = wierszK2(k2, "Ciśnienie P95");
  SPRAWDZ(p && p->sprzedZmiany && p->norma != "uczę się");
  // po 14 nocach z nowymi ustawieniami: norma właściwa, bez znacznika
  std::vector<Noc> h2 = scenariusz(12);
  const Stany& s2 = stanyZPamieci(h2, h2.back().data, PUSTE);
  DaneK1 n1 = budujK1(h2, h2.back().data, s2.norma, {}, &s2);
  DaneK2 n2 = budujK2(h2, h2.back().data, s2.norma, {}, &s2);
  SPRAWDZ(wierszK1(n1, "Wyciek") && !wierszK1(n1, "Wyciek")->sprzedZmiany);
  SPRAWDZ(wierszK2(n2, "Wyciek l/min") && !wierszK2(n2, "Wyciek l/min")->sprzedZmiany);
  // bez normy tymczasowej (13 nocy sprzed zmiany): `uczę się` jak R-KOK-05
  std::vector<Noc> u;
  for (int i = 0; i < 13; i++) u.push_back(nocAutoSet(i, "AutoSet/6/11/2", 10, 9));
  for (int i = 13; i < 18; i++) u.push_back(nocAutoSet(i, "AutoSet/6/12/2", 20, 9));
  const Stany& su = stanyZPamieci(u, u.back().data, PUSTE);
  DaneK1 u1 = budujK1(u, u.back().data, su.norma, {}, &su);
  DaneK2 u2 = budujK2(u, u.back().data, su.norma, {}, &su);
  ROWNE(wierszK1(u1, "Wyciek") ? wierszK1(u1, "Wyciek")->slowo : "", "uczę się");
  ROWNE(wierszK2(u2, "Wyciek l/min") ? wierszK2(u2, "Wyciek l/min")->norma : "", "uczę się");
  SPRAWDZ(!wierszK1(u1, "Wyciek")->sprzedZmiany && !wierszK2(u2, "Wyciek l/min")->sprzedZmiany);
}

struct PlotnoTest : Plotno {
  bool px[480][800] = {};
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[y][x] = c;
  }
  int czarne(int x0, int y0, int x1, int y1) const {
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[y][x];
    return n;
  }
  int roznica(const PlotnoTest& o, int x0, int y0, int x1, int y1) const {  // różne piksele w prostokącie
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[y][x] != o.px[y][x];
    return n;
  }
};

// 20 nocy `6/11/2`, potem `nowych` + 1 nocy `6/12/2` (ostatnia to noc oceniana): nocyNowych = `nowych`
static std::vector<Noc> poZmianie(int nowych) {
  std::vector<Noc> h;
  for (int i = 0; i < 20; i++) h.push_back(nocAutoSet(i, "AutoSet/6/11/2", 10, 10));
  for (int i = 20; i < 21 + nowych; i++) h.push_back(nocAutoSet(i, "AutoSet/6/12/2", 20, 9));
  return h;
}

static void testR_NOR_28_kolko_wypelnienie() {
  // decyzja właściciela projektu 21.11.2026: część koła = noce z nowymi ustawieniami / próg, nigdy pełne
  SPRAWDZ(wypelnienieKolka(0, 14) == 0);
  SPRAWDZ(std::fabs(wypelnienieKolka(3, 14) - 3.0 / 14) < 1e-12);
  SPRAWDZ(std::fabs(wypelnienieKolka(11, 14) - 11.0 / 14) < 1e-12);  // ok. 3/4
  SPRAWDZ(std::fabs(wypelnienieKolka(13, 14) - 13.0 / 14) < 1e-12);
  SPRAWDZ(wypelnienieKolka(14, 14) < 1 && wypelnienieKolka(30, 14) < 1);  // nigdy pełne
  SPRAWDZ(wypelnienieKolka(-5, 14) == 0 && wypelnienieKolka(5, 0) == 0 && wypelnienieKolka(5, 1) == 0);
  SPRAWDZ(szerZnacznikaSprzedZmiany() == ZNACZNIK_KOLKO && ZNACZNIK_KOLKO >= 16 && ZNACZNIK_KOLKO <= 20);
  // piksele: obrys 2 px w każdym stanie, wnętrze rośnie z liczbą nocy, 0 = puste, 13 = z widoczną szczeliną
  auto wnetrze = [](const PlotnoTest& p) { return p.czarne(100 + 4, 92 - 4, 100 + 12, 92 + 4); };
  int poprz = -1;
  for (int n : {0, 3, 7, 11, 13}) {
    static PlotnoTest p;
    p = PlotnoTest();
    rysujKolkoNocy(p, 100, 100, n, 14);
    SPRAWDZ(p.czarne(100, 84, 116, 100) > 0);
    SPRAWDZ(p.czarne(0, 0, 100, 480) == 0 && p.czarne(116, 0, 800, 480) == 0);  // tylko w polu 16 × 16
    SPRAWDZ(p.czarne(100, 0, 116, 84) == 0 && p.czarne(100, 100, 116, 480) == 0);
    SPRAWDZ(p.px[92][100] && p.px[92][101] && p.px[92][115] && p.px[92][114]);  // obrys 2 px z lewej i prawej
    SPRAWDZ(p.px[84][107] && p.px[85][107] && p.px[99][108] && p.px[98][108]);  // u góry i u dołu
    int w = wnetrze(p);
    if (n == 0) SPRAWDZ(w == 0);
    else SPRAWDZ(w > poprz);
    poprz = w;
    // tuż w prawo od godziny 12 czarne od 1 nocy; tuż w lewo białe zawsze (start na 12, nigdy pełne)
    SPRAWDZ(p.px[88][108] == (n > 0));
    SPRAWDZ(!p.px[88][107]);
    // 11 z 14 (ok. 3/4): godzina 9 jeszcze biała (kąt 270° > 283°? nie: 11/14 = 283°, więc 9 czarna), 10:30 biała
    if (n == 11) SPRAWDZ(p.px[92][104] && !p.px[89][105]);
    if (n == 3) SPRAWDZ(p.px[91][111] == false && p.px[89][109]);  // 77°: godzina 1 czarna, godzina 3 biała
  }
}

static void testR_NOR_28_rysowanie_k1_k2() {
  // przy kółku bez legendy na K1 i K2 (R-KOK-17): objaśnienia na ekranie nie ma
  ROWNE(objasnienieSprzedZmiany(800), "");
  ROWNE(objasnienieKolkaLiczb(), "norma sprzed zmiany ustawień; kółko rośnie co noc, nowa po 14.");
  // kółko za najdłuższym słowem oceny K1 mieści się do prawego marginesu 772 (ocena od K1_XM + K1_ODSTEP_OCENY)
  const int xs = K1_XM + K1_ODSTEP_OCENY + szer(F_P22B, "↔ ");
  for (const char* t : {"stabilnie", "rośnie · gorzej", "spada · lepiej", "rośnie", "uczę się"})
    SPRAWDZ(xs + szer(F_P22B, t) + ZNACZNIK_ODSTEP + ZNACZNIK_KOLKO <= 772);
  // K2: najszersze pasmo (`10,4–12,6`) z odstępem i kółkiem w kolumnie od x 650 do 780
  SPRAWDZ(780 - ZNACZNIK_KOLKO - ZNACZNIK_ODSTEP - szer(F_P22B, "10,4–12,6") >= 650);

  std::vector<Noc> h = scenariusz(11);
  std::string dzis = h.back().data;
  const Stany& st = stanyZPamieci(h, dzis, PUSTE);
  DaneK1 k1 = budujK1(h, dzis, st.norma, {}, &st);
  DaneK1 bez = k1;
  for (auto& w : bez.w) w.sprzedZmiany = false;
  static PlotnoTest a, b;
  a = PlotnoTest();
  b = PlotnoTest();
  rysujK1(a, k1);
  rysujK1(b, bez);
  // różnica tylko w kolumnie oceny wierszy z normą tymczasową (kółko za słowem), nigdzie indziej
  int ocena = a.roznica(b, K1_XM + K1_ODSTEP_OCENY, 182, 773, 432);
  SPRAWDZ(ocena > 0);
  SPRAWDZ(a.roznica(b, 0, 0, 800, 480) == ocena);
  SPRAWDZ(a.roznica(b, 0, 150, 800, 182) == 0);  // nagłówki kolumn bez legendy
  // K1 z pasem odkrycia (ocena w jednej linii): kółko też do 772
  DaneK1 odk = k1;
  odk.odkrycieTytul = "Po alkoholu: wyższe AHI";
  odk.odkryciePowod = "AHI 4,1 / h po alkoholu, 1,8 / h bez. Z 6/41 nocy.";
  DaneK1 odkBez = odk;
  for (auto& w : odkBez.w) w.sprzedZmiany = false;
  a = PlotnoTest();
  b = PlotnoTest();
  rysujK1(a, odk);
  rysujK1(b, odkBez);
  SPRAWDZ(a.roznica(b, 0, 0, 800, 480) > 0);
  SPRAWDZ(a.roznica(b, 773, 0, 800, 480) == 0);  // nic za prawym marginesem treści (kreski wierszy sięgają 788)
  DaneK2 k2 = budujK2(h, dzis, st.norma, {}, &st);
  DaneK2 bez2 = k2;
  for (auto& w : bez2.w) w.sprzedZmiany = false;
  a = PlotnoTest();
  b = PlotnoTest();
  rysujK2(a, k2);
  rysujK2(b, bez2);
  int kolumna = a.roznica(b, 648, 120, 781, 434);
  SPRAWDZ(kolumna > 0);
  SPRAWDZ(a.roznica(b, 0, 0, 800, 480) == kolumna);  // zdanie i legenda bez zmian
  // stany 3, 11 i 13 z 14 na K2: kółko pełniejsze przy większej liczbie nocy
  int poprz = -1;
  for (int n : {3, 11, 13}) {
    std::vector<Noc> hn = poZmianie(n);
    Stany sn = liczStany(hn, hn.back().data, PUSTE);
    DaneK2 dn = budujK2(hn, hn.back().data, sn.norma, {}, &sn);
    const WierszK2* w = wierszK2(dn, "Wyciek l/min");
    SPRAWDZ(w && w->sprzedZmiany && w->nocyNowych == n && w->progNocy == 14);
    a = PlotnoTest();
    rysujK2(a, dn);
    int c = a.czarne(780 - ZNACZNIK_KOLKO, 120, 780, 434);
    SPRAWDZ(c > poprz);
    poprz = c;
  }
}

// w ocenie jednej linii (pas odkrycia) kółko nie wypycha strzałki trendu: `↑ rośnie` przed wariantem bez znaku
static void testR_NOR_28_strzalka_przy_kolku() {
  WierszK1 w;
  w.nazwa = "Wyciek";
  w.slowo = "rośnie";
  w.dopisek = "gorzej";
  w.slowo2 = "wyciek widoczny";
  w.sprzedZmiany = true;
  const int maxW = 286 - ZNACZNIK_ODSTEP - ZNACZNIK_KOLKO;
  std::string t = ocenaK1JednaLinia(w, maxW);
  SPRAWDZ(t.rfind("↑", 0) == 0);
  SPRAWDZ(szer(F_P22B, t) <= maxW);
  // przy pełnej szerokości bez zmian: pierwszy pasujący kandydat
  std::string pelna = ocenaK1JednaLinia(w, 800);
  ROWNE(pelna, "↑ rośnie · gorzej · wyciek widoczny");
}

static void testR_NOR_28_co_znacza_te_liczby() {
  // objaśnienie kółka na stronie „Co znaczą te liczby” (5.4.14), pod wierszami, nad zdaniem końcowym (linia 410)
  for (reguly::Tryb t : {reguly::Tryb::AUTOSET, reguly::Tryb::ASV}) {
    podsumowanie::DaneLiczb l = podsumowanie::budujLiczby(t, true);
    ROWNE(l.objasnienieKolka, "norma sprzed zmiany ustawień; kółko rośnie co noc, nowa po 14.");
    static PlotnoTest a, b;
    a = PlotnoTest();
    b = PlotnoTest();
    podsumowanie::rysujLiczby(a, Belka{l.naglowek, ""}, l);
    podsumowanie::DaneLiczb bez = l;
    bez.objasnienieKolka.clear();
    podsumowanie::rysujLiczby(b, Belka{l.naglowek, ""}, bez);
    SPRAWDZ(a.roznica(b, 0, 0, 800, 480) > 0);
    // ostatnia linia (kółko i zdanie) kończy się nad zdaniem końcowym P28B na 410 (wersalik od ok. 389)
    SPRAWDZ(a.czarne(0, 372, 800, 386) == 0);
    SPRAWDZ(a.czarne(28, 100, 28 + ZNACZNIK_KOLKO, 372) > b.czarne(28, 100, 28 + ZNACZNIK_KOLKO, 372));
    SPRAWDZ(28 + ZNACZNIK_KOLKO + ZNACZNIK_ODSTEP + szer(F_P22B, l.objasnienieKolka) <= 772);
  }
}

// ---------------------------------------------------------------- R-NOR-29: reguły bez zmian
static bool rowneWyniki(const std::vector<reguly::Wynik>& x, const std::vector<reguly::Wynik>& y) {
  if (x.size() != y.size()) return false;
  for (size_t i = 0; i < x.size(); i++) {
    const reguly::Wynik &a = x[i], &b = y[i];
    if (a.id != b.id || a.wariant != b.wariant || a.oceniona != b.oceniona || a.spelniona != b.spelniona ||
        a.powodNieoceny != b.powodNieoceny || a.stan != b.stan || a.pilnosc != b.pilnosc || a.poziom != b.poziom ||
        a.grupa != b.grupa || a.zmienne != b.zmienne)
      return false;
  }
  return true;
}

static void testR_NOR_29_reguly_bez_zmian() {
  reguly::zarejestrujRodzajeE();
  wzorce::zarejestrujRodzajeL();
  odkrycia::zarejestrujRodzajeO();
  // scenariusz z dużym wyciekiem i ciśnieniem w ostatnich nocach: przy normie z nocy sprzed zmiany (10–14 L/min)
  // wyciek 40 L/min byłby odchyleniem, więc pomyłka (norma tymczasowa w regułach) zmieniłaby wynik
  std::vector<Noc> h = scenariusz(11);
  for (size_t i = h.size() - 5; i < h.size(); i++) {
    h[i].wp95 = 40;
    h[i].cp95 = 15;
  }
  std::string dzis = h.back().data;
  Stany st = liczStany(h, dzis, PUSTE);
  SPRAWDZ(st.normaKokpitu.sprzedZmiany.wp95.pokaz());  // warunek wstępny: norma tymczasowa jest
  SPRAWDZ(!st.wp95.aktywny && !st.cp95.aktywny);  // R-NOR-29: bez stanu odchylenia z normy tymczasowej
  SPRAWDZ(!st.odchylenie.wp95);
  // reguły dostają Stany::norma bez normy tymczasowej (pasma tylko w Stany::normaKokpitu)
  SPRAWDZ(!st.norma.sprzedZmiany.wp95.pokaz() && st.norma.sprzedZmiany.wp95.nocy == 0);
  SPRAWDZ(st.norma.wp95.n == st.normaKokpitu.wp95.n && st.norma.cmed.n == st.normaKokpitu.cmed.n);
  Stany bez = st;  // kokpit bez normy tymczasowej i bez kontrfaktów R-KOK-37: wynik reguł ma być ten sam
  bez.normaKokpitu = st.norma;
  bez.kokpitAhiZNiepewnymi = !st.kokpitAhiZNiepewnymi;
  bez.kokpitNoceNiepewne.clear();
  const reguly::Zestaw& z = reguly::zestawWbudowany();
  auto ocena = [&](const Stany& s) {
    reguly::Kontekst k;
    k.historia = &h;
    k.dzis = dzis;
    k.norma = &s.norma;
    k.stany = &s;
    k.trybBiezacy = reguly::Tryb::AUTOSET;
    k.zestaw = &z;
    k.wykluczoneNoce = &PUSTE;
    return reguly::ocen(z, k);
  };
  std::vector<reguly::Wynik> zTymczasowa = ocena(st), bezTymczasowej = ocena(bez);
  SPRAWDZ(!zTymczasowa.empty());
  SPRAWDZ(rowneWyniki(zTymczasowa, bezTymczasowej));
  // E-21 (wyciek wobec normy) nie spełnione: metryka bez normy właściwej (R-NOR-11)
  for (auto& w : zTymczasowa)
    if (w.id == "E-21") SPRAWDZ(!w.spelniona);
  // kontrola testu: ta sama ocena z normą tymczasową wstawioną jako właściwa daje inny wynik (test widzi różnicę)
  Stany pomylka = st;
  pomylka.norma.wp95.n = 20;
  pomylka.norma.wp95.med = st.normaKokpitu.sprzedZmiany.wp95.med;
  pomylka.norma.wp95.p10 = st.normaKokpitu.sprzedZmiany.wp95.p10;
  pomylka.norma.wp95.p90 = st.normaKokpitu.sprzedZmiany.wp95.p90;
  pomylka.norma.cp95.n = 20;
  pomylka.norma.cp95.med = st.normaKokpitu.sprzedZmiany.cp95.med;
  pomylka.norma.cp95.p10 = st.normaKokpitu.sprzedZmiany.cp95.p10;
  pomylka.norma.cp95.p90 = st.normaKokpitu.sprzedZmiany.cp95.p90;
  SPRAWDZ(!rowneWyniki(zTymczasowa, ocena(pomylka)));
}

int main() {
  testR_NOR_27_czeste_zmiany_ustawien();
  testR_NOR_27_norma_wlasciwa_bez_zmian();
  testR_NOR_27_ponizej_14_nocy_sprzed_zmiany();
  testR_NOR_27_te_same_wylaczenia();
  testR_NOR_27_bez_odczytu_ustawien();
  testR_NOR_27_bez_znanych_ustawien();
  testR_NOR_27_asv_oddech();
  testR_NOR_27_nowy_model_maski();
  testR_NOR_27_zmiana_trybu();
  testR_NOR_29_tylko_norma_koncowa();
  testR_NOR_28_k1_k2_dane();
  testR_NOR_28_kolko_wypelnienie();
  testR_NOR_28_rysowanie_k1_k2();
  testR_NOR_28_co_znacza_te_liczby();
  testR_NOR_28_strzalka_przy_kolku();
  testR_NOR_29_reguly_bez_zmian();
  printf("testy_norma_tymczasowa: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
