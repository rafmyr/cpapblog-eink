// SPDX-License-Identifier: GPL-3.0-or-later
// Odczyt nieudany ≠ plik pusty (T-WYM-19, T-TEST-515) i ciche błędy dopisywania.
//  O1. Magazyn: istniejący plik nocy, czynników, sypialni, odpowiedzi i dobrego startu, którego odczyt się nie
//      udaje → NIECZYTELNY, wiersz w logu, zapis i dopisanie tej ścieżki odrzucone do końca życia obiektu, plik bajt
//      w bajt; następny obiekt (następne wybudzenie) czyta i zapisuje normalnie.
//  O2. Brak pliku działa jak dotąd: BRAK, pierwszy zapis i pierwsze dopisanie tworzą plik.
//  O3. Plik dopisywany: nieudany odczyt pierwszego wiersza istniejącego pliku → bez odkładania jako `.uszkodzony`
//      i bez nowego pliku; plik pusty (czytelny) nadal odłożony i zaczęty od nowa.
//  O4. Nieczytelny plik docelowy z kompletnym `.tmp`: `.tmp` użyty (T-WYM-53), bez blokady.
//  O6. Nieudane dopisanie (atrapa odmawia) → Magazyn::dopisz() false, plik bez zmian; na dyrygencie wiersz
//      w logu `zapis log/wybudzenia.csv: dopisanie wiersza nieudane`; udane dopisanie bez wiersza.
// Testy na dyrygencie obu ścieżek (głęboki sen i czekanie na kablu, ADR-0047): test/testy_kabel_bez_snu_brzegowe.cpp
// R2 (usterka przejściowa pliku nocy) i R2d (trwała usterka odczytu wszystkich plików danych).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "magazyn.h"
#include "start.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)

struct Log {
  std::vector<std::string> w;
  std::function<void(const std::string&)> f() {
    return [this](const std::string& s) { w.push_back(s); };
  }
  bool ma(const std::string& t) const {
    for (auto& l : w)
      if (l.find(t) != std::string::npos) return true;
    return false;
  }
  int ile(const std::string& t) const {
    int n = 0;
    for (auto& l : w) n += l.find(t) != std::string::npos;
    return n;
  }
};

static std::vector<std::string> wiersze(const char* przedrostek, int n) {
  std::vector<std::string> v;
  for (int i = 1; i <= n; i++) {
    char b[48];
    snprintf(b, sizeof b, "%s-%02d;%d", przedrostek, i, i);
    v.push_back(b);
  }
  return v;
}

// O1: plik jest, odczyt nieudany: przepisywane formaty (noce, czynniki, sypialnia, odpowiedzi, dobry start, reguły)
// jakOtwarcie: istnieje() false dla pliku, który się nie otwiera (LittleFS.exists() przed poprawką)
static void testO1_PrzepisywaneNietkniete(bool jakOtwarcie) {
  for (const char* nazwa : {"noce", "czynniki", "sypialnia", "odpowiedzi", "dobry_start", "reguly", "portret"}) {
    Dziennik d;
    AtrapaPlikow p(d);
    p.montowane = true;
    p.istniejeJakOtwarcie = jakOtwarcie;
    std::string sc;
    {
      Magazyn m(p, nullptr);
      sc = m.sciezkaRoku(nazwa, "2026");
      SPRAWDZ(m.zapisz(nazwa, sc, wiersze("2026-10", 20)));
    }
    const std::string przed = p.pliki[sc];
    p.nieczytelne.insert(sc);
    Log lg;
    Magazyn m(p, lg.f());
    OdczytPliku o = m.czytaj(nazwa, sc);
    SPRAWDZ(o.stan == StanPliku::NIECZYTELNY);
    SPRAWDZ(o.wiersze.empty());
    SPRAWDZ(m.nieczytelny(sc));
    SPRAWDZ(lg.ile("odczyt nieudany, plik zostaje bez zmian") == 1);
    // przepisanie samymi nowymi wierszami (dawniej: 20 wierszy → 2) odrzucone, plik bajt w bajt
    SPRAWDZ(!m.zapisz(nazwa, sc, wiersze("2026-11", 2)));
    SPRAWDZ(!m.zapisz(nazwa, sc, wiersze("2026-11", 2)));
    SPRAWDZ(p.pliki[sc] == przed);
    SPRAWDZ(!p.pliki.count(sc + ".tmp") && !p.pliki.count(sc + ".uszkodzony"));
    SPRAWDZ(lg.ile("odczyt nieudany") == 1);  // jeden wiersz na ścieżkę na wybudzenie
    // drugi odczyt w tym samym wybudzeniu: ten sam stan, bez drugiego wiersza logu
    SPRAWDZ(m.czytaj(nazwa, sc).stan == StanPliku::NIECZYTELNY && lg.ile("odczyt nieudany") == 1);
    // nośnik znów sprawny w tym samym wybudzeniu: blokada zostaje (pamięć mogła już zbudować stan z pustego odczytu)
    p.nieczytelne.clear();
    SPRAWDZ(!m.zapisz(nazwa, sc, wiersze("2026-11", 2)) && p.pliki[sc] == przed);
    // następne wybudzenie (nowy obiekt): odczyt pełny, zapis działa
    Magazyn n(p, nullptr);
    OdczytPliku o2 = n.czytaj(nazwa, sc);
    SPRAWDZ(o2.stan == StanPliku::DOBRY && o2.wiersze.size() == 20);
    std::vector<std::string> v = o2.wiersze;
    for (auto& x : wiersze("2026-11", 2)) v.push_back(x);
    SPRAWDZ(n.zapisz(nazwa, sc, v));
    SPRAWDZ(Magazyn(p, nullptr).czytaj(nazwa, sc).wiersze.size() == 22);
    printf("O1%s %s: odczyt nieudany → %s, 20 wierszy nietkniętych, w następnym wybudzeniu zapis 22\n", jakOtwarcie ? " (exists = open)" : "", nazwa,
           o.stan == StanPliku::NIECZYTELNY ? "NIECZYTELNY" : "inny stan");
  }
}

