// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe limitu ładowania (ADR-0046, R-DANE-66…R-DANE-68), 16.11.2026. Własny model układu
// SY6974B (niezależny od AtrapaLadowarki) według karty Rev. 0.9: wstrzykiwanie błędu na KAŻDYM kroku
// sekwencji (odczyt, zapis nieprzyjęty, zapis przyjęty mimo NACK, zanik zasilania), zawodna NVS, wpis `lad_zast`
// nieaktualny / z innego egzemplarza / z kodem 01111 i wyższym, inny układ i wisząca magistrala, liczba operacji
// na wybudzenie, przebieg kabel → bateria → kabel, restart między krokami.
// Test oznaczony „[USTERKA]” sprawdza dawną usterkę, poprawioną 16.11.2026; od poprawki przechodzi.
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "adaptery_atrapy.h"
#include "ladowarka.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)

using namespace ladowarka;

struct ZanikZasilania {};  // rzucany przez model w chwili „wyciągnięcia wtyczki” (restart ESP32 w połowie sekwencji)

// Model układu: rejestry 00…0B, wartości po starcie zasilania z karty [K]; każdy błąd wstrzykiwany na operacji nr `n`
// (liczonej od 1 łącznie dla odczytów i zapisów).
enum class Blad { BRAK, ODCZYT, ZAPIS_ODRZUCONY, ZAPIS_PRZYJETY_NACK, ZANIK };
struct Model : public Rejestry {
  uint8_t r[12] = {0x04, 0x1A, 0x88, 0x22, 0x58, 0x8F, 0x62, 0x4C, 0xBC, 0x00, 0x80, 0x40};  // jeden egzemplarz panelu 16.11
  int op = 0, odczyty = 0, bladNr = -1;
  Blad blad = Blad::BRAK;
  int stalyBajt = -1;  // >= 0: wisząca magistrala, każdy odczyt zwraca ten bajt, zapisy „udane” bez skutku
  std::vector<std::pair<uint8_t, uint8_t>> zapisy;
  bool czytaj(uint8_t rej, uint8_t& w) override {
    op++;
    odczyty++;
    if (op == bladNr && blad == Blad::ZANIK) throw ZanikZasilania();
    if (op == bladNr && blad == Blad::ODCZYT) return false;
    if (stalyBajt >= 0) { w = (uint8_t)stalyBajt; return true; }
    if (rej > 0x0B) return false;
    w = r[rej];
    return true;
  }
  bool zapisz(uint8_t rej, uint8_t w) override {
    op++;
    if (op == bladNr && blad == Blad::ZANIK) throw ZanikZasilania();
    zapisy.push_back({rej, w});
    if (op == bladNr && blad == Blad::ZAPIS_ODRZUCONY) return false;
    if (stalyBajt >= 0) return true;
    if (rej <= 0x07) r[rej] = w;
    if (op == bladNr && blad == Blad::ZAPIS_PRZYJETY_NACK) return false;
    return true;
  }
  int vreg() const { return napiecieVreg((r[4] & 0xF8) >> 3); }
};

// NVS, której zapis ginie po cichu albo w której zanik zasilania przerywa zapis.
struct NvsZawodna : public AtrapaNvs {
  explicit NvsZawodna(Dziennik& d) : AtrapaNvs(d) {}
  bool gubi = false, zanik = false;
  int zapisowLiczb = 0;
  void ustawLiczbeU(const char* p, const char* kl, uint32_t v) override {
    zapisowLiczb++;
    if (zanik) throw ZanikZasilania();
    if (!gubi) AtrapaNvs::ustawLiczbeU(p, kl, v);
  }
};

struct Stan {
  Dziennik d;
  NvsZawodna nvs{d};
  PamiecZastanychNvs pam{nvs};
  Model m;
  // Jedno wybudzenie; zanik zasilania = koniec wybudzenia bez wyniku (zwraca false).
  bool wybudzenie(bool wl, bool kabel, WynikLimitu* w = nullptr) {
    try {
      WynikLimitu x = limitLadowania(m, pam, wl, kabel);
      if (w) *w = x;
      return true;
    } catch (const ZanikZasilania&) {
      return false;
    }
  }
  void bezBledow() { m.blad = Blad::BRAK; m.bladNr = -1; nvs.gubi = nvs.zanik = false; }
};

