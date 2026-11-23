// SPDX-License-Identifier: GPL-3.0-or-later
// R-AW-41 (decyzja właściciela projektu, 23.11.2026): linia E-08 `Brak odczytu CO2 od {czas}.` także wtedy, gdy samo
// lokalne źródło otoczenia nie odpowiada (brak odpowiedzi -1, -400, odmowa 401, zła wersja…) co najmniej OTO_CISZA_CO2_S
// (3 h) od pierwszej nieudanej próby po ostatniej udanej odpowiedzi (`{czas}` = ostatnia udana odpowiedź); raz na
// epizod; koniec epizodu po pierwszej udanej odpowiedzi. Przed zmianą ostrzeżenie E-08 przychodziło tylko w udanej
// odpowiedzi, a CO2 po 30 min znikało ze stopki bez komunikatu (źródło mogło milczeć wiele dni niezauważone).
// T-TEST-523. Przypadki brzegowe A6…A10; A11 świeżość CO2 75 min (decyzja właściciela projektu, 23.11.2026).
// Harness z test/testy_slot_dzienny_dyrygent.cpp (main przemianowany), jak test/testy_karta_po_aktualizacji_slot.cpp:
// doba 4.11.2026 na baterii, start zasilania 00:05, noc z AirBridge od 06:40.
// Część A (dyrygent na atrapach) kompiluje się i na kodzie sprzed poprawki (sprawdzenie „test pada bez poprawki”);
// część B (funkcje R-AW-41) dopiero z poprawką.
#define main main_testy_slot_dzienny_dyrygent
#include "testy_slot_dzienny_dyrygent.cpp"
#undef main

#include "otoczenie.h"
#include "sypialnia.h"

#ifndef CO2_CISZA_BEZ_POPRAWKI
#include "wybudzenie.h"
#else
const uint32_t OTO_CISZA_CO2_S = 3 * 3600;  // kod sprzed poprawki nie ma stałej R-AW-41
#endif

// Źródło otoczenia w symulacji: 200 z CO2 600 poza [awariaOd, powrotOd) i drugą awarią od awaria2Od; w awarii `rodzaj`.
enum class Awaria { BRAK_ODPOWIEDZI, CZAS_400, ODMOWA_401, ZLA_WERSJA };
static Awaria rodzaj = Awaria::BRAK_ODPOWIEDZI;
static uint32_t awariaOd = 0, powrotOd = 0xFFFFFFFFu, awaria2Od = 0xFFFFFFFFu;
static const char* OTO_OK = "{\"wersja\":1,\"sypialnia\":{\"co2\":600},\"ostrzezenia\":[]}";
static void odpCisza(PanelSym& s, uint32_t t) {
  odpowiedziB9(s, t);
  s.siec.odpowiedzi.erase("/otoczenie");
  bool awaria = (t >= awariaOd && t < powrotOd) || t >= awaria2Od;
  if (!awaria) {
    s.siec.dodaj("/otoczenie", 200, OTO_OK);
    return;
  }
  switch (rodzaj) {
    case Awaria::BRAK_ODPOWIEDZI: s.siec.dodaj("/otoczenie", -1, ""); break;
    case Awaria::CZAS_400: s.siec.dodaj("/otoczenie", 200, OTO_OK, OTO_BUDZET_MS + 1000); break;
    case Awaria::ODMOWA_401: s.siec.dodaj("/otoczenie", 401, ""); break;
    case Awaria::ZLA_WERSJA: s.siec.dodaj("/otoczenie", 200, "{\"wersja\":2}"); break;
  }
}
static void ustawZrodlo(Awaria r, uint32_t od, uint32_t powrot = 0xFFFFFFFFu, uint32_t od2 = 0xFFFFFFFFu) {
  rodzaj = r;
  awariaOd = od;
  powrotOd = powrot;
  awaria2Od = od2;
}

