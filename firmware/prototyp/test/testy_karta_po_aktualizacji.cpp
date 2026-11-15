// SPDX-License-Identifier: GPL-3.0-or-later
// Informacja po aktualizacji samodzielnej (R-KONF-61,
// decyzja 11 właściciela projektu z 14.11.2026 „karta”): po instalacji z automatu (R-KONF-45) panel pokazuje jednorazowo U-07
// „Zaktualizowano do {X}” z listą „Co nowego” (albo wariant „nie udało się” po powrocie do poprzedniej wersji,
// R-NF-35) przy pierwszym ekranie slotu dziennego bez sprawy (decyzja 12 z 15.11.2026: rano zawsze wynik nocy); znika po
// dowolnym przycisku albo z końcem doby, w której stanęła.
// Testy z kolumny „Weryfikacja” R-KONF-61: teksty i zapis w NVS (rdzeń), scenariusze z dyrygentem na atrapach
// (automat → restart → samosprawdzenie → informacja raz; przyciski; koniec doby; restart; pierwszeństwo ekranów
// ze sprawą; slot nocny, terapia; nieudana instalacja; ręczna bez zmian; bez listy nowości; zapisy NVS i odświeżenia).
// Uruchomienie: tools/prototyp/testy.sh (program testy_karta_po_aktualizacji). Dane syntetyczne.
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "aktualizacja.h"
#include "dyrygent.h"
#include "ekrany_konfiguracji.h"
#include "magazyn.h"
#include "nawigacja.h"
#include "sekrety.h"
#include "sprawy.h"
#include "start.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

using aktualizacja::Informacja;
using aktualizacja::RodzajInformacji;

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

// R-KONF-61 (decyzja 12): współrzędne miasta, żeby slot dzienny miał ekran bez sprawy (S-K-37 bez sieci)
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true, false, nullptr, 50.2597, 19.0217};
// bez miasta i obrazków: slot dzienny bez ekranu (ekran z rana zostaje, R-KON-54)
static const KonfiguracjaPanelu KONF_BEZ_MIASTA = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const char* STATUS_TERAPIA =
    "{\"version\":\"v2.2.1\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"THERAPY\","
    "\"rop\":1,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"blocked\"}";
static const char* const TOKEN = "github_pat_ZNACZNIKTOKENU_0123456789abcdef";
static const uint8_t B_LEWY = 1u << (int)Przycisk::LEWY, B_SRODEK = 1u << (int)Przycisk::SRODEK,
                     B_PRAWY = 1u << (int)Przycisk::PRAWY;
static const char* const KLUCZ = "aktual/u07_inf";

