// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe szybszego wyniku nocy rano (A + B + D; R-DANE-61, R-DANE-62, R-PRZ-22).
// Każdy błąd podaje w komunikacie nazwę przypadku. Część testów tylko mierzy i drukuje liczby.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_poranek_brzegowe, blokuje).
// LOG_PRZEBIEGOW=1 drukuje przebiegi dób.
#include <cmath>
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
#define SPRAWDZ(id, w)                                                              \
  do {                                                                              \
    if (w) ok++;                                                                    \
    else { zle++; printf("BŁĄD %s %s:%d: %s\n", id, __FILE__, __LINE__, #w); }     \
  } while (0)

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";
static bool logPrzebiegow() { return getenv("LOG_PRZEBIEGOW") != nullptr; }

static int sd(int h, int m, int s = 0) { return h * 3600 + m * 60 + s; }
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
static std::vector<Noc> historia(int ile, int koniec = 7 * 60) {
  std::vector<Noc> h;
  for (int i = ile; i >= 1; i--) h.push_back(noc(dodajDni("2026-11-04", -i), 660, koniec + 24 * 60 - 23 * 60));
  return h;
}

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
static const long DOBA = nrDnia("2026-11-03");

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

struct PanelR {
  Panel p;
  explicit PanelR(int nocyHistorii, int koniec = 7 * 60) {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    std::string w;
    for (auto& n : historia(nocyHistorii, koniec)) w += wierszNocyV3(n) + "\n";
    p.pliki.pliki["/dane/noce/2026.csv"] = std::string("#cpap-eink noce 4\n") + POLA_V3 + "\n" + w + "#koniec " +
                                           std::to_string(nocyHistorii) + "\n";
    p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  }
  bool maNoc() const {
    auto it = p.pliki.pliki.find("/dane/noce/2026.csv");
    return it != p.pliki.pliki.end() && it->second.find("\n2026-11-04;") != std::string::npos;
  }
};

// Stan AirBridge: terapia do terapiaDo, przetwarzanie do przetwarzaDo, potem noc; kod401 = złe hasło cały czas;
// cisza = Wi-Fi jest, AirBridge nie odpowiada.
struct Scen {
  uint32_t terapiaDo = 0, przetwarzaDo = 0;
  bool kod401 = false, cisza = false;
};
static void odpowiedzi(PanelR& s, const Scen& sc, uint32_t t) {
  s.p.siec.odpowiedzi.clear();
  if (sc.cisza) return;
  if (sc.kod401) {
    s.p.siec.dodaj("/api/status", 401, "");
    return;
  }
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

struct Wyb {
  uint32_t ep = 0;
  int minuta = 0;
  PowodWybudzenia powod = PowodWybudzenia::ZEGAR;
  int polaczen = 0, odswiezen = 0, get = 0;
  uint32_t msWybudzenia = 0;
  bool spal = false, noc = false;
  std::string powody;
};
static Wyb wybudz(PanelR& s, uint32_t t, PowodWybudzenia pw) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.p.siec.polaczonaTeraz = false;
  s.p.d.z.clear();
  int snow = s.p.sen.snow;
  Dyrygent(s.p.adaptery(), KONF).wybudzenie();
  Wyb w;
  w.ep = t;
  w.minuta = minutaLok(t);
  w.powod = pw;
  w.spal = s.p.sen.snow == snow + 1;
  w.noc = s.maNoc();
  w.msWybudzenia = s.p.zegar.msTeraz;
  for (auto& z : s.p.d.z) {
    w.polaczen += z == "siec.polacz";
    w.odswiezen += z == "ekran.odswiez";
    w.get += z.rfind("siec.get", 0) == 0;
    size_t a = z.find("wybudzenie ");
    if (w.powody.empty() && a != std::string::npos && z.find("cpap-eink") != std::string::npos) {
      size_t b = z.find(',', a);
      w.powody = z.substr(a + 11, b == std::string::npos ? std::string::npos : b - a - 11);
    }
  }
  return w;
}
// Doba wybudzeń zegarem; temperatura przy łóżku zmienia się co wybudzenie o `krokTemp` (0 = stała, jak w testach podstawowych).
static std::vector<Wyb> symuluj(PanelR& s, const Scen& sc, uint32_t od, uint32_t doT, float krokTemp = 0) {
  std::vector<Wyb> v;
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 600 && t < doT; i++) {
    odpowiedzi(s, sc, t);
    if (krokTemp != 0) s.p.czujniki.o.temp = 20.0f + krokTemp * (float)(i % 4);
    Wyb w = wybudz(s, t, pw);
    v.push_back(w);
    if (!w.spal) break;
    uint32_t snu = s.p.sen.plan.budzZegarem ? s.p.sen.plan.sekund : OKRES_SNU_S;
    t += s.p.zegar.msTeraz / 1000 + (snu ? snu : 1);
    pw = PowodWybudzenia::ZEGAR;
  }
  if (logPrzebiegow())
    for (auto& w : v)
      printf("    %s Wi-Fi %d, GET %d, e-ink %d, %u ms, noc %d, %s\n", hhmm(w.minuta).c_str(), w.polaczen, w.get,
             w.odswiezen, (unsigned)w.msWybudzenia, (int)w.noc, w.powody.c_str());
  return v;
}
struct Suma {
  int wyb = 0, zWifi = 0, eink = 0, einkRano = 0, gestych = 0;
  uint32_t msWifiWyb = 0;
};
static Suma suma(const std::vector<Wyb>& v) {
  Suma s;
  for (auto& w : v) {
    if (w.powod != PowodWybudzenia::ZEGAR) continue;
    s.wyb++;
    s.zWifi += w.polaczen > 0;
    s.eink += w.odswiezen;
    if (w.minuta >= 5 * 60 && w.minuta < 11 * 60) s.einkRano += w.odswiezen;
    s.gestych += w.powody.find("okno_geste") != std::string::npos;
    if (w.polaczen > 0) s.msWifiWyb += w.msWybudzenia;
  }
  return s;
}

static Wyb przycisk(PanelR& s, uint32_t t, uint8_t bit, Ekran e, uint32_t trzymajMs = 150) {
  s.p.start.budzace = bit;
  s.p.start.stan.ekran = (uint8_t)e;
  s.p.start.stan.ekranOd = t - 60;
  s.p.przyciski.skrypt = {{0, bit}, {trzymajMs, 0}};
  return wybudz(s, t, PowodWybudzenia::PRZYCISK);
}
static bool wLogu(PanelR& s, const std::string& f) {
  for (auto& z : s.p.d.z)
    if (z.find(f) != std::string::npos) return true;
  return false;
}

// ============================================================================================================
// E-ink. Wybudzenia gęstego okna rysują ekran dnia ze świeżym odczytem czujnika w stopce (stopkaZPamieci
// tylko dla slotów), więc każda zmiana temperatury o 0,1 °C to pełne odświeżenie e-ink. Autor liczył dobę przy stałej
// temperaturze (atrapa 21,5 °C), stąd „wybudzenia gęstego okna nie zmieniały klatki”.
// ============================================================================================================
static void testEinkPrzyZmiennejTemperaturze() {
  Scen typowa{ep(2026, 11, 4, 7, 2), ep(2026, 11, 4, 7, 2)};
  PanelR przed(2), po(5);
  if (logPrzebiegow()) printf("  e-ink przed zmianą (2 noce):\n");
  Suma a = suma(symuluj(przed, typowa, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 4), 0.2f));
  if (logPrzebiegow()) printf("  e-ink po zmianie (5 nocy):\n");
  Suma b = suma(symuluj(po, typowa, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 4), 0.2f));
  printf("  e-ink: doba typowa, temperatura zmienna (krok 0,2 °C): e-ink przed %d (rano %d), po %d (rano %d); "
         "wybudzenia gęstego okna %d\n", a.eink, a.einkRano, b.eink, b.einkRano, b.gestych);
  // oczekiwanie: wybudzenie gęstego okna bez nowej nocy nie odświeża e-ink (jak slot dzienny, stopka z RTC)
  // wzrost bez przekroczenia budżetu: P3 (do pomiaru na egzemplarzu), bez czerwonego testu
  SPRAWDZ("e-ink: budżet 8.1 (30 e-ink)", b.eink <= 30);
}

