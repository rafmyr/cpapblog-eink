// SPDX-License-Identifier: GPL-3.0-or-later
// Slot dzienny (spec 5.8.12, ADR-0027, R-KON-41…R-KON-58): prognoza godzinowa z serwisu pogody, reguła wietrzenia
// S-K-33, „Jutro rano” S-K-34, wymiana sprzętu S-K-35, pyłek S-K-36, „Dzień i noc” S-K-37 (wschód i zachód słońca
// liczone na panelu), wygaszacz z obrazkiem dnia S-K-38 (src/obrazki.h, R-KON-73…R-KON-77), drzemka S-K-39, stopka dzienna i wybór ekranu
// według części „Dzień” tabeli 7.2. Bez zależności od Arduino; rysowanie w src/ekrany_kontekst.cpp.
//
// Prognoza (ADR-0027 „Granice bez zmian”, ADR-0001, ADR-0019 pkt 4): panel pyta serwis pogody Open-Meteo
// bezpośrednio, bez serwera domowego i bez danych terapii w zapytaniu (tylko współrzędne miasta). Kontrakt wersji 1 lokalnego
// źródła otoczenia nie ma pól godzinowych, a wariant „prognoza ze źródła na serwerze domowym (kontrakt wersji 2)” odrzucono
// w ADR-0027, więc lokalnego źródła nie rozszerzamy. Odpowiedzi serwisu: `/v1/forecast` (temperatura, opad,
// zachmurzenie, porywy) i `/v1/air-quality` (PM2.5, pyłki), oba z `timezone=Europe/Warsaw`, więc godziny są lokalne.
// Sprawdzone z sieci domowej (część „dostęp”): oba adresy odpowiadają po `http://` (bez TLS, którego
// adapter HTTP panelu nie ma), 200, ok. 2 KB na dwie doby; pyłki poza sezonem mają wartość 0.
//
// Godziny horyzontu: indeks w Prognoza::h = godzina od 00:00 dnia Prognoza::data (0…47: dziś i jutro).
#pragma once
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "dobranoc.h"  // hhmm(): 1421 → "23:41"
#include "dobry_start.h"
#include "noc.h"
#include "uklad_ekranow.h"

// ---------- prognoza godzinowa (M-26, M-27) ----------
enum Pylek : std::uint8_t { PYL_BRZOZA = 0, PYL_TRAWY = 1, PYL_OLCHA = 2, PYL_BYLICA = 3, PYL_N = 4 };
extern const char* const PYLEK_DOPELNIACZ[PYL_N];  // `brzozy`, `traw`, `olchy`, `bylicy` (tytuły S-K-06, S-K-36)

struct GodzinaPrognozy {
  float temp = NAN;    // °C (temperature_2m)
  float opad = NAN;    // prawdopodobieństwo opadu, % (precipitation_probability)
  float chmury = NAN;  // zachmurzenie, % (cloud_cover)
  float porywy = NAN;  // porywy wiatru, km/h (wind_gusts_10m)
  float pm25 = NAN;    // µg/m³ (pm2_5)
  float pylek[PYL_N] = {NAN, NAN, NAN, NAN};  // ziarna/m³
};

const int PROG_GODZIN = 48;  // dziś i jutro (R-KON-52: do 09:00 następnego dnia)
struct Prognoza {
  std::string data;          // dzień godziny 0 (RRRR-MM-DD); "" = brak prognozy
  uint32_t pobrano = 0;      // czas unix pobrania (nagłówek ekranu `prognoza {hh:mm}`, ważność R-KON-52, R-KON-53)
  std::string godzinaPobrania;  // `07:55` (czas lokalny pobrania, do nagłówka)
  std::vector<GodzinaPrognozy> h = std::vector<GodzinaPrognozy>(PROG_GODZIN);
  bool jest() const { return !data.empty(); }
  // Godzina `g` dnia `dzien` (0…23) albo nullptr, gdy poza prognozą.
  const GodzinaPrognozy* godzina(const std::string& dzien, int g) const;
};

