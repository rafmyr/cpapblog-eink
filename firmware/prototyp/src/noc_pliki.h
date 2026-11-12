// SPDX-License-Identifier: GPL-3.0-or-later
// Noc liczona na panelu z plików sesji aparatu (PLD, EVE, CSL, opcjonalnie SAD i BRP) pobieranych z AirBridge od
// v2.3.1, bez zależności od Arduino, bez wyjątków i RTTI (ADR-0036). Metody liczenia jak `noc_z_katalogu()`
// i `metryki()` w tools/kalibracja/noce.py (czytnik nocy etapu 2 walidacji, porównywany z raportem AirBridge w WY9),
// rekord nocy przez te same reguły kanałów i ustawień co raport JSON (src/airbridge.h kanalyRaportu(),
// ustawieniaRaportu()), ustawienia doby ze STR.edf (src/str.h ustawieniaStr()).
//
// Przetwarzanie strumieniowe po pliku: plik jest czytany z bufora wołającego (na panelu cały plik w PSRAM, jeden
// naraz), a LicznikNocy zostawia sobie tylko liczniki wartości kanałów (wartość → liczba próbek), granice zapisu
// sesji i zdarzenia. Pamięć liczników: kilkaset różnych wartości na kanał, niezależnie od długości nocy.
//
// Protokół (AirBridge v2.3.1, spec Załącznik A pkt 10):
//   GET /api/sessions?since=RRRRMMDD&limit=8&order=asc[&cursor=…]
//       → {"sessions":[{"day":"20261108","start":"…","end":"…"|null,"closed":true,
//                       "files":[{"path":"/DATALOG/20261108/20261109_010635_PLD.edf","size":363140},…]}],
//          "str":{…}|null,"next_cursor":null|"…"}; `day` = doba terapii (od południa), `start`/`end` czas lokalny
//          aparatu; otwarta sesja może mieć ścieżki `.part`; 404 = AirBridge starszy niż 2.3.1; 503 = później
//   GET /api/storage/download?path=…   → cały plik; w trakcie terapii zablokowany; plik może zniknąć po liście
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "airbridge.h"
#include "edf.h"
#include "noc.h"
#include "portret.h"
#include "str.h"

