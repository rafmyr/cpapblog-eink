// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (zdarzenia E, src/rodzaje_e.cpp, docs/spec/katalog/E.csv).
// Niezależne od test/testy_zdarzenia_e.cpp. Komentarz przy teście opisuje sprawdzany przypadek; testy „trzyma”
// sprawdzają zachowania zgodne ze spec.
// Dane wyłącznie syntetyczne. Uruchomienie: tools/prototyp/testy.sh (program testy_zdarzenia_e_brzegowe).
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "noc.h"
#include "reguly.h"
#include "reguly_pomoc.h"
#include "reguly_sygnaly.h"
#include "stany.h"
#include "wspolne.h"

namespace reguly {
void zarejestrujRodzajeE();
using AtrapaMetryki = double (*)(const Noc&, const std::string&);
void ustawAtrapeMetrykiE(AtrapaMetryki f);
}  // namespace reguly

using reguly::Tryb;
using reguly::Wynik;

static int bledy = 0, sprawdzen = 0;
#define SPRAWDZ(warunek, opis)                                   \
  do {                                                           \
    sprawdzen++;                                                 \
    if (!(warunek)) {                                            \
      bledy++;                                                   \
      std::printf("BŁĄD %s:%d %s\n", __FILE__, __LINE__, opis); \
    }                                                            \
  } while (0)

static std::map<std::string, double> M08;
static double atrapaM08(const Noc& n, const std::string& id) {
  if (id != "M-08") return NAN;
  auto it = M08.find(n.data);
  return it == M08.end() ? NAN : it->second;
}

static const char* START = "2026-05-01";
static std::string dzien(int i) { return dodajDni(START, i); }

static Noc baza(int i, const char* ust = "ASVAuto/6/10/3/15") {
  Noc n;
  n.data = dzien(i);
  n.wazna = true;
  n.m01 = 420;
  n.ahi = 1.0f;
  n.cmed = 12.0f;
  n.cp95 = 14.0f;
  n.wmed = 5.0f;
  n.wp95 = 12.0f;  // w wariancie C noc z P95 ≥ 15 L/min jest niepewna
  n.emed = 8.0f;
  n.ep95 = 10.0f;
  n.psmed = 4.0f;
  n.psp95 = 6.0f;
  n.ustawienia = ust;
  n.oa = 0.5f;
  n.ca = 1.0f;
  n.h = 0.5f;
  n.csr = 0.0f;
  return n;
}
static std::vector<Noc> spokojne(int ile, const char* ust = "ASVAuto/6/10/3/15") {
  std::vector<Noc> h;
  for (int i = 0; i < ile; i++) h.push_back(baza(i, ust));
  return h;
}
static Noc& dopisz(std::vector<Noc>& h) {
  std::string ust = h.empty() ? std::string("ASVAuto/6/10/3/15") : h.back().ustawienia;
  h.push_back(baza(h.empty() ? 0 : dniMiedzy(START, h.back().data) + 1));
  h.back().ustawienia = ust;
  return h.back();
}

static const reguly::Odpowiedzi BEZ_ODPOWIEDZI;
static const reguly::Pamiec BEZ_PAMIECI;

