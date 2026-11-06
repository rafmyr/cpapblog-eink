// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (czynniki nocy, odkrycia O-03…O-12 z testem permutacyjnym, S7, S8, „Co znaczą te
// liczby”). Każdy test pada z numerem R15-nn, dopóki usterka nie zostanie poprawiona.
// Testy „trzyma” (SPRAWDZ) potwierdzają przypadki, w których odkrycie
// NIE powinno się pojawić. Dane wyłącznie syntetyczne.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <set>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "czynniki.h"
#include "dyrygent.h"
#include "ekrany.h"
#include "kokpit.h"
#include "noc.h"
#include "odkrycia.h"
#include "reguly.h"
#include "sprawy.h"
#include "wspolne.h"
#include "wybudzenie.h"
#include "zestaw_kolejki.h"

static int ok = 0, zle = 0, trzyma = 0;
#define PRZYPADEK(nr, w)                                                                  \
  do {                                                                                     \
    if (w) ok++;                                                                           \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); }     \
  } while (0)
#define SPRAWDZ(w)                                                                         \
  do {                                                                                     \
    if (w) trzyma++;                                                                       \
    else { zle++; printf("BŁĄD (trzyma?) %s:%d: %s\n", __FILE__, __LINE__, #w); }         \
  } while (0)

using namespace reguly;
static const char* const U = "ASVAuto/6/10/3/15";

static Noc noc(const std::string& d, double ahi, int m01 = 420, double wp95 = 8, double ca = 0.5) {
  Noc n;
  n.data = d;
  n.wazna = m01 >= 240;
  n.m01 = m01;
  n.ahi = (float)ahi;
  n.wp95 = (float)wp95;
  n.wmed = 4;
  n.ca = (float)ca;
  n.ustawienia = U;
  return n;
}
static Kontekst kontekst(const std::vector<Noc>& h, const std::string& dzis) {
  Kontekst k;
  k.historia = &h;
  k.dzis = dzis;
  k.trybBiezacy = Tryb::ASVAUTO;
  k.zestaw = &zestawWbudowany();
  return k;
}
static const Wynik* znajdz(const std::vector<Wynik>& w, const std::string& kl) {
  for (auto& x : w)
    if (odkrycia::klucz(x) == kl) return &x;
  return nullptr;
}
static void dodaj(std::vector<Noc>& h, std::vector<WpisCzynnikow>& v, std::string& d, int ile, char war, uint8_t c,
                  double ahi, double ca = 0.5) {
  for (int i = 0; i < ile; i++) {
    h.push_back(noc(d, ahi, 420, 8, ca));
    WpisCzynnikow w;
    w.data = d;
    w.stan = 'O';
    w.czynniki = c;
    w.warstwa = war;
    w.pokazano = true;
    v.push_back(w);
    d = dodajDni(d, 1);
  }
}
static std::vector<Wynik> przeglad(const std::vector<Noc>& h, const std::vector<WpisCzynnikow>& v,
                                   const std::set<std::string>& pokazane = {}, const std::string& dzis = "") {
  odkrycia::CechyNocy c;
  c.tryb = odkrycia::TrybOceny::PRZEGLAD;
  c.czynniki = &v;
  c.pokazane = pokazane;
  return odkrycia::ocenOdkrycia(zestawWbudowany(), kontekst(h, dzis.empty() ? h.back().data : dzis), c);
}
static int spelnioneO(const std::vector<Wynik>& w, const char* prefiks = "O-") {
  int n = 0;
  for (auto& x : w)
    if (x.oceniona && x.spelniona && x.id.compare(0, std::string(prefiks).size(), prefiks) == 0) n++;
  return n;
}
// prosty generator (wynik powtarzalny, bez zależności od biblioteki)
struct Los {
  uint32_t s;
  explicit Los(uint32_t z) : s(z) {}
  uint32_t operator()() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
  double jedn() { return (*this)() / 32768.0; }
};

// ---------- K1 z odkryciem chowa 4 liczby ostatniej nocy ----------
struct PlotnoH : Plotno {
  uint64_t h = 1469598103934665603ull;
  void piksel(int x, int y, bool c) override {
    if (!c) return;
    h ^= (uint64_t)(x * 1000 + y);
    h *= 1099511628211ull;
  }
};
static void testK1LiczbyNocy() {
  std::vector<Noc> h;
  std::string d = "2026-10-01";
  for (int i = 0; i < 40; i++, d = dodajDni(d, 1)) h.push_back(noc(d, 1.0));
  DaneK1 k1 = budujK1(h, h.back().data, liczNorme(h, h.back().data, {}), {}, nullptr);
  k1.odkrycieTytul = "Po alkoholu: wyższe AHI";
  k1.odkryciePowod = "AHI 4,1 / h po alkoholu, 1,8 / h bez. Z 6/41 nocy.";
  PlotnoH a, b;
  rysujK1(a, k1);
  k1.liczby[1] = "9,9";  // AHI ostatniej nocy inne: gdyby liczby były na ekranie, obraz by się zmienił
  k1.liczby[0] = "1 h 05";
  rysujK1(b, k1);
  // 5.5.2: „pod nagłówkiem pełny tytuł i powód”, nie „zamiast 4 liczb”; liczby ostatniej nocy nie mogą zniknąć
  PRZYPADEK("K1 z odkryciem chowa 4 liczby ostatniej nocy", a.h != b.h);
}

// ---------- O-03 bez Δ w wyniku, więc R-EKR-27 (30 dni) go nie zatrzymuje ----------
static void testO03Powrot() {
  std::vector<Noc> h;
  std::string d = "2026-09-01";
  for (int i = 0; i < 40; i++, d = dodajDni(d, 1)) h.push_back(noc(d, 3.0));
  for (int i = 0; i < 7; i++, d = dodajDni(d, 1)) h.push_back(noc(d, 1.0));
  odkrycia::CechyNocy c;
  std::vector<Wynik> w = odkrycia::ocenOdkrycia(zestawWbudowany(), kontekst(h, h.back().data), c);
  const Wynik* o3 = znajdz(w, "O-03/M-02");
  SPRAWDZ(o3 && o3->spelniona);
  if (!o3) return;
  std::vector<Wynik> dz = {*o3};
  // pokazane 8 dni temu z tą samą wartością (1,0): limit 7 dni minął, 30 dni nie (Δ AHI = 3 / h)
  std::vector<odkrycia::Pokazanie> pok = {{"O-03/M-02", dodajDni(h.back().data, -8), 1.0}};
  odkrycia::WejscieWyboru ww;
  ww.dzis = h.back().data;
  ww.dzienne = &dz;
  ww.pokazane = &pok;
  ww.zestaw = &zestawWbudowany();
  std::string powod;
  const Wynik* x = odkrycia::wybierz(ww, powod);
  printf("  delta_num O-03 = \"%s\", wybór: %s (%s)\n",
         o3->zmienne.count("delta_num") ? o3->zmienne.at("delta_num").c_str() : "brak", x ? x->id.c_str() : "brak",
         powod.c_str());
  PRZYPADEK("O-03 bez Δ w wyniku, więc R-EKR-27", x == nullptr);
}

// ---------- O-12 na CA/h (M-03ca) przed zaliczeniem ----------
static void testO12Centralne() {
  std::vector<Noc> h;
  std::vector<WpisCzynnikow> v;
  std::string d = "2026-05-01";
  dodaj(h, v, d, 6, 'O', CZ_LEKI, 1.5, 4.0);
  dodaj(h, v, d, 4, 'K', CZ_LEKI, 1.5, 4.0);
  dodaj(h, v, d, 10, 'O', CZ_NIC, 1.5, 0.5);
  dodaj(h, v, d, 35, 'K', CZ_NIC, 1.5, 0.5);
  std::vector<Wynik> w = przeglad(h, v);
  const Wynik* a = znajdz(w, "O-12/L.M-03ca");
  if (a && a->spelniona) {
    odkrycia::UstawieniaTekstow u;
    u.czynnikiWrazliwe = true;
    odkrycia::TrescOdkrycia t = odkrycia::trescOdkrycia(zestawWbudowany(), *a, Tryb::ASVAUTO, u);
    printf("  %s / %s / %s\n", t.tytul.c_str(), t.powod.c_str(), t.krok.c_str());
  }
  // Wariant B: O-12 zostaje także dla CA/h,
  // mimo że do zaliczenia S8, E-33 i L-01 nie dają kroku do lekarza. Test potwierdza decyzję:
  // O-12 na CA/h jest oceniane i spełnione, przy lekach nasennych z krokiem do lekarza (6.5 O-12).
  SPRAWDZ(a && a->oceniona && a->spelniona);
  if (a && a->spelniona) {
    odkrycia::UstawieniaTekstow u;
    u.czynnikiWrazliwe = true;
    SPRAWDZ(odkrycia::trescOdkrycia(zestawWbudowany(), *a, Tryb::ASVAUTO, u).krok == "→ Powiedz o tym lekarzowi.");
  }
}

// ---------- test permutacyjny O-12 bez warstw (R-NF-24: przetasowanie osobno w każdej warstwie) ----------
static void testO12Warstwy() {
  // W każdej warstwie wartości są stałe (O: 6,0, K: 1,5) niezależnie od czynnika: przetasowanie etykiet w warstwie
  // nie zmienia statystyki, więc test warstwowy daje p = 1 i O-12 nie przechodzi. Różnica median ważonych wynika
  // tylko ze składu warstw grupy „alkohol” (12 nocy O, 1 noc K).
  std::vector<Noc> h;
  std::vector<WpisCzynnikow> v;
  std::string d = "2026-05-01";
  dodaj(h, v, d, 12, 'O', CZ_ALKOHOL, 6.0);
  dodaj(h, v, d, 1, 'K', CZ_ALKOHOL, 1.5);
  dodaj(h, v, d, 10, 'O', CZ_NIC, 6.0);
  dodaj(h, v, d, 35, 'K', CZ_NIC, 1.5);
  std::vector<Wynik> w = przeglad(h, v);
  const Wynik* a = znajdz(w, "O-12/A.M-02");
  printf("  O-12/A.M-02 oceniona %d, spełniona %d, p = %s\n", a ? a->oceniona : -1, a ? a->spelniona : -1,
         a && a->zmienne.count("p") ? a->zmienne.at("p").c_str() : "-");
  PRZYPADEK("test permutacyjny O-12 bez warstw", !(a && a->spelniona));
}

// ---------- trzyma: cotygodniowe przeglądy rosnących danych, odsetek fałszywych odkryć (R-NF-24, 9.3) ----------
static void testPrzegladyTygodniowe() {
  const int PRZEBIEGOW = 10, TYGODNI = 16;
  int zFalszywym = 0, przegladow = 0;
  double msSuma = 0, msMax = 0;
  for (int p = 0; p < PRZEBIEGOW; p++) {
    Los los(1000u + (uint32_t)p * 7919u);
    std::vector<Noc> h;
    std::vector<WpisCzynnikow> v;
    std::string d = "2026-01-04";  // niedziela
    std::set<std::string> pokazane;
    bool falsz = false;
    for (int t = 0; t < TYGODNI; t++) {
      for (int i = 0; i < 7; i++) {
        // wszystkie metryki losowe, bez związku z czynnikiem ani warstwą (czynnik nic nie zmienia)
        Noc n = noc(d, 0.5 + 4.0 * los.jedn(), 300 + (int)(180 * los.jedn()), 4 + 20 * los.jedn(), 2.0 * los.jedn());
        h.push_back(n);
        WpisCzynnikow w;
        w.data = d;
        w.stan = 'O';
        w.pokazano = true;
        w.warstwa = los() % 4 == 0 ? 'O' : 'K';
        uint32_t r = los() % 6;
        w.czynniki = (uint8_t)(r == 0 ? CZ_ALKOHOL : r == 1 ? CZ_STRES : r == 2 ? CZ_KOLACJA : CZ_NIC);
        v.push_back(w);
        d = dodajDni(d, 1);
      }
      if (t < 6) continue;  // przeglądy od 7. tygodnia (grupy mają już po kilkanaście nocy)
      auto t0 = std::chrono::steady_clock::now();
      std::vector<Wynik> wy = przeglad(h, v, pokazane, h.back().data);
      double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      msSuma += ms;
      msMax = std::max(msMax, ms);
      przegladow++;
      for (auto& x : wy)
        if (x.oceniona && x.spelniona && x.id == "O-12") {
          falsz = true;
          pokazane.insert(odkrycia::klucz(x));  // para pokazana wypada z kolejnych przeglądów (9.3)
        }
    }
    zFalszywym += falsz;
  }
  printf("  trzyma (R-NF-24): przebiegi z co najmniej jednym fałszywym O-12 w %d przeglądach: %d z %d; przegląd %.0f ms średnio,"
         " %.0f ms najdłużej (PC)\n",
         TYGODNI - 6, zFalszywym, PRZEBIEGOW, msSuma / std::max(1, przegladow), msMax);
  // R-NF-24: kontrola błędu w kolejnych przeglądach; „para raz” + Holm + wagi dają w symulacji 1 z 10 (16 tygodni),
  // a w przebiegu 30 × 32 tygodnie 1 z 30
  SPRAWDZ(zFalszywym * 10 <= PRZEBIEGOW);  // najwyżej 10% przebiegów z fałszywym odkryciem
}

// ---------- koszt przeglądu w najgorszym przypadku (9.3 „koszt obliczeń”), pomiar na PC ----------
static void testKosztPrzegladu() {
  Los los(4242u);
  std::vector<Noc> h;
  std::vector<WpisCzynnikow> v;
  std::string d = "2025-11-02";
  const uint8_t CZ[] = {CZ_ALKOHOL, CZ_KATAR, CZ_STRES, CZ_KOLACJA, CZ_LEKI, CZ_THC, CZ_POZA_DOMEM};
  for (int i = 0; i < 365; i++, d = dodajDni(d, 1)) {  // rok odpowiedzi: wszystkie 7 czynników × 4 metryki oceniane
    h.push_back(noc(d, 0.5 + 4.0 * los.jedn(), 300 + (int)(180 * los.jedn()), 4 + 20 * los.jedn(), 2.0 * los.jedn()));
    WpisCzynnikow w;
    w.data = d;
    w.stan = 'O';
    w.pokazano = true;
    w.warstwa = los() % 3 == 0 ? 'O' : 'K';
    uint32_t r = los() % 10;
    w.czynniki = r < 7 ? CZ[r] : (uint8_t)CZ_NIC;
    v.push_back(w);
  }
  auto t0 = std::chrono::steady_clock::now();
  std::vector<Wynik> wy = przeglad(h, v);
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  int por = 0, nocyMax = 0;
  for (auto& x : wy) {
    por += x.zmienne.count("p") ? 1 : 0;
    if (x.id == "O-12" && x.oceniona) nocyMax = std::max(nocyMax, std::atoi(x.zmienne.at("n").c_str()) + std::atoi(x.zmienne.at("m").c_str()));
  }
  printf("  przegląd roku odpowiedzi: %d porównań z testem, O-12 do %d nocy w parze, %.0f ms na PC (-O1); ESP32-S3 240 MHz ok. 10–30× "
         "wolniej [szacunek], w porannym wybudzeniu przed rysowaniem ekranu\n", por, nocyMax, ms);
  // 9.3 liczy koszt „do 90 nocy”; O-12 bierze całą historię odpowiedzi (probki(k, m, 0)), więc koszt i wiek nocy
  // rosną bez granicy (wagi r z 90 dni stosowane do nocy sprzed roku)
  PRZYPADEK("koszt przeglądu w najgorszym przypadku", nocyMax <= 90);
}

// ---------- trzyma: przypadki, w których odkrycie NIE powinno się pojawić ----------
static void testBezOdkrycia() {
  // (a) szum: O-08 (weekend) na losowym AHI, 20 archiwów po 90 nocy → fałszywe O-08 najwyżej w 2 z 20
  int o08 = 0;
  for (int p = 0; p < 20; p++) {
    Los los(77u + (uint32_t)p);
    std::vector<Noc> h;
    std::string d = "2026-02-01";
    for (int i = 0; i < 90; i++, d = dodajDni(d, 1)) h.push_back(noc(d, 0.5 + 9.0 * los.jedn()));
    std::vector<WpisCzynnikow> v;
    std::vector<Wynik> w = przeglad(h, v);
    const Wynik* x = znajdz(w, "O-08/M-02");
    o08 += x && x->spelniona;
  }
  printf("  trzyma: O-08 na szumie (AHI 0,5…9,5): %d z 20\n", o08);
  SPRAWDZ(o08 <= 2);
  // (b) mało nocy: 4 noce po alkoholu (2 O, 2 K) z AHI 9 → brak O-12 (R-EKR-26: ≥ 5 nocy w grupie)
  {
    std::vector<Noc> h;
    std::vector<WpisCzynnikow> v;
    std::string d = "2026-05-01";
    dodaj(h, v, d, 2, 'O', CZ_ALKOHOL, 9.0);
    dodaj(h, v, d, 2, 'K', CZ_ALKOHOL, 9.0);
    dodaj(h, v, d, 10, 'O', CZ_NIC, 1.5);
    dodaj(h, v, d, 35, 'K', CZ_NIC, 1.5);
    std::vector<Wynik> wy_a = przeglad(h, v);  // bez wskaźnika do tymczasowego wektora (ASan)
    const Wynik* a = znajdz(wy_a, "O-12/A.M-02");
    SPRAWDZ(a && !a->spelniona);
  }
  // (c) jedna skrajna noc: alkohol 9 nocy O z AHI 1,5 i 1 noc K z AHI 30 (waga 7) → brak O-12
  {
    std::vector<Noc> h;
    std::vector<WpisCzynnikow> v;
    std::string d = "2026-05-01";
    dodaj(h, v, d, 9, 'O', CZ_ALKOHOL, 1.5);
    dodaj(h, v, d, 1, 'K', CZ_ALKOHOL, 30.0);
    dodaj(h, v, d, 10, 'O', CZ_NIC, 1.5);
    dodaj(h, v, d, 35, 'K', CZ_NIC, 1.5);
    std::vector<Wynik> wy_a = przeglad(h, v);  // bez wskaźnika do tymczasowego wektora (ASan)
    const Wynik* a = znajdz(wy_a, "O-12/A.M-02");
    SPRAWDZ(a && !a->spelniona);
  }
  // (d) jedna skrajna noc weekendowa (AHI 40) przy zwykłych nocach → brak O-08
  {
    std::vector<Noc> h;
    std::string d = "2026-02-01";
    bool jedna = false;
    for (int i = 0; i < 90; i++, d = dodajDni(d, 1)) {
      bool skrajna = !jedna && i > 40 && dzienTygodnia(d) == 6;  // jedna sobota z AHI 40
      jedna = jedna || skrajna;
      h.push_back(noc(d, skrajna ? 40 : 1.5 + 0.1 * (i % 5)));
    }
    std::vector<WpisCzynnikow> v;
    std::vector<Wynik> wy_x = przeglad(h, v);  // bez wskaźnika do tymczasowego wektora (ASan)
    const Wynik* x = znajdz(wy_x, "O-08/M-02");
    SPRAWDZ(x && !x->spelniona);
  }
  // (e) noce bez odpowiedzi (stan B) i oznaczenie automatyczne A: nie wchodzą do żadnej grupy (R-CZY-04, R-CZY-19)
  {
    std::vector<Noc> h;
    std::vector<WpisCzynnikow> v;
    std::string d = "2026-05-01";
    dodaj(h, v, d, 6, 'O', CZ_POZA_DOMEM, 9.0);
    dodaj(h, v, d, 6, 'K', CZ_POZA_DOMEM, 9.0);
    for (auto& x : v) x.warstwa = 'A', x.pokazano = false;
    dodaj(h, v, d, 10, 'O', CZ_NIC, 1.5);
    dodaj(h, v, d, 35, 'K', CZ_NIC, 1.5);
    std::vector<Wynik> wy_a = przeglad(h, v);  // bez wskaźnika do tymczasowego wektora (ASan)
    const Wynik* a = znajdz(wy_a, "O-12/P.M-02");
    SPRAWDZ(a && !a->oceniona);
  }
  // (f) rozruch globalny i dzienny tryb: porównania grup tylko w przeglądzie
  {
    std::vector<Noc> h;
    std::vector<WpisCzynnikow> v;
    std::string d = "2026-05-01";
    dodaj(h, v, d, 6, 'O', CZ_ALKOHOL, 9.0);
    dodaj(h, v, d, 6, 'K', CZ_ALKOHOL, 9.0);
    dodaj(h, v, d, 10, 'O', CZ_NIC, 1.5);
    dodaj(h, v, d, 35, 'K', CZ_NIC, 1.5);
    odkrycia::CechyNocy c;
    c.czynniki = &v;
    SPRAWDZ(spelnioneO(odkrycia::ocenOdkrycia(zestawWbudowany(), kontekst(h, h.back().data), c), "O-12") == 0);
  }
  // (g) powtarzalność: ten sam przegląd dwa razy → te same wartości p (ziarno z dnia i klucza)
  {
    std::vector<Noc> h;
    std::vector<WpisCzynnikow> v;
    std::string d = "2026-05-01";
    dodaj(h, v, d, 6, 'O', CZ_ALKOHOL, 4.9);
    dodaj(h, v, d, 8, 'K', CZ_ALKOHOL, 4.9);
    dodaj(h, v, d, 10, 'O', CZ_NIC, 1.8);
    dodaj(h, v, d, 35, 'K', CZ_NIC, 1.8);
    std::vector<Wynik> a = przeglad(h, v), b = przeglad(h, v);
    bool rowne = a.size() == b.size();
    for (size_t i = 0; rowne && i < a.size(); i++)
      rowne = (a[i].zmienne.count("p") ? a[i].zmienne.at("p") : "") == (b[i].zmienne.count("p") ? b[i].zmienne.at("p") : "");
    SPRAWDZ(rowne);
  }
}

// ---------- cofnięcie „nie” na pytanie o wyjazd po uśpieniu panelu ----------
static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900, t.tm_mon = mi - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static std::string raportNocy(long day) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static reguly::Zestaw ZESTAW_W = atrapa_b5::zestaw();
static std::vector<reguly::Wynik> wynikiW(const reguly::Kontekst&) {
  std::vector<reguly::Wynik> w;
  for (auto& k : ZESTAW_W.katalog) w.push_back(atrapa_b5::wynik(ZESTAW_W, k.id, k.wariant, true, false));
  return w;
}
static void wybudz(Panel& p, uint32_t epoch, PowodWybudzenia pw, uint8_t bit = 0,
                   std::vector<std::pair<uint32_t, uint8_t>> skrypt = {}) {
  p.port.log.clear();
  p.zegar.epoch = epoch;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = pw;
  p.start.budzace = bit;
  p.przyciski.skrypt = skrypt;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  long doba = nrDnia("2026-11-04") - 1;
  p.siec.dodaj("/api/status", 200,
               "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\","
               "\"system\":\"IDLE\",\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}");
  p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[{\"day\":" + std::to_string(doba) +
                                         ",\"session_count\":1,\"stale\":false}]}");
  p.siec.dodaj(sciezkaNocy(doba), 200, raportNocy(doba));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static bool wLogu(const Panel& p, const std::string& f) {
  for (auto& l : p.port.log)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static int wpisowA(Panel& p) {
  int n = 0;
  const std::string& s = p.pliki.pliki["/dane/czynniki.csv"];
  for (size_t i = 0; (i = s.find(";A;", i)) != std::string::npos; i++) n++;
  return n;
}
static void testCofniecieWyjazduPoUspieniu() {
  sprawy::zrodloTestowe.wyniki = wynikiW;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_W;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_W);
  long zapis = nrDnia("2026-11-03");
  std::string plik = "#cpap-eink czynniki 1\ndata;stan;kody;warstwa;termin;pokazano\n";
  for (const char* d : {"2026-10-31", "2026-11-01", "2026-11-02"})
    plik += std::string(d) + ";O;P;A;" + std::to_string(zapis) + ";0\n";
  plik += "#koniec 3\n";
  Panel p;
  p.pliki.pliki["/dane/czynniki.csv"] = plik;
  wybudz(p, ep(2026, 11, 4, 7, 0), PowodWybudzenia::ZEGAR);
  SPRAWDZ(wLogu(p, "ekran dnia: pytanie wyjazd"));
  wybudz(p, ep(2026, 11, 4, 7, 5), PowodWybudzenia::PRZYCISK, BIT_LEWY, {{0, BIT_LEWY}, {300, 0}});  // ◀ = „nie”
  SPRAWDZ(wpisowA(p) == 0);
  printf("  po „nie” ekran %d, sen %u s, zegar po wybudzeniu +%u s\n", (int)p.start.stan.ekran,
         (unsigned)p.sen.plan.sekund, (unsigned)(p.zegar.teraz() - ep(2026, 11, 4, 7, 5)));
  // panel zasypia po czuwaniu; ◀ 2 s po 40 s (w 60 s z R-PRZ-11) budzi go w nowym wybudzeniu
  wybudz(p, ep(2026, 11, 4, 7, 5) + 40, PowodWybudzenia::PRZYCISK, BIT_LEWY, {{0, BIT_LEWY}, {2400, 0}});
  bool cofnieta = wLogu(p, "odpowiedź cofnięta");
  int a = wpisowA(p);
  printf("  ekran przed ◀ 2 s %d, odpowiedź cofnięta: %s, wpisów A po cofnięciu: %d (oczekiwane 3)\n",
         (int)p.start.stan.ekran, cofnieta ? "tak" : "nie", a);
  // gdy odpowiedź jest cofnięta, oznaczenie musi wrócić; b5.zdjeteWyjazd żyje tylko w pamięci jednego wybudzenia
  PRZYPADEK("cofnięcie „nie” na pytanie o wyjazd po uśpieniu panelu", cofnieta && a == 3);
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

int main() {
  setenv("TZ", "Europe/Warsaw", 1);
  tzset();
  odkrycia::zarejestrujRodzajeO();
  testK1LiczbyNocy();
  testO03Powrot();
  testO12Centralne();
  testO12Warstwy();
  testBezOdkrycia();
  testCofniecieWyjazduPoUspieniu();
  testPrzegladyTygodniowe();
  testKosztPrzegladu();
  printf("testy_czynniki_odkrycia_brzegowe: %d ok, %d trzyma, %d błędów\n", ok, trzyma, zle);
  return zle ? 1 : 0;
}
