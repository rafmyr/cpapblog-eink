// SPDX-License-Identifier: GPL-3.0-or-later
// Testy poprawek czytelności kokpitu i ekranów z nim związanych (10.11.2026): legenda `↔ stabilnie` w K1
// (R-KOK-17), lista do odczytu na U-01 (R-KONF-55), termin wymiany w „Sprzęt” (R-MAS-05, R-MAS-15), powód S-K-01
// bez powtórzonej liczby CO2, wizualizacja slotu dziennego przy linii uwagi i podpisy godzin osi (R-KON-63).
// Uruchomienie: tools/prototyp/testy.sh (program testy_kokpit_legenda_u01_sprzet).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "aktualizacja.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
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

// ---------- „Sprzęt”, termin następnej wymiany i podpis ◀ (R-MAS-15) ----------
// wymaganie: R-MAS-15
static void testR_MAS_15_terminWymiany() {
  sprzet::Elementy e;
  e.el[EL_MASKA] = {"2026-07-15", 6, false};  // termin 15.01.2027: inny rok niż dziś
  e.el[EL_FILTR] = {"2026-10-20", 4, true};   // termin 17.11.2026
  std::vector<sprzet::WierszSprzetu> w = sprzet::wierszeSprzetu(e, "2026-11-10");
  ROWNE(w[0].termin, "15.01.2027 · co 6 mies.");
  ROWNE(w[1].termin, "17.11 · co 4 tyg.");
  SPRAWDZ(!w[0].bezDaty && !w[1].bezDaty && !w[0].poTerminie && !w[1].poTerminie);
  // okres i wiek zostają w danych (R-MAS-05)
  ROWNE(w[0].okres, "6 mies.");
  ROWNE(w[0].wiek, "3 mies.");
  // po terminie: `po {data}` w negatywie; w dniu terminu już po terminie (jak sprzet::poTerminie)
  w = sprzet::wierszeSprzetu(e, "2026-11-17");
  ROWNE(w[1].termin, "po 17.11 · co 4 tyg.");
  SPRAWDZ(w[1].poTerminie);
  e.el[EL_MASKA] = {"2025-03-01", 6, false};  // termin 01.09.2025, rok inny niż dziś
  w = sprzet::wierszeSprzetu(e, "2026-11-10");
  ROWNE(w[0].termin, "po 01.09.2025 · co 6 mies.");
  // bez daty: wiek `–`, termin `brak daty · co …`, podpowiedź pod tabelą (R-MAS-06: nie po terminie)
  e.el[EL_FILTR] = {"", 4, true};
  w = sprzet::wierszeSprzetu(e, "2026-11-10");
  ROWNE(w[1].wiek, "–");
  ROWNE(w[1].termin, "brak daty · co 4 tyg.");
  SPRAWDZ(w[1].bezDaty && !w[1].poTerminie);
  // podpowiedź nazywa element bez daty (ramka może stać na innym wierszu, a ◀ 2 s zapisuje element pod ramką)
  ROWNE(sprzet::podpowiedzBezDaty(w), "Filtr bez daty: po wymianie ramka na filtrze, ◀ 2 s.");
  SPRAWDZ(szer(F_P22B, sprzet::podpowiedzBezDaty(w)) <= 744);
  e.el[EL_MASKA] = {"", 6, true};
  std::vector<sprzet::WierszSprzetu> oba = sprzet::wierszeSprzetu(e, "2026-11-10");
  ROWNE(sprzet::podpowiedzBezDaty(oba), "Maska i filtr bez daty: po wymianie ramka na wierszu, ◀ 2 s.");
  SPRAWDZ(szer(F_P22B, sprzet::podpowiedzBezDaty(oba)) <= 744);
  e.el[EL_FILTR] = {"2026-10-20", 4, false};
  ROWNE(sprzet::podpowiedzBezDaty(sprzet::wierszeSprzetu(e, "2026-11-10")), "Maska bez daty: po wymianie ramka na masce, ◀ 2 s.");
  e.el[EL_MASKA] = {"2025-03-01", 6, false};
  ROWNE(sprzet::podpowiedzBezDaty(sprzet::wierszeSprzetu(e, "2026-11-10")), "");
  e.el[EL_FILTR] = {"", 4, true};
  // najdłuższe wartości w kolumnie od x 300 do 772 (472 px), nagłówek kolumny też
  for (const char* t : {"po 31.12.2025 · co 12 mies.", "po 31.12.2025 · co 104 tyg.", "brak daty · co 104 tyg.",
                        "31.12.2027 · co 104 tyg."})
    SPRAWDZ(szer(F_P24S, t) <= 472);
  SPRAWDZ(szer(F_P22B, wersaliki("następna wymiana"), 2) <= 472);
  SPRAWDZ(180 + szer(F_P24S, "104 tyg.") + 12 <= 300);  // wiek nie wchodzi na termin
  // stopka: ◀ na pierwszym wierszu podpisany celem; obie wersje w 744 px
  ROWNE(sprzet::stopkaSprzetu(EL_MASKA), "◀ norma · 2 s wymienione   ▶ niżej   ↻ ekran dnia · 2 s nowy model");
  ROWNE(sprzet::stopkaSprzetu(EL_MASKA, true), "◀ liczby · 2 s wymienione   ▶ niżej   ↻ ekran dnia · 2 s nowy model");
  ROWNE(sprzet::stopkaSprzetu(EL_FILTR, true), "◀ wyżej · 2 s wymienione   ▶ ustawienia   ↻ ekran dnia");
  for (int k = 0; k < EL_N; k++)
    for (bool pub : {false, true}) {
      std::string st = sprzet::stopkaSprzetu(k, pub);
      std::vector<std::string> g;
      for (size_t a = 0; a < st.size();) {
        size_t b = st.find("   ", a);
        g.push_back(st.substr(a, b == std::string::npos ? std::string::npos : b - a));
        a = b == std::string::npos ? st.size() : b + 3;
      }
      SPRAWDZ(szerStopki(g) <= 744);
    }
  // render: element bez daty → podpowiedź pod tabelą (y 196…226); oba z datą → pusto
  PlotnoTest p, q;
  sprzet::rysujSprzet(p, w, 0);
  SPRAWDZ(p.czarne(28, 200, 772, 232) > 0);
  e.el[EL_FILTR] = {"2026-10-20", 4, true};
  sprzet::rysujSprzet(q, sprzet::wierszeSprzetu(e, "2026-11-10"), 0);
  SPRAWDZ(q.czarne(28, 200, 772, 232) == 0);
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
  testR_MAS_15_terminWymiany();
  testR_KON_01_powodBezLiczby();
  printf("testy_kokpit_legenda_u01_sprzet: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
