// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: kokpit K1 i K2 w wersji docelowej, słowa `rośnie`/`spada`.
// Napisane przed kodem ze spec funkcjonalnej: R-KOK-16 (reguła słów trendu,
// wariant A), R-KOK-03 i R-KOK-05 (pierwszeństwo stanu odchylenia i rozruchu), R-KOK-15 i fail-path
// (wiersze według trybu bieżącego, ADR-0030, 4.11), R-NOR-19 (noce niepewne poza AHI), R-DANE-29 (noc w trybie
// spoza wydania), R-EKR-59 (`wyciek widoczny` w K1).
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_kokpit_k1_k2).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "ekrany.h"
#include "kokpit.h"
#include "noc.h"
#include "stany.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
#define ROWNE(a, b)                                                                                     \
  do {                                                                                                  \
    std::string _a = (a), _b = (b);                                                                     \
    if (_a == _b) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)
#define ROWNE_L(a, b)                                                                                    \
  do {                                                                                                   \
    long long _a = (long long)(a), _b = (long long)(b);                                                  \
    if (_a == _b) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); }  \
  } while (0)

static const std::string DZIS = "2026-11-12";
static const char* const U_ASVAUTO = "ASVAuto/6/10/3/15";
static const char* const U_ASV = "ASV/8///";
static const char* const U_AUTOSET = "AutoSet/5/13//";
static const char* const U_CPAP = "CPAP/9///";

// Noc spokojna z kompletem metryk K1 (bez nocy niepewnej: mediana wycieku 4 L/min).
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

// 30 nocy kończących się na DZIS (indeks 0 = DZIS − 29): 23 „starsze”, potem 7 „nowych”.
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

// Norma „jest” dla każdej metryki (słowa trendu wymagają normy, R-KOK-05); pasma szerokie, bez znaczenia dla trendu.
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

static int wiersz(const DaneK1& d, const std::string& nazwa) {
  for (int i = 0; i < d.wierszy; i++)
    if (nazwa == d.w[i].nazwa) return i;
  return -1;
}
static int wiersz(const DaneK2& d, const std::string& etykieta) {
  for (int i = 0; i < d.wierszy; i++)
    if (etykieta == d.w[i].etykieta) return i;
  return -1;
}

static std::string slowo(const std::vector<Noc>& h, const std::string& nazwa, const Stany* st = nullptr,
                         const Norma& nm = normaPelna()) {
  DaneK1 d = budujK1(h, DZIS, nm, {}, st);
  int i = wiersz(d, nazwa);
  return i < 0 ? std::string("(brak wiersza)") : d.w[i].slowo;
}

