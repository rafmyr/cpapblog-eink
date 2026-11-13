// SPDX-License-Identifier: GPL-3.0-or-later
// Testy częstszego sprawdzania AirBridge rano na zasilaniu zewnętrznym (decyzja właściciela projektu 13.11.2026).
// Pisane z wymagań R-DANE-63 (okno poranne co 5 min na zasilaniu), R-DANE-64 (gęste okno co 2 min na zasilaniu),
// R-DANE-65 (stan zasilania raz na wybudzenie; nieznany = jak bateria) i z zachowanych R-DANE-19, R-DANE-61
// (bateria bez zmian; bez zagęszczenia po braku odpowiedzi AirBridge, decyzja z 10.11.2026).
//
// (a) Plan wybudzenia (src/plan.h): terminy poranka na baterii (regresja liczbowa) i na zasilaniu, wyłączenia.
// (b) Dyrygent z atrapami: bateria, zasilanie, stan nieznany, odłączenie i podłączenie kabla w trakcie okna,
//     AirBridge bez odpowiedzi, noc kompletna, e-ink przy zmiennej temperaturze.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_zasilanie_rytm). LOG_PORANKA=1 drukuje przebiegi z (b).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "noc.h"
#include "plan.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

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

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";
static bool logPoranka() { return getenv("LOG_PORANKA") != nullptr; }

static int sd(int h, int m, int s = 0) { return h * 3600 + m * 60 + s; }
static WejsciePlanu wp(int h, int m, int s = 5) {
  WejsciePlanu w;
  w.zegar = true;
  w.sekundaDoby = sd(h, m, s);
  return w;
}
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
static int minutaLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  return l.tm_hour * 60 + l.tm_min;
}

// Noc z poranka `data` z terapią od `startOd12` (minuty od 12:00 wieczoru, M-18) przez `minut` (M-01).
static Noc noc(const std::string& data, int startOd12, int minut) {
  Noc n;
  n.data = data;
  n.wazna = true;
  n.m01 = minut;
  n.m18 = startOd12;
  n.ahi = 1.5f;
  n.tryb = "ASVAuto";
  return n;
}
// Historia `ile` nocy przed 4.11.2026 (poranki 3.11, 2.11, …) z końcem terapii o `koniec` (minuta doby):
// start 23:00 (M-18 = 660), M-01 = koniec + 24 h − 23:00.
static std::vector<Noc> historia(int ile, int koniec = 7 * 60) {
  std::vector<Noc> h;
  for (int i = ile; i >= 1; i--) h.push_back(noc(dodajDni("2026-11-04", -i), 660, koniec + 24 * 60 - 23 * 60));
  return h;
}

// Oczekiwane minuty zapytań w [05:00, 11:00) przy nocy niekompletnej cały poranek i zwykłej porze końca terapii
// 07:00 (gęste okno 06:15…07:45). Liczone tutaj wprost z treści wymagań, niezależnie od src/plan.cpp.
// Bateria (R-DANE-19, R-DANE-61): co 30 min, w gęstym oknie co 5 min: 28 zapytań.
static std::vector<int> minutyBaterii() {
  std::vector<int> v;
  for (int m = 300; m < 660; m++) {
    bool geste = m >= 375 && m <= 465;
    if (m % 30 == 0 || (geste && m % 5 == 0)) v.push_back(m);
  }
  return v;
}
// Zasilanie (R-DANE-63, R-DANE-64): terminy baterii, a do tego co 5 min poza gęstym oknem i co 2 min w nim: 108.
static std::vector<int> minutyZasilania() {
  std::vector<int> v;
  for (int m = 300; m < 660; m++) {
    bool geste = m >= 375 && m <= 465;
    if (m % 5 == 0 || (geste && m % 2 == 0)) v.push_back(m);
  }
  return v;
}

// ============================================================================================================
// (a) Plan wybudzenia
// ============================================================================================================

struct TerminP {
  int minuta;
  Powody powody;
  bool operator==(const TerminP& o) const { return minuta == o.minuta && powody == o.powody; }
};
// Terminy sieciowe doby planem od 00:00:05 (stan `w` się nie zmienia: noc nie przychodzi), z powodami.
static std::vector<TerminP> terminyDoby(WejsciePlanu w) {
  std::vector<TerminP> v;
  long teraz = 5;
  for (int i = 0; i < 2000 && teraz < 86400 - 60; i++) {
    w.sekundaDoby = (int)teraz;
    Termin t = nastepnyTermin(w);
    w.obsluzoneZnane = true;
    w.obsluzoneDo = (int)teraz;
    teraz += t.sekund;
    if (teraz >= 86400) break;
    w.sekundaDoby = (int)teraz;
    Powody p = powodyTeraz(w);
    if (p & POWOD_SPRAWDZENIE_11) w.sprawdzenie11 = true;
    if (sieciowy(p)) v.push_back({(int)(teraz / 60), p});
  }
  return v;
}
static std::vector<int> minutyRano(const std::vector<TerminP>& v) {
  std::vector<int> m;
  for (auto& t : v)
    if (t.minuta >= 300 && t.minuta < 660) m.push_back(t.minuta);
  return m;
}
static Powody powodyW(const std::vector<TerminP>& v, int minuta) {
  for (auto& t : v)
    if (t.minuta == minuta) return t.powody;
  return 0;
}
static WejsciePlanu stan(bool naZasilaniu, int zwyklyKoniec = 7 * 60) {
  WejsciePlanu w = wp(0, 0);
  w.zwyklyKoniec = zwyklyKoniec;
  w.naZasilaniu = naZasilaniu;
  return w;
}

