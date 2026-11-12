// SPDX-License-Identifier: GPL-3.0-or-later
// Testy Portretu nocy (spec 5.4.16, ADR-0040):
//  T-TEST-414: odcinki 5 min z src/portret.cpp przy jedynym przejściu LicznikNocy po plikach sesji równe
//    wzorcowi tools/portret/przebieg.py na syntetycznej karcie tools/portret/karta_portretu.py (katalog z testy.sh:
//    `karta/` i `wzorzec/portret-RRRR-MM-DD.csv`); plik T-DANE-38 tam i z powrotem; plik uszkodzony (R-POR-21);
//    pamięć robocza zbieracza.
//  T-TEST-415: obrazek z src/portret_ekran.cpp równy wzorcowi `wzorzec/portret-RRRR-MM-DD.pbm` w strefie
//    treści w tolerancji pikseli; podpisy i legenda jak we wzorcu; teksty w limitach; czas rysowania.
// Dane wyłącznie syntetyczne. Bez argumentu części ze wzorcem są pominięte.
#include <dirent.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "airbridge.h"
#include "ekrany.h"
#include "noc_pliki.h"
#include "portret.h"
#include "portret_ekran.h"
#include "humor.h"
#include "nawigacja.h"
#include "str.h"
#include "wspolne.h"

using namespace portret;

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE(a, b)                                                                                     \
  do {                                                                                                  \
    std::string _a = (a), _b = (b);                                                                     \
    if (_a == _b) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)

static bool czytajPlik(const std::string& s, std::string& t) {
  std::ifstream f(s, std::ios::binary);
  if (!f) return false;
  std::stringstream b;
  b << f.rdbuf();
  t = b.str();
  return true;
}
static std::vector<std::string> pliki(const std::string& k) {
  std::vector<std::string> v;
  if (DIR* d = opendir(k.c_str())) {
    while (dirent* e = readdir(d))
      if (e->d_name[0] != '.') v.push_back(e->d_name);
    closedir(d);
  }
  std::sort(v.begin(), v.end());
  return v;
}
static std::vector<std::string> linie(const std::string& t) {
  std::vector<std::string> v;
  std::stringstream s(t);
  std::string l;
  while (std::getline(s, l)) v.push_back(l);
  return v;
}

// Doba z katalogu karty liczona jak na panelu: LicznikNocy z dołączonym zbieraczem (jedno przejście).
static Przebieg przebiegDoby(const std::string& katalog, long dzien, const Str& st, size_t* bajty = nullptr) {
  noc_pliki::LicznikNocy licz(dzien);
  Zbieracz z(dzien, maksimumZeStr(st, dzien));
  licz.dolaczPortret(&z);
  for (auto& n : pliki(katalog)) licz.dodajNazwe(n);
  for (auto& n : licz.kolejnosc(false, false)) {
    std::string t;
    if (!czytajPlik(katalog + "/" + n, t)) continue;
    licz.dodajPlik(n, (const uint8_t*)t.data(), t.size());
  }
  noc_pliki::WynikNocy w = licz.wynik();
  if (bajty) *bajty = z.bajty();
  return z.wynik(w, licz.zdarzeniaPortretu());
}

// T-TEST-414: zgodność z wzorcem Pythona
static void zgodnoscZWzorcem(const std::string& k) {
  std::string karta = k + "/karta", s;
  Str st;
  SPRAWDZ(czytajPlik(karta + "/STR.edf", s) && czytajStr(s, st) == edf::Blad::BRAK);
  int doby = 0;
  size_t najwiecej = 0;
  for (auto& d : pliki(karta + "/DATALOG")) {
    std::string data = d.substr(0, 4) + "-" + d.substr(4, 2) + "-" + d.substr(6, 2);
    long dzien = nrDnia(data);
    size_t b = 0;
    Przebieg p = przebiegDoby(karta + "/DATALOG/" + d, dzien, st, &b);
    najwiecej = std::max(najwiecej, b);
    std::string wz;
    SPRAWDZ(czytajPlik(k + "/wzorzec/portret-" + dataPoranka(dzien) + ".csv", wz));
    std::vector<std::string> l = linie(wz);
    SPRAWDZ(l.size() >= 4);
    if (l.size() < 4) continue;
    std::vector<std::string> oczekiwane(l.begin() + 2, l.end() - 1);
    std::vector<std::string> w = wiersze(p);
    ROWNE(std::to_string(w.size()), std::to_string(oczekiwane.size()));
    for (size_t i = 0; i < w.size() && i < oczekiwane.size(); i++) ROWNE(w[i], oczekiwane[i]);
    // tam i z powrotem
    Przebieg q;
    std::string blad;
    SPRAWDZ(parsuj(w, q, blad));
    SPRAWDZ(wiersze(q) == w);
    doby++;
  }
  SPRAWDZ(doby == 8);
  printf("  zgodność z wzorcem: %d dób, pamięć robocza zbieracza najwyżej %u B\n", doby, (unsigned)najwiecej);
  SPRAWDZ(najwiecej < 64 * 1024);
}

// wymaganie: R-POR-21
static void testR_POR_21_plikUszkodzony() {
  Przebieg p, q;
  p.data = "2026-11-04";
  p.sesje.push_back({660, 1140});
  Odcinek o;
  o.od = 900;
  o.minuty = 5;
  o.wyciek95 = 18;
  o.wycS = 300;
  o.zew = 2;
  o.centr = 0;
  p.odcinki.push_back(o);
  p.zdarzenia.push_back({901, true, false});
  std::vector<std::string> w = wiersze(p);
  std::string blad;
  SPRAWDZ(parsuj(w, q, blad));
  SPRAWDZ(q.odcinek(180) && q.odcinek(180)->wyciek95 == 18 && q.odcinek(180)->ps == BRAK);
  auto zly = [&](std::vector<std::string> x) {
    Przebieg r;
    std::string b;
    return !parsuj(x, r, b) && !b.empty();
  };
  std::vector<std::string> x = w;
  x.pop_back();  // bez wiersza B
  SPRAWDZ(zly(x));
  x = w;
  x[1] = "2026-11-04;O;900;5;18;300;-1;0;;;";  // liczba zdarzeń −1
  SPRAWDZ(zly(x));
  x = w;
  x[0] = "2026-11-04;S;1500;1600;;;;;;;";  // minuta spoza doby
  SPRAWDZ(zly(x));
  x = w;
  x[2] = "2026-11-05;Z;901;;;;1;0;;;";  // inna data
  SPRAWDZ(zly(x));
  x = w;
  x.back() = "2026-11-04;B;;;;;;;;;X";
  SPRAWDZ(zly(x));
  x = w;
  x[1] = "2026-11-04;Q;900;5;;;;;;;";
  SPRAWDZ(zly(x));
  x = w;
  x[1] = "2026-11-04;O;902;5;;;;;;;";  // minuta nie na siatce 5 min
  SPRAWDZ(zly(x));
}

void testyEkranuPortretu(const std::string& k);  // część T-TEST-415 (niżej, gdy jest wzorzec)

int main(int argc, char** argv) {
  testR_POR_21_plikUszkodzony();
  if (argc > 1) zgodnoscZWzorcem(argv[1]);
  testyEkranuPortretu(argc > 1 ? argv[1] : "");
  printf("testy_portret: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}

struct Bufor : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)y * 800 + (size_t)x] = c ? 1 : 0;
  }
};
static bool czytajPbm(const std::string& s, std::vector<uint8_t>& px) {
  std::string t;
  if (!czytajPlik(s, t) || t.compare(0, 11, "P4\n800 480\n") != 0 || t.size() < 11 + 100 * 480) return false;
  px.assign(800 * 480, 0);
  for (int y = 0; y < 480; y++)
    for (int x = 0; x < 800; x++) px[(size_t)y * 800 + (size_t)x] = ((uint8_t)t[11 + (size_t)y * 100 + (size_t)x / 8] >> (7 - x % 8)) & 1;
  return true;
}
static std::map<std::string, std::string> wejscie(const std::string& s) {
  std::map<std::string, std::string> m;
  std::string t;
  czytajPlik(s, t);
  for (auto& l : linie(t)) {
    size_t r = l.find('=');
    if (r != std::string::npos) m[l.substr(0, r)] = l.substr(r + 1);
  }
  return m;
}
static std::string zlaczPionowa(const std::vector<std::string>& v) {
  std::string w;
  for (size_t i = 0; i < v.size(); i++) w += (i ? "|" : "") + v[i];
  return w;
}

// Tolerancja porównania z wzorcem (T-TEST-415): w strefie treści najwyżej 0,5%
// pikseli czarnych wzorca i najwyżej TOLERANCJA_PX pikseli różnych od obrazka panelu, bez bloku 3×3 samych różnic
// (różnice to pojedyncze piksele krawędzi, nie przesunięty kształt), żadna różnica w kolumnie tekstu.
// Różnice biorą się tylko z krawędzi kształtów: wzorzec liczy kąt i pierwiastek w liczbach zmiennoprzecinkowych,
// panel tablicą kątów i pierwiastkiem całkowitym (T-WYM-97).
static const double TOLERANCJA_PROC = 0.5;
static const long TOLERANCJA_PX = 60;

