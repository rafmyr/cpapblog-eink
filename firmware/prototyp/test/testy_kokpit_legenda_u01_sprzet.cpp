// SPDX-License-Identifier: GPL-3.0-or-later
// Testy poprawek czytelności kokpitu i ekranów z nim związanych (10.11.2026; K1 od 13.11.2026: znak i słowo w każdym wierszu, bez legendy)
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

// ---------- K1, znak i słowo w każdym wierszu oceny, bez legendy (R-KOK-17 od 13.11.2026) ----------
// wymaganie: R-KOK-17
static void testR_KOK_17_znakISlowo() {
  ROWNE(znakOcenyK1("stabilnie"), "↔");
  ROWNE(znakOcenyK1("rośnie"), "↑");
  ROWNE(znakOcenyK1("wyżej niż zwykle"), "↑");
  ROWNE(znakOcenyK1("spada"), "↓");
  ROWNE(znakOcenyK1("niżej niż zwykle"), "↓");
  ROWNE(znakOcenyK1("uczę się"), "…");
  ROWNE(znakOcenyK1(""), "");
  WierszK1 w;
  w.slowo = "stabilnie";
  ROWNE(tekstOcenyK1(w), "↔ stabilnie");
  w.slowo = "uczę się";
  ROWNE(tekstOcenyK1(w), "… uczę się");
  w.slowo = "";
  ROWNE(tekstOcenyK1(w), "");  // metryka bez Δ: kolumna pusta, bez samotnego znaku
  // dane bez zmian: cztery wiersze `stabilnie` w DaneK1 (R-KOK-16)
  std::vector<Noc> h = okno();
  DaneK1 d = budujK1(h, DZIS, normaPelna(), {});
  int stab = 0;
  for (int i = 0; i < d.wierszy; i++) stab += d.w[i].slowo == "stabilnie";
  SPRAWDZ(stab == 4);
  // render: w każdym wierszu znak w kolumnie znaku i słowo za nim; w nagłówku kolumn nie ma już legendy z prawej
  PlotnoTest p;
  rysujK1(p, d);
  const int xo = K1_XM + K1_ODSTEP_OCENY, wc = szer(F_P22B, "↔ ");
  SPRAWDZ(p.czarne(xo + szer(F_P22B, "ocena") + 8, 160, 772, 181) == 0);  // za słowem `ocena` pusto
  for (int i = 0; i < 4; i++) {  // wiersze od y 182 co 50 px
    int y = 182 + 50 * i;
    SPRAWDZ(p.czarne(xo, y + 5, xo + wc, y + 44) > 0);                                        // znak `↔`
    SPRAWDZ(p.czarne(xo + wc, y + 5, xo + wc + szer(F_P22B, "stabilnie") + 2, y + 44) > 0);   // słowo
    SPRAWDZ(p.czarne(xo + wc + szer(F_P22B, "stabilnie") + 4, y + 5, 772, y + 44) == 0);      // nic dalej
  }
}

