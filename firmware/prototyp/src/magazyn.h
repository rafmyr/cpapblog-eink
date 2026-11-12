// SPDX-License-Identifier: GPL-3.0-or-later
// Magazyn danych.
// Rdzeń bez Arduino: rejestr formatów plików, nagłówek `#cpap-eink {nazwa} {wersja}`, wiersz nazw pól, wiersz
// `#koniec {n}` w plikach przepisywanych w całości, zapis atomowy przez `{plik}.tmp` na prostych operacjach adaptera
// (src/adaptery.h Pliki::zapisz(), czytaj(), usun(), zmienNazwe()), wybór pliku tymczasowego przy odczycie (T-WYM-53),
// pliki z nowszej wersji tylko do odczytu (T-WYM-54), migracje krok po kroku (T-WYM-55, T-WYM-56) i NVS z zapisem
// tylko przy zmianie (T-WYM-58).
//
// Interfejs jest najmniejszy, jakiego potrzebują testy (test/testy_magazyn_danych.cpp, napisane przed kodem);
// rozszerzenie jest dozwolone, zmiana kontraktu wymaga zmiany testów i opisu.
//
// Formaty wersji 1 (R2 karty): dokładnie pola prototypu w kolejności z komentarzy src/noc.h, src/czynniki.h,
// src/sypialnia.h, src/humor.h, src/bateria.h, src/airbridge.h, plus pole `import` na końcu rekordu nocy (R1):
//   noce        /dane/noce/{RRRR}.csv      ← /noce.csv           przepisanie  data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;
//                                                                            emed;ep95;psmed;psp95;ustawienia;zrodlo;
//                                                                            czesciowa;epap_str;oa;ca;h;csr;import
//   czynniki    /dane/czynniki.csv         ← /czynniki.csv       przepisanie  data;stan;kody;warstwa;termin;pokazano
//   sypialnia   /dane/sypialnia/{RRRR}.csv ← /sypialnia.csv      przepisanie  data;temp;wilg;co2
//                                            + /sypialnia-co2.csv (połączone po dacie)
//   humor       /dane/humor.csv            ← /humor.csv          dopisanie    data;typ;klucz;tryb;scena;zdanie
//   wybudzenia  /log/wybudzenia.csv        ← /bateria.csv        dopisanie    epoch;mV;wifi_ms;aktywny_ms;powod
//   pominiete   /dane/ab-pominiete.csv     ← /ab_pominiete.txt   przepisanie  day
//   proby       /dane/ab-proby.csv         ← /ab_proby.txt       przepisanie  day;proby;ostatnia;stale
//   reguly      /dane/reguly.csv           (bez wersji 0)        przepisanie  id;wariant;aktywna_od;ostatnio;licznik;
//                                                                            stan_wzorca;wiem;omowione
//   porownanie  /dane/porownanie.csv       (bez wersji 0)        przepisanie  data;werdykt;rozjazd;pominiete;przyczyna
//   portret     /dane/portret.csv          (bez wersji 0)        przepisanie  data;rodzaj;od;do;wyciek95;wyc_s;zew;centr;ps;
//                                                                            maks;braki (Portret nocy, src/portret.h)
// `{RRRR}` w ścieżce = rok daty poranka (pierwsze pole wiersza).
// Od `noce` ma wersję 2: pola wersji 1 i pola (src/noc.h wierszNocyV2, POLA_NOCY_V2); krok 1 → 2 to
// ogólne dopisanie pustych pól (migrujWMiejscu()), a scalenie i przywracanie zapisują plik czekający na ten krok w jego
// własnej wersji (T-WYM-43).
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "adaptery.h"

enum class RodzajZapisu : uint8_t {
  PRZEPISANIE,  // cały plik przez `.tmp`, na końcu `#koniec {n}` (T-WYM-51, T-WYM-52)
  DOPISANIE,    // dopisywanie wiersza, bez `#koniec`; urwany ostatni wiersz pomijany przy odczycie
};

// Jeden format pliku w rejestrze.
struct FormatPliku {
  std::string nazwa;      // nazwa w nagłówku, np. "noce"
  std::string sciezka;    // ścieżka wersji ≥ 1; "{RRRR}" = rok (pliki roczne), np. "/dane/noce/{RRRR}.csv"
  std::string sciezkaV0;  // plik prototypu (wersja 0, bez nagłówka), np. "/noce.csv"; "" = format bez wersji 0
  RodzajZapisu rodzaj = RodzajZapisu::PRZEPISANIE;
  // pola[w - 1] = nazwy pól wersji w (w ≥ 1); wersja znana firmware = pola.size(). Każda lista jest początkiem
  // następnej (T-WYM-57), więc krok n → n+1 to dopisanie pustych pól na końcu wiersza.
  std::vector<std::vector<std::string>> pola;
  int wersja() const { return (int)pola.size(); }
};

