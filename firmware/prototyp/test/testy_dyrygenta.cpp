// SPDX-License-Identifier: GPL-3.0-or-later
// Scenariusze dyrygenta z atrapami.
// Uzupełniają testy z test/testy_wybudzenie.cpp o ścieżki przebiegu wybudzenia, które prowadzi dyrygent: poranek z listą i zaległymi dobami, logowanie Digest, brak Wi-Fi,
// czuwanie i nawigacja, ekran testu z oknem serwisowym, komendy `Z` i `O`, pytanie „Co było inaczej?”, źródło
// otoczenia, slot nocny, import historii, tryb awaryjny (U-05, ◀, ↻ 2 s), brak zegara, zacięty przycisk,
// przerwanie ponowienia, błędy listy i raportu. Testy charakteryzujące (utrwalają obecne zachowanie);
// to nie są testy z kolumny „Weryfikacja”.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_dyrygenta, po testach kroku 1).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <set>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "czynniki.h"
#include "bateria.h"
#include "humor.h"
#include "noc.h"
#include "otoczenie.h"
#include "dyrygent.h"
#include "plan.h"
#include "nawigacja.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)

static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_BLOKADA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const char* OTO_PRZYKLAD =
    R"({"wersja":1,"ts":"2026-11-04T06:55:00+01:00","pogoda":{"temp":8.2,"opis":"deszcz","kod":61,"tmin":6,"tmax":10,)"
    R"("opady_proc":80,"miasto":"Katowice","ts":"2026-11-04T06:50:00+01:00"},"powietrze":{"pm25":12,)"
    R"("ts":"2026-11-04T06:50:00+01:00"},"sypialnia":{"co2":967,"temp":null,"wilg":null,)"
    R"("ts":"2026-11-04T06:50:00+01:00","od":"2026-11-04T06:50:00+01:00"},"ostrzezenia":[]})";

// Raport doby jak w test/testy.cpp raportNocy() (Załącznik A), skrócony do kanałów potrzebnych zapisowi nocy.
static std::string raport(long day, bool partial = false) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":" +
         (partial ? "true" : "false") +
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Press\",\"label\":\"C\",\"unit\":\"cmH2O\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":6.2,\"mean\":10.1,\"median\":9.8,\"p95\":13.6,\"max\":16.0,"
         "\"source\":\"edf\"},{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}

static const long DZIS_DOBA = nrDnia("2026-11-03");  // dataPoranka() = 2026-11-04

static void panel(Panel& p, int h, int m, PowodWybudzenia w = PowodWybudzenia::ZEGAR) {
  p.zegar.epoch = ep(2026, 11, 4, h, m);
  p.start.przyczynaStartu = w == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = w;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
}

// (magazyn danych): wiersze danych pliku z nagłówkiem `#cpap-eink`, bez nagłówka, nazw pól i `#koniec`.
static std::vector<std::string> daneP03(const Panel& p, const std::string& plik) {
  std::vector<std::string> v;
  auto i = p.pliki.pliki.find(plik);
  if (i == p.pliki.pliki.end()) return v;
  size_t a = 0, nr = 0;
  const std::string& t = i->second;
  while (a < t.size()) {
    size_t b = t.find('\n', a);
    if (b == std::string::npos) b = t.size();
    std::string l = t.substr(a, b - a);
    if (nr++ >= 2 && !l.empty() && l[0] != '#') v.push_back(l);
    a = b + 1;
  }
  return v;
}
static int wierszyDanych(const Panel& p, const std::string& plik) { return (int)daneP03(p, plik).size(); }
static std::string pierwszyDanych(const Panel& p, const std::string& plik) {
  std::vector<std::string> v = daneP03(p, plik);
  return v.empty() ? std::string() : v[0];
}
static bool wLogu(const Panel& p, const std::string& f) {
  for (auto& l : p.port.log)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static int zadan(const Panel& p, const std::string& sciezka) {
  int n = 0;
  for (auto& u : p.siec.zadania) n += AtrapaSieci::sciezka(u).compare(0, sciezka.size(), sciezka) == 0;
  return n;
}

// Poranek: status, lista, raport ostatniej nocy, potem 4 zaległe doby w tej samej sesji Wi-Fi (krok 2: przed wyłączeniem Wi-Fi i rysowaniem, 7.1 krok 8); zapis nocy, dziennik ekranów, wpis
// baterii, stan AirBridge w NVS.
static void testPoranekZListaIZaleglymi() {
  Panel p;
  panel(p, 7, 0);
  p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  p.siec.dodaj(sciezkaListy(), 200, lista(DZIS_DOBA - 6, DZIS_DOBA));
  for (long d = DZIS_DOBA - 6; d <= DZIS_DOBA; d++) p.siec.dodaj(sciezkaNocy(d), 200, raport(d));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.sen.snow == 1 && p.sen.plan.budzZegarem);
  SPRAWDZ(zadan(p, "/api/status") == 1 && zadan(p, sciezkaListy()) == 1);
  SPRAWDZ(zadan(p, "/api/report?source=edf&day=") == 5);  // ostatnia noc + ZALEGLE_NA_WYBUDZENIE
  SPRAWDZ(wierszyDanych(p, "/dane/noce/2026.csv") == 5);
  SPRAWDZ(pierwszyDanych(p, "/dane/noce/2026.csv").compare(0, 10, "2026-10-31") == 0);
  SPRAWDZ(wierszyDanych(p, "/dane/humor.csv") == 1 && wierszyDanych(p, "/log/wybudzenia.csv") == 1);
  SPRAWDZ(p.pliki.pliki["/log/wybudzenia.csv"].find(";3950;") != std::string::npos);
  SPRAWDZ(p.nvs.bajtyMap.count("noc/ab") && p.nvs.bajtyMap["noc/ab"].size() == sizeof(StanAB));
  SPRAWDZ(p.nvs.teksty["cpap/ab_rano"] == "2026-11-04");  // R-DANE-24
  SPRAWDZ(p.start.stan.ab.ma && p.start.stan.ostatniOdczytOk);
  SPRAWDZ(p.ekran.odswiezen == 1);
  // R-DANE-04: zapis nocy przed odświeżeniem ekranu; krok 2 (7.1 kroki 8–10): zaległe doby w tej samej sesji Wi-Fi,
  // przed wyłączeniem Wi-Fi, zapis po wyłączeniu, rysowanie na końcu
  SPRAWDZ(p.d.pierwsze("pliki.nazwa /dane/noce/2026.csv.tmp") < p.d.pierwsze("ekran.odswiez"));
  int zalegla = p.d.pierwsze("siec.get http://192.168.1.20/api/report?source=edf&day=20759");
  SPRAWDZ(zalegla >= 0 && zalegla < p.d.pierwsze("siec.wylacz"));
  SPRAWDZ(p.d.pierwsze("siec.wylacz") < p.d.pierwsze("pliki.nazwa /dane/noce/2026.csv.tmp"));
  // sen do startu slotu dziennego o 08:00 (import 07:00 + 60 min, R-KON-41; wcześniej: do sprawdzenia
  // o 11:00, R-DANE-24), nie do 07:30
  SPRAWDZ(p.zegar.teraz() + p.sen.plan.sekund == ep(2026, 11, 4, 8, 0) + 5);
  // następne wybudzenie o 11:00: ponowne pobranie nocy zapisanej przed 11:00 (R-DANE-24), lista i raport
  p.siec.zadania.clear();
  p.d.z.clear();
  p.sen.snow = 0;
  p.zegar.epoch = ep(2026, 11, 4, 11, 0) + 5;
  p.zegar.msTeraz = 0;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.sen.snow == 1 && zadan(p, "/api/status") == 1 && zadan(p, sciezkaListy()) == 1);
  SPRAWDZ(p.nvs.teksty["cpap/ab_11"] == "2026-11-04");
  // o 11:00 dochodzą 2 ostatnie zaległe doby (7 nocy): nowy ekran dnia (kamień milowy) z nowym wpisem dziennika;
  // w prototypie to samo działo się o 07:30
  SPRAWDZ(wierszyDanych(p, "/dane/noce/2026.csv") == 7);
  SPRAWDZ(wierszyDanych(p, "/dane/humor.csv") == 2 &&
          p.pliki.pliki["/dane/humor.csv"].find("\n2026-11-04;KM;7;") != std::string::npos);
}

// Raport dziś częściowy, lista z błędem, lista pusta, lista nieczytelna, raport 404.
static void testBledyListyIRaportu() {
  {
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    p.siec.dodaj(sciezkaListy(), 500, "");
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.sen.snow == 1 && !p.start.stan.ostatniOdczytOk && p.start.stan.ab.bladOd != 0);
    SPRAWDZ(wLogu(p, "nieudana próba pobrania (kod 500)"));
  }
  {
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    p.siec.dodaj(sciezkaListy(), 200, "{\"days\":[]}");
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.start.stan.ostatniOdczytOk && p.start.stan.ab.wBudowie);
    SPRAWDZ(wLogu(p, "lista nocy pusta"));
  }
  {
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    p.siec.dodaj(sciezkaListy(), 200, "{\"days\":{}}");
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(!p.start.stan.ostatniOdczytOk);  // E-03 (R-AW-02)
  }
  {
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    p.siec.dodaj(sciezkaListy(), 200, lista(DZIS_DOBA - 1, DZIS_DOBA));
    p.siec.dodaj(sciezkaNocy(DZIS_DOBA), 200, raport(DZIS_DOBA, true));
    p.siec.dodaj(sciezkaNocy(DZIS_DOBA - 1), 404, "");
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.start.stan.ostatniOdczytOk && p.start.stan.ab.wBudowie);  // noc częściowa: CZEKA
    SPRAWDZ(wLogu(p, "odpowiedź bez raportu"));                           // zaległa doba 404: nieudana próba
    SPRAWDZ(p.pliki.pliki.count("/dane/ab-proby.csv") == 1);
  }
  {
    // wersja AirBridge spoza zakresu: E-03 z wersją w stanie (S5 pokazuje wersję), bez listy
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, "{\"version\":\"v2.1.9\",\"rop\":0}");
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(std::string(p.start.stan.ab.wersja) == "v2.1.9" && zadan(p, sciezkaListy()) == 0);
    SPRAWDZ(wLogu(p, "wersja spoza zakresu"));
  }
  {
    // status przetwarza pliki po terapii: bez listy, noc w budowie
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, "{\"version\":\"v2.2.1\",\"rop\":0,\"edf\":\"post-processing\"}");
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.start.stan.ab.wBudowie && zadan(p, sciezkaListy()) == 0);
  }
}

