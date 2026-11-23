// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe limitu ładowania, część 2 (ADR-0046), 16.11.2026. Niezależny model SY6974B i NVS, własna funkcja
// kod→mV (nie używa napiecieVreg/kodVregNieWyzej z ladowarka.cpp do oceny). Przegląd:
//  A) jedno wybudzenie: REG04 0…255 × wpis NVS (brak, zły znacznik, złe bity 23:16, REG04 wpisu 0…255) × opcja
//     wł./wył. × kabel tak/nie × REG05 {8F,9F,AF,BF} × błąd wstrzyknięty na każdej operacji (I2C i NVS);
//  B) sekwencje 3 wybudzeń: kod VREG 0…31 × wpis (brak, zły, kod 0…31) × REG05 × opcja 2^3 × kabel {TTT,TFT,FTT,TTF}
//     × jeden błąd w dowolnej operacji sekwencji (6 rodzajów), potem 2 czyste wybudzenia z opcją wyłączoną.
// Własności W1…W6 (W1 granica VREG, W2 koniec po wyłączeniu, W3…W6 opisane przy liczeniu); przypadki „podniesienia” i wyjątki W6 są liczone i wypisywane, nie
// oblewają testu, jeśli należą do klas opisanych w ADR-0046 (wpis z karty, wpis z NVS sprzed sekwencji).
// Zmiana 16.11.2026 (poprawki błędu odczytu NVS i wpisu poniżej limitu): oba liczniki usterek, „wpis nadpisany 4208 mV po błędzie
// odczytu NVS” i „koniec poniżej limitu przez wpis z kodem < limitu”, są teraz sprawdzeniami
// blokującymi (muszą wyjść 0). Klasa oczekiwana W2 „przywrócenie z wpisu” obejmuje wpis z kodem < limitu tak samo jak
// wpis z kodem > 01011: według ADR-0046 pkt 7 oba są uszkodzone i wracają do 4208 mV z karty.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <map>
#include <string>
#include "adaptery_atrapy.h"
#include "ladowarka.h"

using namespace ladowarka;

static int mvZKodu(int kod) {  // własna mapa wg karty SY6974B Rev. 0.9
  if (kod >= 24) return 4624;
  if (kod == 15) return 4352;
  return 3856 + 32 * kod;
}
static int mvReg(uint8_t reg04) { return mvZKodu(reg04 >> 3); }
static const int LIM_MV = 3920, DOM_MV = 4208;

struct Zanik {};
enum Rodzaj { R_BRAK, R_ODCZYT, R_ZAPIS_ODRZ, R_ZAPIS_NACK, R_ZANIK, R_NVS_GUBI, R_NVS_ODCZYT0 };

struct Wstrzykniecie {
  int op = 0, nr = -1;
  Rodzaj r = R_BRAK;
  bool tu() { return ++op == nr; }
};

struct ModelSY : public Rejestry {
  uint8_t r[12] = {0x04, 0x1A, 0x88, 0x22, 0x58, 0x8F, 0x62, 0x4C, 0xBC, 0x00, 0x80, 0x40};
  Wstrzykniecie* w = nullptr;
  int zapisow = 0, naruszenW3 = 0, naruszenW5 = 0, maxMvZapisu = 0;
  bool czytaj(uint8_t rej, uint8_t& v) override {
    bool t = w->tu();
    if (t && w->r == R_ZANIK) throw Zanik();
    if (t && w->r == R_ODCZYT) return false;
    if (rej > 0x0B) return false;
    v = r[rej];
    return true;
  }
  bool zapisz(uint8_t rej, uint8_t v) override {
    bool t = w->tu();
    if (t && w->r == R_ZANIK) throw Zanik();
    zapisow++;
    if (rej == 0x04) {
      if ((uint8_t)((v ^ r[4]) & ~0xF8) != 0) naruszenW3++;
      if (mvReg(v) > maxMvZapisu) maxMvZapisu = mvReg(v);
    } else if (rej == 0x05) {
      if ((uint8_t)((v ^ r[5]) & ~0x30) != 0) naruszenW3++;
      if ((v & 0x30) != 0) naruszenW5++;  // firmware nie może zapisać niezerowego licznika (ani włączyć)
    } else {
      naruszenW3++;
    }
    if (t && w->r == R_ZAPIS_ODRZ) return false;
    if (rej < 12) r[rej] = v;
    if (t && w->r == R_ZAPIS_NACK) return false;
    return true;
  }
};