// Rejestr formatów tej wersji firmware (tabela wyżej). Zwraca zawsze ten sam obiekt.
const std::vector<FormatPliku>& rejestrFormatow();
// Format o nazwie `nazwa` z rejestru `rejestr` albo nullptr.
const FormatPliku* formatPliku(const std::vector<FormatPliku>& rejestr, const std::string& nazwa);

enum class StanPliku : uint8_t {
  BRAK,        // nie ma ani pliku, ani kompletnego `.tmp`
  DOBRY,       // nagłówek z wersją znaną firmware, przy przepisywanych `#koniec {n}` zgodny z liczbą wierszy
  NOWSZY,      // wersja wyższa niż znana: wiersze obcięte do pól znanej wersji, plik tylko do odczytu (T-WYM-54)
  USZKODZONY,  // przepisywany bez `#koniec` albo z inną liczbą wierszy, zły nagłówek; wiersze puste
};

struct OdczytPliku {
  StanPliku stan = StanPliku::BRAK;
  int wersja = -1;                   // wersja z nagłówka; -1 = brak nagłówka albo pliku
  std::vector<std::string> wiersze;  // wiersze danych (bez nagłówka, nazw pól i `#koniec`), pola znanej wersji
};

struct WynikMigracji {
  std::string plik;    // nazwa pliku źródłowego bez ukośnika, np. "noce.csv"
  bool ok = false;
  int przed = 0, po = 0;  // liczba rekordów przed i po migracji
  std::string powod;   // przy błędzie: powód do logu i ekranu testu (`migracja {plik}: błąd`)
};

class Magazyn {
 public:
  // `pliki`: zamontowana partycja; `log`: jeden wiersz logu (port serwisowy) na błąd zapisu, odczytu i migracji;
  // `rejestr`: formaty tej wersji firmware (testy podają rejestr z testową wersją 2). Obiekty muszą żyć dłużej.
  Magazyn(Pliki& pliki, std::function<void(const std::string&)> log,
          const std::vector<FormatPliku>& rejestr = rejestrFormatow());

  // Odczyt pliku formatu `format` spod `sciezka`. Przy obecnym `{sciezka}.tmp` (T-WYM-53): kompletny `.tmp` zastępuje
  // brakujący albo uszkodzony plik docelowy (zmiana nazwy); w każdym innym przypadku `.tmp` jest usuwany. Oba
  // uszkodzone → USZKODZONY i wpis w logu z nazwą pliku (bez karty: praca bez pliku, R4 karty). Plik dopisywany
  // bez `\n` na końcu: urwany ostatni wiersz pominięty. Plik bez nagłówka (wersja 0) nie jest czytany tą funkcją:
  // stan USZKODZONY. Wersja wyższa niż znana → NOWSZY, a plik zostaje tylko do odczytu do końca pracy (tylkoDoOdczytu()).
  // Rozszerzenie implementacji uszkodzony plik docelowy (bez kompletnego `.tmp`) dostaje nazwę
  // `{sciezka}.uszkodzony`, żeby następny zapis zaczął plik od nowa, a treść została do ręcznego odzyskania; wcześniejsze
  // kopie przesuwają się na `.uszkodzony.1`, `.uszkodzony.2` (najwyżej KOPII_USZKODZONYCH najnowszych); wersja niższa niż znana → DOBRY z tą wersją, plik czeka na migrujWMiejscu().
  OdczytPliku czytaj(const std::string& format, const std::string& sciezka);

  // Zapis atomowy pliku przepisywanego: nagłówek z wersją znaną firmware, wiersz nazw pól, `wiersze`, `#koniec {n}`,
  // przez `{sciezka}.tmp`, kontrola liczby zapisanych bajtów, zmiana nazwy (T-WYM-51). Przy niezgodnej liczbie bajtów
  // albo błędzie: `.tmp` usunięty, poprzednia treść bajt w bajt, wpis w logu, false. Plik tylko do odczytu: false
  // bez zmian. Rozszerzenia implementacji wiersz z mniejszą liczbą pól niż znana wersja dostaje puste pola na
  // końcu; wiersz ze znakiem końca linii → false bez zapisu; plik czekający na migrację w miejscu (przeczytany
  // w starszej wersji) → false bez zapisu (odrzuconoZapisStarszego()); format dopisywany → to samo przepisanie
  // atomowe bez `#koniec` (przycięcie dziennika).
  bool zapisz(const std::string& format, const std::string& sciezka, const std::vector<std::string>& wiersze);
  // Dopisanie wiersza do pliku dopisywanego; brak pliku → nowy z nagłówkiem i nazwami pól. Urwany ostatni wiersz nie
  // skleja się z nowym. false przy błędzie albo pliku tylko do odczytu. Rozszerzenie implementacji wersję pliku
  // sprawdza pierwszy wiersz (nowsza → tylko do odczytu, starsza → odrzucone jak w zapisz()); plik bez czytelnego
  // nagłówka dostaje nazwę `{sciezka}.uszkodzony` (kopie jak w czytaj()) i zaczyna się od nowa (nagłówek, nazwy pól,
  // `wiersz`), wpis w logu.
  bool dopisz(const std::string& format, const std::string& sciezka, const std::string& wiersz);

