// SPDX-License-Identifier: GPL-3.0-or-later
// AirBridge od v2.2.0 (sprawdzone na v2.2.1), bez zależności od Arduino: logowanie HTTP Digest, odczyt
// JSON, status (terapia trwa, start sesji), lista nocy, raport nocy → rekord nocy (od wersji beta z polami wersji 2 formatu
// `noce`, src/noc.h wierszNocyV2()), plan pobrania historii.
// Żądania HTTP robi adapter (src/adaptery_esp32.cpp: HTTPClient, MD5 z mbedtls); testy: test/testy.cpp na
// syntetycznych JSON-ach.
//
// Protokół (spec Załącznik A, stan modułu v2.2.1):
//   GET /api/status                               → wersja, model (pna), tryb (mode), system, rop (0 czuwanie,
//                                                   1 terapia, …), edf_prefix (start sesji w trakcie terapii),
//                                                   report_state (w terapii "blocked", report_error "not_idle")
//   GET /api/report?source=edf&action=days        → 202 {"job":"edf:8"} albo 200 {"days":[{"day":20746,…}],…}
//   GET /api/report?source=edf&day=20746          → numer dnia (dni od 1970-01-01; day=20261020 → 400), też
//                                                   202/job, potem 200
//   GET /api/report?source=edf&job=edf:8          → odpytywanie joba aż do 200
// Każde żądanie z logowaniem Digest (pierwsze dostaje 401 z nonce). Panel tylko czyta (ADR-0001, R-LEK-08).
#pragma once
#include <cstdint>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "noc.h"

// Noc jest ważna w każdym trybie obsługiwanym (trybObslugiwany() w src/noc.h, ADR-0030); model aparatu nie decyduje.

// ---------- JSON (mały parser: raport ma 6–9 KB, lista nocy rośnie z każdą dobą na karcie modułu) ----------
struct Json {
  enum Typ : uint8_t { BRAK, NUL, BOOL, LICZBA, TEKST, TABLICA, OBIEKT };
  Typ typ = BRAK;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<std::string> klucze;  // obiekt: klucze w kolejności, wartości w `el`
  std::vector<Json> el;             // tablica albo wartości obiektu
  const Json& operator[](const char* klucz) const;  // brak klucza = wartość typu BRAK
  const Json& operator[](size_t i) const;
  bool jest() const { return typ != BRAK; }
  bool liczba(double& v) const;     // tylko typ LICZBA
};
// Filtr jak w ArduinoJson: zostają tylko klucze z listy (zakończonej {nullptr, nullptr}); `pod` = filtr wartości
// (dla tablicy: każdego elementu); pod == nullptr = cała wartość. Filtr nullptr = cały dokument.
struct FiltrJson {
  const char* klucz;
  const FiltrJson* pod;
};
bool parsujJson(const std::string& tekst, Json& wynik, const FiltrJson* filtr = nullptr);
// Filtr podrzędny „tylko obecność”: klucz zostaje z wartością NUL, a sama wartość jest sprawdzona i pominięta (bez
// alokacji), np. `days` w odpowiedzi z listą tysięcy dób.
extern const FiltrJson TYLKO_KLUCZ[];

// ---------- logowanie HTTP Digest (RFC 2617, qop=auth) ----------
struct WyzwanieDigest {
  bool ok = false;
  std::string realm, nonce, opaque, qop, algorytm;
};
// Nagłówek `WWW-Authenticate: Digest realm="…", qop="auth", nonce="…", opaque="…"` → pola; ok = false dla innego
// schematu (np. Basic) albo bez nonce.
WyzwanieDigest parsujWyzwanie(const std::string& naglowek);
using Md5Hex = std::string (*)(const std::string&);  // MD5 jako 32 małe znaki szesnastkowe
// Wartość nagłówka `Authorization` dla żądania `metoda uri` (uri ze ścieżką i zapytaniem, jak w żądaniu).
// nc: licznik użyć tego samego nonce (od 1); cnonce: losowy tekst klienta.
std::string naglowekDigest(const WyzwanieDigest& w, const std::string& uzytkownik, const std::string& haslo,
                           const std::string& metoda, const std::string& uri, unsigned nc, const std::string& cnonce,
                           Md5Hex md5);

