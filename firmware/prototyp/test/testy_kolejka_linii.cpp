// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: kolejka linii uwagi (7.5), ekran wzorca L (5.12.5),
// „Otwarte sprawy” (5.4.15), „wiem” i „Omówione” (5.12.7), pytania tak/nie (5.12.8), buzzer pilności A z ciszą nocną
// (5.14), warunek uspokojenia z wyników (5.4.13 pkt 1, 4, 7; ADR-0029 pkt 3 i 7), mapa przycisków (5.6.2).
// Napisane przed kodem (TDD) na atrapie wyników (test/atrapy/zestaw_kolejki.h) do czasu. Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_kolejka_linii).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <set>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "dzien.h"
#include "ekrany.h"
#include "kolejka.h"
#include "nawigacja.h"
#include "sprawy.h"
#include "stany.h"
#include "wspolne.h"
#include "wybudzenie.h"
#include "zestaw_kolejki.h"

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

using reguly::Pilnosc;
using reguly::StanReguly;
using reguly::Wynik;

static const std::string DZIS = "2026-11-05";  // czwartek
static const std::string NIEDZIELA = "2026-11-08";

static int znaki(const std::string& s) {  // punkty kodowe UTF-8
  int n = 0;
  for (unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}
static bool wszystkieZnakiWKroju(const std::string& s) {
  for (size_t i = 0; i < s.size();) {
    unsigned char c = (unsigned char)s[i];
    uint32_t cp;
    int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    cp = n == 1 ? c : (uint32_t)(c & (0xFF >> (n + 1)));
    for (int k = 1; k < n && i + (size_t)k < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i + (size_t)k] & 0x3F);
    if (!znakJestWKroju(cp)) {
      printf("  znak spoza kroju U+%04X w \"%s\"\n", cp, s.c_str());
      return false;
    }
    i += (size_t)n;
  }
  return true;
}

struct Scena {
  reguly::Zestaw z = atrapa_b5::zestaw();
  std::vector<Wynik> w;
  reguly::Pamiec pam;
  std::vector<sprawy::SprawaL> s;
  void wynik(const std::string& id, const std::string& war, bool oc, bool sp, const std::string& od = "") {
    w.push_back(atrapa_b5::wynik(z, id, war, oc, sp, {{"x", "12,4"}, {"n", "3"}, {"min", "42"}, {"m", "27"}}));
    if (!od.empty()) pam[sprawy::klucz(w.back())].aktywnaOd = od;
    // (R-PIE-08): rytm pełnego ekranu liczy się od rzeczywistego pokazania; scena zakłada pokazanie w dniu wejścia
    if (!od.empty()) sprawy::zapiszPokazanie(pam, sprawy::klucz(w.back()), od);
  }
  void licz() { s = sprawy::sprawyOtwarte(z, w, pam, atrapa_b5::funkcja(z)); }
  const sprawy::SprawaL* sprawa(const std::string& k) {
    for (auto& x : s)
      if (x.klucz == k) return &x;
    return nullptr;
  }
  std::vector<kolejka::LiniaUwagi> kolejka(const std::string& dzis, const reguly::Odpowiedzi* o = nullptr,
                                          const std::string& zap = "") {
    kolejka::WejscieKolejki k;
    k.dzis = dzis;
    k.sprawy = &s;
    k.odpowiedzi = o;
    k.ekranDnia = sprawy::sprawaEkranuDnia(s, dzis);
    k.zaproszenie = zap;
    k.pamiec = &pam;  // rytmy linii od rzeczywistego pokazania (R-PIE-08)
    return kolejka::kolejkaLinii(k);
  }
};

// ---------- sprawy otwarte: kto jest sprawą i w jakiej kolejności (R-EKR-48) ----------
static void testSprawyOtwarte() {
  Scena c;
  c.wynik("L-09", "", true, true, "2026-11-01");
  c.wynik("L-02", "a", true, true, "2026-10-30");     // „tylko raport”
  c.wynik("L-08", "", true, true, "2026-10-20");      // C
  c.wynik("L-11", "", true, true, "2026-11-02");      // B
  c.wynik("L-06", "b", true, true, "2026-11-05");     // A (reguła testowa)
  c.wynik("L-03", "10", false, true);                 // nie oceniam: nie jest sprawą
  c.wynik("L-04", "", true, false);                   // niespełniona
  c.wynik("L-06", "a", true, true);                   // zablokowana: nie liczy się wcale
  c.licz();
  {  // wiersz pomocniczy bez poziomu (np. `L-09/.wyjscie_e20`) nie jest sprawą ani ekranem dnia
    Wynik pom = atrapa_b5::wynik(c.z, "L-09", "", true, true);
    pom.wariant = ".wyjscie_e20";
    pom.poziom.clear();
    std::vector<Wynik> wp = {pom};
    SPRAWDZ(sprawy::sprawyOtwarte(c.z, wp, c.pam, atrapa_b5::funkcja(c.z)).empty());
  }
  ROWNE_L(c.s.size(), 5);
  if (c.s.size() == 5) {
    ROWNE(c.s[0].klucz, "L-06/b");
    ROWNE(c.s[1].klucz, "L-11");
    ROWNE(c.s[2].klucz, "L-08");
    ROWNE(c.s[3].klucz, "L-02/a");
    ROWNE(c.s[4].klucz, "L-09");
    SPRAWDZ(c.s[3].pilnosc == Pilnosc::BRAK);  // „tylko raport” bez pilności (5.12.2)
    SPRAWDZ(c.s[3].ton == sprawy::Ton::BRAK);  // bez ekranu lekarskiego (ADR-0029 pkt 3)
    SPRAWDZ(c.s[0].ton == sprawy::Ton::LEKARZ && c.s[4].ton == sprawy::Ton::PROBLEM);
    ROWNE(c.s[1].powod, "Senność w 3 z 4 tygodni, urządzenie liczy 12,4 / h.");  // tekst z zestawu ze zmiennymi
  }
}

// ---------- pełny ekran L i jego rytm (5.12.2) ----------
static void testEkranDnia() {
  Scena c;
  c.wynik("L-11", "", true, true, DZIS);
  c.wynik("L-06", "b", true, true, "2026-11-03");  // A od 2 dni: dziś nie pełny ekran (co 3 dni)
  c.licz();
  const sprawy::SprawaL* e = sprawy::sprawaEkranuDnia(c.s, DZIS);
  SPRAWDZ(e == nullptr);  // A dziś bez pełnego ekranu: zwykły ekran z linią A, B czeka
  e = sprawy::sprawaEkranuDnia(c.s, "2026-11-06");  // A: 3. dzień od wejścia → pełny ekran przed B
  SPRAWDZ(e && e->klucz == "L-06/b");
  sprawy::zapiszPokazanie(c.pam, "L-06/b", "2026-11-06");  // R-PIE-08: pokazany 6.11, więc 7.11 bez ekranu
  c.licz();
  e = sprawy::sprawaEkranuDnia(c.s, "2026-11-07");
  SPRAWDZ(e == nullptr);
  SPRAWDZ(sprawy::pelnyEkranDzis(*c.sprawa("L-11"), "2026-11-12"));  // B co 7 dni
  Scena tylkoB;  // bez A: B w dniu wejścia jest ekranem dnia
  tylkoB.wynik("L-11", "", true, true, DZIS);
  tylkoB.licz();
  e = sprawy::sprawaEkranuDnia(tylkoB.s, DZIS);
  SPRAWDZ(e && e->klucz == "L-11");
  // wzrost pilności od „wiem” kończy wyciszenie (5.12.7): „wiem” przy C, dziś B
  reguly::Pamiec pw;
  sprawy::zapiszWiem(pw, "L-11", DZIS, Pilnosc::C);
  SPRAWDZ(sprawy::pilnoscWzrosla(pw["L-11"].wiem, Pilnosc::B) && !sprawy::pilnoscWzrosla(pw["L-11"].wiem, Pilnosc::C));
  tylkoB.pam["L-11"].wiem = pw["L-11"].wiem;
  tylkoB.licz();
  SPRAWDZ(tylkoB.s[0].wiem.empty() && sprawy::sprawaEkranuDnia(tylkoB.s, DZIS) != nullptr);
  sprawy::zapiszOmowione(pw, "L-11", DZIS, Pilnosc::B);
  ROWNE(sprawy::dataWyciszenia(pw["L-11"].omowione), DZIS);
  // „tylko raport” nigdy nie jest ekranem dnia
  Scena r;
  r.wynik("L-01", "", true, true, DZIS);
  r.licz();
  SPRAWDZ(sprawy::sprawaEkranuDnia(r.s, DZIS) == nullptr);
}