namespace noc_pliki {

// ---------- pliki sesji ----------
enum class Rodzaj : uint8_t { BRP, PLD, SAD, EVE, CSL, INNY };
// Nazwa pliku (bez katalogu) `RRRRMMDD_HHMMSS_XXX.edf`, wielkość liter dowolna (PLIK_RE w noce.py) → rodzaj; INNY,
// gdy nazwa nie pasuje (np. `.part`). `prefiks`: część `RRRRMMDD_HHMMSS`; `czas`: sekundy cywilne z prefiksu albo
// -1, gdy prefiks nie jest prawdziwą datą i godziną (wtedy BRP, PLD i SAD nie wchodzą do sesji, uwaga jak w noce.py).
Rodzaj rodzajPliku(const std::string& nazwa, std::string* prefiks = nullptr, long long* czas = nullptr);
const char* nazwaRodzaju(Rodzaj r);
std::string nazwaZeSciezki(const std::string& sciezka);  // część po ostatnim '/'

// Największa różnica prefiksów czasu plików BRP, PLD i SAD tej samej sesji (TOLERANCJA_SESJI_S w noce.py, domysł).
const int TOLERANCJA_SESJI_S = 10;
// Próg AirBridge dla `leak_high_seconds` (LEAK_THRESHOLD w edf_report.cpp): definicja pola M-08, nie próg reguły.
const double PRZECIEK_AIRBRIDGE = 24.0;

// Progi definicji nocy (docs/spec/progi.csv; te same wartości czyta noce.py `Parametry`; zgodność sprawdza test
// zgodności z noce.py na nocy z drzemką i z wczesnym snem).
struct Parametry {
  double startNocyMin = 6 * 60;     // R-DANE-27.start_nocy 18:00, w minutach od 12:00 dnia `day`
  double wczesnySenS = 4 * 3600.0;  // R-DANE-27.wczesny_sen_min 4 h
  double przeciekBardzoDuzy = 27;   // E-23.przeciek_p95 (tylko `leak_27_seconds` w narzędziu porównania)
};

struct Statystyka {
  bool jest = false;
  double med = NAN, p95 = NAN, min = NAN, max = NAN;
};
struct SesjaNocy {
  std::string prefiks;
  bool maZakres = false;  // start i koniec z plików z sygnałami do wykresu (BRP, PLD, SAD)
  double startS = 0, koniecS = 0;
  bool maZapis = false;   // granice zapisu z PLD (pierwszy i ostatni rekord z poprawną próbką ciśnienia)
  double zapisOd = 0, zapisDo = 0;
};
// Wynik doby: pola raportu nocy AirBridge (Załącznik A) jak `noc_z_katalogu()` w noce.py, bez ustawień (te są ze STR).
struct WynikNocy {
  long dzien = -1;
  bool sesjeObecne = false, czesciowa = false, zdarzeniaZnane = false;
  bool klasyfikacja = false;  // pliki EVE mają „Obstructive Apnea” albo „Central Apnea” (inaczej OA i CA nieznane)
  double sekundy = NAN;       // `used_seconds`; NAN = nieznane
  long oa = -1, ca = -1, ua = -1, h = -1, rera = -1;  // liczby zdarzeń; -1 = nieznane
  double ahi = NAN, rdi = NAN;
  double csrS = NAN;          // `csr_seconds`; NAN = nieznane
  std::string csrZrodlo;      // "CSL", "EVE" albo ""
  double przeciekWysokiS = NAN, przeciek27S = NAN;  // `leak_high_seconds`, `leak_27_seconds`; NAN = brak kanału
  std::vector<SesjaNocy> sesje;
  std::map<std::string, Statystyka> kanaly;  // id kanału (KANALY w noce.py) → statystyka z próbek EDF
  Statystyka ps;                             // wsparcie PS = Press.2s − EprPress.2s próbka po próbce (M-28, M-28p)
  std::map<std::string, int> adnotacjeInne;
  std::vector<std::string> problemy;  // błędy plików: noc częściowa
  std::vector<std::string> uwagi;     // informacje bez wpływu na `partial`
};

// Licznik jednej doby. Kolejność użycia: dodajNazwe() dla wszystkich plików doby (z listy sesji AirBridge albo
// z katalogu), potem dodajPlik() dla plików z kolejnosc() (dowolna kolejność), na końcu wynik().
class LicznikNocy {
 public:
  explicit LicznikNocy(long dzien, const Parametry& p = Parametry());
  void dodajNazwe(const std::string& nazwa);
  // Nazwy do przeczytania: PLD każdej sesji, SAD (gdy zSad), BRP (gdy zBrp), EVE i CSL. BRP i SAD bez treści nie są
  // błędem: BRP liczy się wtedy tylko jako znak sesji (start i koniec z PLD), SAD daje tylko SpO2 i tętno.
  std::vector<std::string> kolejnosc(bool zBrp, bool zSad) const;
  // Treść pliku `nazwa` (cały plik, bufor tylko na czas wywołania). false: plik nieczytelny (problem: noc częściowa)
  // albo nazwa spoza dodajNazwe().
  bool dodajPlik(const std::string& nazwa, const uint8_t* bajty, size_t rozmiar);
  // Plik PLD, EVE albo CSL, którego nie da się przeczytać (np. zniknął, ponad limit): problem, noc częściowa.
  void brakPliku(const std::string& nazwa, const std::string& powod);
  WynikNocy wynik() const;
  size_t bajtyLicznikow() const;  // przybliżona pamięć liczników (diagnostyka budżetu)
  // Portret nocy (T-WYM-98): zbieracz odcinków 5 min dostaje każdy przeczytany plik PLD w tym samym przejściu
  // (nullptr = bez Portretu). Zbieracz musi żyć dłużej niż dodajPlik().
  void dolaczPortret(portret::Zbieracz* z) { portret_ = z; }
  // Zdarzenia z plików EVE bez powtórzeń, rosnąco po chwili: (chwila w s cywilnych, tekst) do Zbieracz::wynik().
  std::vector<std::pair<double, std::string>> zdarzeniaPortretu() const;

