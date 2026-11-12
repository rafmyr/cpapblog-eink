// SPDX-License-Identifier: GPL-3.0-or-later
// Symulator doby: prawdziwy dyrygent (src/dyrygent.h,
// ten sam kod co na panelu) złożony z atrapami z test/atrapy/adaptery_atrapy.h. Wejście: katalog zrzutu plików panelu
// (format tools/prototyp/zrzut_plikow.py, wersja 0 z płaskimi plikami albo wersja 1 lub 2 z /dane/…), okres i skrypt
// przycisków. Wyjście: klatka PBM każdego odświeżenia ekranu, log.txt (jeden wiersz na wybudzenie) i log-panelu.txt
// (wszystkie wiersze portu serwisowego z godziną). Uruchamia go tools/symulator/symuluj.py, który kompiluje, zamienia
// PBM na PNG i pilnuje, żeby dane zdrowotne nie trafiły do repozytorium (WY12).
//
// Zasady symulacji:
//  - pliki zrzutu wczytane do atrapy pamięci (kopia w pamięci programu; katalog zrzutu tylko czytany); pliki wersji 0
// pod starymi ścieżkami (`/noce.csv` …), migrację do wersji 1 robi firmware przy starcie (magazyn);
//  - zegar sterowany przez symulator: pierwsze wybudzenie = start zasilania o `--od`, każde następne o chwili z planu
//    snu firmware (Sen::spij(): PlanSnu), chyba że wcześniej wypada naciśnięcie przycisku ze skryptu;
//  - AirBridge (atrapa): terapia nie trwa (`/api/status` spoczynek, tryb ASVAuto), lista nocy = noce zrzutu kompletne
//    z AirBridge (źródło A), więc brak nowych nocy do pobrania; raport doby → 404 (nie powinien być potrzebny);
//    `--bez-sieci` = brak Wi-Fi;
//  - zegar panelu jak na panelu: po starcie zasilania nieznany, aż NTP się uda (potem chodzi w śnie);
//    `--zegar-znany` = panel, który już chodził i zna czas (np. test braku Wi-Fi z działającym zegarem);
// `--bez-ntp` = internet zablokowany w routerze: NTP milczy, a `/api/status` AirBridge podaje `esp_time`
//    (czas lokalny symulacji, do minuty), więc panel bierze czas z AirBridge (R-AW-18);
//  - bez lokalnego źródła otoczenia; czujnik przy łóżku stały (21,5 °C, 45 %, 3,95 V);
//  - przycisk: krótkie naciśnięcie trzymane 300 ms, `2s` = 2500 ms (próg R-PRZ-08 to 2000 ms).
// Od (scenariusze, obrazki dnia, prognoza; tools/symulator/README.md):
//  - `--lat`, `--lon`: współrzędne miasta (KonfiguracjaPanelu::pogodaLat/Lon, na panelu POGODA_LAT/LON z secrets.h);
//  - `--prognoza PLIK`: atrapa Open-Meteo. Plik od symuluj.py: 48 wierszy `g;temp;opad;chmury;porywy;pm25;brzoza;
//    trawy;olcha;bylica` (godzina 0…47 od północy dnia wybudzenia, puste = null). Każde wybudzenie dostaje odpowiedź
//    `/v1/forecast` i `/v1/air-quality` z datami dnia wybudzenia i następnego (kształt odpowiedzi serwisu);
//  - `--otoczenie PLIK`: atrapa lokalnego źródła otoczenia (OTO_URL w sieci domowej, kontrakt wersji 1) z CO2 sypialni:
//    wiersze `HH:MM;co2` (wartość od tej godziny do następnego wiersza, codziennie);
//  - `--obrazki KATALOG`: pliki `.pbm` i `obrazki.json` z katalogu do atrapy pamięci pod `/obrazki/` (S-K-38);
//  - `--noce-scenariusza PLIK`: wiersze nocy `noce` wersji 2 (41 pól, wymyślone przez scenariusz). Noc z poranka D
//    trafia do `/dane/noce/RRRR.csv` i na listę AirBridge o 04:30 dnia D (noce do --od od razu, przed startem);
//  - `--airbridge-milczy-od CZAS`: od tej chwili AirBridge nie odpowiada (Wi-Fi jest; scenariusz `brak-danych`);
//  - `--opis TEKST` (można kilka razy): wiersz opisu scenariusza w nagłówku log.txt;
//  - `--otoczenie-od CZAS`, `--otoczenie-do CZAS`: profil CO2 tylko w tym okresie, poza nim źródło podaje 650 ppm
// (CO2 scenariusza `co2` tylko w jego dniach).
// Od (scenariusze):
//  - `--konfiguracja PLIK`: wiersze `klucz=wartość` (np. `wymiana.poduszka=2026-03-01`) scalone z `/konfiguracja.txt`
//    kopii zrzutu przed startem (klucz zastąpiony albo dopisany);
//  - `--czynniki-scenariusza PLIK`: wiersze `czynniki` (`data;stan;kody;warstwa;termin;pokazano`, sprawdzone parserem
//    firmware) w `/dane/czynniki.csv` przed startem: odpowiedzi „Co było inaczej?” na noce historii scenariusza
//    (scenariusz podaje tylko noce sprzed --od);
//  - `--airbridge-przerwa OD,DO` (można kilka razy): AirBridge milczy w tym okresie, Wi-Fi jest (wyjazd z aparatem);
//    noce scenariusza z tego czasu dochodzą do pamięci panelu w pierwszym wybudzeniu po powrocie;
//  - `--noc-opozniona RRRR-MM-DD,HH:MM` (można kilka razy): noc tego poranka dochodzi o tej godzinie zamiast 04:30,
//    więc w oknie porannym jej brak i panel czyta listę nocy AirBridge (pełny odczyt, R-CZY-19 w src/dyrygent.cpp);
//  - `--aparat PNA,TRYB`: model i tryb aparatu tylko w atrapie `/api/status` (konfiguracja panelu jest taka jak na prawdziwym panelu) (scenariusze AutoSet
//    i CPAP);
//  - nazwy klatek `norma` (S8 z K1), `liczby`, `sprzet`, `sprzet-zapisano`.
// Przebieg z nocami scenariusza: każdy obrazek ma przedrostek `SYN-` i znak wodny `DANE SYNTETYCZNE` w prawym
// górnym rogu (tylko w symulatorze, firmware go nie rysuje); po każdej zmianie ostatniej nocy w pamięci panelu wiersz
// `SYMULATOR ostatnia noc w pamięci panelu: …` (data, tryb, M-01, czy syntetyczna) w log-panelu.txt.
//
// Użycie: symulator --dane KATALOG --od RRRR-MM-DDTHH:MM --do RRRR-MM-DDTHH:MM --wyjscie KATALOG
//                    [--przyciski PLIK] [--bez-sieci] [--zegar-znany] [--bez-ntp] [--lat X --lon Y] [--prognoza PLIK]
//                    [--otoczenie PLIK] [--obrazki KATALOG] [--noce-scenariusza PLIK] [--airbridge-milczy-od CZAS]
//                    [--opis TEKST]…
// Plik przycisków (już sprawdzony przez symuluj.py): wiersze `RRRR-MM-DDTHH:MM[:SS] lewy|srodkowy|prawy [2s]`.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "czynniki.h"
#include "dyrygent.h"
#include "ekrany.h"
#include "magazyn.h"
#include "reguly.h"
#include "nawigacja.h"
#include "noc.h"
#include "serwer_raportu.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

namespace fs = std::filesystem;

namespace {

const char* const STREFA = "CET-1CEST,M3.5.0,M10.5.0/3";  // jak STREFA w src/adaptery_esp32.cpp
const char* const STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
const uint32_t TRZYMANIE_KROTKIE_MS = 300;
const uint32_t TRZYMANIE_DLUGIE_MS = 2500;
const uint32_t OKNO_SKRYPTU_S = 30;          // naciśnięcia tyle sekund po początku wybudzenia trafiają do jego skryptu
const size_t NAJWIEKSZY_PLIK = 8u << 20;     // większy plik zrzutu jest pomijany (to nie są pliki panelu)
// bezpiecznik przeciw zapętleniu: najwyżej jedno wybudzenie na minutę okresu plus zapas (plan firmware budzi rzadziej)
const uint32_t ZAPAS_WYBUDZEN = 1000;

void blad(const std::string& s) {
  fprintf(stderr, "BŁĄD: %s\n", s.c_str());
  exit(2);
}

// Liczba z rzeczownikiem w polskiej odmianie: „1 plik”, „2 pliki”, „5 plików”.
std::string ile(size_t n, const char* jeden, const char* kilka, const char* wiele) {
  size_t d = n % 10, s = n % 100;
  const char* r = n == 1 ? jeden : (d >= 2 && d <= 4 && (s < 12 || s > 14)) ? kilka : wiele;
  return std::to_string(n) + " " + r;
}

// „RRRR-MM-DDTHH:MM[:SS]” w strefie panelu → czas unix; 0 = zły zapis.
uint32_t czasZTekstu(const std::string& s) {
  int r = 0, mi = 0, d = 0, h = 0, m = 0, sek = 0;
  char t = 0;
  int n = sscanf(s.c_str(), "%4d-%2d-%2d%c%2d:%2d:%2d", &r, &mi, &d, &t, &h, &m, &sek);
  if (n < 6 || (t != 'T' && t != ' ')) return 0;
  std::tm x{};
  x.tm_year = r - 1900;
  x.tm_mon = mi - 1;
  x.tm_mday = d;
  x.tm_hour = h;
  x.tm_min = m;
  x.tm_sec = n == 7 ? sek : 0;
  x.tm_isdst = -1;
  time_t e = mktime(&x);
  return e <= 0 ? 0 : (uint32_t)e;
}
// Czas lokalny panelu według wzoru strftime (wzory tylko stałe z tego pliku).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
std::string format(uint32_t t, const char* f) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  char b[32];
  strftime(b, sizeof b, f, &l);
  return b;
}
#pragma GCC diagnostic pop

