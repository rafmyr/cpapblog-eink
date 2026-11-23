// SPDX-License-Identifier: GPL-3.0-or-later
// Kanał wczesny aktualizacji (R-KONF-62, decyzja właściciela projektu z 23.11.2026, ADR-0032 „Zmiana 2026-11-23”; T-TEST-521):
// wybór z listy wydań według SemVer (nie według kolejności z API), robocze nigdy, pre-release tylko przy włączonym
// kanale; przy wyłączonym (brak wiersza, `nie`, zła wartość) te same zapytania i te same decyzje co bez kanału
// (równoważność na atrapie `Wydania`); ten sam host i token (ADR-0001, R-NF-36); limit odpowiedzi jak `releases/latest`.
// Dane syntetyczne. Uruchomienie: tools/prototyp/testy.sh (program testy_kanal_wczesny).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "aktualizacja.h"
#include "dyrygent.h"
#include "formularz.h"
#include "magazyn.h"
#include "sekrety.h"
#include "start.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

using aktualizacja::WynikListy;

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
static const char* const TOKEN = "github_pat_ZNACZNIKTOKENU_0123456789abcdef";
static const uint8_t B_SRODEK = 1u << (int)Przycisk::SRODEK, B_PRAWY = 1u << (int)Przycisk::PRAWY;