struct NvsModel : public AtrapaNvs {
  Dziennik dz;
  NvsModel() : AtrapaNvs(dz) {}
  bool jest = false;
  uint32_t v = 0;
  int zapisowNvs = 0;
  bool zgubionyOdczyt = false;
  // Zmiana 16.11.2026: „wpis nadpisany po błędzie odczytu” liczony po samym zdarzeniu, nie po skutku: w tym samym wybudzeniu co
  // zgubiony odczyt ważnego wpisu firmware zapisuje do NVS inną wartość w miejsce tego wpisu. Wcześniejszy licznik
  // („odczyt zgubiony i koniec 4208 mV”) po poprawce liczył też przypadki, w których wpis 4208 mV „z karty” zapisał
  // wcześniej sam firmware przy podpisie limitu bez wpisu (klasa oczekiwana), a zgubiony odczyt tylko przerwał później
  // jedno wybudzenie.
  bool zgubionyTeraz = false, nadpisanyPoBledzie = false;
  Wstrzykniecie* w = nullptr;
  uint32_t liczbaU(const char*, const char*, uint32_t dom) override {
    bool t = w->tu();
    if (t && w->r == R_ZANIK) throw Zanik();
    if (t && w->r == R_NVS_ODCZYT0) {
      if (jest && (v >> 24) == 0x5Au && (v & 0xFF0000u) == 0) zgubionyOdczyt = zgubionyTeraz = true;  // ważny wpis był, firmware dostał „brak”
      return dom;
    }
    return jest ? v : dom;
  }
  void ustawLiczbeU(const char*, const char*, uint32_t x) override {
    bool t = w->tu();
    if (t && w->r == R_ZANIK) throw Zanik();
    zapisowNvs++;
    if (zgubionyTeraz && jest && (v >> 24) == 0x5Au && (v & 0xFF0000u) == 0 && x != v) nadpisanyPoBledzie = true;
    if (t && w->r == R_NVS_GUBI) return;
    jest = true;
    v = x;
  }
  void usun(const char*, const char*) override {
    bool t = w->tu();
    if (t && w->r == R_ZANIK) throw Zanik();
    zapisowNvs++;
    if (t && w->r == R_NVS_GUBI) return;
    jest = false;
  }
};

struct Swiat {
  Wstrzykniecie w;
  ModelSY u;
  NvsModel n;
  PamiecZastanychNvs pam{n};
  Swiat() { u.w = &w; n.w = &w; }
  bool wybudz(bool wl, bool kabel) {
    n.zgubionyTeraz = false;
    try {
      limitLadowania(u, pam, wl, kabel);
      return true;
    } catch (Zanik&) {
      return false;
    }
  }
};