// Stałe z decyzji 13.11.2026 i ich relacja do rytmu baterii.
static void testStale() {
  ROWNE_L(PLAN_ZASILANIE_CO_MIN, 5);
  ROWNE_L(PLAN_ZASILANIE_GESTE_CO_MIN, 2);
  ROWNE_L(PLAN_CO_ILE_MIN, 30);   // bateria bez zmian
  ROWNE_L(PLAN_GESTE_CO_MIN, 5);  // bateria bez zmian
  ROWNE_L(minutyBaterii().size(), 28);
  ROWNE_L(minutyZasilania().size(), 108);
}

// Bateria: terminy poranka liczbowo jak przed 13.11.2026 (R-DANE-19, R-DANE-61), bez powodu `okno_zasilanie`.
static void testBateriaBezZmian() {
  std::vector<TerminP> v = terminyDoby(stan(false));
  SPRAWDZ(minutyRano(v) == minutyBaterii());
  for (auto& t : v) SPRAWDZ(!(t.powody & POWOD_OKNO_ZASILANIE));
  ROWNE_L(powodyW(v, 390), POWOD_OKNO_PORANNE);  // 06:30 (05:30 ma też zapis czujników)
  ROWNE_L(powodyW(v, 375), POWOD_OKNO_GESTE);
  ROWNE_L(powodyW(v, 305), 0);
  ROWNE_L(powodyW(v, 376), 0);
  // bez gęstego okna (mniej niż 3 noce): co 30 min
  std::vector<int> co30;
  for (int m = 300; m < 660; m += 30) co30.push_back(m);
  SPRAWDZ(minutyRano(terminyDoby(stan(false, -1))) == co30);
}

// Zasilanie: R-DANE-63 (co 5 min w oknie porannym) i R-DANE-64 (co 2 min w gęstym oknie); terminy baterii zostają
// z tymi samymi powodami, nowe mają powód `okno_zasilanie`.
static void testZasilanieTerminy() {
  std::vector<TerminP> z = terminyDoby(stan(true)), b = terminyDoby(stan(false));
  SPRAWDZ(minutyRano(z) == minutyZasilania());
  ROWNE_L(powodyW(z, 305), POWOD_OKNO_ZASILANIE);   // poza gęstym oknem, co 5 min
  ROWNE_L(powodyW(z, 390), POWOD_OKNO_PORANNE);     // 06:30: siatka 30 min bez zmian
  ROWNE_L(powodyW(z, 370), POWOD_OKNO_ZASILANIE);   // ostatnia minuta przed gęstym oknem
  ROWNE_L(powodyW(z, 375), POWOD_OKNO_GESTE);       // termin baterii w gęstym oknie bez zmian
  ROWNE_L(powodyW(z, 376), POWOD_OKNO_ZASILANIE);   // gęste okno, co 2 min
  ROWNE_L(powodyW(z, 377), 0);
  ROWNE_L(powodyW(z, 380), POWOD_OKNO_GESTE);
  ROWNE_L(powodyW(z, 464), POWOD_OKNO_ZASILANIE);
  ROWNE_L(powodyW(z, 466), 0);                      // po gęstym oknie znów co 5 min
  ROWNE_L(powodyW(z, 470), POWOD_OKNO_ZASILANIE);
  ROWNE_L(powodyW(z, 655), POWOD_OKNO_ZASILANIE);
  SPRAWDZ(powodyW(z, 300) == powodyW(b, 300));      // 05:00 i 11:00 jak na baterii
  SPRAWDZ(powodyW(z, 660) == powodyW(b, 660));
  // największy odstęp między zapytaniami: 2 min w gęstym oknie, 5 min poza nim
  std::vector<int> m = minutyRano(z);
  for (size_t i = 1; i < m.size(); i++) {
    bool geste = m[i - 1] >= 375 && m[i] <= 465;
    SPRAWDZ(m[i] - m[i - 1] <= (geste ? 2 : 5));
  }
  // każdy termin baterii jest też na zasilaniu, z tym samym powodem (cała doba)
  for (auto& t : b) SPRAWDZ(powodyW(z, t.minuta) == t.powody);
  // poza oknem porannym (05:00, 11:00) nic nowego
  int poza = 0;
  for (auto& t : z) poza += (t.minuta <= 300 || t.minuta >= 660) && powodyW(b, t.minuta) != t.powody;
  ROWNE_L(poza, 0);
  // bez gęstego okna (mniej niż 3 noce): cały poranek co 5 min
  std::vector<int> co5;
  for (int k = 300; k < 660; k += 5) co5.push_back(k);
  SPRAWDZ(minutyRano(terminyDoby(stan(true, -1))) == co5);
  // gęste okno przycięte do okna porannego (zwykła pora 05:20 → 05:01…06:05)
  std::vector<TerminP> wcz = terminyDoby(stan(true, 5 * 60 + 20));
  ROWNE_L(powodyW(wcz, 302), POWOD_OKNO_ZASILANIE);
  ROWNE_L(powodyW(wcz, 301), 0);
  ROWNE_L(powodyW(wcz, 366), 0);
  ROWNE_L(powodyW(wcz, 370), POWOD_OKNO_ZASILANIE);
}

