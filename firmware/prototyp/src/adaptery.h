// SPDX-License-Identifier: GPL-3.0-or-later
// Interfejsy adapterów (spec techniczna 6.4: T-KOMP-16…T-KOMP-19, T-KOMP-22, T-KOMP-23; ADR-0021 pkt 1–2).
// Adapter to jedyne miejsce z Arduino, ESP-IDF i bibliotekami sprzętu; tu tylko klasy abstrakcyjne bez Arduino,
// żeby dyrygent (src/dyrygent.h) i rdzeń kompilowały się na komputerze. Atrapy z zachowaniem sterowanym z testu:
// test/atrapy/adaptery_atrapy.h (T-WYM-02). Adapter niczego nie decyduje: zwraca wynik, a rdzeń wybiera reakcję (7.6).
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "airbridge.h"
#include "ekrany.h"
#include "nawigacja.h"
#include "start.h"
#include "wybudzenie.h"

// Zegar systemowy i procesora (część T-KOMP-02 po stronie sprzętu). Czas doby rdzeń dostaje jako dane (T-WYM-04).
class Zegar {
 public:
  virtual ~Zegar() = default;
  // Czas unix (UTC) zegara systemowego; poniżej CZAS_ZNANY_OD = zegar nieustawiony.
  virtual uint32_t teraz() = 0;
  // Milisekundy zegara procesora od startu programu (limity kroków, T-WYM-22).
  virtual uint32_t ms() = 0;
  // Czekanie ms milisekund (na panelu delay(); atrapa przesuwa czas).
  virtual void czekaj(uint32_t ms) = 0;
  // Czas lokalny Europe/Warsaw z czasem letnim dla czasu unix t. Zwraca false, gdy nie da się przeliczyć.
  virtual bool lokalny(uint32_t t, std::tm& wynik) = 0;
  // Odwrotność lokalny(): czas unix (UTC) z godziny ściennej w strefie panelu (czas letni według reguły strefy,
  // tm_isdst ignorowane). Zwraca 0, gdy nie da się przeliczyć. Czas z AirBridge (R-AW-18) jest lokalny.
  virtual uint32_t zLokalnego(const std::tm& lokalny) = 0;
  // Ustawia zegar systemowy na czas unix t (R-AW-18: czas z AirBridge). Zwraca false, gdy się nie udało.
  virtual bool ustaw(uint32_t t) = 0;
};

// Pamięć ustawień NVS (część T-KOMP-18). Typy kluczy jak w prototypie (Preferences), bo panel ma już dane w NVS.
class Nvs {
 public:
  virtual ~Nvs() = default;
  virtual std::string tekst(const char* przestrzen, const char* klucz, const std::string& domysl) = 0;
  // false = zapis się nie udał (sekrety sprawdzają też odczytem kontrolnym, bo flash bywa zawodny po cichu).
  virtual bool ustawTekst(const char* przestrzen, const char* klucz, const std::string& v) = 0;
  virtual uint32_t liczbaU(const char* przestrzen, const char* klucz, uint32_t domysl) = 0;
  virtual void ustawLiczbeU(const char* przestrzen, const char* klucz, uint32_t v) = 0;
  virtual int32_t liczba(const char* przestrzen, const char* klucz, int32_t domysl) = 0;
  virtual void ustawLiczbe(const char* przestrzen, const char* klucz, int32_t v) = 0;
  virtual bool flaga(const char* przestrzen, const char* klucz, bool domysl) = 0;
  virtual void ustawFlage(const char* przestrzen, const char* klucz, bool v) = 0;
  // Bajty pod kluczem: zwraca liczbę skopiowanych do `cel` (0 = brak klucza albo inna długość niż `rozmiar`).
  virtual size_t bajty(const char* przestrzen, const char* klucz, void* cel, size_t rozmiar) = 0;
  virtual void ustawBajty(const char* przestrzen, const char* klucz, const void* zrodlo, size_t rozmiar) = 0;
  virtual void usun(const char* przestrzen, const char* klucz) = 0;
};

// Partycja plików z tablicy partycji, z której panel faktycznie wystartował (T-WYM-84, R7).
struct OpisPartycji {
  uint32_t przesuniecie = 0;  // adres partycji `spiffs` we flash
  uint32_t rozmiar = 0;       // rozmiar partycji `spiffs` w bajtach
  uint32_t uklad = 0;         // identyfikator tablicy partycji panelu (porównywany z src/magazyn.h ukladWkompilowany())
};

