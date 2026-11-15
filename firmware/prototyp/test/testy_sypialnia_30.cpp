// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: strona kokpitu „Sypialnia · 30 nocy” (spec 5.5.8, R-KOK-29…R-KOK-33, ADR-0044; T-TEST-430):
// miejsce w ciągu stron i numer `n / 4`, numery stron w tekstach, wykres bez odczytu, pasmo i słowo, luki w linii,
// rysunek 1 bit, stopka i belka, powrót po restarcie z pamięci RTC przez dyrygenta na atrapach. Dane syntetyczne.
// Uruchomienie: tools/prototyp/testy.sh (testy_sypialnia_30).
#include <cmath>
#include <cstdio>
#include <ctime>
#include <set>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "kontekst_sk.h"
#include "nawigacja.h"
#include "podsumowanie.h"
#include "sprawy.h"
#include "start.h"
#include "sypialnia.h"
#include "sypialnia_ekran.h"
#include "wspolne.h"
#include "wybudzenie.h"
#include "zestaw_kolejki.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)

struct PlotnoTest : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)(y * 800 + x)] = c;
  }
  bool cz(int x, int y) const { return x >= 0 && x < 800 && y >= 0 && y < 480 && px[(size_t)(y * 800 + x)]; }
  int czarne(int x0, int y0, int x1, int y1) const {  // [x0, x1) × [y0, y1)
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += cz(x, y);
    return n;
  }
};

static const std::string DZIS = "2026-11-14";

// 30 nocy (poranki DZIS − 29 … DZIS) z wartościami wokół podanych, deterministycznie; `bez` = indeksy nocy bez odczytu
static std::vector<NocSypialni> noce(int ile, float t, float w, float c, std::set<int> bez = {}) {
  std::vector<NocSypialni> v;
  for (int i = 0; i < ile; i++) {
    if (bez.count(i)) continue;
    std::string d = dodajDni(DZIS, i - (ile - 1));
    float dt = (float)((i * 7) % 5 - 2) * 0.2f, dw = (float)((i * 3) % 7 - 3), dc = (float)((i * 11) % 9 - 4) * 20;
    v.push_back(NocSypialni{d, t + dt, w + dw, std::isnan(c) ? NAN : c + dc});
  }
  return v;
}

// ---------- R-KOK-29: strona w ciągu tylko z danymi, numer n / 4 ----------
// wymaganie: R-KOK-29
static void testR_KOK_29_ciagStron() {
  // kiedy strona jest w ciągu
  SPRAWDZ(!stronaSypialniJest({}, DZIS));
  SPRAWDZ(stronaSypialniJest(noce(1, 20, 40, NAN), DZIS));
  SPRAWDZ(!stronaSypialniJest({NocSypialni{dodajDni(DZIS, -30), 20, 40, 600}}, DZIS));  // 31. noc wstecz: poza oknem
  SPRAWDZ(stronaSypialniJest({NocSypialni{dodajDni(DZIS, -29), NAN, NAN, 600}}, DZIS));  // samo CO2 wystarcza
  SPRAWDZ(!stronaSypialniJest({NocSypialni{DZIS, NAN, NAN, NAN}}, DZIS));                 // wiersz bez odczytu
  SPRAWDZ(!stronaSypialniJest({NocSypialni{dodajDni(DZIS, 1), 20, 40, 600}}, DZIS));       // data w przyszłości
  SPRAWDZ(!stronaSypialniJest(noce(30, 20, 40, 600), ""));                                // bez zegara

  // numeracja: bez strony sypialni jak przed ADR-0044, z nią n / 4, U-01 zawsze ostatnia
  ustawSypialnieWKokpicie(false);
  SPRAWDZ(stronKokpitu() == 3 && numerStronyTekst(Ekran::K1) == "1 / 3" && numerStronyTekst(Ekran::USTAWIENIA) == "3 / 3");
  SPRAWDZ(numerStronyTekst(Ekran::SYPIALNIA).empty() && stronaKokpitu(Ekran::SYPIALNIA));
  ustawSypialnieWKokpicie(true);
  SPRAWDZ(stronKokpitu() == 4 && numerStronyTekst(Ekran::K1) == "1 / 4" && numerStronyTekst(Ekran::K2) == "2 / 4");
  SPRAWDZ(numerStronyTekst(Ekran::SYPIALNIA) == "3 / 4" && numerStronyTekst(Ekran::USTAWIENIA) == "4 / 4");
  SPRAWDZ(numerStronyTekst(Ekran::SPRAWY).empty() && numerStronyTekst(Ekran::LICZBY).empty());
  SPRAWDZ(zKolejkiWolno(Ekran::SYPIALNIA, false) && !zKolejkiWolno(Ekran::SYPIALNIA, true));

  // ciąg ▶ i odwrócenie ◀ we wszystkich kombinacjach: sprawy, profil publiczny, sypialnia
  for (int maska = 0; maska < 8; maska++) {
    KontekstNaw k;
    k.sprawyDostepne = maska & 1;
    k.spraw = k.sprawyDostepne ? 1 : 0;
    k.sprawa = 1;
    k.profilPubliczny = maska & 2;
    k.sypialniaDostepna = maska & 4;
    ustawSypialnieWKokpicie(k.sypialniaDostepna);
    std::vector<Ekran> ciag = {Ekran::DZIEN};
    Ekran e = Ekran::DZIEN;
    for (int i = 0; i < 10 && e != Ekran::USTAWIENIA; i++) ciag.push_back(e = nacisniecie(e, Przycisk::PRAWY, false, k).ekran);
    std::vector<Ekran> ocz = {Ekran::DZIEN, Ekran::K1, Ekran::K2};
    if (k.profilPubliczny) ocz.push_back(Ekran::LICZBY);
    if (k.sprawyDostepne) ocz.push_back(Ekran::SPRAWY);
    if (k.sypialniaDostepna) ocz.push_back(Ekran::SYPIALNIA);
    ocz.push_back(Ekran::USTAWIENIA);
    SPRAWDZ(ciag == ocz);
    if (ciag != ocz) printf("  maska %d: ciąg ▶ inny niż oczekiwany\n", maska);
    // numery stron rosną po 1 i kończą się na U-01 = liczba stron
    int ostatni = 0;
    for (Ekran x : ciag)
      if (numerStrony(x)) {
        SPRAWDZ(numerStrony(x) == ostatni + 1);
        ostatni = numerStrony(x);
      }
    SPRAWDZ(ostatni == (k.sypialniaDostepna ? 4 : 3));
    // ◀ z U-01 cofa dokładnie odwrotnie
    std::vector<Ekran> wstecz = {Ekran::USTAWIENIA};
    for (int i = 0; i < 10 && e != Ekran::DZIEN; i++) wstecz.push_back(e = nacisniecie(e, Przycisk::LEWY, false, k).ekran);
    std::vector<Ekran> odwr(ocz.rbegin(), ocz.rend());
    SPRAWDZ(wstecz == odwr);
  }
  // ◀ ze strony sypialni na „Otwarte sprawy” wchodzi od ostatniej karty (R-KOK-20), ▶ ze spraw otwiera sypialnię
  KontekstNaw k;
  k.sprawyDostepne = true;
  k.spraw = 2;
  k.sprawa = 2;
  k.sypialniaDostepna = true;
  ustawSypialnieWKokpicie(true);
  SPRAWDZ(nacisniecie(Ekran::SYPIALNIA, Przycisk::LEWY, false, k).akcja == Akcja::SPRAWY_OD_KONCA);
  SPRAWDZ(nacisniecie(Ekran::SPRAWY, Przycisk::PRAWY, false, k).ekran == Ekran::SYPIALNIA);
  // B31: na stronie sypialni ↻ ekran dnia, przytrzymania nic, ▶ U-01
  SPRAWDZ(nacisniecie(Ekran::SYPIALNIA, Przycisk::SRODEK, false, k).ekran == Ekran::DZIEN);
  for (Przycisk p : {Przycisk::LEWY, Przycisk::PRAWY, Przycisk::SRODEK}) {
    Przejscie x = nacisniecie(Ekran::SYPIALNIA, p, true, k);
    SPRAWDZ(x.ekran == Ekran::SYPIALNIA && x.akcja == Akcja::NIC);
  }
  SPRAWDZ(nacisniecie(Ekran::SYPIALNIA, Przycisk::PRAWY, false, k).ekran == Ekran::USTAWIENIA);
  SPRAWDZ(nacisniecie(Ekran::USTAWIENIA, Przycisk::LEWY, false, k).ekran == Ekran::SYPIALNIA);
  // wartość w pamięci RTC
  SPRAWDZ(ekranZRtc((unsigned char)Ekran::SYPIALNIA) == Ekran::SYPIALNIA && ekranZRtc(20) == Ekran::DZIEN);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::PRZYCISK, Ekran::SYPIALNIA, true, 60, 600) == Ekran::SYPIALNIA);
  SPRAWDZ(ekranPoStarcie(PowodWybudzenia::ZEGAR, Ekran::SYPIALNIA, true, 400, 600) == Ekran::DZIEN);  // po 5 min
  ustawSypialnieWKokpicie(false);
}