// Wyłączenia na zasilaniu: noc kompletna, E-01 (brak odpowiedzi AirBridge albo Wi-Fi, także odrzucone logowanie;
// decyzja z 10.11.2026 zachowana), brak czasu ważnego. Plan wtedy dokładnie jak na baterii.
static void testZasilanieWylaczenia() {
  for (int wariant = 0; wariant < 4; wariant++) {
    WejsciePlanu z = stan(true), b = stan(false);
    for (WejsciePlanu* w : {&z, &b}) {
      if (wariant == 0) w->nocKompletna = true;
      if (wariant == 1) w->e01 = true;
      if (wariant == 2) w->e01 = w->logowanieOdrzucone = true;
      if (wariant == 3) { w->e01 = true; w->nocyBezDanych = 2; }
    }
    std::vector<TerminP> tz = terminyDoby(z), tb = terminyDoby(b);
    SPRAWDZ(tz == tb);
    for (auto& t : tz) SPRAWDZ(!(t.powody & POWOD_OKNO_ZASILANIE));
  }
  WejsciePlanu bezCzasu = stan(true);
  bezCzasu.zegar = false;
  ROWNE_L(nastepnyTermin(bezCzasu).powody, POWOD_CZAS);
  ROWNE_L(nastepnyTermin(bezCzasu).sekund, OKRES_SNU_S);
}

// Własności powodu `okno_zasilanie` i powtórka R-DANE-62 obok niego.
static void testPowod() {
  SPRAWDZ(sieciowy(POWOD_OKNO_ZASILANIE));
  SPRAWDZ(!tylkoStatus(POWOD_OKNO_ZASILANIE));
  SPRAWDZ(!przerysowanieDnia(POWOD_OKNO_ZASILANIE));
  SPRAWDZ(pobranieZegarem(POWOD_OKNO_ZASILANIE).pobierz);
  SPRAWDZ(!wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, POWOD_OKNO_ZASILANIE));
  SPRAWDZ(nazwyPowodow(POWOD_OKNO_ZASILANIE) == "okno_zasilanie");
  SPRAWDZ(powodWpisuBaterii(PowodWybudzenia::ZEGAR, POWOD_OKNO_ZASILANIE | POWOD_CZUJNIKI) ==
          "zegar:czujniki+okno_zasilanie");
  SPRAWDZ(tylkoRytmZasilania(POWOD_OKNO_ZASILANIE));
  SPRAWDZ(tylkoRytmZasilania(POWOD_OKNO_ZASILANIE | POWOD_CZUJNIKI));
  SPRAWDZ(!tylkoRytmZasilania(POWOD_OKNO_ZASILANIE | POWOD_OKNO_PORANNE));
  SPRAWDZ(!tylkoRytmZasilania(POWOD_OKNO_GESTE));
  SPRAWDZ(!tylkoRytmZasilania(0));
  // następny termin i czas snu: 08:00:05 na zasilaniu → 08:05 (300 s − 5 s + zapas), na baterii 08:30
  WejsciePlanu z = stan(true), b = stan(false);
  z.sekundaDoby = b.sekundaDoby = sd(8, 0, 5);
  z.obsluzoneZnane = b.obsluzoneZnane = true;
  z.obsluzoneDo = b.obsluzoneDo = sd(8, 0, 5);
  ROWNE_L(nastepnyTermin(z).sekund, 5 * 60 - 5 + ZAPAS_POLGODZINY_S);
  ROWNE_L(nastepnyTermin(z).powody, POWOD_OKNO_ZASILANIE);
  ROWNE_L(nastepnyTermin(b).sekund, 30 * 60 - 5 + ZAPAS_POLGODZINY_S);
  // gęste okno 07:10:05 → 07:12 na zasilaniu, 07:15 na baterii
  z.sekundaDoby = b.sekundaDoby = z.obsluzoneDo = b.obsluzoneDo = sd(7, 10, 5);
  ROWNE_L(nastepnyTermin(z).sekund, 2 * 60 - 5 + ZAPAS_POLGODZINY_S);
  ROWNE_L(nastepnyTermin(b).sekund, 5 * 60 - 5 + ZAPAS_POLGODZINY_S);
  // powtórka R-DANE-62 (status „przetwarza” o 08:03:10): termin 08:05:10 leży po 08:05 z rytmu zasilania, więc
  // wybudzenie o 08:05 nie zużywa licznika powtórek; na baterii powtórka jak dotąd
  z.sekundaDoby = b.sekundaDoby = sd(8, 3, 20);
  z.obsluzoneDo = b.obsluzoneDo = sd(8, 3, 5);
  z.przetwarzaOd = b.przetwarzaOd = sd(8, 3, 10);
  ROWNE_L(nastepnyTermin(z).powody, POWOD_OKNO_ZASILANIE);
  ROWNE_L(nastepnyTermin(b).powody, POWOD_POWTORKA);
  z.sekundaDoby = sd(8, 5, 12);  // obie chwile minęły w tym samym przedziale
  SPRAWDZ(!(powodyTeraz(z) & POWOD_POWTORKA));
  SPRAWDZ(powodyTeraz(z) & POWOD_OKNO_ZASILANIE);
}

