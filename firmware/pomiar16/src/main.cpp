// SPDX-License-Identifier: GPL-3.0-or-later
// Pomiar 16 (T-TEST-253): czy pamięć flash powyżej 16 MB (0x1000000) działa na egzemplarzu E1001.
//
// Przebieg (stan trzymany w NVS, bo test przeżywa restarty):
//   1. kontrola: sterownik zna 32 MB, partycja plików obejmuje obszar powyżej 0x1000000, adres się nie zawija;
//   2. test surowy: cały obszar partycji powyżej 0x1000000 (16 MB): kasowanie, zapis wzorca zależnego od adresu,
//      odczyt i porównanie bajt po bajcie;
//   3. LittleFS: montowanie z formatowaniem (pierwsze formatowanie), plik 20 MB wzorcem z licznika, SHA-256,
//      odczyt i porównanie skrótu;
//   4. trzy restarty programowe; po każdym ponowny odczyt pliku i porównanie skrótu z zapisanym w NVS.
// Wynik: jeden wiersz `POMIAR16 WYNIK: ZALICZONE` albo `POMIAR16 WYNIK: NIE ZALICZONE: {powód}` i wiersze ze szczegółami.
// Całość w porcie szeregowym 115200; wyniku nie trzeba łapać na żywo, po zakończeniu jest powtarzany co 10 s.
// Znak `R` wysłany do portu po zakończeniu kasuje stan testu i zaczyna od nowa.
//
// Uwaga: szkic zapisuje w partycji plików (0x820000 do 0x2000000). Nie dotyka aplikacji ani NVS z ustawieniami,
// ale to szkic na NOWY, pusty egzemplarz: gdy na panelu są pliki, odmawia i niczego nie zapisuje.
#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_flash.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <string.h>

namespace {

constexpr uint32_t GRANICA_16MB = 0x1000000;            // adresy od tego miejsca wymagają 4 bajtów adresu flash
constexpr uint32_t ROZMIAR_32MB = 32u * 1024u * 1024u;
constexpr uint32_t BLOK = 4096;                         // rozmiar sektora flash; wszystko w kawałkach po sektorze
constexpr uint32_t ROZMIAR_PLIKU = 20u * 1024u * 1024u; // plik 20 MB
constexpr uint8_t RESTARTY = 3;                         // tyle restartów wymaga pomiar 16
constexpr const char* PLIK = "/p148.bin";
constexpr uint32_t ZIARNO_SUROWY = 0x5A17A001u;
constexpr uint32_t ZIARNO_PLIK = 0xC0FFEE16u;

// Fazy zapisane w NVS PRZED długą operacją i zerowane po niej. Jeśli po starcie faza nie jest zerem, poprzedni
// przebieg urwał się w środku (watchdog, zanik zasilania, awaria sterownika) i to jest wynik testu.
enum Faza : uint8_t { F_BRAK = 0, F_SUROWY = 1, F_FORMAT = 2, F_ZAPIS = 3 };

Preferences nvs;
uint32_t bufor[BLOK / 4];
uint32_t bufor2[BLOK / 4];
String powod;  // opis ostatniego błędu, wypełniany tam, gdzie błąd powstaje

// Wzorzec deterministyczny: xorshift32 z ziarnem z numeru bloku. Przy surowym teście numer bloku to adres flash
// / 4096, więc gdyby adresy powyżej 16 MB zawijały się na dolne, zapisane dane nie zgadzałyby się z oczekiwanymi.
void wzorzec(uint32_t ziarno, uint32_t blok, uint32_t* wyj) {
  uint32_t s = (blok * 2654435761u) ^ ziarno ^ 0x9E3779B9u;
  if (s == 0) s = 1;
  for (uint32_t i = 0; i < BLOK / 4; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    wyj[i] = s;
  }
}

struct Skrot {
  mbedtls_sha256_context c;
  Skrot() {
    mbedtls_sha256_init(&c);
    mbedtls_sha256_starts(&c, 0);
  }
  ~Skrot() { mbedtls_sha256_free(&c); }
  void dodaj(const void* d, size_t n) { mbedtls_sha256_update(&c, static_cast<const unsigned char*>(d), n); }
  void koniec(uint8_t* wyj32) { mbedtls_sha256_finish(&c, wyj32); }
};

String szesnastkowo(const uint8_t* d, size_t n) {
  static const char* C = "0123456789abcdef";
  String s;
  s.reserve(n * 2);
  for (size_t i = 0; i < n; i++) {
    s += C[d[i] >> 4];
    s += C[d[i] & 15];
  }
  return s;
}

String sekundy(uint32_t ms) { return String(ms / 1000.0, 1) + " s"; }

const esp_partition_t* partycjaPlikow() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
}