// ---------- K1, dopisek `lepiej` / `gorzej` przy trendzie AHI i wycieku (R-KOK-25) ----------
// wymaganie: R-KOK-25
static void testR_KOK_25_dopisek() {
  ROWNE(dopisekTrenduK1("AHI", "spada"), "lepiej");
  ROWNE(dopisekTrenduK1("AHI", "rośnie"), "gorzej");
  ROWNE(dopisekTrenduK1("Wyciek", "spada"), "lepiej");
  ROWNE(dopisekTrenduK1("Wyciek", "rośnie"), "gorzej");
  for (const char* s : {"stabilnie", "uczę się", "wyżej niż zwykle", "niżej niż zwykle", ""}) {
    ROWNE(dopisekTrenduK1("AHI", s), "");
    ROWNE(dopisekTrenduK1("Wyciek", s), "");
  }
  // wiersze, w których kierunek nie ma jednego znaczenia: bez dopisku
  for (const char* n : {"Czas z maską", "Ciśnienie", "EPAP", "Wsparcie"})
    for (const char* s : {"rośnie", "spada"}) ROWNE(dopisekTrenduK1(n, s), "");
  WierszK1 w;
  w.slowo = "spada";
  w.dopisek = "lepiej";
  ROWNE(tekstOcenyK1(w), "↓ spada · lepiej");
  // z danych: wyciek rośnie w 7 ostatnich nocach → `gorzej`; AHI stabilnie → bez dopisku
  std::vector<Noc> h = okno();
  for (int i = 0; i < 30; i++) h[i].wp95 = i < 23 ? 6.0f : 12.0f;
  DaneK1 d = budujK1(h, DZIS, normaPelna(), {});
  for (int i = 0; i < d.wierszy; i++) {
    if (std::string(d.w[i].nazwa) == "Wyciek") {
      ROWNE(d.w[i].slowo, "rośnie");
      ROWNE(d.w[i].dopisek, "gorzej");
    } else {
      ROWNE(d.w[i].dopisek, "");
    }
  }
  for (int i = 0; i < 30; i++) h[i].wp95 = i < 23 ? 12.0f : 6.0f;
  d = budujK1(h, DZIS, normaPelna(), {});
  for (int i = 0; i < d.wierszy; i++)
    if (std::string(d.w[i].nazwa) == "Wyciek") ROWNE(tekstOcenyK1(d.w[i]), "↓ spada · lepiej");
  // stan odchylenia ma pierwszeństwo (R-KOK-03): bez dopisku, także gdy trend by go dał
  Stany st;
  st.wp95.aktywny = true;
  st.wp95.wyzej = true;
  st.wp95.nocy = 9;
  d = budujK1(h, DZIS, normaPelna(), {}, &st);
  for (int i = 0; i < d.wierszy; i++)
    if (std::string(d.w[i].nazwa) == "Wyciek") {
      ROWNE(d.w[i].dopisek, "");
      ROWNE(tekstOcenyK1(d.w[i]), "↑ wyżej niż zwykle");
    }
  // fail-path (ADR-0029 pkt 7): AHI `spada` policzone bez nocy niepewnych, a 3 ostatnie noce niepewne z wysokim AHI:
  // panel nie mówi `lepiej` (tych nocy nie ocenia); `gorzej` przy nocy niepewnej zostaje
  {
    std::vector<Noc> hn = okno();
    for (int i = 0; i < 30; i++) hn[i].ahi = i < 23 ? 4.0f : 1.0f;
    DaneK1 dn = budujK1(hn, DZIS, normaPelna(), {});
    for (int i = 0; i < dn.wierszy; i++)
      if (std::string(dn.w[i].nazwa) == "AHI") ROWNE(tekstOcenyK1(dn.w[i]), "↓ spada · lepiej");  // kontrola: noce pewne
    for (int i = 27; i < 30; i++) {
      hn[i].ahi = 9.0f;
      hn[i].wp95 = 30.0f;
    }
    dn = budujK1(hn, DZIS, normaPelna(), {});
    for (int i = 0; i < dn.wierszy; i++)
      if (std::string(dn.w[i].nazwa) == "AHI") {
        ROWNE(dn.w[i].slowo, "spada");
        ROWNE(dn.w[i].dopisek, "");
      }
    // jedna noc niepewna 5 nocy temu (w oknie 7 nocy) też zdejmuje `lepiej`; 8 nocy temu już nie
    hn = okno();
    for (int i = 0; i < 30; i++) hn[i].ahi = i < 23 ? 4.0f : 1.0f;
    hn[24].wp95 = 30.0f;
    dn = budujK1(hn, DZIS, normaPelna(), {});
    for (int i = 0; i < dn.wierszy; i++)
      if (std::string(dn.w[i].nazwa) == "AHI") ROWNE(dn.w[i].dopisek, "");
    hn[24].wp95 = hn[23].wp95;
    hn[21].wp95 = 30.0f;
    dn = budujK1(hn, DZIS, normaPelna(), {});
    for (int i = 0; i < dn.wierszy; i++)
      if (std::string(dn.w[i].nazwa) == "AHI") ROWNE(dn.w[i].dopisek, "lepiej");
    // `gorzej` zostaje także z nocą niepewną w oknie
    hn = okno();
    for (int i = 0; i < 30; i++) hn[i].ahi = i < 23 ? 1.0f : 4.0f;
    hn[29].wp95 = 30.0f;
    dn = budujK1(hn, DZIS, normaPelna(), {});
    for (int i = 0; i < dn.wierszy; i++)
      if (std::string(dn.w[i].nazwa) == "AHI" && dn.w[i].slowo == "rośnie") ROWNE(dn.w[i].dopisek, "gorzej");
  }
  // bez normy: `uczę się`, bez dopisku (panel nie ocenia tego, czego się uczy)
  Norma bez = normaPelna();
  bez.wp95 = NormaMetryki();
  d = budujK1(h, DZIS, bez, {});
  for (int i = 0; i < d.wierszy; i++)
    if (std::string(d.w[i].nazwa) == "Wyciek") ROWNE(d.w[i].dopisek, "");
}

