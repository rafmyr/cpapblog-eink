// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: nowa nawigacja panelu (spec 5.6.1,
// 5.6.2). Jedna reguła: ▶ dalej / więcej / „tak”, ◀ wstecz / „nie”, ↻ ekran dnia; przytrzymanie tylko zapisuje (albo
// uruchamia usługę z Wi-Fi: raport ogólny, sieć panelu), nigdy nie otwiera strony; kokpit to ciąg stron K1 … U-01
// z numerem `n / 3` (5.5.7, R-KOK-19); jedyny wyjątek to siatka „Co było inaczej?”. Uruchomienie: tools/prototyp/testy.sh (testy_nawigacja).
#include <cstdio>
#include <ctime>
#include <set>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "nawigacja.h"
#include "podsumowanie.h"
#include "sprzet.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

static const Przycisk P3[3] = {Przycisk::LEWY, Przycisk::PRAWY, Przycisk::SRODEK};
static const Ekran WSZYSTKIE[] = {Ekran::DZIEN, Ekran::K1, Ekran::K2, Ekran::PYTANIE, Ekran::TEST, Ekran::OMOWIONE,
                                  Ekran::SPRAWY, Ekran::PRZENIESIONE, Ekran::ZAPISANO, Ekran::RAPORT, Ekran::NORMA,
                                  Ekran::LICZBY, Ekran::SPRZET, Ekran::SPRZET_ZAPISANO, Ekran::USTAWIENIA,
                                  Ekran::FORMULARZ, Ekran::SIEC_PANELU, Ekran::AKTUALIZACJA};
static const Ekran STRONY[] = {Ekran::K1, Ekran::K2, Ekran::SPRAWY, Ekran::LICZBY, Ekran::USTAWIENIA, Ekran::FORMULARZ,
                               Ekran::TEST};

// Warianty kontekstu: zwykły ekran dnia, S5, S9, L lekarz, S3, pytanie tak/nie, S-K-31, z zaproszeniem, profil
// publiczny, bez spraw, sprawy w środku listy, bez cofnięcia, blokada omówienia.
static std::vector<KontekstNaw> konteksty() {
  std::vector<KontekstNaw> w;
  KontekstNaw k;
  k.sprawyDostepne = true;
  k.spraw = 3;
  k.ostatniaNoc = true;
  w.push_back(k);
  KontekstNaw x = k; x.s5 = true; w.push_back(x);
  x = k; x.nocny = true; w.push_back(x);
  x = k; x.wzorzec = true; w.push_back(x);
  x = k; x.wzorzec = x.problem = true; w.push_back(x);
  x = k; x.pytanieTN = true; w.push_back(x);
  x = k; x.sk31 = true; x.zaproszenie = true; w.push_back(x);
  x = k; x.zaproszenie = true; w.push_back(x);
  x = k; x.profilPubliczny = true; w.push_back(x);
  x = k; x.sprawyDostepne = false; x.spraw = 0; w.push_back(x);
  x = k; x.sprawa = 2; w.push_back(x);
  x = k; x.cofniecie = false; w.push_back(x);
  x = k; x.omowienieZablokowane = true; w.push_back(x);
  x = k; x.zrodlo = Ekran::K2; w.push_back(x);
  x = k; x.zrodlo = Ekran::TEST; w.push_back(x);
  return w;
}

// ---------- ciąg stron w obie strony (bez dyrygenta) ----------
// Krok ▶ albo ◀ z ekranu `e` z kursorem kart „Otwartych spraw” prowadzonym tak, jak robi to dyrygent.
struct Stan {
  Ekran e = Ekran::DZIEN;
  KontekstNaw k;
};
static void krok(Stan& s, Przycisk p) {
  Przejscie pr = nacisniecie(s.e, p, false, s.k);
  switch (pr.akcja) {
    case Akcja::SPRAWY_OTWORZ: s.k.sprawa = 1; break;
    case Akcja::SPRAWY_OD_KONCA: s.k.sprawa = s.k.spraw > 0 ? s.k.spraw : 1; break;
    case Akcja::SPRAWY_DALEJ: s.k.sprawa++; break;
    case Akcja::SPRAWY_WSTECZ: s.k.sprawa--; break;
    default: break;
  }
  s.e = pr.ekran;
}
static std::string nazwa(Ekran e) {
  static const char* const N[] = {"DZIEN", "K1", "K2", "PYTANIE", "TEST", "OMOWIONE", "SPRAWY", "PRZENIESIONE",
                                  "ZAPISANO", "RAPORT", "NORMA", "LICZBY", "SPRZET", "SPRZET_ZAPISANO", "USTAWIENIA",
                                  "FORMULARZ", "SIEC_PANELU", "AKTUALIZACJA"};
  return N[(int)e];
}

