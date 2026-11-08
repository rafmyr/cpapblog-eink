// SPDX-License-Identifier: GPL-3.0-or-later
// Testy (
// T-TEST-330…T-TEST-336): (A) przerywniki S2d: teksty autora projektu dosłownie, karta z warunkiem i neutralna, warunki nazwane
// (R-HUM-28), „nie oceniam” bez karty warunkowej, pierwszeństwo ekranów (R-HUM-29), układ karty bez etykiety mieści 36 tekstów
// i nie wchodzi na rysunek budzika, (B) (S-K-17 w okresie infekcji) pilnuje test/testy_sprzet_infekcja_sk.cpp (T-TEST-335), (C) krok zamienny przy braku danych
// (powodBrakuDanych(), Niespokoj::DANE). Dane syntetyczne. Uruchomienie: tools/prototyp/testy.sh (program testy_przerywniki_s2d).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "dzien.h"
#include "ekrany.h"
#include "humor.h"
#include "kolejka.h"
#include "kontekst_sk.h"
#include "noc.h"
#include "wspolne.h"

using namespace kolejka;
static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE(a, b)                                                                                     \
  do {                                                                                                  \
    std::string _a = (a), _b = (b);                                                                     \
    if (_a == _b) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)

static std::string REPO = ".";

// ---------- plik autora (docs/design/przerywniki-teksty.md) ----------
struct Teksty {
  std::map<std::string, std::string> warunek, neutralna, warunekOpis;  // scena → pełny tekst
};
static Teksty wczytajPlik() {
  Teksty t;
  std::ifstream f(REPO + "/docs/design/przerywniki-teksty.md");
  std::string l, sekcja;
  while (std::getline(f, l)) {
    if (l.rfind("## Karty z warunkiem", 0) == 0) sekcja = "w";
    else if (l.rfind("## Karty neutralne", 0) == 0) sekcja = "n";
    else if (l.size() > 4 && l[0] == 'G' && l[3] == ' ') {
      std::string sc = l.substr(0, 3);
      if (sekcja == "w") {
        size_t a = l.find(" | "), b = l.find(" | ", a + 3), c = l.find(" | ", b + 3);
        t.warunekOpis[sc] = l.substr(b + 3, c - b - 3);
        t.warunek[sc] = l.substr(c + 3);
      } else if (sekcja == "n") {
        t.neutralna[sc] = l.substr(4);
      }
    }
  }
  return t;
}
static std::string scenaId(int g) {
  char s[8];
  snprintf(s, sizeof s, "G%02d", g);
  return s;
}
static std::string polacz(const TekstPrzerywnika& t) { return t.powod.empty() ? t.tytul : t.tytul + " " + t.powod; }

// T-TEST-330: tekst dosłownie równy plikowi autora dla wszystkich 36 kart; podział na tytuł (pierwsze zdanie) i powód
static void testTekstyDoslownie() {
  Teksty t = wczytajPlik();
  SPRAWDZ(t.warunek.size() == 18 && t.neutralna.size() == 18);
  std::set<std::string> wszystkie;
  for (int g = 1; g <= 18; g++) {
    std::string sc = scenaId(g);
    for (bool war : {true, false}) {
      TekstPrzerywnika tp = tekstPrzerywnika(sc, war);
      const std::string& plik = war ? t.warunek[sc] : t.neutralna[sc];
      ROWNE(polacz(tp), plik);
      SPRAWDZ(!tp.tytul.empty());
      SPRAWDZ(tp.tytul.back() == '.');  // tytuł kończy się kropką (5.7.2)
      SPRAWDZ(tp.tytul.find(". ") == std::string::npos);  // tytuł = jedno (pierwsze) zdanie
      std::string w = polacz(tp);
      // R-HUM-21: bez kroku; ADR-0029 pkt 7: żadnego „w porządku” (nie dotyczy tekstów autora poza listą niżej)
      SPRAWDZ(w.find("→") == std::string::npos);
      // ADR-0002: żadnej wartości ustawień ani drogi ich zmiany
      for (const char* z : {"cmH2O", "cmH₂O", "EPR", "EPAP", "menu", "Menu"}) SPRAWDZ(w.find(z) == std::string::npos);
      bool cyfra = false;
      for (char c : w) cyfra = cyfra || (c >= '0' && c <= '9');
      SPRAWDZ(!cyfra);
      // każdy znak jest w kroju
      for (size_t i = 0; i < w.size();) {
        unsigned char c = (unsigned char)w[i];
        int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        uint32_t cp = n == 1 ? c : (uint32_t)(c & (0xFF >> (n + 1)));
        for (int k = 1; k < n; k++) cp = (cp << 6) | ((unsigned char)w[i + (size_t)k] & 0x3F);
        if (!znakJestWKroju(cp)) printf("  znak spoza kroju U+%04X w %s %s\n", cp, sc.c_str(), war ? "warunek" : "neutralna");
        SPRAWDZ(znakJestWKroju(cp));
        i += (size_t)n;
      }
      wszystkie.insert(w);
    }
  }
  SPRAWDZ(wszystkie.size() == 36);
  // scena spoza G i pusta: bez tekstu
  SPRAWDZ(tekstPrzerywnika("B01", true).tytul.empty() && tekstPrzerywnika("", false).tytul.empty());
}