// ============================================================================================================
// (b) Dyrygent z atrapami
// ============================================================================================================

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_TERAPIA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* STATUS_PRZETWARZA =
    "{\"version\":\"v2.2.1\",\"mode\":\"ASVAuto\",\"rop\":0,\"edf\":\"post-processing\",\"edf_prefix\":\"\","
    "\"report_state\":\"idle\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const char* POLA_V3 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb;m18";
static const long DOBA = nrDnia("2026-11-03");  // noc z poranka 4.11.2026

static std::string raport(long day) {
  std::string d = std::to_string(day);
  return "{\"day\":" + d + ",\"present\":true,\"partial\":false,\"used_seconds\":28920,\"ahi\":1.4,"
         "\"events_known\":true,\"session_count\":1,\"counts\":{\"oa\":1,\"ca\":5,\"h\":4},\"channels\":[{\"id\":"
         "\"Press\",\"unit\":\"cmH2O\",\"median\":9.8,\"p95\":13.6,\"source\":\"edf\"},{\"id\":\"Leak\",\"unit\":"
         "\"L/min\",\"median\":2.4,\"p95\":9.6,\"source\":\"edf\"}],\"settings\":{\"available\":true,\"fields\":"
         "[{\"tag\":\"MOP\",\"value\":\"ASVAuto\",\"option\":8}]}}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}

struct PanelP {
  Panel p;
  explicit PanelP(int nocyHistorii, int koniec = 7 * 60) {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    std::string w;
    for (auto& n : historia(nocyHistorii, koniec)) w += wierszNocyV3(n) + "\n";
    p.pliki.pliki["/dane/noce/2026.csv"] = std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n" + w + "#koniec " +
                                           std::to_string(nocyHistorii) + "\n";
    p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";  // bez trybu porównawczego (drugie połączenie)
  }
  bool maNoc() const {
    auto it = p.pliki.pliki.find("/dane/noce/2026.csv");
    return it != p.pliki.pliki.end() && it->second.find("\n2026-11-04;") != std::string::npos;
  }
};

// Stan AirBridge w chwili t: terapia do `terapiaDo`, przetwarzanie do `przetwarzaDo`, potem noc na liście.
struct ScenariuszAB {
  uint32_t terapiaDo = 0, przetwarzaDo = 0;
};
static void odpowiedzi(PanelP& s, const ScenariuszAB& sc, uint32_t t) {
  s.p.siec.odpowiedzi.clear();
  if (t < sc.terapiaDo) {
    s.p.siec.dodaj("/api/status", 200, STATUS_TERAPIA);
  } else if (t < sc.przetwarzaDo) {
    s.p.siec.dodaj("/api/status", 200, STATUS_PRZETWARZA);
  } else {
    s.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.p.siec.dodaj(sciezkaListy(), 200, lista(DOBA, DOBA));
    s.p.siec.dodaj(sciezkaNocy(DOBA), 200, raport(DOBA));
  }
}

// Konfiguracja przebiegu: domyślnie bez źródła otoczenia; testDyrygentEinkOtoczenie podstawia KONF_OTO.
static const KonfiguracjaPanelu KONF_OTO = {"192.168.1.20", "cpap", "haslo", "http://192.168.1.60:8080/otoczenie", "", true};
static const KonfiguracjaPanelu* konfTeraz = &KONF;

struct WybP {
  uint32_t ep = 0;
  int minuta = 0;
  PowodWybudzenia powod = PowodWybudzenia::ZEGAR;
  int polaczen = 0, odswiezen = 0;
  int otoczen = 0;  // odczyty źródła otoczenia (linie logu `otoczenie: HTTP …`)
  bool spal = false, noc = false;
  std::string powody;  // pole `powod` wiersza dziennika wybudzeń (z logu)
};
static WybP wybudz(PanelP& s, uint32_t t, PowodWybudzenia pw) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.p.siec.polaczonaTeraz = false;
  s.p.d.z.clear();
  int snow = s.p.sen.snow;
  Dyrygent(s.p.adaptery(), *konfTeraz).wybudzenie();
  WybP w;
  w.ep = t;
  w.minuta = minutaLok(t);
  w.powod = pw;
  w.spal = s.p.sen.snow == snow + 1;
  w.noc = s.maNoc();
  for (auto& z : s.p.d.z) {
    w.polaczen += z == "siec.polacz";
    w.odswiezen += z == "ekran.odswiez";
    w.otoczen += z.find("otoczenie: HTTP") != std::string::npos;
    size_t a = z.find("wybudzenie ");
    if (w.powody.empty() && a != std::string::npos && z.find("cpap-eink") != std::string::npos) {
      size_t b = z.find(',', a);
      w.powody = z.substr(a + 11, b == std::string::npos ? std::string::npos : b - a - 11);
    }
  }
  return w;
}
// Przebieg zegarem od startu o `od` do `doT`. `przed(t)` ustawia panel przed każdym wybudzeniem (stan zasilania,
// sieć, temperatura).
template <typename F>
static std::vector<WybP> symuluj(PanelP& s, const ScenariuszAB& sc, uint32_t od, uint32_t doT, F przed) {
  std::vector<WybP> v;
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 1500 && t < doT; i++) {
    przed(t);
    odpowiedzi(s, sc, t);
    WybP w = wybudz(s, t, pw);
    v.push_back(w);
    if (!w.spal) break;
    uint32_t snu = s.p.sen.plan.budzZegarem ? s.p.sen.plan.sekund : OKRES_SNU_S;
    t += s.p.zegar.msTeraz / 1000 + (snu ? snu : 1);
    pw = PowodWybudzenia::ZEGAR;
  }
  if (logPoranka())
    for (auto& w : v)
      printf("  %s %-9s Wi-Fi %d, e-ink %d, noc %d, %s\n", hhmm(w.minuta).c_str(),
             w.powod == PowodWybudzenia::START ? "start" : "zegar", w.polaczen, w.odswiezen, (int)w.noc,
             w.powody.c_str());
  return v;
}
static std::vector<WybP> symuluj(PanelP& s, const ScenariuszAB& sc, uint32_t od, uint32_t doT) {
  return symuluj(s, sc, od, doT, [](uint32_t) {});
}
// Minuty wybudzeń zegarem z Wi-Fi w [odMin, doMin).
static std::vector<int> zWifi(const std::vector<WybP>& v, int odMin = 300, int doMin = 660) {
  std::vector<int> m;
  for (auto& w : v)
    if (w.powod == PowodWybudzenia::ZEGAR && w.polaczen > 0 && w.minuta >= odMin && w.minuta < doMin)
      m.push_back(w.minuta);
  return m;
}
static int zPowodem(const std::vector<WybP>& v, const std::string& nazwa) {
  int n = 0;
  for (auto& w : v) n += w.powody.find(nazwa) != std::string::npos;
  return n;
}
static uint32_t chwilaNocy(const std::vector<WybP>& v) {
  for (auto& w : v)
    if (w.noc) return w.ep;
  return 0;
}
static void drukuj(const char* co, const std::vector<int>& m) {
  printf("    %s:", co);
  for (int x : m) printf(" %s", hhmm(x).c_str());
  printf("\n");
}
// Granice przebiegów jako funkcje: liczone po ustawieniu strefy panelu w main() (stała globalna policzyłaby się
// wcześniej, w strefie komputera, i na maszynie w UTC przebieg zaczynałby się o 05:00 zamiast o 04:00).
static uint32_t start4() { return ep(2026, 11, 4, 4, 0); }
static uint32_t poludnie() { return ep(2026, 11, 4, 12, 0); }
// Terapia trwa do 12:00: noc nie przychodzi cały poranek, AirBridge odpowiada (status „terapia”).
static ScenariuszAB terapiaDoPoludnia() { return ScenariuszAB{poludnie(), poludnie()}; }

