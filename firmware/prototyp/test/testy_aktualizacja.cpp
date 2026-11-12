// SPDX-License-Identifier: GPL-3.0-or-later
// Testy rdzenia aktualizacji z wydań GitHub (ADR-0032, ADR-0038):
// porównanie wersji, mini JSON, odpowiedź `releases/latest`, opis `wydanie.json`, SHA-256 (wektory FIPS 180-4),
// decyzja „czy instalować”, reguły automatu, stan w NVS, wynik po restarcie, teksty U-07, U-01, U-05, token tylko do
// api.github.com, nawigacja U-01 → U-07, ikona w stopce, sekrety (token, hasło okna, dodatki) i formularz U-03.
// Funkcje testów mają ID wymagania w nazwie (spojnosc_tech.py, sprawdzenie k). Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (program testy_aktualizacja).
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "aktualizacja.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "formularz.h"
#include "magazyn.h"
#include "nawigacja.h"
#include "sekrety.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

using namespace aktualizacja;

static bool zawiera(const std::string& t, const std::string& x) { return t.find(x) != std::string::npos; }

// ---------- wersje (R-NF-34: tylko wersja wyższa) ----------
static void testR_NF_34_porownanieWersji() {
  SPRAWDZ(nowsza("1.0.1", "1.0.0"));
  SPRAWDZ(nowsza("v1.0.0", "1.0.0-beta.1"));  // wydanie wyżej od bety
  SPRAWDZ(nowsza("1.0.0-beta.2", "1.0.0-beta.1"));
  SPRAWDZ(nowsza("1.0.0-beta.10", "1.0.0-beta.9"));  // liczbowo, nie tekstowo
  SPRAWDZ(nowsza("1.0.0-rc.1", "1.0.0-beta.11"));
  SPRAWDZ(nowsza("1.0.0-beta.1.1", "1.0.0-beta.1"));  // dłuższy zestaw wyżej
  SPRAWDZ(nowsza("1.0.0-beta", "1.0.0-1"));            // tekst wyżej od liczby
  SPRAWDZ(nowsza("2.0.0", "1.99.99"));
  SPRAWDZ(nowsza("1.10.0", "1.9.0"));
  SPRAWDZ(!nowsza("1.0.0", "1.0.0"));                   // równa
  SPRAWDZ(!nowsza("1.0.0+zzz", "1.0.0+abc"));           // budowa bez znaczenia
  SPRAWDZ(!nowsza("0.9.9", "1.0.0"));                   // niższa
  SPRAWDZ(!nowsza("1.0.0-beta.1", "1.0.0"));
  // zły zapis = bez instalacji
  for (const char* z : {"", "1.0", "1.0.0.0", "01.0.0", "1.0.0-", "1.0.0-beta..1", "1.0.0-be ta", "a.b.c", "1.0.0-01",
                        "1.0.0+", "1234567.0.0", "-1.0.0", "1.0.0-ą"})
    SPRAWDZ(!nowsza(z, "0.0.1"));
  SPRAWDZ(!nowsza("1.0.1", "zepsuta"));
  SPRAWDZ(bezBudowy("v1.0.1+abc1234") == "1.0.1");
  SPRAWDZ(bezBudowy("1.0.0-beta.1+abc") == "1.0.0-beta.1");
  Wersja w;
  SPRAWDZ(parsujWersje("1.0.0-beta.1+abc1234", w) && w.glowna == 1 && w.przed.size() == 2 && w.przed[0] == "beta");
}

// ---------- JSON ----------
static void testJson() {
  aktualizacja::Json j;
  SPRAWDZ(parsujJson(R"( {"a":[1,2,{"b":null}],"c":"x\u0142\"\\\n","d":true,"e":-1.5e3} )", j));
  SPRAWDZ(j.pole("c") && j.pole("c")->tekst == "x\xc5\x82\"\\\n");
  SPRAWDZ(j.pole("a") && j.pole("a")->elementy.size() == 3);
  SPRAWDZ(parsujJson(R"("\ud83d\ude00")", j) && j.tekst == "\xf0\x9f\x98\x80");  // para zastępcza
  for (const char* z : {"", "{", "{\"a\":}", "[1,]", "{\"a\":1,}", "\"\\x\"", "01", "1.", "tru", "{} x", "\"\\ud800\"",
                        "\"a\nb\""})
    SPRAWDZ(!parsujJson(z, j));
  std::string gleboko(40, '[');
  gleboko += std::string(40, ']');
  SPRAWDZ(!parsujJson(gleboko, j));  // zagnieżdżenie ponad limit
  uint64_t v = 0;
  aktualizacja::Json n;
  SPRAWDZ(parsujJson("123", n) && liczbaZJson(&n, 1000, v) && v == 123);
  SPRAWDZ(!liczbaZJson(&n, 100, v));
  SPRAWDZ(parsujJson("-5", n) && !liczbaZJson(&n, 1000, v));
  SPRAWDZ(parsujJson("1.5", n) && !liczbaZJson(&n, 1000, v));
  SPRAWDZ(parsujJson("99999999999999999999999", n) && !liczbaZJson(&n, UINT64_MAX, v));
}

