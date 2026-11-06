// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (podpisy osi S3, ocena częściowa, trend PS w K1, tytuły L-07 i O-09). Każdy test
// pada z numerem R17-nn, dopóki usterka nie zostanie poprawiona.
// TRZYMA = zachowanie sprawdzone przez przegląd. W odróżnieniu od test/testy_trend_ps_podpisy_osi.cpp podpisy osi S3
// liczone są z prawdziwego silnika reguł na syntetycznej historii (nie ze zmyślonych zmiennych tekstu).
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "dzien.h"
#include "ekrany.h"
#include "ekrany_zdarzen.h"
#include "kokpit.h"
#include "noc.h"
#include "reguly.h"
#include "sprawy.h"
#include "stany.h"
#include "wspolne.h"
#include "zdarzenia_ekrany_e.h"

static int ok = 0, zle = 0;
#define PRZYPADEK(nr, w)                                                              \
  do {                                                                                 \
    if (w) ok++;                                                                       \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); } \
  } while (0)
#define TRZYMA(w) PRZYPADEK("(trzyma)", w)

using std::string;
using std::vector;

static string REPO = "../..";  // nadpisywane argumentem (katalog repozytorium)

// 90 nocy ważnych z drganiami; ostatnie `nowe` nocy zmienia `f` (wejście w stan odchylenia)
static vector<Noc> historia(const string& tryb, const std::function<void(Noc&)>& f, int nowe = 10) {
  vector<Noc> h;
  for (int i = 0; i < 90; i++) {
    Noc n;
    n.data = dodajDni("2026-08-07", i);
    n.wazna = true;
    double j = (i % 5) * 0.1;
    n.m01 = 420 + (i % 5) * 6;
    n.ahi = (float)(1.0 + j);
    n.cmed = 9;
    n.cp95 = (float)(10 + j);
    n.wmed = 4;
    n.wp95 = (float)(10 + 2 * j);
    n.emed = 7;
    n.ep95 = (float)(8 + j);
    n.psmed = 5;
    n.psp95 = 6;
    n.oa = 0.3f;
    n.ca = (float)(0.5 + j);
    n.h = 0.3f;
    n.m13 = (float)(15 + j);
    n.m17 = 1;
    n.m08 = 2;
    n.ustawienia = tryb == "ASVAuto" ? "ASVAuto/6/10/3/15" : tryb == "CPAP" ? "CPAP/10" : "AutoSet/5/12";
    n.tryb = tryb;
    if (i >= 90 - nowe) f(n);
    h.push_back(n);
  }
  return h;
}

struct Ekran {
  bool jest = false;
  sprawy::SprawaL s;
  DaneWzorca w;
};

// Silnik reguł → sprawy → pełny ekran S3 zdarzenia `id` (jak dyrygent: ocen(), sprawyOtwarte(), trescZdarzenia())
static Ekran ekranE(const string& id, const string& tryb, const std::function<void(Noc&)>& f) {
  static vector<Noc> h;
  static Stany st;
  h = historia(tryb, f);
  string dzis = h.back().data;
  const reguly::Zestaw& z = b14::zestaw();
  st = liczStany(h, dzis, {});
  reguly::Kontekst k;
  k.historia = &h;
  k.dzis = dzis;
  k.norma = &st.norma;
  k.stany = &st;
  k.zestaw = &z;
  k.trybBiezacy = reguly::trybZUstawien(h.back().ustawienia);
  vector<reguly::Wynik> w = reguly::ocen(z, k);
  reguly::Pamiec p;
  Ekran e;
  for (auto& s : sprawy::sprawyOtwarte(z, w, p, b14::funkcjaTekstu(k.trybBiezacy))) {
    if (s.id != id || s.wykluczona) continue;
    DaneDnia d;
    d.dzis = dzis;
    d.hist = &h;
    d.stany = &st;
    d.norma = st.norma;
    e.jest = true;
    e.s = s;
    e.w = zdarzenia::trescZdarzenia(s, d);
    break;
  }
  return e;
}

static string poZwykle(const string& p) {
  size_t i = p.find("zwykle ");
  return i == string::npos ? "" : p.substr(i + 7);
}