// Bateria i stan nieznany: rytm jak przed 13.11.2026 (28 zapytań w oknie porannym), bez powodu `okno_zasilanie`.
static void testDyrygentBateria() {
  for (Zasilanie z : {Zasilanie::BATERIA, Zasilanie::NIEZNANE}) {
    PanelP s(5);
    s.p.czujniki.zas = z;
    std::vector<WybP> v = symuluj(s, terapiaDoPoludnia(), start4(), poludnie());
    SPRAWDZ(zWifi(v) == minutyBaterii());
    if (zWifi(v) != minutyBaterii()) drukuj("bateria", zWifi(v));
    ROWNE_L(zWifi(v).size(), 28);
    ROWNE_L(zPowodem(v, "okno_zasilanie"), 0);
    ROWNE_L(zPowodem(v, "okno_geste"), 16);  // 06:15…07:45 co 5 min bez 06:30, 07:00, 07:30
    // stan zasilania czytany raz na każde wybudzenie (R-DANE-65)
    ROWNE_L(s.p.czujniki.odczytowZasilania, (long long)v.size());
  }
}

// Zasilanie: co 5 min poza gęstym oknem, co 2 min w gęstym (108 zapytań w oknie porannym), terminy baterii zostają.
static void testDyrygentZasilanie() {
  PanelP s(5);
  s.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
  std::vector<WybP> v = symuluj(s, terapiaDoPoludnia(), start4(), poludnie());
  SPRAWDZ(zWifi(v) == minutyZasilania());
  if (zWifi(v) != minutyZasilania()) drukuj("zasilanie", zWifi(v));
  ROWNE_L(zWifi(v).size(), 108);
  ROWNE_L(zPowodem(v, "okno_zasilanie"), 108 - 28);
  ROWNE_L(zPowodem(v, "okno_geste"), 16);
  // przy trwającej terapii każde wybudzenie rytmu zasilania tylko z jednym połączeniem (sam `/api/status`)
  for (auto& w : v)
    if (w.powody.find("okno_zasilanie") != std::string::npos) ROWNE_L(w.polaczen, 1);
  // po 11:00 i przed 05:00 jak na baterii
  PanelP b(5);
  b.p.czujniki.zas = Zasilanie::BATERIA;
  std::vector<WybP> vb = symuluj(b, terapiaDoPoludnia(), start4(), poludnie());
  SPRAWDZ(zWifi(v, 0, 300) == zWifi(vb, 0, 300));
  SPRAWDZ(zWifi(v, 660, 1440) == zWifi(vb, 660, 1440));
}

