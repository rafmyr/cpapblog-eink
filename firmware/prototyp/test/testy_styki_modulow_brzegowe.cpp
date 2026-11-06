// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych na styku modułów. Każdy test usterki pada z numerem znaleziska,
// dopóki usterka nie zostanie poprawiona. Testy „trzyma”
// (makro TRZYMA) pilnują rzeczy już sprawdzonych. Dane wyłącznie syntetyczne. Część sprawdzeń (katalog,
// #include, kompilacja na ESP32-S3) wykonuje tools/test_styki_modulow_brzegowe.py.
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "ekrany.h"
#include "kolejka.h"
#include "kontekst_sk.h"
#include "plan.h"
#include "wybudzenie.h"
#include "nawigacja.h"
#include "reguly.h"
#include "sprawy.h"
#include "stany.h"
#include "wspolne.h"

static int ok = 0, zle = 0, oczekuje = 0;
// Po poprawkach znaleziska czekające na decyzję albo na inny moduł (U-01 to konfiguracja z telefonu; ↻ 2 s na
// U-05: pakiet z karty R-NF-30 nie istnieje; „W normie” przy pełnej ocenie: pkt 3 i 5 zostają nieoceniane)
// wypisują się, ale nie blokują bramki.
#define OCZEKUJE(nr, w)                                                                    \
  do {                                                                                     \
    if (w) ok++;                                                                           \
    else { oczekuje++; printf("OCZEKUJE %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); } \
  } while (0)
#define PRZYPADEK(nr, w)                                                                  \
  do {                                                                                     \
    if (w) ok++;                                                                           \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); }    \
  } while (0)
#define TRZYMA(w)                                                                          \
  do {                                                                                     \
    if (w) ok++;                                                                           \
    else { zle++; printf("REGRES (%s:%d): %s\n", __FILE__, __LINE__, #w); }               \
  } while (0)

static bool zawiera(const std::vector<std::string>& v, const std::string& s) {
  for (auto& x : v)
    if (x.find(s) != std::string::npos) return true;
  return false;
}

static reguly::Wynik wynik(const std::string& id, bool oceniona, bool spelniona, const std::string& poziom = "lekarz") {
  reguly::Wynik w;
  w.id = id;
  w.oceniona = oceniona;
  w.spelniona = spelniona;
  w.poziom = poziom;
  if (!oceniona) w.powodNieoceny = "za malo nocy 3/14";
  return w;
}

// „Otwarte sprawy” w stanie pustym mówią `Ocena: pełna`, choć silnik ma reguły
// „nie oceniam” (punkt 7 warunku uspokojenia niespełniony, a pkt 5 `Metryki niewalidowane` panel w ogóle nie liczy).
// Na zrzucie z importem karty (145 z 150 nocy bez M-03, M-08, M-13, M-17) to zwykły stan rano.
static void testR7_03() {
  std::vector<reguly::Wynik> w = {wynik("L-03", false, false), wynik("L-05", false, false), wynik("E-30", true, false, "problem")};
  kolejka::OcenaUspokojenia o = kolejka::ocenUspokojenie(w, {});
  TRZYMA(!o.wszystkieOcenione);  // silnik wie, że nie wszystko ocenił
  sprawy::WejscieSpraw ws;
  ws.dzis = "2026-11-05";
  std::vector<sprawy::SprawaL> brak;
  ws.sprawy = &brak;
  TRZYMA(sprawy::kartySpraw(ws).empty());  // stan pusty
  sprawy::KartaSprawy k = sprawy::kartaPusta(ws);
  printf("  stan pusty przy regułach „nie oceniam”: `Ocena: %s`\n", k.ocena.c_str());
  PRZYPADEK("„Otwarte sprawy” w stanie pustym bez pełnej oceny", k.ocena != "pełna");
}

// stopki K1 i K2. K1 bez `▶ sprawy` i `◀ 2 s norma`,
// K2 bez `▶ raport` i `↻ 2 s sprzęt`, choć przejścia działają (nawigacja.cpp). Jedyna droga do „Otwartych spraw”,
// raportu dla lekarza i „Sprzętu” jest niepodpisana.
static void testR7_04() {
  std::vector<std::string> k1 = stopkaK1(true), k2 = stopkaK2();
  KontekstNaw kn;
  kn.sprawyDostepne = true;
  // ciąg stron K1 ▶ K2 ▶ sprawy, raport ogólny ▶ 2 s w K2; „Twoja norma” i „Sprzęt”
  // to strony 4 i 5, nie przytrzymania. Znalezisko zostaje w tej postaci: każda droga podpisana w stopce.
  TRZYMA(nacisniecie(Ekran::K1, Przycisk::PRAWY, false, kn).ekran == Ekran::K2);
  TRZYMA(nacisniecie(Ekran::K2, Przycisk::PRAWY, false, kn).ekran == Ekran::SPRAWY);
  TRZYMA(nacisniecie(Ekran::K2, Przycisk::PRAWY, true, kn).ekran == Ekran::RAPORT);
  PRZYPADEK("stopki K1 i K2 według mapy przycisków", zawiera(k1, "▶ 7 nocy"));
  PRZYPADEK("stopki K1 i K2 według mapy przycisków", zawiera(k2, "▶ sprawy"));
  PRZYPADEK("stopki K1 i K2 według mapy przycisków", zawiera(k2, "2 s raport"));
}

// (przed testami na egzemplarzu, przypadek 7.8 nr 30): K2 ◀ 2 s otwiera U-05 „Test sprzętu” zamiast U-01, a S5 mówi
// `→ Sprawdź AirBridge i sieć. Ustawienia: K2 → ◀ 2 s`, stopka K2 `2 s ustawienia`. U-01 i U-02 z U-01 nie istnieją,
// więc po zmianie routera użytkownik nie zmieni sieci z panelu.
static void testR7_05() {
  KontekstNaw kn;
  Przejscie p = nacisniecie(Ekran::K2, Przycisk::LEWY, true, kn);
  OCZEKUJE("K2 ◀ 2 s otwiera U-05 „Test sprzętu” zamiast U-01, a S5 mówi", p.ekran != Ekran::TEST);
}

// drobne rozbieżności mapy. „Co znaczą te liczby” ▶ → K1 (kod: nic); U-05 ▶ → nic (kod:
// ekran dnia), ↻ 2 s → pakiet z karty (kod: to samo co ↻), ◀ → U-01 (kod: K2).
static void testR7_06() {
  KontekstNaw kn;
  // „Co znaczą te liczby” ▶ → „Sprzęt” (następna strona), U-05 ▶ odśwież (↻ to dom), ◀ → U-01
  PRZYPADEK("drobne rozbieżności mapy", nacisniecie(Ekran::LICZBY, Przycisk::PRAWY, false, kn).ekran == Ekran::SPRZET);
  PRZYPADEK("drobne rozbieżności mapy", nacisniecie(Ekran::TEST, Przycisk::PRAWY, false, kn).akcja == Akcja::PONOW_POBRANIE);
  PRZYPADEK("drobne rozbieżności mapy", nacisniecie(Ekran::TEST, Przycisk::LEWY, false, kn).ekran == Ekran::USTAWIENIA);
  OCZEKUJE("drobne rozbieżności mapy", nacisniecie(Ekran::TEST, Przycisk::SRODEK, true, kn).akcja !=
                          nacisniecie(Ekran::TEST, Przycisk::SRODEK, false, kn).akcja);
}

// (do decyzji, zakres „odblokowanie W normie 5.4.13”): stała lista PUNKTY_USPOKOJENIA (stany.cpp) ma na
// zawsze `oceniany = false` dla pkt 2 E-33 i S-K-17 (oba liczone), pkt 3 M-03…M-15 i pkt 5; kolejka
// zdejmuje tylko pkt 1, 4 i 7. Skutek: `W normie`, `Nic do zrobienia. Tak trzymaj.` i zdania uspokajające budzika
// nie pojawią się nigdy, nawet przy pełnej ocenie. Bezpieczne (ADR-0029 pkt 7), ale punkt zakresu nie jest
// zrobiony i nie stoi w „Braki”.
static void testR7_08() {
  std::vector<std::string> n = nieocenianeUspokojenia();
  PRZYPADEK("stała lista PUNKTY_USPOKOJENIA", !zawiera(n, "E-33"));
  PRZYPADEK("stała lista PUNKTY_USPOKOJENIA", !zawiera(n, "S-K-17"));
  // pełna ocena silnika, bez spraw: uspokojenie dalej niespełnione
  std::vector<Noc> h;
  for (int i = 0; i < 40; i++) {
    Noc x;
    x.data = dodajDni("2026-09-01", i);
    x.wazna = true;
    x.m01 = 420;
    x.ahi = 1.0f;
    x.wmed = 4;
    x.wp95 = 10;
    x.cp95 = 10;
    x.ep95 = 8;
    x.ustawienia = "ASVAuto/6/10/3/15";
    h.push_back(x);
  }
  Stany st = liczStany(h, h.back().data, {});
  std::vector<reguly::Wynik> w = {wynik("L-03", true, false), wynik("E-30", true, false, "problem")};
  kolejka::zastosujUspokojenie(st, kolejka::ocenUspokojenie(w, {}));
  printf("  pełna ocena silnika, brak spraw → uspokojenie %s, punkty nieoceniane %zu\n",
         st.uspokojenie ? "tak" : "nie", st.nieoceniane.size());
  OCZEKUJE("stała lista PUNKTY_USPOKOJENIA", st.uspokojenie);
}

// ---------- trzyma ----------
static void testTrzyma() {
  // „nie oceniam” nie daje uspokojenia (ADR-0029 pkt 7)
  std::vector<Noc> h;
  Stany st = liczStany(h, "2026-11-05", {});
  kolejka::zastosujUspokojenie(st, kolejka::ocenUspokojenie({wynik("L-03", false, false)}, {}));
  TRZYMA(!st.uspokojenie);
  // ekrany z potwierdzeniem w nocy: bez powrotu (R-PRZ-19), potwierdzenie „Omówione” wraca po 60 s także w nocy
  TRZYMA(sekundDoPowrotu(Ekran::ZAPISANO, 100, 2 * 60) == -1);
  TRZYMA(sekundDoPowrotu(Ekran::PRZENIESIONE, 100, 2 * 60) == -1);
  TRZYMA(sekundDoPowrotu(Ekran::OMOWIONE, 61, 2 * 60) == 0);
  TRZYMA(sekundDoPowrotu(Ekran::K1, 301, 2 * 60) == -1);
  TRZYMA(sekundDoPowrotu(Ekran::K1, 301, 8 * 60) == 0);
  // mapa 5.6.2: ekran L ◀ 2 s omówione, problem S3 ◀ 2 s nic, pytanie tak/nie ▶ tak, ↻ pomiń
  KontekstNaw kn;
  kn.wzorzec = true;
  TRZYMA(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, kn).ekran == Ekran::OMOWIONE);
  kn.problem = true;
  TRZYMA(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, kn).akcja == Akcja::NIC);
  KontekstNaw tn;
  tn.pytanieTN = true;
  TRZYMA(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, tn).akcja == Akcja::ODP_TAK);
  TRZYMA(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, tn).akcja == Akcja::ODP_POMIN);
  // tryby 4.11: wszystkie cztery rozpoznane (wycofane)
  TRZYMA(reguly::trybZUstawien("CPAP/9/2") == reguly::Tryb::CPAP);
  TRZYMA(reguly::trybZUstawien("AutoSet/7/15/2") == reguly::Tryb::AUTOSET);
  TRZYMA(reguly::trybZUstawien("ASV/8/3/15") == reguly::Tryb::ASV);
  TRZYMA(reguly::trybZUstawien("ASVAuto/6/10/3/15") == reguly::Tryb::ASVAUTO);
  TRZYMA(reguly::trybZUstawien("VAuto/6/10") == reguly::Tryb::NIEZNANY);  // przypadek 7.8 nr 50
}

