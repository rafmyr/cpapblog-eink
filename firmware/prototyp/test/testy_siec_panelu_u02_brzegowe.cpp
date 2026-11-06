// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (sieć panelu U-02, tylko ręcznie).
// Testy z identyfikatorem przypadku pokazują znalezisko; testy bez niego sprawdzają, co trzyma (AP po wyjściu,
// generator przy radiu, droga do U-02 bez sieci i bez danych). Dane syntetyczne, bez prawdziwych haseł.
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "nawigacja.h"
#include "sekrety.h"
#include "serwer_raportu.h"
#include "start.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(nr, w)                                                        \
  do {                                                                        \
    if (w) ok++;                                                              \
    else { zle++; printf("BŁĄD %s %s:%d: %s\n", nr, __FILE__, __LINE__, #w); } \
  } while (0)

static sekrety::Dane dane(const char* ssid, const char* hw, const char* ab) {
  sekrety::Dane d;
  d.ssid = ssid;
  d.hasloWifi = hw;
  d.abAdres = ab;
  d.abKonto = "admin";
  d.abHaslo = "Ab-Tajne-5511";
  return d;
}

// NVS, w którym po `limit` zapisach panel „traci zasilanie”: dalsze zapisy przepadają (wynik true, jak gdyby kod
// szedł dalej; stan NVS = stan po dokładnie `limit` zapisach, każdy zapis atomowy).
class NvsRestart : public AtrapaNvs {
 public:
  explicit NvsRestart(Dziennik& d) : AtrapaNvs(d) {}
  int limit = 1 << 30, wykonane = 0;
  bool ustawTekst(const char* p, const char* kl, const std::string& v) override {
    if (wykonane++ >= limit) return true;
    return AtrapaNvs::ustawTekst(p, kl, v);
  }
  void ustawLiczbeU(const char* p, const char* kl, uint32_t v) override {
    if (wykonane++ >= limit) return;
    AtrapaNvs::ustawLiczbeU(p, kl, v);
  }
};

// restart w każdej chwili zapisu nowej sieci po udanym sprawdzeniu w U-02 (sekrety::zapiszZFormularza)
// musi zostawić stare albo nowe dane, nigdy secrets.h (stara sieć sprzed zmiany routera) ani „pierwszą konfigurację”.
static void restartWZapisie() {
  Dziennik dz;
  const sekrety::Dane S = dane("SiecZKabla", "Haslo-Kabel-1", "192.168.0.40");   // secrets.h z wgrania kablem
  const sekrety::Dane A = dane("RouterA", "Haslo-A-12345", "192.168.0.41");      // pierwsza zmiana przez U-02
  const sekrety::Dane B = dane("RouterB", "Haslo-B-12345", "192.168.0.42");      // druga zmiana przez U-02
  for (int wariant = 0; wariant < 2; wariant++) {  // 0: zmiana z „f” (A → B); 1: zmiana z „k” (S → B)
    AtrapaNvs baza(dz);
    sekrety::wczytaj(baza, S);
    if (wariant == 0) sekrety::zapiszZFormularza(baza, A);
    const std::string przed = wariant == 0 ? A.ssid : S.ssid;
    NvsRestart licz(dz);
    licz.teksty = baza.teksty;
    licz.liczbyU = baza.liczbyU;
    sekrety::zapiszZFormularza(licz, B);
    int W = licz.wykonane;
    int zleS = 0, zleBrak = 0;
    for (int N = 0; N <= W; N++) {
      for (int zKabla = 0; zKabla < 2; zKabla++) {  // obraz z secrets.h egzemplarza testowego albo obraz wydania bez sekretów
        NvsRestart n(dz);
        n.teksty = baza.teksty;
        n.liczbyU = baza.liczbyU;
        n.limit = N;
        sekrety::zapiszZFormularza(n, B);
        AtrapaNvs po(dz);
        po.teksty = n.teksty;
        po.liczbyU = n.liczbyU;
        sekrety::Wynik w = sekrety::wczytaj(po, zKabla ? S : sekrety::Dane());
        bool dobre = w.d.ssid == przed || w.d.ssid == B.ssid;
        if (!dobre && w.pierwszaKonfiguracja()) zleBrak++;
        else if (!dobre) zleS++;
        if (!dobre)
          printf("  restart-zapisu-sieci: wariant %s, restart po %d z %d zapisów, obraz %s → %s\n", wariant ? "k→f" : "f→f", N, W,
                 zKabla ? "z secrets.h" : "bez sekretów",
                 w.pierwszaKonfiguracja() ? "pierwsza konfiguracja" : ("sieć " + w.d.ssid).c_str());
      }
    }
    SPRAWDZ("restart-zapisu-sieci", zleS == 0 && zleBrak == 0);
  }
}

// ---------- dyrygent na atrapach ----------
class Gniazdo : public serwer::GniazdoHttp {
 public:
  bool slucha = false;
  std::deque<Zadanie> doWyslania;
  std::vector<serwer::Odpowiedz> odp;
  bool start(uint16_t) override { slucha = true; return true; }
  void stop() override { slucha = false; }
  void obsluz(const std::function<serwer::Odpowiedz(const std::string&)>&) override {}
  void obsluzZadania(const std::function<serwer::Odpowiedz(const Zadanie&)>& f) override {
    if (!slucha || doWyslania.empty()) return;
    Zadanie z = doWyslania.front();
    doWyslania.pop_front();
    odp.push_back(f(z));
  }
};
static KonfiguracjaPanelu konf(Panel& p, const sekrety::Dane& zk) {
  sekrety::Wynik s = sekrety::wczytaj(p.nvs, zk);
  KonfiguracjaPanelu k;
  k.abAdres = s.d.abAdres;
  k.abKonto = s.d.abKonto;
  k.abHaslo = s.d.abHaslo;
  k.ssid = s.d.ssid;
  k.hasloWifi = s.d.hasloWifi;
  k.oknoZHaslem = true;
  k.pierwszaKonfiguracja = s.pierwszaKonfiguracja();
  k.opisSekretow = sekrety::opis(s);
  return k;
}
static Ekran ekran(Panel& p) { return (Ekran)p.start.stan.ekran; }
static int ile(Panel& p, const std::string& x) {
  int n = 0;
  for (auto& l : p.d.z) n += l.find(x) != std::string::npos;
  return n;
}
// Jedno wybudzenie przyciskiem `pr` (dlugo = 2,3 s), zegar `ep` (0 = nieznany).
static void nacisnij(Panel& p, const sekrety::Dane& zk, Przycisk pr, bool dlugo, uint32_t ep) {
  uint8_t bit = (uint8_t)(1u << (int)pr);
  p.zegar.epoch = ep;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = {{0, bit}, {dlugo ? 2300u : 300u, 0}};
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), konf(p, zk)).wybudzenie();
}
static void start(Panel& p, const sekrety::Dane& zk, uint32_t ep) {
  p.zegar.epoch = ep;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  p.start.powodWybudzenia = PowodWybudzenia::START;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), konf(p, zk)).wybudzenie();
}

