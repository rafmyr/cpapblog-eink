// SPDX-License-Identifier: GPL-3.0-or-later
// Aktualizacja firmware z wydań GitHub (ADR-0032, ADR-0038;
// spec funkcjonalna R-NF-32…R-NF-36, R-KONF-43…R-KONF-46, R-EKR-61, ekran U-07; spec techniczna 8.8, 8.10).
// Rdzeń bez Arduino i bez sieci (ADR-0021): porównanie wersji, odczyt odpowiedzi GitHub API `releases/latest`
// i pliku opisu `wydanie.json`, decyzja „czy instalować”, reguły automatu, stan trwały między wybudzeniami (NVS
// `aktual`), skrót SHA-256 liczony w locie, teksty U-07 i wierszy U-01, U-05. Sieć (HTTPS, zapis drugiego slotu
// aplikacji) robi adapter `Wydania` (src/adaptery.h); dyrygent (src/dyrygent.cpp) tylko woła te funkcje.
// Testy na komputerze: test/testy_aktualizacja.cpp, test/testy_aktualizacja_dyrygent.cpp.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "adaptery.h"

// Adres repozytorium wydań (ADR-0038 pkt 5): parametr budowy `-DREPO_WYDAN="wlasciciel/nazwa"` z wartością domyślną
// (09.11.2026). W publicznym kodzie domyślnie to repozytorium; wydania z gotowym obrazem są w nim dopiero po opinii
// prawnika (ADR-0033 pkt 6), a inne źródło wydań podaje się parametrem budowy.
#ifndef REPO_WYDAN
#define REPO_WYDAN "rafmyr/cpapblog-eink"
#endif
// Model płytki w opisie wydania (`plytka`), jak nazwa płytki PlatformIO (firmware/boards/reterminal_e1001.json).
#ifndef PLYTKA_WYDANIA
#define PLYTKA_WYDANIA "reterminal_e1001"
#endif

