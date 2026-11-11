// SPDX-License-Identifier: GPL-3.0-or-later
// Historia nocy i norma osobista, bez zależności od Arduino.
// /noce.csv w pamięci flash panelu (LittleFS), 20 pól (wcześniej 14):
//   data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr
// zrodlo: A = AirBridge, K = import z kopii karty SD aparatu, S = doba z pliku STR.edf aparatu pobranego z AirBridge
// (src/str.h, ADR-0035; od wersji 4 pliku `noce`). Puste pole = brak danych.
// czesciowa: 1 = noc z `partial = true` zapisana jako „częściowa” (R-DANE-15, R-DANE-25): nie jest ważna, kompletny
// raport ją nadpisze (R-AW-03). epap_str: 1 = emed/ep95 z kanału `EprPress` z `source = "str"` (EPAP docelowe
// urządzenia): pokazywane z dopiskiem `(urządzenie)`, poza normą i regułami (R-NOR-08, R-KOK-15).
// oa, ca, h: zdarzenia na godzinę (M-03: `counts.oa/ca/h` × 3600 / `used_seconds`); csr: oddychanie okresowe w minutach
// (M-09: `csr_seconds` / 60). Noce z karty nie mają tych pól (R-DANE-31, 4.11): puste.
// Panel obsługuje tylko tryb ASV (ADR-0016): cmed/cp95 to ciśnienie wdechowe (IPAP, M-05/M-05p; P95 tylko
// wewnętrznie), emed/ep95 to EPAP (M-06, M-06p), psmed/psp95 to wsparcie PS (M-28, M-28p), wszystko w cmH2O.
// ustawienia: tekst z historia.py, np. `ASVAuto/6/10/3/15` (MOP/EAI/EAX/ANS/AXS); puste = nieznane.
// Parser przyjmuje też inne układy (brakujące pola = NAN, ustawienia puste):
//   8 pól  import.csv w pierwszym formacie: data;wazna;m01;ahi;cisnienie_med;cisnienie_p95;wyciek_med;wyciek_p95
//   9 pól  /noce.csv w pierwszym formacie: jak 8 + zrodlo
//   12 pól import.csv (historia.py): jak 8 + epap_med;epap_p95;ps_med;ps_p95
//   13 pól import.csv (historia.py): jak 12 + ustawienia
//   14 pól /noce.csv: jak 13 + zrodlo
//   20 pól /noce.csv: jak 14 + czesciowa;epap_str;oa;ca;h;csr
// Od pliki roczne `/dane/noce/RRRR.csv` z magazynu (src/magazyn.h): wersja 1 = 20 pól i `import`; od wersji beta
// wersja 2 = wersja 1 i pola (21 pól, lista przy wierszNocyV2; parsujNocV2); wersja 3 = 42 pola (`m18`), wersja 4
// bez nowych pól (źródło `S`), wersja 5 = 44 pola (`m14;m15`, wierszNocyV5; parsujNocV5).
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

