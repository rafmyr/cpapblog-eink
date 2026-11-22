// SPDX-License-Identifier: GPL-3.0-or-later
// Adaptery panelu reTerminal E1001 (src/adaptery_esp32.h): kod sprzętu, kontrakty w src/adaptery.h.
#include "adaptery_esp32.h"
#include "magazyn.h"
#include "aktualizacja.h"
#include "szybkosc.h"
#include "czekanie.h"  // ADR-0047: KAWALEK_CZEKANIA_S
#include "wysylka_obrazu.h"  // obraz do sterownika e-ink blokiem (SterownikBlokiem)
#include "certyfikaty_github.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <mbedtls/md5.h>
#include <Preferences.h>
#include <nvs.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <cerrno>
#include <LittleFS.h>
#include <Wire.h>
#include <SPI.h>
#include <time.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <esp_random.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <soc/soc.h>
#include <soc/gpio_reg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <GxEPD2_BW.h>

// --- piny (wiki Seeed, reTerminal E10xx) ---
#define EPD_SCK 7
#define EPD_MOSI 9
#define EPD_CS 10
#define EPD_DC 11
#define EPD_RES 12
#define EPD_BUSY 13
// Zegar SPI ekranu w Hz. Od 22.11.2026 8 MHz (pomiar na panelu 22.11.2026: częściowe 1,33 s wobec 1,66 s
// przy 2 MHz, bez artefaktów; jeden egzemplarz, temperatura pokojowa). Cofnięcie bez zmiany kodu:
// -DEPD_SPI_HZ=2000000.
#ifndef EPD_SPI_HZ
#define EPD_SPI_HZ 8000000
#endif
// Wysyłka obrazu do sterownika jednym blokiem SPI (klasa SterownikBlokiem niżej, od 22.11.2026). Wyłącznik do
// porównania na panelu i cofnięcia bez zmiany kodu: -DEPD_WYSYLKA_BLOKIEM=0 (droga przez GxEPD2 bajt po bajcie).
#ifndef EPD_WYSYLKA_BLOKIEM
#define EPD_WYSYLKA_BLOKIEM 1
#endif
// Według oznaczeń na obudowie: zielony = ↻, `>` = ▶, `<` = ◀, tak samo jak podpisy
// w stopce (inne przypisanie sprawiało, że przycisk podpisany w stopce jako ▶ wracał na ekran dnia).
#define KEY_SRODEK 3  // KEY0, zielony: ↻
#define KEY_PRAWY 4   // KEY1, `>`: ▶
#define KEY_LEWY 5    // KEY2, `<`: ◀
#define BUZZER 45
#define LED 6         // logika odwrócona
#define I2C_SDA 19
#define I2C_SCL 20
#define BAT_ADC 1
#define BAT_EN 21
// Ładowarka baterii U2 SY6974B na drugiej magistrali I2C (R-DANE-65). Źródła: schemat producenta reTerminal E1001
// v1.2 (historia zmian w PDF: 20.11.2025; ramka arkusza: 2025-11-14), arkusz „03 Power” (U2 SY6974B: SCL = ESP_IO40/I2C1_SCL, SDA = ESP_IO39/I2C1_SDA, BUS =
// TYPEC_5V; wyjście STAT idzie tylko do czerwonej diody, nie do ESP32):
// https://files.seeedstudio.com/wiki/reterminal_e10xx/res/202004307_reTerminal_E1001_V1_2_SCH_251120.pdf
// adres 0x6B i rejestry: opis płytki i sterownik w Zephyrze (boards/seeed/reterminal_e1001/
// reterminal_e1001_procpu.dts `sy6974b@6b`, drivers/charger/charger_sy6974b.c). Historia zmian schematu: przed v1.2
// U2 to ETA6003 (bez I2C), więc na starszych płytkach odczyt się nie uda i stan zostaje NIEZNANE.
// NIEZWERYFIKOWANE NA EGZEMPLARZU: do pomiaru z kablem i bez.
#define I2C1_SDA 39
#define I2C1_SCL 40
#define LAD_ADRES 0x6B           // SY6974B
#define LAD_REJ_STAN 0x08        // REG08: stan
#define LAD_STAN_PG 0x04         // REG08 bit 2 (PG_STAT): napięcie na wejściu BUS dobre
#define LAD_REJ_UKLAD 0x0B       // REG0B: bity 6..3 = numer układu
#define LAD_UKLAD_MASKA 0x78
#define LAD_UKLAD_SY6974B 0x40   // 1000 na bitach 6..3
#define LAD_REJ_WEJSCIE 0x0A     // REG0A: bit 7 BUS_GD (sterownik Zephyra bierze z niego „online”); tylko do logu
#define LAD_LIMIT_MS 20          // limit czasu magistrali I2C1 na jedną operację (wisząca magistrala)

static const char* STREFA = "CET-1CEST,M3.5.0,M10.5.0/3";
static const char* OTA_HOST = "cpap-eink";  // cpap-eink.local, upload_port w [env:e1001_ota]

// ---------- piny i przygotowanie ----------
static int pin(Przycisk p) { return p == Przycisk::LEWY ? KEY_LEWY : p == Przycisk::SRODEK ? KEY_SRODEK : KEY_PRAWY; }

void piskBudzenia() {
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) tone(BUZZER, 2000, 60);
}

void przygotujPanel() {
  pinMode(KEY_PRAWY, INPUT);
  pinMode(KEY_SRODEK, INPUT);
  pinMode(KEY_LEWY, INPUT);
  // bufor odbioru portu serwisowego 4 KB przed begin(): komenda `W` (T-WYM-87) śle treść pliku bez przerw, a rdzeń
  // czyta port w obiegach co OBIEG_OKNA_PRZYWRACANIA_MS (domyślny bufor 256 B to 22 ms przy 115 200 b/s)
  Serial.setRxBufferSize(4096);
  Serial.begin(115200);
  delay(50);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  setenv("TZ", STREFA, 1);  // zegar RTC chodzi w śnie, strefa nie: ustawiamy zawsze
  tzset();
}

void logZajetosciNvs() {
  nvs_stats_t s{};
  const esp_err_t e = nvs_get_stats(nullptr, &s);  // domyślna partycja `nvs` (firmware/partycje-e1001.csv)
  if (e != ESP_OK) {
    Serial.printf("nvs start: odczyt zajetosci nieudany (blad 0x%x)\n", (unsigned)e);
    return;
  }
  Serial.printf("nvs start: wpisy zajete %u, wolne %u (dostepne %u), wszystkie %u, przestrzenie %u\n",
                (unsigned)s.used_entries, (unsigned)s.free_entries, (unsigned)s.available_entries,
                (unsigned)s.total_entries, (unsigned)s.namespace_count);
}

