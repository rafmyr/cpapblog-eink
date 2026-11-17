// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe limitu ładowania ADR-0046 razem z czekaniem na kablu ADR-0047 (17.11.2026). Uprząż panelu (Bieg, przebieg, porównanie) skopiowana z
// test/testy_kabel_bez_snu_brzegowe_2.cpp (tam statyczna), bez usterek nośnika. Testy własne:
//  T1. Losowe sekwencje na dwóch ścieżkach (A głęboki sen, B czekanie z PlikiZPamiecia): opcja `ladowanie_limit`
//      przełączana komendą `W`, odłączenia kabla (także w czekaniu), reset rejestrów układu (zanik BUS i BAT, REG_RST,
//      upływ watchdoga w trybie host), błędy I2C, nieudany zapis, błąd odczytu NVS `lad_zast`, restarty po strażniku
//      i zaniku zasilania, trzy doby. Niezmienniki po każdym przebiegu: identyczne rejestry, tryb host i lista zapisów
//      w obu ścieżkach; log i NVS zgodne; żaden zapis VREG powyżej 4208 mV; przy opcji wyłączonej od początku i nigdy
//      niewłączonej zero zapisów do układu (także przy limicie dawnej wersji bez wpisu w NVS).
//  T2. Wyłączenie opcji komendą `W` na ścieżce czekania: następny przebieg (konfiguracja z pamięci plików) przywraca
//      VREG 4208 mV i usuwa wpis.
//  T5. Chwilowo nieudany odczyt /konfiguracja.txt przy włączonym limicie (głęboki sen): limit nie jest zdejmowany
//      (od 17.11.2026 test blokujący: REG04 bez zmian i 0 zapisów do układu w przebiegu z nieudanym odczytem).
//  T6. (dopisany przy poprawce, 17.11.2026) nieudany odczyt /konfiguracja.txt na ścieżce czekania: `kabel_bez_snu`
//      nieznane, głęboki sen w tym przebiegu także przy `nie` i przy domyślnym `tak`.
//  T3. Formularz U-03 (formularz::scal) zachowuje `kabel_bez_snu`, wiersz wygaszacza i limitu.
// Uruchomienie: tools/prototyp/testy.sh (program testy_kabel_ladowanie_brzegowe). KL_ZIARNA=n: liczba ziaren T1 na wariant.
#include <algorithm>
#include <cstring>
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
#include "formularz.h"
#include "ladowarka.h"
#include "sekrety.h"

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


// ---------- nośnik z usterkami i lustrem ----------
enum : int { U_CZYTAJ = 1, U_ISTNIEJE = 2, U_LISTUJ = 4, U_PIERWSZY = 8, U_ZAPIS = 16 };
struct Klucz {
  std::string op, nazwa;
  int nr;
  bool operator<(const Klucz& o) const {
    if (op != o.op) return op < o.op;
    if (nazwa != o.nazwa) return nazwa < o.nazwa;
    return nr < o.nr;
  }
};
// Wspólne dla A i B: B (pierwszy w przebiegu) losuje usterkę przy odczycie z nośnika i zapisuje klucz (operacja,
// ścieżka, numer wywołania logicznego w przebiegu); A dostaje tę samą usterkę przy tym samym wywołaniu logicznym.
struct Usterki {
  int maska = 0, promili = 0;
  Los* los = nullptr;
  std::set<Klucz> przebieg;
  int wB = 0, wA = 0;
  std::map<int, int> rodzajeB;
  std::string uzbrojOp, uzbrojNazwa;  // jednorazowa usterka przy najbliższym dostępie B do nośnika
};
static int typOp(const std::string& op) {
  if (op == "czytaj" || op == "wiersze") return U_CZYTAJ;
  if (op == "istnieje") return U_ISTNIEJE;
  if (op == "listuj") return U_LISTUJ;
  if (op == "pierwszy") return U_PIERWSZY;
  return U_ZAPIS;
}
// Wywołania logiczne (to, co widzi dyrygent): B liczy nad PlikiZPamiecia, A w samym nośniku.
struct Logika {
  std::map<std::string, int> n;
  int nast(const std::string& op, const char* nazwa) { return ++n[op + "|" + nazwa]; }
  int teraz(const std::string& op, const char* nazwa) { return n[op + "|" + nazwa]; }
};
class Nosnik : public Pliki {
 public:
  Nosnik(AtrapaPlikow& p, Usterki& u, Logika& l, bool b) : p_(p), u_(u), l_(l), b_(b) {}
  bool zamontuj() override { return p_.zamontuj(); }
  bool zamontowane() override { return p_.zamontowane(); }
  bool istnieje(const char* n) override {
    if (usterka("istnieje", n)) return false;
    return p_.istnieje(n);
  }
  bool czytajWiersze(const char* n, std::vector<std::string>& w) override {
    if (usterka("wiersze", n)) { w.clear(); return false; }
    return p_.czytajWiersze(n, w);
  }
  bool zapiszAtomowo(const char* n, const std::string& t) override {
    if (usterka("zapis", n)) return false;
    return p_.zapiszAtomowo(n, t);
  }
  bool dopiszWiersz(const char* n, const std::string& w) override {
    if (usterka("dopisz", n)) return false;
    return p_.dopiszWiersz(n, w);
  }
  bool pierwszyWiersz(const char* n, std::string& w) override {
    bool u = usterka("pierwszy", n);
    bool r = p_.pierwszyWiersz(n, w);
    if (u && r && w.size() > 1) w.resize(w.size() / 2);  // obcięty String (readStringUntil bez pamięci)
    return r;
  }
  bool usun(const char* n) override {
    if (usterka("usun", n)) return false;
    return p_.usun(n);
  }
  bool zmienNazwe(const char* z, const char* na) override {
    if (usterka("nazwa", z)) return false;
    return p_.zmienNazwe(z, na);
  }
  void odmontuj() override { p_.odmontuj(); }
  bool czytaj(const char* n, std::string& t) override {
    if (usterka("czytaj", n)) { t.clear(); return false; }
    return p_.czytaj(n, t);
  }
  size_t zapisz(const char* n, const std::string& t) override {
    if (usterka("zapisz", n)) return 0;
    return p_.zapisz(n, t);
  }
  bool listuj(const char* k, std::vector<std::string>& nazwy) override {
    bool u = usterka("listuj", k);
    bool r = p_.listuj(k, nazwy);
    if (u && r && !nazwy.empty()) nazwy.pop_back();  // openNextFile() == pusty File: pętla kończy się wcześniej
    return r;
  }
  OpisPartycji partycja() override { return p_.partycja(); }
  bool czytajPartycje(size_t ile, std::string& b) override { return p_.czytajPartycje(ile, b); }
  bool formatuj() override { return p_.formatuj(); }
  long long wolneMiejsce() override { return p_.wolneMiejsce(); }

