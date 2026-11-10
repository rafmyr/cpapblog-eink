// SPDX-License-Identifier: GPL-3.0-or-later
// Testy wykresu slotu dziennego przy linii uwagi (zwykłej i wzorca A, R-KON-63), U-01 i „Sprzętu” na wartościach
// granicznych, stopki karty bez zdania budzika i podpowiedzi „bez daty”. Każdy test, który pada, wypisuje swój temat.
// Uruchomienie: tools/prototyp/testy.sh (program testy_slot_ustawienia_sprzet).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "ekrany_kontekst.h"
#include "slot_dzienny.h"
#include "sprzet.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(nr, w)                                                               \
  do {                                                                               \
    if (w) ok++;                                                                     \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); } \
  } while (0)

struct PlotnoR : Plotno {
  std::vector<unsigned char> px = std::vector<unsigned char>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[y * 800 + x] = c;
  }
  bool cz(int x, int y) const { return x >= 0 && x < 800 && y >= 0 && y < 480 && px[y * 800 + x]; }
};

static const double KAT_LAT = 50.2597, KAT_LON = 19.0217;

static Prognoza prognoza(const std::string& dzis, float t, float pm) {
  Prognoza p;
  p.data = dzis;
  p.pobrano = 1;
  p.godzinaPobrania = "07:55";
  for (auto& g : p.h) {
    g.temp = t;
    g.pm25 = pm;
    g.opad = 0;
    g.chmury = 20;
    g.porywy = 20;
  }
  return p;
}
static WejscieSlotu wejscie(const std::string& dzis, int minuta, const Prognoza* p, float przyLozku) {
  WejscieSlotu w;
  w.dzis = dzis;
  w.minuta = minuta;
  w.prognoza = p;
  w.przyLozku = przyLozku;
  w.wspolrzedne = true;
  w.lat = KAT_LAT;
  w.lon = KAT_LON;
  w.obrazekJest = false;
  return w;
}

// Piksele, w których tekst ekranu (blok godziny, tytuł, powód, krok) i wizualizacja są czarne naraz, a podkład
// (nagłówek, linia uwagi, stopka) nie: tekst sam (ekran bez wizualizacji: SK34 przy linii uwagi), wizualizacja sama
// (teksty puste), podkład (oba puste). Margines `m` px: sąsiedztwo bliżej niż m px też liczy się jako styk.
static int nakladanie(const WyborSlotu& w, const TekstySlotu& t, const WejscieSlotu& we, int m, int* yMin = nullptr) {
  StopkaDzienna s = stopkaDzienna(nullptr, true, 0, NAN, 21);
  WyborSlotu bez = w;
  bez.ekran = EkranSlotu::SK34;  // przy linii uwagi SK34 nie ma wizualizacji
  TekstySlotu pusty;
  PlotnoR tekst, wiz, pod;
  rysujSlotDzienny(tekst, {"wt 03.11", ""}, bez, t, we, s);
  rysujSlotDzienny(wiz, {"wt 03.11", ""}, w, pusty, we, s);
  rysujSlotDzienny(pod, {"wt 03.11", ""}, bez, pusty, we, s);
  int n = 0, ym = 999;
  for (int y = 0; y < 434; y++)
    for (int x = 0; x < 800; x++) {
      if (!wiz.px[y * 800 + x] || pod.px[y * 800 + x]) continue;
      bool styk = false;
      for (int dy = -m; dy <= m && !styk; dy++)
        for (int dx = -m; dx <= m && !styk; dx++)
          if (tekst.cz(x + dx, y + dy) && !pod.cz(x + dx, y + dy)) styk = true;
      if (styk) { n++; ym = std::min(ym, y); }
    }
  if (yMin) *yMin = ym;
  return n;
}

