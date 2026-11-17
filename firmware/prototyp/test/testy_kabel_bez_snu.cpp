// SPDX-License-Identifier: GPL-3.0-or-later
// Czekanie na zasilaniu zewnętrznym zamiast głębokiego snu (decyzja właściciela projektu 16.11.2026, ADR-0047, R-NF-45,
// T-WYM-10…T-WYM-14; testy T-TEST-500). Pisane przed kodem z wymagań:
//  A. decyzja rdzenia czekanie::decyzja(): tylko pewne „zewnętrzne” na starcie i przed snem, nie w trybie awaryjnym,
//     pamięć w budżecie, PSRAM, planowy restart (okno i twarde limity);
//  B. warstwa pamięci plików PlikiZPamiecia: odczyt z pamięci, każda zmiana przez warstwę unieważnia;
//  C. równoważność: doba i tydzień na JEDNYM żywym stanie (czekanie, pamięć przebiegów, pamięć plików, adapter ekranu
//     z kopią obrazu) wobec obecnej ścieżki (głęboki sen, nowy stan na każde wybudzenie): te same klatki, NVS, pliki,
//     wiersze logu (poza wierszami czekania i rodzajem odświeżenia), plan snu; częściowe tylko tam, gdzie R-NF-25;
//  D. liczniki na historii 366 nocy: przebieg przyciskiem po czekaniu bez oceny silnika, bez rozkładu nocy i bez
//     odczytu plików nocy z nośnika, pierwsze ▶ częściowe; po głębokim śnie jak dotąd;
//  E. odłączenie kabla (na starcie, przed snem, w trakcie czekania), stan nieznany: głęboki sen jak dotąd;
//  F. awaria: restart po błędzie w trakcie pracy bez snu wznawia jak po głębokim śnie, tryb awaryjny zawsze głęboki;
//  G. 30 dób na jednym żywym stanie: liczba żywych bloków sterty bez wzrostu.
// Uruchomienie: tools/prototyp/testy.sh (program testy_kabel_bez_snu). LOG_CZEKANIA=1 drukuje liczniki przebiegów.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <new>
#include <set>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "czekanie.h"
#include "dyrygent.h"
#include "magazyn.h"
#include "noc.h"
#include "pliki_pamiec.h"
#include "przebiegi.h"
#include "slot_dzienny.h"
#include "start.h"
#include "szybkosc.h"
#include "wspolne.h"
#include "wybudzenie.h"