void snAwaryjny() {
  Serial.println("dyrygent wrócił bez snu: sen awaryjny 30 min");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(30ULL * 60 * 1000000ULL);
  esp_sleep_enable_ext1_wakeup((1ULL << KEY_PRAWY) | (1ULL << KEY_SRODEK) | (1ULL << KEY_LEWY), ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

// ---------- czekanie na kablu zamiast głębokiego snu (ADR-0047) ----------
// Stan startu przebiegu po czekaniu (SenEsp32::czekaj()): UruchomienieEsp32 mówi wtedy „wybudzenie z uśpienia” z tym
// powodem i przyciskami, ZegarEsp32::ms() liczy od końca czekania, obraz pomiarowy podpisuje wiersz `POMIAR`.
namespace czekanieEsp32 {
bool poCzekaniu = false;
PowodWybudzenia powod = PowodWybudzenia::START;
uint8_t budzace = 0;
uint32_t poczatekMs = 0;  // millis() początku bieżącego przebiegu (0 = start programu)
uint32_t trwanieBudzacego = 0;  // ms trzymania naciśnięcia budzącego już puszczonego (z kolejki), 0 = trzymany teraz
}  // namespace czekanieEsp32

// ---------- zegar ----------
uint32_t ZegarEsp32::teraz() {
  time_t t;
  time(&t);
  return (uint32_t)t;
}
// od startu programu albo od końca czekania na kablu (ADR-0047): rdzeń liczy przytrzymanie przycisku budzącego,
// chwilę wybudzenia i aktywność od 0 każdego przebiegu, jak po głębokim śnie
uint32_t ZegarEsp32::ms() { return (uint32_t)millis() - czekanieEsp32::poczatekMs; }
void ZegarEsp32::czekaj(uint32_t m) { delay(m); }
bool ZegarEsp32::lokalny(uint32_t t, std::tm& w) {
  time_t tt = (time_t)t;
  return localtime_r(&tt, &w) != nullptr;
}
// R-AW-18: strefa panelu jest ustawiona (setenv TZ w starcie), mktime liczy według jej reguły
uint32_t ZegarEsp32::zLokalnego(const std::tm& l) {
  std::tm kopia = l;
  kopia.tm_isdst = -1;
  time_t t = mktime(&kopia);
  return t < 0 ? 0 : (uint32_t)t;
}
bool ZegarEsp32::ustaw(uint32_t t) {
  struct timeval tv;
  tv.tv_sec = (time_t)t;
  tv.tv_usec = 0;
  return settimeofday(&tv, nullptr) == 0;
}

// ---------- NVS (Preferences, jak w prototypie) ----------
static Preferences prefs;

std::string NvsEsp32::tekst(const char* p, const char* k, const std::string& d) {
  prefs.begin(p, true);
  std::string v = prefs.getString(k, d.c_str()).c_str();
  prefs.end();
  return v;
}
bool NvsEsp32::ustawTekst(const char* p, const char* k, const std::string& v) {
  if (!prefs.begin(p, false)) return false;
  size_t n = prefs.putString(k, v.c_str());  // liczba zapisanych znaków, 0 przy błędzie (pusty tekst: odczyt kontrolny)
  prefs.end();
  return n == v.size();
}
uint32_t NvsEsp32::liczbaU(const char* p, const char* k, uint32_t d) {
  prefs.begin(p, true);
  uint32_t v = prefs.getUInt(k, d);
  prefs.end();
  return v;
}
// Preferences::getUInt() zwraca wartość domyślną i przy braku klucza, i przy błędzie, więc wprost przez ESP-IDF (ta sama
// domyślna partycja `nvs` i ten sam typ u32 co Preferences::putUInt()): ESP_ERR_NVS_NOT_FOUND = brak przestrzeni albo
// klucza, każdy inny kod różny od ESP_OK (także zły typ wpisu) = błąd odczytu (ADR-0046 pkt 4).
Nvs::Odczyt NvsEsp32::liczbaUStan(const char* p, const char* k, uint32_t& v) {
  nvs_handle_t h = 0;
  esp_err_t e = nvs_open(p, NVS_READONLY, &h);
  if (e == ESP_ERR_NVS_NOT_FOUND) return Odczyt::BRAK;
  if (e != ESP_OK) return Odczyt::BLAD;
  uint32_t x = 0;
  e = nvs_get_u32(h, k, &x);
  nvs_close(h);
  if (e == ESP_ERR_NVS_NOT_FOUND) return Odczyt::BRAK;
  if (e != ESP_OK) return Odczyt::BLAD;
  v = x;
  return Odczyt::JEST;
}
void NvsEsp32::ustawLiczbeU(const char* p, const char* k, uint32_t v) {
  prefs.begin(p, false);
  prefs.putUInt(k, v);
  prefs.end();
}
int32_t NvsEsp32::liczba(const char* p, const char* k, int32_t d) {
  prefs.begin(p, true);
  int32_t v = prefs.getInt(k, d);
  prefs.end();
  return v;
}
void NvsEsp32::ustawLiczbe(const char* p, const char* k, int32_t v) {
  prefs.begin(p, false);
  prefs.putInt(k, v);
  prefs.end();
}
bool NvsEsp32::flaga(const char* p, const char* k, bool d) {
  prefs.begin(p, true);
  bool v = prefs.getBool(k, d);
  prefs.end();
  return v;
}
void NvsEsp32::ustawFlage(const char* p, const char* k, bool v) {
  prefs.begin(p, false);
  prefs.putBool(k, v);
  prefs.end();
}
size_t NvsEsp32::bajty(const char* p, const char* k, void* cel, size_t n) {
  prefs.begin(p, true);
  size_t w = 0;
  if (prefs.getBytesLength(k) == n) w = prefs.getBytes(k, cel, n);
  prefs.end();
  return w == n ? n : 0;
}
void NvsEsp32::ustawBajty(const char* p, const char* k, const void* z, size_t n) {
  prefs.begin(p, false);
  prefs.putBytes(k, z, n);
  prefs.end();
}
void NvsEsp32::usun(const char* p, const char* k) {
  prefs.begin(p, false);
  prefs.remove(k);
  prefs.end();
}

// ---------- pliki w LittleFS ----------
bool PlikiEsp32::zamontuj() {
  montowane_ = LittleFS.begin(false);  // bez formatowania: nieudane montowanie nie kasuje historii
  return montowane_;
}
// (22.11.2026): w arduino-esp32 3.x `LittleFS.exists()` to `VFSImpl::exists()` = próba
// otwarcia pliku do odczytu (`VFSFileImpl` z `fopen(…, "r")`, libraries/FS/src/vfs_api.cpp), więc plik, który chwilowo
// się nie otwiera (brak pamięci na bufor przy Wi-Fi i TLS, limit otwartych plików, błąd nośnika), wyglądał jak brak
// pliku. Istnienie z `stat()` na pełnej ścieżce VFS (`LittleFS.mountpoint()` + nazwa, FS::mountpoint() z FS.h; przy
// `LittleFS.begin(false)` ścieżka bazowa `/littlefs`): bez przydziału `FILE` i bufora, także dla katalogów.
static bool statVfs(const char* nazwa, struct stat& st) {
  const char* mp = LittleFS.mountpoint();
  if (!mp || !nazwa) return false;
  std::string p = std::string(mp) + nazwa;
  return ::stat(p.c_str(), &st) == 0;
}
static bool istniejeVfs(const char* nazwa) {
  struct stat st;
  return statVfs(nazwa, st);
}
bool PlikiEsp32::istnieje(const char* n) { return montowane_ && istniejeVfs(n); }

bool PlikiEsp32::czytajWiersze(const char* nazwa, std::vector<std::string>& w) {
  w.clear();
  if (!montowane_) return false;
  String tmp = String(nazwa) + ".tmp";
  if (!istniejeVfs(nazwa) && istniejeVfs(tmp.c_str())) LittleFS.rename(tmp, nazwa);
  File f = LittleFS.open(nazwa, "r");
  if (!f) return false;
  while (f.available()) {
    String l = f.readStringUntil('\n');
    l.trim();
    if (l.length()) w.push_back(l.c_str());
  }
  f.close();
  return true;
}

static void utworzKatalogi(const char* nazwa);  // niżej; LittleFS.open(…, "w") nie zakłada katalogów (R4 raportu na sprzęcie)

// Rozmiar pliku na flash po close(): write() liczy bajty przyjęte do bufora, a brak miejsca może wyjść dopiero przy
// zrzucie bufora w close(). Brak pliku = SIZE_MAX (nigdy równe oczekiwanemu rozmiarowi).
static size_t rozmiarNaFlash(const char* nazwa) {
  File f = LittleFS.open(nazwa, "r");
  if (!f) return SIZE_MAX;
  size_t n = f.size();
  f.close();
  return n;
}

bool PlikiEsp32::zapiszAtomowo(const char* nazwa, const std::string& tresc) {
  if (!montowane_) return false;
  utworzKatalogi(nazwa);
  String tmp = String(nazwa) + ".tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  size_t zapisane = f.write((const uint8_t*)tresc.data(), tresc.size());
  f.close();
  if (zapisane == tresc.size()) zapisane = rozmiarNaFlash(tmp.c_str());  // przed rename: plik naprawdę cały
  if (zapisane != tresc.size()) {  // pełna pamięć albo błąd zapisu: stary plik zostaje nietknięty
    Serial.printf("zapis %s: %u z %u bajtów, zostaje stary plik\n", nazwa, (unsigned)zapisane, (unsigned)tresc.size());
    LittleFS.remove(tmp);
    return false;
  }
  if (!LittleFS.rename(tmp, nazwa)) {  // littlefs: rename nadpisuje cel atomowo
    LittleFS.remove(nazwa);
    return LittleFS.rename(tmp, nazwa);
  }
  return true;
}

bool PlikiEsp32::dopiszWiersz(const char* nazwa, const std::string& wiersz) {
  if (!montowane_) return false;
  utworzKatalogi(nazwa);
  bool nowaLinia = false;  // urwany ostatni wiersz (np. zanik zasilania w trakcie zapisu) nie skleja się z nowym
  // rozmiar przed dopisaniem tylko z udanego stat() albo open("r"); bez niego wynik z write() bez porównania
  // rozmiaru na flash (bez fałszywej porażki udanego dopisania)
  bool rozmiarZnany = false;
  size_t przed = 0;
  struct stat st;
  errno = 0;
  if (statVfs(nazwa, st)) {
    przed = (size_t)st.st_size;
    rozmiarZnany = true;
  } else if (errno == ENOENT) {
    rozmiarZnany = true;  // pliku nie ma: przed dopisaniem 0 B
  }
  File r = LittleFS.open(nazwa, "r");
  if (r) {
    size_t n = r.size();
    przed = n;
    rozmiarZnany = true;
    if (n && r.seek((uint32_t)(n - 1)) && r.read() != '\n') nowaLinia = true;
    r.close();
  }
  File f = LittleFS.open(nazwa, "a");
  if (!f) {
    Serial.printf("dopisanie %s: nie da się otworzyć pliku\n", nazwa);
    return false;
  }
  // liczba bajtów sprawdzona jak w zapisz() (wcześniej true bez sprawdzenia wyniku print())
  std::string t = (nowaLinia ? "\n" : "") + wiersz + "\n";
  size_t zapisane = f.write((const uint8_t*)t.data(), t.size());
  f.close();
  if (zapisane == t.size() && rozmiarZnany) {  // brak miejsca może wyjść dopiero przy zrzucie bufora w close()
    size_t naFlash = rozmiarNaFlash(nazwa);
    if (naFlash != przed + t.size()) zapisane = naFlash == SIZE_MAX || naFlash < przed ? 0 : naFlash - przed;
  }
  if (zapisane != t.size()) {
    Serial.printf("dopisanie %s: zapisano %u z %u bajtów\n", nazwa, (unsigned)zapisane, (unsigned)t.size());
    return false;
  }
  return true;
}

bool PlikiEsp32::pierwszyWiersz(const char* nazwa, std::string& wiersz) {
  if (!montowane_) return false;
  File f = LittleFS.open(nazwa, "r");
  if (!f) return false;
  size_t rozmiar = f.size();
  String l = f.readStringUntil('\n');
  f.close();
  // readStringUntil() przy błędzie odczytu daje "" bez sygnału błędu; pusty wynik przy niepustym pliku to
  // odczyt niepewny (false), wołający potwierdza go pełnym czytaj() (Magazyn::dopisz())
  if (l.length() == 0 && rozmiar > 0) return false;
  wiersz = l.c_str();
  return true;
}

bool PlikiEsp32::usun(const char* n) { return montowane_ && LittleFS.remove(n); }
bool PlikiEsp32::zmienNazwe(const char* z, const char* na) {
  if (!montowane_) return false;
  if (LittleFS.rename(z, na)) return true;  // littlefs: rename nadpisuje cel atomowo
  if (!istniejeVfs(z) || !istniejeVfs(na)) return false;
  LittleFS.remove(na);  // zapasowo, jak zapiszAtomowo(): gdyby rename nie nadpisał celu
  return LittleFS.rename(z, na);
}
void PlikiEsp32::odmontuj() {
  LittleFS.end();
  montowane_ = false;
}

// ---------- proste operacje dla magazynu (src/magazyn.h; kontrakty w src/adaptery.h) ----------
// Plik większy niż PLIKI_MAKS_CZYTANIA nie jest czytany (czytaj() false): ten sam limit co plik przyjmowany komendą `W`
// (R8 karty) [PRÓG]; chroni stertę przed jednym wielkim plikiem.
static const size_t PLIKI_MAKS_CZYTANIA = 4u * 1024 * 1024;
static const int PLIKI_MAKS_W_KATALOGU = 1000;  // listuj(): limit obiegu pętli [PRÓG]

// Brakujące katalogi nad plikiem `nazwa` („/dane/noce/2026.csv” → „/dane”, „/dane/noce”).
static void utworzKatalogi(const char* nazwa) {
  std::string s = nazwa;
  for (size_t i = s.find('/', 1); i != std::string::npos; i = s.find('/', i + 1)) {
    std::string k = s.substr(0, i);
    if (!istniejeVfs(k.c_str())) LittleFS.mkdir(k.c_str());
  }
}

bool PlikiEsp32::czytaj(const char* nazwa, std::string& tresc) {
  tresc.clear();
  // bez wstępnego exists() (to też otwarcie pliku); od razu open. false = pliku nie ma albo odczyt się nie
  // udał; rozstrzyga istnieje() (stat()), które nie zależy od otwarcia pliku
  if (!montowane_) return false;
  File f = LittleFS.open(nazwa, "r");
  if (!f) return false;
  if (f.isDirectory()) {
    f.close();
    return false;
  }
  size_t n = f.size();
  if (n > PLIKI_MAKS_CZYTANIA) {
    Serial.printf("odczyt %s: %u B, ponad limit %u B\n", nazwa, (unsigned)n, (unsigned)PLIKI_MAKS_CZYTANIA);
    f.close();
    return false;
  }
  tresc.resize(n);
  size_t r = n ? f.read((uint8_t*)&tresc[0], n) : 0;
  f.close();
  if (r != n) {
    tresc.clear();
    return false;
  }
  return true;
}

size_t PlikiEsp32::zapisz(const char* nazwa, const std::string& tresc) {
  if (!montowane_) return 0;
  utworzKatalogi(nazwa);
  File f = LittleFS.open(nazwa, "w");
  if (!f) return 0;
  size_t n = tresc.empty() ? 0 : f.write((const uint8_t*)tresc.data(), tresc.size());
  f.close();
  if (n == tresc.size() && n) {  // wołający porównuje wynik z rozmiarem (np. `.tmp` przed rename w magazynie)
    size_t naFlash = rozmiarNaFlash(nazwa);
    if (naFlash != n) {
      Serial.printf("zapis %s: na flash %d z %u bajtów\n", nazwa, naFlash == SIZE_MAX ? -1 : (int)naFlash, (unsigned)n);
      return naFlash == SIZE_MAX ? 0 : naFlash;
    }
  }
  return n;
}

bool PlikiEsp32::listuj(const char* katalog, std::vector<std::string>& nazwy) {
  nazwy.clear();
  if (!montowane_ || !istniejeVfs(katalog)) return false;
  File d = LittleFS.open(katalog, "r");
  if (!d) return false;
  if (!d.isDirectory()) {
    d.close();
    return false;
  }
  std::string p = katalog;
  if (p.empty() || p.back() != '/') p += '/';
  for (int i = 0; i < PLIKI_MAKS_W_KATALOGU; i++) {
    File f = d.openNextFile();
    if (!f) break;
    if (!f.isDirectory()) {
      std::string n = f.name();
      size_t u = n.rfind('/');  // name() w rdzeniu 2.x i 3.x bez ścieżki; na wszelki wypadek po ostatnim ukośniku
      nazwy.push_back(p + (u == std::string::npos ? n : n.substr(u + 1)));
    }
    f.close();
  }
  d.close();
  std::sort(nazwy.begin(), nazwy.end());
  return true;
}

// ---------- partycja plików i pierwsze formatowanie (T-WYM-38, T-WYM-84; decyzja w rdzeniu: src/magazyn.h
// decyzjaPartycji()) ----------
static const char* const ETYKIETA_PLIKOW = "spiffs";  // LittleFS.h: partitionLabel = "spiffs" (spec 8.2)

static const esp_partition_t* partycjaPlikow() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, ETYKIETA_PLIKOW);
}

// Tablica, z której panel faktycznie wystartował (bootloader ładuje ją spod 0x8000): wszystkie partycje app i data
// przez esp_partition_find(), identyfikator tym samym algorytmem co skrypt budowy (src/magazyn.h
// identyfikatorUkladu()); przesunięcie i rozmiar partycji `spiffs`.
OpisPartycji PlikiEsp32::partycja() {
  OpisPartycji o;
  const esp_partition_t* p = partycjaPlikow();
  if (p) {
    o.przesuniecie = p->address;
    o.rozmiar = p->size;
  }
  std::vector<WpisPartycji> t;
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  for (int i = 0; it && i < 64; i++) {  // limit obiegu: tablica ma najwyżej kilkanaście wierszy
    const esp_partition_t* x = esp_partition_get(it);
    if (x && ((int)x->type == 0 || (int)x->type == 1)) {
      WpisPartycji w;
      w.nazwa = x->label;
      w.typ = (uint8_t)x->type;
      w.podtyp = (uint8_t)x->subtype;
      w.przesuniecie = x->address;
      w.rozmiar = x->size;
      t.push_back(w);
    }
    it = esp_partition_next(it);  // na końcu zwalnia iterator i zwraca NULL
  }
  esp_partition_iterator_release(it);  // NULL dozwolone; przy przerwaniu limitem zwalnia iterator
  o.uklad = identyfikatorUkladu(t);
  return o;
}

bool PlikiEsp32::czytajPartycje(size_t ile, std::string& bajty) {
  bajty.clear();
  const esp_partition_t* p = partycjaPlikow();
  if (!p || ile == 0 || ile > p->size || ile > 64 * 1024) return false;
  bajty.resize(ile);
  if (esp_partition_read(p, 0, &bajty[0], ile) != ESP_OK) {
    bajty.clear();
    return false;
  }
  return true;
}

// Jedyne miejsce z formatowaniem w firmware: wołane przez rdzeń tylko przy decyzji FORMATUJ (T-WYM-84).
bool PlikiEsp32::formatuj() {
  LittleFS.end();
  montowane_ = false;
  if (!LittleFS.format()) {
    Serial.println("formatowanie partycji plików: błąd");
    return false;
  }
  montowane_ = LittleFS.begin(false);  // po formatowaniu zwykłe montowanie, nadal bez formatowania przy błędzie
  return montowane_;
}

long long PlikiEsp32::wolneMiejsce() {
  if (!montowane_) return -1;
  size_t c = LittleFS.totalBytes(), u = LittleFS.usedBytes();
  return u >= c ? 0 : (long long)(c - u);
}

