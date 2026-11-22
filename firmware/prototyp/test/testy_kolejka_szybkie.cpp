// SPDX-License-Identifier: GPL-3.0-or-later
// Szybkie naciśnięcia (R-PRZ-20, T-TEST-516 w spec technicznej): naciśnięcia z czasu, gdy panel liczy stronę
// albo rysuje, zapamiętane i wykonane po kolei, także gdy pierwsze z serii panel odczytał ze stanu przycisku.
// Usterka z panelu 22.11.2026 16:53: trzy szybkie ▶ dały dwa ekrany, w logu `kolejka naciśnięć: 1 wykonane,
// 0 pominięte`; czuwanie() po obsłudze naciśnięcia ze stanu czyściło całą kolejkę (wyczyscKolejke()).
//
// Przyciski z modelem próbkowania panelu (AtrapaPrzyciskowProbkowanych: próbka co 5 ms, odsianie drgań 15 ms,
// naciśnięcie do kolejki przy puszczeniu). Czas na panelu: obsługa naciśnięcia (liczenie strony) OBSLUGA_MS, rysowanie
// RYSOWANIE_MS; atrapa przesuwa o nie zegar, więc naciśnięcie zaplanowane w tym czasie trafia do kolejki jak na
// panelu. Wzorzec: ten sam ciąg naciśnięć jako osobne wybudzenia (jak naciśnięcia z przerwą).
// Liczba obsłużonych naciśnięć z haczyka fazy `przycisk` (początek Dyrygent::obsluz()).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "nawigacja.h"
#include "przyciski_probki.h"
#include "start.h"
#include "szybkosc.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t ep(int h, int m, int s = 0) {
  struct tm t = {};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 10;
  t.tm_mday = 4;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}

// log z panelu 22.11.2026 (obraz pomiarowy): liczenie strony ok. 0,5–0,6 s; rysowanie z odświeżeniem ok. 1,5 s
const uint32_t OBSLUGA_MS = 550, RYSOWANIE_MS = 1500;

class EkranZCzasem : public AtrapaWyswietlacza {
 public:
  EkranZCzasem(Dziennik& d, AtrapaZegara& z) : AtrapaWyswietlacza(d), z_(z) {}
  std::function<void(int)> hak;  // numer odświeżenia w przebiegu (od 1), przed upływem RYSOWANIE_MS
  int wPrzebiegu = 0;
  void odswiez() override {
    AtrapaWyswietlacza::odswiez();
    wPrzebiegu++;
    if (hak) hak(wPrzebiegu);
    z_.msTeraz += RYSOWANIE_MS;
  }
 private:
  AtrapaZegara& z_;
};
// dioda gaśnie, gdy panel czeka na naciśnięcie (zwykleWybudzenie() tuż przed czuwaniem, czuwanie po każdej obsłudze)
class PrzyciskiZHakiem : public AtrapaPrzyciskowProbkowanych {
 public:
  explicit PrzyciskiZHakiem(AtrapaZegara& z) : AtrapaPrzyciskowProbkowanych(z) {}
  std::function<void(int)> poZgaszeniu;  // numer zgaszenia w przebiegu (od 1)
  int zgaszen = 0;
  void dioda(bool s) override {
    AtrapaPrzyciskowProbkowanych::dioda(s);
    if (!s && poZgaszeniu) poZgaszeniu(++zgaszen);
  }
};

struct Stanowisko {
  Panel p;
  PrzyciskiZHakiem prz{p.zegar};
  EkranZCzasem ekr{p.d, p.zegar};
  std::function<void(int)> poObsludze;  // numer obsługi w przebiegu (od 1), przed upływem OBSLUGA_MS
  int obslug = 0;
  int ekranObslugi = -1;  // ekran (pamięć RTC) w chwili ostatniej obsługi naciśnięcia
  Adaptery ad() {
    return Adaptery{p.zegar, p.nvs, p.pliki, p.siec, ekr, prz, p.czujniki, p.dzwiek, p.start, p.straznik, p.sen,
                    p.port, p.diag, &p.wydania};
  }
  Stanowisko() {
    p.zegar.epoch = ep(10, 0);
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.pliki.pliki["/konfiguracja.txt"] = "";
  }
  uint32_t teraz() const { return p.zegar.msTeraz; }
  void nacisnij(uint8_t bit, uint32_t od, uint32_t trzymaj) { prz.nacisnij(bit, od, od + trzymaj); }
  bool wLogu(const std::string& t) const {
    for (const std::string& w : p.port.log)
      if (w.find(t) != std::string::npos) return true;
    return false;
  }
  void zrzucLog() const {
    for (const std::string& w : p.port.log) printf("      %s\n", w.c_str());
  }
  int wierszy(const std::string& t) const {
    int n = 0;
    for (const std::string& w : p.port.log)
      if (w.find(t) != std::string::npos) n++;
    return n;
  }
};
static Stanowisko* biezace = nullptr;
static void haczyk(const char* nazwa) {
  if (!biezace || strcmp(nazwa, "przycisk") != 0) return;
  int nr = ++biezace->obslug;
  biezace->ekranObslugi = (int)ekranZRtc(biezace->p.start.stan.ekran);  // jak Przebieg::ekran()
  if (biezace->poObsludze) biezace->poObsludze(nr);
  biezace->p.zegar.msTeraz += OBSLUGA_MS;
}

