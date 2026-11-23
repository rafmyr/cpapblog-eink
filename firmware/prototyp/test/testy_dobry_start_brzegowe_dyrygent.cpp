// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe programu „Dobry start” z dyrygentem na atrapach (harness doby z
// test/testy_slot_dzienny_dyrygent.cpp, jak w test/testy_dobry_start_brzegowe.cpp): plik stanu uszkodzony między dobami
// (R-DS-43), wyłączenie i ponowne włączenie (R-DS-32). Testy CZERWONE (przypadki dawnych usterek) blokują, dopóki
// usterka nie jest poprawiona.
#define main main_testy_slot_dzienny_dyrygent
#include "testy_slot_dzienny_dyrygent.cpp"
#undef main

namespace {

int czerwone = 0;
#define CZERWONY(rd, w)                                                                   \
  do {                                                                                    \
    if (w) ok++;                                                                          \
    else { czerwone++; printf("CZERWONY %s (%s:%d): %s\n", rd, __FILE__, __LINE__, #w); } \
  } while (0)

const std::string PLIK = "/dane/dobry_start.csv";

std::string wiersz(const std::string& plik, const std::string& pocz) {
  size_t p = plik.find("\n" + pocz);
  if (p == std::string::npos) return "";
  size_t k = plik.find('\n', p + 1);
  std::string w = plik.substr(p + 1, k - p - 1);
  while (!w.empty() && w.back() == ';') w.pop_back();  // magazyn dopełnia wiersz do 7 pól
  return w;
}
int ileP(const std::string& plik) {
  int n = 0;
  for (size_t p = 0; (p = plik.find("\nP;", p)) != std::string::npos; p++) n++;
  return n;
}
bool zdarzenie(const std::vector<Wyb>& v, const std::string& f) {
  for (auto& w : v)
    for (auto& z : w.zdarzenia)
      if (z.find(f) != std::string::npos) return true;
  return false;
}
// jedna doba panelu od 00:05 do 23:00 na plikach i NVS z poprzedniej
std::vector<Wyb> doba(PanelSym& s, int dzien) {
  return symuluj(s, ep(2026, 11, dzien, 0, 5), ep(2026, 11, dzien, 23, 0), odpowiedziB9);
}

// (R-DS-43): plik stanu uszkodzony (np. bez `#koniec` po awarii karty) → magazyn odkłada go jako
// `.uszkodzony.1`, a dyrygent zaczyna program od nowa z dzisiejszym dniem włączenia i kolejką 1 (porady od początku,
// termin 56 dób od nowa), choć spec każe odtworzyć kolejkę z wpisów P.
// stan programu z 25.10 (dziewięć porad planu narysowanych, kolejka 10), z `#koniec` albo bez niego
std::string plikStanu(bool koniec) {
  std::vector<std::string> w = {"S;start;2026-10-25", "S;kolejka;10", "S;stan;aktywny", "S;przerwa_dni;0", "S;d0_z_rzedu;0",
                                "S;tryb;ASV", "S;ponad;0", "S;noce_terapii;9", "S;ankieta_od;2026-10-28",
                                "S;doba;2026-11-02", "S;dzien_ust;1", "S;plan_koniec;0"};
  const dobry_start::Katalog& k = dobry_start::katalogWbudowany();
  for (int m = 1; m <= 9; m++) {
    const dobry_start::Porada* p = dobry_start::wariantDoby(k, dobry_start::Faza::START_RANO, m, dobry_start::Tryb::ASV, -1);
    w.push_back("P;" + dodajDni("2026-10-25", m - 1) + ";" + (p ? p->id : "?") + ";plan;;dzien;panel");
  }
  std::string t = "#cpap-eink dobry_start 2\nrodzaj;a;b;c;d;e;f\n";  // wersja 2 pliku (R-DS-67); wiersze wersji 1 pominięte
  for (auto& x : w) {
    std::string r = x;
    int pol = 1;
    for (char ch : r) pol += ch == ';';
    while (pol++ < 7) r += ";";
    t += r + "\n";
  }
  if (koniec) t += "#koniec " + std::to_string(w.size()) + "\n";
  return t;
}
std::string poDobie(const std::string& stan, std::vector<Wyb>* v = nullptr) {
  PanelSym s;
  s.p.pliki.pliki["/konfiguracja.txt"] = "start.program=tak\n";
  s.p.pliki.pliki[PLIK] = stan;
  std::vector<Wyb> x = doba(s, 4);
  if (v) *v = x;
  return s.p.pliki.pliki[PLIK];
}
void brz_uszkodzonyPlikStanu() {
  std::string dobry = poDobie(plikStanu(true));
  printf("  przypadek 8: plik cały: %s, %s, wpisów P %d\n", wiersz(dobry, "S;start;").c_str(), wiersz(dobry, "S;kolejka;").c_str(),
         ileP(dobry));
  // wersja 2 (R-DS-67): kolejki faz z wpisów P (bez wiersza `S;kolejka`)
  SPRAWDZ(wiersz(dobry, "S;start;") == "S;start;2026-10-25" && wiersz(dobry, "S;kolejka;").empty() && ileP(dobry) == 10);
  std::vector<Wyb> v;
  std::string zly = poDobie(plikStanu(false), &v);
  printf("  przypadek 8: plik bez `#koniec`: %s, %s, wpisów P %d (oczekiwane: start 2026-10-25, wpisów P 10)\n",
         wiersz(zly, "S;start;").c_str(), wiersz(zly, "S;kolejka;").c_str(), ileP(zly));
  SPRAWDZ(zdarzenie(v, "uszkodzony"));
  CZERWONY("przypadek 8", wiersz(zly, "S;start;") == "S;start;2026-10-25" && ileP(zly) == 10);
}
void brz_doba5() {
  PanelSym a;
  a.p.pliki.pliki["/konfiguracja.txt"] = "start.program=tak\n";
  doba(a, 4);
  if (getenv("BRZEG_LOG")) printf("%s\n", a.p.pliki.pliki[PLIK].c_str());
  PanelSym b;
  b.p.pliki.pliki = a.p.pliki.pliki;
  b.p.nvs.teksty = a.p.nvs.teksty;
  std::vector<Wyb> v5 = doba(b, 5);
  if (getenv("BRZEG_LOG")) printf("    doba 5: %zu wybudzeń, ostatnie %02d:%02d, spał %d\n", v5.size(),
                                v5.empty() ? 0 : v5.back().minuta / 60, v5.empty() ? 0 : v5.back().minuta % 60,
                                v5.empty() ? 0 : (int)v5.back().spal);
  if (getenv("BRZEG_LOG"))
    for (auto& w : v5)
      for (auto& z : w.zdarzenia)
        if (z.find("dobry") != std::string::npos || z.find("porada") != std::string::npos ||
            z.find("ekran dnia") != std::string::npos || z.find("slot dzienny") != std::string::npos)
          printf("    %02d:%02d %s\n", w.minuta / 60, w.minuta % 60, z.substr(0, 140).c_str());
  // diagnostyka harnessu: kolejne doby z plikami i NVS poprzedniej (atrapa AirBridge ma tylko noc z 4.11)
  printf("  doba 5 po dobie 4: wpisów P %d (%s)\n", ileP(b.p.pliki.pliki[PLIK]), wiersz(b.p.pliki.pliki[PLIK], "S;doba;").c_str());
}

// R-DS-32: wyłączenie w trakcie i ponowne włączenie: stan zachowany, dalej od tego samego miejsca, bez S-K-48 w dobie
// wyłączenia; formularz pyta „dalej czy od nowa” (pole start_wznow)
void brz_R_DS_32_wylaczIWlacz() {
  PanelSym a;
  a.p.pliki.pliki["/konfiguracja.txt"] = "start.program=nie\n";
  a.p.pliki.pliki[PLIK] = plikStanu(true);
  std::vector<Wyb> v = doba(a, 4);
  SPRAWDZ(!zdarzenie(v, "porada dnia: S-K-48"));
  SPRAWDZ(wiersz(a.p.pliki.pliki[PLIK], "S;stan;") == "S;stan;zatrzymany" && ileP(a.p.pliki.pliki[PLIK]) == 9);
  std::string wyl = a.p.pliki.pliki[PLIK];
  std::string pl = poDobie(wyl, &v);
  // wersja 2: dalej od doby programu (doby bez programu nie liczą się, R-DS-56)
  SPRAWDZ(zdarzenie(v, "porada dnia: S-K-48") && zdarzenie(v, "włączony ponownie, dalej od doby programu"));
  SPRAWDZ(wiersz(pl, "S;start;") == "S;start;2026-10-25" && ileP(pl) >= 10 && wiersz(pl, "S;stan;") == "S;stan;aktywny");
  printf("  R-DS-32: wyłączony bez S-K-48 (zatrzymany), ponownie włączony: %s, wpisów P %d\n", wiersz(pl, "S;przerwa_dni;").c_str(),
         ileP(pl));
}

}  // namespace

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  brz_uszkodzonyPlikStanu();
  brz_doba5();
  brz_R_DS_32_wylaczIWlacz();
  printf("testy_dobry_start_brzegowe_dyrygent: %d ok, %d błędów, czerwone: %d\n", ok, zle, czerwone);
  return (zle || czerwone) ? 1 : 0;  // czerwone = przypadki dawnych usterek: blokują, dopóki nie są poprawione
}