namespace aktualizacja {

// ---------- wersje (SemVer 2.0.0: MAJOR.MINOR.PATCH[-pre][+budowa]) ----------
struct Wersja {
  long glowna = 0, poboczna = 0, poprawka = 0;
  std::vector<std::string> przed;  // identyfikatory wersji przedpremierowej (`beta`, `1`); puste = wydanie
};
// Przyjmuje `1.2.3`, `1.0.0-beta.1`, `v1.0.1` (tag), `1.0.0-beta.1+abc1234` (budowa pomijana). Zwraca false przy
// innym zapisie (pusty, liczba z zerem na początku, liczba > 999999, identyfikator spoza [0-9A-Za-z-], > 64 znaki).
bool parsujWersje(const std::string& t, Wersja& w);
// Kolejność SemVer: -1 a < b, 0 równe, 1 a > b (wersja przedpremierowa niższa od wydania o tym samym numerze).
int porownaj(const Wersja& a, const Wersja& b);
// true tylko, gdy obie wersje są poprawne i `kandydat` jest wyższy od `wlasna`. Zła wersja = false (bez instalacji).
bool nowsza(const std::string& kandydat, const std::string& wlasna);
// Wersja bez przedrostka `v` i bez części budowy: `v1.0.1+abc` → `1.0.1`.
std::string bezBudowy(const std::string& t);

// ---------- mini JSON (tylko do odpowiedzi GitHub i wydanie.json) ----------
struct Json {
  enum class Typ : unsigned char { NUL, LOGICZNA, LICZBA, TEKST, TABLICA, OBIEKT } typ = Typ::NUL;
  bool logiczna = false;
  std::string tekst;  // TEKST: wartość po dekodowaniu; LICZBA: zapis z pliku
  std::vector<Json> elementy;                          // TABLICA
  std::vector<std::pair<std::string, Json>> pola;      // OBIEKT (kolejność z pliku)
  const Json* pole(const char* nazwa) const;           // nullptr, gdy nie ma albo to nie obiekt
};
const size_t JSON_MAKS_GLEBOKOSC = 16;
// Cały tekst `t` jako jedna wartość JSON (RFC 8259; \uXXXX do UTF-8 z parami zastępczymi). false przy błędzie,
// zagnieżdżeniu ponad JSON_MAKS_GLEBOKOSC albo znakach po wartości.
bool parsujJson(const std::string& t, Json& wynik);
// Liczba całkowita nieujemna z pola LICZBA albo TEKST (same cyfry, bez znaku, ≤ maks). false przy złym zapisie.
bool liczbaZJson(const Json* j, uint64_t maks, uint64_t& wynik);

// ---------- odpowiedź GitHub API `GET /repos/{repo}/releases/latest` ----------
struct Zasob {
  std::string nazwa, adresApi;  // `name`, `url` (https://api.github.com/repos/…/releases/assets/{id})
  uint64_t rozmiar = 0;
};
struct Wydanie {
  std::string tag;  // `tag_name`, np. `v1.0.1`
  Zasob opis, obraz;  // `wydanie.json` i `cpap-eink-{wersja}.bin`; adresApi "" = brak zasobu
};
const char* const NAZWA_OPISU = "wydanie.json";
// Nazwa pliku obrazu wydania dla wersji (bez `v`): `cpap-eink-1.0.1.bin`.
std::string nazwaObrazu(const std::string& wersja);
// Odczyt odpowiedzi; false, gdy to nie obiekt, nie ma `tag_name` albo zasobu `wydanie.json` (obraz może brakować:
// wtedy opis mówi o pliku, którego nie ma, i decyzja daje powód „uszkodzony”). Adres zasobu tylko https://api.github.com/.
bool parsujWydanie(const std::string& json, Wydanie& w);

// ---------- kanał wczesny (R-KONF-62, decyzja właściciela projektu z 23.11.2026, ADR-0032 „Zmiana 2026-11-23”) ----------
// Przy `aktualizacja_wczesna=tak` w /konfiguracja.txt panel pyta o listę wydań tego samego repozytorium (ten sam host
// i token, ADR-0001, ADR-0038) zamiast `releases/latest`, które z definicji GitHuba pomija pre-release. Bez wiersza
// (domyślnie) wszystko jak dotąd: `releases/latest`, te same zapytania sieciowe, decyzje, log i NVS; jeden odczyt
// pliku więcej.
const char* const KLUCZ_KANALU_WCZESNEGO = "aktualizacja_wczesna";
// Ile ostatnio utworzonych wydań bierze lista (`per_page`) [PRÓG]: 5 wydań z notą i dwoma zasobami to ok. 25 KB,
// w tym samym limicie odpowiedzi MAKS_ODPOWIEDZI_API co `releases/latest` (ta sama pamięć HTTP/TLS i JSON na panelu).
// Wydanie starsze niż piąte od końca (np. poprawka starej linii opublikowana po nowszych) lista pomija.
const int WYDAN_NA_LISCIE = 5;
// `https://api.github.com/repos/{repo}/releases?per_page={WYDAN_NA_LISCIE}`; "" przy złej nazwie repo (jak
// adresNajnowszego()).
std::string adresListyWydan(const std::string& repo);
enum class WynikListy : unsigned char {
  WYBRANE = 0,  // `w` = wydanie o najwyższej wersji SemVer
  BRAK,         // lista poprawna, ale bez wydania do wzięcia (pusta, same robocze, bez `wydanie.json`, zły tag)
  NIECZYTELNA,  // to nie tablica JSON albo błąd składni
};
// Wybór z odpowiedzi `GET /repos/{repo}/releases`: pomija wydania robocze (`draft` = true albo w innej postaci niż `false`;
// bez pola = zwykłe), wydania bez zasobu `wydanie.json` przez API i tagi spoza SemVer (parsujWersje); z pozostałych, zwykłych
// i oznaczonych jako pre-release, bierze najwyższe według porownaj() (SemVer), nie według kolejności z API. Przy dwóch
// tagach o tej samej wersji (np. `v1.0.1` i `1.0.1`) zostaje pierwszy z listy.
// `l` (opcjonalne, do logu): ile obiektów wydań było na liście i ile z nich roboczych.
struct LicznikiListy {
  int wydan = 0, roboczych = 0;
};
WynikListy wybierzZListy(const std::string& json, Wydanie& w, LicznikiListy* l = nullptr);

// ---------- opis wydania `wydanie.json` (ADR-0032) ----------
// {"wersja":"1.0.1","plytka":"reterminal_e1001","uklad":"0x1a2b3c4d","rozmiar":1700000,
//  "sha256":"…64 znaki szesnastkowe…","plik":"cpap-eink-1.0.1.bin"[,"nowosci":["…","…"]]}
// `nowosci` (od 14.11.2026, R-KONF-56): lista „Co nowego” na U-07, podawana przy wydaniu (tools/wydanie/wydaj.py
// --nowosc). Pole opcjonalne: wydania bez niego i panele bez jego obsługi działają jak dotąd.
const size_t NOWOSCI_MAKS = 3;           // [PRÓG] linii „Co nowego” na U-07
const size_t NOWOSC_MAKS_ZNAKOW = 40;    // [PRÓG] znaków (punktów kodowych) w linii
struct OpisWydania {
  std::string wersja, plytka, sha256, plik;
  uint32_t uklad = 0;
  uint64_t rozmiar = 0;
  std::vector<std::string> nowosci;  // po nowosciZJson(); pusta = blok „Co nowego” pominięty
};
// false przy braku pola, złej wersji, skrócie innym niż 64 znaki [0-9a-f], rozmiarze 0 albo > 64 MB, układzie 0.
// `nowosci` nie decyduje o poprawności opisu, gdy reszta opisu jest poprawna: zły typ pola albo elementu = lista
// pusta; pole, którego parsujJson() nie przyjmuje (samotny surogat, surowy znak sterujący, zagnieżdżenie ponad
// JSON_MAKS_GLEBOKOSC), jest wycinane i opis czytany bez niego. Opis dłuższy niż MAKS_OPISU (także przez
// długie `nowosci`) albo z błędem składni poza tym polem jest odrzucany w całości.
bool parsujOpis(const std::string& json, OpisWydania& o);
// Linie „Co nowego” z pola `nowosci` (R-KONF-56): tylko elementy tekstowe, bez znaków sterujących (< 0x20 i 0x7F),
// spacje na brzegach obcięte, puste pominięte, najwyżej NOWOSCI_MAKS pierwszych; linia dłuższa niż
// NOWOSC_MAKS_ZNAKOW skrócona do NOWOSC_MAKS_ZNAKOW − 1 znaków i `…`. Pole brak albo nie tablica: lista pusta.
std::vector<std::string> nowosciZJson(const Json* pole);
// Treść `wydanie.json` (narzędzie wydania tools/wydanie/ pisze ten sam układ pól; test zgodności w pytest).
// `nowosci` tylko, gdy lista niepusta, jako ostatnie pole; teksty z ucieczką jak json.dumps(ensure_ascii=False).
std::string zapiszOpis(const OpisWydania& o);

// ---------- adresy i token (R-NF-32, R-NF-36) ----------
const char* const HOST_API = "api.github.com";
// Host z adresu `https://host[:port]/…` małymi literami; "" przy adresie bez https:// albo pustym hoście.
std::string hostZAdresu(const std::string& url);
// Token wolno dołączyć tylko do żądania https:// pod HOST_API (także po przekierowaniu: inny host = bez tokenu).
bool wolnoWyslacToken(const std::string& url);
// `https://api.github.com/repos/{repo}/releases/latest`; "" przy złej nazwie repo (inna niż `wlasciciel/nazwa`
// ze znakami [A-Za-z0-9._-]).
std::string adresNajnowszego(const std::string& repo);
// Nagłówki żądania do `url`: Accept, User-Agent, wersja API i (tylko gdy wolnoWyslacToken) Authorization.
std::vector<std::pair<std::string, std::string>> naglowki(const std::string& url, const std::string& token,
                                                          bool binarne);
// logika adaptera WydaniaEsp32 testowalna na PC:
// Kod HTTP przekierowania, za którym adapter idzie (301, 302, 303, 307, 308).
bool kodPrzekierowania(int kod);
// Następny adres po przekierowaniu z `obecny` na `location`: adres bezwzględny tylko `https://`, adres zaczynający się
// od `/` na tym samym hoście (port z obecnego); "" = nie iść (pusty, `http://`, względny bez `/`, `//host`, dane
// logowania, krok > PRZEKIEROWAN_MAKS).
std::string nastepnyAdres(const std::string& obecny, const std::string& location, int krok);
// Czy ciało jest kompletne: przy znanym Content-Length (≥ 0) dokładnie tyle bajtów; bez niego (−1, HTTP/1.0 bez
// długości) tylko po zamknięciu połączenia przez serwer i bez przekroczenia limitu `maks` (0 = bez limitu).
bool cialoKompletne(long contentLength, size_t odczytane, bool zamkniete, size_t maks);
// Ile z kolejnego kawałka `n` bajtów zapisać do slotu, gdy zapisano już `zapisane` z `rozmiar` (nadmiar obcięty).
size_t ileZapisac(uint64_t zapisane, uint64_t rozmiar, size_t n);
// Limity HTTPS adaptera [PRÓG]: połączenie i uzgadnianie TLS osobno, krócej niż strażnik zadania (150 s).
const uint32_t LIMIT_POLACZENIA_MS = 20000;
const uint32_t LIMIT_PRZESTOJU_MS = 20000;  // bez nowych bajtów ciała
// Token wydań do pola formularza: 20–255 znaków [A-Za-z0-9_] (GitHub: `github_pat_…`, `ghp_…`).
bool tokenOk(const std::string& t);

// ---------- SHA-256 (FIPS 180-4), liczone w locie przy pobieraniu ----------
class Sha256 {
 public:
  Sha256();
  void dodaj(const uint8_t* d, size_t n);
  std::string hex();  // 64 znaki małymi literami; po hex() obiekt do wyrzucenia
 private:
  void blok(const uint8_t* b);
  uint32_t h_[8];
  uint8_t bufor_[64];
  size_t wBuforze_ = 0;
  uint64_t bajtow_ = 0;
};
std::string sha256(const std::string& dane);

// ---------- decyzje ----------
// Powód, dla którego czegoś nie zrobiono (tekst na U-07: tekstPowodu()).
enum class Powod : unsigned char {
  BRAK = 0,       // wszystko w porządku
  BRAK_SIECI,     // brak Wi-Fi albo internetu (także brak poprawnego czasu: HTTPS bez czasu nie działa)
  BRAK_TOKENU,    // token wydań pusty: bez żadnego zapytania (R-NF-33)
  TOKEN_ODRZUCONY,  // 401 albo 403 z GitHuba
  BATERIA,        // bateria poniżej progu, bez zasilania z kabla
  USZKODZONY,     // plik niezgodny ze skrótem, ucięty, brak pliku w wydaniu, zły opis
  INNY_PANEL,     // wydanie dla innej płytki albo układu partycji, większe niż slot
  NIE_WSTALA,     // nowa wersja nie potwierdziła startu, panel wrócił do poprzedniej (R-NF-35)
  NOC,            // slot nocny albo trwa terapia (`rop` = 1)
  NIE_NOWSZA,     // wydanie nie jest nowsze od własnej wersji (bez tekstu: stan „aktualna”)
  BRAK_WYDAN,     // 404 z `releases/latest`: repo bez wydań albo token bez dostępu (nie „aktualna”)
};
const Powod POWOD_OSTATNI = Powod::BRAK_WYDAN;  // kontrola zakresu wartości z NVS
// Tekst powodu na U-07; {x} procent baterii, {wersja} wersja panelu. "" dla BRAK i NIE_NOWSZA.
std::string tekstPowodu(Powod p, int procentBaterii, const std::string& wersja);
const char* nazwaPowodu(Powod p);  // do logu: `bateria`, `uszkodzony`, …

const int PROG_BATERII = 50;               // % [PRÓG] (R-KONF-45)
const uint64_t SLOT_APLIKACJI = 0x400000;  // 4 MB: app0, app1 w firmware/partycje-e1001.csv (spec techniczna 8.2)
struct DanePanelu {
  std::string wersja;           // własna (bez budowy)
  std::string plytka = PLYTKA_WYDANIA;
  uint32_t uklad = 0;           // ukladWkompilowany() (src/magazyn.h)
  uint64_t slot = SLOT_APLIKACJI;
  int procentBaterii = -1;      // M-25; -1 = brak odczytu (nie blokuje: pobranie do drugiego slotu nie psuje bieżącej wersji)
  bool zKabla = false;          // zasilanie z kabla (E1001 nie ma sygnału: zawsze false, warunek niesie próg baterii)
};
// R-NF-34 i karta 2 pkt 3: wersja wyższa, płytka i układ zgodne, rozmiar ≤ slot, bateria ≥ PROG_BATERII albo kabel.
// Kolejność sprawdzania: wersja, płytka, układ, rozmiar, bateria (pierwszy niespełniony warunek jest powodem).
Powod decyzjaInstalacji(const OpisWydania& o, const DanePanelu& p);
// Slot nocny 23:30–05:00 albo trwająca terapia (`rop` = 1): instalacja (ręczna i automat) czeka.
bool nocAlboTerapia(int minutaDoby, int rop);

// ---------- stan trwały (NVS, przestrzeń `aktual`, klucze ≤ 15 znaków) ----------
const char* const PRZESTRZEN = "aktual";
struct Stan {
  std::string znana;          // `znana`: najnowsza znana wersja wyższa od własnej ("" = brak)
  uint32_t sprawdzonoEp = 0;  // `spr_ep`: ostatnie udane sprawdzenie (czas unix)
  std::string sprawdzonoDnia; // `spr_d`: doba ostatniego sprawdzenia bez udziału użytkownika (RRRR-MM-DD)
  std::string probaWersja;    // `pr_w`: wersja ostatnich prób instalacji
  int probNieudanych = 0;     // `pr_n`: rozpoczęte i nieudane próby tej wersji (automat pomija po PROBY_MAKS)
  std::string probaDnia;      // `pr_d`: doba ostatniej próby (najwyżej jedna na dobę dla automatu)
  std::string instalujeZ, instalujeNa;  // `in_z`, `in_na`: wersja przed i po restarcie instalacji ("" = bez)
  bool automatyczna = false;  // `in_a`: ta instalacja z automatu (po restarcie bez ekranu U-07)
  Powod ostatni = Powod::BRAK;  // `wynik`: powód ostatniej nieudanej próby (U-05, U-07)
  std::string odrzucona;      // `odrz`: wersja, która nie wstała (U-05 `odrzucona {X}`)
  // R-KONF-46: zapytania automatu do sieci w dobie `au_d`: `au_n` zapytań, `au_k` = koniec prób do następnej doby
  std::string siecDnia;       // `au_d` (RRRR-MM-DD)
  int siecZapytan = 0;        // `au_n`
  bool siecKoniec = false;    // `au_k`
};
Stan wczytaj(Nvs& n);
void zapisz(Nvs& n, const Stan& s);  // zapis tylko zmienionych kluczy

const int PROBY_MAKS = 3;  // [PRÓG] (R-NF-35)

// R-KONF-45, R-KONF-46: automat sam włącza Wi-Fi i pyta GitHuba najwyżej AUTOMAT_ZAPYTAN_NA_DOBE razy na dobę
// (pierwsze zapytanie i jedno ponowienie po braku sieci albo internetu). Po błędzie trwałym (token odrzucony, brak
// wydań, wydanie bez obrazu albo z innym obrazem, zły skrót, inna płytka albo układ, wersja nie nowsza) koniec prób
// automatu do następnej doby. Warunki przejściowe lokalne (terapia, noc, sprawa A/B, bateria) są sprawdzane przed
// włączeniem Wi-Fi i nie liczą się tutaj.
const int AUTOMAT_ZAPYTAN_NA_DOBE = 2;  // [PRÓG] (10.11.2026)
bool bladTrwaly(Powod p);                // koniec prób automatu do następnej doby
bool automatMozeSiec(const Stan& s, const std::string& dzis);
// Po zapytaniu automatu (`zapytanie` = true: licznik doby) albo po odmowie instalacji bez nowego zapytania (false):
// koniec prób po błędzie trwałym.
void poZapytaniuAutomatu(Stan& s, const std::string& dzis, Powod wynik, bool zapytanie = true);
// Sprawdzenie dobowe z błędem trwałym: automat tej doby już nie pyta (bez zmiany licznika).
void poSprawdzeniuDobowym(Stan& s, const std::string& dzis, Powod wynik);

// R-KONF-46: sprawdzenie bez udziału użytkownika najwyżej raz na dobę, tylko w oknie Wi-Fi otwartym z innego powodu,
// z tokenem (R-NF-33), poprawnym czasem (karta 2 pkt 7) i poza slotem nocnym (rano po pobraniu nocy).
// nie w trakcie terapii (`rop` = 1): sprawdzenie czeka na okno Wi-Fi po pobraniu nocy, bez zużycia doby.
bool sprawdzicDzis(const Stan& s, const std::string& dzis, bool wifi, bool token, bool czas, int minutaDoby,
                   int rop = 0);

struct WejscieAutomatu {
  bool wlaczony = false;        // `aktualizacja_auto=tak` w /konfiguracja.txt
  std::string dzis;
  int minutaDoby = -1;
  int rop = -1;                 // AirBridge `rop`; 1 = trwa terapia
  bool sprawaAB = false;        // na ekranie dnia jest sprawa z pilnością A albo B
};
// R-KONF-45 i karta 2 pkt 6: automat instaluje `wersja` bez pytania tylko przy włączonym ustawieniu, w slocie
// dziennym, poza terapią, bez sprawy A/B na ekranie, raz na dobę i najwyżej PROBY_MAKS nieudanych prób tej wersji.
// Warunki z decyzjaInstalacji() sprawdza wołający osobno (po pobraniu opisu). Zwraca BRAK albo powód (log).
// odmowa z warunku przejściowego (noc, terapia, sprawa A/B, bateria) nie zużywa szansy doby: dyrygent pyta
// automat w każdym kolejnym wybudzeniu dnia; „jedna próba na dobę” liczy tylko rozpoczęte instalacje (zacznijProbe).
enum class PowodAutomatu : unsigned char { TAK = 0, WYLACZONY, NOC, SPRAWA_AB, DZIS_BYLA_PROBA, POMINIETA, BRAK_WERSJI };
PowodAutomatu decyzjaAutomatu(const Stan& s, const std::string& wersja, const WejscieAutomatu& w);
const char* nazwaPowoduAutomatu(PowodAutomatu p);

// Początek próby instalacji `wersja` (licznik prób tej wersji, doba próby, wersje przed i po restarcie).
void zacznijProbe(Stan& s, const std::string& wersja, const std::string& wlasna, const std::string& dzis, bool automat);
// Nieudana próba bez restartu (pobranie, skrót, zapis slotu): powód zostaje, licznik prób już policzony.
void nieudanaProba(Stan& s, Powod p);

// Stan po starcie (pierwsze wybudzenie po restarcie z instalacji): wołane z własną wersją przy każdym starcie.
enum class PoStarcie : unsigned char {
  NIC = 0,         // bez instalacji w toku
  ZAINSTALOWANO,   // działa wersja docelowa (R-KONF-44: `Zainstalowano wersję {X}.`)
  NIE_WSTALA,      // działa wersja sprzed instalacji: nowa nie potwierdziła startu (R-NF-35)
};
// Zmienia stan: ZAINSTALOWANO czyści próby i znaną wersję (gdy nie jest wyższa od nowej własnej), NIE_WSTALA zapisuje
// powód, wersję odrzuconą i zostawia licznik prób; inna wersja (np. wgranie kablem) czyści instalację bez wyniku.
PoStarcie poStarcie(Stan& s, const std::string& wlasna);
// Wynik sprawdzenia: `najnowsza` wersja wydania (""= nieznana). Ustawia `znana` (tylko wyższa od własnej).
void poSprawdzeniu(Stan& s, const std::string& najnowsza, const std::string& wlasna, uint32_t teraz,
                   const std::string& dzienAutomatu);

// ---------- ekran U-07 i wiersze U-01, U-05 (teksty przyjęte 09.11.2026) ----------
enum class StanU07 : unsigned char {
  SPRAWDZANIE = 0, AKTUALNA, NOWSZA, POBIERANIE, INSTALACJA, PO_UDANEJ, PO_NIEUDANEJ, WYLACZONE,
  SPRAWDZENIE_NIEUDANE,  // samo sprawdzenie się nie udało (tylko powód, bez „Aktualizacja się nie udała”)
  SAMOSPRAWDZENIE,       // po restarcie, przed potwierdzeniem nowej wersji (`Sprawdzam nową wersję…`)
};
const StanU07 STAN_U07_OSTATNI = StanU07::SAMOSPRAWDZENIE;
// powody, które pasują do nieudanego sprawdzenia (bez pobierania obrazu).
bool powodSprawdzenia(Powod p);
struct DaneU07 {
  StanU07 stan = StanU07::SPRAWDZANIE;
  std::string wlasna, nowa;  // {wersja}, {X}
  std::string sprawdzono;    // `DD.MM hh:mm`
  Powod powod = Powod::BRAK;
  int procentBaterii = -1;
  std::vector<std::string> coNowego;  // OpisWydania::nowosci wersji `nowa` (R-KONF-56); pusta = bez bloku
  // R-KONF-61: informacja po instalacji z automatu (R-KONF-45) na miejscu ekranu dnia; tylko PO_UDANEJ
  // (`Zaktualizowano do {X}.`, `Panel zaktualizował się sam.`) i PO_NIEUDANEJ (teksty R-NF-35 z `PROBOWAL_SAM`
  // przed powodem, decyzja 12); stopka
  // `▶ dalej` `↻ ekran dnia` (przyciski jak na ekranie dnia)
  bool samodzielnie = false;
};
// R-KONF-61, decyzja 12 z 15.11.2026: druga linia wariantu „nie udało się” po automacie (przed powodem R-NF-35)
const char* const PROBOWAL_SAM = "Panel próbował zaktualizować się sam.";
// Rysunek panelu 1 bit po lewej stronie U-07 (decyzja 3 z 14.11.2026, według projektu ekranu).
enum class ObrazekU07 : unsigned char {
  PANEL = 0,  // sam panel: sprawdzanie, aktualizacje wyłączone
  NOWA,       // strzałka w dół nad panelem: jest nowa wersja
  W_TOKU,     // strzałka wchodząca w ekran panelu, bez skali (nie udaje postępu): pobieranie, instalacja,
              // samosprawdzenie; który krok trwa, mówią kroki i `krok n z 4` w belce
  UDANA,      // odhaczenie: zainstalowano, wersja aktualna
  NIEUDANA,   // krzyżyk: nieudana aktualizacja albo nieudane sprawdzenie
};
// Stan jednego z czterech kroków (Pobranie, Sprawdzenie, Instalacja, Ponowny start).
enum class KrokU07 : unsigned char { CZEKA = 0, TERAZ, ZROBIONY, NIEUDANY };
const int KROKI_U07 = 4;
extern const char* const NAZWY_KROKOW_U07[KROKI_U07];  // `Pobranie`, `Sprawdzenie`, `Instalacja`, `Ponowny start`
struct TresciU07 {
  std::vector<std::string> linie;   // wszystkie zdania stanu (log `U-07 · …`, teksty przyjęte 09.11.2026)
  std::vector<std::string> stopka;  // grupy przycisków ◀ ▶ ↻ (pusta = bez stopki)
  // układ ekranu (R-KONF-57): belka `Aktualizacja` ze stanem w środku, rysunek, po prawej wersje albo zdanie
  // główne, zdania opisu, „Co nowego”, kroki; pas w negatywie nad stopką
  std::string stanBelki;            // `jest nowa wersja`, `krok 1 z 4`, `gotowe`, `nie udało się`, …
  ObrazekU07 obrazek = ObrazekU07::PANEL;
  std::string wersjaZ, wersjaNa;    // obie niepuste: dużym krojem `{wersjaZ} → {wersjaNa}` zamiast zdania głównego
  std::string glowne;               // zdanie główne (P28B), gdy bez wersji
  std::vector<std::string> opis;    // zdania pod spodem (P24S)
  std::vector<std::string> coNowego;
  bool kroki = false;
  KrokU07 krok[KROKI_U07] = {KrokU07::CZEKA, KrokU07::CZEKA, KrokU07::CZEKA, KrokU07::CZEKA};
  std::string pas;                  // pas w negatywie nad stopką (`Nie odłączaj zasilania.`); "" = bez pasa
};
TresciU07 trescU07(const DaneU07& d);
// Krok, na którym zatrzymała się nieudana próba (indeks 0…3) z powodu: brak sieci, brak albo odrzucony klucz, brak
// wydań, bateria, noc i inny panel (opis wydania dla innej płytki, innego układu partycji albo obraz większy niż slot,
// odrzucony przed pobraniem obrazu) = 0 (do pobrania nie doszło albo się przerwało), plik uszkodzony = 1
// (sprawdzenie: skrót, rozmiar albo zapis slotu), nowa wersja nie wstała = 3 (ponowny start); BRAK i NIE_NOWSZA = −1
// (bez kroków).
int krokNieudany(Powod p);
// Wiersz U-01 (karta 1.1): bez tokenu `Aktualizacje wyłączone…`, ze znaną nowszą `Jest wersja {X} · ↻ 2 s Aktualizacja`,
// inaczej `Oprogramowanie {wersja} · ↻ 2 s Sprawdź aktualizację`. Od 14.11.2026 tylko do logu (na ekranie kafelek).
std::string wierszU01(bool token, const std::string& znana, const std::string& wlasna);
// Kafelek ↻ na U-01 (R-KONF-55, decyzja 2 z 14.11.2026): `Oprogramowanie`, wersja własna (jedyne miejsce wersji na
// U-01) i stan: bez tokenu `wyłączone: włącz na telefonie`; znana nowsza `jest {X} · zainstaluj` (w odwróconym polu);
// po udanym sprawdzeniu `aktualne · sprawdzono {DD.MM}`; bez sprawdzenia `sprawdź aktualizację`.
struct KafelekOprogramowania {
  std::string wersja, stan;
  bool odwrocony = false;
};
KafelekOprogramowania kafelekU01(bool token, const std::string& znana, const std::string& wlasna,
                                 const std::string& sprawdzonoDnia);
// Dopisek U-05 przy wersji (ADR-0032 pkt 2): ` · jest wersja {X}` albo ` · odrzucona {X}` (R-NF-35); "" bez.
std::string dopisekU05(const Stan& s);
// Czy ekran dnia ma ikonę aktualizacji (R-EKR-61): znana wersja wyższa od własnej; przy automacie, który w tym
// jeden warunek, ikona stoi zawsze, gdy znana jest nowsza wersja niezainstalowana (także przy automacie).
bool ikonaAktualizacji(const Stan& s, const std::string& wlasna, bool token = true);  // bez tokenu bez ikony

// ---------- informacja po aktualizacji samodzielnej (R-KONF-61, decyzja 11 z 14.11.2026) ----------
// Jeden klucz NVS `u07_inf` w przestrzeni `aktual`: `{U|N}\n{wersja panelu}\n{X}\n{doba pokazania}` ("" = brak).
// Zapis tylko przy zmianie stanu: po potwierdzeniu wersji albo powrocie (czeka), przy pierwszym pokazaniu (doba)
// i po przycisku na informacji (""); koniec doby pokazania rozpoznaje odczyt, bez zapisu.
const char* const KLUCZ_INFORMACJI = "u07_inf";
enum class RodzajInformacji : unsigned char { BRAK = 0, UDANA, NIEUDANA };
struct Informacja {
  RodzajInformacji rodzaj = RodzajInformacji::BRAK;
  std::string wlasna;  // wersja panelu, dla której informacja jest prawdziwa (bez `+budowy`)
  std::string nowa;    // {X}: wersja zainstalowana (UDANA) albo odrzucona (NIEUDANA)
  std::string dzien;   // RRRR-MM-DD pierwszego pokazania; "" = czeka na pierwszy ekran slotu dziennego bez sprawy
};
// Zapis do NVS; BRAK = "". Wersje dłuższe niż 32 znaki albo z `\n` dają "" (nic do pokazania).
std::string zapisInformacji(const Informacja& i);
// Odczyt z NVS; zapis uszkodzony (zły rodzaj, pusta albo za długa wersja, zła doba, inna liczba pól) = BRAK.
Informacja odczytInformacji(const std::string& z);
// Czy informację wolno pokazać teraz: jest, dotyczy wersji `wlasna` (inna wersja, np. wgranie kablem: nie) i czeka
// albo stanęła w dobie `dzis` (z końcem doby pokazania znika).
bool informacjaAktualna(const Informacja& i, const std::string& wlasna, const std::string& dzis);

// Czas na wybór na U-07 w stanie „jest nowsza” (R-KONF-44): 5 min [PRÓG], potem U-01 bez instalacji.
const long U07_CZAS_S = 300;
// Limity sieci [PRÓG]: odpowiedź API (release z notą) i opis wydania; czas żądania i pobrania obrazu.
const size_t MAKS_ODPOWIEDZI_API = size_t{64} * 1024;
const size_t MAKS_OPISU = 4096;
const uint32_t LIMIT_API_MS = 20000;
const uint32_t LIMIT_OBRAZU_MS = 300000;

}  // namespace aktualizacja
