// SPDX-License-Identifier: GPL-3.0-or-later
// Testy awarii sieci i drugiej sesji nocy (zgłoszenia z egzemplarza testowego, 24.11.2026):
//  (1) odrzucone logowanie AirBridge (401 na `/api/status`) rano: ekran danych R-AW-35 zamiast S10 (R-EKR-57);
//  (2) awaria sieci a strażnik zadania (150 s, T-WYM-81) i tryb awaryjny U-06 (R-AW-39): najdłuższa przerwa między
//      odnowieniami strażnika w wybudzeniu z siecią, która nie odpowiada, i restarty strażnika w trakcie pracy w sieci;
//  (3) druga sesja tej samej nocy po imporcie (R-DANE-24 przy nocy z plików sesji, ADR-0036; R-DANE-69 wariant B).
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_awarie_sieci). LOG_AWARII=1 drukuje przebiegi.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "aktualizacja.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "dzien.h"
#include "nawigacja.h"
#include "noc.h"
#include "noc_pliki.h"
#include "plan.h"
#include "siec_limity.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
#define ROWNE(a, b)                                                                                     \
  do {                                                                                                  \
    std::string _a = (a), _b = (b);                                                                     \
    if (_a == _b) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)
#define ROWNE_L(a, b)                                                                                    \
  do {                                                                                                   \
    long long _a = (long long)(a), _b = (long long)(b);                                                  \
    if (_a == _b) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); }  \
  } while (0)

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";
static bool logAwarii() { return getenv("LOG_AWARII") != nullptr; }

static uint32_t ep(int r, int mi, int d, int h, int m, int s = 0) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static int minutaLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  return l.tm_hour * 60 + l.tm_min;
}
static std::string bezTwardych(std::string t) {  // twarda spacja (R-EKR-08) jako zwykła
  for (size_t p; (p = t.find("\xC2\xA0")) != std::string::npos;) t.replace(p, 2, " ");
  return t;
}
static bool wLogu(const std::vector<std::string>& log, const std::string& t) {
  for (auto& l : log)
    if (l.find(t) != std::string::npos) return true;
  return false;
}
static bool wDzienniku(const Dziennik& d, const std::string& t) {
  for (auto& z : d.z)
    if (z.find(t) != std::string::npos) return true;
  return false;
}

static const char* POLA_V3 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb;m18";
// Historia `ile` nocy przed porankiem `dzis` z terapią 23:00–07:00 (jak test/testy_poranek_szybciej.cpp).
static std::string plikNocy(const std::string& dzis, int ile) {
  std::string w;
  for (int i = ile; i >= 1; i--) {
    Noc n;
    n.data = dodajDni(dzis, -i);
    n.wazna = true;
    n.m01 = 480;
    n.m18 = 660;
    n.ahi = 1.5f;
    n.tryb = "ASVAuto";
    w += wierszNocyV3(n) + "\n";
  }
  return std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n" + w + "#koniec " + std::to_string(ile) + "\n";
}

// ============================================================================================================
// (1) Usterka 1: odrzucone logowanie AirBridge rano (R-AW-35)
// ============================================================================================================

static const char* WYZWANIE = "Digest realm=\"AirBridge\", nonce=\"abc123\", qop=\"auth\"";

// R-AW-35: teksty ekranu danych przy odrzuconym logowaniu, od pierwszej nocy bez danych, krok z numerem strony
// „Ustawień” według bieżącego ciągu stron kokpitu (R-KOK-23, R-KOK-30; ADR-0044: strona sypialni daje `n / 4`).
// wymaganie: R-AW-35
static void testR_AW_35_teksty() {
  for (int noce = 1; noce <= 3; noce++) {
    DaneDnia d;
    d.dzis = "2026-11-24";
    d.httpKod = 401;
    d.logowanieOdrzucone = true;
    d.bladData = dodajDni(d.dzis, -(noce - 1));  // pierwsza nieudana próba: od niej liczy się noc bez danych
    d.bladOd = "05:00";
    TrescEkranu t = trescDnia("S5", "", d);
    ROWNE(t.etykieta, "BRAK DANYCH Z NOCY");
    ROWNE(t.tytul, "AirBridge odrzucił logowanie");
    ROWNE(bezTwardych(t.powod), "Panel ma inne konto lub hasło niż AirBridge. Ponawiam co 30 min.");
    ROWNE(bezTwardych(t.krok), "→ Popraw konto i hasło. Ustawienia: strona 3 / 3.");
  }
  // strona „Sypialnia · 30 nocy” w kokpicie: Ustawienia na stronie 4 / 4
  ustawSypialnieWKokpicie(true);
  DaneDnia d;
  d.dzis = "2026-11-24";
  d.httpKod = 401;
  d.logowanieOdrzucone = true;
  ROWNE(bezTwardych(trescDnia("S5", "", d).krok), "→ Popraw konto i hasło. Ustawienia: strona 4 / 4.");
  ustawSypialnieWKokpicie(false);
  // brak odpowiedzi AirBridge (nie 401): teksty R-AW-32 bez zmian
  DaneDnia b;
  b.dzis = "2026-11-24";
  b.httpKod = -11;
  b.bladOd = "05:00";
  ROWNE(trescDnia("S5", "", b).tytul, "Noc 24.11 jeszcze w drodze.");
}

struct PanelL {
  Panel p;
  PanelL() {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.pliki.montujeSie = true;
    p.pliki.montowane = true;
    p.pliki.pliki["/dane/noce/2026.csv"] = plikNocy("2026-11-24", 5);
    p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  }
  void odpowiedz401() {
    p.siec.odpowiedzi.clear();
    OdpowiedzAtrapy a;
    a.o.kod = 401;
    a.o.wwwAuthenticate = WYZWANIE;
    p.siec.odpowiedzi["/api/status"].push_back(a);
    p.siec.odpowiedzi["/api/report"].push_back(a);
  }
};
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "zle-haslo", "", "", true};

static void wybudzL(PanelL& s, uint32_t t, PowodWybudzenia pw) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  if (pw == PowodWybudzenia::START) s.p.start.stan = StanRtc();
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.p.siec.polaczonaTeraz = false;
  s.p.d.z.clear();
  s.p.port.log.clear();
  Dyrygent(s.p.adaptery(), KONF).wybudzenie();
  if (logAwarii())
    for (auto& l : s.p.port.log)
      if (l.find("ekran dnia") != std::string::npos || l.find("AirBridge") != std::string::npos)
        printf("    %s\n", l.c_str());
}

