// SPDX-License-Identifier: GPL-3.0-or-later
// Testy magazynu danych, migracji, układu pamięci 32 MB i przywracania (wersja pełna).
// Pisane ze spec technicznej (8.2, 8.4, 8.5, 8.9, 9.1, 9.2: T-WYM-34,
// T-WYM-38, T-WYM-43, T-WYM-50…T-WYM-58, T-WYM-62, T-WYM-84, T-WYM-87; T-TEST-204, 206, 207, 208, 274, 276, 280) i
// funkcjonalnej (R-NF-12, R-NF-19, R-NF-27, R-NF-28, R-DANE-37, R-AW-16, R-KON-41), bez projektowania pod implementację.
// Testowane: src/magazyn.cpp, otoczenie.cpp adresWSieciLokalnej() i Dyrygent::teksty(),
// a także firmware/partycje-e1001*.csv.
// Interfejsy zaprojektowane do tych testów (kontrakty w nagłówkach): src/magazyn.h (rejestr formatów, Magazyn,
// NvsTylkoZmiany, ukladWkompilowany(), znacznikPartycji(), sha256Hex()), src/adaptery.h (Pliki::czytaj(), zapisz(),
// listuj(), partycja(), czytajPartycje(), formatuj(); OpisPartycji), src/otoczenie.h adresWSieciLokalnej(),
// src/dyrygent.h (KonfiguracjaPanelu::rejestr, Dyrygent::teksty()). Atrapa plików (test/atrapy/adaptery_atrapy.h):
// pełna pamięć, zanik zasilania po zadanym bajcie, odmowa zapisu pod przedrostkiem, surowe bloki partycji.
//
// Ustalenia autora testów tam, gdzie spec albo karta zostawiają wybór (raport autora testów, sekcja 6):
// - wersja 1 każdego pliku = pola prototypu z komentarzy src/*.h (R2), plus `import` w nocach (R1);
//    `pominiete` ma jedno pole `day`, `sypialnia` pola `data;temp;wilg;co2` (połączenie /sypialnia.csv
//    i /sypialnia-co2.csv);
//  - komenda `W` (R8): wiersz `W {nazwa} {rozmiar} {sha256}\n`, potem treść; zła nazwa i za duży rozmiar są
//    sprawdzane w osobnych oknach (nie wiadomo, czy panel czyta treść po odrzuconym nagłówku);
//  - okno przywracania liczy 10 min od startu; test ustawia znany zegar (bez zegara po zaniku zasilania godziny
//    `do {hh:mm}` nie da się policzyć, spec tego nie rozstrzyga);
//  - T-WYM-84: formatowanie tylko przy braku znacznika `fs_init`; test „znacznik i partycja nieczytelna” używa
//    śmieci bez `0xFF`, więc nie rozstrzyga, czy skasowana partycja ze znacznikiem ma być formatowana.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_magazyn_danych, na końcu).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "bateria.h"
#include "czynniki.h"
#include "dyrygent.h"
#include "humor.h"
#include "magazyn.h"
#include "noc.h"
#include "otoczenie.h"
#include "start.h"
#include "sypialnia.h"
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

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";  // jak test/testy_slot_dzienny_w_planie.cpp

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

// ============================================================================================================
// Pomocnicze: teksty, pliki, dane syntetyczne (żadnych prawdziwych danych zdrowotnych)
// ============================================================================================================

static std::vector<std::string> dziel(const std::string& s, char z) {
  std::vector<std::string> v;
  size_t a = 0;
  for (;;) {
    size_t b = s.find(z, a);
    v.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  return v;
}
static std::string zlacz(const std::vector<std::string>& v, size_t od, size_t ile) {
  std::string t;
  for (size_t i = od; i < od + ile && i < v.size(); i++) t += (i > od ? ";" : "") + v[i];
  return t;
}
// Wiersze treści pliku (bez pustego elementu po ostatnim '\n').
static std::vector<std::string> wiersze(const std::string& t) {
  std::vector<std::string> v = dziel(t, '\n');
  if (!v.empty() && v.back().empty()) v.pop_back();
  return v;
}
static bool zawiera(const std::vector<std::string>& v, const std::string& p) {
  for (auto& s : v)
    if (s.find(p) != std::string::npos) return true;
  return false;
}
static std::string polaTekst(const std::vector<std::string>& p) { return zlacz(p, 0, p.size()); }

// Oczekiwane formaty wersji 1 (R2 karty tabela „Pliki”; nazwy pól z komentarzy src/*.h).
static const char* POLA_NOCE =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import";
static const char* POLA_CZYNNIKI = "data;stan;kody;warstwa;termin;pokazano";
static const char* POLA_SYPIALNIA = "data;temp;wilg;co2";
static const char* POLA_HUMOR = "data;typ;klucz;tryb;scena;zdanie";
static const char* POLA_WYBUDZENIA = "epoch;mV;wifi_ms;aktywny_ms;powod";
struct Oczekiwany {
  const char* nazwa;
  const char* sciezka;
  const char* v0;
  RodzajZapisu rodzaj;
  const char* pola;
};
static const Oczekiwany FORMATY[] = {
    {"noce", "/dane/noce/{RRRR}.csv", "/noce.csv", RodzajZapisu::PRZEPISANIE, POLA_NOCE},
    {"czynniki", "/dane/czynniki.csv", "/czynniki.csv", RodzajZapisu::PRZEPISANIE, POLA_CZYNNIKI},
    {"sypialnia", "/dane/sypialnia/{RRRR}.csv", "/sypialnia.csv", RodzajZapisu::PRZEPISANIE, POLA_SYPIALNIA},
    {"humor", "/dane/humor.csv", "/humor.csv", RodzajZapisu::DOPISANIE, POLA_HUMOR},
    {"wybudzenia", "/log/wybudzenia.csv", "/bateria.csv", RodzajZapisu::DOPISANIE, POLA_WYBUDZENIA},
    {"pominiete", "/dane/ab-pominiete.csv", "/ab_pominiete.txt", RodzajZapisu::PRZEPISANIE, "day"},
    {"proby", "/dane/ab-proby.csv", "/ab_proby.txt", RodzajZapisu::PRZEPISANIE, "day;proby;ostatnia;stale"},
    // pamięć reguł (src/reguly_pamiec.h), bez pliku prototypu
    {"reguly", "/dane/reguly.csv", "", RodzajZapisu::PRZEPISANIE,
     "id;wariant;aktywna_od;ostatnio;licznik;stan_wzorca;wiem;omowione"},
    // odpowiedzi tak/nie 5.12.8 (bez pliku prototypu)
    {"odpowiedzi", "/dane/odpowiedzi.csv", "", RodzajZapisu::PRZEPISANIE, "data;pytanie;odpowiedz"},
};
static std::string sciezkaRoku(const std::string& wzor, const std::string& rok) {
  std::string s = wzor;
  size_t i = s.find("{RRRR}");
  if (i != std::string::npos) s.replace(i, 6, rok);
  return s;
}

// Plik w formacie wersji `wersja`: nagłówek, nazwy pól, wiersze, przy przepisywanych `#koniec {n}`.
static std::string plik(const std::string& nazwa, int wersja, const std::string& pola,
                        const std::vector<std::string>& w, bool koniec) {
  std::string t = "#cpap-eink " + nazwa + " " + std::to_string(wersja) + "\n" + pola + "\n";
  for (auto& x : w) t += x + "\n";
  if (koniec) t += "#koniec " + std::to_string(w.size()) + "\n";
  return t;
}
// Plik prototypu (wersja 0): same wiersze.
static std::string plikV0(const std::vector<std::string>& w) {
  std::string t;
  for (auto& x : w) t += x + "\n";
  return t;
}

// Noc syntetyczna `i` (wartości z zakresów prototypu, bez związku z żadną osobą).
static Noc nocTestowa(const std::string& data, int i) {
  Noc n;
  n.data = data;
  n.wazna = i % 9 != 0;
  n.m01 = 300 + i % 150;
  n.ahi = 0.5f + (float)(i % 40) * 0.1f;
  n.cmed = 9.5f + (float)(i % 10) * 0.1f;
  n.cp95 = 12.5f + (float)(i % 8) * 0.2f;
  n.wmed = 2.0f + (float)(i % 7) * 0.3f;
  n.wp95 = 8.0f + (float)(i % 5);
  n.emed = 7.0f;
  n.ep95 = 8.0f + (float)(i % 3) * 0.5f;
  n.psmed = 3.0f;
  n.psp95 = 5.0f;
  n.ustawienia = "ASVAuto/6/10/3/15";
  n.zrodlo = 'A';
  n.oa = 0.2f;
  n.ca = 0.6f;
  n.h = 0.5f;
  n.csr = (float)(i % 5);
  return n;
}
// Wiersz nocy wersji 0 w układzie `pol` pól (src/noc.h: 8, 9, 12, 13, 14, 20) z pełnego wiersza 20 pól.
static std::string nocWUkladzie(const std::string& w20, int pol) {
  std::vector<std::string> f = dziel(w20, ';');
  switch (pol) {
    case 8: return zlacz(f, 0, 8);
    case 9: return zlacz(f, 0, 8) + ";" + f[13];
    case 12: return zlacz(f, 0, 12);
    case 13: return zlacz(f, 0, 13);
    case 14: return zlacz(f, 0, 14);
    default: return w20;
  }
}
// `n` kolejnych nocy kończących się datą poranka `ostatnia`, wiersze wersji 0 w układzie 20 pól.
static std::vector<std::string> noceV0(int n, const std::string& ostatnia, int pol = 20) {
  std::vector<std::string> v;
  for (int i = 0; i < n; i++)
    v.push_back(nocWUkladzie(zapiszNoc(nocTestowa(dodajDni(ostatnia, i - (n - 1)), i + 1)), pol));
  return v;
}
// To samo w wersji 1 (pole `import` puste).
static std::vector<std::string> noceV1(int n, const std::string& ostatnia) {
  std::vector<std::string> v = noceV0(n, ostatnia);
  for (auto& w : v) w += ";";
  return v;
}

// format `noce` ma wersję 2 (pola wersji 1 i `m13;m17;pna;srn;m08`).
// Testy tworzą pliki nocy „bieżącej” wersji (panel po aktualizacji) w znanej wersji formatu z rejestru, a migrację
// 1 → 2 sprawdza test/testy_rekord_nocy.cpp; rejestrV2() daje wersję o jeden wyższą niż znana (dla nocy: 3).
static const FormatPliku& formatNoce() { return *formatPliku(rejestrFormatow(), "noce"); }
static int wersjaNoce() { return formatNoce().wersja(); }
static std::string polaNoceZnane() { return polaTekst(formatNoce().pola.back()); }
// Wiersz nocy (wersja 1, 21 pól) dopełniony pustymi polami do znanej wersji.
static std::string doZnanejNoce(const std::string& w) {
  size_t n = dziel(w, ';').size(), ile = formatNoce().pola.back().size();
  return n >= ile ? w : w + std::string(ile - n, ';');
}
static std::vector<std::string> noceZnane(int n, const std::string& ostatnia) {
  std::vector<std::string> v = noceV1(n, ostatnia);
  for (auto& w : v) w = doZnanejNoce(w);
  return v;
}
static std::string plikNoce(const std::vector<std::string>& w) {
  std::vector<std::string> v;
  for (auto& x : w) v.push_back(doZnanejNoce(x));
  return plik("noce", wersjaNoce(), polaNoceZnane(), v, true);
}
static std::string naglowekNoce(int wersja) { return "#cpap-eink noce " + std::to_string(wersja); }

static std::string wierszFormatu(const std::string& nazwa, int i) {
  std::string d = dodajDni("2026-10-01", i);
  if (nazwa == "noce") return doZnanejNoce(zapiszNoc(nocTestowa(d, i + 1)) + ";");
  if (nazwa == "czynniki") return zapiszWpis(WpisCzynnikow{d, 'O', CZ_STRES, 'K', -1, true});
  // sypialnia w wersji 2 (seria CO2 nocy: co2_ponad;co2_maks;co2_seria), tu bez serii
  if (nazwa == "sypialnia") return zapiszSypialnie(NocSypialni{d, 20.5f, 45.0f}) + ";" + std::to_string(700 + i) + ";;;";
  if (nazwa == "prognoza") return d + ";" + std::to_string(i % 24) + ";4.5;10.0;80.0;20.0;12.0;;;;;1793800000";
  if (nazwa == "obrazki") return d + ";00" + std::to_string(i % 10) + "_x.pbm;n";
  if (nazwa == "humor") return zapiszHumor(WpisHumoru{d, "S2A", std::to_string(i + 1), 'D', "", ""});
  if (nazwa == "wybudzenia")
    return zapiszBaterie(WpisBaterii{ep(2026, 10, 1, 7, 0) + (uint32_t)i * 1800u, 3950, 1200, 5000, "zegar:okno_poranne"});
  if (nazwa == "pominiete") return std::to_string(nrDnia(d));
  if (nazwa == "reguly") return "E-30;;" + d + ";" + d + ";" + std::to_string(i + 1) + ";;;";
  if (nazwa == "odpowiedzi") return d + ";sennosc;" + (i % 2 ? "tak" : "nie");
  return std::to_string(nrDnia(d)) + ";2;" + std::to_string(ep(2026, 10, 1, 7, 0)) + ";0";  // proby
}

// Log magazynu do wektora.
struct Log {
  std::vector<std::string> w;
  std::function<void(const std::string&)> f() {
    return [this](const std::string& s) { w.push_back(s); };
  }
  bool ma(const std::string& p) const { return zawiera(w, p); }
};

// Rejestr z testową wersją o jeden wyższą niż znana formatu `nazwa` (pola znanej wersji + `test_v2`; dla
// nocy wersja 3, dla pozostałych 2); false, gdy formatu nie ma w rejestrze.
static bool rejestrV2(std::vector<FormatPliku>& r, const std::string& nazwa) {
  r = rejestrFormatow();
  for (auto& f : r)
    if (f.nazwa == nazwa && f.wersja() >= 1) {
      std::vector<std::string> p = f.pola.back();
      p.push_back("test_v2");
      f.pola.push_back(p);
      return true;
    }
  return false;
}

// Liczba rekordów nocy we wszystkich plikach `/dane/noce/RRRR.csv` (bez `.tmp` i kopii `.v{n}`).
static int rekordyNocy(const AtrapaPlikow& pl) {
  int n = 0;
  for (auto& kv : pl.pliki) {
    const std::string& s = kv.first;
    if (s.compare(0, 11, "/dane/noce/") != 0 || s.size() < 4 || s.compare(s.size() - 4, 4, ".csv") != 0) continue;
    std::vector<std::string> w = wiersze(kv.second);
    for (size_t i = 0; i < w.size(); i++)
      if (!w[i].empty() && w[i][0] != '#' && i != 1) n++;
  }
  return n;
}

// ============================================================================================================
// (a) Magazyn: formaty, zapis atomowy, wiersz końca, plik tymczasowy, migracje (src/magazyn.h)
// ============================================================================================================

static void montowana(AtrapaPlikow& pl) {
  pl.montujeSie = true;
  pl.montowane = true;
}

// T-WYM-50 (część magazynu): rejestr z formatami wersji 1 z tabeli karty; po zapisie każdego pliku pierwszy wiersz
// `#cpap-eink {nazwa} 1`, drugi nazwy pól, przy przepisywanych na końcu `#koniec 1`.
static void testT_WYM_50_naglowkiPlikow() {
  const std::vector<FormatPliku>& r = rejestrFormatow();
  ROWNE_L(r.size(), 11);  // 7 formatów + `reguly` + `odpowiedzi` + `prognoza`, `obrazki`
  for (auto& e : FORMATY) {
    const FormatPliku* f = formatPliku(r, e.nazwa);
    SPRAWDZ(f != nullptr);
    if (!f) continue;
    ROWNE(f->sciezka, e.sciezka);
    ROWNE(f->sciezkaV0, e.v0);
    SPRAWDZ(f->rodzaj == e.rodzaj);
    ROWNE_L(f->wersja(), std::string(e.nazwa) == "noce" ? 3 : std::string(e.nazwa) == "sypialnia" ? 2 : 1);
    if (f->wersja() >= 1) ROWNE(polaTekst(f->pola[0]), e.pola);
  }
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  Log lg;
  Magazyn m(pl, lg.f());
  for (auto& e : FORMATY) {
    std::string s = sciezkaRoku(e.sciezka, "2026"), w = wierszFormatu(e.nazwa, 3);
    bool przep = e.rodzaj == RodzajZapisu::PRZEPISANIE;
    bool udany = przep ? m.zapisz(e.nazwa, s, {w}) : m.dopisz(e.nazwa, s, w);
    SPRAWDZ(udany);
    std::vector<std::string> t = wiersze(pl.tresc(s));
    ROWNE_L(t.size(), przep ? 4 : 3);
    if (t.size() < 3) continue;
    const FormatPliku* fz = formatPliku(r, e.nazwa);
    ROWNE(t[0], std::string("#cpap-eink ") + e.nazwa + " " + std::to_string(fz ? fz->wersja() : 1));
    ROWNE(t[1], fz ? polaTekst(fz->pola.back()) : std::string(e.pola));
    ROWNE(t[2], w);
    if (przep && t.size() == 4) ROWNE(t[3], "#koniec 1");
  }
}

// T-WYM-51, T-TEST-206 (pełna pamięć): zapis, którego nie da się dokończyć → poprzedni plik bajt w bajt, bez `.tmp`,
// wpis w logu z nazwą pliku.
static void testT_WYM_51_zapisAtomowyPelnaPamiec() {
  struct P {
    const char* format;
    const char* sciezka;
  };
  for (P x : {P{"czynniki", "/dane/czynniki.csv"}, P{"noce", "/dane/noce/2026.csv"}}) {
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    Log lg;
    Magazyn m(pl, lg.f());
    std::vector<std::string> a, b;
    for (int i = 0; i < 5; i++) a.push_back(wierszFormatu(x.format, i));
    for (int i = 0; i < 8; i++) b.push_back(wierszFormatu(x.format, 10 + i));
    SPRAWDZ(m.zapisz(x.format, x.sciezka, a));
    std::string przed = pl.tresc(x.sciezka);
    SPRAWDZ(!przed.empty());
    pl.wolneBajty = 20;
    lg.w.clear();
    SPRAWDZ(!m.zapisz(x.format, x.sciezka, b));
    ROWNE(pl.tresc(x.sciezka), przed);
    SPRAWDZ(!pl.pliki.count(std::string(x.sciezka) + ".tmp"));
    std::string krotka = std::string(x.sciezka).substr(std::string(x.sciezka).rfind('/') + 1);
    SPRAWDZ(lg.ma(krotka));
  }
}

// T-WYM-52: `#koniec 41` i 41 wierszy → wczytany; 40 wierszy → uszkodzony; brak wiersza → uszkodzony; ucięty
// w połowie → uszkodzony; plik dopisywany bez `#koniec` z urwanym ostatnim wierszem → urwany pominięty.
static void testT_WYM_52_wierszKonca() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  Log lg;
  Magazyn m(pl, lg.f());
  const std::string s = "/dane/czynniki.csv";
  std::vector<std::string> w41;
  for (int i = 0; i < 41; i++) w41.push_back(wierszFormatu("czynniki", i));
  std::string pelny = plik("czynniki", 1, POLA_CZYNNIKI, w41, true);
  pl.pliki[s] = pelny;
  OdczytPliku r = m.czytaj("czynniki", s);
  SPRAWDZ(r.stan == StanPliku::DOBRY);
  ROWNE_L(r.wersja, 1);
  SPRAWDZ(r.wiersze == w41);
  // 40 wierszy z `#koniec 41`
  std::vector<std::string> w40(w41.begin(), w41.end() - 1);
  pl.pliki[s] = plik("czynniki", 1, POLA_CZYNNIKI, w40, false) + "#koniec 41\n";
  r = m.czytaj("czynniki", s);
  SPRAWDZ(r.stan == StanPliku::USZKODZONY);
  SPRAWDZ(r.wiersze.empty());
  // bez wiersza końca
  pl.pliki[s] = plik("czynniki", 1, POLA_CZYNNIKI, w41, false);
  SPRAWDZ(m.czytaj("czynniki", s).stan == StanPliku::USZKODZONY);
  // ucięty w połowie (spec 9.2, weryfikacja T-WYM-52)
  pl.pliki[s] = pelny.substr(0, pelny.size() / 2);
  SPRAWDZ(m.czytaj("czynniki", s).stan == StanPliku::USZKODZONY);
  // plik dopisywany: urwany ostatni wiersz pominięty
  std::vector<std::string> h;
  for (int i = 0; i < 5; i++) h.push_back(wierszFormatu("humor", i));
  pl.pliki["/dane/humor.csv"] = plik("humor", 1, POLA_HUMOR, h, false) + "2026-11-04;S2A;9;D";
  r = m.czytaj("humor", "/dane/humor.csv");
  SPRAWDZ(r.stan == StanPliku::DOBRY);
  SPRAWDZ(r.wiersze == h);
}