static const std::string LINIA = "Brak odczytu CO2";
static std::string hhmm(uint32_t t) {
  int m = minutaLok(t);
  char b[16];
  snprintf(b, sizeof b, "%02d:%02d", m / 60, m % 60);
  return b;
}
// Chwila ostatniego wybudzenia przed `doT`, które zapytało źródło i dostało 200 (pobrano = ta chwila + czas odpowiedzi).
static uint32_t ostatniaUdana(const std::vector<Wyb>& v, uint32_t doT) {
  uint32_t t = 0;
  for (auto& w : v)
    if (w.ep < doT && zadanZeSciezka(w, "/otoczenie") > 0) t = w.ep;
  return t;
}
// Chwila pierwszego wybudzenia od `od`, które zapytało źródło (w awarii: pierwsza nieudana próba).
static uint32_t pierwszaProba(const std::vector<Wyb>& v, uint32_t od) {
  for (auto& w : v)
    if (w.ep >= od && zadanZeSciezka(w, "/otoczenie") > 0) return w.ep;
  return 0;
}
static KonfiguracjaPanelu konfOto() {
  KonfiguracjaPanelu k = konf(true);
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  k.otoToken = "t";
  return k;
}
// Wybudzenia z linią (minuty) i pierwsza z tekstem linii.
struct Linie {
  std::vector<int> minuty;
  std::string tekst;
};
static Linie linie(const std::vector<Wyb>& v, uint32_t od = 0, uint32_t doT = 0xFFFFFFFFu) {
  Linie l;
  for (auto& w : v) {
    if (w.ep < od || w.ep >= doT) continue;
    for (auto& z : w.zdarzenia) {
      size_t p = z.find(LINIA);
      if (p == std::string::npos) continue;
      l.minuty.push_back(w.minuta);
      if (l.tekst.empty()) l.tekst = z.substr(p, z.find('.', p) == std::string::npos ? std::string::npos : z.find('.', p) - p + 1);
      break;
    }
  }
  return l;
}
static void drukujLinie(const char* nazwa, const Linie& l) {
  printf("  %s: wybudzeń z linią %zu", nazwa, l.minuty.size());
  if (!l.minuty.empty()) printf(", pierwsza %02d:%02d `%s`", l.minuty[0] / 60, l.minuty[0] % 60, l.tekst.c_str());
  printf("\n");
}

// A1: źródło nie odpowiada od 08:00 (-1, -400, 401, zła wersja) → linia E-08 najwcześniej 3 h po pierwszej nieudanej
// próbie, z godziną ostatniej udanej odpowiedzi; wcześniej bez linii; U-05 podaje przyczynę słowami.
// wymaganie: R-AW-41
static void testR_AW_41_episodPo3h(Awaria r, const char* nazwa, const char* slowoU05) {
  SPRAWDZ(OTO_CISZA_CO2_S == 3u * 3600u);  // kotwica progu R-AW-41: 3 h wpisane na sztywno
  ustawZrodlo(r, ep(2026, 11, 4, 8, 0));
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 14, 30), odpCisza, false, konfOto());
  uint32_t udana = ostatniaUdana(v, awariaOd), nieudana = pierwszaProba(v, awariaOd);
  Linie l = linie(v);
  drukujLinie(nazwa, l);
  SPRAWDZ(udana > 0 && nieudana > udana);
  SPRAWDZ(!l.minuty.empty());
  if (l.minuty.empty()) return;
  int prog = minutaLok(nieudana) + (int)(OTO_CISZA_CO2_S / 60);
  SPRAWDZ(l.minuty.front() >= prog);  // przed 3 h ciszy bez linii
  for (int m : l.minuty) SPRAWDZ(m >= prog);
  SPRAWDZ(l.tekst == "Brak odczytu CO2 od " + hhmm(udana) + ".");
  // U-05 (R-AW-25, R-KONF-58): przyczyna słowami, wiersz z usterką
  const Otoczenie& o = s.p.start.stan.oto;
  std::vector<std::string> u05 = wartosciTestuOtoczenia(o, "");
  printf("  %s: U-05 `Pogoda i powietrze: %s`\n", nazwa, u05.empty() ? "" : u05.front().c_str());
  SPRAWDZ(!u05.empty() && u05.front() == slowoU05);
  SPRAWDZ(usterkaOtoczenia(o));
}

// A2: bez OTO_URL (funkcja wyłączona) i przy `co2=nie` linii nie ma, choć źródło milczy od 08:00.
// wymaganie: R-AW-41
static void testR_AW_41_bezZrodlaIBezCo2() {
  ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, ep(2026, 11, 4, 8, 0));
  {
    PanelSym s;
    std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 14, 30), odpCisza, false, konf(true));
    Linie l = linie(v);
    drukujLinie("bez OTO_URL", l);
    SPRAWDZ(l.minuty.empty());
    SPRAWDZ(ileZadan(v, "/otoczenie") == 0);
  }
  {
    PanelSym s;
    s.p.pliki.pliki["/konfiguracja.txt"] = "co2=nie\n";
    std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 14, 30), odpCisza, false, konfOto());
    Linie l = linie(v);
    drukujLinie("co2=nie", l);
    SPRAWDZ(l.minuty.empty());
    SPRAWDZ(ileZadan(v, "/otoczenie") > 0);  // źródło jest pytane (pogoda), tylko linii CO2 nie ma
  }
}