// ---------- pkt 1: podpis prawy = linia, którą wykres naprawdę rysuje; 30 nocy = 30 punktów ----------
static void testPodpisyZSilnika() {
  struct Przypadek {
    const char* id;
    const char* tryb;
    std::function<void(Noc&)> f;
  };
  vector<Przypadek> pr = {
      {"E-31", "AutoSet", [](Noc& n) { n.ahi = 4.6f; }},
      {"E-31", "ASVAuto", [](Noc& n) { n.ahi = 4.6f; }},
      {"E-33", "ASVAuto", [](Noc& n) { n.ca = 4.0f; }},
      {"E-36", "AutoSet", [](Noc& n) { n.cp95 = 13.5f; }},
      {"E-36", "CPAP", [](Noc& n) { n.cp95 = 13.5f; }},
      {"E-36", "ASVAuto", [](Noc& n) { n.ep95 = 11.5f; }},
      {"E-21", "AutoSet", [](Noc& n) { n.wp95 = 23; }},
  };
  for (auto& x : pr) {
    Ekran e = ekranE(x.id, x.tryb, x.f);
    TRZYMA(e.jest);
    if (!e.jest) {
      printf("    %s %s: brak ekranu S3 w scenariuszu\n", x.id, x.tryb);
      continue;
    }
    TRZYMA(e.w.wartosci.size() == 30);  // podpis `· 30 nocy`
    TRZYMA(e.w.podpisLewo.find("30 nocy") != string::npos);
    // linia przerywana to mediana normy, a podpis podaje tę samą wartość (ta sama norma, także zamrożona w E-36)
    TRZYMA(poZwykle(e.w.podpisPrawo) == przecinek1(e.w.prog));
    if (poZwykle(e.w.podpisPrawo) != przecinek1(e.w.prog))
      printf("    %s %s: podpis „%s”, linia %s\n", x.id, x.tryb, e.w.podpisPrawo.c_str(), przecinek1(e.w.prog).c_str());
  }
  // E-36: słowo sygnału trybu w podpisie zgodne z metryką wykresu (4.11)
  TRZYMA(ekranE("E-36", "AutoSet", [](Noc& n) { n.cp95 = 13.5f; }).w.podpisLewo.compare(0, 10, "Ciśnienie") == 0);
  TRZYMA(ekranE("E-36", "ASVAuto", [](Noc& n) { n.ep95 = 11.5f; }).w.podpisLewo.compare(0, 4, "EPAP") == 0);
}

// ---------- E-13 z prawdziwego silnika: norma czasu z maską w minutach z przecinkiem, bez jednostki ----------
// Test (testy_trend_ps_podpisy_osi.cpp) podstawia zmyśloną zmienną `{norma}` = `7 h 20`; silnik (rStanOdchylenia)
// daje przecinek1(mediana M-01 w minutach) = `432,0`. Podpis `linia przerywana = zwykle 432,0` nie mówi pacjentowi,
// że linia to czas z maską, a powód `Średnio 280,0, zwykle 432,0 (ostatnie 5 nocy).` jest tak samo nieczytelny.
static void testR17_01() {
  Ekran e = ekranE("E-13", "AutoSet", [](Noc& n) { n.m01 = 280; });
  TRZYMA(e.jest);
  if (!e.jest) return;
  printf("  E-13 podpis „%s”, powód „%s”\n", e.w.podpisPrawo.c_str(), e.s.powod.c_str());
  PRZYPADEK("E-13 z prawdziwego silnika", e.w.podpisPrawo.find(" h") != string::npos);  // np. `zwykle 7 h 12` jak w 6.4 i teście
  PRZYPADEK("E-13 z prawdziwego silnika", e.s.powod.find(",0") == string::npos);
}

// ---------- E-14 liczba przerw z przecinkiem (`zwykle 1,0`) ----------
static void testR17_02() {
  Ekran e = ekranE("E-14", "AutoSet", [](Noc& n) { n.m17 = 6; });
  TRZYMA(e.jest);
  if (!e.jest) return;
  printf("  E-14 podpis „%s”, powód „%s”\n", e.w.podpisPrawo.c_str(), e.s.powod.c_str());
  PRZYPADEK("E-14 liczba przerw z przecinkiem", e.w.podpisPrawo.find(",0") == string::npos);
}

// ---------- podgląd „4 wiersze” nie pokazuje K1 trybu CPAP/AutoSet ----------
// podglad.cpp obcina 5-wierszowy K1 ASV do 4 wierszy (zostają EPAP i Wsparcie, znika Wyciek), a komentarz mówi
// „CPAP, AutoSet … wiersze 46 px, ocena dalej w dwóch liniach” (faktycznie 45 px i jedna linia). Tryb egzemplarza testowego
// (AutoSet) nie ma prawdziwego podglądu K1 z odkryciem.
static void testR17_03() {
  std::ifstream f(REPO + "/firmware/prototyp/test/podglad.cpp");
  std::stringstream ss;
  ss << f.rdbuf();
  string s = ss.str();
  size_t i = s.find("b17-k1-odkrycie-4-wiersze");
  TRZYMA(i != string::npos);
  string blok = i == string::npos ? "" : s.substr(i > 600 ? i - 600 : 0, 600);
  PRZYPADEK("podgląd „4 wiersze” nie pokazuje K1 trybu CPAP/AutoSet", blok.find("k1.wierszy = 4;") == string::npos);
  // prawdziwy K1 AutoSet: 4 wiersze, ostatni Wyciek (R-KOK-15)
  vector<Noc> h = historia("AutoSet", [](Noc&) {});
  Stany st = liczStany(h, h.back().data, {});
  DaneK1 k = budujK1(h, h.back().data, st.norma, {}, &st);
  TRZYMA(k.wierszy == 4 && string(k.w[3].nazwa) == "Wyciek");
}