// ---------- K1, godzina danych w nagłówku (R-KOK-26) ----------
// wymaganie: R-KOK-26
static void testR_KOK_26_godzinaDanych() {
  std::vector<Noc> h = okno();
  DaneK1 d = budujK1(h, DZIS, normaPelna(), {});
  SPRAWDZ(d.ubiegla);
  const std::string baza = d.naglowek;
  ROWNE(naglowekK1Lewo(d), baza);  // godzina nieznana: bez dopisku, bez samego słowa `dane`
  d.godzinaDanych = "07:12";
  ROWNE(naglowekK1Lewo(d), baza + " · dane 07:12");
  d.romb = true;
  ROWNE(naglowekK1Lewo(d), baza + " · dane 07:12 ◆");
  // ostatnia zapisana noc nie jest ubiegłą (R-KOK-04): `Noc {data}` bez godziny dzisiejszego odczytu
  DaneK1 s = budujK1(h, dodajDni(DZIS, 2), normaPelna(), {});
  SPRAWDZ(!s.ubiegla);
  s.godzinaDanych = "07:12";
  SPRAWDZ(naglowekK1Lewo(s).find("dane") == std::string::npos);
  // brak nocy: sam tytuł
  DaneK1 e = budujK1({}, DZIS, normaPelna(), {});
  e.godzinaDanych = "07:12";
  ROWNE(naglowekK1Lewo(e), "Ostatnia noc");
  // najdłuższy nagłówek z godziną, rombem i oceną mieści się w 744 px z odstępem 24 px
  // najszerszy z 7 skrótów dni i cyfr (`wt 05.05`, `05:55`), nie tylko przykład ze spec
  for (const char* n : {"Ostatnia noc · śr 30.12 · dane 07:12 ◆", "Ostatnia noc · wt 05.05 · dane 05:55 ◆",
                        "Ostatnia noc · cz 28.08 · dane 08:08 ◆", "Ostatnia noc · nd 20.06 · dane 06:00 ◆"})
    SPRAWDZ(szer(F_P24B, n) + 24 + szer(F_P22B, "ocena ograniczona · 1 / 3") <= 744);
}

// ---------- K1, przyczyna oceny ograniczonej przy liczbie wycieku (R-KOK-27) ----------
// wymaganie: R-KOK-27
static void testR_KOK_27_duzyWyciek() {
  std::vector<Noc> h = okno();
  DaneK1 d = budujK1(h, DZIS, normaPelna(), {});
  ROWNE(d.opisy[3], "wyciek l/min");  // noc pewna
  h.back().wp95 = 16.0f;  // noc niepewna od 15 L/min (4.9)
  SPRAWDZ(nocNiepewna(h.back()));
  d = budujK1(h, DZIS, normaPelna(), {});
  ROWNE(d.opisy[3], OPIS_DUZY_WYCIEK);
  ROWNE(d.liczby[3], "16");  // liczba zostaje
  h.back().wp95 = 14.9f;
  d = budujK1(h, DZIS, normaPelna(), {});
  ROWNE(d.opisy[3], "wyciek l/min");
  // opis mieści się w kolumnie liczby wycieku (608…772)
  SPRAWDZ(szer(F_P22B, OPIS_DUZY_WYCIEK) <= 772 - 608);
}

