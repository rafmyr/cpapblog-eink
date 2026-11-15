// SPDX-License-Identifier: GPL-3.0-or-later
// Rysowanie ekranów 800×480 1-bit, bez zależności od Arduino: na panelu przez GFXcanvas1,
// na komputerze przez bufor w pamięci (firmware/prototyp/test/podglad.cpp → PNG).
// Kroje i sceny: include/fonty.h, include/obrazy.h (generuje tools/prototyp/generuj.py).
// Tekst w UTF-8; znaki ◀ ▶ ↻ rysowane są jako ikony przycisków, ◆ jako romb (R-KOK-14), ↔ ↑ ↓ jako strzałki
// rysowane prymitywami (wysokość = wersalik kroju, grubość = px/11, skala B, docs/design/czytelnosc-b/SPEC-B.md).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <gfxfont.h>  // Adafruit GFX; na komputerze atrapa z firmware/prototyp/test/atrapy/
#include "dobranoc.h"
#include "kokpit.h"

// src/nawigacja.h (bez dołączania całego nagłówka: wyliczenie `Ekran` nie wchodzi do każdego pliku z ekrany.h):
// strona „Sypialnia · 30 nocy” w ciągu kokpitu (R-KOK-29), domyślny podpis ▶/◀ stopek sąsiednich stron
bool sypialniaWKokpicie();

struct Plotno {
  virtual ~Plotno() {}
  virtual void piksel(int x, int y, bool czarny) = 0;
  virtual void prostokat(int x, int y, int w, int h, bool czarny);
  void ramka(int x, int y, int w, int h, int grubosc, bool czarny = true);
  void linia(int x0, int y0, int x1, int y1, bool czarny = true);
  void kolo(int cx, int cy, int r, bool wypelnij, bool czarny = true);
  void trojkat(int x0, int y0, int x1, int y1, int x2, int y2, bool czarny = true);
  void bitmapa(const uint8_t* b, int x, int y, int w, int h);  // bit 1 = czarny
};

// Kroje (generuj.py): S = Source Serif 4, SB = 700, SN = 600 (tylko cyfry), P = Public Sans (P22B = 600, P24B, P28B,
// P34B = 700); liczba = rozmiar w px. Skala B: B1 SN96, B2 S120, B3 SB44, B4 P34B, B5 P28B, B6 P24B,
// B7 P22B (minimum na ekranach dnia, kokpicie, pytaniu, S9 i kartach); SN56 = 4 liczby ostatniej nocy w K1.
extern const GFXfont* const F_S140;  // tylko cyfry i dwukropek (godzina w ekranie nocnym A3)
extern const GFXfont* const F_S120;  // tylko cyfry i kropka (numer serii, S2-seria)
extern const GFXfont* const F_S110;  // tylko znaki napisu `Dobranoc.` (A3 bez godziny)
extern const GFXfont* const F_SN96;  // tylko cyfry, `,`, `h`, `–`, `:` (liczby ostatniej nocy, kamień)
extern const GFXfont* const F_SN56;  // tylko cyfry, `,`, `:`, `–`, `h` (K1)
extern const GFXfont* const F_P34B;
extern const GFXfont* const F_P28B;
extern const GFXfont* const F_P24B;
extern const GFXfont* const F_P24S;  // B6 w wadze 600: wartości wierszy kart „Otwartych spraw” (kanwa X-otwarte-sprawy-*)
extern const GFXfont* const F_SB44;
extern const GFXfont* const F_SB24;
extern const GFXfont* const F_P22B;
// Ekrany serwisowe U-05 „Test sprzętu” i U-01 „Ustawienia” (od 14.11.2026): Public Sans 400 i 600 w 20 px, 400 w 16 px
// (tylko wersaliki, nazwy kolumn), 600 w 18 px (tylko małe litery, cyfry i `·`; od 14.11.2026 nieużywany w ekranach,
// zostaje w krojach). Ekrany serwisowe poza skalą B (minimum 22 px nie dotyczy).
extern const GFXfont* const F_P20;
extern const GFXfont* const F_P20B;
extern const GFXfont* const F_P16;
extern const GFXfont* const F_P18B;

// Szerokość i rysowanie tekstu: pozycje znaków liczone w 1/16 px (szerokości z generuj.py), zaokrąglane dopiero przy
// stawianiu glifu, jak w przeglądarce. odstep = dodatkowy odstęp po każdym znaku w px (letter-spacing etykiet).
int szer(const GFXfont* f, const std::string& utf8, int odstep = 0);
int tekst(Plotno& p, const GFXfont* f, int x, int y, const std::string& utf8, bool czarny = true, int odstep = 0);
void tekstPrawo(Plotno& p, const GFXfont* f, int xPrawy, int y, const std::string& utf8, bool czarny = true, int odstep = 0);
void tekstSrodek(Plotno& p, const GFXfont* f, int xSrodek, int y, const std::string& utf8, bool czarny = true);
// Zawijanie słów na szerokość; twarda spacja (U+00A0) nie łamie wiersza (R-EKR-08).
std::vector<std::string> zawin(const GFXfont* f, const std::string& utf8, int maxW);
// Wersaliki ASCII i polskich liter (tytuł paska S2a, S2c, np. `PRZY ŁÓŻKU · OSTATNIA NOC`).
std::string wersaliki(const std::string& utf8);
// Linia uwagi pod nagłówkiem (5.4.3, 5.4.5, R-EKR-52, R-EKR-62), jedno miejsce rysowania dla wszystkich ekranów danych:
// pasek od y = 54, 48 px (B6, linia bazowa 87), linia wzorca A (liniaA) 60 px (B5, linia bazowa 94). Ostrzeżenie
// (liniaA albo ostrzezenie): pasek w kolorze tekstu ekranu, odwrócony względem tła (na białym czarny, w negatywie
// biały), znak `!` i tekst w kolorze tła. Informacja albo prośba: tło ekranu, ramka 2 px w kolorze tekstu, tekst
// w kolorze tekstu, bez `!`. negatyw = ekran z czarnym tłem (S3, L, S-K w tonie „problem”, S-K-39); x0, x1 = granice
// paska (ekran L w ramce 6 px). Zwraca wysokość paska, 0 bez linii (pusty tekst, nic nie rysuje).
int liniaUwagiEkranu(Plotno& p, const std::string& t, bool liniaA, bool ostrzezenie, bool negatyw, int x0 = 0,
                     int x1 = 800);
// Czy znak (punkt kodowy) ma glif w krojach panelu albo jest ikoną; inaczej panel rysuje `?`. Teksty z zewnątrz
// (opis pogody i miasto ze źródła otoczenia) pomijają takie znaki.
bool znakJestWKroju(uint32_t cp);