// Adresy zapytań (bez danych terapii; R-DANE-16, ADR-0001). Współrzędne z 4 miejscami po przecinku (ok. 11 m).
std::string urlPrognozy(double lat, double lon);
std::string urlPowietrza(double lat, double lon);
std::string urlGeokodowania(const std::string& miasto);  // współrzędne miasta raz po zmianie miasta
// Parsery odpowiedzi 200 (JSON Open-Meteo, blok `hourly` z tablicą `time` `RRRR-MM-DDTHH:MM`). Zwracają false przy
// nieczytelnym JSON, braku bloku `hourly` albo tablicy `time`. Wartość `null` albo spoza zakresu wiarygodności = brak
// (nigdy 0). Godziny spoza `p.data` i dnia następnego pomijane; pierwsza odpowiedź (prognoza) ustala `p.data` na datę
// pierwszej godziny, druga (powietrze) dokłada PM2.5 i pyłki do tych samych godzin.
bool parsujPrognoze(const std::string& json, Prognoza& p);
bool parsujPowietrze(const std::string& json, Prognoza& p);
bool parsujGeokodowanie(const std::string& json, double& lat, double& lon);
// Zapis w magazynie (format `prognoza`, /dane/prognoza.csv, src/magazyn.cpp): 48 wierszy
// `data;godz;temp;opad;chmury;porywy;pm25;brzoza;trawy;olcha;bylica;pobrano` (puste = brak). godzinaPobrania ustawia
// czytający (czas lokalny z `pobrano`). prognozaZWierszy() przy złych wierszach zwraca false i pustą prognozę.
std::vector<std::string> wierszePrognozy(const Prognoza& p);
bool prognozaZWierszy(const std::vector<std::string>& w, Prognoza& p);

const uint32_t PROGNOZA_WAZNA_S = 12 * 3600;  // R-KON-53: prognoza starsza niż 12 h [PRÓG] = brak prognozy
bool prognozaWazna(const Prognoza& p, uint32_t teraz);

// ---------- reguła wietrzenia S-K-33 (5.8.12, R-KON-43, R-KON-44, R-KON-45) ----------
const float WIETRZ_PM25_MAX = 35;      // czysta godzina: PM2.5 ≤ 35 µg/m³ [PRÓG] (GIOŚ „dobry”, 1 h)
const float WIETRZ_ZIMA_ROZNICA = 8;   // tryb zimowy: przy łóżku − maksimum za oknem ≥ 8 °C [PRÓG]
const float WIETRZ_ZIMA_PASMO = 2;     // okno zimowe: najwyżej 2 °C chłodniej niż najcieplejsza czysta godzina [PRÓG]
const float WIETRZ_LATO_OD = 24;       // tryb letni i upał: przy łóżku ≥ 24 °C [PRÓG]
const float WIETRZ_LATO_ROZNICA = 2;   // tryb letni: za oknem chłodniej o ≥ 2 °C [PRÓG]
const int WIETRZ_HORYZONT_DO = 23;     // horyzont: od bieżącej pełnej godziny do 23:00 [PRÓG] (godziny 0…22)
const int CO2_PROG_TERAZ = 1000;       // wariant CO2: próg S-K-01 [PRÓG]

enum class Wietrzenie : uint8_t { BRAK, CO2, TRWA, POZNIEJ, UPAL_DO, UPAL_CALY_DZIEN, SMOG };
enum class TrybWietrzenia : uint8_t { BRAK, ZIMA, LATO };
// Dlaczego okno się kończy (TRWA) albo dlaczego czekać (POZNIEJ); wybiera tekst powodu 5.8.4.
enum class PowodOkna : uint8_t { BRAK, SMOG, ZIMNIEJ, CIEPLEJ, CZYSTO_DO_WIECZORA, TERAZ_SMOG, NAJCIEPLEJ, CHLODNIEJ };

// Progi pyłku „górne 10% wartości sezonu” (S-K-06) dla zaznaczonych rodzajów; NAN = rodzaj niezaznaczony albo próg
// nieznany (wtedy rodzaj nie wpływa na regułę). Ustawienie `Pyłki w dzień` (5.6.4) jest domyślnie puste.
struct PylkiDnia {
  bool zaznaczony[PYL_N] = {false, false, false, false};
  float prog[PYL_N] = {NAN, NAN, NAN, NAN};
  bool jakikolwiek() const;
};

struct WejscieWietrzenia {
  const Prognoza* prognoza = nullptr;  // ważna prognoza (R-KON-53) albo nullptr
  std::string dzis;
  int godzina = -1;      // bieżąca pełna godzina (0…23)
  float przyLozku = NAN; // M-22, czujnik panelu z tego wybudzenia (R-AW-13: NAN = brak, S-K-33 pominięte)
  int co2 = -1;          // CO2 sypialni z odczytu nie starszego niż 30 min (M-23); -1 = brak
  PylkiDnia pylki;
};

struct WynikWietrzenia {
  Wietrzenie wariant = Wietrzenie::BRAK;
  TrybWietrzenia tryb = TrybWietrzenia::BRAK;
  PowodOkna powod = PowodOkna::BRAK;
  int od = -1, do_ = -1;  // okno: pierwsza godzina okna i godzina końca (pierwsza po oknie, ≤ 23); -1 = brak
  float pm25Teraz = NAN;
  // horyzont do wizualizacji: godziny od `godzina` do 22, w kolejności
  std::vector<float> pm25, temp;
  std::vector<bool> czyste;
};
bool godzinaCzysta(const GodzinaPrognozy& g, const PylkiDnia& pylki);
WynikWietrzenia regulaWietrzenia(const WejscieWietrzenia& w);

