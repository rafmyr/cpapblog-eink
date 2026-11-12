// SPDX-License-Identifier: GPL-3.0-or-later
// Portret nocy (spec 5.4.16, ADR-0040): przebieg ostatniej nocy w odcinkach 5 min i plik `/dane/portret.csv`
// (T-DANE-38, wersja 1), bez zależności od Arduino, bez wyjątków i RTTI. Wzorzec: tools/portret/przebieg.py (ta sama
// kolejność działań na liczbach, test zgodności T-TEST-414 w test/testy_portret.cpp).
//
// Zbieranie (T-WYM-98): LicznikNocy (src/noc_pliki.h) z dołączonym Zbieraczem przekazuje mu każdy przeczytany plik
// `PLD` w tym samym przejściu, w którym liczy noc, w kolejności czasu (LicznikNocy::kolejnosc()); zbieracz trzyma
// liczniki 288 odcinków doby i jeden bufor próbek bieżącego odcinka na kanał (wyciek, PS), który przechodzi na
// następny plik, gdy dwie sesje dzielą odcinek; bufor ma najwyżej BUFOR_PROBEK próbek (150 próbek 2 s odcinka, zapas
// na pliki zachodzące w czasie). Pamięć robocza nie zależy od liczby sesji. Zdarzenia i sesje bierze z wyniku
// LicznikNocy.
#pragma once
#include <cstdint>
#include <bitset>
#include <string>
#include <vector>
#include "edf.h"

struct Str;
namespace noc_pliki {
struct WynikNocy;
class LicznikNocy;
}