// ---------- fakty nocy ----------
static Noc noc(const std::string& data, int m01, float ahi, float wp95, bool wazna = true) {
  Noc n;
  n.data = data;
  n.wazna = wazna;
  n.m01 = m01;
  n.ahi = ahi;
  n.wp95 = wp95;
  return n;
}
static const std::string DZIS = "2026-11-12";
static NocPrzerywnika fakty(const std::vector<Noc>& h, bool stanAhi = false) { return nocPrzerywnika(h, DZIS, stanAhi); }
static std::set<std::string> spelnione(const NocPrzerywnika& n) {
  std::set<std::string> s;
  for (int g = 1; g <= 18; g++)
    if (warunekPrzerywnika(scenaId(g), n)) s.insert(scenaId(g));
  return s;
}
static std::set<std::string> zbior(std::initializer_list<const char*> l) { return std::set<std::string>(l.begin(), l.end()); }

// tydzień w oknie 7 dni (11.11 wstecz), ostatnia noc DZIS
static std::vector<Noc> tydzien(float ahiDzis, int m01Dzis, float wpDzis, const std::vector<float>& wczesniej) {
  std::vector<Noc> h;
  for (size_t i = 0; i < wczesniej.size(); i++) h.push_back(noc(dodajDni(DZIS, -(int)(i + 1)), 400, wczesniej[i], 12.0f));
  h.push_back(noc(DZIS, m01Dzis, ahiDzis, wpDzis));
  return h;
}