  // Migracje wersji 0 → 1 na nowe ścieżki wszystkich plików z rejestru, które mają plik prototypu (`sciezkaV0`)
  // (T-WYM-43, R9 karty): rekordy parserem wersji 0, zapis pod nową ścieżką, porównanie liczby rekordów; plik
  // źródłowy nietknięty. Przy błędzie danego pliku: wynik ok = false, wpis w logu, częściowe pliki wynikowe usunięte,
  // pozostałe pliki migrują dalej. Zwraca true, gdy wszystkie udane (albo nie było czego migrować). Wyniki: migracje().
  // Stan migracji każdego formatu pamięta plik znacznika
  // znacznikMigracji(nazwa): `w toku` zapisany przed pierwszym plikiem nowej ścieżki, `ok {crc}` (CRC-32 treści plików
  // prototypu) po porównaniu liczby rekordów. Przy starcie:
  //  - brak znacznika i brak plików nowej ścieżki: migracja;
  //  - `w toku` (zanik zasilania w trakcie): pliki nowej ścieżki tego formatu usunięte, migracja od nowa ze źródła;
  //  - `ok` z tym samym CRC: nic (źródło się nie zmieniło);
  //  - `ok` z innym CRC albo pliki nowej ścieżki bez znacznika (np. poprzednia wersja po powrocie przez bootloader
  //    dopisała noce do `/noce.csv`): scalenie źródła z plikami nowej ścieżki (scalWiersze()), potem `ok {nowy crc}`.
  // `sprawdzCrc` = false (wybudzenie z uśpienia: plik prototypu zmienia tylko poprzednia wersja
  // firmware, czyli przed startem) → format ze znacznikiem `ok …` pominięty bez czytania plików prototypu i liczenia
  // CRC; pozostałe przypadki (brak znacznika, `w toku`) jak wyżej. Dyrygent podaje true tylko przy starcie.
  bool migrujNaNoweSciezki(bool sprawdzCrc = true);
  // Migracje w miejscu (wersja n ≥ 1 → wersja znana, krok po kroku) plików pod nowymi ścieżkami: kopia
  // `{sciezka}.v{n}`, porównanie liczby rekordów, zamiana; po udanej migracji usuwa kopie starszych wersji tego
  // pliku, także plik źródłowy wersji 0 (np. `/noce.csv`), ale tylko gdy migracja 0 → 1 tego formatu jest potwierdzona
  // (znacznik `ok` z CRC obecnego źródła). Wolno wołać dopiero po oznaczeniu wersji jako
  // dobrej (T-WYM-43). Zwraca true, gdy wszystkie udane. Wyniki: migracje().
  bool migrujWMiejscu();
  // Wyniki migracji z ostatniego wywołania migrujNaNoweSciezki() albo migrujWMiejscu() (tylko pliki, które migrowały).
  const std::vector<WynikMigracji>& migracje() const;
  // true, gdy plik pod `sciezka` przeczytano w wersji nowszej niż znana (do końca pracy bez zapisu, T-WYM-54).
  bool tylkoDoOdczytu(const std::string& sciezka) const;

  // ---------- rozszerzenia implementacji; karta, „Implementacja ”) ----------
  // Ścieżki plików formatu `format`: dla plików rocznych ("{RRRR}") istniejące pliki `{katalog}/RRRR.csv` (rok
  // z czterech cyfr, bez `.tmp`, kopii `.v{n}` i `.uszkodzony`) rosnąco; dla pozostałych jedna ścieżka z rejestru
  // (także gdy pliku nie ma). Nieznany format albo partycja niezamontowana: pusta lista.
  std::vector<std::string> sciezki(const std::string& format);
  // Ścieżka pliku formatu `format` dla roku `rok` (cztery cyfry, np. "2026"); plik bez "{RRRR}": ścieżka z rejestru.
  // Nieznany format: "".
  std::string sciezkaRoku(const std::string& format, const std::string& rok) const;
  // Po nieudanym samosprawdzeniu (powrót do poprzedniej wersji, T-WYM-41): usuwa pliki i znaczniki zapisane przez
  // ostatnie migrujNaNoweSciezki() tego obiektu, żeby następna próba nowej wersji przeniosła pliki prototypu na nowo
  // (z nocami, które poprzednia wersja dopisze do `/noce.csv` po powrocie). Pliki źródłowe zostają nietknięte. Powrót
  // przez bootloader (bez tego wywołania) obsługuje znacznik migracji (scalenie przy następnym starcie, U6).
  void wycofajMigracje();
  // Ścieżki przeczytane w wersji nowszej niż znana (ekran testu: `dane z nowszej wersji: {plik}`).
  const std::vector<std::string>& plikiTylkoDoOdczytu() const { return tylkoOdczyt_; }
  // true, gdy od utworzenia obiektu zapis albo dopisanie trafiło na plik w wersji starszej niż znana, który czeka na
  // migrację w miejscu (T-WYM-55: migracja przed pierwszym zapisem, więc zapis odrzucony). Dyrygent po udanej
  // migrujWMiejscu() zapisuje wtedy dane z pamięci jeszcze raz.
  bool odrzuconoZapisStarszego() const { return odrzuconoStarszy_; }

