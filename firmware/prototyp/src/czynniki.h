// SPDX-License-Identifier: GPL-3.0-or-later
// Czynniki nocy i pytanie „Co było inaczej?” (ADR-0006, moduł CZYNNIKI 5.11), bez zależności od Arduino.
// Plik /czynniki.csv w LittleFS, jeden wiersz na noc (data poranka):
//   data;stan;kody;warstwa;termin;pokazano
//   stan:    C = czeka (linia zaproszenia do terminu), O = odpowiedź zapisana, B = brak odpowiedzi
//   kody:    zaznaczone czynniki (A alkohol, K katar/infekcja, S stres, J kolacja, L leki, T THC,
//            P spanie poza domem, N nic szczególnego); puste przy C i B
//   warstwa: O = zaproszenie po odchyleniu w jednej nocy, K = grupa kontrolna 1 na 7, Z = na żądanie z K1 albo
//            zaproszenie bez warstwy O-12 (R-CZY-07, S-K-17), A = oznaczenie automatyczne „spanie poza domem”
// po przerwie AirBridge (R-CZY-19; nie jest odpowiedzią, nie wchodzi do O-12)
//   termin:  termin odpowiedzi w minutach od północy dnia `data` (R-PIE-13), -1 = brak; przy warstwie A numer dnia
//            (nrDnia) zapisu oznaczenia (limit 7 dni pytania o wyjazd, R-CZY-22)
//   pokazano: 1 = linia zaproszenia była na ekranie (R-PIE-08; do wag O-12, R-CZY-24)
// Czynniki zostają tylko na panelu (ADR-0001); do raportu idą tylko zdania zdaniaRaportu() według konfiguracji
// (R-CZY-06, ADR-0013 pkt 2). warstwa A, M-13 w wyzwalaczu, wagi R-CZY-24, pytanie o wyjazd.
#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "noc.h"

enum Czynnik : uint8_t {
  CZ_ALKOHOL = 1, CZ_KATAR = 2, CZ_STRES = 4, CZ_KOLACJA = 8,
  CZ_LEKI = 16, CZ_THC = 32, CZ_POZA_DOMEM = 64, CZ_NIC = 128,
};

// Pozycje na ekranie pytania w kolejności kursora (wierszami siatki 2×4 z artboardu F08):
// alkohol | katar lub infekcja / stres | późna, ciężka kolacja / leki … | THC / spanie poza domem | nic szczególnego
struct PozycjaPytania { uint8_t bit; char kod; const char* etykieta; };
extern const PozycjaPytania POZYCJE[8];
const int POZYCJA_NIC = 7;  // R-CZY-18: kursor startuje na „nic szczególnego”

struct WpisCzynnikow {
  std::string data;
  char stan = 'C';
  uint8_t czynniki = 0;
  char warstwa = 'Z';
  int termin = -1;
  bool pokazano = false;
};

bool parsujWpis(const std::string& linia, WpisCzynnikow& w);
std::string zapiszWpis(const WpisCzynnikow& w);
std::string kody(uint8_t czynniki);
WpisCzynnikow* wpisZDnia(std::vector<WpisCzynnikow>& v, const std::string& data);
const WpisCzynnikow* wpisZDnia(const std::vector<WpisCzynnikow>& v, const std::string& data);

// Grupa kontrolna (R-CZY-02): noc wylosowana, gdy skrót (FNV-1a) z daty i ziarna panelu daje resztę 0 z 7.
// Ten sam wynik przy tym samym ziarnie; ziarno losowane raz na panel i trzymane w pamięci NVS.
bool losujKontrolna(const std::string& data, uint32_t ziarno);

// Termin odpowiedzi (R-PIE-13): import nocy do 09:00 → 11:00; później import + 2 h, najpóźniej 23:30.
int terminOdpowiedzi(int minutaImportu);