// Wybudzenie przyciskiem `bit`: po głębokim śnie trzymany od startu `trzymaj` ms; po czekaniu na kablu
// (`puszczonyWCzekaniu` > 0) już puszczony, zdjęty z kolejki przez adapter, a `kolejkaCzekania` to naciśnięcia
// puszczone po nim, które adapter zostawia w kolejce na ten przebieg (ADR-0047, czekanie::przyciskKonczacy()).
static void wybudz(Stanowisko& s, uint32_t kiedy, uint8_t bit, uint32_t trzymaj, uint32_t puszczonyWCzekaniu = 0,
                   const std::vector<AtrapaPrzyciskowProbkowanych::Wpis>& kolejkaCzekania = {}) {
  Panel& p = s.p;
  p.zegar.epoch = kiedy;
  p.zegar.msTeraz = 0;
  p.d.z.clear();
  p.port.log.clear();
  p.sen.snow = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.start.trwanieBudzacego = puszczonyWCzekaniu;
  s.prz.odcinki.clear();
  if (!puszczonyWCzekaniu) s.nacisnij(bit, 0, trzymaj);
  s.prz.start();
  for (const auto& w : kolejkaCzekania) s.prz.kolejka.push_back(w);
  s.prz.zgaszen = 0;
  s.ekr.wPrzebiegu = 0;
  s.obslug = 0;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  biezace = &s;
  szybkosc::haczykFazy = haczyk;
  Dyrygent(s.ad(), KONF).wybudzenie();
  szybkosc::haczykFazy = nullptr;
  biezace = nullptr;
}
static Ekran ekran(const Stanowisko& s) { return (Ekran)s.p.start.stan.ekran; }

// wzorzec: te same przyciski jako osobne wybudzenia z przerwą 30 s
static Ekran osobno(const std::vector<uint8_t>& bity) {
  Stanowisko w;
  uint32_t t = ep(10, 0);
  for (uint8_t b : bity) wybudz(w, t += 30, b, 120);
  return ekran(w);
}

static void wypisz(const char* opis, const Stanowisko& s, Ekran ocz) {
  printf("  %s: obsłużonych %d, odświeżeń %d, ekran %d (oczekiwany %d)\n", opis, s.obslug, s.ekr.odswiezen,
         (int)ekran(s), (int)ocz);
  for (const std::string& w : s.p.port.log)
    if (w.find("kolejka naciśnięć") != std::string::npos || w.find("zacięty") != std::string::npos)
      printf("    log: %s\n", w.c_str());
}

// ---------- czuwanie: pierwsze z trzech ▶ odczytane ze stanu, drugie w czasie liczenia strony ----------
// wymaganie: R-PRZ-20
// `drugieTrzymane`: drugie wciśnięte w czasie liczenia strony i puszczone dopiero po nim (trzymane w chwili, gdy
// dawny kod czyścił kolejkę); inaczej puszczone przed końcem liczenia.
static void testCzuwanieTrzyPrawe(bool drugieTrzymane) {
  Ekran ocz = osobno({BIT_SRODEK, BIT_PRAWY, BIT_PRAWY, BIT_PRAWY});
  Stanowisko s;
  // ↻ budzi (ekran dnia, rysowanie 1); w czuwaniu ▶ ze stanu (obsługa 2), w czasie liczenia strony ▶, w czasie
  // rysowania po nim ▶
  s.prz.poZgaszeniu = [&](int nr) {
    if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 200, 120);
  };
  s.poObsludze = [&](int nr) {
    if (nr == 2) s.nacisnij(BIT_PRAWY, s.teraz() + (drugieTrzymane ? 400 : 100), drugieTrzymane ? 300 : 120);
  };
  s.ekr.hak = [&](int nr) {
    if (nr == 2) s.nacisnij(BIT_PRAWY, s.teraz() + 300, 120);
  };
  wybudz(s, ep(10, 5), BIT_SRODEK, 120);
  wypisz(drugieTrzymane ? "czuwanie, drugie trzymane" : "czuwanie, drugie puszczone", s, ocz);
  SPRAWDZ(s.obslug == 4);          // ↻ i trzy ▶, każde raz
  SPRAWDZ(ekran(s) == ocz);        // ten sam ekran co ↻ i trzy osobne ▶
  SPRAWDZ(s.wLogu("kolejka naciśnięć: 2 wykonane, 0 pominięte"));
  SPRAWDZ(s.ekr.odswiezen == 3);   // ↻, ▶ ze stanu, jedno na końcu serii z kolejki
  SPRAWDZ(s.prz.kolejka.empty());
}

// ---------- po głębokim śnie: ▶ budzi, drugie ▶ w czasie liczenia strony, trzecie wciśnięte, gdy panel kończy
// rysowanie i zaczyna czuwanie (odczytane ze stanu) ----------
// wymaganie: R-PRZ-20
static void testGlebokiSenTrzyPrawe() {
  Ekran ocz = osobno({BIT_PRAWY, BIT_PRAWY, BIT_PRAWY});
  Stanowisko s;
  s.poObsludze = [&](int nr) {
    if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 100, 120);
  };
  s.prz.poZgaszeniu = [&](int nr) {
    if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz(), 150);
  };
  wybudz(s, ep(10, 5), BIT_PRAWY, 120);
  wypisz("po głębokim śnie", s, ocz);
  SPRAWDZ(s.obslug == 3);
  SPRAWDZ(ekran(s) == ocz);
  // drugie i trzecie jedną serią z kolejki (trzecie puszczone w czasie liczenia strony drugiego), jedno rysowanie
  SPRAWDZ(s.wLogu("kolejka naciśnięć: 2 wykonane, 0 pominięte"));
  SPRAWDZ(s.ekr.odswiezen == 2);
  SPRAWDZ(s.prz.kolejka.empty());
}