// ---------- /api/status ----------
struct WersjaAB {
  int a = -1, b = -1, c = -1;
  bool ok() const { return a >= 0 && b >= 0 && c >= 0; }
};
WersjaAB parsujWersje(const std::string& tekst);  // "v2.2.1-25-gebbd145" → 2.2.1
bool wersjaObslugiwana(const WersjaAB& w);       // ≥ 2.2.0 (Załącznik A pkt 4)

struct StatusAB {
  std::string wersja, model, tryb, system, raport;  // version, pna, mode, system, report_state
  std::string bladRaportu, prefiks;                 // report_error, edf_prefix (RRRRMMDD_HHMMSS albo "")
  std::string edf;                                  // `edf`: "recording" w terapii, "post-processing" po niej
  int rop = -1;                                     // -1 = brak pola
  std::string seryjny;                              // `srn`: numer seryjny aparatu (R-NOR-25); "" = brak
  std::string czas;                                 // `esp_time`: "RRRR-MM-DD GG:MM" czas lokalny AirBridge; "" = brak
};
bool parsujStatus(const std::string& tresc, StatusAB& s);  // false: nie obiekt JSON
// (R-AW-18): `esp_time` "RRRR-MM-DD GG:MM" (dokładnie ta postać, data istniejąca, 00:00–23:59) → tm bez strefy.
// "--" (AirBridge bez czasu ważnego), pusty tekst i każda inna postać dają false.
bool parsujCzasAb(const std::string& tekst, std::tm& wynik);
// Terapia trwa albo moduł nie buduje raportów (`rop` = 1 albo `report_state` = "blocked"): panel nie pyta o raporty
// w tym wybudzeniu. To nie jest brak danych ani błąd (bez S5, bez E-03): na urządzeniu w terapii
// `report_state` = "blocked", `report_error` = "not_idle", a `action=days` nie zwraca dni.
bool raportyZablokowane(const StatusAB& s);
// Moduł po terapii przetwarza pliki (sprawdzone na module: `edf` = "post-processing", `report_state` =
// "waiting_storage" albo "storage_busy", a `action=days` daje 200 z pustą listą `days`). Lista z tej chwili nie mówi
// nic o nocach: panel jej nie pobiera ani nie używa; ostatnia noc bez rekordu jest „w budowie” (bez S5, bez E-10).
// Uwaga: `edf_prefix` zostaje wtedy ustawiony (start ostatniej sesji), więc godzina maski tylko przy `rop` = 1.
bool modulPrzetwarza(const StatusAB& s);
// Godzina założenia maski (ekran nocny): przy `rop` = 1 i `edf_prefix` w układzie RRRRMMDD_HHMMSS (start
// bieżącej sesji, czas lokalny modułu) minuta doby HH*60+MM; inaczej -1 (bez zgadywania, R-KON-33). Data z prefiksu
// musi być dziś albo wczoraj według zegara panelu (`dzis`, `minuta` = teraz), a start dziś nie może być późniejszy
// niż teraz; inaczej -1 (zły zegar modułu albo panelu, stara sesja).
int startZPrefiksu(int rop, const std::string& prefiks, const std::string& dzis, int minuta);

