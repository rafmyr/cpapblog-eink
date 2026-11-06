// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: silnik reguł (src/reguly.h kontrakt, src/reguly.cpp, src/reguly_pomoc.h,
// src/reguly_pamiec.h, src/reguly_sygnaly.h; spec techniczna 9.3, ADR-0023, ADR-0029).
// Napisane przed kodem. Dane wyłącznie syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_silnik_regul).
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "magazyn.h"
#include "noc.h"
#include "reguly.h"
#include "reguly_pamiec.h"
#include "reguly_pomoc.h"
#include "reguly_sygnaly.h"
#include "stany.h"
#include "wspolne.h"

using namespace reguly;

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

// ---------- budowa zestawu testowego ----------
static std::string plikZ(const std::string& rodzaj, const std::string& cialo, const std::string& zestaw = "t1",
                         const std::string& pochodzenie = "wbudowany") {
  return "#cpap-eink " + rodzaj + " 1 zestaw=" + zestaw + " pochodzenie=" + pochodzenie +
         " firmware=1.0.0..1.99.99 tryby=CPAP,AutoSet,ASV,ASVAuto zamrozenie=brak zrodlo=brak skrot=sha256:" +
         sha256Hex(cialo) + "\n" + cialo;
}

static const char* PROGI_T =
    "id_progu;wartosc;jednostka;tryb\n"
    "E-30.ahi;5;/h;wszystkie\n"
    "E-30.okno;7;noce;wszystkie\n"
    "R-SKI-27.noc_z_terapia_m01;15;min;wszystkie\n"
    "R-NOR-10.min_noce;14;noce;wszystkie\n"
    "T.kzn;3/7;noce (k z n);wszystkie\n"
    "T.asv;2;/h;ASV,ASVAuto\n"
    "T.godzina;18:00;godzina;wszystkie\n"
    "T.n3;3;noce;wszystkie\n"
    "T.delta;3;/h;wszystkie\n"
    "T.razy;2;x;wszystkie\n"
    "T.min;5;noce;wszystkie\n"
    "T.max;10;noce;wszystkie\n"
    "T.kzn24;2/4;tygodnie (k z n);wszystkie\n"
    "T.a;7;noce;wszystkie\n"
    "T.b;14;noce;wszystkie\n"
    "T.udzial;70;%;wszystkie\n"
    "T.m01_4h;240;min;wszystkie\n"
    "T.pol;1,5;/h;wszystkie\n"
    "T.h4;4;h;wszystkie\n"
    "T.s600;600;s;wszystkie\n"
    "T.min300;300;min;wszystkie\n"
    "T.lmin24;24;L/min;wszystkie\n"
    "T.cm2;2;cmH2O;wszystkie\n"
    "T.proc10;10;%;wszystkie\n"
    "T.hm1;-1;h;wszystkie\n";

static const char* TEKSTY_T =
    "id_tekstu;rola;limit;tekst\n"
    "E-30.tytul;tytul;30;Noce z AHI powyżej {prog:E-30.ahi}\n"
    "E-30.powod;powod;70;AHI powyżej {prog:E-30.ahi} w {n} z {prog:E-30.okno} nocy, ostatnio {x}.\n"
    "T.linia;linia;60;Od {prog:T.godzina}; połowa {prog:T.pol}; brak {nieznana}; {ceil(0,7·d)}.\n";

struct WK {
  std::string id, wariant, rodzaj, parametry, liczone = "wazne", okno, poziom = "problem", grupa = "R5b",
      slot = "rano", tryby = "ASV,ASVAuto", wymagaNormy = "nie", wRozruchu = "tak", wyklucza, wymaga, bramki, limit,
      teksty, stan = "aktywna", pilnosc;
};
static WK wk(const std::string& id, const std::string& rodzaj, const std::string& par) {
  WK w;
  w.id = id;
  w.rodzaj = rodzaj;
  w.parametry = par;
  return w;
}
static std::string linia(const WK& w) {
  return w.id + ";" + w.wariant + ";" + w.rodzaj + ";" + w.parametry + ";" + w.liczone + ";" + w.okno + ";" +
         w.poziom + ";" + w.grupa + ";" + w.slot + ";" + w.tryby + ";" + w.wymagaNormy + ";" + w.wRozruchu + ";" +
         w.wyklucza + ";" + w.wymaga + ";" + w.bramki + ";" + w.limit + ";" + w.teksty + ";" + w.stan + ";" +
         w.pilnosc + "\n";
}
static const char* POLA_K =
    "id_reguly;wariant;rodzaj;parametry;liczone_noce;okno;poziom;grupa;slot;tryby;wymaga_normy;w_rozruchu;wyklucza;"
    "wymaga;bramki;limit;teksty;stan;pilnosc\n";

static bool zestawZ(const std::vector<WK>& wiersze, Zestaw& z, std::string& powod) {
  std::string k = POLA_K;
  for (auto& w : wiersze) k += linia(w);
  return wczytajZestaw(plikZ("progi", PROGI_T), plikZ("katalog", k), plikZ("teksty", TEKSTY_T), z, powod);
}
static Zestaw zestaw(const std::vector<WK>& wiersze) {
  Zestaw z;
  std::string p;
  bool w = zestawZ(wiersze, z, p);
  if (!w) printf("zestaw testowy odrzucony: %s\n", p.c_str());
  return z;
}

// ---------- noce syntetyczne ----------
static Noc noc(const std::string& data, double ahi) {
  Noc n;
  n.data = data;
  n.wazna = true;
  n.m01 = 400;
  n.ahi = (float)ahi;
  n.wmed = 5;
  n.wp95 = 12;
  n.oa = 0.5f;
  n.ca = (float)(ahi / 2);
  n.h = 0.5f;
  n.ustawienia = "ASVAuto/6/10/3/15";
  return n;
}
// Noce od 2026-10-01 dzień po dniu z podanym AHI; ostatnia = dzis.
static std::vector<Noc> noce(const std::vector<double>& ahi, const std::string& od = "2026-10-01") {
  std::vector<Noc> h;
  for (size_t i = 0; i < ahi.size(); i++) h.push_back(noc(dodajDni(od, (int)i), ahi[i]));
  return h;
}
static Kontekst kontekst(const std::vector<Noc>& h, const Zestaw& z) {
  Kontekst k;
  k.historia = &h;
  k.dzis = h.empty() ? "2026-10-01" : h.back().data;
  k.trybBiezacy = Tryb::ASVAUTO;
  k.zestaw = &z;
  return k;
}
static const Wynik* znajdz(const std::vector<Wynik>& w, const std::string& id, const std::string& wariant = "") {
  for (auto& x : w)
    if (x.id == id && x.wariant == wariant) return &x;
  return nullptr;
}
static Wynik jeden(const WK& w, const Kontekst& k0, const Zestaw& z) {
  Kontekst k = k0;
  k.zestaw = &z;
  std::vector<Wynik> r = ocen(z, k);
  const Wynik* x = znajdz(r, w.id, w.wariant);
  if (!x) {
    Wynik b;
    b.powodNieoceny = "BRAK WYNIKU";
    return b;
  }
  return *x;
}
static Wynik ocenJeden(const WK& w, const std::vector<Noc>& h) {
  Zestaw z = zestaw({w});
  return jeden(w, kontekst(h, z), z);
}

// ============================================================================================================
// Fala 1 (minimum): parser, ocen(), tekst(), prog_nocy, k_z_n_nocy, statystyka_okna, zlozenie
// ============================================================================================================