// ---------- R-KOK-30: numery stron w tekstach ----------
// wymaganie: R-KOK-30
static void testR_KOK_30_numeryWTekstach() {
  const char* przyklady[] = {"Bezdechy 6 / h od 4 tygodni. Na kontroli: raport 2 / 3.", "→ Otwarte sprawy: za stroną 2 / 3.",
                             "→ Sprawdź AirBridge i sieć. Ustawienia: strona 3 / 3.",
                             "Część ocen wyłączona. Lista na stronie 1 / 3 (znak *).", "13 / 30 nocy", "3 / 31", "x2 / 3"};
  const char* oczek[] = {"Bezdechy 6 / h od 4 tygodni. Na kontroli: raport 2 / 4.", "→ Otwarte sprawy: za stroną 2 / 4.",
                         "→ Sprawdź AirBridge i sieć. Ustawienia: strona 4 / 4.",
                         "Część ocen wyłączona. Lista na stronie 1 / 4 (znak *).", "13 / 30 nocy", "3 / 31", "x2 / 3"};
  ustawSypialnieWKokpicie(false);
  for (const char* t : przyklady) SPRAWDZ(numeryStronWTekscie(t) == t);
  ustawSypialnieWKokpicie(true);
  for (size_t i = 0; i < sizeof(przyklady) / sizeof(*przyklady); i++) {
    SPRAWDZ(numeryStronWTekscie(przyklady[i]) == oczek[i]);
    if (numeryStronWTekscie(przyklady[i]) != oczek[i]) printf("  „%s” → „%s”\n", przyklady[i], numeryStronWTekscie(przyklady[i]).c_str());
  }
  // tylko po słowie strony; ułamki i numery bez słowa zostają, twarda spacja U+00A0 jak zwykła
  for (const char* t : {"2 / 3", "Oddychanie okresowe w 2 / 3 nocy.", "Oddechy 3 / 3 s.", "Ustawienia 3 / 3.",
                        "raport 2 / 3,5", "raport 2 / 30", "przestrona 2 / 3", "raport 2/3"})
    SPRAWDZ(numeryStronWTekscie(t) == t);
  SPRAWDZ(numeryStronWTekscie("(strona\xC2\xA0" "2 / 3)") == "(strona\xC2\xA0" "2 / 4)");
  SPRAWDZ(numeryStronWTekscie("raport 2\xC2\xA0/\xC2\xA0" "3.") == "raport 2\xC2\xA0/\xC2\xA0" "4.");
  SPRAWDZ(numeryStronWTekscie("na stronie 3 / 3 i raportu 1 / 3") == "na stronie 4 / 4 i raportu 1 / 4");
  for (const char* t : {"raport 2 / 3.", "za stroną 2 / 3.", "strona 3 / 3"}) {  // drugie przeliczenie nic nie zmienia
    std::string a = numeryStronWTekscie(t);
    SPRAWDZ(numeryStronWTekscie(a) == a);
  }
  // linia C z zestawu ma przy stronie sypialni `raport 2 / 4`; karta sprawy zdejmuje ją tak samo jak `2 / 3`
  for (const char* num : {"2 / 3", "2 / 4"}) {
    sprawy::SprawaL x;
    x.klucz = x.id = "L-03";
    x.stan = reguly::StanReguly::AKTYWNA;
    x.pilnosc = reguly::Pilnosc::C;
    x.ton = sprawy::Ton::LEKARZ;
    x.linia = std::string("Bezdechy 6 / h od 4 tygodni. Na kontroli: raport ") + num + ".";
    std::vector<sprawy::SprawaL> v = {x};
    sprawy::WejscieSpraw we;
    we.dzis = DZIS;
    we.sprawy = &v;
    std::vector<sprawy::KartaSprawy> k = sprawy::kartySpraw(we);
    SPRAWDZ(!k.empty() && k[0].tytul == "Sprawa na kontrolę" && k[0].powod == "Bezdechy 6 / h od 4 tygodni.");
    if (!k.empty() && k[0].powod != "Bezdechy 6 / h od 4 tygodni.") printf("  powód karty: „%s”\n", k[0].powod.c_str());
  }
  // tekst z zestawu idzie przez reguly::tekst(): ten sam tekst ma numer według ciągu
  std::string z = sk::tekst("S-K-18.krok_sprawy");
  SPRAWDZ(z == "→ Otwarte sprawy: za stroną 2 / 4.");
  ustawSypialnieWKokpicie(false);
  SPRAWDZ(sk::tekst("S-K-18.krok_sprawy") == "→ Otwarte sprawy: za stroną 2 / 3.");
}