// ---------- odpowiedź HTTP i ponowienia ----------
// Kod panelu (ujemny, obok kodów HTTPClient): ciało odpowiedzi urwane (mniej bajtów niż w Content-Length albo błąd
// odczytu strumienia). Traktowane jak brak odpowiedzi (ponowienie), nigdy jak nieczytelny format.
const int AB_KOD_URWANE = -300;
// Kod panelu: ciało ponad limit trasy (limitOdpowiedziAB()). Nieudana próba jak AB_KOD_URWANE, nie E-03, ale bez
// ponowień w tym wybudzeniu: ten sam AirBridge odda ten sam rozmiar.
const int AB_KOD_ZA_DUZO = -301;
// górna granica ciała odpowiedzi AirBridge, osobno dla trasy. Większe ciało adapter odrzuca
// bez czytania (Content-Length) albo przerywa w trakcie (OdpowiedzHttp::zaDuzo); wynik AB_KOD_ZA_DUZO. Limit liczy
// się z trasy pierwotnej także przy odpytywaniu joba (lista i raport przychodzą zwykle pod ścieżką joba).
// Zmierzone na module v2.2.1: status 2,1 KB, lista 1,7 KB (3 doby, z narzutem), raport doby 6–9 KB. Lista rośnie z
// liczbą dób w katalogu AirBridge (do 4 096 sesji), wpis ok. 150 B (czytnik listy w airbridge.cpp). [PRÓG] 512 KiB
// to ok. 3 000 dób przy 150 B na wpis, ok. 900 dób przy 570 B (pomiar 01.11 bez odjęcia narzutu);.
const size_t AB_MAKS_BAJTOW_STATUSU = size_t{16} * 1024;
const size_t AB_MAKS_BAJTOW_LISTY = size_t{512} * 1024;
const size_t AB_MAKS_BAJTOW_RAPORTU = size_t{64} * 1024;  // raport doby i inne trasy
// STR.edf z AirBridge od v2.3.1 (src/str.h, ADR-0035):
std::string sciezkaSesjiStr();     // /api/sessions?order=desc&limit=1
std::string sciezkaPobraniaStr();  // /api/storage/download?path=/STR.edf
// Limity ciała odpowiedzi (limitOdpowiedziAB()) [stałe techniczne]: lista jednej sesji z kilkoma plikami to ok. 1 KB;
// STR.edf zmierzony 09.11.2026: 134 200 B przy 359 dobach (ok. 370 B na dobę), więc 1 MiB to ok. 2 800 dób.
const size_t AB_MAKS_BAJTOW_SESJI = size_t{16} * 1024;
const size_t AB_MAKS_BAJTOW_STR = size_t{1024} * 1024;
// Plik sesji z /api/storage/download (noc z plików, src/noc_pliki.h, ADR-0036) [stała techniczna]: PLD zmierzony
// 09.11.2026: 363 KB na 8 h 18 min (ok. 44 KB na godzinę), więc 2 MiB to ok. 47 h jednej sesji; większy plik =
// noc z raportu JSON (zapas).
const size_t AB_MAKS_BAJTOW_PLIKU_SESJI = size_t{2} * 1024 * 1024;

size_t limitOdpowiedziAB(const std::string& sciezka);
// liczba JSON spoza ±1e15 (także nieskończoność z `1e309`) w polu, które parser zapamiętuje,
// czyni dokument nieczytelnym; pola pominięte filtrem nie są sprawdzane.
// Największe prawdziwe pole AirBridge to czas w ms od 1970 (ok. 1,8e12).
const double AB_MAKS_LICZBY = 1e15;
// Ciało kompletne: zapis strumienia bez błędu (wynikZapisu ≥ 0) i, gdy serwer podał Content-Length (rozmiar ≥ 0),
// odebrano dokładnie tyle bajtów. Bez Content-Length kompletność sprawdza dopiero parser JSON.
bool cialoKompletne(int rozmiar, int wynikZapisu, size_t odebrane);
// R-AW-01, R-AW-12: brak odpowiedzi = kod ≤ 0 (połączenie, czas, ciało urwane) albo kod HTTP inny niż 200, 202, 409.
// 401 (złe hasło) to osobny powód: bez ponowień (R-AW-01 dotyczy braku odpowiedzi).
bool brakOdpowiedzi(int kod);
// R-AW-01: najwyżej 3 próby jednego żądania w odstępie 10 s [PRÓG], odpowiedź do 15 s; ponowienie tylko wtedy, gdy
// kolejna próba (odstęp + czas odpowiedzi) mieści się w budżecie wybudzenia (`zostaloMs`).
const int AB_PROB_ZADANIA = 3;
const uint32_t AB_ODSTEP_PROB_MS = 10000;
const uint32_t AB_LIMIT_ODPOWIEDZI_MS = 15000;
bool ponowicZadanie(int kod, int proba, long zostaloMs);
// R-DANE-11: odpytywanie joba co 5 s [PRÓG], łącznie najwyżej 120 s [PRÓG] na raport (R-AW-11).
const uint32_t AB_ODPYTYWANIE_MS = 5000;
const uint32_t AB_LIMIT_JOBA_MS = 120000;

