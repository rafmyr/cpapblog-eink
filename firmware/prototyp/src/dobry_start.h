// SPDX-License-Identifier: GPL-3.0-or-later
// Program „Dobry start” (ADR-0049, spec funkcjonalna 5.8.14, wymagania R-DS-01…R-DS-53): porady na pierwsze tygodnie
// terapii. Rdzeń bez sprzętu i bez Arduino (ADR-0021): katalog porad z zestawu danych, stan programu z pliku
// `/dane/dobry_start.csv`, wybór porady dnia, dziennik pokazań i ankieta. Rysowanie: src/ekrany_kontekst.cpp
// (rysujPorade()), formularz: src/formularz.cpp, wywołania: src/dyrygent.cpp (rysujPoradeB49()).
//
// Kod nie zna ID porad ani ich liczby: wszystko, co różni porady (miejsce w planie, tryb, nawilżacz, ekran, warunek,
// kolejność, limity, stopnie, porada objaśniająca, ostatnia porada planu), stoi w kolumnach pliku porad
// (docs/spec/katalog/porady.csv, kolumny `warunek`, `kol`, `kol_ankiety`, `flagi`, `wymaga_porady`, `stopien_wyzej`).
// Warunek porady wyzwalanej to lista tokenów `|`: porada jest kandydatem, gdy aktywny jest któryś token bez `!`
// i żaden z `!`. Tokeny z danych składa dyrygent z wyników ISTNIEJĄCYCH reguł oceny poranka (tokenyRegul()); tokeny
// `DS-Q{nn}={k}` (odpowiedź k pytania z ankiety) bierze silnik z ostatniej ankiety tygodniowej.
#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace dobry_start {

// ---------- progi programu (spec 5.8.14; wszystkie bez źródła klinicznego: docs/spec/progi-poza-zakresem.csv) ----------
const int BLOK_MIN = 180;          // R-DS-03 [PRÓG]: blok porady od startu slotu dziennego, minuty
const int D0_DOBY = 3;             // R-DS-04 [PRÓG]: od tylu kolejnych dób z D0 porada planu stoi wieczorem
const int ZALEGLA_DNI = 2;         // R-DS-06 [PRÓG]: porada zaległa czeka najwyżej tyle dni
const int LIMIT_POCZATEK = 2;      // R-DS-07 [PRÓG]: porad wyzwalanych w oknie 7 dni do doby LIMIT_GRANICA
const int LIMIT_GRANICA = 28;      // R-DS-07 [PRÓG]: doba od dnia włączenia, do której działa LIMIT_POCZATEK
const int LIMIT_POZNIEJ = 1;       // R-DS-07 [PRÓG]: porad wyzwalanych w oknie 7 dni od doby LIMIT_GRANICA + 1
const int OKNO_DNI = 7;            // R-DS-07, R-DS-08 [PRÓG]: okno kroczące; ta sama porada najwyżej raz na tyle dni
const int RAZY_W_PROGRAMIE = 2;    // R-DS-08 [PRÓG]: ta sama porada wyzwalana najwyżej tyle razy w programie
const int KONIEC_DOBA = 56;        // R-DS-12 [PRÓG]: najpóźniej w tej dobie (plus dni przerwy) ostatnia porada planu
const int ANKIETA_NOCE = 3;        // R-DS-26 [PRÓG]: pierwszy tydzień ankiety po tylu nocach z terapią
const int ANKIETA_CO_DNI = 7;      // R-DS-26 [PRÓG]: kolejne tygodnie ankiety co tyle dni
const int DWA_RAZY = 2;            // R-DS-28 [PRÓG]: odpowiedź w tylu kolejnych wypełnionych ankietach (flaga dwa_razy)