// ---------- R-KOK-16: granica 0,5 × Δ, tuż pod, oba kierunki, każdy wiersz K1 ----------
static void testGranice() {
  // AHI: Δ 3 / h → 1,5
  std::vector<Noc> h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].ahi = 2.5f;
  ROWNE(slowo(h, "AHI"), "rośnie");  // 2,5 − 1,0 = 1,5 ≥ 1,5
  for (int i = 23; i < 30; i++) h[i].ahi = 2.4f;
  ROWNE(slowo(h, "AHI"), "stabilnie");  // 1,4 < 1,5
  for (int i = 0; i < 30; i++) h[i].ahi = i < 23 ? 3.0f : 1.5f;
  ROWNE(slowo(h, "AHI"), "spada");
  for (int i = 23; i < 30; i++) h[i].ahi = 1.6f;
  ROWNE(slowo(h, "AHI"), "stabilnie");
  // mediany, nie średnie: jedna noc 9,0 w oknie 7 nie zmienia słowa
  for (int i = 23; i < 30; i++) h[i].ahi = 3.0f;
  h[29].ahi = 20.0f;
  ROWNE(slowo(h, "AHI"), "stabilnie");

  // czas z maską: Δ 1,5 h → 45 min, w obu kierunkach
  h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].m01 = 375;
  ROWNE(slowo(h, "Czas z maską"), "spada");  // −45 min
  for (int i = 23; i < 30; i++) h[i].m01 = 376;
  ROWNE(slowo(h, "Czas z maską"), "stabilnie");
  for (int i = 23; i < 30; i++) h[i].m01 = 465;
  ROWNE(slowo(h, "Czas z maską"), "rośnie");

  // wyciek P95: Δ 10 L/min → 5
  h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].wp95 = 15.0f;
  ROWNE(slowo(h, "Wyciek"), "rośnie");
  for (int i = 23; i < 30; i++) h[i].wp95 = 14.9f;
  ROWNE(slowo(h, "Wyciek"), "stabilnie");
  for (int i = 0; i < 30; i++) h[i].wp95 = i < 23 ? 9.0f : 4.0f;
  ROWNE(slowo(h, "Wyciek"), "spada");

  // EPAP (M-06 mediana) w ASV i ASVAuto: Δ M-06p 1,5 cmH2O (komplet progów 11.11.2026) → 0,75
  for (const char* u : {U_ASVAUTO, U_ASV}) {
    h = okno(u);
    for (int i = 23; i < 30; i++) h[i].emed = 7.75f;
    ROWNE(slowo(h, "EPAP"), "rośnie");
    for (int i = 23; i < 30; i++) h[i].emed = 7.7f;
    ROWNE(slowo(h, "EPAP"), "stabilnie");
    for (int i = 23; i < 30; i++) h[i].emed = 6.0f;
    ROWNE(slowo(h, "EPAP"), "spada");
  }
  // EPAP docelowe urządzenia (STR) nie liczy się do trendu (R-NOR-08): 4 noce z EDF w oknie 7 → słowo, 3 → `uczę się`
  h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].emed = 8.0f;
  for (int i = 26; i < 30; i++) h[i].epapStr = true;
  ROWNE(slowo(h, "EPAP"), "uczę się");
  h[26].epapStr = false;
  ROWNE(slowo(h, "EPAP"), "rośnie");

  // ciśnienie (M-05 mediana) w CPAP i AutoSet: Δ M-05p 2 cmH2O → 1,0
  for (const char* u : {U_AUTOSET, U_CPAP}) {
    h = okno(u);
    for (int i = 23; i < 30; i++) h[i].cmed = 10.0f;
    ROWNE(slowo(h, "Ciśnienie"), "rośnie");
    for (int i = 23; i < 30; i++) h[i].cmed = 9.9f;
    ROWNE(slowo(h, "Ciśnienie"), "stabilnie");
    for (int i = 23; i < 30; i++) h[i].cmed = 8.0f;
    ROWNE(slowo(h, "Ciśnienie"), "spada");
    // pozostałe wiersze działają tak samo w każdym trybie
    for (int i = 23; i < 30; i++) h[i].ahi = 2.5f;
    ROWNE(slowo(h, "AHI"), "rośnie");
  }

  // wsparcie PS (M-28): bez Δ w 4.5, ale od słowo trendu z Δ 2 cmH2O
  // jak EPAP; bez normy `uczę się` (R-KOK-05)
  h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].psmed = 10.0f;
  ROWNE(slowo(h, "Wsparcie"), "rośnie");
  Norma bezPs = normaPelna();
  bezPs.psmed = NormaMetryki();
  ROWNE(slowo(h, "Wsparcie", nullptr, bezPs), "uczę się");
}

// ---------- pierwszeństwo: rozruch (R-KOK-05) i stan odchylenia (R-KOK-03) przed trendem ----------
static void testPierwszenstwo() {
  std::vector<Noc> h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) {
    h[i].ahi = 4.0f;
    h[i].wp95 = 20.0f;
    h[i].m01 = 300;
  }
  Stany st;
  st.ahi.aktywny = true;
  st.ahi.wyzej = true;
  st.ahi.nocy = 5;
  st.m01.aktywny = true;
  st.m01.wyzej = false;
  st.m01.nocy = 3;
  DaneK1 d = budujK1(h, DZIS, normaPelna(), {}, &st);
  const WierszK1& ahi = d.w[wiersz(d, "AHI")];
  SPRAWDZ(ahi.stan);
  ROWNE(ahi.slowo + " / " + ahi.slowo2, "wyżej niż zwykle / od 5 nocy");
  const WierszK1& cz = d.w[wiersz(d, "Czas z maską")];
  ROWNE(cz.slowo + " / " + cz.slowo2, "niżej niż zwykle / od 3 nocy");
  const WierszK1& wy = d.w[wiersz(d, "Wyciek")];
  SPRAWDZ(!wy.stan);
  ROWNE(wy.slowo, "rośnie");  // bez stanu: trend
  // bez normy: `uczę się`, nawet przy trendzie i przy stanie
  Norma bez = normaPelna();
  bez.ahi = NormaMetryki();
  bez.wp95 = NormaMetryki();
  d = budujK1(h, DZIS, bez, {}, &st);
  ROWNE(d.w[wiersz(d, "AHI")].slowo, "uczę się");
  SPRAWDZ(!d.w[wiersz(d, "AHI")].stan);
  ROWNE(d.w[wiersz(d, "Wyciek")].slowo, "uczę się");
}