namespace portret {

// Progi i stałe (docs/spec/progi-poza-zakresem.csv; te same w tools/portret/przebieg.py)
const int ODCINEK_S = 300;       // odcinek 5 min [PRÓG]
const int ODCINKI_DOBY = 288;
const int MIN_PROBEK = 30;       // odcinek liczy się do pików, wycieku i maksimum od 30 poprawnych próbek [PRÓG]
const double PROG_WYCIEKU = 15.0;  // L/min (R-NOR-05.niepewna_p95, ADR-0034)
const double MARGINES_MAKS = 0.5;  // cmH2O: ciśnienie na maksimum od maksimum z ustawień − 0,5
const int MAKS_ZDARZEN = 500;    // wierszy `Z` w pliku [PRÓG]
const int BRAK = -32768;         // pole bez pomiaru (puste w pliku)
const int BUFOR_PROBEK = 300;    // bufor odcinka na kanał: 150 próbek 2 s, podwójnie przy plikach zachodzących
// Zakresy pól pliku T-DANE-38 (jedno miejsce dla zapisu w Zbieracz::wynik() i odczytu w parsuj(), R-POR-21)
const int SESJA_OD_MAKS = 1440;  // sesja może zacząć się w ostatniej minucie doby (minuta 1440 po zaokrągleniu)
const int SESJA_DO_MAKS = 2880;
const int MINUTA_ZDARZENIA_MAKS = 1439;
const int WYC_S_MAKS = ODCINEK_S;  // pliki zachodzące w czasie liczą próbki podwójnie: sekundy przycięte do odcinka

struct Sesja {
  int od = 0, do_ = 0;  // minuty od 12:00 dnia `day` (granice zapisu z PLD, do najbliższej minuty)
};
struct Odcinek {
  int od = 0;          // minuta początku (wielokrotność 5)
  int minuty = 0;      // minuty z poprawną próbką ciśnienia (0–5)
  int wyciek95 = BRAK; // P95 `Leak` w L/min, w dół do całości
  int wycS = BRAK;     // sekundy z `Leak` ≥ 15 L/min
  int zew = BRAK, centr = BRAK;  // zdarzenia na zewnątrz (OA, Apnea, H) i centralne (CA)
  int ps = BRAK;       // mediana PS w 0,1 cmH2O
  int maks = BRAK;     // minuty z ciśnieniem (AutoSet) albo EPAP (ASVAuto) na maksimum
};
struct Zdarzenie {
  int minuta = 0;  // minuty od 12:00 dnia `day`
  bool zew = false, centr = false;
};
struct Przebieg {
  std::string data;  // data poranka RRRR-MM-DD; "" = brak przebiegu
  std::vector<Sesja> sesje;
  std::vector<Odcinek> odcinki;  // rosnąco po `od`
  std::vector<Zdarzenie> zdarzenia;
  // litery L (brak `Leak`), P (brak par PS), E (zdarzenia nieznane), R (bezdechy bez rodzaju), M (AutoSet albo ASVAuto
  // bez maksimum z ustawień albo bez kanału: ciśnienie na maksimum nieznane)
  std::string braki;
  bool brak(char c) const { return braki.find(c) != std::string::npos; }
  const Odcinek* odcinek(int k) const;  // odcinek o indeksie k (od = 5k) albo nullptr
};

// Kanał ciśnienia na maksimum: AutoSet `Press.2s` ≥ MPA − 0,5, ASVAuto `EprPress.2s` ≥ EAX − 0,5 (spec 5.4.16).
struct Maksimum {
  const char* kanal = nullptr;  // nullptr = nie dotyczy (CPAP, ASV) albo brak ustawień: pole `maks` puste
  double prog = 0;
  bool potrzebne = false;       // tryb AutoSet albo ASVAuto: bez kanału albo progu brak pomiaru `M`
};
// Ustawienia doby `dzien` ze STR.edf → próg maksimum; `tryb` dostaje tryb doby (tag MOP), "" bez doby.
Maksimum maksimumZeStr(const Str& st, long dzien, std::string* tryb = nullptr);
Maksimum maksimumTrybu(const std::string& tryb, float mpa, float eax);

class Zbieracz {
 public:
  explicit Zbieracz(long dzien, Maksimum m = Maksimum());
  // Jeden przeczytany plik PLD (wołane przez LicznikNocy::dodajPlik() z tym samym edf::Plik), w kolejności czasu.
  void plikPld(const edf::Plik& p);
  // Wynik: sesje z granic zapisu, zdarzenia z `zdarzenia` (chwila w s cywilnych, tekst EVE) w czasie zapisu,
  // braki; zamyka bufory odcinków (wołane raz, po ostatnim pliku). Pola w zakresach z góry pliku.
  Przebieg wynik(const noc_pliki::WynikNocy& w, const std::vector<std::pair<double, std::string>>& zdarzenia);
  size_t bajty() const;  // pamięć robocza (pomiar T-WYM-98)

 private:
  struct Akum {
    uint16_t n = 0;
    uint8_t minuty = 0, maksMin = 0;  // maski bitowe minut 0–4 odcinka
    float wycS = 0;                   // suma kroków próbek (całkowite sekundy, dokładne w float)
    int16_t wyciek95 = BRAK, ps = BRAK;
  };
  struct Bufor {
    int k = -1;
    std::vector<double> v;
  };
  long dzien_;
  long long t0_;
  Maksimum maks_;
  std::vector<Akum> a_;
  Bufor leak_, ps_;               // bieżący odcinek na kanał, przechodzi między plikami
  std::bitset<288> uzyte_;        // odcinki z jakąkolwiek próbką
  bool maLeak_ = false, maPs_ = false, maMaks_ = false;
  void dodaj(Bufor& b, int k, double v, bool leak);
  void zamknij(Bufor& b, bool leak);
};

// Plik T-DANE-38: wiersze danych (bez nagłówka magazynu, nazw pól i `#koniec`).
std::vector<std::string> wiersze(const Przebieg& p);
// Wiersze → przebieg; false (i `blad` z opisem) przy złej wartości (R-POR-21): nieznany rodzaj, zła liczba pól, minuta
// spoza doby, liczba ujemna albo poza zakresem, różne daty, nieznana litera braku, brak wiersza `B`.
bool parsuj(const std::vector<std::string>& w, Przebieg& p, std::string& blad);

}  // namespace portret
