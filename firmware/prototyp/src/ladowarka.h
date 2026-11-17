// SPDX-License-Identifier: GPL-3.0-or-later
// Limit ładowania baterii na kablu (ADR-0046, R-DANE-66…R-DANE-68): rdzeń bez Arduino. Decyduje, co zapisać w układzie
// ładowarki Silergy SY6974B (I2C1, adres 0x6B), a adapter ESP32 (src/adaptery_esp32.cpp) tylko wykonuje odczyt i zapis
// jednego rejestru.
//
// Fakty o układzie (każdy z adresem źródła; czego źródła nie potwierdzają: [DANE?]):
// [K] karta katalogowa SY6974B Rev. 0.9 (Silergy 2020): https://www.silergy.com/download/downloadFile?id=4013&type=product&ftype=note
// [Z] sterownik Zephyra drivers/charger/charger_sy6974b.c:
//     https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/charger/charger_sy6974b.c
// [B] wiązanie Zephyra silergy,sy6974b: https://docs.zephyrproject.org/latest/build/dts/api/bindings/charger/silergy,sy6974b.html
// [L] łata Linux „dt-bindings: power: supply: Add Silergy SY6974B charger” (Silergy, 15.12.2025):
//     https://lkml.iu.edu/hypermail/linux/kernel/2512.2/10698.html
// [S] karta SGM41512 (układ tej samej rodziny, ten sam układ rejestrów): https://www.sg-micro.com/rect/assets/f3a68311-b70a-4a49-b161-30fa253fc690/SGM41512.pdf
//
// - REG04 bity 7:3 VREG: 3856 mV + kod × 32 mV, kody 11000…11111 = 4624 mV; kod 01111 to wyjątek 4352 mV (tylko [K];
//   sterownik Zephyra go nie zna); domyślnie 01011 = 4208 mV [K], [Z], [B], [L].
// - BATOVP: przy napięciu baterii powyżej 104% VREG (103–105%) układ przerywa ładowanie, ustawia REG09 bit 3 (BAT_FAULT,
//   zatrzaskiwany: czytać dwa razy) i daje impuls na wyjściu /INT [K]. Przy limicie 3920 mV i pełnej baterii (ok. 4,2 V)
//   to stan oczekiwany, nie awaria (ADR-0046 pkt 10).
// - REG04 bity 2:1 TOPOFF_TIMER, bit 0 VRECHG (próg ponownego ładowania 100 mV, przy 1: 200 mV poniżej VREG) [K]:
//   firmware ich nie zmienia.
// - REG05 bity 5:4 WATCHDOG: 00 wyłączony, 01 = 40 s (domyślnie), 10 = 80 s, 11 = 160 s [K]. Po starcie zasilania układ
//   jest w trybie domyślnym; każdy zapis przez I2C przełącza go w tryb host, w którym trzeba co najwyżej co tyle sekund
//   zapisać WD_RST (REG01 bit 6) albo wyłączyć licznik; po upływie licznika układ „wraca do trybu domyślnego” [K].
//   Czy przy tym rejestry wracają do wartości domyślnych, karta SY6974B nie mówi [DANE?]; karta SGM41512 mówi:
//   wszystkie poza IINDPM, VINDPM i bitami BATFET, czyli także VREG [S]. Sterownik Zephyra wyłącza licznik przy starcie
//   z tego właśnie powodu [Z].
// - REG0B bit 7 REG_RST (przywraca wartości domyślne), bity 6:3 PN = 1000 [K], [Z].
// - Rejestry są zasilane z wyższego z BUS i BAT [K]: po odłączeniu kabla wartości zostają, giną dopiero przy zaniku
//   obu (bateria odłączona albo rozładowana do odcięcia) [DANE?: karta tego wprost nie mówi].
// - CHG_CONFIG (REG01 bit 4, włączenie ładowania) domyślnie 1 [K]; firmware go nie zmienia.
#pragma once
#include <cstdint>
#include <string>

class Nvs;