// ============================================================================================================
// Złe hasło (401). Gęste okno zostaje (R-DANE-61), a noc nigdy nie przyjdzie: koszt Wi-Fi każdego dnia.
// Pomiar (bez progu): wybudzenia z Wi-Fi na dobę przed zmianą i po niej.
// ============================================================================================================
static void testZleHaslo() {
  Scen z;
  z.kod401 = true;
  PanelR przed(2), po(5);
  Suma a = suma(symuluj(przed, z, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 4)));
  Suma b = suma(symuluj(po, z, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 4)));
  printf("  złe hasło: doba ze złym hasłem: wybudzenia z Wi-Fi przed %d, po %d (gęste %d); czas wybudzeń z Wi-Fi przed "
         "%u s, po %u s (atrapa: Wi-Fi 1,5 s)\n", a.zWifi, b.zWifi, b.gestych, (unsigned)(a.msWifiWyb / 1000),
         (unsigned)(b.msWifiWyb / 1000));
  SPRAWDZ("złe hasło: pomiar", b.zWifi >= a.zWifi);
}

// ============================================================================================================
// D przy AirBridge, który milczy (Wi-Fi jest). Każde wybudzenie przyciskiem rano (raz na 2 min) trzyma
// panel w pobieraniu do ok. 70 s; żądanie HTTP (15 s) nie sprawdza przycisków, więc naciśnięcia w tym czasie idą
// tylko do kolejki (holdy i ekrany spoza ciągu stron przepadają, R-PRZ-20). Gęste okno wyłączone przy E-01
// (decyzja właściciela projektu 10.11.2026), a D nie.
// ============================================================================================================
static void testPrzyciskPrzyCiszyAirBridge() {
  Scen cisza;
  cisza.cisza = true;
  PanelR s(5);
  uint32_t t0 = ep(2026, 11, 4, 4, 0);
  symuluj(s, cisza, t0, ep(2026, 11, 4, 6, 5));  // 05:00, 05:30, 06:00 bez odpowiedzi: E-01
  uint32_t t = ep(2026, 11, 4, 6, 10, 30);
  odpowiedzi(s, cisza, t);
  Wyb w = przycisk(s, t, BIT_LEWY, Ekran::DZIEN);
  PanelR s12(5);
  symuluj(s12, cisza, t0, ep(2026, 11, 4, 6, 5));
  uint32_t t12 = ep(2026, 11, 4, 12, 0, 30);
  odpowiedzi(s12, cisza, t12);
  Wyb w12 = przycisk(s12, t12, BIT_LEWY, Ekran::DZIEN);
  printf("  cisza AirBridge: przycisk 06:10 przy ciszy AirBridge: Wi-Fi %d, GET %d, wybudzenie %u ms (12:00 bez D: %u ms; "
         "w obu 20 s czuwania)\n", w.polaczen, w.get, (unsigned)w.msWybudzenia, (unsigned)w12.msWybudzenia);
  // oczekiwanie: przy E-01 (ostatnia próba bez odpowiedzi) D nie blokuje panelu (jak gęste okno)
  SPRAWDZ("cisza AirBridge", w.msWybudzenia <= w12.msWybudzenia + 10000);
}

