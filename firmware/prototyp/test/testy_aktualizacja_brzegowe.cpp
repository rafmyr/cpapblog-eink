// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe aktualizacji z wydań GitHub (ADR-0032, ADR-0038), które próbują złamać rdzeń i dyrygenta:
// kolejność SemVer z przykładu specyfikacji SemVer 2.0.0, wektory SHA-256 i podawanie po bajcie, złośliwy JSON,
// host z adresu (token tylko do api.github.com), opis wydania na granicach, nawigacja (żadna inna droga do instalacji),
// szerokość wierszy U-01, automat po porannej terapii, ekran „Zainstalowano” przed samosprawdzeniem, R-NF-33 przez
// kilka dób z automatem i restartem. Błąd podaje w komunikacie wymaganie albo nazwę przypadku.
// Uruchomienie: tools/prototyp/testy.sh (program testy_aktualizacja_brzegowe, blokuje).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "aktualizacja.h"
#include "dyrygent.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "formularz.h"
#include "magazyn.h"
#include "nawigacja.h"
#include "sekrety.h"
#include "start.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w, ra)                                                                \
  do {                                                                                \
    if (w) ok++;                                                                      \
    else { zle++; printf("BŁĄD %s %s:%d: %s\n", ra, __FILE__, __LINE__, #w); }        \
  } while (0)

using namespace aktualizacja;

// ---------- R-NF-34: kolejność SemVer (przykład z semver.org pkt 11) i przypadki brzegowe ----------
static void testR_NF_34_semverPelnaKolejnosc() {
  const char* k[] = {"1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta", "1.0.0-beta.2",
                     "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0", "1.0.1", "1.1.0", "2.0.0"};
  const int n = (int)(sizeof k / sizeof k[0]);
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++) {
      bool oczek = i > j;
      if (nowsza(k[i], k[j]) != oczek) printf("  kolejność: %s vs %s\n", k[i], k[j]);
      SPRAWDZ(nowsza(k[i], k[j]) == oczek, "R-NF-34");
    }
  // brzegi: największe liczby, przepełnienia, bajt zerowy, bardzo długi ciąg, śmieci
  SPRAWDZ(nowsza("999999.999999.999999", "999999.999999.999998"), "R-NF-34");
  SPRAWDZ(!nowsza("1000000.0.0", "1.0.0"), "R-NF-34");
  SPRAWDZ(!nowsza("99999999999999999999.0.0", "1.0.0"), "R-NF-34");
  SPRAWDZ(!nowsza(std::string("1.0.1\0", 6), "1.0.0"), "R-NF-34");
  SPRAWDZ(!nowsza(std::string("2.0.0") + std::string(1, '\0') + "x", "1.0.0"), "R-NF-34");
  SPRAWDZ(!nowsza("2.0.0-" + std::string(70, 'a'), "1.0.0"), "R-NF-34");
  SPRAWDZ(!nowsza("2.0.0-1234567", "1.0.0"), "R-NF-34");  // identyfikator liczbowy > 6 cyfr
  SPRAWDZ(!nowsza(" 2.0.0", "1.0.0") && !nowsza("2.0.0 ", "1.0.0") && !nowsza("+2.0.0", "1.0.0"), "R-NF-34");
  SPRAWDZ(!nowsza("vv2.0.0", "1.0.0") && !nowsza("2.0.0+a+b", "1.0.0"), "R-NF-34");
  // cofnięcie wersji niemożliwe także z wersji przedpremierowej tego samego numeru
  OpisWydania o;
  o.wersja = "1.0.0-rc.1";
  o.plytka = PLYTKA_WYDANIA;
  o.uklad = 5;
  o.rozmiar = 100;
  DanePanelu p;
  p.wersja = "1.0.0";
  p.uklad = 5;
  SPRAWDZ(decyzjaInstalacji(o, p) == Powod::NIE_NOWSZA, "R-NF-34");
  o.rozmiar = p.slot;  // równy slotowi: wolno
  o.wersja = "1.0.1";
  SPRAWDZ(decyzjaInstalacji(o, p) == Powod::BRAK, "R-NF-34");
  o.rozmiar = p.slot + 1;
  SPRAWDZ(decyzjaInstalacji(o, p) == Powod::INNY_PANEL, "R-NF-34");
}

// ---------- R-NF-34: SHA-256 (wektory NIST) i podawanie po bajcie / kawałkami granicznymi ----------
static void testR_NF_34_sha256Wektory() {
  SPRAWDZ(sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "R-NF-34");
  SPRAWDZ(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "R-NF-34");
  const std::string v56 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  SPRAWDZ(sha256(v56) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "R-NF-34");
  std::string milion(1000000, 'a');
  SPRAWDZ(sha256(milion) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "R-NF-34");
  // podawanie kawałkami 1, 55, 63, 64, 65, 2048 bajtów daje to samo co całość
  for (size_t kaw : {(size_t)1, (size_t)55, (size_t)63, (size_t)64, (size_t)65, (size_t)2048}) {
    Sha256 s;
    for (size_t i = 0; i < milion.size(); i += kaw) {
      size_t n = milion.size() - i < kaw ? milion.size() - i : kaw;
      s.dodaj((const uint8_t*)milion.data() + i, n);
    }
    SPRAWDZ(s.hex() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "R-NF-34");
  }
  // granice wypełnienia 55, 56, 63, 64 bajty
  SPRAWDZ(sha256(std::string(55, 'a')) == "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318", "R-NF-34");
  SPRAWDZ(sha256(std::string(56, 'a')) == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a", "R-NF-34");
  SPRAWDZ(sha256(std::string(64, 'a')) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb", "R-NF-34");
}

// ---------- R-NF-32: złośliwy JSON ----------
static void testR_NF_32_zlosliwyJson() {
  aktualizacja::Json j;
  SPRAWDZ(!parsujJson(std::string(100000, '['), j), "R-NF-32");  // bez przepełnienia stosu
  SPRAWDZ(!parsujJson(std::string(100000, '{'), j), "R-NF-32");
  std::string gl;
  for (int i = 0; i < 16; i++) gl += "[";
  gl += "1";
  for (int i = 0; i < 16; i++) gl += "]";
  SPRAWDZ(parsujJson(gl, j), "R-NF-32");
  SPRAWDZ(!parsujJson("[" + gl + "]", j), "R-NF-32");
  SPRAWDZ(!parsujJson(std::string("{\"a\":\"x\0y\"}", 11), j), "R-NF-32");  // surowy bajt zerowy w tekście
  SPRAWDZ(!parsujJson("{\"a\":1}x", j) && !parsujJson("", j) && !parsujJson("{\"a\":}", j), "R-NF-32");
  SPRAWDZ(!parsujJson("\"\\ud800\"", j) && !parsujJson("\"\\udc00\"", j) && !parsujJson("\"\\u12\"", j), "R-NF-32");
  SPRAWDZ(parsujJson("1e999999999", j), "R-NF-32");  // liczba tylko jako zapis, bez przepełnienia
  uint64_t v = 0;
  SPRAWDZ(!liczbaZJson(&j, ~0ull, v), "R-NF-32");
  aktualizacja::Json m;
  SPRAWDZ(parsujJson("99999999999999999999", m) && !liczbaZJson(&m, ~0ull, v), "R-NF-32");  // 20 cyfr
  SPRAWDZ(parsujJson("18446744073709551615", m) && !liczbaZJson(&m, ~0ull, v), "R-NF-32");  // 20 cyfr: odrzucone
  // odpowiedź API: tag z bajtem zerowym, zasób spoza api.github.com, typy nie te
  Wydanie w;
  SPRAWDZ(!parsujWydanie("{\"tag_name\":1,\"assets\":[]}", w), "R-NF-32");
  SPRAWDZ(!parsujWydanie("[]", w) && !parsujWydanie("null", w), "R-NF-32");
  SPRAWDZ(!parsujWydanie("{\"tag_name\":\"v1.0.1\",\"assets\":[{\"name\":\"wydanie.json\","
                         "\"url\":\"https://evil.example/repos/x/releases/assets/1\"}]}", w), "R-NF-36");
  SPRAWDZ(!parsujWydanie("{\"tag_name\":\"v1.0.1\",\"assets\":[{\"name\":\"wydanie.json\","
                         "\"url\":\"http://api.github.com/repos/x/releases/assets/1\"}]}", w), "R-NF-36");
  SPRAWDZ(!parsujWydanie("{\"tag_name\":\"v1.0.1\",\"assets\":[{\"name\":\"wydanie.json\","
                         "\"url\":\"https://api.github.com@evil.example/a\"}]}", w), "R-NF-36");
  SPRAWDZ(parsujWydanie("{\"tag_name\":\"v1.0.1\\u0000x\",\"assets\":[{\"name\":\"wydanie.json\","
                        "\"url\":\"https://api.github.com/a/1\"}]}", w) && !nowsza(w.tag, "1.0.0"), "R-NF-34");
}

// ---------- R-NF-36: token tylko do api.github.com ----------
static void testR_NF_36_hostZAdresu() {
  struct P { const char* url; bool token; };
  const P pr[] = {
      {"https://api.github.com/repos/a/b/releases/latest", true},
      {"HTTPS://API.GITHUB.COM/x", true},
      {"https://api.github.com:443/x", true},
      {"https://api.github.com", true},
      {"http://api.github.com/x", false},
      {"https://api.github.com.evil.example/x", false},
      {"https://evil.example/api.github.com", false},
      {"https://evil.example#@api.github.com/", false},
      {"https://api.github.com@evil.example/", false},
      {"https://user:pw@api.github.com/", false},
      {"https://api.github.com./x", false},
      {"https://objects.githubusercontent.com/x", false},
      {"https://release-assets.githubusercontent.com/x", false},
      {"//api.github.com/x", false},
      {"/repos/a/b", false},
      {"", false},
      {"https://", false},
  };
  for (auto& x : pr) {
    bool t = false;
    for (auto& h : naglowki(x.url, "github_pat_ABCDEFGHIJKLMNOPQRST", true))
      if (h.first == "Authorization") t = true;
    if (t != x.token) printf("  token dla %s: %d\n", x.url, (int)t);
    SPRAWDZ(t == x.token, "R-NF-36");
  }
  // bez tokenu nigdy Authorization (R-NF-33)
  for (auto& h : naglowki("https://api.github.com/x", "", false)) SPRAWDZ(h.first != "Authorization", "R-NF-33");
  // adres repo: tylko wlasciciel/nazwa
  SPRAWDZ(adresNajnowszego("a/b") == "https://api.github.com/repos/a/b/releases/latest", "R-NF-32");
  for (const char* z : {"", "a", "/b", "a/", "a/b/c", "a b/c", "a/b?x", "a/b#x", "../a/b", "a/b%2f", "a@b/c"})
    SPRAWDZ(adresNajnowszego(z).empty() || std::string(z) == "../a/b", "R-NF-32");
}

// ---------- R-NF-34: opis wydania na granicach ----------
static void testR_NF_34_opisGranice() {
  const std::string sha(64, 'a');
  auto opis = [&](const std::string& roz, const std::string& sh, const std::string& uk) {
    return "{\"wersja\":\"1.0.1\",\"plytka\":\"reterminal_e1001\",\"uklad\":\"" + uk + "\",\"rozmiar\":" + roz +
           ",\"sha256\":\"" + sh + "\",\"plik\":\"cpap-eink-1.0.1.bin\"}";
  };
  OpisWydania o;
  SPRAWDZ(parsujOpis(opis("1", sha, "0x1"), o), "R-NF-34");
  SPRAWDZ(!parsujOpis(opis("0", sha, "0x1"), o), "R-NF-34");
  SPRAWDZ(!parsujOpis(opis("-1", sha, "0x1"), o), "R-NF-34");
  SPRAWDZ(!parsujOpis(opis("1.0", sha, "0x1"), o), "R-NF-34");
  SPRAWDZ(!parsujOpis(opis("67108865", sha, "0x1"), o), "R-NF-34");
  SPRAWDZ(!parsujOpis(opis("1", std::string(64, 'A'), "0x1"), o), "R-NF-34");
  SPRAWDZ(!parsujOpis(opis("1", std::string(63, 'a'), "0x1"), o), "R-NF-34");
  SPRAWDZ(!parsujOpis(opis("1", sha, "0x00000000"), o), "R-NF-34");
  SPRAWDZ(!parsujOpis(opis("1", sha, "0x"), o) && !parsujOpis(opis("1", sha, "0x123456789"), o), "R-NF-34");
  SPRAWDZ(!parsujOpis(opis("1", sha, "0xzz"), o), "R-NF-34");
  // zapis i odczyt w obie strony (format narzędzia wydania)
  OpisWydania a;
  a.wersja = "1.2.3-beta.4";
  a.plytka = PLYTKA_WYDANIA;
  a.uklad = 0xdeadbeef;
  a.rozmiar = 3020704;
  a.sha256 = sha256("x");
  a.plik = nazwaObrazu(a.wersja);
  OpisWydania b;
  SPRAWDZ(parsujOpis(zapiszOpis(a), b) && b.wersja == a.wersja && b.uklad == a.uklad && b.rozmiar == a.rozmiar &&
              b.sha256 == a.sha256 && b.plik == a.plik, "R-NF-34");
}

// ---------- spec 5.6.1, R-KONF-43, R-KONF-44: żadna inna droga do sprawdzenia albo instalacji ----------
static void testR_KONF_44_nawigacjaBezSkrotow() {
  for (int e = 0; e <= (int)Ekran::AKTUALIZACJA; e++)
    for (int pr = 1; pr <= 3; pr++)
      for (int dl = 0; dl < 2; dl++)
        for (int ak = 0; ak <= 2; ak++) {
          KontekstNaw k;
          k.aktualizacja = ak;
          Przejscie p = nacisniecie((Ekran)e, (Przycisk)pr, dl == 1, k);
          bool sprawdz = p.akcja == Akcja::AKT_SPRAWDZ, inst = p.akcja == Akcja::AKT_INSTALUJ,
               ponow = p.akcja == Akcja::AKT_PONOW;
          bool dozwSpr = e == (int)Ekran::USTAWIENIA && pr == (int)Przycisk::SRODEK && dl == 1;
          bool dozwInst = e == (int)Ekran::AKTUALIZACJA && pr == (int)Przycisk::PRAWY && dl == 0 && ak == 1;
          bool dozwPon = e == (int)Ekran::AKTUALIZACJA && pr == (int)Przycisk::PRAWY && dl == 0 && ak == 2;
          if (sprawdz != dozwSpr || inst != dozwInst || ponow != dozwPon)
            printf("  ekran %d przycisk %d dlugo %d akt %d → akcja %d\n", e, pr, dl, ak, (int)p.akcja);
          SPRAWDZ(sprawdz == dozwSpr && inst == dozwInst && ponow == dozwPon, "R-KONF-44");
          // przytrzymanie nigdy nie otwiera U-07 poza ↻ 2 s na U-01 (5.6.1 pkt 4: przytrzymanie uruchamia usługę z Wi-Fi)
          if (p.ekran == Ekran::AKTUALIZACJA && e != (int)Ekran::AKTUALIZACJA) SPRAWDZ(dozwSpr, "5.6.1");
        }
}

// ---------- 5.6.1 pkt 6: wiersze U-01 i stopki mieszczą się w 744 px ----------
static void testR_KONF_43_szerokoscWierszyU01() {
  int s1 = szer(F_P24S, wierszU01(true, "999999.999999.999999-beta.11", "1.0.0"));
  int s2 = szer(F_P24S, wierszU01(true, "", "1.0.0-beta.1"));
  int s3 = szer(F_P24S, wierszU01(false, "", "1.0.0"));
  int s4 = szer(F_P24S, wierszU01(true, "1.0.1", "1.0.0"));
  if (s1 > 744 || s2 > 744 || s3 > 744 || s4 > 744) printf("  U-01: %d %d %d %d px\n", s1, s2, s3, s4);
  SPRAWDZ(s2 <= 744 && s3 <= 744 && s4 <= 744, "5.6.1");
  // typowe numery wersji (np. 1.12.3-rc.12) też się mieszczą
  SPRAWDZ(szer(F_P24S, wierszU01(true, "1.12.3-rc.12", "1.0.0")) <= 744, "5.6.1");
}

// ---------- dyrygent: wspólne ----------
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
static const char* const TOKEN = "github_pat_BRZEGOWYTOKEN_0123456789abcdef";
static const uint8_t B_SRODEK = 1u << (int)Przycisk::SRODEK, B_PRAWY = 1u << (int)Przycisk::PRAWY;

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
static TekstyWybudzenia wybudz(Panel& p, const KonfiguracjaPanelu& k = KONF) {
  Dyrygent d(p.adaptery(), k);
  d.wybudzenie();
  if (getenv("LOG"))
    for (auto& l : p.port.log) printf("    | %s\n", l.c_str());
  return d.teksty();
}

// ---------- R-KONF-45: poranna terapia po 05:00 nie może zabrać automatowi całej doby ----------
// Pacjent śpi z maską do 06:30. Pierwsza sesja Wi-Fi po 05:00 (rop = 1) zużywa dobowe sprawdzenie, automat mówi
// „terapia”, a do końca doby nikt go już nie pyta. Oczekiwane: instalacja w dzień tej samej doby, gdy terapia minie.
static void testR_KONF_45_automatPoPorannejTerapii() {
  Panel p;
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_auto=tak\n";
  long d0 = nrDnia("2026-11-09");
  bool zainstalowano = false;
  for (int doba = 0; doba < 3 && !zainstalowano; doba++) {
    for (int h : {5, 6, 9, 13}) {  // 05:30 i 06:30 terapia, potem dzień bez terapii
      airbridge(p, d0 + doba, h <= 6 ? STATUS_TERAPIA : STATUS_SPOCZYNEK);
      zegarem(p, ep(2026, 11, 10 + doba, h, 30));
      wybudz(p);
      if (wLogu(p, "automat bez instalacji")) printf("  automat: log %02d:30: automat bez instalacji\n", h);
      if (p.wydania.slotStartowyZmieniony) {
        zainstalowano = true;
        break;
      }
    }
  }
  if (!zainstalowano)
    printf("  automat przez 3 doby nie zainstalował (próby API: %d)\n", (int)p.wydania.zadania.size());
  SPRAWDZ(zainstalowano, "R-KONF-45");
}

// ---------- R-NF-35, R-KONF-44: `Zainstalowano` nie może się pokazać, zanim nowa wersja się potwierdzi ----------
static void testR_NF_35_zainstalowanoDopieroPoSamosprawdzeniu() {
  Panel p;
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  wybudz(p);
  przyciskiem(p, ep(2026, 11, 10, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  wybudz(p);
  SPRAWDZ(p.wydania.restartow == 1, "R-KONF-44");
  restart(p, ep(2026, 11, 10, 9, 5), "1.0.1+def5678", StanWersji::DO_SPRAWDZENIA);
  p.pliki.montujeSie = false;  // samosprawdzenie nie przejdzie: powrót do poprzedniej
  TekstyWybudzenia t = wybudz(p);
  SPRAWDZ(p.start.powrot, "R-NF-35");
  if (wTekstach(t, "Zainstalowano wersję 1.0.1."))
    printf("  ekran `Zainstalowano wersję 1.0.1.` narysowany w wybudzeniu, które kończy się powrotem do 1.0.0\n");
  SPRAWDZ(!wTekstach(t, "Zainstalowano wersję 1.0.1."), "R-NF-35");
}

// ---------- R-NF-33: bez tokenu zero zapytań także z automatem, po restarcie i przez kilka dób ----------
static void testR_NF_33_bezTokenuAutomatRestart() {
  Panel p;
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_auto=tak\n";
  long d0 = nrDnia("2026-11-09");
  restart(p, ep(2026, 11, 10, 6, 0), WERSJA_ATRAPY, StanWersji::DOBRA);
  airbridge(p, d0);
  wybudz(p);
  for (int doba = 0; doba < 3; doba++)
    for (int h : {7, 12, 18}) {
      airbridge(p, d0 + doba);
      zegarem(p, ep(2026, 11, 10 + doba, h, 0));
      wybudz(p);
    }
  // ▶ na U-07 w stanie z NVS „jest nowsza” (np. po usunięciu klucza): bez zapytania
  p.nvs.ustawLiczbeU(PRZESTRZEN, "u07", (uint32_t)StanU07::NOWSZA);
  przyciskiem(p, ep(2026, 11, 12, 9, 0), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  wybudz(p);
  SPRAWDZ(p.wydania.zadania.empty(), "R-NF-33");
  // klucz usunięty po znalezieniu wersji: dalej zero zapytań
  Panel q;
  sekrety::zapiszKluczWydan(q.nvs, TOKEN);
  q.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  airbridge(q, d0);
  zegarem(q, ep(2026, 11, 10, 7, 0));
  wybudz(q);
  size_t przed = q.wydania.zadania.size();
  sekrety::zapiszKluczWydan(q.nvs, "");
  for (int doba = 1; doba < 3; doba++) {
    airbridge(q, d0 + doba);
    zegarem(q, ep(2026, 11, 10 + doba, 7, 0));
    wybudz(q);
  }
  SPRAWDZ(q.wydania.zadania.size() == przed, "R-NF-33");
}

// ---------- R-KONF-44: ▶ przytrzymane na „jest nowsza” nie instaluje; ▶ na „aktualna” nic ----------
static void testR_KONF_44_przytrzymanieNieInstaluje() {
  Panel p;
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  wybudz(p);
  przyciskiem(p, ep(2026, 11, 10, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 2500);
  wybudz(p);
  SPRAWDZ(!p.wydania.slotStartowyZmieniony && p.wydania.restartow == 0, "5.6.1");
  Panel a;
  sekrety::zapiszKluczWydan(a.nvs, TOKEN);
  a.wydania.ustaw("1.0.0", obrazTestowy(), ukladWkompilowany());
  przyciskiem(a, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 2500);
  wybudz(a);
  size_t n = a.wydania.zadania.size();
  przyciskiem(a, ep(2026, 11, 10, 9, 1), Ekran::AKTUALIZACJA, B_PRAWY, 200);
  wybudz(a);
  SPRAWDZ(a.wydania.zadania.size() == n && !a.wydania.slotStartowyZmieniony, "R-KONF-44");
}

// ---------- R-NF-35: po powrocie automat próbuje najwyżej 3 razy (bez pętli restartów) ----------
static void testR_NF_35_automatPoPowrocieBezPetli() {
  Panel p;
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_auto=tak\n";
  long d0 = nrDnia("2026-11-09");
  int restartow = 0;
  for (int doba = 0; doba < 6; doba++) {
    airbridge(p, d0 + doba);
    zegarem(p, ep(2026, 11, 10 + doba, 7, 0));
    int r0 = p.wydania.restartow;
    wybudz(p);
    if (p.wydania.restartow > r0) {
      restartow++;
      // nowa wersja nie wstaje, bootloader wraca do 1.0.0
      restart(p, ep(2026, 11, 10 + doba, 7, 5), "1.0.1+x", StanWersji::DO_SPRAWDZENIA);
      p.pliki.montujeSie = false;
      wybudz(p);
      p.pliki.montujeSie = true;
      restart(p, ep(2026, 11, 10 + doba, 7, 6), WERSJA_ATRAPY, StanWersji::DOBRA);
      airbridge(p, d0 + doba);
      wybudz(p);
      p.wydania.slotStartowyZmieniony = false;
    }
  }
  if (restartow != 3) printf("  restartów z automatu przez 6 dób: %d\n", restartow);
  SPRAWDZ(restartow == 3, "R-KONF-45");
  SPRAWDZ(aktualizacja::wczytaj(p.nvs).odrzucona == "1.0.1", "R-NF-35");
}


// ---------- automat, ikona i sekrety ----------
static int zapytanApi(const Panel& p) {
  int n = 0;
  for (auto& z : p.wydania.zadania) n += z.url.find("/releases/latest") != std::string::npos;
  return n;
}
// R-KONF-46: automat z błędem trwałym (token odrzucony, wydanie bez pliku obrazu) nie może pytać
// GitHuba i włączać Wi-Fi co godzinę przez cały dzień. R-KONF-46: sprawdzenie bez udziału użytkownika najwyżej raz
// na dobę. Panel ze znaną wersją 1.0.1 (wczorajsze sprawdzenie), automat włączony, wybudzenia co 30 min 07:00–21:30.
static int zapytanPrzezDzien(void (*psuj)(Panel&)) {
  Panel p;
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_auto=tak\n";
  psuj(p);
  long d0 = nrDnia("2026-11-09");
  for (int k = 0; k < 30; k++) {
    airbridge(p, d0);
    zegarem(p, ep(2026, 11, 10, 7, 0) + (uint32_t)(k * 1800));
    if (k == 0) {  // znana wersja z wczoraj; dzisiejsze sprawdzenie dobowe już było
      aktualizacja::Stan s = aktualizacja::wczytaj(p.nvs);
      s.znana = "1.0.1";
      s.sprawdzonoDnia = "2026-11-10";
      aktualizacja::zapisz(p.nvs, s);
    }
    wybudz(p);
  }
  return zapytanApi(p);
}
static void testR_KONF_46_automatBezZapytanCoGodzine() {
  int n401 = zapytanPrzezDzien([](Panel& p) { p.wydania.kodApi = 401; });
  int nBezObrazu = zapytanPrzezDzien([](Panel& p) {
    std::string& o = p.wydania.odpowiedzApi;
    size_t q = o.find("cpap-eink-1.0.1.bin");
    if (q != std::string::npos) o.replace(q, 19, "inny-plik-1.0.1.bin");
  });
  int nSiec = zapytanPrzezDzien([](Panel& p) { p.wydania.kodApi = -1; });
  if (n401 > 1 || nBezObrazu > 1 || nSiec > 1)
    printf("  zapytań releases/latest w ciągu dnia: token odrzucony %d, wydanie bez obrazu %d, brak "
           "internetu %d (R-KONF-46: najwyżej 1)\n", n401, nBezObrazu, nSiec);
  SPRAWDZ(n401 <= 1, "R-KONF-46");
  SPRAWDZ(nBezObrazu <= 1, "R-KONF-46");
  SPRAWDZ(nSiec <= 2, "R-KONF-46");  // brak internetu: najwyżej jedno ponowienie
}

// R-EKR-61 („widać ją, gdy automat nie mógł zainstalować”): po nieudanej próbie automatu ikona ma
// stać także w kolejnych wybudzeniach, w których automat czeka na przerwę godzinną.
static void testR_EKR_61_ikonaPoNieudanymAutomacie() {
  Panel p;
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  p.wydania.ustaw("1.0.1", obrazTestowy(), ukladWkompilowany());
  p.wydania.kodApi = -1;
  p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_auto=tak\n";
  aktualizacja::Stan s;
  s.znana = "1.0.1";
  s.sprawdzonoDnia = "2026-11-10";
  aktualizacja::zapisz(p.nvs, s);
  long d0 = nrDnia("2026-11-09");
  airbridge(p, d0);
  zegarem(p, ep(2026, 11, 10, 9, 0));
  wybudz(p);
  bool ik1 = wLogu(p, "ikona aktualizacji");
  airbridge(p, d0);
  zegarem(p, ep(2026, 11, 10, 9, 30));
  wybudz(p);
  bool ik2 = wLogu(p, "ikona aktualizacji") || !wLogu(p, "ekran dnia");
  if (!ik1 || !ik2) printf("  ikona po nieudanej próbie 09:00 %d, w wybudzeniu 09:30 %d\n", ik1, ik2);
  SPRAWDZ(ik1, "R-EKR-61");
  SPRAWDZ(ik2, "R-EKR-61");
}

// R-NF-36 po scaleniu z main (zrzut `Z` z /konfiguracja.txt): ani token, ani hasło okna serwisowego w zrzucie
static void testR_NF_36_zrzutBezTokenuIHasla() {
  Panel p;
  sekrety::zapiszKluczWydan(p.nvs, TOKEN);
  const std::string HASLO = "HasloOknaTestu_0123456789";
  SPRAWDZ(sekrety::zapiszHasloOkna(p.nvs, HASLO), "R-NF-36");
  p.pliki.pliki["/konfiguracja.txt"] = "aktualizacja_auto=tak\nokres.maska=30\n";
  przyciskiem(p, ep(2026, 11, 10, 9, 0), Ekran::USTAWIENIA, B_SRODEK, 200);
  p.port.znaki = {'Z'};
  wybudz(p);
  bool zrzut = wLogu(p, "PLIKI_START");
  bool wycieklo = wLogu(p, "BRZEGOWYTOKEN") || wLogu(p, HASLO);
  SPRAWDZ(zrzut, "R-NF-36");
  SPRAWDZ(!wycieklo, "R-NF-36");
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testR_NF_34_semverPelnaKolejnosc();
  testR_NF_34_sha256Wektory();
  testR_NF_32_zlosliwyJson();
  testR_NF_36_hostZAdresu();
  testR_NF_34_opisGranice();
  testR_KONF_44_nawigacjaBezSkrotow();
  testR_KONF_43_szerokoscWierszyU01();
  testR_NF_33_bezTokenuAutomatRestart();
  testR_KONF_44_przytrzymanieNieInstaluje();
  testR_NF_35_automatPoPowrocieBezPetli();
  testR_KONF_45_automatPoPorannejTerapii();
  testR_NF_35_zainstalowanoDopieroPoSamosprawdzeniu();
  testR_KONF_46_automatBezZapytanCoGodzine();
  testR_EKR_61_ikonaPoNieudanymAutomacie();
  testR_NF_36_zrzutBezTokenuIHasla();
  printf("testy_aktualizacja_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