// ---------- za mało nocy w którymś oknie: `uczę się` (próg R-KOK-16: 4 z 7, 14 z 23) ----------
static void testZaMaloNocy() {
  std::vector<Noc> h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].ahi = 2.5f;
  // okno 7: 3 noce z wartością → `uczę się`, 4 → słowo
  std::vector<Noc> h7 = h;
  h7.erase(h7.begin() + 23, h7.begin() + 27);  // zostają noce 27, 28, 29
  ROWNE(slowo(h7, "AHI"), "uczę się");
  h7 = h;
  h7.erase(h7.begin() + 23, h7.begin() + 26);  // 26…29
  ROWNE(slowo(h7, "AHI"), "rośnie");
  // okno 23: 13 nocy → `uczę się`, 14 → słowo; noc bez wartości metryki liczy się jak brak nocy
  std::vector<Noc> h23 = h;
  for (int i = 0; i < 10; i++) h23[i].ahi = NAN;  // 13 nocy z wartością
  ROWNE(slowo(h23, "AHI"), "uczę się");
  h23[9].ahi = 1.0f;  // 14
  ROWNE(slowo(h23, "AHI"), "rośnie");
  // pusta historia: `uczę się` w każdym wierszu
  DaneK1 d = budujK1({}, DZIS, normaPelna(), {});
  for (int i = 0; i < d.wierszy; i++)
    ROWNE(d.w[i].slowo, "uczę się");  // także `Wsparcie` (trend PS z Δ 2 cmH2O)
}

// ---------- R-NOR-19: noce niepewne poza trendem AHI (jak w normie), w innych wierszach zostają ----------
static void testNoceNiepewne() {
  std::vector<Noc> h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].ahi = 9.0f;
  for (int i = 26; i < 30; i++) h[i].wmed = 31.0f;  // 4 noce niepewne (4.9: mediana wycieku > 30 L/min)
  SPRAWDZ(nocNiepewna(h[29]) && !nocNiepewna(h[25]));
  ROWNE(slowo(h, "AHI"), "uczę się");  // bez niepewnych w oknie 7 zostają 3 noce
  h[25].ahi = 9.0f;
  h[26].wmed = 4.0f;  // 4 noce pewne z 9,0
  ROWNE(slowo(h, "AHI"), "rośnie");
  // noce niepewne zostają w trendzie wycieku i czasu z maską
  h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) {
    h[i].wmed = 31.0f;
    h[i].wp95 = 16.0f;
  }
  ROWNE(slowo(h, "Wyciek"), "rośnie");
  // niepewne w oknie 23 też wypadają z AHI: 10 niepewnych → 13 pewnych → `uczę się`
  h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].ahi = 2.5f;
  for (int i = 0; i < 10; i++) h[i].wmed = 31.0f;
  ROWNE(slowo(h, "AHI"), "uczę się");
}