static int vregKodu(uint8_t reg04) { return napiecieVreg((reg04 & 0xF8) >> 3); }

// Warianty wpisu `lad_zast` przed wywołaniem.
enum Wpis { BRAK_WPISU, WPIS_JAK_UKLAD, WPIS_ZASTANY, WPIS_DOMYSLNY, WPIS_KOD15, WPIS_KOD24, WPIS_ZEPSUTY, LICZBA_WPISOW };
static void ustawWpis(Stan& s, Wpis w, uint8_t r4, uint8_t r5) {
  auto zap = [&](uint8_t a, uint8_t b) { s.nvs.liczbyU["cpap/lad_zast"] = (0x5Au << 24) | ((uint32_t)b << 8) | a; };
  switch (w) {
    case BRAK_WPISU: break;
    case WPIS_JAK_UKLAD: zap(r4, r5); break;
    case WPIS_ZASTANY: zap(0x58, 0x8F); break;
    case WPIS_DOMYSLNY: zap(0x58, 0x9F); break;
    case WPIS_KOD15: zap(0x78, 0x8F); break;   // 01111 = 4352 mV (wyjątek z karty)
    case WPIS_KOD24: zap(0xC0, 0x8F); break;   // 11000 = 4624 mV
    case WPIS_ZEPSUTY: s.nvs.liczbyU["cpap/lad_zast"] = 0x5A12ABCDu; break;
    default: break;
  }
}

struct Licznik {
  long n = 0;
  bool pierwszy = true;
  void dodaj(const char* co, int r4, int r5, int wpis, int wl, int kabel, int blad, int nr) {
    n++;
    if (pierwszy) {
      printf("  przykład [%s]: REG04=%02X REG05=%02X wpis=%d opcja=%d kabel=%d błąd=%d na operacji %d\n", co, r4, r5, wpis,
             wl, kabel, blad, nr);
      pierwszy = false;
    }
  }
};