static std::string raport(long day, const char* ahi = "1.4") {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":" + std::string(ahi) + ",\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,"
         "\"ua\":0,\"h\":4,\"rera\":2},\"channels\":[{\"id\":\"Press\",\"label\":\"C\",\"unit\":\"cmH2O\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":6.2,\"mean\":10.1,\"median\":9.8,\"p95\":13.6,\"max\":16.0,"
         "\"source\":\"edf\"},{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
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
static std::string obrazTestowy(size_t n = 20000) {
  std::string b(n, '\0');
  for (size_t i = 0; i < n; i++) b[i] = (char)(i * 31 + 7);
  return b;
}
// AirBridge z nocami do doby `d` (poranek d+1); `nocy` nocy wstecz
static void airbridge(Panel& p, long d, const char* status = STATUS_SPOCZYNEK, int nocy = 3, const char* ahi = "1.4") {
  p.siec.odpowiedzi.clear();
  p.siec.dodaj("/api/status", 200, status);
  p.siec.dodaj(sciezkaListy(), 200, lista(d - nocy + 1, d));
  for (long x = d - nocy + 1; x <= d; x++) p.siec.dodaj(sciezkaNocy(x), 200, raport(x, ahi));
}
static void zegarem(Panel& p, uint32_t t) {
  p.zegar.epoch = t;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  p.przyciski.skrypt.clear();
  p.siec.polaczonaTeraz = false;
  p.d.z.clear();
  p.port.log.clear();
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
}
static void przyciskiem(Panel& p, uint32_t t, uint8_t bit, uint32_t ms) {
  Ekran e = (Ekran)p.start.stan.ekran;  // ekran z RTC sprzed snu
  uint32_t od = p.start.stan.ekranOd;
  zegarem(p, t);
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = {{0, bit}, {ms, 0}};
  p.start.stan.ekran = (uint8_t)e;
  p.start.stan.ekranOd = od;
}
static void restart(Panel& p, uint32_t t, const char* wersja, StanWersji sw) {
  zegarem(p, t);
  p.start.przyczynaStartu = PrzyczynaStartu::PROGRAMOWY;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  p.start.napisWersji = wersja;
  p.start.stanW = sw;
  p.start.stan = StanRtc();
}
static bool wLogu(const Panel& p, const std::string& f) {
  for (auto& l : p.port.log)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static bool wTekstach(const TekstyWybudzenia& t, const std::string& f) {
  for (auto& l : t.linieTestu)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static TekstyWybudzenia wybudz(Panel& p, const KonfiguracjaPanelu& k = KONF) {
  Dyrygent d(p.adaptery(), k);
  d.wybudzenie();
  if (getenv("LOG"))
    for (auto& l : p.port.log) printf("    | %s\n", l.c_str());
  return d.teksty();
}
static std::string zapisNvs(const Panel& p) {
  auto i = p.nvs.teksty.find(KLUCZ);
  return i == p.nvs.teksty.end() ? "" : i->second;
}
static Informacja informacjaNvs(const Panel& p) { return aktualizacja::odczytInformacji(zapisNvs(p)); }
static bool informacjaNaEkranie(const TekstyWybudzenia& t) {
  return wTekstach(t, "U-07 · Zaktualizowano do ") || wTekstach(t, "U-07 · Nowa wersja nie uruchomiła się");
}
// informacja ustawiona wprost w NVS (dla wersji panelu z atrapy 1.0.0), do testów pierwszeństwa
static void uzbroj(Panel& p, RodzajInformacji r = RodzajInformacji::UDANA, const std::string& wlasna = "1.0.0",
                   const std::string& nowa = "1.0.0") {
  Informacja i;
  i.rodzaj = r;
  i.wlasna = wlasna;
  i.nowa = nowa;
  p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, aktualizacja::zapisInformacji(i));
}
static Panel* panelAutomatu(const std::vector<std::string>& nowosci = {}) {
  Panel* p = new Panel();
  sekrety::zapiszKluczWydan(p->nvs, TOKEN);
  p->wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany(), PLYTKA_WYDANIA, nowosci);
  p->pliki.pliki["/konfiguracja.txt"] = "aktualizacja_auto=tak\n";
  return p;
}
static long d0() { return nrDnia("2026-11-09"); }

// ---------- rdzeń: teksty i zapis ----------
static void testR_KONF_61_tresc() {
  aktualizacja::DaneU07 d;
  d.stan = aktualizacja::StanU07::PO_UDANEJ;
  d.wlasna = d.nowa = "1.0.1";
  d.coNowego = {"Portret nocy: łagodniejsze piki", "Ustawienia w nowym układzie"};
  d.samodzielnie = true;
  aktualizacja::TresciU07 t = aktualizacja::trescU07(d);
  SPRAWDZ(t.linie.size() == 2 && t.linie[0] == "Zaktualizowano do 1.0.1." && t.linie[1] == "Panel zaktualizował się sam.");
  SPRAWDZ(t.glowne == "Zaktualizowano do 1.0.1." && t.opis.size() == 1 && t.opis[0] == "Panel zaktualizował się sam.");
  SPRAWDZ(t.coNowego == d.coNowego);
  SPRAWDZ(t.stopka == (std::vector<std::string>{"▶ dalej", "↻ ekran dnia"}));  // B31, ◀ ▶ ↻
  SPRAWDZ(t.stanBelki == "gotowe" && t.obrazek == aktualizacja::ObrazekU07::UDANA && t.pas.empty());
  SPRAWDZ(t.kroki);
  for (int k = 0; k < aktualizacja::KROKI_U07; k++) SPRAWDZ(t.krok[k] == aktualizacja::KrokU07::ZROBIONY);
  d.coNowego.clear();  // wydanie bez pola `nowosci`: sama informacja o wersji
  SPRAWDZ(aktualizacja::trescU07(d).coNowego.empty());
  // ręczna (dotychczasowa) bez zmian
  d.samodzielnie = false;
  aktualizacja::TresciU07 r = aktualizacja::trescU07(d);
  SPRAWDZ(r.linie == (std::vector<std::string>{"Zainstalowano wersję 1.0.1."}));
  SPRAWDZ(r.stopka == (std::vector<std::string>{"◀ ustawienia", "↻ ekran dnia"}));
  // nieudana: istniejące teksty R-NF-35, krok `Ponowny start` z krzyżykiem
  aktualizacja::DaneU07 n;
  n.stan = aktualizacja::StanU07::PO_NIEUDANEJ;
  n.powod = aktualizacja::Powod::NIE_WSTALA;
  n.wlasna = "1.0.0";
  n.nowa = "1.0.1";
  n.samodzielnie = true;
  aktualizacja::TresciU07 u = aktualizacja::trescU07(n);
  // decyzja 12 (zgoda właściciela projektu 15.11.2026 00:28): zdanie o próbie samodzielnej przed powodem
  SPRAWDZ(u.linie == (std::vector<std::string>{"Aktualizacja się nie udała. Zostaje wersja 1.0.0.",
                                               "Panel próbował zaktualizować się sam.",
                                               "Nowa wersja nie uruchomiła się poprawnie. Panel wrócił do 1.0.0."}));
  SPRAWDZ(u.glowne == u.linie[0] && u.opis.size() == 2 && u.opis[0] == "Panel próbował zaktualizować się sam.");
  SPRAWDZ(u.stopka == (std::vector<std::string>{"▶ dalej", "↻ ekran dnia"}));  // bez „spróbuj ponownie”
  SPRAWDZ(u.stanBelki == "nie udało się" && u.krok[3] == aktualizacja::KrokU07::NIEUDANY && u.coNowego.empty());
  n.samodzielnie = false;
  SPRAWDZ(aktualizacja::trescU07(n).stopka ==
          (std::vector<std::string>{"◀ ustawienia", "▶ spróbuj ponownie", "↻ ekran dnia"}));
  SPRAWDZ(aktualizacja::trescU07(n).linie.size() == 2);  // ręczna bez zdania o próbie samodzielnej
  for (auto& l : aktualizacja::trescU07(n).linie) SPRAWDZ(l != aktualizacja::PROBOWAL_SAM);
}

// układ: najdłuższe wersje i 3 linie `Co nowego` po 40 najszerszych znaków bez wejścia na stopkę (434), rysunek
// i prawy margines (jak R-KONF-57); stopka z kolejnością ◀ ▶ ↻ (tu ▶ i ↻)
static int czarneW(const PlotnoAtrapy& p, int x0, int y0, int x1, int y1) {
  int n = 0;
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) n += (p.b[(size_t)(y * 100 + x / 8)] >> (7 - (x & 7))) & 1;
  return n;
}
static void testR_KONF_61_uklad() {
  aktualizacja::DaneU07 d;
  d.samodzielnie = true;
  d.wlasna = "12.12.12-rc.12";
  d.nowa = "12.12.13-rc.12";
  d.coNowego = {std::string(40, 'W'), std::string(40, 'W'), std::string(40, 'W')};
  d.powod = aktualizacja::Powod::NIE_WSTALA;
  for (aktualizacja::StanU07 st : {aktualizacja::StanU07::PO_UDANEJ, aktualizacja::StanU07::PO_NIEUDANEJ}) {
    d.stan = st;
    aktualizacja::TresciU07 t = aktualizacja::trescU07(d);
    PlotnoAtrapy p;
    rysujAktualizacje(p, t);
    SPRAWDZ(t.pas.empty());
    SPRAWDZ(czarneW(p, U07_X, 434, 800, 434) == 0);
    int poza = czarneW(p, U07_X, 420, 800, 434);  // treść kończy się nad kreską stopki
    if (poza) printf("  R-KONF-61 stan %d: treść przy stopce (%d px)\n", (int)st, poza);
    SPRAWDZ(poza == 0);
    SPRAWDZ(czarneW(p, U07_OBRAZEK_X + U07_OBRAZEK_W + 1, 44, U07_X, 433) == 0);
    SPRAWDZ(czarneW(p, 773, 44, 800, 433) == 0);
    SPRAWDZ(czarneW(p, 0, 437, 800, 480) > 0);  // stopka narysowana
  }
}