static std::string raport(long day) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[],\"settings\":{\"day\":" + d + ",\"available\":false,\"fields\":[]}}";
}
static std::string listaNocy(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}
static void airbridge(Panel& p, long d) {
  p.siec.odpowiedzi.clear();
  p.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  p.siec.dodaj(sciezkaListy(), 200, listaNocy(d - 2, d));
  for (long x = d - 2; x <= d; x++) p.siec.dodaj(sciezkaNocy(x), 200, raport(x));
}
static std::string obrazTestowy(size_t n = 20000) {
  std::string b(n, '\0');
  for (size_t i = 0; i < n; i++) b[i] = (char)(i * 31 + 7);
  return b;
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
static void przyciskiem(Panel& p, uint32_t t, Ekran e, uint8_t bit, uint32_t ms) {
  zegarem(p, t);
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = {{0, bit}, {ms, 0}};
  p.start.stan.ekran = (uint8_t)e;
  p.start.stan.ekranOd = t - 10;
}
static bool wTekstach(const TekstyWybudzenia& t, const std::string& f) {
  for (auto& l : t.linieTestu)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static TekstyWybudzenia wybudz(Panel& p) {
  Dyrygent d(p.adaptery(), KONF);
  d.wybudzenie();
  if (getenv("LOG"))
    for (auto& l : p.port.log) printf("    | %s\n", l.c_str());
  return d.teksty();
}
static int zadan(const Panel& p, const char* fragment) {
  int n = 0;
  for (auto& z : p.wydania.zadania) n += z.url.find(fragment) != std::string::npos;
  return n;
}
static std::string u07(Panel& p) { return std::to_string(p.nvs.liczbaU(aktualizacja::PRZESTRZEN, "u07", 99)); }

// Obiekt wydania jak w odpowiedzi GitHub API (`draft`, `prerelease`, zasoby przez API).
static std::string wydanie(const std::string& tag, bool pre, const char* draft = "false", bool zOpisem = true) {
  std::string api = std::string("https://api.github.com/repos/") + REPO_WYDAN + "/releases/assets/";
  std::string w = tag[0] == 'v' ? tag.substr(1) : tag;
  std::string s = "{\"tag_name\":\"" + tag + "\",\"name\":\"" + tag + "\",\"draft\":" + draft +
                  ",\"prerelease\":" + (pre ? "true" : "false") + ",\"body\":\"Nota\",\"assets\":[";
  if (zOpisem) s += "{\"id\":11,\"name\":\"wydanie.json\",\"url\":\"" + api + "11\",\"size\":300},";
  s += "{\"id\":12,\"name\":\"cpap-eink-" + w + ".bin\",\"url\":\"" + api + "12\",\"size\":20000}]}";
  return s;
}
static std::string lista(const std::vector<std::string>& w) {
  std::string s = "[";
  for (size_t i = 0; i < w.size(); i++) s += (i ? "," : "") + w[i];
  return s + "]";
}

// ---------- rdzeń: adres listy i wybór z listy ----------
// wymaganie: R-KONF-62
static void testR_KONF_62_wyborZListy() {
  SPRAWDZ(aktualizacja::adresListyWydan("wlasciciel/wydania") ==
          "https://api.github.com/repos/wlasciciel/wydania/releases?per_page=5");
  SPRAWDZ(aktualizacja::hostZAdresu(aktualizacja::adresListyWydan(REPO_WYDAN)) == aktualizacja::HOST_API);
  for (const char* z : {"", "rafmyr", "/x", "a/", "a/b/c", "a/b?c", "a b/c", "a/b#"})
    SPRAWDZ(aktualizacja::adresListyWydan(z).empty());
  aktualizacja::Wydanie w;
  // pre-release nowszy i zwykły starszy: pre-release (kolejność z API dowolna)
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wydanie("v1.0.1", false), wydanie("v1.1.0-beta.1", true)}), w) ==
          WynikListy::WYBRANE);
  SPRAWDZ(w.tag == "v1.1.0-beta.1" && w.obraz.nazwa == "cpap-eink-1.1.0-beta.1.bin" && !w.opis.adresApi.empty());
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wydanie("v1.1.0-beta.1", true), wydanie("v1.0.1", false)}), w) ==
              WynikListy::WYBRANE && w.tag == "v1.1.0-beta.1");
  // zwykłe 1.0.2 wyższe od 1.0.2-beta.3 (SemVer), choć beta jest pierwsza na liście
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wydanie("v1.0.2-beta.3", true), wydanie("v1.0.2", false),
                                             wydanie("v1.0.2-beta.10", true)}), w) == WynikListy::WYBRANE &&
          w.tag == "v1.0.2");
  // identyfikatory liczbowe porównywane liczbowo: beta.10 > beta.9
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wydanie("v1.0.2-beta.9", true), wydanie("v1.0.2-beta.10", true)}), w) ==
              WynikListy::WYBRANE && w.tag == "v1.0.2-beta.10");
  // robocze nigdy: `draft` true, a także w innej postaci niż false
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wydanie("v9.0.0", false, "true"), wydanie("v8.0.0", true, "\"false\""),
                                             wydanie("v7.0.0", true, "null"), wydanie("v1.0.1", false)}), w) ==
              WynikListy::WYBRANE && w.tag == "v1.0.1");
  // bez wydanie.json, tag spoza SemVer, zasób spoza API: pominięte
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wydanie("v2.0.0", false, "false", false), wydanie("najnowsze", true),
                                             wydanie("v01.0.0", false), wydanie("v1.0.3", true)}), w) ==
              WynikListy::WYBRANE && w.tag == "v1.0.3");
  std::string obce = wydanie("v3.0.0", false);
  obce.replace(obce.find("https://api.github.com"), 22, "https://evil.example");
  SPRAWDZ(aktualizacja::wybierzZListy(lista({obce, wydanie("v1.0.3", false)}), w) == WynikListy::WYBRANE &&
          w.tag == "v1.0.3");
  // ta sama wersja dwa razy: zostaje pierwsze
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wydanie("1.0.4", false), wydanie("v1.0.4", true)}), w) ==
              WynikListy::WYBRANE && w.tag == "1.0.4");
  // brak wydania do wzięcia
  aktualizacja::Wydanie przed = w;
  SPRAWDZ(aktualizacja::wybierzZListy("[]", w) == WynikListy::BRAK && w.tag == przed.tag);
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wydanie("v9.0.0", false, "true")}), w) == WynikListy::BRAK);
  SPRAWDZ(aktualizacja::wybierzZListy("[1,\"x\",null,[]]", w) == WynikListy::BRAK);
  // nie tablica albo błąd składni
  SPRAWDZ(aktualizacja::wybierzZListy(wydanie("v1.0.1", false), w) == WynikListy::NIECZYTELNA);
  SPRAWDZ(aktualizacja::wybierzZListy("", w) == WynikListy::NIECZYTELNA);
  SPRAWDZ(aktualizacja::wybierzZListy("[" + wydanie("v1.0.1", false), w) == WynikListy::NIECZYTELNA);
  // lista WYDAN_NA_LISCIE wydań z dłuższą notą mieści się w limicie odpowiedzi `releases/latest`
  std::vector<std::string> pelna;
  for (int i = 0; i < aktualizacja::WYDAN_NA_LISCIE; i++) {
    std::string e = wydanie("v1.0." + std::to_string(i) + "-beta.1", true);
    e.replace(e.find("\"Nota\""), 6, "\"" + std::string(2000, 'n') + "\"");
    pelna.push_back(e);
  }
  SPRAWDZ(lista(pelna).size() < aktualizacja::MAKS_ODPOWIEDZI_API / 4);
  SPRAWDZ(aktualizacja::wybierzZListy(lista(pelna), w) == WynikListy::WYBRANE && w.tag == "v1.0.4-beta.1");
}