static bool pelny = false;  // BRZEGOWE2_PELNY=1: pełny przegląd (ok. 4 min); domyślnie co trzeci bajt i REG05 8F, 9F
// CPAP_TESTY_SANITIZERY=1 (ustawia tools/jakosc/sprawdz.sh sanitizery): mniej wartości rejestru i wpisu, każdy rodzaj
// kontroli zostaje (każdy rodzaj i każde miejsce błędu, opcje, kable, REG05, wszystkie własności W1…W6, P3-a, P3-b).
// Pod ASan każdy wyjątek `Zanik` kosztuje tyle, że tryb domyślny trwał 10–21 min (pomiar CI 23.11.2026); tryb
// domyślny (co trzeci bajt) bez sanitizera dalej biegnie w tools/prototyp/testy.sh (ok. 10 s).
static bool podSanitizerami = false;
// Kody VREG z osobnym znaczeniem w ADR-0046 i w ocenie W2/W6: 0, 1, kod limitu 2 (3920 mV), 3, 11 (4208 mV), 12 (pierwszy
// wyżej niż wpis z karty), 15 (osobny wpis karty, 4352 mV), 23, 24 (≥ 24 = 4624 mV), 31.
static bool kodSzczegolny(int kod) {
  return kod == 0 || kod == 1 || kod == 2 || kod == 3 || kod == 11 || kod == 12 || kod == 15 || kod == 23 || kod == 24 ||
         kod == 31;
}
// Faza A: wartości REG04 (r4 i REG04 wpisu) brane pod sanitizerami: co 30 i po jednej z każdego kodu szczególnego.
static bool bajtWProbce(int b, int krok) {
  if (!podSanitizerami || b < 0) return true;
  if (b % 30 == 0) return true;
  return kodSzczegolny(b >> 3) && (b - krok < 0 || ((b - krok) >> 3) != (b >> 3));  // pierwsza odwiedzona w kodzie
}
// Faza B: kody VREG (kod i kod wpisu) pod sanitizerami: tylko szczególne, brak wpisu i zły wpis zawsze.
static bool kodWProbce(int kod) { return !podSanitizerami || kod < 0 || kodSzczegolny(kod); }
static long long przebiegow = 0;
static long long nW1 = 0, nW1zapis = 0, nW1wl = 0, nW3 = 0, nW4 = 0, nW5 = 0;
static long long podnZWpisu = 0, podnBezWpisu = 0, w6Wpis = 0, w6Utkniety = 0, w6NizejZWpisu = 0;
static long long podnNadpisany = 0, podnInne = 0, w6NizejInne = 0;
static std::map<std::string, long long> przyklady;
static void przyklad(const char* klasa, const char* nazwy, int a, int b, int c, int d, int e) {
  char t[300];
  snprintf(t, sizeof t, "[%s] %d %d %d %d %d", nazwy, a, b, c, d, e);
  static std::map<std::string, int> naKlase;
  std::string k = std::string(klasa) + ": " + t;
  if (przyklady.count(k) || naKlase[klasa]++ < 8) przyklady[k]++;
}

// wpis: -1 brak, -2 zły znacznik, -3 złe bity 23:16, 0…255 = REG04 wpisu (REG05 wpisu 0x8F)
static void ustawWpis(NvsModel& n, int wpis, uint8_t reg04) {
  n.jest = wpis != -1;
  if (wpis == -2) n.v = 0x5B008F00u | reg04;
  else if (wpis == -3) n.v = 0x5A018F00u | reg04;
  else if (wpis >= 0) n.v = 0x5A008F00u | (uint32_t)wpis;
}

static void fazaA() {
  static const uint8_t R05[] = {0x8F, 0x9F, 0xAF, 0xBF};
  const int krok = pelny ? 1 : 3;
  for (int r4 = 0; r4 < 256; r4 += krok)
    for (int wpis = -3; wpis < 256; wpis += (wpis < 0 ? 1 : krok))
      for (uint8_t r5 : R05)
        for (int wl = 0; wl < 2; wl++)
          for (int kab = 0; kab < 2; kab++)
            for (int rodz = R_BRAK; rodz <= R_NVS_ODCZYT0; rodz++)
              for (int nr = (rodz == R_BRAK ? -1 : 1); nr <= (rodz == R_BRAK ? -1 : 14); nr++) {
                if (!bajtWProbce(r4, krok) || !bajtWProbce(wpis, krok)) continue;
                Swiat s;
                s.u.r[4] = (uint8_t)r4;
                s.u.r[5] = r5;
                ustawWpis(s.n, wpis, (uint8_t)r4);
                s.w.r = (Rodzaj)rodz;
                s.w.nr = nr;
                const int start = mvReg((uint8_t)r4);
                const uint8_t wd0 = r5 & 0x30;
                s.wybudz(wl, kab);
                przebiegow++;
                const int kon = mvReg(s.u.r[4]);
                const int gr = start > DOM_MV ? start : DOM_MV;
                if (kon > gr) { nW1++; przyklad("W1", "r4 wpis wl rodz nr", r4, wpis, wl, rodz, nr); }
                if (s.u.maxMvZapisu > gr) nW1zapis++;
                if (wl && kon > start) { nW1wl++; przyklad("W1-wl", "r4 wpis kab rodz nr", r4, wpis, kab, rodz, nr); }
                if (s.u.naruszenW3) nW3++;
                if (s.u.naruszenW5 || (wd0 == 0 && (s.u.r[5] & 0x30) != 0)) nW5++;
                if (!wl && wpis < 0 && (s.u.zapisow || s.n.zapisowNvs)) {
                  // W4 dotyczy braku wpisu; wpis z błędnym znacznikiem jest „brakiem” dla firmware
                  nW4++;
                  przyklad("W4", "r4 wpis r5 rodz nr", r4, wpis, r5, rodz, nr);
                }
              }
}