// Użytkownik po wymianie routera: panel skonfigurowany („k”), stara sieć nie istnieje, panel bez danych i bez zegara
// (U-04). Droga z kartki: ekran dnia ▶ po kolei przez strony kokpitu do U-01 (6 / 6) → ◀ 2 s → U-02. Po każdym
// wybudzeniu sieć panelu wyłączona i serwer nie słucha (AP nie przeżywa wybudzenia), generator przy radiu.
static void drogaBezSieci(Gniazdo& g) {
  Panel p;
  const sekrety::Dane S = dane("StaryRouter", "Haslo-Stare-1", "192.168.0.40");
  p.siec.siecJest = false;
  start(p, S, 0);
  SPRAWDZ("", p.siec.apStartow == 0 && !p.siec.apTeraz);
  nacisnij(p, S, Przycisk::PRAWY, false, 0);
  SPRAWDZ("", ekran(p) == Ekran::K1);
  for (int i = 0; i < 12 && ekran(p) != Ekran::USTAWIENIA; i++) nacisnij(p, S, Przycisk::PRAWY, false, 0);
  SPRAWDZ("", ekran(p) == Ekran::USTAWIENIA);
  p.siec.losowychBezRadia = 0;  // ziarno panelu przy pierwszym starcie (sprzed etapu 4) się nie liczy
  nacisnij(p, S, Przycisk::LEWY, true, 0);
  SPRAWDZ("", p.siec.apStartow == 1);                   // U-02 bez sieci domowej i bez zegara
  SPRAWDZ("", p.siec.losowychBezRadia == 0);           // hasło i token przy włączonym radiu (R-KONF-08, R-NF-09)
  SPRAWDZ("", !p.siec.apTeraz && !g.slucha);           // po 15 min: AP wyłączony, serwer nie słucha
  SPRAWDZ("", ekran(p) == Ekran::USTAWIENIA);
  SPRAWDZ("", p.siec.ssid == "StaryRouter");           // R-KONF-29: zapisana sieć do następnego pobrania
}

// Pierwsza konfiguracja bez zegara (nowy panel po `erase`, obraz bez sieci): bez Wi-Fi w ogóle przy starcie (bateria,
// R-KONF-07 „bez prób pobierania”), ◀ 2 s uruchamia AP także bez zegara; ◀ wraca na ekran dnia, AP wyłączony.
static void pierwszaBezZegara(Gniazdo& g) {
  Panel p;
  const sekrety::Dane brak;
  start(p, brak, 0);
  // pierwszy start (ADR-0011 pkt 3): panel bez żadnej sieci po starcie zasilania sam
  // uruchamia sieć panelu (było: apStartow == 0); po 15 min AP wyłączony, bez łączenia i bez żądań
  SPRAWDZ("", ile(p, "siec.polacz") == 0 && ile(p, "siec.get") == 0 && p.siec.apStartow == 1);
  SPRAWDZ("", !p.siec.apTeraz && !g.slucha && ekran(p) == Ekran::DZIEN);
  p.d.z.clear();
  // ◀ 2 s budzące, po 30 s ◀ anuluj
  uint8_t L = (uint8_t)(1u << (int)Przycisk::LEWY);
  p.zegar.epoch = 0;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = L;
  p.przyciski.skrypt = {{0, L}, {2300u, 0}, {30000u, L}, {30300u, 0}};
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), konf(p, brak)).wybudzenie();
  SPRAWDZ("", p.siec.apStartow == 2 && ekran(p) == Ekran::DZIEN && !p.siec.apTeraz && !g.slucha);  // P07c: 2
  SPRAWDZ("", ile(p, "siec.polacz") == 0);  // anulowanie nie łączy z (pustą) siecią domową
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  Gniazdo g;
  serwer::ustawGniazdo(&g);
  restartWZapisie();
  drogaBezSieci(g);
  pierwszaBezZegara(g);
  printf("testy_siec_panelu_u02_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