// ---------- kolejka linii 7.5 ----------
static void testKolejka() {
  Scena c;
  c.wynik("L-06", "b", true, true, "2026-11-03");  // A, dziś bez pełnego ekranu → linia grupy 0
  c.wynik("L-11", "", true, true, "2026-11-02");   // B, 3. dzień → linia co 3 dni (grupa 2)
  c.wynik("L-02", "a", true, true, "2026-11-02");  // „tylko raport”: nigdy linii
  c.licz();
  reguly::Odpowiedzi o;
  o.sennosc[DZIS] = true;
  auto k = c.kolejka(DZIS, &o, "Co było inaczej? ↻ 2 s");
  ROWNE_L(k.size(), 3);  // zaproszenie odpada przy niewyciszonym A (R-PIE-06)
  if (k.size() == 3) {
    ROWNE(k[0].tekst, "Wciąż: Saturacja: sprawa dla lekarza.");
    SPRAWDZ(k[0].wzorzecA && k[0].zdrowie());
    ROWNE(k[1].tekst, "Senność w dzień: powiedz o tym lekarzowi.");
    SPRAWDZ(!k[1].wzorzecA && k[1].grupa == GrupaLinii::PILNOSC_A_SENNOSC);
    ROWNE(k[2].tekst, "Wciąż: Senność mimo terapii.");
    SPRAWDZ(k[2].grupa == GrupaLinii::WZORCE_L && k[2].zdrowie());
  }
  for (auto& l : k) SPRAWDZ(l.zrodlo != "L-02/a");
  DaneDnia d;
  kolejka::ustawLinie(d, k);
  SPRAWDZ(d.linia == "Wciąż: Saturacja: sprawa dla lekarza." && d.liniaA && d.liniaOstrzezenie);
  // w dniu pełnego ekranu A linii tej sprawy nie ma (sprawa stoi w polu głównym)
  auto k2 = c.kolejka("2026-11-06");
  for (auto& l : k2) SPRAWDZ(l.zrodlo != "L-06/b");
  // bez A: zaproszenie wraca, za liniami wzorców L, przed danymi; awaria przed wzorcami L
  Scena b;
  b.wynik("L-11", "", true, true, "2026-11-02");
  b.licz();
  kolejka::WejscieKolejki w;
  w.dzis = DZIS;
  w.sprawy = &b.s;
  w.zaproszenie = "Co było inaczej? ↻ 2 s";
  w.e08 = "Brak odczytu CO2.";
  w.awaria = "Pamięć panelu: dane tylko do odczytu.";
  auto k3 = kolejka::kolejkaLinii(w);
  ROWNE_L(k3.size(), 4);
  if (k3.size() == 4) {
    ROWNE(k3[0].zrodlo, "awaria");
    ROWNE(k3[1].zrodlo, "L-11");
    ROWNE(k3[2].zrodlo, "zaproszenie");
    ROWNE(k3[3].zrodlo, "E-08");
    SPRAWDZ(!k3[0].zdrowie() && !k3[2].zdrowie() && !k3[3].zdrowie());
  }
  // B w dzień 4. od wejścia: ani pełnego ekranu, ani linii (co 3 dni)
  ROWNE_L(b.kolejka("2026-11-03").size(), 0);
  // pilność C: linia tylko w niedzielę, z zestawu (R-SKI-12)
  Scena cc;
  cc.wynik("L-08", "", true, true, "2026-10-20");
  cc.wynik("L-07", "", true, true, "2026-10-20");  // „tylko raport” z linią C w zestawie: bez linii
  cc.licz();
  ROWNE_L(cc.kolejka(DZIS).size(), 0);
  auto kn = cc.kolejka(NIEDZIELA);
  ROWNE_L(kn.size(), 1);
  if (!kn.empty()) ROWNE(kn[0].tekst, "Regularność niższa od 4 tygodni. Na kontroli: raport z K2.");
  // „gorzej” = tak: linia R-SKI-36 przez 3 dni
  reguly::Odpowiedzi g;
  g.gorzej["2026-11-03"] = true;
  Scena pusta;
  ROWNE_L(pusta.kolejka(DZIS, &g).size(), 1);
  ROWNE_L(pusta.kolejka("2026-11-06", &g).size(), 0);
}

// ---------- „tylko raport”: bez linii, bez ekranu, bez buzzera, ale w sprawach i blokuje uspokojenie ----------
static void testTylkoRaport() {
  Scena c;
  c.wynik("L-04", "", true, true, DZIS);
  c.wynik("L-02", "a", true, true, "2026-10-01");
  c.licz();
  for (auto d : {DZIS, NIEDZIELA, std::string("2026-11-29")}) ROWNE_L(c.kolejka(d).size(), 0);
  SPRAWDZ(!kolejka::buzzerPilnosci(c.s, {}, DZIS, 9 * 60).graj);
  sprawy::WejscieSpraw ws;
  ws.dzis = DZIS;
  ws.sprawy = &c.s;
  auto karty = sprawy::kartySpraw(ws);
  ROWNE_L(karty.size(), 2);
  if (!karty.empty()) {
    ROWNE(karty[0].tytul, "Sprawa w raporcie");
    ROWNE(karty[0].dalej, "w raporcie, bez pilności");
    ROWNE(karty[0].koniec, "gdy warunek wyjścia wzorca się spełni");
  }
  // reguła „tylko raport” z samą linią C (L-07): powód = linia bez `Na kontroli: …`
  Scena l7;
  l7.wynik("L-07", "", true, true, DZIS);
  l7.licz();
  ws.sprawy = &l7.s;
  auto k7 = sprawy::kartySpraw(ws);
  if (!k7.empty()) ROWNE(k7[0].powod, "Ciśnienie rośnie od 2 tygodni.");
  auto oc = kolejka::ocenUspokojenie(c.w, {});
  SPRAWDZ(oc.wszystkieOcenione && !oc.pkt1);
}

// ---------- „wiem” i „Omówione” (5.12.7) ----------
static void testWiemOmowione() {
  Scena c;
  c.wynik("L-11", "", true, true, DZIS);
  c.licz();
  SPRAWDZ(sprawy::sprawaEkranuDnia(c.s, DZIS) != nullptr);
  sprawy::zapiszWiem(c.pam, "L-11", DZIS);
  ROWNE(c.pam["L-11"].wiem, DZIS + "@-");  // bez pilności w wywołaniu: `@-`
  c.licz();
  SPRAWDZ(sprawy::sprawaEkranuDnia(c.s, DZIS) == nullptr);  // pełny ekran schodzi do linii
  auto k = c.kolejka(DZIS);
  ROWNE_L(k.size(), 1);
  if (!k.empty()) ROWNE(k[0].tekst, "Wciąż: Senność mimo terapii.");
  sprawy::WejscieSpraw ws;
  ws.dzis = DZIS;
  ws.sprawy = &c.s;
  auto karty = sprawy::kartySpraw(ws);  // „wiem” zostaje w sprawach
  ROWNE_L(karty.size(), 1);
  if (!karty.empty()) {
    ROWNE(karty[0].tytul, "Senność mimo terapii");
    ROWNE(karty[0].ty, "wiem do 11.11");
    ROWNE(karty[0].dalej, "wraca do wyboru 12.11, obserwacja trwa");
  }
  SPRAWDZ(sprawy::wiemAktywne(*c.sprawa("L-11"), "2026-11-11") && !sprawy::wiemAktywne(*c.sprawa("L-11"), "2026-11-12"));
  SPRAWDZ(sprawy::pelnyEkranDzis(*c.sprawa("L-11"), "2026-11-12"));  // po 7 dniach znowu w rytmie co 7 dni
  sprawy::cofnijWiem(c.pam, "L-11");
  c.licz();
  SPRAWDZ(sprawy::sprawaEkranuDnia(c.s, DZIS) != nullptr);
  // „Omówione” przy B: cisza na 56 nocy, karta zostaje
  sprawy::zapiszOmowione(c.pam, "L-11", DZIS);
  c.licz();
  SPRAWDZ(sprawy::sprawaEkranuDnia(c.s, DZIS) == nullptr);
  ROWNE_L(c.kolejka(DZIS).size(), 0);
  ROWNE_L(c.kolejka("2026-11-08").size(), 0);
  karty = sprawy::kartySpraw(ws);
  if (!karty.empty()) {
    ROWNE(karty[0].ty, "omówione z lekarzem 05.11");
    ROWNE(karty[0].dalej, "cisza przez 56 nocy od 05.11, obserwacja trwa");
  }
  auto k56 = c.kolejka(dodajDni(DZIS, 56));  // R-SKI-11: raz, gdy warunek trwa
  ROWNE_L(k56.size(), 1);
  if (!k56.empty()) ROWNE(k56[0].tekst, "Senność mimo terapii: trwa 8 tyg. po wizycie.");
  // A po „Omówione”: bez pełnego ekranu, linia R-LEK-24 co 3 dni w grupie 2
  Scena a;
  a.wynik("L-06", "b", true, true, "2026-11-01");
  sprawy::zapiszOmowione(a.pam, "L-06/b", DZIS);
  a.licz();
  SPRAWDZ(sprawy::sprawaEkranuDnia(a.s, "2026-11-07") == nullptr);
  auto ka = a.kolejka("2026-11-08");
  ROWNE_L(ka.size(), 1);
  if (!ka.empty()) {
    ROWNE(ka[0].tekst, "Wciąż: Saturacja: sprawa dla lekarza. Omówione 05.11.");
    SPRAWDZ(ka[0].grupa == GrupaLinii::WZORCE_L && !ka[0].wzorzecA);
  }
  // rytm R-LEK-24 od rzeczywistego pokazania; pokazana 8.11, więc 9.11 bez linii
  sprawy::zapiszPokazanie(a.pam, "linia:L-06/b", "2026-11-08");
  ROWNE_L(a.kolejka("2026-11-09").size(), 0);
  // L-09 po „wiem”: przypomnienie w grupie 5, nie ostrzeżenie
  Scena p;
  p.wynik("L-09", "", true, true, DZIS);
  sprawy::zapiszWiem(p.pam, "L-09", DZIS);
  p.licz();
  auto kp = p.kolejka("2026-11-06");
  ROWNE_L(kp.size(), 1);
  if (!kp.empty()) SPRAWDZ(kp[0].grupa == GrupaLinii::PRZYPOMNIENIE && !kp[0].zdrowie());
}

