// SPDX-License-Identifier: GPL-3.0-or-later
// Testy trybu porównawczego (`noc_zrodlo=porownanie`, src/porownanie.h, ADR-0036 „Zmiana 2026-11-09”; od „Zmiany
// 2026-11-11” tylko jawnie, domyślny jest `plik`, więc testy trybu porównawczego ustawiają klucz):
//  (a) rdzeń: porownanie::porownaj() w tolerancjach WY9 (ta sama funkcja co tools/kalibracja/noc_panelu.cpp), wiersze
//      pominięte (OA/CA „nieznane” w plikach przy 0 w raporcie, PS tylko w plikach), brak w plikach = rozjazd;
//  (b) wpisy /dane/porownanie.csv: zapis i odczyt, złe wiersze, zastąpienie doby, limit 30 dób;
//  (c) wiersz U-05 `Noce z plików`: licznik z ostatnich 7 dób z porównaniem, dopisek przy 7 z 7, brak porównania
//      z przyczyną, szerokość w kolumnie;
//  (d) dyrygent z atrapami: tryb `porownanie`, noc w historii zawsze z raportu JSON (bajt w bajt jak przy `json`), wpis
//      zgodna / rozjazd / brak z każdą przyczyną, 404, plik ucięty, bez powtórnych pobrań, `json` nic nie pobiera,
//      `plik` bez zmian, U-05 i diagnostyka;
//  (e) tryb domyślny `plik` (decyzja właściciela 11.11.2026): bez wiersza i przy złej wartości noc z plików
//      z PS, zapas JSON przy problemie z plikami (R-DANE-44), bez wpisów porównania, noce zapisane wcześniej bez zmian.
// Dane wyłącznie syntetyczne. Pomocnicze funkcje z test/testy_noc_pliki.cpp (dołączony bez main()).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define main main_testy_noc_pliki
#include "testy_noc_pliki.cpp"
#undef main
#pragma GCC diagnostic pop

#include "ekrany.h"
#include "porownanie.h"

using namespace porownanie;

// ============================================================================================================
// (a) rdzeń porównania
// ============================================================================================================
static NocAB nocWzorcowa() {
  NocAB n;
  n.stan = StanRaportu::KOMPLETNA;
  n.pelna = true;
  n.sesje = 1;
  n.tryb = "ASVAuto";
  Noc& x = n.noc;
  x.data = "2026-11-04";
  x.wazna = true;
  x.m01 = 480;
  x.ahi = 0.5f;
  x.oa = 0;
  x.ca = 0;
  x.h = 0.5f;
  x.ua = 0;
  x.cmed = 14;
  x.cp95 = 14;
  x.emed = 7;
  x.ep95 = 7;
  x.wmed = 6;
  x.wp95 = 6;
  x.m08 = 0;
  x.csr = 0;
  x.m10 = 0.05f;
  x.m10p = 0.05f;
  x.m13 = 15;
  x.m14 = 450;
  x.m15 = 7.5f;
  x.m17 = 0;
  x.m18 = 660;
  x.ustawienia = "ASVAuto/6/10/3/15";
  return n;
}
static const Wiersz* wiersz(const Wynik& w, const std::string& id) {
  for (auto& x : w.wiersze)
    if (x.id == id) return &x;
  return nullptr;
}
static Ocena ocena(const Wynik& w, const std::string& id) {
  const Wiersz* x = wiersz(w, id);
  return x ? x->ocena : Ocena::BEZ_STR;
}

