// SPDX-License-Identifier: GPL-3.0-or-later
// Program „Dobry start” a trwale nieczytelny plik oceny (R-AW-40: ocena niepełna, „bez kart pochwalnych”; R-DS-60:
// weto pochwał `ocena_niepelna`, ADR-0029 pkt 7). Doba 4.11.2026 z harnessu slotu dziennego (test/testy_co2_cisza.cpp):
// do 07:30 bez usterki (pliki powstają), potem zanik zasilania i usterka odczytu plików oceny; ocena niepełna od 3.
// wybudzenia z usterką. Kontrola (bez usterki): pochwała DS-P* staje i wraca w bloku; od oceny niepełnej żadnej pochwały.
#define CO2_CISZA_BEZ_MAIN
#include "testy_co2_cisza.cpp"
#include <set>

static std::vector<std::string> poradyZ(const std::vector<Wyb>& v, int* niepelna) {
  std::vector<std::string> r;
  for (auto& x : v)
    for (auto& z : x.zdarzenia) {
      size_t i = z.find("porada dnia: S-K-48 ");
      if (i != std::string::npos)
        r.push_back(z.substr(i + 20, z.find(' ', i + 20) - i - 20) + "@" + std::to_string(x.minuta / 60) + ":" +
                    std::to_string(x.minuta % 60));
      if (z.find("ocena: niepełna, nieczytelny plik oceny") != std::string::npos) {
        (*niepelna)++;
        r.push_back("NIEPELNA@" + std::to_string(x.minuta / 60) + ":" + std::to_string(x.minuta % 60));
      }
    }
  return r;
}

static std::vector<std::string> dzien(const std::set<std::string>& zepsute, int* niepelna) {
  ustawZrodlo(Awaria::BRAK_ODPOWIEDZI, 0xFFFFFFF0u);
  PanelSym s;
  s.p.pliki.pliki["/konfiguracja.txt"] = "start.program=tak\n";
  std::vector<Wyb> a = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 7, 30), odpCisza, false, konf(true));
  PanelSym r;
  r.p.pliki.pliki = s.p.pliki.pliki;
  r.p.nvs.teksty = s.p.nvs.teksty;
  for (auto& z : zepsute) r.p.pliki.nieczytelne.insert(z);
  std::vector<Wyb> b = symuluj(r, ep(2026, 11, 4, 7, 35), ep(2026, 11, 4, 12, 0), odpCisza, false, konf(true));
  int n0 = 0;
  std::vector<std::string> pa = poradyZ(a, &n0);
  std::vector<std::string> pb = poradyZ(b, niepelna);
  pa.insert(pa.end(), pb.begin(), pb.end());
  return pa;
}

// pochwały narysowane (`po` = tylko od pierwszego wybudzenia z oceną niepełną; wcześniej ocena była pełna)
static int pochwal(const std::vector<std::string>& v, bool po = false) {
  int n = 0;
  bool niepelna = false;
  for (auto& x : v) {
    niepelna = niepelna || x.compare(0, 8, "NIEPELNA") == 0;
    if (x.compare(0, 4, "DS-P") == 0 && (!po || niepelna)) n++;
  }
  return n;
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  int nk = 0, nz = 0;
  std::vector<std::string> kontrola = dzien({}, &nk);
  std::vector<std::string> usterka = dzien({"/dane/czynniki.csv", "/dane/odpowiedzi.csv", "/dane/reguly.csv"}, &nz);
  auto druk = [](const char* n, const std::vector<std::string>& v, int k) {
    printf("  %s: logów „ocena niepełna” %d, porady:", n, k);
    for (auto& x : v) printf(" %s", x.c_str());
    printf("\n");
  };
  druk("kontrola", kontrola, nk);
  druk("nieczytelne pliki oceny", usterka, nz);
  SPRAWDZ(nk == 0 && nz > 0);              // scenariusz daje ocenę niepełną tylko przy usterce
  SPRAWDZ(pochwal(kontrola) > 0);          // bez usterki pochwała staje
  SPRAWDZ(pochwal(kontrola) >= 2);         // pochwała dnia wraca w kolejnych wybudzeniach bloku
  SPRAWDZ(pochwal(usterka, true) == 0);    // R-AW-40, R-DS-60: od oceny niepełnej żadnej pochwały (ani powrotu)
  printf("testy_dobry_start_ocena_niepelna: %d OK, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
