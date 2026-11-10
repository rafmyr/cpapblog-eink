// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: zdarzenia E w katalogu reguł (docs/spec/katalog/E.csv, teksty.csv) i rodzaje spoza rdzenia
// (src/rodzaje_e.cpp). Testy przed kodem: scenariusze na syntetycznej historii nocy, ze spec funkcjonalnej 6.4,
// 7.2 i 4.11, przez zestaw wbudowany (reguly::zestawWbudowany()) i ocenę silnika (reguly::ocen()).
// Dla każdego aktywnego E: spełnione, niespełnione, dokładnie na progu, za mało nocy albo brak normy → „nie oceniam”,
// wykluczenia z 6.4 i 7.2. Dane wyłącznie syntetyczne.
//
// Ustalenia: zdarzenie wykluczone przez inne jest „nieaktywne” (brak wyniku albo spelniona = false);
// „nie oceniam” = wynik jest i ma oceniona = false. Odpowiedzi::przerwaPlanowana trzyma daty nocy przerwy planowanej.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_zdarzenia_e).
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
void zarejestrujRodzajeE();  // src/rodzaje_e.cpp
using AtrapaMetryki = double (*)(const Noc&, const std::string&);
void ustawAtrapeMetrykiE(AtrapaMetryki f);
}

// Pole m17 (M-17) dokłada (kontrakt 3); do jej scalenia ustawienie jest pomijane.
template <class T>
auto ustawM17(T& n, int v, int) -> decltype(n.m17 = v, void()) {
  n.m17 = v;
}
template <class T>
void ustawM17(T&, int, long) {}

// Atrapa M-08 (minuty dużego wycieku) do scalenia pola `m08` z data nocy → minuty.
static std::map<std::string, double> M08;
static double atrapaM08(const Noc& n, const std::string& id) {
  if (id != "M-08") return NAN;
  auto it = M08.find(n.data);
  return it == M08.end() ? NAN : it->second;
}
static int pominiete = 0;

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

static const char* START = "2026-08-01";
static std::string dzien(int i) { return dodajDni(START, i); }

// Noc spokojna: ważna, 7 h z maską, AHI 1, wyciek P95 12 L/min (w wariancie C noc z P95 ≥ 15 jest niepewna), EPAP P95 10 cmH2O, CA 1 / h, bez przerw, ASVAuto.
static Noc baza(int i) {
  Noc n;
  n.data = dzien(i);
  n.wazna = true;
  n.m01 = 420;
  n.ahi = 1.0f;
  n.cmed = 12.0f;
  n.cp95 = 14.0f;
  n.wmed = 5.0f;
  n.wp95 = 12.0f;
  n.emed = 8.0f;
  n.ep95 = 10.0f;
  n.psmed = 4.0f;
  n.psp95 = 6.0f;
  n.ustawienia = "ASVAuto/6/10/3/15";
  n.oa = 0.5f;
  n.ca = 1.0f;
  n.h = 0.5f;
  n.csr = 0.0f;
  ustawM17(n, 0, 0);
  return n;
}
static std::vector<Noc> spokojne(int ile) {
  std::vector<Noc> h;
  for (int i = 0; i < ile; i++) h.push_back(baza(i));
  return h;
}
// Dopisuje noc po ostatniej w historii; zwraca referencję do zmiany pól.
static Noc& dopisz(std::vector<Noc>& h) {
  h.push_back(baza(h.empty() ? 0 : dniMiedzy(START, h.back().data) + 1));
  return h.back();
}
static Noc& dopiszBezTerapii(std::vector<Noc>& h, int m01) {
  Noc& n = dopisz(h);
  n.m01 = m01;
  n.wazna = m01 >= 240;
  return n;
}

static const reguly::Odpowiedzi BEZ_ODPOWIEDZI;
static const reguly::Pamiec BEZ_PAMIECI;

static std::vector<Wynik> ocena(const std::vector<Noc>& h, const std::string& dzis, Tryb tryb = Tryb::ASVAUTO,
                                const reguly::Odpowiedzi* odp = nullptr, const reguly::Sygnaly* syg = nullptr,
                                const reguly::Pamiec* pam = nullptr) {
  Stany s = liczStany(h, dzis, {});
  reguly::Kontekst k;
  k.historia = &h;
  k.dzis = dzis;
  k.norma = &s.norma;
  k.stany = &s;
  k.trybBiezacy = tryb;
  k.odpowiedzi = odp ? odp : &BEZ_ODPOWIEDZI;
  k.pamiec = pam ? pam : &BEZ_PAMIECI;
  k.zestaw = &reguly::zestawWbudowany();
  reguly::OpcjeOceny o;
  o.sygnaly = syg;
  return reguly::ocen(*k.zestaw, k, o);
}
static std::string ostatni(const std::vector<Noc>& h) { return h.back().data; }

static const Wynik* znajdz(const std::vector<Wynik>& v, const std::string& id, const std::string& wariant) {
  for (const Wynik& w : v)
    if (w.id == id && w.wariant == wariant) return &w;
  return nullptr;
}
static bool aktywna(const std::vector<Wynik>& v, const std::string& id, const std::string& wariant = "") {
  const Wynik* w = znajdz(v, id, wariant);
  return w != nullptr && w->oceniona && w->spelniona;
}
static bool ocenionaNiespelniona(const std::vector<Wynik>& v, const std::string& id, const std::string& wariant = "") {
  const Wynik* w = znajdz(v, id, wariant);
  return w != nullptr && w->oceniona && !w->spelniona;
}
// Stan odchylenia metryki, którego src/stany.* jeszcze nie liczy (M-17, M-03ca): test pominięty
// z komunikatem, dopóki silnik zwraca „nie oceniam” z powodem „nie liczony”; gdy stan jest liczony, test działa sam.
static bool czekaNaStan(const std::vector<Wynik>& v, const std::string& id, const std::string& wariant = "") {
  for (const Wynik& w : v)
    if (w.id == id && w.wariant == wariant && !w.oceniona && w.powodNieoceny.find("nie liczony") != std::string::npos) {
      pominiete++;
      std::printf("POMINIĘTE %s%s%s: %s (czeka na stan w rekordzie nocy)\n", id.c_str(), wariant.empty() ? "" : "/",
                  wariant.c_str(), w.powodNieoceny.c_str());
      return true;
    }
  return false;
}
static bool nieOceniam(const std::vector<Wynik>& v, const std::string& id, const std::string& wariant = "") {
  const Wynik* w = znajdz(v, id, wariant);
  return w != nullptr && !w->oceniona && !w->spelniona;
}