// ---------- R-KOK-31, R-KOK-32: wartość, pasmo, słowo, brak odczytu ----------
// wymaganie: R-KOK-31
static void testR_KOK_31_brakOdczytu() {
  // brak CO2 we wszystkich nocach: wykres z napisem „brak odczytu” w polu wykresu, bez słowa, bez kropki
  std::vector<NocSypialni> s = noce(30, 20, 45, NAN);
  DaneSyp30 d = budujSyp30(s, DZIS, {});
  SPRAWDZ(!d.w[2].cokolwiek() && d.w[2].slowo == SlowoSyp30::BEZ_DANYCH && d.w[2].wartosc == "–");
  PlotnoTest p;
  rysujSyp30(p, d);
  const int y2 = SYP30_RZAD_Y0 + 2 * SYP30_RZAD_H;
  SPRAWDZ(p.czarne(SYP30_X_WYKRESU, y2 + SYP30_WYKRES_GORA, SYP30_X_WYKRESU + SYP30_W_WYKRESU,
                   y2 + SYP30_WYKRES_GORA + SYP30_WYKRES_H) > 200);  // napis, nie puste pole
  PlotnoTest napis;
  tekstSrodek(napis, F_P22B, 400, 240, "brak odczytu");
  SPRAWDZ(p.czarne(SYP30_X_WYKRESU, y2 + SYP30_WYKRES_GORA, SYP30_X_WYKRESU + SYP30_W_WYKRESU,
                   y2 + SYP30_WYKRES_GORA + SYP30_WYKRES_H) == napis.czarne(0, 0, 800, 480));
  SPRAWDZ(p.czarne(SYP30_X_SLOWA_MIN, y2, 800, y2 + SYP30_RZAD_H - 2) == 0);  // bez słowa (nie „jak zwykle”)
  // brak odczytu tylko ostatniej nocy: linia jest, słowo `brak odczytu`, bez kropki, wartość `–`
  s = noce(30, 20, 45, 700, {29});
  d = budujSyp30(s, DZIS, {});
  for (auto& w : d.w) {
    SPRAWDZ(w.cokolwiek() && !w.jestOstatnia() && w.slowo == SlowoSyp30::BRAK_OSTATNIEJ && w.wartosc == "–");
    SPRAWDZ(std::string(tekstSlowaSyp30(w.slowo)) == "brak odczytu");
  }
  // brak ostatniej nocy przy małej liczbie nocy (bez pasma): `brak odczytu`, nie `uczę się`
  for (int ile : {2, 5, 10}) {
    d = budujSyp30(noce(ile, 20, 45, 700, {ile - 1}), DZIS, {});
    for (auto& w : d.w) SPRAWDZ(!w.pasmo && w.slowo == SlowoSyp30::BRAK_OSTATNIEJ);
  }
  // przed zamknięciem nocy (przed 06:00) `noc trwa` przy wartości, bez kropki i bez słowa; poprzednia noc nie
  // udaje dzisiejszej
  {
    std::vector<NocSypialni> v = noce(30, 20, 45, 700, {29});
    DaneSyp30 t = budujSyp30(v, DZIS, {}, true);
    for (auto& w : t.w) SPRAWDZ(w.slowo == SlowoSyp30::NOC_TRWA && w.wartosc == "–" && !w.jestOstatnia());
    SPRAWDZ(std::string(tekstSlowaSyp30(SlowoSyp30::NOC_TRWA)) == "noc trwa");
    PlotnoTest pt;
    rysujSyp30(pt, t);
    for (int r = 0; r < 3; r++) {
      const int y0 = SYP30_RZAD_Y0 + r * SYP30_RZAD_H;
      SPRAWDZ(pt.czarne(SYP30_X_SLOWA_MIN, y0, 800, y0 + SYP30_RZAD_H - 2) == 0);  // bez słowa z prawej
      PlotnoTest nap;
      tekst(nap, F_P22B, 28, y0 + 74, "noc trwa");
      SPRAWDZ(pt.czarne(28, y0 + 40, SYP30_X_WYKRESU - 12, y0 + SYP30_RZAD_H - 2) == nap.czarne(28, y0 + 40, 400, y0 + 116));
      // bez kropki w kolumnie ostatniej nocy (linia średniej kończy się na nocy 28)
      // (kolumny x29 + 2 i x29 + 3 leżą w przerwie kresek pasma: kreska 6 px co 12 px od x 228)
      SPRAWDZ((xNocySyp30(29) + 2 - SYP30_X_WYKRESU) % (2 * SYP30_PASMO_KRESKA) >= SYP30_PASMO_KRESKA);
      SPRAWDZ(pt.czarne(xNocySyp30(29) + 2, y0 + SYP30_WYKRES_GORA, xNocySyp30(29) + 4,
                        y0 + SYP30_WYKRES_GORA + SYP30_WYKRES_H) == 0);
    }
    // noc zamknięta (jest wiersz) przed 06:00 albo noc bez wiersza po 06:00: jak dotąd
    SPRAWDZ(budujSyp30(noce(30, 20, 45, 700), DZIS, {}, true).w[0].slowo != SlowoSyp30::NOC_TRWA);
    SPRAWDZ(budujSyp30(v, DZIS, {}, false).w[0].slowo == SlowoSyp30::BRAK_OSTATNIEJ);
  }
  // brak odczytu nigdy nie daje „jak zwykle” (ADR-0029 pkt 7): wszystkie kombinacje braków ostatniej nocy
  for (int m = 0; m < 8; m++) {
    std::vector<NocSypialni> v = noce(30, 20, 45, 700);
    if (m & 1) v.back().t = NAN;
    if (m & 2) v.back().w = NAN;
    if (m & 4) v.back().co2 = NAN;
    DaneSyp30 x = budujSyp30(v, DZIS, {});
    for (int k = 0; k < 3; k++)
      if (m & (1 << k)) SPRAWDZ(x.w[k].slowo == SlowoSyp30::BRAK_OSTATNIEJ);
  }
}

