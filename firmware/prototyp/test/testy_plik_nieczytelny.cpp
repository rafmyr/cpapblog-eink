// SPDX-License-Identifier: GPL-3.0-or-later
// Trwale nieczytelny plik danych na ekranie (decyzja właściciela projektu 22.11.2026; R-AW-40, T-WYM-19,
// T-TEST-520). Plik nocy roku jest na nośniku, a jego odczyt się nie udaje (AtrapaPlikow::nieczytelne):
//  N1. jedno wybudzenie z nieudanym odczytem nie robi sprawy (U-05 `Pamięć panelu: wolne …`, bez wpisu NVS);
//  N2. trzecie kolejne wybudzenie z nieudanym odczytem: U-05 `Pamięć panelu: nieczytelne od 22.11`, w werdykcie
//      `pamięć (noce 2026)`, NVS `cpap`/`nczyt` = `{ścieżka}@{chwila}`, jeden wiersz logu przy powstaniu;
//      czwarte wybudzenie bez nowego wiersza logu (NVS bez zapisu);
//  N3. zanik zasilania (RTC od nowa) przy wciąż nieczytelnym pliku: sprawa od razu, ta sama data „od” (z NVS);
//  N4. udany odczyt kończy sprawę: `Pamięć panelu: wolne …`, wpis NVS usunięty; nowa usterka liczy od zera;
//  N5. przerwana seria (nieudany, udany, nieudany, nieudany) nie robi sprawy;
//  N6. „Otwarte sprawy” w ciągu kokpitu: ▶ z K2 otwiera stronę spraw, a treść klatki (pod belką, nad stopką) to karta
//      `Nie odczytuję danych` z dokładnym tekstem; bez usterki ▶ z K2 nie otwiera „Otwartych spraw”;
//  N8. ścieżka czekania na kablu (ADR-0047): to samo na przebiegach bez głębokiego snu;
//  M1. rdzeń src/nieczytelne.h i teksty w szablonach (dopisane z poprawką);
//  N7. bez usterki klatki bajt w bajt jak na `main` 01494f38 (skróty CRC-32 klatek scenariusza zapisane z `main`;
//      przy zamierzonej zmianie wyglądu tych ekranów skróty trzeba zapisać na nowo: program wypisuje je przy różnicy).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "dzien.h"
#include "ekrany.h"
#include "kolejka.h"
#include "magazyn.h"
#include "nieczytelne.h"
#include "noc.h"
#include "pliki_pamiec.h"
#include "przebiegi.h"
#include "sprawy.h"
#include "stany.h"
#include "start.h"
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
    std::string a_ = (a), b_ = (b);                                                                     \
    if (a_ == b_) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: „%s” ≠ „%s”\n", __FILE__, __LINE__, a_.c_str(), b_.c_str()); } \
  } while (0)

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* const SC = "/dane/noce/2026.csv";
static const char* const KLUCZ_NVS = "cpap/nczyt";

static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900, t.tm_mon = mi - 1, t.tm_mday = d, t.tm_hour = h, t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static uint32_t rano(int h, int m) { return ep(2026, 11, 22, h, m); }

// Panel z 28 nocami (25.10…21.11) w pliku roku 2026, bez sieci.
static void przygotuj0(Panel& p) {
  p.pliki.montowane = true;
  Magazyn m(p.pliki, nullptr);
  std::vector<std::string> w;
  for (int i = 0; i < 28; i++) {
    Noc x;
    std::string l = dodajDni("2026-10-25", i) + ";1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;";
    if (!parsujNoc(l, 'A', x)) continue;
    x.zrodlo = 'A';
    w.push_back(wierszNocyV5(x));
  }
  m.zapisz("noce", SC, w);
  p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  p.pliki.montowane = false;
  p.siec.siecJest = false;
}

// teksty końcowe (Przebieg::tekstyKoncowe()) są tylko w testach; na panelu ich nie ma
// (`bezTekstowKoncowych`). Każdy panel testu ma bliźniaka bez tekstów końcowych, który dostaje te same wybudzenia
// i te same usterki: serie i sprawy liczą się na bliźniaku (jak na panelu), a wiersze U-05 bierzemy z tekstów
// panelu z tekstami. Po każdym wybudzeniu stan serii w RTC i klucz NVS obu muszą być bajt w bajt równe, więc teksty
// końcowe niczego w serii nie zmieniają.
static std::map<const Panel*, std::unique_ptr<Panel>> blizniaki;
static int rozbieznosci = 0;
static void przygotuj(Panel& p) {
  przygotuj0(p);
  blizniaki[&p].reset(new Panel());
  przygotuj0(*blizniaki[&p]);
}
static Panel* blizniak(const Panel& p) {
  auto i = blizniaki.find(&p);
  return i == blizniaki.end() ? nullptr : i->second.get();
}
static void porownajZBlizniakiem(const Panel& p) {
  const Panel* b = blizniak(p);
  if (!b) return;
  bool rtc = std::memcmp(&p.start.stan.nieczytelne, &b->start.stan.nieczytelne, sizeof p.start.stan.nieczytelne) == 0;
  auto n1 = p.nvs.teksty.find(KLUCZ_NVS), n2 = b->nvs.teksty.find(KLUCZ_NVS);
  bool nvs = (n1 == p.nvs.teksty.end()) == (n2 == b->nvs.teksty.end()) && (n1 == p.nvs.teksty.end() || n1->second == n2->second);
  if (!rtc || !nvs) {
    rozbieznosci++;
    printf("BŁĄD: stan serii z tekstami końcowymi ≠ jak na panelu (RTC %d, NVS %d)\n", (int)rtc, (int)nvs);
  }
}
static KonfiguracjaPanelu konfPanelu() {
  KonfiguracjaPanelu k = KONF;
  k.bezTekstowKoncowych = true;
  return k;
}
static void wybudzJeden(Panel& p, uint32_t t, bool zanik, const KonfiguracjaPanelu& k, TekstyWybudzenia* teksty) {
  p.port.log.clear();
  p.zegar.epoch = t;
  p.zegar.msTeraz = 0;
  if (zanik) p.start.stan = StanRtc();
  p.start.przyczynaStartu = zanik ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = zanik ? PowodWybudzenia::START : PowodWybudzenia::ZEGAR;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent dy(p.adaptery(), k);
  dy.wybudzenie();
  if (teksty) *teksty = dy.teksty();
}

