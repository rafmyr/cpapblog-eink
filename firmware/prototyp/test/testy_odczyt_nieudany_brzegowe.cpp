// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe odczytu nieudanego (22.11.2026, T-WYM-19), niezależne od test/testy_odczyt_nieudany.cpp.
//
// Kluczowy model nośnika (V-xx): na panelu `PlikiEsp32::istnieje()` to `LittleFS.exists()`, a w arduino-esp32
// `VFSImpl::exists()` to `fopen(…, "r")` (libraries/FS/src/vfs_api.cpp). Gdy plik się nie otwiera (brak pamięci na
// bufor przy Wi-Fi i TLS, błąd nośnika, limit otwartych plików), `istnieje()` też zwraca false; `PlikiEsp32::czytaj()`
// zaczyna od tego samego `LittleFS.exists()`. Atrapa `AtrapaPlikow::nieczytelne` zostawia
// `istnieje()` true, więc nie modeluje najczęstszej usterki. `PlikiVfs` niżej: ścieżka „nie otwiera się” = istnieje,
// czytaj, czytajWiersze i pierwszyWiersz false (zapis działa), jak na panelu.
//
//  V1 (CZERWONY): Magazyn, plik nocy się nie otwiera → stan powinien być NIECZYTELNY i zapis odrzucony;
//     jest BRAK i przepisanie 20 wierszy dwoma.
//  V2 (CZERWONY): Magazyn::dopisz() dziennika, który się nie otwiera → dziennik zastąpiony jednym wierszem.
//  V3 (CZERWONY): dyrygent, import /import.csv przy pliku nocy, który się nie otwiera → rok przepisany
//     samymi nowymi nocami (ten sam przebieg co R2 przed poprawką, ale z semantyką panelu).
//  V4 (CZERWONY): dyrygent, niepełna lista katalogu (plik roku pominięty przez listuj()) → to samo.
//  V5 (kontrola, ZIELONY): V3 z usterką w modelu atrapy (istnieje true) → plik nietknięty, import ponowiony.
//  V6 (ZIELONY): dwa lata, rok 2025 nieczytelny, 2026 dobry, import nocy z obu lat → 2025 bajt w bajt, 2026 zapisany,
//     /import.csv zostaje; po naprawie nośnika 2025 dopisany, noce nie przechodzą między plikami.
//  V7 (CZERWONY): dopisanie, gdy plik się otwiera, a pierwszy wiersz czyta się pusto (PlikiEsp32::
//     pierwszyWiersz() przy błędzie odczytu daje "" z true) → dziennik odłożony jako `.uszkodzony` i zaczęty od nowa.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <set>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "magazyn.h"
#include "noc.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)

// Nośnik z semantyką arduino-esp32: ścieżka z `zamkniete` się nie otwiera (exists() == open() == false).
class PlikiVfs : public Pliki {
 public:
  explicit PlikiVfs(AtrapaPlikow& p) : p_(p) {}
  std::set<std::string> zamkniete;    // nie otwiera się: istnieje/czytaj/czytajWiersze/pierwszyWiersz false
  std::set<std::string> ukryte;       // pominięte w listuj() (openNextFile() bez pamięci), lista nadal true
  std::set<std::string> pustyPierwszy;  // otwiera się, ale pierwszy wiersz czyta się jako "" (readStringUntil przy błędzie)
  bool zamontuj() override { return p_.zamontuj(); }
  bool zamontowane() override { return p_.zamontowane(); }
  bool istnieje(const char* n) override { return !zamkniete.count(n) && p_.istnieje(n); }
  bool czytajWiersze(const char* n, std::vector<std::string>& w) override {
    w.clear();
    return !zamkniete.count(n) && p_.czytajWiersze(n, w);
  }
  bool zapiszAtomowo(const char* n, const std::string& t) override { return p_.zapiszAtomowo(n, t); }
  bool dopiszWiersz(const char* n, const std::string& w) override { return p_.dopiszWiersz(n, w); }
  bool pierwszyWiersz(const char* n, std::string& w) override {
    if (zamkniete.count(n)) return false;
    if (pustyPierwszy.count(n) && p_.istnieje(n)) {
      w.clear();
      return true;
    }
    return p_.pierwszyWiersz(n, w);
  }
  bool usun(const char* n) override { return p_.usun(n); }
  bool zmienNazwe(const char* z, const char* na) override { return p_.zmienNazwe(z, na); }
  void odmontuj() override { p_.odmontuj(); }
  bool czytaj(const char* n, std::string& t) override {
    t.clear();
    return !zamkniete.count(n) && p_.czytaj(n, t);
  }
  size_t zapisz(const char* n, const std::string& t) override { return p_.zapisz(n, t); }
  bool listuj(const char* k, std::vector<std::string>& nazwy) override {
    bool w = p_.listuj(k, nazwy);
    nazwy.erase(std::remove_if(nazwy.begin(), nazwy.end(), [&](const std::string& x) { return ukryte.count(x) > 0; }),
                nazwy.end());
    return w;
  }
  OpisPartycji partycja() override { return p_.partycja(); }
  bool czytajPartycje(size_t ile, std::string& b) override { return p_.czytajPartycje(ile, b); }
  bool formatuj() override { return p_.formatuj(); }
  long long wolneMiejsce() override { return p_.wolneMiejsce(); }