// ---------- po poprawkach (sprawdzenia poprawek, blokujące) ----------
static void testPoPoprawkach() {
  // `Ocena: pełna` tylko przy pewnej pełnej ocenie; noc niepewna mówi swoje
  sprawy::WejscieSpraw ws;
  ws.dzis = "2026-11-05";
  TRZYMA(sprawy::kartaPusta(ws).ocena == "częściowa");  // karta pusta też ma wiersz oceny
  ws.ocenaPelna = true;
  TRZYMA(sprawy::kartaPusta(ws).ocena == "pełna");
  ws.ostatniaNiepewna = true;
  TRZYMA(sprawy::kartaPusta(ws).ocena == "ograniczona: ostatnia noc z dużym wyciekiem");
  // powód nieuspokojenia do kroku zamiennego R-EKR-32
  std::vector<Noc> h;
  for (int i = 0; i < 40; i++) {
    Noc x;
    x.data = dodajDni("2026-09-01", i);
    x.wazna = true;
    x.m01 = 420;
    x.ahi = 1.0f;
    x.wmed = 4;
    x.wp95 = 10;
    x.cp95 = 10;
    x.ep95 = 8;
    x.ustawienia = "ASVAuto/6/10/3/15";
    h.push_back(x);
  }
  Stany st = liczStany(h, h.back().data, {});
  TRZYMA(st.niespokoj == Stany::Niespokoj::SPRAWA);  // bez silnika nie wiadomo, czy sprawy nie ma
  Stany a = st;
  kolejka::zastosujUspokojenie(a, kolejka::ocenUspokojenie({wynik("L-03", true, false)}, {}));
  TRZYMA(a.niespokoj == Stany::Niespokoj::OCENY);  // zostają tylko pkt 3 i 5, których panel nie liczy
  Stany b = st;
  kolejka::zastosujUspokojenie(b, kolejka::ocenUspokojenie({wynik("L-03", false, false)}, {}));
  // „nie oceniam” z braku danych (`za malo nocy`) to DANE (`→ Część ocen jeszcze nie
  // działa: za mało nocy.`), a REGULY (`Część reguł nie działa`) zostaje dla awarii zestawu (R-AW-31)
  TRZYMA(b.niespokoj == Stany::Niespokoj::DANE);
  Stany b2 = st;
  reguly::Wynik awaria = wynik("L-03", false, false);
  awaria.powodNieoceny = "nieznany rodzaj xyz";
  kolejka::zastosujUspokojenie(b2, kolejka::ocenUspokojenie({awaria}, {}));
  TRZYMA(b2.niespokoj == Stany::Niespokoj::REGULY);
  Stany c = st;
  kolejka::zastosujUspokojenie(c, kolejka::ocenUspokojenie({wynik("L-03", true, true)}, {}));
  TRZYMA(c.niespokoj == Stany::Niespokoj::SPRAWA);
  // warunek S-K-10 „od kilku nocy” wyklucza E-23, wiersz pomocniczy dostaje ocenę
  std::vector<Noc> w3 = h;
  for (size_t i = w3.size() - 3; i < w3.size(); i++) w3[i].wp95 = 30;
  TRZYMA(sk::sk10Kilka(&w3, w3.back().data, false) && !sk::sk10Kilka(&w3, w3.back().data, true) &&
         !sk::sk10Kilka(&h, h.back().data, false));
  reguly::Wynik e23 = wynik("E-23", true, true, "problem"), pom = wynik("E-23", false, false, "");
  pom.wariant = "bez_sk10_kilka";
  std::vector<reguly::Wynik> wy = {e23, pom};
  sk::zastosujSk10Kilka(wy, true);
  TRZYMA(!wy[0].spelniona && wy[0].zmienne["wykluczona_przez"] == "S-K-10/kilka" && wy[1].oceniona && !wy[1].spelniona);
  wy = {e23, pom};
  sk::zastosujSk10Kilka(wy, false);
  TRZYMA(wy[0].spelniona && wy[1].oceniona && wy[1].spelniona);
  // stopki K1 i K2 dosłownie ze spec (5.5.3)
  TRZYMA(stopkaK2() == std::vector<std::string>({"◀ ostatnia noc", "▶ sprawy · 2 s raport", "↻ ekran dnia"}));
  // (R-PRZ-19): w nocy jedno wybudzenie po czasie cofnięcia, potem nic
  TRZYMA(sekundDoKoncaCofniecia(Ekran::PRZENIESIONE, 10, 2 * 60) == 20);
  TRZYMA(sekundDoKoncaCofniecia(Ekran::ZAPISANO, 10, 2 * 60) == 50);
  TRZYMA(sekundDoKoncaCofniecia(Ekran::ZAPISANO, 10, 9 * 60) == -1);  // w dzień zwykły powrót
  TRZYMA(koniecCofnieciaTeraz(Ekran::PRZENIESIONE, 32, 2 * 60) && !koniecCofnieciaTeraz(Ekran::PRZENIESIONE, 90, 2 * 60));
  PlanSnu ps = planSnu(true, 1000000u, Ekran::ZAPISANO, 1000000u - 10, 2 * 60, 0);
  TRZYMA(ps.sekund == 50 + ZAPAS_POWROTU_S);
  WejsciePlanu wp;
  wp.zegar = true;
  wp.sekundaDoby = 2 * 3600 + 32;
  wp.ekran = Ekran::PRZENIESIONE;
  wp.sekundOdNacisniecia = 32;
  TRZYMA((powodyTeraz(wp) & POWOD_POWROT) != 0);
  wp.sekundOdNacisniecia = 10;
  wp.sekundaDoby = 2 * 3600 + 10;
  Termin tn = nastepnyTermin(wp);
  TRZYMA(tn.sekund == 20 + ZAPAS_POWROTU_S && (tn.powody & POWOD_POWROT));
  // „Co znaczą te liczby” ▶ → „Sprzęt” (sprawdzane też wyżej)
  KontekstNaw kn;
  TRZYMA(nacisniecie(Ekran::LICZBY, Przycisk::PRAWY, false, kn).ekran == Ekran::SPRZET);
}

int main() {
  testR7_03();
  testR7_04();
  testR7_05();
  testR7_06();
  testR7_08();
  testTrzyma();
  testPoPoprawkach();
  printf("testy_styki_modulow_brzegowe: %d ok, %d błędów, %d oczekujących (nie blokują)\n", ok, zle, oczekuje);
  return zle ? 1 : 0;
}