static void testSha256Wektor() {
  ROWNE(sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  ROWNE(sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

static void testTrybZUstawien() {
  SPRAWDZ(trybZUstawien("ASVAuto/6/10/3/15") == Tryb::ASVAUTO);
  SPRAWDZ(trybZUstawien("ASV/8/3/15") == Tryb::ASV);
  SPRAWDZ(trybZUstawien("AutoSet/5/13/3") == Tryb::AUTOSET);
  SPRAWDZ(trybZUstawien("CPAP/9/3") == Tryb::CPAP);
  SPRAWDZ(trybZUstawien("VAuto/6/10") == Tryb::NIEZNANY);
  SPRAWDZ(trybZUstawien("") == Tryb::NIEZNANY);
  SPRAWDZ(trybZUstawien("ASVAuto") == Tryb::ASVAUTO);
}

static void testWczytajZestawPoprawny() {
  WK a = wk("E-30", "statystyka_okna", "metryka=M-02,stat=mediana,prog=E-30.ahi,por=ge,n=E-30.okno");
  a.okno = "E-30.okno";
  a.wyklucza = "E-31,E-33/b";
  a.teksty = "E-30.tytul,E-30.powod";
  a.pilnosc = "B";
  a.stan = "tylko_raport";
  a.wymagaNormy = "tak";
  a.wRozruchu = "nie";
  Zestaw z;
  std::string p;
  SPRAWDZ(zestawZ({a}, z, p));
  ROWNE(p, "");
  ROWNE(z.identyfikator, "t1");
  ROWNE_L(z.katalog.size(), 1);
  if (z.katalog.size() == 1) {
    const WierszKatalogu& w = z.katalog[0];
    ROWNE(w.id, "E-30");
    ROWNE(w.rodzaj, "statystyka_okna");
    ROWNE(w.parametry.at("metryka"), "M-02");
    ROWNE(w.parametry.at("n"), "E-30.okno");
    ROWNE_L(w.parametry.size(), 5);
    ROWNE(w.okno, "E-30.okno");
    ROWNE(w.liczoneNoce, "wazne");
    ROWNE(w.tryby, "ASV,ASVAuto");
    SPRAWDZ(w.wymagaNormy);
    SPRAWDZ(!w.wRozruchu);
    ROWNE_L(w.wyklucza.size(), 2);
    if (w.wyklucza.size() == 2) ROWNE(w.wyklucza[1], "E-33/b");
    ROWNE_L(w.teksty.size(), 2);
    SPRAWDZ(w.stan == StanReguly::TYLKO_RAPORT);
    SPRAWDZ(w.pilnosc == Pilnosc::B);
  }
  // progi: liczba, liczba z przecinkiem, tekst, tryb
  const Prog* pr = z.prog("E-30.ahi", Tryb::ASVAUTO);
  SPRAWDZ(pr && pr->wartosc == 5 && pr->jednostka == "/h");
  pr = z.prog("T.pol", Tryb::ASV);
  SPRAWDZ(pr && std::fabs(pr->wartosc - 1.5) < 1e-9);
  pr = z.prog("T.godzina", Tryb::ASV);
  SPRAWDZ(pr && pr->tekst == "18:00");
  pr = z.prog("T.kzn", Tryb::ASV);
  SPRAWDZ(pr && pr->tekst == "3/7");
  SPRAWDZ(z.prog("T.asv", Tryb::ASV) != nullptr);
  SPRAWDZ(z.prog("T.asv", Tryb::ASVAUTO) != nullptr);
  SPRAWDZ(z.prog("T.asv", Tryb::CPAP) == nullptr);
  SPRAWDZ(z.prog("brak.progu", Tryb::ASV) == nullptr);
  // teksty: tekst do końca wiersza (ze średnikami), limit
  ROWNE_L(z.teksty.size(), 3);
  if (z.teksty.count("T.linia")) {
    ROWNE(z.teksty.at("T.linia").tekst, "Od {prog:T.godzina}; połowa {prog:T.pol}; brak {nieznana}; {ceil(0,7·d)}.");
    ROWNE_L(z.teksty.at("T.linia").limit, 60);
    ROWNE(z.teksty.at("T.linia").rola, "linia");
  }
}

static void testWczytajZestawBledy() {
  WK a = wk("E-30", "statystyka_okna", "metryka=M-02,stat=mediana,prog=E-30.ahi,n=E-30.okno");
  std::string k = std::string(POLA_K) + linia(a);
  Zestaw z;
  std::string p;
  // poprawny
  SPRAWDZ(wczytajZestaw(plikZ("progi", PROGI_T), plikZ("katalog", k), plikZ("teksty", TEKSTY_T), z, p));
  // jedna cyfra zmieniona po zbudowaniu → skrót niezgodny
  std::string pr = plikZ("progi", PROGI_T);
  size_t i = pr.find("E-30.ahi;5");
  pr[i + 9] = '6';
  p.clear();
  SPRAWDZ(!wczytajZestaw(pr, plikZ("katalog", k), plikZ("teksty", TEKSTY_T), z, p));
  SPRAWDZ(p.find("skrot") != std::string::npos);
  // plik tekstów z innego zestawu
  p.clear();
  SPRAWDZ(!wczytajZestaw(plikZ("progi", PROGI_T), plikZ("katalog", k), plikZ("teksty", TEKSTY_T, "t2"), z, p));
  SPRAWDZ(p.find("zestaw") != std::string::npos);
  // inne pochodzenie w jednym pliku
  SPRAWDZ(!wczytajZestaw(plikZ("progi", PROGI_T), plikZ("katalog", k), plikZ("teksty", TEKSTY_T, "t1", "karta"), z,
                         p));
  // pliki zamienione miejscami (rodzaj w nagłówku)
  SPRAWDZ(!wczytajZestaw(plikZ("katalog", k), plikZ("progi", PROGI_T), plikZ("teksty", TEKSTY_T), z, p));
  // bez nagłówka, zła wersja formatu
  SPRAWDZ(!wczytajZestaw(PROGI_T, plikZ("katalog", k), plikZ("teksty", TEKSTY_T), z, p));
  std::string v2 = plikZ("progi", PROGI_T);
  v2.replace(v2.find(" 1 "), 3, " 2 ");
  SPRAWDZ(!wczytajZestaw(v2, plikZ("katalog", k), plikZ("teksty", TEKSTY_T), z, p));
  // próg, którego nie ma
  WK b = wk("E-30", "statystyka_okna", "metryka=M-02,prog=E-30.brak");
  p.clear();
  SPRAWDZ(!zestawZ({b}, z, p));
  SPRAWDZ(p.find("E-30.brak") != std::string::npos);
  WK b2 = wk("E-30", "statystyka_okna", "metryka=M-02,prog=E-30.ahi");
  b2.okno = "E-30.brak_okna";
  SPRAWDZ(!zestawZ({b2}, z, p));
  // tekst, którego nie ma
  WK c = wk("E-30", "prog_nocy", "metryka=M-02,prog=E-30.ahi");
  c.teksty = "E-30.tytul,E-30.nie_ma";
  p.clear();
  SPRAWDZ(!zestawZ({c}, z, p));
  SPRAWDZ(p.find("E-30.nie_ma") != std::string::npos);
  // nieznane liczone_noce, stan, pilnosc
  WK d = wk("E-30", "prog_nocy", "metryka=M-02,prog=E-30.ahi");
  d.liczone = "byle_jakie";
  SPRAWDZ(!zestawZ({d}, z, p));
  d.liczone = "wazne";
  d.stan = "wlaczona";
  SPRAWDZ(!zestawZ({d}, z, p));
  d.stan = "aktywna";
  d.pilnosc = "D";
  SPRAWDZ(!zestawZ({d}, z, p));
  // nieznany rodzaj: zestaw przyjęty, wiersz niepoliczalny (T-WYM-64, T-WYM-66; rodzaje rejestrują się później)
  WK e = wk("E-99", "nieznany", "");
  SPRAWDZ(zestawZ({e}, z, p));
  // nazwy liczone_noce z kontraktu
  for (const char* ln : {"kompletne", "z_rekordem", "z_terapia", "wazne", "wazne_bez_niepewnych", "wszystkie", ""}) {
    WK f = wk("E-30", "prog_nocy", "metryka=M-02,prog=E-30.ahi");
    f.liczone = ln;
    SPRAWDZ(zestawZ({f}, z, p));
  }
}

static void testMetrykaMedianaNoceOkna() {
  Noc n = noc("2026-10-01", 2.5);
  n.m01 = -1;
  n.cp95 = 11;
  n.ep95 = 8.5f;
  n.wmed = 6;
  n.wp95 = 20;
  n.csr = 12;
  SPRAWDZ(std::isnan(metryka(n, "M-01")));
  n.m01 = 300;
  SPRAWDZ(metryka(n, "M-01") == 300);
  SPRAWDZ(std::fabs(metryka(n, "M-02") - 2.5) < 1e-6);
  SPRAWDZ(std::fabs(metryka(n, "M-03ca") - 1.25) < 1e-6);
  SPRAWDZ(std::fabs(metryka(n, "M-03oa") - 0.5) < 1e-6);
  SPRAWDZ(std::fabs(metryka(n, "M-03h") - 0.5) < 1e-6);
  SPRAWDZ(std::fabs(metryka(n, "M-05p") - 11) < 1e-6);
  SPRAWDZ(std::fabs(metryka(n, "M-06p") - 8.5) < 1e-6);
  SPRAWDZ(std::fabs(metryka(n, "M-07") - 20) < 1e-6);  // M-07 w regułach = wyciek P95 (4.5)
  SPRAWDZ(std::fabs(metryka(n, "M-09") - 12) < 1e-6);
  n.epapStr = true;
  SPRAWDZ(std::isnan(metryka(n, "M-06p")));  // EPAP docelowe urządzenia: bez reguł (R-NOR-08)
  SPRAWDZ(std::isnan(metryka(n, "M-13")));   // pola puste
  SPRAWDZ(std::isnan(metryka(n, "M-17")));
  n.m13 = 16.5f;
  n.m17 = 2;
  n.m08 = 12;
  n.ua = 0.25f;
  n.m10 = 0.05f;
  n.m10p = 0.2f;
  n.mpa = 15;
  SPRAWDZ(std::fabs(metryka(n, "M-13") - 16.5) < 1e-6);
  SPRAWDZ(metryka(n, "M-17") == 2);
  SPRAWDZ(std::fabs(metryka(n, "M-08") - 12) < 1e-6);
  SPRAWDZ(std::fabs(metryka(n, "M-03ua") - 0.25) < 1e-6);
  SPRAWDZ(std::fabs(metryka(n, "M-03ob") - 1.25) < 1e-6);  // OA 0,5 + UA 0,25 + H 0,5
  SPRAWDZ(std::fabs(metryka(n, "M-10") - 0.05) < 1e-6);
  SPRAWDZ(std::fabs(metryka(n, "M-10p") - 0.2) < 1e-6);
  SPRAWDZ(metryka(n, "M-21mpa") == 15);
  SPRAWDZ(std::isnan(metryka(n, "M-21mpi")));
  SPRAWDZ(std::isnan(metryka(n, "M-99")));
  // mediana: ta sama metoda co norma (średnia dwóch środkowych)
  SPRAWDZ(std::fabs(reguly::mediana({4, 1, 3, 2}) - 2.5) < 1e-9);
  SPRAWDZ(std::fabs(reguly::mediana({3, 1, 2}) - 2) < 1e-9);
  SPRAWDZ(std::isnan(reguly::mediana({})));
  // noceOkna: rodzaj nocy, okno do dzis, ostatnie n
  std::vector<Noc> h = noce({1, 2, 3, 4, 5, 6, 7, 8});
  h[1].wazna = false;            // 10-02 nieważna
  h[2].wmed = 31;                // 10-03 niepewna (4.9)
  h[3].wazna = false;            // 10-04 krótka, z terapią
  h[3].m01 = 20;
  h[4].wazna = false;            // 10-05 bez terapii
  h[4].m01 = 5;
  h[5].czesciowa = true;         // 10-06 częściowa
  h[5].wazna = false;
  Zestaw z = zestaw({});
  Kontekst k = kontekst(h, z);
  k.dzis = "2026-10-07";         // noc 10-08 po dzis
  auto daty = [](const std::vector<const Noc*>& v) {
    std::string s;
    for (auto* x : v) s += x->data.substr(8) + " ";
    return s;
  };
  ROWNE(daty(noceOkna(k, "wazne", 0)), "01 03 07 ");
  ROWNE(daty(noceOkna(k, "", 0)), "01 03 07 ");
  ROWNE(daty(noceOkna(k, "wazne_bez_niepewnych", 0)), "01 07 ");
  ROWNE(daty(noceOkna(k, "wazne_bez_niepewnych", 1)), "07 ");
  ROWNE(daty(noceOkna(k, "z_terapia", 0)), "01 02 03 04 07 ");
  ROWNE(daty(noceOkna(k, "z_terapia_bez_niepewnych", 0)), "01 02 04 07 ");
  ROWNE(daty(noceOkna(k, "kompletne", 0)), "01 02 03 04 05 07 ");
  ROWNE(daty(noceOkna(k, "z_rekordem", 0)), "01 02 03 04 05 06 07 ");
  ROWNE(daty(noceOkna(k, "wszystkie", 2)), "06 07 ");
  ROWNE(daty(noceOkna(k, "wazne", 2)), "03 07 ");
  // noc w trybie nieobsługiwanym (R-DANE-29) wypada z okien; puste ustawienia = ostatnie znane
  std::vector<Noc> h2 = noce({1, 2, 3, 4});
  h2[1].ustawienia = "VAuto/5/10";
  h2[2].ustawienia = "";
  k = kontekst(h2, z);
  ROWNE(daty(noceOkna(k, "wazne", 0)), "01 04 ");
  SPRAWDZ(trybNocy(k, h2[2]) == Tryb::NIEZNANY);
  SPRAWDZ(trybNocy(k, h2[3]) == Tryb::ASVAUTO);
}

static void testProgNocy() {
  WK w = wk("E-23", "prog_nocy", "metryka=M-02,prog=E-30.ahi,por=ge");
  Wynik r = ocenJeden(w, noce({1, 1, 6}));
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["x"], "6,0");
  ROWNE(r.zmienne["data"], "2026-10-03");
  r = ocenJeden(w, noce({1, 1, 5}));  // dokładnie na progu, ge
  SPRAWDZ(r.oceniona && r.spelniona);
  r = ocenJeden(w, noce({1, 1, 4.9}));
  SPRAWDZ(r.oceniona && !r.spelniona);
  WK g = wk("E-23", "prog_nocy", "metryka=M-02,prog=E-30.ahi,por=gt");
  r = ocenJeden(g, noce({1, 1, 5}));
  SPRAWDZ(r.oceniona && !r.spelniona);
  WK l = wk("E-23", "prog_nocy", "metryka=M-02,prog=E-30.ahi,por=lt");
  r = ocenJeden(l, noce({1, 1, 4}));
  SPRAWDZ(r.oceniona && r.spelniona);
  WK le = wk("E-23", "prog_nocy", "metryka=M-02,prog=E-30.ahi,por=le");
  r = ocenJeden(le, noce({1, 1, 5}));
  SPRAWDZ(r.oceniona && r.spelniona);
  // brak nocy dziś → nie oceniam
  std::vector<Noc> h = noce({1, 1, 6});
  Zestaw z = zestaw({w});
  Kontekst k = kontekst(h, z);
  k.dzis = "2026-10-05";
  r = jeden(w, k, z);
  SPRAWDZ(!r.oceniona);
  SPRAWDZ(!r.powodNieoceny.empty());
  // brak metryki → nie oceniam
  h = noce({1, 1, 6});
  h[2].ahi = NAN;
  r = ocenJeden(w, h);
  SPRAWDZ(!r.oceniona);
  // noc dziś, ale nie tego rodzaju (nieważna przy liczone=wazne) → nie oceniam
  h = noce({1, 1, 6});
  h[2].wazna = false;
  r = ocenJeden(w, h);
  SPRAWDZ(!r.oceniona);
  // próg tylko dla ASV: w ASVAuto jest; brak parametru prog → nie oceniam (T-WYM-66)
  WK a = wk("E-23", "prog_nocy", "metryka=M-02,prog=T.asv");
  r = ocenJeden(a, noce({3}));
  SPRAWDZ(r.oceniona && r.spelniona);
  WK bez = wk("E-23", "prog_nocy", "metryka=M-02");
  r = ocenJeden(bez, noce({3}));
  SPRAWDZ(!r.oceniona);
  WK zlaM = wk("E-23", "prog_nocy", "metryka=M-99,prog=E-30.ahi");
  r = ocenJeden(zlaM, noce({3}));
  SPRAWDZ(!r.oceniona);
}

static void testKZNNocy() {
  WK w = wk("E-22", "k_z_n_nocy", "metryka=M-02,prog=E-30.ahi,kzn=T.kzn");
  Wynik r = ocenJeden(w, noce({1, 6, 1, 6, 1, 6, 1}));  // dokładnie 3 z 7
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["n"], "3");
  ROWNE(r.zmienne["z"], "7");
  r = ocenJeden(w, noce({6, 6, 1, 6, 1, 1, 1, 1}));  // 2 z ostatnich 7
  SPRAWDZ(r.oceniona && !r.spelniona);
  ROWNE(r.zmienne["n"], "2");
  r = ocenJeden(w, noce({6, 6, 6, 6, 6, 6}));  // 6 nocy: za mało
  SPRAWDZ(!r.oceniona);
  SPRAWDZ(r.powodNieoceny.find("6/7") != std::string::npos);
  // noc bez metryki wypada z okna (okno to noce z wartością)
  std::vector<Noc> h = noce({6, 6, 6, 1, 1, 1, 1, 1});
  h[7].ahi = NAN;
  r = ocenJeden(w, h);
  SPRAWDZ(r.oceniona && r.spelniona);
  // kzn nie w postaci k/n → nie oceniam
  WK zly = wk("E-22", "k_z_n_nocy", "metryka=M-02,prog=E-30.ahi,kzn=E-30.okno");
  r = ocenJeden(zly, noce({6, 6, 6, 6, 6, 6, 6}));
  SPRAWDZ(!r.oceniona);
}

static void testStatystykaOkna() {
  WK w = wk("E-30", "statystyka_okna", "metryka=M-02,stat=mediana,prog=E-30.ahi,por=ge,n=E-30.okno");
  w.liczone = "wazne_bez_niepewnych";
  Wynik r = ocenJeden(w, noce({9, 1, 9, 5, 2, 7, 6}));  // mediana 6
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["n"], "5");  // nocy z AHI ≥ 5
  ROWNE(r.zmienne["x"], "6,0");
  ROWNE(r.zmienne["wartosc"], "6,0");
  r = ocenJeden(w, noce({9, 1, 9, 5, 2, 1, 5}));  // mediana dokładnie 5
  SPRAWDZ(r.oceniona && r.spelniona);
  r = ocenJeden(w, noce({9, 1, 9, 4.9, 2, 1, 5}));  // mediana 4,9
  SPRAWDZ(r.oceniona && !r.spelniona);
  r = ocenJeden(w, noce({9, 9, 9, 9, 9, 9}));  // 6 z 7
  SPRAWDZ(!r.oceniona);
  SPRAWDZ(r.powodNieoceny.find("6/7") != std::string::npos);
  // noc niepewna wypada z okna (R-NOR-05): okno sięga nocy wcześniejszej
  std::vector<Noc> h = noce({1, 1, 1, 9, 9, 9, 9, 1});
  h[7].wmed = 40;
  r = ocenJeden(w, h);  // okno: 1 1 1 9 9 9 9 → mediana 9
  SPRAWDZ(r.oceniona && r.spelniona);
  // okno z pola `okno`, gdy brak parametru n
  WK o = wk("E-30", "statystyka_okna", "metryka=M-02,stat=mediana,prog=E-30.ahi");
  o.okno = "E-30.okno";
  r = ocenJeden(o, noce({9, 9, 9, 9, 1, 1, 1}));
  SPRAWDZ(r.oceniona && r.spelniona);
  // bez n i bez okna → nie oceniam
  WK bez = wk("E-30", "statystyka_okna", "metryka=M-02,stat=mediana,prog=E-30.ahi");
  r = ocenJeden(bez, noce({9, 9, 9, 9, 1, 1, 1}));
  SPRAWDZ(!r.oceniona);
  // średnia, suma, odchylenie std, nieznana statystyka
  WK sr = wk("L-01", "statystyka_okna", "metryka=M-02,stat=srednia,prog=E-30.ahi,n=E-30.okno");
  r = ocenJeden(sr, noce({5, 5, 5, 5, 5, 5, 5}));
  SPRAWDZ(r.oceniona && r.spelniona);
  r = ocenJeden(sr, noce({5, 5, 5, 5, 5, 5, 4.9}));
  SPRAWDZ(r.oceniona && !r.spelniona);
  WK su = wk("L-01", "statystyka_okna", "metryka=M-02,stat=suma,prog=E-30.ahi,n=T.n3");
  r = ocenJeden(su, noce({2, 2, 1}));
  SPRAWDZ(r.oceniona && r.spelniona);
  WK sd = wk("S-K-07", "statystyka_okna", "metryka=M-02,stat=odch_std,prog=T.razy,n=T.n3");
  r = ocenJeden(sd, noce({1, 3, 5}));  // odchylenie std próby = 2
  SPRAWDZ(r.oceniona && r.spelniona);
  WK nz = wk("L-01", "statystyka_okna", "metryka=M-02,stat=moda,prog=E-30.ahi,n=T.n3");
  r = ocenJeden(nz, noce({2, 2, 1}));
  SPRAWDZ(!r.oceniona);
  // udział nocy z M-01 ≥ 240 min wobec progu 70% (S-K-27)
  WK ud = wk("S-K-27", "statystyka_okna", "metryka=M-01,stat=udzial,prog_nocy=T.m01_4h,prog=T.udzial,n=E-30.okno");
  ud.liczone = "z_terapia";
  std::vector<Noc> u = noce({1, 1, 1, 1, 1, 1, 1});
  u[0].m01 = 100;
  u[1].m01 = 100;
  r = ocenJeden(ud, u);  // 5/7 = 71%
  SPRAWDZ(r.oceniona && r.spelniona);
  u[2].m01 = 100;
  r = ocenJeden(ud, u);  // 4/7 = 57%
  SPRAWDZ(r.oceniona && !r.spelniona);
}

