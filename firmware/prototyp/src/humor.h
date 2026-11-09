// SPDX-License-Identifier: GPL-3.0-or-later
// Ekran dnia, karta z budzikiem i zdania budzika (ADR-0007, ADR-0008, ADR-0009, moduł KARTA I HUMOR 5.7),
// bez zależności od Arduino.
//
// Dziennik wyświetleń `/dane/humor.csv` (magazyn src/magazyn.h; plik prototypu `/humor.csv` migruje do niego),
// jeden wiersz na wyświetlenie ekranu dnia:
//   data;typ;klucz;tryb;scena;zdanie
//   typ:   S2A, S2B, S2C (dowód spokoju), S5 (brak danych), S6 (rozruch), KM (kamień milowy S-K-28)
//   klucz: S2A/S2C numer kolejnego wyświetlenia (R-HUM-09), S5 epizod braku danych (R-HUM-08), KM liczba nocy
//   tryb:  K = karta z budzikiem, D = ekran danych
//   zdanie: identyfikator `pula:numer` (R-HUM-20)
// „Wyświetlenie” w prototypie = pierwszy wybór danego ekranu dnia w danym dniu; to samo wyświetlenie
// odświeżane co 30 min nie jest nowym wyświetleniem. Wyjątek S5: karta tylko przy pierwszym rysunku braku danych.
//
// Warunek uspokojenia (5.4.13) liczy src/stany.cpp w zakresie danych panelu; zdania uspokajające
// (5.7.5, R-EKR-32) są losowane tylko przy KontekstHumoru::uspokojenie. S2c wchodzi do rotacji, gdy jest norma sypialni
// i odczyt ostatniej nocy (StanDnia::sypialnia).
// Od humor zna całą tabelę 5.7.3 (typKarty, ekranKarty), wszystkie pule 5.7.5 i przerywnik S2d
// (wybierzPrzerywnik()). Typy w dzienniku poza S2A, S2B, S2C, S5, S6, S10, KM:
//   S2D przerywnik (klucz `L` = z limitu 1 na 7 dni, `O` = okazja ze stałą datą);
//   SK01…SK09, SK10, SK16, SK18, SK21, SK27, SK29, SK31, SK32 = ekrany S-K-xx (klucz: sprawa, a przy regule
//   „1 na 2” `{sprawa}#{n}`, n = numer wyświetlenia sprawy, R-HUM-07);
//   S1 odkrycie (klucz `O-xx`, przy O-12 `O-12/{kod czynnika}` z src/czynniki.h).
// Przerywniki nie są losowane, dopóki UstawieniaPrzerywnikow::tekstyPrzyjete = false (decyzja: brak przyjętych
// tekstów, 5.7.6, 5.7.8); ekrany S-K, S1 i S2D rysuje dopiero moduł ekranów, humor daje tryb, scenę
// i zdanie.
#pragma once
#include <string>
#include <vector>
#include "noc.h"

enum class Humor : unsigned char { WYLACZONY = 0, OSZCZEDNY = 1, NORMALNY = 2 };

struct WpisHumoru {
  std::string data, typ, klucz;
  char tryb = 'D';
  std::string scena, zdanie;
};
bool parsujHumor(const std::string& linia, WpisHumoru& w);
std::string zapiszHumor(const WpisHumoru& w);
const WpisHumoru* znajdzWpis(const std::vector<WpisHumoru>& log, const std::string& data, const std::string& typ,
                             const std::string& klucz);

