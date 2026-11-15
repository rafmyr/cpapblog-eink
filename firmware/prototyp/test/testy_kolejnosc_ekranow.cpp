// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: kolejność i widoczność ekranów z telefonu (ADR-0045, decyzja właściciela projektu 14.11.2026;
// spec 5.5.9, 5.6.6, 5.8.12; R-KOK-34…R-KOK-36, R-KON-70…R-KON-72, R-KONF-60; T-TEST-435…T-TEST-437):
// - nawigacja dla wszystkich permutacji trzech stron z numerem × ukrycie sypialni × dane sypialni × sprawy × profil
//   publiczny: ciąg ▶, odwrotny ◀, numery `n / N`, stopki zgodne z celem przycisku, ukryta strona nigdzie w stopce;
// - zgodność z bazą: plik bez nowych kluczy = zachowanie sprzed ADR-0045 (src/nawigacja.cpp i wybierzSlot() z commitu
//   e7bb4d4, test/baza/*.inc) dla każdej pary ekran × przycisk × kontekst i dla losowych wejść slotu dziennego;
// - konfiguracja: wartości uszkodzone, nazwy ekranów zawsze włączonych w `*.ukryte` (ręcznie zmieniony plik) bez skutku;
// - numery stron w tekstach dla dowolnej kolejności, zdejmowanie `Na kontroli: raport n / N.`;
// - slot dzienny z wyłączonymi rodzajami i inną kolejnością; ekrany zawsze włączone;
// - formularz: sekcja „Ekrany”, parsowanie, walidacja, zapis, XSS, działanie bez skryptu;
// - dyrygent na atrapach: ciąg z pliku konfiguracji, powrót z pamięci RTC na stronę ukrytą → ekran dnia.
// Uruchomienie: tools/prototyp/testy.sh (testy_kolejnosc_ekranow).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "formularz.h"
#include "nawigacja.h"
#include "podsumowanie.h"
#include "slot_dzienny.h"
#include "sprawy.h"
#include "start.h"
#include "sypialnia.h"
#include "sypialnia_ekran.h"
#include "uklad_ekranow.h"
#include "wspolne.h"
#include "wybudzenie.h"

namespace baza {
#include "baza/nawigacja_przed_adr0045_h.inc"  // własne typy baza::Ekran, baza::KontekstNaw (bez kolizji z bieżącymi)
#include "baza/nawigacja_przed_adr0045.inc"
#include "baza/wybierz_slot_przed_adr0045.inc"
// kontekst bieżący → kontekst bazy (te same pola)
KontekstNaw zBiezacego(const ::KontekstNaw& k) {
  KontekstNaw b;
  b.s5 = k.s5;
  b.s5Bateria = k.s5Bateria;
  b.zaproszenie = k.zaproszenie;
  b.ostatniaNoc = k.ostatniaNoc;
  b.nocny = k.nocny;
  b.wzorzec = k.wzorzec;
  b.problem = k.problem;
  b.pytanieTN = k.pytanieTN;
  b.sprawa = k.sprawa;
  b.spraw = k.spraw;
  b.cofniecie = k.cofniecie;
  b.pytanieRuszone = k.pytanieRuszone;
  b.sk31 = k.sk31;
  b.sprawyDostepne = k.sprawyDostepne;
  b.profilPubliczny = k.profilPubliczny;
  b.sypialniaDostepna = k.sypialniaDostepna;
  b.zrodlo = static_cast<Ekran>(k.zrodlo);
  b.omowienieZablokowane = k.omowienieZablokowane;
  b.bezKonfiguracji = k.bezKonfiguracji;
  b.aktualizacja = k.aktualizacja;
  return b;
}
}  // namespace baza

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

static bool zawiera(const std::string& s, const std::string& t) { return s.find(t) != std::string::npos; }
static size_t ile(const std::string& s, const std::string& t) {
  size_t n = 0;
  for (size_t p = s.find(t); p != std::string::npos; p = s.find(t, p + t.size())) n++;
  return n;
}

static const StronaPrzestawna K1S = StronaPrzestawna::OSTATNIA_NOC, K2S = StronaPrzestawna::SIEDEM_NOCY,
                              SYS = StronaPrzestawna::SYPIALNIA;
// sześć permutacji trzech stron z numerem
static std::vector<UkladKokpitu> permutacje() {
  std::vector<UkladKokpitu> r;
  StronaPrzestawna a[3] = {K1S, K2S, SYS};
  std::sort(a, a + 3);
  do {
    UkladKokpitu u;
    for (int i = 0; i < 3; i++) u.kolejnosc[i] = a[i];
    r.push_back(u);
  } while (std::next_permutation(a, a + 3));
  return r;
}

// oczekiwany ciąg ▶ (bez ekranu dnia) zbudowany niezależnie od nawigacji: strony z numerem w kolejności układu,
// po K2 „liczby” i „sprawy”, sypialnia tylko z danymi i bez ukrycia, U-01 na końcu
static std::vector<Ekran> oczekiwanyCiag(const UkladKokpitu& u, bool sprawy, bool liczby, bool syp) {
  std::vector<Ekran> c;
  for (StronaPrzestawna s : u.kolejnosc) {
    if (s == K1S) c.push_back(Ekran::K1);
    if (s == K2S) {
      c.push_back(Ekran::K2);
      if (liczby) c.push_back(Ekran::LICZBY);
      if (sprawy) c.push_back(Ekran::SPRAWY);
    }
    if (s == SYS && syp && u.sypialnia) c.push_back(Ekran::SYPIALNIA);
  }
  c.push_back(Ekran::USTAWIENIA);
  return c;
}

// podpis z elementu stopki (`◀ 7 nocy` → `7 nocy`, `▶ sprawy · 2 s raport` → `sprawy`)
static std::string cel(const std::string& el) {
  size_t p = el.find(' ');
  std::string r = p == std::string::npos ? "" : el.substr(p + 1);
  size_t k = r.find(" · ");
  return k == std::string::npos ? r : r.substr(0, k);
}