struct Noc {
  std::string data;  // RRRR-MM-DD, data poranka (4.1)
  bool wazna = false;
  int m01 = -1;      // minuty z maską, -1 = brak
  float ahi = NAN, cmed = NAN, cp95 = NAN, wmed = NAN, wp95 = NAN;
  float emed = NAN, ep95 = NAN, psmed = NAN, psp95 = NAN;  // EPAP (M-06), wsparcie PS (M-28), cmH2O
  std::string ustawienia;  // "" = nieznane
  char zrodlo = 'A';
  bool czesciowa = false;  // `partial = true` (R-DANE-15): nieważna, do nadpisania kompletnym raportem
  bool epapStr = false;    // emed/ep95 ze STR (`source = "str"`): bez normy i reguł (R-NOR-08)
  float oa = NAN, ca = NAN, h = NAN;  // M-03: na godzinę
  float csr = NAN;                    // M-09: minuty
  // (R1): chwila importu, czyli czas unix UTC pierwszego zapisu kompletnego rekordu tej nocy
  // z AirBridge; 0 = nieznana (noce z karty i starsze). Tylko w pliku wersji 1 (`/dane/noce/RRRR.csv`, pole `import`).
  uint32_t import = 0;
  // pola rekordu nocy z AirBridge, tylko w pliku `noce` wersji 2
  // (`/dane/noce/RRRR.csv`, pola `m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb`
// po `import`). Noce z karty (import) i mają je puste.
  float m13 = NAN;  // M-13 (4.3): oddechy na minutę, kanał `RespRate` raportu: `median` (Załącznik A); NAN = brak
  int m17 = -1;     // M-17 (4.3): zdjęcia maski = `session_count` − 1, najmniej 0 (fail-path R-DANE-27); -1 = brak
  std::string pna, srn;  // model i numer seryjny aparatu z `/api/status` w chwili pobrania nocy (R-NOR-25)
  float m08 = NAN;  // M-08 (4.3): minuty dużego wycieku, `leak_high_seconds` / 60 (próg AirBridge 24 L/min, R-DANE-23)
  // Reguły L i H: ustawienia aparatu z `settings.fields[]` raportu dnia
  // (tagi `MPA`, `MPI`, `IPC`, `EEP`, `MNS`, `MXS`, cmH2O, 4.11 i M-21) tylko do odczytu w regułach, nigdy do pokazania
  // jako wartość docelowa (ADR-0002); UA/h (`counts.ua` × 3600 / `used_seconds`); M-10 (`FlowLim`: `median`, `p95`).
  float mpa = NAN, mpi = NAN, ipc = NAN, eep = NAN, mns = NAN, mxs = NAN;
  float ua = NAN;
  float m10 = NAN, m10p = NAN;
  // Ustawienia trybu ASVAuto (`EAI`, `EAX` min. i maks. EPAP, `ANS`, `AXS` min. i maks. PS,
  // 4.11) i `EPR` (CPAP, AutoSet), tylko do odczytu jak wyżej; tryb nocy (`MOP` z raportu, bez tagu `mode` z
  // `/api/status`, R-DANE-29; "" = nieznany)
  float eai = NAN, eax = NAN, ans = NAN, axs = NAN, epr = NAN;
  std::string tryb;
  // (R-KON-09): M-18 (4.3), start pierwszej sesji nocy z `sessions[].start_ms` raportu
  // AirBridge w czasie lokalnym panelu, w minutach od 12:00 dnia wieczoru (0…1439; 23:10 = 670, 00:30 = 750);
  // -1 = brak (noce z karty, `start_ms` = null). Tylko w pliku `noce` wersji 3 (pole `m18` na końcu).
  int m18 = -1;
  // M-14 (4.3): objętość oddechowa w mL, kanał `TidVol`: `median`; M-15 (4.3): wentylacja minutowa w L/min, kanał
  // `MinVent`: `median` (raport JSON i noc z plików sesji, PLD `TidVol.2s` i `MinVent.2s`); NAN = brak (noce z karty,
  // starsze rekordy). Tylko w pliku `noce` wersji 5 (pola `m14;m15` na końcu).
  float m14 = NAN;
  float m15 = NAN;
};
// EPAP do normy, reguł i porównań (tylko z EDF, R-NOR-08); NAN dla EPAP docelowego urządzenia
float epapMed(const Noc& n);
float epapP95(const Noc& n);
// Uzupełnia puste pola rekordu `cel` wartościami z `z` tej samej nocy (pola kanałów, PS, ustawienia, M-03, M-09,
// także M-13, M-14, M-15, M-17, M-18, `pna`, `srn`, M-08, ustawienia `mpa`…`mxs`, UA/h i M-10);
// m01, AHI, ważność i źródło zostają z `cel`. TYLKO_SPOZA_AIRBRIDGE: tylko wsparcie PS (raport AirBridge go nie ma),
// np. z rekordu A, który dostał PS z karty. Zwraca true, gdy coś dopisano.
enum class PolaUzupelnienia : unsigned char { WSZYSTKIE, TYLKO_SPOZA_AIRBRIDGE };
bool uzupelnijNoc(Noc& cel, const Noc& z, PolaUzupelnienia zakres = PolaUzupelnienia::WSZYSTKIE);
// Tekst pola `/noce.csv` bez separatora `;` i znaków sterujących (CR, LF, tab i inne < 0x20, 0x7F): usunięte.
std::string czystePole(const std::string& t);