// O2: brak pliku jak dotąd (pierwszy zapis i pierwsze dopisanie tworzą plik)
static void testO2_BrakPliku() {
  Dziennik d;
  AtrapaPlikow p(d);
  p.montowane = true;
  Log lg;
  Magazyn m(p, lg.f());
  for (const char* nazwa : {"noce", "czynniki", "sypialnia"}) {
    std::string sc = m.sciezkaRoku(nazwa, "2026");
    OdczytPliku o = m.czytaj(nazwa, sc);
    SPRAWDZ(o.stan == StanPliku::BRAK && !m.nieczytelny(sc));
    SPRAWDZ(m.zapisz(nazwa, sc, wiersze("2026-11", 3)));
    SPRAWDZ(Magazyn(p, nullptr).czytaj(nazwa, sc).wiersze.size() == 3);
  }
  std::string sh = m.sciezkaRoku("humor", "");
  SPRAWDZ(m.czytaj("humor", sh).stan == StanPliku::BRAK);
  SPRAWDZ(m.dopisz("humor", sh, "2026-11-22;a;b;c;d;e"));
  SPRAWDZ(m.dopisz("humor", sh, "2026-11-22;a;b;c;d;f"));
  SPRAWDZ(Magazyn(p, nullptr).czytaj("humor", sh).wiersze.size() == 2);
  SPRAWDZ(!lg.ma("odczyt nieudany"));
  printf("O2 brak pliku: pierwszy zapis i dopisanie tworzą plik, bez blokady\n");
}