static void testZlozenie() {
  WK a = wk("A", "prog_nocy", "metryka=M-02,prog=E-30.ahi");
  WK b = wk("B", "prog_nocy", "metryka=M-02,prog=T.asv");
  WK n = wk("N", "prog_nocy", "metryka=M-13,prog=E-30.ahi");  // M-13 bez wartości → nie oceniam
  WK i = wk("I", "zlozenie", "op=i,skladniki=A+B");
  WK l = wk("L", "zlozenie", "op=lub,skladniki=A+B");
  WK nie = wk("NIE", "zlozenie", "op=nie,skladniki=A");
  WK in = wk("IN", "zlozenie", "op=i,skladniki=A+N");
  WK ln = wk("LN", "zlozenie", "op=lub,skladniki=B+N");
  WK brak = wk("BR", "zlozenie", "op=i,skladniki=A+Q");
  WK zlyOp = wk("ZO", "zlozenie", "op=xor,skladniki=A+B");
  Zestaw z = zestaw({a, b, n, i, l, nie, in, ln, brak, zlyOp});
  std::vector<Noc> h = noce({3});  // A: 3 ≥ 5 nie; B: 3 ≥ 2 tak
  Kontekst k = kontekst(h, z);
  std::vector<Wynik> r = ocen(z, k);
  auto w = [&](const char* id) { return znajdz(r, id) ? *znajdz(r, id) : Wynik(); };
  SPRAWDZ(w("I").oceniona && !w("I").spelniona);
  SPRAWDZ(w("L").oceniona && w("L").spelniona);
  SPRAWDZ(w("NIE").oceniona && w("NIE").spelniona);
  SPRAWDZ(w("IN").oceniona && !w("IN").spelniona);  // fałsz I nieznane = fałsz
  SPRAWDZ(w("LN").oceniona && w("LN").spelniona);   // prawda LUB nieznane = prawda
  SPRAWDZ(!w("BR").oceniona);  // składnik, którego nie ma w katalogu: błąd zestawu, nie „fałsz”
  SPRAWDZ(!w("ZO").oceniona);
  h = noce({6});  // A tak, B tak
  k = kontekst(h, z);
  r = ocen(z, k);
  SPRAWDZ(w("I").oceniona && w("I").spelniona);
  SPRAWDZ(!w("IN").oceniona);  // prawda I nieznane = nie oceniam
  SPRAWDZ(w("NIE").oceniona && !w("NIE").spelniona);
  // składnik po wariancie i składnik bez wariantu przy wierszach z wariantami
  WK va = wk("V", "prog_nocy", "metryka=M-02,prog=E-30.ahi");
  va.wariant = "a";
  WK vb = wk("V", "prog_nocy", "metryka=M-02,prog=T.asv");
  vb.wariant = "b";
  WK sa = wk("SA", "zlozenie", "op=i,skladniki=V/a");
  WK sv = wk("SV", "zlozenie", "op=i,skladniki=V");  // całe id: spełnione, gdy którykolwiek wariant spełniony
  z = zestaw({va, vb, sa, sv});
  h = noce({3});
  k = kontekst(h, z);
  r = ocen(z, k);
  SPRAWDZ(w("SA").oceniona && !w("SA").spelniona);
  SPRAWDZ(w("SV").oceniona && w("SV").spelniona);
}