// Czasy kolejnych etapów w jednym napisie w NVS: przeżywa restarty i trafia do wiersza końcowego.
void dopiszCzas(const char* nazwa, uint32_t ms) {
  String t = nvs.getString("czasy", "");
  t += String(nazwa) + "=" + sekundy(ms) + " ";
  nvs.putString("czasy", t);
}

bool kontrola() {
  uint32_t rozmiar = 0;
  if (esp_flash_get_size(nullptr, &rozmiar) != ESP_OK) {
    powod = "sterownik nie podał rozmiaru flash";
    return false;
  }
  nvs.putUInt("flash", rozmiar);
  Serial.printf("POMIAR16 flash wg sterownika: %u B (%u MB)\n", (unsigned)rozmiar, (unsigned)(rozmiar >> 20));
  // Bez tego zapis powyżej 16 MB sterownik i tak odrzuca; lepiej nazwać przyczynę wprost (zła definicja płytki).
  if (rozmiar < ROZMIAR_32MB) {
    powod = "sterownik zna tylko " + String(rozmiar >> 20) + " MB flash (definicja płytki 32 MB nie zadziałała)";
    return false;
  }
  const esp_partition_t* p = partycjaPlikow();
  if (!p) {
    powod = "brak partycji plików w tablicy partycji panelu";
    return false;
  }
  nvs.putUInt("part", p->size);
  Serial.printf("POMIAR16 partycja plików: adres 0x%X, rozmiar %u B (%.1f MB), koniec 0x%X\n", (unsigned)p->address,
                (unsigned)p->size, p->size / 1048576.0, (unsigned)(p->address + p->size));
  if (p->address >= GRANICA_16MB || p->address + p->size <= GRANICA_16MB) {
    powod = "partycja plików nie obejmuje obszaru powyżej 0x1000000 (to układ zapasowy 16 MB albo zła tablica)";
    return false;
  }
  uint32_t dolna = GRANICA_16MB - p->address;  // część partycji poniżej granicy
  Serial.printf("POMIAR16 poniżej 0x1000000: %u B, powyżej: %u B; plik %u B musi więc sięgać powyżej granicy "
                "(co najmniej %u B)\n",
                (unsigned)dolna, (unsigned)(p->size - dolna), (unsigned)ROZMIAR_PLIKU,
                (unsigned)(ROZMIAR_PLIKU > dolna ? ROZMIAR_PLIKU - dolna : 0));
  // Zabezpieczenie przed zawijaniem adresu: gdyby flash miał 16 MB, zapis pod 0x1000000 mógłby trafić w bootloader
  // (0x0). Najpierw tylko odczyt: jeśli oba miejsca dają te same dane, a to nie jest czysty flash, nie zapisujemy.
  uint8_t a[32], b[32];
  if (esp_flash_read(nullptr, a, 0, sizeof(a)) != ESP_OK ||
      esp_flash_read(nullptr, b, GRANICA_16MB, sizeof(b)) != ESP_OK) {
    powod = "odczyt flash pod adresem 0x1000000 zakończył się błędem";
    return false;
  }
  if (memcmp(a, b, sizeof(a)) == 0 && a[0] != 0xFF) {
    powod = "adres 0x1000000 zwraca te same dane co 0x0 (flash 16 MB zawija adresy), zapisu nie robiono";
    return false;
  }
  return true;
}

