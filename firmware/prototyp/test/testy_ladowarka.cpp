// SPDX-License-Identifier: GPL-3.0-or-later
// Testy limitu ładowania baterii na kablu (ADR-0046, R-DANE-66…R-DANE-68), pisane z wymagań przed kodem:
// (a) przeliczenie kodu VREG i zaokrąglenie progu do kroku układu (wartości brzegowe),
// (b) decyzja rdzenia na atrapie rejestrów SY6974B: opcja wyłączona = zero zapisów (regresja), włączona na kablu =
//     dokładnie oczekiwane zapisy i odczyt kontrolny, na baterii, inny układ, brak odpowiedzi, błąd w połowie, odczyt po
//     zapisie niezgodny, wyłączenie = przywrócenie, upływ watchdoga = ponowne ustawienie, NVS bez wpisu,
// (c) własność: dla każdej wartości zastanej VREG zapisany nigdy wyżej niż zastane, tylko pola VREG i WATCHDOG,
// (d) klucz `ladowanie_limit` w /konfiguracja.txt i formularz U-03 (zapis, odczyt, zła wartość),
// (e) dyrygent z atrapami: jedno wybudzenie z opcją i bez.
// Uruchomienie: tools/prototyp/testy.sh (program testy_ladowarka).
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "formularz.h"
#include "ladowarka.h"
#include "magazyn.h"
#include "start.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
#define ROWNE_L(a, b)                                                                                    \
  do {                                                                                                   \
    long long _a = (long long)(a), _b = (long long)(b);                                                  \
    if (_a == _b) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); }  \
  } while (0)

using namespace ladowarka;

// Pamięć zastanych w atrapie NVS (ta sama klasa co na panelu).
struct Stanowisko {
  Dziennik d;
  AtrapaNvs nvs{d};
  PamiecZastanychNvs pam{nvs};
  AtrapaLadowarki lad;
  WynikLimitu wolaj(bool wl, bool kabel, int limit = LIMIT_LADOWANIA_MV) { return limitLadowania(lad, pam, wl, kabel, limit); }
  bool wpis() { return pam.wczytaj().znane; }
  int vreg() const { return napiecieVreg((lad.r[4] & 0xF8) >> 3); }
  int wd() const { return (lad.r[5] & 0x30) >> 4; }
};

// Pamięć, której zapis się nie udaje (pełna albo zawodna NVS).
struct PamiecZawodna : public PamiecZastanych {
  Zastane wczytaj() override { return Zastane(); }
  bool zapisz(const Zastane&) override { return false; }
  void usun() override {}
};

// ---------- (a) kody i próg ----------
static void testKodyVreg() {
  ROWNE_L(napiecieVreg(0), 3856);
  ROWNE_L(napiecieVreg(2), 3920);
  ROWNE_L(napiecieVreg(11), 4208);  // domyślne [K]
  ROWNE_L(napiecieVreg(14), 4304);
  ROWNE_L(napiecieVreg(15), 4352);  // wyjątek z karty (nie 4336)
  ROWNE_L(napiecieVreg(16), 4368);
  ROWNE_L(napiecieVreg(24), 4624);
  ROWNE_L(napiecieVreg(25), 4624);  // [K]: 11000…11111 = 4624 mV
  ROWNE_L(napiecieVreg(31), 4624);
  ROWNE_L(napiecieVreg(32), -1);
  ROWNE_L(napiecieVreg(-1), -1);
  for (int k = 1; k <= KOD_VREG_MAX; k++) SPRAWDZ(napiecieVreg(k) > napiecieVreg(k - 1));  // rośnie z kodem
  ROWNE_L(kodVregNieWyzej(LIMIT_LADOWANIA_MV), 2);  // 3950 → 3920 mV
  ROWNE_L(kodVregNieWyzej(3952), 3);
  ROWNE_L(kodVregNieWyzej(3951), 2);
  ROWNE_L(kodVregNieWyzej(3920), 2);
  ROWNE_L(kodVregNieWyzej(3919), 1);
  ROWNE_L(kodVregNieWyzej(3856), 0);
  ROWNE_L(kodVregNieWyzej(3855), -1);
  ROWNE_L(kodVregNieWyzej(4351), 14);
  ROWNE_L(kodVregNieWyzej(4352), 15);
  ROWNE_L(kodVregNieWyzej(99999), 24);
  // próg ok. 70% z krzywej M-25 i nie wyżej niż wartość domyślna układu
  SPRAWDZ(napiecieVreg(kodVregNieWyzej(LIMIT_LADOWANIA_MV)) <= LIMIT_LADOWANIA_MV);
  SPRAWDZ(LIMIT_LADOWANIA_MV < napiecieVreg(KOD_VREG_DOMYSLNY));
}

// ---------- (b) decyzja na atrapie ----------
static void testWylaczonaBezZapisow() {
  Stanowisko s;
  for (bool kabel : {false, true}) {
    WynikLimitu w = s.wolaj(false, kabel);
    SPRAWDZ(w.wynik == Wynik::WYLACZONY);
    ROWNE_L(w.zapisow, 0);
  }
  ROWNE_L(s.lad.zapisy.size(), 0);
  ROWNE_L(s.nvs.zapisy, 0);
  SPRAWDZ(!s.lad.host);
  // bez wpisu zero zapisów dla układu w każdym stanie, także z podpisem limitu (licznik 00, VREG 3920 mV);
  // VREG niższy niż 4208 mV tylko w logu (WYLACZONY_NIZEJ)
  for (int r4 = 0; r4 < 256; r4++)
    for (uint8_t r5 : {0x9F, 0x8F, 0xBF, 0xFF, 0x00, 0x1F}) {
      Stanowisko t;
      t.lad.r[4] = (uint8_t)r4;
      t.lad.r[5] = r5;
      WynikLimitu w = t.wolaj(false, true);
      SPRAWDZ(t.lad.zapisy.empty() && t.nvs.zapisy == 0);
      SPRAWDZ(w.wynik == ((r4 >> 3) < 11 ? Wynik::WYLACZONY_NIZEJ : Wynik::WYLACZONY));
    }
}