static void testOcenMechanizmy() {
  WK e30 = wk("E-30", "prog_nocy", "metryka=M-02,prog=E-30.ahi");
  e30.wyklucza = "E-31";
  e30.pilnosc = "B";
  WK e31 = wk("E-31", "prog_nocy", "metryka=M-02,prog=T.asv");
  WK e33 = wk("E-33", "prog_nocy", "metryka=M-02,prog=T.asv");
  e33.wariant = "x";
  WK wyk = wk("E-40", "prog_nocy", "metryka=M-02,prog=T.asv");
  wyk.wyklucza = "E-33/x";
  WK raport = wk("L-04", "prog_nocy", "metryka=M-02,prog=T.asv");
  raport.stan = "tylko_raport";
  raport.pilnosc = "B";
  raport.wyklucza = "E-36";  // tylko raport nie wyklucza (R-SKI-44)
  WK e36 = wk("E-36", "prog_nocy", "metryka=M-02,prog=T.asv");
  WK blok = wk("L-06", "prog_nocy", "metryka=M-02,prog=T.asv");
  blok.stan = "zablokowana";
  WK wym = wk("E-50", "prog_nocy", "metryka=M-02,prog=T.asv");
  wym.wymaga = "E-30";
  WK wymNie = wk("E-51", "prog_nocy", "metryka=M-02,prog=T.asv");
  wymNie.wymaga = "E-52";
  WK e52 = wk("E-52", "prog_nocy", "metryka=M-02,prog=T.max");  // 6 ≥ 10 nie
  WK wymBlok = wk("E-53", "prog_nocy", "metryka=M-02,prog=T.asv");
  wymBlok.wymaga = "L-06";
  WK tylkoCpap = wk("L-01", "prog_nocy", "metryka=M-02,prog=E-30.ahi");
  tylkoCpap.tryby = "CPAP,AutoSet";
  WK wszystkie = wk("E-10", "prog_nocy", "metryka=M-02,prog=E-30.ahi");
  wszystkie.tryby = "wszystkie";
  WK nieznany = wk("E-99", "nieznany_rodzaj", "");
  WK norma = wk("E-21", "prog_nocy", "metryka=M-02,prog=T.asv");
  norma.wymagaNormy = "tak";
  WK rozruch = wk("L-07", "prog_nocy", "metryka=M-02,prog=T.asv");
  rozruch.wRozruchu = "nie";
  Zestaw z = zestaw({e30, e31, e33, wyk, raport, e36, blok, wym, e52, wymNie, wymBlok, tylkoCpap, wszystkie,
                     nieznany, norma, rozruch});
  std::vector<Noc> h = noce({6});
  Kontekst k = kontekst(h, z);
  std::vector<std::string> log;
  OpcjeOceny o;
  o.log = [&](const std::string& s) { log.push_back(s); };
  std::vector<Wynik> r = ocen(z, k, o);
  // kolejność katalogu, bez reguł zablokowanych i spoza trybu
  std::string ids;
  for (auto& x : r) ids += kluczWyniku(x.id, x.wariant) + " ";
  ROWNE(ids, "E-30 E-31 E-33/x E-40 L-04 E-36 E-50 E-52 E-51 E-53 E-10 E-99 E-21 L-07 ");
  auto w = [&](const char* id, const char* war = "") { return znajdz(r, id, war) ? *znajdz(r, id, war) : Wynik(); };
  SPRAWDZ(w("E-30").oceniona && w("E-30").spelniona);
  SPRAWDZ(w("E-30").stan == StanReguly::AKTYWNA && w("E-30").pilnosc == Pilnosc::B);
  ROWNE(w("E-30").poziom, "problem");
  ROWNE(w("E-30").grupa, "R5b");
  // wykluczenie na całym id i na id/wariant
  SPRAWDZ(w("E-31").oceniona && !w("E-31").spelniona);
  ROWNE(w("E-31").zmienne["wykluczona_przez"], "E-30");
  SPRAWDZ(w("E-33", "x").oceniona && !w("E-33", "x").spelniona);
  // tylko raport: liczy się, bez pilności, nie wyklucza
  SPRAWDZ(w("L-04").oceniona && w("L-04").spelniona);
  SPRAWDZ(w("L-04").stan == StanReguly::TYLKO_RAPORT && w("L-04").pilnosc == Pilnosc::BRAK);
  SPRAWDZ(w("E-36").oceniona && w("E-36").spelniona);
  // wymaga: spełniona / niespełniona / zablokowana (= nieaktywna)
  SPRAWDZ(w("E-50").oceniona && w("E-50").spelniona);
  SPRAWDZ(w("E-51").oceniona && !w("E-51").spelniona);
  SPRAWDZ(!w("E-53").oceniona);  // wymaga zablokowanej: nie oceniam
  // „wszystkie” = cztery tryby obsługiwane
  SPRAWDZ(w("E-10").oceniona && w("E-10").spelniona);
  // nieznany rodzaj: nie oceniam z powodem i wpis w logu
  SPRAWDZ(!w("E-99").oceniona);
  SPRAWDZ(w("E-99").powodNieoceny.find("nieznany_rodzaj") != std::string::npos);
  bool jest = false;
  for (auto& s : log) jest = jest || (s.find("E-99") != std::string::npos && s.find("nie oceniam") != std::string::npos);
  SPRAWDZ(jest);
  // wymaga normy bez normy; rozruch (< 14 nocy ważnych) przy w_rozruchu = nie
  SPRAWDZ(!w("E-21").oceniona);
  SPRAWDZ(!w("L-07").oceniona);
  // z normą i po rozruchu
  std::vector<Noc> h20 = noce(std::vector<double>(20, 6));
  Norma nm;
  nm.n = 20;
  nm.ahi.n = 20;
  nm.ahi.med = 1;
  k = kontekst(h20, z);
  k.norma = &nm;
  r = ocen(z, k);
  SPRAWDZ(w("E-21").oceniona && w("E-21").spelniona);
  SPRAWDZ(w("L-07").oceniona && w("L-07").spelniona);
  // tryb bieżący CPAP (4.11, ADR-0030: silnik obsługuje cztery tryby): reguła z trybami CPAP,AutoSet i „wszystkie”
  // się liczą, reguła z trybami ASV,ASVAuto w CPAP nie działa (bez wiersza, 4.11 „nie działa w trybie”)
  for (auto& n : h20) n.ustawienia = "CPAP/9/3";
  k.trybBiezacy = Tryb::CPAP;
  r = ocen(z, k);
  SPRAWDZ(w("L-01").oceniona && w("L-01").spelniona);
  SPRAWDZ(znajdz(r, "E-30") == nullptr);
  SPRAWDZ(w("E-10").oceniona && w("E-10").spelniona);
  k.trybBiezacy = Tryb::AUTOSET;
  for (auto& n : h20) n.ustawienia = "AutoSet/5/13/3";
  r = ocen(z, k);
  SPRAWDZ(w("L-01").oceniona && w("L-01").spelniona);
  // tryb nieznany: wszystko „nie oceniam”, poza wyjątkami R-DANE-29 (lista w 4.11)
  k.trybBiezacy = Tryb::NIEZNANY;
  r = ocen(z, k);
  for (auto& x : r) {
    bool wyj = false;
    for (const char* id : {"E-01", "E-03", "E-04", "E-05", "E-07", "E-10", "E-11", "E-12", "E-20", "E-22"}) wyj = wyj || x.id == id;
    if (!wyj) SPRAWDZ(!x.oceniona);
  }
  SPRAWDZ(!r.empty());
}

static void testTekst() {
  WK w = wk("E-30", "statystyka_okna", "metryka=M-02,stat=mediana,prog=E-30.ahi,n=E-30.okno");
  w.teksty = "E-30.tytul,E-30.powod";
  Zestaw z = zestaw({w});
  Wynik r;
  r.id = "E-30";
  r.zmienne["n"] = "4";
  r.zmienne["x"] = "6,2";
  r.zmienne["ceil(0,7·d)"] = "21";
  ROWNE(tekst(z, "E-30.tytul", r, Tryb::ASVAUTO), "Noce z AHI powyżej 5");
  ROWNE(tekst(z, "E-30.powod", r, Tryb::ASVAUTO), "AHI powyżej 5 w 4 z 7 nocy, ostatnio 6,2.");
  ROWNE(tekst(z, "T.linia", r, Tryb::ASV), "Od 18:00; połowa 1,5; brak –; 21.");
  ROWNE(tekst(z, "nie.ma", r, Tryb::ASV), "");
}