namespace ladowarka {

const uint8_t REJ_VREG = 0x04;           // REG04 [K]
const uint8_t MASKA_VREG = 0xF8;         // bity 7:3 [K], [Z]
const int PRZESUNIECIE_VREG = 3;
const uint8_t REJ_WATCHDOG = 0x05;       // REG05 [K]
const uint8_t MASKA_WATCHDOG = 0x30;     // bity 5:4 [K], [Z]
const uint8_t WATCHDOG_WYLACZONY = 0x00; // 00 [K]
const uint8_t WATCHDOG_DOMYSLNY = 0x10;  // 01 = 40 s [K]
const uint8_t REJ_ID = 0x0B;             // REG0B [K]
const uint8_t MASKA_PN = 0x78;           // bity 6:3 [K]
const uint8_t PN_SY6974B = 0x40;         // 1000 na bitach 6:3 [K], [Z]
const int VREG_MIN_MV = 3856, VREG_KROK_MV = 32, VREG_MAX_MV = 4624;  // [K], [L]
const int KOD_VREG_DOMYSLNY = 11;        // 4208 mV [K], [B]
const int KOD_VREG_WYJATEK = 15;         // 4352 mV zamiast 4336 [K]
const int KOD_VREG_MAX = 24;             // 11000 = 4624 mV; 11001…11111 też 4624 mV [K]
// Napięcie końca ładowania przy włączonym limicie: ok. 70% z krzywej M-25 (src/bateria.cpp, KRZYWA_BATERII: 3,95 V =
// 70%). [PRÓG] R-DANE-66.limit_mv (docs/spec/progi.csv), obserwacja własna, do kalibracji na egzemplarzu.
// Układ ustawia najbliższe dozwolone napięcie nie wyższe: 3920 mV.
const int LIMIT_LADOWANIA_MV = 3950;
// Kody VREG, które firmware kiedykolwiek ustawiał jako limit (podpis limitu = licznik 00 i jeden z tych kodów). Przy
// zmianie progu w przyszłej wersji stary kod ZOSTAJE na tej liście: inaczej limit sprzed aktualizacji przy wyczyszczonej
// NVS byłby nierozpoznany i opcji nie dałoby się zdjąć (ADR-0046 pkt 7).
const int KODY_LIMITU[] = {2};  // 3920 mV, od 16.11.2026
// Twarda granica każdego zapisu VREG: nigdy wyżej niż kod domyślny 01011 (4208 mV) ani wyżej niż zastane.

// Napięcie VREG w mV dla kodu z REG04 bitów 7:3 (0…31; 24…31 = 4624 mV); -1 = kod spoza 0…31.
int napiecieVreg(int kod);
// Największy kod, którego napięcie nie przekracza `mV`; -1 = `mV` poniżej najniższego (3856 mV).
int kodVregNieWyzej(int mV);
// `ladowanie_limit=tak` w /konfiguracja.txt (5.6.4, formularz U-03); każda inna wartość i brak wiersza = wyłączony.
bool limitWKonfiguracji(const std::string& konfiguracja);
const char* const KLUCZ_KONFIGURACJI = "ladowanie_limit";

// Dostęp do rejestrów układu: adapter ESP32 (Wire1) albo atrapa w testach. false = brak potwierdzenia na magistrali.
class Rejestry {
 public:
  virtual ~Rejestry() = default;
  virtual bool czytaj(uint8_t rejestr, uint8_t& wartosc) = 0;
  virtual bool zapisz(uint8_t rejestr, uint8_t wartosc) = 0;
};

// Ustawienie zastane przed pierwszą zmianą (surowe REG04 i REG05); przywracane jest z niego tylko pole VREG, i to
// tylko do kodu 01011 (4208 mV) najwyżej; pole WATCHDOG z wpisu nie jest zapisywane nigdy (ADR-0046 pkt 4 i 7).
struct Zastane {
  bool znane = false;
  bool blad = false;  // odczyt NVS się nie udał (nie wiadomo, czy wpis jest): limitLadowania() nic nie zapisuje
  uint8_t reg04 = 0, reg05 = 0;
};
class PamiecZastanych {
 public:
  virtual ~PamiecZastanych() = default;
  virtual Zastane wczytaj() = 0;
  virtual bool zapisz(const Zastane& z) = 0;  // true = zapisane i potwierdzone odczytem
  virtual void usun() = 0;
};
// NVS przestrzeń `cpap`, klucz `lad_zast` (liczba: 0x5A w bitach 31:24, REG05 w 15:8, REG04 w 7:0).
class PamiecZastanychNvs : public PamiecZastanych {
 public:
  explicit PamiecZastanychNvs(Nvs& n) : n_(n) {}
  Zastane wczytaj() override;
  bool zapisz(const Zastane& z) override;
  void usun() override;