static void testRdzen() {
  NocAB r = nocWzorcowa();
  NocAB p = nocWzorcowa();
  p.noc.psmed = 7;
  p.noc.psp95 = 7;
  p.noc.oa = NAN;  // EVE tylko `Apnea`: OA i CA nieznane w plikach
  p.noc.ca = NAN;
  Wynik w = porownaj(p, r);
  ROWNE_L(w.rozjazdy, 0);
  SPRAWDZ(ocena(w, "m03oa") == Ocena::POMINIETE_APARAT && ocena(w, "m03ca") == Ocena::POMINIETE_APARAT);
  SPRAWDZ(ocena(w, "m28") == Ocena::TYLKO_PLIKI && ocena(w, "m28p") == Ocena::TYLKO_PLIKI);
  ROWNE_L(w.wiersze.size(), 28);  // 3 flagi, sesje, 21 metryk liczbowych (z M-14, M-15), M-21 i PS ×2 jak w tabeli narzędzia
  Wpis e = wpisZWyniku("2026-11-04", w);
  SPRAWDZ(e.werdykt == Werdykt::ZGODNA);
  ROWNE(e.rozjazd, "");
  ROWNE(e.pominiete, "m03oa:aparat|m03ca:aparat|m28:pliki|m28p:pliki");
  // w tolerancji na granicy: M-05 +0,5, M-01 +1, M-18 +5
  p.noc.cmed = 14.5f;
  p.noc.m01 = 481;
  p.noc.m18 = 665;
  ROWNE_L(porownaj(p, r).rozjazdy, 0);
  // rozjazd: M-05 o 0,6, ważność inna
  p.noc.cmed = 14.6f;
  p.noc.wazna = false;
  w = porownaj(p, r);
  ROWNE_L(w.rozjazdy, 2);
  e = wpisZWyniku("2026-11-04", w);
  SPRAWDZ(e.werdykt == Werdykt::ROZJAZD);
  ROWNE(e.rozjazd, "wazna=nie~tak|m05=14.60~14.00");
  // OA w raporcie > 0 przy „nieznane” w plikach: to rozjazd (brak w plikach), nie pominięcie
  p = nocWzorcowa();
  p.noc.oa = NAN;
  r.noc.oa = 1.5f;
  w = porownaj(p, r);
  SPRAWDZ(ocena(w, "m03oa") == Ocena::BRAK_W_PLIKACH);
  ROWNE_L(w.rozjazdy, 1);
  // metryka tylko w raporcie (M-08) = rozjazd; tylko w plikach (M-13) = bez wpływu
  r = nocWzorcowa();
  p = nocWzorcowa();
  p.noc.m08 = NAN;
  r.noc.m13 = NAN;
  w = porownaj(p, r);
  SPRAWDZ(ocena(w, "m08") == Ocena::BRAK_W_PLIKACH && ocena(w, "m13") == Ocena::BRAK_W_RAPORCIE);
  ROWNE_L(w.rozjazdy, 1);
  // M-15 (wentylacja minutowa, 11.11.2026): tolerancja WY9 0,3 L/min; brak w plikach przy wartości w raporcie = rozjazd
  r = nocWzorcowa();
  p = nocWzorcowa();
  p.noc.m15 = 7.75f;
  w = porownaj(p, r);
  SPRAWDZ(ocena(w, "m15") == Ocena::TAK);
  ROWNE_L(w.rozjazdy, 0);
  p.noc.m15 = 7.85f;
  w = porownaj(p, r);
  SPRAWDZ(ocena(w, "m15") == Ocena::NIE);
  ROWNE_L(w.rozjazdy, 1);
  p.noc.m15 = NAN;
  SPRAWDZ(ocena(porownaj(p, r), "m15") == Ocena::BRAK_W_PLIKACH);
  // granica tolerancji z liczb float (margines względny): 7.8f − 7.5f = 0.3000002 → tak, 7.81f → NIE
  p.noc.m15 = 7.8f;
  SPRAWDZ(ocena(porownaj(p, r), "m15") == Ocena::TAK);
  p.noc.m15 = 7.81f;
  SPRAWDZ(ocena(porownaj(p, r), "m15") == Ocena::NIE);
  p = nocWzorcowa();
  p.noc.cmed = 14.5f;  // M-05 na granicy 0,5
  SPRAWDZ(ocena(porownaj(p, r), "m05") == Ocena::TAK);
  p.noc.cmed = 14.51f;
  SPRAWDZ(ocena(porownaj(p, r), "m05") == Ocena::NIE);
  // M-14 (objętość oddechowa): tolerancja WY9 20 mL
  p = nocWzorcowa();
  p.noc.m14 = 470;
  SPRAWDZ(ocena(porownaj(p, r), "m14") == Ocena::TAK);
  p.noc.m14 = 471;
  SPRAWDZ(ocena(porownaj(p, r), "m14") == Ocena::NIE);
  // ustawienia: inny tekst = rozjazd; bez STR (narzędzie) = pominięte
  p = nocWzorcowa();
  p.noc.ustawienia = "ASVAuto/6/10/3/14";
  ROWNE_L(porownaj(p, r).rozjazdy, 1);
  w = porownaj(p, r, false);
  ROWNE_L(w.rozjazdy, 0);
  SPRAWDZ(ocena(w, "m21") == Ocena::BEZ_STR);
  // wartości z separatorami pliku nie psują wiersza
  p.noc.ustawienia = "a;b|c~d=e:f";
  e = wpisZWyniku("2026-11-04", porownaj(p, r));
  SPRAWDZ(e.rozjazd.find(';') == std::string::npos);
  Wpis z;
  SPRAWDZ(parsujWpis(wierszWpisu(e), z) && z.rozjazd == e.rozjazd);
  ROWNE(liczba(NAN), "–");
}