// ---------- katalog porad (zestaw danych, ADR-0023, ADR-0024) ----------
enum class Rodzaj : uint8_t { PLAN, ZDARZENIE, ANKIETA, PONAD };
enum class Tryb : uint8_t { NIEZNANY, CPAP, AUTOSET, ASV };  // tryb ostatniej nocy; ASVAuto = ASV (ADR-0030)
const uint8_t TR_CPAP = 1, TR_AUTOSET = 2, TR_ASV = 4, TR_WSZYSTKIE = 8;  // kolumna `tryb` jako maska
enum Flaga : uint16_t {
  FL_PRZERWA = 1, FL_POZA_LIMITEM = 2, FL_BEZ_LIMITU = 4, FL_WYCIEK = 8, FL_BEZ_WYCIEKU_7 = 16,
  FL_PIERWSZA = 32, FL_DWA_RAZY = 64, FL_PODSTAWOWA = 128, FL_KONCOWA = 256, FL_OBJAW = 512,
};
// Fragment tekstu w pliku porad (tytuł, treść i krok nie są kopiowane do pamięci: czyta je tekst()).
struct Wycinek {
  uint32_t od = 0;
  uint16_t dl = 0;
};
struct Porada {
  std::string id, baza;  // baza = ID bez wariantu (`DS-D23.cpap` → `DS-D23`)
  Rodzaj rodzaj = Rodzaj::PLAN;
  int dzien = 0;         // miejsce w kolejce planu (plan); 0 = poza planem
  uint8_t tryby = 0;     // maska TR_*
  int8_t nawilzacz = 0;  // kolumna `wymaga`: 1 `nawilżacz`, -1 `bez nawilżacza`, 0 bez wymagania
  bool karta = false;    // `ekran=karta`
  std::string przy, zamiast, wymagaPorady, stopienWyzej;
  int miejsceBez = 0;    // kolumna `miejsce_bez`: miejsce planu przy `start.nawilzacz=nie` (R-DS-29); 0 = brak
  std::vector<std::string> warunek, weta;  // tokeny warunku (OR) i tokeny `!` (żaden nie może być aktywny)
  int kol = 0, kolAnkiety = 0;
  uint16_t flagi = 0;
  Wycinek tytul, tresc, krok;
  bool ma(Flaga f) const { return (flagi & f) != 0; }
};
struct Pytanie {
  std::string id, typ;      // typ `jedna` | `wiele`
  int odTygodnia = 1;       // 0 = pytanie startowe (bez tygodnia), 1 = od pierwszego tygodnia, 2 = od drugiego
  int wylaczna = 0;         // numer odpowiedzi, która odznacza pozostałe; 0 = brak
  std::string klucz;        // klucz `/konfiguracja.txt`, w który idzie odpowiedź (pytanie startowe); "" = brak
  std::vector<std::string> wartosci;  // wartości klucza dla kolejnych odpowiedzi
  std::string pytanie;
  std::vector<std::string> odpowiedzi;
};
struct Katalog {
  std::string wlasny;           // treść pliku z pamięci panelu (gdy nie z firmware)
  const char* zrodlo = nullptr; // treść pliku porad (wlasny albo stała z zestawu wbudowanego)
  size_t dlugosc = 0;
  std::string skrot;            // skrót z nagłówka pliku porad (`S;zestaw;…`)
  std::vector<Porada> porady;
  std::vector<Pytanie> pytania;
  std::vector<std::string> log;  // wiersze pominięte (R-DS-37): `{ID}: {powód}`
  int miejsc = 0;                // ostatnie miejsce planu (42 w zestawie z 21.11.2026)
  int ponadIle = 0;              // liczba porad „ponad program” (różnych baz)
  bool jest() const { return !porady.empty(); }
  int tygodni() const { return (miejsc + 6) / 7; }
  std::string tekst(const Wycinek& w) const;
  const Porada* porada(const std::string& id) const;
  const Pytanie* pytanieId(const std::string& id) const;
  Katalog() {}
  Katalog(const Katalog& k);
  Katalog& operator=(const Katalog& k);
};
// Plik porad i plik ankiety w formacie zestawu (nagłówek `#cpap-eink porady 1 … skrot=sha256:…`, wiersz nazw pól,
// wiersze). false i powód przy złym nagłówku, skrócie albo nazwach pól (R-DS-36); uszkodzony wiersz pominięty
// z wpisem w `log` (R-DS-37). Treść pliku porad musi żyć tyle co katalog, chyba że `kopiuj`.
bool wczytajKatalog(const char* porady, const char* ankieta, Katalog& k, std::string& powod, bool kopiuj = false);
const Katalog& katalogWbudowany();  // zestaw wbudowany (src/zestaw_wbudowany.h); pusty, gdy zapas jest nieczytelny

