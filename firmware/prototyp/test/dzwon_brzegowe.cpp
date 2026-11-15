// SPDX-License-Identifier: GPL-3.0-or-later
// Testy brzegowe piku „dzwon” (14.11.2026): własne sprawdzenia piku „dzwon” (R-POR-09…11) w panelu.
// Nie jest częścią testy.sh. Budowa (po testy.sh, z jego obiektami):
//   g++ -std=c++17 -O1 $INCLUDY test/dzwon_brzegowe.cpp .pio/testy/*.o (bez testy*.o) -o /tmp/dzwon_brzegowe
// Tryby:
//   garby PLIK         garb16(h, d) dla h 0…432, d −1300…1300 (do porównania: tools/portret/dzwon_brzegowe.py garby)
//   porownaj KATALOG   noce losowe ze wzorca Pythona: różnice pikseli panel/wzorzec, osobno w pasie pików
//   scenariusze KAT    przypadki brzegowe z liczbami i obrazkami PBM do obejrzenia
// Dane wyłącznie syntetyczne.
#include <dirent.h>
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "portret.h"
#include "portret_ekran.h"

using namespace portret;

struct Bufor : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)y * 800 + (size_t)x] = c ? 1 : 0;
  }
  bool cz(int x, int y) const { return x >= 0 && x < 800 && y >= 0 && y < 480 && px[(size_t)y * 800 + (size_t)x]; }
};