// ---------- R-KOK-34, R-KOK-35: ciąg, numery i stopki dla wszystkich układów ----------
// wymaganie: R-KOK-34
static void testR_KOK_34_ciagWszystkichUkladow() {
  int ukladow = 0;
  for (UkladKokpitu u : permutacje()) {
    for (bool pokazSyp : {true, false}) {
      u.sypialnia = pokazSyp;
      ustawUkladKokpitu(u);
      for (int maska = 0; maska < 8; maska++) {
        ukladow++;
        const bool sprawy = maska & 1, liczby = maska & 2, dane = maska & 4;
        ustawSypialnieWKokpicie(dane);
        KontekstNaw k;
        k.sprawyDostepne = sprawy;
        k.spraw = sprawy ? 2 : 0;
        k.sprawa = 1;
        k.profilPubliczny = liczby;
        k.sypialniaDostepna = sypialniaWKokpicie();
        const bool sypWCiagu = dane && pokazSyp;
        SPRAWDZ(k.sypialniaDostepna == sypWCiagu);
        std::vector<Ekran> ocz = oczekiwanyCiag(u, sprawy, liczby, dane);
        SPRAWDZ(ciagKokpitu(k) == ocz);
        // ▶ od ekranu dnia; na „Otwartych sprawach” ▶ przechodzi przez obie karty
        std::vector<Ekran> c;
        Ekran e = Ekran::DZIEN;
        for (int i = 0; i < 20 && e != Ekran::USTAWIENIA; i++) {
          Przejscie pr = nacisniecie(e, Przycisk::PRAWY, false, k);
          if (pr.akcja == Akcja::SPRAWY_DALEJ) { k.sprawa++; continue; }
          if (pr.ekran == Ekran::SPRAWY) k.sprawa = 1;
          c.push_back(e = pr.ekran);
        }
        SPRAWDZ(c == ocz);
        if (c != ocz) printf("  układ %s, maska %d: ciąg ▶ inny\n", kolejnoscKokpituTekst(u).c_str(), maska);
        // ◀ z U-01 dokładnie odwrotnie, na sprawy od ostatniej karty
        std::vector<Ekran> w;
        for (int i = 0; i < 20 && e != Ekran::DZIEN; i++) {
          if (e == Ekran::SPRAWY && k.sprawa > 1) {
            SPRAWDZ(nacisniecie(e, Przycisk::LEWY, false, k).akcja == Akcja::SPRAWY_WSTECZ);
            k.sprawa--;
            continue;
          }
          Przejscie pr = nacisniecie(e, Przycisk::LEWY, false, k);
          if (pr.ekran == Ekran::SPRAWY) {
            SPRAWDZ(pr.akcja == Akcja::SPRAWY_OD_KONCA);
            k.sprawa = k.spraw;
          }
          w.push_back(e = pr.ekran);
        }
        std::vector<Ekran> odwr(ocz.rbegin() + 1, ocz.rend());
        odwr.push_back(Ekran::DZIEN);
        SPRAWDZ(w == odwr);
        // numery stron: 1…N po kolei w ciągu, U-01 = N, N = 3 albo 4 z liczby stron z numerem w ciągu
        int ostatni = 0;
        for (Ekran x : ocz)
          if (numerStrony(x)) {
            SPRAWDZ(numerStrony(x) == ostatni + 1);
            ostatni = numerStrony(x);
          }
        SPRAWDZ(ostatni == stronKokpitu() && stronKokpitu() == (sypWCiagu ? 4 : 3));
        SPRAWDZ(numerStronyTekst(Ekran::USTAWIENIA) == std::to_string(stronKokpitu()) + " / " + std::to_string(stronKokpitu()));
        // ukryta albo bez danych strona sypialni nie ma numeru i nie jest celem żadnego naciśnięcia w ciągu
        if (!sypWCiagu) {
          SPRAWDZ(numerStrony(Ekran::SYPIALNIA) == 0);
          for (Ekran x : ocz)
            for (Przycisk p : {Przycisk::LEWY, Przycisk::PRAWY}) SPRAWDZ(nacisniecie(x, p, false, k).ekran != Ekran::SYPIALNIA);
          SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, k).ekran != Ekran::SYPIALNIA);
        }
        // R-KOK-35: każda stopka strony w ciągu nazywa cel ◀ i ▶ z nacisniecie(); ukryta strona w żadnej stopce
        auto sprawdzStopke = [&](Ekran x, const std::vector<std::string>& st, bool zDalej) {
          KontekstNaw kk = k;
          kk.sprawa = x == Ekran::SPRAWY ? 1 : k.sprawa;
          Ekran lewy = nacisniecie(x, Przycisk::LEWY, false, kk).ekran;
          SPRAWDZ(cel(st[0]) == podpisStrony(lewy));
          if (cel(st[0]) != podpisStrony(lewy))
            printf("  układ %s maska %d: %s ◀ „%s” zamiast „%s”\n", kolejnoscKokpituTekst(u).c_str(), maska,
                   podpisStrony(x), st[0].c_str(), podpisStrony(lewy));
          if (zDalej) {
            kk.sprawa = x == Ekran::SPRAWY ? kk.spraw : kk.sprawa;
            Ekran prawy = nacisniecie(x, Przycisk::PRAWY, false, kk).ekran;
            SPRAWDZ(cel(st[1]) == podpisStrony(prawy));
            if (cel(st[1]) != podpisStrony(prawy))
              printf("  układ %s maska %d: %s ▶ „%s” zamiast „%s”\n", kolejnoscKokpituTekst(u).c_str(), maska,
                     podpisStrony(x), st[1].c_str(), podpisStrony(prawy));
          }
          for (const std::string& el : st)
            if (!sypWCiagu) SPRAWDZ(!zawiera(el, "sypialnia"));
        };
        sprawdzStopke(Ekran::K1, stopkaK1(true, "", sprawy, liczby, sypWCiagu), true);
        sprawdzStopke(Ekran::K2, stopkaK2(sprawy, liczby, sypWCiagu), true);
        if (sprawy) {  // pierwsza karta (◀) i ostatnia (▶)
          std::vector<std::string> p1 = stopkaSpraw(1, 2, liczby, sypWCiagu), p2 = stopkaSpraw(2, 2, liczby, sypWCiagu);
          sprawdzStopke(Ekran::SPRAWY, {p1[0], p2[1]}, true);
        }
        if (liczby) {
          podsumowanie::DaneLiczb d = podsumowanie::budujLiczby(reguly::Tryb::CPAP, false, sprawy, sypWCiagu);
          size_t a = d.stopkaLewo.find("   "), b = d.stopkaLewo.find("   ", a + 3);
          sprawdzStopke(Ekran::LICZBY, {d.stopkaLewo.substr(0, a), d.stopkaLewo.substr(a + 3, b - a - 3)}, true);
        }
        if (sypWCiagu) sprawdzStopke(Ekran::SYPIALNIA, stopkaSyp30(sprawy, liczby), true);
        sprawdzStopke(Ekran::USTAWIENIA, stopkaU01(sprawy, liczby, sypWCiagu), false);
        // K2 zawsze z raportem pod ▶ 2 s, w każdej kolejności
        SPRAWDZ(zawiera(stopkaK2(sprawy, liczby, sypWCiagu)[1], " · 2 s raport"));
        SPRAWDZ(nacisniecie(Ekran::K2, Przycisk::PRAWY, true, k).ekran == Ekran::RAPORT);
        // ekran nocny i ekran wzorca: ▶ na pierwszą stronę ciągu
        KontekstNaw kn = k;
        kn.nocny = true;
        SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, kn).ekran == ocz.front());
        kn.nocny = false;
        kn.wzorzec = true;
        SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, kn).ekran == ocz.front());
      }
    }
  }
  SPRAWDZ(ukladow == 6 * 2 * 8);
  ustawUkladKokpitu(UkladKokpitu());
  ustawSypialnieWKokpicie(false);
}

// ---------- T-TEST-435: plik bez nowych kluczy = zachowanie sprzed ADR-0045 ----------
// wymaganie: R-KOK-34
static void testR_KOK_34_zgodnoscZBaza() {
  UstawieniaDnia u = parsujUstawieniaDnia("profil=publiczny\nco2=nie\n");
  SPRAWDZ(u.ekrany.kokpit.domyslny() && u.ekrany.dzien.domyslny() && u.bledy.empty());
  ustawUkladKokpitu(u.ekrany.kokpit);
  const Ekran EKRANY[] = {Ekran::DZIEN, Ekran::K1, Ekran::K2, Ekran::PYTANIE, Ekran::TEST, Ekran::OMOWIONE, Ekran::SPRAWY,
                          Ekran::PRZENIESIONE, Ekran::ZAPISANO, Ekran::RAPORT, Ekran::NORMA, Ekran::LICZBY, Ekran::SPRZET,
                          Ekran::SPRZET_ZAPISANO, Ekran::USTAWIENIA, Ekran::FORMULARZ, Ekran::SIEC_PANELU,
                          Ekran::AKTUALIZACJA, Ekran::PORTRET, Ekran::SYPIALNIA};
  int porownan = 0, roznic = 0;
  for (int maska = 0; maska < (1 << 12); maska++) {
    KontekstNaw k;
    k.sprawyDostepne = maska & 1;
    k.profilPubliczny = maska & 2;
    k.sypialniaDostepna = maska & 4;
    k.nocny = maska & 8;
    k.wzorzec = maska & 16;
    k.problem = maska & 32;
    k.s5 = maska & 64;
    k.pytanieTN = maska & 128;
    k.sk31 = maska & 256;
    k.zaproszenie = maska & 512;
    k.ostatniaNoc = maska & 1024;
    k.spraw = maska & 2048 ? 3 : 0;
    k.zrodlo = maska & 2048 ? Ekran::K2 : Ekran::DZIEN;
    ustawSypialnieWKokpicie(k.sypialniaDostepna);
    baza::ustawSypialnieWKokpicie(k.sypialniaDostepna);
    for (int karta = 1; karta <= 3; karta++) {
      k.sprawa = karta;
      for (Ekran e : EKRANY)
        for (Przycisk p : {Przycisk::LEWY, Przycisk::SRODEK, Przycisk::PRAWY})
          for (bool dl : {false, true}) {
            Przejscie a = nacisniecie(e, p, dl, k);
            baza::Przejscie b = baza::nacisniecie(static_cast<baza::Ekran>(e), static_cast<baza::Przycisk>(p), dl,
                                                  baza::zBiezacego(k));
            porownan++;
            if ((int)a.ekran != (int)b.ekran || (int)a.akcja != (int)b.akcja) {
              if (roznic++ < 5)
                printf("  różnica: ekran %d przycisk %d dł %d maska %d: %d/%d zamiast %d/%d\n", (int)e, (int)p, dl, maska,
                       (int)a.ekran, (int)a.akcja, (int)b.ekran, (int)b.akcja);
            }
          }
    }
    for (Ekran e : EKRANY)
      SPRAWDZ(numerStronyTekst(e) == baza::numerStronyTekst(static_cast<baza::Ekran>(e)) &&
              stronaKokpitu(e) == baza::stronaKokpitu(static_cast<baza::Ekran>(e)));
  }
  SPRAWDZ(roznic == 0);
  // stopki w układzie domyślnym jak przed zmianą (dosłownie)
  for (int m = 0; m < 8; m++) {
    bool s = m & 1, l = m & 2, sy = m & 4;
    ustawSypialnieWKokpicie(sy);
    SPRAWDZ(stopkaK1(true, "", s, l, sy) == (std::vector<std::string>{"◀ ekran dnia", "▶ 7 nocy", "↻ ekran dnia · 2 s co było inaczej"}));
    SPRAWDZ(stopkaK2(s, l, sy)[0] == "◀ ostatnia noc");
    SPRAWDZ(stopkaK2(s, l, sy)[1] == (l ? "▶ liczby · 2 s raport" : s ? "▶ sprawy · 2 s raport"
                                                                  : sy ? "▶ sypialnia · 2 s raport" : "▶ ustawienia · 2 s raport"));
    SPRAWDZ(stopkaU01(s, l, sy)[0] == (sy ? "◀ sypialnia" : s ? "◀ sprawy" : l ? "◀ liczby" : "◀ 7 nocy"));
    SPRAWDZ(stopkaSyp30(s, l)[0] == (s ? "◀ sprawy" : l ? "◀ liczby" : "◀ 7 nocy") && stopkaSyp30(s, l)[1] == "▶ ustawienia");
    SPRAWDZ(stopkaSpraw(1, 2, l, sy)[0] == (l ? "◀ liczby" : "◀ 7 nocy") &&
            stopkaSpraw(2, 2, l, sy)[1] == (sy ? "▶ sypialnia" : "▶ ustawienia"));
    std::string li = podsumowanie::budujLiczby(reguly::Tryb::CPAP, false, s, sy).stopkaLewo;
    SPRAWDZ(li == (s ? "◀ 7 nocy   ▶ sprawy   ↻ ekran dnia" : sy ? "◀ 7 nocy   ▶ sypialnia   ↻ ekran dnia"
                                                               : "◀ 7 nocy   ▶ ustawienia   ↻ ekran dnia"));
  }
  // numery w tekstach: jak przed zmianą
  for (bool sy : {false, true}) {
    ustawSypialnieWKokpicie(sy);
    baza::ustawSypialnieWKokpicie(sy);
    for (const char* t : {"Na kontroli: raport 2 / 3.", "→ Otwarte sprawy: za stroną 2 / 3.", "Ustawienia: strona 3 / 3.",
                          "Lista na stronie 1 / 3 (znak *).", "13 / 30 nocy", "raport 2 / 3,5"})
      SPRAWDZ(numeryStronWTekscie(t) == baza::numeryStronWTekscie(t));
  }
  printf("zgodność nawigacji z bazą e7bb4d4: %d porównań, %d różnic\n", porownan, roznic);
  ustawSypialnieWKokpicie(false);
}

