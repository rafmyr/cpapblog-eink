// SPDX-License-Identifier: GPL-3.0-or-later
// Serwer raportu dla telefonu (T-INT-06, T-KOMP-11 spec technicznej 7.7; spec funkcjonalna 5.13.5–5.13.7).
// Rdzeń bez gniazd: trasy, token, limit 15 min, licznik złych żądań, lista raportów z 90 dni, strona HTML.
// Gniazdo HTTP to adapter (GniazdoHttp): na panelu src/serwer_raportu_esp32.cpp (WebServer z rdzenia Arduino-ESP32,
// tylko sieć domowa, port 80), w testach i symulatorze atrapa. Serwer czyta tylko `/raporty/` (R-NF-10); EDF
// podaje jako własny plik, bez adresu AirBridge (R-LEK-20, ADR-0002).
// Token: 128 bitów z generatora sprzętowego po włączeniu Wi-Fi (T-WYM-35, T-NF-27); rdzeń buduje z bajtów tekst
// 26 znaków base32 (alfabet Crockforda małymi literami: bez i, l, o, u, łatwiejszy do przepisania z ekranu).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace serwer {

const uint32_t CZAS_MS = 15u * 60u * 1000u;  // 15 min [PRÓG] (R-LEK-04)
const int ZLE_MAKS = 5;                      // [PRÓG] żądań bez ważnego tokenu (R-LEK-03b)
const int DNI_LISTY = 90;                    // [PRÓG] raporty na liście (R-LEK-32)
const int BAJTY_TOKENU = 16;                 // 128 bitów (T-NF-27)

// Token z 16 bajtów: 26 znaków [0-9a-hjkmnp-tv-z].
std::string token(const uint8_t bajty[BAJTY_TOKENU]);

enum class Tryb : unsigned char { PDF, SCIAGA };  // ściąga: serwer podaje tylko EDF (R-LEK-18)

struct Pozycja {  // raport z `/raporty/`
  std::string sciezka;  // `/raporty/2026-11-04-L-01.pdf`
  std::string data;     // data utworzenia (z nazwy pliku), RRRR-MM-DD
};
// R-NF-09, R-NF-27: adres IPv4 z sieci prywatnej (10/8, 172.16/12, 192.168/16, 169.254/16); inne = serwer nie startuje.
bool adresPrywatny(const std::string& ip);
// Porównanie tokenu w stałym czasie (bez wczesnego wyjścia na pierwszym różnym znaku).
bool rowneStalyCzas(const std::string& a, const std::string& b);

// Pozycje z listy plików katalogu `/raporty/` (nazwy `RRRR-MM-DD-….pdf`), od najnowszej; inne nazwy pominięte.
std::vector<Pozycja> pozycjeZPlikow(const std::vector<std::string>& sciezki);

struct Odpowiedz {
  int kod = 404;
  std::string typ = "text/plain; charset=utf-8";
  std::string cialo;
  std::string plik;          // ścieżka pliku z `/raporty/` do podania jako ciało (czyta wołający przez Pliki)
  std::string nazwaPobrania; // nagłówek Content-Disposition
  bool edf = false;          // żądanie EDF (podaje wołający; bez paczki EDF odpowiedź EDF_NIEDOSTEPNA)
  bool pdfPobrany = false;   // R-LEK-21: wołający pokazuje `Pobrano o {hh:mm}.`
};

// Paczki EDF w tej wersji nie ma: strona i odpowiedź na `/edf` mówią to wprost, a nie zgłaszają awarii
// AirBridge, której nie ma.
constexpr const char* EDF_NIEDOSTEPNA = "Paczka EDF: niedostępna w tej wersji.";

class Sesja {
 public:
  // Nowa sesja z nowym tokenem (R-LEK-02); poprzedni token przestaje działać (R-LEK-03). `adres` = IP panelu w sieci
  // domowej; `biezacy` = plik raportu z tego otwarcia ("" w trybie ściągi); `dzis` do granicy 90 dni.
  void otworz(const std::string& token, uint32_t ms, const std::string& adres, Tryb tryb, const std::string& biezacy,
              const std::vector<Pozycja>& raporty, const std::string& dzis, const std::string& temat,
              const std::string& okres, bool edfDostepny);
  bool aktywna(uint32_t ms) const;  // false po 15 min (R-LEK-04), po 5 złych żądaniach (R-LEK-03b) i po zamknij()
  void zamknij() { otwarta_ = false; }
  bool zamknietaZlymi() const { return zle_ >= ZLE_MAKS; }
  // Żądanie GET `sciezka` (bez hosta, z zapytaniem albo bez) w chwili `ms`.
  Odpowiedz obsluz(const std::string& sciezka, uint32_t ms);
  // R-LEK-33: lista ze starszymi raportami do końca tej sesji.
  void pokazStarsze() {
    starsze_ = true;
    liczWidoczne();
  }
  bool starsze() const { return starsze_; }
  bool edf() const { return edf_; }  // paczka EDF dostępna w tej sesji
  uint32_t koniecMs() const { return od_ + CZAS_MS; }
  // Adres z kodu QR: strona (`http://{ip}/r/{token}/`) albo w trybie ściągi EDF (`…/edf`).
  std::string adresQr() const;
  // Adres do wpisania ręcznie, bez `http://` (stopka ekranu raportu, 5.13.6).
  std::string adresReczny() const;
  const std::vector<Pozycja>& lista() const { return widoczne_; }
  int zleZadania() const { return zle_; }

 private:
  bool otwarta_ = false, starsze_ = false, edf_ = false;
  uint32_t od_ = 0;
  int zle_ = 0;
  std::string token_, adres_, biezacy_, dzis_, temat_, okres_;
  Tryb tryb_ = Tryb::PDF;
  std::vector<Pozycja> wszystkie_, widoczne_;
  void liczWidoczne();
  std::string strona() const;
};

// Adapter gniazda HTTP (część T-KOMP-16). start(): nasłuch w sieci domowej (tylko gdy Wi-Fi połączone), stop():
// koniec nasłuchu i zamknięcie połączeń (R-LEK-11), obsluz(): jedno przejście pętli, każde przychodzące żądanie GET
// przekazane do `f` (ścieżka z zapytaniem), odpowiedź wysłana. Panel ma jedno gniazdo raportu.
class GniazdoHttp {
 public:
  virtual ~GniazdoHttp() = default;
  virtual bool start(uint16_t port) = 0;
  virtual void stop() = 0;
  virtual void obsluz(const std::function<Odpowiedz(const std::string& sciezka)>& f) = 0;
  // (formularz konfiguracji, T-INT-07): żądanie z metodą i ciałem. `cialo` dla POST to pary
  // `application/x-www-form-urlencoded` (adapter ESP32 składa je z argumentów WebServer). Domyślnie: tylko GET przez
  // obsluz (atrapy raportu sprzed).
  struct Zadanie {
    std::string metoda;  // "GET", "POST", inne: "INNA"
    std::string sciezka, cialo;
  };
  virtual void obsluzZadania(const std::function<Odpowiedz(const Zadanie& z)>& f) {
    obsluz([&f](const std::string& sc) { return f(Zadanie{"GET", sc, ""}); });
  }
};
// Rejestracja gniazda: na panelu robi to src/serwer_raportu_esp32.cpp przy starcie programu, w testach atrapa.
// nullptr = brak gniazda (dyrygent pokazuje `Nie mogę włączyć Wi-Fi`, R-LEK-22).
GniazdoHttp* gniazdo();
void ustawGniazdo(GniazdoHttp* g);

}  // namespace serwer