// O3: plik dopisywany (humor, wybudzenia): nieudany odczyt pierwszego wiersza ≠ nagłówek nieczytelny
static void testO3_Dopisywane(bool jakOtwarcie) {
  for (const char* nazwa : {"humor", "wybudzenia"}) {
    Dziennik d;
    AtrapaPlikow p(d);
    p.montowane = true;
    p.istniejeJakOtwarcie = jakOtwarcie;
    std::string sc;
    {
      Magazyn m(p, nullptr);
      sc = m.sciezkaRoku(nazwa, "");
      SPRAWDZ(m.zapisz(nazwa, sc, wiersze("2026-10", 30)));  // przepisanie atomowe dziennika (przycięcie)
    }
    const std::string przed = p.pliki[sc];
    p.nieczytelne.insert(sc);
    Log lg;
    Magazyn m(p, lg.f());
    // dopisanie bez wcześniejszego odczytu (tak dopisuje dyrygent): dawniej plik odłożony jako `.uszkodzony`
    // i dziennik zaczęty od nowa jednym wierszem
    SPRAWDZ(!m.dopisz(nazwa, sc, "2026-11-01;1"));
    SPRAWDZ(p.pliki[sc] == przed);
    SPRAWDZ(!p.pliki.count(sc + ".uszkodzony"));
    SPRAWDZ(m.nieczytelny(sc) && lg.ile("odczyt nieudany") == 1);
    SPRAWDZ(!m.dopisz(nazwa, sc, "2026-11-02;2") && p.pliki[sc] == przed);
    SPRAWDZ(!m.zapisz(nazwa, sc, wiersze("2026-11", 1)) && p.pliki[sc] == przed);
    p.nieczytelne.clear();
    Magazyn n(p, nullptr);
    SPRAWDZ(n.dopisz(nazwa, sc, "2026-11-03;3"));
    SPRAWDZ(Magazyn(p, nullptr).czytaj(nazwa, sc).wiersze.size() == 31);
  }
  // plik pusty jest czytelny: dalej jak plik bez nagłówka (odłożony, nowy plik), bez blokady
  Dziennik d;
  AtrapaPlikow p(d);
  p.montowane = true;
  Log lg;
  Magazyn m(p, lg.f());
  std::string sc = m.sciezkaRoku("humor", "");
  p.pliki[sc] = "";
  SPRAWDZ(m.dopisz("humor", sc, "2026-11-01;a;b;c;d;e"));
  SPRAWDZ(p.pliki.count(sc + ".uszkodzony") == 1 && !m.nieczytelny(sc));
  SPRAWDZ(Magazyn(p, nullptr).czytaj("humor", sc).wiersze.size() == 1);
  printf("O3 dopisywane: nieudany odczyt nagłówka bez odkładania i bez nowego pliku; plik pusty jak dotąd\n");
}

// O4: nieczytelny plik docelowy, kompletny `.tmp` (zanik zasilania przed zmianą nazwy): `.tmp` użyty, bez blokady
static void testO4_KompletnyTmp() {
  Dziennik d;
  AtrapaPlikow p(d);
  p.montowane = true;
  std::string sc;
  {
    Magazyn m(p, nullptr);
    sc = m.sciezkaRoku("noce", "2026");
    SPRAWDZ(m.zapisz("noce", sc, wiersze("2026-10", 5)));
    SPRAWDZ(m.zapisz("noce", sc + ".x", wiersze("2026-10", 6)));
  }
  p.pliki[sc + ".tmp"] = p.pliki[sc + ".x"];
  p.pliki.erase(sc + ".x");
  p.nieczytelne.insert(sc);
  Log lg;
  Magazyn m(p, lg.f());
  OdczytPliku o = m.czytaj("noce", sc);
  SPRAWDZ(o.stan == StanPliku::DOBRY && o.wiersze.size() == 6);
  SPRAWDZ(!m.nieczytelny(sc) && !lg.ma("odczyt nieudany"));
  SPRAWDZ(!p.pliki.count(sc + ".tmp"));
  printf("O4 kompletny .tmp przy nieczytelnym pliku docelowym: użyty (T-WYM-53), %zu wierszy\n", o.wiersze.size());
}