static const reguly::WierszKatalogu* wiersz(const std::string& id, const std::string& wariant) {
  for (const auto& w : reguly::zestawWbudowany().katalog)
    if (w.id == id && w.wariant == wariant) return &w;
  return nullptr;
}
static bool wyklucza(const std::string& id, const std::string& wariant, const std::string& cel) {
  const reguly::WierszKatalogu* w = wiersz(id, wariant);
  if (w == nullptr) return false;
  for (const auto& x : w->wyklucza)
    if (x == cel) return true;
  return false;
}

// ---------- katalog ----------
static void testKatalog() {
  const reguly::Zestaw& z = reguly::zestawWbudowany();
  const std::vector<std::pair<std::string, std::string>> aktywne = {
      {"E-05", "krytyczne"}, {"E-05", "linia"},  {"E-04", ""},          {"E-07", ""},        {"E-08", ""},
      {"E-30", ""},          {"E-31", "lekarz"}, {"E-31", "obserwuj"}, {"E-33", "mediana"}, {"E-33", ""},
      {"E-36", "lekarz"},    {"E-36", "obserwuj"}, {"E-23", ""},       {"E-23", "bez_sk10_kilka"}, {"E-22", ""},        {"E-21", ""},
      {"E-21", "wilgotnosc"}, {"E-10", "problem"}, {"E-12", ""},       {"E-13", ""},        {"E-14", ""},
      {"E-10", "linia"},     {"E-11", ""},       {"E-20", ""}};
  const std::vector<std::pair<std::string, std::string>> zablokowane = {
      {"E-01", ""}, {"E-03", ""}, {"E-37", "gi"}, {"E-37", "bilans"}};
  for (const auto& p : aktywne) {
    const reguly::WierszKatalogu* w = wiersz(p.first, p.second);
    SPRAWDZ(w != nullptr && w->stan == reguly::StanReguly::AKTYWNA, ("aktywny wiersz " + p.first + "/" + p.second).c_str());
    if (w == nullptr) continue;
    SPRAWDZ(w->pilnosc == reguly::Pilnosc::BRAK, ("E bez pilności " + p.first).c_str());
    if (!w->poziom.empty())
      SPRAWDZ(!w->teksty.empty(), ("aktywne E z poziomem ma teksty " + p.first + "/" + p.second).c_str());
    for (const auto& t : w->teksty) SPRAWDZ(z.teksty.count(t) == 1, ("tekst istnieje " + t).c_str());
  }
  for (const auto& p : zablokowane) {
    const reguly::WierszKatalogu* w = wiersz(p.first, p.second);
    SPRAWDZ(w != nullptr && w->stan == reguly::StanReguly::ZABLOKOWANA,
            ("zablokowany wiersz " + p.first + "/" + p.second).c_str());
  }
  // wykluczenia 6.4 i 7.2
  SPRAWDZ(wyklucza("E-30", "", "E-31"), "E-30 wyklucza E-31");
  // wariant jednonocny S-K-10 to w SK.csv wariant pusty (`S-K-10`); przy `kilka` E-23 nie wchodzi wcale
  // (sk::zastosujSk10Kilka), więc wykluczenie całego S-K-10 przez spełnione E-23 = wykluczenie jednonocnego
  SPRAWDZ(wyklucza("E-23", "", "S-K-10") && !wyklucza("E-23", "", "S-K-10/kilka") &&
              wyklucza("E-23", "", "S-K-14") && wyklucza("E-23", "", "E-20"),
          "E-23 wyklucza S-K-10 (wariant jednonocny), S-K-14 i linię E-20");
  SPRAWDZ(!wyklucza("E-23", "", "S-K-11"), "E-23 nie wyklucza S-K-11 (6.4, 7.2 R5b)");
  SPRAWDZ(wyklucza("E-10", "problem", "E-13") && wyklucza("E-10", "problem", "E-14"), "E-10 problem wyklucza E-13, E-14");
  SPRAWDZ(wyklucza("E-21", "", "S-K-14"), "E-21 wyklucza S-K-14");
  SPRAWDZ(wyklucza("E-22", "", "E-21"), "E-22 wyklucza E-21");
  SPRAWDZ(wyklucza("E-14", "", "S-K-16"), "E-14 wyklucza S-K-16");
  // teksty: liczba progu przez {prog:…}, granica ADR-0002 (zero wartości ustawień, drogi do zmiany, diagnozy)
  Wynik pusty;
  SPRAWDZ(reguly::tekst(z, "E-30.tytul", pusty, Tryb::ASVAUTO) == "Noce z AHI powyżej 5", "tytuł E-30 z progiem");
  SPRAWDZ(reguly::tekst(z, "E-23.tytul", pusty, Tryb::ASVAUTO) == "Wyciek w nocy ponad próg", "tytuł E-23 bez liczby");
  // powód z wartością nocy i progiem (bez wyniku `{x}` = `–`, panel nie wymyśla liczby)
  SPRAWDZ(reguly::tekst(z, "E-23.powod", pusty, Tryb::ASVAUTO) == "– l/min lub więcej przez 5% czasu terapii, próg 27.",
          "powód E-23 bez wartości");
  Wynik e23;
  e23.id = "E-23";
  e23.zmienne["x"] = "38,2";
  SPRAWDZ(reguly::tekst(z, "E-23.powod", e23, Tryb::ASVAUTO) == "38,2 l/min lub więcej przez 5% czasu terapii, próg 27.",
          "powód E-23 z wartością nocy");
  const char* zakazane[] = {"cmH2O", "EPR", "Ustaw", "ustaw", "Obniż", "Podnieś", "Zwiększ", "Zmniejsz", "menu",
                            "choroba", "diagnoz", "bezdech senny"};
  for (const auto& t : z.teksty) {
    if (t.first.rfind("E-", 0) != 0) continue;
    for (const char* s : zakazane)
      SPRAWDZ(t.second.tekst.find(s) == std::string::npos, ("ADR-0002 w tekście " + t.first).c_str());
  }
}

