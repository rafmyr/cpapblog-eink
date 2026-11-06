// SPDX-License-Identifier: GPL-3.0-or-later
// Treść ekranu dnia (tytuł, powód, fakty, krok) dla typów z humor.h, bez zależności od Arduino.
// Ten sam tekst na karcie i na ekranie danych (R-HUM-13); karta zamiast wizualizacji ma rząd faktów.
// Teksty: spec 5.4.6, 5.4.8, 5.8.4 (S-K-28), 6.4 (E-01) i docs/spec/robocze/03-teksty-kart.md.
#pragma once
#include <string>
#include <vector>
#include "ekrany.h"
#include "noc.h"
#include "sprawy.h"
#include "stany.h"
#include "sypialnia.h"

// Kod S5 dla E-03 (nieczytelny format albo wersja AirBridge spoza zakresu); inne kody błędu to na ekranie E-01.
const int KOD_E03 = -200;
const int KOD_WIFI = -100;  // brak Wi-Fi (opis tylko w logu i na ekranie testu)

struct DaneDnia {
  std::string dzis;
  const std::vector<Noc>* hist = nullptr;
  Norma norma;
  const Stany* stany = nullptr;  // stany odchylenia (stany.h) dla `dzis`; nullptr = trescDnia liczy je sama, bez czynników
  // S5
  std::string ostatnieDane;  // kiedyOdczyt() ostatniego udanego odczytu, np. "05:42" albo "28.10 05:42"
  std::string bladOd;        // kiedyOdczyt() pierwszej nieudanej próby od ostatniego udanego odczytu
  std::string abData;        // data ostatniego udanego odczytu AirBridge (licznik nocy E-01); "" = nie było
  int abMinuta = -1;         // minuta doby ostatniego udanego odczytu
  std::string bladData;      // data pierwszej nieudanej próby (licznik nocy E-01 bez udanego odczytu); "" = nieznana
  int httpKod = 0;           // KOD_E03: raport nieczytelny albo wersja AirBridge spoza zakresu; inny = E-01
  std::string abWersja;      // `version` z /api/status (tekst E-03)
  bool logowanieOdrzucone = false;  // ostatnia próba: odrzucone logowanie (R-AW-34, StanRtc); E-01 od 2. nocy
                                    // mówi wtedy `Ponawiam co 30 min.` (ponawianie co 30 min, R-DANE-18), a nie
                                    // `Sprawdzam co 3 h.` (R-AW-24)
  // S5 z E-05 krytycznym (6.4, R1): tytuł, powód i krok z zestawu (`Naładuj ekran` / `Bateria {x}%.` /
  // `→ Podłącz kabel USB-C.`); pusty tytuł = S5 z E-01 albo E-03
  std::string e05Tytul, e05Powod, e05Krok;
  // nagłówek i stopka (5.4.3)
  std::string godzinaDanych;  // `05:42`: godzina pobrania danych (R-EKR-04); "" = jeszcze nic nie pobrano
  std::string pogoda;         // lewa część stopki (M-26) z lokalnego źródła otoczenia (pogodaTeraz, otoczenie.h, ADR-0019):
                              // `Katowice 8° · deszcz`; "" = brak źródła albo dane nieświeże, bez komunikatu (R-AW-07)
  std::string przyLozku;      // prawa część stopki: odczyt czujnika w obudowie panelu z tego wybudzenia (stopkaCzujnika,
                              // `przy łóżku 21° · 46%`), z CO2 sypialni ze źródła, gdy świeże (`… · CO2 770`); "" = brak
                              // odczytu (R-AW-13, R-AW-06)
  const NocSypialni* sypNoc = nullptr;  // S2c: mediana ostatniej nocy
  NormaSypialni sypNorma;
  std::string linia;          // linia uwagi (ekran danych)
  bool liniaA = false;        // linia niewyciszonego wzorca A (60 px, 28 px, R-EKR-42); ustawia kolejka::ustawLinie
  bool liniaOstrzezenie = false;  // liniaJestOstrzezeniem(grupa): czarny pasek z `!`; zaproszenie to prośba (biała)
};