// Jedno wybudzenie zegarem z uśpienia (RTC zostaje) albo start zasilania (`zanik`: RTC od nowa); bliźniak jak na panelu.
static TekstyWybudzenia wybudz(Panel& p, uint32_t t, bool zanik = false) {
  if (Panel* b = blizniak(p)) {
    b->pliki.nieczytelne = p.pliki.nieczytelne;
    wybudzJeden(*b, t, zanik, konfPanelu(), nullptr);
  }
  TekstyWybudzenia w;
  wybudzJeden(p, t, zanik, KONF, &w);
  porownajZBlizniakiem(p);
  return w;
}
// sprawy w RTC bliźniaka (jak na panelu, bez tekstów końcowych)
static size_t sprawJakPanel(const Panel& p) {
  const Panel* b = blizniak(p);
  return nieczytelne::sprawy((b ? *b : p).start.stan.nieczytelne).size();
}
static int zapisowNvs(const Panel& p) {
  int n = 0;
  for (auto& z : p.d.z) n += z == std::string("nvs.zapis ") + KLUCZ_NVS;
  return n;
}
// Wybudzenie przyciskiem `bit` na ekranie `e`; zwraca ekran po naciśnięciu.
static Ekran nacisnijJeden(Panel& p, uint32_t t, Ekran e, uint8_t bit, const KonfiguracjaPanelu& k) {
  p.port.log.clear();
  p.zegar.epoch = t;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = {{0, bit}, {300, 0}};
  p.start.stan.ekran = (uint8_t)e;
  p.start.stan.ekranOd = t - 10;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), k).wybudzenie();
  p.start.budzace = 0;
  p.przyciski.skrypt.clear();
  return (Ekran)p.start.stan.ekran;
}
static Ekran nacisnij(Panel& p, uint32_t t, Ekran e, uint8_t bit) {
  if (Panel* b = blizniak(p)) {
    b->pliki.nieczytelne = p.pliki.nieczytelne;
    nacisnijJeden(*b, t, e, bit, konfPanelu());
  }
  Ekran w = nacisnijJeden(p, t, e, bit, KONF);
  porownajZBlizniakiem(p);
  return w;
}
// Ostatni wiersz U-05 o nazwie `nazwa` (stan tuż przed snem), bez nazwy.
static std::string wiersz(const TekstyWybudzenia& t, const std::string& nazwa) {
  std::string w;
  for (auto& l : t.linieTestu)
    if (l.compare(0, nazwa.size() + 2, nazwa + ": ") == 0) w = l.substr(nazwa.size() + 2);
  return w;
}
static bool wLogu(const Panel& p, const std::string& t) {
  for (auto& l : p.port.log)
    if (l.find(t) != std::string::npos) return true;
  return false;
}
static bool nvsJest(const Panel& p) { return p.nvs.teksty.count(KLUCZ_NVS) > 0; }
static std::string nvs(const Panel& p) {
  auto i = p.nvs.teksty.find(KLUCZ_NVS);
  return i == p.nvs.teksty.end() ? "" : i->second;
}
static bool wolne(const std::string& v) { return v.compare(0, 6, "wolne ") == 0; }

// wymaganie: R-AW-40
// wymaganie: T-WYM-19
static void testN1N2N3N4_SeriaSprawaZanikKoniec() {
  Panel p;
  przygotuj(p);
  TekstyWybudzenia t = wybudz(p, rano(6, 50), true);  // start zasilania, plik czytelny
  SPRAWDZ(wolne(wiersz(t, "Pamięć panelu")));
  p.pliki.nieczytelne.insert(SC);
  // N1: jedno nieudane czytanie = bez sprawy
  t = wybudz(p, rano(7, 0));
  std::string w1 = wiersz(t, "Pamięć panelu");
  printf("N1 jedno wybudzenie z nieudanym odczytem: Pamięć panelu: %s; NVS %s\n", w1.c_str(), nvsJest(p) ? "jest" : "brak");
  SPRAWDZ(wolne(w1));
  SPRAWDZ(!nvsJest(p));
  SPRAWDZ(sprawJakPanel(p) == 0);
  SPRAWDZ(wiersz(t, "Werdykt").find("pamięć panelu") == std::string::npos);
  t = wybudz(p, rano(7, 10));
  SPRAWDZ(wolne(wiersz(t, "Pamięć panelu")) && !nvsJest(p));
  // N2: trzecie kolejne
  const int zapisyPrzed = zapisowNvs(p);
  t = wybudz(p, rano(7, 20));
  std::string w3 = wiersz(t, "Pamięć panelu"), wer = wiersz(t, "Werdykt");
  SPRAWDZ(sprawJakPanel(p) == 1);                // liczone jak na panelu (bez tekstów końcowych)
  SPRAWDZ(zapisowNvs(p) - zapisyPrzed == 1);     // jeden zapis `nczyt` przy powstaniu sprawy
  printf("N2 trzecie wybudzenie z rzędu: Pamięć panelu: %s; Werdykt: %s; NVS „%s”\n", w3.c_str(), wer.c_str(),
         nvs(p).c_str());
  ROWNE(w3, "nieczytelne od 22.11");
  // bez sieci także Wi-Fi i AirBridge; pas mieści dwie nazwy, trzecia ustępuje `…` (R-KONF-58, jak dotąd)
  ROWNE(wer, "3 rzeczy do sprawdzenia: pamięć (noce 2026), Wi-Fi, …");
  ROWNE(nvs(p), std::string(SC) + "@" + std::to_string(rano(7, 0)) + "\n");
  SPRAWDZ(wLogu(p, "pliki nieczytelne: noce/2026.csv od 22.11 07:00"));
  // czwarte: sprawa trwa, bez nowego zapisu NVS (bez wiersza logu powstania)
  const int zapisyTrzecie = zapisowNvs(p);
  t = wybudz(p, rano(7, 30));
  ROWNE(wiersz(t, "Pamięć panelu"), "nieczytelne od 22.11");
  SPRAWDZ(!wLogu(p, "pliki nieczytelne:"));
  SPRAWDZ(zapisowNvs(p) == zapisyTrzecie);  // sprawa trwa, 0 zapisów `nczyt`
  // N3: zanik zasilania (RTC od nowa), plik dalej nieczytelny: sprawa od razu, ta sama data
  t = wybudz(p, rano(7, 40), true);
  std::string wz = wiersz(t, "Pamięć panelu");
  printf("N3 po zaniku zasilania: Pamięć panelu: %s\n", wz.c_str());
  ROWNE(wz, "nieczytelne od 22.11");
  ROWNE(nvs(p), std::string(SC) + "@" + std::to_string(rano(7, 0)) + "\n");
  // N4: udany odczyt kończy sprawę
  p.pliki.nieczytelne.clear();
  t = wybudz(p, rano(7, 50));
  std::string wk = wiersz(t, "Pamięć panelu");
  printf("N4 udany odczyt: Pamięć panelu: %s; NVS %s\n", wk.c_str(), nvsJest(p) ? "jest" : "brak");
  SPRAWDZ(wolne(wk));
  SPRAWDZ(!nvsJest(p));
  SPRAWDZ(wLogu(p, "pliki nieczytelne: koniec sprawy"));
  // nowa usterka liczy od zera
  p.pliki.nieczytelne.insert(SC);
  t = wybudz(p, rano(8, 0));
  SPRAWDZ(wolne(wiersz(t, "Pamięć panelu")));
}