// ---------- Wi-Fi, HTTP, NTP, okno serwisowe ----------
static int wifiPowod = 0;

bool SiecEsp32::polacz(uint32_t limitMs, const std::function<bool()>& przerwij, bool& przerwane) {
  static bool zdarzenia = false;
  if (!zdarzenia) {
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t i) { wifiPowod = i.wifi_sta_disconnected.reason; },
                 ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    zdarzenia = true;
  }
  przerwane = false;
  if (k_.ssid.empty()) return false;  // bez zapisanej sieci (pierwsza konfiguracja)
  ap_ = false;  // tryb STA kończy sieć panelu
  WiFi.mode(WIFI_STA);
  WiFi.begin(k_.ssid.c_str(), k_.haslo.c_str());
  uint32_t t0 = (uint32_t)millis();
  while (WiFi.status() != WL_CONNECTED && (uint32_t)millis() - t0 < limitMs) {
    if (przerwij && przerwij()) {
      przerwane = true;
      return false;
    }
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool SiecEsp32::polaczona() { return WiFi.status() == WL_CONNECTED; }

void SiecEsp32::wylacz() {
  if (okno_) ArduinoOTA.end();
  // sieć panelu kończy samo WiFi.mode(WIFI_OFF) (esp_wifi_stop i deinit); bez softAPdisconnect(), które
  // przez AP.clear() na chwilę ustawia otwartą konfigurację
  if (!ap_) WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  okno_ = false;
  ap_ = false;
}

// (sieć panelu, R-KONF-07, R-KONF-08), kolejność według rdzenia 3.3.12 (pioarduino 55.03.312-1,
// WiFiGeneric.cpp mode()/espWiFiStart(), AP.cpp create()): wlaczRadio() robi pełne wyłączenie (mode OFF = deinit),
// persistent(false) przed init (konfiguracja Wi-Fi tylko w RAM, hasło AP nie trafia do flash) i STA bez łączenia
// (esp_random() ma szum radia). uruchomSiecPanelu(): radio zatrzymane esp_wifi_stop() przy zostawionej
// fladze rdzenia „started”, więc WiFi.mode(WIFI_AP) przełącza tryb i interfejs bez startu radia; konfiguracja WPA2
// przez esp_wifi_set_config() i dopiero wtedy esp_wifi_start(): radio nadaje od razu z hasłem, bez chwili otwartej
// sieci. softAP() z tą samą konfiguracją tylko czeka na start (config równy, bez ponownego ustawienia). Serwer HTTP
// rusza dopiero po adresie. DNS niepotrzebny: adres IP jest w kodzie QR.
bool SiecEsp32::wlaczRadio() {
  wylacz();
  WiFi.persistent(false);
  return WiFi.mode(WIFI_STA);
}

bool SiecEsp32::uruchomSiecPanelu(const std::string& ssid, const std::string& haslo) {
  if (ssid.empty() || ssid.size() > 32 || haslo.size() < 8 || haslo.size() > 63) return false;
  if (WiFi.getMode() != WIFI_STA && !wlaczRadio()) return false;
  ap_ = true;
  esp_wifi_stop();  // radio cicho; rdzeń dalej uważa Wi-Fi za uruchomione, więc mode() poniżej nie startuje radia
  if (!WiFi.mode(WIFI_AP)) {
    wylacz();
    return false;
  }
  wifi_config_t c;
  memset(&c, 0, sizeof c);
  memcpy(c.ap.ssid, ssid.data(), ssid.size());
  c.ap.ssid_len = (uint8_t)ssid.size();
  memcpy(c.ap.password, haslo.data(), haslo.size());
  c.ap.channel = 1;
  c.ap.max_connection = 2;
  c.ap.beacon_interval = 100;
  c.ap.authmode = WIFI_AP_DEFAULT_AUTH_MODE;  // WPA2-PSK
  c.ap.pairwise_cipher = WIFI_AP_DEFAULT_CIPHER;
  if (esp_wifi_set_config(WIFI_IF_AP, &c) != ESP_OK || esp_wifi_start() != ESP_OK) {
    Serial.println("sieć panelu: konfiguracja albo start AP nieudany");
    wylacz();
    return false;
  }
  if (!WiFi.softAP(ssid.c_str(), haslo.c_str(), 1, 0, 2)) {  // czeka na start, konfiguracja już równa
    Serial.println("sieć panelu: softAP nieudany");
    wylacz();
    return false;
  }
  uint32_t t0 = (uint32_t)millis();
  while ((uint32_t)WiFi.softAPIP() == 0 && (uint32_t)millis() - t0 < 3000) delay(50);  // adres DHCP serwera AP
  if ((uint32_t)WiFi.softAPIP() == 0) {
    wylacz();
    return false;
  }
  return true;
}

std::string SiecEsp32::adresSieciPanelu() {
  if (!ap_ || (uint32_t)WiFi.softAPIP() == 0) return "";
  return WiFi.softAPIP().toString().c_str();
}

std::string SiecEsp32::idPanelu() {
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  char b[5];
  snprintf(b, sizeof b, "%02X%02X", mac[4], mac[5]);
  return b;
}

std::string SiecEsp32::opisBledu() {
  Serial.printf("Wi-Fi: brak, status %d, powód rozłączenia %d\n", (int)WiFi.status(), wifiPowod);
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  bool widac = false;
  for (int i = 0; i < n; i++) {
    Serial.printf("  skan: '%s' kanał %d RSSI %d zabezp. %d\n", WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i),
                  (int)WiFi.encryptionType(i));
    if (std::string(WiFi.SSID(i).c_str()) == k_.ssid) widac = true;
  }
  if (widac) return "sieć widać, połączenie odrzucone (powód " + std::to_string(wifiPowod) + ")";
  return std::string("nie widzę sieci ") + k_.ssid + " na 2,4 GHz";
}

std::string SiecEsp32::opisPolaczenia() {
  return std::string(WiFi.localIP().toString().c_str()) + " · RSSI " + std::to_string(WiFi.RSSI()) + " dBm";
}

// Czeka na odpowiedź SNTP w tym wywołaniu, nie tylko na „zegar ustawiony”: getLocalTime() wraca od razu, gdy zegar
// jest już ustawiony z poprzedniego wybudzenia, a zegar RC w uśpieniu dryfuje ok. 1% (pomiar 12). Status synchronizacji zerowany przed startem klienta, potem odpytywany co 20 ms do limitMs.
bool SiecEsp32::czasZSieci(uint32_t limitMs) {
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  configTzTime(STREFA, "pool.ntp.org", "time.google.com");  // ADR-0013
  uint32_t t0 = (uint32_t)millis();
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    if ((uint32_t)millis() - t0 >= limitMs) return false;
    delay(20);
  }
  return true;
}

// Ciało odpowiedzi prosto do std::string z limitem rozmiaru, końcem czasu i przerwaniem: po przekroczeniu
// write() zwraca 0, co kończy writeToStream() błędem zapisu. Koniec czasu: budzetCialaMs od `startMs` (millis() przy
// wejściu w get()), oba na zegarze adaptera; bez czasu rdzenia (Zegar::ms() po czekaniu na kablu ma inną podstawę,
// ADR-0047, zmiana 2026-11-22).
struct DoNapisu : Stream {
  std::string& s;
  const ZadanieHttp& z;
  const uint32_t startMs;
  bool zaDuzo = false, zaDlugo = false, przerwane = false;
  DoNapisu(std::string& t, const ZadanieHttp& zad, uint32_t start) : s(t), z(zad), startMs(start) {}
  bool stop() {
    if (budzetCialaMinal(z.budzetCialaMs, startMs, (uint32_t)millis())) zaDlugo = true;
    if (!zaDlugo && z.przerwij && z.przerwij()) przerwane = true;
    return zaDlugo || przerwane;
  }
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* b, size_t n) override {
    if ((z.budzetCialaMs || z.przerwij) && stop()) return 0;
    if (z.maksBajtow && s.size() + n > z.maksBajtow) {
      zaDuzo = true;
      return 0;
    }
    s.append((const char*)b, n);
    return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};

OdpowiedzHttp SiecEsp32::get(const ZadanieHttp& z) {
  OdpowiedzHttp o;
  HTTPClient http;
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);  // R-NF-27: 3xx to odpowiedź, bez podążania (R-AW-25)
  http.setTimeout(z.limitMs);
  http.setConnectTimeout((int32_t)z.limitMs);
  uint32_t t0 = (uint32_t)millis();
  if (!http.begin(String(z.url.c_str()))) {
    o.kod = -1;
    return o;
  }
  const char* naglowki[] = {"WWW-Authenticate"};
  http.collectHeaders(naglowki, 1);
  for (auto& n : z.naglowki) http.addHeader(n.first.c_str(), n.second.c_str());
  o.kod = http.GET();
  if (o.kod == 401) {
    o.wwwAuthenticate = http.header("WWW-Authenticate").c_str();
  } else if (o.kod > 0 && (!z.cialoTylko200 || o.kod == 200)) {
    o.contentLength = http.getSize();  // -1 = brak (wtedy kompletność sprawdza parser JSON)
    if (z.maksBajtow && o.contentLength > (int)z.maksBajtow) {
      o.zaDuzo = true;  // bez czytania ciała
    } else {
      if (o.contentLength > 0) o.cialo.reserve((size_t)o.contentLength);
      DoNapisu cel(o.cialo, z, t0);
      o.wynikZapisu = http.writeToStream(&cel);
      o.zaDuzo = cel.zaDuzo;
      o.zaDlugo = cel.zaDlugo;
      o.przerwane = cel.przerwane;
    }
  }
  http.end();
  o.ms = (uint32_t)millis() - t0;
  return o;
}

