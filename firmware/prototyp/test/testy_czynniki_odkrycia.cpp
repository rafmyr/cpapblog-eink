// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: czynniki nocy pełne (5.11),
// odkrycia O-03…O-12 (5.4.10, 6.5) z testem permutacyjnym i poprawką Holma (R-NF-24, spec techniczna 9.3), podsumowanie
// miesiąca S7, „Twoja norma” S8, „Co znaczą te liczby” (5.4.7, 5.4.8, 5.4.14) i nawigacja (5.6.2).
// Przypadki z kolumny „Weryfikacja” spec funkcjonalnej, gdy się dało dosłownie.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_czynniki_odkrycia).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "cykl.h"
#include "czynniki.h"
#include "dyrygent.h"
#include "kokpit.h"
#include "sprawy.h"
#include "wybudzenie.h"
#include "zestaw_kolejki.h"
#include "ekrany.h"
#include "nawigacja.h"
#include "noc.h"
#include "odkrycia.h"
#include "podsumowanie.h"
#include "reguly.h"
#include "wspolne.h"

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

using namespace reguly;
static const char* const U = "ASVAuto/6/10/3/15";

static Noc noc(const std::string& d, double ahi, int m01 = 420, double wp95 = 8) {
  Noc n;
  n.data = d;
  n.wazna = m01 >= 240;
  n.m01 = m01;
  n.ahi = (float)ahi;
  n.wp95 = (float)wp95;
  n.wmed = 4;
  n.ustawienia = U;
  return n;
}
static Kontekst kontekst(const std::vector<Noc>& h, const std::string& dzis) {
  Kontekst k;
  k.historia = &h;
  k.dzis = dzis;
  k.trybBiezacy = Tryb::ASVAUTO;
  k.zestaw = &zestawWbudowany();
  return k;
}
static const Wynik* znajdz(const std::vector<Wynik>& w, const std::string& kl) {
  for (auto& x : w)
    if (odkrycia::klucz(x) == kl) return &x;
  return nullptr;
}

// ---------- czynniki ----------
static void testCzynnikiWyzwalacz() {
  Norma nm;
  nm.ahi.n = nm.m01.n = nm.wp95.n = nm.m13.n = 30;
  nm.ahi.med = 1; nm.ahi.p10 = 0.5; nm.ahi.p90 = 2.5;
  nm.m01.med = 420; nm.m01.p10 = 380; nm.m01.p90 = 460;
  nm.wp95.med = 8; nm.wp95.p10 = 6; nm.wp95.p90 = 12;
  nm.m13.med = 15; nm.m13.p10 = 14; nm.m13.p90 = 16;
  Noc n = noc("2026-11-12", 1.0);
  n.m13 = 18.5f;  // R-CZY-01: M-13 w wyzwalaczu (P90 16, Δ 2 od mediany 15)
  WejscieDecyzji we;
  we.noc = &n;
  we.norma = nm;
  we.nocyWaznychPrzed = 30;
  SPRAWDZ(odchylenieCzynnikow(n, nm));
  SPRAWDZ(decyzjaZaproszenia(we) == 'O');
  we.wzorzecA = true;  // R-PIE-06
  SPRAWDZ(decyzjaZaproszenia(we) == 0);
  we.wzorzecA = false;
  we.sk30 = true;  // R-CZY-15
  SPRAWDZ(decyzjaZaproszenia(we) == 0);
  we.sk30 = false;
  n.m13 = 15;
  we.sk17 = true;  // R-CZY-07: zaproszenie poza warstwami O-12
  SPRAWDZ(decyzjaZaproszenia(we) == 'Z');
  n.m13 = 16.9f;  // powyżej P90, ale mniej niż Δ od mediany
  SPRAWDZ(!odchylenieCzynnikow(n, nm));
}

static void testWyjazd() {
  // R-CZY-19: AirBridge nie odpowiada od 21.10 06:00 do 25.10 14:00, noce 22.10…25.10 (daty poranka)
  std::vector<WpisCzynnikow> v;
  std::vector<std::string> noce = {"2026-10-21", "2026-10-22", "2026-10-23", "2026-10-24", "2026-10-25"};
  SPRAWDZ(oznaczPozaDomem(v, noce, "2026-10-21", 6 * 60, "2026-10-25", 14 * 60) == 4);
  SPRAWDZ(wykluczoneZNormy(v).size() == 4 && nocePozaDomem(v).count("2026-10-25"));
  WpisCzynnikow w;
  SPRAWDZ(parsujWpis(zapiszWpis(v[0]), w) && w.warstwa == 'A' && w.czynniki == CZ_POZA_DOMEM);
  for (auto& x : v) SPRAWDZ(!wWarstwieO12(x));  // R-CZY-04: nie są odpowiedzią
  // pierwsza odpowiedź 26.10 o 05:30: ostatnia noc (powrót do domu) bez czynnika
  std::vector<WpisCzynnikow> v2;
  noce.push_back("2026-10-26");
  SPRAWDZ(oznaczPozaDomem(v2, noce, "2026-10-21", 6 * 60, "2026-10-26", 5 * 60 + 30) == 4);
  SPRAWDZ(!wpisZDnia(v2, "2026-10-26"));
  // przerwa 30 h: bez oznaczenia
  std::vector<WpisCzynnikow> v3;
  SPRAWDZ(oznaczPozaDomem(v3, noce, "2026-10-21", 6 * 60, "2026-10-22", 12 * 60) == 0);
  // odpowiedź użytkownika ma pierwszeństwo (R-CZY-13)
  std::vector<WpisCzynnikow> v4;
  zapiszOdpowiedz(v4, "2026-10-23", CZ_ALKOHOL);
  SPRAWDZ(oznaczPozaDomem(v4, noce, "2026-10-21", 6 * 60, "2026-10-25", 14 * 60) == 3);
  SPRAWDZ(wpisZDnia(v4, "2026-10-23")->czynniki == CZ_ALKOHOL);
  // R-CZY-22: jeden okres (22.10–25.10 bez 23.10 → dwa okresy), teksty pytania
  std::vector<OkresWyjazdu> o = okresyWyjazdu(v);
  SPRAWDZ(o.size() == 1 && o[0].nocy == 4 && o[0].od == "2026-10-22");
  TekstyWyjazdu t = tekstyWyjazdu(o[0], 11 * 60);
  ROWNE(t.etykieta, "Noce bez AirBridge: 4");
  ROWNE(t.tytul, "Spanie poza domem od 22.10?");
  ROWNE(t.opis, "Panel nie miał wtedy połączenia z AirBridge.");
  ROWNE(t.stopka, "Bez odpowiedzi do 11:00 = zostaje „poza domem”.");
  ROWNE(tekstyWyjazdu(o[0], 15 * 60).stopka, "Bez odpowiedzi do 15:00 = zostaje „poza domem”.");
  SPRAWDZ(okresyWyjazdu(v4).size() == 2);
  SPRAWDZ(okresDoPytania(o, {}, "2026-10-26") == &o[0]);
  SPRAWDZ(okresDoPytania(o, {"2026-10-22"}, "2026-10-26") == nullptr);
  SPRAWDZ(okresDoPytania(o, {}, "2026-11-02") == nullptr);  // po 7 dniach przepada
  // R-CZY-23: „nie” zdejmuje, cofnięcie przywraca
  std::vector<WpisCzynnikow> zd = zdejmijPozaDomem(v, o[0]);
  SPRAWDZ(zd.size() == 4 && v.empty() && wykluczoneZNormy(v).empty());
  przywrocWpisy(v, zd);
  SPRAWDZ(v.size() == 4);
  // odpowiedź z K1 dla nocy z oznaczeniem automatycznym: odpowiedź użytkownika, poza O-12 (warstwa Z)
  zapiszOdpowiedz(v, "2026-10-25", CZ_STRES);
  SPRAWDZ(wpisZDnia(v, "2026-10-25")->warstwa == 'Z' && wpisZDnia(v, "2026-10-25")->czynniki == CZ_STRES);
}