// ---------- ustawienia z `/konfiguracja.txt` (5.6.4) ----------
struct Ustawienia {
  bool program = false;   // `start.program=tak`
  int dzien = 1;          // `start.dzien=1…miejsc` (R-DS-34)
  int nawilzacz = -1;     // `start.nawilzacz`: 1 tak, 0 nie, -1 nie wiem albo bez wiersza (działa jak tak, R-DS-29)
  bool telefon = false;   // `start.objawy=telefon` (R-DS-50)
  bool ponad = true;      // `start.ponad` (R-DS-14)
};
// Klucz `start.*` z wartością; false, gdy klucz nie jest kluczem programu. Zła wartość: wartość domyślna i `blad`.
bool ustawienie(Ustawienia& u, const std::string& klucz, const std::string& wartosc, std::string& blad);
const int DZIEN_MAKS = 99;  // górna granica `start.dzien` przy czytaniu pliku (dokładna: Katalog::miejsc)

// ---------- stan programu (`/dane/dobry_start.csv`, format `dobry_start` w src/magazyn.cpp, ADR-0022) ----------
struct WpisP {  // porada narysowana (R-DS-10)
  std::string data, id, zrodlo, regula, pora, gdzie;  // zrodlo: plan|dane|ankieta|podstawowa|tryb|ponad
};
struct WpisZ {  // porada zaległa (R-DS-06)
  std::string data, id, regula;
};
struct WpisO {  // odpowiedź ankiety (R-DS-25)
  std::string data, tydzien, pytanie, kody;  // tydzien: `start` albo `t1`…; kody: numery odpowiedzi po `,`
};
struct Stan {
  std::string start;            // dzień włączenia; "" = programu jeszcze nie było
  int kolejka = 1;              // następne miejsce planu
  std::string stan = "aktywny"; // aktywny | przerwa | zatrzymany | zakonczony
  int przerwaDni = 0, d0ZRzedu = 0, ponad = 0, noceTerapii = 0;
  std::string tryb;             // tryb ostatniej oceny (`CPAP`, `AutoSet`, `ASV`, "" nieznany)
  std::string zestaw;           // skrót pliku porad
  std::string ankietaOd;        // początek pierwszego tygodnia ankiety; "" = jeszcze nie
  std::string doba;             // ostatnia doba poranka przetworzona przez nowaDoba()
  int dzienUstawiony = 1;       // `start.dzien` przy starcie albo ostatniej zmianie
  bool planKoniec = false;      // ostatnia porada planu narysowana
  std::string odNowa;           // „Zacznij od nowa”: data (albo `?`, gdy nieznana) do startu od najbliższej doby
  // wybór na dziś przed narysowaniem (wiersz D): data, ID, źródło, reguła, tylko wieczorem
  std::string wDzis, wId, wZrodlo, wRegula;
  bool wWieczor = false, wBezWpisu = false;
  std::vector<WpisP> p;
  std::vector<WpisZ> z;
  std::vector<WpisO> o;
  std::vector<std::pair<int, std::string>> zaliczone;  // miejsca planu zaliczone poza kolejką (miejsce, data)
  std::vector<std::string> wyciek;                      // doby z aktywnym tokenem porad z flagą `wyciek`
  std::vector<std::string> trybPoZmianie;               // bazy porad do pokazania w nowym trybie (R-DS-45)
  std::vector<std::string> bledy;                       // do logu: wiersze pliku nieczytelne
};
// Wiersze pliku (bez nagłówka magazynu) ↔ stan. Nieznany rodzaj wiersza pominięty z wpisem w `bledy`; brak wiersza
// `S;kolejka` = kolejka z ostatniego miejsca planu z wpisów P + 1 (R-DS-43).
Stan stanZWierszy(const std::vector<std::string>& w, const Katalog& k);
std::vector<std::string> wierszeStanu(const Stan& s);
extern const char* const POLA_PLIKU;  // nazwy pól formatu `dobry_start` (rodzaj;a;b;c;d;e;f)