// Test surowy: obszar partycji powyżej granicy. Zapis przez esp_partition_*, więc niezależny od LittleFS: jeśli
// zawiedzie, wiadomo, że winien jest flash albo rdzeń, nie system plików.
bool testSurowy() {
  const esp_partition_t* p = partycjaPlikow();
  uint32_t od = GRANICA_16MB - p->address;
  uint32_t dl = p->size - od;
  Serial.printf("POMIAR16 test surowy: kasuję i zapisuję %u B od 0x%X do 0x%X ...\n", (unsigned)dl,
                (unsigned)GRANICA_16MB, (unsigned)(p->address + p->size));
  if (esp_partition_erase_range(p, od, dl) != ESP_OK) {
    powod = "kasowanie obszaru powyżej 0x1000000 nie powiodło się";
    return false;
  }
  for (uint32_t o = 0; o < dl; o += BLOK) {
    wzorzec(ZIARNO_SUROWY, (GRANICA_16MB + o) / BLOK, bufor);
    if (esp_partition_write(p, od + o, bufor, BLOK) != ESP_OK) {
      powod = "zapis surowy nie powiódł się pod adresem 0x" + String(GRANICA_16MB + o, HEX);
      return false;
    }
    if ((o & 0xFFFFF) == 0) {
      Serial.printf("POMIAR16 zapis surowy %u/%u MB\n", (unsigned)(o >> 20), (unsigned)(dl >> 20));
      delay(1);
    }
  }
  for (uint32_t o = 0; o < dl; o += BLOK) {
    wzorzec(ZIARNO_SUROWY, (GRANICA_16MB + o) / BLOK, bufor);
    if (esp_partition_read(p, od + o, bufor2, BLOK) != ESP_OK) {
      powod = "odczyt surowy nie powiódł się pod adresem 0x" + String(GRANICA_16MB + o, HEX);
      return false;
    }
    if (memcmp(bufor, bufor2, BLOK) != 0) {
      powod = "dane surowe różnią się pod adresem 0x" + String(GRANICA_16MB + o, HEX);
      return false;
    }
    if ((o & 0xFFFFF) == 0) {
      Serial.printf("POMIAR16 odczyt surowy %u/%u MB\n", (unsigned)(o >> 20), (unsigned)(dl >> 20));
      delay(1);
    }
  }
  return true;
}

bool zapiszPlik(uint8_t* skrot32) {
  File f = LittleFS.open(PLIK, "w");
  if (!f) {
    powod = "nie udało się otworzyć pliku do zapisu";
    return false;
  }
  Skrot h;
  for (uint32_t o = 0; o < ROZMIAR_PLIKU; o += BLOK) {
    wzorzec(ZIARNO_PLIK, o / BLOK, bufor);
    h.dodaj(bufor, BLOK);
    if (f.write(reinterpret_cast<uint8_t*>(bufor), BLOK) != BLOK) {
      powod = "zapis pliku urwał się na " + String(o) + " B (brak miejsca albo błąd flash)";
      f.close();
      return false;
    }
    if ((o & 0xFFFFF) == 0) {
      Serial.printf("POMIAR16 zapis pliku %u/%u MB\n", (unsigned)(o >> 20), (unsigned)(ROZMIAR_PLIKU >> 20));
      delay(1);
    }
  }
  f.close();
  h.koniec(skrot32);
  return true;
}