// ---------- E-04 noc częściowa ----------
static void testE04() {
  auto h = spokojne(20);
  Noc& n = dopisz(h);
  n.czesciowa = true;
  n.wazna = false;
  reguly::Sygnaly po11;
  po11.stan["po_11"] = true;
  auto w = ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &po11);
  SPRAWDZ(aktywna(w, "E-04"), "E-04: noc częściowa o 11:00");
  const Wynik* e = znajdz(w, "E-04", "");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-04.linia", *e, Tryb::ASVAUTO) ==
                              "Noc " + ddmm(ostatni(h)) + " zapisana częściowo, liczby mogą być niepełne.",
          "E-04: linia z datą");
  reguly::Sygnaly przed11;
  przed11.stan["po_11"] = false;
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &przed11), "E-04"),
          "E-04: przed 11:00 raport w budowie, bez linii");
  SPRAWDZ(nieOceniam(ocena(h, ostatni(h)), "E-04"), "E-04: bez sygnału godziny → nie oceniam");
  h.back().czesciowa = false;
  h.back().wazna = true;
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &po11), "E-04"), "E-04: noc kompletna");
  SPRAWDZ(nieOceniam(ocena(h, dodajDni(ostatni(h), 1), Tryb::ASVAUTO, nullptr, &po11), "E-04"),
          "E-04: brak nocy dziś → nie oceniam");
}

// ---------- E-10 noc bez terapii ----------
static void testE10() {
  auto h = spokojne(20);
  dopiszBezTerapii(h, 10);
  auto w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-10", "linia"), "E-10 linia: M-01 10 min");
  SPRAWDZ(ocenionaNiespelniona(w, "E-10", "problem"), "E-10 problem: jedna noc to za mało");
  h.back().m01 = 15;  // próg E-10.noc_bez_terapii_m01 = 15 min, warunek „poniżej”
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h)), "E-10", "linia"), "E-10 linia: dokładnie 15 min to noc z terapią");
  SPRAWDZ(nieOceniam(ocena(h, dodajDni(ostatni(h), 1)), "E-10", "linia"), "E-10 linia: brak nocy → nie oceniam");
  h.back().m01 = 10;
  dopiszBezTerapii(h, 0);
  w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-10", "problem"), "E-10 problem: 2 kolejne noce bez terapii");
  SPRAWDZ(!aktywna(w, "E-10", "linia"), "E-10 problem wyklucza linię E-10");
  std::vector<Noc> jedna = {baza(0)};
  jedna[0].m01 = 10;
  jedna[0].wazna = false;
  SPRAWDZ(nieOceniam(ocena(jedna, jedna[0].data), "E-10", "problem"), "E-10 problem: 1 noc w historii → nie oceniam");
  // E-10 „problem” wyklucza E-13 (stan odchylenia M-01 trwa)
  auto h2 = spokojne(30);
  for (int i = 0; i < 3; i++) dopisz(h2).m01 = 300;
  SPRAWDZ(aktywna(ocena(h2, ostatni(h2)), "E-13"), "E-13 przed nocami bez terapii");
  dopiszBezTerapii(h2, 10);
  dopiszBezTerapii(h2, 10);
  w = ocena(h2, ostatni(h2));
  SPRAWDZ(aktywna(w, "E-10", "problem") && !aktywna(w, "E-13"), "E-10 problem wyklucza E-13");
}

// ---------- E-11 krótka noc ----------
static void testE11() {
  auto h = spokojne(20);
  dopiszBezTerapii(h, 200);
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-11"), "E-11: 3 h 20");
  const Wynik* e = nullptr;
  auto w = ocena(h, ostatni(h));
  e = znajdz(w, "E-11", "");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-11.linia", *e, Tryb::ASVAUTO) ==
                              "Krótka noc: " + godzH(200) + ". Liczby mniej pewne.",
          "E-11: linia z czasem");
  h.back().m01 = 240;
  h.back().wazna = true;
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h)), "E-11"), "E-11: dokładnie 4 h to nie krótka noc");
  h.back().m01 = 15;
  h.back().wazna = false;
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-11"), "E-11: dokładnie 15 min to krótka noc");
  h.back().m01 = 14;
  w = ocena(h, ostatni(h));
  SPRAWDZ(ocenionaNiespelniona(w, "E-11") && aktywna(w, "E-10", "linia"), "E-11: 14 min to noc bez terapii (E-10)");
  SPRAWDZ(nieOceniam(ocena(h, dodajDni(ostatni(h), 1)), "E-11"), "E-11: brak nocy → nie oceniam");
  h.back().m01 = 200;
  h.back().czesciowa = true;
  SPRAWDZ(nieOceniam(ocena(h, ostatni(h)), "E-11"), "E-11: noc częściowa → nie oceniam");
}

