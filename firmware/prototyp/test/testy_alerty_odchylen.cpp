// SPDX-License-Identifier: GPL-3.0-or-later
// Testy „alerty odchyleń od normy” (ADR-0039, specyfikacja funkcjonalna 5.8.13): karty S-K-40…S-K-47
// (rodzaj `sk_odchylenie`, sk::odchylenia w src/kontekst_sk.cpp), stany w kierunkach kart (src/stany.cpp), linia „dobra
// zmiana” (sk::liniaDobrejZmiany, kolejka 7.5 grupa „norma”). Wymagania: R-KON-64…R-KON-68. Dane wyłącznie syntetyczne.
#include <cmath>
#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "ekrany.h"
#include "humor.h"
#include "infekcja.h"
#include "kolejka.h"
#include "kontekst_sk.h"
#include "sprawy.h"
#include "stany.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                                   \
  do {                                                                               \
    if (w) ok++;                                                                     \
    else { zle++; printf("BŁĄD (%s:%d): %s\n", __FILE__, __LINE__, #w); }            \
  } while (0)

static const std::string D0 = "2026-08-01";
// liczba znaków UTF-8 (limity 5.4.4 liczą znaki, nie bajty)
static int dlugoscZnakow(const std::string& t) {
  int n = 0;
  for (unsigned char c : t) n += (c & 0xC0) != 0x80;
  return n;
}
static std::string d(int i) { return dodajDni(D0, i); }

// noc ASVAuto z wartościami bazowymi z lekkim rozrzutem (P10 < mediana < P90), żeby granice normy były ostre
static Noc noc(int i) {
  Noc n;
  n.data = d(i);
  n.wazna = true;
  n.m01 = 420;
  n.ahi = 3.0f + 0.5f * (float)(i % 5);      // mediana 4, P10 3, P90 5
  n.wmed = 4;
  n.wp95 = 8.0f + (float)(i % 5);            // mediana 10, P90 12
  n.cp95 = 10;
  n.emed = 7;
  n.ep95 = 8.0f + 0.1f * (float)(i % 5);
  n.psmed = 6.0f + 0.1f * (float)(i % 5);    // mediana 6,2, P10 6,0, P90 6,4
  n.psp95 = 9;
  n.m13 = 14.0f + 0.2f * (float)(i % 5);     // mediana 14,4, P90 14,8
  n.csr = 4.0f + (float)(i % 5);             // CSR% 0,95…1,9 (mediana 6 min = 1,43%)
  n.m10p = 0.05f + 0.01f * (float)(i % 5);   // mediana 0,07, P90 0,09
  n.m15 = 7.0f + 0.1f * (float)(i % 5);      // mediana 7,2, P10 7,0, P90 7,4
  n.m14 = 460.0f + 5.0f * (float)(i % 5);    // mediana 470, P10 460, P90 480
  n.m17 = 0;
  n.m08 = 0;
  n.ustawienia = "ASVAuto/6/10/3/15";
  n.tryb = "ASVAuto";
  return n;
}
static std::vector<Noc> historia(int ile) {
  std::vector<Noc> h;
  for (int i = 0; i < ile; i++) h.push_back(noc(i));
  return h;
}

// Jedna karta z tabeli i jej odchylenie (wartość nocy z odchyleniem i wartość „w normie” do wyjścia ze stanu)
struct Karta {
  const char* id;
  const char* typ;
  std::function<void(Noc&, float)> ustaw;
  float odch, wNormie;
};
static std::vector<Karta> karty() {
  return {
      {"S-K-40", "SK40", [](Noc& n, float v) { n.psmed = v; }, 8.0f, 6.2f},
      {"S-K-41", "SK41", [](Noc& n, float v) { n.m15 = v; }, 9.0f, 7.2f},
      {"S-K-42", "SK42", [](Noc& n, float v) { n.m15 = v; }, 5.0f, 7.2f},
      {"S-K-43", "SK43", [](Noc& n, float v) { n.m14 = v; }, 380.0f, 470.0f},
      {"S-K-44", "SK44", [](Noc& n, float v) { n.m14 = v; }, 560.0f, 470.0f},
      {"S-K-45", "SK45", [](Noc& n, float v) { n.m13 = v; }, 17.0f, 14.4f},
      {"S-K-46", "SK46", [](Noc& n, float v) { n.csr = v; }, 30.0f, 6.0f},
      {"S-K-47", "SK47", [](Noc& n, float v) { n.m10p = v; }, 0.25f, 0.07f},
  };
}

struct Rano {
  std::vector<Noc> h;
  Stany st;
  std::vector<WpisHumoru> dz;
  sk::Wejscie w;
  void ocen(const std::string& dzis, reguly::Tryb tryb = reguly::Tryb::ASVAUTO) {
    st = liczStany(h, dzis, {});
    w = sk::Wejscie();
    w.dzis = dzis;
    w.hist = &h;
    w.norma = &st.norma;
    w.stany = &st;
    w.tryb = tryb;
    w.trybProbny = false;
    w.dziennik = &dz;
  }
  std::vector<sk::Tresc> v() const { return sk::rano(w); }
};
// kopia kandydata (wektor z sk::rano() bywa tymczasowy): wskaźnik ważny do końca programu
static const sk::Tresc* znajdz(const std::vector<sk::Tresc>& v, const std::string& id) {
  static std::deque<sk::Tresc> kopie;
  for (auto& t : v)
    if (t.id == id) {
      kopie.push_back(t);
      return &kopie.back();
    }
  return nullptr;
}
static int ileOdchylen(const std::vector<sk::Tresc>& v) {
  int k = 0;
  for (auto& t : v) k += t.grupa == "R5e";
  return k;
}
static int indeks(const std::vector<sk::Tresc>& v, const std::string& id) {
  for (size_t i = 0; i < v.size(); i++)
    if (v[i].id == id) return (int)i;
  return -1;
}
static WpisHumoru wpis(const std::string& data, const std::string& typ, const std::string& klucz) {
  WpisHumoru x;
  x.data = data;
  x.typ = typ;
  x.klucz = klucz;
  x.tryb = 'D';
  return x;
}

// wymaganie: R-KON-64
// 3 z 5 nocy poza granicą → karta; 2 z 5 → brak; teksty dosłownie z katalogu (teksty.csv)
static void testR_KON_64_trzy_z_pieciu() {
  for (const Karta& k : karty()) {
    Rano r;
    r.h = historia(35);
    for (int i : {30, 32, 34}) k.ustaw(r.h[(size_t)i], k.odch);
    r.ocen(d(34));
    const sk::Tresc* t = znajdz(r.v(), k.id);
    SPRAWDZ(t != nullptr);
    if (t) {
      SPRAWDZ(t->grupa == "R5e" && !t->problem && !t->karta && t->typ == k.typ);
      SPRAWDZ(t->t.etykieta == "Ostatnie noce");
      SPRAWDZ(t->t.krok == "→ Obserwuj kolejne noce.");
      SPRAWDZ(t->wiz == sk::Wiz::LINIA && t->linia.size() == 14 && !std::isnan(t->prog));
    }
    Rano r2;
    r2.h = historia(35);
    for (int i : {31, 33}) k.ustaw(r2.h[(size_t)i], k.odch);
    r2.ocen(d(34));
    SPRAWDZ(znajdz(r2.v(), k.id) == nullptr);
  }
}

// wymaganie: R-KON-64
// teksty i liczby: {x} = mediana 5 ostatnich nocy ważnych, {norma} = mediana normy (teksty.csv)
static void testR_KON_64_teksty() {
  Rano r;
  r.h = historia(35);
  for (int i : {30, 31, 32, 33, 34}) r.h[(size_t)i].psmed = 8.0f;
  r.ocen(d(34));
  const sk::Tresc* t = znajdz(r.v(), "S-K-40");
  SPRAWDZ(t && t->t.tytul == "PS wyżej niż zwykle");
  SPRAWDZ(t && t->t.powod == "PS 8,0 cmH2O, zwykle 6,2. Bywa przy zatkanym nosie lub na plecach.");
  SPRAWDZ(t && t->podpisLewo == "PS · 14 nocy" && t->podpisPrawo == "linia przerywana = zwykle 6,2");
  Rano r2;
  r2.h = historia(35);
  for (int i : {30, 31, 32, 33, 34}) r2.h[(size_t)i].m10p = 0.25f;
  r2.ocen(d(34));
  t = znajdz(r2.v(), "S-K-47");
  SPRAWDZ(t && t->t.tytul == "Wdechy bardziej spłaszczone");
  SPRAWDZ(t && t->t.powod == "Ograniczenie przepływu 0,25, zwykle 0,07. Bywa przy zatkanym nosie.");
  Rano r3;
  r3.h = historia(35);
  for (int i : {30, 31, 32, 33, 34}) r3.h[(size_t)i].m13 = 17.2f;
  r3.ocen(d(34));
  t = znajdz(r3.v(), "S-K-45");
  SPRAWDZ(t && t->t.tytul == "Oddech szybszy niż zwykle");
  SPRAWDZ(t && t->t.powod == "Oddechy 17,2 / min, zwykle 14,4. Bywa przy katarze, cieple, stresie.");
  Rano r5;
  r5.h = historia(35);
  for (int i : {30, 31, 32, 33, 34}) r5.h[(size_t)i].m14 = 380.0f;
  r5.ocen(d(34));
  t = znajdz(r5.v(), "S-K-43");
  SPRAWDZ(t && t->t.tytul == "Płytszy oddech niż zwykle");
  SPRAWDZ(t && t->t.powod == "Objętość oddechu 380 ml, zwykle 470. Bywa w innej pozycji snu.");
  Rano r6;
  r6.h = historia(35);
  for (int i : {30, 31, 32, 33, 34}) r6.h[(size_t)i].m15 = 5.4f;
  r6.ocen(d(34));
  t = znajdz(r6.v(), "S-K-42");
  SPRAWDZ(t && t->t.tytul == "Wentylacja niższa niż zwykle");
  SPRAWDZ(t && t->t.powod == "Oddychasz 5,4 L/min, zwykle 7,2. Aparat pilnuje celu wentylacji.");
  SPRAWDZ(t && t->podpisLewo == "wentylacja · 14 nocy");
  Rano r4;
  r4.h = historia(35);
  for (int i : {30, 31, 32, 33, 34}) r4.h[(size_t)i].csr = 42;  // 10% nocy
  r4.ocen(d(34));
  t = znajdz(r4.v(), "S-K-46");
  SPRAWDZ(t && t->t.tytul == "Więcej oddychania okresowego");
  SPRAWDZ(t && t->t.powod == "10,0% nocy, zwykle 1,4%. Aparat ASV ma ten wzorzec wyrównywać.");
  SPRAWDZ(t && t->podpisPrawo == "linia przerywana = zwykle 1,4%");
}

// wymaganie: R-KON-64
// noc 7. stanu → krok „omów z lekarzem”, noce 1–6 → „obserwuj”
static void testR_KON_64_krok_od_siodmej_nocy() {
  for (const Karta& k : karty()) {
    Rano r;
    r.h = historia(39);
    for (int i = 30; i <= 38; i++) k.ustaw(r.h[(size_t)i], k.odch);  // wejście w nocy 32 (3. noc odchylenia)
    r.ocen(d(37));
    const sk::Tresc* t = znajdz(r.v(), k.id);
    SPRAWDZ(t && t->t.krok == "→ Obserwuj kolejne noce.");  // noc 6. stanu
    r.ocen(d(38));
    t = znajdz(r.v(), k.id);
    SPRAWDZ(t && t->t.krok == "→ Trwa od tygodnia. Omów to z lekarzem.");  // noc 7.
  }
}

// wymaganie: R-KON-65
// ta sama karta wraca po 7 dniach, dopóki trwa stan; dziś pokazana zostaje przy ponownej ocenie
static void testR_KON_65_powrot_po_7_dniach() {
  for (const Karta& k : karty()) {
    Rano r;
    r.h = historia(45);
    for (int i = 30; i < 45; i++) k.ustaw(r.h[(size_t)i], k.odch);
    r.dz.push_back(wpis(d(32), k.typ, std::string(k.id) + "/" + d(32)));
    r.ocen(d(32));
    SPRAWDZ(znajdz(r.v(), k.id) != nullptr);  // dziś pokazana: ponowna ocena tego dnia
    for (int i = 33; i <= 38; i++) {
      r.ocen(d(i));
      SPRAWDZ(znajdz(r.v(), k.id) == nullptr);
    }
    r.ocen(d(39));
    SPRAWDZ(znajdz(r.v(), k.id) != nullptr);
  }
}

// wymaganie: R-KON-64
// po wyjściu ze stanu (R-NOR-04: 3 kolejne noce w połowie Δ od mediany) → brak karty
static void testR_KON_64_po_wyjsciu_brak() {
  for (const Karta& k : karty()) {
    Rano r;
    r.h = historia(38);
    for (int i = 30; i <= 34; i++) k.ustaw(r.h[(size_t)i], k.odch);
    for (int i = 35; i <= 37; i++) k.ustaw(r.h[(size_t)i], k.wNormie);
    r.ocen(d(36));
    SPRAWDZ(znajdz(r.v(), k.id) != nullptr);  // 2 noce w normie: stan trwa
    r.ocen(d(37));
    SPRAWDZ(znajdz(r.v(), k.id) == nullptr);
  }
}

// wymaganie: R-KON-64
// brak normy → brak karty (sygnał bez 14 nocy z wartością; stan bez normy podany wprost)
static void testR_KON_64_bez_normy() {
  Rano r;
  r.h = historia(35);
  for (int i = 0; i < 25; i++) r.h[(size_t)i].psmed = NAN;  // 5 nocy z PS przed odchyleniem: bez normy M-28
  for (int i : {30, 32, 34}) r.h[(size_t)i].psmed = 8.0f;
  r.ocen(d(34));
  SPRAWDZ(!r.st.norma.psmed.jest() && znajdz(r.v(), "S-K-40") == nullptr);
  // stan aktywny, a dziś bez normy (np. zerowanie po zmianie ustawień, R-NOR-07): bez karty
  Rano r2;
  r2.h = historia(35);
  r2.ocen(d(34));
  r2.st.ps.aktywny = true;
  r2.st.ps.nocy = 3;
  r2.st.norma.psmed = NormaMetryki();
  SPRAWDZ(znajdz(r2.v(), "S-K-40") == nullptr);
  r2.st.norma = liczNorme(r2.h, d(34), {});
  SPRAWDZ(znajdz(r2.v(), "S-K-40") != nullptr);  // kontrola: z normą karta jest
  // rozruch globalny (< 14 nocy ważnych): bez kart
  Rano r3;
  r3.h = historia(13);
  r3.ocen(d(12));
  r3.st.ps.aktywny = true;
  r3.st.ps.nocy = 3;
  r3.st.norma.psmed.n = 14;
  SPRAWDZ(znajdz(r3.v(), "S-K-40") == nullptr);
}

// wymaganie: R-KON-65
// dwa kandydaty tego samego ranka → jedna karta (najdłużej trwający stan, potem niższe ID); problem poranny zostaje
// nad kartą, nie zamiast niej
static void testR_KON_65_jedna_karta_pod_problemami() {
  Rano r;
  r.h = historia(37);
  for (int i = 30; i <= 36; i++) r.h[(size_t)i].m10p = 0.25f;  // S-K-47 od nocy 32
  for (int i = 32; i <= 36; i++) r.h[(size_t)i].psmed = 8.0f;  // S-K-40 od nocy 34
  r.ocen(d(36));
  std::vector<sk::Tresc> v = r.v();
  SPRAWDZ(ileOdchylen(v) == 1 && znajdz(v, "S-K-47") && !znajdz(v, "S-K-40"));
  // remis długości: niższe ID
  Rano r2;
  r2.h = historia(37);
  for (int i = 32; i <= 36; i++) {
    r2.h[(size_t)i].m10p = 0.25f;
    r2.h[(size_t)i].psmed = 8.0f;
  }
  r2.ocen(d(36));
  v = r2.v();
  SPRAWDZ(ileOdchylen(v) == 1 && znajdz(v, "S-K-40"));
  // inna karta odchylenia pokazana dziś: nowej tego dnia nie ma (najwyżej jedna dziennie)
  r2.dz.push_back(wpis(d(36), "SK47", "S-K-47/" + d(34)));
  r2.ocen(d(36));
  v = r2.v();
  SPRAWDZ(ileOdchylen(v) == 1 && znajdz(v, "S-K-47"));
  // problem poranny (S-K-16, R5d) i karta odchylenia: obie na liście, karta niżej
  Rano r3;
  r3.h = historia(35);
  for (int i : {30, 32, 34}) r3.h[(size_t)i].psmed = 8.0f;
  r3.h[34].m17 = 4;
  r3.ocen(d(34));
  v = r3.v();
  int i16 = indeks(v, "S-K-16"), i40 = indeks(v, "S-K-40");
  SPRAWDZ(i16 >= 0 && i40 > i16);
  SPRAWDZ(sk::ranga("R5d") < sk::ranga("R5e") && sk::ranga("R5e") < sk::ranga("R6"));
  // ranga typów ekranu dnia bez zmiany kolejności grup R1, R6…R9
  SPRAWDZ(sk::rangaTypu("S7") == sk::ranga("R6") && sk::rangaTypu("KM") == sk::ranga("R7") &&
          sk::rangaTypu("SK13") == sk::ranga("R8") && sk::rangaTypu("S2A") == sk::ranga("R9") &&
          sk::rangaTypu("S5") == sk::ranga("R1"));
  SPRAWDZ(sk::typB16("SK40") && sk::typB16("SK47") && !sk::typWieczorny("SK45"));
  SPRAWDZ(!sk::problemNaEkranieRano(std::vector<sk::Tresc>{*znajdz(v, "S-K-40")}));
}

// wymaganie: R-KON-66
// S-K-45 z S-K-30 albo S-K-17 tego ranka → brak S-K-45; w okresie infekcji wstrzymany
static void testR_KON_66_sk45_wykluczenia() {
  auto oddech = [](Rano& r) {
    r.h = historia(35);
    for (int i = 30; i <= 34; i++) r.h[(size_t)i].m13 = 17.0f;
  };
  Rano r;
  oddech(r);
  r.ocen(d(34));
  SPRAWDZ(znajdz(r.v(), "S-K-45") && !znajdz(r.v(), "S-K-30") && !znajdz(r.v(), "S-K-17"));  // kontrola
  // S-K-30: oddechy w 2 nocach i EPAP P95 (tryb ASV, sygnał towarzyszący) ostatniej nocy
  Rano r30;
  oddech(r30);
  r30.h[34].ep95 = 11.0f;
  r30.ocen(d(34));
  SPRAWDZ(znajdz(r30.v(), "S-K-30") && !znajdz(r30.v(), "S-K-45"));
  // S-K-17: oddechy i wyciek w 2 kolejnych nocach (2 z 5 sygnałów), bez sygnału towarzyszącego S-K-30
  Rano r17;
  oddech(r17);
  r17.h[33].wp95 = 25;
  r17.h[34].wp95 = 25;
  r17.ocen(d(34));
  SPRAWDZ(znajdz(r17.v(), "S-K-17") && !znajdz(r17.v(), "S-K-45"));
  // okres infekcji (R-INF-02): wstrzymany jak S-K-17
  Rano ri;
  oddech(ri);
  ri.ocen(d(34));
  ri.w.okresInfekcji = true;
  SPRAWDZ(!znajdz(ri.v(), "S-K-45"));
}

// wymaganie: R-KON-67
// S-K-46 tylko w trybach ASV i ASVAuto i tylko gdy M-09 ostatniej nocy ma wartość (ADR-0037)
static void testR_KON_67_sk46_tryb_i_wartosc() {
  Rano r;
  r.h = historia(35);
  for (int i = 30; i <= 34; i++) r.h[(size_t)i].csr = 30;
  r.ocen(d(34), reguly::Tryb::ASV);
  SPRAWDZ(znajdz(r.v(), "S-K-46") != nullptr);
  r.ocen(d(34), reguly::Tryb::AUTOSET);
  SPRAWDZ(znajdz(r.v(), "S-K-46") == nullptr);
  r.ocen(d(34), reguly::Tryb::CPAP);
  SPRAWDZ(znajdz(r.v(), "S-K-46") == nullptr);
  r.h[34].csr = NAN;  // ostatnia noc bez M-09: karta się nie pokazuje (stan z 3 z 5 nocy trwa)
  r.ocen(d(34), reguly::Tryb::ASVAUTO);
  SPRAWDZ(r.st.csr.aktywny && znajdz(r.v(), "S-K-46") == nullptr);
}

// wymaganie: R-KON-64
// stan w kierunku karty: noce stanu wypadają z normy sygnału (R-NOR-09), metryka zdrowotna trzyma normę (R-NOR-22)
static void testR_KON_64_stan_i_norma() {
  std::vector<Noc> h = historia(45);
  for (int i = 30; i < 45; i++) h[(size_t)i].m13 = 17.0f;
  Stany s = liczStany(h, d(44), {});
  SPRAWDZ(s.m13.aktywny && s.m13.wyzej && s.m13.od == d(32) && s.m13.nocy == 13);
  SPRAWDZ(s.wyl.m13.count(d(40)) == 1 && std::fabs(s.norma.m13.med - 14.4) < 1e-4);
  // PS: Δ +1 cmH2O (komplet progów 11.11.2026): mediana 6,2 → 7,2 wchodzi, 7,1 nie
  std::vector<Noc> h2 = historia(35);
  for (int i : {30, 32, 34}) h2[(size_t)i].psmed = 7.2f;
  SPRAWDZ(liczStany(h2, d(34), {}).ps.aktywny);
  for (int i : {30, 32, 34}) h2[(size_t)i].psmed = 7.1f;
  SPRAWDZ(!liczStany(h2, d(34), {}).ps.aktywny);
  // PS w trybie CPAP nie liczy się (4.11: wsparcie tylko w trybie ASV)
  for (auto& n : h2) { n.tryb = "AutoSet"; n.ustawienia = "AutoSet/5/12"; }
  for (int i : {30, 32, 34}) h2[(size_t)i].psmed = 8.0f;
  SPRAWDZ(!liczStany(h2, d(34), {}).ps.aktywny);
  // M-10 P95: Δ +0,15; 0,23 − 0,07 ≥ 0,15 → tak; 0,21 → nie
  std::vector<Noc> h3 = historia(35);
  for (int i : {30, 32, 34}) h3[(size_t)i].m10p = 0.23f;
  SPRAWDZ(liczStany(h3, d(34), {}).m10.aktywny);
  for (int i : {30, 32, 34}) h3[(size_t)i].m10p = 0.21f;
  SPRAWDZ(!liczStany(h3, d(34), {}).m10.aktywny);
}

// wymaganie: R-KON-68
// linia „dobra zmiana”: AHI w dobrym kierunku 3 z 5 → linia; drugi raz najwcześniej po 7 dniach od pokazania
static void testR_KON_68_dobra_zmiana() {
  std::vector<Noc> h = historia(50);
  for (int i : {30, 32, 34}) h[(size_t)i].ahi = 0.5f;
  for (int i = 35; i < 50; i++) h[(size_t)i].ahi = 0.5f;
  Stany s = liczStany(h, d(34), {});
  SPRAWDZ(s.dobraAhi.aktywny && !s.dobraAhi.wyzej && s.dobraAhi.odPierwszej == 5);
  std::string l = sk::liniaDobrejZmiany(s, true);
  SPRAWDZ(l == "Dobra zmiana: AHI niżej niż zwykle od 5 nocy.");
  // noce dobrej zmiany zostają w normie (to nie stan odchylenia z 4.6)
  SPRAWDZ(s.wyl.ahi.count(d(34)) == 0);
  // 2 z 5 → bez linii
  std::vector<Noc> h2 = historia(35);
  for (int i : {31, 33}) h2[(size_t)i].ahi = 0.5f;
  SPRAWDZ(sk::liniaDobrejZmiany(liczStany(h2, d(34), {}), true).empty());
  // kolejka: grupa „norma”, rytm 7 dni od pokazania (R-PIE-08)
  reguly::Pamiec pam;
  std::vector<sprawy::SprawaL> sp;
  kolejka::WejscieKolejki wk;
  wk.sprawy = &sp;
  wk.pamiec = &pam;
  wk.dzis = d(34);
  wk.dobraZmiana = l;
  std::vector<kolejka::LiniaUwagi> k = kolejka::kolejkaLinii(wk);
  SPRAWDZ(k.size() == 1 && k[0].tekst == l && k[0].grupa == GrupaLinii::NORMA && !k[0].zdrowie());
  if (!k.empty()) sprawy::zapiszPokazanie(pam, kolejka::kluczPokazaniaLinii(k[0]), d(34));
  for (int i = 35; i <= 40; i++) {
    wk.dzis = d(i);
    wk.dobraZmiana = sk::liniaDobrejZmiany(liczStany(h, d(i), {}), true);
    SPRAWDZ(!wk.dobraZmiana.empty() && kolejka::kolejkaLinii(wk).empty());
  }
  wk.dzis = d(41);
  wk.dobraZmiana = sk::liniaDobrejZmiany(liczStany(h, d(41), {}), true);
  SPRAWDZ(kolejka::kolejkaLinii(wk).size() == 1);
  // czas z maską w górę: `wyżej`; wyciek, przerwy, PS (tylko ASV) i spłaszczenie w dół: `niżej`
  std::vector<Noc> h3 = historia(35);
  for (int i : {30, 32, 34}) h3[(size_t)i].m01 = 600;  // norma 7 h, +3 h ≥ Δ 1,5 h
  Stany s3 = liczStany(h3, d(34), {});
  SPRAWDZ(sk::liniaDobrejZmiany(s3, false) == "Dobra zmiana: czas z maską wyżej niż zwykle od 5 nocy.");
  std::vector<Noc> h4 = historia(35);
  for (int i : {30, 32, 34}) h4[(size_t)i].psmed = 5.0f;
  Stany s4 = liczStany(h4, d(34), {});
  SPRAWDZ(sk::liniaDobrejZmiany(s4, true) == "Dobra zmiana: PS niżej niż zwykle od 5 nocy.");
  SPRAWDZ(sk::liniaDobrejZmiany(s4, false).empty());
  // linia w limicie 60 znaków (5.4.4) dla każdego wskaźnika przy 3-cyfrowej liczbie nocy
  for (const char* nz : {"AHI", "PS", "wyciek", "przerwy", "spłaszczenie", "czas z maską"}) {
    std::string t = sk::tekst("R-KON-68.linia", {{"wskaznik", nz}, {"kierunek", "wyżej"}, {"n", "120"}});
    SPRAWDZ(dlugoscZnakow(t) <= 60);
  }
}

// wymaganie: R-KON-64
// teksty mieszczą się na ekranie (szablon A): tytuł w jednej linii SB44, powód w 2 liniach P28B, krok i podpisy
// w 744 px, przy liczbach o największej szerokości z danych
static void testR_KON_64_teksty_na_ekranie() {
  const char* ids[] = {"S-K-40", "S-K-41", "S-K-42", "S-K-43", "S-K-44", "S-K-45", "S-K-46", "S-K-47"};
  for (const char* id : ids) {
    std::string p = id;
    std::map<std::string, std::string> z = {{"x", "1888"}, {"norma", "1888"}};
    std::string tyt = sk::tekst(p + ".tytul"), pow_ = sk::tekst(p + ".powod", z);
    SPRAWDZ(!tyt.empty() && dlugoscZnakow(tyt) <= 30 && zawin(F_SB44, tyt, 744).size() == 1);
    SPRAWDZ(!pow_.empty() && dlugoscZnakow(sk::tekst(p + ".powod", {{"x", "88,8"}, {"norma", "88,8"}})) <= 70);
    SPRAWDZ(zawin(F_P28B, pow_, 744).size() <= 2);
    for (const char* kr : {".krok", ".krok_lekarz"}) {
      std::string t = sk::tekst(p + kr);
      SPRAWDZ(!t.empty() && dlugoscZnakow(t) <= 60 && szer(F_P28B, t) <= 744);
    }
    std::string pl = sk::tekst(p + ".podpis_lewo"), pp = sk::tekst(p + ".podpis_prawo", z);
    SPRAWDZ(!pl.empty() && dlugoscZnakow(pl) <= 40 && dlugoscZnakow(pp) <= 40);
    SPRAWDZ(szer(F_P22B, pl) + 24 + szer(F_P22B, pp) <= 744);
  }
}

// wymaganie: R-NOR-07
// zmiana ustawień w ASVAuto zeruje normę M-13 (14 nocy bez normy) i kończy stan M-13; w AutoSet bez zerowania
static void testR_NOR_07_oddech_zerowany_w_asv() {
  std::vector<Noc> h = historia(45);
  for (int i = 30; i < 45; i++) h[(size_t)i].m13 = 17.0f;
  for (int i = 38; i < 45; i++) h[(size_t)i].ustawienia = "ASVAuto/6/10/4/15";  // zmiana PS w nocy 38
  Stany s = liczStany(h, d(44), {});
  SPRAWDZ(!s.m13.aktywny && !s.norma.m13.jest());
  SPRAWDZ(!s.norma.m14.jest() && !s.norma.m15.jest());  // M-14, M-15 zerowane razem z M-13
  SPRAWDZ(liczStany(h, d(37), {}).m13.aktywny);  // przed zmianą stan trwał
  // po 14 nocach z nowymi ustawieniami norma z tych nocy (nowy poziom oddechu bez stanu)
  std::vector<Noc> h2 = historia(60);
  for (int i = 30; i < 60; i++) {
    h2[(size_t)i].m13 = 17.0f + 0.2f * (float)(i % 5);
    h2[(size_t)i].ustawienia = "ASVAuto/6/10/4/15";
  }
  Stany s2 = liczStany(h2, d(59), {});
  SPRAWDZ(s2.norma.m13.jest() && s2.norma.m13.med > 16.9 && !s2.m13.aktywny);
  // AutoSet: ta sama zmiana nie zeruje normy M-13
  std::vector<Noc> h3 = historia(45);
  for (auto& n : h3) { n.tryb = "AutoSet"; n.ustawienia = "AutoSet/5/12"; }
  for (int i = 38; i < 45; i++) h3[(size_t)i].ustawienia = "AutoSet/5/14";
  for (int i = 30; i < 45; i++) h3[(size_t)i].m13 = 17.0f;
  Stany s3 = liczStany(h3, d(44), {});
  SPRAWDZ(s3.norma.m13.jest() && s3.m13.aktywny);
  // PS: zmiana ustawień kończy stan PS (R-NOR-22 fail-path)
  std::vector<Noc> h4 = historia(45);
  for (int i = 30; i < 45; i++) h4[(size_t)i].psmed = 8.0f;
  for (int i = 40; i < 45; i++) h4[(size_t)i].ustawienia = "ASVAuto/6/10/4/15";
  SPRAWDZ(liczStany(h4, d(39), {}).ps.aktywny && !liczStany(h4, d(44), {}).ps.aktywny);
}

// wymaganie: R-NOR-12
// Δ M-06p (EPAP P95, tryby ASV i ASVAuto) = 1,5 cmH2O od 11.11.2026 (komplet progów): E-36 (5 z 7 nocy) przy +1,6,
// nie przy +1,4
static void testR_NOR_12_delta_m06p_asv() {
  std::vector<Noc> h = historia(37);
  for (int i = 30; i < 37; i++) h[(size_t)i].ep95 = 9.8f;  // mediana normy 8,2, P90 8,4
  Stany s = liczStany(h, d(36), {});
  SPRAWDZ(s.e36 && s.ep95.aktywny);
  for (int i = 30; i < 37; i++) h[(size_t)i].ep95 = 9.6f;
  s = liczStany(h, d(36), {});
  SPRAWDZ(!s.e36 && !s.ep95.aktywny);
  // CPAP i AutoSet: Δ M-05p bez zmian (2 cmH2O)
  std::vector<Noc> h2 = historia(37);
  for (auto& n : h2) { n.tryb = "AutoSet"; n.ustawienia = "AutoSet/5/12"; n.cp95 = 10.0f + 0.1f * (float)(std::stoi(n.data.substr(8)) % 5); }
  for (int i = 30; i < 37; i++) h2[(size_t)i].cp95 = 11.5f;
  SPRAWDZ(!liczStany(h2, d(36), {}).e36);
  for (int i = 30; i < 37; i++) h2[(size_t)i].cp95 = 12.5f;
  SPRAWDZ(liczStany(h2, d(36), {}).e36);
}

// wymaganie: R-NOR-12
// Δ względne M-14 i M-15 (15% mediany normy): dokładnie 15% → odchylenie, tuż pod progiem → nie, mediana normy 0 → brak
// Δ (sygnał pomijany), mediana bliska zera → Δ też bliskie zera; S-K-30: sygnał towarzyszący M-15 według 15%
static void testR_NOR_12_delta_wzgledna() {
  auto wynik = [](float baza, float v) {
    std::vector<Noc> h = historia(35);
    for (auto& n : h) n.m15 = baza;
    for (int i : {30, 32, 34}) h[(size_t)i].m15 = v;
    return liczStany(h, d(34), {}).m15g.aktywny;
  };
  SPRAWDZ(wynik(8.0f, 9.2f));       // dokładnie +15%
  SPRAWDZ(!wynik(8.0f, 9.15f));     // tuż pod progiem
  SPRAWDZ(!wynik(0.0f, 5.0f));      // mediana 0: brak Δ
  SPRAWDZ(wynik(0.01f, 0.0116f));   // mediana bliska zera: Δ 0,0015
  // w dół (S-K-42) i M-14 (S-K-43): −15%
  std::vector<Noc> h = historia(35);
  for (auto& n : h) n.m14 = 500;
  for (int i : {30, 32, 34}) h[(size_t)i].m14 = 425;  // −15%
  SPRAWDZ(liczStany(h, d(34), {}).m14d.aktywny);
  for (int i : {30, 32, 34}) h[(size_t)i].m14 = 426;
  SPRAWDZ(!liczStany(h, d(34), {}).m14d.aktywny);
  // S-K-30: M-13 +3 w 2 nocach, M-15 +15% ostatniej nocy → sygnał „wentylacja też wyższa”; +14% → bez sygnału
  std::vector<Noc> h30 = historia(35);
  for (auto& n : h30) n.m15 = 8.0f;
  h30[33].m13 = h30[34].m13 = 17.5f;
  h30[34].m15 = 9.2f;
  Norma nm = liczNorme(h30, d(34), {});
  SPRAWDZ(infekcja::ocenSK30(h30, nm, d(34), true).warunek);
  h30[34].m15 = 9.12f;
  SPRAWDZ(!infekcja::ocenSK30(h30, nm, d(34), true).warunek);
  for (auto& n : h30) n.m15 = 0.0f;
  h30[34].m15 = 5.0f;
  nm = liczNorme(h30, d(34), {});
  SPRAWDZ(!infekcja::ocenSK30(h30, nm, d(34), true).warunek);  // mediana 0: sygnał pomijany (R-INF-01)
}

// wymaganie: R-NOR-03
// Spadek czasu z maską liczy 3 z 5 także z nocy z terapią krótszych niż 4 h (noce nieważne)
static void testM01_krotkie_noce_w_spadku() {
  std::vector<Noc> h = historia(35);
  for (int i : {30, 32, 34}) {
    h[(size_t)i].m01 = 200;  // 3 h 20: nie jest nocą ważną (4.2)
    h[(size_t)i].wazna = false;
  }
  Stany s = liczStany(h, d(34), {});
  SPRAWDZ(s.m01.aktywny && !s.m01.wyzej && s.odchylenie.m01);
  SPRAWDZ(s.norma.m01.jest() && std::fabs(s.norma.m01.med - 420) < 1e-6);  // norma tylko z nocy ważnych
  // noc bez terapii (M-01 < 15 min) i noc częściowa nie liczą się (E-10, R-DANE-15)
  for (int i : {30, 32, 34}) h[(size_t)i].m01 = 10;
  SPRAWDZ(!liczStany(h, d(34), {}).m01.aktywny);
  for (int i : {30, 32, 34}) { h[(size_t)i].m01 = 200; h[(size_t)i].czesciowa = true; }
  SPRAWDZ(!liczStany(h, d(34), {}).m01.aktywny);
}

// wymaganie: R-NOR-07
// Przypadek brzegowy: noc zmiany ustawień bez wartości metryki (M-14/M-15 z karty albo JSON, M-28 bez próbek 2 s,
// R-DANE-32; M-07) też zeruje stan; potem norma z nowych ustawień i bez karty przy nowym poziomie
static void testR_NOR_07_noc_zmiany_bez_wartosci() {
  {
    std::vector<Noc> h = historia(70);
    for (int i = 30; i < 70; i++) h[(size_t)i].m14 = 380;
    for (int i = 40; i < 70; i++) h[(size_t)i].ustawienia = "ASVAuto/6/10/4/15";
    h[40].m14 = NAN;
    h[40].m15 = NAN;
    SPRAWDZ(liczStany(h, d(39), {}).m14d.aktywny);
    SPRAWDZ(!liczStany(h, d(44), {}).m14d.aktywny);
    Rano r;
    r.h = h;
    r.ocen(d(69));
    SPRAWDZ(!r.st.m14d.aktywny && r.st.norma.m14.jest() && std::fabs(r.st.norma.m14.med - 380) < 1e-6);
    SPRAWDZ(znajdz(r.v(), "S-K-43") == nullptr);
  }
  {
    std::vector<Noc> h = historia(70);
    for (int i = 30; i < 70; i++) h[(size_t)i].psmed = 8.0f;
    for (int i = 40; i < 70; i++) h[(size_t)i].ustawienia = "ASVAuto/6/10/4/15";
    h[40].psmed = NAN;
    SPRAWDZ(!liczStany(h, d(69), {}).ps.aktywny);
  }
  {
    std::vector<Noc> h = historia(70);
    for (int i = 30; i < 70; i++) h[(size_t)i].wp95 = 25.0f;
    for (int i = 40; i < 70; i++) h[(size_t)i].ustawienia = "ASVAuto/6/10/4/15";
    h[40].wp95 = NAN;
    SPRAWDZ(liczStany(h, d(39), {}).wp95.aktywny && !liczStany(h, d(50), {}).wp95.aktywny);
  }
}

// wymaganie: R-KON-64
// Przypadek brzegowy: stan trwa dłużej, niż dzisiejsza norma ma noce (okno 90 dni bez nocy stanu, R-NOR-09) → karta
// dalej, z normą zamrożoną przy wejściu (R-NOR-22) w powodzie i na wykresie; tak dla każdej z 8 kart
static void testR_KON_64_dlugi_stan_norma_zamrozona() {
  std::vector<Noc> h = historia(160);
  for (int i = 30; i < 160; i++) h[(size_t)i].psmed = 8.0f;
  for (int dz : {40, 110, 130, 159}) {
    Rano r;
    r.h = h;
    r.ocen(d(dz));
    const sk::Tresc* t = znajdz(r.v(), "S-K-40");
    SPRAWDZ(r.st.ps.aktywny && r.st.ps.zamrozona.jest());
    SPRAWDZ(t && t->t.powod == "PS 8,0 cmH2O, zwykle 6,2. Bywa przy zatkanym nosie lub na plecach.");
    SPRAWDZ(t && std::fabs(t->prog - 6.2f) < 1e-4f);
    if (dz >= 130) SPRAWDZ(!r.st.norma.psmed.jest());  // dzisiejsza norma już znikła
  }
  for (const Karta& k : karty()) {
    Rano r;
    r.h = historia(140);
    for (int i = 30; i < 140; i++) k.ustaw(r.h[(size_t)i], k.odch);
    r.ocen(d(139));
    SPRAWDZ(znajdz(r.v(), k.id) != nullptr);
  }
}

int main() {
  testR_NOR_07_noc_zmiany_bez_wartosci();
  testR_KON_64_dlugi_stan_norma_zamrozona();
  testR_NOR_12_delta_wzgledna();
  testM01_krotkie_noce_w_spadku();
  testR_NOR_07_oddech_zerowany_w_asv();
  testR_NOR_12_delta_m06p_asv();
  testR_KON_64_trzy_z_pieciu();
  testR_KON_64_teksty();
  testR_KON_64_krok_od_siodmej_nocy();
  testR_KON_65_powrot_po_7_dniach();
  testR_KON_64_po_wyjsciu_brak();
  testR_KON_64_bez_normy();
  testR_KON_65_jedna_karta_pod_problemami();
  testR_KON_66_sk45_wykluczenia();
  testR_KON_67_sk46_tryb_i_wartosc();
  testR_KON_64_stan_i_norma();
  testR_KON_68_dobra_zmiana();
  testR_KON_64_teksty_na_ekranie();
  printf("alerty odchyleń (ADR-0039): %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