// T-TEST-331: warunek spełniony i niespełniony dla każdej sceny (R-HUM-28)
static void testWarunki() {
  // progi: AHI 5, wyciek 24, M-01 4 h / 6 h / 7 h, AHI 1 i 10
  SPRAWDZ(PROG_AHI == 5.0 && PROG_WYCIEK == 24.0);
  SPRAWDZ(PRZ_M01_KROTKA_MIN == 240 && PRZ_M01_W_CELU_MIN == 360 && PRZ_M01_DLUGA_MIN == 420);
  SPRAWDZ(PRZ_AHI_BARDZO_NISKIE == 1.0 && PRZ_AHI_DUZO == 10.0 && PRZ_TYDZIEN_NOCE == 5);
  // dobra noc: AHI 0,6, wyciek 9 (G15 „wycieki niewielkie” do 10 L/min), 8 h; tydzień 6
  // nocy z AHI 1…2
  {
    NocPrzerywnika n = fakty(tydzien(0.6f, 480, 9.0f, {1, 2, 1.5f, 2, 1, 2}));
    SPRAWDZ(n.jest && n.ocenione && n.noce7 == 7);
    SPRAWDZ(duzeWycieki(n) == false && niskieAhi(n) && bardzoNiskieAhi(n) && stabilneAhi(n) && dobraTerapia(n));
    SPRAWDZ(dlugaTerapia(n) && wszystkoWCelu(n) && dobryTydzien(n));  // poprzednie noce 6 h 40, dziś 8 h: poprawa 80 min
    SPRAWDZ(!podwyzszoneAhi(n) && !duzoZdarzen(n) && !krotkaTerapia(n));
    SPRAWDZ(spelnione(n) == zbior({"G02", "G03", "G04", "G05", "G06", "G08", "G13", "G15", "G16", "G18"}));
    // niespełnione: G01, G07, G09, G10, G11, G12, G14, G17
    for (const char* sc : {"G01", "G07", "G09", "G10", "G11", "G12", "G14", "G17"}) SPRAWDZ(!warunekPrzerywnika(sc, n));
  }
  // granice progów: AHI 5,0 nie jest niskie, 4,9 jest; AHI 1,0 nie jest bardzo niskie; 10,0 to dużo zdarzeń
  SPRAWDZ(!niskieAhi(fakty(tydzien(5.0f, 480, 12.0f, {}))) && podwyzszoneAhi(fakty(tydzien(5.0f, 480, 12.0f, {}))));
  SPRAWDZ(niskieAhi(fakty(tydzien(4.9f, 480, 12.0f, {}))) && !podwyzszoneAhi(fakty(tydzien(4.9f, 480, 12.0f, {}))));
  SPRAWDZ(!bardzoNiskieAhi(fakty(tydzien(1.0f, 480, 12.0f, {}))) && bardzoNiskieAhi(fakty(tydzien(0.9f, 480, 12.0f, {}))));
  SPRAWDZ(duzoZdarzen(fakty(tydzien(10.0f, 480, 12.0f, {}))) && !duzoZdarzen(fakty(tydzien(9.9f, 480, 12.0f, {}))));
  // wyciek: 24 nie jest dużym wyciekiem (powyżej progu), 24,1 tak; dobra terapia wymaga wycieku poniżej progu
  SPRAWDZ(!duzeWycieki(fakty(tydzien(2.0f, 480, 24.0f, {}))) && duzeWycieki(fakty(tydzien(2.0f, 480, 24.1f, {}))));
  // wariant C: noc z P95 ≥ 15 L/min jest niepewna (4.9), więc AHI nieocenione i „dobra terapia” odpada
  // już od 15, choć duży wyciek (duzeWycieki) dalej od 24
  SPRAWDZ(!dobraTerapia(fakty(tydzien(2.0f, 480, 24.0f, {}))) && !dobraTerapia(fakty(tydzien(2.0f, 480, 23.9f, {}))));
  SPRAWDZ(!dobraTerapia(fakty(tydzien(2.0f, 480, 15.0f, {}))) && dobraTerapia(fakty(tydzien(2.0f, 480, 14.9f, {}))));
  // czas terapii: 3 h 59 krótka, 4 h nie; 6 h 59 nie jest długa, 7 h jest; w celu od 6 h
  SPRAWDZ(krotkaTerapia(fakty(tydzien(2.0f, 239, 12.0f, {}))) && !krotkaTerapia(fakty(tydzien(2.0f, 240, 12.0f, {}))));
  // długi czas = 7 h ORAZ co najmniej 1 h ponad medianą M-01 z 7 poprzednich nocy ważnych (min. 5 nocy)
  {
    auto z = [&](int m01Dzis, int m01Poprz, int ile) {  // `ile` poprzednich nocy po m01Poprz minut
      std::vector<Noc> h;
      for (int i = 1; i <= ile; i++) h.push_back(noc(dodajDni(DZIS, -i), m01Poprz, 2.0f, 12.0f));
      h.push_back(noc(DZIS, m01Dzis, 2.0f, 12.0f));
      return fakty(h);
    };
    SPRAWDZ(PRZ_M01_POPRAWA_MIN == 60 && PRZ_M01_POPRAWA_NOCE == 5);
    SPRAWDZ(!dlugaTerapia(z(420, 540, 7)));  // norma 9 h, noc 7 h: krótka noc, karta neutralna
    SPRAWDZ(!warunekPrzerywnika("G06", z(420, 540, 7)) && !warunekPrzerywnika("G16", z(420, 540, 7)));
    SPRAWDZ(dlugaTerapia(z(420, 330, 7)) && warunekPrzerywnika("G16", z(420, 330, 7)));  // mediana 5 h 30, noc 7 h
    SPRAWDZ(dlugaTerapia(z(480, 420, 7)) && !dlugaTerapia(z(479, 420, 7)));  // granica dokładnie 1 h ponad medianą
    SPRAWDZ(!dlugaTerapia(z(419, 300, 7)) && dlugaTerapia(z(420, 300, 7)));  // próg 7 h zostaje
    SPRAWDZ(dlugaTerapia(z(480, 420, 5)) && !dlugaTerapia(z(480, 420, 4)) && !dlugaTerapia(z(480, 420, 0)));  // za mało nocy / brak historii
    {  // mediana parzysta: 3 noce po 300 i 3 po 400 → 350; 420 to tylko +70, 410 to +60
      std::vector<Noc> h;
      for (int i = 1; i <= 6; i++) h.push_back(noc(dodajDni(DZIS, -i), i <= 3 ? 300 : 400, 2.0f, 12.0f));
      h.push_back(noc(DZIS, 420, 2.0f, 12.0f));
      SPRAWDZ(dlugaTerapia(fakty(h)));
    }
    {  // noce nieważne (< 4 h) do mediany nie wchodzą
      std::vector<Noc> h;
      for (int i = 1; i <= 7; i++) h.push_back(noc(dodajDni(DZIS, -i), 100, 2.0f, 12.0f, false));
      h.push_back(noc(DZIS, 480, 2.0f, 12.0f));
      SPRAWDZ(!dlugaTerapia(fakty(h)));
    }
  }
  SPRAWDZ(!wszystkoWCelu(fakty(tydzien(2.0f, 359, 9.0f, {}))) && wszystkoWCelu(fakty(tydzien(2.0f, 360, 9.0f, {}))));
  // G15 `wycieki niewielkie`: M-07 P95 do 10 L/min (R-HUM-28.wyciek_niewielki; było 16)
  SPRAWDZ(PRZ_WYCIEK_NIEWIELKI == 10.0);
  SPRAWDZ(wszystkoWCelu(fakty(tydzien(2.0f, 480, 10.0f, {}))) && !wszystkoWCelu(fakty(tydzien(2.0f, 480, 10.1f, {}))));
  SPRAWDZ(!wszystkoWCelu(fakty(tydzien(2.0f, 480, 12.0f, {}))) && dobraTerapia(fakty(tydzien(2.0f, 480, 12.0f, {}))));
  // podwyższone AHI i dużo zdarzeń: AHI 12, wyciek 12
  {
    NocPrzerywnika n = fakty(tydzien(12.0f, 480, 12.0f, {}));
    SPRAWDZ(spelnione(n) == zbior({"G09", "G10"}));  // G06 i G16 tylko przy AHI < 5
  }
  // duże wycieki przy ważnej nocy: G01, G07, G12, G17 (AHI nieocenione z dużym wyciekiem tylko przy niepewnej nocy)
  {
    NocPrzerywnika n = fakty(tydzien(2.0f, 480, 31.0f, {}));
    SPRAWDZ(spelnione(n).count("G01") && spelnione(n).count("G07") && spelnione(n).count("G12") && spelnione(n).count("G17"));
    SPRAWDZ(!spelnione(n).count("G02") && !spelnione(n).count("G18"));  // wyciek ponad próg: nie „dobra terapia”
  }
  // dobry wynik tygodnia: mediana < 5 przy co najmniej 5 nocach; 4 noce za mało; mediana 5,0 nie
  SPRAWDZ(dobryTydzien(fakty(tydzien(2.0f, 480, 12.0f, {3, 3, 4, 4}))));            // 5 nocy, mediana 3
  SPRAWDZ(!dobryTydzien(fakty(tydzien(2.0f, 480, 12.0f, {3, 3, 4}))));              // 4 noce
  SPRAWDZ(!dobryTydzien(fakty(tydzien(6.0f, 480, 12.0f, {6, 6, 6, 1, 1}))));        // mediana 6
  SPRAWDZ(!dobryTydzien(fakty(tydzien(5.0f, 480, 12.0f, {5, 5, 5, 5, 5}))));        // mediana 5,0 nie < 5
  // stan odchylenia AHI: AHI niskie, ale G08 (stabilne) nie
  {
    NocPrzerywnika n = fakty(tydzien(2.0f, 480, 12.0f, {}), true);
    SPRAWDZ(niskieAhi(n) && !stabilneAhi(n) && !warunekPrzerywnika("G08", n) && warunekPrzerywnika("G05", n));
  }
}

