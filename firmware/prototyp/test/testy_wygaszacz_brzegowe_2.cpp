// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe wygaszacza ekranu, wariant W3 (decyzja właściciela projektu 16.11.2026 22:08; R-KON-73…R-KON-77,
// R-KON-69, R-EKR-61). Część czysta (wybierzSlot): pełna tabela wariantów S-K-33 i ekranów
// slotu wobec list właściciela, kombinacje, stara baza sprzed całej zmiany (stan sprzed zmiany, bez bloku
// obrazka) uruchomiona na nowym kodzie bez wygaszacza. Część na dyrygencie z atrapami: przejścia `Wietrz od` →
// `Wietrz do` → koniec okna, okno zimowe „przesuwane” po szczycie temperatury, bateria spadająca w trakcie wygaszacza,
// informacja po aktualizacji pojawiająca się w trakcie wygaszacza, energia doby, dziennik obrazków, piksele stopki.
// Atrapa i pomocnicze funkcje z test/testy_slot_dzienny_dyrygent.cpp (dołączony z przemianowanym main).
// Uruchomienie: tools/prototyp/testy.sh (program testy_wygaszacz_brzegowe_2).
#include <functional>
int glowna_slotu_dyrygenta_rec2();
#define main glowna_slotu_dyrygenta_rec2
#include "testy_slot_dzienny_dyrygent.cpp"
#undef main
#include "aktualizacja.h"
#include "bateria.h"
#include "ekrany.h"
#include "ekrany_kontekst.h"
#include "sekrety.h"
#include "slot_dzienny.h"

// ---------- stara baza: wybierzSlot() z stan sprzed zmiany (test/baza/wybierz_slot_przed_adr0045.inc),
// jedyna zmiana: usunięty blok obrazka R-KON-50 (pole `slotOd` już nie istnieje; blok wycofany decyzją właściciela) ----
namespace stara {
WyborSlotu wybierzSlot(const WejscieSlotu& w) {
  WyborSlotu r;
  int godz = w.minuta >= 0 ? w.minuta / 60 : -1;
  if (wyciekDrzemki(w.drzemka)) {
    r.ekran = EkranSlotu::SK39;
    return r;
  }
  if (w.prognoza && w.prognoza->jest()) {
    WejscieWietrzenia ww;
    ww.prognoza = w.prognoza;
    ww.dzis = w.dzis;
    ww.godzina = godz;
    ww.przyLozku = w.przyLozku;
    ww.co2 = w.co2;
    ww.pylki = w.pylki;
    r.okno = regulaWietrzenia(ww);
    r.jutro = jutroRano(*w.prognoza, w.dzis);
    r.pylek = pylekDnia(*w.prognoza, w.dzis, godz, w.pylki);
  }
  bool okno = r.okno.wariant != Wietrzenie::BRAK;
  // (blok obrazka R-KON-50 usunięty)
  bool jutro = r.jutro.wariant != Jutro::BRAK;
  bool oknoBliskie = r.okno.wariant == Wietrzenie::TRWA || r.okno.wariant == Wietrzenie::CO2 ||
                     (r.okno.wariant == Wietrzenie::POZNIEJ && r.okno.od - godz <= 1);
  if (jutro && okno && godz >= JUTRO_PRZED_OKNEM_OD && !oknoBliskie) { r.ekran = EkranSlotu::SK34; return r; }
  if (okno) { r.ekran = EkranSlotu::SK33; return r; }
  if (jutro) { r.ekran = EkranSlotu::SK34; return r; }
  if (w.sprzet) {
    r.wymiana = wymianaSprzetu(*w.sprzet, w.dzis, w.ostatnieSK35);
    if (r.wymiana.element >= 0) { r.ekran = EkranSlotu::SK35; return r; }
  }
  if (r.pylek.rodzaj >= 0) { r.ekran = EkranSlotu::SK36; return r; }
  if (w.wspolrzedne) r.slonce = dzienINoc(w.dzis, w.lat, w.lon);
  bool sk37 = r.slonce.jest, sk38 = w.obrazekJest && !w.liniaUwagi;
  r.ekran = sk37 ? EkranSlotu::SK37 : sk38 ? EkranSlotu::SK38 : EkranSlotu::BRAK;
  return r;
}
}  // namespace stara

static const char* nazwaWariantu(Wietrzenie w) {
  switch (w) {
    case Wietrzenie::BRAK: return "brak";
    case Wietrzenie::CO2: return "Przewietrz teraz";
    case Wietrzenie::TRWA: return "Wietrz do";
    case Wietrzenie::POZNIEJ: return "Wietrz od";
    case Wietrzenie::UPAL_DO: return "Okna zamknięte do";
    case Wietrzenie::UPAL_CALY_DZIEN: return "Okna zamknięte cały dzień";
    case Wietrzenie::SMOG: return "Dziś bez wietrzenia";
  }
  return "?";
}
static std::string opisEkranu(const WyborSlotu& w) {
  std::string s = w.ekran == EkranSlotu::BRAK ? std::string("rano") : idEkranuSlotu(w.ekran);
  if (w.ekran == EkranSlotu::SK33) s += std::string(" ") + nazwaWariantu(w.okno.wariant);
  return s;
}
// Listy właściciela z 22:08: 1 = zasłania, 2 = na wierzchu. Upał w obu wersjach: na wierzchu (zasłanianie
// dosłowne; w pierwszej wersji tego testu 0 = poza listami).
static int listaWlasciciela(const WyborSlotu& e0) {
  switch (e0.ekran) {
    case EkranSlotu::SK33:
      switch (e0.okno.wariant) {
        case Wietrzenie::POZNIEJ: case Wietrzenie::SMOG: return 1;
        case Wietrzenie::CO2: case Wietrzenie::TRWA: return 2;
        default: return 2;  // upał w obu wersjach
      }
    case EkranSlotu::SK34: case EkranSlotu::SK36: case EkranSlotu::SK37: case EkranSlotu::SK38: case EkranSlotu::BRAK:
      return 1;  // SK38 tu: zastępstwo D5; BRAK: spokojny ekran z rana (W4)
    case EkranSlotu::SK35: case EkranSlotu::SK39: return 2;
  }
  return 0;
}