// ---------- K1 z odkryciem: ocena w jednej linii przy długim stanie odchylenia (trzyma, zapas 4 px) ----------
// Przy wierszu < 46 px ocena idzie jedną linią P22B od x = 486 do prawego marginesu (788): 302 px.
static void testJednaLinia() {
  const int miejsce = 788 - (470 + 16);
  TRZYMA(szer(F_P22B, "wyżej niż zwykle od 3 nocy") <= miejsce);
  int w3 = szer(F_P22B, "wyżej niż zwykle od 120 nocy");
  printf("  K1 z odkryciem: `wyżej niż zwykle od 120 nocy` %d px przy %d px miejsca\n", w3, miejsce);
  TRZYMA(w3 <= miejsce);
  TRZYMA(szer(F_P22B, "stabilnie · wyciek widoczny") <= miejsce);
}

// ---------- pkt 2: „Ocena: częściowa”; „pełna” tylko przy pewnej pełnej ocenie ----------
static void testOcena() {
  sprawy::WejscieSpraw ws;
  ws.dzis = "2026-11-05";
  TRZYMA(sprawy::kartaPusta(ws).ocena == "częściowa");
  TRZYMA(sprawy::kartaPusta(ws).dopisek == "Część reguł nie ma jeszcze danych.");
  ws.ostatniaNiepewna = true;  // R-EKR-50: noc niepewna ma pierwszeństwo, nigdy `pełna`
  TRZYMA(sprawy::kartaPusta(ws).ocena != "pełna");
}

// ---------- pkt 12: trend PS tylko w ASV; w AutoSet nie ma wiersza Wsparcie ----------
static void testTrendPs() {
  vector<Noc> h = historia("AutoSet", [](Noc& n) { n.psmed = 9; });
  Stany st = liczStany(h, h.back().data, {});
  DaneK1 k = budujK1(h, h.back().data, st.norma, {}, &st);
  bool ps = false;
  for (int i = 0; i < k.wierszy; i++) ps = ps || string(k.w[i].nazwa) == "Wsparcie";
  TRZYMA(!ps);
  h = historia("ASVAuto", [](Noc& n) { n.psmed = 9; n.psp95 = NAN; });
  st = liczStany(h, h.back().data, {});
  k = budujK1(h, h.back().data, st.norma, {}, &st);
  string sl;
  for (int i = 0; i < k.wierszy; i++)
    if (string(k.w[i].nazwa) == "Wsparcie") sl = k.w[i].slowo;
  TRZYMA(sl == "rośnie");  // 23 noce 5, 7 nocy 9 (przy 10 nowych nocach mediana 7 ostatnich = 9)
  // PS bez danych (np. noce z AirBridge bez PS): `uczę się`, nie `stabilnie`
  h = historia("ASVAuto", [](Noc&) {});
  for (auto& n : h) n.psmed = NAN;
  st = liczStany(h, h.back().data, {});
  k = budujK1(h, h.back().data, st.norma, {}, &st);
  for (int i = 0; i < k.wierszy; i++)
    if (string(k.w[i].nazwa) == "Wsparcie") sl = k.w[i].slowo;
  TRZYMA(sl == "uczę się");
}

// ---------- dokumentacja aktualna ----------
static string plik(const string& p) {
  std::ifstream f(REPO + "/" + p);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
static void testDokumentacja() {
  string fw = plik("firmware/prototyp/README.md");
  TRZYMA(!fw.empty());
  PRZYPADEK("testDokumentacja", fw.find("wiersz `Wsparcie` bez reguły") == string::npos);
  string tech = plik("docs/spec/specyfikacja-techniczna.md");
  TRZYMA(!tech.empty());
  PRZYPADEK("testDokumentacja", tech.find("testy_trend_ps_podpisy_osi.cpp") != string::npos);
}

int main(int argc, char** argv) {
  if (argc > 1) REPO = argv[1];
  testPodpisyZSilnika();
  testR17_01();
  testR17_02();
  testR17_03();
  testJednaLinia();
  testOcena();
  testTrendPs();
  testDokumentacja();
  printf("testy_trend_ps_podpisy_osi_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