// (a) Własność jednego wywołania po wszystkich REG04 × REG05 × opcja × zasilanie × wpis × błąd na każdym kroku.
static void testWlasnoscPojedynczegoWywolania() {
  Licznik pozaRej, pozaMaska, wyzejNaOpcji, licznikWlaczony, bezWpisuObnizony, wyzejNiz4208, zapisBezOpcji;
  long przebiegow = 0;
  const Blad bledy[] = {Blad::BRAK, Blad::ODCZYT, Blad::ZAPIS_ODRZUCONY, Blad::ZAPIS_PRZYJETY_NACK};
  for (int r4 = 0; r4 < 256; r4++)
    for (int r5 = 0; r5 < 256; r5++) {
      // pełna macierz błędów dla 8 wartości REG05 (4 pola WATCHDOG × tło 8F i 00), bez błędów dla co piątej wartości
      bool pelna = (r5 & 0xCF) == 0x8F || (r5 & 0xCF) == 0x00;  // 4 pola WATCHDOG × 2 tła
      if (!pelna && (r5 % 5) != 0) continue;
      for (int wp = 0; wp < LICZBA_WPISOW; wp++)
        for (int wl = 0; wl < 2; wl++)
          for (int kabel = 0; kabel < 2; kabel++)
            for (int b = 0; b < (pelna ? 4 : 1); b++)
              for (int nr = (b == 0 ? 0 : 1); nr <= (b == 0 ? 0 : 9); nr++) {
                Stan s;
                s.m.r[4] = (uint8_t)r4;
                s.m.r[5] = (uint8_t)r5;
                ustawWpis(s, (Wpis)wp, (uint8_t)r4, (uint8_t)r5);
                s.m.blad = bledy[b];
                s.m.bladNr = nr;
                uint8_t przed[12];
                for (int i = 0; i < 12; i++) przed[i] = s.m.r[i];
                int vPrzed = vregKodu(przed[4]);
                bool podpis = (przed[5] & 0x30) == 0 && ((przed[4] & 0xF8) >> 3) == 2;
                s.wybudzenie(wl, kabel);
                przebiegow++;
                for (auto& z : s.m.zapisy)
                  if (z.first != 0x04 && z.first != 0x05) pozaRej.dodaj("zapis poza REG04/05", r4, r5, wp, wl, kabel, b, nr);
                bool inne = false;
                for (int i = 0; i < 12; i++)
                  if (i != 4 && i != 5 && s.m.r[i] != przed[i]) inne = true;
                if (inne || (s.m.r[4] & 0x07) != (przed[4] & 0x07) || (s.m.r[5] & 0xCF) != (przed[5] & 0xCF))
                  pozaMaska.dodaj("bit poza maską", r4, r5, wp, wl, kabel, b, nr);
                int vPo = s.m.vreg();
                if (wl && vPrzed >= 0 && vPo > vPrzed) wyzejNaOpcji.dodaj("opcja wł.: VREG wyżej niż przed", r4, r5, wp, wl, kabel, b, nr);
                if (wl && (s.m.r[5] & 0x30) != 0 && (s.m.r[5] & 0x30) != (przed[5] & 0x30))
                  licznikWlaczony.dodaj("opcja wł.: licznik zmieniony na włączony", r4, r5, wp, wl, kabel, b, nr);
                // przy opcji wł. obniżenie VREG bez wpisu uniemożliwiłoby powrót (przywracanie z wpisu przy opcji wył.
                // obniża z założenia i usuwa wpis: to osobna obserwacja niżej, nie ten niezmiennik)
                if (wl && vPrzed >= 0 && vPo >= 0 && vPo < vPrzed && !s.pam.wczytaj().znane && !podpis)
                  bezWpisuObnizony.dodaj("VREG obniżony bez wpisu w NVS", r4, r5, wp, wl, kabel, b, nr);
                int granica = vPrzed > 4208 ? vPrzed : 4208;
                if (vPo > granica) wyzejNiz4208.dodaj("VREG powyżej max(przed, 4208 mV)", r4, r5, wp, wl, kabel, b, nr);
                if (!wl && wp == BRAK_WPISU && !podpis && !s.m.zapisy.empty())
                  zapisBezOpcji.dodaj("opcja wył., bez wpisu: zapis", r4, r5, wp, wl, kabel, b, nr);
              }
    }
  printf("  przebiegów: %ld\n", przebiegow);
  SPRAWDZ(pozaRej.n == 0);
  SPRAWDZ(pozaMaska.n == 0);
  SPRAWDZ(licznikWlaczony.n == 0);
  SPRAWDZ(bezWpisuObnizony.n == 0);
  SPRAWDZ(zapisBezOpcji.n == 0);
  // [USTERKA] nieaktualny / obcy wpis `lad_zast` z kodem VREG > 01011 jest przywracany bez ograniczenia; przy
  // opcji włączonej to samo robi cofnięcie VREG po nieudanym zapisie licznika (ladowarka.cpp, `ustawPole(... zz.reg04`)
  // obserwacja: przy opcji wł. VREG bywa podnoszony z wartości niższej niż limit (np. 3856 → 3920 mV), gdy wpis
  // mówi „zastane 4208 mV”, a układ ktoś od tego czasu przestawił niżej; tu bez SPRAWDZ, liczba w logu
  printf("  VREG wyżej niż przed przy opcji wł. (oba przypadki razem): %ld, VREG powyżej max(przed, 4208 mV): %ld\n",
         wyzejNaOpcji.n, wyzejNiz4208.n);
  SPRAWDZ(wyzejNiz4208.n == 0);
}

// Upływ licznika watchdoga między wybudzeniami (model [S]: w trybie host z włączonym licznikiem wartości domyślne
// poza REG00, REG06, REG07). `host` = był zapis od startu zasilania.
static void uplyw(Model& m, bool host) {
  if (!host || (m.r[5] & 0x30) == 0) return;
  const uint8_t d[12] = {0x17, 0x1A, 0xA2, 0x22, 0x58, 0x9F, 0xE6, 0x4C, 0x00, 0x80, 0x00, 0x40};
  for (int i = 0; i < 12; i++)
    if (i != 0 && i != 6 && i != 7) m.r[i] = d[i];
}