// ---------- „Otwarte sprawy”: kolejność, metryki, linie, stan pusty ----------
static void testKartySpraw() {
  Scena c;
  c.wynik("L-08", "", true, true, "2026-10-20");
  c.wynik("L-11", "", true, true, "2026-11-01");
  c.licz();
  std::vector<sprawy::SprawaMetryki> m = {{"AHI", 9, "6,2", "0,8", "2,1", "6,2", "M-02", ""}};
  sprawy::WejscieSpraw ws;
  ws.dzis = DZIS;
  ws.sprawy = &c.s;
  ws.metryki = &m;
  ws.linieZdrowia = {"Senność w dzień: powiedz o tym lekarzowi."};
  auto k = sprawy::kartySpraw(ws);
  ROWNE_L(k.size(), 4);
  if (k.size() == 4) {
    ROWNE(k[0].tytul, "Senność mimo terapii");
    ROWNE(k[0].dalej, "linia co 3 dni, obserwacja trwa");  // B w dniu bez pełnego ekranu (R-SKI-37)
    ROWNE(k[1].tytul, "Sprawa na kontrolę");
    ROWNE(k[1].powod, "Regularność niższa od 4 tygodni.");
    ROWNE(k[1].dalej, "w niedzielnym podsumowaniu, K2 i raport");
    ROWNE(k[2].tytul, "Poza normą od 9 nocy");
    ROWNE(k[2].powod, "AHI 6,2 przy zwykłych 0,8–2,1.");
    ROWNE(k[3].tytul, "Sprawa z linii uwagi");
    ROWNE(k[3].dalej, "w linii uwagi");
  }
  ws.ekranDnia = c.sprawa("L-11");
  k = sprawy::kartySpraw(ws);
  if (!k.empty()) ROWNE(k[0].dalej, "na ekranie dnia");
  sprawy::WejscieSpraw p;
  p.dzis = DZIS;
  SPRAWDZ(sprawy::kartySpraw(p).empty());
  ROWNE(sprawy::kartaPusta(p).tytul, "Brak otwartych spraw.");
  p.ostatniaNiepewna = true;
  ROWNE(sprawy::kartaPusta(p).ocena, "ograniczona: ostatnia noc z dużym wyciekiem");
  DaneSpraw d = trescSpraw({}, 1, p);
  ROWNE_L(d.n, 0);
  ROWNE(d.naglowek, "Otwarte sprawy · cz 05.11");
  // linie zdrowia bez sprawy: z kolejki tylko te, których nie niesie sprawa L/E
  std::vector<kolejka::LiniaUwagi> kol = {{"Wciąż: Senność mimo terapii.", GrupaLinii::WZORCE_L, false, "L-11"},
                                          {"Senność w dzień: powiedz o tym lekarzowi.", GrupaLinii::PILNOSC_A_SENNOSC,
                                           false, "sennosc"},
                                          {"Co było inaczej? ↻ 2 s", GrupaLinii::PYTANIA, false, "zaproszenie"}};
  auto lz = kolejka::linieZdrowiaBezSprawy(kol);
  ROWNE_L(lz.size(), 1);
}

// ---------- warunek uspokojenia z wyników (5.4.13 pkt 1, 4, 7) ----------
static void testUspokojenie() {
  Scena c;
  c.wynik("L-11", "", true, false);
  c.wynik("L-03", "10", true, false);
  c.wynik("L-06", "a", false, false);  // zablokowana i nieoceniona: nie blokuje
  Stany st;
  st.nieoceniane = nieocenianeUspokojenia();
  st.uspokojenieOcenione = true;
  size_t przed = st.nieoceniane.size();
  auto o = kolejka::ocenUspokojenie(c.w, {});
  SPRAWDZ(o.silnik && o.wszystkieOcenione && o.pkt1 && o.pkt4);
  kolejka::zastosujUspokojenie(st, o);
  ROWNE_L(st.nieoceniane.size(), przed - 3);  // pkt 1, 4 i 7 ocenione
  for (auto& x : st.nieoceniane) SPRAWDZ(x.compare(0, 6, "pkt 1:") != 0 && x.compare(0, 6, "pkt 7:") != 0);
  SPRAWDZ(st.uspokojenieOcenione && st.ocenaNiepelna && !st.uspokojenie);  // pkt 5 dalej nieoceniany
  // ten sam zestaw, gdy reszta punktów byłaby oceniona: uspokojenie spełnione
  Stany pelna;
  pelna.uspokojenieOcenione = true;
  pelna.nieoceniane = {"pkt 1: wzorce L (L-01…L-11)", "pkt 4: linie grupy „zdrowie” (7.5)",
                       "pkt 7: każda reguła zestawu policzona (R-AW-31)"};
  kolejka::zastosujUspokojenie(pelna, o);
  SPRAWDZ(pelna.uspokojenie);
  // „nie oceniam” jednej aktywnej reguły: punkty zostają nieoceniane, uspokojenie niespełnione
  Scena n;
  n.wynik("L-11", "", false, false);
  n.wynik("L-03", "10", true, false);
  auto on = kolejka::ocenUspokojenie(n.w, {});
  SPRAWDZ(!on.wszystkieOcenione && on.nieOceniam.size() == 1);
  Stany sn = pelna;
  sn.uspokojenie = true;
  sn.nieoceniane = {"pkt 1: wzorce L (L-01…L-11)", "pkt 7: każda reguła zestawu policzona (R-AW-31)"};
  kolejka::zastosujUspokojenie(sn, on);
  SPRAWDZ(!sn.uspokojenie && sn.ocenaNiepelna && !sn.uspokojenieOcenione);
  // „nie oceniam” reguły „tylko raport” też blokuje
  Scena t;
  t.wynik("L-02", "a", false, false);
  SPRAWDZ(!kolejka::ocenUspokojenie(t.w, {}).wszystkieOcenione);
  // spełniona L (wyciszona „wiem”) i linia zdrowia: pkt 1 i 4 niespełnione
  Scena s;
  s.wynik("L-11", "", true, true, DZIS);
  sprawy::zapiszWiem(s.pam, "L-11", DZIS);
  s.licz();
  auto ow = kolejka::ocenUspokojenie(s.w, s.kolejka(DZIS));
  SPRAWDZ(!ow.pkt1 && !ow.pkt4);
  // wiersz pomocniczy bez poziomu (np. `E-33/mediana`) z „nie oceniam” nie blokuje uspokojenia; z poziomem blokuje
  {
    std::vector<Wynik> wp = {atrapa_b5::wynik(c.z, "L-11", "", true, false)};
    Wynik pom;
    pom.id = "E-33";
    pom.wariant = "mediana";
    pom.oceniona = false;
    pom.powodNieoceny = "za malo nocy";
    wp.push_back(pom);
    auto op = kolejka::ocenUspokojenie(wp, {});
    SPRAWDZ(op.wszystkieOcenione && op.pkt1 && op.nieOceniam.empty());
    wp.back().poziom = "problem";
    SPRAWDZ(!kolejka::ocenUspokojenie(wp, {}).wszystkieOcenione);
  }
  // bez wyników silnika nic się nie zmienia
  Stany bez;
  bez.nieoceniane = nieocenianeUspokojenia();
  kolejka::zastosujUspokojenie(bez, kolejka::ocenUspokojenie({}, {}));
  ROWNE_L(bez.nieoceniane.size(), przed);
  bool jest7 = false;
  for (auto& x : nieocenianeUspokojenia()) jest7 = jest7 || x.compare(0, 6, "pkt 7:") == 0;
  SPRAWDZ(jest7);
}

