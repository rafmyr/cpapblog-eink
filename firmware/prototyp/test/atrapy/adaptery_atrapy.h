// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapy adapterów z src/adaptery.h z zachowaniem sterowanym z testu (T-WYM-02, spec techniczna 9.10, 11.1
// „integracja z atrapami”). Wspólny wirtualny czas (AtrapaZegara) i wspólny dziennik zdarzeń (Dziennik), żeby test
// sprawdzał kolejność wywołań między adapterami (strażnik przed siecią, odnowienie w pętli joba, zapis przed snem).
// Bez Arduino; tylko do testów na komputerze. Atrapy można rozszerzać w miarę potrzeb testów.
#pragma once
#include <cstring>
#include <ctime>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "przyciski_probki.h"
#include "adaptery.h"
#include "aktualizacja.h"
#include "magazyn.h"

// `sessions[].start_ms` / `end_ms` raportu AirBridge: sekundy kalendarza cywilnego aparatu (czas lokalny zakodowany jak
// UTC, bez strefy) × 1000. Testy podają chwilę unix `ep` (czas lokalny strefy TZ), a ta funkcja koduje ją jak AirBridge.
inline unsigned long long cywilneMs(uint32_t ep) {
  time_t t = (time_t)ep;
  std::tm l{};
  localtime_r(&t, &l);
  long long r = l.tm_year + 1900, m = l.tm_mon + 1, d = l.tm_mday;  // dni od 1970-01-01 (days_from_civil)
  r -= m <= 2;
  long long era = (r >= 0 ? r : r - 399) / 400, yoe = r - era * 400;
  long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long long dni = era * 146097 + doe - 719468;
  return (unsigned long long)(dni * 86400 + l.tm_hour * 3600 + l.tm_min * 60 + l.tm_sec) * 1000ULL;
}

// Dziennik zdarzeń wszystkich atrap: „adapter.zdarzenie argumenty”, w kolejności wywołań.
struct Dziennik {
  std::vector<std::string> z;
  void dodaj(const std::string& s) { z.push_back(s); }
  // Indeks pierwszego zdarzenia zaczynającego się od `p` albo -1.
  int pierwsze(const std::string& p) const {
    for (size_t i = 0; i < z.size(); i++)
      if (z[i].compare(0, p.size(), p) == 0) return (int)i;
    return -1;
  }
  int ile(const std::string& p) const {
    int n = 0;
    for (auto& s : z)
      if (s.compare(0, p.size(), p) == 0) n++;
    return n;
  }
};

class AtrapaZegara : public Zegar {
 public:
  explicit AtrapaZegara(Dziennik& d) : d_(d) {}
  uint32_t epoch = 0;   // czas unix w chwili ms = 0; 0 = zegar nieustawiony
  uint32_t msTeraz = 0; // zegar procesora (surowy: millis() panelu od startu programu)
  // ADR-0047 na panelu: ms() rdzenia liczy od początku przebiegu (od końca czekania na kablu), a adapter sieci ma
  // własny zegar procesora od startu programu. poczatekPrzebiegu = surowy czas początku przebiegu; 0 = jedna
  // podstawa czasu (wszystkie testy sprzed 22.11.2026). Atrapa sieci liczy czas żądania na surowe().
  uint32_t poczatekPrzebiegu = 0;
  uint32_t surowe() const { return msTeraz; }
  uint32_t teraz() override { return epoch ? epoch + msTeraz / 1000 : msTeraz / 1000; }
  uint32_t ms() override { return msTeraz - poczatekPrzebiegu; }
  void czekaj(uint32_t ms) override { msTeraz += ms; }
  bool lokalny(uint32_t t, std::tm& w) override {
    time_t tt = (time_t)t;
    return localtime_r(&tt, &w) != nullptr;  // test ustawia TZ na strefę panelu
  }
  uint32_t zLokalnego(const std::tm& l) override {
    std::tm k = l;
    k.tm_isdst = -1;
    time_t t = mktime(&k);
    return t < 0 ? 0 : (uint32_t)t;
  }
  int ustawienia = 0;  // ile razy rdzeń ustawił zegar (R-AW-18)
  bool ustaw(uint32_t t) override {
    ustawienia++;
    epoch = t - msTeraz / 1000;
    return true;
  }
  void przesun(uint32_t ms) { msTeraz += ms; }
 private:
  Dziennik& d_;
};

class AtrapaNvs : public Nvs {
 public:
  explicit AtrapaNvs(Dziennik& d) : d_(d) {}
  std::map<std::string, std::string> teksty;
  std::map<std::string, uint32_t> liczbyU;
  std::map<std::string, int32_t> liczby;
  std::map<std::string, bool> flagi;
  std::map<std::string, std::string> bajtyMap;
  int zapisy = 0;
  static std::string k(const char* p, const char* kl) { return std::string(p) + "/" + kl; }
  std::map<std::string, int> odczyty;  // liczba odczytów każdego klucza `p/kl` (tekst, liczby, flagi, bajty)
  std::string tekst(const char* p, const char* kl, const std::string& dom) override {
    odczyty[k(p, kl)]++;
    auto i = teksty.find(k(p, kl));
    return i == teksty.end() ? dom : i->second;
  }
  std::set<std::string> nieudaneTeksty;  // klucze `p/kl`, których zapis tekstu się nie udaje (pełna NVS)
  bool ustawTekst(const char* p, const char* kl, const std::string& v) override {
    if (nieudaneTeksty.count(k(p, kl))) return false;
    teksty[k(p, kl)] = v; zapis(p, kl); return true;
  }
  // Klucze `p/kl`, których odczyt liczby się nie udaje: liczbaU() zwraca wartość domyślną (jak Preferences na panelu),
  // liczbaUStan() zwraca BLAD (ADR-0046). Poza tym liczbaUStan() idzie przez wersję
  // z Nvs (dwa odczyty liczbaU()), żeby klasy pochodne w testach przypadków brzegowych, które podmieniają tylko liczbaU(), działały.
  std::set<std::string> bledneOdczytyU;
  uint32_t liczbaU(const char* p, const char* kl, uint32_t dom) override {
    odczyty[k(p, kl)]++;
    if (bledneOdczytyU.count(k(p, kl))) return dom;
    auto i = liczbyU.find(k(p, kl));
    return i == liczbyU.end() ? dom : i->second;
  }
  Odczyt liczbaUStan(const char* p, const char* kl, uint32_t& v) override {
    if (bledneOdczytyU.count(k(p, kl))) return Odczyt::BLAD;
    return Nvs::liczbaUStan(p, kl, v);
  }
  void ustawLiczbeU(const char* p, const char* kl, uint32_t v) override { liczbyU[k(p, kl)] = v; zapis(p, kl); }
  int32_t liczba(const char* p, const char* kl, int32_t dom) override {
    odczyty[k(p, kl)]++;
    auto i = liczby.find(k(p, kl));
    return i == liczby.end() ? dom : i->second;
  }
  void ustawLiczbe(const char* p, const char* kl, int32_t v) override { liczby[k(p, kl)] = v; zapis(p, kl); }
  bool flaga(const char* p, const char* kl, bool dom) override {
    odczyty[k(p, kl)]++;
    auto i = flagi.find(k(p, kl));
    return i == flagi.end() ? dom : i->second;
  }
  void ustawFlage(const char* p, const char* kl, bool v) override { flagi[k(p, kl)] = v; zapis(p, kl); }
  size_t bajty(const char* p, const char* kl, void* cel, size_t n) override {
    odczyty[k(p, kl)]++;
    auto i = bajtyMap.find(k(p, kl));
    if (i == bajtyMap.end() || i->second.size() != n) return 0;
    memcpy(cel, i->second.data(), n);
    return n;
  }
  void ustawBajty(const char* p, const char* kl, const void* z, size_t n) override {
    bajtyMap[k(p, kl)] = std::string((const char*)z, n); zapis(p, kl);
  }
  void usun(const char* p, const char* kl) override {
    std::string x = k(p, kl);
    teksty.erase(x); liczbyU.erase(x); liczby.erase(x); flagi.erase(x); bajtyMap.erase(x);
    zapis(p, kl);
  }
 private:
  void zapis(const char* p, const char* kl) { zapisy++; d_.dodaj("nvs.zapis " + k(p, kl)); }
  Dziennik& d_;
};