static void porownajZWzorcem(const std::string& kat, const std::string& rano, int& sprawdzonych) {
  std::string baza = kat + "/portret-" + rano;
  std::vector<uint8_t> wz;
  if (!czytajPbm(baza + ".pbm", wz)) return;  // noc bez Portretu (R-POR-16): bez obrazka
  std::map<std::string, std::string> we = wejscie(baza + ".txt");
  std::string csv, blad;
  SPRAWDZ(czytajPlik(baza + ".csv", csv));
  std::vector<std::string> l = linie(csv);
  Przebieg pr;
  SPRAWDZ(l.size() >= 4 && parsuj(std::vector<std::string>(l.begin() + 2, l.end() - 1), pr, blad));
  DanePortretu d;
  d.przebieg = &pr;
  d.tryb = we["tryb"];
  d.normaPs10 = we["norma_ps10"].empty() ? BRAK : std::atoi(we["norma_ps10"].c_str());
  d.zwykla = std::atoi(we["zwykla"].c_str());
  d.pierwszy = we["pierwszy"];
  d.czas = we["czas"];
  d.ahi = we["ahi"];
  d.niepewna = we["niepewna"] == "1";
  bool linia = !we["linia"].empty();
  SPRAWDZ(jestPortret(pr));
  ROWNE(zlaczPionowa(podpisy(d, linia)), we["podpisy"]);
  ROWNE(zlaczPionowa(legenda(d)), we["legenda"]);
  Bufor b;
  int gora = 54 + (linia ? 48 : 0);
  auto t0 = std::chrono::steady_clock::now();
  rysujCialoPortretu(b, d, gora, linia);
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  long czarne = 0, rozne = 0, rozneKolumna = 0;
  for (int y = gora; y < 434; y++)
    for (int x = 0; x < 800; x++) {
      size_t i = (size_t)y * 800 + (size_t)x;
      czarne += wz[i];
      if (wz[i] != b.px[i]) {
        if (getenv("PORTRET_ROZNICE") && rozne < 40) printf("    (%d,%d) wz %d panel %d\n", x, y, wz[i], b.px[i]);
        rozne++;
        if (x >= 380) rozneKolumna++;
      }
    }
  double proc = czarne ? 100.0 * (double)rozne / (double)czarne : 0;
  printf("  %s%s: %ld z %ld pikseli czarnych wzorca różnych (%.2f%%), kolumna %ld, rysowanie %.1f ms (PC)\n",
         rano.c_str(), linia ? " z linią" : "", rozne, czarne, proc, rozneKolumna, ms);
  SPRAWDZ(proc <= TOLERANCJA_PROC);
  SPRAWDZ(rozne <= TOLERANCJA_PX);
  SPRAWDZ(rozneKolumna == 0);
  bool blok = false;  // blok 3×3 samych różnic
  for (int y = gora + 1; y < 433 && !blok; y++)
    for (int x = 1; x < 799 && !blok; x++) {
      bool caly = true;
      for (int dy = -1; dy <= 1 && caly; dy++)
        for (int dx = -1; dx <= 1 && caly; dx++) {
          size_t i = (size_t)(y + dy) * 800 + (size_t)(x + dx);
          caly = wz[i] != b.px[i];
        }
      blok = caly;
    }
  SPRAWDZ(!blok);
  sprawdzonych++;
}

// wymaganie: R-POR-22
static void testR_POR_22_katTarczy() {
  // kąt z tablicy wobec atan2: błąd najwyżej 2 s tarczy (0,017°) w całym pierścieniu
  int maks = 0;
  for (int dy = -172; dy <= 172; dy += 3)
    for (int dx = -172; dx <= 172; dx += 3) {
      if (!dx && !dy) continue;
      double k = std::atan2((double)dx, (double)-dy);
      if (k < 0) k += 2 * M_PI;
      int w = (int)std::lround(k / (2 * M_PI) * 43200) % 43200;
      int r = std::abs(katTarczy(dx, dy) - w);
      maks = std::max(maks, std::min(r, 43200 - r));
    }
  printf("  kąt z tablicy: największy błąd %d s tarczy\n", maks);
  SPRAWDZ(maks <= 2);
  SPRAWDZ(katTarczy(0, -10) == 0 && katTarczy(10, 0) == 10800 && katTarczy(0, 10) == 21600 && katTarczy(-10, 0) == 32400);
}

// wymaganie: R-POR-12
static void testR_POR_12_tekstyWLimitach() {
  SPRAWDZ(szer(F_P28B, "12 h 59 z maską · AHI 99,9") <= 382);
  SPRAWDZ(zawin(F_P28B, "Noc w normie. Tydzień: 4 wskaźniki inaczej niż zwykle.", 382).size() == 2);
  for (const char* t : {"Wsparcie oddechu: uczę się.", "Wsparcie oddechu: brak odczytu.", "Wyciek: brak odczytu.",
                        "Zdarzenia: brak odczytu.", "Rodzaj bezdechów: brak zapisu.", "Najgęściej 23:55–00:55",
                        "Wyciek przez 720 min", "Przerwy: 99, najdłuższa 720 min", "Centralne: 999",
                        "Ciśnienie na maksimum 720 min", "EPAP na maksimum 720 min", "Część nocy poza tarczą",
                        "Drzemka poza tarczą"})
    SPRAWDZ(szer(F_P24B, t) <= 382);
  for (const char* k : {"maska", "wyciek", "wsparcie", "zdarzenia", "centralne", "kropka"})
    SPRAWDZ(42 + szer(F_P22B, opisLegendy(k)) <= 382);
  // najgorszy przypadek (pierwszy w 2 liniach, 3 podpisy, 4 pozycje legendy) w strefie 54–434; od 12.11.2026 noc niepewna ma
  // jeden wiersz liczb jak każda noc: 8 + 34 + 10 + 68 + 96 + 17 + 112 = 345 ≤ 380 (podpis zbiorczy
  // braków w 2 liniach: 377, testR_POR_27_brakiNigdyNieWypadaja())
  Przebieg pr;
  pr.data = "2026-11-04";
  pr.sesje = {{600, 700}, {720, 800}, {820, 900}, {1300, 1400}};
  pr.braki = "LE";
  DanePortretu d;
  d.przebieg = &pr;
  d.tryb = "AutoSet";
  d.zwykla = 660;
  d.pierwszy = "Noc w normie. Tydzień: 4 wskaźniki inaczej niż zwykle.";
  d.czas = "12 h 59";
  d.niepewna = true;
  SPRAWDZ(podpisy(d, false).size() == 4 && podpisy(d, true).size() == 2);
  Bufor b;
  rysujCialoPortretu(b, d, 54, false);
  for (int x = 0; x < 800; x++) SPRAWDZ(!b.px[(size_t)434 * 800 + (size_t)x] && !b.px[(size_t)433 * 800 + (size_t)x]);
}

// wymaganie: R-POR-16
static void testR_POR_16_nocBezTerapii() {
  Przebieg p;
  p.data = "2026-11-04";
  p.sesje = {{660, 670}};
  SPRAWDZ(!jestPortret(p));
  p.sesje = {{660, 675}};
  SPRAWDZ(jestPortret(p));
  p.data.clear();
  SPRAWDZ(!jestPortret(p));
}

// T-WYM-97: czas rysowania noc z 288 odcinkami i 500 zdarzeniami (na PC; na panelu w logu wybudzenia)
static void czasRysowania() {
  Przebieg p;
  p.data = "2026-11-04";
  p.sesje = {{0, 1440}};
  for (int k = 0; k < 288; k++) {
    Odcinek o;
    o.od = k * 5;
    o.minuty = 5;
    o.wyciek95 = k % 7 == 0 ? 20 : 5;
    o.wycS = 0;
    o.zew = k % 9;
    o.centr = k % 5 == 0 ? 2 : 0;
    o.ps = 50 + k % 60;
    o.maks = 0;
    p.odcinki.push_back(o);
  }
  for (int i = 0; i < 500; i++) p.zdarzenia.push_back({(i * 7) % 1440, true, false});
  for (const char* tryb : {"AutoSet", "ASVAuto"}) {
    DanePortretu d;
    d.przebieg = &p;
    d.tryb = tryb;
    d.normaPs10 = 50;
    d.zwykla = 660;
    d.pierwszy = "Tydzień jak zwykle.";
    d.czas = "8 h 00";
    d.ahi = "4,1";
    Bufor b;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 10; i++) rysujCialoPortretu(b, d, 54, false);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 10;
    printf("  czas rysowania %s, 288 odcinków i 500 zdarzeń: %.1f ms na PC\n", tryb, ms);
    SPRAWDZ(ms < 200);  // bramka na PC; bramka panelu po pomiarze w logu (T-WYM-97)
  }
}

// ---------- przypadki brzegowe ze znaną odpowiedzią ----------
// Plik PLD w pamięci: Press.2s, EprPress.2s, Leak.2s (L/min), rekord 60 s = 30 próbek, 0,1 jednostki na krok.
struct PldPamiec {
  std::vector<uint8_t> buf;
  edf::Plik p;
  PldPamiec(long long startS, int rekordy, double leak, double press = 10, double epr = 6, int szum = 0) {
    const char* et[3] = {"Press.2s", "EprPress.2s", "Leak.2s"};
    double v[3] = {press, epr, leak};
    p.startS = startS;
    p.czasRekordu = 60;
    p.rekordy = rekordy;
    p.bajtyRekordu = 3 * 30 * 2;
    for (int s = 0; s < 3; s++) {
      edf::Sygnal g;
      g.etykieta = et[s];
      g.jednostka = s == 2 ? "L/min" : "cmH2O";
      g.fizMin = 0;
      g.fizMax = 200;
      g.cyfMin = 0;
      g.cyfMax = 2000;
      g.probki = 30;
      g.offset = (size_t)s * 60;
      p.sygnaly.push_back(g);
    }
    buf.assign((size_t)rekordy * p.bajtyRekordu + 2, 0);
    for (int r = 0; r < rekordy; r++)
      for (int s = 0; s < 3; s++)
        for (int i = 0; i < 30; i++) {
          int c = (int)(v[s] * 10 + 0.5) + (szum ? i % szum : 0);
          size_t o = (size_t)r * p.bajtyRekordu + (size_t)s * 60 + (size_t)i * 2;
          buf[o] = (uint8_t)(c & 255);
          buf[o + 1] = (uint8_t)(c >> 8);
        }
    p.dane = buf.data();
    p.rozmiar = buf.size();
  }
};
static long long poludnie(long dzien) { return (long long)dzien * 86400LL + 12 * 3600LL; }
static noc_pliki::SesjaNocy sesjaZapisu(double od, double do_) {
  noc_pliki::SesjaNocy s;
  s.maZapis = true;
  s.zapisOd = od;
  s.zapisDo = do_;
  return s;
}
static bool czytelny(const Przebieg& p) {
  Przebieg q;
  std::string blad;
  bool ok_ = parsuj(wiersze(p), q, blad);
  if (!ok_) printf("    parsuj: %s\n", blad.c_str());
  return ok_ && wiersze(q) == wiersze(p);
}
static DanePortretu dane(const Przebieg& p, const char* tryb, int zwykla) {
  DanePortretu d;
  d.przebieg = &p;
  d.tryb = tryb;
  d.zwykla = zwykla;
  d.pierwszy = "Tydzień jak zwykle.";
  d.czas = "8 h 00";
  d.ahi = "1,0";
  return d;
}
static bool maPodpis(const DanePortretu& d, const std::string& s, bool linia = false) {
  for (auto& x : podpisy(d, linia))
    if (x == s) return true;
  return false;
}
static bool maLegende(const DanePortretu& d, const std::string& k) {
  for (auto& x : legenda(d))
    if (x == k) return true;
  return false;
}

