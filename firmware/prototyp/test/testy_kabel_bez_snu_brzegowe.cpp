// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe czekania na kablu zamiast głębokiego snu (ADR-0047).
// Testy przypadków brzegowych, niezależne od test/testy_kabel_bez_snu.cpp (własna uprząż, własne atrapy usterek):
//  R1. PlikiZPamiecia a przejściowy błąd odczytu nośnika: czy „brak pliku” z jednego nieudanego odczytu zostaje
//      w pamięci na kolejne przebiegi (na ścieżce z głębokim snem następne wybudzenie czyta od nowa).
//  R2. To samo na dyrygencie: jeden przejściowy błąd odczytu pliku nocy tuż po imporcie, obie ścieżki z tą samą
//      usterką nośnika (pierwszy odczyt z nośnika po uzbrojeniu), porównanie klatek, plików i logu dalej.
//  R3. Wyłącznik `kabel_bez_snu=nie` wgrany komendą `W` w czuwaniu: czy ten sam przebieg jeszcze czeka.
//  R4. Losowe sekwencje (własność): naciśnięcia z kolejką, komendy `W` (wyłącznik, N, układ), odłączanie kabla między
//      przebiegami i w czekaniu, brak Wi-Fi, start zasilania, restart po błędzie (strażnik, panika), skok zegara;
//      ścieżka A (głęboki sen) wobec B (czekanie) → te same klatki, NVS (bez cz_akt/cz_blok), pliki, log, plan.
// Uruchomienie: tools/prototyp/testy.sh (program testy_kabel_bez_snu_brzegowe). BRZEG_LOG=1 drukuje szczegóły.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <map>
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

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
static bool gadaj() { return getenv("BRZEG_LOG") != nullptr; }

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

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_TERAPIA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static std::string raport(long day) {
  std::string d = std::to_string(day);
  std::string ahi = std::to_string(2 + day % 5) + ".4", wp95 = std::to_string(12 + (day * 7) % 19) + ".6";
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":" + ahi + ",\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":25,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Press\",\"label\":\"C\",\"unit\":\"cmH2O\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":6.2,\"mean\":10.1,\"median\":9.8,\"p95\":13.6,\"max\":16.0,"
         "\"source\":\"edf\"},{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":" + wp95 + ",\"max\":30.2,"
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

// ---------- losowość powtarzalna ----------
struct Los {
  uint64_t s;
  explicit Los(uint64_t z) : s(z * 0x9E3779B97F4A7C15ULL + 1) {}
  uint32_t n() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return (uint32_t)(s >> 11);
  }
  int od(int a, int b) { return a + (int)(n() % (uint32_t)(b - a + 1)); }
  bool p(int procent) { return (int)(n() % 100) < procent; }
};

static Noc nocLos(Los& r, const std::string& d, int i) {
  Noc n;
  n.data = d;
  n.wazna = true;
  n.m01 = r.od(300, 480);
  n.ahi = (float)r.od(5, 90) / 10.0f;
  n.cmed = (float)r.od(90, 108) / 10.0f;
  n.cp95 = (float)r.od(120, 138) / 10.0f;
  n.wmed = (float)r.od(15, 45) / 10.0f;
  n.wp95 = (float)r.od(70, 300) / 10.0f;
  n.emed = 6.5f;
  n.ep95 = 8.0f;
  n.psmed = 4.0f;
  n.psp95 = 7.0f;
  n.zrodlo = 'A';
  n.import = ep(2026, 1, 1, 7, 0) + (uint32_t)i * 86400u;
  n.oa = 0.5f;
  n.ca = (float)r.od(5, 80) / 10.0f;
  n.h = 1.0f;
  n.csr = 3.0f;
  n.tryb = "ASVAuto";
  n.ustawienia = "ASVAuto/EAI5/EAX10/ANS3/AXS10";
  n.pna = "AirCurve_10_CS_PaceWave";
  return n;
}
static void bezLogu(const std::string&) {}

// ---------- nośnik z przejściową usterką odczytu ----------
// Uzbrojona ścieżka: pierwszy odczyt treści z nośnika (czytaj()) po uzbrojeniu się nie udaje (np. brak pamięci na
// bufor pliku, LittleFS.open() == nullptr przy Wi-Fi i TLS); następne działają. Ta sama usterka fizyczna na obu
// ścieżkach (A bez warstwy pamięci, B z PlikiZPamiecia nad tym samym typem nośnika).
class PlikiUsterka : public Pliki {
 public:
  explicit PlikiUsterka(AtrapaPlikow& p) : p_(p) {}
  std::set<std::string> uzbrojone;
  int usterek = 0;
  bool zamontuj() override { return p_.zamontuj(); }
  bool zamontowane() override { return p_.zamontowane(); }
  bool istnieje(const char* n) override { return p_.istnieje(n); }
  bool czytajWiersze(const char* n, std::vector<std::string>& w) override { return p_.czytajWiersze(n, w); }
  bool zapiszAtomowo(const char* n, const std::string& t) override { return p_.zapiszAtomowo(n, t); }
  bool dopiszWiersz(const char* n, const std::string& w) override { return p_.dopiszWiersz(n, w); }
  bool pierwszyWiersz(const char* n, std::string& w) override { return p_.pierwszyWiersz(n, w); }
  bool usun(const char* n) override { return p_.usun(n); }
  bool zmienNazwe(const char* z, const char* na) override { return p_.zmienNazwe(z, na); }
  void odmontuj() override { p_.odmontuj(); }
  bool czytaj(const char* n, std::string& t) override {
    if (p_.zamontowane() && uzbrojone.erase(n)) {
      usterek++;
      t.clear();
      return false;
    }
    return p_.czytaj(n, t);
  }
  size_t zapisz(const char* n, const std::string& t) override { return p_.zapisz(n, t); }
  bool listuj(const char* k, std::vector<std::string>& nazwy) override { return p_.listuj(k, nazwy); }
  OpisPartycji partycja() override { return p_.partycja(); }
  bool czytajPartycje(size_t ile, std::string& b) override { return p_.czytajPartycje(ile, b); }
  bool formatuj() override { return p_.formatuj(); }
  long long wolneMiejsce() override { return p_.wolneMiejsce(); }

