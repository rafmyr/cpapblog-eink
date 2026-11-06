// SPDX-License-Identifier: GPL-3.0-or-later
// Pozostałe ekrany kontekstowe S-K (spec 5.8.3, 5.8.4, 5.8.5, 7.2–7.4): wieczór S-K-03…S-K-09,
// rano S-K-10…S-K-17 (bez S-K-13), S-K-27, S-K-29, S-K-30 (moduł INFEKCJA), S-K-31 i S-K-32 (moduł SPRZĘT), tydzień
// S-K-18, poniedziałek S-K-19…S-K-21. S-K-01, S-K-02, S-K-13 i S-K-33…S-K-39 robi (src/kontekst.*,
// src/slot_dzienny.*), S-K-28 (kamień) humor (src/humor.h, typ `KM`). Bez Arduino.
//
// Katalog: docs/spec/katalog/SK.csv (ID, slot, grupa 7.2, stan, teksty, rodzaj z rodzaje-sk.csv, powód blokady).
// Warunki są tu w kodzie, a nie w silniku (src/reguly.h), bo potrzebują danych spoza reguly::Kontekst (czujnik i
// rekord sypialni, prognoza, dziennik pokazań, moduły SPRZĘT i INFEKCJA), a kontraktu silnika nie zmienia.
// Teksty biorę z zestawu (docs/spec/katalog/teksty.csv, wiersze `S-K-xx.*`), który skrypt tools/zestaw/buduj_zestaw.py
// sprawdza dosłownie ze spec funkcjonalną (granica ADR-0002); progi z docs/spec/progi.csv (wiersze `// próg: …`
// sprawdza tools/test_katalog_sk.py).
//
// Zablokowane (SK.csv, stan `zablokowana`): S-K-11 (przebieg wycieku w czasie, etap 3: brak danych w rekordzie
// nocy), S-K-19 wariant GI. Bez danych w rekordzie (milczą, „nie oceniam”, bez komunikatu, R-KON-06): S-K-07
// i krok „pora” S-K-18 (M-18 poza rekordem nocy, pozycja 10 listy ekranów wieczornych), S-K-06 i S-K-21 wariant
// pyłek od drugiego sezonu (brak historii pyłku), S-K-21 wariant CO2 bez rekordów CO2 sypialni.
#pragma once
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "ekrany.h"
#include "humor.h"
#include "infekcja.h"
#include "kontekst.h"
#include "noc.h"
#include "reguly.h"
#include "slot_dzienny.h"
#include "sprzet.h"
#include "sypialnia.h"