// ---------- belka informacyjna (spec 5.4.17, ADR-0043; decyzja właściciela projektu 14.11.2026) ----------
// Stały pas u góry ekranów z danymi i kokpitu, w miejscu dawnego nagłówka. Położenie i funkcja pól są te same na
// każdym ekranie z belką: lewe pole mówi, gdzie jestem (nazwa ekranu, w kokpicie z numerem strony), środek, czego
// dotyczą dane (data nocy, godzina odczytu, zakres oceny), prawe pole pokazuje stan panelu (bateria, przed nią
// krótki tekst stanu). Belka jest jasna na każdym ekranie, także na ekranach w negatywie i na karcie w tonie
// „problem”. Slot dzienny (ekrany S-K-33…S-K-39 i obrazek dnia) oraz ekran nocny S9 belki nie mają.
struct Belka {
  std::string nazwa;   // lewe pole (P24B): `Poranek`, `Ostatnia noc 1 / 3`
  std::string srodek;  // środek (P22B): `pt 30.10`, `pt 13.11 · odczyt 07:05 · ocena ograniczona`; "" = puste
  std::string stan;    // prawe pole przed baterią (P22B) z ` ·`; "" = sama bateria (od 14.11.2026 nieużywane:
                       // `jest aktualizacja ·` z U-01 zastąpił znak nowej wersji, R-EKR-69)
  // Kolejność ustępowania środka: gdy `srodek` nie mieści się przed prawym polem z odstępem BELKA_ODSTEP_PRAWEGO,
  // belka bierze pierwszy tekst z tej listy, który się mieści; gdy żaden, ostatni skrócony do pełnych słów.
  std::vector<std::string> ustepuje;
  // R-EKR-67 (dane nieaktualne): tekst odwróconego pola środka bez `!` (`dane z 13.11`, `brak danych`); "" = bez
  // pola. Pole stoi za `srodek` (biały tekst z `! ` na czarnym), nie ustępuje nigdy; `poPolu` stoi za nim (romb
  // i ocena K1) i też nie ustępuje. Ustawiają je tylko belki ekranów o nocy (belkaDnia(), belkaPortretu(),
  // belkaK1(), belkaK2()) według znakiBelki().
  std::string alarm;
  std::string poPolu;
  Belka() {}
  Belka(const std::string& n, const std::string& s, const std::string& st = "") : nazwa(n), srodek(s), stan(st) {}
};
const int BELKA_WYSOKOSC = 44;        // pas 0…43; treść ekranu zaczyna się najwcześniej na y = 44
const int BELKA_LINIA_Y = 41;         // linia na dole belki: y 41…43
const int BELKA_LINIA_GRUBOSC = 3;
const int BELKA_BAZA = 30;            // linia bazowa tekstu wszystkich pól
const int BELKA_X = 28, BELKA_XP = 772;   // lewy i prawy margines pól (jak treść ekranu)
const int BELKA_ODSTEP_SRODKA = 28;   // od końca nazwy do początku środka
const int BELKA_ODSTEP_PRAWEGO = 16;  // najmniejszy odstęp środka od prawego pola
const int BELKA_ODSTEP_STANU = 8;     // między tekstem stanu (z ` ·`) a ikoną baterii
// Ikona baterii: obrys 34 × 18 px kreską 2 px, „nosek” 3 × 8 px z prawej, w środku do 4 pól wypełnienia.
const int BATERIA_W = 34, BATERIA_H = 18, BATERIA_KRESKA = 2, BATERIA_NOSEK_W = 3, BATERIA_NOSEK_H = 8;
const int BATERIA_ODSTEP = 8;         // między ikoną (z noskiem) a tekstem procentu
const int BATERIA_POLA = 4;           // stopnie wypełnienia: progi BATERIA_PROGI_POL
const int BATERIA_PROGI_POL[BATERIA_POLA] = {13, 38, 63, 88};  // ≥ 13% 1 pole, ≥ 38% 2, ≥ 63% 3, ≥ 88% 4
const int BATERIA_NISKA = 20;         // [PRÓG] poniżej: `podłącz kabel` w stopce slotu dziennego (R-KON-69)
const int BATERIA_NISKA_KONIEC = 25;  // [PRÓG] stan „niska” kończy się od tego poziomu albo na kablu (histereza)

// Stan baterii pokazywany na ekranie. Ustawia go dyrygent przed rysowaniem (wartość z ostatnio wyświetlonej klatki,
// żeby sama zmiana procentu nie odświeżała e-ink, R-EKR-66); bez ustawienia: procent nieznany.
struct BateriaEkranu {
  int procent = -1;    // M-25 (0…100); -1 = nieznany (`–`)
  bool kabel = false;  // panel na zasilaniu zewnętrznym (R-DANE-65): słowo `kabel` zamiast procentu
  bool niska = false;  // stan „niska bateria” (poniżej BATERIA_NISKA, z histerezą): stopka slotu dziennego
};
void ustawBaterieEkranu(const BateriaEkranu& b);
BateriaEkranu bateriaEkranu();
int polaBaterii(int procent);                       // 0…4 pól ikony; procent nieznany = 0
std::string tekstBaterii(const BateriaEkranu& b);   // `78%`, `–` (nieznany) albo `kabel`
// Stan „niska bateria” z histerezą: wchodzi poniżej BATERIA_NISKA, wychodzi od BATERIA_NISKA_KONIEC albo na kablu.
// Procent nieznany (brak pomiaru w tym wybudzeniu) niczego nie zmienia: zostaje stan poprzedni, żeby chwilowy brak
// odczytu nie przełączał stopki i nie odświeżał e-ink.
bool bateriaNiska(int procent, bool kabel, bool bylaNiska);  // procent nieznany: stan się nie zmienia
// Ikona baterii z lewym górnym rogiem obrysu w (x, y); `pola` 0…4; `kabel`: bez pól, pionowa kreska w środku.
// Zwraca x za noskiem. `czarny` = kolor kreski (na jasnym tle true).
int ikonaBaterii(Plotno& p, int x, int y, int pola, bool kabel, bool czarny = true);
int szerPolaBaterii(const BateriaEkranu& b);        // ikona z noskiem, odstęp i tekst
// Ikona i tekst baterii od x w lewo do prawej, tekst P22B na linii bazowej `baza`; zwraca x za tekstem.
int rysujBaterie(Plotno& p, int x, int baza, const BateriaEkranu& b, const std::string& tekst);
// ---------- znaki stanu danych na belce (spec 5.4.17, R-EKR-67…R-EKR-69; decyzja właściciela 14.11.2026) ----------
// „Aby człowiek nie musiał myśleć, czy dane są OK, tylko widział od razu.” Stan pokazany wyjątkiem: przy świeżych
// danych, bez pobierania i bez nowej wersji belka wygląda jak przed 14.11.2026. Ustawia je dyrygent przed
// rysowaniem (Przebieg::pokaz()), jak stan baterii; bez ustawienia: żadnego znaku.
struct ZnakiBelki {
  bool nieaktualne = false;  // R-EKR-67: daneNieaktualne() (src/dzien.h); odwrócone pole na belkach ekranów o nocy
  std::string daneZ;         // `13.11`: data poranka ostatniej zapisanej nocy; "" = żadnej (`brak danych`)
  bool pobieram = false;     // R-EKR-68: panel pobiera noc z AirBridge zaraz po narysowaniu tej klatki (R-PRZ-22)
  bool nowaWersja = false;   // R-EKR-69: warunek ikony aktualizacji (aktualizacja::ikonaAktualizacji())
};
void ustawZnakiBelki(const ZnakiBelki& z);
ZnakiBelki znakiBelki();
// `dane z 13.11` albo `brak danych` (bez nocy w historii): tekst odwróconego pola, bez `!`
std::string tekstNieaktualnych(const ZnakiBelki& z);
// Czy ostatnio narysowana belka miała klepsydrę (R-EKR-68; dyrygent przerysowuje po pobraniu, żeby znak nie
// zostawał na e-inku po końcu pobierania). Zeruje ustawZnakiBelki().
bool klepsydraNarysowana();
bool znakWersjiNarysowany();  // to samo dla znaku nowej wersji (do logu)
// Geometria znaków: odwrócone pole y 5…36 (32 px), tekst P22B na linii bazowej belki, 6 px wcięcia z obu stron;
// klepsydra 18 × 22 px (y 9…30, od linii bazowej 30 w górę wyżej niż wielkie litery P22B; szyjka prosta 4 px),
// za nią `pobieram`; znak nowej wersji: koło o średnicy
// 23 px kreską 2 px ze strzałką w dół (y 10…32), BELKA_ODSTEP_ZNAKU przed ikoną baterii.
const int BELKA_POLE_Y = 5, BELKA_POLE_H = 32, BELKA_POLE_WCIECIE = 6;
const int BELKA_ODSTEP_POLA = 10;        // między tekstem środka a odwróconym polem albo klepsydrą
const int KLEPSYDRA_W = 18, KLEPSYDRA_H = 22, KLEPSYDRA_Y = 9, KLEPSYDRA_ODSTEP = 6;
const int ZNAK_WERSJI_D = 23, ZNAK_WERSJI_Y = 10, BELKA_ODSTEP_ZNAKU = 10;
extern const char* const BELKA_POBIERAM;  // `pobieram`
int szerPolaAlarmu(const std::string& alarm);    // `! {alarm}` z wcięciami; "" = 0
int szerKlepsydry(bool zeSlowem);                // znak (i ` pobieram`)
// Klepsydra z lewym górnym rogiem w (x, y), kolorem `cz`; zwraca x za znakiem.
int rysujKlepsydre(Plotno& p, int x, int y, bool cz = true);
// Znak nowej wersji (koło ze strzałką w dół) z lewym górnym rogiem w (x, y); zwraca x za znakiem.
int rysujZnakWersji(Plotno& p, int x, int y, bool cz = true);
// Środek belki po ustępowaniu, z polem alarmu i klepsydrą: `tekst` (środek z kolejności ustępowania), czy klepsydra
// ma słowo `pobieram`. Kolejność ustępowania przy braku miejsca: najpierw słowo `pobieram` (znak zostaje), potem
// środek według `ustepuje`; pole alarmu, `poPolu` i znak klepsydry nie ustępują nigdy.
struct UkladSrodka {
  std::string tekst;
  bool klepsydra = false, slowo = false;
};
UkladSrodka ukladSrodkaBelki(const Belka& b);
// Czy znak nowej wersji i klepsydra mieszczą się na tej belce. Gdy środek z polem alarmu, rombem i oceną nie
// zostawia miejsca, ustępuje najpierw znak nowej wersji, potem klepsydra (pole, romb i ocena nigdy).
bool znakWersjiNaBelce(const Belka& b);
bool klepsydraNaBelce(const Belka& b);

