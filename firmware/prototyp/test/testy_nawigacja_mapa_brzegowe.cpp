// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (nowa nawigacja panelu): test tabelaryczny mapy 5.6.2 (każdy ekran × 6 akcji:
// ◀, ◀ 2 s, ▶, ▶ 2 s, ↻, ↻ 2 s), stopki z mapą (R-PRZ-04) i przypadki R31-nn. TRZYMA = zachowanie zgodne z mapą
// (powinno przechodzić), PRZYPADEK = szkoda dla użytkownika (pada, dopóki nie poprawione albo nie odrzucone
// decyzją projektową).
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "dzien.h"
#include "ekrany.h"
#include "nawigacja.h"
#include "sprzet.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0, znal = 0;
#define TRZYMA(w)                                                                   \
  do {                                                                              \
    if (w) ok++;                                                                    \
    else { zle++; printf("BŁĄD (mapa 5.6.2) %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define PRZYPADEK(nr, w)                                                              \
  do {                                                                                 \
    if (w) ok++;                                                                       \
    else { znal++; printf("BŁĄD %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); } \
  } while (0)

static const Przycisk L = Przycisk::LEWY, P = Przycisk::PRAWY, S = Przycisk::SRODEK;
static const Akcja X = Akcja::NIC;  // „nic”: ekran bez zmiany, akcja NIC

static std::string nazwa(Ekran e) {
  static const char* const N[] = {"DZIEN", "K1", "K2", "PYTANIE", "TEST", "OMOWIONE", "SPRAWY", "PRZENIESIONE",
                                  "ZAPISANO", "RAPORT", "NORMA", "LICZBY", "SPRZET", "SPRZET_ZAPISANO", "USTAWIENIA",
                                  "FORMULARZ", "SIEC_PANELU", "AKTUALIZACJA"};
  return N[(int)e];
}

// Kontekst bazowy: silnik działa, 3 karty spraw, karta 1, jest ostatnia noc, kursor „Sprzęt” na masce.
static KontekstNaw baza() {
  KontekstNaw k;
  k.sprawyDostepne = true;
  k.spraw = 3;
  k.sprawa = 1;
  k.ostatniaNoc = true;
  k.kursorSprzetu = 0;
  k.wierszySprzetu = EL_N;
  return k;
}

// Jeden wiersz mapy 5.6.2: oczekiwany (ekran, akcja) dla ◀, ◀ 2 s, ▶, ▶ 2 s, ↻, ↻ 2 s. Ekran = tam, dokąd prowadzi
// przejście; dla „nic” podany ekran wiersza i X.
struct Cel {
  Ekran e;
  Akcja a;
};
struct Wiersz {
  const char* opis;
  Ekran e;
  KontekstNaw k;
  Cel c[6];
};