// (b) Restart / błąd w każdym kroku pierwszego wybudzenia z opcją, potem wybudzenia bez błędów: limit dochodzi do
// skutku, wyłączenie przywraca zastane, a w żadnej chwili VREG nie jest wyżej niż zastane (ani niż 4208 mV po resecie).
static void testRestartMiedzyKrokami() {
  long przypadkow = 0, wyzej = 0, nieUstawiony = 0, nieWrocil = 0, wpisZostal = 0;
  const Blad bledy[] = {Blad::ODCZYT, Blad::ZAPIS_ODRZUCONY, Blad::ZAPIS_PRZYJETY_NACK, Blad::ZANIK};
  for (int kod = 0; kod < 32; kod++)
    for (int wd = 0; wd < 4; wd++)
      for (int b = 0; b < 6; b++)  // 4 rodzaje błędu układu + NVS gubi + zanik w trakcie zapisu NVS
        for (int nr = 1; nr <= 10; nr++) {
          if (b >= 4 && nr > 1) continue;
          if (kod == 2 && wd == 0) continue;  // zastane = podpis limitu: osobny przypadek (testPodpisObcy)
          Stan s;
          s.m.r[4] = (uint8_t)((kod << 3) | 0x01);  // VRECHG = 1, żeby było widać nietknięte bity
          s.m.r[5] = (uint8_t)(0x8F & ~0x30) | (uint8_t)(wd << 4);
          const uint8_t z4 = s.m.r[4], z5 = s.m.r[5];
          const int vz = vregKodu(z4);
          bool host = wd == 0;  // licznik wyłączony = ktoś już pisał (jak na jednym egzemplarzu panelu)
          if (b < 4) { s.m.blad = bledy[b]; s.m.bladNr = nr; }
          if (b == 4) s.nvs.gubi = true;
          if (b == 5) s.nvs.zanik = true;
          auto krok = [&](bool wl) {
            size_t zp = s.m.zapisy.size();
            s.wybudzenie(wl, true);
            if (s.m.zapisy.size() != zp) host = true;
            int v = s.m.vreg();
            if (vz >= 0 && v > (vz > 4208 ? vz : 4208)) wyzej++;
            uplyw(s.m, host);
            if (host && (s.m.r[5] & 0x30) != 0) host = false;  // po upływie: tryb domyślny
          };
          krok(true);
          s.bezBledow();
          krok(true);
          krok(true);
          przypadkow++;
          if (vz > 3920 && !(s.m.vreg() == 3920 && (s.m.r[5] & 0x30) == 0)) nieUstawiony++;
          krok(false);
          krok(false);
          int v = s.m.vreg();
          if (vz > 3920 && v != vz && v != 4208) nieWrocil++;
          if (s.pam.wczytaj().znane) wpisZostal++;
          (void)z5;
        }
  printf("  przypadków: %ld, VREG wyżej: %ld, limit nieustawiony po 2 czystych wybudzeniach: %ld, nie wrócił: %ld, "
         "wpis został: %ld\n", przypadkow, wyzej, nieUstawiony, nieWrocil, wpisZostal);
  SPRAWDZ(wyzej == 0);
  SPRAWDZ(nieUstawiony == 0);
  SPRAWDZ(nieWrocil == 0);
  SPRAWDZ(wpisZostal == 0);
}