// ============================================================================================================
// D na pytaniu „Co było inaczej?” (siatka) i innych ekranach spoza ciągu stron. Wymaganie wymienia je jako
// wyjątki; kod wyklucza tylko TEST, SIEC_PANELU, FORMULARZ. Na PYTANIE kolejka naciśnięć z czasu pobierania jest
// odrzucana (zKolejkiWolno), więc ruch kursora w czasie żądania HTTP przepada.
// ============================================================================================================
static void testPytanieBezPobrania() {
  Scen jest;
  jest.terapiaDo = ep(2026, 11, 4, 9, 0);  // terapia trwa: sam status, noc nie przyjdzie
  jest.przetwarzaDo = jest.terapiaDo;
  PanelR s(5);
  uint32_t t = ep(2026, 11, 4, 6, 10, 30);
  odpowiedzi(s, jest, t);
  // ↻ 2 s na K1 budzi panel i otwiera „Co było inaczej?” dla ostatniej nocy (R-CZY-08)
  Wyb w = przycisk(s, t, BIT_SRODEK, Ekran::K1, 2200);
  printf("  pytanie bez pobrania: ↻ 2 s na K1 o 06:10: ekran %d (PYTANIE = %d), Wi-Fi %d\n", (int)s.p.start.stan.ekran,
         (int)Ekran::PYTANIE, w.polaczen);
  if (s.p.start.stan.ekran == (uint8_t)Ekran::PYTANIE) SPRAWDZ("pytanie bez pobrania", w.polaczen == 0);
  // ten sam ekran, naciśnięcie z czasu pobierania tylko w kolejce (ISR): na PYTANIE przepada
  PanelR s2(5);
  odpowiedzi(s2, jest, t);
  s2.p.przyciski.kolejka = {{(int)Przycisk::PRAWY, 150}};
  przycisk(s2, t, BIT_SRODEK, Ekran::K1, 2200);
  bool pominiete = wLogu(s2, "0 wykonane, 1 pominięte");
  printf("  pytanie bez pobrania: naciśnięcie ▶ z czasu pobierania na PYTANIE: %s\n", pominiete ? "pominięte (przepada)" : "wykonane");
}