class AtrapaPlikow : public Pliki {
 public:
  explicit AtrapaPlikow(Dziennik& d) : d_(d) {}
  std::map<std::string, std::string> pliki;  // nazwa → treść (wiersze z '\n')
  bool montujeSie = true, montowane = false, odmawiaZapisu = false;
  int zapisy = 0;  // każda zmiana treści: zapis atomowy, dopisanie, usunięcie, zmiana nazwy
  // ---------- wstrzykiwanie błędów i partycja (testy test/testy_magazyn_danych.cpp) ----------
  // Ścieżki zaczynające się od tego przedrostka nie dają się zapisać, usunąć ani przemianować (np. "/dane/" =
  // „atrapa odmawia zapisu nowych plików”, T-WYM-43, T-WYM-56); "" = bez odmowy.
  std::string odmawiaZapisuPod;
  // Pełna pamięć (T-WYM-51): zapisz() zapisuje najwyżej tyle bajtów; -1 = bez limitu.
  long wolneBajty = -1;
  // Zanik zasilania (R-NF-12): po tylu bajtach zapisanych przez zapisz() i dopiszWiersz() zasilanie znika; od tej
  // chwili żadna operacja zmieniająca pliki nie działa (do „restartu”: zanik = false, zanikPoBajtach = -1).
  long zanikPoBajtach = -1;
  bool zanik = false;
  // (T-TEST-515): ścieżki, których plik jest (istnieje() true), a odczyt się nie udaje (czytaj(),
  // czytajWiersze(), pierwszyWiersz() → false, jak LittleFS.open() == nullptr albo plik ponad limit adaptera); zapis
  // działa. Usterka trwała, dopóki ścieżka jest w zbiorze.
  std::set<std::string> nieczytelne;
  // zachowanie panelu przed poprawką adaptera (LittleFS.exists() = próba otwarcia): true = istnieje() też
  // false dla ścieżek z `nieczytelne` (plik nadal widoczny w listuj()). Testy usterki biegną w obu trybach.
  bool istniejeJakOtwarcie = false;
  // (T-TEST-515): ścieżki z tym przedrostkiem nie dają się dopisać (dopiszWiersz() → false, treść bez zmian:
  // pełna pamięć albo błąd nośnika przy dopisaniu); "" = bez odmowy. Zapis atomowy działa.
  std::string odmawiaDopisaniaPod;
  std::map<std::string, int> odczyty;  // liczba odczytów każdego pliku (czytaj(), czytajWiersze(), pierwszyWiersz())
  std::map<std::string, int> pierwsze;     // z tego pierwszyWiersz() (ADR-0047, koszt bez pamięci list, 17.11.2026)
  std::map<std::string, int> listowania;  // wywołania listuj() każdego katalogu (jw.)
  // Partycja: dwa pierwsze bloki (2 × 4096 B) i opis z tablicy panelu; domyślnie poprawny superblok i układ
  // wkompilowany w firmware.
  std::string poczatek = superblok();
  OpisPartycji opis = domyslnyOpis();
  bool partycjaCzytelna = true;  // czytajPartycje() się udaje
  bool formatujeSie = true;      // formatuj() się udaje
  int formatowan = 0;
  static const size_t BLOK = 4096;
  // Dwa bloki z superblokiem LittleFS (napis `littlefs` od bajtu 8 każdego bloku, jak lfs_superblock).
  static std::string superblok() {
    std::string b(2 * BLOK, '\0');
    for (size_t k = 0; k < 2; k++) b.replace(k * BLOK + 8, 8, "littlefs");
    return b;
  }
  static OpisPartycji domyslnyOpis() {
    OpisPartycji o;
    o.przesuniecie = 0x820000;
    o.rozmiar = 0x17E0000;
    o.uklad = ukladWkompilowany();
    return o;
  }
  bool zamontuj() override { d_.dodaj("pliki.zamontuj"); montowane = montujeSie; return montowane; }
  bool zamontowane() override { return montowane; }
  bool istnieje(const char* n) override {
    return montowane && pliki.count(n) && !(istniejeJakOtwarcie && nieczytelne.count(n));
  }
  bool czytajWiersze(const char* n, std::vector<std::string>& w) override {
    w.clear();
    if (!montowane) return false;
    odczyty[n]++;
    if (nieczytelne.count(n) && pliki.count(n)) return false;
    std::string tmp = std::string(n) + ".tmp";
    if (!pliki.count(n) && pliki.count(tmp)) { pliki[n] = pliki[tmp]; pliki.erase(tmp); }
    auto i = pliki.find(n);
    if (i == pliki.end()) return false;
    size_t a = 0;
    while (a < i->second.size()) {
      size_t b = i->second.find('\n', a);
      if (b == std::string::npos) b = i->second.size();
      std::string l = i->second.substr(a, b - a);
      while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
      if (!l.empty()) w.push_back(l);
      a = b + 1;
    }
    return true;
  }
  bool zapiszAtomowo(const char* n, const std::string& t) override {
    d_.dodaj(std::string("pliki.zapis ") + n);
    if (!montowane || odmawiaZapisu || odmowa(n)) return false;
    zapisy++; pliki[n] = t; return true;
  }
  bool dopiszWiersz(const char* n, const std::string& w) override {
    d_.dodaj(std::string("pliki.dopisz ") + n);
    if (!montowane || odmawiaZapisu || odmowa(n)) return false;
    if (!odmawiaDopisaniaPod.empty() && std::string(n).compare(0, odmawiaDopisaniaPod.size(), odmawiaDopisaniaPod) == 0)
      return false;
    std::string& t = pliki[n];
    if (!t.empty() && t.back() != '\n') t += '\n';
    std::string dop = w + "\n";
    size_t ile = doZaniku(dop.size());
    t += dop.substr(0, ile);
    zapisy++;
    return ile == dop.size();
  }
  bool pierwszyWiersz(const char* n, std::string& w) override {
    if (montowane) pierwsze[n]++;
    std::vector<std::string> v;
    if (!czytajWiersze(n, v) || v.empty()) return false;
    w = v[0];
    return true;
  }
  bool usun(const char* n) override {
    d_.dodaj(std::string("pliki.usun ") + n);
    if (!montowane || odmawiaZapisu || odmowa(n)) return false;
    zapisy++; return pliki.erase(n) > 0;
  }
  bool zmienNazwe(const char* z, const char* na) override {
    d_.dodaj(std::string("pliki.nazwa ") + z + " " + na);
    if (!montowane || odmawiaZapisu || odmowa(z) || odmowa(na) || !pliki.count(z)) return false;
    zapisy++; pliki[na] = pliki[z]; pliki.erase(z); return true;
  }
  void odmontuj() override { montowane = false; }
  bool czytaj(const char* n, std::string& t) override {
    t.clear();
    if (!montowane) return false;
    odczyty[n]++;
    auto i = pliki.find(n);
    if (i == pliki.end() || nieczytelne.count(n)) return false;
    t = i->second;
    return true;
  }
  size_t zapisz(const char* n, const std::string& t) override {
    d_.dodaj(std::string("pliki.zapisz ") + n);
    if (!montowane || odmawiaZapisu || odmowa(n)) return 0;
    size_t ile = t.size();
    if (wolneBajty >= 0 && ile > (size_t)wolneBajty) ile = (size_t)wolneBajty;
    ile = doZaniku(ile);
    zapisy++;
    pliki[n] = t.substr(0, ile);
    return ile;
  }
  bool listuj(const char* k, std::vector<std::string>& nazwy) override {
    nazwy.clear();
    if (!montowane) return false;
    listowania[k]++;
    std::string p = std::string(k);
    if (p.empty() || p.back() != '/') p += '/';
    for (auto& kv : pliki)
      if (kv.first.compare(0, p.size(), p) == 0 && kv.first.find('/', p.size()) == std::string::npos)
        nazwy.push_back(kv.first);
    return !nazwy.empty();
  }
  OpisPartycji partycja() override { return opis; }
  bool czytajPartycje(size_t ile, std::string& b) override {
    b.clear();
    if (!partycjaCzytelna) return false;
    b = poczatek.substr(0, ile);
    return true;
  }
  bool formatuj() override {
    d_.dodaj("pliki.formatuj");
    formatowan++;
    if (!formatujeSie) return false;
    pliki.clear();
    poczatek = superblok();
    montujeSie = true;
    montowane = true;
    return true;
  }
  // U-05: wolne = limit `wolneBajty`, gdy ustawiony, inaczej rozmiar partycji z `opis` minus treść plików.
  long long wolneMiejsce() override {
    if (!montowane) return -1;
    if (wolneBajty >= 0) return wolneBajty;
    long long z = 0;
    for (auto& kv : pliki) z += (long long)kv.second.size();
    return z >= (long long)opis.rozmiar ? 0 : (long long)opis.rozmiar - z;
  }