// Digest (T-WYM-25): 401 z wyzwaniem → drugie żądanie z nagłówkiem Authorization; dwa 401 z rzędu = złe hasło.
static void testDigest() {
  const char* WYZWANIE = "Digest realm=\"AirBridge\", qop=\"auth\", nonce=\"abc\", opaque=\"xyz\"";
  {
    Panel p;
    panel(p, 10, 0);
    p.siec.dodaj("/api/status", 401, "");
    p.siec.odpowiedzi["/api/status"].back().o.wwwAuthenticate = WYZWANIE;
    p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.siec.zadania.size() == 2 && p.siec.naglowki[0].empty());
    SPRAWDZ(p.siec.naglowki.size() == 2 && p.siec.naglowki[1].size() == 1 &&
            p.siec.naglowki[1][0].first == "Authorization" &&
            p.siec.naglowki[1][0].second.compare(0, 7, "Digest ") == 0);
    SPRAWDZ(p.start.stan.ostatniOdczytOk);
  }
  {
    Panel p;
    panel(p, 10, 0);
    p.siec.dodaj("/api/status", 401, "");
    p.siec.odpowiedzi["/api/status"].back().o.wwwAuthenticate = WYZWANIE;
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.siec.zadania.size() == 2 && !p.start.stan.ostatniOdczytOk);  // 401 bez ponowień R-AW-01
    SPRAWDZ(wLogu(p, "401, logowanie Digest odrzucone"));
  }
}

// Brak Wi-Fi: S5 z E-01, opis do ekranu testu, początek błędu w stanie AirBridge.
static void testBezWiFi() {
  Panel p;
  panel(p, 9, 0);
  p.siec.siecJest = false;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.siec.zadania.empty() && p.d.ile("siec.polacz") == 1);
  SPRAWDZ(!p.start.stan.ostatniOdczytOk && p.start.stan.ab.bladOd == ep(2026, 11, 4, 9, 0) + 20);
  SPRAWDZ(p.ekran.odswiezen == 1 && p.sen.snow == 1);
  SPRAWDZ(wLogu(p, "Wi-Fi: brak (atrapa: brak sieci)"));
  // ponowienie ↻ na S5 w czuwaniu (R-PRZ-06): druga próba połączenia
  p.d.z.clear();
  p.sen.snow = 0;
  p.zegar.msTeraz = 0;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = BIT_SRODEK;
  p.przyciski.skrypt = {{0, BIT_SRODEK}, {300, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.d.ile("siec.polacz") == 1 && p.sen.snow == 1);
}

// Czuwanie (R-PRZ-01): ▶ budzi → K1, ▶ w czuwaniu (w K1 bez akcji) przerysowuje; po 20 s bez naciśnięcia sen;
// R-PRZ-02: sen do powrotu po 5 min. Krok 2: czuwanie tylko po wybudzeniu przyciskiem (src/plan.h czuwanieMs()).
static void testCzuwanieINawigacja() {
  Panel p;
  panel(p, 10, 0, PowodWybudzenia::PRZYCISK);
  p.start.budzace = BIT_PRAWY;
  // (nowa nawigacja): ▶ budzi → K1, ▶ → K2, a akcją bez skutku jest ◀ 2 s w K2
  // (przytrzymania na stronach kokpitu nic nie otwierają, mapa 5.6.2); zmiana testu:
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {300, 0}, {6000, BIT_PRAWY}, {6300, 0}, {9000, BIT_LEWY}, {11300, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  // ◀ 2 s w K2 bez akcji daje tę samą klatkę: bez kolejnego odświeżenia e-ink
  SPRAWDZ(p.ekran.odswiezen == 2 && p.start.stan.ekran == (uint8_t)Ekran::K2);
  SPRAWDZ(wLogu(p, "bez zmian: klatka jak wyświetlona"));
  SPRAWDZ(p.dzwiek.piskow == 4 && !p.przyciski.swieci && p.siec.zadania.empty());
  // K1 otwarty: sen do powrotu po 5 min + 2 s od naciśnięcia (R-PRZ-02); czuwanie 20 s po naciśnięciu już minęło
  SPRAWDZ(p.sen.plan.sekund == 302 - (p.zegar.teraz() - p.start.stan.ekranOd));
  SPRAWDZ(p.zegar.teraz() - p.start.stan.ekranOd >= 20);
  // następne wybudzenie zegarem po 5 min: powrót do ekranu dnia
  p.d.z.clear();
  p.sen.snow = 0;
  p.przyciski.skrypt.clear();
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  p.zegar.epoch = p.start.stan.ekranOd + 302;
  p.zegar.msTeraz = 0;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::DZIEN && p.sen.snow == 1);
}

// Zacięty przycisk (T-WYM-29, U4): wciśnięty od chwili przed snem po wybudzeniu zegarem do końca → jedno naciśnięcie
// (krok 2: obsłużone jak budzące, R-PRZ-09), po ZACIECIE_MS zacięty, potem ignorowany, nie budzi.
static void testZacietyPrzycisk() {
  Panel p;
  panel(p, 10, 0);
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  p.przyciski.skrypt = {{1000, BIT_LEWY}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.dzwiek.piskow == 2 && p.sen.snow == 1);  // ◀ 2 s na ekranie dnia: nic, dwa piski
  SPRAWDZ(p.sen.plan.przyciski == (BIT_SRODEK | BIT_PRAWY));
  SPRAWDZ(wLogu(p, "zacięty"));
}

// ---------- krok 2 ----------

// R-PRZ-09 (ryzyko z): po wybudzeniu zegarem nie ma czuwania, więc przycisk wciśnięty w chwili
// zasypiania byłby uznany za zacięty i nie budził. Dyrygent obsługuje go jak przycisk budzący: krótkie ▶ → K1,
// czuwanie 20 s, sen z budzeniem wszystkimi przyciskami; Wi-Fi wyłączone przed rysowaniem i czuwaniem.
static void testR_PRZ_09_przyciskPrzedSnemPoZegarze() {
  Panel p;
  panel(p, 10, 0);
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  // pobranie (1,5 s łączenia + NTP + status) kończy się ok. 1,8 s; ▶ wciśnięty od 1,0 s do 2,5 s
  p.przyciski.skrypt = {{1000, BIT_PRAWY}, {2500, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::K1 && p.dzwiek.piskow == 1);
  SPRAWDZ(p.ekran.odswiezen == 2);  // ekran dnia po zegarze, potem K1
  SPRAWDZ(p.sen.snow == 1 && p.sen.plan.przyciski == WSZYSTKIE_PRZYCISKI);  // nie zacięty
  SPRAWDZ(p.zegar.msTeraz >= 2500 + CZUWANIE_MS);
  SPRAWDZ(p.d.pierwsze("siec.wylacz") < p.d.pierwsze("ekran.odswiez"));
  SPRAWDZ(wLogu(p, "przycisk wciśnięty przed snem"));
  // K1 otwarty: sen do powrotu po 5 min + 2 s (R-PRZ-02)
  SPRAWDZ(p.sen.plan.sekund == 302 - (p.zegar.teraz() - p.start.stan.ekranOd));
  // przycisk puszczony przed końcem pracy po zegarze nie jest widziany (bez czuwania): ekran dnia, bez czuwania
  Panel q;
  panel(q, 10, 0);
  q.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  q.przyciski.skrypt = {{1000, BIT_PRAWY}, {1300, 0}};
  Dyrygent(q.adaptery(), KONF).wybudzenie();
  SPRAWDZ(q.start.stan.ekran == (uint8_t)Ekran::DZIEN && q.dzwiek.piskow == 0 && q.zegar.msTeraz < 3000);
}

// T-WYM-31 z dyrygentem: wybudzenie pomiarowe bez włączania ekranu i diody (w nocy bez światła w sypialni),
// z wpisem baterii; zapis czujników przy braku odczytu (NAN) bez wpisu w buforze (R-AW-13).
static void testWybudzeniePomiarowe() {
  Panel p;
  panel(p, 3, 0);
  p.zegar.epoch += 5;
  p.przyciski.swieci = false;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.d.ile("ekran.wlacz") == 0 && p.ekran.odswiezen == 0 && p.d.ile("siec.polacz") == 0);
  SPRAWDZ(!p.przyciski.swieci && p.sen.snow == 1 && p.start.stan.buforSyp.n == 1);
  SPRAWDZ(p.pliki.pliki["/log/wybudzenia.csv"].find(";0;-1;zegar:czujniki") != std::string::npos);
  SPRAWDZ(p.zegar.teraz() + p.sen.plan.sekund == ep(2026, 11, 4, 3, 15) + 5);
  p.czujniki.o.temp = NAN;
  p.zegar.epoch = ep(2026, 11, 4, 3, 15) + 5;
  p.zegar.msTeraz = 0;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.buforSyp.n == 1 && p.sen.snow == 2);
  // wybudzenie zegarem bez powodu (np. 07:32 przy nocy kompletnej): jak pomiarowe, wiersz `zegar:` bez nazwy
  Panel q;
  panel(q, 7, 32);
  q.pliki.pliki["/noce.csv"] = "2026-11-04;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;\n";
  Dyrygent(q.adaptery(), KONF).wybudzenie();
  SPRAWDZ(q.d.ile("siec.polacz") == 0 && q.ekran.odswiezen == 0);
  SPRAWDZ(q.pliki.pliki["/log/wybudzenia.csv"].find(";zegar:\n") != std::string::npos);
  SPRAWDZ(q.zegar.teraz() + q.sen.plan.sekund == ep(2026, 11, 4, 11, 0) + 5);
}