// ============================================================================================================
// (b) wpisy /dane/porownanie.csv
// ============================================================================================================
static void testWpisy() {
  Wpis b = wpisBraku("2026-11-05", Przyczyna::STARY_AIRBRIDGE);
  ROWNE(wierszWpisu(b), "2026-11-05;brak;;;AirBridge < 2.3.1");
  Wpis z;
  SPRAWDZ(parsujWpis("2026-11-05;brak;;;AirBridge < 2.3.1", z) && z.werdykt == Werdykt::BRAK &&
          z.przyczyna == "AirBridge < 2.3.1");
  SPRAWDZ(parsujWpis("2026-11-04;rozjazd;m05=14.60~14.00;m28:pliki;", z) && z.werdykt == Werdykt::ROZJAZD);
  for (const char* zly : {"", "2026-11-04;zgodna;;", "2026-13-40;zgodna;;;", "2026-11-04;ok;;;", "x;zgodna;;;",
                          "2026-11-04;zgodna;;;;"})
    SPRAWDZ(!parsujWpis(zly, z));
  for (Przyczyna p : {Przyczyna::STARY_AIRBRIDGE, Przyczyna::BLAD_LISTY, Przyczyna::BEZ_SESJI, Przyczyna::ZA_DUZO_SESJI,
                      Przyczyna::NIEZAMKNIETA, Przyczyna::BEZ_STR, Przyczyna::STR_BEZ_DOBY, Przyczyna::PLIK_NIEPOBRANY,
                      Przyczyna::PLIK_NIECZYTELNY, Przyczyna::CZESCIOWA, Przyczyna::BRAK_CZASU}) {
    std::string o = opisPrzyczyny(p);
    SPRAWDZ(!o.empty() && o.find(';') == std::string::npos);
  }
  ROWNE(opisPrzyczyny(Przyczyna::BEZ_SESJI), "brak plików");
  ROWNE(opisPrzyczyny(Przyczyna::BRAK_CZASU), "brak czasu");
  ROWNE(opisPrzyczyny(Przyczyna::NIEZAMKNIETA), "sesja niezamknięta");
  // parsujWpisy: złe wiersze pominięte, sortowanie, późniejszy wiersz tej daty wygrywa
  std::vector<Wpis> v = parsujWpisy({"2026-11-06;zgodna;;;", "zly", "2026-11-04;brak;;;brak czasu", "2026-11-04;zgodna;;;"});
  ROWNE_L(v.size(), 2);
  SPRAWDZ(v.size() == 2 && v[0].data == "2026-11-04" && v[0].werdykt == Werdykt::ZGODNA);
  // dopisz: zastąpienie doby, limit 30
  std::vector<Wpis> w;
  for (int i = 0; i < 40; i++) dopisz(w, wpisBraku(dataZNr(nrDnia("2026-10-01") + i), Przyczyna::BRAK_CZASU));
  ROWNE_L(w.size(), MAKS_WPISOW);
  ROWNE(w.front().data, dataZNr(nrDnia("2026-10-01") + 10));
  SPRAWDZ(!gotowe(w, w.back().data));  // brak = do powtórzenia
  Wpis g;
  g.data = w.back().data;
  g.werdykt = Werdykt::ZGODNA;
  dopisz(w, g);
  ROWNE_L(w.size(), MAKS_WPISOW);
  SPRAWDZ(gotowe(w, g.data));
}

// ============================================================================================================
// (c) wiersz U-05
// ============================================================================================================
static std::vector<Wpis> wpisy(const std::vector<Werdykt>& v, const std::string& od = "2026-11-01") {
  std::vector<Wpis> w;
  for (size_t i = 0; i < v.size(); i++) {
    Wpis e;
    e.data = dataZNr(nrDnia(od) + (long)i);
    e.werdykt = v[i];
    if (v[i] == Werdykt::ROZJAZD) e.rozjazd = "m05p=14.60~13.40|m02=3.10~2.40";
    if (v[i] == Werdykt::BRAK) e.przyczyna = "sesja niezamknięta";
    w.push_back(e);
  }
  return w;
}
static std::string u05(const std::vector<Wpis>& w, Zrodlo z = Zrodlo::POROWNANIE) {
  return dopasujWartosc("Noce z plików", wartosciU05(policz(w), z), TEST_SZEROKOSC);
}

static void testU05() {
  const Werdykt Z = Werdykt::ZGODNA, R = Werdykt::ROZJAZD, B = Werdykt::BRAK;
  ROWNE(u05({}), "jeszcze bez porównania");
  ROWNE(u05({}, Zrodlo::JSON), "wyłączone (json)");
  ROWNE(u05({}, Zrodlo::PLIK), "włączone (plik)");
  std::vector<Wpis> stary = {wpisBraku("2026-11-04", Przyczyna::STARY_AIRBRIDGE)};
  ROWNE(u05(stary), "brak porównania: AirBridge < 2.3.1");
  ROWNE(u05({wpisBraku("2026-11-04", Przyczyna::PLIK_NIECZYTELNY)}), "brak porównania: plik nieczytelny");
  ROWNE(u05(wpisy({Z, Z, Z})), "zgodne 3 z 3");
  ROWNE(u05(wpisy({Z, Z, Z, Z, Z, Z})), "zgodne 6 z 6");
  Licznik l7 = policz(wpisy({Z, Z, Z, Z, Z, Z, Z}));
  SPRAWDZ(l7.moznaPrzelaczyc());
  std::string w7 = u05(wpisy({Z, Z, Z, Z, Z, Z, Z}));
  ROWNE(w7, "zgodne 7 z 7, można przełączyć na pliki");
  // dni bez porównania (brak) nie przerywają ani nie liczą się do 7
  SPRAWDZ(policz(wpisy({Z, Z, B, Z, Z, Z, B, Z, Z, B})).moznaPrzelaczyc());
  // rozjazd w ostatnich 7: licznik i metryki (M-05 z m05p, M-02)
  ROWNE(u05(wpisy({Z, Z, Z, R, Z, Z, Z})), "rozjazd 1 z 7 (M-05, M-02)");
  SPRAWDZ(!policz(wpisy({Z, Z, Z, R, Z, Z, Z})).moznaPrzelaczyc());
  // rozjazd starszy niż ostatnie 7 dób z porównaniem: nie liczy się, 7 z 7 znowu
  SPRAWDZ(policz(wpisy({R, Z, Z, Z, Z, Z, Z, Z})).moznaPrzelaczyc());
  // każdy wariant mieści się w kolumnie (bez „…” w skrajnych przypadkach)
  for (auto& w : {u05(stary), u05(wpisy({R, R, R, R, R, R, R})), w7})
    SPRAWDZ(szer(F_P20B, w) <= miejsceNaWartosc("Noce z plików", TEST_SZEROKOSC));
  // „Nie oceniam” nigdy nie znaczy „w porządku” (ADR-0029 pkt 7): żaden wariant nie mówi o nocy pacjenta
  for (auto& v : {wartosciU05(policz(wpisy({Z, Z, Z, Z, Z, Z, Z})), Zrodlo::POROWNANIE), wartosciU05(policz(stary), Zrodlo::POROWNANIE)})
    for (auto& x : v) SPRAWDZ(x.find("w porządku") == std::string::npos && x.find("dobra") == std::string::npos);
  std::string d = liniaDiagnostyki(wpisy({Z, Z, R, B}), Zrodlo::POROWNANIE);
  SPRAWDZ(d.find("noc_zrodlo=porownanie") != std::string::npos && d.find("m05p=14.60~13.40") != std::string::npos &&
          d.find("sesja niezamknięta") != std::string::npos);
  SPRAWDZ(liniaDiagnostyki(wpisy({Z, Z, Z, Z, Z, Z, Z}), Zrodlo::POROWNANIE).find("noc_zrodlo=plik") != std::string::npos);
}