  // Treść pliku albo "" (wygoda testów).
  std::string tresc(const std::string& n) const {
    auto i = pliki.find(n);
    return i == pliki.end() ? std::string() : i->second;
  }
 private:
  bool odmowa(const char* n) {
    if (zanik) return true;
    return !odmawiaZapisuPod.empty() && std::string(n).compare(0, odmawiaZapisuPod.size(), odmawiaZapisuPod) == 0;
  }
  // Ile z `ile` bajtów zapisze się przed zanikiem zasilania (i ustawia zanik, gdy budżet się skończył).
  size_t doZaniku(size_t ile) {
    if (zanik) return 0;
    if (zanikPoBajtach < 0) return ile;
    if (ile > (size_t)zanikPoBajtach) ile = (size_t)zanikPoBajtach;
    zanikPoBajtach -= (long)ile;
    if (zanikPoBajtach == 0) zanik = true;
    return ile;
  }
  Dziennik& d_;
};

// Odpowiedź atrapy sieci na żądanie, którego URL kończy się (bez parametrów) ścieżką `sciezka` albo ma ją jako
// przedrostek ścieżki. Kolejka: każda odpowiedź raz, ostatnia zostaje na kolejne żądania.
struct OdpowiedzAtrapy {
  OdpowiedzHttp o;
  uint32_t opoznienieMs = 100;  // ile trwa żądanie (przesuwa wirtualny czas)
};
class AtrapaSieci : public Siec {
 public:
  AtrapaSieci(Dziennik& d, AtrapaZegara& z) : d_(d), z_(z) {}
  bool siecJest = true, ntpJest = true, hasloOkna = true;
  uint32_t czasPolaczeniaMs = 1500;
  uint32_t czasNtp = 0;  // czas unix ustawiany przez NTP (0 = bez zmiany zegara)
  std::map<std::string, std::deque<OdpowiedzAtrapy>> odpowiedzi;  // przedrostek ścieżki (np. "/api/status")
  std::vector<std::string> zadania;  // URL-e wszystkich GET
  std::vector<std::vector<std::pair<std::string, std::string>>> naglowki;  // nagłówki każdego GET (jak zadania)
  int wywolania = 0;                 // każde wywołanie poza polaczona() i opis*()
  bool polaczonaTeraz = false, okno = false;
  void dodaj(const std::string& sciezka, int kod, const std::string& cialo, uint32_t opoznienieMs = 100) {
    OdpowiedzAtrapy a;
    a.o.kod = kod;
    a.o.cialo = cialo;
    a.o.contentLength = (int)cialo.size();
    a.o.wynikZapisu = (int)cialo.size();
    a.opoznienieMs = opoznienieMs;
    odpowiedzi[sciezka].push_back(a);
  }
  // sieć domowa ustawiona przez dyrygenta (ustawSiecDomowa); `widoczne` niepuste = łączy się tylko z sieciami
  // o tych nazwach i hasłach (nazwa → hasło); puste = z każdą (testy sprzed).
  std::string ssid, haslo;
  std::map<std::string, std::string> widoczne;
  int ustawieniaSieci = 0;
  void ustawSiecDomowa(const std::string& s, const std::string& h) override { ssid = s; haslo = h; ustawieniaSieci++; }
  bool polacz(uint32_t limitMs, const std::function<bool()>&, bool& przerwane) override {
    wywolania++; przerwane = false;
    d_.dodaj("siec.polacz");
    apTeraz = false;  // tryb STA kończy sieć panelu
    bool jest = siecJest && (widoczne.empty() || (widoczne.count(ssid) && widoczne[ssid] == haslo));
    z_.przesun(jest ? czasPolaczeniaMs : limitMs);
    polaczonaTeraz = jest;
    return jest;
  }
  bool polaczona() override { return polaczonaTeraz; }
  void wylacz() override {
    if (polaczonaTeraz) wywolania++;
    d_.dodaj("siec.wylacz");
    polaczonaTeraz = false;
    okno = false;
    radio = false;
    if (apTeraz) d_.dodaj("siec.ap.stop");
    apTeraz = false;
  }
  // sieć panelu (AP). `apMozliwy` = false: radio nie startuje punktu dostępu. `losowe` niepuste:
  // kolejne wyniki losowa() (inaczej stała 0x12345678); losowaPrzyRadiu liczy wywołania przy włączonym radiu.
  bool apMozliwy = true, apTeraz = false, radio = false;
  std::string apSsid, apHaslo;
  int apStartow = 0, losowychBezRadia = 0;
  std::deque<uint32_t> losowe;
  bool wlaczRadio() override {
    wylacz();
    radio = true;
    d_.dodaj("siec.radio");
    return true;
  }
  bool uruchomSiecPanelu(const std::string& s, const std::string& h) override {
    polaczonaTeraz = false;
    if (!apMozliwy || h.size() < 8 || h.size() > 63) return false;
    apTeraz = radio = true;
    apSsid = s;
    apHaslo = h;
    apStartow++;
    d_.dodaj("siec.ap.start " + s);  // bez hasła (R-KONF-38)
    return true;
  }
  std::string adresSieciPanelu() override { return apTeraz ? "192.168.4.1" : ""; }
  std::string idPanelu() override { return "A1B2"; }
  std::string opisBledu() override { return "atrapa: brak sieci"; }
  std::string opisPolaczenia() override { return "192.168.1.50 · RSSI -60 dBm"; }
  bool czasZSieci(uint32_t limitMs) override {
    wywolania++; d_.dodaj("siec.ntp");
    if (!polaczonaTeraz || !ntpJest) { z_.przesun(limitMs); return false; }
    z_.przesun(200);
    if (czasNtp) z_.epoch = czasNtp - z_.msTeraz / 1000;
    return true;
  }
  OdpowiedzHttp get(const ZadanieHttp& zad) override {
    wywolania++;
    zadania.push_back(zad.url);
    naglowki.push_back(zad.naglowki);
    d_.dodaj("siec.get " + zad.url);
    OdpowiedzHttp brak;
    brak.kod = -1;  // HTTPC_ERROR_CONNECTION_REFUSED
    if (!polaczonaTeraz) return brak;
    std::string sc = sciezka(zad.url);
    const std::string* najlepszy = nullptr;
    for (auto& kv : odpowiedzi)
      if (sc.compare(0, kv.first.size(), kv.first) == 0 && (!najlepszy || kv.first.size() > najlepszy->size()))
        najlepszy = &kv.first;
    if (!najlepszy || odpowiedzi[*najlepszy].empty()) { z_.przesun(zad.limitMs); brak.kod = -11; return brak; }
    auto& q = odpowiedzi[*najlepszy];
    OdpowiedzAtrapy a = q.front();
    if (q.size() > 1) q.pop_front();
    const uint32_t start = z_.surowe();  // jak t0 = millis() w SiecEsp32::get()
    z_.przesun(a.opoznienieMs);
    a.o.ms = a.opoznienieMs;
    // kontrakt Siec::get(): przy 401 bez ciała; przy cialoTylko200 ciało tylko przy 200; ciało ponad limit nieczytane
    if (a.o.kod == 401 || a.o.kod <= 0 || (zad.cialoTylko200 && a.o.kod != 200)) {
      a.o.cialo.clear();
      a.o.wynikZapisu = 0;
    } else if (zad.maksBajtow && a.o.cialo.size() > zad.maksBajtow) {
      a.o.cialo.clear();
      a.o.zaDuzo = true;
    } else if (!a.o.cialo.empty() && budzetCialaMinal(zad.budzetCialaMs, start, z_.surowe())) {
      // jak adapter panelu (SiecEsp32::get(), DoNapisu::stop()): pierwszy bajt ciała po budżecie odczytu, liczonym
      // od wejścia w get() na zegarze adaptera, kończy odczyt (writeToStream: błąd zapisu)
      a.o.cialo.clear();
      a.o.wynikZapisu = -10;  // HTTPC_ERROR_STREAM_WRITE
      a.o.zaDlugo = true;
    }
    return a.o;
  }
  bool otworzOknoSerwisowe() override {
    wywolania++; d_.dodaj("siec.okno");
    okno = hasloOkna && polaczonaTeraz;
    return okno;
  }
  bool oknoSerwisoweOtwarte() override { return okno; }
  void obsluzOknoSerwisowe() override {}
  uint32_t losowa() override {
    if (!radio && !polaczonaTeraz) losowychBezRadia++;
    if (losowe.empty()) return 0x12345678u;
    uint32_t x = losowe.front();
    losowe.pop_front();
    losowe.push_back(x * 2654435761u + 0x9E3779B9u);  // dalej różne wartości
    return x;
  }
  std::string adresIp() override { return polaczonaTeraz ? "192.168.1.50" : ""; }
  // U-05 (R-KONF-41): nazwa sieci i sygnał jak w opisPolaczenia
  std::string nazwaSieci() override { return polaczonaTeraz ? "Dom" : ""; }
  int rssi() override { return polaczonaTeraz ? -60 : 0; }
  // MD5 atrapy: stały skrót (testy dyrygenta nie sprawdzają Digest; Digest ma testy w test/testy.cpp).
  static std::string md5Atrapy(const std::string&) { return "0123456789abcdef0123456789abcdef"; }
  Md5Hex md5() override { return md5Atrapy; }
  // Ścieżka z parametrami z URL („http://host/api/report?x” → „/api/report?x”).
  static std::string sciezka(const std::string& url) {
    size_t a = url.find("://");
    a = url.find('/', a == std::string::npos ? 0 : a + 3);
    return a == std::string::npos ? "/" : url.substr(a);
  }
 private:
  Dziennik& d_;
  AtrapaZegara& z_;
};