// R-PIE-13, R-CZY-03, R-NF-02 (przykład z weryfikacji): import o 13:00, termin 15:00 → wybudzenie o 15:00 bez Wi-Fi,
// linia zaproszenia znika z ekranu, „brak odpowiedzi” zapisany w tym wybudzeniu.
static void testR_PIE_13_wybudzenieWTerminie() {
  Panel p;
  panel(p, 15, 0);
  p.zegar.epoch += 5;
  // wyciek P95 9 (pomijalny): P95 12 przy domyślnym „Uwaga o masce od” 10 daje `Wyciek widoczny` (R-KONF-42),
  // a ten test dotyczy linii zaproszenia, nie ekranu wycieku
  p.pliki.pliki["/noce.csv"] = "2026-11-04;1;414;1.20;9.40;11.30;4.00;9.00;;;;;;A;0;0;;;;\n";
  p.pliki.pliki["/czynniki.csv"] = "2026-11-04;C;;O;900;1\n";
  p.nvs.teksty["cpap/ab_11"] = "2026-11-04";
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.d.ile("siec.polacz") == 0 && p.ekran.odswiezen == 1);
  SPRAWDZ(pierwszyDanych(p, "/dane/czynniki.csv").compare(0, 13, "2026-11-04;B;") == 0);
  SPRAWDZ(p.pliki.pliki["/log/wybudzenia.csv"].find(";zegar:pytanie") != std::string::npos);
  // R-EKR-60: ekran dnia narysowany o 15:00 → przerysowanie bez Wi-Fi o 17:00
  SPRAWDZ(p.zegar.teraz() + p.sen.plan.sekund == ep(2026, 11, 4, 17, 0) + 5);
}

// Slot dzienny na dyrygencie. Import o 07:00 zapisuje chwilę importu w RTC i daje sen
// do startu slotu o 08:00; ▶ o 07:10 na ekranie dnia zaczyna slot od tej chwili (R-KON-41), bez Wi-Fi; ocena o 08:00
// bez terminu startu, z jednym `GET /api/status`, bez przesunięcia godziny `dane` (R-EKR-04); po zaniku zasilania
// (RTC bez chwili importu) plan bez slotu. Funkcje planu do dyrygenta: tylkoStatus(), przerysowanieDnia(),
// startSlotuPrzyciskiem().
// wymaganie: R-KON-41, R-KON-42
static void testR_KON_41_slotDziennyDyrygent() {
  SPRAWDZ(tylkoStatus(POWOD_SLOT_DZIENNY_GODZINA) && tylkoStatus(POWOD_SLOT_DZIENNY_START | POWOD_PROGNOZA_GODZINOWA));
  SPRAWDZ(!tylkoStatus(POWOD_SLOT_DZIENNY_GODZINA | POWOD_SPRAWDZENIE_11) && !tylkoStatus(POWOD_SLOT_DZIENNY_BEZ_SIECI));
  SPRAWDZ(!tylkoStatus(0) && !tylkoStatus(POWOD_ODSWIEZENIE));
  SPRAWDZ(przerysowanieDnia(POWOD_ODSWIEZENIE) && przerysowanieDnia(POWOD_SLOT_DZIENNY_GODZINA | POWOD_CZUJNIKI));
  SPRAWDZ(przerysowanieDnia(POWOD_SLOT_DZIENNY_BEZ_SIECI) && przerysowanieDnia(POWOD_SLOT_DZIENNY_START | POWOD_PROGNOZA_GODZINOWA));
  SPRAWDZ(!przerysowanieDnia(0) && !przerysowanieDnia(POWOD_CZUJNIKI) && !przerysowanieDnia(POWOD_PROGNOZA_GODZINOWA));
  SPRAWDZ(!przerysowanieDnia(POWOD_SLOT_DZIENNY_GODZINA | POWOD_SPRAWDZENIE_11));
  WejsciePlanu w;
  w.zegar = true;
  w.sekundaDoby = 7 * 3600 + 10 * 60;
  w.importNocy = 7 * 3600;
  SPRAWDZ(startSlotuPrzyciskiem(w, Ekran::DZIEN) && !startSlotuPrzyciskiem(w, Ekran::K1));
  w.slotDziennyOd = 7 * 3600 + 5 * 60;
  SPRAWDZ(!startSlotuPrzyciskiem(w, Ekran::DZIEN));  // slot już trwa
  w.slotDziennyOd = -1;
  w.importNocy = -1;
  SPRAWDZ(!startSlotuPrzyciskiem(w, Ekran::DZIEN));  // przed importem
  w.importNocy = 7 * 3600;
  w.sekundaDoby = 21 * 3600;
  SPRAWDZ(!startSlotuPrzyciskiem(w, Ekran::DZIEN));  // po końcu slotu dziennego
  w.zegar = false;
  w.sekundaDoby = 7 * 3600 + 10 * 60;
  SPRAWDZ(!startSlotuPrzyciskiem(w, Ekran::DZIEN));

  Panel p;
  panel(p, 7, 0);
  p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  p.siec.dodaj(sciezkaListy(), 200, lista(DZIS_DOBA, DZIS_DOBA));
  p.siec.dodaj(sciezkaNocy(DZIS_DOBA), 200, raport(DZIS_DOBA));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.importNocyEp >= ep(2026, 11, 4, 7, 0) && p.start.stan.importNocyEp < ep(2026, 11, 4, 7, 1));
  SPRAWDZ(p.start.stan.slotDziennyEp == 0);
  SPRAWDZ(p.zegar.teraz() + p.sen.plan.sekund == ep(2026, 11, 4, 8, 0) + 5);
  uint32_t pobrano = p.start.stan.ab.pobrano;
  // ▶ o 07:10 na ekranie dnia: slot dzienny od 07:10, bez Wi-Fi, K1, powrót o 07:15
  p.d.z.clear();
  p.siec.zadania.clear();
  p.sen.snow = 0;
  p.zegar.epoch = ep(2026, 11, 4, 7, 10);
  p.zegar.msTeraz = 0;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = BIT_PRAWY;
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {300, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.slotDziennyEp == ep(2026, 11, 4, 7, 10) && p.start.stan.nacisniecieRanoEp == ep(2026, 11, 4, 7, 10));
  SPRAWDZ(wLogu(p, "slot dzienny: start naciśnięciem") && p.d.ile("siec.polacz") == 0);
  // ocena o 08:00:05 (po powrocie o 07:15): jedno /api/status, ekran dnia, bez startu, `dane` zostaje z 07:00
  p.start.budzace = 0;
  p.przyciski.skrypt.clear();
  p.start.stan.ekran = (uint8_t)Ekran::DZIEN;
  p.d.z.clear();
  p.siec.zadania.clear();
  p.sen.snow = 0;
  p.zegar.epoch = ep(2026, 11, 4, 8, 0) + 5;
  p.zegar.msTeraz = 0;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(wLogu(p, "wybudzenie zegar:slot_dzienny_godzina,") && !wLogu(p, "zegar:slot_dzienny+"));
  SPRAWDZ(p.siec.zadania.size() == 1 && zadan(p, "/api/status") == 1);
  SPRAWDZ(wLogu(p, "slot dzienny: tylko odczyt stanu aparatu"));
  SPRAWDZ(zadan(p, "/api/sessions") == 0 && zadan(p, "/api/storage") == 0);  // R-KON-42 bez STR.edf (ADR-0035 pkt 1)
  SPRAWDZ(p.start.stan.ab.pobrano == pobrano && p.start.stan.slotDziennyEp == ep(2026, 11, 4, 7, 10));
  SPRAWDZ(p.zegar.teraz() + p.sen.plan.sekund == ep(2026, 11, 4, 9, 0) + 5);
  // zanik zasilania (RTC od nowa, NVS i pliki zostają): chwila importu nieznana; (było:
  // plan kroku 2 bez slotu, sen do 11:00:05) i (było: odczyt z tego wybudzenia 09:30 → start 10:30)
  // zastępnik z NVS sprzed pobrania: odczyt 07:00 → start 08:00 już minął → slot od startu zasilania 09:30, ocena 10:00
  Panel q;
  panel(q, 9, 30, PowodWybudzenia::START);
  // noce z /dane/noce/2026.csv z pustym polem `import` (jak noce sprzed): ten test sprawdza zastępnik z NVS
  // (R1 karty zastępnik tylko dla pustego pola; chwilę z rekordu sprawdza testy_magazyn_danych.cpp)
  {
    std::string t;
    // wiersze nocy w wersji 2 (pola wersji 2 po `import`), więc do wersji 0 pierwsze 20 pól
    for (auto& w : daneP03(p, "/dane/noce/2026.csv")) t += pierwszePola(w, 20) + "\n";
    q.pliki.pliki["/noce.csv"] = t;  // wersja 0: migracja przy starcie
  }
  q.nvs.bajtyMap = p.nvs.bajtyMap;  // StanAB z odczytem 07:00 (zapis po każdym odczycie)
  q.nvs.teksty["cpap/ab_11"] = "";
  q.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  q.siec.dodaj(sciezkaListy(), 200, lista(DZIS_DOBA, DZIS_DOBA));
  Dyrygent(q.adaptery(), KONF).wybudzenie();
  SPRAWDZ(q.start.stan.importNocyEp == pobrano);
  SPRAWDZ(q.start.stan.slotDziennyEp >= ep(2026, 11, 4, 9, 30) && q.start.stan.slotDziennyEp < ep(2026, 11, 4, 9, 31));
  SPRAWDZ(q.zegar.teraz() + q.sen.plan.sekund == ep(2026, 11, 4, 10, 0) + 5);
  SPRAWDZ(wLogu(q, "następny termin: slot_dzienny_godzina") && wLogu(q, "zastępnik z odczytu AirBridge 07:00"));
}

