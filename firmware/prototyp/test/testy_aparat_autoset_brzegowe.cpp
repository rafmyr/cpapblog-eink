// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (aparat AirSense w trybie AutoSet, nowy panel bez historii).
// Raporty mają format Załącznika A spec (tagi MOP, MPI, MPA, EPR, kanały EDF) i idą prawdziwą ścieżką nocZRaportu() →
// Dyrygent → magazyn: symulator tej ścieżki nie przechodzi (atrapa odpowiada 404 na raport dnia, noce wstawia wprost do
// plików), a scenariusze `autoset` i `cpap` mają ustawienia `AutoSet/7/15/2`, `CPAP/10`, których raport nigdy nie daje.
// Wszystkie przypadki są SPRAWDZ (blokują); makro OCZEKUJE zostaje na przyszłe.
// Dane syntetyczne. Pierwszy argument: katalog repozytorium (odczyt plików źródłowych).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "ekrany_zdarzen.h"
#include "kokpit.h"
#include "magazyn.h"
#include "noc.h"
#include "odkrycia.h"
#include "reguly.h"
#include "sprawy.h"
#include "stany.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"
#include "wzorce.h"

namespace reguly { void zarejestrujRodzajeE(); }  // src/rodzaje_e.cpp, jak w src/dyrygent.cpp

