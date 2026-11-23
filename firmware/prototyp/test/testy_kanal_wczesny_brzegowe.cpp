// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe kanału wczesnego (R-KONF-62, T-TEST-522; 23.11.2026), niezależne od testy_kanal_wczesny.cpp:
// SemVer na liście (beta.9/beta.13/rc.1/zwykłe), nigdy w dół, SHA-256 przy wersji z listy, przełączenie kanału między
// sprawdzeniem a instalacją, plik konfiguracji nieczytelny, CRLF, duplikat klucza (panel wobec formularza),
// realistyczna odpowiedź GitHuba (rozmiar), robocze na początku listy. Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_kanal_wczesny_brzegowe).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "aktualizacja.h"
#include "dyrygent.h"
#include "formularz.h"
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
static const char* const TOKEN = "github_pat_ZNACZNIKTOKENU_0123456789abcdef";
static const uint8_t B_SRODEK = 1u << (int)Przycisk::SRODEK, B_PRAWY = 1u << (int)Przycisk::PRAWY;

static std::string obrazTestowy(size_t n = 20000) {
  std::string b(n, '\0');
  for (size_t i = 0; i < n; i++) b[i] = (char)(i * 31 + 7);
  return b;
}
static void przyciskiem(Panel& p, uint32_t t, Ekran e, uint8_t bit, uint32_t ms) {
  p.zegar.epoch = t;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.siec.polaczonaTeraz = false;
  p.d.z.clear();
  p.port.log.clear();
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = {{0, bit}, {ms, 0}};
  p.start.stan.ekran = (uint8_t)e;
  p.start.stan.ekranOd = t - 10;
}
static TekstyWybudzenia wybudz(Panel& p) {
  Dyrygent d(p.adaptery(), KONF);
  d.wybudzenie();
  if (getenv("LOG"))
    for (auto& l : p.port.log) printf("    | %s\n", l.c_str());
  return d.teksty();
}
static bool wTekstach(const TekstyWybudzenia& t, const std::string& f) {
  for (auto& l : t.linieTestu)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static bool wLogu(const Panel& p, const std::string& f) {
  for (auto& l : p.port.log)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static int zadan(const Panel& p, const char* fragment) {
  int n = 0;
  for (auto& z : p.wydania.zadania) n += z.url.find(fragment) != std::string::npos;
  return n;
}
static std::string u07(Panel& p) { return std::to_string(p.nvs.liczbaU(aktualizacja::PRZESTRZEN, "u07", 99)); }
static std::string stan(aktualizacja::StanU07 s) { return std::to_string((int)s); }

// Obiekt wydania jak w GitHub API, z polami, które API naprawdę zwraca (author, uploader, reactions, adresy).
static std::string uzytkownik() {
  return "{\"login\":\"rafmyr\",\"id\":123456789,\"node_id\":\"MDQ6VXNlcjEyMzQ1Njc4OQ==\","
         "\"avatar_url\":\"https://avatars.githubusercontent.com/u/123456789?v=4\",\"gravatar_id\":\"\","
         "\"url\":\"https://api.github.com/owner/rafmyr\",\"html_url\":\"https://github.com/rafmyr\","
         "\"followers_url\":\"https://api.github.com/owner/rafmyr/followers\","
         "\"following_url\":\"https://api.github.com/owner/rafmyr/following{/other_user}\","
         "\"gists_url\":\"https://api.github.com/owner/rafmyr/gists{/gist_id}\","
         "\"starred_url\":\"https://api.github.com/owner/rafmyr/starred{/owner}{/repo}\","
         "\"subscriptions_url\":\"https://api.github.com/owner/rafmyr/subscriptions\","
         "\"organizations_url\":\"https://api.github.com/owner/rafmyr/orgs\","
         "\"repos_url\":\"https://api.github.com/owner/rafmyr/repos\","
         "\"events_url\":\"https://api.github.com/owner/rafmyr/events{/privacy}\","
         "\"received_events_url\":\"https://api.github.com/owner/rafmyr/received_events\",\"type\":\"User\","
         "\"user_view_type\":\"public\",\"site_admin\":false}";
}
static std::string zasob(int id, const std::string& nazwa, size_t rozmiar, const std::string& tag) {
  std::string api = std::string("https://api.github.com/repos/") + REPO_WYDAN + "/releases/assets/" + std::to_string(id);
  return "{\"url\":\"" + api + "\",\"id\":" + std::to_string(id) + ",\"node_id\":\"RA_kwDOOabcdefghijklmn\",\"name\":\"" +
         nazwa + "\",\"label\":\"\",\"uploader\":" + uzytkownik() +
         ",\"content_type\":\"application/octet-stream\",\"state\":\"uploaded\",\"size\":" + std::to_string(rozmiar) +
         ",\"digest\":\"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
         "\"download_count\":3,\"created_at\":\"2026-11-23T10:00:00Z\",\"updated_at\":\"2026-11-23T10:00:05Z\","
         "\"browser_download_url\":\"https://github.com/" + REPO_WYDAN + "/releases/download/" + tag + "/" + nazwa + "\"}";
}
static std::string wydanieGitHub(const std::string& tag, bool pre, bool draft, size_t dlugoscNoty, int id) {
  std::string w = tag[0] == 'v' || tag[0] == 'V' ? tag.substr(1) : tag;
  std::string repo = std::string("https://api.github.com/repos/") + REPO_WYDAN;
  std::string nota(dlugoscNoty, 'n');
  return "{\"url\":\"" + repo + "/releases/" + std::to_string(id) + "\",\"assets_url\":\"" + repo + "/releases/" +
         std::to_string(id) + "/assets\",\"upload_url\":\"https://uploads.github.com/repos/" + REPO_WYDAN +
         "/releases/" + std::to_string(id) + "/assets{?name,label}\",\"html_url\":\"https://github.com/" + REPO_WYDAN +
         "/releases/tag/" + tag + "\",\"id\":" + std::to_string(id) + ",\"author\":" + uzytkownik() +
         ",\"node_id\":\"RE_kwDOOabcdefghijk\",\"tag_name\":\"" + tag +
         "\",\"target_commitish\":\"main\",\"name\":\"cpap-eink " + w + "\",\"draft\":" + (draft ? "true" : "false") +
         ",\"immutable\":false,\"prerelease\":" + (pre ? "true" : "false") +
         ",\"created_at\":\"2026-11-23T10:00:00Z\",\"updated_at\":\"2026-11-23T10:00:06Z\","
         "\"published_at\":\"2026-11-23T10:00:06Z\",\"assets\":[" +
         zasob(id * 10 + 1, "cpap-eink-" + w + ".bin", 1700000, tag) + "," + zasob(id * 10 + 2, "wydanie.json", 300, tag) +
         "],\"tarball_url\":\"" + repo + "/tarball/" + tag + "\",\"zipball_url\":\"" + repo + "/zipball/" + tag +
         "\",\"body\":\"" + nota + "\",\"reactions\":{\"url\":\"" + repo + "/releases/" + std::to_string(id) +
         "/reactions\",\"total_count\":0,\"+1\":0,\"-1\":0,\"laugh\":0,\"hooray\":0,\"confused\":0,\"heart\":0,"
         "\"rocket\":0,\"eyes\":0},\"mentions_count\":0}";
}
static std::string lista(const std::vector<std::string>& w) {
  std::string s = "[";
  for (size_t i = 0; i < w.size(); i++) s += (i ? "," : "") + w[i];
  return s + "]";
}
static std::string wyd(const std::string& tag, bool pre, bool draft = false) { return wydanieGitHub(tag, pre, draft, 200, 7); }

// ---------- 1. SemVer na liście ----------
static void brz_semver() {
  aktualizacja::Wydanie w;
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wyd("v1.0.0-beta.9", true), wyd("v1.0.0-beta.13", true),
                                             wyd("v1.0.0-rc.1", true), wyd("v1.0.0", false)}), w) == WynikListy::WYBRANE &&
          w.tag == "v1.0.0");
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wyd("v1.0.0-beta.13", true), wyd("v1.0.0-rc.1", true),
                                             wyd("v1.0.0-beta.9", true)}), w) == WynikListy::WYBRANE &&
          w.tag == "v1.0.0-rc.1");
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wyd("v1.0.0-beta.9", true), wyd("v1.0.0-beta.13", true)}), w) ==
              WynikListy::WYBRANE && w.tag == "v1.0.0-beta.13");
  // `V` wielką literą i metadane budowy: SemVer przyjmuje; metadane nie podnoszą wersji
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wyd("V1.0.1", false), wyd("v1.0.1+abc", true)}), w) == WynikListy::WYBRANE &&
          w.tag == "V1.0.1");
  // robocze z najwyższym numerem przy tokenie z prawem zapisu (API zwraca je wtedy na liście)
  SPRAWDZ(aktualizacja::wybierzZListy(lista({wyd("v9.9.9", true, true), wyd("v1.0.1", false)}), w) == WynikListy::WYBRANE &&
          w.tag == "v1.0.1");
  // 5 wydań roboczych na początku listy (GitHub pokazuje robocze pierwsze) zasłania zwykłe: BRAK
  std::vector<std::string> robocze;
  for (int i = 0; i < aktualizacja::WYDAN_NA_LISCIE; i++) robocze.push_back(wyd("v2.0." + std::to_string(i), false, true));
  SPRAWDZ(aktualizacja::wybierzZListy(lista(robocze), w) == WynikListy::BRAK);
}

