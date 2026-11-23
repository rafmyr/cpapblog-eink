// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe programu „Dobry start” w wersji 2 (spec 5.8.14.14, R-DS-54…R-DS-71). Test oznaczony
// `przypadek n` to przypadek dawnej usterki: CZERWONY, gdyby wróciła.
// Pozostałe testy to niezależne sprawdzenia, które trzymają (fazy, limity, restart, zegar, 100 dób, migracja).
// Uruchomienie: tools/prototyp/testy.sh (wynik nie blokuje skryptu: `|| echo`).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "dobry_start.h"
#include "ekrany.h"
#include "ekrany_kontekst.h"
#include "magazyn.h"
#include "qr.h"
#include "reguly.h"
#include "wspolne.h"

using namespace dobry_start;

static int ok = 0, zle = 0;
static std::set<std::string> czerwone;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
// usterka z raportu: nie blokuje innych sprawdzeń, ale wynik całości jest czerwony
#define USTERKA(rv, w)                                                                  \
  do {                                                                                  \
    if (w) ok++;                                                                        \
    else { zle++; czerwone.insert(rv); printf("USTERKA %s (%s:%d): %s\n", rv, __FILE__, __LINE__, #w); } \
  } while (0)

static const std::string START = "2027-01-04";
static const Katalog& K() { return katalogWbudowany(); }
static Ustawienia wl() {
  Ustawienia u;
  u.program = true;
  return u;
}
static std::string dataDoby(int n) { return dodajDni(START, n - 1); }
static Doba db(int n, std::set<std::string> tok = {}, Tryb t = Tryb::CPAP, bool terapia = true) {
  Doba d;
  d.dzis = dataDoby(n);
  d.tryb = t;
  d.tokeny = tok;
  d.nocZTerapia = terapia;
  return d;
}
static const Porada* P(const std::string& i) { return K().porada(i); }
static std::string id(const WyborPorady& w) { return w.porada ? w.porada->id : ""; }
static std::pair<std::string, std::string> doba2(Stan& s, const Ustawienia& u, const Doba& d) {
  WyborPorady r = wybierz(K(), s, u, d);
  if (r.porada) zapiszPokazanie(K(), s, r, d.dzis, r.tylkoWieczor, r.telefon);
  WyborPorady e = wybierzWieczor(K(), s, u, d);
  std::string w;
  if (e.porada && e.porada->faza == Faza::START_WIECZOR && e.zrodlo == Zrodlo::PLAN && !e.narysowana) {
    zapiszPokazanie(K(), s, e, d.dzis, true, e.telefon);
    w = e.porada->id;
  }
  return {id(r), w};
}
static int progMin(const char* p) {
  const reguly::Prog* x = reguly::zestawWbudowany().prog(p, reguly::Tryb::NIEZNANY);
  return x ? (int)std::lround(x->jednostka == "h" ? x->wartosc * 60 : x->wartosc) : 0;
}
static ProgiPochwal progi() {
  ProgiPochwal p;
  p.terapia = progMin("R-SKI-27.noc_z_terapia_m01");
  p.m01Min = progMin("R-DANE-13.m01_min");
  p.wyciekUwaga = progMin("S-K-10.uwaga_o_masce");
  p.noceProblem = progMin("E-10.noce_problem");
  return p;
}
// seria nocy ≥ 4 h liczona niezależnie (jak M-20: od nocy z poranka `dzis` wstecz)
static int seriaNocy(const std::vector<NocProgramu>& n, const std::string& dzis) {
  int s = 0;
  for (int i = 0; i < 400; i++) {
    std::string d = dodajDni(dzis, -i);
    bool jest = false;
    for (auto& x : n) jest = jest || (x.data == d && x.m01 >= 240);
    if (!jest) break;
    s++;
  }
  return s;
}
static std::set<std::string> tokeny(const std::vector<NocProgramu>& n, const std::string& dzis, std::set<std::string> t = {}) {
  int s = seriaNocy(n, dzis);
  tokenyPochwal(n, dzis, s, progi(), t);
  t.insert("M-20/" + std::to_string(s));
  return t;
}
static bool pochwala(const std::string& i) { return P(i) && P(i)->rodzaj == Rodzaj::POCHWALA; }

// przypadek 1: R-DS-60: po nocy niepewnej, z E-20 albo S-K-12 żadnej pochwały tego dnia (nie tylko za szczelność)
static void testPrzypadek1_pochwalaPoNocyNiepewnej() {
  Ustawienia u = wl();
  for (const char* zly : {"E-20", "noc_niepewna", "S-K-12"}) {
    Stan s;
    doba2(s, u, db(1));
    // pierwsza taka noc w starcie zajmuje porada z danych DS-Z13 (S-K-12, noc niepewna); druga w 7 dniach już nie
    std::set<std::string> t = {"pochwala/pierwsza_noc", "pochwala/pierwsze_4h", zly};
    std::string dni;
    bool jest = false;
    for (int n = 2; n <= 6; n++) {
      std::string x = doba2(s, u, db(n, t)).first;
      dni += " " + (x.empty() ? std::string("-") : x);
      jest = jest || pochwala(x);
    }
    printf("  przypadek 1 %s, doby 2…6:%s\n", zly, dni.c_str());
    USTERKA("przypadek 1", !jest);
  }
  // adaptacja: pochwała serii po nocy niepewnej
  Stan a;
  for (int n = 1; n <= 19; n++) doba2(a, u, db(n));
  std::string d20 = doba2(a, u, db(20, {"M-20/14", "noc_niepewna", "E-20"})).first;
  printf("  przypadek 1 adaptacja, M-20/14 + noc niepewna + E-20: %s\n", d20.c_str());
  USTERKA("przypadek 1", !pochwala(d20));
}

// stan pliku wersji 1 w 17. dobie (kolejka 17, 16 wpisów P), jak na panelach z wersją 1
static std::vector<std::string> stanV1() {
  std::vector<std::string> w = {"S;start;" + START, "S;kolejka;17", "S;stan;aktywny", "S;przerwa_dni;0", "S;d0_z_rzedu;0",
                                "S;tryb;CPAP", "S;ponad;0", "S;noce_terapii;15", "S;ankieta_od;" + dataDoby(4),
                                "S;doba;" + dataDoby(16), "S;plan_koniec;0"};
  const char* ids[] = {"DS-D01", "DS-D02", "DS-D03", "DS-D04", "DS-D05", "DS-D06", "DS-D07", "DS-D08",
                       "DS-D09", "DS-D10", "DS-D11", "DS-D12", "DS-D13", "DS-D14", "DS-D45", "DS-D16"};
  for (int i = 0; i < 16; i++) w.push_back(std::string("P;") + dataDoby(i + 1) + ";" + ids[i] + ";plan;;dzien;panel");
  return w;
}

// przypadek 2: pochwały „pierwszej nocy” (DS-P01, DS-P02, DS-P06) po migracji z wersji 1 i przy `start.dzien` > 1
static void testPrzypadek2_pierwszaNocPoMigracji() {
  Ustawienia u = wl();
  Stan s = stanZWierszy(stanV1(), K());
  std::vector<NocProgramu> noce;
  for (int n = 1; n <= 30; n++) noce.push_back({dataDoby(n), 300 + (n % 2) * 90, n % 2, 5});  // co druga 6 h 30 bez zdjęcia
  std::vector<std::string> pierwsze;
  for (int n = 17; n <= 40; n++) {
    std::string r = doba2(s, u, db(n, tokeny(noce, dataDoby(n)))).first;
    if (r == "DS-P01" || r == "DS-P02" || r == "DS-P06") pierwsze.push_back(r + " w dobie " + std::to_string(n));
  }
  for (auto& x : pierwsze) printf("  przypadek 2 po migracji (17. noc terapii i dalej): %s\n", x.c_str());
  USTERKA("przypadek 2", pierwsze.empty());
  // `Który to dzień terapii?` = 30: pacjent ma za sobą 29 nocy
  Ustawienia u30 = wl();
  u30.dzien = 30;
  Stan t;
  std::vector<std::string> p30;
  for (int n = 1; n <= 20; n++) {
    std::string r = doba2(t, u30, db(n, tokeny(noce, dataDoby(n)))).first;
    if (r == "DS-P01" || r == "DS-P02" || r == "DS-P06") p30.push_back(r);
  }
  for (auto& x : p30) printf("  przypadek 2 start.dzien=30: %s\n", x.c_str());
  USTERKA("przypadek 2", p30.empty());
}

// przypadek 3: pochwała zaległa (czeka 1 dobę, R-DS-59) pokazuje liczby DZISIEJSZEJ nocy: „wyciek 34 L/min, poniżej granicy”
static void testPrzypadek3_zaleglaPochwalaZDzisiejszymiLiczbami() {
  Ustawienia u = wl();
  ProgiPochwal pr = progi();
  Stan s;
  std::vector<NocProgramu> noce;
  int duzy = pr.wyciekUwaga + 10;
  for (int n = 1; n <= 7; n++) {
    noce.push_back({dataDoby(n), 300, 1, n == 3 ? duzy : 4});
    doba2(s, u, db(n, n == 3 ? std::set<std::string>{"S-K-10", "S-K-10/opisowy"} : std::set<std::string>{}));
  }
  noce.push_back({dataDoby(8), 300, 1, 4});  // 7. noc z rzędu, szczelna: DS-P04 (karta) wygrywa, DS-P07 czeka 1 dobę
  std::string d8 = doba2(s, u, db(8, {"M-20/7", "pochwala/szczelna"})).first;
  noce.push_back({dataDoby(9), 300, 1, duzy});  // noc z wyciekiem ponad próg uwagi (S-K-10 opisowy; DS-Z01 była w dobie 3)
  WyborPorady w = wybierz(K(), s, u, db(9, {"S-K-10", "S-K-10/opisowy", "M-20/8"}));
  std::string tekst = w.porada ? podstawZnaczniki(K().tekst(w.porada->tresc), znacznikiNocy(noce, dataDoby(9), s.start, 8, pr))
                               : "";
  printf("  przypadek 3: doba 8 %s, doba 9 %s (belka „po nocy %s”): %s\n", d8.c_str(), id(w).c_str(), ddmm(w.dataNocy).c_str(),
         tekst.substr(0, 80).c_str());
  USTERKA("przypadek 3", id(w) != "DS-P07" || tekst.find("wyciek " + std::to_string(duzy) + " L/min") == std::string::npos);
  // ta sama pochwała zaległa w dobie BEZ rekordu nocy (AirBridge nie dał nocy): staje z `–` w miejscu liczby
  Stan b;
  std::vector<NocProgramu> nb;
  for (int n = 1; n <= 7; n++) {
    nb.push_back({dataDoby(n), 300, 1, n == 3 ? duzy : 4});
    doba2(b, u, db(n, n == 3 ? std::set<std::string>{"S-K-10", "S-K-10/opisowy"} : std::set<std::string>{}));
  }
  nb.push_back({dataDoby(8), 300, 1, 4});
  doba2(b, u, db(8, {"M-20/7", "pochwala/szczelna"}));
  Doba bez = db(9, {"M-20/0"}, Tryb::CPAP, false);
  bez.bezNocy = false;
  WyborPorady wb = wybierz(K(), b, u, bez);
  std::string tb = wb.porada ? podstawZnaczniki(K().tekst(wb.porada->tresc), znacznikiNocy(nb, dataDoby(9), b.start, 0, pr)) : "";
  printf("  przypadek 3 doba 9 bez nocy: %s: %s\n", id(wb).c_str(), tb.substr(0, 60).c_str());
  USTERKA("przypadek 3", !pochwala(id(wb)));
}

// przypadek 4: fail-path R-DS-59: pochwała i porada z danych tej samej nocy → rano porada z danych, pochwała czeka 1 dobę
static void testPrzypadek4_pochwalaCzekaPoPoradzieZDanych() {
  Ustawienia u = wl();
  Stan s;
  for (int n = 1; n <= 7; n++) doba2(s, u, db(n));
  std::string d8 = doba2(s, u, db(8, {"M-20/7", "E-11"})).first;  // 7 nocy z rzędu, rano porada do zdarzenia
  std::string d9 = doba2(s, u, db(9, {"M-20/8"})).first;
  printf("  przypadek 4: doba 8 %s, doba 9 %s (oczekiwane DS-P04)\n", d8.c_str(), d9.c_str());
  USTERKA("przypadek 4", d9 == "DS-P04");
  Stan a;  // adaptacja: DS-P09 (14 nocy) w dniu porady z danych
  for (int n = 1; n <= 20; n++) doba2(a, u, db(n));
  std::string a21 = doba2(a, u, db(21, {"M-20/14", "E-11"})).first;
  std::string a22 = doba2(a, u, db(22, {"M-20/15"})).first;
  printf("  przypadek 4 adaptacja: doba 21 %s, doba 22 %s (oczekiwane DS-P09)\n", a21.c_str(), a22.c_str());
  USTERKA("przypadek 4", a22 == "DS-P09");
}

// Znaczniki danych policzone niezależnie (tabela 4.3) na 500 losowych tygodniach nocy
static void testZnacznikiNiezaleznie() {
  ProgiPochwal pr = progi();
  unsigned los = 12345;
  auto r = [&](int m) { los = los * 1103515245u + 12345u; return (int)((los >> 8) % (unsigned)m); };
  int rozne = 0;
  for (int i = 0; i < 500; i++) {
    std::vector<NocProgramu> n;
    std::string dzis = dataDoby(20);
    for (int d = 12; d >= 0; d--) {
      if (r(4) == 0) continue;  // brak rekordu
      int m01 = r(5) == 0 ? 0 : r(5) == 0 ? 1439 : r(700);
      n.push_back({dodajDni(dzis, -d), m01, r(3), r(40)});
    }
    Znaczniki z = znacznikiNocy(n, dzis, START, 0, pr);
    int zm = 0, z4 = 0, suma = 0, ile = 0, czas = -1;
    bool sa = false;
    const NocProgramu* dz = nullptr;
    const NocProgramu* pop = nullptr;
    for (auto& x : n) {
      int odl = dniMiedzy(x.data, dzis);
      if (x.data == dzis) { dz = &x; czas = x.m01; }
      if (x.data < dzis && x.m01 >= std::max(1, pr.terapia) && (!pop || x.data > pop->data)) pop = &x;
      if (odl < 0 || odl >= 7) continue;
      sa = true;
      if (x.m01 >= pr.terapia) { zm++; suma += x.m01; ile++; }
      if (x.m01 >= pr.m01Min) z4++;
    }
    int sr = ile ? (int)std::lround((double)suma / ile) : -1;
    int dl = dz && pop && dz->m01 >= pop->m01 ? dz->m01 - pop->m01 : -1;
    bool zgodne = z.czas == czas && z.nocyZMaska == (sa ? zm : -1) && z.nocy4h == (sa ? z4 : -1) &&
                  std::abs(z.srednio - sr) <= (sr < 0 ? 0 : 1) && z.oIleDluzej == dl;
    if (!zgodne) rozne++;
  }
  printf("  znaczniki: 500 tygodni, niezgodnych z niezależnym liczeniem: %d\n", rozne);
  SPRAWDZ(rozne == 0);
  Znaczniki z;
  z.czas = 1439;
  z.srednio = 1439;
  z.oIleDluzej = 1439;
  z.nocyZRzedu = 999;
  SPRAWDZ(podstawZnaczniki("{czas}|{srednio}|{o_ile_dluzej}|{noce_z_rzedu}", z) == "23 h 59|23 h 59|23 h 59|999");
  z.czas = 0;
  SPRAWDZ(podstawZnaczniki("{czas}", z) == "0 h 00");  // tylko informacyjnie: pochwały wymagają M-01 ≥ progu terapii
}

// 100 dób bez wyłączenia: liczby porad na dobę według faz, koniec po DS-U02, nic po końcu
static void test100Dob() {
  Ustawienia u = wl();
  Stan s;
  int zleStart = 0, zleAdapt = 0, poKoncu = 0, utrwPlanPozaNominalna = 0;
  std::set<int> nominalne = {43, 46, 50, 53, 57, 60, 64, 67, 71, 74, 78, 84};
  for (int n = 1; n <= 100; n++) {
    auto x = doba2(s, u, db(n));
    int ile = 0, rano = 0;
    for (auto& p : s.p)
      if (p.data == dataDoby(n)) { ile++; rano += p.pora == "dzien"; }
    if (n <= 10 && (ile > 2 || rano > 1)) zleStart++;
    if (n > 10 && n <= 84 && ile > 1) zleAdapt++;
    if (n > 84 && ile > 0) poKoncu++;
    if (n > 42 && n <= 84 && !x.first.empty() && P(x.first)->rodzaj == Rodzaj::PLAN && !nominalne.count(n))
      utrwPlanPozaNominalna++;
  }
  bool u02 = false;
  for (auto& p : s.p) u02 = u02 || (p.id == "DS-U02" && p.data == dataDoby(84));
  printf("  100 dób: start >2 %d, adaptacja/utrwalenie >1 %d, po końcu %d, plan utrwalenia poza dobą nominalną %d, "
         "DS-U02 w dobie 84 %d, stan %s\n", zleStart, zleAdapt, poKoncu, utrwPlanPozaNominalna, (int)u02, s.stan.c_str());
  SPRAWDZ(zleStart == 0 && zleAdapt == 0 && poKoncu == 0 && utrwPlanPozaNominalna == 0 && u02 && s.stan == "zakonczony");
}

// restart, dwa wybudzenia w minucie, zegar cofnięty i przesunięty, „Zacznij od nowa” wieczorem w starcie
static void testCzasIRestart() {
  Ustawienia u = wl();
  Stan s;
  for (int n = 1; n <= 4; n++) doba2(s, u, db(n));
  WyborPorady a = wybierz(K(), s, u, db(5)), b = wybierz(K(), s, u, db(5));
  SPRAWDZ(id(a) == id(b) && !id(a).empty());
  zapiszPokazanie(K(), s, a, dataDoby(5), false, false);
  Stan r = stanZWierszy(wierszeStanu(s), K());  // restart między poradą poranną a wieczorną
  WyborPorady e1 = wybierzWieczor(K(), r, u, db(5));
  zapiszPokazanie(K(), r, e1, dataDoby(5), true, false);
  Stan r2 = stanZWierszy(wierszeStanu(r), K());
  WyborPorady e2 = wybierzWieczor(K(), r2, u, db(5));
  zapiszPokazanie(K(), r2, e2, dataDoby(5), true, false);
  int ile = 0;
  for (auto& x : r2.p) ile += x.data == dataDoby(5);
  SPRAWDZ(id(e1) == id(e2) && e2.narysowana && ile == 2 && id(wybierz(K(), r2, u, db(5))) == id(a));
  // zegar cofnięty o 3 doby: bez nowej porady i bez wpisu
  size_t przed = r2.p.size();
  doba2(r2, u, db(2));
  SPRAWDZ(r2.p.size() == przed);
  // zegar skoczył o 200 dób do przodu, potem wrócił: zachowanie do opisu (program zakończony na stałe)
  Stan z = r2;
  doba2(z, u, db(205));
  doba2(z, u, db(6));
  printf("  zegar +200 dób i z powrotem: stan %s, porada doby 6: %s\n", z.stan.c_str(), doba2(z, u, db(7)).first.c_str());
  // „Zacznij od nowa” wieczorem w starcie: dziś bez porady wieczornej, jutro doba 1
  Stan o = nowyStan(u, K());
  o.odNowa = dataDoby(6);
  WyborPorady w = wybierzWieczor(K(), o, u, db(6));
  SPRAWDZ(!w.porada && o.start == dataDoby(7));
  SPRAWDZ(doba2(o, u, db(7)).first == "DS-D01");
}

// R-DS-67: kopia `.v1` bajt w bajt, nienadpisana przy drugim starcie
static void testMigracjaKopia() {
  Dziennik dz;
  AtrapaPlikow pl(dz);
  pl.montujeSie = pl.montowane = true;
  std::string t = "#cpap-eink dobry_start 1\nrodzaj;a;b;c;d;e;f\n";
  std::vector<std::string> v1 = stanV1();
  for (auto& x : v1) {
    std::string w = x;
    int pol = 1;
    for (char ch : w) pol += ch == ';';
    while (pol++ < 7) w += ";";
    t += w + "\n";
  }
  t += "#koniec " + std::to_string(v1.size()) + "\n";
  pl.pliki["/dane/dobry_start.csv"] = t;
  {
    Magazyn m(pl, [](const std::string&) {});
    m.czytaj("dobry_start", "/dane/dobry_start.csv");
    SPRAWDZ(m.migrujWMiejscu());
  }
  std::string kopia;
  for (auto& kv : pl.pliki)
    if (kv.first.rfind("/dane/dobry_start.csv.v1", 0) == 0) kopia = kv.second;
  SPRAWDZ(kopia == t);
  pl.pliki["/dane/dobry_start.csv"] += "P;" + dataDoby(17) + ";DS-D48;plan;;dzien;panel\n";
  {
    Magazyn m(pl, [](const std::string&) {});
    m.czytaj("dobry_start", "/dane/dobry_start.csv");
    m.migrujWMiejscu();
  }
  std::string kopia2;
  for (auto& kv : pl.pliki)
    if (kv.first.rfind("/dane/dobry_start.csv.v1", 0) == 0) kopia2 = kv.second;
  SPRAWDZ(kopia2 == t);
}

// Kod QR: strefa ciszy wolna od tekstu i stopki, zrzut PBM każdej porady z kodem do dekodowania własnym dekoderem
// (tools/test_dobry_start_v2_brzegowe.py, zxing-cpp); katalog z BRZEG2_QR_ZRZUT
struct PlotnoT : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)(y * 800 + x)] = c;
  }
  int obszar(int x0, int y0, int x1, int y1) const {  // [x0, x1) × [y0, y1)
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[(size_t)(y * 800 + x)];
    return n;
  }
};
static void testQrStrefa() {
  const char* kat = getenv("BRZEG2_QR_ZRZUT");
  int kody = 0, zlaStrefa = 0, tekstWKolumnie = 0;
  for (auto& p : K().porady) {
    if (!p.qr.dl) continue;
    kody++;
    WyborPorady w;
    w.porada = &p;
    w.zrodlo = p.rodzaj == Rodzaj::POCHWALA ? Zrodlo::POCHWALA : Zrodlo::PLAN;
    w.dataNocy = dataDoby(3);
    DanePorady d;
    d.o = opis(K(), Stan(), w);
    Znaczniki z;  // najdłuższe wartości (tabela 4.3)
    z.czas = z.srednio = z.oIleDluzej = z.srednioProg = 1439;
    z.nocyZRzedu = z.nocyZMaskaProg = 999;
    z.wyciek = 99;
    z.nocyZMaska = z.nocy4h = 7;
    for (std::string* t : {&d.o.tytul, &d.o.tresc, &d.o.krok}) *t = podstawZnaczniki(*t, z);
    d.belka = Belka(NAZWA_BELKI, d.o.srodekBelki);
    d.stopka.lewo = "6 h 38 z maską · wyciek 99 l/min · drzemka 23 h 59";
    d.stopka.prawo = "za oknem -88,8° · przy łóżku 88,8°";
    PlotnoT pl;
    bool qr = rysujPorade(pl, d);
    int bok = qr::koduj(d.o.qr).rozmiar * 7;
    int x0 = 772 - bok, y0 = 386 - bok;  // przypadek 15: kod o 1 px w lewo (prawa krawędź x 771)
    // strefa ciszy 28 px wokół kodu (prawa krawędź: do brzegu ekranu)
    int s = pl.obszar(x0 - 28, y0 - 28, 800, y0) + pl.obszar(x0 - 28, y0, x0, 386) + pl.obszar(772, y0, 800, 386) +
            pl.obszar(x0 - 28, 386, 800, 414);
    if (!qr || s) { zlaStrefa++; printf("  QR %s: strefa ciszy %d px tuszu, rysunek %d\n", p.id.c_str(), s, (int)qr); }
    if (pl.obszar(541, 154, 569, 414)) tekstWKolumnie++;
    if (kat) {
      std::string f = std::string(kat) + "/" + p.id + ".pbm";
      FILE* o = fopen(f.c_str(), "wb");
      if (o) {
        fprintf(o, "P1\n800 480\n");
        for (int y = 0; y < 480; y++) {
          for (int x = 0; x < 800; x++) fputc(pl.px[(size_t)(y * 800 + x)] ? '1' : '0', o);
          fputc('\n', o);
        }
        fclose(o);
      }
    }
  }
  printf("  QR: %d porad z kodem, zła strefa ciszy %d, tekst w pasie x 541…568 %d; prawa strefa ciszy do brzegu: %d px\n",
         kody, zlaStrefa, tekstWKolumnie, 800 - 772);
  SPRAWDZ(kody == 27 && zlaStrefa == 0 && tekstWKolumnie == 0);
}

int main() {
  testQrStrefa();
  testPrzypadek1_pochwalaPoNocyNiepewnej();
  testPrzypadek2_pierwszaNocPoMigracji();
  testPrzypadek3_zaleglaPochwalaZDzisiejszymiLiczbami();
  testPrzypadek4_pochwalaCzekaPoPoradzieZDanych();
  testZnacznikiNiezaleznie();
  test100Dob();
  testCzasIRestart();
  testMigracjaKopia();
  std::string c;
  for (auto& x : czerwone) c += " " + x;
  printf("testy_dobry_start_v2_brzegowe: %d OK, %d BŁĘDÓW%s%s\n", ok, zle, c.empty() ? "" : "; czerwone (usterki):", c.c_str());
  return zle ? 1 : 0;
}