// T-WYM-53 (gałąź bez karty, R4 karty): wybór między plikiem docelowym a `.tmp`.
static void testT_WYM_53_plikTymczasowy() {
  const std::string s = "/dane/czynniki.csv", t = s + ".tmp";
  std::vector<std::string> a, b;
  for (int i = 0; i < 6; i++) a.push_back(wierszFormatu("czynniki", i));
  for (int i = 0; i < 9; i++) b.push_back(wierszFormatu("czynniki", 20 + i));
  std::string pa = plik("czynniki", 1, POLA_CZYNNIKI, a, true), pb = plik("czynniki", 1, POLA_CZYNNIKI, b, true);
  {  // docelowy dobry, `.tmp` ucięty → docelowy, `.tmp` usunięty
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    Log lg;
    Magazyn m(pl, lg.f());
    pl.pliki[s] = pa;
    pl.pliki[t] = pb.substr(0, pb.size() - 15);
    OdczytPliku r = m.czytaj("czynniki", s);
    SPRAWDZ(r.stan == StanPliku::DOBRY && r.wiersze == a);
    SPRAWDZ(!pl.pliki.count(t));
    ROWNE(pl.tresc(s), pa);
  }
  {  // docelowego brak, `.tmp` kompletny → `.tmp` staje się docelowym
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    Log lg;
    Magazyn m(pl, lg.f());
    pl.pliki[t] = pb;
    OdczytPliku r = m.czytaj("czynniki", s);
    SPRAWDZ(r.stan == StanPliku::DOBRY && r.wiersze == b);
    SPRAWDZ(!pl.pliki.count(t));
    ROWNE(pl.tresc(s), pb);
  }
  {  // docelowy uszkodzony, `.tmp` kompletny → `.tmp`
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    Log lg;
    Magazyn m(pl, lg.f());
    pl.pliki[s] = pa.substr(0, pa.size() / 2);
    pl.pliki[t] = pb;
    OdczytPliku r = m.czytaj("czynniki", s);
    SPRAWDZ(r.stan == StanPliku::DOBRY && r.wiersze == b);
    SPRAWDZ(!pl.pliki.count(t));
    ROWNE(pl.tresc(s), pb);
  }
  {  // oba ucięte (czynniki, bez karty: R4) → wpis w logu, praca bez pliku
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    Log lg;
    Magazyn m(pl, lg.f());
    pl.pliki[s] = pa.substr(0, pa.size() / 2);
    pl.pliki[t] = pb.substr(0, pb.size() / 2);
    OdczytPliku r = m.czytaj("czynniki", s);
    SPRAWDZ(r.stan == StanPliku::USZKODZONY);
    SPRAWDZ(r.wiersze.empty());
    SPRAWDZ(lg.ma("czynniki.csv"));
  }
}

// R-NF-12, T-TEST-206: zanik zasilania w losowym bajcie zapisu (ziarno stałe), 50 razy dla każdego z trzech plików →
// po „restarcie” plik w starej albo nowej wersji, nigdy w połowie, bez `.tmp`.
static void testR_NF_12_zanikZasilania50Razy() {
  struct P {
    const char* format;
    const char* sciezka;
  };
  std::mt19937 g(20261104u);
  for (P x : {P{"noce", "/dane/noce/2026.csv"}, P{"czynniki", "/dane/czynniki.csv"},
              P{"sypialnia", "/dane/sypialnia/2026.csv"}}) {
    std::vector<std::string> a, b;
    for (int i = 0; i < 10; i++) a.push_back(wierszFormatu(x.format, i));
    for (int i = 0; i < 11; i++) b.push_back(wierszFormatu(x.format, 30 + i));
    const FormatPliku* f = formatPliku(rejestrFormatow(), x.format);
    std::string pola = f && f->wersja() >= 1 ? polaTekst(f->pola.back()) : "";
    size_t dlugosc = plik(x.format, f ? f->wersja() : 1, pola, b, true).size();
    std::uniform_int_distribution<long> los(0, (long)dlugosc + 20);
    int dobrych = 0, starych = 0, nowych = 0, zapisanych = 0;
    for (int k = 0; k < 50; k++) {
      Dziennik d;
      AtrapaPlikow pl(d);
      montowana(pl);
      Log lg;
      {
        Magazyn m(pl, lg.f());
        zapisanych += m.zapisz(x.format, x.sciezka, a);
        pl.zanikPoBajtach = los(g);
        // pierwsze trzy zaniki w stałych punktach wokół końca `.tmp`: bajt przed
        // końcem, zaraz po kompletnym `.tmp` (przed zmianą nazwy) i po zmianie nazwy (los rzadko trafia w koniec pliku)
        if (k < 3) pl.zanikPoBajtach = (long)dlugosc - 1 + k;
        m.zapisz(x.format, x.sciezka, b);
      }
      pl.zanik = false;  // restart
      pl.zanikPoBajtach = -1;
      Magazyn m2(pl, lg.f());
      OdczytPliku r = m2.czytaj(x.format, x.sciezka);
      bool stara = r.wiersze == a, nowa = r.wiersze == b;
      if (r.stan == StanPliku::DOBRY && (stara || nowa) && !pl.pliki.count(std::string(x.sciezka) + ".tmp")) dobrych++;
      starych += stara;
      nowych += nowa;
    }
    printf("  R-NF-12 %s: 50 zaników, czytelnych %d (stara wersja %d, nowa %d)\n", x.sciezka, dobrych, starych, nowych);
    ROWNE_L(zapisanych, 50);
    ROWNE_L(dobrych, 50);
    SPRAWDZ(starych >= 2 && nowych >= 1);  // U14: zanik po kompletnym `.tmp` daje starą wersję, po zmianie nazwy nową
  }
}

// Wiersz nocy wersji 1 → noc (pierwsze 20 pól parserem wersji 0); pole `import` zwrócone osobno.
static bool nocZV1(const std::string& w, Noc& n, std::string& import) {
  std::vector<std::string> f = dziel(pierwszePola(w, 21), ';');  // pola wersji 2 dalej pominięte
  if (f.size() != 21) return false;
  import = f[20];
  return parsujNoc(zlacz(f, 0, 20), 'A', n);
}

// T-WYM-55, T-TEST-207: `/noce.csv` w układach 8, 9, 12, 13, 14 i 20 pól → `/dane/noce/RRRR.csv` w wersji 1, ta sama
// liczba rekordów, te same wartości pól wspólnych, `import` puste, `/noce.csv` bez zmian.
static void testT_WYM_55_migracjaNocyKazdyUklad() {
  for (int pol : {8, 9, 12, 13, 14, 20}) {
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    Log lg;
    Magazyn m(pl, lg.f());
    std::vector<std::string> v0 = noceV0(30, "2026-11-03", pol);
    std::string zrodlo = plikV0(v0);
    pl.pliki["/noce.csv"] = zrodlo;
    SPRAWDZ(m.migrujNaNoweSciezki());
    ROWNE(pl.tresc("/noce.csv"), zrodlo);
    std::vector<std::string> t = wiersze(pl.tresc("/dane/noce/2026.csv"));
    SPRAWDZ(!t.empty() && t[0] == naglowekNoce(wersjaNoce()));
    OdczytPliku r = m.czytaj("noce", "/dane/noce/2026.csv");
    SPRAWDZ(r.stan == StanPliku::DOBRY);
    ROWNE_L(r.wiersze.size(), v0.size());
    int zgodnych = 0;
    for (size_t i = 0; i < r.wiersze.size() && i < v0.size(); i++) {
      Noc a, b;
      std::string imp = "x";
      if (parsujNoc(v0[i], 'A', a) && nocZV1(r.wiersze[i], b, imp) && zapiszNoc(a) == zapiszNoc(b) && imp.empty())
        zgodnych++;
    }
    ROWNE_L(zgodnych, v0.size());
    bool wynik = false;
    for (auto& w : m.migracje())
      if (w.plik == "noce.csv") wynik = w.ok && w.przed == 30 && w.po == 30;
    SPRAWDZ(wynik);
    if (zgodnych != (int)v0.size()) printf("  (układ %d pól)\n", pol);
  }
}

// T-WYM-55, T-TEST-207: 400 nocy z lat 2025 i 2026 → dwa pliki roczne, razem 400; potem migracja 1 → 2 (testowa
// wersja 2 z polem na końcu) → `/noce.csv` usunięty, kopie `.v1` zostają.
static void testT_WYM_55_migracja400NocyDwaLata() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  Log lg;
  Magazyn m(pl, lg.f());
  std::vector<std::string> v0 = noceV0(400, "2026-11-03");
  std::string zrodlo = plikV0(v0);
  pl.pliki["/noce.csv"] = zrodlo;
  int w2025 = 0;
  for (auto& w : v0) w2025 += w.compare(0, 4, "2025") == 0;
  SPRAWDZ(w2025 > 0 && w2025 < 400);
  SPRAWDZ(m.migrujNaNoweSciezki());
  ROWNE(pl.tresc("/noce.csv"), zrodlo);
  OdczytPliku r25 = m.czytaj("noce", "/dane/noce/2025.csv"), r26 = m.czytaj("noce", "/dane/noce/2026.csv");
  ROWNE_L(r25.wiersze.size(), w2025);
  ROWNE_L(r26.wiersze.size(), 400 - w2025);
  ROWNE_L(rekordyNocy(pl), 400);
  bool lataZgodne = true;
  for (auto& w : r25.wiersze) lataZgodne = lataZgodne && w.compare(0, 4, "2025") == 0;
  for (auto& w : r26.wiersze) lataZgodne = lataZgodne && w.compare(0, 4, "2026") == 0;
  SPRAWDZ(lataZgodne);
  // migracja w miejscu 1 → 2
  std::vector<FormatPliku> rej;
  SPRAWDZ(rejestrV2(rej, "noce"));
  std::string v1_25 = pl.tresc("/dane/noce/2025.csv"), v1_26 = pl.tresc("/dane/noce/2026.csv");
  Magazyn m2(pl, lg.f(), rej);
  SPRAWDZ(m2.migrujWMiejscu());
  SPRAWDZ(!pl.pliki.count("/noce.csv"));
  std::string kopia = ".v" + std::to_string(wersjaNoce());
  ROWNE(pl.tresc("/dane/noce/2025.csv" + kopia), v1_25);
  ROWNE(pl.tresc("/dane/noce/2026.csv" + kopia), v1_26);
  std::vector<std::string> t = wiersze(pl.tresc("/dane/noce/2025.csv"));
  SPRAWDZ(t.size() >= 3 && t[0] == naglowekNoce(wersjaNoce() + 1) && t[1] == polaNoceZnane() + ";test_v2");
  OdczytPliku n25 = m2.czytaj("noce", "/dane/noce/2025.csv");
  SPRAWDZ(n25.stan == StanPliku::DOBRY && n25.wersja == wersjaNoce() + 1);
  ROWNE_L(n25.wiersze.size(), w2025);
  bool pustePole = !n25.wiersze.empty();
  for (size_t i = 0; i < n25.wiersze.size() && i < r25.wiersze.size(); i++)
    pustePole = pustePole && n25.wiersze[i] == r25.wiersze[i] + ";";
  SPRAWDZ(pustePole);
}

// T-WYM-55, T-TEST-207: `/sypialnia.csv` z 3 i 4 polami + `/sypialnia-co2.csv` → `/dane/sypialnia/RRRR.csv`,
// połączone po dacie; liczba nocy = liczba unikalnych dat; stare pliki nietknięte.
static void testT_WYM_55_migracjaSypialni() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  Log lg;
  Magazyn m(pl, lg.f());
  std::vector<std::string> syp, co2;
  // 4 pola (wczesna wersja prototypowa): 29–31.12.2025, z CO2
  for (int i = 0; i < 3; i++) syp.push_back(dodajDni("2025-12-29", i) + ";18.5;50;9" + std::to_string(i) + "0");
  // 3 pola: 1–10.10.2026
  for (int i = 0; i < 10; i++) syp.push_back(zapiszSypialnie(NocSypialni{dodajDni("2026-10-01", i), 20.0f + (float)i * 0.1f, 45.0f}));
  // CO2: 6–15.10.2026 (5 dat wspólnych z plikiem 3 pól, 5 nowych)
  for (int i = 0; i < 10; i++) co2.push_back(dodajDni("2026-10-06", i) + ";" + std::to_string(800 + i * 10));
  std::string zs = plikV0(syp), zc = plikV0(co2);
  pl.pliki["/sypialnia.csv"] = zs;
  pl.pliki["/sypialnia-co2.csv"] = zc;
  SPRAWDZ(m.migrujNaNoweSciezki());
  ROWNE(pl.tresc("/sypialnia.csv"), zs);
  ROWNE(pl.tresc("/sypialnia-co2.csv"), zc);
  OdczytPliku r25 = m.czytaj("sypialnia", "/dane/sypialnia/2025.csv");
  OdczytPliku r26 = m.czytaj("sypialnia", "/dane/sypialnia/2026.csv");
  SPRAWDZ(r25.stan == StanPliku::DOBRY && r26.stan == StanPliku::DOBRY);
  ROWNE_L(r25.wiersze.size(), 3);
  ROWNE_L(r26.wiersze.size(), 15);  // 10 z czujnika + 5 dat tylko z CO2
  std::map<std::string, NocSypialni> po;
  for (auto* r : {&r25, &r26})
    for (auto& w : r->wiersze) {
      NocSypialni n;
      if (parsujSypialnie(w, n)) po[n.data] = n;
    }
  ROWNE_L(po.size(), 18);
  auto jest = [&](const std::string& data) { return po.count(data) ? po[data] : NocSypialni{"", NAN, NAN}; };
  NocSypialni a = jest("2026-10-07"), b = jest("2026-10-13"), c = jest("2025-12-30"), e = jest("2026-10-02");
  SPRAWDZ(std::fabs(a.t - 20.6f) < 0.01f && std::fabs(a.w - 45.0f) < 0.01f && std::fabs(a.co2 - 810.0f) < 0.5f);
  SPRAWDZ(std::isnan(b.t) && std::fabs(b.co2 - 870.0f) < 0.5f);
  SPRAWDZ(std::fabs(c.t - 18.5f) < 0.01f && std::fabs(c.co2 - 910.0f) < 0.5f);
  SPRAWDZ(std::fabs(e.t - 20.1f) < 0.01f && std::isnan(e.co2));
}

// T-WYM-55, T-TEST-207: czynniki, humor, bateria, ab_pominiete, ab_proby → nowe ścieżki, liczby rekordów i wiersze
// 1:1 (9.1: bez zmiany pól), stare pliki nietknięte.
static void testT_WYM_55_migracjaPozostalych() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  Log lg;
  Magazyn m(pl, lg.f());
  struct P {
    const char* format;
    const char* stara;
    const char* nowa;
    int ile;
  };
  const P pliki[] = {{"czynniki", "/czynniki.csv", "/dane/czynniki.csv", 12},
                     {"humor", "/humor.csv", "/dane/humor.csv", 15},
                     {"wybudzenia", "/bateria.csv", "/log/wybudzenia.csv", 20},
                     {"pominiete", "/ab_pominiete.txt", "/dane/ab-pominiete.csv", 4},
                     {"proby", "/ab_proby.txt", "/dane/ab-proby.csv", 3}};
  std::map<std::string, std::vector<std::string>> zrodla;
  for (auto& x : pliki) {
    std::vector<std::string> w;
    for (int i = 0; i < x.ile; i++) w.push_back(wierszFormatu(x.format, i));
    zrodla[x.format] = w;
    pl.pliki[x.stara] = plikV0(w);
  }
  SPRAWDZ(m.migrujNaNoweSciezki());
  for (auto& x : pliki) {
    ROWNE(pl.tresc(x.stara), plikV0(zrodla[x.format]));
    std::vector<std::string> t = wiersze(pl.tresc(x.nowa));
    SPRAWDZ(!t.empty() && t[0] == std::string("#cpap-eink ") + x.format + " 1");
    OdczytPliku r = m.czytaj(x.format, x.nowa);
    SPRAWDZ(r.stan == StanPliku::DOBRY);
    ROWNE_L(r.wiersze.size(), x.ile);
    SPRAWDZ(r.wiersze == zrodla[x.format]);
  }
}

// T-WYM-57: pola wersji 1 każdego formatu z rejestru są początkiem listy wersji 2 testowej; plik wersji 2 czytany
// przez firmware znający wersję 1 → te same wartości pól wspólnych (stan NOWSZY), a znający wersję 2 → całe wiersze.
static void testT_WYM_57_poleTylkoNaKoncu() {
  const std::vector<FormatPliku>& r = rejestrFormatow();
  ROWNE_L(r.size(), 11);  // 7 formatów + `reguly` + `odpowiedzi` + `prognoza`, `obrazki`
  for (auto& f : r) {
    std::vector<FormatPliku> rej;
    SPRAWDZ(rejestrV2(rej, f.nazwa));
    const FormatPliku* f2 = formatPliku(rej, f.nazwa);
    if (!f2 || f2->wersja() != f.wersja() + 1) continue;
    SPRAWDZ(std::equal(f2->pola[0].begin(), f2->pola[0].end(), f2->pola.back().begin()));
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    Log lg;
    std::string s = sciezkaRoku(f.sciezka, "2026");
    std::vector<std::string> w1, w2;
    for (int i = 0; i < 3; i++) {
      w1.push_back(wierszFormatu(f.nazwa, i));
      w2.push_back(w1.back() + ";X" + std::to_string(i));
    }
    pl.pliki[s] = plik(f.nazwa, f2->wersja(), polaTekst(f2->pola.back()), w2, f.rodzaj == RodzajZapisu::PRZEPISANIE);
    Magazyn m1(pl, lg.f());
    OdczytPliku o1 = m1.czytaj(f.nazwa, s);
    SPRAWDZ(o1.stan == StanPliku::NOWSZY && o1.wersja == f2->wersja());
    SPRAWDZ(o1.wiersze == w1);
    SPRAWDZ(m1.tylkoDoOdczytu(s));
    Magazyn m2(pl, lg.f(), rej);
    OdczytPliku o2 = m2.czytaj(f.nazwa, s);
    SPRAWDZ(o2.stan == StanPliku::DOBRY && o2.wiersze == w2);
  }
}

// T-WYM-58 (warstwa nad adapterem): ponowny zapis tej samej wartości → 0 zapisów w adapterze; zmiana → 1.
static void testT_WYM_58_warstwaNvs() {
  Dziennik d;
  AtrapaNvs n(d);
  NvsTylkoZmiany w(n);
  w.ustawTekst("cpap", "ab_rano", "2026-11-04");
  int z = n.zapisy;
  ROWNE_L(z, 1);
  w.ustawTekst("cpap", "ab_rano", "2026-11-04");
  ROWNE_L(n.zapisy, z);
  StanAB a;
  a.magia = MAGIA_AB;
  w.ustawBajty("noc", "ab", &a, sizeof a);
  z = n.zapisy;
  w.ustawBajty("noc", "ab", &a, sizeof a);
  ROWNE_L(n.zapisy, z);
  a.zalegle = 3;  // zmiana zaległych dób
  w.ustawBajty("noc", "ab", &a, sizeof a);
  ROWNE_L(n.zapisy, z + 1);
  w.ustawLiczbeU("cpap", "ziarno", 7);
  z = n.zapisy;
  w.ustawLiczbeU("cpap", "ziarno", 7);
  w.ustawLiczbe("cpap", "ab_zal", -1);
  w.ustawLiczbe("cpap", "ab_zal", -1);
  w.ustawFlage("cpap", "noc_s9", false);
  w.ustawFlage("cpap", "noc_s9", false);
  ROWNE_L(n.zapisy, z + 2);  // pierwsze ab_zal i pierwsze noc_s9 (brak klucza = zmiana)
}

