// SPDX-License-Identifier: GPL-3.0-or-later
// Testy „Rekord nocy”. Zakres: pola M-13 (`RespRate`), M-17 (`session_count` − 1, fail-path
// R-DANE-27), M-08 (`leak_high_seconds`), `pna` i `srn` z `/api/status` (R-NOR-25) w `struct Noc`, ich zapis przy pobraniu nocy
// z AirBridge, zasada „pełniejszy rekord wygrywa” (uzupelnijNoc), format `noce` wersji 2 z migracją 1 → 2
// (ADR-0022, T-WYM-55, T-WYM-57) i łańcuch 0 → 2 z płaskiego `/noce.csv` (stan panelu przed), zrzut `Z`
// i przywracanie `W` z na formacie wersji 2.
// Dane wyłącznie syntetyczne (bez związku z żadną osobą). Uruchomienie: tools/prototyp/testy.sh (program testy_rekord_nocy).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "magazyn.h"
#include "noc.h"
#include "stany.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
#define ROWNE(a, b)                                                                                     \
  do {                                                                                                  \
    std::string _a = (a), _b = (b);                                                                     \
    if (_a == _b) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)
#define ROWNE_L(a, b)                                                                                    \
  do {                                                                                                   \
    long long _a = (long long)(a), _b = (long long)(b);                                                  \
    if (_a == _b) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); }  \
  } while (0)
#define BLISKO(a, b) SPRAWDZ(std::fabs((double)(a) - (double)(b)) < 1e-3)

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";

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