static void testCiagStron() {
  for (bool sprawy : {true, false})
    for (bool pub : {false, true})
      for (int spraw : {0, 1, 3}) {
        if (sprawy != (spraw > 0)) continue;  // R-KOK-20: strona „Otwarte sprawy” tylko przy co najmniej jednej sprawie
        Stan s;
        s.k.sprawyDostepne = sprawy;
        s.k.profilPubliczny = pub;
        s.k.spraw = sprawy ? spraw : 0;
        std::vector<std::string> naprzod;
        for (int i = 0; i < 30 && !(s.e == Ekran::USTAWIENIA && i > 0); i++) {
          naprzod.push_back(nazwa(s.e));
          krok(s, Przycisk::PRAWY);
        }
        naprzod.push_back(nazwa(s.e));
        // oczekiwany ciąg ▶ (R-KOK-19): dzień, K1, K2, [liczby], sprawy (każda karta), U-01; U-03 i U-05 tylko przez
        // ▶ 2 s (Wi-Fi), krótkie ▶ na U-01 kończy ciąg przeglądania; bez S8 i „Sprzętu”
        std::vector<std::string> ocz = {"DZIEN", "K1", "K2"};
        if (pub) ocz.push_back("LICZBY");
        for (int c = 0; c < spraw; c++) ocz.push_back("SPRAWY");
        ocz.push_back("USTAWIENIA");
        SPRAWDZ(naprzod == ocz);
        if (naprzod != ocz) {
          for (auto& n : naprzod) printf(" %s", n.c_str());
          printf("\n");
        }
        // ▶ na U-01 i U-03 nic, ▶ 2 s: U-03 i U-05 (usługi z Wi-Fi); ▶ na U-05 odświeża, nie przechodzi dalej
        SPRAWDZ(nacisniecie(Ekran::USTAWIENIA, Przycisk::PRAWY, false, s.k).akcja == Akcja::NIC);
        SPRAWDZ(nacisniecie(Ekran::USTAWIENIA, Przycisk::PRAWY, true, s.k).ekran == Ekran::FORMULARZ);
        SPRAWDZ(nacisniecie(Ekran::FORMULARZ, Przycisk::PRAWY, false, s.k).akcja == Akcja::NIC);
        SPRAWDZ(nacisniecie(Ekran::FORMULARZ, Przycisk::PRAWY, true, s.k).ekran == Ekran::TEST);
        SPRAWDZ(nacisniecie(Ekran::TEST, Przycisk::PRAWY, false, s.k).akcja == Akcja::PONOW_POBRANIE);
        SPRAWDZ(nacisniecie(Ekran::TEST, Przycisk::LEWY, false, s.k).ekran == Ekran::USTAWIENIA);
        SPRAWDZ(nacisniecie(Ekran::FORMULARZ, Przycisk::LEWY, false, s.k).ekran == Ekran::USTAWIENIA);
        // wstecz od U-01 dokładne odwrócenie ciągu do ekranu dnia (z U-03 i U-05 ◀ wraca do U-01, sprawdzone wyżej)
        std::vector<std::string> wstecz;
        for (int i = 0; i < 30 && s.e != Ekran::DZIEN; i++) {
          krok(s, Przycisk::LEWY);
          wstecz.push_back(nazwa(s.e));
        }
        std::vector<std::string> odwr;  // ciąg naprzód odwrócony, bez U-01 (stoimy na nim)
        for (auto it = ocz.rbegin() + 1; it != ocz.rend(); ++it) odwr.push_back(*it);
        SPRAWDZ(wstecz == odwr);
        if (wstecz != odwr) {
          for (auto& n : wstecz) printf(" %s", n.c_str());
          printf("  <- wstecz\n");
        }
        SPRAWDZ(s.e == Ekran::DZIEN);
        // ◀ na ekranie dnia: nic (nie ma „wstecz” od ekranu dnia)
        SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, false, s.k).akcja == Akcja::NIC);
      }
  // numery stron w nagłówkach
  // numery stron w nagłówkach (R-KOK-19): `n / 3`; „Otwarte sprawy” i „Co znaczą te liczby” bez numeru
  SPRAWDZ(numerStronyTekst(Ekran::K1) == "1 / 3" && numerStronyTekst(Ekran::K2) == "2 / 3");
  SPRAWDZ(numerStronyTekst(Ekran::SPRAWY).empty() && numerStronyTekst(Ekran::NORMA).empty());
  SPRAWDZ(numerStronyTekst(Ekran::LICZBY).empty() && numerStronyTekst(Ekran::SPRZET).empty());
  SPRAWDZ(numerStronyTekst(Ekran::USTAWIENIA) == "3 / 3" && numerStronyTekst(Ekran::DZIEN).empty());
  SPRAWDZ(stronaKokpitu(Ekran::SPRAWY) && stronaKokpitu(Ekran::LICZBY) && !stronaKokpitu(Ekran::NORMA) &&
          !stronaKokpitu(Ekran::SPRZET) && !stronaKokpitu(Ekran::FORMULARZ));
  SPRAWDZ(numerStronyTekst(Ekran::TEST).empty() && numerStronyTekst(Ekran::RAPORT).empty());
}

// ---------- ↻ = ekran dnia z każdego ekranu ----------
static void testZielonyDom() {
  for (const KontekstNaw& k : konteksty())
    for (Ekran e : WSZYSTKIE) {
      Przejscie pr = nacisniecie(e, Przycisk::SRODEK, false, k);
      if (e == Ekran::PYTANIE) {  // jedyny wyjątek: siatka „Co było inaczej?” (↻ zaznacza)
        SPRAWDZ(pr.ekran == Ekran::PYTANIE && pr.akcja == Akcja::PYT_ZAZNACZ);
        continue;
      }
      if (e == Ekran::DZIEN) {  // na ekranie dnia: nic, pomiń (pytanie tak/nie) albo ponów pobranie (S5, S10)
        SPRAWDZ(pr.ekran == Ekran::DZIEN);
        // na L i S3 krótkie ↻ rysuje ekran dnia od nowa (widoczna reakcja)
        Akcja oczek = k.pytanieTN && !k.nocny ? Akcja::ODP_POMIN
                      : (k.wzorzec && !k.nocny && !k.pytanieTN) ? Akcja::POKAZ
                      : (k.s5 && !k.nocny && !k.pytanieTN && !k.wzorzec) ? Akcja::PONOW_POBRANIE : Akcja::NIC;
        SPRAWDZ(pr.akcja == oczek);
        continue;
      }
      SPRAWDZ(pr.ekran == Ekran::DZIEN);
      if (pr.ekran != Ekran::DZIEN) printf("  ↻ z %s → %s\n", nazwa(e).c_str(), nazwa(pr.ekran).c_str());
      // ↻ niczego nie zapisuje (poza anulowaniem sieci panelu, które przywraca poprzednią sieć)
      SPRAWDZ(pr.akcja == Akcja::POKAZ || pr.akcja == Akcja::SIEC_PANELU_ANULUJ);
    }
}