static void testZestawWbudowanyE30() {
  const Zestaw& z = zestawWbudowany();
  SPRAWDZ(!z.identyfikator.empty());
  bool jest = false;
  for (auto& w : z.katalog) jest = jest || w.id == "E-30";
  SPRAWDZ(jest);
  SPRAWDZ(z.prog("E-30.ahi", Tryb::ASVAUTO) != nullptr);
  // łańcuch skrypt → nagłówek → zestawWbudowany() → ocen() → tekst()
  std::vector<Noc> h = noce({1, 1, 1, 5, 6, 7, 8});  // mediana 5
  Kontekst k = kontekst(h, z);
  std::vector<Wynik> r = ocen(z, k);
  const Wynik* e = znajdz(r, "E-30");
  SPRAWDZ(e && e->oceniona && e->spelniona);
  if (e) {
    ROWNE(tekst(z, "E-30.tytul", *e, Tryb::ASVAUTO), "Noce z AHI powyżej 5");
    ROWNE(tekst(z, "E-30.powod", *e, Tryb::ASVAUTO), "AHI powyżej 5 w 4 z 7 nocy, ostatnio 8,0.");
    ROWNE(tekst(z, "E-30.krok", *e, Tryb::ASVAUTO), "→ Sprawdź wyciek i obserwuj kolejne noce.");
  }
  h = noce({1, 1, 1, 4.9, 6, 7, 8});
  k = kontekst(h, z);
  r = ocen(z, k);
  e = znajdz(r, "E-30");
  SPRAWDZ(e && e->oceniona && !e->spelniona);
  SPRAWDZ(&zestawWbudowany() == &z);  // jedno parsowanie
}

// ============================================================================================================
// Fala 2: pozostałe rodzaje rdzenia, pamięć reguł
// ============================================================================================================

static Norma normaAhi(double med, double p10, double p90) {
  Norma nm;
  nm.n = 30;
  nm.ahi.n = 30;
  nm.ahi.med = med;
  nm.ahi.p10 = p10;
  nm.ahi.p90 = p90;
  return nm;
}
static Wynik ocenZ(const WK& w, const Kontekst& k0) {
  Zestaw z = zestaw({w});
  return jeden(w, k0, z);
}

static void testKolejneNoce() {
  WK w = wk("E-10", "kolejne_noce", "metryka=M-02,prog=E-30.ahi,por=ge,n=T.n3");
  Wynik r = ocenJeden(w, noce({1, 5, 6, 7}));
  SPRAWDZ(r.oceniona && r.spelniona);  // ostatnie 3: 5 6 7, dokładnie na progu
  r = ocenJeden(w, noce({9, 4.9, 6, 7}));
  SPRAWDZ(r.oceniona && !r.spelniona);
  r = ocenJeden(w, noce({6, 7}));
  SPRAWDZ(!r.oceniona);
  SPRAWDZ(r.powodNieoceny.find("2/3") != std::string::npos);
  // wobec normy: M-02 ≥ mediana normy + delta (3) w 3 kolejnych nocach
  WK n = wk("L-05", "kolejne_noce", "metryka=M-02,wzgledem=mediana,delta=T.delta,por=ge,n=T.n3");
  std::vector<Noc> h = noce({1, 4, 4, 4});
  Norma nm = normaAhi(1, 0.5, 2);
  Zestaw z = zestaw({n});
  Kontekst k = kontekst(h, z);
  k.norma = &nm;
  r = jeden(n, k, z);
  SPRAWDZ(r.oceniona && r.spelniona);  // 4 ≥ 1 + 3
  h = noce({1, 4, 3.9, 4});
  k = kontekst(h, z);
  k.norma = &nm;
  r = jeden(n, k, z);
  SPRAWDZ(r.oceniona && !r.spelniona);
  k.norma = nullptr;  // brak normy → nie oceniam
  r = jeden(n, k, z);
  SPRAWDZ(!r.oceniona);
  Norma bez = nm;
  bez.ahi.n = 5;  // norma tej metryki w rozruchu
  k.norma = &bez;
  r = jeden(n, k, z);
  SPRAWDZ(!r.oceniona);
}

static void testOknoWobecNormy() {
  WK w = wk("L-07", "okno_wobec_normy", "metryka=M-02,stat=mediana,n=E-30.okno,wzgledem=mediana,delta=T.delta,por=ge");
  Norma nm = normaAhi(2, 1, 3);
  std::vector<Noc> h = noce({5, 5, 5, 5, 1, 1, 1});  // mediana 5 = 2 + 3
  Zestaw z = zestaw({w});
  Kontekst k = kontekst(h, z);
  k.norma = &nm;
  Wynik r = jeden(w, k, z);
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["norma"], "2,0");
  ROWNE(r.zmienne["wartosc"], "5,0");
  h = noce({4.9, 5, 5, 4.9, 1, 1, 4.9});
  k = kontekst(h, z);
  k.norma = &nm;
  r = jeden(w, k, z);
  SPRAWDZ(r.oceniona && !r.spelniona);
  // razy i delta razem (L-02 b: ≥ 2 × mediana i ≥ +3): 4 ≥ 2×2 tak, 4 ≥ 2+3 nie
  WK rd = wk("L-02", "okno_wobec_normy", "metryka=M-02,n=T.n3,wzgledem=mediana,razy=T.razy,delta=T.delta");
  h = noce({4, 4, 4});
  k = kontekst(h, z);
  k.norma = &nm;
  r = ocenZ(rd, k);
  SPRAWDZ(r.oceniona && !r.spelniona);
  WK tylkoRazy = wk("L-02", "okno_wobec_normy", "metryka=M-02,n=T.n3,wzgledem=mediana,razy=T.razy");
  r = ocenZ(tylkoRazy, k);
  SPRAWDZ(r.oceniona && r.spelniona);
  WK p90 = wk("L-02", "okno_wobec_normy", "metryka=M-02,n=T.n3,wzgledem=p90,por=gt");
  r = ocenZ(p90, k);
  SPRAWDZ(r.oceniona && r.spelniona);  // 4 > P90 3
  WK zly = wk("L-02", "okno_wobec_normy", "metryka=M-02,n=T.n3,wzgledem=p50");
  r = ocenZ(zly, k);
  SPRAWDZ(!r.oceniona);
  WK bezNormy = wk("L-02", "okno_wobec_normy", "metryka=M-09,n=T.n3");  // M-09 nie ma normy
  r = ocenZ(bezNormy, k);
  SPRAWDZ(!r.oceniona);
  h = noce({4, 4});  // za mało nocy
  k = kontekst(h, z);
  k.norma = &nm;
  r = ocenZ(tylkoRazy, k);
  SPRAWDZ(!r.oceniona);
}

static void testStanOdchylenia() {
  std::vector<Noc> h = noce({3, 3, 4});
  Stany s;
  s.ahi.aktywny = true;
  s.ahi.od = "2026-10-01";
  s.ahi.nocy = 21;
  s.norma = normaAhi(1.2, 0.5, 2.5);
  WK w = wk("E-31", "stan_odchylenia", "metryka=M-02");
  Zestaw z = zestaw({w});
  Kontekst k = kontekst(h, z);
  k.stany = &s;
  Wynik r = jeden(w, k, z);
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["nocy"], "21");
  ROWNE(r.zmienne["n"], "21");
  ROWNE(r.zmienne["od"], "2026-10-01");
  ROWNE(r.zmienne["norma"], "1,2");
  ROWNE(r.zmienne["x"], "3,3");  // średnia z 5 ostatnich nocy ważnych (3, 3, 4), 6.4
  // min nocy stanu (R-NOR-23): 21 ≥ 21 tak, 20 nie
  WK m = wk("E-31", "stan_odchylenia", "metryka=M-02,min=T.b");  // 14
  r = ocenZ(m, k);
  SPRAWDZ(r.oceniona && r.spelniona);
  s.ahi.nocy = 13;
  r = ocenZ(m, k);
  SPRAWDZ(r.oceniona && !r.spelniona);
  s.ahi.nocy = 14;
  r = ocenZ(m, k);
  SPRAWDZ(r.oceniona && r.spelniona);
  s.ahi.aktywny = false;
  r = jeden(w, k, z);
  SPRAWDZ(r.oceniona && !r.spelniona);
  // metryka bez stanu w panelu (M-03ca, M-17) i brak Stany → nie oceniam
  WK ca = wk("E-33", "stan_odchylenia", "metryka=M-03ca");
  r = ocenZ(ca, k);
  SPRAWDZ(!r.oceniona);
  k.stany = nullptr;
  r = jeden(w, k, z);
  SPRAWDZ(!r.oceniona);
  // pozostałe metryki ze stanem
  s.m01.aktywny = true;
  s.wp95.aktywny = true;
  s.ep95.aktywny = true;
  for (NormaMetryki* nm : {&s.norma.m01, &s.norma.wp95, &s.norma.ep95}) {
    nm->n = 30;
    nm->med = 1;
  }
  k.stany = &s;
  for (const char* mm : {"M-01", "M-07", "M-06p"}) {
    WK x = wk("E-13", "stan_odchylenia", std::string("metryka=") + mm);
    r = ocenZ(x, k);
    SPRAWDZ(r.oceniona && r.spelniona);
  }
}

// Historia: 30 nocy z AHI 1; 1,5; 2 na zmianę (norma: mediana 1,5, P90 2), potem `ost`.
static std::vector<Noc> zNorma(const std::vector<double>& ost) {
  std::vector<double> v;
  for (int i = 0; i < 30; i++) v.push_back(1 + 0.5 * (i % 3));
  v.insert(v.end(), ost.begin(), ost.end());
  return noce(v, "2026-09-01");
}