static void testRaport() {
  // R-CZY-06: domyślnie bez alkoholu; katar; noce automatyczne osobnym zdaniem
  std::vector<WpisCzynnikow> v;
  for (int i = 0; i < 3; i++) zapiszOdpowiedz(v, dodajDni("2026-11-12", -i), CZ_KATAR | CZ_ALKOHOL);
  zapiszOdpowiedz(v, "2026-11-05", CZ_POZA_DOMEM);
  oznaczPozaDomem(v, {"2026-11-01", "2026-11-02", "2026-11-03", "2026-11-04"}, "2026-10-31", 6 * 60, "2026-11-04", 12 * 60);
  std::vector<std::string> z = zdaniaRaportu(v, "2026-11-12", RAPORT_DOMYSLNIE, false);
  SPRAWDZ(z.size() == 1);
  ROWNE(z.empty() ? "" : z[0], "Zgłoszone przez pacjenta: katar lub infekcja w 3 z 14 nocy.");
  z = zdaniaRaportu(v, "2026-11-12", CZ_POZA_DOMEM, false);
  SPRAWDZ(z.size() == 2);
  ROWNE(z.size() > 1 ? z[0] : "", "Zgłoszone przez pacjenta: spanie poza domem w 1 z 14 nocy.");
  ROWNE(z.size() > 1 ? z[1] : "", "Bez danych z domu: 4 z 14 nocy.");
  SPRAWDZ(zdaniaRaportu(v, "2026-11-12", 0, false).empty());
  std::vector<WpisCzynnikow> l;
  zapiszOdpowiedz(l, "2026-11-12", CZ_LEKI);
  SPRAWDZ(zdaniaRaportu(l, "2026-11-12", RAPORT_DOMYSLNIE, false).empty());  // leki tylko przy L-01 (ADR-0013)
  SPRAWDZ(zdaniaRaportu(l, "2026-11-12", RAPORT_DOMYSLNIE, true).size() == 1);
}

// ---------- O-12 ----------
// Noce z odpowiedzią: `ile` nocy od `od` w warstwie `war` z czynnikiem `c` i AHI `ahi`; zaproszenie pokazane.
static void dodaj(std::vector<Noc>& h, std::vector<WpisCzynnikow>& v, std::string& d, int ile, char war, uint8_t c, double ahi) {
  for (int i = 0; i < ile; i++) {
    h.push_back(noc(d, ahi));
    WpisCzynnikow w;
    w.data = d;
    w.stan = 'O';
    w.czynniki = c;
    w.warstwa = war;
    w.pokazano = true;
    v.push_back(w);
    d = dodajDni(d, 1);
  }
}
static std::vector<Wynik> ocenO12(const std::vector<Noc>& h, const std::vector<WpisCzynnikow>& v, odkrycia::TrybOceny t,
                                  std::set<std::string> pokazane = {}) {
  odkrycia::CechyNocy c;
  c.tryb = t;
  c.czynniki = &v;
  c.pokazane = pokazane;
  return odkrycia::ocenOdkrycia(zestawWbudowany(), kontekst(h, h.back().data), c);
}

static void testO12Wagi() {
  // R-CZY-24, przypadek z kolumny „Weryfikacja”: efektywna liczebność alkoholu (4 + 14)² / (4 + 98) = 3,2 < 5 → brak O-12
  std::vector<Noc> h;
  std::vector<WpisCzynnikow> v;
  std::string d = "2026-06-01";
  dodaj(h, v, d, 4, 'O', CZ_ALKOHOL, 6.0);
  dodaj(h, v, d, 2, 'K', CZ_ALKOHOL, 1.5);
  dodaj(h, v, d, 10, 'O', CZ_NIC, 5.0);
  dodaj(h, v, d, 30, 'K', CZ_NIC, 1.5);
  std::vector<Wynik> w = ocenO12(h, v, odkrycia::TrybOceny::REWALIDACJA);
  const Wynik* a = znajdz(w, "O-12/A.M-02");
  SPRAWDZ(a && !a->oceniona && a->powodNieoceny.find("efektywna") != std::string::npos);
  // dzienna ocena: O-12 tylko w przeglądzie
  w = ocenO12(h, v, odkrycia::TrybOceny::DZIENNY);
  a = znajdz(w, "O-12/A.M-02");
  SPRAWDZ(a && !a->oceniona);
  // noce bez pokazanej linii (pytanie z K1, warstwa Z) i bez odpowiedzi nie wchodzą (R-CZY-04)
  std::vector<WpisCzynnikow> v2 = v;
  for (auto& x : v2) x.warstwa = 'Z';
  std::vector<Wynik> wy2_a = ocenO12(h, v2, odkrycia::TrybOceny::REWALIDACJA);
  a = znajdz(wy2_a, "O-12/A.M-02");
  SPRAWDZ(a && !a->oceniona);
}

