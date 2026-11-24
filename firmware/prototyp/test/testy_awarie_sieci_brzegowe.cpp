// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe awarii sieci i drugiej sesji nocy (24.11.2026) na atrapach z test/testy_awarie_sieci.cpp
// (włączony bez jego main()).
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_awarie_sieci_brzegowe). LOG_AWARII=1 drukuje przebiegi.
#define main main_testow_awarii
#include "testy_awarie_sieci.cpp"
#undef main

static bool logZawiera(const std::vector<std::string>& log, const std::string& t) { return wLogu(log, t); }

// Sesja zaczęta po imporcie przed 11:00 i trwająca po 11:00 (10:30–12:30). R-DANE-69 jej nie bierze (okno do 11:00;
// o 11:00 `rop` = 1, R-DANE-24 widzi noc bez zmian), więc drzemkaB9() nie może odrzucać każdej sesji zaczętej przed
// 11:00. Przed zmianą 24.11.2026 ta sesja była drzemką R-KON-57 (`drzemka {czas}` w stopce). Oczekiwane: sesja trafia
// gdzieś (noc pobrana ponownie albo drzemka), nigdy nigdzie.
// wymaganie: R-KON-57, R-DANE-69
static void testR_KON_57_sesjaPrzed11DoPo11() {
  PanelD s;
  DrugaSesja d(10 * 60 + 30, 120);  // 10:30–12:30
  uint32_t t = ep(2026, 11, 24, 5, 0);
  PowodWybudzenia pw = PowodWybudzenia::START;
  bool drzemka = false, drzemkaPoKoncu = false;
  std::string m01;
  for (int i = 0; i < 300 && t < ep(2026, 11, 24, 16, 0); i++) {
    WybD w = wybudzD(s, d, t, pw);
    if (logZawiera(s.p.port.log, "drzemka")) {
      drzemka = true;
      if (t >= d.koniec2) drzemkaPoKoncu = true;
    }
    m01 = w.m01;
    if (!w.spal) break;
    uint32_t snu = s.p.sen.plan.budzZegarem ? s.p.sen.plan.sekund : OKRES_SNU_S;
    t += s.p.zegar.msTeraz / 1000 + (snu ? snu : 1);
    pw = PowodWybudzenia::ZEGAR;
  }
  printf("  sesja 10:30–12:30 po imporcie 06:00: M-01 %s, drzemka w logu %d (po końcu sesji %d)\n", m01.c_str(),
         drzemka, drzemkaPoKoncu);
  SPRAWDZ(m01 == "525" || drzemkaPoKoncu);
}

// R-AW-35, potem brak odpowiedzi. 401 o 06:00 (S5 z R-AW-35), o 06:30 AirBridge milczy:
// „ostatnia próba” to brak odpowiedzi, więc przed 11:00 S10 (R-EKR-57, R-AW-32), bez tekstów R-AW-35.
// wymaganie: R-AW-35, R-EKR-57
static void testR_AW_35_potemCisza() {
  PanelL s;
  s.odpowiedz401();
  wybudzL(s, ep(2026, 11, 24, 6, 0), PowodWybudzenia::START);
  SPRAWDZ(wLogu(s.p.port.log, "ekran dnia S5"));
  s.p.siec.odpowiedzi.clear();
  wybudzL(s, ep(2026, 11, 24, 6, 30), PowodWybudzenia::ZEGAR);
  SPRAWDZ(!wLogu(s.p.port.log, "ekran dnia S5"));
  // po 401 i ciszy: po 11:00 teksty R-AW-32 (nie R-AW-35)
  s.p.siec.odpowiedzi.clear();
  wybudzL(s, ep(2026, 11, 24, 11, 0), PowodWybudzenia::ZEGAR);
  SPRAWDZ(!wLogu(s.p.port.log, "odrzucił logowanie"));
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testR_KON_57_sesjaPrzed11DoPo11();
  testR_AW_35_potemCisza();
  printf("testy_awarie_sieci_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