static void testOdchylenieNocy() {
  // E-36: odchylenie M-02 (norma noc po nocy, Δ 3) w ≥ 3 z 7 nocach
  WK w = wk("E-36", "odchylenie_nocy", "metryka=M-02,kzn=T.kzn,delta=T.delta");
  Wynik r = ocenJeden(w, zNorma({5, 5, 4.5, 1, 1, 1, 1}));  // 4,5 − 1,5 = 3: dokładnie Δ
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["n"], "3");
  r = ocenJeden(w, zNorma({5, 5, 4.4, 1, 1, 1, 1}));
  SPRAWDZ(r.oceniona && !r.spelniona);
  r = ocenJeden(w, noce({4, 4, 4, 1, 1, 1, 1}));  // bez normy w żadnej nocy → nie oceniam
  SPRAWDZ(!r.oceniona);
  r = ocenJeden(w, noce({4, 4, 4, 1, 1, 1}));  // 6 z 7
  SPRAWDZ(!r.oceniona);
  // jedna noc (bez kzn): ostatnia noc dziś; w dół przy Δ ujemnym (M-01 −1,5 h)
  WK j = wk("S-K-30", "odchylenie_nocy", "metryka=M-02,delta=T.delta");
  SPRAWDZ(ocenJeden(j, zNorma({5})).spelniona);
  SPRAWDZ(ocenJeden(j, zNorma({5})).oceniona);
  WK dol = wk("E-13", "odchylenie_nocy", "metryka=M-01,delta=T.hm1");
  std::vector<Noc> h = zNorma({1});
  for (size_t i = 0; i + 1 < h.size(); i++) h[i].m01 = 400 + (int)(i % 3) * 10;
  h.back().m01 = 410 - 61;  // 349 < P10 400 i mediana 410 − 349 ≥ 60
  r = ocenJeden(dol, h);
  SPRAWDZ(r.oceniona && r.spelniona);
  h.back().m01 = 410 - 59;
  r = ocenJeden(dol, h);
  SPRAWDZ(r.oceniona && !r.spelniona);
  // m z n sygnałów (S-K-17): 2 z puli; M-13 bez normy wypada z puli
  WK s = wk("S-K-17", "odchylenie_nocy", "metryki=M-02+M-07+M-13,kzn=T.kzn24,delta_z=R-NOR-12.delta");
  s.liczone = "wazne";
  h = zNorma({5});
  for (size_t i = 0; i < h.size(); i++) h[i].wp95 = 12.0f + (float)(i % 3);
  h.back().wp95 = 30;  // 30 > P90 14 i 30 − 13 ≥ 10
  Zestaw z = zestaw({s});
  std::string pz = std::string(PROGI_T);
  (void)pz;
  // progi Δ z wiersza R-NOR-12 w zestawie testowym
  Zestaw z2;
  std::string powod;
  std::string progi = std::string(PROGI_T) + "R-NOR-12.delta.M-02;3;/h;wszystkie\nR-NOR-12.delta.M-07;10;L/min;wszystkie\n"
                      "R-NOR-12.delta.M-13;2;/min;wszystkie\n";
  std::string kat = std::string(POLA_K) + linia(s);
  SPRAWDZ(wczytajZestaw(plikZ("progi", progi), plikZ("katalog", kat), plikZ("teksty", TEKSTY_T), z2, powod));
  Kontekst k = kontekst(h, z2);
  r = jeden(s, k, z2);
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["n"], "2");
  ROWNE(r.zmienne["z"], "2");  // M-13 bez wartości: pula 2
  h.back().wp95 = 14;
  k = kontekst(h, z2);
  r = jeden(s, k, z2);
  SPRAWDZ(r.oceniona && !r.spelniona);
  h.back().ahi = NAN;  // pula 1 < k 2
  k = kontekst(h, z2);
  SPRAWDZ(!jeden(s, k, z2).oceniona);
}

static void testTrendRekordKamien() {
  WK t = wk("S-K-31", "trend", "metryka=M-02,n=T.n3,prog=T.pol,por=ge");  // nachylenie ≥ 1,5 / noc
  SPRAWDZ(ocenJeden(t, noce({1, 2.5, 4})).spelniona);
  SPRAWDZ(ocenJeden(t, noce({1, 2.5, 4})).oceniona);
  Wynik r = ocenJeden(t, noce({1, 2.4, 3.9}));
  SPRAWDZ(r.oceniona && !r.spelniona);
  SPRAWDZ(!ocenJeden(t, noce({1, 2})).oceniona);
  // rekord: mediana 3 ostatnich nocy najniższa w historii
  WK rk = wk("O-03", "rekord_historii", "metryka=M-02,stat=srednia,n=T.n3,min=T.min");
  r = ocenJeden(rk, noce({3, 3, 3, 2, 2, 1, 1, 1}));
  SPRAWDZ(r.oceniona && r.spelniona);
  r = ocenJeden(rk, noce({1, 1, 1, 3, 3, 1, 1, 1}));  // równa pierwszemu oknu: nie rekord
  SPRAWDZ(r.oceniona && !r.spelniona);
  SPRAWDZ(!ocenJeden(rk, noce({3, 2, 1, 1})).oceniona);  // historia krótsza niż min 5
  // kamień: 7. noc tego rodzaju; rocznica pierwszej nocy
  WK km = wk("S-K-28", "kamien", "prog=T.kamienie,rocznica=tak");
  std::string progi = std::string(PROGI_T) + "T.kamienie;7,30,100;noce;wszystkie\n";
  Zestaw z;
  std::string powod;
  SPRAWDZ(wczytajZestaw(plikZ("progi", progi), plikZ("katalog", std::string(POLA_K) + linia(km)),
                        plikZ("teksty", TEKSTY_T), z, powod));
  std::vector<Noc> h = noce({1, 1, 1, 1, 1, 1, 1});
  Kontekst k = kontekst(h, z);
  r = jeden(km, k, z);
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["n"], "7");
  h.pop_back();
  k = kontekst(h, z);
  r = jeden(km, k, z);
  SPRAWDZ(r.oceniona && !r.spelniona);
  std::vector<Noc> rok = {noc("2025-10-06", 1), noc("2026-10-05", 1), noc("2026-10-06", 1)};
  k = kontekst(rok, z);
  r = jeden(km, k, z);
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["n"], "1");
}

static void testCzasPozaProgiemGrupy() {
  // E-20: duży wyciek ≥ 30 min albo ≥ 10% czasu terapii
  WK w = wk("E-20", "czas_poza_progiem", "metryka=M-08,prog=T.min,prog_udzial=T.proc10");  // 5 min albo 10%
  std::vector<Noc> h = noce({1});
  h[0].m08 = 5;
  Wynik r = ocenJeden(w, h);
  SPRAWDZ(r.oceniona && r.spelniona);
  h[0].m08 = 4;
  h[0].m01 = 40;  // 10% z 40 min
  r = ocenJeden(w, h);
  SPRAWDZ(r.oceniona && r.spelniona);
  h[0].m01 = 41;
  r = ocenJeden(w, h);
  SPRAWDZ(r.oceniona && !r.spelniona);
  h[0].m08 = NAN;
  SPRAWDZ(!ocenJeden(w, h).oceniona);
  WK seria = wk("S-K-01", "czas_poza_progiem", "metryka=M-02,prog=T.min");
  SPRAWDZ(!ocenJeden(seria, noce({1})).oceniona);
  // porównanie grup: weekend wyżej o ≥ 3 / h, test permutacyjny z Holmem
  WK g = wk("O-12", "porownanie_grup", "metryka=M-02,cecha=weekend,n=T.b,min=T.n3,delta=T.delta,por=ge");
  std::vector<Noc> hg;
  for (int i = 0; i < 28; i++) {
    std::string d = dodajDni("2026-10-01", i);
    int dt = dzienTygodnia(d);
    hg.push_back(noc(d, (dt == 0 || dt == 6) ? 6 + 0.1 * i : 1 + 0.05 * i));
  }
  Zestaw z = zestaw({g});
  Kontekst k = kontekst(hg, z);
  r = jeden(g, k, z);
  SPRAWDZ(r.oceniona && r.spelniona);
  SPRAWDZ(std::atof(r.zmienne["p"].c_str()) < 0.05);
  Wynik r2 = jeden(g, k, z);
  ROWNE(r2.zmienne["p"], r.zmienne["p"]);  // powtarzalny (stałe ziarno)
  for (auto& n : hg) n.ahi = 2 + (float)(nrDnia(n.data) % 5) * 0.3f;  // bez różnicy grup
  r = jeden(g, k, z);
  SPRAWDZ(r.oceniona && !r.spelniona);
  WK zla = wk("O-12", "porownanie_grup", "metryka=M-02,cecha=czynnik,n=T.b,min=T.n3,delta=T.delta");
  SPRAWDZ(!ocenZ(zla, k).oceniona);
  // percentyl sezonu: odczyt w górnych 10% (P90) wartości sezonu
  WK ps = wk("S-K-06", "percentyl_sezonu", "metryka=pylek,prog=T.proc10");
  Zestaw zp = zestaw({ps});
  std::vector<Noc> h1 = noce({1});
  Kontekst kp = kontekst(h1, zp);
  Sygnaly sg;
  for (int i = 1; i <= 20; i++) sg.serie["pylek"].push_back(i);
  sg.odczyty["pylek"] = 18;  // P90 z 1…20 = 18
  OpcjeOceny o;
  o.sygnaly = &sg;
  std::vector<Wynik> wy = ocen(zp, kp, o);
  SPRAWDZ(znajdz(wy, "S-K-06") && znajdz(wy, "S-K-06")->oceniona && znajdz(wy, "S-K-06")->spelniona);
  sg.odczyty["pylek"] = 17.9;
  wy = ocen(zp, kp, o);
  SPRAWDZ(znajdz(wy, "S-K-06") && znajdz(wy, "S-K-06")->oceniona && !znajdz(wy, "S-K-06")->spelniona);
  sg.serie["pylek"].resize(5);
  wy = ocen(zp, kp, o);
  SPRAWDZ(znajdz(wy, "S-K-06") && !znajdz(wy, "S-K-06")->oceniona);
}

static void testPorownanieOkien() {
  // średnia z 3 ostatnich nocy wobec 3 wcześniejszych + Δ 3
  WK w = wk("O-04", "porownanie_okien", "metryka=M-02,stat=srednia,okno_a=T.n3,okno_b=T.n3,delta=T.delta,por=ge");
  Wynik r = ocenJeden(w, noce({1, 1, 1, 4, 4, 4}));
  SPRAWDZ(r.oceniona && r.spelniona);  // 4 ≥ 1 + 3
  ROWNE(r.zmienne["wartosc"], "4,0");
  ROWNE(r.zmienne["wartosc_b"], "1,0");
  r = ocenJeden(w, noce({1, 1, 1, 4, 4, 3.9}));
  SPRAWDZ(r.oceniona && !r.spelniona);
  r = ocenJeden(w, noce({1, 1, 4, 4, 4}));  // okno B ma 2 z 3 nocy
  SPRAWDZ(!r.oceniona);
  // bez delta i razy: A {por} B
  WK s = wk("O-04", "porownanie_okien", "metryka=M-02,okno_a=T.n3,okno_b=T.n3,por=lt");
  r = ocenJeden(s, noce({3, 3, 3, 1, 1, 1}));
  SPRAWDZ(r.oceniona && r.spelniona);
  // od zmiany ustawień (L-12): A = pierwsze noce po zmianie, B = noce przed nią
  WK z = wk("L-12", "porownanie_okien", "metryka=M-02,okno_a=T.n3,okno_b=T.n3,zdarzenie=zmiana_ustawien,por=ge,delta=T.delta");
  std::vector<Noc> h = noce({1, 1, 1, 5, 5, 5, 1});
  for (size_t i = 3; i < h.size(); i++) h[i].ustawienia = "ASVAuto/7/11/3/15";
  r = ocenJeden(z, h);  // A = 5 5 5 (noce 4–6), B = 1 1 1
  SPRAWDZ(r.oceniona && r.spelniona);
  h = noce({1, 1, 1, 5, 5});
  for (size_t i = 3; i < h.size(); i++) h[i].ustawienia = "ASVAuto/7/11/3/15";
  r = ocenJeden(z, h);  // po zmianie tylko 2 noce
  SPRAWDZ(!r.oceniona);
  r = ocenJeden(z, noce({1, 1, 1, 5, 5, 5}));  // bez zmiany ustawień
  SPRAWDZ(!r.oceniona);
}