// T-WYM-38: tablica partycji (8.2) i wariant zapasowy do 16 MB (R6) z plików w repozytorium.
struct Partycja {
  std::string nazwa, typ, podtyp;
  long off = -1, rozmiar = -1;
};
static long liczbaTablicy(std::string t) {
  while (!t.empty() && (t[0] == ' ' || t[0] == '\t')) t.erase(0, 1);
  while (!t.empty() && (t.back() == ' ' || t.back() == '\t' || t.back() == '\r')) t.pop_back();
  if (t.empty()) return -1;
  long mn = 1;
  if (t.back() == 'K' || t.back() == 'k') mn = 1024;
  if (t.back() == 'M' || t.back() == 'm') mn = 1024 * 1024;
  if (mn > 1) t.pop_back();
  char* k = nullptr;
  long v = strtol(t.c_str(), &k, 0);
  if (!k || *k) return -1;
  return v * mn;
}
static bool czytajTablice(const std::string& sciezka, std::vector<Partycja>& v) {
  std::ifstream f(sciezka);
  if (!f) return false;
  std::string l;
  while (std::getline(f, l)) {
    if (l.empty() || l[0] == '#') continue;
    std::vector<std::string> p = dziel(l, ',');
    if (p.size() < 5) continue;
    auto czysty = [](std::string s) {
      s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return c == ' ' || c == '\t' || c == '\r'; }), s.end());
      return s;
    };
    Partycja x;
    x.nazwa = czysty(p[0]);
    x.typ = czysty(p[1]);
    x.podtyp = czysty(p[2]);
    x.off = liczbaTablicy(p[3]);
    x.rozmiar = liczbaTablicy(p[4]);
    v.push_back(x);
  }
  return !v.empty();
}
static const Partycja* partycjaONazwie(const std::vector<Partycja>& v, const std::string& n) {
  for (auto& x : v)
    if (x.nazwa == n) return &x;
  return nullptr;
}
static void sprawdzTablice(const std::string& sciezka, long koniecPlikow) {
  std::vector<Partycja> v;
  bool jest = czytajTablice(sciezka, v);
  if (!jest) printf("  T-WYM-38: brak albo pusty plik %s\n", sciezka.c_str());
  SPRAWDZ(jest);
  if (!jest) return;
  const Partycja *nvs = partycjaONazwie(v, "nvs"), *ota = partycjaONazwie(v, "otadata"),
                 *a0 = partycjaONazwie(v, "app0"), *a1 = partycjaONazwie(v, "app1"),
                 *cd = partycjaONazwie(v, "coredump"), *fs = partycjaONazwie(v, "spiffs");
  SPRAWDZ(nvs && ota && a0 && a1 && cd && fs);
  if (!(nvs && ota && a0 && a1 && cd && fs)) return;
  SPRAWDZ(nvs->typ == "data" && nvs->podtyp == "nvs" && nvs->off == 0x9000 && nvs->rozmiar == 0x5000);
  SPRAWDZ(ota->typ == "data" && ota->podtyp == "ota" && ota->off == 0xE000 && ota->rozmiar == 0x2000);
  SPRAWDZ(a0->typ == "app" && a0->podtyp == "ota_0" && a0->off == 0x10000 && a0->rozmiar == 0x400000);
  SPRAWDZ(a1->typ == "app" && a1->podtyp == "ota_1" && a1->off == 0x410000 && a1->rozmiar == 0x400000);
  SPRAWDZ(cd->podtyp == "coredump" && cd->off >= 0 && cd->off + cd->rozmiar <= 0x1000000);
  SPRAWDZ(a0->off + a0->rozmiar <= 0x1000000 && a1->off + a1->rozmiar <= 0x1000000);
  SPRAWDZ(fs->typ == "data" && fs->off == 0x820000 && fs->off + fs->rozmiar == koniecPlikow);
  // bez nakładania, wszystko po tablicy partycji (0x8000 + 0x1000)
  std::vector<Partycja> s = v;
  std::sort(s.begin(), s.end(), [](const Partycja& x, const Partycja& y) { return x.off < y.off; });
  bool rozlaczne = true;
  for (size_t i = 0; i < s.size(); i++) {
    rozlaczne = rozlaczne && s[i].off >= 0x9000 && s[i].rozmiar > 0;
    if (i + 1 < s.size()) rozlaczne = rozlaczne && s[i].off + s[i].rozmiar <= s[i + 1].off;
  }
  SPRAWDZ(rozlaczne);
  SPRAWDZ(s.back().off + s.back().rozmiar <= 0x2000000);
}
static void testT_WYM_38_tablicaPartycji() {
  std::string t = __FILE__;
  std::string fw = t.substr(0, t.rfind('/')) + "/../../";  // firmware/prototyp/test → firmware/
  sprawdzTablice(fw + "partycje-e1001.csv", 0x2000000);
  sprawdzTablice(fw + "partycje-e1001-16mb.csv", 0x1000000);
}

// R-NF-27, T-TEST-204: funkcja sprawdzająca adres źródła otoczenia (bez formularza i pliku: R5 karty).
static void testR_NF_27_adresyLokalne() {
  for (const char* u : {"http://10.0.0.5/o", "http://172.16.0.1/o", "http://172.31.255.254/o",
                        "http://192.168.1.60:8080/otoczenie", "http://169.254.10.1/o", "http://ha.local/o",
                        "http://10.0.0.7/o", "http://172.20.1.2/o", "http://dom.local/o"}) {
    bool w = adresWSieciLokalnej(u);
    if (!w) printf("  odrzucony, a powinien być przyjęty: %s\n", u);
    SPRAWDZ(w);
  }
  for (const char* u : {"http://172.32.0.1/o", "http://203.0.113.5/otoczenie", "http://8.8.8.8/o", "http://ha.lan/o",
                        "http://192.168.1.60.nip.io/o", "https://192.168.1.60/o", "", "192.168.1.60/o",
                        "10.0.0.5"}) {
    bool w = adresWSieciLokalnej(u);
    if (w) printf("  przyjęty, a powinien być odrzucony: \"%s\"\n", u);
    SPRAWDZ(!w);
  }
  // host jak HTTPClient (do pierwszego `/`), znaki niejednoznaczne,
  // zapisy IP inne niż cztery liczby dziesiętne, IPv6, końcowa kropka, `localhost`, wielkie litery w schemacie.
  for (const char* u : {"http://192.168.1.60?@203.0.113.5/otoczenie", "http://192.168.1.60#@203.0.113.5/o",
                        "http://dom.local?x=@8.8.8.8/o", "http://192.168.1.60?x/o", "http://192.168.1.60#x",
                        "http://a@192.168.1.60/o", "http://192.168.1.60/o?u=a@b", "http://192.168.1.60\\@8.8.8.8/o",
                        "http://192.168.1.60 /o", "http://192.168.1.60/o p", "http://192.168.1.60\t/o",
                        "http://dom.local\x7f/o", "http://d\xc3\xb3m.local/o", "HTTP://192.168.1.60/o",
                        "Http://192.168.1.60/o", "http://010.0.0.5/o", "http://0300.0250.1.1/o", "http://0xC0A80101/o",
                        "http://0xc0.0xa8.1.1/o", "http://3232235777/o", "http://192.168.1/o", "http://192.168.1.60./o",
                        "http://ha.local./o", "http://[fe80::1]/o", "http://fe80::1/o", "http://192.168.1.60:80:81/o",
                        "http://localhost/o", "http://localhost:8080/o", "http://LOCALHOST/o", "http://.local/o",
                        "http://192.168.001.060/o", "http://192.168.1.256/o", "http://192.168.1.60:0/o",
                        "http://192.168.1.60:65536/o"}) {
    bool w = adresWSieciLokalnej(u);
    if (w) printf("  przyjęty, a powinien być odrzucony (U1): \"%s\"\n", u);
    SPRAWDZ(!w);
  }
  for (const char* u : {"http://HA.LOCAL/o", "http://Dom.Local:8123/api", "http://192.168.0.1", "http://10.0.0.0/o?a=1#b",
                        "http://192.168.1.60:8080/o?x=1&y=2"}) {
    bool w = adresWSieciLokalnej(u);
    if (!w) printf("  odrzucony, a powinien być przyjęty (U1): %s\n", u);
    SPRAWDZ(w);
  }
}

// ============================================================================================================
// (b) Dyrygent z atrapami (test/atrapy/adaptery_atrapy.h)
// ============================================================================================================

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_BLOKADA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const long DOBA_A = nrDnia("2026-11-03");  // noc z poranka 4.11.2026
static const char* R_AW_16 = "Błąd pamięci panelu. Wyniki mogą być niepełne.";

// Raport doby i lista nocy jak w test/testy_slot_dzienny_w_planie.cpp (dane syntetyczne).
static std::string raport(long day, uint32_t startEp, uint32_t koniecEp) {
  std::string d = std::to_string(day);
  std::string s = std::to_string((unsigned long long)startEp * 1000ULL);
  std::string k = std::to_string((unsigned long long)koniecEp * 1000ULL);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":" + s + ",\"end_ms\":" + k + "}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25200,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Press\",\"label\":\"C\",\"unit\":\"cmH2O\",\"count\":12600,"
         "\"coverage_seconds\":25200,\"min\":6.2,\"mean\":10.1,\"median\":9.8,\"p95\":13.6,\"max\":16.0,"
         "\"source\":\"edf\"},{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12600,"
         "\"coverage_seconds\":25200,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
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

// Atrapa sieci z chwilą (ms zegara procesora) pierwszego wywołania w wybudzeniu: okno przywracania bez sieci.
class SiecP03 : public AtrapaSieci {
 public:
  SiecP03(Dziennik& d, AtrapaZegara& z) : AtrapaSieci(d, z), zg_(z) {}
  uint32_t pierwszeMs = UINT32_MAX;
  bool polacz(uint32_t limitMs, const std::function<bool()>& p, bool& przerwane) override {
    notuj();
    return AtrapaSieci::polacz(limitMs, p, przerwane);
  }
  bool czasZSieci(uint32_t limitMs) override {
    notuj();
    return AtrapaSieci::czasZSieci(limitMs);
  }
  OdpowiedzHttp get(const ZadanieHttp& z) override {
    notuj();
    return AtrapaSieci::get(z);
  }
  bool otworzOknoSerwisowe() override {
    notuj();
    return AtrapaSieci::otworzOknoSerwisowe();
  }
 private:
  void notuj() {
    if (pierwszeMs == UINT32_MAX) pierwszeMs = zg_.msTeraz;
  }
  AtrapaZegara& zg_;
};

struct PanelP03 {
  Panel p;
  SiecP03 siec{p.d, p.zegar};
  KonfiguracjaPanelu konf = KONF;
  PanelP03() { zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji)); }
  Adaptery adaptery() {
    return Adaptery{p.zegar, p.nvs, p.pliki, siec, p.ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik,
                    p.sen, p.port, p.diag};
  }
};

// Jedno wybudzenie o czasie unix `t`. START = włączenie zasilania (pamięć RTC od nowa, jak adapter ESP32).
static TekstyWybudzenia wybudz(PanelP03& s, uint32_t t, PowodWybudzenia pw) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  if (pw == PowodWybudzenia::START) s.p.start.stan = StanRtc();
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.siec.polaczonaTeraz = false;
  s.siec.okno = false;
  s.siec.pierwszeMs = UINT32_MAX;
  s.p.d.z.clear();
  s.p.port.log.clear();
  Dyrygent dy(s.adaptery(), s.konf);
  dy.wybudzenie();
  return dy.teksty();
}

static int minutaLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  return l.tm_hour * 60 + l.tm_min;
}
struct Wyb {
  uint32_t ep = 0;
  int minuta = 0;
  std::vector<std::string> zdarzenia;
  TekstyWybudzenia t;
};
using Odpowiedzi = void (*)(PanelP03&, uint32_t);
// Kolejne wybudzenia od włączenia zasilania o `od` do `doT`, każde po śnie z planu dyrygenta (jak symuluj).
static std::vector<Wyb> symuluj(PanelP03& s, uint32_t od, uint32_t doT, Odpowiedzi odp) {
  std::vector<Wyb> v;
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 800 && t < doT; i++) {
    odp(s, t);
    int snow = s.p.sen.snow;
    Wyb w;
    w.t = wybudz(s, t, pw);
    w.ep = t;
    w.minuta = minutaLok(t);
    w.zdarzenia = s.p.d.z;
    v.push_back(w);
    if (s.p.sen.snow != snow + 1) break;
    uint32_t snu = s.p.sen.plan.budzZegarem ? s.p.sen.plan.sekund : OKRES_SNU_S;
    if (snu == 0) snu = 1;
    t += s.p.zegar.msTeraz / 1000 + snu;
    pw = PowodWybudzenia::ZEGAR;
  }
  return v;
}

// AirBridge: noc 3/4.11.2026 od 06:40 (jak odpowiedziSlotu), wcześniej terapia trwa.
static void odpowiedziDoby(PanelP03& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  if (t >= ep(2026, 11, 4, 6, 40) && t < ep(2026, 11, 5, 0, 0)) {
    s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
    s.siec.dodaj(sciezkaNocy(DOBA_A), 200, raport(DOBA_A, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40)));
  } else {
    s.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  }
}
static void blokada(PanelP03& s, uint32_t) {
  s.siec.odpowiedzi.clear();
  s.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
}

// Partycja z nocami wersji 1: `n` nocy do poranka 3.11.2026 w `/dane/noce/2026.csv`.
static std::string zNocami(PanelP03& s, int n) {
  std::string t = plikNoce(noceZnane(n, "2026-11-03"));  // znana wersja (2)
  s.p.pliki.pliki["/dane/noce/2026.csv"] = t;
  return t;
}
static int indeksZawiera(const std::vector<std::string>& z, const std::string& p, bool ostatni) {
  int w = -1;
  for (size_t i = 0; i < z.size(); i++)
    if (z[i].find(p) != std::string::npos) {
      w = (int)i;
      if (!ostatni) return w;
    }
  return w;
}
static bool logMa(const PanelP03& s, const std::string& p) { return zawiera(s.p.port.log, p); }
static void ustawZnacznik(PanelP03& s) {
  s.p.nvs.teksty[std::string("cpap/") + NVS_KLUCZ_FS_INIT] = znacznikPartycji(s.p.pliki.opis);
  s.p.nvs.liczbyU[std::string("cpap/") + NVS_KLUCZ_UKLAD] = ukladWkompilowany();
}
static std::string smieci(uint32_t ziarno) {
  std::mt19937 g(ziarno);
  std::string b(2 * AtrapaPlikow::BLOK, '\0');
  for (auto& c : b) c = (char)(g() % 250);  // bez 0xFF w całości
  size_t i;
  while ((i = b.find("littlefs")) != std::string::npos) b[i] = 'X';
  return b;
}
// Partycja nowa (8.9 krok 3): obszar skasowany, NVS bez znacznika i bez identyfikatora układu.
static void partycjaSkasowana(PanelP03& s) {
  s.p.pliki.pliki.clear();
  s.p.pliki.montujeSie = false;
  s.p.pliki.poczatek = std::string(2 * AtrapaPlikow::BLOK, '\xff');
}

// R-NF-27, T-TEST-204: adres spoza sieci → 0 żądań do źródła, token nigdzie, ekran testu `adres poza siecią domową`,
// bez ponowień (także w kolejnym wybudzeniu).
static void testR_NF_27_bezZadaniaPozaSiecia() {
  PanelP03 s;
  zNocami(s, 30);
  const std::string token = "TAJNY-TOKEN-P03";
  s.konf.otoUrl = "http://203.0.113.5/otoczenie";
  s.konf.otoToken = token;
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"ostrzezenia\":[]}");
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  TekstyWybudzenia t1 = wybudz(s, ep(2026, 11, 4, 12, 0), PowodWybudzenia::START);
  std::vector<std::string> log1 = s.p.port.log;
  TekstyWybudzenia t2 = wybudz(s, ep(2026, 11, 4, 12, 30), PowodWybudzenia::START);
  SPRAWDZ(!s.siec.zadania.empty());  // AirBridge pytany jak zwykle
  SPRAWDZ(!zawiera(s.siec.zadania, "203.0.113.5"));
  bool tokenWyslany = false;
  for (auto& n : s.siec.naglowki)
    for (auto& h : n) tokenWyslany = tokenWyslany || h.second.find(token) != std::string::npos;
  SPRAWDZ(!tokenWyslany);
  SPRAWDZ(!zawiera(log1, token) && !zawiera(s.p.port.log, token));
  SPRAWDZ(!zawiera(t1.linieTestu, token) && !zawiera(t1.diagnostykaTestu, token));
  SPRAWDZ(zawiera(t1.linieTestu, "Źródło otoczenia: poza siecią domową"));  // U-05: wariant mieszczący się w kolumnie
  SPRAWDZ(zawiera(t1.diagnostykaTestu, "adres poza siecią domową"));
  SPRAWDZ(zawiera(t2.linieTestu, "Źródło otoczenia: poza siecią domową") && zawiera(t2.diagnostykaTestu, "adres poza siecią domową"));
}

// R-NF-27, R-AW-25, T-TEST-204: źródło w sieci lokalnej odpowiada 301 albo 302 → jedno żądanie bez drugiego, odmowa
// bez ponowień, w logu `HTTP 30x`, ekran testu `przekierowanie`.
static void testR_NF_27_przekierowanie() {
  for (int kod : {301, 302}) {
    PanelP03 s;
    zNocami(s, 30);
    s.konf.otoUrl = "http://192.168.1.60:8080/otoczenie";
    s.konf.otoToken = "t";
    s.siec.dodaj("/otoczenie", kod, "");
    s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 12, 0), PowodWybudzenia::START);
    int doZrodla = 0;
    for (auto& u : s.siec.zadania) doZrodla += AtrapaSieci::sciezka(u).compare(0, 10, "/otoczenie") == 0;
    ROWNE_L(doZrodla, 1);
    SPRAWDZ(logMa(s, "HTTP " + std::to_string(kod)));
    SPRAWDZ(zawiera(t.linieTestu, "Źródło otoczenia: przekierowanie"));
  }
}

// T-WYM-50 (część dyrygenta): po pracy panelu NVS `cpap`/`wersja` i `noc`/`wersja` = 1, a każdy plik danych zaczyna
// się wierszem `#cpap-eink {nazwa} {wersja}`.
static void testT_WYM_50_nvsIPlikiPoPracy() {
  PanelP03 s;
  s.p.pliki.montujeSie = true;
  zNocami(s, 30);
  symuluj(s, ep(2026, 11, 4, 6, 30), ep(2026, 11, 4, 8, 30), odpowiedziDoby);
  ROWNE_L(s.p.nvs.liczbaU("cpap", NVS_KLUCZ_WERSJA, 0), WERSJA_NVS_CPAP);
  ROWNE_L(s.p.nvs.liczbaU("noc", NVS_KLUCZ_WERSJA, 0), WERSJA_NVS_NOC);
  ROWNE_L(rekordyNocy(s.p.pliki), 31);  // noc z poranka 4.11 dopisana
  int bezNaglowka = 0;
  for (auto& kv : s.p.pliki.pliki) {
    if (kv.first.compare(0, 14, "/przywracanie/") == 0 || kv.first.compare(0, 7, "/import") == 0) continue;
    if (kv.second.compare(0, 11, "#cpap-eink ") != 0) {
      bezNaglowka++;
      printf("  T-WYM-50: plik bez nagłówka: %s\n", kv.first.c_str());
    }
  }
  ROWNE_L(bezNaglowka, 0);
}

// T-WYM-56, T-TEST-207: atrapa odmawia zapisu w połowie migracji (rok 2026) → stare pliki nietknięte, noce wczytane
// parserem wersji 0, ekran testu `migracja noce.csv: błąd`, wpis w logu; restart z działającą atrapą → migracja udana.
static void testT_WYM_56_migracjaPrzerwana() {
  PanelP03 s;
  std::string zrodlo = plikV0(noceV0(400, "2026-11-03"));
  s.p.pliki.pliki["/noce.csv"] = zrodlo;
  s.siec.siecJest = false;
  s.p.pliki.odmawiaZapisuPod = "/dane/noce/2026";
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  ROWNE(s.p.pliki.tresc("/noce.csv"), zrodlo);
  SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: migracja noce: błąd") && zawiera(t.diagnostykaTestu, "migracja noce.csv: błąd"));
  SPRAWDZ(zawiera(t.diagnostykaTestu, "historia 400 nocy"));
  bool wpis = false;
  for (auto& l : s.p.port.log) wpis = wpis || (l.find("migracja") != std::string::npos && l.find("noce.csv") != std::string::npos);
  SPRAWDZ(wpis);
  s.p.pliki.odmawiaZapisuPod = "";
  TekstyWybudzenia t2 = wybudz(s, ep(2026, 11, 4, 9, 30), PowodWybudzenia::START);
  ROWNE_L(rekordyNocy(s.p.pliki), 400);
  SPRAWDZ(!zawiera(t2.linieTestu, "migracja noce") && !zawiera(t2.diagnostykaTestu, "migracja noce.csv: błąd"));
  ROWNE(s.p.pliki.tresc("/noce.csv"), zrodlo);
}

// T-WYM-54, T-TEST-208: `czynniki.csv` w wersji 2, firmware zna 1 → czynniki wczytane, po dobie symulowanej pracy
// plik bajt w bajt, ekran testu `dane z nowszej wersji: czynniki.csv`.
static void testT_WYM_54_nowszyPlik() {
  PanelP03 s;
  zNocami(s, 30);
  std::vector<std::string> w;
  for (int i = 0; i < 5; i++) w.push_back(zapiszWpis(WpisCzynnikow{dodajDni("2026-10-28", i), 'O', CZ_STRES, 'K', -1, true}) + ";X");
  std::string tresc = plik("czynniki", 2, std::string(POLA_CZYNNIKI) + ";test_v2", w, true);
  s.p.pliki.pliki["/dane/czynniki.csv"] = tresc;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 23, 55), odpowiedziDoby);
  SPRAWDZ(v.size() > 10);
  ROWNE(s.p.pliki.tresc("/dane/czynniki.csv"), tresc);
  SPRAWDZ(!v.empty() && zawiera(v.back().t.linieTestu, "Pamięć panelu: dane z nowszej wersji") &&
          zawiera(v.back().t.diagnostykaTestu, "dane z nowszej wersji: czynniki.csv"));
  // pierwsze wybudzenie (00:05): czynniki z pliku, przed jakimkolwiek pytaniem tej doby
  SPRAWDZ(!v.empty() && zawiera(v.front().t.diagnostykaTestu, "Czynniki: 5 nocy"));
}