// ============================================================================================================
// (d) dyrygent
// ============================================================================================================
// Raport JSON zgodny z nocą z plików dobaAB(): 480 min, 1 hipopnea, OA i CA 0 (aparat ASV nie rozróżnia), ciśnienia
// 14/7, wyciek 6 L/min, ustawienia jak STR doby
static std::string raportZgodny(long day) {
  long long s = (long long)day * 86400 + 23 * 3600;
  return "{\"day\":" + std::to_string(day) +
         ",\"present\":true,\"partial\":false,\"used_seconds\":28800,\"ahi\":0.125,\"events_known\":true,"
         "\"session_count\":1,\"sessions\":[{\"start_ms\":" + std::to_string(s * 1000) + ",\"end_ms\":" +
         std::to_string((s + 28800) * 1000) +
         "}],\"counts\":{\"oa\":0,\"ca\":0,\"ua\":0,\"h\":1,\"rera\":0},\"csr_seconds\":0,\"leak_high_seconds\":0,"
         "\"channels\":[{\"id\":\"Press\",\"unit\":\"cmH2O\",\"median\":14,\"p95\":14,\"source\":\"edf\"},"
         "{\"id\":\"EprPress\",\"unit\":\"cmH2O\",\"median\":7,\"p95\":7,\"source\":\"edf\"},"
         "{\"id\":\"Leak\",\"unit\":\"L/min\",\"median\":6,\"p95\":6,\"source\":\"edf\"},"
         "{\"id\":\"RespRate\",\"unit\":\"bpm\",\"median\":15,\"p95\":15,\"source\":\"edf\"},"
         "{\"id\":\"FlowLim\",\"unit\":\"\",\"median\":0.05,\"p95\":0.05,\"source\":\"edf\"}],"
         "\"settings\":{\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"value\":\"ASVAuto\",\"option\":8},"
         "{\"tag\":\"EAI\",\"value\":\"6.0\"},{\"tag\":\"EAX\",\"value\":\"10.0\"},{\"tag\":\"ANS\",\"value\":\"3.0\"},"
         "{\"tag\":\"AXS\",\"value\":\"15.0\"}]}}";
}
static std::string plikPor(const PanelN& s) { return s.p.pliki.tresc("/dane/porownanie.csv"); }
static std::vector<std::string> wpisyPliku(const PanelN& s) {
  std::vector<std::string> w;
  for (auto& l : dziel(plikPor(s), '\n'))
    if (!l.empty() && l[0] != '#' && l.compare(0, 5, "data;") != 0) w.push_back(l);
  return w;
}
static std::string noce(const PanelN& s) { return s.p.pliki.tresc("/dane/noce/2026.csv"); }
// historia po wybudzeniu w trybie `json` z tymi samymi odpowiedziami (wzorzec: porównanie nie zmienia nocy)
static std::string historiaJson(const DobaAB& d, const std::string& rap) {
  PanelN s("noc_zrodlo=json\n");
  odpowiedzi(s, d);
  s.p.siec.odpowiedzi[sciezkaNocy(d.dzien)].clear();
  s.p.siec.dodaj(sciezkaNocy(d.dzien), 200, rap);
  wybudz(s, ep(2026, 11, 4, 7, 0));
  return noce(s);
}