static std::vector<Wynik> ocena(const std::vector<Noc>& h, const std::string& dzis, Tryb tryb = Tryb::ASVAUTO,
                                const reguly::Sygnaly* syg = nullptr, const reguly::Pamiec* pam = nullptr,
                                Stany* stanyWyj = nullptr) {
  Stany s = liczStany(h, dzis, {});
  if (stanyWyj) *stanyWyj = s;
  reguly::Kontekst k;
  k.historia = &h;
  k.dzis = dzis;
  k.norma = &s.norma;
  k.stany = &s;
  k.trybBiezacy = tryb;
  k.odpowiedzi = &BEZ_ODPOWIEDZI;
  k.pamiec = pam ? pam : &BEZ_PAMIECI;
  k.zestaw = &reguly::zestawWbudowany();
  reguly::OpcjeOceny o;
  o.sygnaly = syg;
  return reguly::ocen(*k.zestaw, k, o);
}
static std::string ostatni(const std::vector<Noc>& h) { return h.back().data; }
static const Wynik* znajdz(const std::vector<Wynik>& v, const std::string& id, const std::string& wariant = "") {
  for (const Wynik& w : v)
    if (w.id == id && w.wariant == wariant) return &w;
  return nullptr;
}
static bool aktywna(const std::vector<Wynik>& v, const std::string& id, const std::string& wariant = "") {
  const Wynik* w = znajdz(v, id, wariant);
  return w != nullptr && w->oceniona && w->spelniona;
}
static bool niespelniona(const std::vector<Wynik>& v, const std::string& id, const std::string& wariant = "") {
  const Wynik* w = znajdz(v, id, wariant);
  return w != nullptr && w->oceniona && !w->spelniona;
}
static std::string tekst(const std::string& idT, const Wynik* w, Tryb t) {
  return w ? reguly::tekst(reguly::zestawWbudowany(), idT, *w, t) : std::string("<brak wyniku>");
}

// ---------- zdarzenia E bez wariantu w 4.11 działają w każdym trybie obsługiwanym ----------
// 4.11: „Reguła, której tu nie ma, działa tak samo w każdym trybie”; wycofane (wszystkie cztery tryby).
// Katalog ma `tryby = ASV,ASVAuto` przy E-13, E-14, E-21, E-23, E-30, E-31, E-33, E-36: w CPAP i AutoSet milczą.
static void testR01Tryby() {
  // E-30: mediana AHI 8 z 7 nocy, tryb CPAP
  auto h = spokojne(30, "CPAP/9");
  for (int i = 0; i < 7; i++) dopisz(h).ahi = 8.0f;
  auto w = ocena(h, ostatni(h), Tryb::CPAP);
  SPRAWDZ(aktywna(w, "E-30"), "R-01: E-30 w trybie CPAP (mediana AHI 8) milczy");
  // E-23: wyciek P95 30 L/min ostatniej nocy, tryb AutoSet
  h = spokojne(30, "AutoSet/5/13/3");
  dopisz(h).wp95 = 30.0f;
  w = ocena(h, ostatni(h), Tryb::AUTOSET);
  SPRAWDZ(aktywna(w, "E-23"), "R-01: E-23 w trybie AutoSet (P95 30 L/min) milczy");
  // E-13: stan odchylenia M-01 (noce o 2 h krótsze), tryb CPAP
  h = spokojne(30, "CPAP/9");
  for (int i = 0; i < 5; i++) dopisz(h).m01 = 300;
  w = ocena(h, ostatni(h), Tryb::CPAP);
  SPRAWDZ(aktywna(w, "E-13"), "R-01: E-13 w trybie CPAP (stan odchylenia M-01) milczy");
  // E-36 w AutoSet: sygnał M-05p (ciśnienie P95), teksty `Ciśnienie wyższe niż zwykle` (6.4, 4.11)
  h = spokojne(30, "AutoSet/5/13/3");
  for (int i = 0; i < 7; i++) dopisz(h).cp95 = 17.0f;
  w = ocena(h, ostatni(h), Tryb::AUTOSET);
  const Wynik* e = znajdz(w, "E-36", "obserwuj");
  SPRAWDZ(e != nullptr && e->oceniona && e->spelniona, "R-01: E-36 w AutoSet (M-05p +3 w 7 z 7) milczy");
  SPRAWDZ(tekst("E-36.tytul", e, Tryb::AUTOSET) == "Ciśnienie wyższe niż zwykle",
          "R-01: E-36 poza ASV bez tytułu `Ciśnienie wyższe niż zwykle` (6.4)");
}

// ---------- E-33/mediana liczy noce niepewne (R-NOR-05: noc niepewna poza regułami M-03) ----------
static void testR02Niepewne() {
  auto h = spokojne(30);
  for (int i = 0; i < 5; i++) {
    Noc& n = dopisz(h);
    n.ca = 5.0f;
    n.ahi = 8.0f;
    n.wmed = 35.0f;  // mediana wycieku > 30 L/min: noc niepewna (4.9)
  }
  auto w = ocena(h, ostatni(h));
  SPRAWDZ(!aktywna(w, "E-33", "mediana"), "R-02: E-33/mediana spełniona na 5 nocach niepewnych (CA/h 5)");
  // trzyma: E-30 ma `wazne_bez_niepewnych`
  SPRAWDZ(!aktywna(w, "E-30"), "E-30: noce niepewne nie liczą się do mediany AHI");
}