// ---------- każde przytrzymanie = zapis; żadne nie otwiera strony ----------
static void testPrzytrzymaniaToZapis() {
  const std::set<Akcja> zapisy = {Akcja::WIEM, Akcja::SPRZET_WYMIENIONE, Akcja::COFNIJ_WIEM,
                                  Akcja::COFNIJ_ODP, Akcja::SPRZET_COFNIJ, Akcja::PYT_ZAPISZ, Akcja::PYT_POMIN,
                                  Akcja::OTWORZ_PYTANIE, Akcja::RAPORT_STARSZE, Akcja::SIEC_PANELU_START,
                                  Akcja::AKT_SPRAWDZ};  // usługa z Wi-Fi (R-KONF-43)
  // ekrany, na które przytrzymanie może prowadzić: zapis z potwierdzeniem albo cofnięciem, odpowiedź na pytanie,
  // usługa z Wi-Fi (raport ogólny, sieć panelu); wyjście z ekranu potwierdzenia na ekran dnia
  const std::set<Ekran> celePrzytrzyman = {Ekran::DZIEN, Ekran::PRZENIESIONE, Ekran::OMOWIONE, Ekran::SPRZET_ZAPISANO,
                                           Ekran::PYTANIE, Ekran::RAPORT, Ekran::SIEC_PANELU};
  for (const KontekstNaw& k : konteksty())
    for (Ekran e : WSZYSTKIE)
      for (Przycisk p : P3) {
        Przejscie pr = nacisniecie(e, p, true, k);
        bool zostaje = pr.ekran == e;
        bool wifi = (p == Przycisk::PRAWY && ((e == Ekran::USTAWIENIA && pr.ekran == Ekran::FORMULARZ) ||
                                              (e == Ekran::FORMULARZ && pr.ekran == Ekran::TEST))) ||
                    // aktualizacja z GitHub: ↻ 2 s na U-01 uruchamia sprawdzenie wersji (Wi-Fi), R-KONF-43
                    (p == Przycisk::SRODEK && e == Ekran::USTAWIENIA && pr.ekran == Ekran::AKTUALIZACJA &&
                     pr.akcja == Akcja::AKT_SPRAWDZ);  // usługi z Wi-Fi
        bool cel = celePrzytrzyman.count(pr.ekran) > 0 ||
                   (e == Ekran::SIEC_PANELU && pr.ekran == k.zrodlo) || wifi;
        SPRAWDZ(zostaje || cel);
        if (!(zostaje || cel)) printf("  2 s %d na %s → %s\n", (int)p, nazwa(e).c_str(), nazwa(pr.ekran).c_str());
        // akcja przytrzymania: zapis, nic, albo POKAZ tylko do potwierdzenia „Omówione”, raportu z K2 i wyjścia
        // z ekranów potwierdzeń (zapis już zrobiony, przytrzymanie nie cofa poza swoim przyciskiem)
        bool pokazDozw = pr.akcja == Akcja::POKAZ &&
                         ((e == Ekran::DZIEN && (pr.ekran == Ekran::OMOWIONE || pr.ekran == Ekran::RAPORT)) ||
                          (e == Ekran::K2 && pr.ekran == Ekran::RAPORT) || wifi || e == Ekran::PYTANIE ||
                          e == Ekran::PRZENIESIONE || e == Ekran::ZAPISANO || e == Ekran::SPRZET_ZAPISANO);
        SPRAWDZ(pr.akcja == Akcja::NIC || zapisy.count(pr.akcja) || pokazDozw ||
                (e == Ekran::SIEC_PANELU && pr.akcja == Akcja::SIEC_PANELU_ANULUJ));
        // żadne przytrzymanie nie otwiera strony kokpitu ani ekranu Ustawień (poza anulowaniem U-02 ◀ 2 s, które
        // wraca tam, skąd uruchomiono)
        for (Ekran s : STRONY)
          if (pr.ekran == s && e != s)
            SPRAWDZ((e == Ekran::SIEC_PANELU && s == k.zrodlo) || wifi);
      }
}

