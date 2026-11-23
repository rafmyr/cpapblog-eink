// SPDX-License-Identifier: GPL-3.0-or-later
// Program „Dobry start” (ADR-0049, ADR-0051, spec funkcjonalna 5.8.14, wymagania R-DS-01…R-DS-71): porady na pierwsze
// 12 tygodni terapii w trzech fazach (start, adaptacja, utrwalenie). Rdzeń bez sprzętu i bez Arduino (ADR-0021): katalog porad z zestawu danych, stan programu z pliku
// `/dane/dobry_start.csv`, wybór porady dnia, dziennik pokazań i ankieta. Rysowanie: src/ekrany_kontekst.cpp
// (rysujPorade()), formularz: src/formularz.cpp, wywołania: src/dyrygent.cpp (rysujPoradeB49()).
//
// Kod nie zna ID porad ani ich liczby: wszystko, co różni porady (faza, doba nominalna, pora, tryb, nawilżacz, ekran,
// warunek, kolejność, limity, stopnie, porada objaśniająca, ostatnia porada fazy i programu, kod QR, fakty karty),
// stoi w kolumnach pliku porad (docs/spec/katalog/porady.csv). Granice faz to ostatnie doby nominalne porad danej fazy.
// Warunek porady wyzwalanej to lista tokenów `|`: porada jest kandydatem, gdy aktywny jest któryś token bez `!`
// i żaden z `!`. Tokeny z danych składa dyrygent z wyników ISTNIEJĄCYCH reguł oceny poranka (tokenyRegul()); tokeny
// `DS-Q{nn}={k}` (odpowiedź k pytania z ankiety) bierze silnik z ostatniej ankiety tygodniowej.
#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <utility>
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
const int POCHWALA_CZEKA_DNI = 1;  // R-DS-59 [PRÓG]: pochwała, która nie stanęła, czeka tyle dni
const int POCHWALA_OKNO_DNI = 7;   // R-DS-59 [PRÓG]: poza startem najwyżej jedna pochwała (nie karta) na tyle dni
const int POCHWALA_LIMIT = 3;      // R-DS-58 [PRÓG]: pochwała bez flagi `raz` i `dwa_razy` najwyżej tyle razy
const int CALA_NOC_MIN = 360;      // R-DS-58 [PRÓG]: „cała noc z maską” od tylu minut M-01 (przy M-17 = 0)
const int DLUZEJ_MIN = 30;         // R-DS-58 [PRÓG]: noc dłuższa od poprzedniej z terapią co najmniej o tyle minut
// fail-path R-DS-69: skok daty poranka w przód o więcej niż SKOK_DNI dób między dwoma wybudzeniami → program
// `wstrzymany`; rusza po powrocie daty albo po SKOK_POTWIERDZ kolejnych dobach z nową datą (wtedy data prawdziwa)
const int SKOK_DNI = 30;           // [PRÓG]
// R-DS-60: tokeny doby, przy których nie ma żadnej pochwały (także wariant `{token}/…`); bez danych nocy też nie.
// `ocena_niepelna`: ocena poranka niepełna przez trwale nieczytelny plik oceny (R-AW-40: bez kart pochwalnych).
constexpr const char* WETO_POCHWAL[] = {"noc_niepewna", "noc_czesciowa",  "E-20",
                                         "S-K-12",       "ekran/problem", "ocena_niepelna"};
const int SKOK_POTWIERDZ = 3;      // [PRÓG]
const int ANKIETA_NOCE = 3;        // R-DS-26 [PRÓG]: pierwszy tydzień ankiety po tylu nocach z terapią
const int ANKIETA_CO_DNI = 7;      // R-DS-26 [PRÓG]: kolejne tygodnie ankiety co tyle dni
const int DWA_RAZY = 2;            // R-DS-28 [PRÓG]: odpowiedź w tylu kolejnych wypełnionych ankietach (flaga dwa_razy)