// ---------- E-12 terapia nieregularna ----------
static std::vector<Noc> regularnosc(int dni, const std::set<int>& krotkie, const std::set<int>& bezRekordu = {}) {
  std::vector<Noc> h;
  for (int i = 0; i < dni; i++) {
    if (bezRekordu.count(i)) continue;
    Noc n = baza(i);
    if (krotkie.count(i)) {
      n.m01 = 180;
      n.wazna = false;
    }
    h.push_back(n);
  }
  return h;
}
static void testE12() {
  // 20 dni historii, 13 nocy ≥ 4 h: 65% < 70%
  auto h = regularnosc(20, {13, 14, 15, 16, 17, 18, 19});
  auto w = ocena(h, dzien(19));
  SPRAWDZ(aktywna(w, "E-12"), "E-12: 13 z 20 nocy");
  const Wynik* e = znajdz(w, "E-12", "");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-12.powod", *e, Tryb::ASVAUTO) ==
                              "Ponad 4 h tylko w 13 z 20 nocy, cel: 14.",
          "E-12: powód z n, d i celem");
  // dokładnie 70%: 14 z 20 (krótkie noce na końcu, więc wcześniejsze dni też ≥ 70%)
  SPRAWDZ(ocenionaNiespelniona(ocena(regularnosc(20, {14, 15, 16, 17, 18, 19}), dzien(19)), "E-12"),
          "E-12: dokładnie 70% to nie nieregularność");
  SPRAWDZ(nieOceniam(ocena(regularnosc(13, {10, 11, 12}), dzien(12)), "E-12"), "E-12: 13 dni historii → nie oceniam");
  // wyjście dopiero po 3 kolejnych dniach z odsetkiem ≥ 70% (okno 30 dni)
  std::set<int> krotkie;
  for (int i = 9; i <= 18; i++) krotkie.insert(i);
  auto h3 = regularnosc(42, krotkie);
  SPRAWDZ(aktywna(ocena(h3, dzien(38)), "E-12"), "E-12: 20 z 30 nocy");
  SPRAWDZ(aktywna(ocena(h3, dzien(39)), "E-12"), "E-12: 1. dzień ≥ 70% → trwa");
  SPRAWDZ(aktywna(ocena(h3, dzien(40)), "E-12"), "E-12: 2. dzień ≥ 70% → trwa");
  SPRAWDZ(ocenionaNiespelniona(ocena(h3, dzien(41)), "E-12"), "E-12: 3. dzień ≥ 70% → znika");
  // przerwa planowana (R-SKI-35): noce przerwy poza d i poza licznikiem
  std::set<int> przerwa = {13, 14, 15, 16, 17, 18, 19};
  auto h4 = regularnosc(21, {}, przerwa);
  SPRAWDZ(aktywna(ocena(h4, dzien(20)), "E-12"), "E-12: przerwa bez odpowiedzi to brak terapii");
  reguly::Odpowiedzi odp;
  for (int i : przerwa) odp.przerwaPlanowana.insert(dzien(i));
  SPRAWDZ(ocenionaNiespelniona(ocena(h4, dzien(20), Tryb::ASVAUTO, &odp), "E-12"),
          "E-12: przerwa planowana nie psuje regularności");
}

