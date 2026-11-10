// SPDX-License-Identifier: GPL-3.0-or-later
// Wzorce lekarskie L i hipotezy H w wersji beta: stany wzorca L (spec 5.12.12),
// łączenie i wybór hipotezy (5.12.6, 5.12.9), „wiem” i „Omówione z lekarzem” (5.12.7), lista „Otwarte sprawy” (5.4.15).
// Warunki liczy silnik reguł (src/reguly.h) z wierszy docs/spec/katalog/L.csv i H.csv; ten plik zamienia wyniki
// jednego przebiegu w stan wzorca, który trwa między przebiegami (pole PamiecReguly::stanWzorca).
//
// Konwencja wierszy katalogu L (wiersze sprawdza skrypt zestawu ze spec):
//   - wiersz główny: `wariant` bez kropki ("" albo gałąź, np. "ge10", "a", "szybki"); jego `spelniona` = warunek wejścia;
//   - wiersz pomocniczy: `wariant` z kropką "<gałąź>.<nazwa>" (np. ".srednia", "ge10.mediana"); nie jest sprawą;
//   - wiersz wyjścia: "<gałąź>.wyjscie"; gdy go nie ma, gałąź dzieli wyjście ".wyjscie" całego wzorca.
// Klucz wzorca (klucz Pamiec, kontrakt 1): "L-xx" dla gałęzi bez własnego wyjścia, "L-xx/<gałąź>" dla gałęzi z własnym
// wyjściem (L-02/a, L-02/b, L-03/ge10, L-03/5_10). Gałęzie jednego klucza (L-08 i L-08 szybki, L-11 i L-11 2tyg) to
// jeden wzorzec z pilnością najwyższą wśród spełnionych.
//
// Hipotezy H: wiersz H.csv ma `wariant` = ID wzorca, przy którym stoi (np. H-09 z wariantem L-11), albo
// "<ID>~<odmiana>" dla odmiany trybu (H-02 z L-04~autoset); "<ID>.<nazwa>" to wiersz pomocniczy. Kolejność wierszy
// = kolejność tabeli 5.12.9 (H-08 ostatnia, bo jest zapasowa). Hipoteza jest tylko przy wzorcu w stanie AKTYWNA
// (R-SKI-44: reguła „tylko raport” nie ma hipotez).
//
// Tryby: wiersze mają w polu `tryby` swoje tryby (ADR-0030); odmiany trybu wzorca to gałęzie jednego klucza (L-01
// i L-01 cpap, L-04 i L-04 autoset, L-07 i L-07 cpap), bo w danej nocy ocenia się tylko wiersz jej trybu.
//
// Noce liczone jako dni kalendarzowe między datami poranka (wiem 3 albo 7 dni, Omówione 56 nocy, wygaszenie B po 28
// dniach); liczenie nocy przy przerwach w terapii.
#pragma once
#include <string>
#include <vector>

#include "noc.h"
#include "reguly.h"