bool SiecEsp32::otworzOknoSerwisowe() {
  if (!k_.hasloOkna || okno_ || WiFi.status() != WL_CONNECTED) return okno_;
  // puste albo krótkie hasło zna albo zgadnie każdy w sieci domowej: okno się nie otwiera (main.cpp sprawdza to
  // też przy kompilacji, tu zostaje zabezpieczenie dla hasła podanego inną drogą)
  if (strlen(k_.hasloOkna) < HASLO_OKNA_MIN) {
    Serial.printf("OTA: okno wgrywania wyłączone: słabe hasło (krótsze niż %u znaków)\n", (unsigned)HASLO_OKNA_MIN);
    return false;
  }
  MDNS.end();  // ArduinoOTA uruchamia mDNS od nowa z nazwą cpap-eink i usługą _arduino._tcp
  ArduinoOTA.setHostname(OTA_HOST);
  ArduinoOTA.setPassword(k_.hasloOkna);
  ArduinoOTA.setRebootOnSuccess(true);
  PlikiEsp32* pliki = &pliki_;
  ArduinoOTA.onStart([pliki]() {
    bool fs = ArduinoOTA.getCommand() == U_SPIFFS;
    if (fs) pliki->odmontuj();  // obraz pamięci plików nadpisuje historię, czynniki i dziennik
    Serial.printf("OTA: start (%s)\n", fs ? "pamięć plików" : "firmware");
  });
  // wgrywanie blokuje pętlę czuwania (ArduinoOTA.handle()): strażnik odnawiany przy każdym kawałku danych (T-WYM-81)
  ArduinoOTA.onProgress([](unsigned int, unsigned int) { esp_task_wdt_reset(); });
  ArduinoOTA.onEnd([]() { Serial.println("OTA: koniec, restart"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA: błąd %d\n", (int)e); });
  ArduinoOTA.begin();
  okno_ = true;
  Serial.printf("OTA: nasłuch %s.local (%s)\n", OTA_HOST, WiFi.localIP().toString().c_str());
  return true;
}

void SiecEsp32::obsluzOknoSerwisowe() {
  if (okno_) ArduinoOTA.handle();  // wgrywanie blokuje tu do końca, potem restart
}

uint32_t SiecEsp32::losowa() { return esp_random(); }

std::string SiecEsp32::adresIp() {
  if (WiFi.status() != WL_CONNECTED) return "";
  return WiFi.localIP().toString().c_str();
}

std::string SiecEsp32::nazwaSieci() {
  if (WiFi.status() != WL_CONNECTED) return "";
  return WiFi.SSID().c_str();
}

int SiecEsp32::rssi() { return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0; }

static std::string md5Hex(const std::string& t) {
  unsigned char w[16];
  mbedtls_md5((const unsigned char*)t.data(), t.size(), w);
  char h[33];
  for (int i = 0; i < 16; i++) snprintf(h + 2 * i, 3, "%02x", w[i]);
  return h;
}
Md5Hex SiecEsp32::md5() { return md5Hex; }

// ---------- pomiar czasu zmiany ekranu (tylko obraz `e1001_pomiar`, -DPOMIAR_CZASU=1) ----------
// Wiersze `POMIAR …` na porcie szeregowym przy każdym rysowaniu: ile minęło od startu programu, od pierwszego
// wciśnięcia i od ostatniego puszczenia przycisku po poprzednim rysowaniu (czas liczenia strony), ile trwało włączenie
// sterownika ekranu, przesłanie obrazu z odświeżeniem i uśpienie sterownika. Zwykłe obrazy są bez tego kodu.
#if POMIAR_CZASU
namespace pomiar {
volatile int64_t pierwszeWcisniecieUs = 0;  // pierwsze wciśnięcie po poprzednim rysowaniu (0 = nie było)
volatile int64_t ostatniePuszczenieUs = 0;  // ostatnie puszczenie po poprzednim rysowaniu (0 = nie było)
uint32_t wlaczenieMs = 0;                   // czas ostatniego WyswietlaczEsp32::wlacz() (0 = sterownik już włączony)
uint32_t numer = 0;                         // numer rysowania w tym wybudzeniu
// Historia wierszy w pamięci NVS (przestrzeń `pomiar`): pomiar nie zależy od tego, czy kabel był podłączony w chwili
// rysowania (mostek USB potrafi zniknąć z komputera, 16.11.2026). Najstarsze wiersze wypadają powyżej HISTORIA_MAX.
// Napis NVS musi zmieścić się w jednej stronie (126 wpisów po 32 B): 1500 znaków + znak końca = 1 wpis nagłówka
// + 47 wpisów danych = 48 wpisów (przy wierszu POMIAR ok. 320 znaków ok. 4 ostatnie rysowania). Wcześniej
// 3500 znaków = 111 wpisów, czyli strona z najwyżej 15 innymi wpisami, i `nvs_set_str fail: h NOT_ENOUGH_SPACE`
// na panelu 22.11.2026.
const size_t HISTORIA_MAX = 1500;
bool bladZapisuZgloszony = false;  // wiersz o nieudanym zapisie raz na start programu, bez zalewania portu
void historiaDopisz(const char* wiersz) {
  Preferences p;
  if (!p.begin("pomiar", false)) return;
  String h = p.getString("h", "");
  h += wiersz;
  h += '\n';
  while (h.length() > HISTORIA_MAX) {
    int i = h.indexOf('\n');
    if (i < 0) { h = ""; break; }
    h.remove(0, i + 1);
  }
  // putString() zwraca liczbę zapisanych znaków, 0 przy błędzie (pusty napis tylko po wierszu dłuższym niż limit)
  if (p.putString("h", h) == 0 && h.length() > 0 && !bladZapisuZgloszony) {
    bladZapisuZgloszony = true;
    Serial.printf("POMIAR historia: zapis do NVS nieudany (%u znakow), wiersze tylko na porcie\n", (unsigned)h.length());
  }
  p.end();
}
// Punkty faz liczenia przed rysowaniem (T-WYM-99, haczyk szybkosc::faza() z rdzenia). Wewnątrz mierzonego odcinka
// tylko dopisanie do bufora w RAM (bez portu szeregowego i bez zapisu flash); bufor trafia jako pole `fazy=` do
// głównego wiersza `POMIAR` po rysowaniu, razem z nim na port i jednym zapisem do historii NVS. Postać skrócona:
// `{litera}{trwało ms}` po spacji (h historia, p przycisk, r reguly, s sprawy, ? inna), od poprzedniego punktu albo
// końca poprzedniego rysowania; nadmiar ponad bufor znaczony `+` (główne wiersze nie są wypychane z historii).
uint32_t ostatniPunktMs = 0;
char fazy[96] = "";
size_t fazyDl = 0;
void punktFazy(const char* nazwa) {
  uint32_t t = (uint32_t)millis();
  char lit = nazwa[0] == 'h' || nazwa[0] == 'p' || nazwa[0] == 'r' || nazwa[0] == 's' ? nazwa[0] : '?';
  char kawalek[16];
  int n = snprintf(kawalek, sizeof kawalek, "%s%c%lu", fazyDl ? " " : "", lit, (unsigned long)(t - ostatniPunktMs));
  ostatniPunktMs = t;
  if (n > 0 && fazyDl + (size_t)n + 2 < sizeof fazy) {
    memcpy(fazy + fazyDl, kawalek, (size_t)n);
    fazyDl += (size_t)n;
    fazy[fazyDl] = 0;
  } else if (fazyDl + 1 < sizeof fazy && (fazyDl == 0 || fazy[fazyDl - 1] != '+')) {
    fazy[fazyDl++] = '+';
    fazy[fazyDl] = 0;
  }
}
// Haczyk ustawiany przy starcie programu (inicjalizacja statyczna, przed setup()); tylko w obrazie pomiarowym.
struct UstawHaczykFazy {
  UstawHaczykFazy() { szybkosc::haczykFazy = punktFazy; }
} ustawHaczykFazy;
}  // namespace pomiar

// Historia na port szeregowy przy każdym starcie programu; po resecie z komputera (linia EN, `rst:0x1`) historia jest
// po wypisaniu kasowana, więc kolejny pomiar zaczyna od pustej.
void pomiarHistoria() {
  Preferences p;
  if (!p.begin("pomiar", false)) return;
  String h = p.getString("h", "");
  Serial.println("POMIAR-HISTORIA-OD");
  Serial.print(h);
  Serial.println("POMIAR-HISTORIA-DO");
  if (esp_reset_reason() == ESP_RST_POWERON) p.remove("h");
  p.end();
}
#endif

// ---------- ekran e-ink ----------
static SPIClass hspi(HSPI);
#if EPD_WYSYLKA_BLOKIEM
// Sterownik GDEY075T7 (UC8179) z wysyłką obrazu CAŁEJ powierzchni 800×480 jednym blokiem SPI (`SPIClass::writeBytes`,
// kawałki po wysylka_obrazu::KAWALEK_B w jednej transakcji z CS niskim) zamiast bajt po bajcie (`_transfer(bajt)`
// w GxEPD2). Komendy i dane DOKŁADNIE jak w GxEPD2 1.6.9 (platformio.ini: `zinggjm/GxEPD2 @ 1.6.9`),
// src/gdey/GxEPD2_750_GDEY075T7.cpp, dla x = 0, y = 0, w = 800, h = 480, invert = true, mirror_y = false, pgm = false:
// `_writeImage()` (z `writeScreenBuffer()` przy `_initial_write`), `_Init_Part()`, `_InitDisplay()`, `_PowerOn()`,
// `_setPartialRamArea()`. Te funkcje są w bibliotece prywatne (nie da się ich wywołać z klasy pochodnej), więc są tu
// przepisane 1:1 na chronionych `_writeCommand`, `_writeData`, `_startTransfer`, `_endTransfer`, `_reset`,
// `_waitWhileBusy` i polach stanu `_initial_write`, `_initial_refresh`, `_using_partial_mode`, `_power_is_on`,
// `_hibernating`. Odświeżenie, wyłączenie zasilania i uśpienie zostają w bibliotece (`refresh()`, `powerOff()`,
// `hibernate()` są publiczne). Zmiana wersji GxEPD2 = przegląd tej klasy (stałe pilnuje static_assert niżej).
class SterownikBlokiem : public GxEPD2_750_GDEY075T7 {
 public:
  SterownikBlokiem(int16_t cs, int16_t dc, int16_t rst, int16_t busy) : GxEPD2_750_GDEY075T7(cs, dc, rst, busy) {}

  // ≡ GxEPD2 `writeImageToPrevious(obraz, 0, 0, 800, 480, true, false, false)` (komenda 0x10) albo
  // `writeImage(…)` / `writeImageForFullRefresh(…)` z tymi samymi argumentami (0x13). `obraz`: 48 000 B, 1 = czarny.
  void obrazCalyOdwrocony(uint8_t komenda, const uint8_t* obraz) {
    if (_initial_write) czyscPamiec();  // = writeScreenBuffer(), `_writeImage()` l. 1
    delay(1);                           // jak biblioteka (yield przed strażnikiem zadania)
    if (!_using_partial_mode) initPart();
    _writeCommand(0x91);  // partial in
    obszarCaly();
    _writeCommand(komenda);
#if POMIAR_CZASU
    const int64_t t0 = esp_timer_get_time();
#endif
    _startTransfer();
    wysylka_obrazu::wyslijOdwrocone(obraz, BAJTOW, kawalek, sizeof kawalek, piszSpi, _pSPIx);
    _endTransfer();
#if POMIAR_CZASU
    pomDaneUs += esp_timer_get_time() - t0;
#endif
    _writeCommand(0x92);  // partial out
    delay(1);
  }

#if POMIAR_CZASU
  // Obraz pomiarowy: czasy w µs od pomiarZeruj() (pola `e_…` wiersza POMIAR, WyswietlaczEsp32::odswiezJako()).
  // pomDaneUs: same dane obrazu (od `_startTransfer()` do `_endTransfer()`, bez komend 0x91/0x90/komenda/0x92);
  // pomInitUs: initPart() w całości (reset przy `_hibernating`, `_InitDisplay`, E0/E5, `_PowerOn` z czekaniem na
  // BUSY); pomCzyscUs: czyszczenie pamięci sterownika (dwie komendy z danymi 48 000 B, bez initPart()).
  int64_t pomDaneUs = 0, pomInitUs = 0, pomCzyscUs = 0;
  void pomiarZeruj() { pomDaneUs = pomInitUs = pomCzyscUs = 0; }
#endif

 private:
  static constexpr size_t BAJTOW = size_t(WIDTH) * size_t(HEIGHT) / 8;
  // kawałek roboczy w pamięci wewnętrznej (512 B, nie 48 KB); wyrównany do 4 B, bo `spiWriteNL()` czyta słowami
  alignas(4) static uint8_t kawalek[wysylka_obrazu::KAWALEK_B];

  static void piszSpi(void* spi, const uint8_t* dane, size_t dl) {
    static_cast<SPIClass*>(spi)->writeBytes(dane, static_cast<uint32_t>(dl));
  }
  // ≡ GxEPD2 `_writeScreenBuffer(komenda, wartosc)`
  void pamiecWartoscia(uint8_t komenda, uint8_t wartosc) {
#if POMIAR_CZASU
    const int64_t t0 = esp_timer_get_time();
#endif
    _writeCommand(komenda);
    _startTransfer();
    wysylka_obrazu::wyslijWartosc(wartosc, BAJTOW, kawalek, sizeof kawalek, piszSpi, _pSPIx);
    _endTransfer();
#if POMIAR_CZASU
    pomCzyscUs += esp_timer_get_time() - t0;
#endif
  }
  // ≡ GxEPD2 `writeScreenBuffer(0xFF)`: pierwszy zapis po `init()` czyści pamięć „nowego” i (przy pierwszym
  // odświeżeniu) „poprzedniego” obrazu sterownika
  void czyscPamiec() {
    if (!_using_partial_mode) initPart();
    pamiecWartoscia(0x13, 0xFF);                         // set current
    if (_initial_refresh) pamiecWartoscia(0x10, 0x00);   // set previous
    _initial_write = false;
  }
  // ≡ GxEPD2 `_setPartialRamArea(0, 0, 800, 480)`
  void obszarCaly() {
    const uint16_t x = 0, y = 0, xe = uint16_t((x + WIDTH - 1) | 0x0007), ye = uint16_t(y + HEIGHT - 1);
    _writeCommand(0x90);  // partial window
    _writeData(uint8_t(x / 256));
    _writeData(uint8_t(x % 256));
    _writeData(uint8_t(xe / 256));
    _writeData(uint8_t(xe % 256));
    _writeData(uint8_t(y / 256));
    _writeData(uint8_t(y % 256));
    _writeData(uint8_t(ye / 256));
    _writeData(uint8_t(ye % 256));
    _writeData(0x01);
  }
  // ≡ GxEPD2 `_PowerOn()`
  void powerOn() {
    if (!_power_is_on) {
      _writeCommand(0x04);
      _waitWhileBusy("_PowerOn", power_on_time);
    }
    _power_is_on = true;
  }
  // ≡ GxEPD2 `_InitDisplay()`
  void initDisplay() {
    if (_hibernating) _reset();
    _writeCommand(0x00);  // PANEL SETTING
    _writeData(0x1f);
    _writeCommand(0x01);  // POWER SETTING
    _writeData(0x07);
    _writeData(0x07);
    _writeData(0x3f);
    _writeData(0x3f);
    _writeData(0x09);
    _writeCommand(0x06);  // Booster Soft Start
    _writeData(0x17);
    _writeData(0x17);
    _writeData(0x28);
    _writeData(0x17);
    _writeCommand(0x61);  // tres
    _writeData(uint8_t(WIDTH / 256));
    _writeData(uint8_t(WIDTH % 256));
    _writeData(uint8_t(HEIGHT / 256));
    _writeData(uint8_t(HEIGHT % 256));
    _writeCommand(0x15);  // DUSPI
    _writeData(0x00);
    _writeCommand(0x50);  // VCOM AND DATA INTERVAL SETTING
    _writeData(0x29);
    _writeData(0x07);
    _writeCommand(0x60);  // TCON SETTING
    _writeData(0x22);
    _writeCommand(0xE3);  // PWS
    _writeData(0x22);
  }
  // ≡ GxEPD2 `_Init_Part()` w gałęzi `hasFastPartialUpdate && useFastPartialUpdateFromOTP` (jedynej w 1.6.9 dla
  // tego panelu, static_assert niżej)
  void initPart() {
#if POMIAR_CZASU
    const int64_t t0 = esp_timer_get_time();
#endif
    initDisplay();
    _writeCommand(0xE0);  // Cascade Setting (CCSET)
    _writeData(0x02);     // TSFIX
    _writeCommand(0xE5);  // Force Temperature (TSSET)
    _writeData(0x6E);     // 110
    powerOn();
    _using_partial_mode = true;
#if POMIAR_CZASU
    pomInitUs += esp_timer_get_time() - t0;
#endif
  }
};
alignas(4) uint8_t SterownikBlokiem::kawalek[wysylka_obrazu::KAWALEK_B];
// stałe GxEPD2 1.6.9, na których opiera się przepisana sekwencja (inna wersja biblioteki = przegląd klasy)
static_assert(SterownikBlokiem::WIDTH == 800 && SterownikBlokiem::HEIGHT == 480, "GxEPD2: inny rozmiar panelu");
static_assert(SterownikBlokiem::hasFastPartialUpdate && SterownikBlokiem::useFastPartialUpdateFromOTP,
              "GxEPD2: inna gałąź _Init_Part()");
static_assert(SterownikBlokiem::power_on_time == 140, "GxEPD2: inny czas _PowerOn()");
static_assert(size_t(SterownikBlokiem::WIDTH) * SterownikBlokiem::HEIGHT / 8 == BAJTOW_KLATKI, "rozmiar klatki");
using SterownikEkranu = SterownikBlokiem;
#else
using SterownikEkranu = GxEPD2_750_GDEY075T7;  // dotychczasowa droga przez GxEPD2 bajt po bajcie
#endif
#if EPD_WYSYLKA_BLOKIEM
// Od 22.11.2026: przy wysyłce blokiem bufor strony GxEPD2_BW (48 000 B w pamięci wewnętrznej)
// nie był do niczego używany, więc zamiast GxEPD2_BW sam sterownik. Te same wywołania sterownika co w GxEPD2_BW 1.6.9:
// `init(uint32_t)` = `epd2.init(serial_diag_bitrate)` (reszta to stan okna i strony GxEPD2_BW, w tej drodze
// nieużywany), `hibernate()` = `epd2.hibernate()`; miejsca wywołań (`epd.init`, `epd.epd2.…`, `epd.hibernate`) bez zmian.
struct EkranBezBufora {
  SterownikEkranu epd2;
  void init(uint32_t serial_diag_bitrate) { epd2.init(serial_diag_bitrate); }
  void hibernate() { epd2.hibernate(); }
};
static EkranBezBufora epd{SterownikEkranu(EPD_CS, EPD_DC, EPD_RES, EPD_BUSY)};
#else
static GxEPD2_BW<SterownikEkranu, SterownikEkranu::HEIGHT> epd(SterownikEkranu(EPD_CS, EPD_DC, EPD_RES, EPD_BUSY));
#endif
static GFXcanvas1 kanwa(800, 480);  // 1 = czarny

struct PlotnoGFX : Plotno {
  void piksel(int x, int y, bool c) override { kanwa.drawPixel((int16_t)x, (int16_t)y, c ? 1 : 0); }
  void prostokat(int x, int y, int w, int h, bool c) override {
    kanwa.fillRect((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, c ? 1 : 0);
  }
};
static PlotnoGFX plotnoGfx;

#if ZRZUT_EKRANU
static void zrzut() {  // bufor 800x480 1 bit w base64 po PackBits, do podglądu na komputerze
  static const char* B = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const uint8_t* src = kanwa.getBuffer();
  const size_t ns = 800 / 8 * 480;
  static std::vector<uint8_t> pk;
  pk.clear();
  pk.reserve(ns / 4);
  for (size_t i = 0; i < ns;) {
    size_t r = 1;
    while (i + r < ns && r < 128 && src[i + r] == src[i]) r++;
    if (r >= 2) { pk.push_back((uint8_t)(257 - r)); pk.push_back(src[i]); i += r; continue; }
    size_t j = i, l = 0;
    while (j < ns && l < 128 && !(j + 1 < ns && src[j + 1] == src[j])) { j++; l++; }
    if (l == 0) { l = 1; j = i + 1; }
    pk.push_back((uint8_t)(l - 1));
    for (size_t q = i; q < j; q++) pk.push_back(src[q]);
    i = j;
  }
  const uint8_t* b = pk.data();
  const size_t n = pk.size();
  Serial.println("ZRZUT_START 800x480 packbits");
  char linia[77];
  int k = 0;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)b[i] << 16 | (i + 1 < n ? b[i + 1] << 8 : 0) | (i + 2 < n ? b[i + 2] : 0);
    linia[k++] = B[(v >> 18) & 63];
    linia[k++] = B[(v >> 12) & 63];
    linia[k++] = i + 1 < n ? B[(v >> 6) & 63] : '=';
    linia[k++] = i + 2 < n ? B[v & 63] : '=';
    if (k == 76) { linia[k] = 0; Serial.println(linia); k = 0; }
  }
  if (k) { linia[k] = 0; Serial.println(linia); }
  Serial.println("ZRZUT_KONIEC");
}
#endif

// ADR-0047: sterownik inicjowany raz na start programu. Kolejny przebieg po czekaniu na kablu nie woła epd.init(),
// bo GxEPD2 ustawia wtedy `_initial_refresh` i każde częściowe zamienia na pełne; ze stanu hibernacji sterownik
// budzi się sam resetem przy następnym zapisie (`_InitDisplay()`).
static bool sterownikGotowy = false;
void WyswietlaczEsp32::wlacz() {
  if (sterownikGotowy) {
#if POMIAR_CZASU
    pomiar::wlaczenieMs = 0;
#endif
    return;
  }
  sterownikGotowy = true;
#if POMIAR_CZASU
  uint32_t p0 = (uint32_t)millis();
#endif
  hspi.begin(EPD_SCK, -1, EPD_MOSI, -1);
  // Zegar SPI ekranu EPD_SPI_HZ: od 22.11.2026 8 MHz (do 22.11.2026 2 MHz; częściowe 1,66 → 1,33 s na panelu,
  // obraz pomiarowy 22.11.2026), cofnięcie flagą budowy -DEPD_SPI_HZ=2000000.
  epd.epd2.selectSPI(hspi, SPISettings(EPD_SPI_HZ, MSBFIRST, SPI_MODE0));
  epd.init(0);
#if POMIAR_CZASU
  pomiar::wlaczenieMs = (uint32_t)millis() - p0;
#endif
}

Plotno& WyswietlaczEsp32::plotno() { return plotnoGfx; }

const uint8_t* WyswietlaczEsp32::bufor() { return kanwa.getBuffer(); }  // GFXcanvas1 800×480: 48 000 B

// R-NF-25 (src/odswiezanie.h): kopia obrazu ostatnio wysłanego do sterownika (bit 1 = czarny, jak kanwa), potrzebna
// do odświeżenia częściowego. Sterownik UC8179 porównuje obraz poprzedni (komenda 0x10) z nowym (0x13); po
// `hibernate()` budzi się resetem (GxEPD2 `_InitDisplay()`), więc na jego pamięci nie polegamy: przed każdym
// częściowym wysyłamy obraz poprzedni z tej kopii, a dopiero potem nowy (2 × 48 000 B po SPI, ok. 0,19 s więcej
// [szacunek]). Kopia żyje tylko do głębokiego snu (zmienne zwykłej pamięci): pierwsze odświeżenie po wybudzeniu
// i po starcie zawsze pełne. Brak PSRAM na kopię: zawsze pełne, wpis w logu (bez zapasu w pamięci
// wewnętrznej).
static uint8_t* poprzedniObraz = nullptr;
static bool poprzedniZnany = false;

static void zapamietajObraz() {
  if (!poprzedniObraz) {
    // tylko PSRAM: 48 000 B z pamięci wewnętrznej odebrałoby stertę Wi-Fi i TLS
    poprzedniObraz = (uint8_t*)heap_caps_malloc(BAJTOW_KLATKI, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!poprzedniObraz) {
      Serial.println("e-ink: brak PSRAM na kopię obrazu, odświeżenie zawsze pełne (R-NF-25)");
      poprzedniZnany = false;
      return;
    }
  }
  memcpy(poprzedniObraz, kanwa.getBuffer(), BAJTOW_KLATKI);
  poprzedniZnany = true;
}

void WyswietlaczEsp32::odswiez() { (void)odswiezJako(Odswiezenie::PELNE); }

// ADR-0047: kopia obrazu z tego startu programu (po czekaniu na kablu przeżywa przebieg; po głębokim śnie nie)
bool WyswietlaczEsp32::poprzedniObrazZnany() { return sterownikGotowy && poprzedniZnany && poprzedniObraz != nullptr; }

#if POMIAR_CZASU
// Pole `powod` wiersza POMIAR: po czekaniu na kablu `przycisk-czekanie` albo `zegar-czekanie` (ADR-0047).
static const char* powodPomiaru() {
  if (czekanieEsp32::poCzekaniu)
    return czekanieEsp32::powod == PowodWybudzenia::PRZYCISK ? "przycisk-czekanie" : "zegar-czekanie";
  esp_sleep_wakeup_cause_t pw = esp_sleep_get_wakeup_cause();
  return pw == ESP_SLEEP_WAKEUP_EXT1 ? "przycisk" : pw == ESP_SLEEP_WAKEUP_TIMER ? "zegar" : "start";
}
#endif

Odswiezenie WyswietlaczEsp32::odswiezJako(Odswiezenie rodzaj) {
#if POMIAR_CZASU
  int64_t pUs = esp_timer_get_time();
  uint32_t p0 = (uint32_t)millis();
#endif
  // częściowe: sterownik z szybkim przebiegiem (GxEPD2_750_GDEY075T7::hasFastPartialUpdate) i znany poprzedni obraz
  const bool czesciowe = rodzaj == Odswiezenie::CZESCIOWE && GxEPD2_750_GDEY075T7::hasFastPartialUpdate &&
                         poprzedniZnany && poprzedniObraz != nullptr;
  if (rodzaj == Odswiezenie::CZESCIOWE && !czesciowe)
    Serial.println("e-ink: odświeżenie częściowe niemożliwe (sterownik albo brak poprzedniego obrazu), pełne");
#if POMIAR_CZASU
  // Pola `e_…` wiersza POMIAR (rozpisanie `obraz_ms`, 22.11.2026), w µs; -1 = nie dotyczy albo nie do zmierzenia
  // w tej drodze (opis pól: firmware/prototyp/README.md, „Pomiar czasu zmiany ekranu”).
  int64_t ePop = -1, eNow = -1, eIni = -1, eCzy = -1, eRys = -1, eOds = -1, eWyl = -1;
#if EPD_WYSYLKA_BLOKIEM
  epd.epd2.pomiarZeruj();
#endif
#endif
  if (czesciowe) {
    // cała powierzchnia szybkim przebiegiem bez błysku (GxEPD2: _Init_Part() z tablicą z OTP, `refresh(x, y, w, h)`);
    // invert: kanwa ma 1 = czarny, sterownik 1 = biały
#if EPD_WYSYLKA_BLOKIEM
    epd.epd2.obrazCalyOdwrocony(0x10, poprzedniObraz);     // ≡ writeImageToPrevious(…, true, false, false)
#if POMIAR_CZASU
    ePop = epd.epd2.pomDaneUs;
#endif
    epd.epd2.obrazCalyOdwrocony(0x13, kanwa.getBuffer());  // ≡ writeImage(…, true, false, false)
#if POMIAR_CZASU
    eNow = epd.epd2.pomDaneUs - ePop;
#endif
#else
#if POMIAR_CZASU
    // droga przez bibliotekę: całe wywołania (z inicjalizacją, czyszczeniem pamięci i komendami w środku)
    const int64_t w0 = esp_timer_get_time();
#endif
    epd.epd2.writeImageToPrevious(poprzedniObraz, 0, 0, 800, 480, true, false, false);
#if POMIAR_CZASU
    const int64_t w1 = esp_timer_get_time();
    ePop = w1 - w0;
#endif
    epd.epd2.writeImage(kanwa.getBuffer(), 0, 0, 800, 480, true, false, false);
#if POMIAR_CZASU
    eNow = esp_timer_get_time() - w1;
#endif
#endif
#if POMIAR_CZASU
    const int64_t r0 = esp_timer_get_time();
#endif
    epd.epd2.refresh(0, 0, 800, 480);
#if POMIAR_CZASU
    eOds = esp_timer_get_time() - r0;
#endif
  } else {
#if EPD_WYSYLKA_BLOKIEM
    // ≡ stronicowanie GxEPD2_BW przy jednej stronie (`_pages == 1`, pełne okno, `GxEPD2_BW::nextPage()`):
    // `writeImageForFullRefresh(bufor biblioteki)`, `refresh(false)`, `writeImageAgain()` (u tego sterownika pusta:
    // N2OCP, sterownik sam kopiuje nowy obraz do poprzedniego), `powerOff()`. Bufor biblioteki po
    // `fillScreen(GxEPD_WHITE)` i `drawBitmap(kanwa, GxEPD_BLACK)` to bajt w bajt `~kanwa`, więc wysyłamy kanwę
    // z odwróceniem bez przepisywania 384 000 pikseli do drugiego bufora (test/testy_wysylka_obrazu.cpp).
    epd.epd2.obrazCalyOdwrocony(0x13, kanwa.getBuffer());
#if POMIAR_CZASU
    eNow = epd.epd2.pomDaneUs;
    const int64_t r0 = esp_timer_get_time();
#endif
    epd.epd2.refresh(false);
#if POMIAR_CZASU
    const int64_t r1 = esp_timer_get_time();
    eOds = r1 - r0;
#endif
    epd.epd2.powerOff();
#if POMIAR_CZASU
    eWyl = esp_timer_get_time() - r1;
#endif
#else
#if POMIAR_CZASU
    const int64_t d0 = esp_timer_get_time();
#endif
    epd.setFullWindow();
    epd.firstPage();
    do {
      epd.fillScreen(GxEPD_WHITE);
      epd.drawBitmap(0, 0, kanwa.getBuffer(), 800, 480, GxEPD_BLACK);
#if POMIAR_CZASU
      // jedna strona: pętla przechodzi raz; nextPage() (wysyłka, odświeżenie, powerOff) zostaje w `e_r`
      eRys = esp_timer_get_time() - d0;
#endif
    } while (epd.nextPage());
#endif
  }
#if POMIAR_CZASU && EPD_WYSYLKA_BLOKIEM
  eIni = epd.epd2.pomInitUs;
  eCzy = epd.epd2.pomCzyscUs;
#endif
#if POMIAR_CZASU
  uint32_t p1 = (uint32_t)millis();
#endif
  epd.hibernate();
  zapamietajObraz();
#if POMIAR_CZASU
  {
    uint32_t p2 = (uint32_t)millis();
    int64_t wc = pomiar::pierwszeWcisniecieUs, pu = pomiar::ostatniePuszczenieUs;
    pomiar::pierwszeWcisniecieUs = 0;
    pomiar::ostatniePuszczenieUs = 0;
    const uint32_t od = czekanieEsp32::poczatekMs;  // ADR-0047: od końca czekania na kablu (0 = od startu programu)
    char godz[12] = "--:--:--";
    time_t tt = time(nullptr);
    struct tm tl;
    if (localtime_r(&tt, &tl)) strftime(godz, sizeof godz, "%H:%M:%S", &tl);
    // Rozpisanie `obraz_ms` (22.11.2026): pola `e_…` w ms (część całkowita z µs), -1 = nie dotyczy albo nie do
    // zmierzenia w tej drodze; `e_r` = obraz_ms − suma pól ≥ 0, więc obraz_ms = e_pop + e_now + e_ini + e_czy + e_rys
    // + e_ods + e_wyl + e_r (pola -1 pominięte) z definicji; w `e_r` zostają komendy bez danych, delay(1) biblioteki,
    // zaokrąglenia (±1 ms na pole) i wszystko, czego dana droga nie rozpisuje.
    auto msPola = [](int64_t us) { return us < 0 ? -1L : (long)(us / 1000); };
    const long ePopMs = msPola(ePop), eNowMs = msPola(eNow), eIniMs = msPola(eIni), eCzyMs = msPola(eCzy),
               eRysMs = msPola(eRys), eOdsMs = msPola(eOds), eWylMs = msPola(eWyl);
    long eSuma = 0;
    for (long v : {ePopMs, eNowMs, eIniMs, eCzyMs, eRysMs, eOdsMs, eWylMs})
      if (v > 0) eSuma += v;
    const long eRMs = (long)(p1 - p0) - eSuma;
    // 512 B: najdłuższy wiersz ok. 470 znaków (pola stałe ok. 300 z liczbami, `fazy` do 95, pola `e_…` do ok. 90)
    char wiersz[512];
    snprintf(wiersz, sizeof wiersz,
                  "POMIAR %s nr=%lu powod=%s rodzaj=%s od_startu_programu_ms=%lu od_wcisniecia_ms=%ld "
                  "od_puszczenia_ms=%ld wlaczenie_ekranu_ms=%lu obraz_ms=%lu uspienie_ekranu_ms=%lu "
                  "razem_do_konca_ms=%lu e_pop=%ld e_now=%ld e_ini=%ld e_czy=%ld e_rys=%ld e_ods=%ld e_wyl=%ld "
                  "e_r=%ld fazy=%s",
                  godz, (unsigned long)++pomiar::numer, powodPomiaru(),
                  odswiezanie::nazwa(czesciowe ? Odswiezenie::CZESCIOWE : Odswiezenie::PELNE), (unsigned long)(p0 - od),
                  wc ? (long)((pUs - wc) / 1000) : -1L, pu ? (long)((pUs - pu) / 1000) : -1L,
                  (unsigned long)pomiar::wlaczenieMs, (unsigned long)(p1 - p0), (unsigned long)(p2 - p1),
                  (unsigned long)(p2 - od), ePopMs, eNowMs, eIniMs, eCzyMs, eRysMs, eOdsMs, eWylMs, eRMs,
                  pomiar::fazyDl ? pomiar::fazy : "-");
    pomiar::fazyDl = 0;
    pomiar::fazy[0] = 0;
    Serial.println(wiersz);
    pomiar::historiaDopisz(wiersz);
    pomiar::wlaczenieMs = 0;
    pomiar::ostatniPunktMs = (uint32_t)millis();  // następna faza liczona od końca tego rysowania
  }
#endif
#if ZRZUT_EKRANU
  // nie przy co-30-minutowym budzeniu (także po czekaniu na kablu zakończonym zegarem, ADR-0047)
  if (czekanieEsp32::poCzekaniu ? czekanieEsp32::powod != PowodWybudzenia::ZEGAR
                                : esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER)
    zrzut();
#endif
  return czesciowe ? Odswiezenie::CZESCIOWE : Odswiezenie::PELNE;
}

// ---------- przyciski i dioda (digitalWrite(LED, …)) ----------
// Kolejka naciśnięć (R-PRZ-20): zadanie esp_timer co 5 ms próbkuje piny przycisków, także w czasie rysowania ekranu,
// gdy rdzeń nie czyta przycisków. Logika (odsianie drgań 15 ms, kolejka 8 naciśnięć, `pomin`, zdjęcie naciśnięcia
// obsłużonego ze stanu) jest w rdzeniu bez Arduino src/przyciski_probki.h: ten sam kod
// sprawdzają testy na PC. Zadanie esp_timer i pętla główna działają na różnych rdzeniach, więc każde wywołanie modelu
// jest w całości pod muxPrzyciskow; pisk (`tone`) i znaczniki obrazu pomiarowego poza sekcją krytyczną,
// według wyniku próbki. Próbkowanie działa tylko, gdy panel nie śpi (głębokie uśpienie zatrzymuje esp_timer).
#include "przyciski_probki.h"  // tu, nie na górze pliku: zmiana ograniczona do sekcji przycisków (inne gałęzie)
namespace {
przyciski_probki::Model modelPrzyciskow;
portMUX_TYPE muxPrzyciskow = portMUX_INITIALIZER_UNLOCKED;
const uint8_t PIN_PRZYCISKU[4] = {0, KEY_LEWY, KEY_SRODEK, KEY_PRAWY};
esp_timer_handle_t zegarPrzyciskow = nullptr;

uint8_t surowePrzyciski() {  // bity (1 << (int)Przycisk) pinów w stanie niskim
  uint32_t we = REG_READ(GPIO_IN_REG);
  uint8_t b = 0;
  for (int i = 1; i <= 3; i++)
    if (((we >> PIN_PRZYCISKU[i]) & 1u) == 0) b = (uint8_t)(b | (1u << i));
  return b;
}

void probkujPrzyciski(void*) {
  uint8_t s = surowePrzyciski();
  int64_t t = esp_timer_get_time();
  portENTER_CRITICAL(&muxPrzyciskow);
  przyciski_probki::ZmianyProbki z = modelPrzyciskow.probka(s, t);
  portEXIT_CRITICAL(&muxPrzyciskow);
  if (z.piski) tone(BUZZER, 2000, 60);  // pisk w chwili naciśnięcia, nie po rysowaniu
#if POMIAR_CZASU
  if (z.piski && pomiar::pierwszeWcisniecieUs == 0) pomiar::pierwszeWcisniecieUs = t;
  if (z.puszczone) pomiar::ostatniePuszczenieUs = t;
#endif
}
}  // namespace

PrzyciskiEsp32::PrzyciskiEsp32() {
  pinMode(KEY_PRAWY, INPUT);
  pinMode(KEY_SRODEK, INPUT);
  pinMode(KEY_LEWY, INPUT);
  // przycisk, który obudził panel, jest już wciśnięty: bez pisku tutaj (piszczy rdzeń) i bez kolejki po puszczeniu
  uint8_t s = surowePrzyciski();
  portENTER_CRITICAL(&muxPrzyciskow);
  modelPrzyciskow.start(s);
  portEXIT_CRITICAL(&muxPrzyciskow);
  esp_timer_create_args_t z = {};
  z.callback = probkujPrzyciski;
  z.name = "przyciski";
  if (esp_timer_create(&z, &zegarPrzyciskow) == ESP_OK)
    esp_timer_start_periodic(zegarPrzyciskow, (uint64_t)przyciski_probki::OKRES_PROBKI_US);
}

bool PrzyciskiEsp32::zKolejki(int& przycisk, uint32_t& trwaloMs) {
  portENTER_CRITICAL(&muxPrzyciskow);
  bool jest = modelPrzyciskow.zKolejki(przycisk, trwaloMs);
  portEXIT_CRITICAL(&muxPrzyciskow);
  return jest;
}

void PrzyciskiEsp32::wyczyscKolejke() {
  portENTER_CRITICAL(&muxPrzyciskow);
  modelPrzyciskow.wyczysc();
  portEXIT_CRITICAL(&muxPrzyciskow);
}

// R-PRZ-20 (zmiana 2026-11-22): rdzeń obsłuży sam naciśnięcie przycisku odczytane właśnie ze stanu; z kolejki znika
// tylko ono, nie cała kolejka (dawne wyczyscKolejke() w czuwaniu kasowało naciśnięcia z czasu liczenia strony).
void PrzyciskiEsp32::obsluzoneZeStanu(int przycisk) {
  int64_t t = esp_timer_get_time();
  portENTER_CRITICAL(&muxPrzyciskow);
  modelPrzyciskow.obsluzone(przycisk, t);
  portEXIT_CRITICAL(&muxPrzyciskow);
}

// chwila wciśnięcia (po odsianiu drgań) w ms zegara rdzenia (ZegarEsp32::ms() liczy od
// czekanieEsp32::poczatekMs); przy starcie programu (odUs = 0): nieznana; przed początkiem przebiegu: 0
bool PrzyciskiEsp32::wcisnietyOd(int przycisk, uint32_t& odMs) {
  int64_t od = 0;
  portENTER_CRITICAL(&muxPrzyciskow);
  bool znane = modelPrzyciskow.wcisnietyOd(przycisk, od);
  portEXIT_CRITICAL(&muxPrzyciskow);
  return znane && przyciski_probki::chwilaWPrzebiegu(od, czekanieEsp32::poczatekMs, odMs);
}

// stan po odsianiu drgań (jak kolejka), nie surowy pin
uint8_t PrzyciskiEsp32::wcisniete() {
  portENTER_CRITICAL(&muxPrzyciskow);
  uint8_t b = modelPrzyciskow.wcisniete();
  portEXIT_CRITICAL(&muxPrzyciskow);
  return b;
}

void PrzyciskiEsp32::dioda(bool swieci) { digitalWrite(LED, swieci ? LOW : HIGH); }

// ADR-0047: przed czekaniem kolejka od nowa (po głębokim śnie też byłaby pusta); próbkowanie działa dalej, więc
// naciśnięcie w czasie czekania piszczy od razu (R-PRZ-20) i ma czas w obrazie pomiarowym.
static void przyciskiPrzedCzekaniem() {
  portENTER_CRITICAL(&muxPrzyciskow);
  modelPrzyciskow.oproznijKolejke();
  portEXIT_CRITICAL(&muxPrzyciskow);
#if POMIAR_CZASU
  pomiar::pierwszeWcisniecieUs = 0;
  pomiar::ostatniePuszczenieUs = 0;
#endif
}
// ADR-0047: przycisk, który kończy czekanie, tylko z maski planu; bit 0 = żaden. Wybór w rdzeniu z testem na PC
// (czekanie::przyciskKonczacy()): starsze naciśnięcie z kolejki przed trzymanym teraz,
// naciśnięcie z kolejki niesie czas trzymania. Wciśnięty teraz: bez kolejki po puszczeniu (przycisk budzący, jak po
// głębokim śnie). Naciśnięcie krótsze niż obieg czekania (już w kolejce): zdjęte z kolejki i budzi, więc nie ginie
// i nie jest obsłużone drugi raz.
static czekanie::PrzyciskKonczacy przyciskKonczacyCzekanie(uint8_t maska) {
  czekanie::KlawiszCzekania k[4];
  czekanie::NacisniecieCzekania q[przyciski_probki::MIEJSC_KOLEJKI];
  portENTER_CRITICAL(&muxPrzyciskow);
  for (int i = 1; i <= 3; i++) {
    const przyciski_probki::Klawisz& kl = modelPrzyciskow.klawisz(i);
    k[i].wcisniety = kl.wcisniety;
    k[i].pomin = kl.pomin;
    k[i].odUs = kl.odUs;
  }
  int ile = modelPrzyciskow.ile();
  for (int j = 0; j < ile; j++) {
    const przyciski_probki::Nacisniecie& n = modelPrzyciskow.wpis(j);
    q[j].przycisk = n.przycisk;
    q[j].trwaloMs = n.trwaloMs;
    q[j].puszczonyUs = n.puszczonyUs;
  }
  czekanie::PrzyciskKonczacy w = czekanie::przyciskKonczacy(maska, k, q, ile);
  if (w.bit && !w.zKolejki) modelPrzyciskow.klawisz(w.klawisz).pomin = true;
  modelPrzyciskow.zdejmij(w.zdjacZKolejki);
  portEXIT_CRITICAL(&muxPrzyciskow);
  return w;
}

// ---------- czujniki ----------
OdczytCzujnikow CzujnikiEsp32::czytaj() {
  OdczytCzujnikow o;
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.beginTransmission(0x44);
  Wire.write(0xFD);  // pomiar wysokiej precyzji
  if (Wire.endTransmission() == 0) {
    delay(10);
    if (Wire.requestFrom(0x44, 6) == 6) {
      uint8_t d[6];
      for (auto& x : d) x = (uint8_t)Wire.read();
      o.temp = -45 + 175.0f * ((d[0] << 8) | d[1]) / 65535.0f;
      o.wilg = constrain(-6 + 125.0f * ((d[3] << 8) | d[4]) / 65535.0f, 0.0f, 100.0f);
    }
  }
  // przed epd.init() i przed Wi-Fi (bez spadku pod obciążeniem), średnia z 8 próbek
  pinMode(BAT_EN, OUTPUT);
  digitalWrite(BAT_EN, HIGH);
  delay(5);
  float suma = 0;
  for (int i = 0; i < 8; i++) {
    suma += (float)analogReadMilliVolts(BAT_ADC);
    delay(2);
  }
  o.bateriaV = suma / 8.0f * 2.0f / 1000.0f;
  digitalWrite(BAT_EN, LOW);
  return o;
}

// Jeden rejestr ładowarki; false = brak potwierdzenia na magistrali albo brak bajtu.
static bool rejestrLadowarki(uint8_t rejestr, uint8_t& wartosc) {
  Wire1.beginTransmission(LAD_ADRES);
  Wire1.write(rejestr);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(LAD_ADRES, 1) != 1) return false;
  wartosc = (uint8_t)Wire1.read();
  return true;
}

// R-DANE-65: zasilanie zewnętrzne z rejestru stanu ładowarki (PG_STAT), tylko odczyt (żadnego zapisu do ładowarki).
// Układ rozpoznany po numerze w REG0B; inny układ, brak odpowiedzi albo błąd odczytu = NIEZNANE (rdzeń: jak bateria).
// Każda operacja na magistrali ma limit LAD_LIMIT_MS (najwyżej 3 odczyty rejestrów, czyli wisząca magistrala kosztuje
// ułamek sekundy na wybudzenie, nie blokuje go). REG0A idzie tylko do logu (PG z REG08 czy BUS_GD z REG0A).
OdczytZasilania CzujnikiEsp32::zasilanie() {
  OdczytZasilania o;
  if (!Wire1.begin(I2C1_SDA, I2C1_SCL)) return o;
  Wire1.setTimeOut(LAD_LIMIT_MS);
  uint8_t uklad = 0, stan = 0, wejscie = 0;
  if (!rejestrLadowarki(LAD_REJ_UKLAD, uklad) || (uklad & LAD_UKLAD_MASKA) != LAD_UKLAD_SY6974B) return o;
  if (!rejestrLadowarki(LAD_REJ_STAN, stan)) return o;
  o.rejStanu = stan;
  if (rejestrLadowarki(LAD_REJ_WEJSCIE, wejscie)) o.rejWejscia = wejscie;
  o.stan = (stan & LAD_STAN_PG) ? Zasilanie::ZEWNETRZNE : Zasilanie::BATERIA;
  return o;
}

// ADR-0046 (limit ładowania): decyzję, co zapisać, podejmuje rdzeń (src/ladowarka.cpp: rozpoznanie układu po REG0B,
// tylko pola VREG i WATCHDOG, odczyt kontrolny); adapter wykonuje pojedyncze operacje na Wire1 z limitem LAD_LIMIT_MS.
bool RejestryLadowarkiEsp32::magistrala() {
  if (!gotowa_) {
    if (!Wire1.begin(I2C1_SDA, I2C1_SCL)) return false;
    Wire1.setTimeOut(LAD_LIMIT_MS);
    gotowa_ = true;
  }
  return true;
}
bool RejestryLadowarkiEsp32::czytaj(uint8_t rejestr, uint8_t& wartosc) {
  return magistrala() && rejestrLadowarki(rejestr, wartosc);
}
bool RejestryLadowarkiEsp32::zapisz(uint8_t rejestr, uint8_t wartosc) {
  if (!magistrala()) return false;
  Wire1.beginTransmission(LAD_ADRES);
  Wire1.write(rejestr);
  Wire1.write(wartosc);
  return Wire1.endTransmission() == 0;
}

#if POMIAR_CZASU
// Obraz pomiarowy: rejestry 0x00–0x0B ładowarki SY6974B na port szeregowy przy starcie programu. TYLKO ODCZYT
// (żadnego zapisu do ładowarki): stan wyjściowy przed pracą nad limitem ładowania (16.11.2026).
void pomiarLadowarka() {
  if (!Wire1.begin(I2C1_SDA, I2C1_SCL)) { Serial.println("POMIAR ladowarka: brak magistrali"); return; }
  Wire1.setTimeOut(LAD_LIMIT_MS);
  char wiersz[160];
  int k = snprintf(wiersz, sizeof wiersz, "POMIAR ladowarka:");
  for (uint8_t r = 0x00; r <= 0x0B; r++) {
    uint8_t w = 0;
    if (rejestrLadowarki(r, w)) k += snprintf(wiersz + k, sizeof wiersz - k, " %02X=%02X", r, w);
    else k += snprintf(wiersz + k, sizeof wiersz - k, " %02X=--", r);
  }
  // REG09 (błędy) zatrzaskuje się do odczytu [K]: drugi odczyt to stan bieżący (BATOVP przy limicie: ADR-0046 pkt 10)
  uint8_t w9 = 0;
  if (rejestrLadowarki(0x09, w9)) k += snprintf(wiersz + k, sizeof wiersz - k, " 09bis=%02X", w9);
  else k += snprintf(wiersz + k, sizeof wiersz - k, " 09bis=--");
  Serial.println(wiersz);
}
#endif

// ---------- buzzer ----------
void DzwiekEsp32::pisk(int n) {
  for (int i = 0; i < n; i++) {
    tone(BUZZER, 2000, 60);
    if (i + 1 < n) delay(120);
  }
}

// ---------- uruchomienie: przyczyna startu, RTC, wersja (T-WYM-82, T-WYM-41) ----------
// Licznik startów po błędzie: pamięć RTC, której nie zeruje ani bootloader, ani restart po błędzie programu
// i strażniku (RTC_NOINIT_ATTR); po włączeniu zasilania przypadkowa, ważność sprawdza rdzeń (start.h).
RTC_NOINIT_ATTR static LicznikStartow licznikRtc;
// Stan między wybudzeniami (StanRtc): surowy blok RTC_DATA_ATTR, tworzony od nowa przy każdym starcie innym niż
// wybudzenie z głębokiego uśpienia (jak zmienne RTC_DATA_ATTR prototypu po starcie z bootloadera). Bez konstruktora
// zmiennej globalnej: ten nadpisałby stan przy każdym wybudzeniu.
RTC_DATA_ATTR alignas(StanRtc) static uint8_t blokRtc[sizeof(StanRtc)];

UruchomienieEsp32::UruchomienieEsp32(const char* wersja) : wersja_(wersja) {
  if (esp_reset_reason() == ESP_RST_DEEPSLEEP) rtc_ = reinterpret_cast<StanRtc*>(blokRtc);
  else rtc_ = new (blokRtc) StanRtc();
}

PrzyczynaStartu UruchomienieEsp32::przyczyna() {
  if (czekanieEsp32::poCzekaniu) return PrzyczynaStartu::USPIENIE;  // ADR-0047: przebieg po czekaniu na kablu
  switch (esp_reset_reason()) {
    case ESP_RST_UNKNOWN: return PrzyczynaStartu::NIEZNANA;
    case ESP_RST_POWERON: return PrzyczynaStartu::ZASILANIE;
    case ESP_RST_EXT: return PrzyczynaStartu::ZEWNETRZNY;
    case ESP_RST_SW: return PrzyczynaStartu::PROGRAMOWY;
    case ESP_RST_PANIC: return PrzyczynaStartu::PANIKA;
    case ESP_RST_INT_WDT: return PrzyczynaStartu::INT_WDT;
    case ESP_RST_TASK_WDT: return PrzyczynaStartu::TASK_WDT;
    case ESP_RST_WDT: return PrzyczynaStartu::WDT;
    case ESP_RST_DEEPSLEEP: return PrzyczynaStartu::USPIENIE;
    case ESP_RST_BROWNOUT: return PrzyczynaStartu::SPADEK;
    default: return PrzyczynaStartu::INNA;
  }
}

PowodWybudzenia UruchomienieEsp32::powod() {
  if (czekanieEsp32::poCzekaniu) return czekanieEsp32::powod;
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER: return PowodWybudzenia::ZEGAR;
    case ESP_SLEEP_WAKEUP_EXT1: return PowodWybudzenia::PRZYCISK;
    default: return PowodWybudzenia::START;
  }
}