// Płótno w pamięci 800×480 (bit 1 = czarny), jak test/podglad.cpp PlotnoPamiec.
struct PlotnoAtrapy : Plotno {
  std::vector<uint8_t> b = std::vector<uint8_t>(800 / 8 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x < 0 || y < 0 || x >= 800 || y >= 480) return;
    uint8_t m = (uint8_t)(0x80 >> (x & 7));
    if (c) b[(size_t)(y * 100 + x / 8)] |= m;
    else b[(size_t)(y * 100 + x / 8)] &= (uint8_t)~m;
  }
};
class AtrapaWyswietlacza : public Wyswietlacz {
 public:
  explicit AtrapaWyswietlacza(Dziennik& d) : d_(d) {}
  PlotnoAtrapy p;
  int odswiezen = 0;
  std::vector<std::vector<uint8_t>> klatki;  // bufor przy każdym odświeżeniu
  // R-NF-25: rodzaj każdego odświeżenia (ten sam indeks co `klatki`); `czesciowePozwala` = false udaje sterownik
  // bez odświeżenia częściowego (adapter wykonuje pełne i zwraca PELNE)
  std::vector<Odswiezenie> rodzaje;
  bool czesciowePozwala = true;
  // ADR-0047: `pamietaObraz` = adapter jak na panelu przy czekaniu na kablu (kopia obrazu przeżywa przebieg bez
  // głębokiego snu); AtrapaSnu::spij() (głęboki sen) ją kasuje (zapomnij()). Domyślnie false: jak dotąd.
  bool pamietaObraz = false;
  void zapomnij() { znany_ = false; }
  bool poprzedniObrazZnany() override { return pamietaObraz && znany_; }
  void wlacz() override { d_.dodaj("ekran.wlacz"); }
  Plotno& plotno() override { return p; }
  void odswiez() override {
    odswiezen++;
    klatki.push_back(p.b);
    rodzaje.push_back(biezacy_);
    biezacy_ = Odswiezenie::PELNE;
    znany_ = true;
    d_.dodaj("ekran.odswiez");
  }
  Odswiezenie odswiezJako(Odswiezenie rodzaj) override {
    biezacy_ = czesciowePozwala ? rodzaj : Odswiezenie::PELNE;
    Odswiezenie wykonane = biezacy_;
    odswiez();  // podklasy testów liczą klatki w odswiez()
    return wykonane;
  }
  const uint8_t* bufor() override { return p.b.data(); }
 private:
  Dziennik& d_;
  Odswiezenie biezacy_ = Odswiezenie::PELNE;
  bool znany_ = false;
};

