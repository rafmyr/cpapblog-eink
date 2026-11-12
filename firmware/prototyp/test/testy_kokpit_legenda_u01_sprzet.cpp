// SPDX-License-Identifier: GPL-3.0-or-later
// Testy poprawek czytelności kokpitu i ekranów z nim związanych (10.11.2026): legenda `↔ stabilnie` w K1
// (R-KOK-17), lista do odczytu na U-01 (R-KONF-55), termin wymiany i nowy model maski w formularzu U-03
// (R-MAS-16, R-MAS-17, ADR-0041), `ocena częściowa` w nagłówku K1 (R-KOK-21), powód S-K-01
// bez powtórzonej liczby CO2, wizualizacja slotu dziennego przy linii uwagi i podpisy godzin osi (R-KON-63).
// Uruchomienie: tools/prototyp/testy.sh (program testy_kokpit_legenda_u01_sprzet).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "aktualizacja.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "formularz.h"
#include "stany.h"
#include "kontekst.h"
#include "kokpit.h"
#include "noc.h"
#include "sprzet.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                                     \
  do {                                                                                 \
    if (w) ok++;                                                                       \
    else { zle++; printf("BŁĄD (%s:%d): %s\n", __FILE__, __LINE__, #w); }              \
  } while (0)
#define ROWNE(a, b)                                                                                              \
  do {                                                                                                           \
    std::string _a = (a), _b = (b);                                                                              \
    if (_a == _b) ok++;                                                                                          \
    else { zle++; printf("BŁĄD (%s:%d): %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)

struct PlotnoTest : Plotno {
  std::vector<unsigned char> px = std::vector<unsigned char>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[y * 800 + x] = c;
  }
  int czarne(int x0, int y0, int x1, int y1) const {
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[y * 800 + x];
    return n;
  }
};

static const std::string DZIS = "2026-11-12";

static Noc noc(const std::string& d) {
  Noc n;
  n.data = d;
  n.m01 = 420;
  n.wazna = true;
  n.ahi = 1.0f;
  n.cmed = 9.0f;
  n.cp95 = 10.5f;
  n.wmed = 4.0f;
  n.wp95 = 10.0f;
  n.ustawienia = "AutoSet/5/13//";
  n.zrodlo = 'A';
  return n;
}
static std::vector<Noc> okno() {
  std::vector<Noc> h;
  for (int i = 0; i < 30; i++) h.push_back(noc(dodajDni(DZIS, i - 29)));
  return h;
}
static NormaMetryki nmJest(double med, double p10, double p90) {
  NormaMetryki m;
  m.n = 30;
  m.med = med;
  m.p10 = p10;
  m.p90 = p90;
  return m;
}
static Norma normaPelna() {
  Norma n;
  n.ahi = nmJest(1.0, 0.5, 1.5);
  n.m01 = nmJest(420, 380, 460);
  n.cmed = nmJest(9.0, 8.5, 9.5);
  n.cp95 = nmJest(10.5, 10, 11);
  n.wp95 = nmJest(10.0, 8.0, 12.0);
  return n;
}

// ---------- K1, słowo `stabilnie` raz, w legendzie (R-KOK-17) ----------
// wymaganie: R-KOK-17
static void testR_KOK_17_legenda() {
  ROWNE(slowoOcenyK1("stabilnie"), "↔");
  for (const char* s : {"rośnie", "spada", "uczę się", "wyżej niż zwykle", "niżej niż zwykle", ""}) ROWNE(slowoOcenyK1(s), s);
  ROWNE(LEGENDA_K1, "↔ stabilnie");
  // nagłówek kolumn: `ocena` od x 522 (bez odkrycia) i od 486 (z odkryciem O-12) nie dochodzi do legendy przy x 772
  SPRAWDZ(522 + szer(F_P22B, "ocena") + 16 <= 772 - szer(F_P22B, LEGENDA_K1));
  SPRAWDZ(486 + szer(F_P22B, "ocena") + 16 <= 772 - szer(F_P22B, LEGENDA_K1));
  // ocena w jednej linii (pas odkrycia): `↔ wyciek widoczny` od x 486 do 772
  SPRAWDZ(szer(F_P22B, "↔ wyciek widoczny") <= 286);
  // dane bez zmian: cztery wiersze `stabilnie` w DaneK1 (R-KOK-16), na ekranie znak `↔`
  std::vector<Noc> h = okno();
  DaneK1 d = budujK1(h, DZIS, normaPelna(), {});
  int stab = 0;
  for (int i = 0; i < d.wierszy; i++) stab += d.w[i].slowo == "stabilnie";
  SPRAWDZ(stab == 4);
  // render: legenda w nagłówku kolumn (y 156…188) z prawej, przy x 650…772 piksele czarne
  PlotnoTest p;
  rysujK1(p, d);
  SPRAWDZ(p.czarne(772 - szer(F_P22B, LEGENDA_K1), 160, 772, 186) > 0);
  // kolumna oceny w wierszu: sam znak `↔` jest węższy niż słowo `stabilnie`, więc pikseli mniej niż dawniej
  PlotnoTest q;
  rysujK1(q, d);
  // wiersze od y 182 co 50 px; tekst oceny w środku wiersza, bez kresek między wierszami (y + 49)
  for (int i = 0; i < 4; i++) {
    int y = 182 + 50 * i;
    SPRAWDZ(q.czarne(522, y + 5, 522 + szer(F_P22B, "↔") + 2, y + 44) > 0);       // znak `↔`
    SPRAWDZ(q.czarne(522 + szer(F_P22B, "↔") + 4, y + 5, 772, y + 44) == 0);  // bez słowa za nim
  }
}

// ---------- U-01 jako lista do odczytu (R-KONF-55) ----------
// wymaganie: R-KONF-55
static void testR_KONF_55_listaUstawien() {
  DaneUstawien d;
  std::vector<WierszUstawien> w = wierszeUstawien(d);
  SPRAWDZ(w.size() == 9);
  ROWNE(w[0].nazwa, "Sieć Wi-Fi");
  ROWNE(w[0].wartosc, "nie ustawiona");
  ROWNE(w[1].wartosc, "nie ustawiony");
  ROWNE(w[2].wartosc, "nie wybrano");
  ROWNE(w[3].wartosc, "prywatny");
  ROWNE(w[4].wartosc, "normalny");
  ROWNE(w[5].wartosc, "włączony, cisza 22:00–08:00");
  ROWNE(w[6].wartosc, "włączona");
  ROWNE(w[7].wartosc, "brak");
  ROWNE(w[8].nazwa, "Oprogramowanie");
  d.ssid = "Dom";
  d.abAdres = "airbridge.local:8080";
  d.miasto = "Gorzów Wielkopolski";
  d.profilPubliczny = true;
  d.humor = 0;
  d.dzwiek = false;
  d.podpowiedz = false;
  d.co2 = 0;
  d.wersja = "1.2.3";
  w = wierszeUstawien(d);
  ROWNE(w[0].wartosc, "Dom");
  ROWNE(w[1].wartosc, "airbridge.local:8080");
  ROWNE(w[2].wartosc, "Gorzów Wielkopolski");
  ROWNE(w[3].wartosc, "publiczny");
  ROWNE(w[4].wartosc, "wyłączony");
  ROWNE(w[5].wartosc, "wyłączony");
  ROWNE(w[6].wartosc, "wyłączona");
  ROWNE(w[7].wartosc, "jest, ekrany CO2 wyłączone");
  ROWNE(w[8].wartosc, "1.2.3");
  d.co2 = 1;
  d.humor = 1;
  d.dzwiek = true;
  d.ciszaOd = 23 * 60 + 30;
  d.ciszaDo = 7 * 60 + 5;
  w = wierszeUstawien(d);
  ROWNE(w[4].wartosc, "oszczędny");
  ROWNE(w[5].wartosc, "włączony, cisza 23:30–07:05");
  ROWNE(w[7].wartosc, "jest, ekrany CO2 włączone");
  // najdłuższe wartości: SSID 32 znaki i adres z nazwą .local i portem skrócone `…` do kolumny wartości
  d.ssid = "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
  d.abAdres = "bardzo-dluga-nazwa-airbridge-w-sieci.local:65535";
  w = wierszeUstawien(d);
  SPRAWDZ(w[0].wartosc.size() > 3 && w[0].wartosc.substr(w[0].wartosc.size() - 3) == "…");
  for (auto& x : w) {
    SPRAWDZ(szer(F_P24B, x.wartosc) <= U01_SZER_WARTOSCI);
    SPRAWDZ(28 + szer(F_P22B, x.nazwa) + 12 <= U01_X_WARTOSCI);  // nazwa nie wchodzi na wartość
  }
  // podpowiedź i zdanie granicy w 744 px; zdanie ADR-0002 bez zmian
  for (const char* t : {U01_PODPOWIEDZ[0], U01_PODPOWIEDZ[1], U01_GRANICA}) SPRAWDZ(szer(F_P22B, t) <= 744);
  ROWNE(U01_GRANICA, "Te ustawienia dotyczą tylko panelu, nie aparatu.");
  // żaden wiersz nie jest hasłem ani kontem: nazwy wierszy bez „hasło”, „konto”, „token”
  for (auto& x : w)
    SPRAWDZ(x.nazwa.find("asło") == std::string::npos && x.nazwa.find("onto") == std::string::npos &&
            x.nazwa.find("oken") == std::string::npos);
  // render: lista od y 64 do 316 ma treść, podpowiedź i granica nad stopką (434)
  PlotnoTest p;
  rysujUstawienia(p, d);
  SPRAWDZ(p.czarne(28, 64, 772, 316) > 0);
  SPRAWDZ(p.czarne(28, 340, 772, 422) > 0);
  SPRAWDZ(p.czarne(0, 422, 800, 433) == 0);  // nic nie dochodzi do linii stopki
}

// ---------- U-01: lista do odczytu razem z wierszem aktualizacji i stopką z ↻ 2 s ----------
// wymaganie: R-KONF-55
static void testR_KONF_55_wierszAktualizacjiPodLista() {
  // wszystkie warianty wiersza stanu oprogramowania mieszczą się w 744 px krojem linii pod listą (P22B)
  for (const std::string& t :
       {aktualizacja::wierszU01(true, "", "1.0.0-beta.1"), aktualizacja::wierszU01(false, "", "1.0.0"),
        aktualizacja::wierszU01(true, "1.0.1", "1.0.0"), aktualizacja::wierszU01(true, "1.12.3-rc.12", "1.0.0")}) {
    if (szer(F_P22B, t) > 744) printf("  U-01 wiersz aktualizacji %d px: %s\n", szer(F_P22B, t), t.c_str());
    SPRAWDZ(szer(F_P22B, t) <= 744);
  }
  // stopka z mapy 5.6.2 (↻ 2 s aktualizacja) w kolejności ◀ ▶ ↻
  std::vector<std::string> st = stopkaU01();
  SPRAWDZ(st.size() == 3 && st[2] == "↻ ekran dnia · 2 s aktualizacja");
  std::vector<std::string> u = st;
  uporzadkujPrzyciski(u);
  SPRAWDZ(u == st);
  // najdłuższe wartości listy i wiersz aktualizacji: lista, cztery linie pod nią i stopka bez nakładania
  DaneUstawien d;
  d.ssid = "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
  d.abAdres = "bardzo-dluga-nazwa-airbridge-w-sieci.local:65535";
  d.miasto = MIASTO_ZE_WSPOLRZEDNYCH;
  d.co2 = 0;
  d.wersja = "1.12.3-rc.12";
  d.aktualizacja = aktualizacja::wierszU01(false, "", "1.12.3-rc.12");
  PlotnoTest p, q;
  rysujUstawienia(p, d);
  SPRAWDZ(p.czarne(28, 316, 772, 320) == 0);   // odstęp między listą (ostatni wiersz 308) a linią aktualizacji
  SPRAWDZ(p.czarne(28, 320, 772, 344) > 0);    // linia aktualizacji (338)
  SPRAWDZ(p.czarne(0, 422, 800, 433) == 0);    // nic nie dochodzi do linii stopki
  SPRAWDZ(p.czarne(28, 440, 772, 475) > 0);    // stopka
  d.aktualizacja.clear();
  rysujUstawienia(q, d);
  SPRAWDZ(q.czarne(28, 320, 772, 344) == 0);   // bez wiersza aktualizacji pusto, reszta na swoich miejscach
  SPRAWDZ(q.czarne(28, 346, 772, 420) > 0);
  // żadna linia pod listą nie zawiera tokenu ani hasła (R-KONF-38)
  for (bool tok : {true, false})
    for (const std::string& z : {std::string(""), std::string("1.0.1")}) {
      std::string t = aktualizacja::wierszU01(tok, z, "1.0.0");
      SPRAWDZ(t.find("ghp_") == std::string::npos && t.find("asło") == std::string::npos);
    }
}

// ---------- termin następnej wymiany w formularzu U-03 (R-MAS-17, od ADR-0041 zamiast „Sprzętu”) ----------
// wymaganie: R-MAS-17
static void testR_MAS_17_terminWFormularzu() {
  // termin przy polu daty w formularzu U-03 (dawna kolumna `następna wymiana` strony „Sprzęt”, ADR-0041)
  sprzet::Elementy e;
  e.el[EL_MASKA] = {"2026-07-15", 6, false};  // termin 15.01.2027
  e.el[EL_FILTR] = {"2026-10-20", 4, true};   // termin 17.11.2026
  ROWNE(sprzet::terminFormularza(e.el[EL_MASKA], "2026-11-10"), "następna wymiana 15.01.2027");
  ROWNE(sprzet::terminFormularza(e.el[EL_FILTR], "2026-11-17"), "po terminie od 17.11.2026");  // w dniu terminu już po
  ROWNE(sprzet::terminFormularza(ElementWymiany{"", 4, true}, "2026-11-10"), "brak daty wymiany");  // R-MAS-06
  formularz::Pola f;
  f.termin[0] = sprzet::terminFormularza(e.el[EL_MASKA], "2026-11-10");
  f.termin[1] = sprzet::terminFormularza(ElementWymiany{"", 4, true}, "2026-11-10");
  std::string html = formularz::strona(f, "/u/x/", "", {});
  SPRAWDZ(html.find("następna wymiana 15.01.2027") != std::string::npos);
  SPRAWDZ(html.find("brak daty wymiany") != std::string::npos);
  // R-MAS-16: pole `Nowy model maski od dziś`, zawsze niezaznaczone
  SPRAWDZ(html.find("name=\"nowy_model\" value=\"1\">") != std::string::npos);
  SPRAWDZ(html.find("Nowy model maski od dziś") != std::string::npos);
  SPRAWDZ(html.find("nowy_model\" value=\"1\" checked") == std::string::npos);
  // bez terminów (zPliku bez wołającego) bez wiersza terminu
  SPRAWDZ(formularz::strona(formularz::Pola(), "/u/x/", "", {}).find("następna wymiana") == std::string::npos);
}

// wymaganie: R-MAS-16
static void testR_MAS_16_nowyModelWFormularzu() {
  sekrety::Dane s;
  formularz::Wynik w = formularz::waliduj({{"nowy_model", "1"}, {"wymiana.maska", "2026-01-01"}}, s, "2026-11-12");
  SPRAWDZ(w.ok() && w.nowyModelMaski);
  ROWNE(w.plik["wymiana.maska"], "2026-11-12");  // dziś wygrywa z polem daty
  SPRAWDZ(w.plik.count("wymiana.poduszka") && w.plik["wymiana.poduszka"].empty());
  w = formularz::waliduj({{"wymiana.maska", "2026-01-01"}}, s, "2026-11-12");
  SPRAWDZ(w.ok() && !w.nowyModelMaski);  // bez zaznaczenia norma M-07 bez zmian
  ROWNE(w.plik["wymiana.maska"], "2026-01-01");
  w = formularz::waliduj({{"nowy_model", "1"}}, s, "");  // bez daty panelu: odrzucone
  SPRAWDZ(!w.ok() && !w.nowyModelMaski && w.bledy.count("nowy_model"));
}

// wymaganie: R-KOK-24
// wariant b, decyzja właściciela projektu 11.11.2026: krok S8 `→ Pasmo AHI wysokie. Obserwuj kolejne noce.` w K2, gdy jest
// miejsce. Pomiar we wszystkich trybach przy pełnej tabeli (bez zwiniętych wierszy, R-KOK-18): CPAP i AutoSet mają
// linię pod legendą, ASV i ASVAuto nie (brak miejsca, wypisane px); pasmo poniżej progu → bez linii.
static void testR_KOK_24_krokPasmaWK2() {
  const char* const TRYBY[4] = {"CPAP/9///", "AutoSet/5/13//", "ASV/8///", "ASVAuto/6/10/3/15"};
  for (int t = 0; t < 4; t++) {
    std::vector<Noc> h = okno();
    for (auto& n : h) {
      n.ustawienia = TRYBY[t];
      n.oa = 0.4f, n.ca = 0.3f, n.h = 0.3f, n.emed = 7.0f, n.ep95 = 8.0f, n.psmed = 5.0f, n.psp95 = 7.0f;
      n.m13 = 14.0f, n.m17 = 1;
    }
    Norma nm = normaPelna();
    nm.ahi = nmJest(3.0, 1.2, 5.4);
    DaneK2 d = budujK2(h, DZIS, nm, {});
    bool asv = t >= 2;
    ROWNE(d.krokPasma, asv ? "" : "→ Pasmo AHI wysokie. Obserwuj kolejne noce.");
    bool zwin[DaneK2::WIERSZE] = {};
    SPRAWDZ(zwinieteK2(d, zwin).empty());  // pełna tabela: najciaśniejszy układ trybu
    PlotnoTest p;
    rysujK2(p, d);
    int dol = -1;  // najniższy czarny piksel treści nad kreską stopki (434)
    for (int y = 433; y >= 300 && dol < 0; y--)
      if (p.czarne(0, y, 800, y + 1)) dol = y;
    printf("  K2 %s: %d wierszy, treść do y %d, wolne nad stopką %d px%s\n", TRYBY[t], d.wierszy, dol, 433 - dol,
           d.krokPasma.empty() ? "" : " (z linią kroku)");
    SPRAWDZ(dol < 432);  // co najmniej 2 px nad kreską stopki
    if (!asv) {
      SPRAWDZ(szer(F_P22B, d.krokPasma) <= 744);
      PlotnoTest q;
      DaneK2 bez = d;
      bez.krokPasma.clear();
      rysujK2(q, bez);
      SPRAWDZ(p.czarne(0, 400, 800, 432) > q.czarne(0, 400, 800, 432));  // linia jest narysowana
    } else {
      SPRAWDZ(433 - dol < 26);  // w ASV bez miejsca na linię P22B (26 px): dlatego bez kroku
    }
    nm.ahi = nmJest(3.0, 1.2, 4.9);  // poniżej progu R-EKR-36.p90_ahi (5 / h)
    SPRAWDZ(budujK2(h, DZIS, nm, {}).krokPasma.empty());
    nm.ahi = NormaMetryki();  // bez normy AHI (`uczę się`)
    SPRAWDZ(budujK2(h, DZIS, nm, {}).krokPasma.empty());
  }
}

// wymaganie: R-KOK-24
// wariant c, decyzja właściciela projektu 11.11.2026: przy wysokim paśmie AHI, gdy pełnej linii pod legendą nie ma (ASV,
// ASVAuto) albo się nie mieści, krótki tekst `→ Pasmo AHI wysokie.` w linii legendy z prawej (wolne x 422–772). Cztery
// tryby, pełna tabela i tabela z linią `brak danych:` (R-KOK-18): krótki tekst nie nachodzi na legendę (≥ 26 px
// odstępu) ani na stopkę (nic poniżej y 432), mieści się do prawego marginesu 772; w CPAP i AutoSet z miejscem pod
// legendą jest tylko pełna linia.
static void testR_KOK_24_krotkiWLegendzie() {
  const char* const TRYBY[4] = {"CPAP/9///", "AutoSet/5/13//", "ASV/8///", "ASVAuto/6/10/3/15"};
  SPRAWDZ(szer(F_P22B, "→ Pasmo AHI wysokie.") <= 772 - 422);
  for (int t = 0; t < 4; t++)
    for (int brak = 0; brak < 2; brak++) {
      std::vector<Noc> h = okno();
      for (auto& n : h) {
        n.ustawienia = TRYBY[t];
        n.oa = 0.4f, n.ca = 0.3f, n.h = 0.3f, n.emed = 7.0f, n.ep95 = 8.0f, n.psmed = 5.0f, n.psp95 = 7.0f;
        n.m13 = 14.0f, n.m17 = 1;
      }
      Norma nm = normaPelna();
      nm.ahi = nmJest(3.0, 1.2, 5.4);
      DaneK2 d = budujK2(h, DZIS, nm, {});
      bool asv = t >= 2;
      ROWNE(d.krokPasmaKrotki, "→ Pasmo AHI wysokie.");
      if (brak) {  // ostatni wiersz bez danych: zwinięty do linii `brak danych:` nad legendą (R-KOK-18)
        WierszK2& w = d.w[d.wierszy - 1];
        w.pusty = true;
        if (!*w.nazwaBraku) w.nazwaBraku = "wiersz";
      }
      bool zwin[DaneK2::WIERSZE] = {};
      SPRAWDZ(zwinieteK2(d, zwin).empty() == !brak);
      PlotnoTest p, q;
      rysujK2(p, d);
      DaneK2 bez = d;
      bez.krokPasmaKrotki.clear();
      rysujK2(q, bez);
      int x0 = 800, x1 = -1, y0 = 480, y1 = -1;  // piksele krótkiego tekstu = różnica rysunków
      for (int y = 0; y < 480; y++)
        for (int x = 0; x < 800; x++)
          if (p.px[y * 800 + x] != q.px[y * 800 + x]) x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
      bool krotki = x1 >= 0;
      printf("  K2 %s%s: %s\n", TRYBY[t], brak ? " z `brak danych:`" : "",
             krotki ? ("krótki tekst x " + std::to_string(x0) + "–" + std::to_string(x1) + ", y " + std::to_string(y0) + "–" +
                       std::to_string(y1)).c_str()
                    : "pełna linia pod legendą");
      SPRAWDZ(krotki == asv);  // CPAP i AutoSet: miejsce pod legendą jest także z linią `brak danych:`
      if (!krotki) {
        SPRAWDZ(!d.krokPasma.empty());
        continue;
      }
      SPRAWDZ(x0 >= 422 && x1 <= 772 && y1 < 432);
      int leg = -1;  // legenda w tych samych wierszach: najdalszy czarny piksel na lewo od krótkiego tekstu
      for (int y = y0; y <= y1; y++)
        for (int x = 0; x < x0; x++)
          if (q.px[y * 800 + x]) leg = std::max(leg, x);
      SPRAWDZ(leg > 300 && x0 - leg > 26);  // to linia legendy (`poza twoją normą` kończy się za x 300), odstęp > 26 px
      // bez wysokiego pasma: bez krótkiego tekstu
      nm.ahi = nmJest(3.0, 1.2, 4.9);
      SPRAWDZ(budujK2(h, DZIS, nm, {}).krokPasmaKrotki.empty());
    }
}

// wymaganie: R-KOK-21
static void testR_KOK_21_ocenaWNaglowkuK1() {
  Stany st;
  st.ocenaNiepelna = true;
  ROWNE(ocenaNaglowkaK1(st), "ocena częściowa");
  st.ostatniaNiepewna = true;  // obie przyczyny: silniejsza (fail-path R-KOK-21)
  ROWNE(ocenaNaglowkaK1(st), "ocena ograniczona");
  st.ocenaNiepelna = false;
  ROWNE(ocenaNaglowkaK1(st), "ocena ograniczona");
  st.ostatniaNiepewna = false;
  ROWNE(ocenaNaglowkaK1(st), "");
  ROWNE(ocenaNaglowkaK1(nullptr), "ocena częściowa");  // bez stanów nic nie jest ocenione
  // stany bety z liczStany(): punkty warunku uspokojenia nieoceniane → częściowa
  std::vector<Noc> h = okno();
  Stany sb = liczStany(h, DZIS, {});
  SPRAWDZ(sb.ocenaNiepelna);
  DaneK1 d = budujK1(h, DZIS, normaPelna(), {}, &sb);
  ROWNE(d.ocena, sb.ostatniaNiepewna ? "ocena ograniczona" : "ocena częściowa");
  ROWNE(naglowekK1Prawo(d), d.ocena + " · 1 / 3");
  d.ocena.clear();
  ROWNE(naglowekK1Prawo(d), "1 / 3");
  // najdłuższy nagłówek mieści się w 744 px z odstępem
  SPRAWDZ(szer(F_P24B, "Ostatnia noc · śr 30.12 ◆") + 16 + szer(F_P22B, "ocena ograniczona · 1 / 3") <= 744);
  // render: prawa część nagłówka zaczyna się dalej w lewo przy dopisku (piksele w x 500…640 tylko z dopiskiem)
  d.ocena = "ocena ograniczona";
  PlotnoTest a, b;
  rysujK1(a, d);
  d.ocena.clear();
  rysujK1(b, d);
  int x0 = 772 - szer(F_P22B, "ocena ograniczona · 1 / 3");
  SPRAWDZ(a.czarne(x0, 14, x0 + 60, 44) > 0 && b.czarne(x0, 14, x0 + 60, 44) == 0);
}

// ---------- S-K-01 bez trzykrotnej liczby CO2 (5.8.4) ----------
// wymaganie: R-KON-01
static void testR_KON_01_powodBezLiczby() {
  WejscieWieczoru w;
  w.dzis = DZIS;
  w.co2Teraz = 1250;
  WynikWieczoru r;
  r.ekran = EkranWieczoru::SK01;
  r.zOdczytu = true;
  r.wartosc = 1250;
  TrescKontekstu k = trescSK01(w, r);
  ROWNE(k.t.powod, "Powietrze w sypialni jest teraz zastałe.");
  SPRAWDZ(k.t.powod.find("1250") == std::string::npos);
  // liczba zostaje w fakcie karty (`1250` `ppm teraz`); nagłówek `sypialnia {hh:mm} · CO2 1250` składa dyrygent
  SPRAWDZ(!k.t.fakty.empty() && k.t.fakty[0].w == "1250" && k.t.fakty[0].p == "ppm teraz");
  // powód mieści się w 2 liniach kolumny tekstu karty (474 px) i w 1 linii ekranu danych (744 px, P28B)
  SPRAWDZ(zawin(F_P22B, k.t.powod, 474).size() <= 2);
  SPRAWDZ(szer(F_P28B, k.t.powod) <= 744);
  SPRAWDZ(kartaMiesci(k.t));
  // wariant z zeszłej nocy bez zmian (liczba godzin, nie ppm)
  r.zOdczytu = false;
  r.wartosc = 180;
  ROWNE(trescSK01(w, r).t.powod, "Zeszłej nocy CO2 przez 3\xC2\xA0h ponad 1000\xC2\xA0ppm.");
}

int main() {
  testR_KOK_17_legenda();
  testR_KONF_55_listaUstawien();
  testR_KONF_55_wierszAktualizacjiPodLista();
  testR_MAS_17_terminWFormularzu();
  testR_MAS_16_nowyModelWFormularzu();
  testR_KOK_21_ocenaWNaglowkuK1();
  testR_KOK_24_krokPasmaWK2();
  testR_KOK_24_krotkiWLegendzie();
  testR_KON_01_powodBezLiczby();
  printf("testy_kokpit_legenda_u01_sprzet: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
