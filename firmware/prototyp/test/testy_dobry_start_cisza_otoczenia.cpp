// SPDX-License-Identifier: GPL-3.0-or-later
// Program „Dobry start” włączony a cisza źródła otoczenia (R-AW-41, linia E-08): porady i wpisy P te same co przy
// działającym źródle, linia E-08 nie ginie w bloku porady i staje w tym samym wybudzeniu co bez programu. Harness
// z test/testy_co2_cisza.cpp (doba 4.11.2026 z harnessu slotu dziennego).
#define CO2_CISZA_BEZ_MAIN
#include "testy_co2_cisza.cpp"
#include <set>

namespace ds_cisza {
static int wpisyPliku(const std::map<std::string, std::string>& pl) {
  auto i = pl.find("/dane/dobry_start.csv");
  if (i == pl.end()) return 0;
  int n = 0;
  size_t p = 0;
  while ((p = i->second.find("\nP;", p)) != std::string::npos) { n++; p++; }
  return n;
}
struct Wynik {
  std::vector<std::string> porady;  // ID porad z logu "porada dnia: S-K-48 X"
  std::vector<int> minutyPorady;
  std::vector<int> minutyE08;       // log "E-08: źródło otoczenia nie odpowiada"
  std::vector<int> minutyLiniiCO2;  // dowolne zdarzenie z "Brak odczytu CO2"
  int pierwszaPoradaZLinia = -1;
  std::map<std::string, std::string> pliki;
  std::vector<Wyb> v;
};
static Wynik dzien(bool cisza, const char* konfTxt) {
  if (cisza) ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, ep(2026, 11, 4, 6, 0));
  else ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, 0xFFFFFFF0u);
  PanelSym s;
  s.p.pliki.pliki["/konfiguracja.txt"] = konfTxt;
  Wynik w;
  w.v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 23, 30), odpCisza, false, konfOto());
  for (auto& x : w.v) {
    bool porada = false, e08 = false;
    for (auto& z : x.zdarzenia) {
      size_t i = z.find("porada dnia: S-K-48 ");
      if (i != std::string::npos) {
        porada = true;
        w.porady.push_back(z.substr(i + 20, z.find(' ', i + 20) - i - 20));
        w.minutyPorady.push_back(x.minuta);
      }
      if (z.find("E-08: źródło otoczenia nie odpowiada") != std::string::npos) {
        e08 = true;
        w.minutyE08.push_back(x.minuta);
      }
      if (z.find("Brak odczytu CO2") != std::string::npos) w.minutyLiniiCO2.push_back(x.minuta);
    }
    if (porada && e08 && w.pierwszaPoradaZLinia < 0) w.pierwszaPoradaZLinia = x.minuta;
  }
  w.pliki = s.p.pliki.pliki;
  return w;
}
static void druk(const char* n, const Wynik& w) {
  printf("  %s: porad S-K-48 %zu (", n, w.porady.size());
  for (size_t i = 0; i < w.porady.size(); i++)
    printf("%s%s@%02d:%02d", i ? ", " : "", w.porady[i].c_str(), w.minutyPorady[i] / 60, w.minutyPorady[i] % 60);
  printf("), E-08 cisza w %zu wybudzeniach", w.minutyE08.size());
  if (!w.minutyE08.empty()) printf(" od %02d:%02d", w.minutyE08[0] / 60, w.minutyE08[0] % 60);
  printf(", zdarzeń z tekstem linii %zu, pierwsza porada z linią E-08: %d, wpisy P %d\n", w.minutyLiniiCO2.size(),
         w.pierwszaPoradaZLinia, wpisyPliku(w.pliki));
}
}  // namespace ds_cisza

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  const char* PROG = "start.program=tak\n";
  ds_cisza::Wynik wOk = ds_cisza::dzien(false, PROG);
  ds_cisza::Wynik ci = ds_cisza::dzien(true, PROG);
  ds_cisza::Wynik bez = ds_cisza::dzien(true, "");
  ds_cisza::druk("program + źródło OK", wOk);
  ds_cisza::druk("program + cisza od 06:00", ci);
  ds_cisza::druk("bez programu + cisza od 06:00", bez);
  // 1. Program działa także przy ciszy źródła: te same porady i te same wpisy P co przy działającym źródle
  SPRAWDZ(!wOk.porady.empty());
  SPRAWDZ(!ci.porady.empty());
  SPRAWDZ(ds_cisza::wpisyPliku(wOk.pliki) == ds_cisza::wpisyPliku(ci.pliki));
  std::set<std::string> a(wOk.porady.begin(), wOk.porady.end()), b(ci.porady.begin(), ci.porady.end());
  SPRAWDZ(a == b);
  // 2. Linia E-08 z ciszy pojawia się przy włączonym programie (nie ginie w bloku porady) i nie przed 3 h ciszy
  SPRAWDZ(!ci.minutyE08.empty());
  SPRAWDZ(wOk.minutyE08.empty());
  SPRAWDZ(!bez.minutyE08.empty());
  if (!ci.minutyE08.empty()) SPRAWDZ(ci.minutyE08.front() >= 9 * 60);
  // 3. Linia z ciszy pojawia się w tym samym pierwszym wybudzeniu z programem i bez programu (program nie przesuwa ani
  // nie „zjada” epizodu: raz na epizod liczy się od pokazania)
  if (!ci.minutyE08.empty() && !bez.minutyE08.empty()) SPRAWDZ(ci.minutyE08.front() == bez.minutyE08.front());
  printf("testy_dobry_start_cisza_otoczenia: %d OK, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