// Obserwacja na egzemplarzu testowym (beta.17, 24.11.2026): `/api/status` 401, ekran dnia S10 przez ponad 4 h. Atrapa AirBridge
// odrzuca logowanie o 06:00 i o 09:35, ostatniej nocy nie ma: od pierwszej próby ekran danych S5 (E-01) z tekstami
// R-AW-35, bez S10 (R-EKR-57 wyklucza S10 przy E-01).
// wymaganie: R-AW-35
static void testR_AW_35_dyrygent() {
  PanelL s;
  s.odpowiedz401();
  wybudzL(s, ep(2026, 11, 24, 6, 0), PowodWybudzenia::START);
  SPRAWDZ(wLogu(s.p.port.log, "logowanie Digest odrzucone"));
  SPRAWDZ(!wLogu(s.p.port.log, "ekran dnia S10"));
  SPRAWDZ(wLogu(s.p.port.log, "ekran dnia S5"));
  s.odpowiedz401();
  wybudzL(s, ep(2026, 11, 24, 9, 35), PowodWybudzenia::ZEGAR);
  SPRAWDZ(!wLogu(s.p.port.log, "ekran dnia S10"));
  // 09:35 bez nowego rysowania albo znowu S5: S10 nie może wrócić
  SPRAWDZ(!wDzienniku(s.p.d, "S10"));
  // plan po odrzuconym logowaniu: dalej co 30 min (R-AW-34, R-DANE-61 bez gęstego okna)
  SPRAWDZ(s.p.sen.plan.budzZegarem && s.p.sen.plan.sekund <= 30 * 60 + 10);
}

// ============================================================================================================
// (2) Usterka 2: awaria sieci a strażnik zadania i U-06
// ============================================================================================================

// Tryby sieci, która nie odpowiada (atrapa z wirtualnym czasem, który przesuwa każda operacja o tyle, ile trwa ona
// w adapterze panelu w najgorszym razie). „Najgorszy razie” to model adaptera SiecEsp32 (src/adaptery_esp32.cpp)
// na rdzeniu Arduino-ESP32 3.3.12:
//  - BEZ_IP: Wi-Fi bez adresu (DHCP milczy): polacz() do limitu, przerwij() co 200 ms; potem opisBledu() ze skanem
//    sieci (WiFi.scanNetworks(): czekanie na koniec skanu do WiFiScanClass::_scanTimeout);
//  - DNS_MILCZY: AirBridge pod nazwą (`airbridge.local`), serwer DNS nie odpowiada: lwip_getaddrinfo() czeka na
//    koniec ponowień lwIP (NetworkManager::hostByName(), bez własnego limitu);
//  - BEZ_SYN_ACK: AirBridge pod adresem IP, TCP bez odpowiedzi: setConnectTimeout(limitMs);
//  - CIALO_MILCZY: połączenie i nagłówki są, ciało milknie (moduł zgubił sieć w połowie odpowiedzi, połączenie TCP
//    wisi bez RST): HTTPClient::writeToStreamDataBlock() czyta, dopóki connected(), bez limitu czasu;
//  - NA_LIMICIE: każda faza na swoim limicie naraz (nazwa hosta, DNS, połączenie, wysyłka, nagłówki, ciało).
enum class Awaria { BRAK, BEZ_IP, DNS_MILCZY, BEZ_SYN_ACK, CIALO_MILCZY, NA_LIMICIE };
static const char* nazwa(Awaria a) {
  switch (a) {
    case Awaria::BRAK: return "brak";
    case Awaria::BEZ_IP: return "Wi-Fi bez IP";
    case Awaria::DNS_MILCZY: return "DNS milczy";
    case Awaria::BEZ_SYN_ACK: return "TCP bez SYN-ACK";
    case Awaria::CIALO_MILCZY: return "ciało milczy";
    case Awaria::NA_LIMICIE: return "wszystko na limicie";
  }
  return "?";
}

// Model czasu adaptera ESP32 (najgorszy przypadek każdej operacji). Do commitu z testami odtwarzającymi (`main`,
// beta.17) model opisywał adapter sprzed zmiany: skan do 60 s (WiFiScan.cpp `_scanTimeout = 60000`) i ciało bez
// limitu (HTTPClient::writeToStreamDataBlock() czyta, dopóki connected()). Od poprawki T-WYM-81 (24.11.2026) adapter
// (src/adaptery_esp32.cpp czytajCialo(), opisBledu()) i ten model biorą liczby z jednego miejsca, src/siec_limity.h:
// skan SKAN_LIMIT_MS, DNS DNS_NAJDLUZEJ_MS [szacunek], wysyłka WYSYLKA_NAJDLUZEJ_MS, ciało bez danych kończy się po
// przestojCialaMs() bez odnowienia strażnika (odnowienie tylko przy nowych bajtach). Warunek testu
// (przerwa ≤ limit − zapas) bez zmian.
// Atrapa strumienia ciała z wirtualnym czasem dla prawdziwej pętli siec_limity::czytajCialo() (ta sama, której używa
// adapter panelu): `zdarzenia` = (chwila ms, bajty), które stają się dostępne o tej chwili; `zamkniecieMs` = chwila
// zamknięcia połączenia przez drugą stronę (0 = nigdy).
struct StrumienAtrapy : siec_limity::StrumienCiala {
  uint32_t teraz = 0, zamkniecieMs = 0;
  int odnowien = 0;
  std::vector<std::pair<uint32_t, std::string>> zdarzenia;
  std::string bufor;
  size_t nast = 0;
  void dociagnij() {
    while (nast < zdarzenia.size() && zdarzenia[nast].first <= teraz) bufor += zdarzenia[nast++].second;
  }
  int dostepne() override { dociagnij(); return (int)bufor.size(); }
  int czytaj(uint8_t* b, size_t n) override {
    dociagnij();
    size_t k = std::min(n, bufor.size());
    memcpy(b, bufor.data(), k);
    bufor.erase(0, k);
    return (int)k;
  }
  bool polaczony() override { dociagnij(); return !(zamkniecieMs && teraz >= zamkniecieMs && bufor.empty()); }
  uint32_t ms() override { return teraz; }
  void czekaj(uint32_t m) override { teraz += m; }
  void odnow() override { odnowien++; }
};
// Czas prawdziwej pętli ciała na strumieniu, który milknie od razu (połączenie bez danych i bez zamknięcia).
static uint32_t czasCialaMilczacego(const ZadanieHttp& z) {
  StrumienAtrapy st;
  OdpowiedzHttp o;
  o.contentLength = 1000;
  siec_limity::czytajCialo(st, z, 0, false, o);
  return st.teraz;
}

struct ModelAdaptera {
  uint32_t skanMs = siec_limity::SKAN_LIMIT_MS;
  uint32_t dnsMs = siec_limity::DNS_NAJDLUZEJ_MS;
  uint32_t wysylkaMs = siec_limity::WYSYLKA_NAJDLUZEJ_MS;
  // ciało, które milknie: ile trwa, zanim get() wróci; 0 = nie wraca (bez limitu; w atrapie godzina)
  uint32_t cialoMilczyMs(const ZadanieHttp& z) const { return czasCialaMilczacego(z); }  // prawdziwa pętla
  // odnowienie strażnika przez adapter w trakcie get() przy ciele bez danych: nie (tylko przy nowych bajtach)
  bool odnawiaWCiele = false;
};
static const uint32_t GODZINA_MS = 3600u * 1000u;