static int bledy = 0;
#define WYMAG(w...)                     \
  do {                                    \
    if (!(w)) {                           \
      bledy++;                            \
      printf("  NIE: %s: ", #w);          \
      printf(__VA_ARGS__);                \
      printf("\n");                       \
    }                                     \
  } while (0)

static bool czytaj(const std::string& s, std::string& t) {
  std::ifstream f(s, std::ios::binary);
  if (!f) return false;
  std::stringstream b;
  b << f.rdbuf();
  t = b.str();
  return true;
}
static std::vector<std::string> linie(const std::string& t) {
  std::vector<std::string> v;
  std::stringstream s(t);
  std::string l;
  while (std::getline(s, l)) v.push_back(l);
  return v;
}
static void zapiszPbm(const Bufor& b, const std::string& s) {
  std::ofstream f(s, std::ios::binary);
  f << "P4\n800 480\n";
  for (int y = 0; y < 480; y++)
    for (int x0 = 0; x0 < 800; x0 += 8) {
      unsigned char c = 0;
      for (int i = 0; i < 8; i++)
        if (b.cz(x0 + i, y)) c |= (unsigned char)(0x80 >> i);
      f.put((char)c);
    }
}
static bool czytajPbm(const std::string& s, std::vector<uint8_t>& px) {
  std::string t;
  if (!czytaj(s, t) || t.compare(0, 11, "P4\n800 480\n") != 0) return false;
  px.assign(800 * 480, 0);
  for (int y = 0; y < 480; y++)
    for (int x = 0; x < 800; x++) px[(size_t)y * 800 + (size_t)x] = ((uint8_t)t[11 + (size_t)y * 100 + (size_t)x / 8] >> (7 - x % 8)) & 1;
  return true;
}

// ---------- garby ----------
static int garby(const char* plik) {
  FILE* f = fopen(plik, "w");
  if (!f) return 1;
  for (int h = 0; h <= 432; h++)
    for (long d = -1300; d <= 1300; d++) fprintf(f, "%d %ld %d\n", h, d, garb16(h, d));
  fclose(f);
  return 0;
}

// ---------- porównanie z wzorcem na nocach losowych ----------
static int porownaj(const std::string& kat) {
  std::vector<std::string> v;
  if (DIR* d = opendir(kat.c_str())) {
    while (dirent* e = readdir(d)) {
      std::string n = e->d_name;
      if (n.size() > 4 && n.compare(n.size() - 4, 4, ".pbm") == 0) v.push_back(n.substr(0, n.size() - 4));
    }
    closedir(d);
  }
  std::sort(v.begin(), v.end());
  long sumaR = 0, sumaPik = 0, maksR = 0, ponadTol = 0, kolumna = 0, nocy = 0, bloki = 0;
  double maksMs = 0;
  std::string najgorsza;
  for (auto& n : v) {
    std::string base = kat + "/" + n, csv, txt, blad;
    std::vector<uint8_t> wz;
    if (!czytajPbm(base + ".pbm", wz) || !czytaj(base + ".csv", csv) || !czytaj(base + ".txt", txt)) continue;
    std::map<std::string, std::string> we;
    for (auto& l : linie(txt)) {
      size_t r = l.find('=');
      if (r != std::string::npos) we[l.substr(0, r)] = l.substr(r + 1);
    }
    std::vector<std::string> l = linie(csv);
    Przebieg pr;
    if (!parsuj(std::vector<std::string>(l.begin() + 2, l.end() - 1), pr, blad)) {
      printf("  %s: parsuj: %s\n", n.c_str(), blad.c_str());
      bledy++;
      continue;
    }
    DanePortretu d;
    d.przebieg = &pr;
    d.tryb = we["tryb"];
    d.normaPs10 = we["norma_ps10"].empty() ? BRAK : std::atoi(we["norma_ps10"].c_str());
    d.zwykla = std::atoi(we["zwykla"].c_str());
    d.pierwszy = we["pierwszy"];
    d.czas = we["czas"];
    d.ahi = we["ahi"];
    bool linia = !we["linia"].empty();
    Bufor b;
    int gora = 54 + (linia ? 48 : 0), skala = linia ? 85 : 100, cy = (gora + 434) / 2;
    auto t0 = std::chrono::steady_clock::now();
    rysujCialoPortretu(b, d, gora, linia);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    maksMs = std::max(maksMs, ms);
    const int rOut = (134 * skala + 50) / 100, rIn = (118 * skala + 50) / 100;
    long r = 0, rp = 0, k = 0;
    for (int y = gora; y < 434; y++)
      for (int x = 0; x < 800; x++) {
        size_t i = (size_t)y * 800 + (size_t)x;
        if (wz[i] == b.px[i]) continue;
        r++;
        if (x >= 380) k++;
        long dx = x - 200, dy = y - cy, r2 = dx * dx + dy * dy;
        if (r2 >= (long)rOut * rOut || r2 < (long)rIn * rIn) rp++;
      }
    bool blok = false;
    for (int y = gora + 1; y < 433 && !blok; y++)
      for (int x = 1; x < 799 && !blok; x++) {
        bool caly = true;
        for (int dy = -1; dy <= 1 && caly; dy++)
          for (int dx = -1; dx <= 1 && caly; dx++) {
            size_t i = (size_t)(y + dy) * 800 + (size_t)(x + dx);
            caly = wz[i] != b.px[i];
          }
        blok = caly;
      }
    bloki += blok;
    nocy++;
    sumaR += r;
    sumaPik += rp;
    kolumna += k;
    if (r > maksR) {
      maksR = r;
      najgorsza = n;
    }
    if (r > 60) ponadTol++;
  }
  printf("porównanie panel/wzorzec: nocy %ld, różnych pikseli razem %ld (w pasie pików %ld), najwięcej w nocy %ld (%s), "
         "nocy ponad 60 px %ld, z blokiem 3×3 %ld, różnic w kolumnie %ld, najdłuższe rysowanie %.1f ms (PC, -O1)\n",
         nocy, sumaR, sumaPik, maksR, najgorsza.c_str(), ponadTol, bloki, kolumna, maksMs);
  return 0;
}

// ---------- scenariusze ----------
static Odcinek odc(int m, int zew, int centr = 0, int ps = 60) {
  Odcinek o;
  o.od = m;
  o.minuty = 5;
  o.wyciek95 = 5;
  o.wycS = 0;
  o.zew = zew;
  o.centr = centr;
  o.ps = ps;
  o.maks = 0;
  return o;
}
static Przebieg noc(std::vector<Sesja> ses) {
  Przebieg p;
  p.data = "2026-11-14";
  p.sesje = ses;
  for (auto& s : ses)
    for (int m = s.od / 5 * 5; m < s.do_; m += 5) p.odcinki.push_back(odc(m, 0));
  return p;
}
static Odcinek& o(Przebieg& p, int m) {
  for (auto& x : p.odcinki)
    if (x.od == m) return x;
  p.odcinki.push_back(odc(m, 0));
  std::sort(p.odcinki.begin(), p.odcinki.end(), [](const Odcinek& a, const Odcinek& b) { return a.od < b.od; });
  for (auto& x : p.odcinki)
    if (x.od == m) return x;
  return p.odcinki[0];
}
static DanePortretu dane(const Przebieg& p, const char* tryb, int zwykla) {
  DanePortretu d;
  d.przebieg = &p;
  d.tryb = tryb;
  d.zwykla = zwykla;
  d.normaPs10 = 50;
  d.pierwszy = "Tydzień jak zwykle.";
  d.czas = "6 h 00";
  d.ahi = "1,0";
  return d;
}
// kąt tarczy w sekundach (0 = 12) dla chwili u (s od 12:00)
static void punktU(long u, double r, int cy, int& x, int& y) {
  double a = (double)(u % 43200) / 43200.0 * 2 * M_PI;
  x = 200 + (int)std::lround(r * std::sin(a));
  y = cy - (int)std::lround(r * std::cos(a));
}
// najdalszy czarny promień na zewnątrz pierścienia w kierunku chwili u (piksele wzdłuż promienia, krok 0,25 px)
static double zasiegZewn(const Bufor& b, long u, int cy, int rOut) {
  double best = 0;
  for (double r = rOut; r < 175; r += 0.25) {
    int x, y;
    punktU(u, r, cy, x, y);
    if (b.cz(x, y)) best = r - rOut + 0.5;
  }
  return best;
}
static int czarneWLuku(const Bufor& b, long u0, long u1, int cy, int r0, int r1) {
  int n = 0;
  for (int y = cy - r1; y <= cy + r1; y++)
    for (int x = 200 - r1; x <= 200 + r1; x++) {
      long dx = x - 200, dy = y - cy, r2 = dx * dx + dy * dy;
      if (r2 < (long)r0 * r0 || r2 >= (long)r1 * r1 || !b.cz(x, y)) continue;
      double a = std::atan2((double)dx, (double)-dy);
      if (a < 0) a += 2 * M_PI;
      long u = (long)(a / (2 * M_PI) * 43200);
      long a0 = ((u0 % 43200) + 43200) % 43200, a1 = ((u1 % 43200) + 43200) % 43200;
      bool w = a0 <= a1 ? (u >= a0 && u < a1) : (u >= a0 || u < a1);
      n += w;
    }
  return n;
}

static int scenariusze(const std::string& kat) {
  // (1) wysokość czubka = wartość odcinka, każda wartość, samotny pik i pik obok wyższego/niższego sąsiada
  printf("(1) czubek piku a wartość odcinka (samotny pik, środek odcinka 02:00):\n");
  for (int skala : {100, 85}) {
    bool linia = skala == 85;
    int gora = linia ? 102 : 54, cy = (gora + 434) / 2, rOut = (134 * skala + 50) / 100;
    for (const char* tryb : {"AutoSet", "ASVAuto"}) {
      int n = std::string(tryb) == "ASVAuto" ? 13 : 13;
      for (int v = 1; v < n; v++) {
        Przebieg p = noc({{600, 1080}});
        if (std::string(tryb) == "ASVAuto") o(p, 840).ps = 50 + 20 + (v - 1) * 5;  // ΔPS 2,0 … 7,5
        else o(p, 840).zew = v;
        DanePortretu d = dane(p, tryb, 720);
        Bufor b;
        rysujCialoPortretu(b, d, gora, linia);
        double h = pikZewn16(d, o(p, 840), skala) / 16.0;
        double z = zasiegZewn(b, 840 * 60 + 150, cy, rOut);
        WYMAG(std::fabs(z - h) <= 1.0, "%s skala %d v %d: czubek %.2f px, wartość %.3f px", tryb, skala, v, z, h);
        if (skala == 100 && (v == 1 || v == 7 || v == 12))
          printf("  %s v=%d skala %d: wartość %.3f px, czubek na obrazku %.2f px\n", tryb, v, skala, h, z);
      }
    }
  }
  // CPAP: 1 zdarzenie obok 7: czy niższy odcinek jest w ogóle widoczny jako osobny czubek
  {
    printf("(1b) sąsiedzi o różnych wysokościach (AutoSet, odcinek 02:00 z k, 02:05 z 7):\n");
    for (int k = 1; k <= 7; k++) {
      std::vector<int> h((size_t)ODCINKI_DOBY, 0);
      Przebieg p = noc({{600, 1080}});
      o(p, 840).zew = k;
      o(p, 845).zew = 7;
      DanePortretu d = dane(p, "AutoSet", 720);
      h[168] = pikZewn16(d, o(p, 840));
      h[169] = pikZewn16(d, o(p, 845));
      int naSrodku = obwiednia16(h, 840 * 60 + 150);
      // czy jest lokalne maksimum (osobny czubek) w okolicy środka odcinka 02:00
      bool czubek = false;
      for (long u = 840 * 60; u < 845 * 60 - 1; u++)
        if (obwiednia16(h, u) > obwiednia16(h, u + 1) && obwiednia16(h, u) >= obwiednia16(h, u - 1)) czubek = true;
      printf("  k=%d: wartość %.2f px, obwiednia na środku odcinka %.2f px, osobny czubek: %s\n", k, h[168] / 16.0,
             naSrodku / 16.0, czubek ? "tak" : "NIE (niewidoczny)");
    }
    // odcinek bez zdarzeń obok 7 zdarzeń
    std::vector<int> h((size_t)ODCINKI_DOBY, 0);
    h[169] = 432;
    printf("  odcinek bez zdarzeń obok odcinka z 7: obwiednia na jego środku %.2f px (= pik 5 zdarzeń %.2f px), "
           "2 odcinki dalej %.2f px\n",
           obwiednia16(h, 168 * 300 + 150) / 16.0, 338 / 16.0, obwiednia16(h, 167 * 300 + 150) / 16.0);
  }
  // (2) arytmetyka: brzegi
  printf("(2) arytmetyka:\n");
  {
    WYMAG(garb16(432, 1259) > 0 && garb16(432, 1260) == 0, "brzeg tablicy");
    WYMAG(garb16(INT32_MAX / 70000, 0) == INT32_MAX / 70000, "duże h");
    WYMAG(garb16(-5, 0) == 0, "h < 0");
    WYMAG(garb16(432, LONG_MIN + 1) == 0, "d bardzo ujemne");
    std::vector<int> e((size_t)ODCINKI_DOBY, 0);
    WYMAG(obwiednia16(e, -100000) == 0 && obwiednia16(e, 10000000) == 0, "u daleko poza dobą");
    std::vector<int> pusty;
    WYMAG(obwiednia16(pusty, 100) == 0, "pusty wektor");
    // monotoniczność względem h przy stałym d (wyższy odcinek nigdy niższy garb)
    bool mono = true;
    for (int h = 1; h < 432; h++)
      for (long d = 0; d < 1260; d += 7) mono = mono && garb16(h, d) <= garb16(h + 1, d);
    WYMAG(mono, "garb16 rosnący w h");
    // okno przed 12:00 (u ujemne) i okno po północy doby (u > 86400)
    for (int zw : {50, 100, 1380, 1430}) {
      Przebieg p = noc({{zw, std::min(1439, zw + 400)}});
      o(p, zw / 5 * 5).zew = 7;
      DanePortretu d = dane(p, "AutoSet", zw);
      Bufor b;
      rysujCialoPortretu(b, d, 54, false);
      int w = okno(d);
      printf("  zwykła %d → okno %d min (%s), czubek pierwszego odcinka %.2f px\n", zw, w, hhmmOdPoludnia(w).c_str(),
             zasiegZewn(b, (long)(zw / 5 * 5) * 60 + 150 + 43200 * 10, 244, 134));
    }
  }
  // (4)+(5) pik przy znaczniku startu okna, przy przerwie, przed startem sesji, w skali 0,85, maksymalne wysokości
  printf("(4/5) kolizje i garb poza maską:\n");
  struct Sc {
    const char* nazwa;
    std::vector<Sesja> ses;
    std::vector<std::pair<int, int>> piki;  // minuta odcinka, k
    int zwykla;
    bool linia;
    const char* tryb;
  };
  std::vector<Sc> sc = {
      {"pik-pierwszy-odcinek-okna", {{600, 1000}}, {{600, 7}}, 720, false, "AutoSet"},
      {"pik-ostatni-odcinek-okna", {{600, 1315}}, {{1310, 7}}, 720, false, "AutoSet"},
      {"pik-oba-brzegi-okna-085", {{600, 1315}}, {{600, 7}, {1310, 7}}, 720, true, "AutoSet"},
      {"pik-10-min-przed-oknem", {{610, 1000}}, {{610, 7}}, 720, false, "AutoSet"},
      {"pik-przy-przerwie-20-min", {{660, 840}, {860, 1100}}, {{835, 7}, {860, 7}}, 720, false, "AutoSet"},
      {"pik-w-krotkiej-przerwie-10-min", {{660, 840}, {850, 1100}}, {{835, 1}}, 720, false, "AutoSet"},
      {"pik-koniec-sesji", {{660, 900}}, {{895, 7}}, 720, false, "AutoSet"},
      {"pik-przy-wycieku", {{660, 1100}}, {{835, 7}}, 720, false, "AutoSet"},
      {"asv-max-kropki", {{660, 1100}}, {{835, 99}, {840, 99}, {845, 99}}, 720, false, "ASVAuto"},
      {"centralne-max-085", {{660, 1100}}, {{720, 77}, {900, 77}, {1080, 77}, {990, 77}}, 720, true, "CPAP"},
      {"maks-wszedzie", {{600, 1315}}, {}, 720, false, "AutoSet"},
      {"maks-wszedzie-085", {{600, 1315}}, {}, 720, true, "AutoSet"},
  };
  for (auto& s : sc) {
    Przebieg p = noc(s.ses);
    for (auto& pk : s.piki) {
      Odcinek& x = o(p, pk.first);
      if (pk.second == 99) x.ps = 130;
      else if (pk.second == 77) x.centr = 12, x.zew = 12;
      else x.zew = pk.second;
      if (std::string(s.tryb) == "ASVAuto") p.zdarzenia.push_back({pk.first + 2, true, false});
    }
    if (std::string(s.nazwa) == "pik-przy-wycieku") o(p, 840).wycS = 300, o(p, 840).wyciek95 = 25;
    if (std::string(s.nazwa).rfind("maks-wszedzie", 0) == 0)
      for (auto& x : p.odcinki) x.zew = 12, x.centr = 12;
    DanePortretu d = dane(p, s.tryb, s.zwykla);
    Bufor b;
    int gora = s.linia ? 102 : 54, skala = s.linia ? 85 : 100, cy = (gora + 434) / 2;
    rysujCialoPortretu(b, d, gora, s.linia);
    zapiszPbm(b, kat + "/rec-" + s.nazwa + ".pbm");
    int w = okno(d);
    int rOut = (134 * skala + 50) / 100, rIn = (118 * skala + 50) / 100, rMax = (172 * skala + 50) / 100;
    // czarne piksele poza strefą tarczy
    int poza = 0;
    for (int y = 0; y < 480; y++)
      for (int x = 0; x < 390; x++)
        if (b.cz(x, y) && (x >= 380 || y < gora || y >= 434)) poza++;
    // piki na zewnątrz w chwilach bez maski (poza sesjami), w oknie
    int bezMaski = 0;
    double maksBez = 0;
    for (long u = (long)w * 60; u < (long)(w + 720) * 60; u += 10) {
      bool ses = false;
      for (auto& z : p.sesje) ses = ses || ((long)z.od * 60 <= u && u < (long)z.do_ * 60);
      if (ses) continue;
      long odStartu = ((u - (long)w * 60) % 43200 + 43200) % 43200;
      if (odStartu < 900 || odStartu > 43200 - 900) continue;  // bez okolicy znacznika startu (grot)
      double z = zasiegZewn(b, u, cy, rOut);
      if (z > 0) bezMaski++, maksBez = std::max(maksBez, z);
    }
    // maks. zasięg piku i odległość od cyfr (promień cyfr 82 ± ok. 9): najmniejszy promień czarnego piku do środka
    double rMinWewn = 1e9;
    for (int y = cy - rIn; y <= cy + rIn; y++)
      for (int x = 200 - rIn; x <= 200 + rIn; x++) {
        long dx = x - 200, dy = y - cy, r2 = dx * dx + dy * dy;
        if (r2 >= (long)rIn * rIn || r2 < (long)((96 * skala + 50) / 100) * ((96 * skala + 50) / 100)) continue;
        if (b.cz(x, y)) rMinWewn = std::min(rMinWewn, std::sqrt((double)r2));
      }
    double rMaxZewn = 0;
    for (int y = cy - rMax; y <= cy + rMax; y++)
      for (int x = 200 - rMax; x <= 200 + rMax; x++) {
        long dx = x - 200, dy = y - cy, r2 = dx * dx + dy * dy;
        if (r2 >= (long)rOut * rOut && b.cz(x, y)) rMaxZewn = std::max(rMaxZewn, std::sqrt((double)r2));
      }
    printf("  %-32s okno %s, pikseli poza strefą %d, chwil bez maski z pikiem %d (co 10 s, poza ±15 min od znacznika; najwyższy %.1f px), "
           "najdalej na zewnątrz r=%.1f, najbliżej środka r=%.1f\n",
           s.nazwa, hhmmOdPoludnia(w).c_str(), poza, bezMaski, maksBez, rMaxZewn, rMinWewn > 1e8 ? 0 : rMinWewn);
    WYMAG(poza == 0, "%s", s.nazwa);
  }
  // (6) pojedyncze zdarzenie: widoczne wysokie piksele (liczba pikseli piku i wysokość) w skali 100 i 85
  printf("(6) pojedyncze zdarzenie:\n");
  for (int skala : {100, 85}) {
    bool linia = skala == 85;
    int gora = linia ? 102 : 54, cy = (gora + 434) / 2, rOut = (134 * skala + 50) / 100;
    int najmn = 1000;
    double najnizszy = 100;
    for (int m = 600; m < 1300; m += 5) {
      Przebieg p = noc({{595, 1320}});
      o(p, m).zew = 1;
      DanePortretu d = dane(p, "AutoSet", 720);
      Bufor b;
      rysujCialoPortretu(b, d, gora, linia);
      najnizszy = std::min(najnizszy, zasiegZewn(b, (long)m * 60 + 150, cy, rOut));
      najmn = std::min(najmn, czarneWLuku(b, (long)m * 60 - 1500, (long)m * 60 + 1800, cy, rOut + 1, rOut + 40));
    }
    printf("  skala %d: najniższy czubek pojedynczego zdarzenia na obrazku %.2f px, najmniej pikseli piku %d\n", skala,
           najnizszy, najmn);
    WYMAG(najnizszy >= 9.0 * skala / 100 - 0.75, "pojedyncze zdarzenie ≥ 9 px · skala");
  }
  // (7) czas: noc z pikami w każdym odcinku, stary kształt nieznany tu; mierzymy nowy
  {
    Przebieg p = noc({{600, 1315}});
    for (auto& x : p.odcinki) x.zew = 7, x.centr = 7;
    DanePortretu d = dane(p, "AutoSet", 720);
    double maks = 0;
    for (int i = 0; i < 20; i++) {
      Bufor b;
      auto t0 = std::chrono::steady_clock::now();
      rysujCialoPortretu(b, d, 54, false);
      maks = std::max(maks, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    }
    long wyw = 0;
    std::vector<int> h((size_t)ODCINKI_DOBY, 432);
    auto t0 = std::chrono::steady_clock::now();
    for (long u = 0; u < 43200 * 2; u++) wyw += obwiednia16(h, u);
    double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    printf("(7) najgorszy przypadek (piki w każdym odcinku 12 h, oba kierunki): rysowanie do %.2f ms (PC, -O1); "
           "obwiednia16 %.3f µs/wywołanie (%ld)\n",
           maks, us / 86400.0, wyw % 7);
  }
  printf("scenariusze: %d niespełnionych\n", bledy);
  return bledy ? 1 : 0;
}

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "garby") return garby(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "porownaj") return porownaj(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "scenariusze") return scenariusze(argv[2]);
  printf("użycie: dzwon_brzegowe garby PLIK | porownaj KATALOG | scenariusze KATALOG\n");
  return 2;
}
