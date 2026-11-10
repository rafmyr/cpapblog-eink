// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe nocy liczonej na panelu z plików sesji (src/noc_pliki.*, ADR-0036). Każdy test, który pada,
// podaje w komunikacie nazwę przypadku. Dane wyłącznie syntetyczne.
// Pomocnicze funkcje (budowa plików EDF, atrapa AirBridge, panel) z test/testy_noc_pliki.cpp, dołączonego bez main().
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define main main_testy_noc_pliki
#include "testy_noc_pliki.cpp"
#undef main
#pragma GCC diagnostic pop

#include <random>

static void przypadek(const char* nr, bool w, const char* opis) {
  if (w) {
    ok++;
  } else {
    zle++;
    printf("BŁĄD %s: %s\n", nr, opis);
  }
}

// M-18 ścieżki JSON. Porównanie 10 nocy narzędziem noc_panelu.sh liczy M-18 raportu z `sessions[]`
// jak czas lokalny aparatu (m17m18(), nie nocZRaportu()) i wyszło zgodne z plikami: to dowód, że `start_ms` raportu
// to sekundy cywilne aparatu × 1000. Panel w ścieżce JSON (nocZRaportu() → m18ZChwili()) przelicza je przez
// localtime_r() w strefie CET/CEST, więc M-18 jest przesunięte o +60 min (zima) albo +120 min (lato).
static void testM18Json() {
  struct P {
    const char* poranek;
    int r, mi, d;
  } przypadki[] = {{"2026-11-04", 2026, 11, 3}, {"2026-07-04", 2026, 7, 3}};
  for (auto& x : przypadki) {
    long day = nrDnia(x.poranek) - 1;
    long long startMs = cyw(x.r, x.mi, x.d, 23, 0) * 1000LL;  // jak noce.py i AirBridge: czas aparatu 23:00
    std::string rap = "{\"day\":" + std::to_string(day) +
                      ",\"present\":true,\"partial\":false,\"used_seconds\":28800,\"ahi\":1.0,\"events_known\":true,"
                      "\"session_count\":1,\"sessions\":[{\"start_ms\":" +
                      std::to_string(startMs) + ",\"end_ms\":" + std::to_string(startMs + 28800000LL) +
                      "}],\"counts\":{\"oa\":1,\"ca\":1,\"h\":1},\"channels\":[],\"settings\":{\"available\":false}}";
    NocAB j = nocZRaportu(rap, day, "ASVAuto", "", "");
    // noc z plików tej doby: M-18 = 660 (23:00 od 12:00)
    PlikiDoby p = sesja(cyw(x.r, x.mi, x.d, 23, 0), 480);
    NocAB f = nocZWyniku(policz(day, p), nullptr, "ASVAuto");
    printf("  M-18 JSON %s: M-18 z plików %d, ze ścieżki JSON %d\n", x.poranek, f.noc.m18, j.noc.m18);
    przypadek("M-18 JSON", f.noc.m18 == 660 && j.noc.m18 == f.noc.m18,
               "M-18 ścieżki JSON przesunięte o strefę panelu (przesunięcie nie jest fałszywym alarmem)");
  }
}