// A3: koniec epizodu po pierwszej udanej odpowiedzi (źródło wraca 11:30; w slocie dziennym panel pyta o pełnych
// godzinach), potem bez linii; druga awaria od 13:30 to nowy epizod z godziną ostatniej udanej odpowiedzi przed nią.
// wymaganie: R-AW-41
static void testR_AW_41_koniecINowyEpizod() {
  ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, ep(2026, 11, 4, 8, 0), ep(2026, 11, 4, 11, 30), ep(2026, 11, 4, 13, 30));
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 20, 30), odpCisza, false, konfOto());
  uint32_t ok1 = ostatniaUdana(v, awariaOd), ok2 = ostatniaUdana(v, awaria2Od);
  Linie a = linie(v, 0, powrotOd), b = linie(v, powrotOd, ok2 + OTO_CISZA_CO2_S), c = linie(v, ok2 + OTO_CISZA_CO2_S);
  drukujLinie("epizod 1", a);
  drukujLinie("po powrocie, przed 3 h drugiej ciszy", b);
  drukujLinie("epizod 2", c);
  SPRAWDZ(!a.minuty.empty() && a.tekst == "Brak odczytu CO2 od " + hhmm(ok1) + ".");
  SPRAWDZ(ok2 >= powrotOd && ok2 < awaria2Od);
  SPRAWDZ(b.minuty.empty());
  SPRAWDZ(!c.minuty.empty() && c.tekst == "Brak odczytu CO2 od " + hhmm(ok2) + ".");
}

// A4: restart (zanik zasilania, pamięć RTC pusta) w trakcie ciszy o 09:30, przed linią: linia dalej po 3 h od
// ostatniej udanej odpowiedzi sprzed restartu, z jej godziną (nie 3 h od restartu); restart po linii o 12:10 nie zmienia
// godziny w linii.
// wymaganie: R-AW-41
static PanelSym* poRestarcie(PanelSym& s) {
  PanelSym* q = new PanelSym;
  q->p.pliki.pliki = s.p.pliki.pliki;
  q->p.nvs.teksty = s.p.nvs.teksty;
  q->p.nvs.liczbyU = s.p.nvs.liczbyU;
  q->p.nvs.liczby = s.p.nvs.liczby;
  q->p.nvs.flagi = s.p.nvs.flagi;
  q->p.nvs.bajtyMap = s.p.nvs.bajtyMap;
  return q;
}
static void testR_AW_41_restart() {
  ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, ep(2026, 11, 4, 8, 0));
  PanelSym s;
  std::vector<Wyb> v1 = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 9, 30), odpCisza, false, konfOto());
  uint32_t udana = ostatniaUdana(v1, awariaOd), nieudana = pierwszaProba(v1, awariaOd);
  SPRAWDZ(udana > 0 && nieudana > udana && linie(v1).minuty.empty());
  PanelSym* q = poRestarcie(s);
  std::vector<Wyb> v2 = symuluj(*q, ep(2026, 11, 4, 9, 35), ep(2026, 11, 4, 12, 0), odpCisza, false, konfOto());
  Linie l = linie(v2);
  drukujLinie("restart 09:35 w ciszy", l);
  int prog = minutaLok(nieudana) + (int)(OTO_CISZA_CO2_S / 60);
  SPRAWDZ(!l.minuty.empty());
  if (!l.minuty.empty()) {
    SPRAWDZ(l.minuty.front() >= prog);
    SPRAWDZ(l.minuty.front() < 9 * 60 + 35 + (int)(OTO_CISZA_CO2_S / 60));  // nie 3 h od restartu
    SPRAWDZ(l.tekst == "Brak odczytu CO2 od " + hhmm(udana) + ".");
  }
  PanelSym* r = poRestarcie(*q);
  std::vector<Wyb> v3 = symuluj(*r, ep(2026, 11, 4, 12, 10), ep(2026, 11, 4, 14, 30), odpCisza, false, konfOto());
  Linie l3 = linie(v3);
  drukujLinie("restart 12:10 po linii", l3);
  SPRAWDZ(l3.tekst.empty() || l3.tekst == "Brak odczytu CO2 od " + hhmm(udana) + ".");
  delete q;
  delete r;
}

// A5: regresja E-08 z ostrzeżenia `co2_milczy` (R-AW-27) bez zmian: źródło odpowiada z ostrzeżeniem od 10:00 → linia
// z godziną `od` ostrzeżenia.
// wymaganie: R-AW-27
static void odpOstrzezenie(PanelSym& s, uint32_t t) {
  odpowiedziB9(s, t);
  s.siec.odpowiedzi.erase("/otoczenie");
  std::string ost = t >= ep(2026, 11, 4, 10, 0) ? "[{\"kod\":\"co2_milczy\",\"od\":\"2026-11-04T09:40:00+01:00\"}]" : "[]";
  s.siec.dodaj("/otoczenie", 200, "{\"wersja\":1,\"sypialnia\":{\"co2\":600},\"ostrzezenia\":" + ost + "}");
}
static void testR_AW_27_regresja() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 14, 30), odpOstrzezenie, false, konfOto());
  Linie l = linie(v);
  drukujLinie("E-08 z co2_milczy", l);
  SPRAWDZ(!l.minuty.empty() && l.minuty.front() >= 10 * 60 && l.tekst == "Brak odczytu CO2 od 09:40.");
}

