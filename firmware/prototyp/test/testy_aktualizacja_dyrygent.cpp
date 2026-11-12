// SPDX-License-Identifier: GPL-3.0-or-later
// Scenariusze aktualizacji z wydań GitHub z dyrygentem na atrapach (ADR-0032, ADR-0038; kolumna
// „Weryfikacja” R-NF-32…R-NF-36, R-KONF-43…R-KONF-46, R-EKR-61):
// bez tokenu zero zapytań, ↻ 2 s na U-01 i U-07 we wszystkich stanach, instalacja ▶ z restartem i wynikiem po starcie,
// powrót do poprzedniej wersji, odrzucone wydania (równa, niższa, inna płytka, inny układ, zły skrót, ucięty plik),
// sprawdzenie raz na dobę w porannym oknie Wi-Fi (3 doby), automat z warunkami, ikona w stopce ekranu dnia, token nigdy
// w logu, zrzucie `Z`, plikach i tekstach ekranów, token tylko do api.github.com, certyfikat spoza zestawu.
// Atrapa GitHuba: test/atrapy/adaptery_atrapy.h AtrapaWydan. Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_aktualizacja_dyrygent).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "aktualizacja.h"
#include "dyrygent.h"
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

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const char* STATUS_TERAPIA =
    "{\"version\":\"v2.2.1\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"THERAPY\","
    "\"rop\":1,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"blocked\"}";
static const char* const TOKEN = "github_pat_ZNACZNIKTOKENU_0123456789abcdef";
static const char* const ZNACZNIK = "ZNACZNIKTOKENU";
static const uint8_t B_LEWY = 1u << (int)Przycisk::LEWY, B_SRODEK = 1u << (int)Przycisk::SRODEK,
                     B_PRAWY = 1u << (int)Przycisk::PRAWY;