uint32_t UruchomienieEsp32::trwanieBudzacegoMs() { return czekanieEsp32::poCzekaniu ? czekanieEsp32::trwanieBudzacego : 0; }

uint8_t UruchomienieEsp32::przyciskiBudzace() {
  if (czekanieEsp32::poCzekaniu) return czekanieEsp32::budzace;
  uint64_t m = esp_sleep_get_ext1_wakeup_status();
  uint8_t b = 0;
  for (Przycisk p : {Przycisk::LEWY, Przycisk::SRODEK, Przycisk::PRAWY})
    if (m & (1ULL << pin(p))) b = (uint8_t)(b | (1u << (int)p));
  return b;
}

std::string UruchomienieEsp32::slot() {
  const esp_partition_t* p = esp_ota_get_running_partition();
  return p ? p->label : "?";
}

LicznikStartow& UruchomienieEsp32::licznik() { return licznikRtc; }
StanRtc& UruchomienieEsp32::rtc() { return *rtc_; }

StanWersji UruchomienieEsp32::stanWersji() {
  esp_ota_img_states_t s;
  const esp_partition_t* p = esp_ota_get_running_partition();
  if (p && esp_ota_get_state_partition(p, &s) == ESP_OK && s == ESP_OTA_IMG_PENDING_VERIFY)
    return StanWersji::DO_SPRAWDZENIA;
  return StanWersji::DOBRA;  // ESP_OTA_IMG_VALID albo bez danych OTA (wgranie kablem)
}

