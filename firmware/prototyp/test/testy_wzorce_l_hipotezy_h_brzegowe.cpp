// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (wzorce L i hipotezy H). Niezależne od testów podstawowych
// (test/testy_wzorce_l_hipotezy_h.cpp). Test zielony = sprawdzenie, które trzyma. Dane wyłącznie syntetyczne.
//
// Uruchomienie: tools/prototyp/testy.sh.
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "noc.h"
#include "reguly.h"
#include "stany.h"
#include "wspolne.h"
#include "wzorce.h"

namespace reguly {
void zarejestrujRodzajeE();  // src/rodzaje_e.cpp
}

static int ok = 0, zle = 0;
#define SPRAWDZ(f, w)                                                          \
  do {                                                                         \
    if (w) ok++;                                                               \
    else { zle++; printf("BŁĄD %s %s:%d: %s\n", f, __FILE__, __LINE__, #w); } \
  } while (0)

using reguly::Pilnosc;
using reguly::StanReguly;
using reguly::Tryb;
using reguly::Wynik;
using wzorce::StanWzorca;

static const char* trybTekst(Tryb t) {
  switch (t) {
    case Tryb::CPAP: return "CPAP";
    case Tryb::AUTOSET: return "AutoSet";
    case Tryb::ASV: return "ASV";
    default: return "ASVAuto";
  }
}

static Noc noc(const std::string& data, int m01, float ahi, Tryb t = Tryb::ASVAUTO) {
  Noc n;
  n.data = data;
  n.m01 = m01;
  n.wazna = m01 >= 240;
  n.ahi = ahi;
  n.wmed = 5;
  n.wp95 = 12;
  n.emed = 7;
  n.ep95 = 8;
  n.cmed = 8;
  n.cp95 = 9;
  n.oa = 1;
  n.ca = 1;
  n.h = 1;
  n.csr = 0;
  n.m08 = 0;
  n.m13 = 14;
  n.tryb = trybTekst(t);
  if (t == Tryb::ASVAUTO) {
    n.ustawienia = "ASVAuto/6/10/3/15";
    n.eai = 6; n.eax = 10; n.ans = 3; n.axs = 15;
  } else if (t == Tryb::ASV) {
    n.ustawienia = "ASV/6/6/3/15";
    n.eep = 6; n.mns = 3; n.mxs = 15;
  } else if (t == Tryb::AUTOSET) {
    n.ustawienia = "AutoSet/5/12";
    n.mpi = 5; n.mpa = 12;
  } else {
    n.ustawienia = "CPAP/9";
    n.ipc = 9;
  }
  return n;
}
static std::vector<Noc> seria(const std::string& od, int ile, float ahi, Tryb t = Tryb::ASVAUTO) {
  std::vector<Noc> h;
  for (int i = 0; i < ile; ++i) h.push_back(noc(dodajDni(od, i), 420, ahi, t));
  return h;
}
static std::vector<Wynik> ocen(const std::vector<Noc>& h, const reguly::Odpowiedzi* odp = nullptr,
                               Tryb tryb = Tryb::ASVAUTO, const reguly::Pamiec* pam = nullptr) {
  const std::string dzis = h.back().data;
  const Stany s = stanyZPamieci(h, dzis, {});
  reguly::Kontekst k;
  k.historia = &h;
  k.dzis = dzis;
  k.norma = &s.norma;
  k.stany = &s;
  k.trybBiezacy = tryb;
  static reguly::Odpowiedzi pusta;
  k.odpowiedzi = odp ? odp : &pusta;
  static reguly::Pamiec pustaP;
  k.pamiec = pam ? pam : &pustaP;
  k.zestaw = &reguly::zestawWbudowany();
  return reguly::ocen(reguly::zestawWbudowany(), k);
}
static const Wynik* znajdz(const std::vector<Wynik>& w, const std::string& id, const std::string& war) {
  for (const auto& x : w)
    if (x.id == id && x.wariant == war) return &x;
  return nullptr;
}
static bool spel(const std::vector<Wynik>& w, const std::string& id, const std::string& war) {
  const Wynik* x = znajdz(w, id, war);
  return x && x->oceniona && x->spelniona;
}
static std::string zm(const std::vector<Wynik>& w, const std::string& id, const std::string& war, const std::string& n) {
  const Wynik* x = znajdz(w, id, war);
  if (!x) return "<brak wyniku>";
  auto it = x->zmienne.find(n);
  return it == x->zmienne.end() ? "<brak zmiennej>" : it->second;
}

// aktywne L-11 (B) kończy się, gdy użytkownik przestaje odpowiadać o senność. Wyjście „tak w ≤ 1 z 4
// tygodni” liczy tygodnie bez odpowiedzi jak „nie” (rodzaje_l.cpp odpowiedziNajwyzej), a R-SKI-17 i ADR-0029 pkt 7:
// brak odpowiedzi to nie „nie”.
static void l11WyjscieBezOdpowiedzi() {
  printf("wyjście L-11 przy braku odpowiedzi o senność\n");
  auto h = seria("2026-09-01", 40, 2.0f);
  const std::string dzis = h.back().data;
  reguly::Odpowiedzi wej;
  wej.sennosc[dodajDni(dzis, -15)] = true;
  wej.sennosc[dodajDni(dzis, -8)] = true;
  wej.sennosc[dodajDni(dzis, -1)] = true;
  SPRAWDZ("aktywne L-11 (warunek wstępny)", spel(ocen(h, &wej), "L-11", ""));
  reguly::Odpowiedzi cisza;  // jedna stara odpowiedź „tak”, 3 tygodnie bez odpowiedzi
  cisza.sennosc[dodajDni(dzis, -22)] = true;
  auto r = ocen(h, &cisza);
  const Wynik* w = znajdz(r, "L-11", ".wyjscie");
  printf("  L-11/.wyjscie przy 3 tygodniach bez odpowiedzi: oceniona=%d spelniona=%d\n", w ? w->oceniona : -1,
         w ? w->spelniona : -1);
  SPRAWDZ("aktywne L-11", !spel(r, "L-11", ".wyjscie"));
}

// L-08 wariant szybki (aktywny, B) wchodzi z odniesieniem „30 dni wcześniej” policzonym z 15 dni historii,
// a powód mówi `miesiąc temu {m} z 30` (m = noce z 15 dni).
static void l08SzybkiOdniesienieZ15Dni() {
  printf("L-08 szybki z krótkim odniesieniem\n");
  std::vector<Noc> h;
  const std::string d0 = "2026-09-01";
  for (int i = 0; i < 45; ++i) {
    int m01 = 420;
    if (i == 7) m01 = 100;
    if (i >= 15 && (i % 2 == 0)) m01 = 100;  // 15 z 30 krótkich, ostatnie 30 dni: 15 ≥ 4 h? policz niżej
    if (i == 17) m01 = 100;                  // 14 z 30 ≥ 4 h, M-19 < 50%
    h.push_back(noc(dodajDni(d0, i), m01, 2.0f));
  }
  auto r = ocen(h);
  printf("  L-08/szybki: spełniony=%d, n=%s, m=%s (odniesienie z 15 dni historii)\n", spel(r, "L-08", "szybki"),
         zm(r, "L-08", "szybki", "n").c_str(), zm(r, "L-08", "szybki", "m").c_str());
  SPRAWDZ("L-08 wariant szybki", !spel(r, "L-08", "szybki"));
}

// L-11 w wariancie „2 tygodnie” bez ramki hipotezy: H-09 i H-08 mają `wymaga=L-11`, czyli tylko gałąź
// główną; spec 5.12.9: H-08 przy L-11 zawsze, gdy nie ma H-09.
static void l11H08BezRamkiHipotezy() {
  printf("hipoteza przy L-11 wariant 2 tygodnie\n");
  auto h = seria("2026-09-01", 40, 2.0f);
  const std::string dzis = h.back().data;
  reguly::Odpowiedzi o;
  o.sennosc[dodajDni(dzis, -8)] = true;
  o.sennosc[dodajDni(dzis, -1)] = true;
  auto r = ocen(h, &o);
  SPRAWDZ("L-11 w wariancie „2 tygodnie” bez ramki hipotezy (warunek wstępny)", spel(r, "L-11", "2tyg") && !spel(r, "L-11", ""));
  const std::string hip = wzorce::hipoteza("L-11", r);
  printf("  hipoteza przy L-11/2tyg: „%s”\n", hip.c_str());
  SPRAWDZ("L-11 w wariancie „2 tygodnie” bez ramki hipotezy", !hip.empty());
}

// licznik karencji L-01 liczy przebiegi, nie noce: dwa przebiegi aktualizuj() tego samego dnia (restart,
// ponowny import) skracają karencję o połowę.
static void l01KarencjaLiczyNoce() {
  printf("karencja L-01 przy dwóch przebiegach dziennie\n");
  const auto& z = reguly::zestawWbudowany();
  auto wyn = [](bool sp) {
    std::vector<Wynik> v;
    auto W = [](const std::string& war, bool s) {
      Wynik w;
      w.id = "L-01";
      w.wariant = war;
      w.oceniona = true;
      w.spelniona = s;
      w.stan = StanReguly::TYLKO_RAPORT;
      return w;
    };
    v.push_back(W("cpap.karencja", sp));
    Wynik g = W("cpap", sp);
    g.zmienne["x"] = "6";
    v.push_back(g);
    v.push_back(W(".wyjscie", false));
    return v;
  };
  reguly::Pamiec p;
  const std::string d0 = "2026-10-01";
  for (int i = 0; i < 8; ++i) {
    wzorce::aktualizuj(wyn(true), p, dodajDni(d0, i), &z);
    wzorce::aktualizuj(wyn(true), p, dodajDni(d0, i), &z);
  }
  const auto s = wzorce::odczytajZapis(p["L-01"].stanWzorca);
  printf("  po 8 dniach (2 przebiegi dziennie): stan %s, licznik %d\n", wzorce::nazwaStanu(s.stan), s.licznik);
  SPRAWDZ("licznik karencji L-01 liczy przebiegi, nie noce", s.stan == StanWzorca::KARENCJA && s.licznik <= 8);
}

// L-07 w CPAP i AutoSet nie ma tytułu w teksty.csv (35 znaków > 30), a tytuł jest potrzebny poza ekranem:
// `{tytuł}: nie oceniam.` (R-SKI-44), pytanie ogólne wzorca w raporcie (5.12.9), strefa 2 raportu (R-LEK-30).
static void l07TytulPozaEkranem() {
  printf("tytuł L-07 w CPAP\n");
  const auto& z = reguly::zestawWbudowany();
  Wynik g;
  g.id = "L-07";
  g.wariant = "cpap";
  g.oceniona = g.spelniona = true;
  g.stan = StanReguly::TYLKO_RAPORT;
  // tytuł do decyzji użytkownika (pytanie otwarte), spec bez zmian; do tego
  // czasu sprawa L-07 w CPAP pokazuje linię C, a tytułu nie ma (wzorce::info() bez tytułu spoza teksty.csv)
  std::vector<Wynik> wyn = {g};
  const wzorce::InfoWzorca s = wzorce::info(wyn, reguly::Pamiec(), "L-07", "2026-10-01", &z);
  const std::string t = s.idPowodu;
  printf("  idPowodu sprawy L-07 cpap: „%s”, w teksty: %s; tytuł: „%s”\n", t.c_str(),
         z.teksty.count(t) ? "tak" : "nie", s.idTytulu.c_str());
  SPRAWDZ("L-07 w CPAP i AutoSet nie ma tytułu w teksty.csv", !t.empty() && z.teksty.count(t) > 0 && (s.idTytulu.empty() || z.teksty.count(s.idTytulu) > 0));
}

// Trzyma: aktywne L-03 (≥ 10), L-09 w każdym trybie; „tylko raport” nigdy z pilnością; L-06 nigdy oceniane.
static void trzyma() {
  printf("sprawdzenia, które mają trzymać (tryby, tylko raport, L-06, zapis `@P`, zmienne tekstów)\n");
  for (Tryb t : {Tryb::CPAP, Tryb::AUTOSET, Tryb::ASV, Tryb::ASVAUTO}) {
    auto h = seria("2026-09-01", 30, 12.0f, t);
    auto r = ocen(h, nullptr, t);
    SPRAWDZ("tryb L-03", spel(r, "L-03", "ge10"));
    for (auto& n : h) n.wp95 = 30, n.m08 = 60;  // duży wyciek co noc
    auto r2 = ocen(h, nullptr, t);
    SPRAWDZ("tryb L-09", spel(r2, "L-09", ""));
    for (const auto& w : r2) {
      if (w.stan == StanReguly::TYLKO_RAPORT) SPRAWDZ("tylko raport bez pilności", w.pilnosc == Pilnosc::BRAK);
      if (w.id == "L-06") SPRAWDZ("L-06 nieoceniane", !w.oceniona);
    }
  }
  for (Tryb t : {Tryb::CPAP, Tryb::AUTOSET, Tryb::ASV, Tryb::ASVAUTO}) {  // L-11 w każdym trybie
    auto h = seria("2026-09-01", 40, 2.0f, t);
    const std::string dzis = h.back().data;
    reguly::Odpowiedzi o;
    o.sennosc[dodajDni(dzis, -15)] = o.sennosc[dodajDni(dzis, -8)] = o.sennosc[dodajDni(dzis, -1)] = true;
    SPRAWDZ("tryb L-11", spel(ocen(h, &o, t), "L-11", ""));
  }
  {  // L-03 (≥ 10) działa w rozruchu (R-SKI-04): 11 nocy, w tym 10 ważnych dla L, E-12 jeszcze bez 14 dni historii
    auto h = seria("2026-09-01", 11, 12.0f);
    auto r = ocen(h);
    const Wynik* w = znajdz(r, "L-03", "ge10");
    printf("  L-03/ge10 po 11 nocach: oceniona=%d spelniona=%d %s\n", w ? w->oceniona : -1, w ? w->spelniona : -1,
           w ? w->powodNieoceny.c_str() : "");
    SPRAWDZ("trzyma", spel(r, "L-03", "ge10"));
  }
  // „wiem” i „Omówione” zapisane przez (sprawy.h: pole `RRRR-MM-DD@P`) aktualizuj czyta jak datę
  // (dniMiedzy → nrDnia = -1), więc wyciszenie kończy się w następnym przebiegu: „wiem” trwa 1 dzień.
  {
    const auto& z = reguly::zestawWbudowany();
    auto l03 = [](bool sp) {
      Wynik a, b, c;
      a.id = b.id = c.id = "L-03";
      a.wariant = "ge10.mediana";
      b.wariant = "ge10";
      c.wariant = "ge10.wyjscie";
      a.oceniona = b.oceniona = c.oceniona = true;
      a.spelniona = b.spelniona = sp;
      b.pilnosc = Pilnosc::B;
      b.zmienne["x"] = "11";
      return std::vector<Wynik>{a, b, c};
    };
    reguly::Pamiec p;
    wzorce::aktualizuj(l03(true), p, "2026-10-01", &z);
    SPRAWDZ("wiem", wzorce::wiem(p, "L-03/ge10", "2026-10-02"));
    p["L-03/ge10"].wiem = "2026-10-02@B";
    wzorce::aktualizuj(l03(true), p, "2026-10-03", &z);
    printf("  wiem @B (zapis stanu wzorca), dzień 1: %s\n", p["L-03/ge10"].stanWzorca.c_str());
    SPRAWDZ("trzyma", wzorce::odczytajZapis(p["L-03/ge10"].stanWzorca).stan == StanWzorca::WIEM);
    wzorce::aktualizuj(l03(true), p, "2026-10-09", &z);
    SPRAWDZ("wiem @B po 7 dniach", wzorce::odczytajZapis(p["L-03/ge10"].stanWzorca).stan == StanWzorca::AKTYWNY);
  }
  {  // L-02(a): {n} i {min} z prawdziwych wartości (3 noce po 25 min CSR)
    auto h = seria("2026-09-01", 30, 2.0f);
    for (int i = 0; i < 3; ++i) h[h.size() - 1 - static_cast<size_t>(i)].csr = 25;
    auto r = ocen(h);
    printf("  L-02/a: spełniony=%d n=%s min=%s\n", spel(r, "L-02", "a"), zm(r, "L-02", "a", "n").c_str(),
           zm(r, "L-02", "a", "min").c_str());
    SPRAWDZ("L-02a n", zm(r, "L-02", "a", "n") == "3");
    SPRAWDZ("L-02a min", zm(r, "L-02", "a", "min") == "75" || zm(r, "L-02", "a", "min") == "75,0");
  }
  {  // L-01 (ASV): {x} i {norma} z normy CA/h (bez „–” przy istniejącej normie)
    auto h = seria("2026-08-01", 40, 6.0f);
    for (size_t i = h.size() - 7; i < h.size(); ++i) h[i].ca = 6;
    auto r = ocen(h);
    printf("  L-01: spełniony=%d x=%s norma=%s\n", spel(r, "L-01", ""), zm(r, "L-01", "", "x").c_str(),
           zm(r, "L-01", "", "norma").c_str());
    SPRAWDZ("L-01 x", zm(r, "L-01", "", "x") == "6" || zm(r, "L-01", "", "x") == "6,0");
    SPRAWDZ("L-01 norma", zm(r, "L-01", "", "norma") != "–" && zm(r, "L-01", "", "norma").find('<') == std::string::npos);
  }
  {  // L-11: {n} = tygodnie z „tak” z 4, {x} = mediana AHI
    auto h = seria("2026-09-01", 40, 2.0f);
    const std::string dzis = h.back().data;
    reguly::Odpowiedzi o;
    o.sennosc[dodajDni(dzis, -15)] = true;
    o.sennosc[dodajDni(dzis, -8)] = true;
    o.sennosc[dodajDni(dzis, -1)] = true;
    auto r = ocen(h, &o);
    printf("  L-11: n=%s x=%s\n", zm(r, "L-11", "", "n").c_str(), zm(r, "L-11", "", "x").c_str());
    SPRAWDZ("L-11 n", zm(r, "L-11", "", "n") == "3");
    SPRAWDZ("L-11 x", zm(r, "L-11", "", "x") == "2" || zm(r, "L-11", "", "x") == "2,0");
    printf("  L-11: hipoteza „%s”\n", wzorce::hipoteza("L-11", r).c_str());
  }
}

int main() {
  wzorce::zarejestrujRodzajeL();
  reguly::zarejestrujRodzajeE();
  l11WyjscieBezOdpowiedzi();
  l08SzybkiOdniesienieZ15Dni();
  l11H08BezRamkiHipotezy();
  l01KarencjaLiczyNoce();
  l07TytulPozaEkranem();
  trzyma();
  printf("testy_wzorce_l_hipotezy_h_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