int xPrawegoPolaBelki(const Belka& b);              // lewa krawędź prawego pola (tekst stanu albo ikona)
int xSrodkaBelki(const Belka& b);                   // początek środka: za nazwą + BELKA_ODSTEP_SRODKA
int miejsceSrodkaBelki(const Belka& b);             // szerokość dla środka (do prawego pola minus odstęp)
std::string srodekBelki(const Belka& b);            // tekst środka po zastosowaniu kolejności ustępowania
// Środek belki ekranów o nocy (ekran dnia, Portret nocy, K1) z jedną kolejnością ustępowania (R-EKR-65):
// pełny `[przed]{dzień} {data} · odczyt {hh:mm} [◆] [· {dopisek}]` → bez dnia tygodnia → bez daty (tylko gdy jest
// godzina i data może ustąpić) → bez godziny → sam romb i dopisek. Romb i dopisek (`ocena …`, `dane z {data}`) są
// w każdym tekście; przy `dataStala` data też (noc nieubiegła: `noc 18.10`).
struct SrodekNocy {
  std::string przed;     // słowa przed datą: `noc na ` (Portret), `noc ` (noc nieubiegła); "" = brak
  std::string dzien;     // skrót dnia tygodnia; "" = bez
  std::string data;      // `13.11`; "" = bez daty (brak nocy)
  std::string odczyt;    // godzina importu nocy `07:05`; "" = nieznana albo nie dotyczy
  bool romb = false;     // R-KOK-14
  std::string dopisek;   // `ocena częściowa`, `ocena ograniczona`, `dane z 18.10`; "" = brak
  bool dataStala = false;  // data nie ustępuje
};
void ustawSrodekNocy(Belka& b, const SrodekNocy& s);
// R-EKR-67: przy znakiBelki().nieaktualne przebudowuje środek belki ekranu o nocy: data dnia (ustępuje dzień
// tygodnia, potem data), odwrócone pole `! dane z {data}` (albo `! brak danych`) i za nim romb i ocena K1;
// `dane z …` ekranu dnia i `noc {data}` K1 przechodzą do pola. Przy danych aktualnych nic nie zmienia.
void ustawAlarmNocy(Belka& b, const SrodekNocy& s);
// Jedyne miejsce rysowania belki: czyści pas 0…43 na biało, rysuje pola i linię. Stan baterii z ustawBaterieEkranu().
void belka(Plotno& p, const Belka& b);
// Nazwy ekranów w lewym polu belki (jedno miejsce; numer strony kokpitu dopisuje nazwaStronyKokpitu()).
extern const char* const BELKA_PORANEK;    // `Poranek`: ekran dnia (ekran danych i karta)
extern const char* const BELKA_WIECZOR;    // `Wieczór`: ekrany slotu wieczornego
extern const char* const BELKA_PYTANIE;    // `Pytanie`
std::string nazwaStronyKokpitu(const std::string& nazwa, const std::string& numer);  // `Ostatnia noc 1 / 3`
// Stopka slotu dziennego przy niskiej baterii (R-KON-69): tekst za ikoną, np. `15% · podłącz kabel`.
std::string tekstNiskiejBaterii(const BateriaEkranu& b);

// ---------- treść ekranów ----------

struct Fakt {
  std::string w, p;  // liczba (P28B) nad podpisem (P22B)
};

// Kolumna paska `OSTATNIE 7 NOCY · MEDIANA` (S2a) i `SYPIALNIA OSTATNIEJ NOCY` (S2c), SPEC-B 4.
struct Kolumna {
  std::string et, w, p;  // nazwa kolumny, wartość, słowo pod wartością (`lepiej niż zwykle`, `uczę się`; przy `jak
                         // zwykle` puste: mówi legenda `↔ jak zwykle`)
  std::string s;         // strzałka za wartością: `↔`, `↑`, `↓` albo "" (bez strzałki: brak wartości, `uczę się`)
  bool poza = false;     // `wyżej/niżej niż zwykle` (S2c: poza pasmem): kolumna w ramce 2 px, legenda panelu „ramka =
                         // poza twoją normą” (czarne pole tylko „ponad próg”: komórki K2 i kolumna
                         // CO2 S2c, R-EKR-54; czarny pasek linii zdrowia to osobny element)
  bool ponadProg = false;  // wartość ponad stały próg (S2c: CO2 nocy ≥ 1000 ppm): czarne pole, tekst biały (jedna
                           // legenda: czarne pole = ponad próg, jak w K2)
};