static void testDyrygentPorownanie() {
  DobaAB d = dobaAB(DZIS4);
  // `noc_zrodlo=porownanie` (do 10.11.2026 tryb domyślny): noc z raportu JSON (300 min, bez PS), pliki pobrane raz,
  // wpis `rozjazd`
  {
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, d);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, sciezkaNocy(DZIS4)), 1);
    ROWNE_L(zadan(s, "/api/sessions?since=20261103"), 1);
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 3);
    ROWNE_L(zadan(s, sciezkaPobraniaStr()), 1);
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    SPRAWDZ(w.size() > 13 && w[2] == "300" && w[10].empty() && w[13] == "A");  // z raportu JSON
    ROWNE(noce(s), historiaJson(d, raport(DZIS4)));  // historia bajt w bajt jak przy `json`
    std::vector<std::string> p = wpisyPliku(s);
    ROWNE_L(p.size(), 1);
    SPRAWDZ(p.size() == 1 && p[0].compare(0, 19, "2026-11-04;rozjazd;") == 0 && p[0].find("m01=480.00~300.00") != std::string::npos);
    SPRAWDZ(plikPor(s).compare(0, 24, "#cpap-eink porownanie 1\n") == 0);
    SPRAWDZ(wLogu(s, "porównanie nocy 2026-11-04 z plików z raportem JSON: rozjazd"));
    SPRAWDZ(!wLogu(s, "E-03"));
    // drugie wybudzenie (R-DANE-24, 11:00, noc pobrana ponownie): porównanie już jest, bez pobrań plików
    s.p.siec.zadania.clear();
    odpowiedzi(s, d);
    s.p.zegar.epoch = ep(2026, 11, 4, 11, 5);
    s.p.zegar.msTeraz = 0;
    s.p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    s.p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    s.p.port.log.clear();
    Dyrygent(s.p.adaptery(), KONF).wybudzenie();
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 0);
    ROWNE_L(zadan(s, "/api/sessions?since="), 0);
    ROWNE_L(wpisyPliku(s).size(), 1);
  }
  // `noc_zrodlo=porownanie` z raportem zgodnym: wpis `zgodna`, OA/CA pominięte, PS tylko w plikach
  {
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, d);
    s.p.siec.odpowiedzi[sciezkaNocy(DZIS4)].clear();
    s.p.siec.dodaj(sciezkaNocy(DZIS4), 200, raportZgodny(DZIS4));
    wybudz(s, ep(2026, 11, 4, 7, 0));
    std::vector<std::string> p = wpisyPliku(s);
    if (p.size() != 1 || p[0].compare(0, 18, "2026-11-04;zgodna;") != 0)
      for (auto& l : s.p.port.log)
        if (l.find("porównanie") != std::string::npos) printf("    log: %s\n", l.c_str());
    SPRAWDZ(p.size() == 1 && p[0] == "2026-11-04;zgodna;;m03oa:aparat|m03ca:aparat|m28:pliki|m28p:pliki;");
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    SPRAWDZ(w.size() > 10 && w[2] == "480" && w[10].empty());  // M-01 z raportu, PS z raportu (brak), nie z plików
    ROWNE(noce(s), historiaJson(d, raportZgodny(DZIS4)));
  }
}

static std::vector<std::string> wierszZ(const std::string& plik, const std::string& data) {
  for (auto& l : dziel(plik, '\n'))
    if (l.compare(0, data.size(), data) == 0) return dziel(l, ';');
  return {};
}
// każda przyczyna braku porównania: noc z raportu JSON (300 min), wpis `brak` z przyczyną, bez E-03
static void testDyrygentBrak() {
  DobaAB d = dobaAB(DZIS4);
  std::string wzor = historiaJson(d, raport(DZIS4));
  auto sprawdz = [&](PanelN& s, const std::string& przyczyna, int linia) {
    wybudz(s, ep(2026, 11, 4, 7, 0));
    std::vector<std::string> p = wpisyPliku(s);
    // noc 4.11 jak przy `json`. Porównujemy tylko wiersz 4.11: atrapa tego pliku ma `/api/sessions?order=desc` na 404,
    // więc przy `json` panel nie pobiera STR i nie ma nocy 'S', a porównanie pobiera STR do ustawień doby i
    // strDoNocy() daje noce 'S' innych dat (ADR-0035). Prawdziwy AirBridge v2.3.1 podaje etag STR i wtedy obie
    // ścieżki mają te same noce 'S' (test/testy_porownanie_brzegowe.cpp, atrapa jak v2.3.1). Pole `import` wyzerowane
    // tylko z ostrożności: od przeniesienia porównania na koniec wybudzenia jest takie jak przy `json`.
    auto bezImportu = [](std::vector<std::string> w) {
      if (w.size() > 20) w[20].clear();
      return w;
    };
    bool dobrze = p.size() == 1 && p[0] == "2026-11-04;brak;;;" + przyczyna &&
                  bezImportu(wiersz(s, "2026-11-04")) == bezImportu(wierszZ(wzor, "2026-11-04")) && !wLogu(s, "E-03");
    if (!dobrze) printf("  (linia %d) wpis: %s\n", linia, p.empty() ? "–" : p[0].c_str());
    SPRAWDZ(dobrze);
  };
  for (int kod : {404, 503}) {
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, d, kod);
    sprawdz(s, kod == 404 ? "AirBridge < 2.3.1" : "błąd listy sesji", __LINE__);
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 0);
  }
  for (int war = 0; war < 2; war++) {  // sesja niezamknięta albo `.part`
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, d, 200, sesjeJson(d, war == 1, war == 1));
    sprawdz(s, "sesja niezamknięta", __LINE__);
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 0);
  }
  {
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, d, 200, "{\"sessions\":[],\"str\":null,\"next_cursor\":null}");
    sprawdz(s, "brak plików", __LINE__);
  }
  std::string pldN;
  for (auto& x : d.pliki)
    if (rodzajPliku(x.first) == Rodzaj::PLD) pldN = x.first;
  for (int war = 0; war < 3; war++) {  // PLD 404, ucięty, ponad limit
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, d);
    std::string sc = sciezkaPobraniaPliku(d.katalog + pldN);
    s.p.siec.odpowiedzi[sc].clear();
    if (war == 0) s.p.siec.dodaj(sc, 404, "{\"error\":\"not_found\"}");
    if (war == 1) s.p.siec.dodaj(sc, 200, d.pliki[pldN].substr(0, 1000));
    if (war == 2) s.p.siec.dodaj(sc, 200, std::string(AB_MAKS_BAJTOW_PLIKU_SESJI + 1, 'x'));
    sprawdz(s, war == 1 ? "plik nieczytelny" : "plik niepobrany", __LINE__);
  }
  {
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, d);
    s.p.siec.odpowiedzi[sciezkaPobraniaStr()].clear();
    s.p.siec.dodaj(sciezkaPobraniaStr(), 500, "");
    sprawdz(s, "brak STR.edf", __LINE__);
  }
  {
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, d);
    s.p.siec.odpowiedzi[sciezkaPobraniaStr()].clear();
    s.p.siec.dodaj(sciezkaPobraniaStr(), 200, strDoby(DZIS4 - 1));
    sprawdz(s, "STR bez tej doby", __LINE__);
  }
  {
    DobaAB e = d;
    for (auto it = e.pliki.begin(); it != e.pliki.end();)
      it = rodzajPliku(it->first) == Rodzaj::EVE ? e.pliki.erase(it) : std::next(it);
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, e);
    sprawdz(s, "noc z plików częściowa", __LINE__);
  }
  {  // raport JSON odpowiada po 80 s: na porównanie zostaje mniej niż CZAS_POROWNANIA_MS
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, d);
    s.p.siec.odpowiedzi[sciezkaNocy(DZIS4)].clear();
    s.p.siec.dodaj(sciezkaNocy(DZIS4), 200, raport(DZIS4), 80000);
    sprawdz(s, "brak czasu", __LINE__);
    ROWNE_L(zadan(s, "/api/sessions?since="), 0);
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 0);
  }
  // brak porównania nie blokuje następnej próby: po 404 w następnym wybudzeniu z pobraniem doby lista znowu
  {
    PanelN s("noc_zrodlo=porownanie\n");
    odpowiedzi(s, d, 404);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    SPRAWDZ(!gotowe(parsujWpisy(wpisyPliku(s)), "2026-11-04"));
  }
}