// ============================================================================================================
// Poranne pytanie po nocy z D. D pobiera bez porannej decyzji; decyzja zapada przy najbliższym wybudzeniu
// z pobraniem. Sprawdzamy, że nie przepada na stałe i kiedy zapada (wpis „C” w /dane/czynniki.csv).
// Noc z odchyleniem (AHI 25) i 20 nocy historii, żeby decyzja była deterministyczna (warstwa O).
// ============================================================================================================
static std::string raportZly(long day) {
  std::string r = raport(day);
  size_t a = r.find("\"ahi\":1.4");
  return r.replace(a, 9, "\"ahi\":25.0");
}
static void odpowiedziZle(PanelR& s, uint32_t) {
  s.p.siec.odpowiedzi.clear();
  s.p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  s.p.siec.dodaj(sciezkaListy(), 200, lista(DOBA, DOBA));
  s.p.siec.dodaj(sciezkaNocy(DOBA), 200, raportZly(DOBA));
}
static bool wpisCzynnikow(PanelR& s) {
  auto it = s.p.pliki.pliki.find("/dane/czynniki.csv");
  return it != s.p.pliki.pliki.end() && it->second.find("2026-11-04") != std::string::npos;
}
static void testPorannePytaniePoD() {
  // ścieżka zegara: noc o 06:30 z wybudzenia zegarem
  PanelR z(20);
  odpowiedziZle(z, 0);
  wybudz(z, ep(2026, 11, 4, 6, 30, 5), PowodWybudzenia::START);
  bool zZegara = wpisCzynnikow(z);
  // ścieżka D: noc o 06:10 po przycisku, potem zegar do 12:00
  PanelR d(20);
  Scen terapia{ep(2026, 11, 4, 6, 0), ep(2026, 11, 4, 6, 0)};
  symuluj(d, terapia, ep(2026, 11, 4, 4, 0), ep(2026, 11, 4, 5, 50));
  uint32_t t = ep(2026, 11, 4, 6, 10, 30);
  odpowiedziZle(d, t);
  przycisk(d, t, BIT_PRAWY, Ekran::DZIEN);
  bool poD = wpisCzynnikow(d);
  bool nocD = d.maNoc();
  int kiedy = -1;
  uint32_t tt = t + 60;
  PowodWybudzenia pw = PowodWybudzenia::ZEGAR;
  for (int i = 0; i < 100 && tt < ep(2026, 11, 4, 12, 0); i++) {
    odpowiedziZle(d, tt);
    Wyb w = wybudz(d, tt, pw);
    if (kiedy < 0 && wpisCzynnikow(d)) kiedy = w.minuta;
    tt += d.p.zegar.msTeraz / 1000 + (d.p.sen.plan.sekund ? d.p.sen.plan.sekund : 1);
  }
  printf("  poranne pytanie: poranna decyzja: z zegara od razu %d; po D (noc %d) od razu %d, potem o %s\n", (int)zZegara,
         (int)nocD, (int)poD, kiedy < 0 ? "nigdy do 12:00" : hhmm(kiedy).c_str());
  SPRAWDZ("poranne pytanie nie przepada", !zZegara || kiedy >= 0);
  // oczekiwanie: decyzja w tym samym wybudzeniu co noc (jak R-PRZ-06 ↻ na S5/S10), linia zaproszenia
  // na pierwszym ekranie z nocą
  SPRAWDZ("poranne pytanie od razu", !zZegara || poD);
}