// ---------- buzzer pilności A i cisza nocna (5.14) ----------
static void testBuzzer() {
  SPRAWDZ(!kolejka::ciszaNocna(21 * 60 + 59) && kolejka::ciszaNocna(22 * 60) && kolejka::ciszaNocna(0));
  SPRAWDZ(kolejka::ciszaNocna(7 * 60 + 59) && !kolejka::ciszaNocna(8 * 60));
  SPRAWDZ(kolejka::ciszaNocna(9 * 60 + 59, 22 * 60, 10 * 60) && !kolejka::ciszaNocna(10 * 60, 22 * 60, 10 * 60));
  Scena c;
  c.wynik("L-06", "b", true, true, DZIS);
  c.wynik("L-11", "", true, true, DZIS);  // B: bez dźwięku
  c.licz();
  auto d = kolejka::buzzerPilnosci(c.s, {}, DZIS, 6 * 60);  // import o 06:00: cisza nocna (R-BUZ-02)
  SPRAWDZ(!d.graj && d.odlozony);
  d = kolejka::buzzerPilnosci(c.s, {}, DZIS, 8 * 60);  // R-BUZ-02b: po końcu ciszy
  SPRAWDZ(d.graj && d.znaczniki.size() == 1);
  std::set<std::string> zagrane(d.znaczniki.begin(), d.znaczniki.end());
  SPRAWDZ(!kolejka::buzzerPilnosci(c.s, zagrane, DZIS, 9 * 60).graj);       // raz na wejście (R-BUZ-04)
  SPRAWDZ(!kolejka::buzzerPilnosci(c.s, zagrane, "2026-11-08", 9 * 60).graj);  // ponowny pełny ekran: cisza
  SPRAWDZ(!kolejka::buzzerPilnosci(c.s, {}, DZIS, 9 * 60, false).graj);    // R-BUZ-03
  // „wiem” przed końcem ciszy: odłożony sygnał nie gra (R-BUZ-05)
  sprawy::zapiszWiem(c.pam, "L-06/b", DZIS);
  c.licz();
  SPRAWDZ(!kolejka::buzzerPilnosci(c.s, {}, DZIS, 8 * 60).graj);
  // nowe wejście po wyjściu: nowy znacznik, sygnał znowu (R-BUZ-06)
  Scena n;
  n.wynik("L-06", "b", true, true, "2026-11-30");
  n.licz();
  SPRAWDZ(kolejka::buzzerPilnosci(n.s, zagrane, "2026-11-30", 9 * 60).graj);
  // „tylko raport” z pilnością docelową A w katalogu: nigdy buzzera
  Scena t;
  t.z.katalog.push_back(atrapa_b5::wiersz("L-99", "", StanReguly::TYLKO_RAPORT, Pilnosc::A, "R2"));
  t.wynik("L-99", "", true, true, DZIS);
  t.licz();
  SPRAWDZ(!kolejka::buzzerPilnosci(t.s, {}, DZIS, 9 * 60).graj);
  ROWNE_L(kolejka::SYGNALY_A, 3);
}

// ---------- pytania tak/nie (5.12.8) i plik odpowiedzi ----------
static void testPytania() {
  sprawy::Odpowiedz o;
  SPRAWDZ(sprawy::parsujOdpowiedz("2026-11-08;sennosc;tak", o) && o.odp == 't' && o.pytanie == sprawy::Pytanie::SENNOSC);
  ROWNE(sprawy::wierszOdpowiedzi(o), "2026-11-08;sennosc;tak");
  SPRAWDZ(!sprawy::parsujOdpowiedz("2026-11-08;sennosc;moze", o) && !sprawy::parsujOdpowiedz("x;gorzej;tak", o));
  std::vector<sprawy::Odpowiedz> l;
  sprawy::zapiszOdpowiedz(l, {"2026-11-08", sprawy::Pytanie::SENNOSC, 'b'});
  sprawy::zapiszOdpowiedz(l, {"2026-11-05", sprawy::Pytanie::GORZEJ, 't'});
  sprawy::zapiszOdpowiedz(l, {"2026-11-08", sprawy::Pytanie::SENNOSC, 'n'});  // zastępuje „brak”
  ROWNE_L(l.size(), 2);
  reguly::Odpowiedzi r = sprawy::odpowiedziSilnika(l);
  SPRAWDZ(r.sennosc.count("2026-11-08") && !r.sennosc["2026-11-08"] && r.gorzej["2026-11-05"]);
  std::vector<sprawy::Odpowiedz> b = {{"2026-11-08", sprawy::Pytanie::SENNOSC, 'b'}};
  SPRAWDZ(sprawy::odpowiedziSilnika(b).sennosc.empty());  // brak odpowiedzi to nie „nie” (R-SKI-17)
  // senność: niedziela od 17:00, do odpowiedzi
  Scena c;
  c.licz();
  std::vector<sprawy::Odpowiedz> brak;
  kolejka::WejsciePytan w;
  w.dzis = NIEDZIELA;
  w.minuta = 17 * 60;
  w.sprawy = &c.s;
  w.odpowiedzi = &brak;
  auto p = kolejka::pytanieDnia(w);
  SPRAWDZ(p.jest && p.pytanie == sprawy::Pytanie::SENNOSC && p.data == NIEDZIELA);
  w.minuta = 16 * 60 + 59;
  SPRAWDZ(!kolejka::pytanieDnia(w).jest);
  w.minuta = 18 * 60;
  std::vector<sprawy::Odpowiedz> jest = {{NIEDZIELA, sprawy::Pytanie::SENNOSC, 'n'}};
  w.odpowiedzi = &jest;
  SPRAWDZ(!kolejka::pytanieDnia(w).jest);
  w.odpowiedzi = &brak;
  w.dzis = DZIS;
  SPRAWDZ(!kolejka::pytanieDnia(w).jest);  // nie niedziela
  // niewyciszony wzorzec A: bez pytań (R-SKI-16)
  Scena a;
  a.wynik("L-06", "b", true, true, NIEDZIELA);
  a.licz();
  w.dzis = NIEDZIELA;
  w.sprawy = &a.s;
  SPRAWDZ(!kolejka::pytanieDnia(w).jest);
  // „gorzej”: rano przy spełnionym L-05 („tylko raport” też, bo pytanie zbiera odpowiedź do raportu i pilności)
  Scena g;
  g.z.katalog.push_back(atrapa_b5::wiersz("L-05", "", StanReguly::TYLKO_RAPORT, Pilnosc::B, "R3"));
  g.wynik("L-05", "", true, true, DZIS);
  g.licz();
  kolejka::WejsciePytan wg;
  wg.dzis = DZIS;
  wg.minuta = 7 * 60;
  wg.sprawy = &g.s;
  wg.odpowiedzi = &brak;
  p = kolejka::pytanieDnia(wg);
  SPRAWDZ(p.jest && p.pytanie == sprawy::Pytanie::GORZEJ);
  wg.minuta = 11 * 60;
  SPRAWDZ(!kolejka::pytanieDnia(wg).jest);  // po terminie
  wg.minuta = 7 * 60;
  std::vector<sprawy::Odpowiedz> wczoraj = {{"2026-11-04", sprawy::Pytanie::GORZEJ, 'n'}};
  wg.odpowiedzi = &wczoraj;
  SPRAWDZ(!kolejka::pytanieDnia(wg).jest);  // najwyżej raz na 3 dni
  // przerwa planowana: powrót po 4 nocach bez terapii
  std::vector<Noc> h;
  for (int i = 20; i >= 0; i--) {
    if (i >= 1 && i <= 4) continue;  // 4 noce bez rekordu
    Noc n;
    n.data = dodajDni(DZIS, -i);
    n.m01 = 400;
    n.wazna = true;
    h.push_back(n);
  }
  Scena pusta;
  pusta.licz();
  kolejka::WejsciePytan wp;
  wp.dzis = DZIS;
  wp.minuta = 7 * 60;
  wp.sprawy = &pusta.s;
  wp.odpowiedzi = &brak;
  wp.hist = &h;
  p = kolejka::pytanieDnia(wp);
  SPRAWDZ(p.jest && p.pytanie == sprawy::Pytanie::PRZERWA && p.przerwaNocy == 4 && p.data == DZIS);
  std::vector<sprawy::Odpowiedz> odp = {{DZIS, sprawy::Pytanie::PRZERWA, 't'}};
  wp.odpowiedzi = &odp;
  SPRAWDZ(!kolejka::pytanieDnia(wp).jest);  // raz na przerwę
  wp.odpowiedzi = &brak;
  std::vector<Noc> krotka(h.begin() + 16, h.end());  // historia zaczyna się po przerwie: to nie przerwa
  wp.hist = &krotka;
  SPRAWDZ(!kolejka::pytanieDnia(wp).jest);
}