 private:
  AtrapaPlikow& p_;
};

// ---------- jeden panel ----------
struct Bieg {
  Panel p;
  const bool czekanie;
  PlikiUsterka u;
  PlikiZPamiecia pl;
  PamiecPrzebiegow pp;
  int programow = 1;
  Bieg(bool c, const std::string& ostatnia, int nocy, uint64_t ziarno) : czekanie(c), u(p.pliki), pl(u) {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.pliki.montowane = true;
    Magazyn mag(p.pliki, bezLogu);
    Los r(ziarno);
    std::map<std::string, std::vector<std::string>> lata;
    for (int i = 0; i < nocy; i++) {
      std::string d = dodajDni(ostatnia, i - (nocy - 1));
      lata[d.substr(0, 4)].push_back(wierszNocyV5(nocLos(r, d, i)));
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
    Pliki& f = czekanie ? static_cast<Pliki&>(pl) : static_cast<Pliki&>(u);
    return Adaptery{p.zegar, p.nvs, f, p.siec, p.ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik,
                    p.sen, p.port, p.diag, &p.wydania};
  }
  void nowyProgram() {  // jak start programu od zera (src/main.cpp): pamięć RAM ginie
    pp = PamiecPrzebiegow();
    pl.wlacz(false);
    p.ekran.zapomnij();
    programow++;
  }
};

struct Wejscie {
  uint32_t ep = 0;
  PowodWybudzenia pw = PowodWybudzenia::ZEGAR;
  PrzyczynaStartu prz = PrzyczynaStartu::USPIENIE;
  uint8_t budzace = 0;
  std::vector<std::pair<uint32_t, uint8_t>> skrypt;
  std::vector<std::pair<int, uint32_t>> kolejka;
  std::string port;
};

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
  uint64_t klatki = 0, stan = 0, log = 0, pliki = 0;
  int klatek = 0, czesciowych = 0;
  PlanSnu plan;
  bool spal = false, czekal = false;
  uint32_t msPrzebiegu = 0;
  std::vector<std::string> wiersze;
  std::map<std::string, std::string> plikiMapa;
};

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
  int snow0 = p.sen.snow, czek0 = p.sen.czekan;
  bool czekano = false;
  if (b.czekanie) czekano = Dyrygent(b.ad(), KONF, &b.pp).wybudzenie();
  else Dyrygent(b.ad(), KONF).wybudzenie();
  Wynik r;
  r.msPrzebiegu = p.zegar.msTeraz;
  r.spal = p.sen.snow > snow0;
  r.czekal = p.sen.czekan > czek0 && czekano;
  r.plan = p.sen.plan;
  Skrot k;
  for (size_t i = kl0; i < p.ekran.klatki.size(); i++) {
    k.bajty(p.ekran.klatki[i].data(), p.ekran.klatki[i].size());
    r.czesciowych += p.ekran.rodzaje[i] == Odswiezenie::CZESCIOWE;
  }
  r.klatek = (int)(p.ekran.klatki.size() - kl0);
  r.klatki = k.k;
  Skrot s, sp;
  for (auto& kv : p.nvs.teksty) {
    if (kv.first == "cpap/cz_blok") continue;
    s.tekst(kv.first);
    s.tekst(kv.second);
  }
  for (auto& kv : p.nvs.liczbyU) { s.tekst(kv.first); s.pole(kv.second); }
  for (auto& kv : p.nvs.liczby) { s.tekst(kv.first); s.pole(kv.second); }
  for (auto& kv : p.nvs.flagi) {
    if (kv.first == "cpap/cz_akt") continue;
    s.tekst(kv.first);
    s.pole(kv.second);
  }
  for (auto& kv : p.nvs.bajtyMap) { s.tekst(kv.first); s.tekst(kv.second); }
  for (auto& kv : p.pliki.pliki) { sp.tekst(kv.first); sp.tekst(kv.second); }
  r.plikiMapa = p.pliki.pliki;
  const StanRtc& rt = p.start.stan;
  s.pole(rt.ekran); s.pole(rt.ekranOd); s.pole(rt.klatkaSkrot); s.pole(rt.klatkaOd); s.pole(rt.obsluzoneDo);
  s.pole(rt.wybudzen); s.pole(rt.planNaZasilaniu); s.pole(rt.slotDziennyEp);
  r.stan = s.k;
  r.pliki = sp.k;
  Skrot lg;
  for (size_t i = log0; i < p.port.log.size(); i++) {
    if (wierszCzekania(p.port.log[i])) continue;
    r.wiersze.push_back(bezRodzaju(p.port.log[i]));
    lg.tekst(r.wiersze.back());
  }
  r.log = lg.k;
  if (b.czekanie) {
    if (czekano) b.pl.wlacz(true);
    else b.nowyProgram();
  }
  p.d.z.clear();
  p.siec.zadania.clear();
  p.siec.naglowki.clear();
  return r;
}