// zapis ekranu do PBM (P1 tekstowy), do obejrzenia najgorszego przypadku
static void zapisz(const char* plik, const WyborSlotu& w, const TekstySlotu& t, const WejscieSlotu& we) {
  PlotnoR p;
  rysujSlotDzienny(p, {"wt 03.11", ""}, w, t, we, stopkaDzienna(nullptr, true, 0, NAN, 21));
  FILE* f = fopen(plik, "w");
  if (!f) return;
  fprintf(f, "P1\n800 480\n");
  for (int y = 0; y < 480; y++) {
    for (int x = 0; x < 800; x++) fputc(p.px[y * 800 + x] ? '1' : '0', f);
    fputc('\n', f);
  }
  fclose(f);
}

// ---------- R-KON-63, wykres przy linii uwagi wzorca A (60 px) nachodzi na blok godziny / powód ----------
static void testWykresPrzyLiniiA() {
  // S-K-33 z godziną (`wietrz do` / `wietrz od`): smog po południu, czysto rano
  int najS33 = 0, najS33b = 0, najLato = 0, najLatoB = 0, najS37 = 0, s33 = 0, lato = 0, s37 = 0;
  for (bool a : {false, true})
    for (int godz = 6; godz <= 18; godz++)
      for (float t : {-5.0f, 8.0f, 18.0f, 27.0f, 33.0f})
        for (float lozko : {18.0f, 21.0f, 26.0f, 30.0f}) {
          Prognoza p = prognoza("2026-11-03", t, 10);
          for (int h = godz + 3; h < 24; h++) p.h[h].pm25 = 90;
          WejscieSlotu we = wejscie("2026-11-03", godz * 60, &p, lozko);
          we.liniaUwagi = true;
          we.linia = a ? "Wciąż: Noce z AHI powyżej 5." : "Co było inaczej? ↻ 2 s";
          we.liniaA = a;
          we.liniaOstrzezenie = a;
          WyborSlotu w = wybierzSlot(we);
          TekstySlotu tt = tekstySlotu(w, we);
          bool zmiesci = !tt.godzina.empty() || zawin(F_P28B, tt.powod, 744).size() <= 1;
          if (!zmiesci) continue;
          if (w.ekran == EkranSlotu::SK33) {
            int n = nakladanie(w, tt, we, 0);
            if (w.okno.tryb == TrybWietrzenia::LATO) {
              lato++;
              if (a && n > najLato) { najLato = n; zapisz("/tmp/ux2-lato-A.pbm", w, tt, we); }
              if (!a && n > najLatoB) { najLatoB = n; zapisz("/tmp/ux2-lato-zwykla.pbm", w, tt, we); }
            }
            else {
              s33++;
              if (a && n > najS33) { najS33 = n; zapisz("/tmp/ux2-slupki-A.pbm", w, tt, we); }
              if (!a) najS33b = std::max(najS33b, n);
            }
          } else if (w.ekran == EkranSlotu::SK37) {
            s37++;
            int n = a ? nakladanie(w, tt, we, 0) : 0;
            if (n > najS37) { najS37 = n; zapisz("/tmp/ux2-s37-A.pbm", w, tt, we); }
          }
        }
  printf("  sprawdzone ekrany przy linii uwagi: S-K-33 słupki %d, S-K-33 linia temperatury %d, S-K-37 %d\n", s33, lato, s37);
  printf("  największe nakładanie (px): S-K-33 słupki linia A %d, linia zwykła %d; linia temperatury A %d, zwykła %d; "
         "S-K-37 A %d (najgorsze przypadki: /tmp/ux2-*.pbm)\n", najS33, najS33b, najLato, najLatoB, najS37);
  SPRAWDZ("S-K-33 słupki, linia A", najS33 == 0);
  SPRAWDZ("S-K-33 słupki, linia zwykła", najS33b == 0);
  SPRAWDZ("S-K-33 linia temperatury, linia A", najLato == 0);
  SPRAWDZ("S-K-33 linia temperatury, linia zwykła", najLatoB == 0);
  SPRAWDZ("S-K-37, linia A", najS37 == 0);
}