static void testWlaczonaNaKablu() {
  Stanowisko s;
  WynikLimitu w = s.wolaj(true, true);
  SPRAWDZ(w.wynik == Wynik::USTAWIONO);
  ROWNE_L(w.zapisow, 2);
  // dokładnie: REG04 = 00010 000 (3920 mV, bity 2:0 bez zmian), potem REG05 = 1 0 00 1 1 1 1 (licznik wyłączony)
  SPRAWDZ(s.lad.zapisy == (std::vector<std::pair<uint8_t, uint8_t>>{{0x04, 0x10}, {0x05, 0x8F}}));
  ROWNE_L(s.vreg(), 3920);
  ROWNE_L(s.wd(), 0);
  ROWNE_L(w.vregMv, 3920);
  ROWNE_L(w.watchdog, 0);
  // zastane zapisane w NVS przed pierwszym zapisem do układu
  ROWNE_L(s.nvs.liczbyU["cpap/lad_zast"], 0x5A009F58);
  SPRAWDZ(s.d.pierwsze("nvs.zapis cpap/lad_zast") == 0);
  SPRAWDZ(opis(w) == "ladowanie: limit ustawiono, VREG 3920 mV, watchdog 0, zapisow 2");
  // kolejne wybudzenie: nic do zrobienia
  w = s.wolaj(true, true);
  SPRAWDZ(w.wynik == Wynik::USTAWIONY);
  ROWNE_L(s.lad.zapisy.size(), 2);
  ROWNE_L(s.nvs.zapisy, 1);
  // zostaje także po upływie czasu (licznik wyłączony: upływ nic nie zmienia)
  s.lad.uplywWatchdoga();
  ROWNE_L(s.vreg(), 3920);
}

static void testNaBaterii() {
  Stanowisko s;
  WynikLimitu w = s.wolaj(true, false);
  SPRAWDZ(w.wynik == Wynik::NIE_NA_KABLU);
  SPRAWDZ(s.lad.zapisy.empty() && s.nvs.zapisy == 0);
  // ustawiony na kablu zostaje po odłączeniu kabla (bez przywracania, bez zapisów)
  s.wolaj(true, true);
  size_t z = s.lad.zapisy.size();
  w = s.wolaj(true, false);
  SPRAWDZ(w.wynik == Wynik::NIE_NA_KABLU && s.lad.zapisy.size() == z && s.vreg() == 3920);
}

static void testInnyUkladIBledy() {
  {  // inny układ (PN 1001)
    Stanowisko s;
    s.lad.r[0x0B] = 0x48;
    SPRAWDZ(s.wolaj(true, true).wynik == Wynik::BRAK_UKLADU);
    SPRAWDZ(s.lad.zapisy.empty() && s.nvs.zapisy == 0);
  }
  {  // brak odpowiedzi
    Stanowisko s;
    s.lad.odpowiada = false;
    SPRAWDZ(s.wolaj(true, true).wynik == Wynik::BRAK_UKLADU);
    SPRAWDZ(s.wolaj(false, true).wynik == Wynik::BRAK_UKLADU);
    SPRAWDZ(s.lad.zapisy.empty() && s.nvs.zapisy == 0);
  }
  for (uint8_t rej : {0x04, 0x05}) {  // odczyt REG04 albo REG05 nieudany
    Stanowisko s;
    s.lad.nieczytelne.insert(rej);
    SPRAWDZ(s.wolaj(true, true).wynik == Wynik::BLAD_ODCZYTU);
    SPRAWDZ(s.lad.zapisy.empty() && s.nvs.zapisy == 0);
  }
  {  // kod 11111 = 4624 mV [K]: limit obniża do 3920 mV; wpis z kodem 11111 przy przywracaniu = uszkodzony → 4208 mV
    Stanowisko s;
    s.lad.r[4] = 0xF8;
    SPRAWDZ(s.wolaj(true, true).wynik == Wynik::USTAWIONO && s.vreg() == 3920);
    SPRAWDZ(s.wolaj(false, true).wynik == Wynik::PRZYWROCONO_Z_KARTY && s.vreg() == 4208);
  }
  {  // NVS nie przyjmuje zastanych: zero zapisów do układu
    AtrapaLadowarki lad;
    PamiecZawodna p;
    SPRAWDZ(limitLadowania(lad, p, true, true).wynik == Wynik::BLAD_PAMIECI);
    SPRAWDZ(lad.zapisy.empty() && !lad.host);
  }
}

static void testBladWPolowie() {
  {  // pierwszy zapis (VREG) nieudany: układ bez zmian, wpis zostaje, następne wybudzenie kończy pracę
    Stanowisko s;
    s.lad.nieudanyZapisNr = 1;
    WynikLimitu w = s.wolaj(true, true);
    SPRAWDZ(w.wynik == Wynik::BLAD_ZAPISU);
    ROWNE_L(s.vreg(), 4208);
    ROWNE_L(s.wd(), 1);
    SPRAWDZ(s.wpis());
    w = s.wolaj(true, true);
    SPRAWDZ(w.wynik == Wynik::USTAWIONO && s.vreg() == 3920 && s.wd() == 0);
  }
  {  // drugi zapis (licznik) nieudany: VREG wraca do zastanego, licznik włączony (stan bezpieczny: ładowanie do pełna)
    Stanowisko s;
    s.lad.nieudanyZapisNr = 2;
    WynikLimitu w = s.wolaj(true, true);
    SPRAWDZ(w.wynik == Wynik::BLAD_ZAPISU);
    ROWNE_L(s.vreg(), 4208);
    ROWNE_L(s.wd(), 1);
    ROWNE_L(w.vregMv, 4208);
    ROWNE_L(w.watchdog, 1);
  }
  {  // drugi i trzeci (powrót) nieudane: VREG obniżony, ale licznik włączony, więc po 40 s układ wraca do domyślnych
    Stanowisko s;
    s.lad.nieudanyZapisNr = 2;
    s.wolaj(true, true);
    s.lad.nieudanyZapisNr = -1;
    // symulacja: trzeci zapis też przepadł = rejestr VREG obniżony, licznik włączony
    s.lad.r[4] = 0x10;
    SPRAWDZ(s.wd() == 1 && s.lad.host);
    s.lad.uplywWatchdoga();
    ROWNE_L(s.vreg(), 4208);  // [S]: upływ licznika przywraca VREG
  }
  {  // odczyt po zapisie niezgodny (układ potwierdza, ale nie przyjmuje VREG): bez zapisu licznika
    Stanowisko s;
    s.lad.nieprzyjmuje = 0x04;
    WynikLimitu w = s.wolaj(true, true);
    SPRAWDZ(w.wynik == Wynik::BLAD_ZAPISU);
    ROWNE_L(s.lad.zapisy.size(), 1);
    ROWNE_L(s.wd(), 1);
  }
  {  // licznik potwierdzony, ale nieprzyjęty: próba powrotu VREG do zastanego
    Stanowisko s;
    s.lad.nieprzyjmuje = 0x05;
    WynikLimitu w = s.wolaj(true, true);
    SPRAWDZ(w.wynik == Wynik::BLAD_ZAPISU);
    ROWNE_L(s.vreg(), 4208);
    ROWNE_L(s.wd(), 1);
  }
}