static void testTrwanie() {
  WK e12 = wk("E-12", "prog_nocy", "metryka=M-02,prog=E-30.ahi");
  WK t = wk("L-08", "trwanie", "regula=E-12,min=T.min,jednostka=dni");  // ≥ 5 dni
  WK tn = wk("L-09", "trwanie", "regula=E-12,min=T.min,jednostka=noce");
  Zestaw z = zestaw({e12, t, tn});
  std::vector<Noc> h = noce({6, 6, 6, 6, 6, 6, 6});  // 10-01…10-07
  h.erase(h.begin() + 4);                            // brak nocy 10-05
  Kontekst k = kontekst(h, z);
  Pamiec p;
  p["E-12"].aktywnaOd = "2026-10-03";  // 10-03…10-07 = 5 dni, noce 10-03, 10-04, 10-06, 10-07 = 4
  k.pamiec = &p;
  std::vector<Wynik> r = ocen(z, k);
  auto w = [&](const char* id) { return znajdz(r, id) ? *znajdz(r, id) : Wynik(); };
  SPRAWDZ(w("L-08").oceniona && w("L-08").spelniona);
  ROWNE(w("L-08").zmienne["n"], "5");
  SPRAWDZ(w("L-09").oceniona && !w("L-09").spelniona);
  ROWNE(w("L-09").zmienne["n"], "4");
  p["E-12"].aktywnaOd = "2026-10-04";  // 4 dni
  r = ocen(z, k);
  SPRAWDZ(w("L-08").oceniona && !w("L-08").spelniona);
  p.clear();  // pierwsze spełnienie dziś: 1 dzień
  r = ocen(z, k);
  SPRAWDZ(w("L-08").oceniona && !w("L-08").spelniona);
  ROWNE(w("L-08").zmienne["n"], "1");
  k.pamiec = nullptr;  // bez pamięci → nie oceniam
  r = ocen(z, k);
  SPRAWDZ(!w("L-08").oceniona);
  // reguła niespełniona → trwanie niespełnione; reguła nie oceniona → nie oceniam
  k.pamiec = &p;
  h.back().ahi = 1;
  r = ocen(z, k);
  SPRAWDZ(w("L-08").oceniona && !w("L-08").spelniona);
  h.back().ahi = NAN;
  r = ocen(z, k);
  SPRAWDZ(!w("L-08").oceniona);
}

static void testOdZdarzenia() {
  WK w = wk("L-01", "od_zdarzenia", "zdarzenie=pierwsza_noc,max=T.min,jednostka=noce");  // w 5 nocach
  w.liczone = "z_rekordem";
  Wynik r = ocenJeden(w, noce({1, 1, 1, 1, 1}));
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["n"], "5");
  ROWNE(r.zmienne["data"], "2026-10-01");
  r = ocenJeden(w, noce({1, 1, 1, 1, 1, 1}));
  SPRAWDZ(r.oceniona && !r.spelniona);
  WK d = wk("H-04", "od_zdarzenia", "zdarzenie=zmiana_ustawien,min=T.n3,jednostka=dni");  // po ≥ 3 dniach
  std::vector<Noc> h = noce({1, 1, 1, 1, 1});
  h[2].ustawienia = "ASVAuto/7/11/3/15";
  h[3].ustawienia = "";  // puste = bez zmiany
  h[4].ustawienia = "ASVAuto/7/11/3/15";
  r = ocenJeden(d, h);  // zmiana 10-03, dziś 10-05 = 3. dzień
  SPRAWDZ(r.oceniona && r.spelniona);
  ROWNE(r.zmienne["data"], "2026-10-03");
  h.pop_back();
  r = ocenJeden(d, h);
  SPRAWDZ(r.oceniona && !r.spelniona);
  r = ocenJeden(d, noce({1, 1, 1}));  // bez zmiany: zdarzenia nie było
  SPRAWDZ(r.oceniona && !r.spelniona);
  WK np = wk("H-07", "od_zdarzenia", "zdarzenie=nowy_poziom,min=T.n3");
  r = ocenJeden(np, noce({1}));
  SPRAWDZ(!r.oceniona);
  WK bez = wk("H-07", "od_zdarzenia", "zdarzenie=pierwsza_noc");
  r = ocenJeden(bez, noce({1}));
  SPRAWDZ(!r.oceniona);
}