// ---------- raport: budowa (job) ----------
enum class Odpowiedz : uint8_t { GOTOWA, W_BUDOWIE, BLAD };
struct OdpowiedzAB {
  Odpowiedz r = Odpowiedz::BLAD;
  std::string job;  // przy W_BUDOWIE, gdy moduł go podał
};
// 200 = gotowa (chyba że ciało ma job z `pending: true` albo job bez raportu: bez `days` i `present`; wtedy w budowie,
// ponowny GET), 202 i 409 = raport w budowie (R-AW-15), reszta = błąd
// (R-AW-12). 200 z ciałem, które nie jest poprawnym obiektem JSON (np. urwane bez Content-Length), = błąd: ponowienie,
// nie E-03.
OdpowiedzAB klasyfikujOdpowiedz(int kod, const std::string& tresc);
std::string sciezkaListy();               // /api/report?source=edf&action=days
std::string sciezkaNocy(long dzien);      // /api/report?source=edf&day={numer dnia}
std::string sciezkaJoba(const std::string& job);  // /api/report?source=edf&job={job z kodowaniem %XX}

// ---------- lista nocy ----------
struct DzienListy {
  long day = -1;   // numer dnia AirBridge (noc = okno 12:00–12:00, 4.1)
  int sesje = -1;  // session_count
  bool stale = false;  // moduł oznacza raport doby jako nieaktualny: doba pobierana jeszcze raz (raz na takie oznaczenie)
};
// false: brak tablicy `days` albo element bez liczby `day` (klucz krytyczny, R-AW-02 → E-03), albo tekst nie jest
// poprawnym JSON. Czyta strumieniowo, bez drzewa Json (jedna alokacja na wektor wyniku).
bool parsujListeNocy(const std::string& tresc, std::vector<DzienListy>& dni);
std::string dataPoranka(long day);  // day + 1 jako RRRR-MM-DD (4.1, R-DANE-12)