// ---------- R-KOK-15 i ADR-0030: wiersze K1 i K2 według trybu bieżącego ----------
static void testTryby() {
  struct { const char* u; bool asv; } tryby[4] = {{U_CPAP, false}, {U_AUTOSET, false}, {U_ASV, true}, {U_ASVAUTO, true}};
  for (auto& t : tryby) {
    std::vector<Noc> h = okno(t.u);
    h.back().cmed = 9.6f;
    h.back().emed = 7.2f;
    DaneK1 k1 = budujK1(h, DZIS, normaPelna(), {});
    DaneK2 k2 = budujK2(h, DZIS, normaPelna(), {});
    if (t.asv) {
      ROWNE_L(k1.wierszy, 5);
      ROWNE(std::string(k1.w[0].nazwa) + "|" + k1.w[1].nazwa + "|" + k1.w[2].nazwa + "|" + k1.w[3].nazwa + "|" + k1.w[4].nazwa,
            "AHI|Czas z maską|EPAP|Wsparcie|Wyciek");
      ROWNE(k1.opisy[2], "EPAP");
      ROWNE(k1.liczby[2], "7,2");
      ROWNE_L(k2.wierszy, 10);
      ROWNE(k2.w[7].etykieta, "EPAP P95");
      ROWNE(k2.w[8].etykieta, "Wsparcie PS");
      ROWNE(k2.w[9].etykieta, "Oddechy /min");
      SPRAWDZ(wiersz(k2, "Ciśnienie P95") < 0 && wiersz(k1, "Ciśnienie") < 0);
    } else {
      ROWNE_L(k1.wierszy, 4);
      ROWNE(std::string(k1.w[0].nazwa) + "|" + k1.w[1].nazwa + "|" + k1.w[2].nazwa + "|" + k1.w[3].nazwa,
            "AHI|Czas z maską|Ciśnienie|Wyciek");
      ROWNE(k1.opisy[2], "ciśnienie");
      ROWNE(k1.liczby[2], "9,6");
      ROWNE_L(k2.wierszy, 9);
      ROWNE(k2.w[7].etykieta, "Ciśnienie P95");
      ROWNE(k2.w[7].k[6].tekst, "10,5");
      ROWNE(k2.w[7].norma, "10,0–11,0");
      ROWNE(k2.w[8].etykieta, "Oddechy /min");
      SPRAWDZ(wiersz(k2, "EPAP P95") < 0 && wiersz(k2, "Wsparcie PS") < 0 && wiersz(k1, "EPAP") < 0 &&
              wiersz(k1, "Wsparcie") < 0);
    }
    // wspólne wiersze K2 w tej samej kolejności
    ROWNE(std::string(k2.w[0].etykieta) + "|" + k2.w[4].etykieta + "|" + k2.w[5].etykieta + "|" + k2.w[6].etykieta,
          "AHI|Czas z maską|Zdjęcia maski|Wyciek l/min");
    SPRAWDZ(k2.w[4].grupa && k2.w[6].grupa);
  }
  // bez żadnej nocy ze znanym trybem (stary import bez ustawień): układ ASV jak przed ADR-0030
  std::vector<Noc> h = okno("");
  for (Noc& n : h) n.zrodlo = 'K';
  ROWNE_L(budujK1(h, DZIS, normaPelna(), {}).wierszy, 5);
  ROWNE_L(budujK2(h, DZIS, normaPelna(), {}).wierszy, 10);
  SPRAWDZ(trybBiezacy(h, DZIS) == reguly::Tryb::NIEZNANY);
  for (int i = 23; i < 30; i++) h[i].ahi = 2.5f;
  ROWNE(slowo(h, "AHI"), "rośnie");  // noce z karty bez trybu liczą się do okien (jak)
  // te same noce z AirBridge (źródło A) bez odczytu trybu: tryb nieznany, poza oknami (R-DANE-29)
  for (Noc& n : h) n.zrodlo = 'A';
  ROWNE(slowo(h, "AHI"), "uczę się");
  ROWNE_L(budujK1(h, DZIS, normaPelna(), {}).wierszy, 5);
  // noc z karty z pustymi ustawieniami po nocach AutoSet: tryb ostatnich znanych (AutoSet); pole `tryb` wygrywa
  h = okno(U_AUTOSET);
  h.back().ustawienia = "";
  h.back().zrodlo = 'K';
  ROWNE_L(budujK1(h, DZIS, normaPelna(), {}).wierszy, 4);
  SPRAWDZ(trybBiezacy(h, DZIS) == reguly::Tryb::AUTOSET);
  h.back().zrodlo = 'A';  // noc z AirBridge bez trybu: nieznany, tryb bieżący z nocy wcześniejszej
  SPRAWDZ(trybBiezacy(h, DZIS) == reguly::Tryb::AUTOSET);
  h.back().tryb = "ASVAuto";
  SPRAWDZ(trybBiezacy(h, DZIS) == reguly::Tryb::ASVAUTO);
  ROWNE_L(budujK1(h, DZIS, normaPelna(), {}).wierszy, 5);
  SPRAWDZ(trybBiezacy(okno(U_CPAP), DZIS) == reguly::Tryb::CPAP && trybBiezacy(okno(U_ASV), DZIS) == reguly::Tryb::ASV);
  SPRAWDZ(trybBiezacy({}, DZIS) == reguly::Tryb::NIEZNANY);
}