// ---------- K1, tabela 30 nocy na całą szerokość treści (R-KOK-28) ----------
// wymaganie: R-KOK-28
static void testR_KOK_28_uklad() {
  const int xo = K1_XM + K1_ODSTEP_OCENY, wc = szer(F_P22B, "↔ "), miejsce = 772 - xo - wc;
  // każda pierwsza i druga linia oceny mieści się za znakiem do prawego marginesu
  for (const char* s : {"stabilnie", "rośnie · gorzej", "spada · lepiej", "rośnie", "spada", "uczę się",
                        "wyżej niż zwykle", "niżej niż zwykle", "od 999 nocy", "wyciek widoczny"}) {
    if (szer(F_P22B, s) > miejsce) printf("  za szerokie: %s (%d > %d)\n", s, szer(F_P22B, s), miejsce);
    SPRAWDZ(szer(F_P22B, s) <= miejsce);
  }
  // linia 30 nocy kończy się przed najszerszą medianą (`10:59`, P28B) z odstępem co najmniej 8 px
  SPRAWDZ(K1_X_LINII + K1_WL + 8 <= K1_XM - szer(F_P28B, "10:59"));
  SPRAWDZ(K1_WL >= 260);  // szersza niż dawne 210 px
  // nagłówki kolumn nie nachodzą na siebie
  SPRAWDZ(28 + szer(F_P22B, "Ostatnie 30 nocy") + 16 <= K1_XM - szer(F_P22B, "mediana"));
  // z pasem odkrycia: ocena w jednej linii, kandydaci po kolei aż do mieszczącego się
  const int m1 = 772 - (K1_XM_ODKRYCIE + K1_ODSTEP_OCENY);
  WierszK1 w;
  w.slowo = "spada";
  w.dopisek = "lepiej";
  w.slowo2 = "wyciek widoczny";
  ROWNE(ocenaK1JednaLinia(w, m1), "↓ spada · wyciek widoczny");  // pełna za szeroka: bez dopisku
  ROWNE(ocenaK1JednaLinia(w, 1000), "↓ spada · lepiej · wyciek widoczny");
  w = WierszK1();
  w.slowo = "stabilnie";
  w.slowo2 = "wyciek widoczny";
  ROWNE(ocenaK1JednaLinia(w, m1), "↔ wyciek widoczny");  // pełna za szeroka o kilkanaście px
  ROWNE(ocenaK1JednaLinia(w, 1000), "↔ stabilnie · wyciek widoczny");
  w = WierszK1();
  w.slowo = "wyżej niż zwykle";
  w.slowo2 = "od 3 nocy";
  SPRAWDZ(szer(F_P22B, ocenaK1JednaLinia(w, m1)) <= m1);
  // stan od wielu nocy: pełny tekst (298 px) nie mieści się, więc krótsze słowo ze znakiem; nigdy szerzej niż miejsce
  for (const char* od : {"od 99 nocy", "od 120 nocy", "od 999 nocy"})
    for (const char* sl : {"wyżej niż zwykle", "niżej niż zwykle"}) {
      w.slowo = sl;
      w.slowo2 = od;
      SPRAWDZ(szer(F_P22B, ocenaK1JednaLinia(w, m1)) <= m1);
    }
  w.slowo = "wyżej niż zwykle";
  w.slowo2 = "od 120 nocy";
  ROWNE(ocenaK1JednaLinia(w, m1), "↑ wyżej od 120 nocy");
  w.slowo2 = "od 3 nocy";
  ROWNE(ocenaK1JednaLinia(w, m1), "wyżej niż zwykle od 3 nocy");  // krótki stan bez zmian
  // każda kombinacja słowa, dopisku i drugiej linii mieści się w miejscu
  for (const char* sl : {"stabilnie", "rośnie", "spada", "uczę się"})
    for (const char* dp : {"", "lepiej", "gorzej"})
      for (const char* s2 : {"", "wyciek widoczny"}) {
        WierszK1 x;
        x.slowo = sl;
        x.dopisek = dp;
        x.slowo2 = s2;
        SPRAWDZ(szer(F_P22B, ocenaK1JednaLinia(x, m1)) <= m1);
      }
  w.slowo = "uczę się";
  w.slowo2 = "";
  ROWNE(ocenaK1JednaLinia(w, m1), "… uczę się");
  // render prawdziwego K1: prawa część wiersza nie jest pusta (dawniej 772 − 522 px z samym znakiem)
  std::vector<Noc> h = okno();
  PlotnoTest p;
  rysujK1(p, budujK1(h, DZIS, normaPelna(), {}));
  SPRAWDZ(p.czarne(420, 190, K1_X_LINII + K1_WL, 230) > 0);  // linia 30 nocy sięga za dawne 406 px
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
  SPRAWDZ(szer(F_P24B, "Ostatnia noc · śr 30.12 · dane 07:12 ◆") + 16 + szer(F_P22B, "ocena ograniczona · 1 / 3") <= 744);
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
  testR_KOK_17_znakISlowo();
  testR_KOK_25_dopisek();
  testR_KOK_26_godzinaDanych();
  testR_KOK_27_duzyWyciek();
  testR_KOK_28_uklad();
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