static int ok = 0, zle = 0, oczekuje = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define OCZEKUJE(id, w)                                                                   \
  do {                                                                                    \
    if (w) ok++;                                                                          \
    else { oczekuje++; printf("OCZEKUJE %s (%s:%d): %s\n", id, __FILE__, __LINE__, #w); } \
  } while (0)

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_AIRSENSE =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirSense_10_AutoSet\",\"srn\":\"22222222222\",\"mode\":\"AutoSet\","
    "\"system\":\"IDLE\",\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";

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

static std::string pole(const char* tag, const std::string& v, int opcja = -1) {
  std::string s = std::string("{\"tag\":\"") + tag + "\",\"label\":\"x\",\"value\":\"" + v + "\",\"unit\":\"\"";
  if (opcja >= 0) s += ",\"option\":" + std::to_string(opcja);
  return s + "}";
}
static std::string ustAutoSet(const std::string& mpi, const std::string& mpa, const std::string& epr) {
  return pole("MOP", "AutoSet", 1) + "," + pole("MPI", mpi) + "," + pole("MPA", mpa) + "," + pole("EPR", epr) + "," +
         pole("RMA", "Off") + "," + pole("HMS", "3");
}
static std::string kanal(const char* id, const char* j, double med, double p95) {
  char b[256];
  snprintf(b, sizeof b,
           "{\"id\":\"%s\",\"label\":\"x\",\"unit\":\"%s\",\"count\":12600,\"coverage_seconds\":25200,\"min\":1,"
           "\"mean\":%g,\"median\":%g,\"p95\":%g,\"max\":%g,\"source\":\"edf\"}",
           id, j, med, med, p95, p95 + 1);
  return b;
}
// Raport dnia aparatu AirSense: bez EAI, EAX, ANS, AXS, EEP, MNS, MXS; `ustawienia` = zawartość settings.fields.
static std::string raport(long day, double pMed, double pP95, const std::string& ustawienia, int sekund = 25200,
                          double eprMed = NAN, double eprP95 = NAN) {
  std::string d = std::to_string(day);
  unsigned long long k = (unsigned long long)(day * 86400 + 86400 + 5 * 3600), s = k - (unsigned long long)sekund;
  if (std::isnan(eprMed)) { eprMed = pMed - 2; eprP95 = pP95 - 2; }
  return "{\"source\":\"edf\",\"revision\":1,\"view\":\"day\",\"day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":" + std::to_string(s * 1000ULL) + ",\"end_ms\":" + std::to_string(k * 1000ULL) +
         "}],\"present\":true,\"partial\":false,\"used_seconds\":" + std::to_string(sekund) +
         ",\"rdi\":2,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":60,\"session_count\":1,\"ahi\":1.4,"
         "\"events_known\":true,\"counts\":{\"oa\":3,\"ca\":2,\"ua\":0,\"h\":5,\"rera\":1},\"channels\":[" +
         kanal("Press", "cmH2O", pMed, pP95) + "," + kanal("EprPress", "cmH2O", eprMed, eprP95) + "," +
         kanal("Leak", "L/min", 2, 9) + "," + kanal("MinVent", "L/min", 7, 10) + "," + kanal("RespRate", "/min", 14, 18) +
         "," + kanal("TidVol", "L", 0.5, 0.7) + "," + kanal("FlowLim", "", 0.02, 0.1) +
         "],\"settings\":{\"day\":" + d + ",\"available\":" + (ustawienia.empty() ? "false" : "true") + ",\"fields\":[" +
         ustawienia + "]}}";
}

static std::string czytajPlik(const std::string& p) {
  std::ifstream f(p);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}
static bool logMa(const Panel& p, const std::string& t) {
  for (auto& l : p.port.log)
    if (l.find(t) != std::string::npos) return true;
  return false;
}

// ---------- pola AirSense: ścieżka raport → rekord (trzyma) ----------
static void rekordAutoSet() {
  long d = nrDnia("2026-11-06");
  NocAB a = nocZRaportu(raport(d, 9.5, 12, ustAutoSet("6.0 cmH2O", "14.0 cmH2O", "3")), d, "AutoSet",
                        "AirSense_10_AutoSet", "22222222222");
  SPRAWDZ(a.stan == StanRaportu::KOMPLETNA && a.noc.wazna && a.noc.tryb == "AutoSet");
  SPRAWDZ(a.noc.mpi == 6.0f && a.noc.mpa == 14.0f && a.noc.epr == 3.0f);
  SPRAWDZ(std::isnan(a.noc.eai) && std::isnan(a.noc.eax) && std::isnan(a.noc.psmed) && a.noc.cp95 == 12.0f);
  SPRAWDZ(a.noc.pna == "AirSense_10_AutoSet");
  // modele z nagłówków 22 kart: model nie decyduje, model nie jest kryterium; to nie znaczy, że seria 11 jest obsługiwana
  for (const char* m : {"AirSense_10_AutoSet_For_Her", "AirSense11AutoSet", "AirSense11Elite"}) {
    NocAB x = nocZRaportu(raport(d, 9.5, 12, ustAutoSet("6", "14", "3")), d, "AutoSet", m, "1");
    SPRAWDZ(x.noc.wazna && x.noc.pna == m && x.noc.tryb == "AutoSet");
    NocAB y = nocZRaportu(raport(d, 9.5, 12, pole("MOP", "CPAP", 0) + "," + pole("IPC", "9") + "," + pole("EPR", "2")), d, "CPAP", m, "1");
    SPRAWDZ(y.noc.wazna && y.noc.tryb == "CPAP");
  }
  // `pna` pusty i brak numeru: noc ważna, „brak odczytu nie jest zmianą aparatu”
  NocAB b = nocZRaportu(raport(d, 9.5, 12, ustAutoSet("6", "14", "3")), d, "AutoSet");
  SPRAWDZ(b.noc.wazna && b.noc.pna.empty());
  // tryb decyduje dalej (R-DANE-29): VAuto, APAP, AutoSet Her, nieznany numer opcji poza oceną, także na modelu AirSense
  for (const char* t : {"VAuto", "APAP", "AutoSet Her", "iVAPS"}) {
    NocAB x = nocZRaportu(raport(d, 9.5, 12, pole("MOP", t)), d, "AutoSet", "AirSense_10_AutoSet");
    SPRAWDZ(!x.noc.wazna && x.noc.tryb == t);
  }
  NocAB o = nocZRaportu(raport(d, 9.5, 12, "{\"tag\":\"MOP\",\"value\":99}"), d, "AutoSet", "AirSense_10_AutoSet");
  SPRAWDZ(!o.noc.wazna && o.noc.tryb.empty());
  // brak `settings` i brak `mode`: tryb nieznany, poza oceną
  NocAB n = nocZRaportu(raport(d, 9.5, 12, ""), d, "", "AirSense_10_AutoSet");
  SPRAWDZ(!n.noc.wazna && n.noc.tryb.empty());
  // K1 i K2 od pierwszej nocy w układzie AutoSet: bez EPAP i Wsparcia (trzyma)
  Norma nm;
  std::vector<Noc> h = {a.noc};
  DaneK1 k1 = budujK1(h, a.noc.data, nm, {});
  DaneK2 k2 = budujK2(h, a.noc.data, nm, {});
  SPRAWDZ(k1.wierszy == 4 && k1.opisy[2] == "ciśnienie" && std::string(k1.w[2].nazwa) == "Ciśnienie" && k2.wierszy == 9);
  for (int i = 0; i < k2.wierszy; i++) SPRAWDZ(std::string(k2.w[i].etykieta).find("EPAP") == std::string::npos);
}

// ---------- zmiana ustawień AutoSet i CPAP niewidoczna ----------
static void ustawieniaBezWartosci() {
  long d = nrDnia("2026-11-06");
  Noc a = nocZRaportu(raport(d, 9.5, 12, ustAutoSet("6", "14", "3")), d, "AutoSet").noc;
  Noc b = nocZRaportu(raport(d, 9.5, 12, ustAutoSet("8", "18", "3")), d, "AutoSet").noc;
  Noc c = nocZRaportu(raport(d, 9.5, 12, ustAutoSet("6", "14", "1")), d, "AutoSet").noc;
  SPRAWDZ(a.ustawienia != b.ustawienia);  // dziś oba „AutoSet////”; zestaw: R-NOR-07.zmiana_ustawien MOP,MPI,MPA,IPC,EPR
  SPRAWDZ(a.ustawienia != c.ustawienia);  // EPR
  Noc e = nocZRaportu(raport(d, 10, 10, pole("MOP", "CPAP", 0) + "," + pole("IPC", "9") + "," + pole("EPR", "2")), d, "CPAP").noc;
  Noc f = nocZRaportu(raport(d, 12, 12, pole("MOP", "CPAP", 0) + "," + pole("IPC", "12") + "," + pole("EPR", "2")), d, "CPAP").noc;
  SPRAWDZ(e.ustawienia != f.ustawienia);
  Noc g = nocZRaportu(raport(d, 12, 15, pole("MOP", "ASV", 7) + "," + pole("EEP", "6") + "," + pole("MNS", "3") + "," + pole("MXS", "15")), d, "ASV").noc;
  Noc i = nocZRaportu(raport(d, 12, 15, pole("MOP", "ASV", 7) + "," + pole("EEP", "8") + "," + pole("MNS", "3") + "," + pole("MXS", "15")), d, "ASV").noc;
  SPRAWDZ(g.ustawienia != i.ustawienia);  // ten sam skrót w ASV ze stałym EPAP
  // ASVAuto bez zmian: format zgodny z tools/prototyp/historia.py (trzyma, po poprawce też ma trzymać)
  Noc j = nocZRaportu(raport(d, 12, 15, pole("MOP", "ASVAuto", 8) + "," + pole("EAI", "6") + "," + pole("EAX", "10") + "," +
                                             pole("ANS", "3") + "," + pole("AXS", "15")), d, "ASVAuto").noc;
  SPRAWDZ(j.ustawienia == "ASVAuto/6/10/3/15");
}

// R-NOR-07, weryfikacja ze spec: „Zmiana max z 13 na 15 → przez 14 nocy brak E-36 …, normy tych metryk puste”. Dziś w AutoSet
// norma ciśnienia ze starego zakresu zostaje, a E-36 wchodzi: użytkownik po zmianie zakresu przez lekarza dostaje ekran
// „Ciśnienie wyższe niż zwykle”, którego spec zabrania. Ten sam przebieg z tekstem ustawień scenariusza symulatora
// (`AutoSet/8/18/3`) ekranu nie daje: symulator tego nie pokaże.
static void e36PoZmianieZakresu() {
  reguly::zarejestrujRodzajeE();
  wzorce::zarejestrujRodzajeL();
  odkrycia::zarejestrujRodzajeO();
  const reguly::Zestaw& z = reguly::zestawWbudowany();
  long d0 = nrDnia("2026-10-01");
  auto przebieg = [&](bool jakScenariusz, Stany& st) {
    std::vector<Noc> h;
    for (int i = 0; i < 40; i++) {
      bool po = i >= 33;
      Noc n = nocZRaportu(raport(d0 + i, po ? 12.5 : 9.5, po ? 15.0 + 0.01 * i : 12.0 + 0.01 * (i % 5),
                                 po ? ustAutoSet("8", "18", "3") : ustAutoSet("6", "14", "3"), 25200 + i * 30),
                          d0 + i, "AutoSet", "AirSense_10_AutoSet", "1").noc;
      if (jakScenariusz) n.ustawienia = po ? "AutoSet/8/18/3" : "AutoSet/6/14/3";
      h.push_back(n);
    }
    std::string dzis = h.back().data;
    std::set<std::string> wyk;
    st = liczStany(h, dzis, wyk);
    reguly::Kontekst k;
    k.historia = &h;
    k.dzis = dzis;
    k.norma = &st.norma;
    k.stany = &st;
    k.trybBiezacy = reguly::trybZUstawien(h.back().ustawienia);
    static const reguly::Odpowiedzi odp;
    static const reguly::Pamiec pam;
    k.wykluczoneNoce = &wyk;
    k.odpowiedzi = &odp;
    k.pamiec = &pam;
    k.zestaw = &z;
    bool e36 = false;
    for (auto& r : reguly::ocen(z, k))
      if (r.id == "E-36" && r.spelniona) e36 = true;
    return e36;
  };
  Stany st;
  bool e36Scenariusz = przebieg(true, st);
  SPRAWDZ(!e36Scenariusz);
  SPRAWDZ(!st.norma.cp95.jest());
  bool e36Raport = przebieg(false, st);
  SPRAWDZ(!e36Raport);
  SPRAWDZ(!st.norma.cp95.jest());
}

// ---------- dodatkowe przypadki ----------
static void zeroNocy() {
  Norma nm;
  DaneK1 k1 = budujK1({}, "2026-11-07", nm, {}, nullptr, "AutoSet");  // tryb z /api/status
  DaneK2 k2 = budujK2({}, "2026-11-07", nm, {}, nullptr, "AutoSet");
  DaneK1 k1a = budujK1({}, "2026-11-07", nm, {});  // bez statusu: układ ASV jak dotąd (egzemplarz referencyjny)
  SPRAWDZ(k1a.opisy[2] == "EPAP");
  // bez nocy panel rysuje układ ASV, choć `/api/status` podaje AutoSet (obrazek w notatce; tylko do pierwszej nocy)
  SPRAWDZ(k1.opisy[2] != "EPAP");
  SPRAWDZ(k2.wierszy != 10);
  // rodzinaAsv(NIEZNANY) i sprawy.cpp:144: bez skutku w AutoSet: E-36 dostaje M-05p w AutoSet i CPAP, M-06p w ASV (trzyma)
  sprawy::SprawaL s;
  s.metrykaWiz = "M-06p";
  s.metrykaWizCpap = "M-05p";
  SPRAWDZ(zdarzenia::metrykaWykresu(s, reguly::Tryb::AUTOSET) == "M-05p");
  SPRAWDZ(zdarzenia::metrykaWykresu(s, reguly::Tryb::CPAP) == "M-05p");
  SPRAWDZ(zdarzenia::metrykaWykresu(s, reguly::Tryb::ASVAUTO) == "M-06p");
  const reguly::Zestaw& z = reguly::zestawWbudowany();
  const reguly::Prog* pa = z.prog("E-36.sygnal_cisnienia", reguly::Tryb::AUTOSET);
  const reguly::Prog* pc = z.prog("E-36.sygnal_cisnienia", reguly::Tryb::CPAP);
  SPRAWDZ(pa && pc && pa->tekst == "M-05p" && pc->tekst == "M-05p");
  // E-36 w AutoSet nie liczy EprPress (atrapa scenariusza tego kanału nie podaje, AirSense tak):
  // Press stałe, EprPress skacze (np. zmiana EPR) → bez E-36 (trzyma)
  long d0 = nrDnia("2026-10-01");
  std::vector<Noc> h;
  for (int i = 0; i < 40; i++)
    h.push_back(nocZRaportu(raport(d0 + i, 9.5, 12.0 + 0.01 * (i % 5), ustAutoSet("6", "14", "3"), 25200 + i * 30,
                                   i >= 33 ? 5.0 : 7.5, i >= 33 ? 6.0 : 10.0), d0 + i, "AutoSet").noc);
  std::set<std::string> wyk;
  Stany st = liczStany(h, h.back().data, wyk);
  SPRAWDZ(!st.cp95.aktywny && !st.e36);
}

// ---------- przebieg przez dyrygenta: noc po nocy, raport z Załącznika A ----------
struct Sym {
  Panel p;
  std::map<long, std::string> raporty;
  void wybudz(uint32_t t, PowodWybudzenia pw) {
    p.siec.odpowiedzi.clear();
    p.siec.siecJest = true;
    p.siec.dodaj("/api/status", 200, STATUS_AIRSENSE);
    std::string l = "{\"source\":\"edf\",\"days\":[";
    bool pierwszy = true;
    for (auto& kv : raporty) {
      l += std::string(pierwszy ? "" : ",") + "{\"day\":" + std::to_string(kv.first) + ",\"session_count\":1,\"stale\":false}";
      pierwszy = false;
    }
    p.siec.dodaj(sciezkaListy(), 200, l + "]}");
    for (auto& kv : raporty) p.siec.dodaj(sciezkaNocy(kv.first), 200, kv.second);
    p.zegar.epoch = t;
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = pw;
    p.siec.polaczonaTeraz = false;
    p.siec.okno = false;
    p.port.log.clear();
    p.d.z.clear();
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
  }
};

// Start bez nocy, potem pierwsze poranki: rekordy na dysku, bez „nieważnej” i bez E-03.
static void pierwszeDni() {
  Sym s;
  s.wybudz(ep(2026, 11, 6, 14, 0), PowodWybudzenia::START);
  SPRAWDZ(!logMa(s.p, "E-03"));
  long d1 = nrDnia("2026-11-06"), d2 = nrDnia("2026-11-07");
  s.raporty[d1] = raport(d1, 9.5, 12, ustAutoSet("6.0", "14.0", "3"));
  s.wybudz(ep(2026, 11, 7, 6, 30), PowodWybudzenia::ZEGAR);
  std::string pl = s.p.pliki.tresc("/dane/noce/2026.csv");
  SPRAWDZ(pl.find("2026-11-07;1;420;") != std::string::npos);  // noc ważna w pierwszym dniu
  SPRAWDZ(pl.find("AirSense_10_AutoSet") != std::string::npos);
  SPRAWDZ(!logMa(s.p, "tryb aparatu nieznany"));
  // kolejna noc z `settings.available = false` (AirSense znany tylko z kodu). 4.11, fail-path R-DANE-29: po
  // wcześniejszym odczycie panel przyjmuje tryb z ostatniej nocy z odczytem. Dziś dyrygent bierze tryb z pustego pola
  // ostatniej nocy i wyłącza wszystkie reguły, a kokpit dalej pokazuje AutoSet.
  s.raporty[d2] = raport(d2, 9.5, 12, "");
  s.wybudz(ep(2026, 11, 8, 6, 30), PowodWybudzenia::ZEGAR);
  SPRAWDZ(!logMa(s.p, "tryb aparatu nieznany"));
}

// R-DANE-29: raz na serię linia `Tryb aparatu {tryb}: ta wersja go nie ocenia.` albo `Nie znam trybu aparatu…`;
// instrukcja wgrania ma ją jako objaw na egzemplarzu testowym. W src/ takiego tekstu nie ma, ekran mówi `Uczę się`.
static void liniaTrybu(const std::string& repo) {
  const std::string sc = czytajPlik(repo + "/firmware/prototyp/src/dyrygent.cpp") + czytajPlik(repo + "/firmware/prototyp/src/zestaw_wbudowany.h");
  if (sc.empty()) return;
  SPRAWDZ(sc.find("ta wersja go nie ocenia") != std::string::npos);
  SPRAWDZ(sc.find("Nie znam trybu aparatu") != std::string::npos);
  // R-NOR-14: linii `Nowe ustawienia…` też nie ma w żadnym trybie, więc fałszywego „Nowe ustawienia” nie będzie (trzyma), ale
  // po poprawce zmiana ustawień przejdzie bez słowa na ekranie
  SPRAWDZ(sc.find("Nowe ustawienia") != std::string::npos);
}

// ---------- `W konfiguracja.txt` ----------
static std::string komendaW(const std::string& n, const std::string& t) {
  return "W " + n + " " + std::to_string(t.size()) + " " + sha256Hex(t) + "\n" + t;
}
static void doPortu(Panel& p, const std::string& b) {
  for (char c : b) p.port.znaki.push_back((unsigned char)c);
}
static bool portMa(const Panel& p, const std::string& t) { return logMa(p, t); }
static void wybudzZegarem(Panel& p, int h, int m) {
  p.zegar.epoch = ep(2026, 11, 4, h, m);
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static void konfiguracjaW() {
  // limit: dokładnie 16 KB przechodzi, bajt więcej nie; port zsynchronizowany po odrzuceniu
  {
    Panel p;
    std::string g(MAKS_KONFIGURACJI_W, '#');
    std::string za(MAKS_KONFIGURACJI_W + 1, '#');
    doPortu(p, "?" + komendaW("konfiguracja.txt", za) + komendaW("konfiguracja.txt", g));
    p.nvs.ustawTekst("cpap", NVS_KLUCZ_PRZYWR, "2026-11-01 40");
    wybudzZegarem(p, 14, 0);
    SPRAWDZ(portMa(p, "BLAD konfiguracja.txt rozmiar"));
    SPRAWDZ(portMa(p, "OK konfiguracja.txt"));
    SPRAWDZ(p.pliki.tresc("/konfiguracja.txt") == g);
  }
  // nazwy złośliwe nie piszą nigdzie
  for (const char* n : {"../konfiguracja.txt", "/konfiguracja.txt", "konfiguracja.txt/", "KONFIGURACJA.TXT", "dane/konfiguracja.txt"}) {
    Panel p;
    p.nvs.ustawTekst("cpap", NVS_KLUCZ_PRZYWR, "2026-11-01 40");
    p.pliki.pliki["/konfiguracja.txt"] = "profil=prywatny\n";
    doPortu(p, "?" + komendaW(n, "profil=publiczny\n"));
    wybudzZegarem(p, 14, 0);
    SPRAWDZ(p.pliki.tresc("/konfiguracja.txt") == "profil=prywatny\n");
    SPRAWDZ(portMa(p, std::string("BLAD ") + n));
  }
  // treść niepoprawna (śmieci bez bajtu 0, CRLF, BOM) nie psuje niczego; poprawne wiersze działają, złe w logu; noce,
  // NVS i /przywracanie/ nietknięte (rekordy z prawdziwego przebiegu)
  {
    Sym s;
    long d = nrDnia("2026-11-06");
    s.raporty[d] = raport(d, 9.5, 12, ustAutoSet("6", "14", "3"));
    s.wybudz(ep(2026, 11, 6, 14, 0), PowodWybudzenia::START);
    s.wybudz(ep(2026, 11, 7, 6, 30), PowodWybudzenia::ZEGAR);
    const std::string noce = s.p.pliki.tresc("/dane/noce/2026.csv");
    SPRAWDZ(noce.find("2026-11-07;1;") != std::string::npos);
    std::string nvsPrzed = s.p.nvs.tekst("cpap", NVS_KLUCZ_PRZYWR, "");
    std::string t = "\xEF\xBB\xBFprofil=publiczny\r\nco2=nie\r\nsmieci\xFF\xFE bez rownosci\r\nwymiana.maska=RRRR-MM-DD\r\n";
    doPortu(s.p, "?" + komendaW("konfiguracja.txt", t));
    s.wybudz(ep(2026, 11, 7, 6, 45), PowodWybudzenia::ZEGAR);
    SPRAWDZ(portMa(s.p, "OK konfiguracja.txt"));
    SPRAWDZ(s.p.pliki.tresc("/konfiguracja.txt") == t);
    SPRAWDZ(!s.p.pliki.pliki.count("/konfiguracja.txt.tmp"));
    SPRAWDZ(s.p.pliki.tresc("/dane/noce/2026.csv") == noce);
    SPRAWDZ(s.p.nvs.tekst("cpap", NVS_KLUCZ_PRZYWR, "") == nvsPrzed);
    for (auto& kv : s.p.pliki.pliki) SPRAWDZ(kv.first.compare(0, 14, "/przywracanie/") != 0);
    // nowa konfiguracja czytana w następnym wybudzeniu (instrukcja, krok 8)
    s.wybudz(ep(2026, 11, 7, 7, 0), PowodWybudzenia::ZEGAR);
    SPRAWDZ(portMa(s.p, "konfiguracja: profil publiczny"));
    SPRAWDZ(portMa(s.p, "pominięty wiersz"));
  }
  // w oknie przywracania: bez znacznika udanego przywrócenia, bez próby przywracania; okno kończy K
  {
    Panel p;
    doPortu(p, "?" + komendaW("konfiguracja.txt", "profil=publiczny\nco2=nie\n") + "K");
    p.zegar.epoch = ep(2026, 11, 4, 7, 0);
    p.zegar.msTeraz = 0;
    p.start.stan = StanRtc();
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(portMa(p, "OK konfiguracja.txt"));
    SPRAWDZ(p.pliki.tresc("/konfiguracja.txt") == "profil=publiczny\nco2=nie\n");
    SPRAWDZ(!p.nvs.flaga("cpap", NVS_KLUCZ_PRZYWR_PROBA, false));
  }
}

// ---------- plik testu po przejściu na firmware panelu ----------
// Instrukcja, krok 2 pkt 6: „partycja plików będzie sformatowana pod firmware w jego pierwszym uruchomieniu (T-WYM-84), plik
// testowy `/p148.bin` zniknie”. Szkic zostawia poprawny LittleFS, a firmware przy zamontowanej partycji nie formatuje
// (src/dyrygent.cpp sprawdzPartycje(), decyzjaPartycji() → ZAMONTOWANA), więc 20 MB z 23,9 MB zostaje zajęte.
static void plikTestuPomiaru16() {
  Panel p;
  p.pliki.pliki["/p148.bin"] = "syntetyczny plik testu";
  p.zegar.epoch = ep(2026, 11, 6, 9, 0);
  p.zegar.msTeraz = 0;
  p.start.stan = StanRtc();
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  doPortu(p, "K");
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(p.pliki.formatowan == 0);  // zachowanie firmware (nie formatuje zamontowanej partycji): tak ma być, chroni noce
  // plik testu usuwa się przed wgraniem firmware (`pio run -t erase`), nie w firmware
  SPRAWDZ(p.pliki.pliki.count("/p148.bin"));
}


// ---------- poprawki ustawienia z raportu w każdym trybie, linie R-NOR-14 i R-DANE-29 ----------
static bool ekranMa(const Panel& p, const std::string& t) {
  for (auto& z : p.d.z)
    if (z.find(t) != std::string::npos) return true;
  return false;
}

// ASVAuto co do bajta jak w tools/prototyp/historia.py (panele w terenie mają te teksty w historii); R-NOR-15
static void formatyUstawien() {
  long d = nrDnia("2026-11-06");
  auto ust = [&](const std::string& pola, const char* tryb) { return nocZRaportu(raport(d, 12, 15, pola), d, tryb).noc.ustawienia; };
  SPRAWDZ(ust(pole("MOP", "ASVAuto", 8) + "," + pole("EAI", "6") + "," + pole("EAX", "10") + "," + pole("ANS", "3") + "," +
              pole("AXS", "15"), "ASVAuto") == "ASVAuto/6/10/3/15");
  SPRAWDZ(ust(pole("MOP", "ASVAuto", 8) + "," + pole("EAI", "6.0 cmH2O") + "," + pole("EAX", "10") + "," + pole("ANS", "3") +
              "," + pole("AXS", "15") + "," + pole("EPR", "2"), "ASVAuto") == "ASVAuto/6/10/3/15");  // EPR pomijany
  SPRAWDZ(ust(pole("MOP", "ASVAuto", 8) + "," + pole("EAI", "6") + "," + pole("ANS", "3") + "," + pole("AXS", "15"),
              "ASVAuto") == "ASVAuto/6//3/15");  // brak tagu w ASVAuto: jak dotąd
  SPRAWDZ(ust(pole("MOP", "VAuto"), "VAuto") == "VAuto////");
  SPRAWDZ(ust(pole("EAI", "6") + "," + pole("EAX", "10") + "," + pole("ANS", "3") + "," + pole("AXS", "15"), "ASVAuto") ==
          "/6/10/3/15");
  SPRAWDZ(ust("", "ASVAuto").empty());
  // nowe formaty
  SPRAWDZ(ust(ustAutoSet("6.0 cmH2O", "14.0 cmH2O", "3"), "AutoSet") == "AutoSet/6/14/3");
  SPRAWDZ(ust(pole("MOP", "CPAP", 0) + "," + pole("IPC", "9") + "," + pole("EPR", "2"), "CPAP") == "CPAP/9/2");
  SPRAWDZ(ust(pole("MOP", "ASV", 7) + "," + pole("EEP", "6") + "," + pole("MNS", "3") + "," + pole("MXS", "15"), "ASV") ==
          "ASV/6/3/15");
  // R-NOR-15: brak któregokolwiek tagu to brak odczytu, nie zmiana
  SPRAWDZ(ust(pole("MOP", "AutoSet", 1) + "," + pole("MPI", "6") + "," + pole("EPR", "3"), "AutoSet").empty());
  SPRAWDZ(ust(pole("MOP", "CPAP", 0) + "," + pole("EPR", "2"), "CPAP").empty());
  SPRAWDZ(ust(pole("MOP", "ASV", 7) + "," + pole("EEP", "6") + "," + pole("MXS", "15"), "ASV").empty());
  SPRAWDZ(ust(pole("MOP", "AutoSet", 1) + "," + pole("MPI", "x") + "," + pole("MPA", "14") + "," + pole("EPR", "3"), "AutoSet").empty());
  // tryb rekordu zostaje, choć ustawienia są puste (K1 i reguły biorą go z `tryb`)
  SPRAWDZ(nocZRaportu(raport(d, 12, 15, pole("MOP", "AutoSet", 1) + "," + pole("EPR", "3")), d, "AutoSet").noc.tryb == "AutoSet");
}

// Punkt 2: 25 nocy AutoSet prawdziwą ścieżką (raport Załącznika A → nocZRaportu() → rekord → silnik); lekarz zmienia MPA po
// 20 nocach. Norma ciśnienia zerowana, E-36 nie wchodzi; ta sama historia z tekstem ustawień podstawionym (bez zmiany) ma
// normę, więc test rozróżnia zmianę ustawień od jej braku.
static void zmianaMpaPrawdziwaSciezka() {
  reguly::zarejestrujRodzajeE();
  wzorce::zarejestrujRodzajeL();
  odkrycia::zarejestrujRodzajeO();
  long d0 = nrDnia("2026-10-01");
  for (int wariant = 0; wariant < 2; wariant++) {
    std::vector<Noc> h;
    for (int i = 0; i < 25; i++) {
      bool po = i >= 20;
      Noc n = nocZRaportu(raport(d0 + i, po ? 12.5 : 9.5, po ? 15.5 : 12.0 + 0.01 * (i % 5),
                                 po ? ustAutoSet("6", "18", "3") : ustAutoSet("6", "14", "3"), 25200 + i * 30),
                          d0 + i, "AutoSet", "AirSense_10_AutoSet", "1").noc;
      SPRAWDZ(n.ustawienia == (po ? "AutoSet/6/18/3" : "AutoSet/6/14/3"));
      if (wariant == 1) n.ustawienia = "AutoSet/6/14/3";  // podstawiony tekst: bez zmiany ustawień
      h.push_back(n);
    }
    std::string dzis = h.back().data;
    std::set<std::string> wyk;
    Stany st = liczStany(h, dzis, wyk);
    if (wariant == 0) {
      SPRAWDZ(!st.norma.cp95.jest());  // zmiana MPA: norma ciśnienia od zera, 5 nocy to za mało
      SPRAWDZ(!st.e36 && !st.cp95.aktywny);
    } else {
      SPRAWDZ(st.norma.cp95.jest());  // bez zmiany ustawień norma stoi (25 nocy), więc test jest czuły
    }
  }
}

// Linie R-NOR-14 i R-DANE-29 na ekranie dnia (przebieg dyrygenta), jednorazowo.
static void liniaZmianyIserii() {
  Sym s;
  long d0 = nrDnia("2026-10-20");
  s.wybudz(ep(2026, 10, 20, 14, 0), PowodWybudzenia::START);
  auto noc = [&](int i, const std::string& ust) { s.raporty[d0 + i] = raport(d0 + i, 9.5, 12, ust, 25200 + i * 20); };
  auto rano = [&](int i) { s.wybudz(ep(2026, 10, 21 + i, 6, 30), PowodWybudzenia::ZEGAR); };
  for (int i = 0; i < 16; i++) {
    noc(i, ustAutoSet("6", "14", "3"));
    rano(i);
    SPRAWDZ(!ekranMa(s.p, "Nowe ustawienia aparatu"));
  }
  noc(16, ustAutoSet("6", "18", "3"));
  rano(16);
  SPRAWDZ(ekranMa(s.p, "Nowe ustawienia aparatu. Uczę się normy."));
  SPRAWDZ(!ekranMa(s.p, "Nowe ustawienia: "));  // bez wartości (ADR-0002)
  noc(17, ustAutoSet("6", "18", "3"));
  rano(17);
  SPRAWDZ(!ekranMa(s.p, "Nowe ustawienia aparatu"));  // raz dla tej zmiany
  // nocy bez odczytu ustawień nie traktuje jako zmiany (R-NOR-15)
  noc(18, "");
  rano(18);
  SPRAWDZ(!ekranMa(s.p, "Nowe ustawienia aparatu"));
  noc(19, ustAutoSet("6", "18", "3"));
  rano(19);
  SPRAWDZ(!ekranMa(s.p, "Nowe ustawienia aparatu"));
  // R-DANE-29: trzy noce VAuto → jedna linia trybu, bez linii `Nowe ustawienia`; noc w trybie obsługiwanym kończy serię
  for (int i = 20; i < 23; i++) {
    noc(i, pole("MOP", "VAuto"));
    rano(i);
    SPRAWDZ(!ekranMa(s.p, "Nowe ustawienia aparatu"));
    SPRAWDZ(ekranMa(s.p, "Tryb aparatu VAuto: ta wersja go nie ocenia.") == (i == 20));
  }
}

// i R-DANE-29: tryb nieznany nie wyłącza zdarzeń z listy wyjątków (E-10 itd.), reszta „nie oceniam”.
static void wyjatkiTrybuNieznanego() {
  reguly::zarejestrujRodzajeE();
  const reguly::Zestaw& z = reguly::zestawWbudowany();
  std::vector<Noc> h;
  long d0 = nrDnia("2026-10-01");
  for (int i = 0; i < 3; i++) {
    NocAB a = nocZRaportu(raport(d0 + i, 9.5, 12, ""), d0 + i, "", "AirSense_10_AutoSet");
    h.push_back(a.noc);
  }
  std::string dzis = h.back().data;
  std::set<std::string> wyk;
  Stany st = liczStany(h, dzis, wyk);
  reguly::Kontekst k;
  k.historia = &h;
  k.dzis = dzis;
  k.norma = &st.norma;
  k.stany = &st;
  k.trybBiezacy = reguly::Tryb::NIEZNANY;
  static const reguly::Odpowiedzi odp;
  static const reguly::Pamiec pam;
  k.wykluczoneNoce = &wyk;
  k.odpowiedzi = &odp;
  k.pamiec = &pam;
  k.zestaw = &z;
  bool wyjatek = false, reszta = false;
  for (auto& r : reguly::ocen(z, k)) {
    bool nieznany = r.powodNieoceny == "tryb aparatu nieznany";
    if (r.id == "E-10" || r.id == "E-01" || r.id == "E-03" || r.id == "E-04" || r.id == "E-05" || r.id == "E-07" ||
        r.id == "E-11" || r.id == "E-12" || r.id == "E-20" || r.id == "E-22") {
      SPRAWDZ(!nieznany);
      wyjatek = true;
    }
    if (r.id == "E-30" || r.id == "E-36" || r.id == "L-01") {
      SPRAWDZ(nieznany);
      reszta = true;
    }
  }
  SPRAWDZ(wyjatek && reszta);
}

// przez dyrygenta: noc z `settings.available = false` po nocach z odczytem nie wyłącza reguł dnia.
static void nocBezOdczytu() {
  Sym s;
  long d0 = nrDnia("2026-10-20");
  s.wybudz(ep(2026, 10, 20, 14, 0), PowodWybudzenia::START);
  for (int i = 0; i < 3; i++) {
    s.raporty[d0 + i] = raport(d0 + i, 9.5, 12, ustAutoSet("6", "14", "3"));
    s.wybudz(ep(2026, 10, 21 + i, 6, 30), PowodWybudzenia::ZEGAR);
  }
  s.raporty[d0 + 3] = raport(d0 + 3, 9.5, 12, "");
  s.wybudz(ep(2026, 10, 24, 6, 30), PowodWybudzenia::ZEGAR);
  SPRAWDZ(!logMa(s.p, "tryb aparatu nieznany"));
}

// (R-MAS-16, R-MAS-19): nowy model maski zapisany formularzem (`sp_model` = data dnia zapisu): od następnego
// ranka jednorazowo linia `Nowy model maski. Uczę się normy.`; po cofnięciu (`sp_model` usunięty) linii nie ma.
static void liniaNowegoModelu() {
  for (bool cofniety : {false, true}) {
    Sym s;
    long d0 = nrDnia("2026-10-20");
    s.wybudz(ep(2026, 10, 20, 14, 0), PowodWybudzenia::START);
    auto noc = [&](int i) { s.raporty[d0 + i] = raport(d0 + i, 9.5, 12, ustAutoSet("6", "14", "3"), 25200 + i * 20); };
    for (int i = 0; i < 16; i++) {
      noc(i);
      s.wybudz(ep(2026, 10, 21 + i, 6, 30), PowodWybudzenia::ZEGAR);
    }
    if (!cofniety) s.p.nvs.teksty["cpap/sp_model"] = "2026-11-05";  // zapis z formularza 5.11 (dzień 15)
    noc(16);
    s.wybudz(ep(2026, 11, 6, 6, 30), PowodWybudzenia::ZEGAR);
    SPRAWDZ(ekranMa(s.p, "Nowy model maski. Uczę się normy.") == !cofniety);
  }
}

int main(int argc, char** argv) {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  std::string repo = argc > 1 ? argv[1] : "";
  reguly::zarejestrujRodzajeE();
  wzorce::zarejestrujRodzajeL();
  odkrycia::zarejestrujRodzajeO();
  rekordAutoSet();
  ustawieniaBezWartosci();
  e36PoZmianieZakresu();
  zeroNocy();
  pierwszeDni();
  liniaTrybu(repo);
  konfiguracjaW();
  plikTestuPomiaru16();
  formatyUstawien();
  zmianaMpaPrawdziwaSciezka();
  liniaZmianyIserii();
  liniaNowegoModelu();
  wyjatkiTrybuNieznanego();
  nocBezOdczytu();
  printf("testy_aparat_autoset_brzegowe: %d ok, %d błędów, %d oczekujących\n", ok, zle, oczekuje);
  // OCZEKUJE nie blokuje (ten program jest też w TESTY z zrodla.sh, czyli w sanitizerach tools/jakosc/sprawdz.sh)
  return zle ? 1 : 0;
}