  // ---------- rozszerzenia implementacji: okno przywracania i przywrócenie, T-WYM-87, T-WYM-62) ----------
  // Zapis atomowy treści bez formatu (plik przyjęty komendą `W` do `/przywracanie/`, T-WYM-51): `{sciezka}.tmp`,
  // kontrola liczby bajtów, zmiana nazwy. Przy błędzie `.tmp` usunięty, poprzednia treść bez zmian, wpis w logu, false.
  bool zapiszSurowy(const std::string& sciezka, const std::string& tresc);
  // true, gdy partycja nie ma żadnego pliku nocy: ani pliku rocznego `/dane/noce/RRRR.csv`, ani pliku prototypu
  // `/noce.csv` (T-WYM-62, T-WYM-87: „partycja plików bez plików nocy”). Partycja niezamontowana: false.
  bool bezPlikowNocy();
  // Przywrócenie ze zrzutu prototypu w katalogu `katalog` (np. "/przywracanie"; pliki wersji 0 pod nazwami z
  // PLIKI_ZRZUTU, 8.9 krok 1), T-WYM-62: rekordy parserami wersji 0 (te same co migracja na nowe ścieżki).
  // Zasady:
  //  - scalenie, nie zastąpienie: każdy plik wynikowy = scalWiersze(wiersze we flash, wiersze zrzutu), więc noce
  //    pobrane z AirBridge przed przywróceniem i poprzednie przywrócenia zostają (przywrócenie można powtórzyć);
  //  - liczba: rekordy nocy zrzutu, których data po zapisie jest w plikach nocy („przyjęte albo już obecne”), równa
  //    liczbie niepustych wierszy pliku nocy zrzutu; plik nocy bez `\n` na końcu (urwany) albo z wierszem
  //    nieczytelnym = niezgodna liczba bez zapisu;
  //  - atomowo względem całego zestawu: najpierw treść wszystkich plików wynikowych do `{plik}.tmp`, dopiero gdy
  //    wszystkie się zapisały, zmiany nazw; błąd zapisu w połowie usuwa tylko pliki `.tmp`, istniejące pliki bez zmian;
  //  - co najmniej jeden plik zrzutu w katalogu, ale bez pliku nocy: proba = true, wpis `przywracanie: niezgodna
  //    liczba nocy (0 z ?, brak pliku nocy)` (T-WYM-87 weryfikacja);
  // - plik nocy bez żadnej nocy: niezgodna liczba `(0 z 0, brak nocy w pliku nocy)`, bez zapisu.
  // Wpis `przywrócono {n} z {m}` przy zgodnej liczbie, inaczej `przywracanie: niezgodna liczba nocy …`. Pliki
  // w `katalog` nigdy się nie zmieniają. Bez żadnego pliku zrzutu w katalogu: proba = false, bez wpisu.
  struct WynikPrzywracania {
    bool proba = false;  // w katalogu był co najmniej jeden plik zrzutu, więc przywracanie się odbyło
    bool ok = false;
    int oczekiwane = 0;  // niepuste wiersze pliku nocy zrzutu
    int przywrocone = 0; // rekordy nocy zrzutu obecne po zapisie (albo przeczytane parserem wersji 0 bez zapisu)
    int nowych = 0;      // z tego noce, których przed przywróceniem nie było we flash
    std::string powod;   // przy błędzie: krótki powód do logu i ekranu testu
  };
  WynikPrzywracania przywrocZKatalogu(const std::string& katalog);