// ---------- stany odchylenia: E-13, E-14, E-21, E-31, E-33 ----------
static void testE13() {
  auto h = spokojne(30);
  for (int i = 0; i < 3; i++) dopisz(h).m01 = 300;
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-13"), "E-13: stan odchylenia M-01");
  auto h2 = spokojne(30);
  for (int i = 0; i < 3; i++) dopisz(h2).m01 = 330;  // Δ −1,5 h dokładnie
  SPRAWDZ(aktywna(ocena(h2, ostatni(h2)), "E-13"), "E-13: dokładnie −90 min");
  auto h3 = spokojne(30);
  for (int i = 0; i < 3; i++) dopisz(h3).m01 = 331;
  SPRAWDZ(ocenionaNiespelniona(ocena(h3, ostatni(h3)), "E-13"), "E-13: −89 min to nie odchylenie");
  auto h4 = spokojne(10);
  for (int i = 0; i < 3; i++) dopisz(h4).m01 = 300;
  SPRAWDZ(nieOceniam(ocena(h4, ostatni(h4)), "E-13"), "E-13: bez normy → nie oceniam");
}
static void testE14() {
  auto h = spokojne(30);
  for (int i = 0; i < 3; i++) ustawM17(dopisz(h), 3, 0);
  if (czekaNaStan(ocena(h, ostatni(h)), "E-14")) return;
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-14"), "E-14: stan odchylenia M-17");
  auto h2 = spokojne(30);
  for (int i = 0; i < 3; i++) ustawM17(dopisz(h2), 2, 0);  // Δ +2 dokładnie
  SPRAWDZ(aktywna(ocena(h2, ostatni(h2)), "E-14"), "E-14: dokładnie +2 przerwy");
  auto h3 = spokojne(30);
  for (int i = 0; i < 3; i++) ustawM17(dopisz(h3), 1, 0);
  SPRAWDZ(ocenionaNiespelniona(ocena(h3, ostatni(h3)), "E-14"), "E-14: +1 przerwa to nie odchylenie");
  auto h4 = spokojne(10);
  for (int i = 0; i < 3; i++) ustawM17(dopisz(h4), 3, 0);
  SPRAWDZ(nieOceniam(ocena(h4, ostatni(h4)), "E-14"), "E-14: bez normy → nie oceniam");
}
static void testE21() {
  auto h = spokojne(30);
  for (int i = 0; i < 3; i++) dopisz(h).wp95 = 22.0f;  // Δ +10 L/min dokładnie
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-21"), "E-21: wyciek P95 +10 L/min");
  auto h2 = spokojne(30);
  for (int i = 0; i < 3; i++) dopisz(h2).wp95 = 21.5f;
  SPRAWDZ(ocenionaNiespelniona(ocena(h2, ostatni(h2)), "E-21"), "E-21: +9,5 L/min to nie odchylenie");
  auto h3 = spokojne(10);
  for (int i = 0; i < 3; i++) dopisz(h3).wp95 = 35.0f;
  SPRAWDZ(nieOceniam(ocena(h3, ostatni(h3)), "E-21"), "E-21: bez normy → nie oceniam");
}
static void testE31() {
  auto h = spokojne(30);
  for (int i = 0; i < 3; i++) dopisz(h).ahi = 4.5f;
  auto w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-31", "obserwuj") && !aktywna(w, "E-31", "lekarz"), "E-31: stan M-02, E-30 nieaktywne");
  auto h2 = spokojne(30);
  for (int i = 0; i < 3; i++) dopisz(h2).ahi = 4.0f;  // Δ +3 / h dokładnie
  SPRAWDZ(aktywna(ocena(h2, ostatni(h2)), "E-31", "obserwuj"), "E-31: dokładnie +3 / h");
  auto h3 = spokojne(30);
  for (int i = 0; i < 3; i++) dopisz(h3).ahi = 3.9f;
  SPRAWDZ(ocenionaNiespelniona(ocena(h3, ostatni(h3)), "E-31", "obserwuj"), "E-31: +2,9 / h to nie odchylenie");
  auto h4 = spokojne(10);
  for (int i = 0; i < 3; i++) dopisz(h4).ahi = 4.5f;
  SPRAWDZ(nieOceniam(ocena(h4, ostatni(h4)), "E-31", "obserwuj"), "E-31: bez normy → nie oceniam");
  // krok do lekarza od rana po 21. nocy stanu (R-NOR-23): stan wchodzi w 3. nocy odchylenia
  auto h5 = spokojne(30);
  for (int i = 0; i < 22; i++) dopisz(h5).ahi = 4.5f;  // 20 nocy stanu
  w = ocena(h5, ostatni(h5));
  SPRAWDZ(aktywna(w, "E-31", "obserwuj") && !aktywna(w, "E-31", "lekarz"), "E-31: 20 nocy stanu → obserwuj");
  dopisz(h5).ahi = 4.5f;  // 21. noc stanu
  w = ocena(h5, ostatni(h5));
  SPRAWDZ(aktywna(w, "E-31", "lekarz"), "E-31: 21. noc stanu → krok do lekarza");
  SPRAWDZ(!aktywna(w, "E-31", "obserwuj"), "E-31 lekarz wyklucza wariant obserwuj");
}
static void testE33() {
  auto h = spokojne(30);
  for (int i = 0; i < 5; i++) dopisz(h).ca = 3.5f;
  auto w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-33", "mediana"), "E-33: mediana CA/h 3,5");
  if (czekaNaStan(w, "E-33")) return;
  SPRAWDZ(aktywna(w, "E-33"), "E-33: stan M-03 i mediana CA/h 3,5");
  auto h2 = spokojne(30);
  for (int i = 0; i < 5; i++) dopisz(h2).ca = 3.0f;  // mediana dokładnie 3 / h
  SPRAWDZ(aktywna(ocena(h2, ostatni(h2)), "E-33"), "E-33: mediana CA/h dokładnie 3");
  auto h3 = spokojne(30);
  for (int i = 0; i < 5; i++) dopisz(h3).ca = 2.5f;  // stan odchylenia jest, mediana poniżej progu
  w = ocena(h3, ostatni(h3));
  SPRAWDZ(ocenionaNiespelniona(w, "E-33", "mediana") && !aktywna(w, "E-33"), "E-33: mediana 2,5 → nieaktywne");
  std::vector<Noc> h4;
  for (int i = 0; i < 4; i++) {
    h4.push_back(baza(i));
    h4.back().ca = 4.0f;
  }
  SPRAWDZ(nieOceniam(ocena(h4, ostatni(h4)), "E-33", "mediana"), "E-33: 4 noce → nie oceniam mediany");
  SPRAWDZ(!aktywna(ocena(h4, ostatni(h4)), "E-33"), "E-33: bez normy nieaktywne");
}

// ---------- E-23 bardzo duży wyciek ----------
static void testE23() {
  auto h = spokojne(20);
  dopisz(h).wp95 = 27.0f;  // próg E-23.przeciek_p95, warunek „co najmniej”
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-23"), "E-23: dokładnie 27 L/min");
  h.back().wp95 = 26.9f;
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h)), "E-23"), "E-23: 26,9 L/min");
  h.back().wp95 = 40.0f;
  h.back().m01 = 10;
  h.back().wazna = false;
  SPRAWDZ(nieOceniam(ocena(h, ostatni(h)), "E-23"), "E-23: noc bez terapii → nie oceniam");
  SPRAWDZ(nieOceniam(ocena(h, dodajDni(ostatni(h), 1)), "E-23"), "E-23: brak nocy → nie oceniam");
}