// ============================================================================================================
// Powtórka B liczona także wtedy, gdy wypada w tej samej sekundzie co termin siatki (licznik doby zużyty,
// choć wybudzenie i tak było). Test planu.
// ============================================================================================================
static void testPowtorkaRownaSiatce() {
  WejsciePlanu w;
  w.zegar = true;
  w.sekundaDoby = sd(7, 30, 5);
  w.obsluzoneZnane = true;
  w.obsluzoneDo = sd(7, 28, 10);
  w.przetwarzaOd = sd(7, 28, 0);  // powtórka 07:30:00 = termin siatki 07:30
  Powody p = powodyTeraz(w);
  printf("  powtórka a siatka: 07:30:05 po statusie „przetwarza” o 07:28:00: powody %s\n", nazwyPowodow(p).c_str());
  SPRAWDZ("powtórka a siatka", !((p & POWOD_OKNO_PORANNE) && (p & POWOD_POWTORKA)));
}

// ============================================================================================================
// Regresja: zero zapytań 23:30–05:00 przy D na granicach, przy B ze stanem RTC sprzed północy, przy zegarze nieznanym.
// ============================================================================================================
static void testRegresjaGranice() {
  Scen jest;
  for (int s : {sd(4, 59, 59), sd(23, 29, 59), sd(23, 30, 0), sd(11, 0, 0)}) {
    PanelR p(5);
    uint32_t t = ep(2026, 11, 4, 0, 0) + (uint32_t)s;
    odpowiedzi(p, jest, t);
    Wyb w = przycisk(p, t, BIT_PRAWY, Ekran::DZIEN);
    SPRAWDZ("granica D", w.polaczen == 0);
  }
  {  // 05:00:00 dokładnie: D pobiera (okno poranne od 05:00)
    PanelR p(5);
    uint32_t t = ep(2026, 11, 4, 5, 0, 0);
    odpowiedzi(p, jest, t);
    SPRAWDZ("granica D 05:00", przycisk(p, t, BIT_PRAWY, Ekran::DZIEN).polaczen == 1);
  }
  // B: przetwarzanie odczytane wieczorem (np. 23:30 POCZATEK_NOCY) nie daje powtórki w nocy ani rano
  WejsciePlanu w;
  w.zegar = true;
  w.sekundaDoby = sd(23, 31, 0);
  w.przetwarzaOd = sd(23, 30, 10);
  SPRAWDZ("B wieczór", terminPowtorki(w) < 0);
  w.sekundaDoby = sd(4, 0, 0);
  w.przetwarzaOd = sd(3, 59, 0);
  SPRAWDZ("B noc", terminPowtorki(w) < 0);
  w.sekundaDoby = sd(10, 58, 30);
  w.przetwarzaOd = sd(10, 58, 0);
  SPRAWDZ("B przed 11", terminPowtorki(w) < 0);
  w.zegar = false;
  w.przetwarzaOd = sd(7, 0, 0);
  w.sekundaDoby = sd(7, 1, 0);
  SPRAWDZ("B bez zegara", terminPowtorki(w) < 0);
  // A: koniec terapii nocy z M-18 po północy (sesja od 01:00) i bardzo duże M-01
  SPRAWDZ("A po północy", koniecTerapiiNocy(noc("2026-11-03", 13 * 60, 360)) == 13 * 60 + 360);
  std::vector<Noc> h = {noc("2026-11-01", 13 * 60, 360), noc("2026-11-02", 13 * 60, 360), noc("2026-11-03", 13 * 60, 360)};
  SPRAWDZ("A po północy mediana", zwyklyKoniecTerapii(h, "2026-11-04") == 7 * 60);
  std::vector<Noc> g = {noc("2026-11-01", 660, 1400), noc("2026-11-02", 660, 1400), noc("2026-11-03", 660, 1400)};
  int zk = zwyklyKoniecTerapii(g, "2026-11-04");  // 22:20: poza oknem porannym, gęste okno puste
  WejsciePlanu wg;
  wg.zwyklyKoniec = zk;
  bool jakiekolwiek = false;
  for (int m = 0; m < 24 * 60; m++) jakiekolwiek = jakiekolwiek || wGestymOknie(wg, m);
  SPRAWDZ("A M-01 ogromne", !jakiekolwiek);
}