// wymaganie: R-POR-17
// Odcinek 02:10–02:15 dzielony przez dwa pliki (sesja 02:00–02:12 bez wycieku, 02:13–02:30 z 30 L/min): P95 z próbek
// obu części (60 × 0 i 60 × 30 → ranga 114 ze 120 = 30), 120 s wycieku, minuty 10, 11, 13, 14.
static void testR_POR_17_odcinekDwochPlikow() {
  long dz = nrDnia("2026-11-03");
  long long b = poludnie(dz);
  PldPamiec a(b + 14 * 3600, 12, 0.0), c(b + 14 * 3600 + 13 * 60, 17, 30.0);
  Zbieracz z(dz);
  z.plikPld(a.p);
  z.plikPld(c.p);
  noc_pliki::WynikNocy w;
  w.zdarzeniaZnane = true;
  w.sesje = {sesjaZapisu((double)a.p.startS, a.p.koniecS()), sesjaZapisu((double)c.p.startS, c.p.koniecS())};
  Przebieg p = z.wynik(w, {});
  const Odcinek* o = p.odcinek((14 * 60 + 10) / 5);
  SPRAWDZ(o && o->wyciek95 == 30 && o->wycS == 120 && o->minuty == 4);
  const Odcinek* o2 = p.odcinek((14 * 60 + 5) / 5);
  SPRAWDZ(o2 && o2->wyciek95 == 0);
}

// wymaganie: R-POR-21
// plik zapisany przez panel jest czytelny dla panelu także przy plikach zachodzących w czasie (zegar aparatu
// cofnięty: `wyc_s` przycięte do 300) i przy sesji zaczętej w ostatniej minucie doby (`od` = 1440, zdarzenie po
// dobie bez wiersza `Z`).
static void testR_POR_21_zakresyZapisuIOdczytu() {
  long dz = nrDnia("2026-10-25");
  long long b = poludnie(dz);
  PldPamiec a(b + 14 * 3600, 60, 20.0), c(b + 14 * 3600 + 30 * 60, 60, 20.0);  // 02:00–03:00 i 02:30–03:30
  Zbieracz z(dz);
  z.plikPld(a.p);
  z.plikPld(c.p);
  noc_pliki::WynikNocy w;
  w.zdarzeniaZnane = true;
  w.sesje = {sesjaZapisu((double)a.p.startS, a.p.koniecS()), sesjaZapisu((double)c.p.startS, c.p.koniecS())};
  Przebieg p = z.wynik(w, {});
  SPRAWDZ(czytelny(p));
  const Odcinek* o = p.odcinek((14 * 60 + 40) / 5);
  SPRAWDZ(o && o->wycS == WYC_S_MAKS);
  long dz2 = nrDnia("2026-11-03");
  long long b2 = poludnie(dz2);
  PldPamiec k(b2 + 86400 - 20, 30, 5.0);  // 11:59:40 następnego dnia
  Zbieracz z2(dz2);
  z2.plikPld(k.p);
  noc_pliki::WynikNocy w2;
  w2.zdarzeniaZnane = true;
  w2.sesje = {sesjaZapisu((double)b2 + 23 * 3600, (double)b2 + 23 * 3600 + 1800), sesjaZapisu((double)k.p.startS, k.p.koniecS())};
  Przebieg p2 = z2.wynik(w2, {{(double)b2 + 86400 + 60, "Hypopnea"}});
  SPRAWDZ(czytelny(p2));
  SPRAWDZ(p2.sesje.back().od == SESJA_OD_MAKS && p2.zdarzenia.empty());
}

// wymaganie: R-POR-09
// 600 spłyceń i 10 bezdechów centralnych po nich (wiersze `Z` mają tylko pierwsze 500): `Centralne: 10`
// z odcinków, piki do środka w legendzie.
static void testR_POR_09_centralnePonad500() {
  Przebieg p;
  p.data = "2026-11-04";
  p.sesje = {{660, 1140}};
  for (int k = 132; k < 228; k++) {
    Odcinek o;
    o.od = k * 5;
    o.minuty = 5;
    o.wyciek95 = 0;
    o.wycS = 0;
    o.zew = 0;
    o.centr = 0;
    p.odcinki.push_back(o);
  }
  for (int i = 0; i < 600; i++) {
    int m = 660 + i * 400 / 600;
    if ((int)p.zdarzenia.size() < MAKS_ZDARZEN) p.zdarzenia.push_back({m, true, false});
    for (auto& o : p.odcinki)
      if (o.od == m / 5 * 5) o.zew++;
  }
  for (int i = 0; i < 10; i++) {
    int m = 1100 + i;
    if ((int)p.zdarzenia.size() < MAKS_ZDARZEN) p.zdarzenia.push_back({m, false, true});
    for (auto& o : p.odcinki)
      if (o.od == m / 5 * 5) o.centr++;
  }
  DanePortretu d = dane(p, "AutoSet", 660);
  SPRAWDZ(maLegende(d, "centralne"));
  SPRAWDZ(maPodpis(d, "Centralne: 10"));
}

// wymaganie: R-POR-06
// noc 01:07–09:25 i drzemka 12:22–14:02 przy zwykłej porze 17:50 (M-18 z historii sprzed 09.11.2026): okno od
// startu najdłuższej sesji minus 2 h (23:05–11:05), cała sesja na tarczy; drzemka poza tarczą w podpisie. Noc dłuższa
// niż 12 h: okno od jej startu, `Część nocy poza tarczą` przed `Drzemka poza tarczą`.
static void testR_POR_06_oknoNaNajdluzszejSesji() {
  Przebieg p;
  p.data = "2026-11-09";
  p.sesje = {{22, 122}, {787, 1285}};
  DanePortretu d = dane(p, "ASVAuto", 350);
  d.normaPs10 = 50;
  int w = okno(d);
  ROWNE(hhmmOdPoludnia(w), "23:05");
  SPRAWDZ(w <= 787 && w + OKNO_MIN >= 1285);
  SPRAWDZ(maPodpis(d, "Drzemka poza tarczą") && !maPodpis(d, "Część nocy poza tarczą"));
  // zwykła pora 23:47 (mediana z 14 nocy), sesja 23:40–06:50 → okno 21:45–09:45 (R-POR-06)
  Przebieg q;
  q.data = "2026-11-04";
  q.sesje = {{700, 1130}};
  DanePortretu e = dane(q, "AutoSet", 707);
  ROWNE(hhmmOdPoludnia(okno(e)), "21:45");
  // sesja 13 h z drzemką: okno od startu sesji, część nocy poza tarczą przed drzemką
  Przebieg r;
  r.data = "2026-11-05";
  r.sesje = {{10, 60}, {600, 1380}};
  DanePortretu f = dane(r, "AutoSet", 600);
  ROWNE(hhmmOdPoludnia(okno(f)), "22:00");
  std::vector<std::string> pd = podpisy(f, false);
  bool czesc = false, drzemka = false;
  size_t ic = 0, id = 0;
  for (size_t i = 0; i < pd.size(); i++) {
    if (pd[i] == "Część nocy poza tarczą") { czesc = true; ic = i; }
    if (pd[i] == "Drzemka poza tarczą") { drzemka = true; id = i; }
  }
  SPRAWDZ(czesc && drzemka && ic < id);
}

// wymaganie: R-POR-13
// sesja kończy się 03:02, odcinek 03:00 z P95 20: biały łuk jest na rysunku (02:00–03:02 sesji w odcinku),
// więc legenda ma `wyciek od 15 l/min`; odcinek bez sesji w ogóle (01:00) nie daje legendy.
static void testR_POR_13_legendaWycieku() {
  Przebieg p;
  p.data = "2026-11-04";
  p.sesje = {{660, 902}};
  Odcinek o;
  o.od = 900;
  o.minuty = 2;
  o.wyciek95 = 20;
  o.wycS = 120;
  o.zew = 0;
  o.centr = 0;
  p.odcinki.push_back(o);
  DanePortretu d = dane(p, "AutoSet", 660);
  SPRAWDZ(maLegende(d, "wyciek"));
  SPRAWDZ(maPodpis(d, "Wyciek przez 2 min"));  // podpis z tego samego odcinka co rysunek
  Bufor bf;
  rysujCialoPortretu(bf, d, 54, false);
  bool bialy = false;  // biały punkt na promieniu 126 w odcinku 03:00–03:02
  for (int s = 900 * 60 + 5; s < 902 * 60; s += 20) {
    double a = (s % 43200) / 43200.0 * 2 * M_PI;
    int x = 200 + (int)std::lround(126 * std::sin(a)), y = 244 - (int)std::lround(126 * std::cos(a));
    bialy = bialy || !bf.px[(size_t)y * 800 + (size_t)x];
  }
  SPRAWDZ(bialy);
  p.sesje = {{660, 840}};
  SPRAWDZ(!maLegende(d, "wyciek"));
  SPRAWDZ(!maPodpis(d, "Wyciek przez 2 min"));  // odcinek bez sesji: bez łuku, bez podpisu
  SPRAWDZ(maLegende(d, "maska") && legenda(d).size() == 1);  // noc bez pików: tylko `maska na twarzy`
}