// (c) Wpisy nieaktualne i obce. Wartości, które mają się nie zdarzyć na jednym egzemplarzu panelu, ale których kod nie wyklucza.
static void testWpisyNieaktualne() {
  {  // [USTERKA] wpis z kodem 01111 (4352 mV) przy limicie w układzie; wyłączenie opcji podnosi VREG ponad 4208 mV
    Stan s;
    s.m.r[4] = 0x10;
    ustawWpis(s, WPIS_KOD15, 0, 0);
    WynikLimitu w;
    s.wybudzenie(false, true, &w);
    printf("  wpis 01111, limit w układzie, opcja wył.: %s, VREG po = %d mV\n", nazwa(w.wynik), s.m.vreg());
    SPRAWDZ(s.m.vreg() <= 4208);
  }
  {  // [USTERKA] wpis z kodem 11000 (4624 mV)
    Stan s;
    s.m.r[4] = 0x10;
    ustawWpis(s, WPIS_KOD24, 0, 0);
    s.wybudzenie(false, false);
    printf("  wpis 11000, limit w układzie, opcja wył., na baterii: VREG po = %d mV\n", s.m.vreg());
    SPRAWDZ(s.m.vreg() <= 4208);
  }
  {  // obserwacja: wpis z jednego egzemplarza panelu (05=8F), układ zresetowany (05=9F, tryb domyślny), opcja wył.: zapis licznika
    Stan s;
    s.m.r[5] = 0x9F;
    s.m.r[2] = 0xA2;
    ustawWpis(s, WPIS_ZASTANY, 0, 0);
    WynikLimitu w;
    s.wybudzenie(false, true, &w);
    printf("  wpis 58/8F, układ po resecie 58/9F, opcja wył.: %s, zapisów %d, REG05 po = %02X\n", nazwa(w.wynik),
           (int)s.m.zapisy.size(), s.m.r[5]);
  }
  {  // obserwacja: wpis z trybu domyślnego (05=9F), układ teraz z limitem i 05=8F: przywrócenie włącza licznik
    Stan s;
    s.m.r[4] = 0x10;
    ustawWpis(s, WPIS_DOMYSLNY, 0, 0);
    WynikLimitu w;
    s.wybudzenie(false, true, &w);
    uplyw(s.m, true);
    printf("  wpis 58/9F, układ 10/8F (prąd 480 mA), opcja wył.: %s, po upływie licznika REG02 = %02X (%d mA)\n",
           nazwa(w.wynik), s.m.r[2], (s.m.r[2] & 0x3F) * 60);
  }
}

// (d) Inny układ pod 0x6B, PN inne niż 1000, wisząca magistrala (stały bajt), brak odpowiedzi.
static void testInnyUklad() {
  for (int pn = 0; pn < 16; pn++) {
    if (pn == 8) continue;
    for (int wl = 0; wl < 2; wl++) {
      Stan s;
      s.m.r[11] = (uint8_t)(pn << 3);
      ustawWpis(s, WPIS_ZASTANY, 0, 0);
      s.m.r[4] = 0x10;
      s.wybudzenie(wl, true);
      SPRAWDZ(s.m.zapisy.empty());
    }
  }
  const int stale[] = {0x00, 0xFF, 0x40, 0xC0, 0x47};
  for (int b : stale)
    for (int wl = 0; wl < 2; wl++)
      for (int wp = 0; wp < 2; wp++) {
        Stan s;
        s.m.stalyBajt = b;
        if (wp) ustawWpis(s, WPIS_ZASTANY, 0, 0);
        WynikLimitu w;
        s.wybudzenie(wl, true, &w);
        // wisząca magistrala z bajtem, który wygląda jak PN 1000: zapis może pójść, ale nigdy nie jest „ustawiono”
        SPRAWDZ(w.wynik != Wynik::USTAWIONO && w.wynik != Wynik::PRZYWROCONO);
        for (auto& z : s.m.zapisy) SPRAWDZ(z.first == 0x04 || z.first == 0x05);
        if ((b & 0x78) != 0x40) SPRAWDZ(s.m.zapisy.empty());
      }
}

// (e) Częstość: operacje I2C i zapisy NVS na wybudzenie w stanie ustalonym; (f) kabel → bateria → kabel.
static void testCzestoscIPrzebieg() {
  Stan s;
  WynikLimitu w;
  s.wybudzenie(true, true, &w);
  SPRAWDZ(w.wynik == Wynik::USTAWIONO && s.m.zapisy.size() == 1 && s.nvs.zapisowLiczb == 1);
  SPRAWDZ(s.m.r[4] == 0x10 && s.m.r[5] == 0x8F && s.m.r[2] == 0x88);
  int nvs0 = s.nvs.zapisowLiczb;
  for (int i = 0; i < 1000; i++) {
    int od0 = s.m.odczyty;
    size_t z0 = s.m.zapisy.size();
    bool kabel = (i % 10) != 3;  // co dziesiąte wybudzenie na baterii
    s.wybudzenie(true, kabel, &w);
    SPRAWDZ(s.m.zapisy.size() == z0);
    SPRAWDZ(s.m.odczyty - od0 == 3);
    SPRAWDZ(w.wynik == (kabel ? Wynik::USTAWIONY : Wynik::NIE_NA_KABLU));
  }
  SPRAWDZ(s.nvs.zapisowLiczb == nvs0);
  SPRAWDZ(s.m.r[4] == 0x10);
  // wyłączenie: jeden zapis, wpis usunięty; potem opcja wył. bez wpisu: 3 odczyty, zero zapisów
  s.wybudzenie(false, false, &w);
  SPRAWDZ(w.wynik == Wynik::PRZYWROCONO && s.m.r[4] == 0x58 && s.m.r[5] == 0x8F && !s.pam.wczytaj().znane);
  size_t z1 = s.m.zapisy.size();
  int od1 = s.m.odczyty;
  s.wybudzenie(false, true, &w);
  SPRAWDZ(w.wynik == Wynik::WYLACZONY && s.m.zapisy.size() == z1 && s.m.odczyty - od1 == 3);
}