// ---------- pule zdań (5.7.5) ----------
// Lista `kanwa` = zdania przyjęte (od 08.11.2026 cała przyjęta pula, kryterium WY15 spełnione); lista `propozycje` =
// zdania do akceptacji, losowane tylko przy zPropozycjami (przełącznik prototypu HUMOR_PROPOZYCJE). Od 08.11.2026 lista
// propozycji jest pusta w każdej puli, więc przełącznik nie zmienia zdań; zostaje na następną rundę propozycji.
struct Pula {
  const char* id;
  std::vector<const char*> kanwa, propozycje;
};
const Pula* pula(const std::string& id);
bool uspokajajace(const std::string& zdanie);  // lista „nic się nie dzieje” (R-EKR-32)
std::vector<std::string> zdaniaDoLosowania(const Pula& p, bool zPropozycjami, bool uspokojenie);
// Tekst zdania o ID `pula:numer` (numer liczony przez kanwę, potem propozycje), np. `seria:0`.
// Zwraca "" dla nieznanej puli i dla numeru, który nie jest liczbą 0…(rozmiar puli − 1) zapisaną samymi
// cyframi (ID przychodzi z `/humor.csv`, więc może być uszkodzone: `seria:-1`, `seria:abc`).
std::string tekstZdania(const std::string& id);
// Czy kartę z wpisu `/humor.csv` wolno narysować ponownie (slot nocny, to samo wyświetlenie dnia): zdanie z ID
// ma tekst w dzisiejszej puli (stary numer spoza nowej puli → nie) i nie jest uspokajające bez spełnionego
// warunku uspokojenia (R-EKR-32, ADR-0029 pkt 7). `uspokojenie` wołane tylko dla zdania uspokajającego (koszt).
// Przy `false` panel rysuje ekran danych zamiast karty.
template <class F> bool kartaZWpisuWazna(const std::string& id, F uspokojenie) {
  std::string z = tekstZdania(id);
  return !z.empty() && (!uspokajajace(z) || uspokojenie());
}

// ---------- ekran dnia ----------
struct StanDnia {
  std::string dzis;
  bool brakDanych = false;   // brak odczytu i brak danych ostatniej nocy (S5)
  std::string kluczBraku;    // epizod braku danych: czas ostatniego udanego odczytu
  bool kartaBraku = true;    // false: brak Wi-Fi albo nieczytelny raport → S5 zawsze ekranem danych (R-HUM-08, A-59)
  bool synchronizacja = false;  // S10: noc jeszcze w drodze (R-EKR-57)
  bool nocDzis = false;      // jest rekord nocy z datą poranka = dziś
  bool nocDzisPonad4h = false;
  int nocyWaznych = 0;       // do rozruchu globalnego (< 14, rozdz. 4.10)
  bool maNorme = false;      // norma czasu z maską (R-NOR-10)
  int seria = 0;             // M-20
  int nocyPonad4h = 0;       // S-K-28
  bool sypialnia = false;    // S2c możliwe: mediana ostatniej nocy z czujnika panelu i norma sypialni (≥ 14 nocy)
};

struct EkranDnia {
  std::string typ, klucz;
  bool kartaZTabeli = false;  // tabela 5.7.3 przy humorze `normalny`, przed limitami
  bool kartaOszczedny = false;
  bool tonProblem = false;    // karta w tonie „problem” (S-K-10, S-K-16, S-K-31): jasne tło, czarny pasek nagłówka
                              // i etykieta w czarnej ramce (R-HUM-24); rysuje ekrany, humor tylko oznacza
};

// Kolejność (rozdz. 7.2 w zakresie prototypu): S5 (R1) > S10 synchronizacja (R-EKR-57) > kamień milowy S-K-28 (R7) > rozruch S6 (R9)
// > dowód spokoju w rotacji S2a → S2b → S2c (R9, 5.4.6, R-EKR-16).
EkranDnia wybierzEkranDnia(const StanDnia& s, const std::vector<WpisHumoru>& log);
// Liczba z pola `klucz` wpisu `/humor.csv` (licznik wyświetleń S2a/S2c, liczba nocy kamienia).
// Zwraca true i `n` w 0…KLUCZ_MAX tylko dla samych cyfr w tym zakresie; inaczej false i `n` bez zmian
// (plik może być uszkodzony: `2147483647`, `-5`, `abc`).
extern const int KLUCZ_MAX;  // 1 000 000: ponad 2700 lat wyświetleń co dzień, więc licznik + 1 nie przepełni int
bool liczbaKlucza(const std::string& klucz, int& n);
extern const int KAMIENIE[5];  // 7, 30, 100, 180, 365 [PRÓG]
extern const int SERIA_MIN;    // S2b od tylu nocy z rzędu ponad 4 h (R-EKR-16) [PRÓG]
// S10: tekst zdania karty bez dziennika (ekran przejściowy), rotacja po dacie; "" = brak zdań → ekran danych
std::string zdanieSynchronizacji(const std::string& dzis, bool zPropozycjami);