// Regresja: limit 5 powtórek przy przetwarzaniu do 10:59 (licznik w RTC przez całą dobę).
static void testRegresjaLimitPowtorek() {
  Scen d{ep(2026, 11, 4, 5, 10), ep(2026, 11, 4, 11, 30)};
  PanelR s(5);
  std::vector<Wyb> v = symuluj(s, d, ep(2026, 11, 4, 4, 0), ep(2026, 11, 4, 13, 0));
  int powt = 0, krotkich = 0;
  for (size_t i = 0; i < v.size(); i++) {
    powt += v[i].powody.find("powtorka_przetwarzania") != std::string::npos;
    if (i > 0 && v[i].ep - v[i - 1].ep < 60) krotkich++;
  }
  printf("  regresja: przetwarzanie 05:10–11:30: powtórek %d, wybudzeń w odstępie < 60 s: %d\n", powt, krotkich);
  SPRAWDZ("limit B", powt == PLAN_POWTORKI_MAKS);
  SPRAWDZ("bez serii krótkich snów", krotkich == 0);
}

// Zmienione starsze testy: samosprawdzenie po przycisku rano (wersja do sprawdzenia, T-WYM-41) przy nocy
// nieobecnej; test podstawowy przesunięty na 12:00. Rano D pobiera przed samosprawdzeniem.
static void testSamosprawdzenieRano() {
  for (int h : {10, 12}) {
    Panel p;
    p.zegar.epoch = ep(2026, 11, 4, h, 0);
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.start.stanW = StanWersji::DO_SPRAWDZENIA;
    p.start.budzace = BIT_PRAWY;
    p.przyciski.skrypt = {{0, BIT_PRAWY}, {200, 0}};
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    printf("  samosprawdzenie po przycisku %02d:00: połączeń %d, żądań HTTP %d, oznaczona %d, sen %d, %u ms\n", h,
           p.d.ile("siec.polacz"), (int)p.siec.zadania.size(), (int)p.start.oznaczona, p.sen.snow,
           (unsigned)p.zegar.msTeraz);
    SPRAWDZ("samosprawdzenie rano", p.start.oznaczona && p.sen.snow == 1);
  }
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testEinkPrzyZmiennejTemperaturze();
  testZleHaslo();
  testPrzyciskPrzyCiszyAirBridge();
  testPytanieBezPobrania();
  testPorannePytaniePoD();
  testPowtorkaRownaSiatce();
  testRegresjaGranice();
  testRegresjaLimitPowtorek();
  testSamosprawdzenieRano();
  printf("testy_poranek_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
