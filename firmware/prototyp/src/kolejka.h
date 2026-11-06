// SPDX-License-Identifier: GPL-3.0-or-later
// Kolejka linii uwagi (spec 7.5, 5.4.5, R-EKR-12, R-EKR-52), warunek uspokojenia z wyników silnika (5.4.13 pkt 1, 4
// i 7), buzzer pilności A z ciszą nocną (5.14) i wybór pytania tak/nie (5.12.8). Wejście to wyniki
// silnika przez src/sprawy.h, nic więcej z reguł. Bez zależności od Arduino.
//
// Linie z wyników (7.5): grupa 0 `Wciąż: {tytuł}.` niewyciszonego wzorca A, który dziś nie jest ekranem dnia (pasek
// 60 px, R-EKR-42), i `Senność w dzień: powiedz o tym lekarzowi.` w dniu odpowiedzi „tak” (R-SKI-28); grupa 2
// `Wciąż: {tytuł}.` wzorca B po „wiem” (codziennie w okresie „wiem”) i w dni bez pełnego ekranu co 3 dni (R-SKI-37),
// `{nazwa}: trwa 8 tyg. po wizycie.` (R-SKI-11, raz, 56. dzień po „Omówione”), linia pilności C w niedzielę z zestawu
// (R-SKI-12), `Gorsze samopoczucie: skontaktuj się z lekarzem, jeśli trwa.` przez 3 dni po „gorzej” = tak (R-SKI-36),
// `Wciąż: {tytuł}. Omówione {data}.` wzorca A po „Omówione” co 3 dni (R-LEK-24); grupa 5 `Wciąż: {tytuł}.` L-09 po
// „wiem” (R-EKR-22). Reguła w stanie „tylko raport” nie daje linii nigdy (ADR-0029 pkt 3).
// Linie spoza silnika (zaproszenie, E-08, awaria pamięci R-AW-16) przychodzą gotowe od dyrygenta i stają w swojej grupie.
#pragma once
#include <set>
#include <string>
#include <vector>

#include "dzien.h"
#include "sprawy.h"
#include "stany.h"