void UruchomienieEsp32::oznaczWersjeDobra() {
  esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
  Serial.printf("wersja %s oznaczona jako dobra (%d)\n", wersja_, (int)e);
}

void UruchomienieEsp32::wrocDoPoprzedniej() {
  Serial.println("powrót do poprzedniej wersji (T-WYM-41)");
  Serial.flush();
  esp_err_t e = esp_ota_mark_app_invalid_rollback_and_reboot();  // przy sukcesie nie wraca
  // Nie ma poprzedniej wersji (np. pierwsza wersja z OTA): zostaje ta, oznaczona, żeby panel mógł zasnąć.
  Serial.printf("powrót niemożliwy (%d): wersja zostaje\n", (int)e);
  esp_ota_mark_app_valid_cancel_rollback();
}

// ---------- strażnik zadania (T-WYM-81) ----------
void StraznikEsp32::zapisz(uint32_t limitS, bool panika) {
  esp_task_wdt_config_t c = {};
  c.timeout_ms = limitS * 1000;
  // ten sam zestaw zadań bezczynności co w sdkconfig rdzenia (CPU0 w 3.3.12): zmienia się tylko limit
  c.idle_core_mask = 0;
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  c.idle_core_mask |= 1u << 0;
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  c.idle_core_mask |= 1u << 1;
#endif
  c.trigger_panic = panika;
  esp_err_t e = esp_task_wdt_reconfigure(&c);
  if (e == ESP_ERR_INVALID_STATE) e = esp_task_wdt_init(&c);  // strażnik niezainicjowany przez rdzeń
  // zadanie loop (setup() biegnie w nim) tylko przy ustawionym limicie: przy domyślnych 5 s każde dłuższe żądanie
  // HTTP kończyłoby się restartem; bez strażnika zostaje licznik startów
  // ADR-0047: po czekaniu na kablu zadanie jest już zapisane (ten sam program), a drugie esp_task_wdt_add() daje
  // w logu `task_wdt: add_entry(196): task is already subscribed` przy każdym przebiegu
  esp_err_t d = e;
  if (e == ESP_OK) d = esp_task_wdt_status(NULL) == ESP_OK ? ESP_OK : esp_task_wdt_add(NULL);
  Serial.printf("strażnik zadania: %lu s, panika %d (%d, %d)\n", (unsigned long)limitS, (int)panika, (int)e, (int)d);
}