// Przyciski ze skryptem: stan od chwili (ms zegara procesora); przed pierwszym wpisem nic nie wciśnięte.
class AtrapaPrzyciskow : public Przyciski {
 public:
  explicit AtrapaPrzyciskow(AtrapaZegara& z) : z_(z) {}
  std::vector<std::pair<uint32_t, uint8_t>> skrypt;  // (od ms, bity wciśniętych), rosnąco
  bool swieci = false;
  void dioda(bool s) override { swieci = s; }
  uint8_t wcisniete() override {
    uint8_t s = 0;
    for (auto& p : skrypt)
      if (z_.msTeraz >= p.first) s = p.second;
    return s;
  }
  // kolejka naciśnięć z czasu rysowania (spec 5.6.1): (numer jak (int)Przycisk, ms trzymania), najstarsze pierwsze
  std::vector<std::pair<int, uint32_t>> kolejka;
  int wyczyszczen = 0;
  bool zKolejki(int& przycisk, uint32_t& trwaloMs) override {
    if (kolejka.empty()) return false;
    przycisk = kolejka.front().first;
    trwaloMs = kolejka.front().second;
    kolejka.erase(kolejka.begin());
    return true;
  }
  void wyczyscKolejke() override {
    kolejka.clear();
    wyczyszczen++;
  }
  // kolejka tej atrapy to naciśnięcia już puszczone przed przebiegiem (ze skryptu nic do niej nie trafia), więc
  // naciśnięcie odczytane ze stanu nigdy w niej nie jest: tylko licznik
  int obsluzonychZeStanu = 0;
  void obsluzoneZeStanu(int przycisk) override {
    (void)przycisk;
    obsluzonychZeStanu++;
  }
 private:
  AtrapaZegara& z_;
};

// Przyciski panelu z próbkowaniem (R-PRZ-20): ten sam rdzeń co PrzyciskiEsp32 (src/przyciski_probki.h),
// stan surowy z odcinków wciśnięcia [od, do) w ms zegara procesora, próbka co 5 ms liczona
// przy każdym wywołaniu do chwili zegara; naciśnięcie w czasie, gdy rdzeń liczy albo rysuje (atrapa przesuwa wtedy
// zegar), trafia do kolejki jak na panelu. Pisk adaptera w chwili naciśnięcia liczony w `piskow`
// (piskPrzyNacisnieciu() = true jak na panelu). Czas modelu = ms * 1000 + PRZESUNIECIE_US (0 w modelu = „nieznana”).
class AtrapaPrzyciskowProbkowanych : public Przyciski {
 public:
  explicit AtrapaPrzyciskowProbkowanych(AtrapaZegara& z) : kolejka(*this), z_(z) {}
  static constexpr int64_t PRZESUNIECIE_US = 1000000;
  struct Odcinek {
    uint32_t od, doMs;
    uint8_t bit;
  };
  struct Wpis {
    int przycisk;
    uint32_t trwaloMs, puszczonyMs;
  };
  // widok kolejki modelu (testy: empty(), size(), push_back() naciśnięcia zostawionego przez adapter)
  class Kolejka {
   public:
    explicit Kolejka(AtrapaPrzyciskowProbkowanych& a) : a_(a) {}
    bool empty() const { return a_.m_.ile() == 0; }
    size_t size() const { return (size_t)a_.m_.ile(); }
    void push_back(const Wpis& w) {
      przyciski_probki::Nacisniecie n;
      n.przycisk = (uint8_t)w.przycisk;
      n.trwaloMs = w.trwaloMs;
      n.puszczonyUs = (int64_t)w.puszczonyMs * 1000 + PRZESUNIECIE_US;
      a_.m_.dopisz(n);
    }
   private:
    AtrapaPrzyciskowProbkowanych& a_;
  };
  std::vector<Odcinek> odcinki;
  Kolejka kolejka;
  bool swieci = false;
  int obsluzonychZeStanu = 0, wyczyszczen = 0, piskow = 0;
  // wciśnięcie `bit` od `od` do `doMs` (ms zegara procesora); `od` nie przed chwilą ostatniej policzonej próbki
  void nacisnij(uint8_t bit, uint32_t od, uint32_t doMs) { odcinki.push_back({od, doMs, bit}); }
  // start programu w chwili zegara (jak konstruktor PrzyciskiEsp32)
  // `przedPrzebiegiem`: przyciski wciśnięte w chwili startu trzymane już przed początkiem przebiegu (czekanie na kablu,
  // który ich nie zakończył): chwila wciśnięcia sprzed przebiegu, bez `pomin`; inaczej jak po głębokim śnie
  void start(bool przedPrzebiegiem = false) {
    probka_ = z_.msTeraz;
    m_.start(surowe(z_.msTeraz));
    for (int i = 1; przedPrzebiegiem && i <= 3; i++)
      if (m_.klawisz(i).wcisniety) {
        m_.klawisz(i).pomin = false;
        m_.klawisz(i).odUs = us(z_.msTeraz) - 2000000;
      }
  }
  uint8_t wcisniete() override {
    probkuj();
    return m_.wcisniete();
  }
  void dioda(bool s) override { swieci = s; }
  bool zKolejki(int& przycisk, uint32_t& trwaloMs) override {
    probkuj();
    return m_.zKolejki(przycisk, trwaloMs);
  }
  void wyczyscKolejke() override {
    probkuj();
    wyczyszczen++;
    m_.wyczysc();
  }
  void obsluzoneZeStanu(int przycisk) override {
    probkuj();
    obsluzonychZeStanu++;
    m_.obsluzone(przycisk, us(z_.msTeraz));
  }
  bool piskPrzyNacisnieciu() const override { return true; }
  bool wcisnietyOd(int przycisk, uint32_t& odMs) override {
    probkuj();
    int64_t od = 0;  // początek przebiegu = ms 0 atrapy (PRZESUNIECIE_US w modelu)
    return m_.wcisnietyOd(przycisk, od) &&
           przyciski_probki::chwilaWPrzebiegu(od, (uint32_t)(PRZESUNIECIE_US / 1000), odMs);
  }
  przyciski_probki::Model& model() { return m_; }