// ---------- R-KOK-34: wartości uszkodzone i ekrany zawsze włączone w pliku ----------
// wymaganie: R-KOK-34
static void testR_KOK_34_plikUszkodzony() {
  auto u = [](const std::string& t) { return parsujUstawieniaDnia(t); };
  // kolejność: znane w podanej kolejności, reszta na końcu w kolejności domyślnej; powtórzone i nieznane pominięte
  UstawieniaDnia a = u("kokpit.kolejnosc=xx, sypialnia ,sypialnia,7_nocy\n");
  SPRAWDZ(kolejnoscKokpituTekst(a.ekrany.kokpit) == "sypialnia,7_nocy,ostatnia_noc" && a.bledy.size() == 1);
  // jeden wpis na klucz z liczbą pominiętych nazw i najwyżej 3 przykładami
  SPRAWDZ(a.bledy.size() == 1 && a.bledy[0] == "kokpit.kolejnosc: pominięte 2 nazwy: xx nieznane, sypialnia powtórzone");
  SPRAWDZ(u("kokpit.kolejnosc=\n").ekrany.kokpit.domyslny() && u("kokpit.kolejnosc=,,,\n").ekrany.kokpit.domyslny());
  SPRAWDZ(u("kokpit.kolejnosc=ostatnia_noc,7_nocy,sypialnia,ustawienia\n").ekrany.kokpit.domyslny());
  SPRAWDZ(u("kokpit.kolejnosc=" + std::string(5000, ',') + "\n").ekrany.kokpit.domyslny());
  // ręcznie zmieniony plik: ekrany zawsze włączone nie dają się ukryć ani przestawić za U-01
  UstawieniaDnia z = u("kokpit.ukryte=ostatnia_noc,7_nocy,ustawienia,sprawy,liczby,sypialnia,portret\n"
                       "dzien.ukryte=drzemka,przewietrz,s5,lekarz,problem,pytanie,bateria,E-05,obrazek\n");
  SPRAWDZ(!z.ekrany.kokpit.sypialnia && z.bledy.size() == 2);  // jeden wpis na klucz (6 i 8 nazw)
  for (const std::string& b : z.bledy) SPRAWDZ(zawiera(b, "zawsze włączone") || zawiera(b, "nieznane"));
  SPRAWDZ(z.bledy.size() == 2 && zawiera(z.bledy[0], "kokpit.ukryte: pominięte 6 nazw: ostatnia_noc zawsze włączone, ") &&
          zawiera(z.bledy[0], ", …") && zawiera(z.bledy[1], "dzien.ukryte: pominięte 8 nazw: drzemka "));
  // bardzo długi wiersz (10 000 nazw po 40 znaków): jeden wpis, krótki, wartość czytana do 160 B
  {
    std::string dlugi = "dzien.ukryte=";
    for (int i = 0; i < 10000; i++) dlugi += (i ? "," : "") + std::string(40, 'x');
    UstawieniaDnia w = u(dlugi + "\nkokpit.kolejnosc=" + std::string(100000, 'q') + "\n");
    SPRAWDZ(w.bledy.size() == 2 && w.ekrany.dzien.domyslny() && w.ekrany.kokpit.domyslny());
    for (const std::string& b : w.bledy) SPRAWDZ(b.size() < 300);  // bajty UTF-8: 3 przykłady po 24 B + powody
    SPRAWDZ(w.bledy.size() == 2 && zawiera(w.bledy[0], "dzien.ukryte: pominięte 4 nazwy: ") &&
            zawiera(w.bledy[1], "kokpit.kolejnosc: ") && zawiera(w.bledy[1], "q… nieznane"));
  }
  ustawUkladKokpitu(z.ekrany.kokpit);
  ustawSypialnieWKokpicie(true);
  for (int m = 0; m < 4; m++) {
    KontekstNaw k;
    k.sprawyDostepne = m & 1;
    k.profilPubliczny = m & 2;
    k.sypialniaDostepna = sypialniaWKokpicie();
    std::vector<Ekran> c = ciagKokpitu(k);
    SPRAWDZ(std::count(c.begin(), c.end(), Ekran::K1) == 1 && std::count(c.begin(), c.end(), Ekran::K2) == 1 &&
            c.back() == Ekran::USTAWIENIA && std::count(c.begin(), c.end(), Ekran::SPRAWY) == (k.sprawyDostepne ? 1 : 0) &&
            std::count(c.begin(), c.end(), Ekran::LICZBY) == (k.profilPubliczny ? 1 : 0));
  }
  // dzień: tylko obrazek ukryty; S-K-39 i `Przewietrz teraz.` nie mają nazwy w pliku
  for (int i = 0; i < RODZAJOW_DZIENNYCH; i++) SPRAWDZ(z.ekrany.dzien.pokaz[i] == (i != (int)RodzajDzienny::OBRAZEK));
  // kolejność, która nie jest permutacją (zepsuta pamięć), daje układ domyślny, nie pusty kokpit
  UkladKokpitu zly;
  zly.kolejnosc[0] = zly.kolejnosc[1] = zly.kolejnosc[2] = K1S;
  ustawUkladKokpitu(zly);
  SPRAWDZ(ukladKokpitu().kolejnosc[1] == K2S && ukladKokpitu().kolejnosc[2] == SYS);
  UkladKokpitu poza;
  poza.kolejnosc[0] = static_cast<StronaPrzestawna>(7);
  ustawUkladKokpitu(poza);
  SPRAWDZ(ukladKokpitu().domyslny());
  // dzień: kolejność uszkodzona w WejscieSlotu (nie permutacja) = domyślna
  UkladDnia zd;
  zd.kolejnosc[0] = zd.kolejnosc[1] = RodzajDzienny::PYLEK;
  WejscieSlotu we;
  we.dzis = "2026-11-14";
  we.minuta = 12 * 60;
  we.uklad = zd;
  WejscieSlotu wd = we;
  wd.uklad = UkladDnia();
  SPRAWDZ(wybierzSlot(we).ekran == wybierzSlot(wd).ekran);
  // zapis wartości: domyślne = pusty tekst (wiersz usuwany)
  SPRAWDZ(kolejnoscKokpituTekst(UkladKokpitu()).empty() && ukryteKokpituTekst(UkladKokpitu()).empty() &&
          kolejnoscDniaTekst(UkladDnia()).empty() && ukryteDniaTekst(UkladDnia()).empty());
  ustawUkladKokpitu(UkladKokpitu());
  ustawSypialnieWKokpicie(false);
}