// ---------- dyrygent: oba ustawienia na tej samej liście ----------
struct Wynik {
  std::vector<std::string> adresy;
  std::string u07;
  TekstyWybudzenia t;
};
// Panel 1.0.0 z tokenem; GitHub: `releases/latest` = 1.0.1, lista = 1.1.0-beta.1 (pre-release) i 1.0.1.
// `opisWersji`: wersja, której opis i obraz zwraca atrapa.
static Wynik sprawdzenieReczne(Panel& p, const std::string& konf, const std::string& opisWersji) {
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  if (!konf.empty()) p.pliki.pliki["/konfiguracja.txt"] = konf;
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  std::string latest = p.wydania.odpowiedzApi;
  p.wydania.ustaw(opisWersji, obrazTestowy(), ukladWkompilowany());
  p.wydania.odpowiedzApi = latest;
  p.wydania.odpowiedzListy = lista({wydanie("v1.0.1", false), wydanie("v1.1.0-beta.1", true),
                                    wydanie("v2.0.0", false, "true")});
  przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  Wynik w;
  w.t = wybudz(p);
  for (auto& z : p.wydania.zadania) w.adresy.push_back(z.url);
  w.u07 = u07(p);
  return w;
}
// wymaganie: R-KONF-62
static void testR_KONF_62_obaUstawienia() {
  {  // wyłączony: `releases/latest`, pre-release niewidoczny
    Panel p;
    Wynik w = sprawdzenieReczne(p, "", "1.0.1");
    SPRAWDZ(zadan(p, "/releases/latest") == 1 && zadan(p, "/releases?") == 0);
    SPRAWDZ(wTekstach(w.t, "U-07 · Jest wersja 1.0.1. Masz 1.0.0."));
    SPRAWDZ(w.u07 == std::to_string((int)aktualizacja::StanU07::NOWSZA));
  }
  {  // włączony: lista, najwyższa wersja SemVer, roboczy 2.0.0 pominięty
    Panel p;
    Wynik w = sprawdzenieReczne(p, "aktualizacja_wczesna=tak\n", "1.1.0-beta.1");
    SPRAWDZ(zadan(p, "/releases/latest") == 0 && zadan(p, "/releases?per_page=5") == 1);
    SPRAWDZ(wTekstach(w.t, "U-07 · Jest wersja 1.1.0-beta.1. Masz 1.0.0."));
    SPRAWDZ(w.u07 == std::to_string((int)aktualizacja::StanU07::NOWSZA));
    // ADR-0001, R-NF-36: tylko api.github.com i serwer plików GitHuba, token tylko do api.github.com, to samo repo
    for (auto& z : p.wydania.zadania) {
      SPRAWDZ(z.host == "api.github.com" || z.host == AtrapaWydan::HOST_PLIKOW);
      SPRAWDZ(z.host != "api.github.com" || z.url.find(std::string("/repos/") + REPO_WYDAN + "/") != std::string::npos);
    }
    SPRAWDZ(p.wydania.tokenPoszedlDo("api.github.com") && !p.wydania.tokenPoszedlDo(AtrapaWydan::HOST_PLIKOW));
    // ▶ instaluje wersję próbną tą samą drogą co zwykłą
    p.wydania.zadania.clear();
    przyciskiem(p, ep(2026, 11, 23, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
    TekstyWybudzenia t = wybudz(p);
    SPRAWDZ(wTekstach(t, "U-07 · Pobieram wersję 1.1.0-beta.1…"));
    SPRAWDZ(p.wydania.slotStartowyZmieniony && p.wydania.restartow == 1);
    SPRAWDZ(p.nvs.tekst(aktualizacja::PRZESTRZEN, "in_na", "") == "1.1.0-beta.1");
    SPRAWDZ(zadan(p, "/releases/latest") == 0);
  }
  // ta sama wersja w obu ustawieniach: bez instalacji (aktualna), bez pobrania opisu i obrazu
  for (const char* konf : {"", "aktualizacja_wczesna=tak\n"}) {
    Panel p;
    sekrety::zapiszKluczWydan(p.nvs, TOKEN);
    if (*konf) p.pliki.pliki["/konfiguracja.txt"] = konf;
    p.start.napisWersji = "1.1.0-beta.1+abc1234";
    p.wydania.ustaw("1.1.0-beta.1", obrazTestowy(), ukladWkompilowany());
    p.wydania.odpowiedzApi = wydanie("v1.0.1", false);
    p.wydania.odpowiedzListy = lista({wydanie("v1.0.1", false), wydanie("v1.1.0-beta.1", true)});
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    TekstyWybudzenia t = wybudz(p);
    SPRAWDZ(wTekstach(t, "U-07 · Masz aktualną wersję 1.1.0-beta.1."));
    SPRAWDZ(u07(p) == std::to_string((int)aktualizacja::StanU07::AKTUALNA));
    SPRAWDZ(p.wydania.zadania.size() == 1 && !p.wydania.slotStartowyZmieniony);  // samo zapytanie API
  }
  {  // panel na wersji próbnej, kanał wyłączony potem: zwykłe 1.0.1 niższe, bez cofania wersji
    Panel p;
    sekrety::zapiszKluczWydan(p.nvs, TOKEN);
    p.start.napisWersji = "1.1.0-beta.1+abc1234";
    p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    TekstyWybudzenia t = wybudz(p);
    SPRAWDZ(u07(p) == std::to_string((int)aktualizacja::StanU07::AKTUALNA) && p.wydania.zadania.size() == 1);
  }
}

// ---------- dyrygent: błędy listy jak błędy `releases/latest` ----------
// wymaganie: R-KONF-62
static void testR_KONF_62_bledyListy() {
  struct Przypadek {
    const char* opis;
    int kod;
    std::string cialo;
    aktualizacja::Powod powod;
  };
  std::string duza = "[" + wydanie("v1.0.1", true);
  for (int i = 0; duza.size() <= aktualizacja::MAKS_ODPOWIEDZI_API; i++) duza += "," + wydanie("v1.0." + std::to_string(i + 2), true);
  duza += "]";
  std::vector<Przypadek> pp = {
      {"pusta lista", 200, "[]", aktualizacja::Powod::BRAK_WYDAN},
      {"same robocze", 200, lista({wydanie("v1.0.1", false, "true")}), aktualizacja::Powod::BRAK_WYDAN},
      {"404", 404, "", aktualizacja::Powod::BRAK_WYDAN},
      {"401", 401, "", aktualizacja::Powod::TOKEN_ODRZUCONY},
      {"obiekt zamiast listy", 200, wydanie("v1.0.1", false), aktualizacja::Powod::BRAK_SIECI},
      {"za duża", 200, duza, aktualizacja::Powod::BRAK_SIECI},
  };
  for (auto& c : pp) {
    Panel p;
    sekrety::zapiszKluczWydan(p.nvs, TOKEN);
    p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_wczesna=tak\n";
    p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
    p.wydania.kodListy = c.kod;
    p.wydania.odpowiedzListy = c.cialo.empty() ? "[]" : c.cialo;
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    uint32_t pw = p.nvs.liczbaU(aktualizacja::PRZESTRZEN, "u07_p", 99);
    bool dobrze = pw == (uint32_t)c.powod && p.wydania.zadania.size() == 1 &&
                  u07(p) == std::to_string((int)aktualizacja::StanU07::SPRAWDZENIE_NIEUDANE);
    if (!dobrze) printf("  przypadek „%s”: powód %u, zapytań %zu, u07 %s\n", c.opis, pw, p.wydania.zadania.size(), u07(p).c_str());
    SPRAWDZ(dobrze);
    // lista bez wydania do wzięcia z powodem w logu (liczba wydań i roboczych)
    if (c.powod == aktualizacja::Powod::BRAK_WYDAN && c.kod == 200) {
      bool wpis = false;
      std::string oczekiwany = std::string("na liście ") + (c.cialo == "[]" ? "0, roboczych 0" : "1, roboczych 1");
      for (auto& l : p.port.log) wpis = wpis || l.find(oczekiwany) != std::string::npos;
      SPRAWDZ(wpis);
    }
  }
  // plik jest, odczyt się nie udał → kanał wyłączony w tym sprawdzeniu, wiersz logu, `releases/latest`
  Panel p;
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_wczesna=tak\n";
  p.pliki.nieczytelne.insert("/konfiguracja.txt");
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  wybudz(p);
  bool wpis = false;
  for (auto& l : p.port.log) wpis = wpis || l == "kanał wczesny: konfiguracja nieczytelna, wyłączony w tym sprawdzeniu";
  SPRAWDZ(wpis && zadan(p, "/releases/latest") == 1 && zadan(p, "/releases?") == 0);
}

// ---------- równoważność: kanał wyłączony = te same zapytania sieciowe, decyzje, log i NVS; jeden odczyt pliku więcej ----------
struct Przebieg {
  std::vector<std::string> zadania;  // adres i nagłówki każdego żądania
  std::vector<std::string> log;      // wiersze `aktualizacja: …` z logu
  std::vector<std::string> pominiete;  // `konfiguracja: pominięty wiersz 'aktualizacja…'` (zły zapis, tylko log)
  std::vector<std::string> u07;      // stan U-07 po każdym wybudzeniu
  std::map<std::string, std::string> nvsTeksty;
  std::map<std::string, uint32_t> nvsLiczby;
  int restartow = 0;
  bool slot = false;
};
// 3 poranki ze sprawdzeniem dobowym, ↻ 2 s na U-01, ▶ instalacja; przy `automat` instalacja z automatu.
static Przebieg przebieg(const std::string& konf, bool automat) {
  Panel p;
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  std::string k = (automat ? "aktualizacja_auto=tak\n" : "") + konf;
  if (!k.empty()) p.pliki.pliki["/konfiguracja.txt"] = k;
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  // lista z wersją próbną: przy wyłączonym kanale panel jej nie widzi i nie pyta o nią
  p.wydania.odpowiedzListy = lista({wydanie("v1.0.1", false), wydanie("v1.1.0-beta.1", true)});
  Przebieg w;
  auto zbierz = [&]() {
    for (auto& l : p.port.log)
      if (l.rfind("aktualizacja", 0) == 0) w.log.push_back(l);
      else if (l.rfind("konfiguracja: pominięty wiersz 'aktualizacja", 0) == 0) w.pominiete.push_back(l);
    w.u07.push_back(u07(p));
  };
  long d0 = nrDnia("2026-11-20");
  for (int doba = 0; doba < 3; doba++) {
    for (int h : {7, 12}) {
      airbridge(p, d0 + doba);
      zegarem(p, ep(2026, 11, 21 + doba, h, 0));
      wybudz(p);
      zbierz();
    }
  }
  przyciskiem(p, ep(2026, 11, 23, 15, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  wybudz(p);
  zbierz();
  przyciskiem(p, ep(2026, 11, 23, 15, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  wybudz(p);
  zbierz();
  for (auto& z : p.wydania.zadania) {
    std::string s = z.url;
    for (auto& h : z.naglowki) s += "|" + h.first + "=" + h.second;
    w.zadania.push_back(s);
  }
  for (auto& e : p.nvs.teksty)
    if (e.first.rfind(std::string(aktualizacja::PRZESTRZEN) + "/", 0) == 0) w.nvsTeksty[e.first] = e.second;
  for (auto& e : p.nvs.liczbyU)
    if (e.first.rfind(std::string(aktualizacja::PRZESTRZEN) + "/", 0) == 0) w.nvsLiczby[e.first] = e.second;
  w.restartow = p.wydania.restartow;
  w.slot = p.wydania.slotStartowyZmieniony;
  return w;
}
static bool rowne(const Przebieg& a, const Przebieg& b) {
  if (getenv("ROZNICE")) {
    printf("    zadania %d log %d u07 %d nvsT %d nvsL %d restart %d/%d slot %d\n", a.zadania == b.zadania, a.log == b.log,
           a.u07 == b.u07, a.nvsTeksty == b.nvsTeksty, a.nvsLiczby == b.nvsLiczby, a.restartow, b.restartow, a.slot == b.slot);
    for (size_t i = 0; i < std::max(a.log.size(), b.log.size()); i++) {
      std::string x = i < a.log.size() ? a.log[i] : "-", y = i < b.log.size() ? b.log[i] : "-";
      if (x != y) printf("      %s\n   != %s\n", x.c_str(), y.c_str());
    }
  }
  return a.zadania == b.zadania && a.log == b.log && a.u07 == b.u07 && a.nvsTeksty == b.nvsTeksty &&
         a.nvsLiczby == b.nvsLiczby && a.restartow == b.restartow && a.slot == b.slot;
}
// wymaganie: R-KONF-62
static void testR_KONF_62_rownowaznosc() {
  for (bool automat : {false, true}) {
    Przebieg wzor = przebieg("", automat);
    SPRAWDZ(!wzor.zadania.empty() && wzor.restartow >= 1 && wzor.slot);  // scenariusz naprawdę pyta i instaluje
    SPRAWDZ(wzor.pominiete.empty());  // poprawny `aktualizacja_auto=tak` nie jest „pominiętym wierszem”
    for (auto& z : wzor.zadania) SPRAWDZ(z.find("/releases?") == std::string::npos);
    for (const char* konf : {"aktualizacja_wczesna=nie\n", "aktualizacja_wczesna=\n", "aktualizacja_wczesna=TAK\n",
                             "aktualizacja_wczesna = tak\n", "#aktualizacja_wczesna=tak\n"}) {
      Przebieg b = przebieg(konf, automat);
      if (!rowne(wzor, b)) printf("  równoważność: „%s” (automat %d) inna niż bez wiersza\n", konf, automat);
      SPRAWDZ(rowne(wzor, b));
      // zły zapis (panel go nie stosuje) zostaje w logu jako pominięty wiersz; `nie` i komentarz nie
      bool zly = std::string(konf) != "aktualizacja_wczesna=nie\n" && konf[0] != '#';
      SPRAWDZ(b.pominiete.empty() != zly);
    }
    // kontrola: włączony pyta o listę, nigdy o `releases/latest`
    Przebieg wl = przebieg("aktualizacja_wczesna=tak\n", automat);
    SPRAWDZ(!rowne(wzor, wl) && wl.pominiete.empty());
    int lista = 0, latest = 0;
    for (auto& z : wl.zadania) {
      lista += z.find("/releases?per_page=5|") != std::string::npos;
      latest += z.find("/releases/latest") != std::string::npos;
    }
    SPRAWDZ(lista >= 1 && latest == 0);
  }
}

// ---------- formularz U-03: pole, zapis, walidacja, nagłówek sekcji; zrzut `Z` z kluczem ----------
// wymaganie: R-KONF-62
static void testR_KONF_62_formularz() {
  sekrety::Dane ob;
  SPRAWDZ(formularz::zPliku("", ob).aktualizacjaWczesna == "nie");
  SPRAWDZ(formularz::zPliku("aktualizacja_wczesna=tak\n", ob).aktualizacjaWczesna == "tak");
  SPRAWDZ(formularz::zPliku("aktualizacja_wczesna=cos\n", ob).aktualizacjaWczesna == "nie");
  auto w = formularz::waliduj(formularz::parsujCialo("aktualizacja_wczesna=tak"), ob, "2026-11-23");
  SPRAWDZ(w.ok() && w.plik["aktualizacja_wczesna"] == "tak");
  w = formularz::waliduj(formularz::parsujCialo("aktualizacja_wczesna=nie"), ob, "2026-11-23");
  SPRAWDZ(w.ok() && w.plik.count("aktualizacja_wczesna") && w.plik["aktualizacja_wczesna"].empty());  // wiersz znika
  w = formularz::waliduj(formularz::parsujCialo("aktualizacja_wczesna=moze"), ob, "2026-11-23");
  SPRAWDZ(!w.ok() && w.bledy["aktualizacja_wczesna"] == "Wybierz z listy." && !w.plik.count("aktualizacja_wczesna"));
  w = formularz::waliduj(formularz::parsujCialo("aktualizacja_auto=tak"), ob, "2026-11-23");
  SPRAWDZ(w.ok() && !w.plik.count("aktualizacja_wczesna"));  // bez pola w żądaniu: plik bez zmiany wiersza
  // zapis do pliku i odczyt z powrotem
  std::string plik = formularz::scal("buzzer=nie\n", {{"aktualizacja_wczesna", "tak"}});
  SPRAWDZ(plik.find("aktualizacja_wczesna=tak\n") != std::string::npos);
  SPRAWDZ(formularz::zPliku(plik, ob).aktualizacjaWczesna == "tak");
  plik = formularz::scal(plik, {{"aktualizacja_wczesna", ""}});
  SPRAWDZ(plik.find("aktualizacja_wczesna") == std::string::npos);
  // strona: pole w sekcji „Aktualizacje” z opisem, wybór zaznaczony, nagłówek sekcji `· próbne`
  formularz::Pola p = formularz::zPliku("aktualizacja_wczesna=tak\n", ob);
  std::string s = formularz::strona(p, "/u/0123456789abcdef0123456789abcdef/", "", {});
  size_t a = s.find("<b>Aktualizacje</b>"), f = s.find("aktualizacja_wczesna"), k = s.find("klucz_wydan");
  SPRAWDZ(a != std::string::npos && f > a && f < k);
  SPRAWDZ(s.find("Dostawaj wersje próbne przed innymi") != std::string::npos);
  SPRAWDZ(s.find("Wersje próbne mogą mieć błędy. Wyłączenie nie cofa wersji.") != std::string::npos);
  SPRAWDZ(s.find("ręcznie · próbne") != std::string::npos);
  SPRAWDZ(s.size() <= formularz::REZERWACJA_STRONY);
  std::string sb = formularz::strona(formularz::zPliku("", ob), "/u/0123456789abcdef0123456789abcdef/", "",
                                     {{"aktualizacja_wczesna", "Wybierz z listy."}});
  SPRAWDZ(sb.find("<details open><summary><b>Aktualizacje</b>") != std::string::npos);  // sekcja z błędem otwarta
  SPRAWDZ(sb.find("ręcznie · próbne") == std::string::npos);
  // zrzut `Z` niesie /konfiguracja.txt z kluczem bajt w bajt
  Panel z;
  z.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_wczesna=tak\n";
  zegarem(z, ep(2026, 11, 23, 14, 0));
  z.port.znaki.push_back('Z');
  wybudz(z);
  bool jest = false, sekcja = false;
  for (auto& l : z.port.log) {
    sekcja = sekcja || l == "PLIK /konfiguracja.txt";
    jest = jest || (sekcja && l == "aktualizacja_wczesna=tak");
  }
  SPRAWDZ(jest);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testR_KONF_62_wyborZListy();
  testR_KONF_62_obaUstawienia();
  testR_KONF_62_bledyListy();
  testR_KONF_62_rownowaznosc();
  testR_KONF_62_formularz();
  printf("testy_kanal_wczesny: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