static void testMapy() {
  const Ekran D = Ekran::DZIEN;
  std::vector<Wiersz> w;
  KontekstNaw k = baza();
  // ekran dnia „spokój” (pierwszy wiersz mapy), bez linii zaproszenia
  w.push_back({"dzień spokój", D, k, {{D, X}, {D, X}, {Ekran::K1, Akcja::POKAZ}, {D, X}, {D, X}, {D, X}}});
  KontekstNaw z = k;
  z.zaproszenie = true;
  w.push_back({"dzień z zaproszeniem", D, z,
               {{D, X}, {D, X}, {Ekran::K1, Akcja::POKAZ}, {D, X}, {D, X}, {Ekran::PYTANIE, Akcja::OTWORZ_PYTANIE}}});
  KontekstNaw s3 = k;
  s3.wzorzec = s3.problem = true;
  w.push_back({"S3 problem poranny", D, s3,
               {{D, X}, {D, X}, {Ekran::K1, Akcja::POKAZ}, {D, X}, {D, Akcja::POKAZ}, {Ekran::PRZENIESIONE, Akcja::WIEM}}});
  KontekstNaw sk = k;
  sk.sk31 = true;
  w.push_back({"S-K-31", D, sk,
               {{D, X}, {Ekran::SPRZET_ZAPISANO, Akcja::SPRZET_WYMIENIONE}, {Ekran::K1, Akcja::POKAZ}, {D, X}, {D, X}, {D, X}}});
  KontekstNaw s5 = k;
  s5.s5 = true;
  w.push_back({"S5", D, s5, {{D, X}, {D, X}, {Ekran::K1, Akcja::POKAZ}, {D, X}, {D, Akcja::PONOW_POBRANIE}, {D, X}}});
  KontekstNaw s9 = k;
  s9.nocny = true;
  s9.zaproszenie = true;  // w nocy linia zaproszenia nie działa (S9: ↻ 2 s nic)
  w.push_back({"S9", D, s9, {{D, X}, {D, X}, {Ekran::K1, Akcja::POKAZ}, {D, X}, {D, X}, {D, X}}});
  KontekstNaw l = k;
  l.wzorzec = true;
  w.push_back({"L (A, B)", D, l,
               {{D, X}, {Ekran::OMOWIONE, Akcja::POKAZ}, {Ekran::K1, Akcja::POKAZ}, {Ekran::RAPORT, Akcja::POKAZ},
                {D, Akcja::POKAZ}, {Ekran::PRZENIESIONE, Akcja::WIEM}}});
  KontekstNaw tn = k;
  tn.pytanieTN = true;
  w.push_back({"pytanie tak/nie", D, tn,
               {{Ekran::ZAPISANO, Akcja::ODP_NIE}, {D, X}, {Ekran::ZAPISANO, Akcja::ODP_TAK}, {D, X},
                {D, Akcja::ODP_POMIN}, {D, X}}});
  KontekstNaw bk = k;
  bk.bezKonfiguracji = true;
  w.push_back({"dzień bez konfiguracji (U-02 instrukcja)", D, bk,
               {{D, X}, {Ekran::SIEC_PANELU, Akcja::SIEC_PANELU_START}, {Ekran::K1, Akcja::POKAZ}, {D, X}, {D, X},
                {D, X}}});
  w.push_back({"Q-omowione", Ekran::OMOWIONE, k,
               {{D, Akcja::POKAZ}, {Ekran::OMOWIONE, X}, {D, Akcja::OMOWIONE_TAK}, {Ekran::OMOWIONE, X},
                {D, Akcja::POKAZ}, {Ekran::OMOWIONE, X}}});
  KontekstNaw ob = k;
  ob.omowienieZablokowane = true;
  w.push_back({"Q-omowione blokada", Ekran::OMOWIONE, ob,
               {{D, Akcja::POKAZ}, {Ekran::OMOWIONE, X}, {Ekran::OMOWIONE, X}, {Ekran::OMOWIONE, X},
                {D, Akcja::POKAZ}, {Ekran::OMOWIONE, X}}});
  w.push_back({"raport z L", Ekran::RAPORT, k,
               {{D, Akcja::POKAZ}, {Ekran::RAPORT, X}, {Ekran::K1, Akcja::POKAZ}, {Ekran::RAPORT, X},
                {D, Akcja::POKAZ}, {Ekran::RAPORT, Akcja::RAPORT_STARSZE}}});
  KontekstNaw rk = k;
  rk.zrodlo = Ekran::K2;
  w.push_back({"raport z K2", Ekran::RAPORT, rk,
               {{Ekran::K2, Akcja::POKAZ}, {Ekran::RAPORT, X}, {Ekran::SPRAWY, Akcja::SPRAWY_OTWORZ},
                {Ekran::RAPORT, X}, {D, Akcja::POKAZ}, {Ekran::RAPORT, Akcja::RAPORT_STARSZE}}});
  w.push_back({"K1", Ekran::K1, k,
               {{D, Akcja::POKAZ}, {Ekran::K1, X}, {Ekran::K2, Akcja::POKAZ}, {Ekran::K1, X}, {D, Akcja::POKAZ},
                {Ekran::PYTANIE, Akcja::OTWORZ_PYTANIE}}});
  w.push_back({"K2", Ekran::K2, k,
               {{Ekran::K1, Akcja::POKAZ}, {Ekran::K2, X}, {Ekran::SPRAWY, Akcja::SPRAWY_OTWORZ},
                {Ekran::RAPORT, Akcja::POKAZ}, {D, Akcja::POKAZ}, {Ekran::K2, X}}});
  KontekstNaw bs = k;
  bs.sprawyDostepne = false;
  bs.spraw = 0;
  w.push_back({"K2 bez silnika", Ekran::K2, bs,
               {{Ekran::K1, Akcja::POKAZ}, {Ekran::K2, X}, {Ekran::NORMA, Akcja::POKAZ}, {Ekran::RAPORT, Akcja::POKAZ},
                {D, Akcja::POKAZ}, {Ekran::K2, X}}});
  w.push_back({"sprawy 1/3", Ekran::SPRAWY, k,
               {{Ekran::K2, Akcja::POKAZ}, {Ekran::SPRAWY, X}, {Ekran::SPRAWY, Akcja::SPRAWY_DALEJ}, {Ekran::SPRAWY, X},
                {D, Akcja::POKAZ}, {Ekran::SPRAWY, X}}});
  KontekstNaw s2 = k;
  s2.sprawa = 2;
  w.push_back({"sprawy 2/3", Ekran::SPRAWY, s2,
               {{Ekran::SPRAWY, Akcja::SPRAWY_WSTECZ}, {Ekran::SPRAWY, X}, {Ekran::SPRAWY, Akcja::SPRAWY_DALEJ},
                {Ekran::SPRAWY, X}, {D, Akcja::POKAZ}, {Ekran::SPRAWY, X}}});
  KontekstNaw s33 = k;
  s33.sprawa = 3;
  w.push_back({"sprawy 3/3", Ekran::SPRAWY, s33,
               {{Ekran::SPRAWY, Akcja::SPRAWY_WSTECZ}, {Ekran::SPRAWY, X}, {Ekran::NORMA, Akcja::POKAZ},
                {Ekran::SPRAWY, X}, {D, Akcja::POKAZ}, {Ekran::SPRAWY, X}}});
  KontekstNaw sp = k;
  sp.spraw = 0;
  w.push_back({"sprawy stan pusty", Ekran::SPRAWY, sp,
               {{Ekran::K2, Akcja::POKAZ}, {Ekran::SPRAWY, X}, {Ekran::NORMA, Akcja::POKAZ}, {Ekran::SPRAWY, X},
                {D, Akcja::POKAZ}, {Ekran::SPRAWY, X}}});
  w.push_back({"S8", Ekran::NORMA, k,
               {{Ekran::SPRAWY, Akcja::SPRAWY_OD_KONCA}, {Ekran::NORMA, X}, {Ekran::SPRZET, Akcja::SPRZET_OTWORZ},
                {Ekran::NORMA, X}, {D, Akcja::POKAZ}, {Ekran::NORMA, X}}});
  w.push_back({"S8 bez silnika", Ekran::NORMA, bs,
               {{Ekran::K2, Akcja::POKAZ}, {Ekran::NORMA, X}, {Ekran::SPRZET, Akcja::SPRZET_OTWORZ}, {Ekran::NORMA, X},
                {D, Akcja::POKAZ}, {Ekran::NORMA, X}}});
  KontekstNaw pub = k;
  pub.profilPubliczny = true;
  w.push_back({"S8 profil publiczny", Ekran::NORMA, pub,
               {{Ekran::SPRAWY, Akcja::SPRAWY_OD_KONCA}, {Ekran::NORMA, X}, {Ekran::LICZBY, Akcja::POKAZ},
                {Ekran::NORMA, X}, {D, Akcja::POKAZ}, {Ekran::NORMA, X}}});
  w.push_back({"Co znaczą te liczby", Ekran::LICZBY, pub,
               {{Ekran::NORMA, Akcja::POKAZ}, {Ekran::LICZBY, X}, {Ekran::SPRZET, Akcja::SPRZET_OTWORZ},
                {Ekran::LICZBY, X}, {D, Akcja::POKAZ}, {Ekran::LICZBY, X}}});
  w.push_back({"Sprzęt, maska", Ekran::SPRZET, k,
               {{Ekran::NORMA, Akcja::POKAZ}, {Ekran::SPRZET_ZAPISANO, Akcja::SPRZET_WYMIENIONE},
                {Ekran::SPRZET, Akcja::SPRZET_DALEJ}, {Ekran::SPRZET, X}, {D, Akcja::POKAZ},
                {Ekran::SPRZET_ZAPISANO, Akcja::SPRZET_NOWY_MODEL}}});
  KontekstNaw pm = pub;
  w.push_back({"Sprzęt, maska, profil publiczny", Ekran::SPRZET, pm,
               {{Ekran::LICZBY, Akcja::POKAZ}, {Ekran::SPRZET_ZAPISANO, Akcja::SPRZET_WYMIENIONE},
                {Ekran::SPRZET, Akcja::SPRZET_DALEJ}, {Ekran::SPRZET, X}, {D, Akcja::POKAZ},
                {Ekran::SPRZET_ZAPISANO, Akcja::SPRZET_NOWY_MODEL}}});
  KontekstNaw kf = k;
  kf.kursorSprzetu = 1;
  w.push_back({"Sprzęt, filtr", Ekran::SPRZET, kf,
               {{Ekran::SPRZET, Akcja::SPRZET_WSTECZ}, {Ekran::SPRZET_ZAPISANO, Akcja::SPRZET_WYMIENIONE},
                {Ekran::USTAWIENIA, Akcja::POKAZ}, {Ekran::SPRZET, X}, {D, Akcja::POKAZ}, {Ekran::SPRZET, X}}});
  w.push_back({"U-01", Ekran::USTAWIENIA, k,
               {{Ekran::SPRZET, Akcja::SPRZET_OD_KONCA}, {Ekran::SIEC_PANELU, Akcja::SIEC_PANELU_START},
                {Ekran::USTAWIENIA, X}, {Ekran::FORMULARZ, Akcja::POKAZ}, {D, Akcja::POKAZ},
                {Ekran::AKTUALIZACJA, Akcja::AKT_SPRAWDZ}}});  // ↻ 2 s: U-07 (R-KONF-43, aktualizacja z GitHub)
  KontekstNaw u7 = k;
  u7.aktualizacja = 1;  // U-07 „jest nowsza” (R-KONF-44)
  w.push_back({"U-07 jest nowsza", Ekran::AKTUALIZACJA, u7,
               {{Ekran::USTAWIENIA, Akcja::POKAZ}, {Ekran::AKTUALIZACJA, X}, {Ekran::AKTUALIZACJA, Akcja::AKT_INSTALUJ},
                {Ekran::AKTUALIZACJA, X}, {D, Akcja::POKAZ}, {Ekran::AKTUALIZACJA, X}}});
  KontekstNaw u2 = k;
  u2.zrodlo = Ekran::USTAWIENIA;
  w.push_back({"U-02 z U-01", Ekran::SIEC_PANELU, u2,
               {{Ekran::USTAWIENIA, Akcja::SIEC_PANELU_ANULUJ}, {Ekran::USTAWIENIA, Akcja::SIEC_PANELU_ANULUJ},
                {Ekran::SIEC_PANELU, X}, {Ekran::SIEC_PANELU, X}, {D, Akcja::SIEC_PANELU_ANULUJ},
                {Ekran::SIEC_PANELU, Akcja::SIEC_PANELU_START}}});
  KontekstNaw u5 = k;
  u5.zrodlo = Ekran::TEST;
  w.push_back({"U-02 z U-05", Ekran::SIEC_PANELU, u5,
               {{Ekran::TEST, Akcja::SIEC_PANELU_ANULUJ}, {Ekran::TEST, Akcja::SIEC_PANELU_ANULUJ},
                {Ekran::SIEC_PANELU, X}, {Ekran::SIEC_PANELU, X}, {D, Akcja::SIEC_PANELU_ANULUJ},
                {Ekran::SIEC_PANELU, Akcja::SIEC_PANELU_START}}});
  w.push_back({"U-03", Ekran::FORMULARZ, k,
               {{Ekran::USTAWIENIA, Akcja::POKAZ}, {Ekran::FORMULARZ, X}, {Ekran::FORMULARZ, X},
                {Ekran::TEST, Akcja::POKAZ}, {D, Akcja::POKAZ}, {Ekran::FORMULARZ, X}}});
  w.push_back({"U-05", Ekran::TEST, k,
               {{Ekran::USTAWIENIA, Akcja::POKAZ}, {Ekran::SIEC_PANELU, Akcja::SIEC_PANELU_START},
                {Ekran::TEST, Akcja::PONOW_POBRANIE}, {Ekran::TEST, X}, {D, Akcja::POKAZ}, {Ekran::TEST, X}}});
  w.push_back({"pytanie „Co było inaczej?”", Ekran::PYTANIE, k,
               {{Ekran::PYTANIE, Akcja::PYT_WSTECZ}, {D, Akcja::PYT_POMIN}, {Ekran::PYTANIE, Akcja::PYT_DALEJ},
                {Ekran::PYTANIE, X}, {Ekran::PYTANIE, Akcja::PYT_ZAZNACZ}, {D, Akcja::POKAZ}}});  // bez ruchu bez zapisu
  KontekstNaw pr = k;
  pr.pytanieRuszone = true;
  w.push_back({"pytanie „Co było inaczej?” po zmianie w siatce", Ekran::PYTANIE, pr,
               {{Ekran::PYTANIE, Akcja::PYT_WSTECZ}, {D, Akcja::PYT_POMIN}, {Ekran::PYTANIE, Akcja::PYT_DALEJ},
                {Ekran::PYTANIE, X}, {Ekran::PYTANIE, Akcja::PYT_ZAZNACZ}, {D, Akcja::PYT_ZAPISZ}}});
  w.push_back({"Przeniesione", Ekran::PRZENIESIONE, k,
               {{D, Akcja::POKAZ}, {D, Akcja::COFNIJ_WIEM}, {D, Akcja::POKAZ}, {D, Akcja::POKAZ}, {D, Akcja::POKAZ},
                {D, Akcja::POKAZ}}});
  KontekstNaw bc = k;
  bc.cofniecie = false;
  w.push_back({"Przeniesione bez cofnięcia (noc po 30 s)", Ekran::PRZENIESIONE, bc,
               {{D, Akcja::POKAZ}, {D, Akcja::POKAZ}, {D, Akcja::POKAZ}, {D, Akcja::POKAZ}, {D, Akcja::POKAZ},
                {D, Akcja::POKAZ}}});
  w.push_back({"Nowy model maski zapisany", Ekran::SPRZET_ZAPISANO, k,
               {{Ekran::SPRZET, Akcja::POKAZ}, {Ekran::SPRZET, Akcja::SPRZET_COFNIJ}, {Ekran::SPRZET, Akcja::POKAZ},
                {Ekran::SPRZET, Akcja::POKAZ}, {D, Akcja::POKAZ}, {Ekran::SPRZET, Akcja::POKAZ}}});
  w.push_back({"Zapisano", Ekran::ZAPISANO, k,
               {{D, Akcja::POKAZ}, {D, Akcja::COFNIJ_ODP}, {D, Akcja::POKAZ}, {D, Akcja::POKAZ}, {D, Akcja::POKAZ},
                {D, Akcja::POKAZ}}});

  const Przycisk PR[6] = {L, L, P, P, S, S};
  const bool DL[6] = {false, true, false, true, false, true};
  const char* const AK[6] = {"◀", "◀ 2 s", "▶", "▶ 2 s", "↻", "↻ 2 s"};
  int komorek = 0;
  for (auto& r : w)
    for (int i = 0; i < 6; i++) {
      Przejscie p = nacisniecie(r.e, PR[i], DL[i], r.k);
      bool dobrze = p.ekran == r.c[i].e && p.akcja == r.c[i].a;
      komorek++;
      if (dobrze) ok++;
      else {
        zle++;
        printf("BŁĄD (mapa 5.6.2) %s, %s: jest %s/%d, ma być %s/%d\n", r.opis, AK[i], nazwa(p.ekran).c_str(),
               (int)p.akcja, nazwa(r.c[i].e).c_str(), (int)r.c[i].a);
      }
    }
  printf("  mapa 5.6.2: %zu wierszy × 6 akcji = %d komórek\n", w.size(), komorek);
}

