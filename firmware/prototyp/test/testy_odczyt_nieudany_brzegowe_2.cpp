// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe odczytu nieudanego, część 2 (22.11.2026: magazyn odmawia przepisania pliku, który
// jest na nośniku, a nie był w tym wybudzeniu przeczytany).
//
//  S1. Bez żadnej usterki nośnika, historia z dwóch lat (60 nocy do 28.12.2025), symulacja 29.12.2025–02.01.2026
//      (przełom roku: pierwszy zapis pliku 2026, naciśnięcia rano, w południe i wieczorem) na obu ścieżkach (głęboki sen
//      i czekanie na kablu, ADR-0047). Oczekiwane: ani jednej odmowy i ani jednego `odczyt nieudany` w logu,
//      ścieżki zgodne. Program wypisuje skrót każdego pliku (`PLIK A|B {ścieżka} {rozmiar} {skrót}`), żeby ten sam plik
//      testu zbudowany na `origin/main` dał porównanie bajt w bajt (przed 22.11.2026 magazyn nie miał tej odmowy).
//  S2. Ta sama doba z wybudzeniami bez pobrania (tylko przyciski, siec niedostępna): bez odmów.
//  S3 (CZERWONY do poprawki): kopia `/dane/dobry_start.poprzedni.csv` przy drugim „od nowa” odrzucona.
//
// Harness z test/testy_kabel_bez_snu_brzegowe.cpp (Bieg, symuluj, AirBridge z raportami), bez jego main().
#pragma GCC diagnostic ignored "-Wunused-function"
#define main main_testy_kabel_bez_snu_brzegowe
#include "testy_kabel_bez_snu_brzegowe.cpp"
#undef main

static uint64_t fnv(const std::string& t) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : t) {
    h ^= c;
    h *= 1099511628211ull;
  }
  return h;
}
static int ileWLogu(const Bieg& b, const std::string& t) {
  int n = 0;
  for (auto& l : b.p.port.log) n += l.find(t) != std::string::npos;
  return n;
}
static void wypiszPliki(const char* kto, const Bieg& b) {
  for (auto& kv : b.p.pliki.pliki)
    printf("PLIK %s %s %zu %016llx\n", kto, kv.first.c_str(), kv.second.size(), (unsigned long long)fnv(kv.second));
}

static void testS1_DwaLataPrzelomRoku() {
  Bieg A(false, "2025-12-28", 60, 21), B(true, "2025-12-28", 60, 21);
  Opcje o;
  for (int d = 29; d <= 33; d++) {  // 29.12 … 02.01 (mktime normalizuje dzień 32 i 33)
    o.stale.push_back({ep(2025, 12, d, 9, 10), BIT_PRAWY});
    o.stale.push_back({ep(2025, 12, d, 13, 40), BIT_PRAWY});
    o.stale.push_back({ep(2025, 12, d, 21, 5), BIT_SRODEK});
  }
  WynikSym ws = symuluj(A, B, ep(2025, 12, 29, 4, 50), ep(2026, 1, 2, 23, 0), o, "S1");
  int odmA = ileWLogu(A, "nie był w tym wybudzeniu przeczytany"), odmB = ileWLogu(B, "nie był w tym wybudzeniu przeczytany");
  int ncA = ileWLogu(A, ": odczyt nieudany"), ncB = ileWLogu(B, ": odczyt nieudany");
  bool jest2026 = A.p.pliki.pliki.count("/dane/noce/2026.csv") == 1;
  printf("S1 dwa lata, przełom roku, bez usterek: przebiegów %d, niezgodnych %d, odmów A %d B %d, "
         "„odczyt nieudany” A %d B %d, plik 2026 %d\n",
         ws.przebiegow, ws.niezgodnych, odmA, odmB, ncA, ncB, (int)jest2026);
  SPRAWDZ(ws.niezgodnych == 0);
  SPRAWDZ(odmA == 0 && odmB == 0);
  SPRAWDZ(ncA == 0 && ncB == 0);
  SPRAWDZ(jest2026);
  wypiszPliki("A", A);
  wypiszPliki("B", B);
}