void StraznikEsp32::odnow() { esp_task_wdt_reset(); }

// ---------- sen ----------
void SenEsp32::spij(const PlanSnu& plan) {
  if (plan.budzZegarem) esp_sleep_enable_timer_wakeup((uint64_t)plan.sekund * 1000000ULL);
  // U4: przycisk wciśnięty w chwili zasypiania (zacięty) nie budzi; maskę wybiera rdzeń (planSnu())
  uint64_t maska = 0;
  for (Przycisk p : {Przycisk::PRAWY, Przycisk::SRODEK, Przycisk::LEWY})
    if (plan.przyciski & (1u << (int)p)) maska |= 1ULL << pin(p);
  if (maska) esp_sleep_enable_ext1_wakeup(maska, ESP_EXT1_WAKEUP_ANY_LOW);
  Serial.flush();
  esp_deep_sleep_start();
}

// ADR-0047, T-WYM-10, T-WYM-12: czekanie na kablu. Wariant prosty (wydanie bez nocy obserwacji, 16.11.2026): pętla
// z delay() zamiast lekkiego uśpienia: zegar, Wi-Fi wyłączone (rdzeń zamknął sieć przed snem), ekran uśpiony,
// próbkowanie przycisków co 5 ms jak w czuwaniu (pisk, kolejka), strażnik zadania odnawiany w każdym obiegu, co
// czekanie::KAWALEK_CZEKANIA_S sprawdzenie zasilania przez rdzeń (`przerwij`). Procesor w obiegu stoi w delay()
// (zadanie bezczynności), bez pętli zajętej. Lekkie uśpienie odłożone (do decyzji).
static const uint32_t OBIEG_CZEKANIA_MS = 20;  // [PRÓG] reakcja na przycisk w czekaniu (próbkowanie i tak co 5 ms)
WynikCzekania SenEsp32::czekaj(const PlanSnu& plan, const std::function<bool()>& przerwij) {
  WynikCzekania w;
  if (!plan.budzZegarem) return w;  // rdzeń o to nie prosi (tryb awaryjny śpi głęboko)
  przyciskiPrzedCzekaniem();
  Serial.flush();
  const int64_t start = esp_timer_get_time();
  const int64_t koniec = start + (int64_t)plan.sekund * 1000000LL;
  int64_t sprawdzenie = start + (int64_t)czekanie::KAWALEK_CZEKANIA_S * 1000000LL;
  czekanie::PrzyciskKonczacy b;
  for (;;) {
    esp_task_wdt_reset();
    b = przyciskKonczacyCzekanie(plan.przyciski);
    if (b.bit) {
      w.rodzaj = WynikCzekania::Rodzaj::PRZYCISK;
      break;
    }
    int64_t t = esp_timer_get_time();
    if (t >= koniec) {
      w.rodzaj = WynikCzekania::Rodzaj::ZEGAR;
      break;
    }
    if (t >= sprawdzenie) {
      sprawdzenie = t + (int64_t)czekanie::KAWALEK_CZEKANIA_S * 1000000LL;
      if (przerwij && przerwij()) {
        w.rodzaj = WynikCzekania::Rodzaj::PRZERWANE;
        int64_t zostalo = (koniec - t + 999999) / 1000000;
        w.zostaloS = zostalo < 1 ? 1u : (uint32_t)zostalo;
        return w;
      }
    }
    delay(OBIEG_CZEKANIA_MS);
  }
  // następny przebieg jak po wybudzeniu z głębokiego snu (przyczyna USPIENIE, powód, przyciski, ms od 0)
  czekanieEsp32::poCzekaniu = true;
  czekanieEsp32::powod =
      w.rodzaj == WynikCzekania::Rodzaj::PRZYCISK ? PowodWybudzenia::PRZYCISK : PowodWybudzenia::ZEGAR;
  czekanieEsp32::budzace = b.bit;
  // Naciśnięcie już puszczone niesie czas trzymania (rdzeń: przytrzymanie zostaje przytrzymaniem); trzymany
  // teraz: ms() rdzenia liczy od chwili wciśnięcia (millis() = esp_timer_get_time() / 1000), więc przytrzymanie
  // budzącego jest mierzone od naciśnięcia, nie od końca czekania
  czekanieEsp32::trwanieBudzacego = b.bit && b.zKolejki ? (b.trwaloMs ? b.trwaloMs : 1u) : 0u;
  czekanieEsp32::poczatekMs =
      b.bit && !b.zKolejki && b.odUs > 0 ? (uint32_t)(b.odUs / 1000) : (uint32_t)millis();
#if POMIAR_CZASU
  pomiar::ostatniPunktMs = (uint32_t)millis();
  pomiar::numer = 0;
  pomiar::ostatniePuszczenieUs = 0;
#endif
  return w;
}