static void testDyrygentTryby() {
  DobaAB d = dobaAB(DZIS4);
  // `json`: nic z plików, bez wpisu
  {
    PanelN s("noc_zrodlo=json\n");
    odpowiedzi(s, d);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, "/api/sessions?since="), 0);
    ROWNE_L(zadan(s, "/api/storage/download?path="), 0);
    ROWNE(plikPor(s), "");
  }
  // `plik`: noc z plików jak w etapie 2, bez raportu JSON i bez wpisu porównania
  {
    PanelN s("noc_zrodlo=plik\n");
    odpowiedzi(s, d);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, sciezkaNocy(DZIS4)), 0);
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    SPRAWDZ(w.size() > 10 && w[2] == "480" && w[10] == "7.00");
    ROWNE(plikPor(s), "");
  }
  // zła wartość klucza: wiersz pominięty, tryb domyślny (`plik` od 11.11.2026): noc z plików, bez raportu
  // JSON i bez wpisu porównania
  {
    PanelN s("noc_zrodlo=pliki\n");
    odpowiedzi(s, d);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    SPRAWDZ(wLogu(s, "pominięty wiersz"));
    ROWNE_L(zadan(s, sciezkaNocy(DZIS4)), 0);
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 3);
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    SPRAWDZ(w.size() > 13 && w[2] == "480" && w[10] == "7.00" && w[13] == "A");
    ROWNE(plikPor(s), "");
    SPRAWDZ(wLogu(s, "konfiguracja: noc z plików sesji (domyślnie, ADR-0036)"));
  }
  // `porównanie` z polskimi znakami też
  {
    UstawieniaDnia u = parsujUstawieniaDnia("noc_zrodlo=porównanie\n");
    ROWNE_L(u.nocZrodlo, 2);
    ROWNE_L(parsujUstawieniaDnia("").nocZrodlo, -1);
    ROWNE_L(parsujUstawieniaDnia("").nocZrodloSkuteczne(), 1);  // bez wiersza: `plik`
    ROWNE_L(parsujUstawieniaDnia("noc_zrodlo=edf\n").nocZrodloSkuteczne(), 1);
    ROWNE_L(u.nocZrodloSkuteczne(), 2);
    ROWNE_L(parsujUstawieniaDnia("noc_zrodlo=json\n").nocZrodloSkuteczne(), 0);
    ROWNE_L(NOC_ZRODLO_DOMYSLNE, 1);
  }
  // porównanie zapisane wcześniej (zgodne) = bez pobrań plików; plik bez zmian
  {
    PanelN s("noc_zrodlo=porownanie\n");
    std::string t = "#cpap-eink porownanie 1\n" + std::string(POLA) + "\n2026-11-04;zgodna;;;\n#koniec 1\n";
    s.p.pliki.pliki["/dane/porownanie.csv"] = t;
    odpowiedzi(s, d);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, "/api/sessions?since="), 0);
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 0);
    ROWNE(plikPor(s), t);
  }
}