// Układ ekranu danych (5.4.3, skala B według docs/design/czytelnosc-b/SPEC-B.md, wariant B).
enum class Uklad : unsigned char {
  OGOLNY,   // szablon A: etykieta, tytuł B3 albo duża liczba B1, powód B5, fakt, krok (S5, kamień milowy)
  TYDZIEN,  // S2a i S2c: blok liczb nocy B1, zdanie B4, pasek 4 kolumn z ramką przy kolumnie poza normą
  SERIA,    // S2b: duży numer, `noc z rzędu` / `ponad 4 h` B4, 30 kropek
  ROZRUCH,  // S6: jak OGOLNY z 14 polami postępu
  PORTRET,  // S11 Portret nocy (5.4.16): tarcza i kolumna z src/portret_ekran.h (TrescEkranu::portret)
};
struct DanePortretu;  // src/portret_ekran.h

struct TrescEkranu {
  Uklad uklad = Uklad::OGOLNY;
  std::string etykieta, tytul, powod, krok;  // tytul: karta i S5/S6; TYDZIEN: tylko karta (`Noc: …`, 5.4.2)
  // TYDZIEN: blok liczb ostatniej nocy (podpis B6 nad liczbą B1) i zdanie (powód 5.4.2, przy `W normie` z przedrostkiem)
  std::string podpisNocy;         // `Ostatnia noc` albo `Noc {data}` (gdy ostatnia zapisana noc nie jest ubiegłą)
  bool wNormie = false;           // TYDZIEN: przedrostek `W normie. ` przed zdaniem, tylko na ekranie danych (karta K22
                                  // ma `W normie.` w tytule)
  std::string powodKarty;         // powód na karcie, gdy inny niż na ekranie danych (K22: na karcie nie ma paska `niżej`)
  std::string czas, ahi;          // `9 h 09`, `0,1`; brak = `–` (R-DANE-07)
  bool ahiNiepewne = false;       // noc niepewna (4.9, R-EKR-46): w miejscu AHI `nie oceniam:` / `duży wyciek`
  std::string tytulPaska;         // `Ostatnie 7 nocy · mediana` / `Przy łóżku · ostatnia noc` (wersalikami na ekranie)
  std::string duza, podpisDuzej;  // OGOLNY: duża liczba B1 z podpisem B4 zamiast tytułu (kamień milowy, 5.8.3)
  std::string fakt;               // OGOLNY: linia faktu B7 pod powodem (S5 przy E-03: `ostatnie dane: …`)
  std::string etykietaKarty;      // etykieta na karcie, gdy inna niż na ekranie danych (K22: `OSTATNIA NOC`)
  bool kartaBezEtykiety = false;  // S2d (ADR-0009): żaden napis nad tytułem; tytuł wyżej, a gdy
                                  // pierwsze zdanie nie mieści się w 2 liniach SB44, tytuł w P34B (2 linie); powód do 3 linii
  std::vector<Fakt> fakty;        // karta: rząd faktów; szablon A: fakty pod powodem
  std::vector<Kolumna> kolumny;   // TYDZIEN: 4 kolumny
  int seria = 0;                  // SERIA: numer nocy z rzędu
  bool kropka[30] = {};           // SERIA: pełna = noc ≥ 4 h
  int postep = -1;                // ROZRUCH: noc {n} z 14
  std::string postepLewo, postepPrawo;
  bool ramkaPrzerywana = false;   // ton „brak danych” (S5)
  std::string linia;              // linia uwagi (tylko ekran danych, R-HUM-17)
  bool liniaA = false;            // linia wzorca A: 60 px i (28 px) zamiast 48 px i B6; zawsze ostrzeżenie
  // Wygląd linii (5.4.5): ostrzeżenie (grupa „zdrowie”, 7.5) = czarny pasek z `!`; informacja
  // i prośba (np. `Co było inaczej? ↻ 2 s`, awarie, dane, noc) = biały pasek w ramce 2 px, bez `!`.
  bool liniaOstrzezenie = false;
  std::string stopkaLewo, stopkaPrawo;
  // Stopka standardowa ekranu dnia (R-EKR-10): pogoda i `przy łóżku` zamiast akcji (S5 `↻ ponów`, S10 `↻ pobierz
  // teraz`, ekran nocny `▶ kokpit`); ustawia src/dzien.h trescDnia(). Tylko taki ekran dnia przerysowuje R-EKR-60.
  bool stopkaStandardowa = false;
  // Karta w tonie „problem” (S-K-10, S-K-16, S-K-31; R-HUM-24, EkranDnia::tonProblem z ekranKarty()): jasne tło,
  // czarny pasek nagłówka, etykieta w czarnym polu; budzik zawsze na bieli (ADR-0009). Ekran danych bez zmian.
  bool tonProblem = false;
  const DanePortretu* portret = nullptr;  // Uklad::PORTRET: dane rysunku (musi żyć do końca rysowania)
};

// R-HUM-05: tytuł ≤ 2 linie na 748 px, powód ≤ 2 linie i krok 1 linia w kolumnie tekstu karty (SB44, P22B).
bool kartaMiesci(const TrescEkranu& t);
// Scena z include/obrazy.h; nullptr, gdy jej nie ma (R-HUM-11).
bool scenaJest(const std::string& id);
std::vector<std::string> sceny();

