// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych: przerywniki S2d. Szkoda, której
// szukamy: karta mówi pacjentowi coś fałszywego o jego nocy albo zasłania ważniejszy ekran. PRZYPADEK R25-nn pada z numerem
// do poprawki albo decyzji, TRZYMA pilnuje tego, co potwierdzono. Dyrygent z prawdziwym silnikiem reguł albo z atrapą jak
// test/testy_ekrany_co2_brzegowe.cpp, historia w pliku panelu. Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "dzien.h"
#include "humor.h"
#include "noc.h"
#include "slot_dzienny.h"
#include "sprawy.h"
#include "start.h"
#include "wspolne.h"
#include "atrapy/zestaw_kolejki.h"
#include "magazyn.h"
#include "reguly_pamiec.h"
#include "stany.h"

static uint32_t ep(int r, int mi, int d, int h, int m) {
  std::tm t{};
  t.tm_year = r - 1900; t.tm_mon = mi - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = m; t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static uint32_t epDnia(const std::string& data, int h, int m) {
  return ep(atoi(data.substr(0, 4).c_str()), atoi(data.substr(5, 2).c_str()), atoi(data.substr(8, 2).c_str()), h, m);
}
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static std::string status(const std::string& mop) {
  return "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"" + mop +
         "\",\"system\":\"IDLE\",\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}
// zdrowa noc: AHI i inne pola lekko zmienne, żeby norma miała pasmo; tryb i ustawienia jak ASVAuto
static Noc nocZ(const std::string& data, const std::string& tryb = "ASVAuto") {
  int h = (int)(nrDnia(data) % 7);
  Noc n;
  n.data = data; n.wazna = true; n.zrodlo = 'A'; n.import = epDnia(data, 6, 0);
  n.m01 = 420 + h * 5; n.ahi = 1.0f + 0.1f * (float)h; n.oa = 0.3f; n.ca = 0.4f + 0.02f * (float)h; n.h = 0.3f; n.csr = 0;
  n.cmed = 9.8f; n.cp95 = 13.0f + 0.05f * (float)h; n.wmed = 2.5f; n.wp95 = 9.0f + 0.3f * (float)h;
  n.m13 = 15; n.m17 = 0; n.m08 = 0; n.ua = 0.2f; n.m10 = 0.05f; n.m10p = 0.2f; n.pna = "AirCurve_10_CS_PaceWave"; n.srn = "SYN1";
  n.tryb = tryb; n.m18 = 660;
  if (tryb == "ASVAuto" || tryb == "ASV") { n.emed = 6.8f; n.ep95 = 8.2f; n.psmed = 3.8f; n.psp95 = 7.0f; }
  if (tryb == "ASVAuto") { n.ustawienia = "ASVAuto/5/10/3/15"; n.eai = 5; n.eax = 10; n.ans = 3; n.axs = 15; }
  else if (tryb == "ASV") { n.ustawienia = "ASV/6/3/15"; n.eep = 6; n.mns = 3; n.mxs = 15; }
  else if (tryb == "AutoSet") { n.ustawienia = "AutoSet/7/15/2"; n.mpi = 7; n.mpa = 15; n.epr = 2; }
  else if (tryb == "CPAP") { n.ustawienia = "CPAP/9/2"; n.ipc = 9; n.epr = 2; }
  return n;
}
static const char* POLA_V3 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb;m18";
struct Wyb { std::string ekran, linia; std::vector<std::string> log; };
struct Sym {
  Panel p;
  bool pierwszy = true;
  std::string trybStatus = "ASVAuto";
  std::map<std::string, Noc> noce;  // data poranka → rekord
  // historia zdrowa do dnia `dzis` włącznie (ileDni wstecz); nadpisanie: s.noce[data] = …
  void historia(const std::string& dzis, int ileDni = 80, const std::string& tryb = "ASVAuto") {
    trybStatus = tryb;
    for (int i = 0; i <= ileDni; i++) { std::string d = dodajDni(dzis, -i); noce[d] = nocZ(d, tryb); }
  }
  void zapisz() {
    std::map<std::string, std::vector<const Noc*>> lata;
    for (auto& x : noce) lata[x.first.substr(0, 4)].push_back(&x.second);
    for (auto& l : lata) {
      std::string t = std::string("#cpap-eink noce 3\n") + POLA_V3 + "\n";
      for (auto* n : l.second) t += wierszNocyV3(*n) + "\n";
      p.pliki.pliki["/dane/noce/" + l.first + ".csv"] = t + "#koniec " + std::to_string(l.second.size()) + "\n";
    }
  }
  Wyb wybudz(const std::string& dzis, int h, int m = 0, bool przepiszHistorie = true) {
    if (przepiszHistorie) zapisz();
    long doba = nrDnia(dzis) - 1;
    p.siec.odpowiedzi.clear();
    p.siec.siecJest = true;
    p.siec.dodaj("/api/status", 200, status(trybStatus));
    p.siec.dodaj(sciezkaListy(), 200, lista(doba - 2, doba));
    p.zegar.epoch = epDnia(dzis, h, m);
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = pierwszy ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = pierwszy ? PowodWybudzenia::START : PowodWybudzenia::ZEGAR;
    pierwszy = false;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.port.log.clear();
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    Wyb w; w.log = p.port.log;
    for (auto& l : w.log) {
      size_t k = l.find("ekran dnia");
      if (k != std::string::npos) w.ekran = l.substr(k);
      k = l.find("ekran S-K");
      if (k != std::string::npos && w.ekran.empty()) w.ekran = l.substr(k);
      k = l.find("linia uwagi:");
      if (k != std::string::npos) w.linia = l.substr(k);
    }
    return w;
  }
};

static int ok = 0, zle = 0;
#define PRZYPADEK(id, w)                                                       \
  do {                                                                          \
    if (w) ok++;                                                                \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", id, __FILE__, __LINE__, #w); } \
  } while (0)
// znane ograniczenie zostawione świadomie (czeka na decyzję, otwarte pytania); nie liczy się jako błąd
#define OCZEKUJE(id, w)                                                         \
  do {                                                                          \
    if (w) ok++;                                                                \
    else printf("OCZEKUJE NA DECYZJĘ %s (%s:%d)\n", id, __FILE__, __LINE__);    \
  } while (0)
#define TRZYMA(w)                                                               \
  do {                                                                          \
    if (w) ok++;                                                                \
    else { zle++; printf("REGRES (%s:%d): %s\n", __FILE__, __LINE__, #w); }     \
  } while (0)

static Noc R(const std::string& d, float ahi, float wp, int m01 = 480, float m08 = 0) {
  Noc n = nocZ(d);
  n.ahi = ahi; n.wp95 = wp; n.m01 = m01; n.m08 = m08;
  n.oa = ahi * 0.5f; n.ca = ahi * 0.2f; n.h = ahi * 0.3f;
  n.wazna = m01 >= 240;
  return n;
}
static bool karta(const Wyb& w) { return w.ekran.find("S2D-") != std::string::npos; }
static bool warunkowa(const Wyb& w, const char* g) { return w.ekran.find(std::string("S2D-") + g + "-warunkowa") != std::string::npos; }
static bool neutralna(const Wyb& w, const char* g) { return w.ekran.find(std::string("S2D-") + g + "-neutralna") != std::string::npos; }
static std::string sc(int g) { char s[8]; snprintf(s, sizeof s, "G%02d", g); return s; }
static NocPrzerywnika fakty(const std::vector<Noc>& h, const std::string& d, bool stan = false) { return nocPrzerywnika(h, d, stan); }

// ---------- atrapa silnika, jak test/testy_ekrany_co2_brzegowe.cpp: wybrane wzorce L aktywne ----------
static reguly::Zestaw ZESTAW = atrapa_b5::zestaw();
static std::vector<std::string> KLUCZE;
static std::vector<reguly::Wynik> wyniki(const reguly::Kontekst&) {
  std::vector<reguly::Wynik> w;
  for (auto& k : ZESTAW.katalog) {
    std::string kl = k.wariant.empty() ? k.id : k.id + "/" + k.wariant;
    bool sp = std::find(KLUCZE.begin(), KLUCZE.end(), kl) != KLUCZE.end();
    w.push_back(atrapa_b5::wynik(ZESTAW, k.id, k.wariant, k.stan != reguly::StanReguly::ZABLOKOWANA, sp,
                                 {{"x", "1,4"}, {"n", "3"}, {"min", "42"}, {"m", "27"}}));
  }
  return w;
}
static void atrapaWl() {
  sprawy::zrodloTestowe.wyniki = wyniki;
  sprawy::zrodloTestowe.zestaw = &ZESTAW;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW);
}
static void atrapaWyl() { sprawy::zrodloTestowe = sprawy::ZrodloWynikow{}; }
static void omowione(Sym& s, const std::string& kl, const std::string& data, reguly::Pilnosc pil) {
  s.p.pliki.montujeSie = true;
  s.p.pliki.montowane = true;
  Magazyn m(s.p.pliki, [](const std::string&) {});
  reguly::Pamiec pa;
  sprawy::zapiszOmowione(pa, kl, data, pil);
  reguly::zapiszPamiec(m, pa);
}

// ---------- 1. warunek wobec tekstu, granice progów ----------
static void testGranice() {
  const std::string d = "2026-11-12";
  auto f = [&](float a, float w, int m) { return fakty({R(d, a, w, m)}, d); };
  // granice z listy kontrolnej: 5; 4,99; 24; 4 h; 7 h
  TRZYMA(warunekPrzerywnika("G09", f(5.0f, 10, 480)) && !warunekPrzerywnika("G05", f(5.0f, 10, 480)));
  TRZYMA(!warunekPrzerywnika("G09", f(4.94f, 10, 480)) && warunekPrzerywnika("G05", f(4.94f, 10, 480)));  // 4,99 → „5,0”, nie „w celu”
  TRZYMA(!warunekPrzerywnika("G12", f(1, 24.0f, 480)) && !warunekPrzerywnika("G18", f(1, 24.0f, 480)));
  // wariant C: P95 ≥ 15 L/min = noc niepewna, więc G18 („bez zarzutu”) dopiero poniżej 15
  TRZYMA(warunekPrzerywnika("G12", f(1, 24.01f, 480)) && warunekPrzerywnika("G18", f(1, 14.99f, 480)) &&
         !warunekPrzerywnika("G18", f(1, 23.99f, 480)) && !warunekPrzerywnika("G18", f(1, 15.0f, 480)));
  TRZYMA(!warunekPrzerywnika("G11", f(1, 10, 240)) && warunekPrzerywnika("G11", f(1, 10, 239)));
  {  // G16 „kolejna” noc: z poprzednią ważną nocą
    auto g = [&](int m) {  // 6 poprzednich nocy po 5 h 30 (poprawa ponad medianę), potem próg 7 h
      std::vector<Noc> h;
      for (int i = 6; i >= 1; i--) h.push_back(R(dodajDni(d, -i), 1.0f, 10, 330));
      h.push_back(R(d, 1.0f, 10, m));
      return fakty(h, d);
    };
    TRZYMA(!warunekPrzerywnika("G16", g(419)) && warunekPrzerywnika("G16", g(420)));
  }
  TRZYMA(!warunekPrzerywnika("G15", f(1, 10, 359)) && warunekPrzerywnika("G15", f(1, 10, 360)));
  TRZYMA(warunekPrzerywnika("G13", f(0.99f, 10, 480)) && !warunekPrzerywnika("G13", f(1.0f, 10, 480)));
  TRZYMA(warunekPrzerywnika("G10", f(10.0f, 10, 480)) && !warunekPrzerywnika("G10", f(9.99f, 10, 480)));

  // G16 `To była dobra noc.` (wcześniej z drugim zdaniem o konsekwencji), G06 `okoliczności są sprzyjające` warunek patrzy
  // tylko na M-01: AHI 30 i wyciek 40 to dalej „solidna noc”
  PRZYPADEK("testGranice", !warunekPrzerywnika("G16", f(30.0f, 40.0f, 480)));
  PRZYPADEK("testGranice", !warunekPrzerywnika("G06", f(30.0f, 40.0f, 480)));
  {  // „Kolejna” bez poprzedniej nocy
    std::vector<Noc> h = {R(dodajDni(d, -1), 1.0f, 10, 60), R(d, 1.0f, 10, 480)};
    PRZYPADEK("testGranice", !warunekPrzerywnika("G16", fakty(h, d)));
  }
  // G15 `wycieki niewielkie` przy P95 23,9 L/min (0,1 od progu dużego wycieku) i 29 minutach dużego wycieku (M-08);
  // G18 `Wyniki terapii bez zarzutu` przy 4 h 00, AHI 4,99 (na karcie `5,0`) i tym samym wycieku; także przy stanie odchylenia AHI
  PRZYPADEK("testGranice", !warunekPrzerywnika("G15", fakty({R(d, 1.0f, 23.9f, 480, 29.0f)}, d)));
  PRZYPADEK("testGranice", !warunekPrzerywnika("G18", f(4.99f, 23.9f, 240)));
  PRZYPADEK("testGranice", !warunekPrzerywnika("G18", fakty({R(d, 2.0f, 10, 480)}, d, true)));
  // G08 `AHI pozostaje niskie`, wczoraj AHI 35: warunek jednej nocy, zdanie o trwaniu
  PRZYPADEK("testGranice", !warunekPrzerywnika("G08", fakty({R(dodajDni(d, -1), 35.0f, 10), R(d, 2.0f, 10)}, d)));
  // zaokrąglenie: `AHI w celu.` nad faktem `5,0 AHI`
  {
    std::vector<Noc> h = {R(d, 4.96f, 10)};
    DaneDnia dd;
    dd.dzis = d;
    dd.hist = &h;
    std::vector<Fakt> fk = faktyPrzerywnika(dd);
    TRZYMA(fk.size() == 2);
    PRZYPADEK("testGranice", !(warunekPrzerywnika("G05", fakty(h, d)) && fk[1].w == "5,0"));
  }
  // G06 `Maska została na twarzy do rana`: 7 h od 21:00 kończy się o 4:00, trzy zdjęcia maski (M-17) bez znaczenia
  {
    Noc x = R(d, 1.0f, 10, 420);
    x.m17 = 2;
    x.m18 = 540;  // start 21:00
    // 7 h zostaje (bez sprawdzania końca sesji); karta warunkowa wymaga poprawy ponad medianę 7 poprzednich nocy
    std::vector<Noc> h;
    for (int i = 1; i <= 6; i++) h.push_back(R(dodajDni(d, -i), 1.0f, 10, 330));
    h.push_back(x);
    TRZYMA(warunekPrzerywnika("G06", fakty(h, d)));
    TRZYMA(!warunekPrzerywnika("G06", fakty({R(dodajDni(d, -1), 1.0f, 10, 480), x}, d)));  // brak historii: neutralna
  }
  // ostatnia niedziela marca: zegar zabiera godzinę, neutralna G01 mówi `zyskaliśmy godzinę`
  // G01 wraca na marzec z własnym neutralnym tekstem; 28.03.2027 (Wielkanoc) wygrywa G01 (niższe ID)
  TRZYMA(okazjaDnia("2027-03-28") == "G01" && okazjaDnia("2028-03-26") == "G01" && okazjaDnia("2028-04-16") == "G06");
  TRZYMA(tekstPrzerywnika("G01", false, "2027-03-28").tytul == "Dzisiaj śpisz godzinę krócej." && tekstPrzerywnika("G01", false, "2027-03-28").powod.empty());
  TRZYMA(tekstPrzerywnika("G01", false, "2026-10-25").tytul == "Czas zmienił się w nocy." && tekstPrzerywnika("G01", false).tytul == "Czas zmienił się w nocy.");
  TRZYMA(tekstPrzerywnika("G01", true, "2027-03-28").tytul == "Zegar przestawił się sam." && tekstPrzerywnika("G01", true, "2026-10-25").tytul == "Zegar przestawił się sam.");
  TRZYMA(okazjaDnia("2026-10-25") == "G01");
}

// ---------- 2. brak danych: nigdy „dobra” karta ----------
static void testBrakDanych() {
  const std::string d = "2026-11-12";
  const std::vector<std::string> DOBRE = {"G02", "G03", "G04", "G05", "G08", "G13", "G15", "G18"};
  auto bezDobrych = [&](const NocPrzerywnika& n) {
    for (auto& g : DOBRE)
      if (warunekPrzerywnika(g, n)) return false;
    return true;
  };
  std::vector<Noc> tyg;
  for (int i = 1; i <= 6; i++) tyg.push_back(R(dodajDni(d, -i), 1.0f, 10));
  auto zDzis = [&](Noc x) { std::vector<Noc> h = tyg; h.push_back(x); return fakty(h, d); };
  Noc x = R(d, 0.5f, 10);
  x.czesciowa = true;
  TRZYMA(bezDobrych(zDzis(x)));  // częściowa
  x = R(d, 0.5f, 10, 480, 45.0f);
  TRZYMA(bezDobrych(zDzis(x)));  // niepewna (M-08)
  x = R(d, 0.5f, 10);
  x.wmed = 31.0f;
  TRZYMA(bezDobrych(zDzis(x)));  // niepewna (mediana wycieku)
  x = R(d, NAN, 10);
  TRZYMA(bezDobrych(zDzis(x)));  // AHI brak
  x = R(d, 0.5f, 10, 480);
  x.wazna = false;
  x.tryb = "VAuto";
  TRZYMA(bezDobrych(zDzis(x)));  // tryb nieznany: nieważna (airbridge.cpp, R-DANE-29)
  x = R(d, 0.5f, 10, 190);
  TRZYMA(bezDobrych(zDzis(x)));  // poniżej 4 h
  TRZYMA(bezDobrych(fakty(tyg, d)));  // brak rekordu z dziś
  x = R(d, NAN, NAN);  // import karty bez metryk
  x.zrodlo = 'K'; x.m08 = NAN; x.wmed = NAN; x.tryb = "";
  TRZYMA(bezDobrych(zDzis(x)));
  x = R(d, 1.0f, NAN);  // wyciek brak: bez „dobrej terapii”
  x.m08 = NAN; x.wmed = NAN;
  TRZYMA(!warunekPrzerywnika("G18", zDzis(x)) && !warunekPrzerywnika("G15", zDzis(x)));
  // fakty pod tekstem: karta z AHI nigdy nad `AHI niepewne`
  {
    std::vector<Noc> h = {R(d, 0.5f, 10, 480, 45.0f)};
    DaneDnia dd;
    dd.dzis = d;
    dd.hist = &h;
    std::vector<Fakt> fk = faktyPrzerywnika(dd);
    TRZYMA(fk.size() == 2 && fk[1].p == "AHI niepewne" && fk[1].w == "–");
    TRZYMA(!warunekPrzerywnika("G05", fakty(h, d)));
  }
  // przez dyrygenta z prawdziwym silnikiem: tryb nieznany i niepewna noc, 25.12 (G05)
  const std::string s25 = "2026-12-25";
  {
    Sym s;
    s.historia(s25);
    s.noce[s25].tryb = "VAuto";
    s.noce[s25].wazna = false;
    Wyb w = s.wybudz(s25, 7);
    TRZYMA(!warunkowa(w, "G05") && w.linia.find("R-DANE-29") != std::string::npos);
  }
  {
    Sym s;
    s.historia(s25);
    s.noce[s25] = R(s25, 0.5f, 10, 480, 45.0f);
    Wyb w = s.wybudz(s25, 7);
    TRZYMA(!karta(w) && w.linia.find("E-20") != std::string::npos);
  }
  {
    Sym s;
    s.historia(s25);
    s.noce[s25].ahi = NAN;
    TRZYMA(neutralna(s.wybudz(s25, 7), "G05"));
  }
}

// ---------- 3. tryby ----------
static void testTryby() {
  const std::string d = "2026-12-25";
  for (const char* t : {"CPAP", "AutoSet", "ASV", "ASVAuto"}) {
    Sym s;
    s.historia(d, 80, t);
    TRZYMA(warunkowa(s.wybudz(d, 7), "G05"));
  }
}

// ---------- 4. noc a dzień: ten sam ekran po restarcie i w nocy, noc późno ----------
static bool roznicaTylkoGodzina(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  if (a.size() != b.size()) return false;
  for (size_t y = 0; y < 480; y++)
    for (size_t x = 0; x < 100; x++)
      if (a[y * 100 + x] != b[y * 100 + x] && !(y < 40 && x >= 80)) return false;  // `dane GG:MM` w prawym górnym rogu
  return true;
}
static void testPrzerysowanie() {
  const std::string d = "2026-12-25";
  {
    Sym s;
    s.historia(d);
    TRZYMA(warunkowa(s.wybudz(d, 7), "G05"));
    std::vector<uint8_t> rano = s.p.ekran.klatki.back();
    TRZYMA(warunkowa(s.wybudz(d, 12), "G05"));
    TRZYMA(roznicaTylkoGodzina(rano, s.p.ekran.klatki.back()));
    s.pierwszy = true;  // restart
    TRZYMA(warunkowa(s.wybudz(d, 13), "G05"));
    TRZYMA(roznicaTylkoGodzina(rano, s.p.ekran.klatki.back()));
    s.wybudz("2026-12-26", 2);  // slot nocny: wpis z dziennika, wariant z faktów nocy dnia ekranu
    TRZYMA(roznicaTylkoGodzina(rano, s.p.ekran.klatki.back()));
  }
  {  // S10 → noc dochodzi w oknie porannym: karta dotyczy tej nocy
    Sym s;
    s.historia(dodajDni(d, -1));
    TRZYMA(s.wybudz(d, 7).ekran.find("S10") != std::string::npos);
    s.noce[d] = R(d, 1.4f, 10);
    TRZYMA(warunkowa(s.wybudz(d, 9), "G05"));
    s.noce[d] = R(d, 1.4f, 10);
    TRZYMA(warunkowa(s.wybudz(d, 12), "G05"));
  }
  {  // noc dochodzi po 11:00: bez przerywnika (R-HUM-14)
    Sym s;
    s.historia(dodajDni(d, -1));
    s.wybudz(d, 7);
    s.noce[d] = R(d, 1.4f, 10);
    TRZYMA(!karta(s.wybudz(d, 12)));
  }
}

// ---------- 5. pierwszeństwo ----------
static void testPierwszenstwo() {
  const std::string ny = "2027-01-01";  // G04 `aparat nadal skutecznie studzi ich ambicje`: warunek `Dobra terapia`
  const std::string d = "2026-12-25";
  // prawdziwy silnik: każdy ekran problemu i linia przed kartą
  auto z = [&](Noc x) { Sym s; s.historia(d); s.noce[d] = x; return s.wybudz(d, 7); };
  TRZYMA(!karta(z(R(d, 1.4f, 24.1f))));  // S-K-10
  TRZYMA(!karta(z(R(d, 1.4f, 27.0f))));  // E-23
  TRZYMA(!karta(z(R(d, 1.4f, 9.6f, 239))));  // linia E-11
  TRZYMA(!karta(z(R(d, 40.0f, 9.6f))));  // poza normą: zaproszenie (linia)
  {  // E-30, potem „wiem” z linią `Wciąż`
    Sym s;
    s.historia(ny);
    for (int i = 1; i <= 7; i++) { std::string x = dodajDni(ny, -i); s.noce[x] = R(x, 8, 9.6f); }
    s.noce[ny] = R(ny, 2, 9.6f);
    TRZYMA(!karta(s.wybudz(ny, 7)));
  }
  // atrapa silnika: wzorzec z pełnym ekranem przed kartą, wyciszony B bez karty (R-HUM-26)
  atrapaWl();
  for (const char* kl : {"L-11", "L-03/10", "L-08/szybki", "L-06/b", "L-09"}) {
    KLUCZE = {kl};
    Sym s;
    s.historia(ny);
    TRZYMA(!karta(s.wybudz(ny, 7)));
  }
  {
    KLUCZE = {"L-11"};
    Sym s;
    s.historia(ny);
    omowione(s, "L-11", ny, reguly::Pilnosc::B);
    Wyb w = s.wybudz(ny, 7);
    printf("  wyciszony L-11: %s | %s\n", w.ekran.c_str(), w.linia.c_str());
    TRZYMA(!karta(w));
  }
  {  // wyciszony wzorzec A: bramka patrzy tylko na B; dziś A daje tylko zablokowany L-06, więc na panelu nie wystąpi
    KLUCZE = {"L-06/b"};
    Sym s;
    s.historia(ny);
    omowione(s, "L-06/b", ny, reguly::Pilnosc::A);
    Wyb w = s.wybudz(ny, 7);
    printf("  wyciszony L-06/b: %s | %s\n", w.ekran.c_str(), w.linia.c_str());
    PRZYPADEK("testPierwszenstwo", !karta(w));
  }
  // 5.7.6 `wzorzec lekarski (wzorce L, także z pilnością B) … przed każdą kartą`, R-EKR-32 (5.4.13 pkt 1): aktywny
  // wzorzec C i reguły „tylko raport”: kh.wzorzecB patrzy tylko na pilność B, więc G04 mówi „skutecznie” przy otwartej sprawie
  for (const char* kl : {"L-01", "L-02/a", "L-04", "L-07", "L-08"}) {
    KLUCZE = {kl};
    Sym s;
    s.historia(ny);
    Wyb w = s.wybudz(ny, 7);
    printf("  aktywny %s: %s\n", kl, w.ekran.c_str());
    PRZYPADEK("testPierwszenstwo", !warunkowa(w, "G04"));
  }
  atrapaWyl();
  // 5.4.13 pkt 6: PS poza pasmem normy w K2, panel nie mówi „w normie”, karta mówi
  {
    Sym s;
    s.historia(ny);
    s.noce[ny].psmed = 9.0f;
    Wyb w = s.wybudz(ny, 7);
    printf("  PS poza pasmem: %s\n", w.ekran.c_str());
    PRZYPADEK("testPierwszenstwo", !warunkowa(w, "G04"));
  }
}

// ---------- 6. dobra i zła wiadomość przy spokojnym dniu ----------
static const float SZEROKA[] = {1.5f, 2.5f, 13.0f, 4.0f, 12.0f, 3.0f, 2.0f};  // mediana 7 nocy poniżej 5, norma szeroka
static Sym szeroka(const std::string& d, float ahiDzis, int odchyl) {
  Sym s;
  s.historia(d);
  for (int i = 1; i <= 80; i++) { std::string x = dodajDni(d, -i); s.noce[x] = R(x, SZEROKA[(i + odchyl) % 7], 9.6f); }
  s.noce[d] = R(d, ahiDzis, 9.6f);
  return s;
}
static void testSpokojnyDzien() {
  // G03 `Tydzień zamknięty, terapia pod kontrolą.` nad faktem `12,0 AHI` (piątek 26.02.2027, prawdziwy silnik):
  // warunek to mediana z 7 nocy; ostatnia noc AHI ≥ 10 go nie zdejmuje, 3 z 7 nocy złych też nie
  {
    Sym s;
    s.historia("2027-02-26");
    for (int i = 1; i <= 80; i++) { std::string x = dodajDni("2027-02-26", -i); s.noce[x] = R(x, SZEROKA[i % 7], 9.6f); }
    s.noce["2027-02-26"] = R("2027-02-26", 12.0f, 9.6f);
    Wyb w = s.wybudz("2027-02-26", 7);
    printf("  piątek, AHI 12: %s\n", w.ekran.c_str());
    TRZYMA(karta(w));
    PRZYPADEK("testSpokojnyDzien", !warunkowa(w, "G03"));
  }
  {
    const std::string d = "2026-11-13";
    std::vector<Noc> h;
    for (float a : {1.0f, 1.0f, 1.0f, 1.0f, 40.0f, 40.0f}) h.push_back(R(dodajDni(d, -(int)h.size() - 1), a, 10));
    h.push_back(R(d, 30.0f, 10));
    PRZYPADEK("testSpokojnyDzien", !warunekPrzerywnika("G03", fakty(h, d)));
  }
  // sprawdza kontekst bez faktów nocy (`!p.warunkowa` przechodzi zawsze). Z faktami G03 wraca:
  // piątek po E-23 wczoraj (wyciek 38) i tydzień „pod kontrolą”
  {
    const std::string d = "2026-11-13";
    std::vector<Noc> h;
    for (int i = 6; i >= 1; i--) h.push_back(R(dodajDni(d, -i), 1.0f, 10));
    h.back().wp95 = 38.0f;  // wczoraj E-23
    h.push_back(R(d, 1.0f, 10));
    KontekstHumoru k;
    k.dzis = d;
    k.sceny = sceny();
    k.zPropozycjami = true;
    k.noc = fakty(h, d);
    UstawieniaPrzerywnikow u = ustawieniaPrzerywnikowBety();
    u.ogolne = {"G13"};
    std::vector<WpisHumoru> log = {{dodajDni(d, -2), "L", "L-11", 'D', "", ""}, {dodajDni(d, -1), "S3", "E-23", 'D', "", ""}};
    PRZYPADEK("testSpokojnyDzien", !warunekPrzerywnika("G03", k.noc));
    TRZYMA(wybierzPrzerywnik(EkranDnia{"S2A", "1", false, false}, log, k, u).jest);
  }
  // G09/G10 po jednej nocy, która mieści się w normie osobistej: panel bez karty mówi `Tydzień jak zwykle.`,
  // bez linii i bez ekranu, karta mówi `AHI powyżej celu… liczyłem na lepszy raport`
  {
    bool g09 = false, g10 = false, g16 = false;
    for (int k = 0; k < 60 && !(g09 && g10 && g16); k++) {
      std::string d = dodajDni("2027-02-01", k);
      Sym s = szeroka(d, 12.0f, k);
      Wyb w = s.wybudz(d, 7);
      g09 = g09 || warunkowa(w, "G09");
      g10 = g10 || warunkowa(w, "G10");
      g16 = g16 || warunkowa(w, "G16");
    }
    TRZYMA(!g16);  // G16 `To była dobra noc` przy AHI 12 przez dyrygenta (neutralna)
    TRZYMA(g09 && g10);  // zostają po jednej nocy (tekst i próg bezwzględny autora), zamknięty
    const std::string d = "2026-12-25";
    Sym s = szeroka(d, 12.0f, 0);
    std::vector<Noc> h;
    for (auto& x : s.noce) h.push_back(x.second);
    DaneDnia dd;
    dd.dzis = d;
    dd.hist = &h;
    Stany st = liczStany(h, d, {});
    dd.stany = &st;
    TrescEkranu t = trescDnia("S2A", "1", dd);
    printf("  S2A tej nocy: `%s` `%s`\n", t.tytul.c_str(), t.powod.c_str());
  }
}

// ---------- 7. cudze testy i teksty: sedno (bez tytułu `Spokojny tydzień`) ----------
static void testTeksty() {
  for (int g = 1; g <= 18; g++)
    for (bool war : {true, false}) {
      TekstPrzerywnika t = tekstPrzerywnika(sc(g), war);
      TRZYMA(!t.tytul.empty());
      TRZYMA((t.tytul + t.powod).find("Spokojny tydzień") == std::string::npos);
    }
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testGranice();
  testBrakDanych();
  testTryby();
  testPrzerysowanie();
  testPierwszenstwo();
  testSpokojnyDzien();
  testTeksty();
  printf("testy_przerywniki_s2d_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