// R-NF-28, T-TEST-208: `noce/2026.csv` w wersji 2 → noce wczytane z pól znanych, plik bez zmian po pobraniu nowej
// nocy, ekran testu `dane z nowszej wersji`; „powrót” do firmware znającego wersję 2 → dane kompletne.
static void testR_NF_28_nowszyPlikNocy() {
  PanelP03 s;
  std::vector<std::string> w = noceZnane(30, "2026-11-03");
  for (auto& x : w) x += ";X";  // pole `test_v2` wypełnione (wersja o jeden wyższa niż znana)
  std::string tresc = plik("noce", wersjaNoce() + 1, polaNoceZnane() + ";test_v2", w, true);
  s.p.pliki.pliki["/dane/noce/2026.csv"] = tresc;
  odpowiedziDoby(s, ep(2026, 11, 4, 7, 0));
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(zawiera(s.siec.zadania, "day=" + std::to_string(DOBA_A)));  // nowa noc pobrana
  ROWNE(s.p.pliki.tresc("/dane/noce/2026.csv"), tresc);
  SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: dane z nowszej wersji"));
  SPRAWDZ(zawiera(t.diagnostykaTestu, "historia 3"));  // 30 z pliku (+ nowa noc z pamięci)
  // powrót do firmware znającego wersję 2
  std::vector<FormatPliku> rej;
  SPRAWDZ(rejestrV2(rej, "noce"));
  s.konf.rejestr = &rej;
  s.siec.siecJest = false;
  TekstyWybudzenia t2 = wybudz(s, ep(2026, 11, 4, 7, 30), PowodWybudzenia::START);
  SPRAWDZ(!zawiera(t2.linieTestu, "dane z nowszej wersji") && !zawiera(t2.diagnostykaTestu, "dane z nowszej wersji"));
  SPRAWDZ(zawiera(t2.diagnostykaTestu, "historia 30 nocy"));
  Log lg;
  Magazyn m(s.p.pliki, lg.f(), rej);
  OdczytPliku r = m.czytaj("noce", "/dane/noce/2026.csv");
  SPRAWDZ(r.stan == StanPliku::DOBRY);
  std::set<std::string> po(r.wiersze.begin(), r.wiersze.end());
  int zachowane = 0;
  for (auto& x : w) zachowane += po.count(x) ? 1 : 0;
  ROWNE_L(zachowane, 30);
}

// T-WYM-43, T-TEST-274: obraz z `/noce.csv` (400 nocy), stan „do sprawdzenia”, samosprawdzenie zdane → pliki wersji 1
// zapisane przed oznaczeniem wersji, `/noce.csv` bajt w bajt.
static void testT_WYM_43_migracjaPrzedOznaczeniem() {
  PanelP03 s;
  std::string zrodlo = plikV0(noceV0(400, "2026-11-03"));
  s.p.pliki.pliki["/noce.csv"] = zrodlo;
  s.p.start.stanW = StanWersji::DO_SPRAWDZENIA;
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(s.p.start.oznaczona && !s.p.start.powrot);
  ROWNE(s.p.pliki.tresc("/noce.csv"), zrodlo);
  SPRAWDZ(s.p.pliki.tresc("/dane/noce/2025.csv").compare(0, 18, naglowekNoce(wersjaNoce()) + "\n") == 0);
  SPRAWDZ(s.p.pliki.tresc("/dane/noce/2026.csv").compare(0, 18, naglowekNoce(wersjaNoce()) + "\n") == 0);
  ROWNE_L(rekordyNocy(s.p.pliki), 400);
  // oba pliki roczne powstały (zmiana nazwy z `.tmp`, T-WYM-51) przed oznaczeniem wersji
  int oznacz = indeksZawiera(s.p.d.z, "start.oznacz", false);
  int p25 = indeksZawiera(s.p.d.z, "pliki.nazwa /dane/noce/2025.csv.tmp", false);
  int p26 = indeksZawiera(s.p.d.z, "pliki.nazwa /dane/noce/2026.csv.tmp", false);
  SPRAWDZ(p25 >= 0 && p26 >= 0 && oznacz > p25 && oznacz > p26);
}

// T-WYM-43, T-WYM-41, T-TEST-274: ta sama migracja, atrapa odmawia zapisu nowych plików → powrót do poprzedniej
// wersji, `/noce.csv` bez zmian i czytelny parserem wersji 0.
static void testT_WYM_43_odmowaZapisuPowrot() {
  PanelP03 s;
  std::vector<std::string> v0 = noceV0(400, "2026-11-03");
  std::string zrodlo = plikV0(v0);
  s.p.pliki.pliki["/noce.csv"] = zrodlo;
  s.p.pliki.odmawiaZapisuPod = "/dane/";
  s.p.start.stanW = StanWersji::DO_SPRAWDZENIA;
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(s.p.start.powrot);
  SPRAWDZ(!s.p.start.oznaczona);
  ROWNE(s.p.pliki.tresc("/noce.csv"), zrodlo);
  int czytelnych = 0;
  for (auto& l : wiersze(s.p.pliki.tresc("/noce.csv"))) {
    Noc n;
    czytelnych += parsujNoc(l, 'A', n);
  }
  ROWNE_L(czytelnych, 400);
}

// T-WYM-43, T-TEST-274: migracja w miejscu (1 → 2 tego samego pliku) dopiero po oznaczeniu: obraz, który nie zdaje
// samosprawdzenia z innego powodu (Wi-Fi), zostawia plik w wersji 1 bez kopii `.v1`; obraz, który zdaje, migruje
// po oznaczeniu.
static void testT_WYM_43_migracjaWMiejscuPoOznaczeniu() {
  std::vector<FormatPliku> rej;
  SPRAWDZ(rejestrV2(rej, "noce"));
  {
    PanelP03 s;
    std::string t = zNocami(s, 30);
    s.konf.rejestr = &rej;
    s.p.start.stanW = StanWersji::DO_SPRAWDZENIA;
    s.siec.siecJest = false;
    wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
    SPRAWDZ(s.p.start.powrot && !s.p.start.oznaczona);
    ROWNE(s.p.pliki.tresc("/dane/noce/2026.csv"), t);
    SPRAWDZ(!s.p.pliki.pliki.count("/dane/noce/2026.csv.v" + std::to_string(wersjaNoce())));
  }
  {
    PanelP03 s;
    std::string t = zNocami(s, 30);
    s.konf.rejestr = &rej;
    s.p.start.stanW = StanWersji::DO_SPRAWDZENIA;
    blokada(s, 0);
    wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
    SPRAWDZ(s.p.start.oznaczona && !s.p.start.powrot);
    SPRAWDZ(s.p.pliki.tresc("/dane/noce/2026.csv").compare(0, 18, naglowekNoce(wersjaNoce() + 1) + "\n") == 0);
    ROWNE(s.p.pliki.tresc("/dane/noce/2026.csv.v" + std::to_string(wersjaNoce())), t);
    int oznacz = indeksZawiera(s.p.d.z, "start.oznacz", false);
    int zapis = std::min(indeksZawiera(s.p.d.z, "pliki.zapisz /dane/noce/", false) < 0
                             ? 1 << 30 : indeksZawiera(s.p.d.z, "pliki.zapisz /dane/noce/", false),
                         indeksZawiera(s.p.d.z, "pliki.nazwa /dane/noce/", false) < 0
                             ? 1 << 30 : indeksZawiera(s.p.d.z, "pliki.nazwa /dane/noce/", false));
    SPRAWDZ(oznacz >= 0 && zapis < (1 << 30) && zapis > oznacz);
  }
}

// T-WYM-58: doba slotu dziennego bez zmiany stanu AirBridge (oceny godzinowe z tym samym `/api/status`) → 0 zapisów
// bloba `noc`/`ab` w NVS.
static void testT_WYM_58_nvsTylkoPrzyZmianie() {
  testT_WYM_58_warstwaNvs();
  PanelP03 s;
  zNocami(s, 30);
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 20, 30), odpowiedziDoby);
  int ocen = 0, zapisow = 0;
  for (auto& w : v) {
    if (w.minuta < 8 * 60 + 30 || w.minuta > 20 * 60) continue;
    // implementacja bez wybudzenia z pełnym pobraniem (11:00, R-DANE-24: lista
    // i raport nocy), które zmienia godzinę danych `StanAB::pobrano`, więc zapis jest zmianą (T-DANE-15)
    if (indeksZawiera(w.zdarzenia, "/api/report", false) >= 0) continue;
    if (indeksZawiera(w.zdarzenia, "/api/status", false) >= 0) ocen++;
    for (auto& z : w.zdarzenia) zapisow += z == "nvs.zapis noc/ab";
  }
  printf("  T-WYM-58: ocen z /api/status 08:30–20:00: %d, zapisów noc/ab: %d\n", ocen, zapisow);
  SPRAWDZ(ocen >= 5);
  ROWNE_L(zapisow, 0);
}

// ---------- T-WYM-84, T-TEST-276: pierwsze formatowanie tylko partycji nowej ----------

static void testT_WYM_84_partycjaSkasowana() {
  PanelP03 s;
  partycjaSkasowana(s);
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 1);
  SPRAWDZ(!znacznikPartycji(s.p.pliki.opis).empty());
  ROWNE(s.p.nvs.tekst("cpap", NVS_KLUCZ_FS_INIT, "?"), znacznikPartycji(s.p.pliki.opis));
  SPRAWDZ(ukladWkompilowany() != 0);
  ROWNE_L(s.p.nvs.liczbaU("cpap", NVS_KLUCZ_UKLAD, 0), ukladWkompilowany());
  SPRAWDZ(logMa(s, "pierwsze formatowanie"));
  SPRAWDZ(zawiera(t.linieTestu, "Okno przywracania do 07:10"));
}

static void testT_WYM_84_smieciBezSuperbloku() {
  PanelP03 s;
  s.p.pliki.montujeSie = false;
  s.p.pliki.poczatek = smieci(84);
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 1);
  SPRAWDZ(logMa(s, "pierwsze formatowanie"));
}

static void testT_WYM_84_uszkodzonySuperblok() {
  PanelP03 s;
  s.p.pliki.montujeSie = false;
  std::string b = AtrapaPlikow::superblok(), z = smieci(85);
  b.replace(16, 200, z.substr(0, 200));  // napis `littlefs` zostaje, reszta superbloku zniszczona
  b.replace(AtrapaPlikow::BLOK + 16, 200, z.substr(200, 200));
  s.p.pliki.poczatek = b;
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 0);
  SPRAWDZ(s.p.pliki.poczatek == b);
  SPRAWDZ(!logMa(s, "pierwsze formatowanie"));
  SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: nie zamontowana"));
  SPRAWDZ(zawiera(t.linieUwagi, R_AW_16));
}

static void testT_WYM_84_znacznikINieczytelna() {
  PanelP03 s;
  s.p.pliki.montujeSie = false;
  s.p.pliki.poczatek = smieci(86);
  std::string b = s.p.pliki.poczatek;
  ustawZnacznik(s);
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 0);
  SPRAWDZ(s.p.pliki.poczatek == b);
  SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: nie zamontowana"));
  SPRAWDZ(zawiera(t.linieUwagi, R_AW_16));
}

static void testT_WYM_84_drugiStart() {
  PanelP03 s;
  partycjaSkasowana(s);
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 1);
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 8, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 1);
  SPRAWDZ(s.p.pliki.montowane);
  SPRAWDZ(!logMa(s, "pierwsze formatowanie"));
  SPRAWDZ(!zawiera(t.linieTestu, "Pamięć panelu: nie zamontowana"));
}

// R7 karty tablica partycji panelu inna niż wkompilowana → bez
// formatowania, bez migracji, ekran testu `układ partycji: stary, wgraj kablem`.
static void testT_WYM_84_staryUkladBezFormatowania() {
  PanelP03 s;
  std::string zrodlo = plikV0(noceV0(30, "2026-11-03"));
  s.p.pliki.pliki["/noce.csv"] = zrodlo;
  s.p.pliki.opis.uklad = ukladWkompilowany() + 1;
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 0);
  ROWNE(s.p.pliki.tresc("/noce.csv"), zrodlo);
  int nowych = 0;
  for (auto& kv : s.p.pliki.pliki) nowych += kv.first.compare(0, 6, "/dane/") == 0;
  ROWNE_L(nowych, 0);
  SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: układ: wgraj kablem") &&
          zawiera(t.diagnostykaTestu, "układ partycji: stary, wgraj kablem"));
}

// T-WYM-34: partycja nie montuje się i nie jest nowa (znacznik w NVS, superblok) → bez formatowania, praca na danych
// w pamięci (pobranie i ekran), ekran testu `pamięć plików: nie zamontowana`, linia R-AW-16.
static void testT_WYM_34_bezFormatowania() {
  PanelP03 s;
  s.p.pliki.montujeSie = false;
  ustawZnacznik(s);
  odpowiedziDoby(s, ep(2026, 11, 4, 7, 0));
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 0);
  SPRAWDZ(!s.siec.zadania.empty());
  SPRAWDZ(s.p.ekran.odswiezen > 0);
  SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: nie zamontowana"));
  SPRAWDZ(zawiera(t.linieUwagi, R_AW_16));
}

// ---------- T-WYM-87, T-WYM-62, T-TEST-280: okno przywracania i przywrócenie kablem ----------

// Zrzut prototypu (8.9 krok 1): 6 plików wersji 0, noce: `noc` nocy do poranka 3.11.2026 (dane syntetyczne).
static std::map<std::string, std::string> zrzut(int noc) {
  std::map<std::string, std::string> z;
  z["noce.csv"] = plikV0(noceV0(noc, "2026-11-03"));
  std::vector<std::string> c, h, sy, co, b;
  for (int i = 0; i < 20; i++) c.push_back(zapiszWpis(WpisCzynnikow{dodajDni("2026-10-15", i), 'O', CZ_KOLACJA, 'K', -1, true}));
  for (int i = 0; i < 30; i++)
    h.push_back(zapiszHumor(WpisHumoru{dodajDni("2026-10-05", i), "S2A", std::to_string(i + 1), 'D', "", ""}));
  for (int i = 0; i < 30; i++) sy.push_back(zapiszSypialnie(NocSypialni{dodajDni("2026-10-05", i), 20.0f, 45.0f}));
  for (int i = 0; i < 20; i++) co.push_back(dodajDni("2026-10-15", i) + ";" + std::to_string(800 + i));
  for (int i = 0; i < 50; i++)
    b.push_back(zapiszBaterie(WpisBaterii{ep(2026, 11, 3, 0, 0) + (uint32_t)i * 1800u, 3950, 1200, 5000, "zegar:czujniki"}));
  z["czynniki.csv"] = plikV0(c);
  z["humor.csv"] = plikV0(h);
  z["sypialnia.csv"] = plikV0(sy);
  z["sypialnia-co2.csv"] = plikV0(co);
  z["bateria.csv"] = plikV0(b);
  return z;
}
static std::string komendaW(const std::string& nazwa, const std::string& tresc) {
  return "W " + nazwa + " " + std::to_string(tresc.size()) + " " + sha256Hex(tresc) + "\n" + tresc;
}
static void doPortu(PanelP03& s, const std::string& b) {
  for (char c : b) s.p.port.znaki.push_back((unsigned char)c);
}

// T-WYM-87, T-TEST-280: po formatowaniu 0 wywołań sieci w oknie, ekran testu `okno przywracania do {hh:mm}`, 6 plików
// zrzutu komendą `W` → `OK` i pliki w `/przywracanie/`; `K` kończy okno. Skrót: wektory testowe SHA-256 (FIPS 180-2).
static void testT_WYM_87_oknoPrzywracania() {
  ROWNE(sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  ROWNE(sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  ROWNE(sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  PanelP03 s;
  partycjaSkasowana(s);
  std::map<std::string, std::string> z = zrzut(412);
  for (auto& kv : z) doPortu(s, komendaW(kv.first, kv.second));
  doPortu(s, "K");
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(zawiera(t.linieTestu, "Okno przywracania do 07:10"));
  int ostatnieOk = -1;
  for (auto& kv : z) {
    SPRAWDZ(logMa(s, "OK " + kv.first));
    ROWNE_L(s.p.pliki.tresc("/przywracanie/" + kv.first).size(), kv.second.size());
    SPRAWDZ(s.p.pliki.tresc("/przywracanie/" + kv.first) == kv.second);
    ostatnieOk = std::max(ostatnieOk, indeksZawiera(s.p.d.z, "port OK " + kv.first, true));
  }
  int pierwszaSiec = indeksZawiera(s.p.d.z, "siec.", false);
  SPRAWDZ(ostatnieOk >= 0 && (pierwszaSiec < 0 || pierwszaSiec > ostatnieOk));
  SPRAWDZ(s.siec.pierwszeMs < 10u * 60 * 1000);  // `K` zakończyło okno przed czasem
}

// Odpowiedź panelu w świeżym oknie przywracania na bajty `b` (port log).
static std::vector<std::string> odpowiedzOkna(const std::string& b, PanelP03& s) {
  partycjaSkasowana(s);
  doPortu(s, b);
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  return s.p.port.log;
}

// T-WYM-87, T-TEST-280: jeden zmieniony bajt → `BLAD … skrot`, pozostałe przyjęte; rozmiar ponad 4 MB → `BLAD …
// rozmiar`; nazwa spoza listy zrzutu, z `..` albo z ukośnikiem → `BLAD … nazwa`.
static void testT_WYM_87_zlySkrotIRozmiar() {
  {
    PanelP03 s;
    std::map<std::string, std::string> z = zrzut(30);
    std::string zly = komendaW("czynniki.csv", z["czynniki.csv"]);
    zly[zly.size() - 5] = zly[zly.size() - 5] == '1' ? '2' : '1';  // treść zmieniona po policzeniu skrótu
    std::vector<std::string> l = odpowiedzOkna(komendaW("noce.csv", z["noce.csv"]) + zly +
                                                   komendaW("humor.csv", z["humor.csv"]) + "K", s);
    SPRAWDZ(zawiera(l, "OK noce.csv"));
    SPRAWDZ(zawiera(l, "BLAD czynniki.csv skrot"));
    SPRAWDZ(zawiera(l, "OK humor.csv"));
    SPRAWDZ(!s.p.pliki.pliki.count("/przywracanie/czynniki.csv"));
    SPRAWDZ(s.p.pliki.tresc("/przywracanie/humor.csv") == z["humor.csv"]);
  }
  {
    PanelP03 s;
    std::vector<std::string> l = odpowiedzOkna("W humor.csv 5000000 " + sha256Hex("x") + "\n", s);
    SPRAWDZ(zawiera(l, "BLAD humor.csv rozmiar"));
    SPRAWDZ(!s.p.pliki.pliki.count("/przywracanie/humor.csv"));
  }
  // `konfiguracja.txt` nie jest już „spoza listy”: osobna ścieżka wprost do /konfiguracja.txt (testy_wieczor_bez_wifi_obrazki.cpp)
  for (const char* n : {"../noce.csv", "konfiguracja.txt.bak", "/noce.csv"}) {
    PanelP03 s;
    std::vector<std::string> l = odpowiedzOkna(komendaW(n, "abc"), s);
    SPRAWDZ(zawiera(l, std::string("BLAD ") + n + " nazwa"));
    int wPrzywracaniu = 0;
    for (auto& kv : s.p.pliki.pliki) wPrzywracaniu += kv.first.compare(0, 14, "/przywracanie/") == 0;
    ROWNE_L(wPrzywracaniu, 0);
  }
}

// T-WYM-87, T-TEST-280: komenda `W` przy partycji z nocami (poza oknem) → `BLAD … poza oknem`, pliki bez zmian.
static void testT_WYM_87_pozaOknem() {
  PanelP03 s;
  std::string t = zNocami(s, 30);
  doPortu(s, komendaW("czynniki.csv", zrzut(30)["czynniki.csv"]));
  wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "BLAD czynniki.csv poza oknem"));
  ROWNE(s.p.pliki.tresc("/dane/noce/2026.csv"), t);
  int wPrzywracaniu = 0;
  for (auto& kv : s.p.pliki.pliki) wPrzywracaniu += kv.first.compare(0, 14, "/przywracanie/") == 0;
  ROWNE_L(wPrzywracaniu, 0);
}

// T-WYM-87, T-TEST-280: 10 min bez komendy → zwykła praca bez historii; sieć dopiero po oknie.
static void testT_WYM_87_oknoBezKomendy() {
  PanelP03 s;
  partycjaSkasowana(s);
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(zawiera(t.linieTestu, "Okno przywracania do 07:10"));
  SPRAWDZ(s.siec.pierwszeMs != UINT32_MAX && s.siec.pierwszeMs >= 10u * 60 * 1000);
  ROWNE_L(rekordyNocy(s.p.pliki), 0);
}

// Partycja zamontowana, bez nocy, ze zrzutem w `/przywracanie/`.
static void zrzutWPrzywracaniu(PanelP03& s, const std::map<std::string, std::string>& z) {
  s.p.pliki.pliki.clear();
  for (auto& kv : z) s.p.pliki.pliki["/przywracanie/" + kv.first] = kv.second;
}

// T-WYM-62, R-DANE-37, T-TEST-280: 6 plików zrzutu (412 nocy, czynniki, dziennik) w `/przywracanie/` → 412 nocy
// w wersji 1, czynniki i dziennik jak w zrzucie, w logu `przywrócono 412 z 412` przed pierwszym pobraniem.
static void testT_WYM_62_przywrocenie412() {
  PanelP03 s;
  std::map<std::string, std::string> z = zrzut(412);
  zrzutWPrzywracaniu(s, z);
  odpowiedziDoby(s, ep(2026, 11, 4, 7, 0));
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  // implementacja: po przywróceniu zwykłe pobranie o 07:00 dopisuje noc 4.11
  // z odpowiedziDoby(), więc w plikach 412 nocy ze zrzutu i jedna pobrana (było: 412 razem)
  ROWNE_L(rekordyNocy(s.p.pliki), 413);
  SPRAWDZ(zawiera(wiersze(s.p.pliki.tresc("/dane/noce/2026.csv")), "2026-11-04;"));
  SPRAWDZ(s.p.pliki.tresc("/dane/noce/2025.csv").compare(0, 18, naglowekNoce(wersjaNoce()) + "\n") == 0);
  SPRAWDZ(s.p.pliki.tresc("/dane/noce/2026.csv").compare(0, 18, naglowekNoce(wersjaNoce()) + "\n") == 0);
  std::vector<std::string> cz = wiersze(s.p.pliki.tresc("/dane/czynniki.csv")),
                           hu = wiersze(s.p.pliki.tresc("/dane/humor.csv"));
  std::set<std::string> czs(cz.begin(), cz.end()), hus(hu.begin(), hu.end());
  int czOk = 0, huOk = 0;
  std::vector<std::string> zc = wiersze(z["czynniki.csv"]), zh = wiersze(z["humor.csv"]);
  for (auto& w : zc) czOk += czs.count(w) ? 1 : 0;
  for (auto& w : zh) huOk += hus.count(w) ? 1 : 0;
  ROWNE_L(czOk, zc.size());
  ROWNE_L(huOk, zh.size());
  int wpis = indeksZawiera(s.p.d.z, "przywrócono 412 z 412", false);
  int pobranie = indeksZawiera(s.p.d.z, "siec.get", false);
  SPRAWDZ(wpis >= 0);
  SPRAWDZ(pobranie < 0 || wpis < pobranie);
}

// T-WYM-62, R-DANE-37, T-TEST-280: ucięty plik nocy w zrzucie → `niezgodna liczba`, pliki w `/przywracanie/` bez zmian,
// praca bez historii.
static void testT_WYM_62_niezgodnaLiczba() {
  PanelP03 s;
  std::map<std::string, std::string> z = zrzut(412);
  z["noce.csv"] = z["noce.csv"].substr(0, z["noce.csv"].size() - 60);  // ostatni wiersz urwany w połowie
  zrzutWPrzywracaniu(s, z);
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "niezgodna liczba"));
  for (auto& kv : z) SPRAWDZ(s.p.pliki.tresc("/przywracanie/" + kv.first) == kv.second);
  ROWNE_L(rekordyNocy(s.p.pliki), 0);
  SPRAWDZ(!logMa(s, "przywrócono 41"));
}