// ---------- po czekaniu na kablu: ▶ kończy czekanie (już puszczone), drugie ▶ puszczone w czekaniu po nim (zostaje
// w kolejce), trzecie wciśnięte, gdy panel zaczyna czuwanie ----------
// wymaganie: R-PRZ-20
static void testKabelTrzyPrawe() {
  Ekran ocz = osobno({BIT_PRAWY, BIT_PRAWY, BIT_PRAWY});
  Stanowisko s;
  s.ekr.pamietaObraz = true;
  s.prz.poZgaszeniu = [&](int nr) {
    if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz(), 150);
  };
  wybudz(s, ep(10, 5), BIT_PRAWY, 0, 120, {{(int)Przycisk::PRAWY, 110u, 0u}});
  wypisz("po czekaniu na kablu", s, ocz);
  SPRAWDZ(s.obslug == 3);
  SPRAWDZ(ekran(s) == ocz);
  SPRAWDZ(s.wLogu("kolejka naciśnięć: 2 wykonane, 0 pominięte"));
  SPRAWDZ(s.ekr.odswiezen == 2);
  SPRAWDZ(s.prz.kolejka.empty());
}

// ---------- ▶ ▶ ↻: kolejność naciśnięć zachowana (starsze z kolejki przed trzymanym teraz) ----------
// wymaganie: R-PRZ-20
static void testKolejnosc() {
  // ▶ budzi (K1), ▶ w czasie liczenia (K2), ↻ wciśnięte, gdy panel zaczyna czuwanie: ekran dnia; w odwrotnej
  // kolejności (↻ przed ▶) byłoby K1
  Ekran ocz = osobno({BIT_PRAWY, BIT_PRAWY, BIT_SRODEK});
  SPRAWDZ(ocz == Ekran::DZIEN && osobno({BIT_PRAWY, BIT_SRODEK, BIT_PRAWY}) == Ekran::K1);
  Stanowisko s;
  s.poObsludze = [&](int nr) {
    if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 100, 120);
  };
  s.prz.poZgaszeniu = [&](int nr) {
    if (nr == 1) s.nacisnij(BIT_SRODEK, s.teraz(), 150);
  };
  wybudz(s, ep(10, 5), BIT_PRAWY, 120);
  wypisz("▶ ▶ ↻", s, ocz);
  SPRAWDZ(s.obslug == 3);
  SPRAWDZ(ekran(s) == ocz);
}

// ---------- bez zmian: zacięty przycisk, przytrzymanie 2 s, skrót ◀ + ▶ ----------
// wymaganie: R-PRZ-09, R-PRZ-08, R-PRZ-21
static void testBezFalszywychNacisniec() {
  // ▶ trzymane 12 s w czuwaniu (zacięte, ZACIECIE_MS): jedna obsługa, po puszczeniu nic z kolejki
  {
    Stanowisko s;
    s.prz.poZgaszeniu = [&](int nr) {
      if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 200, ZACIECIE_MS + 2000);
    };
    wybudz(s, ep(10, 5), BIT_SRODEK, 120);
    wypisz("zacięty ▶", s, Ekran::DZIEN);
    SPRAWDZ(s.obslug == 2);
    SPRAWDZ(!s.wLogu("kolejka naciśnięć"));
    SPRAWDZ(s.prz.kolejka.empty());
  }
  // ▶ 2,5 s w czuwaniu na K1 (przytrzymanie, R-PRZ-08: na K1 nic): jedna obsługa jako przytrzymanie, nic z kolejki
  {
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)Ekran::K2;
    s.prz.poZgaszeniu = [&](int nr) {
      if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 200, 2500);
    };
    wybudz(s, ep(10, 5), BIT_LEWY, 120);  // K2 ◀ → K1
    wypisz("▶ 2,5 s na K1", s, Ekran::K1);
    SPRAWDZ(s.obslug == 2);
    SPRAWDZ(ekran(s) == Ekran::K1);
    SPRAWDZ(!s.wLogu("kolejka naciśnięć"));
  }
  // skrót ◀ + ▶ puszczony przed 2 s (R-PRZ-21: bez żadnej akcji), w czuwaniu i przy wybudzeniu; ▶ dołożone w czasie
  // trzymania ◀ nie wraca z kolejki
  {
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)Ekran::K2;
    s.prz.poZgaszeniu = [&](int nr) {
      if (nr != 1) return;
      s.nacisnij(BIT_LEWY, s.teraz() + 200, 600);
      s.nacisnij(BIT_PRAWY, s.teraz() + 300, 400);
    };
    wybudz(s, ep(10, 5), BIT_LEWY, 120);  // K2 ◀ → K1
    wypisz("skrót w czuwaniu, puszczony przed 2 s", s, Ekran::K1);
    SPRAWDZ(s.obslug == 1);
    SPRAWDZ(ekran(s) == Ekran::K1);
    SPRAWDZ(s.wLogu("skrót ◀ + ▶ puszczony przed 2 s"));
    SPRAWDZ(!s.wLogu("kolejka naciśnięć"));
  }
  {
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)Ekran::K1;
    s.prz.odcinki.clear();
    // wybudza ◀ (trzymane od startu), ▶ dołożone po 100 ms, oba puszczone przed 2 s
    s.prz.poZgaszeniu = nullptr;
    Panel& p = s.p;
    p.zegar.epoch = ep(10, 5);
    p.zegar.msTeraz = 0;
    p.port.log.clear();
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
    p.start.budzace = BIT_LEWY;
    p.start.trwanieBudzacego = 0;
    s.nacisnij(BIT_LEWY, 0, 600);
    s.nacisnij(BIT_PRAWY, 100, 400);
    s.prz.start();
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    biezace = &s;
    szybkosc::haczykFazy = haczyk;
    Dyrygent(s.ad(), KONF).wybudzenie();
    szybkosc::haczykFazy = nullptr;
    biezace = nullptr;
    wypisz("skrót przy wybudzeniu, puszczony przed 2 s", s, Ekran::K1);
    SPRAWDZ(s.obslug == 0);
    SPRAWDZ(ekran(s) == Ekran::K1);
    SPRAWDZ(!s.wLogu("kolejka naciśnięć"));
  }
}