// ---------- odpowiedź GitHub i opis wydania ----------
static std::string opisPrzyklad(const std::string& wersja = "1.0.1") {
  OpisWydania o;
  o.wersja = wersja;
  o.plytka = PLYTKA_WYDANIA;
  o.uklad = 0x1a2b3c4d;
  o.rozmiar = 1700000;
  o.sha256 = std::string(64, 'a');
  o.plik = nazwaObrazu(wersja);
  return zapiszOpis(o);
}
static void testR_NF_32_odpowiedzGitHubIOpis() {
  Dziennik d;
  AtrapaZegara z(d);
  AtrapaWydan aw(d, z);
  aw.ustaw("1.0.1", "OBRAZ", 0x11223344);
  Wydanie w;
  SPRAWDZ(parsujWydanie(aw.odpowiedzApi, w));
  SPRAWDZ(w.tag == "v1.0.1" && w.opis.nazwa == "wydanie.json" && w.obraz.nazwa == "cpap-eink-1.0.1.bin");
  SPRAWDZ(w.obraz.rozmiar == 5 && wolnoWyslacToken(w.opis.adresApi) && wolnoWyslacToken(w.obraz.adresApi));
  // zasób z adresem spoza api.github.com (np. browser_download_url) nie jest brany (token, R-NF-32)
  SPRAWDZ(!parsujWydanie(R"({"tag_name":"v1.0.1","assets":[{"name":"wydanie.json","url":"https://evil.example/x"}]})", w));
  SPRAWDZ(!parsujWydanie(R"({"assets":[]})", w));
  SPRAWDZ(!parsujWydanie("[]", w));
  OpisWydania o;
  SPRAWDZ(parsujOpis(opisPrzyklad(), o) && o.wersja == "1.0.1" && o.uklad == 0x1a2b3c4d && o.rozmiar == 1700000);
  SPRAWDZ(parsujOpis(aw.opis, o) && o.sha256 == sha256("OBRAZ") && o.uklad == 0x11223344);
  std::string dobry = opisPrzyklad();
  auto zmien = [&](const std::string& z, const std::string& na) {
    std::string t = dobry;
    size_t p = t.find(z);
    return p == std::string::npos ? std::string("x") : t.replace(p, z.size(), na);
  };
  SPRAWDZ(!parsujOpis(zmien("\"1.0.1\"", "\"1.0\""), o));                    // zła wersja
  SPRAWDZ(!parsujOpis(zmien(std::string(64, 'a'), std::string(63, 'a')), o));  // skrót za krótki
  SPRAWDZ(!parsujOpis(zmien(std::string(64, 'a'), std::string(64, 'A')), o));  // wielkie litery
  SPRAWDZ(!parsujOpis(zmien("1700000", "0"), o));
  SPRAWDZ(!parsujOpis(zmien("1700000", "99999999999"), o));
  SPRAWDZ(!parsujOpis(zmien("0x1a2b3c4d", "0x0"), o));
  SPRAWDZ(!parsujOpis(zmien("0x1a2b3c4d", "0xzz"), o));
  SPRAWDZ(!parsujOpis(zmien(",\"plik\"", ",\"pliki\""), o));
  SPRAWDZ(!parsujOpis(std::string(5000, ' ') + dobry, o));  // ponad MAKS_OPISU
}

// ---------- SHA-256 (FIPS 180-4, przykłady NIST) ----------
static void testR_NF_34_sha256() {
  SPRAWDZ(sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  SPRAWDZ(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  SPRAWDZ(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  std::string milion(1000000, 'a');
  SPRAWDZ(sha256(milion) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  // w locie kawałkami różnej długości = całość naraz
  Sha256 s;
  for (size_t i = 0; i < milion.size();) {
    size_t n = 1 + (i % 97);
    if (i + n > milion.size()) n = milion.size() - i;
    s.dodaj((const uint8_t*)milion.data() + i, n);
    i += n;
  }
  SPRAWDZ(s.hex() == sha256(milion));
}

// ---------- adresy i token (R-NF-32, R-NF-36) ----------
static void testR_NF_36_tokenTylkoDoApi() {
  SPRAWDZ(hostZAdresu("https://API.github.com/repos/x") == "api.github.com");
  SPRAWDZ(hostZAdresu("https://api.github.com:443/x") == "api.github.com");
  SPRAWDZ(hostZAdresu("http://api.github.com/x").empty());  // bez TLS: nic
  SPRAWDZ(hostZAdresu("https://api.github.com@evil.example/x").empty());
  SPRAWDZ(wolnoWyslacToken("https://api.github.com/repos/a/b/releases/latest"));
  SPRAWDZ(!wolnoWyslacToken("https://objects.githubusercontent.com/x"));
  SPRAWDZ(!wolnoWyslacToken("https://api.github.com.evil.example/x"));
  SPRAWDZ(!wolnoWyslacToken("http://api.github.com/x"));
  auto h = naglowki("https://api.github.com/x", "github_pat_ZNACZNIK", true);
  bool auth = false, accept = false;
  for (auto& p : h) {
    if (p.first == "Authorization") auth = p.second == "Bearer github_pat_ZNACZNIK";
    if (p.first == "Accept") accept = p.second == "application/octet-stream";
  }
  SPRAWDZ(auth && accept);
  for (auto& p : naglowki("https://objects.githubusercontent.com/x", "github_pat_ZNACZNIK", true))
    SPRAWDZ(p.first != "Authorization" && !zawiera(p.second, "ZNACZNIK"));
  for (auto& p : naglowki("https://api.github.com/x", "", false)) SPRAWDZ(p.first != "Authorization");
  SPRAWDZ(adresNajnowszego("wlasciciel/wydania") ==
          "https://api.github.com/repos/wlasciciel/wydania/releases/latest");
  SPRAWDZ(adresNajnowszego(REPO_WYDAN) == "https://api.github.com/repos/rafmyr/cpapblog-eink/releases/latest");
  for (const char* z : {"", "rafmyr", "/x", "a/", "a/b/c", "a/b?c", "a b/c", "a/b#"}) SPRAWDZ(adresNajnowszego(z).empty());
  // wzory tokenów składane w czasie działania, żeby skanery sekretów nie brały atrapy za prawdziwy token
  SPRAWDZ(tokenOk(std::string("github_pat") + "_11ABCDEFG0123456789_abcdefghijklmnopqrstuvwxyz"));
  SPRAWDZ(tokenOk(std::string("ghp") + "_0123456789abcdefghij0123456789abcdef"));
  SPRAWDZ(!tokenOk("krotki") && !tokenOk("github_pat_ze spacją_0123456789") && !tokenOk(std::string(256, 'a')));
  SPRAWDZ(!tokenOk("github_pat_0123456789\n0123456789"));
}

// ---------- decyzja instalacji (R-NF-34, R-KONF-45) ----------
static void testR_NF_34_decyzjaInstalacji() {
  OpisWydania o;
  o.wersja = "1.0.1";
  o.plytka = PLYTKA_WYDANIA;
  o.uklad = 7;
  o.rozmiar = 1000;
  DanePanelu p;
  p.wersja = "1.0.0";
  p.uklad = 7;
  p.procentBaterii = 80;
  SPRAWDZ(decyzjaInstalacji(o, p) == Powod::BRAK);
  DanePanelu q = p;
  q.wersja = "1.0.1";
  SPRAWDZ(decyzjaInstalacji(o, q) == Powod::NIE_NOWSZA);  // równa
  q.wersja = "1.1.0";
  SPRAWDZ(decyzjaInstalacji(o, q) == Powod::NIE_NOWSZA);  // niższa
  OpisWydania r = o;
  r.plytka = "inna_plytka";
  SPRAWDZ(decyzjaInstalacji(r, p) == Powod::INNY_PANEL);
  r = o;
  r.uklad = 8;
  SPRAWDZ(decyzjaInstalacji(r, p) == Powod::INNY_PANEL);
  r = o;
  r.rozmiar = SLOT_APLIKACJI + 1;
  SPRAWDZ(decyzjaInstalacji(r, p) == Powod::INNY_PANEL);
  r.rozmiar = SLOT_APLIKACJI;
  SPRAWDZ(decyzjaInstalacji(r, p) == Powod::BRAK);  // dokładnie slot
  q = p;
  q.procentBaterii = PROG_BATERII - 1;
  SPRAWDZ(decyzjaInstalacji(o, q) == Powod::BATERIA);
  q.procentBaterii = PROG_BATERII;
  SPRAWDZ(decyzjaInstalacji(o, q) == Powod::BRAK);
  q.procentBaterii = 10;
  q.zKabla = true;
  SPRAWDZ(decyzjaInstalacji(o, q) == Powod::BRAK);
  q.zKabla = false;
  q.procentBaterii = -1;  // brak odczytu nie blokuje (drugi slot, zanik zasilania nie psuje bieżącej wersji)
  SPRAWDZ(decyzjaInstalacji(o, q) == Powod::BRAK);
  SPRAWDZ(nocAlboTerapia(23 * 60 + 30, 0) && nocAlboTerapia(4 * 60 + 59, 0) && !nocAlboTerapia(5 * 60, 0));
  SPRAWDZ(nocAlboTerapia(12 * 60, 1) && !nocAlboTerapia(23 * 60 + 29, 0));
}

// ---------- automat (R-KONF-45, karta 2 pkt 6) ----------
static void testR_KONF_45_automat() {
  Stan s;
  WejscieAutomatu w;
  w.wlaczony = true;
  w.dzis = "2026-11-10";
  w.minutaDoby = 7 * 60;
  w.rop = 0;
  SPRAWDZ(decyzjaAutomatu(s, "1.0.1", w) == PowodAutomatu::TAK);
  WejscieAutomatu x = w;
  x.wlaczony = false;
  SPRAWDZ(decyzjaAutomatu(s, "1.0.1", x) == PowodAutomatu::WYLACZONY);
  x = w;
  x.minutaDoby = 23 * 60 + 45;
  SPRAWDZ(decyzjaAutomatu(s, "1.0.1", x) == PowodAutomatu::NOC);
  x.minutaDoby = 3 * 60;
  SPRAWDZ(decyzjaAutomatu(s, "1.0.1", x) == PowodAutomatu::NOC);
  x = w;
  x.rop = 1;
  SPRAWDZ(decyzjaAutomatu(s, "1.0.1", x) == PowodAutomatu::NOC);
  x = w;
  x.sprawaAB = true;
  SPRAWDZ(decyzjaAutomatu(s, "1.0.1", x) == PowodAutomatu::SPRAWA_AB);
  SPRAWDZ(decyzjaAutomatu(s, "", w) == PowodAutomatu::BRAK_WERSJI);
  // najwyżej jedna próba na dobę, po 3 nieudanych tej samej wersji pominięcie do następnego wydania
  Stan p;
  zacznijProbe(p, "1.0.1", "1.0.0", "2026-11-10", true);
  nieudanaProba(p, Powod::USZKODZONY);
  SPRAWDZ(p.probNieudanych == 1 && decyzjaAutomatu(p, "1.0.1", w) == PowodAutomatu::DZIS_BYLA_PROBA);
  w.dzis = "2026-11-11";
  SPRAWDZ(decyzjaAutomatu(p, "1.0.1", w) == PowodAutomatu::TAK);
  zacznijProbe(p, "1.0.1", "1.0.0", "2026-11-11", true);
  nieudanaProba(p, Powod::USZKODZONY);
  w.dzis = "2026-11-12";
  zacznijProbe(p, "1.0.1", "1.0.0", "2026-11-12", true);
  nieudanaProba(p, Powod::USZKODZONY);
  w.dzis = "2026-11-13";
  SPRAWDZ(p.probNieudanych == 3 && decyzjaAutomatu(p, "1.0.1", w) == PowodAutomatu::POMINIETA);
  SPRAWDZ(decyzjaAutomatu(p, "1.0.2", w) == PowodAutomatu::TAK);  // następne wydanie: od nowa
  zacznijProbe(p, "1.0.2", "1.0.0", "2026-11-13", true);
  SPRAWDZ(p.probNieudanych == 1 && p.probaWersja == "1.0.2");
}

// ---------- R-KONF-46: raz na dobę, w oknie Wi-Fi, z tokenem i czasem ----------
static void testR_KONF_46_sprawdzicDzis() {
  Stan s;
  SPRAWDZ(sprawdzicDzis(s, "2026-11-10", true, true, true, 7 * 60));
  SPRAWDZ(!sprawdzicDzis(s, "2026-11-10", false, true, true, 7 * 60));  // bez Wi-Fi z innego powodu
  SPRAWDZ(!sprawdzicDzis(s, "2026-11-10", true, false, true, 7 * 60));  // R-NF-33
  SPRAWDZ(!sprawdzicDzis(s, "2026-11-10", true, true, false, 7 * 60));  // bez czasu
  SPRAWDZ(!sprawdzicDzis(s, "2026-11-10", true, true, true, 2 * 60));   // noc
  SPRAWDZ(!sprawdzicDzis(s, "", true, true, true, 7 * 60));
  poSprawdzeniu(s, "1.0.1", "1.0.0", 1000, "2026-11-10");
  SPRAWDZ(s.znana == "1.0.1" && s.sprawdzonoEp == 1000);
  SPRAWDZ(!sprawdzicDzis(s, "2026-11-10", true, true, true, 12 * 60));  // drugi raz tego dnia: nie
  SPRAWDZ(sprawdzicDzis(s, "2026-11-11", true, true, true, 6 * 60));
  poSprawdzeniu(s, "1.0.0", "1.0.0", 2000, "");  // ręczne sprawdzenie bez nowszej: znana znika, doba bez zmian
  SPRAWDZ(s.znana.empty() && s.sprawdzonoDnia == "2026-11-10");
}

// ---------- stan w NVS i wynik po restarcie (R-NF-35, R-KONF-44) ----------
static void testR_NF_35_stanPoStarcie() {
  Dziennik d;
  AtrapaNvs n(d);
  Stan s = wczytaj(n);
  SPRAWDZ(s.znana.empty() && s.instalujeNa.empty() && s.ostatni == Powod::BRAK);
  s.znana = "1.0.1";
  zacznijProbe(s, "1.0.1", "1.0.0+abc", "2026-11-10", false);
  zapisz(n, s);
  Stan t = wczytaj(n);
  SPRAWDZ(t.instalujeZ == "1.0.0" && t.instalujeNa == "1.0.1" && t.probNieudanych == 1 && !t.automatyczna);
  // nowa wersja wstała
  Stan u = t;
  SPRAWDZ(poStarcie(u, "1.0.1+def") == PoStarcie::ZAINSTALOWANO);
  SPRAWDZ(u.znana.empty() && u.probNieudanych == 0 && u.instalujeNa.empty() && u.ostatni == Powod::BRAK);
  SPRAWDZ(poStarcie(u, "1.0.1") == PoStarcie::NIC);  // drugi start: bez wyniku
  // nowa nie wstała: działa wersja sprzed instalacji
  Stan v = t;
  SPRAWDZ(poStarcie(v, "1.0.0+abc") == PoStarcie::NIE_WSTALA);
  SPRAWDZ(v.ostatni == Powod::NIE_WSTALA && v.odrzucona == "1.0.1" && v.probNieudanych == 1 && v.instalujeNa.empty());
  SPRAWDZ(dopisekU05(v) == " · odrzucona 1.0.1");
  // inna wersja (wgranie kablem): instalacja zapomniana bez wyniku
  Stan x = t;
  SPRAWDZ(poStarcie(x, "2.0.0") == PoStarcie::NIC && x.instalujeNa.empty());
  // zakres liczby z NVS
  n.ustawLiczbeU(PRZESTRZEN, "pr_n", 4000000000u);
  n.ustawLiczbeU(PRZESTRZEN, "wynik", 200);
  Stan y = wczytaj(n);
  SPRAWDZ(y.probNieudanych == 100 && y.ostatni == Powod::BRAK);
}

// ---------- teksty U-07, U-01, U-05 (karta 1.1–1.3) ----------
static void testR_KONF_44_teksty() {
  DaneU07 d;
  d.wlasna = "1.0.0";
  d.nowa = "1.0.1";
  d.stan = StanU07::SPRAWDZANIE;
  TresciU07 t = trescU07(d);
  SPRAWDZ(t.linie[0] == "Sprawdzam, czy jest nowa wersja…" && t.stopka == std::vector<std::string>{"↻ ekran dnia"});
  d.stan = StanU07::AKTUALNA;
  d.sprawdzono = "10.11 07:12";
  t = trescU07(d);
  SPRAWDZ(t.linie.size() == 2 && t.linie[0] == "Masz aktualną wersję 1.0.0." && t.linie[1] == "Sprawdzono 10.11 07:12.");
  SPRAWDZ((t.stopka == std::vector<std::string>{"◀ ustawienia", "↻ ekran dnia"}));
  d.stan = StanU07::NOWSZA;
  t = trescU07(d);
  SPRAWDZ(t.linie[0] == "Jest wersja 1.0.1. Masz 1.0.0." && t.linie[1] == "Zainstalować teraz? Potrwa około 3 minut." &&
          t.linie[2] == "Nie odłączaj zasilania. Dane z nocy zostają.");
  SPRAWDZ((t.stopka == std::vector<std::string>{"◀ nie teraz", "▶ tak, instaluj", "↻ ekran dnia"}));
  d.stan = StanU07::POBIERANIE;
  t = trescU07(d);
  SPRAWDZ(t.linie[0] == "Pobieram wersję 1.0.1…" && t.linie[1] == "Nie odłączaj zasilania." && t.stopka.empty());
  d.stan = StanU07::INSTALACJA;
  t = trescU07(d);
  SPRAWDZ(t.linie[0] == "Instaluję. Panel zaraz uruchomi się ponownie." && t.stopka.empty());
  d.stan = StanU07::SAMOSPRAWDZENIE;  // po restarcie, przed potwierdzeniem nowej wersji
  t = trescU07(d);
  SPRAWDZ(t.linie.size() == 1 && t.linie[0] == "Sprawdzam nową wersję…" && t.stopka.empty());
  d.stan = StanU07::PO_UDANEJ;
  t = trescU07(d);
  SPRAWDZ(t.linie[0] == "Zainstalowano wersję 1.0.1.");
  d.stan = StanU07::PO_NIEUDANEJ;
  d.powod = Powod::BATERIA;
  d.procentBaterii = 31;
  t = trescU07(d);
  SPRAWDZ(t.linie[0] == "Aktualizacja się nie udała. Zostaje wersja 1.0.0." &&
          t.linie[1] == "Bateria 31%. Podłącz zasilanie i spróbuj ponownie.");
  SPRAWDZ((t.stopka == std::vector<std::string>{"◀ ustawienia", "▶ spróbuj ponownie", "↻ ekran dnia"}));
  // każdy powód z karty 1.3
  SPRAWDZ(tekstPowodu(Powod::BRAK_SIECI, -1, "1.0.0") == "Nie mogę sprawdzić wersji. Brak połączenia z internetem.");
  SPRAWDZ(tekstPowodu(Powod::BRAK_TOKENU, -1, "1.0.0") == "Aktualizacje wyłączone. Ustaw je w „Więcej na telefonie”.");
  SPRAWDZ(tekstPowodu(Powod::TOKEN_ODRZUCONY, -1, "1.0.0") ==
          "GitHub odrzucił klucz wydań. Wpisz nowy w „Więcej na telefonie”.");
  SPRAWDZ(tekstPowodu(Powod::USZKODZONY, -1, "1.0.0") == "Pobrany plik jest uszkodzony. Spróbuj ponownie.");
  SPRAWDZ(tekstPowodu(Powod::INNY_PANEL, -1, "1.0.0") == "Ta wersja nie pasuje do tego panelu. Potrzebny kabel.");
  SPRAWDZ(tekstPowodu(Powod::NIE_WSTALA, -1, "1.0.0") ==
          "Nowa wersja nie uruchomiła się poprawnie. Panel wrócił do 1.0.0.");
  SPRAWDZ(tekstPowodu(Powod::NOC, -1, "1.0.0") == "Teraz nie instaluję: trwa noc. Spróbuj rano.");
  SPRAWDZ(tekstPowodu(Powod::BATERIA, -1, "1.0.0") == "Bateria –%. Podłącz zasilanie i spróbuj ponownie.");
  d.stan = StanU07::WYLACZONE;
  t = trescU07(d);
  SPRAWDZ(t.linie.size() == 1 && t.linie[0] == "Aktualizacje wyłączone. Ustaw je w „Więcej na telefonie”.");
  // U-01 (karta 1.1)
  SPRAWDZ(wierszU01(true, "", "1.0.0+abc") == "Oprogramowanie 1.0.0 · ↻ 2 s Sprawdź aktualizację");
  SPRAWDZ(wierszU01(true, "1.0.1", "1.0.0") == "Jest wersja 1.0.1 · ↻ 2 s Aktualizacja");
  SPRAWDZ(wierszU01(true, "1.0.0", "1.0.0") == "Oprogramowanie 1.0.0 · ↻ 2 s Sprawdź aktualizację");
  SPRAWDZ(wierszU01(false, "1.0.1", "1.0.0") == "Aktualizacje wyłączone. Ustaw je w „Więcej na telefonie”.");
  // U-05 (ADR-0032 pkt 2)
  Stan s;
  s.znana = "1.0.1";
  SPRAWDZ(dopisekU05(s) == " · jest wersja 1.0.1");
  SPRAWDZ(dopisekU05(Stan()).empty());
}

// ---------- nawigacja U-01 ↻ 2 s → U-07 (R-KONF-43, R-KONF-44, spec 5.6.1) ----------
static void testR_KONF_43_nawigacja() {
  KontekstNaw k;
  Przejscie p = nacisniecie(Ekran::USTAWIENIA, Przycisk::SRODEK, true, k);
  SPRAWDZ(p.ekran == Ekran::AKTUALIZACJA && p.akcja == Akcja::AKT_SPRAWDZ);
  p = nacisniecie(Ekran::USTAWIENIA, Przycisk::SRODEK, false, k);  // krótkie ↻: ekran dnia bez sprawdzenia
  SPRAWDZ(p.ekran == Ekran::DZIEN && p.akcja == Akcja::POKAZ);
  // U-07 „jest nowsza”: ▶ instaluje, ◀ nie teraz → U-01, ↻ dom, przytrzymania nic
  k.aktualizacja = 1;
  SPRAWDZ(nacisniecie(Ekran::AKTUALIZACJA, Przycisk::PRAWY, false, k).akcja == Akcja::AKT_INSTALUJ);
  SPRAWDZ(nacisniecie(Ekran::AKTUALIZACJA, Przycisk::LEWY, false, k).ekran == Ekran::USTAWIENIA);
  SPRAWDZ(nacisniecie(Ekran::AKTUALIZACJA, Przycisk::SRODEK, false, k).ekran == Ekran::DZIEN);
  for (Przycisk b : {Przycisk::LEWY, Przycisk::PRAWY, Przycisk::SRODEK})
    SPRAWDZ(nacisniecie(Ekran::AKTUALIZACJA, b, true, k).akcja == Akcja::NIC);  // przytrzymanie nie otwiera i nie instaluje
  k.aktualizacja = 2;
  SPRAWDZ(nacisniecie(Ekran::AKTUALIZACJA, Przycisk::PRAWY, false, k).akcja == Akcja::AKT_PONOW);
  k.aktualizacja = 0;  // aktualna, po udanej, wyłączone: ▶ nic
  SPRAWDZ(nacisniecie(Ekran::AKTUALIZACJA, Przycisk::PRAWY, false, k).akcja == Akcja::NIC);
  SPRAWDZ(numerStrony(Ekran::AKTUALIZACJA) == 0 && !zKolejkiWolno(Ekran::AKTUALIZACJA, false));
  SPRAWDZ(ekranZRtc(17) == Ekran::AKTUALIZACJA && ekranZRtc(19) == Ekran::DZIEN);
  // 5 min bez wyboru: U-01, bez instalacji (R-KONF-44); w nocy zostaje (R-PRZ-18)
  SPRAWDZ(sekundDoPowrotu(Ekran::AKTUALIZACJA, 299, 9 * 60) == 1 && sekundDoPowrotu(Ekran::AKTUALIZACJA, 300, 9 * 60) == 0);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::AKTUALIZACJA, true, U07_CZAS_S, 9 * 60) == Ekran::USTAWIENIA);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::AKTUALIZACJA, true, 100, 9 * 60) == Ekran::AKTUALIZACJA);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::AKTUALIZACJA, true, 900, 1 * 60) == Ekran::AKTUALIZACJA);
}