// wymaganie: R-POR-08
// biały odcinek, podpis `Wyciek przez {n} min` i legenda z tego samego warunku:
// odcinek z co najmniej 60 s próbek ≥ 15 L/min. Wyciek rozproszony (30 odcinków × 12 s = 6 min, 12 × 16 s) z P95
// odcinka 20 L/min: bez białego, bez podpisu, bez legendy; jeden odcinek 60 s: biały, `Wyciek przez 1 min`, legenda;
// 59 s: czarny, bez podpisu.
static Przebieg nocRozproszona(int n, int sek) {
  Przebieg p;
  p.data = "2026-11-04";
  p.sesje = {{660, 1140}};
  for (int m = 660; m < 1140; m += 5) {
    Odcinek o;
    o.od = m;
    o.minuty = 5;
    o.wyciek95 = 0;
    o.wycS = 0;
    o.zew = 0;
    o.centr = 0;
    p.odcinki.push_back(o);
  }
  for (int i = 0; i < n; i++) {
    p.odcinki[(size_t)(2 * i)].wycS = sek;
    p.odcinki[(size_t)(2 * i)].wyciek95 = 20;
  }
  return p;
}
static bool maPodpisWycieku(const DanePortretu& d) {
  for (auto& x : podpisy(d, false))
    if (x.compare(0, 13, "Wyciek przez ") == 0) return true;
  return false;
}
static void testR_POR_08_wyciekZJednegoZrodla() {
  for (auto nk : {std::pair<int, int>{30, 12}, std::pair<int, int>{12, 16}}) {
    Przebieg p = nocRozproszona(nk.first, nk.second);
    DanePortretu d = dane(p, "AutoSet", 660);
    SPRAWDZ(!maPodpisWycieku(d));
    SPRAWDZ(!maLegende(d, "wyciek"));
    Bufor b;
    rysujCialoPortretu(b, d, 54, false);
    bool czarne = true;  // pierścień czarny w środku każdego odcinka z wyciekiem
    for (int i = 0; i < nk.first; i++) {
      int x, y;
      double a = (((660 + i * 10) * 60 + 150) % 43200) / 43200.0 * 2 * M_PI;
      x = 200 + (int)std::lround(126 * std::sin(a));
      y = 244 - (int)std::lround(126 * std::cos(a));
      czarne = czarne && b.px[(size_t)y * 800 + (size_t)x];
    }
    SPRAWDZ(czarne);
  }
  Przebieg p = nocRozproszona(0, 0);
  p.odcinki[(780 - 660) / 5].wycS = 60;
  p.odcinki[(780 - 660) / 5].wyciek95 = 16;
  DanePortretu d = dane(p, "AutoSet", 660);
  SPRAWDZ(maPodpis(d, "Wyciek przez 1 min"));
  SPRAWDZ(maLegende(d, "wyciek"));
  Bufor b;
  rysujCialoPortretu(b, d, 54, false);
  double a = ((782 * 60) % 43200) / 43200.0 * 2 * M_PI;
  int x = 200 + (int)std::lround(126 * std::sin(a)), y = 244 - (int)std::lround(126 * std::cos(a));
  SPRAWDZ(!b.px[(size_t)y * 800 + (size_t)x]);  // biały środek
  x = 200 + (int)std::lround(119 * std::sin(a));
  y = 244 - (int)std::lround(119 * std::cos(a));
  SPRAWDZ(b.px[(size_t)y * 800 + (size_t)x]);  // czarny obrys
  p.odcinki[(780 - 660) / 5].wycS = 59;
  SPRAWDZ(!maPodpisWycieku(d) && !maLegende(d, "wyciek"));
  // przykład z R-POR-08: 04:00–04:30 z 300, 120, 60, 59, 0, 300 s → białe 04:00–04:15 i 04:25–04:30, 780 s = 13 min
  {
    Przebieg q = nocRozproszona(0, 0);
    const int sek[6] = {300, 120, 60, 59, 0, 300};
    for (int i = 0; i < 6; i++) q.odcinki[(size_t)((960 - 660) / 5 + i)].wycS = sek[i];
    DanePortretu dq = dane(q, "AutoSet", 660);
    SPRAWDZ(maPodpis(dq, "Wyciek przez 13 min"));
  }
  // odcinek z 60 s wycieku, ale za mało próbek (pole `wyciek95` puste, odcinek pusty): czarny, bez podpisu
  p.odcinki[(780 - 660) / 5].wycS = 300;
  p.odcinki[(780 - 660) / 5].wyciek95 = BRAK;
  SPRAWDZ(!maPodpisWycieku(d) && !maLegende(d, "wyciek"));
}

// wymaganie: R-POR-27
// brak pomiaru nigdy nie wypada z podpisów. Każda kombinacja
// liter L, P, E, R, M (32) × tryb × norma PS × linia uwagi × noc niepewna × pierwszy podpis w 1 albo 2 liniach, przy
// nocy z 4 pozycjami legendy i podpisami 2–7: każdy podpis „B” jest pełnym podpisem albo nazwą w podpisie zbiorczym
// (pełne przed zbiorczym w kolejności tabeli), podpis zbiorczy zawsze z nazwami w najwyżej 2 liniach po 382 px
// (F_P24B), pozostałe podpisy w 1 linii, nic z kolumny poniżej y = 434; znane odpowiedzi jak we wzorcu
// (tools/portret/test_portret.py test_braki_zbiorczy_znane_odpowiedzi).
static std::string nazwaBraku(const std::string& t) {
  if (t == "Wsparcie oddechu: brak odczytu.") return "wsparcie";
  if (t == "Wsparcie oddechu: uczę się.") return "norma wsparcia";
  if (t == "Wyciek: brak odczytu.") return "wyciek";
  if (t == "Zdarzenia: brak odczytu.") return "zdarzenia";
  if (t == "Rodzaj bezdechów: brak zapisu.") return "rodzaj bezdechów";
  if (t == "Maksimum ciśnienia: nieznane." || t == "Maksimum EPAP: nieznane.") return "maksimum";
  return "";
}
// noc z pikami (Najgęściej), przerwą, białym wyciekiem, centralnymi albo wsparciem i kropkami: legenda z 4 pozycjami
static Przebieg nocPelna(bool asvTryb) {
  Przebieg p;
  p.data = "2026-11-04";
  p.sesje = {{660, 905}, {920, 1140}};
  for (int m = 660; m < 1140; m += 5) {
    if (m >= 905 && m < 920) continue;
    Odcinek o;
    o.od = m;
    o.minuty = 5;
    o.wyciek95 = m == 960 ? 22 : 5;
    o.wycS = m == 960 ? 300 : 0;
    o.zew = (m == 840 || m == 845 || m == 850) ? 3 : 0;
    o.centr = m == 870 ? 2 : 0;
    o.ps = (m == 840 || m == 845 || m == 850) ? 110 : 50;
    p.odcinki.push_back(o);
  }
  if (asvTryb)
    for (int m : {700, 842, 1000}) p.zdarzenia.push_back({m, true, false});
  return p;
}
static void testR_POR_27_brakiNigdyNieWypadaja() {
  const char* litery = "LPERM";
  int sprawdzonych = 0, zbiorczych = 0, dwuliniowych = 0, legenda4 = 0;
  for (int m = 0; m < 32; m++)
    for (const char* tryb : {"CPAP", "AutoSet", "ASV", "ASVAuto"})
      for (int norma : {50, BRAK})
        for (bool linia : {false, true})
          for (bool niepewna : {false, true})
            for (bool dlugi : {false, true}) {
              Przebieg p = nocPelna(asv(tryb));
              for (int i = 0; i < 5; i++)
                if (m & (1 << i)) p.braki += litery[i];
              DanePortretu d = dane(p, tryb, 660);
              d.normaPs10 = norma;
              d.niepewna = niepewna;
              d.czas = "12 h 59";
              d.ahi = "99,9";
              d.pierwszy = dlugi ? "Noc w normie. Tydzień: 4 wskaźniki inaczej niż zwykle." : "Tydzień jak zwykle.";
              std::vector<std::string> oczek;
              for (auto& x : braki(d)) oczek.push_back(x.first);
              std::vector<std::string> pd = podpisy(d, linia);
              SPRAWDZ(pd.size() <= (linia ? 2u : 4u));
              std::vector<std::string> pelne, zb;
              for (size_t i = 1; i < pd.size(); i++) {
                bool zbiorczy = pd[i].compare(0, 10, "Nieznane: ") == 0;
                if (!nazwaBraku(pd[i]).empty()) pelne.push_back(pd[i]);
                if (zbiorczy) zb.push_back(pd[i]);
                std::vector<std::string> l = zawin(F_P24B, pd[i], 382);
                SPRAWDZ(l.size() <= (zbiorczy ? 2u : 1u));
                for (auto& x : l) SPRAWDZ(szer(F_P24B, x) <= 382);
                dwuliniowych += zbiorczy && l.size() == 2;
              }
              SPRAWDZ(zb.size() <= 1);
              SPRAWDZ(pelne.size() <= oczek.size() && std::equal(pelne.begin(), pelne.end(), oczek.begin()));
              size_t reszta = oczek.size() - std::min(oczek.size(), pelne.size());
              if (reszta) {
                SPRAWDZ(!zb.empty() && reszta >= 2);
                if (zb.empty()) continue;
                std::string nazwy;
                for (size_t i = pelne.size(); i < oczek.size(); i++) nazwy += (nazwy.empty() ? "" : ", ") + nazwaBraku(oczek[i]);
                SPRAWDZ(zb[0] == "Nieznane: " + nazwy + ".");
                SPRAWDZ(pd.size() > pelne.size() + 1 && pd[pelne.size() + 1] == zb[0]);
                zbiorczych++;
              } else {
                SPRAWDZ(zb.empty());
              }
              legenda4 += legenda(d).size() == 4;
              Bufor b;
              rysujCialoPortretu(b, d, linia ? 102 : 54, linia);
              bool poza = false;  // kolumna (x od 390) nie wychodzi poniżej strefy treści
              for (int y = 434; y < 480; y++)
                for (int x = 380; x < 800; x++) poza = poza || b.px[(size_t)y * 800 + (size_t)x];
              SPRAWDZ(!poza);
              sprawdzonych++;
            }
  printf("  braki pomiaru: %d kombinacji, %d z podpisem zbiorczym (%d w 2 liniach), %d z legendą 4 pozycji\n",
         sprawdzonych, zbiorczych, dwuliniowych, legenda4);
  SPRAWDZ(sprawdzonych == 32 * 4 * 2 * 2 * 2 * 2);
  SPRAWDZ(zbiorczych > 0 && dwuliniowych > 0 && legenda4 > 0);
  // znane odpowiedzi (te same we wzorcu)
  auto pod = [](const char* braki, const char* tryb, int norma, bool linia) {
    static Przebieg p;
    p = Przebieg();
    p.data = "2026-11-04";
    p.sesje = {{660, 1140}};
    p.braki = braki;
    DanePortretu d = dane(p, tryb, 660);
    d.normaPs10 = norma;
    std::string w;
    for (auto& x : podpisy(d, linia)) w += (w.empty() ? "" : "|") + x;
    return w;
  };
  ROWNE(pod("LE", "AutoSet", 50, true), "Tydzień jak zwykle.|Nieznane: wyciek, zdarzenia.");
  ROWNE(pod("LPEM", "ASVAuto", 50, false),
        "Tydzień jak zwykle.|Wsparcie oddechu: brak odczytu.|Wyciek: brak odczytu.|Nieznane: zdarzenia, maksimum.");
  ROWNE(pod("LPEM", "ASVAuto", 50, true), "Tydzień jak zwykle.|Nieznane: wsparcie, wyciek, zdarzenia, maksimum.");
  ROWNE(pod("LRM", "AutoSet", 50, true), "Tydzień jak zwykle.|Nieznane: wyciek, rodzaj bezdechów, maksimum.");
  ROWNE(pod("L", "ASV", BRAK, true), "Tydzień jak zwykle.|Nieznane: norma wsparcia, wyciek.");
  ROWNE(pod("LE", "AutoSet", 50, false), "Tydzień jak zwykle.|Wyciek: brak odczytu.|Zdarzenia: brak odczytu.");
}