// ---------- z kolejki tylko czysta nawigacja ----------
// wymaganie: R-PRZ-20
// Tabela wszystkich ekranów × przycisków × kontekstów ekranu dnia: czystaNawigacja(e, nacisniecie(e, p, krótkie, k))
// wobec niezależnie zapisanej reguły: na ekranie poza ciągiem stron zawsze odrzucone; na ekranie dnia odrzucone pytanie
// tak/nie (▶ „tak”, ◀ „nie”, ↻ pomiń) i ↻ na S5 (pobranie albo pomiar baterii); strony kokpitu i strona Portretu: tak.
static const char* const NAZWY[] = {"DZIEN", "K1", "K2", "PYTANIE", "TEST", "OMOWIONE", "SPRAWY", "PRZENIESIONE",
                                    "ZAPISANO", "RAPORT", "NORMA", "LICZBY", "SPRZET", "SPRZET_ZAPISANO",
                                    "USTAWIENIA", "FORMULARZ", "SIEC_PANELU", "AKTUALIZACJA", "PORTRET", "SYPIALNIA"};
static void testTabelaCzystejNawigacji() {
  std::vector<std::pair<const char*, KontekstNaw>> kk;
  KontekstNaw b;
  b.sprawyDostepne = true;
  b.spraw = 3;
  b.ostatniaNoc = true;
  kk.push_back({"zwykły", b});
  KontekstNaw x = b; x.pytanieTN = true; kk.push_back({"pytanie tak/nie", x});
  x = b; x.s5 = true; kk.push_back({"S5", x});
  x = b; x.s5 = x.s5Bateria = true; kk.push_back({"S5 baterii", x});
  x = b; x.wzorzec = true; kk.push_back({"ekran L", x});
  x = b; x.wzorzec = x.problem = true; kk.push_back({"problem", x});
  x = b; x.nocny = true; kk.push_back({"nocny", x});
  x = b; x.sk31 = true; x.zaproszenie = true; kk.push_back({"S-K-31", x});
  x = b; x.sprawa = 2; kk.push_back({"sprawa 2 z 3", x});
  x = b; x.aktualizacja = 1; kk.push_back({"U-07 nowsza", x});
  x = b; x.aktualizacja = 2; kk.push_back({"U-07 ponów", x});
  x = b; x.profilPubliczny = true; x.sypialniaDostepna = true; kk.push_back({"publiczny, sypialnia", x});
  const Przycisk P[3] = {Przycisk::LEWY, Przycisk::SRODEK, Przycisk::PRAWY};
  const char* const ZN[3] = {"◀", "↻", "▶"};
  int niezgodnych = 0, par = 0;
  printf("  tabela (ekran, przycisk) → z kolejki (dla kontekstu zwykłego; w nawiasie konteksty z odrzuceniem):\n");
  for (int ei = 0; ei <= (int)Ekran::SYPIALNIA; ei++) {
    Ekran e = (Ekran)ei;
    bool wCiagu = e == Ekran::DZIEN || stronaKokpitu(e) || e == Ekran::PORTRET;
    std::string wiersz;
    for (int pi = 0; pi < 3; pi++) {
      std::string odrz;
      bool zwykly = false;
      for (auto& kv : kk) {
        Przejscie pr = nacisniecie(e, P[pi], false, kv.second);
        bool c = czystaNawigacja(e, pr);
        bool ocz = wCiagu;
        if (e == Ekran::DZIEN && kv.second.pytanieTN && !kv.second.nocny) ocz = false;
        if (e == Ekran::DZIEN && P[pi] == Przycisk::SRODEK && kv.second.s5 && !kv.second.nocny && !kv.second.pytanieTN &&
            !kv.second.wzorzec)
          ocz = false;
        par++;
        if (c != ocz) {
          niezgodnych++;
          printf("    NIEZGODNE: %s %s [%s]: czysta %d, oczekiwane %d (akcja %d, ekran %d)\n", NAZWY[ei], ZN[pi],
                 kv.first, c, ocz, (int)pr.akcja, (int)pr.ekran);
        }
        if (&kv == &kk.front()) zwykly = c;
        else if (!c && zwykly) odrz += std::string(odrz.empty() ? "" : ", ") + kv.first;
      }
      wiersz += std::string("  ") + ZN[pi] + " " + (zwykly ? "nawigacja" : "odrzucone") +
                (odrz.empty() ? "" : " (" + odrz + ")");
    }
    printf("    %-16s%s\n", NAZWY[ei], wiersz.c_str());
  }
  printf("  tabela: %d par, %d niezgodnych z regułą\n", par, niezgodnych);
  SPRAWDZ(niezgodnych == 0);
}

