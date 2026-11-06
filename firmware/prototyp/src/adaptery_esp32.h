// SPDX-License-Identifier: GPL-3.0-or-later
// Adaptery panelu reTerminal E1001 (ESP32-S3, Arduino-ESP32 3.3.12 z ESP-IDF 5.5): jedyne miejsce z Arduino,
// ESP-IDF i bibliotekami sprzętu (ADR-0021 pkt 1–2, spec techniczna 6.4). Obejmuje też strażnik zadania (T-WYM-81),
// licznik startów w pamięci RTC bez zerowania (T-WYM-82) i stan wersji z powrotem do poprzedniej (T-WYM-41).
// Interfejsy i kontrakty: src/adaptery.h. Adapter niczego nie decyduje: decyzje są w rdzeniu (src/dyrygent.cpp).
// Kompilowany tylko przez PlatformIO; na komputerze tylko sprawdzenie składni z atrapami (tools/prototyp/testy.sh).
#pragma once
#include <Arduino.h>
#include "adaptery.h"

class ZegarEsp32 : public Zegar {
 public:
  uint32_t teraz() override;
  uint32_t ms() override;
  void czekaj(uint32_t ms) override;
  bool lokalny(uint32_t t, std::tm& wynik) override;
  uint32_t zLokalnego(const std::tm& lokalny) override;
  bool ustaw(uint32_t t) override;
};

class NvsEsp32 : public Nvs {
 public:
  std::string tekst(const char* przestrzen, const char* klucz, const std::string& domysl) override;
  bool ustawTekst(const char* przestrzen, const char* klucz, const std::string& v) override;
  uint32_t liczbaU(const char* przestrzen, const char* klucz, uint32_t domysl) override;
  void ustawLiczbeU(const char* przestrzen, const char* klucz, uint32_t v) override;
  int32_t liczba(const char* przestrzen, const char* klucz, int32_t domysl) override;
  void ustawLiczbe(const char* przestrzen, const char* klucz, int32_t v) override;
  bool flaga(const char* przestrzen, const char* klucz, bool domysl) override;
  void ustawFlage(const char* przestrzen, const char* klucz, bool v) override;
  size_t bajty(const char* przestrzen, const char* klucz, void* cel, size_t rozmiar) override;
  void ustawBajty(const char* przestrzen, const char* klucz, const void* zrodlo, size_t rozmiar) override;
  void usun(const char* przestrzen, const char* klucz) override;
};

class PlikiEsp32 : public Pliki {
 public:
  bool zamontuj() override;
  bool zamontowane() override { return montowane_; }
  bool istnieje(const char* nazwa) override;
  bool czytajWiersze(const char* nazwa, std::vector<std::string>& wiersze) override;
  bool zapiszAtomowo(const char* nazwa, const std::string& tresc) override;
  bool dopiszWiersz(const char* nazwa, const std::string& wiersz) override;
  bool pierwszyWiersz(const char* nazwa, std::string& wiersz) override;
  bool usun(const char* nazwa) override;
  bool zmienNazwe(const char* z, const char* na) override;
  void odmontuj() override;
  // czytaj, zapisz, listuj; partycja, czytajPartycje, formatuj: esp_partition_*,
  // LittleFS.format()). Kontrakty w src/adaptery.h.
  bool czytaj(const char* nazwa, std::string& tresc) override;
  size_t zapisz(const char* nazwa, const std::string& tresc) override;
  bool listuj(const char* katalog, std::vector<std::string>& nazwy) override;
  OpisPartycji partycja() override;
  bool czytajPartycje(size_t ile, std::string& bajty) override;
  bool formatuj() override;
  long long wolneMiejsce() override;

 private:
  bool montowane_ = false;
};

// Konfiguracja sieci: nazwa i hasło sieci domowej z NVS (src/sekrety.h), hasło okna z include/secrets.h.
struct KonfiguracjaSieci {
  std::string ssid;   // "" = pierwsza konfiguracja: polacz() od razu false, bez WiFi.begin()
  std::string haslo;
  const char* hasloOkna;  // OTA_PASS albo nullptr (bez hasła okna serwisowego nie ma, T-WYM-45)
};