static std::vector<Noc> gH;
static std::vector<WpisCzynnikow> gV;
static void archiwumAlkoholu(double ahiAlk, int kontrolneAlk) {
  gH.clear();
  gV.clear();
  std::string d = "2026-05-01";
  dodaj(gH, gV, d, 6, 'O', CZ_ALKOHOL, ahiAlk);
  dodaj(gH, gV, d, kontrolneAlk, 'K', CZ_ALKOHOL, ahiAlk);
  dodaj(gH, gV, d, 10, 'O', CZ_NIC, 1.8);
  dodaj(gH, gV, d, 35, 'K', CZ_NIC, 1.8);
}

static void testO12Odkrycie() {
  archiwumAlkoholu(4.9, 8);  // r = 1: wagi 1 (O) i 7 (K); efektywna (6 + 56)² / (6 + 392) = 9,7
  std::vector<Wynik> w = ocenO12(gH, gV, odkrycia::TrybOceny::PRZEGLAD);
  const Wynik* a = znajdz(w, "O-12/A.M-02");
  SPRAWDZ(a && a->oceniona && a->spelniona);
  SPRAWDZ(a && a->zmienne.count("holm") && std::atof(a->zmienne.at("p").c_str()) < 0.01);
  if (!a) return;
  odkrycia::UstawieniaTekstow u;
  u.czynnikiWrazliwe = true;
  odkrycia::TrescOdkrycia t = odkrycia::trescOdkrycia(zestawWbudowany(), *a, Tryb::ASVAUTO, u);
  ROWNE(t.tytul, "Po alkoholu: wyższe AHI");
  ROWNE(t.powod, "AHI 4,9 / h po alkoholu, 1,8 / h bez. Z 14/45 nocy.");
  ROWNE(t.krok, "→ Sam zdecyduj, co z tym zrobisz.");
  ROWNE(odkrycia::kluczHumoru(*a), "O-12/A");
  // R-CZY-20: domyślnie czynnik ukryty na ekranie dnia, pełny tytuł i powód w K1
  u.czynnikiWrazliwe = false;
  t = odkrycia::trescOdkrycia(zestawWbudowany(), *a, Tryb::ASVAUTO, u);
  ROWNE(t.tytul, "Odkrycie o czynniku nocy");
  ROWNE(t.powod, "Wynik z twoich odpowiedzi rano. Szczegóły: ▶ kokpit.");
  ROWNE(t.tytulK1, "Po alkoholu: wyższe AHI");
  SPRAWDZ(t.ukryty);
  // profil publiczny: AHI bez zamiany w obu profilach (5.4.14)
  u.czynnikiWrazliwe = true;
  t = odkrycia::trescOdkrycia(zestawWbudowany(), *a, Tryb::ASVAUTO, u);
  ROWNE(t.tytul, "Po alkoholu: wyższe AHI");
  // para czynnik–metryka pokazana: wypada z przeglądów (9.3)
  w = ocenO12(gH, gV, odkrycia::TrybOceny::PRZEGLAD, {"O-12/A.M-02"});
  a = znajdz(w, "O-12/A.M-02");
  SPRAWDZ(a && !a->oceniona);
  // R-CZY-21: mała grupa (< 10 nocy) → `Może to przypadek.`
  archiwumAlkoholu(4.9, 3);  // 9 nocy z alkoholem, efektywna (6 + 21)² / (6 + 147) = 4,8 < 5
  std::vector<Wynik> w9 = ocenO12(gH, gV, odkrycia::TrybOceny::REWALIDACJA);
  a = znajdz(w9, "O-12/A.M-02");
  SPRAWDZ(a && !a->oceniona);
  archiwumAlkoholu(4.9, 4);  // 10 nocy: bez zdania
  std::vector<Wynik> w10 = ocenO12(gH, gV, odkrycia::TrybOceny::REWALIDACJA);
  a = znajdz(w10, "O-12/A.M-02");
  SPRAWDZ(a && a->oceniona && a->zmienne.at("mala") == "nie");
}

static void testO12Leki() {
  gH.clear();
  gV.clear();
  std::string d = "2026-05-01";
  dodaj(gH, gV, d, 6, 'O', CZ_LEKI, 6.0);
  dodaj(gH, gV, d, 3, 'K', CZ_LEKI, 6.0);
  dodaj(gH, gV, d, 10, 'O', CZ_NIC, 1.8);
  dodaj(gH, gV, d, 35, 'K', CZ_NIC, 1.8);
  // efektywna (6 + 21)² / (6 + 147) = 4,8: za mało; dołóż 1 kontrolną
  dodaj(gH, gV, d, 1, 'K', CZ_LEKI, 6.0);
  std::vector<Wynik> wl = ocenO12(gH, gV, odkrycia::TrybOceny::REWALIDACJA);
  const Wynik* a = znajdz(wl, "O-12/L.M-02");
  SPRAWDZ(a && a->oceniona && a->spelniona && a->zmienne.at("mala") == "nie");
  if (!a) return;
  odkrycia::UstawieniaTekstow u;
  u.czynnikiWrazliwe = true;
  odkrycia::TrescOdkrycia t = odkrycia::trescOdkrycia(zestawWbudowany(), *a, Tryb::ASVAUTO, u);
  ROWNE(t.krok, "→ Powiedz o tym lekarzowi.");
  ROWNE(t.tytul, "Odkrycie: leki nasenne");  // `Po lekach nasennych: wyższe AHI` ma 31 znaków (reguła tytułu O-12)
}