// System plików na partycji flash (część T-KOMP-18). Zapis atomowy przez plik tymczasowy i zmianę nazwy (R-NF-12)
// jest w adapterze, bo to protokół nośnika, nie decyzja. Protokół zapisu atomowego z wierszem `#koniec`
// (T-WYM-51…T-WYM-53) realizuje magazyn (src/magazyn.h) na prostych operacjach `czytaj()`, `zapisz()`, `usun()`,
// `zmienNazwe`; `zapiszAtomowo` zostaje dla kodu, który jeszcze nie przeszedł na magazyn.
class Pliki {
 public:
  virtual ~Pliki() = default;
  // Montowanie bez formatowania (T-WYM-34). Zwraca false, gdy partycja się nie montuje.
  virtual bool zamontuj() = 0;
  virtual bool zamontowane() = 0;
  virtual bool istnieje(const char* nazwa) = 0;
  // Wiersze pliku bez pustych i bez końców linii; przy braku pliku i obecnym `{nazwa}.tmp` najpierw przywraca
  // plik tymczasowy. Zwraca false, gdy pliku nie ma.
  virtual bool czytajWiersze(const char* nazwa, std::vector<std::string>& wiersze) = 0;
  // Cała treść atomowo: plik tymczasowy, kontrola liczby bajtów, zmiana nazwy. Przy błędzie
  // stary plik zostaje nietknięty i wynik false.
  virtual bool zapiszAtomowo(const char* nazwa, const std::string& tresc) = 0;
  // Dopisanie wiersza; urwany ostatni wiersz nie skleja się z nowym.
  virtual bool dopiszWiersz(const char* nazwa, const std::string& wiersz) = 0;
  // Pierwszy wiersz pliku. Zwraca false, gdy pliku nie ma.
  virtual bool pierwszyWiersz(const char* nazwa, std::string& wiersz) = 0;
  virtual bool usun(const char* nazwa) = 0;
  virtual bool zmienNazwe(const char* z, const char* na) = 0;
  // Odmontowanie (przed przyjęciem obrazu pamięci plików w oknie serwisowym).
  virtual void odmontuj() = 0;

  // ---------- proste operacje dla magazynu ----------
  // Cała treść pliku bajt w bajt, bez przywracania `{nazwa}.tmp` i bez dzielenia na wiersze. Zwraca false, gdy pliku
  // nie ma albo partycja nie jest zamontowana (tresc pusta).
  virtual bool czytaj(const char* nazwa, std::string& tresc) = 0;
  // Zapis całej treści pod `nazwa` (nadpisanie; brakujące katalogi adapter tworzy sam), bez pliku tymczasowego.
  // Zwraca liczbę bajtów faktycznie zapisanych: mniej niż tresc.size() = brak miejsca, błąd nośnika albo zanik
  // zasilania (plik zostaje z tym, co się zapisało). Bez zamontowanej partycji 0.
  virtual size_t zapisz(const char* nazwa, const std::string& tresc) = 0;
  // Pełne ścieżki plików leżących bezpośrednio w katalogu `katalog` (np. "/dane/noce" → "/dane/noce/2026.csv"), bez
  // podkatalogów, w porządku rosnącym. Zwraca false, gdy katalogu nie ma (nazwy puste).
  virtual bool listuj(const char* katalog, std::vector<std::string>& nazwy) = 0;
  // Partycja plików z tablicy partycji panelu (esp_partition_find_first(…, "spiffs") i skrót tablicy spod 0x8000).
  virtual OpisPartycji partycja() = 0;
  // Pierwsze `ile` bajtów partycji plików wprost z flash, bez montowania (T-WYM-84: dwa pierwsze bloki, 2 × 4096 B).
  // Zwraca false przy błędzie odczytu (bajty puste).
  virtual bool czytajPartycje(size_t ile, std::string& bajty) = 0;
  // Formatowanie partycji plików i zamontowanie jej (tylko partycja nowa, T-WYM-84; decyzję podejmuje rdzeń).
  // Zwraca true, gdy po formatowaniu partycja jest zamontowana.
  virtual bool formatuj() = 0;
  // (U-05, T-WYM-39): wolne miejsce w partycji plików w bajtach (LittleFS totalBytes − usedBytes);
  // -1 bez zamontowanej partycji.
  virtual long long wolneMiejsce() = 0;
};