class AtrapaSieciAwarii : public AtrapaSieci {
 public:
  AtrapaSieciAwarii(Dziennik& d, AtrapaZegara& z, AtrapaStraznika& s) : AtrapaSieci(d, z), zg(z), st(s) {}
  Awaria awaria = Awaria::BRAK;
  ModelAdaptera model;
  bool zawieszenie = false;  // get() nigdy nie wraca: restart strażnika (wyjątek RestartStraznika)
  struct RestartStraznika {};
  bool polacz(uint32_t limitMs, const std::function<bool()>& przerwij, bool& przerwane) override {
    if (awaria != Awaria::BEZ_IP) return AtrapaSieci::polacz(limitMs, przerwij, przerwane);
    przerwane = false;
    for (uint32_t t = 0; t < limitMs; t += 200) {  // jak SiecEsp32::polacz(): przerwij() co 200 ms
      if (przerwij && przerwij()) {
        przerwane = true;
        return false;
      }
      zg.przesun(200);
    }
    polaczonaTeraz = false;
    return false;
  }
  std::string opisBledu() override {
    if (awaria == Awaria::BEZ_IP) zg.przesun(model.skanMs);
    return "atrapa: awaria sieci";
  }
  bool czasZSieci(uint32_t limitMs) override {
    if (awaria == Awaria::BRAK || awaria == Awaria::BEZ_IP) return AtrapaSieci::czasZSieci(limitMs);
    zg.przesun(limitMs);  // SNTP bez odpowiedzi: do limitu
    return false;
  }
  OdpowiedzHttp get(const ZadanieHttp& z) override {
    if (zawieszenie) throw RestartStraznika{};
    if (awaria == Awaria::BRAK || awaria == Awaria::BEZ_IP) return AtrapaSieci::get(z);
    wywolania++;
    zadania.push_back(z.url);
    OdpowiedzHttp o;
    bool nazwaHosta = z.url.find("://airbridge") != std::string::npos || z.url.find("://cpap-") != std::string::npos;
    switch (awaria) {
      case Awaria::DNS_MILCZY:
        zg.przesun(nazwaHosta ? model.dnsMs : z.limitMs);
        o.kod = -1;
        return o;
      case Awaria::BEZ_SYN_ACK:
        zg.przesun(z.limitMs);
        o.kod = -1;
        return o;
      case Awaria::CIALO_MILCZY:
      case Awaria::NA_LIMICIE: {
        if (awaria == Awaria::NA_LIMICIE)
          zg.przesun((nazwaHosta ? model.dnsMs : 0) + z.limitMs + model.wysylkaMs + z.limitMs);
        else
          zg.przesun(100);
        uint32_t c = model.cialoMilczyMs(z);
        if (model.odnawiaWCiele) st.odnow();
        zg.przesun(c ? c : GODZINA_MS);
        o.kod = 200;
        o.contentLength = 1000;
        o.wynikZapisu = -10;  // ciało urwane
        return o;
      }
      default: break;
    }
    return o;
  }
  AtrapaZegara& zg;
  AtrapaStraznika& st;
};

struct PanelS {
  Panel p;
  AtrapaSieciAwarii siec{p.d, p.zegar, p.straznik};
  explicit PanelS(const std::string& dzis = "2026-11-24") {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.pliki.montujeSie = true;
    p.pliki.montowane = true;
    p.pliki.pliki["/dane/noce/2026.csv"] = plikNocy(dzis, 5);
    p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  }
  Adaptery adaptery() {
    return Adaptery{p.zegar, p.nvs,   p.pliki, siec,   p.ekran, p.przyciski, p.czujniki,
                    p.dzwiek, p.start, p.straznik, p.sen, p.port, p.diag,     &p.wydania};
  }
};
struct WynikS {
  bool restart = false;  // wybudzenie skończyło się restartem strażnika (zawieszenie)
  bool spal = false;
  uint32_t trwaloMs = 0;
};
static WynikS wybudzS(PanelS& s, const KonfiguracjaPanelu& k, uint32_t t, PrzyczynaStartu prz, PowodWybudzenia pw) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  // po restarcie programu (inny niż wybudzenie z uśpienia) pamięć RTC_DATA jest od nowa; licznik startów
  // (RTC_NOINIT, src/start.h) zostaje
  if (prz != PrzyczynaStartu::USPIENIE) s.p.start.stan = StanRtc();
  s.p.start.przyczynaStartu = prz;
  s.p.start.powodWybudzenia = pw;
  s.siec.polaczonaTeraz = false;
  s.p.d.z.clear();
  s.p.port.log.clear();
  s.p.straznik.najdluzszaPrzerwaMs = 0;
  s.p.straznik.przekroczen = 0;
  int snow = s.p.sen.snow;
  WynikS w;
  try {
    Dyrygent(s.adaptery(), k).wybudzenie();
  } catch (const AtrapaSieciAwarii::RestartStraznika&) {
    w.restart = true;
  }
  s.p.straznik.sprawdz();
  w.spal = s.p.sen.snow == snow + 1;
  w.trwaloMs = s.p.zegar.msTeraz;
  return w;
}

// Zapas między najdłuższą przerwą bez odnowienia strażnika a jego limitem (STRAZNIK_LIMIT_S) [PRÓG] techniczny
// (src/siec_limity.h; do poprawki stała 30 000 ms w tym pliku, ta sama wartość).
static const uint32_t ZAPAS_STRAZNIKA_MS = siec_limity::ZAPAS_STRAZNIKA_MS;

