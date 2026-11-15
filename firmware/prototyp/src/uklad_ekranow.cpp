// SPDX-License-Identifier: GPL-3.0-or-later
// Układ ekranów z telefonu: opis w src/uklad_ekranow.h (ADR-0045).
#include "uklad_ekranow.h"

const char* const ID_STRONY[STRON_PRZESTAWNYCH] = {"ostatnia_noc", "7_nocy", "sypialnia"};
const char* const NAZWA_STRONY[STRON_PRZESTAWNYCH] = {"Ostatnia noc", "7 nocy", "Sypialnia · 30 nocy"};
const char* const ID_DZIENNY[RODZAJOW_DZIENNYCH] = {"wietrzenie", "jutro_rano", "wymiana", "pylek", "dzien_i_noc",
                                                    "obrazek"};
const char* const NAZWA_DZIENNEGO[RODZAJOW_DZIENNYCH] = {"Wietrzenie", "Jutro rano", "Wymiana sprzętu",
                                                         "Pyłek w dzień", "Dzień i noc", "Obrazek dnia"};
const char* const KLUCZ_KOKPIT_KOLEJNOSC = "kokpit.kolejnosc";
const char* const KLUCZ_KOKPIT_UKRYTE = "kokpit.ukryte";
const char* const KLUCZ_DZIEN_KOLEJNOSC = "dzien.kolejnosc";
const char* const KLUCZ_DZIEN_UKRYTE = "dzien.ukryte";

namespace {
// wartość klucza układu czytana najwyżej do MAKS_WARTOSCI bajtów (najdłuższa poprawna, `dzien.ukryte` z sześcioma
// nazwami, ma 52 B), a pominięte nazwy dają jeden wpis na klucz z liczbą i najwyżej PRZYKLADOW przykładami po
// MAKS_PRZYKLADU bajtów, więc ręcznie zepsuty wiersz nie zalewa logu ani pamięci.
const size_t MAKS_WARTOSCI = 160;
const size_t PRZYKLADOW = 3;
const size_t MAKS_PRZYKLADU = 24;

// pominięte nazwy jednego klucza: zbiera, potem jeden wpis w `bledy`
struct Pominiete {
  std::vector<std::string> przyklady;
  size_t ile = 0;
  void dodaj(const std::string& x, const char* dlaczego) {
    if (ile++ >= PRZYKLADOW) return;
    std::string n = x.size() > MAKS_PRZYKLADU ? x.substr(0, MAKS_PRZYKLADU) + "…" : x;
    przyklady.push_back(n + " " + dlaczego);
  }
  // jedna nazwa: `kokpit.ukryte: 7_nocy zawsze włączone`; więcej: `kokpit.ukryte: pominięte 5 nazw: a nieznane, …`
  void doBledow(const std::string& klucz, std::vector<std::string>& bledy) const {
    if (ile == 0) return;
    if (ile == 1) {
      bledy.push_back(klucz + ": " + przyklady.front());
      return;
    }
    const size_t j = ile % 10, d = ile % 100;
    const bool kilka = j >= 2 && j <= 4 && !(d >= 12 && d <= 14);  // 2 nazwy, 5 nazw, 22 nazwy, 12 nazw
    std::string w = klucz + ": pominięte " + std::to_string(ile) + (kilka ? " nazwy" : " nazw");
    for (size_t i = 0; i < przyklady.size(); i++) w += (i ? ", " : ": ") + przyklady[i];
    if (ile > przyklady.size()) w += ", …";
    bledy.push_back(w);
  }
};

// nazwy ekranów zawsze włączonych, które ktoś mógłby wpisać do `*.ukryte` (rozpoznane, żeby log mówił „zawsze
// włączone”, a nie „nieznane”); wyłączyć się ich nie da (ADR-0045 pkt 3)
const char* const ZAWSZE_KOKPIT[] = {"ostatnia_noc", "7_nocy", "ustawienia", "sprawy", "liczby"};

std::vector<std::string> nazwy(const std::string& calosc) {  // lista po przecinku, bez spacji na brzegach, bez pustych
  const std::string v = calosc.substr(0, MAKS_WARTOSCI);
  std::vector<std::string> r;
  size_t i = 0;
  while (i <= v.size()) {
    size_t j = v.find(',', i);
    if (j == std::string::npos) j = v.size();
    std::string x = v.substr(i, j - i);
    while (!x.empty() && (x[0] == ' ' || x[0] == '\t')) x.erase(0, 1);
    while (!x.empty() && (x.back() == ' ' || x.back() == '\t')) x.pop_back();
    if (!x.empty()) r.push_back(x);
    i = j + 1;
  }
  return r;
}
int indeks(const char* const* lista, int n, const std::string& x) {
  for (int i = 0; i < n; i++)
    if (x == lista[i]) return i;
  return -1;
}
// Kolejność z listy nazw: znane, niepowtórzone w podanej kolejności, brakujące na końcu w kolejności domyślnej.
// `wynik` dostaje indeksy 0…n−1 (permutacja).
void kolejnosc(const std::string& klucz, const std::string& v, const char* const* ids, int n, int* wynik,
               std::vector<std::string>& bledy) {
  bool byla[8] = {};
  int k = 0;
  Pominiete pom;
  for (const std::string& x : nazwy(v)) {
    int i = indeks(ids, n, x);
    if (i < 0) { pom.dodaj(x, "nieznane"); continue; }
    if (byla[i]) { pom.dodaj(x, "powtórzone"); continue; }
    byla[i] = true;
    wynik[k++] = i;
  }
  for (int i = 0; i < n; i++)
    if (!byla[i]) wynik[k++] = i;
  pom.doBledow(klucz, bledy);
}
}  // namespace