// wymaganie: R-POR-26
// AutoSet bez MPA w STR (Maksimum bez progu) → litera `M` i `Maksimum ciśnienia: nieznane.`; ASVAuto bez EAX →
// `Maksimum EPAP: nieznane.`; CPAP → bez litery i bez podpisu.
static void testR_POR_26_maksimumNieznane() {
  Maksimum m = maksimumTrybu("AutoSet", NAN, NAN);
  SPRAWDZ(m.potrzebne && !m.kanal);
  long dz = nrDnia("2026-11-03");
  PldPamiec a(poludnie(dz) + 11 * 3600, 60, 5.0);
  Zbieracz z(dz, m);
  z.plikPld(a.p);
  noc_pliki::WynikNocy w;
  w.zdarzeniaZnane = true;
  w.sesje = {sesjaZapisu((double)a.p.startS, a.p.koniecS())};
  Przebieg p = z.wynik(w, {});
  SPRAWDZ(p.brak('M') && czytelny(p));
  for (auto& o : p.odcinki) SPRAWDZ(o.maks == BRAK);
  DanePortretu d = dane(p, "AutoSet", 660);
  SPRAWDZ(maPodpis(d, "Maksimum ciśnienia: nieznane."));
  d.tryb = "ASVAuto";
  SPRAWDZ(maPodpis(d, "Maksimum EPAP: nieznane."));
  Zbieracz zc(dz, maksimumTrybu("CPAP", NAN, NAN));
  zc.plikPld(a.p);
  SPRAWDZ(!zc.wynik(w, {}).brak('M'));
  Zbieracz zs(dz, maksimumTrybu("AutoSet", 15.0f, NAN));  // z MPA i kanałem Press.2s: bez braku
  zs.plikPld(a.p);
  SPRAWDZ(!zs.wynik(w, {}).brak('M'));
}

// wymaganie: R-POR-25
// po nocy niepewnej pierwszy podpis `Tej nocy nie oceniam: duży wyciek.` zamiast zdania o tygodniu,
// w 2 liniach kolumny; noc pewna: zdanie o tygodniu.
static void testR_POR_25_nocNiepewna() {
  Przebieg p;
  p.data = "2026-11-04";
  p.sesje = {{660, 1140}};
  DanePortretu d = dane(p, "AutoSet", 660);
  ROWNE(podpisy(d, false).front(), "Tydzień jak zwykle.");
  d.niepewna = true;
  ROWNE(podpisy(d, false).front(), "Tej nocy nie oceniam: duży wyciek.");
  SPRAWDZ(zawin(F_P28B, podpisy(d, false).front(), 382).size() == 2);
}

// wymaganie: R-POR-15
// po nocy niepewnej jeden komunikat. Liczby nocy to sam wiersz `8 h 00 z maską`
// (bez AHI i bez drugiego wiersza `AHI nie oceniam: duży wyciek`), pierwszy podpis `Tej nocy nie oceniam: duży
// wyciek.` stoi zaraz pod nim (od y = 106); AHI nie pokazuje się jako liczba, nawet gdy jest w danych.
static void testR_POR_15_jedenKomunikat() {
  Przebieg p;
  p.data = "2026-11-04";
  p.sesje = {{660, 1140}};
  DanePortretu d = dane(p, "AutoSet", 660);
  d.niepewna = true;
  d.ahi = "4,2";
  Bufor b, c, wz;
  rysujCialoPortretu(b, d, 54, false);
  d.ahi = "";
  rysujCialoPortretu(c, d, 54, false);
  SPRAWDZ(b.px == c.px);  // AHI z danych nie ma wpływu na obraz
  // wzór kolumny: wiersz liczb i pierwszy podpis w 2 liniach, bez wiersza AHI między nimi
  tekst(wz, F_P28B, 390, 62 + 26, "8 h 00 z maską");
  std::vector<std::string> l = zawin(F_P28B, "Tej nocy nie oceniam: duży wyciek.", 382);
  SPRAWDZ(l.size() == 2);
  for (size_t i = 0; i < l.size(); i++) tekst(wz, F_P28B, 390, 106 + 26 + 34 * (int)i, l[i]);
  bool rowne = true;
  for (int y = 54; y < 106 + 68; y++)
    for (int x = 390; x < 772; x++) rowne = rowne && b.px[(size_t)y * 800 + (size_t)x] == wz.px[(size_t)y * 800 + (size_t)x];
  SPRAWDZ(rowne);
  // nic z dawnego drugiego wiersza (`AHI nie oceniam: duży wyciek` B6 na y 104–127) poza pierwszym podpisem
  Bufor stary;
  tekst(stary, F_P24B, 390, 62 + 34 + 25, "AHI nie oceniam: duży wyciek");
  bool jakStary = true;
  for (int y = 96; y < 128; y++)
    for (int x = 390; x < 772; x++)
      if (stary.px[(size_t)y * 800 + (size_t)x]) jakStary = jakStary && b.px[(size_t)y * 800 + (size_t)x];
  SPRAWDZ(!jakStary);
}

// T-WYM-98: pamięć robocza zbieracza nie rośnie z liczbą sesji (najgorszy przypadek: 40 krótkich sesji
// zaczętych w środku odcinka, wszystkie odcinki na granicy pliku) i mieści się w ZBIERACZ_MAKS_B.
static const size_t ZBIERACZ_MAKS_B = 16 * 1024;
static void testT_WYM_98_pamiecZbieracza() {
  long dz = nrDnia("2026-11-03");
  long long b = poludnie(dz);
  size_t najw = 0;
  for (int n : {1, 5, 20, 40}) {
    Zbieracz z(dz);
    std::vector<PldPamiec*> v;
    for (int i = 0; i < n; i++) {
      v.push_back(new PldPamiec(b + 11 * 3600 + i * (600 / (n > 20 ? 2 : 1)) + 150, n > 20 ? 4 : 8, 5, 10, 6, 7));
      z.plikPld(v.back()->p);
    }
    najw = std::max(najw, z.bajty());
    printf("  pamięć zbieracza przy %d sesjach: %u B\n", n, (unsigned)z.bajty());
    for (auto* x : v) delete x;
  }
  SPRAWDZ(najw <= ZBIERACZ_MAKS_B);
}

// wymaganie: R-POR-20
// 7 poranków z Portretem: wpisy `S11` w dzienniku nie zmieniają limitu kart (R-HUM-23) ani rotacji S2a–S2c.
static void testR_POR_20_poraLimitow() {
  std::vector<WpisHumoru> bez, z;
  for (int i = 1; i <= 7; i++) {
    std::string d = dodajDni("2026-11-04", -i);
    WpisHumoru w{d, "S2A", std::to_string(i), i <= 4 ? 'K' : 'D', i <= 4 ? "A01" : "", ""};
    bez.push_back(w);
    z.push_back({d, "S11", "", 'D', "", ""});
    z.push_back(w);
  }
  EkranDnia e;
  e.typ = "S2A";
  e.klucz = "3";
  e.kartaZTabeli = true;
  KontekstHumoru kh;
  kh.dzis = "2026-11-04";
  kh.sceny = sceny();
  Wybor a = wybierzTryb(e, true, bez, kh), b = wybierzTryb(e, true, z, kh);
  SPRAWDZ(a.tryb == b.tryb && std::string(a.dlaczego) == b.dlaczego);
  ROWNE(std::string(a.dlaczego), "połowa wyświetleń w 7 dniach (R-HUM-23)");
}