// ---------- stopki: kolejność ◀ ▶ ↻ i szerokość ≤ 744 px (R-PRZ-04) ----------
static void testR_KONF_43_stopki() {
  std::vector<std::string> u01 = stopkaU01();
  SPRAWDZ((u01 == std::vector<std::string>{"◀ 7 nocy · 2 s sieć", "▶ 2 s telefon", "↻ ekran dnia · 2 s aktualizacja"}));
  std::vector<std::string> u = u01;
  uporzadkujPrzyciski(u);
  SPRAWDZ(u == u01);
  SPRAWDZ(szerStopki(u01) <= 744);
  DaneU07 d;
  for (int st = 0; st <= (int)StanU07::WYLACZONE; st++) {
    d.stan = (StanU07)st;
    d.wlasna = "1.0.0-beta.1";
    d.nowa = "1.0.0-beta.12";
    TresciU07 t = trescU07(d);
    std::vector<std::string> g = t.stopka;
    uporzadkujPrzyciski(g);
    SPRAWDZ(g == t.stopka);
    SPRAWDZ(szerStopki(t.stopka) <= 744);
  }
}

// ---------- R-EKR-61: ikona w stopce ----------
static int czarneW(const PlotnoAtrapy& p, int x0, int y0, int x1, int y1) {
  int n = 0;
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) n += (p.b[(size_t)(y * 100 + x / 8)] >> (7 - (x & 7))) & 1;
  return n;
}
static void testR_EKR_61_ikonaStopki() {
  PlotnoAtrapy bez, z;
  stopkaEkranu(bez, {}, "przy łóżku 21,5° · 48%");
  {
    IkonaStopki ik(true);
    SPRAWDZ(prawyBrzegStopki() == 772 - BOK_IKONY_AKTUALIZACJI - ODSTEP_IKONY_AKTUALIZACJI);
    stopkaEkranu(z, {}, "przy łóżku 21,5° · 48%");
    SPRAWDZ(ikonaAktualizacji(z, false));
  }
  SPRAWDZ(!ikonaAktualizacjiWlaczona() && prawyBrzegStopki() == 772);  // RAII: po rysowaniu wyłączona
  int ix = 772 - BOK_IKONY_AKTUALIZACJI;
  SPRAWDZ(czarneW(z, ix, 437, 772, 480) > 200 && czarneW(bez, ix, 437, 772, 480) > 0);  // bez ikony tu był tekst
  SPRAWDZ(czarneW(z, 773, 437, 800, 480) == 0);  // nic poza marginesem
  // tekst wyrównany do prawej przesunięty przed ikonę (bez nachodzenia)
  SPRAWDZ(czarneW(z, ix - ODSTEP_IKONY_AKTUALIZACJI, 437, ix, 480) == 0);
  // za długa treść stopki skraca się, ikona zostaje cała
  PlotnoAtrapy dl;
  {
    IkonaStopki ik(true);
    stopkaEkranu(dl, {"◀ bardzo długi podpis przycisku stopki", "▶ jeszcze dłuższy podpis drugiego przycisku"}, "");
    ikonaAktualizacji(dl, false);
  }
  SPRAWDZ(czarneW(dl, ix, 437, 772, 480) == czarneW(z, ix, 437, 772, 480));
  // treść skracana do pełnych słów, zanim ikona ją przykryje: bez nakładki nic nie wchodzi w miejsce ikony
  PlotnoAtrapy bezNakladki;
  {
    IkonaStopki ik(true);
    stopkaEkranu(bezNakladki, {"◀ bardzo długi podpis przycisku stopki", "▶ jeszcze dłuższy podpis drugiego przycisku"}, "");
    PlotnoAtrapy b2;
    stopkaEkranu(b2, {}, "przy łóżku 21,5° · 48% · bardzo długi dopisek stopki z wieloma słowami do skrócenia");
    SPRAWDZ(czarneW(b2, ix - ODSTEP_IKONY_AKTUALIZACJI, 437, 800, 480) == 0);
  }
  SPRAWDZ(czarneW(bezNakladki, ix - ODSTEP_IKONY_AKTUALIZACJI, 437, 800, 480) == 0);
  SPRAWDZ(czarneW(bezNakladki, 28, 437, 300, 480) > 0);  // początek treści został
  SPRAWDZ(skrocDoSlow(F_P22B, "jedno dwa trzy", 10000) == "jedno dwa trzy");
  SPRAWDZ(skrocDoSlow(F_P22B, "jedno dwa trzy", szer(F_P22B, "jedno dwa") + 1) == "jedno dwa");
  SPRAWDZ(skrocDoSlow(F_P22B, "jedno · dwa", szer(F_P22B, "jedno ·") + 1) == "jedno");  // bez `·` na końcu
  SPRAWDZ(skrocDoSlow(F_P22B, "jedno", 2).empty());
  // bez włączenia nic nie rysuje
  PlotnoAtrapy n;
  SPRAWDZ(!ikonaAktualizacji(n, false) && czarneW(n, 0, 0, 800, 480) == 0);
  Stan s;
  s.znana = "1.0.1";
  SPRAWDZ(ikonaAktualizacji(s, "1.0.0"));  // jeden warunek, także przy automacie
  SPRAWDZ(!ikonaAktualizacji(s, "1.0.1") && !ikonaAktualizacji(Stan(), "1.0.0"));
}