static const std::string DR = "2026-11-03";
static Prognoza prognozaR(float t, float pm) {
  Prognoza p;
  p.data = DR;
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
struct Los {
  uint32_t s;
  int operator()(int n) {
    s = s * 1103515245u + 12345u;
    return (int)((s >> 16) % (unsigned)n);
  }
};
// Losowe wejście slotu (jak testR_KON_70_zgodnoscZBaza(), szerszy zakres temperatur i przy łóżku, żeby padały
// wszystkie warianty S-K-33, także upał w obu wersjach i tryb letni).
static WejscieSlotu losoweWejscie(Los& r, Prognoza& p, ElementWymiany (&el)[EL_N]) {
  int profil = r(3);
  p = prognozaR((float)(r(45) - 10), (float)r(80));
  for (int h = 0; h < PROG_GODZIN; h++) {
    auto& g = p.h[(size_t)h];
    if (profil == 0) {  // losowo co godzinę
      if (r(3) == 0) g.temp = (float)(r(45) - 10);
      if (r(3) == 0) g.pm25 = (float)r(80);
    } else if (profil == 1) {  // szczyt temperatury w środku dnia, smog rano albo wieczorem
      int szczyt = 10 + r(8);
      g.temp = (float)(r(20) - 5 + 0) - (float)std::abs(h % 24 - szczyt);
      g.pm25 = (h % 24 < 6 + r(10) || h % 24 > 14 + r(9)) ? (float)(20 + r(60)) : (float)r(30);
    } else {  // ciepło (lato, upał)
      g.temp = (float)(18 + r(18)) - (float)std::abs(h % 24 - 15) * 0.7f;
      g.pm25 = (float)r(50);
    }
    g.opad = (float)r(100);
    g.porywy = (float)r(90);
    g.pylek[PYL_BRZOZA] = (float)r(300);
  }
  for (auto& e : el) e = ElementWymiany();
  if (r(2)) el[EL_FILTR] = {r(2) ? "2026-09-01" : "2026-10-28", 4, true};
  if (r(2)) el[EL_MASKA] = {r(2) ? "2026-04-01" : "2026-10-01", 6, false};
  WejscieSlotu w;
  w.dzis = DR;
  w.minuta = 6 * 60 + r(17 * 60);
  w.prognoza = r(6) ? &p : nullptr;
  w.przyLozku = r(8) ? (float)(15 + r(18)) : NAN;
  w.co2 = r(3) ? -1 : 400 + r(1600);
  if (r(2)) {
    w.pylki.zaznaczony[PYL_BRZOZA] = true;
    w.pylki.prog[PYL_BRZOZA] = 100;
  }
  w.sprzet = r(4) ? &el : nullptr;
  w.ostatnieSK35 = r(3) ? "" : "2026-11-02";
  w.wspolrzedne = r(4) != 0;
  w.lat = 50.2597;
  w.lon = 19.0217;
  w.obrazekJest = r(4) != 0;
  w.liniaUwagi = r(5) == 0;
  if (w.liniaUwagi) w.linia = "Co było inaczej? ↻ 2 s";
  if (r(12) == 0) {
    w.drzemka.minut = 40;
    w.drzemka.wyciekP95 = (float)r(50);
  }
  return w;
}
static bool rowneWybory(const WyborSlotu& a, const WyborSlotu& b) {
  return a.ekran == b.ekran && a.okno.wariant == b.okno.wariant && a.jutro.wariant == b.jutro.wariant &&
         a.wymiana.element == b.wymiana.element && a.pylek.rodzaj == b.pylek.rodzaj && a.slonce.jest == b.slonce.jest;
}

// (f) Stara baza z stanu sprzed zmiany (bez bloku obrazka) na nowym kodzie bez wygaszacza: wybór slotu identyczny.
// Dodatkowo nowy kod z wygaszaczem wyłączonym nigdy nie ustawia `wygaszacz`.
// wymaganie: R-KON-73
static void testRec2_staraBazaBezWygaszacza() {
  Los r{20261116u};
  int roznic = 0;
  const int N = 60000;
  for (int i = 0; i < N; i++) {
    Prognoza p;
    ElementWymiany el[EL_N];
    WejscieSlotu w = losoweWejscie(r, p, el);
    w.bezczynnosc = false;
    WyborSlotu a = wybierzSlot(w), b = stara::wybierzSlot(w);
    if (!rowneWybory(a, b) || a.wygaszacz) {
      if (roznic++ < 5) printf("  rec2 baza: różnica %d: %s zamiast %s\n", i, opisEkranu(a).c_str(), opisEkranu(b).c_str());
    }
  }
  printf("  rec2 stara baza (bez bloku obrazka) vs nowy kod bez wygaszacza: %d przypadków, %d różnic\n", N, roznic);
  SPRAWDZ(roznic == 0);
}

// (a) Pełna tabela: ekran bez wygaszacza (e0, stara baza) × wariant S-K-33 → wynik po progu bezczynności.
// Twarde: (1) ekran z listy 2 właściciela (S-K-39, `Przewietrz teraz.`, `Wietrz do`, S-K-35) i upał nigdy nie znika
// pod wygaszaczem ani nie zmienia się na inny; (2) wygaszacz staje tylko w miejscu ekranu z listy 1; (3) bez obrazka
// (brak pliku albo linia uwagi) wynik jak bez wygaszacza; (4) od decyzji o zasłanianiu dosłownym
// zero odstępstw od dosłownej listy: wynik to e0 albo S-K-38, nigdy trzeci ekran.
// Zmiana 2026-11-16: w pierwszej wersji odstępstwa tylko wypisywano, bo kod
// sięgał po następnego niezasłoniętego kandydata (ok. 19% przypadków z obrazkiem); teraz są twardym błędem.
// wymaganie: R-KON-75
static void testRec2_pelnaTabela() {
  Los r{777u};
  std::map<std::string, int> pokrycie, odstepstwa;
  int naruszenWierzchu = 0, wygNieTam = 0, bezObrazka = 0, trzeci = 0;
  const int N = 120000;
  for (int i = 0; i < N; i++) {
    Prognoza p;
    ElementWymiany el[EL_N];
    WejscieSlotu w = losoweWejscie(r, p, el);
    w.bezczynnosc = false;
    WyborSlotu e0 = stara::wybierzSlot(w);
    w.bezczynnosc = true;
    WyborSlotu e1 = wybierzSlot(w);
    const bool obrazek = w.obrazekJest && !w.liniaUwagi;
    const int lista = listaWlasciciela(e0);
    pokrycie[opisEkranu(e0) + (obrazek ? " | obrazek" : " | bez obrazka")]++;
    if (!obrazek && !rowneWybory(e0, e1)) bezObrazka++;
    if (lista != 1 && !rowneWybory(e0, e1)) {
      if (naruszenWierzchu++ < 5)
        printf("  rec2 NA WIERZCHU zmienione: %s → %s (%02d:%02d)\n", opisEkranu(e0).c_str(), opisEkranu(e1).c_str(),
               w.minuta / 60, w.minuta % 60);
    }
    if (e1.wygaszacz && e1.ekran == EkranSlotu::SK38 && lista != 1) wygNieTam++;
    // dosłowna lista właściciela: ekran z listy 1 przy obrazku → wygaszacz; reszta bez zmian
    EkranSlotu wlasc = (obrazek && lista == 1) ? EkranSlotu::SK38 : e0.ekran;
    if (e1.ekran != wlasc) odstepstwa[opisEkranu(e0) + " → " + opisEkranu(e1)]++;
    // własność „ten sam ekran albo S-K-38”
    if (!rowneWybory(e0, e1) && e1.ekran != EkranSlotu::SK38) trzeci++;
    if (e1.ekran == EkranSlotu::SK38 && e0.ekran != EkranSlotu::SK38 && e1.wymiana.element >= 0) trzeci++;
  }
  printf("  rec2 pełna tabela, %d losowań; pokrycie (ekran bez wygaszacza | obrazek):\n", N);
  for (auto& k : pokrycie) printf("    %6d  %s\n", k.second, k.first.c_str());
  printf("  rec2 odstępstwa od dosłownej listy właściciela (powinno być 0):\n");
  int odst = 0;
  for (auto& k : odstepstwa) {
    printf("    %6d  %s\n", k.second, k.first.c_str());
    odst += k.second;
  }
  printf("  rec2 razem odstępstw %d; zmienionych ekranów z wierzchu %d; wygaszacz nie w miejscu listy 1: %d; bez obrazka "
         "inny wynik: %d; trzeci ekran (ani e0, ani S-K-38) albo oceniona wymiana pod wygaszaczem: %d\n", odst,
         naruszenWierzchu, wygNieTam, bezObrazka, trzeci);
  SPRAWDZ(odst == 0);
  SPRAWDZ(trzeci == 0);
  SPRAWDZ(naruszenWierzchu == 0);
  SPRAWDZ(wygNieTam == 0);
  SPRAWDZ(bezObrazka == 0);
}

// Wejście z prognozą zimową: przy łóżku 21°, czysto (PM2.5 10), temperatura z profilu `t(h)`.
static WejscieSlotu wejscieZimowe(Prognoza& p, int minuta, const std::function<float(int)>& t,
                                  const std::function<float(int)>& pm = [](int) { return 10.0f; }) {
  p = prognozaR(5, 10);
  for (int h = 0; h < PROG_GODZIN; h++) {
    p.h[(size_t)h].temp = t(h % 24);
    p.h[(size_t)h].pm25 = pm(h % 24);
  }
  WejscieSlotu w;
  w.dzis = DR;
  w.minuta = minuta;
  w.prognoza = &p;
  w.przyLozku = 21;
  w.wspolrzedne = true;
  w.lat = 50.2597;
  w.lon = 19.0217;
  w.obrazekJest = true;
  w.bezczynnosc = true;
  return w;
}

// (b) kombinacje: kilka zasłoniętych naraz, zasłonięty D1 + niezasłonięty D3, linia uwagi przy zasłoniętym ekranie,
// drzemka z wyciekiem przy wygaszaczu, kolejność z telefonu.
// wymaganie: R-KON-75
static void testRec2_kombinacje() {
  Prognoza p;
  // szczyt 9° o 14:00, rośnie 1°/h: okno [12, 17) (pasmo 2°); o 09:00 `Wietrz od 12:00`
  auto szczyt14 = [](int h) { return h <= 14 ? (float)(h - 5) : (float)(9 - (h - 14)); };
  WejscieSlotu w = wejscieZimowe(p, 9 * 60, szczyt14);
  WyborSlotu a = wybierzSlot(w);
  printf("  rec2 09:00 `Wietrz od` po progu: %s (wygaszacz %d)\n", opisEkranu(a).c_str(), a.wygaszacz);
  SPRAWDZ(a.ekran == EkranSlotu::SK38 && a.wygaszacz);
  w.bezczynnosc = false;
  SPRAWDZ(wybierzSlot(w).ekran == EkranSlotu::SK33 && wybierzSlot(w).okno.wariant == Wietrzenie::POZNIEJ);
  // zasłonięty D1 + S-K-35 niżej (D3). Pierwsza wersja testu dokumentowała S-K-35 (następny kandydat);
  // od zasłaniania dosłownego wygaszacz, a S-K-35 nie jest oceniany
  ElementWymiany el[EL_N];
  el[EL_FILTR] = {"2026-09-01", 4, true};
  w.sprzet = &el;
  w.bezczynnosc = false;
  WyborSlotu bez = wybierzSlot(w);
  w.bezczynnosc = true;
  WyborSlotu zw = wybierzSlot(w);
  printf("  rec2 `Wietrz od` + S-K-35 niżej: bez wygaszacza %s, po progu %s\n", opisEkranu(bez).c_str(), opisEkranu(zw).c_str());
  SPRAWDZ(bez.ekran == EkranSlotu::SK33 && zw.ekran == EkranSlotu::SK38 && zw.wygaszacz);
  SPRAWDZ(zw.wymiana.element < 0);
  // linia uwagi przy zasłanianym ekranie: bez obrazka (R-KON-54), więc `Wietrz od` z linią
  w.sprzet = nullptr;
  w.liniaUwagi = true;
  w.linia = "Co było inaczej? ↻ 2 s";
  WyborSlotu l = wybierzSlot(w);
  SPRAWDZ(l.ekran == EkranSlotu::SK33 && !l.wygaszacz);
  w.liniaUwagi = false;
  w.linia.clear();
  // drzemka z wyciekiem przy wygaszaczu: S-K-39
  w.drzemka.minut = 60;
  w.drzemka.wyciekP95 = 40;
  SPRAWDZ(wybierzSlot(w).ekran == EkranSlotu::SK39 && !wybierzSlot(w).wygaszacz);
  w.drzemka = Drzemka();
  // kolejność z telefonu: pyłek pierwszy, potem wymiana; pyłek zasłonięty, wymiana bez daty → wygaszacz
  w.uklad.kolejnosc[0] = RodzajDzienny::PYLEK;
  w.uklad.kolejnosc[1] = RodzajDzienny::WYMIANA;
  w.uklad.kolejnosc[2] = RodzajDzienny::WIETRZENIE;
  w.uklad.kolejnosc[3] = RodzajDzienny::JUTRO_RANO;
  SPRAWDZ(wybierzSlot(w).ekran == EkranSlotu::SK38);
  // `obrazek` ukryty z telefonu: bez wygaszacza, `Wietrz od` zostaje
  w.uklad = UkladDnia();
  w.uklad.pokaz[(int)RodzajDzienny::OBRAZEK] = false;
  WyborSlotu u = wybierzSlot(w);
  SPRAWDZ(u.ekran == EkranSlotu::SK33 && !u.wygaszacz);
}

// (c, czysto) Okno zimowe po szczycie temperatury: reguła 5.8.10 liczy okno od „najcieplejszej czystej godziny
// z pozostałych”, więc po końcu prawdziwego okna (12:00–17:00, szczyt 9° o 14:00) przy spadającej temperaturze
// wychodzi nowe okno zaczynające się „teraz” (`Wietrz do …`) aż do końca horyzontu. Pod W3 `Wietrz do` jest na
// wierzchu, więc w zimowe popołudnie bez smogu wygaszacz nie wraca do końca slotu. Test dokumentuje (bez porażki),
// ile godzin od 12:00 do 20:00 stoi `Wietrz do` i ile wygaszacz.
// wymaganie: R-KON-75
static void testRec2_oknoZimowePrzesuwane() {
  auto szczyt14 = [](int h) { return h <= 14 ? (float)(h - 5) : (float)(9 - (h - 14)); };
  int wietrzDo = 0, wyg = 0;
  std::string przebieg;
  for (int g = 9; g <= 20; g++) {
    Prognoza p;
    WejscieSlotu w = wejscieZimowe(p, g * 60, szczyt14);
    WyborSlotu a = wybierzSlot(w);
    char b[96];
    snprintf(b, sizeof b, " %02d:00 %s%s;", g, opisEkranu(a).c_str(),
             a.ekran == EkranSlotu::SK33 && a.okno.do_ > 0 ? (" " + std::to_string(a.okno.do_) + ":00").c_str() : "");
    przebieg += b;
    if (g >= 12 && a.ekran == EkranSlotu::SK33 && a.okno.wariant == Wietrzenie::TRWA) wietrzDo++;
    if (g >= 12 && a.ekran == EkranSlotu::SK38) wyg++;
  }
  printf("  rec2 zimowy dzień, szczyt 9° o 14:00, czysto, próg minął:%s\n", przebieg.c_str());
  printf("  rec2 12:00–20:00: `Wietrz do` %d h, wygaszacz %d h\n", wietrzDo, wyg);
  SPRAWDZ(wietrzDo >= 1);  // okno trwa: na wierzchu (lista 2)
}

// (e) R-KON-46 po 16:00 przy upale: bez wygaszacza S-K-34 przed `Okna zamknięte do 20:00`. Pierwsza wersja testu
// dokumentowała upał wychodzący na wierzch po zasłonięciu S-K-34 (ekran, którego bez wygaszacza by nie było).
// Od zasłaniania dosłownego: wygaszacz w miejscu S-K-34.
// wymaganie: R-KON-75
static void testRec2_upalPo16() {
  Prognoza p = prognozaR(28, 10);
  for (int h = 0; h < PROG_GODZIN; h++) {
    if (h % 24 >= 20) p.h[(size_t)h].temp = 20;   // od 20:00 chłodniej o ≥ 2° niż przy łóżku: okno letnie
    if (h >= 24 + 6 && h < 24 + 9) p.h[(size_t)h].temp = -2;  // jutro rano mróz: S-K-34
  }
  WejscieSlotu w;
  w.dzis = DR;
  w.minuta = 17 * 60;
  w.prognoza = &p;
  w.przyLozku = 26;
  w.wspolrzedne = true;
  w.lat = 50.2597;
  w.lon = 19.0217;
  w.obrazekJest = true;
  WyborSlotu bez = wybierzSlot(w);
  w.bezczynnosc = true;
  WyborSlotu zw = wybierzSlot(w);
  printf("  rec2 17:00 upał + jutro mróz: bez wygaszacza %s, po progu %s\n", opisEkranu(bez).c_str(), opisEkranu(zw).c_str());
  SPRAWDZ(bez.ekran == EkranSlotu::SK34);
  SPRAWDZ(zw.ekran == EkranSlotu::SK38 && zw.wygaszacz);
  // ten sam upał przed 16:00 (bez S-K-34 przed oknem): upał na wierzchu
  w.minuta = 12 * 60;
  WyborSlotu poludnie = wybierzSlot(w);
  printf("  rec2 12:00 upał + jutro mróz, po progu: %s\n", opisEkranu(poludnie).c_str());
  SPRAWDZ(poludnie.ekran == EkranSlotu::SK33 && poludnie.okno.wariant == Wietrzenie::UPAL_DO && !poludnie.wygaszacz);
}

// ---------- (g) piksele stopki obrazka: ikona nowej wersji + niska bateria + wynik nocy ----------
struct PlotnoR2 : Plotno {
  std::vector<uint8_t> b = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool cz) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) b[(size_t)y * 800 + x] = cz;
  }
  int czarnych(int x0, int y0, int x1, int y1) const {
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += b[(size_t)y * 800 + x];
    return n;
  }
};
// Obrazek 800×434 cały czarny (najgorszy przypadek dla krawędzi stopki).
static ObrazPbm obrazCzarny() {
  ObrazPbm o;
  o.w = 800;
  o.h = 434;
  o.bity.assign((size_t)100 * 434, 0xFF);
  return o;
}
// wymaganie: R-EKR-61
static void testRec2_stopkaObrazkaPiksele() {
  const int xIk = 772 - BOK_IKONY_AKTUALIZACJI - ODSTEP_IKONY_AKTUALIZACJI;  // lewa krawędź pasa ikony
  const std::string lewo = "noc 7 h 52 · drzemka 1 h 10 · AHI 12,4 · wyciek 24";
  const std::string prawo = "za oknem -12° · przy łóżku 21°";
  struct Przyp {
    const char* nazwa;
    bool ikona, bateria;
  } pr[] = {{"wygaszacz, wynik nocy", true, false}, {"wygaszacz, niska bateria", true, true},
            {"zastępstwo D5, wynik nocy", false, false}, {"zastępstwo D5, niska bateria", false, true}};
  for (auto& c : pr) {
    PlotnoR2 p;
    BateriaEkranu be;
    be.procent = 18;
    be.niska = c.bateria;
    ustawBaterieEkranu(be);
    ustawIkoneAktualizacji(c.ikona);
    StopkaDzienna s;
    s.lewo = lewo;
    s.prawo = prawo;
    s.bateriaNiska = c.bateria;
    rysujObrazekDnia(p, obrazCzarny(), s);
    int podIkona = p.czarnych(xIk, 437, 800, 480);
    int zaBrzegiem = p.czarnych(773, 437, 800, 480);
    bool ik = ikonaAktualizacji(p, false);
    int ikonaPx = p.czarnych(772 - BOK_IKONY_AKTUALIZACJI, 437, 773, 480);
    int tekstPx = p.czarnych(0, 437, xIk, 480);
    printf("  rec2 stopka obrazka (%s): pod pasem ikony przed ikoną %d px, za 772 %d px, ikona %d (%d px), treść %d px\n",
           c.nazwa, podIkona, zaBrzegiem, ik, ikonaPx, tekstPx);
    SPRAWDZ(zaBrzegiem == 0);
    if (c.ikona) SPRAWDZ(podIkona == 0 && ik && ikonaPx > 100);
    else SPRAWDZ(!ik);
    SPRAWDZ(tekstPx > 0);
    if (c.bateria) {  // ikona baterii z lewej (obrys od x 28, y 447)
      bool obrys = true;
      for (int x = 28; x < 62; x++) obrys = obrys && p.b[(size_t)447 * 800 + x];
      SPRAWDZ(obrys);
    }
  }
  ustawIkoneAktualizacji(false);
  ustawBaterieEkranu(BateriaEkranu());
}