static void testR_KONF_61_zapisNvs() {
  Informacja i;
  i.rodzaj = RodzajInformacji::UDANA;
  i.wlasna = "1.0.1";
  i.nowa = "1.0.1";
  std::string z = aktualizacja::zapisInformacji(i);
  Informacja o = aktualizacja::odczytInformacji(z);
  SPRAWDZ(o.rodzaj == RodzajInformacji::UDANA && o.wlasna == "1.0.1" && o.nowa == "1.0.1" && o.dzien.empty());
  i.dzien = "2026-11-10";
  o = aktualizacja::odczytInformacji(aktualizacja::zapisInformacji(i));
  SPRAWDZ(o.dzien == "2026-11-10");
  i.rodzaj = RodzajInformacji::NIEUDANA;
  SPRAWDZ(aktualizacja::odczytInformacji(aktualizacja::zapisInformacji(i)).rodzaj == RodzajInformacji::NIEUDANA);
  // brak informacji i zapis uszkodzony = brak (nic nie pokazuje)
  SPRAWDZ(aktualizacja::zapisInformacji(Informacja()).empty());
  for (const char* zly : {"", "X\n1.0.1\n1.0.1\n", "U\n\n1.0.1\n", "U\n1.0.1\n\n", "U\n1.0.1\n1.0.1\n2026-1-10",
                          "U\n1.0.1", "U\n1.0.1\n1.0.1\n2026-11-10\nnadmiar", "U\n1.0.1\n1.0.1\nabcd-ef-gh"})
    SPRAWDZ(aktualizacja::odczytInformacji(zly).rodzaj == RodzajInformacji::BRAK);
  SPRAWDZ(aktualizacja::odczytInformacji("U\n" + std::string(40, '1') + "\n1.0.1\n").rodzaj == RodzajInformacji::BRAK);
  // czy pokazać: wersja panelu taka jak przy zapisie; czeka (bez doby) albo pokazana dziś
  Informacja c;
  c.rodzaj = RodzajInformacji::UDANA;
  c.wlasna = c.nowa = "1.0.1";
  SPRAWDZ(aktualizacja::informacjaAktualna(c, "1.0.1", "2026-11-10"));
  SPRAWDZ(!aktualizacja::informacjaAktualna(c, "1.0.2", "2026-11-10"));  // inna wersja (np. wgranie kablem)
  c.dzien = "2026-11-10";
  SPRAWDZ(aktualizacja::informacjaAktualna(c, "1.0.1", "2026-11-10"));
  SPRAWDZ(!aktualizacja::informacjaAktualna(c, "1.0.1", "2026-11-11"));  // koniec doby pokazania
  SPRAWDZ(!aktualizacja::informacjaAktualna(Informacja(), "1.0.1", "2026-11-10"));
}

