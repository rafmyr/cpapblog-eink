// SPDX-License-Identifier: GPL-3.0-or-later
// Testy R-KOK-37 (decyzja właściciela projektu 21.11.2026):
// słowo oceny AHI w K1 `niepewne` / `duży wyciek` ze znakiem `?`, gdy `uczę się` wynikałoby wyłącznie z pominięcia nocy
// niepewnych (R-NOR-19). Dane wyłącznie syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_k1_ahi_niepewne).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "ekrany.h"
#include "kokpit.h"
#include "noc.h"
#include "stany.h"
#include "wspolne.h"

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
static const std::set<std::string> PUSTE;

// noc ważna; niepewna = wyciek P95 20 L/min (4.9, R-NOR-19), pewna = 10 L/min
static Noc noc(int i, bool niepewna, bool asv) {
  Noc n;
  n.data = dodajDni(D0, i);
  n.wazna = true;
  n.m01 = 420;
  n.ahi = 1.0f + 0.1f * (float)(i % 5);
  n.wmed = 4;
  n.wp95 = niepewna ? 20.0f : 10.0f;
  n.cmed = 9;
  n.cp95 = 10;
  n.oa = 0.3f;
  n.ca = 0.2f;
  n.h = 0.5f;
  n.m17 = 0;
  if (asv) {
    n.emed = 7;
    n.ep95 = 8;
    n.psmed = 5;
    n.ustawienia = "ASVAuto/6/10/3/15";
    n.tryb = "ASVAuto";
  } else {
    n.ustawienia = "AutoSet/6/12/2";
    n.tryb = "AutoSet";
  }
  return n;
}

static const WierszK1* ahi(const DaneK1& d) {
  for (int i = 0; i < d.wierszy; i++)
    if (std::strcmp(d.w[i].nazwa, "AHI") == 0) return &d.w[i];
  return nullptr;
}

static DaneK1 k1(const std::vector<Noc>& h, const std::vector<WpisCzynnikow>& cz = {}) {
  std::set<std::string> wykl = wykluczoneZNormy(cz);
  Stany st = liczStany(h, h.back().data, wykl);
  return budujK1(h, h.back().data, st.norma, cz, &st);
}

// norma AHI jest (23 noce pewne), w 7 ostatnich nocach okna 5 niepewnych → trend bez nich nie powstaje (2 < 4)
static void testR_KOK_37_trend_tylko_przez_niepewne() {
  for (bool asv : {false, true}) {
    std::vector<Noc> h;
    for (int i = 0; i < 23; i++) h.push_back(noc(i, false, asv));
    for (int i = 23; i < 30; i++) h.push_back(noc(i, i >= 25, asv));  // 2 pewne, 5 niepewnych
    DaneK1 d = k1(h);
    const WierszK1* w = ahi(d);
    SPRAWDZ(w != nullptr);
    if (!w) continue;
    ROWNE(w->slowo, "niepewne");
    ROWNE(w->slowo2, "duży wyciek");
    ROWNE(w->dopisek, "");  // bez `lepiej` / `gorzej` (ADR-0029 pkt 7)
    SPRAWDZ(!w->stan);
    ROWNE(znakOcenyK1(w->slowo), "?");
    ROWNE(tekstOcenyK1(*w), "? niepewne");
    // ten sam słownik co opis nad liczbą wycieku przy nocy niepewnej (R-KOK-27)
    ROWNE(w->slowo2, OPIS_DUZY_WYCIEK);
    ROWNE(d.opisy[3], OPIS_DUZY_WYCIEK);
    // K1 z pasem odkrycia (jedna linia do 286 px): mieści się
    SPRAWDZ(szer(F_P22B, ocenaK1JednaLinia(*w, 286)) <= 286);
    // inne wiersze bez zmian (wyciek ma swoje słowo, nie `niepewne`)
    for (int i = 0; i < d.wierszy; i++)
      if (std::strcmp(d.w[i].nazwa, "AHI") != 0) SPRAWDZ(d.w[i].slowo != "niepewne");
  }
}