// Decyzja rano, po imporcie ostatniej nocy (R-CZY-01, R-CZY-02, R-CZY-09, R-PIE-10, R-PIE-11).
// Zwraca 'O' (odchylenie), 'K' (kontrolna) albo 0 (bez zaproszenia).
// prosbaNaEkranie: ekran dnia jest już prośbą (7.4); w prototypie takich ekranów nie ma (zdarzenia E i ekrany
// S-K to pakiet 3), parametr jest dla zgodności z regułą.
struct WejscieDecyzji {
  const Noc* noc = nullptr;  // rekord nocy z datą poranka = dziś
  Norma norma;
  int nocyWaznychPrzed = 0;  // rozruch globalny: < 14 nocy ważnych (4.10)
  bool prosbaNaEkranie = false;
  uint32_t ziarno = 0;
  // (5.11.3): niewyciszony wzorzec L z pilnością A (R-CZY-01, R-PIE-06), S-K-30 kandydatem slotu porannego
  // (R-CZY-15), S-K-17 spełnione (R-CZY-07; zaproszenie bez warstwy O-12: zwraca 'Z')
  bool wzorzecA = false, sk30 = false, sk17 = false;
};
// Okoliczności ranka z silnika reguł i spraw (dyrygent, poranekCzynniki()): przenoszone do WejscieDecyzji.
struct OkolicznosciRanka {
  bool prosbaNaEkranie = false;  // ekranem dnia jest pełny ekran sprawy (prośba, 7.4, R-PIE-10, R-CZY-17)
  bool wzorzecA = false;         // niewyciszony wzorzec L z pilnością A (R-CZY-01, R-PIE-06)
  bool sk30 = false;             // S-K-30 spełnione dziś: kandydat slotu porannego (R-CZY-15)
  bool sk17 = false;             // S-K-17 spełnione dziś (R-CZY-07)
};
// Odchylenie w jednej nocy w M-02, M-07, M-13 albo M-01 (4.5, R-CZY-01, R-CZY-24): odchylenieWNocy() i M-13
// (powyżej P90 normy i co najmniej Δ = 2 / min nad medianą, tabela 4.5).
bool odchylenieCzynnikow(const Noc& n, const Norma& nm);
char decyzjaZaproszenia(const WejscieDecyzji& we);

// Linia `Co było inaczej? ↻ 2 s` stoi na ekranie dnia, dopóki wpis czeka i nie minął termin (R-CZY-17, R-CZY-03).
bool zaproszenieAktywne(const WpisCzynnikow* w, const std::string& dzis, int minutaTeraz);
// Po terminie bez odpowiedzi: „brak odpowiedzi” (R-CZY-03). Zwraca true, gdy coś zmienił.
bool zamknijPoTerminie(std::vector<WpisCzynnikow>& v, const std::string& dzis, int minutaTeraz);

// Zaznaczanie na ekranie pytania (R-CZY-11): „nic szczególnego” odznacza resztę, inna pozycja odznacza „nic”.
uint8_t przelacz(uint8_t zaznaczone, int pozycja);

// Zapis odpowiedzi (R-CZY-10, R-CZY-13, R-CZY-14): bez zaznaczeń = „brak odpowiedzi”; nowa odpowiedź
// zastępuje poprzednią; noc bez wpisu (pytanie z K1) dostaje warstwę Z. zLinii = pytanie otwarte z linii
// zaproszenia; odpowiedź spoza linii dla wpisu już zamkniętego (B) przechodzi do warstwy Z.
void zapiszOdpowiedz(std::vector<WpisCzynnikow>& v, const std::string& data, uint8_t czynniki, bool zLinii = false);
void pominPytanie(std::vector<WpisCzynnikow>& v, const std::string& data);  // ◀ 2 s (R-CZY-14)

// Noce wyłączone z normy (R-CZY-05): odpowiedź z „katar/infekcja” albo „spanie poza domem”.
std::set<std::string> wykluczoneZNormy(const std::vector<WpisCzynnikow>& v);
// Noce wyłączone z normy sypialni (4.4, 5.11.5): odpowiedź z „spanie poza domem”.
std::set<std::string> nocePozaDomem(const std::vector<WpisCzynnikow>& v);
// Znak ◆ w K1 i K2 (R-KOK-14): odpowiedź z „alkohol”, „stres”, „kolacja”, „leki” albo „THC”.
bool romb(const WpisCzynnikow* w);

// ---------- wagi O-12 (R-CZY-24) ----------
// Wpis z odpowiedzią z warstwy O-12: zaproszenie pokazane (R-PIE-08), warstwa O albo K, stan O (bez A, Z).
bool wWarstwieO12(const WpisCzynnikow& w);
// r: odsetek odpowiedzi na linie zaproszenia pokazane w warstwie `warstwa` (O albo K) w `dni` dniach do `dzis`
// włącznie (stan O wobec O i B; stan C czeka i się nie liczy). NAN, gdy w oknie nie pokazano żadnej linii.
double odsetekOdpowiedzi(const std::vector<WpisCzynnikow>& v, const std::string& dzis, char warstwa, int dni);