// ---------- dyrygent: automat → restart → samosprawdzenie → informacja raz ----------
// Pierwszy ekran dnia po potwierdzeniu wersji pokazuje U-07 „Zaktualizowano do 1.0.1” z „Co nowego”; ten sam dzień
// bez przycisku: informacja stoi (bez zapisu NVS i bez odświeżenia e-ink); ◀ → ekran dnia; potem nie wraca.
static void testR_KONF_61_poAutomacie() {
  Panel* p = panelAutomatu({"Portret nocy: łagodniejsze piki", "Ustawienia w nowym układzie"});
  airbridge(*p, d0());
  zegarem(*p, ep(2026, 11, 10, 9, 0));
  wybudz(*p);
  SPRAWDZ(p->wydania.slotStartowyZmieniony && p->wydania.restartow == 1);
  SPRAWDZ(zapisNvs(*p).empty());  // przed restartem nic
  // nowa wersja: samosprawdzenie po pierwszym rysowaniu; informacja zapisana dopiero przy wersji dobrej
  restart(*p, ep(2026, 11, 10, 9, 5), "1.0.1+def5678", StanWersji::DO_SPRAWDZENIA);
  airbridge(*p, d0());
  TekstyWybudzenia t = wybudz(*p);
  SPRAWDZ(p->start.oznaczona && p->start.stan.ekran == (uint8_t)Ekran::DZIEN);
  SPRAWDZ(!informacjaNaEkranie(t));  // pierwsze rysowanie przed potwierdzeniem: zwykły ekran dnia
  Informacja i = informacjaNvs(*p);
  SPRAWDZ(i.rodzaj == RodzajInformacji::UDANA && i.wlasna == "1.0.1" && i.nowa == "1.0.1" && i.dzien.empty());
  SPRAWDZ(p->d.ile(std::string("nvs.zapis ") + KLUCZ) == 1);
  // następne rysowanie ekranu dnia (wybudzenie zegarem w slocie dziennym): informacja zamiast ekranu dnia
  p->start.napisWersji = "1.0.1+def5678";
  p->start.stanW = StanWersji::DOBRA;
  airbridge(*p, d0());
  zegarem(*p, ep(2026, 11, 10, 11, 0));
  int odsw = p->ekran.odswiezen;
  t = wybudz(*p);
  SPRAWDZ(wTekstach(t, "U-07 · Zaktualizowano do 1.0.1."));
  SPRAWDZ(wTekstach(t, "U-07 · Panel zaktualizował się sam."));
  SPRAWDZ(wTekstach(t, "U-07 · Co nowego: Portret nocy: łagodniejsze piki"));
  SPRAWDZ(wTekstach(t, "U-07 · Co nowego: Ustawienia w nowym układzie"));
  SPRAWDZ(p->start.stan.ekran == (uint8_t)Ekran::DZIEN);  // nawigacja jak na ekranie dnia
  SPRAWDZ(p->ekran.odswiezen == odsw + 1);                // w klatce, która i tak się rysuje
  SPRAWDZ(informacjaNvs(*p).dzien == "2026-11-10");
  SPRAWDZ(p->d.ile(std::string("nvs.zapis ") + KLUCZ) == 1);
  SPRAWDZ(wLogu(*p, "informacja po aktualizacji samodzielnej"));
  // ta sama doba, bez przycisku: informacja stoi, bez zapisu i bez odświeżenia (klatka taka sama)
  airbridge(*p, d0());
  zegarem(*p, ep(2026, 11, 10, 13, 0));
  odsw = p->ekran.odswiezen;
  t = wybudz(*p);
  SPRAWDZ(informacjaNaEkranie(t));
  SPRAWDZ(p->d.ile(std::string("nvs.zapis ") + KLUCZ) == 0);
  SPRAWDZ(p->ekran.odswiezen == odsw);
  // ◀: informacja znika, ekran dnia (bez innej akcji), zapis „zamknięta”
  przyciskiem(*p, ep(2026, 11, 10, 13, 10), B_LEWY, 200);
  t = wybudz(*p);
  SPRAWDZ(!informacjaNaEkranie(t));
  SPRAWDZ(p->start.stan.ekran == (uint8_t)Ekran::DZIEN);
  SPRAWDZ(zapisNvs(*p).empty() && p->d.ile(std::string("nvs.zapis ") + KLUCZ) == 1);
  // nie wraca: kolejne wybudzenie i restart
  airbridge(*p, d0());
  zegarem(*p, ep(2026, 11, 10, 15, 0));
  SPRAWDZ(!informacjaNaEkranie(wybudz(*p)));
  restart(*p, ep(2026, 11, 10, 15, 30), "1.0.1+def5678", StanWersji::DOBRA);
  airbridge(*p, d0());
  SPRAWDZ(!informacjaNaEkranie(wybudz(*p)));
  delete p;
}

// ▶ na informacji: to, co na ekranie dnia (pierwsza strona kokpitu albo strona Portretu), i informacja znika;
// ↻: ekran dnia; przytrzymanie: tylko zamyka (bez zapisu czegokolwiek z ekranu dnia pod spodem)
static void testR_KONF_61_przyciski() {
  for (int wariant = 0; wariant < 3; wariant++) {
    Panel p;
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 9, 0));
    wybudz(p);  // noce w historii
    uzbroj(p);
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 11, 0));
    SPRAWDZ(informacjaNaEkranie(wybudz(p)));
    uint8_t bit = wariant == 0 ? B_PRAWY : wariant == 1 ? B_SRODEK : B_LEWY;
    przyciskiem(p, ep(2026, 11, 10, 11, 5), bit, wariant == 2 ? 2500 : 200);
    TekstyWybudzenia t = wybudz(p);
    SPRAWDZ(!informacjaNaEkranie(t));
    SPRAWDZ(zapisNvs(p).empty());
    Ekran e = (Ekran)p.start.stan.ekran;
    if (wariant == 0) SPRAWDZ(e != Ekran::DZIEN && e != Ekran::AKTUALIZACJA);  // ▶: dalej jak z ekranu dnia
    else SPRAWDZ(e == Ekran::DZIEN);
    if (wariant == 2) SPRAWDZ(!wLogu(p, "sprzęt: wymiana"));
  }
}

// z końcem doby, w której stanęła, informacja znika bez przycisku (bez zapisu NVS)
static void testR_KONF_61_koniecDoby() {
  Panel p;
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 9, 0));
  wybudz(p);
  uzbroj(p);
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 11, 0));
  SPRAWDZ(informacjaNaEkranie(wybudz(p)));
  // 23:45 slot nocny: S9, nie informacja
  zegarem(p, ep(2026, 11, 10, 23, 45));
  SPRAWDZ(!informacjaNaEkranie(wybudz(p)));
  // następny dzień: nie wraca; pierwsze rysowanie ekranu dnia kasuje wpis pokazany w innej dobie (1 zapis),
  // potem bez zapisu
  for (int h : {9, 12, 15}) {
    airbridge(p, d0() + 1);
    zegarem(p, ep(2026, 11, 11, h, 0));
    SPRAWDZ(!informacjaNaEkranie(wybudz(p)));
    SPRAWDZ(p.d.ile(std::string("nvs.zapis ") + KLUCZ) == (h == 9 ? 1 : 0));
    SPRAWDZ(zapisNvs(p).empty());
  }
  restart(p, ep(2026, 11, 11, 16, 0), WERSJA_ATRAPY, StanWersji::DOBRA);
  airbridge(p, d0() + 1);
  SPRAWDZ(!informacjaNaEkranie(wybudz(p)));
}