// Zasilanie, noc przychodzi: w gęstym oknie wynik najpóźniej ok. 2 min po końcu terapii, poza nim ok. 5 min;
// po nocy kompletnej koniec odpytywania do 11:00.
static void testDyrygentZasilanieNoc() {
  {
    PanelP s(5);
    s.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
    ScenariuszAB sc{ep(2026, 11, 4, 7, 3), ep(2026, 11, 4, 7, 3)};
    std::vector<WybP> v = symuluj(s, sc, start4(), poludnie());
    uint32_t n = chwilaNocy(v);
    SPRAWDZ(n >= sc.terapiaDo && n - sc.terapiaDo <= 2 * 60 + 30);
    ROWNE_L(minutaLok(n), 7 * 60 + 4);
    int poNocy = 0;
    for (auto& w : v)
      if (w.ep > n && w.minuta < 11 * 60 &&
          (w.powody.find("okno_") != std::string::npos || w.powody.find("powtorka") != std::string::npos))
        poNocy++;
    ROWNE_L(poNocy, 0);
  }
  {
    PanelP s(5);
    s.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
    ScenariuszAB sc{ep(2026, 11, 4, 9, 11), ep(2026, 11, 4, 9, 11)};
    std::vector<WybP> v = symuluj(s, sc, start4(), poludnie());
    uint32_t n = chwilaNocy(v);
    SPRAWDZ(n >= sc.terapiaDo && n - sc.terapiaDo <= 5 * 60 + 30);
    ROWNE_L(minutaLok(n), 9 * 60 + 15);
    // ta sama doba na baterii: noc dopiero o 09:30
    PanelP b(5);
    b.p.czujniki.zas = Zasilanie::BATERIA;
    ROWNE_L(minutaLok(chwilaNocy(symuluj(b, sc, start4(), poludnie()))), 9 * 60 + 30);
  }
}

// Doba typowa (terapia do 07:02, zwykła pora 07:00): ile zapytań do nocy kompletnej na baterii i na zasilaniu
// i kiedy przychodzi noc.
static void testDyrygentDobaTypowa() {
  int zapytan[2] = {0, 0}, minutaNocy[2] = {0, 0}, i = 0;
  for (Zasilanie z : {Zasilanie::BATERIA, Zasilanie::ZEWNETRZNE}) {
    PanelP s(5);
    s.p.czujniki.zas = z;
    ScenariuszAB sc{ep(2026, 11, 4, 7, 2), ep(2026, 11, 4, 7, 2)};
    std::vector<WybP> v = symuluj(s, sc, start4(), poludnie());
    uint32_t n = chwilaNocy(v);
    minutaNocy[i] = minutaLok(n);
    for (auto& w : v)
      if (w.powod == PowodWybudzenia::ZEGAR && w.polaczen > 0 && w.minuta >= 300 && w.ep <= n) zapytan[i]++;
    i++;
  }
  printf("  doba typowa (terapia do 07:02): bateria %d zapytań, noc %s; zasilanie %d zapytań, noc %s\n", zapytan[0],
         hhmm(minutaNocy[0]).c_str(), zapytan[1], hhmm(minutaNocy[1]).c_str());
  ROWNE_L(zapytan[0], 14);  // 05:00, 05:30, 06:00, 06:15…07:05 co 5 min
  ROWNE_L(minutaNocy[0], 7 * 60 + 5);
  ROWNE_L(zapytan[1], 44);  // 05:00…06:10 co 5 min (15), 06:15…07:02 parzyste i podzielne przez 5 (29)
  ROWNE_L(minutaNocy[1], 7 * 60 + 2);
}

// Kabel odłączony w połowie gęstego okna (06:43) i poza nim (08:22): wybudzenie zaplanowane jeszcze na zasilaniu
// (06:44, 08:25) wstaje ze swoim powodem `okno_zasilanie` i sprawdza AirBridge (R-NF-02: bez wybudzeń bez powodu),
// a od następnego wybudzenia rytm baterii.
static void testDyrygentOdlaczenie() {
  for (int przypadek = 0; przypadek < 2; przypadek++) {
    uint32_t koniecZasilania = przypadek == 0 ? ep(2026, 11, 4, 6, 43) : ep(2026, 11, 4, 8, 22);
    int minutaKonca = przypadek == 0 ? 6 * 60 + 43 : 8 * 60 + 22;
    int zaplanowane = przypadek == 0 ? 6 * 60 + 44 : 8 * 60 + 25;
    PanelP s(5);
    std::vector<WybP> v = symuluj(s, terapiaDoPoludnia(), start4(), poludnie(), [&](uint32_t t) {
      s.p.czujniki.zas = t < koniecZasilania ? Zasilanie::ZEWNETRZNE : Zasilanie::BATERIA;
    });
    std::vector<int> m = zWifi(v), oczek;
    for (int x : minutyZasilania())
      if (x < minutaKonca) oczek.push_back(x);
    oczek.push_back(zaplanowane);
    for (int x : minutyBaterii())
      if (x > zaplanowane) oczek.push_back(x);
    SPRAWDZ(m == oczek);
    if (m != oczek) drukuj("po odłączeniu", m);
    int bezPowodu = 0;
    for (auto& w : v) {
      if (w.minuta > zaplanowane) SPRAWDZ(w.powody.find("okno_zasilanie") == std::string::npos);
      if (w.minuta == zaplanowane) SPRAWDZ(w.powody == "zegar:okno_zasilanie");
      bezPowodu += w.powod == PowodWybudzenia::ZEGAR && w.powody == "zegar:";
    }
    ROWNE_L(bezPowodu, 0);
  }
}