// ---------- mapa przycisków (5.6.2) ----------
static void testPrzyciski() {
  KontekstNaw k;
  k.wzorzec = true;
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, true, k).akcja == Akcja::WIEM &&
          nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, true, k).ekran == Ekran::PRZENIESIONE);
  // na ekranie L ◀ krótko nic (z ekranu dnia nie ma „wstecz”);: ↻ krótko = ekran dnia od nowa (reakcja)
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, k).akcja == Akcja::POKAZ);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k).ekran == Ekran::OMOWIONE);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, false, k).akcja == Akcja::NIC);
  // ▶ = K1 jak na każdym ekranie dnia, „Raport dla lekarza” pod ▶ 2 s
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, k).ekran == Ekran::K1);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, true, k).ekran == Ekran::RAPORT);  // „Raport dla lekarza”
  SPRAWDZ(nacisniecie(Ekran::RAPORT, Przycisk::LEWY, false, k).ekran == Ekran::DZIEN);
  SPRAWDZ(nacisniecie(Ekran::RAPORT, Przycisk::PRAWY, false, k).ekran == Ekran::K1);
  SPRAWDZ(nacisniecie(Ekran::RAPORT, Przycisk::SRODEK, false, k).ekran == Ekran::DZIEN);  // ↻ dom
  ROWNE_L(sekundDoPowrotu(Ekran::RAPORT, 5, 2 * 60), 15);
  k.problem = true;  // L-09: ◀ 2 s nic
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k).akcja == Akcja::NIC &&
          nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k).ekran == Ekran::DZIEN);
  KontekstNaw q;
  SPRAWDZ(nacisniecie(Ekran::OMOWIONE, Przycisk::PRAWY, false, q).akcja == Akcja::OMOWIONE_TAK);
  SPRAWDZ(nacisniecie(Ekran::OMOWIONE, Przycisk::LEWY, false, q).ekran == Ekran::DZIEN &&
          nacisniecie(Ekran::OMOWIONE, Przycisk::LEWY, false, q).akcja == Akcja::POKAZ);
  SPRAWDZ(nacisniecie(Ekran::OMOWIONE, Przycisk::SRODEK, false, q).akcja == Akcja::POKAZ);
  SPRAWDZ(nacisniecie(Ekran::OMOWIONE, Przycisk::PRAWY, true, q).akcja == Akcja::NIC);
  ROWNE_L(sekundDoPowrotu(Ekran::OMOWIONE, 10, 2 * 60), 50);  // R-PRZ-05, także w nocy
  ROWNE_L(sekundDoPowrotu(Ekran::OMOWIONE, 61, 9 * 60), 0);
  KontekstNaw s;
  // „Otwarte sprawy” to strona 3 / 6 (po K2); bez wyników silnika pominięta (K2 ▶ S8)
  SPRAWDZ(nacisniecie(Ekran::K2, Przycisk::PRAWY, false, s).ekran == Ekran::NORMA);
  s.sprawyDostepne = true;
  SPRAWDZ(nacisniecie(Ekran::K2, Przycisk::PRAWY, false, s).ekran == Ekran::SPRAWY);
  s.sprawa = 1;
  s.spraw = 3;
  SPRAWDZ(nacisniecie(Ekran::SPRAWY, Przycisk::PRAWY, false, s).akcja == Akcja::SPRAWY_DALEJ);
  SPRAWDZ(nacisniecie(Ekran::SPRAWY, Przycisk::LEWY, false, s).ekran == Ekran::K2);
  s.sprawa = 3;
  SPRAWDZ(nacisniecie(Ekran::SPRAWY, Przycisk::PRAWY, false, s).ekran == Ekran::NORMA);  // za ostatnią: S8
  SPRAWDZ(nacisniecie(Ekran::SPRAWY, Przycisk::LEWY, false, s).akcja == Akcja::SPRAWY_WSTECZ);
  SPRAWDZ(nacisniecie(Ekran::SPRAWY, Przycisk::SRODEK, false, s).ekran == Ekran::DZIEN);
  s.spraw = 0;
  s.sprawa = 1;
  SPRAWDZ(nacisniecie(Ekran::SPRAWY, Przycisk::LEWY, false, s).ekran == Ekran::K2);  // stan pusty
  ROWNE_L(sekundDoPowrotu(Ekran::SPRAWY, 0, 9 * 60), 300);
  ROWNE_L(sekundDoPowrotu(Ekran::SPRAWY, 0, 2 * 60), -1);
  KontekstNaw p;
  p.pytanieTN = true;
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, false, p).akcja == Akcja::ODP_NIE);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, p).akcja == Akcja::ODP_TAK);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, p).ekran == Ekran::ZAPISANO);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, p).akcja == Akcja::ODP_POMIN);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, p).ekran == Ekran::DZIEN);  // pominięcie bez „Zapisano”
  // „Przeniesione” (R-PRZ-16, R-PRZ-17) i „Zapisano” (R-PRZ-11, R-PRZ-12)
  KontekstNaw c;
  // cofnięcie „wiem” przytrzymaniem ◀ jak na „Zapisano”; krótkie ↻ = dom („wiem” zostaje)
  SPRAWDZ(nacisniecie(Ekran::PRZENIESIONE, Przycisk::LEWY, true, c).akcja == Akcja::COFNIJ_WIEM);
  SPRAWDZ(nacisniecie(Ekran::PRZENIESIONE, Przycisk::SRODEK, false, c).akcja == Akcja::POKAZ);
  SPRAWDZ(nacisniecie(Ekran::PRZENIESIONE, Przycisk::PRAWY, false, c).akcja == Akcja::POKAZ);
  SPRAWDZ(nacisniecie(Ekran::ZAPISANO, Przycisk::LEWY, true, c).akcja == Akcja::COFNIJ_ODP);
  SPRAWDZ(nacisniecie(Ekran::ZAPISANO, Przycisk::LEWY, false, c).akcja == Akcja::POKAZ);
  SPRAWDZ(nacisniecie(Ekran::ZAPISANO, Przycisk::SRODEK, false, c).ekran == Ekran::DZIEN);
  c.cofniecie = false;  // w nocy po swoim czasie cofnięcie przestaje działać (R-PRZ-19)
  SPRAWDZ(nacisniecie(Ekran::PRZENIESIONE, Przycisk::LEWY, true, c).akcja == Akcja::POKAZ);
  SPRAWDZ(nacisniecie(Ekran::ZAPISANO, Przycisk::LEWY, true, c).akcja == Akcja::POKAZ);
  ROWNE_L(sekundDoPowrotu(Ekran::PRZENIESIONE, 10, 9 * 60), 20);
  ROWNE_L(sekundDoPowrotu(Ekran::ZAPISANO, 10, 9 * 60), 50);
  ROWNE_L(sekundDoPowrotu(Ekran::PRZENIESIONE, 10, 1 * 60), -1);  // w nocy zostaje do 05:00
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, true, p).akcja == Akcja::NIC);
}

