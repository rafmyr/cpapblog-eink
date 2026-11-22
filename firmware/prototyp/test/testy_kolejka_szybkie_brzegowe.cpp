// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe szybkich naciśnięć (R-PRZ-20, 22.11.2026) po zmianie czuwanie() „najpierw kolejka, potem
// trzymany” i Przyciski::obsluzoneZeStanu().
// Testy niezależne od testy_kolejka_szybkie.cpp (wspólna jest tylko atrapa przycisków z próbkowaniem).
//
// Dawna usterka: kolejka wykonuje krótkie ▶ na ekranie dnia z pytaniem tak/nie (5.12.8), którego nikt nie widział:
//   zapis „tak” w /dane/odpowiedzi.csv. Spec R-PRZ-20 każe pominąć („pytanie”), R-PRZ-22 (tabela) twierdzi, że
//   „kolejka naciśnięć jest tam odrzucana”. zKolejkiWolno(DZIEN) nie widzi pytania. Testy testPytanie_*.
// Dawna usterka: naciśnięcie zaczęte w czasie rysowania i trzymane w chwili odczytu stanu omija
//   zKolejkiWolno: wykonane ze stanu na ekranie, którego w chwili wciśnięcia nie było. testRysowanie_*.
// Po poprawkach wszystkie zielone.
// Reszta: serie losowe wobec wzorca „każde osobno” (naciśnięcie odrzucane z kolejki, tu ↻ na S5, kończy
// kolejkę: wzorzec bez bloku naciśnięć od niego; serie bez odrzucanych: dokładnie jak pojedynczo), przepełnienie
// kolejki, skrót ◀ + ▶, zacięcie, progi 2 s, trzy szybkie ▶/◀ w kokpicie z danymi.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "nawigacja.h"
#include "sprawy.h"
#include "start.h"
#include "szybkosc.h"
#include "wspolne.h"
#include "wybudzenie.h"
#include "zestaw_kolejki.h"

using reguly::StanReguly;
using reguly::Wynik;