// T-TEST-332: „nie oceniam”, brak danych, noc nieważna: brak karty warunkowej, nigdy tekstu „dobrego” (ADR-0029 pkt 7)
static void testNieOceniam() {
  const std::set<std::string> DOBRE = zbior({"G02", "G03", "G04", "G05", "G08", "G13", "G15", "G18"});
  const std::set<std::string> Z_AHI = zbior({"G02", "G03", "G04", "G05", "G08", "G09", "G10", "G13", "G15", "G18"});
  // brak rekordu nocy z dzisiejszą datą (poprzednia noc dobra): żadnej karty warunkowej
  {
    std::vector<Noc> h = {noc(dodajDni(DZIS, -1), 480, 0.5f, 10.0f)};
    NocPrzerywnika n = fakty(h);
    SPRAWDZ(!n.jest && spelnione(n).empty());
  }
  SPRAWDZ(spelnione(NocPrzerywnika{}).empty());  // domyślne fakty = brak danych
  // noc niepewna (duży wyciek M-08 45 min): AHI nieocenione, więc żadna karta z AHI; wyciek sam jest oceniony
  {
    std::vector<Noc> h = tydzien(0.4f, 480, 40.0f, {1, 1, 1, 1, 1, 1});
    h.back().m08 = 45.0f;
    NocPrzerywnika n = fakty(h);
    SPRAWDZ(n.jest && !n.ocenione);
    for (auto& sc : Z_AHI) SPRAWDZ(!warunekPrzerywnika(sc, n));
    SPRAWDZ(spelnione(n) == zbior({"G01", "G07", "G12", "G17"}));  // G06 i G16 wymagają AHI
    for (auto& sc : DOBRE) SPRAWDZ(!warunekPrzerywnika(sc, n));
  }
  // noc bez AHI (NAN): jak niepewna
  {
    NocPrzerywnika n = fakty(tydzien(NAN, 480, 12.0f, {1, 1, 1, 1, 1}));
    SPRAWDZ(!n.ocenione);
    for (auto& sc : Z_AHI) SPRAWDZ(!warunekPrzerywnika(sc, n));
    SPRAWDZ(!dobraTerapia(n) && !wszystkoWCelu(n) && !dobryTydzien(n));
  }
  // noc nieważna (3 h): tylko Krótka terapia (G11, G14); nic z AHI i wycieku, nawet przy AHI 0,2
  {
    std::vector<Noc> h = {noc(DZIS, 190, 0.2f, 40.0f, false)};
    NocPrzerywnika n = fakty(h);
    SPRAWDZ(n.jest && !n.ocenione && spelnione(n) == zbior({"G11", "G14"}));
  }
  // noc częściowa i noc bez terapii: nic
  {
    Noc c = noc(DZIS, 300, 1.0f, 10.0f);
    c.czesciowa = true;
    SPRAWDZ(spelnione(fakty({c})).empty());
    SPRAWDZ(spelnione(fakty({noc(DZIS, 0, NAN, NAN, false)})).empty());
  }
  // G03: ostatnia noc nieoceniona → bez karty, nawet gdy tydzień dobry
  {
    std::vector<Noc> h = tydzien(NAN, 480, 12.0f, {1, 1, 1, 1, 1, 1});
    SPRAWDZ(!warunekPrzerywnika("G03", fakty(h)));
  }
  // noce niepewne nie liczą się do tygodnia: 3 dobre + 3 niepewne + dziś dobra = 4 oceniane noce → bez karty
  {
    std::vector<Noc> h = tydzien(1.0f, 480, 12.0f, {1, 1, 1, 1, 1, 1});
    for (int i = 0; i < 3; i++) { h[(size_t)i].m08 = 60.0f; }
    SPRAWDZ(!dobryTydzien(fakty(h)));
  }
}