// wymaganie: R-KOK-32
static void testR_KOK_32_pasmoISlowo() {
  // mniej niż 14 nocy przed ostatnią: bez pasma, `uczę się`; 14 nocy: pasmo
  for (int ile : {1, 10, 14, 15, 30}) {
    DaneSyp30 d = budujSyp30(noce(ile, 20, 45, 700), DZIS, {});
    for (auto& w : d.w) {
      SPRAWDZ(w.pasmo == (ile - 1 >= 14));
      SPRAWDZ((w.slowo == SlowoSyp30::UCZE_SIE) == (ile - 1 < 14));
    }
  }
  // słowo wobec pasma; zgodne ze słowem S2c (slowoSypialni) dla tych samych danych
  std::vector<NocSypialni> s = noce(30, 20, 45, 700);
  NormaSypialni nm = liczNormeSypialni(s, DZIS, {});
  struct P { float t; SlowoSyp30 oczek; };
  for (P x : {P{(float)nm.t.p90 + 1.0f, SlowoSyp30::WYZEJ}, P{(float)nm.t.p10 - 1.0f, SlowoSyp30::NIZEJ},
              P{(float)nm.t.p10, SlowoSyp30::JAK_ZWYKLE}, P{(float)nm.t.p90, SlowoSyp30::JAK_ZWYKLE}}) {
    s.back().t = x.t;
    DaneSyp30 d = budujSyp30(s, DZIS, {});
    SPRAWDZ(d.w[0].slowo == x.oczek);
    bool spokojne;
    slowoSypialni(x.t, nm.t, spokojne, "°");
    SPRAWDZ(spokojne == (x.oczek == SlowoSyp30::JAK_ZWYKLE));
  }
  // wartości i jednostki ostatniej nocy
  s = noce(30, 20, 45, 700);
  s.back().t = 19.46f;
  s.back().w = 37.6f;
  s.back().co2 = 1240.4f;
  DaneSyp30 d = budujSyp30(s, DZIS, {});
  SPRAWDZ(d.w[0].wartosc == "19,5" && d.w[1].wartosc == "38" && d.w[2].wartosc == "1240");
  SPRAWDZ(d.w[0].jednostka == "°C" && d.w[1].jednostka == "%" && d.w[2].jednostka == "ppm");
  SPRAWDZ(d.zakres == "16.10–14.11");
  // noc poza domem nie wchodzi do pasma (norma sypialni 4.4)
  std::set<std::string> poza;
  for (int i = 0; i < 20; i++) poza.insert(dodajDni(DZIS, -1 - i));
  SPRAWDZ(!budujSyp30(noce(30, 20, 45, 700), DZIS, poza).w[0].pasmo);
  // słowa bez ocen „dobrze/źle” i bez zaleceń (ADR-0002)
  for (SlowoSyp30 x : {SlowoSyp30::BRAK_OSTATNIEJ, SlowoSyp30::UCZE_SIE, SlowoSyp30::JAK_ZWYKLE, SlowoSyp30::WYZEJ, SlowoSyp30::NIZEJ}) {
    std::string t = tekstSlowaSyp30(x);
    for (const char* z : {"dobrze", "źle", "lepiej", "gorzej", "przewietrz", "ustaw", "zmień"}) SPRAWDZ(t.find(z) == std::string::npos);
  }
}

// ---------- R-KOK-33: luki przerywają linię, rysunek 1 bit, linia ≥ 2 px ----------
// wymaganie: R-KOK-33
static void testR_KOK_33_luki() {
  // 10 nocy (bez pasma), luka 3 nocy w środku: w kolumnie luki brak pikseli linii nocy
  std::vector<NocSypialni> s = noce(30, 20, 45, 700);
  std::vector<NocSypialni> v;
  for (auto& n : s)
    if (n.data >= dodajDni(DZIS, -9) && n.data != dodajDni(DZIS, -5) && n.data != dodajDni(DZIS, -4) && n.data != dodajDni(DZIS, -3))
      v.push_back(n);
  v.push_back(NocSypialni{dodajDni(DZIS, -15), 20, 45, 700});  // pojedyncza noc między lukami
  DaneSyp30 d = budujSyp30(v, DZIS, {});
  SPRAWDZ(!d.w[0].pasmo);
  PlotnoTest p;
  rysujSyp30(p, d);
  for (int r = 0; r < 3; r++) {
    const int gy = SYP30_RZAD_Y0 + r * SYP30_RZAD_H + SYP30_WYKRES_GORA;
    // kolumny między nocą 25 (DZIS − 4) a nocą 26: luka, bez pasma → pole puste
    int xs = (xNocySyp30(24) + xNocySyp30(25)) / 2;
    SPRAWDZ(p.czarne(xs - 1, gy, xs + 2, gy + SYP30_WYKRES_H) == 0);
    // noc pojedyncza (indeks 14) widoczna
    int x14 = xNocySyp30(14), y14 = yWartosciSyp30(d.w[r], r, d.w[r].v[14]);
    SPRAWDZ(p.czarne(x14 - 2, y14 - 2, x14 + 3, y14 + 3) == 25);
    // linia między nocą 20 i 21 (obie z odczytem) jest i ma co najmniej 2 px grubości w pionie
    int xm = (xNocySyp30(20) + xNocySyp30(21)) / 2;
    int grub = 0;
    for (int y = gy; y < gy + SYP30_WYKRES_H; y++) grub += p.cz(xm, y);
    SPRAWDZ(grub >= 2);
    // kropka ostatniej nocy: koło r 6
    int xo = xNocySyp30(29), yo = yWartosciSyp30(d.w[r], r, d.w[r].v[29]);
    SPRAWDZ(p.czarne(xo - 3, yo - 3, xo + 4, yo + 4) == 49);
  }
  // wykres w polu: wszystkie wartości (także skrajne) w pionie pola wykresu
  for (int r = 0; r < 3; r++)
    for (int i = 0; i < SYP30_NOCY; i++)
      if (d.w[r].jestNoc(i)) {
        int y = yWartosciSyp30(d.w[r], r, d.w[r].v[i]), gy = SYP30_RZAD_Y0 + r * SYP30_RZAD_H + SYP30_WYKRES_GORA;
        SPRAWDZ(y - SYP30_KROPKA_R >= gy && y + SYP30_KROPKA_R < gy + SYP30_WYKRES_H);
      }
  // stała wartość (zakres 0) i pasmo: bez dzielenia przez zero, linia w środku pola
  std::vector<NocSypialni> st;
  for (int i = 0; i < 30; i++) st.push_back(NocSypialni{dodajDni(DZIS, -i), 21, 50, 800});
  DaneSyp30 ds = budujSyp30(st, DZIS, {});
  SPRAWDZ(ds.w[0].pasmo && ds.w[0].slowo == SlowoSyp30::JAK_ZWYKLE);
  int yc = yWartosciSyp30(ds.w[0], 0, 21), gy0 = SYP30_RZAD_Y0 + SYP30_WYKRES_GORA;
  SPRAWDZ(yc > gy0 + SYP30_WYKRES_H / 3 && yc < gy0 + 2 * SYP30_WYKRES_H / 3);
}

