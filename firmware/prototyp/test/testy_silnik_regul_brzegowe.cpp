// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (silnik reguł). Komentarz przy teście wskazuje sprawdzany przypadek;
// testy „trzyma” pilnują zachowań potwierdzonych wcześniej. Dane wyłącznie syntetyczne.
// Pomocnicze funkcje skopiowane z test/testy_silnik_regul.cpp (bez zmian poza dodatkowymi progami T.p51, T.p03, T.kzn0, T.p125).
// Uruchomienie: tools/prototyp/testy.sh.
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
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
    "T.p51;5.1;/h;wszystkie\n"
    "T.p03;0.3;/h;wszystkie\n"
    "T.kzn0;0/7;noce (k z n);wszystkie\n"
    "T.p125;1.25;x;wszystkie\n";

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


// wartości nocy są float (noc.h), progi double; epsilon 1e-9 w porownaj() nie wystarcza.
// AHI 5,1 (float 5.0999999) przy progu ge 5,1 → silnik mówi „nie”; AHI 0,3 (float 0.30000001) przy le 0,3 → „nie”.
static void testR01FloatNaProgu() {
  WK w = wk("E-30", "statystyka_okna", "metryka=M-02,stat=mediana,prog=T.p51,por=ge,n=E-30.okno");
  Wynik r = ocenJeden(w, noce({5.1, 5.1, 5.1, 5.1, 5.1, 5.1, 5.1}));
  SPRAWDZ(r.oceniona);
  SPRAWDZ(r.spelniona);  // mediana 5,1 ≥ próg 5,1
  WK p = wk("E-12", "prog_nocy", "metryka=M-02,prog=T.p51,por=ge");
  r = ocenJeden(p, noce({5.1}));
  SPRAWDZ(r.oceniona && r.spelniona);  // noc 5,1 ≥ 5,1
  WK l = wk("E-12", "prog_nocy", "metryka=M-02,prog=T.p03,por=le");
  r = ocenJeden(l, noce({0.3}));
  SPRAWDZ(r.oceniona && r.spelniona);  // noc 0,3 ≤ 0,3
  WK g = wk("E-12", "prog_nocy", "metryka=M-02,prog=T.p03,por=gt");
  r = ocenJeden(g, noce({0.3}));
  SPRAWDZ(r.oceniona && !r.spelniona);  // 0,3 nie jest > 0,3 (fałszywy alarm)
  // trzyma: double 0,1 + 0,2 wobec 0,3
  SPRAWDZ(porownaj(0.1 + 0.2, "le", 0.3) && porownaj(0.1 + 0.2, "ge", 0.3) && !porownaj(0.1 + 0.2, "gt", 0.3));
  SPRAWDZ(!porownaj(NAN, "ge", 1) && !porownaj(1, "le", NAN) && !porownaj(NAN, "lt", NAN));
  SPRAWDZ(porownaj(-1, "lt", 0) && porownaj(-2, "le", -2) && !porownaj(-2, "gt", -2));
}

// wykluczona reguła (R-EKR-48: „sprawa trwa, ekran nie”) ma oceniona=true, spelniona=false, więc
// aktualizujPamiec() kasuje jej aktywna_od i licznik. `trwanie` na tej regule liczy od nowa po każdym wykluczeniu.
static void testR02WykluczenieKasujePamiec() {
  WK a = wk("E-31", "zawsze", "");
  a.wyklucza = "E-30";
  WK b = wk("E-30", "zawsze", "");
  Zestaw z = zestaw({a, b});
  std::vector<Noc> h = noce({1});
  Kontekst k = kontekst(h, z);
  std::vector<Wynik> r = ocen(z, k);
  const Wynik* e30 = znajdz(r, "E-30");
  SPRAWDZ(e30 && !e30->spelniona && e30->zmienne.count("spelniona_przed_wykluczeniem"));
  Pamiec p;
  p["E-30"].aktywnaOd = "2026-09-20";
  p["E-30"].licznik = 11;
  aktualizujPamiec(p, r, k.dzis);
  ROWNE(p["E-30"].aktywnaOd, "2026-09-20");  // sprawa trwa, początek nie może zniknąć
  SPRAWDZ(p["E-30"].licznik >= 11);
}