// ---------- S-K-37 przy linii A, ręcznie: godzina zachodu (B1) a znacznik i podpis `teraz` ----------
static void testS37LiniaARecznie() {
  for (int minuta : {7 * 60, 9 * 60, 13 * 60, 16 * 60, 19 * 60, 21 * 60}) {
    WejscieSlotu we = wejscie("2026-11-04", minuta, nullptr, 21);
    we.liniaUwagi = true;
    we.linia = "Wciąż: Noce z AHI powyżej 5.";
    we.liniaA = true;
    we.liniaOstrzezenie = true;
    WyborSlotu w;
    w.ekran = EkranSlotu::SK37;
    w.slonce = dzienINoc("2026-11-04", KAT_LAT, KAT_LON);
    TekstySlotu tt = tekstySlotu(w, we);
    int y = 0;
    int n = nakladanie(w, tt, we, 0, &y);
    if (n) printf("  S-K-37 linia A o %02d:%02d: %d px wspólnych (od y %d)\n", minuta / 60, minuta % 60, n, y);
    SPRAWDZ("S-K-37, linia A, teraz/pas a blok godziny", n == 0);
  }
}

// ---------- R-KON-63: gałąź „powód w 2 liniach” pod tytułem B3 (wykres ustępuje) ----------
static void testGalazDwochLinii() {
  // te same dane co testR_KON_63_wykresPrzyLinii() w testy_slot_dzienny.cpp
  Prognoza p = prognoza("2026-11-03", 1, 80);
  WejscieSlotu ws = wejscie("2026-11-03", 11 * 60, &p, 21);
  ws.liniaUwagi = true;
  ws.linia = "Wciąż: Bezdechy częstsze.";
  WyborSlotu w2 = wybierzSlot(ws);
  TekstySlotu t2 = tekstySlotu(w2, ws);
  bool sk33 = w2.ekran == EkranSlotu::SK33;
  // realne teksty S-K-33 tego wariantu nie dają (niżej), więc powód w 2 liniach budowany ręcznie, jak w testy_slot_dzienny.cpp; tu ta sama
  // treść i sprawdzenie, że wykres ustępuje
  TekstySlotu t3 = t2;
  t3.godzina.clear();
  t3.powod = "Smog za oknem przez cały dzień, a w sypialni powietrze jeszcze dość czyste i chłodne.";
  PlotnoR c;
  rysujSlotDzienny(c, {"wt 03.11", ""}, w2, t3, ws, stopkaDzienna(nullptr, true, 0, NAN, 21));
  int os = 0;
  for (int x = 28; x < 772; x++) os += c.px[352 * 800 + x];
  bool dwie = sk33 && zawin(F_P28B, t3.powod, 744).size() > 1 && os < 100;
  printf("  dane: ekran %s, godzina \"%s\", tytuł \"%s\", powód \"%s\" → gałąź „2 linie”: %s\n",
         sk33 ? "S-K-33" : "inny", t2.godzina.c_str(), t2.tytul.c_str(), t2.powod.c_str(), dwie ? "tak" : "nie");
  SPRAWDZ("gałąź „powód w 2 liniach”: wykres ustępuje", dwie);
  // czy jakikolwiek realny wariant S-K-33 / S-K-36 / S-K-37 przy linii uwagi ma tytuł B3 z powodem w 2 liniach
  int dwieLinie = 0, b3 = 0;
  for (int godz = 6; godz <= 21; godz++)
    for (float t : {-5.0f, 8.0f, 27.0f, 33.0f})
      for (float pm : {5.0f, 40.0f, 90.0f})
        for (float lozko : {18.0f, 26.0f}) {
          Prognoza q = prognoza("2026-11-03", t, pm);
          WejscieSlotu we = wejscie("2026-11-03", godz * 60, &q, lozko);
          we.liniaUwagi = true;
          we.linia = "Co było inaczej? ↻ 2 s";
          WyborSlotu w = wybierzSlot(we);
          if (w.ekran != EkranSlotu::SK33 && w.ekran != EkranSlotu::SK36 && w.ekran != EkranSlotu::SK37) continue;
          TekstySlotu tt = tekstySlotu(w, we);
          if (!tt.godzina.empty()) continue;
          b3++;
          if (zawin(F_P28B, tt.powod, 744).size() > 1) dwieLinie++;
        }
  printf("  warianty z tytułem B3 przy linii uwagi: %d, z powodem w 2 liniach: %d\n", b3, dwieLinie);
}