// ---------- ekran U-07 (rysowanie bez wyjścia poza ekran, bez tokenu) ----------
static void testR_KONF_44_rysowanieU07() {
  DaneU07 d;
  d.wlasna = "1.0.0-beta.1";
  d.nowa = "1.0.1";
  d.powod = Powod::TOKEN_ODRZUCONY;
  for (int st = 0; st <= (int)StanU07::WYLACZONE; st++) {
    d.stan = (StanU07)st;
    TresciU07 t = trescU07(d);
    PlotnoAtrapy p;
    rysujAktualizacje(p, t.linie, t.stopka);
    SPRAWDZ(czarneW(p, 0, 60, 800, 430) > 0);
    SPRAWDZ(czarneW(p, 0, 437, 800, 480) > 0 || t.stopka.empty());
  }
}

// ---------- sekrety: token, hasło okna, dodatki (R-NF-36) ----------
static void testR_NF_36_sekrety() {
  Dziennik d;
  AtrapaNvs n(d);
  SPRAWDZ(sekrety::kluczWydan(n).empty());
  SPRAWDZ(sekrety::zapiszKluczWydan(n, "github_pat_ZNACZNIK_0123456789"));
  SPRAWDZ(sekrety::kluczWydan(n) == "github_pat_ZNACZNIK_0123456789");
  SPRAWDZ(sekrety::zapiszKluczWydan(n, "") && sekrety::kluczWydan(n).empty());
  // hasło okna: z kompilacji jako wartość początkowa („k”), formularz („f”) wygrywa, obraz bez sekretów nic nie zmienia
  Dziennik d2;
  AtrapaNvs m(d2);
  SPRAWDZ(sekrety::wczytajHasloOkna(m, "").empty() && !sekrety::hasloOknaUstawione(m));
  SPRAWDZ(sekrety::wczytajHasloOkna(m, "haslo-z-kompilacji-16zn") == "haslo-z-kompilacji-16zn");
  SPRAWDZ(sekrety::wczytajHasloOkna(m, "", false) == "haslo-z-kompilacji-16zn");  // obraz wydania: hasło z NVS
  SPRAWDZ(sekrety::hasloOknaUstawione(m));
  {  // obraz z secrets.h bez OTA_PASS wyłącza okno (hasło z kompilacji usunięte)
    Dziennik dk;
    AtrapaNvs k(dk);
    sekrety::wczytajHasloOkna(k, "haslo-z-kompilacji-16zn");
    SPRAWDZ(sekrety::wczytajHasloOkna(k, "").empty() && !sekrety::hasloOknaUstawione(k));
  }
  SPRAWDZ(!sekrety::zapiszHasloOkna(m, "krotkie"));
  SPRAWDZ(!sekrety::zapiszHasloOkna(m, std::string("haslo\nz-nowa-linia-1")));
  SPRAWDZ(sekrety::zapiszHasloOkna(m, "haslo-z-formularza-1234"));
  SPRAWDZ(sekrety::wczytajHasloOkna(m, "haslo-z-kompilacji-16zn") == "haslo-z-formularza-1234");
  SPRAWDZ(sekrety::wczytajHasloOkna(m, "") == "haslo-z-formularza-1234" && sekrety::hasloOknaUstawione(m));
  SPRAWDZ(sekrety::wczytajHasloOkna(m, "", false) == "haslo-z-formularza-1234");
  // dodatki: niepuste z kompilacji do NVS, puste (obraz wydania) z NVS; współrzędne parą
  Dziennik d3;
  AtrapaNvs q(d3);
  sekrety::Dodatki zk;
  zk.otoUrl = "http://192.168.1.60:8080/otoczenie";
  zk.otoToken = "tok";
  zk.pogodaLat = "50.259700";
  zk.pogodaLon = "19.021700";
  sekrety::Dodatki a = sekrety::wczytajDodatki(q, zk);
  SPRAWDZ(a.otoUrl == zk.otoUrl && a.pogodaLat == "50.259700");
  // obraz wydania (bez sekretów) na NVS z wartościami niczego nie kasuje
  sekrety::Dodatki b = sekrety::wczytajDodatki(q, sekrety::Dodatki(), false);
  SPRAWDZ(b.otoUrl == zk.otoUrl && b.otoToken == "tok" && b.pogodaLat == "50.259700" && b.pogodaLon == "19.021700");
  SPRAWDZ(sekrety::wczytajDodatki(q, sekrety::Dodatki(), false).otoUrl == zk.otoUrl);  // drugie wybudzenie też
  sekrety::Dodatki pol;
  pol.pogodaLat = "1.0";  // obraz wydania z połową pary: dalej NVS
  sekrety::Dodatki c = sekrety::wczytajDodatki(q, pol, false);
  SPRAWDZ(c.pogodaLat == "50.259700" && c.pogodaLon == "19.021700");
  // obraz z secrets.h bez OTO_URL (usunięte) wyłącza źródło otoczenia; bez pary współrzędnych obie puste
  sekrety::Dodatki bezOto;
  bezOto.pogodaLat = "1.0";
  sekrety::Dodatki dd = sekrety::wczytajDodatki(q, bezOto);
  SPRAWDZ(dd.otoUrl.empty() && dd.otoToken.empty() && dd.pogodaLat.empty() && dd.pogodaLon.empty());
  SPRAWDZ(sekrety::wczytajDodatki(q, sekrety::Dodatki(), false).otoUrl.empty());
}