namespace sk {

// ---------- progi (docs/spec/progi.csv) ----------
// próg: S-K-03.temperatura = 16
const float SK03_TEMP = 16;
// próg: S-K-04.wilgotnosc = 35
const float SK04_WILG = 35;
// próg: S-K-05.prognoza_max = 29
const float SK05_MAX = 29;
// próg: S-K-05.sypialnia = 26
const float SK05_SYP = 26;
// próg: S-K-07.sd_startu = 60
const double SK07_SD = 60;
// próg: S-K-08.m01 = 30
const int SK08_M01 = 30;
// próg: S-K-09.weekendy = 4
const int SK09_WEEKENDY = 4;
// próg: S-K-09.roznica = 30
const double SK09_ROZNICA = 30;
// próg: S-K-09.grupa_min = 5
const int SK09_GRUPA = 5;
// próg: S-K-10.przeciek_nosowa = 24
const float SK10_P95 = 24;  // duży wyciek „terapia nie działa”: powód z minutami ponad 24, „od kilku nocy” 24, S-K-14, S-K-31
// próg: S-K-10.problem_p95 = 15
const float SK10_PROBLEM_P95 = 15;  // S-K-10 w tonie „problem” od M-07 P95 co najmniej tyle (= R-NOR-05.niepewna_p95)
// próg: E-23.przeciek_p95 = 27
const float E23_P95 = 27;
// próg: S-K-12.mediana_przecieku = 30
const float SK12_WMED = 30;
// próg: S-K-12.terapia_min = 15
const int SK12_MIN = 15;
// próg: S-K-12.terapia_max = 2
const int SK12_MAX_H = 2;
// próg: S-K-14.wilgotnosc = 35
const float SK14_WILG = 35;
// próg: S-K-15.sypialnia = 26
const float SK15_SYP = 26;
// próg: S-K-15.za_oknem = 29
const float SK15_OKNO = 29;
// próg: S-K-15.m01 = 4
const int SK15_M01_H = 4;
// próg: S-K-16.m17 = 3
const int SK16_M17 = 3;
// próg: S-K-17.noce = 2
const int SK17_NOCE = 2;
// próg: S-K-18.rekordy = 5/7
const int SK18_REKORDY = 5;
// próg: S-K-19.tymczasowy_dodatek = 0.1
const double SK19_DODATEK = 0.1;
// próg: R-KON-30.po_zmianie = 14
const int SK19_PO_ZMIANIE = 14;
// próg: S-K-21.okno = 60
const int SK21_OKNO = 60;
// próg: S-K-21.grupa_min = 5
const int SK21_GRUPA = 5;
// próg: S-K-21.roznica = 30
const double SK21_ROZNICA = 30;
// próg: S-K-27.odsetek = 70
const int SK27_ODSETEK = 70;
// próg: S-K-27.okres = 90
const int SK27_OKRES = 90;
// próg: S-K-29.noce_bez = 3
const int SK29_NOCE_BEZ = 3;
// próg: R-KON-13.kilka_nocy = 3/7
const int SK10_KILKA = 3;
// próg: R-KON-14.pokazania = 2/7
const int PROBLEM_POKAZAN = 2;
// próg: R-KON-19.limit_dni = 7
const int SK07_CO_DNI = 7;
// próg: R-KON-29.limit_dni = 7
const int SEZON_CO_DNI = 7;
// próg: R-KON-29.pokazania = 4
const int SEZON_POKAZAN = 4;
// próg: R-EKR-25.odkrycia = 7
const int ODKRYCIA_CO_DNI = 7;
// próg: R-NOR-12.delta.M-01 = -1.5
const double DELTA_M01_H = -1.5;
// próg: R-NOR-12.delta.M-13 = 2
const double DELTA_M13 = 2;
// próg: R-NOR-12.delta.M-05p = 2
const double DELTA_M05P = 2;
// próg: R-NOR-12.delta.M-06p = 2
const double DELTA_M06P = 2;
// próg: R-NOR-12.delta.M-07 = 10
const double DELTA_M07 = 10;
const int EKRANOW_SK_NA_DOBE = 3;  // 7.4: najwyżej 3 różne ekrany S-K w dobie [PRÓG]
const int TEN_SAM_WIECZOR_DNI = 3;  // 7.4: ten sam ekran wieczorny raz na 3 dni [PRÓG]
const int ROZRUCH_NOCY = 14;        // 7.3: rozruch globalny < 14 nocy ważnych (ADR-0003)

// ---------- treść ekranu ----------
enum class Wiz : unsigned char { BRAK, LINIA, KROPKI, SLUPKI, PARY, KOLKA };
struct Tresc {
  std::string id;          // `S-K-10`
  std::string wariant;     // `kilka` (S-K-10 „od kilku nocy”), `co2`, `upal`, `weekend` (S-K-21) …
  std::string typ;         // typ w dzienniku `/humor.csv` i w tabeli 5.7.3 (`SK10`)
  std::string grupa;       // pozycja 7.2: `W1`, `R5c`, `T2` …
  int kolejnosc = 0;       // kolejność w grupie (7.2), mniejsza wygrywa
  bool problem = false;    // ton „problem” (negatyw, S3)
  bool karta = false;      // typ ma kartę w tabeli 5.7.3 (ekranKarty()); inaczej zawsze ekran danych
  std::string sprawa;      // klucz sprawy do ekranKarty() i limitów
  int wartosc = 0;         // wartość wyzwalacza (log, 7.4 „≥ 25% gorsza”)
  bool doLogu = false;     // tryb próbny (R-KON-05): tylko wpis w logu
  TrescEkranu t;           // etykieta, tytuł, powód, krok
  Wiz wiz = Wiz::BRAK;
  std::vector<float> linia;
  float prog = NAN;        // linia przerywana na wykresie
  std::vector<int> kropki; // 1 pełna, 0 pusta
  std::vector<Fakt> pary;  // PARY: liczba i podpis; SLUPKI: wartość słupka (tekst) i podpis
  std::vector<float> slupki;
  std::vector<std::string> kolka;
  std::vector<bool> kolkaPelne;
  std::string podpisLewo, podpisPrawo;  // pod wizualizacją
  std::string stopkaPrawo;              // S-K-30: sygnał towarzyszący
  std::string pytanie;                  // S-K-30: pytanie zamiast kroku
  const char* dlaczego = "";            // do logu
};

// ---------- wejście ----------
struct Wejscie {
  std::string dzis;                                 // data poranka (rano) albo data wieczoru (zeszła noc = noc `dzis`)
  const std::vector<Noc>* hist = nullptr;           // po dacie rosnąco
  const Norma* norma = nullptr;                     // norma z dnia `dzis` (bez nocy wyłączonych, R-CZY-05)
  reguly::Tryb tryb = reguly::Tryb::NIEZNANY;       // ASV, ASVAuto: M-06p zamiast M-05p (4.11)
  const std::vector<NocSypialni>* sypialnia = nullptr;
  float tempTeraz = NAN, wilgTeraz = NAN;           // czujnik panelu (M-22) z tego wybudzenia; NAN = brak (R-AW-13)
  const Prognoza* prognoza = nullptr;               // ważna prognoza godzinowa (R-KON-53) albo nullptr
  const PylkiDnia* pylki = nullptr;                 // progi „górne 10% sezonu” rodzajów pyłku
  const std::vector<WpisHumoru>* dziennik = nullptr;
  bool trybProbny = TRYB_PROBNY_KONTEKSTU;          // 5.8.8 (wyłączony, do decyzji)
  bool airbridge = true;                            // E-01 nieaktywne (S-K-08)
  bool tagNawilzania = false, nawilzanie = false;   // S-K-03: tag nawilżania w `settings.fields[]` (etap 3: brak)
  // wyniki silnika potrzebne wykluczeniom 7.2
  bool e14 = false, e21 = false, e23 = false;
  bool l07Aktywne = false;                          // L-07 spełnione w stanie AKTYWNA (S-K-20 = ekran tygodnia L-07)
  // moduły
  bool okresInfekcji = false;                       // bramka 7.3 (R-INF-02)
  const std::vector<infekcja::Okres>* okresy = nullptr;
  const sprzet::Elementy* sprzet = nullptr;
  std::set<std::string> pozaDomem;                  // czynnik „spanie poza domem” (R-CZY-12)
  std::map<std::string, int> startyM18;             // M-18: minuta startu od 12:00 dnia poprzedniego; puste = brak
  // S-K-18 (krok, 5.8.4)
  bool sennoscTak = false;                          // „tak” na pytanie o senność w tym tygodniu (R-SKI-28)
  bool uspokojenie = false;                         // warunek 5.4.13 (R-EKR-32)
  bool liniaZdrowiaNaEkranie = false;               // linia grupy „zdrowie” stoi na S-K-18
  // (R-EKR-32): uspokojenie niespełnione tylko przez punkty bez oceny: pkt 7 (reguły) albo pkt 3/5
  bool tylkoRegulyBezOceny = false, tylkoOcenyWylaczone = false;
  bool tylkoBrakDanych = false;                     // „nie oceniam” tylko z braku danych, bez awarii zestawu
  int uwagaOMasce = UWAGA_O_MASCE_DOMYSLNIE;        // R-KONF-42 „Uwaga o masce od” (10–15 L/min): wariant opisowy S-K-10
  bool giTymczasowy = true;                         // ustawienie `GI` = `tymczasowy` (5.6.4; bez jedyny wariant)
  // limity 7.4 (wieczór)
  bool prosbaDzis = false;                          // R-PIE-10: dziś była już prośba
  int wieczorowZKrokiem7 = 0;                       // R-PIE-12
};

int nocyWaznych(const Wejscie& w);  // do rozruchu globalnego (7.3)
// (6.4 E-23, R-KON-13): warunek wariantu „od kilku nocy” S-K-10 (wyciek P95 > SK10_P95 w co najmniej
// SK10_KILKA z 7 ostatnich nocy z terapią do `dzis`), ten sam co w sk10(); poza okresem infekcji (R-INF-02).
bool sk10Kilka(const std::vector<Noc>* hist, const std::string& dzis, bool okresInfekcji);
// Wynik silnika po warunku 6.4 „E-23 nie wchodzi, gdy spełniony jest wariant od kilku nocy S-K-10”: wiersz pomocniczy
// E-23/bez_sk10_kilka dostaje ocenę (S-K-10 liczy kontekst, nie silnik), a spełnione E-23 przy `kilka` jest
// wykluczone przez `S-K-10/kilka`.
void zastosujSk10Kilka(std::vector<reguly::Wynik>& wyniki, bool kilka);
bool rozruch(const Wejscie& w);

// Kandydaci slotu po bramkach 7.3 i limitach 7.4, w kolejności tabeli 7.2 (pierwszy wygrywa). Kandydat z `doLogu`
// (tryb próbny) nie jest pokazywany: wołający zapisuje log i bierze następnego.
std::vector<Tresc> rano(const Wejscie& w);
std::vector<Tresc> wieczor(const Wejscie& w);  // S-K-03…S-K-09 (W1: S-K-05 przed S-K-02, S-K-01)
// Slot tygodniowy (niedziela 17:00–19:00, T2): S-K-18; false = warunek niespełniony (< 5 rekordów w 7 nocach).
bool tydzien(const Wejscie& w, Tresc& t);
// Trendy tygodnia T3 (S-K-19, S-K-20, S-K-21): ocenione w niedzielę (`niedziela` = data poranka niedzieli), pokazywane
// w poniedziałek w R8 (R-KON-27); rano() woła je sama w poniedziałek.
std::vector<Tresc> trendyTygodnia(const Wejscie& w, const std::string& niedziela);

// Pozycja grupy z tabeli 7.2 (rano: R0 = 0 … R9 = 12; wieczór W1 = 20 … W3 = 22; tydzień T2 = 30), mniejsza wygrywa.
int ranga(const std::string& grupa);
// Ranga typu ekranu dnia z rotacji prototypu (src/humor.h wybierzEkranDnia: S5, S10, KM, S6, S2x, S1).
int rangaTypu(const std::string& typEkranuDnia);
// Treść z dziennika (dzień z wpisem: ten sam ekran po ponownej ocenie); nullptr gdy dziś nie było ekranu S-K.
const WpisHumoru* wpisDzis(const std::vector<WpisHumoru>& d, const std::string& dzis, bool wieczorny);
bool typB16(const std::string& typ);         // SK03…SK12, SK14…SK21, SK27, SK29, SK30, SK31, SK32
bool typWieczorny(const std::string& typ);   // SK03…SK09

// Rysowanie (szablon A, kanwa „interfejs docelowy v1” artboardy SKnn, skala B): nagłówek, linia uwagi, etykieta,
// tytuł SB44, powód P28B (2 linie), wizualizacja, krok, stopka. Ton „problem” w negatywie (S3, R-EKR-52).
void rysuj(Plotno& p, const Naglowek& n, const Tresc& t, const std::string& stopkaLewo, const std::string& stopkaPrawo);

// Tekst z zestawu (teksty.csv) z podstawionymi zmiennymi; "" gdy brak.
std::string tekst(const std::string& idTekstu, const std::map<std::string, std::string>& zmienne = {});

}  // namespace sk