static void testS2_BezSieci() {
  Bieg A(false, "2026-11-08", 60, 5), B(true, "2026-11-08", 60, 5);
  A.p.siec.siecJest = B.p.siec.siecJest = false;
  Opcje o;
  for (int h : {8, 10, 12, 15, 19, 22}) o.stale.push_back({ep(2026, 11, 9, h, 7), BIT_PRAWY});
  o.stale.push_back({ep(2026, 11, 9, 20, 30), BIT_LEWY});
  WynikSym ws = symuluj(A, B, ep(2026, 11, 9, 4, 50), ep(2026, 11, 9, 23, 30), o, "S2");
  int odm = ileWLogu(A, "nie był w tym wybudzeniu przeczytany") + ileWLogu(B, "nie był w tym wybudzeniu przeczytany");
  printf("S2 doba bez sieci, same przyciski: przebiegów %d, niezgodnych %d, odmów %d\n", ws.przebiegow,
         ws.niezgodnych, odm);
  SPRAWDZ(ws.niezgodnych == 0);
  SPRAWDZ(odm == 0);
}

// S3 (CZERWONY): `Przebieg::odNowaB49()` (R-DS-33) zapisuje kopię `/dane/dobry_start.poprzedni.csv` przez
// Magazyn::zapisz() bez odczytu tej ścieżki (nikt jej nie czyta). Pierwsze „od nowa” tworzy kopię; drugie (inne
// wybudzenie) trafia na plik, który jest, a nie był przeczytany: magazyn odmawia, w kopii zostaje stan sprzed PIERWSZEGO
// „od nowa”, a stan programu sprzed drugiego przepada (w logu tylko `kopia … nieudana`). Ten sam ciąg wywołań co
// w dyrygencie: odczyt `/dane/dobry_start.csv` (wczytajB49), zapis kopii, zapis nowego stanu.
static void testS3_KopiaOdNowa() {
  Panel p;
  p.pliki.montowane = true;
  const std::string sc = "/dane/dobry_start.csv", kopia = "/dane/dobry_start.poprzedni.csv";
  auto odNowa = [&](const std::vector<std::string>& nowyStan) {
    Magazyn m(p.pliki, bezLogu);
    OdczytPliku o = m.czytaj("dobry_start", sc);  // wczytajB49()
    bool k = m.zapisz("dobry_start", kopia, o.wiersze);  // odNowaB49(): kopia poprzedniego stanu
    m.zapisz("dobry_start", sc, nowyStan);
    return k;
  };
  {
    Magazyn m(p.pliki, bezLogu);
    m.zapisz("dobry_start", sc, {"start;2026-10-01;program1"});
  }
  bool k1 = odNowa({"start;2026-11-01;program2"});
  bool k2 = odNowa({"start;2026-11-20;program3"});
  Magazyn m(p.pliki, bezLogu);
  std::vector<std::string> w = m.czytaj("dobry_start", kopia).wiersze;
  std::string jest = w.empty() ? "(pusto)" : w[0];
  printf("S3 dwa razy „od nowa”: kopia 1 %s, kopia 2 %s, w kopii: %s (oczekiwane program2)\n",
         k1 ? "zapisana" : "ODRZUCONA", k2 ? "zapisana" : "ODRZUCONA", jest.c_str());
  SPRAWDZ(k1);
  SPRAWDZ(k2);  // CZERWONY: odmowa
  SPRAWDZ(jest.rfind("start;2026-11-01;program2", 0) == 0);  // CZERWONY: w kopii program1 (parser dokłada puste pola)
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  printf("przypadki brzegowe, część 2: legalne zapisy bez odmów\n");
  testS1_DwaLataPrzelomRoku();
  testS2_BezSieci();
  testS3_KopiaOdNowa();
  printf("testy_odczyt_nieudany_brzegowe_2: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
