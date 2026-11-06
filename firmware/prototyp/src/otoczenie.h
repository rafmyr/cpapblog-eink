// SPDX-License-Identifier: GPL-3.0-or-later
// Lokalne źródło danych otoczenia (ADR-0019, spec Załącznik C), bez zależności od Arduino.
// Opcjonalne urządzenie w sieci domowej (np. Home Assistant, Node-RED albo własny skrypt na serwerze domowym) podaje
// pogodę, powietrze za oknem i CO2 sypialni jako JSON w wersji 1:
//   GET {OTO_URL}   (np. http://192.168.1.60:8080/otoczenie), opcjonalny nagłówek `X-Token: {OTO_TOKEN}`
//   200 → {"wersja":1, "ts":…, "pogoda":{…}|null, "powietrze":{…}|null, "sypialnia":{…}|null, "ostrzezenia":[…]}
// Panel tylko czyta; nic z danych terapii nie wychodzi z panelu (ADR-0001). Żądanie HTTP robi adapter, a tu jest
// parser kontraktu, ocena świeżości i teksty (stopka, ekran testu). Testy: test/testy.cpp na syntetycznych JSON-ach.
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

const int OTO_WERSJA = 1;            // jedyna obsługiwana wersja kontraktu; inna = brak źródła (i log)
const size_t OTO_MAX_BAJTOW = 4096;  // kontrakt: odpowiedź < 2 KB; większa jest odrzucana (bez parsowania)
// Świeżość kopii na panelu (od `ts` bloku do chwili rysowania), te same granice co po stronie źródła [PRÓG]:
const uint32_t OTO_SWIEZOSC_POGODY_S = 9 * 3600;  // pogoda i powietrze za oknem
const uint32_t OTO_SWIEZOSC_SYPIALNI_S = 30 * 60;  // CO2 sypialni
const uint32_t OTO_SWIEZOSC_OSTRZEZEN_S = 60 * 60;  // ostrzeżenia z ostatniej udanej odpowiedzi
const uint32_t OTO_ZAPAS_ZEGARA_S = 10 * 60;       // `ts` z przyszłości o więcej = nieświeże (zegar źródła albo panelu)
// R-AW-26 (ponowienia jak R-AW-01, czasy dopasowane do kontraktu: odpowiedź < 200 ms) [stałe prototypu]:
const int OTO_PROB = 3;
const uint32_t OTO_ODSTEP_PROB_MS = 2000;
const uint32_t OTO_LIMIT_ODPOWIEDZI_MS = 3000;
const uint32_t OTO_BUDZET_MS = 15000;  // razem na jedno wybudzenie, z odczytem ciała odpowiedzi
const uint32_t OTO_BUDZET_1_MS = 6000;  // tryb 1 próby: twardy limit od startu (połączenie 3 s + odpowiedź 3 s)
// Energia: po wybudzeniu bez odpowiedzi źródła kolejne mają tylko 1 próbę (aż do udanej
// odpowiedzi), w slocie nocnym zawsze 1 próba.
int probOtoczenia(bool poprzedniaNieudana, bool slotNocny);
// Zakresy wiarygodności; wartość spoza zakresu = brak wartości (nigdy 0) [stałe prototypu]
const int OTO_CO2_MIN = 100, OTO_CO2_MAX = 9999, OTO_PM25_MAX = 999;

enum class StanOto : int8_t {
  WYLACZONE = 0,     // brak OTO_URL w secrets.h: funkcja wyłączona, panel jak bez źródła
  NIE_BYLO = 1,      // skonfigurowane, jeszcze bez próby (np. brak Wi-Fi od startu)
  OK = 2,
  BRAK_ODPOWIEDZI = 3,  // połączenie, czas, ciało urwane
  ODMOWA = 4,        // 401 albo 403: zły albo brakujący OTO_TOKEN
  HTTP = 5,          // inny kod HTTP
  ZLA_WERSJA = 6,        // `wersja` ≠ 1 albo brak pola
  NIECZYTELNE = 7,   // nie obiekt JSON
  ZA_DUZE = 8,       // ciało ponad OTO_MAX_BAJTOW
  PRZEKIEROWANIE = 9,  // R-NF-27: odpowiedź 3xx, bez podążania i bez ponowień (R-AW-25)
  POZA_SIECIA = 10,    // R-NF-27: adres spoza sieci lokalnej (adresWSieciLokalnej), bez żądania i tokenu
};
const int OTO_KOD_CZAS = -400;  // kod panelu: odczyt ciała przekroczył OTO_BUDZET_MS (brak odpowiedzi)