// Wejście planu z historii i czynników (src/plan.h nocKompletnaDzis(), terminPytaniaDzis(), wybudzeniePomiarowe()).
// wymaganie: R-DANE-19, R-PIE-13, T-WYM-31
static void testWejsciePlanu() {
  std::vector<Noc> h;
  const std::set<long> bezPominietych;
  SPRAWDZ(!nocKompletnaDzis(h, "2026-11-04", bezPominietych));
  // doba ostatniej nocy pominięta na stałe (/ab_pominiete.txt): noc „jest”, jak w potrzebnaLista
  SPRAWDZ(nocKompletnaDzis(h, "2026-11-04", {nrDnia("2026-11-04") - 1}));
  SPRAWDZ(!nocKompletnaDzis(h, "2026-11-04", {nrDnia("2026-11-04") - 2}));
  SPRAWDZ(!nocKompletnaDzis(h, "", {nrDnia("2026-11-04") - 1}));
  Noc n;
  SPRAWDZ(parsujNoc("2026-11-04;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;", 'A', n));
  h.push_back(n);
  SPRAWDZ(nocKompletnaDzis(h, "2026-11-04", bezPominietych) && !nocKompletnaDzis(h, "2026-11-05", bezPominietych) &&
          !nocKompletnaDzis(h, "", bezPominietych));
  h[0].czesciowa = true;
  SPRAWDZ(!nocKompletnaDzis(h, "2026-11-04", bezPominietych));
  h[0].czesciowa = false;
  h[0].zrodlo = 'K';  // noc z importu: AirBridge pyta dalej (jak potrzebnaLista())
  SPRAWDZ(!nocKompletnaDzis(h, "2026-11-04", bezPominietych));
  WpisCzynnikow c;
  c.data = "2026-11-04";
  c.stan = 'C';
  c.termin = 15 * 60;
  SPRAWDZ(terminPytaniaDzis(&c) == 900 && terminPytaniaDzis(nullptr) == -1);
  c.stan = 'O';
  SPRAWDZ(terminPytaniaDzis(&c) == -1);
  c.stan = 'C';
  c.termin = 2000;  // spoza doby (plik z zewnątrz)
  SPRAWDZ(terminPytaniaDzis(&c) == -1);
  SPRAWDZ(wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, POWOD_CZUJNIKI) && wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, 0));
  SPRAWDZ(!wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, POWOD_CZUJNIKI | POWOD_OKNO_PORANNE));
  SPRAWDZ(!wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, POWOD_POWROT) && !wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, POWOD_PYTANIE));
  SPRAWDZ(!wybudzeniePomiarowe(PowodWybudzenia::PRZYCISK, 0) && !wybudzeniePomiarowe(PowodWybudzenia::START, 0));
  Pobranie pp = pobranieZegarem(POWOD_PYTANIE);
  SPRAWDZ(!pp.pobierz && pp.poranekCzynnikow);  // R-CZY-03 bez Wi-Fi
  // terminy po północy i powrót równy terminowi siatki (powody łączone)
  WejsciePlanu w;
  w.zegar = true;
  w.sekundaDoby = 10 * 3600 + 25 * 60;
  w.ekran = Ekran::K1;
  w.sekundOdNacisniecia = 0;  // powrót o 10:30:00, razem z oknem porannym
  Termin t = nastepnyTermin(w);
  SPRAWDZ(t.sekund == 302 && t.powody == (POWOD_POWROT | POWOD_OKNO_PORANNE));
  w.ekran = Ekran::DZIEN;
  w.sekundaDoby = 23 * 3600 + 59 * 60 + 50;
  t = nastepnyTermin(w);
  SPRAWDZ(t.sekund == 15 && t.powody == POWOD_CZUJNIKI);  // 00:00 następnej doby
  w.sekundaDoby = 30;  // 00:00:30: termin 00:00 w tolerancji
  SPRAWDZ(powodyTeraz(w) == POWOD_CZUJNIKI);
  SPRAWDZ(nocyBezDanych(true, "2026-10-22", "2026-10-25", "") == 0);  // odczyt „z przyszłości” (zmiana zegara)
  SPRAWDZ(nocyBezDanych(true, "2026-10-22", "zła", "") == 1 && nocyBezDanych(true, "2026-10-22", "", "zła") == 1);
  SPRAWDZ(powodWpisuBaterii(PowodWybudzenia::ZEGAR, 0x1FF).size() < 100);
  SPRAWDZ(zapiszBaterie(WpisBaterii{1793790000, 3950, 12345, 67890,
                                    powodWpisuBaterii(PowodWybudzenia::ZEGAR, 0x1FF)}).find("+pytanie+czas") != std::string::npos);
}

// ---------- sen i następne wybudzenie ----------

// Następne wybudzenie zegarem po śnie z planu: RTC, NVS i pliki zostają jak po głębokim śnie, zegar procesora od zera.
static void poSnie(Panel& p) {
  p.zegar.epoch = p.zegar.teraz() + p.sen.plan.sekund;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  p.start.budzace = 0;
  p.siec.polaczonaTeraz = false;
  p.siec.okno = false;
  p.siec.zadania.clear();
  p.d.z.clear();
  p.port.log.clear();
  p.ekran.odswiezen = 0;
}
// Ostatni wiersz /log/wybudzenia.csv (było /bateria.csv) (pole `powod` i `wifi_ms` tego wybudzenia).
static WpisBaterii ostatniWpisBaterii(Panel& p) {
  const std::string& t = p.pliki.pliki["/log/wybudzenia.csv"];
  WpisBaterii w;
  size_t k = t.size() && t.back() == '\n' ? t.size() - 1 : t.size();
  size_t a = t.rfind('\n', k ? k - 1 : 0);
  a = a == std::string::npos ? 0 : a + 1;
  if (k > a) parsujBaterie(t.substr(a, k - a), w);
  return w;
}
static const char* NOC_DZIS = "2026-11-04;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;\n";

// ▶ o 10:59:50 przy nocy pobranej rano; czuwanie 20 s przechodzi przez 11:00. Sprawdzenie o 11:00
// (R-DANE-24) nie może przepaść: sen 1 s, potem wybudzenie zegarem z powodem `sprawdzenie_11` i Wi-Fi, raz.
static void testR_DANE_24_przyciskPrzedTerminem() {
  Panel p;
  panel(p, 10, 59, PowodWybudzenia::PRZYCISK);
  p.zegar.epoch += 50;
  p.pliki.pliki["/noce.csv"] = NOC_DZIS;
  // tryb porównawczy (domyślny od 09.11.2026) łączyłby się drugi raz na końcu wybudzenia (R-DANE-60): tu tylko
  // termin 11:00 (R-DANE-24), więc `json`
  p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  p.nvs.teksty["cpap/ab_rano"] = "2026-11-04";
  p.start.budzace = BIT_PRAWY;
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {300, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::K1 && p.sen.snow == 1 && p.d.ile("siec.polacz") == 0);
  SPRAWDZ(p.zegar.teraz() > ep(2026, 11, 4, 11, 0) && p.sen.plan.sekund == 1);
  poSnie(p);
  p.przyciski.skrypt.clear();
  p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  p.siec.dodaj(sciezkaListy(), 200, lista(DZIS_DOBA, DZIS_DOBA));
  p.siec.dodaj(sciezkaNocy(DZIS_DOBA), 200, raport(DZIS_DOBA));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.d.ile("siec.polacz") == 1 && zadan(p, "/api/status") == 1);
  SPRAWDZ(ostatniWpisBaterii(p).powod == "zegar:sprawdzenie_11");
  // każdy termin najwyżej raz: kolejny sen do powrotu z K1 (R-PRZ-02), nie znowu 1 s
  SPRAWDZ(p.sen.snow == 2 && p.sen.plan.sekund > 60);
}

// naciśnięcie o 23:29:50, czuwanie 20 s przechodzi przez 23:30 → początek slotu nocnego (R-KON-31,
// R-KON-33: zapytanie AirBridge i pierwsza decyzja nocy) nie przepada: sen 1 s, potem wybudzenie zegarem z Wi-Fi.
// ▶ otwiera K1, który zostaje do 05:00 bez S9 (R-KON-35, przykład „23:28 ▶ → K1”); ◀ 2 s na ekranie dnia nic nie
// otwiera, więc o 23:30 powstaje S9.
static void testR_KON_31_przyciskPrzedPoczatkiemNocy() {
  for (int wariant = 0; wariant < 2; wariant++) {
    bool k1 = wariant == 0;
    Panel p;
    panel(p, 23, 29, PowodWybudzenia::PRZYCISK);
    p.zegar.epoch += 50;
    p.pliki.pliki["/noce.csv"] = NOC_DZIS;
    p.nvs.teksty["cpap/ab_11"] = "2026-11-04";
    p.start.budzace = k1 ? BIT_PRAWY : BIT_LEWY;
    p.przyciski.skrypt = {{0, p.start.budzace}, {k1 ? 300u : 2600u, 0}};
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.sen.snow == 1 && p.sen.plan.sekund == 1 && p.d.ile("siec.polacz") == 0);
    SPRAWDZ(p.start.stan.ekran == (uint8_t)(k1 ? Ekran::K1 : Ekran::DZIEN));
    poSnie(p);
    p.przyciski.skrypt.clear();
    p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.d.ile("siec.polacz") == 1 && zadan(p, "/api/status") == 1);
    SPRAWDZ(ostatniWpisBaterii(p).powod.find("poczatek_nocy") != std::string::npos);
    SPRAWDZ(p.nvs.teksty.count("cpap/noc_w") == 1);  // pierwsza decyzja nocy zapisana
    SPRAWDZ(p.ekran.odswiezen == (k1 ? 0 : 1));
    SPRAWDZ(wLogu(p, k1 ? "bez ekranu nocnego (R-KON-35)" : "ekran nocny"));
    SPRAWDZ(p.sen.snow == 2 && p.sen.plan.sekund > 60);  // potem czujniki 23:45, nie znowu 1 s
  }
}