static int ok = 0, zle = 0;
static const char* biezacyTest = "";
#define SPRAWDZ(w)                                                                     \
  do {                                                                                 \
    if (w) ok++;                                                                       \
    else { zle++; printf("BŁĄD [%s] %s:%d: %s\n", biezacyTest, __FILE__, __LINE__, #w); } \
  } while (0)

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t ep(int r, int mi, int d, int h, int m, int s = 0) {
  struct tm t = {};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
const uint32_t OBSLUGA_MS = 550, RYSOWANIE_MS = 1500;  // jak w testy_kolejka_szybkie.cpp (log z panelu 22.11.2026)

// ---------- stanowisko: atrapa przycisków z próbkowaniem, zegar przesuwany przez obsługę i rysowanie ----------
class EkranZCzasem : public AtrapaWyswietlacza {
 public:
  EkranZCzasem(Dziennik& d, AtrapaZegara& z) : AtrapaWyswietlacza(d), z_(z) {}
  std::function<void(int)> hak;  // przed upływem RYSOWANIE_MS
  int wPrzebiegu = 0;
  void odswiez() override {
    AtrapaWyswietlacza::odswiez();
    wPrzebiegu++;
    if (hak) hak(wPrzebiegu);
    z_.msTeraz += RYSOWANIE_MS;
  }
 private:
  AtrapaZegara& z_;
};
class PrzyciskiZHakiem : public AtrapaPrzyciskowProbkowanych {
 public:
  explicit PrzyciskiZHakiem(AtrapaZegara& z) : AtrapaPrzyciskowProbkowanych(z) {}
  std::function<void(int)> poZgaszeniu;
  int zgaszen = 0;
  void dioda(bool s) override {
    AtrapaPrzyciskowProbkowanych::dioda(s);
    if (!s && poZgaszeniu) poZgaszeniu(++zgaszen);
  }
};
struct Stanowisko {
  Panel p;
  PrzyciskiZHakiem prz{p.zegar};
  EkranZCzasem ekr{p.d, p.zegar};
  std::function<void(int)> poObsludze;  // przed upływem OBSLUGA_MS
  int obslug = 0;
  Adaptery ad() {
    return Adaptery{p.zegar, p.nvs, p.pliki, p.siec, ekr, prz, p.czujniki, p.dzwiek, p.start, p.straznik, p.sen,
                    p.port, p.diag, &p.wydania};
  }
  Stanowisko() {
    p.zegar.epoch = ep(2026, 11, 4, 10, 0);
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.pliki.pliki["/konfiguracja.txt"] = "";
  }
  uint32_t teraz() const { return p.zegar.msTeraz; }
  void nacisnij(uint8_t bit, uint32_t od, uint32_t trzymaj) { prz.nacisnij(bit, od, od + trzymaj); }
  bool wLogu(const std::string& t) const {
    for (const std::string& w : p.port.log)
      if (w.find(t) != std::string::npos) return true;
    return false;
  }
  void zrzucLog() const {
    for (const std::string& w : p.port.log) printf("      %s\n", w.c_str());
  }
};
static Stanowisko* biezace = nullptr;
static void haczyk(const char* nazwa) {
  if (!biezace || strcmp(nazwa, "przycisk") != 0) return;
  int nr = ++biezace->obslug;
  if (biezace->poObsludze) biezace->poObsludze(nr);
  biezace->p.zegar.msTeraz += OBSLUGA_MS;
}

// wybudzenie przyciskiem `bit` po głębokim śnie, trzymany od startu `trzymaj` ms; `dodatkowe` zaplanowane z góry
struct Nac {
  uint8_t bit;
  uint32_t od, trzymaj;
};
static void wybudz(Stanowisko& s, uint32_t kiedy, uint8_t bit, uint32_t trzymaj, const std::vector<Nac>& dodatkowe = {},
                   PowodWybudzenia w = PowodWybudzenia::PRZYCISK) {
  Panel& p = s.p;
  p.zegar.epoch = kiedy;
  p.zegar.msTeraz = 0;
  p.d.z.clear();
  p.port.log.clear();
  p.sen.snow = 0;
  p.start.przyczynaStartu = w == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = w;
  p.start.budzace = w == PowodWybudzenia::PRZYCISK ? bit : 0;
  p.start.trwanieBudzacego = 0;
  s.prz.odcinki.clear();
  if (w == PowodWybudzenia::PRZYCISK) s.nacisnij(bit, 0, trzymaj);
  for (const Nac& n : dodatkowe) s.nacisnij(n.bit, n.od, n.trzymaj);
  s.prz.start();
  s.prz.zgaszen = 0;
  s.ekr.wPrzebiegu = 0;
  s.obslug = 0;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  biezace = &s;
  szybkosc::haczykFazy = haczyk;
  Dyrygent(s.ad(), KONF).wybudzenie();
  szybkosc::haczykFazy = nullptr;
  biezace = nullptr;
}
static Ekran ekran(const Stanowisko& s) { return (Ekran)s.p.start.stan.ekran; }
// 12:05: poza oknem porannym, bez pobrania po przycisku (R-PRZ-22); 10:05: z nim
static const int GODZ = 12;
static Ekran osobno(const std::vector<uint8_t>& bity, Ekran start = Ekran::DZIEN, int godz = GODZ) {
  Stanowisko w;
  w.p.start.stan.ekran = (uint8_t)start;
  uint32_t t = ep(2026, 11, 4, godz, 0);
  for (uint8_t b : bity) wybudz(w, t += 30, b, 120);
  return ekran(w);
}
// to samo z ekranem przed każdym naciśnięciem i po ostatnim (tr[0] = start, tr[n] = koniec); `odrzucane[j]`: naciśnięcie
// j wykonane pojedynczo nie jest samą nawigacją (dziś w tych seriach: ↻ na ekranie dnia S5 bez danych = ponowienie
// pobrania z Wi-Fi), więc z kolejki albo z czasu rysowania panel je pomija razem z resztą kolejki (R-PRZ-20)
static std::vector<Ekran> trajektoria(const std::vector<uint8_t>& bity, Ekran start, int godz,
                                      std::vector<bool>& odrzucane) {
  Stanowisko w;
  w.p.start.stan.ekran = (uint8_t)start;
  uint32_t t = ep(2026, 11, 4, godz, 0);
  std::vector<Ekran> tr{start};
  odrzucane.clear();
  for (uint8_t b : bity) {
    Ekran przed = ekran(w);
    wybudz(w, t += 30, b, 120);
    // stanowisko bez danych: ekran dnia to S5, ↻ na nim ponawia pobranie (mapa 5.6.2); inne naciśnięcia w tych
    // seriach (▶ ◀ na ekranie dnia i stronach kokpitu, ↻ na stronie) to sama nawigacja
    odrzucane.push_back(b == BIT_SRODEK && przed == Ekran::DZIEN);
    tr.push_back(ekran(w));
  }
  return tr;
}

// ---------- dzień z pytaniem tak/nie (niedziela 17:05, pytanie o senność, jak testy_kolejka_linii.cpp) ----------
static const char* STATUS =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static std::string raportNocy(long day) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static reguly::Zestaw ZESTAW_D = atrapa_b5::zestaw();
static std::vector<Wynik> wynikiD(const reguly::Kontekst&) {
  std::vector<Wynik> w;
  for (auto& k : ZESTAW_D.katalog)
    w.push_back(atrapa_b5::wynik(ZESTAW_D, k.id, k.wariant, k.stan != StanReguly::ZABLOKOWANA, false,
                                 {{"x", "12,4"}, {"n", "3"}, {"min", "42"}, {"m", "27"}}));
  return w;
}
// start zasilania w niedzielę 17:05 z nocą w AirBridge: ekran dnia = pytanie o senność
static void dzienZDanymi(Stanowisko& s, const char* data, uint32_t start) {
  sprawy::zrodloTestowe.wyniki = wynikiD;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_D;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_D);
  long doba = nrDnia(data) - 1;
  s.p.siec.dodaj("/api/status", 200, STATUS);
  s.p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[{\"day\":" + std::to_string(doba) +
                                           ",\"session_count\":1,\"stale\":false}]}");
  s.p.siec.dodaj(sciezkaNocy(doba), 200, raportNocy(doba));
  wybudz(s, start, 0, 0, {}, PowodWybudzenia::START);
}
static void dzienZPytaniem(Stanowisko& s) {
  sprawy::zrodloTestowe.wyniki = wynikiD;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_D;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_D);
  long doba = nrDnia("2026-11-08") - 1;
  s.p.siec.dodaj("/api/status", 200, STATUS);
  s.p.siec.dodaj(sciezkaListy(), 200, "{\"source\":\"edf\",\"days\":[{\"day\":" + std::to_string(doba) +
                                           ",\"session_count\":1,\"stale\":false}]}");
  s.p.siec.dodaj(sciezkaNocy(doba), 200, raportNocy(doba));
  wybudz(s, ep(2026, 11, 8, 17, 5), 0, 0, {}, PowodWybudzenia::START);
}
static bool zapisanoOdpowiedz(Stanowisko& s) {
  return s.p.pliki.pliki["/dane/odpowiedzi.csv"].find("sennosc;") != std::string::npos;
}

// ---------- kolejka na ekranie dnia z pytaniem tak/nie ----------
// wymaganie: R-PRZ-20 („pierwsze zapamiętane naciśnięcie poza tym (… pytanie …) panel pomija”), R-PRZ-22 (tabela)
static void testKontrolaBezPytania() {
  biezacyTest = "kontrola: K1 ↻ ▶ bez pytania";
  Stanowisko s;
  s.p.start.stan.ekran = (uint8_t)Ekran::K1;
  s.poObsludze = [&](int nr) {
    if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 100, 120);
  };
  wybudz(s, ep(2026, 11, 4, GODZ, 5), BIT_SRODEK, 120);
  SPRAWDZ(s.obslug == 2);
  SPRAWDZ(ekran(s) == osobno({BIT_SRODEK, BIT_PRAWY}, Ekran::K1));
}
static void testPytanie_wybudzenie() {
  biezacyTest = "przypadek 1a: ↻ z K1 budzi na pytanie, ▶ w czasie liczenia";
  Stanowisko s;
  dzienZPytaniem(s);
  SPRAWDZ(s.wLogu("ekran dnia: pytanie sennosc"));
  s.p.start.stan.ekran = (uint8_t)Ekran::K1;
  s.poObsludze = [&](int nr) {
    if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 100, 120);  // ▶ puszczone, zanim pytanie się narysowało
  };
  wybudz(s, ep(2026, 11, 8, 17, 6), BIT_SRODEK, 120);
  bool zapis = zapisanoOdpowiedz(s);
  if (zapis) {
    printf("  przypadek 1a: odpowiedź „%s” zapisana z kolejki na niewidzianym pytaniu, ekran %d\n",
           s.p.pliki.pliki["/dane/odpowiedzi.csv"].c_str(), (int)ekran(s));
    for (const std::string& w : s.p.port.log)
      if (w.find("kolejka") != std::string::npos || w.find("pytanie") != std::string::npos) printf("      %s\n", w.c_str());
  }
  SPRAWDZ(!zapis);
  SPRAWDZ(ekran(s) == Ekran::DZIEN);
}
static void testPytanie_czuwanie() {
  biezacyTest = "przypadek 1b: w czuwaniu ↻ ze stanu na pytanie, ▶ w czasie liczenia (okno otwarte przez kolejkę z czasu liczenia)";
  Stanowisko s;
  dzienZPytaniem(s);
  s.p.start.stan.ekran = (uint8_t)Ekran::K2;
  s.prz.poZgaszeniu = [&](int nr) {
    if (nr == 1) s.nacisnij(BIT_SRODEK, s.teraz() + 200, 120);
  };
  s.poObsludze = [&](int nr) {
    if (nr == 2) s.nacisnij(BIT_PRAWY, s.teraz() + 100, 120);
  };
  wybudz(s, ep(2026, 11, 8, 17, 6), BIT_LEWY, 120);  // K2 ◀ → K1, potem ↻ → pytanie
  bool zapis = zapisanoOdpowiedz(s);
  if (zapis)
    printf("  przypadek 1b: odpowiedź „%s” zapisana z kolejki, ekran %d, obsłużonych %d\n",
           s.p.pliki.pliki["/dane/odpowiedzi.csv"].c_str(), (int)ekran(s), s.obslug);
  SPRAWDZ(!zapis);
}
// ---------- naciśnięcie zaczęte w czasie rysowania pytania, trzymane w chwili odczytu stanu ----------
static void testRysowanie_trzymanyZRysowania() {
  biezacyTest = "▶ wciśnięte w czasie rysowania pytania, trzymane po nim";
  Stanowisko s;
  dzienZPytaniem(s);
  s.p.start.stan.ekran = (uint8_t)Ekran::K1;
  s.ekr.hak = [&](int nr) {
    if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 1300, 400);  // trzymane jeszcze 200 ms po końcu rysowania
  };
  wybudz(s, ep(2026, 11, 8, 17, 6), BIT_SRODEK, 120);
  bool zapis = zapisanoOdpowiedz(s);
  if (zapis) printf("  odpowiedź „%s” zapisana ze stanu przycisku\n", s.p.pliki.pliki["/dane/odpowiedzi.csv"].c_str());
  SPRAWDZ(!zapis);
}