// ---------- wybór przerywnika ----------
static KontekstHumoru kontekst(const std::string& dzis, const NocPrzerywnika& n) {
  KontekstHumoru k;
  k.dzis = dzis;
  k.sceny = sceny();
  k.zPropozycjami = true;
  k.noc = n;
  return k;
}

// T-TEST-333: karta z warunkiem albo neutralna po wyborze sceny; ekran problemu, wzorzec lekarski i linia uwagi przed kartą
static void testWybor() {
  UstawieniaPrzerywnikow u = ustawieniaPrzerywnikowBety();
  u.zDatami = false;  // bez okazji i dni tygodnia: pula ogólna zawężona do jednej sceny
  const EkranDnia dowod{"S2A", "1", false, false};
  // wtorek 2026-11-10, jedna scena w puli: G13 (bardzo niskie AHI)
  u.ogolne = {"G13"};
  {
    KontekstHumoru k = kontekst("2026-11-10", nocPrzerywnika({noc("2026-11-10", 480, 0.5f, 10.0f)}, "2026-11-10", false));
    Przerywnik p = wybierzPrzerywnik(dowod, {}, k, u);
    SPRAWDZ(p.jest && p.scena == "G13" && p.warunkowa);
    k.noc = nocPrzerywnika({noc("2026-11-10", 480, 2.0f, 10.0f)}, "2026-11-10", false);
    p = wybierzPrzerywnik(dowod, {}, k, u);
    SPRAWDZ(p.jest && p.scena == "G13" && !p.warunkowa);  // warunek niespełniony: karta neutralna
    k.noc = NocPrzerywnika{};
    p = wybierzPrzerywnik(dowod, {}, k, u);
    SPRAWDZ(p.jest && !p.warunkowa);  // brak danych o nocy: neutralna, jeśli zwykłe warunki przerywnika spełnione
    // ten sam dzień: ten sam przerywnik (wiersz S2D w dzienniku), wariant z faktów nocy
    std::vector<WpisHumoru> log = {{"2026-11-10", "S2D", "L", 'K', "G13", "przerywnik_sceny:12"}};
    k.noc = nocPrzerywnika({noc("2026-11-10", 480, 0.5f, 10.0f)}, "2026-11-10", false);
    p = wybierzPrzerywnik(dowod, log, k, u);
    SPRAWDZ(p.jest && p.scena == "G13" && p.warunkowa);
    // teksty obu wariantów różne
    SPRAWDZ(polacz(tekstPrzerywnika("G13", true)) != polacz(tekstPrzerywnika("G13", false)));
  }
  // pierwszeństwo: ekran problemu (S3, SK10), linia uwagi, wzorzec B: bez przerywnika, mimo spełnionego warunku
  {
    KontekstHumoru k = kontekst("2026-11-10", nocPrzerywnika({noc("2026-11-10", 480, 0.5f, 10.0f)}, "2026-11-10", false));
    SPRAWDZ(wybierzPrzerywnik(dowod, {}, k, u).jest);
    SPRAWDZ(!wybierzPrzerywnik(EkranDnia{"S3", "E-23", false, false}, {}, k, u).jest);
    SPRAWDZ(!wybierzPrzerywnik(EkranDnia{"SK10", "s#1", false, false}, {}, k, u).jest);
    SPRAWDZ(!wybierzPrzerywnik(EkranDnia{"L", "L-03", false, false}, {}, k, u).jest);
    KontekstHumoru kl = k;
    kl.liniaUwagi = true;
    SPRAWDZ(!wybierzPrzerywnik(dowod, {}, kl, u).jest);
    KontekstHumoru kb = k;
    kb.wzorzecB = true;
    SPRAWDZ(!wybierzPrzerywnik(dowod, {}, kb, u).jest);
    KontekstHumoru ko = k;
    ko.humor = Humor::OSZCZEDNY;
    SPRAWDZ(!wybierzPrzerywnik(dowod, {}, ko, u).jest);
  }
  // wybór sceny jak dotąd: z datami G02 tylko w poniedziałek (2026-11-09), G03 tylko w piątek (2026-11-13); karta z warunkiem
  // nie zmienia losowania (ta sama scena niezależnie od faktów nocy)
  {
    UstawieniaPrzerywnikow ud = ustawieniaPrzerywnikowBety();
    for (const char* dzien : {"2026-11-09", "2026-11-13", "2026-11-17", "2026-11-24"}) {
      KontekstHumoru dobra = kontekst(dzien, nocPrzerywnika({noc(dzien, 480, 0.5f, 10.0f)}, dzien, false));
      KontekstHumoru brak = kontekst(dzien, NocPrzerywnika{});
      Przerywnik a = wybierzPrzerywnik(dowod, {}, dobra, ud), b = wybierzPrzerywnik(dowod, {}, brak, ud);
      SPRAWDZ(a.jest && b.jest && a.scena == b.scena);
      SPRAWDZ(!b.warunkowa);
    }
  }
}