// wymaganie: R-POR-23
// Portret jest ekranem dnia: przy linii `Co było inaczej? ↻ 2 s` przytrzymanie ↻ otwiera pytanie (mapa ekranu dnia),
// a krótkie ◀ i ↻ nic nie robią (R-POR-01).
static void testR_POR_23_liniaPytania() {
  KontekstNaw k;
  k.zaproszenie = true;
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, true, k).ekran == Ekran::PYTANIE);
  KontekstNaw k0;
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, k0).akcja == Akcja::NIC);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, false, k0).akcja == Akcja::NIC);
}

// wymaganie: R-POR-07
// wymaganie: R-POR-08
// wymaganie: R-POR-10
// wymaganie: R-POR-11
// wymaganie: R-POR-14
// wymaganie: R-POR-19
// Rysunek ze znaną odpowiedzią: dziura 03:05–03:20 z kropką śladu, biały odcinek bez kropek z obrysem, piki ASV
// (16,2 i 36 px), sąsiednie piki bez szczeliny przy pierścieniu, teksty bez wartości ustawień, tarcza przy linii
// uwagi w strefie 102–434.
static bool czarny(const Bufor& b, int x, int y) { return b.px[(size_t)y * 800 + (size_t)x] != 0; }
static void punktTarczy(int s, double r, int cy, int& x, int& y) {
  double a = (s % 43200) / 43200.0 * 2 * M_PI;
  x = 200 + (int)std::lround(r * std::sin(a));
  y = cy - (int)std::lround(r * std::cos(a));
}
static void testRysunekZnaneOdpowiedzi() {
  Przebieg p;
  p.data = "2026-11-04";
  p.sesje = {{700, 905}, {920, 1130}};  // 23:40–03:05 i 03:20–06:50
  auto odc = [&](int od, int w95, int ps) {
    Odcinek o;
    o.od = od;
    o.minuty = 5;
    o.wyciek95 = w95;
    o.wycS = w95 >= 15 ? 300 : 0;  // biały odcinek od 60 s próbek ≥ 15 L/min
    o.zew = 0;
    o.centr = 0;
    o.ps = ps;
    p.odcinki.push_back(o);
  };
  for (int m = 700; m < 1130; m += 5)
    if (m < 905 || m >= 920) odc(m, m >= 960 && m < 980 ? 18 : 5, m == 840 ? 70 : m == 845 ? 110 : 50);
  DanePortretu d = dane(p, "ASVAuto", 707);
  d.normaPs10 = 50;
  Bufor b;
  rysujCialoPortretu(b, d, 54, false);
  int x, y;
  // R-POR-07: pierścień w sesji czarny, dziura 03:05–03:20 bez pierścienia, z kropką śladu gdzieś w dziurze
  punktTarczy(800 * 60, 126, 244, x, y);
  SPRAWDZ(czarny(b, x, y));
  punktTarczy(912 * 60, 120, 244, x, y);
  SPRAWDZ(!czarny(b, x, y));
  bool kropka = false;
  for (int s = 906 * 60; s < 919 * 60; s += 10) {
    punktTarczy(s, 126, 244, x, y);
    kropka = kropka || czarny(b, x, y);
  }
  SPRAWDZ(kropka);
  // R-POR-08: odcinki 04:00–04:20 (300 s wycieku) białe w środku, obrys czarny, bez kropek śladu na promieniu 126
  bool bialeCale = true;
  for (int s = 960 * 60 + 30; s < 980 * 60 - 30; s += 10) {
    punktTarczy(s, 126, 244, x, y);
    bialeCale = bialeCale && !czarny(b, x, y);
  }
  SPRAWDZ(bialeCale);
  punktTarczy(970 * 60, 119, 244, x, y);
  SPRAWDZ(czarny(b, x, y));
  // R-POR-10: piki ASV 16,2 px (PS 7,0) i 36 px (PS 11,0) na środku odcinków 02:00 i 02:05
  punktTarczy(842 * 60 + 30, 134 + 15, 244, x, y);
  SPRAWDZ(czarny(b, x, y));
  punktTarczy(847 * 60 + 30, 134 + 35, 244, x, y);
  SPRAWDZ(czarny(b, x, y));
  punktTarczy(847 * 60 + 30, 134 + 38, 244, x, y);
  SPRAWDZ(!czarny(b, x, y));
  // R-POR-11: dwa sąsiednie piki bez szczeliny przy pierścieniu (granica odcinków 02:05)
  punktTarczy(845 * 60, 134 + 3, 244, x, y);
  SPRAWDZ(czarny(b, x, y));
  // R-POR-14: podpisy, legenda i liczby bez wartości w cmH2O
  for (auto& s : podpisy(d, false)) SPRAWDZ(s.find("cmH2O") == std::string::npos);
  for (auto& s : legenda(d)) SPRAWDZ(std::string(opisLegendy(s)).find("cmH2O") == std::string::npos);
  // R-POR-19: z linią uwagi tarcza w strefie 102–434 (żaden piksel tarczy poniżej 434 ani nad 102)
  Bufor bl;
  rysujCialoPortretu(bl, d, 102, true);
  bool poza = false;
  for (int yy = 0; yy < 800 * 480 / 800; yy++)
    for (int xx = 0; xx < 380; xx++)
      if (czarny(bl, xx, yy) && (yy < 102 || yy >= 434)) poza = true;
  SPRAWDZ(!poza);
  SPRAWDZ(podpisy(d, true).size() <= 2);
}

// ---------- znacznik startu okna zamiast napisu okna (R-POR-06, zmiana 2026-11-12 (2), decyzja właściciela projektu 12.11.2026) ----------
// Punkt w układzie znacznika: `a` wzdłuż promienia pod kątem początku okna `w`, `t` w poprzek (dodatnie zgodnie ze
// wskazówkami zegara); liczone niezależnie od kodu panelu (sin i cos w liczbach zmiennoprzecinkowych).
static void punktZnacznika(int w, double a, double t, int cy, int& x, int& y) {
  double k = (double)(((w % 720) + 720) % 720) * 0.5 * M_PI / 180.0;
  double X = a * std::sin(k) + t * std::cos(k), Y = a * std::cos(k) - t * std::sin(k);
  x = 200 + (int)std::lround(X);
  y = cy - (int)std::lround(Y);
}
static int skalowany(int r, int skala) { return (r * skala + 50) / 100; }
static bool czarnyZn(const Bufor& b, int w, double a, double t, int cy) {
  int x, y;
  punktZnacznika(w, a, t, cy, x, y);
  return czarny(b, x, y);
}
// czarny piksel w odległości najwyżej `tol` px od punktu (a, t) znacznika
static bool czarnyBlisko(const Bufor& b, int w, double a, double t, int cy, int tol) {
  int x0, y0;
  punktZnacznika(w, a, t, cy, x0, y0);
  for (int y = y0 - tol; y <= y0 + tol; y++)
    for (int x = x0 - tol; x <= x0 + tol; x++)
      if ((x - x0) * (x - x0) + (y - y0) * (y - y0) <= tol * tol && czarny(b, x, y)) return true;
  return false;
}
// Niezmienniki znacznika, prawdziwe przy każdej nocy (znacznik rysowany po pierścieniu i pikach): kreska czarna przez
// tor pierścienia, biała przerwa po obu stronach kreski w pierścieniu, rdzeń grota czarny po stronie ruchu wskazówek.
// `bezToru` (wyciek w pierwszym albo ostatnim odcinku okna): kreska tylko pod i nad pierścieniem, bez przerwy.
static int sprawdzZnacznikZawsze(const Bufor& b, int w, int cy, int skala, bool bezToru = false) {
  const double rIn = skalowany(118, skala), rOut = skalowany(134, skala), r2 = skalowany(150, skala);
  const double A = r2 - 7.5;
  int zle_ = 0;
  if (bezToru) {
    for (double a : {rIn - 3, rOut + 2, r2 - 4}) zle_ += !czarnyZn(b, w, a, 0, cy);
  } else {
    for (double a : {rIn + 1, (rIn + rOut) / 2, rOut - 1, r2 - 4}) zle_ += !czarnyZn(b, w, a, 0, cy);
    for (double a : {rIn + 2, (rIn + rOut) / 2, rOut - 2})
      for (double t : {-3.2, 3.2}) zle_ += czarnyZn(b, w, a, t, cy);
  }
  zle_ += !czarnyZn(b, w, A, 5.5, cy);
  zle_ += !czarnyBlisko(b, w, A, 13.5, cy, 2);  // czubek grota w tolerancji 2 px
  return zle_;
}
static Przebieg nocZnacznika(std::vector<Sesja> sesje) {
  Przebieg p;
  p.data = "2026-11-12";
  p.sesje = sesje;
  for (auto& s : sesje)
    for (int m = s.od / 5 * 5; m < s.do_; m += 5) {
      Odcinek o;
      o.od = m;
      o.minuty = 5;
      o.wyciek95 = 5;
      o.wycS = 0;
      o.zew = 0;
      o.centr = 0;
      p.odcinki.push_back(o);
    }
  return p;
}

