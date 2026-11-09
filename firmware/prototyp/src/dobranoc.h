// SPDX-License-Identifier: GPL-3.0-or-later
// Ekran nocny S9 „Dobranoc” (ADR-0018, spec 5.8.11), bez zależności od Arduino.
//
// Kiedy (R-KON-31, R-KON-35): o decyzji „S9 tej nocy albo nie” przesądza pierwsze wybudzenie w slocie nocnym
// 23:30–05:00 (zwykle zegar o 23:30). S9 powstaje, gdy na ekranie jest ekran dnia i ten ekran nie musi zostać na noc
// (w prototypie: S5 „Brak danych”; w spec też L z pilnością A albo B i niewyciszony wzorzec A). Ekran otwarty
// przyciskiem albo S5 o 23:30: S9 tej nocy nie powstaje, także po powrocie przyciskiem do ekranu dnia (wtedy wraca
// ostatni ekran dnia, spec 5.8.11 ścieżki błędu, R-KON-35). Decyzja jest w NVS (`noc_w`, `noc_s9`), więc restart
// jej nie zmienia. Start zasilania w nocy przed decyzją: S9 od razu (wyjątek „start zasilania”, R-KON-31).
// Kolejne wybudzenia tej nocy niczego nie rysują (R-KON-02, R-KON-34), o 05:00 wraca ekran dnia.
// ↻ w K1 albo K2 w nocy z S9 wraca do S9 w tej samej treści (R-KON-37): fakty z pierwszego rysowania są w RTC.
//
// Rotacja (R-KON-32): n = numer dnia poranka (dni od 1.01.1970 do daty wieczoru + 1 dzień), wariant n mod 3
// (A1, A2, A3), scena (n div 3) mod k na liście wariantu (tylko sceny z SCENY_NOCY_DOZWOLONE). Zdanie budzika jest przypięte do sceny
// (pula „noc” w 5.7.5): W12, G14, G11, G10, G13, G15, G12 mają po jednym zdaniu.
//
// Granice: zero oceny zdrowia i wskazówek o ustawieniach (ADR-0002), budzik na białym tle (ADR-0009).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "humor.h"  // Humor (ustawienie `humor=`, R-KON-39)

enum class WariantNocy : unsigned char { A1 = 0, A2 = 1, A3 = 2 };

// Data wieczoru, od którego liczy się noc: od 23:30 dziś, przed 05:00 wczoraj; "" poza slotem nocnym albo bez daty.
std::string wieczorNocy(const std::string& dzis, int minuta);
// Numer dnia poranka nocy zaczętej wieczorem `wieczor` (R-KON-32); -1 dla złej daty.
long numerPoranka(const std::string& wieczor);

struct WyborNocy {
  long n = -1;  // numer dnia poranka
  WariantNocy wariant = WariantNocy::A1;
  std::string scena;   // id w SCENY_NOCY, np. "A1-G14"
  std::string zdanie;  // zdanie budzika przypięte do sceny: stopka w A1 i A3, linia pod `Dobranoc.` w A2
};
// zPropozycjami: bez skutku od przyjęcia puli „noc” (08.11.2026): zdanie przypięte do sceny jest zawsze;
// parametr zostaje na następną rundę propozycji (przełącznik HUMOR_PROPOZYCJE).
// humor `wyłączony` (R-KON-39, R-HUM-03): S9 bez sceny i bez zdania budzika (wariant i numer bez zmian).
WyborNocy wyborNocy(long n, bool zPropozycjami = true, Humor humor = Humor::NORMALNY);
// Zdanie budzika przypięte do sceny nocnej ("A1-W12" i "A3-W12" → W12); "" dla nieznanej sceny.
std::string zdanieNocy(const std::string& scena);
struct ZdanieSceny {
  const char* scena;  // id sceny bez wariantu, np. "W12"
  const char* zdanie;
};
extern const std::vector<ZdanieSceny> ZDANIA_NOCY;
// (propozycja do akceptacji): sceny spokojne dozwolone na S9, id bez wariantu (np. "W12"). Jedyne
// miejsce z tą listą; rotacja R-KON-32 liczy tylko sceny z niej (spec 5.8.11, 5.7.4).
extern const std::vector<const char*> SCENY_NOCY_DOZWOLONE;
bool scenaNocyDozwolona(const std::string& scena);  // "A1-W12" albo "W12"
std::vector<std::string> scenyNocyWariantu(WariantNocy w);  // sceny wariantu w kolejności rotacji, tylko dozwolone