// wymaganie: R-AW-40
static void testN5_PrzerwanaSeria() {
  Panel p;
  przygotuj(p);
  wybudz(p, rano(6, 50), true);
  p.pliki.nieczytelne.insert(SC);
  wybudz(p, rano(7, 0));
  wybudz(p, rano(7, 10));
  p.pliki.nieczytelne.clear();
  wybudz(p, rano(7, 20));  // udany odczyt przerywa serię
  p.pliki.nieczytelne.insert(SC);
  wybudz(p, rano(7, 30));
  TekstyWybudzenia t = wybudz(p, rano(7, 40));
  std::string w = wiersz(t, "Pamięć panelu");
  printf("N5 seria przerwana udanym odczytem (2 + 2): Pamięć panelu: %s\n", w.c_str());
  SPRAWDZ(wolne(w) && !nvsJest(p));
  t = wybudz(p, rano(7, 50));
  ROWNE(wiersz(t, "Pamięć panelu"), "nieczytelne od 22.11");
  ROWNE(nvs(p), std::string(SC) + "@" + std::to_string(rano(7, 30)) + "\n");
}

// Treść klatki pod belką i nad stopką (wiersze 60…433) równa karcie `k` (1 z 1) narysowanej wprost.
static bool tresKarty(const std::vector<uint8_t>& klatka, const sprawy::KartaSprawy& k, const std::string& dzis) {
  sprawy::WejscieSpraw ws;
  ws.dzis = dzis;
  PlotnoAtrapy pl;
  rysujSprawy(pl, trescSpraw({k}, 1, ws));
  for (size_t y = 60; y < 434; y++)
    for (size_t x = 0; x < 100; x++)
      if (klatka[y * 100 + x] != pl.b[y * 100 + x]) return false;
  return true;
}

// wymaganie: R-AW-40
static void testN6_OtwarteSprawyWKokpicie() {
  {  // bez usterki: ▶ z K2 nie otwiera „Otwartych spraw”
    Panel p;
    przygotuj(p);
    wybudz(p, rano(6, 50), true);
    Ekran e = nacisnij(p, rano(7, 0), Ekran::K2, BIT_PRAWY);
    printf("N6 bez usterki: ▶ z K2 → %s\n", e == Ekran::SPRAWY ? "Otwarte sprawy" : "inna strona");
    SPRAWDZ(e != Ekran::SPRAWY);
  }
  Panel p;
  przygotuj(p);
  wybudz(p, rano(6, 50), true);
  p.pliki.nieczytelne.insert(SC);
  wybudz(p, rano(7, 0));
  wybudz(p, rano(7, 10));
  wybudz(p, rano(7, 20));
  Ekran e = nacisnij(p, rano(7, 30), Ekran::K2, BIT_PRAWY);
  sprawy::KartaSprawy k;
  k.tytul = "Nie odczytuję danych";
  k.powod = "Od 22.11 nie odczytuję: noce 2026. Nic tam nie zapisuję.";
  k.ocena = "częściowa: bez danych z tego pliku";
  k.ty = "po tygodniu daj znać osobie, która instalowała panel";
  k.dalej = "ponawiam odczyt przy każdym wybudzeniu";
  k.koniec = "po pierwszym udanym odczycie";
  bool tresc = e == Ekran::SPRAWY && !p.ekran.klatki.empty() && tresKarty(p.ekran.klatki.back(), k, "2026-11-22");
  printf("N6 z usterką: ▶ z K2 → %s, karta „Nie odczytuję danych” w klatce: %s\n",
         e == Ekran::SPRAWY ? "Otwarte sprawy" : "inna strona", tresc ? "tak" : "nie");
  SPRAWDZ(e == Ekran::SPRAWY);
  SPRAWDZ(tresc);
  // ◀ ze strony spraw wraca na K2, ▶ za ostatnią kartą prowadzi dalej (reguła przycisków bez zmian)
  SPRAWDZ(nacisnij(p, rano(7, 31), Ekran::SPRAWY, BIT_LEWY) == Ekran::K2);
  // po udanym odczycie strona znika z ciągu
  p.pliki.nieczytelne.clear();
  wybudz(p, rano(7, 40));
  SPRAWDZ(nacisnij(p, rano(7, 41), Ekran::K2, BIT_PRAWY) != Ekran::SPRAWY);
}

// N7: scenariusz bez usterki (start, 2 wybudzenia zegarem, ▶ przez kokpit, U-05) i skróty CRC-32 każdej klatki
static std::vector<uint32_t> skrotyBezUsterki() {
  Panel p;
  przygotuj(p);
  wybudz(p, rano(6, 50), true);
  wybudz(p, rano(7, 0));
  wybudz(p, rano(7, 10));
  Ekran e = Ekran::DZIEN;
  for (int i = 0; i < 4; i++) e = nacisnij(p, rano(7, 11 + i), e, BIT_PRAWY);
  nacisnij(p, rano(7, 20), Ekran::FORMULARZ, BIT_PRAWY);  // U-05 „Test sprzętu”
  std::vector<uint32_t> s;
  for (auto& k : p.ekran.klatki) s.push_back(crc32(k.data(), k.size()));
  return s;
}
// wymaganie: R-AW-40
static void testN7_BezUsterkiJakMain() {
  // skróty z `main` 01494f38 (ten sam scenariusz, zapisane przed zmianą)
  static const uint32_t MAIN[] = {0x3ADF58CDu, 0x3324D109u, 0xE391F1B1u, 0xFF55B6D2u, 0x0DEF1FE5u};
  std::vector<uint32_t> s = skrotyBezUsterki();
  const size_t n = sizeof MAIN / sizeof MAIN[0];
  bool rowne = s.size() == n;
  for (size_t i = 0; rowne && i < n; i++) rowne = s[i] == MAIN[i];
  printf("N7 bez usterki: %zu klatek, %s z main\n", s.size(), rowne ? "bajt w bajt" : "RÓŻNE");
  if (!rowne) {
    printf("   skróty tej wersji:");
    for (uint32_t x : s) printf(" 0x%08Xu,", (unsigned)x);
    printf("\n");
  }
  SPRAWDZ(rowne);
}

