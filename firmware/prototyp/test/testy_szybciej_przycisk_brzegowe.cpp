// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe T-WYM-99 (pamięci podręczne ścieżki „przycisk → rysowanie”, 16.11.2026): testy różnicowe
// z pamięcią podręczną i bez niej (szybkosc::bezPamieci) oraz mutacje pól wejścia kluczy.
//  A. skrotNocy(): każde pole Noc zmienione o najmniejszy krok zmienia skrót; zamiana pól; wypełnienie struktury
//     (padding) nie wchodzi do skrótu; luz rozmiaru Noc (przed poprawką z 16.11.2026 static_assert(sizeof(Noc)==320) nie
//     widział pola w 16 B wypełnienia; od poprawki Noc bez wypełnienia, a src/noc.cpp porównuje sizeof z listą pól).
//  B. stanyZPamieci(): każda mutacja pola nocy w środku historii, data i noce wyłączone = liczenie od nowa.
//  C. reguly::ocen() z PamiecOceny i bez: losowe historie (duplikaty dat, drzemki, noce bez terapii, zmiany trybu,
//     dziury), losowe sygnały, odpowiedzi i pamięć reguł; wyniki i wiersze logu identyczne.
//  D. wybudzenia dyrygenta z pamięcią i bez: wejścia silnika zmienne w trakcie jednego wybudzenia (północ w czuwaniu,
//     przytrzymania, bateria spadająca w czasie, `sp_model` w oknie 120 dni z infekcją), losowe sekwencje przycisków;
//     klatki e-ink, NVS, pliki i log identyczne.
// Wymaganie: T-WYM-99.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "czynniki.h"
#include "dyrygent.h"
#include "magazyn.h"
#include "noc.h"
#include "odkrycia.h"
#include "reguly.h"
#include "reguly_pamiec.h"
#include "reguly_pomoc.h"
#include "reguly_sygnaly.h"
#include "sprawy.h"
#include "stany.h"
#include "start.h"
#include "szybkosc.h"
#include "wspolne.h"
#include "wybudzenie.h"
#include "wzorce.h"

namespace reguly {
void zarejestrujRodzajeE();  // src/rodzaje_e.cpp
}