// ---------- dyrygent z atrapami ----------
// Open-Meteo na 4.11.2026 z profilem temperatury i PM2.5 (jutro jak dziś, bez opadu: bez S-K-34).
static std::string jsonPrognozyP(const std::string& d, const std::function<float(int)>& t) {
  std::string tt = "\"time\":[", a = "\"temperature_2m\":[", o = "\"precipitation_probability\":[",
              c = "\"cloud_cover\":[", w = "\"wind_gusts_10m\":[";
  for (int i = 0; i < 48; i++) {
    std::string dz = i < 24 ? d : dodajDni(d, 1);
    char b[32];
    snprintf(b, sizeof b, "\"%sT%02d:00\"", dz.c_str(), i % 24);
    const char* s = i ? "," : "";
    char v[16];
    snprintf(v, sizeof v, "%.1f", (double)t(i % 24));
    tt += s + std::string(b);
    a += s + std::string(v);
    o += s + std::string("0");
    c += s + std::string("40");
    w += s + std::string("20");
  }
  return "{\"hourly\":{" + tt + "]," + a + "]," + o + "]," + c + "]," + w + "]}}";
}
static std::string jsonPowietrzaP(const std::string& d, const std::function<float(int)>& pm) {
  std::string t = "\"time\":[", p = "\"pm2_5\":[";
  for (int i = 0; i < 48; i++) {
    std::string dz = i < 24 ? d : dodajDni(d, 1);
    char b[32];
    snprintf(b, sizeof b, "\"%sT%02d:00\"", dz.c_str(), i % 24);
    char v[16];
    snprintf(v, sizeof v, "%.1f", (double)pm(i % 24));
    t += (i ? "," : "") + std::string(b);
    p += (i ? "," : "") + std::string(v);
  }
  return "{\"hourly\":{" + t + "]," + p + "]}}";
}
struct DobaR2 {
  std::vector<Wyb> v;
  int odswiezen = 0;
  std::string obrazkiCsv;
  std::map<int, std::vector<uint8_t>> klatki;  // minuta doby → klatka po odświeżeniu e-ink
};
// Doba 4.11.2026 od 00:05 jak symuluj(); `pogoda` podmienia prognozę i powietrze, `hak` przed każdym wybudzeniem.
static DobaR2 dobaR2(PanelSym& s, uint32_t doT, const std::function<float(int)>& t, const std::function<float(int)>& pm,
                     const std::function<void(PanelSym&, uint32_t)>& hak, const std::string& plikKonf = "",
                     bool obrazki = true, bool miasto = true) {
  if (obrazki) {
    s.p.pliki.pliki["/obrazki/001_syntetyczny.pbm"] = pbm(434, 434);
    s.p.pliki.pliki["/obrazki/obrazki.json"] = "{\"wersja\":1,\"obrazki\":[{\"plik\":\"001_syntetyczny.pbm\"}]}";
  }
  if (!plikKonf.empty()) s.p.pliki.pliki["/konfiguracja.txt"] = plikKonf;
  s.p.czujniki.o.temp = 21.0f;
  DobaR2 d;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  uint32_t tt = ep(2026, 11, 4, 0, 5);
  PowodWybudzenia pw = PowodWybudzenia::START;
  const KonfiguracjaPanelu k = konf(miasto);
  for (int i = 0; i < 800 && tt < doT; i++) {
    odpowiedziB9(s, tt);
    s.siec.odpowiedzi.erase("/v1/forecast");
    s.siec.odpowiedzi.erase("/v1/air-quality");
    s.siec.dodaj("/v1/forecast", 200, jsonPrognozyP("2026-11-04", t));
    s.siec.dodaj("/v1/air-quality", 200, jsonPowietrzaP("2026-11-04", pm));
    hak(s, tt);
    Wyb w = wybudz(s, tt, pw, k);
    d.v.push_back(w);
    d.odswiezen += w.m.odswiezen;
    if (w.m.odswiezen) d.klatki[w.minuta].assign(s.ekran.bufor(), s.ekran.bufor() + 48000);
    if (!w.spal) break;
    uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
    if (snu == 0) snu = 1;
    tt += w.m.msPrzySnie / 1000 + snu;
    pw = PowodWybudzenia::ZEGAR;
  }
  d.obrazkiCsv = s.p.pliki.pliki["/dane/obrazki.csv"] + s.p.pliki.pliki["/obrazki.csv"];
  return d;
}
static std::string ekranyGodzin(const std::vector<Wyb>& v, int od, int doG) {
  std::string r;
  for (int g = od; g <= doG; g++) {
    std::string e = "-";
    for (const char* id : {"S-K-33", "S-K-34", "S-K-35", "S-K-36", "S-K-37", "S-K-38", "S-K-39"})
      if (ekranO(v, id, g)) e = id + 4;
    char b[24];
    snprintf(b, sizeof b, " %02d:%s", g, e.c_str());
    r += b;
  }
  return r;
}
static size_t wpisowObrazka(const std::string& csv) {
  size_t n = 0;
  for (size_t q = csv.find("2026-11-04;"); q != std::string::npos; q = csv.find("2026-11-04;", q + 1)) n++;
  return n;
}
static void bezHaka(PanelSym&, uint32_t) {}