// ---------- wybór porady dnia (spec 5.8.14, rozdz. 5) ----------
struct Doba {
  std::string dzis;               // data poranka (R-NF-20); "" = bez ważnego czasu (U-04)
  Tryb tryb = Tryb::NIEZNANY;     // tryb ostatniej nocy
  // brak nocy w rekordach (R-DS-44): tryb z ostatniej doby programu, a przed pierwszą nocą wariant CPAP i AutoSet
  // (porada planu ma stanąć także bez nocy; tryb spoza listy z nocy to co innego: R-DS-38)
  bool bezNocy = false;
  bool nocZTerapia = false;       // noc z tej doby z terapią (R-SKI-27.noc_z_terapia_m01)
  std::set<std::string> tokeny;   // aktywne reguły po ocenie poranka (tokenyRegul() w dyrygencie)
  bool d0 = false;                // ekran z rana trzymany na cały dzień (D0, 7.2)
  bool w0 = false;                // wieczorem też trzymany (W0: S5, L z pilnością A albo B)
};
enum class Zrodlo : uint8_t { PLAN, DANE, ANKIETA, PODSTAWOWA, TRYB, PONAD };
const char* nazwaZrodla(Zrodlo z);
struct WyborPorady {
  const Porada* porada = nullptr;  // nullptr = bez porady dziś
  Zrodlo zrodlo = Zrodlo::PLAN;
  std::string regula;              // token, który wyzwolił poradę (dane, ankieta), do dziennika
  std::string dataNocy;            // data poranka nocy wyzwalającej (belka `po nocy {DD.MM}`)
  bool tylkoWieczor = false;       // dzień z D0: porada tylko w slocie wieczornym (R-DS-04)
  bool bezWpisu = false;           // porada dnia bez nowego wpisu P (DS z flagą `przerwa` w trwającej przerwie)
  bool narysowana = false;         // wpis P z dziś już jest (restart, kolejne oceny, R-DS-43)
  bool telefon = false;            // `start.objawy=telefon` i porada z flagą `objaw` (R-DS-50)
  int ponadIle = 0;                // porady „ponad program” tej osoby (R-DS-14, R-DS-29: bez nawilżacza mniej)
  std::vector<std::string> log;    // do logu panelu
};
// Raz na dobę poranka (kolejne wywołania tej doby dają ten sam wybór). Zmienia stan (nowa doba, wybór na dziś, zaległe,
// zatrzymanie po wyłączeniu); wywołujący zapisuje plik, gdy wierszeStanu() się zmieniły.
WyborPorady wybierz(const Katalog& k, Stan& s, const Ustawienia& u, const Doba& d);
// Wpis P po narysowaniu porady (R-DS-10): kolejka, miejsca zaliczone, zaległe, koniec planu i programu. `wieczor`:
// slot wieczorny; `telefon`: narysowano „Nowa porada na telefonie.”. Bez zmian przy w.bezWpisu i przy wpisie z dziś.
void zapiszPokazanie(const Katalog& k, Stan& s, const WyborPorady& w, const std::string& dzis, bool wieczor, bool telefon);
// R-DS-33: nowy program (poprzedni stan w kopii `/dane/dobry_start.poprzedni.csv` robi wywołujący).
Stan nowyStan(const Ustawienia& u, const Katalog& k);