// ---------- 2. rozmiar realistycznej odpowiedzi ----------
static void brz_rozmiar() {
  std::vector<std::string> v;
  for (int i = 0; i < aktualizacja::WYDAN_NA_LISCIE; i++)
    v.push_back(wydanieGitHub("v1.1." + std::to_string(i) + "-beta.1", true, false, 400, 200 + i));
  std::string s = lista(v);
  printf("  realistyczna lista %d wydań (nota 400 B, 2 zasoby): %zu B, limit %zu B\n", aktualizacja::WYDAN_NA_LISCIE,
         s.size(), aktualizacja::MAKS_ODPOWIEDZI_API);
  SPRAWDZ(s.size() < aktualizacja::MAKS_ODPOWIEDZI_API);
  aktualizacja::Wydanie w;
  SPRAWDZ(aktualizacja::wybierzZListy(s, w) == WynikListy::WYBRANE && w.tag == "v1.1.4-beta.1");
  // przy jakiej długości noty lista przestaje się mieścić
  size_t bez = s.size() - (size_t)aktualizacja::WYDAN_NA_LISCIE * 400;
  size_t prog = (aktualizacja::MAKS_ODPOWIEDZI_API - bez) / aktualizacja::WYDAN_NA_LISCIE;
  printf("  lista bez not %zu B; przekroczenie limitu przy średniej nocie > %zu B na wydanie\n", bez, prog);
  SPRAWDZ(prog > 4000);  // nota z wydaj.py ma kilkaset bajtów
}