// Krótkie naciśnięcie zapisuje tylko odpowiedź tak/nie i „tak, omówione” (5.6.1 pkt 4); przytrzymanie nigdy nie otwiera
// strony do przeglądania (K1, K2, sprawy, S8, liczby, U-01, U-03, U-05).
static bool zapis(Akcja a) {
  switch (a) {
    case Akcja::WIEM: case Akcja::OMOWIONE_TAK: case Akcja::ODP_TAK: case Akcja::ODP_NIE: case Akcja::COFNIJ_WIEM:
    case Akcja::COFNIJ_ODP: case Akcja::SPRZET_WYMIENIONE: case Akcja::SPRZET_NOWY_MODEL: case Akcja::SPRZET_COFNIJ:
    case Akcja::PYT_ZAPISZ: return true;
    default: return false;
  }
}
static void testZasad() {
  std::vector<KontekstNaw> ks;
  KontekstNaw k = baza();
  ks.push_back(k);
  for (int i = 0; i < 12; i++) {
    KontekstNaw x = k;
    x.s5 = i == 0; x.nocny = i == 1; x.wzorzec = i == 2 || i == 3; x.problem = i == 3; x.pytanieTN = i == 4;
    x.sk31 = i == 5; x.zaproszenie = i == 6; x.profilPubliczny = i == 7; x.sprawa = i == 8 ? 3 : 1;
    x.kursorSprzetu = i == 9 ? 1 : 0; x.cofniecie = i != 10; x.omowienieZablokowane = i == 11;
    ks.push_back(x);
  }
  const Ekran E[] = {Ekran::DZIEN, Ekran::K1, Ekran::K2, Ekran::PYTANIE, Ekran::TEST, Ekran::OMOWIONE, Ekran::SPRAWY,
                     Ekran::PRZENIESIONE, Ekran::ZAPISANO, Ekran::RAPORT, Ekran::NORMA, Ekran::LICZBY, Ekran::SPRZET,
                     Ekran::SPRZET_ZAPISANO, Ekran::USTAWIENIA, Ekran::FORMULARZ, Ekran::SIEC_PANELU};
  for (auto& x : ks)
    for (Ekran e : E)
      for (Przycisk p : {L, P, S}) {
        Przejscie kr = nacisniecie(e, p, false, x);
        if (zapis(kr.akcja))
          TRZYMA(kr.akcja == Akcja::ODP_TAK || kr.akcja == Akcja::ODP_NIE || kr.akcja == Akcja::OMOWIONE_TAK);
        Przejscie dl = nacisniecie(e, p, true, x);
        bool strona = dl.ekran != e && (dl.ekran == Ekran::K1 || dl.ekran == Ekran::K2 || dl.ekran == Ekran::SPRAWY ||
                                        dl.ekran == Ekran::NORMA || dl.ekran == Ekran::LICZBY ||
                                        dl.ekran == Ekran::USTAWIENIA || dl.ekran == Ekran::FORMULARZ ||
                                        (dl.ekran == Ekran::TEST && dl.akcja != Akcja::SIEC_PANELU_ANULUJ));
        // usługi z Wi-Fi tylko przytrzymaniem ▶: U-01 ▶ 2 s U-03, U-03 ▶ 2 s U-05
        if (p == P && ((e == Ekran::USTAWIENIA && dl.ekran == Ekran::FORMULARZ) || (e == Ekran::FORMULARZ && dl.ekran == Ekran::TEST)))
          strona = false;
        TRZYMA(!strona);
        if (strona) printf("  przytrzymanie %d na %s otwiera %s\n", (int)p, nazwa(e).c_str(), nazwa(dl.ekran).c_str());
      }
  // „Omówione” krótkim naciśnięciem tylko z ekranu potwierdzenia; potwierdzenie tylko po ◀ 2 s na L (nie na S3)
  for (auto& x : ks)
    for (Ekran e : E)
      for (Przycisk p : {L, P, S})
        for (bool d : {false, true}) {
          Przejscie r = nacisniecie(e, p, d, x);
          if (r.akcja == Akcja::OMOWIONE_TAK) TRZYMA(e == Ekran::OMOWIONE && p == P && !d && !x.omowienieZablokowane);
          if (r.ekran == Ekran::OMOWIONE && e != Ekran::OMOWIONE)
            TRZYMA(e == Ekran::DZIEN && p == L && d && x.wzorzec && !x.problem);
          if (r.akcja == Akcja::WIEM) TRZYMA(e == Ekran::DZIEN && p == S && d && x.wzorzec);
        }
  // powrót: potwierdzenie „Omówione” wraca po 60 s także w nocy (R-PRZ-05), strony kokpitu po 5 min, w nocy nie
  TRZYMA(sekundDoPowrotu(Ekran::OMOWIONE, 60, 2 * 60) == 0);
  TRZYMA(sekundDoPowrotu(Ekran::OMOWIONE, 0, 2 * 60) == 60);
  for (Ekran e : {Ekran::K1, Ekran::K2, Ekran::SPRAWY, Ekran::NORMA, Ekran::LICZBY, Ekran::SPRZET, Ekran::USTAWIENIA}) {
    TRZYMA(sekundDoPowrotu(e, 300, 8 * 60) == 0);
    TRZYMA(sekundDoPowrotu(e, 300, 2 * 60) == -1);
  }
  // wybudzenie zegarem po 5 min: strona kokpitu → ekran dnia (U-01 → K2, R-KONF-05 do decyzji)
  TRZYMA(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::SPRZET, true, 301, 8 * 60) == Ekran::DZIEN);
  TRZYMA(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::SPRAWY, true, 301, 8 * 60) == Ekran::DZIEN);
  TRZYMA(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::SPRAWY, true, 3000, 2 * 60) == Ekran::SPRAWY);  // noc
  TRZYMA(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::OMOWIONE, true, 61, 2 * 60) == Ekran::DZIEN);
}