// ---------- dane z pliku ----------
// wymaganie: R-KOK-31
static void testR_KOK_31_daneZPliku() {
  // ta sama noc dwa razy: strona bierze pierwszy wiersz, jak S2c (sypialniaZDnia); słowa nigdy sprzeczne
  std::vector<NocSypialni> s = noce(30, 20, 45, 700);
  NormaSypialni nm = liczNormeSypialni(s, DZIS, {});
  for (float drugi : {(float)nm.t.p90 + 3.0f, (float)nm.t.p10 - 3.0f, (float)nm.t.med}) {
    for (float pierwszy : {(float)nm.t.med, (float)nm.t.p90 + 3.0f}) {
      std::vector<NocSypialni> v = s;
      v.back().t = pierwszy;
      v.push_back(NocSypialni{DZIS, drugi, 45, 700});
      bool spokojne = false;
      slowoSypialni(sypialniaZDnia(v, DZIS)->t, liczNormeSypialni(v, DZIS, {}).t, spokojne, "°");
      DaneSyp30 d = budujSyp30(v, DZIS, {});
      SPRAWDZ(spokojne == (d.w[0].slowo == SlowoSyp30::JAK_ZWYKLE));
      SPRAWDZ(std::fabs(d.w[0].v[29] - pierwszy) < 1e-6f);
    }
  }
  // `inf`, `-inf`, `nan` w pliku odrzucają wiersz; strona nie wchodzi do ciągu z samych `inf`
  NocSypialni n;
  for (const char* l : {"2026-11-14;inf;45", "2026-11-14;20;-inf", "2026-11-14;20;45;inf", "2026-11-14;nan;45",
                        "2026-11-14;20;45;700;inf;800;700"})
    SPRAWDZ(!parsujSypialnie(l, n));
  std::string dc;
  float c = 0;
  SPRAWDZ(!parsujCo2Nocy("2026-11-14;inf", dc, c));
  SPRAWDZ(!stronaSypialniJest({NocSypialni{DZIS, INFINITY, -INFINITY, INFINITY}}, DZIS));
  SPRAWDZ(stronaSypialniJest({NocSypialni{DZIS, INFINITY, 45, INFINITY}}, DZIS));
  // wartości skrajne przycięte przy wczytaniu; calkowita() bez zachowania niezdefiniowanego
  SPRAWDZ(parsujSypialnie("2026-11-14;1e30;-5;65535;99999;1e30;700", n));
  SPRAWDZ(n.t == (float)SYP_T_MAX && n.w == 0 && n.co2 == (float)SYP_CO2_MAX && n.co2Ponad == 24 * 60 &&
          n.co2Maks == (int)SYP_CO2_MAX);
  SPRAWDZ(parsujSypialnie("2026-11-14;-1e30;250", n) && n.t == (float)SYP_T_MIN && n.w == 100);
  SPRAWDZ(parsujCo2Nocy("2026-11-14;65535", dc, c) && c == (float)SYP_CO2_MAX);
  SPRAWDZ(!calkowita(1e300).empty() && !calkowita(-1e300).empty() && !calkowita(INFINITY).empty());
  std::vector<NocSypialni> v = noce(30, 20, 45, 700);
  SPRAWDZ(parsujSypialnie("2026-11-14;20;45;65535", n));
  v.back() = n;
  DaneSyp30 d = budujSyp30(v, DZIS, {});
  SPRAWDZ(d.w[2].wartosc == "9999" && 28 + szer(F_P34B, d.w[2].wartosc) + 6 + szer(F_P22B, "ppm") <= SYP30_X_WYKRESU - 12);
}