// Wybudzenia z linią i tekstem każdej z nich (nie tylko pierwszej) oraz czy było wśród nich przerysowanie R-EKR-60.
struct LinieWszystkie {
  std::vector<std::string> teksty;
  int przerysowan = 0;  // wybudzenia z linią i wpisem „przerysowanie R-EKR-60” (linia z pamięci RTC, dzienLinia = 3)
};
static LinieWszystkie linieWszystkie(const std::vector<Wyb>& v) {
  LinieWszystkie l;
  for (auto& w : v) {
    std::string tekst;
    bool przer = false;
    for (auto& z : w.zdarzenia) {
      size_t p = z.find(LINIA);
      if (p != std::string::npos && tekst.empty()) {
        size_t k = z.find('.', p);
        tekst = z.substr(p, k == std::string::npos ? std::string::npos : k - p + 1);
      }
      if (z.find("przerysowanie R-EKR-60") != std::string::npos) przer = true;
    }
    if (tekst.empty()) continue;
    l.teksty.push_back(tekst);
    l.przerysowan += przer;
  }
  return l;
}
static void wypiszProby(const char* nazwa, const std::vector<Wyb>& v) {
  printf("  %s: zapytania /otoczenie:", nazwa);
  for (auto& w : v)
    if (zadanZeSciezka(w, "/otoczenie") > 0) printf(" %s%02d:%02d", w.data.substr(8).c_str(), w.minuta / 60, w.minuta % 60);
  printf("\n");
}

// A6: nocą (01:00–05:00) panel źródła nie pyta; jedna nieudana próba ok. 05:00, potem
// źródło działa → bez linii (liczenie od ostatniej udanej dałoby linię `Brak odczytu CO2 od 00:xx.` o 05:00,
// bo od ostatniej udanej odpowiedzi minęło ponad 3 h).
// wymaganie: R-AW-41
static void testR_AW_41_jednaNieudanaRano() {
  ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, ep(2026, 11, 4, 4, 55), ep(2026, 11, 4, 5, 10));
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 9, 0), odpCisza, false, konfOto());
  wypiszProby("jedna nieudana rano", v);
  uint32_t udana = ostatniaUdana(v, awariaOd), nieudana = pierwszaProba(v, awariaOd);
  Linie l = linie(v);
  drukujLinie("jedna nieudana rano", l);
  SPRAWDZ(nieudana >= awariaOd && nieudana < powrotOd);  // próba w awarii była
  SPRAWDZ(udana > 0 && nieudana - udana >= OTO_CISZA_CO2_S);  // od ostatniej udanej ponad 3 h (noc bez pytań)
  SPRAWDZ(l.minuty.empty());
}

// A7: źródło nie odpowiada 04:50–06:50 (np. nocna kopia albo aktualizacja serwera)
// → bez linii; awaria 04:50–09:30 → linia dopiero 3 h po pierwszej nieudanej próbie (próba ok. 05:00, więc nie przy
// próbie o 08:00, która wypada kilka sekund przed progiem, tylko o 09:00), z godziną ostatniej udanej odpowiedzi sprzed
// nocy.
// wymaganie: R-AW-41
static void testR_AW_41_awariaPoranna() {
  {
    ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, ep(2026, 11, 4, 4, 50), ep(2026, 11, 4, 6, 50));
    PanelSym s;
    std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 9, 0), odpCisza, false, konfOto());
    wypiszProby("awaria 04:50-06:50", v);
    Linie l = linie(v);
    drukujLinie("awaria 04:50-06:50", l);
    SPRAWDZ(pierwszaProba(v, awariaOd) < powrotOd);
    SPRAWDZ(l.minuty.empty());
  }
  {
    ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, ep(2026, 11, 4, 4, 50), ep(2026, 11, 4, 9, 30));
    PanelSym s;
    std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 10, 30), odpCisza, false, konfOto());
    wypiszProby("awaria 04:50-09:30", v);
    uint32_t udana = ostatniaUdana(v, awariaOd), nieudana = pierwszaProba(v, awariaOd);
    Linie l = linie(v);
    drukujLinie("awaria 04:50-09:30", l);
    SPRAWDZ(udana > 0 && nieudana > udana);
    SPRAWDZ(!l.minuty.empty());
    // godzina ostatniej udanej odpowiedzi z logu panelu (`pobrano`; przy starcie zasilania odpowiedź przychodzi kilka
    // minut po wybudzeniu 00:05)
    std::string zLogu;
    for (auto& w : v)
      for (auto& z : w.zdarzenia) {
        size_t p = z.find("ostatnia udana ");
        if (p != std::string::npos && zLogu.empty()) zLogu = z.substr(p + 15, 5);
      }
    printf("  awaria 04:50-09:30: ostatnia udana z logu `%s`, pierwsza nieudana %s\n", zLogu.c_str(), hhmm(nieudana).c_str());
    if (!l.minuty.empty()) {
      SPRAWDZ(l.minuty.front() >= minutaLok(nieudana) + (int)(OTO_CISZA_CO2_S / 60));
      SPRAWDZ(!zLogu.empty() && minutaLok(udana) <= 60 && l.tekst == "Brak odczytu CO2 od " + zLogu + ".");
      SPRAWDZ(l.tekst != "Brak odczytu CO2 od " + hhmm(nieudana) + ".");
    }
  }
}