// Grupy kolejki linii uwagi (7.5). Ostrzeżeniem (czarny pasek z `!`, R-EKR-52) jest tylko grupa „zdrowie”: linie
// grup 0 i 2, `Wciąż: {tytuł}.` zdarzeń R5b w grupie 5 i R-SKI-24 w grupie 6. Awarie, dane, norma, pytania
// (zaproszenie) i reszta grupy 6 to informacja albo prośba (biały pasek w ramce).
enum class GrupaLinii : unsigned char { PILNOSC_A_SENNOSC, AWARIE, WZORCE_L, PYTANIA, DANE, NORMA, PRZYPOMNIENIE,
                                        PRZYPOMNIENIE_R5B, NOC, NOC_R_SKI_24 };
bool liniaJestOstrzezeniem(GrupaLinii g);

// Stopka ekranu dnia (R-EKR-10, 5.4.3): lewa i prawa część w jednej linii 744 px z odstępem 24 px. Gdy się nie mieszczą,
// ustępują po kolei: opis pogody, miasto, dopisek CO2 (test: najdłuższe wartości w test/testy.cpp).
const int SZER_STOPKI = 744, ODSTEP_STOPKI = 24;
void dopasujStopke(std::string& lewo, std::string& prawo);
// E-08 (6.4, grupa „dane” 7.5): `Brak odczytu CO2 od {czas}.` z `od` ostrzeżenia `co2_milczy` (dzień i minuta w czasie
// panelu; `{czas}` jak w E-01: `13:00` dziś, `20.10 13:00` innego dnia); bez `od`: `Brak odczytu CO2.`
std::string tekstE08(const std::string& dataOd, int minutaOd, const std::string& dzis);

// typ i klucz z wybierzEkranDnia (humor.h)
TrescEkranu trescDnia(const std::string& typ, const std::string& klucz, const DaneDnia& d);
// fakty karty przerywnika S2d (5.7.6: tylko fakty ostatniej nocy, M-01 i AHI): `{czas} z maską`, `{ahi} AHI`
// (brak = „–”, R-DANE-07); bez nocy pusto.
std::vector<Fakt> faktyPrzerywnika(const DaneDnia& d);
// Treść karty przerywnika S2d (5.7.6): tytuł i powód autora (humor.h tekstPrzerywnika()), bez etykiety (ADR-0009)
// i bez kroku (R-HUM-21), fakty ostatniej nocy. Reszta pól (nagłówek, stopka) zostaje z `t`.
void trescPrzerywnika(TrescEkranu& t, const std::string& tytul, const std::string& powod, const DaneDnia& d);
// Godzina odczytu do powodu S5 (U7): `05:42` dziś, `28.10 05:42` innego dnia; "" bez odczytu.
std::string kiedyOdczyt(const std::string& dataOdczytu, int minuta, const std::string& dzis);
// Linia powodu na ekranie pytania (5.11.4): przy odchyleniu do dwóch metryk, np. `AHI 6,8 przy zwykłych 0,8–2,1`;
// przy grupie kontrolnej i na żądanie `{M-01} · AHI {x}`.
std::string opisNocyPytania(const Noc& n, const Norma& nm, bool innaNoc);
// Lewa część nagłówka porannego na każdym ekranie dnia i karcie: tylko `{dzień tyg.} {data poranka}` (5.4.3:
// liczby nocy stoją na ekranie raz, w bloku liczb albo w faktach karty)
std::string naglowekDniaLewo(const DaneDnia& d);
// Prawa część nagłówka porannego (5.4.3): `dane {hh:mm}` (R-EKR-04), gdy ostatnia zapisana noc jest nocą ubiegłą;
// z dopiskiem `· ocena ograniczona`, gdy ta noc jest niepewna (R-EKR-46); `dane z {data}`, gdy nie jest (R-EKR-39);
// "" bez żadnej nocy.
std::string naglowekDniaPrawo(const DaneDnia& d);
// Tytuł karty K22 dla S2a i S2c (5.4.2, 5.7.2, R-EKR-32, R-NF-23): `W normie.` tylko przy warunku z nocWNormie, inaczej
// `Noc: {czas} z maską.` (bez oceny), a bez czasu `Noc zapisana.`; czas z tytułu nie powtarza się w faktach.
// Ekran danych S2a i S2c pokazuje zamiast tytułu blok liczb nocy (wariant B, 5.4.2).
std::string tytulNocy(const Noc& n, const Stany& st);
// `W normie` (5.4.2, 5.4.13): tylko bez odchylenia w ostatniej nocy, przy nocy ważnej i ocenianej, z czasem z maską
// i przy spełnionym warunku uspokojenia (Stany::uspokojenie: false przy ocenie niepełnej, Stany::ocenaNiepelna).
bool nocWNormie(const Noc& n, const Stany& st);