static float szczyt14T(int h) { return h <= 14 ? (float)(h - 5) : (float)(9 - (h - 14)); }
static float smogOd16(int h) { return h >= 16 ? 60.0f : 10.0f; }
static float czysto(int) { return 10.0f; }

// (c) `Wietrz od 12:00` → wygaszacz 11:00 → 12:00 `Wietrz do 16:00` → 16:00 smog (okno minęło) → wygaszacz od razu,
// bez nowego progu (licznik od startu slotu 08:00 liczy dalej, R-KON-73/R-KON-76). Dziennik obrazków: jeden wpis
// mimo dwóch wejść wygaszacza. Energia: tyle samo wybudzeń co z wygaszaczem wyłączonym, odświeżenia ≤ budżet 30.
// wymaganie: R-KON-75
static void testRec2_przejsciaOkna() {
  PanelSym s, q;
  DobaR2 d = dobaR2(s, ep(2026, 11, 4, 20, 30), szczyt14T, smogOd16, bezHaka);
  DobaR2 off = dobaR2(q, ep(2026, 11, 4, 20, 30), szczyt14T, smogOd16, bezHaka, "dzien.wygaszacz=nie\n");
  printf("  rec2 doba `Wietrz od 12` / smog od 16, wygaszacz:   %s\n", ekranyGodzin(d.v, 8, 20).c_str());
  printf("  rec2 doba `Wietrz od 12` / smog od 16, wyłączony:   %s\n", ekranyGodzin(off.v, 8, 20).c_str());
  printf("  rec2 wybudzeń %zu / %zu, odświeżeń e-ink %d / %d, wpisów obrazka %zu\n", d.v.size(), off.v.size(), d.odswiezen,
         off.odswiezen, wpisowObrazka(d.obrazkiCsv));
  SPRAWDZ(ekranO(d.v, "S-K-33", 8) && ekranO(d.v, "S-K-33", 10));
  SPRAWDZ(ekranO(d.v, "S-K-38", 11));
  SPRAWDZ(ekranO(d.v, "S-K-33", 12) && ekranO(d.v, "S-K-33", 15));
  SPRAWDZ(logMa(d.v, "slot dzienny: S-K-38", 16 * 60, 21 * 60));
  ROWNE_L(d.v.size(), off.v.size());
  SPRAWDZ(d.odswiezen <= 30);
  SPRAWDZ(wpisowObrazka(d.obrazkiCsv) == 1);
}

