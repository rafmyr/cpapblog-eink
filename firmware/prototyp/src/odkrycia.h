// SPDX-License-Identifier: GPL-3.0-or-later
// Odkrycia O-03…O-12 (spec funkcjonalna 5.4.10, 6.5, 5.11.6; spec techniczna 9.3).
// Bez zależności od Arduino. Wiersze katalogu: docs/spec/katalog/O.csv, rodzaje spoza tabeli 9.3: rodzaje-o.csv.
//
// Cechy nocy (rozszerzenie kontekstu obok src/reguly.h, wzór src/reguly_sygnaly.h: kontrakt się nie zmienia): dyrygent
// buduje CechyNocy (wartości cech nocy z sypialni, odpowiedzi „Co było inaczej?”, pary już pokazane) i ustawia je na
// czas oceny obiektem Zakres; rodzaje odkryć czytają je przez cechyOceny(). Bez cech rodzaje dają „nie oceniam”.
//
// Rytm (6.5, R-KON-08, 9.3 „Parametry testu permutacyjnego”):
//  - O-03, O-04 (bez porównania dwóch grup) oceniane każdego ranka (tryb DZIENNY);
//  - porównania grup (O-06…O-10, O-12) tylko w przeglądzie tygodnia (tryb PRZEGLAD, niedziela): test permutacyjny,
//    2000 przetasowań, ziarno z numeru dnia przeglądu, Holm–Bonferroni na wszystkich porównaniach przeglądu (ocen()
//    silnika reguł, zmienna `holm`); para czynnik–metryka (dla O-06…O-10: para grup i metryka) daje odkrycie najwyżej raz,
//    po pokazaniu wypada z kolejnych przeglądów (CechyNocy::pokazane);
//  - kolejka z przeglądu czeka do pokazania; przed pokazaniem wiersz liczony ponownie bez testu (tryb REWALIDACJA,
//    R-EKR-30: odpada, gdy grupy albo różnica przestaną spełniać warunek).
// Wiersze odkryć nie wchodzą do dziennej oceny E/L/H (bezOdkryc()): nie są sprawami pacjenta i ich „nie oceniam” nie
// zmienia warunku uspokojenia (5.4.13 pkt 7 liczy reguły zdrowia).
#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "czynniki.h"
#include "reguly.h"
#include "reguly_pomoc.h"

namespace odkrycia {

enum class TrybOceny : unsigned char { DZIENNY, PRZEGLAD, REWALIDACJA };

struct CechyNocy {
  TrybOceny tryb = TrybOceny::DZIENNY;
  // cecha → data poranka → wartość (NAN albo brak = nieznana): `co2` (mediana nocy, ppm), `wilgotnosc` (%), `start`
  // (M-18, minuty od 18:00; dziś bez źródła w rekordzie nocy), `gi` (moduł GI). `weekend` liczy się z daty.
  std::map<std::string, std::map<std::string, double>> wartosci;
  const std::vector<WpisCzynnikow>* czynniki = nullptr;  // O-12 (R-CZY-04, R-CZY-24)
  std::set<std::string> pokazane;                        // klucze wyników (`O-12/A.M-02`) po pokazaniu (9.3)
};
class Zakres {
 public:
  explicit Zakres(const CechyNocy* c);
  ~Zakres();
  Zakres(const Zakres&) = delete;
  Zakres& operator=(const Zakres&) = delete;