struct ZadanieHttp {
  // http://adres/ścieżka, bez TLS: AirBridge i lokalne źródło otoczenia w sieci domowej, a prognoza Open-Meteo
  // (src/slot_dzienny.h) przez internet; ryzyko opisuje SECURITY.md.
  std::string url;
  std::vector<std::pair<std::string, std::string>> naglowki;  // np. Authorization, X-Token
  uint32_t limitMs = 15000;                                 // połączenie i odpowiedź
  size_t maksBajtow = 0;                                    // 0 = bez limitu; powyżej → wynik zaDuzo
  uint32_t koniecMs = 0;                                    // 0 = bez; zegar procesora końca odczytu ciała
  std::function<bool()> przerwij;                           // pusty = bez; true = przerwać odczyt ciała (przycisk)
  bool cialoTylko200 = false;                               // ciało tylko przy 200 (źródło otoczenia)
};
struct OdpowiedzHttp {
  int kod = 0;                 // kod HTTP albo ujemny błąd połączenia (jak HTTPClient)
  std::string cialo;
  std::string wwwAuthenticate; // nagłówek przy 401 (Digest, T-WYM-25)
  int contentLength = -1;      // -1 = brak nagłówka
  int wynikZapisu = 0;         // wynik writeToStream(): bajty albo ujemny błąd (cialoKompletne(), T-WYM-26)
  bool zaDuzo = false;         // ciało dłuższe niż maksBajtow
  bool zaDlugo = false;        // minął koniecMs w trakcie odczytu ciała
  bool przerwane = false;      // przerwij() zwróciło true
  uint32_t ms = 0;             // czas żądania
};

// Wi-Fi, HTTP, NTP, nasłuch wgrywania (T-KOMP-16). Tylko GET (T-WYM-05).
class Siec {
 public:
  virtual ~Siec() = default;
  // Połączenie z siecią domową (dane z konfiguracji adaptera). Czeka najwyżej limitMs, woła `przerwij` co obieg;
  // przerwanie → false i przerwane = true. Zwraca true po połączeniu.
  virtual bool polacz(uint32_t limitMs, const std::function<bool()>& przerwij, bool& przerwane) = 0;
  virtual bool polaczona() = 0;
  virtual void wylacz() = 0;  // rozłączenie i radio wyłączone
  // Opis braku połączenia do ekranu testu (powód rozłączenia, skan sieci).
  virtual std::string opisBledu() = 0;
  // Adres i siła sygnału do ekranu testu („192.168.1.20 · RSSI -60 dBm”).
  virtual std::string opisPolaczenia() = 0;
  // Czas z NTP w strefie panelu (ADR-0013); czeka najwyżej limitMs na odpowiedź serwera w tym wywołaniu. Zwraca true,
  // gdy odpowiedź przyszła i zegar jest z niej ustawiony; false, gdy w limicie jej nie było (zegar bez korekty).
  virtual bool czasZSieci(uint32_t limitMs) = 0;
  // GET z nagłówkami z `z`. Ciało czytane przy kodzie > 0 innym niż 401 (przy cialoTylko200 tylko przy 200); przy
  // Content-Length powyżej maksBajtow bez czytania, zaDuzo = true. Przy 401 wwwAuthenticate z nagłówka.
  virtual OdpowiedzHttp get(const ZadanieHttp& z) = 0;
  // Okno serwisowe: nasłuch wgrywania z hasłem i mDNS (T-WYM-40); bez hasła w konfiguracji false (T-WYM-45).
  virtual bool otworzOknoSerwisowe() = 0;
  virtual bool oknoSerwisoweOtwarte() = 0;
  // Obsługa wgrywania w pętli czuwania (ArduinoOTA.handle()); na panelu po udanym wgraniu restart.
  virtual void obsluzOknoSerwisowe() = 0;
  // Bajty losowe z generatora sprzętowego (cnonce Digest, ziarno).
  virtual uint32_t losowa() = 0;
  // Adres IP panelu w sieci domowej („192.168.1.20”) do linii okna serwisowego na ekranie testu; "" bez połączenia.
  virtual std::string adresIp() = 0;
  // (U-05, R-KONF-41): nazwa sieci (SSID) i siła sygnału w dBm bieżącego połączenia; "" i 0 bez połączenia.
  virtual std::string nazwaSieci() = 0;
  virtual int rssi() = 0;
  // Funkcja MD5 platformy do logowania Digest (na panelu mbedtls).
  virtual Md5Hex md5() = 0;
  // (formularz, src/sekrety.h): nazwa i hasło sieci domowej dla następnych polacz (sprawdzenie nowej sieci przed
  // zapisem i powrót do poprzedniej). Nie łączy i nie rozłącza; hasła adapter nie wypisuje.
  virtual void ustawSiecDomowa(const std::string& ssid, const std::string& haslo) = 0;
  // (sieć panelu, U-02, R-KONF-07, R-KONF-08): tylko na żądanie z przycisku, nigdy sama po starcie.
  // wlaczRadio(): koniec połączenia z siecią domową, radio włączone bez łączenia (generator sprzętowy losowa() ma
  // wtedy źródło szumu radia); konfiguracja Wi-Fi bez zapisu we flash. uruchomSiecPanelu(): punkt dostępu WPA2
  // `ssid` z hasłem `haslo` (8–63 znaki), najwyżej 2 telefony; true, gdy działa i ma adres. adresSieciPanelu():
  // adres panelu w tej sieci („192.168.4.1”), "" gdy nie działa. idPanelu(): 4 znaki szesnastkowe z adresu MAC
  // (nazwa `cpap-eink-XXXX`). Sieć panelu kończy wylacz(). Domyślnie (atrapy bez sieci panelu): brak sieci panelu.
  virtual bool wlaczRadio() { return false; }
  virtual bool uruchomSiecPanelu(const std::string& ssid, const std::string& haslo) {
    (void)ssid;
    (void)haslo;
    return false;
  }
  virtual std::string adresSieciPanelu() { return ""; }
  virtual std::string idPanelu() { return "0000"; }
};