static int pokazanych = 0;
// true = zgodne
static bool porownaj(const char* co, uint32_t t, const Wynik& a, const Wynik& b) {
  bool zgodne = a.klatki == b.klatki && a.klatek == b.klatek && a.stan == b.stan && a.pliki == b.pliki &&
                a.log == b.log && a.plan.sekund == b.plan.sekund && a.plan.przyciski == b.plan.przyciski &&
                a.plan.budzZegarem == b.plan.budzZegarem && a.spal == (b.spal || b.czekal) &&
                b.czesciowych >= a.czesciowych;
  if (zgodne) return true;
  if (++pokazanych > 12) return false;
  printf("  NIEZGODNOŚĆ %s %s: klatki %d/%d (%s), stan %s, pliki %s, log %s, plan %lu/%lu s, przyciski %u/%u, "
         "sen %d/%d czekanie %d\n",
         co, hhmm(t).c_str(), a.klatek, b.klatek, a.klatki == b.klatki ? "=" : "≠", a.stan == b.stan ? "=" : "≠",
         a.pliki == b.pliki ? "=" : "≠", a.log == b.log ? "=" : "≠", (unsigned long)a.plan.sekund,
         (unsigned long)b.plan.sekund, (unsigned)a.plan.przyciski, (unsigned)b.plan.przyciski, (int)a.spal,
         (int)b.spal, (int)b.czekal);
  size_t n = std::max(a.wiersze.size(), b.wiersze.size());
  for (size_t i = 0, pok = 0; i < n && pok < 4; i++) {
    std::string x = i < a.wiersze.size() ? a.wiersze[i] : "(brak)", y = i < b.wiersze.size() ? b.wiersze[i] : "(brak)";
    if (x == y) continue;
    printf("    A: %s\n    B: %s\n", x.c_str(), y.c_str());
    pok++;
  }
  for (auto& kv : a.plikiMapa) {
    auto j = b.plikiMapa.find(kv.first);
    if (j == b.plikiMapa.end()) printf("    plik tylko w A: %s\n", kv.first.c_str());
    else if (j->second != kv.second)
      printf("    plik różny: %s (A %zu B, B %zu B)\n", kv.first.c_str(), kv.second.size(), j->second.size());
  }
  for (auto& kv : b.plikiMapa)
    if (!a.plikiMapa.count(kv.first)) printf("    plik tylko w B: %s\n", kv.first.c_str());
  return false;
}

// AirBridge: terapia 23:00–06:30, potem noc z „wczoraj” na liście.
static void airbridge(Bieg& b, uint32_t t) {
  b.p.siec.odpowiedzi.clear();
  int m = minutaLok(t);
  if (m >= 23 * 60 || m < 6 * 60 + 30) {
    b.p.siec.dodaj("/api/status", 200, STATUS_TERAPIA);
    return;
  }
  long ostatnia = nrDnia(dataLok(t)) - 1;
  b.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  b.p.siec.dodaj(sciezkaListy(), 200, lista(ostatnia - 9, ostatnia));
  for (long d = ostatnia - 9; d <= ostatnia; d++) b.p.siec.dodaj(sciezkaNocy(d), 200, raport(d));
}

static std::vector<std::pair<uint32_t, uint8_t>> naciski(uint8_t budzacy, uint32_t trzymaj,
                                                         const std::vector<uint8_t>& kolejne) {
  std::vector<std::pair<uint32_t, uint8_t>> s = {{0, budzacy}, {trzymaj, 0}};
  uint32_t t = trzymaj + 2500;
  for (uint8_t b : kolejne) {
    s.push_back({t, b});
    s.push_back({t + 200, 0});
    t += 2500;
  }
  return s;
}
static std::string komendaKonfiguracji(const std::string& tresc) {
  return "W konfiguracja.txt " + std::to_string(tresc.size()) + " " + sha256Hex(tresc) + "\n" + tresc;
}