static void testWylaczeniePrzywraca() {
  Stanowisko s;
  s.wolaj(true, true);
  s.lad.zapisy.clear();
  WynikLimitu w = s.wolaj(false, true);
  SPRAWDZ(w.wynik == Wynik::PRZYWROCONO);
  // tylko VREG do zastanego; licznik zostaje wyłączony (włączony przywróciłby po 40 s wszystkie wartości domyślne)
  SPRAWDZ(s.lad.zapisy == (std::vector<std::pair<uint8_t, uint8_t>>{{0x04, 0x58}}));
  ROWNE_L(s.lad.r[4], 0x58);
  ROWNE_L(s.lad.r[5], 0x8F);
  SPRAWDZ(!s.wpis());
  // następne wybudzenia: zero zapisów, także na baterii
  s.lad.zapisy.clear();
  SPRAWDZ(s.wolaj(false, true).wynik == Wynik::WYLACZONY);
  SPRAWDZ(s.wolaj(false, false).wynik == Wynik::WYLACZONY);
  SPRAWDZ(s.lad.zapisy.empty());
  // przywracanie działa też na baterii (opcja wyłączona w czasie pracy bez kabla)
  Stanowisko b;
  b.wolaj(true, true);
  SPRAWDZ(b.wolaj(false, false).wynik == Wynik::PRZYWROCONO && b.vreg() == 4208 && b.wd() == 0);
  // nieudane przywrócenie: wpis zostaje do następnego wybudzenia
  Stanowisko c;
  c.wolaj(true, true);
  c.lad.nieudanyZapisNr = (int)c.lad.zapisy.size() + 1;
  SPRAWDZ(c.wolaj(false, true).wynik == Wynik::BLAD_ZAPISU);
  SPRAWDZ(c.wpis());
  SPRAWDZ(c.wolaj(false, true).wynik == Wynik::PRZYWROCONO && c.vreg() == 4208 && !c.wpis());
  // układ po resecie (limitu w nim nie ma): wpis nieaktualny, bez zapisów, wpis usunięty
  Stanowisko d;
  d.wolaj(true, true);
  d.lad.startZasilania();
  d.lad.zapisy.clear();
  SPRAWDZ(d.wolaj(false, true).wynik == Wynik::WPIS_NIEAKTUALNY && d.lad.zapisy.empty() && !d.wpis());
}

static void testPoResecie() {
  Stanowisko s;
  s.wolaj(true, true);
  int nvsPrzed = s.nvs.zapisy;
  // reset układu (REG_RST albo zanik obu zasilań): rejestry domyślne, wpis w NVS zostaje
  s.lad.startZasilania();
  s.lad.zapisy.clear();
  WynikLimitu w = s.wolaj(true, true);
  SPRAWDZ(w.wynik == Wynik::USTAWIONO && s.vreg() == 3920 && s.wd() == 0);
  ROWNE_L(s.lad.zapisy.size(), 2);
  ROWNE_L(s.nvs.zapisy, nvsPrzed);  // zastane już znane: bez ponownego zapisu do NVS
  // na baterii po resecie: bez zapisu (ponowne ustawienie przy następnym wybudzeniu na kablu)
  s.lad.startZasilania();
  s.lad.zapisy.clear();
  SPRAWDZ(s.wolaj(true, false).wynik == Wynik::NIE_NA_KABLU && s.lad.zapisy.empty());
}

// Odczyt z jednego egzemplarza panelu 16.11.2026 21:22, na kablu, przed jakimkolwiek zapisem (obraz e1001_pomiar):
// układ w trybie host (REG09=00) z licznikiem wyłączonym (REG05=8F) i prądem 480 mA (REG02=88) od wcześniejszego
// oprogramowania. Limit zmienia tylko VREG (jeden zapis), wyłączenie wraca do 58 bez włączania licznika, a reszta
// rejestrów (w tym prąd) zostaje nietknięta.
static void testOdczytZEgzemplarza() {
  const uint8_t odczyt[12] = {0x04, 0x1A, 0x88, 0x22, 0x58, 0x8F, 0x62, 0x4C, 0xBC, 0x00, 0x80, 0x40};
  Stanowisko s;
  for (int i = 0; i < 12; i++) s.lad.r[i] = odczyt[i];
  s.lad.host = true;
  ROWNE_L(s.vreg(), 4208);  // 04=58: VREG 01011
  ROWNE_L(s.wd(), 0);       // 05=8F: licznik wyłączony
  WynikLimitu w = s.wolaj(true, true);
  SPRAWDZ(w.wynik == Wynik::USTAWIONO);
  SPRAWDZ(s.lad.zapisy == (std::vector<std::pair<uint8_t, uint8_t>>{{0x04, 0x10}}));
  ROWNE_L(s.nvs.liczbyU["cpap/lad_zast"], 0x5A008F58);
  for (int i = 0; i < 12; i++)
    if (i != 4) SPRAWDZ(s.lad.r[i] == odczyt[i]);
  ROWNE_L(s.lad.r[4], 0x10);
  s.lad.uplywWatchdoga();  // licznik wyłączony: nic się nie dzieje
  ROWNE_L(s.lad.r[4], 0x10);
  s.lad.zapisy.clear();
  w = s.wolaj(false, true);
  SPRAWDZ(w.wynik == Wynik::PRZYWROCONO);
  SPRAWDZ(s.lad.zapisy == (std::vector<std::pair<uint8_t, uint8_t>>{{0x04, 0x58}}));
  for (int i = 0; i < 12; i++) SPRAWDZ(s.lad.r[i] == odczyt[i]);  // dokładnie stan zastany
  s.lad.uplywWatchdoga();
  ROWNE_L(s.lad.r[2], 0x88);  // prąd 480 mA zostaje (licznik nie został włączony)
  // NVS wyczyszczona przy ustawionym limicie: opcja wył. = zero zapisów i wiersz logu; zdjęcie przez włączenie
  // (wpis VREG z karty, bez zapisu do układu) i wyłączenie (VREG 4208 mV, licznik dalej wyłączony)
  Stanowisko t;
  for (int i = 0; i < 12; i++) t.lad.r[i] = odczyt[i];
  t.lad.host = true;
  t.wolaj(true, true);
  t.nvs.usun("cpap", "lad_zast");
  t.lad.zapisy.clear();
  SPRAWDZ(t.wolaj(false, true).wynik == Wynik::WYLACZONY_NIZEJ && t.lad.zapisy.empty() && t.lad.r[4] == 0x10);
  SPRAWDZ(t.wolaj(true, true).wynik == Wynik::USTAWIONY && t.lad.zapisy.empty() && t.wpis());
  SPRAWDZ(t.wolaj(false, true).wynik == Wynik::PRZYWROCONO);
  for (int i = 0; i < 12; i++) SPRAWDZ(t.lad.r[i] == odczyt[i]);
}