// ---------- układ karty bez etykiety ----------
struct Pamiec : Plotno {
  std::vector<unsigned char> b = std::vector<unsigned char>(800 * 480, 0);
  void piksel(int x, int y, bool czarny) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) b[(size_t)(y * 800 + x)] = czarny ? 1 : 0;
  }
  bool czarny(int x, int y) const { return b[(size_t)(y * 800 + x)] != 0; }
};

// T-TEST-334: 36 kart mieści się w układzie S2d i nie wchodzi na rysunek budzika; karta bez etykiety
static void testUklad() {
  Teksty tk = wczytajPlik();
  Naglowek n{"czwartek 12.11", "AirBridge 05:42"};
  DaneDnia d;
  d.dzis = DZIS;
  int p34 = 0;
  std::string wyjatki;
  for (int g = 1; g <= 18; g++) {
    std::string sc = scenaId(g);
    for (bool war : {true, false}) {
      TekstPrzerywnika tp = tekstPrzerywnika(sc, war);
      TrescEkranu t;
      t.etykieta = "OSTATNIA NOC";  // etykieta ekranu danych nie może trafić na kartę S2d
      t.stopkaPrawo = "przy łóżku 21° · 46%";
      trescPrzerywnika(t, tp.tytul, tp.powod, d);
      SPRAWDZ(t.kartaBezEtykiety && t.etykietaKarty.empty() && t.krok.empty());
      bool miesci = kartaPrzerywnikaMiesci(t);
      if (!miesci) wyjatki += " " + sc + (war ? "w" : "n");
      SPRAWDZ(miesci);
      if (zawin(F_SB44, tp.tytul, 748).size() > 2) {
        p34++;
        SPRAWDZ(zawin(F_P34B, tp.tytul, 748).size() <= 2);
      }
      // brak nakładania na rysunek budzika: karta z tekstem i fakty (M-01, AHI) kontra ten sam budzik na pustej karcie
      t.fakty = {{"8 h 00", "z maską"}, {"0,6", "AHI"}};
      Pamiec a, b;
      rysujKarte(a, n, t, "", "");  // bez sceny: sam tekst
      TrescEkranu pusta;
      pusta.kartaBezEtykiety = true;
      rysujKarte(b, n, pusta, sc, "");  // sama scena
      int naklada = 0, czarnych = 0;
      for (int y = 52; y < 430; y++)
        for (int x = 0; x < 800; x++) {
          if (a.czarny(x, y)) czarnych++;
          if (a.czarny(x, y) && b.czarny(x, y)) naklada++;
        }
      if (naklada) printf("  nakłada się na rysunek: %s %s (%d px)\n", sc.c_str(), war ? "warunek" : "neutralna", naklada);
      SPRAWDZ(naklada == 0 && czarnych > 500);
      // nic nie nad tytułem: w pasie 48…70 px (dawna etykieta 22 px) brak tekstu
      int nad = 0;
      for (int y = 49; y < 72; y++)
        for (int x = 0; x < 800; x++) nad += a.czarny(x, y);
      SPRAWDZ(nad == 0);
      // tekst nie wychodzi poza kolumnę: lewy margines 26 px, prawy 774 px
      int poza = 0;
      for (int y = 52; y < 430; y++)
        for (int x = 0; x < 800; x++)
          if (a.czarny(x, y) && (x < 24 || x > 776)) poza++;
      SPRAWDZ(poza == 0);
    }
  }
  if (!wyjatki.empty()) printf("  karty poza układem:%s\n", wyjatki.c_str());
  SPRAWDZ(p34 == 2);  // G12 z warunkiem i G05 neutralna: pierwsze zdanie dłuższe niż 2 linie SB44, tytuł w P34B
  (void)tk;
}