// Zwykła struktura bez wskaźników: leży w pamięci RTC między wybudzeniami.
struct Otoczenie {
  uint32_t magia;
  uint32_t adres;        // suma kontrolna adresu źródła (hashAdresu): inny adres w secrets.h = dane kasowane
  int8_t stan;           // StanOto ostatniej próby
  int16_t kod;           // kod HTTP ostatniej próby (ujemny: błąd połączenia)
  int16_t wersja;        // `wersja` z ostatniej odpowiedzi 200 (-1: brak pola albo nie liczba)
  uint32_t proba;        // epoch ostatniej próby
  uint32_t pobrano;      // epoch ostatniej udanej odpowiedzi (stan OK)
  // pogoda (M-26): `temp` °C, `opis` do 20 znaków, `miasto`
  bool pogoda;
  float temp;
  char opis[64];
  char miasto[80];
  uint32_t tsPogody;
  // powietrze za oknem (M-27): PM2.5 µg/m³
  bool powietrze;
  int16_t pm25;          // -1 = brak
  uint32_t tsPowietrza;
  // sypialnia (M-23): CO2 ppm; temperatura i wilgotność sypialni tylko zapisane (stopka i S2c biorą je z czujnika
  // panelu, M-22)
  bool sypialnia;
  int16_t co2;           // -1 = brak
  float tempSyp, wilgSyp;
  uint32_t tsSypialni;
  // ostrzeżenie `co2_milczy` (E-08): od kiedy brak udanego pomiaru CO2 (0 = źródło nie zna chwili)
  bool co2Milczy;
  uint32_t co2MilczyOd;
  uint8_t zleTs;         // bloki odrzucone w ostatniej odpowiedzi przez nieczytelne `ts` (1 pogoda, 2 powietrze, 4 sypialnia)
  uint32_t tsCo2WBuforze;  // `ts` pomiaru CO2 ostatnio dopisanego do bufora nocy (ten sam pomiar nie wchodzi dwa razy)
};
const uint32_t OTO_MAGIA = 0x07C20002;
// adres = hashAdresu(OTO_URL) albo 0 bez źródła
void wyczyscOtoczenie(Otoczenie& o, bool skonfigurowane, uint32_t adres = 0);
uint32_t hashAdresu(const char* url);  // FNV-1a
// Pomiar CO2 z ostatniej odpowiedzi do bufora nocy: tylko gdy świeży i nowszy niż ostatnio dopisany (-1: nie).
int co2DoBufora(Otoczenie& o, uint32_t teraz);

// `2026-10-30T22:15:00+01:00` (także `Z`, `+0100`, bez sekund, z ułamkiem sekund) → epoch UTC; false bez strefy albo przy
// złej dacie.
bool epochZIso(const std::string& iso, uint32_t& epoch);

// Odpowiedź 200 → nowe dane w `o` (stan OK) albo powód odrzucenia: przy złej wersji dane znikają (jak brak źródła),
// przy nieczytelnym albo za dużym ciele stare dane zostają, z ich czasem. `teraz` = czas
// panelu: blok bez `ts` (albo z `ts` = null) dostaje czas odebrania; blok z `ts` nieczytelnym albo bez strefy = brak
// bloku (bit w `zleTs`, log). Bloki null, brakujące albo złego typu = brak bloku; pole liczbowe null albo złego typu =
// brak wartości (nigdy 0). Nieznane klucze i nieznane kody ostrzeżeń są pomijane; BOM UTF-8 na początku tolerowany.
// Opis pogody i miasto: bez znaków sterujących i znaków spoza kroju panelu, urwana sekwencja UTF-8 obcięta.
StanOto parsujOtoczenie(const std::string& tresc, uint32_t teraz, Otoczenie& o);
// Wynik nieudanej próby HTTP (kod ≤ 0 albo ≠ 200): stan w `o`, dane bez zmian; 401, 403 → ODMOWA, 3xx →
// PRZEKIEROWANIE (R-NF-27), ≤ 0 i 5xx → BRAK_ODPOWIEDZI, inne → HTTP.
void nieudanaOtoczenia(Otoczenie& o, int kod, uint32_t teraz);
// R-AW-26: ponowienie tylko przy braku odpowiedzi (kod ≤ 0, 5xx), nie przy 401/403 ani przy odpowiedzi 200
// (zła wersja albo nieczytelne ciało nie zmienią się po 2 s), i tylko w budżecie.
bool ponowicOtoczenie(int kod, int proba, long zostaloMs, int maxProb = OTO_PROB);
bool brakOdpowiedziOtoczenia(int kod);  // kod ≤ 0 albo 5xx

