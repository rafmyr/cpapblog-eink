// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe R-NF-25 (odświeżenie częściowe e-ink, 16.11.2026): szukają dróg, którymi ekran dnia albo odświeżenie z inicjatywy panelu
// dostaje częściowe, sprawdzają, że zmiana nie dodaje odświeżeń ani nie zmienia treści klatek (porównanie z N = 0,
// czyli zachowaniem sprzed zmiany), licznik w RTC (wartość spoza zakresu, N = 1 i N = 20, zmiana N między
// wybudzeniami), kolejkę naciśnięć kończącą się na ekranie dnia i parser klucza `eink.pelne_co`.
// Uruchomienie: tools/prototyp/testy.sh (program testy_odswiezanie_czesciowe_brzegowe).
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "nawigacja.h"
#include "odswiezanie.h"
#include "slot_dzienny.h"
#include "start.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

static const Odswiezenie P = Odswiezenie::PELNE, C = Odswiezenie::CZESCIOWE;
static std::string tekst(const std::vector<Odswiezenie>& v) {
  std::string s;
  for (Odswiezenie r : v) s += r == C ? 'C' : 'P';
  return s;
}

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
struct Nacisk {
  uint8_t bit;
  uint32_t trzymaj;
};
static const Nacisk PR{BIT_PRAWY, 300}, LE{BIT_LEWY, 300}, SR{BIT_SRODEK, 300};