// R-DANE-37: pamięć panelu z rekordami nocy → `/przywracanie/` nie jest czytany, praca z nocami panelu.
static void testR_DANE_37_pamiecZNocami() {
  PanelP03 s;
  zrzutWPrzywracaniu(s, zrzut(412));
  std::string t = zNocami(s, 30);
  TekstyWybudzenia tk = wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  int odczytow = 0;
  for (auto& kv : s.p.pliki.odczyty) odczytow += kv.first.compare(0, 14, "/przywracanie/") == 0 ? kv.second : 0;
  ROWNE_L(odczytow, 0);
  ROWNE(s.p.pliki.tresc("/dane/noce/2026.csv"), t);
  SPRAWDZ(!logMa(s, "przywrócono"));
  SPRAWDZ(zawiera(tk.diagnostykaTestu, "historia 30 nocy"));
}

// R-AW-16: pamięć nieczytelna (nie nowa) → linia R-AW-16, bez przywracania i bez okna przywracania.
static void testR_AW_16_nieczytelnaBezPrzywracania() {
  PanelP03 s;
  s.p.pliki.montujeSie = false;
  ustawZnacznik(s);
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(zawiera(t.linieUwagi, R_AW_16));
  SPRAWDZ(!zawiera(t.linieTestu, "Okno przywracania"));
  SPRAWDZ(!logMa(s, "przywrócono"));
  ROWNE_L(s.p.pliki.formatowan, 0);
  SPRAWDZ(s.siec.pierwszeMs < 10u * 60 * 1000);
}

// R-NF-19: 100 nocy w wersji 1, czynniki i dziennik, „aktualizacja” do firmware z tą samą wersją formatów (stan „do
// sprawdzenia”) → wersja oznaczona dobra, rekordy i czynniki bajt w bajt, dziennik bez zmian w istniejących wierszach.
static void testR_NF_19_aktualizacjaZachowujeDane() {
  PanelP03 s;
  std::string noce = plikNoce(noceZnane(100, "2026-11-02"));
  s.p.pliki.pliki["/dane/noce/2026.csv"] = noce;
  std::vector<std::string> c, h;
  for (int i = 0; i < 10; i++) c.push_back(zapiszWpis(WpisCzynnikow{dodajDni("2026-10-20", i), 'O', CZ_STRES, 'K', -1, true}));
  for (int i = 0; i < 10; i++) h.push_back(zapiszHumor(WpisHumoru{dodajDni("2026-10-20", i), "S2A", std::to_string(i + 1), 'D', "", ""}));
  std::string czy = plik("czynniki", 1, POLA_CZYNNIKI, c, true), hum = plik("humor", 1, POLA_HUMOR, h, false);
  s.p.pliki.pliki["/dane/czynniki.csv"] = czy;
  s.p.pliki.pliki["/dane/humor.csv"] = hum;
  s.p.start.stanW = StanWersji::DO_SPRAWDZENIA;
  s.p.start.napisWersji = "1.0.1+abc1234";
  blokada(s, 0);
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(s.p.start.oznaczona);
  ROWNE(s.p.pliki.tresc("/dane/noce/2026.csv"), noce);
  ROWNE(s.p.pliki.tresc("/dane/czynniki.csv"), czy);
  SPRAWDZ(s.p.pliki.tresc("/dane/humor.csv").compare(0, hum.size(), hum) == 0);
  SPRAWDZ(zawiera(t.diagnostykaTestu, "historia 100 nocy"));
  int kopii = 0;
  for (auto& kv : s.p.pliki.pliki) kopii += kv.first.find(".csv.v") != std::string::npos;
  ROWNE_L(kopii, 0);
}

// R-KON-41 (R1 karty): noc z polem `import` 07:00, zanik zasilania, start 09:30 → chwila importu z rekordu
// (start slotu dziennego 08:00 minął, więc slot od tego wybudzenia, przypadek „panel uśpiony”), nie 10:30
// z zastępnika NVS; noc z pustym `import` → zastępnik z NVS jak.
static void testR_KON_41_importZRekorduPoZanikuZasilania() {
  for (bool zImportem : {true, false}) {
    PanelP03 s;
    std::vector<std::string> w = noceV0(10, "2026-11-04");
    for (auto& x : w) x += ";";
    if (zImportem) w.back() += std::to_string(ep(2026, 11, 4, 7, 0));
    s.p.pliki.pliki["/dane/noce/2026.csv"] = plikNoce(w);
    StanAB a;  // kopia stanu AirBridge w NVS sprzed zaniku: ostatni odczyt 09:29:50
    a.magia = MAGIA_AB;
    a.ma = true;
    a.pobrano = ep(2026, 11, 4, 9, 29, 50);
    a.rop = 0;
    s.p.nvs.bajtyMap["noc/ab"] = std::string((const char*)&a, sizeof a);
    s.siec.siecJest = false;
    wybudz(s, ep(2026, 11, 4, 9, 30), PowodWybudzenia::START);
    const StanRtc& r = s.p.start.stan;
    if (zImportem) {
      ROWNE_L(r.importNocyEp, ep(2026, 11, 4, 7, 0));
      SPRAWDZ(r.slotDziennyEp >= ep(2026, 11, 4, 9, 30) && r.slotDziennyEp <= ep(2026, 11, 4, 9, 31));
    } else {
      ROWNE_L(r.importNocyEp, ep(2026, 11, 4, 9, 29, 50));
      ROWNE_L(r.slotDziennyEp, 0);
    }
  }
}

// ============================================================================================================
// (c) Testy dodane przez implementację: rozstrzygnięcia, R7 przy
// wersji do sprawdzenia, identyfikator układu, urwana treść komendy `W`, przejście na kopii zrzutu z panelu
// ============================================================================================================

// T-WYM-38 (R7): identyfikator układu z pliku tablicy w repozytorium = wkompilowany; wariant 16 MB i tablica
// prototypu (default_8MB.csv) dają inny; kolejność wierszy i partycje innych typów nie zmieniają wyniku. Ta sama
// liczba co w skrypcie budowy (firmware/prototyp/uklad_partycji.py, pytest tools/test_wgraj_pliki.py).
static std::string czytajPlikRepo(const std::string& wzgledna) {
  std::string t = __FILE__;
  std::ifstream f(t.substr(0, t.rfind('/')) + "/../../" + wzgledna);
  std::stringstream b;
  b << f.rdbuf();
  return b.str();
}
static void testT_WYM_38_identyfikatorUkladu() {
  std::vector<WpisPartycji> t32, t16, proto, bledna;
  SPRAWDZ(tablicaZCsv(czytajPlikRepo("partycje-e1001.csv"), t32));
  SPRAWDZ(tablicaZCsv(czytajPlikRepo("partycje-e1001-16mb.csv"), t16));
  ROWNE_L(t32.size(), 6);
  ROWNE_L(identyfikatorUkladu(t32), ukladWkompilowany());
  ROWNE_L(ukladWkompilowany(), 0x6fcc622eu);
  SPRAWDZ(identyfikatorUkladu(t16) != ukladWkompilowany());
  SPRAWDZ(tablicaZCsv("nvs,data,nvs,0x9000,0x5000\notadata,data,ota,0xe000,0x2000\n"
                      "app0,app,ota_0,0x10000,0x330000\napp1,app,ota_1,0x340000,0x330000\n"
                      "spiffs,data,spiffs,0x670000,0x180000\ncoredump,data,coredump,0x7F0000,0x10000\n", proto));
  SPRAWDZ(identyfikatorUkladu(proto) != ukladWkompilowany());
  std::vector<WpisPartycji> odwrotnie(t32.rbegin(), t32.rend());
  WpisPartycji boot;
  boot.nazwa = "bootloader";
  boot.typ = 2;
  odwrotnie.push_back(boot);
  ROWNE_L(identyfikatorUkladu(odwrotnie), ukladWkompilowany());
  SPRAWDZ(!tablicaZCsv("nvs,data,nvs,,0x5000\n", bledna));
  SPRAWDZ(!tablicaZCsv("nvs,dane,nvs,0x9000,0x5000\n", bledna));
  SPRAWDZ(!tablicaZCsv("# sam komentarz\n", bledna));
  ROWNE_L(identyfikatorUkladu({}), 0);
}

// T-WYM-84: znacznik `fs_init` tej partycji w NVS i partycja w całości 0xFF → bez formatowania (T-WYM-34).
static void testT_WYM_84_znacznikIPartycjaSkasowana() {
  PanelP03 s;
  partycjaSkasowana(s);
  ustawZnacznik(s);
  std::string b = s.p.pliki.poczatek;
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 0);
  SPRAWDZ(s.p.pliki.poczatek == b);
  SPRAWDZ(!logMa(s, "pierwsze formatowanie"));
  SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: nie zamontowana"));
  SPRAWDZ(zawiera(t.linieUwagi, R_AW_16));
  SPRAWDZ(!zawiera(t.linieTestu, "Okno przywracania"));
}

// R7 przy wersji „do sprawdzenia” (aktualizacja przez Wi-Fi na starą tablicę): samosprawdzenie niezdane, powrót do
// poprzedniej wersji, pliki prototypu bajt w bajt, bez plików pod nowymi ścieżkami i bez klucza `wersja` w NVS.
static void testT_WYM_84_staryUkladPowrotDoPoprzedniej() {
  PanelP03 s;
  std::string zrodlo = plikV0(noceV0(30, "2026-11-03"));
  s.p.pliki.pliki["/noce.csv"] = zrodlo;
  s.p.pliki.opis.uklad = ukladWkompilowany() ^ 0x55u;
  s.p.start.stanW = StanWersji::DO_SPRAWDZENIA;
  s.p.start.napisWersji = "1.0.1+abc1234";
  blokada(s, 0);
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(s.p.start.powrot);
  SPRAWDZ(!s.p.start.oznaczona);
  ROWNE(s.p.pliki.tresc("/noce.csv"), zrodlo);
  ROWNE_L(s.p.pliki.zapisy, 0);
  ROWNE_L(s.p.nvs.liczbaU("cpap", NVS_KLUCZ_WERSJA, 0), 0);
  ROWNE_L(s.p.pliki.formatowan, 0);
  (void)t;
}

// T-WYM-87: bez znanego zegara linia `okno przywracania: {m} min` (minuty do końca okna od startu).
static void testT_WYM_87_oknoBezZegara() {
  PanelP03 s;
  partycjaSkasowana(s);
  s.siec.ntpJest = false;
  TekstyWybudzenia t = wybudz(s, 0, PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 1);
  SPRAWDZ(zawiera(t.linieTestu, "Okno przywracania: 10 min"));
  SPRAWDZ(!zawiera(t.linieTestu, "Okno przywracania do"));
  SPRAWDZ(s.siec.pierwszeMs != UINT32_MAX && s.siec.pierwszeMs >= 10u * 60 * 1000);
}

// T-WYM-87, R8: treść krótsza niż rozmiar z nagłówka → po 5 s bez bajtu `BLAD … rozmiar`, bez pliku; następna
// komenda w tym samym oknie przyjęta.
static void testT_WYM_87_urwanaTresc() {
  PanelP03 s;
  partycjaSkasowana(s);
  std::map<std::string, std::string> z = zrzut(30);
  std::string h = z["humor.csv"];
  doPortu(s, "W humor.csv " + std::to_string(h.size()) + " " + sha256Hex(h) + "\n" + h.substr(0, h.size() / 2));
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "BLAD humor.csv rozmiar"));
  SPRAWDZ(!s.p.pliki.pliki.count("/przywracanie/humor.csv"));
  SPRAWDZ(!s.p.pliki.pliki.count("/przywracanie/humor.csv.tmp"));
  // to samo okno: najpierw urwany plik, potem (po przerwie ponad 5 s) cały → odrzucony urwany, nic nie sklejone
  PanelP03 q;
  partycjaSkasowana(q);
  doPortu(q, komendaW("czynniki.csv", z["czynniki.csv"]) + "K");
  wybudz(q, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(q, "OK czynniki.csv"));
  ROWNE(q.p.pliki.tresc("/przywracanie/czynniki.csv"), z["czynniki.csv"]);
}

// Atrapa plików z błędami, których nie da się wywołać przedrostkiem odmowy: nieudana zmiana nazwy i zapis pliku
// tymczasowego (`.tmp`) przerwany po kilku bajtach.
class PlikiZBledami : public AtrapaPlikow {
 public:
  explicit PlikiZBledami(Dziennik& d) : AtrapaPlikow(d) {}
  bool bezZmianyNazwy = false, tmpUciety = false;
  bool zmienNazwe(const char* z, const char* na) override {
    return bezZmianyNazwy ? false : AtrapaPlikow::zmienNazwe(z, na);
  }
  size_t zapisz(const char* n, const std::string& t) override {
    std::string s = n;
    if (tmpUciety && s.size() > 4 && s.compare(s.size() - 4, 4, ".tmp") == 0)
      return AtrapaPlikow::zapisz(n, t.substr(0, t.size() / 2));
    return AtrapaPlikow::zapisz(n, t);
  }
};

// T-WYM-51, T-WYM-54, T-WYM-55 (implementacja; pokrycie ścieżek błędów magazynu): nieznany format,
// wiersz ze znakiem końca linii, nieudana zmiana nazwy (plik zostaje, `.tmp` usunięty), zapis i dopisanie do pliku
// czekającego na migrację w miejscu, dopisanie do pliku z nowszej wersji i bez czytelnego nagłówka.
static void testT_WYM_51_bledyZapisuMagazynu() {
  Dziennik d;
  PlikiZBledami pl(d);
  montowana(pl);
  Log lg;
  Magazyn m(pl, lg.f());
  SPRAWDZ(m.czytaj("nieznany", "/dane/x.csv").stan == StanPliku::USZKODZONY);
  SPRAWDZ(lg.ma("nieznany format"));
  SPRAWDZ(!m.zapisz("czynniki", "/dane/czynniki.csv", {"2026-11-01;O\n;K;K;-1;1"}));
  SPRAWDZ(!pl.pliki.count("/dane/czynniki.csv"));
  SPRAWDZ(m.zapisz("czynniki", "/dane/czynniki.csv", {wierszFormatu("czynniki", 1)}));
  std::string przed = pl.tresc("/dane/czynniki.csv");
  pl.bezZmianyNazwy = true;
  SPRAWDZ(!m.zapisz("czynniki", "/dane/czynniki.csv", {wierszFormatu("czynniki", 2)}));
  ROWNE(pl.tresc("/dane/czynniki.csv"), przed);
  SPRAWDZ(!pl.pliki.count("/dane/czynniki.csv.tmp"));
  SPRAWDZ(lg.ma("zmiana nazwy się nie udała"));
  SPRAWDZ(!m.zapiszSurowy("/przywracanie/noce.csv", "abc\n"));
  SPRAWDZ(!pl.pliki.count("/przywracanie/noce.csv") && !pl.pliki.count("/przywracanie/noce.csv.tmp"));
  // kompletny `.tmp` przy uszkodzonym docelowym, ale zmiana nazwy się nie udaje: odczyt z `.tmp`, wpis w logu
  pl.pliki["/dane/czynniki.csv"] = "#cpap-eink czynniki 1\nucięty";
  pl.pliki["/dane/czynniki.csv.tmp"] = przed;
  OdczytPliku o = m.czytaj("czynniki", "/dane/czynniki.csv");
  SPRAWDZ(o.stan == StanPliku::DOBRY);
  SPRAWDZ(lg.ma("plik tymczasowy kompletny, ale zmiana nazwy się nie udała"));
  pl.bezZmianyNazwy = false;
  pl.tmpUciety = true;
  SPRAWDZ(!m.zapiszSurowy("/przywracanie/noce.csv", "abcdef\n"));
  SPRAWDZ(lg.ma("zapisano 3 z 7 B"));
  pl.tmpUciety = false;
  // dopisanie: plik bez czytelnego nagłówka odłożony, nowy plik; plik z nowszej wersji tylko do odczytu
  pl.pliki["/dane/humor.csv"] = "śmieci\n2026-10-01;S2A;1;D;;\n";
  SPRAWDZ(m.dopisz("humor", "/dane/humor.csv", wierszFormatu("humor", 1)));
  SPRAWDZ(pl.pliki.count("/dane/humor.csv.uszkodzony"));
  SPRAWDZ(pl.tresc("/dane/humor.csv").compare(0, 19, "#cpap-eink humor 1\n") == 0);
  pl.pliki["/dane/humor.csv"] = std::string("#cpap-eink humor 3\n") + POLA_HUMOR + ";a;b\n";
  std::string nowszy = pl.tresc("/dane/humor.csv");
  SPRAWDZ(!m.dopisz("humor", "/dane/humor.csv", wierszFormatu("humor", 2)));
  SPRAWDZ(m.tylkoDoOdczytu("/dane/humor.csv"));
  ROWNE(pl.tresc("/dane/humor.csv"), nowszy);
  // rejestr z wersją 2: plik wersji 1 czeka na migrację w miejscu, zapis i dopisanie odrzucone
  std::vector<std::string> wh;
  for (int i = 0; i < 3; i++) wh.push_back(wierszFormatu("humor", i));
  for (const char* f : {"czynniki", "humor"}) {
    std::vector<FormatPliku> r2;
    SPRAWDZ(rejestrV2(r2, f));
    Dziennik d2;
    AtrapaPlikow p2(d2);
    montowana(p2);
    Magazyn m2(p2, lg.f(), r2);
    bool przep = std::string(f) == "czynniki";
    std::string sc = przep ? "/dane/czynniki.csv" : "/dane/humor.csv";
    p2.pliki[sc] = przep ? plik("czynniki", 1, POLA_CZYNNIKI, {wierszFormatu("czynniki", 1)}, true)
                         : plik("humor", 1, POLA_HUMOR, wh, false);
    std::string v1 = p2.tresc(sc);
    if (std::string(f) == "humor") {
      SPRAWDZ(!m2.dopisz(f, sc, wierszFormatu(f, 9)));  // bez wcześniejszego odczytu: wersja z nagłówka
      SPRAWDZ(m2.odrzuconoZapisStarszego());
    }
    SPRAWDZ(m2.czytaj(f, sc).stan == StanPliku::DOBRY);
    SPRAWDZ(przep ? !m2.zapisz(f, sc, {wierszFormatu(f, 9)}) : !m2.dopisz(f, sc, wierszFormatu(f, 9)));
    SPRAWDZ(m2.odrzuconoZapisStarszego());
    ROWNE(p2.tresc(sc), v1);
  }
}