 private:
  AtrapaPlikow& p_;
  Usterki& u_;
  Logika& l_;
  bool b_;
  bool usterka(const char* op, const char* n) {
    if (!p_.zamontowane()) return false;
    int nr = b_ ? l_.teraz(op, n) : l_.nast(op, n);
    Klucz k{op, n, nr};
    if (b_ && !u_.uzbrojOp.empty() && u_.uzbrojOp == op && u_.uzbrojNazwa == n) {
      u_.uzbrojOp.clear();
      u_.przebieg.insert(k);
      u_.wB++;
      return true;
    }
    if (b_) {
      if (!(u_.maska & typOp(op)) || !u_.los || (int)(u_.los->n() % 1000) >= u_.promili) return false;
      u_.przebieg.insert(k);
      if (getenv("BRZEG2_USTERKI")) printf("    usterka B %s %s #%d\n", op, n, nr);
      u_.wB++;
      u_.rodzajeB[typOp(op)]++;
      return true;
    }
    if (!u_.przebieg.count(k)) return false;
    u_.wA++;
    return true;
  }
};
// B: liczy wywołania logiczne nad PlikiZPamiecia.
class Licznik : public Pliki {
 public:
  Licznik(Pliki& p, Logika& l) : p_(p), l_(l) {}
  bool zamontuj() override { return p_.zamontuj(); }
  bool zamontowane() override { return p_.zamontowane(); }
  bool istnieje(const char* n) override { l_.nast("istnieje", n); return p_.istnieje(n); }
  bool czytajWiersze(const char* n, std::vector<std::string>& w) override { l_.nast("wiersze", n); return p_.czytajWiersze(n, w); }
  bool zapiszAtomowo(const char* n, const std::string& t) override { l_.nast("zapis", n); return p_.zapiszAtomowo(n, t); }
  bool dopiszWiersz(const char* n, const std::string& w) override { l_.nast("dopisz", n); return p_.dopiszWiersz(n, w); }
  bool pierwszyWiersz(const char* n, std::string& w) override { l_.nast("pierwszy", n); return p_.pierwszyWiersz(n, w); }
  bool usun(const char* n) override { l_.nast("usun", n); return p_.usun(n); }
  bool zmienNazwe(const char* z, const char* na) override { l_.nast("nazwa", z); return p_.zmienNazwe(z, na); }
  void odmontuj() override { p_.odmontuj(); }
  bool czytaj(const char* n, std::string& t) override { l_.nast("czytaj", n); return p_.czytaj(n, t); }
  size_t zapisz(const char* n, const std::string& t) override { l_.nast("zapisz", n); return p_.zapisz(n, t); }
  bool listuj(const char* k, std::vector<std::string>& nazwy) override { l_.nast("listuj", k); return p_.listuj(k, nazwy); }
  OpisPartycji partycja() override { return p_.partycja(); }
  bool czytajPartycje(size_t ile, std::string& b) override { return p_.czytajPartycje(ile, b); }
  bool formatuj() override { return p_.formatuj(); }
  long long wolneMiejsce() override { return p_.wolneMiejsce(); }

 private:
  Pliki& p_;
  Logika& l_;
};