// ---------- S-K-34 „Jutro rano” (R-KON-46, R-KON-47) ----------
const int JUTRO_OD = 6, JUTRO_DO = 9;  // prognoza jutro 06:00–09:00 [PRÓG] (godziny 6, 7, 8)
const float JUTRO_SLISKO_OPAD = 50, JUTRO_DESZCZ_OPAD = 60, JUTRO_WIATR = 60;  // %, %, km/h [PRÓG]
const int JUTRO_PRZED_OKNEM_OD = 16;  // R-KON-46: od 16:00 [PRÓG] S-K-34 przed S-K-33
enum class Jutro : uint8_t { BRAK, SLISKO, MROZ, DESZCZ, WIATR };
struct WynikJutra {
  Jutro wariant = Jutro::BRAK;
  float tMin = NAN;     // najniższa temperatura rano
  int godzinaTMin = -1;
  float opadMax = NAN, porywyMax = NAN;
};
WynikJutra jutroRano(const Prognoza& p, const std::string& dzis);

// ---------- S-K-35 „Wymiana sprzętu” (5.9.2, R-KON-48) ----------
// poduszka i maska to jeden element „Maska”; rury i komory nawilżacza panel nie pilnuje.
enum ElementSprzetu : std::uint8_t { EL_MASKA = 0, EL_FILTR, EL_N };
struct ElementWymiany {
  std::string wymieniono;   // data ostatniej wymiany (RRRR-MM-DD); "" = bez zapisanej daty (R-MAS-06)
  int okres = 0;            // okres wymiany: miesiące (maska, 6) albo tygodnie (`tygodnie`; filtr, 4), 5.9.2
  bool tygodnie = false;    // true: `okres` w tygodniach (filtr)
};
const int WYMIANA_PRZED_DNI = 7, WYMIANA_CO_DNI = 2;  // R-KON-48 [PRÓG]
struct WynikWymiany {
  int element = -1;         // -1 = brak S-K-35
  std::string termin;       // RRRR-MM-DD
  bool poTerminie = false;
};
std::string terminWymiany(const ElementWymiany& e);  // data + okres (miesiące albo tygodnie); "" bez daty
// `ostatniePokazanie` = data ostatniego dnia z S-K-35 (dziennik slotu); "" = nigdy.
WynikWymiany wymianaSprzetu(const ElementWymiany (&el)[EL_N], const std::string& dzis, const std::string& ostatniePokazanie);
std::string wiekElementu(const std::string& wymieniono, const std::string& dzis);  // `3 tyg.`, `7 mies.`, `–`
// Wiek elementu w jego jednostce: filtr zawsze w pełnych tygodniach (`5 tyg.`), reszta jak wiekElementu(data, dzis).
std::string wiekElementu(const ElementWymiany& e, const std::string& dzis);
std::string okresElementuTekst(const ElementWymiany& e);  // `6 mies.`, `4 tyg.`
extern const char* const NAZWY_ELEMENTU[EL_N];   // `maska`, `filtr`

// ---------- S-K-36 „Pyłek w dzień” (R-KON-45) ----------
struct WynikPylku {
  int rodzaj = -1;  // -1 = brak S-K-36
  int od = -1, do_ = -1;  // godziny najwyższego pyłku: pierwsza i ostatnia godzina ponad próg + 1
  std::vector<float> wartosci;  // horyzont od bieżącej godziny do 22
  std::vector<bool> wysoko;
};
WynikPylku pylekDnia(const Prognoza& p, const std::string& dzis, int godzina, const PylkiDnia& pylki);

// ---------- S-K-37 „Dzień i noc” (R-KON-49) ----------
// Wschód i zachód słońca (algorytm NOAA, środek tarczy z refrakcją −0,833°) w minutach doby czasu lokalnego Polski
// (CET, od ostatniej niedzieli marca do ostatniej niedzieli października CEST); false przy dniu polarnym i nocy polarnej.
bool slonce(const std::string& data, double lat, double lon, int& wschod, int& zachod);
int przesuniecieStrefy(const std::string& data);  // minuty od UTC w Europe/Warsaw o 12:00 tego dnia (60 albo 120)
struct WynikSlonca {
  bool jest = false;
  int wschod = -1, zachod = -1;
  int dlugosc = 0, zmiana = 0;  // minuty; zmiana wobec dnia sprzed 7 dni
  std::string powod;            // zdanie (5.8.4)
};
WynikSlonca dzienINoc(const std::string& dzis, double lat, double lon);