// ---------- R-KOK-36: numery stron w tekstach dla dowolnej kolejności ----------
// wymaganie: R-KOK-36
static void testR_KOK_36_numeryWTekstach() {
  for (UkladKokpitu u : permutacje())
    for (bool syp : {false, true}) {
      ustawUkladKokpitu(u);
      ustawSypialnieWKokpicie(syp);
      const std::string N = std::to_string(stronKokpitu());
      const std::string n1 = std::to_string(numerStrony(Ekran::K1)), n2 = std::to_string(numerStrony(Ekran::K2));
      SPRAWDZ(numeryStronWTekscie("Na kontroli: raport 2 / 3.") == "Na kontroli: raport " + n2 + " / " + N + ".");
      SPRAWDZ(numeryStronWTekscie("→ Otwarte sprawy: za stroną 2 / 3.") == "→ Otwarte sprawy: za stroną " + n2 + " / " + N + ".");
      SPRAWDZ(numeryStronWTekscie("Ustawienia: strona 3 / 3.") == "Ustawienia: strona " + N + " / " + N + ".");
      SPRAWDZ(numeryStronWTekscie("Lista na stronie 1 / 3 (znak *).") == "Lista na stronie " + n1 + " / " + N + " (znak *).");
      SPRAWDZ(numeryStronWTekscie("13 / 30 nocy") == "13 / 30 nocy" && numeryStronWTekscie("raport 2/3") == "raport 2/3");
      // linia C z dowolnym numerem: karta sprawy zdejmuje `Na kontroli: …`
      sprawy::SprawaL x;
      x.klucz = x.id = "L-03";
      x.stan = reguly::StanReguly::AKTYWNA;
      x.pilnosc = reguly::Pilnosc::C;
      x.ton = sprawy::Ton::LEKARZ;
      x.linia = numeryStronWTekscie("Bezdechy 6 / h od 4 tygodni. Na kontroli: raport 2 / 3.");
      std::vector<sprawy::SprawaL> v = {x};
      sprawy::WejscieSpraw we;
      we.dzis = "2026-11-14";
      we.sprawy = &v;
      std::vector<sprawy::KartaSprawy> k = sprawy::kartySpraw(we);
      SPRAWDZ(!k.empty() && k[0].powod == "Bezdechy 6 / h od 4 tygodni.");
    }
  SPRAWDZ(dlugoscKontroliZNumerem("A. Na kontroli: raport 3 / 4.") == 27 && dlugoscKontroliZNumerem("A. Na kontroli: raport 0 / 4.") == 0 &&
          dlugoscKontroliZNumerem("raport 3 / 4.") == 0 && dlugoscKontroliZNumerem("A. Na kontroli: raport 3/4..") == 0);
  ustawUkladKokpitu(UkladKokpitu());
  ustawSypialnieWKokpicie(false);
}

// ---------- R-KON-70…R-KON-72: slot dzienny z kolejnością i wyłączonymi rodzajami ----------
static const double KAT_LAT = 50.2597, KAT_LON = 19.0217;
static Prognoza prognoza(const std::string& dzis, float t, float pm) {
  Prognoza p;
  p.data = dzis;
  p.pobrano = 1;
  p.godzinaPobrania = "07:55";
  for (auto& g : p.h) {
    g.temp = t;
    g.pm25 = pm;
    g.opad = 0;
    g.chmury = 20;
    g.porywy = 20;
  }
  return p;
}
static const std::string D = "2026-11-03";
// wszystkie cztery D1…D4 naraz: zima, pyłek brzozy wysoko 09:00–20:00 (okno wietrzenia od 20:00, S-K-36), jutro
// mróz (S-K-34), filtr po terminie (S-K-35)
struct Scena {
  Prognoza p = prognoza(D, 1, 20);
  ElementWymiany el[EL_N];
  Scena() {
    for (int h = 24 + 6; h < 24 + 9; h++) p.h[(size_t)h].temp = -4;  // jutro rano mróz (S-K-34)
    for (int h = 0; h < PROG_GODZIN; h++) p.h[(size_t)h].pylek[PYL_BRZOZA] = h >= 9 && h < 20 ? 500.f : 10.f;  // okno od 20:00
    el[EL_FILTR] = {"2026-09-01", 4, true};
  }
  WejscieSlotu we(int minuta, bool co2 = false) const {
    WejscieSlotu w;
    w.dzis = D;
    w.minuta = minuta;
    w.prognoza = &p;
    w.przyLozku = 21;
    w.co2 = co2 ? 1400 : -1;
    w.pylki.zaznaczony[PYL_BRZOZA] = true;
    w.pylki.prog[PYL_BRZOZA] = 100;
    w.sprzet = &el;
    w.wspolrzedne = true;
    w.lat = KAT_LAT;
    w.lon = KAT_LON;
    w.obrazekJest = true;
    w.slotOd = 8 * 60;
    return w;
  }
};
static EkranSlotu ekranRodzaju(RodzajDzienny r) {
  switch (r) {
    case RodzajDzienny::WIETRZENIE: return EkranSlotu::SK33;
    case RodzajDzienny::JUTRO_RANO: return EkranSlotu::SK34;
    case RodzajDzienny::WYMIANA: return EkranSlotu::SK35;
    case RodzajDzienny::PYLEK: return EkranSlotu::SK36;
    case RodzajDzienny::DZIEN_I_NOC: return EkranSlotu::SK37;
    default: return EkranSlotu::SK38;
  }
}
// wymaganie: R-KON-70
static void testR_KON_70_kolejnoscDnia() {
  Scena s;
  WejscieSlotu w = s.we(13 * 60);
  WyborSlotu dom = wybierzSlot(w);
  if (dom.ekran != EkranSlotu::SK33) printf("  scena: ekran %d okno %d jutro %d pylek %d\n", (int)dom.ekran, (int)dom.okno.wariant, (int)dom.jutro.wariant, dom.pylek.rodzaj);
  SPRAWDZ(dom.ekran == EkranSlotu::SK33);  // scena: przed 16:00 okno, wszystkie cztery kandydatami
  // każda permutacja D1…D4 (24) × każdy podzbiór włączonych (16): wygrywa pierwszy włączony z listy, bez żadnego
  // włączonego S-K-37; ten sam wynik przed 16:00 (bez R-KON-46)
  RodzajDzienny a[4] = {RodzajDzienny::WIETRZENIE, RodzajDzienny::JUTRO_RANO, RodzajDzienny::WYMIANA, RodzajDzienny::PYLEK};
  int przypadkow = 0;
  do {
    for (int pod = 0; pod < 16; pod++) {
      w.uklad = UkladDnia();
      for (int i = 0; i < 4; i++) {
        w.uklad.kolejnosc[i] = a[i];
        w.uklad.pokaz[(int)a[i]] = (pod >> i) & 1;
      }
      EkranSlotu ocz = EkranSlotu::SK37;
      for (RodzajDzienny r : a)
        if (w.uklad.pokazuj(r)) { ocz = ekranRodzaju(r); break; }
      SPRAWDZ(wybierzSlot(w).ekran == ocz);
      przypadkow++;
    }
  } while (std::next_permutation(a, a + 4));
  SPRAWDZ(przypadkow == 24 * 16);
  // R-KON-46 po 16:00: S-K-34 tuż przed S-K-33, gdy S-K-33 stoi wyżej; inna kolejność bez zmian
  WejscieSlotu po = s.we(17 * 60);
  SPRAWDZ(wybierzSlot(po).ekran == EkranSlotu::SK34);  // domyślnie
  po.uklad.kolejnosc[0] = RodzajDzienny::WYMIANA;      // wymiana, jutro, wietrzenie, pyłek
  po.uklad.kolejnosc[1] = RodzajDzienny::JUTRO_RANO;
  po.uklad.kolejnosc[2] = RodzajDzienny::WIETRZENIE;
  SPRAWDZ(wybierzSlot(po).ekran == EkranSlotu::SK35);
  po.uklad = UkladDnia();
  po.uklad.kolejnosc[0] = RodzajDzienny::PYLEK;  // pyłek, jutro, wymiana, wietrzenie
  po.uklad.kolejnosc[3] = RodzajDzienny::WIETRZENIE;
  po.uklad.pokaz[(int)RodzajDzienny::PYLEK] = false;
  SPRAWDZ(wybierzSlot(po).ekran == EkranSlotu::SK34);
  po.uklad.pokaz[(int)RodzajDzienny::JUTRO_RANO] = false;
  SPRAWDZ(wybierzSlot(po).ekran == EkranSlotu::SK35);
}