// Kolejne poranki bez rekordu nocy do dziś włącznie (licznik E-01, R-AW-24): 1 = tylko dziś.
int nocyBezDanych(const DaneDnia& d);

// ---------- treść ekranów z wyników silnika (src/sprawy.h) ----------
// Etykiety pilności (5.12.2): A `Skontaktuj się w ciągu kilku dni`, B `Warto skonsultować · w najbliższych tygodniach`;
// L-09 (ton „problem”) bez etykiety pilności.
std::string etykietaPilnosci(reguly::Pilnosc p);
// Ekran wzorca L dla sprawy, która dziś jest ekranem dnia (sprawy::sprawaEkranuDnia); nagłówek jak ekran dnia.
// Wizualizacja (5.13.4, parametr kluczowy) dla wzorców z ekranem w 1.0: L-03 AHI i L-09 wyciek P95 z 14 nocy do
// `d.dzis` (linia, przerywana = próg), L-08 2 × 30 nocy ≥ 4 h (rzędy). L-11 bez wizualizacji: kanwa ma kółka tygodni
// z tekstami spoza spec (do decyzji); L-06 (SpO2) zablokowany.
DaneWzorca trescWzorca(const sprawy::SprawaL& s, const DaneDnia& d);
// „Przeniesione” po „wiem” (5.4.9); `cofniecie` = podpis `◀ 2 s cofnij` (w nocy po 30 s bez, R-PRZ-19).
DanePotwierdzenia trescPrzeniesione(const sprawy::SprawaL& s, const DaneDnia& d, bool cofniecie);
// „Zapisano: tak.” / „Zapisano: nie.” (5.6.2); nad tytułem treść pytania.
DanePotwierdzenia trescZapisano(sprawy::Pytanie p, bool tak, const DaneDnia& d, bool cofniecie);
// „Raport dla lekarza” z ekranu L (stan R-LEK-05).
DaneRaportu trescRaportu(const sprawy::SprawaL& s, const DaneDnia& d);
// Potwierdzenie „Omówione z lekarzem?” dla sprawy z ekranu L.
// `wariant` = wzorce::WariantOmowienia jako liczba (ekrany.h zdaniaOmowienia); -1 = według pilności
DaneOmowione trescOmowione(const sprawy::SprawaL& s, const std::string& dzis, int wariant = -1);
// Karta `i` (od 1) ekranu „Otwarte sprawy”; pusta lista = stan pusty (R-EKR-50).
DaneSpraw trescSpraw(const std::vector<sprawy::KartaSprawy>& karty, int i, const sprawy::WejscieSpraw& w);
// Pytanie tak/nie (5.12.8): `termin` = minuta doby terminu odpowiedzi pytań porannych (R-PIE-13), `nocyPrzerwy` dla
// pytania o przerwę.
DanePytaniaTN trescPytaniaTN(sprawy::Pytanie p, const DaneDnia& d, int termin, int nocyPrzerwy);