 private:
  // `wersja`: wersja nagłówka i nazw pól (0 = znana firmware; scalenie pliku czekającego na migrację w miejscu)
  bool zapiszCaly(const FormatPliku& f, const std::string& sciezka, const std::vector<std::string>& wiersze,
                  int wersja = 0);
  bool migrujPlikNaNowa(const FormatPliku& f, WynikMigracji& w, uint32_t crcZrodla);
  bool scalZrodlo(const FormatPliku& f, WynikMigracji& w, uint32_t crcZrodla);
  bool crcZrodel(const FormatPliku& f, uint32_t& crc);
  bool zrodloPotwierdzone(const FormatPliku& f);
  std::vector<std::string> plikiNowejSciezki(const FormatPliku& f);
  std::string odlozUszkodzony(const std::string& sciezka);
  bool migrujPlikWMiejscu(const FormatPliku& f, const std::string& sciezka, WynikMigracji& w);
  void loguj(const std::string& s) {
    if (log_) log_(s);
  }
  bool starszy(const std::string& sciezka) const;
  void bezStarszego(const std::string& sciezka);
  Pliki& pliki_;
  std::function<void(const std::string&)> log_;
  const std::vector<FormatPliku>& rejestr_;
  std::vector<WynikMigracji> migracje_;
  std::vector<std::string> tylkoOdczyt_;
  std::vector<std::string> starsze_;     // przeczytane w wersji starszej niż znana (czekają na migrujWMiejscu())
  std::vector<std::string> utworzone_;   // pliki i znaczniki zapisane przez ostatnie migrujNaNoweSciezki()
  bool odrzuconoStarszy_ = false;
};

// Najwięcej kopii `.uszkodzony` jednego pliku: `.uszkodzony` (najnowsza), `.uszkodzony.1`,
// `.uszkodzony.2`; przy następnej znika najmniejsza z nich, a starsze
// od niej zachowują numery [PRÓG].
const int KOPII_USZKODZONYCH = 3;
// Plik znacznika migracji 0 → 1 formatu `nazwa` na nowe ścieżki: `/dane/.migracja-{nazwa}`.
std::string znacznikMigracji(const std::string& nazwa);
// Scalenie wierszy formatu `nazwa` (wersja znana firmware) z pamięci flash (`flash`) i z drugiego źródła (`zrodlo`:
// zrzut przywracania albo plik prototypu dopisany po migracji). Klucz = pierwsze pole (data poranka, data wpisu, czas
// unix); wynik posortowany po kluczu (krótszy klucz najpierw, potem znak po znaku), w obrębie klucza kolejność
// z pliku. Reguła (U2 wariant A; uzasadnienie: karta D40):
//  - klucz tylko w jednym źródle: wiersze z tego źródła;
//  - klucz w obu: wiersze z flash (rekord z AirBridge, odpowiedź czynników, wpis dziennika zapisane już na panelu),
//    z wyjątkami:
//    - noc we flash pusta (bez minut z maską) albo częściowa (`czesciowa` = 1), a w drugim źródle pełna → noc
//      z drugiego źródła z polem `import` z flash;
// - czynniki: wpis z odpowiedzią (`O`) wygrywa z wpisem bez
//      odpowiedzi (`C` czeka, `B` brak odpowiedzi); dwa `O` albo dwa bez odpowiedzi → flash;
//    - dziennik wyświetleń (`humor`, kilka wierszy na dobę): suma wierszy obu źródeł bez powtórzeń całego wiersza
//      (najpierw wiersze z flash, potem nowe z drugiego źródła). Dziennik wybudzeń ma klucz jednoznaczny (chwila
//      wybudzenia), więc jak pozostałe: flash.
// `dodane` (może być nullptr): liczba wierszy z drugiego źródła, które weszły do wyniku.
std::vector<std::string> scalWiersze(const std::string& nazwa, const std::vector<std::string>& flash,
                                     const std::vector<std::string>& zrodlo, int* dodane = nullptr);

// Nazwa pliku do logu i ekranu testu: ścieżka bez przedrostka `/dane/` albo `/` (np. "/dane/noce/2026.csv" →
// "noce/2026.csv", "/dane/czynniki.csv" → "czynniki.csv", "/log/wybudzenia.csv" → "log/wybudzenia.csv").
std::string nazwaPliku(const std::string& sciezka);
// Pierwsze `ile` pól wiersza CSV z separatorem `;` (wiersz z mniejszą liczbą pól bez zmian). Dyrygent tnie nim wiersze
// do pól, które rozumie jego parser (pola wersji 1), także przy rejestrze znającym wersję 2.
std::string pierwszePola(const std::string& wiersz, size_t ile);