// ---------- katalog porad (zestaw danych, ADR-0023, ADR-0024) ----------
enum class Rodzaj : uint8_t { PLAN, ZDARZENIE, ANKIETA, POCHWALA };
// Kolejka planu (kolumna `faza`, R-DS-54); poza planem BRAK.
enum class Faza : uint8_t { BRAK, START_RANO, START_WIECZOR, ADAPTACJA, UTRWALENIE };
// Faza programu wyliczona z doby programu (R-DS-54).
enum class Etap : uint8_t { START, ADAPTACJA, UTRWALENIE, KONIEC };
enum class Tryb : uint8_t { NIEZNANY, CPAP, AUTOSET, ASV };  // tryb ostatniej nocy; ASVAuto = ASV (ADR-0030)
const uint8_t TR_CPAP = 1, TR_AUTOSET = 2, TR_ASV = 4, TR_WSZYSTKIE = 8;  // kolumna `tryb` jako maska
enum Flaga : uint16_t {
  FL_PRZERWA = 1, FL_POZA_LIMITEM = 2, FL_BEZ_LIMITU = 4, FL_WYCIEK = 8, FL_BEZ_WYCIEKU_7 = 16,
  FL_PIERWSZA = 32, FL_DWA_RAZY = 64, FL_PODSTAWOWA = 128, FL_KONCOWA = 256, FL_OBJAW = 512,
  FL_POCHWALA = 1024, FL_RAZ = 2048, FL_KONCOWA_FAZY = 4096,
};
// Fragment tekstu w pliku porad (tytuł, treść i krok nie są kopiowane do pamięci: czyta je tekst()).
struct Wycinek {
  uint32_t od = 0;
  uint16_t dl = 0;
};
struct Porada {
  std::string id, baza;  // baza = ID bez wariantu (`DS-D23.cpap` → `DS-D23`)
  Rodzaj rodzaj = Rodzaj::PLAN;
  int dzien = 0;         // doba nominalna porady planu (kolejność kolejki fazy, napis belki); 0 = poza planem
  Faza faza = Faza::BRAK;
  uint8_t tryby = 0;     // maska TR_*
  int8_t nawilzacz = 0;  // kolumna `wymaga`: 1 `nawilżacz`, -1 `bez nawilżacza`, 0 bez wymagania
  bool karta = false;    // `ekran=karta`
  std::string zamiast, wymagaPorady, stopienWyzej;
  std::vector<std::string> warunek, weta;  // tokeny warunku (OR) i tokeny `!` (żaden nie może być aktywny)
  int kol = 0, kolAnkiety = 0;
  uint16_t flagi = 0;
  Wycinek tytul, tresc, krok, qr, fakty;  // qr: adres kodu QR (R-DS-62); fakty: `liczba::podpis | …` (R-DS-64)
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
  int miejsc = 0;                // ostatnia doba nominalna planu = koniec programu (84 w zestawie z 22.11.2026)
  int koniecStartu = 0;          // ostatnia doba nominalna kolejek startu (10)
  int koniecAdaptacji = 0;       // ostatnia doba nominalna adaptacji (42)
  int utrwalenieIle = 0;         // porad planu w utrwaleniu (różnych dób nominalnych, 12)
  bool jest() const { return !porady.empty(); }
  int tygodni() const { return (miejsc + 6) / 7; }
  Etap etap(int dobaProgramu) const;
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
  int dzien = 1;          // `start.dzien=1…miejsc` (R-DS-65)
  int nawilzacz = -1;     // `start.nawilzacz`: 1 tak, 0 nie, -1 nie wiem albo bez wiersza (działa jak tak, R-DS-29)
  bool telefon = false;   // `start.objawy=telefon` (R-DS-50)
};
// Klucz `start.*` z wartością; false, gdy klucz nie jest kluczem programu. Zła wartość: wartość domyślna i `blad`.
bool ustawienie(Ustawienia& u, const std::string& klucz, const std::string& wartosc, std::string& blad);
const int DZIEN_MAKS = 99;  // górna granica `start.dzien` przy czytaniu pliku (dokładna: Katalog::miejsc)
// Doba programu dnia włączenia z `start.dzien`; spoza 1…miejsc (ręczna edycja pliku) → 1 (fail-path R-DS-65).
int dzienStartu(const Ustawienia& u, const Katalog& k);

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
  std::string stan = "aktywny"; // aktywny | przerwa | zatrzymany | wstrzymany | zakonczony
  int przerwaDni = 0, d0ZRzedu = 0, noceTerapii = 0;  // przerwaDni: doby przerwy w terapii i z wyłączonym programem
  std::string tryb;             // tryb ostatniej oceny (`CPAP`, `AutoSet`, `ASV`, "" nieznany)
  std::string zestaw;           // skrót pliku porad
  std::string ankietaOd;        // początek pierwszego tygodnia ankiety; "" = jeszcze nie
  std::string doba;             // ostatnia doba poranka przetworzona przez nowaDoba()
  int dzienUstawiony = 1;       // `start.dzien` przy starcie albo ostatniej zmianie: doba programu dnia włączenia
  bool planKoniec = false;      // porada `koncowa` narysowana (od następnej doby `zakonczony`)
  std::string odNowa;           // „Zacznij od nowa”: data (albo `?`, gdy nieznana) do startu od najbliższej doby
  std::string skokData;         // `wstrzymany`: ostatnia data poranka po skoku zegara; skokIle: kolejne doby z nią
  int skokIle = 0;
  // wybór na dziś przed narysowaniem (wiersz D): data, ID, źródło, reguła, tylko wieczorem
  std::string wDzis, wId, wZrodlo, wRegula;
  bool wWieczor = false, wBezWpisu = false;
  std::string eDzis, eId;       // wybór porady wieczornej startu przed narysowaniem (wiersz E)
  std::vector<WpisP> p;
  std::vector<WpisZ> z;
  std::vector<WpisO> o;
  std::vector<std::pair<std::string, std::string>> zaliczone;  // porady planu zaliczone poza kolejką (baza, data)
  std::vector<std::string> wyciek;                      // doby z aktywnym tokenem porad z flagą `wyciek`
  std::vector<std::string> trybPoZmianie;               // bazy porad do pokazania w nowym trybie (R-DS-45)
  std::vector<std::string> bledy;                       // do logu: wiersze pliku nieczytelne
};
// Wiersze pliku (bez nagłówka magazynu) ↔ stan. Nieznany rodzaj wiersza pominięty z wpisem w `bledy`. Kolejki faz nie
// są zapisywane: porada planu jest pokazana, gdy jest wpis P tej porady albo porady tej samej fazy i doby nominalnej
// (R-DS-43, R-DS-67). Wiersze wersji 1 (`S;kolejka`, `S;ponad`, `K;{miejsce}`) przy odczycie pominięte z wpisem w `bledy`
// (migracja v1 → v2).
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
enum class Zrodlo : uint8_t { PLAN, DANE, ANKIETA, PODSTAWOWA, TRYB, POCHWALA };
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
  std::vector<std::string> log;    // do logu panelu
};
// Raz na dobę poranka (kolejne wywołania tej doby dają ten sam wybór). Zmienia stan (nowa doba, wybór na dziś, zaległe,
// zatrzymanie po wyłączeniu); wywołujący zapisuje plik, gdy wierszeStanu() się zmieniły.
WyborPorady wybierz(const Katalog& k, Stan& s, const Ustawienia& u, const Doba& d);
// Porada slotu wieczornego (W3b, R-DS-55): w fazie start z kolejki `start_wieczor` (inna niż poranna), poza startem
// porada dnia z wybierz(). Woła wybierz() (nowa doba, ten sam stan). W0 → bez porady.
WyborPorady wybierzWieczor(const Katalog& k, Stan& s, const Ustawienia& u, const Doba& d);
// Doba programu (R-DS-54): doby od dnia włączenia minus doby przerwy i wyłączenia, od `start.dzien`; 0 = przed startem.
int dobaProgramu(const Stan& s, const std::string& dzis);
// Wpis P po narysowaniu porady (R-DS-10): kolejka, miejsca zaliczone, zaległe, koniec planu i programu. `wieczor`:
// slot wieczorny; `telefon`: narysowano „Nowa porada na telefonie.”. Bez zmian przy w.bezWpisu i przy wpisie z dziś.
void zapiszPokazanie(const Katalog& k, Stan& s, const WyborPorady& w, const std::string& dzis, bool wieczor, bool telefon);
// R-DS-33: nowy program od najbliższej doby (poprzedni stan w kopii `/dane/dobry_start.poprzedni.csv` robi wywołujący).
Stan nowyStan(const Ustawienia& u, const Katalog& k);