class SiecEsp32 : public Siec {
 public:
  SiecEsp32(const KonfiguracjaSieci& k, PlikiEsp32& pliki) : k_(k), pliki_(pliki) {}
  bool polacz(uint32_t limitMs, const std::function<bool()>& przerwij, bool& przerwane) override;
  bool polaczona() override;
  void wylacz() override;
  std::string opisBledu() override;
  std::string opisPolaczenia() override;
  bool czasZSieci(uint32_t limitMs) override;
  OdpowiedzHttp get(const ZadanieHttp& z) override;
  bool otworzOknoSerwisowe() override;
  bool oknoSerwisoweOtwarte() override { return okno_; }
  void obsluzOknoSerwisowe() override;
  uint32_t losowa() override;
  std::string adresIp() override;
  std::string nazwaSieci() override;
  int rssi() override;
  Md5Hex md5() override;
  void ustawSiecDomowa(const std::string& ssid, const std::string& haslo) override {
    k_.ssid = ssid;
    k_.haslo = haslo;
  }
  bool wlaczRadio() override;
  bool uruchomSiecPanelu(const std::string& ssid, const std::string& haslo) override;
  std::string adresSieciPanelu() override;
  std::string idPanelu() override;

 private:
  KonfiguracjaSieci k_;
  PlikiEsp32& pliki_;
  bool okno_ = false;
  bool ap_ = false;  // sieć panelu działa
};

class WyswietlaczEsp32 : public Wyswietlacz {
 public:
  void wlacz() override;
  Plotno& plotno() override;
  void odswiez() override;
  const uint8_t* bufor() override;
};

class PrzyciskiEsp32 : public Przyciski {
 public:
  PrzyciskiEsp32();
  uint8_t wcisniete() override;
  void dioda(bool swieci) override;
  bool zKolejki(int& przycisk, uint32_t& trwaloMs) override;
  void wyczyscKolejke() override;
  bool piskPrzyNacisnieciu() const override { return true; }
};

class CzujnikiEsp32 : public Czujniki {
 public:
  OdczytCzujnikow czytaj() override;
};

class DzwiekEsp32 : public Dzwiek {
 public:
  void pisk(int ile) override;
};

class UruchomienieEsp32 : public Uruchomienie {
 public:
  explicit UruchomienieEsp32(const char* wersja);
  PrzyczynaStartu przyczyna() override;
  PowodWybudzenia powod() override;
  uint8_t przyciskiBudzace() override;
  LicznikStartow& licznik() override;
  StanRtc& rtc() override;
  const char* wersja() override { return wersja_; }
  std::string slot() override;
  StanWersji stanWersji() override;
  void oznaczWersjeDobra() override;
  void wrocDoPoprzedniej() override;

 private:
  const char* wersja_;
  StanRtc* rtc_;
};

class StraznikEsp32 : public Straznik {
 public:
  void zapisz(uint32_t limitS, bool panika) override;
  void odnow() override;
};

class SenEsp32 : public Sen {
 public:
  void spij(const PlanSnu& plan) override;
};

class PortEsp32 : public PortSerwisowy {
 public:
  void wiersz(const std::string& tekst) override;
  int znak() override;
};

class DiagnostykaEsp32 : public Diagnostyka {
 public:
  uint32_t minWolnaSterta() override;
  uint32_t zapasStosu() override;
};

// Piny i przygotowanie sprzętu przed dyrygentem, wołane z setup() w src/main.cpp (piny przycisków, UART, dioda,
// strefa czasu).
void przygotujPanel();
// Pierwsza czynność programu po wybudzeniu przyciskiem: krótki pisk, zanim ruszy reszta startu (R-PRZ-20, Z1).
void piskBudzenia();
// Awaryjny sen, gdy dyrygent wróci (na panelu tylko po nieudanym powrocie do poprzedniej wersji): 30 min albo
// przycisk, bez pętli bez snu.
void snAwaryjny();