// ---------- E-30 mediana AHI z 7 nocy ----------
static std::vector<Noc> ahi7(const std::vector<float>& ostatnie) {
  auto h = spokojne(30);
  for (float a : ostatnie) dopisz(h).ahi = a;
  return h;
}
static void testE30() {
  auto h = ahi7({1, 1, 1, 6, 6, 6, 6});
  auto w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-30"), "E-30: mediana 6");
  SPRAWDZ(!aktywna(w, "E-31", "obserwuj") && !aktywna(w, "E-31", "lekarz"), "E-30 wyklucza E-31");
  h = ahi7({1, 1, 1, 5, 5, 5, 5});
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-30"), "E-30: mediana dokładnie 5");
  h = ahi7({1, 1, 1, 4.9f, 4.9f, 4.9f, 4.9f});
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h)), "E-30"), "E-30: mediana 4,9");
  std::vector<Noc> krotka;
  for (int i = 0; i < 6; i++) {
    krotka.push_back(baza(i));
    krotka.back().ahi = 8.0f;
  }
  SPRAWDZ(nieOceniam(ocena(krotka, ostatni(krotka)), "E-30"), "E-30: 6 nocy ważnych → nie oceniam");
  // noce niepewne (mediana wycieku > 30 L/min, 4.9) nie liczą się do okna (R-NOR-05)
  h = spokojne(30);
  for (int i = 0; i < 4; i++) {
    Noc& n = dopisz(h);
    n.ahi = 8.0f;
    n.wmed = 31.0f;
  }
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h)), "E-30"), "E-30: noce niepewne poza oknem");
  // tryb spoza: nie oceniam
  h = ahi7({1, 1, 1, 6, 6, 6, 6});
  for (Noc& n : h) n.ustawienia = "";
  SPRAWDZ(nieOceniam(ocena(h, ostatni(h), Tryb::NIEZNANY), "E-30"), "E-30: tryb nieznany → nie oceniam");
}

// ---------- E-36 EPAP P95 w 5 z 7 nocy (tryb ASV, 4.11) ----------
static std::vector<Noc> ep(int spokojnych, const std::vector<float>& ostatnie) {
  auto h = spokojne(spokojnych);
  for (float v : ostatnie) dopisz(h).ep95 = v;
  return h;
}
static void testE36() {
  auto h = ep(30, {12.5f, 12.5f, 12.5f, 12.5f, 12.5f, 12.5f, 12.5f});
  auto w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-36", "obserwuj"), "E-36: 7 z 7 nocy z odchyleniem M-06p");
  SPRAWDZ(!aktywna(w, "E-36", "lekarz"), "E-36: krótki stan bez kroku do lekarza");
  const Wynik* e = znajdz(w, "E-36", "obserwuj");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-36.tytul", *e, Tryb::ASVAUTO) ==
                              "EPAP wyższe niż zwykle",
          "E-36: tytuł w trybie ASV (4.11)");
  h = ep(30, {10, 10, 12, 12, 12, 12, 12});  // Δ +2 dokładnie w 5 z 7
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-36", "obserwuj"), "E-36: dokładnie 5 z 7 i Δ 2");
  h = ep(30, {10, 10, 10, 12.5f, 12.5f, 12.5f, 12.5f});
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h)), "E-36", "obserwuj"), "E-36: 4 z 7");
  h = ep(30, {11.9f, 11.9f, 11.9f, 11.9f, 11.9f, 11.9f, 11.9f});
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h)), "E-36", "obserwuj"), "E-36: Δ 1,9 to nie odchylenie");
  h = ep(5, {12.5f, 12.5f, 12.5f, 12.5f, 12.5f, 12.5f, 12.5f});  // 12 nocy: norma M-06p wymaga 14
  SPRAWDZ(nieOceniam(ocena(h, ostatni(h)), "E-36", "obserwuj"), "E-36: bez normy → nie oceniam");
  std::vector<Noc> krotka;
  for (int i = 0; i < 6; i++) krotka.push_back(baza(i));
  SPRAWDZ(nieOceniam(ocena(krotka, ostatni(krotka)), "E-36", "obserwuj"), "E-36: 6 nocy → nie oceniam");
  // krok do lekarza od rana po 21. nocy stanu odchylenia M-06p (R-NOR-23)
  std::vector<float> dlugo(23, 12.5f);  // stan od 3. nocy: 21 nocy stanu
  h = ep(30, dlugo);
  w = ocena(h, ostatni(h));
  SPRAWDZ(aktywna(w, "E-36", "lekarz") && !aktywna(w, "E-36", "obserwuj"), "E-36: 21. noc stanu → krok do lekarza");
  dlugo.pop_back();
  h = ep(30, dlugo);
  w = ocena(h, ostatni(h));
  SPRAWDZ(!aktywna(w, "E-36", "lekarz") && aktywna(w, "E-36", "obserwuj"), "E-36: 20 nocy stanu → obserwuj");
}

// ---------- E-05 bateria (odczyt M-25 komponentu baterii) ----------
static void testE05() {
  auto h = spokojne(20);
  reguly::Sygnaly s;
  s.odczyty["M-25"] = 15;  // próg E-05.bateria_linia, „przy tym poziomie i niżej”
  auto w = ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &s);
  SPRAWDZ(aktywna(w, "E-05", "linia") && !aktywna(w, "E-05", "krytyczne"), "E-05: 15% → linia");
  const Wynik* e = znajdz(w, "E-05", "linia");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-05.linia", *e, Tryb::ASVAUTO) ==
                              "Ekran: bateria 15%, naładuj w tym tygodniu.",
          "E-05: tekst linii");
  s.odczyty["M-25"] = 16;
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &s), "E-05", "linia"), "E-05: 16% bez linii");
  s.odczyty["M-25"] = 5;
  w = ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &s);
  SPRAWDZ(aktywna(w, "E-05", "krytyczne") && !aktywna(w, "E-05", "linia"), "E-05: 5% → krytyczne wyklucza linię");
  reguly::Sygnaly bez;
  w = ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &bez);
  SPRAWDZ(nieOceniam(w, "E-05", "linia") && nieOceniam(w, "E-05", "krytyczne"), "E-05: bez odczytu → nie oceniam");
  // znika dopiero powyżej 20% (E-05.bateria_koniec)
  reguly::Pamiec p;
  p["E-05/linia"].aktywnaOd = dzien(10);
  s.odczyty["M-25"] = 20;
  SPRAWDZ(aktywna(ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &s, &p), "E-05", "linia"), "E-05: 20% po linii → trwa");
  s.odczyty["M-25"] = 21;
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &s, &p), "E-05", "linia"),
          "E-05: 21% → znika");
  reguly::Pamiec pk;
  pk["E-05/krytyczne"].aktywnaOd = dzien(10);
  s.odczyty["M-25"] = 6;
  w = ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &s, &pk);
  SPRAWDZ(!aktywna(w, "E-05", "krytyczne") && aktywna(w, "E-05", "linia"), "E-05: 6% po krytycznym → linia (6.4)");
}