// ---------- szerokości tekstów (5.4.4) i układ ----------
static void testSzerokosci() {
  Scena c;
  for (auto& k : c.z.katalog) c.wynik(k.id, k.wariant, true, true, DZIS);
  c.licz();
  for (auto& s : c.s) {
    bool lek = s.ton != sprawy::Ton::PROBLEM;
    if (!s.tytul.empty()) {
      SPRAWDZ(znaki(s.tytul) <= 30);
      if (szer(F_SB44, s.tytul) > szerTytuluWzorca(lek)) printf("  tytuł %s: %d px\n", s.tytul.c_str(), szer(F_SB44, s.tytul));
      SPRAWDZ(szer(F_SB44, s.tytul) <= szerTytuluWzorca(lek));
      SPRAWDZ(wszystkieZnakiWKroju(s.tytul));
      std::string l = "Wciąż: " + s.tytul + ". Omówione 30.11.";  // najdłuższa linia z tytułem (R-LEK-24, 24 px)
      SPRAWDZ(znaki(l) <= 60);
      SPRAWDZ(szer(F_P24B, "! " + l) <= 744);
      SPRAWDZ(szer(F_P28B, "! Wciąż: " + s.tytul + ".") <= 744);  // linia A 28 px (R-EKR-42)
    }
    if (!s.powod.empty()) {
      SPRAWDZ(znaki(s.powod) <= 70);
      SPRAWDZ(zawin(F_P28B, s.powod, szerTresciWzorca(lek)).size() <= 2);
      SPRAWDZ(zawin(F_P28B, s.powod, 744).size() <= 2);  // karta „Otwartych spraw”
      SPRAWDZ(wszystkieZnakiWKroju(s.powod));
    }
    if (!s.krok.empty()) {
      SPRAWDZ(znaki(s.krok) <= 60);
      if (szer(F_P28B, s.krok) > szerTresciWzorca(lek)) printf("  krok %s: %d px\n", s.krok.c_str(), szer(F_P28B, s.krok));
      SPRAWDZ(szer(F_P28B, s.krok) <= szerTresciWzorca(lek));
    }
    if (!s.linia.empty()) {
      SPRAWDZ(znaki(s.linia) <= 60);
      SPRAWDZ(szer(F_P24B, "! " + s.linia) <= 744);
    }
  }
  for (auto p : {Pilnosc::A, Pilnosc::B}) {
    std::string e = wersaliki(etykietaPilnosci(p));
    SPRAWDZ(szer(F_P22B, e, 2) <= szerTresciWzorca(true));
  }
  SPRAWDZ(szer(F_P24B, ZDANIE_R_LEK_28) <= szerTresciWzorca(true));
  SPRAWDZ(szerStopki(stopkaWzorca(true)) <= szerTresciWzorca(true));
  for (auto& x : c.s)
    if (x.ton == sprawy::Ton::LEKARZ)
      SPRAWDZ(szer(F_P28B, "Nie udało się przygotować raportu (" + std::string(KOD_RAPORTU_BRAK) + ")") <= 744 &&
              szer(F_SB44, x.tytul) <= 744);
  // wiersze kart „Otwartych spraw” (≤ 60 znaków, od x = 140 do 772)
  std::vector<std::string> wiersze = {"ograniczona: 12 z 14 nocy pominięte (duży wyciek)",
                                      "ograniczona: ostatnia noc z dużym wyciekiem",
                                      "cisza przez 56 nocy od 30.11, obserwacja trwa",
                                      "wraca do wyboru 30.11, obserwacja trwa",
                                      "dziś ekranem jest inna sprawa, obserwacja trwa",
                                      "w niedzielnym podsumowaniu, K2 i raport",
                                      "gdy warunek zdarzenia przestanie być spełniony",
                                      "gdy warunek wyjścia wzorca się spełni", "omówione z lekarzem 30.11"};
  for (auto& w : wiersze) {
    SPRAWDZ(znaki(w) <= 60);
    if (szer(F_P24B, w) > 772 - SPRAWY_X_WARTOSCI) printf("  wiersz %s: %d px\n", w.c_str(), szer(F_P24B, w));
    SPRAWDZ(szer(F_P24B, w) <= 772 - SPRAWY_X_WARTOSCI);
    SPRAWDZ(wszystkieZnakiWKroju(w));
  }
  for (auto& t : {"Sprawa w raporcie", "Sprawa na kontrolę", "Poza normą od 120 nocy", "Sprawa z linii uwagi",
                  "Brak otwartych spraw."})
    SPRAWDZ(szer(F_SB44, t) <= 744 && wszystkieZnakiWKroju(t));
  for (int n : {1, 3, 12}) SPRAWDZ(szerStopki(stopkaSpraw(2, n)) <= 744);
  // pytania tak/nie
  DaneDnia d;
  d.dzis = NIEDZIELA;
  for (auto p : {sprawy::Pytanie::SENNOSC, sprawy::Pytanie::GORZEJ, sprawy::Pytanie::PRZERWA}) {
    DanePytaniaTN t = trescPytaniaTN(p, d, 13 * 60 + 5, 14);
    SPRAWDZ(szer(F_SB44, t.tytul) <= 744 && szer(F_P28B, t.opis) <= 744 && szer(F_P22B, t.stopka) <= 744);
    SPRAWDZ(szer(F_P22B, wersaliki(t.etykieta), 2) <= 744);
    SPRAWDZ(wszystkieZnakiWKroju(t.tytul + t.opis + t.stopka + t.etykieta + t.nadPolami));
  }
  ROWNE(trescPytaniaTN(sprawy::Pytanie::GORZEJ, d, 13 * 60 + 5, 0).stopka, "Bez odpowiedzi do 13:05 = brak, nie „nie”.");
  // potwierdzenie „Omówione”: zdania ≤ 2 linie, pola mieszczą słowa
  for (bool a : {true, false})
    for (auto& z : zdaniaOmowione(a)) SPRAWDZ(zawin(F_P22B, z, 744).size() <= 2 && wszystkieZnakiWKroju(z));
  SPRAWDZ(szer(F_P28B, "▶ tak, omówione") <= 340);
  // „Przeniesione” i „Zapisano”
  for (auto& x : c.s) {
    if (x.ton == sprawy::Ton::BRAK) continue;
    DanePotwierdzenia pp = trescPrzeniesione(x, d, true);
    SPRAWDZ(szer(F_SB44, pp.tytul) <= 744 && zawin(F_P28B, pp.powod, 744).size() <= 2);
  }
  for (auto q : {sprawy::Pytanie::SENNOSC, sprawy::Pytanie::GORZEJ, sprawy::Pytanie::PRZERWA}) {
    DanePotwierdzenia z = trescZapisano(q, true, d, true);
    SPRAWDZ(szer(F_P28B, z.nad) <= 744);
  }
  // podpisy osi wizualizacji
  for (auto& x : c.s) SPRAWDZ(szer(F_P22B, x.podpisLewo) + 24 + szer(F_P22B, x.podpisPrawo) <= 744);
}

// ---------- rysowanie: negatyw ekranu L, linia A na ekranie dnia ----------
struct PlotnoT : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)(y * 800 + x)] = c;
  }
  bool cz(int x, int y) const { return px[(size_t)(y * 800 + x)] != 0; }
};
static void testRysowanie() {
  Scena c;
  c.wynik("L-06", "b", true, true, DZIS);
  c.licz();
  DaneDnia d;
  d.dzis = DZIS;
  DaneWzorca w = trescWzorca(c.s[0], d);
  SPRAWDZ(w.lekarz && w.pilnoscA);
  ROWNE(w.etykieta, "Skontaktuj się w ciągu kilku dni");
  // wizualizacja: L-03 linia AHI z 14 nocy i próg z katalogu, L-08 dwa rzędy po 30 nocy
  std::vector<Noc> h;
  for (int i = 70; i >= 0; i--) {
    Noc n;
    n.data = dodajDni(DZIS, -i);
    n.m01 = i > 30 ? 400 : (i % 3 ? 120 : 300);
    n.wazna = n.m01 >= 240;
    n.ahi = 11.0f + (float)(i % 4);
    h.push_back(n);
  }
  d.hist = &h;
  Scena v;
  v.wynik("L-03", "10", true, true, DZIS);
  v.wynik("L-08", "szybki", true, true, DZIS);
  v.licz();
  DaneWzorca w3 = trescWzorca(*v.sprawa("L-03/10"), d);
  SPRAWDZ(w3.wiz == WizWzorca::LINIA && w3.wartosci.size() == 14 && w3.prog == 10.0f);
  ROWNE(w3.podpisPrawo, "linia przerywana = 10 / h");
  DaneWzorca w8 = trescWzorca(*v.sprawa("L-08/szybki"), d);
  SPRAWDZ(w8.wiz == WizWzorca::RZEDY && w8.rzad1[0] && w8.rzad2[29]);
  int pel1 = 0, pel2 = 0;
  for (int i = 0; i < 30; i++) { pel1 += w8.rzad1[i]; pel2 += w8.rzad2[i]; }
  SPRAWDZ(pel1 == 30 && pel2 == 10);
  d.hist = nullptr;
  PlotnoT p;
  rysujWzorzec(p, w);
  SPRAWDZ(p.cz(2, 2) && !p.cz(6, 6) && p.cz(20, 300));  // czarne tło, biała ramka 6 px od 4 px, czarne wnętrze
  // linia wzorca A na ekranie dnia: czarny pasek 60 px (54…113)
  d.linia = "Wciąż: Saturacja: sprawa dla lekarza.";
  d.liniaA = d.liniaOstrzezenie = true;
  TrescEkranu t = trescDnia("S5", "", d);
  SPRAWDZ(t.liniaA && t.liniaOstrzezenie);
  PlotnoT q;
  rysujEkranDanych(q, {"cz 05.11", ""}, t);
  SPRAWDZ(q.cz(400, 56) && q.cz(400, 112) && !q.cz(400, 116));
  // karta w tonie „problem” (R-HUM-24): czarny pasek nagłówka, etykieta w czarnym polu, tło jasne (ADR-0009)
  TrescEkranu kp;
  kp.etykieta = "MASKA · TRZECIA NOC";
  kp.tytul = "Maska przecieka.";
  kp.tonProblem = true;
  PlotnoT kpp;
  rysujKarte(kpp, {"pn 09.11", "dane 05:44"}, kp, "", "");
  SPRAWDZ(kpp.cz(400, 5) && kpp.cz(400, 44) && kpp.cz(27, 62) && !kpp.cz(400, 300) && !kpp.cz(790, 300));
  kp.tonProblem = false;
  PlotnoT kz;
  rysujKarte(kz, {"pn 09.11", "dane 05:44"}, kp, "", "");
  SPRAWDZ(!kz.cz(400, 5) && !kz.cz(27, 62));
}