// ---------- drzemka (R-KON-56…R-KON-58) ----------
// Sesja doby z raportu AirBridge: początek i koniec w czasie unix, P95 wycieku sesji (L/min).
struct SesjaDoby {
  uint32_t od = 0, do_ = 0;
  float wyciekP95 = NAN;
};
const int DRZEMKA_MIN_S = 15 * 60;  // sesje krótsze niż próg terapii S-K-12 (15 min) [PRÓG] pomijane
const float WYCIEK_PROG_SK10 = 24;  // L/min, próg S-K-10 (R-DANE-23)
struct Drzemka {
  int minut = 0;            // suma czasu sesji po końcu ostatniej nocy; 0 = brak drzemki
  float wyciekP95 = NAN;    // największy P95 sesji drzemki (zapis bez przebiegu: górna granica P95 całości)
  uint32_t od = 0, do_ = 0; // pierwsza i ostatnia chwila drzemki (nagłówek S-K-39 `drzemka {hh:mm}–{hh:mm}`)
};
Drzemka liczDrzemke(const std::vector<SesjaDoby>& s, uint32_t koniecNocy);
// Raport doby AirBridge (R-KON-57): sesje `sessions[]` (start_ms, end_ms) i P95 kanału `Leak` całej doby. P95 sesji
// drzemki raport nie podaje; gdy P95 doby z drzemką jest wyższe niż P95 samej nocy (rekord nocy), wyciek drzemki był
// co najmniej taki jak P95 doby, więc to on idzie do S-K-39 (domysł); inaczej NAN (bez S-K-39).
bool sesjeZRaportu(const std::string& json, std::vector<SesjaDoby>& s, float& wyciekP95Doby);
// Sekundy kalendarza cywilnego aparatu (`start_ms` / 1000 raportu AirBridge, czas lokalny zakodowany jak UTC) → chwila
// unix (strefa panelu, z czasem letnim); 0 przy wartości ≤ 0 albo nieczytelnej.
uint32_t chwilaZCywilnego(long long cyw);
float wyciekDrzemkiZDoby(float p95Doby, float p95Nocy);
// Stan drzemki dnia w NVS: `{data};{minut};{p95 × 10 albo -1};{od};{do}`.
std::string zapiszDrzemke(const std::string& dzis, const Drzemka& d);
bool parsujDrzemke(const std::string& t, const std::string& dzis, Drzemka& d);
// `edf_prefix` `RRRRMMDD_HHMMSS` → data i minuta doby; false przy innym układzie.
bool prefiksSesji(const std::string& p, std::string& data, int& minuta);