// Przez dyrygenta: wybudzenie zegarem na ekranie `e` (z pamięci RTC, naciśnięcie minutę temu), w kolejce jedno
// naciśnięcie zostawione przez adapter (jak po rysowaniu); czuwanie po zegarze wykonuje kolejkę raz.
static int zKolejkiNaEkranie(Ekran e, int przycisk, Stanowisko& s) {
  Panel& p = s.p;
  uint32_t kiedy = ep(12, 5);
  p.zegar.epoch = kiedy;
  p.zegar.msTeraz = 0;
  p.d.z.clear();
  p.port.log.clear();
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  p.start.budzace = 0;
  p.start.trwanieBudzacego = 0;
  p.start.stan.ekran = (uint8_t)e;
  p.start.stan.ekranOd = kiedy - 1;  // bez powrotu po czasie (R-PRZ-02) w tym wybudzeniu
  s.prz.odcinki.clear();
  s.prz.start();
  s.prz.kolejka.push_back({przycisk, 120u, 0u});
  s.obslug = 0;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  biezace = &s;
  szybkosc::haczykFazy = haczyk;
  Dyrygent(s.ad(), KONF).wybudzenie();
  szybkosc::haczykFazy = nullptr;
  biezace = nullptr;
  return s.obslug;
}
static void testDyrygentTylkoNawigacja() {
  const int P[3] = {(int)Przycisk::LEWY, (int)Przycisk::SRODEK, (int)Przycisk::PRAWY};
  int sprawdzonych = 0, wlasnych = 0, zle0 = zle;
  for (int ei = 0; ei <= (int)Ekran::SYPIALNIA; ei++) {
    for (int pi = 0; pi < 3; pi++) {
      Ekran e = (Ekran)ei;
      Stanowisko s;
      int n = zKolejkiNaEkranie(e, P[pi], s);
      // ekran, na którym kolejka była wykonana, z wiersza logu: `… na ekranie {e} to nie sama nawigacja` albo obsługa
      bool odrzucone = s.wLogu("to nie sama nawigacja");
      bool wykonane = s.wLogu("kolejka naciśnięć: 1 wykonane");
      // ekran, na którym kolejka była wykonana: z wiersza odrzucenia albo z chwili obsługi
      int na = -1;
      for (const std::string& w : s.p.port.log) {
        size_t i = w.find(" na ekranie ");
        if (w.find("to nie sama nawigacja") != std::string::npos && i != std::string::npos) na = atoi(w.c_str() + i + 12);
      }
      if (wykonane) na = s.ekranObslugi;
      if (na < 0) continue;
      if (na == ei) wlasnych++;
      e = (Ekran)na;
      sprawdzonych++;
      bool wCiagu = e == Ekran::DZIEN || stronaKokpitu(e) || e == Ekran::PORTRET;
      if (!wCiagu) SPRAWDZ(odrzucone && n == 0);  // odrzucone: obsluz() nie wołane, więc bez zapisu i sieci
      if (odrzucone) SPRAWDZ(n == 0);
      if (wykonane) SPRAWDZ(n == 1 && wCiagu);
      if (zle != zle0) {
        printf("    ekran %s, przycisk %d: obsłużonych %d, odrzucone %d, wykonane %d\n", NAZWY[ei], P[pi], n,
               odrzucone, wykonane);
        if (getenv("KOL_LOG")) s.zrzucLog();
        zle0 = zle;
      }
    }
  }
  printf("  dyrygent: %d par (ekran, przycisk) z kolejki po wybudzeniu zegarem, %d na ekranie z pamięci RTC\n",
         sprawdzonych, wlasnych);
  SPRAWDZ(sprawdzonych == 60 && wlasnych >= 40);
  // ↻ na ekranie dnia bez nocy (S5: ponowienie pobrania) z kolejki: odrzucone, bez żądania do AirBridge
  Stanowisko s;
  zKolejkiNaEkranie(Ekran::K1, (int)Przycisk::SRODEK, s);  // K1 ↻ → ekran dnia (S5 w atrapie bez nocy)
  SPRAWDZ(ekran(s) == Ekran::DZIEN && s.obslug == 1);
  zKolejkiNaEkranie(Ekran::DZIEN, (int)Przycisk::SRODEK, s);
  printf("  ↻ z kolejki na S5: obsłużonych %d, %s\n", s.obslug,
         s.wLogu("to nie sama nawigacja") ? "odrzucone" : "WYKONANE");
  SPRAWDZ(s.obslug == 0 && s.wLogu("to nie sama nawigacja"));
}

