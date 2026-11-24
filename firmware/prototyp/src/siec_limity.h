// SPDX-License-Identifier: GPL-3.0-or-later
// Limity czasu operacji sieciowych wobec strażnika zadania (T-WYM-81, zmiana 2026-11-24)
// i znacznik „restart w trakcie pracy w sieci” (R-AW-39, zmiana 2026-11-24; ADR-0024, zmiana 2026-11-24).
//
// Strażnik zadania (T-WYM-81, STRAZNIK_LIMIT_S = 150 s) restartuje program, gdy zadanie główne nie odnowi go dłużej
// niż limit. Rdzeń odnawia go przed i po każdej operacji sieciowej (Przebieg::odnow()), więc o restarcie decyduje
// najdłuższa pojedyncza operacja sieciowa adaptera. Ten plik opisuje ją liczbami, z których korzysta adapter panelu
// (src/adaptery_esp32.cpp) i atrapa w testach na PC (test/testy_awarie_sieci.cpp):
//  - odczyt ciała odpowiedzi HTTP ma limit przestoju (bez nowych bajtów) i limit całkowity; strażnik odnawia się
//    tylko przy postępie (nowe bajty), więc milczące połączenie kończy się po przestoju, a nie po restarcie;
//  - pozostałe fazy GET (DNS, połączenie TCP, wysyłka żądania, nagłówki) mają limity rdzenia Arduino-ESP32 3.3.12,
//    których adapter nie skraca (DNS) albo ustawia (połączenie i nagłówki: ZadanieHttp::limitMs);
//  - najdluzszaPrzerwaGetMs() sumuje je na najgorszy przypadek; T-WYM-81 wymaga, żeby suma była krótsza niż limit
//    strażnika o ZAPAS_STRAZNIKA_MS.
// Bez Arduino: decyzje i liczby testowane na PC; adapter tylko wykonuje.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include "adaptery.h"