// ---------- fail-path R-KOK-15: zmiana trybu w oknie i noc w trybie spoza wydania (R-DANE-29) ----------
static void testZmianaTrybu() {
  // 26 nocy ASVAuto, potem 4 noce AutoSet → tryb bieżący AutoSet
  std::vector<Noc> h = okno(U_ASVAUTO);
  for (int i = 26; i < 30; i++) {
    h[i].ustawienia = U_AUTOSET;
    h[i].cmed = 9.5f;
    h[i].cp95 = 11.5f;
  }
  for (int i = 0; i < 26; i++) h[i].cmed = 14.0f;  // ciśnienie wdechowe ASVAuto (IPAP): nie miesza się z AutoSet
  DaneK1 k1 = budujK1(h, DZIS, normaPelna(), {});
  ROWNE_L(k1.wierszy, 4);
  const WierszK1& c = k1.w[wiersz(k1, "Ciśnienie")];
  int ile = 0;
  for (int i = 0; i < 30; i++) ile += c.linia.ma[i];
  ROWNE_L(ile, 4);                  // linia 30 nocy tylko z nocy w trybie bieżącym
  ROWNE(c.mediana, "9,5");          // mediana też
  ROWNE(c.slowo, "uczę się");       // 0 nocy AutoSet w oknie 23
  ROWNE(k1.w[wiersz(k1, "AHI")].slowo, "stabilnie");  // wiersze niezależne od trybu z wszystkich nocy
  DaneK2 k2 = budujK2(h, DZIS, normaPelna(), {});
  const WierszK2& p95 = k2.w[wiersz(k2, "Ciśnienie P95")];
  for (int i = 0; i < 3; i++) ROWNE(p95.k[i].tekst, "–");  // noce 23…25 w ASVAuto
  for (int i = 3; i < 7; i++) ROWNE(p95.k[i].tekst, "11,5");
  for (int i = 0; i < 7; i++) ROWNE(k2.w[0].k[i].tekst, "1,0");  // AHI ze wszystkich nocy

  // ASV ze stałym EPAP i ASVAuto to różne tryby: noc ASV w K2 trybu ASVAuto ma `–` w wierszach EPAP i PS
  h = okno(U_ASVAUTO);
  h[28].ustawienia = U_ASV;
  k2 = budujK2(h, DZIS, normaPelna(), {});
  ROWNE(k2.w[wiersz(k2, "EPAP P95")].k[5].tekst, "–");
  ROWNE(k2.w[wiersz(k2, "Wsparcie PS")].k[5].tekst, "–");
  ROWNE(k2.w[wiersz(k2, "EPAP P95")].k[6].tekst, "8,0");

  // noc w trybie spoza wydania (VAuto): tryb bieżący zostaje ASVAuto, noc bez oceny i poza trendem każdego wiersza
  h = okno(U_ASVAUTO);
  for (int i = 23; i < 30; i++) h[i].ahi = 9.0f;
  for (int i = 26; i < 30; i++) h[i].ustawienia = "VAuto/4/20//";
  k1 = budujK1(h, DZIS, normaPelna(), {});
  ROWNE_L(k1.wierszy, 5);
  ROWNE(k1.w[wiersz(k1, "AHI")].slowo, "uczę się");  // 3 noce ASVAuto w oknie 7
  ROWNE(k1.liczby[1], "9,0");                          // liczby nocy jak zapisane (4.11)
  k2 = budujK2(h, DZIS, normaPelna(), {});
  ROWNE(k2.w[0].k[6].tekst, "9,0");
  ROWNE(k2.w[wiersz(k2, "EPAP P95")].k[6].tekst, "–");
}