// ---------- „Omówione z lekarzem?”: tylko ▶ po wejściu, 60 s bez zmian, także w nocy ----------
static void testOmowione() {
  KontekstNaw k;
  k.wzorzec = true;
  Przejscie w = nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k);  // ◀ 2 s: potwierdzenie, bez zapisu
  SPRAWDZ(w.ekran == Ekran::OMOWIONE && w.akcja == Akcja::POKAZ);
  // ▶ = K1 jak na każdym ekranie dnia, raport ▶ 2 s; żadne z nich nie zapisuje „omówione”
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, k).ekran == Ekran::K1);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, true, k).ekran == Ekran::RAPORT);
  KontekstNaw q;
  for (Przycisk p : P3) SPRAWDZ(nacisniecie(Ekran::OMOWIONE, p, true, q).akcja == Akcja::NIC);  // przytrzymania nic
  SPRAWDZ(nacisniecie(Ekran::OMOWIONE, Przycisk::PRAWY, false, q).akcja == Akcja::OMOWIONE_TAK);
  SPRAWDZ(nacisniecie(Ekran::OMOWIONE, Przycisk::LEWY, false, q).akcja == Akcja::POKAZ);    // nie
  SPRAWDZ(nacisniecie(Ekran::OMOWIONE, Przycisk::SRODEK, false, q).akcja == Akcja::POKAZ);  // dom
  int zapisow = 0;
  for (const KontekstNaw& c : konteksty())
    for (Ekran e : WSZYSTKIE)
      for (Przycisk p : P3)
        for (bool d : {false, true})
          if (nacisniecie(e, p, d, c).akcja == Akcja::OMOWIONE_TAK) {
            zapisow++;
            SPRAWDZ(e == Ekran::OMOWIONE && p == Przycisk::PRAWY && !d && !c.omowienieZablokowane);
          }
  SPRAWDZ(zapisow > 0);
  q.omowienieZablokowane = true;  // R-LEK-26: ▶ nic
  SPRAWDZ(nacisniecie(Ekran::OMOWIONE, Przycisk::PRAWY, false, q).akcja == Akcja::NIC);
  // wygasa po 60 s bez zapisu, także w slocie nocnym (R-PRZ-05)
  for (int m : {9 * 60, 2 * 60, 23 * 60 + 45}) {
    SPRAWDZ(sekundDoPowrotu(Ekran::OMOWIONE, 0, m) == 60 && sekundDoPowrotu(Ekran::OMOWIONE, 59, m) == 1);
    SPRAWDZ(sekundDoPowrotu(Ekran::OMOWIONE, 60, m) == 0);
  }
  // problem (S3): ◀ 2 s nic, ▶ kokpit
  k.problem = true;
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k).akcja == Akcja::NIC);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, k).ekran == Ekran::K1);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, true, k).akcja == Akcja::WIEM);
}

// ---------- raport, U-02, cofnięcia ----------
static void testEkranySzczegolne() {
  KontekstNaw k;
  k.sprawyDostepne = true;
  // raport z L: ◀ wraca na ekran dnia, ▶ dalej (K1); z K2: ◀ do K2, ▶ dalej („Otwarte sprawy” przy sprawie)
  k.zrodlo = Ekran::DZIEN;
  SPRAWDZ(nacisniecie(Ekran::RAPORT, Przycisk::LEWY, false, k).ekran == Ekran::DZIEN);
  SPRAWDZ(nacisniecie(Ekran::RAPORT, Przycisk::PRAWY, false, k).ekran == Ekran::K1);
  k.zrodlo = Ekran::K2;
  SPRAWDZ(nacisniecie(Ekran::RAPORT, Przycisk::LEWY, false, k).ekran == Ekran::K2);
  SPRAWDZ(nacisniecie(Ekran::RAPORT, Przycisk::PRAWY, false, k).ekran == Ekran::SPRAWY);
  SPRAWDZ(nacisniecie(Ekran::RAPORT, Przycisk::SRODEK, true, k).akcja == Akcja::RAPORT_STARSZE);
  SPRAWDZ(nacisniecie(Ekran::K2, Przycisk::PRAWY, true, k).ekran == Ekran::RAPORT);  // raport ogólny (R-LEK-02)
  // U-02: ◀ tam, skąd uruchomiono, ↻ ekran dnia; uruchomienie tylko ◀ 2 s na U-01 i U-05 (albo bez konfiguracji)
  k.zrodlo = Ekran::TEST;
  SPRAWDZ(nacisniecie(Ekran::SIEC_PANELU, Przycisk::LEWY, false, k).ekran == Ekran::TEST);
  SPRAWDZ(nacisniecie(Ekran::SIEC_PANELU, Przycisk::SRODEK, false, k).ekran == Ekran::DZIEN);
  int startow = 0;
  for (const KontekstNaw& c : konteksty())
    for (Ekran e : WSZYSTKIE)
      for (Przycisk p : P3)
        for (bool d : {false, true}) {
          Przejscie pr = nacisniecie(e, p, d, c);
          if (pr.akcja != Akcja::SIEC_PANELU_START) continue;
          startow++;
          SPRAWDZ(d);  // nigdy krótkim naciśnięciem (przeglądanie ▶ nie włącza sieci panelu)
          SPRAWDZ((p == Przycisk::LEWY && (e == Ekran::USTAWIENIA || e == Ekran::TEST)) ||
                  (p == Przycisk::SRODEK && e == Ekran::SIEC_PANELU));
        }
  SPRAWDZ(startow > 0);
  // cofnięcie zawsze ◀ 2 s; krótkie ↻ = ekran dnia, zapis zostaje
  KontekstNaw c;
  SPRAWDZ(nacisniecie(Ekran::PRZENIESIONE, Przycisk::LEWY, true, c).akcja == Akcja::COFNIJ_WIEM);
  SPRAWDZ(nacisniecie(Ekran::ZAPISANO, Przycisk::LEWY, true, c).akcja == Akcja::COFNIJ_ODP);
  SPRAWDZ(nacisniecie(Ekran::SPRZET_ZAPISANO, Przycisk::LEWY, true, c).akcja == Akcja::SPRZET_COFNIJ);
  for (Ekran e : {Ekran::PRZENIESIONE, Ekran::ZAPISANO, Ekran::SPRZET_ZAPISANO}) {
    SPRAWDZ(nacisniecie(e, Przycisk::SRODEK, false, c).ekran == Ekran::DZIEN);
    SPRAWDZ(nacisniecie(e, Przycisk::SRODEK, false, c).akcja == Akcja::POKAZ);
  }
  // siatka „Co było inaczej?”: wyjątek bez zmian
  SPRAWDZ(nacisniecie(Ekran::PYTANIE, Przycisk::LEWY, false, c).akcja == Akcja::PYT_WSTECZ);
  SPRAWDZ(nacisniecie(Ekran::PYTANIE, Przycisk::PRAWY, false, c).akcja == Akcja::PYT_DALEJ);
  SPRAWDZ(nacisniecie(Ekran::PYTANIE, Przycisk::SRODEK, true, c).akcja != Akcja::PYT_ZAPISZ);  // bez ruchu bez zapisu
  c.pytanieRuszone = true;
  SPRAWDZ(nacisniecie(Ekran::PYTANIE, Przycisk::SRODEK, true, c).akcja == Akcja::PYT_ZAPISZ);
  c.pytanieRuszone = false;
  SPRAWDZ(nacisniecie(Ekran::PYTANIE, Przycisk::LEWY, true, c).akcja == Akcja::PYT_POMIN);
  // pytanie tak/nie: ▶ tak, ◀ nie, ↻ pomiń
  KontekstNaw t;
  t.pytanieTN = true;
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, t).akcja == Akcja::ODP_TAK);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, false, t).akcja == Akcja::ODP_NIE);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, t).akcja == Akcja::ODP_POMIN);
}