// ---------- pomocnicze ----------
static std::vector<std::string> dziel(const std::string& s, char z) {
  std::vector<std::string> v;
  size_t a = 0;
  for (;;) {
    size_t b = s.find(z, a);
    v.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  return v;
}
static std::vector<std::string> wiersze(const std::string& t) {
  std::vector<std::string> v = dziel(t, '\n');
  if (!v.empty() && v.back().empty()) v.pop_back();
  return v;
}
static bool zawiera(const std::vector<std::string>& v, const std::string& p) {
  for (auto& s : v)
    if (s.find(p) != std::string::npos) return true;
  return false;
}
static std::string wierszZData(const std::string& tresc, const std::string& data) {
  for (auto& w : wiersze(tresc))
    if (w.compare(0, data.size() + 1, data + ";") == 0) return w;
  return "";
}
static const char* POLA_V1 =
    "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;csr;"
    "import";
static const std::string POLA_V2 = std::string(POLA_V1) + ";m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb";
// bieżąca wersja `noce` to 3 (pole `m18` na końcu); testy zapisu sprawdzają wersję 3
static const std::string POLA_V3 = POLA_V2 + ";m18";
// Puste pola wersji 2 wiersza wersji 2 (po polu `import`) i puste pola po M-08.
static const std::string PUSTE_B1(20, ';');
static const std::string PUSTE_USTAW(15, ';');
static std::string plik(const std::string& nazwa, int wersja, const std::string& pola,
                        const std::vector<std::string>& w) {
  std::string t = "#cpap-eink " + nazwa + " " + std::to_string(wersja) + "\n" + pola + "\n";
  for (auto& x : w) t += x + "\n";
  return t + "#koniec " + std::to_string(w.size()) + "\n";
}
// Noc syntetyczna `i`: źródło K albo A, ostatnie pola wersji 0 (M-03, M-09) czasem puste, jak na panelu w wersji 0.
static Noc nocTestowa(const std::string& data, int i) {
  Noc n;
  n.data = data;
  n.wazna = i % 9 != 0;
  n.m01 = 300 + i % 150;
  n.ahi = 0.5f + (float)(i % 40) * 0.1f;
  n.cmed = 9.5f + (float)(i % 10) * 0.1f;
  n.cp95 = 12.5f + (float)(i % 8) * 0.2f;
  n.wmed = 2.0f + (float)(i % 7) * 0.3f;
  n.wp95 = 8.0f + (float)(i % 11) * 0.5f;
  n.emed = 6.0f;
  n.ep95 = 7.5f;
  n.ustawienia = "ASVAuto/6/10/3/15";
  n.zrodlo = i % 3 == 0 ? 'A' : 'K';
  if (n.zrodlo == 'A' && i % 2 == 0) {
    n.oa = 0.2f;
    n.ca = 0.6f;
    n.h = 0.5f;
    n.csr = (float)(i % 5);
  }
  return n;
}
static std::vector<std::string> noceV0(int n, const std::string& ostatnia) {
  std::vector<std::string> v;
  for (int i = 0; i < n; i++) v.push_back(zapiszNoc(nocTestowa(dodajDni(ostatnia, i - (n - 1)), i + 1)));
  return v;
}
static std::string plikV0(const std::vector<std::string>& w) {
  std::string t;
  for (auto& x : w) t += x + "\n";
  return t;
}
struct Log {
  std::vector<std::string> w;
  std::function<void(const std::string&)> f() {
    return [this](const std::string& s) { w.push_back(s); };
  }
};
static void montowana(AtrapaPlikow& pl) {
  pl.montujeSie = true;
  pl.montowane = true;
}
static int rekordyNocy(const AtrapaPlikow& pl) {
  int n = 0;
  for (auto& kv : pl.pliki) {
    const std::string& s = kv.first;
    if (s.compare(0, 11, "/dane/noce/") != 0 || s.size() < 4 || s.compare(s.size() - 4, 4, ".csv") != 0) continue;
    std::vector<std::string> w = wiersze(kv.second);
    for (size_t i = 0; i < w.size(); i++)
      if (!w[i].empty() && w[i][0] != '#' && i != 1) n++;
  }
  return n;
}

// Raport doby z polami wersji 2. sesje: wartość JSON `session_count` ("" = bez pola); resp: wartość `median` kanału
// `RespRate` ("" = bez kanału).
static std::string raportB1(long day, const std::string& sesje, const std::string& resp, bool partial = false,
                            const std::string& respZrodlo = "edf", const std::string& wyciek = "1800") {
  std::string d = std::to_string(day);
  std::string r = "{\"day\":" + d + ",\"present\":true,\"partial\":" + (partial ? "true" : "false") +
                  ",\"used_seconds\":25200,\"ahi\":1.4,\"events_known\":true,";
  if (!sesje.empty()) r += "\"session_count\":" + sesje + ",";
  if (!wyciek.empty()) r += "\"leak_threshold\":24,\"leak_high_seconds\":" + wyciek + ",";
  r += "\"counts\":{\"oa\":1,\"ca\":5,\"h\":4},\"channels\":[{\"id\":\"Press\",\"unit\":\"cmH2O\",\"median\":9.8,"
       "\"p95\":13.6,\"source\":\"edf\"},{\"id\":\"Leak\",\"unit\":\"L/min\",\"median\":2.4,\"p95\":9.6,"
       "\"source\":\"edf\"}";
  if (!resp.empty())
    r += ",{\"id\":\"RespRate\",\"unit\":\"/min\",\"median\":" + resp + ",\"p95\":19.0,\"source\":\"" + respZrodlo + "\"}";
  r += "],\"settings\":{\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"value\":\"ASVAuto\",\"option\":8}]}}";
  return r;
}
static const long DOBA = 20395;  // syntetyczna doba (poranek 2025-11-04)

// ============================================================================================================
// (a) rekord nocy: pola, wiersz wersji 2, parsery
// ============================================================================================================

static void testPolaDomyslne() {
  Noc n;
  SPRAWDZ(std::isnan(n.m13));
  ROWNE_L(n.m17, -1);
  SPRAWDZ(n.pna.empty() && n.srn.empty());
  ROWNE_L(POLA_NOCY_V2, 41);
  SPRAWDZ(std::isnan(n.m08));
}

static void testWierszV2() {
  Noc n = nocTestowa("2026-11-03", 3);
  n.import = 1762149600u;
  n.m13 = 16.2f;
  n.m17 = 1;
  n.pna = "AirCurve_10_CS_PaceWave";
  n.srn = "SYN00000001";
  std::string w = wierszNocyV2(n);
  ROWNE(w, wierszNocyV1(n) + ";16.20;1;AirCurve_10_CS_PaceWave;SYN00000001;" + PUSTE_USTAW);
  n.m08 = 42.5f;
  w = wierszNocyV2(n);
  ROWNE(w, wierszNocyV1(n) + ";16.20;1;AirCurve_10_CS_PaceWave;SYN00000001;42.50" + PUSTE_USTAW);
  ROWNE_L(dziel(w, ';').size(), 41);
  Noc r;
  SPRAWDZ(parsujNocV2(w, r));
  BLISKO(r.m13, 16.2);
  ROWNE_L(r.m17, 1);
  ROWNE(r.pna, "AirCurve_10_CS_PaceWave");
  ROWNE(r.srn, "SYN00000001");
  BLISKO(r.m08, 42.5);
  ROWNE_L(r.import, 1762149600u);
  ROWNE(wierszNocyV2(r), w);
  // brak wartości: puste pola, nie 0 ani -1
  Noc p = nocTestowa("2026-11-02", 4);
  ROWNE(wierszNocyV2(p), wierszNocyV1(p) + PUSTE_B1);
  Noc q;
  SPRAWDZ(parsujNocV2(wierszNocyV2(p), q) && std::isnan(q.m13) && q.m17 == -1 && q.pna.empty() && q.srn.empty() &&
          std::isnan(q.m08));
  // M-17 = 0 zapisane jako 0 (to wartość, nie brak)
  p.m17 = 0;
  ROWNE(wierszNocyV2(p), wierszNocyV1(p) + ";;0;;;" + PUSTE_USTAW);
}

// Pola tekstowe przez czystePole(): `;` i znaki sterujące usunięte, wiersz zostaje w 41 polach.
// Pola ustawień (tylko do odczytu, ADR-0002), UA/h i M-10 w wierszu wersji 2.
static void testPolaUstawienUaM10() {
  Noc n = nocTestowa("2026-11-03", 3);
  SPRAWDZ(std::isnan(n.mpa) && std::isnan(n.mpi) && std::isnan(n.ipc) && std::isnan(n.eep) && std::isnan(n.mns) &&
          std::isnan(n.mxs) && std::isnan(n.ua) && std::isnan(n.m10) && std::isnan(n.m10p));
  n.mpa = 15.0f;
  n.mpi = 5.0f;
  n.ipc = 9.0f;
  n.eep = 6.0f;
  n.mns = 3.0f;
  n.mxs = 15.0f;
  n.ua = 0.25f;
  n.m10 = 0.05f;
  n.m10p = 0.2f;
  std::string w = wierszNocyV2(n);
  ROWNE(w, wierszNocyV1(n) + ";;;;;;15.00;5.00;9.00;6.00;3.00;15.00;0.25;0.05;0.20;;;;;;");
  Noc r;
  SPRAWDZ(parsujNocV2(w, r));
  ROWNE(wierszNocyV2(r), w);
  BLISKO(r.mpa, 15.0);
  BLISKO(r.m10p, 0.2);
  BLISKO(r.ua, 0.25);
  Noc c = nocTestowa("2026-11-03", 3);
  SPRAWDZ(uzupelnijNoc(c, r));
  BLISKO(c.eep, 6.0);
  BLISKO(c.m10, 0.05);
  // raport: tagi MPA, MPI, IPC, EEP, MNS, MXS (liczba albo tekst z jednostką), counts.ua na godzinę, FlowLim median/p95
  std::string rap = "{\"day\":" + std::to_string(DOBA) + ",\"present\":true,\"partial\":false,\"used_seconds\":36000,"
      "\"ahi\":1.4,\"events_known\":true,\"session_count\":1,\"counts\":{\"oa\":1,\"ca\":5,\"h\":4,\"ua\":5},"
      "\"channels\":[{\"id\":\"Press\",\"median\":9.8,\"p95\":13.6,\"source\":\"edf\"},"
      "{\"id\":\"FlowLim\",\"median\":0.04,\"p95\":0.21,\"source\":\"edf\"}],"
      "\"settings\":{\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"value\":\"ASV\"},"
      "{\"tag\":\"MPA\",\"value\":\"15.0 cmH2O\"},{\"tag\":\"MPI\",\"value\":5},{\"tag\":\"IPC\",\"value\":\"9.0\"},"
      "{\"tag\":\"EEP\",\"value\":\"6.0 cmH2O\"},{\"tag\":\"MNS\",\"value\":3},{\"tag\":\"MXS\",\"value\":\"15\"}]}}";
  NocAB a = nocZRaportu(rap, DOBA, "ASV");
  SPRAWDZ(a.stan == StanRaportu::KOMPLETNA);
  BLISKO(a.noc.mpa, 15.0);
  BLISKO(a.noc.mpi, 5.0);
  BLISKO(a.noc.ipc, 9.0);
  BLISKO(a.noc.eep, 6.0);
  BLISKO(a.noc.mns, 3.0);
  BLISKO(a.noc.mxs, 15.0);
  BLISKO(a.noc.ua, 0.5);  // 5 × 3600 / 36000
  BLISKO(a.noc.m10, 0.04);
  BLISKO(a.noc.m10p, 0.21);
  // brak tagów, `available` = false, wartość nieczytelna, brak `ua` i `FlowLim`: puste (R-AW-09, R-AW-22)
  NocAB b = nocZRaportu(raportB1(DOBA, "1", "16"), DOBA, "ASVAuto");
  SPRAWDZ(std::isnan(b.noc.mpa) && std::isnan(b.noc.eep) && std::isnan(b.noc.ua) && std::isnan(b.noc.m10) &&
          std::isnan(b.noc.m10p));
  std::string rapNie = rap;
  size_t i = rapNie.find("\"available\":true");
  rapNie.replace(i, 16, "\"available\":false");
  SPRAWDZ(std::isnan(nocZRaportu(rapNie, DOBA, "ASV").noc.mpa));
  std::string rapZle = rap;
  i = rapZle.find("\"MPI\",\"value\":5");
  rapZle.replace(i, std::string("\"MPI\",\"value\":5").size(), "\"MPI\",\"value\":\"x\"");
  NocAB z = nocZRaportu(rapZle, DOBA, "ASV");
  SPRAWDZ(std::isnan(z.noc.mpi) && !std::isnan(z.noc.mpa));
}

static void testPnaZSrednikiem() {
  Noc n = nocTestowa("2026-11-03", 3);
  n.pna = "Air;Curve\n10";
  n.srn = "SYN;\t01";
  std::string w = wierszNocyV2(n);
  ROWNE_L(dziel(w, ';').size(), 41);
  SPRAWDZ(w.find('\n') == std::string::npos && w.find('\t') == std::string::npos);
  Noc r;
  SPRAWDZ(parsujNocV2(w, r));
  ROWNE(r.pna, "AirCurve10");
  ROWNE(r.srn, "SYN01");
}

// Plik wersji 2 czytany parserem wersji 1: pola dalsze pominięte, rekord jak z wersji 1.
static void testParsujV1NaWierszuV2() {
  Noc n = nocTestowa("2026-11-03", 3);
  n.import = 1762149600u;
  n.m13 = 15.5f;
  n.m17 = 2;
  n.pna = "X";
  n.srn = "Y";
  Noc r;
  SPRAWDZ(parsujNocV1(wierszNocyV2(n), r));
  ROWNE(zapiszNoc(r), zapiszNoc(n));
  ROWNE_L(r.import, 1762149600u);
  SPRAWDZ(std::isnan(r.m13) && r.m17 == -1 && r.pna.empty());
}

// Wiersz wersji 1 parserem wersji 2: nowe pola puste; za mało pól = błąd; nieczytelne m13 i m17 = brak danych.
static void testParsujV2Brzegi() {
  Noc n = nocTestowa("2026-11-03", 3);
  Noc r;
  SPRAWDZ(parsujNocV2(wierszNocyV1(n), r));
  ROWNE(zapiszNoc(r), zapiszNoc(n));
  SPRAWDZ(std::isnan(r.m13) && r.m17 == -1 && r.pna.empty() && r.srn.empty());
  SPRAWDZ(!parsujNocV2(zapiszNoc(n), r));  // 20 pól: wersja 0, nie 1 ani 2
  std::string v1 = wierszNocyV1(n);
  SPRAWDZ(parsujNocV2(v1 + ";abc;x;M;S;-5", r));
  SPRAWDZ(std::isnan(r.m13) && r.m17 == -1 && std::isnan(r.m08));
  ROWNE(r.pna, "M");
  ROWNE(r.srn, "S");
  SPRAWDZ(parsujNocV2(v1 + ";16.4;-3;;", r) && r.m17 == -1);  // ujemne M-17 = brak
  SPRAWDZ(parsujNocV2(v1 + ";16.4;2", r));  // urwany wiersz (mniej pól): to, co jest
  BLISKO(r.m13, 16.4);
  ROWNE_L(r.m17, 2);
  // pola z nowszej wersji (26.) pominięte
  SPRAWDZ(parsujNocV2(v1 + ";16.4;2;M;S;12.5" + PUSTE_USTAW + ";X", r) && r.srn == "S");
  BLISKO(r.m08, 12.5);
  SPRAWDZ(parsujNocV2(v1 + ";16.4;2;M;S;0", r) && r.m08 == 0.0f);  // 0 minut to wartość, nie brak
  SPRAWDZ(!parsujNocV2("śmieci", r));
}

// uzupelnijNoc obejmuje nowe pola: puste dostają wartość, wypełnione zostają; TYLKO_SPOZA_AIRBRIDGE ich nie rusza.
static void testUzupelnijNowePola() {
  Noc c = nocTestowa("2026-11-03", 3), z = c;
  z.m13 = 14.0f;
  z.m17 = 3;
  z.pna = "P";
  z.srn = "S";
  z.m08 = 30.0f;
  Noc a = c;
  SPRAWDZ(uzupelnijNoc(a, z));
  BLISKO(a.m13, 14.0);
  ROWNE_L(a.m17, 3);
  ROWNE(a.pna, "P");
  ROWNE(a.srn, "S");
  BLISKO(a.m08, 30.0);
  Noc b = c;
  b.m13 = 15.0f;
  b.m17 = 0;
  b.pna = "Q";
  b.srn = "T";
  b.m08 = 0.0f;
  SPRAWDZ(!uzupelnijNoc(b, z));
  BLISKO(b.m13, 15.0);
  ROWNE_L(b.m17, 0);
  ROWNE(b.pna, "Q");
  Noc d = c;
  SPRAWDZ(!uzupelnijNoc(d, z, PolaUzupelnienia::TYLKO_SPOZA_AIRBRIDGE));
  SPRAWDZ(std::isnan(d.m13) && d.m17 == -1 && d.pna.empty() && std::isnan(d.m08));
  // trescNocy: zmiana pola wersji 2 = inna treść, `import` bez znaczenia
  Noc e = c, f = c;
  f.import = 5;
  ROWNE(trescNocy(e), trescNocy(f));
  f.m17 = 1;
  SPRAWDZ(trescNocy(e) != trescNocy(f));
}

// ============================================================================================================
// (b) AirBridge: status, raport → pola wersji 2, wpis do historii
// ============================================================================================================

static void testStatusSrn() {
  StatusAB s;
  SPRAWDZ(parsujStatus("{\"version\":\"v2.2.1\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"srn\":\"SYN00000001\",\"rop\":0}", s));
  ROWNE(s.model, "AirCurve_10_CS_PaceWave");
  ROWNE(s.seryjny, "SYN00000001");
  SPRAWDZ(parsujStatus("{\"version\":\"v2.2.1\",\"srn\":12345}", s));
  ROWNE(s.seryjny, "");  // zły typ = brak odczytu
}

static void testRaportM13M17() {
  NocAB n = nocZRaportu(raportB1(DOBA, "3", "16.2"), DOBA, "ASVAuto", "AirCurve_10_CS_PaceWave", "SYN00000001");
  SPRAWDZ(n.stan == StanRaportu::KOMPLETNA);
  BLISKO(n.noc.m13, 16.2);
  ROWNE_L(n.noc.m17, 2);
  ROWNE(n.noc.pna, "AirCurve_10_CS_PaceWave");
  ROWNE(n.noc.srn, "SYN00000001");
  BLISKO(n.noc.m08, 30.0);
  // M-08: brak pola, null, zły typ, ujemne → brak (R-AW-22); 0 → 0 minut
  SPRAWDZ(std::isnan(nocZRaportu(raportB1(DOBA, "1", "16", false, "edf", ""), DOBA, "ASVAuto").noc.m08));
  SPRAWDZ(std::isnan(nocZRaportu(raportB1(DOBA, "1", "16", false, "edf", "null"), DOBA, "ASVAuto").noc.m08));
  SPRAWDZ(std::isnan(nocZRaportu(raportB1(DOBA, "1", "16", false, "edf", "\"60\""), DOBA, "ASVAuto").noc.m08));
  SPRAWDZ(std::isnan(nocZRaportu(raportB1(DOBA, "1", "16", false, "edf", "-60"), DOBA, "ASVAuto").noc.m08));
  SPRAWDZ(nocZRaportu(raportB1(DOBA, "1", "16", false, "edf", "0"), DOBA, "ASVAuto").noc.m08 == 0.0f);
  BLISKO(nocZRaportu(raportB1(DOBA, "1", "16", false, "edf", "90"), DOBA, "ASVAuto").noc.m08, 1.5);
  SPRAWDZ(nocZRaportu(raportB1(DOBA, "1", "16", false, "edf", ""), DOBA, "ASVAuto").stan == StanRaportu::KOMPLETNA);
  // brak `RespRate`, median null, zły typ: M-13 brak (R-DANE-07), reszta rekordu bez zmian
  NocAB b = nocZRaportu(raportB1(DOBA, "1", ""), DOBA, "ASVAuto");
  SPRAWDZ(b.stan == StanRaportu::KOMPLETNA && std::isnan(b.noc.m13) && b.noc.m17 == 0 && b.noc.wazna);
  SPRAWDZ(b.noc.pna.empty() && b.noc.srn.empty());
  SPRAWDZ(std::isnan(nocZRaportu(raportB1(DOBA, "1", "null"), DOBA, "ASVAuto").noc.m13));
  SPRAWDZ(std::isnan(nocZRaportu(raportB1(DOBA, "1", "\"16\""), DOBA, "ASVAuto").noc.m13));
  // `RespRate` ze STR: wprost (M-13 nie ma ograniczenia źródła w 4.3)
  BLISKO(nocZRaportu(raportB1(DOBA, "1", "15.0", false, "str"), DOBA, "ASVAuto").noc.m13, 15.0);
  // session_count 0 → M-17 0 (najmniej 0); brak pola, null albo tekst → -1 (brak)
  ROWNE_L(nocZRaportu(raportB1(DOBA, "0", "16"), DOBA, "ASVAuto").noc.m17, 0);
  ROWNE_L(nocZRaportu(raportB1(DOBA, "", "16"), DOBA, "ASVAuto").noc.m17, -1);
  ROWNE_L(nocZRaportu(raportB1(DOBA, "null", "16"), DOBA, "ASVAuto").noc.m17, -1);
  ROWNE_L(nocZRaportu(raportB1(DOBA, "\"2\"", "16"), DOBA, "ASVAuto").noc.m17, -1);
  ROWNE_L(nocZRaportu(raportB1(DOBA, "-2", "16"), DOBA, "ASVAuto").noc.m17, -1);
  ROWNE_L(nocZRaportu(raportB1(DOBA, "2.5", "16"), DOBA, "ASVAuto").noc.m17, -1);  // niecałkowita = brak
  ROWNE_L(nocZRaportu(raportB1(DOBA, "2.0", "16"), DOBA, "ASVAuto").noc.m17, 1);
  // `pna` z `;` w środku: czystePole
  NocAB c = nocZRaportu(raportB1(DOBA, "2", "16"), DOBA, "ASVAuto", "Air;Curve", "S;1\r");
  ROWNE(c.noc.pna, "AirCurve");
  ROWNE(c.noc.srn, "S1");
  ROWNE_L(dziel(wierszNocyV2(c.noc), ';').size(), 41);
  // noc częściowa: pola wersji 2 też są (rekord „częściowa”, do nadpisania kompletnym)
  NocAB d = nocZRaportu(raportB1(DOBA, "1", "17.5", true), DOBA, "ASVAuto", "P", "S");
  SPRAWDZ(d.stan == StanRaportu::CZESCIOWA);
  BLISKO(d.noc.m13, 17.5);
}

// Noc częściowa nadpisana kompletną: pola wersji 2 z kompletnej; kompletna bez `RespRate` dostaje M-13 częściowej? Nie:
// zasada wpiszNocAB — przy zastąpieniu rekordu A uzupełniane są tylko pola spoza AirBridge (PS), więc pola wersji 2 są
// z nowego raportu.
static void testCzesciowaNadpisanaKompletna() {
  std::vector<Noc> hist;
  NocAB cz = nocZRaportu(raportB1(DOBA, "1", "17.5", true), DOBA, "ASVAuto", "P", "S1");
  SPRAWDZ(wpiszNocAB(hist, cz.noc, false, cz.pelna));
  NocAB k = nocZRaportu(raportB1(DOBA, "2", "16.0"), DOBA, "ASVAuto", "P", "S2");
  SPRAWDZ(wpiszNocAB(hist, k.noc, false, k.pelna));
  ROWNE_L(hist.size(), 1);
  SPRAWDZ(!hist[0].czesciowa);
  BLISKO(hist[0].m13, 16.0);
  ROWNE_L(hist[0].m17, 1);
  ROWNE(hist[0].srn, "S2");
  // R-DANE-24: ponowne sprawdzenie o 11:00, zmienione tylko `session_count` → rekord nadpisany (M-17 z nowego)
  NocAB k2 = nocZRaportu(raportB1(DOBA, "3", "16.0"), DOBA, "ASVAuto", "P", "S2");
  SPRAWDZ(wpiszNocAB(hist, k2.noc, true, k2.pelna));
  ROWNE_L(hist[0].m17, 2);
  // ten sam raport jeszcze raz: bez zmiany
  SPRAWDZ(!wpiszNocAB(hist, k2.noc, true, k2.pelna));
  // kompletna ze zmienionym tylko M-13 przy ponownym sprawdzeniu: też zmiana
  NocAB k3 = nocZRaportu(raportB1(DOBA, "3", "15.0"), DOBA, "ASVAuto", "P", "S2");
  SPRAWDZ(wpiszNocAB(hist, k3.noc, true, k3.pelna));
  BLISKO(hist[0].m13, 15.0);
}

// Import z karty po nocy z AirBridge: pełny rekord A zostaje z polami wersji 2; noc A niepełna (częściowa) ustępuje nocy
// z karty, która dostaje pola wersji 2 z rekordu A.
static void testImportPoAirBridge() {
  std::vector<Noc> hist;
  NocAB a = nocZRaportu(raportB1(DOBA, "2", "16.0"), DOBA, "ASVAuto", "P", "S");
  SPRAWDZ(wpiszNocAB(hist, a.noc, false, a.pelna));
  Noc k = nocTestowa(a.noc.data, 1);
  k.zrodlo = 'K';
  wpiszImport(hist, k);
  ROWNE_L(hist.size(), 1);
  SPRAWDZ(hist[0].zrodlo == 'A');
  BLISKO(hist[0].m13, 16.0);
  ROWNE_L(hist[0].m17, 1);
  ROWNE(hist[0].pna, "P");
  ROWNE(hist[0].srn, "S");
  std::vector<Noc> h2;
  NocAB c = nocZRaportu(raportB1(DOBA, "2", "17.0", true), DOBA, "ASVAuto", "P", "S");
  SPRAWDZ(wpiszNocAB(h2, c.noc, false, c.pelna));
  SPRAWDZ(wpiszImport(h2, k) == WynikImportu::ZASTAPIONA);
  SPRAWDZ(h2[0].zrodlo == 'K');
  BLISKO(h2[0].m13, 17.0);
  ROWNE_L(h2[0].m17, 1);
  ROWNE(h2[0].srn, "S");
  // sama noc z karty: pola wersji 2 puste
  std::vector<Noc> h3;
  wpiszImport(h3, k);
  SPRAWDZ(std::isnan(h3[0].m13) && h3[0].m17 == -1 && h3[0].pna.empty() && h3[0].srn.empty());
  // pełna noc z AirBridge zastępuje noc z karty i ma swoje pola wersji 2
  SPRAWDZ(wpiszNocAB(h3, a.noc, false, a.pelna));
  BLISKO(h3[0].m13, 16.0);
  ROWNE(h3[0].srn, "S");
}

// ============================================================================================================
// (c) magazyn: format `noce` wersji 2, migracje
// ============================================================================================================

static std::string polaTekst(const std::vector<std::string>& p) {
  std::string t;
  for (size_t i = 0; i < p.size(); i++) t += (i ? ";" : "") + p[i];
  return t;
}

static void testRejestrNoceV2() {
  const FormatPliku* f = formatPliku(rejestrFormatow(), "noce");
  SPRAWDZ(f != nullptr);
  if (!f) return;
  ROWNE_L(f->wersja(), 3);  // wersja 3 (M-18)
  if (f->wersja() < 2) return;
  ROWNE(polaTekst(f->pola[0]), POLA_V1);
  ROWNE(polaTekst(f->pola[1]), POLA_V2);
  ROWNE_L(f->pola[1].size(), POLA_NOCY_V2);
  // pozostałe formaty bez zmian (wersja 1); `sypialnia` w wersji 2 (seria CO2 nocy)
  for (auto& x : rejestrFormatow())
    if (x.nazwa != "noce") ROWNE_L(x.wersja(), x.nazwa == "sypialnia" ? 2 : 1);
}

// Prawdziwy panel jest dziś w wersji 0 (płaski `/noce.csv`, 20 pól, źródła K i A, ostatnie pola czasem puste):
// syntetyczny plik z 412 nocami → migracja na nowe ścieżki i w miejscu → 412 rekordów w wersji 2, pola wersji 2 puste,
// pola wersji 0 bez zmian.
static void testMigracja0do2_412() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  std::vector<std::string> v0 = noceV0(412, "2026-11-03");
  pl.pliki["/noce.csv"] = plikV0(v0);
  Log lg;
  Magazyn m(pl, lg.f());
  SPRAWDZ(m.migrujNaNoweSciezki());
  SPRAWDZ(m.migrujWMiejscu());
  ROWNE_L(rekordyNocy(pl), 412);
  std::map<std::string, std::string> wgDaty;
  for (auto& s : m.sciezki("noce")) {
    std::string t = pl.tresc(s);
    SPRAWDZ(t.compare(0, 18, "#cpap-eink noce 3\n") == 0);
    std::vector<std::string> w = wiersze(t);
    SPRAWDZ(w.size() >= 3 && w[1] == POLA_V3);
    OdczytPliku o = m.czytaj("noce", s);
    SPRAWDZ(o.stan == StanPliku::DOBRY && o.wersja == 3);
    for (auto& x : o.wiersze) {
      SPRAWDZ(dziel(x, ';').size() == 42);
      wgDaty[x.substr(0, 10)] = x;
    }
  }
  ROWNE_L(wgDaty.size(), 412);
  int zgodne = 0, pusteB1 = 0, k = 0, a = 0;
  for (auto& l : v0) {
    Noc o, n;
    if (!parsujNoc(l, 'A', o)) continue;
    auto i = wgDaty.find(o.data);
    if (i == wgDaty.end() || !parsujNocV2(i->second, n)) continue;
    zgodne += zapiszNoc(n) == zapiszNoc(o) ? 1 : 0;
    pusteB1 += std::isnan(n.m13) && n.m17 == -1 && n.pna.empty() && n.srn.empty() && std::isnan(n.m08) ? 1 : 0;
    k += n.zrodlo == 'K';
    a += n.zrodlo == 'A';
  }
  ROWNE_L(zgodne, 412);
  ROWNE_L(pusteB1, 412);
  SPRAWDZ(k > 0 && a > 0);
  SPRAWDZ(!zawiera(lg.w, "błąd"));
}