// Odczyt pliku: skrót z odczytanych bajtów i niezależne porównanie z wzorcem przeliczonym od nowa (łapie błąd
// także wtedy, gdy zapisany w NVS skrót byłby uszkodzony).
bool odczytajPlik(const uint8_t* oczekiwany, uint8_t* odczytany) {
  File f = LittleFS.open(PLIK, "r");
  if (!f) {
    powod = "plik testowy nie istnieje po restarcie";
    return false;
  }
  if (f.size() != ROZMIAR_PLIKU) {
    powod = "plik ma " + String((unsigned)f.size()) + " B zamiast " + String((unsigned)ROZMIAR_PLIKU);
    f.close();
    return false;
  }
  Skrot h;
  for (uint32_t o = 0; o < ROZMIAR_PLIKU; o += BLOK) {
    if (f.read(reinterpret_cast<uint8_t*>(bufor2), BLOK) != BLOK) {
      powod = "odczyt pliku urwał się na " + String(o) + " B";
      f.close();
      return false;
    }
    h.dodaj(bufor2, BLOK);
    wzorzec(ZIARNO_PLIK, o / BLOK, bufor);
    if (memcmp(bufor, bufor2, BLOK) != 0) {
      powod = "treść pliku różni się od wzorca w bloku na " + String(o) + " B";
      f.close();
      return false;
    }
    if ((o & 0xFFFFF) == 0) delay(1);
  }
  f.close();
  h.koniec(odczytany);
  if (memcmp(oczekiwany, odczytany, 32) != 0) {
    powod = "skrót SHA-256 odczytanego pliku różni się od zapisanego";
    return false;
  }
  return true;
}

void niezaliczone() {
  nvs.putString("wynik", "NIE ZALICZONE: " + powod);
  nvs.putUChar("faza", F_BRAK);
}

// Wiersz końcowy wypisywany po zakończeniu testu i potem co 10 s (wynik jest w NVS, więc nie trzeba go łapać na żywo).
void wypiszWynik() {
  String w = nvs.getString("wynik", "");
  Serial.println();
  Serial.println("POMIAR16 WYNIK: " + w);
  Serial.printf("POMIAR16 flash=%u B partycja plików=%u B plik=%u B restarty=%u\n", (unsigned)nvs.getUInt("flash", 0),
                (unsigned)nvs.getUInt("part", 0), (unsigned)ROZMIAR_PLIKU, (unsigned)nvs.getUChar("etap", 0));
  Serial.println("POMIAR16 skrót SHA-256 pliku: " + nvs.getString("hex", "(brak)"));
  Serial.println("POMIAR16 czasy: " + nvs.getString("czasy", ""));
  Serial.println("POMIAR16 (ponowny przebieg: wyślij znak R do portu)");
}

// Blokada: test kasuje całą partycję plików, a panele wyglądają tak samo. Pomyłka portu na panelu z nocami to utrata
// danych, więc przy jakimkolwiek pliku innym niż plik tego testu szkic odmawia i niczego nie zapisuje.
bool panelMaDane() {
  if (!LittleFS.begin(false)) return false;  // nie montuje się: nowy, pusty panel
  bool sa = false;
  File kat = LittleFS.open("/");
  for (File f = kat.openNextFile(); f; f = kat.openNextFile()) {
    if (String("/") + f.name() != PLIK && String(f.name()) != PLIK) {
      Serial.println(String("POMIAR16 plik na panelu: ") + f.name());
      sa = true;
    }
  }
  LittleFS.end();
  return sa;
}