// ---------- R-EKR-59: `wyciek widoczny` w drugiej linii oceny wiersza `Wyciek` ----------
static void testWyciekWidoczny() {
  std::vector<Noc> h = okno(U_ASVAUTO);
  for (Noc& n : h) n.wp95 = 11.0f;
  DaneK1 d = budujK1(h, DZIS, normaPelna(), {});
  const WierszK1& w = d.w[wiersz(d, "Wyciek")];
  ROWNE(w.mediana, "11");
  ROWNE(w.slowo + " / " + w.slowo2, "stabilnie / wyciek widoczny");
  SPRAWDZ(!w.stan);
  for (Noc& n : h) n.wp95 = 10.4f;  // na ekranie `10`: próg „powyżej”, bez słowa
  d = budujK1(h, DZIS, normaPelna(), {});
  ROWNE(d.w[wiersz(d, "Wyciek")].slowo2, "");
  // z trendem: `rośnie` / `wyciek widoczny`
  h = okno(U_ASVAUTO);
  for (int i = 0; i < 30; i++) h[i].wp95 = i < 23 ? 11.0f : 17.0f;
  d = budujK1(h, DZIS, normaPelna(), {});
  ROWNE(d.w[wiersz(d, "Wyciek")].slowo + " / " + d.w[wiersz(d, "Wyciek")].slowo2, "rośnie / wyciek widoczny");
  // w stanie odchylenia: `wyżej niż zwykle` / `od {n} nocy`, bez `wyciek widoczny`
  Stany st;
  st.wp95.aktywny = true;
  st.wp95.nocy = 9;
  d = budujK1(h, DZIS, normaPelna(), {}, &st);
  ROWNE(d.w[wiersz(d, "Wyciek")].slowo + " / " + d.w[wiersz(d, "Wyciek")].slowo2, "wyżej niż zwykle / od 9 nocy");
  // bez normy: `uczę się` / `wyciek widoczny` (informacja, nie ocena)
  Norma bez = normaPelna();
  bez.wp95 = NormaMetryki();
  d = budujK1(h, DZIS, bez, {});
  ROWNE(d.w[wiersz(d, "Wyciek")].slowo + " / " + d.w[wiersz(d, "Wyciek")].slowo2, "uczę się / wyciek widoczny");
  // tylko wiersz `Wyciek`
  for (int i = 0; i < d.wierszy; i++)
    if (std::string(d.w[i].nazwa) != "Wyciek") ROWNE(d.w[i].slowo2, "");
}

// ---------- szerokości tekstów (SPEC-KOKPIT 3 i 4) ----------
struct PlotnoTest : Plotno {
  std::vector<unsigned char> px = std::vector<unsigned char>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[y * 800 + x] = c;
  }
  int czarne(int x0, int y0, int x1, int y1) const {
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[y * 800 + x];
    return n;
  }
};

static void testSzerokosci() {
  // ocena K1: P22B od x 522 do 772 (250 px)
  for (const char* s : {"rośnie", "spada", "stabilnie", "uczę się", "wyżej niż zwykle", "niżej niż zwykle",
                        "od 999 nocy", "wyciek widoczny"})
    if (szer(F_P22B, s) > 250) { zle++; printf("BŁĄD ocena K1 za szeroka: %s\n", s); } else ok++;
  // nazwa wiersza K1 (P24B od x 28) przed linią 30 nocy (x 196)
  for (const char* s : {"AHI", "Czas z maską", "Ciśnienie", "EPAP", "Wsparcie", "Wyciek"})
    SPRAWDZ(28 + szer(F_P24B, s) <= 196 - 8);
  // opis liczby K1 `ciśnienie` (kolumna od 398 do 608)
  SPRAWDZ(398 + szer(F_P22B, "ciśnienie") <= 608 - 8);
  // etykieta K2 `Ciśnienie P95` w kolumnie 20–164, także w ramce (R-KOK-09)
  SPRAWDZ(20 + szer(F_P22B, "Ciśnienie P95") <= 164);
  // w ramce (R-KOK-09) `Ciśnienie P95` miałoby 175 px > 171: dziś bez ramki, bo stanu M-05p panel nie liczy
  // (src/stany.h: tylko M-06p); stan EPAP P95 z ASV nie obramowuje wiersza ciśnienia w AutoSet
  {
    Stany st;
    st.ep95.aktywny = true;
    st.ep95.nocy = 4;
    DaneK2 k = budujK2(okno(U_AUTOSET), DZIS, normaPelna(), {}, &st);
    SPRAWDZ(!k.w[wiersz(k, "Ciśnienie P95")].stan);
  }
  // K1 w CPAP: 4 wiersze, pas piątego wiersza (382…432) pusty; w ASV narysowany
  std::vector<Noc> h = okno(U_AUTOSET);
  PlotnoTest p;
  rysujK1(p, budujK1(h, DZIS, normaPelna(), {}));
  ROWNE_L(p.czarne(0, 384, 800, 430), 0);
  PlotnoTest q;
  rysujK1(q, budujK1(okno(U_ASVAUTO), DZIS, normaPelna(), {}));
  SPRAWDZ(q.czarne(0, 384, 800, 430) > 0);
  // K2 w CPAP: 9 wierszy, legenda o jeden wiersz wyżej niż w ASV; noce z każdą metryką z AirBridge, żeby
  // żaden wiersz nie zwinął się do linii `brak danych: …` (R-KOK-18)
  auto komplet = [](std::vector<Noc> v) {
    for (Noc& n : v) { n.oa = 0.4f; n.ca = 0.3f; n.h = 0.3f; n.m13 = 14.0f; n.m17 = 1; }
    return v;
  };
  PlotnoTest r, s;
  rysujK2(r, budujK2(komplet(h), DZIS, normaPelna(), {}));
  rysujK2(s, budujK2(komplet(okno(U_ASVAUTO)), DZIS, normaPelna(), {}));
  SPRAWDZ(r.czarne(0, 400, 800, 432) < s.czarne(0, 400, 800, 432));
}