// ---------- trzyma: linia E-21 o suchym powietrzu znika, gdy E-22 wyklucza E-21 (6.4, 7.2 R5c) ----------
static void testR03WilgotnoscPrzyE22() {
  reguly::ustawAtrapeMetrykiE(atrapaM08);
  auto h = spokojne(30);
  for (const Noc& n : h) M08[n.data] = 0;
  for (int i = 0; i < 5; i++) {
    Noc& n = dopisz(h);
    n.wp95 = 24.0f;          // M-07 P95 +12: odchylenie, stan od 3. nocy
    M08[n.data] = 60;        // E-20 w 5 z 5: E-22
  }
  reguly::Sygnaly s;
  s.odczyty["wilgotnosc_min_nocy"] = 20;
  auto w = ocena(h, ostatni(h), Tryb::ASVAUTO, &s);
  SPRAWDZ(aktywna(w, "E-22"), "E-22 aktywne (warunek testu)");
  SPRAWDZ(!aktywna(w, "E-21"), "E-22 wyklucza E-21");
  SPRAWDZ(!aktywna(w, "E-21", "wilgotnosc"),
          "linia E-21 o suchym powietrzu przy E-21 wykluczonym przez E-22");
  reguly::ustawAtrapeMetrykiE(nullptr);
}

// ---------- E-05 krytyczne trzyma ekran S5 aż do 20%, choć poziom krytyczny to ≤ 5% ----------
static void testR04Bateria() {
  auto h = spokojne(20);
  reguly::Sygnaly s;
  reguly::Pamiec p;
  p["E-05/krytyczne"].aktywnaOd = dzien(15);  // wczoraj 4%: krytyczne
  s.odczyty["M-25"] = 10;                      // po częściowym ładowaniu
  auto w = ocena(h, ostatni(h), Tryb::ASVAUTO, &s, &p);
  SPRAWDZ(!aktywna(w, "E-05", "krytyczne") && aktywna(w, "E-05", "linia"),
          "R-04: 10% po krytycznym nadal `brak danych` (S5) zamiast linii (6.4: krytyczne ≤ 5%)");
  // trzyma: granice
  reguly::Pamiec pusta;
  s.odczyty["M-25"] = 5;
  SPRAWDZ(aktywna(ocena(h, ostatni(h), Tryb::ASVAUTO, &s, &pusta), "E-05", "krytyczne"), "E-05: 5% krytyczne");
  s.odczyty["M-25"] = 6;
  w = ocena(h, ostatni(h), Tryb::ASVAUTO, &s, &pusta);
  SPRAWDZ(!aktywna(w, "E-05", "krytyczne") && aktywna(w, "E-05", "linia"), "E-05: 6% linia");
  s.odczyty["M-25"] = NAN;
  w = ocena(h, ostatni(h), Tryb::ASVAUTO, &s, &pusta);
  const Wynik* e = znajdz(w, "E-05", "linia");
  SPRAWDZ(e && !e->oceniona, "E-05: NAN → nie oceniam");
}

// ---------- E-10 „problem” znika po nocy z terapią < 4 h (6.4: znika po nocy ≥ 4 h) ----------
static void testR05E10Wyjscie() {
  auto h = spokojne(20);
  for (int i = 0; i < 2; i++) {
    Noc& n = dopisz(h);
    n.m01 = 0;
    n.wazna = false;
  }
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-10", "problem"), "E-10: 2 noce bez terapii → problem");
  Noc& n = dopisz(h);
  n.m01 = 120;  // 2 h terapii
  n.wazna = false;
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-10", "problem"),
          "R-05: E-10 `problem` znika po nocy 2 h (spec: znika po nocy ≥ 4 h)");
}