// wymaganie: R-POR-06
// Środek tarczy bez napisu: piksele w kole o promieniu 96 (z linią uwagi 82) to same cyfry i kreski godzin, więc są
// te same przy każdym oknie (napis okna różniłby się) i nie ma ich bliżej środka niż 60 px (z linią uwagi 51).
static void testR_POR_06_srodekTarczyPusty() {
  const int okna[] = {540, 570, 680, 720, 600, 455};  // 21:00, 21:30, 23:20, 00:00, 22:00, 19:35
  for (bool linia : {false, true}) {
    const int gora = linia ? 102 : 54, cy = (gora + 434) / 2, skala = linia ? 85 : 100;
    const int rSrodka = skalowany(96, skala), rPusty = skalowany(60, skala);
    std::vector<uint8_t> pierwszy;
    for (int w : okna) {
      Przebieg p = nocZnacznika({{w + 150, w + 600}});
      DanePortretu d = dane(p, "AutoSet", w + 120);
      SPRAWDZ(okno(d) == w);
      Bufor b;
      rysujCialoPortretu(b, d, gora, linia);
      std::vector<uint8_t> srodek;
      int wPustym = 0, czarnychSrodka = 0;
      for (int y = cy - rSrodka; y <= cy + rSrodka; y++)
        for (int x = 200 - rSrodka; x <= 200 + rSrodka; x++) {
          int r2 = (x - 200) * (x - 200) + (y - cy) * (y - cy);
          if (r2 >= rSrodka * rSrodka) continue;
          srodek.push_back(b.px[(size_t)y * 800 + (size_t)x]);
          czarnychSrodka += b.px[(size_t)y * 800 + (size_t)x];
          if (r2 < rPusty * rPusty) wPustym += b.px[(size_t)y * 800 + (size_t)x];
        }
      SPRAWDZ(wPustym == 0);
      SPRAWDZ(czarnychSrodka > 0);  // cyfry i kreski są
      if (pierwszy.empty()) pierwszy = srodek;
      else SPRAWDZ(srodek == pierwszy);
    }
  }
}

// wymaganie: R-POR-06
// Znacznik startu okna pod kątem początku okna (21:00, 21:30, 23:20, 00:00), grot zgodnie ze wskazówkami zegara:
// na kropkowanym śladzie (sesja zaczyna się 1 h po starcie okna) i na pierścieniu (sesja przez start okna).
static void testR_POR_06_znacznikStartu() {
  for (bool linia : {false, true}) {
    const int gora = linia ? 102 : 54, cy = (gora + 434) / 2, skala = linia ? 85 : 100;
    const double r1 = skalowany(110, skala), r2 = skalowany(150, skala), rOut = skalowany(134, skala);
    const double A = r2 - 7.5;
    for (int w : {540, 570, 680, 720}) {
      // a) start okna na śladzie: noc od w + 60 min do w + 600 min
      Przebieg p = nocZnacznika({{w + 60, w + 600}});
      DanePortretu d = dane(p, "AutoSet", w + 120);
      SPRAWDZ(okno(d) == w);
      Bufor b;
      rysujCialoPortretu(b, d, gora, linia);
      SPRAWDZ(sprawdzZnacznikZawsze(b, w, cy, skala) == 0);
      SPRAWDZ(!czarnyZn(b, w, r1 - 4, 0, cy));        // kreska zaczyna się 8 px pod pierścieniem
      SPRAWDZ(!czarnyZn(b, w, r2 + 4, 0, cy));        // i kończy 16 px nad nim
      SPRAWDZ(!czarnyZn(b, w, A, -6, cy));            // grot tylko po stronie ruchu wskazówek
      SPRAWDZ(!czarnyZn(b, w, A, -11, cy));
      SPRAWDZ(!czarnyZn(b, w, A, 19, cy));            // czubek nie dalej niż 15 px od osi kreski (+ tolerancja)
      SPRAWDZ(!czarnyZn(b, w, rOut + 2, 10, cy));     // grot zwęża się ku czubkowi (trójkąt, nie prostokąt)
      SPRAWDZ(!czarnyZn(b, w, r2 - 1, 9, cy));
      // ten sam punkt 30° obok (o godzinę tarczy) jest pusty: znacznik stoi na starcie okna, nie obok
      SPRAWDZ(!czarnyZn(b, w + 60, A, 5.5, cy) && !czarnyZn(b, w - 60, A, 5.5, cy));
      // b) start okna na pierścieniu: wcześniejsza sesja przez start okna (noc zaczęta wcześniej niż zwykle)
      Przebieg q = nocZnacznika({{w - 20, w + 40}, {w + 100, w + 600}});
      DanePortretu e = dane(q, "AutoSet", w + 120);
      SPRAWDZ(okno(e) == w);
      Bufor c;
      rysujCialoPortretu(c, e, gora, linia);
      SPRAWDZ(sprawdzZnacznikZawsze(c, w, cy, skala) == 0);
      // pierścień za przerwą (zgodnie ze wskazówkami), przed startem okna go nie ma (promień 120, z dala od kropek śladu)
      SPRAWDZ(czarnyZn(c, w, skalowany(120, skala), 8, cy));
      SPRAWDZ(!czarnyZn(c, w, skalowany(120, skala), -8, cy));
    }
  }
}

// wymaganie: R-POR-06
static int widoczneWycieku(const Bufor& b, int w, int cy, int skala, long odS = -1, long doS = -1, bool scisle = false);
// wyciek w pierwszym albo ostatnim odcinku okna widoczny na rysunku (z danych, niezależnie od kodu panelu)
static bool wyciekPrzyStarcieDane(const Przebieg& p, int w) {
  for (auto& o : p.odcinki) {
    if (o.od != w && o.od != w + 715) continue;
    if (o.wyciek95 == BRAK || o.wycS == BRAK || o.wycS < 60) continue;
    for (auto& s_ : p.sesje)
      if (s_.od < o.od + 5 && s_.do_ > o.od) return true;
  }
  return false;
}
// Losowo 2400 nocy (tryb, okno, sesje, piki, wyciek, kropki ASV, z linią uwagi i bez): żaden piksel tarczy poza strefą
// (x < 380, y od góry treści do 434), w pasie 380–389 nic, niezmienniki znacznika zawsze spełnione.
static void testR_POR_06_losowo() {
  uint32_t ziarno = 20261112;
  auto los = [&](int n) {
    ziarno = ziarno * 1103515245u + 12345u;
    return (int)((ziarno >> 8) % (uint32_t)n);
  };
  const char* tryby[4] = {"CPAP", "AutoSet", "ASV", "ASVAuto"};
  int poza = 0, znacznik = 0, nocy = 0, zgodnoscWycieku = 0;
  for (int i = 0; i < 2400; i++) {
    Przebieg p;
    p.data = "2026-11-12";
    int start = 300 + los(600);  // 17:00–03:00
    int n = 1 + los(3), t = start;
    for (int k = 0; k < n && t < 1400; k++) {
      int dl = 20 + los(500), od = t, do_ = std::min(1439, t + dl);
      p.sesje.push_back({od, do_});
      t = do_ + 5 + los(120);
    }
    for (auto& s : p.sesje)
      for (int m = s.od / 5 * 5; m < s.do_; m += 5) {
        Odcinek o;
        o.od = m;
        o.minuty = 5;
        o.wyciek95 = los(30);
        o.wycS = los(4) == 0 ? los(300) : 0;
        o.zew = los(3) == 0 ? los(12) : 0;
        o.centr = los(5) == 0 ? los(10) : 0;
        o.ps = 40 + los(80);
        o.maks = los(6);
        p.odcinki.push_back(o);
        if (los(6) == 0) p.zdarzenia.push_back({m + los(5), true, false});
      }
    if (p.sesje.empty() || !jestPortret(p)) continue;
    std::sort(p.odcinki.begin(), p.odcinki.end(), [](const Odcinek& a, const Odcinek& b) { return a.od < b.od; });
    p.odcinki.erase(std::unique(p.odcinki.begin(), p.odcinki.end(), [](const Odcinek& a, const Odcinek& b) { return a.od == b.od; }),
                    p.odcinki.end());
    DanePortretu d = dane(p, tryby[los(4)], 360 + los(720));
    d.normaPs10 = los(4) ? 50 : BRAK;
    bool linia = los(3) == 0;
    const int gora = linia ? 102 : 54, cy = (gora + 434) / 2, skala = linia ? 85 : 100;
    Bufor b;
    rysujCialoPortretu(b, d, gora, linia);
    nocy++;
    for (int y = 0; y < 480; y++)
      for (int x = 0; x < 390; x++)
        if (czarny(b, x, y) && (x >= 380 || y < gora || y >= 434)) poza++;
    znacznik += sprawdzZnacznikZawsze(b, okno(d), cy, skala, wyciekPrzyStarcieDane(p, okno(d))) != 0;
    // legenda `wyciek` ⇔ na rysunku widoczny biały odcinek z czarnym obrysem
    {  // legenda `wyciek` ⇒ widoczny biały odcinek z obrysem; bez legendy ⇒ żadnego (ścisłe sprawdzenie)
      bool lw = maLegende(d, "wyciek");
      zgodnoscWycieku += lw ? widoczneWycieku(b, okno(d), cy, skala) == 0 : widoczneWycieku(b, okno(d), cy, skala, -1, -1, true) > 0;
    }
  }
  printf("  znacznik startu okna: %d nocy losowych, pikseli poza strefą %d, nocy z błędem znacznika %d, legenda wycieku "
         "niezgodna z rysunkiem %d\n", nocy, poza, znacznik, zgodnoscWycieku);
  SPRAWDZ(nocy >= 2000);
  SPRAWDZ(poza == 0);
  SPRAWDZ(znacznik == 0);
  SPRAWDZ(zgodnoscWycieku == 0);
}