// ---------- jeden panel ----------
static KonfiguracjaPanelu konfZPogoda() {
  KonfiguracjaPanelu k = KONF;
  k.pogodaLat = 50.2597;  // jak test I autora: współrzędne, żeby slot dzienny (obrazek, wygaszacz) się pojawiał
  k.pogodaLon = 19.0217;
  return k;
}
static std::string pbm(int w, int h) {
  return "P4\n" + std::to_string(w) + " " + std::to_string(h) + "\n" + std::string((size_t)((w + 7) / 8 * h), '\x0F');
}
struct Bieg {
  Panel p;
  const bool czekanie;
  Logika lg;
  Nosnik n;
  PlikiZPamiecia pl;
  Licznik lc;
  PamiecPrzebiegow pp;
  KonfiguracjaPanelu konf = konfZPogoda();
  Bieg(bool c, Usterki& u, const std::string& ostatnia, int nocy, uint64_t ziarno, const std::string& konfTxt)
      : czekanie(c), n(p.pliki, u, lg, c), pl(n), lc(pl, lg) {
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
    p.pliki.pliki["/konfiguracja.txt"] = konfTxt;
    p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
    p.pliki.pliki["/obrazki/obrazki.json"] = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\"}]}";
    p.pliki.montowane = false;
    p.d.z.clear();
    p.czujniki.zas = Zasilanie::ZEWNETRZNE;
    p.sen.czekanieWlaczone = c;
    p.ekran.pamietaObraz = c;
  }
  Adaptery ad() {
    Pliki& f = czekanie ? static_cast<Pliki&>(lc) : static_cast<Pliki&>(n);
    return Adaptery{p.zegar, p.nvs, f, p.siec, p.ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik,
                    p.sen, p.port, p.diag, &p.wydania};
  }
  void nowyProgram() {
    pp = PamiecPrzebiegow();
    pl.wlacz(false);
    p.ekran.zapomnij();
  }
};