// wymaganie: R-KON-71
static void testR_KON_71_wylaczone() {
  Scena s;
  // nic poza S-K-37 i S-K-38: po bloku obrazka S-K-37; wyłączony S-K-37 → S-K-38; oba wyłączone → ekran z rana
  WejscieSlotu w = s.we(13 * 60);
  for (int i = 0; i < 4; i++) w.uklad.pokaz[i] = false;
  SPRAWDZ(wybierzSlot(w).ekran == EkranSlotu::SK37);
  w.uklad.pokaz[(int)RodzajDzienny::DZIEN_I_NOC] = false;
  SPRAWDZ(wybierzSlot(w).ekran == EkranSlotu::SK38 && !wybierzSlot(w).slonce.jest);
  w.uklad.pokaz[(int)RodzajDzienny::OBRAZEK] = false;
  SPRAWDZ(wybierzSlot(w).ekran == EkranSlotu::BRAK);  // dyrygent: ekran z rana (W4, R-KON-54), nigdy pusty
  // obrazek wyłączony: w pierwszym bloku slotu bez S-K-38, od razu lista
  WejscieSlotu b = s.we(8 * 60 + 30);
  SPRAWDZ(wybierzSlot(b).ekran == EkranSlotu::SK38);
  b.uklad.pokaz[(int)RodzajDzienny::OBRAZEK] = false;
  SPRAWDZ(wybierzSlot(b).ekran == EkranSlotu::SK33);
  // jedyny kandydat wyłączony: to, co bez niego (następny, a na końcu S-K-37)
  WejscieSlotu j = s.we(13 * 60);
  j.prognoza = nullptr;  // bez prognozy: S-K-33, S-K-34, S-K-36 milczą; zostaje S-K-35
  SPRAWDZ(wybierzSlot(j).ekran == EkranSlotu::SK35);
  j.uklad.pokaz[(int)RodzajDzienny::WYMIANA] = false;
  SPRAWDZ(wybierzSlot(j).ekran == EkranSlotu::SK37);
}

// wymaganie: R-KON-72
static void testR_KON_72_zawszeWlaczone() {
  Scena s;
  UkladDnia nic;
  for (bool& b : nic.pokaz) b = false;
  nic.kolejnosc[0] = RodzajDzienny::PYLEK;
  nic.kolejnosc[3] = RodzajDzienny::WIETRZENIE;
  // `Przewietrz teraz.` (CO2 ≥ 1000 przy czystej godzinie) przed wszystkim, także przy wyłączonym „Wietrzenie”
  // i w bloku obrazka
  for (int minuta : {8 * 60 + 30, 13 * 60, 17 * 60}) {
    WejscieSlotu w = s.we(minuta, true);
    w.pylki = PylkiDnia();  // godzina czysta (bez pyłku w regule), więc `Przewietrz teraz.`
    w.uklad = nic;
    WyborSlotu r = wybierzSlot(w);
    SPRAWDZ(r.ekran == EkranSlotu::SK33 && r.okno.wariant == Wietrzenie::CO2);
  }
  // drzemka z wyciekiem (S-K-39) przed wszystkim
  WejscieSlotu d = s.we(13 * 60, true);
  d.uklad = nic;
  d.drzemka.minut = 60;
  d.drzemka.wyciekP95 = 40;
  SPRAWDZ(wybierzSlot(d).ekran == EkranSlotu::SK39);
  // plik nie ma nazw dla ekranów zawsze włączonych: żadna wartość `dzien.ukryte` nie wyłącza S-K-39 ani CO2
  UstawieniaDnia u = parsujUstawieniaDnia("dzien.ukryte=wietrzenie,jutro_rano,wymiana,pylek,dzien_i_noc,obrazek,S-K-39,"
                                          "drzemka,co2,przewietrz,przewietrz_teraz,linia,bateria\n");
  d.uklad = u.ekrany.dzien;
  SPRAWDZ(wybierzSlot(d).ekran == EkranSlotu::SK39);
  WejscieSlotu c = s.we(13 * 60, true);
  c.pylki = PylkiDnia();
  c.uklad = u.ekrany.dzien;
  SPRAWDZ(wybierzSlot(c).okno.wariant == Wietrzenie::CO2 && wybierzSlot(c).ekran == EkranSlotu::SK33);
}

// T-TEST-435: wybierzSlot() z układem domyślnym = baza sprzed ADR-0045 (losowe wejścia)
// wymaganie: R-KON-70
static void testR_KON_70_zgodnoscZBaza() {
  unsigned los = 12345;
  auto r = [&](int n) {
    los = los * 1103515245u + 12345u;
    return (int)((los >> 16) % (unsigned)n);
  };
  int roznic = 0;
  const int N = 20000;
  for (int i = 0; i < N; i++) {
    Prognoza p = prognoza(D, (float)(r(40) - 10), (float)r(80));
    for (int h = 0; h < PROG_GODZIN; h++) {
      if (r(3) == 0) p.h[(size_t)h].temp = (float)(r(40) - 10);
      if (r(3) == 0) p.h[(size_t)h].pm25 = (float)r(80);
      p.h[(size_t)h].opad = (float)r(100);
      p.h[(size_t)h].porywy = (float)r(90);
      p.h[(size_t)h].pylek[PYL_BRZOZA] = (float)r(300);
    }
    ElementWymiany el[EL_N];
    if (r(2)) el[EL_FILTR] = {r(2) ? "2026-09-01" : "2026-10-28", 4, true};
    if (r(2)) el[EL_MASKA] = {r(2) ? "2026-04-01" : "2026-10-01", 6, false};
    WejscieSlotu w;
    w.dzis = D;
    w.minuta = 6 * 60 + r(17 * 60);
    w.prognoza = r(5) ? &p : nullptr;
    w.przyLozku = r(6) ? (float)(15 + r(15)) : NAN;
    w.co2 = r(3) ? -1 : 400 + r(1600);
    if (r(2)) {
      w.pylki.zaznaczony[PYL_BRZOZA] = true;
      w.pylki.prog[PYL_BRZOZA] = 100;
    }
    w.sprzet = r(4) ? &el : nullptr;
    w.ostatnieSK35 = r(3) ? "" : "2026-11-02";
    w.wspolrzedne = r(4) != 0;
    w.lat = KAT_LAT;
    w.lon = KAT_LON;
    w.obrazekJest = r(2);
    w.liniaUwagi = r(4) == 0;
    w.slotOd = r(4) ? 6 * 60 + r(6 * 60) : -1;
    if (r(10) == 0) {
      w.drzemka.minut = 40;
      w.drzemka.wyciekP95 = (float)r(50);
    }
    WyborSlotu a = wybierzSlot(w), b = baza::wybierzSlot(w);
    bool rowne = a.ekran == b.ekran && a.okno.wariant == b.okno.wariant && a.jutro.wariant == b.jutro.wariant &&
                 a.wymiana.element == b.wymiana.element && a.pylek.rodzaj == b.pylek.rodzaj && a.slonce.jest == b.slonce.jest;
    if (!rowne && roznic++ < 5) printf("  slot: różnica w przypadku %d: %d zamiast %d\n", i, (int)a.ekran, (int)b.ekran);
  }
  SPRAWDZ(roznic == 0);
  printf("zgodność slotu dziennego z bazą e7bb4d4: %d przypadków, %d różnic\n", N, roznic);
}