// ---------- serie losowe: 2–8 krótkich naciśnięć ▶ ◀ ↻ z losowymi odstępami wobec „każde osobno” ----------
// wymaganie: R-PRZ-20. Naciśnięcia nie zachodzą na siebie (jeden przycisk naraz), trzymanie 40–400 ms.
static uint32_t los = 12345;
static uint32_t losowa(uint32_t n) {
  los = los * 1103515245u + 12345u;
  return (los >> 8) % n;
}
static void testSerieLosowe() {
  biezacyTest = "serie losowe";
  int prob = getenv("RK2_PROB") ? atoi(getenv("RK2_PROB")) : 150;
  int zleSerie = 0, zOdrzuceniem = 0;
  const uint8_t B[3] = {BIT_LEWY, BIT_SRODEK, BIT_PRAWY};
  for (int i = 0; i < prob; i++) {
    int n = 2 + (int)losowa(7);
    bool tylkoPrawy = losowa(3) == 0;  // co trzecia seria same ▶ (jak w zgłoszeniu z panelu)
    std::vector<uint8_t> bity;
    for (int j = 0; j < n; j++) bity.push_back(tylkoPrawy ? BIT_PRAWY : B[losowa(3)]);
    Ekran start = losowa(2) ? Ekran::DZIEN : Ekran::K1;
    uint32_t h0 = 40 + losowa(360);
    std::vector<Nac> dalej;
    uint32_t t = h0;
    for (int j = 1; j < n; j++) {
      // połowa odstępów długa (panel zdąży narysować i czeka w czuwaniu: naciśnięcie ze stanu, po nim szybkie w czasie
      // liczenia strony, jak w zgłoszeniu z panelu), połowa krótka (kolejka)
      uint32_t przerwa = losowa(2) ? 2200 + losowa(1500) : 30 + losowa(870), trz = 40 + losowa(360);
      dalej.push_back({bity[j], t + przerwa, trz});
      t += przerwa + trz;
    }
    int godz = i % 4 == 3 ? 10 : GODZ;  // co czwarta seria rano bez nocy: pobranie po przycisku w środku serii
    std::vector<bool> odrz;
    std::vector<Ekran> tr = trajektoria(bity, start, godz, odrz);
    Ekran ocz = tr.back();
    bool zOdrzucanym = false;
    for (bool o : odrz) zOdrzucanym = zOdrzucanym || o;
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)start;
    wybudz(s, ep(2026, 11, 4, godz, 5), bity[0], h0, dalej);
    // pominięte wolno tylko ↻ na ekranie dnia (S5); każde inne odrzucenie to błąd
    int odrzucen = 0;
    bool obceOdrzucenie = false;
    for (const std::string& w : s.p.port.log)
      if (w.find("to nie sama nawigacja") != std::string::npos) {
        odrzucen++;
        if (w.find("↻") == std::string::npos || w.find("na ekranie 0") == std::string::npos) obceOdrzucenie = true;
      }
    bool dobrze;
    if (!zOdrzucanym || odrzucen == 0) {
      // bez naciśnięć odrzucanych (albo żadne nie wypadło z kolejki ani z czasu rysowania): dokładnie jak pojedynczo
      dobrze = s.obslug == n && ekran(s) == ocz && s.prz.kolejka.empty() && odrzucen == 0;
    } else {
      // odrzucone ↻ kończy kolejkę; ono i naciśnięcia po nim w tej samej kolejce przepadają. Wzorzec: te same
      // naciśnięcia pojedynczo bez jednego albo dwóch ciągłych bloków, z których każdy zaczyna się od ↻; liczba
      // obsłużonych = liczba zostawionych, ekran = ekran pojedynczych naciśnięć z tego podciągu
      dobrze = false;
      if (!obceOdrzucenie && s.prz.kolejka.empty() && s.obslug < n) {
        auto pasuje = [&](const std::vector<uint8_t>& pod) {
          return (int)pod.size() == s.obslug && (pod.empty() ? start : osobno(pod, start, godz)) == ekran(s);
        };
        for (int a = 0; a < n && !dobrze; a++) {
          if (bity[a] != BIT_SRODEK) continue;
          for (int b = a + 1; b <= n && !dobrze; b++) {
            std::vector<uint8_t> pod(bity.begin(), bity.begin() + a);
            pod.insert(pod.end(), bity.begin() + b, bity.end());
            if (odrzucen == 1 && (int)pod.size() == s.obslug) dobrze = pasuje(pod);
            for (int c = b; c < n && !dobrze && odrzucen >= 2; c++) {
              if (bity[c] != BIT_SRODEK) continue;
              for (int d = c + 1; d <= n && !dobrze; d++) {
                std::vector<uint8_t> pod2(bity.begin(), bity.begin() + a);
                pod2.insert(pod2.end(), bity.begin() + b, bity.begin() + c);
                pod2.insert(pod2.end(), bity.begin() + d, bity.end());
                if ((int)pod2.size() == s.obslug) dobrze = pasuje(pod2);
              }
            }
          }
        }
      }
      zOdrzuceniem++;
    }
    if (!dobrze && zleSerie < 6) {
      printf("  seria %d: start %d, n %d, obsłużonych %d, ekran %d (oczekiwany %d):", i, (int)start, n, s.obslug,
             (int)ekran(s), (int)ocz);
      printf(" [%d,0..%u]", bity[0], h0);
      for (const Nac& d : dalej) printf(" [%d,%u..%u]", d.bit, d.od, d.od + d.trzymaj);
      printf("\n");
      for (const std::string& w : s.p.port.log)
        if (w.find("kolejka") != std::string::npos || w.find("przerwane") != std::string::npos ||
            w.find("nie sama nawigacja") != std::string::npos ||
            w.find("zacięty") != std::string::npos)
          printf("      %s\n", w.c_str());
    }
    if (!dobrze) zleSerie++;
  }
  printf("  serie losowe: %d prób (z odrzuconym ↻ na S5: %d), %d niezgodnych z wzorcem\n", prob, zOdrzuceniem, zleSerie);
  SPRAWDZ(zleSerie == 0);
}