// ---------- raport nocy → rekord /noce.csv ----------
// KOMPLETNA: `partial` = false. CZESCIOWA: `partial` = true albo null (rekord „częściowa”, nieważny, R-DANE-15).
// ZLY_FORMAT: poprawny JSON, który nie jest obiektem albo nie ma klucza krytycznego lub ma jego zły typ (R-AW-02 →
// E-03). NIECZYTELNY: tekst nie jest poprawnym JSON (np. urwany) albo `day` w raporcie jest inny niż żądany: ta doba
// nic nie zapisuje, ponowienie.
enum class StanRaportu : uint8_t { KOMPLETNA, CZESCIOWA, ZLY_FORMAT, NIECZYTELNY };
struct NocAB {
  StanRaportu stan = StanRaportu::NIECZYTELNY;
  Noc noc;           // zrodlo 'A'; przy CZESCIOWA z `czesciowa` = true
  std::string tryb;  // MOP z raportu albo tryb zapasowy
  int sesje = -1;
  // Pełna noc: kompletna, `events_known` = true i co najmniej jeden z kanałów rekordu (`Press`, `EprPress`, `Leak`) ma
  // `source` = "edf" i liczbę w `median`. Doba bez tego (np. tylko podsumowanie STR) nie wypiera żadnego rekordu.
  bool pelna = false;
};
// dzien: numer dnia z żądania (-1: z pola `day` raportu, tylko w testach); trybZapasowy: `mode` z /api/status (gdy raport nie ma tagu MOP).
// Model aparatu (`pna`) nie decyduje o ważności nocy (R-DANE-29 fail-path, ADR-0030): liczy się tylko tryb.
// Noc ważna według 4.2 (R-DANE-13): `present`, `partial` = false, `events_known`, czas z maską ≥ 4 h liczony z tej samej
// zaokrąglonej wartości co M-01 i seria (m01 ≥ 240 min), i tylko w trybie obsługiwanym (R-DANE-29, jak historia.py).
// Raport bez `channels`, `counts` albo `settings` (np. doba tylko z podsumowania STR) daje rekord z tym, co jest:
// brakujące pola puste, nigdy 0 (R-AW-22).
// M-08 = `leak_high_seconds` / 60 (minuty; brak, null, zły typ albo ujemne = NAN, R-AW-22);
// M-13 = `median` kanału `RespRate` (liczba; pierwszy wpis z `source` = "edf" wygrywa, bez EDF pierwszy
// z liczbą; brak kanału albo `null` = NAN, R-DANE-07); M-17 i M-18 według R-DANE-27 z `sessions[]` (`start_ms`,
// `end_ms` w czasie cywilnym aparatu, m17m18Sesji()); bez `sessions[]` M-17 = `session_count` − 1, najmniej 0
// (fail-path R-DANE-27) i M-18 brak; brak `session_count` albo zły typ = -1. `pna`, `srn`: model i numer aparatu
// z `/api/status` tego samego wybudzenia (R-NOR-25,; raport dnia ich nie ma), zapisane przez czystePole;
// "" = brak odczytu (nie jest zmianą aparatu).
NocAB nocZRaportu(const std::string& tresc, long dzien, const std::string& trybZapasowy,
                  const std::string& pna = "", const std::string& srn = "");

// R-DANE-27 (m17_m18() w tools/kalibracja/noce.py): sesje doby w czasie cywilnym aparatu (sekundy kalendarza aparatu
// od 1970-01-01, bez strefy: tak AirBridge koduje `sessions[].start_ms` / 1000 i tak liczy nagłówek EDF). M-17 = sesje od
// 18:00 [PRÓG] dnia `dzien` − 1 (wyjątek wczesnego snu: najdłuższa sesja przed 18:00 ≥ 4 h [PRÓG] i bez takiej po 18:00
// → ta sesja i późniejsze); M-18 = start najdłuższej z nich w minutach od 12:00 dnia `dzien` (bez długości: najwcześniejszy
// start; sesja bez startu liczy się tylko do M-17); NAN = brak. Bez sesji: M-17 = 0.
struct SesjaCywilna {
  bool start = false;
  double startS = 0;
  bool koniec = false;
  double koniecS = 0;
};
// docs/spec/progi.csv: R-DANE-27.start_nocy (18:00 = 360 min od 12:00), R-DANE-27.wczesny_sen_min (4 h)
const double R_DANE_27_START_NOCY_MIN = 360;
const double R_DANE_27_WCZESNY_SEN_S = 4 * 3600.0;
void m17m18Sesji(const std::vector<SesjaCywilna>& sesje, long dzien, double startNocyMin, double wczesnySenS,
                 int& m17, double& m18Min);