// ---------- formularz U-03: sekcja „Aktualizacje” (karta 1.4, R-NF-36) ----------
static void testR_NF_36_formularz() {
  sekrety::Dane ob;
  ob.ssid = "Dom";
  ob.abAdres = "192.168.1.20";
  formularz::Pola p = formularz::zPliku("aktualizacja_auto=tak\n", ob);
  SPRAWDZ(p.aktualizacjaAuto == "tak");
  SPRAWDZ(formularz::zPliku("aktualizacja_auto=cos\n", ob).aktualizacjaAuto == "nie");
  SPRAWDZ(formularz::zPliku("", ob).aktualizacjaAuto == "nie");
  p.kluczWydan = true;
  std::string st = formularz::strona(p, "/u/t/", "", {});
  SPRAWDZ(zawiera(st, "<legend>Aktualizacje</legend>") && zawiera(st, "Instaluj nowe wersje samodzielnie") &&
          zawiera(st, "Klucz wydań (token GitHub)") && zawiera(st, "placeholder=\"ustawiony\"") &&
          zawiera(st, "usuń klucz") &&
          zawiera(st, "Klucz dostajesz od osoby, która instalowała panel. Bez klucza panel nie pobiera aktualizacji.") &&
          zawiera(st, "Panel instaluje sam tylko w dzień, poza terapią, przy naładowanej baterii. Raz na dobę pyta GitHub "
                      "o nową wersję."));
  SPRAWDZ(!zawiera(formularz::strona(formularz::zPliku("", ob), "/u/t/", "", {}), "placeholder=\"ustawiony\""));
  // walidacja
  auto w = formularz::waliduj(formularz::parsujCialo("aktualizacja_auto=tak&klucz_wydan=github_pat_ZNACZNIK_0123456789"),
                              ob, "2026-11-10");
  SPRAWDZ(w.ok() && w.plik["aktualizacja_auto"] == "tak" && w.kluczWydan == "github_pat_ZNACZNIK_0123456789");
  for (auto& kv : w.plik) SPRAWDZ(!zawiera(kv.second, "ZNACZNIK"));  // token nigdy do pliku konfiguracji
  w = formularz::waliduj(formularz::parsujCialo("aktualizacja_auto=nie&klucz_wydan=&klucz_usun=1"), ob, "2026-11-10");
  SPRAWDZ(w.ok() && w.plik["aktualizacja_auto"].empty() && w.plik.count("aktualizacja_auto") && w.usunKlucz &&
          w.kluczWydan.empty());
  w = formularz::waliduj(formularz::parsujCialo("aktualizacja_auto=moze&klucz_wydan=zly+klucz&haslo_okna=krotkie"), ob,
                         "2026-11-10");
  SPRAWDZ(w.bledy.count("aktualizacja_auto") && w.bledy.count("klucz_wydan") && w.bledy.count("haslo_okna"));
  SPRAWDZ(w.kluczWydan.empty() && w.hasloOkna.empty() && !w.usunKlucz);
  w = formularz::waliduj(formularz::parsujCialo("klucz_wydan=&haslo_okna=nowe-haslo-okna-12345"), ob, "2026-11-10");
  SPRAWDZ(w.ok() && w.kluczWydan.empty() && !w.usunKlucz && w.hasloOkna == "nowe-haslo-okna-12345");
  // wartość tokenu z błędem nie wraca do strony (R-NF-36)
  formularz::Pola pp = formularz::zPliku("", ob);
  std::string bl = formularz::strona(pp, "/u/t/", "", {{"klucz_wydan", "Klucz: od 20 do 255 znaków, same litery, cyfry i znak _."}});
  SPRAWDZ(!zawiera(bl, "zly klucz"));
}