// Rachunek najgorszego przypadku z src/siec_limity.h dla każdego rodzaju żądania panelu: AirBridge (15 s), ocena
// slotu i źródło otoczenia (3 s), AirBridge pod nazwą (DNS), HTTPS wydań (20 s): każde krótsze niż limit strażnika
// z zapasem; odczyt ciała kończy się po przestoju i po limicie całkowitym; adres IP bez DNS.
// wymaganie: T-WYM-81
static void testT_WYM_81_rachunek() {
  const uint32_t dozw = siec_limity::dozwolonaPrzerwaMs(STRAZNIK_LIMIT_S);
  ROWNE_L(dozw, 120000);
  for (const char* host : {"192.168.1.20", "airbridge.local"})
    for (uint32_t limit : {AB_LIMIT_ODPOWIEDZI_MS, LIMIT_STATUSU_SLOTU_MS, 3000u, 20000u}) {
      ZadanieHttp z;
      z.url = std::string("http://") + host + "/api/status";
      z.limitMs = limit;
      uint32_t p = siec_limity::najdluzszaPrzerwaGetMs(z);
      printf("  GET %s, limit %u s: najdłuższa przerwa strażnika %u s (dozwolone %u s)\n", host, (unsigned)(limit / 1000),
             (unsigned)(p / 1000), (unsigned)(dozw / 1000));
      SPRAWDZ(p <= dozw);
    }
  ZadanieHttp ip;
  ip.url = "http://192.168.1.20/api/status";
  ip.limitMs = 15000;
  ROWNE_L(siec_limity::najdluzszaPrzerwaGetMs(ip), 15000 + 10000 + 15000 + 15000);
  uint32_t https = siec_limity::najdluzszaPrzerwaHttpsMs(aktualizacja::LIMIT_POLACZENIA_MS);
  printf("  HTTPS wydań: najdłuższa przerwa strażnika %u s\n", (unsigned)(https / 1000));
  SPRAWDZ(https <= dozw);
  // host: nazwa albo IPv4
  SPRAWDZ(!siec_limity::hostJestNazwa("http://192.168.1.20/api/status"));
  SPRAWDZ(!siec_limity::hostJestNazwa("http://192.168.100.200:8080/api/x"));
  SPRAWDZ(siec_limity::hostJestNazwa("http://airbridge.local/api/status"));
  SPRAWDZ(siec_limity::hostJestNazwa("https://api.open-meteo.com/v1/forecast?x"));
  SPRAWDZ(siec_limity::hostJestNazwa("http://1.2.3/x"));
  SPRAWDZ(siec_limity::hostJestNazwa("http://1234.2.3.4/x"));
  // decyzja w pętli ciała: przestój = limitMs, limit całkowity = budżet albo CIALO_NAJDLUZEJ_MS
  ZadanieHttp z;
  z.limitMs = 15000;
  SPRAWDZ(siec_limity::stanCiala(z, 0, 0, 14999) == siec_limity::StanCiala::CZYTAJ);
  SPRAWDZ(siec_limity::stanCiala(z, 0, 0, 15000) == siec_limity::StanCiala::PRZESTOJ);
  SPRAWDZ(siec_limity::stanCiala(z, 0, 110000, 119999) == siec_limity::StanCiala::CZYTAJ);
  SPRAWDZ(siec_limity::stanCiala(z, 0, 119000, 120000) == siec_limity::StanCiala::ZA_DLUGO);
  z.budzetCialaMs = 5000;
  SPRAWDZ(siec_limity::stanCiala(z, 1000, 5000, 6000) == siec_limity::StanCiala::ZA_DLUGO);
  z.budzetCialaMs = 0;
  SPRAWDZ(siec_limity::stanCiala(z, 4294967000u, 4294967000u, 1000) == siec_limity::StanCiala::CZYTAJ);  // przepełnienie millis()
  z.limitMs = 0;
  ROWNE_L(siec_limity::przestojCialaMs(z), siec_limity::PRZESTOJ_DOMYSLNY_MS);
}

// Prawdziwa pętla ciała adaptera (siec_limity::czytajCialo(), wspólna z src/adaptery_esp32.cpp) na atrapie strumienia:
// milczenie, kapanie, `chunked` w kawałkach, urwanie przed Content-Length, przekroczenie maksBajtow, koniec przez
// zamknięcie bez długości (testy na prawdziwym kodzie, nie na modelu).
// wymaganie: T-WYM-81
static void testT_WYM_81_petlaCiala() {
  ZadanieHttp z;
  z.limitMs = 15000;
  {  // milczy od razu: koniec po przestoju, bez odnowienia strażnika
    StrumienAtrapy st;
    OdpowiedzHttp o;
    o.contentLength = 1000;
    ROWNE_L(siec_limity::czytajCialo(st, z, 0, false, o), -11);
    SPRAWDZ(st.teraz >= 15000 && st.teraz <= 15000 + siec_limity::OBIEG_CIALA_MS);
    ROWNE_L(st.odnowien, 0);
    SPRAWDZ(!o.zaDlugo);
  }
  {  // kapie co 10 s (krócej niż przestój): płynie do limitu całkowitego 120 s, strażnik przy każdym bajcie
    StrumienAtrapy st;
    for (uint32_t t = 0; t < 300000; t += 10000) st.zdarzenia.push_back({t, "x"});
    OdpowiedzHttp o;
    o.contentLength = 1000;
    ROWNE_L(siec_limity::czytajCialo(st, z, 0, false, o), -11);
    SPRAWDZ(o.zaDlugo);
    SPRAWDZ(st.teraz >= 120000 && st.teraz <= 120000 + siec_limity::OBIEG_CIALA_MS);
    ROWNE_L(st.odnowien, 12);
    ROWNE_L(o.cialo.size(), 12);
  }
  {  // chunked w kawałkach rozciętych w środku rozmiaru i danych, z przerwami krótszymi niż przestój
    StrumienAtrapy st;
    st.zdarzenia = {{0, "4\r\nWi"}, {3000, "ki\r\n6;x=1\r"}, {6000, "\npedia \r\n"}, {9000, "0\r\n\r\n"}};
    OdpowiedzHttp o;
    ROWNE_L(siec_limity::czytajCialo(st, z, 0, true, o), 10);
    ROWNE(o.cialo, "Wikipedia ");
  }
  {  // urwane przed Content-Length: zamknięcie po 40 z 100 bajtów
    StrumienAtrapy st;
    st.zdarzenia = {{0, std::string(40, 'a')}};
    st.zamkniecieMs = 1000;
    OdpowiedzHttp o;
    o.contentLength = 100;
    ROWNE_L(siec_limity::czytajCialo(st, z, 0, false, o), -11);
    SPRAWDZ(st.teraz < 2000);
  }
  {  // ponad maksBajtow: koniec od razu, bez nadmiaru w ciele
    StrumienAtrapy st;
    st.zdarzenia = {{0, std::string(50, 'b')}};
    ZadanieHttp m = z;
    m.maksBajtow = 10;
    OdpowiedzHttp o;
    ROWNE_L(siec_limity::czytajCialo(st, m, 0, false, o), -10);
    SPRAWDZ(o.zaDuzo && o.cialo.size() <= 10);
  }
  {  // bez długości: koniec przez zamknięcie, ciało kompletne
    StrumienAtrapy st;
    st.zdarzenia = {{0, "{\"a\":1}"}};
    st.zamkniecieMs = 500;
    OdpowiedzHttp o;
    ROWNE_L(siec_limity::czytajCialo(st, z, 0, false, o), 7);
    ROWNE(o.cialo, "{\"a\":1}");
  }
  {  // przerwanie przyciskiem
    StrumienAtrapy st;
    ZadanieHttp p = z;
    p.przerwij = [] { return true; };
    OdpowiedzHttp o;
    o.contentLength = 10;
    ROWNE_L(siec_limity::czytajCialo(st, p, 0, false, o), -10);
    SPRAWDZ(o.przerwane);
  }
}