// pierwszeństwo: brak danych, terapia, slot nocny, sprawa A/B (L-03): informacja czeka i staje później
static void testR_KONF_61_pierwszenstwo() {
  {  // brak danych (AirBridge nie odpowiada, bez historii): S5, informacja czeka
    Panel p;
    uzbroj(p);
    p.siec.odpowiedzi.clear();
    zegarem(p, ep(2026, 11, 10, 9, 0));
    TekstyWybudzenia t = wybudz(p);
    SPRAWDZ(!informacjaNaEkranie(t));
    SPRAWDZ(informacjaNvs(p).rodzaj == RodzajInformacji::UDANA && informacjaNvs(p).dzien.empty());
    airbridge(p, d0());  // dane przyszły o 11:00: rano wynik nocy, informacja czeka na slot dzienny (decyzja 12)
    zegarem(p, ep(2026, 11, 10, 11, 0));
    SPRAWDZ(!informacjaNaEkranie(wybudz(p)));
    airbridge(p, d0());  // slot dzienny (60 min po imporcie nocy): pierwszy ekran slotu bez sprawy
    zegarem(p, ep(2026, 11, 10, 13, 0));
    SPRAWDZ(informacjaNaEkranie(wybudz(p)));
  }
  {  // trwa terapia (`rop` = 1): nie w trakcie terapii
    Panel p;
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 9, 0));
    wybudz(p);
    uzbroj(p);
    airbridge(p, d0(), STATUS_TERAPIA);
    zegarem(p, ep(2026, 11, 10, 14, 0));
    SPRAWDZ(!informacjaNaEkranie(wybudz(p)));
    SPRAWDZ(informacjaNvs(p).dzien.empty());
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 16, 0));
    SPRAWDZ(informacjaNaEkranie(wybudz(p)));
  }
  {  // slot nocny: bez informacji i bez zapisu; rano po nocy czeka dalej
    Panel p;
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 9, 0));
    wybudz(p);
    uzbroj(p);
    zegarem(p, ep(2026, 11, 10, 23, 40));
    SPRAWDZ(!informacjaNaEkranie(wybudz(p)));
    zegarem(p, ep(2026, 11, 11, 2, 0));
    SPRAWDZ(!informacjaNaEkranie(wybudz(p)));
    SPRAWDZ(informacjaNvs(p).dzien.empty());
  }
  {  // sprawa B (L-03) na ekranie dnia: informacja nie zasłania
    sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
    Panel p;
    const int NOCY = 15;
    long ost = nrDnia("2026-11-04") - 1;
    bool jest = false;
    for (int h = 0; h < 12 && !jest; h++) {
      airbridge(p, ost, STATUS_SPOCZYNEK, NOCY, "12.0");
      restart(p, ep(2026, 11, 4, 7, 0) + (uint32_t)(h * 1800), WERSJA_ATRAPY, StanWersji::DOBRA);
      p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
      wybudz(p);
      jest = wLogu(p, "ekran dnia: wzorzec L-03");
    }
    SPRAWDZ(jest);
    uzbroj(p);
    airbridge(p, ost, STATUS_SPOCZYNEK, NOCY, "12.0");
    zegarem(p, ep(2026, 11, 4, 13, 0));
    TekstyWybudzenia t = wybudz(p);
    SPRAWDZ(wLogu(p, "ekran dnia: wzorzec L-03") && !informacjaNaEkranie(t));
    SPRAWDZ(informacjaNvs(p).dzien.empty());
  }
}