// T-TEST-017 (skrót): czynnik przypisany losowo → odsetek przeglądów z O-12 nie większy niż poziom istotności.
static void testLosowyCzynnik() {
  int zOdkryciem = 0, przebiegow = 20, ocenionych = 0;
  uint32_t s = 12345;
  auto los = [&]() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; };
  for (int p = 0; p < przebiegow; p++) {
    std::vector<Noc> h;
    std::vector<WpisCzynnikow> v;
    std::string d = "2026-03-01";
    for (int i = 0; i < 120; i++) {
      double ahi = 1.0 + (los() % 400) / 100.0;  // 1,0…5,0 bez związku z czynnikiem
      uint8_t c = los() % 3 == 0 ? (uint8_t)(los() % 2 ? CZ_ALKOHOL : CZ_STRES) : (uint8_t)CZ_NIC;
      dodaj(h, v, d, 1, i % 4 == 0 ? 'O' : 'K', c, ahi);
    }
    std::vector<Wynik> w = ocenO12(h, v, odkrycia::TrybOceny::PRZEGLAD);
    bool jest = false;
    for (auto& x : w) {
      if (x.zmienne.count("holm") && std::atof(x.zmienne.at("p").c_str()) > 0.05) SPRAWDZ(!x.spelniona);
      if (x.id == "O-12" && x.spelniona) jest = true;
      if (x.id == "O-12" && x.oceniona) ocenionych++;
    }
    zOdkryciem += jest;
  }
  printf("  T-TEST-017 (skrót): przeglądy z O-12 przy czynniku losowym: %d z %d (porównań ocenionych %d)\n", zOdkryciem,
         przebiegow, ocenionych);
  SPRAWDZ(ocenionych >= przebiegow * 4);  // test ma sens tylko na ocenionych porównaniach (2 czynniki × 4 metryki)
  SPRAWDZ(zOdkryciem <= 2);  // 5% z 20 = 1; zapas na wariancję
}

// ---------- O-03, O-04, O-08 ----------
static void testO03O04() {
  std::vector<Noc> h;
  std::string d = "2026-09-01";
  for (int i = 0; i < 40; i++, d = dodajDni(d, 1)) h.push_back(noc(d, 3.0));
  for (int i = 0; i < 7; i++, d = dodajDni(d, 1)) h.push_back(noc(d, 1.0));
  odkrycia::CechyNocy c;
  std::vector<Wynik> w = odkrycia::ocenOdkrycia(zestawWbudowany(), kontekst(h, h.back().data), c);
  const Wynik* o3 = znajdz(w, "O-03/M-02");
  SPRAWDZ(o3 && o3->oceniona && o3->spelniona);
  const Wynik* o4 = znajdz(w, "O-04/M-02");
  SPRAWDZ(o4 && o4->oceniona && !o4->spelniona);  // mediana 30 nocy 3,0, tydzień 1,0: różnica 2 < Δ 3
  SPRAWDZ(znajdz(w, "O-12/A.M-02") && !znajdz(w, "O-12/A.M-02")->oceniona);  // tylko przegląd
  if (o3) {
    odkrycia::TrescOdkrycia t = odkrycia::trescOdkrycia(zestawWbudowany(), *o3, Tryb::ASVAUTO, {}, &h);
    ROWNE(t.tytul, "Rekordowy tydzień");
    ROWNE(t.powod, "AHI 1,0 / h, najniżej od września.");
    ROWNE(t.krok, "→ Nic do zmiany. Tak trzymaj.");
    SPRAWDZ(t.linia && t.wartosci30.size() == 30);
    odkrycia::UstawieniaTekstow u;
    u.uspokojenie = false;  // R-EKR-32: krok zamienny
    ROWNE(odkrycia::trescOdkrycia(zestawWbudowany(), *o3, Tryb::ASVAUTO, u, &h).krok, "→ Otwarte sprawy: za stroną 2 / 3.");
    // bez kart spraw (strony nie ma, R-KOK-19) bez kroku do niej
    u.bezKartSpraw = true;
    ROWNE(odkrycia::trescOdkrycia(zestawWbudowany(), *o3, Tryb::ASVAUTO, u, &h).krok, "→ Obserwuj kolejne noce.");
    u.bezSilnika = true;
    ROWNE(odkrycia::trescOdkrycia(zestawWbudowany(), *o3, Tryb::ASVAUTO, u, &h).krok, "→ Część reguł nie działa: zobacz Test sprzętu.");
    u.bezKartSpraw = u.bezSilnika = false;
    // O-03 bez wariantu publicznego (AHI w obu profilach)
    SPRAWDZ(odkrycia::trescOdkrycia(zestawWbudowany(), *o3, Tryb::ASVAUTO, u, &h).powod.rfind("AHI 1,0 / h", 0) == 0);
  }
  // historia krótsza niż 30 dni → nie oceniam
  std::vector<Noc> k(h.end() - 20, h.end());
  std::vector<Wynik> wk = odkrycia::ocenOdkrycia(zestawWbudowany(), kontekst(k, k.back().data), c);
  const Wynik* x = znajdz(wk, "O-03/M-02");
  SPRAWDZ(x && !x->oceniona);
  // O-04: tydzień 1,0, miesiąc 5,0 → lepiej o 4 ≥ Δ 3
  std::vector<Noc> h4;
  d = "2026-09-01";
  for (int i = 0; i < 23; i++, d = dodajDni(d, 1)) h4.push_back(noc(d, 5.0));
  for (int i = 0; i < 7; i++, d = dodajDni(d, 1)) h4.push_back(noc(d, 1.0));
  o4 = nullptr;
  std::vector<Wynik> w4 = odkrycia::ocenOdkrycia(zestawWbudowany(), kontekst(h4, h4.back().data), c);
  o4 = znajdz(w4, "O-04/M-02");
  SPRAWDZ(o4 && o4->spelniona);
  if (o4) ROWNE(odkrycia::trescOdkrycia(zestawWbudowany(), *o4, Tryb::ASVAUTO, {}).powod, "AHI 1,0 / h, w ostatnim miesiącu 5,0 / h.");
}