static void fazaB() {
  static const uint8_t R05[] = {0x8F, 0x9F, 0xAF, 0xBF};
  static const int KABLE[] = {0x7, 0x5, 0x6, 0x3};  // bit i = kabel w wybudzeniu i: TTT, TFT, FTT(bit0=F), TTF
  for (int kod = 0; kod < 32; kod++)
    for (int wpis = -3; wpis < 32; wpis++) {
      if (wpis == -2) continue;  // -2 i -3 to ten sam przypadek dla firmware; -3 zostaje
      if (!kodWProbce(kod) || !kodWProbce(wpis)) continue;
      for (uint8_t r5 : R05) {
        if (!pelny && (r5 == 0xAF || r5 == 0xBF)) continue;
        for (int opcje = 0; opcje < 8; opcje++)
          for (int kable : KABLE)
            for (int rodz = R_BRAK; rodz <= R_NVS_ODCZYT0; rodz++)
              for (int nr = (rodz == R_BRAK ? -1 : 1); nr <= (rodz == R_BRAK ? -1 : 30); nr++) {
                Swiat s;
                const uint8_t r4 = (uint8_t)((kod << 3) | 5);
                s.u.r[4] = r4;
                s.u.r[5] = r5;
                ustawWpis(s.n, wpis >= 0 ? ((wpis << 3) | 5) : wpis, r4);
                s.w.r = (Rodzaj)rodz;
                s.w.nr = nr;
                const int start = mvReg(r4);
                const int gr = start > DOM_MV ? start : DOM_MV;
                const uint8_t wd0 = r5 & 0x30;
                bool wd00 = wd0 == 0;
                for (int i = 0; i < 3; i++) {
                  s.wybudz(opcje >> i & 1, kable >> i & 1);
                  if (wd00 && (s.u.r[5] & 0x30)) nW5++;
                  wd00 = (s.u.r[5] & 0x30) == 0;
                }
                przebiegow++;
                if (mvReg(s.u.r[4]) > gr || s.u.maxMvZapisu > gr) {
                  nW1++;
                  przyklad("W1-B", "kod wpis opcje rodz nr", kod, wpis, opcje, rodz, nr);
                }
                if (s.u.naruszenW3) nW3++;
                if (s.u.naruszenW5) nW5++;
                if (opcje == 0 && wpis == -1 && (s.u.zapisow || s.n.zapisowNvs)) nW4++;
                // koniec po wyłączeniu opcji i dwóch czystych wybudzeniach
                s.w.nr = -1;
                s.wybudz(false, true);
                s.wybudz(false, true);
                const int kon = mvReg(s.u.r[4]);
                if (s.u.maxMvZapisu > gr || kon > gr) nW1++;
                if (s.n.nadpisanyPoBledzie) {  // blokujące: niezależnie od tego, czy koniec jest wyżej
                  podnNadpisany++;
                  przyklad("wpis nadpisany po bledzie odczytu", "kod wpis opcje rodz nr", kod, wpis, opcje, rodz, nr);
                }
                if (kon > start) {
                  // oczekiwane: przywrócenie do wartości z wpisu (najwyżej 4208 mV) albo wpis z karty przy podpisie
                  // wpis z kodem > 01011 albo < kodu limitu (2) = uszkodzony: przywrócenie do 4208 mV (ADR-0046 pkt 7)
                  const int zWpisu = wpis >= 0 ? mvZKodu((wpis > 11 || wpis < 2) ? 11 : wpis) : 0;
                  const bool podpisNaStarcie = kod == 2;  // licznik 01…11 firmware sam wyłącza w pierwszym wybudzeniu z wpisem
                  if (wpis >= 0 && kon <= zWpisu) podnZWpisu++;
                  else if (s.n.nadpisanyPoBledzie) {
                    // wpis nadpisany po błędzie odczytu (blokujące od poprawki, policzone wyżej): wpis był, odczyt NVS zawiódł, a firmware w tym samym
                    // wybudzeniu zapisał w jego miejsce inną wartość (przed poprawką: zKarty() = 4208 mV)
                  } else if (podpisNaStarcie && kon == DOM_MV) {
                    podnBezWpisu++;  // ADR-0046 pkt 4 i 7: podpis limitu bez (aktualnego) wpisu → wpis z karty 4208 mV
                  } else {
                    podnInne++;
                    przyklad("W2 inne", "kod wpis opcje rodz nr", kod, wpis, opcje, rodz, nr);
                  }
                }
                const bool wpisZostal = s.n.jest && (s.n.v >> 24) == 0x5A && (s.n.v & 0xFF0000u) == 0;
                if (wpisZostal) { w6Wpis++; przyklad("W6 wpis", "kod wpis opcje rodz nr", kod, wpis, opcje, rodz, nr); }
                if (kon <= LIM_MV && start > LIM_MV) {
                  w6Utkniety++;
                  przyklad("W6 utkniety na limicie", "kod wpis opcje rodz nr", kod, wpis, opcje, rodz, nr);
                }
                if (kon < LIM_MV && start >= LIM_MV) {
                  if (wpis >= 0 && mvZKodu(wpis) < LIM_MV && kon == mvZKodu(wpis)) w6NizejZWpisu++;  // wpis z kodem < limitu
                  else { w6NizejInne++; przyklad("W6 nizej inne", "kod wpis opcje rodz nr", kod, wpis, opcje, rodz, nr); }
                }
              }
      }
    }
}