// ---------- ustawienia slotu dziennego (5.6.4, 5.9.2) ----------
// Do formularza z pliku `/konfiguracja.txt` w pamięci panelu, wiersze `klucz=wartość` (`#` komentarz):
//   wymiana.maska|filtr = RRRR-MM-DD   data ostatniej wymiany (R-MAS-06: bez daty bez S-K-35). Stary `wymiana.poduszka`
// maska = późniejsza z dat `wymiana.maska` i `wymiana.poduszka`
//   okres.maska = {miesiące}           okres wymiany maski (domyślnie 6, refundacja co 6 miesięcy)
//   okres.filtr_tyg = {tygodnie}       okres wymiany filtra (domyślnie 4). Stary `okres.filtr` (miesiące, bez
//                                      jednostki) = ×4 tygodnie, ignorowany przy obecnym `okres.filtr_tyg`
// Ignorowane bez błędu: wymiana.|okres. poduszka, rura, komora; poduszka = silikon|pianka
//   pylki = brzoza,trawy,olcha,bylica                      ustawienie `Pyłki w dzień` (domyślnie puste, R-KON-45)
//   pylek.brzoza|trawy|olcha|bylica = {ziarna/m³}          próg „górne 10% sezonu” [PRÓG] do policzenia z sezonu
//   profil = publiczny|prywatny                            ustawienie „Profil publiczny” (5.6.4, R-EKR-51: ekran
// „Co znaczą te liczby” z S8); domyślnie prywatny
//   poczatek_leczenia = RRRR-MM-DD                         „Początek leczenia” (R-SKI-08): karencja L-01 w CPAP
//                                                          i AutoSet tylko w 56 dniach od tej daty; bez wiersza brak
//   noc_zrodlo = plik|porownanie|json                      źródło nocy przy AirBridge od v2.3.1 (ADR-0036): pliki
//                                                          sesji liczone na panelu, raport JSON jako zapas (domyślnie,
//                                                          od 11.11.2026), raport JSON z porównaniem z nocą z plików
//                                                          (src/porownanie.h) albo sam raport JSON
//   noc_json = m02,m08                                     metryki, które przy `plik` zostają z raportu JSON
//   co2 = tak|nie                                          ekrany CO2 (S-K-01, S-K-02, S-K-13, wariant CO2 S-K-33)
//                                                          i ocena wieczorna z Wi-Fi (R-KON-01); bez wiersza: według
// lokalnego źródła otoczenia (
//                                                          5.8.8); `tak` bez źródła nic nie włącza (brak danych CO2)
//   dzien.wygaszacz = tak|nie                              wygaszacz z obrazkiem dnia (R-KON-77), domyślnie `tak`;
//                                                          `obrazek` w `dzien.ukryte` ukrywa obrazek w obu rolach
//   dzien.wygaszacz_min = {minuty}                         próg bezczynności wygaszacza (R-KON-74), 30–720,
//                                                          domyślnie 180
// Nieznany klucz i zła wartość: pominięte (wartość domyślna), z wpisem w `bledy`.
const float PYLEK_PROG_DOMYSLNY[PYL_N] = {100, 50, 50, 30};  // [PRÓG] zastępczy do statystyki sezonu
// 5.6.4 „Uwaga o masce od” (R-KONF-42): zakres [PRÓG R-KONF-42.zakres_min, R-KONF-42.zakres_max] i wartość domyślna
// [PRÓG S-K-10.uwaga_o_masce], L/min
// (model pasm wycieku: 10–15 L/min „istotny, odczyty prawidłowe”): 10–15, domyślnie 10
// Źródło nocy bez wiersza `noc_zrodlo` (ADR-0036 „Zmiana 2026-11-11”): 1 = `plik`
const int NOC_ZRODLO_DOMYSLNE = 1;
const int UWAGA_O_MASCE_MIN = 10, UWAGA_O_MASCE_MAX = 15, UWAGA_O_MASCE_DOMYSLNIE = 10;
// R-KON-74: próg bezczynności wygaszacza, minuty [PRÓG R-KON-74.bezczynnosc, R-KON-74.zakres_min, R-KON-74.zakres_max]
const int WYGASZACZ_MIN_DOMYSLNIE = 180, WYGASZACZ_MIN_OD = 30, WYGASZACZ_MIN_DO = 720;
struct UstawieniaDnia {
  ElementWymiany sprzet[EL_N];
  PylkiDnia pylki;
  bool profilPubliczny = false;  // `profil=publiczny`; bez wiersza albo przy złej wartości prywatny
  int co2 = -1;  // 1 = `co2=tak`, 0 = `co2=nie`, -1 = bez wiersza (według źródła otoczenia, co2Wlaczone)
  // (formularz U-03, 5.6.4): humor, dźwięk, cisza nocna, podpowiedź „co do poprawy”, miasto do pogody. -1 = bez
  // wiersza albo zła wartość: wartość domyślna ze spec (humor `normalny`, dźwięk włączony, cisza 22:00–08:00, podpowiedź
  // włączona; stosuje je dyrygent, bo dla ciszy i dźwięku zostaje jeszcze wpis w NVS z etapu 1).
  int humor = -1;       // 0 `wylaczony`, 1 `oszczedny`, 2 `normalny` (wartości z polskimi znakami też czytane)
  int buzzer = -1;      // `buzzer=tak` 1, `buzzer=nie` 0
  int ciszaOd = -1, ciszaDo = -1;  // `cisza=GG:MM-GG:MM`, minuty doby; początek różny od końca (R-BUZ-02)
  int podpowiedz = -1;  // `podpowiedz=tak|nie` (R-HIP-02, R-HIP-07)
  std::string miasto;   // `miasto=` z tabeli src/miasta.h; "" = bez wiersza
  // `poczatek_leczenia=RRRR-MM-DD` (5.6.4 „Początek leczenia”, R-SKI-08; ustawia osoba wydająca panel, tylko plik K,
  // bez formularza i przycisków); "" = data nieznana: L-01 w CPAP i AutoSet bez karencji
  std::string poczatekLeczenia;
  // `uwaga_o_masce=NN` (5.6.4 „Uwaga o masce od”, R-KONF-42): liczba całkowita 10–15 L/min [PRÓG R-KONF-42.zakres_min,
  // R-KONF-42.zakres_max]; -1 = bez wiersza albo zła wartość (wiersz w `bledy`): domyślnie UWAGA_O_MASCE_DOMYSLNIE (10).
  // Steruje tylko wariantem opisowym S-K-10 `Wyciek widoczny` (P95 od ustawienia do < 15 L/min); S-K-10 w tonie
  // „problem” (od 15), nocy niepewnej i dużego wycieku 24 L/min nie zmienia.
  int uwagaOMasce = -1;
  // `dzien.wygaszacz_min=NN` (R-KON-74): próg bezczynności wygaszacza w minutach, liczba całkowita WYGASZACZ_MIN_OD…
  // WYGASZACZ_MIN_DO; -1 = bez wiersza albo zła wartość (wiersz w `bledy`): WYGASZACZ_MIN_DOMYSLNIE.
  int wygaszaczMin = -1;
  int wygaszaczMinSkuteczne() const { return wygaszaczMin < 0 ? WYGASZACZ_MIN_DOMYSLNIE : wygaszaczMin; }
  // `dzien.wygaszacz=tak|nie` (R-KON-77): rola wygaszacza obrazka dnia; 1 tak, 0 nie, -1 bez wiersza albo zła wartość
  // (wiersz w `bledy`): włączony. Wyłącza tylko wygaszacz; obrazek na D5 (zastępstwo R-KON-54) zostaje, a `obrazek`
  // w `dzien.ukryte` (R-KON-71) ukrywa obrazek w obu rolach. Zasilanie bez wpływu (decyzja właściciela projektu 16.11.2026).
  int wygaszacz = -1;
  bool wygaszaczWlaczony() const { return wygaszacz != 0; }
  // Współrzędne pogody i wschodu słońca z miasta z `miasto=` (lista 18 miast wojewódzkich, src/miasta.h); NAN = bez
  // wpisu (dyrygent bierze wtedy `POGODA_LAT` i `POGODA_LON` z secrets.h, a bez nich Katowice). Wpis z konfiguracji
  // ma pierwszeństwo przed secrets.h.
  double pogodaLat = NAN, pogodaLon = NAN;
  // `noc_zrodlo=plik|porownanie|json` (ADR-0036 pkt 5, „Zmiana 2026-11-09” i „Zmiana 2026-11-11”): źródło nocy przy
  // AirBridge od v2.3.1; 1 = pliki sesji (src/noc_pliki.h), 0 = `json`, 2 = `porownanie` (noc z raportu JSON
  // i porównanie z nocą z plików, src/porownanie.h), -1 = bez wiersza albo zła wartość (domyślnie jak `plik`,
  // NOC_ZRODLO_DOMYSLNE, decyzja właściciela 11.11.2026). `noc_json=` lista metryk (m01, m02, …), które przy
  // `plik` zostają z raportu JSON; zła lista = wiersz pominięty.
  int nocZrodlo = -1;
  // Źródło nocy po zastosowaniu wartości domyślnej: 1 `plik`, 0 `json`, 2 `porownanie` (nigdy -1).
  int nocZrodloSkuteczne() const { return nocZrodlo < 0 ? NOC_ZRODLO_DOMYSLNE : nocZrodlo; }
  std::set<std::string> nocJson;
  bool nocJsonZly = false;  // wiersz `noc_json` z nieznanym ID: przy `plik` cała noc z raportu JSON (bezpieczny kierunek)
  // ADR-0045 (R-KOK-34, R-KON-70): `kokpit.kolejnosc`, `kokpit.ukryte`, `dzien.kolejnosc`, `dzien.ukryte`
  // (src/uklad_ekranow.h); bez wierszy układ domyślny. Kokpit stosuje dyrygent (sypialniaWKokpicieB44() na starcie
  // obsluz() i przed rysowaniem), dzień idzie do slotu dziennego (WejscieSlotu::uklad); wartości złe pominięte
  // z jednym wpisem na klucz w `bledy`.
  UkladEkranow ekrany;
  // `eink.pelne_co=N` (R-NF-25, src/odswiezanie.h): po tylu odświeżeniach częściowych z rzędu pełne, 0 = częściowe
  // wyłączone; liczba całkowita 0…odswiezanie::PELNE_CO_MAX, bez pola w formularzu U-03 (przełącznik do prób na
  // panelu). -1 = bez wiersza albo zła wartość (wiersz w `bledy`): odswiezanie::PELNE_CO_DOMYSLNIE.
  int pelneCo = -1;
  // `kabel_bez_snu=tak|nie` (ADR-0047, T-WYM-16): na zasilaniu zewnętrznym czekanie zamiast głębokiego snu.
  // -1 = bez wiersza albo zła wartość (wiersz w `bledy`): włączone (decyzja właściciela projektu 16.11.2026); 0 = `nie`, wyłącznik
  // awaryjny: głęboki sen jak przed 16.11.2026; 1 = `tak`. Bez pola w formularzu U-03.
  int kabelBezSnu = -1;
  bool kabelBezSnuWlaczone() const { return kabelBezSnu != 0; }
  // ADR-0049: `start.program`, `start.dzien`, `start.nawilzacz`, `start.objawy`, `start.ponad` (src/dobry_start.h)
  dobry_start::Ustawienia start;
  std::vector<std::string> bledy;
  UstawieniaDnia();
};
UstawieniaDnia parsujUstawieniaDnia(const std::string& tresc);
// `GG:MM-GG:MM` → minuty doby; false przy złym zapisie albo gdy początek równa się końcowi (R-BUZ-02).
bool ciszaZTekstu(const std::string& tekst, int& od, int& doo);
// (5.8.8): ekrany CO2 i ocena wieczorna z Wi-Fi włączone w tym egzemplarzu. `zrodlo` = lokalne źródło
// otoczenia skonfigurowane i w sieci lokalnej (dyrygent otoUzywalne()); `ustawienie` = UstawieniaDnia::co2. Bez źródła
// zawsze false (egzemplarz testowy: bez ekranów CO2 i bez wybudzeń z Wi-Fi wieczorem), ze źródłem true, chyba że `co2=nie`.
bool co2Wlaczone(bool zrodlo, int ustawienie);
bool wyciekDrzemki(const Drzemka& d);  // R-KON-58: P95 drzemki > próg S-K-10