static bool ma(const std::vector<std::string>& g, const std::string& t) {
  for (auto& s : g)
    if (s.find(t) != std::string::npos) return true;
  return false;
}
static bool ma(const std::string& s, const std::string& t) { return s.find(t) != std::string::npos; }

// Stopki (5.6.1 pkt 6, R-PRZ-04): podpis każdej akcji zgodny z tabelą wyżej.
static void testStopek() {
  std::vector<std::string> k1 = stopkaK1(true);
  TRZYMA(k1.size() == 3 && ma(k1[0], "◀ ekran dnia") && ma(k1[1], "▶") && ma(k1[2], "↻ ekran dnia") &&
         ma(k1[2], "2 s co było inaczej"));
  TRZYMA(!ma(stopkaK1(false), "2 s"));
  std::vector<std::string> k2 = stopkaK2(true);
  TRZYMA(k2.size() == 3 && ma(k2[0], "◀ ostatnia noc") && ma(k2[1], "▶ sprawy") && ma(k2[1], "2 s raport") &&
         k2[2] == "↻ ekran dnia");
  TRZYMA(ma(stopkaK2(false)[1], "▶ norma"));
  TRZYMA(stopkaSpraw(1, 3) == std::vector<std::string>({"◀ 7 nocy", "▶ następna", "↻ ekran dnia"}));
  TRZYMA(stopkaSpraw(3, 3)[1] == "▶ norma" && stopkaSpraw(2, 3)[0] == "◀ poprzednia");
  TRZYMA(stopkaSpraw(1, 0)[0] == "◀ 7 nocy" && stopkaSpraw(1, 0)[1] == "▶ norma");
  std::string m = sprzet::stopkaSprzetu(EL_MASKA), f = sprzet::stopkaSprzetu(EL_FILTR);
  TRZYMA(ma(m, "◀ norma · 2 s wymienione") && ma(m, "▶ niżej") && ma(m, "↻ ekran dnia · 2 s nowy model"));
  TRZYMA(ma(f, "◀ wyżej · 2 s wymienione") && ma(f, "▶ ustawienia") && !ma(f, "nowy model"));
  TRZYMA(stopkaWzorca(true) == std::vector<std::string>({"◀ 2 s omówione", "▶ kokpit · 2 s raport", "↻ 2 s wiem"}));
  TRZYMA(stopkaWzorca(false) == std::vector<std::string>({"▶ kokpit", "↻ 2 s wiem"}));
  TRZYMA(stopkaPytania() == std::vector<std::string>({"◀ wstecz · 2 s pomiń", "▶ dalej", "↻ zaznacz · 2 s zapisz"}));
  std::vector<std::string> t = stopkaTestu(false);
  TRZYMA(t.size() == 3 && ma(t[0], "◀ ustawienia · 2 s sieć panelu") && t[1] == "▶ odśwież" && t[2] == "↻ ekran dnia");
  TRZYMA(numerStronyTekst(Ekran::K1) == "1 / 6" && numerStronyTekst(Ekran::USTAWIENIA) == "6 / 6" &&
         numerStronyTekst(Ekran::LICZBY) == "4 / 6" && numerStronyTekst(Ekran::RAPORT).empty() &&
         numerStronyTekst(Ekran::FORMULARZ).empty());
}