struct Nacisniecie {
  uint32_t t = 0;      // czas unix naciśnięcia
  uint8_t bit = 0;     // BIT_LEWY, BIT_SRODEK, BIT_PRAWY
  uint32_t ms = 0;     // jak długo trzymany
  std::string opis;    // „↻ środkowy”, „◀ lewy 2 s”
  // `do:{ekran}` (np. `prawy do:sprzet`): naciśnięcie powtarzane co 4 s, aż panel pokaże ten ekran (najwyżej
  // POWTORZEN_DO razy), bo liczba stron do celu zależy od danych (np. liczba kart „Otwartych spraw”)
  std::string cel;
  int zostalo = 0;
};
const int POWTORZEN_DO = 15;
const uint32_t ODSTEP_DO_S = 4;
const uint32_t PRZESUNIECIE_DO_S = 30 * 60;

std::vector<Nacisniecie> wczytajPrzyciski(const std::string& plik) {
  std::vector<Nacisniecie> v;
  std::ifstream f(plik);
  if (!f) blad("nie mogę otworzyć pliku przycisków " + plik);
  std::string l;
  int nr = 0;
  while (std::getline(f, l)) {
    nr++;
    size_t h = l.find('#');
    if (h != std::string::npos) l = l.substr(0, h);
    std::istringstream w(l);
    std::string kiedy, ktory, dlugo;
    if (!(w >> kiedy)) continue;
    w >> ktory >> dlugo;
    Nacisniecie n;
    n.t = czasZTekstu(kiedy);
    if (!n.t) blad("plik przycisków, wiersz " + std::to_string(nr) + ": zła data " + kiedy);
    if (ktory == "lewy") { n.bit = BIT_LEWY; n.opis = "◀ lewy"; }
    else if (ktory == "srodkowy") { n.bit = BIT_SRODEK; n.opis = "↻ środkowy"; }
    else if (ktory == "prawy") { n.bit = BIT_PRAWY; n.opis = "▶ prawy"; }
    else blad("plik przycisków, wiersz " + std::to_string(nr) + ": nieznany przycisk " + ktory);
    n.ms = dlugo == "2s" ? TRZYMANIE_DLUGIE_MS : TRZYMANIE_KROTKIE_MS;
    if (dlugo == "2s") n.opis += " 2 s";
    else if (dlugo.compare(0, 3, "do:") == 0 && dlugo.size() > 3) {
      n.cel = dlugo.substr(3);
      n.zostalo = POWTORZEN_DO;
      n.opis += " (do " + n.cel + ")";
    } else if (!dlugo.empty()) blad("plik przycisków, wiersz " + std::to_string(nr) + ": nieznane „" + dlugo + "”");
    v.push_back(n);
  }
  std::stable_sort(v.begin(), v.end(), [](const Nacisniecie& a, const Nacisniecie& b) { return a.t < b.t; });
  return v;
}

// Pliki zrzutu: ścieżka względna katalogu → ścieżka na panelu („noce.csv” → „/noce.csv”, „dane/noce/2026.csv” →
// „/dane/noce/2026.csv”). Pomijane: kopie `.poprzedni` z zrzut_plikow.py, pliki systemu macOS, pliki za duże.
std::map<std::string, std::string> wczytajZrzut(const std::string& katalog, std::vector<std::string>& uwagi) {
  std::map<std::string, std::string> w;
  std::error_code ec;
  if (!fs::is_directory(katalog, ec)) blad("katalog zrzutu nie istnieje: " + katalog);
  for (auto it = fs::recursive_directory_iterator(katalog, ec); !ec && it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    if (!it->is_regular_file()) continue;
    std::string nazwa = it->path().filename().string();
    std::string rel = fs::relative(it->path(), katalog).generic_string();
    if (nazwa == ".DS_Store" || nazwa.compare(0, 2, "._") == 0) continue;
    if (nazwa.size() > 10 && nazwa.compare(nazwa.size() - 10, 10, ".poprzedni") == 0) continue;
    if (it->file_size() > NAJWIEKSZY_PLIK) {
      uwagi.push_back("pominięty plik za duży jak na panel: " + rel);
      continue;
    }
    std::ifstream f(it->path(), std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    std::string t = s.str();
    if (t.compare(0, 3, "\xEF\xBB\xBF") == 0) {  // znacznik BOM z Numbers albo Excela: panel go nie zapisuje
      t.erase(0, 3);
      uwagi.push_back("usunięty znacznik BOM na początku pliku " + rel + " (zapis z Numbers albo Excela)");
    }
    w["/" + rel] = t;
  }
  if (ec) blad("nie mogę przeczytać katalogu zrzutu: " + ec.message());
  return w;
}

std::vector<std::string> wiersze(const std::string& t) {
  std::vector<std::string> v;
  std::istringstream s(t);
  std::string l;
  while (std::getline(s, l)) {
    while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
    if (!l.empty()) v.push_back(l);
  }
  return v;
}

// Doby AirBridge (numer dnia = data poranka − 1) nocy kompletnych z AirBridge w zrzucie (źródło A, nie „częściowa”):
// tylko te są na liście atrapy, więc firmware nie widzi żadnej nocy do pobrania (airbridge.cpp planPobrania()).
struct StatNocy {
  int nocy = 0;                        // różne daty nocy
  std::string ostatnia;                // najpóźniejsza data poranka (każde źródło)
  std::vector<std::string> pominiete;  // „noce.csv: wiersz 11” dla wierszy, których parser nocy nie czyta
};
std::set<long> dobyAirBridge(const std::map<std::string, std::string>& pliki, StatNocy& st) {
  std::set<long> d;
  std::set<std::string> daty;
  for (auto& kv : pliki) {
    bool v0 = kv.first == "/noce.csv";
    bool v1 = kv.first.compare(0, 11, "/dane/noce/") == 0 && kv.first.size() == 19 &&
              kv.first.compare(15, 4, ".csv") == 0;
    if (!v0 && !v1) continue;
    std::vector<std::string> w = wiersze(kv.second);
    for (size_t i = 0; i < w.size(); i++) {
      const std::string& l = w[i];
      if (l[0] == '#' || l.compare(0, 5, "data;") == 0) continue;
      Noc n;
      // wersja 1 i 2 jednym parserem (parsujNocV2() czyta też wiersz wersji 1: brakujące pola = brak danych)
      bool ok = v0 ? parsujNoc(l, 'A', n) : parsujNocV2(l, n);
      if (!ok || nrDnia(n.data) <= 0) {
        st.pominiete.push_back(kv.first.substr(1) + ": wiersz " + std::to_string(i + 1));
        continue;
      }
      daty.insert(n.data);
      if (n.data > st.ostatnia) st.ostatnia = n.data;
      if (n.zrodlo == 'A' && !n.czesciowa && nrDnia(n.data) > 0) d.insert(nrDnia(n.data) - 1);
    }
  }
  st.nocy = (int)daty.size();
  return d;
}

std::string lista(const std::set<long>& doby) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  bool pierwszy = true;
  for (long x : doby) {
    l += std::string(pierwszy ? "" : ",") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
    pierwszy = false;
  }
  return l + "]}";
}