 private:
  przyciski_probki::Model m_;
  AtrapaZegara& z_;
  uint32_t probka_ = 0;
  static int64_t us(uint32_t ms) { return (int64_t)ms * 1000 + PRZESUNIECIE_US; }
  uint8_t surowe(uint32_t t) const {
    uint8_t b = 0;
    for (const Odcinek& o : odcinki)
      if (t >= o.od && t < o.doMs) b = (uint8_t)(b | o.bit);
    return b;
  }
  void probkuj() {
    for (; probka_ <= z_.msTeraz; probka_ += 5) {
      przyciski_probki::ZmianyProbki z = m_.probka(surowe(probka_), us(probka_));
      for (int i = 1; i <= 3; i++)
        if (z.piski & (1u << i)) piskow++;
    }
  }
};

class AtrapaCzujnikow : public Czujniki {
 public:
  OdczytCzujnikow o{21.5f, 45.0f, 3.95f};
  OdczytCzujnikow czytaj() override { return o; }
  Zasilanie zas = Zasilanie::NIEZNANE;  // R-DANE-65: stan zasilania zewnętrznego, który „widzi” panel
  int odczytowZasilania = 0;
  // ADR-0047: >= 0 = od odczytu o tym numerze (licząc od 1) zasilanie z baterii (kabel odłączony w trakcie
  // przebiegu albo czekania); -1 = bez zmiany.
  int bateriaOdOdczytu = -1;
  // true = każdy odczyt zasilania czyta też z `lad` REG0B, REG08 i REG0A jak CzujnikiEsp32::zasilanie() na tej samej
  // magistrali Wire1 (test K w testy_kabel_bez_snu.cpp: odczyty co 10 s w czekaniu obok zapisu limitu ładowania).
  // Stan zasilania dalej z `zas`. Domyślnie false: liczniki odczytów układu w innych testach bez zmian.
  bool zasilanieZUkladu = false;
  OdczytZasilania zasilanie() override {
    odczytowZasilania++;
    if (bateriaOdOdczytu >= 0 && odczytowZasilania >= bateriaOdOdczytu) {
      zas = Zasilanie::BATERIA;
      bateriaOdOdczytu = -1;
    }
    OdczytZasilania odczyt;
    odczyt.stan = zas;
    if (zasilanieZUkladu && lad) {
      uint8_t w = 0;
      if (lad->czytaj(0x0B, w) && lad->czytaj(0x08, w)) odczyt.rejStanu = w;
      if (lad->czytaj(0x0A, w)) odczyt.rejWejscia = w;
    }
    return odczyt;
  }
  ladowarka::Rejestry* lad = nullptr;  // ADR-0046: atrapa układu ładowarki (AtrapaLadowarki), nullptr = bez układu
  ladowarka::Rejestry* rejestryLadowarki() override { return lad; }
};

// Model rejestrów ładowarki SY6974B (ADR-0046) według karty Rev. 0.9 (src/ladowarka.h [K]) i karty SGM41512 [S]:
// wartości po starcie zasilania, tryb domyślny i host (każdy zapis = host), upływ licznika watchdoga w trybie host
// przywraca wartości domyślne (poza REG00, REG06, REG07, jak w [S]); REG08…REG0A tylko do odczytu.
// Wstrzykiwanie błędów: brak odpowiedzi, nieudany n-ty zapis, rejestr, który potwierdza zapis, ale go nie przyjmuje.
class AtrapaLadowarki : public ladowarka::Rejestry {
 public:
  uint8_t r[12] = {};
  bool host = false;
  bool odpowiada = true;
  std::set<uint8_t> nieczytelne;  // rejestry, których odczyt się nie udaje
  int nieudanyZapisNr = -1;       // numer zapisu (od 1), który się nie udaje (bez zmiany rejestru); -1 = brak
  int nieprzyjmuje = -1;          // rejestr, który potwierdza zapis, ale zostaje bez zmian; -1 = brak
  std::vector<std::pair<uint8_t, uint8_t>> zapisy;
  int odczytow = 0;
  AtrapaLadowarki() { startZasilania(); }
  // Wartości po starcie zasilania [K]: REG04 01011 000 (4208 mV), REG05 1 0 01 1 1 1 1 (WATCHDOG 40 s), REG09 bit 7
  // WATCHDOG_FAULT (tryb domyślny), REG0B PN 1000. Pozostałe rejestry: wartości przykładowe (logika ich nie czyta).
  void startZasilania() {
    uint8_t d[12] = {0x17, 0x1A, 0xA2, 0x22, 0x58, 0x9F, 0xE6, 0x4C, 0x00, 0x80, 0x00, 0x40};
    for (int i = 0; i < 12; i++) r[i] = d[i];
    host = false;
  }
  // Upływ licznika w trybie host z włączonym licznikiem: tryb domyślny, wartości domyślne poza REG00, REG06, REG07.
  void uplywWatchdoga() {
    if (!host || (r[5] & 0x30) == 0) return;
    uint8_t r0 = r[0], r6 = r[6], r7 = r[7];
    startZasilania();
    r[0] = r0;
    r[6] = r6;
    r[7] = r7;
  }
  bool czytaj(uint8_t rej, uint8_t& w) override {
    odczytow++;
    if (!odpowiada || rej > 0x0B || nieczytelne.count(rej)) return false;
    w = r[rej];
    return true;
  }
  bool zapisz(uint8_t rej, uint8_t w) override {
    if (!odpowiada || rej > 0x0B) return false;
    zapisy.push_back({rej, w});
    if ((int)zapisy.size() == nieudanyZapisNr) return false;
    host = true;
    if (rej >= 0x08 && rej <= 0x0A) return true;  // tylko do odczytu
    if ((int)rej != nieprzyjmuje) r[rej] = w;
    return true;
  }
};

class AtrapaDzwieku : public Dzwiek {
 public:
  int piskow = 0;
  void pisk(int n) override { piskow += n; }
};

// Identyfikator wersji atrapy jak na panelu (src/main.cpp: `{WERSJA}+{skrót commitu}`, numer wydania,
// bez „proto”), żeby symulator pokazywał na U-05 to samo co firmware; slot jak etykieta w firmware/partycje-e1001.csv.
static const char* const WERSJA_ATRAPY = "1.0.0+abc1234";

class AtrapaUruchomienia : public Uruchomienie {
 public:
  explicit AtrapaUruchomienia(Dziennik& d) : d_(d) {}
  PrzyczynaStartu przyczynaStartu = PrzyczynaStartu::USPIENIE;
  PowodWybudzenia powodWybudzenia = PowodWybudzenia::ZEGAR;
  uint8_t budzace = 0;
  LicznikStartow l{};
  StanRtc stan;
  const char* napisWersji = WERSJA_ATRAPY;
  StanWersji stanW = StanWersji::DOBRA;
  bool oznaczona = false, powrot = false;
  PrzyczynaStartu przyczyna() override { d_.dodaj("start.przyczyna"); return przyczynaStartu; }
  PowodWybudzenia powod() override { return powodWybudzenia; }
  uint8_t przyciskiBudzace() override { return budzace; }
  uint32_t trwanieBudzacego = 0;  // ADR-0047 (17.11.2026): naciśnięcie budzące już puszczone, ms; 0 = trzymane
  uint32_t trwanieBudzacegoMs() override { return trwanieBudzacego; }
  LicznikStartow& licznik() override { return l; }
  StanRtc& rtc() override { return stan; }
  const char* wersja() override { return napisWersji; }
  std::string slot() override { return "app0"; }
  StanWersji stanWersji() override { return stanW; }
  void oznaczWersjeDobra() override { d_.dodaj("start.oznacz"); oznaczona = true; stanW = StanWersji::DOBRA; }
  void wrocDoPoprzedniej() override { d_.dodaj("start.powrot"); powrot = true; }
 private:
  Dziennik& d_;
};