// Terminy zaległe i obsłużona chwila w planie (src/plan.h powodyZalegle(), powodyTeraz()): termin
// z powodem do wykonania po obsłużonej chwili → sen 1 s; każdy termin najwyżej raz; bez znanej chwili tolerancja 60 s;
// spóźnienie ponad 60 s nie gubi powodu.
// wymaganie: R-NF-02, R-DANE-24, R-KON-31, R-PIE-13
static void testR_NF_02_terminyZalegle() {
  WejsciePlanu w;
  w.zegar = true;
  w.nocKompletna = true;
  w.sekundaDoby = 11 * 3600 + 12;  // 11:00:12, czuwanie po ▶ od 10:59:50
  SPRAWDZ(powodyZalegle(w) == 0);  // obsłużona chwila nieznana (pierwszy start, brak zegara): bez zaległych
  w.obsluzoneZnane = true;
  w.obsluzoneDo = 10 * 3600 + 59 * 60 + 50;
  SPRAWDZ(powodyZalegle(w) == POWOD_SPRAWDZENIE_11);
  Termin t = nastepnyTermin(w);
  SPRAWDZ(t.sekund == ZALEGLY_TERMIN_S && t.powody == POWOD_SPRAWDZENIE_11);
  // wybudzenie po 1 s: powód z przedziału od obsłużonej chwili, potem obsłużona = początek tego wybudzenia
  w.sekundaDoby = 11 * 3600 + 13;
  SPRAWDZ(powodyTeraz(w) == POWOD_SPRAWDZENIE_11);
  w.obsluzoneDo = 11 * 3600 + 13;
  w.sekundaDoby = 11 * 3600 + 20;
  SPRAWDZ(powodyZalegle(w) == 0 && nastepnyTermin(w).sekund > 3600);
  // cofnięty zegar (obsłużona po teraz): bez zaległych, powody z tolerancji 60 s
  w.obsluzoneDo = 11 * 3600 + 30;
  SPRAWDZ(powodyZalegle(w) == 0 && powodyTeraz(w) == POWOD_SPRAWDZENIE_11);
  // same czujniki w trakcie wybudzenia nie wymuszają snu 1 s
  w.obsluzoneDo = 23 * 3600 + 44 * 60 + 50;
  w.sekundaDoby = 23 * 3600 + 45 * 60 + 10;
  SPRAWDZ(powodyZalegle(w) == 0 && nastepnyTermin(w).powody == POWOD_CZUJNIKI);
  // początek nocy i termin pytania wypadłe w trakcie wybudzenia
  w.obsluzoneDo = 23 * 3600 + 29 * 60 + 50;
  w.sekundaDoby = 23 * 3600 + 30 * 60 + 12;
  SPRAWDZ(powodyZalegle(w) == POWOD_POCZATEK_NOCY);
  w.terminPytania = 15 * 60;
  w.obsluzoneDo = 14 * 3600 + 59 * 60 + 50;
  w.sekundaDoby = 15 * 3600 + 10;
  SPRAWDZ(powodyZalegle(w) == POWOD_PYTANIE);
  w.terminPytania = -1;
  // spóźnienie ponad 60 s (R5): 11:01:10 po obsłużonym 07:00:05 → sprawdzenie 11:00 dalej powodem
  w.obsluzoneDo = 7 * 3600 + 5;
  w.sekundaDoby = 11 * 3600 + 70;
  SPRAWDZ(powodyTeraz(w) == POWOD_SPRAWDZENIE_11);
  w.obsluzoneZnane = false;
  SPRAWDZ(powodyTeraz(w) == 0);  // bez obsłużonej chwili jak dotąd: tolerancja 60 s
  // obsłużona chwila z poprzedniej doby: przedział od dzisiejszej północy
  w.obsluzoneZnane = true;
  w.obsluzoneDo = -1;
  w.sekundaDoby = 5;
  SPRAWDZ(powodyTeraz(w) == POWOD_CZUJNIKI && powodyZalegle(w) == 0);
  w.zegar = false;
  SPRAWDZ(powodyZalegle(w) == 0 && nastepnyTermin(w).powody == POWOD_CZAS);
}

// Start zasilania tuż przed 05:00: termin okna porannego wypada w trakcie startu → sen 1 s, jedno
// wybudzenie z oknem porannym, potem zwykły plan (bez pętli wybudzeń).
// wymaganie: R-DANE-19
static void testR_DANE_19_startTuzPrzedTerminem() {
  Panel p;
  panel(p, 4, 59, PowodWybudzenia::START);
  p.zegar.epoch += 59;
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.zegar.teraz() >= ep(2026, 11, 4, 5, 0) && p.sen.plan.sekund == 1);
  SPRAWDZ(p.start.stan.obsluzoneDo == ep(2026, 11, 4, 4, 59) + 59);
  poSnie(p);
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.d.ile("siec.polacz") == 1 && ostatniWpisBaterii(p).powod == "zegar:okno_poranne+czujniki");
  SPRAWDZ(p.sen.plan.sekund > 60);
  // ten sam termin nie wraca: kolejne wybudzenie to 05:15 (czujniki, bez Wi-Fi)
  poSnie(p);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.d.ile("siec.polacz") == 0 && ostatniWpisBaterii(p).powod == "zegar:czujniki");
}

// T-WYM-29: przycisk zablokowany taśmą w nocy. Pierwsze wybudzenie wykrywa zacięcie (najwyżej jedno
// naciśnięcie, bez 20 s czuwania), kolejne wybudzenia pomiarowe co 15 min bez pisku i bez diody, przycisk wyłączony
// z budzenia; po zdjęciu taśmy przycisk znowu budzi.
static void testT_WYM_29_zacietyWNocy() {
  Panel p;
  panel(p, 23, 45);
  p.zegar.epoch += 5;
  p.pliki.pliki["/noce.csv"] = NOC_DZIS;
  p.nvs.teksty["cpap/ab_11"] = "2026-11-04";
  p.przyciski.skrypt = {{0, BIT_LEWY}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.dzwiek.piskow <= 2 && p.zegar.msTeraz < 15000);
  SPRAWDZ(p.sen.plan.przyciski == (BIT_SRODEK | BIT_PRAWY));
  for (int i = 0; i < 3; i++) {
    poSnie(p);
    int piski = p.dzwiek.piskow;
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(p.dzwiek.piskow == piski && p.zegar.msTeraz < 3000 && !p.przyciski.swieci);
    SPRAWDZ(p.sen.plan.przyciski == (BIT_SRODEK | BIT_PRAWY) && p.ekran.odswiezen == 0);
  }
  poSnie(p);
  p.przyciski.skrypt.clear();  // taśma zdjęta
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.sen.plan.przyciski == WSZYSTKIE_PRZYCISKI);
}

// T-NF-06: ekran testu bez hasła okna serwisowego (okno się nie otwiera): Wi-Fi wyłączone przed
// rysowaniem, bez radia w czuwaniu.
static void testT_NF_06_ekranTestuBezOkna() {
  Panel p;
  panel(p, 10, 0, PowodWybudzenia::PRZYCISK);
  p.siec.hasloOkna = false;
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  p.start.budzace = BIT_PRAWY;
  p.start.stan.ekran = (uint8_t)Ekran::FORMULARZ;  // U-05 z U-03 (ciąg U-01 ▶ U-03 ▶ U-05)
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {2300, 0}};  // ▶ 2 s z U-03
  p.pliki.pliki["/noce.csv"] = "2026-11-03;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;\n";
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::TEST && p.d.ile("siec.okno") == 1 && !p.siec.okno);
  int wyl = p.d.pierwsze("siec.wylacz");
  SPRAWDZ(wyl >= 0 && wyl < p.d.pierwsze("ekran.odswiez"));
  WpisBaterii b = ostatniWpisBaterii(p);
  SPRAWDZ(b.wifiMs >= 0 && b.wifiMs < 10000);
}

// NTP w wybudzeniu z Wi-Fi (ADR-0013): wynik i chwila ostatniej udanej synchronizacji w logu i w RTC;
// bez odpowiedzi chwila ostatniej synchronizacji bez zmiany.
static void testNtpWLoguIRtc() {
  Panel p;
  panel(p, 11, 0);
  p.zegar.epoch += 5;
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  uint32_t ntp = p.start.stan.ntpOstatnio;
  SPRAWDZ(ntp >= ep(2026, 11, 4, 11, 0) && ntp < ep(2026, 11, 4, 11, 1));
  SPRAWDZ(wLogu(p, "NTP: zsynchronizowany, ostatnia synchronizacja 2026-11-04 11:00"));
  poSnie(p);
  p.zegar.epoch = ep(2026, 11, 4, 23, 30) + 5;
  p.siec.ntpJest = false;
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.ntpOstatnio == ntp);
  SPRAWDZ(wLogu(p, "NTP: brak odpowiedzi, zegar bez korekty, ostatnia synchronizacja 2026-11-04 11:00"));
}