// decyzja 12 (15.11.2026): rano zawsze wynik nocy. Dziesięć poranków pod rząd z informacją uzbrojoną przed
// 05:00, wybudzenia co godzinę 05:00–13:00 (historia 20 nocy, więc są dowody spokoju S2a/S2b, przerywnik S2d i ekran
// poranka S-K-27; noc przychodzi o 07:00, slot dzienny zaczyna się 60 min po imporcie): każde wybudzenie przed startem slotu
// dziennego rysuje ekran dnia z wynikiem nocy bez przycisku, nigdy informacji; informacja dopiero na ekranie slotu
// (pierwszego dnia; potem wpis już pokazany, więc każdego ranka uzbrojony od nowa). Portret i drugi ekran poranka:
// test/testy_karta_po_aktualizacji_portret.cpp. ▶ na informacji: pierwsza strona kokpitu.
static void testR_KONF_61_spokojnyPoranek() {
  Panel p;
  for (int h = 0; h < 8; h++) {  // historia partiami (po 4 doby na wybudzenie przy starcie zasilania)
    airbridge(p, d0(), STATUS_SPOCZYNEK, 20);
    restart(p, ep(2026, 11, 10, 12, 0) + (uint32_t)(h * 1800), WERSJA_ATRAPY, StanWersji::DOBRA);
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    wybudz(p);
  }
  int poranki = 0, porankiZWynikiem = 0, wybudzenRano = 0, ranoZInformacja = 0, przerywnikow = 0, dowodow = 0;
  int dniZInformacjaWSlocie = 0;
  for (int dzien = 0; dzien < 10; dzien++) {
    uzbroj(p);  // przed porankiem: czeka (bez doby)
    bool wynik = false, wSlocie = false;
    poranki++;
    for (int m = 5 * 60; m <= 13 * 60; m += 60) {
      // noc dnia przychodzi o 07:00 (wcześniej AirBridge ma tylko poprzednią), więc slot od 08:00
      if (m < 7 * 60) airbridge(p, d0() + dzien, STATUS_SPOCZYNEK, 20 + dzien);
      else airbridge(p, d0() + 1 + dzien, STATUS_SPOCZYNEK, 21 + dzien);
      zegarem(p, ep(2026, 11, 11 + dzien, m / 60, m % 60));
      TekstyWybudzenia t = wybudz(p);
      bool slot = wLogu(p, "slot dzienny: S-K-");
      if (!slot) {
        wybudzenRano++;
        if (informacjaNaEkranie(t)) ranoZInformacja++;
        // ekran poranka z wynikiem nocy (dowód spokoju, przerywnik, rozruch, ekran poranka S-K-…) bez informacji
        if (!informacjaNaEkranie(t) && (wLogu(p, "ekran dnia S") || wLogu(p, "ekran S-K-"))) wynik = true;
        przerywnikow += wLogu(p, "ekran dnia S2D-");
        dowodow += wLogu(p, "ekran dnia S2A") || wLogu(p, "ekran dnia S2B") || wLogu(p, "ekran dnia S2C");
      } else if (informacjaNaEkranie(t)) {
        wSlocie = true;
      }
    }
    porankiZWynikiem += wynik;
    dniZInformacjaWSlocie += wSlocie;
  }
  printf("  R-KONF-61 poranki: %d, z wynikiem nocy bez przycisku %d; wybudzeń przed slotem %d (z informacją %d; "
         "przerywnik %d, dowód spokoju %d); dni z informacją w slocie %d\n",
         poranki, porankiZWynikiem, wybudzenRano, ranoZInformacja, przerywnikow, dowodow, dniZInformacjaWSlocie);
  SPRAWDZ(porankiZWynikiem == poranki);  // 100 % poranków
  SPRAWDZ(ranoZInformacja == 0);
  SPRAWDZ(przerywnikow > 0 && dowodow > 0);  // oba rodzaje poranka w próbie
  SPRAWDZ(dniZInformacjaWSlocie == poranki);
  // ▶ na informacji (slot dzienny): pierwsza strona kokpitu, informacja zamknięta
  przyciskiem(p, ep(2026, 11, 20, 13, 5), B_PRAWY, 200);
  wybudz(p);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::K1 && zapisNvs(p).empty());
}

// przycisk na informacji nie uruchamia akcji ekranu, którego nie było widać, także startu slotu dziennego:
// informacja na ekranie slotu, stan RTC startu slotu wyzerowany (jak po utracie), ◀ → ekran dnia, bez startu slotu.
static void testR_KONF_61_przyciskBezStartuSlotu() {
  Panel p;
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 9, 0));
  wybudz(p);
  uzbroj(p);
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 11, 0));
  SPRAWDZ(informacjaNaEkranie(wybudz(p)) && wLogu(p, "slot dzienny: S-K-"));
  p.start.stan.slotDziennyEp = 0;  // slot „nie zaczęty” z punktu widzenia przycisku
  przyciskiem(p, ep(2026, 11, 10, 11, 5), B_LEWY, 200);
  airbridge(p, d0());
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(!informacjaNaEkranie(t) && zapisNvs(p).empty());
  SPRAWDZ(!wLogu(p, "slot dzienny: start naciśnięciem"));
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::DZIEN);
}

// wpis, który nie może się już pokazać, kasowany przy rysowaniu ekranu dnia (1 zapis, potem bez zapisów):
// informacja innej wersji (czeka albo pokazana), pokazana w innej dobie, także w dobie z przyszłości (zły zegar przy
// pierwszym pokazaniu); wpis czekający dla tej wersji zostaje; bez zegara kasuje tylko inna wersja.
static void testR_KONF_61_porzadki() {
  struct Przypadek {
    const char* opis;
    const char* wlasna;
    const char* dzien;
    bool zegarZnany;
    bool kasowany;
  };
  const Przypadek pr[] = {
      {"inna wersja, czeka", "0.9.9", "", true, true},
      {"inna wersja, pokazana dziś", "0.9.9", "2026-11-10", true, true},
      {"pokazana wczoraj", "1.0.0", "2026-11-09", true, true},
      {"pokazana w przyszłości (zły zegar)", "1.0.0", "2027-01-01", true, true},
      {"czeka, ta wersja", "1.0.0", "", true, false},
      {"bez zegara: pokazana w innej dobie", "1.0.0", "2026-11-09", false, false},
      {"bez zegara: inna wersja", "0.9.9", "", false, true},
  };
  for (auto& c : pr) {
    Panel p;
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 7, 0));
    wybudz(p);
    Informacja i;
    i.rodzaj = RodzajInformacji::UDANA;
    i.wlasna = c.wlasna;
    i.nowa = c.wlasna;
    i.dzien = c.dzien;
    p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, aktualizacja::zapisInformacji(i));
    airbridge(p, d0());
    zegarem(p, c.zegarZnany ? ep(2026, 11, 10, 8, 0) : 1000);  // rysowanie ekranu dnia (start slotu)
    TekstyWybudzenia t = wybudz(p);
    int zap = p.d.ile(std::string("nvs.zapis ") + KLUCZ);
    bool pusty = zapisNvs(p).empty();
    if (pusty != c.kasowany) printf(" %s: skasowany %d, zapisów %d\n", c.opis, pusty, zap);
    SPRAWDZ(pusty == c.kasowany);
    if (c.kasowany) SPRAWDZ(!informacjaNaEkranie(t) && zap == 1);
    else SPRAWDZ(informacjaNvs(p).rodzaj == RodzajInformacji::UDANA);  // czekający wpis tej wersji może stanąć
    // kolejne rysowanie: bez zapisu
    airbridge(p, d0());
    zegarem(p, c.zegarZnany ? ep(2026, 11, 10, 9, 0) : 2000);
    wybudz(p);
    SPRAWDZ(p.d.ile(std::string("nvs.zapis ") + KLUCZ) == 0);
  }
  {  // recenzja: zegar z przyszłości przy pierwszym pokazaniu, potem poprawiony: wpis nie zostaje do 2027-01-01
    Panel p;
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 9, 0));
    wybudz(p);
    Informacja i;
    i.rodzaj = RodzajInformacji::UDANA;
    i.wlasna = i.nowa = "1.0.0";
    i.dzien = "2027-01-01";
    p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, aktualizacja::zapisInformacji(i));
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 12, 0));  // slot dzienny
    SPRAWDZ(!informacjaNaEkranie(wybudz(p)) && zapisNvs(p).empty());
  }
}