// Serie losowe tylko z ◀ i ▶ (wszystkie przejścia to czysta nawigacja): ten sam ekran co każde osobno, każde raz.
// Serie brzegowe z ↻ (test/testy_kolejka_szybkie_brzegowe.cpp) porównują z „każde osobno” także ↻ na S5, którego
// z kolejki od 22.11.2026 wykonywać nie wolno (ponowienie pobrania), więc tam wzorzec różni się od reguły.
static void testSerieLewyPrawy() {
  uint32_t los = 777;
  auto losowa = [&](uint32_t n) {
    los = los * 1103515245u + 12345u;
    return (los >> 8) % n;
  };
  int niezgodnych = 0;
  for (int i = 0; i < 80; i++) {
    int n = 2 + (int)losowa(6);
    std::vector<uint8_t> bity;
    for (int j = 0; j < n; j++) bity.push_back(losowa(3) ? BIT_PRAWY : BIT_LEWY);
    Ekran ocz = osobno(bity);
    Stanowisko s;
    uint32_t h0 = 40 + losowa(300), t = h0;
    std::vector<std::pair<uint8_t, std::pair<uint32_t, uint32_t>>> dalej;
    for (int j = 1; j < n; j++) {
      uint32_t przerwa = 30 + losowa(700), trz = 40 + losowa(300);
      dalej.push_back({bity[j], {t + przerwa, trz}});
      t += przerwa + trz;
    }
    s.prz.poZgaszeniu = nullptr;
    Panel& p = s.p;
    p.zegar.epoch = ep(10, 5);
    p.zegar.msTeraz = 0;
    p.port.log.clear();
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
    p.start.budzace = bity[0];
    p.start.trwanieBudzacego = 0;
    s.prz.odcinki.clear();
    s.nacisnij(bity[0], 0, h0);
    for (auto& d : dalej) s.nacisnij(d.first, d.second.first, d.second.second);
    s.prz.start();
    s.obslug = 0;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    biezace = &s;
    szybkosc::haczykFazy = haczyk;
    Dyrygent(s.ad(), KONF).wybudzenie();
    szybkosc::haczykFazy = nullptr;
    biezace = nullptr;
    if (s.obslug != n || ekran(s) != ocz || !s.prz.kolejka.empty()) niezgodnych++;
  }
  printf("  serie ◀ ▶: 80 prób, %d niezgodnych\n", niezgodnych);
  SPRAWDZ(niezgodnych == 0);
}


// ---------- przycisk wciśnięty przed końcem rysowania i trzymany po nim ----------
// wymaganie: R-PRZ-20, R-PRZ-08
static void testWcisnietyWRysowaniu() {
  // ▶ z ekranu dnia budzi (K1); w czasie rysowania K1 kolejne ▶, trzymane jeszcze 300 ms po końcu rysowania: czysta
  // nawigacja, wykonane raz (K2)
  {
    Stanowisko s;
    s.ekr.hak = [&](int nr) {
      if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 1200, 600);
    };
    wybudz(s, ep(12, 5), BIT_PRAWY, 120);
    wypisz("▶ z rysowania, trzymane po nim", s, osobno({BIT_PRAWY, BIT_PRAWY}));
    SPRAWDZ(s.obslug == 2 && ekran(s) == osobno({BIT_PRAWY, BIT_PRAWY}));
    SPRAWDZ(!s.wLogu("z czasu rysowania"));
  }
  // K1 ▶ → K2; ▶ wciśnięte 500 ms przed końcem rysowania K2, trzymane 2,1 s: przytrzymanie na ekranie niewidzianym
  // w chwili wciśnięcia → odrzucone (bez raportu), K2 zostaje
  {
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)Ekran::K1;
    s.ekr.hak = [&](int nr) {
      if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 1000, 2100);
    };
    wybudz(s, ep(12, 5), BIT_PRAWY, 120);
    wypisz("▶ 2,1 s zaczęte w rysowaniu K2", s, Ekran::K2);
    SPRAWDZ(ekran(s) == Ekran::K2 && s.obslug == 1);
    SPRAWDZ(s.wLogu("z czasu rysowania na ekranie 2 to nie sama nawigacja: pominięte"));
  }
  // to samo przytrzymanie zaczęte 200 ms po końcu rysowania K2: raport ogólny (▶ 2 s na K2, R-LEK-02); przytrzymanie
  // liczone od wciśnięcia
  {
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)Ekran::K1;
    s.ekr.hak = [&](int nr) {
      if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + RYSOWANIE_MS + 200, 2100);
    };
    wybudz(s, ep(12, 5), BIT_PRAWY, 120);
    wypisz("▶ 2,1 s zaczęte po rysowaniu K2", s, Ekran::RAPORT);
    SPRAWDZ(ekran(s) == Ekran::RAPORT && s.obslug == 2);
  }
  // przytrzymanie od chwili wciśnięcia: ▶ zaczęte w czuwaniu, odczytane przez rdzeń dopiero po 1,9 s (np. obieg
  // zajęty), trzymane łącznie 2,1 s → przytrzymanie (dawniej liczone od odczytu: krótkie)
  {
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)Ekran::K1;
    uint32_t od = 0;
    s.prz.poZgaszeniu = [&](int nr) {
      if (nr != 1) return;
      od = s.teraz() + 100;
      s.nacisnij(BIT_PRAWY, od, 2100);
      s.p.zegar.msTeraz += 2000;  // rdzeń zajęty 2 s (atrapa), przycisk już trzymany 1,9 s w chwili odczytu
    };
    wybudz(s, ep(12, 5), BIT_PRAWY, 120);
    wypisz("▶ odczytane po 1,9 s trzymania", s, Ekran::RAPORT);
    SPRAWDZ(ekran(s) == Ekran::RAPORT);
  }
}