 private:
  const CechyNocy* poprz_;
};
const CechyNocy* cechyOceny();
// Wartość cechy nocy z poranka `data`; NAN = nieznana. `weekend` = 1 (sobota, niedziela) albo 0.
double cecha(const std::string& nazwa, const std::string& data);

// Rodzaje spoza rdzenia (docs/spec/katalog/rodzaje-o.csv): rekord_tygodnia (O-03), tydzien_miesiac (O-04),
// grupy_nocy (O-06…O-10), czynnik_nocy (O-12), rozklad_w_nocy (O-05, bez danych), zmiana_cechy_gi (O-11).
// Wywołać przed oceną (jak zarejestrujRodzajeE()), bez statycznych inicjatorów.
void zarejestrujRodzajeO();

bool wierszOdkrycia(const std::string& idReguly);  // `O-xx`
// Zestaw bez wierszy odkryć (dzienna ocena E/L/H) i same odkrycia (przegląd, ranek); kopie trzymane dla `z`.
const reguly::Zestaw& bezOdkryc(const reguly::Zestaw& z);
const reguly::Zestaw& tylkoOdkrycia(const reguly::Zestaw& z);
std::string klucz(const reguly::Wynik& w);  // `O-12/A.M-02`, `O-03/M-02`

// Ocena odkryć w trybie `c.tryb` (DZIENNY: O-03, O-04; PRZEGLAD i REWALIDACJA: porównania grup).
std::vector<reguly::Wynik> ocenOdkrycia(const reguly::Zestaw& z, const reguly::Kontekst& k, const CechyNocy& c,
                                         const reguly::OpcjeOceny& o = reguly::OpcjeOceny());
// Przegląd tygodnia (R-KON-08: niedziela): dziś niedziela i ostatni przegląd nie dziś, albo ostatni przegląd starszy
// niż 7 dni (panel uśpiony w niedzielę, przegląd przy pierwszym wybudzeniu potem).
bool przegladDzis(const std::string& dzis, const std::string& ostatni);

// ---------- pokazania, kolejka i limity (5.4.10, R-EKR-25…R-EKR-31) ----------
struct Pokazanie {
  std::string klucz, data;  // klucz wyniku, data poranka pokazania
  double wartosc = 0;       // wartość odkrycia (R-EKR-27)
};
std::string zapiszPokazania(const std::vector<Pokazanie>& p);  // `klucz@data@wartość|…` (NVS `odk_pok`)
std::vector<Pokazanie> parsujPokazania(const std::string& s);
// Kolejka z przeglądu: klucze spełnionych porównań z datą przeglądu (`data;klucz|klucz…`, NVS `odk_kol`).
struct Kolejka {
  std::string przeglad;  // data poranka przeglądu
  std::vector<std::string> klucze;
};
std::string zapiszKolejke(const Kolejka& k);
Kolejka parsujKolejke(const std::string& s);
// Klucze par wypadłych z przeglądów (9.3): pokazane porównania grup (O-06…O-12).
std::set<std::string> paryPokazane(const std::vector<Pokazanie>& p);
bool porownanieGrup(const std::string& klucz);  // O-06…O-10, O-12

struct WejscieWyboru {
  std::string dzis;
  const std::vector<reguly::Wynik>* dzienne = nullptr;  // ocenOdkrycia(DZIENNY)
  const std::vector<reguly::Wynik>* kolejka = nullptr;  // ocenOdkrycia(REWALIDACJA) dla kluczy kolejki
  const Kolejka* kol = nullptr;
  const std::vector<Pokazanie>* pokazane = nullptr;
  // daty poranków, w których pokazano odkrycie albo S-K-21, S-K-32 (wspólny limit R-EKR-25, z dziennika ekranów)
  std::vector<std::string> pokazaniaWspolne;
  bool rozruch = false;   // rozruch globalny (R-EKR-28)
  const reguly::Zestaw* zestaw = nullptr;  // Δ metryk (R-EKR-27)
  int limitDni = 7, powrotDni = 30;        // R-EKR-25.odkrycia, R-EKR-27.powrot [PRÓG]
};
// Odkrycie na dziś (R8) albo nullptr z powodem do logu. Kolejność R-PIE-03: niższy numer O-xx; O-12 od poniedziałku po
// przeglądzie (D4). Pokazane już dziś (R-EKR-31: widoczne jeden dzień) wraca to samo.
const reguly::Wynik* wybierz(const WejscieWyboru& w, std::string& powod);
double wartoscWyniku(const reguly::Wynik& w);  // zmienna `wartosc_num`

// ---------- teksty ekranu S1 (6.5, R-HUM-13, R-CZY-20, R-CZY-21, 5.4.14) ----------
struct UstawieniaTekstow {
  bool czynnikiWrazliwe = false; // „Czynniki wrażliwe na ekranie” (5.6.4, R-CZY-20), domyślnie `nie`
  bool uspokojenie = true;       // warunek 5.4.13 (krok O-03, R-EKR-32)
  bool liniaZdrowia = false;     // na ekranie stoi linia grupy „zdrowie” (krok zamienny R-EKR-32)
  bool tylkoRegulyBezOceny = false, tylkoOcenyWylaczone = false;  // jak sk::Wejscie
  bool tylkoBrakDanych = false;  // jak sk::Wejscie
};
struct TrescOdkrycia {
  std::string id, klucz, etykieta, tytul, powod, krok;
  std::string podpisOsi;          // `Może to przypadek.` przy powodzie ponad 70 znaków albo czynniku ukrytym (R-CZY-21)
  std::string tytulK1, powodK1;   // pełny tytuł i powód O-12 przy czynniku ukrytym: w K1 (R-CZY-20)
  bool ukryty = false;            // O-12 przy alkoholu, THC, lekach przy ustawieniu `nie`
  bool doLekarza = false;         // krok do lekarza: zawsze ekran danych (R-HUM-25)
  // wizualizacja (kolumna „Wizualizacja” 6.5): 2 słupki albo linia 30 nocy (O-03)
  bool linia = false;
  std::vector<double> wartosci30;  // O-03: mediana… wartości nocy (NAN = brak), najstarsza pierwsza
  double a = 0, b = 0;             // 2 słupki: grupa z cechą i grupa porównawcza
  std::string etA, etB, wartA, wartB;
  std::string kodCzynnika;         // O-12: kod z src/czynniki.h (klucz humoru `O-12/{kod}`)
};
TrescOdkrycia trescOdkrycia(const reguly::Zestaw& z, const reguly::Wynik& w, reguly::Tryb tryb,
                            const UstawieniaTekstow& u, const std::vector<Noc>* hist = nullptr);
// Klucz humoru S1 (5.7.3): `O-xx` albo `O-12/{kod czynnika}`.
std::string kluczHumoru(const reguly::Wynik& w);

}  // namespace odkrycia
