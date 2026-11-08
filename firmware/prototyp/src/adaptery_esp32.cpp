// SPDX-License-Identifier: GPL-3.0-or-later
// Adaptery panelu reTerminal E1001 (src/adaptery_esp32.h): kod sprzętu, kontrakty w src/adaptery.h.
#include "adaptery_esp32.h"
#include "magazyn.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <mbedtls/md5.h>
#include <Preferences.h>
#include <sys/time.h>
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

void snAwaryjny() {
  Serial.println("dyrygent wrócił bez snu: sen awaryjny 30 min");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(30ULL * 60 * 1000000ULL);
  esp_sleep_enable_ext1_wakeup((1ULL << KEY_PRAWY) | (1ULL << KEY_SRODEK) | (1ULL << KEY_LEWY), ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

// ---------- zegar ----------
uint32_t ZegarEsp32::teraz() {
  time_t t;
  time(&t);
  return (uint32_t)t;
}
uint32_t ZegarEsp32::ms() { return (uint32_t)millis(); }
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
bool PlikiEsp32::istnieje(const char* n) { return montowane_ && LittleFS.exists(n); }

bool PlikiEsp32::czytajWiersze(const char* nazwa, std::vector<std::string>& w) {
  w.clear();
  if (!montowane_) return false;
  String tmp = String(nazwa) + ".tmp";
  if (!LittleFS.exists(nazwa) && LittleFS.exists(tmp)) LittleFS.rename(tmp, nazwa);
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
  File r = LittleFS.open(nazwa, "r");
  if (r) {
    size_t n = r.size();
    if (n && r.seek((uint32_t)(n - 1)) && r.read() != '\n') nowaLinia = true;
    r.close();
  }
  File f = LittleFS.open(nazwa, "a");
  if (!f) return false;
  if (nowaLinia) f.print("\n");
  f.print((wiersz + "\n").c_str());
  f.close();
  return true;
}

bool PlikiEsp32::pierwszyWiersz(const char* nazwa, std::string& wiersz) {
  if (!montowane_) return false;
  File f = LittleFS.open(nazwa, "r");
  if (!f) return false;
  String l = f.readStringUntil('\n');
  f.close();
  wiersz = l.c_str();
  return true;
}

bool PlikiEsp32::usun(const char* n) { return montowane_ && LittleFS.remove(n); }
bool PlikiEsp32::zmienNazwe(const char* z, const char* na) {
  if (!montowane_) return false;
  if (LittleFS.rename(z, na)) return true;  // littlefs: rename nadpisuje cel atomowo
  if (!LittleFS.exists(z) || !LittleFS.exists(na)) return false;
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
    if (!LittleFS.exists(k.c_str())) LittleFS.mkdir(k.c_str());
  }
}

bool PlikiEsp32::czytaj(const char* nazwa, std::string& tresc) {
  tresc.clear();
  if (!montowane_ || !LittleFS.exists(nazwa)) return false;
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
  if (!montowane_ || !LittleFS.exists(katalog)) return false;
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
// write() zwraca 0, co kończy writeToStream() błędem zapisu.
struct DoNapisu : Stream {
  std::string& s;
  const ZadanieHttp& z;
  bool zaDuzo = false, zaDlugo = false, przerwane = false;
  DoNapisu(std::string& t, const ZadanieHttp& zad) : s(t), z(zad) {}
  bool stop() {
    if (z.koniecMs && (int32_t)((uint32_t)millis() - z.koniecMs) >= 0) zaDlugo = true;
    if (!zaDlugo && z.przerwij && z.przerwij()) przerwane = true;
    return zaDlugo || przerwane;
  }
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* b, size_t n) override {
    if ((z.koniecMs || z.przerwij) && stop()) return 0;
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
      DoNapisu cel(o.cialo, z);
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

// ---------- ekran e-ink ----------
static SPIClass hspi(HSPI);
static GxEPD2_BW<GxEPD2_750_GDEY075T7, GxEPD2_750_GDEY075T7::HEIGHT> epd(
    GxEPD2_750_GDEY075T7(EPD_CS, EPD_DC, EPD_RES, EPD_BUSY));
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

void WyswietlaczEsp32::wlacz() {
  hspi.begin(EPD_SCK, -1, EPD_MOSI, -1);
  epd.epd2.selectSPI(hspi, SPISettings(2000000, MSBFIRST, SPI_MODE0));
  epd.init(0);
}

Plotno& WyswietlaczEsp32::plotno() { return plotnoGfx; }

const uint8_t* WyswietlaczEsp32::bufor() { return kanwa.getBuffer(); }  // GFXcanvas1 800×480: 48 000 B

void WyswietlaczEsp32::odswiez() {
  epd.setFullWindow();
  epd.firstPage();
  do {
    epd.fillScreen(GxEPD_WHITE);
    epd.drawBitmap(0, 0, kanwa.getBuffer(), 800, 480, GxEPD_BLACK);
  } while (epd.nextPage());
  epd.hibernate();
#if ZRZUT_EKRANU
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) zrzut();  // nie przy co-30-minutowym budzeniu
#endif
}

// ---------- przyciski i dioda (digitalWrite(LED, …)) ----------
// Kolejka naciśnięć (R-PRZ-20): zadanie esp_timer co 5 ms próbkuje piny przycisków, także w czasie rysowania ekranu,
// gdy rdzeń nie czyta przycisków. Stan liczy się po 3 zgodnych próbkach (15 ms), więc drgania styku nie dają
// fałszywych naciśnięć ani zgubionych puszczeń. Wciśnięcie: od razu krótki pisk; puszczenie: naciśnięcie do kolejki
// z czasem trzymania. Próbkowanie działa tylko, gdy panel nie śpi (głębokie uśpienie zatrzymuje esp_timer).
namespace {
struct NacisniecieKolejki {
  uint8_t przycisk;
  uint32_t trwaloMs;
};
struct StanKlawisza {
  bool wcisniety = false;  // stan po odsianiu drgań
  uint8_t zgodnych = 0;    // kolejne próbki różne od `wcisniety`
  int64_t odUs = 0;        // chwila wciśnięcia (0 = nieznana)
  bool pomin = false;      // puszczenie nie trafia do kolejki (przycisk budzący, wyczyscKolejke())
};
const int KOLEJKA_NACISNIEC = 8;            // R-PRZ-20 [PRÓG]; nadmiar przepada (najnowsze)
const uint64_t OKRES_PROBKI_US = 5000;
const uint8_t PROBEK_STANU = 3;             // 15 ms zgodnych próbek
NacisniecieKolejki kolejkaPrz[KOLEJKA_NACISNIEC];
uint8_t kolejkaGlowa = 0, kolejkaOgon = 0;
StanKlawisza klawisze[4];                   // indeks = (int)Przycisk
portMUX_TYPE muxPrzyciskow = portMUX_INITIALIZER_UNLOCKED;
const uint8_t PIN_PRZYCISKU[4] = {0, KEY_LEWY, KEY_SRODEK, KEY_PRAWY};
esp_timer_handle_t zegarPrzyciskow = nullptr;

bool wcisnietyTeraz(uint32_t wejscia, int i) { return ((wejscia >> PIN_PRZYCISKU[i]) & 1u) == 0; }  // stan niski

void probkujPrzyciski(void*) {
  uint32_t we = REG_READ(GPIO_IN_REG);
  int64_t t = esp_timer_get_time();
  for (int i = 1; i <= 3; i++) {
    StanKlawisza& k = klawisze[i];
    bool teraz = wcisnietyTeraz(we, i);
    if (teraz == k.wcisniety) {
      k.zgodnych = 0;
      continue;
    }
    if (++k.zgodnych < PROBEK_STANU) continue;
    k.zgodnych = 0;
    k.wcisniety = teraz;
    if (teraz) {
      k.odUs = t;
      if (!k.pomin) tone(BUZZER, 2000, 60);  // pisk w chwili naciśnięcia, nie po rysowaniu
      continue;
    }
    portENTER_CRITICAL(&muxPrzyciskow);
    uint8_t nast = (uint8_t)((kolejkaOgon + 1) % KOLEJKA_NACISNIEC);
    if (!k.pomin && k.odUs != 0 && nast != kolejkaGlowa) {
      kolejkaPrz[kolejkaOgon].przycisk = (uint8_t)i;
      kolejkaPrz[kolejkaOgon].trwaloMs = (uint32_t)((t - k.odUs) / 1000);
      kolejkaOgon = nast;
    }
    k.odUs = 0;
    k.pomin = false;
    portEXIT_CRITICAL(&muxPrzyciskow);
  }
}
}  // namespace

PrzyciskiEsp32::PrzyciskiEsp32() {
  pinMode(KEY_PRAWY, INPUT);
  pinMode(KEY_SRODEK, INPUT);
  pinMode(KEY_LEWY, INPUT);
  // przycisk, który obudził panel, jest już wciśnięty: bez pisku tutaj (piszczy rdzeń) i bez kolejki po puszczeniu
  uint32_t we = REG_READ(GPIO_IN_REG);
  for (int i = 1; i <= 3; i++) klawisze[i].wcisniety = klawisze[i].pomin = wcisnietyTeraz(we, i);
  esp_timer_create_args_t z = {};
  z.callback = probkujPrzyciski;
  z.name = "przyciski";
  if (esp_timer_create(&z, &zegarPrzyciskow) == ESP_OK) esp_timer_start_periodic(zegarPrzyciskow, OKRES_PROBKI_US);
}

bool PrzyciskiEsp32::zKolejki(int& przycisk, uint32_t& trwaloMs) {
  bool jest = false;
  portENTER_CRITICAL(&muxPrzyciskow);
  if (kolejkaGlowa != kolejkaOgon) {
    przycisk = kolejkaPrz[kolejkaGlowa].przycisk;
    trwaloMs = kolejkaPrz[kolejkaGlowa].trwaloMs;
    kolejkaGlowa = (uint8_t)((kolejkaGlowa + 1) % KOLEJKA_NACISNIEC);
    jest = true;
  }
  portEXIT_CRITICAL(&muxPrzyciskow);
  return jest;
}

void PrzyciskiEsp32::wyczyscKolejke() {
  portENTER_CRITICAL(&muxPrzyciskow);
  kolejkaGlowa = kolejkaOgon;
  for (int i = 1; i <= 3; i++)
    if (klawisze[i].wcisniety) klawisze[i].pomin = true;
  portEXIT_CRITICAL(&muxPrzyciskow);
}

uint8_t PrzyciskiEsp32::wcisniete() {
  uint8_t b = 0;
  for (Przycisk p : {Przycisk::LEWY, Przycisk::SRODEK, Przycisk::PRAWY})
    if (digitalRead(pin(p)) == LOW) b = (uint8_t)(b | (1u << (int)p));
  return b;
}

void PrzyciskiEsp32::dioda(bool swieci) { digitalWrite(LED, swieci ? LOW : HIGH); }

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
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER: return PowodWybudzenia::ZEGAR;
    case ESP_SLEEP_WAKEUP_EXT1: return PowodWybudzenia::PRZYCISK;
    default: return PowodWybudzenia::START;
  }
}

uint8_t UruchomienieEsp32::przyciskiBudzace() {
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
  esp_err_t d = e == ESP_OK ? esp_task_wdt_add(NULL) : e;
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

// ---------- port serwisowy (UART) ----------
void PortEsp32::wiersz(const std::string& t) { Serial.println(t.c_str()); }
int PortEsp32::znak() { return Serial.available() ? Serial.read() : -1; }

// ---------- diagnostyka pamięci ----------
uint32_t DiagnostykaEsp32::minWolnaSterta() {
  return (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
uint32_t DiagnostykaEsp32::zapasStosu() { return (uint32_t)uxTaskGetStackHighWaterMark(NULL); }  // bajty (ESP-IDF)