// Dekoder `chunked` w pętli ciała adaptera: kawałki z rozszerzeniem, dowolny podział wejścia, kawałek 0 kończy,
// zły znak i za długi rozmiar to błąd.
// wymaganie: T-WYM-81
static void testT_WYM_81_chunked() {
  const std::string we = "4\r\nWiki\r\n6;x=1\r\npedia \r\nE\r\nin \r\n\r\nchunks.\r\n0\r\n\r\n";
  for (size_t krok = 1; krok <= we.size(); krok++) {
    siec_limity::DekoderChunked d;
    std::string wy;
    bool okD = true;
    for (size_t i = 0; i < we.size(); i += krok)
      okD = d.dodaj((const uint8_t*)we.data() + i, std::min(krok, we.size() - i), wy) && okD;
    SPRAWDZ(okD && d.koniec() && !d.blad());
    ROWNE(wy, "Wikipedia in \r\n\r\nchunks.");
  }
  for (const char* zle : {"x\r\n", "4\nWiki", "4\r\nWikiX\r\n", "123456789\r\n", "\r\n"}) {
    siec_limity::DekoderChunked d;
    std::string wy;
    d.dodaj((const uint8_t*)zle, strlen(zle), wy);
    SPRAWDZ(d.blad());
  }
  siec_limity::DekoderChunked d;
  std::string wy;
  SPRAWDZ(d.dodaj((const uint8_t*)"A\r\n01234", 8, wy) && !d.koniec() && wy == "01234");
}

// Awaria sieci w każdym trybie: najdłuższa przerwa między odnowieniami strażnika w wybudzeniu z pobraniem rano jest
// krótsza niż limit strażnika z zapasem, więc samo milczenie sieci nie kończy wybudzenia restartem (T-WYM-81).
// wymaganie: T-WYM-81
static void testT_WYM_81_przerwaStraznika() {
  for (Awaria a : {Awaria::BEZ_IP, Awaria::DNS_MILCZY, Awaria::BEZ_SYN_ACK, Awaria::CIALO_MILCZY, Awaria::NA_LIMICIE}) {
    for (int host = 0; host < 2; host++) {
      PanelS s;
      s.siec.awaria = a;
      KonfiguracjaPanelu k = KONF;
      k.abHaslo = "haslo";
      if (host) k.abAdres = "airbridge.local";
      WynikS w = wybudzS(s, k, ep(2026, 11, 24, 7, 0), PrzyczynaStartu::ZASILANIE, PowodWybudzenia::START);
      printf("  %s, AirBridge pod %s: najdłuższa przerwa strażnika %u s, wybudzenie %u s\n", nazwa(a),
             host ? "nazwą" : "adresem IP", (unsigned)(s.p.straznik.najdluzszaPrzerwaMs / 1000),
             (unsigned)(w.trwaloMs / 1000));
      SPRAWDZ(!w.restart);
      SPRAWDZ(s.p.straznik.najdluzszaPrzerwaMs <= STRAZNIK_LIMIT_S * 1000 - ZAPAS_STRAZNIKA_MS);
      ROWNE_L(s.p.straznik.przekroczen, 0);
      SPRAWDZ(w.spal);
    }
  }
}

// R-AW-39 (zmiana 2026-11-24): restart strażnika w trakcie pracy w sieci nie liczy się do trybu awaryjnego. Sieć
// zawiesza każde żądanie (get() nie wraca, strażnik restartuje program): kolejne starty po restarcie w sieci i zwykłe
// wybudzenia na zmianę, ani razu U-06; po restarcie w sieci wybudzenie bez sieci rysuje ekran braku danych (S10 przed
// 11:00, S5 od 11:00) i zasypia.
// wymaganie: R-AW-39
static void testR_AW_39_restartyWSieci() {
  PanelS s;
  s.siec.zawieszenie = true;
  KonfiguracjaPanelu k = KONF;
  k.abHaslo = "haslo";
  uint32_t t = ep(2026, 11, 24, 7, 0);
  PrzyczynaStartu prz = PrzyczynaStartu::ZASILANIE;
  PowodWybudzenia pw = PowodWybudzenia::START;
  int restartow = 0, ekranowBraku = 0, u06 = 0;
  for (int i = 0; i < 8; i++) {
    WynikS w = wybudzS(s, k, t, prz, pw);
    bool awaryjny = wLogu(s.p.port.log, "tryb awaryjny");
    u06 += awaryjny;
    if (logAwarii())
      printf("    %02d:%02d %s: %s\n", minutaLok(t) / 60, minutaLok(t) % 60,
             prz == PrzyczynaStartu::TASK_WDT ? "start po strażniku" : "start", w.restart ? "restart strażnika" :
             awaryjny ? "U-06" : "sen");
    if (w.restart) {
      restartow++;
      prz = PrzyczynaStartu::TASK_WDT;
      pw = PowodWybudzenia::START;
      t += 150;
      continue;
    }
    if (!awaryjny && (wLogu(s.p.port.log, "ekran dnia S10") || wLogu(s.p.port.log, "ekran dnia S5"))) ekranowBraku++;
    prz = PrzyczynaStartu::USPIENIE;
    pw = PowodWybudzenia::ZEGAR;
    uint32_t snu = s.p.sen.plan.budzZegarem ? s.p.sen.plan.sekund : OKRES_SNU_S;
    t += s.p.zegar.msTeraz / 1000 + (snu ? snu : 1);
  }
  SPRAWDZ(restartow >= 3);
  ROWNE_L(u06, 0);
  SPRAWDZ(s.p.nvs.tekst(NVS_PRZESTRZEN, NVS_KLUCZ_AWARYJNY, "").empty());
  SPRAWDZ(ekranowBraku >= 3);
}

// R-AW-39 bez zmian poza siecią: zawieszenie poza pracą w sieci (restart strażnika bez znacznika sieci) i błąd programu
// dalej liczą się do trybu awaryjnego (3 starty po błędzie → U-06).
// wymaganie: R-AW-39
static void testR_AW_39_pozaSieciaBezZmian() {
  LicznikStartow l{};
  zerujPrzedSnem(l, 7);
  ZnacznikAwaryjny z;
  DecyzjaStartu d;
  for (int i = 0; i < 3; i++) d = decyzjaStartu(l, PrzyczynaStartu::TASK_WDT, 7, z);
  SPRAWDZ(d.awaryjny);
  LicznikStartow l2{};
  zerujPrzedSnem(l2, 7);
  for (int i = 0; i < 3; i++) d = decyzjaStartu(l2, PrzyczynaStartu::PANIKA, 7, z);
  SPRAWDZ(d.awaryjny);
}

// ============================================================================================================
// (3) Usterka 3: druga sesja tej samej nocy (R-DANE-24 przy nocy z plików sesji; R-DANE-69 wariant B)
// ============================================================================================================

