// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: ustawienie
// „Profil publiczny” w `/konfiguracja.txt` (`profil=publiczny|prywatny`, 5.6.4), teksty z prawej kolumny 5.4.14 na
// ekranach dnia (R-EKR-43: AHI i PM2.5 w obu profilach) i ekran
// „Co znaczą te liczby” z S8 (R-EKR-51), także fail-path „profil wyłączony w trakcie oglądania”; (BOM, spacje).
// Dane syntetyczne. Uruchomienie: tools/prototyp/testy.sh (program testy_profil_publiczny).
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "dzien.h"
#include "ekrany.h"
#include "podsumowanie.h"
#include "raport.h"
#include "nawigacja.h"
#include "noc.h"
#include "slot_dzienny.h"
#include "sprawy.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE(a, b)                                                                                              \
  do {                                                                                                           \
    std::string a_ = (a), b_ = (b);                                                                              \
    if (a_ == b_) ok++;                                                                                          \
    else { zle++; printf("BŁĄD %s:%d: %s = „%s”, ma być „%s”\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } \
  } while (0)

static bool ma(const std::string& s, const char* x) { return s.find(x) != std::string::npos; }
static Noc noc(const std::string& d, double ahi, int m01 = 420) {
  Noc n;
  n.data = d;
  n.wazna = m01 >= 240;
  n.m01 = m01;
  n.ahi = (float)ahi;
  n.wp95 = 8;
  n.wmed = 4;
  n.ustawienia = "ASVAuto/6/10/3/15";
  return n;
}

// ---------- 5.6.4: klucz `profil` w /konfiguracja.txt ----------
static void testKonfiguracja() {
  SPRAWDZ(!UstawieniaDnia().profilPubliczny);  // domyślnie prywatny
  SPRAWDZ(!parsujUstawieniaDnia("").profilPubliczny);
  SPRAWDZ(parsujUstawieniaDnia("profil=publiczny\n").profilPubliczny);
  SPRAWDZ(parsujUstawieniaDnia("profil = publiczny\r\n").profilPubliczny);
  UstawieniaDnia u = parsujUstawieniaDnia("profil=prywatny\n");
  SPRAWDZ(!u.profilPubliczny && u.bledy.empty());
  u = parsujUstawieniaDnia("profil=tak\nwymiana.maska=2026-10-01\n");  // zła wartość: prywatny, wiersz w logu
  SPRAWDZ(!u.profilPubliczny && u.bledy.size() == 1 && u.sprzet[EL_MASKA].wymieniono == "2026-10-01");
}

// ---------- R-EKR-43: AHI i PM2.5 w obu profilach (5.4.14) ----------
static void testAhiWObuProfilach() {
  Norma n;
  n.ahi.p10 = 1.5;
  n.ahi.p90 = 6.2;
  n.ahi.med = 3;
  n.ahi.n = 30;
  n.m01.p10 = 380;
  n.m01.p90 = 460;
  n.m01.n = 30;
  podsumowanie::WejscieS8 w;
  w.norma = &n;
  w.tryb = reguly::Tryb::ASVAUTO;
  podsumowanie::DaneS8 a = podsumowanie::budujS8(w);
  w.profilPubliczny = true;
  podsumowanie::DaneS8 b = podsumowanie::budujS8(w);
  SPRAWDZ(!a.wiersze.empty() && a.wiersze == b.wiersze && a.krok == b.krok);  // S8: wiersz i krok z `AHI` w obu
  SPRAWDZ(!b.wiersze.empty() && b.wiersze[0].rfind("AHI ", 0) == 0);
  ROWNE(b.krok, "→ Pasmo AHI wysokie. Obserwuj kolejne noce.");
  // od ADR-0041 S8 nie jest stroną kokpitu: profil nie różni S8 (stopka standardowa ekranu dnia w obu)
  SPRAWDZ(podsumowanie::trescS8(a).stopkaStandardowa && podsumowanie::trescS8(b).stopkaStandardowa);
  // S7: ten sam tekst w obu profilach (`AHI, mediana`, zdanie z `AHI`)
  std::vector<Noc> h;
  for (int i = 0; i < 61; i++) h.push_back(noc(dodajDni("2026-10-01", i), i < 31 ? 1.0 : 3.5));
  podsumowanie::DaneS7 s0 = podsumowanie::budujS7("2026-12-01", h, reguly::Tryb::ASVAUTO, false);
  podsumowanie::DaneS7 s1 = podsumowanie::budujS7("2026-12-01", h, reguly::Tryb::ASVAUTO, true);
  ROWNE(s1.podpisAhi, "AHI, mediana");
  ROWNE(s1.zdanie, s0.zdanie);
  SPRAWDZ(!ma(s1.zdanie, "Bezdech"));
  // „Co znaczą te liczby” dalej objaśnia AHI (5.4.14): bezdechy i spłycenia na godzinę
  podsumowanie::DaneLiczb l = podsumowanie::budujLiczby(reguly::Tryb::AUTOSET, false);
  SPRAWDZ(!l.wiersze.empty() && l.wiersze[0].nazwa == "Bezdechy na godzinę (AHI):" &&
          ma(l.wiersze[0].opis, "spłycenia"));
}

// ---------- BOM i spacje na końcu dla każdego klucza /konfiguracja.txt ----------
static void testBomISpacje() {
  SPRAWDZ(parsujUstawieniaDnia("profil=publiczny \n").profilPubliczny);
  SPRAWDZ(parsujUstawieniaDnia("profil=publiczny\t\r\n").profilPubliczny);
  SPRAWDZ(parsujUstawieniaDnia("\xEF\xBB\xBFprofil=publiczny\n").profilPubliczny);
  UstawieniaDnia u = parsujUstawieniaDnia("\xEF\xBB\xBFwymiana.maska=2026-10-01 \nco2=nie  \n");
  SPRAWDZ(u.sprzet[EL_MASKA].wymieniono == "2026-10-01" && u.co2 == 0 && u.bledy.empty());
  // `raport=` i `raport.czynniki=` czyta raport.cpp: nie jest „pominiętym wierszem” w logu
  SPRAWDZ(parsujUstawieniaDnia("raport=sciaga\nraport.czynniki=K\nraport=pdf\n").bledy.empty());
  SPRAWDZ(parsujUstawieniaDnia("raportx=1\nraport\n").bledy.size() == 2);
  SPRAWDZ(raport::wariantZKonfiguracji("\xEF\xBB\xBFraport=sciaga\t\n") == raport::Wariant::SCIAGA);
  bool ust = false;
  raport::czynnikiZKonfiguracji("\xEF\xBB\xBFraport.czynniki=K \n", ust);
  SPRAWDZ(ust);
}

// ---------- R-EKR-51: S8 → „Co znaczą te liczby” przez przełącznik; fail-path w trakcie oglądania ----------
static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static uint32_t ep(int r, int mi, int d, int h, int m) {
  struct tm t = {};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  return (uint32_t)timegm(&t);
}
static void przycisk(Panel& p, uint32_t kiedy, uint8_t bit, std::vector<std::pair<uint32_t, uint8_t>> skrypt) {
  p.zegar.epoch = kiedy;
  p.zegar.msTeraz = 0;
  p.d.z.clear();
  p.sen.snow = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::PRZYCISK;
  p.start.budzace = bit;
  p.przyciski.skrypt = skrypt;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}
static Ekran ekran(const Panel& p) { return (Ekran)p.start.stan.ekran; }

static void testLiczby() {
  for (bool pub : {false, true}) {
    Panel p;
    p.zegar.epoch = ep(2026, 11, 4, 10, 0);
    zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
    p.pliki.pliki["/konfiguracja.txt"] = pub ? "profil=publiczny\n" : "";
    // ▶ z ekranu dnia po kolei przez strony kokpitu aż do K2 (R-KOK-19), potem ▶: „Co znaczą te liczby” tylko
    // w profilu publicznym (R-KOK-22), inaczej „Otwarte sprawy” (przy sprawie) albo U-01
    for (int i = 0; i < 4 && ekran(p) != Ekran::K2; i++)
      przycisk(p, ep(2026, 11, 4, 10, 0) + (uint32_t)i, BIT_PRAWY, {{0, BIT_PRAWY}, {300, 0}});
    SPRAWDZ(ekran(p) == Ekran::K2);
    przycisk(p, ep(2026, 11, 4, 10, 0) + 20, BIT_PRAWY, {{0, BIT_PRAWY}, {300, 0}});
    if (!pub) {
      SPRAWDZ(ekran(p) == Ekran::USTAWIENIA || ekran(p) == Ekran::SPRAWY);
      SPRAWDZ(ekran(p) != Ekran::NORMA && ekran(p) != Ekran::SPRZET);  // S8 i „Sprzęt” poza kokpitem (ADR-0041)
      continue;
    }
    SPRAWDZ(ekran(p) == Ekran::LICZBY);
    // ◀ wraca do K2, ▶ znowu do „Co znaczą te liczby”
    przycisk(p, ep(2026, 11, 4, 10, 1), BIT_LEWY, {{0, BIT_LEWY}, {300, 0}});
    SPRAWDZ(ekran(p) == Ekran::K2);
    przycisk(p, ep(2026, 11, 4, 10, 2), BIT_PRAWY, {{0, BIT_PRAWY}, {300, 0}});
    SPRAWDZ(ekran(p) == Ekran::LICZBY);
    // fail-path: profil wyłączony w trakcie oglądania; ekran zostaje do naciśnięcia, ◀ → K2, następne ▶ na K2 omija
    // „Co znaczą te liczby”
    p.pliki.pliki["/konfiguracja.txt"] = "profil=prywatny\n";
    przycisk(p, ep(2026, 11, 4, 10, 3), BIT_LEWY, {{0, BIT_LEWY}, {300, 0}});
    SPRAWDZ(ekran(p) == Ekran::K2);
    przycisk(p, ep(2026, 11, 4, 10, 4), BIT_PRAWY, {{0, BIT_PRAWY}, {300, 0}});
    SPRAWDZ(ekran(p) == Ekran::USTAWIENIA || ekran(p) == Ekran::SPRAWY);
  }
  // nawigacja bez dyrygenta: ten sam przełącznik (5.5.7)
  KontekstNaw k;
  SPRAWDZ(nacisniecie(Ekran::K2, Przycisk::PRAWY, false, k).ekran == Ekran::USTAWIENIA);  // bez spraw i profilu
  k.profilPubliczny = true;
  SPRAWDZ(nacisniecie(Ekran::K2, Przycisk::PRAWY, false, k).ekran == Ekran::LICZBY);
  k.profilPubliczny = false;  // wyłączony w trakcie: z „Co znaczą te liczby” ◀ nadal wraca do K2
  SPRAWDZ(nacisniecie(Ekran::LICZBY, Przycisk::LEWY, false, k).ekran == Ekran::K2);
  SPRAWDZ(nacisniecie(Ekran::LICZBY, Przycisk::PRAWY, false, k).ekran == Ekran::USTAWIENIA);
}

int main() {
  testKonfiguracja();
  testAhiWObuProfilach();
  testBomISpacje();
  testLiczby();
  printf("testy_profil_publiczny: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