namespace siec_limity {

// DNS: lwIP 2.1 (rdzeń 3.3.12) pyta serwer 4 razy (DNS_MAX_RETRIES) po 1, 1, 2 i 3 s, czyli ok. 7 s na serwer,
// najwyżej 3 serwery (CONFIG_LWIP_DNS_MAX_SERVERS), a przy globalnym IPv6 NetworkManager::hostByName() pyta dwa razy
// (AF_INET6, potem AF_UNSPEC): 2 × 3 × 7 s = 42 s. [szacunek] z kodu lwIP i rdzenia, nie zmierzone; adres AirBridge
// jako IP (zalecany w instrukcji) nie ma tej fazy.
const uint32_t DNS_NAJDLUZEJ_MS = 42000;
// Wysyłka żądania: NetworkClient::write() próbuje 10 razy po 1 s (WIFI_CLIENT_MAX_WRITE_RETRY,
// WIFI_CLIENT_SELECT_TIMEOUT_US) [szacunek z kodu rdzenia].
const uint32_t WYSYLKA_NAJDLUZEJ_MS = 10000;
// Skan sieci przy braku połączenia (opis błędu do logu i U-05): WiFi.setScanTimeout() [PRÓG] techniczny (rdzeń
// domyślnie 60 s); 13 kanałów po SKAN_NA_KANAL_MS to ok. 4 s, więc 10 s ma zapas.
const uint32_t SKAN_LIMIT_MS = 10000;
const uint32_t SKAN_NA_KANAL_MS = 300;
// Zapas między najdłuższą przerwą bez odnowienia strażnika a jego limitem [PRÓG] techniczny: opóźnienia rdzenia
// poza modelem (zamknięcie gniazda, rozłączenie Wi-Fi, zapis portu) i błąd szacunku DNS.
const uint32_t ZAPAS_STRAZNIKA_MS = 30000;
// Odczyt ciała: limit całkowity od wejścia w get(), gdy ZadanieHttp::budzetCialaMs nie podaje krótszego [PRÓG]
// techniczny: największy plik (plik sesji do AB_MAKS_BAJTOW_PLIKU_SESJI) przy 20 kB/s; strażnik odnawia się przy
// każdym kawałku danych, więc ten limit nie musi być krótszy niż strażnik.
const uint32_t CIALO_NAJDLUZEJ_MS = 120000;
// Przestój ciała (bez nowych bajtów), gdy ZadanieHttp::limitMs = 0.
const uint32_t PRZESTOJ_DOMYSLNY_MS = 15000;

// Przestój ciała: ZadanieHttp::limitMs (ten sam limit co połączenie i nagłówki), 0 → PRZESTOJ_DOMYSLNY_MS.
uint32_t przestojCialaMs(const ZadanieHttp& z);
// Limit całkowity odczytu od wejścia w get(): budzetCialaMs, gdy niezerowy i krótszy, inaczej CIALO_NAJDLUZEJ_MS.
uint32_t limitCialaMs(const ZadanieHttp& z);

enum class StanCiala : uint8_t { CZYTAJ, PRZESTOJ, ZA_DLUGO };
// Decyzja w pętli odczytu ciała (adapter: millis()). Przyjmuje zadanie, chwilę wejścia w get(), chwilę ostatniego
// kawałka danych (na początku: koniec nagłówków) i teraz, na jednym zegarze. Zwraca ZA_DLUGO po limitCialaMs() od
// wejścia, PRZESTOJ po przestojCialaMs() bez danych, inaczej CZYTAJ.
StanCiala stanCiala(const ZadanieHttp& z, uint32_t startMs, uint32_t ostatniMs, uint32_t terazMs);

// Czy host w adresie `http://host[:port]/…` jest nazwą (wymaga DNS), a nie adresem IPv4.
bool hostJestNazwa(const std::string& url);

// Najdłuższa przerwa bez odnowienia strażnika w jednym Siec::get() adaptera panelu (najgorszy przypadek): DNS (tylko
// przy nazwie hosta), połączenie TCP (limitMs), wysyłka, nagłówki (limitMs od ostatniego bajtu, bez danych) i ciało
// bez danych (przestój). Po każdym kawałku danych adapter odnawia strażnika, więc ciało, które płynie, się nie liczy.
uint32_t najdluzszaPrzerwaGetMs(const ZadanieHttp& z);
// To samo dla HTTPS wydań (WydaniaEsp32, ADR-0032): DNS, połączenie, uzgadnianie TLS, wysyłka i nagłówki, każde
// z limitem `limitPolaczeniaMs` (aktualizacja::LIMIT_POLACZENIA_MS); ciało ma pętlę z odnowieniem w każdym obiegu.
uint32_t najdluzszaPrzerwaHttpsMs(uint32_t limitPolaczeniaMs);
// Najdłuższa dozwolona przerwa: limit strażnika minus ZAPAS_STRAZNIKA_MS (T-WYM-81).
uint32_t dozwolonaPrzerwaMs(uint32_t limitStraznikaS);

// Dekoder `Transfer-Encoding: chunked` (RFC 9112 7.1) dla pętli odczytu ciała adaptera: rozmiar szesnastkowo
// (rozszerzenia po `;` pomijane), CRLF, dane, CRLF; kawałek 0 kończy ciało (trailer pomijany). Wejście dowolnymi
// kawałkami. Rozmiar kawałka ponad 8 cyfr albo zły znak → błąd.
class DekoderChunked {
 public:
  // Dopisuje zdekodowane bajty do `wyj`; zwraca false przy błędzie (dalej już tylko false).
  bool dodaj(const uint8_t* b, size_t n, std::string& wyj);
  bool koniec() const { return stan_ == Stan::KONIEC; }
  bool blad() const { return stan_ == Stan::BLAD; }
 private:
  enum class Stan : uint8_t { ROZMIAR, ROZSZERZENIE, LF_ROZMIARU, DANE, CR_DANYCH, LF_DANYCH, KONIEC, BLAD };
  Stan stan_ = Stan::ROZMIAR;
  uint32_t rozmiar_ = 0, zostalo_ = 0;
  int cyfr_ = 0;
};

// Strumień ciała odpowiedzi HTTP (adapter: NetworkClient rdzenia, millis(), delay(), esp_task_wdt_reset(); testy: atrapa
// z wirtualnym czasem). Wszystkie czasy na zegarze strumienia (ms()).
class StrumienCiala {
 public:
  virtual ~StrumienCiala() = default;
  virtual int dostepne() = 0;                       // bajty do odczytu bez czekania
  virtual int czytaj(uint8_t* b, size_t n) = 0;     // przeczytane bajty (≤ 0: nic)
  virtual bool polaczony() = 0;                     // druga strona nie zamknęła połączenia
  virtual uint32_t ms() = 0;                        // zegar adaptera
  virtual void czekaj(uint32_t ms) = 0;             // krótka przerwa w obiegu bez danych
  virtual void odnow() = 0;                         // strażnik zadania, tylko przy nowych bajtach
};
const uint32_t OBIEG_CIALA_MS = 5;  // przerwa w obiegu pętli bez danych
// Pętla odczytu ciała (T-WYM-81, zmiana 2026-11-24; do beta.17 HTTPClient::writeToStream() bez limitu przy połączeniu
// milknącym bez zamknięcia). `t0`: chwila wejścia w get() na zegarze strumienia; `chunked`: Transfer-Encoding;
// o.contentLength: Content-Length albo -1. Kończy się: ciało kompletne (Content-Length, kawałek 0 `chunked`, albo bez
// długości zamknięcie połączenia) → liczba bajtów ciała; przestój albo limit całkowity (stanCiala(), o.zaDlugo przy
// limicie) albo zamknięcie przed końcem → -11 (HTTPC_ERROR_READ_TIMEOUT); przerwij() → -10, o.przerwane; ciało ponad
// maksBajtow → -10, o.zaDuzo, ciało bez nadmiaru; zły `chunked` → -7 (HTTPC_ERROR_ENCODING). Strażnik odnawia się
// tylko po kawałku danych.
int czytajCialo(StrumienCiala& s, const ZadanieHttp& z, uint32_t t0, bool chunked, OdpowiedzHttp& o);

// Sieć ze znacznikiem „w sieci” (R-AW-39, zmiana 2026-11-24): przed każdą operacją, która może czekać na sieć
// (polacz, opisBledu, czasZSieci, get), ustawia znacznik w liczniku startów (RTC_NOINIT, src/start.h
// wejscieWSiec()), po niej go zdejmuje. Restart strażnika w trakcie takiej operacji następny start rozpoznaje po
// znaczniku (decyzjaStartu(): DecyzjaStartu::restartWSieci) i nie liczy go do trybu awaryjnego. `bezSieci` = true:
// wybudzenie po takim restarcie: polacz() od razu false (bez radia), get() i czasZSieci() bez sieci; reszta jak
// w opakowanej sieci.
class SiecZeZnacznikiem : public Siec {
 public:
  SiecZeZnacznikiem(Siec& s, Uruchomienie& u) : s_(s), u_(u) {}
  bool bezSieci = false;
  bool polacz(uint32_t limitMs, const std::function<bool()>& przerwij, bool& przerwane) override;
  bool polaczona() override { return !bezSieci && s_.polaczona(); }
  void wylacz() override { s_.wylacz(); }
  std::string opisBledu() override;
  std::string opisPolaczenia() override { return s_.opisPolaczenia(); }
  bool czasZSieci(uint32_t limitMs) override;
  OdpowiedzHttp get(const ZadanieHttp& z) override;
  bool otworzOknoSerwisowe() override { return s_.otworzOknoSerwisowe(); }
  bool oknoSerwisoweOtwarte() override { return s_.oknoSerwisoweOtwarte(); }
  void obsluzOknoSerwisowe() override { s_.obsluzOknoSerwisowe(); }
  uint32_t losowa() override { return s_.losowa(); }
  std::string adresIp() override { return s_.adresIp(); }
  std::string nazwaSieci() override { return s_.nazwaSieci(); }
  int rssi() override { return s_.rssi(); }
  Md5Hex md5() override { return s_.md5(); }
  void ustawSiecDomowa(const std::string& ssid, const std::string& haslo) override { s_.ustawSiecDomowa(ssid, haslo); }
  bool wlaczRadio() override { return s_.wlaczRadio(); }
  bool uruchomSiecPanelu(const std::string& ssid, const std::string& haslo) override {
    return s_.uruchomSiecPanelu(ssid, haslo);
  }
  std::string adresSieciPanelu() override { return s_.adresSieciPanelu(); }
  std::string idPanelu() override { return s_.idPanelu(); }
 private:
  void znacznik(bool w);
  Siec& s_;
  Uruchomienie& u_;
};

// Wydania (HTTPS GitHub, ADR-0032) ze znacznikiem „w sieci” wokół pobierania (jak SiecZeZnacznikiem).
class WydaniaZeZnacznikiem : public Wydania {
 public:
  WydaniaZeZnacznikiem(Wydania* w, Uruchomienie& u) : w_(w), u_(u) {}
  OdpowiedzWydania pobierzTekst(const ZadanieWydania& z) override;
  WynikObrazu pobierzObraz(const ZadanieWydania& z, uint64_t rozmiar, const std::function<void()>& postep) override;
  bool ustawStartowy() override { return w_->ustawStartowy(); }
  void porzuc() override { w_->porzuc(); }
  void restart() override { w_->restart(); }
  uint64_t rozmiarSlotu() override { return w_->rozmiarSlotu(); }
  Wydania* opakowane() const { return w_; }
 private:
  void znacznik(bool w);
  Wydania* w_;
  Uruchomienie& u_;
};

}  // namespace siec_limity