static void testOdpowiedziTygodni() {
  std::vector<Noc> h = noce({1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
  // dzis = 2026-10-28; tygodnie wstecz: [10-22..10-28], [10-15..10-21], [10-08..10-14], [10-01..10-07]
  Odpowiedzi o;
  o.sennosc["2026-10-27"] = true;
  o.sennosc["2026-10-20"] = false;
  o.sennosc["2026-10-21"] = true;  // ostatnia w tym tygodniu = tak
  o.sennosc["2026-10-02"] = false;
  WK w = wk("L-03", "odpowiedzi_tygodni", "pytanie=sennosc,odpowiedz=tak,kzn=T.kzn24");
  Zestaw z = zestaw({w});
  Kontekst k = kontekst(h, z);
  k.odpowiedzi = &o;
  Wynik r = jeden(w, k, z);
  SPRAWDZ(r.oceniona && r.spelniona);  // 2 z 4
  ROWNE(r.zmienne["n"], "2");
  o.sennosc["2026-10-21"] = false;
  r = jeden(w, k, z);
  SPRAWDZ(!r.oceniona);  // 1 tak, 1 tydzień bez odpowiedzi: może być 2 z 4
  o.sennosc["2026-10-10"] = false;
  r = jeden(w, k, z);
  SPRAWDZ(r.oceniona && !r.spelniona);
  // ostatnia odpowiedź (bez kzn)
  WK ost = wk("L-02", "odpowiedzi_tygodni", "pytanie=gorzej,odpowiedz=tak");
  r = ocenZ(ost, k);
  SPRAWDZ(!r.oceniona);  // brak odpowiedzi na „gorzej”
  o.gorzej["2026-10-20"] = true;
  o.gorzej["2026-10-29"] = false;  // po dzis: nie liczy się
  r = ocenZ(ost, k);
  SPRAWDZ(r.oceniona && r.spelniona);
  k.odpowiedzi = nullptr;
  r = jeden(w, k, z);
  SPRAWDZ(!r.oceniona);
  WK zlePyt = wk("L-02", "odpowiedzi_tygodni", "pytanie=humor,odpowiedz=tak");
  k.odpowiedzi = &o;
  r = ocenZ(zlePyt, k);
  SPRAWDZ(!r.oceniona);
}

static void testZawszeSygnalProgBiezacy() {
  WK z = wk("H-05", "zawsze", "");
  Wynik r = ocenJeden(z, noce({1}));
  SPRAWDZ(r.oceniona && r.spelniona);
  WK s = wk("E-01", "sygnal_komponentu", "sygnal=brak_danych");
  WK b = wk("E-05", "prog_biezacy", "metryka=M-25,prog=T.max,por=le");  // bateria ≤ 10 %
  Zestaw zs = zestaw({s, b});
  std::vector<Noc> h = noce({1});
  Kontekst k = kontekst(h, zs);
  std::vector<Wynik> w = ocen(zs, k);  // bez sygnałów: nie oceniam
  SPRAWDZ(znajdz(w, "E-01") && !znajdz(w, "E-01")->oceniona);
  SPRAWDZ(znajdz(w, "E-05") && !znajdz(w, "E-05")->oceniona);
  Sygnaly sg;
  sg.stan["brak_danych"] = true;
  sg.zmienne["n"] = "3";
  sg.odczyty["M-25"] = 10;
  OpcjeOceny o;
  o.sygnaly = &sg;
  w = ocen(zs, k, o);
  SPRAWDZ(znajdz(w, "E-01") && znajdz(w, "E-01")->oceniona && znajdz(w, "E-01")->spelniona);
  if (znajdz(w, "E-01")) ROWNE(znajdz(w, "E-01")->zmienne.at("n"), "3");
  SPRAWDZ(znajdz(w, "E-05") && znajdz(w, "E-05")->oceniona && znajdz(w, "E-05")->spelniona);
  sg.stan["brak_danych"] = false;
  sg.odczyty["M-25"] = 11;
  w = ocen(zs, k, o);
  SPRAWDZ(znajdz(w, "E-01") && znajdz(w, "E-01")->oceniona && !znajdz(w, "E-01")->spelniona);
  SPRAWDZ(znajdz(w, "E-05") && znajdz(w, "E-05")->oceniona && !znajdz(w, "E-05")->spelniona);
  sg.odczyty["M-25"] = NAN;
  w = ocen(zs, k, o);
  SPRAWDZ(znajdz(w, "E-05") && !znajdz(w, "E-05")->oceniona);
  SPRAWDZ(sygnalyOceny() == nullptr);  // poza ocen() sygnałów nie ma
}

static void testRejestrRodzajow() {
  for (const char* r : {"prog_nocy", "k_z_n_nocy", "kolejne_noce", "statystyka_okna", "okno_wobec_normy",
                        "stan_odchylenia", "porownanie_okien", "trwanie", "od_zdarzenia", "odpowiedzi_tygodni",
                        "zawsze", "zlozenie", "sygnal_komponentu", "prog_biezacy", "odchylenie_nocy", "trend",
                        "rekord_historii", "kamien", "czas_poza_progiem", "porownanie_grup", "percentyl_sezonu"})
    SPRAWDZ(znanyRodzaj(r));
  SPRAWDZ(!znanyRodzaj("nowy_rodzaj_testu"));
  zarejestrujRodzaj("nowy_rodzaj_testu", [](const WierszKatalogu& w, const Kontekst&) { return oceniona(w, true); });
  SPRAWDZ(znanyRodzaj("nowy_rodzaj_testu"));
  WK x = wk("E-98", "nowy_rodzaj_testu", "");
  Wynik r = ocenJeden(x, noce({1}));
  SPRAWDZ(r.oceniona && r.spelniona);
}


// Jednostki progów: próg w h wobec M-01 w minutach przeliczany, jednostka niezgodna
// z metryką = „nie oceniam”, jednostki bez znaczenia fizycznego bez zmian.
static void testJednostkiProgow() {
  std::vector<Noc> h = noce({1});
  h[0].m01 = 240;  // 4 h
  WK a = wk("R-DANE-13", "prog_nocy", "metryka=M-01,prog=T.h4,por=ge");
  Wynik r = ocenJeden(a, h);
  SPRAWDZ(r.oceniona && r.spelniona);  // 240 min ≥ 4 h, dokładnie na progu
  h[0].m01 = 239;
  r = ocenJeden(a, h);
  SPRAWDZ(r.oceniona && !r.spelniona);
  WK s = wk("X", "prog_nocy", "metryka=M-01,prog=T.s600,por=le");  // 600 s = 10 min
  h[0].m01 = 10;
  r = ocenJeden(s, h);
  SPRAWDZ(r.oceniona && r.spelniona);
  WK m = wk("X", "prog_nocy", "metryka=M-01,prog=T.min300,por=lt");
  r = ocenJeden(m, h);
  SPRAWDZ(r.oceniona && r.spelniona);
  WK l = wk("X", "prog_nocy", "metryka=M-07,prog=T.lmin24,por=ge");  // wp95 = 12
  r = ocenJeden(l, h);
  SPRAWDZ(r.oceniona && !r.spelniona);
  WK zla = wk("X", "prog_nocy", "metryka=M-02,prog=T.lmin24");  // L/min wobec AHI
  r = ocenJeden(zla, h);
  SPRAWDZ(!r.oceniona);
  SPRAWDZ(r.powodNieoceny.find("jednostka") != std::string::npos);
  WK zla2 = wk("X", "prog_nocy", "metryka=M-01,prog=T.cm2");
  SPRAWDZ(!ocenJeden(zla2, h).oceniona);
  WK proc = wk("X", "prog_nocy", "metryka=M-09p,prog=T.proc10,por=ge");  // CSR% = csr / m01
  h[0].m01 = 100;
  h[0].csr = 10;
  r = ocenJeden(proc, h);
  SPRAWDZ(r.oceniona && r.spelniona);
  // delta w godzinach (L-12: −1 h wobec okna przed) w porównaniu okien M-01
  WK d = wk("L-12", "porownanie_okien", "metryka=M-01,okno_a=T.n3,okno_b=T.n3,delta=T.hm1,por=le");
  std::vector<Noc> h2 = noce({1, 1, 1, 1, 1, 1});
  for (size_t i = 3; i < 6; i++) h2[i].m01 = 400 - 60;
  r = ocenJeden(d, h2);  // 340 ≤ 400 − 60
  SPRAWDZ(r.oceniona && r.spelniona);
  h2[5].m01 = 400 - 30;
  r = ocenJeden(d, h2);  // średnia z 340, 340, 370 > 340? mediana 340 ≤ 340
  SPRAWDZ(r.oceniona && r.spelniona);
  h2[4].m01 = 400 - 30;
  r = ocenJeden(d, h2);  // mediana 370 > 340
  SPRAWDZ(r.oceniona && !r.spelniona);
  // noc z SpO2 (L-06): rekord nie ma kanału SpO2 → „nie oceniam”
  WK sp = wk("L-06", "k_z_n_nocy", "metryka=M-02,prog=E-30.ahi,kzn=T.kzn");
  sp.liczone = "z_terapia_spo2";
  SPRAWDZ(!ocenJeden(sp, noce({1, 1, 1, 1, 1, 1, 1})).oceniona);
}

// Norma metryk bez pola w Norma (M-09 CSR, M-13 oddechy): liczona lokalnie tą samą metodą co liczNorme().
static void testNormaLokalna() {
  std::vector<Noc> h = noce(std::vector<double>(31, 1));
  for (size_t i = 0; i < h.size(); i++) h[i].m13 = 14.0f + (float)(i % 3);  // 14, 15, 16 …
  h.back().m13 = 19;
  WK w = wk("L-05", "okno_wobec_normy", "metryka=M-13,n=E-30.okno,wzgledem=mediana,delta=T.n3,por=ge");
  Norma nm;
  nm.n = 30;
  Zestaw z = zestaw({w});
  Kontekst k = kontekst(h, z);
  k.norma = &nm;
  Wynik r = jeden(w, k, z);  // mediana 7 ostatnich: 15/16/14… ≈ 15 wobec normy 15 + 3: nie
  SPRAWDZ(r.oceniona && !r.spelniona);
  ROWNE(r.zmienne["norma"], "15,0");
  for (size_t i = 24; i < h.size(); i++) h[i].m13 = 18;
  r = jeden(w, k, z);
  SPRAWDZ(r.oceniona && r.spelniona);  // 18 ≥ 15 + 3
  // noce wyłączone czynnikami (R-CZY-05) poza normą: wyłączenie 20 nocy zostawia 10 < 14 → brak normy
  std::set<std::string> wyl;
  for (size_t i = 0; i < 20; i++) wyl.insert(h[i].data);
  k.wykluczoneNoce = &wyl;
  SPRAWDZ(!jeden(w, k, z).oceniona);
  k.wykluczoneNoce = nullptr;
  std::vector<Noc> krotka = noce(std::vector<double>(10, 1));  // za mało nocy do normy
  for (auto& n : krotka) n.m13 = 18;
  k = kontekst(krotka, z);
  k.norma = &nm;
  SPRAWDZ(!jeden(w, k, z).oceniona);
}

static void testPamiec() {
  Pamiec p;
  p["L-03"].stanWzorca = "wyciszony_wiem";
  p["L-03"].wiem = "2026-10-01";
  p["L-03"].omowione = "2026-10-02";
  auto wynik = [](const char* id, const char* war, bool oc, bool sp) {
    Wynik w;
    w.id = id;
    w.wariant = war;
    w.oceniona = oc;
    w.spelniona = sp;
    return w;
  };
  aktualizujPamiec(p, {wynik("L-03", "", true, true), wynik("E-30", "", true, true), wynik("L-02", "a", true, true)},
                   "2026-10-05");
  ROWNE(p["L-03"].aktywnaOd, "2026-10-05");
  ROWNE(p["L-03"].ostatnio, "2026-10-05");
  ROWNE_L(p["L-03"].licznik, 1);
  ROWNE(p["L-03"].stanWzorca, "wyciszony_wiem");  // pola stanu wzorca, „wiem” i „Omówione” bez zmian
  ROWNE(p["L-03"].wiem, "2026-10-01");
  ROWNE(p["L-03"].omowione, "2026-10-02");
  SPRAWDZ(p.count("L-02/a") == 1);
  aktualizujPamiec(p, {wynik("L-03", "", true, true)}, "2026-10-05");  // drugi przebieg tego samego ranka
  ROWNE_L(p["L-03"].licznik, 1);
  aktualizujPamiec(p, {wynik("L-03", "", true, true), wynik("E-30", "", false, false)}, "2026-10-06");
  ROWNE(p["L-03"].aktywnaOd, "2026-10-05");
  ROWNE_L(p["L-03"].licznik, 2);
  ROWNE(p["E-30"].aktywnaOd, "2026-10-05");  // „nie oceniam” nie zmienia stanu
  aktualizujPamiec(p, {wynik("L-03", "", true, false)}, "2026-10-07");
  ROWNE(p["L-03"].aktywnaOd, "");
  ROWNE_L(p["L-03"].licznik, 0);
  ROWNE(p["L-03"].ostatnio, "2026-10-06");
  ROWNE(p["L-03"].wiem, "2026-10-01");
  // zapis i odczyt przez magazyn (format `reguly` w rejestrze)
  const FormatPliku* f = formatPliku(rejestrFormatow(), "reguly");
  SPRAWDZ(f != nullptr);
  if (f) {
    ROWNE(f->sciezka, "/dane/reguly.csv");
    ROWNE_L(f->wersja(), 1);
  }
  Dziennik d;
  AtrapaPlikow pl(d);
  pl.montujeSie = true;
  pl.montowane = true;
  std::vector<std::string> log;
  Magazyn m(pl, [&](const std::string& s) { log.push_back(s); });
  SPRAWDZ(czytajPamiec(m).empty());  // brak pliku
  p["X;zly"].ostatnio = "2026-10-01\n";  // znaki rozdzielające usunięte przy zapisie
  SPRAWDZ(zapiszPamiec(m, p));
  std::string t = pl.pliki[SCIEZKA_PAMIECI];
  SPRAWDZ(t.find("#cpap-eink reguly 1\nid;wariant;aktywna_od;ostatnio;licznik;stan_wzorca;wiem;omowione\n") == 0);
  SPRAWDZ(t.find("L-02;a;2026-10-05;2026-10-05;1;;;\n") != std::string::npos);
  Pamiec q = czytajPamiec(m);
  ROWNE_L(q.size(), p.size());
  ROWNE(q["L-03"].ostatnio, "2026-10-06");
  ROWNE(q["L-03"].stanWzorca, "wyciszony_wiem");
  ROWNE(q["L-02/a"].aktywnaOd, "2026-10-05");
  ROWNE_L(q["E-30"].licznik, 1);
  // wiersz z licznikiem nieczytelnym pominięty
  pl.pliki[SCIEZKA_PAMIECI] =
      "#cpap-eink reguly 1\nid;wariant;aktywna_od;ostatnio;licznik;stan_wzorca;wiem;omowione\n"
      "E-30;;2026-10-01;2026-10-01;x;;;\nE-31;;;;0;;;\n#koniec 2\n";
  q = czytajPamiec(m);
  ROWNE_L(q.size(), 1);
  SPRAWDZ(q.count("E-31") == 1);
}

int main() {
  testSha256Wektor();
  testTrybZUstawien();
  testWczytajZestawPoprawny();
  testWczytajZestawBledy();
  testMetrykaMedianaNoceOkna();
  testProgNocy();
  testKZNNocy();
  testStatystykaOkna();
  testZlozenie();
  testOcenMechanizmy();
  testTekst();
  testZestawWbudowanyE30();
  testKolejneNoce();
  testOknoWobecNormy();
  testStanOdchylenia();
  testOdchylenieNocy();
  testPorownanieOkien();
  testTrwanie();
  testOdZdarzenia();
  testOdpowiedziTygodni();
  testZawszeSygnalProgBiezacy();
  testRejestrRodzajow();
  testPamiec();
  testTrendRekordKamien();
  testCzasPozaProgiemGrupy();
  testJednostkiProgow();
  testNormaLokalna();
  printf("testy_silnik_regul: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