static void testO08Weekend() {
  std::vector<Noc> h;
  std::string d = "2026-08-01";
  for (int i = 0; i < 90; i++, d = dodajDni(d, 1)) {
    int dt = dzienTygodnia(d);
    h.push_back(noc(d, dt == 0 || dt == 6 ? 6.0 : 1.0));
  }
  odkrycia::CechyNocy c;
  c.tryb = odkrycia::TrybOceny::PRZEGLAD;
  std::vector<Wynik> w = odkrycia::ocenOdkrycia(zestawWbudowany(), kontekst(h, h.back().data), c);
  const Wynik* o = znajdz(w, "O-08/M-02");
  SPRAWDZ(o && o->oceniona && o->spelniona);
  if (o) ROWNE(odkrycia::trescOdkrycia(zestawWbudowany(), *o, Tryb::ASVAUTO, {}).powod,
               "W weekendy AHI wyższe o 5,0 / h. Z 90 nocy.");
  // O-09 bez M-18 w rekordzie nocy: „nie oceniam”; O-07 bez wilgotności: „nie oceniam”
  SPRAWDZ(znajdz(w, "O-09/M-02") && !znajdz(w, "O-09/M-02")->oceniona);
  SPRAWDZ(znajdz(w, "O-07/M-07") && !znajdz(w, "O-07/M-07")->oceniona);
  // O-07 z wilgotnością: suche noce wyciek wyższy o 12 (Δ 10)
  for (auto& n : h) {
    bool sucho = n.data.substr(8, 2) < "10";
    c.wartosci["wilgotnosc"][n.data] = sucho ? 30 : 45;
    n.wp95 = sucho ? 22.0f : 10.0f;
  }
  w = odkrycia::ocenOdkrycia(zestawWbudowany(), kontekst(h, h.back().data), c);
  o = znajdz(w, "O-07/M-07");
  SPRAWDZ(o && o->spelniona);
  if (o) ROWNE(odkrycia::trescOdkrycia(zestawWbudowany(), *o, Tryb::ASVAUTO, {}).powod,
               "Wyciek wyższy o 12 l/min przy wilgotności poniżej 35%.");
  // grupa weekendowa za mała (4 noce w oknie 30 nocy z min 5): nie oceniam
  std::vector<Noc> k(h.end() - 12, h.end());
  std::vector<Wynik> wk = odkrycia::ocenOdkrycia(zestawWbudowany(), kontekst(k, k.back().data), c);
  o = znajdz(wk, "O-08/M-02");
  SPRAWDZ(o && !o->oceniona);
}

static void testZablokowane() {
  for (const char* id : {"O-05", "O-06", "O-10", "O-11"}) {
    bool jest = false, blok = true;
    for (auto& w : zestawWbudowany().katalog)
      if (w.id == id) { jest = true; blok = blok && w.stan == StanReguly::ZABLOKOWANA; }
    SPRAWDZ(jest && blok);
  }
  int o12 = 0;
  for (auto& w : zestawWbudowany().katalog) o12 += w.id == "O-12";
  SPRAWDZ(o12 == 28);  // 7 czynników × 4 metryki (9.3)
  for (auto& w : odkrycia::bezOdkryc(zestawWbudowany()).katalog) SPRAWDZ(!odkrycia::wierszOdkrycia(w.id));
}

// ---------- wybór i limity ----------
static Wynik wynik(const std::string& id, const std::string& war, double wart, double delta) {
  Wynik w;
  w.id = id;
  w.wariant = war;
  w.oceniona = w.spelniona = true;
  w.zmienne["wartosc_num"] = std::to_string(wart);
  w.zmienne["delta_num"] = std::to_string(delta);
  return w;
}
static void testWybor() {
  std::vector<Wynik> dz = {wynik("O-04", "M-02", 1.0, 3), wynik("O-03", "M-02", 1.0, 3)};
  std::vector<odkrycia::Pokazanie> pok;
  odkrycia::WejscieWyboru w;
  w.dzis = "2026-11-12";
  w.dzienne = &dz;
  w.pokazane = &pok;
  std::string p;
  const Wynik* x = odkrycia::wybierz(w, p);
  SPRAWDZ(x && x->id == "O-03");  // R-PIE-03: niższy numer
  w.rozruch = true;
  SPRAWDZ(!odkrycia::wybierz(w, p));  // R-EKR-28
  w.rozruch = false;
  pok.push_back({"O-04/M-02", "2026-11-08", 1.0});
  SPRAWDZ(!odkrycia::wybierz(w, p) && p.find("R-EKR-25") != std::string::npos);  // 1 na 7 dni
  pok[0].data = "2026-11-05";  // 7 dni temu: limit minął
  dz = {wynik("O-04", "M-02", 1.0, 3)};
  SPRAWDZ(!odkrycia::wybierz(w, p));  // R-EKR-27: w 30 dniach bez zmiany o Δ
  dz = {wynik("O-04", "M-02", 4.5, 3)};
  SPRAWDZ(odkrycia::wybierz(w, p));  // zmiana o ≥ Δ
  w.pokazaniaWspolne = {"2026-11-10"};  // S-K-21 w tym samym limicie
  SPRAWDZ(!odkrycia::wybierz(w, p));
  w.pokazaniaWspolne.clear();
  // kolejka z przeglądu: O-12 od następnego dnia po przeglądzie (D4), para raz
  std::vector<Wynik> kq = {wynik("O-12", "A.M-02", 2.3, 3)};
  odkrycia::Kolejka kol;
  kol.przeglad = "2026-11-12";
  kol.klucze = {"O-12/A.M-02"};
  w.kolejka = &kq;
  w.kol = &kol;
  dz.clear();
  pok.clear();
  SPRAWDZ(!odkrycia::wybierz(w, p));
  w.dzis = "2026-11-13";
  x = odkrycia::wybierz(w, p);
  SPRAWDZ(x && x->id == "O-12");
  pok.push_back({"O-12/A.M-02", "2026-11-13", 2.3});
  SPRAWDZ(odkrycia::wybierz(w, p) == x);  // R-EKR-31: ten sam dzień
  w.dzis = "2026-12-30";
  SPRAWDZ(!odkrycia::wybierz(w, p));  // para pokazana: nigdy więcej (9.3)
  kq[0].spelniona = false;  // R-EKR-30
  w.dzis = "2026-11-13";
  pok.clear();
  SPRAWDZ(!odkrycia::wybierz(w, p));
  // zapis i odczyt
  std::vector<odkrycia::Pokazanie> pp = odkrycia::parsujPokazania(odkrycia::zapiszPokazania({{"O-03/M-02", "2026-11-01", 1.5}}));
  SPRAWDZ(pp.size() == 1 && pp[0].klucz == "O-03/M-02" && std::fabs(pp[0].wartosc - 1.5) < 1e-9);
  odkrycia::Kolejka k2 = odkrycia::parsujKolejke(odkrycia::zapiszKolejke(kol));
  SPRAWDZ(k2.przeglad == "2026-11-12" && k2.klucze.size() == 1);
  SPRAWDZ(odkrycia::przegladDzis("2026-11-15", "2026-11-08"));   // niedziela
  SPRAWDZ(!odkrycia::przegladDzis("2026-11-15", "2026-11-15"));
  SPRAWDZ(!odkrycia::przegladDzis("2026-11-16", "2026-11-15"));
  SPRAWDZ(odkrycia::przegladDzis("2026-11-24", "2026-11-15"));   // panel uśpiony w niedzielę
}