namespace kolejka {

struct LiniaUwagi {
  std::string tekst;
  GrupaLinii grupa = GrupaLinii::NOC;
  bool wzorzecA = false;  // linia niewyciszonego wzorca A (R-SKI-22): pasek 60 px, 28 px (R-EKR-42)
  std::string zrodlo;     // klucz sprawy albo nazwa (do logu i testów)
  bool ostrzezenie = false;  // ostrzeżenie spoza grupy „zdrowie”: E-05 krytyczne jako linia (R-EKR-52, R-PIE-07)
  bool zdrowie() const { return ostrzezenie || liniaJestOstrzezeniem(grupa); }
};

// Miejsce grupy w kolejce (7.5): 0, 1, 2, potem R-SKI-24 z grupy 6 przed 2a (zaproszenie), 3, 4, 5, 6.
int rangaGrupy(GrupaLinii g);

struct WejscieKolejki {
  std::string dzis;
  const std::vector<sprawy::SprawaL>* sprawy = nullptr;
  const reguly::Odpowiedzi* odpowiedzi = nullptr;
  const sprawy::SprawaL* ekranDnia = nullptr;  // sprawa na ekranie dnia: jej linii dziś nie ma
  // linie spoza silnika, gotowe teksty; "" = brak
  std::string zaproszenie;  // `Co było inaczej? ↻ 2 s` (grupa 2a)
  std::string e08;          // E-08 (grupa „dane”)
  std::string awaria;       // R-AW-16 (grupa „awarie”)
  // R-PIE-07: E-05 krytyczne w dniu pełnego ekranu wzorca A, `Naładuj ekran: bateria {x}%.` (grupa „awarie”, pierwsza,
  // jako ostrzeżenie R-EKR-52); "" = brak
  std::string bateriaKrytyczna;
  // rytmy linii według rzeczywistych pokazań (R-PIE-08) i linie z danych spoza wyników silnika
  const reguly::Pamiec* pamiec = nullptr;  // pokazania linii (sprawy::ostatniePokazanie(), `linia:{źródło}`)
  int minuta = -1;  // minuta doby; slot tygodniowy od 17:00 [PRÓG] w niedzielę (5.8.2); -1 = cała niedziela
  const std::vector<sprawy::SprawaMetryki>* metryki = nullptr;  // R-NOR-21: metryki zdrowotne w stanie odchylenia
  const std::vector<sprawy::Odpowiedz>* odpowiedziPelne = nullptr;  // R-SKI-39: pokazane pytania o senność („brak”)
  std::string okresInfekcjiOd;  // R-INF-10: pierwszy dzień okresu infekcji (5.10.3); "" = okresu nie ma
  // przegląd: linie zdarzeń E z wyników w grupach 7.5 (E-10 `Noc {data} bez terapii.` w grupie „noc”,
  // E-04 w grupie „dane”, E-05 w „awariach”)
  const std::vector<sprawy::LiniaZdarzenia>* linieE = nullptr;
};
// Źródło linii do zapisu pokazania (sprawy::zapiszPokazanie(pamiec, "linia:" + zrodlo, dzis)), gdy linia rzeczywiście
// stanęła na ekranie (R-PIE-08): rytmy R-NOR-21 (7 dni), R-INF-10 (3 dni), rotacja C (R-SKI-41) i naprzemienność
// trwałej awarii z linią wzorca L (R-PIE-09).
std::string kluczPokazaniaLinii(const LiniaUwagi& l);
const int SLOT_TYGODNIOWY_OD = 17 * 60;  // [PRÓG] 5.8.2: niedziela 17:00–19:00 (R-KON-08)

// Wszystkie linie, które dziś czekają, w kolejności 7.5 (pierwsza stoi na ekranie, R-EKR-12). Zaproszenie odpada przy
// niewyciszonym wzorcu A (R-PIE-06).
std::vector<LiniaUwagi> kolejkaLinii(const WejscieKolejki& w);
// Linia na ekranie dnia: pierwsza z kolejki albo pusta (DaneDnia::linia, liniaA, liniaOstrzezenie).
void ustawLinie(DaneDnia& d, const std::vector<LiniaUwagi>& k);
// Linie grupy „zdrowie” z kolejki (R-EKR-48 (5)), bez linii, które niesie sprawa z kartą (te mają `zrodlo` sprawy).
std::vector<std::string> linieZdrowiaBezSprawy(const std::vector<LiniaUwagi>& k);
// Czy niewyciszony wzorzec A trwa (R-PIE-06, R-SKI-16: bez pytań przed ekranem).
bool niewyciszonyA(const std::vector<sprawy::SprawaL>& s, const std::string& dzis);
// R-HUM-26: aktywny wzorzec L z pilnością B, także wyciszony („wiem”, omówione) albo po
// 28 dniach (R-SKI-09); wtedy S2a i S2c bez K22 i bez przerywnika S2d.
bool aktywnyB(const std::vector<sprawy::SprawaL>& s);
bool aktywnyA(const std::vector<sprawy::SprawaL>& s);  // aktywny wzorzec A, także wyciszony („Omówione”)

// ---------- warunek uspokojenia (5.4.13) z wyników ----------
// czy powód „nie oceniam” (reguly::Wynik::powodNieoceny) to brak danych (za mało nocy, brak historii, normy,
// metryki, odczytu), a nie awaria zestawu. Lista dopuszczonych początków jest zamknięta: powód nierozpoznany (w tym
// `wymaga …`, `nieznany rodzaj`, `brak progu`, `zle …`) liczy się jak awaria i zostaje przy kroku `Część reguł nie działa`.
bool powodBrakuDanych(const std::string& powodNieoceny);
struct OcenaUspokojenia {
  bool silnik = false;      // są wyniki silnika
  bool wszystkieOcenione = false;  // pkt 7: żadna reguła (aktywna i „tylko raport”) nie ma „nie oceniam”
  bool awaria = false;      // któraś „nie oceniam” z powodem spoza listy braku danych (awaria zestawu, R-AW-31, albo powód nierozpoznany)
  bool pkt1 = false;        // brak spełnionej reguły L (aktywnej w dowolnej pilności, wyciszonej i „tylko raport”)
  // pkt 2: brak spełnionego zdarzenia z grupy R5b (np. E-23), także bez ekranu, linii i wykluczonego
  bool pkt2 = false;
  bool pkt4 = false;        // brak linii grupy „zdrowie” w kolejce
  std::vector<std::string> nieOceniam;  // `id: powód` do logu
};
OcenaUspokojenia ocenUspokojenie(const std::vector<reguly::Wynik>& wyniki, const std::vector<LiniaUwagi>& k);
// Liczą się tylko reguły z poziomem (aktywne i „tylko raport”); wiersze pomocnicze bez poziomu są pomijane.
// Wpisuje ocenę do stanów: punkty 1, 4 i 7 schodzą z listy nieocenianych tylko wtedy, gdy wszystkie reguły są
// ocenione (inaczej zostają, a uspokojenie jest niespełnione, ADR-0029 pkt 3 i 7); spełnienie wchodzi do
// uspokojenieOcenione (pkt 2 z wyników reguł). Pozostałe punkty bez zmian.
void zastosujUspokojenie(Stany& st, const OcenaUspokojenia& o);

// ---------- buzzer pilności A (5.14) ----------
// Cisza nocna buzzera (5.6.4, R-BUZ-02): domyślnie 22:00–08:00 [PRÓG]; wariant 22:00–10:00 (R-KONF-30).
const int CISZA_OD = 22 * 60, CISZA_DO = 8 * 60;
bool ciszaNocna(int minutaDoby, int od = CISZA_OD, int do_ = CISZA_DO);
// Znacznik wejścia w pilność A: `klucz@aktywnaOd` (nowe wejście po wyjściu = nowy znacznik, R-BUZ-06).
std::string znacznikWejscia(const sprawy::SprawaL& s, const std::string& dzis);
struct DecyzjaBuzzera {
  bool graj = false;
  std::vector<std::string> znaczniki;  // do zapamiętania po zagraniu (każde wejście raz, R-BUZ-04)
  bool odlozony = false;               // jest wejście, ale trwa cisza nocna (R-BUZ-02b: gra po jej końcu)
};
// Wejścia w A: tylko aktywna reguła z pilnością A (nigdy „tylko raport”), niewyciszona „wiem” ani „Omówione”
// (R-BUZ-05), bez znacznika w `zagrane`; w ciszy nocnej nic (odłożony), poza nią jeden sygnał na wszystkie wejścia
// (5.14.3). `wlaczony` = ustawienie `Dźwięk (buzzer)` (R-BUZ-03).
DecyzjaBuzzera buzzerPilnosci(const std::vector<sprawy::SprawaL>& s, const std::set<std::string>& zagrane,
                              const std::string& dzis, int minutaDoby, bool wlaczony = true, int ciszaOd = CISZA_OD,
                              int ciszaDo = CISZA_DO);
const int SYGNALY_A = 3;  // 3 krótkie dźwięki [PRÓG] (R-BUZ-01)

// ---------- pytanie tak/nie (5.12.8) ----------
struct PytanieDnia {
  bool jest = false;
  sprawy::Pytanie pytanie = sprawy::Pytanie::SENNOSC;
  std::string data;       // data, pod którą odpowiedź trafi do pliku
  int przerwaNocy = 0;    // przerwa planowana: noce bez terapii
};
struct WejsciePytan {
  std::string dzis;
  int minuta = 0;
  const std::vector<sprawy::SprawaL>* sprawy = nullptr;
  const std::vector<sprawy::Odpowiedz>* odpowiedzi = nullptr;
  const std::vector<Noc>* hist = nullptr;
  int terminRano = 11 * 60;  // termin pytań porannych (R-PIE-13: 11:00, przy imporcie po 09:00 import + 2 h)
  int pokazanoOd = -1;       // minuta pierwszego pokazania pytania o senność tego dnia (-1 = jeszcze nie)
  bool gorzejInfekcja = false;  // R-INF-07: okres infekcji wymaga dziś pytania „gorzej” (src/infekcja.h)
};
// Senność: niedziela od 17:00 [PRÓG] (T0; prototyp nie ma slotu tygodniowego), do 23:30 i najwyżej 30 min [PRÓG] od
// pierwszego pokazania (R-SKI-17; potem dyrygent zapisuje „brak odpowiedzi”), bez odpowiedzi tej niedzieli;
// „gorzej”: rano do terminu, gdy spełniona jest reguła L-02 (wariant b), L-05 albo L-06 (wariant c), najwyżej raz na
// 3 dni [PRÓG]; przerwa planowana: rano do terminu, gdy ostatnia noc ma terapię po ≥ 3 [PRÓG] nocach bez terapii
// (E-10: brak rekordu albo M-01 < 15 min), raz na przerwę, nie w dniu pytania „gorzej” (najdłużej 7 dni od powrotu).
// Żadnego przy niewyciszonym wzorcu A (R-PIE-06, R-SKI-16).
PytanieDnia pytanieDnia(const WejsciePytan& w);

}  // namespace kolejka