// ---------- symulacja obu ścieżek ----------
static const char* const KONFIGI[] = {
    "noc_zrodlo=json\n",
    "noc_zrodlo=json\nkabel_bez_snu=nie\n",
    "noc_zrodlo=json\nkabel_bez_snu=tak\neink.pelne_co=2\n",
    "noc_zrodlo=json\nkabel_bez_snu=NIE\n",
    "noc_zrodlo=json\neink.pelne_co=0\nkokpit.kolejnosc=k2,k1\n",
};
struct Opcje {
  Los* los = nullptr;              // losowe zdarzenia; nullptr = tylko terminy planu i `naciski`
  std::vector<std::pair<uint32_t, uint8_t>> stale;  // naciśnięcia o stałych chwilach
  std::map<uint32_t, std::string> konfigi;  // chwila stałego naciśnięcia → treść komendy `W konfiguracja.txt`
  std::string sciezkaUsterki;      // uzbrojenie usterki odczytu po pierwszej zmianie tego pliku (import)
  int krokow = 4000;
};
struct WynikSym {
  int przebiegow = 0, niezgodnych = 0, czekan = 0, glebokichB = 0, przerwanychB = 0, usterekA = 0, usterekB = 0;
  bool uzbrojono = false;
  int przebiegowPoUzbrojeniu = 0, niezgodnychPoUzbrojeniu = 0;
};

static WynikSym symuluj(Bieg& A, Bieg& B, uint32_t od, uint32_t doT, Opcje& o, const char* nazwa) {
  WynikSym ws;
  Wejscie w;
  w.ep = od;
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  bool odlaczWCzekaniu = false;
  size_t is = 0;
  std::string poprzedniPlik;
  for (int krok = 0; krok < o.krokow && w.ep < doT; krok++) {
    if (odlaczWCzekaniu) B.p.sen.wCzekaniu = [&B]() { B.p.czujniki.zas = Zasilanie::BATERIA; };
    airbridge(A, w.ep);
    airbridge(B, w.ep);
    int czek0 = B.p.sen.czekan, prz0 = B.p.sen.przerwanych;
    Wynik a = przebieg(A, w);
    Wynik b = przebieg(B, w);
    B.p.sen.wCzekaniu = nullptr;
    if (odlaczWCzekaniu) {  // kabel odłączony po przebiegu: obie ścieżki od teraz na baterii
      A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::BATERIA;
      odlaczWCzekaniu = false;
    }
    bool zg = porownaj(nazwa, w.ep, a, b);
    ws.przebiegow++;
    ws.niezgodnych += !zg;
    if (ws.uzbrojono) {
      ws.przebiegowPoUzbrojeniu++;
      ws.niezgodnychPoUzbrojeniu += !zg;
    }
    ws.czekan += B.p.sen.czekan > czek0;
    ws.glebokichB += b.spal;
    ws.przerwanychB += B.p.sen.przerwanych > prz0;
    if (gadaj())
      printf("  %s %s %-8s A klatek %d, B klatek %d (częśc. %d), B czekał %d, sen %lu s%s\n", nazwa, hhmm(w.ep).c_str(),
             w.pw == PowodWybudzenia::PRZYCISK ? "przycisk" : w.pw == PowodWybudzenia::START ? "start" : "zegar",
             a.klatek, b.klatek, b.czesciowych, (int)b.czekal, (unsigned long)a.plan.sekund, zg ? "" : "  ≠");
    if (gadaj() && ws.uzbrojono && ws.przebiegowPoUzbrojeniu <= 3)
      for (auto& l : a.wiersze)
        if (l.find("noce") != std::string::npos || l.find("2026.csv") != std::string::npos ||
            l.find("historia") != std::string::npos)
          printf("    A log: %s\n", l.c_str());
    if (!o.sciezkaUsterki.empty() && !ws.uzbrojono) {
      auto f = a.plikiMapa.find(o.sciezkaUsterki);
      std::string teraz = f == a.plikiMapa.end() ? "" : f->second;
      if (!poprzedniPlik.empty() && teraz != poprzedniPlik) {  // import zapisał plik nocy: B unieważnił pamięć
        A.u.uzbrojone.insert(o.sciezkaUsterki);
        B.u.uzbrojone.insert(o.sciezkaUsterki);
        ws.uzbrojono = true;
        if (gadaj()) printf("  usterka odczytu %s uzbrojona po %s\n", o.sciezkaUsterki.c_str(), hhmm(w.ep).c_str());
      }
      poprzedniPlik = teraz;
    }
    uint32_t koniec = w.ep + a.msPrzebiegu / 1000;
    uint32_t nast = koniec + (a.plan.budzZegarem ? (a.plan.sekund ? a.plan.sekund : 1) : OKRES_SNU_S);
    Wejscie n;
    n.ep = nast;
    // stałe naciśnięcia
    while (is < o.stale.size() && o.stale[is].first <= koniec) is++;
    if (is < o.stale.size() && o.stale[is].first <= nast && (a.plan.przyciski & o.stale[is].second)) {
      n.ep = o.stale[is].first;
      n.pw = PowodWybudzenia::PRZYCISK;
      n.budzace = o.stale[is].second;
      n.skrypt = naciski(o.stale[is].second, 200, {BIT_PRAWY});
      auto kf = o.konfigi.find(o.stale[is].first);
      if (kf != o.konfigi.end()) n.port = komendaKonfiguracji(kf->second);
      is++;
    }
    if (o.los && nast > koniec + 2) {
      Los& r = *o.los;
      int z = r.od(0, 99);
      uint32_t kiedy = koniec + 1 + r.n() % (nast - koniec - 1);
      static const uint8_t BITY[] = {BIT_LEWY, BIT_SRODEK, BIT_PRAWY};
      if (z < 53) {  // naciśnięcie (z < 45) albo komenda `W` w czuwaniu (45…52)
        uint8_t bit = BITY[r.od(0, 2)];
        if (a.plan.przyciski & bit) {
          n.ep = kiedy;
          n.pw = PowodWybudzenia::PRZYCISK;
          n.budzace = bit;
          std::vector<uint8_t> dalej;
          for (int k = r.od(0, 3); k > 0; k--) dalej.push_back(BITY[r.od(0, 2)]);
          n.skrypt = naciski(bit, r.p(10) ? 2600 : 180, dalej);
          for (int k = r.od(0, 2) * (r.p(25) ? 1 : 0); k > 0; k--)
            n.kolejka.push_back({r.od(1, 3), (uint32_t)(r.p(15) ? 2400 : 120)});
          if (z >= 45) n.port = komendaKonfiguracji(KONFIGI[r.od(0, 4)]);
        }
      } else if (z < 59) {
        A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::BATERIA;
      } else if (z < 67) {
        A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
      } else if (z < 71) {
        A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::NIEZNANE;
      } else if (z < 76) {
        odlaczWCzekaniu = true;  // dotyczy czekania po NASTĘPNYM przebiegu
      } else if (z < 80) {
        A.p.siec.siecJest = B.p.siec.siecJest = false;
      } else if (z < 86) {
        A.p.siec.siecJest = B.p.siec.siecJest = true;
      } else if (z < 89) {  // start zasilania albo restart po błędzie
        n.ep = kiedy;
        n.pw = PowodWybudzenia::START;
        int q = r.od(0, 2);
        n.prz = q == 0 ? PrzyczynaStartu::ZASILANIE : q == 1 ? PrzyczynaStartu::TASK_WDT : PrzyczynaStartu::PANIKA;
        A.p.start.stan = StanRtc();
        B.p.start.stan = StanRtc();
        if (b.czekal) B.nowyProgram();
      } else if (z < 91) {  // skok zegara (NTP poprawia RTC): następne wybudzenie „widzi” inną chwilę
        n.ep = nast + (r.p(50) ? 3600u * 5 : 0u) - (r.p(50) ? 3600u * 3 : 0u);
      }
    }
    w = n;
  }
  ws.usterekA = A.u.usterek;
  ws.usterekB = B.u.usterek;
  return ws;
}