// ---------- S7, S8, „Co znaczą te liczby” ----------
static void testS7() {
  std::vector<Noc> h;
  std::string d = "2026-10-01";
  for (int i = 0; i < 31; i++, d = dodajDni(d, 1)) h.push_back(noc(d, 1.2, 420, 14));
  for (int i = 0; i < 30; i++, d = dodajDni(d, 1)) h.push_back(noc(d, 1.0, i < 5 ? 200 : 420, 2));
  SPRAWDZ(podsumowanie::kandydatS7("2026-12-01", h));
  SPRAWDZ(!podsumowanie::kandydatS7("2026-12-02", h));
  podsumowanie::DaneS7 s = podsumowanie::budujS7("2026-12-01", h, Tryb::ASVAUTO, false);
  ROWNE(s.etykieta, "Twój listopad");
  ROWNE(s.noce + s.z, "25/30");
  ROWNE(s.ahi, "1,0");
  ROWNE(s.godziny, std::to_string((int)std::lround((25 * 420 + 5 * 200) / 60.0)));
  ROWNE(s.zdanie, "Wyciek niższy niż w październiku.");  // R-EKR-19: iloraz 1,2 (Δ 10)
  for (auto& n : h)
    if (n.data >= "2026-11-01") { n.ahi = 4.6f; n.wp95 = 14; }
  ROWNE(podsumowanie::budujS7("2026-12-01", h, Tryb::ASVAUTO, false).zdanie, "AHI wyższe niż w październiku.");
  ROWNE(podsumowanie::budujS7("2026-12-01", h, Tryb::ASVAUTO, true).zdanie, "AHI wyższe niż w październiku.");  // AHI w obu
  for (auto& n : h)
    if (n.data >= "2026-11-01") n.ahi = 1.3f;
  ROWNE(podsumowanie::budujS7("2026-12-01", h, Tryb::ASVAUTO, false).zdanie, "Podobnie jak w październiku.");
  std::vector<Noc> k(h.begin() + 20, h.end());  // październik z 11 nocami: bez zdania (R-EKR-20)
  SPRAWDZ(podsumowanie::budujS7("2026-12-01", k, Tryb::ASVAUTO, false).zdanie.empty());
}

static void testS8() {
  Norma n;
  n.ahi.n = n.wp95.n = n.m01.n = n.ca.n = 30;
  n.ahi.p10 = 0.8; n.ahi.p90 = 2.1;
  n.wp95.p10 = 6; n.wp95.p90 = 12;
  n.m01.p10 = 372; n.m01.p90 = 456;
  n.ca.p10 = 0.2; n.ca.p90 = 0.9;
  podsumowanie::WejscieS8 w;
  w.norma = &n;
  w.tryb = Tryb::AUTOSET;
  podsumowanie::DaneS8 d = podsumowanie::budujS8(w);
  SPRAWDZ(d.wiersze.size() == 4);
  ROWNE(d.wiersze[0], "AHI 0,8–2,1 / h");
  ROWNE(d.wiersze[1], "Wyciek 6–12 l/min");
  ROWNE(d.wiersze[2], "Czas 6 h 12–7 h 36");
  ROWNE(d.wiersze[3], "Centralne 0,2–0,9 / h");
  ROWNE(d.krok, "Odchylenia liczę od tego.");
  SPRAWDZ(!d.informacja);
  n.ahi.p10 = 1.2; n.ahi.p90 = 5.4;  // przypadek 35 w 7.8
  d = podsumowanie::budujS8(w);
  ROWNE(d.krok, "→ Pasmo AHI wysokie. Obserwuj kolejne noce.");
  SPRAWDZ(d.informacja);
  n.ahi.p10 = 1.5; n.ahi.p90 = 4.2; n.ca.p10 = 0.8; n.ca.p90 = 2.5;
  ROWNE(podsumowanie::budujS8(w).krok, "Odchylenia liczę od tego.");  // przed
  w.p199 = true;
  ROWNE(podsumowanie::budujS8(w).krok, "→ Pasmo centralnych wysokie. Obserwuj kolejne noce.");
  w.tryb = Tryb::ASVAUTO;
  ROWNE(podsumowanie::budujS8(w).krok, "Odchylenia liczę od tego.");
  w.wzorzecL = true;
  ROWNE(podsumowanie::budujS8(w).krok, "→ Otwarte sprawy: za stroną 2 / 3.");  // numer strony kokpitu (R-KOK-23)
  n.ca.n = 0;  // bez normy CA/h: bez wiersza `Centralne`
  n.wp95.n = 0;
  d = podsumowanie::budujS8(w);
  SPRAWDZ(d.wiersze.size() == 3 && !d.centralne);
  ROWNE(d.wiersze[1], "Wyciek: uczę się");
  w.profilPubliczny = true;
  d = podsumowanie::budujS8(w);
  ROWNE(d.wiersze[0], "AHI 1,5–4,2 / h");  // AHI w obu profilach
  // od ADR-0041 S8 jest tylko ekranem dnia (kamień milowy R7): tytuł `Twoja norma`, stopka standardowa (R-EKR-34)
  ROWNE(d.tytul, "Twoja norma");
  SPRAWDZ(podsumowanie::trescS8(d).stopkaStandardowa);
  n.ahi.n = 20;
  SPRAWDZ(podsumowanie::kandydatS8(n, false) == false);  // norma AHI bez 30 nocy
  n.ahi.n = n.m01.n = 30;
  SPRAWDZ(podsumowanie::kandydatS8(n, false) && !podsumowanie::kandydatS8(n, true));
  podsumowanie::DaneLiczb l = podsumowanie::budujLiczby(Tryb::AUTOSET, false);
  SPRAWDZ(l.wiersze.size() == 5);
  SPRAWDZ(podsumowanie::budujLiczby(Tryb::AUTOSET, true).wiersze.size() == 6);
  SPRAWDZ(podsumowanie::budujLiczby(Tryb::ASV, true).wiersze.size() == 7);
  ROWNE(l.zdanie, "Co jest dobre dla ciebie, powie lekarz.");
  // R-KOK-22, 5.5.7 „Stopki stron”: po K2, ▶ „Otwarte sprawy” albo U-01
  ROWNE(l.stopkaLewo, "◀ 7 nocy   ▶ ustawienia   ↻ ekran dnia");
  ROWNE(podsumowanie::budujLiczby(Tryb::AUTOSET, false, true).stopkaLewo, "◀ 7 nocy   ▶ sprawy   ↻ ekran dnia");
}