// ---------- R-KONF-60: formularz, sekcja „Ekrany” ----------
static std::map<std::string, std::string> poleFormularza(const std::string& strona) {  // pola tak, jak wyśle przeglądarka
  std::map<std::string, std::string> m;
  size_t f0 = strona.find("<form"), f1 = strona.find("</form>");
  std::string f = strona.substr(f0, f1 - f0);
  for (size_t p = f.find("<input"); p != std::string::npos; p = f.find("<input", p + 1)) {
    std::string t = f.substr(p, f.find('>', p) - p);
    auto attr = [&](const char* a) {
      std::string k = std::string(a) + "=\"";
      size_t i = t.find(k);
      return i == std::string::npos ? std::string() : t.substr(i + k.size(), t.find('"', i + k.size()) - i - k.size());
    };
    std::string typ = attr("type"), n = attr("name");
    if (n.empty() || typ == "password") continue;
    if (typ == "checkbox") {
      if (zawiera(t, " checked")) m[n] = attr("value");
    } else m[n] = attr("value");
  }
  for (size_t p = f.find("<select name=\""); p != std::string::npos; p = f.find("<select name=\"", p + 1)) {
    size_t a = p + 14;
    std::string n = f.substr(a, f.find('"', a) - a);
    size_t k = f.find("</select>", p);
    std::string s = f.substr(p, k - p);
    size_t sel = s.find(" selected");
    if (sel == std::string::npos) continue;
    if (n.compare(0, 3, "kk_") == 0 || n.compare(0, 3, "dk_") == 0) m[n] = s.substr(sel + 10, 1);  // `<option selected>3`
  }
  return m;
}
// wymaganie: R-KONF-60
static void testR_KONF_60_formularz() {
  sekrety::Dane s;
  s.ssid = "Dom";
  s.abAdres = "192.168.1.20";
  formularz::Pola p = formularz::zPliku("", s, true);
  std::string h = formularz::strona(p, "/u/abc/", "", {});
  // sekcja po „Panel”, zwinięta, w nagłówku skrót stanu
  SPRAWDZ(zawiera(h, "<details><summary><b>Ekrany</b><span>do 4 stron · 6 ekranów dziennych</span></summary>"));
  SPRAWDZ(h.find("<b>Panel</b>") < h.find("<b>Ekrany</b>") && h.find("<b>Ekrany</b>") < h.find("<b>Dźwięk i humor</b>"));
  // przełączniki tylko przy ekranach przełączalnych; K1, K2 i U-01 bez pola wyboru, z opisem „zawsze”
  for (const char* n : {"pokaz_sypialnia", "pokaz_wietrzenie", "pokaz_jutro_rano", "pokaz_wymiana", "pokaz_pylek",
                        "pokaz_dzien_i_noc", "pokaz_obrazek"})
    SPRAWDZ(ile(h, std::string("name=\"") + n + "\" value=\"1\" checked") == 1);
  for (const char* n : {"pokaz_ostatnia_noc", "pokaz_7_nocy", "pokaz_ustawienia", "pokaz_sprawy", "pokaz_liczby", "pokaz_drzemka"})
    SPRAWDZ(!zawiera(h, n));
  SPRAWDZ(zawiera(h, "<b class=\"c\">Ostatnia noc</b><p class=\"s\">zawsze widoczna (nie da się ukryć): ") &&
          zawiera(h, "<b class=\"c\">7 nocy</b><p class=\"s\">zawsze widoczna (nie da się ukryć): "));
  // bez żargonu (pilność, „tylko raport”, ocena częściowa) w sekcji; błąd w wierszu na własnej linii, za strzałkami
  {
    size_t a0 = h.find("<b>Ekrany</b>"), a1 = h.find("</details>", a0);
    std::string sek = h.substr(a0, a1 - a0);
    SPRAWDZ(!zawiera(sek, "pilno") && !zawiera(sek, "tylko raport") && !zawiera(sek, "ocena częściowa") &&
            !zawiera(sek, "Poranek bez zmian"));
    SPRAWDZ(zawiera(h, ".p .b{flex-basis:100%;order:1}"));
  }
  SPRAWDZ(zawiera(h, "<b>Ustawienia</b> zawsze na końcu.") && zawiera(h, "„Otwarte sprawy” (gdy jest sprawa)"));
  SPRAWDZ(zawiera(h, "Zawsze przed listą: ekran z rana ze sprawą") && zawiera(h, "„Przewietrz teraz”"));
  // bez skryptu: miejsce z listy w każdym wierszu, bez przycisków strzałek w HTML (dokłada je skrypt), cele ≥ 44 px
  SPRAWDZ(ile(h, "<label class=\"m\">miejsce <select name=\"kk_") == 3 && ile(h, "<label class=\"m\">miejsce <select name=\"dk_") == 4);
  SPRAWDZ(ile(h, "<button") == 1 && !zawiera(h, "type=\"button\""));
  SPRAWDZ(zawiera(h, ".p button{display:inline-block;width:44px;min-height:44px") && zawiera(h, "label.c{display:flex;align-items:center;gap:.6em;min-height:44px"));
  SPRAWDZ(zawiera(h, "createElement('button')") && zawiera(h, "selectedIndex=j"));
  SPRAWDZ(!zawiera(h, "http://") && !zawiera(h, "https://") && !zawiera(h, "src="));
  // przesłanie strony bez zmian = układ domyślny, wszystkie cztery wiersze usuwane (R-KONF-20)
  std::map<std::string, std::string> m = poleFormularza(h);
  SPRAWDZ(m["ekrany_start"] == "1" && m["kk_start"] == "ostatnia_noc,7_nocy,sypialnia" && m["kk_sypialnia"] == "3");
  formularz::Wynik w = formularz::waliduj(m, s, "2026-11-14");
  SPRAWDZ(w.ok() && w.plik.count("kokpit.kolejnosc") && w.plik["kokpit.kolejnosc"].empty() && w.plik["kokpit.ukryte"].empty() &&
          w.plik["dzien.kolejnosc"].empty() && w.plik["dzien.ukryte"].empty());
  // bez JS: sypialnia na miejsce 1, „Jutro rano” w dół na 4, sypialnia i obrazek wyłączone
  std::map<std::string, std::string> z = m;
  z["kk_sypialnia"] = "1";
  z["dk_jutro_rano"] = "4";
  z.erase("pokaz_sypialnia");
  z.erase("pokaz_obrazek");
  w = formularz::waliduj(z, s, "2026-11-14");
  SPRAWDZ(w.ok() && w.plik["kokpit.kolejnosc"] == "sypialnia,ostatnia_noc,7_nocy" && w.plik["kokpit.ukryte"] == "sypialnia" &&
          w.plik["dzien.kolejnosc"] == "wietrzenie,wymiana,pylek,jutro_rano" && w.plik["dzien.ukryte"] == "obrazek");
  // zapis i odczyt: plik → formularz → ten sam układ; reszta pliku bez zmian
  std::string plik = formularz::scal("profil=publiczny\n# komentarz\n", w.plik);
  SPRAWDZ(zawiera(plik, "profil=publiczny\n# komentarz\n") && zawiera(plik, "kokpit.kolejnosc=sypialnia,ostatnia_noc,7_nocy\n") &&
          zawiera(plik, "dzien.ukryte=obrazek\n"));
  formularz::Pola p2 = formularz::zPliku(plik, s, true);
  std::string h2 = formularz::strona(p2, "/u/abc/", "", {});
  SPRAWDZ(zawiera(h2, "<span>3 strony · 5 ekranów dziennych</span>"));
  SPRAWDZ(h2.find("name=\"kk_sypialnia\"") < h2.find("name=\"kk_ostatnia_noc\"") &&
          h2.find("name=\"dk_pylek\"") < h2.find("name=\"dk_jutro_rano\""));
  SPRAWDZ(zawiera(h2, "name=\"pokaz_sypialnia\" value=\"1\">") && zawiera(h2, "name=\"pokaz_obrazek\" value=\"1\">"));
  // ponowne przesłanie strony po zapisie: bez zmian
  formularz::Wynik w2 = formularz::waliduj(poleFormularza(h2), s, "2026-11-14");
  SPRAWDZ(w2.plik["kokpit.kolejnosc"] == "sypialnia,ostatnia_noc,7_nocy" && w2.plik["dzien.ukryte"] == "obrazek" &&
          w2.plik["dzien.kolejnosc"] == "wietrzenie,wymiana,pylek,jutro_rano");
  // ze skryptem: strzałki zamieniają wiersze i numerują od nowa (spójna permutacja) → ten sam wynik serwera
  std::map<std::string, std::string> js = m;
  js["kk_ostatnia_noc"] = "2";
  js["kk_7_nocy"] = "1";  // ▲ przy „7 nocy”
  SPRAWDZ(formularz::waliduj(js, s, "2026-11-14").plik["kokpit.kolejnosc"] == "7_nocy,ostatnia_noc,sypialnia");
  js = m;
  js["kk_ostatnia_noc"] = "3";
  js["kk_7_nocy"] = "1";
  js["kk_sypialnia"] = "2";
  SPRAWDZ(formularz::waliduj(js, s, "2026-11-14").plik["kokpit.kolejnosc"] == "7_nocy,sypialnia,ostatnia_noc");
  // wszystkie 24 kolejności dnia przez numerację (jak po strzałkach) dają dokładnie tę kolejność
  int a[4] = {0, 1, 2, 3}, perm = 0;
  do {
    std::map<std::string, std::string> q = m;
    std::string ocz;
    for (int i = 0; i < 4; i++) {
      q[std::string("dk_") + ID_DZIENNY[a[i]]] = std::to_string(i + 1);
      ocz += (i ? "," : "") + std::string(ID_DZIENNY[a[i]]);
    }
    std::string got = formularz::waliduj(q, s, "2026-11-14").plik["dzien.kolejnosc"];
    SPRAWDZ(got == (ocz == "wietrzenie,jutro_rano,wymiana,pylek" ? "" : ocz));
    perm++;
  } while (std::next_permutation(a, a + 4));
  SPRAWDZ(perm == 24);
  // złe wartości: błąd przy polu, układ bez zmian (żaden klucz w pliku), sekcja otwarta; XSS bez echa
  std::map<std::string, std::string> zl = m;
  zl["kk_sypialnia"] = "<script>alert(1)</script>";
  zl["dk_pylek"] = "9";
  formularz::Wynik wz = formularz::waliduj(zl, s, "2026-11-14");
  SPRAWDZ(!wz.ok() && wz.bledy.count("kk_sypialnia") && wz.bledy.count("dk_pylek") && !wz.plik.count("kokpit.kolejnosc") &&
          !wz.plik.count("dzien.ukryte"));
  std::string hz = formularz::strona(p, "/u/abc/", "", wz.bledy);
  SPRAWDZ(zawiera(hz, "<details open><summary><b>Ekrany") && !zawiera(hz, "<script>alert") && ile(hz, "Wybierz miejsce z listy.") == 2);
  std::map<std::string, std::string> zs = m;
  zs["kk_start"] = "\"><script>alert(1)</script>";
  formularz::Wynik ws = formularz::waliduj(zs, s, "2026-11-14");
  SPRAWDZ(ws.bledy.count("ekrany_start") && !ws.plik.count("kokpit.kolejnosc"));
  SPRAWDZ(!zawiera(formularz::strona(p, "/u/abc/", "", ws.bledy), "<script>alert"));
  // plik z wartościami do HTML: nieznane nazwy nie trafiają na stronę (tylko stałe nazwy z listy)
  formularz::Pola px = formularz::zPliku("kokpit.kolejnosc=<script>x</script>,sypialnia\ndzien.ukryte=\"><img>\n", s, true);
  std::string hx = formularz::strona(px, "/u/abc/", "", {});
  SPRAWDZ(!zawiera(hx, "<script>x") && !zawiera(hx, "<img>") && hx.find("name=\"kk_sypialnia\"") < hx.find("name=\"kk_ostatnia_noc\""));
  // pola zawsze włączonych dopisane ręcznie do ciała POST nic nie zmieniają
  std::map<std::string, std::string> rz = m;
  rz["pokaz_ostatnia_noc"] = "";
  rz["kk_ustawienia"] = "1";
  rz["dk_drzemka"] = "1";
  rz["pokaz_drzemka"] = "";
  formularz::Wynik wr = formularz::waliduj(rz, s, "2026-11-14");
  SPRAWDZ(wr.ok() && wr.plik["kokpit.kolejnosc"].empty() && wr.plik["kokpit.ukryte"].empty() && wr.plik["dzien.ukryte"].empty());
  // formularz bez sekcji (stara strona w przeglądarce): układ bez zmian, żaden klucz
  std::map<std::string, std::string> stara = {{"profil", "prywatny"}};
  SPRAWDZ(!formularz::waliduj(stara, s, "2026-11-14").plik.count("kokpit.kolejnosc"));
  // ciało POST z całą sekcją mieści się w limicie MAKS_CIALA
  std::string cialo;
  for (auto& kv : m) cialo += (cialo.empty() ? "" : "&") + formularz::koduj(kv.first) + "=" + formularz::koduj(kv.second);
  SPRAWDZ(cialo.size() < formularz::MAKS_CIALA);
  printf("formularz: strona %zu B, sekcja „Ekrany” %zu B, ciało POST bez haseł %zu B (limit %zu B)\n", h.size(),
         h.find("</details>", h.find("<b>Ekrany</b>")) - h.find("<details><summary><b>Ekrany") + 10, cialo.size(),
         formularz::MAKS_CIALA);
  // ciało POST całej strony z sekcją „Ekrany” w najgorszym przypadku. Realny: każde pole tekstowe
  // z `maxlength` do końca znakami wymagającymi `%XX` (3 B), SSID 32 B UTF-8 (16 znaków dwubajtowych), pola bez
  // `maxlength` (daty, liczby, miasto) po 30 znaków dwubajtowych, ukryte z wartością ze strony, pola wyboru `1`, listy
  // 24 znaki. Teoretyczny: każde pole z `maxlength` do końca znakami dwubajtowymi (6 B po zakodowaniu). Limit ma mieć
  // co najmniej 2× zapasu wobec realnego i mieścić teoretyczny (inaczej 413 i nic nie zapisane).
  {
    formularz::Pola pp = formularz::zPliku("kokpit.kolejnosc=sypialnia,7_nocy\n", s, true);
    std::string hp = formularz::strona(pp, "/u/abc/", "", {});
    std::string f = hp.substr(hp.find("<form"), hp.find("</form>") - hp.find("<form"));
    auto atr = [](const std::string& t, const char* n) {
      std::string k = std::string(n) + "=\"";
      size_t a0 = t.find(k);
      return a0 == std::string::npos ? std::string() : t.substr(a0 + k.size(), t.find('"', a0 + k.size()) - a0 - k.size());
    };
    std::string realny, teoretyczny;
    int pol = 0;
    for (const char* tag : {"<input", "<select"}) {
      for (size_t q = f.find(tag); q != std::string::npos; q = f.find(tag, q + 1)) {
        std::string t = f.substr(q, f.find('>', q) - q), n = atr(t, "name"), typ = atr(t, "type"), ml = atr(t, "maxlength");
        if (n.empty()) continue;
        std::string r, wt;
        if (typ == "hidden") r = wt = atr(t, "value");
        else if (typ == "checkbox") r = wt = "1";
        else if (std::string(tag) == "<select") r = wt = std::string(24, 'x');
        else if (!ml.empty()) {
          int L = atoi(ml.c_str());
          std::string dwub;
          for (int i = 0; i < L; i++) dwub += "\xC5\xBC";  // ż
          r = n == "ssid" ? dwub.substr(0, 32) : std::string((size_t)L, '!');
          wt = dwub;
        } else {
          for (int i = 0; i < 30; i++) r += "\xC5\xBC";
          wt = r;
        }
        realny += (realny.empty() ? "" : "&") + formularz::koduj(n) + "=" + formularz::koduj(r);
        teoretyczny += (teoretyczny.empty() ? "" : "&") + formularz::koduj(n) + "=" + formularz::koduj(wt);
        pol++;
      }
    }
    SPRAWDZ(pol >= 45 && 2 * realny.size() <= formularz::MAKS_CIALA && teoretyczny.size() <= formularz::MAKS_CIALA);
    // ciało z sekcją przechodzi przez sesję (nie 413) i zapisuje układ
    formularz::Sesja ses;
    ses.otworz("tok", 1000, "192.168.1.9");
    formularz::Odpowiedz o = ses.obsluz("POST", "/u/tok/", teoretyczny, 1000, std::string());
    SPRAWDZ(o.kod == 200 && o.zapis && o.pola.count("kk_start") == 1);
    printf("ciało POST całej strony (%d pól): realny najgorszy %zu B, teoretyczny %zu B, MAKS_CIALA %zu B (zapas %.1f×)\n",
           pol, realny.size(), teoretyczny.size(), formularz::MAKS_CIALA, (double)formularz::MAKS_CIALA / (double)realny.size());
  }
}