// Ekran testu (◀ 2 s w K2): pobranie, okno serwisowe 10 min, komenda `Z` (U2) w czuwaniu; po oknie sen.
static void testEkranTestuIOknoSerwisowe() {
  Panel p;
  panel(p, 10, 0, PowodWybudzenia::PRZYCISK);
  p.start.budzace = BIT_PRAWY;
  p.start.stan.ekran = (uint8_t)Ekran::FORMULARZ;  // U-05 z U-03 (ciąg U-01 ▶ U-03 ▶ U-05)
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {2300, 0}};  // ▶ 2 s z U-03
  p.pliki.pliki["/noce.csv"] = "2026-11-03;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;\n";
  p.port.znaki = {'x', 'Z'};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::TEST);
  SPRAWDZ(p.d.ile("siec.polacz") == 1 && p.d.ile("siec.okno") == 1 && zadan(p, "/api/status") == 1);
  SPRAWDZ(p.zegar.msTeraz >= OKNO_SERWISOWE_MS && p.sen.snow == 1);
  SPRAWDZ(wLogu(p, "PLIKI_START") && wLogu(p, "PLIK /noce.csv") && wLogu(p, "2026-11-03;1;414"));
  SPRAWDZ(p.ekran.odswiezen == 1);
  // ▶ na ekranie testu w czuwaniu ponawia pobranie (bez drugiego okna; ↻ to ekran dnia)
  p.d.z.clear();
  p.sen.snow = 0;
  p.zegar.msTeraz = 0;
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {2300, 0}, {5000, BIT_PRAWY}, {5200, 0}};
  p.start.stan.ekran = (uint8_t)Ekran::FORMULARZ;  // U-05 z U-03 (ciąg U-01 ▶ U-03 ▶ U-05)
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.d.ile("siec.polacz") == 2 && p.d.ile("siec.okno") == 1);
}

// Komenda `O` z UART (T-INT-10): ekran testu i okno serwisowe bez przycisków, czuwanie 10 min.
static void testKomendaO() {
  Panel p;
  panel(p, 10, 0);
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  p.port.znaki = {'O'};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::TEST && p.d.ile("siec.okno") == 1);
  SPRAWDZ(p.ekran.odswiezen == 2 && p.zegar.msTeraz >= OKNO_SERWISOWE_MS);
}

// Pytanie „Co było inaczej?” z K1 (↻ 2 s), zaznaczenie, zapis (R-CZY-10): /czynniki.csv.
static void testPytanie() {
  Panel p;
  panel(p, 12, 0, PowodWybudzenia::PRZYCISK);
  p.start.budzace = BIT_SRODEK;
  p.start.stan.ekran = (uint8_t)Ekran::K1;
  p.pliki.pliki["/noce.csv"] = "2026-11-04;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;\n";
  // ↻ 2 s budzi: pytanie; potem ▶ (dalej), ↻ (zaznacz), ◀ (wstecz), ↻ 2 s (zapisz)
  p.przyciski.skrypt = {{0, BIT_SRODEK},     {2500, 0},     {4000, BIT_PRAWY}, {4100, 0},
                        {6000, BIT_SRODEK},  {6100, 0},     {8000, BIT_LEWY},  {8100, 0},
                        {10000, BIT_SRODEK}, {12500, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(std::string(p.start.stan.pyt.data) == "2026-11-04");
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::DZIEN);
  SPRAWDZ(wierszyDanych(p, "/dane/czynniki.csv") == 1);
  SPRAWDZ(p.siec.zadania.empty());  // T-WYM-24: bez Wi-Fi
  // pominięcie (◀ 2 s) przy drugim otwarciu
  p.d.z.clear();
  p.zegar.msTeraz = 0;
  p.start.stan.ekran = (uint8_t)Ekran::K1;
  p.przyciski.skrypt = {{0, BIT_SRODEK}, {2500, 0}, {4000, BIT_LEWY}, {6500, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::DZIEN && p.d.ile("pliki.nazwa /dane/czynniki.csv.tmp") == 1);
}

// Lokalne źródło otoczenia (ADR-0019): odpowiedź 200, błąd 500, ciało ponad limit, przerwanie nie dotyczy.
static void testOtoczenie() {
  KonfiguracjaPanelu k = KONF;
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  k.otoToken = "t";
  {
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
    p.siec.dodaj("/otoczenie", 200, OTO_PRZYKLAD);
    Dyrygent(p.adaptery(), k).wybudzenie();
    SPRAWDZ(p.start.stan.oto.stan == (int8_t)StanOto::OK && p.start.stan.oto.co2 == 967);
    SPRAWDZ(p.siec.naglowki.size() == 2 && p.siec.naglowki[1].size() == 1 && p.siec.naglowki[1][0].first == "X-Token");
    SPRAWDZ(wLogu(p, "otoczenie: pogoda"));
  }
  {
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
    p.siec.dodaj("/otoczenie", 500, "blad");
    Dyrygent(p.adaptery(), k).wybudzenie();
    SPRAWDZ(p.start.stan.oto.stan == (int8_t)StanOto::BRAK_ODPOWIEDZI && zadan(p, "/otoczenie") >= 2);
  }
  {
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
    p.siec.dodaj("/otoczenie", 200, std::string(OTO_MAX_BAJTOW + 10, ' '));
    Dyrygent(p.adaptery(), k).wybudzenie();
    SPRAWDZ(p.start.stan.oto.stan != (int8_t)StanOto::OK);
    SPRAWDZ(wLogu(p, "otoczenie: odpowiedź odrzucona"));
  }
  {
    // inny adres źródła niż w pamięci RTC: dane od nowa (wyczyscOtoczenie())
    Panel p;
    panel(p, 7, 0);
    p.start.stan.oto.magia = OTO_MAGIA;
    p.start.stan.oto.adres = 1;
    p.start.stan.oto.pobrano = 123;
    p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
    Dyrygent(p.adaptery(), k).wybudzenie();
    SPRAWDZ(p.start.stan.oto.adres == hashAdresu(k.otoUrl.c_str()));
  }
}

// Slot nocny (U1, R-KON-37): po zegarze bez rysowania; pierwsza decyzja nocy zapisana w NVS. Krok 2: pierwsza decyzja
// w wybudzeniu o 23:30 (R-KON-31), w nocy tylko wybudzenia pomiarowe (T-WYM-31), bez Wi-Fi i bez rysowania.
static void testSlotNocny() {
  Panel p;
  panel(p, 23, 30);  // pierwsza decyzja tej nocy (zapis w NVS), rysowanie według decyzji S9
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.nvs.teksty.count("cpap/noc_w") == 1 && zadan(p, "/api/status") == 1);
  p.zegar.epoch = ep(2026, 11, 5, 2, 0);  // kolejne wybudzenie zegarem w nocy: bez błysku e-ink, bez Wi-Fi
  p.zegar.msTeraz = 0;
  p.ekran.odswiezen = 0;
  p.port.log.clear();
  p.siec.zadania.clear();
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.ekran.odswiezen == 0 && wLogu(p, "wybudzenie pomiarowe") && p.siec.zadania.empty());
  // naciśnięcie bez skutku w nocy (◀ 2 s na ekranie dnia) bez błysku
  p.d.z.clear();
  p.zegar.msTeraz = 0;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = BIT_LEWY;
  p.przyciski.skrypt = {{0, BIT_LEWY}, {2600, 0}, {6000, BIT_LEWY}, {8600, 0}};
  p.ekran.odswiezen = 0;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.ekran.odswiezen == 0 && wLogu(p, "naciśnięcie bez akcji"));
}

// Import historii z karty (R-DANE-31): /import.csv → /noce.csv, plik przemianowany.
static void testImport() {
  Panel p;
  panel(p, 7, 0);
  p.siec.siecJest = false;
  p.pliki.pliki["/import.csv"] =
      "data_poranka;x\n2026-10-01;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;\n"
      "2026-10-02;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;\nzly wiersz\n2099-01-01;1;1;1;1;1;1;1;;;;;;K;0;0;;;;\n";
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(wierszyDanych(p, "/dane/noce/2026.csv") == 2);
  SPRAWDZ(p.pliki.pliki.count("/import.csv") == 0 && p.pliki.pliki.count("/import.wczytany") == 1);
  SPRAWDZ(wLogu(p, "import historii: wczytane 2, uzupełnione 0, pominięte 2"));
}

// zapis pliku roku nocy nieudany, a zmiana nazwy /import.csv by się udała. Import nie
// może zostać oznaczony jako wczytany, gdy którykolwiek plik roku się nie zapisał (tu 2025, a 2026 tak); następne
// wybudzenie z działającym zapisem wczytuje go od nowa bez dublowania nocy.
static void testImportZapisNieudany() {
  Panel p;
  panel(p, 7, 0);
  p.siec.siecJest = false;
  p.pliki.pliki["/import.wczytany"] = "stary import\n";
  p.pliki.pliki["/import.csv"] =
      "2025-12-31;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;\n"
      "2026-10-01;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;\n"
      "2026-10-02;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;\n";
  p.pliki.odmawiaZapisuPod = "/dane/noce/2025";
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.pliki.pliki.count("/import.csv") == 1);
  SPRAWDZ(p.pliki.pliki["/import.wczytany"] == "stary import\n");  // poprzedni import nietknięty
  SPRAWDZ(p.pliki.pliki.count("/dane/noce/2025.csv") == 0);
  SPRAWDZ(wierszyDanych(p, "/dane/noce/2026.csv") == 2);  // rok, który się zapisał, zostaje zapisany
  SPRAWDZ(wLogu(p, "import historii: zapis plików nocy nieudany, /import.csv zostaje do ponowienia"));
  SPRAWDZ(!wLogu(p, "import historii: wczytane"));

  p.pliki.odmawiaZapisuPod.clear();
  p.port.log.clear();
  panel(p, 7, 30);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(wierszyDanych(p, "/dane/noce/2025.csv") == 1 && wierszyDanych(p, "/dane/noce/2026.csv") == 2);
  SPRAWDZ(p.pliki.pliki.count("/import.csv") == 0 && p.pliki.pliki.count("/import.wczytany") == 1);
  SPRAWDZ(wLogu(p, "import historii: wczytane 1, uzupełnione 0, pominięte 2"));
}