bool UkladDnia::domyslny() const {
  UkladDnia d;
  for (int i = 0; i < RODZAJOW_PRZESTAWNYCH; i++)
    if (kolejnosc[i] != d.kolejnosc[i]) return false;
  for (int i = 0; i < RODZAJOW_DZIENNYCH; i++)
    if (!pokaz[i]) return false;
  return true;
}

int UkladDnia::wlaczonych() const {
  int n = 0;
  for (bool b : pokaz) n += b ? 1 : 0;
  return n;
}

bool wierszUkladu(const std::string& k, const std::string& v, UkladEkranow& u, std::vector<std::string>& bledy) {
  if (k == KLUCZ_KOKPIT_KOLEJNOSC) {
    int w[STRON_PRZESTAWNYCH];
    kolejnosc(k, v, ID_STRONY, STRON_PRZESTAWNYCH, w, bledy);
    for (int i = 0; i < STRON_PRZESTAWNYCH; i++) u.kokpit.kolejnosc[i] = static_cast<StronaPrzestawna>(w[i]);
    return true;
  }
  if (k == KLUCZ_KOKPIT_UKRYTE) {
    u.kokpit.sypialnia = true;
    Pominiete pom;
    for (const std::string& x : nazwy(v)) {
      if (x == "sypialnia") u.kokpit.sypialnia = false;
      else if (indeks(ZAWSZE_KOKPIT, (int)(sizeof ZAWSZE_KOKPIT / sizeof *ZAWSZE_KOKPIT), x) >= 0)
        pom.dodaj(x, "zawsze włączone");
      else pom.dodaj(x, "nieznane");
    }
    pom.doBledow(k, bledy);
    return true;
  }
  if (k == KLUCZ_DZIEN_KOLEJNOSC) {
    int w[RODZAJOW_PRZESTAWNYCH];
    kolejnosc(k, v, ID_DZIENNY, RODZAJOW_PRZESTAWNYCH, w, bledy);
    for (int i = 0; i < RODZAJOW_PRZESTAWNYCH; i++) u.dzien.kolejnosc[i] = static_cast<RodzajDzienny>(w[i]);
    return true;
  }
  if (k == KLUCZ_DZIEN_UKRYTE) {
    for (bool& b : u.dzien.pokaz) b = true;
    Pominiete pom;
    for (const std::string& x : nazwy(v)) {
      int i = indeks(ID_DZIENNY, RODZAJOW_DZIENNYCH, x);
      if (i >= 0) u.dzien.pokaz[i] = false;
      else pom.dodaj(x, "nieznane albo zawsze włączone");
    }
    pom.doBledow(k, bledy);
    return true;
  }
  return false;
}

std::string kolejnoscKokpituTekst(const UkladKokpitu& u) {
  UkladKokpitu d;
  bool dom = true;
  std::string r;
  for (int i = 0; i < STRON_PRZESTAWNYCH; i++) {
    dom = dom && u.kolejnosc[i] == d.kolejnosc[i];
    r += (i ? "," : "") + std::string(ID_STRONY[static_cast<int>(u.kolejnosc[i])]);
  }
  return dom ? "" : r;
}
std::string ukryteKokpituTekst(const UkladKokpitu& u) { return u.sypialnia ? "" : "sypialnia"; }
std::string kolejnoscDniaTekst(const UkladDnia& u) {
  UkladDnia d;
  bool dom = true;
  std::string r;
  for (int i = 0; i < RODZAJOW_PRZESTAWNYCH; i++) {
    dom = dom && u.kolejnosc[i] == d.kolejnosc[i];
    r += (i ? "," : "") + std::string(ID_DZIENNY[static_cast<int>(u.kolejnosc[i])]);
  }
  return dom ? "" : r;
}
std::string ukryteDniaTekst(const UkladDnia& u) {
  std::string r;
  for (int i = 0; i < RODZAJOW_DZIENNYCH; i++)
    if (!u.pokaz[i]) r += (r.empty() ? "" : ",") + std::string(ID_DZIENNY[i]);
  return r;
}