// ---------- logika adaptera WydaniaEsp32 w rdzeniu (przekierowania, kompletność ciała, nadmiar bajtów) ----------
static void testR_NF_36_przekierowaniaAdaptera() {
  const std::string api = "https://api.github.com/repos/a/b/releases/assets/12";
  SPRAWDZ(kodPrzekierowania(301) && kodPrzekierowania(302) && kodPrzekierowania(303) && kodPrzekierowania(307) &&
          kodPrzekierowania(308));
  SPRAWDZ(!kodPrzekierowania(200) && !kodPrzekierowania(304) && !kodPrzekierowania(300) && !kodPrzekierowania(-1));
  SPRAWDZ(nastepnyAdres(api, "https://objects.githubusercontent.com/x?sig=1", 1) == "https://objects.githubusercontent.com/x?sig=1");
  SPRAWDZ(nastepnyAdres(api, "/repos/a/b/releases/assets/13", 1) == "https://api.github.com/repos/a/b/releases/assets/13");
  SPRAWDZ(nastepnyAdres("https://api.github.com:443/x", "/y", 1) == "https://api.github.com:443/y");
  for (const char* z : {"", "http://objects.githubusercontent.com/x", "//evil.example/x", "wzgledny/x",
                        "https://user:pass@evil.example/x", "ftp://x", "https://", "https://a b/x", "https://x/\n"})
    SPRAWDZ(nastepnyAdres(api, z, 1).empty());
  SPRAWDZ(!nastepnyAdres(api, "https://x.example/", PRZEKIEROWAN_MAKS).empty());
  SPRAWDZ(nastepnyAdres(api, "https://x.example/", PRZEKIEROWAN_MAKS + 1).empty());  // pętla przekierowań: stop
  // token po przekierowaniu względnym zostaje tylko na api.github.com
  SPRAWDZ(wolnoWyslacToken(nastepnyAdres(api, "/inny", 1)));
  SPRAWDZ(!wolnoWyslacToken(nastepnyAdres(api, "https://objects.githubusercontent.com/x", 1)));
  // kompletność ciała (HTTP/1.0)
  SPRAWDZ(cialoKompletne(100, 100, false, 0) && !cialoKompletne(100, 99, true, 0) && !cialoKompletne(100, 101, true, 0));
  SPRAWDZ(cialoKompletne(-1, 50, true, 0) && !cialoKompletne(-1, 50, false, 0));  // bez długości: do zamknięcia
  SPRAWDZ(!cialoKompletne(-1, 70000, true, 65536) && cialoKompletne(0, 0, false, 0));
  // nadmiar bajtów obrazu obcięty do rozmiaru z opisu
  SPRAWDZ(ileZapisac(0, 100, 2048) == 100 && ileZapisac(90, 100, 2048) == 10 && ileZapisac(100, 100, 5) == 0 &&
          ileZapisac(120, 100, 5) == 0 && ileZapisac(0, 1u << 20, 2048) == 2048);
  SPRAWDZ(LIMIT_POLACZENIA_MS < 150000 && LIMIT_PRZESTOJU_MS < 150000);  // krócej niż strażnik zadania
  // 404 = brak wydań (nie „aktualna”), nieudane sprawdzenie tylko z powodem sprawdzenia
  SPRAWDZ(tekstPowodu(Powod::BRAK_WYDAN, -1, "1.0.0") == "Nie widzę wydań. Sprawdź klucz w „Więcej na telefonie”.");
  DaneU07 d;
  d.stan = StanU07::SPRAWDZENIE_NIEUDANE;
  d.wlasna = "1.0.0";
  d.powod = Powod::BRAK_WYDAN;
  TresciU07 t = trescU07(d);
  SPRAWDZ(t.linie.size() == 1 && t.linie[0] == "Nie widzę wydań. Sprawdź klucz w „Więcej na telefonie”.");
  SPRAWDZ((t.stopka == std::vector<std::string>{"◀ ustawienia", "▶ spróbuj ponownie", "↻ ekran dnia"}));
  d.powod = Powod::BATERIA;  // nie pasuje do sprawdzenia: brak sieci
  SPRAWDZ(trescU07(d).linie[0] == "Nie mogę sprawdzić wersji. Brak połączenia z internetem.");
  SPRAWDZ(!powodSprawdzenia(Powod::BATERIA) && !powodSprawdzenia(Powod::NOC) && powodSprawdzenia(Powod::BRAK_WYDAN));
  // sprawdzenie nie w trakcie terapii
  Stan s;
  SPRAWDZ(!sprawdzicDzis(s, "2026-11-10", true, true, true, 6 * 60, 1) && sprawdzicDzis(s, "2026-11-10", true, true, true, 6 * 60, 0));
  // bez tokenu bez ikony
  s.znana = "1.0.1";
  SPRAWDZ(!ikonaAktualizacji(s, "1.0.0", false) && ikonaAktualizacji(s, "1.0.0", true));
}