// ---------- U-01, wartości graniczne (żadna wartość nie wychodzi poza kolumnę, puste i dziwne dane) ----------
static void testU01Brzegowe() {
  DaneUstawien d;
  d.ssid = std::string(32, 'W');
  d.abAdres = "bardzo-dluga-nazwa-airbridge-w-sieci.local:65535";
  d.miasto = "Kędzierzyn-Koźle";
  d.wersja = "1.12.345-beta.7+g0123456789abcdef";
  d.ciszaOd = 23 * 60 + 59;
  d.ciszaDo = 0;
  std::vector<WierszUstawien> w = wierszeUstawien(d);
  for (auto& x : w) SPRAWDZ("wartość szersza niż kolumna", szer(F_P24B, x.wartosc) <= U01_SZER_WARTOSCI);
  // skracanie po znakach UTF-8: SSID z polskimi znakami nie może zostać ucięte w połowie znaku
  d.ssid = "ŻÓŁĆżółćŻÓŁĆżółćŻÓŁĆżółćŻÓŁĆżó";  // 32 znaki, 64 bajty (SSID ma do 32 BAJTÓW, ale test kroju)
  w = wierszeUstawien(d);
  const std::string& s = w[0].wartosc;
  bool utf8 = true;
  for (size_t i = 0; i < s.size();) {
    unsigned char c = (unsigned char)s[i];
    size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (!n || i + n > s.size()) { utf8 = false; break; }
    for (size_t k = 1; k < n; k++)
      if (((unsigned char)s[i + k] & 0xC0) != 0x80) utf8 = false;
    i += n;
  }
  SPRAWDZ("skrócenie SSID psuje UTF-8", utf8);
}

// ---------- „Sprzęt”: termin w dniu wymiany „dziś”, 29 lutego, przełom roku, zgodność z S-K-35 ----------
static void testSprzetTerminy() {
  sprzet::Elementy e;
  // maska wymieniona dziś (10.11.2026), okres 6 mies. → 10.05.2027 (inny rok, z rokiem); nie po terminie
  e.el[EL_MASKA] = {"2026-11-10", 6, false};
  e.el[EL_FILTR] = {"2026-12-03", 4, true};  // data w przyszłości (zegar albo pomyłka): wiek `–`
  auto w = sprzet::wierszeSprzetu(e, "2026-11-10");
  SPRAWDZ("wymiana dziś", w[0].termin == "10.05.2027 · co 6 mies." && !w[0].poTerminie);
  printf("  filtr z datą w przyszłości: wiek \"%s\", termin \"%s\"\n", w[1].wiek.c_str(), w[1].termin.c_str());
  // 29 lutego + 12 mies. → 28.02 (przycięte w terminWymiany); w dniu 28.02 już po terminie
  e.el[EL_MASKA] = {"2028-02-29", 12, false};
  w = sprzet::wierszeSprzetu(e, "2029-02-27");
  SPRAWDZ("29 lutego", w[0].termin == "28.02 · co 12 mies." && !w[0].poTerminie);
  w = sprzet::wierszeSprzetu(e, "2029-02-28");
  SPRAWDZ("29 lutego, dzień terminu", w[0].termin == "po 28.02 · co 12 mies." && w[0].poTerminie);
  // przełom roku: termin 05.01.2027 oglądany 31.12.2026 → z rokiem; 05.01.2027 oglądany 05.01.2027 → po, bez roku
  e.el[EL_FILTR] = {"2026-12-08", 4, true};
  w = sprzet::wierszeSprzetu(e, "2026-12-31");
  SPRAWDZ("przełom roku", w[1].termin == "05.01.2027 · co 4 tyg.");
  w = sprzet::wierszeSprzetu(e, "2027-01-05");
  SPRAWDZ("przełom roku, dzień terminu", w[1].termin == "po 05.01 · co 4 tyg.");
  // zgodność z S-K-35 i znacznikiem: ta sama funkcja terminWymiany i ten sam warunek poTerminie
  for (int d = 0; d < 400; d += 7) {
    std::string dzis = dodajDni("2026-06-01", d);
    e.el[EL_MASKA] = {"2026-01-31", 6, false};
    e.el[EL_FILTR] = {"2026-02-29", 4, true};  // zła data: bez daty
    auto ww = sprzet::wierszeSprzetu(e, dzis);
    for (int i = 0; i < EL_N; i++) {
      std::string t = terminWymiany(e.el[i]);
      SPRAWDZ("bez daty a termin", ww[(size_t)i].bezDaty == t.empty());
      SPRAWDZ("po terminie a negatyw", ww[(size_t)i].poTerminie == sprzet::poTerminie(e.el[i], dzis));
    }
  }
}