void przebieg() {
  uint8_t skrot[32], odczyt[32];
  uint8_t etap = nvs.getUChar("etap", 0);  // ile restartów już zweryfikowano
  uint8_t faza = nvs.getUChar("faza", F_BRAK);
  Serial.printf("POMIAR16 start: etap %u z %u, powód resetu %d\n", (unsigned)etap, (unsigned)RESTARTY,
                (int)esp_reset_reason());

  if (faza != F_BRAK) {
    powod = "poprzedni przebieg urwał się w fazie " + String(faza) + " (1 surowy, 2 format, 3 zapis): reset w trakcie";
    niezaliczone();
    return;
  }

  if (!nvs.getBool("start", false)) {
    if (panelMaDane()) {
      powod = "na panelu są dane, testu nie uruchomiono i nic nie zapisano; to musi być nowy, pusty panel (wgraj z powrotem właściwy firmware)";
      niezaliczone();
      return;
    }
    nvs.putBool("start", true);
    nvs.putString("czasy", "");
    if (!kontrola()) {
      niezaliczone();
      return;
    }
    uint32_t t = millis();
    nvs.putUChar("faza", F_SUROWY);
    if (!testSurowy()) {
      niezaliczone();
      return;
    }
    nvs.putUChar("faza", F_BRAK);
    dopiszCzas("surowy16MB", millis() - t);

    // Pierwsze formatowanie partycji plików: begin(true) formatuje, gdy montowanie się nie udaje. Test surowy
    // zniszczył poprzednią zawartość, więc na tym etapie jest to zawsze świeże formatowanie.
    t = millis();
    nvs.putUChar("faza", F_FORMAT);
    LittleFS.end();
    if (!LittleFS.begin(true)) {
      powod = "montowanie z formatowaniem LittleFS nie powiodło się";
      niezaliczone();
      return;
    }
    nvs.putUChar("faza", F_BRAK);
    dopiszCzas("formatowanie", millis() - t);
    Serial.printf("POMIAR16 LittleFS: razem %u B, zajęte %u B\n", (unsigned)LittleFS.totalBytes(),
                  (unsigned)LittleFS.usedBytes());
    if (LittleFS.exists(PLIK)) LittleFS.remove(PLIK);

    t = millis();
    nvs.putUChar("faza", F_ZAPIS);
    if (!zapiszPlik(skrot)) {
      niezaliczone();
      return;
    }
    nvs.putUChar("faza", F_BRAK);
    dopiszCzas("zapis20MB", millis() - t);
    nvs.putBytes("skrot", skrot, 32);
    nvs.putString("hex", szesnastkowo(skrot, 32));

    t = millis();
    if (!odczytajPlik(skrot, odczyt)) {
      niezaliczone();
      return;
    }
    dopiszCzas("odczyt0", millis() - t);
    Serial.println("POMIAR16 zapis i odczyt w tym samym uruchomieniu: skrót zgodny " + nvs.getString("hex", ""));
    LittleFS.end();
  } else {
    // Uruchomienie po restarcie: zwykłe montowanie, BEZ formatowania. Jeśli się nie uda, to też wynik testu.
    if (!LittleFS.begin(false)) {
      powod = "po restarcie LittleFS się nie zamontował (partycja nie przeżyła restartu)";
      niezaliczone();
      return;
    }
    if (nvs.getBytes("skrot", skrot, 32) != 32) {
      powod = "w NVS brak zapisanego skrótu";
      niezaliczone();
      return;
    }
    uint32_t t = millis();
    if (!odczytajPlik(skrot, odczyt)) {
      niezaliczone();
      return;
    }
    etap++;
    nvs.putUChar("etap", etap);
    dopiszCzas(("odczyt" + String(etap)).c_str(), millis() - t);
    Serial.printf("POMIAR16 po restarcie %u z %u: skrót zgodny %s\n", (unsigned)etap, (unsigned)RESTARTY,
                  szesnastkowo(odczyt, 32).c_str());
    LittleFS.end();
  }

  if (nvs.getUChar("etap", 0) >= RESTARTY) {
    nvs.putString("wynik", "ZALICZONE");
    return;
  }
  Serial.println("POMIAR16 restart programowy za 2 s ...");
  delay(2000);
  ESP.restart();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);  // czas na otwarcie monitora po wgraniu
  Serial.println();
  Serial.println("POMIAR16 pomiar 16: flash powyżej 16 MB (szkic bez firmware panelu)");
  if (!nvs.begin("p148", false)) {
    Serial.println("POMIAR16 WYNIK: NIE ZALICZONE: NVS niedostępny");
    return;
  }
  if (nvs.getString("wynik", "").length() == 0) przebieg();
}

void loop() {
  if (Serial.available() > 0 && Serial.read() == 'R') {
    nvs.clear();
    Serial.println("POMIAR16 stan skasowany, restart i nowy przebieg");
    delay(500);
    ESP.restart();
  }
  static uint32_t ostatni = 0;
  if (millis() - ostatni >= 10000 || ostatni == 0) {
    ostatni = millis();
    if (nvs.getString("wynik", "").length() > 0) wypiszWynik();
  }
  delay(50);
}