// ---------- licznik żywych bloków sterty (G): globalny operator new/delete tego programu ----------
static long long zywychBlokow = 0;
void* operator new(std::size_t n) {
  void* p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc();
  zywychBlokow++;
  return p;
}
void* operator new[](std::size_t n) { return operator new(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
  void* p = std::malloc(n ? n : 1);
  if (p) zywychBlokow++;
  return p;
}
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept { return operator new(n, t); }
void operator delete(void* p) noexcept {
  if (!p) return;
  zywychBlokow--;
  std::free(p);
}
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { operator delete(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { operator delete(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { operator delete(p); }

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
#define ROWNE_L(a, b)                                                                                    \
  do {                                                                                                   \
    long long _a = (long long)(a), _b = (long long)(b);                                                  \
    if (_a == _b) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); }  \
  } while (0)

static bool logCzekania() { return getenv("LOG_CZEKANIA") != nullptr; }

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
static std::string hhmm(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  char b[24];
  strftime(b, sizeof b, "%m-%d %H:%M", &l);
  return b;
}

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static KonfiguracjaPanelu konfTestu = KONF;  // test I: ze współrzędnymi pogody (slot dzienny z prognozą)
static const char* STATUS_TERAPIA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* STATUS_PRZETWARZA =
    "{\"version\":\"v2.2.1\",\"mode\":\"ASVAuto\",\"rop\":0,\"edf\":\"post-processing\",\"edf_prefix\":\"\","
    "\"report_state\":\"idle\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";

// Raport doby (jak test/testy_szybciej_przycisk.cpp), z wahaniem AHI i nieszczelności od numeru doby.
static bool sesjaZDoby = false;  // test I: sesja nocy 23:20–06:40 doby `day` (inaczej stała chwila: ekran S10)
static std::string raport(long day) {
  std::string d = std::to_string(day);
  std::string sesja = "\"start_ms\":1793403600000,\"end_ms\":1793428920000";
  if (sesjaZDoby) {
    // doba AirBridge = data wieczoru początku sesji: północ po niej
    uint32_t polnoc = ep(2026, 11, 4, 0, 0) + (uint32_t)((day + 1 - nrDnia("2026-11-04")) * 86400L);
    sesja = "\"start_ms\":" + std::to_string(cywilneMs(polnoc - 40 * 60)) +
            ",\"end_ms\":" + std::to_string(cywilneMs(polnoc + 6 * 3600 + 40 * 60));
  }
  std::string ahi = std::to_string(2 + day % 5) + ".4", wp95 = std::to_string(12 + (day * 7) % 19) + ".6";
  if (sesjaZDoby) {  // test I: noc spokojna (bez linii uwagi, która zasłania wygaszacz, R-KON-54)
    ahi = "1.4";
    wp95 = "13.6";
  }
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\"," + sesja + "}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":" + ahi + ",\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":" +
         (sesjaZDoby ? "5" : "25") + ",\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Press\",\"label\":\"C\",\"unit\":\"cmH2O\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":6.2,\"mean\":10.1,\"median\":9.8,\"p95\":13.6,\"max\":16.0,"
         "\"source\":\"edf\"},{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":" + wp95 + ",\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}" +
         std::string(sesjaZDoby ? ",{\"tag\":\"EAI\",\"value\":\"5\"},{\"tag\":\"EAX\",\"value\":\"10\"},"
                                  "{\"tag\":\"ANS\",\"value\":\"3\"},{\"tag\":\"AXS\",\"value\":\"10\"}"
                                : "") +
         "]}}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}

// ---------- historia syntetyczna (jak test/testy_szybciej_przycisk.cpp) ----------
static uint32_t ziarno = 20261116u;
static float los(float a, float b) {
  ziarno = ziarno * 1664525u + 1013904223u;
  return a + (b - a) * (float)((ziarno >> 8) & 0xFFFF) / 65535.0f;
}
static Noc nocLos(const std::string& d, int i, int n) {
  Noc r;
  r.data = d;
  r.wazna = i % 37 != 5;
  r.m01 = (int)los(300, 480);
  r.ahi = los(0.6f, i > n - 20 && !sesjaZDoby ? 9.0f : 4.0f);
  r.cmed = los(9.0f, 10.8f);
  r.cp95 = los(12.0f, 13.8f);
  r.wmed = los(1.5f, 4.5f);
  r.wp95 = los(7.0f, i > n - 10 && !sesjaZDoby ? 30.0f : 14.0f);
  r.emed = los(6.0f, 7.2f);
  r.ep95 = los(7.5f, 8.8f);
  r.psmed = los(3.0f, 5.0f);
  r.psp95 = los(6.0f, 8.0f);
  r.zrodlo = i < n / 3 ? 'K' : 'A';
  r.oa = los(0.1f, 1.0f);
  r.ca = los(0.5f, i > n - 15 && !sesjaZDoby ? 8.0f : 3.0f);
  r.h = los(0.2f, 2.0f);
  r.csr = los(0.0f, 20.0f);
  if (r.zrodlo == 'A') {
    r.import = ep(2026, 1, 1, 7, 0) + (uint32_t)i * 86400u;
    r.m13 = los(12.0f, 17.0f);
    r.m17 = (int)los(0, 3);
    r.pna = "AirCurve_10_CS_PaceWave";
    r.srn = "23222345678";
    r.m08 = los(0.0f, 30.0f);
    r.eai = 5.0f;
    r.eax = 10.0f;
    r.ans = 3.0f;
    r.axs = 10.0f;
    r.ua = los(0.0f, 1.0f);
    r.m10 = los(0.0f, 0.1f);
    r.m10p = los(0.1f, 0.3f);
    r.tryb = "ASVAuto";
    r.ustawienia = sesjaZDoby ? "ASVAuto/5/10/3/10" : "ASVAuto/EAI5/EAX10/ANS3/AXS10";  // test I: jak raport
    r.m18 = (int)los(600, 760);
    r.m14 = los(400.0f, 600.0f);
    r.m15 = los(6.0f, 9.0f);
  }
  return r;
}
static void bezLogu(const std::string&) {}

// ---------- jeden panel: obecna ścieżka (głęboki sen) albo czekanie na kablu (ADR-0047) ----------
struct Bieg {
  Panel p;
  const bool czekanie;
  PlikiZPamiecia pl;
  PamiecPrzebiegow pp;
  int programow = 1;  // starty programu (czekanie: głęboki sen, restart = nowy program, jak src/main.cpp)
  explicit Bieg(bool c, const std::string& ostatnia, int nocy) : czekanie(c), pl(p.pliki) {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    ziarno = 20261116u;
    p.pliki.montowane = true;  // zapis przez Magazyn jak na zamontowanej partycji; dyrygent montuje sam
    Magazyn mag(p.pliki, bezLogu);
    std::map<std::string, std::vector<std::string>> lata;
    for (int i = 0; i < nocy; i++) {
      std::string d = dodajDni(ostatnia, i - (nocy - 1));
      lata[d.substr(0, 4)].push_back(wierszNocyV5(nocLos(d, i, nocy)));
    }
    for (auto& kv : lata) mag.zapisz("noce", mag.sciezkaRoku("noce", kv.first), kv.second);
    p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
    p.pliki.montowane = false;
    p.d.z.clear();
    p.czujniki.zas = Zasilanie::ZEWNETRZNE;
    p.sen.czekanieWlaczone = c;
    p.ekran.pamietaObraz = c;
  }
  Adaptery ad() {
    Pliki& pliki = czekanie ? static_cast<Pliki&>(pl) : static_cast<Pliki&>(p.pliki);
    return Adaptery{p.zegar, p.nvs, pliki, p.siec, p.ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik,
                    p.sen, p.port, p.diag, &p.wydania};
  }
  // nowy start programu (po głębokim śnie, restarcie, zaniku zasilania): pamięć programu ginie, RTC, NVS, pliki zostają
  void nowyProgram() {
    pp = PamiecPrzebiegow();
    pl.wlacz(false);
    p.ekran.zapomnij();
    programow++;
  }
};

// Wejście jednego przebiegu (jak po wybudzeniu z głębokiego snu albo po końcu czekania).
struct Wejscie {
  uint32_t ep = 0;
  PowodWybudzenia pw = PowodWybudzenia::ZEGAR;
  PrzyczynaStartu prz = PrzyczynaStartu::USPIENIE;
  uint8_t budzace = 0;
  std::vector<std::pair<uint32_t, uint8_t>> skrypt;
  std::vector<std::pair<int, uint32_t>> kolejka;
  std::string port;  // znaki komendy portu serwisowego
};

// Skrót FNV-1a.
struct Skrot {
  uint64_t k = 1469598103934665603ULL;
  void bajty(const void* d, size_t n) {
    const unsigned char* b = (const unsigned char*)d;
    for (size_t i = 0; i < n; i++) { k ^= b[i]; k *= 1099511628211ULL; }
    k ^= 0xFF; k *= 1099511628211ULL;
  }
  void tekst(const std::string& t) { bajty(t.data(), t.size()); }
  template <typename T>
  void pole(const T& v) { bajty(&v, sizeof v); }
};
// Wiersze logu, które z definicji różnią ścieżki: decyzja o czekaniu (T-WYM-10…T-WYM-12) i rodzaj odświeżenia.
static bool wierszCzekania(const std::string& l) {
  return l.rfind("sen: czekanie", 0) == 0 || l.rfind("sen: głęboki mimo", 0) == 0 || l.rfind("czekanie: ", 0) == 0 ||
         l.rfind("czekanie na kablu: ", 0) == 0;
}
static std::string bezRodzaju(const std::string& l) {
  size_t i = l.find(" narysowany w ");
  if (i == std::string::npos) return l;
  size_t j = l.find(" (odświeżenie", i);
  return j == std::string::npos ? l : l.substr(0, j);
}

struct Wynik {
  uint64_t klatki = 0, stan = 0, log = 0;
  int klatek = 0, czesciowych = 0;
  bool pierwszaCzesciowa = false;
  PlanSnu plan;
  bool spal = false, czekal = false, przerwane = false;
  unsigned long silnik = 0, parsowan = 0, trafien = 0, odczytowNocy = 0;
  // operacje na nośniku, których PlikiZPamiecia nie pamięta (od 17.11.2026)
  unsigned long pierwszych = 0, listowan = 0;
  uint32_t msPrzebiegu = 0;
  std::vector<std::string> wiersze;  // nowe wiersze logu bez wierszy czekania i rodzaju odświeżenia
};

// Odczyty treści plików nocy z nośnika. Bez prób odczytu `{plik}.tmp` (magazyn sprawdza nim przerwany zapis; pliku
// zwykle nie ma): od 17.11.2026 PlikiZPamiecia nie pamięta „braku pliku”, więc taka próba idzie na
// nośnik w każdym przebiegu, jak po głębokim śnie; to nie jest ponowne czytanie nocy. Bez pierwszyWiersz() (nagłówek
// do wersji pliku): od 17.11.2026 zawsze z nośnika, liczony osobno (sumaMapy(pierwsze)).
static unsigned long odczytyNocy(const AtrapaPlikow& f) {
  unsigned long n = 0;
  for (auto& kv : f.odczyty)
    if (kv.first.rfind("/dane/noce/", 0) == 0 &&
        (kv.first.size() < 4 || kv.first.compare(kv.first.size() - 4, 4, ".tmp") != 0)) {
      n += (unsigned long)kv.second;
      auto pw = f.pierwsze.find(kv.first);
      if (pw != f.pierwsze.end()) n -= (unsigned long)pw->second;
    }
  return n;
}
static unsigned long sumaMapy(const std::map<std::string, int>& m) {
  unsigned long n = 0;
  for (auto& kv : m) n += (unsigned long)kv.second;
  return n;
}

static Wynik przebieg(Bieg& b, const Wejscie& w) {
  Panel& p = b.p;
  p.zegar.epoch = w.ep;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = w.prz;
  p.start.powodWybudzenia = w.pw;
  p.start.budzace = w.budzace;
  p.przyciski.skrypt = w.skrypt;
  p.przyciski.kolejka = w.kolejka;
  p.siec.polaczonaTeraz = false;
  for (char c : w.port) p.port.znaki.push_back((unsigned char)c);
  size_t log0 = p.port.log.size(), kl0 = p.ekran.klatki.size();
  int snow0 = p.sen.snow, czek0 = p.sen.czekan, prz0 = p.sen.przerwanych;
  unsigned long sil0 = szybkosc::przebiegiSilnika, par0 = b.pp.parsowanHistorii, traf0 = b.pp.trafienHistorii;
  unsigned long odcz0 = odczytyNocy(p.pliki), pw0 = sumaMapy(p.pliki.pierwsze), li0 = sumaMapy(p.pliki.listowania);
  bool czekano = false;
  if (b.czekanie) czekano = Dyrygent(b.ad(), konfTestu, &b.pp).wybudzenie();
  else Dyrygent(b.ad(), konfTestu).wybudzenie();
  Wynik r;
  r.msPrzebiegu = p.zegar.msTeraz;
  r.spal = p.sen.snow > snow0;
  r.czekal = p.sen.czekan > czek0 && czekano;
  r.przerwane = p.sen.przerwanych > prz0;
  r.plan = p.sen.plan;
  r.silnik = szybkosc::przebiegiSilnika - sil0;
  r.parsowan = b.pp.parsowanHistorii - par0;
  r.trafien = b.pp.trafienHistorii - traf0;
  r.odczytowNocy = odczytyNocy(p.pliki) - odcz0;
  r.pierwszych = sumaMapy(p.pliki.pierwsze) - pw0;
  r.listowan = sumaMapy(p.pliki.listowania) - li0;
  Skrot k;
  for (size_t i = kl0; i < p.ekran.klatki.size(); i++) {
    k.bajty(p.ekran.klatki[i].data(), p.ekran.klatki[i].size());
    r.czesciowych += p.ekran.rodzaje[i] == Odswiezenie::CZESCIOWE;
  }
  r.klatek = (int)(p.ekran.klatki.size() - kl0);
  r.pierwszaCzesciowa = r.klatek > 0 && p.ekran.rodzaje[kl0] == Odswiezenie::CZESCIOWE;
  r.klatki = k.k;
  Skrot s;
  for (auto& kv : p.nvs.teksty) {
    if (kv.first == "cpap/cz_blok") continue;  // T-WYM-17: blokada po restarcie po błędzie tylko na ścieżce B
    s.tekst(kv.first);
    s.tekst(kv.second);
  }
  for (auto& kv : p.nvs.liczbyU) { s.tekst(kv.first); s.pole(kv.second); }
  for (auto& kv : p.nvs.liczby) { s.tekst(kv.first); s.pole(kv.second); }
  for (auto& kv : p.nvs.flagi) {
    if (kv.first == "cpap/cz_akt") continue;  // T-WYM-17: flaga trybu bez snu tylko na ścieżce B (z definicji)
    s.tekst(kv.first);
    s.pole(kv.second);
  }
  for (auto& kv : p.nvs.bajtyMap) { s.tekst(kv.first); s.tekst(kv.second); }
  for (auto& kv : p.pliki.pliki) { s.tekst(kv.first); s.tekst(kv.second); }
  const StanRtc& rt = p.start.stan;  // bez czesciowychZRzedu: licznik częściowych różni ścieżki z definicji
  s.pole(rt.ekran); s.pole(rt.ekranOd); s.pole(rt.klatkaSkrot); s.pole(rt.klatkaOd); s.pole(rt.obsluzoneDo);
  s.pole(rt.wybudzen); s.pole(rt.planNaZasilaniu); s.pole(rt.slotDziennyEp);
  r.stan = s.k;
  Skrot lg;
  for (size_t i = log0; i < p.port.log.size(); i++) {
    if (wierszCzekania(p.port.log[i])) continue;
    r.wiersze.push_back(bezRodzaju(p.port.log[i]));
    lg.tekst(r.wiersze.back());
  }
  r.log = lg.k;
  if (b.czekanie) {
    if (czekano) b.pl.wlacz(true);  // pamięć plików od drugiego przebiegu programu (src/main.cpp)
    else b.nowyProgram();           // głęboki sen: następny przebieg to nowy program
  }
  // wektory atrap rosnące z każdym przebiegiem (test G mierzy stertę dyrygenta, nie atrap)
  p.d.z.clear();
  p.siec.zadania.clear();
  p.siec.naglowki.clear();
  return r;
}

// Porównanie przebiegu obu ścieżek: jedna niezgodność = opis z różnicą logu.
static int niezgodnosci = 0;
static void porownaj(const char* co, uint32_t t, const Wynik& a, const Wynik& b) {
  bool zgodne = a.klatki == b.klatki && a.klatek == b.klatek && a.stan == b.stan && a.log == b.log &&
                a.plan.sekund == b.plan.sekund && a.plan.przyciski == b.plan.przyciski &&
                a.plan.budzZegarem == b.plan.budzZegarem && a.spal == (b.spal || b.czekal) &&
                b.czesciowych >= a.czesciowych;
  if (zgodne) {
    ok++;
    return;
  }
  zle++;
  if (++niezgodnosci > 5) return;
  printf("BŁĄD %s %s: klatki %d/%d (%s), stan %s, log %s, plan %lu/%lu s, przyciski %u/%u, sen %d/%d czekanie %d\n", co,
         hhmm(t).c_str(), a.klatek, b.klatek, a.klatki == b.klatki ? "=" : "≠", a.stan == b.stan ? "=" : "≠",
         a.log == b.log ? "=" : "≠", (unsigned long)a.plan.sekund, (unsigned long)b.plan.sekund,
         (unsigned)a.plan.przyciski, (unsigned)b.plan.przyciski, (int)a.spal, (int)b.spal, (int)b.czekal);
  size_t n = std::max(a.wiersze.size(), b.wiersze.size());
  for (size_t i = 0, pokazane = 0; i < n && pokazane < 6; i++) {
    std::string x = i < a.wiersze.size() ? a.wiersze[i] : "(brak)", y = i < b.wiersze.size() ? b.wiersze[i] : "(brak)";
    if (x == y) continue;
    printf("  A: %s\n  B: %s\n", x.c_str(), y.c_str());
    pokazane++;
  }
}

// ---------- AirBridge w chwili t: terapia 23:00–06:30, przetwarzanie do 06:50, potem noc z dziś na liście ----------
static std::string dataLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  char b[16];
  strftime(b, sizeof b, "%Y-%m-%d", &l);
  return b;
}
static int minutaLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  return l.tm_hour * 60 + l.tm_min;
}
static bool bezNowychNocy = false;  // test G: AirBridge bez nowych nocy (ostatnia z historii)
static long dobaHistorii = 0;       // ostatnia doba w plikach na starcie
static void airbridge(Bieg& b, uint32_t t) {
  b.p.siec.odpowiedzi.clear();
  int m = minutaLok(t);
  if (!bezNowychNocy && (m >= 23 * 60 || m < 6 * 60 + 30)) {
    b.p.siec.dodaj("/api/status", 200, STATUS_TERAPIA);
    return;
  }
  if (!bezNowychNocy && m < 6 * 60 + 50) {
    b.p.siec.dodaj("/api/status", 200, STATUS_PRZETWARZA);
    return;
  }
  long ostatnia = bezNowychNocy ? dobaHistorii : nrDnia(dataLok(t)) - 1;
  if (sesjaZDoby) {  // test I: prognoza Open-Meteo dziś i jutro (15°, PM2.5 10, bez opadu), jak testy slotu dziennego
    std::string d = dataLok(t), godz = "\"time\":[", te = "\"temperature_2m\":[", op = "\"precipitation_probability\":[",
                ch = "\"cloud_cover\":[", wi = "\"wind_gusts_10m\":[", pt = "\"time\":[", pm = "\"pm2_5\":[";
    for (int i = 0; i < 48; i++) {
      char g[32];
      snprintf(g, sizeof g, "\"%sT%02d:00\"", (i < 24 ? d : dodajDni(d, 1)).c_str(), i % 24);
      const char* sp = i ? "," : "";
      godz += sp + std::string(g);
      pt += sp + std::string(g);
      te += sp + std::string("15");
      op += sp + std::string("0");
      ch += sp + std::string("40");
      wi += sp + std::string("20");
      pm += sp + std::string("10");
    }
    b.p.siec.dodaj("/v1/forecast", 200, "{\"hourly\":{" + godz + "]," + te + "]," + op + "]," + ch + "]," + wi + "]}}");
    b.p.siec.dodaj("/v1/air-quality", 200, "{\"hourly\":{" + pt + "]," + pm + "]}}");
  }
  b.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  b.p.siec.dodaj(sciezkaListy(), 200, lista(ostatnia - 9, ostatnia));
  for (long d = ostatnia - 9; d <= ostatnia; d++) b.p.siec.dodaj(sciezkaNocy(d), 200, raport(d));
}

static std::vector<std::pair<uint32_t, uint8_t>> naciski(uint8_t budzacy, const std::vector<uint8_t>& kolejne,
                                                         uint32_t odstep = 3000) {
  std::vector<std::pair<uint32_t, uint8_t>> s = {{0, budzacy}, {300, 0}};
  uint32_t t = odstep;
  for (uint8_t b : kolejne) {
    s.push_back({t, b});
    s.push_back({t + 200, 0});
    t += odstep;
  }
  return s;
}
static std::string komendaKonfiguracji(const std::string& tresc) {
  return "W konfiguracja.txt " + std::to_string(tresc.size()) + " " + sha256Hex(tresc) + "\n" + tresc;
}

struct Zdarzenie {
  enum Co { PRZYCISK, START, AWARIA, ODLACZ, PODLACZ, ODLACZ_W_CZEKANIU, BEZ_WIFI, Z_WIFI, KONFIG };
  Co co = PRZYCISK;
  uint32_t ep = 0;
  uint8_t budzacy = BIT_PRAWY;
  std::vector<uint8_t> dalsze;
  std::vector<std::pair<int, uint32_t>> kolejka;
  std::string tresc;  // KONFIG: nowa treść /konfiguracja.txt (komenda `W` w czuwaniu po przycisku)
};

struct Statystyka {
  int przebiegow = 0, poCzekaniu = 0, przyciskowPoCzekaniu = 0, czesciowychPierwszych = 0, glebokich = 0;
  unsigned long silnikPoCzekaniu = 0, parsowanPoCzekaniu = 0, odczytowNocyPoCzekaniu = 0;
  unsigned long silnikA = 0, odczytowNocyA = 0;
  // koszt rezygnacji z pamięci listuj() i pierwszyWiersz() (17.11.2026): po czekaniu, z tego po przycisku, i ścieżka A
  unsigned long pierwszychPoCzekaniu = 0, listowanPoCzekaniu = 0, pierwszychPrzycisk = 0, listowanPrzycisk = 0;
  unsigned long pierwszychA = 0, listowanA = 0;
  int klatekA = 0, czesciowychA = 0, czesciowychB = 0, pierwszychPelnychA = 0;
};

// Wołane po każdej parze przebiegów (A, B) przed wyborem następnego wybudzenia; test K (limit ładowania) porównuje
// tu atrapy ładowarki obu ścieżek i symuluje upływ licznika watchdoga układu między przebiegami. nullptr = nic.
static std::function<void(Bieg&, Bieg&, uint32_t)> poParze;

// Symulacja od `od` do `doT`: obie ścieżki dostają te same wybudzenia (terminy z planu ścieżki A, zdarzenia).
static void symuluj(Bieg& A, Bieg& B, uint32_t od, uint32_t doT, std::vector<Zdarzenie> zd, const char* nazwa,
                    Statystyka& st) {
  std::sort(zd.begin(), zd.end(), [](const Zdarzenie& x, const Zdarzenie& y) { return x.ep < y.ep; });
  Wejscie w;
  w.ep = od;
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  size_t iz = 0;
  bool poprzednieCzekanie = false;
  for (int krok = 0; krok < 20000 && w.ep < doT; krok++) {
    bool odlaczWCzekaniu = false;
    while (iz < zd.size() && zd[iz].co == Zdarzenie::ODLACZ_W_CZEKANIU && zd[iz].ep <= w.ep) {
      odlaczWCzekaniu = true;
      iz++;
    }
    if (odlaczWCzekaniu) B.p.sen.wCzekaniu = [&B]() { B.p.czujniki.zas = Zasilanie::BATERIA; };
    airbridge(A, w.ep);
    airbridge(B, w.ep);
    Wynik a = przebieg(A, w);
    Wynik b = przebieg(B, w);
    B.p.sen.wCzekaniu = nullptr;
    if (odlaczWCzekaniu) A.p.czujniki.zas = Zasilanie::BATERIA;  // A spała głęboko, kabel odłączony w czasie snu
    porownaj(nazwa, w.ep, a, b);
    if (poParze) poParze(A, B, w.ep);
    st.przebiegow++;
    st.glebokich += b.spal;
    st.klatekA += a.klatek;
    st.czesciowychA += a.czesciowych;
    st.czesciowychB += b.czesciowych;
    st.silnikA += a.silnik;
    st.odczytowNocyA += a.odczytowNocy;
    st.pierwszychA += a.pierwszych;
    st.listowanA += a.listowan;
    if (poprzednieCzekanie) {
      st.pierwszychPoCzekaniu += b.pierwszych;
      st.listowanPoCzekaniu += b.listowan;
      if (w.pw == PowodWybudzenia::PRZYCISK) {
        st.pierwszychPrzycisk += b.pierwszych;
        st.listowanPrzycisk += b.listowan;
      }
      st.poCzekaniu++;
      st.silnikPoCzekaniu += b.silnik;
      st.parsowanPoCzekaniu += b.parsowan;
      st.odczytowNocyPoCzekaniu += b.odczytowNocy;
      if (w.pw == PowodWybudzenia::PRZYCISK) {
        st.przyciskowPoCzekaniu++;
        st.czesciowychPierwszych += b.pierwszaCzesciowa;
        st.pierwszychPelnychA += a.klatek > 0 && !a.pierwszaCzesciowa;
      }
    }
    poprzednieCzekanie = b.czekal;
    if (logCzekania())
      printf("  %s %-8s klatek %d (B częściowych %d) silnik A %lu B %lu, noce B %lu/%lu, czekanie %d, sen %lu s\n",
             hhmm(w.ep).c_str(), w.pw == PowodWybudzenia::PRZYCISK ? "przycisk" : w.pw == PowodWybudzenia::START ? "start" : "zegar",
             b.klatek, b.czesciowych, a.silnik, b.silnik, b.parsowan, b.odczytowNocy, (int)b.czekal,
             (unsigned long)a.plan.sekund);
    uint32_t koniec = w.ep + a.msPrzebiegu / 1000;
    uint32_t nast = koniec + (a.plan.budzZegarem ? (a.plan.sekund ? a.plan.sekund : 1) : OKRES_SNU_S);
    Wejscie n;
    n.ep = nast;
    while (iz < zd.size() && zd[iz].ep <= nast) {
      const Zdarzenie& z = zd[iz];
      if (z.co == Zdarzenie::ODLACZ_W_CZEKANIU) break;  // dotyczy czekania po przebiegu o tej chwili albo później
      iz++;
      uint32_t kiedy = std::max(z.ep, koniec + 1);
      bool wybudza = false;
      switch (z.co) {
        case Zdarzenie::ODLACZ: A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::BATERIA; break;
        case Zdarzenie::PODLACZ: A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::ZEWNETRZNE; break;
        case Zdarzenie::BEZ_WIFI: A.p.siec.siecJest = B.p.siec.siecJest = false; break;
        case Zdarzenie::Z_WIFI: A.p.siec.siecJest = B.p.siec.siecJest = true; break;
        case Zdarzenie::ODLACZ_W_CZEKANIU: break;
        case Zdarzenie::PRZYCISK:
        case Zdarzenie::KONFIG:
          n.ep = kiedy;
          n.pw = PowodWybudzenia::PRZYCISK;
          n.budzace = z.budzacy;
          n.skrypt = naciski(z.budzacy, z.dalsze);
          n.kolejka = z.kolejka;
          if (z.co == Zdarzenie::KONFIG) n.port = komendaKonfiguracji(z.tresc);
          wybudza = true;
          break;
        case Zdarzenie::START:
        case Zdarzenie::AWARIA:
          n.ep = kiedy;
          n.pw = PowodWybudzenia::START;
          n.prz = z.co == Zdarzenie::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::TASK_WDT;
          A.p.start.stan = StanRtc();  // pamięć RTC_DATA ginie przy każdym starcie innym niż z głębokiego snu
          B.p.start.stan = StanRtc();
          if (!B.czekanie || b.czekal) B.nowyProgram();  // po czekaniu: nowy program (po głębokim śnie już jest)
          wybudza = true;
          break;
      }
      if (wybudza) break;
    }
    w = n;
  }
}

// ---------- A. decyzja rdzenia (T-WYM-10, T-WYM-11, T-WYM-13) ----------
static czekanie::Wejscie wejscieKabel() {
  czekanie::Wejscie w;
  w.obsluga = true;
  w.naStarcie = w.przedSnem = Zasilanie::ZEWNETRZNE;
  w.sekundOdStartu = 3600;
  w.przebiegow = 5;
  w.minutaDoby = 12 * 60;
  return w;
}
static void testDecyzja() {
  using czekanie::Powod;
  auto pow = [](const czekanie::Wejscie& w) { return czekanie::decyzja(w).powod; };
  czekanie::Wejscie w = wejscieKabel();
  SPRAWDZ(czekanie::decyzja(w).wybor == czekanie::Wybor::CZEKANIE);
  SPRAWDZ(pow(w) == Powod::CZEKANIE);
  w.obsluga = false;
  SPRAWDZ(pow(w) == Powod::BEZ_OBSLUGI);
  w = wejscieKabel();
  for (Zasilanie z : {Zasilanie::BATERIA, Zasilanie::NIEZNANE}) {  // tylko pewne „zewnętrzne” (fail-path R-NF-45)
    w = wejscieKabel();
    w.naStarcie = z;
    SPRAWDZ(pow(w) == Powod::NIE_KABEL);
    w = wejscieKabel();
    w.przedSnem = z;
    SPRAWDZ(pow(w) == Powod::NIE_KABEL);
  }
  w = wejscieKabel();
  w.awaryjny = true;
  SPRAWDZ(pow(w) == Powod::AWARYJNY);
  w = wejscieKabel();
  w.budzZegarem = false;
  SPRAWDZ(pow(w) == Powod::BEZ_BUDZIKA);
  w = wejscieKabel();
  w.pamiecWBudzecie = false;
  SPRAWDZ(pow(w) == Powod::PAMIEC);
  w = wejscieKabel();
  w.najwiekszyBlokPsram = czekanie::PSRAM_BLOK_MIN_B - 1;
  SPRAWDZ(pow(w) == Powod::PSRAM);
  w.najwiekszyBlokPsram = czekanie::PSRAM_BLOK_MIN_B;
  SPRAWDZ(pow(w) == Powod::CZEKANIE);
  w.najwiekszyBlokPsram = czekanie::BLOK_NIEZNANY;  // nieznany: bez sprawdzenia
  SPRAWDZ(pow(w) == Powod::CZEKANIE);
  w.najwiekszyBlokPsram = 0;  // na ESP32 0 = brak wolnego bloku, nie brak pomiaru
  SPRAWDZ(pow(w) == Powod::PSRAM);
  w.najwiekszyBlokPsram = czekanie::BLOK_NIEZNANY;
  // planowy restart: po PLANOWY_RESTART_PO_S tylko w oknie 02:00–04:00, twardo po 48 h albo 1000 przebiegach
  w = wejscieKabel();
  w.sekundOdStartu = czekanie::PLANOWY_RESTART_PO_S;
  SPRAWDZ(pow(w) == Powod::CZEKANIE);  // 12:00, poza oknem
  w.minutaDoby = czekanie::OKNO_RESTARTU_OD_MIN;
  SPRAWDZ(pow(w) == Powod::PLANOWY_RESTART);
  w.minutaDoby = czekanie::OKNO_RESTARTU_DO_MIN - 1;
  SPRAWDZ(pow(w) == Powod::PLANOWY_RESTART);
  w.minutaDoby = czekanie::OKNO_RESTARTU_DO_MIN;
  SPRAWDZ(pow(w) == Powod::CZEKANIE);
  w.minutaDoby = -1;  // zegar nieznany
  SPRAWDZ(pow(w) == Powod::CZEKANIE);
  w.sekundOdStartu = czekanie::PLANOWY_RESTART_PO_S - 1;
  w.minutaDoby = 3 * 60;
  SPRAWDZ(pow(w) == Powod::CZEKANIE);
  w = wejscieKabel();
  w.sekundOdStartu = czekanie::RESTART_NAJPOZNIEJ_PO_S;
  SPRAWDZ(pow(w) == Powod::PLANOWY_RESTART);
  w = wejscieKabel();
  w.przebiegow = czekanie::PRZEBIEGOW_NAJWIECEJ;
  SPRAWDZ(pow(w) == Powod::PLANOWY_RESTART);
  w.przebiegow = czekanie::PRZEBIEGOW_NAJWIECEJ - 1;
  SPRAWDZ(pow(w) == Powod::CZEKANIE);
  SPRAWDZ(czekanie::przerwacCzekanie(Zasilanie::BATERIA));
  SPRAWDZ(czekanie::przerwacCzekanie(Zasilanie::NIEZNANE));
  SPRAWDZ(!czekanie::przerwacCzekanie(Zasilanie::ZEWNETRZNE));
  for (int i = 0; i <= (int)Powod::PLANOWY_RESTART; i++) SPRAWDZ(std::string(czekanie::opis((Powod)i)) != "?");
}

// ---------- B. warstwa pamięci plików (T-WYM-14) ----------
static void testPlikiZPamiecia() {
  Dziennik d;
  AtrapaPlikow f(d);
  f.montowane = true;
  f.pliki["/dane/noce/2026.csv"] = "a\n";
  f.pliki["/konfiguracja.txt"] = "x=1\n";
  PlikiZPamiecia p(f);
  std::string t;
  // wyłączona: zawsze z nośnika
  SPRAWDZ(p.czytaj("/konfiguracja.txt", t) && t == "x=1\n");
  SPRAWDZ(p.czytaj("/konfiguracja.txt", t));
  ROWNE_L(f.odczyty["/konfiguracja.txt"], 2);
  ROWNE_L(p.bajtow(), 0);
  p.wlacz(true);
  SPRAWDZ(p.czytaj("/konfiguracja.txt", t) && t == "x=1\n");
  SPRAWDZ(p.czytaj("/konfiguracja.txt", t) && t == "x=1\n");
  ROWNE_L(f.odczyty["/konfiguracja.txt"], 3);  // drugi odczyt z pamięci
  ROWNE_L(p.trafien, 1);
  // zapis przez warstwę unieważnia
  SPRAWDZ(p.zapisz("/konfiguracja.txt", "x=2\n") == 4);
  SPRAWDZ(p.czytaj("/konfiguracja.txt", t) && t == "x=2\n");
  SPRAWDZ(p.dopiszWiersz("/konfiguracja.txt", "y=3"));
  SPRAWDZ(p.czytaj("/konfiguracja.txt", t) && t == f.pliki["/konfiguracja.txt"]);
  SPRAWDZ(p.zapiszAtomowo("/konfiguracja.txt", "z\n"));
  SPRAWDZ(p.czytaj("/konfiguracja.txt", t) && t == "z\n");
  // zmiana nazwy: oba końce
  SPRAWDZ(p.czytaj("/dane/noce/2026.csv", t) && t == "a\n");
  SPRAWDZ(p.zmienNazwe("/konfiguracja.txt", "/dane/noce/2026.csv"));
  SPRAWDZ(p.czytaj("/dane/noce/2026.csv", t) && t == "z\n");
  SPRAWDZ(!p.istnieje("/konfiguracja.txt"));
  SPRAWDZ(!p.czytaj("/konfiguracja.txt", t));
  // istnieje i listuj: nowy plik w katalogu widoczny od razu
  std::vector<std::string> n;
  SPRAWDZ(p.listuj("/dane/noce", n) && n.size() == 1);
  SPRAWDZ(!p.istnieje("/dane/noce/2025.csv"));
  SPRAWDZ(p.zapisz("/dane/noce/2025.csv", "b\n") == 2);
  SPRAWDZ(p.istnieje("/dane/noce/2025.csv"));
  SPRAWDZ(p.listuj("/dane/noce", n) && n.size() == 2);
  SPRAWDZ(p.usun("/dane/noce/2025.csv"));
  SPRAWDZ(!p.istnieje("/dane/noce/2025.csv") && p.listuj("/dane/noce", n) && n.size() == 1);
  // czytajWiersze (przywrócenie `.tmp` w adapterze) unieważnia ścieżkę
  SPRAWDZ(p.czytaj("/dane/noce/2026.csv", t));
  f.pliki["/dane/noce/2026.csv"] = "zmienione poza warstwą\n";  // tylko do sprawdzenia unieważnienia
  std::vector<std::string> wi;
  p.czytajWiersze("/dane/noce/2026.csv", wi);
  SPRAWDZ(p.czytaj("/dane/noce/2026.csv", t) && t == "zmienione poza warstwą\n");
  // odmontowanie, montowanie po odmontowaniu i formatowanie: od nowa
  SPRAWDZ(p.bajtow() > 0);
  p.odmontuj();
  ROWNE_L(p.bajtow(), 0);
  SPRAWDZ(p.zamontuj());
  SPRAWDZ(p.czytaj("/dane/noce/2026.csv", t));
  SPRAWDZ(p.bajtow() > 0);
  SPRAWDZ(p.formatuj());
  ROWNE_L(p.bajtow(), 0);
  // limit: plik ponad limit czytany zawsze z nośnika
  PlikiZPamiecia mala(f, 4);
  mala.wlacz(true);
  f.pliki["/duzy"] = "0123456789";
  int przed = f.odczyty["/duzy"];
  SPRAWDZ(mala.czytaj("/duzy", t) && mala.czytaj("/duzy", t) && t == "0123456789");
  ROWNE_L(f.odczyty["/duzy"] - przed, 2);
  ROWNE_L(mala.bajtow(), 0);
  // pierwszy wiersz: od 17.11.2026 bez pamięci, zawsze z nośnika; zapis widoczny od razu
  f.pliki["/dane/noce/2026.csv"] = "#naglowek 5\nwiersz\n";
  p.wlacz(true);
  std::string pw;
  int pw0 = f.odczyty["/dane/noce/2026.csv"];
  SPRAWDZ(p.pierwszyWiersz("/dane/noce/2026.csv", pw) && p.pierwszyWiersz("/dane/noce/2026.csv", pw));
  ROWNE_L(f.odczyty["/dane/noce/2026.csv"] - pw0, 2);
  SPRAWDZ(p.zapisz("/dane/noce/2026.csv", "#naglowek 6\n") > 0);
  SPRAWDZ(p.pierwszyWiersz("/dane/noce/2026.csv", pw) && pw.find("6") != std::string::npos);
  // Zmiana 2026-11-17: listuj() bez pamięci (z ukośnikiem i bez, nowy plik roku
  // widoczny od razu), klucz bez ukośnika na końcu, usunięcie i zmiana nazwy katalogu unieważniają wszystko pod nim
  {
    Dziennik d2;
    AtrapaPlikow g(d2);
    g.montowane = true;
    g.pliki["/dane/noce/2026.csv"] = "#n\na\n";
    g.pliki["/dane/sypialnia/2026.csv"] = "#s\n";
    PlikiZPamiecia q(g);
    q.wlacz(true);
    std::vector<std::string> l1, l2;
    SPRAWDZ(q.listuj("/dane/noce/", l1) && l1.size() == 1);
    g.pliki["/dane/noce/2027.csv"] = "#n\n";  // nawet z pominięciem warstwy: listuj() zawsze z nośnika
    SPRAWDZ(q.listuj("/dane/noce/", l2) && l2.size() == 2);
    SPRAWDZ(q.dopiszWiersz("/dane/sypialnia/2027.csv", "b") && q.listuj("/dane/sypialnia", l2) && l2.size() == 2);
    SPRAWDZ(q.czytaj("/dane/noce/2026.csv", t) && q.istnieje("/dane/noce/2026.csv") && q.istnieje("/dane/noce/2026.csv/"));
    unsigned long n0 = q.odczytowNosnika;
    SPRAWDZ(q.czytaj("/dane/noce/2026.csv/", t) && t == "#n\na\n" && q.odczytowNosnika == n0);  // ten sam klucz
    // katalog usunięty przez warstwę: treść i istnienie wszystkiego pod nim unieważnione (atrapa usuwa tylko wpis
    // katalogu, więc pliki znikają z nośnika ręcznie, jak po rekurencyjnym usunięciu)
    q.usun("/dane/noce");
    g.pliki.erase("/dane/noce/2026.csv");
    g.pliki.erase("/dane/noce/2027.csv");
    SPRAWDZ(!q.istnieje("/dane/noce/2026.csv") && !q.czytaj("/dane/noce/2026.csv", t));
    // zmiana nazwy katalogu (z ukośnikiem na końcu): oba końce unieważnione
    SPRAWDZ(q.czytaj("/dane/sypialnia/2026.csv", t) && t == "#s\n");
    q.zmienNazwe("/dane/sypialnia/", "/dane/stara/");
    g.pliki["/dane/sypialnia/2026.csv"] = "#nowy\n";  // nowa zawartość pod starą nazwą
    SPRAWDZ(q.czytaj("/dane/sypialnia/2026.csv", t) && t == "#nowy\n");
    q.zmienNazwe("/dane", "/dane2");
    g.pliki.erase("/dane/sypialnia/2026.csv");
    SPRAWDZ(!q.istnieje("/dane/sypialnia/2026.csv"));
  }
  // wyłączenie czyści
  SPRAWDZ(p.czytaj("/dane/noce/2026.csv", t) && p.bajtow() > 0);
  p.wlacz(false);
  ROWNE_L(p.bajtow(), 0);
}

// ---------- C. równoważność: doba (366 nocy) i tydzień (60 nocy) na jednym żywym stanie ----------
static Zdarzenie przycisk(uint32_t t, uint8_t budzacy, std::vector<uint8_t> dalsze = {},
                          std::vector<std::pair<int, uint32_t>> kolejka = {}) {
  Zdarzenie z;
  z.co = Zdarzenie::PRZYCISK;
  z.ep = t;
  z.budzacy = budzacy;
  z.dalsze = std::move(dalsze);
  z.kolejka = std::move(kolejka);
  return z;
}
static Zdarzenie inne(Zdarzenie::Co co, uint32_t t) {
  Zdarzenie z;
  z.co = co;
  z.ep = t;
  return z;
}
// Naciśnięcia jednej doby (dzień `d` listopada): rano kokpit tam i z powrotem, kolejka 3 naciśnięć, ↻ w południe,
// wieczorem ▶, w nocy ▶ na S9 (R-KON-37) i nad ranem.
static void naciskiDoby(std::vector<Zdarzenie>& z, int d) {
  const uint8_t P = BIT_PRAWY, L = BIT_LEWY, S = BIT_SRODEK;
  z.push_back(przycisk(ep(2026, 11, d, 7, 40), P, {P, P, L}));
  z.push_back(przycisk(ep(2026, 11, d, 9, 12), P, {},
                       {{(int)Przycisk::PRAWY, 120}, {(int)Przycisk::PRAWY, 120}, {(int)Przycisk::PRAWY, 120}}));
  z.push_back(przycisk(ep(2026, 11, d, 12, 5), S));
  z.push_back(przycisk(ep(2026, 11, d, 20, 30), P, {P}));
  z.push_back(przycisk(ep(2026, 11, d, 23, 50), P));
  z.push_back(przycisk(ep(2026, 11, d + 1, 3, 10), P));
}
static void wypisz(const char* nazwa, const Statystyka& s) {
  printf("  %s: %d przebiegów, %d po czekaniu (%d przyciskiem), %d głębokich snów; e-ink A %d (częściowych A %d, B %d);"
         " po czekaniu: ocen silnika %lu, rozkładów nocy %lu, odczytów plików nocy z nośnika %lu; ścieżka A: ocen %lu,"
         " odczytów plików nocy %lu; pierwsze rysowanie po przycisku po czekaniu częściowe %d z %d (A pełne %d)\n",
         nazwa, s.przebiegow, s.poCzekaniu, s.przyciskowPoCzekaniu, s.glebokich, s.klatekA, s.czesciowychA,
         s.czesciowychB, s.silnikPoCzekaniu, s.parsowanPoCzekaniu, s.odczytowNocyPoCzekaniu, s.silnikA,
         s.odczytowNocyA, s.czesciowychPierwszych, s.przyciskowPoCzekaniu, s.pierwszychPelnychA);
  printf("  %s, bez pamięci list i pierwszych wierszy (17.11.2026): po czekaniu listuj() %lu, pierwszyWiersz() %lu "
         "(na przebieg %.2f i %.2f); z tego po przycisku listuj() %lu, pierwszyWiersz() %lu (na naciśnięcie %.2f i "
         "%.2f); ścieżka A listuj() %lu, pierwszyWiersz() %lu\n",
         nazwa, s.listowanPoCzekaniu, s.pierwszychPoCzekaniu,
         s.poCzekaniu ? (double)s.listowanPoCzekaniu / s.poCzekaniu : 0.0,
         s.poCzekaniu ? (double)s.pierwszychPoCzekaniu / s.poCzekaniu : 0.0, s.listowanPrzycisk, s.pierwszychPrzycisk,
         s.przyciskowPoCzekaniu ? (double)s.listowanPrzycisk / s.przyciskowPoCzekaniu : 0.0,
         s.przyciskowPoCzekaniu ? (double)s.pierwszychPrzycisk / s.przyciskowPoCzekaniu : 0.0, s.listowanA,
         s.pierwszychA);
}
static void testDoba366() {
  int bylo = zle;
  dobaHistorii = nrDnia("2026-11-09") - 1;
  Bieg A(false, "2026-11-09", 366), B(true, "2026-11-09", 366);
  std::vector<Zdarzenie> z;
  naciskiDoby(z, 10);
  Statystyka st;
  symuluj(A, B, ep(2026, 11, 10, 4, 50), ep(2026, 11, 11, 4, 50), z, "doba 366", st);
  wypisz("doba (366 nocy)", st);
  SPRAWDZ(st.poCzekaniu > 20);
  SPRAWDZ(st.przyciskowPoCzekaniu >= 5);
  ROWNE_L(st.glebokich, 0);
  SPRAWDZ(st.czesciowychB > st.czesciowychA);  // pierwsze rysowania po przycisku częściowe tylko po czekaniu
  SPRAWDZ(st.czesciowychPierwszych >= 1 && st.czesciowychPierwszych <= st.pierwszychPelnychA);
  SPRAWDZ(st.parsowanPoCzekaniu <= 3);  // nowa noc rano: rozkład od nowa po zmianie plików, potem z pamięci
  SPRAWDZ(st.silnikPoCzekaniu < st.silnikA);
  if (zle != bylo) printf("  (doba 366: %d niezgodności)\n", zle - bylo);
}
static void testTydzien() {
  int bylo = zle;
  dobaHistorii = nrDnia("2026-11-08") - 1;
  Bieg A(false, "2026-11-08", 60), B(true, "2026-11-08", 60);
  std::vector<Zdarzenie> z;
  for (int d = 9; d <= 15; d++) naciskiDoby(z, d);
  Zdarzenie k = przycisk(ep(2026, 11, 10, 13, 0), BIT_PRAWY);  // komenda serwisowa `W konfiguracja.txt` w czuwaniu
  k.co = Zdarzenie::KONFIG;
  k.tresc = "noc_zrodlo=json\nwymiana.filtr=2026-10-07\nokres.filtr_tyg=4\nkokpit.kolejnosc=k2,k1\n";
  z.push_back(k);
  z.push_back(inne(Zdarzenie::BEZ_WIFI, ep(2026, 11, 11, 8, 0)));  // błąd Wi-Fi
  z.push_back(inne(Zdarzenie::Z_WIFI, ep(2026, 11, 11, 14, 0)));
  z.push_back(inne(Zdarzenie::ODLACZ, ep(2026, 11, 12, 10, 0)));   // kabel odłączony między przebiegami
  z.push_back(inne(Zdarzenie::PODLACZ, ep(2026, 11, 12, 15, 0)));
  z.push_back(inne(Zdarzenie::ODLACZ_W_CZEKANIU, ep(2026, 11, 13, 18, 0)));  // kabel odłączony w czasie czekania
  z.push_back(inne(Zdarzenie::PODLACZ, ep(2026, 11, 13, 18, 40)));
  z.push_back(inne(Zdarzenie::START, ep(2026, 11, 14, 13, 0)));   // zanik zasilania i start
  z.push_back(inne(Zdarzenie::AWARIA, ep(2026, 11, 15, 16, 0)));  // restart po błędzie programu (strażnik)
  Statystyka st;
  symuluj(A, B, ep(2026, 11, 9, 4, 50), ep(2026, 11, 16, 4, 50), z, "tydzień", st);
  wypisz("tydzień (60 nocy)", st);
  SPRAWDZ(st.glebokich >= 2);  // odłączenia kabla
  SPRAWDZ(B.p.sen.przerwanych == 1);
  SPRAWDZ(B.programow >= 4);   // start, głębokie sny po odłączeniu, start zasilania, awaria
  SPRAWDZ(st.czesciowychPierwszych > 0);
  if (zle != bylo) printf("  (tydzień: %d niezgodności)\n", zle - bylo);
}

// ---------- D. liczniki na 366 nocach: przycisk po czekaniu (T-WYM-14, R-NF-45) ----------
static void testLiczniki366() {
  bezNowychNocy = true;
  dobaHistorii = nrDnia("2026-11-04") - 1;
  Bieg A(false, "2026-11-04", 366), B(true, "2026-11-04", 366);
  auto oba = [&](const Wejscie& w, Wynik& a, Wynik& b) {
    airbridge(A, w.ep);
    airbridge(B, w.ep);
    auto t0 = std::chrono::steady_clock::now();
    a = przebieg(A, w);
    auto t1 = std::chrono::steady_clock::now();
    b = przebieg(B, w);
    auto t2 = std::chrono::steady_clock::now();
    porownaj("liczniki", w.ep, a, b);
    return std::make_pair(std::chrono::duration<double, std::milli>(t1 - t0).count(),
                          std::chrono::duration<double, std::milli>(t2 - t1).count());
  };
  Wynik a, b;
  Wejscie w;
  w.ep = ep(2026, 11, 4, 11, 58);
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  oba(w, a, b);
  SPRAWDZ(b.czekal);
  w = Wejscie();
  w.ep = ep(2026, 11, 4, 12, 0) + 5;
  oba(w, a, b);  // zegar w czekaniu
  SPRAWDZ(b.czekal);
  // pierwsze naciśnięcie po zmianie pamięci reguł przez poprzednią ocenę (aktualizujPamiec) może liczyć
  // silnik jeszcze raz: tak jak w jednym wybudzeniu; mierzone jest następne
  w = Wejscie();
  w.ep = ep(2026, 11, 4, 12, 5);
  w.pw = PowodWybudzenia::PRZYCISK;
  w.budzace = BIT_PRAWY;
  w.skrypt = naciski(BIT_PRAWY, {BIT_LEWY});
  oba(w, a, b);
  SPRAWDZ(b.czekal && b.silnik <= 1);
  w = Wejscie();
  w.ep = ep(2026, 11, 4, 12, 10);
  w.pw = PowodWybudzenia::PRZYCISK;
  w.budzace = BIT_PRAWY;
  w.skrypt = naciski(BIT_PRAWY, {BIT_PRAWY, BIT_PRAWY});
  auto ms = oba(w, a, b);
  SPRAWDZ(b.czekal);
  ROWNE_L(b.silnik, 0);        // cel: 0 ocen silnika przy braku zmian danych
  ROWNE_L(b.parsowan, 0);      // 0 rozkładów nocy
  ROWNE_L(b.odczytowNocy, 0);  // 0 odczytów treści plików nocy z nośnika
  // Zmiana 2026-11-17: listuj() i pierwszyWiersz() już nie z pamięci, więc idą na
  // nośnik po czekaniu tak samo jak po głębokim śnie (świadoma zmiana oczekiwania: wcześniej 0). Nie więcej niż A.
  SPRAWDZ(b.listowan <= a.listowan);
  SPRAWDZ(b.pierwszych <= a.pierwszych);
  SPRAWDZ(a.silnik >= 1);      // ścieżka A: ocena od zera po głębokim śnie
  SPRAWDZ(a.odczytowNocy >= 1);
  SPRAWDZ(b.pierwszaCzesciowa);   // pierwsze ▶ po czekaniu częściowe (R-NF-25)
  SPRAWDZ(!a.pierwszaCzesciowa);  // po głębokim śnie pełne jak dotąd
  ROWNE_L(a.klatek, 3);
  ROWNE_L(b.klatek, 3);
  printf("  liczniki (366 nocy, przycisk ▶ ▶ ▶ po czekaniu): ocen silnika A %lu → B %lu, odczytów plików nocy A %lu → B"
         " %lu, rozkładów nocy B %lu, odświeżeń częściowych A %d → B %d; czas na PC A %.0f ms, B %.0f ms\n",
         a.silnik, b.silnik, a.odczytowNocy, b.odczytowNocy, b.parsowan, a.czesciowych, b.czesciowych, ms.first,
         ms.second);
  printf("  liczniki: operacje na nośniku bez pamięci (listuj() A %lu → B %lu, pierwszyWiersz() A %lu → B %lu)\n",
         a.listowan, b.listowan, a.pierwszych, b.pierwszych);
  // ta sama noc zmieniona przez firmware (dopisany czynnik po pytaniu): pamięć się unieważnia, wynik jak A
  bezNowychNocy = false;
}

// ---------- E. odłączenie kabla, stan nieznany, pamięć poza budżetem: głęboki sen (T-WYM-11, T-WYM-12) ----------
static bool wLogu(const Bieg& b, const std::string& fragment, size_t od = 0) {
  for (size_t i = od; i < b.p.port.log.size(); i++)
    if (b.p.port.log[i].find(fragment) != std::string::npos) return true;
  return false;
}
static void testOdlaczenie() {
  bezNowychNocy = true;
  dobaHistorii = nrDnia("2026-11-04") - 1;
  Bieg B(true, "2026-11-04", 30);
  Wejscie w;
  w.ep = ep(2026, 11, 4, 12, 0);
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  airbridge(B, w.ep);
  Wynik r = przebieg(B, w);
  SPRAWDZ(r.czekal && !r.spal);
  SPRAWDZ(wLogu(B, "sen: czekanie na zasilaniu zewnętrznym zamiast głębokiego snu, przebieg 1 (T-WYM-10)"));
  ROWNE_L(B.p.sen.licznikPrzySnie, 0);  // licznik startów od zera przed czekaniem jak przed snem (T-WYM-82)
  SPRAWDZ(B.p.straznik.przekroczen == 0);
  auto krok = [&](uint32_t t) {
    Wejscie x;
    x.ep = t;
    airbridge(B, t);
    return przebieg(B, x);
  };
  // 1. bateria na początku przebiegu: głęboki sen, bez wiersza czekania (log jak przed ADR-0047)
  B.p.czujniki.zas = Zasilanie::BATERIA;
  size_t l0 = B.p.port.log.size();
  int odcz = B.p.czujniki.odczytowZasilania;
  r = krok(ep(2026, 11, 4, 12, 30) + 5);
  SPRAWDZ(r.spal && !r.czekal);
  SPRAWDZ(!wLogu(B, "czekanie", l0) && !wLogu(B, "sen: głęboki mimo", l0));
  ROWNE_L(B.p.czujniki.odczytowZasilania - odcz, 1);  // na baterii bez dodatkowego odczytu ładowarki
  // 2. stan nieznany: jak bateria
  B.p.czujniki.zas = Zasilanie::NIEZNANE;
  l0 = B.p.port.log.size();
  r = krok(ep(2026, 11, 4, 13, 0) + 5);
  SPRAWDZ(r.spal && !r.czekal && !wLogu(B, "czekanie", l0));
  // 3. kabel podłączony: znowu czekanie (nowy program po głębokim śnie)
  B.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
  r = krok(ep(2026, 11, 4, 13, 30) + 5);
  SPRAWDZ(r.czekal && !r.spal);
  ROWNE_L(B.pp.przebiegow, 1);
  // 4. odłączony w trakcie przebiegu (świeży odczyt przed snem): głęboki sen z wierszem
  B.p.czujniki.bateriaOdOdczytu = B.p.czujniki.odczytowZasilania + 2;
  l0 = B.p.port.log.size();
  r = krok(ep(2026, 11, 4, 14, 0) + 5);
  SPRAWDZ(r.spal && !r.czekal);
  SPRAWDZ(wLogu(B, "sen: głęboki mimo zasilania zewnętrznego: kabel odłączony albo stan zasilania nieznany", l0));
  // 5. odłączony w czasie czekania: głęboki sen na resztę planu
  B.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
  B.p.sen.wCzekaniu = [&B]() { B.p.czujniki.zas = Zasilanie::BATERIA; };
  l0 = B.p.port.log.size();
  int snow = B.p.sen.snow;
  r = krok(ep(2026, 11, 4, 14, 30) + 5);
  B.p.sen.wCzekaniu = nullptr;
  SPRAWDZ(r.przerwane && r.spal && !r.czekal);
  ROWNE_L(B.p.sen.snow - snow, 1);
  SPRAWDZ(wLogu(B, "czekanie: zasilanie inne niż zewnętrzne, głęboki sen na pozostałe", l0));
  // 6. pamięć poza budżetem, mały blok PSRAM, planowy restart: głęboki sen z powodem
  B.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
  krok(ep(2026, 11, 4, 15, 0) + 5);
  B.p.diag.sterta = PROG_STERTY_B - 1;
  l0 = B.p.port.log.size();
  r = krok(ep(2026, 11, 4, 15, 30) + 5);
  SPRAWDZ(r.spal && wLogu(B, "sen: głęboki mimo zasilania zewnętrznego: pamięć poza budżetem", l0));
  B.p.diag.sterta = 70000;
  krok(ep(2026, 11, 4, 16, 0) + 5);
  B.p.diag.psram = 512u * 1024u;
  l0 = B.p.port.log.size();
  r = krok(ep(2026, 11, 4, 16, 30) + 5);
  SPRAWDZ(r.spal && wLogu(B, "mały wolny blok PSRAM", l0));
  B.p.diag.psram = czekanie::BLOK_NIEZNANY;
  krok(ep(2026, 11, 4, 17, 0) + 5);
  B.p.diag.sekundy = czekanie::RESTART_NAJPOZNIEJ_PO_S;
  l0 = B.p.port.log.size();
  r = krok(ep(2026, 11, 4, 17, 30) + 5);
  SPRAWDZ(r.spal && wLogu(B, "planowy restart", l0));
  B.p.diag.sekundy = 0;
  // 7. bez pamięci przebiegów (wołający bez pętli przebiegów): nigdy nie czeka, adapter nie jest pytany
  int czek = B.p.sen.czekan;
  B.p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  B.p.zegar.epoch = ep(2026, 11, 4, 18, 0) + 5;
  B.p.zegar.msTeraz = 0;
  SPRAWDZ(!Dyrygent(B.ad(), KONF).wybudzenie());
  ROWNE_L(B.p.sen.czekan, czek);
  bezNowychNocy = false;
}

// ---------- F. awaria: restart po błędzie wznawia jak po głębokim śnie; tryb awaryjny zawsze głęboki ----------
static void testAwaria() {
  bezNowychNocy = true;
  dobaHistorii = nrDnia("2026-11-04") - 1;
  Bieg B(true, "2026-11-04", 30);
  Wejscie w;
  w.ep = ep(2026, 11, 4, 12, 0);
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  airbridge(B, w.ep);
  SPRAWDZ(przebieg(B, w).czekal);
  // strażnik zadania zapisany w przebiegu, odnowiony tuż przed czekaniem (adapter odnawia go w czasie czekania)
  SPRAWDZ(B.p.straznik.zapisany && B.p.straznik.przekroczen == 0);
  SPRAWDZ(B.p.nvs.flagi["cpap/cz_akt"]);  // T-WYM-17: flaga trybu bez snu przed czekaniem
  // restart po błędzie w czasie czekania albo następnego przebiegu: start od zera, licznik startów 1, zwykły przebieg,
  // a czekanie zablokowane do końca doby (T-WYM-17): głęboki sen z wierszem w logu
  B.nowyProgram();
  B.p.start.stan = StanRtc();
  w.ep = ep(2026, 11, 4, 12, 20);
  w.prz = PrzyczynaStartu::TASK_WDT;
  size_t l0 = B.p.port.log.size();
  airbridge(B, w.ep);
  Wynik r = przebieg(B, w);
  SPRAWDZ(r.spal && !r.czekal);
  SPRAWDZ(wLogu(B, "start: przyczyna strażnik zadania, starty po błędzie 1", l0));
  SPRAWDZ(wLogu(B, "czekanie na kablu: restart po błędzie (strażnik zadania) w trybie bez snu, do końca doby "
                   "2026-11-04 głęboki sen (T-WYM-17)", l0));
  SPRAWDZ(wLogu(B, "sen: głęboki mimo zasilania zewnętrznego: restart po błędzie, do końca doby głęboki sen", l0));
  ROWNE_L(B.p.nvs.teksty["cpap/cz_blok"] == "2026-11-04", 1);
  SPRAWDZ(!B.p.nvs.flagi["cpap/cz_akt"]);
  // ta sama doba: dalej głęboki sen; zwykły start zasilania nie zdejmuje blokady
  Wejscie x;
  x.ep = ep(2026, 11, 4, 23, 0);
  airbridge(B, x.ep);
  SPRAWDZ(przebieg(B, x).spal);
  // następna doba: koniec blokady z wierszem, znowu czekanie
  x.ep = ep(2026, 11, 5, 9, 0) + 5;
  l0 = B.p.port.log.size();
  airbridge(B, x.ep);
  r = przebieg(B, x);
  SPRAWDZ(r.czekal);
  SPRAWDZ(wLogu(B, "czekanie na kablu: koniec blokady po restarcie po błędzie z 2026-11-04, nowa doba", l0));
  SPRAWDZ(B.p.nvs.teksty.count("cpap/cz_blok") == 0);
  // start po błędzie bez znanej daty: `?`, ustalona przy pierwszej decyzji ze znanym zegarem
  SPRAWDZ(czekanie::blokada("?", "") == czekanie::Blokada::TRWA);
  SPRAWDZ(czekanie::blokada("?", "2026-11-05") == czekanie::Blokada::USTAL_DZIS);
  SPRAWDZ(czekanie::blokada("2026-11-05", "2026-11-05") == czekanie::Blokada::TRWA);
  SPRAWDZ(czekanie::blokada("2026-11-04", "2026-11-05") == czekanie::Blokada::KONIEC);
  SPRAWDZ(czekanie::blokada("2026-11-05", "") == czekanie::Blokada::TRWA);
  SPRAWDZ(czekanie::blokada("", "2026-11-05") == czekanie::Blokada::BRAK);
  SPRAWDZ(czekanie::startPoBledzie(PrzyczynaStartu::PANIKA) && czekanie::startPoBledzie(PrzyczynaStartu::INT_WDT) &&
          czekanie::startPoBledzie(PrzyczynaStartu::WDT) && !czekanie::startPoBledzie(PrzyczynaStartu::ZASILANIE) &&
          !czekanie::startPoBledzie(PrzyczynaStartu::SPADEK) && !czekanie::startPoBledzie(PrzyczynaStartu::USPIENIE));
  // start zasilania (nie błąd) po czekaniu: flaga zdjęta bez blokady
  SPRAWDZ(B.p.nvs.flagi["cpap/cz_akt"]);
  B.nowyProgram();
  B.p.start.stan = StanRtc();
  x.ep = ep(2026, 11, 5, 9, 20);
  x.pw = PowodWybudzenia::START;
  x.prz = PrzyczynaStartu::ZASILANIE;
  airbridge(B, x.ep);
  SPRAWDZ(przebieg(B, x).czekal);
  SPRAWDZ(B.p.nvs.teksty.count("cpap/cz_blok") == 0);
  // trzy starty po błędzie bez snu: tryb awaryjny, sen tylko głęboki (budzi przycisk), także na kablu
  B.nowyProgram();
  ZnacznikAwaryjny bez;
  for (int i = 0; i < STARTOW_DO_AWARYJNEGO - 1; i++)
    decyzjaStartu(B.p.start.l, PrzyczynaStartu::TASK_WDT, identyfikatorWersji(B.p.start.napisWersji), bez,
                  StanWersji::DOBRA);
  w.ep = ep(2026, 11, 5, 9, 40);
  l0 = B.p.port.log.size();
  int czek = B.p.sen.czekan;
  airbridge(B, w.ep);
  r = przebieg(B, w);
  SPRAWDZ(wLogu(B, "tryb awaryjny (R-AW-39)", l0));
  SPRAWDZ(r.spal && !r.czekal);
  ROWNE_L(B.p.sen.czekan, czek);
  SPRAWDZ(!B.p.sen.plan.budzZegarem || B.p.sen.plan.przyciski == 0);
  bezNowychNocy = false;
}

// ---------- H. wyłącznik `kabel_bez_snu=nie` (T-WYM-16): dokładnie obecna ścieżka ----------
static void testWylacznik() {
  ROWNE_L(parsujUstawieniaDnia("").kabelBezSnu, -1);
  SPRAWDZ(parsujUstawieniaDnia("").kabelBezSnuWlaczone());  // domyślnie włączone (decyzja właściciela projektu 16.11.2026)
  ROWNE_L(parsujUstawieniaDnia("kabel_bez_snu=tak\n").kabelBezSnu, 1);
  ROWNE_L(parsujUstawieniaDnia("kabel_bez_snu = nie \r\n").kabelBezSnu, 0);
  SPRAWDZ(!parsujUstawieniaDnia("kabel_bez_snu=nie\n").kabelBezSnuWlaczone());
  for (const char* zly : {"kabel_bez_snu=TAK\n", "kabel_bez_snu=1\n", "kabel_bez_snu=\n", "kabel_bez_snu=nie # x\n"}) {
    UstawieniaDnia u = parsujUstawieniaDnia(zly);
    SPRAWDZ(u.kabelBezSnu == -1 && u.kabelBezSnuWlaczone() && u.bledy.size() == 1);
  }
  // zły wiersz nie kasuje wcześniejszej dobrej wartości, ostatni dobry wygrywa
  ROWNE_L(parsujUstawieniaDnia("kabel_bez_snu=nie\nkabel_bez_snu=x\n").kabelBezSnu, 0);
  ROWNE_L(parsujUstawieniaDnia("kabel_bez_snu=nie\nkabel_bez_snu=tak\n").kabelBezSnu, 1);
  // pół doby z naciśnięciami: B z wyłącznikiem = A (log także bez wierszy czekania), adapter nie czeka ani razu
  int bylo = zle;
  dobaHistorii = nrDnia("2026-11-08") - 1;
  Bieg A(false, "2026-11-08", 30), B(true, "2026-11-08", 30);
  A.p.pliki.pliki["/konfiguracja.txt"] = B.p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\nkabel_bez_snu=nie\n";
  std::vector<Zdarzenie> z;
  naciskiDoby(z, 9);
  Statystyka st;
  symuluj(A, B, ep(2026, 11, 9, 4, 50), ep(2026, 11, 9, 16, 0), z, "wyłącznik", st);
  ROWNE_L(B.p.sen.czekan, 0);
  ROWNE_L(st.poCzekaniu, 0);
  ROWNE_L(st.czesciowychB, st.czesciowychA);
  SPRAWDZ(!wLogu(B, "czekanie") && !wLogu(B, "sen: głęboki mimo"));
  SPRAWDZ(B.p.nvs.flagi.count("cpap/cz_akt") == 0);
  ROWNE_L(B.p.czujniki.odczytowZasilania, A.p.czujniki.odczytowZasilania);  // bez dodatkowego odczytu przed snem
  // przełączenie w trakcie pracy (komenda `W` w czuwaniu): od następnego snu czekanie
  Zdarzenie k = przycisk(ep(2026, 11, 9, 16, 30), BIT_PRAWY);
  k.co = Zdarzenie::KONFIG;
  k.tresc = "noc_zrodlo=json\nkabel_bez_snu=tak\n";
  Statystyka st2;
  symuluj(A, B, ep(2026, 11, 9, 16, 10), ep(2026, 11, 9, 20, 0), {k}, "wyłącznik → tak", st2);
  SPRAWDZ(B.p.sen.czekan > 0);
  if (zle != bylo) printf("  (wyłącznik: %d niezgodności)\n", zle - bylo);
}

// ---------- I. wygaszacz (obrazek dnia po bezczynności, R-KON-73, R-KON-77) a czekanie (17.11.2026) ----------
// Ta sama doba w dwóch ścieżkach z wygaszaczem włączonym (30 min), włączonym przy `eink.pelne_co=1`, wyłączonym
// i domyślnym przy `eink.pelne_co=0`: identyczne klatki, NVS, pliki, log i plan; ekran dnia i wejście wygaszacza
// zawsze pełne (z własnej inicjatywy panelu), strona kokpitu otwarta ▶ z wygaszacza po czekaniu częściowa (sterownik
// zna obraz, poprzedniObrazZnany()), licznik `eink.pelne_co` przestrzegany.
static std::string pbmKabel(int w, int h) {
  return "P4\n" + std::to_string(w) + " " + std::to_string(h) + "\n" + std::string((size_t)((w + 7) / 8 * h), '\x0F');
}
static int liczWLogu(const Bieg& b, const std::string& f) {
  int n = 0;
  for (auto& l : b.p.port.log) n += l.find(f) != std::string::npos;
  return n;
}
// Przejście logu ścieżki: każda klatka e-ink to wiersz „ekran N narysowany”. Liczy kokpit (ekran 1) otwarty wprost z
// obrazka wygaszacza (poprzednia klatka to ekran 0 po „slot dzienny: S-K-38”) i karty (wiersz z „karta” przed klatką
// ekranu 0); karta z budzikiem ma tło jasne (ADR-0007, ADR-0009), sprawdzane jak w testy_kolejka_linii.cpp: piksele
// (400, 300) i (790, 300) białe. Zmiana 2026-11-17.
struct PrzejscieLogu {
  int klatekZLogu = 0, kokpitZObrazka = 0, kokpitZObrazkaCzesciowo = 0, kart = 0, kartNaCzarnym = 0;
};
static bool czarnyPiksel(const std::vector<uint8_t>& k, int x, int y) {
  return (k[(size_t)(y * 100 + x / 8)] & (uint8_t)(0x80 >> (x & 7))) != 0;
}
static PrzejscieLogu przejdzLog(const Bieg& b) {
  PrzejscieLogu r;
  bool sk38 = false, karta = false, naEkranieObrazek = false;
  for (auto& l : b.p.port.log) {
    if (l.find("slot dzienny: S-K-38") != std::string::npos) sk38 = true;
    if (l.find("karta") != std::string::npos && l.rfind("ekran ", 0) == 0) karta = true;
    int nr = -1;
    if (l.rfind("ekran ", 0) != 0 || l.find(" narysowany w ") == std::string::npos ||
        sscanf(l.c_str(), "ekran %d narysowany", &nr) != 1) continue;
    size_t i = (size_t)r.klatekZLogu++;
    bool czesciowo = l.find("(odświeżenie częściowe ") != std::string::npos;
    if (nr == 1 && naEkranieObrazek) {
      r.kokpitZObrazka++;
      r.kokpitZObrazkaCzesciowo += czesciowo;
    }
    if (nr == 0 && karta && i < b.p.ekran.klatki.size()) {
      r.kart++;
      const std::vector<uint8_t>& k = b.p.ekran.klatki[i];
      r.kartNaCzarnym += czarnyPiksel(k, 400, 300) || czarnyPiksel(k, 790, 300);
    }
    naEkranieObrazek = nr == 0 && sk38 && !karta;
    sk38 = karta = false;
  }
  return r;
}
static void testWygaszacz() {
  struct Wariant {
    const char* nazwa;
    const char* konf;
    bool wygaszacz;
    int pelneCo;
  };
  const Wariant wa[] = {
      {"wygaszacz 30 min", "noc_zrodlo=json\ndzien.wygaszacz=tak\ndzien.wygaszacz_min=30\n", true, 5},
      {"wygaszacz 30 min, pelne_co=1", "noc_zrodlo=json\ndzien.wygaszacz_min=30\neink.pelne_co=1\n", true, 1},
      {"wygaszacz wyłączony", "noc_zrodlo=json\ndzien.wygaszacz=nie\ndzien.wygaszacz_min=30\n", false, 5},
      {"wygaszacz domyślny, pelne_co=0", "noc_zrodlo=json\neink.pelne_co=0\n", true, 0},
  };
  for (const Wariant& v : wa) {
    int bylo = zle;
    dobaHistorii = nrDnia("2026-11-09") - 1;
    sesjaZDoby = true;
    konfTestu.pogodaLat = 50.2597;
    konfTestu.pogodaLon = 19.0217;
    Bieg A(false, "2026-11-09", 30), B(true, "2026-11-09", 30);
    for (Bieg* x : {&A, &B}) {
      x->p.pliki.pliki["/konfiguracja.txt"] = v.konf;
      x->p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbmKabel(434, 434);
      x->p.pliki.pliki["/obrazki/obrazki.json"] = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\"}]}";
    }
    const uint8_t P = BIT_PRAWY, S = BIT_SRODEK;
    std::vector<Zdarzenie> z = {przycisk(ep(2026, 11, 10, 9, 0), P, {P}), przycisk(ep(2026, 11, 10, 10, 40), P, {P, P}),
                                przycisk(ep(2026, 11, 10, 13, 30), P, {P}), przycisk(ep(2026, 11, 10, 17, 45), S),
                                przycisk(ep(2026, 11, 10, 18, 50), P), przycisk(ep(2026, 11, 10, 20, 10), P, {P, P})};
    Statystyka st;
    symuluj(A, B, ep(2026, 11, 10, 4, 50), ep(2026, 11, 10, 21, 30), z, v.nazwa, st);
    int wygA = liczWLogu(A, "slot dzienny: S-K-38"), wygB = liczWLogu(B, "slot dzienny: S-K-38");
    if (getenv("LOG_WYGASZACZA"))
      for (auto& l : A.p.port.log)
        printf("    A: %s\n", l.c_str());
    // ekran dnia (w tym obrazek jako wygaszacz) zawsze pełny; licznik częściowych nigdy ponad `eink.pelne_co`
    int dzienCzesciowy = 0, ponadLicznik = 0;
    for (auto& l : B.p.port.log) {
      if (l.rfind("ekran 0 narysowany", 0) == 0 && l.find("odświeżenie pełne") == std::string::npos) dzienCzesciowy++;
      int x = 0, n = 0;
      size_t i = l.find("(odświeżenie częściowe ");
      if (i != std::string::npos && sscanf(l.c_str() + i, "(odświeżenie częściowe %d z %d", &x, &n) == 2 &&
          (x > n || n != v.pelneCo))
        ponadLicznik++;
    }
    // kokpit liczony tylko otwarty wprost z obrazka wygaszacza
    PrzejscieLogu pa = przejdzLog(A), pb = przejdzLog(B);
    int kokpitCzesciowy = pb.kokpitZObrazkaCzesciowo;
    printf("  wygaszacz (%s): %d przebiegów, %d po czekaniu (%d przyciskiem); S-K-38 A %d B %d; częściowych A %d B %d,"
           " kokpit z obrazka A %d B %d (częściowo A %d B %d); karty A %d B %d (na czarnym tle A %d B %d); pierwsze po"
           " przycisku po czekaniu częściowe %d z %d\n",
           v.nazwa, st.przebiegow, st.poCzekaniu, st.przyciskowPoCzekaniu, wygA, wygB, st.czesciowychA,
           st.czesciowychB, pa.kokpitZObrazka, pb.kokpitZObrazka, pa.kokpitZObrazkaCzesciowo, kokpitCzesciowy, pa.kart,
           pb.kart, pa.kartNaCzarnym, pb.kartNaCzarnym, st.czesciowychPierwszych, st.przyciskowPoCzekaniu);
    ROWNE_L(pa.klatekZLogu, (long)A.p.ekran.klatki.size());  // każda klatka ma swój wiersz w logu
    ROWNE_L(pb.klatekZLogu, (long)B.p.ekran.klatki.size());
    ROWNE_L(pa.kartNaCzarnym, 0);  // budzik nigdy na czarnym tle (ADR-0007, ADR-0009), obie ścieżki
    ROWNE_L(pb.kartNaCzarnym, 0);
    ROWNE_L(pa.kart, pb.kart);
    ROWNE_L(pa.kokpitZObrazka, pb.kokpitZObrazka);
    ROWNE_L(pa.kokpitZObrazkaCzesciowo, 0);  // po głębokim śnie kokpit z obrazka zawsze pełny
    if (!v.wygaszacz) ROWNE_L(pb.kokpitZObrazka, 0);
    ROWNE_L(wygA, wygB);
    SPRAWDZ(v.wygaszacz ? wygB > 0 : wygB == 0);
    ROWNE_L(dzienCzesciowy, 0);
    ROWNE_L(ponadLicznik, 0);
    SPRAWDZ(st.przyciskowPoCzekaniu >= 5);
    if (v.pelneCo == 0) {
      ROWNE_L(st.czesciowychB, 0);
      ROWNE_L(st.czesciowychA, 0);
    } else {
      SPRAWDZ(st.czesciowychB > st.czesciowychA);
      if (v.wygaszacz) SPRAWDZ(kokpitCzesciowy > 0);  // ▶ z obrazka po czekaniu: częściowo (sterownik zna obraz)
    }
    if (zle != bylo) printf("  (wygaszacz %s: %d niezgodności)\n", v.nazwa, zle - bylo);
    sesjaZDoby = false;
    konfTestu = KONF;
  }
}

// ---------- J. poprawki (17.11.2026) ----------
static void testPoprawkiBrzegowe() {
  using czekanie::Powod;
  auto pow = [](const czekanie::Wejscie& w) { return czekanie::decyzja(w).powod; };
  // największy wolny blok pamięci wewnętrznej
  czekanie::Wejscie w = wejscieKabel();
  w.najwiekszyBlokWewn = czekanie::WEWN_BLOK_MIN_B - 1;
  SPRAWDZ(pow(w) == Powod::WEWN_BLOK);
  w.najwiekszyBlokWewn = czekanie::WEWN_BLOK_MIN_B;
  SPRAWDZ(pow(w) == Powod::CZEKANIE);
  w.najwiekszyBlokWewn = 135u * 1024u;  // jeden egzemplarz panelu przy starcie 16.11.2026
  SPRAWDZ(pow(w) == Powod::CZEKANIE);
  w.najwiekszyBlokWewn = czekanie::BLOK_NIEZNANY;  // nieznany: bez sprawdzenia
  SPRAWDZ(pow(w) == Powod::CZEKANIE);
  w.najwiekszyBlokWewn = 0;  // na ESP32 0 = brak wolnego bloku, nie brak pomiaru
  SPRAWDZ(pow(w) == Powod::WEWN_BLOK);
  {  // domyślna Diagnostyka (bez pomiaru) zwraca BLOK_NIEZNANY, atrapa też
    AtrapaDiagnostyki dg;
    ROWNE_L(dg.najwiekszyBlokWewn(), czekanie::BLOK_NIEZNANY);
    ROWNE_L(dg.najwiekszyBlokPsram(), czekanie::BLOK_NIEZNANY);
    SPRAWDZ(czekanie::Wejscie().najwiekszyBlokPsram == czekanie::BLOK_NIEZNANY);
  }
  w.najwiekszyBlokWewn = 1024;
  w.pamiecWBudzecie = false;  // kolejność: budżet sterty przed blokiem
  SPRAWDZ(pow(w) == Powod::PAMIEC);
  // `kabel_bez_snu` nieznane (konfiguracja nieczytelna) = głęboki sen
  w = wejscieKabel();
  w.ustawienieNieznane = true;
  SPRAWDZ(pow(w) == Powod::USTAWIENIE_NIEZNANE);
  SPRAWDZ(std::string(czekanie::opis(Powod::USTAWIENIE_NIEZNANE)).find("nieczytelna") != std::string::npos);
  w.wylaczone = true;
  SPRAWDZ(pow(w) == Powod::WYLACZONE);
  // NVS `cpap` z nowszej wersji
  w = wejscieKabel();
  w.nvsNowszy = true;
  SPRAWDZ(pow(w) == Powod::NVS_NOWSZY);
  w.poBledzie = true;
  SPRAWDZ(pow(w) == Powod::NVS_NOWSZY);
  w.wylaczone = true;
  SPRAWDZ(pow(w) == Powod::WYLACZONE);
  for (int i = 0; i <= (int)Powod::PLANOWY_RESTART; i++) SPRAWDZ(std::string(czekanie::opis((Powod)i)) != "?");
  // blokada z datą w przyszłości
  using czekanie::Blokada;
  SPRAWDZ(czekanie::blokada("2027-11-16", "2026-11-17") == Blokada::PRZYSZLA);
  SPRAWDZ(czekanie::blokada("2026-11-19", "2026-11-17") == Blokada::PRZYSZLA);
  SPRAWDZ(czekanie::blokada("2026-11-18", "2026-11-17") == Blokada::TRWA);  // 1 doba: korekta zegara o godziny
  SPRAWDZ(czekanie::blokada("2026-11-17", "2026-11-17") == Blokada::TRWA);
  SPRAWDZ(czekanie::blokada("2026-11-16", "2026-11-17") == Blokada::KONIEC);
  SPRAWDZ(czekanie::blokada("zepsute", "2026-11-17") == Blokada::PRZYSZLA);
  SPRAWDZ(czekanie::blokada("2027-11-16", "") == Blokada::TRWA);  // zegar nieznany: zostaje
  // przycisk kończący czekanie (wybór w rdzeniu, adapter tylko kopiuje stan)
  {
    czekanie::KlawiszCzekania k[4];
    czekanie::NacisniecieCzekania q[4];
    const uint8_t wszystkie = BIT_LEWY | BIT_SRODEK | BIT_PRAWY;
    czekanie::PrzyciskKonczacy r = czekanie::przyciskKonczacy(wszystkie, k, q, 0);
    SPRAWDZ(r.bit == 0 && r.zdjacZKolejki == 0);
    // (a) z kolejki niesie czas trzymania
    q[0] = {(uint8_t)Przycisk::PRAWY, 2400, 10000000};
    r = czekanie::przyciskKonczacy(wszystkie, k, q, 1);
    SPRAWDZ(r.bit == BIT_PRAWY && r.zKolejki && r.trwaloMs == 2400 && r.zdjacZKolejki == 1 && r.odUs == 7600000);
    // trzymany teraz: od chwili wciśnięcia (c)
    k[(int)Przycisk::LEWY] = {true, false, 12000000};
    r = czekanie::przyciskKonczacy(wszystkie, k, q, 0);
    SPRAWDZ(r.bit == BIT_LEWY && !r.zKolejki && r.klawisz == (int)Przycisk::LEWY && r.odUs == 12000000);
    // (b) starsze z kolejki przed trzymanym (wciśnięte 7,6 s, trzymany od 12 s)
    r = czekanie::przyciskKonczacy(wszystkie, k, q, 1);
    SPRAWDZ(r.bit == BIT_PRAWY && r.zKolejki && r.zdjacZKolejki == 1);
    // trzymany wciśnięty wcześniej niż naciśnięcie z kolejki: najpierw trzymany, kolejka zostaje
    k[(int)Przycisk::LEWY].odUs = 5000000;
    r = czekanie::przyciskKonczacy(wszystkie, k, q, 1);
    SPRAWDZ(r.bit == BIT_LEWY && !r.zKolejki && r.zdjacZKolejki == 0);
    // przycisk spoza maski: trzymany pomijany, z kolejki zdjęty bez obsługi; już obsłużony (`pomin`) nie budzi
    r = czekanie::przyciskKonczacy(BIT_PRAWY, k, q, 1);
    SPRAWDZ(r.bit == BIT_PRAWY && r.zKolejki);
    q[0].przycisk = (uint8_t)Przycisk::SRODEK;
    r = czekanie::przyciskKonczacy(BIT_PRAWY, k, q, 1);
    SPRAWDZ(r.bit == 0 && r.zdjacZKolejki == 1);
    k[(int)Przycisk::LEWY].pomin = true;
    r = czekanie::przyciskKonczacy(wszystkie, k, q, 0);
    SPRAWDZ(r.bit == 0);
    // chwila puszczenia nieznana: z kolejki pierwsze (puszczone, zanim pętla je zobaczyła)
    k[(int)Przycisk::LEWY] = {true, false, 1000};
    q[0] = {(uint8_t)Przycisk::PRAWY, 100, 0};
    r = czekanie::przyciskKonczacy(wszystkie, k, q, 1);
    SPRAWDZ(r.bit == BIT_PRAWY && r.zKolejki && r.odUs == 0);
  }
  // Na dyrygencie: naciśnięcie budzące już puszczone, trzymane 2,6 s (z kolejki) = przytrzymanie jak po
  // głębokim śnie z przyciskiem trzymanym 2,6 s; krótkie (120 ms) = krótkie
  bezNowychNocy = true;
  dobaHistorii = nrDnia("2026-11-04") - 1;
  for (uint32_t trzymal : {2600u, 120u}) {
    Bieg A(false, "2026-11-04", 30), B(false, "2026-11-04", 30);
    Wejscie s;
    s.ep = ep(2026, 11, 4, 12, 0);
    s.pw = PowodWybudzenia::START;
    s.prz = PrzyczynaStartu::ZASILANIE;
    airbridge(A, s.ep);
    airbridge(B, s.ep);
    przebieg(A, s);
    przebieg(B, s);
    Wejscie x;
    x.ep = ep(2026, 11, 4, 12, 30);
    x.pw = PowodWybudzenia::PRZYCISK;
    x.budzace = BIT_PRAWY;
    x.skrypt = {{0, BIT_PRAWY}, {trzymal, 0}};
    Wejscie y = x;
    y.skrypt.clear();
    B.p.start.trwanieBudzacego = trzymal;
    airbridge(A, x.ep);
    airbridge(B, x.ep);
    Wynik a = przebieg(A, x), b = przebieg(B, y);
    B.p.start.trwanieBudzacego = 0;
    // plan snu liczony od końca przebiegu: A trzymała przycisk w przebiegu (2,6 s), B dostała go już puszczonego
    long dPlanu = (long)b.plan.sekund - (long)a.plan.sekund;
    // (stan RTC bez porównania: chwile w nim, np. obsłużone do, przesunięte o czas trzymania w ścieżce A)
    bool zgodne = a.klatki == b.klatki && a.klatek == b.klatek && dPlanu >= 0 &&
                  dPlanu <= (long)trzymal / 1000 + 1 && a.plan.przyciski == b.plan.przyciski;
    for (size_t i = 0; zgodne && i < a.wiersze.size() && i < b.wiersze.size(); i++)
      if (a.wiersze[i] != b.wiersze[i] && a.wiersze[i].rfind("sen na ", 0) != 0) zgodne = false;
    zgodne = zgodne && a.wiersze.size() == b.wiersze.size();
    printf("  przycisk budzący z kolejki (%lu ms) wobec trzymanego: klatek %d/%d, %s\n", (unsigned long)trzymal,
           a.klatek, b.klatek, zgodne ? "zgodne" : "NIEZGODNE");
    if (!zgodne)
      for (size_t i = 0; i < std::max(a.wiersze.size(), b.wiersze.size()); i++) {
        std::string u = i < a.wiersze.size() ? a.wiersze[i] : "", v = i < b.wiersze.size() ? b.wiersze[i] : "";
        if (u != v) printf("    A: %s\n    B: %s\n", u.c_str(), v.c_str());
      }
    SPRAWDZ(zgodne);
  }
  // NVS `cpap` z nowszej wersji: bez czekania i bez zapisu flagi trybu bez snu i blokady
  {
    Bieg B(true, "2026-11-04", 30);
    B.p.nvs.liczbyU["cpap/wersja"] = WERSJA_NVS_CPAP + 1;
    B.p.nvs.flagi["cpap/cz_akt"] = true;  // program był w trybie bez snu, restart po błędzie
    Wejscie s;
    s.ep = ep(2026, 11, 4, 12, 0);
    s.pw = PowodWybudzenia::START;
    s.prz = PrzyczynaStartu::TASK_WDT;
    airbridge(B, s.ep);
    size_t l0 = B.p.port.log.size();
    Wynik r = przebieg(B, s);
    SPRAWDZ(r.spal && !r.czekal);
    SPRAWDZ(wLogu(B, "sen: głęboki mimo zasilania zewnętrznego: NVS z nowszej wersji, tylko do odczytu", l0));
    SPRAWDZ(B.p.nvs.teksty.count("cpap/cz_blok") == 0);
    SPRAWDZ(B.p.nvs.flagi["cpap/cz_akt"]);  // bez zapisu (przestrzeń tylko do odczytu)
  }
  // Na dyrygencie: blokada z datą rok naprzód usunięta przy pierwszej decyzji, czekanie od razu
  {
    Bieg B(true, "2026-11-04", 30);
    B.p.nvs.teksty["cpap/cz_blok"] = "2027-11-04";
    Wejscie s;
    s.ep = ep(2026, 11, 4, 12, 0);
    s.pw = PowodWybudzenia::START;
    s.prz = PrzyczynaStartu::ZASILANIE;
    airbridge(B, s.ep);
    size_t l0 = B.p.port.log.size();
    Wynik r = przebieg(B, s);
    SPRAWDZ(r.czekal);
    SPRAWDZ(B.p.nvs.teksty.count("cpap/cz_blok") == 0);
    SPRAWDZ(wLogu(B, "czekanie na kablu: blokada po restarcie po błędzie z datą 2027-11-04 dalej niż doba po dziś "
                     "2026-11-04, usunięta (T-WYM-17)", l0));
  }
  // Na dyrygencie: mały blok pamięci wewnętrznej = głęboki sen z powodem
  {
    Bieg B(true, "2026-11-04", 30);
    B.p.diag.blokWewn = 20u * 1024u;
    Wejscie s;
    s.ep = ep(2026, 11, 4, 12, 0);
    s.pw = PowodWybudzenia::START;
    s.prz = PrzyczynaStartu::ZASILANIE;
    airbridge(B, s.ep);
    size_t l0 = B.p.port.log.size();
    Wynik r = przebieg(B, s);
    SPRAWDZ(r.spal && !r.czekal && wLogu(B, "mały wolny blok pamięci wewnętrznej", l0));
  }
  // `W kabel_bez_snu=nie` w czuwaniu działa od decyzji o śnie tego przebiegu, bez wierszy logu i odczytów
  {
    Bieg B(true, "2026-11-04", 30);
    Wejscie s;
    s.ep = ep(2026, 11, 4, 12, 0);
    s.pw = PowodWybudzenia::START;
    s.prz = PrzyczynaStartu::ZASILANIE;
    airbridge(B, s.ep);
    SPRAWDZ(przebieg(B, s).czekal);
    Wejscie x;
    x.ep = ep(2026, 11, 4, 12, 10);
    x.pw = PowodWybudzenia::PRZYCISK;
    x.budzace = BIT_PRAWY;
    x.skrypt = naciski(BIT_PRAWY, {});
    x.port = komendaKonfiguracji("noc_zrodlo=json\nkabel_bez_snu=nie\n");
    airbridge(B, x.ep);
    size_t l0 = B.p.port.log.size();
    Wynik r = przebieg(B, x);
    SPRAWDZ(r.spal && !r.czekal);
    SPRAWDZ(!wLogu(B, "sen: głęboki mimo", l0));  // wyłącznik: log jak przed ADR-0047
    // i z powrotem `tak`: od decyzji tego samego przebiegu (po głębokim śnie nowy program)
    B.nowyProgram();
    x.ep = ep(2026, 11, 4, 12, 40);
    x.port = komendaKonfiguracji("noc_zrodlo=json\nkabel_bez_snu=tak\n");
    airbridge(B, x.ep);
    SPRAWDZ(przebieg(B, x).czekal);
  }
  bezNowychNocy = false;
}

// ---------- G. 30 dób na jednym żywym stanie: żywe bloki sterty bez wzrostu ----------
static void testTrzydziesciDob() {
  bezNowychNocy = true;
  dobaHistorii = nrDnia("2026-11-04") - 1;
  Bieg B(true, "2026-11-04", 40);
  std::vector<long long> bloki;  // żywe bloki po przebiegu przyciskiem w południe każdej doby
  Wejscie w;
  w.ep = ep(2026, 11, 4, 11, 0);
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  int przebiegow = 0, programow0 = B.programow;
  for (int i = 0; i < 20000; i++) {
    airbridge(B, w.ep);
    Wynik r = przebieg(B, w);
    B.p.port.log.clear();
    B.p.ekran.klatki.clear();
    B.p.ekran.rodzaje.clear();
    przebiegow++;
    if (w.pw == PowodWybudzenia::PRZYCISK) bloki.push_back(zywychBlokow);
    uint32_t nast = w.ep + r.msPrzebiegu / 1000 + (r.plan.sekund ? r.plan.sekund : 1);
    if (nast >= ep(2026, 12, 4, 11, 0)) break;
    w = Wejscie();
    uint32_t poludnie = ep(2026, 11, 4, 12, 0) + (uint32_t)bloki.size() * 86400u;
    if (nast > poludnie) {
      w.ep = poludnie;
      w.pw = PowodWybudzenia::PRZYCISK;
      w.budzace = BIT_PRAWY;
      w.skrypt = naciski(BIT_PRAWY, {BIT_PRAWY, BIT_PRAWY, BIT_LEWY, BIT_LEWY});
    } else {
      w.ep = nast;
    }
  }
  SPRAWDZ(bloki.size() >= 29);
  // jedyny restart: planowy po PRZEBIEGOW_NAJWIECEJ (czas od startu w atrapie nieznany)
  ROWNE_L(B.programow - programow0, (uint32_t)przebiegow / czekanie::PRZEBIEGOW_NAJWIECEJ);
  long long wczesniej = 0, pozniej = 0;
  for (size_t i = 0; i < bloki.size(); i++) {
    if (i >= 5 && i < 10) wczesniej = std::max(wczesniej, bloki[i]);
    if (i >= bloki.size() - 5) pozniej = std::max(pozniej, bloki[i]);
  }
  printf("  30 dób: %d przebiegów, %d programów, żywe bloki sterty po przycisku: doby 6–10 najwyżej %lld, ostatnie 5"
         " najwyżej %lld; pamięć plików %lu B, noce w pamięci %lu\n",
         przebiegow, B.programow - programow0 + 1, wczesniej, pozniej, (unsigned long)B.pl.bajtow(),
         (unsigned long)B.pp.historia.noce.size());
  SPRAWDZ(pozniej <= wczesniej + 64);  // bez wzrostu (zapas na wahania kolejności zapisów w atrapach)
  bezNowychNocy = false;
}

// ---------- K. limit ładowania (ADR-0046) a czekanie (ADR-0047), 17.11.2026 ----------
// limitLadowania() jest wołane raz na przebieg (Przebieg::limitLadowaniaTeraz()). W czekaniu jeden proces robi wiele
// przebiegów bez głębokiego snu, a między nimi czyta zasilanie (REG0B, REG08, REG0A) co 10 s na tej samej magistrali.
// Ta sama doba w dwóch ścieżkach z atrapą ładowarki na każdej: po każdej parze przebiegów rejestry układu, tryb host
// i lista zapisów mają być identyczne (stan zastany w NVS `lad_zast` porównuje już skrót NVS w porownaj(), wiersze
// `ladowanie: …` skrót logu). Upływ licznika watchdoga układu symulowany między przebiegami w obu ścieżkach.
static void testLimitLadowania() {
  struct Wariant {
    const char* nazwa;
    bool opcja;          // `ladowanie_limit=tak` od początku doby
    bool stanZastany;     // stan układu z jednego egzemplarza panelu 16.11.2026 (licznik wyłączony, host) zamiast stanu po starcie
    int nieudanyZapis;   // numer zapisu, który się nie uda (oba układy), -1 = brak
    bool przelaczanie;   // komendą `W` wyłączenie 13:00, włączenie 18:00; odłączenia kabla wieczorem
  };
  const Wariant warianty[] = {
      {"wyłączona, stan zastany", false, true, -1, false},
      {"wyłączona, stan po starcie", false, false, -1, false},
      {"włączona, stan zastany", true, true, -1, false},
      {"włączona, stan po starcie", true, false, -1, false},
      {"włączona, nieudany zapis licznika", true, false, 2, false},
      {"przełączanie i kabel", true, true, -1, true},
  };
  for (const Wariant& v : warianty) {
    int bylo = zle;
    dobaHistorii = nrDnia("2026-11-08") - 1;
    Bieg A(false, "2026-11-08", 30), B(true, "2026-11-08", 30);
    AtrapaLadowarki ladA, ladB;
    for (AtrapaLadowarki* l : {&ladA, &ladB}) {
      if (v.stanZastany) {
        l->r[5] = 0x8F;
        l->r[6] = 0x62;
        l->host = true;
      }
      l->nieudanyZapisNr = v.nieudanyZapis;
    }
    const uint8_t reg04Start = ladA.r[4];
    A.p.czujniki.lad = &ladA;
    B.p.czujniki.lad = &ladB;
    A.p.czujniki.zasilanieZUkladu = B.p.czujniki.zasilanieZUkladu = true;
    B.p.sen.sprawdzen = 90;  // 15 min czekania po 10 s: 90 odczytów zasilania na magistrali układu (KAWALEK_CZEKANIA_S)
    const std::string bez = "noc_zrodlo=json\n", z = "noc_zrodlo=json\nladowanie_limit=tak\n";
    A.p.pliki.pliki["/konfiguracja.txt"] = B.p.pliki.pliki["/konfiguracja.txt"] = v.opcja ? z : bez;
    std::vector<Zdarzenie> zd;
    naciskiDoby(zd, 9);
    if (v.przelaczanie) {
      Zdarzenie k1 = przycisk(ep(2026, 11, 9, 13, 0), BIT_PRAWY);
      k1.co = Zdarzenie::KONFIG;
      k1.tresc = bez;
      Zdarzenie k2 = przycisk(ep(2026, 11, 9, 18, 0), BIT_PRAWY);
      k2.co = Zdarzenie::KONFIG;
      k2.tresc = z;
      zd.push_back(k1);
      zd.push_back(k2);
      zd.push_back(inne(Zdarzenie::ODLACZ, ep(2026, 11, 9, 19, 0)));
      zd.push_back(inne(Zdarzenie::PODLACZ, ep(2026, 11, 9, 19, 50)));
      zd.push_back(inne(Zdarzenie::ODLACZ_W_CZEKANIU, ep(2026, 11, 9, 21, 0)));
      zd.push_back(inne(Zdarzenie::PODLACZ, ep(2026, 11, 9, 21, 40)));
    }
    int rozne = 0, par = 0;
    std::vector<uint8_t> reg04poKomendzie;  // REG04 po każdej parze (do sprawdzenia przywrócenia i ponownego limitu)
    poParze = [&](Bieg&, Bieg&, uint32_t t) {
      par++;
      bool zgodne = memcmp(ladA.r, ladB.r, sizeof ladA.r) == 0 && ladA.host == ladB.host && ladA.zapisy == ladB.zapisy;
      if (!zgodne && ++rozne <= 3)
        printf("BŁĄD limit ładowania (%s) %s: REG04 %02X/%02X REG05 %02X/%02X zapisów %zu/%zu\n", v.nazwa,
               hhmm(t).c_str(), ladA.r[4], ladB.r[4], ladA.r[5], ladB.r[5], ladA.zapisy.size(), ladB.zapisy.size());
      reg04poKomendzie.push_back(ladA.r[4]);
      ladA.uplywWatchdoga();  // między przebiegami mijają minuty: włączony licznik w trybie host by upłynął
      ladB.uplywWatchdoga();
    };
    Statystyka st;
    symuluj(A, B, ep(2026, 11, 9, 4, 50), ep(2026, 11, 10, 4, 50), zd, "limit ładowania", st);
    poParze = nullptr;
    ROWNE_L(rozne, 0);
    SPRAWDZ(par == st.przebiegow && par > 50);
    SPRAWDZ(st.poCzekaniu > 20 && B.p.sen.czekan > 20);
    SPRAWDZ(ladB.odczytow > ladA.odczytow + 1000);  // odczyty zasilania w czekaniu naprawdę szły przez układ
    int logA = liczWLogu(A, "ladowanie: "), logB = liczWLogu(B, "ladowanie: ");
    ROWNE_L(logA, logB);
    ROWNE_L(A.p.nvs.liczbyU.count("cpap/lad_zast"), B.p.nvs.liczbyU.count("cpap/lad_zast"));
    if (!v.opcja) {
      ROWNE_L(ladB.zapisy.size(), 0);  // opcja wyłączona bez wpisu: zero zapisów w obu ścieżkach
      ROWNE_L(logB, 0);
      ROWNE_L(ladB.r[4], reg04Start);
    } else if (!v.przelaczanie && v.nieudanyZapis < 0) {
      ROWNE_L(logB, st.przebiegow);  // jedno wywołanie limitu na przebieg, także po czekaniu
      ROWNE_L(ladB.r[4], 0x10);      // 3920 mV
      ROWNE_L(ladB.r[5] & 0x30, 0);  // licznik watchdoga wyłączony
      ROWNE_L(ladB.zapisy.size(), v.stanZastany ? 1u : 2u);  // limit raz, potem bez powtórnych zapisów
    } else if (v.nieudanyZapis > 0) {
      ROWNE_L(ladB.r[4], 0x10);
      ROWNE_L(ladB.r[5] & 0x30, 0);
      SPRAWDZ(ladB.zapisy.size() >= 3);
    } else {
      bool przywrocone = std::find(reg04poKomendzie.begin(), reg04poKomendzie.end(), reg04Start) !=
                         reg04poKomendzie.end();
      SPRAWDZ(przywrocone);          // wyłączenie komendą `W` przywróciło zastane VREG (także po czekaniu)
      ROWNE_L(ladB.r[4], 0x10);      // ponowne włączenie: limit znowu
      SPRAWDZ(st.glebokich >= 1);    // odłączenia kabla: głęboki sen na ścieżce B
    }
    printf("  limit ładowania, %s: %d przebiegów (%d po czekaniu), zapisów do układu %zu i %zu, odczytów układu %d i"
           " %d, wierszy `ladowanie:` %d i %d, REG04 %02X\n",
           v.nazwa, st.przebiegow, st.poCzekaniu, ladA.zapisy.size(), ladB.zapisy.size(), ladA.odczytow, ladB.odczytow,
           logA, logB, ladB.r[4]);
    if (zle != bylo) printf("  (limit ładowania, %s: %d niezgodności)\n", v.nazwa, zle - bylo);
  }
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  printf("testy czekania na zasilaniu zewnętrznym zamiast głębokiego snu (ADR-0047, T-TEST-500)\n");
  testDecyzja();
  testPlikiZPamiecia();
  testOdlaczenie();
  testAwaria();
  testWylacznik();
  testPoprawkiBrzegowe();
  testWygaszacz();
  testLimitLadowania();
  testLiczniki366();
  testDoba366();
  testTydzien();
  testTrzydziesciDob();
  printf("testy_kabel_bez_snu: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
