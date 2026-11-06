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
#include <string>
#include <vector>
#include "adaptery.h"
#include "magazyn.h"

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
  uint32_t msTeraz = 0; // zegar procesora
  uint32_t teraz() override { return epoch ? epoch + msTeraz / 1000 : msTeraz / 1000; }
  uint32_t ms() override { return msTeraz; }
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
  std::string tekst(const char* p, const char* kl, const std::string& dom) override {
    auto i = teksty.find(k(p, kl));
    return i == teksty.end() ? dom : i->second;
  }
  bool ustawTekst(const char* p, const char* kl, const std::string& v) override {
    teksty[k(p, kl)] = v; zapis(p, kl); return true;
  }
  uint32_t liczbaU(const char* p, const char* kl, uint32_t dom) override {
    auto i = liczbyU.find(k(p, kl));
    return i == liczbyU.end() ? dom : i->second;
  }
  void ustawLiczbeU(const char* p, const char* kl, uint32_t v) override { liczbyU[k(p, kl)] = v; zapis(p, kl); }
  int32_t liczba(const char* p, const char* kl, int32_t dom) override {
    auto i = liczby.find(k(p, kl));
    return i == liczby.end() ? dom : i->second;
  }
  void ustawLiczbe(const char* p, const char* kl, int32_t v) override { liczby[k(p, kl)] = v; zapis(p, kl); }
  bool flaga(const char* p, const char* kl, bool dom) override {
    auto i = flagi.find(k(p, kl));
    return i == flagi.end() ? dom : i->second;
  }
  void ustawFlage(const char* p, const char* kl, bool v) override { flagi[k(p, kl)] = v; zapis(p, kl); }
  size_t bajty(const char* p, const char* kl, void* cel, size_t n) override {
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
  std::map<std::string, int> odczyty;  // liczba odczytów każdego pliku (czytaj(), czytajWiersze(), pierwszyWiersz())
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
  bool istnieje(const char* n) override { return montowane && pliki.count(n); }
  bool czytajWiersze(const char* n, std::vector<std::string>& w) override {
    w.clear();
    if (!montowane) return false;
    odczyty[n]++;
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
    std::string& t = pliki[n];
    if (!t.empty() && t.back() != '\n') t += '\n';
    std::string dop = w + "\n";
    size_t ile = doZaniku(dop.size());
    t += dop.substr(0, ile);
    zapisy++;
    return ile == dop.size();
  }
  bool pierwszyWiersz(const char* n, std::string& w) override {
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
    if (i == pliki.end()) return false;
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
    z_.przesun(a.opoznienieMs);
    a.o.ms = a.opoznienieMs;
    // kontrakt Siec::get(): przy 401 bez ciała; przy cialoTylko200 ciało tylko przy 200; ciało ponad limit nieczytane
    if (a.o.kod == 401 || a.o.kod <= 0 || (zad.cialoTylko200 && a.o.kod != 200)) {
      a.o.cialo.clear();
      a.o.wynikZapisu = 0;
    } else if (zad.maksBajtow && a.o.cialo.size() > zad.maksBajtow) {
      a.o.cialo.clear();
      a.o.zaDuzo = true;
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
  void wlacz() override { d_.dodaj("ekran.wlacz"); }
  Plotno& plotno() override { return p; }
  void odswiez() override { odswiezen++; klatki.push_back(p.b); d_.dodaj("ekran.odswiez"); }
  const uint8_t* bufor() override { return p.b.data(); }
 private:
  Dziennik& d_;
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
 private:
  AtrapaZegara& z_;
};

class AtrapaCzujnikow : public Czujniki {
 public:
  OdczytCzujnikow o{21.5f, 45.0f, 3.95f};
  OdczytCzujnikow czytaj() override { return o; }
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
  AtrapaSnu(Dziennik& d, AtrapaUruchomienia& u, AtrapaStraznika& s) : d_(d), u_(u), s_(s) {}
  int snow = 0;
  PlanSnu plan;
  uint8_t licznikPrzySnie = 0xFF;  // LicznikStartow::poBledzie w chwili snu
  void spij(const PlanSnu& p) override {
    s_.sprawdz();
    d_.dodaj("sen");
    snow++; plan = p; licznikPrzySnie = u_.l.poBledzie;
  }
 private:
  Dziennik& d_;
  AtrapaUruchomienia& u_;
  AtrapaStraznika& s_;
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
  uint32_t minWolnaSterta() override { return sterta; }
  uint32_t zapasStosu() override { return stos; }
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
  AtrapaSnu sen{d, start, straznik};
  AtrapaPortu port{d};
  AtrapaDiagnostyki diag;
  Adaptery adaptery() {
    return Adaptery{zegar, nvs, pliki, siec, ekran, przyciski, czujniki, dzwiek, start, straznik, sen, port, diag};
  }
};