static void testBezWpisuWNvs() {
  // NVS wyczyszczona, układ z naszym ustawieniem (licznik 00, VREG 3920): wyłączona opcja = zero zapisów
  Stanowisko s;
  s.wolaj(true, true);
  s.nvs.usun("cpap", "lad_zast");
  SPRAWDZ(!s.wpis());
  s.lad.zapisy.clear();
  WynikLimitu w = s.wolaj(false, true);
  SPRAWDZ(w.wynik == Wynik::WYLACZONY_NIZEJ && s.lad.zapisy.empty() && s.vreg() == 3920);
  // włączona: ustawienie rozpoznane, bez zapisów do układu, wpis z VREG z karty (nie obniżony stan jako „zastane”)
  Stanowisko t;
  t.wolaj(true, true);
  t.nvs.usun("cpap", "lad_zast");
  t.lad.zapisy.clear();
  SPRAWDZ(t.wolaj(true, true).wynik == Wynik::USTAWIONY && t.lad.zapisy.empty() && t.wpis());
  SPRAWDZ(t.pam.wczytaj().reg04 == 0x58);
  // przyszła wersja z innym progiem (4000 mV), NVS wyczyszczona, w układzie limit 3920 mV z tej wersji: rozpoznany
  // z listy KODY_LIMITU; nie podnosi, a włączenie i wyłączenie opcji zdejmuje limit
  Stanowisko f;
  f.wolaj(true, true);
  f.nvs.usun("cpap", "lad_zast");
  f.lad.zapisy.clear();
  SPRAWDZ(f.wolaj(true, true, 4000).wynik == Wynik::USTAWIONY && f.lad.zapisy.empty() && f.vreg() == 3920);
  SPRAWDZ(f.wolaj(false, true, 4000).wynik == Wynik::PRZYWROCONO && f.vreg() == 4208);
  // tryb host bez licznika z innym VREG (np. inne oprogramowanie): zastane = bieżące, przywrócone dokładnie
  Stanowisko u;
  u.lad.r[5] = 0x8F;
  u.lad.r[4] = 0xB0;  // kod 22 = 4560 mV
  u.lad.host = true;
  SPRAWDZ(u.wolaj(true, true).wynik == Wynik::USTAWIONO);
  SPRAWDZ(u.pam.wczytaj().reg04 == 0xB0 && u.pam.wczytaj().reg05 == 0x8F);
  // wpis powyżej 4208 mV = uszkodzony: VREG 4208 mV z karty, nigdy 4560 mV; licznik bez zmian
  SPRAWDZ(u.wolaj(false, true).wynik == Wynik::PRZYWROCONO_Z_KARTY && u.vreg() == 4208 && u.wd() == 0);
  // wpis z kodem 11111 i 01111: tak samo
  for (uint32_t kod : {0xF8u, 0x78u}) {
    Stanowisko v;
    v.wolaj(true, true);
    v.nvs.liczbyU["cpap/lad_zast"] = 0x5A009F00u | kod;
    SPRAWDZ(v.wolaj(false, true).wynik == Wynik::PRZYWROCONO_Z_KARTY && v.vreg() == 4208 && v.wd() == 0);
  }
}

static void testProgiBrzegowe() {
  {  // zastane niższe niż limit: nic do obniżenia, licznik nietknięty
    Stanowisko s;
    s.lad.r[4] = 0x08;  // 3888 mV
    SPRAWDZ(s.wolaj(true, true).wynik == Wynik::NIZEJ_FABRYCZNIE);
    SPRAWDZ(s.lad.zapisy.empty() && s.nvs.zapisy == 0);
  }
  {  // zastane równe limitowi
    Stanowisko s;
    s.lad.r[4] = 0x10;
    SPRAWDZ(s.wolaj(true, true).wynik == Wynik::NIZEJ_FABRYCZNIE && s.lad.zapisy.empty());
  }
  {  // limit 3920 i 3919 mV
    Stanowisko a, b;
    a.wolaj(true, true, 3920);
    b.wolaj(true, true, 3919);
    ROWNE_L(a.vreg(), 3920);
    ROWNE_L(b.vreg(), 3888);
  }
  {  // limit poniżej najniższego VREG: bez zapisu
    Stanowisko s;
    SPRAWDZ(s.wolaj(true, true, 3855).wynik == Wynik::POZA_KARTA && s.lad.zapisy.empty());
  }
  {  // limit nie niższy niż zastane 4208: bez zapisu
    Stanowisko s;
    SPRAWDZ(s.wolaj(true, true, 4208).wynik == Wynik::NIZEJ_FABRYCZNIE && s.lad.zapisy.empty());
    SPRAWDZ(s.wolaj(true, true, 5000).wynik == Wynik::NIZEJ_FABRYCZNIE && s.lad.zapisy.empty());
  }
}