// Świeżość znacznika czasu względem `teraz` (bez zegara: nieświeże).
bool swiezy(uint32_t ts, uint32_t teraz, uint32_t maxS);
// Wartości do pokazania teraz (po ocenie świeżości); brak = "" albo -1.
std::string pogodaTeraz(const Otoczenie& o, uint32_t teraz);  // `Katowice 8° · deszcz` (R-EKR-10, M-26)
int co2Teraz(const Otoczenie& o, uint32_t teraz);              // CO2 sypialni albo -1
int pm25Teraz(const Otoczenie& o, uint32_t teraz);             // PM2.5 za oknem albo -1
bool co2MilczyTeraz(const Otoczenie& o, uint32_t teraz);       // ostrzeżenie z udanej odpowiedzi nie starszej niż 60 min

// Linia ekranu testu: `Otoczenie: OK 21:30 · 8° · PM2.5 12 · CO2 967`, `Otoczenie: HTTP 401 (zły OTO_TOKEN) · zostają
// dane z 05:16`… `kiedyPobrano` = chwila ostatniej udanej odpowiedzi (`05:16` dziś, `30.10 05:16` innego dnia; "" bez).
std::string liniaTestuOtoczenia(const Otoczenie& o, uint32_t teraz, const std::string& kiedyPobrano);
// Wartość wiersza `Źródło otoczenia` na U-05 (kanwa) w wariantach od najpełniejszego (wybiera ten, który
// mieści się w kolumnie, src/ekrany.h dopasujWartosc()): `OK {kiedy}`, `wyłączone`, `jeszcze bez odczytu`,
// `brak odpowiedzi`, `HTTP {kod}` (R-AW-25), `przekierowanie`, `adres poza siecią domową` (R-NF-27), `wersja kontraktu
// {n}`, `odpowiedź nieczytelna`, `odpowiedź za duża`. Pełny opis z liniaTestuOtoczenia() idzie do logu.
std::vector<std::string> wartosciTestuOtoczenia(const Otoczenie& o, const std::string& kiedyPobrano);

// R-NF-27: czy adres źródła otoczenia wskazuje sieć lokalną. Przyjmuje pełny URL; przyjęty tylko
// schemat `http://` z hostem będącym adresem IPv4 prywatnym (RFC 1918: 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16),
// link-local (169.254.0.0/16) albo nazwą zakończoną na `.local`, z opcjonalnym portem i ścieżką. Zwraca false dla
// pustego tekstu, adresu bez schematu, innego schematu (np. `https://`), adresu publicznego, nazwy spoza `.local`
// (np. `ha.lan`, `192.168.1.60.nip.io`) i adresu nieczytelnego. Bez sieci (sama analiza tekstu). Tej samej funkcji
// użyje formularz i plik konfiguracji (R-KONF-18, R-KONF-21).
// Host wyznaczany jak HTTPClient rdzenia Arduino 3.3.12 (beginInternal(): po
// `://` do pierwszego `/`, port po `:`), a odrzucane są też: `@`, `\`, spacja, znaki sterujące i bajty spoza ASCII
// gdziekolwiek w adresie; `?` albo `#` przed pierwszym `/`; schemat innymi literami niż `http://`; IPv4 w zapisie
// innym niż cztery liczby dziesiętne 0–255 bez zer wiodących (ósemkowy, szesnastkowy, jedna liczba, trzy części);
// IPv6 (`[`, więcej niż jeden `:`); końcowa kropka; `localhost` (R-NF-27 go nie wymienia). Nazwa `.local` bez względu
// na wielkość liter (mDNS nie rozróżnia).
bool adresWSieciLokalnej(const std::string& url);

// E-08 raz na epizod (6.4): linia w dniu, w którym panel pierwszy raz zobaczył ostrzeżenie z danym `od`; ten sam
// epizod następnego dnia milczy. Stan w NVS.
struct StanE08 {
  uint32_t od = 0;
  std::string dzien;   // data poranka, w której linia stanęła po raz pierwszy ("" = jeszcze nigdy)
  bool koniec = false; // świeża odpowiedź bez ostrzeżenia po tym epizodzie
};
// true: dziś pokazać linię; `s` zmienia się przy nowym epizodzie (wtedy `zmiana` = true, do zapisu). `bezOstrzezenia` =
// świeża odpowiedź źródła bez `co2_milczy`: koniec epizodu, więc ostrzeżenie z `od` = null po powrocie danych to nowy
// epizod; z `od` znanym nowy epizod rozpoznaje tylko inne `od` (to samo `od` po jednej odpowiedzi bez ostrzeżenia, np.
// chwilowe wahanie źródła, linii nie wraca; druga).
bool pokazE08(bool aktywne, bool bezOstrzezenia, uint32_t od, const std::string& dzis, StanE08& s, bool& zmiana);