// A8: cisza przez dwie doby (od 4.11 08:00): linia 4.11, 5.11 tego samego epizodu bez linii; to samo z restartem
// (zanik zasilania) w nocy 5.11 o 02:00. Stan epizodu żyje w NVS (`e08c_*`), dyrygent czyta go przy każdym wybudzeniu.
// wymaganie: R-AW-41
static void testR_AW_41_dwieDoby() {
  ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, ep(2026, 11, 4, 8, 0));
  uint32_t polnoc = ep(2026, 11, 5, 0, 0);
  {
    PanelSym s;
    std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 14, 30), odpCisza, false, konfOto());
    Linie a = linie(v, 0, polnoc), b = linie(v, polnoc);
    drukujLinie("dwie doby, 4.11", a);
    drukujLinie("dwie doby, 5.11", b);
    SPRAWDZ(!a.minuty.empty());
    SPRAWDZ(v.back().ep >= ep(2026, 11, 5, 12, 0));  // symulacja doszła do 5.11 w dzień
    SPRAWDZ(b.minuty.empty());
  }
  {
    PanelSym s;
    std::vector<Wyb> v1 = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 2, 0), odpCisza, false, konfOto());
    PanelSym* q = poRestarcie(s);
    std::vector<Wyb> v2 = symuluj(*q, ep(2026, 11, 5, 2, 5), ep(2026, 11, 5, 14, 30), odpCisza, false, konfOto());
    Linie a = linie(v1), b = linie(v2);
    drukujLinie("dwie doby z restartem, 4.11", a);
    drukujLinie("dwie doby z restartem, 5.11", b);
    SPRAWDZ(!a.minuty.empty());
    SPRAWDZ(!v2.empty() && v2.back().ep >= ep(2026, 11, 5, 12, 0));
    SPRAWDZ(b.minuty.empty());
    delete q;
  }
}

// A9: zmiana adresu źródła (restart z innym OTO_URL) po linii o 11:00: stara cisza i jej epizod nie przechodzą
// na nowe źródło; nowa cisza liczy się od pierwszej nieudanej próby nowego adresu, więc do 14:30 bez linii.
// wymaganie: R-AW-41
static void testR_AW_41_zmianaAdresu() {
  ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, ep(2026, 11, 4, 8, 0));
  PanelSym s;
  std::vector<Wyb> v1 = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 12, 0), odpCisza, false, konfOto());
  SPRAWDZ(!linie(v1).minuty.empty());
  PanelSym* q = poRestarcie(s);
  KonfiguracjaPanelu k = konfOto();
  k.otoUrl = "http://192.168.1.61:8080/otoczenie";
  std::vector<Wyb> v2 = symuluj(*q, ep(2026, 11, 4, 12, 5), ep(2026, 11, 4, 14, 30), odpCisza, false, k);
  Linie l = linie(v2);
  drukujLinie("nowy adres od 12:05", l);
  SPRAWDZ(ileZadan(v2, "/otoczenie") > 0);
  SPRAWDZ(l.minuty.empty());
  delete q;
}