// Slot dzienny bez ekranu bez sprawy (panel bez miasta i obrazków: ekran z rana zostaje cały dzień, R-KON-54):
// informacja nie przepada, czeka bez doby na kolejną dobę; bez zapisów NVS w dobie; po dniu z ekranem slotu staje.
static void testR_KONF_61_bezEkranuSlotu() {
  Panel p;
  airbridge(p, d0());
  zegarem(p, ep(2026, 11, 10, 7, 0));
  wybudz(p, KONF_BEZ_MIASTA);
  uzbroj(p);
  int inf = 0, zap = 0;
  for (int h = 8; h <= 21; h++) {
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, h, 0));
    inf += informacjaNaEkranie(wybudz(p, KONF_BEZ_MIASTA));
    zap += p.d.ile(std::string("nvs.zapis ") + KLUCZ);
  }
  SPRAWDZ(inf == 0 && zap == 0);
  SPRAWDZ(informacjaNvs(p).rodzaj == RodzajInformacji::UDANA && informacjaNvs(p).dzien.empty());
  // następna doba bez miasta: dalej czeka
  airbridge(p, d0() + 1);
  zegarem(p, ep(2026, 11, 11, 12, 0));
  SPRAWDZ(!informacjaNaEkranie(wybudz(p, KONF_BEZ_MIASTA)) && informacjaNvs(p).dzien.empty());
  // doba z ekranem slotu (miasto ustawione): staje
  bool jest = false;
  for (int h = 13; h <= 15 && !jest; h++) {
    airbridge(p, d0() + 1);
    zegarem(p, ep(2026, 11, 11, h, 0));
    jest = informacjaNaEkranie(wybudz(p));
  }
  SPRAWDZ(jest && informacjaNvs(p).dzien == "2026-11-11");
}

// nieudana instalacja automatyczna: nowa wersja nie potwierdza startu, panel wraca; po powrocie wariant „nie udało się”
static void testR_KONF_61_nieudana() {
  Panel* p = panelAutomatu();
  airbridge(*p, d0());
  zegarem(*p, ep(2026, 11, 10, 9, 0));
  wybudz(*p);
  SPRAWDZ(p->wydania.restartow == 1);
  restart(*p, ep(2026, 11, 10, 9, 5), "1.0.1+def5678", StanWersji::DO_SPRAWDZENIA);
  p->pliki.montujeSie = false;
  airbridge(*p, d0());
  wybudz(*p);
  SPRAWDZ(p->start.powrot && !p->start.oznaczona);
  SPRAWDZ(zapisNvs(*p).empty());  // nowa wersja nic nie zapisała
  p->pliki.montujeSie = true;
  restart(*p, ep(2026, 11, 10, 9, 6), WERSJA_ATRAPY, StanWersji::DOBRA);
  airbridge(*p, d0());
  TekstyWybudzenia t = wybudz(*p);
  SPRAWDZ(p->start.stan.ekran == (uint8_t)Ekran::DZIEN);  // nie U-07 jak po ręcznej
  SPRAWDZ(!informacjaNaEkranie(t) && informacjaNvs(*p).dzien.empty());  // rano wynik nocy (decyzja 12)
  SPRAWDZ(wLogu(*p, "nie potwierdziła startu"));
  airbridge(*p, d0());
  zegarem(*p, ep(2026, 11, 10, 11, 0));  // slot dzienny
  t = wybudz(*p);
  SPRAWDZ(p->start.stan.ekran == (uint8_t)Ekran::DZIEN);
  SPRAWDZ(wTekstach(t, "U-07 · Aktualizacja się nie udała. Zostaje wersja 1.0.0."));
  SPRAWDZ(wTekstach(t, "U-07 · Panel próbował zaktualizować się sam."));  // decyzja 12
  SPRAWDZ(wTekstach(t, "U-07 · Nowa wersja nie uruchomiła się poprawnie. Panel wrócił do 1.0.0."));
  SPRAWDZ(!wTekstach(t, "Co nowego"));
  Informacja i = informacjaNvs(*p);
  SPRAWDZ(i.rodzaj == RodzajInformacji::NIEUDANA && i.wlasna == "1.0.0" && i.nowa == "1.0.1" && i.dzien == "2026-11-10");
  przyciskiem(*p, ep(2026, 11, 10, 11, 20), B_SRODEK, 200);
  SPRAWDZ(!informacjaNaEkranie(wybudz(*p)));
  SPRAWDZ(zapisNvs(*p).empty());
  delete p;
}