static int ok = 0, zle = 0;
#define SPRAWDZ(w, opis)                                                      \
  do {                                                                        \
    if (w) ok++;                                                              \
    else { zle++; printf("BŁĄD %s:%d: %s (%s)\n", __FILE__, __LINE__, #w, opis); } \
  } while (0)

// ---------- A. skrotNocy() ----------
static Noc nocPelna() {
  Noc n;
  n.data = "2026-10-15";
  n.wazna = true;
  n.m01 = 412;
  n.ahi = 2.25f; n.cmed = 9.5f; n.cp95 = 12.75f; n.wmed = 3.5f; n.wp95 = 14.25f;
  n.emed = 6.5f; n.ep95 = 8.25f; n.psmed = 3.75f; n.psp95 = 6.5f;
  n.ustawienia = "ASVAuto/EAI5/EAX10/ANS3/AXS10";
  n.zrodlo = 'A';
  n.czesciowa = false;
  n.epapStr = false;
  n.oa = 0.5f; n.ca = 1.25f; n.h = 0.75f; n.csr = 4.5f;
  n.import = 1790000000u;
  n.m13 = 14.5f;
  n.m17 = 1;
  n.pna = "AirCurve_10_CS_PaceWave";
  n.srn = "23222345678";
  n.m08 = 2.5f; n.mpa = 1.5f; n.mpi = 2.5f; n.ipc = 3.5f; n.eep = 4.5f; n.mns = 5.5f; n.mxs = 6.5f;
  n.ua = 0.25f; n.m10 = 0.05f; n.m10p = 0.2f;
  n.eai = 5.0f; n.eax = 10.0f; n.ans = 3.0f; n.axs = 10.0f; n.epr = 2.0f;
  n.tryb = "ASVAuto";
  n.m18 = 655;
  n.m14 = 512.5f;
  n.m15 = 7.25f;
  return n;
}
static float krok(float f) { return std::nextafter(f, INFINITY); }
using Mutacja = std::pair<const char*, std::function<void(Noc&)>>;
#define MF(pole) {#pole, [](Noc& n) { n.pole = krok(n.pole); }}, {#pole " NAN", [](Noc& n) { n.pole = NAN; }}
static std::vector<Mutacja> mutacjeNocy() {
  return {
      {"data", [](Noc& n) { n.data = "2026-10-16"; }},
      {"wazna", [](Noc& n) { n.wazna = !n.wazna; }},
      {"m01", [](Noc& n) { n.m01++; }},
      MF(ahi), MF(cmed), MF(cp95), MF(wmed), MF(wp95), MF(emed), MF(ep95), MF(psmed), MF(psp95),
      {"ustawienia", [](Noc& n) { n.ustawienia.back() = '1'; }},
      {"ustawienia pusta", [](Noc& n) { n.ustawienia.clear(); }},
      {"zrodlo", [](Noc& n) { n.zrodlo = 'K'; }},
      {"czesciowa", [](Noc& n) { n.czesciowa = !n.czesciowa; }},
      {"epapStr", [](Noc& n) { n.epapStr = !n.epapStr; }},
      MF(oa), MF(ca), MF(h), MF(csr),
      {"import", [](Noc& n) { n.import++; }},
      MF(m13),
      {"m17", [](Noc& n) { n.m17++; }},
      {"pna", [](Noc& n) { n.pna.back() = 'X'; }},
      {"srn", [](Noc& n) { n.srn.back() = '9'; }},
      MF(m08), MF(mpa), MF(mpi), MF(ipc), MF(eep), MF(mns), MF(mxs), MF(ua), MF(m10), MF(m10p),
      MF(eai), MF(eax), MF(ans), MF(axs), MF(epr),
      {"tryb", [](Noc& n) { n.tryb = "ASV"; }},
      {"m18", [](Noc& n) { n.m18++; }},
      MF(m14), MF(m15),
  };
}
#undef MF

// wymaganie: T-WYM-99
static void testA_skrotNocy() {
  const Noc b = nocPelna();
  const uint64_t kb = skrotNocy(SKROT_POCZATEK, b);
  int mut = 0;
  for (auto& m : mutacjeNocy()) {
    Noc n = b;
    m.second(n);
    mut++;
    if (skrotNocy(SKROT_POCZATEK, n) == kb) {
      zle++;
      printf("BŁĄD: skrotNocy() nie widzi zmiany pola %s\n", m.first);
    } else {
      ok++;
    }
  }
  printf("  A: %d mutacji pól Noc, każda zmienia skrót\n", mut);
  // zamiany wartości między polami tego samego typu i przesunięcie znaku między sąsiednimi tekstami
  {
    Noc n = b;
    std::swap(n.ahi, n.cmed);
    SPRAWDZ(skrotNocy(SKROT_POCZATEK, n) != kb, "zamiana ahi i cmed");
    n = b;
    std::swap(n.m01, n.m18);
    SPRAWDZ(skrotNocy(SKROT_POCZATEK, n) != kb, "zamiana m01 i m18");
    n = b;
    std::swap(n.pna, n.srn);
    SPRAWDZ(skrotNocy(SKROT_POCZATEK, n) != kb, "zamiana pna i srn");
    Noc x = b, y = b;
    x.pna = "AB"; x.srn = "C";
    y.pna = "A"; y.srn = "BC";
    SPRAWDZ(skrotNocy(SKROT_POCZATEK, x) != skrotNocy(SKROT_POCZATEK, y), "znak między pna i srn");
    x = b; y = b;
    x.tryb = ""; x.ustawienia = "ASVAuto";
    y.tryb = "ASVAuto"; y.ustawienia = "";
    SPRAWDZ(skrotNocy(SKROT_POCZATEK, x) != skrotNocy(SKROT_POCZATEK, y), "ustawienia i tryb zamienione");
  }
  // kolejność nocy w historii i noc dopisana / usunięta
  {
    Noc n1 = b, n2 = b;
    n2.data = "2026-10-16";
    uint64_t a = skrotNocy(skrotNocy(SKROT_POCZATEK, n1), n2), c = skrotNocy(skrotNocy(SKROT_POCZATEK, n2), n1);
    SPRAWDZ(a != c, "kolejność dwóch nocy");
    SPRAWDZ(skrotNocy(SKROT_POCZATEK, n1) != a, "noc dopisana");
  }
  // wypełnienie struktury (padding): te same pola, różne bajty wypełnienia = ten sam skrót
  {
    alignas(Noc) unsigned char b1[sizeof(Noc)], b2[sizeof(Noc)];
    std::memset(b1, 0x00, sizeof b1);
    std::memset(b2, 0xFF, sizeof b2);
    Noc* p1 = new (b1) Noc();
    Noc* p2 = new (b2) Noc();
    *p1 = b;
    *p2 = b;
    SPRAWDZ(skrotNocy(SKROT_POCZATEK, *p1) == skrotNocy(SKROT_POCZATEK, *p2), "padding w skrócie");
    p1->~Noc();
    p2->~Noc();
  }
  // tekst ze znakiem 0xFF (nie występuje w UTF-8) koliduje z separatorem: informacyjnie
  {
    uint64_t a = skrotTekst(SKROT_POCZATEK, std::string("a\xFF")), c = skrotTekst(skrotTekst(SKROT_POCZATEK, "a"), "");
    printf("  A: separator skrotTekst: \"a\\xFF\" %s \"a\"+\"\" (bajt 0xFF nie występuje w UTF-8)\n",
           a == c ? "==" : "!=");
  }
  // Strażnik nowych pól: static_assert(sizeof(Noc) == suma pól z listy skrotNocy()) w src/noc.cpp. Suma rozmiarów pól
  // < sizeof(Noc) znaczy, że nowe pole może wejść w wypełnienie bez zmiany rozmiaru i strażnik go nie zauważy.
  // (Od poprawki z 16.11.2026 ostatnim polem jest teraz epapStr, a nie m15; sprawdzenie bez zmian.)
  {
    Noc n;
    size_t pola = 5 * sizeof(std::string) + 31 * sizeof(float) + 3 * sizeof(int) + sizeof(n.import) +
                  3 * sizeof(bool) + sizeof(char);
    size_t koniec = (size_t)((const char*)&n.epapStr - (const char*)&n) + sizeof(n.epapStr);
    printf("  A: sizeof(Noc) %zu, suma pól %zu, koniec ostatniego pola (epapStr) %zu: luz %zu B (w tym %zu B na końcu)\n",
           sizeof(Noc), pola, koniec, sizeof(Noc) - pola, sizeof(Noc) - koniec);
    SPRAWDZ(pola == sizeof(Noc),
            "static_assert(sizeof(Noc)==320) nie złapie nowego pola: np. float na końcu mieści się w wypełnieniu");
  }
}

// ---------- B. stanyZPamieci() ----------
static std::vector<Noc> historiaB(int ile, const std::string& ostatnia) {
  std::vector<Noc> h;
  for (int i = 0; i < ile; i++) {
    Noc n = nocPelna();
    n.data = dodajDni(ostatnia, i - (ile - 1));
    n.ahi = 1.0f + (float)(i % 7) * 0.5f;
    n.wp95 = 10.0f + (float)(i % 5);
    h.push_back(n);
  }
  return h;
}
// wymaganie: T-WYM-99
static void testB_stany() {
  const std::string dzis = "2026-11-04";
  const std::vector<Noc> baza = historiaB(120, dzis);
  const std::set<std::string> wyl;
  stanyZPamieci(baza, dzis, wyl);
  unsigned long o0 = licznikiStanow().obliczen;
  stanyZPamieci(baza, dzis, wyl);
  SPRAWDZ(licznikiStanow().obliczen == o0, "te same wejścia bez liczenia");
  int brak = 0;
  for (auto& m : mutacjeNocy()) {
    for (int gdzie : {0, 60, 119}) {
      std::vector<Noc> h = baza;
      m.second(h[(size_t)gdzie]);
      unsigned long przed = licznikiStanow().obliczen;
      stanyZPamieci(h, dzis, wyl);
      if (licznikiStanow().obliczen == przed) {
        brak++;
        printf("BŁĄD: stanyZPamieci() nie liczy od nowa po zmianie %s w nocy %d\n", m.first, gdzie);
      }
      stanyZPamieci(baza, dzis, wyl);
    }
  }
  SPRAWDZ(brak == 0, "mutacje pól nocy w stanach");
  unsigned long p = licznikiStanow().obliczen;
  stanyZPamieci(baza, "2026-11-05", wyl);
  SPRAWDZ(licznikiStanow().obliczen == p + 1, "inna data");
  stanyZPamieci(baza, "2026-11-05", {"2026-10-01"});
  SPRAWDZ(licznikiStanow().obliczen == p + 2, "inne noce wyłączone");
  std::vector<Noc> krotsza(baza.begin(), baza.end() - 1);
  stanyZPamieci(krotsza, "2026-11-05", {"2026-10-01"});
  SPRAWDZ(licznikiStanow().obliczen == p + 3, "noc usunięta z końca");
  std::vector<Noc> bezSrodka = baza;
  bezSrodka.erase(bezSrodka.begin() + 50);
  stanyZPamieci(bezSrodka, "2026-11-05", {"2026-10-01"});
  SPRAWDZ(licznikiStanow().obliczen == p + 4, "noc usunięta ze środka");
  printf("  B: stanyZPamieci() liczy od nowa po każdej mutacji (%d mutacji x 3 miejsca)\n", (int)mutacjeNocy().size());
}

// ---------- C. reguly::ocen() z PamiecOceny i bez ----------
static uint32_t ziarnoC = 1u;
static uint32_t losU(uint32_t n) {
  ziarnoC = ziarnoC * 1664525u + 1013904223u;
  return (ziarnoC >> 8) % (n ? n : 1);
}
static float losF(float a, float b) { return a + (b - a) * (float)losU(10001) / 10000.0f; }
static const char* const USTAWIENIA[] = {"ASVAuto/EAI5/EAX10/ANS3/AXS10", "ASV/6/10/3/15", "CPAP/10", "AutoSet/6/14",
                                         "", "VAuto/5/12"};
// Historia rosnąco po dacie z dziurami, duplikatami dat (drzemka częściowa i pełna noc), nocami bez terapii,
// nocami nieważnymi, zmianami trybu w środku i nocami z EPAP ze STR.
static std::vector<Noc> historiaLos(int ile, const std::string& ostatnia) {
  std::vector<Noc> h;
  size_t ust = losU(4);
  for (int i = 0; i < ile; i++) {
    std::string d = dodajDni(ostatnia, i - (ile - 1));
    if (losU(20) == 0) continue;  // dziura
    if (losU(40) == 0) ust = losU(6);  // zmiana trybu
    int powtorzen = losU(15) == 0 ? 2 : 1;
    for (int k = 0; k < powtorzen; k++) {
      Noc n;
      n.data = d;
      n.m01 = (int)losU(520);
      n.wazna = n.m01 >= 240 && losU(30) != 0;
      n.czesciowa = powtorzen == 2 && k == 0;
      n.ustawienia = losU(10) == 0 ? "" : USTAWIENIA[ust];
      if (n.m01 > 0) {
        n.ahi = losF(0.2f, i > ile - 15 ? 12.0f : 5.0f);
        n.cmed = losF(8, 11); n.cp95 = losF(11, 15);
        n.wmed = losF(1, 6); n.wp95 = losF(5, i > ile - 10 ? 40.0f : 18.0f);
        n.emed = losF(5, 8); n.ep95 = losF(7, 10); n.psmed = losF(2, 6); n.psp95 = losF(5, 9);
        n.oa = losF(0, 2); n.ca = losF(0, i > ile - 12 ? 9.0f : 3.0f); n.h = losF(0, 2); n.csr = losF(0, 30);
        n.m13 = losF(11, 18); n.m17 = (int)losU(4); n.m08 = losF(0, 40);
        n.m10 = losF(0, 0.2f); n.m10p = losF(0, 0.4f); n.ua = losF(0, 1);
        n.m14 = losF(350, 650); n.m15 = losF(5, 10); n.m18 = (int)losU(800);
        n.epapStr = losU(25) == 0;
        if (losU(8) == 0) n.ahi = NAN;
      }
      n.zrodlo = losU(3) == 0 ? 'K' : 'A';
      n.tryb = n.ustawienia.substr(0, n.ustawienia.find('/'));
      h.push_back(n);
    }
  }
  return h;
}
static std::string opisWynikow(const std::vector<reguly::Wynik>& w) {
  std::string s;
  for (auto& r : w) {
    s += r.id + "/" + r.wariant + ":" + (r.oceniona ? "o" : "-") + (r.spelniona ? "s" : "-") + ":" + r.powodNieoceny +
         ":" + std::to_string((int)r.stan) + std::to_string((int)r.pilnosc) + ":" + r.poziom + ":" + r.grupa;
    for (auto& kv : r.zmienne) s += "{" + kv.first + "=" + kv.second + "}";
    s += "\n";
  }
  return s;
}
struct WynikOceny {
  std::string wyniki, log;
};
static WynikOceny ocenRaz(const std::vector<Noc>& h, const std::string& dzis, reguly::Tryb tryb,
                          const reguly::Odpowiedzi& odp, const reguly::Pamiec& pam, const reguly::Sygnaly& syg,
                          const std::set<std::string>& wyl, bool bezPamieci) {
  szybkosc::bezPamieci = bezPamieci;
  Stany s = liczStany(h, dzis, wyl);
  reguly::Kontekst k;
  k.historia = &h;
  k.dzis = dzis;
  k.norma = &s.norma;
  k.stany = &s;
  k.trybBiezacy = tryb;
  k.odpowiedzi = &odp;
  k.pamiec = &pam;
  k.wykluczoneNoce = &wyl;
  k.zestaw = &reguly::zestawWbudowany();
  k.poczatekLeczenia = losU(2) ? "" : dodajDni(dzis, -(int)losU(400));
  WynikOceny w;
  reguly::OpcjeOceny o;
  o.sygnaly = &syg;
  o.log = [&w](const std::string& l) { w.log += l + "\n"; };
  w.wyniki = opisWynikow(reguly::ocen(odkrycia::bezOdkryc(reguly::zestawWbudowany()), k, o));
  szybkosc::bezPamieci = false;
  return w;
}
// wymaganie: T-WYM-99
static void testC_ocenZPamieciaOceny() {
  int roznych = 0, ocen = 0, spelnionych = 0;
  for (uint32_t z = 1; z <= 24; z++) {
    ziarnoC = z * 7919u;
    std::string ostatnia = dodajDni("2026-11-04", -(int)losU(200));
    std::vector<Noc> h = historiaLos(30 + (int)losU(380), ostatnia);
    reguly::Odpowiedzi odp;
    reguly::Pamiec pam;
    reguly::Sygnaly syg;
    std::set<std::string> wyl;
    for (int i = 0; i < 6; i++) {
      std::string d = dodajDni(ostatnia, -(int)losU(20));
      if (losU(2)) odp.sennosc[d] = losU(2) == 0;
      if (losU(2)) odp.gorzej[d] = losU(2) == 0;
      if (losU(4) == 0) odp.przerwaPlanowana.insert(d);
      if (losU(3) == 0) wyl.insert(dodajDni(ostatnia, -(int)losU(60)));
    }
    for (auto& w : reguly::zestawWbudowany().katalog)
      if (losU(6) == 0) {
        reguly::PamiecReguly p;
        p.aktywnaOd = dodajDni(ostatnia, -(int)losU(30));
        p.ostatnio = dodajDni(ostatnia, -(int)losU(5));
        p.licznik = (int)losU(5);
        if (losU(3) == 0) p.wiem = dodajDni(ostatnia, -(int)losU(10));
        if (losU(4) == 0) p.omowione = dodajDni(ostatnia, -(int)losU(40));
        pam[w.wariant.empty() ? w.id : w.id + "/" + w.wariant] = p;
      }
    syg.stan["brak_danych"] = losU(4) == 0;
    syg.stan["nieczytelny_raport"] = losU(6) == 0;
    syg.odczyty["M-25"] = losU(3) == 0 ? (double)losF(5, 30) : (double)losF(30, 100);
    for (const std::string& dzis : {ostatnia, dodajDni(ostatnia, 1), dodajDni(ostatnia, -3)}) {
      for (reguly::Tryb t : {reguly::Tryb::ASVAUTO, reguly::Tryb::CPAP, reguly::Tryb::AUTOSET}) {
        uint32_t zz = ziarnoC;
        WynikOceny a = ocenRaz(h, dzis, t, odp, pam, syg, wyl, true);
        ziarnoC = zz;
        WynikOceny b = ocenRaz(h, dzis, t, odp, pam, syg, wyl, false);
        ocen++;
        if (a.wyniki != b.wyniki || a.log != b.log) {
          roznych++;
          printf("BŁĄD: ocena z PamiecOceny różna od liczenia od nowa: ziarno %u, dzis %s, tryb %d\n", z, dzis.c_str(),
                 (int)t);
        }
        for (size_t i = 0; (i = b.wyniki.find(":os:", i)) != std::string::npos; i++) spelnionych++;
      }
    }
  }
  printf("  C: %d ocen reguly::ocen() z PamiecOceny i bez, różnych %d (warunków spełnionych w ocenach: %d)\n", ocen,
         roznych, spelnionych);
  SPRAWDZ(roznych == 0, "PamiecOceny zmienia wynik");
  SPRAWDZ(spelnionych > 0, "test bez spełnionych warunków niczego nie sprawdza");
}

// ---------- D. wybudzenia dyrygenta z pamięcią i bez ----------
static uint32_t ep(int r, int mi, int d, int h, int m, int s = 0) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static void bezLogu(const std::string&) {}

// Czujniki z napięciem baterii zależnym od czasu wybudzenia (sygnał baterii zmienia się między ocenami).
class CzujnikiZmienne : public AtrapaCzujnikow {
 public:
  explicit CzujnikiZmienne(AtrapaZegara& z) : z_(z) {}
  float vStart = 3.95f, vNaSekunde = 0.0f;
  int odczytow = 0;
  OdczytCzujnikow czytaj() override {
    odczytow++;
    OdczytCzujnikow r = o;
    r.bateriaV = vStart - vNaSekunde * (float)z_.msTeraz / 1000.0f;
    return r;
  }

 private:
  AtrapaZegara& z_;
};

struct PanelR {
  Panel p;
  CzujnikiZmienne cz{p.zegar};
  Adaptery adaptery() {
    return Adaptery{p.zegar, p.nvs, p.pliki, p.siec, p.ekran, p.przyciski, cz, p.dzwiek, p.start, p.straznik, p.sen,
                    p.port, p.diag, &p.wydania};
  }
};

// Historia jak w test/testy_szybciej_przycisk.cpp (pliki roczne, wersja 5), z losowym ziarnem i trybem.
static void przygotujR(PanelR& r, const std::string& ostatnia, int nocy) {
  Panel& p = r.p;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  p.pliki.montowane = true;
  Magazyn mag(p.pliki, bezLogu);
  std::map<std::string, std::vector<std::string>> lata;
  std::vector<Noc> h = historiaLos(nocy, ostatnia);
  for (auto& n : h) {
    if (n.zrodlo == 'A') {
      n.import = ep(2026, 1, 1, 7, 0) + (uint32_t)losU(3000000);
      n.pna = "AirCurve_10_CS_PaceWave";
      n.srn = "23222345678";
    }
    lata[n.data.substr(0, 4)].push_back(wierszNocyV5(n));
  }
  for (auto& kv : lata) SPRAWDZ(mag.zapisz("noce", mag.sciezkaRoku("noce", kv.first), kv.second), "zapis nocy");
  p.pliki.pliki["/konfiguracja.txt"] = "noc_zrodlo=json\n";
  p.pliki.montowane = false;
  p.d.z.clear();
}

struct Skrot {
  uint64_t k = 1469598103934665603ULL;
  void bajty(const void* d, size_t n) {
    const unsigned char* b = (const unsigned char*)d;
    for (size_t i = 0; i < n; i++) { k ^= b[i]; k *= 1099511628211ULL; }
    k ^= 0xFF; k *= 1099511628211ULL;
  }
  void tekst(const std::string& t) { bajty(t.data(), t.size()); }
};
struct StanPanelu {
  uint64_t klatki = 0, nvs = 0, pliki = 0;
  std::vector<std::string> log;
  int odswiezen = 0;
  bool operator==(const StanPanelu& o) const {
    return klatki == o.klatki && nvs == o.nvs && pliki == o.pliki && log == o.log && odswiezen == o.odswiezen;
  }
};
static StanPanelu stanPanelu(const Panel& p) {
  StanPanelu s;
  Skrot a, b, c;
  for (auto& k : p.ekran.klatki) a.bajty(k.data(), k.size());
  for (auto& kv : p.nvs.teksty) { b.tekst(kv.first); b.tekst(kv.second); }
  for (auto& kv : p.nvs.liczbyU) { b.tekst(kv.first); b.bajty(&kv.second, sizeof kv.second); }
  for (auto& kv : p.nvs.liczby) { b.tekst(kv.first); b.bajty(&kv.second, sizeof kv.second); }
  for (auto& kv : p.nvs.flagi) { b.tekst(kv.first); b.bajty(&kv.second, sizeof kv.second); }
  for (auto& kv : p.nvs.bajtyMap) { b.tekst(kv.first); b.tekst(kv.second); }
  b.bajty(&p.start.stan.ekran, sizeof p.start.stan.ekran);
  b.bajty(&p.start.stan.ekranOd, sizeof p.start.stan.ekranOd);
  for (auto& kv : p.pliki.pliki) { c.tekst(kv.first); c.tekst(kv.second); }
  s.klatki = a.k;
  s.nvs = b.k;
  s.pliki = c.k;
  for (auto& l : p.port.log) s.log.push_back(l);
  s.odswiezen = p.ekran.odswiezen;
  return s;
}
static void wybudzR(PanelR& r, uint32_t epoch, PowodWybudzenia w, uint8_t budzace,
                    std::vector<std::pair<uint32_t, uint8_t>> skrypt) {
  Panel& p = r.p;
  p.zegar.epoch = epoch;
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = w;
  p.start.budzace = budzace;
  p.przyciski.skrypt = std::move(skrypt);
  p.przyciski.kolejka.clear();
  Dyrygent(r.adaptery(), KONF).wybudzenie();
}

struct Nacisk {
  uint32_t t;
  uint8_t bit;
  uint32_t trwa;
};
static std::vector<std::pair<uint32_t, uint8_t>> skryptZ(uint8_t budzacy, const std::vector<Nacisk>& n) {
  std::vector<std::pair<uint32_t, uint8_t>> s = {{0, budzacy}, {300, 0}};
  for (auto& x : n) {
    s.push_back({x.t, x.bit});
    s.push_back({x.t + x.trwa, 0});
  }
  return s;
}
static const int SCEN_D = 16;
static const char* NAZWA_D[6] = {"północ w czuwaniu (23:59:20, naciśnięcia co 15 s)",
                                 "przytrzymania ◀ ▶ ↻ (2,5 s) na kolejnych stronach",
                                 "bateria spada w trakcie wybudzenia (3,90 V → 3,30 V w 30 s)",
                                 "`sp_model` 20 dni temu, infekcja i alkohol, potem naciśnięcia",
                                 "dwa wybudzenia: przytrzymanie, potem zwykłe naciśnięcia",
                                 "losowy"};
struct WynikD {
  std::vector<StanPanelu> stany;
  unsigned long silnik = 0;
};
static WynikD scenariuszD(int nr, bool bezPamieci) {
  szybkosc::bezPamieci = bezPamieci;
  ziarnoC = 4242u + (uint32_t)nr * 131u;
  WynikD w;
  PanelR r;
  unsigned long s0 = szybkosc::przebiegiSilnika;
  const uint8_t P = BIT_PRAWY, L = BIT_LEWY, S = BIT_SRODEK;
  auto po = [&]() { w.stany.push_back(stanPanelu(r.p)); };
  if (nr == 0) {
    przygotujR(r, "2026-11-04", 366);
    std::vector<Nacisk> n;
    for (uint32_t t = 15000; t <= 90000; t += 15000) n.push_back({t, P, 150});
    wybudzR(r, ep(2026, 11, 4, 23, 59, 20), PowodWybudzenia::PRZYCISK, P, skryptZ(P, n));
    po();
  } else if (nr == 1) {
    przygotujR(r, "2026-11-04", 366);
    wybudzR(r, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK, P,
            skryptZ(P, {{3000, S, 2500}, {8000, P, 150}, {11000, P, 2500}, {16000, L, 2500}, {21000, P, 150},
                        {24000, P, 150}, {27000, S, 150}, {30000, P, 2500}}));
    po();
  } else if (nr == 2) {
    przygotujR(r, "2026-11-04", 366);
    r.cz.vStart = 3.90f;
    r.cz.vNaSekunde = 0.02f;
    std::vector<Nacisk> n;
    for (uint32_t t = 3000; t <= 30000; t += 3000) n.push_back({t, t % 9000 ? P : S, 150});
    wybudzR(r, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK, P, skryptZ(P, n));
    po();
  } else if (nr == 3) {
    przygotujR(r, "2026-11-04", 366);
    r.p.nvs.teksty["cpap/sp_model"] = "2026-10-15";
    r.p.pliki.montowane = true;
    Magazyn mag(r.p.pliki, bezLogu);
    sprawy::Odpowiedz o;
    o.data = "2026-11-02";
    o.pytanie = sprawy::Pytanie::INFEKCJA;
    o.odp = 't';
    SPRAWDZ(mag.zapisz("odpowiedzi", "/dane/odpowiedzi.csv", {sprawy::wierszOdpowiedzi(o)}), "odpowiedź");
    WpisCzynnikow c;
    c.data = "2026-11-03";
    c.czynniki = CZ_ALKOHOL;
    SPRAWDZ(mag.zapisz("czynniki", "/dane/czynniki.csv", {zapiszWpis(c)}), "czynnik");
    r.p.pliki.montowane = false;
    wybudzR(r, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK, P,
            skryptZ(P, {{3000, P, 150}, {6000, P, 150}, {9000, S, 2500}, {14000, P, 150}, {17000, L, 150}}));
    po();
  } else if (nr == 4) {
    przygotujR(r, "2026-11-04", 300);
    wybudzR(r, ep(2026, 11, 4, 8, 0), PowodWybudzenia::PRZYCISK, P, skryptZ(P, {{3000, P, 2500}, {8000, S, 2500}}));
    po();
    wybudzR(r, ep(2026, 11, 4, 8, 20), PowodWybudzenia::PRZYCISK, S,
            skryptZ(S, {{3000, P, 150}, {6000, P, 150}, {9000, L, 150}}));
    po();
  } else {
    std::string ostatnia = dodajDni("2026-11-04", -(int)losU(300));
    przygotujR(r, ostatnia, 20 + (int)losU(390));
    if (losU(2)) r.p.nvs.teksty["cpap/sp_model"] = dodajDni(ostatnia, -(int)losU(150));
    r.cz.vStart = losF(3.4f, 4.1f);
    r.cz.vNaSekunde = losU(2) ? 0.0f : losF(0.0f, 0.03f);
    const uint8_t B[3] = {P, L, S};
    for (int wyb = 0; wyb < 1 + (int)losU(2); wyb++) {
      std::vector<Nacisk> n;
      uint32_t t = 1500;
      int ile = 2 + (int)losU(9);
      for (int i = 0; i < ile; i++) {
        t += 800 + losU(20000);
        n.push_back({t, B[losU(3)], losU(4) == 0 ? 2500u : 150u});
        t += n.back().trwa;
      }
      uint8_t bud = B[losU(3)];
      int godz = (int)losU(24);
      wybudzR(r, ep(2026, 11, 4, godz, (int)losU(60)) + (uint32_t)wyb * 600u, PowodWybudzenia::PRZYCISK, bud,
              skryptZ(bud, n));
      po();
    }
  }
  w.silnik = szybkosc::przebiegiSilnika - s0;
  szybkosc::bezPamieci = false;
  return w;
}
// wymaganie: T-WYM-99
static void testD_wybudzenia() {
  for (int nr = 0; nr < SCEN_D; nr++) {
    WynikD a = scenariuszD(nr, true);
    WynikD b = scenariuszD(nr, false);
    bool rowne = a.stany.size() == b.stany.size();
    std::string co;
    for (size_t i = 0; rowne && i < a.stany.size(); i++) {
      if (!(a.stany[i] == b.stany[i])) {
        rowne = false;
        const StanPanelu &x = a.stany[i], &y = b.stany[i];
        co = std::string(x.klatki != y.klatki ? " klatki" : "") + (x.nvs != y.nvs ? " NVS" : "") +
             (x.pliki != y.pliki ? " pliki" : "") + (x.log != y.log ? " log" : "") +
             (x.odswiezen != y.odswiezen ? " odświeżenia" : "");
        for (size_t j = 0; j < std::min(x.log.size(), y.log.size()); j++)
          if (x.log[j] != y.log[j]) {
            printf("    pierwsza różnica logu (wiersz %zu):\n      bez: %s\n      z:   %s\n", j, x.log[j].c_str(),
                   y.log[j].c_str());
            break;
          }
      }
    }
    int odsw = b.stany.empty() ? 0 : b.stany.back().odswiezen;
    printf("  D%-2d %-58s %s; silnik bez pamięci %lu, z pamięcią %lu; odświeżeń %d\n", nr, NAZWA_D[std::min(nr, 5)],
           rowne ? "identyczne" : ("RÓŻNE:" + co).c_str(), a.silnik, b.silnik, odsw);
    SPRAWDZ(rowne, "wybudzenie z pamięcią podręczną różne od liczenia od nowa");
    SPRAWDZ(odsw >= 1, "scenariusz bez rysowania");
  }
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  reguly::zarejestrujRodzajeE();
  wzorce::zarejestrujRodzajeL();
  odkrycia::zarejestrujRodzajeO();
  testA_skrotNocy();
  testB_stany();
  testC_ocenZPamieciaOceny();
  testD_wybudzenia();
  printf("testy_szybciej_przycisk_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