// Części nocZRaportu() wspólne z nocą ze STR (src/str.h nocZeStr()), żeby reguły kanałów i ustawień były jedne:
// kanalyRaportu(): `channels[]` → pola kanałów (KANALY_AB z regułami źródła: TYLKO_EDF pomija `source` inne niż "edf",
// EDF_ALBO_STR ustawia Noc::epapStr), M-13 (`RespRate`) i M-10 (`FlowLim`); kanalEdf = któryś kanał KANALY_AB ma
// `source` = "edf" i liczbę w `median`.
// ustawieniaRaportu(): `settings` → Noc::ustawienia, mpa…epr; zwraca tryb z tagu MOP (maMop = tag obecny).
void kanalyRaportu(Noc& n, const Json& kanaly, bool& kanalEdf);
std::string ustawieniaRaportu(Noc& n, const Json& settings, bool& maMop);

// Mapowanie kanałów raportu na pola rekordu: jedno miejsce do poprawki po sprawdzeniu na urządzeniu.
enum class ZrodloKanalu : uint8_t {
  TYLKO_EDF,      // wartość tylko przy `source` = "edf" (R-NOR-08)
  EDF_ALBO_STR,   // `source` = "str" zapisane z flagą Noc::epapStr (dopisek `(urządzenie)`, R-KOK-15), EDF ma pierwszeństwo
  DOWOLNE,
};
struct KanalAB {
  const char* id;    // channels[].id; "" = kanału nie ma w raporcie (pole zostaje puste)
  float Noc::*med;   // channels[].median
  float Noc::*p95;   // channels[].p95
  ZrodloKanalu zrodlo;
};
extern const std::vector<KanalAB> KANALY_AB;
// Jednostka wycieku → mnożnik do L/min: `L/min` 1, `L/s` 60, bez względu na wielkość liter i spacje; brak jednostki
// albo inna = NAN (brak danych tej metryki, bez zgadywania).
double mnoznikWycieku(const std::string& jednostka);
// Ustawienia jako tekst jak w imporcie (historia.py): wartości MOP/EAI/EAX/ANS/AXS połączone „/”; liczby w zapisie
// najkrótszym (6.0 → 6, jak format .8g w tools/kalibracja/noce.py; `6.0 cmH2O` → 6); MOP jako numer opcji (`value`
// liczbą albo `option` liczbą) → nazwa trybu z listy OPCJE["MOP"] w tools/kalibracja/noce.py (8 = ASVAuto); bez
// żadnego z tagów: "".
std::string ustawieniaZRaportu(const Json& settings);
extern const char* const TRYBY_MOP[];  // OPCJE["MOP"]: CPAP, AutoSet, APAP, S, ST, T, VAuto, ASV, ASVAuto, iVAPS, …
extern const int N_TRYBOW_MOP;

// ---------- co pobrać w tym wybudzeniu ----------
// Pełna historia z AirBridge: bez filtra daty, wszystkie doby z listy `action=days`
// z datą poranka ≤ dziś, partiami (kilka raportów na wybudzenie, od najnowszej nocy wstecz). Postęp zostaje w pamięci
// panelu: noc zapisana w /noce.csv ze źródłem A (kompletna) nie jest pobierana drugi raz, doba ze zbioru „pominięte”
// (/ab_pominiete.txt) też nie; doba oznaczona na liście `stale` jest pobierana raz jeszcze.
//
// Licznik prób doby (/ab_proby.txt, trwały): nieudana próba starszej doby (błąd HTTP tej doby, ciało urwane albo
// nieczytelne, inny `day`, brak klucza krytycznego, raport w budowie po limicie, noc „częściowa”) zwiększa licznik;
// doba czeka wtedy AB_ODSTEP_DOBY_S do następnej próby (inne doby idą dalej), a po AB_PROB_DOBY próbach w różnych
// wybudzeniach przestaje być pobierana (nieczytelna: do „pominiętych”; częściowa: zostaje rekord „częściowa”).
// Brak połączenia z modułem nie jest próbą doby (wina sieci, nie doby). Ostatnia noc nie czeka: sprawdzenie co 30 min
// (R-DANE-18).
const int AB_PROB_DOBY = 3;                    // [stała prototypu]
const uint32_t AB_ODSTEP_DOBY_S = 3 * 3600;    // [stała prototypu]
struct ProbyDoby {
  int proby = 0;            // nieudane próby (każda w innym wybudzeniu)
  uint32_t ostatnia = 0;    // epoch ostatniej nieudanej próby
  bool staleZrobione = false;  // doba oznaczona `stale` już pobrana ponownie (do zdjęcia oznaczenia na liście)
};
using ProbyDob = std::map<long, ProbyDoby>;
ProbyDob parsujProby(const std::vector<std::string>& wiersze);  // wiersz: day;proby;ostatnia;stale(0/1)
std::vector<std::string> wierszeProb(const ProbyDob& p);
void zapiszNieudanaProbe(ProbyDob& p, long day, uint32_t teraz);
bool dobaCzeka(const ProbyDob& p, long day, uint32_t teraz);     // po nieudanej próbie, przed upływem odstępu
bool dobaWyczerpana(const ProbyDob& p, long day);                // AB_PROB_DOBY prób
// Oznaczenia `stale` z nowej listy: doba już bez `stale` traci znacznik „pobrana ponownie”. true = zmiana.
bool odswiezStale(ProbyDob& p, const std::vector<DzienListy>& lista);