// ---------- decyzja 9: oś z pasmem w środkowej trzeciej części, średnia krocząca z 7 nocy, przycinanie ----------
// wymaganie: R-KOK-33
static void testR_KOK_33_osISrednia() {
  // średnia krocząca ze znaną odpowiedzią: t = i w nocy i okna (0…29), 6 nocy przed oknem po −10
  std::vector<NocSypialni> s;
  for (int i = -6; i < 30; i++) s.push_back(NocSypialni{dodajDni(DZIS, i - 29), i < 0 ? -10.0f : (float)i, 45, 700});
  DaneSyp30 d = budujSyp30(s, DZIS, {});
  SPRAWDZ(std::fabs(d.w[0].sr[0] - (-60.0f / 7)) < 1e-4f);            // noc 0 i 6 nocy sprzed okna
  SPRAWDZ(std::fabs(d.w[0].sr[3] - (-30.0f + 6) / 7) < 1e-4f);        // 0+1+2+3 i trzy razy −10
  for (int i = 6; i < 30; i++) SPRAWDZ(std::fabs(d.w[0].sr[i] - (float)(i - 3)) < 1e-4f);
  // mniej niż 7 dostępnych nocy: średnia z tylu, ile jest; noce bez odczytu nie wchodzą do średniej
  s.clear();
  for (int i = 0; i < 30; i++)
    if (i != 10 && i != 11) s.push_back(NocSypialni{dodajDni(DZIS, i - 29), (float)i, 45, 700});
  d = budujSyp30(s, DZIS, {});
  SPRAWDZ(std::fabs(d.w[0].sr[0] - 0.0f) < 1e-4f && std::fabs(d.w[0].sr[2] - 1.0f) < 1e-4f);
  SPRAWDZ(std::fabs(d.w[0].sr[13] - (7 + 8 + 9 + 12 + 13) / 5.0f) < 1e-4f);
  SPRAWDZ(std::isnan(d.w[0].sr[10]) && std::isnan(d.w[0].sr[11]));  // punkt linii tylko w nocy z odczytem
  // kropka = prawdziwa wartość ostatniej nocy, słowo wobec pasma z prawdziwej wartości (nie ze średniej)
  s = noce(30, 20, 45, 700);
  NormaSypialni nm = liczNormeSypialni(s, DZIS, {});
  s.back().t = (float)nm.t.p90 + 0.5f;
  d = budujSyp30(s, DZIS, {});
  SPRAWDZ(d.w[0].slowo == SlowoSyp30::WYZEJ && d.w[0].sr[29] < d.w[0].v[29]);
  // oś: pasmo P10–P90 w środkowej trzeciej części pola
  {
    const WykresSyp30& w = d.w[0];
    double mn, mx;
    zakresOsiSyp30(w, 0, mn, mx);
    double sz = w.p90 - w.p10;
    SPRAWDZ(std::fabs(mn - (w.p10 - sz)) < 1e-9 && std::fabs(mx - (w.p90 + sz)) < 1e-9);
    const int gora = SYP30_RZAD_Y0 + SYP30_WYKRES_GORA + SYP30_MARGINES;
    const int dol = SYP30_RZAD_Y0 + SYP30_WYKRES_GORA + SYP30_WYKRES_H - 1 - SYP30_MARGINES;
    SPRAWDZ(std::abs(yWartosciSyp30(w, 0, w.p90) - (gora + (dol - gora) / 3)) <= 1);
    SPRAWDZ(std::abs(yWartosciSyp30(w, 0, w.p10) - (dol - (dol - gora) / 3)) <= 1);
    // poza osią: przycięte do krawędzi, kierunek z pozaOsiaSyp30()
    SPRAWDZ(yWartosciSyp30(w, 0, w.p90 + 10 * sz + 5) == gora && pozaOsiaSyp30(w, 0, w.p90 + 10 * sz + 5) == 1);
    SPRAWDZ(yWartosciSyp30(w, 0, w.p10 - 10 * sz - 5) == dol && pozaOsiaSyp30(w, 0, w.p10 - 10 * sz - 5) == -1);
    SPRAWDZ(pozaOsiaSyp30(w, 0, w.p10) == 0 && pozaOsiaSyp30(w, 0, w.p90) == 0);
  }
  // bez pasma („uczę się”): oś od najmniejszej do największej wartości, co najmniej SYP30_ZAKRES_MIN, bez przycinania
  d = budujSyp30(noce(10, 20, 45, 700), DZIS, {});
  for (int r = 0; r < 3; r++)
    for (int i = 0; i < 30; i++)
      if (d.w[r].jestNoc(i)) SPRAWDZ(pozaOsiaSyp30(d.w[r], r, d.w[r].v[i]) == 0);
  // noc odstająca daleko ponad pasmo: kropka przy górnej krawędzi pola ze strzałką obok; bez odstawania bez strzałki
  for (bool odstaje : {false, true}) {
    s = noce(30, 20, 45, 700);
    if (odstaje) s.back().t = (float)nm.t.p90 + 50.0f;
    d = budujSyp30(s, DZIS, {});
    PlotnoTest p;
    rysujSyp30(p, d);
    const int xo = xNocySyp30(29), yo = yWartosciSyp30(d.w[0], 0, d.w[0].v[29]);
    const int gy = SYP30_RZAD_Y0 + SYP30_WYKRES_GORA;
    SPRAWDZ(yo - SYP30_KROPKA_R >= gy && p.czarne(xo - 3, yo - 3, xo + 4, yo + 4) == 49);  // kropka w polu
    int strz = p.czarne(xo + SYP30_KROPKA_R + 2, yo - 4, xo + SYP30_KROPKA_R + 4 + SYP30_STRZALKA_W, yo + 4);
    SPRAWDZ(odstaje ? strz >= 15 : strz == 0);
    SPRAWDZ(xo + SYP30_KROPKA_R + 3 + SYP30_STRZALKA_W < SYP30_X_SLOWA_MIN);
    if (odstaje) SPRAWDZ(d.w[0].slowo == SlowoSyp30::WYZEJ && d.w[0].wartosc == przecinek1(nm.t.p90 + 50.0));
  }
  // średnia poza osią (noc 15 bardzo wysoko): strzałka przy górnej krawędzi pola nad nocą 15
  {
    s = noce(30, 20, 45, 700);
    s[15].t = 200;
    d = budujSyp30(s, DZIS, {});
    SPRAWDZ(pozaOsiaSyp30(d.w[0], 0, d.w[0].sr[15]) == 1);
    PlotnoTest p;
    rysujSyp30(p, d);
    const int gy = SYP30_RZAD_Y0 + SYP30_WYKRES_GORA, x15 = xNocySyp30(15);
    SPRAWDZ(p.czarne(x15 - 4, gy, x15 + 5, gy + SYP30_STRZALKA_H) >= 15);
    SPRAWDZ(p.czarne(xNocySyp30(5) - 4, gy, xNocySyp30(5) + 5, gy + SYP30_STRZALKA_H) == 0);
  }
  // luki: SYP30_LUKA_MAX nocy bez odczytu mostkowane, dłuższa luka przerywa linię
  for (int luka : {SYP30_LUKA_MAX, SYP30_LUKA_MAX + 1}) {
    std::set<int> bez;
    for (int i = 0; i < luka; i++) bez.insert(15 + i);
    d = budujSyp30(noce(30, 20, 45, 700, bez), DZIS, {});
    PlotnoTest p;
    rysujSyp30(p, d);
    const int gy = SYP30_RZAD_Y0 + SYP30_WYKRES_GORA;
    const int xs = (xNocySyp30(15) + xNocySyp30(16)) / 2;
    int pion = p.czarne(xs, gy, xs + 1, gy + SYP30_WYKRES_H);
    SPRAWDZ(luka <= SYP30_LUKA_MAX ? pion >= 2 : pion == 0);
  }
  // legenda mówi wprost: kropka, średnia, pasmo; mieści się nad stopką w 744 px
  int szerLeg = 2 * SYP30_KROPKA_R + 10 + szer(F_P22B, "ostatnia noc") + 26 + 40 + szer(F_P22B, "średnia z 7 nocy") + 26 +
                40 + szer(F_P22B, "zwykłe pasmo");
  SPRAWDZ(szerLeg <= 744);
  PlotnoTest p, ref;
  rysujSyp30(p, budujSyp30(noce(30, 20, 45, 700), DZIS, {}));
  int x = 28 + 2 * SYP30_KROPKA_R + 10;
  x = tekst(ref, F_P22B, x, SYP30_LEGENDA_BAZA, "ostatnia noc") + 26;
  x = tekst(ref, F_P22B, x + 40, SYP30_LEGENDA_BAZA, "średnia z 7 nocy") + 26;
  int xp = x;
  tekst(ref, F_P22B, x + 40, SYP30_LEGENDA_BAZA, "zwykłe pasmo");
  SPRAWDZ(p.czarne(xp + 40, 400, 800, 428) == ref.czarne(xp + 40, 400, 800, 428));
  SPRAWDZ(p.czarne(xp - 200, 400, xp, 428) > 0);
}