// ---------- warunki kart przerywników (R-HUM-28) ----------
// Progi nazwane w jednym miejscu; nowe `[PRÓG]` są w docs/spec/progi.csv (R-HUM-28.*), pozostałe to progi istniejące:
// PROG_AHI = E-30 / (kokpit.h), PROG_WYCIEK = S-K-10.przeciek_nosowa (24 L/min, kokpit.h), M-01 4 h = R-DANE-13.m01_min.
const double PRZ_AHI_BARDZO_NISKIE = 1.0;   // R-HUM-28.ahi_bardzo_niskie [PRÓG]: AHI < 1
const double PRZ_AHI_DUZO = 10.0;           // R-HUM-28.ahi_duzo_zdarzen [PRÓG]: AHI ≥ 10
const int PRZ_M01_KROTKA_MIN = 4 * 60;      // R-DANE-13.m01_min: krótka terapia < 4 h, noc ważna ≥ 4 h
const int PRZ_M01_DLUGA_MIN = 7 * 60;       // R-HUM-28.terapia_dluga [PRÓG]: długi czas terapii ≥ 7 h ORAZ wyraźna poprawa (niżej)
const int PRZ_M01_POPRAWA_MIN = 60;         // R-HUM-28.poprawa_m01 [PRÓG]: M-01 ostatniej nocy co najmniej o tyle dłuższe od mediany M-01 z 7 poprzednich nocy ważnych
const int PRZ_M01_POPRAWA_NOCE = 5;         // R-HUM-28.poprawa_noce [PRÓG]: tyle poprzednich nocy ważnych z 7 wymaga porównanie; mniej = brak karty z warunkiem
const int PRZ_M01_W_CELU_MIN = 6 * 60;      // R-HUM-28.terapia_w_celu [PRÓG]: „wszystkie parametry w celu” ≥ 6 h
const int PRZ_TYDZIEN_NOCE = 5;             // R-HUM-28.tydzien_noce [PRÓG]: co najmniej 5 nocy ważnych z 7
const double PRZ_WYCIEK_NIEWIELKI = 10.0;   // R-HUM-28.wyciek_niewielki [PRÓG]: G15 „wycieki niewielkie”: M-07 P95 do tylu L/min
const double PRZ_WYCIEK_E23 = 27.0;         // E-23.przeciek_p95 (6.4): wyciek P95 nocy, od którego jest zdarzenie E-23
const double PRZ_M08_MINUTY_E20 = 30.0;     // E-20.minuty: M-08 co najmniej tyle minut to duży wyciek (E-20)
const double PRZ_M08_ODSETEK_E20 = 0.10;    // E-20.odsetek_czasu: 10 % czasu z maską
// Fakty ostatniej nocy, z których karta przerywnika wybiera tekst warunkowy albo neutralny. `jest` = rekord nocy z datą
// poranka = dziś, kompletny (nie `partial`); `ocenione` = noc ważna (M-01 ≥ 4 h), nie niepewna (4.9) i z AHI: dopiero
// wtedy warunki z AHI mogą być spełnione. „Nie oceniam” (ADR-0029 pkt 7) nigdy nie daje tekstu „dobrego”.
struct NocPrzerywnika {
  bool jest = false, wazna = false, ocenione = false;
  int m01 = -1;                  // minuty z maską
  double ahi = NAN, wyciekP95 = NAN;
  bool stanAhi = false;          // stan odchylenia AHI (4.6) trwa
  int noce7 = 0;                 // noce z AHI ocenione w 7 dniach do dziś włącznie
  double medianaAhi7 = NAN;      // mediana AHI z tych nocy
  double maxAhi7 = NAN;          // największe AHI z tych nocy
  double m08 = NAN;              // M-08: minuty dużego wycieku ostatniej nocy
  int noceM01Poprz = 0;          // noce ważne (kompletne) w 7 dniach przed dzisiejszym
  double medianaM01Poprz = NAN;  // mediana M-01 (min) z tych nocy
  bool poprzedniaWazna = false;  // noc z dnia przed dzisiejszym jest ważna i kompletna („kolejna” noc)
  // w 7 dniach do dziś był E-23 (wyciek P95 w nocy powyżej E-23.przeciek_p95), ekran S3 albo ekran L;
  // z historii nocy ustawia to nocPrzerywnika(), z pokazań ekranów i dziennika dyrygent
  bool problem7 = false;
  // warunek uspokojenia 5.4.13 (Stany::uspokojenie). Karty pochwalne mają wariant z warunkiem tylko przy
  // true; domyślnie true, żeby test jednostkowy warunku nie wymagał silnika. Dyrygent ustawia z dnia
  bool uspokojenie = true;
};
NocPrzerywnika nocPrzerywnika(const std::vector<Noc>& hist, const std::string& dzis, bool stanAhi);
// Warunki nazwane (R-HUM-28). Każdy daje false, gdy metryka nie jest oceniona.
bool duzeWycieki(const NocPrzerywnika& n);       // M-07 P95 powyżej S-K-10.przeciek_nosowa (24 L/min)
bool podwyzszoneAhi(const NocPrzerywnika& n);    // AHI ≥ 5
bool niskieAhi(const NocPrzerywnika& n);         // AHI < 5
bool bardzoNiskieAhi(const NocPrzerywnika& n);   // AHI < 1
bool stabilneAhi(const NocPrzerywnika& n);       // niskie AHI i bez stanu odchylenia AHI
bool duzoZdarzen(const NocPrzerywnika& n);       // AHI ≥ 10
bool krotkaTerapia(const NocPrzerywnika& n);     // M-01 poniżej 4 h (noc z terapią)
bool dlugaTerapia(const NocPrzerywnika& n);      // M-01 co najmniej 7 h i co najmniej 1 h ponad medianą z 7 poprzednich nocy (min. 5 nocy)
bool dobraTerapia(const NocPrzerywnika& n);      // AHI < 5 (po zaokrągleniu do 0,1), bez stanu odchylenia AHI, wyciek poniżej progu, M-01 co najmniej 4 h
bool dobryTydzien(const NocPrzerywnika& n);      // mediana AHI z 7 nocy < 5 przy co najmniej 5 nocach, ostatnia noc AHI < 5, żadna noc z AHI ≥ 10, bez E-23, S3 i ekranu L w 7 dniach
bool bezZarzutu(const NocPrzerywnika& n);        // dobra terapia i M-01 co najmniej 6 h (G18)
bool wszystkoWCelu(const NocPrzerywnika& n);     // bez zarzutu, wyciek P95 do 10 L/min i M-08 poniżej progu E-20 (G15)
bool stabilnyTydzien(const NocPrzerywnika& n);   // co najmniej 5 nocy z 7 ocenionych, wszystkie AHI < 5, bez stanu AHI (G08)
bool solidnaNoc(const NocPrzerywnika& n);        // długa terapia, AHI < 5, wyciek zmierzony poniżej 24 L/min, poprzednia noc ważna (G06, G16)
// Warunek sceny G01…G18 (tabela 5.7.6); false dla sceny spoza G.
bool warunekPrzerywnika(const std::string& scena, const NocPrzerywnika& n);