// ---------- R1: PlikiZPamiecia a przejściowy błąd odczytu ----------
static void testUsterkaWarstwy() {
  Panel p;
  p.pliki.montowane = true;
  p.pliki.pliki["/dane/noce/2026.csv"] = "naglowek\nwiersz\n";
  PlikiUsterka u(p.pliki);
  PlikiZPamiecia pl(u);
  pl.wlacz(true);
  u.uzbrojone.insert("/dane/noce/2026.csv");
  std::string t;
  bool pierwszy = pl.czytaj("/dane/noce/2026.csv", t);  // usterka nośnika: false jak na ścieżce bez warstwy
  bool drugi = pl.czytaj("/dane/noce/2026.csv", t);     // nośnik już sprawny: bez warstwy true
  bool jest = pl.istnieje("/dane/noce/2026.csv");
  printf("R1 PlikiZPamiecia po jednym nieudanym odczycie: drugi odczyt %s, istnieje() %s (nośnik: plik jest)\n",
         drugi ? "true" : "FALSE", jest ? "true" : "FALSE");
  SPRAWDZ(!pierwszy);
  SPRAWDZ(drugi);  // dawna usterka: „brak pliku” z jednej usterki zostawał w pamięci
  SPRAWDZ(jest);
}

// ---------- R2: ta sama usterka na dyrygencie (dane nocy) ----------
static int wierszyR2b = -1;  // wiersze pliku nocy na końcu R2b (bez usterki); R2b biegnie przed R2
static void testUsterkaPlikuNocy() {
  Bieg A(false, "2026-11-08", 60, 7), B(true, "2026-11-08", 60, 7);
  Panel tmp;
  Magazyn mag(tmp.pliki, bezLogu);
  Opcje o;
  o.sciezkaUsterki = mag.sciezkaRoku("noce", "2026");
  for (int d = 9; d <= 11; d++) {
    o.stale.push_back({ep(2026, 11, d, 9, 10), BIT_PRAWY});
    o.stale.push_back({ep(2026, 11, d, 13, 40), BIT_PRAWY});
    o.stale.push_back({ep(2026, 11, d, 21, 5), BIT_SRODEK});
  }
  WynikSym ws = symuluj(A, B, ep(2026, 11, 9, 4, 50), ep(2026, 11, 11, 23, 0), o, "R2");
  auto liczWiersze = [&](Bieg& x) {
    auto i = x.p.pliki.pliki.find(o.sciezkaUsterki);
    return i == x.p.pliki.pliki.end() ? -1 : (int)std::count(i->second.begin(), i->second.end(), '\n');
  };
  printf("R2 usterka odczytu %s: uzbrojona %d, usterek A %d B %d; przebiegów po uzbrojeniu %d, niezgodnych %d; "
         "wierszy pliku nocy na końcu A %d B %d\n",
         o.sciezkaUsterki.c_str(), (int)ws.uzbrojono, ws.usterekA, ws.usterekB, ws.przebiegowPoUzbrojeniu,
         ws.niezgodnychPoUzbrojeniu, liczWiersze(A), liczWiersze(B));
  SPRAWDZ(ws.uzbrojono);
  SPRAWDZ(ws.niezgodnych == 0);
  SPRAWDZ(liczWiersze(A) == liczWiersze(B));
  // (zmiana 2026-11-22, T-WYM-19, T-TEST-515): dawniej na OBU ścieżkach jeden nieudany odczyt pliku nocy dawał
  // `historia: 0 nocy`, a import tego samego wybudzenia przepisywał plik roku samymi nowymi nocami (63 → 14 wierszy).
  // Teraz magazyn rozróżnia „plik jest, odczyt nieudany” od braku pliku: w tym wybudzeniu bez zapisu pliku nocy,
  // następne czyta całą historię, więc na końcu tyle wierszy co bez usterki (R2b, wierszyR2b).
  SPRAWDZ(wierszyR2b > 0);
  SPRAWDZ(liczWiersze(A) == wierszyR2b);
  SPRAWDZ(liczWiersze(B) == wierszyR2b);
  bool wLogu = false;
  for (auto& l : A.p.port.log) wLogu = wLogu || l.find("noce/2026.csv: odczyt nieudany") != std::string::npos;
  SPRAWDZ(wLogu);
}