// ---------- noc: ekran nocny i „sam wraca” bez zmian ----------
static void testNoc() {
  KontekstNaw k;
  k.nocny = true;
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, k).ekran == Ekran::K1);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, false, k).akcja == Akcja::NIC);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, k).akcja == Akcja::NIC);
  for (Przycisk p : P3) SPRAWDZ(nacisniecie(Ekran::DZIEN, p, true, k).akcja == Akcja::NIC);
  SPRAWDZ(nacisniecie(Ekran::K1, Przycisk::SRODEK, false, k).ekran == Ekran::DZIEN);  // znowu S9
  for (Ekran e : STRONY) {
    if (e == Ekran::TEST) continue;
    SPRAWDZ(sekundDoPowrotu(e, 0, 2 * 60) == -1);    // w nocy strony zostają do 05:00 (R-PRZ-18)
    SPRAWDZ(sekundDoPowrotu(e, 0, 10 * 60) == 300);  // w dzień 5 min (R-PRZ-02)
  }
  SPRAWDZ(slotNocny(23 * 60 + 30) && !slotNocny(5 * 60));
}

// ---------- stopki: każdy przycisk podpisany, mieszczą się w 744 px ----------
static bool maGrupe(const std::vector<std::string>& g, const char* ikona) {
  for (auto& s : g)
    if (s.rfind(ikona, 0) == 0) return true;
  return false;
}
static void testStopki() {
  std::vector<std::vector<std::string>> st = {stopkaK1(false), stopkaK1(true), stopkaK2(true), stopkaK2(false),
                                              stopkaTestu(false)};
  for (int n : {0, 1, 3})
    for (int i = 1; i <= std::max(1, n); i++)
      for (bool l : {false, true}) st.push_back(stopkaSpraw(i, n, l));
  for (bool spr : {false, true})
    for (bool l : {false, true}) {
      st.push_back(stopkaK2(spr, l));
      st.push_back(stopkaU01(spr, l));
    }
  for (auto& g : st) {
    SPRAWDZ(maGrupe(g, "◀") && maGrupe(g, "▶") && maGrupe(g, "↻"));
    SPRAWDZ(szerStopki(g) <= 744);
  }
  // 5.5.7 „Stopki stron”: podpis celu według ciągu stron
  SPRAWDZ(stopkaK2(true)[1] == "▶ sprawy · 2 s raport" && stopkaK2(false)[1] == "▶ ustawienia · 2 s raport");
  SPRAWDZ(stopkaK2(false, true)[1] == "▶ liczby · 2 s raport" && stopkaK2(true, true)[1] == "▶ liczby · 2 s raport");
  SPRAWDZ(stopkaSpraw(1, 3)[0] == "◀ 7 nocy" && stopkaSpraw(3, 3)[1] == "▶ ustawienia");
  SPRAWDZ(stopkaSpraw(1, 3, true)[0] == "◀ liczby" && stopkaSpraw(2, 3, true)[0] == "◀ poprzednia");
  SPRAWDZ(stopkaU01(true)[0] == "◀ sprawy · 2 s sieć" && stopkaU01(false)[0] == "◀ 7 nocy · 2 s sieć");
  SPRAWDZ(stopkaU01(false, true)[0] == "◀ liczby · 2 s sieć" && stopkaU01(true, true)[0] == "◀ sprawy · 2 s sieć");
  SPRAWDZ(stopkaWzorca(false) == std::vector<std::string>({"▶ kokpit", "↻ 2 s wiem"}));
  for (bool spr : {false, true}) {
    std::string s = podsumowanie::budujLiczby(reguly::Tryb::ASV, true, spr).stopkaLewo;
    SPRAWDZ(s.find("◀") != std::string::npos && s.find("▶") != std::string::npos && s.find("↻") != std::string::npos);
    SPRAWDZ(szerStopki({s}) <= 744);
  }
}