static void testNawigacja() {
  KontekstNaw k;
  // od ADR-0041 S8 nie jest stroną kokpitu (R-KOK-19): K2 ▶ U-01 bez spraw i profilu; S8 tylko ekranem dnia (R-EKR-33)
  SPRAWDZ(nacisniecie(Ekran::K1, Przycisk::LEWY, true, k).akcja == Akcja::NIC);
  SPRAWDZ(nacisniecie(Ekran::K2, Przycisk::PRAWY, false, k).ekran == Ekran::USTAWIENIA);
  for (Przycisk pr : {Przycisk::LEWY, Przycisk::SRODEK, Przycisk::PRAWY})  // dawna strona: każdy przycisk = ekran dnia
    SPRAWDZ(nacisniecie(Ekran::NORMA, pr, false, k).ekran == Ekran::DZIEN);
  k.profilPubliczny = true;
  SPRAWDZ(nacisniecie(Ekran::K2, Przycisk::PRAWY, false, k).ekran == Ekran::LICZBY);  // R-KOK-22
  SPRAWDZ(nacisniecie(Ekran::LICZBY, Przycisk::LEWY, false, k).ekran == Ekran::K2);
  SPRAWDZ(nacisniecie(Ekran::LICZBY, Przycisk::SRODEK, false, k).ekran == Ekran::DZIEN);
  SPRAWDZ(sekundDoPowrotu(Ekran::LICZBY, 0, 8 * 60) == 300 && sekundDoPowrotu(Ekran::LICZBY, 300, 8 * 60) == 0);
}

// R-CZY-07, R-CZY-15, R-CZY-01 (wzorzec A), R-CZY-17 (prośba) przez poranekCzynnikow() z okolicznościami ranka
static void testPoranekOkolicznosci() {
  std::vector<Noc> h;
  std::string d = "2026-09-01";
  for (int i = 0; i < 40; i++, d = dodajDni(d, 1)) h.push_back(noc(d, 1.0));
  std::string dzis = h.back().data;
  h.back().ahi = 4.5f;  // odchylenie w jednej nocy w M-02 (R-CZY-01)
  Norma nm = liczNorme(h, dzis, {});
  auto war = [&](const OkolicznosciRanka& okr) {
    std::vector<WpisCzynnikow> cz;
    poranekCzynnikow(cz, h, dzis, 6 * 60, nm, 1, okr);
    return cz.empty() ? '-' : cz[0].warstwa;
  };
  OkolicznosciRanka okr;
  SPRAWDZ(war(okr) == 'O');
  okr.wzorzecA = true;  // R-CZY-01: niewyciszony wzorzec A → tylko na żądanie
  SPRAWDZ(war(okr) == '-');
  okr = OkolicznosciRanka();
  okr.sk30 = true;  // R-CZY-15
  SPRAWDZ(war(okr) == '-');
  okr = OkolicznosciRanka();
  okr.prosbaNaEkranie = true;  // R-CZY-17: ekranem dnia jest prośba
  SPRAWDZ(war(okr) == '-');
  h.back().ahi = 1.0f;  // bez odchylenia: S-K-17 daje zaproszenie poza warstwami O-12 (R-CZY-07)
  nm = liczNorme(h, dzis, {});
  okr = OkolicznosciRanka();
  okr.sk17 = true;
  SPRAWDZ(war(okr) == 'Z');
}