// ---------- (c) własność: nigdy wyżej niż zastane, tylko dwa pola ----------
static void testWlasnoscNigdyWyzej() {
  int przypadkow = 0, zapisowVreg = 0;
  const uint8_t r5s[] = {0x9F, 0x8F, 0xBF, 0xFF, 0x00, 0x1F, 0xAF, 0xCF};
  for (int r4 = 0; r4 < 256; r4++)
    for (uint8_t r5 : r5s)
      for (int wpis = -1; wpis < 25; wpis += 3)  // -1 = bez wpisu, inaczej kod zastanego w NVS
        for (int wl = 0; wl < 2; wl++)
          for (int kabel = 0; kabel < 2; kabel++)
            for (int limit : {3700, 3855, 3856, 3920, 3950, 4100, 4208, 4400}) {
              Stanowisko s;
              s.lad.r[4] = (uint8_t)r4;
              s.lad.r[5] = r5;
              s.lad.host = true;
              if (wpis >= 0) {
                Zastane z;
                z.znane = true;
                z.reg04 = (uint8_t)((wpis << 3) | (r4 & 7));
                z.reg05 = 0x9F;
                s.pam.zapisz(z);
              }
              // granica: każdy zapis VREG ≤ 4208 mV; przy opcji wł. dodatkowo ≤ VREG układu przed wywołaniem
              const int vPrzed = napiecieVreg((r4 & 0xF8) >> 3);
              uint8_t przed4 = s.lad.r[4], przed5 = s.lad.r[5];
              WynikLimitu w = s.wolaj(wl != 0, kabel != 0, limit);
              przypadkow++;
              for (auto& z : s.lad.zapisy) {
                bool dozwolony = z.first == 0x04 || z.first == 0x05;
                if (!dozwolony) printf("  zapis do rejestru %02X\n", z.first);
                SPRAWDZ(dozwolony);
                if (z.first == 0x04) {
                  zapisowVreg++;
                  int mv = napiecieVreg((z.second & 0xF8) >> 3);
                  bool nieWyzej = mv >= 0 && mv <= 4208 && (!wl || mv <= vPrzed);
                  if (!nieWyzej)
                    printf("  VREG %d mV ponad granicę (przed %d mV, r4 %02X r5 %02X wpis %d wl %d)\n", mv, vPrzed, r4,
                           r5, wpis, wl);
                  SPRAWDZ(nieWyzej);
                  SPRAWDZ((z.second & 0x07) == (przed4 & 0x07));  // bity 2:0 bez zmian
                  if (wl && kabel && w.wynik == Wynik::USTAWIONO) SPRAWDZ(mv <= limit);
                }
                if (z.first == 0x05) {
                  SPRAWDZ((z.second & 0xCF) == (przed5 & 0xCF));  // tylko bity 5:4
                  SPRAWDZ((z.second & 0x30) == 0);  // licznik nigdy nie jest włączany
                }
              }
              if (!wl && wpis < 0) SPRAWDZ(s.lad.zapisy.empty());  // wyłączona bez wpisu: zero zapisów
              if (wl && !kabel) SPRAWDZ(s.lad.zapisy.empty());
            }
  printf("własność „VREG nigdy ponad 4208 mV, przy limicie nigdy w górę”: %d przypadków, %d zapisów VREG\n", przypadkow,
         zapisowVreg);
  SPRAWDZ(zapisowVreg > 1000);
}

// ---------- (d) konfiguracja i formularz ----------
static void testKonfiguracja() {
  SPRAWDZ(limitWKonfiguracji("ladowanie_limit=tak\n"));
  SPRAWDZ(limitWKonfiguracji("profil=prywatny\nladowanie_limit=tak"));
  SPRAWDZ(limitWKonfiguracji("ladowanie_limit=tak \r\n"));
  SPRAWDZ(limitWKonfiguracji("\xEF\xBB\xBFladowanie_limit=tak\n"));
  SPRAWDZ(!limitWKonfiguracji(""));
  SPRAWDZ(!limitWKonfiguracji("ladowanie_limit=nie\n"));
  SPRAWDZ(!limitWKonfiguracji("ladowanie_limit=TAK\n"));
  SPRAWDZ(!limitWKonfiguracji("ladowanie_limit=\n"));
  SPRAWDZ(!limitWKonfiguracji("#ladowanie_limit=tak\n"));
  SPRAWDZ(!limitWKonfiguracji(" ladowanie_limit=tak\n"));
  SPRAWDZ(!limitWKonfiguracji("ladowanie_limit=tak\nladowanie_limit=nie\n"));  // ostatni wiersz wygrywa
  SPRAWDZ(limitWKonfiguracji("ladowanie_limit=nie\nladowanie_limit=tak\n"));
}

static bool zawiera(const std::string& s, const std::string& t) { return s.find(t) != std::string::npos; }

static void testFormularz() {
  sekrety::Dane ob;
  SPRAWDZ(formularz::zPliku("", ob).ladowanieLimit == "nie");
  SPRAWDZ(formularz::zPliku("ladowanie_limit=tak\n", ob).ladowanieLimit == "tak");
  SPRAWDZ(formularz::zPliku("ladowanie_limit=cos\n", ob).ladowanieLimit == "nie");
  auto w = formularz::waliduj(formularz::parsujCialo("ladowanie_limit=tak"), ob, "2026-11-16");
  SPRAWDZ(w.ok() && w.plik["ladowanie_limit"] == "tak");
  std::string plik = formularz::scal("profil=prywatny\n", w.plik);
  SPRAWDZ(plik == "profil=prywatny\nladowanie_limit=tak\n" && limitWKonfiguracji(plik));
  w = formularz::waliduj(formularz::parsujCialo("ladowanie_limit=nie"), ob, "2026-11-16");
  SPRAWDZ(w.ok() && w.plik.count("ladowanie_limit") && w.plik["ladowanie_limit"].empty());
  plik = formularz::scal(plik, w.plik);
  SPRAWDZ(plik == "profil=prywatny\n" && !limitWKonfiguracji(plik));  // domyślne `nie`: wiersz znika
  w = formularz::waliduj(formularz::parsujCialo("ladowanie_limit=moze"), ob, "2026-11-16");
  SPRAWDZ(!w.ok() && w.bledy.count("ladowanie_limit") && !w.plik.count("ladowanie_limit"));
  // strona: pole w sekcji „Panel”, wybrana wartość z pliku, opis skutku
  std::string s = formularz::strona(formularz::zPliku("ladowanie_limit=tak\n", ob), "/u/abc/", "", {});
  size_t pan = s.find("<b>Panel</b>"), pol = s.find("name=\"ladowanie_limit\""), kon = s.find("</details>", pan);
  SPRAWDZ(pan != std::string::npos && pol > pan && pol < kon);
  SPRAWDZ(zawiera(s, "Oszczędzanie baterii na kablu: ładuj do ok. 70%"));
  SPRAWDZ(zawiera(s, "Po odłączeniu kabla ma ok. 70%, nie 100%"));
  SPRAWDZ(zawiera(s, "<option value=\"tak\" selected>"));
  std::string s0 = formularz::strona(formularz::zPliku("", ob), "/u/abc/", "", {});
  SPRAWDZ(zawiera(s0, "<option value=\"nie\" selected>"));
}