// Ekran e-ink (T-KOMP-17): płótno 800×480 1 bit i jego przesłanie do sterownika.
class Wyswietlacz {
 public:
  virtual ~Wyswietlacz() = default;
  virtual void wlacz() = 0;            // inicjalizacja sterownika (epd.init)
  virtual Plotno& plotno() = 0;        // bufor, w który rysuje rdzeń (src/ekrany.h)
  virtual void odswiez() = 0;          // pełne odświeżenie z bufora i uśpienie sterownika
  // Bufor płótna po rysowaniu: BAJTOW_KLATKI bajtów (800×480, 1 bit na piksel, wiersz po wierszu), do skrótu klatki.
  // Nie zmienia ekranu.
  virtual const uint8_t* bufor() = 0;
};
const size_t BAJTOW_KLATKI = (size_t)800 / 8 * 480;  // 48 000 B

// Przyciski (część T-KOMP-19). Stan w tej chwili, bez logiki zaciętych (ta jest w rdzeniu).
class Przyciski {
 public:
  virtual ~Przyciski() = default;
  // Bity przycisków wciśniętych teraz (1 << (int)Przycisk).
  virtual uint8_t wcisniete() = 0;
  // Dioda przy przyciskach: świeci, gdy panel
  // pracuje (pobiera, rysuje), gaśnie, gdy czeka na naciśnięcie.
  virtual void dioda(bool swieci) = 0;
  // Naciśnięcie zakończone (wciśnięcie i puszczenie), gdy rdzeń nie czytał stanu, np. w czasie rysowania ekranu
  // (kolejka naciśnięć, spec 5.6.1). Najstarsze pierwsze: numer przycisku jak `(int)Przycisk` i czas trzymania w ms.
  // false, gdy kolejka pusta. Domyślnie bez kolejki.
  virtual bool zKolejki(int& przycisk, uint32_t& trwaloMs) {
    (void)przycisk;
    (void)trwaloMs;
    return false;
  }
  // Kolejka od nowa: naciśnięcie obsłużone już z odczytu stanu nie wraca z kolejki; przycisk trzymany w tej chwili
  // nie trafi do niej po puszczeniu.
  virtual void wyczyscKolejke() {}
  // true: adapter piszczy sam w chwili naciśnięcia (raz), także w czasie rysowania; rdzeń dokłada wtedy tylko drugi
  // pisk przytrzymania i pisk przycisku, który obudził panel (R-PRZ-20). Domyślnie false: piszczy rdzeń.
  virtual bool piskPrzyNacisnieciu() const { return false; }
};