// R-KONF-45, R-KONF-46: zapytania automatu na dobę, koniec prób po błędzie trwałym
static void testR_KONF_46_limitZapytanAutomatu() {
  Stan s;
  const std::string d1 = "2026-11-10", d2 = "2026-11-11";
  SPRAWDZ(automatMozeSiec(s, d1) && !automatMozeSiec(s, ""));
  poZapytaniuAutomatu(s, d1, Powod::BRAK_SIECI);
  SPRAWDZ(automatMozeSiec(s, d1));  // jedno ponowienie po braku sieci
  poZapytaniuAutomatu(s, d1, Powod::BRAK_SIECI);
  SPRAWDZ(!automatMozeSiec(s, d1) && automatMozeSiec(s, d2));
  for (Powod p : {Powod::TOKEN_ODRZUCONY, Powod::BRAK_WYDAN, Powod::USZKODZONY, Powod::INNY_PANEL, Powod::NIE_NOWSZA}) {
    Stan t;
    poZapytaniuAutomatu(t, d1, p);
    SPRAWDZ(bladTrwaly(p) && !automatMozeSiec(t, d1) && automatMozeSiec(t, d2));
  }
  for (Powod p : {Powod::BRAK_SIECI, Powod::BATERIA, Powod::NOC, Powod::BRAK}) SPRAWDZ(!bladTrwaly(p));
  Stan u;  // odmowa instalacji bez nowego zapytania: koniec prób, licznik bez zmian
  poZapytaniuAutomatu(u, d1, Powod::USZKODZONY, false);
  SPRAWDZ(u.siecZapytan == 0 && u.siecKoniec && !automatMozeSiec(u, d1));
  Stan w;  // sprawdzenie dobowe z błędem trwałym: automat tej doby nie pyta; brak sieci w dobowym nie zużywa
  poSprawdzeniuDobowym(w, d1, Powod::BRAK_SIECI);
  SPRAWDZ(automatMozeSiec(w, d1));
  poSprawdzeniuDobowym(w, d1, Powod::TOKEN_ODRZUCONY);
  SPRAWDZ(!automatMozeSiec(w, d1) && automatMozeSiec(w, d2));
  Dziennik dz;
  AtrapaNvs n(dz);  // trwałość w NVS (`au_d`, `au_n`, `au_k`)
  zapisz(n, s);
  Stan z = wczytaj(n);
  SPRAWDZ(z.siecDnia == d1 && z.siecZapytan == 2 && !automatMozeSiec(z, d1));
}

int main() {
  testR_KONF_46_limitZapytanAutomatu();
  testR_NF_34_porownanieWersji();
  testJson();
  testR_NF_32_odpowiedzGitHubIOpis();
  testR_NF_34_sha256();
  testR_NF_36_tokenTylkoDoApi();
  testR_NF_34_decyzjaInstalacji();
  testR_KONF_45_automat();
  testR_KONF_46_sprawdzicDzis();
  testR_NF_35_stanPoStarcie();
  testR_KONF_44_teksty();
  testR_KONF_43_nawigacja();
  testR_KONF_43_stopki();
  testR_EKR_61_ikonaStopki();
  testR_KONF_44_rysowanieU07();
  testR_NF_36_sekrety();
  testR_NF_36_formularz();
  testR_NF_36_przekierowaniaAdaptera();
  printf("testy_aktualizacja: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