// STR.edf bez doby ostatniej nocy. ADR-0036 pkt 4: każdy problem z plikami = doba z raportu JSON.
// Brak doby w STR nie jest traktowany jako problem: noc zapisana z pustymi ustawieniami (M-21, mpa…epr) i nigdy nie
// uzupełniona, choć raport JSON tej doby ustawienia ma.
static void testStrBezDoby() {
  DobaAB d = dobaAB(DZIS4);
  const std::string rapUst =
      "{\"day\":" + std::to_string(DZIS4) +
      ",\"present\":true,\"partial\":false,\"used_seconds\":28800,\"ahi\":1.4,\"events_known\":true,"
      "\"session_count\":1,\"counts\":{\"oa\":1,\"ca\":5,\"h\":4},\"channels\":[{\"id\":\"Press\",\"unit\":\"cmH2O\","
      "\"median\":9.8,\"p95\":13.6,\"source\":\"edf\"}],\"settings\":{\"available\":true,\"fields\":["
      "{\"tag\":\"MOP\",\"value\":\"ASVAuto\",\"option\":8},{\"tag\":\"EAI\",\"value\":\"6.0\"},"
      "{\"tag\":\"EAX\",\"value\":\"10.0\"},{\"tag\":\"ANS\",\"value\":\"3.0\"},{\"tag\":\"AXS\",\"value\":\"15.0\"}]}}";
  std::string ustJson;
  {
    PanelN s("noc_zrodlo=json\n");
    odpowiedzi(s, d);
    s.p.siec.odpowiedzi[sciezkaNocy(DZIS4)].clear();
    s.p.siec.dodaj(sciezkaNocy(DZIS4), 200, rapUst);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    if (w.size() > 12) ustJson = w[12];
  }
  PanelN s;
  odpowiedzi(s, d);
  s.p.siec.odpowiedzi[sciezkaNocy(DZIS4)].clear();
  s.p.siec.dodaj(sciezkaNocy(DZIS4), 200, rapUst);
  s.p.siec.odpowiedzi[sciezkaPobraniaStr()].clear();
  s.p.siec.dodaj(sciezkaPobraniaStr(), 200, strDoby(DZIS4 - 1));  // STR jeszcze bez doby ostatniej nocy
  wybudz(s, ep(2026, 11, 4, 7, 0));
  std::vector<std::string> w = wiersz(s, "2026-11-04");
  std::string ustPliki = w.size() > 12 ? w[12] : "?";
  printf("  STR bez doby: ustawienia ze ścieżki JSON \"%s\", z plików przy STR bez doby \"%s\"\n", ustJson.c_str(),
         ustPliki.c_str());
  SPRAWDZ(!ustJson.empty());
  przypadek("STR bez doby", ustPliki == ustJson,
             "STR bez doby ostatniej nocy: noc z plików zapisana bez ustawień (M-21), bez zapasu JSON (ADR-0036 pkt 4)");
}

// Plik z bajtami po zadeklarowanych rekordach (np. nagłówek z liczbą rekordów sprzed dopisania). noce.py
// (wzorzec) oznacza go `plik niepełny` → noc częściowa; edf::czytaj() go przyjmuje, więc panel liczy noc jako pełną
// z części danych.
static void testPlikNiepelny() {
  long long s = cyw(2026, 11, 1, 23, 0);
  PlikiDoby p = sesja(s, 480);
  std::string& pl = p[prefiks(s) + "_PLD.edf"];
  pl += std::string(602 * 30, '\0');  // 30 rekordów (30 min) za zadeklarowanymi 480
  WynikNocy r = policz(D1, p);
  przypadek("plik niepełny", r.czesciowa,
             "PLD dłuższy niż nagłówek+rekordy: noce.py daje „plik niepełny” (częściowa), panel noc pełną");
}

// Czas rekordu ≤ 0 w PLD (uszkodzony nagłówek) przechodzi edf::czytaj(); granice zapisu wychodzą ujemne
// i po cichu zmniejszają M-01 i zawyżają AHI (noce.py ma tę samą lukę, edf.py też nie sprawdza czasu rekordu).
static void testCzasRekordu() {
  long long s = cyw(2026, 11, 1, 23, 0), s2 = cyw(2026, 11, 2, 5, 0);
  std::vector<Zd> zd;
  for (int i = 0; i < 20; i++) zd.push_back({s + 600 + i * 600, 10, "Hypopnea"});
  PlikiDoby p = sesja(s, 300, nullptr, zd);
  PlikiDoby b = sesja(s2, 60);
  // PLD drugiej sesji z czasem rekordu −60 s
  std::vector<std::vector<std::string>> rek;
  for (int i = 0; i < 60; i++) {
    Wartosci w = domyslne();
    std::vector<std::string> r;
    for (auto& sg : PLD_SYG)
      r.push_back(sg.etykieta == "Crc16" ? probki({0}) : probki(std::vector<long>(30, cyf(sg, w[sg.etykieta]))));
    rek.push_back(r);
  }
  b[prefiks(s2) + "_PLD.edf"] = budujEdf(s2, -60, PLD_SYG, rek);
  p.insert(b.begin(), b.end());
  WynikNocy r = policz(D1, p);
  printf("  czas rekordu: sekundy %.0f (zapis 300 + 60 min), AHI %.2f, częściowa %d\n", r.sekundy, r.ahi, (int)r.czesciowa);
  przypadek("czas rekordu", r.czesciowa || std::isnan(r.sekundy),
             "PLD z czasem rekordu < 0: ujemny czas zapisu po cichu odejmuje się od M-01 (AHI zawyżone)");
}