// ---------- (12.11.2026) ----------
// Piksel wycieku widoczny: biały w paśmie bieli (promienie 121–131 w skali) i czarny obrys pierścienia na tym samym
// kierunku (promień 119,5 i 132,5 w skali). Biała przerwa znacznika przecina cały tor, więc obrys przy niej jest biały
// i takiego piksela nie liczy. `odS`, `doS`: tylko piksele z kątem tarczy w [odS, doS) sekund od startu okna (-1: każdy).
static int widoczneWycieku(const Bufor& b, int w, int cy, int skala, long odS, long doS, bool scisle) {
  const int bIn = skalowany(121, skala), bOut = skalowany(131, skala);
  const double rw = skalowany(119, skala) + 0.5, rz = skalowany(132, skala) + 0.5;
  int n = 0;
  for (int y = cy - bOut; y <= cy + bOut; y++)
    for (int x = 200 - bOut; x <= 200 + bOut; x++) {
      int dx = x - 200, dy = y - cy, r2 = dx * dx + dy * dy;
      if (r2 < bIn * bIn || r2 >= bOut * bOut || czarny(b, x, y)) continue;
      if (odS >= 0) {
        double k = std::atan2((double)dx, (double)-dy);
        if (k < 0) k += 2 * M_PI;
        long q = (long)(k / (2 * M_PI) * 43200.0);
        long u = ((q - (long)w * 60) % 43200 + 43200) % 43200;  // sekundy tarczy od startu okna
        if (u < odS || u >= doS) continue;
      }
      double r = std::sqrt((double)r2);
      bool obrys = true;
      // `scisle`: obrys czarny także na kierunkach obróconych o ±1,5 px (bez fałszywych trafień na krawędzi sesji)
      for (double dt : scisle ? std::vector<double>{-1.5, 0, 1.5} : std::vector<double>{0}) {
        double c = std::cos(dt / r), sn = std::sin(dt / r);
        double ex = (dx * c - dy * sn) / r, ey = (dx * sn + dy * c) / r;
        obrys = obrys && czarny(b, 200 + (int)std::lround(ex * rw), cy + (int)std::lround(ey * rw)) &&
                czarny(b, 200 + (int)std::lround(ex * rz), cy + (int)std::lround(ey * rz));
      }
      if (obrys) n++;
    }
  return n;
}
// obrys pierścienia (promienie rIn…bIn i bOut…rOut) w kątach [odS, doS) od startu okna: liczba pikseli białych
static int bialyObrys(const Bufor& b, int w, int cy, int skala, long odS, long doS) {
  const int rIn = skalowany(118, skala), rOut = skalowany(134, skala), bIn = skalowany(121, skala), bOut = skalowany(131, skala);
  int n = 0;
  for (int y = cy - rOut; y <= cy + rOut; y++)
    for (int x = 200 - rOut; x <= 200 + rOut; x++) {
      int dx = x - 200, dy = y - cy, r2 = dx * dx + dy * dy;
      if (r2 < rIn * rIn || r2 >= rOut * rOut || (r2 >= bIn * bIn && r2 < bOut * bOut)) continue;
      double k = std::atan2((double)dx, (double)-dy);
      if (k < 0) k += 2 * M_PI;
      long u = ((long)(k / (2 * M_PI) * 43200.0) - (long)w * 60 + 43200L * 2) % 43200;
      if (u >= odS && u < doS && !czarny(b, x, y)) n++;
    }
  return n;
}
// Najmniej widocznych pikseli wycieku w pierwszym odcinku okna (wnętrze odcinka, bez 10 s z każdej strony): łuk
// 5 min to 2,5°, pasmo bieli 10 px (z linią uwagi 8 px) na promieniu ok. 126 (107), więc pole ok. 1260 × 0,0436 = 55 px
// (z linią 856 × 0,0436 = 37 px), bez 2 × 10 s ok. 51 i 35 px; po rastrowaniu zmierzone najmniej 48 i 27 px (288
// startów okna). Próg 20 px: zapas ok. 25% pod najmniejszym zmierzonym polem z linią uwagi, a jednocześnie więcej niż
// resztki bieli przy znaczniku w torze (przerwa 8,5 px szerokości zabiera łuk odcinka 5,5 px, z linią 4,7 px).
static const int WYCIEK_MIN_PX = 20;

// wymaganie: R-POR-06
// wymaganie: R-POR-08
// wymaganie: R-POR-13
// 288 startów okna × z linią uwagi i bez, konstrukcja przypadku brzegowego (sesje {w − 20, w + 40}, {w + 100, w + 600},
// pierwszy odcinek okna z 300 s wycieku): legenda `wyciek` i podpis `Wyciek przez 5 min`, w pierwszym odcinku co
// najmniej WYCIEK_MIN_PX widocznych pikseli wycieku i obrys bez białego piksela; znacznik nadal widoczny poza torem.
static void testR_POR_06_wyciekPrzyStarcieOkna() {
  for (bool linia : {false, true}) {
    const int gora = linia ? 102 : 54, cy = (gora + 434) / 2, skala = linia ? 85 : 100;
    int sprawdzonych = 0, najmniej = 1 << 30;
    std::vector<bool> katy(144, false);
    for (int w = 0; w < 1440; w += 5) {
      std::vector<Sesja> ses;
      if (w + 40 <= 1439) ses.push_back({std::max(0, w - 20), w + 40});
      if (w + 100 < 1439) ses.push_back({w + 100, std::min(1439, w + 600)});
      if (ses.empty()) continue;
      Przebieg p = nocZnacznika(ses);
      for (auto& o : p.odcinki)
        if (o.od == w) o.wycS = 300, o.wyciek95 = 22;
      DanePortretu d = dane(p, "AutoSet", w + 120);
      if (okno(d) != w || !maLegende(d, "wyciek")) continue;
      SPRAWDZ(maPodpis(d, "Wyciek przez 5 min", linia));
      Bufor b;
      rysujCialoPortretu(b, d, gora, linia);
      int n = widoczneWycieku(b, w, cy, skala, 10, 290);
      najmniej = std::min(najmniej, n);
      SPRAWDZ(n >= WYCIEK_MIN_PX);
      SPRAWDZ(bialyObrys(b, w, cy, skala, 10, 290) == 0);
      // znacznik poza torem: część kreski pod pierścieniem i rdzeń grota
      SPRAWDZ(czarnyZn(b, w, skalowany(114, skala), 0, cy) && czarnyZn(b, w, skalowany(150, skala) - 7.5, 5.5, cy));
      katy[(size_t)((w % 720) / 5)] = true;
      sprawdzonych++;
    }
    printf("  wyciek na starcie okna (linia %d): %d okien, najmniej %d widocznych pikseli wycieku (próg %d)\n", linia,
           sprawdzonych, najmniej, WYCIEK_MIN_PX);
    SPRAWDZ(sprawdzonych >= 250);
    SPRAWDZ(std::count(katy.begin(), katy.end(), true) == 144);  // każdy kąt tarczy
  }
}

// wymaganie: R-POR-06
// szerokość kreski 4–5 px: przy oknie od 00:00 (kreska pionowa) i 03:00 (pozioma) część pod pierścieniem ma
// dokładnie 5 czarnych pikseli w poprzek; przy każdym z 144 kątów pole części pod pierścieniem / jej długość w 3,5–5,6.
static void testR_POR_06_szerokoscKreski() {
  for (bool linia : {false, true}) {
    const int gora = linia ? 102 : 54, cy = (gora + 434) / 2, skala = linia ? 85 : 100;
    const int r1 = skalowany(110, skala), rIn = skalowany(118, skala), rs = (r1 + rIn) / 2;
    for (int w = 720; w < 1440; w += 5) {
      Przebieg p = nocZnacznika({{w + 60, std::min(1439, w + 600)}});
      DanePortretu d = dane(p, "AutoSet", w + 120);
      if (okno(d) != w) continue;
      Bufor b;
      rysujCialoPortretu(b, d, gora, linia);
      if (w == 720 || w == 900) {  // 00:00 i 03:00
        int bieg = 0;
        for (int i = -8; i <= 8; i++) bieg += w == 720 ? czarny(b, 200 + i, cy - rs) : czarny(b, 200 + rs, cy + i);
        SPRAWDZ(bieg == 5);
      }
      int pole = 0;
      const double k = (double)(w % 720) * 0.5 * M_PI / 180.0;
      for (int y = cy - rIn; y <= cy + rIn; y++)
        for (int x = 200 - rIn; x <= 200 + rIn; x++) {
          double X = x - 200, Y = cy - y, r = std::sqrt(X * X + Y * Y);
          double a = X * std::sin(k) + Y * std::cos(k), t = X * std::cos(k) - Y * std::sin(k);
          if (r >= r1 + 1 && r < rIn - 1 && a > 0 && std::fabs(t) < 10 && czarny(b, x, y)) pole++;
        }
      double szer = (double)pole / (double)(rIn - 1 - (r1 + 1));
      if (szer < 3.5 || szer > 5.6) printf("    szerokość kreski %.2f px (w %d, linia %d)\n", szer, w, linia);
      SPRAWDZ(szer >= 3.5 && szer <= 5.6);
    }
  }
}

void testyEkranuPortretu(const std::string& k) {
  testR_POR_17_odcinekDwochPlikow();
  testR_POR_21_zakresyZapisuIOdczytu();
  testR_POR_09_centralnePonad500();
  testR_POR_06_oknoNaNajdluzszejSesji();
  testR_POR_13_legendaWycieku();
  testR_POR_08_wyciekZJednegoZrodla();
  testR_POR_26_maksimumNieznane();
  testR_POR_25_nocNiepewna();
  testR_POR_15_jedenKomunikat();
  testR_POR_27_brakiNigdyNieWypadaja();
  testT_WYM_98_pamiecZbieracza();
  testR_POR_20_poraLimitow();
  testR_POR_23_liniaPytania();
  testRysunekZnaneOdpowiedzi();
  testR_POR_06_srodekTarczyPusty();
  testR_POR_06_znacznikStartu();
  testR_POR_06_losowo();
  testR_POR_06_wyciekPrzyStarcieOkna();
  testR_POR_06_szerokoscKreski();
  testR_POR_22_katTarczy();
  testR_POR_12_tekstyWLimitach();
  testR_POR_16_nocBezTerapii();
  czasRysowania();
  if (k.empty()) return;
  int n = 0;
  for (auto& f : pliki(k + "/wzorzec"))
    if (f.size() == 22 && f.compare(f.size() - 4, 4, ".pbm") == 0) porownajZWzorcem(k + "/wzorzec", f.substr(8, 10), n);
  for (auto& f : pliki(k + "/wzorzec-linia"))
    if (f.size() == 22 && f.compare(f.size() - 4, 4, ".pbm") == 0) porownajZWzorcem(k + "/wzorzec-linia", f.substr(8, 10), n);
  printf("  obrazków porównanych ze wzorcem: %d\n", n);
  SPRAWDZ(n >= 8);
}