// ---------- teksty ekranu S-K-48 (5.4.17, spec 5.8.14 3.3) ----------
struct OpisPorady {
  std::string srodekBelki;   // `start · dzień {d} z 10`, `adaptacja · dzień {d} z 42`, `utrwalenie · tydzień {t} z 12`,
                             // `po nocy {DD.MM}`, `z twojej ankiety` (R-DS-57)
  std::string etykieta;      // `na dziś`, `na dziś wieczór`, `tydzień {t} z 6`, `porada {n} z 12`, `za ostatnią noc`, …
  int tydzien = 0;           // pasek tygodni: bieżący tydzień 1…T (0 = bez paska)
  int tygodni = 0;           // T
  int wTygodniu = 0;         // miejsce w bieżącym tygodniu 1…7
  std::string tytul, tresc, krok;
  std::string qr;            // adres kodu QR ("" = bez kodu; także „tylko na telefonie”)
  std::vector<std::pair<std::string, std::string>> fakty;  // rząd faktów karty: (liczba ze znacznikami, podpis)
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
// Porada planu fazy `f` na dobie nominalnej `dzien` w wariancie trybu i nawilżacza (nullptr: brak).
const Porada* wariantDoby(const Katalog& k, Faza f, int dzien, Tryb t, int nawilzacz);
const char* nazwaFazy(Faza f);  // `start_rano`, `start_wieczor`, `adaptacja`, `utrwalenie`, ""
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
// ---------- dane nocy: pochwały i znaczniki w tekstach (R-DS-58, R-DS-60) ----------
struct NocProgramu {  // noc z rekordów panelu (poranek `data`)
  std::string data;
  int m01 = -1, m17 = -1;  // minuty z maską, zdjęcia maski; -1 = brak
  int wyciek = -1;         // M-07 P95 w L/min, zaokrąglony; -1 = brak
};
struct ProgiPochwal {
  int terapia = 0;       // R-SKI-27.noc_z_terapia_m01 (minuty)
  int m01Min = 0;        // R-DANE-13.m01_min (minuty)
  int wyciekUwaga = 0;   // S-K-10.uwaga_o_masce (L/min, także ustawienie R-KONF-42)
  int noceProblem = 0;   // E-10.noce_problem (noce bez terapii przed powrotem)
};
// Tokeny `pochwala/…` ostatniej nocy (`noce` rosnąco po dacie, ostatnia z poranka `dzis`); `seria` = M-20.
void tokenyPochwal(const std::vector<NocProgramu>& noce, const std::string& dzis, int seria, const ProgiPochwal& p,
                   std::set<std::string>& t);
struct Znaczniki {  // wartości znaczników `{…}` (tabela 4.3); -1 = brak (`–`)
  int czas = -1, oIleDluzej = -1, nocyZRzedu = -1, wyciek = -1, nocyZMaska = -1, nocy4h = -1, srednio = -1;
  int nocyZMaskaProg = -1, srednioProg = -1;
};
Znaczniki znacznikiNocy(const std::vector<NocProgramu>& noce, const std::string& dzis, const std::string& start,
                        int seria, const ProgiPochwal& p);
// `{czas}`, `{srednio}` … zamienione wartościami (czas `6 h 38` albo `25 min`, braki `–`); nieznany znacznik bez zmian.
std::string podstawZnaczniki(const std::string& t, const Znaczniki& z);
// Znaczniki danych jako opis (`{czas}` → `[czas z maską tej nocy]`), dla strony „Wszystkie porady”.
std::string opisZnacznikow(const std::string& t);
// Pary `liczba::podpis` kolumny `fakty` porady (R-DS-64).
std::vector<std::pair<std::string, std::string>> fakty(const Katalog& k, const Porada& p);
// Fakty po podstawieniu znaczników, przed rysowaniem: brak nocy w oknie albo 0 → bez rzędu faktów (fail-path R-DS-60);
// podpis `nocy …` pod samą liczbą odmieniony (`1 noc`, `2 noce`, `5 nocy`).
void faktyNaEkran(std::vector<std::pair<std::string, std::string>>& f);

Tryb trybZNazwy(const std::string& n);  // `CPAP`, `AutoSet`, `ASV`, `ASVAuto` (→ ASV); inne → NIEZNANY
const char* nazwaTrybu(Tryb t);         // "" dla NIEZNANY
int doba(const Stan& s, const std::string& dzis);  // 1 = dzień włączenia
// Nagłówek sekcji formularza (6.2 wersji 2): `wyłączony`, `start · dzień {d} z 10`, `adaptacja · dzień {d} z 42`,
// `utrwalenie · tydzień {t} z 12`, `przerwa w terapii`, `zakończony`; przy czekającej ankiecie ` · ankieta czeka`.
std::string naglowekSekcji(const Katalog& k, const Stan& s, const Ustawienia& u, const std::string& dzis);

}  // namespace dobry_start