// R-DANE-24: ponowne pobranie ostatniej nocy o 11:00, gdy zapisano ją przed 11:00 (rano = data takiej nocy,
// sprawdzona = data nocy już sprawdzonej o 11:00).
bool sprawdzenie11(const std::string& dzis, int minuta, const std::string& rano, const std::string& sprawdzona);
// Stan zaległych dób z ostatniej listy (bez ostatniej nocy): gotowe do pobrania teraz, czekające po nieudanej próbie
// i najwcześniejszy koniec czekania (epoch; 0 = żadna nie czeka).
struct Zalegle {
  int gotowe = 0, czekajace = 0;
  uint32_t czekajDo = 0;
};
// Lista nocy jest potrzebna tylko poza slotem nocnym i tylko, gdy ostatniej nocy nie ma jako kompletnego rekordu
// z AirBridge ani w pominiętych (np. doba tylko z STR przy rekordzie z karty), czeka ją sprawdzenie o 11:00, zostały
// zaległe doby gotowe do pobrania (`gotowe` > 0; -1 = nie wiadomo, np. po pierwszym starcie) albo minął koniec czekania
// doby po nieudanej próbie. Gdy zostały tylko doby czekające, lista nie jest pobierana co 30 min, tylko raz po upływie
// czekania (najwcześniej po AB_ODSTEP_DOBY_S od nieudanej próby).
bool potrzebnaLista(const std::vector<Noc>& hist, const std::string& dzis, int minuta, bool spr11, int gotowe,
                    uint32_t czekajDo, uint32_t teraz, const std::set<long>& pominiete);
// Numery dni do pobrania, najnowsze najpierw, najwyżej `maks`: doby z listy z datą poranka ≤ dziś bez kompletnego
// rekordu z AirBridge (pełna noc z AirBridge zastępuje noc z importu karty; rekord „częściowa” jest pobierany znów),
// spoza pominiętych, a przy `proby` ≠ nullptr: bez dób czekających i wyczerpanych; ostatnia noc także przy sprawdzeniu
// o 11:00; doba z `stale` (przy `proby` ≠ nullptr) raz jeszcze, także gdy ma rekord albo jest w pominiętych; starsza
// doba z nocą ze STR (zrodlo 'S') tylko, gdy lista podaje dla niej sesje (`session_count` ≥ 1, ADR-0035).
std::vector<long> planPobrania(const std::vector<DzienListy>& lista, const std::vector<Noc>& hist,
                               const std::set<long>& pominiete, const std::string& dzis, bool spr11, int maks,
                               const ProbyDob* proby = nullptr, uint32_t teraz = 0);
Zalegle policzZalegle(const std::vector<DzienListy>& lista, const std::vector<Noc>& hist,
                      const std::set<long>& pominiete, const std::string& dzis, const ProbyDob& proby, uint32_t teraz);