// NVS z zapisem tylko przy zmianie wartości (T-WYM-58): warstwa nad adapterem, przez którą dyrygent pisze do NVS.
// Odczyty przekazuje bez zmian; każdy zapis najpierw czyta obecną wartość tego klucza i woła adapter tylko przy
// różnicy (także brak klucza = różnica). usun() zawsze przekazuje.
class NvsTylkoZmiany : public Nvs {
 public:
  explicit NvsTylkoZmiany(Nvs& n) : n_(n) {}
  std::string tekst(const char* przestrzen, const char* klucz, const std::string& domysl) override;
  bool ustawTekst(const char* przestrzen, const char* klucz, const std::string& v) override;
  uint32_t liczbaU(const char* przestrzen, const char* klucz, uint32_t domysl) override;
  void ustawLiczbeU(const char* przestrzen, const char* klucz, uint32_t v) override;
  int32_t liczba(const char* przestrzen, const char* klucz, int32_t domysl) override;
  void ustawLiczbe(const char* przestrzen, const char* klucz, int32_t v) override;
  bool flaga(const char* przestrzen, const char* klucz, bool domysl) override;
  void ustawFlage(const char* przestrzen, const char* klucz, bool v) override;
  size_t bajty(const char* przestrzen, const char* klucz, void* cel, size_t rozmiar) override;
  void ustawBajty(const char* przestrzen, const char* klucz, const void* zrodlo, size_t rozmiar) override;
  void usun(const char* przestrzen, const char* klucz) override;
  // T-WYM-54 dla NVS (rozszerzenie implementacji): przestrzeń z kluczem `wersja` wyższym niż znana firmware jest
  // tylko do odczytu do końca pracy; każdy zapis i usunięcie w niej są pomijane. `przestrzen` musi żyć dłużej.
  void tylkoDoOdczytu(const char* przestrzen);

 private:
  bool zablokowana(const char* przestrzen) const;
  Nvs& n_;
  std::vector<std::string> zablokowane_;
};

// Klucz `wersja` w każdej przestrzeni NVS (T-WYM-50), zapisywany przez ustawLiczbeU(); wersje tego wydania.
const char* const NVS_KLUCZ_WERSJA = "wersja";
const uint32_t WERSJA_NVS_CPAP = 1;
const uint32_t WERSJA_NVS_NOC = 1;
// NVS `cpap`: znacznik pierwszego formatowania (tekst znacznikPartycji()) i identyfikator układu (liczbaU), T-WYM-84.
const char* const NVS_KLUCZ_FS_INIT = "fs_init";
const char* const NVS_KLUCZ_UKLAD = "uklad";
// NVS `cpap`:
//  - `przywr`: koniec przywracania, okno się już nie otwiera: udane przywrócenie (tekst `{data albo ?} {liczba nocy}`)
//    albo okno zakończone bez żadnej komendy `W` (tekst ZNACZNIK_BEZ_PRZYWRACANIA ` {data albo ?}`); brak = przywracanie
//    czeka, okno przy starcie zasilania;
//  - `przywr_odrz`: pliki zrzutu odrzucone w oknie i nie przyjęte później (nazwy po przecinku), zapis przy każdej
//    odpowiedzi na `W` w oknie, która listę zmienia;
//  - `przywr_k`: okno zakończone komendą `K` przy komplecie plików (1); kasowany przy otwarciu następnego okna;
//  - `przywr_proba`: w którymś oknie przyszła komenda `W` (1; przywracanie było próbowane, więc okno wraca do skutku).
// Wszystkie kasowane przy pierwszym formatowaniu.
const char* const NVS_KLUCZ_PRZYWR = "przywr";
const char* const NVS_KLUCZ_PRZYWR_ODRZ = "przywr_odrz";
const char* const NVS_KLUCZ_PRZYWR_K = "przywr_k";
const char* const NVS_KLUCZ_PRZYWR_PROBA = "przywr_proba";
const char* const ZNACZNIK_BEZ_PRZYWRACANIA = "bez przywracania";
// D52 karty (integracja): chwila (epoch) ostatniego udanego pobrania kompletnej nocy z AirBridge, wiersz U-05
// `Ostatnie pobranie` (T-WYM-39); w NVS, bo przywrócenie ze zrzutu daje noce bez pola `import`.
const char* const NVS_KLUCZ_POBRANIE_NOCY = "pobr_nocy";

// Identyfikator układu partycji wkompilowanego w firmware (firmware/partycje-e1001.csv), w tej samej postaci co
// OpisPartycji::uklad z adaptera. Różny od zera. Na panelu: stała UKLAD_PARTYCJI, którą skrypt budowy
// (firmware/prototyp/uklad_partycji.py) liczy identyfikatorUkladu() z pliku `board_build.partitions`; na komputerze
// (testy, symulator): identyfikatorUkladu() tablicy 8.2 wpisanej w src/magazyn.cpp, zgodność z plikiem w repozytorium
// sprawdza test testT_WYM_38_identyfikatorUkladu.
uint32_t ukladWkompilowany();
// Tekst znacznika `fs_init` dla partycji `p` (przesunięcie i rozmiar). Ten sam tekst dla tej samej partycji, różny
// dla innego przesunięcia albo rozmiaru; niepusty.
std::string znacznikPartycji(const OpisPartycji& p);