// ---------- stopka, belka, szerokości ----------
// wymaganie: R-KOK-29
static void testR_KOK_29_stopkaIBelka() {
  SPRAWDZ(stopkaSyp30(false, false) == std::vector<std::string>({"◀ 7 nocy", "▶ ustawienia", "↻ ekran dnia"}));
  SPRAWDZ(stopkaSyp30(true, false)[0] == "◀ sprawy" && stopkaSyp30(true, true)[0] == "◀ sprawy");
  SPRAWDZ(stopkaSyp30(false, true)[0] == "◀ liczby");
  for (int m = 0; m < 4; m++) SPRAWDZ(szerStopki(stopkaSyp30(m & 1, m & 2)) <= 744);
  // podpisy sąsiednich stron prowadzą do sypialni
  SPRAWDZ(stopkaK2(false, false, true)[1] == "▶ sypialnia · 2 s raport");
  SPRAWDZ(stopkaK2(true, false, true)[1] == "▶ sprawy · 2 s raport" && stopkaK2(false, true, true)[1] == "▶ liczby · 2 s raport");
  SPRAWDZ(stopkaSpraw(2, 2, false, true)[1] == "▶ sypialnia" && stopkaSpraw(1, 2, false, true)[1] == "▶ następna");
  SPRAWDZ(stopkaU01(true, false, true)[0].rfind("◀ sypialnia", 0) == 0);
  SPRAWDZ(podsumowanie::budujLiczby(reguly::Tryb::CPAP, false, false, true).stopkaLewo.find("▶ sypialnia") != std::string::npos);
  SPRAWDZ(podsumowanie::budujLiczby(reguly::Tryb::CPAP, false, true, true).stopkaLewo.find("▶ sprawy") != std::string::npos);
  for (bool spr : {false, true})
    for (bool l : {false, true}) {
      SPRAWDZ(szerStopki(stopkaK2(spr, l, true)) <= 744 && szerStopki(stopkaU01(spr, l, true)) <= 744);
      SPRAWDZ(szerStopki(stopkaSpraw(1, 1, l, true)) <= 744);
    }
  // belka: nazwa z numerem, środek z zakresem albo `30 nocy`
  ustawSypialnieWKokpicie(true);
  DaneSyp30 d = budujSyp30(noce(30, 20, 45, 700), DZIS, {});
  Belka b = belkaSyp30(d);
  SPRAWDZ(b.nazwa == "Sypialnia 3 / 4" && b.srodek == "30 nocy · 16.10–14.11" && srodekBelki(b) == b.srodek);
  ustawSypialnieWKokpicie(false);
  // najdłuższe słowo i najdłuższe wartości mieszczą się w swoich kolumnach
  for (SlowoSyp30 x : {SlowoSyp30::BRAK_OSTATNIEJ, SlowoSyp30::UCZE_SIE, SlowoSyp30::JAK_ZWYKLE, SlowoSyp30::WYZEJ, SlowoSyp30::NIZEJ})
    SPRAWDZ(772 - szer(F_P22B, tekstSlowaSyp30(x)) >= SYP30_X_SLOWA_MIN);
  SPRAWDZ(SYP30_X_WYKRESU + SYP30_W_WYKRESU + 16 <= SYP30_X_SLOWA_MIN);
  for (const char* w : {"–", "-10,5", "45,5", "100", "9999"}) {
    int x = 28 + szer(F_P34B, w) + 6 + szer(F_P22B, "ppm");
    SPRAWDZ(x <= SYP30_X_WYKRESU - 12);
    if (x > SYP30_X_WYKRESU - 12) printf("  wartość %s ppm kończy się na %d\n", w, x);
  }
  for (const char* n : {"TEMPERATURA", "WILGOTNOŚĆ", "CO2"}) SPRAWDZ(28 + szer(F_P22B, n, 2) <= SYP30_X_WYKRESU - 12);
  // rysunek: strefa treści nad kreską stopki (434), nic z treści na belce; legenda nad stopką
  PlotnoTest p;
  rysujSyp30(p, budujSyp30(noce(30, 20, 45, 700), DZIS, {}));
  SPRAWDZ(p.czarne(0, 425 + 8, 800, 434) == 0);  // pas między legendą a kreską stopki (dolne wydłużniki ≤ 8 px)
  SPRAWDZ(p.czarne(0, 434, 800, 436) == 1600);    // kreska stopki 2 px
  SPRAWDZ(p.czarne(28, 400, 400, 428) > 100);     // legenda
}