// ---------- port serwisowy (UART) ----------
void PortEsp32::wiersz(const std::string& t) { Serial.println(t.c_str()); }
int PortEsp32::znak() { return Serial.available() ? Serial.read() : -1; }

// ---------- diagnostyka pamięci ----------
uint32_t DiagnostykaEsp32::minWolnaSterta() {
  return (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
uint32_t DiagnostykaEsp32::zapasStosu() { return (uint32_t)uxTaskGetStackHighWaterMark(NULL); }  // bajty (ESP-IDF)
uint32_t DiagnostykaEsp32::najwiekszyBlokPsram() {  // ADR-0047, T-WYM-13
  return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}
uint32_t DiagnostykaEsp32::najwiekszyBlokWewn() {  // ADR-0047, T-WYM-13 (17.11.2026)
  return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
uint32_t DiagnostykaEsp32::sekundOdStartu() { return (uint32_t)(esp_timer_get_time() / 1000000LL); }

// ---------- WydaniaEsp32 (ADR-0032, ADR-0038) ----------
// Decyzje (adres po przekierowaniu, kompletność ciała, nadmiar bajtów) są w rdzeniu z testami na PC:
// aktualizacja::kodPrzekierowania(), nastepnyAdres(), cialoKompletne(), ileZapisac(). HTTP/1.0: bez kodowania
// `chunked` i bez keep-alive, więc ciało to surowe bajty do zamknięcia połączenia albo do Content-Length.
namespace {
// MBEDTLS_ERR_X509_CERT_VERIFY_FAILED: certyfikat serwera spoza zestawu (żądanie z tokenem nie wyszło)
const int BLAD_CERTYFIKATU = -0x2700;

// Jedno żądanie GET przez HTTPS z przekierowaniami ręcznie. Nagłówki każdego adresu liczy rdzeń
// (aktualizacja::naglowki), więc po przekierowaniu na inny host żądanie idzie bez tokenu (R-NF-36). Połączenie
// i uzgadnianie TLS mają osobny limit LIMIT_POLACZENIA_MS (krótszy niż strażnik zadania); `odnow` woła się
// przed każdym żądaniem. Zwraca kod HTTP ostatniego żądania albo ujemny błąd.
template <typename F>
int getHttps(const ZadanieWydania& z, OdpowiedzWydania& o, const std::function<void()>& odnow, F&& cialo) {
  std::string url = z.url;
  const uint32_t pol = aktualizacja::LIMIT_POLACZENIA_MS;
  for (int krok = 0;; krok++) {
    std::string host = aktualizacja::hostZAdresu(url);
    if (host.empty()) return -2;  // tylko https://
    o.hosty.push_back(host);
    if (odnow) odnow();
    NetworkClientSecure klient;
    klient.setCACert(CERTYFIKATY_GITHUB);
    klient.setTimeout(pol / 1000);
    klient.setHandshakeTimeout(pol / 1000);
    HTTPClient http;
    http.useHTTP10(true);  // bez `Transfer-Encoding: chunked`
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setTimeout((uint16_t)pol);
    http.setConnectTimeout((int32_t)pol);
    if (!http.begin(klient, String(url.c_str()))) return -1;
    const char* zbierane[] = {"Location"};
    http.collectHeaders(zbierane, 1);
    for (auto& n : aktualizacja::naglowki(url, z.token, z.binarne)) http.addHeader(n.first.c_str(), n.second.c_str());
    int kod = http.GET();
    if (odnow) odnow();
    if (kod < 0) {
      char opis[8];
      if (klient.lastError(opis, sizeof opis) == BLAD_CERTYFIKATU) o.certyfikatOk = false;
      http.end();
      return kod;
    }
    if (aktualizacja::kodPrzekierowania(kod)) {
      std::string nowy = aktualizacja::nastepnyAdres(url, http.header("Location").c_str(), krok + 1);
      http.end();
      if (nowy.empty()) return -3;  // obcy schemat, pętla albo za dużo przekierowań
      url = nowy;
      continue;
    }
    if (kod == 200) cialo(http);
    http.end();
    return kod;
  }
}
}  // namespace

OdpowiedzWydania WydaniaEsp32::pobierzTekst(const ZadanieWydania& z) {
  OdpowiedzWydania o;
  bool niekompletne = false;
  o.kod = getHttps(z, o, [] { esp_task_wdt_reset(); }, [&](HTTPClient& http) {
    long dl = http.getSize();
    if (z.maksBajtow && dl > (long)z.maksBajtow) {
      o.zaDuzo = true;
      return;
    }
    NetworkClient* s = http.getStreamPtr();
    uint32_t t0 = (uint32_t)millis(), ostatni = t0;
    uint8_t buf[512];
    bool zamkniete = false;
    while (s && (dl < 0 || (long)o.cialo.size() < dl)) {
      esp_task_wdt_reset();
      uint32_t t = (uint32_t)millis();
      if (t - t0 > z.limitMs || t - ostatni > aktualizacja::LIMIT_PRZESTOJU_MS) break;
      int n = s->available() ? s->read(buf, sizeof buf) : 0;
      if (n <= 0) {
        if (!s->connected()) {
          zamkniete = true;
          break;
        }
        delay(5);
        continue;
      }
      ostatni = t;
      o.cialo.append((const char*)buf, (size_t)n);
      if (z.maksBajtow && o.cialo.size() > z.maksBajtow) break;
    }
    if (!aktualizacja::cialoKompletne(dl, o.cialo.size(), zamkniete, z.maksBajtow)) {
      o.zaDuzo = z.maksBajtow && o.cialo.size() > z.maksBajtow;
      o.cialo.clear();
      niekompletne = true;
    }
  });
  if (niekompletne && !o.zaDuzo) o.kod = -6;  // ciało niekompletne (przestój, zerwane): jak brak połączenia
  return o;
}

WynikObrazu WydaniaEsp32::pobierzObraz(const ZadanieWydania& z, uint64_t rozmiar, const std::function<void()>& postep) {
  WynikObrazu w;
  porzuc();
  const esp_partition_t* slot = esp_ota_get_next_update_partition(nullptr);
  if (!slot || rozmiar == 0 || rozmiar > slot->size) {
    w.http.kod = -4;
    return w;
  }
  if (postep) postep();
  esp_ota_handle_t h = 0;
  if (esp_ota_begin(slot, (size_t)rozmiar, &h) != ESP_OK) {  // kasowanie slotu (do 4 MB, kilkanaście s)
    w.http.kod = -5;
    return w;
  }
  if (postep) postep();
  uchwyt_ = (uint32_t)h;
  aktualizacja::Sha256 sha;
  bool blad = false;
  w.http.kod = getHttps(z, w.http, postep, [&](HTTPClient& http) {
    NetworkClient* s = http.getStreamPtr();
    uint32_t t0 = (uint32_t)millis(), ostatni = t0;
    uint8_t buf[2048];
    while (s && w.bajtow < rozmiar) {
      if (postep) postep();
      uint32_t t = (uint32_t)millis();
      if (t - t0 > z.limitMs || t - ostatni > aktualizacja::LIMIT_PRZESTOJU_MS) break;  // całość i przestój [PRÓG]
      int n = s->available() ? s->read(buf, sizeof buf) : 0;
      if (n <= 0) {
        if (!s->connected()) break;
        delay(5);
        continue;
      }
      ostatni = t;
      size_t ile = aktualizacja::ileZapisac(w.bajtow, rozmiar, (size_t)n);  // nadmiar = plik niezgodny z opisem
      if (esp_ota_write(h, buf, ile) != ESP_OK) {
        blad = true;
        break;
      }
      sha.dodaj(buf, ile);
      w.bajtow += ile;
    }
  });
  w.sha256 = sha.hex();
  w.zapisOk = !blad && w.http.kod == 200 && w.bajtow == rozmiar;
  zapisany_ = w.zapisOk;
  if (!w.zapisOk) porzuc();
  return w;
}

bool WydaniaEsp32::ustawStartowy() {
  if (!uchwyt_ || !zapisany_) return false;
  esp_ota_handle_t h = (esp_ota_handle_t)uchwyt_;
  uchwyt_ = 0;
  zapisany_ = false;
  if (esp_ota_end(h) != ESP_OK) return false;  // ESP-IDF sprawdza nagłówek i skrót obrazu
  const esp_partition_t* slot = esp_ota_get_next_update_partition(nullptr);
  return slot && esp_ota_set_boot_partition(slot) == ESP_OK;
}

void WydaniaEsp32::porzuc() {
  if (uchwyt_) esp_ota_abort((esp_ota_handle_t)uchwyt_);
  uchwyt_ = 0;
  zapisany_ = false;
}

void WydaniaEsp32::restart() { esp_restart(); }

uint64_t WydaniaEsp32::rozmiarSlotu() {
  const esp_partition_t* slot = esp_ota_get_next_update_partition(nullptr);
  return slot ? slot->size : 0;
}