// Plik wersji 1 (panel) → migracja w miejscu 1 → 2: kopia `.v1` bajt w bajt, ta sama liczba rekordów,
// wiersze = wiersze wersji 1 z pustymi polami wersji 2, zamiana atomowa (bez `.tmp`).
static void testMigracja1do2() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  std::vector<std::string> v1;
  for (auto& l : noceV0(30, "2026-11-03")) v1.push_back(l + ";1762149600");
  std::string t1 = plik("noce", 1, POLA_V1, v1);
  pl.pliki["/dane/noce/2026.csv"] = t1;
  Log lg;
  Magazyn m(pl, lg.f());
  OdczytPliku o = m.czytaj("noce", "/dane/noce/2026.csv");
  SPRAWDZ(o.stan == StanPliku::DOBRY && o.wersja == 1);
  SPRAWDZ(!m.zapisz("noce", "/dane/noce/2026.csv", {}));  // przed migracją zapis odrzucony (T-WYM-55)
  SPRAWDZ(m.migrujWMiejscu());
  ROWNE(pl.tresc("/dane/noce/2026.csv.v1"), t1);
  SPRAWDZ(!pl.istnieje("/dane/noce/2026.csv.tmp"));
  std::vector<std::string> w2;
  for (auto& l : v1) w2.push_back(l + PUSTE_B1 + ";");  // migracja do wersji 3 (puste `m18`)
  ROWNE(pl.tresc("/dane/noce/2026.csv"), plik("noce", 3, POLA_V3, w2));
  bool wynik = false;
  for (auto& x : m.migracje()) wynik = wynik || (x.ok && x.przed == 30 && x.po == 30);
  SPRAWDZ(wynik);
  // po migracji: rekord z polami wersji 2 zapisuje się i czyta
  Noc n;
  SPRAWDZ(parsujNocV2(w2.back(), n));
  n.m13 = 16.2f;
  n.m17 = 1;
  n.pna = "P";
  n.srn = "S";
  w2.back() = wierszNocyV3(n);
  SPRAWDZ(m.zapisz("noce", "/dane/noce/2026.csv", w2));
  OdczytPliku o2 = m.czytaj("noce", "/dane/noce/2026.csv");
  SPRAWDZ(o2.stan == StanPliku::DOBRY && o2.wersja == 3 && !o2.wiersze.empty() && o2.wiersze.back() == w2.back());
}