// (i) Obserwacje bez oceny „zła/dobra” : podpis limitu ustawiony przez obce oprogramowanie i zmiana
// progu w przyszłej wersji.
static void testPodpisObcy() {
  {  // układ z VREG 3920 mV i licznikiem wył. od innego oprogramowania, opcja NIGDY nie włączona
    Stan s;
    s.m.r[4] = 0x10;
    WynikLimitu w;
    s.wybudzenie(false, true, &w);
    printf("  obcy podpis 10/8F, opcja nigdy nie włączona: %s, zapisów %d, VREG po = %d mV\n", nazwa(w.wynik),
           (int)s.m.zapisy.size(), s.m.vreg());
  }
  {  // limit 3920 mV w układzie, NVS wyczyszczona, przyszła wersja z progiem 4000 mV
    Stan s;
    s.m.r[4] = 0x10;
    WynikLimitu a, b;
    try { a = limitLadowania(s.m, s.pam, true, true, 4000); } catch (...) {}
    try { b = limitLadowania(s.m, s.pam, false, true, 4000); } catch (...) {}
    printf("  stary limit 3920 mV bez wpisu, próg 4000 mV: wł. = %s, wył. = %s, VREG po = %d mV\n", nazwa(a.wynik),
           nazwa(b.wynik), s.m.vreg());
  }
}

// (h) Parser klucza z /konfiguracja.txt: tylko dokładne `ladowanie_limit=tak` włącza.
static void testParser() {
  SPRAWDZ(limitWKonfiguracji("ladowanie_limit=tak\r\n"));
  SPRAWDZ(limitWKonfiguracji("\xEF\xBB\xBFladowanie_limit=tak"));
  SPRAWDZ(limitWKonfiguracji("ladowanie_limit=tak \n"));
  SPRAWDZ(!limitWKonfiguracji(" ladowanie_limit=tak"));
  SPRAWDZ(!limitWKonfiguracji("ladowanie_limit=Tak"));
  SPRAWDZ(!limitWKonfiguracji("ladowanie_limit=takk"));
  SPRAWDZ(!limitWKonfiguracji("#ladowanie_limit=tak"));
  SPRAWDZ(!limitWKonfiguracji("x_ladowanie_limit=tak"));
  SPRAWDZ(!limitWKonfiguracji("ladowanie_limit=tak\nladowanie_limit=nie"));
  SPRAWDZ(limitWKonfiguracji("ladowanie_limit=nie\nladowanie_limit=tak"));
  SPRAWDZ(!limitWKonfiguracji(""));
}

int main() {
  printf("brzegowe: własność jednego wywołania\n");
  testWlasnoscPojedynczegoWywolania();
  printf("brzegowe: restart i błąd w każdym kroku\n");
  testRestartMiedzyKrokami();
  printf("brzegowe: wpisy nieaktualne i obce\n");
  testWpisyNieaktualne();
  printf("brzegowe: inny układ, wisząca magistrala\n");
  testInnyUklad();
  printf("brzegowe: częstość i kabel → bateria → kabel\n");
  testCzestoscIPrzebieg();
  printf("brzegowe: obserwacje\n");
  testPodpisObcy();
  testParser();
  printf("testy_ladowarka_brzegowe: %d OK, %d BŁĘDÓW\n", ok, zle);
  return zle ? 1 : 0;
}