// Strażnik na wirtualnym czasie: przekroczenie = przerwa między odnowieniami dłuższa niż limit (sprawdzana przy
// każdym odnowieniu i przy wypisaniu stanu).
class AtrapaStraznika : public Straznik {
 public:
  AtrapaStraznika(Dziennik& d, AtrapaZegara& z) : d_(d), z_(z) {}
  bool zapisany = false, panika = false;
  uint32_t limitS = 0;
  uint32_t ostatnieMs = 0, najdluzszaPrzerwaMs = 0;
  int odnowien = 0, przekroczen = 0;
  void zapisz(uint32_t l, bool p) override {
    d_.dodaj("straznik.zapisz " + std::to_string(l) + (p ? " panika" : ""));
    zapisany = true; limitS = l; panika = p; ostatnieMs = z_.msTeraz;
  }
  void odnow() override {
    d_.dodaj("straznik.odnow");
    sprawdz();
    odnowien++;
    ostatnieMs = z_.msTeraz;
  }
  void sprawdz() {
    if (!zapisany) return;
    uint32_t p = z_.msTeraz - ostatnieMs;
    if (p > najdluzszaPrzerwaMs) najdluzszaPrzerwaMs = p;
    if (p > limitS * 1000) przekroczen++;
  }
 private:
  Dziennik& d_;
  AtrapaZegara& z_;
};

class AtrapaSnu : public Sen {
 public:
  AtrapaSnu(Dziennik& d, AtrapaUruchomienia& u, AtrapaStraznika& s, AtrapaWyswietlacza* e = nullptr)
      : d_(d), u_(u), s_(s), e_(e) {}
  int snow = 0;
  PlanSnu plan;
  uint8_t licznikPrzySnie = 0xFF;  // LicznikStartow::poBledzie w chwili snu
  void spij(const PlanSnu& p) override {
    s_.sprawdz();
    d_.dodaj("sen");
    snow++; plan = p; licznikPrzySnie = u_.l.poBledzie;
    if (e_) e_->zapomnij();  // głęboki sen: kopia obrazu w RAM adaptera ginie
  }
  // ADR-0047: czekanie na kablu. `czekanieWlaczone` = adapter umie czekać (jak panel); koniec czekania: `koniec`
  // (ZEGAR albo PRZYCISK; stan startu następnego przebiegu ustawia test, jak przy głębokim śnie). `przerwij` wołane
  // `sprawdzen` razy (jak co KAWALEK_CZEKANIA_S na panelu), pierwsze true kończy czekanie z PRZERWANE.
  bool czekanieWlaczone = false;
  int czekan = 0, przerwanych = 0, sprawdzen = 1;
  WynikCzekania::Rodzaj koniec = WynikCzekania::Rodzaj::ZEGAR;
  std::function<void()> wCzekaniu;  // wołane na początku czekania (np. test odłącza kabel)
  bool czekanieDostepne() override { return czekanieWlaczone; }
  WynikCzekania czekaj(const PlanSnu& p, const std::function<bool()>& przerwij) override {
    s_.sprawdz();
    d_.dodaj("czekanie");
    WynikCzekania w;
    if (!czekanieWlaczone) return w;
    czekan++; plan = p; licznikPrzySnie = u_.l.poBledzie;
    if (wCzekaniu) wCzekaniu();
    for (int i = 0; i < sprawdzen; i++)
      if (przerwij && przerwij()) {
        przerwanych++;
        w.rodzaj = WynikCzekania::Rodzaj::PRZERWANE;
        w.zostaloS = p.sekund;
        return w;
      }
    w.rodzaj = koniec;
    return w;
  }
 private:
  Dziennik& d_;
  AtrapaUruchomienia& u_;
  AtrapaStraznika& s_;
  AtrapaWyswietlacza* e_;
};

class AtrapaPortu : public PortSerwisowy {
 public:
  explicit AtrapaPortu(Dziennik& d) : d_(d) {}
  std::vector<std::string> log;
  std::deque<int> znaki;
  void wiersz(const std::string& t) override { log.push_back(t); d_.dodaj("port " + t); }
  int znak() override {
    if (znaki.empty()) return -1;
    int c = znaki.front(); znaki.pop_front(); return c;
  }
 private:
  Dziennik& d_;
};

class AtrapaDiagnostyki : public Diagnostyka {
 public:
  uint32_t sterta = 70000, stos = 5000;
  uint32_t sekundy = 0;              // ADR-0047: 0 = nieznane (bez sprawdzenia)
  uint32_t psram = UINT32_MAX;       // ADR-0047 (17.11.2026): największy blok PSRAM; UINT32_MAX = nieznany
  uint32_t blokWewn = UINT32_MAX;    // ADR-0047 (17.11.2026): największy blok pamięci wewnętrznej; UINT32_MAX = nieznany
  uint32_t minWolnaSterta() override { return sterta; }
  uint32_t zapasStosu() override { return stos; }
  uint32_t najwiekszyBlokPsram() override { return psram; }
  uint32_t najwiekszyBlokWewn() override { return blokWewn; }
  uint32_t sekundOdStartu() override { return sekundy; }
};

// Atrapa wydań GitHub (ADR-0032, ADR-0038): odpowiedź `releases/latest`, `wydanie.json` i obraz z pamięci.
// Żądanie zasobu binarnego przechodzi przez jedno przekierowanie na HOST_PLIKOW (jak GitHub); nagłówki każdego
// żądania liczy rdzeń (aktualizacja::naglowki), atrapa je zapisuje, więc test widzi, czy token poszedł po przekierowaniu.
class AtrapaWydan : public Wydania {
 public:
  AtrapaWydan(Dziennik& d, AtrapaZegara& z) : d_(d), z_(z) {}
  static constexpr const char* HOST_PLIKOW = "objects.githubusercontent.com";
  struct Zadanie {
    std::string url, host;
    std::vector<std::pair<std::string, std::string>> naglowki;
  };
  std::vector<Zadanie> zadania;     // każde żądanie (także po przekierowaniu)
  int kodApi = 200, kodOpisu = 200, kodObrazu = 200;  // kody odpowiedzi; ujemny = brak połączenia
  bool certyfikatZly = false;       // serwer spoza zestawu certyfikatów: żądanie nie wychodzi
  bool przekierowanieNaHttp = false;  // przekierowanie na http:// (adapter go nie przyjmuje)
  std::string odpowiedzApi;         // ciało `releases/latest`
  // R-KONF-62 (kanał wczesny): ciało i kod `releases?per_page=…` (lista wydań); pusta = `[odpowiedzApi]`
  std::string odpowiedzListy;
  int kodListy = 200;
  std::string opis;                 // ciało `wydanie.json`
  std::string obraz;                // bajty obrazu
  size_t uciecObrazPo = 0;          // >0: połączenie zrywa się po tylu bajtach
  bool startowyOk = true;           // esp_ota_end: obraz przyjęty
  int restartow = 0, porzucen = 0, ustawien = 0;
  bool slotStartowyZmieniony = false;
  uint64_t slot = aktualizacja::SLOT_APLIKACJI;
  uint32_t msZadania = 300, msObrazu = 60000;