// Przywracanie ze zrzutu wersji 0 (T-WYM-62) na flash w wersji 2: noc z AirBridge we flash zostaje z polami wersji 2,
// noce zrzutu dochodzą z pustymi polami wersji 2, plik w wersji 2.
static void testPrzywracanieNaV2() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  Noc a = nocTestowa("2026-11-03", 7);
  a.zrodlo = 'A';
  a.m13 = 16.2f;
  a.m17 = 1;
  a.pna = "P";
  a.srn = "S";
  pl.pliki["/dane/noce/2026.csv"] = plik("noce", 3, POLA_V3, {wierszNocyV3(a)});  // flash w wersji 3
  pl.pliki["/przywracanie/noce.csv"] = plikV0(noceV0(30, "2026-11-03"));
  Log lg;
  Magazyn m(pl, lg.f());
  Magazyn::WynikPrzywracania w = m.przywrocZKatalogu("/przywracanie");
  SPRAWDZ(w.proba && w.ok);
  ROWNE_L(w.oczekiwane, 30);
  ROWNE_L(w.nowych, 29);
  std::string t = pl.tresc("/dane/noce/2026.csv");
  SPRAWDZ(t.compare(0, 18, "#cpap-eink noce 3\n") == 0);
  ROWNE(wierszZData(t, "2026-11-03"), wierszNocyV3(a));
  Noc n;
  SPRAWDZ(parsujNocV2(wierszZData(t, "2026-11-01"), n) && n.pna.empty() && std::isnan(n.m13));
  ROWNE_L(rekordyNocy(pl), 30);
}