// Wiersz pliku → rekord. Puste pole = brak danych (NAN); śmieci w polu, zła data, źródło inne niż A/K/S albo liczba
// pól inna niż 8, 9, 12, 13, 14 lub 20 = błąd wiersza. zapiszNoc pisze zawsze 20 pól.
bool parsujNoc(const std::string& linia, char domyslneZrodlo, Noc& r);
std::string zapiszNoc(const Noc& r);  // bez '\n'
// Rekord nocy w formacie `noce` wersji 1 (src/magazyn.h): 20 pól zapiszNoc() i pole `import` (czas unix albo puste).
std::string wierszNocyV1(const Noc& r);
// Wiersz wersji 1 (co najmniej 21 pól; pola dalsze, np. z nowszej wersji, pominięte) → rekord: pierwsze 20 pól
// parserem parsujNoc() (źródło domyślne 'A'), pole 21 → `import` (same cyfry do 4294967295; puste albo nieczytelne = 0,
// nieznane: zły znacznik chwili nie usuwa rekordu nocy). false: mniej niż 21 pól albo błąd parsujNoc().
bool parsujNocV1(const std::string& wiersz, Noc& r);
// rekord nocy w formacie `noce` wersji 2: pola wersji 1 (wierszNocyV1), potem
// `m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb` (liczby z dwoma miejscami po
// przecinku, m17 liczbą całkowitą ≥ 0, puste = brak; `pna`, `srn`, `tryb` przez czystePole()).
const size_t POLA_NOCY_V2 = 41;
// Tryby obsługiwane (4.11, ADR-0030): CPAP, AutoSet, ASV, ASVAuto. Inny albo "" = noc w trybie spoza wydania (R-DANE-29).
bool trybObslugiwany(const std::string& tryb);
// Tryb nocy: pole `tryb`, a dla nocy bez niego (z karty, sprzed) pierwszy człon pola `ustawienia` (np. `ASVAuto`).
std::string trybNocy(const Noc& n);
std::string wierszNocyV2(const Noc& r);
// wersja 3 = pola wersji 2 i `m18` (POLA_NOCY_V3 = 42). Wiersz wersji 1 albo 2 czyta się z m18 = -1.
const size_t POLA_NOCY_V3 = 42;
// wersja 4 = te same pola co wersja 3; nowa jest tylko dozwolona wartość `S` w polu `zrodlo` (ADR-0035 pkt 6).
// Firmware znający wersję 3 odrzuca wiersz z `S`, więc plik wersji 4 czyta tylko do odczytu (ADR-0022 pkt 4,
// T-WYM-54) i nie przepisuje go z nocami ze STR zgubionymi albo podwojonymi.
// wersja 5 (11.11.2026) = pola wersji 4 i `m14;m15` (M-14, M-15, POLA_NOCY_V5 = 44); migracja 4 → 5 krokiem
// ogólnym (puste pola na końcu, kopia `.v4`). Wiersze piszą i czytają wierszNocyV5() i parsujNocV5().
const int WERSJA_NOCY = 5;
std::string wierszNocyV3(const Noc& r);
bool parsujNocV3(const std::string& wiersz, Noc& r);
const size_t POLA_NOCY_V5 = 44;
std::string wierszNocyV5(const Noc& r);
// Wiersz wersji 1…5 (pola dalsze, np. z nowszej wersji, pominięte) → rekord: pierwsze 42 pola parserem parsujNocV3(),
// pola 43 i 44 → `m14`, `m15` (liczba skończona ≥ 0; brak, puste albo nieczytelne = brak, NAN: zły znacznik nie usuwa
// rekordu nocy). false jak parsujNocV3().
bool parsujNocV5(const std::string& wiersz, Noc& r);
// M-18 z chwili startu pierwszej sesji (czas unix UTC) dla nocy z datą poranka `dataPoranka`: minuty od 12:00 dnia
// wieczoru w czasie lokalnym (strefa z TZ); -1, gdy start nie wypada między 12:00 wieczoru a 12:00 poranka.
int m18ZChwili(long long epoch, const std::string& dataPoranka);
// Wiersz wersji 1 albo 2 (co najmniej 21 pól; pola dalsze, np. z nowszej wersji, pominięte) → rekord: pierwsze 21 pól
// parserem parsujNocV1(), pola 22–41 → pola wersji 2 w kolejności wyżej. Brakujące pola (wiersz wersji 1) = brak danych;
// nieczytelne pole liczbowe = brak danych (jak `import`: zły znacznik nie usuwa rekordu nocy). false jak parsujNocV1().
bool parsujNocV2(const std::string& wiersz, Noc& r);
// Treść rekordu do porównań (czy noc się zmieniła): pola zapiszNoc i pola (bez `import`), z M-18, M-14 i M-15.
std::string trescNocy(const Noc& r);
const Noc* nocZDnia(const std::vector<Noc>& hist, const std::string& data);
const Noc* ostatniaNoc(const std::vector<Noc>& hist, const std::string& najpozniej);  // ostatni rekord ≤ data