// ---------- E-10 „problem” bez wcześniejszej terapii daje „Ostatnia terapia: .” ----------
static void testR06E10BezTerapii() {
  std::vector<Noc> h;
  for (int i = 0; i < 3; i++) {
    Noc n = baza(i);
    n.m01 = 0;
    n.wazna = false;
    h.push_back(n);
  }
  auto w = ocena(h, ostatni(h));
  const Wynik* e = znajdz(w, "E-10", "problem");
  std::string t = tekst("E-10.powod", e, Tryb::ASVAUTO);
  SPRAWDZ(!(e && e->spelniona) || t != "Ostatnia terapia: .", "R-06: E-10 powód z pustą datą `Ostatnia terapia: .`");
}

// ---------- trzyma: E-36 z zamrożoną normą (R-NOR-22) po ponad 90 dniach stanu ----------
static void testE36Zamrozenie() {
  auto h = spokojne(30);
  for (int i = 0; i < 100; i++) dopisz(h).ep95 = 13.0f;
  auto w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-36", "lekarz"), "E-36: po 100 nocach stanu norma zamrożona, krok do lekarza");
  const Wynik* e = znajdz(w, "E-36", "lekarz");
  SPRAWDZ(e && e->zmienne.count("norma") && e->zmienne.at("norma") == "10,0", "E-36: {norma} z nocy przed stanem");
  // zgodność z src/stany.cpp (Stany::e36) na przebiegu z falą odchyleń
  auto g = spokojne(30);
  const float fala[] = {13, 10, 13, 13, 10, 13, 13, 10, 10, 10, 13, 13, 13, 10, 10, 10, 10, 13};
  int rozne = 0;
  for (float v : fala) {
    dopisz(g).ep95 = v;
    Stany s;
    auto ww = ocena(g, ostatni(g), Tryb::ASVAUTO, nullptr, nullptr, &s);
    bool e36 = aktywna(ww, "E-36", "obserwuj") || aktywna(ww, "E-36", "lekarz");
    if (e36 != s.e36) rozne++;
  }
  SPRAWDZ(rozne == 0, "E-36 z katalogu zgodne ze Stany::e36 noc po nocy");
}

// ---------- E-36 nie odfiltrowuje nocy w trybie spoza wydania (R-DANE-29) ----------
// W imporcie (airbridge.cpp) noc w innym trybie nie jest ważna, więc dziś to obrona w głąb; rdzeń (noceOkna) filtruje.
static void testR07TrybSpoza() {
  auto h = spokojne(30);
  for (int i = 0; i < 2; i++) dopisz(h);  // 2 noce spokojne
  for (int i = 0; i < 4; i++) dopisz(h).ep95 = 13.0f;
  Noc& obca = dopisz(h);
  obca.ep95 = 13.0f;
  obca.ustawienia = "VAuto/4/12";
  Noc& ost = dopisz(h);
  ost.ustawienia = "ASVAuto/6/10/3/15";
  ost.ep95 = 10.0f;
  auto w = ocena(h, ostatni(h));
  SPRAWDZ(!aktywna(w, "E-36", "obserwuj"), "R-07: E-36 liczy noc w trybie VAuto do 5 z 7");
}