// ============================================================================================================
// (d) dyrygent z atrapami: zapis pól przy pobraniu nocy, odczyt po następnym wybudzeniu, zrzut `Z`
// ============================================================================================================

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_B1 =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"srn\":\"SYN00000001\",\"mode\":\"ASVAuto\","
    "\"system\":\"IDLE\",\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static const long DOBA_A = nrDnia("2026-11-03");  // noc z poranka 4.11.2026
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":2,\"stale\":false}";
  return l + "]}";
}
struct PanelB1 {
  Panel p;
  KonfiguracjaPanelu konf = KONF;
  PanelB1() { zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji)); }
};
static TekstyWybudzenia wybudz(PanelB1& s, uint32_t t, PowodWybudzenia pw) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  if (pw == PowodWybudzenia::START) s.p.start.stan = StanRtc();
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.p.siec.polaczonaTeraz = false;
  s.p.siec.okno = false;
  s.p.d.z.clear();
  s.p.port.log.clear();
  Dyrygent dy(s.p.adaptery(), s.konf);
  dy.wybudzenie();
  return dy.teksty();
}
// Partycja jak panel po 30 nocy wersji 1 do poranka 3.11.2026.
static void zNocamiV1(PanelB1& s) {
  std::vector<std::string> v1;
  for (auto& l : noceV0(30, "2026-11-03")) v1.push_back(l + ";");
  s.p.pliki.pliki["/dane/noce/2026.csv"] = plik("noce", 1, POLA_V1, v1);
}