// Jeden wiersz tablicy partycji (jak esp_partition_t: etykieta, typ, podtyp, adres, rozmiar).
struct WpisPartycji {
  std::string nazwa;
  uint8_t typ = 0;     // 0 = app, 1 = data (inne typy pomijane w identyfikatorze)
  uint8_t podtyp = 0;  // np. 0x02 nvs, 0x10 ota_0, 0x82 spiffs
  uint32_t przesuniecie = 0, rozmiar = 0;
};
// Identyfikator tablicy partycji: CRC-32 tekstu „{nazwa} {typ} {podtyp} 0x{adres} 0x{rozmiar}\n” wierszy typu app
// i data posortowanych po adresie (liczby szesnastkowe małymi literami). Ten sam algorytm liczy adapter ESP32 z tablicy
// panelu (esp_partition_find) i skrypt budowy z pliku CSV, więc tablica panelu = tablica firmware ⇔ równe
// identyfikatory. Pusta lista: 0; wynik CRC równy 0 zamieniony na 1.
uint32_t identyfikatorUkladu(std::vector<WpisPartycji> tablica);
// Tablica partycji z tekstu CSV w formacie ESP-IDF (`nazwa, typ, podtyp, przesunięcie, rozmiar[, flagi]`; typ i podtyp
// słowami albo liczbą; liczby dziesiętne albo 0x…, z przyrostkiem K albo M; `#` = komentarz). Zwraca false przy
// wierszu nieczytelnym, nieznanej nazwie typu albo podtypu, pustym przesunięciu albo pustej tablicy.
bool tablicaZCsv(const std::string& csv, std::vector<WpisPartycji>& tablica);

// ---------- pierwsze formatowanie (T-WYM-84, R7 karty) ----------
const size_t BLOK_PARTYCJI = 4096;  // blok LittleFS w rdzeniu Arduino; T-WYM-84 czyta dwa pierwsze bloki
struct StanPartycji {
  bool zamontowana = false;
  bool ukladZgodny = true;      // OpisPartycji::uklad z adaptera == ukladWkompilowany() (R7)
  std::string znacznikNvs;      // `cpap`/`fs_init` ("" = brak)
  std::string znacznikBiezacy;  // znacznikPartycji() bieżącej partycji plików
  uint32_t ukladNvs = 0;        // `cpap`/`uklad` (0 = brak)
  bool blokiCzytelne = false;   // czytajPartycje() się udało
  std::string bloki;            // dwa pierwsze bloki partycji (2 × BLOK_PARTYCJI)
};
enum class DecyzjaPartycji : uint8_t {
  ZAMONTOWANA,  // zwykła praca
  STARY_UKLAD,  // tablica panelu ≠ wkompilowana (R7): bez formatowania, bez migracji, bez zapisu
  FORMATUJ,     // partycja nowa (T-WYM-84): formatowanie raz, znacznik w NVS, okno przywracania
  ZNACZNIK,     // znacznik `fs_init` tej partycji w NVS: T-WYM-34 (także przy partycji w całości 0xFF)
  NIECZYTELNA,  // bloków partycji nie da się odczytać: T-WYM-34
  SUPERBLOK,    // w bloku 0 albo 1 napis `littlefs` (uszkodzony system plików z danymi): T-WYM-34
  NIE_NOWA,     // układ znany z NVS i bloki nie są w całości 0xFF: T-WYM-34
};
// Decyzja przy starcie według T-WYM-84; kolejność sprawdzeń jak w enum wyżej.
// Formatowanie tylko przy jednoczesnym spełnieniu wszystkich warunków: partycja się nie montuje, tablica panelu jest
// wkompilowana, w NVS nie ma znacznika tej partycji, bloki są czytelne bez napisu `littlefs`, a układ w NVS jest inny
// niż wkompilowany (albo go brak) albo oba bloki są w całości 0xFF.
DecyzjaPartycji decyzjaPartycji(const StanPartycji& s);
// Krótki opis decyzji do logu („partycja nowa”, „znacznik fs_init w NVS” …).
const char* opisDecyzjiPartycji(DecyzjaPartycji d);