struct OdczytCzujnikow {
  float temp = NAN, wilg = NAN;  // SHT40 w obudowie; NAN = brak odczytu
  float bateriaV = NAN;          // napięcie baterii bez obciążenia (przed ekranem i Wi-Fi)
};
// Czujnik w obudowie i bateria (część T-KOMP-19).
class Czujniki {
 public:
  virtual ~Czujniki() = default;
  virtual OdczytCzujnikow czytaj() = 0;
};

// Buzzer (T-KOMP-22).
class Dzwiek {
 public:
  virtual ~Dzwiek() = default;
  virtual void pisk(int ile) = 0;
};

// Przyczyna startu, wybudzenia, pamięć RTC bez zerowania i wersja aplikacji (część T-KOMP-18 i T-KOMP-19).
class Uruchomienie {
 public:
  virtual ~Uruchomienie() = default;
  virtual PrzyczynaStartu przyczyna() = 0;   // esp_reset_reason()
  virtual PowodWybudzenia powod() = 0;       // esp_sleep_get_wakeup_cause()
  virtual uint8_t przyciskiBudzace() = 0;    // bity przycisków z esp_sleep_get_ext1_wakeup_status()
  virtual LicznikStartow& licznik() = 0;     // blok RTC_NOINIT_ATTR (T-WYM-82)
  virtual StanRtc& rtc() = 0;                // blok RTC_DATA_ATTR: przeżywa głęboki sen, ginie przy restarcie
  virtual const char* wersja() = 0;          // identyfikator wersji: WERSJA+skrót commitu (src/main.cpp)
  virtual std::string slot() = 0;            // etykieta slotu aplikacji (esp_ota_get_running_partition()->label)
  virtual StanWersji stanWersji() = 0;       // esp_ota_get_state_partition() bieżącego slotu
  virtual void oznaczWersjeDobra() = 0;      // esp_ota_mark_app_valid_cancel_rollback()
  // Powrót do poprzedniej wersji (esp_ota_mark_app_invalid_rollback_and_reboot()); na panelu nie wraca.
  virtual void wrocDoPoprzedniej() = 0;
};

// Strażnik zadania głównego (T-WYM-81).
class Straznik {
 public:
  virtual ~Straznik() = default;
  // Zapis zadania głównego do strażnika z limitem bez odnowienia; panika = przekroczenie kończy się restartem.
  virtual void zapisz(uint32_t limitS, bool panika) = 0;
  virtual void odnow() = 0;
};

// Głęboki sen (część T-KOMP-19: budzenie ext1 i zegarem). Na panelu nie wraca.
class Sen {
 public:
  virtual ~Sen() = default;
  virtual void spij(const PlanSnu& plan) = 0;
};

// Port serwisowy UART (T-KOMP-23): log na żywo i komendy `Z`, `O`.
class PortSerwisowy {
 public:
  virtual ~PortSerwisowy() = default;
  virtual void wiersz(const std::string& tekst) = 0;
  virtual int znak() = 0;  // odebrany znak albo -1
};

// Pomiar pamięci do logu wybudzenia.
class Diagnostyka {
 public:
  virtual ~Diagnostyka() = default;
  // heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) od startu programu: pamięć wewnętrzna,
  // bez PSRAM (z PSRAM 8 MB próg 64 KB nie miałby sensu)
  virtual uint32_t minWolnaSterta() = 0;
  virtual uint32_t zapasStosu() = 0;      // uxTaskGetStackHighWaterMark(NULL) zadania loop, w bajtach
};