// ----------: krok zamienny przy braku danych ----------
static void testBrakDanych() {
  // brak danych (powody z rodzajów reguł)
  for (const char* p : {"za malo nocy 9/14", "brak nocy w rekordach", "brak historii", "brak normy", "brak odczytu",
                        "brak metryki M-13", "brak odpowiedzi", "rozruch: 9 nocy", "tryb aparatu nieznany",
                        "trend: co najmniej 2 noce"})
    SPRAWDZ(powodBrakuDanych(p));
  // awaria zestawu i powody nierozpoznane: dalej `Część reguł nie działa`
  for (const char* p : {"nieznany rodzaj xyz", "brak progu L-01.x w trybie CPAP", "brak parametru okno", "zle por ge",
                        "wymaga L-01: nie oceniam", "blad zestawu: zmienne", "", "okres infekcji (R-INF-02)"})
    SPRAWDZ(!powodBrakuDanych(p));
  // OcenaUspokojenia: tylko brak danych → bez awarii; jedna awaria → awaria
  std::vector<reguly::Wynik> w(2);
  for (auto& x : w) { x.poziom = "L"; x.oceniona = false; x.stan = reguly::StanReguly::AKTYWNA; }
  w[0].id = "L-01"; w[0].powodNieoceny = "za malo nocy 9/14";
  w[1].id = "L-02"; w[1].powodNieoceny = "brak normy M-03";
  OcenaUspokojenia o = ocenUspokojenie(w, {});
  SPRAWDZ(o.silnik && !o.wszystkieOcenione && !o.awaria);
  w[1].powodNieoceny = "nieznany rodzaj xyz";
  o = ocenUspokojenie(w, {});
  SPRAWDZ(o.awaria);
  // Stany: niespokoj DANE przy braku danych, REGULY przy awarii
  Stany st;
  st.uspokojenieOcenione = true;
  st.ocenaNiepelna = false;
  w[1].powodNieoceny = "brak normy M-03";
  zastosujUspokojenie(st, ocenUspokojenie(w, {}));
  SPRAWDZ(st.niespokoj == Stany::Niespokoj::DANE);
  Stany st2;
  st2.uspokojenieOcenione = true;
  st2.ocenaNiepelna = false;
  w[1].powodNieoceny = "nieznany rodzaj xyz";
  zastosujUspokojenie(st2, ocenUspokojenie(w, {}));
  SPRAWDZ(st2.niespokoj == Stany::Niespokoj::REGULY);
  // teksty kroków z zestawu
  ROWNE(sk::tekst("S-K-18.krok_dane"), "→ Część ocen jeszcze nie działa: za mało nocy.");
  ROWNE(sk::tekst("S-K-18.krok_reguly"), "→ Część reguł nie działa: zobacz Test sprzętu.");
  ROWNE(sk::tekst("S-K-18.krok_dane_wyciek"), "→ Bezdechów nie oceniam: noce z wyciekiem.");
}