// ---------- tryb: karta albo ekran danych ----------
struct KontekstHumoru {
  std::string dzis;
  Humor humor = Humor::NORMALNY;
  bool liniaUwagi = false;      // R-HUM-17
  bool zPropozycjami = false;
  bool uspokojenie = false;     // warunek 5.4.13 (Stany::uspokojenie)
  std::vector<std::string> sceny;  // sceny dostępne w pamięci panelu (R-HUM-11)
  bool wzorzecB = false;        // aktywny wzorzec z pilnością B, także wyciszony (R-HUM-26)
  NocPrzerywnika noc;           // fakty ostatniej nocy do warunku karty przerywnika (R-HUM-28); domyślnie „brak danych”
};

struct Wybor {
  char tryb = 'D';
  std::string scena, zdanie;
  const char* dlaczego = "";  // powód ekranu danych, do logu
};

std::vector<std::string> scenyTypu(const std::string& typ, const std::string& klucz);
const char* pulaTypu(const std::string& typ);

// Kolejność sprawdzeń: humor (R-HUM-03, R-HUM-06) → tabela 5.7.3 → linia uwagi (R-HUM-17) → pula ≥ 3 zdania
// (R-HUM-27) → tekst mieści się w karcie (R-HUM-05) → 2 karty na dobę (R-HUM-02) → połowa wyświetleń w oknie
// 7 dni (R-HUM-23) → scena spoza 6 poprzednich dni (R-HUM-10, R-HUM-11) → zdanie najdawniej użyte (R-HUM-20).
// Powtarzalny: wpis z dziś tego samego wyświetlenia (data, typ, klucz) jest pomijany, więc ponowna ocena przy
// odświeżeniu daje ten sam tryb, scenę i zdanie. S2D zawsze ekran danych (przerywnik: wybierzPrzerywnik()).
Wybor wybierzTryb(const EkranDnia& e, bool tekstMiesci, const std::vector<WpisHumoru>& log, const KontekstHumoru& k);