// ręczna instalacja bez zmian (U-07 po restarcie), bez informacji na ekranie dnia; bez listy nowości sama wersja;
// wgranie kablem innej wersji: informacja innej wersji się nie pokazuje
static void testR_KONF_61_recznaIBezNowosci() {
  {  // ręczna: U-07 `Zainstalowano wersję 1.0.1.` jak dotąd, bez zapisu informacji
    Panel p;
    sekrety::zapiszKluczWydan(p.nvs, TOKEN);
    p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 9, 0));
    wybudz(p);
    p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    przyciskiem(p, ep(2026, 11, 10, 9, 1), B_SRODEK, 2500);
    wybudz(p);
    przyciskiem(p, ep(2026, 11, 10, 9, 2), B_PRAWY, 200);
    wybudz(p);
    SPRAWDZ(p.wydania.restartow == 1);
    restart(p, ep(2026, 11, 10, 9, 5), "1.0.1+def5678", StanWersji::DO_SPRAWDZENIA);
    airbridge(p, d0());
    TekstyWybudzenia t = wybudz(p);
    SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::AKTUALIZACJA && wTekstach(t, "U-07 · Zainstalowano wersję 1.0.1."));
    SPRAWDZ(zapisNvs(p).empty() && !wTekstach(t, "Zaktualizowano do"));
  }
  {  // automat, wydanie bez pola `nowosci`: sama informacja o wersji
    Panel* p = panelAutomatu();
    airbridge(*p, d0());
    zegarem(*p, ep(2026, 11, 10, 9, 0));
    wybudz(*p);
    restart(*p, ep(2026, 11, 10, 9, 5), "1.0.1+def5678", StanWersji::DOBRA);  // bez samosprawdzenia: od razu
    airbridge(*p, d0());
    TekstyWybudzenia t = wybudz(*p);
    SPRAWDZ(!informacjaNaEkranie(t));  // 09:05 rano: wynik nocy (decyzja 12)
    airbridge(*p, d0());
    zegarem(*p, ep(2026, 11, 10, 11, 0));
    t = wybudz(*p);
    SPRAWDZ(wTekstach(t, "U-07 · Zaktualizowano do 1.0.1."));  // pierwszy ekran slotu dziennego
    SPRAWDZ(!wTekstach(t, "Co nowego"));
    delete p;
  }
  {  // informacja dla innej wersji niż ta na panelu (wgranie kablem po automacie): bez informacji
    Panel p;
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 9, 0));
    wybudz(p);
    uzbroj(p, RodzajInformacji::UDANA, "0.9.9", "0.9.9");
    airbridge(p, d0());
    zegarem(p, ep(2026, 11, 10, 11, 0));
    SPRAWDZ(!informacjaNaEkranie(wybudz(p)));
  }
  {  // aktualizacje wyłączone (bez automatu): nic
    Panel p;
    sekrety::zapiszKluczWydan(p.nvs, TOKEN);
    p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
    for (int h : {9, 11, 13}) {
      airbridge(p, d0());
      zegarem(p, ep(2026, 11, 10, h, 0));
      SPRAWDZ(!informacjaNaEkranie(wybudz(p)));
    }
    SPRAWDZ(zapisNvs(p).empty() && !p.wydania.slotStartowyZmieniony);
  }
}

// liczby: zapisy NVS klucza informacji i odświeżenia e-ink w typowym dniu (wybudzenia co godzinę 09:00–21:00), wobec
// tego samego dnia bez informacji. Dwa przebiegi: temperatura w obudowie stała (klatka ekranu dnia bez zmian, więc
// bez informacji zero odświeżeń) i zmienna co godzinę (stopka ekranu dnia zmienia się, informacja stoi bez zmian).
static void testR_KONF_61_liczby() {
  for (int zmienna = 0; zmienna < 2; zmienna++) {
    int zapisow = 0, odsw[2] = {0, 0};
    for (int z = 0; z < 2; z++) {
      Panel p;
      p.czujniki.o.temp = 21.0f;
      airbridge(p, d0());
      zegarem(p, ep(2026, 11, 10, 7, 0));
      wybudz(p);
      if (z == 0) uzbroj(p);
      int o0 = p.ekran.odswiezen;
      for (int h = 9; h <= 21; h++) {
        if (zmienna) p.czujniki.o.temp = 19.0f + (float)(h % 5);
        airbridge(p, d0());
        zegarem(p, ep(2026, 11, 10, h, 0));
        wybudz(p);
        if (z == 0) zapisow += p.d.ile(std::string("nvs.zapis ") + KLUCZ);
      }
      odsw[z] = p.ekran.odswiezen - o0;
    }
    printf("  R-KONF-61: dzień 09:00–21:00 co godzinę, temperatura %s: odświeżenia z informacją %d, bez %d; zapisy NVS %d\n",
           zmienna ? "zmienna" : "stała", odsw[0], odsw[1], zapisow);
    SPRAWDZ(zapisow == 1);             // tylko pierwsze pokazanie (doba)
    SPRAWDZ(odsw[0] <= odsw[1] + 1);    // najwyżej jedno więcej (pierwsze pokazanie w klatce, która się nie zmieniła)
    if (zmienna) SPRAWDZ(odsw[0] <= odsw[1]);
  }
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testR_KONF_61_tresc();
  testR_KONF_61_uklad();
  testR_KONF_61_zapisNvs();
  testR_KONF_61_poAutomacie();
  testR_KONF_61_przyciski();
  testR_KONF_61_koniecDoby();
  testR_KONF_61_pierwszenstwo();
  testR_KONF_61_spokojnyPoranek();
  testR_KONF_61_przyciskBezStartuSlotu();
  testR_KONF_61_bezEkranuSlotu();
  testR_KONF_61_porzadki();
  testR_KONF_61_nieudana();
  testR_KONF_61_recznaIBezNowosci();
  testR_KONF_61_liczby();
  printf("testy_karta_po_aktualizacji: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