// ---------- dyrygent z atrapami: wyniki ze źródła zastępczego zamiast silnika reguł ----------
static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static std::string raportNocy(long day, const char* ahi = "1.4") {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":" + std::string(ahi) + ",\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static reguly::Zestaw ZESTAW_D = atrapa_b5::zestaw();
static std::vector<std::string> KLUCZE_D;  // wyniki spełnione, które zwraca źródło zastępcze
static bool WYKLUCZONE_PODANE = false;
static std::vector<Wynik> wynikiD(const reguly::Kontekst& kx) {
  WYKLUCZONE_PODANE = kx.wykluczoneNoce != nullptr;  // ten sam zbiór co liczNorme()
  std::vector<Wynik> w;
  for (auto& k : ZESTAW_D.katalog) {
    std::string kl = k.wariant.empty() ? k.id : k.id + "/" + k.wariant;
    bool sp = std::find(KLUCZE_D.begin(), KLUCZE_D.end(), kl) != KLUCZE_D.end();
    w.push_back(atrapa_b5::wynik(ZESTAW_D, k.id, k.wariant, k.stan != StanReguly::ZABLOKOWANA, sp,
                                 {{"x", "12,4"}, {"n", "3"}, {"min", "42"}, {"m", "27"}}));
  }
  return w;
}
static void dzien(Panel& p, int r, int mi, int d, int h, int m, PowodWybudzenia w = PowodWybudzenia::ZEGAR) {
  p.zegar.epoch = ep(r, mi, d, h, m);
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = w == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = w;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  long doba = nrDnia(dataZNr(nrDnia(std::to_string(r) + "-" + (mi < 10 ? "0" : "") + std::to_string(mi) + "-" +
                                    (d < 10 ? "0" : "") + std::to_string(d)))) - 1;
  p.siec.dodaj("/api/status", 200, STATUS);
  p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[{\"day\":" + std::to_string(doba) +
                                         ",\"session_count\":1,\"stale\":false}]}");
  p.siec.dodaj(sciezkaNocy(doba), 200, raportNocy(doba));
}
static bool wLogu(const Panel& p, const std::string& f) {
  for (auto& l : p.port.log)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static void testDyrygent() {
  sprawy::zrodloTestowe.wyniki = wynikiD;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_D;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_D);
  {  // wzorzec B w dniu wejścia: ekran dnia = ekran L, log z kontekstem silnika, bez buzzera
    KLUCZE_D = {"L-11"};
    Panel p;
    dzien(p, 2026, 11, 4, 7, 0);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(wLogu(p, "silnik reguł 2026-11-04"));
    SPRAWDZ(WYKLUCZONE_PODANE);
    SPRAWDZ(wLogu(p, "spełnione 1"));
    SPRAWDZ(wLogu(p, "ekran dnia: wzorzec L-11 (lekarz)"));
    SPRAWDZ(p.dzwiek.piskow == 0);
  }
  {  // wzorzec A wchodzi o 07:00 w ciszy nocnej: sygnał odłożony; o 08:30 trzy sygnały, raz
    KLUCZE_D = {"L-06/b"};
    Panel p;
    dzien(p, 2026, 11, 4, 7, 0);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(wLogu(p, "odłożony do końca ciszy nocnej"));
    SPRAWDZ(p.dzwiek.piskow == 0);
    p.port.log.clear();
    dzien(p, 2026, 11, 4, 8, 30);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.dzwiek.piskow == 3);
    dzien(p, 2026, 11, 4, 9, 0);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.dzwiek.piskow == 3);  // R-BUZ-04
  }
  {  // „tylko raport”: bez ekranu L, bez buzzera, ekran dnia jak zwykle
    KLUCZE_D = {"L-02/a", "L-04"};
    Panel p;
    dzien(p, 2026, 11, 4, 9, 0);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(wLogu(p, "spełnione 2") && !wLogu(p, "ekran dnia: wzorzec") && p.dzwiek.piskow == 0);
  }
  {  // niedziela 17:05: pytanie o senność jako ekran dnia; ▶ → „tak” w /dane/odpowiedzi.csv i ekran „Zapisano”
    KLUCZE_D = {};
    Panel p;
    dzien(p, 2026, 11, 8, 17, 5, PowodWybudzenia::START);  // zegarem o 17:05 panel tylko mierzy (plan wybudzeń)
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(wLogu(p, "ekran dnia: pytanie sennosc"));
    if (getenv("KOLEJKA_LOG")) for (auto& l : p.port.log) printf("    %s\n", l.c_str());
    p.port.log.clear();
    p.zegar.epoch = ep(2026, 11, 8, 17, 6);
    p.zegar.msTeraz = 0;
    p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
    p.start.budzace = BIT_PRAWY;
    p.przyciski.skrypt = {{0, BIT_PRAWY}, {300, 0}};
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.pliki.pliki["/dane/odpowiedzi.csv"].find("2026-11-08;sennosc;tak") != std::string::npos);
    SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::ZAPISANO);
  }
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