// ---------- E-07, E-08 sygnały komponentów ----------
static void testE07E08() {
  auto h = spokojne(20);
  reguly::Sygnaly s;
  s.stan["czujnik_sypialni_milczy"] = true;
  s.stan["co2_milczy"] = true;
  s.zmienne["czas"] = "13:00";
  auto w = ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &s);
  SPRAWDZ(aktywna(w, "E-07") && aktywna(w, "E-08"), "E-07, E-08: sygnał komponentu");
  const Wynik* e = znajdz(w, "E-07", "");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-07.linia", *e, Tryb::ASVAUTO) ==
                              "Czujnik sypialni nie odpowiada od 3 nocy.",
          "E-07: liczba nocy z progu");
  e = znajdz(w, "E-08", "");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-08.linia", *e, Tryb::ASVAUTO) ==
                              "Brak odczytu CO2 od 13:00.",
          "E-08: linia z czasem");
  s.stan["co2_milczy"] = false;
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &s), "E-08"), "E-08: CO2 odpowiada");
  reguly::Sygnaly bez;
  w = ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &bez);
  SPRAWDZ(nieOceniam(w, "E-07") && nieOceniam(w, "E-08"), "E-07, E-08: komponent milczy → nie oceniam");
}

// ---------- E-10 o 11:00 bez nocy na liście (sygnał noc_bez_rekordu) ----------
static void testE10BezRekordu() {
  auto h = spokojne(20);
  std::string dzis = dodajDni(ostatni(h), 1);
  reguly::Sygnaly s;
  s.stan["noc_bez_rekordu"] = true;
  auto w = ocena(h, dzis, Tryb::ASVAUTO, nullptr, &s);
  SPRAWDZ(aktywna(w, "E-10", "linia") && ocenionaNiespelniona(w, "E-10", "problem"), "E-10: o 11:00 nocy brak → linia");
  s.stan["noc_bez_rekordu"] = false;
  SPRAWDZ(ocenionaNiespelniona(ocena(h, dzis, Tryb::ASVAUTO, nullptr, &s), "E-10", "linia"),
          "E-10: przed 11:00 noc nieobecna, nie E-10");
  h.back().m01 = 10;
  h.back().wazna = false;
  s.stan["noc_bez_rekordu"] = true;
  w = ocena(h, dzis, Tryb::ASVAUTO, nullptr, &s);
  SPRAWDZ(aktywna(w, "E-10", "problem"), "E-10: noc < 15 min i noc bez rekordu → problem");
  const Wynik* e = znajdz(w, "E-10", "problem");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-10.powod", *e, Tryb::ASVAUTO) ==
                              "Ostatnia terapia: " + ddmm(dzien(18)) + ".",
          "E-10: data ostatniej terapii");
}

// ---------- E-20, E-22 duży wyciek (M-08; atrapa do scalenia pola m08) ----------
static void testE20E22() {
  reguly::ustawAtrapeMetrykiE(atrapaM08);
  M08.clear();
  auto h = spokojne(20);
  for (const Noc& n : h) M08[n.data] = 0;
  M08[ostatni(h)] = 30;  // próg E-20.minuty, „co najmniej”
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-20"), "E-20: 30 min");
  M08[ostatni(h)] = 29;  // 29 < 30 min i < 10% z 420 min
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h)), "E-20"), "E-20: 29 min przy 7 h");
  h.back().m01 = 280;  // 10% = 28 min
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-20"), "E-20: 29 min to ≥ 10% z 280 min");
  M08.erase(ostatni(h));
  SPRAWDZ(nieOceniam(ocena(h, ostatni(h)), "E-20"), "E-20: bez M-08 → nie oceniam");
  // E-22: E-20 w 3 z 5 nocy
  auto h2 = spokojne(20);
  for (const Noc& n : h2) M08[n.data] = 0;
  for (double v : {40.0, 40.0, 40.0, 0.0, 0.0}) {
    Noc& n = dopisz(h2);
    M08[n.data] = v;
  }
  auto w = ocena(h2, ostatni(h2));
  SPRAWDZ(aktywna(w, "E-22"), "E-22: 3 z 5 nocy (2 noce bez wycieku po wejściu)");
  const Wynik* e = znajdz(w, "E-22", "");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-22.powod", *e, Tryb::ASVAUTO) ==
                              "Duży wyciek w 3 z 5 nocy, średnio 40 min.",
          "E-22: powód");
  Noc& trzecia = dopisz(h2);
  M08[trzecia.data] = 0;
  SPRAWDZ(ocenionaNiespelniona(ocena(h2, ostatni(h2)), "E-22"), "E-22: 3 kolejne noce bez E-20 → znika");
  auto h3 = spokojne(20);
  for (const Noc& n : h3) M08[n.data] = 0;
  for (double v : {40.0, 40.0, 0.0, 0.0, 0.0}) {
    Noc& n = dopisz(h3);
    M08[n.data] = v;
  }
  SPRAWDZ(ocenionaNiespelniona(ocena(h3, ostatni(h3)), "E-22"), "E-22: 2 z 5 nocy");
  M08.clear();
  std::vector<Noc> h4;
  for (int i = 0; i < 4; i++) {
    h4.push_back(baza(i));
    M08[h4.back().data] = 40;
  }
  SPRAWDZ(nieOceniam(ocena(h4, ostatni(h4)), "E-22"), "E-22: 4 noce → nie oceniam");
  // E-22 wyklucza E-21
  auto h5 = spokojne(30);
  for (const Noc& n : h5) M08[n.data] = 0;
  for (int i = 0; i < 3; i++) {
    Noc& n = dopisz(h5);
    n.wp95 = 35.0f;
    M08[n.data] = 40;
  }
  w = ocena(h5, ostatni(h5));
  SPRAWDZ(aktywna(w, "E-22") && !aktywna(w, "E-21"), "E-22 wyklucza E-21");
  M08.clear();
  reguly::ustawAtrapeMetrykiE(nullptr);
}