// budowa plików EDF jak w test/testy_noc_pliki.cpp (PLD, EVE, CSL)
struct SygT {
  std::string etykieta, jednostka;
  double fizMin, fizMax;
  long cyfMin, cyfMax;
  int probki;
};
static std::string pole(const std::string& t, size_t szer) {
  std::string x = t.substr(0, szer);
  x.resize(szer, ' ');
  return x;
}
static std::string num(double v) {
  char b[32];
  if (v == std::floor(v) && std::fabs(v) < 1e9) snprintf(b, sizeof b, "%ld", (long)v);
  else snprintf(b, sizeof b, "%g", v);
  return b;
}
static std::string budujEdf(long long start, double czasRekordu, const std::vector<SygT>& syg,
                            const std::vector<std::vector<std::string>>& rekordy, const std::string& zastrz = "") {
  long dzien = (long)(start / 86400);
  long sek = (long)(start % 86400);
  std::string d = dataZNr(dzien);
  char g[16];
  snprintf(g, sizeof g, "%02ld.%02ld.%02ld", sek / 3600, sek / 60 % 60, sek % 60);
  size_t ns = syg.size();
  std::string h = pole("0", 8) + pole("X", 80) + pole("Startdate X", 80) +
                  pole(d.substr(8, 2) + "." + d.substr(5, 2) + "." + d.substr(2, 2), 8) + pole(g, 8) +
                  pole(std::to_string(256 + 256 * ns), 8) + pole(zastrz, 44) + pole(std::to_string(rekordy.size()), 8) +
                  pole(num(czasRekordu), 8) + pole(std::to_string(ns), 4);
  for (auto& s : syg) h += pole(s.etykieta, 16);
  for (size_t i = 0; i < ns; i++) h += pole("", 80);
  for (auto& s : syg) h += pole(s.jednostka, 8);
  for (auto& s : syg) h += pole(num(s.fizMin), 8);
  for (auto& s : syg) h += pole(num(s.fizMax), 8);
  for (auto& s : syg) h += pole(num((double)s.cyfMin), 8);
  for (auto& s : syg) h += pole(num((double)s.cyfMax), 8);
  for (size_t i = 0; i < ns; i++) h += pole("", 80);
  for (auto& s : syg) h += pole(std::to_string(s.probki), 8);
  for (size_t i = 0; i < ns; i++) h += pole("", 32);
  for (auto& r : rekordy)
    for (size_t i = 0; i < ns; i++) {
      std::string b = i < r.size() ? r[i] : "";
      b.resize((size_t)syg[i].probki * 2, '\0');
      h += b;
    }
  return h;
}
static std::string probki(const std::vector<long>& v) {
  std::string b;
  for (long x : v) {
    uint16_t u = (uint16_t)(x & 0xFFFF);
    b += (char)(u & 0xFF);
    b += (char)(u >> 8);
  }
  return b;
}
static long cyf(const SygT& s, double v) {
  return std::lround((double)s.cyfMin + (v - s.fizMin) * (double)(s.cyfMax - s.cyfMin) / (s.fizMax - s.fizMin));
}
static const SygT CRC = {"Crc16", "", -32768, 32767, -32768, 32767, 1};
static const std::vector<SygT> PLD_SYG = {
    {"MaskPress.2s", "cmH2O", 0, 30, 0, 1500, 30}, {"Press.2s", "cmH2O", 0, 30, 0, 1500, 30},
    {"EprPress.2s", "cmH2O", 0, 30, 0, 1500, 30},  {"Leak.2s", "L/s", 0, 2, 0, 100, 30},
    {"RespRate.2s", "bpm", 0, 50, 0, 250, 30},     {"TidVol.2s", "L", 0, 4, 0, 200, 30},
    {"MinVent.2s", "L/min", 0, 30, 0, 240, 30},    {"Snore.2s", "", 0, 5, 0, 250, 30},
    {"FlowLim.2s", "", 0, 1, 0, 100, 30},          {"TgtVent.2s", "L/min", 0, 30, 0, 240, 30},
    CRC,
};
static std::string pld(long long start, int minuty) {
  std::map<std::string, double> w = {{"MaskPress.2s", 14}, {"Press.2s", 14}, {"EprPress.2s", 7}, {"Leak.2s", 0.1},
                                     {"RespRate.2s", 15},  {"TidVol.2s", 0.46}, {"MinVent.2s", 7.5},
                                     {"Snore.2s", 0.1},    {"FlowLim.2s", 0.05}, {"TgtVent.2s", 6}};
  std::vector<std::vector<std::string>> rek;
  for (int i = 0; i < minuty; i++) {
    std::vector<std::string> r;
    for (auto& s : PLD_SYG)
      r.push_back(s.etykieta == "Crc16" ? probki({0}) : probki(std::vector<long>(30, cyf(s, w[s.etykieta]))));
    rek.push_back(r);
  }
  return budujEdf(start, 60, PLD_SYG, rek);
}
static std::string adnotacje(long long start) {
  std::vector<SygT> syg = {{"EDF Annotations", "", 0, 1, -32768, 32767, 40}, CRC};
  std::string tal = std::string("+0\x14\x14", 4) + std::string(1, '\0') + "+600\x15" "10\x14Hypopnea\x14" +
                    std::string(1, '\0');
  return budujEdf(start, 0, syg, {{tal, probki({0})}}, "EDF+D");
}
static long long cyw(const std::string& data, int h, int m) { return (long long)nrDnia(data) * 86400LL + h * 3600 + m * 60; }
static std::string prefiks(long long t) {
  std::string d = dataZNr((long)(t / 86400));
  long s = (long)(t % 86400);
  char b[32];
  snprintf(b, sizeof b, "%s%s%s_%02ld%02ld%02ld", d.substr(0, 4).c_str(), d.substr(5, 2).c_str(), d.substr(8, 2).c_str(),
           s / 3600, s / 60 % 60, s % 60);
  return b;
}
using PlikiDoby = std::map<std::string, std::string>;
struct Sesja {
  long long start;
  int minuty;
  PlikiDoby pliki;
};
static Sesja sesja(long long start, int minuty) {
  Sesja s{start, minuty, {}};
  std::string pr = prefiks(start);
  s.pliki[pr + "_PLD.edf"] = pld(start, minuty);
  s.pliki[pr + "_EVE.edf"] = adnotacje(start);
  s.pliki[pr + "_CSL.edf"] = adnotacje(start);
  return s;
}
static std::string sesjeJson(long dzien, const std::vector<Sesja>& v) {
  std::string kat = "/DATALOG/" + noc_pliki::dzienAB(dzien) + "/", s;
  for (auto& x : v) {
    std::string f;
    for (auto& p : x.pliki)
      f += std::string(f.empty() ? "" : ",") + "{\"path\":\"" + kat + p.first + "\",\"size\":" +
           std::to_string(p.second.size()) + "}";
    s += std::string(s.empty() ? "" : ",") + "{\"day\":\"" + noc_pliki::dzienAB(dzien) +
         "\",\"start\":\"x\",\"end\":null,\"closed\":true,\"files\":[" + f + "]}";
  }
  return "{\"sessions\":[" + s + "],\"str\":null,\"next_cursor\":null}";
}
static std::string strDoby(long dzien) {
  std::vector<SygT> syg = {{"Date", "", 0, 32767, 0, 32767, 1},           {"Duration", "min.", 0, 1440, 0, 1440, 1},
                           {"Mode", "", 0, 16, 0, 16, 1},                 {"S.AA.MinEPAP", "cmH2O", 1, 30, 50, 1500, 1},
                           {"S.AA.MaxEPAP", "cmH2O", 1, 30, 50, 1500, 1}, {"S.AA.MinPS", "cmH2O", 0, 30, 0, 1500, 1},
                           {"S.AA.MaxPS", "cmH2O", 0, 30, 0, 1500, 1},    {"AHI", "", 0, 240, 0, 2400, 1},
                           CRC};
  std::vector<double> v = {(double)dzien, 480, 8, 6, 10, 3, 15, 2};
  std::vector<std::string> r;
  for (size_t i = 0; i < v.size(); i++) r.push_back(probki({cyf(syg[i], v[i])}));
  r.push_back(probki({0}));
  return budujEdf((long long)dzien * 86400 + 12 * 3600, 86400, syg, {r});
}
static std::string raport(long day, int minut, int sesji) {
  return "{\"day\":" + std::to_string(day) + ",\"present\":true,\"partial\":false,\"used_seconds\":" +
         std::to_string(minut * 60) + ",\"ahi\":1.4,\"events_known\":true,\"session_count\":" + std::to_string(sesji) +
         ",\"counts\":{\"oa\":1,\"ca\":5,\"h\":4},\"channels\":[{\"id\":\"Press\",\"unit\":\"cmH2O\",\"median\":9.8,"
         "\"p95\":13.6,\"source\":\"edf\"}],\"settings\":{\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"value\":"
         "\"ASVAuto\",\"option\":8}]}}";
}
static std::string lista(long day, int sesji) {
  return "{\"source\":\"edf\",\"days\":[{\"day\":" + std::to_string(day) + ",\"session_count\":" +
         std::to_string(sesji) + ",\"stale\":false}]}";
}
static std::string status(int rop, const std::string& pref) {
  return std::string("{\"version\":\"v2.3.1\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"srn\":\"SYN00000001\",") +
         "\"mode\":\"ASVAuto\",\"system\":\"IDLE\",\"rop\":" + std::to_string(rop) + ",\"edf\":\"" +
         (rop ? "recording" : "idle") + "\",\"edf_prefix\":\"" + pref + "\",\"report_state\":\"" +
         (rop ? "blocked" : "idle") + "\"}";
}