// Kabel podłączony w trakcie okna (08:02): panel dowiaduje się przy najbliższym wybudzeniu z rytmu baterii (08:30),
// potem co 5 min.
static void testDyrygentPodlaczenie() {
  PanelP s(5);
  uint32_t od = ep(2026, 11, 4, 8, 2);
  std::vector<WybP> v = symuluj(s, terapiaDoPoludnia(), start4(), poludnie(), [&](uint32_t t) {
    s.p.czujniki.zas = t < od ? Zasilanie::BATERIA : Zasilanie::ZEWNETRZNE;
  });
  std::vector<int> m = zWifi(v), oczek;
  for (int x : minutyBaterii())
    if (x <= 8 * 60 + 30) oczek.push_back(x);
  for (int x : minutyZasilania())
    if (x > 8 * 60 + 30) oczek.push_back(x);
  SPRAWDZ(m == oczek);
  if (m != oczek) drukuj("po podłączeniu", m);
  for (auto& w : v) SPRAWDZ(!(w.powod == PowodWybudzenia::ZEGAR && w.powody == "zegar:"));
}

// AirBridge bez odpowiedzi na zasilaniu (decyzja z 10.11.2026 zachowana): bez zagęszczenia, tylko siatka 30 min;
// po powrocie odpowiedzi (Wi-Fi od 08:10) rytm zasilania wraca od pierwszego udanego odczytu (08:30).
static void testDyrygentBezOdpowiedzi() {
  std::vector<int> co30;
  for (int m = 300; m < 660; m += 30) co30.push_back(m);
  {
    PanelP s(5);
    s.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
    s.p.siec.siecJest = false;
    std::vector<WybP> v = symuluj(s, terapiaDoPoludnia(), start4(), poludnie());
    SPRAWDZ(zWifi(v) == co30);
    if (zWifi(v) != co30) drukuj("bez odpowiedzi", zWifi(v));
    ROWNE_L(zPowodem(v, "okno_zasilanie"), 0);
    ROWNE_L(zPowodem(v, "okno_geste"), 0);
  }
  {
    PanelP s(5);
    s.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
    uint32_t wraca = ep(2026, 11, 4, 8, 10);
    std::vector<WybP> v = symuluj(s, terapiaDoPoludnia(), start4(), poludnie(),
                                  [&](uint32_t t) { s.p.siec.siecJest = t >= wraca; });
    std::vector<int> oczek;
    for (int x : co30)
      if (x <= 8 * 60 + 30) oczek.push_back(x);
    for (int x : minutyZasilania())
      if (x > 8 * 60 + 30) oczek.push_back(x);
    SPRAWDZ(zWifi(v) == oczek);
    if (zWifi(v) != oczek) drukuj("po powrocie", zWifi(v));
  }
}

// Noc kompletna od rana (pobrana o 05:00): na zasilaniu bez odpytywania do 11:00, jak na baterii.
static void testDyrygentNocKompletna() {
  std::vector<int> wifi[2];
  int i = 0;
  for (Zasilanie z : {Zasilanie::ZEWNETRZNE, Zasilanie::BATERIA}) {
    PanelP s(5);
    s.p.czujniki.zas = z;
    ScenariuszAB sc{ep(2026, 11, 4, 4, 30), ep(2026, 11, 4, 4, 30)};
    std::vector<WybP> v = symuluj(s, sc, start4(), poludnie());
    ROWNE_L(minutaLok(chwilaNocy(v)), 5 * 60);
    ROWNE_L(zPowodem(v, "okno_zasilanie"), 0);
    wifi[i++] = zWifi(v, 0, 1440);
  }
  SPRAWDZ(wifi[0] == wifi[1]);
}

// E-ink: temperatura w obudowie zmienia się co 5 min (krok 0,2 °C). Wybudzenia z samym `okno_zasilanie` biorą stopkę
// z ostatniej klatki, więc nie odświeżają ekranu; terminy baterii odświeżają tak samo jak na baterii.
static void testDyrygentEink() {
  int eink[2] = {0, 0}, zRytmu = 0, i = 0;
  for (Zasilanie z : {Zasilanie::BATERIA, Zasilanie::ZEWNETRZNE}) {
    PanelP s(5);
    s.p.czujniki.zas = z;
    std::vector<WybP> v = symuluj(s, terapiaDoPoludnia(), start4(), poludnie(), [&](uint32_t t) {
      s.p.czujniki.o.temp = 20.0f + 0.2f * (float)((minutaLok(t) / 5) % 7);
    });
    for (auto& w : v) {
      if (w.powod != PowodWybudzenia::ZEGAR || w.minuta < 300 || w.minuta >= 660) continue;
      eink[i] += w.odswiezen;
      if (w.powody == "zegar:okno_zasilanie") zRytmu += w.odswiezen;
    }
    i++;
  }
  printf("  e-ink 05:00-11:00 przy zmiennej temperaturze, noc nie przychodzi: bateria %d, zasilanie %d "
         "(z samego rytmu zasilania %d)\n", eink[0], eink[1], zRytmu);
  SPRAWDZ(eink[0] > 0);
  ROWNE_L(eink[1], eink[0]);
  ROWNE_L(zRytmu, 0);
}

