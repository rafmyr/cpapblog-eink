// SPDX-License-Identifier: GPL-3.0-or-later
// Trwale nieczytelny plik danych (src/nieczytelne.h).
#include "nieczytelne.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "wspolne.h"

namespace nieczytelne {

namespace {

void wyczysc(Wpis& w) { std::memset(&w, 0, sizeof w); }

Wpis* znajdz(Stan& s, const std::string& sc) {
  for (auto& w : s.w)
    if (w.sciezka[0] && sc == w.sciezka) return &w;
  return nullptr;
}

bool ustawSciezke(Wpis& w, const std::string& sc) {
  if (sc.empty() || sc.size() >= DLUGOSC_SCIEZKI) return false;  // za długa: tylko log, jak przed zmianą
  std::memcpy(w.sciezka, sc.c_str(), sc.size() + 1);
  return true;
}

}  // namespace

bool odtworz(Stan& s, const std::string& tekst) {
  if (s.magia == MAGIA) return false;
  std::memset(&s, 0, sizeof s);
  s.magia = MAGIA;
  int i = 0;
  size_t p = 0;
  while (p < tekst.size()) {
    size_t k = tekst.find('\n', p);
    if (k == std::string::npos) k = tekst.size();
    const std::string l = tekst.substr(p, k - p);
    p = k + 1;
    size_t at = l.rfind('@');
    if (at == std::string::npos || at == 0 || i >= MIEJSC) continue;
    Wpis& w = s.w[i];
    if (!ustawSciezke(w, l.substr(0, at))) continue;
    w.od = (uint32_t)std::strtoul(l.c_str() + at + 1, nullptr, 10);
    w.kolejnych = WYBUDZEN_DO_SPRAWY;
    w.wybudzenie = 0;
    i++;
  }
  std::string t = tekstNvs(s);
  s.skrotNvs = t.empty() ? 0 : crc32((const uint8_t*)t.data(), t.size());
  return true;
}

bool aktualizuj(Stan& s, uint32_t wybudzenie, const std::vector<std::string>& nieudane,
                const std::function<bool(const std::string&)>& udany,
                const std::function<bool(const std::string&)>& jest, uint32_t teraz) {
  if (s.magia != MAGIA) odtworz(s, "");
  const std::string przed = tekstNvs(s);
  for (const std::string& sc : nieudane) {
    if (Wpis* w = znajdz(s, sc)) {
      if (teraz) w->ostatnio = teraz;
      if (w->wybudzenie == wybudzenie) continue;  // to wybudzenie już policzone
      w->wybudzenie = wybudzenie;
      if (w->kolejnych < WYBUDZEN_DO_SPRAWY) w->kolejnych++;
      continue;
    }
    Wpis* cel = nullptr;
    for (auto& w : s.w)
      if (!w.sciezka[0]) {
        cel = &w;
        break;
      }
    // komplet miejsc, nowa ścieżka wypiera najstarszy wpis poniżej progu, który nie był nieudany w tym
    // wybudzeniu (sprawy zostają; przy więcej niż MIEJSC ścieżkach nieudanych naraz liczą się pierwsze, bez rotacji)
    // plik spoza oceny nigdy nie wypiera wpisu pliku oceny (inaczej wyparta sprawa wraca i wypiera go z powrotem)
    if (!cel)
      for (auto& w : s.w)
        if (w.kolejnych < WYBUDZEN_DO_SPRAWY && w.wybudzenie != wybudzenie && (plikOceny(sc) || !plikOceny(w.sciezka)) &&
            (!cel || w.od < cel->od))
          cel = &w;
    // plik oceny zawsze dostaje miejsce: wypiera najstarszy wpis (także sprawę) pliku spoza oceny
    if (!cel && plikOceny(sc))
      for (auto& w : s.w)
        if (!plikOceny(w.sciezka) && (!cel || w.od < cel->od)) cel = &w;
    if (!cel) continue;
    Wpis n;
    std::memset(&n, 0, sizeof n);
    if (!ustawSciezke(n, sc)) continue;  // za długa: tylko log, jak przed zmianą
    n.od = teraz;
    n.ostatnio = teraz;
    n.wybudzenie = wybudzenie;
    n.kolejnych = 1;
    *cel = n;
  }
  for (auto& w : s.w) {
    if (!w.sciezka[0]) continue;
    const std::string sc = w.sciezka;
    if (std::find(nieudane.begin(), nieudane.end(), sc) == nieudane.end()) {
      // udany dostęp (odczyt, zapis, dopisanie), pliku nie ma na nośniku, panel nie czyta go od ponad
      // WYGASA_S: koniec serii i sprawy
      if ((udany && udany(sc)) || (jest && !jest(sc)) ||
          (teraz && w.ostatnio && teraz > w.ostatnio && teraz - w.ostatnio > WYGASA_S)) {
        wyczysc(w);
        continue;
      }
    }
    if (teraz && !w.ostatnio) w.ostatnio = teraz;  // odtworzony z NVS albo seria bez zegara: czas od teraz
    if (teraz && !w.od) w.od = teraz;              // data „od”, gdy pojawi się zegar
  }
  return tekstNvs(s) != przed;
}

bool ocenaNiepelna(const Stan& s, const std::vector<std::string>& nieudaneTeraz) {
  if (s.magia == MAGIA)
    for (auto& w : s.w)
      if (w.sciezka[0] && w.kolejnych >= WYBUDZEN_DO_SPRAWY && plikOceny(w.sciezka)) return true;
  for (const std::string& sc : nieudaneTeraz) {
    if (!plikOceny(sc)) continue;
    bool wpis = false;
    if (s.magia == MAGIA)
      for (auto& w : s.w) wpis = wpis || sc == w.sciezka;
    if (!wpis) return true;
  }
  return false;
}

bool plikOceny(const std::string& sc) {
  static const char* const PLIKI[] = {"/dane/czynniki.csv", "/dane/odpowiedzi.csv", "/dane/reguly.csv"};
  for (const char* p : PLIKI)
    if (sc == p) return true;
  const std::string k = "/dane/noce/";  // pliki roczne nocy `/dane/noce/RRRR.csv`
  return sc.size() == k.size() + 8 && sc.compare(0, k.size(), k) == 0 && sc.compare(k.size() + 4, 4, ".csv") == 0;
}

std::vector<Sprawa> sprawy(const Stan& s) {
  std::vector<Sprawa> r;
  if (s.magia != MAGIA) return r;
  for (auto& w : s.w)
    if (w.sciezka[0] && w.kolejnych >= WYBUDZEN_DO_SPRAWY) r.push_back({w.sciezka, w.od});
  std::stable_sort(r.begin(), r.end(), [](const Sprawa& a, const Sprawa& b) {
    if (a.od != b.od) return a.od < b.od;
    return a.sciezka < b.sciezka;
  });
  return r;
}

std::string tekstNvs(const Stan& s) {
  std::string t;
  for (const Sprawa& x : sprawy(s)) t += x.sciezka + "@" + std::to_string(x.od) + "\n";
  return t;
}

std::string rodzaj(const std::string& sc) {
  auto rok = [&](const char* katalog, const char* nazwa) -> std::string {
    size_t n = std::strlen(katalog);
    if (sc.size() == n + 8 && sc.compare(0, n, katalog) == 0 && sc.compare(n + 4, 4, ".csv") == 0)
      return std::string(nazwa) + " " + sc.substr(n, 4);
    return "";
  };
  std::string r = rok("/dane/noce/", "noce");
  if (r.empty()) r = rok("/dane/sypialnia/", "sypialnia");
  if (!r.empty()) return r;
  static const char* const NAZWY[][2] = {
      {"/dane/czynniki.csv", "czynniki nocy"},     {"/dane/humor.csv", "dziennik ekranów"},
      {"/log/wybudzenia.csv", "dziennik baterii"}, {"/dane/ab-pominiete.csv", "lista pobrań"},
      {"/dane/ab-proby.csv", "lista pobrań"},      {"/dane/reguly.csv", "pamięć spraw"},
      {"/dane/odpowiedzi.csv", "odpowiedzi"},      {"/dane/prognoza.csv", "prognoza"},
      {"/dane/obrazki.csv", "obrazki dnia"},       {"/dane/porownanie.csv", "porównanie nocy"},
      {"/dane/portret.csv", "portret nocy"},       {"/dane/dobry_start.csv", "Dobry start"},
  };
  for (auto& n : NAZWY)
    if (sc == n[0]) return n[1];
  return "plik danych";
}

}  // namespace nieczytelne