// Porównanie na samym końcu wybudzenia: zdarzenia do pierwszego odświeżenia ekranu (żądania, Wi-Fi, zapisy) takie
// jak przy `json`; pobrania plików dopiero po nim; „brak czasu” ponawiany w kolejnym wybudzeniu (raport JSON ponownie)
static void testDyrygentKoniecWybudzenia() {
  DobaAB d = dobaAB(DZIS4);
  PanelN j("noc_zrodlo=json\n"), p("noc_zrodlo=porownanie\n");
  odpowiedzi(j, d);
  odpowiedzi(p, d);
  wybudz(j, ep(2026, 11, 4, 7, 0));
  wybudz(p, ep(2026, 11, 4, 7, 0));
  // zdarzenia atrapy bez wierszy logu (`port …`: wiersz konfiguracji `noc_zrodlo=json` jest tylko przy `json`)
  auto bezLogu = [](const PanelN& x) {
    std::vector<std::string> v;
    for (auto& z : x.p.d.z)
      if (z.compare(0, 5, "port ") != 0) v.push_back(z);
    return v;
  };
  std::vector<std::string> zj = bezLogu(j), zp = bezLogu(p);
  auto pierwsze = [](const std::vector<std::string>& v, const std::string& t) {
    for (size_t i = 0; i < v.size(); i++)
      if (v[i].compare(0, t.size(), t) == 0) return (int)i;
    return -1;
  };
  int ej = pierwsze(zj, "ekran.odswiez"), ep_ = pierwsze(zp, "ekran.odswiez");
  SPRAWDZ(ej >= 0 && ej == ep_);
  bool rowne = ej == ep_ && std::equal(zj.begin(), zj.begin() + std::max(ej, 0), zp.begin());
  SPRAWDZ(rowne);
  for (int i = 0; !rowne && i < ej && i < ep_; i++)
    if (zj[(size_t)i] != zp[(size_t)i]) {
      printf("    zdarzenie %d: json '%s', porównanie '%s'\n", i, zj[(size_t)i].c_str(), zp[(size_t)i].c_str());
      break;
    }
  int plik = pierwsze(zp, "siec.get http://192.168.1.20/api/storage/download?path=/DATALOG/");
  SPRAWDZ(plik > ep_);
  ROWNE(noce(p), noce(j));  // także pole `import` (chwila zapisu) jak przy `json`
  // brak czasu (raport po 80 s): wpis `brak czasu`, bez Wi-Fi na końcu; następne wybudzenie zegarem z odczytem:
  // raport JSON ponownie (pamięć wybudzenia go nie ma), pliki, wynik
  PanelN s("noc_zrodlo=porownanie\n");
  odpowiedzi(s, d);
  s.p.siec.odpowiedzi[sciezkaNocy(DZIS4)].clear();
  s.p.siec.dodaj(sciezkaNocy(DZIS4), 200, raport(DZIS4), 80000);
  wybudz(s, ep(2026, 11, 4, 7, 0));
  std::vector<std::string> w = wpisyPliku(s);
  SPRAWDZ(w.size() == 1 && w[0] == "2026-11-04;brak;;;brak czasu");
  ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 0);
  odpowiedzi(s, d);
  s.p.siec.zadania.clear();
  s.p.zegar.epoch = ep(2026, 11, 4, 9, 0) + 5;  // slot dzienny: odczyt `/api/status` z Wi-Fi
  s.p.zegar.msTeraz = 0;
  s.p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  s.p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  s.p.port.log.clear();
  Dyrygent(s.p.adaptery(), KONF).wybudzenie();
  w = wpisyPliku(s);
  SPRAWDZ(w.size() == 1 && w[0].compare(0, 19, "2026-11-04;rozjazd;") == 0);
  ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 3);
  // trzecie wybudzenie: porównanie zrobione, bez pobrań plików
  s.p.siec.zadania.clear();
  s.p.zegar.epoch = ep(2026, 11, 4, 10, 0) + 5;
  s.p.zegar.msTeraz = 0;
  Dyrygent(s.p.adaptery(), KONF).wybudzenie();
  ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 0);
}

// U-05: wiersz `Noce z plików` z pliku porównań (7 zgodnych = dopisek), diagnostyka w logu
static void testDyrygentU05() {
  PanelN s("noc_zrodlo=porownanie\n");
  std::string t = "#cpap-eink porownanie 1\n" + std::string(POLA) + "\n";
  for (int i = 0; i < 7; i++) t += dataZNr(nrDnia("2026-10-27") + i) + ";zgodna;;m28:pliki;\n";
  t += "#koniec 7\n";
  s.p.pliki.pliki["/dane/porownanie.csv"] = t;
  s.p.siec.siecJest = false;  // bez sieci: tylko odczyt pliku
  s.p.zegar.epoch = ep(2026, 11, 3, 12, 0);
  s.p.zegar.msTeraz = 0;
  s.p.start.stan = StanRtc();
  s.p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  s.p.start.powodWybudzenia = PowodWybudzenia::START;
  Dyrygent dy(s.p.adaptery(), KONF);
  dy.wybudzenie();
  TekstyWybudzenia tk = dy.teksty();
  bool jest = false;
  for (auto& l : tk.linieTestu)
    if (l.rfind("Noce z plików: ", 0) == 0) {
      jest = true;
      printf("  U-05: \"%s\"\n", l.c_str());
      ROWNE(l, "Noce z plików: zgodne 7 z 7, można przełączyć na pliki");
    }
  SPRAWDZ(jest);
  if (!jest) {
    printf("  linie testu: %d\n", (int)tk.linieTestu.size());
    for (size_t i = 0; i < s.p.port.log.size() && i < 40; i++) printf("    %s\n", s.p.port.log[i].c_str());
  }
  SPRAWDZ(zawiera(tk.diagnostykaTestu, "Noce z plików: noc_zrodlo=porownanie"));
  ROWNE(s.p.pliki.tresc("/dane/porownanie.csv"), t);  // odczyt nie przepisuje pliku
}