// ---------- teksty ekranu S-K-48 (5.4.17, spec 5.8.14 3.3) ----------
struct OpisPorady {
  std::string srodekBelki;   // `dzień {N} z {M}`, `po nocy {DD.MM}`, `z twojej ankiety`, `ponad program`
  std::string etykieta;      // `tydzień {t} z {T}`, `porada do ostatniej nocy`, `porada do ankiety`, `ponad program · {n} z {M}`
  int tydzien = 0;           // pasek tygodni: bieżący tydzień 1…T (0 = bez paska)
  int tygodni = 0;           // T
  int wTygodniu = 0;         // miejsce w bieżącym tygodniu 1…7
  std::string tytul, tresc, krok;
};
extern const char* const NAZWA_BELKI;     // `Dobry start`
extern const char* const TYTUL_TELEFON;   // `Nowa porada na telefonie.`
OpisPorady opis(const Katalog& k, const Stan& s, const WyborPorady& w);

// ---------- ankieta (R-DS-25…R-DS-28, R-DS-48) ----------
std::string tydzienAnkiety(const Stan& s, const std::string& dzis);  // `t1`, `t2`, …
bool ankietaCzeka(const Stan& s, const std::string& dzis);           // tydzień trwa, a zapisu w nim nie ma
// Zapis odpowiedzi: pytanie → numery odpowiedzi (od 1). Odpowiedź wyłączna odznacza pozostałe; pytanie bez odpowiedzi
// zapisane z pustymi kodami (R-DS-48). Pytanie startowe (odTygodnia = 0) dostaje tydzień `start`.
void zapiszAnkiete(const Katalog& k, Stan& s, const std::string& dzis,
                   const std::vector<std::pair<std::string, std::vector<int>>>& odpowiedzi);

// ---------- strona „Wszystkie porady” (R-DS-35) ----------
// Znacznik porady: `pokazana {DD.MM}`, `na telefonie {DD.MM}` albo `jeszcze nie`.
std::string znacznik(const Katalog& k, const Stan& s, const Porada& p);
// Porada w wariancie trybu i nawilżacza (nullptr, gdy porada nie ma wariantu dla tego trybu).
const Porada* wariant(const Katalog& k, const std::string& baza, Tryb t, int nawilzacz);
const Porada* wariantMiejsca(const Katalog& k, int miejsce, Tryb t, int nawilzacz);
// Bazy porad „ponad program” tej osoby w kolejności pliku (bez porady, która stoi albo stała w planie, R-DS-29).
std::vector<std::string> bazyPonad(const Katalog& k, const Stan& s, int nawilzacz);
// To samo bez porad, które nie mają wariantu w trybie `t` (etykieta `ponad program · n z {m}`).
std::vector<std::string> bazyPonad(const Katalog& k, const Stan& s, int nawilzacz, Tryb t);
// Znaczniki `{prog:id}` w tekście porady (R-DS-53) zamienione wartościami z funkcji `wartosc` (id → tekst; "" = bez
// zmiany znacznika).
template <class F> std::string podstawProgi(const std::string& t, F wartosc) {
  std::string w;
  size_t p = 0;
  for (;;) {
    size_t a = t.find("{prog:", p);
    if (a == std::string::npos) break;
    size_t b = t.find('}', a);
    if (b == std::string::npos) break;
    std::string v = wartosc(t.substr(a + 6, b - a - 6));
    w += t.substr(p, a - p) + (v.empty() ? t.substr(a, b - a + 1) : v);
    p = b + 1;
  }
  return w + t.substr(p);
}
Tryb trybZNazwy(const std::string& n);  // `CPAP`, `AutoSet`, `ASV`, `ASVAuto` (→ ASV); inne → NIEZNANY
const char* nazwaTrybu(Tryb t);         // "" dla NIEZNANY
int doba(const Stan& s, const std::string& dzis);  // 1 = dzień włączenia
// Nagłówek sekcji formularza (8.1): `wyłączony`, `dzień {N} z {M}`, `przerwa w terapii`, `ponad program {n} z {M}`,
// `zakończony`; przy czekającej ankiecie dopisek ` · ankieta czeka`.
std::string naglowekSekcji(const Katalog& k, const Stan& s, const Ustawienia& u, const std::string& dzis);

}  // namespace dobry_start