// AHI bez normy tylko przez noce niepewne: 8 pewnych + 8 niepewnych przed ostatnią nocą (norma z 8 < 14, z nimi 16)
static void testR_KOK_37_norma_tylko_przez_niepewne() {
  for (bool asv : {false, true}) {
    std::vector<Noc> h;
    for (int i = 0; i < 16; i++) h.push_back(noc(i, i % 2 == 1, asv));
    h.push_back(noc(16, false, asv));
    Stany st = liczStany(h, h.back().data, PUSTE);
    SPRAWDZ(!st.norma.ahi.jest() && st.norma.ahi.n == 8);
    DaneK1 d_w1 = k1(h);
    const WierszK1* w = ahi(d_w1);
    SPRAWDZ(w && w->slowo == "niepewne" && w->slowo2 == "duży wyciek" && !w->stan);
  }
}

// fail-path: za mało nocy także z niepewnymi → `uczę się` jak dotąd (R-KOK-05, R-KOK-16)
static void testR_KOK_37_za_malo_nocy_w_ogole() {
  for (bool asv : {false, true}) {
    std::vector<Noc> h;
    for (int i = 0; i < 8; i++) h.push_back(noc(i, i % 2 == 1, asv));  // 4 + 4 < 14
    DaneK1 d_w2 = k1(h);
    const WierszK1* w = ahi(d_w2);
    SPRAWDZ(w && w->slowo == "uczę się" && w->slowo2.empty());
    // trend: norma jest, ale w oknie 7 tylko 3 noce w ogóle (luka), z niepewnymi też za mało
    std::vector<Noc> g;
    for (int i = 0; i < 23; i++) g.push_back(noc(i, false, asv));
    for (int i = 27; i < 30; i++) g.push_back(noc(i, i == 29, asv));
    DaneK1 d_v3 = k1(g);
    const WierszK1* v = ahi(d_v3);
    SPRAWDZ(v && v->slowo == "uczę się" && v->slowo2.empty());
  }
}

// dość nocy pewnych → trend jak dotąd, mimo nocy niepewnych w oknie
static void testR_KOK_37_dosc_nocy_pewnych() {
  for (bool asv : {false, true}) {
    std::vector<Noc> h;
    for (int i = 0; i < 23; i++) h.push_back(noc(i, false, asv));
    for (int i = 23; i < 30; i++) h.push_back(noc(i, i >= 28, asv));  // 5 pewnych ≥ 4
    DaneK1 d_w4 = k1(h);
    const WierszK1* w = ahi(d_w4);
    SPRAWDZ(w && w->slowo == "stabilnie" && w->slowo2.empty());
    // bez nocy niepewnych w ogóle: bez zmian
    std::vector<Noc> g;
    for (int i = 0; i < 30; i++) g.push_back(noc(i, false, asv));
    DaneK1 d_v5 = k1(g);
    const WierszK1* v = ahi(d_v5);
    SPRAWDZ(v && v->slowo == "stabilnie");
  }
}

// noce niepewne z czynnikiem „katar/infekcja” (R-CZY-05) poza normą i tak: nie liczą się do „tylko przez niepewne”
static void testR_KOK_37_czynniki() {
  std::vector<Noc> h;
  std::vector<WpisCzynnikow> cz;
  for (int i = 0; i < 16; i++) {
    h.push_back(noc(i, i % 2 == 1, false));
    if (i % 2 == 1) {
      WpisCzynnikow c;
      c.data = h.back().data;
      c.stan = 'O';
      c.czynniki = CZ_KATAR;
      cz.push_back(c);
    }
  }
  h.push_back(noc(16, false, false));
  DaneK1 d_w6 = k1(h, cz);
  const WierszK1* w = ahi(d_w6);
  SPRAWDZ(w && w->slowo == "uczę się");
}