// ---------- przepełnienie: 9 naciśnięć w czasie jednego rysowania ----------
// wymaganie: R-PRZ-20 („zapamiętać naciśnięcie (najwyżej 8 [PRÓG])”)
static void testPrzepelnienie() {
  biezacyTest = "przepełnienie kolejki";
  Stanowisko s;
  s.ekr.hak = [&](int nr) {
    if (nr != 1) return;
    for (int j = 0; j < 9; j++) s.nacisnij(BIT_PRAWY, s.teraz() + 20 + (uint32_t)j * 160, 60);
  };
  wybudz(s, ep(2026, 11, 4, GODZ, 5), BIT_PRAWY, 100);
  int zKolejki = s.obslug - 1;
  printf("  przepełnienie: 9 naciśnięć w czasie rysowania, z kolejki wykonane %d (spec: najwyżej 8)\n", zKolejki);
  SPRAWDZ(zKolejki == 8);  // bufor 8 miejsc mieści 7 naciśnięć
}

// ---------- skrót ◀ + ▶ (R-PRZ-21) w czuwaniu: żadne z dwóch naciśnięć nie wraca z kolejki ----------
static void testSkrot() {
  // oba ≥ 2 s: U-03 (FORMULARZ, dalej jak ▶ 2 s), nic z kolejki
  {
    biezacyTest = "skrót ◀ + ▶ 2,5 s w czuwaniu";
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)Ekran::K2;
    s.prz.poZgaszeniu = [&](int nr) {
      if (nr != 1) return;
      s.nacisnij(BIT_LEWY, s.teraz() + 200, 2800);
      s.nacisnij(BIT_PRAWY, s.teraz() + 300, 2600);
    };
    wybudz(s, ep(2026, 11, 4, GODZ, 5), BIT_LEWY, 120);
    SPRAWDZ(s.wLogu("skrót ◀ + ▶ 2 s"));
    SPRAWDZ(!s.wLogu("kolejka naciśnięć"));
    SPRAWDZ(s.prz.kolejka.empty());
  }
  // ▶ puszczone po 0,5 s, ◀ trzymane dalej 3 s: bez akcji, ani ◀ (ani przytrzymanie ◀), ani ▶ z kolejki
  {
    biezacyTest = "skrót: ▶ puszczone wcześniej, ◀ trzymane 3 s";
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)Ekran::K2;
    s.prz.poZgaszeniu = [&](int nr) {
      if (nr != 1) return;
      s.nacisnij(BIT_LEWY, s.teraz() + 200, 3000);
      s.nacisnij(BIT_PRAWY, s.teraz() + 300, 500);
    };
    wybudz(s, ep(2026, 11, 4, GODZ, 5), BIT_LEWY, 120);
    SPRAWDZ(s.obslug == 1);
    SPRAWDZ(ekran(s) == Ekran::K1);
    SPRAWDZ(!s.wLogu("kolejka naciśnięć"));
  }
  // skrót w czasie rysowania (oba wciśnięte i puszczone przed końcem): z kolejki ◀ i ▶ jako nawigacja (dwa
  // naciśnięcia); zapis tego zachowania, bez oceny (spec R-PRZ-21 nie mówi o skrócie w czasie rysowania)
  {
    biezacyTest = "skrót w czasie rysowania";
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)Ekran::K2;
    s.ekr.hak = [&](int nr) {
      if (nr != 1) return;
      s.nacisnij(BIT_LEWY, s.teraz() + 100, 900);
      s.nacisnij(BIT_PRAWY, s.teraz() + 200, 700);
    };
    wybudz(s, ep(2026, 11, 4, GODZ, 5), BIT_LEWY, 120);
    printf("  skrót w czasie rysowania: obsłużonych %d, ekran %d, %s\n", s.obslug, (int)ekran(s),
           s.wLogu("kolejka naciśnięć: 2 wykonane") ? "z kolejki ◀ i ▶" : "inaczej");
  }
}