  // Wydanie `wersja` z obrazem `bajty` (opis zgodny z panelem przy `uklad` = ukladWkompilowany()).
  // `nowosci`: pole „Co nowego” opisu (R-KONF-56); puste = wydanie bez pola, jak wydania sprzed 14.11.2026.
  void ustaw(const std::string& wersja, const std::string& bajty, uint32_t uklad, const std::string& plytka = PLYTKA_WYDANIA,
             const std::vector<std::string>& nowosci = {}) {
    obraz = bajty;
    aktualizacja::OpisWydania o;
    o.nowosci = nowosci;
    o.wersja = wersja;
    o.plytka = plytka;
    o.uklad = uklad;
    o.rozmiar = bajty.size();
    o.sha256 = aktualizacja::sha256(bajty);
    o.plik = aktualizacja::nazwaObrazu(wersja);
    opis = aktualizacja::zapiszOpis(o);
    std::string api = std::string("https://api.github.com/repos/") + REPO_WYDAN + "/releases/assets/";
    odpowiedzApi = "{\"tag_name\":\"v" + wersja + "\",\"name\":\"v" + wersja +
                   "\",\"body\":\"Nota \\u0142\",\"assets\":[{\"id\":11,\"name\":\"wydanie.json\",\"url\":\"" + api +
                   "11\",\"size\":" + std::to_string(opis.size()) + "},{\"id\":12,\"name\":\"" + o.plik +
                   "\",\"url\":\"" + api + "12\",\"size\":" + std::to_string(bajty.size()) + "}]}";
  }
  bool tokenPoszedlDo(const std::string& host) const {
    for (auto& z : zadania)
      if (z.host == host)
        for (auto& h : z.naglowki)
          if (h.first == "Authorization") return true;
    return false;
  }
  OdpowiedzWydania pobierzTekst(const ZadanieWydania& zw) override {
    OdpowiedzWydania o;
    d_.dodaj("wydania.tekst " + zw.url);
    if (certyfikatZly) { o.certyfikatOk = false; o.kod = -1; return o; }
    bool lista = zw.url.find("/releases?") != std::string::npos;
    bool api = lista || zw.url.find("/releases/latest") != std::string::npos;
    int kod = lista ? kodListy : api ? kodApi : kodOpisu;
    zapiszZadanie(zw.url, zw.token, zw.binarne);
    o.hosty.push_back(aktualizacja::hostZAdresu(zw.url));
    if (!api && zw.binarne && kod > 0) {  // zasób: przekierowanie na serwer plików
      std::string u = std::string(przekierowanieNaHttp ? "http://" : "https://") + HOST_PLIKOW + "/x?token=jednorazowy";
      if (przekierowanieNaHttp) { o.kod = -2; return o; }
      zapiszZadanie(u, zw.token, true);
      o.hosty.push_back(HOST_PLIKOW);
    }
    z_.przesun(msZadania);
    o.kod = kod;
    if (kod == 200) {
      o.cialo = lista ? (odpowiedzListy.empty() ? "[" + odpowiedzApi + "]" : odpowiedzListy) : api ? odpowiedzApi : opis;
      if (zw.maksBajtow && o.cialo.size() > zw.maksBajtow) { o.cialo.clear(); o.zaDuzo = true; }
    }
    return o;
  }
  WynikObrazu pobierzObraz(const ZadanieWydania& zw, uint64_t rozmiar, const std::function<void()>& postep) override {
    WynikObrazu w;
    d_.dodaj("wydania.obraz " + zw.url);
    if (certyfikatZly) { w.http.certyfikatOk = false; w.http.kod = -1; return w; }
    zapiszZadanie(zw.url, zw.token, true);
    w.http.hosty.push_back(aktualizacja::hostZAdresu(zw.url));
    std::string u = std::string("https://") + HOST_PLIKOW + "/obraz";
    zapiszZadanie(u, zw.token, true);
    w.http.hosty.push_back(HOST_PLIKOW);
    w.http.kod = kodObrazu;
    z_.przesun(msObrazu);
    if (kodObrazu != 200 || rozmiar > slot) return w;
    std::string b = uciecObrazPo ? obraz.substr(0, uciecObrazPo) : obraz;
    aktualizacja::Sha256 s;
    for (size_t i = 0; i < b.size(); i += 4096) {
      size_t n = std::min<size_t>(4096, b.size() - i);
      s.dodaj((const uint8_t*)b.data() + i, n);
      if (postep) postep();
    }
    w.bajtow = b.size();
    w.sha256 = s.hex();
    w.zapisOk = w.bajtow == rozmiar;
    zapisane = w.zapisOk;
    return w;
  }
  bool ustawStartowy() override {
    ustawien++;
    d_.dodaj("wydania.startowy");
    if (!zapisane || !startowyOk) return false;
    slotStartowyZmieniony = true;
    return true;
  }
  void porzuc() override { porzucen++; zapisane = false; d_.dodaj("wydania.porzuc"); }
  void restart() override { restartow++; d_.dodaj("wydania.restart"); }
  uint64_t rozmiarSlotu() override { return slot; }

 private:
  void zapiszZadanie(const std::string& url, const std::string& token, bool bin) {
    zadania.push_back({url, aktualizacja::hostZAdresu(url), aktualizacja::naglowki(url, token, bin)});
  }
  bool zapisane = false;
  Dziennik& d_;
  AtrapaZegara& z_;
};

// Cały komplet atrap jednego panelu, z ustawieniami jak zwykłe wybudzenie zegarem o 07:00 w dobie testu.
struct Panel {
  Dziennik d;
  AtrapaZegara zegar{d};
  AtrapaNvs nvs{d};
  AtrapaPlikow pliki{d};
  AtrapaSieci siec{d, zegar};
  AtrapaWyswietlacza ekran{d};
  AtrapaPrzyciskow przyciski{zegar};
  AtrapaCzujnikow czujniki;
  AtrapaDzwieku dzwiek;
  AtrapaUruchomienia start{d};
  AtrapaStraznika straznik{d, zegar};
  AtrapaSnu sen{d, start, straznik, &ekran};
  AtrapaPortu port{d};
  AtrapaDiagnostyki diag;
  AtrapaWydan wydania{d, zegar};
  Adaptery adaptery() {
    return Adaptery{zegar, nvs, pliki, siec, ekran, przyciski, czujniki, dzwiek, start, straznik, sen, port, diag, &wydania};
  }
};