// lista nocy ponad limit trasy odrzucona bez trzymania ciała w pamięci (AB_KOD_ZA_DUZO):
// nieudana próba bez ponowień, nie E-03, bez zapisu nocy; diagnostyka „odpowiedź za duża”.
static void testListaPonadLimit() {
  Panel p;
  panel(p, 7, 0);
  p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  p.siec.dodaj(sciezkaListy(), 200, std::string(AB_MAKS_BAJTOW_LISTY + 1, ' '));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(zadan(p, sciezkaListy()) == 1);  // bez ponowień
  SPRAWDZ(wLogu(p, "odpowiedź ponad 524288 B odrzucona"));
  SPRAWDZ(!p.start.stan.ostatniOdczytOk && wierszyDanych(p, "/dane/noce/2026.csv") == 0);
  SPRAWDZ(zadan(p, "/api/status") == 1);  // status w limicie przechodzi
}

// Lista przychodzi zwykle pod ścieżką joba (202, potem 200 z `days`). Job dziedziczy limit listy (512 KiB), nie
// raportu (64 KiB): lista ponad 64 KiB przechodzi, ponad 512 KiB jest odrzucona.
static void testListaPrzezJob() {
  std::string duza = lista(DZIS_DOBA - 1999, DZIS_DOBA);  // 2000 dób
  SPRAWDZ(duza.size() > AB_MAKS_BAJTOW_RAPORTU && duza.size() < AB_MAKS_BAJTOW_LISTY);
  {
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    p.siec.dodaj(sciezkaListy(), 202, "{\"job\":\"j1\",\"pending\":true}");
    p.siec.dodaj(sciezkaJoba("j1"), 200, duza);
    for (long d = DZIS_DOBA - 6; d <= DZIS_DOBA; d++) p.siec.dodaj(sciezkaNocy(d), 200, raport(d));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(!wLogu(p, "odrzucona") && p.start.stan.ostatniOdczytOk);
    SPRAWDZ(wierszyDanych(p, "/dane/noce/2026.csv") >= 1);
  }
  {
    Panel p;
    panel(p, 7, 0);
    p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    p.siec.dodaj(sciezkaListy(), 202, "{\"job\":\"j1\",\"pending\":true}");
    p.siec.dodaj(sciezkaJoba("j1"), 200, std::string(AB_MAKS_BAJTOW_LISTY + 1, ' '));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(wLogu(p, "odpowiedź ponad 524288 B odrzucona") && zadan(p, sciezkaJoba("j1")) == 1);
    SPRAWDZ(!p.start.stan.ostatniOdczytOk && wierszyDanych(p, "/dane/noce/2026.csv") == 0);
  }
}

// Bez zegara (NTP i AirBridge niedostępne po zaniku zasilania): ekran bez historii, sen 30 min.
static void testBezZegara() {
  Panel p;
  p.zegar.epoch = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  p.siec.siecJest = false;
  p.pliki.pliki["/import.csv"] = "2026-10-01;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;\n";
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.sen.plan.sekund == 1800 && p.ekran.odswiezen == 1);
  SPRAWDZ(wLogu(p, "import historii: czekam na zegar"));
  SPRAWDZ(p.pliki.pliki.count("/log/wybudzenia.csv") == 0);  // wpis baterii tylko ze znanym zegarem
}

// Przerwanie ponowienia ↻ na S5 kolejnym naciśnięciem: stan sprzed pobrania zostaje.
static void testPrzerwaniePonowienia() {
  Panel p;
  panel(p, 12, 0, PowodWybudzenia::PRZYCISK);
  p.start.budzace = BIT_SRODEK;
  p.start.stan.ostatniOdczytOk = false;
  p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  p.siec.dodaj("/api/report", 202, "{\"job\":\"j1\",\"pending\":true}");  // lista i odpytywanie joba
  p.przyciski.skrypt = {{0, BIT_SRODEK}, {300, 0}, {15000, BIT_PRAWY}, {15200, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(wLogu(p, "przerwane przyciskiem"));
  SPRAWDZ(!p.start.stan.ostatniOdczytOk && p.sen.snow == 1);
  // naciśnięcie, które przerwało pobranie przy przycisku budzącym, zjada czekajNaPuszczenie() po obsluz();
  // ekran zostaje ekranem dnia
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::DZIEN);
}

// Tryb awaryjny: ▶ → U-05 z oknem, ◀ → U-06 (okno zamknięte), sen; potem ↻ 2 s → zwykły start od nowa.
static void testAwaryjnyWrocIWyjdz() {
  uint32_t w = identyfikatorWersji(WERSJA_ATRAPY);
  Panel p;
  panel(p, 10, 0, PowodWybudzenia::PRZYCISK);
  ZnacznikAwaryjny z;
  z.jest = true;
  z.wersja = w;
  z.przyczyna = PrzyczynaStartu::TASK_WDT;
  z.u06Narysowane = true;
  p.nvs.teksty["cpap/awaryjny"] = zapiszZnacznik(z);
  p.start.budzace = BIT_PRAWY;
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {200, 0}, {30000, BIT_LEWY}, {30200, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.d.ile("siec.okno") == 1 && p.ekran.odswiezen == 2);  // U-05, potem U-06
  SPRAWDZ(p.d.ile("siec.wylacz") == 1 && p.siec.zadania.empty() && p.pliki.zapisy == 0);
  SPRAWDZ(p.sen.snow == 1 && !p.sen.plan.budzZegarem && p.dzwiek.piskow == 2);
  // ↻ 2 s na U-06 → znacznik usunięty, zwykły start z AirBridge
  p.d.z.clear();
  p.sen.snow = 0;
  p.zegar.msTeraz = 0;
  p.start.budzace = BIT_SRODEK;
  p.przyciski.skrypt = {{0, BIT_SRODEK}, {2500, 0}};
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.nvs.teksty.count("cpap/awaryjny") == 0 && zadan(p, "/api/status") == 1);
  SPRAWDZ(p.sen.snow == 1 && p.sen.plan.budzZegarem && p.start.stan.ekran == (uint8_t)Ekran::DZIEN);
  // U-05 bez sieci: okno nie otwiera się, po 20 s czuwania (okno zamknięte) U-06 i sen
  Panel q;
  panel(q, 10, 0, PowodWybudzenia::PRZYCISK);
  q.nvs.teksty["cpap/awaryjny"] = zapiszZnacznik(z);
  q.siec.siecJest = false;
  q.start.budzace = BIT_PRAWY;
  q.przyciski.skrypt = {{0, BIT_PRAWY}, {200, 0}};
  Dyrygent(q.adaptery(), KONF).wybudzenie();
  SPRAWDZ(q.d.ile("siec.okno") == 0 && q.ekran.odswiezen == 2 && q.sen.snow == 1);
}

// Samosprawdzenie po wybudzeniu przyciskiem (wersja do sprawdzenia, T-WYM-41): Wi-Fi tylko do sprawdzenia.
static void testSamosprawdzeniePoPrzycisku() {
  Panel p;
  panel(p, 10, 0, PowodWybudzenia::PRZYCISK);
  p.start.stanW = StanWersji::DO_SPRAWDZENIA;
  p.start.budzace = BIT_PRAWY;
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {200, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.d.ile("siec.polacz") == 1 && p.siec.zadania.empty());
  SPRAWDZ(p.start.oznaczona && p.sen.snow == 1);
}

// minimum pamięci z doby w RTC (ekran testu); nowa doba zaczyna od nowa.
static void testPamiecDoby() {
  Panel p;
  panel(p, 10, 0);
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  p.diag.sterta = 90000;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  p.diag.sterta = 80000;
  p.zegar.msTeraz = 0;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  p.diag.sterta = 85000;
  p.zegar.msTeraz = 0;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.pamiecSterta == 80000 && p.start.stan.pamiecStos == 5000);
  p.zegar.epoch = ep(2026, 11, 5, 10, 0);
  p.zegar.msTeraz = 0;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.pamiecSterta == 85000);
}

// Zanik zasilania: stan AirBridge z NVS (T-WYM-23: magia ≠ MAGIA_AB → NVS „noc”/„ab”).
static void testStanABzNvs() {
  Panel p;
  panel(p, 10, 0, PowodWybudzenia::START);
  StanAB ab;
  ab.magia = MAGIA_AB;
  ab.ma = true;
  ab.pobrano = 1793000000;
  snprintf(ab.wersja, sizeof ab.wersja, "%s", "v2.2.1");
  p.nvs.ustawBajty("noc", "ab", &ab, sizeof ab);
  p.siec.siecJest = false;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.start.stan.ab.ma && std::string(p.start.stan.ab.wersja) == "v2.2.1");
  // NVS z innym rozmiarem (stary układ): stan od zera
  Panel q;
  panel(q, 10, 0, PowodWybudzenia::START);
  q.nvs.bajtyMap["noc/ab"] = std::string(10, 'x');
  q.siec.siecJest = false;
  Dyrygent(q.adaptery(), KONF).wybudzenie();
  SPRAWDZ(!q.start.stan.ab.ma && q.start.stan.ab.rop == -1 && q.start.stan.ab.zalegle == -1);
}

// ---------- tryb awaryjny ----------