// ---------- tabela 5.7.3 dla wszystkich ekranów z kartą ----------
enum class RegulaKarty : unsigned char {
  ZAWSZE,                // karta zawsze (S2b, kamienie, S-K-18, S-K-29, odkrycia, S-K-32, S2d, S10)
  CO_TRZECIE,            // S2a, S2c: co trzecie wyświetlenie (R-HUM-09), licznik w wybierzEkranDnia()
  JEDEN_NA_DWA,          // wieczór S-K-01…S-K-09: karta przy 1., 3., 5. … wyświetleniu sprawy (R-HUM-07)
  JEDEN_NA_DWA_PROBLEM,  // rano S-K-10, S-K-16, S-K-31 (ton „problem”): karta przy 2., 4. … wyświetleniu (R-HUM-07)
  PIERWSZY_BRAK,         // S5 przy E-01: karta przy pierwszym wyświetleniu danego braku danych (R-HUM-08); tylko przez
                         // wybierzEkranDnia() (StanDnia::kartaBraku: E-03 i E-05 krytyczne zawsze ekranem danych)
  PRZERYWNIK,            // S2d: wyłącznie przez wybierzPrzerywnik (bramka, R-HUM-26); ekranKarty i wybierzTryb
                         // nigdy nie dają S2D jako karty
};
struct TypKarty {
  const char* typ;                 // typ w `/humor.csv`
  RegulaKarty regula;              // kolumna „Tryb (humor `normalny`)”
  bool oszczedny;                  // kolumna „Oszczędny”: karta także przy humorze `oszczędny` (R-HUM-06)
  bool problem;                    // ton „problem” karty (R-HUM-24), przechodzi do EkranDnia::tonProblem
  const char* pula;                // pula zdań 5.7.5
  std::vector<const char*> sceny;  // kolumna „Sceny”; KM i S1 zależą od klucza (scenyTypu())
};
const std::vector<TypKarty>& typyKart();
const TypKarty* typKarty(const std::string& typ);  // nullptr: typ bez karty (S6, wzorce L, K1, K2, …)
// R-HUM-25: krok kieruje do lekarza, gdy zawiera `lekarz` (też `lekarzowi`, `lekarzem`), `kontroli` albo `pomoc`.
bool krokDoLekarza(const std::string& krok);
// Ekran z kartą spoza rotacji dnia (S-K, S1; S2a, S2b, S2c, S5, S10 i KM wybiera wybierzEkranDnia()). Przyjmuje typ,
// sprawę (np. dzień, od którego trwa warunek; przy S1 klucz `O-xx` albo `O-12/{kod czynnika}`, przy SK21 `pylek` albo
// `co2`), dziś, dziennik i krok ekranu. Zwraca EkranDnia z kluczem i trybem z tabeli 5.7.3: reguła „1 na 2” liczy
// wyświetlenia sprawy z dziennika (numer w kluczu; dzień z wpisem zostaje przy swoim numerze), a o karcie decyduje
// tryb poprzedniego wyświetlenia tej sprawy: po ekranie danych karta, po karcie ekran danych; wyświetlenie, które
// ustąpiło linii uwagi albo limitowi, zapisane jako ekran danych, więc następne jest kartą (5.7.8, R-HUM-17). Krok do
// lekarza i odkrycia wrażliwe (O-05, O-10, O-11, O-12 przy A, K, L, T; SK21 `pylek`) dają ekran danych (R-HUM-25).
// S5 i S2D zawsze bez karty: S5 wybiera wybierzEkranDnia, S2D wybierzPrzerywnik.
// `krok`: krok ekranu ("" = bez kroku), zawsze podawany, żeby wywołujący nie pominął R-HUM-25.
EkranDnia ekranKarty(const std::string& typ, const std::string& sprawa, const std::string& dzis,
                     const std::vector<WpisHumoru>& log, const std::string& krok);