// to samo bez usterki (punkt odniesienia: ile wierszy ma plik nocy bez usterki)
static void testBezUsterki() {
  Bieg A(false, "2026-11-08", 60, 7), B(true, "2026-11-08", 60, 7);
  Panel tmp;
  Magazyn mag(tmp.pliki, bezLogu);
  std::string sc = mag.sciezkaRoku("noce", "2026");
  auto wierszy = [&](Bieg& x) {
    auto i = x.p.pliki.pliki.find(sc);
    return i == x.p.pliki.pliki.end() ? -1 : (int)std::count(i->second.begin(), i->second.end(), '\n');
  };
  int start = wierszy(A);
  Opcje o;
  for (int d = 9; d <= 11; d++) {
    o.stale.push_back({ep(2026, 11, d, 9, 10), BIT_PRAWY});
    o.stale.push_back({ep(2026, 11, d, 13, 40), BIT_PRAWY});
    o.stale.push_back({ep(2026, 11, d, 21, 5), BIT_SRODEK});
  }
  WynikSym ws = symuluj(A, B, ep(2026, 11, 9, 4, 50), ep(2026, 11, 11, 23, 0), o, "R2b");
  printf("R2b bez usterki: wierszy pliku nocy na starcie %d, na końcu A %d B %d, niezgodnych %d\n", start, wierszy(A),
         wierszy(B), ws.niezgodnych);
  SPRAWDZ(ws.niezgodnych == 0);
  SPRAWDZ(wierszy(A) >= start);
  wierszyR2b = wierszy(A);
}