struct Wejscie {
  uint32_t ep = 0;
  PowodWybudzenia pw = PowodWybudzenia::ZEGAR;
  PrzyczynaStartu prz = PrzyczynaStartu::USPIENIE;
  uint8_t budzace = 0;
  uint32_t puszczonyPoMs = 0;  // B: naciśnięcie budzące już puszczone w czekaniu (z kolejki adaptera)
  std::vector<std::pair<uint32_t, uint8_t>> skrypt;   // A (i B, gdy puszczonyPoMs == 0)
  std::vector<std::pair<uint32_t, uint8_t>> skryptB;  // B, gdy puszczonyPoMs > 0
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
  int klatek = 0, czesciowych = 0, czesciowychDnia = 0;
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
  bool pusz = b.czekanie && w.puszczonyPoMs > 0;
  p.start.trwanieBudzacego = pusz ? w.puszczonyPoMs : 0;
  p.przyciski.skrypt = pusz ? w.skryptB : w.skrypt;
  p.przyciski.kolejka = w.kolejka;
  p.siec.polaczonaTeraz = false;
  b.lg.n.clear();
  for (char c : w.port) p.port.znaki.push_back((unsigned char)c);
  size_t log0 = p.port.log.size(), kl0 = p.ekran.klatki.size();
  int snow0 = p.sen.snow, czek0 = p.sen.czekan;
  bool czekano = false;
  if (b.czekanie) czekano = Dyrygent(b.ad(), b.konf, &b.pp).wybudzenie();
  else Dyrygent(b.ad(), b.konf).wybudzenie();
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
    const std::string& l = p.port.log[i];
    if (l.rfind("ekran 0 narysowany", 0) == 0 && l.find("odświeżenie pełne") == std::string::npos) r.czesciowychDnia++;
    if (wierszCzekania(l)) continue;
    r.wiersze.push_back(bezRodzaju(l));
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
static bool porownaj(const char* co, uint32_t t, const Wynik& a, const Wynik& b) {
  bool zgodne = a.klatki == b.klatki && a.klatek == b.klatek && a.stan == b.stan && a.pliki == b.pliki &&
                a.log == b.log && a.plan.sekund == b.plan.sekund && a.plan.przyciski == b.plan.przyciski &&
                a.plan.budzZegarem == b.plan.budzZegarem && a.spal == (b.spal || b.czekal) &&
                b.czesciowych >= a.czesciowych && b.czesciowychDnia == 0;
  if (zgodne) return true;
  if (++pokazanych > 10) return false;
  printf("  NIEZGODNOŚĆ %s %s: klatki %d/%d (%s), stan %s, pliki %s, log %s, plan %lu/%lu s, sen %d/%d czekanie %d\n",
         co, hhmm(t).c_str(), a.klatek, b.klatek, a.klatki == b.klatki ? "=" : "≠", a.stan == b.stan ? "=" : "≠",
         a.pliki == b.pliki ? "=" : "≠", a.log == b.log ? "=" : "≠", (unsigned long)a.plan.sekund,
         (unsigned long)b.plan.sekund, (int)a.spal, (int)b.spal, (int)b.czekal);
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
                                                         const std::vector<uint8_t>& kolejne, uint32_t od = 2500) {
  std::vector<std::pair<uint32_t, uint8_t>> s = {{0, budzacy}, {trzymaj, 0}};
  uint32_t t = trzymaj + od;
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

// =====================================================================================================================
// Część własna: limit ładowania ADR-0046 × czekanie na kablu ADR-0047
// =====================================================================================================================

// Układ ładowarki z usterkami sterowanymi przez test (lustro A/B) i strażnikiem granicy VREG przy każdym zapisie.
class LadowarkaBrzegowa : public AtrapaLadowarki {
 public:
  bool zlyZapisWPrzebiegu = false;  // pierwszy zapis w tym przebiegu się nie udaje (bez zmiany rejestru)
  int zapisowPrzebiegu = 0;
  int naruszenVreg = 0;             // zapis REG04 z kodem VREG > 01011 (4208 mV)
  bool zapisz(uint8_t rej, uint8_t w) override {
    if (rej == 0x04 && ((w & 0xF8) >> 3) > 11) naruszenVreg++;
    if (odpowiada && rej <= 0x0B && zlyZapisWPrzebiegu && zapisowPrzebiegu++ == 0) {
      zapisy.push_back({rej, w});
      return false;
    }
    return AtrapaLadowarki::zapisz(rej, w);
  }
};

static const char* const KONFIGI_L[] = {
    "noc_zrodlo=json\nladowanie_limit=tak\n",
    "noc_zrodlo=json\n",
    "noc_zrodlo=json\nladowanie_limit=nie\n",
    "noc_zrodlo=json\nladowanie_limit=tak\nkabel_bez_snu=nie\n",
    "noc_zrodlo=json\nladowanie_limit=TAK\n",                          // wielkość liter: wyłączona
    "noc_zrodlo=json\nladowanie_limit=tak\nladowanie_limit=nie\n",     // ostatni wiersz wygrywa: wyłączona
    "noc_zrodlo=json\nladowanie_limit=nie\nladowanie_limit=tak\r\n",   // ostatni wiersz wygrywa: włączona
    "noc_zrodlo=json\nkabel_bez_snu=nie\n",
    "noc_zrodlo=json\nladowanie_limit=tak\ndzien.wygaszacz=tak\ndzien.wygaszacz_min=30\n",
};
static const int KONFIGOW_L = (int)(sizeof KONFIGI_L / sizeof KONFIGI_L[0]);
static bool wlaczaLimit(const char* k) { return ladowarka::limitWKonfiguracji(k); }

struct StatL {
  int przebiegow = 0, niezgodnychPrzebiegu = 0, niezgodnychUkladu = 0, czekan = 0, przerwanych = 0, resetow = 0,
      bledowI2c = 0, bledowZapisu = 0, bledowNvs = 0, restartow = 0, komend = 0, odlaczen = 0;
};

// Losowe sekwencje na obu ścieżkach: A głęboki sen, B czekanie (PlikiZPamiecia). Zdarzenia układu i NVS lustrzane.
static StatL symulujL(Bieg& A, Bieg& B, LadowarkaBrzegowa& la, LadowarkaBrzegowa& lb, uint32_t od, uint32_t doT, Los& r,
                      bool tylkoWylaczona, const char* nazwa) {
  StatL st;
  Usterki u;  // bez usterek nośnika (te sprawdza testy_kabel_bez_snu_brzegowe_2.cpp)
  Wejscie w;
  w.ep = od;
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  bool odlaczWCzekaniu = false;
  static const uint8_t BITY[] = {BIT_LEWY, BIT_SRODEK, BIT_PRAWY};
  for (int krok = 0; krok < 4000 && w.ep < doT; krok++) {
    // zdarzenia układu i NVS na ten przebieg (lustro)
    bool i2c = r.p(6), zlyZapis = r.p(8), nvsBlad = r.p(5);
    la.odpowiada = lb.odpowiada = !i2c;
    la.zlyZapisWPrzebiegu = lb.zlyZapisWPrzebiegu = zlyZapis;
    la.zapisowPrzebiegu = lb.zapisowPrzebiegu = 0;
    if (nvsBlad) {
      A.p.nvs.bledneOdczytyU.insert("cpap/lad_zast");
      B.p.nvs.bledneOdczytyU.insert("cpap/lad_zast");
    }
    st.bledowI2c += i2c;
    st.bledowZapisu += zlyZapis;
    st.bledowNvs += nvsBlad;
    if (odlaczWCzekaniu) B.p.sen.wCzekaniu = [&B]() { B.p.czujniki.zas = Zasilanie::BATERIA; };
    airbridge(A, w.ep);
    airbridge(B, w.ep);
    int czek0 = B.p.sen.czekan, prz0 = B.p.sen.przerwanych;
    u.przebieg.clear();
    size_t zA0 = la.zapisy.size();
    Wynik b = przebieg(B, w);
    // B zrobił już odczyty co 10 s w czekaniu; i2c w czasie czekania nie zmienia rejestrów, więc A widzi ten sam stan
    Wynik a = przebieg(A, w);
    (void)zA0;
    B.p.sen.wCzekaniu = nullptr;
    la.odpowiada = lb.odpowiada = true;
    la.zlyZapisWPrzebiegu = lb.zlyZapisWPrzebiegu = false;
    A.p.nvs.bledneOdczytyU.erase("cpap/lad_zast");
    B.p.nvs.bledneOdczytyU.erase("cpap/lad_zast");
    if (odlaczWCzekaniu) {
      A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::BATERIA;
      odlaczWCzekaniu = false;
    }
    st.przebiegow++;
    if (!porownaj(nazwa, w.ep, a, b)) st.niezgodnychPrzebiegu++;
    bool uk = memcmp(la.r, lb.r, sizeof la.r) == 0 && la.host == lb.host && la.zapisy == lb.zapisy;
    if (!uk) {
      if (++st.niezgodnychUkladu <= 3)
        printf("  NIEZGODNY UKŁAD %s %s: REG04 %02X/%02X REG05 %02X/%02X zapisów %zu/%zu\n", nazwa, hhmm(w.ep).c_str(),
               la.r[4], lb.r[4], la.r[5], lb.r[5], la.zapisy.size(), lb.zapisy.size());
      // wyrównanie, żeby dalsze kroki nie liczyły jednej rozbieżności wiele razy
      memcpy(lb.r, la.r, sizeof la.r);
      lb.host = la.host;
      lb.zapisy = la.zapisy;
    }
    st.czekan += B.p.sen.czekan > czek0;
    st.przerwanych += B.p.sen.przerwanych > prz0;
    // między przebiegami: upływ licznika watchdoga w trybie host (minuty), czasem reset układu
    la.uplywWatchdoga();
    lb.uplywWatchdoga();
    if (r.p(4)) {
      la.startZasilania();
      lb.startZasilania();
      st.resetow++;
    }
    uint32_t koniec = w.ep + a.msPrzebiegu / 1000;
    uint32_t nast = koniec + (a.plan.budzZegarem ? (a.plan.sekund ? a.plan.sekund : 1) : OKRES_SNU_S);
    Wejscie n;
    n.ep = nast;
    if (nast > koniec + 2) {
      int z = r.od(0, 99);
      uint32_t kiedy = koniec + 1 + r.n() % (nast - koniec - 1);
      if (z < 40) {
        uint8_t bit = BITY[r.od(0, 2)];
        if (a.plan.przyciski & bit) {
          n.ep = kiedy;
          n.pw = PowodWybudzenia::PRZYCISK;
          n.budzace = bit;
          std::vector<uint8_t> dalej;
          for (int k = r.od(0, 2); k > 0; k--) dalej.push_back(BITY[r.od(0, 2)]);
          static const uint32_t TRZ[] = {150, 1700, 2600};
          n.skrypt = naciski(bit, TRZ[r.od(0, 2)], dalej);
          if (r.p(60)) {
            const char* k = KONFIGI_L[r.od(0, KONFIGOW_L - 1)];
            if (tylkoWylaczona && wlaczaLimit(k)) k = "noc_zrodlo=json\nladowanie_limit=nie\n";
            n.port = komendaKonfiguracji(k);
            st.komend++;
          }
        }
      } else if (z < 50) {
        A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::BATERIA;
        st.odlaczen++;
      } else if (z < 65) {
        A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
      } else if (z < 68) {
        A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::NIEZNANE;
      } else if (z < 76) {
        odlaczWCzekaniu = true;
        st.odlaczen++;
      } else if (z < 80) {
        n.ep = kiedy;
        n.pw = PowodWybudzenia::START;
        int q = r.od(0, 2);
        n.prz = q == 0 ? PrzyczynaStartu::ZASILANIE : q == 1 ? PrzyczynaStartu::TASK_WDT : PrzyczynaStartu::PANIKA;
        A.p.start.stan = StanRtc();
        B.p.start.stan = StanRtc();
        if (b.czekal) B.nowyProgram();
        st.restartow++;
        if (q == 0) {  // zanik zasilania całego panelu (bateria odłączona): rejestry układu też od nowa
          la.startZasilania();
          lb.startZasilania();
        }
      }
    }
    w = n;
  }
  return st;
}

// losowe sekwencje, dwie ścieżki, niezmienniki limitu ładowania
static void testLimitLosowo() {
  int ziaren = getenv("KL_ZIARNA") ? atoi(getenv("KL_ZIARNA")) : 2;
  struct Wariant {
    const char* nazwa;
    const char* konfStart;
    int stanUkladu;  // 0 po starcie zasilania, 1 stan zastany (REG05=8F host), 2 limit dawnej wersji bez wpisu (VREG 3920)
    bool tylkoWylaczona;
  };
  const Wariant warianty[] = {
      {"wyłączona zawsze, stan po starcie", "noc_zrodlo=json\n", 0, true},
      {"wyłączona zawsze, stan zastany", "noc_zrodlo=json\n", 1, true},
      {"wyłączona zawsze, VREG 3920 bez wpisu", "noc_zrodlo=json\n", 2, true},
      {"włączona, przełączana, stan po starcie", "noc_zrodlo=json\nladowanie_limit=tak\n", 0, false},
      {"włączona, przełączana, stan zastany", "noc_zrodlo=json\nladowanie_limit=tak\n", 1, false},
      {"wyłączona, przełączana, VREG 3920 bez wpisu", "noc_zrodlo=json\n", 2, false},
  };
  for (const Wariant& v : warianty) {
    StatL suma;
    int naruszen = 0, zapisowWyl = 0, ladowanieLogB = 0;
    for (int z = 1; z <= ziaren; z++) {
      Usterki u;
      Bieg A(false, u, "2026-11-16", 20, 900 + (uint64_t)z, v.konfStart);
      Bieg B(true, u, "2026-11-16", 20, 900 + (uint64_t)z, v.konfStart);
      LadowarkaBrzegowa la, lb;
      for (LadowarkaBrzegowa* l : {&la, &lb}) {
        if (v.stanUkladu == 1) {
          l->r[5] = 0x8F;
          l->r[6] = 0x62;
          l->host = true;
        } else if (v.stanUkladu == 2) {
          l->r[4] = 0x10;  // 3920 mV
          l->r[5] = 0x8F;
          l->host = true;
        }
      }
      A.p.czujniki.lad = &la;
      B.p.czujniki.lad = &lb;
      A.p.czujniki.zasilanieZUkladu = B.p.czujniki.zasilanieZUkladu = true;
      B.p.sen.sprawdzen = 6;
      Los r(7000 + (uint64_t)z * 31 + (uint64_t)v.stanUkladu);
      StatL st = symulujL(A, B, la, lb, ep(2026, 11, 17, 4, 50), ep(2026, 11, 20, 4, 50), r, v.tylkoWylaczona, v.nazwa);
      suma.przebiegow += st.przebiegow;
      suma.niezgodnychPrzebiegu += st.niezgodnychPrzebiegu;
      suma.niezgodnychUkladu += st.niezgodnychUkladu;
      suma.czekan += st.czekan;
      suma.przerwanych += st.przerwanych;
      suma.resetow += st.resetow;
      suma.bledowI2c += st.bledowI2c;
      suma.bledowZapisu += st.bledowZapisu;
      suma.bledowNvs += st.bledowNvs;
      suma.restartow += st.restartow;
      suma.komend += st.komend;
      suma.odlaczen += st.odlaczen;
      naruszen += la.naruszenVreg + lb.naruszenVreg;
      if (v.tylkoWylaczona) zapisowWyl += (int)(la.zapisy.size() + lb.zapisy.size());
      for (auto& l : B.p.port.log) ladowanieLogB += l.rfind("ladowanie: ", 0) == 0;
    }
    printf("T1 %s: %d przebiegów (%d po czekaniu, %d przerwanych), resetów układu %d, błędów I2C %d, nieudanych zapisów"
           " %d, błędów NVS %d, restartów %d, komend W %d, odłączeń %d; niezgodnych przebiegów %d, niezgodności układu"
           " %d, naruszeń VREG %d, zapisów przy opcji zawsze wyłączonej %d, wierszy `ladowanie:` B %d\n",
           v.nazwa, suma.przebiegow, suma.czekan, suma.przerwanych, suma.resetow, suma.bledowI2c, suma.bledowZapisu,
           suma.bledowNvs, suma.restartow, suma.komend, suma.odlaczen, suma.niezgodnychPrzebiegu,
           suma.niezgodnychUkladu, naruszen, zapisowWyl, ladowanieLogB);
    SPRAWDZ(suma.niezgodnychUkladu == 0);
    SPRAWDZ(suma.niezgodnychPrzebiegu == 0);
    SPRAWDZ(naruszen == 0);
    SPRAWDZ(suma.czekan > suma.przebiegow / 5);  // dużo odłączeń kabla i `kabel_bez_snu=nie` w losowaniu
    if (v.tylkoWylaczona) SPRAWDZ(zapisowWyl == 0);
  }
}

// przywrócenie po wyłączeniu opcji na ścieżce czekania, krok po kroku (bez usterek): włączona → limit;
// komenda W wyłącza → następny przebieg po czekaniu przywraca VREG 4208 mV i usuwa wpis.
static void testPrzywrocenieWCzekaniu() {
  Usterki u;
  Bieg B(true, u, "2026-11-16", 10, 5, "noc_zrodlo=json\nladowanie_limit=tak\n");
  LadowarkaBrzegowa lb;
  B.p.czujniki.lad = &lb;
  Wejscie w;
  w.ep = ep(2026, 11, 17, 10, 0);
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  airbridge(B, w.ep);
  Wynik r1 = przebieg(B, w);
  SPRAWDZ(r1.czekal);
  SPRAWDZ(((lb.r[4] & 0xF8) >> 3) == 2);
  // komenda W z wyłączeniem w następnym przebiegu (po czekaniu, przycisk)
  Wejscie w2;
  w2.ep = w.ep + 600;
  w2.pw = PowodWybudzenia::PRZYCISK;
  w2.budzace = BIT_PRAWY;
  w2.skrypt = naciski(BIT_PRAWY, 150, {});
  w2.port = komendaKonfiguracji("noc_zrodlo=json\n");
  airbridge(B, w2.ep);
  Wynik r2 = przebieg(B, w2);
  // trzeci przebieg po czekaniu: konfiguracja czytana z pamięci plików musi już być nowa
  Wejscie w3;
  w3.ep = w2.ep + r2.plan.sekund + 5;
  airbridge(B, w3.ep);
  Wynik r3 = przebieg(B, w3);
  (void)r3;
  printf("T2 po wyłączeniu komendą W (czekanie): REG04 %02X, wpis %s, zapisów %zu\n", lb.r[4],
         B.p.nvs.liczbyU.count("cpap/lad_zast") ? "jest" : "brak", lb.zapisy.size());
  SPRAWDZ(((lb.r[4] & 0xF8) >> 3) == 11);
  SPRAWDZ(!B.p.nvs.liczbyU.count("cpap/lad_zast"));
}

// formularz U-03 (scal) zachowuje kabel_bez_snu, wiersze wygaszacza i limitu, gdy ich pól nie zmieniono.
static void testFormularz() {
  const std::string stara = "noc_zrodlo=json\nkabel_bez_snu=nie\ndzien.wygaszacz=nie\nladowanie_limit=tak\nmiasto=Katowice\n";
  std::map<std::string, std::string> pola = {{"humor", "oszczedny"}, {"ladowanie_limit", "tak"}, {"wygaszacz", "nie"}};
  formularz::Wynik w = formularz::waliduj(pola, sekrety::Dane(), "2026-11-17");
  std::string nowa = formularz::scal(stara, w.plik);
  SPRAWDZ(nowa.find("kabel_bez_snu=nie\n") != std::string::npos);
  SPRAWDZ(ladowarka::limitWKonfiguracji(nowa));
  SPRAWDZ(nowa.find("dzien.wygaszacz=nie\n") != std::string::npos);
  // wyłączenie limitu formularzem: wiersz znika, kabel_bez_snu zostaje
  pola["ladowanie_limit"] = "nie";
  w = formularz::waliduj(pola, sekrety::Dane(), "2026-11-17");
  nowa = formularz::scal(stara, w.plik);
  SPRAWDZ(!ladowarka::limitWKonfiguracji(nowa) && nowa.find("ladowanie_limit") == std::string::npos);
  SPRAWDZ(nowa.find("kabel_bez_snu=nie\n") != std::string::npos);
  printf("T3 formularz: po zapisie\n%s", nowa.c_str());
}


// chwilowo nieudany odczyt /konfiguracja.txt przy włączonym limicie (ścieżka głębokiego snu, bez pamięci plików).
// Do 17.11.2026 Dyrygent::konfiguracjaPliku() zwracał wtedy pusty tekst, więc limitLadowaniaTeraz() widział opcję jako
// wyłączoną; teraz nieudany odczyt istniejącego pliku = ustawienie nieznane, bez zmian w układzie i NVS.
class ZlaKonfiguracja : public Pliki {
 public:
  explicit ZlaKonfiguracja(Pliki& p) : p_(p) {}
  bool zly = false;
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
    if (zly && std::string(n) == "/konfiguracja.txt") { t.clear(); return false; }
    return p_.czytaj(n, t);
  }
  size_t zapisz(const char* n, const std::string& t) override { return p_.zapisz(n, t); }
  bool listuj(const char* k, std::vector<std::string>& nazwy) override { return p_.listuj(k, nazwy); }
  OpisPartycji partycja() override { return p_.partycja(); }
  bool czytajPartycje(size_t ile, std::string& b) override { return p_.czytajPartycje(ile, b); }
  bool formatuj() override { return p_.formatuj(); }
  long long wolneMiejsce() override { return p_.wolneMiejsce(); }
 private:
  Pliki& p_;
};
static void testNieudanyOdczytKonfiguracji() {
  Usterki u;
  Bieg A(false, u, "2026-11-16", 10, 5, "noc_zrodlo=json\nladowanie_limit=tak\n");
  LadowarkaBrzegowa la;
  A.p.czujniki.lad = &la;
  ZlaKonfiguracja zk(A.n);
  uint32_t t = ep(2026, 11, 17, 10, 0);
  std::vector<uint8_t> reg04;
  std::vector<size_t> zapisow;
  std::vector<uint32_t> ladZast;
  for (int i = 0; i < 3; i++) {
    zk.zly = i == 1;
    Panel& p = A.p;
    p.zegar.epoch = t;
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = i == 0 ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = i == 0 ? PowodWybudzenia::START : PowodWybudzenia::ZEGAR;
    p.przyciski.skrypt.clear();
    airbridge(A, t);
    Adaptery ad{p.zegar, p.nvs, zk, p.siec, p.ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik,
                p.sen, p.port, p.diag, &p.wydania};
    Dyrygent(ad, A.konf).wybudzenie();
    reg04.push_back(la.r[4]);
    zapisow.push_back(la.zapisy.size());
    ladZast.push_back(A.p.nvs.liczbaU("cpap", "lad_zast", 0xFFFFFFFFu));
    t += (p.sen.plan.sekund ? p.sen.plan.sekund : 60) + 5;
    p.d.z.clear();
  }
  printf("T5 włączony limit, przebieg 2 z nieudanym odczytem /konfiguracja.txt: REG04 po przebiegach %02X %02X %02X, "
         "zapisów do układu %zu\n", reg04[0], reg04[1], reg04[2], la.zapisy.size());
  // nieudany odczyt pliku to „ustawienie nieznane” (jak niezamontowana pamięć plików): bez przywrócenia
  printf("  zapisów do układu po przebiegach %zu %zu %zu; wpis lad_zast %08X / %08X / %08X\n", zapisow[0],
         zapisow[1], zapisow[2], (unsigned)ladZast[0], (unsigned)ladZast[1], (unsigned)ladZast[2]);
  SPRAWDZ(zapisow[0] > 0);             // przebieg 1 założył limit
  SPRAWDZ(reg04[1] == reg04[0]);       // przebieg 2 (nieudany odczyt): REG04 bez zmian
  SPRAWDZ(zapisow[1] == zapisow[0]);   // przebieg 2: 0 dodatkowych zapisów do układu
  SPRAWDZ(ladZast[1] == ladZast[0]);   // przebieg 2: wpis NVS bez zmian
  SPRAWDZ(reg04[2] == reg04[0]);       // następny przebieg z dobrym odczytem: limit dalej
}

// T6 (dopisany przy poprawce, 17.11.2026): nieudany odczyt istniejącego /konfiguracja.txt na ścieżce czekania.
// `kabel_bez_snu` jest wtedy nieznane: głęboki sen w tym przebiegu (powód w logu), bez względu na treść pliku, więc
// błąd odczytu nie włącza czekania wbrew `nie`; następny przebieg z dobrym odczytem czeka znowu (przy `tak`).
static void testNieczytelnaKonfiguracjaKabel() {
  for (int wariant = 0; wariant < 2; wariant++) {
    Usterki u;
    const char* konf = wariant ? "noc_zrodlo=json\nkabel_bez_snu=nie\n" : "noc_zrodlo=json\n";
    Bieg B(true, u, "2026-11-16", 10, 5, konf);
    ZlaKonfiguracja zk(B.lc);
    uint32_t t = ep(2026, 11, 17, 10, 0);
    std::vector<int> czekal, spal;
    std::vector<bool> powod;
    for (int i = 0; i < 3; i++) {
      zk.zly = i == 1;
      Panel& p = B.p;
      p.zegar.epoch = t;
      p.zegar.msTeraz = 0;
      p.start.przyczynaStartu = i == 0 ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
      p.start.powodWybudzenia = i == 0 ? PowodWybudzenia::START : PowodWybudzenia::ZEGAR;
      p.przyciski.skrypt.clear();
      B.lg.n.clear();
      airbridge(B, t);
      size_t l0 = p.port.log.size();
      int s0 = p.sen.snow, c0 = p.sen.czekan;
      Adaptery ad{p.zegar, p.nvs, zk, p.siec, p.ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik,
                  p.sen, p.port, p.diag, &p.wydania};
      bool czekano = Dyrygent(ad, B.konf, &B.pp).wybudzenie();
      czekal.push_back(p.sen.czekan > c0 && czekano);
      spal.push_back(p.sen.snow > s0);
      bool jest = false;
      for (size_t k = l0; k < p.port.log.size(); k++)
        jest = jest || p.port.log[k].find("konfiguracja nieczytelna, kabel_bez_snu nieznane") != std::string::npos;
      powod.push_back(jest);
      if (czekano) B.pl.wlacz(true);
      else B.nowyProgram();
      t += (p.sen.plan.sekund ? p.sen.plan.sekund : 60) + 5;
      p.d.z.clear();
    }
    printf("T6 %s: czekanie %d %d %d, głęboki sen %d %d %d, powód „nieznane” w przebiegu 2: %d\n",
           wariant ? "kabel_bez_snu=nie" : "domyślnie (tak)", czekal[0], czekal[1], czekal[2], spal[0], spal[1],
           spal[2], (int)powod[1]);
    SPRAWDZ(!czekal[1] && spal[1]);  // nieznane: głęboki sen
    SPRAWDZ(powod[1]);               // z powodem w logu (treści pliku panel w tym przebiegu nie zna)
    if (wariant) SPRAWDZ(!czekal[0] && !czekal[2]);  // `nie`: nigdy nie czeka
    else SPRAWDZ(czekal[0] && czekal[2]);
  }
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  printf("testy brzegowe: limit ładowania × czekanie na kablu (ADR-0046 × ADR-0047)\n");
  testFormularz();
  testPrzywrocenieWCzekaniu();
  testNieudanyOdczytKonfiguracji();
  testNieczytelnaKonfiguracjaKabel();
  testLimitLosowo();
  printf("testy_kabel_ladowanie_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