// N8: ścieżka czekania na kablu (ADR-0047): przebiegi bez głębokiego snu z pamięcią przebiegów i plików liczą się
// jak wybudzenia; sprawa po trzecim przebiegu, koniec po udanym odczycie
// wymaganie: R-AW-40
static void testN8_CzekanieNaKablu() {
  // bliźniak także na ścieżce czekania (panel z tekstami końcowymi i panel jak na kablu, bez nich)
  Panel p, b;
  przygotuj0(p);
  przygotuj0(b);
  PlikiZPamiecia pl(p.pliki), plb(b.pliki);
  PamiecPrzebiegow pp, ppb;
  for (Panel* x : {&p, &b}) {
    x->czujniki.zas = Zasilanie::ZEWNETRZNE;
    x->sen.czekanieWlaczone = true;
    x->ekran.pamietaObraz = true;
  }
  auto jeden = [](Panel& x, PlikiZPamiecia& f, PamiecPrzebiegow& m, const KonfiguracjaPanelu& k, uint32_t t, bool start,
                  TekstyWybudzenia* tw) {
    x.port.log.clear();
    x.zegar.epoch = t;
    x.zegar.msTeraz = 0;
    x.start.przyczynaStartu = start ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
    x.start.powodWybudzenia = start ? PowodWybudzenia::START : PowodWybudzenia::ZEGAR;
    if (start) x.start.stan = StanRtc();
    zerujPrzedSnem(x.start.l, identyfikatorWersji(x.start.napisWersji));
    Adaptery ad{x.zegar, x.nvs, f, x.siec, x.ekran, x.przyciski, x.czujniki, x.dzwiek, x.start, x.straznik,
                x.sen, x.port, x.diag, &x.wydania};
    Dyrygent dy(ad, k, &m);
    bool czekal = dy.wybudzenie();
    if (tw) *tw = dy.teksty();
    return czekal;
  };
  int rozne = 0;
  auto przebieg = [&](uint32_t t, bool start) {
    b.pliki.nieczytelne = p.pliki.nieczytelne;
    jeden(b, plb, ppb, konfPanelu(), t, start, nullptr);
    TekstyWybudzenia tw;
    bool czekal = jeden(p, pl, pp, KONF, t, start, &tw);
    if (std::memcmp(&p.start.stan.nieczytelne, &b.start.stan.nieczytelne, sizeof p.start.stan.nieczytelne) != 0 ||
        p.nvs.teksty.count(KLUCZ_NVS) != b.nvs.teksty.count(KLUCZ_NVS))
      rozne++;
    return std::make_pair(czekal, wiersz(tw, "Pamięć panelu"));
  };
  przebieg(rano(6, 50), true);
  p.pliki.nieczytelne.insert(SC);
  auto a = przebieg(rano(7, 0), false), c1 = przebieg(rano(7, 10), false), c = przebieg(rano(7, 20), false);
  printf("N8 czekanie na kablu: przebiegi z czekaniem %d %d %d; Pamięć panelu: %s → %s → %s\n", (int)a.first,
         (int)c1.first, (int)c.first, a.second.c_str(), c1.second.c_str(), c.second.c_str());
  SPRAWDZ(a.first && c1.first);  // naprawdę ścieżka bez głębokiego snu
  SPRAWDZ(wolne(a.second) && wolne(c1.second));
  ROWNE(c.second, "nieczytelne od 22.11");
  p.pliki.nieczytelne.clear();
  SPRAWDZ(wolne(przebieg(rano(7, 30), false).second) && !nvsJest(p));
  // dziennik baterii z usterką co drugi przebieg (teksty końcowe go czytają): stan bliźniaków równy
  for (int i = 0; i < 6; i++) {
    if (i % 2 == 0) p.pliki.nieczytelne.insert("/log/wybudzenia.csv");
    else p.pliki.nieczytelne.erase("/log/wybudzenia.csv");
    przebieg(ep(2026, 11, 23 + i, 7, 0), false);
  }
  printf("N8 bliźniak na kablu: rozbieżności stanu serii %d; spraw jak na kablu %zu\n", rozne,
         nieczytelne::sprawy(b.start.stan.nieczytelne).size());
  SPRAWDZ(rozne == 0);
}

// bliźniak w głębokim śnie z niekolejnymi usterkami dziennika baterii (teksty końcowe czytają dziennik):
// mutacja „teksty końcowe zmieniają stan” daje rozbieżność stanu serii bliźniaków
// wymaganie: T-WYM-19
static void testN11_BlizniakDziennikBaterii() {
  const int przed = rozbieznosci;
  Panel p;
  przygotuj(p);
  wybudz(p, rano(6, 0), true);
  wybudz(p, rano(6, 10));
  for (int i = 0; i < 6; i++) {
    if (i % 2 == 0) p.pliki.nieczytelne.insert("/log/wybudzenia.csv");
    else p.pliki.nieczytelne.erase("/log/wybudzenia.csv");
    wybudz(p, rano(7, 10 * i));
  }
  for (int i = 0; i < 4; i++) {  // także na nowych dobach (dziennik czytany raz na dobę)
    p.pliki.nieczytelne.insert("/log/wybudzenia.csv");
    wybudz(p, ep(2026, 11, 23 + i, 7, 0));
  }
  printf("N11 bliźniak, dziennik baterii z usterkami: nowych rozbieżności %d, spraw jak na panelu %zu\n",
         rozbieznosci - przed, sprawJakPanel(p));
  SPRAWDZ(rozbieznosci == przed);
  SPRAWDZ(sprawJakPanel(p) == 1);
  // plik czytany tylko przez teksty końcowe (porównanie nocy: wiersz U-05 „Noce z plików”, odczyt po wierszu pamięci,
  // a przed linią `Dane: …` diagnostyki, która odświeża serię): na panelu bez tekstów go nie ma, więc serii też nie;
  // mutacja „teksty końcowe zmieniają stan” (bez bliźniaka) daje tu rozbieżność
  Panel q;
  przygotuj(q);
  for (Panel* x : {&q, blizniak(q)}) {
    x->pliki.montowane = true;
    Magazyn(x->pliki, nullptr).zapisz("porownanie", "/dane/porownanie.csv", {});
    x->pliki.montowane = false;
  }
  wybudz(q, rano(6, 0), true);
  q.pliki.nieczytelne.insert("/dane/porownanie.csv");
  const int przedQ = rozbieznosci;
  for (int i = 0; i < 4; i++) wybudz(q, rano(7, 10 * i));
  printf("N11 plik czytany tylko przez teksty końcowe: nowych rozbieżności %d, spraw jak na panelu %zu, z tekstami %zu\n",
         rozbieznosci - przedQ, sprawJakPanel(q), nieczytelne::sprawy(q.start.stan.nieczytelne).size());
  SPRAWDZ(rozbieznosci == przedQ);
}

// (ADR-0029 pkt 7): 4 sprawy plików spoza oceny zajmują wszystkie miejsca (odtworzone z NVS po starcie),
// potem trwale nieczytelne czynniki: plik oceny wypiera sprawę spoza oceny, po 3 wybudzeniach jest sprawą i ocena
// jest niepełna
// wymaganie: R-AW-40
static void testN12_PlikOcenyMaMiejsce() {
  Panel p;
  przygotuj0(p);
  const char* poza[] = {"/dane/humor.csv", "/log/wybudzenia.csv", "/dane/portret.csv", "/dane/porownanie.csv"};
  p.pliki.montowane = true;
  Magazyn m(p.pliki, nullptr);
  m.zapisz("czynniki", "/dane/czynniki.csv", {});
  p.pliki.montowane = false;
  std::string t;
  uint32_t od = rano(5, 0);
  for (const char* x : poza) {
    if (!p.pliki.pliki.count(x)) p.pliki.pliki[x] = "#cpap-eink x 1\n";
    p.pliki.nieczytelne.insert(x);
    t += std::string(x) + "@" + std::to_string(od++) + "\n";
  }
  p.nvs.teksty[KLUCZ_NVS] = t;
  wybudzJeden(p, rano(6, 0), true, konfPanelu(), nullptr);
  const size_t przed = nieczytelne::sprawy(p.start.stan.nieczytelne).size();
  p.pliki.nieczytelne.insert("/dane/czynniki.csv");
  int wymuszona = 0;
  for (int i = 0; i < 3; i++) {
    wybudzJeden(p, rano(7, 10 * i), false, konfPanelu(), nullptr);
    wymuszona += wLogu(p, "ocena: niepełna, nieczytelny plik oceny");
  }
  nacisnijJeden(p, rano(7, 21), Ekran::DZIEN, BIT_PRAWY, konfPanelu());  // K1 liczy ocenę (jak N10)
  wymuszona += wLogu(p, "ocena: niepełna, nieczytelny plik oceny");
  bool czynniki = false;
  for (auto& x : nieczytelne::sprawy(p.start.stan.nieczytelne)) czynniki = czynniki || x.sciezka == "/dane/czynniki.csv";
  printf("N12 4 sprawy spoza oceny (%zu), potem czynniki: sprawa czynników %s, wymuszenie w %d (3 wybudzenia i K1)\n", przed,
         czynniki ? "jest" : "BRAK", wymuszona);
  SPRAWDZ(przed == 4);
  SPRAWDZ(czynniki);
  SPRAWDZ(wymuszona >= 1);
  SPRAWDZ(sprawy::kartaNieczytelnych({{"czynniki nocy", "22.11", true}, {"dziennik ekranów", "22.11", false}}).ocena ==
          std::string("częściowa: bez części danych"));
}