// ---------- sygnały komponentów dla zdarzeń E ----------
static reguly::Sygnaly SYG;
static int SYG_N = 0;
static void lapSygnaly(const reguly::Sygnaly& s) {
  SYG = s;
  SYG_N++;
}
static void testSygnaly() {
  sprawy::zrodloTestowe.wyniki = wynikiD;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_D;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_D);
  sprawy::zrodloTestowe.sygnaly = lapSygnaly;
  KLUCZE_D = {};
  {  // noc_bez_rekordu: o 09:00 jeszcze nie (fałsz, nie „brak sygnału”); o 11:30 bez sieci też nie: to E-01, a nie
     // E-10 (5.1.4, R-DANE-20, przypadek z odpowiadającym AirBridge w test/testy_stany_nocy_sloty_wieczorne.cpp)
    Panel p;
    dzien(p, 2026, 11, 4, 9, 0);
    p.siec.siecJest = false;
    SYG_N = 0;
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(SYG_N > 0 && SYG.stan.count("noc_bez_rekordu") && !SYG.stan["noc_bez_rekordu"]);
    SPRAWDZ(SYG.stan.count("po_11") && !SYG.stan["po_11"]);
    Panel q;
    dzien(q, 2026, 11, 4, 11, 30);
    q.siec.siecJest = false;
    SYG_N = 0;
    Dyrygent(q.adaptery(), KONF).wybudzenie();
    SPRAWDZ(SYG_N > 0 && !SYG.stan["noc_bez_rekordu"] && SYG.stan["po_11"]);
    // z nocą z dziś w historii: fałsz także po 11:00
    Panel r;
    dzien(r, 2026, 11, 4, 11, 30);
    SYG_N = 0;
    Dyrygent(r.adaptery(), KONF).wybudzenie();
    SPRAWDZ(SYG_N > 0 && !SYG.stan["noc_bez_rekordu"]);
  }
  {  // czujnik_sypialni_milczy: odczyt czujnika panelu NAN
    Panel p;
    dzien(p, 2026, 11, 4, 9, 0);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(SYG.stan.count("czujnik_sypialni_milczy") && !SYG.stan["czujnik_sypialni_milczy"]);
    Panel q;
    dzien(q, 2026, 11, 4, 9, 0);
    q.czujniki.o.temp = NAN;
    q.czujniki.o.wilg = NAN;
    Dyrygent(q.adaptery(), KONF).wybudzenie();
    SPRAWDZ(SYG.stan["czujnik_sypialni_milczy"]);
  }
  {  // co2_milczy: bez źródła otoczenia fałsz; ze źródłem i świeżym ostrzeżeniem tak, z {czas}
    Panel p;
    dzien(p, 2026, 11, 4, 9, 0);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(SYG.stan.count("co2_milczy") && !SYG.stan["co2_milczy"] && !SYG.zmienne.count("czas"));
    KonfiguracjaPanelu k = KONF;
    k.otoUrl = "http://192.168.1.60:8080/otoczenie";
    Panel q;
    dzien(q, 2026, 11, 4, 9, 0);
    q.siec.dodaj("/otoczenie", 200,
                 R"({"wersja":1,"ts":"2026-11-04T08:55:00+01:00","ostrzezenia":[{"kod":"co2_milczy",)"
                 R"("od":"2026-11-04T07:00:00+01:00"}]})");
    Dyrygent(q.adaptery(), k).wybudzenie();
    SPRAWDZ(SYG.stan["co2_milczy"]);
    ROWNE(SYG.zmienne.count("czas") ? SYG.zmienne["czas"] : std::string("-"), "07:00");
  }
  {  // wilgotnosc_min_nocy: odczyty nocy w buforze, minimum po zamknięciu nocy o 06:00; M-25 z napięcia
    Panel p;
    for (int i = 0; i < 4; i++) {
      p.zegar.epoch = ep(2026, 11, 4, 1 + i, 0);
      p.zegar.msTeraz = 0;
      p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
      p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
      p.czujniki.o.wilg = 40.0f + (float)i * 2;  // 40, 42, 44, 46
      zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
      Dyrygent(p.adaptery(), KONF).wybudzenie();
    }
    p.czujniki.o.wilg = 45.0f;
    dzien(p, 2026, 11, 4, 7, 0);
    SYG = reguly::Sygnaly();
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    if (getenv("KOLEJKA_LOG")) for (auto& l : p.port.log) printf("    %s\n", l.c_str());
    SPRAWDZ(SYG.odczyty.count("wilgotnosc_min_nocy") && std::fabs(SYG.odczyty["wilgotnosc_min_nocy"] - 40.0) < 1e-6);
    // M-25 z krzywej napięcie → procent: 3,95 V z atrapy = 70%
    SPRAWDZ(SYG.odczyty.count("M-25") && std::fabs(SYG.odczyty["M-25"] - 70.0) < 1e-6);
  }
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

// ---------- S2a: trzecia kolumna według trybu bieżącego (4.11, ADR-0030; dodatek) ----------
static void testS2aTryb() {
  auto hist = [](const char* ust) {
    std::vector<Noc> h;
    for (int i = 40; i >= 0; i--) {
      Noc n;
      n.data = dodajDni(DZIS, -i);
      n.m01 = 420;
      n.wazna = true;
      n.ahi = 1.2f;
      n.wp95 = 8;
      n.cmed = 9.5f;
      n.emed = 6.5f;
      n.ustawienia = ust;
      h.push_back(n);
    }
    return h;
  };
  for (const char* u : {"CPAP/10", "AutoSet/6/12"}) {
    std::vector<Noc> h = hist(u);
    DaneDnia d;
    d.dzis = DZIS;
    d.hist = &h;
    TrescEkranu t = trescDnia("S2A", "3", d);
    SPRAWDZ(t.kolumny.size() == 4);
    if (t.kolumny.size() == 4) {
      ROWNE(t.kolumny[2].et, "ciśnienie");
      ROWNE(t.kolumny[2].w, "9,5");
    }
  }
  for (const char* u : {"ASVAuto/6/10/3/15", "ASV/6/3/15", ""}) {
    std::vector<Noc> h = hist(u);
    DaneDnia d;
    d.dzis = DZIS;
    d.hist = &h;
    TrescEkranu t = trescDnia("S2A", "3", d);
    if (t.kolumny.size() == 4) {
      ROWNE(t.kolumny[2].et, "EPAP");
      ROWNE(t.kolumny[2].w, "6,5");
    }
  }
  // zmiana trybu: ostatnie 3 noce w CPAP po ASVAuto → kolumna ciśnienia tylko z nocy CPAP
  std::vector<Noc> h = hist("ASVAuto/6/10/3/15");
  for (size_t i = h.size() - 3; i < h.size(); i++) {
    h[i].ustawienia = "CPAP/11";
    h[i].cmed = 11.0f;
  }
  DaneDnia d;
  d.dzis = DZIS;
  d.hist = &h;
  TrescEkranu t = trescDnia("S2A", "3", d);
  if (t.kolumny.size() == 4) {
    ROWNE(t.kolumny[2].et, "ciśnienie");
    ROWNE(t.kolumny[2].w, "11,0");
  }
}

// ---------- prawdziwy silnik z rodzajami E: E-30 rano (pełny ekran S3) ----------
static void testE30Linia() {
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();  // bez atrapy: zestaw wbudowany i reguly::ocen()
  Panel p;
  long ost = nrDnia("2026-11-04") - 1;  // doba ostatniej nocy (data poranka 2026-11-04)
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = ost - 8; x <= ost; x++)
    l += std::string(x > ost - 8 ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  l += "]}";
  bool jest = false;
  for (int h = 0; h < 4 && !jest; h++) {  // ostatnia noc i zaległe doby partiami (ZALEGLE_NA_WYBUDZENIE)
    p.port.log.clear();
    p.siec.odpowiedzi.clear();
    p.zegar.epoch = ep(2026, 11, 4, 7, 0) + (uint32_t)(h * 1800);
    p.zegar.msTeraz = 0;
    // start zasilania: pełne wybudzenie z rysowaniem i zaległymi dobami (zegarem o pół godziny panel tylko mierzy)
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.siec.dodaj("/api/status", 200, STATUS);
    p.siec.dodaj(sciezkaListy(), 200, l);
    for (long x = ost - 8; x <= ost; x++) p.siec.dodaj(sciezkaNocy(x), 200, raportNocy(x, "6.5"));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    // w dniu wejścia E-30 jest pełnym ekranem S3 (6.4, R-KON-14), linia `Wciąż: …` dopiero po „wiem” albo limicie
    jest = wLogu(p, "ekran dnia: zdarzenie E-30 (S3)");
  }
  if (!jest || getenv("KOLEJKA_LOG"))
    for (auto& x : p.port.log)
      if (x.find("silnik") != std::string::npos || x.find("linia") != std::string::npos ||
          x.find("E-30") != std::string::npos || x.find("historia") != std::string::npos)
        printf("    %s\n", x.c_str());
  SPRAWDZ(jest);
}

// ---------- stary układ partycji (R7 karty T-WYM-84): bez zapisu pamięci reguł i odpowiedzi ----------
static void testStaryUklad() {
  sprawy::zrodloTestowe.wyniki = wynikiD;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_D;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_D);
  KLUCZE_D = {"L-11"};  // spełniona: aktualizujPamiec() zmienia pamięć (aktywna_od), więc zapis byłby
  Panel p;
  dzien(p, 2026, 11, 4, 9, 0, PowodWybudzenia::START);
  p.pliki.opis.uklad = ukladWkompilowany() + 1;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  int nowych = 0;
  for (auto& kv : p.pliki.pliki) nowych += kv.first.compare(0, 6, "/dane/") == 0;
  ROWNE_L(nowych, 0);
  SPRAWDZ(wLogu(p, "silnik reguł 2026-11-04"));  // ocena działa dalej, stan w RAM
  // ten sam scenariusz w układzie wkompilowanym: pamięć reguł zapisana
  Panel q;
  dzien(q, 2026, 11, 4, 9, 0);
  Dyrygent(q.adaptery(), KONF).wybudzenie();
  SPRAWDZ(q.pliki.pliki.count("/dane/reguly.csv") == 1);
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // strefa panelu (Europe/Warsaw), jak test/testy_plan_wybudzen.cpp
  tzset();
  testSprawyOtwarte();
  testEkranDnia();
  testKolejka();
  testTylkoRaport();
  testWiemOmowione();
  testKartySpraw();
  testUspokojenie();
  testBuzzer();
  testPytania();
  testPrzyciski();
  testSzerokosci();
  testRysowanie();
  testDyrygent();
  testSygnaly();
  testS2aTryb();
  testE30Linia();
  testStaryUklad();
  printf("testy_kolejka_linii: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