// Wydania firmware z GitHub (ADR-0032, ADR-0038; T-KOMP-16 i T-KOMP-18; rdzeń: src/aktualizacja.h). HTTPS
// z weryfikacją certyfikatu serwera certyfikatami głównymi wkompilowanymi w firmware; bez poprawnego czasu (U-04)
// rdzeń nie woła tego adaptera. Przekierowanie (plik zasobu leży na innym serwerze GitHuba): adapter idzie za nim
// najwyżej PRZEKIEROWAN_MAKS razy, tylko na https://, a nagłówki dla każdego adresu liczy od nowa funkcją rdzenia
// aktualizacja::naglowki(), więc token nie trafia na host inny niż api.github.com (R-NF-36). Adapter niczego nie
// decyduje: zwraca kod, ciało albo liczbę bajtów i skrót SHA-256 liczony w locie; porównanie robi rdzeń.
struct ZadanieWydania {
  std::string url;        // https://api.github.com/…
  std::string token;      // token wydań z NVS (sekrety); "" = bez nagłówka Authorization
  bool binarne = false;   // Accept: application/octet-stream (plik zasobu przez API)
  uint32_t limitMs = 20000;
  size_t maksBajtow = 0;  // ciało tekstowe: powyżej → zaDuzo, bez ciała
};
struct OdpowiedzWydania {
  int kod = 0;                 // kod HTTP po przekierowaniach albo ujemny błąd połączenia
  std::string cialo;           // pobierzTekst(): ciało przy 200
  bool certyfikatOk = true;    // false = serwer bez certyfikatu z zestawu (żądanie nie wyszło)
  bool zaDuzo = false;
  std::vector<std::string> hosty;  // hosty kolejnych żądań (pierwszy i po przekierowaniach), do logu i testów
};
struct WynikObrazu {
  OdpowiedzWydania http;
  uint64_t bajtow = 0;         // bajty zapisane do drugiego slotu aplikacji
  std::string sha256;          // skrót SHA-256 zapisanych bajtów (64 znaki), liczony w locie
  bool zapisOk = false;        // esp_ota_begin/esp_ota_write bez błędu i bajtow == rozmiar
};
const int PRZEKIEROWAN_MAKS = 3;
class Wydania {
 public:
  virtual ~Wydania() = default;
  // GET tekstu (odpowiedź API, `wydanie.json`); ciało tylko przy 200 i w limicie maksBajtow.
  virtual OdpowiedzWydania pobierzTekst(const ZadanieWydania& z) = 0;
  // GET obrazu strumieniem do drugiego slotu aplikacji (esp_ota_begin z rozmiarem `rozmiar`), SHA-256 w locie.
  // Slot nie jest ustawiany jako startowy; przy błędzie zapis przerwany (esp_ota_abort). `postep` wołany co kawałek
  // (strażnik zadania, T-WYM-81).
  virtual WynikObrazu pobierzObraz(const ZadanieWydania& z, uint64_t rozmiar, const std::function<void()>& postep) = 0;
  // Zamknięcie zapisu i ustawienie drugiego slotu jako startowego (esp_ota_end, esp_ota_set_boot_partition): false
  // = obraz odrzucony przez ESP-IDF (zły nagłówek, inny układ), slot startowy bez zmian.
  virtual bool ustawStartowy() = 0;
  // Porzucenie zapisanego, niezatwierdzonego obrazu (esp_ota_abort); bezpieczne bez zapisu w toku.
  virtual void porzuc() = 0;
  // Restart po ustawieniu slotu (esp_restart); na panelu nie wraca, atrapa wraca.
  virtual void restart() = 0;
  // Rozmiar drugiego slotu aplikacji (esp_ota_get_next_update_partition()->size); 0 = brak slotu.
  virtual uint64_t rozmiarSlotu() = 0;
};

// Komplet adapterów jednego panelu; dyrygent dostaje go przy tworzeniu.
struct Adaptery {
  Zegar& zegar;
  Nvs& nvs;
  Pliki& pliki;
  Siec& siec;
  Wyswietlacz& wyswietlacz;
  Przyciski& przyciski;
  Czujniki& czujniki;
  Dzwiek& dzwiek;
  Uruchomienie& uruchomienie;
  Straznik& straznik;
  Sen& sen;
  PortSerwisowy& port;
  Diagnostyka& diagnostyka;
  // (ADR-0032) wydania z GitHub; nullptr = panel bez aktualizacji z sieci (starsze testy, symulator)
  Wydania* wydania = nullptr;
};