// ---------- rdzeń próbkowania (src/przyciski_probki.h): ten sam kod co na panelu ----------
// wymaganie: R-PRZ-20, R-PRZ-09
// Odcinki stanu surowego z drganiami styku; próbka co 5 ms od chwili t0.
static przyciski_probki::ZmianyProbki probkuj(przyciski_probki::Model& m, int64_t& t, int64_t doUs,
                                              const std::vector<std::pair<int64_t, int64_t>>& wcisniety, uint8_t bit,
                                              int& piski) {
  przyciski_probki::ZmianyProbki suma;
  for (; t <= doUs; t += przyciski_probki::OKRES_PROBKI_US) {
    uint8_t sur = 0;
    for (auto& o : wcisniety)
      if (t >= o.first && t < o.second) sur = bit;
    przyciski_probki::ZmianyProbki z = m.probka(sur, t);
    for (int i = 1; i <= 3; i++)
      if (z.piski & (1u << i)) piski++;
    suma.piski = (uint8_t)(suma.piski | z.piski);
    suma.puszczone = (uint8_t)(suma.puszczone | z.puszczone);
  }
  return suma;
}
static void testRdzenProbkowania() {
  using przyciski_probki::Model;
  const int64_t MS = 1000;
  // drgania przy wciśnięciu i puszczeniu (przerwy 1–10 ms): jedno naciśnięcie w kolejce i jeden pisk
  {
    Model m;
    m.start(0);
    int64_t t = 1000 * MS;
    int piski = 0;
    std::vector<std::pair<int64_t, int64_t>> o = {{t + 0, t + 3 * MS},       {t + 4 * MS, t + 6 * MS},
                                                  {t + 16 * MS, t + 200 * MS}, {t + 201 * MS, t + 204 * MS},
                                                  {t + 214 * MS, t + 216 * MS}};
    probkuj(m, t, t + 400 * MS, o, BIT_PRAWY, piski);
    int nr = 0;
    uint32_t tr = 0;
    SPRAWDZ(m.ile() == 1 && piski == 1);
    SPRAWDZ(m.zKolejki(nr, tr) && nr == (int)Przycisk::PRAWY && tr >= 150 && tr <= 230);
  }
  // przycisk wciśnięty przy starcie (budzący): bez pisku i bez kolejki po puszczeniu
  {
    Model m;
    m.start(BIT_LEWY);
    int64_t t = 1000 * MS;
    int piski = 0;
    probkuj(m, t, t + 300 * MS, {{0, t + 100 * MS}}, BIT_LEWY, piski);
    SPRAWDZ(m.ile() == 0 && piski == 0);
  }
  // R-PRZ-20 „najwyżej 8”: 9 naciśnięć bez odczytu, w kolejce 8 najstarszych, 9 pisków
  {
    Model m;
    m.start(0);
    int64_t t = 1000 * MS, t0 = t;
    int piski = 0;
    std::vector<std::pair<int64_t, int64_t>> o;
    for (int j = 0; j < 9; j++) o.push_back({t0 + j * 100 * MS, t0 + j * 100 * MS + 50 * MS});
    probkuj(m, t, t0 + 1000 * MS, o, BIT_PRAWY, piski);
    SPRAWDZ(m.ile() == przyciski_probki::MIEJSC_KOLEJKI && m.ile() == 8 && piski == 9);
  }
  // obsłużone ze stanu w czasie trzymania (także zaciętego): puszczenie nie trafia do kolejki, pisk był raz
  {
    Model m;
    m.start(0);
    int64_t t = 1000 * MS, t0 = t;
    int piski = 0;
    std::vector<std::pair<int64_t, int64_t>> o = {{t0, t0 + 12000 * MS}};
    probkuj(m, t, t0 + 100 * MS, o, BIT_PRAWY, piski);
    SPRAWDZ(m.wcisniete() == BIT_PRAWY);
    m.obsluzone((int)Przycisk::PRAWY, t);
    probkuj(m, t, t0 + 12500 * MS, o, BIT_PRAWY, piski);
    SPRAWDZ(m.ile() == 0 && piski == 1);
  }
  // wyścig: puszczenie (po odsianiu) między odczytem stanu przez rdzeń a obsluzone(): wpis zdjęty z końca kolejki;
  // starszy wpis innego przycisku i wpis sprzed ponad 50 ms zostają
  {
    Model m;
    m.start(0);
    int64_t t = 1000 * MS, t0 = t;
    int piski = 0;
    probkuj(m, t, t0 + 100 * MS, {{t0, t0 + 60 * MS}}, BIT_LEWY, piski);  // ◀ w kolejce
    std::vector<std::pair<int64_t, int64_t>> o = {{t0 + 150 * MS, t0 + 300 * MS}};
    probkuj(m, t, t0 + 295 * MS, o, BIT_PRAWY, piski);
    SPRAWDZ(m.wcisniete() == BIT_PRAWY);  // rdzeń odczytuje ▶ ze stanu …
    probkuj(m, t, t0 + 330 * MS, o, BIT_PRAWY, piski);  // … zadanie esp_timer zdąża z puszczeniem
    SPRAWDZ(m.ile() == 2);
    m.obsluzone((int)Przycisk::PRAWY, t);
    int nr = 0;
    uint32_t tr = 0;
    SPRAWDZ(m.ile() == 1 && m.zKolejki(nr, tr) && nr == (int)Przycisk::LEWY);
    // puszczone ponad 50 ms przed obsluzone(): inne naciśnięcie, zostaje
    std::vector<std::pair<int64_t, int64_t>> o2 = {{t0 + 400 * MS, t0 + 450 * MS}};
    probkuj(m, t, t0 + 600 * MS, o2, BIT_PRAWY, piski);
    m.obsluzone((int)Przycisk::PRAWY, t);
    SPRAWDZ(m.ile() == 1);
  }
  // wyczysc(): kolejka pusta, trzymany bez kolejki po puszczeniu; oproznijKolejke() zostawia trzymany bez `pomin`
  {
    Model m;
    m.start(0);
    int64_t t = 1000 * MS, t0 = t;
    int piski = 0;
    probkuj(m, t, t0 + 100 * MS, {{t0, t0 + 50 * MS}, {t0 + 70 * MS, t0 + 400 * MS}}, BIT_SRODEK, piski);
    SPRAWDZ(m.ile() == 1);
    m.wyczysc();
    probkuj(m, t, t0 + 500 * MS, {{t0 + 70 * MS, t0 + 400 * MS}}, BIT_SRODEK, piski);
    SPRAWDZ(m.ile() == 0);
    probkuj(m, t, t0 + 700 * MS, {{t0 + 550 * MS, t0 + 650 * MS}}, BIT_SRODEK, piski);
    m.oproznijKolejke();
    SPRAWDZ(m.ile() == 0);
  }
  // pisk dokładnie raz za każde fizyczne naciśnięcie także przez dyrygenta: ↻ budzi (bez pisku adaptera), potem
  // trzy szybkie ▶ (ze stanu, z czasu liczenia, z czasu rysowania): 3 piski adaptera
  {
    Stanowisko s;
    s.prz.poZgaszeniu = [&](int nr) {
      if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 200, 120);
    };
    s.poObsludze = [&](int nr) {
      if (nr == 2) s.nacisnij(BIT_PRAWY, s.teraz() + 100, 120);
    };
    s.ekr.hak = [&](int nr) {
      if (nr == 2) s.nacisnij(BIT_PRAWY, s.teraz() + 300, 120);
    };
    wybudz(s, ep(10, 5), BIT_SRODEK, 120);
    printf("  piski adaptera: %d za 3 naciśnięcia po budzącym\n", s.prz.piskow);
    SPRAWDZ(s.prz.piskow == 3 && s.obslug == 4);
  }
}