 private:
  Nvs& n_;
};

enum class Wynik : uint8_t {
  WYLACZONY,          // opcja wyłączona, brak wpisu: zero zapisów (zawsze)
  WYLACZONY_NIZEJ,    // jak WYLACZONY, ale VREG w układzie niższy niż 4208 mV: tylko wiersz logu, zero zapisów
  NIE_NA_KABLU,       // opcja włączona, panel nie na zasilaniu zewnętrznym: zero zapisów, ustawienie zostaje jak jest
  BRAK_UKLADU,        // brak odpowiedzi na magistrali albo inny układ niż SY6974B: zero zapisów
  BLAD_ODCZYTU,       // REG04 albo REG05 nie odczytany: zero zapisów
  POZA_KARTA,         // próg poniżej najniższego VREG (błąd stałej): zero zapisów
  USTAWIONY,          // limit już w układzie: zero zapisów do układu
  NIZEJ_FABRYCZNIE,   // VREG w układzie nie wyższy niż limit: nic do obniżenia, zero zapisów (nic nie podnosimy)
  BLAD_PAMIECI,       // zastane nie zapisały się w NVS albo odczyt wpisu się nie udał: zero zapisów do układu i NVS
  USTAWIONO,          // limit zapisany i potwierdzony odczytem
  PRZYWROCONO,        // VREG z wpisu przywrócony i potwierdzony odczytem; wpis usunięty
  PRZYWROCONO_Z_KARTY,// wpis z VREG powyżej 4208 mV albo poniżej kodu limitu (uszkodzony): VREG 4208 mV z karty;
                      // wpis usunięty
  WPIS_NIEAKTUALNY,   // opcja wyłączona, wpis jest, ale w układzie nie stoi nasz limit: zero zapisów, wpis usunięty
  BLAD_ZAPISU,        // zapis albo odczyt kontrolny nieudany; stan bezpieczny opisany w ADR-0046
};
const char* nazwa(Wynik w);

struct WynikLimitu {
  Wynik wynik = Wynik::WYLACZONY;
  int zapisow = 0;      // zapisy do układu w tym wywołaniu
  int vregMv = -1;      // VREG po wywołaniu (z odczytu), -1 = nieznane
  int watchdog = -1;    // pole WATCHDOG po wywołaniu (0…3), -1 = nieznane
};

// Jedno wywołanie na wybudzenie (R-DANE-67). `wlaczony` z /konfiguracja.txt, `naKablu` = stan zasilania „zewnętrzne”
// (R-DANE-65). Zapisuje tylko pola VREG (REG04 7:3) i WATCHDOG (REG05 5:4), odczyt-modyfikacja-zapis z odczytem
// kontrolnym. Każdy zapis VREG przechodzi przez jedno miejsce z granicą min(cel, zastane, 01011); przy limicie VREG
// nigdy nie rośnie; licznik tylko wyłączany (przy limicie), nigdy włączany.
WynikLimitu limitLadowania(Rejestry& r, PamiecZastanych& p, bool wlaczony, bool naKablu,
                           int limitMv = LIMIT_LADOWANIA_MV);
// Wiersz logu: `ladowanie: limit ustawiono, VREG 3920 mV, watchdog 0, zapisow 2`.
std::string opis(const WynikLimitu& w);

}  // namespace ladowarka