// wymuszenie jako czysta funkcja na Stany (z uspokojeniem spełnionym) i na ekranach przy przełączniku
// testowym „wszystkie punkty ocenione” (kolejka::testUspokojenieSpelnione): bez sprawy „Noc w normie.”, przy sprawie
// pliku oceny nie, przy sprawie pliku spoza oceny ekrany jak bez sprawy
static std::vector<uint32_t> ekranyPrzelacznik(const char* plik, Panel& p) {
  przygotuj0(p);
  p.pliki.montowane = true;
  Magazyn(p.pliki, nullptr).zapisz("czynniki", "/dane/czynniki.csv", {});
  p.pliki.montowane = false;
  wybudzJeden(p, rano(6, 0), true, konfPanelu(), nullptr);
  wybudzJeden(p, rano(6, 10), false, konfPanelu(), nullptr);
  if (plik) p.pliki.nieczytelne.insert(plik);
  for (int d : {23, 24, 25}) {
    // noce do wczoraj, więc ekran dnia to ekran danych z ostatnią nocą (bez S10 „noc nieobecna”)
    const bool zakaz = p.pliki.nieczytelne.erase(SC) > 0;
    p.pliki.montowane = true;
    std::vector<std::string> w;
    for (int i = 0; i < 40; i++) {
      Noc x;
      std::string l = dodajDni("2026-10-15", i + d - 23) + ";1;414;1.20;9.40;11.30;0.10;0.30;;;;;;K;0;0;;;;";
      if (!parsujNoc(l, 'A', x)) continue;
      x.zrodlo = 'A';
      w.push_back(wierszNocyV5(x));
    }
    p.pliki.pliki.erase(SC);  // nowy plik (bez odczytu w tym obiekcie Magazyn nie nadpisuje)
    Magazyn(p.pliki, nullptr).zapisz("noce", SC, w);
    p.pliki.montowane = false;
    if (zakaz) p.pliki.nieczytelne.insert(SC);
    wybudzJeden(p, ep(2026, 11, d, 7, 0), false, konfPanelu(), nullptr);
  }
  SPRAWDZ(wLogu(p, "ekran dnia S2A"));  // dowód spokoju, na którym bez sprawy jest „Noc w normie.”
  std::vector<uint32_t> k;
  k.push_back(crc32(p.ekran.klatki.back().data(), p.ekran.klatki.back().size()));  // ekran dnia
  nacisnijJeden(p, ep(2026, 11, 25, 7, 1), Ekran::DZIEN, BIT_PRAWY, konfPanelu());  // K1
  k.push_back(crc32(p.ekran.klatki.back().data(), p.ekran.klatki.back().size()));
  return k;
}
// (R-KON-68): linia „Dobra zmiana” z trendu normy. 40 nocy, AHI 5,0, ostatnie 6 nocy AHI 1,0 (spadek > 3,0
// w 3 z 5 nocy): bez sprawy linia jest, przy sprawie pliku oceny (czynniki) jej nie ma, przy sprawie pliku spoza oceny
// (dziennik baterii) jest
static bool dobraZmiana(const char* plik, size_t* spraw) {
  Panel p;
  przygotuj0(p);
  p.pliki.montowane = true;
  Magazyn(p.pliki, nullptr).zapisz("czynniki", "/dane/czynniki.csv", {});
  p.pliki.montowane = false;
  wybudzJeden(p, rano(6, 0), true, konfPanelu(), nullptr);
  if (plik) p.pliki.nieczytelne.insert(plik);
  // sprawa (jeśli plik czytany co wybudzenie) przed pierwszym dniem ze spadkiem AHI; ekran dnia 23.11 to S8 (linia
  // trybu aparatu), 24.11 ekran danych S2A z linią uwagi, więc liczy się 24.11
  for (int m : {10, 20, 30}) wybudzJeden(p, rano(6, m), false, konfPanelu(), nullptr);
  bool jest = false;
  for (int d : {23, 24}) {
    const bool zakaz = p.pliki.nieczytelne.erase(SC) > 0;
    p.pliki.montowane = true;
    std::vector<std::string> w;
    for (int i = 0; i < 40; i++) {
      Noc x;
      const std::string data = dodajDni("2026-10-15", i + d - 23);
      const char* ahi = data >= "2026-11-" + std::to_string(d - 5) ? "1.00" : "5.00";
      std::string l = data + ";1;414;" + ahi + ";9.40;11.30;0.10;0.30;;;;;;K;0;0;;;;";
      if (!parsujNoc(l, 'A', x)) continue;
      x.zrodlo = 'A';
      w.push_back(wierszNocyV5(x));
    }
    p.pliki.pliki.erase(SC);
    Magazyn(p.pliki, nullptr).zapisz("noce", SC, w);
    p.pliki.montowane = false;
    if (zakaz) p.pliki.nieczytelne.insert(SC);
    wybudzJeden(p, ep(2026, 11, d, 7, 0), false, konfPanelu(), nullptr);
    if (d == 24) jest = wLogu(p, "linia uwagi: Dobra zmiana");
  }
  *spraw = nieczytelne::sprawy(p.start.stan.nieczytelne).size();
  return jest;
}
// wymaganie: R-AW-40
static void testN14_DobraZmiana() {
  size_t s0 = 0, s1 = 0, s2 = 0;
  const bool bez = dobraZmiana(nullptr, &s0), czy = dobraZmiana("/dane/czynniki.csv", &s1),
             bat = dobraZmiana("/log/wybudzenia.csv", &s2);
  printf("N14 linia „Dobra zmiana” 24.11: bez sprawy %s; sprawa czynników (%zu) %s; dziennik baterii (spraw %zu) %s\n",
         bez ? "jest" : "BRAK", s1, czy ? "JEST" : "brak", s2, bat ? "jest" : "BRAK");
  SPRAWDZ(bez);
  SPRAWDZ(s1 == 1 && !czy);
  SPRAWDZ(bat);
}