// ---------- wiersze K2 z pól `Zdjęcia maski` (M-17), `Oddechy /min` (M-13); stan CA/h i M-17 (R-KOK-09) ----------
static void testWierszeB1() {
  std::vector<Noc> h = okno(U_ASVAUTO);
  for (int i = 0; i < 30; i++) {
    h[i].m17 = i == 29 ? 3 : 1;
    h[i].m13 = 14.3f;
    h[i].ca = 0.4f;
    h[i].oa = 0.3f;  // wszystkie wiersze z wartością: bez zwinięcia (R-KOK-18), pozycje wierszy stałe
    h[i].h = 0.2f;
  }
  h[27].m17 = -1;  // brak pola (noc z karty): `–`
  h[28].m13 = NAN;
  Norma nm = normaPelna();
  nm.m17 = nmJest(1, 0, 2);
  DaneK2 k = budujK2(h, DZIS, nm, {});
  const WierszK2& zm = k.w[wiersz(k, "Zdjęcia maski")];
  ROWNE(zm.k[6].tekst, "3");
  ROWNE(zm.k[5].tekst, "1");
  ROWNE(zm.k[4].tekst, "–");
  SPRAWDZ(zm.k[6].poza && !zm.k[5].poza);  // R-KOK-08: 3 poza pasmem 0–2
  ROWNE(zm.norma, "0–2");
  const WierszK2& od = k.w[wiersz(k, "Oddechy /min")];
  ROWNE(od.k[6].tekst, "14");
  ROWNE(od.k[5].tekst, "–");
  ROWNE(od.norma, "–");  // normy M-13 panel nie liczy: kreska, nie `uczę się`
  SPRAWDZ(!od.k[6].poza);
  // bez normy M-17: `uczę się` (R-KOK-05)
  ROWNE(budujK2(h, DZIS, normaPelna(), {}).w[wiersz(k, "Zdjęcia maski")].norma, "uczę się");
  // stany odchylenia z wersji beta etykiety `centralne` i `Zdjęcia maski` w ramce
  Stany st;
  st.ca.aktywny = true;
  st.m17.aktywny = true;
  nm.ca = nmJest(0.4, 0.2, 0.6);
  k = budujK2(h, DZIS, nm, {}, &st);
  SPRAWDZ(k.w[wiersz(k, "centralne")].stan && k.w[wiersz(k, "Zdjęcia maski")].stan && !k.w[0].stan);
  // ramka etykiety `Zdjęcia maski` (143 px) kończy się co najmniej 2 px przed 1. komórką nocy (x 171): 169…174 białe
  PlotnoTest p;
  rysujK2(p, k);
  int y = 120 + 5 * 28 + 2;  // wiersz 6 (po linii grupy przed `Czas z maską`)
  SPRAWDZ(p.czarne(20, y + 2, 22, y + 26) > 0);    // lewa krawędź ramki
  ROWNE_L(p.czarne(169, y + 2, 175, y + 26), 0);  // odstęp 2 px od komórki nocy
  SPRAWDZ(p.czarne(166, y + 2, 169, y + 26) > 0);  // prawa krawędź ramki na 167…168
  // noce bez M-13 i M-17 w 30 nocach (noce z karty): jak wiersz bez danych
  std::vector<Noc> hk = okno(U_ASVAUTO);
  k = budujK2(hk, DZIS, nm, {});
  ROWNE(k.w[wiersz(k, "Zdjęcia maski")].norma, "–");
  ROWNE(k.w[wiersz(k, "Oddechy /min")].k[6].tekst, "–");
}

int main() {
  testGranice();
  testPierwszenstwo();
  testZaMaloNocy();
  testNoceNiepewne();
  testTryby();
  testZmianaTrybu();
  testWyciekWidoczny();
  testSzerokosci();
  testWierszeB1();
  printf("testy_kokpit_k1_k2: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