static std::string raport(long day) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Press\",\"label\":\"C\",\"unit\":\"cmH2O\",\"count\":12660,"
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
// AirBridge z nocą z doby `d` (poranek d+1)
static void airbridge(Panel& p, long d, const char* status = STATUS_SPOCZYNEK) {
  p.siec.odpowiedzi.clear();
  p.siec.dodaj("/api/status", 200, status);
  p.siec.dodaj(sciezkaListy(), 200, lista(d - 2, d));
  for (long x = d - 2; x <= d; x++) p.siec.dodaj(sciezkaNocy(x), 200, raport(x));
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
// wybudzenie przyciskiem `bit` trzymanym `ms` na ekranie `e` (ekran z RTC sprzed snu)
static void przyciskiem(Panel& p, uint32_t t, Ekran e, uint8_t bit, uint32_t ms) {
  zegarem(p, t);
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = {{0, bit}, {ms, 0}};
  p.start.stan.ekran = (uint8_t)e;
  p.start.stan.ekranOd = t - 10;
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
  if (getenv("LOG2"))
    for (auto& l : t.linieTestu) printf("    linia: %s\n", l.c_str());
  return false;
}
static bool wDiagnostyce(const TekstyWybudzenia& t, const std::string& f) {
  for (auto& l : t.diagnostykaTestu)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static int zadanApi(const Panel& p) {
  int n = 0;
  for (auto& z : p.wydania.zadania) n += z.url.find("/releases/latest") != std::string::npos;
  return n;
}
static std::string u07(Panel& p) { return std::to_string(p.nvs.liczbaU(aktualizacja::PRZESTRZEN, "u07", 99)); }
static TekstyWybudzenia wybudz(Panel& p, const KonfiguracjaPanelu& k = KONF) {
  Dyrygent d(p.adaptery(), k);
  d.wybudzenie();
  if (getenv("LOG"))
    for (auto& l : p.port.log) printf("    | %s\n", l.c_str());
  return d.teksty();
}
static void zToken(Panel& p) { sekrety::zapiszKluczWydan(p.nvs, TOKEN); }

// ---------- R-NF-33: bez tokenu zero zapytań, tekst na U-01 i U-07 ----------
static void testR_NF_33_bezTokenu() {
  Panel p;
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  long d = nrDnia("2026-11-09");
  for (int doba = 0; doba < 2; doba++) {  // poranki z Wi-Fi
    airbridge(p, d + doba);
    zegarem(p, ep(2026, 11, 10 + doba, 7, 0));
    wybudz(p);
  }
  przyciskiem(p, ep(2026, 11, 11, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);  // ↻ 2 s na U-01
  int polaczen = p.d.ile("siec.polacz");
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(p.wydania.zadania.empty());  // zero połączeń do GitHuba
  SPRAWDZ(p.d.ile("siec.polacz") == polaczen && p.d.ile("siec.polacz") == 0);  // bez Wi-Fi
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::AKTUALIZACJA);
  SPRAWDZ(wTekstach(t, "U-07 · Aktualizacje wyłączone. Ustaw je w „Więcej na telefonie”."));
  SPRAWDZ(aktualizacja::wierszU01(sekrety::kluczWydan(p.nvs) != "", "", "1.0.0") ==
          "Aktualizacje wyłączone. Ustaw je w „Więcej na telefonie”.");
  // U-07 wyłączone: ▶ nic, ◀ U-01
  przyciskiem(p, ep(2026, 11, 11, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  wybudz(p);
  SPRAWDZ(p.wydania.zadania.empty() && p.start.stan.ekran == (uint8_t)Ekran::AKTUALIZACJA);
  przyciskiem(p, ep(2026, 11, 11, 9, 2), Ekran::AKTUALIZACJA, B_LEWY, 200);
  wybudz(p);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::USTAWIENIA);
}

// ---------- R-KONF-43, R-KONF-44, R-NF-32, R-NF-36: ↻ 2 s, „jest nowsza”, ▶ instaluje, wynik po restarcie ----------
static void testR_KONF_44_instalacjaPrzyciskiem() {
  Panel p;
  zToken(p);
  std::string obraz = obrazTestowy();
  p.wydania.ustaw("1.0.1", obraz, ukladWkompilowany());
  przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::AKTUALIZACJA);
  SPRAWDZ(wTekstach(t, "U-07 · Sprawdzam, czy jest nowa wersja…"));
  SPRAWDZ(wTekstach(t, "U-07 · Jest wersja 1.0.1. Masz 1.0.0."));
  SPRAWDZ(u07(p) == std::to_string((int)aktualizacja::StanU07::NOWSZA));
  // R-NF-32: tylko adres API i adres pliku; token tylko do api.github.com (R-NF-36)
  SPRAWDZ(zadanApi(p) == 1 && p.wydania.zadania.size() == 3);
  for (auto& z : p.wydania.zadania)
    SPRAWDZ(z.host == "api.github.com" || z.host == AtrapaWydan::HOST_PLIKOW);
  SPRAWDZ(p.wydania.tokenPoszedlDo("api.github.com") && !p.wydania.tokenPoszedlDo(AtrapaWydan::HOST_PLIKOW));
  for (auto& z : p.wydania.zadania)  // bez danych nocy w adresie
    SPRAWDZ((z.url.find("ahi") == std::string::npos && z.url.find("day=") == std::string::npos &&
             z.url.find("rafmyr/cpapblog-eink") != std::string::npos) ||
            z.host == AtrapaWydan::HOST_PLIKOW);
  SPRAWDZ(!p.wydania.slotStartowyZmieniony && p.wydania.restartow == 0);  // samo sprawdzenie niczego nie instaluje
  SPRAWDZ(p.d.ile("siec.wylacz") >= 1 && !p.siec.polaczonaTeraz);  // Wi-Fi wyłączone po sprawdzeniu
  // ▶ „tak, instaluj”
  p.wydania.zadania.clear();
  przyciskiem(p, ep(2026, 11, 10, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  int snow = p.sen.snow;
  t = wybudz(p);
  SPRAWDZ(wTekstach(t, "U-07 · Pobieram wersję 1.0.1…") && wTekstach(t, "U-07 · Instaluję. Panel zaraz uruchomi się ponownie."));
  SPRAWDZ(p.wydania.slotStartowyZmieniony && p.wydania.restartow == 1 && p.sen.snow == snow);  // restart zamiast snu
  SPRAWDZ(!p.wydania.tokenPoszedlDo(AtrapaWydan::HOST_PLIKOW));
  SPRAWDZ(p.nvs.tekst(aktualizacja::PRZESTRZEN, "in_na", "") == "1.0.1");
  SPRAWDZ(p.nvs.tekst(aktualizacja::PRZESTRZEN, "in_z", "") == "1.0.0");
  // restart w nowej wersji: samosprawdzenie, wynik `Zainstalowano wersję 1.0.1.` na U-07
  restart(p, ep(2026, 11, 10, 9, 5), "1.0.1+def5678", StanWersji::DO_SPRAWDZENIA);
  airbridge(p, nrDnia("2026-11-09"));
  t = wybudz(p);
  SPRAWDZ(p.start.oznaczona);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::AKTUALIZACJA);
  SPRAWDZ(wTekstach(t, "U-07 · Zainstalowano wersję 1.0.1."));
  aktualizacja::Stan s = aktualizacja::wczytaj(p.nvs);
  SPRAWDZ(s.instalujeNa.empty() && s.znana.empty() && s.probNieudanych == 0);
  SPRAWDZ(wTekstach(t, "Oprogramowanie: 1.0.1 (app0)"));  // U-05 bez dopisku
}

// ---------- R-KONF-44: ◀ „nie teraz”, ↻ i 5 min bez wyboru nie instalują ----------
static void testR_KONF_44_bezWyboru() {
  for (int wariant = 0; wariant < 3; wariant++) {
    Panel p;
    zToken(p);
    p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
    przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::AKTUALIZACJA);
    uint32_t od = p.start.stan.ekranOd;
    if (wariant == 0) {
      przyciskiem(p, ep(2026, 11, 10, 9, 1), Ekran::AKTUALIZACJA, B_LEWY, 200);
      wybudz(p);
      SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::USTAWIENIA);
    } else if (wariant == 1) {
      przyciskiem(p, ep(2026, 11, 10, 9, 1), Ekran::AKTUALIZACJA, B_SRODEK, 200);
      wybudz(p);
      SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::DZIEN);
    } else {
      zegarem(p, od + (uint32_t)aktualizacja::U07_CZAS_S + 1);  // 5 min [PRÓG] bez wyboru
      wybudz(p);
      SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::USTAWIENIA);
    }
    SPRAWDZ(p.wydania.ustawien == 0 && !p.wydania.slotStartowyZmieniony && p.wydania.restartow == 0);
  }
}