// ---------- próg przytrzymania 2 s (R-PRZ-08) na drogach: kolejka i stan ----------
static void testProgi() {
  // z kolejki: trzymanie 1990 ms krótkie (wykonane), 2010 ms przytrzymanie (pominięte); siatka próbek 5 ms
  for (uint32_t trz : {1990u, 2010u}) {
    biezacyTest = trz < 2000 ? "próg z kolejki 1990" : "próg z kolejki 2010";
    Stanowisko s;
    s.poObsludze = [&](int nr) {
      if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 50, trz);  // puszczone w czasie rysowania
    };
    wybudz(s, ep(2026, 11, 4, GODZ, 5), BIT_PRAWY, 120);
    if (trz < 2000) SPRAWDZ(s.wLogu("kolejka naciśnięć: 1 wykonane, 0 pominięte"));
    else SPRAWDZ(s.wLogu("kolejka naciśnięć: 0 wykonane, 1 pominięte"));
  }
  // ze stanu: K2 ▶ 2 s = raport; ▶ wciśnięte 500 ms przed końcem rysowania i trzymane łącznie 2100 ms.
  // przytrzymanie liczone od odczytu stanu, nie od wciśnięcia, więc wychodzi krótkie ▶ (następna strona)
  {
    biezacyTest = "przytrzymanie zaczęte w czasie rysowania";
    Stanowisko s;
    s.p.start.stan.ekran = (uint8_t)Ekran::K1;
    s.ekr.hak = [&](int nr) {
      if (nr == 1) s.nacisnij(BIT_PRAWY, s.teraz() + 1000, 2100);
    };
    wybudz(s, ep(2026, 11, 4, GODZ, 5), BIT_PRAWY, 120);  // K1 ▶ → K2, potem ▶ 2,1 s
    printf("  ▶ 2,1 s zaczęte w rysowaniu K2: ekran %d (K2=%d, raport=%d), obsłużonych %d\n",
           (int)ekran(s), (int)Ekran::K2, (int)Ekran::RAPORT, s.obslug);
    if (getenv("RK2_LOG")) s.zrzucLog();
  }
}