// Ekran z zapisem każdej klatki: chwila, ekran z RTC i pozycja w logu portu (do nazwy ekranu).
struct Klatka {
  uint32_t t = 0;
  uint8_t ekran = 0;
  size_t logOd = 0, logDo = 0;
  std::vector<uint8_t> b;
};
class EkranSym : public AtrapaWyswietlacza {
 public:
  EkranSym(Dziennik& d, AtrapaZegara& z, AtrapaUruchomienia& u, AtrapaPortu& port)
      : AtrapaWyswietlacza(d), z_(z), u_(u), p_(port) {}
  std::vector<Klatka> nowe;
  size_t logOd = 0;
  uint32_t tWyb = 0;  // prawdziwa chwila początku wybudzenia (zegar panelu bywa nieznany, R13)
  void odswiez() override {
    AtrapaWyswietlacza::odswiez();
    Klatka k;
    k.t = tWyb + z_.msTeraz / 1000;
    k.ekran = u_.rtc().ekran;
    k.logOd = logOd;
    k.logDo = p_.log.size();
    k.b = p.b;
    logOd = k.logDo;
    nowe.push_back(k);
  }
 private:
  AtrapaZegara& z_;
  AtrapaUruchomienia& u_;
  AtrapaPortu& p_;
};
// Przyciski ze skryptem, z zapisem chwil odczytu (naciśnięcie bez odczytu w czasie trzymania = panel go nie widział).
class PrzyciskiSym : public AtrapaPrzyciskow {
 public:
  explicit PrzyciskiSym(AtrapaZegara& z) : AtrapaPrzyciskow(z), z_(z) {}
  std::vector<uint32_t> odczyty;
  uint8_t wcisniete() override {
    odczyty.push_back(z_.msTeraz);
    return AtrapaPrzyciskow::wcisniete();
  }
  // true, gdy panel odczytał przyciski w chwili [od, doMs)
  bool odczytane(uint32_t od, uint32_t doMs) const {
    for (uint32_t x : odczyty)
      if (x >= od && x < doMs) return true;
    return false;
  }
 private:
  AtrapaZegara& z_;
};
// Sieć z NTP podającym prawdziwą chwilę (początek wybudzenia + zegar procesora): bez udanego NTP zegar panelu po
// starcie zasilania jest nieznany, jak na panelu (src/adaptery_esp32.cpp ZegarEsp32::teraz() = time(), po włączeniu
// od zera; w głębokim śnie RTC chodzi dalej). R13.
class SiecSym : public AtrapaSieci {
 public:
  SiecSym(Dziennik& d, AtrapaZegara& z) : AtrapaSieci(d, z), z_(z) {}
  uint32_t tWyb = 0;
  bool czasZSieci(uint32_t limitMs) override {
    czasNtp = tWyb + z_.msTeraz / 1000;
    return AtrapaSieci::czasZSieci(limitMs);
  }
 private:
  AtrapaZegara& z_;
};
class SenSym : public AtrapaSnu {
 public:
  SenSym(Dziennik& d, AtrapaUruchomienia& u, AtrapaStraznika& s, AtrapaZegara& z) : AtrapaSnu(d, u, s), z_(z) {}
  uint32_t msPrzySnie = 0;
  void spij(const PlanSnu& p) override {
    msPrzySnie = z_.msTeraz;
    AtrapaSnu::spij(p);
  }
 private:
  AtrapaZegara& z_;
};

// gniazdo HTTP raportu dla lekarza: nasłuch „działa”, ale żaden telefon nie pyta, więc panel pokazuje
// ściągę z kodem QR i adresem, a raport PDF zostaje w pamięci panelu (symulator kopiuje go do katalogu wyniku).
class GniazdoSym : public serwer::GniazdoHttp {
 public:
  bool start(uint16_t) override { return true; }
  void stop() override {}
  void obsluz(const std::function<serwer::Odpowiedz(const std::string&)>&) override {}
};

bool zawiera(const std::string& s, const char* f) { return s.find(f) != std::string::npos; }

// Nazwa ekranu klatki: K1, K2, pytanie i test wprost z ekranu w RTC (StanRtc::ekran, ustawiany przez
// dyrygent przed rysowaniem). Ekran dnia: znacznik z logu dyrygenta wypisany tuż przed klatką (formaty
// ZNACZNIK_NOCNY, ZNACZNIK_DNIA, ZNACZNIK_S5 z src/dyrygent.cpp; tools/test_symulator.py pada, gdy dyrygent je
// zmieni), a bez znacznika (przerysowanie z wpisu dziennika) typ z ostatniego wpisu dziennika ekranów tego dnia
// (/dane/humor.csv, pole `typ`). Bez żadnego z nich `dzien-nieznany`, nigdy zgadywanie.
const char* const ZNACZNIK_NOCNY = "ekran nocny (";
const char* const ZNACZNIK_DNIA = "ekran dnia ";
const char* const ZNACZNIK_S5 = "S5: ";
const char* const ZNACZNIK_PYTANIA = "ekran dnia: pytanie ";
const char* const ZNACZNIK_WZORCA = "ekran dnia: wzorzec ";
const char* const ZNACZNIK_ZDARZENIA = "ekran dnia: zdarzenie ";  // S3 zdarzeń E
const char* const ZNACZNIK_SLOTU = "slot dzienny: S-K-";
const char* const ZNACZNIK_WIECZORU = "ekran wieczorny ";
const char* const ZNACZNIK_ODKRYCIA = "ekran dnia: odkrycie ";  // S1 z odkryciem O
const char* const ZNACZNIK_PODSUMOWANIA = "ekran dnia: S";      // S7, S8
const char* const ZNACZNIK_SK = "ekran S-K-";                   // ekrany S-K (src/dyrygent.cpp pokazB16)
std::string typZDziennika(Pliki& pl, const std::string& dzien) {
  std::string t, typ;
  if (!pl.czytaj("/dane/humor.csv", t)) return "";
  for (auto& l : wiersze(t))
    if (l.compare(0, dzien.size() + 1, dzien + ";") == 0) {
      size_t b = l.find(';', dzien.size() + 1);
      typ = l.substr(dzien.size() + 1, b == std::string::npos ? std::string::npos : b - dzien.size() - 1);
      // przerywnik z wpisu `S2D` ze sceną (pole 5: `data;typ;klucz;tryb;scena;zdanie`)
      if (typ == "S2D") {
        size_t z = 0;
        for (int i = 0; i < 4 && z != std::string::npos; i++) z = l.find(';', z == 0 && i == 0 ? 0 : z + 1);
        if (z != std::string::npos) {
          std::string sc = l.substr(z + 1, l.find(';', z + 1) == std::string::npos ? std::string::npos : l.find(';', z + 1) - z - 1);
          if (!sc.empty()) typ += "-" + sc;
        }
      }
    }
  return typ;
}
// nazwa ekranu dla `do:{ekran}` w pliku przycisków (jak przedrostki nazw obrazków)
std::string nazwaCelu(Ekran e) {
  switch (e) {
    case Ekran::DZIEN: return "dzien";
    case Ekran::K1: return "K1";
    case Ekran::K2: return "K2";
    case Ekran::SPRAWY: return "sprawy";
    case Ekran::NORMA: return "norma";
    case Ekran::LICZBY: return "liczby";
    case Ekran::SPRZET: return "sprzet";
    case Ekran::USTAWIENIA: return "ustawienia";
    case Ekran::FORMULARZ: return "formularz";
    case Ekran::TEST: return "test";
    case Ekran::RAPORT: return "raport";
    default: return "inny";
  }
}
std::string nazwaEkranu(const Klatka& k, const std::vector<std::string>& log, Pliki& pl) {
  switch ((Ekran)k.ekran) {
    case Ekran::K1: return "K1";
    case Ekran::K2: return "K2";
    case Ekran::PYTANIE: return "pytanie";
    case Ekran::TEST: return "test";
    case Ekran::SPRAWY: return "sprawy";
    case Ekran::OMOWIONE: return "omowione";
    case Ekran::PRZENIESIONE: return "przeniesione";
    case Ekran::ZAPISANO: return "zapisano";
    case Ekran::RAPORT: return "raport";
    case Ekran::LICZBY: return "liczby";           // „Co znaczą te liczby”, bez numeru (R-KOK-22)
    case Ekran::SPRZET_ZAPISANO: return "sprzet-zapisano";  // `Wymiana zapisana.` (R-MAS-01, R-MAS-18)
    case Ekran::USTAWIENIA: return "ustawienia";   // strona 3 / 3
    case Ekran::FORMULARZ: return "formularz";     // U-03
    case Ekran::SIEC_PANELU: return "siec-panelu"; // U-02
    default: break;
  }
  for (size_t i = k.logDo; i > k.logOd; i--) {
    const std::string& l = log[i - 1];
    if (l.compare(0, strlen(ZNACZNIK_NOCNY), ZNACZNIK_NOCNY) == 0) return "Dobranoc";
    // ekrany z wierszy dyrygenta (src/dyrygent.cpp: rysujDzienB5, slot dzienny, ekran wieczorny)
    for (const char* z : {ZNACZNIK_PYTANIA, ZNACZNIK_WZORCA, ZNACZNIK_ZDARZENIA, ZNACZNIK_SLOTU, ZNACZNIK_WIECZORU,
                          ZNACZNIK_ODKRYCIA, ZNACZNIK_PODSUMOWANIA, ZNACZNIK_SK}) {
      if (l.compare(0, strlen(z), z) != 0) continue;
      size_t a = strlen(z), b = l.find_first_of(" (:,", a);
      std::string id = l.substr(a, b == std::string::npos ? std::string::npos : b - a);
      if (id.empty()) continue;
      const char* pref = z == ZNACZNIK_PYTANIA ? "dzien-pytanie-" : z == ZNACZNIK_WZORCA ? "dzien-wzorzec-"
                         : z == ZNACZNIK_ZDARZENIA ? "dzien-zdarzenie-" : z == ZNACZNIK_SLOTU || z == ZNACZNIK_SK ? "dzien-SK"
                         : z == ZNACZNIK_ODKRYCIA ? "dzien-odkrycie-" : z == ZNACZNIK_PODSUMOWANIA ? "dzien-S" : "dzien-wieczor-";
      return pref + id;
    }
    if (l.compare(0, strlen(ZNACZNIK_S5), ZNACZNIK_S5) == 0) return "dzien-S5";
    if (l.compare(0, strlen(ZNACZNIK_DNIA), ZNACZNIK_DNIA) == 0) {
      size_t a = strlen(ZNACZNIK_DNIA), b = l.find_first_of(" (:", a);
      std::string typ = l.substr(a, b == std::string::npos ? std::string::npos : b - a);
      if (!typ.empty()) return "dzien-" + typ;
    }
  }
  std::string typ = typZDziennika(pl, format(k.t, "%Y-%m-%d"));
  return typ.empty() ? "dzien-nieznany" : "dzien-" + typ;
}