// R2d (T-WYM-19, T-TEST-515, dopisany 22.11.2026): trwała usterka odczytu wszystkich plików `/dane/…` i `/log/…`,
// które istnieją po dobie przed usterką (w tym scenariuszu: noce/2026, reguły, sypialnia/2026, dziennik wybudzeń;
// humor, czynniki, odpowiedzi, portret, dobry start i stan pobierania w tej dobie nie powstają, sprawdza je O1 i O3
// w test/testy_odczyt_nieudany.cpp na samym magazynie), przez całą dobę z importem nocy i naciśnięciami, na obu
// ścieżkach (A głęboki sen, B czekanie na kablu). Każdy z tych plików zostaje bajt w bajt, obie ścieżki zgodne, w logu
// wiersz „odczyt nieudany”; po naprawie nośnika następna doba dopisuje nowe noce do pełnej historii.
static void testTrwalaUsterkaWszystkichPlikow() {
  Bieg A(false, "2026-11-08", 60, 11), B(true, "2026-11-08", 60, 11);
  Opcje o;
  for (int d = 9; d <= 11; d++) {
    o.stale.push_back({ep(2026, 11, d, 9, 10), BIT_PRAWY});
    o.stale.push_back({ep(2026, 11, d, 13, 40), BIT_PRAWY});
    o.stale.push_back({ep(2026, 11, d, 21, 5), BIT_SRODEK});
  }
  // doba bez usterki: powstają dzienniki, czynniki, reguły, stan pobierania
  symuluj(A, B, ep(2026, 11, 9, 4, 50), ep(2026, 11, 9, 23, 0), o, "R2d/przed");
  auto dane = [](const std::string& s) { return s.rfind("/dane/", 0) == 0 || s.rfind("/log/", 0) == 0; };
  std::map<std::string, std::string> przedA, przedB;
  for (auto& kv : A.p.pliki.pliki)
    if (dane(kv.first)) przedA[kv.first] = kv.second;
  for (auto& kv : B.p.pliki.pliki)
    if (dane(kv.first)) przedB[kv.first] = kv.second;
  for (auto& kv : przedA) A.p.pliki.nieczytelne.insert(kv.first);
  for (auto& kv : przedB) B.p.pliki.nieczytelne.insert(kv.first);
  B.nowyProgram();  // start zasilania na początku symuluj(): RAM (także pamięć plików) od zera
  size_t log0 = A.p.port.log.size();
  WynikSym ws = symuluj(A, B, ep(2026, 11, 10, 4, 50), ep(2026, 11, 10, 23, 0), o, "R2d");
  int zmienioneA = 0, zmienioneB = 0;
  for (auto& kv : przedA) zmienioneA += A.p.pliki.pliki[kv.first] != kv.second;
  for (auto& kv : przedB) zmienioneB += B.p.pliki.pliki[kv.first] != kv.second;
  bool wLogu = false;
  for (size_t i = log0; i < A.p.port.log.size(); i++)
    wLogu = wLogu || A.p.port.log[i].find(": odczyt nieudany, plik zostaje bez zmian") != std::string::npos;
  Panel tmp;
  Magazyn mag(tmp.pliki, bezLogu);
  std::string sc = mag.sciezkaRoku("noce", "2026");
  auto wierszy = [&](Bieg& x) {
    auto i = x.p.pliki.pliki.find(sc);
    return i == x.p.pliki.pliki.end() ? -1 : (int)std::count(i->second.begin(), i->second.end(), '\n');
  };
  int nocyPrzed = (int)std::count(przedA[sc].begin(), przedA[sc].end(), '\n');
  printf("R2d trwała usterka odczytu %zu plików danych przez dobę: zmienionych A %d B %d, przebiegów %d, niezgodnych %d, "
         "wiersz w logu %d\n",
         przedA.size(), zmienioneA, zmienioneB, ws.przebiegow, ws.niezgodnych, (int)wLogu);
  std::string nazwy;
  for (auto& kv : przedA) nazwy += " " + kv.first;
  printf("  pliki z usterką:%s\n", nazwy.c_str());
  SPRAWDZ(przedA.size() >= 4);  // plik nocy i pliki zapisane w dobie przed usterką (lista wyżej w wyniku testu)
  SPRAWDZ(przedA.count(sc) == 1);
  SPRAWDZ(zmienioneA == 0);
  SPRAWDZ(zmienioneB == 0);
  SPRAWDZ(ws.niezgodnych == 0);
  SPRAWDZ(wLogu);
  // nośnik sprawny: następna doba czyta pełną historię i dopisuje nowe noce
  A.p.pliki.nieczytelne.clear();
  B.p.pliki.nieczytelne.clear();
  B.nowyProgram();
  WynikSym po = symuluj(A, B, ep(2026, 11, 11, 4, 50), ep(2026, 11, 11, 23, 0), o, "R2d/po");
  printf("R2d po naprawie: wierszy pliku nocy przed usterką %d, na końcu A %d B %d, niezgodnych %d\n", nocyPrzed,
         wierszy(A), wierszy(B), po.niezgodnych);
  SPRAWDZ(wierszy(A) > nocyPrzed);
  SPRAWDZ(wierszy(A) == wierszy(B));
  SPRAWDZ(po.niezgodnych == 0);
}

// przejściowy błąd odczytu /konfiguracja.txt tuż po jej zapisie komendą `W` (obie ścieżki czytają wtedy nośnik)
static void testUsterkaKonfiguracji() {
  Bieg A(false, "2026-11-08", 40, 9), B(true, "2026-11-08", 40, 9);
  Opcje o;
  o.sciezkaUsterki = "/konfiguracja.txt";
  for (int d = 9; d <= 10; d++) {
    o.stale.push_back({ep(2026, 11, d, 9, 10), BIT_PRAWY});
    o.stale.push_back({ep(2026, 11, d, 13, 40), BIT_PRAWY});
    o.stale.push_back({ep(2026, 11, d, 21, 5), BIT_SRODEK});
  }
  o.konfigi[ep(2026, 11, 9, 13, 40)] = "noc_zrodlo=json\neink.pelne_co=3\nbuzzer=nie\n";
  WynikSym ws = symuluj(A, B, ep(2026, 11, 9, 4, 50), ep(2026, 11, 10, 23, 0), o, "R2c");
  printf("R2c usterka odczytu /konfiguracja.txt po `W`: uzbrojona %d, usterek A %d B %d; po uzbrojeniu %d "
         "przebiegów, niezgodnych %d\n",
         (int)ws.uzbrojono, ws.usterekA, ws.usterekB, ws.przebiegowPoUzbrojeniu, ws.niezgodnychPoUzbrojeniu);
  SPRAWDZ(ws.uzbrojono);
  SPRAWDZ(ws.niezgodnych == 0);
}