// ---------- karta bez zdania: lewa część stopki z akcją nie może ustąpić odczytowi czujnika ----------
static void testKartaStopkaAkcja() {
  TrescEkranu t;
  t.tytul = "Maska się zużywa.";
  t.stopkaLewo = "◀ 2 s: wymienione dziś";
  t.stopkaPrawo = "przy łóżku 35° · 100% · CO2 4999";
  PlotnoR p;
  rysujKarte(p, {"cz 30.11", "dane 05:46"}, t, "");
  int lewo = 0;
  for (int y = 444; y < 472; y++)
    for (int x = 28; x < 28 + szer(F_P22B, t.stopkaLewo); x++) lewo += p.px[y * 800 + x];
  // ten przypadek mieści się (sprawdzenie, że reguła ustępowania nie zjada akcji przy realnych długościach)
  SPRAWDZ("akcja ◀ w stopce karty ustąpiła", lewo > 50);
}

// ---------- podpowiedź „Brak daty” każe przytrzymać ◀ „na wierszu w ramce”, a ramka może stać na innym elemencie ----------
static void testPodpowiedzBezDaty() {
  // maska z datą (kursor na niej, wiersz 0), filtr bez daty: podpowiedź stoi pod tabelą niezależnie od kursora;
  // ◀ 2 s na wierszu w ramce zapisuje wymianę MASKI (R-MAS-01), nie elementu bez daty
  // po poprawce podpowiedź zależy od wierszy (podpowiedzBezDaty()): maska z datą, filtr bez daty
  sprzet::Elementy e;
  e.el[EL_MASKA] = {"2026-07-15", 6, false};
  e.el[EL_FILTR] = {"", 4, true};
  std::string pod = sprzet::podpowiedzBezDaty(sprzet::wierszeSprzetu(e, "2026-11-10"));
  bool wskazujeKursor = pod.find("w ramce") != std::string::npos;
  bool mowiOWierszuBezDaty = pod.find("bez daty") != std::string::npos || pod.find("Filtr") != std::string::npos;
  SPRAWDZ("podpowiedź wskazuje wiersz w ramce, nie wiersz bez daty", !wskazujeKursor || mowiOWierszuBezDaty);
}

int main() {
  testWykresPrzyLiniiA();
  testS37LiniaARecznie();
  testGalazDwochLinii();
  testU01Brzegowe();
  testSprzetTerminy();
  testKartaStopkaAkcja();
  testPodpowiedzBezDaty();
  printf("testy_slot_ustawienia_sprzet: %d dobrze, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