// Co zrobić z dobą po raporcie:
//   ZAPISZ: rekord do historii (kompletny albo „częściowa”: ostatnia noc z `partial` od 11:00, R-DANE-15; starsza doba
//           z `partial` od razu, R-DANE-25 w uproszczeniu);
//   CZEKAJ: ostatnia noc z `partial` przed 11:00 (noc w budowie), sprawdzenie za 30 min;
//   PONOW: nieczytelny raport (każda doba) albo brak klucza krytycznego (starsza doba): próba doby, następna później;
//   POMIN: starsza doba po AB_PROB_DOBY nieudanych próbach: do pominiętych, log;
//   E03: brak klucza krytycznego w raporcie ostatniej nocy (R-AW-02) albo nieczytelny raport ostatniej nocy po
//        AB_PROB_DOBY próbach: S5 z E-03, ponowienie za 30 min.
// proby: nieudane próby tej doby łącznie z tą.
enum class PoRaporcie : uint8_t { ZAPISZ, CZEKAJ, PONOW, POMIN, E03 };
PoRaporcie poRaporcie(StanRaportu stan, const std::string& data, const std::string& dzis, int minuta = 12 * 60,
                      int proby = 1);
// Zbiór pominiętych dób: jeden numer dnia na wiersz.
std::set<long> parsujPominiete(const std::vector<std::string>& wiersze);
std::vector<std::string> wierszePominietych(const std::set<long>& p);
// Wpis nocy z AirBridge do historii (R-AW-03):
//   - data bez rekordu: zapis zawsze (także doba tylko z STR i noc „częściowa”);
//   - noc „częściowa” zastępuje tylko rekord „częściowa” z AirBridge; kompletny rekord zostaje (`zostawiony` = true);
//   - kompletna noc zastępuje rekord „częściowa” zawsze, a zapisany kompletny rekord z AirBridge tylko przy ponownym
//     sprawdzeniu (`nadpisz`) i tylko, gdy wynik się różni;
//   - pełna noc z AirBridge zastępuje rekord z importu karty tej samej daty;
//   - noc niepełna nigdy nie zastępuje rekordu z karty, pełnego rekordu z AirBridge ani nocy ze STR aparatu (zrodlo
//     'S', ADR-0035 pkt 4; `zostawiony` = true); pełna noc zastępuje noc ze STR;
//   - przy zastąpieniu puste pola nowej nocy dostają wartości ze starego rekordu tej daty (uzupelnijNoc: np. wsparcie
// PS z karty, którego raport AirBridge nie ma), więc zastąpienie niczego nie gubi.
// Zwraca true, gdy historia się zmieniła (wtedy posortuj i zapisz plik).
bool wpiszNocAB(std::vector<Noc>& hist, const Noc& n, bool nadpisz, bool pelna = true, bool* zostawiony = nullptr);
// Rekord historii z pełnymi danymi nocy: kompletny (nie „częściowa”) z karty aparatu albo z AirBridge z EPAP lub
// ciśnieniem z EDF.
bool rekordPelny(const Noc& r);
// Import nocy z karty (R-DANE-31, ADR-0017): data bez rekordu → DODANA; pełny rekord tej daty
// zostaje, a jego puste pola dostają wartości z karty (np. PS) → UZUPELNIONA albo POMINIETA (nic do dopisania);
// niepełny rekord z AirBridge (tylko STR albo „częściowa”) ustępuje nocy z karty, która dostaje jego pola, których
// karta nie ma (M-03, M-09) → ZASTAPIONA. Noc ze STR aparatu ('S') nie jest pełna, więc ustępuje nocy z karty tak samo.
enum class WynikImportu : uint8_t { DODANA, ZASTAPIONA, UZUPELNIONA, POMINIETA };
WynikImportu wpiszImport(std::vector<Noc>& hist, const Noc& k);