// ---------- zacięty przycisk budzący (T-WYM-29, R-PRZ-09): po puszczeniu nic z kolejki ----------
static void testZaciety() {
  biezacyTest = "zacięty ▶ budzący";
  Stanowisko s;
  wybudz(s, ep(2026, 11, 4, GODZ, 5), BIT_PRAWY, ZACIECIE_MS + 3000);
  SPRAWDZ(s.obslug == 1);
  SPRAWDZ(!s.wLogu("kolejka naciśnięć"));
}

// ---------- zgłoszenie z panelu (trzy szybkie ▶ / ◀ w kokpicie z danymi) ----------
// wymaganie: R-PRZ-20. Panel z nocą w AirBridge (środa, bez pytania tak/nie), ciąg ekran dnia → K1 → K2 → … → U-01
// i z powrotem; po głębokim śnie (budzi pierwsze) i w czuwaniu (pierwsze ze stanu); 12:05 i 07:40 (okno poranne,
// start slotu dziennego naciśnięciem, pobranie po przycisku).
static Ekran pojedynczo(int h, int m, Ekran start, const std::vector<uint8_t>& bity, uint8_t budzacy) {
  Stanowisko w;
  dzienZDanymi(w, "2026-11-04", ep(2026, 11, 4, h, m - 2));
  w.p.start.stan.ekran = (uint8_t)start;
  uint32_t t = ep(2026, 11, 4, h, m);
  if (budzacy) wybudz(w, t, budzacy, 120);  // jak w serii: najpierw przycisk budzący, potem naciśnięcia
  for (uint8_t b : bity) wybudz(w, t += 30, b, 120);
  return ekran(w);
}
static void testTrzySzybkieZDanymi() {
  struct Wariant {
    int h, m;
    Ekran start;
    uint8_t bit;
    bool zCzuwania;
  };
  const Wariant W[] = {{12, 5, Ekran::DZIEN, BIT_PRAWY, false}, {12, 5, Ekran::DZIEN, BIT_PRAWY, true},
                       {7, 40, Ekran::DZIEN, BIT_PRAWY, false}, {7, 40, Ekran::DZIEN, BIT_PRAWY, true},
                       {12, 5, Ekran::USTAWIENIA, BIT_LEWY, false}, {12, 5, Ekran::USTAWIENIA, BIT_LEWY, true},
                       {7, 40, Ekran::USTAWIENIA, BIT_LEWY, false}, {7, 40, Ekran::USTAWIENIA, BIT_LEWY, true}};
  for (const Wariant& v : W) {
    biezacyTest = "pkt 2: trzy szybkie z danymi";
    std::vector<uint8_t> trzy(3, v.bit);
    // w czuwaniu budzi ↻ (z ekranu dnia: bez zmiany) albo ◀ (z U-01: pierwszy krok wstecz)
    const uint8_t budzacy = v.zCzuwania ? (v.bit == BIT_LEWY ? BIT_LEWY : BIT_SRODEK) : 0;
    Ekran ocz = pojedynczo(v.h, v.m, v.start, trzy, budzacy);
    Stanowisko s;
    dzienZDanymi(s, "2026-11-04", ep(2026, 11, 4, v.h, v.m - 2));
    s.p.start.stan.ekran = (uint8_t)v.start;
    if (v.zCzuwania) {
      // w czuwaniu pierwsze ze stanu, drugie w czasie liczenia
      // strony, trzecie w czasie rysowania
      s.prz.poZgaszeniu = [&](int nr) {
        if (nr == 1) s.nacisnij(v.bit, s.teraz() + 200, 120);
      };
      s.poObsludze = [&](int nr) {
        if (nr == 2) s.nacisnij(v.bit, s.teraz() + 100, 120);
      };
      s.ekr.hak = [&](int nr) {
        if (nr == 2) s.nacisnij(v.bit, s.teraz() + 300, 120);
      };
      wybudz(s, ep(2026, 11, 4, v.h, v.m), budzacy, 120);
    } else {
      s.poObsludze = [&](int nr) {
        if (nr == 1) s.nacisnij(v.bit, s.teraz() + 100, 120);
      };
      s.ekr.hak = [&](int nr) {
        if (nr == 1) s.nacisnij(v.bit, s.teraz() + 300, 120);
      };
      wybudz(s, ep(2026, 11, 4, v.h, v.m), v.bit, 120);
    }
    int ocztObsl = v.zCzuwania ? 4 : 3;
    bool dobrze = ekran(s) == ocz && s.obslug == ocztObsl;
    printf("  pkt 2: %02d:%02d z ekranu %d, 3 × %s %s: obsłużonych %d z %d, ekran %d (pojedynczo %d)%s\n", v.h, v.m,
           (int)v.start, v.bit == BIT_PRAWY ? "▶" : "◀", v.zCzuwania ? "w czuwaniu" : "po głębokim śnie", s.obslug,
           ocztObsl, (int)ekran(s), (int)ocz, dobrze ? "" : "  <-- NIEZGODNE");
    if (!dobrze)
      for (const std::string& w : s.p.port.log)
        if (w.find("kolejka") != std::string::npos || w.find("nie sama") != std::string::npos ||
            w.find("slot dzienny") != std::string::npos)
          printf("      %s\n", w.c_str());
    SPRAWDZ(dobrze);
  }
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // strefa panelu, jak testy_kolejka_linii.cpp
  tzset();
  testKontrolaBezPytania();
  testSerieLosowe();
  testPrzepelnienie();
  testSkrot();
  testProgi();
  testZaciety();
  testTrzySzybkieZDanymi();
  testPytanie_wybudzenie();
  testPytanie_czuwanie();
  testRysowanie_trzymanyZRysowania();
  printf("testy_kolejka_szybkie_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