// `trwanie` z regula=id (dozwolone w kontrakcie 6) dla reguły z wariantami szuka w pamięci klucza "id",
// a aktualizujPamiec() zapisuje "id/wariant". Początek zawsze = dziś, `min` nigdy nie zostaje osiągnięte.
static void testR03TrwanieWarianty() {
  WK va = wk("L-02", "zawsze", "");
  va.wariant = "a";
  WK vb = wk("L-02", "prog_nocy", "metryka=M-02,prog=E-30.ahi");
  vb.wariant = "b";
  WK t = wk("L-08", "trwanie", "regula=L-02,min=T.min,jednostka=dni");  // ≥ 5 dni
  Zestaw z = zestaw({va, vb, t});
  std::vector<Noc> h = noce({1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
  Kontekst k = kontekst(h, z);
  Pamiec p;
  std::vector<Wynik> r;
  for (int d = 0; d < 10; d++) {  // 10 poranków z L-02/a spełnioną, pamięć aktualizowana jak w dyrygencie
    k.dzis = h[(size_t)d].data;
    k.pamiec = &p;
    r = ocen(z, k);
    aktualizujPamiec(p, r, k.dzis);
  }
  const Wynik* l08 = znajdz(r, "L-08");
  SPRAWDZ(l08 && l08->oceniona);
  SPRAWDZ(l08 && l08->spelniona);  // L-02 spełniona 10 dni z rzędu, min 5
  if (l08) printf("  R-03: L-08 n=%s od=%s\n", l08->zmienne.count("n") ? l08->zmienne.at("n").c_str() : "?",
                  l08->zmienne.count("od") ? l08->zmienne.at("od").c_str() : "?");
}

// odpowiedzi_tygodni z kzn liczy tylko tygodnie z odpowiedzią; tygodnie bez odpowiedzi to „nie wiem”,
// a silnik daje „oceniona, niespełniona” (= w porządku), choć brakujące tygodnie mogą dać k (ADR-0029 pkt 7).
static void testR04OdpowiedziNiepelne() {
  WK w = wk("L-03", "odpowiedzi_tygodni", "pytanie=sennosc,odpowiedz=tak,kzn=T.kzn24");  // 2 z 4 tygodni
  std::vector<Noc> h = noce({1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
  Zestaw z = zestaw({w});
  Kontekst k = kontekst(h, z);
  Odpowiedzi o;
  o.sennosc[k.dzis] = true;  // tydzień 0: tak; tygodnie 1–3: brak odpowiedzi
  k.odpowiedzi = &o;
  Wynik r = jeden(w, k, z);
  SPRAWDZ(!r.oceniona);  // 1 „tak” z 1 znanego tygodnia, 3 nieznane: wynik nieznany
  o.sennosc.clear();
  o.sennosc[k.dzis] = false;  // jedno „nie”, trzy nieznane: nadal może być 2 z 4
  r = jeden(w, k, z);
  SPRAWDZ(!r.oceniona);
  o.sennosc[dodajDni(k.dzis, -7)] = false;
  o.sennosc[dodajDni(k.dzis, -14)] = false;  // trzy „nie”: 2 z 4 już niemożliwe → ocenione, niespełnione (trzyma)
  r = jeden(w, k, z);
  SPRAWDZ(r.oceniona && !r.spelniona);
}

// `nie` (zlozenie) i `wymaga` traktują regułę zablokowaną albo spoza trybu jak „oceniona, niespełniona”.
// `nie L-06` przy L-06 zablokowanej do daje „spełniona” (twierdzenie o czymś, czego panel nie liczy).
static void testR05NieZablokowanej() {
  WK b = wk("L-06", "zawsze", "");
  b.stan = "zablokowana";
  WK c = wk("H-09", "zlozenie", "op=nie,skladniki=L-06");
  Zestaw z = zestaw({b, c});
  std::vector<Noc> h = noce({1});
  Kontekst k = kontekst(h, z);
  std::vector<Wynik> r = ocen(z, k);
  const Wynik* x = znajdz(r, "H-09");
  SPRAWDZ(x && !x->oceniona);  // składnik nieliczony → „nie oceniam”, nie „nie ma L-06”
  WK s = wk("L-04", "zawsze", "");
  s.tryby = "AutoSet";  // nie działa w ASVAuto
  WK n2 = wk("H-10", "zlozenie", "op=nie,skladniki=L-04");
  Zestaw z2 = zestaw({s, n2});
  k.zestaw = &z2;
  r = ocen(z2, k);
  x = znajdz(r, "H-10");
  SPRAWDZ(x && !x->oceniona);
}

// stan_odchylenia nie sprawdza, czy stan był liczony z normą. Stany bez normy (norma.n = 0,
// aktywny = false z braku danych) → „oceniona, niespełniona” = w porządku. Ratuje tylko wymaga_normy=tak w katalogu.
static void testR06StanBezNormy() {
  WK w = wk("E-31", "stan_odchylenia", "metryka=M-02");
  std::vector<Noc> h = noce({9, 9, 9});
  Zestaw z = zestaw({w});
  Kontekst k = kontekst(h, z);
  Stany s;  // norma pusta, nic nie policzone
  k.stany = &s;
  Wynik r = jeden(w, k, z);
  SPRAWDZ(!r.oceniona);
}

// okno nocy reguły ograniczonej polem `tryby` bierze noce innych trybów obsługiwanych.
// E-30 (tryby ASV,ASVAuto) w ASVAuto po zmianie z AutoSet liczy medianę z nocy AutoSet (4.11: reguła w trybie
// spoza `tryby` nie działa; R-DANE-29 i 4.8: zmiana trybu = zmiana ustawień).
static void testR07OknoPrzezZmianeTrybu() {
  WK w = wk("E-30", "statystyka_okna", "metryka=M-02,stat=mediana,prog=E-30.ahi,por=ge,n=E-30.okno");
  std::vector<Noc> h = noce({12, 12, 12, 12, 1, 1, 1});
  for (size_t i = 0; i < 4; i++) h[i].ustawienia = "AutoSet/5/13/3";
  Wynik r = ocenJeden(w, h);
  SPRAWDZ(!r.oceniona);  // 3 noce ASVAuto z 7 → za mało nocy (dziś: spełniona z medianą 12 z nocy AutoSet)
  printf("  R-07: oceniona=%d spelniona=%d wartosc=%s\n", r.oceniona, r.spelniona,
         r.zmienne.count("wartosc") ? r.zmienne.at("wartosc").c_str() : "-");
}

// duplikaty dat w historii liczą tę samą noc dwa razy (k_z_n_nocy, mediana, rozruch).
static void testR08DuplikatDaty() {
  WK w = wk("E-22", "k_z_n_nocy", "metryka=M-02,prog=E-30.ahi,por=ge,kzn=T.kzn");  // 3 z 7
  std::vector<Noc> h = noce({1, 1, 1, 1, 1, 9, 9});
  h.push_back(h.back());  // ta sama noc drugi raz (np. raport częściowy i kompletny z tą samą datą)
  Wynik r = ocenJeden(w, h);
  SPRAWDZ(r.oceniona && !r.spelniona);  // 2 różne noce z AHI ≥ 5, nie 3
}

// próg k/n `0/7` i `1.5/3` przechodzą; 0/7 = reguła zawsze spełniona (fałszywy alarm z literówki).
static void testR09KznZleZapisy() {
  WK w = wk("E-22", "k_z_n_nocy", "metryka=M-02,prog=E-30.ahi,por=ge,kzn=T.kzn0");
  Wynik r = ocenJeden(w, noce({1, 1, 1, 1, 1, 1, 1}));
  SPRAWDZ(!r.oceniona);  // 0/7 nie jest sensownym progiem
}

// duplikat id tekstu i duplikat klucza reguły w pliku z karty przechodzą parser (drugi nadpisuje
// pierwszy, wynik zależy od kolejności); pakiet z karty nie musi przejść przez skrypt.
static void testR10DuplikatyWParserze() {
  std::string t = std::string(TEKSTY_T) + "E-30.tytul;tytul;30;Inny tytuł\n";
  std::string k = std::string(POLA_K) + linia(wk("E-30", "zawsze", "")) + linia(wk("E-30", "zawsze", ""));
  Zestaw z;
  std::string p;
  SPRAWDZ(!wczytajZestaw(plikZ("progi", PROGI_T), plikZ("katalog", POLA_K), plikZ("teksty", t), z, p));
  SPRAWDZ(!wczytajZestaw(plikZ("progi", PROGI_T), plikZ("katalog", k), plikZ("teksty", TEKSTY_T), z, p));
}

// spec techniczna 9.3: „wiersz z trybem …, a gdy takiego nie ma, wiersz `wszystkie`”, czyli kilka wierszy
// z jednym id_progu. Parser odrzuca powtórzony id_progu, więc mechanizm z 9.3 nie działa (dane obchodzą to
// sufiksami .cpap/.autoset, a wtedy reguła musi mieć osobny wariant na tryb).
static void testR11ProgTrybowyJakW93() {
  std::string pr = std::string(PROGI_T) + "T.tr;4;/h;ASVAuto\nT.tr;6;/h;wszystkie\n";
  Zestaw z;
  std::string p;
  bool ok1 = wczytajZestaw(plikZ("progi", pr), plikZ("katalog", POLA_K), plikZ("teksty", TEKSTY_T), z, p);
  SPRAWDZ(ok1);
  if (ok1) {
    SPRAWDZ(z.prog("T.tr", Tryb::ASVAUTO) && z.prog("T.tr", Tryb::ASVAUTO)->wartosc == 4);
    SPRAWDZ(z.prog("T.tr", Tryb::CPAP) && z.prog("T.tr", Tryb::CPAP)->wartosc == 6);
  }
}

// {prog:…} z wartością o dwóch miejscach po przecinku: skrypt sprawdza w spec „1,25”, panel pisze „1,3”.
static void testR12FormatProgu() {
  Zestaw z;
  std::string p;
  std::string t = std::string(TEKSTY_T) + "T.m;linia;60;Mnożnik {prog:T.p125}.\n";
  SPRAWDZ(wczytajZestaw(plikZ("progi", PROGI_T), plikZ("katalog", POLA_K), plikZ("teksty", t), z, p));
  Wynik w;
  ROWNE(tekst(z, "T.m", w, Tryb::ASVAUTO), "Mnożnik 1,25.");
}

// ---------- „trzyma”: sprawdzenia bez usterki ----------
static void testTrzymaNieOceniam() {
  std::vector<Noc> h = noce({9, 9, 9});
  // za mało nocy, brak metryki, tryb nieznany, wymaga_normy bez normy, noc niepewna
  WK a = wk("E-30", "statystyka_okna", "metryka=M-02,stat=mediana,prog=E-30.ahi,por=ge,n=E-30.okno");
  SPRAWDZ(!ocenJeden(a, h).oceniona);
  WK b = wk("E-30", "statystyka_okna", "metryka=M-13,stat=mediana,prog=E-30.ahi,por=ge,n=T.n3");
  SPRAWDZ(!ocenJeden(b, h).oceniona);
  WK c = wk("E-30", "statystyka_okna", "metryka=M-02,stat=mediana,prog=E-30.ahi,por=ge,n=T.n3");
  c.wymagaNormy = "tak";
  SPRAWDZ(!ocenJeden(c, h).oceniona);
  Zestaw z = zestaw({c});
  Kontekst k = kontekst(h, z);
  k.trybBiezacy = Tryb::NIEZNANY;
  SPRAWDZ(!jeden(c, k, z).oceniona);
  // „nie oceniam” w `i`: fałsz I nieznane = fałsz; prawda I nieznane = nieznane; LUB symetrycznie; `wymaga` nieznane
  WK nz = wk("A-1", "prog_nocy", "metryka=M-13,prog=E-30.ahi");  // M-13 bez wartości → nie oceniam
  WK pr = wk("A-2", "zawsze", "");
  WK i1 = wk("A-3", "zlozenie", "op=i,skladniki=A-1+A-2");
  WK l1 = wk("A-4", "zlozenie", "op=lub,skladniki=A-1+A-2");
  WK wy = wk("A-5", "zawsze", "");
  wy.wymaga = "A-1";
  WK nn = wk("A-6", "zlozenie", "op=nie,skladniki=A-1");
  Zestaw z2 = zestaw({nz, pr, i1, l1, wy, nn});
  k = kontekst(h, z2);
  std::vector<Wynik> r = ocen(z2, k);
  SPRAWDZ(znajdz(r, "A-3") && !znajdz(r, "A-3")->oceniona);
  SPRAWDZ(znajdz(r, "A-4") && znajdz(r, "A-4")->oceniona && znajdz(r, "A-4")->spelniona);
  SPRAWDZ(znajdz(r, "A-5") && !znajdz(r, "A-5")->oceniona);
  SPRAWDZ(znajdz(r, "A-6") && !znajdz(r, "A-6")->oceniona);
}

static void testTrzymaNoceOkna() {
  // przyszłość wobec dzis, nieposortowana historia, okno dłuższe niż historia, mediana parzysta
  std::vector<Noc> h = noce({1, 2, 3, 4, 50});
  std::vector<Noc> odwr(h.rbegin(), h.rend());
  Zestaw z = zestaw({});
  Kontekst k = kontekst(odwr, z);
  k.dzis = h[3].data;  // noc 50 jest „z przyszłości”
  std::vector<const Noc*> v = noceOkna(k, "wazne", 0);
  ROWNE_L(v.size(), 4);
  if (v.size() == 4) ROWNE(v.front()->data, h[0].data);
  ROWNE_L(noceOkna(k, "wazne", 10).size(), 4);
  SPRAWDZ(std::fabs(reguly::mediana({1, 2, 3, 4}) - 2.5) < 1e-12);
  SPRAWDZ(std::fabs(reguly::mediana({1, 2, 3, 4}) - ::mediana({1, 2, 3, 4})) < 1e-12);
}

static void testTrzymaTryby() {
  SPRAWDZ(trybZUstawien("ASVAuto/7/11.2/4/10") == Tryb::ASVAUTO);
  SPRAWDZ(trybZUstawien(" asv /8/3/15") == Tryb::ASV);
  SPRAWDZ(trybZUstawien("@@##") == Tryb::NIEZNANY);
  SPRAWDZ(trybZUstawien("/") == Tryb::NIEZNANY);
  SPRAWDZ(trybZUstawien("AutoSetHer/5/13") == Tryb::NIEZNANY);
  Zestaw z = zestaw({});
  SPRAWDZ(z.prog("T.asv", Tryb::ASV) && z.prog("T.asv", Tryb::ASVAUTO));
  SPRAWDZ(!z.prog("T.asv", Tryb::CPAP) && !z.prog("T.asv", Tryb::AUTOSET) && !z.prog("T.asv", Tryb::NIEZNANY));
  SPRAWDZ(z.prog("E-30.ahi", Tryb::CPAP) && z.prog("E-30.ahi", Tryb::NIEZNANY));
}

static void testTrzymaParser() {
  Zestaw z;
  std::string p;
  std::string pr = PROGI_T, k = POLA_K, t = TEKSTY_T;
  auto crlf = [](std::string s) {
    std::string o;
    for (char c : s) {
      if (c == '\n') o += '\r';
      o += c;
    }
    return o;
  };
  SPRAWDZ(wczytajZestaw(plikZ("progi", crlf(pr)), plikZ("katalog", crlf(k)), plikZ("teksty", crlf(t)), z, p));
  SPRAWDZ(!wczytajZestaw("\xEF\xBB\xBF" + plikZ("progi", pr), plikZ("katalog", k), plikZ("teksty", t), z, p));
  SPRAWDZ(!wczytajZestaw("", plikZ("katalog", k), plikZ("teksty", t), z, p));
  SPRAWDZ(!wczytajZestaw(plikZ("progi", pr), plikZ("katalog", k, "inny"), plikZ("teksty", t), z, p));
  std::string zly = plikZ("progi", pr);
  zly[zly.size() - 3] = '9';
  SPRAWDZ(!wczytajZestaw(zly, plikZ("katalog", k), plikZ("teksty", t), z, p));
  SPRAWDZ(!wczytajZestaw(plikZ("progi", pr + "X;1;h\n"), plikZ("katalog", k), plikZ("teksty", t), z, p));
  SPRAWDZ(!wczytajZestaw(plikZ("progi", pr), plikZ("katalog", k + "E-30;;zawsze;;\n"), plikZ("teksty", t), z, p));
  WK w = wk("E-30", "prog_nocy", "metryka=M-02,prog=BRAK");
  SPRAWDZ(!zestawZ({w}, z, p));
  // tekst z `;` w treści zostaje w całości
  SPRAWDZ(wczytajZestaw(plikZ("progi", pr), plikZ("katalog", k), plikZ("teksty", t), z, p));
  ROWNE(z.teksty["T.linia"].tekst.substr(0, 20), "Od {prog:T.godzina};");
  Wynik r;
  r.zmienne["n"] = "3";
  ROWNE(tekst(z, "T.linia", r, Tryb::ASVAUTO), "Od 18:00; połowa 1,5; brak –; –.");
}

int main() {
  testR01FloatNaProgu();
  testR02WykluczenieKasujePamiec();
  testR03TrwanieWarianty();
  testR04OdpowiedziNiepelne();
  testR05NieZablokowanej();
  testR06StanBezNormy();
  testR07OknoPrzezZmianeTrybu();
  testR08DuplikatDaty();
  testR09KznZleZapisy();
  testR10DuplikatyWParserze();
  testR11ProgTrybowyJakW93();
  testR12FormatProgu();
  testTrzymaNieOceniam();
  testTrzymaNoceOkna();
  testTrzymaTryby();
  testTrzymaParser();
  printf("testy_silnik_regul_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