// ---------- dyrygent na atrapach: plik konfiguracji → ciąg, numery, stopki, RTC ----------
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t ep(int d, int h, int m) {
  struct tm t = {};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 10;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
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
static bool wLogu(const Panel& p, const std::string& t) {
  for (auto& l : p.d.z)
    if (l.find(t) != std::string::npos) return true;
  return false;
}
static void panel(Panel& p, const std::string& konf) {
  p.zegar.epoch = ep(14, 10, 0);
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.pliki.pliki["/konfiguracja.txt"] = konf;
  std::string s;
  for (int i = 0; i < 30; i++) s += zapiszSypialnie(NocSypialni{dodajDni("2026-11-14", i - 29), 20, 45, NAN}) + "\n";
  p.pliki.pliki["/sypialnia.csv"] = s;
}
// wymaganie: R-KOK-34
static void testR_KOK_34_dyrygent() {
  // kolejność z pliku: sypialnia, 7 nocy, ostatnia noc
  {
    Panel p;
    panel(p, "kokpit.kolejnosc=sypialnia,7_nocy,ostatnia_noc\n");
    uint32_t t = ep(14, 10, 0);
    std::vector<Ekran> c;
    for (int i = 0; i < 10 && ekran(p) != Ekran::USTAWIENIA; i++) {
      przycisk(p, t += 30, BIT_PRAWY);
      c.push_back(ekran(p));
    }
    SPRAWDZ((c == std::vector<Ekran>{Ekran::SYPIALNIA, Ekran::K2, Ekran::K1, Ekran::USTAWIENIA}));
    SPRAWDZ(wLogu(p, "kokpit: układ z telefonu: sypialnia,7_nocy,ostatnia_noc"));
    przycisk(p, t += 30, BIT_LEWY);
    SPRAWDZ(ekran(p) == Ekran::K1);
    przycisk(p, t += 30, BIT_LEWY);
    SPRAWDZ(ekran(p) == Ekran::K2);
    przycisk(p, t += 30, BIT_LEWY);
    SPRAWDZ(ekran(p) == Ekran::SYPIALNIA);
    SPRAWDZ(wLogu(p, "sypialnia 30 nocy: stopka ◀ ekran dnia · ▶ 7 nocy · ↻ ekran dnia"));
    przycisk(p, t += 30, BIT_LEWY);
    SPRAWDZ(ekran(p) == Ekran::DZIEN);
  }
  // sypialnia ukryta: dane są, ale strony nie ma; ◀ z U-01 na K2; numer 3 / 3
  {
    Panel p;
    panel(p, "kokpit.ukryte=sypialnia\n");
    uint32_t t = ep(14, 10, 0);
    for (int i = 0; i < 10 && ekran(p) != Ekran::USTAWIENIA; i++) {
      przycisk(p, t += 30, BIT_PRAWY);
      SPRAWDZ(ekran(p) != Ekran::SYPIALNIA);
    }
    SPRAWDZ(ekran(p) == Ekran::USTAWIENIA);
    przycisk(p, t += 30, BIT_LEWY);
    SPRAWDZ(ekran(p) != Ekran::SYPIALNIA);
    // powrót po restarcie z pamięci RTC na stronę ukrytą: strona nigdy nie jest rysowana, a powrót (R-PRZ-02) daje ekran
    // dnia; wybudzenie, które rysuje, rysuje ekran dnia (sprawdzenie przed rysowaniem, log)
    for (int krok = 0; krok < 2; krok++) {
      if (krok == 0) p.start.stan.ekran = (uint8_t)Ekran::SYPIALNIA;
      p.zegar.epoch = krok == 0 ? t + 60 : t + 400;
      p.zegar.msTeraz = 0;
      p.d.z.clear();
      p.start.przyczynaStartu = krok == 0 ? PrzyczynaStartu::PROGRAMOWY : PrzyczynaStartu::USPIENIE;
      p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
      p.start.budzace = 0;
      p.przyciski.skrypt.clear();
      if (krok) zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
      Dyrygent(p.adaptery(), KONF).wybudzenie();
      SPRAWDZ(!wLogu(p, "sypialnia 30 nocy:"));
    }
    SPRAWDZ(ekran(p) == Ekran::DZIEN);
    // strona w RTC przy wybudzeniu, które rysuje (przycisk z kolejki pominięty: zegar przed 5 min z rysowaniem):
    // dyrygent przestawia na ekran dnia
    p.start.stan.ekran = (uint8_t)Ekran::SYPIALNIA;
    przycisk(p, t + 450, BIT_SRODEK);  // ↻ z dowolnej strony = ekran dnia (rysowanie)
    SPRAWDZ(ekran(p) == Ekran::DZIEN && !wLogu(p, "sypialnia 30 nocy:"));
    // naciśnięcie z RTC na stronie ukrytej: ▶ prowadzi tam, gdzie prowadziłaby, nigdy na nią samą
    p.start.stan.ekran = (uint8_t)Ekran::SYPIALNIA;
    przycisk(p, t + 500, BIT_PRAWY);
    SPRAWDZ(ekran(p) != Ekran::SYPIALNIA);
  }
  // ręcznie zmieniony plik z nazwami zawsze włączonych: K1, K2 i U-01 w ciągu, log z pominiętymi nazwami
  {
    Panel p;
    panel(p, "kokpit.ukryte=ostatnia_noc,7_nocy,ustawienia\nkokpit.kolejnosc=ustawienia,7_nocy\n");
    uint32_t t = ep(14, 10, 0);
    std::vector<Ekran> c;
    for (int i = 0; i < 10 && ekran(p) != Ekran::USTAWIENIA; i++) {
      przycisk(p, t += 30, BIT_PRAWY);
      c.push_back(ekran(p));
    }
    SPRAWDZ((c == std::vector<Ekran>{Ekran::K2, Ekran::K1, Ekran::SYPIALNIA, Ekran::USTAWIENIA}));
    SPRAWDZ(wLogu(p, "konfiguracja: pominięty wiersz 'kokpit.ukryte: pominięte 3 nazwy: ostatnia_noc zawsze włączone, "
                  "7_nocy zawsze włączone, ustawienia zawsze włączone'"));
  }
}

// wymaganie: R-KOK-34
// wymaganie: R-KOK-36
// układ kokpitu z pliku ustawiany na początku obsługi przycisku, przed silnikiem reguł, więc
// także przycisk w czuwaniu wybudzenia pomiarowego (bez wcześniejszego ustawienia układu w tym wybudzeniu) idzie za
// kolejnością z telefonu: w nocy ▶ z ekranu dnia otwiera pierwszą stronę ciągu (tu „7 nocy”), nie K1.
static void testR_KOK_34_pomiaroweZPrzyciskiem() {
  for (int przestawiony = 0; przestawiony < 2; przestawiony++) {
    Panel p;
    panel(p, przestawiony ? "kokpit.kolejnosc=7_nocy,ostatnia_noc,sypialnia\n" : "");
    p.zegar.epoch = ep(14, 2, 0) + 5;  // slot nocny: wybudzenie pomiarowe (T-WYM-31)
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    p.start.budzace = 0;
    p.przyciski.skrypt = {{0, BIT_PRAWY}, {300, 0}};  // przycisk wciśnięty w czuwaniu (R-PRZ-09)
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(wLogu(p, "wybudzenie pomiarowe"));
    SPRAWDZ(ekran(p) == (przestawiony ? Ekran::K2 : Ekran::K1));
    // teksty silnika (z numerem strony, R-KOK-36) liczone dopiero po ustawieniu układu z telefonu
    if (przestawiony) {
      int iUklad = -1, iSilnik = -1, i = 0;
      for (const std::string& l : p.port.log) {
        if (iUklad < 0 && l.find("kokpit: układ z telefonu") != std::string::npos) iUklad = i;
        if (iSilnik < 0 && l.find("silnik reguł") != std::string::npos) iSilnik = i;
        i++;
      }
      SPRAWDZ(iUklad >= 0 && iSilnik > iUklad);
      if (!(iUklad >= 0 && iSilnik > iUklad)) printf("  USTERKA: układ (wpis %d) po silniku reguł (wpis %d)\n", iUklad, iSilnik);
    }
    if (ekran(p) != (przestawiony ? Ekran::K2 : Ekran::K1))
      printf("  USTERKA: pomiarowe z ▶ w czuwaniu (%s): ekran %d\n", przestawiony ? "7 nocy pierwsza" : "domyślna",
             (int)ekran(p));
  }
}

int main() {
  testR_KOK_34_ciagWszystkichUkladow();
  testR_KOK_34_zgodnoscZBaza();
  testR_KOK_34_plikUszkodzony();
  testR_KOK_36_numeryWTekstach();
  testR_KON_70_kolejnoscDnia();
  testR_KON_71_wylaczone();
  testR_KON_72_zawszeWlaczone();
  testR_KON_70_zgodnoscZBaza();
  testR_KONF_60_formularz();
  testR_KOK_34_dyrygent();
  testR_KOK_34_pomiaroweZPrzyciskiem();
  printf("testy kolejności ekranów: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