static void wybudz(Panel& p, uint32_t kiedy, PowodWybudzenia w, const std::vector<Nacisk>& n) {
  p.zegar.epoch = kiedy;
  p.zegar.msTeraz = 0;
  p.d.z.clear();
  p.sen.snow = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = w;
  p.start.budzace = w == PowodWybudzenia::PRZYCISK ? n.front().bit : 0;
  p.przyciski.skrypt.clear();
  uint32_t t = 0;
  for (const Nacisk& x : n) {
    p.przyciski.skrypt.push_back({t, x.bit});
    p.przyciski.skrypt.push_back({t + x.trzymaj, 0});
    t += x.trzymaj + 3000;
  }
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static void przygotuj(Panel& p, const std::string& konf) {
  p.zegar.epoch = ep(4, 10, 0);
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.pliki.pliki["/konfiguracja.txt"] = konf;
  p.start.stan.ekran = (uint8_t)Ekran::DZIEN;
}
static Ekran ekran(const Panel& p) { return (Ekran)p.start.stan.ekran; }

// Wyświetlacz testów brzegowych: przy każdym odświeżeniu zapisuje także ekran z pamięci RTC (r.ekran w chwili odświeżenia),
// żeby sprawdzić regułę „ekran dnia zawsze pełne” na każdym odświeżeniu, nie tylko na końcu scenariusza.
struct WyswietlaczBrzegowy : AtrapaWyswietlacza {
  Panel* panel;
  std::vector<Ekran> ekrany;
  WyswietlaczBrzegowy(Panel& p) : AtrapaWyswietlacza(p.d), panel(&p) {}
  void odswiez() override {
    ekrany.push_back((Ekran)panel->start.stan.ekran);
    AtrapaWyswietlacza::odswiez();
  }
};
struct Krok {
  uint32_t kiedy;
  PowodWybudzenia w;
  std::vector<Nacisk> n;
  std::vector<std::pair<int, uint32_t>> kolejka;
};
struct Wynik {
  std::string rodzaje;
  std::vector<Ekran> ekrany;
  std::vector<std::vector<uint8_t>> klatki;
  Ekran koniec = Ekran::DZIEN;
  int licznik = 0;
};
static Wynik scenariusz(const std::string& konf, const std::vector<Krok>& kroki, int licznikNaStart = 0) {
  Panel p;
  przygotuj(p, konf);
  p.start.stan.czesciowychZRzedu = (uint8_t)licznikNaStart;
  WyswietlaczBrzegowy wys(p);
  for (const Krok& k : kroki) {
    p.zegar.epoch = k.kiedy;
    p.zegar.msTeraz = 0;
    p.d.z.clear();
    p.sen.snow = 0;
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = k.w;
    p.start.budzace = k.w == PowodWybudzenia::PRZYCISK && !k.n.empty() ? k.n.front().bit : 0;
    p.przyciski.skrypt.clear();
    p.przyciski.kolejka = k.kolejka;
    uint32_t t = 0;
    for (const Nacisk& x : k.n) {
      p.przyciski.skrypt.push_back({t, x.bit});
      p.przyciski.skrypt.push_back({t + x.trzymaj, 0});
      t += x.trzymaj + 3000;
    }
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Adaptery a{p.zegar, p.nvs, p.pliki, p.siec, wys, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik,
               p.sen, p.port, p.diag, &p.wydania};
    Dyrygent(a, KONF).wybudzenie();
  }
  Wynik w;
  w.rodzaje = tekst(wys.rodzaje);
  w.ekrany = wys.ekrany;
  w.klatki = wys.klatki;
  w.koniec = ekran(p);
  w.licznik = p.start.stan.czesciowychZRzedu;
  return w;
}

// Każde odświeżenie, przy którym w pamięci RTC stoi ekran dnia (DZIEN, w tym S9), ma być pełne.
static bool dzienZawszePelny(const Wynik& w) {
  for (size_t i = 0; i < w.ekrany.size() && i < w.rodzaje.size(); i++)
    if (w.ekrany[i] == Ekran::DZIEN && w.rodzaje[i] != 'P') {
      printf("  ekran dnia z częściowym przy odświeżeniu %zu (%s)\n", i, w.rodzaje.c_str());
      return false;
    }
  return true;
}

// (a) i (g): w wielu scenariuszach zmiana nie dodaje ani nie usuwa odświeżeń i nie zmienia treści klatek: wynik dla
// N = 5 (domyślne) ma tyle samo odświeżeń, te same bufory i ten sam ekran końcowy co N = 0 (zachowanie sprzed zmiany);
// ekran dnia zawsze pełny.
// wymaganie: R-NF-25
static void testBrzeg_bezNowychOdswiezen() {
  const uint32_t D = ep(4, 10, 0);  // 11:00 czasu panelu (po porannym pobraniu R-PRZ-22, które w atrapie gubi naciśnięcia ze skryptu)
  const int PRAWY = (int)Przycisk::PRAWY, LEWY = (int)Przycisk::LEWY;
  std::vector<std::pair<const char*, std::vector<Krok>>> sc = {
      {"czuwanie ▶ ▶ ▶ ↻", {{D, PowodWybudzenia::PRZYCISK, {PR, PR, PR, SR}, {}}}},
      {"czuwanie ▶ ◀ ▶ ↻ ▶", {{D, PowodWybudzenia::PRZYCISK, {PR, LE, PR, SR, PR}, {}}}},
      {"kolejka ▶ + [▶ ◀]", {{D, PowodWybudzenia::PRZYCISK, {PR}, {{PRAWY, 150u}, {LEWY, 150u}}}}},
      {"kolejka ▶ + [◀] (koniec na ekranie dnia)", {{D, PowodWybudzenia::PRZYCISK, {PR}, {{LEWY, 150u}}}}},
      {"kolejka ▶ ▶ + [◀ ◀] (koniec na ekranie dnia)",
       {{D, PowodWybudzenia::PRZYCISK, {PR, PR}, {{LEWY, 150u}, {LEWY, 150u}}}}},
      {"noc 02:00 ▶ ▶ ↻ ▶", {{ep(4, 1, 0), PowodWybudzenia::PRZYCISK, {PR, PR, SR, PR}, {}}}},
      {"S9 o 23:30, potem ▶ ↻ o 23:40",
       {{ep(4, 22, 30), PowodWybudzenia::ZEGAR, {}, {}}, {ep(4, 22, 40), PowodWybudzenia::PRZYCISK, {PR, SR}, {}}}},
      {"▶ ▶, zegar po 3 min, zegar po 10 min (R-PRZ-02)",
       {{D, PowodWybudzenia::PRZYCISK, {PR, PR}, {}},
        {ep(4, 10, 3), PowodWybudzenia::ZEGAR, {}, {}},
        {ep(4, 10, 10), PowodWybudzenia::ZEGAR, {}, {}}}},
      {"nowa doba przy otwartym K2",
       {{ep(4, 22, 58), PowodWybudzenia::PRZYCISK, {PR, PR}, {}}, {ep(5, 4, 0), PowodWybudzenia::ZEGAR, {}, {}}}},
  };
  for (auto& s : sc) {
    Wynik w0 = scenariusz("eink.pelne_co=0\n", s.second);
    Wynik w5 = scenariusz("", s.second);
    printf("  %s: N=0 %s, N=5 %s (ekran końcowy %d)\n", s.first, w0.rodzaje.c_str(), w5.rodzaje.c_str(),
           (int)w5.koniec);
    SPRAWDZ(w0.rodzaje.size() == w5.rodzaje.size());
    SPRAWDZ(w0.klatki == w5.klatki);
    SPRAWDZ(w0.koniec == w5.koniec);
    SPRAWDZ(w0.rodzaje.find('C') == std::string::npos);
    SPRAWDZ(dzienZawszePelny(w5));
    SPRAWDZ(!w5.rodzaje.empty() && w5.rodzaje[0] == 'P');  // pierwsze w wybudzeniu pełne
  }
}

// (c) kolejka kończąca się na ekranie dnia: jedno odświeżenie, pełne
// wymaganie: R-NF-25, R-PRZ-20
static void testBrzeg_kolejkaNaDzien() {
  Wynik w = scenariusz("", {{ep(4, 10, 0), PowodWybudzenia::PRZYCISK, {PR}, {{(int)Przycisk::LEWY, 150u}}}});
  printf("  ▶ budzący + [◀] z kolejki: %s, ekran %d\n", w.rodzaje.c_str(), (int)w.koniec);
  SPRAWDZ(w.koniec == Ekran::DZIEN);
  SPRAWDZ(w.rodzaje == "PP");
}

// (b) licznik w RTC: wartość spoza zakresu (zepsuta pamięć RTC, 250), N = 1, N = 20, zmiana N między wybudzeniami
// wymaganie: R-NF-25
static void testBrzeg_licznik() {
  const uint32_t D = ep(4, 10, 0);
  // bajt spoza zakresu: pierwsze i tak pełne, licznik od zera, potem częściowe
  Wynik w = scenariusz("", {{D, PowodWybudzenia::PRZYCISK, {PR, PR, LE}, {}}}, 250);
  printf("  licznik 250 w RTC, ▶ ▶ ◀: %s, licznik %d\n", w.rodzaje.c_str(), w.licznik);
  SPRAWDZ(w.rodzaje == "PCC" && w.licznik == 2);
  // N = 1: na przemian
  w = scenariusz("eink.pelne_co=1\n", {{D, PowodWybudzenia::PRZYCISK, {PR, PR, LE, PR, LE, PR}, {}}});
  SPRAWDZ(w.rodzaje == "PCPCPC");
  // N = 20: 20 częściowych, potem pełne, potem częściowe
  std::vector<Nacisk> n = {PR, PR};
  for (int i = 0; i < 21; i++) n.push_back(i % 2 ? PR : LE);
  w = scenariusz("eink.pelne_co=20\n", {{D, PowodWybudzenia::PRZYCISK, n, {}}});
  printf("  N = 20, 23 zmiany stron: %s\n", w.rodzaje.c_str());
  SPRAWDZ(w.rodzaje == "P" + std::string(20, 'C') + "PC");
  // zmiana N między wybudzeniami (5 → 1): pierwsze pełne zeruje, dalej według nowego N
  Panel p;
  przygotuj(p, "");
  wybudz(p, D, PowodWybudzenia::PRZYCISK, {PR, PR, LE});
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PCC" && p.start.stan.czesciowychZRzedu == 2);
  p.pliki.pliki["/konfiguracja.txt"] = "eink.pelne_co=1\n";
  p.ekran.rodzaje.clear();
  wybudz(p, ep(4, 10, 1), PowodWybudzenia::PRZYCISK, {PR, LE, PR});
  printf("  N 5 → 1 między wybudzeniami: %s\n", tekst(p.ekran.rodzaje).c_str());
  SPRAWDZ(tekst(p.ekran.rodzaje) == "PCP");
}

// (h) parser klucza `eink.pelne_co`
// wymaganie: R-NF-25
static void testBrzeg_parser() {
  int n = -1;
  SPRAWDZ(odswiezanie::pelneCoZTekstu("05", n) && n == 5);  // zero wiodące: przyjęte
  for (const char* zly : {"+5", "5.0", "1e1", "０", "5 5", "\t5", "-0", "999999999999"})
    SPRAWDZ(!odswiezanie::pelneCoZTekstu(zly, n));
  SPRAWDZ(parsujUstawieniaDnia("eink.pelne_co=7   \r\n").pelneCo == 7);   // spacje i \r na końcu
  SPRAWDZ(parsujUstawieniaDnia("eink.pelne_co =\t7\n").pelneCo == 7);     // spacja przed `=`, tab po
  SPRAWDZ(parsujUstawieniaDnia("EINK.PELNE_CO=7\n").pelneCo == -1);       // wielkość liter: inny klucz
  // duplikat: ostatnia dobra wartość wygrywa, zła nie kasuje wcześniejszej dobrej
  SPRAWDZ(parsujUstawieniaDnia("eink.pelne_co=3\neink.pelne_co=7\n").pelneCo == 7);
  UstawieniaDnia u = parsujUstawieniaDnia("eink.pelne_co=3\neink.pelne_co=x\n");
  SPRAWDZ(u.pelneCo == 3 && u.bledy.size() == 1);
  // wiersz z komentarzem na końcu: odrzucony w całości (wartość domyślna) i w logu
  u = parsujUstawieniaDnia("eink.pelne_co=3 # próba\n");
  SPRAWDZ(u.pelneCo == -1 && u.bledy.size() == 1);
}

// (a) potwierdzenie (OMOWIONE, PRZENIESIONE, ZAPISANO, SPRAWY) bez danych
// rysuje w pokaz() ekran dnia (`if (!pokazB5(dzis)) rysujDzien();`, src/dyrygent.cpp), ale o rodzaju decyduje ekran()
// z RTC, więc reguła daje częściowe. Ten test sprawdza regułę na wejściu, jakie pokaz() buduje w tej chwili: R-NF-25
// wymaga pełnego dla ekranu dnia. Przed poprawką z 16.11.2026 PADAŁ: rodzajOdswiezenia()
// nie wiedziała, że zamiast potwierdzenia rysowany jest ekran dnia; od poprawki wejście `naPlotnieDzien`. W praktyce
// zwykle ratuje to „klatka bez zmian” (ekran dnia ten sam, który stał przed otwarciem potwierdzenia), więc
// odświeżenia nie ma wcale.
// wymaganie: R-NF-25
static void testBrzeg_potwierdzenieRysujeDzien() {
  for (Ekran e : {Ekran::OMOWIONE, Ekran::PRZENIESIONE, Ekran::ZAPISANO, Ekran::SPRAWY}) {
    odswiezanie::Wejscie w;
    w.ekran = e;  // to, co pokaz() przekazuje: ekran(), a nie to, co narysowano
    w.poPrzycisku = true;
    w.obrazZnany = true;
    w.pelneCo = 5;
    const bool rysujeDzien = true;  // pokazB5() == false: na płótnie ekran dnia
    // poprawka z 16.11.2026: pokaz() przekazuje teraz, że rysujDzien() narysował ekran dnia (znacznik
    // `dzienNaPlotnie` ustawiany w Przebieg::rysujDzien()); test sprawdza regułę na tym nowym wejściu, sens bez zmian
    w.naPlotnieDzien = rysujeDzien;
    SPRAWDZ(!(rysujeDzien && odswiezanie::rodzajOdswiezenia(w) == C));
  }
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testBrzeg_bezNowychOdswiezen();
  testBrzeg_kolejkaNaDzien();
  testBrzeg_licznik();
  testBrzeg_parser();
  testBrzeg_potwierdzenieRysujeDzien();
  printf("testy_odswiezanie_czesciowe_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