// ---------- dyrygent na atrapach: ciąg ▶, numer 4 / 4, powrót po restarcie ----------
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t ep(int d, int h, int m) {
  struct tm t = {};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 10;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  return (uint32_t)timegm(&t);
}
static void przycisk(Panel& p, uint32_t kiedy, uint8_t bit, bool dlugo = false) {
  p.zegar.epoch = kiedy;
  p.zegar.msTeraz = 0;
  p.d.z.clear();
  p.sen.snow = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = {{0, bit}, {dlugo ? 2500u : 300u, 0}};
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static Ekran ekran(const Panel& p) { return (Ekran)p.start.stan.ekran; }
static bool wLogu(const Panel& p, const std::string& t) {
  for (auto& l : p.d.z)
    if (l.find(t) != std::string::npos) return true;
  return false;
}
static void panel(Panel& p, bool sypialnia) {
  p.zegar.epoch = ep(14, 10, 0);
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.pliki.pliki["/konfiguracja.txt"] = "";
  if (sypialnia) {
    std::string s;
    for (auto& n : noce(30, 20, 45, NAN)) s += zapiszSypialnie(n) + "\n";
    p.pliki.pliki["/sypialnia.csv"] = s;
  }
}

// wymaganie: R-KOK-29
static void testR_KOK_29_dyrygent() {
  for (bool syp : {false, true}) {
    Panel p;
    panel(p, syp);
    uint32_t t = ep(14, 10, 0);
    std::vector<Ekran> ciag;
    for (int i = 0; i < 10 && ekran(p) != Ekran::USTAWIENIA; i++) {
      przycisk(p, t += 30, BIT_PRAWY);
      ciag.push_back(ekran(p));
    }
    bool bylaSyp = false;
    for (Ekran e : ciag) bylaSyp = bylaSyp || e == Ekran::SYPIALNIA;
    SPRAWDZ(bylaSyp == syp);
    SPRAWDZ(ekran(p) == Ekran::USTAWIENIA);
    // ◀ z U-01: sypialnia (z danymi) albo strona przed U-01 jak dotąd
    przycisk(p, t += 30, BIT_LEWY);
    SPRAWDZ((ekran(p) == Ekran::SYPIALNIA) == syp);
    if (syp) SPRAWDZ(wLogu(p, "sypialnia 30 nocy: TEMPERATURA") && wLogu(p, "sypialnia 30 nocy: CO2 –, brak odczytu w 30 nocach"));
  }
  // powrót po restarcie z pamięci RTC: przycisk na stronie sypialni działa od niej (▶ U-01), a zegar przed 5 min ją rysuje
  {
    Panel p;
    panel(p, true);
    p.start.stan.ekran = (uint8_t)Ekran::SYPIALNIA;
    przycisk(p, ep(14, 10, 5), BIT_PRAWY);
    SPRAWDZ(ekran(p) == Ekran::USTAWIENIA);
    p.start.stan.ekran = (uint8_t)Ekran::SYPIALNIA;
    przycisk(p, ep(14, 10, 6), BIT_LEWY);
    SPRAWDZ(ekran(p) == Ekran::K2 || ekran(p) == Ekran::SPRAWY);
  }
  // strona sypialni w RTC, ale danych już nie ma: ekran dnia, nie pusta strona
  {
    Panel p;
    panel(p, false);
    p.start.stan.ekran = (uint8_t)Ekran::SYPIALNIA;
    p.zegar.epoch = ep(14, 10, 8);
    p.zegar.msTeraz = 0;
    p.d.z.clear();
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    p.start.budzace = 0;
    p.przyciski.skrypt.clear();
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    Dyrygent(p.adaptery(), KONF).wybudzenie();
    SPRAWDZ(ekran(p) != Ekran::SYPIALNIA);
  }
  // dyrygent wyklucza noce „poza domem” z pasma (norma sypialni 4.4, jak S2c): 20 takich nocy → `uczę się`
  for (bool zPoza : {false, true}) {
    Panel p;
    panel(p, true);
    std::string s;
    for (auto& n : noce(30, 20, 45, 700)) s += zapiszSypialnie(n) + "\n";
    p.pliki.pliki["/sypialnia.csv"] = s;
    if (zPoza) {
      std::string plik = "#cpap-eink czynniki 1\ndata;stan;kody;warstwa;termin;pokazano\n";
      for (int i = 1; i <= 20; i++) plik += dodajDni(DZIS, -i) + ";O;P;Z;-1;0\n";
      plik += "#koniec 20\n";
      p.pliki.pliki["/dane/czynniki.csv"] = plik;
    }
    p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    przycisk(p, ep(14, 10, 5), BIT_LEWY);
    SPRAWDZ(ekran(p) == Ekran::SYPIALNIA);
    SPRAWDZ(wLogu(p, zPoza ? "noce poza domem 20" : "noce poza domem 0"));
    SPRAWDZ(wLogu(p, "sypialnia 30 nocy: TEMPERATURA") && wLogu(p, zPoza ? ", uczę się" : "zwykle"));
    SPRAWDZ(zPoza == !wLogu(p, "zwykle"));
  }
  // przed 06:00 dyrygent rysuje `noc trwa` (noc z datą poranka dziś jeszcze się nie zamknęła), po 06:00 nie
  for (int godz : {3, 9}) {
    Panel p;
    panel(p, false);
    std::string s;
    for (auto& n : noce(30, 20, 45, 700, {29})) s += zapiszSypialnie(n) + "\n";
    p.pliki.pliki["/sypialnia.csv"] = s;
    p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    przycisk(p, ep(14, godz, 0), BIT_LEWY);
    SPRAWDZ(ekran(p) == Ekran::SYPIALNIA);
    SPRAWDZ(wLogu(p, godz < 6 ? "TEMPERATURA –, noc trwa" : "TEMPERATURA –, brak odczytu"));
  }
  // stopka strony sypialni z dyrygenta przy sprawach: `◀ sprawy`, a ◀ naprawdę prowadzi na „Otwarte sprawy”
  {
    static reguly::Zestaw zw = atrapa_b5::zestaw();
    sprawy::zrodloTestowe.wyniki = [](const reguly::Kontekst&) {
      std::vector<reguly::Wynik> w;
      for (auto& k : zw.katalog) w.push_back(atrapa_b5::wynik(zw, k.id, k.wariant, true, k.id == "L-03"));
      return w;
    };
    sprawy::zrodloTestowe.zestaw = &zw;
    sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(zw);
    for (bool publ : {false, true}) {
      Panel p;
      panel(p, true);
      if (publ) p.pliki.pliki["/konfiguracja.txt"] = "profil=publiczny\n";
      uint32_t t = ep(14, 10, 0);
      bool bylySprawy = false, bylaSyp = false;
      for (int i = 0; i < 12 && ekran(p) != Ekran::USTAWIENIA; i++) {
        przycisk(p, t += 30, BIT_PRAWY);
        bylySprawy = bylySprawy || ekran(p) == Ekran::SPRAWY;
        if (ekran(p) == Ekran::SYPIALNIA) {
          bylaSyp = true;
          SPRAWDZ(wLogu(p, "stopka ◀ sprawy · ▶ ustawienia"));
        }
      }
      SPRAWDZ(bylySprawy && bylaSyp);
      przycisk(p, t += 30, BIT_LEWY);
      SPRAWDZ(ekran(p) == Ekran::SYPIALNIA);
      przycisk(p, t += 30, BIT_LEWY);
      SPRAWDZ(ekran(p) == Ekran::SPRAWY);
    }
    sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
  }
  // bez spraw: `◀ 7 nocy`, w profilu publicznym `◀ liczby`, i ◀ prowadzi tam
  for (bool publ : {false, true}) {
    Panel p;
    panel(p, true);
    if (publ) p.pliki.pliki["/konfiguracja.txt"] = "profil=publiczny\n";
    p.start.stan.ekran = (uint8_t)Ekran::USTAWIENIA;
    przycisk(p, ep(14, 10, 5), BIT_LEWY);
    SPRAWDZ(ekran(p) == Ekran::SYPIALNIA && wLogu(p, publ ? "stopka ◀ liczby" : "stopka ◀ 7 nocy"));
    przycisk(p, ep(14, 10, 6), BIT_LEWY);
    SPRAWDZ(ekran(p) == (publ ? Ekran::LICZBY : Ekran::K2));
  }
  ustawSypialnieWKokpicie(false);
}

int main() {
  testR_KOK_29_ciagStron();
  testR_KOK_30_numeryWTekstach();
  testR_KOK_31_brakOdczytu();
  testR_KOK_32_pasmoISlowo();
  testR_KOK_33_luki();
  testR_KOK_31_daneZPliku();
  testR_KOK_33_osISrednia();
  testR_KOK_29_stopkaIBelka();
  testR_KOK_29_dyrygent();
  printf("testy sypialni 30 nocy: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