// (c) zimowy dzień bez smogu: po 12:00 `Wietrz do` do końca slotu (okno przesuwane po szczycie), wygaszacz tylko
// o 11:00. Dokumentuje skutek W3 + reguły 5.8.10.
// wymaganie: R-KON-75
static void testRec2_zimaBezSmogu() {
  PanelSym s;
  DobaR2 d = dobaR2(s, ep(2026, 11, 4, 20, 30), szczyt14T, czysto, bezHaka);
  printf("  rec2 doba zimowa bez smogu, szczyt 14:00: %s\n", ekranyGodzin(d.v, 8, 20).c_str());
  int godzWyg = 0;
  for (int g = 8; g <= 20; g++) godzWyg += ekranO(d.v, "S-K-38", g);
  printf("  rec2 godzin z wygaszaczem 08–20: %d\n", godzWyg);
  SPRAWDZ(ekranO(d.v, "S-K-33", 12));
}

static float wolt(int procent) {  // napięcie, dla którego krzywa M-25 daje dokładnie `procent`
  for (int mv = 3300; mv <= 4250; mv++)
    if (procentBaterii(mv) == procent) return (float)mv / 1000.0f;
  return NAN;
}
static bool ikonaBateriiK(const std::vector<uint8_t>& k) { return ikonaWStopce(k); }