// ---------- okno przywracania i komenda `W` (T-WYM-87, R8 karty) ----------
const size_t MAKS_PLIKU_W = (size_t)4 * 1024 * 1024;  // rozmiar treści komendy `W` [PRÓG] (R8)
// Krok przydziału bufora treści komendy `W`: bufor rośnie o tyle w miarę przychodzenia bajtów.
const size_t KAWALEK_TRESCI_W = (size_t)64 * 1024;
const char* const KATALOG_PRZYWRACANIA = "/przywracanie";
// Nazwy plików zrzutu prototypu (8.9 krok 1), jedyne przyjmowane przez komendę `W`; ta sama lista co PLIKI_ZRZUTU
// w tools/prototyp/wgraj_pliki.py.
const std::vector<std::string>& plikiZrzutu();
struct KomendaW {
  std::string nazwa;   // drugie słowo wiersza ("?" gdy brak)
  bool rozmiarOk = false;
  size_t rozmiar = 0;  // liczba dziesiętna 0…MAKS_PLIKU_W
  std::string skrot;   // trzecie pole: 64 znaki szesnastkowe małymi literami albo "" (skrót nieczytelny)
};
// Wiersz nagłówka komendy `W {nazwa} {rozmiar} {sha256}` (bez końca linii). Zwraca false, gdy wiersz nie zaczyna się
// od "W "; inaczej true i pola KomendaW (rozmiarOk = false przy rozmiarze nieczytelnym albo ponad MAKS_PLIKU_W).
bool parsujKomendeW(const std::string& wiersz, KomendaW& k);
// Powód odrzucenia pliku przyjętego komendą `W` o treści `tresc` (już przeczytanej, `tresc.size()` == k.rozmiar):
// "nazwa" (spoza plikiZrzutu(), z `..` albo z ukośnikiem), "skrot" (inny albo nieczytelny); "" = plik do zapisu.
std::string sprawdzPlikW(const KomendaW& k, const std::string& tresc);

// obrazki dnia (src/obrazki.h, spec techniczna 8.6) komendą `W` bez `uploadfs`, który kasuje całą
// pamięć plików. Nazwa w komendzie: `obrazki/{nazwa}.pbm` ({nazwa} z [a-z0-9_-], 1…MAKS_NAZWY_OBRAZKA_W znaków) albo
// `obrazki/obrazki.json` (manifest); plik trafia wprost do `/obrazki/` zapisem atomowym (Magazyn::zapiszSurowy()),
// w oknie przywracania i poza nim (czuwanie), bez listy odrzuconych, znaczników przywracania i plików nocy.
const char* const PREFIKS_OBRAZKOW_W = "obrazki/";
const size_t MAKS_NAZWY_OBRAZKA_W = 40;
// Treść jednego pliku obrazków: PBM 800×434 to ok. 43 KB, manifest kilkuset pozycji kilkadziesiąt KB [PRÓG]; mniej niż
// MAKS_PLIKU_W, bo bufor treści leży na stercie. Rozmiar ponad limit: `BLAD … rozmiar` bez czytania treści.
const size_t MAKS_OBRAZKA_W = (size_t)64 * 1024;
// Czy `nazwa` z komendy `W` to plik obrazków dnia (wzór wyżej; `..`, ukośnik w nazwie, wielkie litery: nie).
bool nazwaObrazkaW(const std::string& nazwa);
// Powód odrzucenia pliku obrazków (`nazwaObrazkaW(k.nazwa)`): "rozmiar" (ponad MAKS_OBRAZKA_W), "skrot", "tresc"
// (PBM nieczytelny albo ponad 800×434, parsujPbm(); manifest nieczytelny, parsujManifest()); "" = plik do zapisu.
// Manifest czytelny bez pozycji dla plików z katalogu przechodzi (ślad w logu doboru, wybierzObrazek(), i
// ostrzeżenie w tools/prototyp/wgraj_pliki.py).
std::string sprawdzObrazekW(const KomendaW& k, const std::string& tresc);

// `/konfiguracja.txt` (ustawienia egzemplarza testowego: profil, co2, raport, daty wymiany) tą samą drogą co obrazki,
// bez `uploadfs` (niesprawdzony na układzie 32 MB). Nazwa w komendzie dokładnie `konfiguracja.txt` (bez ukośnika i `..`),
// zapis atomowy wprost do `/konfiguracja.txt`, w oknie przywracania i w czuwaniu; dane nocy, lista odrzuconych, NVS
// i `/przywracanie/` bez zmian.
const char* const NAZWA_KONFIGURACJI_W = "konfiguracja.txt";
// Plik to kilkanaście wierszy `klucz=wartość` [PRÓG]; 16 KB z zapasem, bufor treści leży na stercie.
const size_t MAKS_KONFIGURACJI_W = (size_t)16 * 1024;
// Powód odrzucenia: "nazwa", "rozmiar" (ponad MAKS_KONFIGURACJI_W), "skrot", "tresc" (bajt 0: to nie jest tekst),
// "znacznik" (wiersz `PLIK_KONIEC`, `PLIKI_KONIEC`, `PLIKI_START` albo zaczynający się od `PLIK `: znaczniki zrzutu `Z`,
// w którym plik wraca); "" = do zapisu.
std::string sprawdzKonfiguracjeW(const KomendaW& k, const std::string& tresc);

// SHA-256 bajtów `dane` jako 64 małe znaki szesnastkowe (komenda `W` portu serwisowego, T-WYM-87, R8 karty).
std::string sha256Hex(const std::string& dane);