// ---------- stopka dzienna (R-KON-51) ----------
// Lewo `noc {czas} · AHI {a} · wyciek {w}` (gdy noc nie jest ubiegłą: `noc {dd.mm} …`), po drzemce
// `noc {czas} · drzemka {czas} · AHI {a}`; prawo `za oknem {t}° · przy łóżku {t}°`. Przy braku miejsca w 744 px
// ustępują po kolei: `za oknem {t}° · `, ` · wyciek {w}`, czas nocy. Brak wartości: `–` (R-DANE-07).
struct StopkaDzienna {
  std::string lewo, prawo;
  // R-KON-69: bateria poniżej progu: w miejscu `lewo` ikona baterii i `{x}% · podłącz kabel` (src/ekrany.h
  // tekstNiskiejBaterii()); ustawia dyrygent ze stanu baterii ekranu
  bool bateriaNiska = false;
};
StopkaDzienna stopkaDzienna(const Noc* noc, bool nocUbiegla, int drzemkaMin, float zaOknem, float przyLozku);

// ---------- wybór ekranu slotu dziennego (7.2 część „Dzień”) ----------
// Czy ekran z rana zostaje na cały dzień (D0): pytanie R0 bez odpowiedzi, S5, ekran L, S-K-12, problem poranny.
// Z grupy typu (grupaRano) i z ekranu z kolejki pytanie tak/nie albo wzorzec L dyrygent rozpoznaje przed
// wywołaniem (rysujDzienB5()), więc tu typ z wyboru ekranu dnia; ustępują tylko grupy R6…R9.
int grupaRano(const std::string& typEkranuDnia);  // numer grupy części „Rano” tabeli 7.2 (0…9), 0 = R0…R5 / nieznany
bool zostajeRano(const std::string& typEkranuDnia, bool pytanieR0BezOdpowiedzi);