// (d) bateria w trakcie wygaszacza: 22% do 13:00, 18% (stan „niska”) od 13:00, 15% (linia E-05) od 15:00, 4% (S5)
// od 17:00, 22% od 18:00 (histereza: dalej „niska”), 26% od 19:00. Oczekiwane: wygaszacz o 11:00 bez ostrzeżenia,
// o 13:00 wygaszacz z ikoną baterii w stopce, od 15:00 bez wygaszacza (linia uwagi wygrywa), o 17:00 nic ze slotu
// (S5), wygaszacz wraca po zniknięciu E-05. Wybudzeń tyle samo co przy 78%.
// wymaganie: R-KON-69
static void testRec2_bateriaWTrakcie() {
  const float v22 = wolt(22), v18 = wolt(18), v15 = wolt(15), v4 = wolt(4), v26 = wolt(26), v78 = wolt(78);
  auto hak = [&](PanelSym& s, uint32_t t) {
    int m = minutaLok(t);
    float v = m < 13 * 60 ? v22 : m < 15 * 60 ? v18 : m < 17 * 60 ? v15 : m < 18 * 60 ? v4 : m < 19 * 60 ? v22 : v26;
    s.p.czujniki.o.bateriaV = v;
    s.p.czujniki.zas = Zasilanie::BATERIA;
  };
  PanelSym s, q;
  DobaR2 d = dobaR2(s, ep(2026, 11, 4, 20, 30), szczyt14T, [](int) { return 60.0f; }, hak);  // smog: bez S-K-33 okna
  DobaR2 b78 = dobaR2(q, ep(2026, 11, 4, 20, 30), szczyt14T, [](int) { return 60.0f; },
                      [&](PanelSym& p, uint32_t) { p.p.czujniki.o.bateriaV = v78; p.p.czujniki.zas = Zasilanie::BATERIA; });
  printf("  rec2 bateria 22→18→15→4→22→26%%: %s\n", ekranyGodzin(d.v, 8, 20).c_str());
  printf("  rec2 bateria 78%% stale:          %s\n", ekranyGodzin(b78.v, 8, 20).c_str());
  PanelSym o;  // ta sama bateria z wygaszaczem wyłączonym: czy opóźnienie ostrzeżenia jest takie samo
  DobaR2 off = dobaR2(o, ep(2026, 11, 4, 20, 30), szczyt14T, [](int) { return 60.0f; }, hak, "dzien.wygaszacz=nie\n");
  printf("  rec2 ta sama bateria, wyłączony:  %s\n", ekranyGodzin(off.v, 8, 20).c_str());
  printf("  rec2 wybudzeń %zu / %zu (78%%) / %zu (wył.), odświeżeń %d / %d / %d\n", d.v.size(), b78.v.size(), off.v.size(),
         d.odswiezen, b78.odswiezen, off.odswiezen);
  auto pierwszaIkona = [](const DobaR2& x) {
    for (auto& k : x.klatki)
      if (k.first >= 8 * 60 && ikonaBateriiK(k.second)) return k.first;
    return -1;
  };
  auto pierwszaLinia = [](const DobaR2& x) {
    for (auto& w : x.v)
      if (w.minuta >= 8 * 60)
        for (auto& z : w.zdarzenia)
          if (z.find("linia uwagi:") != std::string::npos) return w.minuta;
    return -1;
  };
  for (auto& k : d.klatki)
    if (k.first >= 8 * 60 && k.first < 21 * 60)
      printf("    odświeżenie %02d:%02d, ikona baterii w stopce %d\n", k.first / 60, k.first % 60, ikonaBateriiK(k.second));
  printf("  rec2 pierwsza ikona baterii: wygaszacz %d min, wyłączony %d min; pierwsza linia uwagi: %d / %d min\n",
         pierwszaIkona(d), pierwszaIkona(off), pierwszaLinia(d), pierwszaLinia(off));
  SPRAWDZ(ekranO(d.v, "S-K-38", 11));
  SPRAWDZ(ekranO(d.v, "S-K-38", 13) || ekranO(d.v, "S-K-38", 14));
  // ostrzeżenie (ikona w stopce, linia E-05) nie później z wygaszaczem niż bez niego
  SPRAWDZ(pierwszaIkona(d) >= 0 && pierwszaIkona(d) <= pierwszaIkona(off));
  SPRAWDZ(pierwszaLinia(d) >= 0 && pierwszaLinia(d) <= pierwszaLinia(off));
  // przy linii E-05 obrazka nie ma (R-KON-54): w godzinach z linią uwagi brak S-K-38
  int obrazekZLinia = 0;
  for (auto& w : d.v) {
    bool l = false, o38 = false;
    for (auto& z : w.zdarzenia) {
      l = l || z.find("linia uwagi:") != std::string::npos;
      o38 = o38 || z.find("slot dzienny: S-K-38") != std::string::npos;
    }
    obrazekZLinia += l && o38;
  }
  ROWNE_L(obrazekZLinia, 0);
  ROWNE_L(d.v.size(), b78.v.size());
  ROWNE_L(d.v.size(), off.v.size());
  SPRAWDZ(d.odswiezen <= off.odswiezen);
}