// ---------- 3. dyrygent: nigdy w dół, SHA-256, przełączenie kanału, plik nieczytelny ----------
static void przygotuj(Panel& p, const std::string& konf, const char* wlasna, const std::string& opisWersji,
                      const std::string& listaWydan) {
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  if (!konf.empty()) p.pliki.pliki["/konfiguracja.txt"] = konf;
  if (*wlasna) p.start.napisWersji = wlasna;
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  std::string latest = p.wydania.odpowiedzApi;
  p.wydania.ustaw(opisWersji, obrazTestowy(), ukladWkompilowany());
  p.wydania.odpowiedzApi = latest;
  p.wydania.odpowiedzListy = listaWydan;
}
static void brz_dyrygent() {
  const std::string L = lista({wyd("v1.0.1", false), wyd("v1.1.0-beta.1", true)});
  {  // nigdy w dół: panel 1.2.0 (zwykła), lista najwyżej 1.1.0-beta.1
    Panel p;
    przygotuj(p, "aktualizacja_wczesna=tak\n", "1.2.0+abc1234", "1.1.0-beta.1", L);
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    SPRAWDZ(u07(p) == stan(aktualizacja::StanU07::AKTUALNA) && p.wydania.zadania.size() == 1);
    SPRAWDZ(!p.wydania.slotStartowyZmieniony);
  }
  {  // wersja zwykła 1.1.0 zainstalowana, na liście 1.1.0-beta.5 (pre-release tego samego numeru): nie nowsza
    Panel p;
    przygotuj(p, "aktualizacja_wczesna=tak\n", "1.1.0+abc1234", "1.1.0-beta.5",
              lista({wyd("v1.1.0-beta.5", true), wyd("v1.0.1", false)}));
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    SPRAWDZ(u07(p) == stan(aktualizacja::StanU07::AKTUALNA) && !p.wydania.slotStartowyZmieniony);
  }
  {  // SHA-256: wersja próbna z listy z uszkodzonym obrazem nie trafia do slotu startowego
    Panel p;
    przygotuj(p, "aktualizacja_wczesna=tak\n", "", "1.1.0-beta.1", L);
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    SPRAWDZ(u07(p) == stan(aktualizacja::StanU07::NOWSZA));
    p.wydania.obraz[100] ^= 0x5a;  // ten sam rozmiar, inny skrót
    przyciskiem(p, ep(2026, 11, 23, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
    wybudz(p);
    SPRAWDZ(!p.wydania.slotStartowyZmieniony && p.wydania.restartow == 0);
    SPRAWDZ(p.nvs.tekst(aktualizacja::PRZESTRZEN, "in_na", "") != "1.1.0-beta.1");
  }
  {  // przełączenie kanału po sprawdzeniu: ekran mówi 1.1.0-beta.1, kanał wyłączony, ▶
    Panel p;
    przygotuj(p, "aktualizacja_wczesna=tak\n", "", "1.1.0-beta.1", L);
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    SPRAWDZ(u07(p) == stan(aktualizacja::StanU07::NOWSZA));
    p.pliki.pliki["/konfiguracja.txt"] = "";  // formularz zapisał `nie`: wiersz znika
    p.wydania.zadania.clear();
    przyciskiem(p, ep(2026, 11, 23, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
    TekstyWybudzenia t = wybudz(p);
    std::string na = p.nvs.tekst(aktualizacja::PRZESTRZEN, "in_na", "");
    printf("  kanał wyłączony między sprawdzeniem a ▶: zapytań listy %d, latest %d, in_na „%s”, slot %d\n",
           zadan(p, "/releases?"), zadan(p, "/releases/latest"), na.c_str(), (int)p.wydania.slotStartowyZmieniony);
    // wersja próbna nie może wejść po wyłączeniu kanału
    SPRAWDZ(na != "1.1.0-beta.1");
    SPRAWDZ(zadan(p, "/releases?") == 0);
    SPRAWDZ(!p.wydania.slotStartowyZmieniony);
    printf("  ekran po ▶: %s\n", wTekstach(t, "Pobieram wersję 1.0.1") ? "Pobieram wersję 1.0.1" : wTekstach(t, "Pobieram") ? "inne pobieranie" : "bez pobierania");
  }
  {  // to samo, ale GitHub spójny (opis i obraz 1.0.1): ▶ instaluje 1.0.1, choć U-07 pokazywał 1.1.0-beta.1
    Panel p;
    przygotuj(p, "aktualizacja_wczesna=tak\n", "", "1.1.0-beta.1", L);
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    p.pliki.pliki["/konfiguracja.txt"] = "";
    p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
    przyciskiem(p, ep(2026, 11, 23, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
    TekstyWybudzenia t = wybudz(p);
    std::string na = p.nvs.tekst(aktualizacja::PRZESTRZEN, "in_na", "");
    printf("  spójny GitHub: po ▶ in_na „%s”, „Pobieram wersję 1.0.1”: %d\n", na.c_str(), (int)wTekstach(t, "Pobieram wersję 1.0.1"));
    SPRAWDZ(na.empty() && !p.wydania.slotStartowyZmieniony);
    // istniejąca ochrona: inna wersja niż na pytaniu = pytanie od nowa, bez instalacji
    SPRAWDZ(wLogu(p, "w pytaniu 1.1.0-beta.1, teraz najnowsza 1.0.1: pytam ponownie"));
  }
  {  // odwrotnie: U-07 pokazał 1.0.1 (kanał wyłączony), właściciel włącza kanał, ▶
    Panel p;
    przygotuj(p, "", "", "1.0.1", L);
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    SPRAWDZ(u07(p) == stan(aktualizacja::StanU07::NOWSZA));
    p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_wczesna=tak\n";
    p.wydania.ustaw("1.1.0-beta.1", obrazTestowy(), ukladWkompilowany());
    przyciskiem(p, ep(2026, 11, 23, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
    TekstyWybudzenia t = wybudz(p);
    std::string na = p.nvs.tekst(aktualizacja::PRZESTRZEN, "in_na", "");
    printf("  włączony między sprawdzeniem a ▶: in_na „%s”, „Pobieram wersję 1.1.0-beta.1”: %d\n", na.c_str(),
           (int)wTekstach(t, "Pobieram wersję 1.1.0-beta.1"));
    SPRAWDZ(na.empty() && !p.wydania.slotStartowyZmieniony);
    SPRAWDZ(wLogu(p, "w pytaniu 1.0.1, teraz najnowsza 1.1.0-beta.1: pytam ponownie"));
  }
  {  // plik konfiguracji nieczytelny przy włączonym kanale: panel wraca do `releases/latest` (bez wersji próbnej)
    Panel p;
    przygotuj(p, "aktualizacja_wczesna=tak\n", "", "1.0.1", L);
    p.pliki.nieczytelne.insert("/konfiguracja.txt");
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    SPRAWDZ(zadan(p, "/releases/latest") == 1 && zadan(p, "/releases?") == 0);
    SPRAWDZ(u07(p) == stan(aktualizacja::StanU07::NOWSZA));
    bool slad = false;  // czy log mówi, że ustawienie kanału jest nieznane
    for (auto& l : p.port.log) slad = slad || (l.find("wczesn") != std::string::npos && l.find("nieczyt") != std::string::npos);
    printf("  konfiguracja nieczytelna: kanał wczesny cicho wyłączony, ślad w logu: %s\n", slad ? "tak" : "nie");
  }
  {  // CRLF: panel i formularz zgodnie `tak`
    Panel p;
    przygotuj(p, "aktualizacja_wczesna=tak\r\n", "", "1.1.0-beta.1", L);
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    sekrety::Dane ob;
    SPRAWDZ(zadan(p, "/releases?") == 1 && formularz::zPliku("aktualizacja_wczesna=tak\r\n", ob).aktualizacjaWczesna == "tak");
  }
  for (const char* konf : {"aktualizacja_wczesna=tak\naktualizacja_wczesna=nie\n",
                           "aktualizacja_wczesna=nie\naktualizacja_wczesna=tak\n"}) {  // duplikat klucza
    Panel p;
    przygotuj(p, konf, "", "1.1.0-beta.1", L);
    przyciskiem(p, ep(2026, 11, 23, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
    wybudz(p);
    sekrety::Dane ob;
    bool panelWl = zadan(p, "/releases?") == 1;
    bool formWl = formularz::zPliku(konf, ob).aktualizacjaWczesna == "tak";
    printf("  duplikat „%s”: panel %s, formularz %s\n", konf[21] == 't' ? "tak,nie" : "nie,tak", panelWl ? "włączony" : "wyłączony",
           formWl ? "tak" : "nie");
    // (P3): przy `tak` przed `nie` panel włączony, formularz pokazuje `nie` (formularz: ostatni wiersz, panel:
    // którykolwiek). Wynik wypisany, bez SPRAWDZ do czasu decyzji o poprawce; po poprawce zamienić na
    // SPRAWDZ(panelWl == formWl).
    SPRAWDZ(panelWl);
  }
}

// ---------- 4. formularz: koszt pola w bajtach strony ----------
static void brz_formularz() {
  sekrety::Dane ob;
  std::string s = formularz::strona(formularz::zPliku("", ob), "/u/abc/", "", {});
  size_t a = s.find("<label for=\"aktualizacja_wczesna\""), b = s.find("<label for=\"klucz_wydan\"");
  SPRAWDZ(a != std::string::npos && b != std::string::npos && a < b);
  printf("  pole kanału wczesnego na stronie: %zu B (strona bez błędów %zu B)\n", b - a, s.size());
  SPRAWDZ(b - a < 500);
  // tekst prosty, bez żargonu (pre-release, GitHub, kanał)
  std::string pole = s.substr(a, b - a);
  SPRAWDZ(pole.find("pre-release") == std::string::npos && pole.find("kanał") == std::string::npos);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  brz_semver();
  brz_rozmiar();
  brz_dyrygent();
  brz_formularz();
  printf("testy_kanal_wczesny_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