// O6: nieudane dopisanie: magazyn zwraca false, plik bez zmian; dyrygent zapisuje wiersz w logu z nazwą pliku
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t ep(int d, int h, int m) {
  std::tm t{};
  t.tm_year = 126, t.tm_mon = 10, t.tm_mday = d, t.tm_hour = h, t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static void wybudzZegarem(Panel& p, uint32_t t) {
  p.port.log.clear();
  p.zegar.epoch = t;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static bool wLogu(const Panel& p, const std::string& t) {
  for (auto& l : p.port.log)
    if (l.find(t) != std::string::npos) return true;
  return false;
}
static void testO6_NieudaneDopisanie() {
  {
    Dziennik d;
    AtrapaPlikow p(d);
    p.montowane = true;
    Magazyn m(p, nullptr);
    std::string sc = m.sciezkaRoku("wybudzenia", "");
    SPRAWDZ(m.dopisz("wybudzenia", sc, "1790000000;3950;0;100;Z"));
    const std::string przed = p.pliki[sc];
    p.odmawiaDopisaniaPod = sc;
    SPRAWDZ(!m.dopisz("wybudzenia", sc, "1790000600;3940;0;100;Z"));
    SPRAWDZ(p.pliki[sc] == przed);
    p.odmawiaDopisaniaPod.clear();
    SPRAWDZ(m.dopisz("wybudzenia", sc, "1790000600;3940;0;100;Z") && p.pliki[sc] != przed);
  }
  Panel p;
  p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  wybudzZegarem(p, ep(20, 12, 0));
  const std::string sc = "/log/wybudzenia.csv";
  bool jest = p.pliki.pliki.count(sc) == 1;
  SPRAWDZ(jest);  // dziennik wybudzeń dopisany w zwykłym wybudzeniu
  SPRAWDZ(!wLogu(p, "dopisanie wiersza nieudane"));  // udane dopisanie bez wiersza w logu
  const std::string przed = jest ? p.pliki.pliki[sc] : "";
  p.pliki.odmawiaDopisaniaPod = sc;
  wybudzZegarem(p, ep(20, 12, 30));
  SPRAWDZ(wLogu(p, "zapis log/wybudzenia.csv: dopisanie wiersza nieudane"));
  SPRAWDZ(p.pliki.pliki[sc] == przed);
  p.pliki.odmawiaDopisaniaPod.clear();
  wybudzZegarem(p, ep(20, 13, 0));
  SPRAWDZ(!wLogu(p, "dopisanie wiersza nieudane") && p.pliki.pliki[sc].size() > przed.size());
  printf("O6: nieudane dopisanie dziennika wybudzeń %s w logu\n",
         jest ? "z wierszem" : "(dziennik nie powstał, test nieważny)");
}

// O7: plik uszkodzony (bez `#koniec`), którego nie da się odłożyć jako `.uszkodzony` (atrapa odmawia zmiany
// nazwy na `.uszkodzony`): NIECZYTELNY, wiersz logu jak przy odczycie nieudanym, plik na liście plikiNieczytelne(),
// zapis odrzucony, plik bajt w bajt. Kontrola: ta sama usterka bez odmowy zmiany nazwy daje odłożenie jak dotąd.
static void testO7_UszkodzonyBezOdlozenia() {
  for (bool odmowaNazwy : {true, false}) {
    Dziennik d;
    AtrapaPlikow p(d);
    p.montowane = true;
    std::string sc;
    {
      Magazyn m(p, nullptr);
      sc = m.sciezkaRoku("noce", "2026");
      SPRAWDZ(m.zapisz("noce", sc, wiersze("2026-10", 20)));
    }
    std::string& t = p.pliki[sc];
    size_t k = t.rfind("#koniec");
    SPRAWDZ(k != std::string::npos);
    t.erase(k);  // przepisywany bez `#koniec` = uszkodzony
    const std::string przed = t;
    if (odmowaNazwy) p.odmawiaZapisuPod = sc + ".uszkodzony";
    Log lg;
    Magazyn m(p, lg.f());
    OdczytPliku o = m.czytaj("noce", sc);
    if (odmowaNazwy) {
      SPRAWDZ(o.stan == StanPliku::NIECZYTELNY);
      SPRAWDZ(m.nieczytelny(sc) && m.plikiNieczytelne().size() == 1);
      SPRAWDZ(lg.ile("odłożenie jako .uszkodzony nieudane") == 1);
      SPRAWDZ(lg.ile("odczyt nieudany, plik zostaje bez zmian") == 1);
      SPRAWDZ(!m.zapisz("noce", sc, wiersze("2026-11", 2)));
      SPRAWDZ(lg.ile("nie był w tym wybudzeniu przeczytany") == 0);  // bez mylącego wiersza odmowy
      SPRAWDZ(p.pliki[sc] == przed && !p.pliki.count(sc + ".uszkodzony"));
    } else {
      SPRAWDZ(o.stan == StanPliku::USZKODZONY && !m.nieczytelny(sc));
      SPRAWDZ(p.pliki.count(sc + ".uszkodzony") && !p.pliki.count(sc));
      SPRAWDZ(m.zapisz("noce", sc, wiersze("2026-11", 2)));
    }
    printf("O7 uszkodzony, %s: stan %s, plików nieczytelnych %u\n",
           odmowaNazwy ? "odłożenie nieudane" : "odłożenie udane (kontrola)",
           o.stan == StanPliku::NIECZYTELNY ? "NIECZYTELNY" : o.stan == StanPliku::USZKODZONY ? "USZKODZONY" : "inny",
           (unsigned)m.plikiNieczytelne().size());
  }
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  printf("testy odczytu nieudanego i dopisania nieudanego (konfiguracja: testy_konfiguracja_z_telefonu)\n");
  testO1_PrzepisywaneNietkniete(false);
  testO1_PrzepisywaneNietkniete(true);
  testO2_BrakPliku();
  testO3_Dopisywane(false);
  testO3_Dopisywane(true);
  testO4_KompletnyTmp();
  testO6_NieudaneDopisanie();
  testO7_UszkodzonyBezOdlozenia();
  printf("testy_odczyt_nieudany: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