// T-WYM-55, T-WYM-56 (implementacja: migracja w miejscu z nieudaną kopią `.v1` i nieudanym zapisem →
// plik w wersji 1 bez zmian, wpis w logu; obok pliku `.tmp` niekompletny (wersja z pełnego odczytu) → migracja
// udana; wycofanie migracji na nowe ścieżki usuwa pliki wynikowe, źródło zostaje.
static void testT_WYM_55_bledyMigracjiWMiejscu() {
  std::vector<FormatPliku> r2;
  SPRAWDZ(rejestrV2(r2, "czynniki"));
  std::string v1 = plik("czynniki", 1, POLA_CZYNNIKI, {wierszFormatu("czynniki", 1), wierszFormatu("czynniki", 2)}, true);
  for (int wariant = 0; wariant < 2; wariant++) {
    Dziennik d;
    PlikiZBledami pl(d);
    montowana(pl);
    Log lg;
    Magazyn m(pl, lg.f(), r2);
    pl.pliki["/dane/czynniki.csv"] = v1;
    if (wariant == 0) pl.odmawiaZapisuPod = "/dane/czynniki.csv.v";
    else pl.tmpUciety = true;
    SPRAWDZ(!m.migrujWMiejscu());
    ROWNE(pl.tresc("/dane/czynniki.csv"), v1);
    SPRAWDZ(lg.ma(wariant == 0 ? "błąd (kopia" : "błąd (zapis"));
  }
  {
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    Log lg;
    Magazyn m(pl, lg.f(), r2);
    pl.pliki["/dane/czynniki.csv"] = v1;
    pl.pliki["/dane/czynniki.csv.tmp"] = "#cpap-eink czynniki 1\nucięty";
    SPRAWDZ(m.migrujWMiejscu());
    SPRAWDZ(pl.tresc("/dane/czynniki.csv").compare(0, 22, "#cpap-eink czynniki 2\n") == 0);
    SPRAWDZ(!pl.pliki.count("/dane/czynniki.csv.tmp"));
  }
  {
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    Log lg;
    Magazyn m(pl, lg.f());
    pl.pliki["/czynniki.csv"] = plikV0({wierszFormatu("czynniki", 1)});
    SPRAWDZ(m.migrujNaNoweSciezki());
    SPRAWDZ(pl.pliki.count("/dane/czynniki.csv"));
    m.wycofajMigracje();
    SPRAWDZ(!pl.pliki.count("/dane/czynniki.csv"));
    SPRAWDZ(pl.pliki.count("/czynniki.csv"));
    SPRAWDZ(lg.ma("migracja wycofana"));
  }
}

// T-WYM-62 (implementacja: pełna pamięć przy zapisie przywracanych plików → `niezgodna liczba`, żaden
// plik nocy nie zostaje, pliki w `/przywracanie/` bez zmian; katalog bez pliku nocy → bez próby.
static void testT_WYM_62_bladZapisuPrzywracania() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  Log lg;
  Magazyn m(pl, lg.f());
  SPRAWDZ(!m.przywrocZKatalogu("/przywracanie").proba);
  std::string n = plikV0(noceV0(40, "2026-11-03"));
  pl.pliki["/przywracanie/noce.csv"] = n;
  pl.wolneBajty = 200;
  Magazyn::WynikPrzywracania w = m.przywrocZKatalogu("/przywracanie");
  SPRAWDZ(w.proba && !w.ok);
  ROWNE_L(w.oczekiwane, 40);
  SPRAWDZ(lg.ma("niezgodna liczba nocy"));
  ROWNE_L(rekordyNocy(pl), 0);
  ROWNE(pl.tresc("/przywracanie/noce.csv"), n);
  SPRAWDZ(m.bezPlikowNocy());
  pl.montowane = false;
  SPRAWDZ(!m.bezPlikowNocy());
}

// T-WYM-84 (implementacja: wszystkie wyniki decyzjaPartycji z opisem do logu, typ i podtyp liczbą
// w tablicy CSV, przestrzeń NVS tylko do odczytu bez zapisu (T-WYM-54).
static void testT_WYM_84_decyzjePartycji() {
  StanPartycji s;
  s.ukladZgodny = false;
  SPRAWDZ(decyzjaPartycji(s) == DecyzjaPartycji::STARY_UKLAD);
  s.ukladZgodny = true;
  s.zamontowana = true;
  SPRAWDZ(decyzjaPartycji(s) == DecyzjaPartycji::ZAMONTOWANA);
  s.zamontowana = false;
  s.znacznikNvs = s.znacznikBiezacy = "820000:17e0000";
  SPRAWDZ(decyzjaPartycji(s) == DecyzjaPartycji::ZNACZNIK);
  s.znacznikNvs = "670000:180000";
  SPRAWDZ(decyzjaPartycji(s) == DecyzjaPartycji::NIECZYTELNA);
  s.blokiCzytelne = true;
  s.bloki = AtrapaPlikow::superblok();
  SPRAWDZ(decyzjaPartycji(s) == DecyzjaPartycji::SUPERBLOK);
  s.bloki = std::string(2 * BLOK_PARTYCJI, '\x11');
  s.ukladNvs = ukladWkompilowany();
  SPRAWDZ(decyzjaPartycji(s) == DecyzjaPartycji::NIE_NOWA);
  s.bloki = std::string(2 * BLOK_PARTYCJI, '\xff');
  SPRAWDZ(decyzjaPartycji(s) == DecyzjaPartycji::FORMATUJ);
  std::set<std::string> opisy;
  for (DecyzjaPartycji x : {DecyzjaPartycji::ZAMONTOWANA, DecyzjaPartycji::STARY_UKLAD, DecyzjaPartycji::FORMATUJ,
                            DecyzjaPartycji::ZNACZNIK, DecyzjaPartycji::NIECZYTELNA, DecyzjaPartycji::SUPERBLOK,
                            DecyzjaPartycji::NIE_NOWA})
    opisy.insert(opisDecyzjiPartycji(x));
  ROWNE_L(opisy.size(), 7);
  ROWNE(opisDecyzjiPartycji((DecyzjaPartycji)99), "?");
  std::vector<WpisPartycji> t;
  SPRAWDZ(tablicaZCsv("spiffs, 1, 0x82, 0x820000, 24448K\napp0, 0, 16, 0x10000, 4M\n", t));
  SPRAWDZ(t.size() == 2 && t[0].typ == 1 && t[0].podtyp == 0x82 && t[0].rozmiar == 0x17E0000 && t[1].podtyp == 0x10);
  SPRAWDZ(!tablicaZCsv("x, data, 300, 0x9000, 0x5000\n", t));
  Dziennik d;
  AtrapaNvs n(d);
  NvsTylkoZmiany z(n);
  z.tylkoDoOdczytu("cpap");
  z.tylkoDoOdczytu("cpap");
  z.ustawLiczbeU("cpap", "x", 5);
  z.ustawTekst("cpap", "y", "a");
  z.usun("cpap", "x");
  ROWNE_L(n.zapisy, 0);
}

// T-WYM-84, T-WYM-87 (implementacja: formatowanie nieudane → T-WYM-34 bez okna przywracania; zapis
// pliku `W` nieudany → `BLAD … zapis`; komenda `W` poza oknem bez końca wiersza i bez treści → po limicie 5 s
// `BLAD … rozmiar`, panel pracuje dalej.
static void testT_WYM_87_bledyOknaIKomendy() {
  {
    PanelP03 s;
    partycjaSkasowana(s);
    s.p.pliki.formatujeSie = false;
    TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    ROWNE_L(s.p.pliki.formatowan, 1);
    SPRAWDZ(logMa(s, "pierwsze formatowanie: błąd"));
    SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: nie zamontowana"));
    SPRAWDZ(!zawiera(t.linieTestu, "Okno przywracania"));
    SPRAWDZ(s.p.nvs.tekst("cpap", NVS_KLUCZ_FS_INIT, "").empty());
  }
  {
    PanelP03 s;
    partycjaSkasowana(s);
    s.p.pliki.odmawiaZapisuPod = "/przywracanie/";
    doPortu(s, komendaW("humor.csv", zrzut(30)["humor.csv"]) + "K");
    wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    SPRAWDZ(logMa(s, "BLAD humor.csv zapis"));
    SPRAWDZ(logMa(s, "okno przywracania: bez plików"));
  }
  {
    PanelP03 s;
    zNocami(s, 30);
    blokada(s, 0);
    doPortu(s, "W czynniki.csv 10");
    wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
    SPRAWDZ(logMa(s, "BLAD czynniki.csv rozmiar"));
    SPRAWDZ(s.p.sen.snow > 0);
  }
}

// T-WYM-53: kopia `{plik}.uszkodzony` wchodzi do zrzutu `Z` (ręczne odzyskanie).
static void testT_WYM_53_uszkodzonyWZrzucie() {
  PanelP03 s;
  zNocami(s, 30);
  s.p.pliki.pliki["/dane/czynniki.csv.uszkodzony"] = "#cpap-eink czynniki 1\nucięte";
  blokada(s, 0);
  doPortu(s, "Z");
  wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "PLIK /dane/czynniki.csv.uszkodzony"));
  SPRAWDZ(logMa(s, "PLIK /dane/noce/2026.csv"));
  SPRAWDZ(logMa(s, "ucięte"));
}

// Kryterium przekazania kopia zrzutu z panelu z katalogu
// w zmiennej CPAP_ZRZUT (poza repozytorium, WY12) → pierwsze formatowanie → pliki zrzutu komendą `W` (jak
// tools/prototyp/wgraj_pliki.py komenda()) → przywrócenie → liczba nocy = liczba niepustych wierszy pliku nocy
// zrzutu, ekran testu z wersją plików i liczbą nocy. Bez zmiennej: pominięty z komunikatem. CPAP_ZRZUT_PBM (katalog)
// = klatki ekranu do PBM (okno przywracania, ekran dnia, ekran testu po przywróceniu).
static void zapiszKlatke(const std::vector<uint8_t>& b, const std::string& sciezka) {
  std::ofstream f(sciezka, std::ios::binary);
  f << "P4\n800 480\n";
  f.write((const char*)b.data(), (std::streamsize)b.size());
}
static void testT_WYM_62_przejscieNaZrzucieZPanelu() {
  const char* kat = getenv("CPAP_ZRZUT");
  if (!kat || !*kat) {
    printf("  przejście na zrzucie z panelu: pominięte (ustaw CPAP_ZRZUT=<katalog zrzutu poza repo>)\n");
    return;
  }
  PanelP03 s;
  partycjaSkasowana(s);
  int oczekiwane = -1, plikow = 0;
  std::string bajty;
  for (auto& n : plikiZrzutu()) {
    std::ifstream f(std::string(kat) + "/" + n, std::ios::binary);
    if (!f) continue;
    std::stringstream b;
    b << f.rdbuf();
    std::string t = b.str();
    if (n == "noce.csv") {
      oczekiwane = 0;
      for (auto& w : wiersze(t)) oczekiwane += w.empty() ? 0 : 1;
    }
    bajty += komendaW(n, t);
    plikow++;
  }
  printf("  przejście na zrzucie z panelu: %s, plików %d, nocy w zrzucie %d\n", kat, plikow, oczekiwane);
  SPRAWDZ(oczekiwane > 0);
  doPortu(s, bajty + "K" + "O");  // `O` po oknie: ekran testu w czuwaniu (jak tools/prototyp, okno serwisowe)
  blokada(s, 0);
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 1);
  for (auto& n : plikiZrzutu())
    if (std::ifstream(std::string(kat) + "/" + n)) SPRAWDZ(logMa(s, "OK " + n));
  ROWNE_L(rekordyNocy(s.p.pliki), oczekiwane);
  SPRAWDZ(logMa(s, "przywrócono " + std::to_string(oczekiwane) + " z " + std::to_string(oczekiwane)));
  SPRAWDZ(zawiera(t.diagnostykaTestu, "historia " + std::to_string(oczekiwane) + " nocy"));
  // noce w wersji 2, pozostałe formaty w wersji 1; zrzut z samym plikiem nocy daje
  // tylko wersję 2 (sprawdzone na zrzutach syntetycznych: 6 plików i sam `noce.csv`)
  bool inne = false;
  for (auto& n : plikiZrzutu())
    if (n != "noce.csv" && std::ifstream(std::string(kat) + "/" + n)) inne = true;
  SPRAWDZ(zawiera(t.diagnostykaTestu, inne ? "Dane: wersje plików 1, " : "Dane: wersja plików 3"));  // noce 3
  const char* pbm = getenv("CPAP_ZRZUT_PBM");
  if (pbm && *pbm)
    for (size_t i = 0; i < s.p.ekran.klatki.size(); i++)
      zapiszKlatke(s.p.ekran.klatki[i], std::string(pbm) + "/przejscie-" + std::to_string(i) + ".pbm");
  // dwa dalsze scenariusze na tym samym zrzucie. Noc z AirBridge
  // z poranka 4.11 dochodzi do historii, gdy zrzut jej nie ma (scalenie: ta sama data = rekord z flash).
  std::map<std::string, std::string> pl;
  for (auto& n : plikiZrzutu()) {
    std::ifstream f(std::string(kat) + "/" + n, std::ios::binary);
    if (!f) continue;
    std::stringstream b;
    b << f.rdbuf();
    pl[n] = b.str();
  }
  bool maNoc4 = pl["noce.csv"].find("2026-11-04;") != std::string::npos;
  int poScaleniu = oczekiwane + (maNoc4 ? 0 : 1);
  std::string wszystkie;
  for (auto& kv : pl) wszystkie += komendaW(kv.first, kv.second);
  // (B) plik nocy z jednym zmienionym bajtem: `BLAD noce.csv skrot`, `K`, praca z nocą z AirBridge; drugi start
  // zasilania (np. odłączenie i podłączenie): okno znowu, poprawny plik nocy, przywrócenie ze scaleniem
  {
    PanelP03 b;
    partycjaSkasowana(b);
    std::string zla = komendaW("noce.csv", pl["noce.csv"]);
    zla[zla.size() - 3] = zla[zla.size() - 3] == '1' ? '2' : '1';
    std::string reszta;
    for (auto& kv : pl)
      if (kv.first != "noce.csv") reszta += komendaW(kv.first, kv.second);
    doPortu(b, zla + reszta + "K");
    odpowiedziDoby(b, ep(2026, 11, 4, 7, 0));
    wybudz(b, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    SPRAWDZ(logMa(b, "BLAD noce.csv skrot"));
    SPRAWDZ(logMa(b, "niezgodna liczba nocy (0 z ?, brak pliku nocy)"));
    ROWNE_L(rekordyNocy(b.p.pliki), 1);
    doPortu(b, komendaW("noce.csv", pl["noce.csv"]) + "K");
    blokada(b, 0);
    TekstyWybudzenia tb = wybudz(b, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
    SPRAWDZ(logMa(b, "OK noce.csv"));
    SPRAWDZ(logMa(b, "przywrócono " + std::to_string(oczekiwane) + " z " + std::to_string(oczekiwane)));
    ROWNE_L(rekordyNocy(b.p.pliki), poScaleniu);
    SPRAWDZ(zawiera(tb.diagnostykaTestu, "historia " + std::to_string(poScaleniu) + " nocy"));  // integracja: log U-05
    SPRAWDZ(!b.p.nvs.teksty[std::string("cpap/") + NVS_KLUCZ_PRZYWR].empty());
    printf("  przejście (B) zły plik nocy, restart zasilania, poprawne wgranie: %d nocy\n", rekordyNocy(b.p.pliki));
  }
  // (C) okno minęło bez plików, noc z AirBridge pobrana; drugi start zasilania: zrzut wgrany i scalony.
  // okno bez żadnej komendy `W` stawia znacznik „bez
  // przywracania”, więc drugi start jest bez okna; wgraj_pliki.py --reset dostaje na `?` odpowiedź `poza oknem, bez
  // przywracania`, wysyła pustą komendę `W` (przy następnym obiegu czuwania: znacznik zdjęty) i restartuje panel:
  // zrzut wchodzi przy czwartym starcie (wcześniej przy drugim)
  {
    PanelP03 c;
    partycjaSkasowana(c);
    odpowiedziDoby(c, ep(2026, 11, 4, 7, 0));
    wybudz(c, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    ROWNE_L(rekordyNocy(c.p.pliki), 1);
    doPortu(c, "?");
    blokada(c, 0);
    wybudz(c, ep(2026, 11, 4, 8, 50), PowodWybudzenia::START);
    SPRAWDZ(logMa(c, "przywracanie: poza oknem, bez przywracania"));
    doPortu(c, komendaW("noce.csv", ""));
    blokada(c, 0);
    wybudz(c, ep(2026, 11, 4, 8, 55), PowodWybudzenia::START);
    SPRAWDZ(logMa(c, "BLAD noce.csv poza oknem"));
    doPortu(c, wszystkie + "K");
    blokada(c, 0);
    TekstyWybudzenia tc = wybudz(c, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
    SPRAWDZ(logMa(c, "przywrócono " + std::to_string(oczekiwane) + " z " + std::to_string(oczekiwane)));
    ROWNE_L(rekordyNocy(c.p.pliki), poScaleniu);
    SPRAWDZ(zawiera(tc.diagnostykaTestu, "historia " + std::to_string(poScaleniu) + " nocy"));  // integracja: log U-05
    printf("  przejście (C) noce z AirBridge przed przywróceniem, scalenie: %d nocy\n", rekordyNocy(c.p.pliki));
  }
}

// ============================================================================================================
// Dodatkowe przypadki magazynu (scalanie przywracanego zrzutu, rotacja kopii), dane syntetyczne.
// ============================================================================================================

static const uint8_t BIT_SRODEK_P03 = (uint8_t)(1u << (int)Przycisk::SRODEK);

// U2 wariant A: udane przywrócenie stawia znacznik `cpap`/`przywr`; następny start zasilania bez okna, `W` poza oknem.
static void testT_WYM_87_znacznikPoUdanymPrzywroceniu() {
  PanelP03 s;
  partycjaSkasowana(s);
  std::map<std::string, std::string> z = zrzut(40);
  std::string b;
  for (auto& kv : z) b += komendaW(kv.first, kv.second);
  doPortu(s, b + "K");
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "przywrócono 40 z 40"));
  std::string zn = s.p.nvs.teksty[std::string("cpap/") + NVS_KLUCZ_PRZYWR];
  SPRAWDZ(zn.size() > 3 && zn.compare(zn.size() - 3, 3, " 40") == 0);
  SPRAWDZ(!s.p.nvs.teksty.count(std::string("cpap/") + NVS_KLUCZ_PRZYWR_ODRZ));
  doPortu(s, komendaW("czynniki.csv", z["czynniki.csv"]));
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(!zawiera(t.linieTestu, "Okno przywracania"));  // integracja ramka U-05 wielką literą
  SPRAWDZ(logMa(s, "BLAD czynniki.csv poza oknem"));
  SPRAWDZ(s.siec.pierwszeMs < 60u * 1000);
  // nowa partycja (np. ponowne przejście): znacznik skasowany przy formatowaniu, okno znowu
  partycjaSkasowana(s);
  s.p.nvs.teksty.erase(std::string("cpap/") + NVS_KLUCZ_FS_INIT);
  t = wybudz(s, ep(2026, 11, 4, 10, 0), PowodWybudzenia::START);
  ROWNE_L(s.p.pliki.formatowan, 2);
  SPRAWDZ(zawiera(t.linieTestu, "Okno przywracania do 10:10"));  // integracja: ramka U-05
  // znacznik udanego przywrócenia skasowany przy formatowaniu, a to
  // okno skończyło się bez żadnej komendy `W` (próba też skasowana) → znacznik „bez przywracania”, nie znacznik
  // udanego przywrócenia (wcześniej: brak klucza)
  SPRAWDZ(s.p.nvs.teksty[std::string("cpap/") + NVS_KLUCZ_PRZYWR].compare(0, 16, "bez przywracania") == 0);
}

// Integracja na U-05: pliki zrzutu odrzucone w oknie i nie przyjęte później → w następnym oknie
// U-05 `Pamięć panelu: odrzucone {n} plik/pliki/plików`, pełna lista w logu `Dane: … przywracanie: odrzucone …`;
// po przyjęciu plików i udanym przywróceniu znika.
static void testU05_odrzuconePlikiZrzutu() {
  for (int wariant = 0; wariant < 2; wariant++) {
    PanelP03 s;
    partycjaSkasowana(s);
    std::map<std::string, std::string> z = zrzut(40);
    std::vector<std::string> odrz = wariant == 0 ? std::vector<std::string>{"czynniki.csv"}
                                                : std::vector<std::string>{"czynniki.csv", "humor.csv", "sypialnia.csv"};
    std::string b;
    for (auto& kv : z) {
      std::string w = komendaW(kv.first, kv.second);
      if (std::find(odrz.begin(), odrz.end(), kv.first) != odrz.end()) w[w.size() - 5] = w[w.size() - 5] == '1' ? '2' : '1';
      b += w;
    }
    doPortu(s, b + "K");
    odpowiedziDoby(s, ep(2026, 11, 4, 7, 0));
    wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    SPRAWDZ(logMa(s, "BLAD czynniki.csv skrot"));
    SPRAWDZ(!s.p.nvs.teksty[std::string("cpap/") + NVS_KLUCZ_PRZYWR_ODRZ].empty());
    // następny start zasilania: okno znowu, U-05 z odrzuconymi plikami
    std::string popr;
    for (auto& n : odrz) popr += komendaW(n, z[n]);
    doPortu(s, popr + "K");
    blokada(s, 0);
    TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
    if (wariant == 0) {
      SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: odrzucone 1 plik"));
      SPRAWDZ(zawiera(t.diagnostykaTestu, "przywracanie: odrzucone czynniki.csv"));
    } else {
      SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: odrzucone 3 pliki"));
      SPRAWDZ(zawiera(t.diagnostykaTestu, "przywracanie: odrzucone czynniki.csv,humor.csv,sypialnia.csv"));
    }
    SPRAWDZ(zawiera(t.linieTestu, "Okno przywracania do 09:10"));
    SPRAWDZ(logMa(s, "przywrócono 40 z 40"));
    SPRAWDZ(!s.p.nvs.teksty[std::string("cpap/") + NVS_KLUCZ_PRZYWR].empty());
    SPRAWDZ(!s.p.nvs.teksty.count(std::string("cpap/") + NVS_KLUCZ_PRZYWR_ODRZ));
  }
}

// Okno przywracania: testy uzupełniające.
// Jedno wybudzenie START z podaną przyczyną startu (np. restart po aktualizacji przez Wi-Fi, błąd programu).
static TekstyWybudzenia wybudzStart(PanelP03& s, uint32_t t, PrzyczynaStartu p) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  s.p.start.stan = StanRtc();
  s.p.start.przyczynaStartu = p;
  s.p.start.powodWybudzenia = PowodWybudzenia::START;
  s.siec.polaczonaTeraz = false;
  s.siec.okno = false;
  s.siec.pierwszeMs = UINT32_MAX;
  s.p.d.z.clear();
  s.p.port.log.clear();
  Dyrygent dy(s.adaptery(), s.konf);
  dy.wybudzenie();
  return dy.teksty();
}
static std::string nvsP03(PanelP03& s, const char* k) {
  auto i = s.p.nvs.teksty.find(std::string("cpap/") + k);
  return i == s.p.nvs.teksty.end() ? "" : i->second;
}