// wymaganie: R-AW-40
static void testN13_WymuszenieNaEkranach() {
  // czysta funkcja
  Stany s;
  s.nieoceniane.clear();
  s.ocenaNiepelna = false;
  s.uspokojenieOcenione = s.uspokojenie = s.bezSpraw = true;
  s.niespokoj = Stany::Niespokoj::BRAK;
  Stany bez = s, z = s;
  wymusOceneNiepelna(bez, false);
  wymusOceneNiepelna(z, true);
  SPRAWDZ(bez.uspokojenie && !bez.ocenaNiepelna && bez.bezSpraw && bez.niespokoj == Stany::Niespokoj::BRAK);
  SPRAWDZ(!z.uspokojenie && z.ocenaNiepelna && !z.bezSpraw && z.niespokoj == Stany::Niespokoj::SPRAWA);
  Noc n;
  std::string l = std::string("2026-11-21") + ";1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;";
  parsujNoc(l, 'A', n);
  Stany sn = s;
  sn.ostatnia = n.data;
  Stany szn = sn;
  wymusOceneNiepelna(szn, true);
  ROWNE(tytulNocy(n, sn), "Noc w normie.");
  SPRAWDZ(tytulNocy(n, szn) != "Noc w normie.");
  // dyrygent z przełącznikiem
  kolejka::testUspokojenieSpelnione = true;
  Panel p0, p1, p2;
  std::vector<uint32_t> e0 = ekranyPrzelacznik(nullptr, p0), e1 = ekranyPrzelacznik("/log/wybudzenia.csv", p1),
                        e2 = ekranyPrzelacznik("/dane/czynniki.csv", p2);
  kolejka::testUspokojenieSpelnione = false;
  printf("N13 przełącznik „uspokojenie spełnione”: dziennik baterii → ekran dnia i K1 %s; czynniki → %s\n",
         e1 == e0 ? "jak bez sprawy" : "INNE", e2[0] != e0[0] && e2[1] != e0[1] ? "inne (bez „W normie”)" : "JAK BEZ SPRAWY");
  SPRAWDZ(e1 == e0);
  SPRAWDZ(e2[0] != e0[0]);  // ekran dnia bez „Noc w normie.” / zdania uspokajającego
  SPRAWDZ(e2[1] != e0[1]);  // K1 z `ocena częściowa`
}