// `noc_json` z jednym złym ID (literówka) odrzuca cały wiersz, więc przy `noc_zrodlo=plik` panel bierze
// WSZYSTKIE metryki z plików, także te, które właściciel chciał zostawić z raportu (rozjazd WY9).
static void testNocJsonLiterowka() {
  DobaAB d = dobaAB(DZIS4);
  PanelN s("noc_zrodlo=plik\nnoc_json=m02,m04\n");
  odpowiedzi(s, d);
  wybudz(s, ep(2026, 11, 4, 7, 0));
  std::vector<std::string> w = wiersz(s, "2026-11-04");
  printf("  literówka noc_json: żądań raportu JSON %d, AHI w rekordzie \"%s\" (raport 1.4)\n", zadan(s, sciezkaNocy(DZIS4)),
         w.size() > 3 ? w[3].c_str() : "?");
  przypadek("literówka noc_json", zadan(s, sciezkaNocy(DZIS4)) == 1,
             "zła lista noc_json: wiersz pominięty, AHI (m02) z plików wbrew intencji; bezpieczniej cała noc z JSON");
}

// ---- sprawdzenia, które trzymają (dowody do raportu) ----

// adnotacje: złośliwe TAL (ogromne liczby, ujemne czasy, brak 0x14, NaN, nieskończoność) i losowe uszkodzenia plików
// EVE, CSL i PLD pod ASan/UBSan: bez awarii, a plik z adnotacją nieczytelną daje noc częściową.
static std::string eveSurowy(long long start, const std::string& tal) {
  std::vector<SygT> syg = {{"EDF Annotations", "", 0, 1, -32768, 32767, 60}, CRC};
  return budujEdf(start, 0, syg, {{std::string("+0\x14\x14\0", 5) + tal, probki({0})}}, "EDF+D");
}
static void testAdnotacjeZlosliwe() {
  long long s = cyw(2026, 11, 1, 23, 0);
  const std::string TALE[] = {
      std::string("+1e308\x14Hypopnea\x14\0", 17),
      std::string("-1e308\x15" "5\x14Hypopnea\x14\0", 19),
      std::string("+99999999999999999999999\x14Hypopnea\x14\0", 34),
      std::string("+5\x15-3\x14Hypopnea\x14\0", 16),
      std::string("+nan\x14Hypopnea\x14\0", 15),
      std::string("+inf\x14Hypopnea\x14\0", 15),
      std::string("+1e400\x14Hypopnea\x14\0", 17),
      std::string("5\x14Hypopnea\x14\0", 13),
      std::string("+5Hypopnea", 10),
      std::string("+\x14\0", 3),
      std::string("-600\x14" "CSR Start\x14\0", 16),
  };
  int czesciowe = 0, n = 0;
  for (auto& t : TALE) {
    PlikiDoby p = sesja(s, 480);
    p[prefiks(s) + "_EVE.edf"] = eveSurowy(s, t);
    p[prefiks(s) + "_CSL.edf"] = eveSurowy(s, t);
    WynikNocy r = policz(D1, p);
    NocAB nab = nocZWyniku(r, nullptr, "ASVAuto");
    n++;
    czesciowe += r.czesciowa;
    // nigdy „ważna” z nieskończonym albo NaN AHI
    SPRAWDZ(!nab.noc.wazna || std::isfinite(nab.noc.ahi));
  }
  printf("  adnotacje złośliwe: %d przypadków, %d nocy częściowych\n", n, czesciowe);
  // losowe uszkodzenia bajtów (nagłówek i dane) we wszystkich trzech plikach
  std::mt19937 los(12345);
  PlikiDoby wzor = sesja(s, 120, nullptr, {{s + 600, 10, "Obstructive Apnea"}, {s + 900, -1, "CSR Start"}});
  int przyjete = 0;
  for (int i = 0; i < 1500; i++) {
    PlikiDoby p = wzor;
    for (auto& x : p) {
      std::string& b = x.second;
      int ile = (int)(los() % 6);
      for (int k = 0; k < ile; k++) b[los() % b.size()] = (char)(los() & 0xFF);
      if (los() % 10 == 0) b.resize(los() % b.size());
    }
    WynikNocy r = policz(D1, p);
    NocAB nab = nocZWyniku(r, nullptr, "ASVAuto");
    przyjete += !r.czesciowa;
    SPRAWDZ(!nab.noc.wazna || (std::isfinite(nab.noc.ahi) && nab.noc.m01 >= 240));
  }
  printf("  losowe uszkodzenia: %d z 1500 nocy niepartial\n", przyjete);
}