// ---------- (e) dyrygent z atrapami ----------
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t epoka(int h, int m) {
  std::tm t{};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 10;
  t.tm_mday = 16;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
struct PanelL {
  Panel p;
  AtrapaLadowarki lad;
  explicit PanelL(const std::string& konf) {
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.czujniki.lad = &lad;
    if (!konf.empty()) p.pliki.pliki["/konfiguracja.txt"] = konf;
  }
  void wybudz(Zasilanie z, uint32_t t) {
    p.czujniki.zas = z;
    p.zegar.epoch = t;
    p.zegar.msTeraz = 0;
    p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
    p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
    p.d.z.clear();
    Dyrygent(p.adaptery(), KONF).wybudzenie();
  }
  int wierszy(const std::string& t) const {
    int n = 0;
    for (auto& z : p.d.z) n += zawiera(z, t);
    return n;
  }
};

static void testDyrygent() {
  {  // bez opcji: zero zapisów do układu i bez wiersza w logu (regresja zachowania sprzed ADR-0046)
    PanelL s("");
    s.wybudz(Zasilanie::ZEWNETRZNE, epoka(14, 0));
    s.wybudz(Zasilanie::BATERIA, epoka(14, 30));
    SPRAWDZ(s.lad.zapisy.empty());
    ROWNE_L(s.wierszy("ladowanie:"), 0);
  }
  {  // z opcją na kablu: limit ustawiony raz, w logu wynik; kolejne wybudzenie bez zapisów
    PanelL s("ladowanie_limit=tak\n");
    s.wybudz(Zasilanie::ZEWNETRZNE, epoka(14, 0));
    ROWNE_L(s.lad.zapisy.size(), 2);
    ROWNE_L(s.wierszy("ladowanie: limit ustawiono, VREG 3920 mV, watchdog 0, zapisow 2"), 1);
    s.wybudz(Zasilanie::ZEWNETRZNE, epoka(14, 30));
    ROWNE_L(s.lad.zapisy.size(), 2);
    ROWNE_L(s.wierszy("ladowanie: limit juz ustawiony"), 1);
    // wyłączenie opcji w pliku: przywrócenie przy następnym wybudzeniu, także na baterii
    s.p.pliki.pliki["/konfiguracja.txt"] = "ladowanie_limit=nie\n";
    s.wybudz(Zasilanie::BATERIA, epoka(15, 0));
    ROWNE_L(s.lad.r[4], 0x58);
    ROWNE_L(s.lad.r[5], 0x8F);  // licznik zostaje wyłączony (ADR-0046 pkt 4)
    ROWNE_L(s.wierszy("ladowanie: przywrocono zastane"), 1);
  }
  {  // z opcją na baterii: bez zapisów
    PanelL s("ladowanie_limit=tak\n");
    s.wybudz(Zasilanie::BATERIA, epoka(14, 0));
    SPRAWDZ(s.lad.zapisy.empty());
    ROWNE_L(s.wierszy("ladowanie: nie na kablu"), 1);
  }
  {  // pamięć plików niezamontowana: opcja nieznana, nic się nie zmienia (ani limit, ani przywrócenie)
    PanelL s("ladowanie_limit=tak\n");
    s.wybudz(Zasilanie::ZEWNETRZNE, epoka(14, 0));
    size_t z = s.lad.zapisy.size();
    s.p.pliki.montujeSie = false;
    s.p.pliki.montowane = false;
    s.wybudz(Zasilanie::ZEWNETRZNE, epoka(14, 30));
    ROWNE_L(s.lad.zapisy.size(), z);
    ROWNE_L(s.lad.r[4], 0x10);
  }
  {  // adapter bez ładowarki z I2C: bez zmian w zachowaniu
    PanelL s("ladowanie_limit=tak\n");
    s.p.czujniki.lad = nullptr;
    s.wybudz(Zasilanie::ZEWNETRZNE, epoka(14, 0));
    SPRAWDZ(s.lad.zapisy.empty() && s.lad.odczytow == 0);
  }
}

// ---------- (c2) twarda granica VREG (ADR-0046 pkt 5) ----------
// Wszystkie 256 wartości bajtu REG04 we wpisie NVS (i brak wpisu) × wszystkie 256 wartości REG04 układu × opcja wł./wył.
// × zasilanie × REG05 z licznikiem wył./wł. × bez błędu / drugi zapis nieudany (ścieżka cofnięcia), w sekwencji trzech
// wywołań: opcja, odwrotność, opcja. Po każdym wywołaniu i przy każdym zapisie:
//  - żaden zapis REG04 nie ma VREG powyżej 4208 mV;
//  - żaden zapis REG04 nie ma VREG powyżej wartości układu na wejściu wywołania, poza jawnym przywróceniem: opcja
//    wyłączona i na wejściu kod limitu (3920 mV); wtedy też najwyżej 4208 mV;
//  - VREG układu po wywołaniu nie jest wyżej niż na wejściu, poza tym samym przywróceniem;
//  - licznik nigdy nie jest włączany; opcja wyłączona bez wpisu = zero zapisów.
static void testGranicaVregPelna() {
  long wywolan = 0, zapisowVreg = 0, przywrocen = 0, zlych = 0;
  for (int wpis = -1; wpis < 256; wpis++)
    for (int r4 = 0; r4 < 256; r4++)
      for (uint8_t r5 : {(uint8_t)0x8F, (uint8_t)0x9F})
        for (int wl = 0; wl < 2; wl++)
          for (int kabel = 0; kabel < 2; kabel++)
            for (int blad : {-1, 2}) {
              Stanowisko s;
              s.lad.r[4] = (uint8_t)r4;
              s.lad.r[5] = r5;
              s.lad.host = r5 == 0x8F;
              s.lad.nieudanyZapisNr = blad;
              if (wpis >= 0) s.nvs.liczbyU["cpap/lad_zast"] = (0x5Au << 24) | ((uint32_t)r5 << 8) | (uint32_t)wpis;
              for (int krok = 0; krok < 3; krok++) {
                const bool opcja = (krok == 1) ? !wl : (wl != 0);
                const int kodPrzed = (s.lad.r[4] & 0xF8) >> 3, vPrzed = napiecieVreg(kodPrzed);
                const bool bylWpis = s.wpis();
                const bool przywrocenie = !opcja && kodPrzed == kodVregNieWyzej(LIMIT_LADOWANIA_MV);
                const size_t z0 = s.lad.zapisy.size();
                s.wolaj(opcja, kabel != 0);
                wywolan++;
                bool zle = false;
                for (size_t i = z0; i < s.lad.zapisy.size(); i++) {
                  const auto& z = s.lad.zapisy[i];
                  if (z.first == 0x04) {
                    zapisowVreg++;
                    const int mv = napiecieVreg((z.second & 0xF8) >> 3);
                    if (mv > 4208) zle = true;
                    if (mv > vPrzed && !przywrocenie) zle = true;
                    if (mv > vPrzed) przywrocen++;
                  } else if (z.first == 0x05) {
                    if ((z.second & 0x30) != 0) zle = true;
                  } else {
                    zle = true;
                  }
                }
                const int vPo = s.vreg();
                if (vPo > vPrzed && !(przywrocenie && vPo <= 4208)) zle = true;
                if (!opcja && !bylWpis && s.lad.zapisy.size() != z0) zle = true;
                if (zle) {
                  if (zlych < 5)
                    printf("  granica VREG: wpis %d r4 %02X r5 %02X wl %d kabel %d błąd %d krok %d: przed %d mV, po %d mV\n",
                           wpis, r4, r5, wl, kabel, blad, krok, vPrzed, vPo);
                  zlych++;
                }
              }
            }
  printf("twarda granica VREG: %ld wywołań, %ld zapisów VREG, %ld jawnych przywróceń w górę, %ld naruszeń\n", wywolan,
         zapisowVreg, przywrocen, zlych);
  ROWNE_L(zlych, 0);
  SPRAWDZ(zapisowVreg > 10000);
  SPRAWDZ(przywrocen > 1000);
}

// przypadki brzegowe poprawione 16.11.2026, z dokładnym oczekiwanym wynikiem.
static void testPoprawkiBrzegowe() {
  {  // wpis z kodem 01111 (4352 mV) przy limicie w układzie: przywrócenie do 4208 mV z karty, nie 4352 mV
    Stanowisko s;
    s.lad.r[4] = 0x10;
    s.lad.r[5] = 0x8F;
    s.nvs.liczbyU["cpap/lad_zast"] = 0x5A008F78u;
    SPRAWDZ(s.wolaj(false, true).wynik == Wynik::PRZYWROCONO_Z_KARTY);
    SPRAWDZ(s.lad.zapisy == (std::vector<std::pair<uint8_t, uint8_t>>{{0x04, 0x58}}) && !s.wpis());
  }
  {  // cofnięcie po nieudanym zapisie licznika przy wejściu 4624 mV: wraca najwyżej do 4208 mV
    Stanowisko s;
    s.lad.r[4] = 0xC0;  // 11000 = 4624 mV, licznik włączony (9F)
    s.lad.nieudanyZapisNr = 2;
    SPRAWDZ(s.wolaj(true, true).wynik == Wynik::BLAD_ZAPISU);
    SPRAWDZ(s.lad.zapisy.size() == 3 && s.lad.zapisy[2] == (std::pair<uint8_t, uint8_t>{0x04, 0x58}));
  }
  {  // układ 3856 mV, wpis mówi 4208 mV, opcja wł.: nic nie podnosimy, wpis usunięty jako nieaktualny
    Stanowisko s;
    s.lad.r[4] = 0x00;
    s.lad.r[5] = 0x8F;
    s.nvs.liczbyU["cpap/lad_zast"] = 0x5A008F58u;
    SPRAWDZ(s.wolaj(true, true).wynik == Wynik::NIZEJ_FABRYCZNIE && s.lad.zapisy.empty() && s.vreg() == 3856);
    SPRAWDZ(!s.wpis());
  }
  {  // wpis z trybu domyślnego (9F), układ z limitem (10/8F): przywraca tylko VREG, licznika nie włącza
    Stanowisko s;
    s.lad.r[4] = 0x10;
    s.lad.r[5] = 0x8F;
    s.lad.r[2] = 0x88;
    s.lad.host = true;
    s.nvs.liczbyU["cpap/lad_zast"] = 0x5A009F58u;
    SPRAWDZ(s.wolaj(false, true).wynik == Wynik::PRZYWROCONO && s.lad.r[4] == 0x58 && s.lad.r[5] == 0x8F);
    s.lad.uplywWatchdoga();
    ROWNE_L(s.lad.r[2], 0x88);  // prąd 480 mA zostaje
  }
  {  // bez wpisu + droga zdejmowania: obcy limit 3920 mV (10/8F), opcja nigdy niewłączona: zero zapisów; potem włączenie
     // (wpis z karty, zero zapisów do układu) i wyłączenie: 4208 mV, jeden zapis, licznik dalej wyłączony
    Stanowisko s;
    s.lad.r[4] = 0x10;
    s.lad.r[5] = 0x8F;
    s.lad.host = true;
    SPRAWDZ(s.wolaj(false, true).wynik == Wynik::WYLACZONY_NIZEJ && s.lad.zapisy.empty() && s.nvs.zapisy == 0);
    SPRAWDZ(s.wolaj(true, true).wynik == Wynik::USTAWIONY && s.lad.zapisy.empty() && s.wpis());
    SPRAWDZ(s.wolaj(false, false).wynik == Wynik::PRZYWROCONO && s.vreg() == 4208 && s.wd() == 0);
    SPRAWDZ(s.lad.zapisy == (std::vector<std::pair<uint8_t, uint8_t>>{{0x04, 0x58}}) && !s.wpis());
    SPRAWDZ(s.wolaj(false, true).wynik == Wynik::WYLACZONY && s.lad.zapisy.size() == 1);
  }
}

// błąd odczytu NVS nie jest „brakiem wpisu”. Sekwencja: zastane 3952 mV,
// opcja wł. (limit), w następnych wybudzeniach błąd odczytu wpisu, potem wyłączenie opcji: koniec na 3952 mV, nie 4208.
static void testBladOdczytuNvs() {
  Stanowisko s;
  s.lad.r[4] = 0x18;  // 00011 = 3952 mV
  s.lad.r[5] = 0x8F;
  s.lad.host = true;
  SPRAWDZ(s.wolaj(true, true).wynik == Wynik::USTAWIONO && s.vreg() == 3920 && s.pam.wczytaj().reg04 == 0x18);
  s.nvs.bledneOdczytyU.insert("cpap/lad_zast");
  const size_t z0 = s.lad.zapisy.size();
  const int n0 = s.nvs.zapisy;
  for (bool wl : {true, false})
    for (bool kabel : {true, false}) {
      WynikLimitu w = s.wolaj(wl, kabel);
      SPRAWDZ(w.wynik == Wynik::BLAD_PAMIECI);
    }
  SPRAWDZ(s.lad.zapisy.size() == z0 && s.nvs.zapisy == n0 && s.vreg() == 3920);  // zero zapisów do układu i NVS
  s.nvs.bledneOdczytyU.clear();
  SPRAWDZ(s.wolaj(false, true).wynik == Wynik::PRZYWROCONO && s.vreg() == 3952 && !s.wpis());
  // błąd odczytu przy podpisie limitu: nie powstaje wpis „z karty”
  Stanowisko t;
  t.lad.r[4] = 0x10;
  t.lad.r[5] = 0x8F;
  t.nvs.bledneOdczytyU.insert("cpap/lad_zast");
  SPRAWDZ(t.wolaj(true, true).wynik == Wynik::BLAD_PAMIECI && t.nvs.zapisy == 0 && t.lad.zapisy.empty());
  // NvsTylkoZmiany przekazuje błąd adaptera
  Dziennik d;
  AtrapaNvs surowa(d);
  surowa.bledneOdczytyU.insert("cpap/lad_zast");
  NvsTylkoZmiany nz(surowa);
  uint32_t v = 0;
  SPRAWDZ(nz.liczbaUStan("cpap", "lad_zast", v) == Nvs::Odczyt::BLAD);
  surowa.bledneOdczytyU.clear();
  SPRAWDZ(nz.liczbaUStan("cpap", "lad_zast", v) == Nvs::Odczyt::BRAK);
  surowa.liczbyU["cpap/lad_zast"] = 0;
  SPRAWDZ(nz.liczbaUStan("cpap", "lad_zast", v) == Nvs::Odczyt::JEST && v == 0);
}

// wpis z kodem poniżej kodu limitu jest uszkodzony (VREG 4208 mV z karty). Własność po
// wszystkich 256 bajtach REG04 we wpisie (i braku wpisu) × 256 REG04 układu × REG05 × opcja × zasilanie w pierwszym
// wybudzeniu, potem dwa czyste wybudzenia z opcją wyłączoną: firmware nigdy nie zapisuje VREG poniżej limitu, a VREG
// poniżej limitu na końcu jest tylko wtedy, gdy układ miał go tak na starcie (nietknięty).
static void testWpisPonizejLimitu() {
  long przypadkow = 0, zlych = 0, przywrocenZKarty = 0;
  const int kodLim = kodVregNieWyzej(LIMIT_LADOWANIA_MV);
  for (int wpis = -1; wpis < 256; wpis++)
    for (int r4 = 0; r4 < 256; r4++)
      for (uint8_t r5 : {(uint8_t)0x8F, (uint8_t)0x9F})
        for (int wl = 0; wl < 2; wl++)
          for (int kabel = 0; kabel < 2; kabel++) {
            Stanowisko s;
            s.lad.r[4] = (uint8_t)r4;
            s.lad.r[5] = r5;
            s.lad.host = r5 == 0x8F;
            if (wpis >= 0) s.nvs.liczbyU["cpap/lad_zast"] = (0x5Au << 24) | ((uint32_t)r5 << 8) | (uint32_t)wpis;
            const int start = s.vreg();
            s.wolaj(wl != 0, kabel != 0);
            for (int i = 0; i < 2; i++)
              if (s.wolaj(false, true).wynik == Wynik::PRZYWROCONO_Z_KARTY) przywrocenZKarty++;
            przypadkow++;
            bool zle = false;
            for (auto& z : s.lad.zapisy)
              if (z.first == 0x04 && ((z.second & 0xF8) >> 3) < kodLim) zle = true;
            if (s.vreg() < napiecieVreg(kodLim) && s.vreg() != start) zle = true;
            if (zle) {
              if (zlych < 5) printf("  wpis poniżej limitu: wpis %d r4 %02X r5 %02X wl %d kabel %d: start %d, koniec %d mV\n",
                                    wpis, r4, r5, wl, kabel, start, s.vreg());
              zlych++;
            }
          }
  printf("wpis poniżej limitu: %ld przypadków, %ld przywróceń z karty, %ld naruszeń\n", przypadkow,
         przywrocenZKarty, zlych);
  ROWNE_L(zlych, 0);
  SPRAWDZ(przywrocenZKarty > 1000);
  {  // przypadek dokładny: limit w układzie, wpis z kodem 00000 (3856 mV) → 4208 mV, wpis usunięty
    Stanowisko s;
    s.lad.r[4] = 0x10;
    s.lad.r[5] = 0x8F;
    s.nvs.liczbyU["cpap/lad_zast"] = 0x5A008F00u;
    SPRAWDZ(s.wolaj(false, true).wynik == Wynik::PRZYWROCONO_Z_KARTY && s.vreg() == 4208 && !s.wpis());
  }
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  testKodyVreg();
  testWylaczonaBezZapisow();
  testWlaczonaNaKablu();
  testNaBaterii();
  testInnyUkladIBledy();
  testBladWPolowie();
  testWylaczeniePrzywraca();
  testOdczytZEgzemplarza();
  testPoResecie();
  testBezWpisuWNvs();
  testProgiBrzegowe();
  testWlasnoscNigdyWyzej();
  testGranicaVregPelna();
  testPoprawkiBrzegowe();
  testBladOdczytuNvs();
  testWpisPonizejLimitu();
  testKonfiguracja();
  testFormularz();
  testDyrygent();
  printf("testy_ladowarka: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