// przywracanie czeka (próba `W` była, przywrócenia nie ma), a start nie jest startem zasilania: restart po
// aktualizacji (ESP_RST_SW), błąd programu, strażnik → bez okna, sieć od razu; start zasilania i reset pinem (EXT)
// → okno.
static void testT_WYM_87_r2_oknoTylkoPoStarcieZasilania() {
  for (PrzyczynaStartu p : {PrzyczynaStartu::PROGRAMOWY, PrzyczynaStartu::PANIKA, PrzyczynaStartu::TASK_WDT,
                            PrzyczynaStartu::SPADEK, PrzyczynaStartu::ZASILANIE, PrzyczynaStartu::ZEWNETRZNY}) {
    PanelP03 s;
    zNocami(s, 30);
    ustawZnacznik(s);
    s.p.nvs.flagi[std::string("cpap/") + NVS_KLUCZ_PRZYWR_PROBA] = true;
    blokada(s, 0);
    TekstyWybudzenia t = wybudzStart(s, ep(2026, 11, 5, 9, 0), p);
    bool okno = p == PrzyczynaStartu::ZASILANIE || p == PrzyczynaStartu::ZEWNETRZNY;
    ROWNE_L(logMa(s, "okno przywracania do"), okno);
    if (!okno) SPRAWDZ(s.siec.pierwszeMs < 60u * 1000);
    if (okno) SPRAWDZ(logMa(s, "okno przywracania: koniec czasu"));
    SPRAWDZ(nvsP03(s, NVS_KLUCZ_PRZYWR).empty());  // była próba: okno wraca do skutku, bez znacznika
  }
}

// nowy egzemplarz testowy: pierwsze formatowanie, okno 10 min bez komendy → znacznik „bez przywracania”, następny
// start zasilania bez okna; `?` poza oknem z podpowiedzią; komenda `W` poza oknem zdejmuje znacznik, okno wraca.
// Znacznik udanego przywrócenia komenda `W` poza oknem nie zdejmuje.
static void testT_WYM_87_r2_nowyEgzemplarzOknoRaz() {
  PanelP03 s;
  partycjaSkasowana(s);
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 6, 18, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "okno przywracania: koniec czasu"));
  SPRAWDZ(logMa(s, "bez komendy W"));
  ROWNE(nvsP03(s, NVS_KLUCZ_PRZYWR), "bez przywracania 2026-11-06");
  doPortu(s, "?");
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 7, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(!logMa(s, "okno przywracania do"));
  SPRAWDZ(s.siec.pierwszeMs < 60u * 1000);
  SPRAWDZ(logMa(s, "przywracanie: poza oknem, bez przywracania"));
  doPortu(s, komendaW("noce.csv", ""));
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 7, 10, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "BLAD noce.csv poza oknem"));
  SPRAWDZ(nvsP03(s, NVS_KLUCZ_PRZYWR).empty());
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 7, 11, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "okno przywracania do"));
  SPRAWDZ(nvsP03(s, NVS_KLUCZ_PRZYWR).empty());  // `W` poza oknem liczy się jako próba: okno wraca do skutku
  // znacznik udanego przywrócenia zostaje mimo komendy `W` poza oknem
  s.p.nvs.teksty[std::string("cpap/") + NVS_KLUCZ_PRZYWR] = "2026-11-07 40";
  doPortu(s, komendaW("noce.csv", ""));
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 7, 12, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "BLAD noce.csv poza oknem"));
  ROWNE(nvsP03(s, NVS_KLUCZ_PRZYWR), "2026-11-07 40");
}

// `K` przy komplecie zapisane (`przywr_k`), zanik zasilania przed znacznikiem → przy następnym starcie przywrócenie
// przed oknem stawia znacznik i okna już nie ma. `K` bez pliku nocy → bez `przywr_k`, bez znacznika.
static void testT_WYM_87_r2_znacznikPoZanikuPoK() {
  {
    PanelP03 s;
    zNocami(s, 0);
    s.p.pliki.pliki.erase("/dane/noce/2026.csv");
    ustawZnacznik(s);
    std::map<std::string, std::string> z = zrzut(40);
    for (auto& kv : z) s.p.pliki.pliki[std::string(KATALOG_PRZYWRACANIA) + "/" + kv.first] = kv.second;
    s.p.nvs.flagi[std::string("cpap/") + NVS_KLUCZ_PRZYWR_K] = true;
    s.p.nvs.flagi[std::string("cpap/") + NVS_KLUCZ_PRZYWR_PROBA] = true;
    blokada(s, 0);
    wybudz(s, ep(2026, 11, 5, 9, 0), PowodWybudzenia::START);
    SPRAWDZ(logMa(s, "przywrócono 40 z 40"));
    ROWNE(nvsP03(s, NVS_KLUCZ_PRZYWR), "2026-11-05 40");
    SPRAWDZ(!logMa(s, "okno przywracania do"));
  }
  {
    PanelP03 s;
    partycjaSkasowana(s);
    std::map<std::string, std::string> z = zrzut(40);
    doPortu(s, komendaW("czynniki.csv", z["czynniki.csv"]) + "K");
    blokada(s, 0);
    wybudz(s, ep(2026, 11, 5, 9, 0), PowodWybudzenia::START);
    SPRAWDZ(logMa(s, "zakończone komendą K (bez pliku nocy)"));
    SPRAWDZ(!s.p.nvs.flagi[std::string("cpap/") + NVS_KLUCZ_PRZYWR_K]);
    SPRAWDZ(nvsP03(s, NVS_KLUCZ_PRZYWR).empty());
    SPRAWDZ(s.p.nvs.flagi[std::string("cpap/") + NVS_KLUCZ_PRZYWR_PROBA]);
  }
}

// U2 wariant A: wybudzenie z uśpienia nie otwiera okna (tylko start zasilania), nawet gdy przywracanie czeka.
static void testT_WYM_87_bezOknaPoUspieniu() {
  PanelP03 s;
  partycjaSkasowana(s);
  odpowiedziDoby(s, ep(2026, 11, 4, 7, 0));
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);  // okno minęło bez plików
  odpowiedziDoby(s, ep(2026, 11, 4, 8, 0));
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 8, 0), PowodWybudzenia::ZEGAR);
  SPRAWDZ(!zawiera(t.linieTestu, "Okno przywracania"));  // integracja: ramka U-05
  SPRAWDZ(!logMa(s, "okno przywracania do"));
}

// U3: `?` w oknie → wiersz stanu okna;: przycisk kończy okno (sieć przed końcem 10 min, bez plików).
static void testT_WYM_87_zapytanieIPrzycisk() {
  PanelP03 s;
  partycjaSkasowana(s);
  doPortu(s, "?");
  s.p.przyciski.skrypt = {{60000, BIT_SRODEK_P03}, {60300, 0}};
  odpowiedziDoby(s, ep(2026, 11, 4, 7, 0));
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  int odp = 0;
  for (auto& l : s.p.port.log) odp += l.compare(0, 24, "okno przywracania do 07:") == 0 ? 1 : 0;
  ROWNE_L(odp, 2);  // wiersz przy otwarciu i odpowiedź na `?`
  SPRAWDZ(logMa(s, "okno przywracania: zakończone przyciskiem"));
  SPRAWDZ(s.siec.pierwszeMs >= 60000u && s.siec.pierwszeMs < 2u * 60 * 1000);
  // poza oknem: `?` → `przywracanie: poza oknem`
  PanelP03 q;
  zNocami(q, 30);
  doPortu(q, "?");
  blokada(q, 0);
  wybudz(q, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(q, "przywracanie: poza oknem"));
}

// U2 wariant A (reguła scalania, karta D40): noc we flash częściowa albo pusta → noc ze zrzutu (z `import` z flash);
// noc pełna we flash zostaje; czynniki i dziennik z tą samą datą: flash; daty tylko w zrzucie dochodzą.
static void testT_WYM_62_regulaScalania() {
  Noc pelnaF = nocTestowa("2026-11-02", 1), czescF = nocTestowa("2026-11-03", 2), pustaF = nocTestowa("2026-11-04", 3);
  czescF.czesciowa = true;
  czescF.wazna = false;
  czescF.import = 1793700000u;
  pustaF.m01 = -1;
  pelnaF.import = 1793600000u;
  Noc pelnaZ = nocTestowa("2026-11-02", 7), czescZ = nocTestowa("2026-11-03", 8), pustaZ = nocTestowa("2026-11-04", 9),
      nowaZ = nocTestowa("2026-11-01", 10);
  std::vector<std::string> fl = {wierszNocyV1(pelnaF), wierszNocyV1(czescF), wierszNocyV1(pustaF)};
  std::vector<std::string> zr = {wierszNocyV1(nowaZ), wierszNocyV1(pelnaZ), wierszNocyV1(czescZ), wierszNocyV1(pustaZ)};
  int d = -1;
  std::vector<std::string> w = scalWiersze("noce", fl, zr, &d);
  ROWNE_L(w.size(), 4u);
  ROWNE_L(d, 3);  // noc 1.11 nowa, 3.11 i 4.11 zastąpione
  ROWNE(w[0], wierszNocyV1(nowaZ));
  ROWNE(w[1], wierszNocyV1(pelnaF));  // pełna we flash zostaje
  Noc oczek = czescZ;
  oczek.import = czescF.import;
  ROWNE(w[2], wierszNocyV3(oczek));  // wersja 3 // zastąpiona noc w wersji 2 (pola wersji 2 z flash)
  ROWNE(w[3], wierszNocyV3(pustaZ));
  // noc częściowa w obu źródłach: flash
  Noc c2 = czescZ;
  c2.czesciowa = true;
  ROWNE(scalWiersze("noce", {wierszNocyV1(czescF)}, {wierszNocyV1(c2)})[0], wierszNocyV1(czescF));
  // czynniki: ten sam dzień → flash, inny → dochodzi; kolejność po dacie
  std::string cf = zapiszWpis(WpisCzynnikow{"2026-10-20", 'O', CZ_STRES, 'K', -1, true});
  std::string cz = zapiszWpis(WpisCzynnikow{"2026-10-20", 'O', CZ_KOLACJA, 'K', -1, true});
  std::string cn = zapiszWpis(WpisCzynnikow{"2026-10-19", 'O', CZ_KOLACJA, 'K', -1, true});
  std::vector<std::string> wc = scalWiersze("czynniki", {cf}, {cz, cn}, &d);
  ROWNE_L(wc.size(), 2u);
  ROWNE(wc[0], cn);
  ROWNE(wc[1], cf);
  ROWNE_L(d, 1);
  // dziennik wybudzeń: czas unix jako klucz, kolejność liczbowa
  std::vector<std::string> wb = scalWiersze("wybudzenia", {"1793746800;3950;1;1;x"}, {"1793743200;3900;1;1;y", "1793746800;1;1;1;z"});
  ROWNE_L(wb.size(), 2u);
  ROWNE(wb[0], "1793743200;3900;1;1;y");
  ROWNE(wb[1], "1793746800;3950;1;1;x");
}

// U7: czwarte uszkodzenie tego samego pliku → najwyżej KOPII_USZKODZONYCH kopii, najnowsza bez numeru, najstarsza znika.
static void testT_WYM_53_kopieUszkodzonych() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  const std::string s = "/dane/czynniki.csv";
  for (int i = 1; i <= 4; i++) {
    pl.pliki[s] = plik("czynniki", 1, POLA_CZYNNIKI, {wierszFormatu("czynniki", i)}, false) + "#koniec " +
                  std::to_string(10 + i) + "\n";  // zła liczba = uszkodzony
    Log lg;
    Magazyn m(pl, lg.f());
    SPRAWDZ(m.czytaj("czynniki", s).stan == StanPliku::USZKODZONY);
  }
  int kopii = 0;
  for (auto& kv : pl.pliki) kopii += kv.first.compare(0, s.size() + 11, s + ".uszkodzony") == 0 ? 1 : 0;
  ROWNE_L(kopii, KOPII_USZKODZONYCH);
  SPRAWDZ(pl.tresc(s + ".uszkodzony").find("#koniec 14") != std::string::npos);
  SPRAWDZ(pl.tresc(s + ".uszkodzony.1").find("#koniec 13") != std::string::npos);
  SPRAWDZ(pl.tresc(s + ".uszkodzony.2").find("#koniec 12") != std::string::npos);
}

// U5: znacznik migracji `w toku` (zanik zasilania w trakcie) → pliki nowej ścieżki usunięte i migracja od nowa;
// znacznik `ok` z tym samym CRC → bez migracji; migracja 1 → 2 przy niepotwierdzonej 0 → 1 nie usuwa `/noce.csv`.
static void testT_WYM_43_znacznikMigracji() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  std::vector<std::string> noce = noceV0(50, "2026-11-03");
  pl.pliki["/noce.csv"] = plikV0(noce);
  pl.pliki[znacznikMigracji("noce")] = "w toku\n";
  pl.pliki["/dane/noce/2026.csv"] = plikNoce(noceZnane(3, "2026-11-03"));  // z przerwanej
  {
    Log lg;
    Magazyn m(pl, lg.f());
    SPRAWDZ(m.migrujNaNoweSciezki());
    SPRAWDZ(zawiera(lg.w, "przerwana wcześniej"));
  }
  int n = 0;
  for (auto& w : wiersze(pl.tresc("/dane/noce/2025.csv")) ) n += w.compare(0, 2, "20") == 0;
  for (auto& w : wiersze(pl.tresc("/dane/noce/2026.csv")) ) n += w.compare(0, 2, "20") == 0;
  ROWNE_L(n, 50);
  SPRAWDZ(pl.tresc(znacznikMigracji("noce")).compare(0, 3, "ok ") == 0);
  int zapisy = pl.zapisy;
  {
    Log lg;
    Magazyn m(pl, lg.f());
    SPRAWDZ(m.migrujNaNoweSciezki());
    SPRAWDZ(m.migracje().empty());
  }
  ROWNE_L(pl.zapisy, zapisy);
  // znacznik niepotwierdzony (np. inny CRC po zmianie pliku prototypu): 1 → 2 bez usunięcia `/noce.csv`
  pl.pliki[znacznikMigracji("noce")] = "w toku\n";
  std::vector<FormatPliku> r2;
  SPRAWDZ(rejestrV2(r2, "noce"));
  Log lg;
  Magazyn m2(pl, lg.f(), r2);
  m2.migrujWMiejscu();
  SPRAWDZ(pl.pliki.count("/noce.csv") == 1);
  SPRAWDZ(zawiera(lg.w, "niepotwierdzona"));
}

// Ścieżki błędów znacznika migracji, scalenia i przywrócenia, pokrycie): znacznik `ok` nie daje się zapisać,
// plik nowej ścieżki z nowszej wersji, odmowa zapisu przy scaleniu, zmiana nazwy nieudana przy przywróceniu.
class PlikiBezZnacznikaOk : public AtrapaPlikow {
 public:
  explicit PlikiBezZnacznikaOk(Dziennik& d) : AtrapaPlikow(d) {}
  size_t zapisz(const char* n, const std::string& t) override {
    if (std::string(n).find(".migracja-") != std::string::npos && t.compare(0, 2, "ok") == 0) return 0;
    return AtrapaPlikow::zapisz(n, t);
  }
};
static void testT_WYM_43_bledyZnacznikaIScalenia() {
  {  // migracja: znacznik `ok` nie zapisany → pliki nowej ścieżki usunięte, źródło nietknięte
    Dziennik d;
    PlikiBezZnacznikaOk pl(d);
    montowana(pl);
    std::string v0 = plikV0(noceV0(20, "2026-11-03"));
    pl.pliki["/noce.csv"] = v0;
    Log lg;
    Magazyn m(pl, lg.f());
    SPRAWDZ(!m.migrujNaNoweSciezki());
    SPRAWDZ(m.sciezki("noce").empty());
    ROWNE(pl.tresc("/noce.csv"), v0);
    SPRAWDZ(lg.ma("znacznik migracji"));
  }
  {  // scalenie: znacznik `ok` nie zapisany → pliki scalone zostają, błąd w logu
    Dziennik d;
    PlikiBezZnacznikaOk pl(d);
    montowana(pl);
    pl.pliki["/noce.csv"] = plikV0(noceV0(20, "2026-11-03"));
    pl.pliki["/dane/noce/2026.csv"] = plikNoce(noceZnane(5, "2026-11-03"));
    Log lg;
    Magazyn m(pl, lg.f());
    SPRAWDZ(!m.migrujNaNoweSciezki());
    SPRAWDZ(lg.ma("scalenie z plikami nowej ścieżki nieudane (znacznik migracji)"));
    ROWNE_L(rekordyNocy(pl), 20);
  }
  {  // scalenie: plik nowej ścieżki z nowszej wersji → bez zapisu
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    pl.pliki["/noce.csv"] = plikV0(noceV0(20, "2026-11-03"));
    std::string v2 = naglowekNoce(wersjaNoce() + 1) + "\n" + polaNoceZnane() + ";test_v2\n#koniec 0\n";
    pl.pliki["/dane/noce/2026.csv"] = v2;
    Log lg;
    Magazyn m(pl, lg.f());
    SPRAWDZ(!m.migrujNaNoweSciezki());
    ROWNE(pl.tresc("/dane/noce/2026.csv"), v2);
    // przywrócenie na ten sam plik z nowszej wersji: niezgodna, bez zapisu
    std::map<std::string, std::string> z = zrzut(5);
    for (auto& kv : z) pl.pliki["/przywracanie/" + kv.first] = kv.second;
    Magazyn::WynikPrzywracania w = m.przywrocZKatalogu("/przywracanie");
    SPRAWDZ(w.proba && !w.ok);
    ROWNE(pl.tresc("/dane/noce/2026.csv"), v2);
  }
  {  // scalenie: odmowa zapisu → błąd, plik nowej ścieżki bez zmian
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    pl.pliki["/noce.csv"] = plikV0(noceV0(20, "2026-11-03"));
    std::string v1 = plikNoce(noceZnane(5, "2026-11-03"));
    pl.pliki["/dane/noce/2026.csv"] = v1;
    pl.odmawiaZapisuPod = "/dane/noce";
    Log lg;
    Magazyn m(pl, lg.f());
    SPRAWDZ(!m.migrujNaNoweSciezki());
    ROWNE(pl.tresc("/dane/noce/2026.csv"), v1);
  }
  {  // przywrócenie: zmiana nazwy nieudana → niezgodna, pliki `.tmp` usunięte
    Dziennik d;
    PlikiZBledami pl(d);
    montowana(pl);
    std::map<std::string, std::string> z = zrzut(5);
    for (auto& kv : z) pl.pliki["/przywracanie/" + kv.first] = kv.second;
    pl.bezZmianyNazwy = true;
    Log lg;
    Magazyn m(pl, lg.f());
    Magazyn::WynikPrzywracania w = m.przywrocZKatalogu("/przywracanie");
    SPRAWDZ(!w.ok && w.powod.compare(0, 12, "zmiana nazwy") == 0);
    int tmp = 0;
    for (auto& kv : pl.pliki) tmp += kv.first.size() > 4 && kv.first.compare(kv.first.size() - 4, 4, ".tmp") == 0;
    ROWNE_L(tmp, 0);
  }
}