// R-CZY-22, R-CZY-23 przez dyrygent: pytanie przed ekranem dnia, ◀ „nie” zdejmuje oznaczenie, ◀ 2 s cofa
static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900, t.tm_mon = mi - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static std::string raportNocy(long day) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static reguly::Zestaw ZESTAW_W = atrapa_b5::zestaw();
static std::vector<reguly::Wynik> wynikiW(const reguly::Kontekst&) {  // nic spełnionego: bez ekranu L
  std::vector<reguly::Wynik> w;
  for (auto& k : ZESTAW_W.katalog) w.push_back(atrapa_b5::wynik(ZESTAW_W, k.id, k.wariant, true, false));
  return w;
}
static void ranoW(Panel& p, int h, int m, PowodWybudzenia pw, uint8_t bit = 0,
                  std::vector<std::pair<uint32_t, uint8_t>> skrypt = {}) {
  p.port.log.clear();
  p.zegar.epoch = ep(2026, 11, 4, h, m);
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = pw;
  p.start.budzace = bit;
  if (!skrypt.empty()) p.przyciski.skrypt = skrypt;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  long doba = nrDnia("2026-11-04") - 1;
  p.siec.dodaj("/api/status", 200,
               "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\","
               "\"system\":\"IDLE\",\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}");
  p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[{\"day\":" + std::to_string(doba) +
                                         ",\"session_count\":1,\"stale\":false}]}");
  p.siec.dodaj(sciezkaNocy(doba), 200, raportNocy(doba));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static bool wLoguW(const Panel& p, const std::string& f) {
  for (auto& l : p.port.log)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static int wpisowA(Panel& p) {
  int n = 0;
  const std::string& s = p.pliki.pliki["/dane/czynniki.csv"];
  for (size_t i = 0; (i = s.find(";A;", i)) != std::string::npos; i++) n++;
  return n;
}
static void testWyjazdDyrygent() {
  sprawy::zrodloTestowe.wyniki = wynikiW;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_W;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_W);
  long zapis = nrDnia("2026-11-03");
  std::string plik = "#cpap-eink czynniki 1\ndata;stan;kody;warstwa;termin;pokazano\n";
  for (const char* d : {"2026-10-31", "2026-11-01", "2026-11-02"})
    plik += std::string(d) + ";O;P;A;" + std::to_string(zapis) + ";0\n";
  plik += "#koniec 3\n";
  {
    Panel p;
    p.pliki.pliki["/dane/czynniki.csv"] = plik;
    ranoW(p, 7, 0, PowodWybudzenia::ZEGAR);
    SPRAWDZ(wLoguW(p, "ekran dnia: pytanie wyjazd"));  // R-CZY-22: przed ekranem dnia
    SPRAWDZ(wpisowA(p) == 3);
    // ◀ = „nie” (R-CZY-23): oznaczenie zdjęte; ◀ 2 s na „Zapisano” w 60 s cofa (R-PRZ-11)
    ranoW(p, 7, 5, PowodWybudzenia::PRZYCISK, BIT_LEWY, {{0, BIT_LEWY}, {300, 0}});
    SPRAWDZ(wLoguW(p, "zdjęte oznaczenie z 3 nocy"));
    SPRAWDZ(wpisowA(p) == 0);
    SPRAWDZ(p.pliki.pliki["/dane/odpowiedzi.csv"].find("2026-10-31;wyjazd;nie") != std::string::npos);
  }
  {
    Panel p;
    p.pliki.pliki["/dane/czynniki.csv"] = plik;
    ranoW(p, 7, 0, PowodWybudzenia::ZEGAR);
    ranoW(p, 7, 5, PowodWybudzenia::PRZYCISK, BIT_LEWY, {{0, BIT_LEWY}, {300, 0}, {3000, BIT_LEWY}, {5400, 0}});
    SPRAWDZ(wLoguW(p, "zdjęte oznaczenie z 3 nocy"));
    SPRAWDZ(wLoguW(p, "odpowiedź cofnięta"));
    SPRAWDZ(wpisowA(p) == 3);
  }
  {  // ten sam ranek z odpowiedzią „gorzej”: pytanie o wyjazd czeka do jutra (R-CZY-22)
    Panel p;
    p.pliki.pliki["/dane/czynniki.csv"] = plik;
    p.pliki.pliki["/dane/odpowiedzi.csv"] = "#cpap-eink odpowiedzi 1\ndata;pytanie;odpowiedz\n2026-11-04;gorzej;nie\n#koniec 1\n";
    ranoW(p, 7, 0, PowodWybudzenia::ZEGAR);
    SPRAWDZ(!wLoguW(p, "pytanie wyjazd"));
  }
  {  // termin R-PIE-13: bez odpowiedzi do 11:00 = „brak odpowiedzi”, oznaczenie zostaje, pytania już nie ma
    Panel p;
    p.pliki.pliki["/dane/czynniki.csv"] = plik;
    ranoW(p, 7, 0, PowodWybudzenia::ZEGAR);
    ranoW(p, 11, 30, PowodWybudzenia::ZEGAR);
    SPRAWDZ(!wLoguW(p, "ekran dnia: pytanie wyjazd"));
    SPRAWDZ(p.pliki.pliki["/dane/odpowiedzi.csv"].find("2026-10-31;wyjazd;brak") != std::string::npos);
    SPRAWDZ(wpisowA(p) == 3);
  }
  {  // import nocy po 09:00: termin import + 2 h, więc o 12:00 pytanie jest (R-PIE-13)
    Panel p;
    p.pliki.pliki["/dane/czynniki.csv"] = plik;
    ranoW(p, 12, 0, PowodWybudzenia::ZEGAR);
    SPRAWDZ(wLoguW(p, "ekran dnia: pytanie wyjazd"));
  }
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

struct PlotnoT : Plotno {
  int czarnych = 0;
  void piksel(int x, int y, bool c) override { if (c && x >= 0 && x < 800 && y >= 0 && y < 480) czarnych++; }
};
static void testRysowanie() {
  Naglowek n{"wt 01.12", "dane 05:49"};
  std::vector<Noc> h;
  std::string d = "2026-10-01";
  for (int i = 0; i < 61; i++, d = dodajDni(d, 1)) h.push_back(noc(d, 1.0));
  podsumowanie::DaneS7 s = podsumowanie::budujS7("2026-12-01", h, Tryb::ASVAUTO, false);
  PlotnoT p;
  podsumowanie::rysujS7(p, n, s, podsumowanie::trescS7(s));
  SPRAWDZ(p.czarnych > 1000);
  podsumowanie::DaneLiczb l = podsumowanie::budujLiczby(Tryb::ASV, true);
  for (auto& w : l.wiersze) SPRAWDZ(szer(F_P24B, w.nazwa) + szer(F_P22B, w.opis) <= 744);  // wiersze mieszczą się
  PlotnoT q;
  podsumowanie::rysujLiczby(q, n, l);
  SPRAWDZ(q.czarnych > 1000);
  // R-CZY-20, 5.5.2: K1 z ekranu O-12 z ukrytym czynnikiem ma tytuł i powód pod nagłówkiem
  DaneK1 k1 = budujK1(h, h.back().data, liczNorme(h, h.back().data, {}), {}, nullptr);
  PlotnoT a, b;
  rysujK1(a, k1);
  k1.odkrycieTytul = "Po alkoholu: wyższe AHI";
  k1.odkryciePowod = "AHI 4,1 / h po alkoholu, 1,8 / h bez. Z 6/41 nocy. Może to przypadek.";
  rysujK1(b, k1);
  SPRAWDZ(a.czarnych != b.czarnych);
  SPRAWDZ(zawin(F_P22B, k1.odkryciePowod, 760).size() <= 2);
}

int main() {
  setenv("TZ", "Europe/Warsaw", 1);
  tzset();
  odkrycia::zarejestrujRodzajeO();
  testCzynnikiWyzwalacz();
  testWyjazd();
  testRaport();
  testO12Wagi();
  testO12Odkrycie();
  testO12Leki();
  testLosowyCzynnik();
  testO03O04();
  testO08Weekend();
  testZablokowane();
  testWybor();
  testS7();
  testS8();
  testNawigacja();
  testRysowanie();
  testPoranekOkolicznosci();
  testWyjazdDyrygent();
  printf("testy_czynniki_odkrycia: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
