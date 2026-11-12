// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe trybu porównawczego (`noc_zrodlo=porownanie`, ADR-0036 „Zmiana 2026-11-09”, R-DANE-60). Od
// ADR-0036 „Zmiana 2026-11-11” tryb domyślny to `plik`, więc każdy przebieg porównawczy ustawia klucz jawnie.
// Każde sprawdzenie podaje w komunikacie nazwę przypadku; „regresja” = sprawdzenie, które zostaje jako test regresji.
// Dane wyłącznie syntetyczne.
//
// Atrapa AirBridge jak w prawdziwym v2.3.1: `/api/sessions?order=desc` działa (etag STR), więc przy `json` panel
// też pobiera STR (ADR-0035). Testy z test/testy_porownanie.cpp mają tę ścieżkę na 404, więc przy `json` nie ma nocy
// 'S', a przy porównaniu są (stąd porównanie tylko wiersza 4.11 w ich testDyrygentBrak).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define main main_testy_noc_pliki
#include "testy_noc_pliki.cpp"
#undef main

#include "ekrany.h"
#include "porownanie.h"

using namespace porownanie;

// pomocnicze skopiowane z test/testy_porownanie.cpp (dwa main() nie dają się dołączyć razem)
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

#pragma GCC diagnostic pop

#include <functional>