bool kartaPrzerywnikaMiesci(const TrescEkranu& t);  // S2d: tytuł do 2 linii (SB44, albo P34B), powód do 3 linii
// Karta z budzikiem (ADR-0009, 5.7.2). Od 10.11.2026 bez zdania budzika w stopce: lewo t.stopkaLewo (jak na ekranie
// danych), prawo t.stopkaPrawo; stopka znika, gdy obie są puste.
void rysujKarte(Plotno& p, const Belka& n, const TrescEkranu& t, const std::string& scena);
void rysujEkranDanych(Plotno& p, const Belka& n, const TrescEkranu& t);
void rysujK1(Plotno& p, const DaneK1& d);
// Ocena wiersza K1 tak, jak stoi na ekranie (5.5.2, R-KOK-17 od 13.11.2026): w każdym wierszu znak i słowo
// (`↔ stabilnie`, `↑ rośnie`, `↓ spada`, `↑ wyżej niż zwykle`, `↓ niżej niż zwykle`, `… uczę się`), po słowie trendu
// w wierszach `AHI` i `Wyciek` dopisek ` · lepiej` / ` · gorzej` (R-KOK-25). Bez legendy w nagłówku kolumn.
std::string znakOcenyK1(const std::string& slowo);   // "" dla słowa pustego albo nieznanego
std::string tekstOcenyK1(const WierszK1& w);          // pierwsza linia oceny: znak, słowo, dopisek
// Ocena w jednej linii (K1 z pasem odkrycia, wiersz < 46 px) w `maxW` px: po kolei pełna, bez dopisku, przy
// `stabilnie` sam znak przed dopiskiem (`↔ wyciek widoczny`), bez znaku, a przy stanie odchylenia od wielu nocy
// krótsze słowo ze znakiem (`↑ wyżej od 120 nocy`).
std::string ocenaK1JednaLinia(const WierszK1& w, int maxW);
// Geometria tabeli „Ostatnie 30 nocy” w K1 (R-KOK-28): linia 30 nocy od K1_X_LINII, mediana do prawej na xm, ocena
// od xm + K1_ODSTEP_OCENY do prawego marginesu 772. Zwykły K1: ocena w 2 liniach; z pasem odkrycia: w jednej, dłuższej.
const int K1_X_LINII = 196, K1_ODSTEP_OCENY = 16;
const int K1_XM = 554, K1_WL = 270;                 // zwykły K1: ocena od 570 (202 px)
const int K1_XM_ODKRYCIE = 470, K1_WL_ODKRYCIE = 174;  // pas odkrycia: ocena od 486 (286 px)
void rysujK2(Plotno& p, const DaneK2& d);
// R-KOK-18: które wiersze K2 rysować jako jedną linię pod tabelą. Przyjmuje dane K2 i tablicę `zwin` na
// DaneK2::WIERSZE pozycji; ustawia zwin[r] dla wierszy `pusty` w kolejności tabeli, dopóki linia
// `brak danych: {nazwy}` mieści się w 744 px (22 px, 600); zwraca tę linię albo "" (nic do zwinięcia).
// Wiersz, który się nie zmieścił, zostaje w tabeli z kreskami.
std::string zwinieteK2(const DaneK2& d, bool zwin[DaneK2::WIERSZE]);

struct DanePytania {
  std::string dataPoranka;  // RRRR-MM-DD
  bool innaNoc = false;     // zaproszenie po odchyleniu: etykieta `Inna noc`
  std::string opisNocy;     // `AHI 6,8 przy zwykłych 0,8–2,1` albo `6:54 · AHI 1,2`
  int kursor = 7;
  uint8_t zaznaczone = 0;
  int termin = -1;          // minuty; -1 = pytanie na żądanie (bez godziny znikania)
};
void rysujPytanie(Plotno& p, const DanePytania& d);

// ---------- U-05 „Test sprzętu” (makieta 14.11.2026, decyzja 4; R-KONF-41, R-KONF-58) ----------
// Belka `Test sprzętu` ze środkiem `sprawdzono {DD.MM hh:mm}`; pod nią pas werdyktu na całą szerokość (werdyktTestu():
// `! {n} rzecz(y) do sprawdzenia: {lista}` w negatywie, `Bez usterek · nie sprawdzono: {lista}` albo `Wszystko działa`
// w ramce 2 px), dwie kolumny po najwyżej 5 wierszy (`PANEL`, `ŁĄCZNOŚĆ`) i pod nimi wiersz `dol` na całą szerokość
// (744 px, `Noce z plików` z pełnym tekstem jak do 14.11.2026, przegląd); bez prawej kolumny (tryb
// awaryjny) lewa na całą szerokość, do 6 wierszy. Nazwa P20 z lewej, wartość P20B z prawej, kreska 1 px pod wierszem;
// wiersz z usterką ma wartość w odwróconym polu ze znakiem `!`. Ramka 2 px na dole (okno serwisowe z lewej, `hasła
// i dane terapii nie są pokazywane` z prawej) i stopka tymi samymi znakami co pozostałe ekrany (stopkaEkranu()). Treść
// wierszy, to, co jest usterką, i to, czego nie dało się ocenić, składa dyrygent (src/dyrygent.cpp
// Przebieg::daneTestu(), T-WYM-39, spec 5.6.3 U-05).
struct WierszTestu {
  std::string nazwa, wartosc;
  bool usterka = false;  // R-KONF-58: wartość w odwróconym polu z `!`, nazwa `krotko` w werdykcie
  std::string krotko;    // nazwa w werdykcie (`AirBridge`, `pogoda`); "" = nazwa wiersza małą literą
  bool nieSprawdzono = false;  // R-KONF-58, ADR-0029 pkt 7: stanu nie da się ocenić (`brak odczytu`, `–`); bez usterki
                               // werdykt `Bez usterek · nie sprawdzono: {krotko}`, nigdy `Wszystko działa`
  WierszTestu() {}
  WierszTestu(const std::string& n, const std::string& w, bool u = false, const std::string& k = "")
      : nazwa(n), wartosc(w), usterka(u), krotko(k) {}
  // Wiersz bez oceny (nieSprawdzono = true, bez usterki).
  static WierszTestu bezOceny(const std::string& n, const std::string& w, const std::string& k = "") {
    WierszTestu x(n, w, false, k);
    x.nieSprawdzono = true;
    return x;
  }
};
struct DaneTestu {
  std::vector<WierszTestu> lewa, prawa;  // `PANEL` i `ŁĄCZNOŚĆ`; pusta kolumna = bez nazwy kolumny (tryb awaryjny)
  std::vector<WierszTestu> dol;          // wiersz na całą szerokość pod kolumnami (najwyżej 1); pusty w trybie awaryjnym
  std::string sprawdzono;                // `DD.MM hh:mm` do belki; "" = bez zegara (środek belki pusty)
  std::string ramkaLewo, ramkaPrawo;
  bool awaryjny = false;                 // U-05 z U-06 (R-AW-39): stopka tylko `◀ wróć` (spec funkcjonalna 5.6.2)
};
const int TEST_WIERSZY = 6;       // najwyżej tyle wierszy w kolumnie bez wiersza `dol` (z nim o jeden mniej)
const int TEST_KOLUMNA = 364;     // szerokość kolumny: (744 − 16) / 2 (od adres z godziną mieści się)
const int TEST_ODSTEP_KOLUMN = 16;  // odstęp między kolumnami
const int TEST_SZEROKOSC = 744;   // wiersz `dol` pod kolumnami: od 28 do 772
const int TEST_ODSTEP = 12;       // najmniejszy odstęp nazwy od wartości
const int TEST_USTERKA_DODATEK = 2 * 4;  // odwrócone pole wartości: po 4 px z obu stron tekstu `! …`
// Szerokość, którą ma wartość obok nazwy w kolumnie: szerokosc − nazwa (P20) − TEST_ODSTEP.
int miejsceNaWartosc(const std::string& nazwa, int szerokosc = TEST_KOLUMNA);
// Pierwszy wariant wartości, który mieści się obok nazwy (P20B); gdy żaden, ostatni skrócony z `…` na końcu.
// `usterka`: miejsce pomniejszone o `! ` i brzegi odwróconego pola (wartość wiersza z usterką).
std::string dopasujWartosc(const std::string& nazwa, const std::vector<std::string>& warianty,
                           int szerokosc = TEST_KOLUMNA, bool usterka = false);