static const std::string DZIS3 = "2026-11-24";
static const long DOBA3 = nrDnia(DZIS3) - 1;  // doba AirBridge nocy z poranka 24.11

// Stan AirBridge w chwili t (pierwsza sesja 23:10–05:55, druga 06:10–07:40, jak na egzemplarzu testowym). `drugaMin` = 0: bez
// drugiej sesji (zwykły poranek).
struct DrugaSesja {
  explicit DrugaSesja(int drugaOdMin = 6 * 60 + 10, int drugaMin = 90)
      : s2(sesja(cyw(DZIS3, drugaOdMin / 60, drugaOdMin % 60), drugaMin ? drugaMin : 1)),
        start2(drugaMin ? ep(2026, 11, 24, drugaOdMin / 60, drugaOdMin % 60) : 0xFFFFFFF0u),
        koniec2(drugaMin ? start2 + (uint32_t)drugaMin * 60 : 0xFFFFFFF0u) {}
  Sesja s1 = sesja(cyw("2026-11-23", 23, 10), 405);
  Sesja s2;
  uint32_t koniec1 = ep(2026, 11, 24, 5, 55), start2, koniec2;
};
static void odpowiedziDS(Panel& p, const DrugaSesja& d, uint32_t t) {
  p.siec.odpowiedzi.clear();
  bool terapia = t < d.koniec1 || (t >= d.start2 && t < d.koniec2);
  bool druga = t >= d.start2;
  p.siec.dodaj("/api/status", 200, status(terapia ? 1 : 0, prefiks(druga ? d.s2.start : d.s1.start)));
  if (t < d.koniec1) return;
  std::vector<Sesja> ses = {d.s1};
  if (t >= d.koniec2) ses.push_back(d.s2);
  int n = (int)ses.size();
  int minut = d.s1.minuty + (n > 1 ? d.s2.minuty : 0);
  p.siec.dodaj(sciezkaListy(), 200, lista(DOBA3, n));
  p.siec.dodaj(sciezkaNocy(DOBA3), 200, raport(DOBA3, minut, n));
  p.siec.dodaj("/api/sessions?order=desc", 404, "");
  p.siec.dodaj("/api/sessions?since=", 200, sesjeJson(DOBA3, ses));
  p.siec.dodaj(sciezkaPobraniaStr(), 200, strDoby(DOBA3));
  std::string kat = "/DATALOG/" + noc_pliki::dzienAB(DOBA3) + "/";
  for (auto& s : ses)
    for (auto& x : s.pliki) p.siec.dodaj(noc_pliki::sciezkaPobraniaPliku(kat + x.first), 200, x.second);
}
struct PanelD {
  Panel p;
  PanelD() {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.pliki.montujeSie = true;
    p.pliki.montowane = true;
    p.pliki.pliki["/dane/noce/2026.csv"] = plikNocy(DZIS3, 5);
    p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=plik\n";
  }
  std::vector<std::string> wiersz(const std::string& data) {
    std::string t = p.pliki.tresc("/dane/noce/2026.csv");
    size_t a = t.find("\n" + data + ";");
    if (a == std::string::npos) return {};
    size_t b = t.find('\n', a + 1);
    std::string l = t.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
    std::vector<std::string> v;
    size_t s = 0;
    for (size_t i = 0; i <= l.size(); i++)
      if (i == l.size() || l[i] == ';') {
        v.push_back(l.substr(s, i - s));
        s = i + 1;
      }
    return v;
  }
  std::string m01() {
    auto w = wiersz(DZIS3);
    return w.size() > 2 ? w[2] : "";
  }
  std::string m17() {
    auto w = wiersz(DZIS3);
    return w.size() > 22 ? w[22] : "";
  }
};
struct WybD {
  uint32_t t = 0;
  int polaczen = 0, odswiezen = 0;
  bool spal = false;
  std::string m01, powody;
};
static WybD wybudzD(PanelD& s, const DrugaSesja& d, uint32_t t, PowodWybudzenia pw) {
  odpowiedziDS(s.p, d, t);
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  if (pw == PowodWybudzenia::START) s.p.start.stan = StanRtc();
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.p.siec.polaczonaTeraz = false;
  s.p.d.z.clear();
  s.p.port.log.clear();
  int snow = s.p.sen.snow;
  Dyrygent(s.p.adaptery(), KONF).wybudzenie();
  WybD w;
  w.t = t;
  w.spal = s.p.sen.snow == snow + 1;
  w.m01 = s.m01();
  for (auto& z : s.p.d.z) {
    w.polaczen += z == "siec.polacz";
    w.odswiezen += z == "ekran.odswiez";
    size_t a = z.find("wybudzenie ");
    if (w.powody.empty() && a != std::string::npos && z.find("cpap-eink") != std::string::npos) {
      size_t b = z.find(',', a);
      w.powody = z.substr(a + 11, b == std::string::npos ? std::string::npos : b - a - 11);
    }
  }
  if (logAwarii())
    printf("    %02d:%02d Wi-Fi %d, e-ink %d, M-01 %s, %s\n", minutaLok(t) / 60, minutaLok(t) % 60, w.polaczen,
           w.odswiezen, w.m01.c_str(), w.powody.c_str());
  return w;
}