// Stan w NVS: noc, dla której zapadła decyzja (wieczór), i czy S9 tej nocy powstał.
struct StanNocy {
  std::string wieczor;
  bool s9 = false;
};

struct DecyzjaNocy {
  std::string wieczor;          // "" poza slotem nocnym
  bool nowa = false;            // pierwsza decyzja tej nocy: zapisz stan (poDecyzji)
  bool nocny = false;           // ekranem dnia jest teraz S9
  bool rysujPoZegarze = false;  // wybudzenie zegarem: narysować S9 (tylko przy pierwszej decyzji)
  WyborNocy wybor;
};
// ekranDnia: na ekranie jest ekran dnia, nie ekran otwarty przyciskiem.
// dzienTrwa: ekran dnia ma zostać na noc (S5, R-KON-35); liczy się tylko przy pierwszej decyzji tej nocy.
DecyzjaNocy decyzjaNocy(const std::string& dzis, int minuta, bool ekranDnia, bool dzienTrwa, const StanNocy& s,
                        bool zPropozycjami = true, Humor humor = Humor::NORMALNY);
StanNocy poDecyzji(const DecyzjaNocy& d, const StanNocy& s);

// ---------- godzina założenia maski ----------
// Start trwającej sesji z `/api/status` AirBridge (sprawdzone na module v2.2.1): przy `rop` = 1 pole `edf_prefix`
// RRRRMMDD_HHMMSS (startZPrefiksu w airbridge.h). Bez terapii albo bez tego pola godziny nie ma (ADR-0018 pkt 5,
// R-KON-33).
struct OdczytMaski {
  int startSesji = -1;  // minuta doby startu trwającej sesji; -1 = brak wiarygodnej godziny
};
int maskaOd(const OdczytMaski& o);  // minuta doby albo -1

// ---------- teksty ----------
struct DaneNocy {
  WyborNocy wybor;
  std::string wieczor;       // RRRR-MM-DD
  int maskaOd = -1;          // minuta doby; -1 = brak wiarygodnej godziny
  float temp = NAN, wilg = NAN;  // czujnik w obudowie panelu (M-22)
  int zwyklySen = -1;        // mediana normy czasu z maską (M-01), minuty; -1 = brak normy
};

struct TekstyNocy {
  std::string naglowekLewo, naglowekPrawo;  // A1, A3
  std::string etykieta, tytul;              // A1: DOBRANOC / Śpij dobrze.; A3: etykieta i godzina albo `Dobranoc.`
  bool tytulGodzina = false;                // A3: tytuł to godzina (krój S140), inaczej `Dobranoc.` (S110)
  std::vector<std::string> linie;           // A1: linie powodu; A3: zdanie (P28B) i mała linia (P22B)
  std::vector<std::pair<std::string, std::string>> fakty;  // A1: liczba, podpis
  std::string krok;                         // A1
  std::string zdanieA2;                     // A2: zdanie budzika w cudzysłowie pod `Dobranoc.`
  std::string liniaA2;                      // A2: linia faktów pod zdaniem
  std::string stopkaLewo, stopkaPrawo;      // A1, A3
};
TekstyNocy tekstyNocy(const DaneNocy& d);

// Fakty S9 z pierwszego rysowania tej nocy (RTC panelu): ↻ z K1 albo K2 wraca do S9 w tej samej treści (R-KON-37,
// R-KON-34). Zapis z innej nocy (albo pusty po zaniku zasilania) zastępują odczyty z tej chwili.
struct FaktyNocy {
  char wieczor[11] = "";
  float temp = NAN, wilg = NAN;
  int16_t zwyklySen = -1, maskaOd = -1;
};
DaneNocy daneNocy(const DecyzjaNocy& d, FaktyNocy& zapis, float temp, float wilg, int zwyklySen, int maskaOd);
std::string hhmm(int minuta);  // 1421 → "23:41"
// „Noc z czwartku na piątek” dla wieczoru w czwartek.
std::string naglowekNocyA3(const std::string& wieczor);