// wymaganie: R-AW-39, T-WYM-83
// naprawa wgrana przez okno serwisowe (wersja „do sprawdzenia”) przy znaczniku tej samej wersji nie wraca przez
// tryb awaryjny do zepsutej wersji: znacznik usunięty, zwykły start, samosprawdzenie i oznaczenie.
static void testNaprawaZTrybuAwaryjnego() {
  uint32_t w = identyfikatorWersji(WERSJA_ATRAPY);
  ZnacznikAwaryjny z;
  z.jest = true;
  z.wersja = w;
  z.przyczyna = PrzyczynaStartu::PANIKA;
  z.u06Narysowane = true;
  LicznikStartow l{};
  DecyzjaStartu d = decyzjaStartu(l, PrzyczynaStartu::PROGRAMOWY, w, z, StanWersji::DO_SPRAWDZENIA);
  SPRAWDZ(!d.awaryjny && d.usunZnacznik && d.poBledzie == 0);
  d = decyzjaStartu(l, PrzyczynaStartu::PROGRAMOWY, w, z, StanWersji::DOBRA);
  SPRAWDZ(d.awaryjny && !d.usunZnacznik);
  // 3 starty po błędzie w wersji „do sprawdzenia” (bootloader i tak wróci przy restarcie): bez znacznika
  l = LicznikStartow{};
  ZnacznikAwaryjny brak;
  decyzjaStartu(l, PrzyczynaStartu::ZASILANIE, w, brak, StanWersji::DO_SPRAWDZENIA);
  for (int i = 0; i < 3; i++) d = decyzjaStartu(l, PrzyczynaStartu::PANIKA, w, brak, StanWersji::DO_SPRAWDZENIA);
  SPRAWDZ(d.awaryjny && d.zapiszZnacznik);  // licznik działa jak dotąd; wersja zła i tak wróci w bootloaderze

  Panel p;
  panel(p, 10, 0, PowodWybudzenia::START);
  p.start.przyczynaStartu = PrzyczynaStartu::PROGRAMOWY;
  p.start.stanW = StanWersji::DO_SPRAWDZENIA;
  p.nvs.teksty["cpap/awaryjny"] = zapiszZnacznik(z);
  p.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.nvs.teksty.count("cpap/awaryjny") == 0);
  SPRAWDZ(p.start.oznaczona && !p.start.powrot && p.sen.snow == 1 && p.sen.plan.budzZegarem);
  SPRAWDZ(zadan(p, "/api/status") == 1);
}

// wymaganie: R-AW-39
// w trybie awaryjnym wszystkie przyciski wciśnięte przy zasypianiu → zegar co 30 min zamiast snu bez końca.
static void testPlanSnuAwaryjny() {
  PlanSnu a = planSnuAwaryjny(0);
  SPRAWDZ(!a.budzZegarem && a.przyciski == WSZYSTKIE_PRZYCISKI);
  PlanSnu b = planSnuAwaryjny(BIT_SRODEK);
  SPRAWDZ(!b.budzZegarem && b.przyciski == (BIT_LEWY | BIT_PRAWY));
  PlanSnu c = planSnuAwaryjny(WSZYSTKIE_PRZYCISKI);
  SPRAWDZ(c.budzZegarem && c.sekund == OKRES_SNU_S && c.przyciski == 0);
  // ↻ na U-05 odświeża stan
  SPRAWDZ(nacisniecieAwaryjne(EkranAwaryjny::U05, Przycisk::SRODEK, false) == AkcjaAwaryjna::ODSWIEZ);
  SPRAWDZ(nacisniecieAwaryjne(EkranAwaryjny::U06, Przycisk::SRODEK, false) == AkcjaAwaryjna::NIC);
}

// U-05 w trybie awaryjnym: wersja ze slotem, ↻ odświeża (drugie odświeżenie ekranu).
static void testU05Awaryjny() {
  ZnacznikAwaryjny z;
  z.jest = true;
  z.wersja = identyfikatorWersji(WERSJA_ATRAPY);
  z.przyczyna = PrzyczynaStartu::PANIKA;
  z.u06Narysowane = true;
  Panel p;
  panel(p, 10, 0, PowodWybudzenia::PRZYCISK);
  p.nvs.teksty["cpap/awaryjny"] = zapiszZnacznik(z);
  p.start.budzace = BIT_PRAWY;
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {200, 0}, {5000, BIT_SRODEK}, {5200, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.ekran.odswiezen == 3);  // U-05, U-05 po ↻, U-06 po zamknięciu okna
  SPRAWDZ(p.siec.zadania.empty() && p.pliki.zapisy == 0);
}

// decyzje ekranu dnia i nawigacji w rdzeniu (R-CZY-17, R-HUM-17, R-PIE-08, E-08, R-PRZ-06, R-EKR-57).
// wymaganie: R-CZY-17, R-HUM-17, R-PIE-08, R-PRZ-06
static void testDecyzjeEkranuDnia() {
  DecyzjaNocy brakNocy;
  KontekstNaw k = kontekstNawigacji(Ekran::DZIEN, brakNocy, true, false, true, true);
  SPRAWDZ(k.s5 && !k.nocny && k.zaproszenie && k.ostatniaNoc);
  SPRAWDZ(kontekstNawigacji(Ekran::DZIEN, brakNocy, false, true, false, false).s5);  // ↻ na S10 też ponawia
  SPRAWDZ(!kontekstNawigacji(Ekran::K1, brakNocy, true, true, false, false).s5);
  DecyzjaNocy s9;
  s9.nocny = true;
  KontekstNaw kn = kontekstNawigacji(Ekran::DZIEN, s9, true, false, false, false);
  SPRAWDZ(kn.nocny && !kn.s5);
  s9.nowa = true;  // pierwsza decyzja tej nocy: S9 jeszcze nie stoi na ekranie
  SPRAWDZ(!kontekstNawigacji(Ekran::DZIEN, s9, true, false, false, false).nocny);

  Otoczenie o{};
  wyczyscOtoczenie(o, true, 1);
  StanE08 st;
  LiniaDnia l = liniaDnia(true, "S2A", o, 1793790000, "2026-11-04", st);
  SPRAWDZ(l.zaproszenie && !l.e08 && !l.zapiszE08);
  l = liniaDnia(true, "S5", o, 1793790000, "2026-11-04", st);  // R-CZY-17 fail-path
  SPRAWDZ(!l.zaproszenie && !l.e08);
  // E-08: świeża odpowiedź z `co2_milczy` → linia raz na epizod
  uint32_t T = 1793790000;
  o.stan = (int8_t)StanOto::OK;
  o.pobrano = T - 60;
  o.proba = T - 60;
  o.co2Milczy = true;
  o.co2MilczyOd = T - 7200;
  bool zm = false;
  StanE08 ref;
  bool oczek = pokazE08(co2MilczyTeraz(o, T), false, o.co2MilczyOd, "2026-11-04", ref, zm);
  l = liniaDnia(false, "S2A", o, T, "2026-11-04", st);
  SPRAWDZ(l.e08 == oczek && l.zapiszE08 == zm && st.od == ref.od && st.dzien == ref.dzien);
  SPRAWDZ(!liniaDnia(false, "S5", o, T, "2026-11-04", st).e08);

  WpisHumoru w{"2026-11-04", "S2A", "", 'K', "B01", "3"};
  PlanWyboru pw = planWyboru(&w, "S2A", false);
  SPRAWDZ(pw.zWpisu && pw.wybor.tryb == 'K' && pw.wybor.scena == "B01" && pw.wybor.zdanie == "3" && !pw.dopisz);
  SPRAWDZ(planWyboru(&w, "S2A", true).wybor.tryb == 'D');  // R-HUM-17
  pw = planWyboru(&w, "S5", false);
  SPRAWDZ(!pw.zWpisu && !pw.dopisz);
  pw = planWyboru(nullptr, "S2A", false);
  SPRAWDZ(!pw.zWpisu && pw.dopisz);

  WpisCzynnikow c;
  SPRAWDZ(oznaczycZaproszenie(true, &c));
  c.pokazano = true;
  SPRAWDZ(!oznaczycZaproszenie(true, &c));
  SPRAWDZ(!oznaczycZaproszenie(false, nullptr) && !oznaczycZaproszenie(true, nullptr));

  SPRAWDZ(kartaS10(Humor::NORMALNY, "zdanie", true, true));
  SPRAWDZ(!kartaS10(Humor::WYLACZONY, "zdanie", true, true));
  SPRAWDZ(!kartaS10(Humor::NORMALNY, "", true, true) && !kartaS10(Humor::NORMALNY, "z", false, true) &&
          !kartaS10(Humor::NORMALNY, "z", true, false));

  StatusAB st1;
  SPRAWDZ(!statusCzytelny(poStatusie(200, "nie json", st1), st1));
  SPRAWDZ(statusCzytelny(poStatusie(200, "{\"version\":\"v2.1.9\",\"rop\":0}", st1), st1));
  SPRAWDZ(statusCzytelny(poStatusie(200, "{\"version\":\"v2.2.1\",\"rop\":0}", st1), st1));
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testPoranekZListaIZaleglymi();
  testBledyListyIRaportu();
  testDigest();
  testBezWiFi();
  testCzuwanieINawigacja();
  testZacietyPrzycisk();
  testR_PRZ_09_przyciskPrzedSnemPoZegarze();
  testWybudzeniePomiarowe();
  testWejsciePlanu();
  testR_KON_41_slotDziennyDyrygent();
  testR_PIE_13_wybudzenieWTerminie();
  testR_DANE_24_przyciskPrzedTerminem();
  testR_KON_31_przyciskPrzedPoczatkiemNocy();
  testR_NF_02_terminyZalegle();
  testR_DANE_19_startTuzPrzedTerminem();
  testT_WYM_29_zacietyWNocy();
  testT_NF_06_ekranTestuBezOkna();
  testNtpWLoguIRtc();
  testEkranTestuIOknoSerwisowe();
  testKomendaO();
  testPytanie();
  testOtoczenie();
  testSlotNocny();
  testImport();
  testImportZapisNieudany();
  testListaPonadLimit();
  testListaPrzezJob();
  testBezZegara();
  testPrzerwaniePonowienia();
  testAwaryjnyWrocIWyjdz();
  testSamosprawdzeniePoPrzycisku();
  testPamiecDoby();
  testStanABzNvs();
  testNaprawaZTrybuAwaryjnego();
  testPlanSnuAwaryjny();
  testU05Awaryjny();
  testDecyzjeEkranuDnia();
  printf("testy dyrygenta: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