// ---------- rdzeń src/nieczytelne.h (dopisane z poprawką; przed nią tego modułu nie było) ----------
// M1: wiele wywołań w jednym wybudzeniu liczy raz; wybudzenie bez czytania pliku nie zmienia licznika; ścieżka za
// długa i piąta ścieżka bez miejsca pominięte; tekst NVS i odtworzenie po utracie RTC; rodzaj danych słowem.
// wymaganie: T-WYM-19
static void testM1_Rdzen() {
  nieczytelne::Stan s{};
  auto nigdy = [](const std::string&) { return false; };
  const std::function<bool(const std::string&)> bezNosnika;  // nie sprawdzać nośnika
  const std::vector<std::string> u = {SC};
  for (int i = 0; i < 5; i++) nieczytelne::aktualizuj(s, 7, u, nigdy, bezNosnika, 100);  // jedno wybudzenie, 5 wywołań
  SPRAWDZ(s.w[0].kolejnych == 1 && nieczytelne::sprawy(s).empty());
  nieczytelne::aktualizuj(s, 8, {}, nigdy, bezNosnika, 200);  // wybudzenie bez czytania pliku: bez zmian
  SPRAWDZ(s.w[0].kolejnych == 1);
  nieczytelne::aktualizuj(s, 9, u, nigdy, bezNosnika, 300);
  SPRAWDZ(nieczytelne::sprawy(s).empty());
  SPRAWDZ(nieczytelne::aktualizuj(s, 10, u, nigdy, bezNosnika, 400));  // trzecie: lista spraw się zmienia
  std::vector<nieczytelne::Sprawa> sp = nieczytelne::sprawy(s);
  SPRAWDZ(sp.size() == 1 && sp[0].sciezka == SC && sp[0].od == 100);
  SPRAWDZ(!nieczytelne::aktualizuj(s, 11, u, nigdy, bezNosnika, 500));  // trwa: bez zmiany listy (bez zapisu NVS)
  ROWNE(nieczytelne::tekstNvs(s), std::string(SC) + "@100\n");
  // odtworzenie po utracie RTC: licznik = próg, data bez zmian; śmieci w tekście pominięte
  nieczytelne::Stan r{};
  SPRAWDZ(nieczytelne::odtworz(r, nieczytelne::tekstNvs(s) + "bez-malpy\n@5\n"));
  ROWNE(nieczytelne::tekstNvs(r), nieczytelne::tekstNvs(s));
  SPRAWDZ(!nieczytelne::odtworz(r, ""));  // ważny blok RTC nie jest nadpisywany
  // udany dostęp kończy serię
  SPRAWDZ(nieczytelne::aktualizuj(r, 12, {}, [](const std::string& x) { return x == SC; }, bezNosnika, 600));
  SPRAWDZ(nieczytelne::sprawy(r).empty() && r.w[0].sciezka[0] == 0);
  // nieudany i „udany” w tym samym wybudzeniu: nieudany wygrywa (seria trwa)
  nieczytelne::Stan q{};
  for (uint32_t w = 1; w <= 3; w++)
    nieczytelne::aktualizuj(q, w, u, [](const std::string&) { return true; }, bezNosnika, 50);
  SPRAWDZ(nieczytelne::sprawy(q).size() == 1);
  // pliku nie ma na nośniku → koniec sprawy (w wybudzeniu bez nieudanego odczytu)
  nieczytelne::Stan z = q;
  SPRAWDZ(nieczytelne::aktualizuj(z, 4, {}, nigdy, [](const std::string&) { return false; }, 60));
  SPRAWDZ(nieczytelne::sprawy(z).empty());
  // ścieżka nieczytana przez ponad WYGASA_S wygasa, do WYGASA_S zostaje
  nieczytelne::Stan e{};
  for (uint32_t w = 1; w <= 3; w++) nieczytelne::aktualizuj(e, w, u, nigdy, bezNosnika, 1000);
  SPRAWDZ(!nieczytelne::aktualizuj(e, 4, {}, nigdy, bezNosnika, 1000 + nieczytelne::WYGASA_S));
  SPRAWDZ(nieczytelne::sprawy(e).size() == 1);
  SPRAWDZ(nieczytelne::aktualizuj(e, 5, {}, nigdy, bezNosnika, 1001 + nieczytelne::WYGASA_S));
  SPRAWDZ(nieczytelne::sprawy(e).empty());
  // seria bez zegara (od = 0) dostaje datę, gdy zegar się pojawi
  nieczytelne::Stan b{};
  for (uint32_t w = 1; w <= 3; w++) nieczytelne::aktualizuj(b, w, u, nigdy, bezNosnika, 0);
  SPRAWDZ(nieczytelne::sprawy(b).size() == 1 && nieczytelne::sprawy(b)[0].od == 0);
  SPRAWDZ(nieczytelne::aktualizuj(b, 4, u, nigdy, bezNosnika, 7777));
  SPRAWDZ(nieczytelne::sprawy(b)[0].od == 7777);
  // ścieżka za długa i więcej ścieżek niż miejsc (sprawy nie są wypierane)
  nieczytelne::Stan m{};
  std::vector<std::string> duzo = {"/dane/a.csv", "/dane/b.csv", "/dane/c.csv", "/dane/d.csv", "/dane/e.csv",
                                   "/dane/bardzo-dluga-nazwa-pliku-danych.csv"};
  for (uint32_t w = 1; w <= 3; w++) nieczytelne::aktualizuj(m, w, duzo, nigdy, bezNosnika, 10 * w);
  SPRAWDZ(nieczytelne::sprawy(m).size() == (size_t)nieczytelne::MIEJSC);
  // przy komplecie wpisów poniżej progu nowa ścieżka wypiera najstarszy
  nieczytelne::Stan w6{};
  const char* stare[] = {"/dane/a.csv", "/dane/b.csv", "/dane/c.csv", "/dane/d.csv"};
  for (uint32_t w = 0; w < 4; w++) nieczytelne::aktualizuj(w6, w + 1, {stare[w]}, nigdy, bezNosnika, 100 + w);
  nieczytelne::aktualizuj(w6, 5, {"/dane/e.csv"}, nigdy, bezNosnika, 200);
  bool aJest = false, eJest = false;
  for (auto& x : w6.w) {
    aJest = aJest || std::string(x.sciezka) == "/dane/a.csv";
    eJest = eJest || std::string(x.sciezka) == "/dane/e.csv";
  }
  SPRAWDZ(!aJest && eJest);
  // 4 sprawy spoza oceny nieudane w każdym wybudzeniu, potem czynniki: plik oceny wypiera sprawę, a wyparta
  // ścieżka nie wypiera go z powrotem (przed poprawką: wymiana co wybudzenie, sprawa czynników nigdy)
  {
    nieczytelne::Stan o{};
    std::vector<std::string> poza = {"/dane/sypialnia/2026.csv", "/log/wybudzenia.csv", "/dane/humor.csv",
                                     "/dane/prognoza.csv"};
    uint32_t w = 1;
    for (; w <= 3; w++) nieczytelne::aktualizuj(o, w, poza, nigdy, bezNosnika, 1000 + w);
    poza.push_back("/dane/czynniki.csv");
    std::vector<std::string> v;
    for (int i = 0; i < 3; i++, w++) {
      nieczytelne::aktualizuj(o, w, poza, nigdy, bezNosnika, 1000 + w);
      v.push_back(nieczytelne::ocenaNiepelna(o, poza) ? "niepełna" : "pełna");
    }
    bool sprawaCzynnikow = false;
    for (auto& x : nieczytelne::sprawy(o)) sprawaCzynnikow = sprawaCzynnikow || x.sciezka == "/dane/czynniki.csv";
    printf("M1 rdzeń: 4 sprawy spoza oceny nieudane co wybudzenie + czynniki: ocena %s, %s, %s; sprawa czynników %s\n",
           v[0].c_str(), v[1].c_str(), v[2].c_str(), sprawaCzynnikow ? "jest" : "BRAK");
    SPRAWDZ(sprawaCzynnikow);
    SPRAWDZ(v == (std::vector<std::string>{"pełna", "pełna", "niepełna"}));
    SPRAWDZ(nieczytelne::sprawy(o).size() == (size_t)nieczytelne::MIEJSC);  // 3 sprawy spoza oceny zostają
  }
  // pliki oceny
  for (const char* x : {"/dane/noce/2026.csv", "/dane/noce/2025.csv", "/dane/czynniki.csv", "/dane/odpowiedzi.csv",
                        "/dane/reguly.csv"})
    SPRAWDZ(nieczytelne::plikOceny(x));
  for (const char* x : {"/log/wybudzenia.csv", "/dane/sypialnia/2026.csv", "/dane/humor.csv", "/dane/prognoza.csv",
                        "/dane/obrazki.csv", "/dane/ab-pominiete.csv", "/dane/ab-proby.csv", "/dane/portret.csv",
                        "/dane/porownanie.csv", "/dane/dobry_start.csv", "/dane/noce/2026.csv.tmp"})
    SPRAWDZ(!nieczytelne::plikOceny(x));
  // rodzaj danych słowem, bez ścieżki
  ROWNE(nieczytelne::rodzaj("/dane/noce/2026.csv"), "noce 2026");
  ROWNE(nieczytelne::rodzaj("/dane/sypialnia/2025.csv"), "sypialnia 2025");
  ROWNE(nieczytelne::rodzaj("/dane/czynniki.csv"), "czynniki nocy");
  ROWNE(nieczytelne::rodzaj("/dane/reguly.csv"), "pamięć spraw");
  ROWNE(nieczytelne::rodzaj("/log/wybudzenia.csv"), "dziennik baterii");
  ROWNE(nieczytelne::rodzaj("/dane/noce/2026.csv.tmp"), "plik danych");
  // każdy format rejestru ma słowo (żaden nie zostaje `plik danych`)
  Dziennik d;
  AtrapaPlikow ap(d);
  Magazyn mg(ap, nullptr);
  for (auto& f : rejestrFormatow()) SPRAWDZ(nieczytelne::rodzaj(mg.sciezkaRoku(f.nazwa, "2026")) != "plik danych");
  // karta: 1 plik, 2–4 pliki (dopełniacz po przeczeniu: ta sama forma dla 2, 3 i 4), bez daty, ocena
  using P = sprawy::WejscieSpraw::PlikNieczytelny;
  ROWNE(sprawy::kartaNieczytelnych({P{"noce 2026", "22.11", true}}).powod,
        "Od 22.11 nie odczytuję: noce 2026. Nic tam nie zapisuję.");
  ROWNE(sprawy::kartaNieczytelnych({P{"noce 2026", "20.11", true}, P{"czynniki nocy", "21.11", true}}).powod,
        "Od 20.11 nie odczytuję 2 plików danych. Nic tam nie zapisuję.");
  ROWNE(sprawy::kartaNieczytelnych({P{"noce 2026", "", true}, P{"a", "", false}, P{"b", "", false}}).powod,
        "Nie odczytuję 3 plików danych. Nic tam nie zapisuję.");
  ROWNE(sprawy::kartaNieczytelnych({P{"a", "1.11", false}, P{"b", "", false}, P{"c", "", false}, P{"d", "", false}}).powod,
        "Od 1.11 nie odczytuję 4 plików danych. Nic tam nie zapisuję.");
  ROWNE(sprawy::kartaNieczytelnych({P{"noce 2026", "", true}}).powod, "Nie odczytuję: noce 2026. Nic tam nie zapisuję.");
  ROWNE(sprawy::kartaNieczytelnych({P{"noce 2026", "20.11", true}}).ocena, "częściowa: bez danych z tego pliku");
  ROWNE(sprawy::kartaNieczytelnych({P{"noce 2026", "20.11", true}, P{"b", "", false}}).ocena,
        "częściowa: bez części danych");  // pliki mieszane
  ROWNE(sprawy::kartaNieczytelnych({P{"noce 2026", "20.11", true}, P{"czynniki nocy", "", true}}).ocena,
        "częściowa: bez danych z tych plików");
  ROWNE(sprawy::kartaNieczytelnych({P{"noce 2026", "20.11", true}}).ty, "po tygodniu daj znać osobie, która instalowała panel");
  ROWNE(sprawy::kartaNieczytelnych({P{"dziennik baterii", "20.11", false}}).ocena, "bez wpływu na ocenę nocy");
  ROWNE(sprawy::kartaNieczytelnych({P{"dziennik baterii", "20.11", false}}).dalej, "ponawiam odczyt przy każdym wybudzeniu");
  // teksty mieszczą się w szablonach: wiersz U-05 bez skrótu, powód w 2 wierszach, wartości w kolumnie karty
  ROWNE(dopasujWartosc("Pamięć panelu", wariantyNieczytelnegoU05("20.11"), TEST_KOLUMNA, true), "nieczytelne od 20.11");
  ROWNE(dopasujWartosc("Pamięć panelu", wariantyNieczytelnegoU05(""), TEST_KOLUMNA, true), "nieczytelne dane");
  // najdłuższe przypadki: powód ≤ 70 znaków (5.4.4) i ≤ 2 wiersze na karcie (W = 744 px treści, src/ekrany.h)
  for (const std::string& t : {sprawy::kartaNieczytelnych({P{"dziennik baterii", "30.11", false}}).powod,
                               sprawy::kartaNieczytelnych({P{"a", "30.11", true}, P{"b", "", false}, P{"c", "", false},
                                                           P{"d", "", false}}).powod}) {
    size_t znakow = 0;
    for (unsigned char c : t) znakow += (c & 0xC0) != 0x80;
    printf("   powód %zu znaków, %d px: %s\n", znakow, szer(F_P28B, t), t.c_str());
    SPRAWDZ(znakow <= 70);
    SPRAWDZ(zawin(F_P28B, t, 744).size() <= 2);
  }
  SPRAWDZ(szer(F_SB44, "Nie odczytuję danych") <= 744);
  for (const sprawy::KartaSprawy& k : {sprawy::kartaNieczytelnych({P{"porównanie nocy", "20.11", true}, P{"b", "", false}}),
                                       sprawy::kartaNieczytelnych({P{"porównanie nocy", "20.11", false}})})
    for (auto* v : {&k.ocena, &k.ty, &k.dalej, &k.koniec}) {
      printf("   %d px z %d: %s\n", szer(F_P24S, *v), 772 - SPRAWY_X_WARTOSCI, v->c_str());
      SPRAWDZ(szer(F_P24S, *v) <= 772 - SPRAWY_X_WARTOSCI);
    }
  printf("M1 rdzeń: raz na wybudzenie, próg 3, NVS, odtworzenie, wygasanie, brak pliku, wypieranie, pliki oceny, teksty\n");
}