// (b, e) informacja po aktualizacji samodzielnej uzbrojona o 13:00, gdy stoi wygaszacz: przy ocenie 13:00 wygaszacz
// ustępuje, informacja staje na ekranie slotu bez sprawy (S-K-37) i stoi do przycisku albo końca doby. Panel bez
// miasta (S-K-38 jako zastępstwo D5): informacja nie ma ekranu bez sprawy, więc nigdy się nie pokazuje.
// wymaganie: R-KON-75
static void testRec2_informacjaWTrakcie() {
  const char* INF = "ekran dnia: informacja po aktualizacji samodzielnej";
  auto uzbroj = [](PanelSym& s) {
    aktualizacja::Informacja i;
    i.rodzaj = aktualizacja::RodzajInformacji::UDANA;
    i.wlasna = i.nowa = "1.0.0";
    s.p.nvs.ustawTekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, aktualizacja::zapisInformacji(i));
  };
  bool raz = false;
  PanelSym s;
  DobaR2 d = dobaR2(s, ep(2026, 11, 4, 20, 30), szczyt14T, [](int) { return 60.0f; }, [&](PanelSym& p, uint32_t t) {
    if (!raz && t >= ep(2026, 11, 4, 12, 30)) { uzbroj(p); raz = true; }
  });
  printf("  rec2 informacja uzbrojona 12:30: %s\n", ekranyGodzin(d.v, 8, 20).c_str());
  printf("  rec2 informacja na ekranie o 13:00 %d, 16:00 %d, 20:00 %d\n", logMa(d.v, INF, 13 * 60, 13 * 60 + 1),
         logMa(d.v, INF, 16 * 60, 16 * 60 + 1), logMa(d.v, INF, 20 * 60, 20 * 60 + 1));
  SPRAWDZ(ekranO(d.v, "S-K-38", 11) && ekranO(d.v, "S-K-38", 12));
  SPRAWDZ(logMa(d.v, INF, 13 * 60, 13 * 60 + 1) && !ekranO(d.v, "S-K-38", 13));
  // bez miasta: zastępstwo D5, informacja czeka cały dzień (nigdy nie ma ekranu bez sprawy)
  bool raz2 = false;
  PanelSym q;
  DobaR2 e = dobaR2(q, ep(2026, 11, 4, 20, 30), [](int) { return 15.0f; }, czysto, [&](PanelSym& p, uint32_t t) {
    if (!raz2 && t >= ep(2026, 11, 4, 9, 30)) { uzbroj(p); raz2 = true; }
  }, "", true, false);
  aktualizacja::Informacja po = aktualizacja::odczytInformacji(
      q.p.nvs.tekst(aktualizacja::PRZESTRZEN, aktualizacja::KLUCZ_INFORMACJI, ""));
  printf("  rec2 bez miasta, informacja od 09:30: %s; informacja pokazana %d, wieczorem nadal czeka %d\n",
         ekranyGodzin(e.v, 8, 20).c_str(), logMa(e.v, INF), po.rodzaj != aktualizacja::RodzajInformacji::BRAK);
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testRec2_staraBazaBezWygaszacza();
  testRec2_pelnaTabela();
  testRec2_kombinacje();
  testRec2_oknoZimowePrzesuwane();
  testRec2_upalPo16();
  testRec2_stopkaObrazkaPiksele();
  testRec2_przejsciaOkna();
  testRec2_zimaBezSmogu();
  testRec2_bateriaWTrakcie();
  testRec2_informacjaWTrakcie();
  printf("testy_wygaszacz_brzegowe_2: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
