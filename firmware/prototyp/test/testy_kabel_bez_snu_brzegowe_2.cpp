// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe czekania na kablu, część 2 (ADR-0047 „Zmiana 2026-11-17”).
// Testy przypadków brzegowych, niezależne od testy_kabel_bez_snu.cpp i testy_kabel_bez_snu_brzegowe.cpp (własna uprząż):
//  Q1. PlikiZPamiecia: lista katalogu podanego z ukośnikiem na końcu (Magazyn::sciezki() „/dane/noce/”) a nowy plik
//      zapisany przez warstwę (nowy rok) — czy lista w pamięci jest unieważniana.
//  Q2. PlikiZPamiecia: częściowy wynik listuj() i obcięty pierwszyWiersz() zwrócone z true (na panelu: brak pamięci
//      w openNextFile(), obcięty String w readStringUntil()) — czy zostają w pamięci na kolejne przebiegi.
//  Q3. PlikiZPamiecia: odczyt pustego pliku, nieudany zapis, usunięcie, zmiana nazwy, `.tmp`, limit 1 MB.
//  Q4. Dyrygent przez Nowy Rok (31.12 → 1.01): obie ścieżki, bez usterek, porównanie klatek, NVS, plików, logu.
//  Q5. Losowe sekwencje z usterkami nośnika na OBU ścieżkach (lustro: usterka w B przy odczycie z nośnika, ta sama
//      w A przy tym samym logicznym wywołaniu): nieudany odczyt, nieudane istnieje(), częściowe listuj(), obcięty
//      pierwszyWiersz(), nieudany zapis; z wygaszaczem, komendami `W`, przytrzymaniami, kolejką i odłączaniem kabla.
//  Q6. czekanie::blokada() na brzegach dat i czekanie::przyciskKonczacy() (próg przytrzymania, kolejność).
// Uruchomienie: tools/prototyp/testy.sh (program testy_kabel_bez_snu_brzegowe_2). BRZEG2_LOG=1 drukuje szczegóły,
// BRZEG2_ZIARNA=n liczba ziaren Q5 na rodzaj usterki.
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
// B: przycisk budzący już puszczony (naciśnięcie z kolejki adaptera); dalsze naciśnięcia w tych samych chwilach
// względem puszczenia co w A (A: przycisk trzymany od startu przez `trzymaj` ms)
static std::vector<std::pair<uint32_t, uint8_t>> naciskiPuszczony(uint32_t trzymaj, const std::vector<uint8_t>& kolejne) {
  std::vector<std::pair<uint32_t, uint8_t>> s = {{0, 0}};
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

// konfiguracje z komend `W`: wyłącznik w różnych zapisach (wielkość liter, spacje, komentarz, powtórzony klucz),
// wygaszacz, licznik pełnych odświeżeń, kolejność kokpitu
static const char* const KONFIGI[] = {
    "noc_zrodlo=json\ndzien.wygaszacz=tak\ndzien.wygaszacz_min=30\n",
    "noc_zrodlo=json\nkabel_bez_snu=nie\n",
    "noc_zrodlo=json\nkabel_bez_snu=tak\neink.pelne_co=2\ndzien.wygaszacz_min=30\n",
    "noc_zrodlo=json\nkabel_bez_snu=NIE\n",
    "noc_zrodlo=json\neink.pelne_co=0\nkokpit.kolejnosc=k2,k1\n",
    "noc_zrodlo=json\n# kabel_bez_snu=nie\ndzien.wygaszacz=tak\ndzien.wygaszacz_min=30\neink.pelne_co=1\n",
    "noc_zrodlo=json\nkabel_bez_snu=nie\nkabel_bez_snu=tak\n",
    "noc_zrodlo=json\n kabel_bez_snu = nie \r\n",
    "noc_zrodlo=json\ndzien.wygaszacz=nie\n",
};
static const int KONFIGOW = (int)(sizeof KONFIGI / sizeof KONFIGI[0]);

struct Opcje {
  Los* los = nullptr;
  int krokow = 3000;
  // B: część naciśnięć budzących już puszczona (trwanieBudzacegoMs()). Wyłączone w Q4, Q5: przebieg B zaczyna się
  // wtedy fizycznie po puszczeniu, więc czasy w logu wybudzeń i plan różnią się o czas trzymania (to nie usterka);
  // zgodność ekranów sprawdza Q7.
  bool puszczone = false;
};
struct WynikSym {
  int przebiegow = 0, niezgodnych = 0, czekan = 0, przerwanych = 0, pierwszaNiezg = -1, skk38 = 0, puszczonych = 0,
      wylaczenWZ = 0;
};

static WynikSym symuluj(Bieg& A, Bieg& B, Usterki& u, uint32_t od, uint32_t doT, Opcje& o, const char* nazwa) {
  WynikSym ws;
  Wejscie w;
  w.ep = od;
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  bool odlaczWCzekaniu = false;
  for (int krok = 0; krok < o.krokow && w.ep < doT; krok++) {
    if (odlaczWCzekaniu) B.p.sen.wCzekaniu = [&B]() { B.p.czujniki.zas = Zasilanie::BATERIA; };
    airbridge(A, w.ep);
    airbridge(B, w.ep);
    int czek0 = B.p.sen.czekan, prz0 = B.p.sen.przerwanych;
    u.przebieg.clear();
    Wynik b = przebieg(B, w);  // B pierwszy: losuje usterki przy odczytach z nośnika
    Wynik a = przebieg(A, w);  // A: te same usterki przy tych samych wywołaniach logicznych
    B.p.sen.wCzekaniu = nullptr;
    if (odlaczWCzekaniu) {
      A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::BATERIA;
      odlaczWCzekaniu = false;
    }
    bool zg = porownaj(nazwa, w.ep, a, b);
    ws.przebiegow++;
    if (!zg) {
      ws.niezgodnych++;
      if (ws.pierwszaNiezg < 0) ws.pierwszaNiezg = krok;
    }
    ws.czekan += B.p.sen.czekan > czek0;
    ws.przerwanych += B.p.sen.przerwanych > prz0;
    for (auto& l : a.wiersze) ws.skk38 += l.find("slot dzienny: S-K-38") != std::string::npos;
    if (getenv("BRZEG2_LOG"))
      printf("  %s %s %-8s A kl %d B kl %d (częśc. %d) czekał %d%s\n", nazwa, hhmm(w.ep).c_str(),
             w.pw == PowodWybudzenia::PRZYCISK ? "przycisk" : w.pw == PowodWybudzenia::START ? "start" : "zegar",
             a.klatek, b.klatek, b.czesciowych, (int)b.czekal, zg ? "" : "  ≠");
    uint32_t koniec = w.ep + a.msPrzebiegu / 1000;
    uint32_t nast = koniec + (a.plan.budzZegarem ? (a.plan.sekund ? a.plan.sekund : 1) : OKRES_SNU_S);
    Wejscie n;
    n.ep = nast;
    if (o.los && nast > koniec + 2) {
      Los& r = *o.los;
      int z = r.od(0, 99);
      uint32_t kiedy = koniec + 1 + r.n() % (nast - koniec - 1);
      static const uint8_t BITY[] = {BIT_LEWY, BIT_SRODEK, BIT_PRAWY};
      if (z < 55) {
        uint8_t bit = BITY[r.od(0, 2)];
        if (a.plan.przyciski & bit) {
          n.ep = kiedy;
          n.pw = PowodWybudzenia::PRZYCISK;
          n.budzace = bit;
          std::vector<uint8_t> dalej;
          for (int k = r.od(0, 3); k > 0; k--) dalej.push_back(BITY[r.od(0, 2)]);
          // przytrzymanie: wyraźnie poniżej albo powyżej progu (2000 ms), także tuż obok
          static const uint32_t TRZ[] = {150, 180, 1700, 2400, 2600, 3500};
          uint32_t trz = TRZ[r.od(0, 5)];
          n.skrypt = naciski(bit, trz, dalej);
          if (o.puszczone && b.czekal && r.p(35)) {  // B: puszczone w czekaniu, zanim pętla je zobaczyła
            n.puszczonyPoMs = trz;
            n.skryptB = naciskiPuszczony(trz, dalej);
            ws.puszczonych++;
          }
          for (int k = r.od(0, 2) * (r.p(25) ? 1 : 0); k > 0; k--)
            n.kolejka.push_back({r.od(1, 3), (uint32_t)(r.p(15) ? 2400 : 120)});
          if (z >= 45) n.port = komendaKonfiguracji(KONFIGI[r.od(0, KONFIGOW - 1)]);
        }
      } else if (z < 61) {
        A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::BATERIA;
      } else if (z < 70) {
        A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
      } else if (z < 73) {
        A.p.czujniki.zas = B.p.czujniki.zas = Zasilanie::NIEZNANE;
      } else if (z < 78) {
        odlaczWCzekaniu = true;
      } else if (z < 81) {
        A.p.siec.siecJest = B.p.siec.siecJest = false;
      } else if (z < 87) {
        A.p.siec.siecJest = B.p.siec.siecJest = true;
      } else if (z < 89) {
        n.ep = kiedy;
        n.pw = PowodWybudzenia::START;
        int q = r.od(0, 2);
        n.prz = q == 0 ? PrzyczynaStartu::ZASILANIE : q == 1 ? PrzyczynaStartu::TASK_WDT : PrzyczynaStartu::PANIKA;
        A.p.start.stan = StanRtc();
        B.p.start.stan = StanRtc();
        if (b.czekal) B.nowyProgram();
      }
    }
    w = n;
  }
  return ws;
}

// ---------- Q1: lista katalogu z ukośnikiem na końcu ----------
static void testListaZUkosnikiem() {
  Panel p;
  p.pliki.montowane = true;
  p.pliki.pliki["/dane/noce/2026.csv"] = "naglowek\nwiersz\n";
  PlikiZPamiecia pl(p.pliki);
  pl.wlacz(true);
  std::vector<std::string> v, raw;
  pl.listuj("/dane/noce/", v);  // jak Magazyn::sciezki("noce"): katalog z f.sciezka do `{RRRR}`, z ukośnikiem
  pl.dopiszWiersz("/dane/noce/2027.csv", "nowy rok");  // nowy plik roku przez warstwę
  pl.listuj("/dane/noce/", v);
  p.pliki.listuj("/dane/noce/", raw);
  printf("Q1 listuj(\"/dane/noce/\") po nowym pliku 2027.csv przez warstwę: warstwa %zu plików, nośnik %zu\n", v.size(),
         raw.size());
  SPRAWDZ(v == raw);
  std::vector<std::string> bez, bezRaw;
  pl.listuj("/dane/noce", bez);
  pl.zapiszAtomowo("/dane/noce/2028.csv", "x\n");
  pl.listuj("/dane/noce", bez);
  p.pliki.listuj("/dane/noce", bezRaw);
  SPRAWDZ(bez == bezRaw);  // bez ukośnika unieważnianie działa
  // to samo przez Magazyn (ścieżka produkcyjna: historia nocy)
  Panel q;
  q.pliki.montowane = true;
  PlikiZPamiecia pq(q.pliki);
  pq.wlacz(true);
  Magazyn mag(pq, bezLogu);
  mag.zapisz("noce", mag.sciezkaRoku("noce", "2026"), {"2026-12-31;1;400;2.0"});
  size_t przed = mag.sciezki("noce").size();
  mag.zapisz("noce", mag.sciezkaRoku("noce", "2027"), {"2027-01-01;1;400;2.0"});
  size_t po = mag.sciezki("noce").size();
  Magazyn magNosnik(q.pliki, bezLogu);
  size_t naNosniku = magNosnik.sciezki("noce").size();
  printf("Q1 Magazyn::sciezki(\"noce\") przez PlikiZPamiecia: przed nowym rokiem %zu, po zapisie 2027 %zu, na nośniku "
         "%zu\n", przed, po, naNosniku);
  SPRAWDZ(po == naNosniku);
  // usunięcie pliku z katalogu
  std::vector<std::string> u1, u2;
  pl.listuj("/dane/noce/", u1);
  pl.usun("/dane/noce/2026.csv");
  pl.listuj("/dane/noce/", u2);
  std::vector<std::string> u3;
  p.pliki.listuj("/dane/noce/", u3);
  SPRAWDZ(u2 == u3);
}

// ---------- Q2: częściowe listuj() i obcięty pierwszyWiersz() zwrócone z true ----------
class Jednorazowa : public Pliki {
 public:
  explicit Jednorazowa(AtrapaPlikow& p) : p_(p) {}
  bool obetnijListe = false, obetnijWiersz = false;
  bool zamontuj() override { return p_.zamontuj(); }
  bool zamontowane() override { return p_.zamontowane(); }
  bool istnieje(const char* n) override { return p_.istnieje(n); }
  bool czytajWiersze(const char* n, std::vector<std::string>& w) override { return p_.czytajWiersze(n, w); }
  bool zapiszAtomowo(const char* n, const std::string& t) override { return p_.zapiszAtomowo(n, t); }
  bool dopiszWiersz(const char* n, const std::string& w) override { return p_.dopiszWiersz(n, w); }
  bool pierwszyWiersz(const char* n, std::string& w) override {
    bool r = p_.pierwszyWiersz(n, w);
    if (r && obetnijWiersz) { w.resize(w.size() / 2); obetnijWiersz = false; }
    return r;
  }
  bool usun(const char* n) override { return p_.usun(n); }
  bool zmienNazwe(const char* z, const char* na) override { return p_.zmienNazwe(z, na); }
  void odmontuj() override { p_.odmontuj(); }
  bool czytaj(const char* n, std::string& t) override { return p_.czytaj(n, t); }
  size_t zapisz(const char* n, const std::string& t) override { return p_.zapisz(n, t); }
  bool listuj(const char* k, std::vector<std::string>& nazwy) override {
    bool r = p_.listuj(k, nazwy);
    if (r && obetnijListe && !nazwy.empty()) { nazwy.pop_back(); obetnijListe = false; }
    return r;
  }
  OpisPartycji partycja() override { return p_.partycja(); }
  bool czytajPartycje(size_t ile, std::string& b) override { return p_.czytajPartycje(ile, b); }
  bool formatuj() override { return p_.formatuj(); }
  long long wolneMiejsce() override { return p_.wolneMiejsce(); }

 private:
  AtrapaPlikow& p_;
};
static void testCzescioweWyniki() {
  Panel p;
  p.pliki.montowane = true;
  p.pliki.pliki["/obrazki/001.pbm"] = "P4\n1 1\n\x01";
  p.pliki.pliki["/obrazki/002.pbm"] = "P4\n1 1\n\x01";
  p.pliki.pliki["/dane/noce/2026.csv"] = "#cpap-eink noce v5;data;wazna\n2026-11-01;1\n";
  Jednorazowa j(p.pliki);
  PlikiZPamiecia pl(j);
  pl.wlacz(true);
  std::vector<std::string> v;
  j.obetnijListe = true;
  pl.listuj("/obrazki", v);   // przebieg 1: chwilowy brak pamięci w openNextFile(), wynik częściowy z true
  pl.listuj("/obrazki", v);   // przebieg 2: nośnik sprawny
  printf("Q2 listuj() po jednym częściowym wyniku: warstwa %zu plików, nośnik 2 (po głębokim śnie: 2)\n", v.size());
  SPRAWDZ(v.size() == 2);
  std::string w;
  j.obetnijWiersz = true;
  pl.pierwszyWiersz("/dane/noce/2026.csv", w);
  pl.pierwszyWiersz("/dane/noce/2026.csv", w);
  printf("Q2 pierwszyWiersz() po jednym obciętym: \"%s\"\n", w.c_str());
  SPRAWDZ(w == "#cpap-eink noce v5;data;wazna");
}

// ---------- Q3: pusty plik, nieudany zapis, zmiana nazwy, `.tmp`, limit ----------
static void testWarstwa() {
  Panel p;
  p.pliki.montowane = true;
  p.pliki.pliki["/pusty.txt"] = "";
  p.pliki.pliki["/a.txt"] = "stary";
  PlikiZPamiecia pl(p.pliki, 64);
  pl.wlacz(true);
  std::string t;
  SPRAWDZ(pl.czytaj("/pusty.txt", t) && t.empty());
  SPRAWDZ(pl.czytaj("/pusty.txt", t) && t.empty());
  SPRAWDZ(pl.czytaj("/a.txt", t) && t == "stary");
  p.pliki.odmawiaZapisu = true;
  SPRAWDZ(!pl.zapiszAtomowo("/a.txt", "nowy"));
  SPRAWDZ(pl.czytaj("/a.txt", t) && t == "stary");
  p.pliki.odmawiaZapisu = false;
  SPRAWDZ(pl.zapiszAtomowo("/a.txt", "nowy") && pl.czytaj("/a.txt", t) && t == "nowy");
  p.pliki.pliki["/b.txt"] = "b";
  SPRAWDZ(pl.czytaj("/b.txt", t));
  SPRAWDZ(pl.zmienNazwe("/a.txt", "/b.txt") && pl.czytaj("/b.txt", t) && t == "nowy" && !pl.istnieje("/a.txt"));
  p.pliki.pliki["/c.txt.tmp"] = "z tmp";  // zanik zasilania przed rename
  SPRAWDZ(!pl.istnieje("/c.txt"));
  std::vector<std::string> w;
  SPRAWDZ(pl.czytajWiersze("/c.txt", w) && pl.istnieje("/c.txt") && pl.czytaj("/c.txt", t) && t == "z tmp");
  p.pliki.pliki["/duzy.txt"] = std::string(100, 'x');  // ponad limit 64 B: zawsze z nośnika
  unsigned long n0 = pl.odczytowNosnika;
  SPRAWDZ(pl.czytaj("/duzy.txt", t) && pl.czytaj("/duzy.txt", t) && pl.odczytowNosnika == n0 + 2);
  SPRAWDZ(pl.bajtow() <= 64);
  pl.odmontuj();
  SPRAWDZ(pl.bajtow() == 0);
}

// ---------- Q4: Nowy Rok na dyrygencie ----------
static void testNowyRok() {
  Usterki u;  // bez usterek
  Bieg A(false, u, "2026-12-29", 60, 11, "noc_zrodlo=json\n"), B(true, u, "2026-12-29", 60, 11, "noc_zrodlo=json\n");
  Opcje o;
  WynikSym ws = symuluj(A, B, u, ep(2026, 12, 30, 4, 50), ep(2027, 1, 2, 12, 0), o, "Q4");
  bool rokA = A.p.pliki.pliki.count("/dane/noce/2027.csv") > 0, rokB = B.p.pliki.pliki.count("/dane/noce/2027.csv") > 0;
  printf("Q4 przez Nowy Rok: %d przebiegów, B czekań %d, niezgodnych %d (pierwsza w kroku %d); plik 2027.csv A %d B %d\n",
         ws.przebiegow, ws.czekan, ws.niezgodnych, ws.pierwszaNiezg, (int)rokA, (int)rokB);
  SPRAWDZ(rokA && rokB);
  SPRAWDZ(ws.czekan > 0);
  SPRAWDZ(ws.niezgodnych == 0);
}

// ---------- Q5: losowe sekwencje z usterkami nośnika na obu ścieżkach ----------
struct Rodzaj {
  const char* nazwa;
  int maska;
};
static void testLosoweZUsterkami() {
  int ziaren = getenv("BRZEG2_ZIARNA") ? atoi(getenv("BRZEG2_ZIARNA")) : 3;
  const Rodzaj rodzaje[] = {{"bez usterek", 0},          {"nieudany czytaj()", U_CZYTAJ},
                            {"nieudane istnieje()", U_ISTNIEJE}, {"częściowe listuj()", U_LISTUJ},
                            {"obcięty pierwszyWiersz()", U_PIERWSZY}, {"nieudany zapis", U_ZAPIS},
                            {"wszystkie", U_CZYTAJ | U_ISTNIEJE | U_LISTUJ | U_PIERWSZY | U_ZAPIS}};
  int lacznie = 0, czekanL = 0, przerwL = 0, puszczL = 0, skkL = 0;
  std::map<std::string, int> niezgRodzaju;
  for (const Rodzaj& rz : rodzaje) {
    if (getenv("BRZEG2_TYLKO_Q5") && atoi(getenv("BRZEG2_TYLKO_Q5")) != rz.maska + 1) continue;
    int prz = 0, niezg = 0, wB = 0, wA = 0, zZiaren = 0;
    for (int z = 1; z <= ziaren; z++) {
      Los r((uint64_t)z * 104729u + (uint64_t)rz.maska);
      Los ru((uint64_t)z * 31337u + 17u);
      Usterki u;
      u.maska = rz.maska;
      // odczyty treści są częste (3 promile), listy, pierwsze wiersze, istnienie i zapisy rzadkie (20 promili)
      u.promili = (rz.maska & U_CZYTAJ) ? 3 : 20;
      u.los = &ru;
      const char* konf = KONFIGI[z % KONFIGOW];
      if (std::string(konf).find("kabel_bez_snu=nie") != std::string::npos ||
          std::string(konf).find("kabel_bez_snu=NIE") != std::string::npos ||
          std::string(konf).find(" kabel_bez_snu = nie") != std::string::npos)
        konf = KONFIGI[0];
      Bieg A(false, u, "2026-11-08", 45, (uint64_t)z, konf), B(true, u, "2026-11-08", 45, (uint64_t)z, konf);
      Opcje o;
      o.los = &r;
      char nazwa[48];
      snprintf(nazwa, sizeof nazwa, "Q5/%s/%d", rz.nazwa, z);
      WynikSym ws = symuluj(A, B, u, ep(2026, 11, 9, 4, 50), ep(2026, 11, 11, 4, 50), o, nazwa);
      prz += ws.przebiegow;
      niezg += ws.niezgodnych;
      zZiaren += ws.niezgodnych > 0;
      wB += u.wB;
      wA += u.wA;
      czekanL += ws.czekan;
      przerwL += ws.przerwanych;
      puszczL += ws.puszczonych;
      skkL += ws.skk38;
      if (ws.niezgodnych && getenv("BRZEG2_LOG"))
        printf("  %s: %d niezgodnych z %d (pierwsza w kroku %d)\n", nazwa, ws.niezgodnych, ws.przebiegow,
               ws.pierwszaNiezg);
    }
    lacznie += prz;
    niezgRodzaju[rz.nazwa] = niezg;
    printf("Q5 %-26s: %d ziaren, %d przebiegów, usterek B %d / A %d, niezgodnych przebiegów %d (ziaren z niezgodnością "
           "%d)\n", rz.nazwa, ziaren, prz, wB, wA, niezg, zZiaren);
  }
  printf("Q5 łącznie %d przebiegów; B czekań %d, przerwanych odłączeniem %d, naciśnięć już puszczonych %d, "
         "slot S-K-38 w logu A %d\n", lacznie, czekanL, przerwL, puszczL, skkL);
  SPRAWDZ(lacznie >= 3000);
  SPRAWDZ(czekanL > 0 && przerwL > 0);
  (void)puszczL;
  for (auto& kv : niezgRodzaju) {
    if (kv.second) printf("  BŁĄD Q5: rodzaj „%s”: %d niezgodnych przebiegów\n", kv.first.c_str(), kv.second);
    SPRAWDZ(kv.second == 0);
  }
}

// losowe sekwencje przez Nowy Rok (bez usterek)
static void testLosoweNowyRok() {
  int ziaren = getenv("BRZEG2_ZIARNA") ? atoi(getenv("BRZEG2_ZIARNA")) : 3;
  int prz = 0, niezg = 0, zZiaren = 0;
  for (int z = 1; z <= ziaren; z++) {
    Los r((uint64_t)z * 7727u);
    Usterki u;
    Bieg A(false, u, "2026-12-29", 45, (uint64_t)z, KONFIGI[0]), B(true, u, "2026-12-29", 45, (uint64_t)z, KONFIGI[0]);
    Opcje o;
    o.los = &r;
    char nazwa[32];
    snprintf(nazwa, sizeof nazwa, "Q5b/%d", z);
    WynikSym ws = symuluj(A, B, u, ep(2026, 12, 31, 4, 50), ep(2027, 1, 2, 4, 50), o, nazwa);
    prz += ws.przebiegow;
    niezg += ws.niezgodnych;
    zZiaren += ws.niezgodnych > 0;
  }
  printf("Q5b losowe przez Nowy Rok: %d ziaren, %d przebiegów, niezgodnych %d (ziaren z niezgodnością %d)\n", ziaren, prz,
         niezg, zZiaren);
  SPRAWDZ(niezg == 0);
}

// ---------- Q6: blokada na brzegach dat, przycisk kończący ----------
static void testBrzegi() {
  using czekanie::Blokada;
  using czekanie::blokada;
  SPRAWDZ(blokada("2026-12-31", "2027-01-01") == Blokada::KONIEC);
  SPRAWDZ(blokada("2027-01-01", "2026-12-31") == Blokada::TRWA);
  SPRAWDZ(blokada("2027-01-02", "2026-12-31") == Blokada::PRZYSZLA);
  SPRAWDZ(blokada("2028-02-29", "2028-02-28") == Blokada::TRWA);
  SPRAWDZ(blokada("2028-03-01", "2028-02-29") == Blokada::TRWA);
  SPRAWDZ(blokada("2027-03-01", "2027-02-27") == Blokada::PRZYSZLA);
  SPRAWDZ(blokada("2027-02-29", "2027-02-28") == Blokada::PRZYSZLA);  // data nieistniejąca
  SPRAWDZ(blokada("?", "2027-01-01") == Blokada::USTAL_DZIS);
  SPRAWDZ(blokada("2026-11-17", "") == Blokada::TRWA);
  SPRAWDZ(blokada("xx", "") == Blokada::TRWA);   // zegar nieznany: nawet nieczytelna zostaje (do czasu zegara)
  SPRAWDZ(blokada("xx", "2026-11-17") == Blokada::PRZYSZLA);
  // zegar cofnięty o dobę po restarcie: blokada trwa (bezpieczniej)
  SPRAWDZ(blokada("2026-11-17", "2026-11-16") == Blokada::TRWA);

  using namespace czekanie;
  KlawiszCzekania k[4];
  NacisniecieCzekania q[4];
  // trzymany PRAWY wciśnięty po naciśnięciu ŚRODKA z kolejki: najpierw kolejka
  k[3].wcisniety = true;
  k[3].odUs = 5000000;
  q[0].przycisk = 2;
  q[0].trwaloMs = 100;
  q[0].puszczonyUs = 4900000;
  PrzyciskKonczacy w = przyciskKonczacy(0x0E, k, q, 1);
  SPRAWDZ(w.zKolejki && w.bit == (1u << 2) && w.zdjacZKolejki == 1 && w.trwaloMs == 100);
  // trzymany wcześniej niż naciśnięcie z kolejki: trzymany, kolejka zostaje
  k[3].odUs = 4000000;
  w = przyciskKonczacy(0x0E, k, q, 1);
  SPRAWDZ(!w.zKolejki && w.bit == (1u << 3) && w.zdjacZKolejki == 0 && w.odUs == 4000000);
  // spoza maski przed w masce: przepada
  KlawiszCzekania k0[4];
  q[0].przycisk = 1;
  q[1].przycisk = 3;
  q[1].trwaloMs = 2500;
  q[1].puszczonyUs = 9000000;
  w = przyciskKonczacy(0x08, k0, q, 2);
  SPRAWDZ(w.zKolejki && w.bit == 0x08 && w.zdjacZKolejki == 2 && w.trwaloMs == 2500);
  // nic w masce (tylko ŚRODEK; w kolejce LEWY i PRAWY): kolejka czyszczona
  w = przyciskKonczacy(0x04, k0, q, 2);
  SPRAWDZ(w.bit == 0 && w.zdjacZKolejki == 2);
  // klawisz `pomin` (budzący poprzedniego przebiegu, wciąż trzymany) nie kończy czekania
  KlawiszCzekania kp[4];
  kp[2].wcisniety = kp[2].pomin = true;
  kp[2].odUs = 1;
  w = przyciskKonczacy(0x0E, kp, q, 0);
  SPRAWDZ(w.bit == 0);
  printf("Q6 blokada na brzegach dat i przycisk kończący: sprawdzone\n");
}

// ---------- Q7: naciśnięcie budzące już puszczone w czekaniu (trwanieBudzacegoMs()) a przytrzymanie po głębokim śnie ----
// Ten sam panel, to samo naciśnięcie: A po głębokim śnie trzyma przycisk od startu przez `trz` ms, B po czekaniu
// dostaje naciśnięcie z kolejki adaptera z czasem `trz`. Te same klatki (ekran), ta sama klasyfikacja krótkie /
// przytrzymanie (próg PROG_PRZYTRZYMANIA_MS jak po głębokim śnie).
static void testPuszczone() {
  static const uint32_t TRZ[] = {150, 1999, 2000, 2600};
  static const uint8_t BITY[] = {BIT_LEWY, BIT_SRODEK, BIT_PRAWY};
  static const uint32_t GODZ[] = {9 * 60 + 10, 13 * 60 + 40, 21 * 60 + 5};
  int prob = 0, zgodnych = 0;
  for (uint32_t trz : TRZ)
    for (uint8_t bit : BITY)
      for (uint32_t g : GODZ) {
        Usterki u;
        Bieg A(false, u, "2026-11-08", 40, 5, KONFIGI[0]), B(true, u, "2026-11-08", 40, 5, KONFIGI[0]);
        Wejscie w;
        w.ep = ep(2026, 11, 9, 4, 50);
        w.pw = PowodWybudzenia::START;
        w.prz = PrzyczynaStartu::ZASILANIE;
        uint32_t cel = ep(2026, 11, 9, 0, 0) + g * 60;
        Wynik a, b;
        for (int k = 0; k < 400 && w.ep < cel; k++) {
          airbridge(A, w.ep);
          airbridge(B, w.ep);
          b = przebieg(B, w);
          a = przebieg(A, w);
          Wejscie n;
          n.ep = w.ep + a.msPrzebiegu / 1000 + (a.plan.budzZegarem ? (a.plan.sekund ? a.plan.sekund : 1) : OKRES_SNU_S);
          w = n;
        }
        if (!b.czekal || !(a.plan.przyciski & bit)) continue;
        Wejscie n;
        n.ep = w.ep - 1;  // sekunda przed terminem planu
        n.pw = PowodWybudzenia::PRZYCISK;
        n.budzace = bit;
        n.skrypt = naciski(bit, trz, {});
        n.puszczonyPoMs = trz;
        n.skryptB = naciskiPuszczony(trz, {});
        airbridge(A, n.ep);
        airbridge(B, n.ep);
        Wynik bb = przebieg(B, n), aa = przebieg(A, n);
        prob++;
        bool z = aa.klatki == bb.klatki && aa.klatek == bb.klatek;
        zgodnych += z;
        if (!z)
          printf("  Q7 trz %u ms bit %u %02u:%02u: klatek A %d B %d (%s)\n", (unsigned)trz, (unsigned)bit, g / 60, g % 60,
                 aa.klatek, bb.klatek, aa.klatki == bb.klatki ? "=" : "≠");
      }
  printf("Q7 naciśnięcie już puszczone: %d prób, zgodne ekrany %d (od 17.11.2026: w obu ścieżkach trzymanie "
         "co najmniej PROG_PRZYTRZYMANIA_MS = przytrzymanie, także dokładnie 2000 ms)\n", prob, zgodnych);
  SPRAWDZ(prob > 10);
  SPRAWDZ(zgodnych == prob);
}

// ---------- Q8: jedna usterka nośnika w pierwszym dostępie po włączeniu pamięci plików, dalej nośnik sprawny ----------
// A (głęboki sen) i B (czekanie) dostają tę samą usterkę w tym samym wywołaniu logicznym; potem dzień bez usterek.
static int q8(const char* op, const char* nazwa, const char* opis) {
  Usterki u;
  Bieg A(false, u, "2026-11-08", 45, 3, KONFIGI[0]), B(true, u, "2026-11-08", 45, 3, KONFIGI[0]);
  Wejscie w;
  w.ep = ep(2026, 11, 9, 4, 50);
  w.pw = PowodWybudzenia::START;
  w.prz = PrzyczynaStartu::ZASILANIE;
  bool uzbrojono = false;
  int niezg = 0, po = 0;
  static const uint32_t NACISKI[] = {9 * 60 + 10, 13 * 60 + 40, 18 * 60 + 20, 21 * 60 + 5};
  for (int k = 0; k < 600 && w.ep < ep(2026, 11, 10, 23, 0); k++) {
    airbridge(A, w.ep);
    airbridge(B, w.ep);
    u.przebieg.clear();
    Wynik b = przebieg(B, w), a = przebieg(A, w);
    bool z = porownaj(opis, w.ep, a, b);
    if (uzbrojono) {
      po++;
      niezg += !z;
    }
    if (!uzbrojono && b.czekal && B.pl.wlaczona()) {
      u.uzbrojOp = op;
      u.uzbrojNazwa = nazwa;
      uzbrojono = true;
    }
    uint32_t koniec = w.ep + a.msPrzebiegu / 1000;
    Wejscie n;
    n.ep = koniec + (a.plan.budzZegarem ? (a.plan.sekund ? a.plan.sekund : 1) : OKRES_SNU_S);
    for (uint32_t d = 0; d < 2; d++)
      for (uint32_t g : NACISKI) {
        uint32_t t = ep(2026, 11, 9, 0, 0) + d * 86400u + g * 60u;
        if (t > koniec && t < n.ep && (a.plan.przyciski & BIT_PRAWY) && n.pw != PowodWybudzenia::PRZYCISK) {
          n.ep = t;
          n.pw = PowodWybudzenia::PRZYCISK;
          n.budzace = BIT_PRAWY;
          n.skrypt = naciski(BIT_PRAWY, 180, {BIT_PRAWY});
        }
      }
    w = n;
  }
  printf("Q8 jedna usterka (%s) po włączeniu pamięci plików: wstrzyknięta B %d / A %d, potem %d przebiegów, "
         "niezgodnych %d\n", opis, u.wB, u.wA, po, niezg);
  SPRAWDZ(u.wB == 1 && u.wA == 1);
  return niezg;
}
static void testJednaUsterka() {
  SPRAWDZ(q8("listuj", "/dane/noce/", "częściowe listuj(\"/dane/noce/\")") == 0);
  SPRAWDZ(q8("czytaj", "/dane/noce/2026.csv", "nieudany czytaj(2026.csv)") == 0);
  SPRAWDZ(q8("czytaj", "/konfiguracja.txt", "nieudany czytaj(/konfiguracja.txt)") == 0);
  SPRAWDZ(q8("istnieje", "/dane/noce/2026.csv", "nieudane istnieje(2026.csv)") == 0);
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  printf("testy czekania na kablu (ADR-0047, zmiana 2026-11-17)\n");
  if (getenv("BRZEG2_TYLKO_Q5")) {  // diagnostyka: tylko Q5
    testLosoweZUsterkami();
    printf("testy_kabel_bez_snu_brzegowe_2: %d dobrze, %d źle\n", ok, zle);
    return zle ? 1 : 0;
  }
  testListaZUkosnikiem();
  testCzescioweWyniki();
  testWarstwa();
  testBrzegi();
  testNowyRok();
  testLosoweNowyRok();
  testPuszczone();
  testJednaUsterka();
  testLosoweZUsterkami();
  printf("testy_kabel_bez_snu_brzegowe_2: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