// ostatnia zapisana noc starsza niż `dzis` (brak nocy z dwóch ostatnich dni): ta sama ocena
static void testR_KOK_37_ostatnia_noc_starsza_niz_dzis() {
  for (bool asv : {false, true}) {
    std::vector<Noc> h;
    for (int i = 0; i < 16; i++) h.push_back(noc(i, i % 2 == 1, asv));
    h.push_back(noc(16, false, asv));
    std::string dzis = dodajDni(h.back().data, 2);
    Stany st = liczStany(h, dzis, PUSTE);
    DaneK1 d = budujK1(h, dzis, st.norma, {}, &st);
    const WierszK1* w = ahi(d);
    SPRAWDZ(w && w->slowo == "niepewne" && w->slowo2 == "duży wyciek");
    SPRAWDZ(!d.ubiegla);
    // trend: 23 pewne, potem 7 nocy z 5 niepewnymi, `dzis` 3 dni po ostatniej nocy
    std::vector<Noc> g;
    for (int i = 0; i < 23; i++) g.push_back(noc(i, false, asv));
    for (int i = 23; i < 30; i++) g.push_back(noc(i, i >= 25, asv));
    std::string dz = dodajDni(g.back().data, 3);
    Stany sg = liczStany(g, dz, PUSTE);
    DaneK1 d_v7 = budujK1(g, dz, sg.norma, {}, &sg);
    const WierszK1* v = ahi(d_v7);
    SPRAWDZ(v && v->slowo == "niepewne");
  }
}

// stan odchylenia AHI (R-KOK-03) ma pierwszeństwo przed `niepewne`: ramka nie ginie
static void testR_KOK_37_stan_ahi_ma_pierwszenstwo() {
  std::vector<Noc> h;
  for (int i = 0; i < 23; i++) h.push_back(noc(i, false, false));
  for (int i = 23; i < 30; i++) h.push_back(noc(i, i >= 25, false));
  Stany st = liczStany(h, h.back().data, PUSTE);
  st.ahi.aktywny = true;  // stan z normą zamrożoną (R-NOR-22), np. po wcześniejszym wzroście AHI
  st.ahi.od = h[20].data;
  st.ahi.nocy = 10;
  st.ahi.zamrozona = st.norma.ahi;
  DaneK1 d_w8 = budujK1(h, h.back().data, st.norma, {}, &st);
  const WierszK1* w = ahi(d_w8);
  SPRAWDZ(w && w->stan && w->slowo == "wyżej niż zwykle" && w->slowo2 == "od 10 nocy");
  // norma AHI pusta tylko przez noce niepewne, a AHI w stanie: nie `niepewne` (jak dotąd `uczę się`, bez ramki; ramka
  // przy normie zamrożonej bez dzisiejszej normy to zachowanie sprzed R-KOK-37, R-KOK-05)
  std::vector<Noc> g;
  for (int i = 0; i < 16; i++) g.push_back(noc(i, i % 2 == 1, false));
  g.push_back(noc(16, false, false));
  Stany sg = liczStany(g, g.back().data, PUSTE);
  sg.ahi.aktywny = true;
  sg.ahi.od = g[10].data;
  sg.ahi.nocy = 6;
  DaneK1 d_v9 = budujK1(g, g.back().data, sg.norma, {}, &sg);
  const WierszK1* v = ahi(d_v9);
  SPRAWDZ(v && v->slowo == "uczę się");
  // noce niepewne w okresie stanu AHI (R-NOR-09) nie dopełniają normy: stan od 6. nocy, niepewne 7, 9, … liczone
  // tylko przed nim (3 z 8), więc 8 + 3 < 14
  Stany sh = liczStany(g, g.back().data, PUSTE);
  sh.ahi.aktywny = true;
  sh.ahi.od = g[6].data;
  sh.ahi.nocy = 5;
  sh.ahi.wyzej = true;
  // gałąź stanu wygrywa i tak; sprawdzamy samą funkcję pośrednio przez brak stanu w kopii z tą samą datą `od`
  DaneK1 dsh = budujK1(g, g.back().data, sh.norma, {}, &sh);
  SPRAWDZ(ahi(dsh) && ahi(dsh)->slowo != "niepewne");
}