// ============================================================================================================
// (e) tryb domyślny `plik` (ADR-0036 „Zmiana 2026-11-11”)
// ============================================================================================================
static void testDyrygentDomyslnyPlik() {
  DobaAB d = dobaAB(DZIS4);
  // bez pliku konfiguracji i z plikiem bez wiersza `noc_zrodlo`: noc z plików sesji, PS (M-28) w historii, bez raportu
  // JSON i bez wpisu porównania
  for (const char* k : {"", "profil=prywatny\n"}) {
    PanelN s(k);
    odpowiedzi(s, d);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, sciezkaNocy(DZIS4)), 0);
    ROWNE_L(zadan(s, "/api/sessions?since=20261103"), 1);
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 3);
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    SPRAWDZ(w.size() >= 42);
    if (w.size() >= 42) {
      ROWNE(w[1], "1");
      ROWNE(w[2], "480");                 // M-01 z plików (raport JSON miał 300)
      ROWNE(w[10], "7.00");               // PS (M-28) z plików: 14 − 7
      ROWNE(w[12], "ASVAuto/6/10/3/15");  // ustawienia ze STR doby
      ROWNE(w[13], "A");
    }
    ROWNE(plikPor(s), "");
    SPRAWDZ(!wLogu(s, "E-03"));
    SPRAWDZ(*k == 0 || wLogu(s, "konfiguracja: noc z plików sesji (domyślnie, ADR-0036)"));
  }
  // problem z plikami (R-DANE-44): ta doba z raportu JSON w tym samym wybudzeniu (300 min, bez PS), bez E-03 i bez
  // wpisu porównania; 404 (AirBridge < 2.3.1), 503, noc z plików częściowa (bez EVE)
  auto zJson = [](PanelN& s) {
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    return w.size() > 13 && w[2] == "300" && w[10].empty() && w[13] == "A";
  };
  for (int kod : {404, 503}) {
    PanelN s("");
    odpowiedzi(s, d, kod);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, "/api/sessions?since="), 1);
    ROWNE_L(zadan(s, sciezkaNocy(DZIS4)), 1);
    ROWNE_L(zadan(s, "/api/storage/download?path=/DATALOG/"), 0);
    SPRAWDZ(zJson(s));
    ROWNE(plikPor(s), "");
    SPRAWDZ(!wLogu(s, "E-03"));
  }
  {
    DobaAB e = d;
    for (auto it = e.pliki.begin(); it != e.pliki.end();)
      it = rodzajPliku(it->first) == Rodzaj::EVE ? e.pliki.erase(it) : std::next(it);
    PanelN s("");
    odpowiedzi(s, e);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, sciezkaNocy(DZIS4)), 1);
    SPRAWDZ(zJson(s) && wLogu(s, "częściowa"));
    ROWNE(plikPor(s), "");
  }
  // noc zapisana wcześniej z raportu JSON (bez PS, np. w trybie `porownanie` przed zmianą) zostaje bajt w bajt:
  // bez przeliczania wstecz, bez żądań o tę dobę; nowa doba już z plików
  {
    Noc n;
    n.data = "2026-11-03";
    n.zrodlo = 'A';
    n.m01 = 300;
    n.ahi = 3;
    n.wazna = true;
    // plik wersji 4: po migracji 4 → 5 wiersz z pustym `m15` na końcu, reszta bajt w bajt
    std::string stara = wierszNocyV5(n);
    PanelN s("");
    s.p.pliki.pliki["/dane/noce/2026.csv"] =
        std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n" + wierszNocyV3(n) + "\n#koniec 1\n";
    odpowiedzi(s, d);
    s.p.siec.odpowiedzi[sciezkaListy()].clear();
    s.p.siec.dodaj(sciezkaListy(), 200, lista(DZIS4 - 1, DZIS4));
    wybudz(s, ep(2026, 11, 4, 7, 0));
    ROWNE_L(zadan(s, sciezkaNocy(DZIS4 - 1)), 0);
    ROWNE_L(zadan(s, "/api/sessions?since=" + dzienAB(DZIS4 - 1)), 0);
    bool jest = false;
    for (auto& l : dziel(noce(s), '\n')) jest = jest || l == stara;
    SPRAWDZ(jest);
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    SPRAWDZ(w.size() > 13 && w[2] == "480" && w[10] == "7.00");
  }
  // U-05 w trybie domyślnym: `włączone (plik)`, diagnostyka z trybem `plik`
  {
    PanelN s("");
    s.p.siec.siecJest = false;
    s.p.zegar.epoch = ep(2026, 11, 3, 12, 0);
    s.p.zegar.msTeraz = 0;
    s.p.start.stan = StanRtc();
    s.p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    s.p.start.powodWybudzenia = PowodWybudzenia::START;
    Dyrygent dy(s.p.adaptery(), KONF);
    dy.wybudzenie();
    TekstyWybudzenia tk = dy.teksty();
    SPRAWDZ(zawiera(tk.linieTestu, "Noce z plików: włączone (plik)"));
    SPRAWDZ(zawiera(tk.diagnostykaTestu, "Noce z plików: noc_zrodlo=plik"));
  }
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testRdzen();
  testWpisy();
  testU05();
  testDyrygentPorownanie();
  testDyrygentDomyslnyPlik();
  testDyrygentBrak();
  testDyrygentTryby();
  testDyrygentKoniecWybudzenia();
  testDyrygentU05();
  printf("testy_porownanie: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