// Wartość `{poczatek skrócony…}{ogon}`: najdłuższy początek (całe znaki UTF-8) z `…`, przy którym całość mieści się obok
// nazwy jak w dopasujWartosc(); ogon (`· słaby`) zostaje zawsze (słowo usterki nie znika). Gdy nie mieści się
// nawet `…{ogon}`: "".
std::string dopasujZOgonem(const std::string& nazwa, const std::string& poczatek, const std::string& ogon,
                           int szerokosc = TEST_KOLUMNA, bool usterka = false);
// ---- Wiersze U-05 z danych: jedno miejsce wariantów wartości, wołają je dyrygent (Przebieg::daneTestu())
// i podgląd ekranów (test/podglad.cpp). Warianty od najpełniejszego; dopasujWartosc() bierze pierwszy, który się mieści.
// Wiersz z wartością dopasowaną do kolumny `szerokosc` (z usterką: miejsce na `! ` i odwrócone pole).
WierszTestu wierszU05(const std::string& nazwa, const std::vector<std::string>& warianty, bool usterka = false,
                      const std::string& krotko = "", bool bezOceny = false, int szerokosc = TEST_KOLUMNA);
// `Wi-Fi`: ssid "" = panel nigdy nie połączył się z siecią (`brak połączenia`, usterka); rssi 0 = bez pomiaru;
// `kiedy` = ` (hh:mm)` przy ostatnim połączeniu bez połączenia teraz, "" przy połączeniu. Usterka: sygnał słaby.
WierszTestu wierszU05Wifi(const std::string& ssid, int rssi, const std::string& kiedy);
// `Adres panelu`: ip "" = `–`; `ddmm` "" = połączenie teraz (sam adres); inaczej `{ip} ({DD.MM hh:mm})`, dziś
// `{ip} ({hh:mm})`, z innego dnia `{ip} ({DD.MM})`, w końcu `{ip}` (godzina nigdy ucięta).
WierszTestu wierszU05Adresu(const std::string& ip, const std::string& ddmm, const std::string& hhmm, bool dzis);
// `Bateria`: na kablu `na zasilaniu · {procent}%` albo `na zasilaniu` bez pomiaru; bez kabla i pomiaru
// `brak odczytu` (bez oceny); inaczej `{procent}%`, usterka poniżej `niska`. procent < 0 = brak pomiaru.
WierszTestu wierszU05Baterii(bool kabel, int procent, int niska);
// Warianty błędu `AirBridge` dla kodu odpowiedzi: 404, 5xx, inny kod HTTP ≠ 200, odpowiedź urwana, za duża,
// nieczytelna (`błąd AirBridge…`); pusta lista = kod nie jest takim błędem (200, 401, brak odpowiedzi, bez zapytania).
std::vector<std::string> bladAirBridgeU05(int kod);
// `Ostatnie pobranie`: `nowsza` = AirBridge ma noc nowszą niż pobrana, `kontaktStary` = ostatni kontakt
// starszy niż POBRANIE_STARE_H; obie to usterka; `bezOceny` bez usterki = wiersz bez oceny. `pobieram` (od 14.11.2026,
//): AirBridge ma doby gotowe do pobrania, żadna nie czeka po nieudanej próbie,
// kontakt świeży → `{pobrano} · pobieram` albo `pobieram`, bez usterki i z oceną (nie wchodzi do pasa werdyktu);
// `nowsza` i `kontaktStary` mają pierwszeństwo.
WierszTestu wierszU05Pobrania(const std::string& pobrano, bool nowsza, bool kontaktStary, bool bezOceny,
                              bool pobieram = false);
// Warianty `Oprogramowanie` z dopiskiem aktualizacji (` · jest wersja {X}`, ` · odrzucona {X}`).
std::vector<std::string> wariantyOprogramowaniaU05(const std::string& wersja, const std::string& dopisek);
// Warianty `Pamięć panelu` przy danych z nowszej wersji (`co` = plik albo `NVS {klucz}`).
std::vector<std::string> wariantyNowszychDanychU05(const std::string& co);

// Werdykt U-05 (R-KONF-58, ADR-0029 pkt 7), wiersze w kolejności lewa, prawa, dol:
//  - z usterką: `{n} rzecz do sprawdzenia: {a}` (n = 1) albo `{n} rzeczy do sprawdzenia: {a}, {b}` (nazwy `krotko`);
//    usterki mają pierwszeństwo przed wierszami bez oceny;
//  - bez usterek, z wierszem bez oceny: `Bez usterek · nie sprawdzono: {a}, {b}`;
//  - `Wszystko działa` tylko wtedy, gdy każdy wiersz jest oceniony i dobry.
// Lista, która nie mieści się w pasie (P24B, 744 − 2 · 14 px, z `! ` przy usterkach), traci nazwy od końca na rzecz `…`.
std::string werdyktTestu(const DaneTestu& d);
int usterkiTestu(const DaneTestu& d);
void rysujTest(Plotno& p, const DaneTestu& d);
// Siła sygnału Wi-Fi słowem (U-05, decyzja 4 z 14.11.2026): od SYGNAL_DOBRY `dobry`, od SYGNAL_SLABY + 1 `średni`,
// niżej `słaby` (usterka wiersza Wi-Fi, R-KONF-58); 0 albo dodatnia (brak pomiaru) "".
const int SYGNAL_DOBRY = -67;  // [PRÓG] dBm
const int SYGNAL_SLABY = -80;  // [PRÓG] dBm: od tej wartości w dół `słaby`
// Wiersz `Ostatnie pobranie` jest usterką, gdy ostatnie udane pobranie nocy jest starsze niż tyle godzin (R-KONF-58).
const int POBRANIE_STARE_H = 48;  // [PRÓG]
std::string slowoSygnalu(int dbm);

// Stopka z akcjami (SPEC-KOKPIT 2.1): jedna grupa na przycisk w kolejności ◀ ↻ ▶ (`◀ ostatnia noc · 2 s ustawienia`),
// odstęp 26 px, P22B na linii bazowej 465; bez zdania budzika grupy od lewego marginesu, ze zdaniem wyrównane do prawej.
// Podpisy tylko akcji, które prototyp ma (nawigacja.h); te grupy są też w testach szerokości (≤ 744 px).
void uporzadkujPrzyciski(std::vector<std::string>& grupy);  // ◀ ▶ ↻ jak przyciski na obudowie
// wstecz "" = podpis strony przed K1 w ciągu (domyślnie `◀ ekran dnia`; z Portretu `◀ portret nocy`, R-POR-24);
// sprawy, liczby, sypialnia: stan ciągu jak w KontekstNaw (przy innej kolejności stron sąsiedzi K1 zależą od nich)
std::vector<std::string> stopkaK1(bool jestNoc, const std::string& wstecz = "", bool sprawy = false, bool liczby = false,
                                  bool sypialnia = sypialniaWKokpicie());
// 5.5.7 „Stopki stron”; `sypialnia`: strona „Sypialnia · 30 nocy” w ciągu (R-KOK-29), bez spraw i profilu `▶ sypialnia`;
// `wstecz` "" = podpis strony przed K2 w ciągu; inaczej podpis ◀ (K2 pierwszą stroną otwartą z Portretu: `◀ portret
// nocy`, R-POR-24)
std::vector<std::string> stopkaK2(bool sprawy = true, bool liczby = false, bool sypialnia = sypialniaWKokpicie(),
                                  const std::string& wstecz = "");