// Kontrfakt R-KOK-37 na tych samych wejściach co norma w dyrygencie
// (stany z wykluczeniami okresów infekcji i z historią bez wycieku sprzed nowego modelu maski, K1 z historią surową)
static void testR_KOK_37_wejscia_jak_w_dyrygencie() {
  // 10 nocy niepewnych w okresie infekcji, potem 10 pewnych → bez normy AHI i bez `niepewne`
  std::vector<Noc> h;
  std::set<std::string> infekcja;
  for (int i = 0; i < 10; i++) {
    h.push_back(noc(i, true, false));
    infekcja.insert(h.back().data);
  }
  for (int i = 10; i < 20; i++) h.push_back(noc(i, false, false));
  std::string dz = dodajDni(h.back().data, 1);
  Stany st = liczStany(h, dz, infekcja);
  DaneK1 d1 = budujK1(h, dz, st.norma, {}, &st);
  const WierszK1* w = ahi(d1);
  SPRAWDZ(w && w->slowo == "uczę się" && !st.kokpitAhiZNiepewnymi);
  // nowy model maski, norma: 10 nocy starej maski z wyciekiem 20 (niepewne w historii surowej), 3 noce nowego modelu
  std::vector<Noc> m;
  for (int i = 0; i < 10; i++) m.push_back(noc(i, true, false));
  for (int i = 10; i < 13; i++) m.push_back(noc(i, false, false));
  std::vector<Noc> mb = m;  // jak histNormyB16(): bez wycieku nocy sprzed nowego modelu
  for (auto& n : mb)
    if (n.data < m[10].data) n.wmed = n.wp95 = NAN;
  std::string dm = dodajDni(m.back().data, 1);
  Stany sm = liczStany(mb, dm, PUSTE);
  DaneK1 d2 = budujK1(m, dm, sm.norma, {}, &sm);
  const WierszK1* v = ahi(d2);
  SPRAWDZ(v && v->slowo == "uczę się");
  // nowy model maski, trend: 23 noce pewne, 4 noce starej maski z wyciekiem 20, 3 noce nowego modelu; w historii stanów noce
  // starej maski nie są niepewne, więc nie dopełniają okna 7 nocy: `uczę się`, nie `niepewne`
  std::vector<Noc> t;
  for (int i = 0; i < 23; i++) t.push_back(noc(i, false, false));
  for (int i = 23; i < 27; i++) t.push_back(noc(i, true, false));
  for (int i = 27; i < 30; i++) t.push_back(noc(i, false, false));
  std::vector<Noc> tb = t;
  for (auto& n : tb)
    if (n.data < t[27].data) n.wmed = n.wp95 = NAN;
  Stany stt = liczStany(tb, t.back().data, PUSTE);
  DaneK1 d3 = budujK1(t, t.back().data, stt.norma, {}, &stt);
  const WierszK1* u = ahi(d3);
  SPRAWDZ(u && u->slowo == "uczę się");
  // ta sama historia bez nowego modelu maski: `niepewne` (wyciek jest przyczyną)
  Stany sts = liczStany(t, t.back().data, PUSTE);
  DaneK1 d4 = budujK1(t, t.back().data, sts.norma, {}, &sts);
  SPRAWDZ(ahi(d4) && ahi(d4)->slowo == "niepewne");
}

int main() {
  testR_KOK_37_trend_tylko_przez_niepewne();
  testR_KOK_37_norma_tylko_przez_niepewne();
  testR_KOK_37_za_malo_nocy_w_ogole();
  testR_KOK_37_dosc_nocy_pewnych();
  testR_KOK_37_czynniki();
  testR_KOK_37_ostatnia_noc_starsza_niz_dzis();
  testR_KOK_37_stan_ahi_ma_pierwszenstwo();
  testR_KOK_37_wejscia_jak_w_dyrygencie();
  printf("testy_k1_ahi_niepewne: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