// R-DANE-24 przy domyślnym źródle nocy z plików sesji (ADR-0036, `noc_zrodlo=plik`): sesja 23:10–05:55, import
// o 06:00, druga sesja 06:10–07:40, sprawdzenie o 11:00 → rekord z dwiema sesjami (M-01 = 405 + 90 min, M-17 = 1),
// ekran dnia narysowany od nowa.
// wymaganie: R-DANE-24
static void testR_DANE_24_nocZPlikow() {
  PanelD s;
  DrugaSesja d;
  wybudzD(s, d, ep(2026, 11, 24, 6, 0), PowodWybudzenia::START);
  ROWNE(s.m01(), "405");
  SPRAWDZ(wLogu(s.p.port.log, "z plików sesji: 1 sesji"));
  WybD w = wybudzD(s, d, ep(2026, 11, 24, 11, 0), PowodWybudzenia::ZEGAR);
  ROWNE(s.m01(), "495");
  ROWNE(s.m17(), "1");
  SPRAWDZ(wLogu(s.p.port.log, "z plików sesji: 2 sesji"));
  SPRAWDZ(w.odswiezen >= 1);
}

struct PrzebiegDS {
  uint32_t import1 = 0, dwie = 0;
  int odswiezPrzyDwoch = 0, zDrugaSesja = 0, zWifi = 0;
};
static std::string hhmm(uint32_t t) {
  char b[32];
  snprintf(b, sizeof b, "%02d:%02d", minutaLok(t) / 60, minutaLok(t) % 60);
  return t ? b : "brak";
}
// Przebieg planem od 05:00 do 11:30 (wybudzenia zegarem według planu panelu).
static PrzebiegDS przebiegDS(PanelD& s, const DrugaSesja& d, int minutDwoch) {
  PrzebiegDS p;
  uint32_t t = ep(2026, 11, 24, 5, 0);
  PowodWybudzenia pw = PowodWybudzenia::START;
  std::string dwie = std::to_string(405 + minutDwoch);
  for (int i = 0; i < 200 && t < ep(2026, 11, 24, 11, 30); i++) {
    WybD w = wybudzD(s, d, t, pw);
    if (!p.import1 && w.m01 == "405") p.import1 = t;
    if (!p.dwie && minutDwoch && w.m01 == dwie) {
      p.dwie = t;
      p.odswiezPrzyDwoch = w.odswiezen;
    }
    if (pw == PowodWybudzenia::ZEGAR) {
      p.zDrugaSesja += w.powody.find("druga_sesja") != std::string::npos;
      p.zWifi += w.polaczen > 0;
    }
    if (!w.spal) break;
    uint32_t snu = s.p.sen.plan.budzZegarem ? s.p.sen.plan.sekund : OKRES_SNU_S;
    t += s.p.zegar.msTeraz / 1000 + (snu ? snu : 1);
    pw = PowodWybudzenia::ZEGAR;
  }
  return p;
}

// R-DANE-69 (wariant B, decyzja właściciela 24.11.2026): po imporcie ostatniej nocy rano `/api/status` znów podaje
// `rop` = 1 (terapia wznowiona); panel po końcu terapii pobiera noc ponownie od razu (nie o 11:00) i rysuje ekran dnia
// od nowa. Przebieg planem od 05:00: noc z dwiema sesjami w historii najpóźniej ok. 30 min po końcu drugiej sesji.
// Krótka druga sesja, która kończy się przed pierwszym odczytem statusu po imporcie (start slotu dziennego 60 min po
// imporcie, R-KON-41): noc pobrana ponownie przy tym odczycie (sesja w `edf_prefix` zaczęta po imporcie), bez
// drzemki. Zwykły poranek (bez drugiej sesji): ani jednego wybudzenia więcej.
// wymaganie: R-DANE-69
static void testR_DANE_69_drugaSesjaRano() {
  {
    PanelD s;
    DrugaSesja d;
    PrzebiegDS p = przebiegDS(s, d, 90);
    printf("  druga sesja 06:10–07:40: import pierwszej części %s, noc z dwiema sesjami %s, wybudzeń z druga_sesja %d, "
           "z Wi-Fi %d\n", hhmm(p.import1).c_str(), hhmm(p.dwie).c_str(), p.zDrugaSesja, p.zWifi);
    SPRAWDZ(p.import1 > 0 && p.import1 < d.start2);
    SPRAWDZ(p.dwie > 0 && p.dwie >= d.koniec2 && p.dwie - d.koniec2 <= 30 * 60 + 60);
    SPRAWDZ(p.odswiezPrzyDwoch >= 1);
    ROWNE(s.m17(), "1");
    SPRAWDZ(p.zDrugaSesja >= 1 && p.zDrugaSesja <= 7);
    SPRAWDZ(s.p.nvs.tekst("cpap", "b9_drz", "").find("2026-11-24") == std::string::npos);  // bez drzemki R-KON-57
  }
  {
    PanelD s;
    DrugaSesja d(6 * 60 + 10, 15);  // 06:10–06:25
    PrzebiegDS p = przebiegDS(s, d, 15);
    printf("  druga sesja 06:10–06:25: noc z dwiema sesjami %s, wybudzeń z druga_sesja %d, z Wi-Fi %d\n",
           hhmm(p.dwie).c_str(), p.zDrugaSesja, p.zWifi);
    SPRAWDZ(p.dwie > 0 && p.dwie <= ep(2026, 11, 24, 7, 5));
    ROWNE_L(p.zDrugaSesja, 0);
    SPRAWDZ(s.p.nvs.tekst("cpap", "b9_drz", "").find("2026-11-24") == std::string::npos);
  }
  {
    PanelD s;
    DrugaSesja d(0, 0);  // zwykły poranek
    PrzebiegDS p = przebiegDS(s, d, 0);
    printf("  zwykły poranek: import %s, wybudzeń z druga_sesja %d, z Wi-Fi %d\n", hhmm(p.import1).c_str(),
           p.zDrugaSesja, p.zWifi);
    ROWNE_L(p.zDrugaSesja, 0);
    ROWNE_L(p.zWifi, 7);  // spec techniczna 2.2: 7 wybudzeń zegarem z Wi-Fi w 05:00–11:30
    ROWNE(s.m01(), "405");
  }
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  // (1)
  testR_AW_35_teksty();
  testR_AW_35_dyrygent();
  // (2)
  testT_WYM_81_rachunek();
  testT_WYM_81_chunked();
  testT_WYM_81_petlaCiala();
  testT_WYM_81_przerwaStraznika();
  testR_AW_39_restartyWSieci();
  testR_AW_39_pozaSieciaBezZmian();
  // (3)
  testR_DANE_24_nocZPlikow();
  testR_DANE_69_drugaSesjaRano();
  printf("testy_awarie_sieci: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