std::string bezPolskich(const std::string& s) {
  std::string w;
  for (char c : s) w += (std::isalnum((unsigned char)c) || c == '-') ? c : '_';
  return w;
}

void zapiszPbm(const std::string& p, const std::vector<uint8_t>& b) {
  std::ofstream f(p, std::ios::binary);
  f << "P4\n800 480\n";
  f.write((const char*)b.data(), (std::streamsize)b.size());
  if (!f) blad("nie mogę zapisać " + p);
}

// ---------- atrapy prognozy i otoczenia, obrazki, noce scenariusza ----------
// Prognoza z pliku symuluj.py: 48 wierszy po 10 pól (g;temp;opad;chmury;porywy;pm25;brzoza;trawy;olcha;bylica).
std::vector<std::vector<std::string>> wczytajPrognoze(const std::string& plik) {
  std::ifstream f(plik);
  if (!f) blad("nie mogę otworzyć pliku prognozy " + plik);
  std::vector<std::vector<std::string>> w(48, std::vector<std::string>(9));
  std::string l;
  int n = 0;
  while (std::getline(f, l)) {
    if (l.empty() || l[0] == '#') continue;
    std::vector<std::string> p;
    size_t a = 0;
    for (;;) {
      size_t b = l.find(';', a);
      p.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a));
      if (b == std::string::npos) break;
      a = b + 1;
    }
    int g = atoi(p[0].c_str());
    if (p.size() != 10 || g < 0 || g > 47) blad("plik prognozy: zły wiersz „" + l + "”");
    for (int i = 0; i < 9; i++) w[(size_t)g][(size_t)i] = p[(size_t)i + 1];
    n++;
  }
  if (n != 48) blad("plik prognozy: oczekuję 48 godzin, jest " + std::to_string(n));
  return w;
}
// Odpowiedź w kształcie Open-Meteo (`hourly` z tablicą `time` i tablicami pól) dla dnia `dzien` i następnego.
std::string jsonOpenMeteo(const std::vector<std::vector<std::string>>& w, const std::string& dzien,
                          const std::vector<std::pair<const char*, int>>& pola) {
  std::string jutro = dodajDni(dzien, 1);
  std::string j = "{\"latitude\":0,\"longitude\":0,\"timezone\":\"Europe/Warsaw\",\"hourly_units\":{},\"hourly\":{\"time\":[";
  char b[32];
  for (int g = 0; g < 48; g++) {
    snprintf(b, sizeof b, "%s\"%sT%02d:00\"", g ? "," : "", (g < 24 ? dzien : jutro).c_str(), g % 24);
    j += b;
  }
  j += "]";
  for (auto& pl : pola) {
    j += std::string(",\"") + pl.first + "\":[";
    for (int g = 0; g < 48; g++) {
      const std::string& v = w[(size_t)g][(size_t)pl.second];
      j += std::string(g ? "," : "") + (v.empty() ? "null" : v);
    }
    j += "]";
  }
  return j + "}}";
}
// Profil CO2 sypialni: (minuta doby, ppm) rosnąco po minucie; wartość obowiązuje do następnego wiersza (z zawinięciem).
std::vector<std::pair<int, int>> wczytajOtoczenie(const std::string& plik) {
  std::ifstream f(plik);
  if (!f) blad("nie mogę otworzyć pliku otoczenia " + plik);
  std::vector<std::pair<int, int>> v;
  std::string l;
  while (std::getline(f, l)) {
    if (l.empty() || l[0] == '#') continue;
    int h = 0, m = 0, c = 0;
    if (sscanf(l.c_str(), "%2d:%2d;%d", &h, &m, &c) != 3 || h > 23 || m > 59) blad("plik otoczenia: zły wiersz „" + l + "”");
    v.push_back({h * 60 + m, c});
  }
  if (v.empty()) blad("plik otoczenia bez wierszy");
  std::sort(v.begin(), v.end());
  return v;
}
int co2O(const std::vector<std::pair<int, int>>& v, int minuta) {
  int c = v.back().second;
  for (auto& x : v)
    if (x.first <= minuta) c = x.second;
  return c;
}
// Pliki .pbm i obrazki.json z katalogu (bez podkatalogów) → `/obrazki/{nazwa}`.
int wczytajObrazki(const std::string& katalog, std::map<std::string, std::string>& pliki) {
  std::error_code ec;
  if (!fs::is_directory(katalog, ec)) blad("katalog obrazków nie istnieje: " + katalog);
  int n = 0;
  for (auto& e : fs::directory_iterator(katalog, ec)) {
    std::string nazwa = e.path().filename().string();
    bool pbm = nazwa.size() > 4 && nazwa.compare(nazwa.size() - 4, 4, ".pbm") == 0;
    if (!e.is_regular_file() || (!pbm && nazwa != "obrazki.json") || nazwa.compare(0, 2, "._") == 0) continue;
    std::ifstream f(e.path(), std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    pliki["/obrazki/" + nazwa] = s.str();
    n += pbm;
  }
  return n;
}
// Noce scenariusza: wiersze `noce` wersji 2 (sprawdzone parserem firmware), rosnąco po dacie poranka.
std::vector<std::string> wczytajNoceScenariusza(const std::string& plik) {
  std::ifstream f(plik);
  if (!f) blad("nie mogę otworzyć pliku nocy scenariusza " + plik);
  std::vector<std::string> v;
  std::string l;
  while (std::getline(f, l)) {
    if (l.empty() || l[0] == '#') continue;
    Noc n;
    if (!parsujNocV2(l, n) || nrDnia(n.data) <= 0) blad("noce scenariusza: wiersz nieczytelny dla firmware „" + l + "”");
    v.push_back(l);
  }
  std::stable_sort(v.begin(), v.end(), [](const std::string& a, const std::string& b) { return a.substr(0, 10) < b.substr(0, 10); });
  return v;
}
// Wstawia wiersz formatu `nazwa` (pierwsze pole = data, klucz wiersza) do pliku nowej ścieżki tego formatu w wersji
// tej wersji firmware: noce do `/dane/noce/RRRR.csv`, czynniki do `/dane/czynniki.csv`. Plik starszej wersji
// w kopii zrzutu dostaje puste pola nowej wersji, jak migracja w miejscu; wiersz z tą samą datą zastąpiony. Plik
// prototypu (`/noce.csv`, `/czynniki.csv`) zostaje: firmware scala go z plikiem nowej ścieżki przy starcie
// (src/magazyn.cpp scalZrodlo()).
void wstawWiersz(std::map<std::string, std::string>& pliki, const char* nazwa, const std::string& wiersz) {
  const FormatPliku* f = formatPliku(rejestrFormatow(), nazwa);
  const std::vector<std::string>& pola = f->pola[(size_t)f->wersja() - 1];
  std::string nazwy;
  for (size_t i = 0; i < pola.size(); i++) nazwy += (i ? ";" : "") + pola[i];
  std::string sc = f->sciezka;
  size_t r = sc.find("{RRRR}");
  if (r != std::string::npos) sc.replace(r, 6, wiersz.substr(0, 4));
  auto uzupelnij = [&](std::string w) {
    size_t n = (size_t)std::count(w.begin(), w.end(), ';') + 1;
    for (; n < pola.size(); n++) w += ";";
    return w;
  };
  std::vector<std::string> w;
  auto it = pliki.find(sc);
  if (it != pliki.end()) {
    std::vector<std::string> l = wiersze(it->second);
    for (size_t i = 2; i < l.size(); i++)
      if (l[i][0] != '#' && l[i].compare(0, 10, wiersz.substr(0, 10)) != 0) w.push_back(uzupelnij(l[i]));
  }
  w.push_back(uzupelnij(wiersz));
  std::stable_sort(w.begin(), w.end(), [](const std::string& a, const std::string& b) { return a.substr(0, 10) < b.substr(0, 10); });
  std::string t = "#cpap-eink " + std::string(nazwa) + " " + std::to_string(f->wersja()) + "\n" + nazwy + "\n";
  for (auto& x : w) t += x + "\n";
  pliki[sc] = t + "#koniec " + std::to_string(w.size()) + "\n";
}
void wstawNoc(std::map<std::string, std::string>& pliki, const std::string& wiersz) { wstawWiersz(pliki, "noce", wiersz); }
// wiersze `klucz=wartość` scenariusza w `/konfiguracja.txt` (daty wymiany sprzętu 5.9.2): klucz z pliku zrzutu
// zastąpiony, nowy dopisany, reszta pliku bez zmian (jak sprzet::zapiszWymiane()).
void scalKonfiguracje(std::map<std::string, std::string>& pliki, const std::vector<std::string>& nowe) {
  std::vector<std::string> l = wiersze(pliki.count("/konfiguracja.txt") ? pliki["/konfiguracja.txt"] : "");
  for (auto& n : nowe) {
    std::string k = n.substr(0, n.find('='));
    bool jest = false;
    for (auto& x : l)
      if (x.compare(0, k.size() + 1, k + "=") == 0) { x = n; jest = true; }
    if (!jest) l.push_back(n);
  }
  std::string t;
  for (auto& x : l) t += x + "\n";
  pliki["/konfiguracja.txt"] = t;
}
// Płótno na kopii bufora klatki (bit 1 = czarny), do znaku wodnego.
struct PlotnoKopii : Plotno {
  std::vector<uint8_t>& b;
  explicit PlotnoKopii(std::vector<uint8_t>& x) : b(x) {}
  void piksel(int x, int y, bool c) override {
    if (x < 0 || y < 0 || x >= 800 || y >= 480) return;
    uint8_t m = (uint8_t)(0x80 >> (x & 7));
    size_t i = (size_t)y * 100 + (size_t)(x >> 3);
    if (c) b[i] |= m; else b[i] &= (uint8_t)~m;
  }
};
// `DANE SYNTETYCZNE` białym tekstem na czarnym polu w prawym górnym rogu klatki.
std::vector<uint8_t> zeZnakiemWodnym(std::vector<uint8_t> b) {
  PlotnoKopii p(b);
  const std::string napis = "DANE SYNTETYCZNE";
  int w = szer(F_P16, napis) + 16, h = 24;
  p.prostokat(800 - w, 0, w, h, true);
  tekst(p, F_P16, 800 - w + 8, 17, napis, false);
  return b;
}
// Ostatnia noc w pamięci panelu (/dane/noce/RRRR.csv) z porankiem nie później niż `dzis`: opis do log-panelu.txt.
std::string ostatniaNocPanelu(const std::map<std::string, std::string>& pliki, const std::string& dzis,
                              const std::set<std::string>& syntetyczne) {
  Noc naj;
  bool jest = false;
  for (auto& kv : pliki) {
    if (kv.first.compare(0, 11, "/dane/noce/") != 0 || kv.first.size() != 19 || kv.first.compare(15, 4, ".csv") != 0)
      continue;
    for (auto& l : wiersze(kv.second)) {
      Noc n;
      if (l[0] == '#' || l.compare(0, 5, "data;") == 0 || !parsujNocV2(l, n) || n.data > dzis) continue;
      if (!jest || n.data > naj.data) { naj = n; jest = true; }
    }
  }
  if (!jest) return "";
  return "rano " + naj.data + ", tryb " + (trybNocy(naj).empty() ? "nieznany" : trybNocy(naj)) + ", M-01 " +
         (naj.m01 < 0 ? "brak" : std::to_string(naj.m01) + " min") + ", syntetyczna " +
         (syntetyczne.count(naj.data) ? "tak" : "nie");
}
// wiersz pamięci reguł do listy „spełnione”: tylko reguły z poziomem w katalogu (nie wiersze pomocnicze),
// hipoteza H dopiero przy spełnionym swoim wzorcu L (sprawdza wołający).
bool zPoziomem(const std::string& id, const std::string& wariant) {
  for (auto& w : reguly::zestawWbudowany().katalog)
    if (w.id == id && w.wariant == wariant) return !w.poziom.empty();
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  setenv("TZ", STREFA, 1);
  tzset();
  std::string dane, od, doT, wyjscie, przyciski;
  bool bezSieci = false, zegarOdStartu = false, bezNtp = false;
  std::string plikPrognozy, plikOtoczenia, katalogObrazkow, plikNocy, milczyOd, otoOd, otoDo;
  std::string plikKonfiguracji, plikCzynnikow;
  std::vector<std::pair<uint32_t, uint32_t>> przerwyAB;  // AirBridge milczy w [od, do) (wyjazd)
  std::map<std::string, std::string> opoznione;
  std::string aparatPna = "AirCurve_10_CS_PaceWave", aparatTryb = "ASVAuto";  // jak STATUS_SPOCZYNEK // data poranka → HH:MM przyjścia nocy zamiast 04:30
  std::vector<std::string> opisy;
  double lat = NAN, lon = NAN;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto nast = [&]() -> std::string {
      if (i + 1 >= argc) blad("brak wartości po " + a);
      return argv[++i];
    };
    if (a == "--dane") dane = nast();
    else if (a == "--od") od = nast();
    else if (a == "--do") doT = nast();
    else if (a == "--wyjscie") wyjscie = nast();
    else if (a == "--przyciski") przyciski = nast();
    else if (a == "--bez-sieci") bezSieci = true;
    else if (a == "--zegar-znany") zegarOdStartu = true;
    else if (a == "--bez-ntp") bezNtp = true;
    else if (a == "--lat") lat = atof(nast().c_str());
    else if (a == "--lon") lon = atof(nast().c_str());
    else if (a == "--prognoza") plikPrognozy = nast();
    else if (a == "--otoczenie") plikOtoczenia = nast();
    else if (a == "--obrazki") katalogObrazkow = nast();
    else if (a == "--noce-scenariusza") plikNocy = nast();
    else if (a == "--airbridge-milczy-od") milczyOd = nast();
    else if (a == "--opis") opisy.push_back(nast());
    else if (a == "--otoczenie-od") otoOd = nast();
    else if (a == "--otoczenie-do") otoDo = nast();
    else if (a == "--konfiguracja") plikKonfiguracji = nast();
    else if (a == "--czynniki-scenariusza") plikCzynnikow = nast();
    else if (a == "--aparat") {  // `PNA,TRYB` aparatu tylko w atrapie `/api/status` (nie w konfiguracji panelu)
      std::string x = nast();
      size_t c = x.find(',');
      if (c == std::string::npos || !c || c + 1 >= x.size()) blad("zły aparat w --aparat (wzór AirSense_10_AutoSet,AutoSet)");
      aparatPna = x.substr(0, c);
      aparatTryb = x.substr(c + 1);
    }
    else if (a == "--noc-opozniona") {
      std::string x = nast();
      if (x.size() != 16 || x[10] != ',' || !czasZTekstu(x.substr(0, 10) + "T" + x.substr(11)))
        blad("zła noc w --noc-opozniona (wzór 2026-11-05,05:20)");
      opoznione[x.substr(0, 10)] = x.substr(11);
    }
    else if (a == "--airbridge-przerwa") {
      std::string x = nast();
      size_t c = x.find(',');
      uint32_t o = c == std::string::npos ? 0 : czasZTekstu(x.substr(0, c)), d = o ? czasZTekstu(x.substr(c + 1)) : 0;
      if (!o || !d || d <= o) blad("zła przerwa w --airbridge-przerwa (wzór 2026-11-05T06:00,2026-11-08T16:00)");
      przerwyAB.push_back({o, d});
    }
    else blad("nieznany argument " + a);
  }
  if (dane.empty() || od.empty() || doT.empty() || wyjscie.empty())
    blad("użycie: symulator --dane KATALOG --od CZAS --do CZAS --wyjscie KATALOG [--przyciski PLIK] [--bez-sieci] "
         "[--zegar-znany]");
  uint32_t tOd = czasZTekstu(od), tDo = czasZTekstu(doT);
  if (!tOd || !tDo) blad("zła data w --od albo --do (wzór 2026-11-05T05:00)");
  if (tDo <= tOd) blad("--do musi być później niż --od");
  std::vector<Nacisniecie> kolejka = przyciski.empty() ? std::vector<Nacisniecie>() : wczytajPrzyciski(przyciski);
  uint32_t tMilczy = milczyOd.empty() ? 0 : czasZTekstu(milczyOd);
  if (!milczyOd.empty() && !tMilczy) blad("zła data w --airbridge-milczy-od");
  std::vector<std::vector<std::string>> prognoza;
  if (!plikPrognozy.empty()) prognoza = wczytajPrognoze(plikPrognozy);
  std::vector<std::pair<int, int>> otoczenie;
  if (!plikOtoczenia.empty()) otoczenie = wczytajOtoczenie(plikOtoczenia);
  std::vector<std::string> noceScen = plikNocy.empty() ? std::vector<std::string>() : wczytajNoceScenariusza(plikNocy);
  std::set<std::string> datySyntetyczne;
  for (auto& w : noceScen) datySyntetyczne.insert(w.substr(0, 10));
  const bool syn = !noceScen.empty();
  uint32_t tOtoOd = otoOd.empty() ? 0 : czasZTekstu(otoOd), tOtoDo = otoDo.empty() ? 0xFFFFFFFFu : czasZTekstu(otoDo);
  if ((!otoOd.empty() && !tOtoOd) || (!otoDo.empty() && !tOtoDo)) blad("zła data w --otoczenie-od albo --otoczenie-do");

  std::vector<std::string> uwagi;
  std::map<std::string, std::string> zrzut = wczytajZrzut(dane, uwagi);
  bool v1 = false, v0 = false;
  for (auto& kv : zrzut) {
    v1 = v1 || kv.first.compare(0, 6, "/dane/") == 0;
    v0 = v0 || kv.first == "/noce.csv";
  }
  StatNocy st;
  std::set<long> doby = dobyAirBridge(zrzut, st);
  int nocy = st.nocy;
  if (!nocy) blad("w zrzucie nie ma żadnej nocy (brak noce.csv albo dane/noce/RRRR.csv z rekordami)");
  const std::set<long> dobyZrzutu = doby;  // nagłówek mówi o zrzucie, nie o nocach scenariusza
  // noce scenariusza z porankiem do --od od razu w kopii zrzutu, późniejsze o 04:30 swojego poranka
  size_t nocScenWstawionych = 0;
  auto chwilaNocy = [&opoznione](const std::string& w) {
    auto o = opoznione.find(w.substr(0, 10));
    return czasZTekstu(w.substr(0, 10) + "T" + (o == opoznione.end() ? std::string("04:30") : o->second));
  };
  while (nocScenWstawionych < noceScen.size() && chwilaNocy(noceScen[nocScenWstawionych]) <= tOd) {
    wstawNoc(zrzut, noceScen[nocScenWstawionych]);
    doby.insert(nrDnia(noceScen[nocScenWstawionych].substr(0, 10)) - 1);
    nocScenWstawionych++;
  }
  int obrazkow = katalogObrazkow.empty() ? -1 : wczytajObrazki(katalogObrazkow, zrzut);
  // ustawienia sprzętu scenariusza w /konfiguracja.txt i odpowiedzi „Co było inaczej?” na noce scenariusza
  std::vector<std::string> konfScen, czynScen;
  for (auto* x : {&plikKonfiguracji, &plikCzynnikow}) {
    if (x->empty()) continue;
    std::ifstream f(*x);
    if (!f) blad("nie mogę otworzyć pliku " + *x);
    std::string l;
    while (std::getline(f, l)) {
      if (l.empty() || l[0] == '#') continue;
      if (x == &plikKonfiguracji) {
        if (l.find('=') == std::string::npos) blad("konfiguracja scenariusza: zły wiersz „" + l + "”");
        konfScen.push_back(l);
      } else {
        WpisCzynnikow w;
        if (!parsujWpis(l, w)) blad("czynniki scenariusza: wiersz nieczytelny dla firmware „" + l + "”");
        czynScen.push_back(l);
      }
    }
  }
  if (!konfScen.empty()) scalKonfiguracje(zrzut, konfScen);
  for (auto& c : czynScen) wstawWiersz(zrzut, "czynniki", c);

  GniazdoSym gniazdoRaportu;
  serwer::ustawGniazdo(&gniazdoRaportu);
  Panel p;
  EkranSym ekran{p.d, p.zegar, p.start, p.port};
  SenSym sen{p.d, p.start, p.straznik, p.zegar};
  PrzyciskiSym przyc{p.zegar};
  p.pliki.pliki = zrzut;
  SiecSym siec{p.d, p.zegar};
  siec.siecJest = !bezSieci;
  siec.ntpJest = !bezNtp;
  Adaptery ad{p.zegar, p.nvs, p.pliki, siec, ekran, przyc, p.czujniki, p.dzwiek, p.start, p.straznik, sen,
              p.port, p.diag};
  KonfiguracjaPanelu k = {"192.168.1.20", "cpap", "haslo", "", "", false};
  std::string status = STATUS_SPOCZYNEK;
  for (auto zm : {std::make_pair(std::string("AirCurve_10_CS_PaceWave"), aparatPna),
                  std::make_pair(std::string("\"mode\":\"ASVAuto\""), "\"mode\":\"" + aparatTryb + "\"")})
    status.replace(status.find(zm.first), zm.first.size(), zm.second);
  k.pogodaLat = lat;
  k.pogodaLon = lon;
  if (!otoczenie.empty()) k.otoUrl = "http://192.168.1.60:8080/otoczenie";  // adres w sieci lokalnej (R-NF-27)

  fs::create_directories(wyjscie);
  std::ofstream log(wyjscie + "/log.txt"), logP(wyjscie + "/log-panelu.txt");
  if (!log || !logP) blad("nie mogę pisać do katalogu wyjścia " + wyjscie);
  log << "Symulator doby cpap-eink (wersja beta). Okres " << format(tOd, "%Y-%m-%d %H:%M") << " – "
      << format(tDo, "%Y-%m-%d %H:%M") << (bezSieci ? ", bez Wi-Fi" : ", Wi-Fi jest")
      << (zegarOdStartu ? ", zegar panelu znany od startu (--zegar-znany)"
                        : ", zegar panelu nieznany po starcie zasilania do pierwszego czasu z sieci (NTP)")
      << ".\n";
  log << "Zrzut: " << ile(zrzut.size(), "plik", "pliki", "plików") << ", " << ile((size_t)nocy, "noc", "noce", "nocy") << " ("<< (v1 ? "wersja 1 lub nowsza, katalog dane/" : "")
      << (v1 && v0 ? " i " : "") << (v0 ? "wersja 0, płaskie pliki: migracja do wersji 1 przy starcie" : "")
      << "). AirBridge: terapia nie trwa, "
      << (syn ? "nowe noce tylko syntetyczne ze scenariusza" : "brak nowych nocy") << " (lista ze zrzutu: "
      << ile(dobyZrzutu.size(), "noc", "noce", "nocy") << " z AirBridge).\n";
  if (!dobyZrzutu.empty() && nrDnia(dataPoranka(*dobyZrzutu.rbegin())) > 0)
    log << "Ostatnia noc z AirBridge w zrzucie: rano " << dataPoranka(*dobyZrzutu.rbegin()) << " (prawdziwa, ze zrzutu).\n";
  for (auto& u : uwagi) log << "Uwaga: " << u << "\n";
  if (!st.pominiete.empty()) {
    log << "Uwaga: pominięte nieczytelne wiersze pliku nocy: " << st.pominiete.size() << " (";
    for (size_t i = 0; i < st.pominiete.size() && i < 5; i++) log << (i ? ", " : "") << st.pominiete[i];
    log << (st.pominiete.size() > 5 ? ", …" : "") << "); panel też ich nie czyta, szczegóły w log-panelu.txt.\n";
  }
  // noce późniejsze niż poranek --od to dla panelu noce z przyszłości
  if (st.ostatnia > format(tOd, "%Y-%m-%d"))
    log << "UWAGA: zrzut ma noce późniejsze niż --od (ostatnia noc: rano " << st.ostatnia
        << "). Panel uzna je za noce z przyszłości, więc wynik nie odpowiada żadnej prawdziwej dobie. "
        << "Poranek z ostatnią nocą: --od " << st.ostatnia << "T05:00.\n";
  // opis scenariusza i atrap
  for (auto& o : opisy) log << o << "\n";
  if (!noceScen.empty())
    log << "Noce syntetyczne (wymyślone przez scenariusz, nie z aparatu): " << ile(noceScen.size(), "noc", "noce", "nocy")
        << ", poranki " << noceScen.front().substr(0, 10) << " – " << noceScen.back().substr(0, 10) << "; "
        << nocScenWstawionych << " w historii od startu, reszta dochodzi o 04:30 swojego poranka.\n";
  if (!std::isnan(lat)) {
    char b[96];
    snprintf(b, sizeof b, "Współrzędne miasta: %.4f, %.4f. ", lat, lon);
    log << b << (prognoza.empty() ? "Bez atrapy prognozy: zapytania do serwisu pogody bez odpowiedzi.\n"
                                    : "Prognoza godzinowa z atrapy serwisu pogody (plik, daty dnia wybudzenia).\n");
  }
  if (!otoczenie.empty())
    log << "Lokalne źródło otoczenia (atrapa): CO2 sypialni według profilu doby z pliku"
        << (otoOd.empty() && otoDo.empty() ? "" : " od " + (otoOd.empty() ? std::string("początku") : otoOd) + " do " +
                                                   (otoDo.empty() ? std::string("końca") : otoDo) + ", poza tym 650 ppm")
        << ".\n";
  if (obrazkow >= 0) log << "Obrazki dnia w pamięci panelu (/obrazki/): " << ile((size_t)obrazkow, "obrazek", "obrazki", "obrazków") << ".\n";
  if (!konfScen.empty()) {
    log << "Konfiguracja panelu ze scenariusza (/konfiguracja.txt):";
    for (auto& x : konfScen) log << " " << x;
    log << ".\n";
  }
  if (!czynScen.empty())
    log << "Odpowiedzi „Co było inaczej?” ze scenariusza (syntetyczne): " << ile(czynScen.size(), "noc", "noce", "nocy") << ".\n";
  for (auto& x : przerwyAB)
    log << "AirBridge milczy od " << format(x.first, "%Y-%m-%d %H:%M") << " do " << format(x.second, "%Y-%m-%d %H:%M")
        << " (wyjazd: Wi-Fi jest, aparat poza domem; noce z tego czasu dochodzą po powrocie).\n";
  if (tMilczy) log << "AirBridge milczy od " << format(tMilczy, "%Y-%m-%d %H:%M") << " (Wi-Fi jest, aparat nie odpowiada).\n";
  log << "\n";

  // naciśnięcia sprzed początku okresu nie mają sensu
  while (!kolejka.empty() && kolejka.front().t < tOd) {
    log << "Przycisk " << kolejka.front().opis << " o " << format(kolejka.front().t, "%Y-%m-%d %H:%M")
        << " jest przed początkiem okresu: pominięty\n";
    kolejka.erase(kolejka.begin());
  }
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  uint32_t t = tOd;
  bool zegarZnany = zegarOdStartu;
  PowodWybudzenia pw = PowodWybudzenia::START;
  size_t nrKlatki = 0;
  int wybudzen = 0;
  std::string ostatnieReguly, ostatniaNoc;
  const int najwiecej = (int)((tDo - tOd) / 60 + ZAPAS_WYBUDZEN);
  for (; wybudzen < najwiecej && t < tDo; wybudzen++) {
    // naciśnięcia w tym wybudzeniu: pierwsze budzi (offset 0), kolejne w oknie czuwania (offset w ms)
    std::vector<Nacisniecie> wTym;
    przyc.skrypt.clear();
    p.start.budzace = 0;
    for (auto& n : kolejka) {
      if (n.t >= t + OKNO_SKRYPTU_S) break;
      wTym.push_back(n);
      if (!n.cel.empty()) break;  // po naciśnięciu `do:{ekran}` dalsze kroki czekają, aż panel dojdzie do celu
    }
    kolejka.erase(kolejka.begin(), kolejka.begin() + (long)wTym.size());
    if (pw == PowodWybudzenia::PRZYCISK && !wTym.empty()) p.start.budzace = wTym[0].bit;
    for (auto& n : wTym) {
      uint32_t off = (n.t > t ? n.t - t : 0) * 1000;
      if (!przyc.skrypt.empty() && off < przyc.skrypt.back().first) off = przyc.skrypt.back().first + 200;
      przyc.skrypt.push_back({off, n.bit});
      przyc.skrypt.push_back({off + n.ms, 0});
    }
    bool abMilczy = tMilczy && t >= tMilczy;
    for (auto& x : przerwyAB) abMilczy = abMilczy || (t >= x.first && t < x.second);
    // noce scenariusza, których poranek właśnie nadszedł (o 04:30), w pamięci panelu i na liście AirBridge;
    // w przerwie AirBridge (wyjazd) noc czeka, aż AirBridge znowu odpowie (aparat zapisał ją poza domem)
    std::vector<std::string> doszly;
    while (!(abMilczy && !przerwyAB.empty()) && nocScenWstawionych < noceScen.size() &&
           chwilaNocy(noceScen[nocScenWstawionych]) <= t) {
      wstawNoc(p.pliki.pliki, noceScen[nocScenWstawionych]);
      doby.insert(nrDnia(noceScen[nocScenWstawionych].substr(0, 10)) - 1);
      doszly.push_back(noceScen[nocScenWstawionych].substr(0, 10));
      nocScenWstawionych++;
    }
    // AirBridge atrapa: odpowiedzi od nowa w każdym wybudzeniu
    przyc.odczyty.clear();
    siec.odpowiedzi.clear();
    if (!abMilczy) {
      std::string stAb = status;
      if (bezNtp) stAb.insert(stAb.size() - 1, ",\"esp_time\":\"" + format(t, "%Y-%m-%d %H:%M") + "\"");
      siec.dodaj("/api/status", 200, stAb);
      siec.dodaj(sciezkaListy(), 200, lista(doby));
      siec.dodaj("/api/report?source=edf&day=", 404, "");
    }
    // atrapa serwisu pogody (dzień wybudzenia według prawdziwego zegara symulacji) i źródła otoczenia
    if (!prognoza.empty()) {
      std::string dzien = format(t, "%Y-%m-%d");
      siec.dodaj("/v1/forecast", 200, jsonOpenMeteo(prognoza, dzien, {{"temperature_2m", 0}, {"precipitation_probability", 1},
                                                                       {"cloud_cover", 2}, {"wind_gusts_10m", 3}}));
      siec.dodaj("/v1/air-quality", 200, jsonOpenMeteo(prognoza, dzien, {{"pm2_5", 4}, {"birch_pollen", 5}, {"grass_pollen", 6},
                                                                          {"alder_pollen", 7}, {"mugwort_pollen", 8}}));
    }
    if (!otoczenie.empty()) {
      int c = t >= tOtoOd && t <= tOtoDo ? co2O(otoczenie, atoi(format(t, "%H").c_str()) * 60 + atoi(format(t, "%M").c_str()))
                                         : 650;
      siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":" + std::to_string(c) +
                                     ",\"temp\":21.5,\"wilg\":45},\"ostrzezenia\":[]}");
    }
    // zegar panelu: prawdziwy, gdy znany (NTP w którymś wybudzeniu albo --zegar-znany), inaczej czas od startu
    // zasilania poniżej CZAS_ZNANY_OD (jak time() na panelu bez NTP)
    p.zegar.epoch = zegarZnany ? t : 1 + (t - tOd);
    siec.tWyb = t;
    ekran.tWyb = t;
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = pw;
    siec.polaczonaTeraz = false;
    siec.okno = false;
    p.d.z.clear();
    size_t logOd = p.port.log.size();
    ekran.logOd = logOd;
    ekran.nowe.clear();
    int snow = sen.snow;
    Dyrygent(ad, k).wybudzenie();
    bool spal = sen.snow == snow + 1;
    bool bezZegara = p.zegar.teraz() < CZAS_ZNANY_OD;
    zegarZnany = !bezZegara;
    uint32_t koniec = t + (spal ? sen.msPrzySnie : p.zegar.msTeraz) / 1000;

    // naciśnięcia, których panel nie odczytał w czasie trzymania (już spał): wracają do kolejki jako budzące po
    // końcu wybudzenia; pierwsze naciśnięcie wybudzenia przyciskiem panel zawsze widzi (maska ext1)
    std::vector<Nacisniecie> widziane;
    for (size_t i = 0; i < wTym.size(); i++) {
      uint32_t off = przyc.skrypt[2 * i].first, doMs = przyc.skrypt[2 * i + 1].first;
      if ((i == 0 && pw == PowodWybudzenia::PRZYCISK) || przyc.odczytane(off, doMs)) {
        widziane.push_back(wTym[i]);
      } else {
        Nacisniecie n = wTym[i];
        n.t = std::max(n.t, koniec + 1);
        kolejka.push_back(n);
      }
    }
    // `do:{ekran}`: ostatnie widziane naciśnięcie z celem, panel jeszcze nie na celu → to samo naciśnięcie znowu
    if (!widziane.empty() && !widziane.back().cel.empty() && widziane.back().zostalo > 0 &&
        nazwaCelu((Ekran)p.start.stan.ekran) != widziane.back().cel) {
      Nacisniecie n = widziane.back();
      uint32_t przed = n.t;
      n.t = std::max(n.t + ODSTEP_DO_S, koniec + 1);
      n.zostalo--;
      // dalsze kroki tego samego ciągu (do 30 min po naciśnięciu) przesuwają się razem z powtórzeniem, więc idą po
      // dojściu do celu w tych samych odstępach
      for (auto& x : kolejka)
        if (x.t > przed && x.t <= przed + PRZESUNIECIE_DO_S) x.t += n.t - przed;
      kolejka.push_back(n);
    }
    std::stable_sort(kolejka.begin(), kolejka.end(), [](const Nacisniecie& a, const Nacisniecie& b) { return a.t < b.t; });

    // log panelu
    for (size_t i = logOd; i < p.port.log.size(); i++) logP << format(t, "%Y-%m-%d %H:%M:%S") << "  " << p.port.log[i] << "\n";
    // reguły spełnione z pamięci panelu (/dane/reguly.csv, src/reguly_pamiec.h), gdy lista się zmieniła: E i L
    // z wariantem bez kropki (wiersze pomocnicze pominięte), także „tylko raport”, którego panel nie pokazuje na ekranie
    {
      std::string akt;
      std::vector<std::vector<std::string>> spel;
      auto it = p.pliki.pliki.find("/dane/reguly.csv");
      if (it != p.pliki.pliki.end())
        for (auto& l : wiersze(it->second)) {
          if (l[0] == '#' || l.compare(0, 3, "id;") == 0) continue;
          std::vector<std::string> f;
          size_t a = 0;
          for (;;) {
            size_t b = l.find(';', a);
            f.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a));
            if (b == std::string::npos) break;
            a = b + 1;
          }
          if (f.size() < 3 || f[2].empty() || !zPoziomem(f[0], f[1])) continue;
          spel.push_back(f);
        }
      // hipoteza H-n/L-m tylko przy spełnionym wzorcu L-m
      for (auto& f : spel) {
        if (f[0].compare(0, 2, "H-") == 0) {
          std::string wz = f[1].substr(0, f[1].find_first_of("~/"));
          bool jestWz = false;
          for (auto& g : spel) jestWz = jestWz || g[0] == wz;
          if (!jestWz) continue;
        }
        akt += (akt.empty() ? "" : ", ") + f[0] + (f[1].empty() ? "" : "/" + f[1]) + " od " + f[2];
      }
      std::string noc = ostatniaNocPanelu(p.pliki.pliki, format(t, "%Y-%m-%d"), datySyntetyczne);
      if (noc != ostatniaNoc) {
        logP << format(t, "%Y-%m-%d %H:%M:%S") << "  SYMULATOR ostatnia noc w pamięci panelu: "
             << (noc.empty() ? "brak" : noc) << "\n";
        ostatniaNoc = noc;
      }
      if (akt != ostatnieReguly) {
        logP << format(t, "%Y-%m-%d %H:%M:%S") << "  SYMULATOR reguły spełnione (pamięć panelu): "
             << (akt.empty() ? "brak" : akt) << "\n";
        ostatnieReguly = akt;
      }
    }
    // powód z wiersza „cpap-eink …, wybudzenie {powód}, licznik …”
    std::string powod = pw == PowodWybudzenia::START ? "start zasilania" : pw == PowodWybudzenia::PRZYCISK ? "przycisk" : "zegar";
    for (size_t i = logOd; i < p.port.log.size(); i++) {
      const std::string& l = p.port.log[i];
      size_t a = l.find(", wybudzenie ");
      if (l.compare(0, 10, "cpap-eink ") == 0 && a != std::string::npos) {
        size_t b = l.find(", licznik", a);
        powod = l.substr(a + 13, b == std::string::npos ? std::string::npos : b - a - 13);
        break;
      }
    }
    log << format(t, "%Y-%m-%d %H:%M:%S") << " · ";
    for (auto& d : doszly) log << "[doszła noc syntetyczna z poranka " << d << "] ";
    if (bezZegara) log << "[zegar panelu nieznany] ";
    if (powod != "przycisk" || widziane.empty()) log << powod;
    for (size_t i = 0; i < widziane.size(); i++)
      log << (i || powod != "przycisk" ? " · " : "") << "przycisk " << widziane[i].opis
          << (widziane[i].t != t ? " (" + format(widziane[i].t, "%H:%M:%S") + ")" : "");
    log << " · Wi-Fi " << (!p.d.ile("siec.polacz") ? "nie" : bezSieci ? "próba, brak sieci" : "tak");
    if (ekran.nowe.empty()) {
      bool bezZmian = false;
      for (size_t i = logOd; i < p.port.log.size(); i++) bezZmian = bezZmian || zawiera(p.port.log[i], "bez odświeżenia e-ink");
      log << (bezZmian ? " · ekran bez zmian (klatka jak wyświetlona)" : " · bez rysowania");
    }
    for (auto& kl : ekran.nowe) {
      nrKlatki++;
      char nr[8];
      snprintf(nr, sizeof nr, "%03zu", nrKlatki);
      std::string ekr = bezPolskich(nazwaEkranu(kl, p.port.log, p.pliki));
      std::string nazwa = std::string(syn ? "SYN-" : "") + nr + "-" + format(kl.t, "%Y%m%d-%H%M") + "-" + ekr;
      logP << format(kl.t, "%Y-%m-%d %H:%M:%S") << "  EKRAN " << ekr << " " << nazwa << ".png\n";
      zapiszPbm(wyjscie + "/" + nazwa + ".pbm", syn ? zeZnakiemWodnym(kl.b) : kl.b);
      log << " · narysowano " << nazwa << ".png";
      // przycisk, który dał ten obrazek: tyle samo obrazków co naciśnięć → po kolei; inaczej ostatnie naciśnięcie
      // tego wybudzenia nie późniejsze niż klatka (czas co do sekundy)
      const Nacisniecie* po = nullptr;
      if (widziane.size() == ekran.nowe.size()) {
        po = &widziane[(size_t)(&kl - &ekran.nowe[0])];
      } else {
        for (auto& w : widziane)
          if (w.t <= kl.t) po = &w;
      }
      if (po) log << " (po " << po->opis << ")";
    }
    if (!spal) {
      log << " · panel nie zasnął (powrót do poprzedniej wersji albo błąd): koniec symulacji\n";
      fprintf(stderr, "UWAGA: panel nie zasnął o %s, symulacja przerwana\n", format(t, "%Y-%m-%d %H:%M").c_str());
      break;
    }
    // następne wybudzenie: z planu snu firmware albo wcześniej przyciskiem
    const PlanSnu& pl = sen.plan;
    uint32_t snu = pl.budzZegarem ? (pl.sekund ? pl.sekund : 1) : 0;
    uint32_t nast = pl.budzZegarem ? koniec + snu : tDo;
    pw = PowodWybudzenia::ZEGAR;
    while (!kolejka.empty() && kolejka.front().t < nast && !(pl.przyciski & kolejka.front().bit)) {
      log << "\n" << format(kolejka.front().t, "%Y-%m-%d %H:%M") << " · przycisk " << kolejka.front().opis
          << " nie budzi panelu (plan snu go nie słucha)";
      kolejka.erase(kolejka.begin());
    }
    if (!kolejka.empty() && kolejka.front().t < nast) {
      nast = std::max(kolejka.front().t, koniec + 1);
      pw = PowodWybudzenia::PRZYCISK;
    }
    log << " · sen do " << format(nast, "%Y-%m-%d %H:%M:%S %Z") << " · strefa " << format(t, "%Z") << "\n";
    if (!pl.budzZegarem && pw != PowodWybudzenia::PRZYCISK) {
      log << "Panel śpi do naciśnięcia przycisku (budzik zegarem wyłączony): koniec symulacji\n";
      break;
    }
    t = nast;
  }
  if (wybudzen >= najwiecej) {
    log << "UWAGA: więcej niż jedno wybudzenie na minutę, symulacja przerwana (możliwe zapętlenie firmware)\n";
    fprintf(stderr, "UWAGA: symulacja przerwana po %d wybudzeniach (możliwe zapętlenie firmware)\n", wybudzen);
  }
  for (auto& n : kolejka)
    log << "Przycisk " << n.opis << " o " << format(n.t, "%Y-%m-%d %H:%M")
        << (n.t >= tDo ? " jest po końcu okresu: pominięty\n" : " nie został użyty\n");
  // raporty dla lekarza (PDF A4), które panel zapisał w pamięci (/raporty/…), do katalogu wyniku
  for (auto& kv : p.pliki.pliki) {
    const std::string& n = kv.first;
    if (n.compare(0, 9, "/raporty/") != 0 || n.size() < 13 || n.compare(n.size() - 4, 4, ".pdf") != 0) continue;
    std::string cel = wyjscie + "/" + (syn ? "SYN-" : "") + "raport-" + n.substr(9);
    std::ofstream f(cel, std::ios::binary);
    f.write(kv.second.data(), (std::streamsize)kv.second.size());
    log << "Raport dla lekarza z pamięci panelu: " << n << " → " << fs::path(cel).filename().string() << "\n";
  }
  log << "\nRazem: " << ile((size_t)wybudzen, "wybudzenie", "wybudzenia", "wybudzeń") << ", "
      << ile(nrKlatki, "narysowany ekran", "narysowane ekrany", "narysowanych ekranów") << ".\n";
  printf("wybudzeń %d, ekranów %zu\n", wybudzen, nrKlatki);
  return 0;
}