// ---------- poprawki mapy przycisków ----------
static void testPoprawkiMapy() {
  KontekstNaw k = baza();
  // na ekranie dnia przycisk, którego krótkie naciśnięcie to „nic” (brak zmiany ekranu, tylko pisk),
  // a przytrzymanie to zapis bez potwierdzenia: L i S3 ↻ → „wiem”, S-K-31 ◀ → „wymienione dziś” (bez cofnięcia).
  // Przed krótkie ↻ na L dawało K1, krótkie ◀ na S-K-31 K2: pacjent widział reakcję. Teraz „nic się nie dzieje”
  // zachęca do mocniejszego, dłuższego naciśnięcia, a ono zapisuje.
  {
    KontekstNaw l = k, s3 = k, sk = k;
    l.wzorzec = true;
    s3.wzorzec = s3.problem = true;
    sk.sk31 = true;
    struct { KontekstNaw k; Przycisk p; } pary[] = {{l, S}, {s3, S}, {sk, L}};
    for (auto& x : pary) {
      bool cicho = nacisniecie(Ekran::DZIEN, x.p, false, x.k).akcja == Akcja::NIC;
      Akcja dl = nacisniecie(Ekran::DZIEN, x.p, true, x.k).akcja;
      // przytrzymanie z potwierdzeniem i cofnięciem (ekran inny niż ekran dnia) nie jest „zapisem bez reakcji”
      Ekran ed = nacisniecie(Ekran::DZIEN, x.p, true, x.k).ekran;
      PRZYPADEK("mapa nawigacji", !(cicho && (dl == Akcja::WIEM || dl == Akcja::SPRZET_WYMIENIONE) && ed == Ekran::DZIEN));
    }
  }
  // w „Sprzęt” ◀ to teraz przycisk przewijania (kursor w górę, poprzednia strona), a ◀ 2 s zapisuje
  // „wymienione dziś” od razu, bez ekranu z cofnięciem (inaczej niż ↻ 2 s „nowy model”); przytrzymanie przy
  // przewijaniu zeruje wiek maski albo filtra, a poprawka jest tylko przez plik konfiguracji (R-MAS-01 fail-path).
  {
    Przejscie w = nacisniecie(Ekran::SPRZET, L, true, k);
    PRZYPADEK("mapa nawigacji", w.akcja != Akcja::SPRZET_WYMIENIONE || w.ekran != Ekran::SPRZET);
  }
  // ↻ = „dom” nie działa w siatce „Co było inaczej?”: K1 ↻ przytrzymane ≥ 2 s (pacjent chce do ekranu
  // dnia, panel reaguje wolno) otwiera siatkę, w niej ↻ zaznacza, a ↻ 2 s zapisuje. Sam zielony przycisk, naciskany
  // „do domu”, zapisuje odpowiedź za ostatnią noc.
  {
    Ekran e = Ekran::K1;
    bool zapisano = false;
    struct { bool d; } seq[] = {{true}, {false}, {true}};
    for (auto& s : seq) {
      Przejscie r = nacisniecie(e, S, s.d, k);
      zapisano = zapisano || r.akcja == Akcja::PYT_ZAPISZ;
      e = r.ekran;
    }
    PRZYPADEK("mapa nawigacji", !zapisano);
  }
  // przeglądanie ▶ włącza usługi z Wi-Fi: U-01 ▶ U-03 (Wi-Fi i serwer formularza, R-KONF-12), U-03 ▶
  // U-05 (pobranie i okno wgrywania przez Wi-Fi na 10 min, R-NF-29). 5.6.1 pkt 4: „przeglądanie ▶ nie może jej
  // włączać” (dlatego raport ogólny przeszedł na ▶ 2 s). Scenariusz symulatora sam naciska ▶ 7 razy z rzędu.
  {
    KontekstNaw x = k;
    x.spraw = 0;
    Ekran e = Ekran::DZIEN;
    bool wifi = false;
    for (int i = 0; i < 12; i++) {
      Ekran przed = e;
      Przejscie r = nacisniecie(e, P, false, x);
      if (r.akcja == Akcja::SPRZET_OTWORZ) x.kursorSprzetu = 0;
      if (r.akcja == Akcja::SPRZET_DALEJ) x.kursorSprzetu++;
      e = r.ekran;
      Pobranie pb = pobraniePoNacisnieciu(r, przed, false);
      wifi = wifi || e == Ekran::FORMULARZ || pb.pobierz || otworzycOknoSerwisowe(e);
    }
    PRZYPADEK("mapa nawigacji", !wifi);
  }
  // karta „Otwartych spraw” w NVS (`b5_karta`) bez przycięcia do liczby kart: dyrygent podaje
  // kn.sprawa = b5.karta, a ekran rysuje min. Gdy liczba kart spadnie przy otwartym ekranie (nowa noc o
  // 04:30 w slocie nocnym, ekran stoi do 05:00), ◀ cofa niewidoczną kartę: kilka naciśnięć bez zmiany ekranu.
  {
    std::vector<sprawy::KartaSprawy> karty(2);
    karty[0].tytul = "A";
    karty[1].tytul = "B";
    sprawy::WejscieSpraw ws;
    int karta = przytnijKarte(4, 2);  // zapamiętana z wczoraj (4 z 4), dziś 2 karty: przycięta przy odczycie
    KontekstNaw x = k;
    x.sprawa = karta;
    x.spraw = 2;
    int przed = trescSpraw(karty, karta, ws).i;
    Przejscie r = nacisniecie(Ekran::SPRAWY, L, false, x);
    if (r.akcja == Akcja::SPRAWY_WSTECZ) karta = karta > 1 ? karta - 1 : 1;
    int po = trescSpraw(karty, karta, ws).i;
    PRZYPADEK("mapa nawigacji", r.ekran != Ekran::SPRAWY || po != przed);
  }
  // S5 i S10 mają akcję ↻ (ponów), więc według 5.6.1 pkt 6 i R-PRZ-04 (weryfikacja wymienia S5) muszą
  // podpisać każdą akcję, także ▶ = K1; stopka ma tylko `↻ ponów` / `↻ pobierz teraz`.
  {
    std::vector<Noc> h;
    DaneDnia d;
    d.dzis = "2026-11-12";
    d.hist = &h;
    TrescEkranu t5 = trescDnia("S5", "1", d), t10 = trescDnia("S10", "", d);
    PRZYPADEK("mapa nawigacji", ma(t5.stopkaLewo + t5.stopkaPrawo, "▶"));
    PRZYPADEK("mapa nawigacji", ma(t10.stopkaLewo + t10.stopkaPrawo, "▶"));
  }
  // spec nadal opisuje starą mapę w weryfikacjach i fail-path (czytelnik trafia na sprzeczność).
  {
    std::string sciezka = std::string(__FILE__);
    sciezka = sciezka.substr(0, sciezka.rfind("/firmware/")) + "/docs/spec/specyfikacja-funkcjonalna.md";
    std::ifstream f(sciezka);
    std::stringstream b;
    b << f.rdbuf();
    std::string s = b.str();
    TRZYMA(!s.empty());
    PRZYPADEK("mapa nawigacji", !ma(s, "02:10 ↻ na ekranie dnia → K1"));                // R-PRZ-18
    PRZYPADEK("mapa nawigacji", !ma(s, "22:50 ↻ → K1"));                                // R-PRZ-18
    PRZYPADEK("mapa nawigacji", !ma(s, "„Przeniesione” ze stopką `↻ cofnij`"));         // R-PRZ-19
    PRZYPADEK("mapa nawigacji", !ma(s, "drugie, osobne krótkie ↻ w 30 s cofa"));        // R-PRZ-15 fail-path
    PRZYPADEK("mapa nawigacji", !ma(s, "| naciśnięcie przycisku na S9 | K1 albo K2"));  // 8.x S9
    PRZYPADEK("mapa nawigacji", !ma(s, "▶ na S8 otwartym z K1"));                       // katalog ekranów
  }
}

int main() {
  testMapy();
  testZasad();
  testStopek();
  testPoprawkiMapy();
  printf("testy_nawigacja_mapa_brzegowe: %d trzyma, %d niezgodnych z mapą, %d błędów\n", ok, zle, znal);
  return zle || znal ? 1 : 0;
}