// 5.4.13 pkt 7 i R-SKI-24: przyczyna braku ocen to wyciek (noce niepewne), nie liczba nocy
static void testBrakOcenZWycieku() {
  auto seria = [](int ile, float wp95, int krotkich = 0) {
    std::vector<Noc> h;
    for (int i = ile - 1; i >= 0; i--) h.push_back(noc(dodajDni(DZIS, -i), i < krotkich ? 200 : 420, 12.0f, wp95, i >= krotkich));
    return h;
  };
  {  // 14 nocy ważnych z P95 18 L/min: wszystkie niepewne → wyciek blokuje oceny, linia R-SKI-24
    Stany s = liczStany(seria(14, 18.0f), DZIS, {});
    SPRAWDZ(s.bezOcenZWycieku && s.nocyNieczytelne);
  }
  {  // 14 nocy ważnych z P95 9 L/min: noce pewne → ani jedno, ani drugie
    Stany s = liczStany(seria(14, 9.0f), DZIS, {});
    SPRAWDZ(!s.bezOcenZWycieku && !s.nocyNieczytelne);
  }
  {  // 5 nocy (nowy pacjent) z P95 18: za mało nocy ważnych w ogóle → przyczyną jest liczba nocy, nie wyciek;
     // R-SKI-24 liczy z dostępnych nocy z terapią: 5 niepewnych ≥ 4 → linia
    Stany s = liczStany(seria(5, 18.0f), DZIS, {});
    SPRAWDZ(!s.bezOcenZWycieku && s.nocyNieczytelne);
    Stany s3 = liczStany(seria(3, 18.0f), DZIS, {});  // 3 noce z terapią, wszystkie niepewne → brak linii
    SPRAWDZ(!s3.nocyNieczytelne);
  }
  {  // 14 nocy, w tym 5 krótkich (nieważnych) i reszta pewna: brak ocen z liczby nocy, nie z wycieku
    Stany s = liczStany(seria(14, 9.0f, 5), DZIS, {});
    SPRAWDZ(!s.bezOcenZWycieku && s.nocyNieczytelne);  // 5 z 7 ostatnich nieważnych → R-SKI-24
  }
  {  // 14 nocy ważnych, 5 niepewnych (P95 18) na początku okna: 9 pewnych < 10 → wyciek; z 7 ostatnich 0 niepewnych
    std::vector<Noc> h = seria(14, 9.0f);
    for (int i = 0; i < 5; i++) h[static_cast<size_t>(i)].wp95 = 18.0f;
    Stany s = liczStany(h, DZIS, {});
    SPRAWDZ(s.bezOcenZWycieku && !s.nocyNieczytelne);
    h[0].wp95 = 9.0f;  // 4 niepewne: 10 pewnych → brak blokady
    SPRAWDZ(!liczStany(h, DZIS, {}).bezOcenZWycieku);
  }
  {  // krok zamienny S-K-18: przy braku ocen z wycieku przyczyna „noce z wyciekiem”, inaczej „za mało nocy”
    std::vector<Noc> h = seria(7, 9.0f);
    sk::Wejscie w;
    w.dzis = DZIS;
    w.hist = &h;
    w.tylkoBrakDanych = true;
    sk::Tresc t;
    SPRAWDZ(sk::tydzien(w, t));
    ROWNE(t.t.krok, "→ Część ocen jeszcze nie działa: za mało nocy.");
    w.brakDanychWyciek = true;
    sk::Tresc t2;
    SPRAWDZ(sk::tydzien(w, t2));
    ROWNE(t2.t.krok, "→ Bezdechów nie oceniam: noce z wyciekiem.");
  }
}

int main(int argc, char** argv) {
  if (argc > 1) REPO = argv[1];
  testTekstyDoslownie();
  testWarunki();
  testNieOceniam();
  testWybor();
  testUklad();
  testBrakDanych();
  testBrakOcenZWycieku();
  printf("testy_przerywniki_s2d: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