// ---------- trzyma: granice progów ----------
static void testGranice() {
  auto h = spokojne(30);
  const float ahi[] = {5, 5, 5, 5, 1, 1, 1};  // mediana dokładnie 5
  for (float a : ahi) dopisz(h).ahi = a;
  auto w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-30"), "E-30: mediana 5,0 → aktywne");
  const Wynik* e = znajdz(w, "E-30");
  SPRAWDZ(tekst("E-30.powod", e, Tryb::ASVAUTO) == "AHI powyżej 5 w 4 z 7 nocy, ostatnio 1,0.",
          "E-30: powód z {n} = noce z AHI ≥ 5");
  SPRAWDZ(!aktywna(w, "E-31"), "E-30 wyklucza E-31");
  h = spokojne(30);
  for (float a : {4.9f, 4.9f, 4.9f, 4.9f, 1.f, 1.f, 1.f}) dopisz(h).ahi = a;
  SPRAWDZ(niespelniona(ocena(h, ostatni(h)), "E-30"), "E-30: mediana 4,9 → nie");
  h = spokojne(30);
  dopisz(h).wp95 = 27.0f;
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-23"), "E-23: 27 L/min → aktywne");
  h.back().wp95 = 26.9f;
  SPRAWDZ(niespelniona(ocena(h, ostatni(h)), "E-23"), "E-23: 26,9 → nie");
  h.back().wp95 = 27.0f;
  h.back().m01 = 14;
  h.back().wazna = false;
  w = ocena(h, ostatni(h));
  SPRAWDZ(!aktywna(w, "E-23"), "E-23: noc 14 min to nie noc z terapią");
  // E-11 i linia E-10
  h = spokojne(20);
  dopisz(h).m01 = 15;
  h.back().wazna = false;
  w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-11") && !aktywna(w, "E-10", "linia"), "E-11: 15 min krótka noc, nie E-10");
  h.back().m01 = 14;
  w = ocena(h, ostatni(h));
  SPRAWDZ(!aktywna(w, "E-11") && aktywna(w, "E-10", "linia"), "E-10: 14 min bez terapii");
  h.back().m01 = 239;
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-11"), "E-11: 239 min");
  h.back().m01 = 240;
  h.back().wazna = true;
  SPRAWDZ(niespelniona(ocena(h, ostatni(h)), "E-11"), "E-11: 240 min nie");
  // E-20: dokładnie 10% czasu terapii
  reguly::ustawAtrapeMetrykiE(atrapaM08);
  h = spokojne(20);
  for (const Noc& n : h) M08[n.data] = 0;
  Noc& n = dopisz(h);
  n.m01 = 250;
  M08[n.data] = 25;
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-20"), "E-20: 25 min z 250 = 10%");
  M08[n.data] = 24;
  SPRAWDZ(niespelniona(ocena(h, ostatni(h)), "E-20"), "E-20: 24 min z 250 < 10%");
  // E-23 wyklucza linię E-20
  M08[n.data] = 60;
  h.back().wp95 = 28.0f;
  w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-23") && !aktywna(w, "E-20"), "E-23 wyklucza linię E-20");
  reguly::ustawAtrapeMetrykiE(nullptr);
  // tryb nieznany: każda reguła „nie oceniam”
  h = spokojne(30);
  dopisz(h).ahi = 9;
  w = ocena(h, ostatni(h), Tryb::NIEZNANY);
  e = znajdz(w, "E-30");
  SPRAWDZ(e && !e->oceniona, "E-30: tryb nieznany → nie oceniam");
}

// ---------- trzyma: teksty E bez wartości ustawień i drogi ich zmiany (ADR-0002) ----------
static void testADR0002() {
  const char* zakazane[] = {"ustaw", "zmień", "Zmień", "obniż", "podnieś", "Obniż", "Podnieś", "menu", "cmH2O",
                            "EPR", "minimum", "maksimum", "AirBridge"};
  int zle = 0;
  for (const auto& kv : reguly::zestawWbudowany().teksty) {
    if (kv.first.rfind("E-", 0) != 0) continue;
    for (const char* z : zakazane)
      if (kv.second.tekst.find(z) != std::string::npos) {
        zle++;
        std::printf("  ADR-0002? %s: %s\n", kv.first.c_str(), kv.second.tekst.c_str());
      }
  }
  SPRAWDZ(zle == 0, "teksty E bez słów o zmianie ustawień");
}

int main() {
  reguly::zarejestrujRodzajeE();
  testR01Tryby();
  testR02Niepewne();
  testR03WilgotnoscPrzyE22();
  testR04Bateria();
  testR05E10Wyjscie();
  testR06E10BezTerapii();
  testE36Zamrozenie();
  testR07TrybSpoza();
  testGranice();
  testADR0002();
  std::printf("testy_zdarzenia_e_brzegowe: %d sprawdzeń, %d błędów\n", sprawdzen, bledy);
  return bledy == 0 ? 0 : 1;
}