// Belka K1 (R-KOK-04, R-KOK-14, R-KOK-21, R-KOK-26, R-EKR-65): nazwa `Ostatnia noc 1 / 3`, środek
// `pt 13.11 · odczyt 07:05[ ◆][ · ocena ograniczona]`, przy nocy nieubiegłej `noc 18.10…`; ustępują kolejno dzień
// tygodnia, data (przy nocy ubiegłej wynika z nazwy strony) i dopiero na końcu `odczyt GG:MM`, nigdy `ocena …`
// (ADR-0029 pkt 7), romb ani `noc {data}`.
Belka belkaK1(const DaneK1& d);
// Belka K2 (R-KOK-26): nazwa `7 nocy 2 / 3`, środek `6–12.11 · odczyt 05:46`; ustępuje `odczyt GG:MM`.
Belka belkaK2(const DaneK2& d);
// R-KOK-24: linia kroku pasma AHI w K2 o tyle niżej niż linia bazowa legendy, najniżej na linii bazowej 425 (dolny
// wydłużnik P22B 6 px nad kreską stopki 434); niżej linia nie jest rysowana (DaneK2::krokPasma tylko poza ASV)
const int K2_KROK_ODSTEP = 26, K2_KROK_BAZA_MAKS = 425;
// R-KOK-24 (wariant c: najmniejszy odstęp krótkiego kroku w linii legendy od końca `poza twoją normą` (jak między grupami)
const int K2_KROK_ODSTEP_LEGENDY = 26;
std::vector<std::string> stopkaPytania();
// U-05: `◀ ustawienia · 2 s sieć panelu`, `▶ odśwież`, `↻ ekran dnia`; w trybie awaryjnym tylko `◀ wróć`.
std::vector<std::string> stopkaTestu(bool awaryjny = false);
int szerStopki(const std::vector<std::string>& grupy);
// Pasek S2a/S2c (SPEC-B 4, reguła ustępowania 2): słowo pod wartością zostaje w każdej
// kolumnie, która nie jest `jak zwykle` (wtedy słowa i tak nie ma, mówi legenda). Gdy pasek ze słowami (142 px) nie
// mieści się między zdaniem a dołem strefy treści (linia uwagi przy obecnej stopce), pasek się zagęszcza (128 px);
// tylko bez żadnego słowa pasek traci rząd słów (114 px). Zwraca true, gdy pasek ma rząd słów.
bool slowaWPasku(const TrescEkranu& t);
// Wysokość paska: 142 (pełny), 128 (zagęszczony, ze słowami) albo 114 (bez rzędu słów).
int wysokoscPaska(const TrescEkranu& t);
// Górna krawędź paska S2a/S2c (linia 2 px); pasek jest przyklejony do dołu strefy treści (reguła 1).
int gornaPaska(const TrescEkranu& t);

// Ekran nocny „Dobranoc” (src/dobranoc.h): warianty A1, A2, A3; sceny z osobnej tabeli SCENY_NOCY.
bool scenaNocyJest(const std::string& id);
void rysujDobranoc(Plotno& p, const DaneNocy& d);

// ---------- ekran wzorca L, potwierdzenie „Omówione”, „Otwarte sprawy”, pytania tak/nie ----------
// Wszystko w skali B (SPEC-B 2, 3, 5.4.3: nagłówek B6, etykieta B7 wersalikami, tytuł B3, powód i krok B5, stopka B7).

// Ekran wzorca L (5.12.5, artboardy w skali B L03-bezdechy, L08-regularnosc, L09-wyciek): negatyw; ton „lekarz”
// z białą ramką 6 px (4…10 px od krawędzi) i znakiem ostrzeżenia przed tytułem, ton „problem” (L-09) bez ramki
// i bez znaku. Od góry: nagłówek (kreska na 52), etykieta pilności (5.12.2), tytuł B3, powód (≤ 2 linie),
// wizualizacja parametru kluczowego z 14 nocy (5.13.4) z podpisami osi B7, ramka hipotezy (5.12.9, gdy jest), przy
// pilności A zdanie R-LEK-28, krok B5 na 418, stopka z akcjami niestandardowymi (R-PRZ-04) na 434.
enum class WizWzorca : unsigned char { BRAK, LINIA, RZEDY };
struct DaneWzorca {
  Belka naglowek;
  bool lekarz = true;
  std::string etykieta, tytul, powod, krok;
  bool pilnoscA = false;
  std::string hipoteza;  // linia ramki H; "" = bez ramki
  WizWzorca wiz = WizWzorca::BRAK;
  std::vector<float> wartosci;  // LINIA: 14 nocy od najstarszej, NAN = noc bez wartości (przerwa w linii)
  float prog = NAN;             // LINIA: linia przerywana; NAN = bez
  bool rzad1[30] = {}, rzad2[30] = {};  // RZEDY (L-08): pełne kółko = noc ponad 4 h z maską
  std::string etRzad1, etRzad2;         // RZEDY: nazwy rzędów
  std::string podpisLewo, podpisPrawo;  // podpisy osi (5.12.5, „Podpisy osi wizualizacji”)
  // ekran „problem” (S3 zdarzeń E, L-09) jest ekranem danych dnia, więc ma strefę linii uwagi pod nagłówkiem
  // (5.4.3, 5.4.5): ostrzeżenie (grupa „zdrowie”) na negatywie jako biały pasek z `!` i czarnym tekstem, informacja
  // jako biała ramka 2 px z białym tekstem; linia wzorca A 60 px i B5. Ton „lekarz” (kanwa L01…L12) bez strefy linii, poza
  // jednym wyjątkiem: E-05 krytyczne w dniu pełnego ekranu wzorca A (R-PIE-07) jako ostrzeżenie w ramce ekranu.
  std::string linia;
  bool liniaA = false, liniaOstrzezenie = false;
  // podpis ▶ w stopce, gdy ▶ nie prowadzi do kokpitu: rano ▶ otwiera stronę Portretu (R-POR-24) → `▶ portret nocy`
  // (tekst `S11.stopka_dalej`); "" = `▶ kokpit`
  std::string stopkaDalej;
};
const int WZ_X = 28;                           // lewy margines treści (kanwa L03: 28 px)
int szerTytuluWzorca(bool lekarz);             // miejsce na tytuł w jednej linii (SB44), za znakiem ostrzeżenia
int szerTresciWzorca(bool lekarz);             // szerokość kolumny tekstu (744)
extern const char* const ZDANIE_R_LEK_28;      // `Duszność albo ból w klatce? Wezwij pomoc.`
std::vector<std::string> stopkaWzorca(bool lekarz, const std::string& dalej = "");  // dalej "" = `▶ kokpit` (R-POR-24)
void rysujWzorzec(Plotno& p, const DaneWzorca& d);

// „Raport dla lekarza” (5.13.6, ADR-0005) w stanie R-LEK-05, dopóki panel nie ma modułu raportu A4 (moduł w wersji pełnej):
// `Raport dla lekarza` · temat (tytuł wzorca) · `Nie udało się przygotować raportu ({kod})`, stopka
// `◀ wróć` · `▶ dalej` · `↻ ekran dnia`; bez kodu QR, bez Wi-Fi i bez serwera (nic nie udaje gotowego raportu).
struct DaneRaportu {
  Belka naglowek;
  std::string temat, kod;
};
const char* const KOD_RAPORTU_BRAK = "R0";  // moduł raportu jeszcze nie istnieje
void rysujRaport(Plotno& p, const DaneRaportu& d);