static void testDyrygentZapisujePolaB1() {
  PanelB1 s;
  zNocamiV1(s);
  s.p.siec.odpowiedzi.clear();
  s.p.siec.dodaj("/api/status", 200, STATUS_B1);
  s.p.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
  s.p.siec.dodaj(sciezkaNocy(DOBA_A), 200, raportB1(DOBA_A, "2", "16.2"));
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  std::string t = s.p.pliki.tresc("/dane/noce/2026.csv");
  SPRAWDZ(t.compare(0, 18, "#cpap-eink noce 3\n") == 0);
  std::string w = wierszZData(t, "2026-11-04");
  SPRAWDZ(!w.empty());
  std::vector<std::string> f = dziel(w, ';');
  ROWNE_L(f.size(), 42);  // wersja 3
  if (f.size() == 42) {
    ROWNE(f[21], "16.20");
    ROWNE(f[22], "1");
    ROWNE(f[23], "AirCurve_10_CS_PaceWave");
    ROWNE(f[24], "SYN00000001");
    ROWNE(f[25], "30.00");
  }
  // noce z pliku wersji 1: pola wersji 2 puste
  std::string st = wierszZData(t, "2026-11-03");
  SPRAWDZ(st.size() > 4 && st.compare(st.size() - PUSTE_B1.size() - 1, PUSTE_B1.size() + 1, PUSTE_B1 + ";") == 0);
  SPRAWDZ(s.p.pliki.istnieje("/dane/noce/2026.csv.v1"));
  // odczyt: partycja w wersji 2 z nocą 2.11 z polami wersji 2; pobranie nowej nocy przepisuje plik roczny, a noc 2.11
  // (wczytana z pliku) zostaje z polami wersji 2
  PanelB1 s2;
  std::vector<std::string> v2;
  for (auto& l : noceV0(30, "2026-11-03")) v2.push_back(l + ";" + PUSTE_B1);
  Noc n2;
  SPRAWDZ(parsujNocV2(v2[28], n2) && n2.data == "2026-11-02");
  n2.m13 = 14.5f;
  n2.m17 = 3;
  n2.pna = "AirCurve_10_CS_PaceWave";
  n2.srn = "SYN00000002";
  v2[28] = wierszNocyV2(n2);
  std::string t2przed = plik("noce", 2, POLA_V2, v2);
  s2.p.pliki.pliki["/dane/noce/2026.csv"] = t2przed;
  s2.p.siec.odpowiedzi.clear();
  s2.p.siec.dodaj("/api/status", 200, STATUS_B1);
  s2.p.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
  s2.p.siec.dodaj(sciezkaNocy(DOBA_A), 200, raportB1(DOBA_A, "1", "15.0"));
  wybudz(s2, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  std::string t2 = s2.p.pliki.tresc("/dane/noce/2026.csv");
  SPRAWDZ(t2 != t2przed);  // plik przepisany (noc 4.11 dopisana)
  ROWNE(wierszZData(t2, "2026-11-02"), wierszNocyV3(n2));
  SPRAWDZ(!wierszZData(t2, "2026-11-04").empty());
  SPRAWDZ(!s2.p.pliki.istnieje("/dane/noce/2026.csv.v1"));  // plik już w wersji 2: bez migracji
  // status bez `srn`: noc bez odczytu numeru (brak odczytu nie jest zmianą, R-NOR-25 fail-path)
  PanelB1 s3;
  zNocamiV1(s3);
  s3.p.siec.odpowiedzi.clear();
  s3.p.siec.dodaj("/api/status", 200,
                  "{\"version\":\"v2.2.1\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"rop\":0,"
                  "\"edf\":\"idle\",\"report_state\":\"idle\"}");
  s3.p.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
  s3.p.siec.dodaj(sciezkaNocy(DOBA_A), 200, raportB1(DOBA_A, "", "", false, "edf", ""));
  wybudz(s3, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  std::string w3 = wierszZData(s3.p.pliki.tresc("/dane/noce/2026.csv"), "2026-11-04");
  Noc n3;
  SPRAWDZ(parsujNocV2(w3, n3) && n3.pna == "AirCurve_10_CS_PaceWave" && n3.srn.empty() && std::isnan(n3.m08) &&
          std::isnan(n3.m13) && n3.m17 == -1 && n3.tryb == "ASVAuto");
}

// Zrzut `Z` na partycji w wersji 2: plik nocy w zrzucie z nagłówkiem wersji 2.
static void testZrzutZNaV2() {
  PanelB1 s;
  Noc a = nocTestowa("2026-11-03", 7);
  a.m13 = 16.2f;
  a.pna = "P";
  s.p.pliki.pliki["/dane/noce/2026.csv"] = plik("noce", 2, POLA_V2, {wierszNocyV2(a)});
  s.p.siec.odpowiedzi.clear();
  s.p.siec.dodaj("/api/status", 200, "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}");
  for (char c : std::string("Z")) s.p.port.znaki.push_back((unsigned char)c);
  wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(zawiera(s.p.port.log, "PLIK /dane/noce/2026.csv"));
  SPRAWDZ(zawiera(s.p.port.log, "#cpap-eink noce 3"));
  SPRAWDZ(zawiera(s.p.port.log, wierszNocyV3(a)));
}

// ============================================================================================================
// (e) Stany odchylenia M-03ca (CA/h, zdrowotna, bez nocy niepewnych) i M-17
//     (zdjęcia maski, techniczna) według 4.5–4.7 i R-NOR-03/04/06/09/19/22; noc niepewna z M-08 (4.9)
// ============================================================================================================

// Noc ważna `d` z CA/h `ca` i M-17 `m17` (reszta stała, bez odchyleń innych metryk).
static Noc nocStanu(const std::string& d, float ca, int m17) {
  Noc n;
  n.data = d;
  n.wazna = true;
  n.zrodlo = 'A';
  n.m01 = 420;
  n.ahi = 1.0f;
  n.wmed = 3.0f;
  n.wp95 = 10.0f;
  n.oa = 0.2f;
  n.h = 0.5f;
  n.ca = ca;
  n.m17 = m17;
  return n;
}
// `ile` nocy normy (CA 1,0/1,1/1,2; M-17 0/1) od `od`.
static std::vector<Noc> noceNormy(const std::string& od, int ile) {
  std::vector<Noc> h;
  for (int i = 0; i < ile; i++) h.push_back(nocStanu(dodajDni(od, i), 1.0f + 0.1f * (float)(i % 3), i % 2));
  return h;
}

static void testNocNiepewnaM08() {
  Noc n = nocStanu("2026-10-01", 1.0f, 0);
  SPRAWDZ(!nocNiepewna(n));  // M-08 brak, mediana wycieku 3: pewna
  n.m08 = 30.0f;
  SPRAWDZ(nocNiepewna(n));  // ≥ 30 min [PRÓG E-20.minuty]
  n.m08 = 29.0f;
  SPRAWDZ(!nocNiepewna(n));  // 29 min < 30 i < 10% z 420 min (42)
  n.m01 = 250;
  n.m08 = 25.0f;
  SPRAWDZ(nocNiepewna(n));  // 25 ≥ 10% z 250 [PRÓG E-20.odsetek_czasu]
  n.m08 = 24.0f;
  SPRAWDZ(!nocNiepewna(n));
  n.m08 = NAN;
  n.wmed = 31.0f;
  SPRAWDZ(nocNiepewna(n));  // drugi warunek bez zmian
  // wariant C: M-07 P95 ≥ 15 L/min [PRÓG R-NOR-05.niepewna_p95] też czyni noc niepewną,
  // jako „albo”: P95 14,9 bez innych warunków pewna, 15,0 niepewna; noc z M-08 ≥ 30 min dalej niepewna przy każdym P95;
  // brak P95 = pozostałe warunki (bez założenia „szczelna”, ADR-0029 pkt 7)
  Noc p = nocStanu("2026-10-02", 1.0f, 0);
  p.wmed = 3.0f;
  p.wp95 = 14.9f;
  SPRAWDZ(!nocNiepewna(p));
  p.wp95 = 15.0f;
  SPRAWDZ(nocNiepewna(p));
  p.wp95 = 22.0f;
  SPRAWDZ(nocNiepewna(p));  // 15–24 L/min: niepewna, choć bez dużego wycieku (E-20)
  p.wp95 = 10.0f;
  p.m08 = 30.0f;
  SPRAWDZ(nocNiepewna(p));  // warunek 24 L/min (M-08) zostaje
  p.wp95 = NAN;
  p.m08 = NAN;
  SPRAWDZ(!nocNiepewna(p));  // brak P95 i M-08, mediana 3: jak przed C
  p.m08 = 45.0f;
  SPRAWDZ(nocNiepewna(p));
}

static void testStanCa() {
  std::vector<Noc> h = noceNormy("2026-09-01", 30);  // do 30.09
  std::string d = "2026-10-01";
  Stany s0 = liczStany(h, "2026-09-30", {});
  SPRAWDZ(s0.norma.ca.jest() && !s0.ca.aktywny);
  // dwie noce z CA 3,0: jeszcze bez stanu; trzecia: wejście (3 z 5, R-NOR-03)
  for (int i = 0; i < 2; i++) h.push_back(nocStanu(dodajDni(d, i), 3.0f, 0));
  Stany s1 = liczStany(h, dodajDni(d, 1), {});
  SPRAWDZ(!s1.ca.aktywny && s1.odchylenie.ca);
  h.push_back(nocStanu(dodajDni(d, 2), 3.0f, 0));
  Stany s2 = liczStany(h, dodajDni(d, 2), {});
  SPRAWDZ(s2.ca.aktywny && s2.ca.wyzej);
  ROWNE(s2.ca.od, dodajDni(d, 2));
  SPRAWDZ(s2.wyl.ca.count(dodajDni(d, 2)) == 1);  // R-NOR-09: noc stanu poza normą CA
  SPRAWDZ(!s2.m17.aktywny && !s2.ahi.aktywny);
  // Δ +1 / h [PRÓG R-NOR-12.delta.M-03]: CA 2,0 przy medianie 1,1 i P90 1,2 to odchylenie, 2,05 − 1,1 < 1? nie: 0,95
  std::vector<Noc> hd = noceNormy("2026-09-01", 30);
  for (int i = 0; i < 3; i++) hd.push_back(nocStanu(dodajDni(d, i), 2.05f, 0));
  SPRAWDZ(!liczStany(hd, dodajDni(d, 2), {}).ca.aktywny);
  // metryka zdrowotna: po 21 nocach stan trwa (R-NOR-06 nie dotyczy, R-NOR-22: norma zamrożona)
  std::vector<Noc> hz = h;
  for (int i = 3; i < 30; i++) hz.push_back(nocStanu(dodajDni(d, i), 3.0f, 0));
  Stany sz = liczStany(hz, dodajDni(d, 29), {});
  SPRAWDZ(sz.ca.aktywny && sz.ca.nocy >= 21);
  // wyjście: 3 noce w granicy Δ/2 od mediany normy (R-NOR-04)
  std::vector<Noc> hw = h;
  for (int i = 3; i < 6; i++) hw.push_back(nocStanu(dodajDni(d, i), 1.1f, 0));
  Stany sw = liczStany(hw, dodajDni(d, 5), {});
  SPRAWDZ(!sw.ca.aktywny);
  // noce niepewne (M-08 ≥ 30 min) z wysokim CA się nie liczą (R-NOR-05, R-NOR-19)
  std::vector<Noc> hn = noceNormy("2026-09-01", 30);
  for (int i = 0; i < 5; i++) {
    Noc n = nocStanu(dodajDni(d, i), 3.0f, 0);
    n.m08 = 60.0f;
    hn.push_back(n);
  }
  Stany sn = liczStany(hn, dodajDni(d, 4), {});
  SPRAWDZ(!sn.ca.aktywny && sn.ostatniaNiepewna);
  SPRAWDZ(sn.wyl.m03.count(dodajDni(d, 4)) == 1);
  // CA brak (noce z karty): bez wartości, bez stanu
  std::vector<Noc> hk = noceNormy("2026-09-01", 30);
  for (int i = 0; i < 5; i++) hk.push_back(nocStanu(dodajDni(d, i), NAN, 0));
  SPRAWDZ(!liczStany(hk, dodajDni(d, 4), {}).ca.aktywny);
}

static void testStanM17() {
  std::vector<Noc> h = noceNormy("2026-09-01", 30);
  std::string d = "2026-10-01";
  Stany s0 = liczStany(h, "2026-10-01", {});  // norma z 30 nocy przed 1.10 (noc oceniana poza normą, A-10)
  SPRAWDZ(s0.norma.m17.jest());
  BLISKO(s0.norma.m17.med, 0.5);
  // M-17 = 3 w 3 nocach (Δ +2 [PRÓG R-NOR-12.delta.M-17], P90 = 1): wejście
  for (int i = 0; i < 3; i++) h.push_back(nocStanu(dodajDni(d, i), 1.1f, 3));
  Stany s1 = liczStany(h, dodajDni(d, 2), {});
  SPRAWDZ(s1.m17.aktywny && s1.m17.wyzej && s1.odchylenie.m17);
  ROWNE(s1.m17.od, dodajDni(d, 2));
  SPRAWDZ(s1.wyl.m17.count(dodajDni(d, 2)) == 1);
  SPRAWDZ(!s1.ca.aktywny);
  // M-17 = 2 (2 − 0,5 < 2): bez odchylenia
  std::vector<Noc> h2 = noceNormy("2026-09-01", 30);
  for (int i = 0; i < 3; i++) h2.push_back(nocStanu(dodajDni(d, i), 1.1f, 2));
  SPRAWDZ(!liczStany(h2, dodajDni(d, 2), {}).m17.aktywny);
  // metryka techniczna: po 21 nocach ważnych w stanie nowy poziom, stan się kończy, noce wracają do normy (R-NOR-06)
  std::vector<Noc> ht = h;
  for (int i = 3; i < 23; i++) ht.push_back(nocStanu(dodajDni(d, i), 1.1f, 3));
  Stany st = liczStany(ht, dodajDni(d, 22), {});
  SPRAWDZ(!st.m17.aktywny);
  SPRAWDZ(st.wyl.m17.empty());
  // noc niepewna liczy się do M-17 (R-NOR-05 dotyczy M-02, M-03, M-11)
  std::vector<Noc> hn = noceNormy("2026-09-01", 30);
  for (int i = 0; i < 3; i++) {
    Noc n = nocStanu(dodajDni(d, i), 1.1f, 3);
    n.m08 = 60.0f;
    hn.push_back(n);
  }
  SPRAWDZ(liczStany(hn, dodajDni(d, 2), {}).m17.aktywny);
  // brak M-17 (-1): noc bez wartości
  std::vector<Noc> hb = noceNormy("2026-09-01", 30);
  for (int i = 0; i < 5; i++) hb.push_back(nocStanu(dodajDni(d, i), 1.1f, -1));
  SPRAWDZ(!liczStany(hb, dodajDni(d, 4), {}).m17.aktywny);
  // pamięć wyniku: zmiana samego M-17 liczy stany od nowa
  std::vector<Noc> hp = noceNormy("2026-09-01", 30);
  for (int i = 0; i < 3; i++) hp.push_back(nocStanu(dodajDni(d, i), 1.1f, 0));
  SPRAWDZ(!stanyZPamieci(hp, dodajDni(d, 2), {}).m17.aktywny);
  for (int i = 30; i < 33; i++) hp[(size_t)i].m17 = 3;
  SPRAWDZ(stanyZPamieci(hp, dodajDni(d, 2), {}).m17.aktywny);
}

// Regresja (test symulatora): `/noce.csv` (wersja 0) zmieniony po migracji, obok pliki
// nowej ścieżki w wersji 1 (panel). Scalenie przy starcie (U6) nie może paść na pliku czekającym na migrację
// 1 → 2: zapis w jego własnej wersji (T-WYM-43: migracja w miejscu dopiero po oznaczeniu), potem migracja do wersji 2.
static void testScalenieZPlikiemWersji1() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  std::vector<std::string> v0 = noceV0(60, "2026-11-03");  // 5.09–3.11.2026
  pl.pliki["/noce.csv"] = plikV0(v0);
  std::vector<std::string> v1;
  for (int i = 0; i < 30; i++) v1.push_back(v0[(size_t)i] + ";");
  pl.pliki["/dane/noce/2026.csv"] = plik("noce", 1, POLA_V1, v1);
  pl.pliki[znacznikMigracji("noce")] = "ok 00000000\n";  // CRC innego (wcześniejszego) pliku prototypu
  Log lg;
  {
    Magazyn m(pl, lg.f());
    SPRAWDZ(m.migrujNaNoweSciezki());
    SPRAWDZ(!zawiera(lg.w, "nieudane"));
    std::string t = pl.tresc("/dane/noce/2026.csv");
    SPRAWDZ(t.compare(0, 18, "#cpap-eink noce 1\n") == 0);  // przed oznaczeniem: w swojej wersji
    ROWNE_L(rekordyNocy(pl), 60);
    SPRAWDZ(m.migrujWMiejscu());
  }
  SPRAWDZ(pl.tresc("/dane/noce/2026.csv").compare(0, 18, "#cpap-eink noce 3\n") == 0);
  ROWNE_L(rekordyNocy(pl), 60);
  // dyrygent: pierwsze wybudzenie po starcie ma 60 nocy w historii
  PanelB1 s;
  s.p.pliki.pliki["/noce.csv"] = plikV0(v0);
  s.p.pliki.pliki["/dane/noce/2026.csv"] = plik("noce", 1, POLA_V1, v1);
  s.p.pliki.pliki[znacznikMigracji("noce")] = "ok 00000000\n";
  s.p.siec.odpowiedzi.clear();
  s.p.siec.dodaj("/api/status", 200, "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}");
  TekstyWybudzenia tw = wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(zawiera(tw.diagnostykaTestu, "historia 60 nocy"));
  SPRAWDZ(!zawiera(s.p.port.log, "scalenie z plikami nowej ścieżki nieudane"));
  // przywracanie `W` na plik nowej ścieżki w wersji 1: też bez odrzucenia
  Dziennik d2;
  AtrapaPlikow p2(d2);
  montowana(p2);
  p2.pliki["/dane/noce/2026.csv"] = plik("noce", 1, POLA_V1, v1);
  p2.pliki["/przywracanie/noce.csv"] = plikV0(v0);
  Magazyn m2(p2, lg.f());
  Magazyn::WynikPrzywracania w = m2.przywrocZKatalogu("/przywracanie");
  SPRAWDZ(w.proba && w.ok);
  ROWNE_L(rekordyNocy(p2), 60);
}

// przeglądu ustawienia trybu ASVAuto (`EAI`, `EAX`, `ANS`, `AXS`), `EPR` i tryb nocy w rekordzie;:
// noce CPAP, AutoSet, ASV i ASVAuto ważne (ADR-0030), tryb spoza listy albo nieznany: zapisana, nieważna (R-DANE-29).
static void testUstawieniaAsvAutoITryb() {
  std::string rap = raportB1(DOBA, "1", "16");
  size_t i = rap.find("{\"tag\":\"MOP\"");
  rap.insert(i, "{\"tag\":\"EAI\",\"value\":\"6.0 cmH2O\"},{\"tag\":\"EAX\",\"value\":10},{\"tag\":\"ANS\",\"value\":\"3\"},"
                "{\"tag\":\"AXS\",\"value\":15.0},{\"tag\":\"EPR\",\"value\":3},");
  NocAB a = nocZRaportu(rap, DOBA, "");
  SPRAWDZ(a.stan == StanRaportu::KOMPLETNA && a.noc.wazna);
  BLISKO(a.noc.eai, 6.0);
  BLISKO(a.noc.eax, 10.0);
  BLISKO(a.noc.ans, 3.0);
  BLISKO(a.noc.axs, 15.0);
  BLISKO(a.noc.epr, 3.0);
  ROWNE(a.noc.tryb, "ASVAuto");
  std::string w = wierszNocyV2(a.noc);
  ROWNE_L(dziel(w, ';').size(), 41);
  const std::string ogon = ";6.00;10.00;3.00;15.00;3.00;ASVAuto";
  SPRAWDZ(w.size() > ogon.size() && w.substr(w.size() - ogon.size()) == ogon);
  Noc r;
  SPRAWDZ(parsujNocV2(w, r) && r.tryb == "ASVAuto");
  BLISKO(r.axs, 15.0);
  ROWNE(wierszNocyV2(r), w);
  // bez tagów: puste
  NocAB b = nocZRaportu(raportB1(DOBA, "1", "16"), DOBA, "");
  SPRAWDZ(std::isnan(b.noc.eai) && std::isnan(b.noc.eax) && std::isnan(b.noc.ans) && std::isnan(b.noc.axs) &&
          std::isnan(b.noc.epr));
  // tryby obsługiwane: noc ważna; spoza listy i nieznany: zapisana, nieważna, tryb w rekordzie
  for (const char* t : {"CPAP", "AutoSet", "ASV", "ASVAuto"}) {
    std::string x = raportB1(DOBA, "1", "16");
    x.replace(x.find("\"value\":\"ASVAuto\""), 17, std::string("\"value\":\"") + t + "\"");
    NocAB n = nocZRaportu(x, DOBA, "");
    SPRAWDZ(n.noc.wazna);
    ROWNE(n.noc.tryb, t);
  }
  std::string x = raportB1(DOBA, "1", "16");
  x.replace(x.find("\"value\":\"ASVAuto\""), 17, "\"value\":\"iVAPS\"");
  NocAB n = nocZRaportu(x, DOBA, "");
  SPRAWDZ(!n.noc.wazna && n.noc.tryb == "iVAPS" && n.stan == StanRaportu::KOMPLETNA);
  x = raportB1(DOBA, "1", "16");
  x.replace(x.find("\"tag\":\"MOP\""), 11, "\"tag\":\"XXX\"");
  n = nocZRaportu(x, DOBA, "");  // bez MOP i bez `mode` w statusie: nieznany
  SPRAWDZ(!n.noc.wazna && n.noc.tryb.empty());
  // trybNocy(): pole `tryb`, a bez niego pierwszy człon `ustawienia` (noce z karty i starsze)
  Noc k = nocTestowa("2026-11-01", 1);
  ROWNE(trybNocy(k), "ASVAuto");
  k.tryb = "AutoSet";
  ROWNE(trybNocy(k), "AutoSet");
  SPRAWDZ(trybObslugiwany("CPAP") && !trybObslugiwany("") && !trybObslugiwany("iVAPS"));
}

// Norma M-13 i CSR% (M-09 / M-01) według 4.4: 30 nocy ważnych, 14 minimum, bez filtra ustawień.
static void testNormaM13Csr() {
  std::vector<Noc> h;
  for (int i = 0; i < 30; i++) {
    Noc n = nocStanu(dodajDni("2026-09-01", i), 1.1f, 0);
    n.m13 = 15.0f + (float)(i % 3);  // 15, 16, 17
    n.csr = 42.0f;                   // 42 min z 420 = 10%
    n.ustawienia = i % 2 ? "ASVAuto/6/10/3/15" : "ASVAuto/6/11/3/15";
    h.push_back(n);
  }
  Norma nm = liczNorme(h, "2026-10-01", {});
  SPRAWDZ(nm.m13.jest() && nm.csrp.jest());
  BLISKO(nm.m13.med, 16.0);
  BLISKO(nm.m13.p90, 17.0);
  BLISKO(nm.csrp.med, 10.0);
  // mniej niż 14 nocy z wartością: brak normy
  std::vector<Noc> h2 = h;
  for (size_t i = 0; i < 17; i++) h2[i].m13 = NAN;
  SPRAWDZ(!liczNorme(h2, "2026-10-01", {}).m13.jest());
  // noc nieważna (np. tryb spoza wydania) poza normą
  std::vector<Noc> h3 = h;
  for (size_t i = 0; i < 17; i++) h3[i].wazna = false;
  SPRAWDZ(!liczNorme(h3, "2026-10-01", {}).csrp.jest());
}

// przegląd stan M-05p (ciśnienie P95) z nocy CPAP i AutoSet; EPAP P95 tylko z nocy ASV i ASVAuto (4.11).
static void testStanM05pTrybami() {
  auto noc = [](const std::string& d, const char* tryb, float cp95, float ep95) {
    Noc n = nocStanu(d, 1.1f, 0);
    n.tryb = tryb;
    n.ustawienia = std::string(tryb) + "/5/15";
    n.cmed = 9.0f;
    n.cp95 = cp95;
    n.emed = 7.0f;
    n.ep95 = ep95;
    return n;
  };
  std::vector<Noc> h;
  for (int i = 0; i < 30; i++) h.push_back(noc(dodajDni("2026-09-01", i), "AutoSet", 11.0f + 0.1f * (float)(i % 3), 8.0f));
  std::string d = "2026-10-01";
  for (int i = 0; i < 3; i++) h.push_back(noc(dodajDni(d, i), "AutoSet", 14.0f, 12.0f));
  Stany s = liczStany(h, dodajDni(d, 2), {});
  SPRAWDZ(s.cp95.aktywny && s.odchylenie.cp95);
  SPRAWDZ(!s.ep95.aktywny);  // EPAP w AutoSet to ciśnienie wydechowe przy EPR, bez stanu (4.11)
  SPRAWDZ(s.wyl.cp95.count(dodajDni(d, 2)) == 1);
  SPRAWDZ(!s.uspokojenieOcenione);
  // Δ +2: 13,0 − 11,1 < 2 → bez stanu
  std::vector<Noc> hd(h.begin(), h.begin() + 30);
  for (int i = 0; i < 3; i++) hd.push_back(noc(dodajDni(d, i), "AutoSet", 13.0f, 8.0f));
  SPRAWDZ(!liczStany(hd, dodajDni(d, 2), {}).cp95.aktywny);
  // te same liczby w ASVAuto: stan EPAP P95, bez M-05p
  std::vector<Noc> ha;
  for (int i = 0; i < 30; i++) ha.push_back(noc(dodajDni("2026-09-01", i), "ASVAuto", 11.0f + 0.1f * (float)(i % 3), 8.0f + 0.1f * (float)(i % 3)));
  for (int i = 0; i < 3; i++) ha.push_back(noc(dodajDni(d, i), "ASVAuto", 14.0f, 11.0f));
  Stany sa = liczStany(ha, dodajDni(d, 2), {});
  SPRAWDZ(sa.ep95.aktywny && !sa.cp95.aktywny);
  // E-36 w trybie CPAP/AutoSet z odchylenia M-05p w ≥ 5 z 7 nocy
  std::vector<Noc> he(h.begin(), h.begin() + 30);
  for (int i = 0; i < 5; i++) he.push_back(noc(dodajDni(d, i), "CPAP", 14.0f, 8.0f));
  for (auto& x : he) { x.tryb = "CPAP"; x.ustawienia = "CPAP/5/15"; }
  SPRAWDZ(liczStany(he, dodajDni(d, 4), {}).e36);
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testPolaDomyslne();
  testWierszV2();
  testPnaZSrednikiem();
  testPolaUstawienUaM10();
  testParsujV1NaWierszuV2();
  testParsujV2Brzegi();
  testUzupelnijNowePola();
  testStatusSrn();
  testRaportM13M17();
  testCzesciowaNadpisanaKompletna();
  testImportPoAirBridge();
  testRejestrNoceV2();
  testMigracja0do2_412();
  testMigracja1do2();
  testPrzywracanieNaV2();
  testDyrygentZapisujePolaB1();
  testZrzutZNaV2();
  testNocNiepewnaM08();
  testStanCa();
  testStanM17();
  testScalenieZPlikiemWersji1();
  testUstawieniaAsvAutoITryb();
  testNormaM13Csr();
  testStanM05pTrybami();
  printf("testy_rekord_nocy: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