// Wygaszacz ekranu (R-KON-73…R-KON-77, decyzje właściciela projektu z 16.11.2026, ADR-0027 „Zmiana 2026-11-16”): obrazek dnia
// S-K-38 nie stoi już w pierwszym bloku slotu (R-KON-50 wycofane, zob. R-KON-73). Ma dwie role: zastępstwo D5 jak
// dotąd (R-KON-54) i wygaszacz po progu bezczynności liczonym od późniejszej z chwil: start slotu dziennego, ostatnie
// naciśnięcie przycisku. Próg z klucza `dzien.wygaszacz_min` (minuty, WYGASZACZ_MIN_OD…WYGASZACZ_MIN_DO [PRÓG
// R-KON-74.zakres_min, R-KON-74.zakres_max], bez wiersza albo przy złej wartości WYGASZACZ_MIN_DOMYSLNIE [PRÓG
// R-KON-74.bezczynnosc]). Wyłącznik samego wygaszacza: `dzien.wygaszacz=nie` (R-KON-77, tylko z formularza na
// telefonie); `obrazek` w `dzien.ukryte` (R-KON-71) ukrywa obie role. Zasilanie (bateria, kabel) nie ma wpływu.
// Stałe WYGASZACZ_MIN_* przy ustawieniach slotu (wyżej).
// Czy minął próg bezczynności: slotEp ≠ 0 i ≤ teraz, liczone od max(slotEp, nacisniecieEp), co najmniej progMin pełnych
// minut zegara między tą chwilą a teraz (minuty ucięte: start slotu 08:00:05, ocena 11:00:00 = 180 min). progMin spoza
// zakresu = WYGASZACZ_MIN_DOMYSLNIE. Naciśnięcie z przyszłości (zepsuta pamięć RTC, zegar cofnięty) liczy się jak brak
// naciśnięcia (od startu slotu); start slotu z przyszłości: bez wygaszacza.
bool poraWygaszacza(uint32_t teraz, uint32_t slotEp, uint32_t nacisniecieEp, int progMin);