// sprawa pliku oceny (czynniki nocy) przy czytelnych nocach robi ocenę niepełną (wiersz logu
// wymuszenia), a sprawa pliku spoza oceny (dziennik baterii) nie: klatki ekranu dnia i K1 bajt w bajt jak bez sprawy.
static std::vector<uint32_t> ekranyOceny(const char* plik, bool* wymuszona, size_t* spraw) {
  Panel p;
  przygotuj0(p);
  p.pliki.montowane = true;
  Magazyn(p.pliki, nullptr).zapisz("czynniki", "/dane/czynniki.csv", {});
  p.pliki.montowane = false;
  wybudzJeden(p, rano(6, 0), true, konfPanelu(), nullptr);
  wybudzJeden(p, rano(6, 10), false, konfPanelu(), nullptr);  // dziennik baterii na nośniku
  if (plik) p.pliki.nieczytelne.insert(plik);
  bool w = false;
  std::vector<uint32_t> k;
  // wybudzenia z dziennikiem baterii czytanym (raz na dobę): nowa doba przy każdej usterce dziennika
  const uint32_t t[3] = {ep(2026, 11, 23, 7, 0), ep(2026, 11, 24, 7, 0), ep(2026, 11, 25, 7, 0)};
  for (uint32_t x : t) {
    wybudzJeden(p, x, false, konfPanelu(), nullptr);
    w = w || wLogu(p, "ocena: niepełna, nieczytelny plik oceny");
  }
  k.push_back(crc32(p.ekran.klatki.back().data(), p.ekran.klatki.back().size()));  // ekran dnia
  nacisnijJeden(p, ep(2026, 11, 25, 7, 1), Ekran::DZIEN, BIT_PRAWY, konfPanelu());  // K1
  w = w || wLogu(p, "ocena: niepełna, nieczytelny plik oceny");
  k.push_back(crc32(p.ekran.klatki.back().data(), p.ekran.klatki.back().size()));
  *wymuszona = w;
  *spraw = nieczytelne::sprawy(p.start.stan.nieczytelne).size();
  return k;
}
// wymaganie: R-AW-40
static void testN10_OcenaTylkoPlikiOceny() {
  bool w0 = false, w1 = false, w2 = false;
  size_t s0 = 0, s1 = 0, s2 = 0;
  std::vector<uint32_t> bez = ekranyOceny(nullptr, &w0, &s0), bat = ekranyOceny("/log/wybudzenia.csv", &w1, &s1),
                        czy = ekranyOceny("/dane/czynniki.csv", &w2, &s2);
  printf("N10 bez sprawy: wymuszenie %d; dziennik baterii: spraw %zu, wymuszenie %d, ekran dnia i K1 %s; czynniki: spraw %zu, "
         "wymuszenie %d\n", (int)w0, s1, (int)w1, bat == bez ? "jak bez sprawy" : "INNE", s2, (int)w2);
  SPRAWDZ(!w0 && s0 == 0);
  SPRAWDZ(s1 == 1 && !w1);  // plik spoza oceny: sprawa jest, ocena bez zmian
  SPRAWDZ(bat == bez);      // ekrany oceny bajt w bajt jak bez sprawy
  SPRAWDZ(s2 == 1 && w2);   // plik oceny: ocena niepełna (mutacja „wymuszenie wyłączone” pada tutaj)
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  printf("trwale nieczytelny plik danych na U-05 i w „Otwartych sprawach” (T-TEST-520)\n");
  testN1N2N3N4_SeriaSprawaZanikKoniec();
  testN5_PrzerwanaSeria();
  testN6_OtwarteSprawyWKokpicie();
  testN7_BezUsterkiJakMain();
  testN8_CzekanieNaKablu();
  testM1_Rdzen();
  testN11_BlizniakDziennikBaterii();
  testN12_PlikOcenyMaMiejsce();
  testN13_WymuszenieNaEkranach();
  testN14_DobraZmiana();
  testN10_OcenaTylkoPlikiOceny();
  if (rozbieznosci) zle++;
  printf("bliźniaki bez tekstów końcowych: rozbieżności stanu serii %d\n", rozbieznosci);
  printf("testy_plik_nieczytelny: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
