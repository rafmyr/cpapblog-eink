// SPDX-License-Identifier: GPL-3.0-or-later
// Testy wzorców L i hipotez H. Pisane przed kodem; niezależne testy przypadków brzegowych:
// testy_wzorce_l_hipotezy_h_brzegowe.cpp.
//
// Część 1 (zawsze): katalog docs/spec/katalog/{L,H,teksty}.csv wobec spec (stan według ADR-0029, progi z progi.csv,
// tekst dosłownie w spec, granica ADR-0002 na liście słów) oraz stany wzorca 5.12.12, „wiem”, „Omówione”, hipoteza
// i „Otwarte sprawy” (src/wzorce.cpp) na wynikach syntetycznych, bez silnika.
// Część 2 (silnik reguł, rodzaje E): każdy wzorzec L z zestawu wbudowanego na
// syntetycznej historii nocy: spełniony, niespełniony, na progu, za mało nocy → „nie oceniam”, noc niepewna;
// przypadki graniczne 11.9 spec technicznej, które da się sprawdzić na rekordzie nocy (T-TEST-161, 163, 164,
// 167, 168, 169, 171, 172, 181, 183, 184, 191, 192 w brzmieniu spec funkcjonalnej po przeglądzie klinicznym).
// Dane wyłącznie syntetyczne.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_wzorce_l_hipotezy_h z argumentem: katalog repozytorium).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <ctime>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "noc.h"
#include "reguly.h"
#include "reguly_pamiec.h"
#include "sprawy.h"
#include "stany.h"
#include "wspolne.h"
#include "wybudzenie.h"
#include "wzorce.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE(a, b)                                                                                   \
  do {                                                                                                \
    std::string a_ = (a), b_ = (b);                                                                   \
    if (a_ == b_) ok++;                                                                               \
    else { zle++; printf("BŁĄD %s:%d: %s = „%s”, oczekiwane „%s”\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } \
  } while (0)

using reguly::Pilnosc;
using reguly::StanReguly;
using reguly::Wynik;
using wzorce::Karta;
using wzorce::StanWzorca;

static std::string REPO = ".";

static std::vector<std::string> linie(const std::string& sciezka) {
  std::ifstream f(sciezka);
  std::vector<std::string> r;
  std::string l;
  while (std::getline(f, l)) r.push_back(l);
  return r;
}
static std::vector<std::string> dziel(const std::string& s, char sep) {
  std::vector<std::string> r;
  std::string cur;
  for (char c : s) {
    if (c == sep) { r.push_back(cur); cur.clear(); }
    else cur += c;
  }
  r.push_back(cur);
  return r;
}
// CSV z przecinkami i cudzysłowami (progi.csv)
static std::vector<std::string> polaCsv(const std::string& l) {
  std::vector<std::string> r;
  std::string cur;
  bool q = false;
  for (size_t i = 0; i < l.size(); ++i) {
    char c = l[i];
    if (q) {
      if (c == '"' && i + 1 < l.size() && l[i + 1] == '"') { cur += '"'; ++i; }
      else if (c == '"') q = false;
      else cur += c;
    } else if (c == '"') q = true;
    else if (c == ',') { r.push_back(cur); cur.clear(); }
    else cur += c;
  }
  r.push_back(cur);
  return r;
}

// Zestaw testowy bez parsera progi z progi.csv (wartość liczbowa albo tekst), katalog L i H, teksty.
static reguly::Zestaw zestawTestowy() {
  reguly::Zestaw z;
  z.identyfikator = "test-b4";
  auto p = linie(REPO + "/docs/spec/progi.csv");
  for (size_t i = 1; i < p.size(); ++i) {
    auto f = polaCsv(p[i]);
    if (f.size() < 9 || f[0].empty()) continue;
    reguly::Prog pr;
    pr.id = f[0];
    pr.jednostka = f[4];
    pr.tryb = f[8];
    char* kon = nullptr;
    pr.wartosc = std::strtod(f[3].c_str(), &kon);
    if (kon == f[3].c_str() || *kon) pr.tekst = f[3];
    if (!z.progi.count(pr.id)) z.progi[pr.id] = pr;
  }
  for (const char* plik : {"/docs/spec/katalog/L.csv", "/docs/spec/katalog/H.csv"}) {
    auto k = linie(REPO + plik);
    for (size_t i = 1; i < k.size(); ++i) {
      auto f = dziel(k[i], ';');
      if (f.size() != 20) continue;
      reguly::WierszKatalogu w;
      w.id = f[0];
      w.wariant = f[1];
      w.rodzaj = f[2];
      if (!f[16].empty()) w.teksty = dziel(f[16], ',');
      w.stan = f[17] == "aktywna" ? StanReguly::AKTYWNA
                                  : (f[17] == "tylko_raport" ? StanReguly::TYLKO_RAPORT : StanReguly::ZABLOKOWANA);
      z.katalog.push_back(w);
    }
  }
  auto t = linie(REPO + "/docs/spec/katalog/teksty.csv");
  for (size_t i = 1; i < t.size(); ++i) {
    auto f = dziel(t[i], ';');
    if (f.size() < 4) continue;
    reguly::Tekst tt;
    tt.id = f[0];
    tt.rola = f[1];
    tt.limit = std::atoi(f[2].c_str());
    tt.tekst = t[i].substr(f[0].size() + f[1].size() + f[2].size() + 3);
    z.teksty[tt.id] = tt;
  }
  return z;
}

// ================= część 1a: katalog wobec spec =================
static void testKatalog() {
  printf("katalog L/H: pola, stan według ADR-0029, progi, teksty dosłownie w spec, granica ADR-0002\n");
  std::set<std::string> progi;
  auto p = linie(REPO + "/docs/spec/progi.csv");
  std::map<std::string, std::string> wartosciProgow;
  for (size_t i = 1; i < p.size(); ++i) {
    auto f = polaCsv(p[i]);
    progi.insert(f[0]);
    if (f.size() > 3 && !wartosciProgow.count(f[0])) wartosciProgow[f[0]] = f[3];
  }
  std::ifstream fs(REPO + "/docs/spec/specyfikacja-funkcjonalna.md");
  std::stringstream ss;
  ss << fs.rdbuf();
  const std::string spec = ss.str();
  std::map<std::string, std::string> teksty;
  auto t = linie(REPO + "/docs/spec/katalog/teksty.csv");
  SPRAWDZ(!t.empty() && t[0] == "id_tekstu;rola;limit;tekst");
  for (size_t i = 1; i < t.size(); ++i) {
    auto f = dziel(t[i], ';');
    if (f.size() < 4 || (f[0].compare(0, 1, "L") != 0 && f[0].compare(0, 2, "H-") != 0)) continue;
    std::string tekst = t[i].substr(f[0].size() + f[1].size() + f[2].size() + 3);
    teksty[f[0]] = tekst;
    // {prog:id} → wartość progu jak w spec (przecinek dziesiętny), potem tekst musi stać dosłownie w spec
    for (size_t a = tekst.find("{prog:"); a != std::string::npos; a = tekst.find("{prog:")) {
      size_t b = tekst.find('}', a);
      std::string id = tekst.substr(a + 6, b - a - 6), v = wartosciProgow[id];
      for (auto& ch : v)
        if (ch == '.') ch = ',';
      SPRAWDZ(!v.empty());
      tekst.replace(a, b - a + 1, v);
    }
    if (spec.find(tekst) == std::string::npos) { zle++; printf("BŁĄD: tekst %s nie stoi dosłownie w spec\n", f[0].c_str()); }
    else ok++;
    std::string male;
    for (char c : tekst) male += static_cast<char>((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    for (const char* zakazane : {"pilnie", "natychmiast", "112", "obniż", "podnieś", "zwiększ", "zmniejsz", "ustaw ",
                                 "menu", "/api/settings", "airbridge"})
      if (male.find(zakazane) != std::string::npos) { zle++; printf("BŁĄD: %s ma słowo zakazane „%s”\n", f[0].c_str(), zakazane); }
    // ramka hipotezy: obszar bez liczby (ADR-0012)
    if (f[1] == "ramka") {
      bool cyfra = false;
      for (char c : tekst) cyfra = cyfra || (c >= '0' && c <= '9');
      SPRAWDZ(!cyfra);
    }
    const int limit = std::atoi(f[2].c_str());
    if (limit > 0) SPRAWDZ(static_cast<int>(tekst.size()) <= limit * 2);  // bajty UTF-8; dokładne limity: skrypt budowy zestawu
  }

  // oczekiwany stan wierszy głównych (ADR-0029)
  const std::map<std::string, std::pair<std::string, std::string>> glowne = {
      {"L-01/", {"tylko_raport", ""}},  {"L-01/cpap", {"tylko_raport", ""}}, {"L-04/autoset", {"tylko_raport", ""}},
      {"L-05/infekcja", {"tylko_raport", ""}}, {"L-07/cpap", {"tylko_raport", ""}}, {"L-02/a", {"tylko_raport", ""}}, {"L-02/b", {"tylko_raport", ""}},
      {"L-03/ge10", {"aktywna", "B"}}, {"L-03/5_10", {"tylko_raport", ""}}, {"L-04/", {"tylko_raport", ""}},
      {"L-05/", {"tylko_raport", ""}},  {"L-06/", {"zablokowana", ""}},     {"L-07/", {"tylko_raport", ""}},
      {"L-08/", {"aktywna", "C"}},     {"L-08/szybki", {"aktywna", "B"}},  {"L-09/", {"aktywna", ""}},
      {"L-10/", {"tylko_raport", ""}},  {"L-11/", {"aktywna", "B"}},       {"L-11/2tyg", {"aktywna", "B"}},
      {"L-12/", {"tylko_raport", ""}}};
  std::map<std::string, std::string> stanGalezi;
  std::set<std::string> widziane;
  for (const char* plik : {"/docs/spec/katalog/L.csv", "/docs/spec/katalog/H.csv"}) {
    auto k = linie(REPO + plik);
    SPRAWDZ(k.size() > 1);
    SPRAWDZ(k[0] ==
            "id_reguly;wariant;rodzaj;parametry;liczone_noce;okno;poziom;grupa;slot;tryby;wymaga_normy;w_rozruchu;"
            "wyklucza;wymaga;bramki;limit;teksty;stan;pilnosc;zrodlo");
    for (size_t i = 1; i < k.size(); ++i) {
      auto f = dziel(k[i], ';');
      if (f.size() != 20) { zle++; printf("BŁĄD: %s wiersz %zu ma %zu pól\n", plik, i, f.size()); continue; }
      const std::string id = f[0], war = f[1], stan = f[17], pil = f[18];
      SPRAWDZ(stan == "aktywna" || stan == "tylko_raport" || stan == "zablokowana");
      SPRAWDZ(pil.empty() || pil == "A" || pil == "B" || pil == "C");
      if (stan != "aktywna") SPRAWDZ(pil.empty());       // „tylko raport” i zablokowana bez pilności
      SPRAWDZ(!f[19].empty());                         // zrodlo
      for (const auto& tr : dziel(f[9], ','))
        SPRAWDZ(tr == "CPAP" || tr == "AutoSet" || tr == "ASV" || tr == "ASVAuto");  // ADR-0030
      // zablokowane tylko z powodem: blokada w spec (L-06, R-SKI-42; H-01 i H-04 w ASV) albo brak danych
      if (stan == "zablokowana")
        SPRAWDZ(f[19].find("brak danych") != std::string::npos || f[19].find("blokada w spec") != std::string::npos ||
                f[19].find("zablokowane do P-0") != std::string::npos || f[19].find("R-SKI-42") != std::string::npos);
      // parametry: liczby wyłącznie przez id_progu
      for (const auto& kv : dziel(f[3], ',')) {
        if (kv.empty()) continue;
        size_t r = kv.find('=');
        SPRAWDZ(r != std::string::npos);
        const std::string nazwa = kv.substr(0, r), v = kv.substr(r + 1);
        static const std::set<std::string> numeryczne = {"prog", "kzn", "n", "delta", "razy", "min", "max", "okno_a",
                                                         "okno_b", "margines", "w_ostatnich", "noc"};
        if (numeryczne.count(nazwa)) {
          if (!progi.count(v)) { zle++; printf("BŁĄD: %s/%s parametr %s=%s bez wiersza w progi.csv\n", id.c_str(), war.c_str(), nazwa.c_str(), v.c_str()); }
          else ok++;
        }
        SPRAWDZ(!(v.size() && v[0] >= '0' && v[0] <= '9'));
        // kzn wskazuje próg `k/n`, liczby nocy i tygodni wskazują liczbę (silnik reguł odrzuca inaczej)
        if (wartosciProgow.count(v)) {
          const bool kn = wartosciProgow[v].find('/') != std::string::npos;
          if (nazwa == "kzn") SPRAWDZ(kn);
          if (nazwa == "n" || nazwa == "min" || nazwa == "max" || nazwa == "okno_a" || nazwa == "okno_b" ||
              nazwa == "w_ostatnich")
            SPRAWDZ(!kn);
        }
      }
      if (!f[5].empty()) SPRAWDZ(progi.count(f[5]) == 1 && wartosciProgow[f[5]].find('/') == std::string::npos);
      for (const auto& tid : dziel(f[16], ','))
        if (!tid.empty()) {
          if (!teksty.count(tid)) { zle++; printf("BŁĄD: brak tekstu %s\n", tid.c_str()); }
          else ok++;
        }
      const bool pomocniczy = war.find('.') != std::string::npos;
      if (id.compare(0, 2, "L-") == 0) {
        std::string galaz = pomocniczy ? war.substr(0, war.find('.')) : war;
        std::string kl = id + "/" + galaz;
        if (!pomocniczy) {
          widziane.insert(kl);
          auto it = glowne.find(kl);
          if (it == glowne.end()) { zle++; printf("BŁĄD: nieoczekiwany wiersz główny %s\n", kl.c_str()); }
          else { ROWNE(stan, it->second.first); ROWNE(pil, it->second.second); }
          stanGalezi[kl] = stan;
        } else if (stanGalezi.count(kl)) {
          ROWNE(stan, stanGalezi[kl]);  // pomocniczy ma stan swojego wzorca
        }
      } else {
        SPRAWDZ(pil.empty());  // H bez pilności
        if (stan == "aktywna") SPRAWDZ((id == "H-08" || id == "H-09") && (war == "L-11" || war == "L-11~2tyg"));
      }
    }
  }
  for (const auto& g : glowne) SPRAWDZ(widziane.count(g.first) == 1);
}

// ================= część 1b: stany wzorca na wynikach syntetycznych =================
static Wynik W(const std::string& id, const std::string& war, bool oc, bool sp, StanReguly st = StanReguly::AKTYWNA,
               Pilnosc pil = Pilnosc::BRAK, const std::string& x = "") {
  Wynik w;
  w.id = id;
  w.wariant = war;
  w.oceniona = oc;
  w.spelniona = sp;
  w.stan = st;
  w.pilnosc = pil;
  if (!x.empty()) w.zmienne["x"] = x;
  if (!oc) w.powodNieoceny = "za malo nocy 9/14";
  return w;
}

// Wzorce L otwarte dziś (stan inny niż nieaktywny) z wzorce::info(), w kolejności pilności jak R-EKR-48
// (A, B, „problem”, C, „tylko raport”); listę spraw na ekranie robi (sprawy::sprawyOtwarte).
static int rangaInfo(const wzorce::InfoWzorca& i) {
  if (i.karta == Karta::W_RAPORCIE) return 0;
  if (i.karta == Karta::NA_KONTROLE) return 1;
  if (i.karta == Karta::PROBLEM) return 2;
  return i.pilnosc == Pilnosc::A ? 4 : 3;
}
static std::vector<wzorce::InfoWzorca> otwarte(const std::vector<Wynik>& w, const reguly::Pamiec& p,
                                               const std::string& dzis, const reguly::Zestaw* z) {
  std::vector<wzorce::InfoWzorca> r;
  for (const auto& k : wzorce::klucze(w)) {
    wzorce::InfoWzorca i = wzorce::info(w, p, k, dzis, z);
    if (i.stan != StanWzorca::NIEAKTYWNY) r.push_back(i);
  }
  std::stable_sort(r.begin(), r.end(),
                   [](const wzorce::InfoWzorca& a, const wzorce::InfoWzorca& b) { return rangaInfo(a) > rangaInfo(b); });
  return r;
}
static StanWzorca stanW(const reguly::Pamiec& p, const std::string& k) {
  auto it = p.find(k);
  return it == p.end() ? StanWzorca::NIEAKTYWNY : wzorce::odczytajZapis(it->second.stanWzorca).stan;
}
// L-03 (≥ 10) aktywna B z wierszem wyjścia
static std::vector<Wynik> l03(bool wej, bool wyj, const std::string& x = "", bool oc = true) {
  return {W("L-03", "ge10.mediana", oc, wej, StanReguly::AKTYWNA), W("L-03", "ge10", oc, wej, StanReguly::AKTYWNA, Pilnosc::B, x),
          W("L-03", "ge10.wyjscie", oc, wyj, StanReguly::AKTYWNA)};
}

static void testStany(const reguly::Zestaw& z) {
  printf("stany wzorca L (5.12.12): wejście, utrzymanie, wyjście, nie oceniam, tylko raport, zablokowana\n");
  const std::string d0 = "2026-11-01";
  {  // wejście, utrzymanie, wyjście
    reguly::Pamiec p;
    wzorce::aktualizuj(l03(true, false, "10,5"), p, d0, &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::AKTYWNY);
    auto s = otwarte(l03(true, false), p, d0, &z);
    SPRAWDZ(s.size() == 1);
    if (s.size() == 1) {
      ROWNE(s[0].klucz, "L-03/ge10");
      SPRAWDZ(s[0].pilnosc == Pilnosc::B);
      SPRAWDZ(s[0].karta == Karta::PELNY_EKRAN);
      ROWNE(s[0].idTytulu, "L-03.tytul");
      ROWNE(s[0].idPowodu, "L-03.powod");
      ROWNE(s[0].wejscie, d0);
    }
    wzorce::aktualizuj(l03(false, false), p, dodajDni(d0, 1), &z);  // utrzymanie bez wyjścia (histereza)
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::AKTYWNY);
    wzorce::aktualizuj(l03(false, true), p, dodajDni(d0, 2), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::NIEAKTYWNY);
    SPRAWDZ(otwarte(l03(false, true), p, dodajDni(d0, 2), &z).empty());
  }
  {  // wejście i wyjście w nocy wejścia: wejście wygrywa ((c)); następnej nocy wyjście działa
    reguly::Pamiec p;
    wzorce::aktualizuj(l03(true, true), p, d0, &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::AKTYWNY);
    wzorce::aktualizuj(l03(true, true), p, dodajDni(d0, 1), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::NIEAKTYWNY);
  }
  {  // nie oceniam: brak wejścia, nie jest sprawą, ale jest na liście nieocenionych (nigdy „w porządku”)
    reguly::Pamiec p;
    auto w = l03(false, false, "", false);
    wzorce::aktualizuj(w, p, d0, &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::NIEAKTYWNY);
    SPRAWDZ(otwarte(w, p, d0, &z).empty());
    auto n = wzorce::nieocenione(w);
    SPRAWDZ(n.size() == 1 && n[0].first == "L-03/ge10" && n[0].second == "za malo nocy 9/14");
    // aktywny wzorzec przy „nie oceniam” trwa (5.12.12, ostatni wiersz)
    wzorce::aktualizuj(l03(true, false), p, d0, &z);
    wzorce::aktualizuj(l03(false, false, "", false), p, dodajDni(d0, 1), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::AKTYWNY);
  }
  {  // tylko raport: sprawa „w raporcie, bez pilności”, bez hipotezy, bez „wiem” i „Omówione”
    reguly::Pamiec p;
    std::vector<Wynik> w = {W("L-04", ".epap", true, true, StanReguly::TYLKO_RAPORT),
                            W("L-04", "", true, true, StanReguly::TYLKO_RAPORT),
                            W("L-04", ".wyjscie", true, false, StanReguly::TYLKO_RAPORT),
                            W("H-02", "L-04", true, true, StanReguly::TYLKO_RAPORT)};
    wzorce::aktualizuj(w, p, d0, &z);
    SPRAWDZ(stanW(p, "L-04") == StanWzorca::AKTYWNY);
    auto s = otwarte(w, p, d0, &z);
    SPRAWDZ(s.size() == 1);
    if (s.size() == 1) {
      SPRAWDZ(s[0].karta == Karta::W_RAPORCIE);
      SPRAWDZ(s[0].pilnosc == Pilnosc::BRAK);
      SPRAWDZ(s[0].stanReguly == StanReguly::TYLKO_RAPORT);
      ROWNE(s[0].hipoteza, "");
      ROWNE(s[0].idPowodu, "L-04.powod_asvauto");
      ROWNE(s[0].idTytulu, "L-04.tytul_asvauto");
    }
    ROWNE(wzorce::hipoteza("L-04", w), "");
    SPRAWDZ(!wzorce::wiem(p, "L-04", d0));
    SPRAWDZ(!wzorce::omowione(p, "L-04", d0));
    // „tylko raport” bez pełnego ekranu w pilności docelowej C: powód = linia C (L-07, 5.4.15)
    std::vector<Wynik> w7 = {W("L-07", "", true, true, StanReguly::TYLKO_RAPORT),
                             W("L-07", ".wyjscie", true, false, StanReguly::TYLKO_RAPORT)};
    auto s7 = otwarte(w7, p, d0, &z);
    SPRAWDZ(s7.size() == 1 && s7[0].idPowodu == "L-07.linia_asv" && s7[0].karta == Karta::W_RAPORCIE);
  }
  {  // zablokowana L-06: nie liczy się wcale, nawet z „spełnionym” wynikiem
    reguly::Pamiec p;
    std::vector<Wynik> w = {W("L-06", "", true, true, StanReguly::ZABLOKOWANA)};
    wzorce::aktualizuj(w, p, d0, &z);
    SPRAWDZ(p.empty());
    SPRAWDZ(otwarte(w, p, d0, &z).empty());
    SPRAWDZ(wzorce::nieocenione(w).empty());
  }

  printf("„wiem” i „Omówione z lekarzem” (5.12.7): okres, Δ powrotu, zmiana ustawień, wygaszenie B\n");
  {  // wiem B: 7 dni
    reguly::Pamiec p;
    wzorce::aktualizuj(l03(true, false, "10,5"), p, d0, &z);
    SPRAWDZ(wzorce::wiem(p, "L-03/ge10", d0));
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::WIEM);
    auto s = otwarte(l03(true, false), p, d0, &z);
    SPRAWDZ(s.size() == 1 && s[0].stan == StanWzorca::WIEM && s[0].wiemDo == dodajDni(d0, 7));
    for (int i = 1; i <= 6; ++i) wzorce::aktualizuj(l03(true, false, "10,5"), p, dodajDni(d0, i), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::WIEM);
    wzorce::aktualizuj(l03(true, false, "10,5"), p, dodajDni(d0, 7), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::AKTYWNY);
  }
  {  // wiem: Δ powrotu L dla AHI = +3 / h (R-LEK-10.delta_ahi)
    reguly::Pamiec p;
    wzorce::aktualizuj(l03(true, false, "10,5"), p, d0, &z);
    wzorce::wiem(p, "L-03/ge10", d0);
    wzorce::aktualizuj(l03(true, false, "13,4"), p, dodajDni(d0, 1), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::WIEM);
    wzorce::aktualizuj(l03(true, false, "13,5"), p, dodajDni(d0, 2), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::AKTYWNY);
  }
  {  // wiem A: 3 dni (syntetyczny wzorzec A; w wersji beta A ma tylko zablokowane L-06(b))
    reguly::Pamiec p;
    std::vector<Wynik> a = {W("L-01", "", true, true, StanReguly::AKTYWNA, Pilnosc::A),
                            W("L-01", ".wyjscie", true, false)};
    wzorce::aktualizuj(a, p, d0, &z);
    SPRAWDZ(wzorce::wiem(p, "L-01", d0));
    wzorce::aktualizuj(a, p, dodajDni(d0, 2), &z);
    SPRAWDZ(stanW(p, "L-01") == StanWzorca::WIEM);
    wzorce::aktualizuj(a, p, dodajDni(d0, 3), &z);
    SPRAWDZ(stanW(p, "L-01") == StanWzorca::AKTYWNY);
  }
  {  // omówione B: 56 nocy; wyjście kasuje wyciszenie
    reguly::Pamiec p;
    wzorce::aktualizuj(l03(true, false, "10,5"), p, d0, &z);
    SPRAWDZ(wzorce::omowione(p, "L-03/ge10", d0));
    auto s = otwarte(l03(true, false), p, d0, &z);
    SPRAWDZ(s.size() == 1 && s[0].omowioneOd == d0);
    wzorce::aktualizuj(l03(true, false, "10,5"), p, dodajDni(d0, 55), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::OMOWIONE);
    wzorce::aktualizuj(l03(true, false, "10,5"), p, dodajDni(d0, 56), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") != StanWzorca::OMOWIONE);
    reguly::Pamiec q;
    wzorce::aktualizuj(l03(true, false), q, d0, &z);
    wzorce::omowione(q, "L-03/ge10", d0);
    wzorce::aktualizuj(l03(false, true), q, dodajDni(d0, 5), &z);
    SPRAWDZ(stanW(q, "L-03/ge10") == StanWzorca::NIEAKTYWNY);
    SPRAWDZ(otwarte(l03(false, true), q, dodajDni(d0, 5), &z).empty());
  }
  {  // omówione kończy się przy zmianie ustawień (R-LEK-09)
    reguly::Pamiec p;
    std::vector<Noc> h(3);
    h[0].data = d0; h[0].ustawienia = "ASVAuto/6/10/3/15";
    h[1].data = dodajDni(d0, 5); h[1].ustawienia = "ASVAuto/6/10/3/15";
    h[2].data = dodajDni(d0, 10); h[2].ustawienia = "ASVAuto/6/11/3/15";
    wzorce::Okolicznosci o;
    o.historia = &h;
    wzorce::aktualizuj(l03(true, false), p, d0, &z, o);
    wzorce::omowione(p, "L-03/ge10", d0);
    wzorce::aktualizuj(l03(true, false), p, dodajDni(d0, 5), &z, o);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::OMOWIONE);
    wzorce::aktualizuj(l03(true, false), p, dodajDni(d0, 10), &z, o);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::AKTYWNY);
  }
  {  // wygaszony B po 28 dniach (R-SKI-09); Δ przywraca (fail-path R-SKI-09)
    reguly::Pamiec p;
    wzorce::aktualizuj(l03(true, false, "11"), p, d0, &z);
    wzorce::aktualizuj(l03(true, false, "11"), p, dodajDni(d0, 27), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::AKTYWNY);
    wzorce::aktualizuj(l03(true, false, "11"), p, dodajDni(d0, 28), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::WYGASZONY_B);
    SPRAWDZ(!wzorce::omowione(p, "L-03/ge10", dodajDni(d0, 28)));  // wygaszony B: „Omówione” niedostępne
    wzorce::aktualizuj(l03(true, false, "14"), p, dodajDni(d0, 29), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::AKTYWNY);
  }
  {  // wzrost pilności przerywa „Omówione” (R-LEK-10): L-08 C → szybki B, jeden wzorzec L-08
    reguly::Pamiec p;
    std::vector<Wynik> c = {W("L-08", "", true, true, StanReguly::AKTYWNA, Pilnosc::C),
                            W("L-08", "szybki", true, false, StanReguly::AKTYWNA, Pilnosc::B),
                            W("L-08", ".wyjscie", true, false)};
    wzorce::aktualizuj(c, p, d0, &z);
    auto s = otwarte(c, p, d0, &z);
    SPRAWDZ(s.size() == 1 && s[0].klucz == "L-08" && s[0].karta == Karta::NA_KONTROLE &&
            s[0].pilnosc == Pilnosc::C && s[0].idTytulu.empty() && s[0].idPowodu == "L-08.linia");
    SPRAWDZ(!wzorce::wiem(p, "L-08", d0));  // C nie ma ekranu
    c[1].spelniona = true;
    wzorce::aktualizuj(c, p, dodajDni(d0, 1), &z);
    s = otwarte(c, p, dodajDni(d0, 1), &z);
    SPRAWDZ(s.size() == 1 && s[0].pilnosc == Pilnosc::B && s[0].karta == Karta::PELNY_EKRAN &&
            s[0].idTytulu == "L-08.tytul_szybki");
    SPRAWDZ(wzorce::omowione(p, "L-08", dodajDni(d0, 1)));
    std::vector<Wynik> a = {W("L-08", "", true, true, StanReguly::AKTYWNA, Pilnosc::A), W("L-08", ".wyjscie", true, false)};
    wzorce::aktualizuj(a, p, dodajDni(d0, 2), &z);
    SPRAWDZ(stanW(p, "L-08") == StanWzorca::AKTYWNY);
  }
  {  // L-09: ekran „problem” bez pilności lekarskiej, „wiem” na 7 dni
    reguly::Pamiec p;
    std::vector<Wynik> w = {W("L-09", "", true, true, StanReguly::AKTYWNA), W("L-09", ".wyjscie", true, false)};
    wzorce::aktualizuj(w, p, d0, &z);
    auto s = otwarte(w, p, d0, &z);
    SPRAWDZ(s.size() == 1 && s[0].karta == Karta::PROBLEM && s[0].pilnosc == Pilnosc::BRAK &&
            s[0].idTytulu == "L-09.tytul" && s[0].idPowodu == "L-09.powod");
    SPRAWDZ(wzorce::wiem(p, "L-09", d0));
    SPRAWDZ(!wzorce::omowione(p, "L-09", d0));
    wzorce::aktualizuj(w, p, dodajDni(d0, 6), &z);
    SPRAWDZ(stanW(p, "L-09") == StanWzorca::WIEM);
    wzorce::aktualizuj(w, p, dodajDni(d0, 7), &z);
    SPRAWDZ(stanW(p, "L-09") == StanWzorca::AKTYWNY);
  }
  {  // okres infekcji wstrzymuje L-03, nie L-08 (R-INF-02)
    reguly::Pamiec p;
    std::vector<Wynik> w = l03(true, false);
    w.push_back(W("L-08", "", true, true, StanReguly::AKTYWNA, Pilnosc::C));
    w.push_back(W("L-08", ".wyjscie", true, false));
    wzorce::Okolicznosci o;
    o.okresInfekcji = true;
    wzorce::aktualizuj(w, p, d0, &z);
    wzorce::aktualizuj(w, p, dodajDni(d0, 1), &z, o);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::WSTRZYMANY && stanW(p, "L-08") == StanWzorca::AKTYWNY);
    wzorce::aktualizuj(w, p, dodajDni(d0, 2), &z);
    SPRAWDZ(stanW(p, "L-03/ge10") == StanWzorca::AKTYWNY);
  }


  printf("karencja L-01 w CPAP i AutoSet (R-SKI-08) i powrót L-01 w 28 nocach (R-SKI-31)\n");
  {
    reguly::Pamiec p;
    auto w = [](bool wej, bool kar, bool ca10, bool wyj) {
      return std::vector<Wynik>{W("L-01", "cpap", true, wej, StanReguly::TYLKO_RAPORT),
                                W("L-01", "cpap.karencja", true, kar, StanReguly::TYLKO_RAPORT),
                                W("L-01", "cpap.karencja_ca", true, ca10, StanReguly::TYLKO_RAPORT),
                                W("L-01", ".wyjscie", true, wyj, StanReguly::TYLKO_RAPORT)};
    };
    wzorce::aktualizuj(w(true, true, false, false), p, d0, &z);  // noc 1 trwania
    SPRAWDZ(stanW(p, "L-01") == StanWzorca::KARENCJA);
    auto s = otwarte(w(true, true, false, false), p, d0, &z);
    SPRAWDZ(s.size() == 1 && s[0].stan == StanWzorca::KARENCJA);
    for (int i = 1; i <= 11; ++i) wzorce::aktualizuj(w(true, true, false, false), p, dodajDni(d0, i), &z);  // noc 12
    wzorce::aktualizuj(w(false, true, false, false), p, dodajDni(d0, 12), &z);  // noc bez warunku: licznik bez zmian
    wzorce::aktualizuj(w(true, true, false, false), p, dodajDni(d0, 13), &z);   // noc 13 trwania
    SPRAWDZ(stanW(p, "L-01") == StanWzorca::KARENCJA);
    wzorce::aktualizuj(w(true, true, false, false), p, dodajDni(d0, 14), &z);   // noc 14 trwania
    SPRAWDZ(stanW(p, "L-01") == StanWzorca::AKTYWNY);
    reguly::Pamiec q;  // średnia CA/h ≥ 10 kończy karencję od razu
    wzorce::aktualizuj(w(true, true, false, false), q, d0, &z);
    wzorce::aktualizuj(w(true, true, true, false), q, dodajDni(d0, 1), &z);
    SPRAWDZ(stanW(q, "L-01") == StanWzorca::AKTYWNY);
    reguly::Pamiec r;  // bez okna karencji: od razu aktywny
    wzorce::aktualizuj(w(true, false, false, false), r, d0, &z);
    SPRAWDZ(stanW(r, "L-01") == StanWzorca::AKTYWNY);
    reguly::Pamiec e;  // wyjście w karencji kasuje karencję
    wzorce::aktualizuj(w(true, true, false, false), e, d0, &z);
    wzorce::aktualizuj(w(false, true, false, true), e, dodajDni(d0, 3), &z);
    SPRAWDZ(stanW(e, "L-01") == StanWzorca::NIEAKTYWNY);
  }
  {  // R-SKI-31: L-01 (syntetycznie aktywne B z „Omówione”) wychodzi, wraca po 12 nocach → wyciszone dalej
    reguly::Pamiec p;
    std::vector<Wynik> wej = {W("L-01", "", true, true, StanReguly::AKTYWNA, Pilnosc::B), W("L-01", ".wyjscie", true, false)};
    std::vector<Wynik> wyj = {W("L-01", "", true, false, StanReguly::AKTYWNA, Pilnosc::B), W("L-01", ".wyjscie", true, true)};
    wzorce::aktualizuj(wej, p, d0, &z);
    SPRAWDZ(wzorce::omowione(p, "L-01", dodajDni(d0, 2)));
    wzorce::aktualizuj(wyj, p, dodajDni(d0, 17), &z);
    SPRAWDZ(stanW(p, "L-01") == StanWzorca::NIEAKTYWNY);
    wzorce::aktualizuj(wej, p, dodajDni(d0, 29), &z);  // 12 nocy po wyjściu
    SPRAWDZ(stanW(p, "L-01") == StanWzorca::OMOWIONE);
    ROWNE(sprawy::dataWyciszenia(p["L-01"].omowione), dodajDni(d0, 2));  // zapis `RRRR-MM-DD@P`
    ROWNE(wzorce::odczytajZapis(p["L-01"].stanWzorca).wejscie, d0);
    wzorce::aktualizuj(wyj, p, dodajDni(d0, 30), &z);
    wzorce::aktualizuj(wej, p, dodajDni(d0, 60), &z);  // 30 nocy po wyjściu → nowy wzorzec
    SPRAWDZ(stanW(p, "L-01") == StanWzorca::AKTYWNY && wzorce::info(wej, p, "L-01", dodajDni(d0, 60), &z).omowioneOd.empty());
    ROWNE(wzorce::odczytajZapis(p["L-01"].stanWzorca).wejscie, dodajDni(d0, 60));
    // inny wzorzec (L-03) po wyjściu i powrocie w 28 nocach to nowy wzorzec
    reguly::Pamiec q;
    wzorce::aktualizuj(l03(true, false), q, d0, &z);
    wzorce::omowione(q, "L-03/ge10", d0);
    wzorce::aktualizuj(l03(false, true), q, dodajDni(d0, 3), &z);
    wzorce::aktualizuj(l03(true, false), q, dodajDni(d0, 5), &z);
    SPRAWDZ(stanW(q, "L-03/ge10") == StanWzorca::AKTYWNY);  // nowy wzorzec: „Omówione” sprzed wyjścia nie działa
  }

  printf("licznik potwierdzeń „Omówione” przy pilności A (R-LEK-24…27, przykład R-LEK-27 na syntetycznym wzorcu A)\n");
  {
    using wzorce::WariantOmowienia;
    reguly::Pamiec p;
    auto a = [](const std::string& x, Pilnosc pil = Pilnosc::A) {
      return std::vector<Wynik>{W("L-06", "", true, true, StanReguly::AKTYWNA, pil, x), W("L-06", ".wyjscie", true, false)};
    };
    const std::string t0 = "2026-11-10";
    wzorce::aktualizuj(a("35"), p, t0, &z);
    SPRAWDZ(wzorce::wariantOmowienia(p, "L-06", t0, &z) == WariantOmowienia::A_PIERWSZE);
    SPRAWDZ(wzorce::omowione(p, "L-06", t0, &z));
    auto s = otwarte(a("35"), p, t0, &z);
    SPRAWDZ(s.size() == 1 && s[0].potwierdzenia == 1 && s[0].stan == StanWzorca::OMOWIONE);
    // 56 nocy później pełny ekran wraca; T88 33 (bez poprawy o Δ = 10 min) i bez raportu → najpierw raport
    const std::string t1 = dodajDni(t0, 57);
    wzorce::aktualizuj(a("33"), p, t1, &z);
    SPRAWDZ(stanW(p, "L-06") == StanWzorca::AKTYWNY);
    SPRAWDZ(wzorce::wariantOmowienia(p, "L-06", t1, &z) == WariantOmowienia::NAJPIERW_RAPORT);
    SPRAWDZ(!wzorce::omowione(p, "L-06", t1, &z));
    wzorce::raportZEkranuL(p, "L-06", t1);
    SPRAWDZ(wzorce::wariantOmowienia(p, "L-06", t1, &z) == WariantOmowienia::A_DRUGIE_BEZ_POPRAWY);
    SPRAWDZ(wzorce::omowione(p, "L-06", t1, &z));
    wzorce::aktualizuj(a("33"), p, dodajDni(t1, 27), &z);
    SPRAWDZ(stanW(p, "L-06") == StanWzorca::OMOWIONE);
    const std::string t2 = dodajDni(t1, 28);  // drugie bez poprawy: 28 nocy (R-LEK-25)
    wzorce::aktualizuj(a("34"), p, t2, &z);
    SPRAWDZ(stanW(p, "L-06") == StanWzorca::AKTYWNY);
    wzorce::raportZEkranuL(p, "L-06", t2);
    SPRAWDZ(wzorce::wariantOmowienia(p, "L-06", t2, &z) == WariantOmowienia::A_KOLEJNE_BEZ_POPRAWY);
    SPRAWDZ(wzorce::omowione(p, "L-06", t2, &z));
    // trzecie bez poprawy: wyciszenie trwa, pełny ekran co 14 nocy (R-LEK-27)
    wzorce::aktualizuj(a("34"), p, dodajDni(t2, 13), &z);
    SPRAWDZ(!otwarte(a("34"), p, dodajDni(t2, 13), &z)[0].pelnyEkranDzis);
    wzorce::aktualizuj(a("34"), p, dodajDni(t2, 14), &z);
    SPRAWDZ(stanW(p, "L-06") == StanWzorca::OMOWIONE);
    SPRAWDZ(otwarte(a("34"), p, dodajDni(t2, 14), &z)[0].pelnyEkranDzis);
    wzorce::aktualizuj(a("34"), p, dodajDni(t2, 100), &z);
    SPRAWDZ(stanW(p, "L-06") == StanWzorca::OMOWIONE);  // bez końca po 56 nocach
    SPRAWDZ(otwarte(a("34"), p, dodajDni(t2, 98), &z)[0].pelnyEkranDzis);
    // czwarte w dniu pełnego ekranu: bez nowego raportu → najpierw raport; z poprawą (T88 20) → jak pierwsze
    const std::string t3 = dodajDni(t2, 112);
    wzorce::aktualizuj(a("34"), p, t3, &z);
    SPRAWDZ(wzorce::wariantOmowienia(p, "L-06", t3, &z) == WariantOmowienia::NAJPIERW_RAPORT);
    wzorce::aktualizuj(a("20"), p, t3, &z);
    SPRAWDZ(wzorce::wariantOmowienia(p, "L-06", t3, &z) == WariantOmowienia::A_PIERWSZE);
    SPRAWDZ(wzorce::omowione(p, "L-06", t3, &z));
    SPRAWDZ(otwarte(a("20"), p, t3, &z)[0].potwierdzenia == 1);
    // spadek pilności poniżej A kasuje licznik i wyciszenie (f, R-LEK-26)
    wzorce::aktualizuj(a("20", Pilnosc::B), p, dodajDni(t3, 1), &z);
    SPRAWDZ(stanW(p, "L-06") == StanWzorca::AKTYWNY);
    SPRAWDZ(otwarte(a("20", Pilnosc::B), p, dodajDni(t3, 1), &z)[0].potwierdzenia == 0);
    SPRAWDZ(wzorce::wariantOmowienia(p, "L-06", dodajDni(t3, 1), &z) == WariantOmowienia::ZWYKLE);
  }
  {  // Δ powrotu L-09 = Δ wycieku P95 z 4.5 (+10 L/min): „wiem” przy 25, 34,9 → dalej wiem, 35 → aktywny
    reguly::Pamiec p;
    auto w9 = [](const std::string& x) {
      return std::vector<Wynik>{W("L-09", "", true, true, StanReguly::AKTYWNA, Pilnosc::BRAK, x), W("L-09", ".wyjscie", true, false)};
    };
    wzorce::aktualizuj(w9("25"), p, d0, &z);
    SPRAWDZ(wzorce::wiem(p, "L-09", d0));
    wzorce::aktualizuj(w9("34,9"), p, dodajDni(d0, 1), &z);
    SPRAWDZ(stanW(p, "L-09") == StanWzorca::WIEM);
    wzorce::aktualizuj(w9("35"), p, dodajDni(d0, 2), &z);
    SPRAWDZ(stanW(p, "L-09") == StanWzorca::AKTYWNY);
  }
  {  // nowe wejście po wyjściu zeruje licznik potwierdzeń A i raport (fail-path R-SKI-03)
    reguly::Pamiec p;
    std::vector<Wynik> a = {W("L-06", "", true, true, StanReguly::AKTYWNA, Pilnosc::A, "35"), W("L-06", ".wyjscie", true, false)};
    std::vector<Wynik> e = {W("L-06", "", true, false, StanReguly::AKTYWNA, Pilnosc::A), W("L-06", ".wyjscie", true, true)};
    wzorce::aktualizuj(a, p, d0, &z);
    wzorce::raportZEkranuL(p, "L-06", d0);
    SPRAWDZ(wzorce::omowione(p, "L-06", d0, &z));
    wzorce::aktualizuj(e, p, dodajDni(d0, 3), &z);
    wzorce::aktualizuj(a, p, dodajDni(d0, 9), &z);
    const auto s = wzorce::odczytajZapis(p["L-06"].stanWzorca);
    SPRAWDZ(s.stan == StanWzorca::AKTYWNY && s.potw == 0 && std::isnan(s.refPotw) && s.raport.empty());
    SPRAWDZ(wzorce::wariantOmowienia(p, "L-06", dodajDni(d0, 9), &z) == wzorce::WariantOmowienia::A_PIERWSZE);
  }
  printf("hipoteza (5.12.9) i kolejność „Otwartych spraw”\n");
  {
    std::vector<Wynik> w = {W("L-11", "", true, true, StanReguly::AKTYWNA, Pilnosc::B), W("L-11", ".wyjscie", true, false),
                            W("H-09", "L-11", true, true), W("H-08", "L-11", true, true)};
    ROWNE(wzorce::hipoteza("L-11", w), "H-09");
    w[2].spelniona = false;
    ROWNE(wzorce::hipoteza("L-11", w), "H-08");
    w[2].oceniona = false;  // H-09 „nie oceniam” → zapasowa H-08
    ROWNE(wzorce::hipoteza("L-11", w), "H-08");
    w[0].spelniona = false;
    ROWNE(wzorce::hipoteza("L-11", w), "");
    // odmiana trybu „~” (H-02 przy L-04 AutoSet) i wiersz pomocniczy „.” nie jest hipotezą
    std::vector<Wynik> a = {W("L-04", "autoset", true, true, StanReguly::AKTYWNA, Pilnosc::B),
                            W("H-02", "L-04.max15", true, true), W("H-02", "L-04~tryb", true, false),
                            W("H-02", "L-04~autoset", true, true)};
    ROWNE(wzorce::hipoteza("L-04", a), "H-02");
    a[3].spelniona = false;
    ROWNE(wzorce::hipoteza("L-04", a), "");
  }
  {
    reguly::Pamiec p;
    std::vector<Wynik> w = {W("L-01", "", true, true, StanReguly::AKTYWNA, Pilnosc::A), W("L-01", ".wyjscie", true, false),
                            W("L-04", "", true, true, StanReguly::TYLKO_RAPORT), W("L-04", ".wyjscie", true, false, StanReguly::TYLKO_RAPORT),
                            W("L-08", "", true, true, StanReguly::AKTYWNA, Pilnosc::C), W("L-08", ".wyjscie", true, false),
                            W("L-09", "", true, true), W("L-09", ".wyjscie", true, false)};
    auto l = l03(true, false);
    w.insert(w.end(), l.begin(), l.end());
    wzorce::aktualizuj(w, p, d0, &z);
    auto s = otwarte(w, p, d0, &z);
    SPRAWDZ(s.size() == 5);
    if (s.size() == 5) {
      ROWNE(s[0].klucz, "L-01");
      ROWNE(s[1].klucz, "L-03/ge10");
      ROWNE(s[2].klucz, "L-09");
      ROWNE(s[3].klucz, "L-08");
      ROWNE(s[4].klucz, "L-04");
    }
  }
  {  // zapis stanu: bez średnika, odczyt zwrotny, śmieci = nieaktywny
    wzorce::ZapisWzorca a;
    a.stan = StanWzorca::OMOWIONE;
    a.pil = 'B';
    a.wejscie = "2026-10-01";
    a.odPilnosci = "2026-10-02";
    a.ref = 10.5;
    a.potw = 2;
    a.refPotw = 33;
    a.raport = "2026-10-03";
    std::string t = wzorce::zapiszZapis(a);
    SPRAWDZ(t.find(';') == std::string::npos);
    auto b = wzorce::odczytajZapis(t);
    SPRAWDZ(b.stan == a.stan && b.pil == 'B' && b.wejscie == a.wejscie && b.odPilnosci == a.odPilnosci &&
            std::fabs(b.ref - 10.5) < 1e-9 && b.potw == 2 && std::fabs(b.refPotw - 33) < 1e-9 && b.raport == a.raport);
    SPRAWDZ(wzorce::odczytajZapis("bzdura").stan == StanWzorca::NIEAKTYWNY);
    SPRAWDZ(wzorce::odczytajZapis("").stan == StanWzorca::NIEAKTYWNY);
    SPRAWDZ(wzorce::odczytajZapis("aktywny,X,,,,,0,,,0,,").stan == StanWzorca::NIEAKTYWNY);
  }
}

namespace reguly {
void zarejestrujRodzajeE();  // src/rodzaje_e.cpp
}
// ================= część 2: wzorce z zestawu wbudowanego na syntetycznej historii =================
static const char* UST = "ASVAuto/6/10/3/15";
static Noc noc(const std::string& data, int m01, float ahi) {
  Noc n;
  n.data = data;
  n.m01 = m01;
  n.wazna = m01 >= 240;
  n.ahi = ahi;
  n.wmed = 5;
  n.wp95 = 12;
  n.emed = 7;
  n.ep95 = 8;
  n.oa = 1;
  n.ca = 1;
  n.h = 1;
  n.csr = 0;
  n.ustawienia = UST;
  n.tryb = "ASVAuto";
  n.m08 = 0;  // bez dużego wycieku (E-20, E-22)
  n.eai = 6;
  n.eax = 10;
  n.ans = 3;
  n.axs = 15;
  return n;
}
static std::vector<Noc> seria(const std::string& od, int ile, float ahi) {
  std::vector<Noc> h;
  for (int i = 0; i < ile; ++i) h.push_back(noc(dodajDni(od, i), 420, ahi));
  return h;
}
// Wyniki w kolejności katalogu (jak ocen()), potrzebne hipoteza() i aktualizuj().
static std::vector<Wynik> ocenWek(const std::vector<Noc>& h, const reguly::Odpowiedzi* odp = nullptr,
                                  const reguly::Pamiec* pam = nullptr, reguly::Tryb tryb = reguly::Tryb::ASVAUTO);
static std::map<std::string, Wynik> ocenDzis(const std::vector<Noc>& h, const reguly::Odpowiedzi* odp = nullptr,
                                             const reguly::Pamiec* pam = nullptr,
                                             reguly::Tryb tryb = reguly::Tryb::ASVAUTO) {
  std::map<std::string, Wynik> r;
  for (const auto& w : ocenWek(h, odp, pam, tryb)) r[w.id + "/" + w.wariant] = w;
  return r;
}
static std::string g_poczatekLeczenia;  // R-SKI-08: ustawienie „Początek leczenia” w ocenWek()
static std::vector<Wynik> ocenWek(const std::vector<Noc>& h, const reguly::Odpowiedzi* odp,
                                  const reguly::Pamiec* pam, reguly::Tryb tryb) {
  const std::string dzis = h.back().data;
  const Stany s = stanyZPamieci(h, dzis, {});
  reguly::Kontekst k;
  k.historia = &h;
  k.dzis = dzis;
  k.norma = &s.norma;
  k.stany = &s;
  k.trybBiezacy = tryb;
  static reguly::Odpowiedzi pusta;
  k.odpowiedzi = odp ? odp : &pusta;
  static reguly::Pamiec pustaP;
  k.pamiec = pam ? pam : &pustaP;
  k.zestaw = &reguly::zestawWbudowany();
  k.poczatekLeczenia = g_poczatekLeczenia;
  std::vector<Wynik> wyn = reguly::ocen(reguly::zestawWbudowany(), k);
  // każda zmienna {…} tekstów spełnionej reguły L albo H jest wypełniona (bez {prog:…})
  const auto& z = reguly::zestawWbudowany();
  for (const auto& w : wyn) {
    if (!w.oceniona || !w.spelniona || (w.id.compare(0, 2, "L-") != 0 && w.id.compare(0, 2, "H-") != 0)) continue;
    for (const auto& wk : z.katalog) {
      if (wk.id != w.id || wk.wariant != w.wariant) continue;
      for (const auto& idt : wk.teksty) {
        auto t = z.teksty.find(idt);
        if (t == z.teksty.end()) continue;
        const std::string& s = t->second.tekst;
        for (size_t a = s.find('{'); a != std::string::npos; a = s.find('{', a + 1)) {
          size_t b = s.find('}', a);
          std::string zm = s.substr(a + 1, b - a - 1);
          if (zm.compare(0, 5, "prog:") == 0) continue;
          if (w.zmienne.count(zm)) ok++;
          else { zle++; printf("BŁĄD: %s/%s tekst %s bez zmiennej {%s}\n", w.id.c_str(), w.wariant.c_str(), idt.c_str(), zm.c_str()); }
        }
      }
    }
  }
  return wyn;
}
static bool spel(const std::map<std::string, Wynik>& r, const std::string& k) {
  auto it = r.find(k);
  if (std::getenv("WZORCE_DIAG")) {
    if (it == r.end()) printf("  diag %s: brak wyniku\n", k.c_str());
    else printf("  diag %s: oceniona=%d spelniona=%d %s\n", k.c_str(), it->second.oceniona, it->second.spelniona,
                it->second.powodNieoceny.c_str());
  }
  return it != r.end() && it->second.oceniona && it->second.spelniona;
}
static bool oceniona(const std::map<std::string, Wynik>& r, const std::string& k) {
  auto it = r.find(k);
  return it != r.end() && it->second.oceniona;
}

static void testSilnik() {
  printf("zestaw wbudowany: L z katalogu na syntetycznej historii (część 2)\n");
  const std::string d0 = "2026-08-01";
  const auto& z = reguly::zestawWbudowany();
  bool jestL = false;
  for (const auto& w : z.katalog) jestL = jestL || w.id == "L-03";
  SPRAWDZ(jestL);
  {  // stałe firmware równe progom zestawu wbudowanego (ADR-0034): noc niepewna i L-09 z tej samej liczby,
     // E-30 w stanach jak w silniku, R-SKI-24 i kryterium braku ocen z wycieku jak w progi.csv
    auto wart = [&](const char* id) {
      const reguly::Prog* p = z.prog(id, reguly::Tryb::ASVAUTO);
      return p && p->tekst.empty() ? p->wartosc : NAN;
    };
    auto tekst = [&](const char* id) {
      const reguly::Prog* p = z.prog(id, reguly::Tryb::ASVAUTO);
      return p ? p->tekst : std::string();
    };
    SPRAWDZ(std::fabs(wart("R-NOR-05.niepewna_p95") - NIEPEWNA_P95) < 1e-6);
    SPRAWDZ(std::fabs(wart("E-30.ahi") - E30_AHI) < 1e-6 && std::fabs(wart("E-30.okno") - E30_OKNO) < 1e-6);
    SPRAWDZ(tekst("R-SKI-24.noce_nieczytelne") == std::to_string(SKI24_NOCE) + "/" + std::to_string(SKI24_OKNO));
    SPRAWDZ(std::fabs(wart("R-SKI-27.noc_z_terapia_m01") - NOC_Z_TERAPIA_MIN) < 1e-6);
    SPRAWDZ(std::fabs(wart("L-03.okno") - WYCIEK_OKNO) < 1e-6 && std::fabs(wart("L-03.min_noce") - WYCIEK_MIN_NOCE) < 1e-6);
  }

  {  // L-03 (≥ 10), T-TEST-163 i 183
    auto h = seria(d0, 14, 10.0f);
    SPRAWDZ(spel(ocenDzis(h), "L-03/ge10"));
    auto r = ocenDzis(h);
    SPRAWDZ(r.count("L-03/ge10") && r["L-03/ge10"].stan == StanReguly::AKTYWNA && r["L-03/ge10"].pilnosc == Pilnosc::B);
    auto h2 = seria(d0, 14, 9.9f);
    SPRAWDZ(!spel(ocenDzis(h2), "L-03/ge10"));
    auto h3 = seria(d0, 14, 10.0f);  // 10 nocy ważnych dla L na progu, 9 → brak
    for (int i = 0; i < 4; ++i) h3[static_cast<size_t>(i)].m01 = 200, h3[static_cast<size_t>(i)].wazna = false;
    SPRAWDZ(spel(ocenDzis(h3), "L-03/ge10"));
    h3[4].m01 = 200;
    h3[4].wazna = false;
    SPRAWDZ(!spel(ocenDzis(h3), "L-03/ge10"));
    auto h4 = seria(d0, 14, 12.0f);  // noc niepewna (mediana wycieku > 30 L/min) się nie liczy
    for (int i = 0; i < 5; ++i) h4[static_cast<size_t>(i)].wmed = 35;
    SPRAWDZ(!spel(ocenDzis(h4), "L-03/ge10"));
    auto h5 = seria(d0, 5, 15.0f);  // za mało nocy → nie oceniam, nigdy „w porządku”
    auto r5 = ocenDzis(h5);
    SPRAWDZ(!oceniona(r5, "L-03/ge10.mediana") || !spel(r5, "L-03/ge10"));
    auto e = seria(d0, 14, 6.9f);
    SPRAWDZ(spel(ocenDzis(e), "L-03/ge10.wyjscie"));
    auto e2 = seria(d0, 14, 7.0f);
    SPRAWDZ(!spel(ocenDzis(e2), "L-03/ge10.wyjscie"));
  }
  {  // L-03 (5–10) tylko raport
    auto h = seria(d0, 28, 5.0f);
    auto r = ocenDzis(h);
    SPRAWDZ(spel(r, "L-03/5_10") && r["L-03/5_10"].stan == StanReguly::TYLKO_RAPORT && r["L-03/5_10"].pilnosc == Pilnosc::BRAK);
    SPRAWDZ(!spel(ocenDzis(seria(d0, 28, 4.9f)), "L-03/5_10"));
    SPRAWDZ(spel(ocenDzis(seria(d0, 28, 4.9f)), "L-03/5_10.wyjscie"));
  }
  {  // L-01 tylko raport, T-TEST-161 (a) i 181 (a)
    auto h = seria(d0, 7, 6.0f);
    for (int i = 0; i < 7; ++i) h[static_cast<size_t>(i)].ca = i < 3 ? 5.0f : 5.0f;
    auto r = ocenDzis(h);
    SPRAWDZ(spel(r, "L-01/") && r["L-01/"].stan == StanReguly::TYLKO_RAPORT && r["L-01/"].pilnosc == Pilnosc::BRAK);
    for (auto& n : h) n.ca = 4.9f;
    SPRAWDZ(!spel(ocenDzis(h), "L-01/"));
    for (auto& n : h) n.ca = 2.9f;
    SPRAWDZ(spel(ocenDzis(h), "L-01/.wyjscie_srednia"));
    for (auto& n : h) n.ca = 3.0f;
    SPRAWDZ(!spel(ocenDzis(h), "L-01/.wyjscie_srednia"));
  }
  {  // L-02 (a) tylko raport: CSR ≥ 20 min w ≥ 3 z 14
    auto h = seria(d0, 14, 2.0f);
    for (int i = 0; i < 3; ++i) h[static_cast<size_t>(10 + i)].csr = 20;
    SPRAWDZ(spel(ocenDzis(h), "L-02/a"));
    h[12].csr = 19;
    SPRAWDZ(!spel(ocenDzis(h), "L-02/a"));
  }
  {  // L-02 (b): bez normy CSR% w rekordzie nocy → nie oceniam
    auto r = ocenDzis(seria(d0, 40, 2.0f));
    SPRAWDZ(!spel(r, "L-02/b"));
  }
  {  // L-04 ASVAuto, T-TEST-164 i 184 (EAX = 10)
    auto h = seria(d0, 7, 6.0f);
    for (size_t i = 0; i < 7; ++i) { h[i].ep95 = i < 5 ? 9.5f : 8.0f; h[i].oa = 5.0f; }
    SPRAWDZ(spel(ocenDzis(h), "L-04/"));
    h[4].ep95 = 9.4f;
    SPRAWDZ(!spel(ocenDzis(h), "L-04/"));
    h[4].ep95 = 9.5f;
    for (auto& n : h) n.oa = 4.9f;
    SPRAWDZ(!spel(ocenDzis(h), "L-04/"));
    for (auto& n : h) n.oa = NAN;  // noce z importu bez rodzaju bezdechu nie liczą się
    SPRAWDZ(!spel(ocenDzis(h), "L-04/"));
    auto e = seria(d0, 7, 6.0f);
    for (size_t i = 0; i < 7; ++i) { e[i].ep95 = i < 2 ? 9.5f : 8.0f; e[i].oa = 5.0f; }
    SPRAWDZ(spel(ocenDzis(e), "L-04/.wyjscie"));
    e[2].ep95 = 9.5f;
    SPRAWDZ(!spel(ocenDzis(e), "L-04/.wyjscie"));
  }
  {  // L-05: rekord nocy bez normy M-13 → nie oceniam (R-SKI-44)
    auto r = ocenDzis(seria(d0, 40, 2.0f));
    SPRAWDZ(!spel(r, "L-05/"));
  }
  {  // L-06 zablokowane: nigdy oceniona
    auto r = ocenDzis(seria(d0, 14, 2.0f));
    SPRAWDZ(!oceniona(r, "L-06/"));
  }
  {  // L-08 szybki (B), T-TEST-168: 27 z 30 nocy ≥ 4 h miesiąc temu, dziś 14 z 30 → wejście; 15 z 30 → brak
    std::vector<Noc> h;
    for (int i = 0; i < 60; ++i) {
      bool dobra = i < 30 ? (i % 10 != 0) : (i - 30 < 14);
      h.push_back(noc(dodajDni(d0, i), dobra ? 420 : 100, 2.0f));
    }
    SPRAWDZ(spel(ocenDzis(h), "L-08/szybki"));
    h[44].m01 = 420;
    SPRAWDZ(!spel(ocenDzis(h), "L-08/szybki"));
  }
  {  // L-09: M-07 P95 ≥ próg nocy niepewnej (R-NOR-05.niepewna_p95) w ≥ 10 z 14 nocy z terapią, także niepewnych
     // (T-TEST-169); duży wyciek ponad 24 L/min liczy się dalej, bo jest powyżej progu nocy niepewnej
    auto h = seria(d0, 14, 2.0f);
    for (size_t i = 0; i < 10; ++i) { h[i].wp95 = 25; h[i].wmed = 35; }
    SPRAWDZ(spel(ocenDzis(h), "L-09/"));
    h[0].wp95 = 24;  // 24 L/min: do 08.11.2026 poza warunkiem (> 24), teraz w paśmie od progu nocy niepewnej
    SPRAWDZ(spel(ocenDzis(h), "L-09/"));
    h[0].wp95 = 14.9f;  // poniżej progu nocy niepewnej: 9 z 14
    SPRAWDZ(!spel(ocenDzis(h), "L-09/"));
  }
  {  // L-09 w paśmie wycieku 15–24 L/min (siatka dla nocy niepewnych bez dużego wycieku E-20)
    auto pasmo = [&](float p95, int ile) {
      auto h = seria(d0, 14, 12.0f);
      for (size_t i = 0; i < h.size(); ++i) h[i].wp95 = i < static_cast<size_t>(ile) ? p95 : 9.0f;
      return h;
    };
    auto a = ocenDzis(pasmo(18.0f, 10));  // (a) 10 z 14 nocy po 18 L/min → L-09
    SPRAWDZ(spel(a, "L-09/"));
    SPRAWDZ(a.count("L-09/.sk10") && a["L-09/.sk10"].zmienne["n"] == "10");
    SPRAWDZ(!spel(ocenDzis(pasmo(12.0f, 10)), "L-09/"));  // (b) 10 z 14 po 12 L/min (pasmo „widoczny”) → nie
    SPRAWDZ(!spel(ocenDzis(pasmo(18.0f, 9)), "L-09/"));   // (c) 9 z 14 po 18 L/min → nie
    SPRAWDZ(spel(ocenDzis(pasmo(15.0f, 10)), "L-09/"));   // dokładnie na progu (≥)
    auto e = ocenDzis(pasmo(18.0f, 14));  // 14 nocy niepewnych: L-03 nie ocenia, L-09 kieruje do dostawcy
    SPRAWDZ(spel(e, "L-09/") && !spel(e, "L-03/ge10"));
  }
  {  // L-11 (T-TEST-171, 191) i wariant „2 tygodnie”
    std::vector<Noc> h;
    for (int i = 0; i < 30; ++i) h.push_back(noc(dodajDni(d0, i), i < 21 ? 420 : 100, 4.9f));
    reguly::Odpowiedzi o;
    const std::string dz = h.back().data;
    o.sennosc[dodajDni(dz, -21)] = true;
    o.sennosc[dodajDni(dz, -14)] = false;
    o.sennosc[dodajDni(dz, -7)] = true;
    o.sennosc[dz] = true;
    SPRAWDZ(spel(ocenDzis(h, &o), "L-11/"));
    for (auto& n : h) n.ahi = 5.0f;
    SPRAWDZ(!spel(ocenDzis(h, &o), "L-11/"));
    reguly::Odpowiedzi o2;
    o2.sennosc[dodajDni(dz, -14)] = true;
    o2.sennosc[dz] = true;  // tydzień -7 bez odpowiedzi: pominięty (R-SKI-17)
    SPRAWDZ(spel(ocenDzis(h, &o2), "L-11/2tyg"));
    reguly::Odpowiedzi o3;
    o3.sennosc[dodajDni(dz, -14)] = true;
    o3.sennosc[dodajDni(dz, -7)] = false;
    o3.sennosc[dz] = true;
    SPRAWDZ(!spel(ocenDzis(h, &o3), "L-11/2tyg"));
    SPRAWDZ(!spel(ocenDzis(h, &o3), "L-11/.wyjscie"));  // 2 „tak” z 4 → trwa
    reguly::Odpowiedzi o4;  // 3 tygodnie z odpowiedzią, „tak” w 1 → wyjście
    o4.sennosc[dz] = true;
    o4.sennosc[dodajDni(dz, -7)] = false;
    o4.sennosc[dodajDni(dz, -14)] = false;
    SPRAWDZ(spel(ocenDzis(h, &o4), "L-11/.wyjscie"));
    reguly::Odpowiedzi o5;  // tylko 2 tygodnie z odpowiedzią: „brak odpowiedzi” to nie „nie” → nie oceniam
    o5.sennosc[dz] = true;
    o5.sennosc[dodajDni(dz, -7)] = false;
    SPRAWDZ(!oceniona(ocenDzis(h, &o5), "L-11/.wyjscie"));
  }
  {  // L-12 tylko raport (T-TEST-172): AHI +3 / h po zmianie ustawień
    auto h = seria(d0, 28, 2.0f);
    for (size_t i = 14; i < 28; ++i) { h[i].ustawienia = "ASVAuto/6/11/3/15"; h[i].ahi = 5.0f; }
    SPRAWDZ(spel(ocenDzis(h), "L-12/"));
    for (size_t i = 14; i < 28; ++i) h[i].ahi = 4.9f;
    SPRAWDZ(!spel(ocenDzis(h), "L-12/"));
    auto h2 = seria(d0, 27, 2.0f);  // 13 nocy po zmianie → brak
    for (size_t i = 14; i < 27; ++i) { h2[i].ustawienia = "ASVAuto/6/11/3/15"; h2[i].ahi = 6.0f; }
    SPRAWDZ(!spel(ocenDzis(h2), "L-12/"));
  }
  {  // H-09 (R-HIP-09): mediana M-01 z 28 nocy z terapią < 6 h; próg w godzinach, M-01 w minutach
    std::vector<Noc> h;
    for (int i = 0; i < 30; ++i) h.push_back(noc(dodajDni(d0, i), 310, 2.0f));  // 5 h 10
    reguly::Odpowiedzi o;  // L-11 spełnione (H-09 `wymaga` L-11): senność tak w 3 z 4 tygodni
    const std::string dz = h.back().data;
    for (int t = 0; t < 3; ++t) o.sennosc[dodajDni(dz, -7 * t)] = true;
    SPRAWDZ(spel(ocenDzis(h, &o), "L-11/"));
    SPRAWDZ(spel(ocenDzis(h, &o), "H-09/L-11"));
    ROWNE(wzorce::hipoteza("L-11", ocenWek(h, &o)), "H-09");
    for (auto& n : h) n.m01 = 400;  // 6 h 40
    SPRAWDZ(!spel(ocenDzis(h, &o), "H-09/L-11"));
    ROWNE(wzorce::hipoteza("L-11", ocenWek(h, &o)), "H-08");
  }
  {  // L-12: czas z maską krótszy o 60 min po zmianie → wejście; o 59 min → brak (T-TEST-172)
    auto h = seria(d0, 28, 2.0f);
    for (size_t i = 14; i < 28; ++i) { h[i].ustawienia = "ASVAuto/6/11/3/15"; h[i].m01 = 360; }
    SPRAWDZ(spel(ocenDzis(h), "L-12/.m01"));
    for (size_t i = 14; i < 28; ++i) h[i].m01 = 361;
    SPRAWDZ(!spel(ocenDzis(h), "L-12/.m01"));
  }
  {  // L-07 (T-TEST-167, ASVAuto, M-06p): norma 9,0; mediana z 7 nocy 11,0 przez 14 nocy → wejście; 13 nocy → brak
    std::vector<Noc> h = seria(d0, 40, 2.0f);
    for (auto& n : h) n.ep95 = 9.0f;
    reguly::Pamiec pam;
    reguly::Odpowiedzi o;
    std::map<std::string, Wynik> r;
    for (int d = 0; d < 20; ++d) {
      h.push_back(noc(dodajDni(d0, 40 + d), 420, 2.0f));
      h.back().ep95 = 11.0f;
      r = ocenDzis(h, &o, &pam);
      std::vector<Wynik> w;
      for (auto& kv : r) w.push_back(kv.second);
      reguly::aktualizujPamiec(pam, w, h.back().data);
      // mediana z 7 nocy ≥ 11 od 4. nocy wzrostu (d = 3): 13 nocy trwania w d = 15, 14 nocy w d = 16
      if (d == 15) SPRAWDZ(!spel(r, "L-07/"));
      if (d == 16) SPRAWDZ(spel(r, "L-07/"));
    }
    SPRAWDZ(spel(r, "L-07/"));
  }
  {  // L-08 (T-TEST-168): E-12 aktywne ≥ 28 dni przy E-22 nieaktywnym → L-08 (C), przez przebieg z pamięcią
    std::vector<Noc> h = seria(d0, 30, 2.0f);
    reguly::Pamiec pam;
    reguly::Odpowiedzi o;
    std::map<std::string, Wynik> r;
    bool e12 = false;
    for (int d = 0; d < 40; ++d) {
      h.push_back(noc(dodajDni(d0, 30 + d), 100, 2.0f));  // noce krótsze niż 4 h: regularność spada
      r = ocenDzis(h, &o, &pam);
      e12 = e12 || spel(r, "E-12/");
      std::vector<Wynik> w;
      for (auto& kv : r) w.push_back(kv.second);
      reguly::aktualizujPamiec(pam, w, h.back().data);
    }
    SPRAWDZ(e12);
    SPRAWDZ(spel(r, "L-08/"));
  }
  {  // L-09 w paśmie 15–24 L/min z pamięcią: stały wyciek 18 L/min nie daje wyjścia (bez E-20), więc L-09 nie miga;
     // wyjście dopiero po 7 kolejnych nocach z P95 poniżej progu nocy niepewnej (ADR-0034)
    std::vector<Noc> h = seria(d0, 7, 12.0f);
    reguly::Pamiec pam;
    reguly::Odpowiedzi o;
    std::map<std::string, Wynik> r;
    int dzienWejscia = -1, wyjsc = 0;
    for (int d = 0; d < 40; ++d) {
      h.push_back(noc(dodajDni(d0, 7 + d), 420, 12.0f));
      h.back().wp95 = d < 30 ? 18.0f : 9.0f;
      r = ocenDzis(h, &o, &pam);
      if (dzienWejscia < 0 && spel(r, "L-09/")) dzienWejscia = d;
      if (d < 30 && spel(r, "L-09/.wyjscie")) wyjsc++;
      if (d == 35) SPRAWDZ(!spel(r, "L-09/.wyjscie"));  // 6 nocy po 9 L/min
      if (d == 36) SPRAWDZ(spel(r, "L-09/.wyjscie"));   // 7. noc
      std::vector<Wynik> w;
      for (auto& kv : r) w.push_back(kv.second);
      reguly::aktualizujPamiec(pam, w, h.back().data);
    }
    SPRAWDZ(dzienWejscia == 9);  // 10. noc z P95 18 w 14
    SPRAWDZ(wyjsc == 0);
  }
  {  // L-09, noce bez P95 (ADR-0034): okno = 14 ostatnich nocy z terapią, noc bez P95 = „nie
     // spełnia”; wyjście po 7 nocach z terapią bez E-20, z których żadna nie ma P95 ≥ 15 (brak P95 nie przerywa serii)
    std::vector<Noc> h = seria(d0, 7, 12.0f);
    reguly::Pamiec pam;
    reguly::Odpowiedzi o;
    std::map<std::string, Wynik> r;
    int dzienWyjscia = -1;
    for (int d = 0; d < 50; ++d) {
      h.push_back(noc(dodajDni(d0, 7 + d), 420, 12.0f));
      h.back().wp95 = d < 20 ? 18.0f : NAN;  // od 21. nocy aparat (albo import) bez P95
      r = ocenDzis(h, &o, &pam);
      if (d == 19) SPRAWDZ(spel(r, "L-09/"));
      if (d >= 20 && dzienWyjscia < 0 && spel(r, "L-09/.wyjscie")) dzienWyjscia = d;
      if (d == 23) SPRAWDZ(spel(r, "L-09/.sk10"));    // 14 ostatnich: 10 po 18 + 4 bez P95 → 10 z 14
      if (d == 24) SPRAWDZ(!spel(r, "L-09/.sk10") && oceniona(r, "L-09/.sk10"));  // 9 po 18 + 5 bez → nie
      if (d == 49) SPRAWDZ(!spel(r, "L-09/") && oceniona(r, "L-09/.sk10"));  // nie trzyma się nocy sprzed tygodni
      std::vector<Wynik> w;
      for (auto& kv : r) w.push_back(kv.second);
      reguly::aktualizujPamiec(pam, w, h.back().data);
    }
    SPRAWDZ(dzienWyjscia == 26);  // 7. noc bez P95
    // przeplot: noc bez P95 nie przerywa serii wyjścia, noc z P95 ≥ 15 przerywa
    std::vector<Noc> h2 = seria(d0, 14, 12.0f);
    for (auto& n : h2) n.wp95 = 18.0f;
    reguly::Pamiec pam2;
    for (int d = 0; d < 9; ++d) {
      h2.push_back(noc(dodajDni(d0, 14 + d), 420, 12.0f));
      h2.back().wp95 = d % 2 ? NAN : 9.0f;
      r = ocenDzis(h2, &o, &pam2);
      if (d == 5) SPRAWDZ(!spel(r, "L-09/.wyjscie"));
      if (d == 6) SPRAWDZ(spel(r, "L-09/.wyjscie"));
      std::vector<Wynik> w;
      for (auto& kv : r) w.push_back(kv.second);
      reguly::aktualizujPamiec(pam2, w, h2.back().data);
    }
  }
  {  // tryby CPAP i AutoSet (ADR-0030, dane syntetyczne): reguły bez wariantu działają tak samo
    auto h = seria(d0, 14, 10.0f);
    for (auto& n : h) { n.ustawienia = "AutoSet////"; n.tryb = "AutoSet"; }
    SPRAWDZ(spel(ocenDzis(h, nullptr, nullptr, reguly::Tryb::AUTOSET), "L-03/ge10"));
    auto c7 = seria(d0, 7, 6.0f);
    for (auto& n : c7) { n.ustawienia = "CPAP////"; n.tryb = "CPAP"; n.ca = 5.0f; }
    auto r = ocenDzis(c7, nullptr, nullptr, reguly::Tryb::CPAP);
    SPRAWDZ(spel(r, "L-01/cpap") && !oceniona(r, "L-01/"));  // w CPAP gałąź cpap, nie gałąź ASV
    // R-SKI-08 (po przeglądzie klinicznym): karencja tylko od znanej daty rozpoczęcia
    // leczenia; bez ustawienia „Początek leczenia” pierwsze noce w rekordach nie dają karencji
    SPRAWDZ(!spel(r, "L-01/cpap.karencja") && !spel(r, "L-01/cpap.okno"));
    g_poczatekLeczenia = c7.front().data;  // leczenie od pierwszej nocy: karencja
    r = ocenDzis(c7, nullptr, nullptr, reguly::Tryb::CPAP);
    SPRAWDZ(spel(r, "L-01/cpap.okno_poczatek") && spel(r, "L-01/cpap.karencja"));
    g_poczatekLeczenia = dodajDni(c7.back().data, -56);  // 57. dzień od początku: poza oknem 56 dni
    SPRAWDZ(!spel(ocenDzis(c7, nullptr, nullptr, reguly::Tryb::CPAP), "L-01/cpap.karencja"));
    g_poczatekLeczenia = dodajDni(c7.back().data, -55);  // 56. dzień: jeszcze w oknie
    SPRAWDZ(spel(ocenDzis(c7, nullptr, nullptr, reguly::Tryb::CPAP), "L-01/cpap.karencja"));
    g_poczatekLeczenia = dodajDni(c7.back().data, 1);  // data w przyszłości: jak nieznana (fail-path R-SKI-08)
    SPRAWDZ(!spel(ocenDzis(c7, nullptr, nullptr, reguly::Tryb::CPAP), "L-01/cpap.karencja"));
    g_poczatekLeczenia = "10.09.2026";  // nieczytelna data: jak nieznana
    SPRAWDZ(!spel(ocenDzis(c7, nullptr, nullptr, reguly::Tryb::CPAP), "L-01/cpap.karencja"));
    g_poczatekLeczenia = c7.front().data;  // tryb ASVAuto z datą: bez karencji (R-SKI-43)
    for (auto& n : c7) { n.ustawienia = "ASVAuto////"; n.tryb = "ASVAuto"; }
    SPRAWDZ(!oceniona(ocenDzis(c7, nullptr, nullptr, reguly::Tryb::ASVAUTO), "L-01/cpap.karencja"));
    g_poczatekLeczenia.clear();
    // R-HIP-10 we wszystkich trybach (pkt 5): w CPAP zamiast H-04 wiersz H-08/L-01~cpap; bez zmiany ustawień niespełniony
    for (auto& n : c7) { n.ustawienia = "CPAP////"; n.tryb = "CPAP"; }
    r = ocenDzis(c7, nullptr, nullptr, reguly::Tryb::CPAP);
    SPRAWDZ(oceniona(r, "H-08/L-01~cpap") && !spel(r, "H-08/L-01~cpap") && !spel(r, "H-04/L-01"));
    SPRAWDZ(!oceniona(r, "H-04/L-01~cpap") && !oceniona(r, "H-04/L-01.okno"));  // wiersze H-04 w CPAP wycofane
    // L-04 AutoSet (MPA z rekordu nocy), T-TEST-358: M-05p ≥ MPA − 0,5 w ≥ 5 z 7 nocy i mediana OA ≥ 5 / h (tylko
    // OA, po przeglądzie klinicznym)
    auto a = seria(d0, 7, 6.0f);
    for (size_t i = 0; i < 7; ++i) {
      a[i].ustawienia = "AutoSet////";
      a[i].tryb = "AutoSet";
      a[i].mpa = 13.0f;
      a[i].cp95 = i < 5 ? 12.5f : 10.0f;
      a[i].oa = 5.0f;
      a[i].ua = 1.0f;
      a[i].h = 2.0f;
    }
    auto ra = ocenDzis(a, nullptr, nullptr, reguly::Tryb::AUTOSET);
    SPRAWDZ(spel(ra, "L-04/autoset") && !oceniona(ra, "L-04/"));
    // H-02 w AutoSet wycofane (pkt 6): przy L-04 H-08 `Przyczynę oceni lekarz.` (R-HIP-04)
    SPRAWDZ(!spel(ra, "H-02/L-04~autoset") && !oceniona(ra, "H-02/L-04~tryb") && spel(ra, "H-08/L-04~autoset"));
    ROWNE(wzorce::hipoteza("L-04", ocenWek(a, nullptr, nullptr, reguly::Tryb::AUTOSET)), "");  // tylko raport: bez ramki
    a[4].cp95 = 12.4f;
    SPRAWDZ(!spel(ocenDzis(a, nullptr, nullptr, reguly::Tryb::AUTOSET), "L-04/autoset"));
    a[4].cp95 = 12.5f;
    for (auto& n : a) n.oa = 4.9f;  // OA 4,9 → brak
    SPRAWDZ(!spel(ocenDzis(a, nullptr, nullptr, reguly::Tryb::AUTOSET), "L-04/autoset"));
    for (auto& n : a) { n.oa = 3.0f; n.ua = 2.0f; n.h = 2.0f; }  // OA 3 i UA + H 4 (suma 7): UA i spłycenia się nie liczą
    SPRAWDZ(!spel(ocenDzis(a, nullptr, nullptr, reguly::Tryb::AUTOSET), "L-04/autoset"));
    for (auto& n : a) { n.oa = 5.0f; n.ua = 1.0f; n.mpa = 15.0f; n.cp95 = 14.5f; }  // maksimum ≥ 15: bez wariantu „tryb”
    auto rt = ocenDzis(a, nullptr, nullptr, reguly::Tryb::AUTOSET);
    SPRAWDZ(spel(rt, "L-04/autoset") && !oceniona(rt, "H-02/L-04~tryb") && spel(rt, "H-08/L-04~autoset"));
    for (auto& n : a) n.mpa = NAN;  // brak odczytu maksimum → nie oceniam, nigdy „w porządku”
    SPRAWDZ(!oceniona(ocenDzis(a, nullptr, nullptr, reguly::Tryb::AUTOSET), "L-04/autoset.cisnienie"));
  }
  {  // L-02 (b) na normie CSR%: norma 1 %, mediana z 7 nocy 2 % (2 × i +1 pkt) → wejście; 1,9 % → brak
    auto h = seria(d0, 37, 2.0f);
    for (size_t i = 0; i < 30; ++i) h[i].csr = 4.2f;  // 1 % z 420 min
    for (size_t i = 30; i < 37; ++i) h[i].csr = 8.4f;  // 2 %
    SPRAWDZ(spel(ocenDzis(h), "L-02/b"));
    for (size_t i = 30; i < 37; ++i) h[i].csr = 7.98f;  // 1,9 %
    SPRAWDZ(!spel(ocenDzis(h), "L-02/b"));
  }
  {  // L-05 na normie M-13, T-TEST-165: norma 15, 18 w 3 kolejnych nocach i „gorzej” = tak → wejście
    auto h = seria(d0, 33, 2.0f);
    for (size_t i = 0; i < 30; ++i) h[i].m13 = 15.0f;
    for (size_t i = 30; i < 33; ++i) h[i].m13 = 18.0f;
    reguly::Odpowiedzi o;
    o.gorzej[h.back().data] = true;
    SPRAWDZ(spel(ocenDzis(h, &o), "L-05/"));
    SPRAWDZ(!spel(ocenDzis(h), "L-05/"));  // bez L-01, L-02(b) i „gorzej” → brak
    h[31].m13 = 17.9f;
    SPRAWDZ(!spel(ocenDzis(h, &o), "L-05/"));
  }
  {  // L-10 na M-10p (wariant tymczasowy, R-KON-30): S-K-19 od 14 dni (pamięć) i senność tak w obu tygodniach → wejście
    std::vector<Noc> h = seria(d0, 40, 2.0f);
    for (size_t i = 0; i < h.size(); ++i) h[i].m10p = (i % 2) ? 0.10f : 0.14f;  // norma: mediana 0,12, P90 0,14
    for (int d = 0; d < 7; ++d) {
      h.push_back(noc(dodajDni(d0, 40 + d), 420, 2.0f));
      h.back().m10p = 0.30f;
    }
    const std::string dz = h.back().data;
    reguly::Odpowiedzi o;
    o.sennosc[dz] = true;
    o.sennosc[dodajDni(dz, -7)] = true;
    reguly::Pamiec pam;
    pam["L-10/.sk19"].aktywnaOd = dodajDni(dz, -13);  // 14. dzień S-K-19
    auto r = ocenDzis(h, &o, &pam);
    SPRAWDZ(spel(r, "L-10/.sk19"));
    SPRAWDZ(spel(r, "L-10/"));
    pam["L-10/.sk19"].aktywnaOd = dodajDni(dz, -12);
    SPRAWDZ(!spel(ocenDzis(h, &o, &pam), "L-10/"));
    pam["L-10/.sk19"].aktywnaOd = dodajDni(dz, -13);
    o.sennosc[dodajDni(dz, -7)] = false;
    SPRAWDZ(!spel(ocenDzis(h, &o, &pam), "L-10/"));
    o.sennosc[dodajDni(dz, -7)] = true;
    r = ocenDzis(h, &o, &pam);
    auto it = r.find("L-10/");
    SPRAWDZ(it != r.end() && it->second.stan == StanReguly::TYLKO_RAPORT && it->second.pilnosc == Pilnosc::BRAK);
  }
  {  // przebieg z aktualizuj(): tylko raport trafia do spraw jako „w raporcie, bez pilności”
    auto h = seria(d0, 28, 5.0f);
    const std::string dz = h.back().data;
    const Stany s = stanyZPamieci(h, dz, {});
    reguly::Kontekst k;
    k.historia = &h;
    k.dzis = dz;
    k.norma = &s.norma;
    k.stany = &s;
    k.trybBiezacy = reguly::Tryb::ASVAUTO;
    reguly::Odpowiedzi o;
    reguly::Pamiec p;
    k.odpowiedzi = &o;
    k.pamiec = &p;
    k.zestaw = &z;
    auto w = reguly::ocen(z, k);
    wzorce::aktualizuj(w, p, dz, &z);
    bool jest = false;
    for (const auto& sp : otwarte(w, p, dz, &z))
      if (sp.klucz == "L-03/5_10") jest = sp.karta == Karta::W_RAPORCIE && sp.pilnosc == Pilnosc::BRAK;
    SPRAWDZ(jest);
  }
}

// ================= część 3: dyrygent z prawdziwym silnikiem (src/dyrygent.cpp) =================
// 15 nocy z AHI 12 / h z AirBridge → rano ekran dnia L-03 (≥ 10, pilność B), stan wzorca zapisany w pamięci reguł.
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
static const char* STATUS =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";
static std::string raportNocy(long day, const char* ahi) {
  std::string d = std::to_string(day);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":" + std::string(ahi) + ",\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12660,"
         "\"coverage_seconds\":25320,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static bool wLogu(const Panel& p, const std::string& f) {
  for (auto& l : p.port.log)
    if (l.find(f) != std::string::npos) return true;
  return false;
}
static void testDyrygentL03() {
  printf("dyrygent z silnikiem: 15 nocy z AHI 12 / h → rano ekran L-03 (silnik reguł w dyrygencie)\n");
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();  // bez atrapy: zestaw wbudowany i reguly::ocen()
  Panel p;
  const int NOCY = 15;
  long ost = nrDnia("2026-11-04") - 1;
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = ost - NOCY + 1; x <= ost; x++)
    l += std::string(x > ost - NOCY + 1 ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  l += "]}";
  bool jest = false;
  for (int h = 0; h < 12 && !jest; h++) {  // ostatnia noc i zaległe doby partiami
    p.port.log.clear();
    p.siec.odpowiedzi.clear();
    p.zegar.epoch = ep(2026, 11, 4, 7, 0) + (uint32_t)(h * 1800);
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
    p.start.powodWybudzenia = PowodWybudzenia::START;
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.siec.dodaj("/api/status", 200, STATUS);
    p.siec.dodaj(sciezkaListy(), 200, l);
    for (long x = ost - NOCY + 1; x <= ost; x++) p.siec.dodaj(sciezkaNocy(x), 200, raportNocy(x, "12.0"));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    jest = wLogu(p, "ekran dnia: wzorzec L-03");
  }
  if (!jest || getenv("WZORCE_LOG"))
    for (auto& x : p.port.log)
      if (x.find("silnik") != std::string::npos || x.find("ekran dnia") != std::string::npos ||
          x.find("L-03") != std::string::npos || x.find("historia") != std::string::npos)
        printf("    %s\n", x.c_str());
  SPRAWDZ(jest);
  // stan wzorca L-03 trafił do pamięci reguł (aktualizuj() po aktualizujPamiec(), przed zapisem)
  const std::string pam = p.pliki.tresc("/dane/reguly.csv");
  SPRAWDZ(pam.find("L-03;ge10;") != std::string::npos && pam.find("aktywny,B,") != std::string::npos);
}

int main(int argc, char** argv) {
  if (argc > 1) REPO = argv[1];
  testKatalog();
  reguly::Zestaw z = zestawTestowy();
  testStany(z);
  wzorce::zarejestrujRodzajeL();
  reguly::zarejestrujRodzajeE();
  testSilnik();
  testDyrygentL03();
  printf("testy_wzorce_l_hipotezy_h: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
