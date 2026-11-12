// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa zestawu reguł i wyników silnika dla test/testy_kolejka_linii.cpp i test/podglad.cpp, niezależna od
// zestawu wbudowanego. Teksty skopiowane dosłownie ze spec funkcjonalnej 5.12.5 (tylko na potrzeby testów szerokości
// i podglądu; na panelu teksty daje zestaw wbudowany z tools/zestaw/). Dane wyłącznie syntetyczne.
#pragma once
#include <string>
#include <vector>

#include "reguly.h"
#include "sprawy.h"

namespace atrapa_b5 {

inline void tekstReguly(reguly::Zestaw& z, reguly::WierszKatalogu& k, const std::string& id, const char* rola,
                        const std::string& t) {
  reguly::Tekst x;
  x.id = id;
  x.rola = rola;
  x.tekst = t;
  z.teksty[id] = x;
  k.teksty.push_back(id);
}

inline reguly::WierszKatalogu wiersz(const std::string& id, const std::string& wariant, reguly::StanReguly st,
                                     reguly::Pilnosc p, const std::string& grupa) {
  reguly::WierszKatalogu k;
  k.id = id;
  k.wariant = wariant;
  k.stan = st;
  k.pilnosc = p;
  k.grupa = grupa;
  k.poziom = grupa == "R5c" ? "problem" : "lekarz";  // reguła pokazywana pacjentowi (wiersz pomocniczy: poziom pusty)
  return k;
}

// Zestaw według ADR-0029 (aktywne L-03 ≥ 10, L-08, L-09, L-11; „tylko raport” L-01, L-02, L-04; L-06
// zablokowany) i jedna reguła testowa z pilnością A („T-A”, jak R-PRZ-13: w 1.x A daje tylko L-06(b), zablokowane).
inline reguly::Zestaw zestaw() {
  using reguly::Pilnosc;
  using reguly::StanReguly;
  reguly::Zestaw z;
  z.identyfikator = "atrapa-b5";
  auto dodaj = [&](reguly::WierszKatalogu k, const char* t, const char* p, const char* kr, const char* l,
                   std::vector<std::pair<const char*, const char*>> podpisy = {}) {
    std::string b = k.id + (k.wariant.empty() ? "" : "." + k.wariant);
    if (t) tekstReguly(z, k, b + ".tytul", "tytul", t);
    if (p) tekstReguly(z, k, b + ".powod", "powod", p);
    if (kr) tekstReguly(z, k, b + ".krok", "krok", kr);
    if (l) tekstReguly(z, k, b + ".linia", "linia", l);
    for (auto& r : podpisy) tekstReguly(z, k, b + "." + r.first, r.first, r.second);
    z.katalog.push_back(k);
  };
  auto prog = [&](const char* id, double v) {
    reguly::Prog x;
    x.id = id;
    x.wartosc = v;
    x.tryb = "wszystkie";
    z.progi[id] = x;
  };
  prog("L-03.prog", 10);
  prog("E-20.wp95", 24);
  dodaj(wiersz("L-06", "b", StanReguly::AKTYWNA, Pilnosc::A, "R2"), "Saturacja: sprawa dla lekarza",
        "{min} min poniżej 88% w {n} z 3 ostatnich nocy.", "→ Skontaktuj się z lekarzem w ciągu kilku dni.", nullptr);
  reguly::WierszKatalogu l03 = wiersz("L-03", "10", StanReguly::AKTYWNA, Pilnosc::B, "R3");
  l03.parametry["prog"] = "L-03.prog";
  dodaj(l03, "Bezdechy utrzymują się wyżej", "Mediana z 14 nocy {x} / h, wyciek mały.",
        "→ Omów z lekarzem, pokaż raport z 2 tygodni.", nullptr,
        {{"podpis_lewo", "AHI · 14 nocy"}, {"podpis_prawo", "linia przerywana = 10 / h"}});
  dodaj(wiersz("L-11", "", StanReguly::AKTYWNA, Pilnosc::B, "R3"), "Senność mimo terapii",
        "Senność w {n} z 4 tygodni, urządzenie liczy {x} / h.", "→ Pokaż to lekarzowi, sama liczba nie wystarczy.",
        nullptr);
  dodaj(wiersz("L-08", "szybki", StanReguly::AKTYWNA, Pilnosc::B, "R3"), "Dużo mniej nocy z maską",
        "Ponad 4 h w {n} z 30 nocy, miesiąc temu {m} z 30.", "→ Omów to z lekarzem w najbliższych tygodniach.",
        nullptr,
        {{"podpis_lewo", "pełna = noc ponad 4 h z maską"}, {"podpis_prawo", "pusta = krócej albo bez maski"},
         {"rzad_1", "miesiąc temu"}, {"rzad_2", "ostatnie 30 nocy"}});
  dodaj(wiersz("L-08", "", StanReguly::AKTYWNA, Pilnosc::C, "R3"), nullptr, nullptr, nullptr,
        "Regularność niższa od 4 tygodni. Na kontroli: raport 2 / 3.");
  reguly::WierszKatalogu l09 = wiersz("L-09", "", StanReguly::AKTYWNA, Pilnosc::BRAK, "R5c");
  l09.parametry["prog_linii"] = "E-20.wp95";
  dodaj(l09, "Wyciek nie ustępuje", "Duży wyciek w {n} z 14 nocy. Bezdechy mniej pewne.",
        "→ Zapytaj dostawcę sprzętu o inną maskę.", nullptr,
        {{"podpis_lewo", "wyciek l/min · 14 nocy"}, {"podpis_prawo", "linia przerywana = 24 l/min"}});
  dodaj(wiersz("L-01", "", StanReguly::TYLKO_RAPORT, Pilnosc::B, "R3"), "Bezdechy centralne utrwalone",
        "Centralne średnio {x} / h z 7 nocy, zwykle {norma}.", "→ Omów to z lekarzem w najbliższych tygodniach.",
        nullptr);
  dodaj(wiersz("L-02", "a", StanReguly::TYLKO_RAPORT, Pilnosc::B, "R3"), "Urządzenie oznaczyło wzorzec",
        "Oddychanie okresowe w {n} z 14 nocy, razem {min} min.", "→ Pokaż to lekarzowi w najbliższych tygodniach.",
        nullptr);
  dodaj(wiersz("L-04", "", StanReguly::TYLKO_RAPORT, Pilnosc::B, "R3"), "EPAP przy górnej granicy",
        "EPAP blisko maksimum {max}, bezdechy obturacyjne {y} / h.", "→ Omów z lekarzem, pokaż raport z 2 tygodni.",
        nullptr);
  dodaj(wiersz("L-07", "", StanReguly::TYLKO_RAPORT, Pilnosc::C, "R3"), nullptr, nullptr, nullptr,
        "Ciśnienie rośnie od 2 tygodni. Na kontroli: raport 2 / 3.");
  dodaj(wiersz("L-06", "a", StanReguly::ZABLOKOWANA, Pilnosc::B, "R3"), "Saturacja: sprawa dla lekarza",
        "{min} min poniżej 88% w {n} z 7 nocy.", "→ Omów to z lekarzem w najbliższych tygodniach.", nullptr);
  return z;
}

// Wynik jak z silnika: stan i pilność z katalogu (silnik reguł przepisuje je z wiersza), zmienne do tekstów.
inline reguly::Wynik wynik(const reguly::Zestaw& z, const std::string& id, const std::string& wariant, bool oceniona,
                           bool spelniona, std::map<std::string, std::string> zm = {}) {
  reguly::Wynik w;
  w.id = id;
  w.wariant = wariant;
  w.oceniona = oceniona;
  w.spelniona = spelniona;
  if (!oceniona) w.powodNieoceny = "za malo nocy 9/14";
  for (auto& k : z.katalog)
    if (k.id == id && k.wariant == wariant) {
      w.stan = k.stan;
      w.pilnosc = k.pilnosc;
      w.grupa = k.grupa;
      w.poziom = k.poziom;
    }
  w.zmienne = zm;
  return w;
}

// Podstawienie {zmienna} ze zmiennych wyniku (atrapa reguly::tekst; bez {prog:…}).
inline std::string tekst(const reguly::Zestaw& z, const std::string& id, const reguly::Wynik& w) {
  auto it = z.teksty.find(id);
  if (it == z.teksty.end()) return "";
  std::string t = it->second.tekst;
  for (auto& kv : w.zmienne) {
    std::string k = "{" + kv.first + "}";
    for (size_t p = t.find(k); p != std::string::npos; p = t.find(k, p + kv.second.size())) t.replace(p, k.size(), kv.second);
  }
  return t;
}

inline sprawy::FunkcjaTekstu funkcja(const reguly::Zestaw& z) {
  return [&z](const std::string& id, const reguly::Wynik& w) { return tekst(z, id, w); };
}

}  // namespace atrapa_b5