// ---------- przycisk trzymany od czasu przed przebiegiem (czekanie na kablu, które zakończył inny) ----------
// wymaganie: R-PRZ-20, R-PRZ-08
static void testTrzymanyPrzedPrzebiegiem() {
  uint32_t odMs = 7;
  SPRAWDZ(!przyciski_probki::chwilaWPrzebiegu(0, 5000, odMs));
  SPRAWDZ(przyciski_probki::chwilaWPrzebiegu(4000000, 5000, odMs) && odMs == 0);     // przed początkiem
  SPRAWDZ(przyciski_probki::chwilaWPrzebiegu(5250000, 5000, odMs) && odMs == 250);   // po początku
  SPRAWDZ(przyciski_probki::chwilaWPrzebiegu(1000000, 0xFFFFFF00u, odMs) && odMs == 1256);  // przez zawinięcie millis()
  // ↻ kończy czekanie (już puszczone), ▶ trzymane od przed przebiegu przez całe rysowanie ekranu dnia i jeszcze
  // ok. 1 s: wciśnięte przed rysowaniem, więc nie jako ▶ na ekranie dnia (dawniej K1)
  Stanowisko s;
  s.ekr.pamietaObraz = true;
  Panel& p = s.p;
  p.zegar.epoch = ep(12, 5);
  p.zegar.msTeraz = 0;
  p.port.log.clear();
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  // koniec czekania zegarem: K1 sprzed 10 min wraca na ekran dnia (R-PRZ-02) i rysuje bez czekania na puszczenie
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  p.start.stan.ekran = (uint8_t)Ekran::K1;
  p.start.stan.ekranOd = ep(11, 55);
  p.start.budzace = 0;
  p.start.trwanieBudzacego = 0;
  s.prz.odcinki.clear();
  s.nacisnij(BIT_PRAWY, 0, 3000);
  s.prz.start(true);
  s.obslug = 0;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  biezace = &s;
  szybkosc::haczykFazy = haczyk;
  Dyrygent(s.ad(), KONF).wybudzenie();
  szybkosc::haczykFazy = nullptr;
  biezace = nullptr;
  wypisz("▶ trzymane od przed przebiegiem", s, Ekran::DZIEN);
  if (getenv("KOL_LOG")) s.zrzucLog();
  SPRAWDZ(ekran(s) == Ekran::DZIEN && s.obslug == 0);
  SPRAWDZ(s.wLogu("z czasu rysowania"));
}

int main() {
  testCzuwanieTrzyPrawe(false);
  testCzuwanieTrzyPrawe(true);
  testGlebokiSenTrzyPrawe();
  testKabelTrzyPrawe();
  testKolejnosc();
  testBezFalszywychNacisniec();
  testTabelaCzystejNawigacji();
  testDyrygentTylkoNawigacja();
  testSerieLewyPrawy();
  testWcisnietyWRysowaniu();
  testRdzenProbkowania();
  testTrzymanyPrzedPrzebiegiem();
  printf("testy_kolejka_szybkie: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