// Ekran „Przeniesione” po „wiem” (5.4.9, R-PRZ-15, artboard P-przeniesione): tytuł `Przeniesione na {d} dni.`, powód
// `Do tego czasu przypomnę na pasku u góry: {tytuł}.`, stopka `◀ 2 s cofnij` (w nocy po 30 s bez podpisu, R-PRZ-19).
// Ekran „Zapisano” po odpowiedzi tak/nie (R-PRZ-10, artboard P-zapisano): nad tytułem treść pytania B5, tytuł
// `Zapisano: tak.` albo `Zapisano: nie.`, stopka `◀ 2 s cofnij`.
struct DanePotwierdzenia {
  Belka naglowek;
  std::string nad, tytul, powod;
  std::vector<std::string> stopka;  // pusta = bez stopki
};
void rysujPotwierdzenie(Plotno& p, const DanePotwierdzenia& d);

// Potwierdzenie „Omówione z lekarzem?” (5.12.7, mapa 5.6.2, artboard Q-omowione w skali B): ekran danych w tonie
// „spokój”, nad tytułem nazwa wzorca, trzy zdania, pola `◀ nie, wróć` i `▶ tak, omówione`, stopka z ↻ anuluj.
struct DaneOmowione {
  Belka naglowek;   // `{dzień} {data} · po wizycie` / `przytrzymano ◀ 2 s`
  std::string nazwa;   // tytuł ekranu wzorca
  std::vector<std::string> zdania;
  bool blokada = false;  // R-LEK-26: jedno zdanie, w strefie przycisków tylko `◀ wróć` (bez ▶)
};
std::vector<std::string> zdaniaOmowione(bool pilnoscA);
// zdania wariantu z 5.12.7 (wzorce::WariantOmowienia jako liczba, żeby ekrany nie zależały od wzorce.h):
// 1 ZWYKLE, 2 A_PIERWSZE, 3 A_DRUGIE_BEZ_POPRAWY, 4 A_KOLEJNE_BEZ_POPRAWY, 5 NAJPIERW_RAPORT (jedno zdanie, blokada).
std::vector<std::string> zdaniaOmowienia(int wariant);
void rysujOmowione(Plotno& p, const DaneOmowione& d);

// „Otwarte sprawy” (5.4.15, R-EKR-49, R-EKR-50, artboardy X-otwarte-sprawy*): nagłówek `Otwarte sprawy · {dzień}
// {data}`, etykieta `Sprawa {i} z {n}`, tytuł B3, powód B5, kreska, cztery wiersze (nazwa z lewej, wartość od 140),
// stopka `◀ wstecz` (na pierwszej `◀ wróć`) · `↻ ekran dnia` · `▶ dalej` (bez na ostatniej). Stan pusty: tylko `Ocena:`.
struct DaneSpraw {
  Belka naglowek;  // `Otwarte sprawy` i data (5.4.15)
  int i = 0, n = 0;  // i od 1; n = 0: stan pusty
  std::string tytul, powod;
  std::string ocena, ty, dalej, koniec;
  std::string dopisek;  // stan pusty, wiersz pod `Ocena:` (R-EKR-50, ocena częściowa)
  bool liczby = false;  // profil publiczny: przed pierwszą kartą „Co znaczą te liczby” (`◀ liczby`, R-KOK-22)
};
const int SPRAWY_X_WARTOSCI = 140;
std::vector<std::string> stopkaSpraw(int i, int n, bool liczby = false, bool sypialnia = sypialniaWKokpicie());  // za ostatnią kartą `▶ sypialnia` (R-KOK-29)
void rysujSprawy(Plotno& p, const DaneSpraw& d);

// R-EKR-61 (decyzja właściciela projektu 09.11.2026): ikona aktualizacji przy prawym marginesie
// stopki ekranu dnia, gdy panel zna wersję nowszą od własnej. Dyrygent włącza ją na czas rysowania ekranu dnia
// (IkonaStopki) i wyłącza przed S9, obrazkiem dnia i ekranem L z pilnością A albo B; stopki ekranu dnia kończą
// treść wyrównaną do prawej na prawyBrzegStopki(), a na końcu rysowania dyrygent kładzie ikonę (ikonaAktualizacji()),
// która czyści pas stopki od lewej krawędzi ikony minus odstęp: za długa treść stopki skraca się, ikona nie.
const int BOK_IKONY_AKTUALIZACJI = 24;  // jak ikony przycisków w stopce (StylStopki: min. 24 px)
const int ODSTEP_IKONY_AKTUALIZACJI = 14;
void ustawIkoneAktualizacji(bool wl);
bool ikonaAktualizacjiWlaczona();
int prawyBrzegStopki();  // 772 albo 772 − bok − odstęp przy włączonej ikonie
// tekst skrócony do pełnych słów (od końca), tak żeby miał najwyżej `maxW` px w kroju `f`; "" gdy nie mieści się
// nawet pierwsze słowo. Stopki ekranu dnia przy włączonej ikonie skracają tak treść, zanim ikona ją przykryje.
std::string skrocDoSlow(const GFXfont* f, const std::string& t, int maxW);
// Ikona: kwadrat w negatywie ze strzałką w dół nad kreską, dolna krawędź na linii bazowej stopki (465). `tloCzarne`:
// stopka w negatywie (ikona w kolorach odwróconych). Rysuje tylko przy włączonej ikonie; zwraca true, gdy narysowała.
bool ikonaAktualizacji(Plotno& p, bool tloCzarne);
struct IkonaStopki {  // RAII: ikona włączona na czas rysowania ekranu dnia
  explicit IkonaStopki(bool wl) { ustawIkoneAktualizacji(wl); }
  ~IkonaStopki() { ustawIkoneAktualizacji(false); }
  IkonaStopki(const IkonaStopki&) = delete;
  IkonaStopki& operator=(const IkonaStopki&) = delete;
};

// Wspólna stopka ekranów danych rysowanych poza ekrany.cpp (U-01, U-07): kreska 2 px na 434, P22B na 465, grupy akcji
// od lewego marginesu co 26 px, `prawo` do prawego marginesu, ikony przycisków w stylu stopki. Nagłówkiem tych ekranów
// jest belka().
void stopkaEkranu(Plotno& p, std::vector<std::string> lewo, const std::string& prawo);

// Pytanie tak/nie (5.12.8, artboardy Q-sennosc, Q-gorzej, Q-przerwa): etykieta, tytuł B3, opis B5, nad polami tekst
// (senność: `odpowiedź trafia do raportu`), trzy pola: `◀ nie` (ramka 3 px), `↻ pomiń` (ramka przerywana),
// `▶ tak` (ramka 3 px), w stopce termin odpowiedzi.
struct DanePytaniaTN {
  Belka naglowek;
  std::string etykieta, tytul, opis, nadPolami, stopka;
};
void rysujPytanieTN(Plotno& p, const DanePytaniaTN& d);