enum class EkranSlotu : uint8_t { BRAK, SK33, SK34, SK35, SK36, SK37, SK38, SK39 };
const char* idEkranuSlotu(EkranSlotu e);  // `S-K-33` …

struct WejscieSlotu {
  std::string dzis;
  int minuta = -1;                 // minuta doby
  const Prognoza* prognoza = nullptr;  // ważna prognoza albo nullptr (R-KON-53)
  float przyLozku = NAN;
  int co2 = -1;
  PylkiDnia pylki;
  const ElementWymiany (*sprzet)[EL_N] = nullptr;  // daty wymiany (5.9.2); nullptr = brak zapisów
  std::string ostatnieSK35;        // ostatni dzień z S-K-35 ("" = nigdy)
  bool wspolrzedne = false;
  double lat = NAN, lon = NAN;
  bool obrazekJest = false;        // jest obrazek do pokazania dziś (src/obrazki.h)
  bool liniaUwagi = false;         // R-KON-54, R-KON-75: z linią uwagi bez wygaszacza
  bool bezczynnosc = false;        // R-KON-73: minął próg bezczynności (poraWygaszacza()) i wygaszacz włączony (R-KON-77)
  // linia uwagi z kolejki 7.5 (src/kolejka.h ustawLinie()): rysowana na ekranie slotu pod nagłówkiem; gdy układ nie
  // ma miejsca, ustępuje wizualizacja (słupki, linia, oś, fakty, lista), nie linia
  std::string linia;
  bool liniaA = false, liniaOstrzezenie = false;
  Drzemka drzemka;                 // R-KON-58
  // ADR-0045 (R-KON-70…R-KON-72): kolejność D1…D4 i ekrany dzienne wyłączone z telefonu; domyślnie jak przed nim
  UkladDnia uklad;
};
struct WyborSlotu {
  EkranSlotu ekran = EkranSlotu::BRAK;  // BRAK = ekran z rana zostaje (R-KON-54: bez współrzędnych i obrazka)
  bool wygaszacz = false;  // S-K-38 w roli wygaszacza (R-KON-73), nie zastępstwa D5: ikona aktualizacji zostaje (R-EKR-61)
  WynikWietrzenia okno;
  WynikJutra jutro;
  WynikWymiany wymiana;
  WynikPylku pylek;
  WynikSlonca slonce;
};
// Kolejność: D0a S-K-39 > D1 S-K-33 (po 16:00 za D2, gdy okno nie trwa i nie zaczyna się w ciągu 60 min) > D2 S-K-34
// > D3 S-K-35 > D4 S-K-36 > D5 S-K-37 (zastępstwo S-K-38, R-KON-54). Wygaszacz S-K-38 (R-KON-73, R-KON-75, decyzja
// 16.11.2026 22:08, W3): tylko po progu bezczynności (w.bezczynnosc); zasłania D5, ekran z rana, S-K-33 `Wietrz od …`
// i `Dziś bez wietrzenia.`, S-K-34, S-K-36; nigdy D0a, `Przewietrz teraz.`, okna trwającego (`Wietrz do …`), upału
// (`Okna zamknięte …`), S-K-35, linii uwagi ani bez obrazka; stan „niska bateria” go nie blokuje (stopka ostrzega).
// Zasłanianie dosłowne (16.11.2026): po progu wynik to ten sam ekran co bez wygaszacza albo S-K-38,
// nigdy dalszy kandydat D1…D4.
// Bez prognozy S-K-33, S-K-34 i S-K-36 milczą (R-KON-53); bez czujnika panelu S-K-33 milczy (5.8.10).
// ADR-0045: D1…D4 w kolejności w.uklad.kolejnosc (R-KON-70; R-KON-46 dalej stawia S-K-34 przed S-K-33 po 16:00, gdy
// S-K-33 jest wyżej), ekran wyłączony (w.uklad.pokaz) nie jest kandydatem, a panel bierze następnego (R-KON-71);
// S-K-39 i `Przewietrz teraz.` (gdy ekrany CO2 włączone) zawsze przed listą i bez przełącznika (R-KON-72); bez S-K-37
// i S-K-38 zostaje ekran z rana (BRAK, jak bez współrzędnych i obrazków, R-KON-54).
WyborSlotu wybierzSlot(const WejscieSlotu& w);

// Teksty i układ ekranu slotu dziennego (5.8.4), bez rysowania: etykieta, podpis nad godziną (`wietrz do`), godzina
// tytuł (gdy bez godziny), powód, krok; rysowanie w src/ekrany_kontekst.cpp.
struct TekstySlotu {
  std::string etykieta, podpis, godzina, tytul, powod, krok;
};
TekstySlotu tekstySlotu(const WyborSlotu& w, const WejscieSlotu& we);
std::string hDoMin(int godzina);    // 15 → "15:00"