 private:
  AtrapaPlikow& p_;
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

// V1: Magazyn, plik nocy się nie otwiera (semantyka panelu)
static void testV1_MagazynVfs() {
  Dziennik d;
  AtrapaPlikow a(d);
  a.montowane = true;
  PlikiVfs p(a);
  std::string sc;
  {
    Magazyn m(p, nullptr);
    sc = m.sciezkaRoku("noce", "2026");
    SPRAWDZ(m.zapisz("noce", sc, wiersze("2026-10", 20)));
  }
  const std::string przed = a.pliki[sc];
  p.zamkniete.insert(sc);
  Magazyn m(p, nullptr);
  OdczytPliku o = m.czytaj("noce", sc);
  bool zapis = m.zapisz("noce", sc, wiersze("2026-11", 2));
  p.zamkniete.clear();
  size_t po = Magazyn(p, nullptr).czytaj("noce", sc).wiersze.size();
  printf("V1 plik nocy się nie otwiera: stan %s, zapis %s, wierszy po %zu z 20\n",
         o.stan == StanPliku::NIECZYTELNY ? "NIECZYTELNY" : o.stan == StanPliku::BRAK ? "BRAK" : "inny",
         zapis ? "PRZYJĘTY" : "odrzucony", po);
  SPRAWDZ(o.stan == StanPliku::NIECZYTELNY);  // CZERWONY: BRAK
  SPRAWDZ(!zapis);                            // CZERWONY
  SPRAWDZ(a.pliki[sc] == przed);              // CZERWONY: 20 → 2
}

// V2: Magazyn::dopisz() dziennika, który się nie otwiera
static void testV2_DopiszVfs() {
  for (const char* nazwa : {"humor", "wybudzenia"}) {
    Dziennik d;
    AtrapaPlikow a(d);
    a.montowane = true;
    PlikiVfs p(a);
    std::string sc;
    {
      Magazyn m(p, nullptr);
      sc = m.sciezkaRoku(nazwa, "");
      SPRAWDZ(m.zapisz(nazwa, sc, wiersze("2026-10", 30)));
    }
    const std::string przed = a.pliki[sc];
    p.zamkniete.insert(sc);
    Magazyn m(p, nullptr);
    m.dopisz(nazwa, sc, "2026-11-01;1");
    p.zamkniete.clear();
    size_t po = Magazyn(p, nullptr).czytaj(nazwa, sc).wiersze.size();
    printf("V2 dopisanie do %s, który się nie otwiera: wierszy po %zu z 30\n", nazwa, po);
    SPRAWDZ(a.pliki[sc] == przed);  // CZERWONY: dziennik zastąpiony jednym wierszem
  }
}

// ---------- dyrygent ----------
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900, t.tm_mon = mi - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static std::string wierszImportu(const std::string& data) {
  return data + ";1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;\n";
}
// Panel z `n` nocami w pliku roku `rok` (od `od` co dzień), bez sieci.
static void przygotuj(Panel& p, const std::string& rok, const std::string& od, int n) {
  p.pliki.montowane = true;
  Magazyn m(p.pliki, nullptr);
  std::vector<std::string> w;
  for (int i = 0; i < n; i++) {
    Noc x;
    std::string l = wierszImportu(dodajDni(od, i));
    l.pop_back();  // bez '\n'
    if (!parsujNoc(l, 'A', x)) continue;
    x.zrodlo = 'A';
    w.push_back(wierszNocyV5(x));
  }
  m.zapisz("noce", m.sciezkaRoku("noce", rok), w);
  p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  p.pliki.montowane = false;
  p.siec.siecJest = false;
}
static void wybudz(Panel& p, Pliki& f, uint32_t t) {
  p.port.log.clear();
  p.zegar.epoch = t;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Adaptery ad{p.zegar, p.nvs, f, p.siec, p.ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik,
              p.sen, p.port, p.diag, &p.wydania};
  Dyrygent(ad, KONF).wybudzenie();
}
static size_t nocyWPliku(Panel& p, const std::string& rok) {
  AtrapaPlikow& a = p.pliki;
  std::set<std::string> nc = a.nieczytelne;
  a.nieczytelne.clear();
  bool mont = a.montowane;
  a.montowane = true;
  Magazyn m(a, nullptr);
  size_t n = m.czytaj("noce", m.sciezkaRoku("noce", rok)).wiersze.size();
  a.montowane = mont;
  a.nieczytelne = nc;
  return n;
}
static bool wLogu(const Panel& p, const std::string& t) {
  for (auto& l : p.port.log)
    if (l.find(t) != std::string::npos) return true;
  return false;
}

// V3, V4, V5: import 2 nocy przy 40 nocach w pliku roku, z trzema rodzajami usterki
enum class Usterka { VFS_ZAMKNIETY, LISTA_NIEPELNA, MODEL_IMPLEMENTUJACEGO };
static size_t importZUsterka(Usterka u, bool& importOznaczony) {
  Panel p;
  przygotuj(p, "2026", "2026-09-20", 40);
  PlikiVfs f(p.pliki);
  const std::string sc = "/dane/noce/2026.csv";
  p.pliki.pliki["/import.csv"] = wierszImportu("2026-11-01") + wierszImportu("2026-11-02");
  if (u == Usterka::VFS_ZAMKNIETY) f.zamkniete.insert(sc);
  if (u == Usterka::LISTA_NIEPELNA) f.ukryte.insert(sc);
  if (u == Usterka::MODEL_IMPLEMENTUJACEGO) p.pliki.nieczytelne.insert(sc);
  wybudz(p, f, ep(2026, 11, 4, 7, 0));
  importOznaczony = p.pliki.pliki.count("/import.wczytany") == 1;
  // usterka mija, następne wybudzenie
  f.zamkniete.clear();
  f.ukryte.clear();
  p.pliki.nieczytelne.clear();
  wybudz(p, f, ep(2026, 11, 4, 7, 30));
  return nocyWPliku(p, "2026");
}
static void testV3V4V5_ImportZUsterka() {
  bool oznA = false, oznB = false, oznC = false;
  size_t a = importZUsterka(Usterka::VFS_ZAMKNIETY, oznA);
  size_t b = importZUsterka(Usterka::LISTA_NIEPELNA, oznB);
  size_t c = importZUsterka(Usterka::MODEL_IMPLEMENTUJACEGO, oznC);
  printf("V3 import przy pliku nocy, który się nie otwiera: nocy w 2026 po %zu z 42 (import oznaczony %d)\n", a,
         (int)oznA);
  printf("V4 import przy liście katalogu bez pliku 2026: nocy w 2026 po %zu z 42 (import oznaczony %d)\n", b,
         (int)oznB);
  printf("V5 (kontrola) import przy usterce z modelu atrapy: nocy w 2026 po %zu z 42 (oznaczony w 1. %d)\n", c,
         (int)oznC);
  SPRAWDZ(a == 42);  // CZERWONY: 2
  SPRAWDZ(b == 42);  // CZERWONY: 2
  SPRAWDZ(c == 42);  // zielony: blokada działa, import ponowiony w następnym wybudzeniu
  SPRAWDZ(!oznC);
}

// V6: dwa lata, 2025 nieczytelny (model atrapy), 2026 dobry; import nocy z obu lat
static void testV6_DwaLata() {
  Panel p;
  przygotuj(p, "2025", "2025-12-01", 20);
  {
    Panel q;
    przygotuj(q, "2026", "2026-01-01", 30);
    p.pliki.pliki["/dane/noce/2026.csv"] = q.pliki.pliki["/dane/noce/2026.csv"];
  }
  const std::string s25 = "/dane/noce/2025.csv", s26 = "/dane/noce/2026.csv";
  const std::string przed25 = p.pliki.pliki[s25];
  p.pliki.pliki["/import.csv"] = wierszImportu("2025-12-25") + wierszImportu("2026-11-01");
  p.pliki.nieczytelne.insert(s25);
  PlikiVfs f(p.pliki);
  wybudz(p, f, ep(2026, 11, 4, 7, 0));
  bool bajt25 = p.pliki.pliki[s25] == przed25;
  size_t n26 = nocyWPliku(p, "2026");
  bool importZostal = p.pliki.pliki.count("/import.csv") == 1;
  bool log25 = wLogu(p, "noce/2025.csv: odczyt nieudany");
  p.pliki.nieczytelne.clear();
  wybudz(p, f, ep(2026, 11, 4, 7, 30));
  size_t n25po = nocyWPliku(p, "2025"), n26po = nocyWPliku(p, "2026");
  printf("V6 dwa lata (2025 nieczytelny): 2025 bajt w bajt %d, 2026 nocy %zu z 31, /import.csv zostaje %d, log %d; "
         "po naprawie 2025 %zu z 21, 2026 %zu z 31\n",
         (int)bajt25, n26, (int)importZostal, (int)log25, n25po, n26po);
  SPRAWDZ(bajt25);
  SPRAWDZ(n26 == 31);
  SPRAWDZ(importZostal);
  SPRAWDZ(log25);
  SPRAWDZ(n25po == 21 && n26po == 31);
  SPRAWDZ(p.pliki.pliki.count("/import.csv") == 0);
}

// V7: plik dziennika się otwiera, ale pierwszy wiersz czyta się pusto (PlikiEsp32::pierwszyWiersz() przy błędzie
// odczytu w readStringUntil()); nagłówek „nieczytelny” → dziennik odłożony jako `.uszkodzony` i zaczęty od nowa
static void testV7_PustyPierwszyWiersz() {
  Dziennik d;
  AtrapaPlikow a(d);
  a.montowane = true;
  PlikiVfs p(a);
  std::string sc;
  {
    Magazyn m(p, nullptr);
    sc = m.sciezkaRoku("humor", "");
    SPRAWDZ(m.zapisz("humor", sc, wiersze("2026-10", 30)));
  }
  p.pustyPierwszy.insert(sc);
  Magazyn m(p, nullptr);
  m.dopisz("humor", sc, "2026-11-01;1");
  p.pustyPierwszy.clear();
  size_t po = Magazyn(p, nullptr).czytaj("humor", sc).wiersze.size();
  printf("V7 pierwszy wiersz czytany pusto przy dopisaniu: dziennik %zu z 31 wierszy, odłożony %d\n", po,
         (int)a.pliki.count(sc + ".uszkodzony"));
  SPRAWDZ(po == 31);  // CZERWONY: 1 (historia w `.uszkodzony`, ekrany bez niej)
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  printf("przypadki brzegowe: odczyt nieudany z semantyką panelu\n");
  testV1_MagazynVfs();
  testV2_DopiszVfs();
  testV3V4V5_ImportZUsterka();
  testV6_DwaLata();
  testV7_PustyPierwszyWiersz();
  printf("testy_odczyt_nieudany_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