// limity: plik dokładnie na limicie 2 MiB jest pobierany, o bajt większy idzie do zapasu; pamięć liczników nocy 47 h
// nie rośnie z długością (liczniki wartości, nie próbki)
static void testPamiecLicznikow() {
  long long s = cyw(2026, 11, 1, 23, 0);
  LicznikNocy a(D1), b(D1);
  policz(D1, sesja(s, 60), {}, &a);
  policz(D1, sesja(s, 600), {}, &b);
  printf("  liczniki: 1 h %u B, 10 h %u B\n", (unsigned)a.bajtyLicznikow(), (unsigned)b.bajtyLicznikow());
  SPRAWDZ(b.bajtyLicznikow() < 2 * a.bajtyLicznikow() + 4096);
  SPRAWDZ(limitOdpowiedziAB(sciezkaPobraniaPliku("/DATALOG/20261103/20261103_230000_PLD.edf")) ==
          AB_MAKS_BAJTOW_PLIKU_SESJI);
  SPRAWDZ(limitOdpowiedziAB(sciezkaPobraniaStr()) == AB_MAKS_BAJTOW_STR);  // STR nie łapie limitu plików sesji
}

// lista sesji: kursor w kółko (ta sama strona z kursorem) → najwyżej STRON_NA_DOBE żądań, potem raport JSON
static void testKursorWKolko() {
  DobaAB d = dobaAB(DZIS4);
  PanelN s;
  odpowiedzi(s, d);
  s.p.siec.odpowiedzi["/api/sessions?since="].clear();
  s.p.siec.dodaj("/api/sessions?since=", 200, sesjeJson(dobaAB(DZIS4 - 1), true, false, "k"));
  wybudz(s, ep(2026, 11, 4, 7, 0));
  printf("  kursor w kółko: %d żądań listy\n", zadan(s, "/api/sessions?since="));
  SPRAWDZ(zadan(s, "/api/sessions?since=") <= STRON_NA_DOBE);
  std::vector<std::string> w = wiersz(s, "2026-11-04");
  SPRAWDZ(w.size() > 2 && w[2] == "300");  // noc z raportu JSON
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testM18Json();
  testStrBezDoby();
  testPlikNiepelny();
  testCzasRekordu();
  testNocJsonLiterowka();
  testAdnotacjeZlosliwe();
  testPamiecLicznikow();
  testKursorWKolko();
  printf("testy_noc_pliki_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
