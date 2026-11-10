// SPDX-License-Identifier: GPL-3.0-or-later
// Porównanie nocy liczonej kodem panelu z plików sesji (firmware/prototyp/src/noc_pliki.cpp, ADR-0036) z raportem
// nocy AirBridge (`/api/report?source=edf&day=N`) w tolerancjach WY9 (spec 9.1). Bramka przed ustawieniem
// `noc_zrodlo=plik` na panelu (ADR-0036 pkt 5). Kompilacja i użycie: tools/kalibracja/noc_panelu.sh.
//
// Wejście: katalog z plikami jednej doby (`DATALOG/RRRRMMDD` z karty albo pliki pobrane z AirBridge), plik raportu JSON
// tej doby, opcjonalnie STR.edf (ustawienia doby, M-21; bez niego ustawienia puste). Domyślnie tak jak panel: BRP i SAD
// nie są czytane (`--pelny`: czytane, jak noce.py).
// M-17 i M-18 strony raportu: nocZRaportu() panelu (ścieżka JSON), czyli to, co panel zapisałby z raportu.
// Porównanie: porownanie::porownaj() z firmware/prototyp/src/porownanie.cpp, ta sama funkcja co panel w trybie
// `noc_zrodlo=porownanie` (ADR-0036 „Zmiana 2026-11-09”).
// Wyjście: tabela `metryka;panel;raport;różnica;tolerancja;wynik` (wynik `tak`, `NIE`, `brak w plikach` (rozjazd),
// `brak w raporcie`, `pominięte: aparat nie rozróżnia` (OA/CA nieznane w plikach, 0 w raporcie), `tylko pliki` (PS),
// `brak (bez STR.edf)` (M-21 bez STR)).
// Kod wyjścia: 0 = wszystkie porównywalne metryki w tolerancji, 1 = rozjazd, 2 = błąd wejścia.
#include <dirent.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "airbridge.h"
#include "noc_pliki.h"
#include "porownanie.h"
#include "str.h"
#include "wspolne.h"

using namespace noc_pliki;

static std::string czytaj(const std::string& p, bool& ok) {
  std::ifstream f(p, std::ios::binary);
  ok = (bool)f;
  std::stringstream b;
  b << f.rdbuf();
  return b.str();
}

// Wiersz tabeli z porownanie::porownaj() (ta sama funkcja co tryb `noc_zrodlo=porownanie` panelu).
static void wypisz(const porownanie::Wiersz& x) {
  using porownanie::liczba;
  if (x.tekst) {
    printf("%s;%s;%s;–;ten sam;%s\n", x.opis.c_str(), x.tPanel.empty() ? "–" : x.tPanel.c_str(),
           x.tRaport.empty() ? "–" : x.tRaport.c_str(), porownanie::opisOceny(x.ocena));
    return;
  }
  double r = x.panel - x.raport;
  printf("%s;%s;%s;%s;%s;%s\n", x.opis.c_str(), liczba(x.panel).c_str(), liczba(x.raport).c_str(),
         std::isnan(r) ? "–" : liczba(r).c_str(), liczba(x.tolerancja).c_str(), porownanie::opisOceny(x.ocena));
}

int main(int argc, char** argv) {
  std::vector<std::string> a;
  bool pelny = false;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--pelny") == 0) pelny = true;
    else a.push_back(argv[i]);
  }
  if (a.size() < 2 || a.size() > 3) {
    fprintf(stderr, "Użycie: noc_panelu [--pelny] KATALOG_DOBY RAPORT.json [STR.edf]\n");
    return 2;
  }
  bool ok;
  std::string raport = czytaj(a[1], ok);
  if (!ok) {
    fprintf(stderr, "brak pliku raportu: %s\n", a[1].c_str());
    return 2;
  }
  Json j;
  long dzien = -1;
  double v;
  if (parsujJson(raport, j) && j["day"].liczba(v)) dzien = (long)v;
  if (dzien < 0) {  // z nazwy katalogu RRRRMMDD
    std::string k = a[0];
    while (!k.empty() && k.back() == '/') k.pop_back();
    k = k.substr(k.rfind('/') == std::string::npos ? 0 : k.rfind('/') + 1);
    if (k.size() == 8) dzien = nrDnia(k.substr(0, 4) + "-" + k.substr(4, 2) + "-" + k.substr(6, 2));
  }
  if (dzien < 0) {
    fprintf(stderr, "nie znam numeru doby (pole `day` raportu ani katalog RRRRMMDD)\n");
    return 2;
  }
  Str st;
  bool jestStr = false;
  if (a.size() == 3) {
    std::string b = czytaj(a[2], ok);
    if (!ok || czytajStr(std::move(b), st) != edf::Blad::BRAK) {
      fprintf(stderr, "STR.edf nieczytelny: %s\n", a[2].c_str());
      return 2;
    }
    jestStr = true;
  }
  LicznikNocy l(dzien);
  std::vector<std::string> nazwy;
  if (DIR* d = opendir(a[0].c_str())) {
    while (dirent* e = readdir(d)) nazwy.push_back(e->d_name);
    closedir(d);
  } else {
    fprintf(stderr, "brak katalogu: %s\n", a[0].c_str());
    return 2;
  }
  for (auto& n : nazwy) l.dodajNazwe(n);
  size_t bajty = 0;
  for (auto& n : l.kolejnosc(pelny, pelny)) {
    std::string b = czytaj(a[0] + "/" + n, ok);
    bajty += b.size();
    if (!ok) l.brakPliku(n, "nie da się otworzyć");
    else l.dodajPlik(n, (const uint8_t*)b.data(), b.size());
  }
  WynikNocy w = l.wynik();
  NocAB r = nocZRaportu(raport, dzien, "");
  NocAB p = nocZWyniku(w, jestStr ? &st : nullptr, r.tryb);  // bez STR tryb z raportu (jak `mode` ze statusu)
  printf("# doba %ld (noc %s), %s, plików %s: %u B, sesji %d, raport: %s\n", dzien, dataPoranka(dzien).c_str(),
         pelny ? "jak noce.py (z BRP i SAD)" : "jak panel (bez BRP i SAD)", a[0].c_str(), (unsigned)bajty,
         (int)w.sesje.size(), a[1].c_str());
  for (auto& x : w.problemy) printf("# problem: %s\n", x.c_str());
  for (auto& x : w.uwagi) printf("# uwaga: %s\n", x.c_str());
  printf("metryka;panel;raport;różnica;tolerancja;wynik\n");
  porownanie::Wynik wyn = porownanie::porownaj(p, r, jestStr);
  for (auto& x : wyn.wiersze) wypisz(x);
  printf("# wynik: %s\n", wyn.rozjazdy ? "ROZJAZD poza tolerancją WY9" : "w tolerancji WY9");
  return wyn.rozjazdy ? 1 : 0;
}