// A10: przerysowanie R-EKR-60 bez pobrania (linia z pamięci RTC, `dzienLinia` = 3) ma tę samą godzinę co
// pierwsze rysowanie linii z ciszy: ostatnia udana odpowiedź, nie `od` ostrzeżenia `co2_milczy`. Wariant bez slotu
// dziennego (jak scenariusz F w test/testy_plan_wybudzen.cpp): przerysowanie co 2 h bez pobrania.
// wymaganie: R-AW-41
static void testR_AW_41_przerysowanie() {
  // bez slotu dziennego panel pyta źródło o 07:00 i 11:00 (nieudane od 06:50), linia o 11:00, potem przerysowania co 2 h
  ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, ep(2026, 11, 4, 6, 50));
  PanelSym s;
  KonfiguracjaPanelu k = konfOto();
  k.bezSlotuDziennego = true;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 20, 30), odpCisza, false, k);
  wypiszProby("przerysowanie", v);
  uint32_t udana = ostatniaUdana(v, awariaOd);
  LinieWszystkie l = linieWszystkie(v);
  printf("  przerysowanie: wybudzeń z linią %zu, w tym przerysowań R-EKR-60 %d\n", l.teksty.size(), l.przerysowan);
  SPRAWDZ(l.przerysowan > 0);
  for (auto& x : l.teksty) {
    if (x != "Brak odczytu CO2 od " + hhmm(udana) + ".") printf("    inny tekst: `%s`\n", x.c_str());
    SPRAWDZ(x == "Brak odczytu CO2 od " + hhmm(udana) + ".");
  }
}

// A11 (decyzja właściciela projektu, 23.11.2026): świeżość CO2 w stopce 75 min (OTO_SWIEZOSC_SYPIALNI_S), bo w dzień
// panel czyta źródło raz na godzinę. Odczyt o 12:00 (blok `sypialnia` bez `ts`: czas odebrania), potem źródło nie
// odpowiada (nowego odczytu nie ma): rysowanie po przycisku o 12:40 → stopka `przy łóżku 21° · 45% · CO2 600` (przy
// 30 min CO2 znikało), o 13:20 → bez CO2 (80 min > 75 min). Stopka składana jak w dyrygencie (`d.przyLozku` =
// stopkaCzujnika(…, co2Teraz())); atrapa ekranu nie zapisuje tekstu stopki, więc poziom funkcji, nie doby dyrygenta.
// wymaganie: R-EKR-10
static void testP_657_swiezoscCo2WStopce() {
  Otoczenie o;
  wyczyscOtoczenie(o, true, 1);
  uint32_t t12 = ep(2026, 11, 4, 12, 0);
  SPRAWDZ(parsujOtoczenie(OTO_OK, t12, o) == StanOto::OK);
  nieudanaOtoczenia(o, -1, ep(2026, 11, 4, 13, 0));  // o 13:00 źródło nie odpowiada: zostaje odczyt z 12:00
  std::string s1240 = stopkaCzujnika(21.0f, 45.0f, co2Teraz(o, ep(2026, 11, 4, 12, 40)));
  std::string s1320 = stopkaCzujnika(21.0f, 45.0f, co2Teraz(o, ep(2026, 11, 4, 13, 20)));
  printf("  świeżość CO2: 12:40 `%s`, 13:20 `%s`\n", s1240.c_str(), s1320.c_str());
  SPRAWDZ(s1240.find("CO2 600") != std::string::npos);
  SPRAWDZ(s1320.find("CO2") == std::string::npos && s1320.find("przy łóżku") != std::string::npos);
  SPRAWDZ(co2Teraz(o, t12 + 75 * 60) == 600 && co2Teraz(o, t12 + 75 * 60 + 1) == -1);  // granica 75 min
  SPRAWDZ(OTO_SWIEZOSC_SYPIALNI_S == 75 * 60);
}