// Noce bez danych z rzędu do dziś włącznie (licznik E-01): jedno liczenie dla ekranu S5 (src/dzien.h nocyBezDanych(),
// R-AW-24, R-AW-32) i planu wybudzeń (src/plan.h nocyBezDanych(), R-DANE-18), żeby tekst `Sprawdzam co 3 h.` zawsze
// zgadzał się z tym, jak często panel pyta. Przyjmuje datę dziś, datę i minutę doby
// ostatniego udanego odczytu AirBridge ("" = nie było; minuta -1 = nieznana, liczona jak po 05:00), datę pierwszej
// nieudanej próby ("" = nieznana) i historię nocy (nullptr = bez historii). Liczy:
//  - z odczytem: noce po dniu odczytu do dziś; odczyt przed 05:00 [PRÓG] nie zna jeszcze nocy z dnia odczytu, więc
//    ona też się liczy (nocyOdOdczytu(), jak R-EKR-57); odczyt późniejszy niż dziś → 0;
//  - bez odczytu: od dnia pierwszej nieudanej próby włącznie;
//  - z historią, w której jest noc sprzed dziś: najwyżej luka w historii (noce od ostatniej nocy w historii do dziś;
//    noc bez terapii przy działającym AirBridge nie jest nocą bez danych, R-AW-32), a bez odczytu i bez daty próby
//    sama luka.
// Zwraca 0…1000; zła data dziś, odczytu albo próby → 1; nic znanego (nowy panel bez historii) → 1.
int nocyBezDanychZRzedu(const std::string& dzis, const std::string& dataOdczytu, int minutaOdczytu,
                        const std::string& dataBledu, const std::vector<Noc>* hist);

// ---------- norma (rozdz. 4.4, R-NOR-01, R-NOR-10) ----------
// Dla każdej metryki osobno: ostatnie 30 nocy ważnych z wartością tej metryki, w 90 dniach kalendarzowych
// przed dzisiejszą nocą (noc oceniana nie wchodzi do własnej normy, A-10), bez nocy z czynnikiem
// „katar/infekcja” albo „spanie poza domem” (R-CZY-05). Mniej niż 14 wartości = brak normy (NAN).
// Mediana: średnia dwóch środkowych przy parzystej liczbie. P10 i P90: metoda najbliższej rangi,
// jak tools/kalibracja/edf.py (ranga = ceil(n × p / 100)).
// R-NOR-07 w uproszczeniu: normy metryk ciśnienia i wycieku (M-05, M-05p, M-06, M-06p, M-07 P95, M-28, M-28p)
// tylko z nocy z tymi samymi ustawieniami co najnowsza noc w historii (≤ dziś). Puste ustawienia nocy = takie same
// jak ostatnie znane wcześniej; noc przed pierwszymi znanymi ustawieniami nie pasuje do znanych. Bez żadnych znanych
// ustawień w historii: bez filtra. Mniej niż 14 takich nocy = brak normy tej metryki (rozruch metryki, R-NOR-11).
// Czas z maską i AHI bez tego filtra. M-10 i M-11 nie ma w danych prototypu.
// Wyłączenia per sygnał (4.4): noce w stanie odchylenia danej metryki (R-NOR-09) i noce niepewne z normy AHI
// (R-NOR-19) podaje WykluczeniaNormy; liczy je src/stany.cpp (liczStany), które woła liczNorme noc po nocy.
struct WykluczeniaNormy {
  std::set<std::string> m01, ahi, wp95, ep95;
  std::set<std::string> m03;  // noce niepewne poza normą M-03 (R-NOR-19)
  // noce w stanie odchylenia CA/h (M-03ca, poza normą `ca` razem z `m03`) i M-17 (R-NOR-09)
  std::set<std::string> ca, m17;
  std::set<std::string> cp95;  // noce w stanie odchylenia M-05p (tryby CPAP i AutoSet)
  // ADR-0039: noce w stanie odchylenia M-28 (PS), M-13, CSR% (M-09), M-10 P95, M-14 i M-15 (R-NOR-09)
  std::set<std::string> ps, m13, csr, m10;
  std::set<std::string> m14, m15;  // M-14 w górę i w dół, M-15 w górę i w dół (S-K-41…S-K-44)
};

