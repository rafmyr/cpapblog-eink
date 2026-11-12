// SPDX-License-Identifier: GPL-3.0-or-later
// Testy „Slot dzienny, obrazki dnia i Przewietrz sypialnię” (spec 5.8.3, 5.8.4, 5.8.5, 5.8.12, R-KON-04, R-KON-05, R-KON-20, R-KON-29, R-KON-41…R-KON-62, ADR-0027).
// Dane wyłącznie syntetyczne (prognozy, obrazki PBM, odczyty CO2); prawdziwe obrazki dnia nie trafiają do repo.
// Uruchomienie: tools/prototyp/testy.sh (program testy_slot_dzienny).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "ekrany.h"
#include "ekrany_kontekst.h"
#include "kontekst.h"
#include "obrazki.h"
#include "plan.h"
#include "slot_dzienny.h"
#include "sypialnia.h"
#include "wspolne.h"
#include "dane_slonce_katowice_2026.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE(a, b)                                                                                      \
  do {                                                                                                   \
    std::string a_ = (a), b_ = (b);                                                                      \
    if (a_ == b_) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s\n  jest:    '%s'\n  ma być:  '%s'\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } \
  } while (0)
#define ROWNE_L(a, b)                                                                                          \
  do {                                                                                                         \
    long a_ = (long)(a), b_ = (long)(b);                                                                       \
    if (a_ == b_) ok++;                                                                                        \
    else { zle++; printf("BŁĄD %s:%d: %s = %ld, ma być %ld\n", __FILE__, __LINE__, #a, a_, b_); }            \
  } while (0)

static const double KAT_LAT = 50.2597, KAT_LON = 19.0217;  // Katowice (geokodowanie Open-Meteo)

// tekst ekranu bez twardych spacji (spec zapisuje zwykłe spacje)
static std::string zwykle(std::string s) {
  for (size_t p; (p = s.find("\xC2\xA0")) != std::string::npos;) s.replace(p, 2, " ");
  return s;
}

// prognoza syntetyczna na `dzis` i jutro: stała temperatura i PM2.5, potem nadpisania godzin
static Prognoza prognoza(const std::string& dzis, float t, float pm) {
  Prognoza p;
  p.data = dzis;
  p.pobrano = 1;
  p.godzinaPobrania = "07:55";
  for (auto& g : p.h) {
    g.temp = t;
    g.pm25 = pm;
    g.opad = 0;
    g.chmury = 20;
    g.porywy = 20;
  }
  return p;
}
static void godziny(Prognoza& p, int od, int do_, float t, float pm) {
  for (int h = od; h < do_; h++) {
    if (!std::isnan(t)) p.h[h].temp = t;
    if (!std::isnan(pm)) p.h[h].pm25 = pm;
  }
}
static WynikWietrzenia okno(const Prognoza& p, const std::string& dzis, int godz, float przyLozku, int co2 = -1) {
  WejscieWietrzenia w;
  w.prognoza = &p;
  w.dzis = dzis;
  w.godzina = godz;
  w.przyLozku = przyLozku;
  w.co2 = co2;
  return regulaWietrzenia(w);
}
static TekstySlotu teksty(const WyborSlotu& w, const WejscieSlotu& we) { return tekstySlotu(w, we); }
static WejscieSlotu wejscie(const std::string& dzis, int minuta, const Prognoza* p, float przyLozku) {
  WejscieSlotu w;
  w.dzis = dzis;
  w.minuta = minuta;
  w.prognoza = p;
  w.przyLozku = przyLozku;
  w.wspolrzedne = true;
  w.lat = KAT_LAT;
  w.lon = KAT_LON;
  w.obrazekJest = true;
  return w;
}

// ---------- R-KON-49: wschód i zachód ----------
static void testR_KON_49_slonce() {
  // wzorzec: api.sunrise-sunset.org (algorytm NOAA), Katowice, czas Europe/Warsaw, odczyt 04.11.2026
  struct { const char* d; int w, z; } wz[] = {
      {"2026-01-15", 7 * 60 + 37, 16 * 60 + 9},  {"2026-02-15", 6 * 60 + 54, 17 * 60 + 1},
      {"2026-03-20", 5 * 60 + 46, 17 * 60 + 56}, {"2026-04-15", 5 * 60 + 50, 19 * 60 + 37},
      {"2026-05-15", 4 * 60 + 56, 20 * 60 + 23}, {"2026-06-21", 4 * 60 + 33, 20 * 60 + 58},
      {"2026-07-15", 4 * 60 + 50, 20 * 60 + 49}, {"2026-08-15", 5 * 60 + 33, 20 * 60 + 3},
      {"2026-09-22", 6 * 60 + 30, 18 * 60 + 42}, {"2026-11-03", 6 * 60 + 37, 16 * 60 + 17},
      {"2026-12-14", 7 * 60 + 36, 15 * 60 + 40}, {"2026-12-21", 7 * 60 + 40, 15 * 60 + 43}};
  for (auto& e : wz) {
    int w = -1, z = -1;
    SPRAWDZ(slonce(e.d, KAT_LAT, KAT_LON, w, z));
    if (std::abs(w - e.w) > 2 || std::abs(z - e.z) > 2) printf("  %s: wschód %d (wzorzec %d), zachód %d (%d)\n", e.d, w, e.w, z, e.z);
    SPRAWDZ(std::abs(w - e.w) <= 2);  // wzorzec obcina sekundy, więc różnica do 2 min
    SPRAWDZ(std::abs(z - e.z) <= 2);
  }
  // cały rok 2026: wschód i zachód w minutach doby (zaokrąglone jak na ekranie) ≤ 2 min
  int pozaGranica = 0;
  double najw = 0;
  for (auto& e : SLONCE_2026) {
    int w = -1, z = -1;
    if (!slonce(e.d, KAT_LAT, KAT_LON, w, z)) { pozaGranica++; continue; }
    najw = std::max(najw, std::max(std::fabs(w - e.w), std::fabs(z - e.z)));
    if (std::fabs(w - e.w) > 2 || std::fabs(z - e.z) > 2) pozaGranica++;
  }
  ROWNE_L(pozaGranica, 0);
  printf("  R-KON-49: największa różnica wschodu albo zachodu w 2026: %.2f min\n", najw);
  ROWNE_L(przesuniecieStrefy("2026-03-28"), 60);
  ROWNE_L(przesuniecieStrefy("2026-03-29"), 120);
  ROWNE_L(przesuniecieStrefy("2026-10-24"), 120);
  ROWNE_L(przesuniecieStrefy("2026-10-25"), 60);
  // 14.12 → dzień krótszy niż tydzień temu, liczba dodatnia
  WynikSlonca s = dzienINoc("2026-12-14", KAT_LAT, KAT_LON);
  SPRAWDZ(s.jest);
  SPRAWDZ(s.powod.rfind("Dzień krótszy o ", 0) == 0);
  SPRAWDZ(s.zmiana < 0);
  // pierwszy dzień dłuższy od poprzedniego po przesileniu zimowym → `Najkrótszy dzień za nami.`
  int ile = 0;
  for (int i = 0; i < 10; i++) {
    std::string d = dodajDni("2026-12-18", i);
    if (dzienINoc(d, KAT_LAT, KAT_LON).powod == "Najkrótszy dzień za nami.") ile++;
  }
  ROWNE_L(ile, 1);
  ile = 0;
  for (int i = 0; i < 10; i++)
    if (dzienINoc(dodajDni("2026-06-17", i), KAT_LAT, KAT_LON).powod == "Najdłuższy dzień za nami.") ile++;
  ROWNE_L(ile, 1);
  // bez współrzędnych: brak S-K-37
  SPRAWDZ(!dzienINoc("2026-11-03", NAN, NAN).jest);
}

// ---------- prognoza Open-Meteo (pola i jednostki prawdziwej odpowiedzi, tu skrócone) ----------
static void testPrognozaOpenMeteo() {
  std::string f =
      "{\"latitude\":50.26,\"hourly_units\":{\"time\":\"iso8601\",\"temperature_2m\":\"°C\"},\"hourly\":{\"time\":["
      "\"2026-11-04T00:00\",\"2026-11-04T01:00\",\"2026-11-05T07:00\",\"2026-11-06T00:00\"],"
      "\"temperature_2m\":[3.5,-1.2,-4.0,9],\"precipitation_probability\":[0,10,null,5],"
      "\"cloud_cover\":[100,90,80,0],\"wind_gusts_10m\":[20.5,30,61,0]}}";
  std::string a =
      "{\"hourly_units\":{\"pm2_5\":\"μg/m³\"},\"hourly\":{\"time\":[\"2026-11-04T00:00\",\"2026-11-04T01:00\"],"
      "\"pm2_5\":[33.6,null],\"birch_pollen\":[0.0,12],\"grass_pollen\":[0,0],\"alder_pollen\":[0,0],"
      "\"mugwort_pollen\":[0,0]}}";
  Prognoza p;
  SPRAWDZ(parsujPrognoze(f, p));
  SPRAWDZ(parsujPowietrze(a, p));
  ROWNE(p.data, "2026-11-04");
  SPRAWDZ(std::fabs(p.h[0].temp - 3.5f) < 1e-4);
  SPRAWDZ(std::fabs(p.h[1].temp + 1.2f) < 1e-4);
  SPRAWDZ(std::fabs(p.h[31].temp + 4.0f) < 1e-4);  // jutro 07:00
  SPRAWDZ(std::isnan(p.h[31].opad));                // null = brak, nie 0
  SPRAWDZ(std::fabs(p.h[31].porywy - 61) < 1e-4);
  SPRAWDZ(std::fabs(p.h[0].pm25 - 33.6f) < 1e-4);
  SPRAWDZ(std::isnan(p.h[1].pm25));
  SPRAWDZ(std::fabs(p.h[1].pylek[PYL_BRZOZA] - 12) < 1e-4);
  const GodzinaPrognozy* g = p.godzina("2026-11-05", 7);
  SPRAWDZ(g && std::fabs(g->temp + 4.0f) < 1e-4);
  SPRAWDZ(p.godzina("2026-11-06", 0) == nullptr);  // poza dwiema dobami
  // zapis i odczyt w pamięci panelu
  p.pobrano = 1793800000;
  p.godzinaPobrania = "07:55";
  Prognoza q;
  SPRAWDZ(prognozaZWierszy(wierszePrognozy(p), q));
  ROWNE_L(wierszePrognozy(p).size(), 48);
  ROWNE(q.data, "2026-11-04");
  SPRAWDZ(q.pobrano == 1793800000u);
  SPRAWDZ(std::fabs(q.h[31].temp + 4.0f) < 1e-4);
  SPRAWDZ(std::isnan(q.h[1].pm25));
  SPRAWDZ(!prognozaZWierszy({"śmieci"}, q) && !q.jest());
  // nieczytelne odpowiedzi
  Prognoza r;
  SPRAWDZ(!parsujPrognoze("{\"hourly\":5}", r));
  SPRAWDZ(!parsujPrognoze("", r));
  SPRAWDZ(!parsujPowietrze(a, r));  // powietrze bez wcześniejszej prognozy (brak daty)
  // ważność R-KON-53: 12 h
  SPRAWDZ(prognozaWazna(p, 1793800000 + 12 * 3600));
  SPRAWDZ(!prognozaWazna(p, 1793800000 + 12 * 3600 + 1));
  // geokodowanie i adresy bez danych terapii (ADR-0001)
  double lat = 0, lon = 0;
  SPRAWDZ(parsujGeokodowanie("{\"results\":[{\"id\":1,\"name\":\"Katowice\",\"latitude\":50.2597,\"longitude\":19.02173}]}", lat, lon));
  SPRAWDZ(std::fabs(lat - 50.2597) < 1e-6 && std::fabs(lon - 19.02173) < 1e-6);
  SPRAWDZ(!parsujGeokodowanie("{\"generationtime_ms\":0.1}", lat, lon));
  ROWNE(urlPrognozy(50.2597, 19.02173),
        "http://api.open-meteo.com/v1/forecast?latitude=50.2597&longitude=19.0217&hourly=temperature_2m,"
        "precipitation_probability,cloud_cover,wind_gusts_10m&timezone=Europe%2FWarsaw&forecast_days=2");
  SPRAWDZ(urlPowietrza(50.2597, 19.02173).find("pm2_5,birch_pollen,grass_pollen,alder_pollen,mugwort_pollen") != std::string::npos);
  ROWNE(urlGeokodowania("Bielsko-Biała"),
        "http://geocoding-api.open-meteo.com/v1/search?name=Bielsko-Bia%C5%82a&count=1&language=pl&format=json");
}

// ---------- R-KON-43, R-KON-44, R-KON-45: reguła wietrzenia ----------
static void testR_KON_43_regula() {
  const std::string d = "2026-11-03";
  // przy łóżku 21°; za oknem 11:00–14:00 od 4° do 6°, PM2.5 20; od 15:00 PM2.5 60
  Prognoza p = prognoza(d, 1, 20);
  godziny(p, 11, 15, 5, 20);
  p.h[12].temp = 6;
  p.h[11].temp = 4;
  godziny(p, 15, 24, 2, 60);
  WynikWietrzenia a = okno(p, d, 10, 21);
  SPRAWDZ(a.wariant == Wietrzenie::POZNIEJ);
  SPRAWDZ(a.tryb == TrybWietrzenia::ZIMA);
  ROWNE_L(a.od, 11);
  WynikWietrzenia b = okno(p, d, 11, 21);
  SPRAWDZ(b.wariant == Wietrzenie::TRWA);
  ROWNE_L(b.do_, 15);
  SPRAWDZ(b.powod == PowodOkna::SMOG);
  WejscieSlotu we = wejscie(d, 11 * 60, &p, 21);
  WyborSlotu wy = wybierzSlot(we);
  SPRAWDZ(wy.ekran == EkranSlotu::SK33);
  TekstySlotu t = teksty(wy, we);
  ROWNE(t.podpis + " " + t.godzina, "wietrz do 15:00");
  ROWNE(t.powod, "Potem smog za oknem.");
  ROWNE(zwykle(t.krok), "→ Krótko i szeroko, 5–10 min.");
  ROWNE(t.etykieta, "Okno na wietrzenie");
  WejscieSlotu we10 = wejscie(d, 10 * 60, &p, 21);
  TekstySlotu t10 = teksty(wybierzSlot(we10), we10);
  ROWNE(t10.podpis + " " + t10.godzina, "wietrz od 11:00");
  ROWNE(zwykle(t10.powod), "Wtedy na dworze najcieplej i czysto.");
  // lato: przy łóżku 26°, za oknem 31° do 19:00 i 23° od 20:00, PM2.5 10
  Prognoza l = prognoza("2026-07-17", 31, 10);
  godziny(l, 20, 48, 23, 10);
  WynikWietrzenia c = okno(l, "2026-07-17", 12, 26);
  SPRAWDZ(c.wariant == Wietrzenie::UPAL_DO);
  ROWNE_L(c.od, 20);
  WejscieSlotu wl = wejscie("2026-07-17", 12 * 60, &l, 26);
  TekstySlotu tl = teksty(wybierzSlot(wl), wl);
  ROWNE(tl.podpis + " " + tl.godzina, "okna zamknięte do 20:00");
  ROWNE(zwykle(tl.powod), "Na dworze cieplej niż w domu.");
  ROWNE(tl.krok, "→ Zasłoń okna od strony słońca.");
  WynikWietrzenia c2 = okno(l, "2026-07-17", 20, 26);
  SPRAWDZ(c2.wariant == Wietrzenie::TRWA);
  ROWNE_L(c2.do_, 23);
  SPRAWDZ(c2.powod == PowodOkna::CZYSTO_DO_WIECZORA);
  WejscieSlotu wl20 = wejscie("2026-07-17", 20 * 60, &l, 26);
  TekstySlotu tl20 = teksty(wybierzSlot(wl20), wl20);
  ROWNE(tl20.podpis + " " + tl20.godzina, "wietrz do 23:00");
  ROWNE(tl20.krok, "→ Otwórz szeroko, zamknij o 23:00.");
  // upał bez chłodniejszej godziny w horyzoncie
  Prognoza u = prognoza("2026-07-17", 31, 10);
  WejscieSlotu wu = wejscie("2026-07-17", 12 * 60, &u, 26);
  WyborSlotu wyu = wybierzSlot(wu);
  SPRAWDZ(wyu.okno.wariant == Wietrzenie::UPAL_CALY_DZIEN);
  ROWNE(teksty(wyu, wu).tytul, "Okna zamknięte cały dzień.");
  // brak trybu: przy łóżku 21°, za oknem 16° cały dzień, bez CO2
  Prognoza n = prognoza(d, 16, 10);
  SPRAWDZ(okno(n, d, 12, 21).wariant == Wietrzenie::BRAK);
  // tryb zimowy i PM2.5 > 35 do 23:00 → `Dziś bez wietrzenia.`
  Prognoza s = prognoza(d, 2, 80);
  WynikWietrzenia sm = okno(s, d, 9, 21);
  SPRAWDZ(sm.wariant == Wietrzenie::SMOG);
  WejscieSlotu ws = wejscie(d, 9 * 60, &s, 21);
  TekstySlotu ts = teksty(wybierzSlot(ws), ws);
  ROWNE(ts.tytul, "Dziś bez wietrzenia.");
  ROWNE(ts.powod, "Smog za oknem do wieczora, PM2.5 80.");
  ROWNE(ts.krok, "→ Jeśli trzeba, wietrz bardzo krótko.");
  // okno minęło (było rano): bez wskazania do końca slotu, nie „bez wietrzenia”
  Prognoza m = prognoza(d, 2, 80);
  godziny(m, 8, 11, 5, 20);
  SPRAWDZ(okno(m, d, 9, 21).wariant == Wietrzenie::TRWA);
  SPRAWDZ(okno(m, d, 13, 21).wariant == Wietrzenie::BRAK);
  // luka w prognozie: godzina bez PM2.5 nie jest czysta i nie wchodzi do okna (5.8.10)
  Prognoza g = prognoza(d, 1, 20);
  g.h[13].pm25 = NAN;
  WynikWietrzenia gg = okno(g, d, 12, 21);
  SPRAWDZ(gg.wariant == Wietrzenie::TRWA);
  ROWNE_L(gg.do_, 13);
  // bez czujnika panelu S-K-33 pominięte; bez prognozy też (R-KON-53)
  SPRAWDZ(okno(p, d, 11, NAN).wariant == Wietrzenie::BRAK);
  SPRAWDZ(wybierzSlot(wejscie(d, 11 * 60, nullptr, 21)).ekran != EkranSlotu::SK33);
  // horyzont do 23:00: o 23:00 brak okna
  SPRAWDZ(okno(prognoza(d, 1, 10), d, 23, 21).wariant == Wietrzenie::BRAK);
  // wizualizacja: słupki od bieżącej godziny do 22
  ROWNE_L(b.pm25.size(), 12);
}

static void testR_KON_44_co2() {
  const std::string d = "2026-11-03";
  Prognoza p = prognoza(d, 10, 20);  // brak trybu (przy łóżku 21°)
  WejscieSlotu we = wejscie(d, 14 * 60, &p, 21);
  we.co2 = 1150;
  WyborSlotu w = wybierzSlot(we);
  SPRAWDZ(w.ekran == EkranSlotu::SK33 && w.okno.wariant == Wietrzenie::CO2);
  TekstySlotu t = teksty(w, we);
  ROWNE(t.tytul, "Przewietrz teraz.");
  ROWNE(t.powod, "CO2 w sypialni 1150 ppm, za oknem czysto.");
  // PM2.5 teraz 70 → wariant z reguły (tu tryb zimowy i okno od 18:00)
  Prognoza q = prognoza(d, 1, 70);
  godziny(q, 18, 21, 4, 10);
  WejscieSlotu we2 = wejscie(d, 14 * 60, &q, 21);
  we2.co2 = 1150;
  WyborSlotu w2 = wybierzSlot(we2);
  SPRAWDZ(w2.okno.wariant == Wietrzenie::POZNIEJ);
  TekstySlotu t2 = teksty(w2, we2);
  ROWNE(t2.podpis + " " + t2.godzina, "wietrz od 18:00");
  ROWNE(t2.powod, "Teraz smog za oknem, PM2.5 70.");
  // brak odczytu CO2 → reguła bez wariantu CO2
  WejscieSlotu we3 = wejscie(d, 14 * 60, &p, 21);
  SPRAWDZ(wybierzSlot(we3).okno.wariant != Wietrzenie::CO2);
}

static void testR_KON_45_pylek() {
  const std::string d = "2026-04-13";
  Prognoza p = prognoza(d, 15, 10);
  for (int h = 0; h < 48; h++) p.h[h].pylek[PYL_BRZOZA] = 20;
  for (int h = 12; h < 16; h++) p.h[h].pylek[PYL_BRZOZA] = 200;
  PylkiDnia py;
  py.zaznaczony[PYL_BRZOZA] = true;
  py.prog[PYL_BRZOZA] = 150;
  WejscieSlotu we = wejscie(d, 9 * 60, &p, 21);
  we.pylki = py;
  WyborSlotu w = wybierzSlot(we);
  SPRAWDZ(w.ekran == EkranSlotu::SK36);
  TekstySlotu t = teksty(w, we);
  ROWNE(t.tytul, "Pyłek brzozy wysoko.");
  ROWNE(zwykle(t.powod), "Najwięcej między 12:00 a 16:00.");
  ROWNE(t.krok, "→ W tych godzinach okna zamknięte.");
  // godziny pyłku nie są czyste w regule wietrzenia
  SPRAWDZ(!godzinaCzysta(p.h[13], py));
  SPRAWDZ(godzinaCzysta(p.h[10], py));
  // zaznaczone tylko trawy → brak S-K-36, godziny brzozy czyste
  PylkiDnia tr;
  tr.zaznaczony[PYL_TRAWY] = true;
  tr.prog[PYL_TRAWY] = 50;
  we.pylki = tr;
  SPRAWDZ(wybierzSlot(we).ekran != EkranSlotu::SK36);
  SPRAWDZ(godzinaCzysta(p.h[13], tr));
  // pusty zestaw (domyślnie) → brak S-K-36
  we.pylki = PylkiDnia();
  SPRAWDZ(wybierzSlot(we).ekran != EkranSlotu::SK36);
}

// ---------- R-KON-46, R-KON-47: „Jutro rano” ----------
static void testR_KON_47_jutro() {
  const std::string d = "2026-11-03";
  auto jutro = [&](float t, float opad, float porywy) {
    Prognoza p = prognoza(d, 8, 10);
    for (int h = 24 + 6; h < 24 + 9; h++) {
      p.h[h].temp = t;
      p.h[h].opad = opad;
      p.h[h].porywy = porywy;
    }
    return p;
  };
  Prognoza a = jutro(-2, 70, 20), b = jutro(-2, 0, 20), c = jutro(5, 80, 20), e = jutro(5, 20, 70), f = jutro(5, 20, 30);
  SPRAWDZ(jutroRano(a, d).wariant == Jutro::SLISKO);
  SPRAWDZ(jutroRano(b, d).wariant == Jutro::MROZ);
  SPRAWDZ(jutroRano(c, d).wariant == Jutro::DESZCZ);
  SPRAWDZ(jutroRano(e, d).wariant == Jutro::WIATR);
  SPRAWDZ(jutroRano(f, d).wariant == Jutro::BRAK);
  WejscieSlotu we = wejscie(d, 12 * 60, &b, 21);
  we.przyLozku = 12;  // bez trybu wietrzenia
  WyborSlotu w = wybierzSlot(we);
  SPRAWDZ(w.ekran == EkranSlotu::SK34);
  TekstySlotu t = teksty(w, we);
  ROWNE(t.tytul, "Jutro rano mróz, −2°.");
  ROWNE(zwykle(t.powod), "Szyby i auta mogą być oszronione.");
  ROWNE(t.krok, "→ Zostaw rano więcej czasu.");
  ROWNE(zwykle(teksty(wybierzSlot(wejscie(d, 12 * 60, &a, 12)), wejscie(d, 12 * 60, &a, 12)).powod),
        "Rano −2° i opady. Na chodnikach może być lód.");
  ROWNE(zwykle(teksty(wybierzSlot(wejscie(d, 12 * 60, &c, 12)), wejscie(d, 12 * 60, &c, 12)).powod),
        "Opady 80% między 6:00 a 9:00.");
  ROWNE(zwykle(teksty(wybierzSlot(wejscie(d, 12 * 60, &e, 12)), wejscie(d, 12 * 60, &e, 12)).powod), "Porywy do 70 km/h.");
  // R-KON-46: jutro −4°; dziś okno 13:00–17:00 → o 15:00 i 16:00 S-K-33, o 17:00 S-K-34
  Prognoza o = jutro(-4, 0, 20);
  for (int h = 0; h < 24; h++) { o.h[h].temp = 1; o.h[h].pm25 = 80; }
  godziny(o, 13, 17, 5, 10);
  SPRAWDZ(wybierzSlot(wejscie(d, 15 * 60, &o, 21)).ekran == EkranSlotu::SK33);
  SPRAWDZ(wybierzSlot(wejscie(d, 16 * 60, &o, 21)).ekran == EkranSlotu::SK33);
  SPRAWDZ(wybierzSlot(wejscie(d, 17 * 60, &o, 21)).ekran == EkranSlotu::SK34);
  // dziś okno 17:00–19:00 → o 16:00 S-K-33 `Wietrz od 17:00.` (zaczyna się w ciągu godziny)
  Prognoza o2 = jutro(-4, 0, 20);
  for (int h = 0; h < 24; h++) { o2.h[h].temp = 1; o2.h[h].pm25 = 80; }
  godziny(o2, 17, 19, 5, 10);
  WejscieSlotu w16 = wejscie(d, 16 * 60, &o2, 21);
  WyborSlotu wy16 = wybierzSlot(w16);
  SPRAWDZ(wy16.ekran == EkranSlotu::SK33);
  ROWNE(teksty(wy16, w16).godzina, "17:00");
  // przed 16:00 przy tym samym wskazaniu S-K-34 → S-K-33 (okno 18:00–20:00 o 14:00)
  Prognoza o3 = jutro(-4, 0, 20);
  for (int h = 0; h < 24; h++) { o3.h[h].temp = 1; o3.h[h].pm25 = 80; }
  godziny(o3, 18, 20, 5, 10);
  SPRAWDZ(wybierzSlot(wejscie(d, 14 * 60, &o3, 21)).ekran == EkranSlotu::SK33);
  SPRAWDZ(wybierzSlot(wejscie(d, 16 * 60, &o3, 21)).ekran == EkranSlotu::SK34);
}

// ---------- R-KON-48: wymiana sprzętu ----------
static void testR_KON_48_wymiana() {
  ElementWymiany el[EL_N];
  // filtr w tygodniach (4 tyg. od 13.10 = termin 10.11, jak dawne 2 mies. od 10.09), maska 6 mies.
  el[EL_FILTR] = {"2026-10-13", 4, true};
  el[EL_MASKA] = {"2026-07-01", 6, false};
  ROWNE(terminWymiany(el[EL_FILTR]), "2026-11-10");
  ROWNE(terminWymiany({"2026-08-31", 6, false}), "2027-02-28");
  // pokazania 03.11, 05.11, 07.11 (nie częściej niż co 2 dni), zapis wymiany 08.11 → od 09.11 brak
  std::string ost;
  std::vector<std::string> dni;
  for (int i = 0; i < 6; i++) {
    std::string d = dodajDni("2026-11-03", i);
    WynikWymiany w = wymianaSprzetu(el, d, ost);
    if (w.element >= 0) { dni.push_back(d); ost = d; }
  }
  SPRAWDZ((dni == std::vector<std::string>{"2026-11-03", "2026-11-05", "2026-11-07"}));
  ElementWymiany po[EL_N];
  for (int i = 0; i < EL_N; i++) po[i] = el[i];
  po[EL_FILTR].wymieniono = "2026-11-08";
  SPRAWDZ(wymianaSprzetu(po, "2026-11-09", "2026-11-07").element < 0);
  // ten sam dzień: kolejne oceny dalej S-K-35
  SPRAWDZ(wymianaSprzetu(el, "2026-11-03", "2026-11-03").element == EL_FILTR);
  // po terminie
  WynikWymiany pt = wymianaSprzetu(el, "2026-11-12", "");
  SPRAWDZ(pt.element == EL_FILTR && pt.poTerminie);
  // bez zapisanej daty → brak S-K-35 (R-MAS-06)
  ElementWymiany puste[EL_N];
  SPRAWDZ(wymianaSprzetu(puste, "2026-11-03", "").element < 0);
  // wiek elementu
  ROWNE(wiekElementu("2026-09-10", "2026-11-03"), "1 mies.");
  ROWNE(wiekElementu("2026-10-13", "2026-11-03"), "3 tyg.");
  ROWNE(wiekElementu("", "2026-11-03"), "–");
  // ekran: tytuł, powód, krok (5.8.4)
  WejscieSlotu we = wejscie("2026-11-03", 12 * 60, nullptr, 21);
  we.sprzet = &el;
  WyborSlotu w = wybierzSlot(we);
  SPRAWDZ(w.ekran == EkranSlotu::SK35);
  TekstySlotu t = teksty(w, we);
  ROWNE(t.tytul, "Wymiana: filtr.");
  ROWNE(zwykle(t.powod), "Termin 10.11, ma 3 tyg.");  // filtr w tygodniach
  // krok R-MAS-18: ◀ 2 s na samym S-K-35 (strony „Sprzęt” nie ma od ADR-0041), bez nazw roboczych stron i bez
  // ↻ 2 s; ≤ 60 znaków i 1 linia na 744 px (5.4.4)
  ROWNE(zwykle(t.krok), "→ Po wymianie przytrzymaj ◀ 2 s.");
  SPRAWDZ(szer(F_P28B, t.krok) <= 744);
  SPRAWDZ(t.krok.find("K2") == std::string::npos && t.krok.find("↻") == std::string::npos);
  SPRAWDZ(szer(F_P28B, t.krok) <= 744);
  WejscieSlotu we2 = wejscie("2026-11-12", 12 * 60, nullptr, 21);
  we2.sprzet = &el;
  ROWNE(zwykle(teksty(wybierzSlot(we2), we2).powod), "Termin minął 10.11, ma 4 tyg.");
  // maska (6 mies.) przed terminem: data i wiek; po terminie: tekst o refundacji (ADR-0002)
  ElementWymiany mk[EL_N];
  mk[EL_MASKA] = {"2026-05-10", 6, false};
  WejscieSlotu we3 = wejscie("2026-11-05", 12 * 60, nullptr, 21);
  we3.sprzet = &mk;
  WyborSlotu w3 = wybierzSlot(we3);
  SPRAWDZ(w3.ekran == EkranSlotu::SK35 && w3.wymiana.element == EL_MASKA);
  ROWNE(teksty(w3, we3).tytul, "Wymiana: maska.");
  ROWNE(zwykle(teksty(w3, we3).powod), "Termin 10.11, ma 5 mies.");
  ROWNE(zwykle(teksty(w3, we3).krok), "→ Po wymianie przytrzymaj ◀ 2 s.");
  SPRAWDZ(szer(F_P28B, teksty(w3, we3).krok) <= 744);
  SPRAWDZ(szer(F_P28B, teksty(w3, we3).krok) <= 744);
  we3.dzis = "2026-11-12";
  WyborSlotu w4 = wybierzSlot(we3);
  SPRAWDZ(w4.ekran == EkranSlotu::SK35 && w4.wymiana.poTerminie);
  ROWNE(zwykle(teksty(w4, we3).powod), "Możesz już odebrać nową maskę z refundacji.");
}

// ---------- R-KON-50, R-KON-54, R-KON-53: obrazek dnia w pierwszym bloku, zastępstwa ----------
// rotacja parzysty/nieparzysty dzień zastąpiona blokiem S-K-38 od startu slotu.
static void testR_KON_50_rotacja() {
  ROWNE_L(nrDnia("2026-11-03"), 20760);
  // poza blokiem (slotOd nieznany albo minęły 2 h) D5: S-K-37 w każdy dzień, parzysty i nieparzysty
  WejscieSlotu a = wejscie("2026-11-03", 12 * 60, nullptr, 21);
  WejscieSlotu b = wejscie("2026-11-04", 12 * 60, nullptr, 21);
  SPRAWDZ(wybierzSlot(a).ekran == EkranSlotu::SK37);
  SPRAWDZ(wybierzSlot(b).ekran == EkranSlotu::SK37);
  b.obrazekJest = false;
  SPRAWDZ(wybierzSlot(b).ekran == EkranSlotu::SK37);
  b.obrazekJest = true;
  b.liniaUwagi = true;
  SPRAWDZ(wybierzSlot(b).ekran == EkranSlotu::SK37);
  a.wspolrzedne = false;  // bez współrzędnych → S-K-38 jako zastępstwo (R-KON-54)
  SPRAWDZ(wybierzSlot(a).ekran == EkranSlotu::SK38);
  a.obrazekJest = false;  // bez współrzędnych i bez obrazka → ekran z rana (W4)
  SPRAWDZ(wybierzSlot(a).ekran == EkranSlotu::BRAK);
  // S-K-37: zachód w bloku B1, powód o długości dnia, bez kroku
  WejscieSlotu c = wejscie("2026-11-03", 12 * 60, nullptr, 21);
  WyborSlotu w = wybierzSlot(c);
  TekstySlotu t = teksty(w, c);
  ROWNE(t.podpis, "zachód słońca");
  ROWNE(t.godzina, hhmm(w.slonce.zachod));
  SPRAWDZ(zwykle(t.powod).rfind("Dzień krótszy o ", 0) == 0);
  SPRAWDZ(t.krok.empty());
  // D0: ekran z rana zostaje przy S5 i S10, dowód spokoju ustępuje
  SPRAWDZ(zostajeRano("S5", false) && zostajeRano("S10", false) && zostajeRano("S2A", true));
  SPRAWDZ(!zostajeRano("S2A", false) && !zostajeRano("S2B", false) && !zostajeRano("KM", false) && !zostajeRano("S6", false));
}

// T-TEST-340: obrazek dnia S-K-38 w pierwszym bloku slotu każdego dnia (R-KON-50)
static void testB27_blokObrazka() {
  SPRAWDZ(OBRAZEK_BLOK_MIN == 120);
  SPRAWDZ(wBlokuObrazka(8 * 60, 8 * 60) && wBlokuObrazka(9 * 60 + 59, 8 * 60) && !wBlokuObrazka(10 * 60, 8 * 60));
  SPRAWDZ(!wBlokuObrazka(7 * 60, 8 * 60) && !wBlokuObrazka(8 * 60, -1));
  // każdy dzień od 01.11 do 31.12, parzysty i nieparzysty: S-K-38 w całym bloku, potem S-K-37
  int dni38 = 0, dni = 0;
  for (int i = 0; i < 61; i++) {
    std::string d = dodajDni("2026-11-01", i);
    WejscieSlotu w = wejscie(d, 8 * 60, nullptr, 21);
    w.slotOd = 8 * 60;
    dni++;
    bool wszystkie = true;
    for (int m = 8 * 60; m < 10 * 60; m += 30) {
      w.minuta = m;
      wszystkie = wszystkie && wybierzSlot(w).ekran == EkranSlotu::SK38;
    }
    dni38 += wszystkie;
    w.minuta = 10 * 60;
    SPRAWDZ(wybierzSlot(w).ekran == EkranSlotu::SK37);  // po bloku D5: S-K-37
  }
  ROWNE_L(dni38, dni);
  // z linią uwagi bez obrazka w bloku (R-KON-54): zwykła kolejność D1…D5
  WejscieSlotu l = wejscie("2026-11-04", 8 * 60, nullptr, 21);
  l.slotOd = 8 * 60;
  l.liniaUwagi = true;
  SPRAWDZ(wybierzSlot(l).ekran == EkranSlotu::SK37);
  // bez obrazków w bloku: bez dziury, zwykła kolejność (S-K-37; bez współrzędnych ekran z rana)
  WejscieSlotu n = wejscie("2026-11-04", 8 * 60, nullptr, 21);
  n.slotOd = 8 * 60;
  n.obrazekJest = false;
  SPRAWDZ(wybierzSlot(n).ekran == EkranSlotu::SK37);
  n.wspolrzedne = false;
  SPRAWDZ(wybierzSlot(n).ekran == EkranSlotu::BRAK);
  // D0a (drzemka z wyciekiem) wygrywa z obrazkiem
  WejscieSlotu dr = wejscie("2026-11-04", 9 * 60, nullptr, 21);
  dr.slotOd = 8 * 60;
  dr.drzemka.minut = 70;
  dr.drzemka.wyciekP95 = 30;
  SPRAWDZ(wybierzSlot(dr).ekran == EkranSlotu::SK39);
  // wariant CO2 `Przewietrz teraz.` (R-KON-44) wygrywa z obrazkiem; po bloku okno wietrzenia wraca do głosu
  Prognoza p = prognoza("2026-11-04", 2, 10);
  WejscieSlotu c = wejscie("2026-11-04", 8 * 60, &p, 21);
  c.slotOd = 8 * 60;
  c.co2 = 1400;
  WyborSlotu wc = wybierzSlot(c);
  SPRAWDZ(wc.ekran == EkranSlotu::SK33 && wc.okno.wariant == Wietrzenie::CO2);
  c.co2 = -1;
  WyborSlotu bez = wybierzSlot(c);
  SPRAWDZ(bez.ekran == EkranSlotu::SK38);  // zwykłe okno S-K-33 w bloku ustępuje obrazkowi
  c.minuta = 10 * 60;
  WyborSlotu po = wybierzSlot(c);
  SPRAWDZ(po.ekran != EkranSlotu::SK38);  // po bloku wraca D1…D5
}

// ---------- R-KON-51: stopka dzienna ----------
static void testR_KON_51_stopka() {
  Noc n;
  n.data = "2026-11-03";
  n.m01 = 7 * 60 + 52;
  n.ahi = 1.2f;
  n.wp95 = 4;
  StopkaDzienna s = stopkaDzienna(&n, true, 0, 6, 21);
  ROWNE(zwykle(s.lewo), "noc 7 h 52 · AHI 1,2 · wyciek 4");
  ROWNE(s.prawo, "za oknem 6° · przy łóżku 21°");
  StopkaDzienna d = stopkaDzienna(&n, true, 70, 6, 21);
  ROWNE(zwykle(d.lewo), "noc 7 h 52 · drzemka 1 h 10 · AHI 1,2");
  SPRAWDZ(szer(F_P22B, d.lewo) + 24 + szer(F_P22B, d.prawo) <= 744);
  StopkaDzienna b = stopkaDzienna(&n, true, 0, NAN, 21);  // bez prognozy
  ROWNE(b.prawo, "przy łóżku 21°");
  StopkaDzienna z = stopkaDzienna(nullptr, true, 0, -3, NAN);
  ROWNE(zwykle(z.lewo), "noc – · AHI – · wyciek –");
  ROWNE(z.prawo, "za oknem −3°");
  StopkaDzienna st = stopkaDzienna(&n, false, 0, 6, 21);  // ostatnia zapisana noc nie jest ubiegłą
  SPRAWDZ(st.lewo.rfind("noc 03.11 ", 0) == 0);
}

// ---------- R-KON-56…R-KON-58: drzemka ----------
static void testR_KON_57_drzemka() {
  uint32_t koniecNocy = 1793700000;
  std::vector<SesjaDoby> s = {{koniecNocy - 30000, koniecNocy, 5},               // noc
                              {koniecNocy + 30000, koniecNocy + 30000 + 2400, 10},  // 40 min
                              {koniecNocy + 40000, koniecNocy + 40000 + 1200, 30},  // 20 min
                              {koniecNocy + 50000, koniecNocy + 50000 + 300, 50}};  // 5 min: pominięta
  Drzemka d = liczDrzemke(s, koniecNocy);
  ROWNE_L(d.minut, 60);
  SPRAWDZ(std::fabs(d.wyciekP95 - 30) < 1e-4);
  SPRAWDZ(wyciekDrzemki(d));
  // S-K-39 przed S-K-33 (D0a)
  Prognoza p = prognoza("2026-11-03", 1, 20);
  WejscieSlotu we = wejscie("2026-11-03", 17 * 60, &p, 21);
  we.drzemka = d;
  WyborSlotu w = wybierzSlot(we);
  SPRAWDZ(w.ekran == EkranSlotu::SK39);
  TekstySlotu t = teksty(w, we);
  ROWNE(t.tytul, "Drzemka: maska przeciekała.");
  ROWNE(zwykle(t.powod), "Wyciek w drzemce do 30 l/min, próg 24.");
  ROWNE(zwykle(t.krok), "→ Sprawdź poduszkę i paski przed nocą.");
  // P95 0 → brak S-K-39; sama sesja 5 min → bez drzemki
  Drzemka zero = liczDrzemke({{koniecNocy + 100, koniecNocy + 4300, 0}}, koniecNocy);
  SPRAWDZ(!wyciekDrzemki(zero) && zero.minut == 70);
  SPRAWDZ(liczDrzemke({{koniecNocy + 100, koniecNocy + 400, 40}}, koniecNocy).minut == 0);
}

// ---------- R-KON-59…R-KON-62: obrazek dnia ----------
static std::string pbm(int w, int h) {
  std::string s = "P4\n" + std::to_string(w) + " " + std::to_string(h) + "\n";
  s += std::string((size_t)((w + 7) / 8 * h), '\x55');
  return s;
}
static void testR_KON_60_dobor() {
  std::string manifest =
      "{\"wersja\":1,\"opis\":\"syntetyczny\",\"obrazki\":["
      "{\"plik\":\"001_pn.png\",\"dni\":[\"pn\"],\"pogoda\":[],\"sezon\":[],\"inne\":[]},"
      "{\"plik\":\"002_pn.pbm\",\"dni\":[\"pn\"]},"
      "{\"plik\":\"003_pn.pbm\",\"dni\":[\"pn\"],\"tekst\":\"z \\\"cudzysłowem\\\" i \\u0142\"},"
      "{\"plik\":\"004_listopad.pbm\",\"sezon\":[\"listopad\"]},"
      "{\"plik\":\"005_listopad.pbm\",\"sezon\":[\"listopad\"],\"wulgarne\":false},"
      "{\"plik\":\"006_nd.pbm\",\"dni\":[\"nd\"]},"
      "{\"plik\":\"007_deszcz.pbm\",\"pogoda\":[\"deszcz\"]},"
      "{\"plik\":\"008_mgla.pbm\",\"pogoda\":[\"mgla\"],\"inne\":[\"sen\"]},"
      "{\"plik\":\"009_n.pbm\"},{\"plik\":\"010_n.pbm\",\"dni\":[]},{\"plik\":\"011_n.pbm\"},"
      "{\"plik\":\"999_brak.pbm\",\"dni\":[\"pn\"]}]}";
  std::vector<PozycjaManifestu> m;
  SPRAWDZ(parsujManifest(manifest, m));
  ROWNE_L(m.size(), 12);
  SPRAWDZ(!maTagKontekstu(m[7]));  // nieznany tag `mgla` i `sen` ignorowane → pula neutralna
  std::vector<std::string> pliki;
  for (int i = 1; i <= 11; i++) {
    char b[64];
    const char* nazwy[] = {"", "001_pn", "002_pn", "003_pn", "004_listopad", "005_listopad", "006_nd", "007_deszcz",
                           "008_mgla", "009_n", "010_n", "011_n"};
    snprintf(b, sizeof b, "/obrazki/%s.pbm", nazwy[i]);
    pliki.push_back(b);
  }
  pliki.push_back("/obrazki/obrazki.json");
  KontekstObrazka k;
  k.dzis = "2026-11-02";  // poniedziałek, listopad
  auto czyt = [](const std::string&) { return true; };
  std::vector<WpisObrazka> dz;
  WyborObrazka a = wybierzObrazek(pliki, &m, dz, k, czyt);
  SPRAWDZ(a.pula == 'k' && a.nowy);
  bool kontekstowy = a.plik.find("_pn") != std::string::npos || a.plik.find("_listopad") != std::string::npos;
  SPRAWDZ(kontekstowy);
  // powtarzalność (R-KON-59, R-KON-61): te same wejścia → ten sam plik; wpis z dziś wygrywa (restart)
  SPRAWDZ(wybierzObrazek(pliki, &m, dz, k, czyt).plik == a.plik);
  dz.push_back({k.dzis, a.plik.substr(9), a.pula});
  WyborObrazka a2 = wybierzObrazek(pliki, &m, dz, k, czyt);
  SPRAWDZ(a2.plik == a.plik && !a2.nowy);
  // następny dzień S-K-38: pula neutralna (licznik nieparzysty); obrazek z `nd` nigdy we wtorek
  KontekstObrazka k2;
  k2.dzis = "2026-11-04";
  WyborObrazka b = wybierzObrazek(pliki, &m, dz, k2, czyt);
  SPRAWDZ(b.pula == 'n');
  SPRAWDZ(b.plik.find("_n.pbm") != std::string::npos || b.plik.find("_mgla") != std::string::npos);
  // przez kolejne dni każdy obrazek puli raz, zanim któryś się powtórzy (R-KON-61)
  std::vector<WpisObrazka> dz2;
  std::vector<std::string> neutralne;
  std::string dzien = "2026-11-03";  // wtorek: kontekst tylko listopad (004, 005)
  for (int i = 0; i < 16; i++) {
    KontekstObrazka kk;
    kk.dzis = dodajDni(dzien, 2 * i);  // co drugi dzień (rotacja D5)
    WyborObrazka w = wybierzObrazek(pliki, &m, dz2, kk, czyt);
    if (w.pula == 'n') neutralne.push_back(w.plik);
    dz2.push_back({kk.dzis, w.plik.substr(9), w.pula});
  }
  std::vector<std::string> pierwsze4(neutralne.begin(), neutralne.begin() + 4);
  std::sort(pierwsze4.begin(), pierwsze4.end());
  SPRAWDZ(std::unique(pierwsze4.begin(), pierwsze4.end()) == pierwsze4.end());  // 4 neutralne bez powtórzeń
  // dzień bez pasujących tagów → pula neutralna niezależnie od licznika (wtorek w październiku)
  KontekstObrazka k3;
  k3.dzis = "2026-10-06";
  SPRAWDZ(wybierzObrazek(pliki, &m, {}, k3, czyt).pula == 'n');
  // pogoda: deszcz tylko z prognozą (brak internetu → tag pogody nie pasuje, tag dni działa)
  Prognoza p = prognoza(k3.dzis, 10, 10);
  for (int h = 0; h < 24; h++) p.h[h].opad = 80;
  KontekstObrazka k4 = k3;
  k4.prognoza = &p;
  k4.godzina = 9;
  std::vector<std::string> tg = tagiDzis(k4);
  SPRAWDZ(std::find(tg.begin(), tg.end(), "deszcz") != tg.end());
  SPRAWDZ(std::find(tg.begin(), tg.end(), "brzydka") != tg.end());
  SPRAWDZ(std::find(tg.begin(), tg.end(), "zmienna") == tg.end());
  WyborObrazka dsz = wybierzObrazek(pliki, &m, {}, k4, czyt);
  SPRAWDZ(dsz.pula == 'k' && dsz.plik.find("007_deszcz") != std::string::npos);
  std::vector<std::string> bez = tagiDzis(k3);
  SPRAWDZ(std::find(bez.begin(), bez.end(), "deszcz") == bez.end());
  // tagi kalendarza
  KontekstObrazka k5;
  k5.dzis = "2026-12-01";
  std::vector<std::string> t5 = tagiDzis(k5);
  SPRAWDZ(std::find(t5.begin(), t5.end(), "zima") != t5.end());
  SPRAWDZ(std::find(t5.begin(), t5.end(), "poczatek_miesiaca") != t5.end());
  SPRAWDZ(std::find(t5.begin(), t5.end(), "wt") != t5.end());
  // R-KON-62: bez manifestu wszystkie pliki jak jedna pula neutralna, w logu `obrazki: bez manifestu`
  WyborObrazka bm = wybierzObrazek(pliki, nullptr, {}, k, czyt);
  SPRAWDZ(bm.pula == 'n' && !bm.plik.empty());
  SPRAWDZ(!bm.log.empty() && bm.log[0] == "obrazki: bez manifestu");
  std::vector<PozycjaManifestu> zly;
  SPRAWDZ(!parsujManifest("{\"obrazki\":[{\"plik\":", zly));
  SPRAWDZ(!parsujManifest("[1,2]", zly));
  // pusty katalog → brak obrazka (S-K-37, R-KON-54)
  SPRAWDZ(wybierzObrazek({}, &m, {}, k, czyt).plik.empty());
  // plik nieczytelny albo za duży → pominięty z wpisem w logu, wybór z reszty puli
  auto tylko002 = [](const std::string& s) { return s.find("002_pn") != std::string::npos; };
  WyborObrazka zp = wybierzObrazek(pliki, &m, {}, k, tylko002);
  SPRAWDZ(zp.plik.find("002_pn") != std::string::npos || zp.plik.empty() || zp.pula == 'n');
  // R-KON-59: dobór nie ma wejścia z danych terapii (KontekstObrazka: data, godzina, prognoza)
  // PBM
  ObrazPbm o;
  std::string powod;
  SPRAWDZ(parsujPbm(pbm(434, 434), o, powod) && o.w == 434 && o.h == 434 && o.bity.size() == 55u * 434u);
  SPRAWDZ(parsujPbm(std::string("P4\n# komentarz\n8 2\n\xFF\x00", 21), o, powod) && o.w == 8 && o.h == 2);
  SPRAWDZ(!parsujPbm(pbm(800, 435), o, powod));
  SPRAWDZ(!parsujPbm(pbm(801, 10), o, powod));
  SPRAWDZ(!parsujPbm("P5\n1 1\n\x00", o, powod));
  SPRAWDZ(!parsujPbm("P4\n100 100\n\x00\x00", o, powod));
  // dziennik
  WpisObrazka wo;
  SPRAWDZ(parsujWpisObrazka("2026-11-02;005_x.pbm;k", wo) && wo.pula == 'k' && wo.plik == "005_x.pbm");
  SPRAWDZ(!parsujWpisObrazka("2026-11-02;005_x.pbm;z", wo));
  ROWNE(zapiszWpisObrazka(wo), "2026-11-02;005_x.pbm;k");
}

// ---------- S-K-01, S-K-02, S-K-13 (5.8.3, 5.8.4, 5.8.5, R-KON-04, R-KON-05, R-KON-20, R-KON-29) ----------
static Co2Nocy noc(int odPpm, int godzinPonad) {
  std::vector<ProbkaCo2> p;
  uint32_t t0 = 1793660000;  // syntetyczna noc, odczyt co 30 min przez 7 h
  for (int i = 0; i < 14; i++) p.push_back({t0 + (uint32_t)i * 1800, i < 2 * godzinPonad ? 1300 : odPpm});
  return statystykaCo2(p);
}
static void testSK01() {
  Co2Nocy n5 = noc(800, 5), n1 = noc(800, 1), cisza = noc(800, 0);
  ROWNE_L(n5.minutPonad, 300);
  ROWNE_L(n1.minutPonad, 60);
  // rzadka seria : 23:30 i 05:00 ponad progiem → najwyżej 2 × 30 min, nie 5,5 h
  Co2Nocy rzadka = statystykaCo2({{1793660000, 1400}, {1793660000 + 5 * 3600 + 1800, 1400}});
  ROWNE_L(rzadka.minutPonad, 60);
  std::vector<WpisHumoru> dz;
  WejscieWieczoru w;
  w.dzis = "2026-05-12";
  w.noc = &n5;
  w.dziennik = &dz;
  WynikWieczoru r = ocenWieczor(w);
  SPRAWDZ(r.ekran == EkranWieczoru::SK01 && !r.zOdczytu && !r.doLogu);
  TrescKontekstu t = trescSK01(w, r);
  ROWNE(t.t.tytul, "Przewietrz sypialnię");
  ROWNE(t.t.etykieta, "Przed snem");
  ROWNE(zwykle(t.t.powod), "Zeszłej nocy CO2 przez 5 h ponad 1000 ppm.");
  ROWNE(t.t.krok, "→ Uchyl okno na noc.");
  SPRAWDZ(t.linia.size() == 14);
  // zeszła noc tylko 1 h ponad progiem → brak; bieżący odczyt ≥ 1000 → wariant `Powietrze w sypialni jest teraz zastałe.`
  w.noc = &n1;
  SPRAWDZ(ocenWieczor(w).ekran == EkranWieczoru::BRAK);
  w.co2Teraz = 1150;
  WynikWieczoru rt = ocenWieczor(w);
  SPRAWDZ(rt.ekran == EkranWieczoru::SK01 && rt.zOdczytu);
  ROWNE(trescSK01(w, rt).t.powod, "Powietrze w sypialni jest teraz zastałe.");
  w.co2Teraz = 999;
  SPRAWDZ(ocenWieczor(w).ekran == EkranWieczoru::BRAK);
  // PM2.5 za oknem blokuje (bez PM2.5 sypialni: próg zastępczy 35)
  w.noc = &n5;
  w.co2Teraz = -1;
  w.pm25ZaOknem = 60;
  SPRAWDZ(ocenWieczor(w).ekran == EkranWieczoru::BRAK);
  w.pm25ZaOknem = 12;
  SPRAWDZ(ocenWieczor(w).ekran == EkranWieczoru::SK01);
  // z PM2.5 sypialni: za oknem wyżej niż w sypialni i > 15 → S-K-02; za oknem wyżej, ale ≤ 15 → brak
  w.pm25Sypialni = 8;
  w.pm25ZaOknem = 40;
  w.co2Teraz = 1200;
  WynikWieczoru r2 = ocenWieczor(w);
  SPRAWDZ(r2.ekran == EkranWieczoru::SK02);
  ROWNE(trescSK02(w).t.powod, "Pył za oknem 40, w pokoju 8. Długie wietrzenie go wpuści.");
  ROWNE(trescSK02(w).t.krok, "→ Wietrz krótko, na noc zamknij.");
  w.pm25ZaOknem = 12;
  SPRAWDZ(ocenWieczor(w).ekran == EkranWieczoru::BRAK);
  w.pm25Sypialni = NAN;
  w.pm25ZaOknem = NAN;
  w.co2Teraz = -1;
  // tabela konfliktów 5.8.5: sypialnia ≤ 16 °C → krok z otuliną; pyłek wysoki → krok krótkiego wietrzenia
  w.tempSypialni = 15;
  ROWNE(trescSK01(w, ocenWieczor(w)).t.krok, "→ Uchyl okno, owiń rurę otuliną.");
  w.tempSypialni = 20;
  w.pylekWysoki = true;
  ROWNE(trescSK01(w, ocenWieczor(w)).t.krok, "→ Wietrz krótko, dużo pyłku.");
  w.pylekWysoki = false;
  // R-KON-04 poza sezonem (maj): warunek 5 dni z rzędu → dzień 1 i 4; w dniu 3 wartość o 30% wyższa → także dzień 3
  std::vector<std::string> pokazane;
  for (int i = 0; i < 5; i++) {
    WejscieWieczoru x = w;
    x.dzis = dodajDni("2026-05-04", i);
    WynikWieczoru y = ocenWieczor(x);
    if (y.ekran == EkranWieczoru::SK01) {
      pokazane.push_back(x.dzis);
      dz.push_back({x.dzis, "SK01", std::to_string(y.wartosc), 'D', "", ""});
    }
  }
  SPRAWDZ((pokazane == std::vector<std::string>{"2026-05-04", "2026-05-07"}));
  dz.clear();
  dz.push_back({"2026-05-04", "SK01", "300", 'D', "", ""});
  Co2Nocy n7 = noc(800, 7);  // 420 min ≥ 1,25 × 300
  WejscieWieczoru x3 = w;
  x3.dzis = "2026-05-06";
  x3.noc = &n7;
  SPRAWDZ(ocenWieczor(x3).ekran == EkranWieczoru::BRAK);  // wartość przy ostatnim pokazaniu nieznana: bez wyjątku
  x3.poprzedniaWartosc = 300;
  SPRAWDZ(ocenWieczor(x3).ekran == EkranWieczoru::SK01);
  // R-KON-29 sezon grzewczy: raz na 7 dni, bez wyjątku 25%; po 4 pokazaniach w sezonie milczy
  dz.clear();
  pokazane.clear();
  for (int i = 0; i < 40; i++) {
    WejscieWieczoru x = w;
    x.dzis = dodajDni("2026-11-01", i);
    x.noc = &n7;
    WynikWieczoru y = ocenWieczor(x);
    if (y.ekran == EkranWieczoru::SK01) {
      pokazane.push_back(x.dzis);
      dz.push_back({x.dzis, "SK01", std::to_string(y.wartosc), 'D', "", ""});
    }
  }
  SPRAWDZ((pokazane == std::vector<std::string>{"2026-11-01", "2026-11-08", "2026-11-15", "2026-11-22"}));
  SPRAWDZ(sezonGrzewczy("2026-10-01") && sezonGrzewczy("2027-04-30") && !sezonGrzewczy("2026-05-01"));
  // R-KON-05: tryb próbny → wpis w logu, ekran bez zmian
  dz.clear();
  WejscieWieczoru tp = w;
  tp.trybProbny = true;
  SPRAWDZ(ocenWieczor(tp).doLogu);
  SPRAWDZ(!TRYB_PROBNY_KONTEKSTU);  // do potwierdzenia
  // karta 1 na 2 wyświetlenia sprawy (5.7.3, R-HUM-07, src/humor.h ekranKarty()): 1. karta, po karcie dane
  std::vector<WpisHumoru> k;
  EkranDnia e1 = ekranKarty("SK01", "co2", "2026-11-01", k, "→ Uchyl okno na noc.");
  SPRAWDZ(e1.kartaZTabeli);
  k.push_back({"2026-11-01", "SK01", e1.klucz, 'K', "W01", ""});
  SPRAWDZ(!ekranKarty("SK01", "co2", "2026-11-02", k, "→ Uchyl okno na noc.").kartaZTabeli);
  // karta K05: tytuł z kropką, krok mieści się w kolumnie karty (5.7.2)
  w.co2Teraz = 1150;
  w.pm25ZaOknem = 12;
  TrescKontekstu kk = trescSK01(w, ocenWieczor(w));
  kk.t.tytul += ".";
  SPRAWDZ(kartaMiesci(kk.t));
  ROWNE_L(kk.t.fakty.size(), 2);
  // seria nocy w rekordzie sypialni wersja 2: bufor → rekord → wiersz → Co2Nocy
  BuforSypialni bs;
  for (int i = 0; i < 8; i++) {
    if (i == 0) dodajCo2(bs, "2026-11-03", 23 * 60 + 30, 1300);
    else dodajCo2(bs, "2026-11-04", 30 * (i - 1), i < 6 ? 1300 : 800);
  }
  NocSypialni ns;
  co2ZBufora(bs, ns);
  ROWNE_L(ns.co2Maks, 1300);
  ROWNE_L(ns.co2Ponad, 180);
  NocSypialni ps;
  SPRAWDZ(parsujSypialnie("2026-11-04;21.0;45.0;1100" + polaCo2V2(ns), ps));
  Co2Nocy zr = co2ZNocy(&ps);
  ROWNE_L(zr.minutPonad, 180);
  ROWNE_L(zr.wartosci.size(), 8);
  SPRAWDZ(parsujSypialnie("2026-11-04;21.0;45.0", ps) && parsujSypialnie("2026-11-04;21.0;45.0;900", ps));
  SPRAWDZ(!parsujSypialnie("2026-11-04;21.0;45.0;900;1;2", ps));
  // r01: powód z wyzwalacza wyboru, nie z oceny w tej chwili; bez wyzwalacza pusty, nigdy `0 h`
  WynikWieczoru brak;
  SPRAWDZ(trescSK01(w, brak).t.powod.empty());
  WynikWieczoru zap;
  zap.ekran = EkranWieczoru::SK01;
  zap.zOdczytu = true;
  zap.wartosc = 1250;
  ROWNE(trescSK01(w, zap).t.powod, "Powietrze w sypialni jest teraz zastałe.");
  // r07: prośba dziś i 4 wieczory z krokiem w 7 dniach blokują; wyjątek 25% dla ppm
  WejscieWieczoru pr = w;
  pr.dziennik = nullptr;
  pr.prosbaDzis = true;
  SPRAWDZ(ocenWieczor(pr).ekran == EkranWieczoru::BRAK);
  pr.prosbaDzis = false;
  pr.wieczorowZKrokiem7 = 4;
  SPRAWDZ(ocenWieczor(pr).ekran == EkranWieczoru::BRAK);
  std::vector<WpisHumoru> dzp = {{"2026-05-05", "SK01", "co2#1", 'D', "", ""}};
  WejscieWieczoru pp = w;
  pp.dzis = "2026-05-06";
  pp.noc = nullptr;
  pp.dziennik = &dzp;
  pp.co2Teraz = 1300;
  pp.poprzedniZOdczytu = true;
  pp.poprzedniaWartosc = 1100;  // 1300 < 1,25 × 1100
  SPRAWDZ(ocenWieczor(pp).ekran == EkranWieczoru::BRAK);
  pp.co2Teraz = 1400;  // ≥ 1375
  SPRAWDZ(ocenWieczor(pp).ekran == EkranWieczoru::SK01);
  pp.poprzedniZOdczytu = false;  // inny rodzaj wyzwalacza: bez wyjątku
  SPRAWDZ(ocenWieczor(pp).ekran == EkranWieczoru::BRAK);
}

static void testSK13() {
  Co2Nocy zaduch = noc(800, 5);  // 300 min > 1000
  std::vector<WpisHumoru> dz;
  WejscieRanka r;
  r.dzis = "2026-11-15";
  r.noc = &zaduch;
  r.dziennik = &dz;
  WynikRanka a = ocenRano(r);
  SPRAWDZ(a.sk13);
  TrescKontekstu t = trescSK13(zaduch);
  ROWNE(zwykle(t.t.powod), "CO2 w nocy doszło do 1300 ppm. Wtedy sen bywa płytszy.");
  ROWNE(t.t.tytul, "Zaduch w sypialni w nocy");
  ROWNE(t.t.krok, "→ Dziś wieczorem spróbuj z uchylonym oknem.");
  // maksimum > 2000 ppm wystarcza
  Co2Nocy szczyt = statystykaCo2({{1793660000, 900}, {1793661800, 2100}, {1793663600, 900}});
  r.noc = &szczyt;
  SPRAWDZ(ocenRano(r).sk13);
  // 3 h ponad progiem i maksimum 1300 → brak
  Co2Nocy krotko = noc(800, 3);
  r.noc = &krotko;
  SPRAWDZ(!ocenRano(r).sk13);
  r.noc = &zaduch;
  // R-KON-20: wieczorem S-K-01 → rano brak; ostatnie S-K-13 3 dni temu → brak do dnia 8
  r.poprzedniWieczorSK01 = true;
  SPRAWDZ(!ocenRano(r).sk13);
  r.poprzedniWieczorSK01 = false;
  dz.push_back({"2026-11-12", "SK13", "300", 'D', "", ""});
  SPRAWDZ(!ocenRano(r).sk13);
  r.dzis = "2026-11-19";
  SPRAWDZ(ocenRano(r).sk13);
  // spanie poza domem (R-CZY-12)
  r.pozaDomem = true;
  SPRAWDZ(!ocenRano(r).sk13);
}

// ---------- R-KON-41: granice slotu (start i koniec, plan) ----------
static void testR_KON_41_granice() {
  WejsciePlanu w;
  w.zegar = true;
  w.sekundaDoby = 7 * 3600;
  w.nocKompletna = true;
  w.importNocy = 6 * 3600 + 55 * 60;
  w.koniecTerapii = 6 * 3600 + 40 * 60;
  ROWNE_L(poczatekSlotuDziennego(w), 7 * 3600 + 55 * 60);
  w.importNocy = 20 * 3600 + 30 * 60;  // start wypadałby po początku slotu wieczornego → brak slotu
  w.koniecTerapii = -1;
  SPRAWDZ(poczatekSlotuDziennego(w) < 0);
  w.importNocy = 19 * 3600 + 59 * 60;  // start 20:59 → jeszcze w slocie
  ROWNE_L(poczatekSlotuDziennego(w), 20 * 3600 + 59 * 60);
  ROWNE_L(PLAN_SLOT_DZIENNY_DO, 21 * 60);
}

// ---------- r04, r06, r09: linia uwagi na ekranie slotu, ustawienia z pliku, drzemka z raportu doby ----------
struct PlotnoT : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)(y * 800 + x)] = c;
  }
  int czarnych(int y0, int y1) const {
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = 0; x < 800; x++) n += px[(size_t)(y * 800 + x)];
    return n;
  }
};
// ---------- R-KON-63: wizualizacja slotu dziennego przy linii uwagi, podpisy godzin osi ----------
// wymaganie: R-KON-63
static void testR_KON_63_wykresPrzyLinii() {
  ROWNE(godzinaOsiSlotu(20), "20:00");
  ROWNE(godzinaOsiSlotu(7), "07:00");
  ROWNE(godzinaOsiSlotu(25), "01:00");
  ROWNE(godzinaOsiSlotu(0), "00:00");
  // S-K-37 z linią uwagi: pas doby zostaje (y 294…324), wiersz `wschód` / `dzień` na 392 też
  WejscieSlotu we = wejscie("2026-11-04", 12 * 60, nullptr, 21);
  we.obrazekJest = false;
  we.liniaUwagi = true;
  we.linia = "Wciąż: Noce z AHI powyżej 5.";
  we.liniaOstrzezenie = true;
  WyborSlotu w = wybierzSlot(we);
  SPRAWDZ(w.ekran == EkranSlotu::SK37);
  PlotnoT a;
  rysujSlotDzienny(a, {"śr 04.11", ""}, w, tekstySlotu(w, we), we, stopkaDzienna(nullptr, true, 0, NAN, 21));
  SPRAWDZ(a.czarnych(296, 322) > 8000);   // noc czarna po obu stronach dnia (ok. 440 px × 26)
  SPRAWDZ(a.czarnych(372, 396) > 500);    // `wschód` / `dzień`
  SPRAWDZ(a.czarnych(271, 279) < 200);    // nad pasem tylko znacznik i podpis `teraz`
  // S-K-33 z tytułem B3 i powodem w 2 liniach (wariant bez godziny) przy linii uwagi: wykres ustępuje
  Prognoza p = prognoza("2026-11-03", 1, 80);  // smog cały dzień: `Dziś bez wietrzenia.`
  WejscieSlotu ws = wejscie("2026-11-03", 11 * 60, &p, 21);
  ws.obrazekJest = false;
  ws.liniaUwagi = true;
  ws.linia = "Wciąż: Bezdechy częstsze.";
  WyborSlotu w2 = wybierzSlot(ws);
  if (w2.ekran == EkranSlotu::SK33) {
    TekstySlotu t2 = tekstySlotu(w2, ws);
    PlotnoT b;
    rysujSlotDzienny(b, {"wt 03.11", ""}, w2, t2, ws, stopkaDzienna(nullptr, true, 0, NAN, 21));
    bool dwie = t2.godzina.empty() && zawin(F_P28B, t2.powod, 744).size() > 1;
    if (dwie) SPRAWDZ(b.czarnych(350, 354) < 100);   // ustępuje
    else SPRAWDZ(b.czarnych(350, 354) > 1000);       // zostaje
    // gałąź „powód w 2 liniach”: realne teksty S-K-33 jej dziś nie dają, więc treść zbudowana ręcznie
    TekstySlotu t3 = t2;
    t3.godzina.clear();
    t3.tytul = "Dziś bez wietrzenia.";
    t3.powod = "Smog za oknem przez cały dzień, a w sypialni powietrze jeszcze dość czyste i chłodne.";
    SPRAWDZ(zawin(F_P28B, t3.powod, 744).size() == 2);
    PlotnoT c;
    rysujSlotDzienny(c, {"wt 03.11", ""}, w2, t3, ws, stopkaDzienna(nullptr, true, 0, NAN, 21));
    SPRAWDZ(c.czarnych(350, 354) < 100);  // oś słupków (352) nie narysowana: wykres ustąpił
    SPRAWDZ(c.czarnych(410, 422) > 0 || t3.krok.empty());  // krok zostaje
  }
  SPRAWDZ(w2.ekran == EkranSlotu::SK33);  // dane testu trafiają w S-K-33
  // linia wzorca A (60 px zamiast 48): ciasny wykres niższy o 12 px, więc między tekstem a wykresem pas pusty
  // (S-K-36 słupki pyłku, S-K-37 pas doby; S-K-33 w obu trybach sprawdza testy_slot_ustawienia_sprzet.cpp na całej siatce)
  for (int ekr = 0; ekr < 2; ekr++) {
    WejscieSlotu wa = wejscie("2026-11-04", 12 * 60, nullptr, 21);
    wa.obrazekJest = false;
    wa.liniaUwagi = true;
    wa.linia = "Wciąż: Noce z AHI powyżej 5.";
    wa.liniaA = true;
    wa.liniaOstrzezenie = true;
    WyborSlotu wy = wybierzSlot(wa);
    TekstySlotu ta;
    if (ekr == 0) {
      wy.ekran = EkranSlotu::SK36;
      wy.pylek.rodzaj = 0;
      wy.pylek.wartosci.assign(12, 40.0f);
      wy.pylek.wysoko.assign(12, true);
      ta.etykieta = "PYŁEK";
      ta.tytul = "Brzoza pyli mocno.";
      ta.powod = "Najwięcej po południu.";
    } else {
      SPRAWDZ(wy.ekran == EkranSlotu::SK37);
      ta = tekstySlotu(wy, wa);
    }
    PlotnoT z;
    rysujSlotDzienny(z, {"śr 04.11", ""}, wy, ta, wa, stopkaDzienna(nullptr, true, 0, NAN, 21));
    if (ekr == 0) {
      SPRAWDZ(z.czarnych(266, 292) == 0);   // pod powodem (255 + ogonki) a nad znacznikiem `teraz` (292) pusto
      SPRAWDZ(z.czarnych(350, 354) > 1000);  // oś słupków zostaje
    } else {
      SPRAWDZ(z.czarnych(308, 334) > 8000);  // pas doby o 12 px niżej (306…336)
      SPRAWDZ(z.czarnych(372, 396) > 500);   // `wschód` / `dzień` na 392
    }
  }
}