#ifndef CO2_CISZA_BEZ_POPRAWKI
// B1: ciszaPoProbie(), ciszaZrodlaTeraz() i probaNieudanaOtoczenia() na każdym stanie próby.
// wymaganie: R-AW-41
static void testR_AW_41_funkcje() {
  Otoczenie o;
  wyczyscOtoczenie(o, true, 1);
  uint32_t T = ep(2026, 11, 4, 8, 0);
  SPRAWDZ(ciszaPoProbie(o, 0, T) == 0);  // jeszcze bez odczytu: bez ciszy
  o.stan = (int8_t)StanOto::OK;
  o.pobrano = T - 1800;
  SPRAWDZ(ciszaPoProbie(o, 0, T) == 0 && ciszaPoProbie(o, T - 99, T) == 0);  // udana: koniec ciszy
  for (int kod : {-1, -11, OTO_KOD_CZAS, 401, 403, 404, 302, 503}) {
    Otoczenie x = o;
    nieudanaOtoczenia(x, kod, T);
    SPRAWDZ(probaNieudanaOtoczenia(x));
    SPRAWDZ(ciszaPoProbie(x, 0, T) == T);  // od pierwszej nieudanej próby, nie od ostatniej udanej
    SPRAWDZ(ciszaPoProbie(x, T - 5000, T) == T - 5000);  // dotychczasowy początek zostaje
    SPRAWDZ(!ciszaZrodlaTeraz(x, T, T + OTO_CISZA_CO2_S - 1));
    SPRAWDZ(ciszaZrodlaTeraz(x, T, T + OTO_CISZA_CO2_S));
  }
  for (const char* t : {"{\"wersja\":2}", "nie json", "[]"}) {
    Otoczenie x = o;
    parsujOtoczenie(t, T, x);
    SPRAWDZ(probaNieudanaOtoczenia(x) && ciszaPoProbie(x, 0, T) == T);
  }
  Otoczenie d = o;
  parsujOtoczenie(std::string(OTO_MAX_BAJTOW + 1, ' '), T, d);
  SPRAWDZ(probaNieudanaOtoczenia(d));
  // po restarcie (pobrano = 0): od pierwszej nieudanej próby; bez zegara 0
  Otoczenie r;
  wyczyscOtoczenie(r, true, 1);
  nieudanaOtoczenia(r, -1, T);
  SPRAWDZ(ciszaPoProbie(r, 0, T) == T && ciszaPoProbie(r, 0, 0) == 0);
  SPRAWDZ(!ciszaZrodlaTeraz(r, T, 0));  // bez zegara
  SPRAWDZ(!ciszaZrodlaTeraz(r, T + 10, T));  // początek z przyszłości
  SPRAWDZ(!ciszaZrodlaTeraz(r, 0, T + OTO_CISZA_CO2_S));
  // adres poza siecią i wyłączone: bez próby, bez ciszy i bez zmiany początku
  Otoczenie p = o;
  p.stan = (int8_t)StanOto::POZA_SIECIA;
  SPRAWDZ(!probaNieudanaOtoczenia(p) && ciszaPoProbie(p, 0, T) == 0 && ciszaPoProbie(p, 77, T) == 77);
  SPRAWDZ(!ciszaZrodlaTeraz(p, 1, T + OTO_CISZA_CO2_S));
  Otoczenie w;
  wyczyscOtoczenie(w, false);
  SPRAWDZ(!probaNieudanaOtoczenia(w) && ciszaPoProbie(w, 0, T) == 0);
  Otoczenie bez = o;
  bez.magia = 0;
  bez.stan = (int8_t)StanOto::BRAK_ODPOWIEDZI;
  SPRAWDZ(!probaNieudanaOtoczenia(bez) && ciszaPoProbie(bez, 5, T) == 5);
  // `{czas}` linii: ostatnia udana, bez niej (albo późniejsza od początku) początek ciszy
  SPRAWDZ(czasLiniiCiszy(T, T - 1800) == T - 1800);
  SPRAWDZ(czasLiniiCiszy(T, 0) == T);
  SPRAWDZ(czasLiniiCiszy(T, T + 5) == T);
}