struct NormaMetryki {
  int n = 0;
  double med = NAN, p10 = NAN, p90 = NAN;
  bool jest() const { return n >= 14; }
};

struct Norma {
  bool zegar = false;
  int n = 0, zKarty = 0;  // liczba nocy w normie czasu z maską i ile z nich z karty
  NormaMetryki m01, ahi, cmed, cp95, wp95;
  NormaMetryki emed, ep95, psmed, psp95;  // EPAP i wsparcie PS: tylko do pokazania normy, bez reguł (ADR-0002)
  NormaMetryki oa, ca, h;                 // M-03 na godzinę (K2); CA/h ze stanem odchylenia od wersji beta
  NormaMetryki m17;                       // M-17 zdjęcia maski: stan odchylenia, E-14
  NormaMetryki m13;                       // M-13 oddechy na minutę: K2, L-05, S-K-17, S-K-30
  NormaMetryki m14;                       // M-14 objętość oddechowa (4.4): S-K-43, S-K-44 (ADR-0039)
  NormaMetryki m15;                       // M-15 wentylacja minutowa: sygnał towarzyszący S-K-30 (5.10.2), S-K-41, S-K-42
  NormaMetryki csrp;                      // M-09 / M-01 w % (CSR%): L-02(b), S-K-46, raport
  // M-10 P95 (`FlowLim`): S-K-47 i linia „dobra zmiana” (ADR-0039); jak normy ciśnienia tylko z nocy z ustawieniami
  // najnowszej nocy (R-NOR-07; 4.4: 14 nocy bez oceny po zmianie, R-KON-30)
  NormaMetryki m10p;
  std::string ustawienia;                 // ustawienia, z których liczone są normy ciśnienia i wycieku ("" = bez filtra)
};

Norma liczNorme(const std::vector<Noc>& hist, const std::string& dzis, const std::set<std::string>& wykluczone,
                 const WykluczeniaNormy* wm = nullptr);
// To samo na historii posortowanej raz (stany.cpp liczy normę noc po nocy): koszt jednej normy to okno 90 dni.
struct HistoriaPos {
  std::vector<const Noc*> noce;         // po dacie rosnąco
  std::vector<const std::string*> ust;  // ustawienia każdej nocy z przeniesieniem ostatnich znanych (R-NOR-07)
};
HistoriaPos posortujHistorie(const std::vector<Noc>& hist);
Norma liczNorme(const HistoriaPos& h, const std::string& dzis, const std::set<std::string>& wykluczone,
                 const WykluczeniaNormy* wm = nullptr);
double mediana(std::vector<double> v);              // NAN dla pustego
double percentyl(std::vector<double> v, int proc);  // najbliższa ranga; NAN dla pustego

// ---------- odchylenie w jednej nocy (4.5) ----------
// Wartość poza P90 (P10 dla czasu z maską) i różnica od mediany normy co najmniej Δ [PRÓG]:
// M-01 −90 min, M-02 +3 / h, M-07 P95 +10 L/min. Tylko noc ważna; metryka bez normy nie ma odchylenia.
// M-13 (oddechy na minutę) jest w rekordzie od (Noc::m13), ale bez normy i odchylenia (reguły: silnik).
struct Odchylenie {
  bool m01 = false, ahi = false, wp95 = false;
  bool jakiekolwiek() const { return m01 || ahi || wp95; }
};
Odchylenie odchylenieWNocy(const Noc& n, const Norma& nm);

// ---------- regularność ----------
int liczbaNocyWaznych(const std::vector<Noc>& hist, const std::string& najpozniej);
// M-20: kolejne noce z czasem z maską ≥ 4 h, kończąc na nocy `dzis` (0, gdy jej brak albo < 4 h).
int seria(const std::vector<Noc>& hist, const std::string& dzis);
// S-K-28: liczba nocy z czasem z maską ≥ 4 h od pierwszej nocy w rekordach do `dzis` włącznie.
int nocyPonad4h(const std::vector<Noc>& hist, const std::string& dzis);