// ---------- R-NF-34: wydania odrzucone ----------
static void sprawdzOdrzucone(const char* opis, void (*psuj)(Panel&), aktualizacja::StanU07 oczekiwany,
                             const char* tekst) {
  Panel p;
  zToken(p);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  psuj(p);
  przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  TekstyWybudzenia t = wybudz(p);
  if (oczekiwany == aktualizacja::StanU07::NOWSZA) {  // błąd wychodzi dopiero przy pobraniu
    przyciskiem(p, ep(2026, 11, 10, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
    t = wybudz(p);
    oczekiwany = aktualizacja::StanU07::PO_NIEUDANEJ;
  }
  bool dobrze = u07(p) == std::to_string((int)oczekiwany) && !p.wydania.slotStartowyZmieniony &&
                p.wydania.restartow == 0 && wTekstach(t, tekst);
  if (!dobrze) printf("  odrzucone wydanie: %s\n", opis);
  SPRAWDZ(dobrze);
}
static void testR_NF_34_odrzucone() {
  using aktualizacja::StanU07;
  sprawdzOdrzucone("wersja równa", [](Panel& p) { p.wydania.ustaw("1.0.0", obrazTestowy(), ukladWkompilowany()); },
                   StanU07::AKTUALNA, "U-07 · Masz aktualną wersję 1.0.0.");
  sprawdzOdrzucone("wersja niższa", [](Panel& p) { p.wydania.ustaw("0.9.0", obrazTestowy(), ukladWkompilowany()); },
                   StanU07::AKTUALNA, "U-07 · Masz aktualną wersję 1.0.0.");
  sprawdzOdrzucone("inna płytka",
                   [](Panel& p) { p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany(), "inna_plytka"); },
                   StanU07::NOWSZA, "Ta wersja nie pasuje do tego panelu. Potrzebny kabel.");
  sprawdzOdrzucone("inny układ", [](Panel& p) { p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany() ^ 1u); },
                   StanU07::NOWSZA, "Ta wersja nie pasuje do tego panelu. Potrzebny kabel.");
  sprawdzOdrzucone("zły skrót", [](Panel& p) { p.wydania.obraz[100] ^= 1; }, StanU07::NOWSZA,
                   "Pobrany plik jest uszkodzony. Spróbuj ponownie.");
  sprawdzOdrzucone("plik ucięty", [](Panel& p) { p.wydania.uciecObrazPo = 1000; }, StanU07::NOWSZA,
                   "Pobrany plik jest uszkodzony. Spróbuj ponownie.");
  sprawdzOdrzucone("obraz odrzucony przez ESP-IDF", [](Panel& p) { p.wydania.startowyOk = false; }, StanU07::NOWSZA,
                   "Pobrany plik jest uszkodzony. Spróbuj ponownie.");
  sprawdzOdrzucone("za duży na slot", [](Panel& p) { p.wydania.slot = 1000; }, StanU07::NOWSZA,
                   "Ta wersja nie pasuje do tego panelu. Potrzebny kabel.");
  sprawdzOdrzucone("token odrzucony", [](Panel& p) { p.wydania.kodApi = 401; }, StanU07::SPRAWDZENIE_NIEUDANE,
                   "GitHub odrzucił klucz wydań. Wpisz nowy w „Więcej na telefonie”.");
  sprawdzOdrzucone("brak internetu", [](Panel& p) { p.wydania.kodApi = -1; }, StanU07::SPRAWDZENIE_NIEUDANE,
                   "Nie mogę sprawdzić wersji. Brak połączenia z internetem.");
  sprawdzOdrzucone("brak Wi-Fi", [](Panel& p) { p.siec.siecJest = false; }, StanU07::SPRAWDZENIE_NIEUDANE,
                   "Nie mogę sprawdzić wersji. Brak połączenia z internetem.");
  sprawdzOdrzucone("bateria", [](Panel& p) { p.czujniki.o.bateriaV = 3.55f; }, StanU07::NOWSZA,
                   "Podłącz zasilanie i spróbuj ponownie.");
  // ▶ „spróbuj ponownie” po nieudanym pobraniu: druga próba z dobrym plikiem instaluje
  Panel p;
  zToken(p);
  std::string ob = obrazTestowy();
  p.wydania.ustaw("1.0.1", ob, ukladWkompilowany());
  p.wydania.uciecObrazPo = 500;
  przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  wybudz(p);
  przyciskiem(p, ep(2026, 11, 10, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  wybudz(p);
  SPRAWDZ(p.wydania.porzucen >= 1 && !p.wydania.slotStartowyZmieniony);
  p.wydania.uciecObrazPo = 0;
  przyciskiem(p, ep(2026, 11, 10, 9, 2), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  wybudz(p);
  SPRAWDZ(p.wydania.slotStartowyZmieniony && p.wydania.restartow == 1);
  // w nocy ręczna instalacja czeka
  Panel n;
  zToken(n);
  n.wydania.ustaw("1.0.1", ob, ukladWkompilowany());
  przyciskiem(n, ep(2026, 11, 10, 23, 40), Ekran::USTAWIENIA, B_SRODEK, 2500);
  wybudz(n);
  przyciskiem(n, ep(2026, 11, 10, 23, 41), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  TekstyWybudzenia t = wybudz(n);
  SPRAWDZ(wTekstach(t, "Teraz nie instaluję: trwa noc. Spróbuj rano.") && !n.wydania.slotStartowyZmieniony);
}

// ---------- R-NF-35: nowa wersja nie wstała, panel wrócił ----------
static void testR_NF_35_powrot() {
  Panel p;
  zToken(p);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  airbridge(p, nrDnia("2026-11-09"));
  zegarem(p, ep(2026, 11, 10, 7, 0));
  wybudz(p);  // noce w plikach
  std::string noce = p.pliki.pliki["/dane/noce/2026.csv"];
  SPRAWDZ(!noce.empty());
  przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  wybudz(p);
  przyciskiem(p, ep(2026, 11, 10, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  wybudz(p);
  SPRAWDZ(p.wydania.restartow == 1);
  // nowa wersja startuje i nie przechodzi samosprawdzenia (pliki się nie montują): powrót do poprzedniej
  restart(p, ep(2026, 11, 10, 9, 5), "1.0.1+def5678", StanWersji::DO_SPRAWDZENIA);
  p.pliki.montujeSie = false;
  wybudz(p);
  SPRAWDZ(p.start.powrot && !p.start.oznaczona);
  SPRAWDZ(p.nvs.tekst(aktualizacja::PRZESTRZEN, "in_na", "") == "1.0.1");  // wynik nie zapisany przed oznaczeniem
  // bootloader wraca do 1.0.0: wynik na U-07 i U-05
  p.pliki.montujeSie = true;
  restart(p, ep(2026, 11, 10, 9, 6), WERSJA_ATRAPY, StanWersji::DOBRA);
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(p.start.stan.ekran == (uint8_t)Ekran::AKTUALIZACJA);
  SPRAWDZ(wTekstach(t, "U-07 · Aktualizacja się nie udała. Zostaje wersja 1.0.0."));
  SPRAWDZ(wTekstach(t, "U-07 · Nowa wersja nie uruchomiła się poprawnie. Panel wrócił do 1.0.0."));
  SPRAWDZ(wTekstach(t, "Oprogramowanie: 1.0.0 · odrz. 1.0.1"));  // U-05 (kolumna ok. 188 px)
  SPRAWDZ(wDiagnostyce(t, "· odrzucona 1.0.1"));
  SPRAWDZ(wLogu(p, "nie potwierdziła startu"));
  SPRAWDZ(p.pliki.pliki["/dane/noce/2026.csv"] == noce);  // rekordy nocy bajt w bajt
}

// ---------- R-KONF-46: raz na dobę w porannym oknie Wi-Fi, bez dodatkowych wybudzeń; wynik na U-01 i U-05 ----------
static void testR_KONF_46_razNaDobe() {
  Panel z, bez;
  zToken(z);
  z.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  long d0 = nrDnia("2026-11-09");
  for (int doba = 0; doba < 3; doba++) {
    for (int h : {7, 11, 14}) {
      for (Panel* p : {&z, &bez}) {
        airbridge(*p, d0 + doba);
        zegarem(*p, ep(2026, 11, 10 + doba, h, 0));
      }
      TekstyWybudzenia t = wybudz(z);
      wybudz(bez);
      // plan snu bez dodatkowego wybudzenia: ta sama chwila następnego wybudzenia (sprawdzenie trwa ok. 0,6 s
      // czasu atrapy, więc liczba sekund snu może być o 1 mniejsza, gdy sprawdzenie przekroczy pełną sekundę)
      SPRAWDZ(z.zegar.teraz() + z.sen.plan.sekund == bez.zegar.teraz() + bez.sen.plan.sekund);
      if (doba == 2 && h == 14)
        SPRAWDZ(wTekstach(t, "Oprogramowanie: 1.0.0 · jest 1.0.1") && wDiagnostyce(t, "· jest wersja 1.0.1"));
    }
  }
  SPRAWDZ(zadanApi(z) == 3);  // 3 doby: 3 zapytania
  SPRAWDZ(bez.wydania.zadania.empty());
  SPRAWDZ(aktualizacja::wczytaj(z.nvs).znana == "1.0.1");
  SPRAWDZ(aktualizacja::wierszU01(true, aktualizacja::wczytaj(z.nvs).znana, "1.0.0") ==
          "Jest wersja 1.0.1 · ↻ 2 s Aktualizacja");
  SPRAWDZ(!z.wydania.slotStartowyZmieniony);  // bez automatu: tylko informacja
  // bez poprawnego czasu: bez sprawdzania (karta 2 pkt 7)
  Panel c;
  zToken(c);
  c.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  airbridge(c, d0);
  zegarem(c, 1000);  // zegar nieustawiony
  c.siec.ntpJest = false;
  wybudz(c);
  SPRAWDZ(c.wydania.zadania.empty());
}

// ---------- R-KONF-45: automat ----------
static Panel* panelAutomatu(bool wl) {
  Panel* p = new Panel();
  zToken(*p);
  p->wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  if (wl) p->pliki.pliki["/konfiguracja.txt"] = "aktualizacja_auto=tak\n";
  return p;
}
static void testR_KONF_45_automat() {
  long d0 = nrDnia("2026-11-09");
  {  // wszystkie warunki: instalacja bez pytania, bez ekranu U-07
    Panel* p = panelAutomatu(true);
    airbridge(*p, d0);
    zegarem(*p, ep(2026, 11, 10, 7, 0));
    wybudz(*p);
    SPRAWDZ(p->wydania.slotStartowyZmieniony && p->wydania.restartow == 1);
    SPRAWDZ(p->start.stan.ekran == (uint8_t)Ekran::DZIEN);
    restart(*p, ep(2026, 11, 10, 7, 5), "1.0.1+def5678", StanWersji::DO_SPRAWDZENIA);
    airbridge(*p, d0);
    wybudz(*p);
    SPRAWDZ(p->start.oznaczona && p->start.stan.ekran == (uint8_t)Ekran::DZIEN);  // automat: po restarcie ekran dnia
    SPRAWDZ(aktualizacja::wczytaj(p->nvs).znana.empty());
    delete p;
  }
  {  // ustawienie wyłączone: tylko znana wersja
    Panel* p = panelAutomatu(false);
    airbridge(*p, d0);
    zegarem(*p, ep(2026, 11, 10, 7, 0));
    wybudz(*p);
    SPRAWDZ(!p->wydania.slotStartowyZmieniony && aktualizacja::wczytaj(p->nvs).znana == "1.0.1");
    delete p;
  }
  {  // bateria poniżej progu: bez instalacji, powód w logu
    Panel* p = panelAutomatu(true);
    p->czujniki.o.bateriaV = 3.55f;
    airbridge(*p, d0);
    zegarem(*p, ep(2026, 11, 10, 7, 0));
    wybudz(*p);
    SPRAWDZ(!p->wydania.slotStartowyZmieniony && wLogu(*p, "automat bez instalacji: bateria"));
    delete p;
  }
  {  // trwa terapia (rop = 1): bez sprawdzenia i bez instalacji (doba nie zużyta)
    Panel* p = panelAutomatu(true);
    airbridge(*p, d0, STATUS_TERAPIA);
    zegarem(*p, ep(2026, 11, 10, 7, 0));
    wybudz(*p);
    SPRAWDZ(!p->wydania.slotStartowyZmieniony && p->wydania.zadania.empty());
    SPRAWDZ(aktualizacja::wczytaj(p->nvs).sprawdzonoDnia.empty());
    // znana wersja i terapia: automat odmawia z powodem przejściowym, bez Wi-Fi do GitHuba
    aktualizacja::Stan s = aktualizacja::wczytaj(p->nvs);
    s.znana = "1.0.1";
    aktualizacja::zapisz(p->nvs, s);
    airbridge(*p, d0, STATUS_TERAPIA);
    zegarem(*p, ep(2026, 11, 10, 8, 0));
    wybudz(*p);
    SPRAWDZ(!p->wydania.slotStartowyZmieniony && wLogu(*p, "automat bez instalacji: noc albo terapia"));
    // terapia minęła: ta sama doba, instalacja
    airbridge(*p, d0, STATUS_SPOCZYNEK);
    zegarem(*p, ep(2026, 11, 10, 9, 0));
    wybudz(*p);
    SPRAWDZ(p->wydania.slotStartowyZmieniony && p->wydania.restartow == 1);
    delete p;
  }
  {  // nieudane pobranie: jedna próba na dobę, po 3 nieudanych pomija wersję
    Panel* p = panelAutomatu(true);
    p->wydania.uciecObrazPo = 100;
    int prob = 0;
    bool pominieta = false;
    for (int doba = 0; doba < 5; doba++) {
      for (int h : {7, 11}) {
        airbridge(*p, d0 + doba);
        zegarem(*p, ep(2026, 11, 10 + doba, h, 0));
        wybudz(*p);
        prob += p->d.ile("wydania.obraz");
        pominieta = pominieta || wLogu(*p, "wersja pominięta po 3 nieudanych próbach");
      }
    }
    SPRAWDZ(prob == 3 && p->wydania.porzucen == 3 && !p->wydania.slotStartowyZmieniony);  // jedna próba na dobę
    SPRAWDZ(aktualizacja::wczytaj(p->nvs).probNieudanych == 3);
    SPRAWDZ(pominieta);
    delete p;
  }
}

// ---------- R-EKR-61: ikona w stopce ekranu dnia ----------
static int czarneIkony(const Panel& p) {
  int n = 0;
  int x0 = 772 - BOK_IKONY_AKTUALIZACJI;
  for (int y = 437; y < 480; y++)
    for (int x = x0; x < 772; x++) n += (p.ekran.p.b[(size_t)(y * 100 + x / 8)] >> (7 - (x & 7))) & 1;
  return n;
}
static std::string logEkranuDnia(const Panel& p) {
  for (auto& l : p.port.log)
    if (l.compare(0, 11, "ekran dnia ") == 0 && l.find("ikona") == std::string::npos) return l;
  return "";
}
static void testR_EKR_61_ikona() {
  long d0 = nrDnia("2026-11-09");
  Panel z, bez;
  for (Panel* p : {&z, &bez}) {
    airbridge(*p, d0);
    zegarem(*p, ep(2026, 11, 10, 7, 0));
  }
  aktualizacja::Stan s;
  s.znana = "1.0.1";
  aktualizacja::zapisz(z.nvs, s);
  zToken(z);  // bez klucza ikony nie ma
  wybudz(z);
  wybudz(bez);
  SPRAWDZ(czarneIkony(z) > 200 && wLogu(z, "ikona aktualizacji"));
  SPRAWDZ(!wLogu(bez, "ikona aktualizacji"));
  if (logEkranuDnia(z) != logEkranuDnia(bez)) printf("  [%s] [%s]\n", logEkranuDnia(z).c_str(), logEkranuDnia(bez).c_str());
  SPRAWDZ(logEkranuDnia(z) == logEkranuDnia(bez) && !logEkranuDnia(z).empty());  // ten sam wybór ekranu dnia
  // S9 w nocy: bez ikony
  airbridge(z, d0 + 1);
  zegarem(z, ep(2026, 11, 10, 23, 30));
  wybudz(z);
  SPRAWDZ(wLogu(z, "ekran nocny") && !wLogu(z, "ikona aktualizacji"));
  // po instalacji wersji 1.0.1: bez ikony
  z.start.napisWersji = "1.0.1+def";
  airbridge(z, d0 + 1);
  zegarem(z, ep(2026, 11, 11, 7, 0));
  wybudz(z);
  SPRAWDZ(!wLogu(z, "ikona aktualizacji"));
  // ekrany serwisowe: U-01 bez ikony
  z.start.napisWersji = WERSJA_ATRAPY;
  // ◀ z testu sprzętu U-05 → U-01 (od ADR-0041 bez strony „Sprzęt” przed U-01)
  przyciskiem(z, ep(2026, 11, 11, 9, 0), Ekran::TEST, B_LEWY, 200);
  aktualizacja::zapisz(z.nvs, s);
  wybudz(z);
  SPRAWDZ(z.start.stan.ekran == (uint8_t)Ekran::USTAWIENIA && !wLogu(z, "ikona aktualizacji"));
}

// ---------- R-NF-36, R-NF-32: token nigdzie poza żądaniem do API; certyfikat spoza zestawu ----------
static bool gdziesZnacznik(const Panel& p, const TekstyWybudzenia& t) {
  for (auto& l : p.port.log)
    if (l.find(ZNACZNIK) != std::string::npos) return true;
  for (auto& f : p.pliki.pliki)
    if (f.second.find(ZNACZNIK) != std::string::npos) return true;
  for (auto& l : t.linieTestu)
    if (l.find(ZNACZNIK) != std::string::npos) return true;
  for (auto& l : t.diagnostykaTestu)
    if (l.find(ZNACZNIK) != std::string::npos) return true;
  for (auto& kv : p.nvs.teksty)  // poza przestrzenią `sekrety` (zrzut i kopia na karcie biorą tylko `cpap` i `noc`)
    if (kv.first.compare(0, 8, "sekrety/") != 0 && kv.second.find(ZNACZNIK) != std::string::npos) return true;
  return false;
}
static void testR_NF_36_tokenNigdzie() {
  Panel p;
  zToken(p);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  airbridge(p, nrDnia("2026-11-09"));
  zegarem(p, ep(2026, 11, 10, 7, 0));
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(!gdziesZnacznik(p, t));
  przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  p.port.znaki = {'Z'};  // zrzut plików w czuwaniu
  t = wybudz(p);
  SPRAWDZ(wLogu(p, "PLIKI_START") && !gdziesZnacznik(p, t));
  przyciskiem(p, ep(2026, 11, 10, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  t = wybudz(p);
  SPRAWDZ(!gdziesZnacznik(p, t) && !p.wydania.tokenPoszedlDo(AtrapaWydan::HOST_PLIKOW));
  // certyfikat spoza zestawu: żadne żądanie z tokenem nie wychodzi, wpis w logu
  Panel c;
  zToken(c);
  c.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  c.wydania.certyfikatZly = true;
  przyciskiem(c, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  t = wybudz(c);
  SPRAWDZ(c.wydania.zadania.empty() && wLogu(c, "certyfikat serwera spoza zestawu"));
  SPRAWDZ(wTekstach(t, "Nie mogę sprawdzić wersji. Brak połączenia z internetem."));
  // przekierowanie na http:// (bez TLS): bez pobrania opisu
  Panel h;
  zToken(h);
  h.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  h.wydania.przekierowanieNaHttp = true;
  przyciskiem(h, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  wybudz(h);
  SPRAWDZ(u07(h) == std::to_string((int)aktualizacja::StanU07::SPRAWDZENIE_NIEUDANE) && !h.wydania.tokenPoszedlDo(AtrapaWydan::HOST_PLIKOW));
}


// ---------- R-KONF-45: automat nie instaluje, gdy na ekranie dnia stoi L z pilnością B ----------
// 15 nocy z AHI 12 / h → ekran dnia L-03 (pilność B), jak test/testy_wzorce_l_hipotezy_h.cpp testDyrygentL03().
static std::string raportAhi(long day, const char* ahi) {
  std::string r = raport(day);
  size_t p = r.find("\"ahi\":1.4");
  return p == std::string::npos ? r : r.replace(p, 9, std::string("\"ahi\":") + ahi);
}
static void testR_KONF_45_sprawaABNaEkranie() {
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
  Panel p;
  const int NOCY = 15;
  long ost = nrDnia("2026-11-04") - 1;
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = ost - NOCY + 1; x <= ost; x++)
    l += std::string(x > ost - NOCY + 1 ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  l += "]}";
  auto odpowiedzi = [&]() {
    p.siec.odpowiedzi.clear();
    p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    p.siec.dodaj(sciezkaListy(), 200, l);
    for (long x = ost - NOCY + 1; x <= ost; x++) p.siec.dodaj(sciezkaNocy(x), 200, raportAhi(x, "12.0"));
  };
  bool jest = false;
  for (int h = 0; h < 12 && !jest; h++) {  // historia partiami, aż ekran dnia to L-03
    odpowiedzi();
    restart(p, ep(2026, 11, 4, 7, 0) + (uint32_t)(h * 1800), WERSJA_ATRAPY, StanWersji::DOBRA);
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    wybudz(p);
    jest = wLogu(p, "ekran dnia: wzorzec L-03");
  }
  SPRAWDZ(jest);
  SPRAWDZ(p.nvs.flaga(aktualizacja::PRZESTRZEN, "ekr_ab", false));  // pilność ekranu zapamiętana trwale
  // automat z kluczem i znaną nowszą wersją: przy L-03 (B) na ekranie bez instalacji
  zToken(p);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_auto=tak\n";
  aktualizacja::Stan s = aktualizacja::wczytaj(p.nvs);
  s.znana = "1.0.1";
  aktualizacja::zapisz(p.nvs, s);
  odpowiedzi();
  zegarem(p, ep(2026, 11, 4, 13, 0));
  wybudz(p);
  SPRAWDZ(!p.wydania.slotStartowyZmieniony && wLogu(p, "automat bez instalacji: sprawa A albo B na ekranie"));
  SPRAWDZ(!wLogu(p, "ikona aktualizacji"));  // ekran L A/B bez ikony
  // wybudzenie bez rysowania ekranu dnia (przycisk na K1): zapamiętana pilność dalej blokuje
  przyciskiem(p, ep(2026, 11, 4, 13, 10), Ekran::K1, B_PRAWY, 200);
  wybudz(p);
  SPRAWDZ(!p.wydania.slotStartowyZmieniony && p.nvs.flaga(aktualizacja::PRZESTRZEN, "ekr_ab", false));
}

// ---------- ▶ instaluje wersję z pytania; nowsza w międzyczasie → pytanie od nowa ----------
static void testR_KONF_44_wersjaZPytania() {
  Panel p;
  zToken(p);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  wybudz(p);
  p.wydania.ustaw("1.0.2", obrazTestowy(30000), ukladWkompilowany());  // nowe wydanie w czasie pytania
  przyciskiem(p, ep(2026, 11, 10, 9, 2), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(!p.wydania.slotStartowyZmieniony && wTekstach(t, "U-07 · Jest wersja 1.0.2. Masz 1.0.0."));
  przyciskiem(p, ep(2026, 11, 10, 9, 3), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  wybudz(p);
  SPRAWDZ(p.wydania.slotStartowyZmieniony && p.nvs.tekst(aktualizacja::PRZESTRZEN, "in_na", "") == "1.0.2");
}

// ---------- 404 to nie „aktualna”; po usunięciu klucza bez ikony ----------
static void testR_NF_33_brakWydanIBezKlucza() {
  Panel p;
  zToken(p);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  p.wydania.kodApi = 404;
  przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(wTekstach(t, "U-07 · Nie widzę wydań. Sprawdź klucz w „Więcej na telefonie”."));
  SPRAWDZ(!wTekstach(t, "Masz aktualną wersję"));
  Panel q;
  zToken(q);
  aktualizacja::Stan s;
  s.znana = "1.0.1";
  aktualizacja::zapisz(q.nvs, s);
  sekrety::zapiszKluczWydan(q.nvs, "");
  airbridge(q, nrDnia("2026-11-09"));
  zegarem(q, ep(2026, 11, 10, 7, 0));
  wybudz(q);
  SPRAWDZ(!wLogu(q, "ikona aktualizacji") && q.wydania.zadania.empty());
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testR_NF_33_bezTokenu();
  testR_KONF_44_instalacjaPrzyciskiem();
  testR_KONF_44_bezWyboru();
  testR_NF_34_odrzucone();
  testR_NF_35_powrot();
  testR_KONF_46_razNaDobe();
  testR_KONF_45_automat();
  testR_EKR_61_ikona();
  testR_NF_36_tokenNigdzie();
  testR_KONF_45_sprawaABNaEkranie();
  testR_KONF_44_wersjaZPytania();
  testR_NF_33_brakWydanIBezKlucza();
  printf("testy_aktualizacja_dyrygent: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