// ---------- dyrygent: cały ciąg ▶ od ekranu dnia do U-05 i z powrotem ◀, ↻ z każdej strony ----------
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t ep(int h, int m, int s = 0) {
  struct tm t = {};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 10;
  t.tm_mday = 4;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  return (uint32_t)timegm(&t);
}
static void przycisk(Panel& p, uint32_t kiedy, uint8_t bit, bool dlugo = false) {
  p.zegar.epoch = kiedy;
  p.zegar.msTeraz = 0;
  p.d.z.clear();
  p.sen.snow = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = {{0, bit}, {dlugo ? 2500u : 300u, 0}};
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static Ekran ekran(const Panel& p) { return (Ekran)p.start.stan.ekran; }

static void testDyrygentCiag() {
  Panel p;
  p.zegar.epoch = ep(10, 0);
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.pliki.pliki["/konfiguracja.txt"] = "";
  uint32_t t = ep(10, 0);
  std::vector<Ekran> naprzod = {ekran(p)};
  for (int i = 0; i < 20 && ekran(p) != Ekran::USTAWIENIA; i++) {
    przycisk(p, t += 30, BIT_PRAWY);
    naprzod.push_back(ekran(p));
  }
  SPRAWDZ(ekran(p) == Ekran::USTAWIENIA);
  // krótkie ▶ na U-01 nic (bez Wi-Fi), ▶ 2 s otwiera U-03, na U-03 krótkie ▶ nic, ▶ 2 s otwiera U-05
  przycisk(p, t += 30, BIT_PRAWY);
  SPRAWDZ(ekran(p) == Ekran::USTAWIENIA);
  przycisk(p, t += 30, BIT_PRAWY, true);
  SPRAWDZ(ekran(p) == Ekran::FORMULARZ);
  przycisk(p, t += 30, BIT_PRAWY);
  SPRAWDZ(ekran(p) == Ekran::FORMULARZ);
  przycisk(p, t += 30, BIT_PRAWY, true);
  SPRAWDZ(ekran(p) == Ekran::TEST);
  przycisk(p, t += 30, BIT_LEWY);
  SPRAWDZ(ekran(p) == Ekran::USTAWIENIA);
  // kolejne strony w kolejności numerów 1 / 3 … 3 / 3 (R-KOK-19); bez S8 i „Sprzętu”
  std::vector<int> nr;
  for (Ekran e : naprzod)
    if (numerStrony(e) && (nr.empty() || nr.back() != numerStrony(e))) nr.push_back(numerStrony(e));
  SPRAWDZ(nr.size() == 3 && nr.front() == 1 && nr.back() == 3);
  for (size_t i = 1; i < nr.size(); i++) SPRAWDZ(nr[i] == nr[i - 1] + 1);
  for (Ekran e : naprzod) SPRAWDZ(e != Ekran::SPRZET && e != Ekran::NORMA);
  // ◀ z U-05: U-01, potem wstecz aż do ekranu dnia
  std::vector<Ekran> wstecz;
  for (int i = 0; i < 20 && ekran(p) != Ekran::DZIEN; i++) {
    przycisk(p, t += 30, BIT_LEWY);
    wstecz.push_back(ekran(p));
  }
  SPRAWDZ(ekran(p) == Ekran::DZIEN);
  SPRAWDZ(!wstecz.empty() && (wstecz.front() == Ekran::K2 || wstecz.front() == Ekran::SPRAWY));
  // od U-01 wstecz to dokładnie ciąg naprzód od U-01 odwrócony (bez U-03 i U-05)
  std::vector<Ekran> ocz;
  for (auto it = naprzod.rbegin() + 1; it != naprzod.rend(); ++it) ocz.push_back(*it);  // bez U-01, na którym stoimy
  SPRAWDZ(wstecz == ocz);
  // ↻ z każdej strony: ekran dnia
  for (Ekran e : STRONY) {
    if (e == Ekran::LICZBY) continue;  // tylko w profilu publicznym
    p.start.stan.ekran = (uint8_t)e;
    przycisk(p, t += 30, BIT_SRODEK);
    SPRAWDZ(ekran(p) == Ekran::DZIEN);
    if (ekran(p) != Ekran::DZIEN) printf("  ↻ z %s → %s\n", nazwa(e).c_str(), nazwa(ekran(p)).c_str());
  }
  // przytrzymanie ◀ i ▶ w K1: nic (S8 i K2 nie przytrzymaniem)
  p.start.stan.ekran = (uint8_t)Ekran::K1;
  przycisk(p, t += 30, BIT_LEWY, true);
  SPRAWDZ(ekran(p) == Ekran::K1);
  przycisk(p, t += 30, BIT_PRAWY, true);
  SPRAWDZ(ekran(p) == Ekran::K1);
}

// ---------- „wymienione dziś” przez ekran z cofnięciem, przycięcie karty spraw ----------
static std::string konfiguracja(const Panel& p) {
  auto i = p.pliki.pliki.find("/konfiguracja.txt");
  return i == p.pliki.pliki.end() ? "" : i->second;
}
static void testWymianaZCofnieciem() {
  Panel p;
  p.zegar.epoch = ep(10, 0);
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.pliki.pliki["/konfiguracja.txt"] = "wymiana.filtr=2026-10-01\n";
  p.nvs.liczbyU["cpap/b16_sk31"] = EL_FILTR + 1;  // ekran dnia S-K-35 „Wymiana: filtr.” (R-MAS-18)
  p.start.stan.ekran = (uint8_t)Ekran::DZIEN;
  uint32_t t = ep(10, 0);
  przycisk(p, t += 30, BIT_LEWY, true);  // ◀ 2 s: zapis i ekran z cofnięciem, nie cichy zapis
  SPRAWDZ(ekran(p) == Ekran::SPRZET_ZAPISANO);
  SPRAWDZ(konfiguracja(p).find("wymiana.filtr=2026-11-04") != std::string::npos);
  przycisk(p, t += 10, BIT_LEWY, true);  // ◀ 2 s na ekranie potwierdzenia: data sprzed zapisu, ekran dnia
  SPRAWDZ(ekran(p) == Ekran::DZIEN);
  SPRAWDZ(konfiguracja(p).find("wymiana.filtr=2026-10-01") != std::string::npos);
  SPRAWDZ(konfiguracja(p).find("2026-11-04") == std::string::npos);
  // ↻ z ekranu potwierdzenia zostawia zapis
  p.nvs.liczbyU["cpap/b16_sk31"] = EL_FILTR + 1;
  przycisk(p, t += 30, BIT_LEWY, true);
  SPRAWDZ(ekran(p) == Ekran::SPRZET_ZAPISANO);
  przycisk(p, t += 10, BIT_SRODEK);
  SPRAWDZ(ekran(p) == Ekran::DZIEN);
  SPRAWDZ(konfiguracja(p).find("wymiana.filtr=2026-11-04") != std::string::npos);
  // karta spraw przycięta do liczby kart
  SPRAWDZ(przytnijKarte(4, 2) == 2 && przytnijKarte(0, 3) == 1 && przytnijKarte(2, 0) == 1 && przytnijKarte(2, 3) == 2);
}

// ---------- kolejka naciśnięć z czasu rysowania (spec 5.6.1) ----------
// wymaganie: R-PRZ-20
static void testKolejkaNacisniec() {
  // czysta funkcja: tylko krótkie naciśnięcie na ekranie dnia albo stronie kokpitu (także bez numeru)
  SPRAWDZ(zKolejkiWolno(Ekran::DZIEN, false));
  SPRAWDZ(zKolejkiWolno(Ekran::K1, false) && zKolejkiWolno(Ekran::SPRAWY, false) && zKolejkiWolno(Ekran::USTAWIENIA, false));
  SPRAWDZ(zKolejkiWolno(Ekran::LICZBY, false) && !zKolejkiWolno(Ekran::NORMA, false) && !zKolejkiWolno(Ekran::SPRZET, false));
  SPRAWDZ(!zKolejkiWolno(Ekran::K1, true));         // przytrzymanie zapisuje: nie z kolejki
  SPRAWDZ(!zKolejkiWolno(Ekran::PYTANIE, false));   // odpowiedź na niewidziane pytanie: nie
  SPRAWDZ(!zKolejkiWolno(Ekran::OMOWIONE, false));  // potwierdzenie: nie
  SPRAWDZ(!zKolejkiWolno(Ekran::FORMULARZ, false) && !zKolejkiWolno(Ekran::TEST, false));
  // strona Portretu pod ▶ (R-POR-24): krótkie naciśnięcie nic nie zapisuje, więc z kolejki tak; przytrzymanie nie
  SPRAWDZ(zKolejkiWolno(Ekran::PORTRET, false) && !zKolejkiWolno(Ekran::PORTRET, true));
  // 6 szybkich ▶ (budzące i 5 z kolejki) = ten sam ekran co 6 osobnych ▶, przy dwóch odświeżeniach zamiast sześciu
  const int N = 6;
  Panel a;
  a.zegar.epoch = ep(10, 0);
  zerujPrzedSnem(a.start.l, identyfikatorWersji(a.start.napisWersji));
  a.pliki.pliki["/konfiguracja.txt"] = "";
  uint32_t t = ep(10, 0);
  for (int i = 0; i < N; i++) przycisk(a, t += 30, BIT_PRAWY);
  Panel b;
  b.zegar.epoch = ep(10, 0);
  zerujPrzedSnem(b.start.l, identyfikatorWersji(b.start.napisWersji));
  b.pliki.pliki["/konfiguracja.txt"] = "";
  b.przyciski.kolejka.assign(N - 1, {(int)Przycisk::PRAWY, 150u});
  int przed = b.ekran.odswiezen;
  przycisk(b, ep(10, 0) + 30, BIT_PRAWY);
  SPRAWDZ(ekran(b) == ekran(a));
  if (ekran(b) != ekran(a)) printf("  kolejka: %s, osobno: %s\n", nazwa(ekran(b)).c_str(), nazwa(ekran(a)).c_str());
  SPRAWDZ(b.ekran.odswiezen - przed == 2);
  SPRAWDZ(b.przyciski.kolejka.empty());
  // pisk tylko za naciśnięcie budzące: odtwarzane z kolejki nie piszczą (zlewały się po rysowaniu, 06.11.2026)
  SPRAWDZ(b.dzwiek.piskow == 1);
  // przytrzymanie w kolejce: kończy kolejkę, nic po nim się nie wykonuje
  Panel c;
  c.zegar.epoch = ep(10, 0);
  zerujPrzedSnem(c.start.l, identyfikatorWersji(c.start.napisWersji));
  c.pliki.pliki["/konfiguracja.txt"] = "";
  c.przyciski.kolejka = {{(int)Przycisk::PRAWY, 2500u}, {(int)Przycisk::PRAWY, 150u}};
  przycisk(c, ep(10, 0) + 30, BIT_PRAWY);
  SPRAWDZ(ekran(c) == Ekran::K1);
  SPRAWDZ(c.przyciski.kolejka.empty());
}

// ---------- skrót ◀ + ▶ 2 s do testu sprzętu (R-PRZ-21) ----------
// wymaganie: R-PRZ-21
static void skrot(Panel& p, uint32_t kiedy, uint32_t trzymaj) {
  p.zegar.epoch = kiedy;
  p.zegar.msTeraz = 0;
  p.d.z.clear();
  p.sen.snow = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = (uint8_t)(BIT_LEWY | BIT_PRAWY);
  p.przyciski.skrypt = {{0, (uint8_t)(BIT_LEWY | BIT_PRAWY)}, {trzymaj, 0}};
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static void testSkrotTestu() {
  Panel p;
  p.zegar.epoch = ep(10, 0);
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.pliki.pliki["/konfiguracja.txt"] = "";
  uint32_t t = ep(10, 0);
  przycisk(p, t += 30, BIT_SRODEK);  // ekran dnia
  SPRAWDZ(ekran(p) == Ekran::DZIEN);
  skrot(p, t += 30, 2500);  // z ekranu dnia, z uśpienia: test sprzętu
  SPRAWDZ(ekran(p) == Ekran::TEST);
  p.start.stan.ekran = (uint8_t)Ekran::K2;
  skrot(p, t += 30, 2500);  // z dowolnej strony
  SPRAWDZ(ekran(p) == Ekran::TEST);
  p.start.stan.ekran = (uint8_t)Ekran::K1;
  skrot(p, t += 30, 500);  // puszczone przed 2 s: bez akcji (nie ◀ i nie ▶)
  SPRAWDZ(ekran(p) == Ekran::K1);
  przycisk(p, t += 30, BIT_LEWY, true);  // samo ◀ 2 s w K1: nic, jak dotąd
  SPRAWDZ(ekran(p) == Ekran::K1);
}

// ekran z pamięci RTC z kontrolą zakresu
static void testEkranZRtc() {
  SPRAWDZ(ekranZRtc(0) == Ekran::DZIEN && ekranZRtc(9) == Ekran::RAPORT);
  SPRAWDZ(ekranZRtc((unsigned char)Ekran::SIEC_PANELU) == Ekran::SIEC_PANELU);
  SPRAWDZ(ekranZRtc((unsigned char)Ekran::PORTRET) == Ekran::PORTRET);
  SPRAWDZ(ekranZRtc((unsigned char)Ekran::PORTRET + 1) == Ekran::DZIEN && ekranZRtc(255) == Ekran::DZIEN);
  // powrót po uśpieniu (pamięć RTC, R-KOK-19): strony kokpitu `n / 3` i bez numeru wracają same, dawne S8 i „Sprzęt”
  // (obraz sprzed ADR-0041) dają ekran dnia; numery wyliczenia za nimi bez zmian (U-01 = 14)
  SPRAWDZ((int)Ekran::USTAWIENIA == 14 && (int)Ekran::SPRZET_ZAPISANO == 13 && (int)Ekran::LICZBY == 11);
  for (Ekran e : {Ekran::K1, Ekran::K2, Ekran::LICZBY, Ekran::SPRAWY, Ekran::USTAWIENIA}) {
    SPRAWDZ(ekranZRtc((unsigned char)e) == e);
    SPRAWDZ(ekranPoStarcie(PowodWybudzenia::PRZYCISK, ekranZRtc((unsigned char)e), true, 60, 10 * 60) == e);
    SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, ekranZRtc((unsigned char)e), true, 60, 10 * 60) == e);  // przed 5 min
  }
  SPRAWDZ(ekranZRtc((unsigned char)Ekran::NORMA) == Ekran::DZIEN && ekranZRtc((unsigned char)Ekran::SPRZET) == Ekran::DZIEN);
  // przez dyrygenta: obudzony przyciskiem panel z K2 w RTC idzie dalej po ciągu, z dawnym „Sprzęt” od ekranu dnia
  Panel p;
  p.zegar.epoch = ep(10, 0);
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.pliki.pliki["/konfiguracja.txt"] = "";
  p.start.stan.ekran = (uint8_t)Ekran::K2;
  przycisk(p, ep(10, 1), BIT_LEWY);
  SPRAWDZ(ekran(p) == Ekran::K1);
  p.start.stan.ekran = (uint8_t)Ekran::SPRZET;
  przycisk(p, ep(10, 2), BIT_PRAWY);
  SPRAWDZ(ekran(p) == Ekran::K1);  // ekran dnia ▶ K1
}

int main() {
  testCiagStron();
  testEkranZRtc();
  testZielonyDom();
  testPrzytrzymaniaToZapis();
  testOmowione();
  testEkranySzczegolne();
  testNoc();
  testStopki();
  testDyrygentCiag();
  testWymianaZCofnieciem();
  testKolejkaNacisniec();
  testSkrotTestu();
  printf("testy_nawigacja: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