// ---------- U-05 według kanwy ----------
// wymaganie: T-WYM-39, R-KONF-41, T-WYM-45, T-WYM-87
// Klatka 800×480 z atrapy wyświetlacza (bit 1 = czarny).
static bool czarnyK(const std::vector<uint8_t>& k, int x, int y) { return (k[(size_t)(y * 100 + x / 8)] >> (7 - (x & 7))) & 1; }
// Kreski i ramka U-05 w pozycjach z kanwy (design/kanwa/U05-test-sprzetu.dc.html); wierszy w lewej i prawej kolumnie
// (0 = kolumny nie ma: bez nazwy i kresek).
static bool ukladU05(const std::vector<uint8_t>& k, int lewa, int prawa) {
  auto linia = [&](int y, int x0, int x1) {
    for (int x = x0; x <= x1; x++) if (!czarnyK(k, x, y)) return false;
    return true;
  };
  bool ok = linia(48, 0, 799) && linia(49, 0, 799) && linia(436, 0, 799) && linia(437, 0, 799);
  for (int y : {378, 379, 420, 421}) ok = ok && linia(y, 26, 773);
  for (int y = 378; y <= 421; y++) ok = ok && czarnyK(k, 26, y) && czarnyK(k, 773, y);
  for (int x0 : {26, 414}) {
    int wierszy = x0 == 26 ? lewa : prawa;
    ok = ok && linia(89, x0, x0 + 359) == (wierszy > 0);
    for (int i = 0; i < 5; i++) ok = ok && linia(137 + 47 * i, x0, x0 + 359) == (i < wierszy);
  }
  return ok;
}
static void zapiszU05(const std::vector<uint8_t>& k, const char* nazwa) {  // CPAP_U05_PBM = katalog na podgląd
  const char* kat = getenv("CPAP_U05_PBM");
  if (!kat || !*kat) return;
  std::ofstream f(std::string(kat) + "/" + nazwa + ".pbm", std::ios::binary);
  f << "P4\n800 480\n";
  f.write((const char*)k.data(), (std::streamsize)k.size());
}
// Wejście na U-05 z U-03 (ciąg U-01 ▶ U-03 ▶ U-05; ▶ na `Test sprzętu` w U-01, wcześniej z K2 ◀ 2 s)
// o 10:00 przy 30 nocach w plikach.
static TekstyWybudzenia naU05(PanelP03& s) {
  blokada(s, 0);
  s.p.start.budzace = BIT_PRAWY;
  s.p.start.stan.ekran = (uint8_t)Ekran::FORMULARZ;
  s.p.przyciski.skrypt = {{0, BIT_PRAWY}, {2300, 0}};  // U-05 z U-03 przez ▶ 2 s
  return wybudz(s, ep(2026, 11, 4, 10, 0), PowodWybudzenia::PRZYCISK);
}

// U-05 z oknem Wi-Fi: 10 wierszy w kolejności kanwy z wartościami z danych firmware (T-WYM-39), ramka z godziną końca
// okna, numer wydania bez skrótu commitu (skrót tylko w logu), diagnostyka prototypu w logu portu, układ klatki z kanwy.
static void testU05_wierszeZDanychFirmware() {
  ROWNE(wersjaWydania("1.0.0+abc1234"), "1.0.0");  // 8.7: na ekranie numer wydania, skrót commitu tylko w logu
  ROWNE(wersjaWydania("1.0.1"), "1.0.1");
  ROWNE(wersjaWydania(nullptr), "");
  PanelP03 s;
  zNocami(s, 30);
  TekstyWybudzenia t = naU05(s);
  std::vector<std::string> ocz = {"Oprogramowanie: 1.0.0 (app0)", "Zestaw reguł: wbudowany", "Pamięć panelu: wolne 23 MB",
                                  "Karta: brak karty", "Źródło otoczenia: wyłączone", "Panel: 192.168.1.50",
                                  "Wi-Fi: Dom −60 dBm", "AirBridge: 192.168.1.20 · v2.2.1", "Ostatnie pobranie: –",
                                  "Bateria: 70%", "Wgrywanie przez Wi-Fi do 10:10", "hasła i dane terapii nie są pokazywane"};
  SPRAWDZ(t.linieTestu.size() >= ocz.size());
  for (size_t i = 0; i < ocz.size() && i < t.linieTestu.size(); i++) ROWNE(t.linieTestu[i], ocz[i]);
  SPRAWDZ(!zawiera(t.linieTestu, "abc1234") && !zawiera(t.linieTestu, "proto"));
  SPRAWDZ(logMa(s, "U-05 · Wersja: 1.0.0+abc1234 · slot app0"));
  for (const char* d : {"U-05 · Wybudzenie: przycisk", "U-05 · Wi-Fi: 192.168.1.50 · RSSI -60 dBm", "U-05 · AirBridge: 192.168.1.20",
                        "historia 30 nocy", "U-05 · Czynniki: ", "U-05 · Bateria 3,95 V", "U-05 · Otoczenie: wyłączone",
                        "U-05 · Pamięć: min. sterta", "U-05 · Dane: wersja plików 3"})  // noce w wersji 3 (M-18)
    SPRAWDZ(logMa(s, d));
  SPRAWDZ(!s.p.ekran.klatki.empty() && ukladU05(s.p.ekran.klatki.front(), 5, 5));
  if (!s.p.ekran.klatki.empty()) zapiszU05(s.p.ekran.klatki.front(), "u05-okno-wifi");
  // bez hasła okna (T-WYM-45): `Wgrywanie przez Wi-Fi: wyłączone`, bez godziny
  PanelP03 b;
  b.konf.oknoZHaslem = false;
  zNocami(b, 30);
  TekstyWybudzenia tb = naU05(b);
  SPRAWDZ(zawiera(tb.linieTestu, "Wgrywanie przez Wi-Fi: wyłączone") && !zawiera(tb.linieTestu, "Wgrywanie przez Wi-Fi do"));
  // AirBridge bez pola `version` (R-KONF-41: `{adres} · wersja nieznana`, w kolumnie krótszy wariant)
  PanelP03 v;
  zNocami(v, 30);
  blokada(v, 0);
  v.siec.odpowiedzi.clear();
  v.siec.dodaj("/api/status", 200, "{\"rop\":1,\"report_state\":\"blocked\"}");
  v.p.start.budzace = BIT_PRAWY;
  v.p.start.stan.ekran = (uint8_t)Ekran::FORMULARZ;  // U-05 z U-03
  v.p.przyciski.skrypt = {{0, BIT_PRAWY}, {2300, 0}};  // ▶ 2 s
  TekstyWybudzenia tv = wybudz(v, ep(2026, 11, 4, 10, 0), PowodWybudzenia::PRZYCISK);
  // `192.168.1.20 · wersja nieznana` ma 281 px przy 259 px miejsca obok nazwy: krótszy wariant
  SPRAWDZ(zawiera(tv.linieTestu, "AirBridge: 192.168.1.20 · bez wersji"));
  // przestrzeń NVS z nowszej wersji (T-WYM-54): `Pamięć panelu: dane z nowszej wersji…`, pełny tekst w diagnostyce
  PanelP03 nv;
  zNocami(nv, 30);
  nv.p.nvs.liczbyU["cpap/" + std::string(NVS_KLUCZ_WERSJA)] = 99;
  TekstyWybudzenia tn = naU05(nv);
  SPRAWDZ(zawiera(tn.linieTestu, "Pamięć panelu: dane z nowszej wersji") &&
          zawiera(tn.diagnostykaTestu, "dane z nowszej wersji: NVS cpap"));
}

// R-KONF-41: U-05 bez połączenia z Wi-Fi pokazuje ostatnie połączenie z godziną w nawiasie, a AirBridge `brak odpowiedzi
// od {dd.mm hh:mm}` (ostatnia udana odpowiedź); przed pierwszym połączeniem `brak połączenia z Wi-Fi`.
static void testU05_bezWifiOstatniePolaczenie() {
  PanelP03 s;
  zNocami(s, 30);
  naU05(s);  // 10:00: połączenie i odpowiedź AirBridge
  s.siec.siecJest = false;
  s.p.start.budzace = BIT_PRAWY;
  s.p.przyciski.skrypt = {{0, BIT_PRAWY}, {2300, 0}};  // ▶ 2 s
  s.p.start.stan.ekran = (uint8_t)Ekran::FORMULARZ;  // U-05 z U-03
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK);
  SPRAWDZ(zawiera(t.linieTestu, "Panel: 192.168.1.50 (10:00)"));
  SPRAWDZ(zawiera(t.linieTestu, "Wi-Fi: Dom −60 dBm (10:00)"));
  SPRAWDZ(zawiera(t.linieTestu, "AirBridge: brak odp. od 04.11 10:00"));
  SPRAWDZ(zawiera(t.linieTestu, "Wgrywanie przez Wi-Fi: brak sieci"));
  PanelP03 n;
  zNocami(n, 30);
  n.siec.siecJest = false;
  TekstyWybudzenia tn = naU05(n);
  SPRAWDZ(zawiera(tn.linieTestu, "Panel: brak połączenia z Wi-Fi") && zawiera(tn.linieTestu, "Wi-Fi: –"));
  SPRAWDZ(zawiera(tn.linieTestu, "AirBridge: brak odpowiedzi"));
}

// T-WYM-87, T-WYM-62, 8.9 krok 6: U-05 w oknie przywracania po pierwszym formatowaniu (ramka `Okno przywracania do
// {hh:mm}`, wiersze jak zwykle) i U-05 po przywróceniu 412 nocy (`Pamięć panelu` z wynikiem przywrócenia, liczba nocy
// w diagnostyce). Klatki do podglądu: CPAP_U05_PBM.
static void testU05_oknoPrzywracaniaIPo412() {
  PanelP03 s;
  partycjaSkasowana(s);
  std::string b;
  for (auto& kv : zrzut(412)) b += komendaW(kv.first, kv.second);
  doPortu(s, b + "K" + "O");  // `O` po oknie: U-05 w czuwaniu (okno serwisowe), jak testT_WYM_62_przejscieNaZrzucieZPanelu
  blokada(s, 0);
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(t.linieTestu.size() >= 24);
  if (t.linieTestu.size() < 24) return;
  ROWNE(t.linieTestu[10], "Okno przywracania do 07:10");
  ROWNE(t.linieTestu[11], "hasła i dane terapii nie są pokazywane");
  ROWNE(t.linieTestu[2], "Pamięć panelu: wolne 23 MB");  // świeżo sformatowana partycja
  ROWNE(t.linieTestu[14], "Pamięć panelu: przywrócono 412/412");  // 2. U-05: po przywróceniu
  SPRAWDZ(zawiera(t.diagnostykaTestu, "historia 412 nocy") && zawiera(t.diagnostykaTestu, "przywrócono 412 z 412"));
  SPRAWDZ(s.p.ekran.klatki.size() >= 2);
  if (s.p.ekran.klatki.size() < 2) return;
  SPRAWDZ(ukladU05(s.p.ekran.klatki.front(), 5, 5));
  zapiszU05(s.p.ekran.klatki.front(), "u05-okno-przywracania");
  for (auto& k : s.p.ekran.klatki)
    if (ukladU05(k, 5, 5) && &k != &s.p.ekran.klatki.front()) zapiszU05(k, "u05-po-przywroceniu-412");
}

// D52 (integracja): `Ostatnie pobranie` = chwila ostatniego udanego pobrania nocy z AirBridge (T-WYM-39), nie tylko
// `import` z rekordów nocy. (A) pobranie nocy zapisuje chwilę w NVS `cpap`/`pobr_nocy`; ponowne pobranie przesuwa ją,
// choć `import` nocy zostaje z pierwszego zapisu. (B) po przywróceniu 412 nocy ze zrzutu (noce bez `import`) U-05
// pokazuje chwilę z NVS zamiast `–`. Klatka (B) do podglądu: CPAP_U05_PBM.
static void testU05_ostatniePobraniePoPrzywroceniu() {
  const std::string klucz = std::string("cpap/") + NVS_KLUCZ_POBRANIE_NOCY;
  {
    PanelP03 s;
    zNocami(s, 30);
    odpowiedziDoby(s, ep(2026, 11, 4, 7, 0));
    wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    SPRAWDZ(s.p.nvs.liczbyU.count(klucz) && s.p.nvs.liczbyU[klucz] >= ep(2026, 11, 4, 7, 0) &&
            s.p.nvs.liczbyU[klucz] < ep(2026, 11, 4, 7, 10));
    TekstyWybudzenia t = naU05(s);
    SPRAWDZ(zawiera(t.linieTestu, "Ostatnie pobranie: 04.11 07:0"));
  }
  {
    PanelP03 s;
    partycjaSkasowana(s);
    s.p.nvs.liczbyU[klucz] = ep(2026, 11, 4, 5, 32);  // pobranie nocy na starym firmware: NVS zostaje przy przejściu 8.9
    std::string b;
    for (auto& kv : zrzut(412)) b += komendaW(kv.first, kv.second);
    doPortu(s, b + "K" + "O");
    blokada(s, 0);
    TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
    SPRAWDZ(t.linieTestu.size() >= 24);
    if (t.linieTestu.size() < 24) return;
    ROWNE(t.linieTestu[14], "Pamięć panelu: przywrócono 412/412");
    ROWNE(t.linieTestu[20], "Ostatnie pobranie: 04.11 05:32");  // przed D52: `–` (noce zrzutu bez `import`)
    SPRAWDZ(s.p.ekran.klatki.size() >= 2);
    for (auto& k : s.p.ekran.klatki)
      if (ukladU05(k, 5, 5) && &k != &s.p.ekran.klatki.front()) zapiszU05(k, "u05-po-przywroceniu-412-pobranie");
  }
}

// T-WYM-83, R-AW-39: U-05 w trybie awaryjnym według układu kanwy: tylko wersja ze slotem i przyczyna (2 wiersze, jedna
// kolumna), ramka z oknem, stopka `◀ wróć`; pełny identyfikator i liczba startów w logu.
static void testU05_trybAwaryjny() {
  Panel p;
  p.zegar.epoch = ep(2026, 11, 4, 10, 0);
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  ZnacznikAwaryjny z;
  z.jest = true;
  z.wersja = identyfikatorWersji(WERSJA_ATRAPY);
  z.przyczyna = PrzyczynaStartu::PANIKA;
  z.u06Narysowane = true;
  p.nvs.teksty["cpap/awaryjny"] = zapiszZnacznik(z);
  p.start.budzace = BIT_PRAWY;
  p.przyciski.skrypt = {{0, BIT_PRAWY}, {200, 0}};
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(!p.ekran.klatki.empty() && ukladU05(p.ekran.klatki.front(), 2, 0));
  if (!p.ekran.klatki.empty()) {
    const std::vector<uint8_t>& k = p.ekran.klatki.front();
    int prawa = 0, stopkaL = 0;
    for (int y = 52; y < 378; y++) for (int x = 386; x < 800; x++) prawa += czarnyK(k, x, y);
    for (int y = 440; y < 480; y++) for (int x = 0; x < 400; x++) stopkaL += czarnyK(k, x, y);
    SPRAWDZ(prawa == 0 && stopkaL == 0);  // bez prawej kolumny, w stopce tylko `◀ wróć` z prawej
    zapiszU05(k, "u05-tryb-awaryjny");
  }
  SPRAWDZ(zawiera(p.port.log, "U-05 · Wersja: 1.0.0+abc1234 · slot app0"));
  SPRAWDZ(zawiera(p.port.log, "U-05 · Tryb awaryjny: 3 starty po błędzie (błąd programu)"));
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  // (a) magazyn, formaty, funkcje rdzenia
  testT_WYM_50_naglowkiPlikow();
  testT_WYM_51_zapisAtomowyPelnaPamiec();
  testT_WYM_52_wierszKonca();
  testT_WYM_53_plikTymczasowy();
  testR_NF_12_zanikZasilania50Razy();
  testT_WYM_55_migracjaNocyKazdyUklad();
  testT_WYM_55_migracja400NocyDwaLata();
  testT_WYM_55_migracjaSypialni();
  testT_WYM_55_migracjaPozostalych();
  testT_WYM_57_poleTylkoNaKoncu();
  testT_WYM_38_tablicaPartycji();
  testR_NF_27_adresyLokalne();
  // (b) dyrygent z atrapami
  testR_NF_27_bezZadaniaPozaSiecia();
  testR_NF_27_przekierowanie();
  testT_WYM_50_nvsIPlikiPoPracy();
  testT_WYM_56_migracjaPrzerwana();
  testT_WYM_54_nowszyPlik();
  testR_NF_28_nowszyPlikNocy();
  testT_WYM_43_migracjaPrzedOznaczeniem();
  testT_WYM_43_odmowaZapisuPowrot();
  testT_WYM_43_migracjaWMiejscuPoOznaczeniu();
  testT_WYM_58_nvsTylkoPrzyZmianie();
  testT_WYM_84_partycjaSkasowana();
  testT_WYM_84_smieciBezSuperbloku();
  testT_WYM_84_uszkodzonySuperblok();
  testT_WYM_84_znacznikINieczytelna();
  testT_WYM_84_drugiStart();
  testT_WYM_84_staryUkladBezFormatowania();
  testT_WYM_34_bezFormatowania();
  testT_WYM_87_oknoPrzywracania();
  testT_WYM_87_zlySkrotIRozmiar();
  testT_WYM_87_pozaOknem();
  testT_WYM_87_oknoBezKomendy();
  testT_WYM_62_przywrocenie412();
  testT_WYM_62_niezgodnaLiczba();
  testR_DANE_37_pamiecZNocami();
  testR_AW_16_nieczytelnaBezPrzywracania();
  testR_NF_19_aktualizacjaZachowujeDane();
  testR_KON_41_importZRekorduPoZanikuZasilania();
  // (c) testy dodane przez implementację
  testT_WYM_38_identyfikatorUkladu();
  testT_WYM_84_znacznikIPartycjaSkasowana();
  testT_WYM_84_staryUkladPowrotDoPoprzedniej();
  testT_WYM_87_oknoBezZegara();
  testT_WYM_87_urwanaTresc();
  testT_WYM_53_uszkodzonyWZrzucie();
  testT_WYM_51_bledyZapisuMagazynu();
  testT_WYM_55_bledyMigracjiWMiejscu();
  testT_WYM_62_bladZapisuPrzywracania();
  testT_WYM_84_decyzjePartycji();
  testT_WYM_87_bledyOknaIKomendy();
  testT_WYM_87_znacznikPoUdanymPrzywroceniu();
  testT_WYM_87_bezOknaPoUspieniu();
  testT_WYM_87_r2_oknoTylkoPoStarcieZasilania();
  testT_WYM_87_r2_nowyEgzemplarzOknoRaz();
  testT_WYM_87_r2_znacznikPoZanikuPoK();
  testT_WYM_87_zapytanieIPrzycisk();
  testU05_odrzuconePlikiZrzutu();
  testT_WYM_62_regulaScalania();
  testT_WYM_53_kopieUszkodzonych();
  testT_WYM_43_znacznikMigracji();
  testT_WYM_43_bledyZnacznikaIScalenia();
  testT_WYM_62_przejscieNaZrzucieZPanelu();
  // (c) U-05 według kanwy
  testU05_wierszeZDanychFirmware();
  testU05_bezWifiOstatniePolaczenie();
  testU05_oknoPrzywracaniaIPo412();
  testU05_ostatniePobraniePoPrzywroceniu();
  testU05_trybAwaryjny();
  printf("testy_magazyn_danych: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