static void testLiniaUwagiIPrzypadkiDodatkowe() {
  // r04: linia uwagi (ostrzeżenie) pod nagłówkiem S-K-33; od 10.11.2026 wizualizacja zostaje, niższa (R-KON-63),
  // krok zostaje
  Prognoza p = prognoza("2026-11-03", 1, 20);
  WejscieSlotu we = wejscie("2026-11-03", 11 * 60, &p, 21);
  we.linia = "Wciąż: Bezdechy częstsze.";
  we.liniaOstrzezenie = true;
  we.liniaUwagi = true;
  WyborSlotu w = wybierzSlot(we);
  SPRAWDZ(w.ekran == EkranSlotu::SK33);
  PlotnoT a, b;
  rysujSlotDzienny(a, {"wt 03.11", ""}, w, tekstySlotu(w, we), we, stopkaDzienna(nullptr, true, 0, NAN, 21));
  SPRAWDZ(a.czarnych(56, 100) > 30000);  // czarny pasek 48 px
  WejscieSlotu bez = we;
  bez.linia.clear();
  bez.liniaUwagi = false;
  rysujSlotDzienny(b, {"wt 03.11", ""}, w, tekstySlotu(w, bez), bez, stopkaDzienna(nullptr, true, 0, NAN, 21));
  SPRAWDZ(b.czarnych(340, 356) > 1000);   // bez linii: oś słupków na 352
  SPRAWDZ(a.czarnych(350, 354) > 1000);   // z linią: oś słupków zostaje (R-KON-63)
  SPRAWDZ(a.czarnych(271, 279) == 0);     // odstęp między blokiem godziny (do 270) a wykresem (od 280)
  SPRAWDZ(a.czarnych(395, 420) > 500);    // krok zostaje
  // r11: stopka biała pod negatywem S-K-39
  WejscieSlotu wd = wejscie("2026-11-03", 17 * 60, nullptr, 21);
  wd.drzemka.minut = 70;
  wd.drzemka.wyciekP95 = 31;
  WyborSlotu w39 = wybierzSlot(wd);
  PlotnoT c;
  rysujSlotDzienny(c, {"wt 03.11", ""}, w39, tekstySlotu(w39, wd), wd, stopkaDzienna(nullptr, true, 70, NAN, 21));
  SPRAWDZ(c.czarnych(100, 110) > 7000);  // negatyw
  SPRAWDZ(c.czarnych(470, 480) < 500);    // stopka biała
  // r06: ustawienia z pliku konfiguracji z wartościami domyślnymi 5.9.2
  UstawieniaDnia d = parsujUstawieniaDnia("");
  ROWNE_L(d.sprzet[EL_FILTR].okres, 4);  // filtr 4 tygodnie, maska 6 miesięcy
  SPRAWDZ(d.sprzet[EL_FILTR].tygodnie && !d.sprzet[EL_MASKA].tygodnie && d.sprzet[EL_MASKA].okres == 6);
  SPRAWDZ(d.sprzet[EL_FILTR].wymieniono.empty() && !d.pylki.jakikolwiek());
  UstawieniaDnia u = parsujUstawieniaDnia("# komentarz\nwymiana.filtr=2026-09-10\nokres.rura=6\npoduszka=pianka\nwymiana.komora=2026-01-01\n"
                                          "pylki=brzoza,trawy\npylek.brzoza=120\nzly=1\nwymiana.maska=31-12\n");
  ROWNE(u.sprzet[EL_FILTR].wymieniono, "2026-09-10");
  // rura, komora i typ poduszki czytane bez błędu i ignorowane (bez zmiany okresu maski i filtra)
  ROWNE_L(u.sprzet[EL_MASKA].okres, 6);
  ROWNE_L(u.sprzet[EL_FILTR].okres, 4);
  SPRAWDZ(u.pylki.zaznaczony[PYL_BRZOZA] && u.pylki.zaznaczony[PYL_TRAWY] && !u.pylki.zaznaczony[PYL_OLCHA]);
  SPRAWDZ(std::fabs(u.pylki.prog[PYL_BRZOZA] - 120) < 1e-4 && std::fabs(u.pylki.prog[PYL_TRAWY] - 50) < 1e-4);
  ROWNE_L(u.bledy.size(), 2);
  // r06: drzemka z raportu doby
  std::vector<SesjaDoby> ses;
  float p95 = NAN;
  SPRAWDZ(sesjeZRaportu("{\"sessions\":[{\"id\":1,\"start_ms\":1000000,\"end_ms\":2000000},{\"id\":2,\"start_ms\":5000000,"
                        "\"end_ms\":9200000}],\"channels\":[{\"id\":\"Leak\",\"unit\":\"L/min\",\"p95\":30.5}]}",
                        ses, p95));
  ROWNE_L(ses.size(), 2);
  SPRAWDZ(std::fabs(p95 - 30.5f) < 1e-4);
  SPRAWDZ(!sesjeZRaportu("{\"day\":1}", ses, p95));
  SPRAWDZ(std::fabs(wyciekDrzemkiZDoby(30.5f, 9.6f) - 30.5f) < 1e-4);
  SPRAWDZ(std::isnan(wyciekDrzemkiZDoby(9.6f, 9.6f)));
  Drzemka dz;
  dz.minut = 70;
  dz.wyciekP95 = 30.5f;
  dz.od = 100;
  dz.do_ = 200;
  Drzemka dz2;
  SPRAWDZ(parsujDrzemke(zapiszDrzemke("2026-11-04", dz), "2026-11-04", dz2) && dz2.minut == 70 && std::fabs(dz2.wyciekP95 - 30.5f) < 1e-4);
  SPRAWDZ(!parsujDrzemke(zapiszDrzemke("2026-11-03", dz), "2026-11-04", dz2));
  std::string pd;
  int pm = 0;
  SPRAWDZ(prefiksSesji("20261104_151000", pd, pm) && pd == "2026-11-04" && pm == 15 * 60 + 10);
  SPRAWDZ(!prefiksSesji("", pd, pm) && !prefiksSesji("2026-11-04", pd, pm));
  // r09: S-K-13 (R8) wypiera rozruch S6 i dowód spokoju (R9), kamień (R7) nie
  ROWNE_L(grupaRano("S6"), 9);
  ROWNE_L(grupaRano("S2B"), 9);
  ROWNE_L(grupaRano("KM"), 7);
  // r02: brak PM2.5 w horyzoncie i pyłek nie są smogiem
  Prognoza bp = prognoza("2026-11-04", 2, 20);
  for (auto& g : bp.h) g.pm25 = NAN;
  SPRAWDZ(okno(bp, "2026-11-04", 9, 21).wariant == Wietrzenie::BRAK);
}

int main() {
  testR_KON_49_slonce();
  testPrognozaOpenMeteo();
  testR_KON_43_regula();
  testR_KON_44_co2();
  testR_KON_45_pylek();
  testR_KON_47_jutro();
  testR_KON_48_wymiana();
  testR_KON_50_rotacja();
  testB27_blokObrazka();
  testR_KON_51_stopka();
  testR_KON_57_drzemka();
  testR_KON_60_dobor();
  testSK01();
  testSK13();
  testR_KON_41_granice();
  testLiniaUwagiIPrzypadkiDodatkowe();
  testR_KON_63_wykresPrzyLinii();
  printf("testy_slot_dzienny: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