// ---------- R3: wyłącznik wgrany komendą `W` w czuwaniu ----------
static void testWylacznikWTymSamymPrzebiegu() {
  Bieg B(true, "2026-11-08", 40, 3);
  airbridge(B, ep(2026, 11, 9, 12, 0));
  Wejscie w;
  w.ep = ep(2026, 11, 9, 12, 0);
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  Wynik s = przebieg(B, w);
  SPRAWDZ(s.czekal);
  Wejscie p;
  p.ep = w.ep + 600;
  p.pw = PowodWybudzenia::PRZYCISK;
  p.budzace = BIT_PRAWY;
  p.skrypt = naciski(BIT_PRAWY, 150, {});
  p.port = komendaKonfiguracji("noc_zrodlo=json\nkabel_bez_snu=nie\n");
  airbridge(B, p.ep);
  Wynik b = przebieg(B, p);
  bool zapisany = B.p.pliki.pliki["/konfiguracja.txt"].find("kabel_bez_snu=nie") != std::string::npos;
  printf("R3 `W kabel_bez_snu=nie` w czuwaniu: plik zapisany %d, ten sam przebieg kończy się %s\n", (int)zapisany,
         b.czekal ? "CZEKANIEM (wyłącznik działa dopiero od następnego przebiegu)" : "głębokim snem");
  SPRAWDZ(zapisany);
  // informacyjnie: README firmware mówi „działa od następnego snu”, a ten przebieg jeszcze czeka
  // (`ustawieniaB9_` przeczytane przed komendą `W`); czekanie kończy się terminem albo przyciskiem, potem głęboki sen
  Wejscie n;
  n.ep = p.ep + b.msPrzebiegu / 1000 + b.plan.sekund;
  airbridge(B, n.ep);
  Wynik c = przebieg(B, n);
  SPRAWDZ(!c.czekal);
}

// ---------- R4: losowe sekwencje (własność A ≡ B) ----------
static void testLosowe() {
  int seeds = getenv("BRZEG_ZIARNA") ? atoi(getenv("BRZEG_ZIARNA")) : 8;
  int lacznie = 0, niezg = 0, czekan = 0, glebokich = 0, przerw = 0;
  for (int z = 1; z <= seeds; z++) {
    Los r((uint64_t)z * 7919u);
    Bieg A(false, "2026-11-08", 45, (uint64_t)z), B(true, "2026-11-08", 45, (uint64_t)z);
    Opcje o;
    o.los = &r;
    char nazwa[16];
    snprintf(nazwa, sizeof nazwa, "R4/%d", z);
    WynikSym ws = symuluj(A, B, ep(2026, 11, 9, 4, 50), ep(2026, 11, 11, 4, 50), o, nazwa);
    lacznie += ws.przebiegow;
    niezg += ws.niezgodnych;
    czekan += ws.czekan;
    glebokich += ws.glebokichB;
    przerw += ws.przerwanychB;
    if (ws.niezgodnych) printf("  ziarno %d: %d niezgodnych z %d przebiegów\n", z, ws.niezgodnych, ws.przebiegow);
  }
  printf("R4 losowe: %d ziaren, %d przebiegów, niezgodnych %d, B czekań %d, głębokich %d, przerwanych %d\n", seeds,
         lacznie, niezg, czekan, glebokich, przerw);
  SPRAWDZ(czekan > 0);
  SPRAWDZ(przerw > 0);
  SPRAWDZ(niezg == 0);
}

// ---------- R5: blokada po błędzie i zegar cofnięty (czysta funkcja) ----------
static void testBlokada() {
  using czekanie::Blokada;
  SPRAWDZ(czekanie::blokada("2026-11-16", "2026-11-17") == Blokada::KONIEC);
  SPRAWDZ(czekanie::blokada("2026-11-16", "2026-11-16") == Blokada::TRWA);
  SPRAWDZ(czekanie::blokada("?", "") == Blokada::TRWA);
  SPRAWDZ(czekanie::blokada("?", "2026-11-16") == Blokada::USTAL_DZIS);
  // zegar był błędnie w przyszłości w chwili restartu: blokada trwa aż do tej daty (tu rok) — informacyjnie
  bool dlugo = czekanie::blokada("2027-11-16", "2026-11-17") == Blokada::TRWA;
  printf("R5 blokada zapisana z datą z przyszłości trwa do tej daty: %s\n", dlugo ? "tak" : "nie");
  SPRAWDZ(czekanie::startPoBledzie(PrzyczynaStartu::PANIKA));
  SPRAWDZ(!czekanie::startPoBledzie(PrzyczynaStartu::SPADEK));
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  printf("testy czekania na kablu (ADR-0047)\n");
  testUsterkaWarstwy();
  testBlokada();
  testWylacznikWTymSamymPrzebiegu();
  testBezUsterki();
  testUsterkaPlikuNocy();
  testTrwalaUsterkaWszystkichPlikow();
  testUsterkaKonfiguracji();
  testLosowe();
  printf("testy_kabel_bez_snu_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