// Źródło otoczenia z CO2 innym przy każdym odczycie. Wybudzenie z samym
// `okno_zasilanie` nie czyta źródła otoczenia (zostaje ostatni odczyt, jak stopka), więc e-ink odświeża się tyle razy
// co na baterii; terminy baterii czytają źródło jak dotąd (świeżość nie gorsza niż na baterii). Gdy w takim
// wybudzeniu przyjdzie noc, źródło jest czytane (nowy ekran dnia ze świeżym odczytem).
static void testDyrygentEinkOtoczenie() {
  konfTeraz = &KONF_OTO;
  int eink[2] = {0, 0}, oto[2] = {0, 0}, zRytmu = 0, otoZRytmu = 0, i = 0;
  std::vector<int> minutyOto[2];
  for (Zasilanie z : {Zasilanie::BATERIA, Zasilanie::ZEWNETRZNE}) {
    PanelP s(5);
    s.p.czujniki.zas = z;
    int k = 0;
    ScenariuszAB sc = terapiaDoPoludnia();
    std::vector<WybP> v;
    uint32_t t = start4();
    PowodWybudzenia pw = PowodWybudzenia::START;
    for (int n = 0; n < 1500 && t < poludnie(); n++) {
      odpowiedzi(s, sc, t);
      s.p.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":" + std::to_string(700 + 37 * (k++ % 23)) +
                                            "},\"ostrzezenia\":[]}");
      WybP w = wybudz(s, t, pw);
      v.push_back(w);
      if (!w.spal) break;
      uint32_t snu = s.p.sen.plan.budzZegarem ? s.p.sen.plan.sekund : OKRES_SNU_S;
      t += s.p.zegar.msTeraz / 1000 + (snu ? snu : 1);
      pw = PowodWybudzenia::ZEGAR;
    }
    for (auto& w : v) {
      if (w.powod != PowodWybudzenia::ZEGAR || w.minuta < 300 || w.minuta >= 660) continue;
      eink[i] += w.odswiezen;
      oto[i] += w.otoczen;
      if (w.otoczen) minutyOto[i].push_back(w.minuta);
      if (w.powody == "zegar:okno_zasilanie") {
        zRytmu += w.odswiezen;
        otoZRytmu += w.otoczen;
        ROWNE_L(w.polaczen, 1);  // AirBridge dalej sprawdzany przy każdym wybudzeniu rytmu
      }
    }
    i++;
  }
  printf("  e-ink 05:00-11:00 ze źródłem otoczenia (CO2 inne przy każdym odczycie), noc nie przychodzi: bateria %d, "
         "zasilanie %d (z samego rytmu zasilania %d); odczyty otoczenia: bateria %d, zasilanie %d\n",
         eink[0], eink[1], zRytmu, oto[0], oto[1]);
  SPRAWDZ(eink[0] > 0);
  ROWNE_L(eink[1], eink[0]);
  ROWNE_L(zRytmu, 0);
  ROWNE_L(otoZRytmu, 0);
  SPRAWDZ(oto[0] > 0);
  SPRAWDZ(minutyOto[1] == minutyOto[0]);  // źródło czytane w tych samych minutach co na baterii
  // noc przychodzi w wybudzeniu z samego rytmu zasilania (09:15): źródło otoczenia czytane w tym wybudzeniu
  {
    PanelP s(5);
    s.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
    ScenariuszAB sc{ep(2026, 11, 4, 9, 11), ep(2026, 11, 4, 9, 11)};
    std::vector<WybP> v = symuluj(s, sc, start4(), poludnie(), [&](uint32_t) {});
    // symuluj() czyści odpowiedzi przed każdym wybudzeniem (odpowiedzi()), więc źródło otoczenia nie odpowiada:
    // liczy się sama próba odczytu w wybudzeniu z nocą
    int proby = 0, probyBezNocy = 0;
    for (auto& w : v) {
      if (w.powody != "zegar:okno_zasilanie") continue;
      if (w.minuta == 9 * 60 + 15) proby += w.otoczen;
      else probyBezNocy += w.otoczen;
    }
    SPRAWDZ(proby >= 1);
    ROWNE_L(probyBezNocy, 0);
    ROWNE_L(minutaLok(chwilaNocy(v)), 9 * 60 + 15);
  }
  konfTeraz = &KONF;
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  // (a)
  testStale();
  testBateriaBezZmian();
  testZasilanieTerminy();
  testZasilanieWylaczenia();
  testPowod();
  // (b)
  testDyrygentBateria();
  testDyrygentZasilanie();
  testDyrygentZasilanieNoc();
  testDyrygentDobaTypowa();
  testDyrygentOdlaczenie();
  testDyrygentPodlaczenie();
  testDyrygentBezOdpowiedzi();
  testDyrygentNocKompletna();
  testDyrygentEink();
  testDyrygentEinkOtoczenie();
  printf("testy_zasilanie_rytm: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