#define PRZYPADEK(nr, w)                                                    \
  do {                                                                      \
    if (w) {                                                                \
      ok++;                                                                 \
    } else {                                                                \
      zle++;                                                                \
      printf("  BŁĄD %s (linia %d): %s\n", nr, __LINE__, #w);              \
    }                                                                       \
  } while (0)

// STR z dobami od..do (ASVAuto 6–10, PS 3–15, 480 min, AHI 2)
static std::string strDob(long od, long doDnia) {
  std::vector<SygT> syg = {{"Date", "", 0, 32767, 0, 32767, 1},           {"Duration", "min.", 0, 1440, 0, 1440, 1},
                           {"Mode", "", 0, 16, 0, 16, 1},                 {"S.AA.MinEPAP", "cmH2O", 1, 30, 50, 1500, 1},
                           {"S.AA.MaxEPAP", "cmH2O", 1, 30, 50, 1500, 1}, {"S.AA.MinPS", "cmH2O", 0, 30, 0, 1500, 1},
                           {"S.AA.MaxPS", "cmH2O", 0, 30, 0, 1500, 1},    {"AHI", "", 0, 240, 0, 2400, 1},
                           CRC};
  std::vector<std::vector<std::string>> rek;
  for (long d = od; d <= doDnia; d++) {
    std::vector<double> v = {(double)d, 480, 8, 6, 10, 3, 15, 2};
    std::vector<std::string> r;
    for (size_t i = 0; i < v.size(); i++) r.push_back(probki({cyf(syg[i], v[i])}));
    r.push_back(probki({0}));
    rek.push_back(r);
  }
  return budujEdf((long long)od * 86400 + 12 * 3600, 86400, syg, rek);
}
static std::string zEtagiem(std::string sesje, const std::string& etag) {
  size_t p = sesje.find("\"str\":null");
  if (p != std::string::npos) sesje.replace(p, 10, "\"str\":{\"etag\":\"" + etag + "\",\"size\":100}");
  return sesje;
}

// Atrapa „prawdziwa”: doby `doby` (najnowsza ostatnia), każda z raportem `rap(day)` i plikami, lista sesji z etagiem
// STR, `/api/sessions?order=desc` 200 z tym samym etagiem, STR z dobami `strOd`..`strDo`.
struct Atrapa {
  std::vector<DobaAB> doby;
  std::function<std::string(long)> rap = raport;
  uint32_t opPliku = 100, opListy = 100, opRaportu = 100;
  long strOd = 0, strDo = 0;
  std::string etag = "E1";
};
static void ustaw(PanelN& s, const Atrapa& a) {
  s.p.siec.odpowiedzi.clear();
  s.p.siec.dodaj("/api/status", 200, STATUS);
  s.p.siec.dodaj(sciezkaListy(), 200, lista(a.doby.front().dzien, a.doby.back().dzien));
  s.p.siec.dodaj("/api/sessions?order=desc", 200,
                 "{\"sessions\":[],\"str\":{\"etag\":\"" + a.etag + "\",\"size\":100},\"next_cursor\":null}");
  long od = a.strOd ? a.strOd : a.doby.front().dzien, dd = a.strDo ? a.strDo : a.doby.back().dzien;
  s.p.siec.dodaj(sciezkaPobraniaStr(), 200, strDob(od, dd), a.opPliku);
  for (auto& d : a.doby) {
    s.p.siec.dodaj(sciezkaNocy(d.dzien), 200, a.rap(d.dzien), a.opRaportu);
    s.p.siec.dodaj("/api/sessions?since=" + dzienAB(d.dzien), 200, zEtagiem(sesjeJson(d), a.etag), a.opListy);
    for (auto& x : d.pliki) s.p.siec.dodaj(sciezkaPobraniaPliku(d.katalog + x.first), 200, x.second, a.opPliku);
  }
}
// Stan panelu, który widzi pacjent albo który wpływa na to, co zobaczy: pliki poza /dane/porownanie.csv i logami,
// NVS poza znacznikami czasu, ostatnia klatka ekranu.
struct Migawka {
  std::map<std::string, std::string> pliki;
  std::map<std::string, std::string> nvs;
  std::vector<uint8_t> ekran;
  int nocy = 0, nocyS = 0;
};
static Migawka migawka(const PanelN& s) {
  Migawka m;
  for (auto& kv : s.p.pliki.pliki)
    // `/dane/portret.csv`: przebieg nocy z plików zapisuje także tryb porównawczy, na końcu wybudzenia (R-POR-17)
    if (kv.first != "/dane/porownanie.csv" && kv.first != "/dane/portret.csv" && kv.first != "/konfiguracja.txt" &&
        kv.first.compare(0, 5, "/log/") != 0 &&
        kv.first.find("wybudzenia") == std::string::npos)
      m.pliki[kv.first] = kv.second;
  for (auto& kv : s.p.nvs.teksty) m.nvs["t:" + kv.first] = kv.second;
  for (auto& kv : s.p.nvs.liczby) m.nvs["l:" + kv.first] = std::to_string(kv.second);
  for (auto& kv : s.p.nvs.flagi) m.nvs["f:" + kv.first] = kv.second ? "1" : "0";
  if (!s.p.ekran.klatki.empty()) m.ekran = s.p.ekran.klatki.back();
  for (auto& l : dziel(s.p.pliki.tresc("/dane/noce/2026.csv"), '\n'))
    if (l.size() > 10 && l[4] == '-' && l[0] == '2') {
      m.nocy++;
      std::vector<std::string> w = dziel(l, ';');
      if (w.size() > 13 && w[13] == "S") m.nocyS++;
    }
  return m;
}
// pole `import` (chwila zapisu) wyzerowane: różnica tylko w nim = opóźnienie zapisu, nie inne dane
static std::string bezImportu(const std::string& plik) {
  std::string o;
  for (auto& l : dziel(plik, '\n')) {
    std::vector<std::string> w = dziel(l, ';');
    if (w.size() > 20 && l[0] == '2') {
      w[20] = "";
      std::string x;
      for (size_t i = 0; i < w.size(); i++) x += (i ? ";" : "") + w[i];
      o += x + "\n";
    } else {
      o += l + "\n";
    }
  }
  return o;
}
static void roznice(const Migawka& a, const Migawka& b) {
  for (auto& kv : a.pliki) {
    auto it = b.pliki.find(kv.first);
    if (it == b.pliki.end()) {
      printf("    tylko json: %s\n", kv.first.c_str());
    } else if (it->second != kv.second) {
      printf("    inny plik: %s\n", kv.first.c_str());
      std::vector<std::string> x = dziel(kv.second, '\n'), y = dziel(it->second, '\n');
      for (size_t i = 0; i < x.size() && i < y.size(); i++)
        if (x[i] != y[i]) printf("      json:       %.200s\n      porównanie: %.200s\n", x[i].c_str(), y[i].c_str());
    }
  }
  for (auto& kv : b.pliki)
    if (!a.pliki.count(kv.first)) printf("    tylko porównanie: %s\n", kv.first.c_str());
  for (auto& kv : a.nvs) {
    auto it = b.nvs.find(kv.first);
    if (it == b.nvs.end() || it->second != kv.second)
      printf("    NVS %s: json '%s', porównanie '%s'\n", kv.first.c_str(), kv.second.c_str(),
             it == b.nvs.end() ? "–" : it->second.c_str());
  }
  for (auto& kv : b.nvs)
    if (!a.nvs.count(kv.first)) printf("    NVS tylko porównanie %s='%s'\n", kv.first.c_str(), kv.second.c_str());
  if (a.ekran != b.ekran) printf("    inna ostatnia klatka ekranu\n");
  printf("    nocy json %d (S %d), porównanie %d (S %d)\n", a.nocy, a.nocyS, b.nocy, b.nocyS);
}
using Przygotuj = std::function<void(PanelN&)>;
static PanelN* przebieg(const std::string& konf, const Przygotuj& f, uint32_t t, PanelN& s) {
  s.p.pliki.pliki["/konfiguracja.txt"] = konf;
  f(s);
  wybudz(s, t);
  return &s;
}

// ============================================================================================================
// punkt 1: nienaruszalność historii, wiele scenariuszy, cały przebieg dyrygenta w obu trybach
// ============================================================================================================
static void scenariusz(const char* nazwa, const Przygotuj& f, uint32_t t = ep(2026, 11, 4, 7, 0),
                       const char* nr = "historia", bool scisle = true) {
  PanelN j("noc_zrodlo=json\n"), p("noc_zrodlo=porownanie\n");
  przebieg("noc_zrodlo=json\n", f, t, j);
  przebieg("noc_zrodlo=porownanie\n", f, t, p);
  Migawka a = migawka(j), b = migawka(p);
  bool rowneHist = bezImportu(a.pliki["/dane/noce/2026.csv"]) == bezImportu(b.pliki["/dane/noce/2026.csv"]);
  bool rowneWszystko = a.pliki == b.pliki && a.ekran == b.ekran;
  std::vector<std::string> wp = wpisyPliku(p);
  printf("  scenariusz %-34s %6u/%6u ms wpis: %-45s historia %s, pliki+ekran %s\n", nazwa, j.p.zegar.msTeraz,
         p.p.zegar.msTeraz,
         wp.empty() ? "–" : wp.back().substr(0, 45).c_str(), rowneHist ? "=" : "RÓŻNA",
         rowneWszystko ? "=" : "różne");
  if (!rowneHist || (!rowneWszystko && scisle)) roznice(a, b);
  PRZYPADEK(nr, rowneHist);
  if (scisle) PRZYPADEK(nr, rowneWszystko);
  if (a.nvs != b.nvs) {
    printf("    NVS różne:\n");
    roznice(a, b);
  }
}

static void testNienaruszalnosc() {
  printf("punkt 1: historia, pliki i ekran przy `porownanie` wobec `json` (atrapa v2.3.1 z etagiem STR)\n");
  DobaAB d = dobaAB(DZIS4);
  Atrapa a;
  a.doby = {d};
  scenariusz("zgodna", [&](PanelN& s) {
    Atrapa x = a;
    x.rap = raportZgodny;
    ustaw(s, x);
  });
  scenariusz("rozjazd", [&](PanelN& s) { ustaw(s, a); });
  scenariusz("404 (stary AirBridge)", [&](PanelN& s) {
    ustaw(s, a);
    s.p.siec.odpowiedzi["/api/sessions?order=desc"].clear();
    s.p.siec.dodaj("/api/sessions?order=desc", 404, "");
    s.p.siec.odpowiedzi["/api/sessions?since=" + dzienAB(DZIS4)].clear();
    s.p.siec.dodaj("/api/sessions?since=" + dzienAB(DZIS4), 404, "");
  });
  scenariusz("503 listy sesji doby", [&](PanelN& s) {
    ustaw(s, a);
    s.p.siec.odpowiedzi["/api/sessions?since=" + dzienAB(DZIS4)].clear();
    s.p.siec.dodaj("/api/sessions?since=" + dzienAB(DZIS4), 503, "{}");
  });
  scenariusz("PLD ucięty", [&](PanelN& s) {
    ustaw(s, a);
    for (auto& x : d.pliki)
      if (rodzajPliku(x.first) == Rodzaj::PLD) {
        std::string sc = sciezkaPobraniaPliku(d.katalog + x.first);
        s.p.siec.odpowiedzi[sc].clear();
        s.p.siec.dodaj(sc, 200, x.second.substr(0, 1000));
      }
  });
  scenariusz("brak czasu (raport po 80 s)", [&](PanelN& s) {
    Atrapa x = a;
    x.opRaportu = 80000;
    ustaw(s, x);
  });
  scenariusz("STR bez tej doby (3 starsze doby)", [&](PanelN& s) {
    Atrapa x = a;
    x.strOd = DZIS4 - 3;
    x.strDo = DZIS4 - 1;
    ustaw(s, x);
  });
  scenariusz("noc z plików częściowa (bez EVE)", [&](PanelN& s) {
    DobaAB e = d;
    for (auto it = e.pliki.begin(); it != e.pliki.end();)
      it = rodzajPliku(it->first) == Rodzaj::EVE ? e.pliki.erase(it) : std::next(it);
    Atrapa x = a;
    x.doby = {e};
    ustaw(s, x);
  });
  scenariusz("raport JSON partial", [&](PanelN& s) {
    Atrapa x = a;
    x.rap = [](long day) {
      std::string r = raport(day);
      r.replace(r.find("\"partial\":false"), 15, "\"partial\":true");
      return r;
    };
    ustaw(s, x);
  });
  scenariusz("terapia w toku (rop 1)", [&](PanelN& s) {
    ustaw(s, a);
    std::string st = STATUS;
    st.replace(st.find("\"rop\":0"), 7, "\"rop\":1");
    s.p.siec.odpowiedzi["/api/status"].clear();
    s.p.siec.dodaj("/api/status", 200, st);
  });
  // wiele zaległych dób (6 dób, pusta historia): ostatnia noc + 4 zaległe po rysowaniu w obu trybach
  scenariusz("6 dób, szybki AirBridge", [&](PanelN& s) {
    Atrapa x = a;
    x.doby.clear();
    for (long k = DZIS4 - 5; k <= DZIS4; k++) x.doby.push_back(dobaAB(k));
    ustaw(s, x);
  });
  // przerwanie przyciskiem w trakcie porównania: przycisk wciśnięty od 3. sekundy wybudzenia na stałe
  scenariusz(
      "przycisk w trakcie porównania", [&](PanelN& s) {
        Atrapa x = a;
        x.opPliku = 3000;
        ustaw(s, x);
        s.p.przyciski.skrypt = {{9000, 1}};
      },
      ep(2026, 11, 4, 7, 0), "historia", true);
}

// ============================================================================================================
// punkt 2: budżet i kolejność
// ============================================================================================================
static void testBudzet() {
  printf("punkt 2: budżet wybudzenia i kolejność\n");
  // (a) zaległe doby przy wolnym AirBridge (plik 10 s, lista 2 s; mieści się w AB_LIMIT_ODPOWIEDZI_MS):
  // przy `json` jedno wybudzenie = ostatnia noc + 4 zaległe; porównanie nie może zabrać miejsca zaległym dobom
  for (uint32_t op : {3000u, 10000u, 14000u}) {
    Atrapa x;
    for (long k = DZIS4 - 5; k <= DZIS4; k++) x.doby.push_back(dobaAB(k));
    x.opPliku = op;
    x.opListy = 2000;
    PanelN j("noc_zrodlo=json\n"), p("noc_zrodlo=porownanie\n");
    przebieg("noc_zrodlo=json\n", [&](PanelN& s) { ustaw(s, x); }, ep(2026, 11, 4, 7, 0), j);
    przebieg("noc_zrodlo=porownanie\n", [&](PanelN& s) { ustaw(s, x); }, ep(2026, 11, 4, 7, 0), p);
    Migawka a = migawka(j), b = migawka(p);
    int raportyJ = zadan(j, "/api/report?source=edf&day="), raportyP = zadan(p, "/api/report?source=edf&day=");
    int plikiP = zadan(p, "/api/storage/download?path=/DATALOG/");
    printf("  STR.edf pobrany: json %d, porównanie %d\n", zadan(j, sciezkaPobraniaStr()), zadan(p, sciezkaPobraniaStr()));
    printf("  plik %5u ms: json %d nocy A+S (%d raportów, %u ms), porównanie %d nocy (%d raportów, %d plików, %u ms), "
           "wpisów %d\n",
           op, a.nocy, raportyJ, j.p.zegar.msTeraz, b.nocy, raportyP, plikiP, p.p.zegar.msTeraz,
           (int)wpisyPliku(p).size());
    PRZYPADEK("budżet", raportyP == raportyJ);
    PRZYPADEK("budżet", a.nocy == b.nocy);
  }
  // (b) kolejność: przy `json` ekran poranka rysowany zaraz po raporcie ostatniej nocy; porównanie (pobrania plików)
  // nie powinno stać przed pierwszym odświeżeniem ekranu (zaległe idą po rysowaniu, R-DANE-06)
  {
    Atrapa x;
    x.doby = {dobaAB(DZIS4)};
    x.opPliku = 4000;
    PanelN j("noc_zrodlo=json\n"), p("noc_zrodlo=porownanie\n");
    przebieg("noc_zrodlo=json\n", [&](PanelN& s) { ustaw(s, x); }, ep(2026, 11, 4, 7, 0), j);
    przebieg("noc_zrodlo=porownanie\n", [&](PanelN& s) { ustaw(s, x); }, ep(2026, 11, 4, 7, 0), p);
    int ekr = p.p.d.pierwsze("ekran.odswiez"), plik = p.p.d.pierwsze("siec.get http://192.168.1.20/api/storage/download?path=/DATALOG/");
    int ekrJ = j.p.d.pierwsze("ekran.odswiez");
    // ms do pierwszego odświeżenia: zdarzeń „siec.get” przed nim
    auto getyPrzed = [](const PanelN& s, int i) {
      int n = 0;
      for (int k = 0; k < i && k < (int)s.p.d.z.size(); k++) n += s.p.d.z[k].compare(0, 8, "siec.get") == 0;
      return n;
    };
    printf("  pierwsze odświeżenie ekranu: json po %d żądaniach, porównanie po %d (pierwsze pobranie pliku: zdarzenie %d, "
           "ekran: %d)\n",
           getyPrzed(j, ekrJ), getyPrzed(p, ekr), plik, ekr);
    PRZYPADEK("kolejność", ekr >= 0 && (plik < 0 || plik > ekr));
  }
  // (c) historia ze STR (ADR-0035) w głównym wybudzeniu: porównanie nie może jej wypchnąć z budżetu 120 s
  {
    Atrapa x;
    x.doby = {dobaAB(DZIS4)};
    x.strOd = DZIS4 - 9;
    x.strDo = DZIS4;
    x.opPliku = 14000;
    x.opListy = 14000;
    x.opRaportu = 14000;
    PanelN j("noc_zrodlo=json\n"), p("noc_zrodlo=porownanie\n");
    przebieg("noc_zrodlo=json\n", [&](PanelN& s) { ustaw(s, x); }, ep(2026, 11, 4, 7, 0), j);
    przebieg("noc_zrodlo=porownanie\n", [&](PanelN& s) { ustaw(s, x); }, ep(2026, 11, 4, 7, 0), p);
    Migawka a = migawka(j), b = migawka(p);
    printf("  wolny AirBridge (14 s na żądanie), STR 10 dób: json %d nocy S, porównanie %d nocy S; wpis %s\n", a.nocyS,
           b.nocyS, wpisyPliku(p).empty() ? "–" : wpisyPliku(p).back().c_str());
    PRZYPADEK("noce S", a.nocyS == b.nocyS);
  }
  // (d) wpis `brak` z przyczyną trwałą (404) przy kolejnych pobraniach tej samej doby (11:00, R-DANE-24): ile żądań
  {
    Atrapa x;
    x.doby = {dobaAB(DZIS4)};
    PanelN s("noc_zrodlo=porownanie\n");
    ustaw(s, x);
    s.p.siec.odpowiedzi["/api/sessions?since=" + dzienAB(DZIS4)].clear();
    s.p.siec.dodaj("/api/sessions?since=" + dzienAB(DZIS4), 200, sesjeJson(x.doby[0], false));  // niezamknięta
    wybudz(s, ep(2026, 11, 4, 7, 0));
    int listy1 = zadan(s, "/api/sessions?since=");
    // 11:05: ponowne pobranie ostatniej nocy, sesja już zamknięta → porównanie ponowione (przyczyna przejściowa)
    ustaw(s, x);
    s.p.siec.zadania.clear();
    s.p.zegar.epoch = ep(2026, 11, 4, 11, 5);
    s.p.zegar.msTeraz = 0;
    s.p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    s.p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    s.p.port.log.clear();
    Dyrygent(s.p.adaptery(), KONF).wybudzenie();
    std::vector<std::string> w = wpisyPliku(s);
    printf("  przejściowa przyczyna: 7:00 list %d, 11:05 list %d, wpis po 11:05: %s\n", listy1,
           zadan(s, "/api/sessions?since="), w.empty() ? "–" : w.back().c_str());
    PRZYPADEK("regresja", !w.empty() && w.back().compare(0, 18, "2026-11-04;rozjazd") == 0);
  }
}

// ============================================================================================================
// punkt 4: plik /dane/porownanie.csv
// ============================================================================================================
static void testPlik() {
  printf("punkt 4: /dane/porownanie.csv\n");
  Atrapa x;
  x.doby = {dobaAB(DZIS4)};
  // (a) uszkodzony plik (śmieci, ucięty bez stopki): panel działa, noc z JSON zapisana, porównanie dopisane
  for (const char* zly : {"\xff\xfe garbage;;;\n", "#cpap-eink porownanie 1\ndata;werdykt;rozjazd;pominiete;przyczyna\n"
                                                    "2026-11-03;zgodna;;;\n2026-11-02;zg",
                          "#cpap-eink porownanie 9\ndata;werdykt;rozjazd;pominiete;przyczyna\n#koniec 0\n"}) {
    PanelN s("noc_zrodlo=porownanie\n");
    s.p.pliki.pliki["/dane/porownanie.csv"] = zly;
    ustaw(s, x);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    std::vector<std::string> w = wiersz(s, "2026-11-04");
    std::string po = plikPor(s);
    bool nowsza = std::string(zly).find("porownanie 9") != std::string::npos;
    printf("  plik %-22.22s → pobrań plików %d, noc 4.11 %s, plik po: %.60s\n",
           nowsza ? "z wersji 9" : (zly[0] == '#' ? "ucięty" : "śmieci"), zadan(s, "/api/storage/download?path=/DATALOG/"),
           w.size() > 2 ? w[2].c_str() : "–", po.c_str());
    PRZYPADEK("regresja", w.size() > 2 && w[2] == "300");
    if (nowsza) {
      PRZYPADEK("regresja", po == zly);  // plik z nowszej wersji nie jest nadpisany
      // R-DANE-60 fail-path: plik z nowszej wersji = bez pobrań plików (wynik i tak nie byłby zapisany)
      PRZYPADEK("bez DATALOG", zadan(s, "/api/storage/download?path=/DATALOG/") == 0);
    }
  }
  // (b) 31. doba wypiera najstarszą; zapis z nagłówkiem i stopką
  {
    PanelN s("noc_zrodlo=porownanie\n");
    std::string t = "#cpap-eink porownanie 1\n" + std::string(POLA) + "\n";
    for (int i = 0; i < 30; i++) t += dataZNr(nrDnia("2026-10-04") + i) + ";zgodna;;;\n";
    t += "#koniec 30\n";
    s.p.pliki.pliki["/dane/porownanie.csv"] = t;
    ustaw(s, x);
    wybudz(s, ep(2026, 11, 4, 7, 0));
    std::vector<std::string> w = wpisyPliku(s);
    PRZYPADEK("regresja", w.size() == 30 && w.front().compare(0, 10, "2026-10-05") == 0 &&
                                w.back().compare(0, 10, "2026-11-04") == 0);
    PRZYPADEK("regresja", plikPor(s).find("#koniec 30") != std::string::npos);
  }
}

// ============================================================================================================
// punkt 5: werdykt
// ============================================================================================================
static void testWerdykt() {
  printf("punkt 5: werdykt porownaj()\n");
  NocAB r = nocWzorcowa(), p = nocWzorcowa();
  // OA: plik 0, raport 0,4 → w tolerancji; plik 0, raport 2 → NIE
  r.noc.oa = 2;
  p.noc.oa = 0;
  PRZYPADEK("regresja", ocena(porownaj(p, r), "m03oa") == Ocena::NIE);
  // OA: plik nieznane, raport nieznane → tak (nie rozjazd)
  r.noc.oa = NAN;
  p.noc.oa = NAN;
  PRZYPADEK("regresja", ocena(porownaj(p, r), "m03oa") == Ocena::TAK);
  // CA: plik nieznane, raport 0,3 (> 0, w tolerancji 0,5 od „zera”): spec pomija tylko przy 0 → rozjazd
  r = nocWzorcowa();
  p = nocWzorcowa();
  p.noc.ca = NAN;
  r.noc.ca = 0.3f;
  PRZYPADEK("regresja", ocena(porownaj(p, r), "m03ca") == Ocena::BRAK_W_PLIKACH);
  // PS: raport ma PS (gdyby AirBridge zaczął podawać), pliki nie → brak w plikach (rozjazd), nie „tylko pliki”
  r = nocWzorcowa();
  p = nocWzorcowa();
  r.noc.psmed = 7;
  PRZYPADEK("regresja", ocena(porownaj(p, r), "m28") == Ocena::BRAK_W_PLIKACH);
  // tolerancja tuż poza granicą float: M-05 14,0 wobec 14,5 zapisane jako float
  r = nocWzorcowa();
  p = nocWzorcowa();
  p.noc.cmed = 14.5f;
  PRZYPADEK("regresja", ocena(porownaj(p, r), "m05") == Ocena::TAK);
  p.noc.m10 = 0.07f;  // 0,05 → 0,07 (float): różnica 0,02 na granicy
  PRZYPADEK("regresja", ocena(porownaj(p, r), "m10") == Ocena::TAK);
  // licznik: 7 z 7 tylko z kolejnych dób z porównaniem; doby `brak` pomijane
  const Werdykt Z = Werdykt::ZGODNA, B = Werdykt::BRAK;
  PRZYPADEK("regresja", policz(wpisy({Z, B, B, B, Z, Z, Z, Z, Z, Z})).moznaPrzelaczyc());
}

// ============================================================================================================
// punkt 6: U-05, najdłuższe warianty
// ============================================================================================================
static void testU05Szerokosc() {
  printf("punkt 6: U-05 najdłuższe warianty\n");
  int m = miejsceNaWartosc("Noce z plików", TEST_SZEROKOSC);
  for (Przyczyna p : {Przyczyna::STARY_AIRBRIDGE, Przyczyna::BLAD_LISTY, Przyczyna::BEZ_SESJI, Przyczyna::ZA_DUZO_SESJI,
                      Przyczyna::NIEZAMKNIETA, Przyczyna::BEZ_STR, Przyczyna::STR_BEZ_DOBY, Przyczyna::PLIK_NIEPOBRANY,
                      Przyczyna::PLIK_NIECZYTELNY, Przyczyna::CZESCIOWA, Przyczyna::BRAK_CZASU}) {
    std::string w = wartosciU05(policz({wpisBraku("2026-11-04", p)}), Zrodlo::POROWNANIE).front();
    PRZYPADEK("regresja", szer(F_P20B, w) <= m);
  }
  // rozjazd we wszystkich metrykach: pierwszy wariant za długi, dopasujWartosc() bierze krótszy bez „…” na końcu
  Wpis e;
  e.data = "2026-11-04";
  e.werdykt = Werdykt::ROZJAZD;
  e.rozjazd = "wazna=nie~tak|sesje=2~1|m01=1~2|m02=1~2|m03oa=1~2|m03ca=1~2|m03h=1~2|m05=1~2|m06=1~2|m07=1~2|m08=1~2|"
              "m09=1~2|m10=1~2|m13=1~2|m17=1~2|m18=1~2|m21=a~b";
  std::string w = dopasujWartosc("Noce z plików", wartosciU05(policz({e}), Zrodlo::POROWNANIE), TEST_SZEROKOSC);
  printf("  najdłuższy rozjazd: \"%s\" (%d px z %d)\n", w.c_str(), szer(F_P20B, w), m);
  PRZYPADEK("regresja", szer(F_P20B, w) <= m);
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testNienaruszalnosc();
  testBudzet();
  testPlik();
  testWerdykt();
  testU05Szerokosc();
  printf("testy_porownanie_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