// ---------- E-21 linia o suchym powietrzu ----------
static void testE21Wilgotnosc() {
  auto h = spokojne(30);
  for (int i = 0; i < 3; i++) dopisz(h).wp95 = 22.0f;
  reguly::Sygnaly s;
  s.odczyty["wilgotnosc_min_nocy"] = 34;
  auto w = ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &s);
  SPRAWDZ(aktywna(w, "E-21", "wilgotnosc"), "E-21: wilgotność 34% przy E-21");
  const Wynik* e = znajdz(w, "E-21", "wilgotnosc");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-21.linia", *e, Tryb::ASVAUTO) ==
                              "Powietrze w sypialni suche: 34%.",
          "E-21: linia wilgotności");
  s.odczyty["wilgotnosc_min_nocy"] = 35;  // „poniżej 35%”
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h), Tryb::ASVAUTO, nullptr, &s), "E-21", "wilgotnosc"),
          "E-21: dokładnie 35% bez linii");
  auto h2 = spokojne(30);
  s.odczyty["wilgotnosc_min_nocy"] = 20;
  SPRAWDZ(!aktywna(ocena(h2, ostatni(h2), Tryb::ASVAUTO, nullptr, &s), "E-21", "wilgotnosc"),
          "E-21: bez E-21 linii wilgotności nie ma");
}

static void testWarunkiE23E10IOstatniaTerapia() {
  // warunek „E-23 nie wchodzi przy S-K-10 od kilku nocy” do czasu wiersza S-K-10 → „nie oceniam”
  auto h = spokojne(20);
  dopisz(h).wp95 = 30.0f;
  auto w = ocena(h, ostatni(h));
  SPRAWDZ(nieOceniam(w, "E-23", "bez_sk10_kilka") && aktywna(w, "E-23"), "E-23: warunek S-K-10 „nie oceniam”, E-23 liczy się");
  // E-10 „problem” trwa po nocy 2 h, znika po nocy ≥ 4 h
  h = spokojne(20);
  dopiszBezTerapii(h, 0);
  dopiszBezTerapii(h, 0);
  dopiszBezTerapii(h, 120);
  SPRAWDZ(aktywna(ocena(h, ostatni(h)), "E-10", "problem"), "E-10 problem: noc 2 h nie kończy");
  dopiszBezTerapii(h, 240);
  SPRAWDZ(ocenionaNiespelniona(ocena(h, ostatni(h)), "E-10", "problem"), "E-10 problem: noc 4 h kończy");
  // bez nocy z terapią w historii „Ostatnia terapia: –.”
  std::vector<Noc> bez;
  for (int i = 0; i < 3; i++) {
    bez.push_back(baza(i));
    bez.back().m01 = 0;
    bez.back().wazna = false;
  }
  w = ocena(bez, ostatni(bez));
  const Wynik* e = znajdz(w, "E-10", "problem");
  SPRAWDZ(e != nullptr && e->spelniona &&
              reguly::tekst(reguly::zestawWbudowany(), "E-10.powod", *e, Tryb::ASVAUTO) == "Ostatnia terapia: –.",
          "E-10: bez terapii w historii data „–”");
  // E-36 w CPAP na M-05p, tytuł „Ciśnienie wyższe niż zwykle”
  h = spokojne(30);
  for (Noc& n : h) n.ustawienia = "CPAP/9";
  for (int i = 0; i < 7; i++) {
    Noc& n = dopisz(h);
    n.ustawienia = "CPAP/9";
    n.cp95 = 16.5f;
  }
  w = ocena(h, ostatni(h), Tryb::CPAP);
  e = znajdz(w, "E-36", "obserwuj");
  SPRAWDZ(e != nullptr && e->oceniona && e->spelniona, "E-36: CPAP, M-05p +2,5 w 7 z 7");
  SPRAWDZ(e != nullptr && reguly::tekst(reguly::zestawWbudowany(), "E-36.powod", *e, Tryb::CPAP) ==
                              "Ciśnienie w szczytach nocy 16,5, zwykle 14,0.",
          "E-36: powód poza ASV");
  // E-12 noc częściowa poza d (20 dni, 14 nocy ≥ 4 h, 6 częściowych na końcu → 14 z 14)
  auto r = regularnosc(20, {});
  for (int i = 14; i < 20; i++) {
    r[(size_t)i].czesciowa = true;
    r[(size_t)i].wazna = false;
    r[(size_t)i].m01 = 100;
  }
  SPRAWDZ(ocenionaNiespelniona(ocena(r, dzien(19)), "E-12"), "E-12: noce częściowe poza oknem d");
}

int main() {
  reguly::zarejestrujRodzajeE();
  testKatalog();
  testE04();
  testE10();
  testE11();
  testE12();
  testE13();
  testE14();
  testE21();
  testE23();
  testE30();
  testE31();
  testE33();
  testE36();
  testE05();
  testE07E08();
  testE10BezRekordu();
  testE20E22();
  testE21Wilgotnosc();
  testWarunkiE23E10IOstatniaTerapia();
  std::printf("testy_zdarzenia_e: %d sprawdzeń, %d błędów, %d pominiętych (czeka na stan w rekordzie nocy)\n", sprawdzen, bledy, pominiete);
  return bledy == 0 ? 0 : 1;
}