// ---------- przerywnik S2d (5.7.6, R-HUM-14, R-HUM-15, R-HUM-16, R-HUM-21, R-HUM-22, R-HUM-26, R-HUM-27) ----------
struct UstawieniaPrzerywnikow {
  bool tekstyPrzyjete = false;     // do przyjęcia kompletu tekstów przerywniki nie są losowane (5.7.8)
  bool zDatami = false;            // sceny z datami G01–G08 i poniedziałek/piątek (opcjonalne po, R-HUM-15)
  std::vector<std::string> ogolne; // włączone sceny puli ogólnej; puste = G09–G18 (obowiązkowe co najmniej G10, G12,
                                   // G13, G14)
};
struct Przerywnik {
  bool jest = false;
  std::string scena, zdanie, klucz;  // klucz: `L` z limitu 1 na 7 dni, `O` okazja ze stałą datą
  std::string okazja;                // nazwa okazji (`Poniedziałek`, `Nowy Rok`…) do logu; na karcie bez etykiety
  bool warunkowa = false;            // R-HUM-28: noc spełnia warunek sceny → tekst warunkowy, inaczej neutralny
  const char* dlaczego = "";         // powód braku przerywnika, do logu
};
// Okazja ze stałą datą dnia `dzis` (tabela 5.7.6): G01 ostatnia niedziela marca i października, G04 1.01, G05 25.12,
// G06 niedziela wielkanocna, G07 21.03, G08 23.09; przy dwóch: niższe ID. "" = brak okazji.
std::string okazjaDnia(const std::string& dzis);
std::string wielkanoc(int rok);  // RRRR-MM-DD niedzieli wielkanocnej (kalendarz gregoriański)
// Czy w miejscu dowodu spokoju `dowod` (wybierzEkranDnia) jest przerywnik. Kolejność: teksty przyjęte →
// humor `normalny` (R-HUM-16) → dowód spokoju (S2A, S2B, S2C) bez wpisu z dziś innego typu → wzorzec B (R-HUM-26) →
// linia uwagi (R-HUM-22) → okazja ze stałą datą (zDatami) albo 7 dni od ostatniego przerywnika (R-HUM-14) → scena
// spoza 6 poprzednich dni, w pamięci, z przypiętym zdaniem (R-HUM-10, R-HUM-11, R-HUM-27) → limity kart (R-HUM-02,
// R-HUM-23). Dzień z wpisem S2D zostaje przerywnikiem z tą samą sceną i zdaniem. Wywołujący zapisuje w `/humor.csv`
// wiersz `{dzis};S2D;{klucz};K;{scena};{zdanie}` zamiast wiersza dowodu spokoju.
Przerywnik wybierzPrzerywnik(const EkranDnia& dowod, const std::vector<WpisHumoru>& log, const KontekstHumoru& k,
                             const UstawieniaPrzerywnikow& u);
// W wersji beta przerywniki są losowane z kompletem tekstów
// G01–G18 z propozycji do akceptacji na odbiorze (5.7.6, tabela „Teksty przerywników”), ze scenami z datami
// (poniedziałek, piątek, okazje ze stałą datą, R-HUM-15). Po decyzji „nie” wystarczy tu tekstyPrzyjete = false.
UstawieniaPrzerywnikow ustawieniaPrzerywnikowBety();
// Teksty karty przerywnika sceny `scena` (G01…G18) z zestawu: `S2D.{scena}.warunek.tytul|powod`
// albo `S2D.{scena}.neutralna.tytul|powod`, teksty.csv, 5.7.6. Teksty autora, bez
// kroku i bez etykiety nad tytułem. Powód może być pusty (zdanie jest cały w tytule). Pusty tytuł (scena spoza G, tekst
// spoza zestawu) = bez przerywnika (dowód spokoju).
struct TekstPrzerywnika {
  std::string tytul, powod;
};
// `dzis` (RRRR-MM-DD) tylko dla neutralnej G01: w marcu tekst `S2D.G01.neutralna_marzec.tytul` (zegar zabiera godzinę).
TekstPrzerywnika tekstPrzerywnika(const std::string& scena, bool warunkowa, const std::string& dzis = "");