 private:
  struct Plik {
    std::string nazwa;
    Rodzaj rodzaj = Rodzaj::INNY;
    long long czas = -1;
    int sesja = -1;          // indeks sesji (BRP, PLD, SAD w sesji); -1: SAD luzem albo EVE, CSL
    bool przeczytany = false, blad = false;
    bool maZakres = false;
    double startS = 0, koniecS = 0;
  };
  struct Grupa {
    long long t0 = 0;
    std::string prefiks;
    int pliki[3] = {-1, -1, -1};  // BRP, PLD, SAD → indeks w pliki_
  };
  struct Zdarzenie {
    double t;
    bool maTrwanie;
    double trwanie;
    std::string tekst;
    bool operator<(const Zdarzenie& o) const;
  };
  long dzien_;
  Parametry par_;
  std::vector<Plik> pliki_;
  std::vector<Grupa> grupy_;       // grupy po prefiksie (sesje i SAD luzem)
  bool zgrupowane_ = false;
  // liczniki kanałów: id → (wartość fizyczna → liczba próbek); SAD luzem osobno, do dołączenia w wynik()
  std::map<std::string, std::map<double, uint64_t>> wartosci_;
  std::map<int, std::map<std::string, std::map<double, uint64_t>>> sadLuzem_;
  std::map<int, double> sadLuzemWysoki_, sadLuzem27_;
  std::map<double, uint64_t> wsparcie_;
  double przeciekWysoki_ = 0, przeciek27_ = 0;
  bool maPrzeciek_ = false;
  std::map<int, std::pair<bool, std::pair<double, double>>> zapisy_;  // indeks PLD → (jest, (od, do))
  std::map<int, bool> uzycieZnane_;                                   // indeks PLD → rekordy == 0 albo zapis
  std::set<Zdarzenie> zdarzenia_;
  bool eveBlad_ = false;
  portret::Zbieracz* portret_ = nullptr;
  std::vector<std::vector<std::pair<double, std::string>>> csl_;  // CSR Start/End z każdego CSL, kolejność z pliku
  bool cslBlad_ = false;
  std::vector<std::string> problemy_, uwagi_;
  void grupuj();
  int indeks(const std::string& nazwa) const;
  void zbierzKanaly(const edf::Plik& p, Rodzaj r, std::map<std::string, std::map<double, uint64_t>>& cel,
                    double& wysoki, double& b27, bool& maPrzeciek);
  void zbierzWsparcie(const edf::Plik& p, const std::string& nazwa);
};

// Percentyl metodą najbliższej rangi z par (wartość, liczba) posortowanych rosnąco (edf.py percentyl_z_licznikow).
double percentylZLicznikow(const std::map<double, uint64_t>& m, int procent);

// M-17 i M-18 według R-DANE-27 (m17_m18() w noce.py): sesje od 18:00 dnia `day`, wyjątek wczesnego snu, M-18 ze
// startu najdłuższej z nich (bez długości: najwcześniejszej). m18Min: minuty od 12:00 dnia `day` (NAN = brak).
void m17m18(const WynikNocy& w, const Parametry& p, int& m17, double& m18Min);

// Doba → rekord nocy (zrodlo 'A', jak nocZRaportu() dla raportu tej doby). `str` = STR.edf tego wybudzenia albo
// nullptr; ustawienia z doby `dzien` w STR (bez doby: `available` = false, tryb z `trybZapasowy` jak przy raporcie
// bez tagu MOP); kanały bez próbek EDF ze statystyk STR (`source` = "str"), dalej reguły kanalyRaportu() (ciśnienie
// wdechowe ze STR pominięte, EPAP ze STR z flagą `epapStr`). M-01 = round(sekundy / 60); AHI i M-03 ze zdarzeń EVE
// w czasie zapisu sesji; M-09 z CSL (zapasowo z EVE); M-08 = sekundy wycieku > 24 L/min / 60; PS (M-28, M-28p) tylko
// przy kanale `Press` z próbek EDF; M-13, M-14, M-15 i M-10 z `RespRate`, `TidVol` (mediana mL), `MinVent` (mediana
// L/min) i `FlowLim`; M-17
// i M-18 według R-DANE-27. Ważna jak noc z AirBridge (obecna, nie częściowa, zdarzenia znane, ≥ 240 min, tryb
// obsługiwany); `pelna` jak przy raporcie.
NocAB nocZWyniku(const WynikNocy& w, const Str* str, const std::string& trybZapasowy, const std::string& pna = "",
                 const std::string& srn = "", const Parametry& p = Parametry());

// ---------- źródło nocy i metryki z raportu JSON (bramka ADR-0036 pkt 5) ----------
// Klucze /konfiguracja.txt: `noc_zrodlo=plik|porownanie|json` (domyślnie plik: noc z plików sesji, raport JSON jako
// zapas przy każdym problemie, R-DANE-44; `porownanie`: noc z raportu JSON, noc z plików tylko do porównania,
// src/porownanie.h; ADR-0036 „Zmiana 2026-11-11”) i `noc_json=` lista ID metryk po przecinku,
// które przy `plik` zostają z raportu JSON (rozjazd w porównaniu poza tolerancją WY9). Dozwolone ID: m01, m02, m03,
// m05, m06, m07, m08, m09, m10, m13, m14, m15, m17, m18, m21 (ustawienia), m28.
bool metrykaJsonOk(const std::string& id);
// "m02,m08" → zbiór; false przy nieznanym ID albo pustym elemencie (wiersz konfiguracji pominięty).
bool parsujMetrykiJson(const std::string& tekst, std::set<std::string>& wynik);
// Noc z plików `zPlikow` z polami metryk `metryki` wziętymi z nocy z raportu JSON `zJson` tej samej doby; ważność
// liczona od nowa (M-01, M-02 mogą przyjść z raportu). Pozostałe pola i `pelna` z plików.
void scalZJson(NocAB& zPlikow, const NocAB& zJson, const std::set<std::string>& metryki);

// ---------- /api/sessions dla doby ----------
struct PlikAB {
  std::string sciezka;
  double rozmiar = -1;
};
struct SesjaAB {
  std::string day;  // RRRRMMDD
  bool closed = false;
  std::vector<PlikAB> pliki;
};
struct ListaSesji {
  std::vector<SesjaAB> sesje;
  std::string kursor;  // `next_cursor`; "" = koniec
};
// false: nie obiekt JSON z tablicą `sessions`, sesja bez tekstu `day` albo plik bez tekstu `path`.
bool parsujListeSesji(const std::string& tresc, ListaSesji& l);
// Najwięcej sesji na stronę i stron na dobę [stałe techniczne]: 8 sesji × 5 plików to ok. 4 KB odpowiedzi (limit trasy
// AB_MAKS_BAJTOW_SESJI 16 KiB); 8 stron = 64 sesje na dobę.
const int SESJI_NA_STRONE = 8;
const int STRON_NA_DOBE = 8;
std::string dzienAB(long dzien);  // numer dnia → "RRRRMMDD"
std::string sciezkaSesjiDoby(long dzien, const std::string& kursor = "");
std::string sciezkaPobraniaPliku(const std::string& sciezka);  // /api/storage/download?path=… (kodowanie %XX)
// Pliki pobierane przez panel: PLD każdej sesji, EVE, CSL. BRP (ok. 3 MB na noc, przepływ 25 Hz) nigdy; SAD
// (ok. 120 KB na noc, SpO2 i tętno) tylko przy POBIERAJ_SAD: rekord nocy panelu nie ma pól saturacji (R-SKI-42),
// więc dziś SAD nie jest pobierany (BRP i SAD wpływają wtedy tylko na granice sesji o sekundy, nie na M-01).
const bool POBIERAJ_SAD = false;

}  // namespace noc_pliki