int main() {
  pelny = getenv("BRZEGOWE2_PELNY") != nullptr;
  podSanitizerami = !pelny && getenv("CPAP_TESTY_SANITIZERY") != nullptr;
  printf("tryb: %s\n", pelny ? "pelny" : podSanitizerami ? "pod sanitizerami (probka wartosci)"
                                                         : "szybki (BRZEGOWE2_PELNY=1: pelny)");
  fazaA();
  long long poA = przebiegow;
  printf("faza A: %lld wybudzen; W1 %lld (zapis %lld), W1-wl (podniesienie przy wlaczonej) %lld, W3 %lld, W4 %lld, W5 %lld\n",
         poA, nW1, nW1zapis, nW1wl, nW3, nW4, nW5);
  long long a1 = nW1 + nW1zapis + nW1wl, a3 = nW3, a4 = nW4, a5 = nW5;
  nW1 = nW1zapis = nW1wl = nW3 = nW4 = nW5 = 0;
  fazaB();
  printf("faza B: %lld sekwencji; W1 %lld, W3 %lld, W4 %lld, W5 %lld\n", przebiegow - poA, nW1, nW3, nW4, nW5);
  printf("W2: koniec wyzej niz start: przywrocenie wartosci z wpisu sprzed sekwencji (<= 4208 mV) %lld; podpis limitu "
         "bez wpisu -> 4208 mV z karty (ADR-0046 pkt 4, 7) %lld; (blokujace) wpis byl, odczyt NVS zawiodl "
         "i firmware w tym wybudzeniu zapisal w jego miejsce inna wartosc %lld; inne %lld\n", podnZWpisu, podnBezWpisu, podnNadpisany, podnInne);
  printf("W6: wpis zostal %lld, utkniety na limicie (start > 3920 mV) %lld, ponizej limitu z wpisu o kodzie < limitu "
         "(blokujace) %lld, ponizej limitu inne %lld\n", w6Wpis, w6Utkniety, w6NizejZWpisu, w6NizejInne);
  for (auto& p : przyklady) printf("  %s  (x%lld)\n", p.first.c_str(), p.second);
  const bool zle = a1 || a3 || a4 || a5 || nW1 || nW3 || nW4 || nW5 || podnInne || w6Wpis || w6Utkniety || w6NizejInne ||
                   podnNadpisany || w6NizejZWpisu;  // blokujące od poprawki 16.11.2026
  printf(zle ? "WYNIK: NARUSZENIA\n" : "WYNIK: W1 W3 W4 W5 bez naruszen\n");
  return zle ? 1 : 0;
}