// ---------- spanie poza domem po przerwie AirBridge (R-CZY-19, R-CZY-22, R-CZY-23) ----------
struct ProgiWyjazdu {
  int przerwaDob = 2;             // R-CZY-19.przerwa [PRÓG]
  int oknoOd = 11 * 60;           // ostatnia noc z czynnikiem, gdy pierwsza odpowiedź od 11:00 …
  int oknoDo = 23 * 60 + 30;      // … do 23:30 (R-CZY-19.okno_ostatniej_nocy) [PRÓG]
};
// Po pierwszej udanej odpowiedzi AirBridge (`dataTeraz`, `minutaTeraz`) po ostatniej udanej (`dataOst`, `minutaOst`):
// przy przerwie ≥ 2 doby zapisuje nocom z listy `noce` (daty poranka nocy, które AirBridge podał) z datą po `dataOst`
// czynnik automatyczny (warstwa A), a nocy z datą `dataTeraz` tylko przy odpowiedzi między 11:00 a 23:30. Odpowiedź
// użytkownika (stan O spoza warstwy A) ma pierwszeństwo (R-CZY-13). Zwraca liczbę oznaczonych nocy.
int oznaczPozaDomem(std::vector<WpisCzynnikow>& v, const std::vector<std::string>& noce, const std::string& dataOst,
                    int minutaOst, const std::string& dataTeraz, int minutaTeraz, const ProgiWyjazdu& p = ProgiWyjazdu());
// Okres nocy z oznaczeniem automatycznym: kolejne daty poranka (dzień po dniu), od najstarszego.
struct OkresWyjazdu {
  std::string od, do_;  // daty poranka pierwszej i ostatniej nocy okresu
  int nocy = 0;
  long zapis = -1;      // numer dnia zapisu oznaczenia (pole `termin` wpisów A)
};
std::vector<OkresWyjazdu> okresyWyjazdu(const std::vector<WpisCzynnikow>& v);
// R-CZY-22: okres do pytania `Spanie poza domem od {data}?` dziś: najstarszy okres, o który panel nie pytał (`zapytane`:
// daty `od` okresów z pytaniem pokazanym), zapisany najwyżej `dni` [PRÓG] dni temu. nullptr = brak pytania.
const OkresWyjazdu* okresDoPytania(const std::vector<OkresWyjazdu>& o, const std::set<std::string>& zapytane,
                                   const std::string& dzis, int dni = 7);
// R-CZY-23: „nie” zdejmuje oznaczenie automatyczne z nocy okresu (wpisy A z datą od…do); zwraca zdjęte wpisy
// (do cofnięcia ◀ 2 s w 60 s, R-PRZ-11: przywrocWpisy()). Noce z odpowiedzią użytkownika zostają.
std::vector<WpisCzynnikow> zdejmijPozaDomem(std::vector<WpisCzynnikow>& v, const OkresWyjazdu& o);
void przywrocWpisy(std::vector<WpisCzynnikow>& v, const std::vector<WpisCzynnikow>& zdjete);
// Teksty pytania tak/nie R-CZY-22 (etykieta, tytuł, opis, stopka); `termin` = minuta doby terminu (R-PIE-13).
struct TekstyWyjazdu { std::string etykieta, tytul, opis, stopka; };
TekstyWyjazdu tekstyWyjazdu(const OkresWyjazdu& o, int termin);

// ---------- czynniki w raporcie dla lekarza (5.11.7, R-CZY-06, ADR-0013 pkt 2) ----------
// Domyślnie w raporcie: „katar/infekcja” i „leki nasenne/przeciwbólowe” (ten tylko w raporcie o L-01).
const uint8_t RAPORT_DOMYSLNIE = CZ_KATAR | CZ_LEKI;
// Zdania strefy „Dlaczego ten raport” z 14 nocy do `dzis` włącznie: `Zgłoszone przez pacjenta: {czynnik} w {n} z 14
// nocy.` dla każdego czynnika włączonego (`wlaczone`, bity Czynnik), a noce automatyczne „spanie poza domem” osobnym
// zdaniem `Bez danych z domu: {n} z 14 nocy.`; leki tylko przy `raportL01`. Bez nocy z czynnikiem: bez zdania.
std::vector<std::string> zdaniaRaportu(const std::vector<WpisCzynnikow>& v, const std::string& dzis, uint8_t wlaczone,
                                       bool raportL01);