namespace wzorce {

// Stany z 5.12.12. KARENCJA: L-01 w trybach CPAP i AutoSet (R-SKI-08), gdy przy wejściu spełniony jest wiersz
// pomocniczy "<gałąź>.karencja"; kończy się w nocy L-01.karencja_noc trwania warunku albo przy wierszu
// "<gałąź>.karencja_ca" (średnia CA/h ≥ 10 / h).
enum class StanWzorca : unsigned char { NIEAKTYWNY, AKTYWNY, KARENCJA, WIEM, OMOWIONE, WYGASZONY_B, WSTRZYMANY };
// "nieaktywny", "aktywny", "karencja", "wiem", "omowione", "wygaszony_b", "wstrzymany"
const char* nazwaStanu(StanWzorca s);

// Treść pola PamiecReguly::stanWzorca. Zapis:
// `stan,pil,wejscie,od_pilnosci,ref,wyjscie,licznik,przed,biez,potw,ref_potw,raport,wiem_anul,omow_anul,dzien`
// (przecinki, bez `;`, bo plik /dane/reguly.csv rozdziela pola średnikiem). `pil`: A, B, C; P = ekran „problem” bez
// pilności lekarskiej (L-09, R5c); R = reguła „tylko raport” (bez pilności, ADR-0029 pkt 3); - = brak.
// `ref` = wartość parametru kluczowego (zmienna {x} wiersza głównego) z ostatniego przebiegu przed wyciszeniem,
// do „pogorszenia o kolejne Δ” (5.12.4, R-LEK-10, R-EKR-23); puste = nieznana. `licznik` = noce trwania warunku
// w karencji (R-SKI-08). `przed` = stan sprzed wyjścia L-01 (R-SKI-31: ponowne wejście w L-01.ponowne_wejscie nocach
// od wyjścia przywraca stan, pilność, datę wejścia i wyciszenie z pierwotną datą); puste = brak.
// `biez` = parametr kluczowy z ostatniego przebiegu; `potw` = licznik potwierdzeń „Omówione” od wejścia w pilność A,
// `ref_potw` = parametr kluczowy przy ostatnim potwierdzeniu, `raport` = data ostatniego raportu tematycznego otwartego
// z ekranu L (R-LEK-14, R-LEK-25…27). `wiem_anul`, `omow_anul` = data z pola „wiem” / „Omówione” (zapis src/sprawy.h,
// `RRRR-MM-DD@P`), która w tym wzorcu już nie działa (minęła, skasowana przez Δ, wzrost pilności albo wyjście);
// `dzien` = data ostatniego przebiegu (licznik karencji liczy noce, nie przebiegi).
struct ZapisWzorca {
  StanWzorca stan = StanWzorca::NIEAKTYWNY;
  char pil = '-';
  std::string wejscie, odPilnosci, wyjscie;  // daty poranka
  double ref = NAN;
  int licznik = 0;
  std::string przed;
  double biez = NAN;
  int potw = 0;
  double refPotw = NAN;
  std::string raport;
  std::string wiemAnul, omowAnul, dzien;
};
ZapisWzorca odczytajZapis(const std::string& pole);  // puste albo nieczytelne = nieaktywny
std::string zapiszZapis(const ZapisWzorca& z);

// Pomocnicze do konwencji katalogu.
bool wierszL(const reguly::Wynik& w);         // id zaczyna się od "L-"
bool wierszPomocniczy(const reguly::Wynik& w);  // wariant z kropką
std::string kluczWzorca(const std::vector<reguly::Wynik>& wyniki, const reguly::Wynik& glowny);

// Zdarzenia przebiegu potrzebne przejściom, których silnik nie daje wprost.
struct Okolicznosci {
  bool okresInfekcji = false;                 // 5.10.3: bramka „okres infekcji” (src/infekcja.h okresDzis())
  bool l09Sprzet = false;                     // 7.3: poduszka albo maska po terminie (src/sprzet.h blokadaL09())
  const std::vector<Noc>* historia = nullptr;  // do wykrycia zmiany ustawień po „Omówione” (R-LEK-09)
};

// Jeden przebieg rano: przejścia 5.12.12 dla każdego wzorca L z wyników `ocen()` i zapis w pamięci. Wołać po ocen(),
// przed info i sprawy::sprawyOtwarte. Wiersze ZABLOKOWANA nie mają stanu. `z` daje progi czasów (R-PRZ-03, R-LEK-09, R-SKI-09,
// R-LEK-10); bez zestawu przejścia czasowe nie działają (wzorzec zostaje w stanie wyciszonym do warunku wyjścia).
// Przejścia:
//   nieaktywny --wejście--> aktywny (pilność z katalogu; „tylko raport” = R) albo karencja (R-SKI-08)
//   karencja --noc L-01.karencja_noc trwania warunku albo średnia CA/h ≥ 10--> aktywny
//   L-01 nieaktywny --wejście w L-01.ponowne_wejscie nocach od wyjścia--> stan sprzed wyjścia (R-SKI-31)
//   aktywny/wyciszony/wygaszony/wstrzymany --wyjście (nie w nocy wejścia)--> nieaktywny, „wiem” i „Omówione” kasowane
//   wiem --3 dni (A) albo 7 dni (B, P)--> aktywny;  omówione --56 nocy albo zmiana ustawień--> aktywny
//   aktywny B --28 dni bez wzrostu pilności--> wygaszony_b
//   wiem/omówione/wygaszony --pogorszenie o Δ albo wzrost pilności--> aktywny
//   aktywny L-03, L-09 --okres infekcji--> wstrzymany --koniec okresu--> aktywny
//   „nie oceniam” (wiersz główny nieoceniony): brak wejścia; aktywny trwa, wyjście liczone dalej z wiersza wyjścia
void aktualizuj(const std::vector<reguly::Wynik>& wyniki, reguly::Pamiec& pamiec, const std::string& dzis,
                const reguly::Zestaw* z, const Okolicznosci& o = {});

// „Wiem” i „Omówione z lekarzem”: zapis pola PamiecReguly::wiem / omowione robi (src/sprawy.h zapiszWiem,
// zapiszOmowione(), format `RRRR-MM-DD@P`); aktualizuj() czyta te pola i liczy z nich stany 5.12.12.
// wiem() i omowione() poniżej to cienkie wywołania sprawy::zapisz…() z kontrolą stanu wzorca i licznikiem A.
// wiem(): tylko wzorzec aktywny A, B albo P (reguła AKTYWNA z ekranem); wraca po 3 dniach (A) albo 7 (B, P), przy
// pogorszeniu o Δ albo wzroście pilności.
bool wiem(reguly::Pamiec& pamiec, const std::string& klucz, const std::string& dzis);
// omowione(): tylko aktywny A albo B; milknie na 56 nocy albo do zmiany ustawień; wraca wcześniej przy pogorszeniu
// o Δ albo wzroście pilności.
// Przy pilności A (R-LEK-24…27): pierwsze potwierdzenie i potwierdzenie z poprawą parametru kluczowego o Δ
// (5.12.4, „Pogorszenie o kolejne Δ”) wyciszają pełny ekran na 56 nocy i zaczynają licznik od nowa; drugie bez poprawy
// na 28 nocy (R-LEK-25); trzecie i kolejne bez poprawy: pełny ekran co 14 nocy od potwierdzenia (R-LEK-27,
// InfoWzorca::pelnyEkranDzis); drugie i kolejne bez poprawy tylko po raporcie tematycznym otwartym z ekranu L po
// poprzednim potwierdzeniu (R-LEK-26, raportZEkranuL()). Licznik kasuje spadek pilności poniżej A i wyjście wzorca.
// `z` daje Δ i okresy; bez zestawu „bez poprawy” (zachowawczo: nic nie chowa).
bool omowione(reguly::Pamiec& pamiec, const std::string& klucz, const std::string& dzis,
              const reguly::Zestaw* z = nullptr);
// Wariant ekranu Q-omowione (5.12.7) dla wzorca teraz; src/sprawy.cpp wybiera po nim zdania i podpisy przycisków.
enum class WariantOmowienia : unsigned char {
  NIEDOSTEPNE,            // nie A ani B w stanie aktywnym (albo A wyciszone bez dnia pełnego ekranu)
  ZWYKLE,                 // B: `Panel przestanie o tym przypominać na 8 tygodni albo do zmiany ustawień.`
  A_PIERWSZE,             // A pierwsze albo z poprawą: `Pełny ekran zniknie na 8 tygodni, przypomnienie co 3 dni zostanie.`
  A_DRUGIE_BEZ_POPRAWY,   // `Drugi raz bez poprawy: pełny ekran zniknie na 4 tygodnie.`
  A_KOLEJNE_BEZ_POPRAWY,  // `Znowu bez poprawy: pełny ekran co 2 tygodnie.`
  NAJPIERW_RAPORT         // `Najpierw nowy raport: ▶ 2 s na ekranie L.` (▶ nic nie zapisuje, R-LEK-26)
};
WariantOmowienia wariantOmowienia(const reguly::Pamiec& pamiec, const std::string& klucz, const std::string& dzis,
                                  const reguly::Zestaw* z = nullptr);
// Raport tematyczny otwarty z ekranu L (▶, R-LEK-14): dyrygent woła przy otwarciu; liczy się do R-LEK-26.
void raportZEkranuL(reguly::Pamiec& pamiec, const std::string& klucz, const std::string& dzis);

// ---------- stan wzorca dla „Otwartych spraw” i ekranu L (src/sprawy.*) ----------
// Listę spraw, kolejność i teksty robi (sprawy::sprawyOtwarte); stąd bierze stan wzorca jednego klucza.
enum class Karta : unsigned char {
  PELNY_EKRAN,  // wzorzec A albo B: tytuł i powód ekranu
  PROBLEM,      // L-09 (R5c): tytuł i powód ekranu „problem”
  NA_KONTROLE,  // pilność C: tytuł `Sprawa na kontrolę`, powód = linia C bez `Na kontroli: raport 2 / 6.`
  W_RAPORCIE    // „tylko raport” ze spełnionym warunkiem: tytuł `Sprawa w raporcie`, wiersz `Dalej: w raporcie, bez pilności`
};
struct InfoWzorca {
  std::string klucz;               // klucz Pamiec ("L-03/ge10", "L-08", …)
  std::string id, galaz;           // "L-03", "ge10"
  std::string idTytulu;            // id_tekstu tytułu ekranu (teksty.csv); dla C bez ekranu: ""
  std::string idPowodu;            // id_tekstu powodu albo linii C (Karta mówi, jak go pokazać)
  reguly::Pilnosc pilnosc = reguly::Pilnosc::BRAK;  // BRAK dla „tylko raport” i L-09
  reguly::StanReguly stanReguly = reguly::StanReguly::AKTYWNA;
  StanWzorca stan = StanWzorca::NIEAKTYWNY;
  Karta karta = Karta::PELNY_EKRAN;
  std::string wejscie;             // data wejścia
  std::string wiemDo;              // pierwszy dzień po okresie „wiem” (`Dalej: wraca do wyboru {data}`), "" = brak
  std::string omowioneOd;          // data „Omówione” (`Ty: omówione z lekarzem {data}`), "" = brak
  std::string hipoteza;            // id H do ramki ekranu (hipoteza()), "" = bez ramki
  int potwierdzenia = 0;           // „Omówione” od wejścia w pilność A (R-LEK-25…27)
  bool pelnyEkranDzis = false;     // A po trzecim i kolejnym „Omówione” bez poprawy: dziś dzień pełnego ekranu (co 14 nocy)
  const reguly::Wynik* wynik = nullptr;  // wiersz główny (zmienne do tekstów); żyje tyle co wektor wyników
};
// Klucze wzorców L (bez ZABLOKOWANA) w kolejności katalogu.
std::vector<std::string> klucze(const std::vector<reguly::Wynik>& wyniki);
// Stan wzorca `klucz` (po aktualizuj()); gdy pamięci jeszcze nie ma, a wejście jest spełnione: aktywny od dziś.
// Nieaktywny i „nie oceniam” = StanWzorca::NIEAKTYWNY (nie jest otwartą sprawą, R-SKI-44, R-NOR-11).
InfoWzorca info(const std::vector<reguly::Wynik>& wyniki, const reguly::Pamiec& pamiec, const std::string& klucz,
                const std::string& dzis, const reguly::Zestaw* z = nullptr);

// Hipoteza do ramki przy wzorcu `idL` (5.12.9, R-HIP-01): pierwszy wiersz H z wariantem `idL` albo `idL~…`, w stanie AKTYWNA,
// oceniony i spełniony; "" gdy żadnej albo gdy wiersz główny `idL` nie jest w stanie AKTYWNA (R-SKI-44).
// Wyłączenie podpowiedzi (R-HIP-02) sprawdza src/sprawy.cpp.
std::string hipoteza(const std::string& idL, const std::vector<reguly::Wynik>& wyniki);

// Klucze wzorców L (bez ZABLOKOWANA), których wiersz główny jest „nie oceniam”, z powodem: do logu i do raportu
// (`{tytuł}: nie oceniam.`, R-SKI-44). Nigdy nie znaczy „w porządku” (ADR-0029 pkt 7).
std::vector<std::pair<std::string, std::string>> nieocenione(const std::vector<reguly::Wynik>& wyniki);

// Rodzaje warunków spoza rdzenia (src/rodzaje_l.cpp): `blisko_ustawienia`, `wartosc_ustawienia`, `regularnosc`,
// `odpowiedzi_kolejne`, `statystyka_okna_min`, `odpowiedzi_najwyzej`, `trwanie_tygodni`, `porownanie_przed_po`.
// Wołać raz przed wczytaniem zestawu (wczytajZestaw odrzuca nieznany rodzaj); kolejne wywołania nic nie robią.
void zarejestrujRodzajeL();

}  // namespace wzorce