// B2: liniaDnia() z ciszą: raz na epizod przez kolejne dni, restart (stan z NVS), koniec po udanej odpowiedzi, bez
// linii na S5, przy zaproszeniu, bez ekranów CO2; linia z `co2_milczy` ma pierwszeństwo i swój stan.
// wymaganie: R-AW-41
static void testR_AW_41_liniaDnia() {
  Otoczenie o;
  wyczyscOtoczenie(o, true, 1);
  uint32_t T0 = ep(2026, 11, 4, 7, 30);  // ostatnia udana
  o.stan = (int8_t)StanOto::OK;
  o.pobrano = T0;
  nieudanaOtoczenia(o, -1, T0 + 1800);
  uint32_t cisza = ciszaPoProbie(o, 0, T0 + 1800);
  SPRAWDZ(cisza == T0 + 1800);  // pierwsza nieudana próba
  StanE08 e, c;
  uint32_t t1 = cisza + OTO_CISZA_CO2_S - 60;
  LiniaDnia l = liniaDnia(false, "S2A", o, t1, "2026-11-04", e, true, &c, cisza, T0);
  SPRAWDZ(!l.e08 && !l.zapiszCisza);  // przed 3 h od pierwszej nieudanej (3 h 29 min od ostatniej udanej)
  uint32_t t2 = cisza + OTO_CISZA_CO2_S;
  l = liniaDnia(false, "S2A", o, t2, "2026-11-04", e, true, &c, cisza, T0);
  SPRAWDZ(l.e08 && l.e08Cisza && l.e08Od == T0 && l.zapiszCisza && !l.zapiszE08);  // `{czas}` = ostatnia udana
  SPRAWDZ(c.od == cisza);  // klucz epizodu: początek ciszy
  l = liniaDnia(false, "S2A", o, t2 + 3600, "2026-11-04", e, true, &c, cisza, T0);
  SPRAWDZ(l.e08 && !l.zapiszCisza);  // ten sam dzień: dalej, bez zapisu
  SPRAWDZ(liniaDnia(false, "S2A", o, t2, "2026-11-04", e, true, &c, cisza).e08Od == cisza);  // bez ostatniej udanej
  StanE08 poRestarcie = c;  // stan z NVS po restarcie
  l = liniaDnia(false, "S2A", o, t2 + 86400, "2026-11-05", e, true, &poRestarcie, cisza, T0);
  SPRAWDZ(!l.e08 && !l.zapiszCisza);  // następny dzień tego samego epizodu: bez linii
  SPRAWDZ(!liniaDnia(false, "S5", o, t2, "2026-11-04", e, true, &c, cisza).e08);
  SPRAWDZ(!liniaDnia(true, "S2A", o, t2, "2026-11-04", e, true, &c, cisza).e08);
  SPRAWDZ(!liniaDnia(false, "S2A", o, t2, "2026-11-04", e, false, &c, cisza).e08);
  StanE08 c0;
  SPRAWDZ(!liniaDnia(false, "S2A", o, t2, "2026-11-04", e, true, nullptr, cisza).e08);  // bez stanu: jak dotąd
  SPRAWDZ(!liniaDnia(false, "S2A", o, t2, "2026-11-04", e, true, &c0, 0).e08);  // bez początku ciszy
  // udana odpowiedź 5.11 12:00: koniec epizodu (zapis), linii nie ma
  Otoczenie u = o;
  uint32_t t3 = t2 + 86400 + 3600;
  parsujOtoczenie("{\"wersja\":1,\"ostrzezenia\":[]}", t3, u);
  l = liniaDnia(false, "S2A", u, t3, "2026-11-05", e, true, &poRestarcie, ciszaPoProbie(u, cisza, t3));
  SPRAWDZ(!l.e08 && l.zapiszCisza && poRestarcie.koniec);
  // nowa cisza od t3: nowy epizod z nową godziną
  nieudanaOtoczenia(u, 401, t3 + 600);
  uint32_t cisza2 = ciszaPoProbie(u, 0, t3 + 600);
  SPRAWDZ(cisza2 == t3 + 600);
  SPRAWDZ(!liniaDnia(false, "S2A", u, t3 + OTO_CISZA_CO2_S, "2026-11-05", e, true, &poRestarcie, cisza2, t3).e08);
  l = liniaDnia(false, "S2A", u, t3 + 600 + OTO_CISZA_CO2_S, "2026-11-05", e, true, &poRestarcie, cisza2, t3);
  SPRAWDZ(l.e08 && l.e08Cisza && l.e08Od == t3 && l.zapiszCisza);
  // świeże ostrzeżenie `co2_milczy`: linia z ostrzeżenia, nie z ciszy (cisza wymaga nieudanej ostatniej próby)
  Otoczenie m;
  wyczyscOtoczenie(m, true, 1);
  parsujOtoczenie("{\"wersja\":1,\"ostrzezenia\":[{\"kod\":\"co2_milczy\",\"od\":\"2026-11-04T05:00:00+01:00\"}]}", t2, m);
  StanE08 e2, c2;
  l = liniaDnia(false, "S2A", m, t2 + 60, "2026-11-04", e2, true, &c2, ciszaPoProbie(m, T0, t2));
  SPRAWDZ(l.e08 && !l.e08Cisza && l.e08Od == ep(2026, 11, 4, 5, 0));
}
#endif

#ifdef CO2_CISZA_BEZ_MAIN  // plik dołącza inny test (testy_dobry_start_cisza_otoczenia.cpp): własny main() tamtego
int main_testy_co2_cisza() {
#else
int main() {
#endif
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testR_AW_41_episodPo3h(Awaria::BRAK_ODPOWIEDZI, "HTTP -1", "brak odpowiedzi");
  testR_AW_41_episodPo3h(Awaria::CZAS_400, "HTTP -400", "brak odpowiedzi");
  testR_AW_41_episodPo3h(Awaria::ODMOWA_401, "HTTP 401", "odrzucony klucz");
  testR_AW_41_episodPo3h(Awaria::ZLA_WERSJA, "wersja 2", "wersja kontraktu 2");
  testR_AW_41_bezZrodlaIBezCo2();
  testR_AW_41_koniecINowyEpizod();
  testR_AW_41_restart();
  testR_AW_27_regresja();
  testR_AW_41_jednaNieudanaRano();
  testR_AW_41_awariaPoranna();
  testR_AW_41_dwieDoby();
  testR_AW_41_zmianaAdresu();
  testR_AW_41_przerysowanie();
  testP_657_swiezoscCo2WStopce();
#ifndef CO2_CISZA_BEZ_POPRAWKI
  testR_AW_41_funkcje();
  testR_AW_41_liniaDnia();
#endif
  printf("testy_co2_cisza: %d OK, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
