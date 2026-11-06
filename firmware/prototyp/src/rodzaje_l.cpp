// SPDX-License-Identifier: GPL-3.0-or-later
// Rodzaje warunków spoza rdzenia silnika, potrzebne wzorcom L (tabela rodzajów w spec technicznej 9.3).
// Każdy rodzaj to czysta funkcja wiersza katalogu i kontekstu (kontrakt src/reguly.h). Liczby wyłącznie z pliku
// progów przez `id_progu` w parametrach; wyjątek: stałe z definicji metryki M-19 w 4.3 („ostatnie d dni,
// d = min(30, dni od pierwszej nocy)”) i „30 dni wcześniej” z definicji wariantu szybkiego L-08 (5.12.4).
//
// Parametry spoza kontraktu 6:
//   blisko_ustawienia, wartosc_ustawienia: `ustawienie` (tag M-21 z pól rekordu nocy), `margines` (id_progu, cmH2O),
//                      `z_rodzajem` (`tak`: tylko noce z rodzajem bezdechu, M-03oa ≠ brak; 4.11, L-04);
//   regularnosc:       `noc` (id_progu: noc liczona, gdy M-01 ≥ tyle), `odniesienie` (`miesiac_wczesniej`),
//                      `bez_przerwy` (`tak`: bez nocy przerwy planowanej, R-SKI-35);
//   odpowiedzi_kolejne: `w_ostatnich` (id_progu: liczba ostatnich tygodni), `n` jak w kontrakcie (liczba kolejnych
//                      tygodni z odpowiedzią).
// Opakowania rodzajów rdzenia, których rdzeń silnika nie ma w potrzebnej postaci:
//   statystyka_okna_min: jak `statystyka_okna` (próg M-01 w godzinach przeliczony na minuty), ale okno = ostatnie `n` nocy z rekordem, z nich noce rodzaju
//                      `liczone_noce`, co najmniej `min` takich nocy (L-03: „z 14 nocy … przy ≥ 10 takich nocach”,
//                      T-TEST-163); mniej → „nie oceniam”;
//   odpowiedzi_najwyzej: jak `odpowiedzi_tygodni` (tydzień 0 = 7 dni do `dzis`, ostatnia odpowiedź tygodnia), ale
//                      spełniona, gdy odpowiedź `odpowiedz` w najwyżej k z n tygodni (L-11 wyjście ≤ 1 z 4); przy
//                      mniej niż `min_odpowiedzi` tygodniach z odpowiedzią „nie oceniam”;
//   porownanie_przed_po: L-12, okno po ostatniej zmianie ustawień wobec okna przed nią, z przeliczeniem progu
//                      czasu z maską z godzin na minuty M-01 (rdzeń porównuje wprost);
//   zlozenie_l:        jak `zlozenie` (op, skladniki; składnik spoza trybu albo zablokowany wypada z `i` i `lub`,
//                      a `nie` na nim = „nie oceniam”, jak w rdzeniu), ale wypełnia zmienne tekstów (rdzeń ich nie
//                      wypełnia): bez parametrów kopiuje zmienne składników (najpierw spełnionych, bez nadpisywania);
//                      `kopiuj` = lista kluczy `+` (zmienne pierwszego spełnionego z nich); `zmienne` = lista `|` wpisów
//                      `zmienna:klucz.zmienna_zrodla` albo `zmienna:norma.M-xx` (mediana normy metryki, 4.4);
//                      `nieznane=falsz`: składnik „nie oceniam” jak niespełniony;
//   trwanie_tygodni:   jak `trwanie` z jednostką tygodnie (`min` w tygodniach; L-10): reguła spełniona teraz i od
//                      PamiecReguly::aktywnaOd minęło co najmniej min × 7 dni.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>

#include "reguly.h"
#include "wspolne.h"
#include "wzorce.h"

namespace reguly {
std::string liczbaTekst(double v);  // src/reguly_pomoc.h (bez dołączania: nazwy pomocnicze jak parametr() kolidują)
}

namespace {

using reguly::Kontekst;
using reguly::WierszKatalogu;
using reguly::Wynik;

Wynik poczatek(const WierszKatalogu& r) {
  Wynik w;
  w.id = r.id;
  w.wariant = r.wariant;
  w.stan = r.stan;
  w.pilnosc = r.pilnosc;
  w.poziom = r.poziom;
  w.grupa = r.grupa;
  return w;
}

Wynik nieOceniam(Wynik w, const std::string& powod) {
  w.oceniona = false;
  w.spelniona = false;
  w.powodNieoceny = powod;
  return w;
}

std::string parametr(const WierszKatalogu& r, const char* nazwa) {
  auto it = r.parametry.find(nazwa);
  return it == r.parametry.end() ? "" : it->second;
}

const reguly::Prog* progZ(const Kontekst& k, const std::string& id) {
  if (!k.zestaw || id.empty()) return nullptr;
  return k.zestaw->prog(id, k.trybBiezacy);
}

// "k/n" z wiersza progu (tekst albo wartość); false, gdy nie da się odczytać.
bool kzn(const reguly::Prog* p, int& kk, int& nn) {
  if (!p) return false;
  const std::string& t = p->tekst;
  size_t uk = t.find('/');
  if (uk == std::string::npos) return false;
  kk = liczbaCalk(t.substr(0, uk).c_str());
  nn = liczbaCalk(t.substr(uk + 1).c_str());
  return kk > 0 && nn > 0 && kk <= nn;
}

// Wartość progu w jednostce metryki: M-01 jest w minutach (rekord nocy), progi czasu z maską bywają w godzinach
// (H-09.m01 = 6 h, L-12.m01 = −1 h, E-12.noc_min = 4 h).
double wJednostceMetryki(const reguly::Prog& p, const std::string& metryka) {
  if (metryka == "M-01" && p.jednostka == "h") return p.wartosc * 60.0;
  return p.wartosc;
}

bool porownaj(double a, double b, const std::string& por) {
  if (por == "gt") return a > b;
  if (por == "le") return a <= b;
  if (por == "lt") return a < b;
  return a >= b;  // ge, domyślnie
}

// Czas z maską jak M-01 na ekranie (4.3): `6 h 38`.
std::string czasMaski(double minuty) {
  if (std::isnan(minuty)) return "–";
  const long m = std::lround(minuty);
  char b[32];
  std::snprintf(b, sizeof b, "%ld h %02ld", m / 60, m % 60);
  return b;
}

// Jeden format liczb z rdzeniem (reguly_pomoc.h liczbaTekst: całkowita bez przecinka, inaczej jedno miejsce po
// przecinku, NAN = „–”).
std::string liczbaPl(double v) { return reguly::liczbaTekst(v); }

// Wartość tagu ustawień nocy (M-21, 4.11) z pól rekordu EAI, EAX, ANS, AXS (ASVAuto), MPI, MPA (AutoSet), IPC
// (CPAP), EPR, EEP, MNS, MXS (ASV). Rekord bez pola: zapas z tekstu Noc::ustawienia w układzie
// MOP/EAI/EAX/ANS/AXS (src/airbridge.cpp, tools/prototyp/historia.py). NAN = brak odczytu, reguła „nie oceniam”.
// Tylko odczyt stanu, nigdy wartość docelowa (ADR-0002).
double tagUstawien(const Noc& n, const std::string& tag) {
  struct Pole {
    const char* tag;
    float Noc::*pole;
  };
  static const Pole POLA[] = {{"EAI", &Noc::eai}, {"EAX", &Noc::eax}, {"ANS", &Noc::ans}, {"AXS", &Noc::axs},
                              {"EPR", &Noc::epr}, {"MPI", &Noc::mpi}, {"MPA", &Noc::mpa}, {"IPC", &Noc::ipc},
                              {"EEP", &Noc::eep}, {"MNS", &Noc::mns}, {"MXS", &Noc::mxs}};
  for (const Pole& p : POLA)
    if (tag == p.tag && !std::isnan(n.*(p.pole))) return static_cast<double>(n.*(p.pole));
  std::vector<std::string> f;
  std::string cur;
  for (char c : n.ustawienia) {
    if (c == '/') {
      f.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  f.push_back(cur);
  if (f.size() != 5) return NAN;
  static const char* const TAGI[] = {"EAI", "EAX", "ANS", "AXS"};
  for (size_t i = 0; i < 4; ++i)
    if (tag == TAGI[i]) {
      std::string v = f[i + 1];
      std::replace(v.begin(), v.end(), ',', '.');
      char* kon = nullptr;
      double x = std::strtod(v.c_str(), &kon);
      return (v.empty() || kon == v.c_str()) ? NAN : x;
    }
  return NAN;
}

// Ostatnia noc z rekordem do `dzis` włącznie.
const Noc* ostatnia(const Kontekst& k) {
  if (!k.historia) return nullptr;
  return ostatniaNoc(*k.historia, k.dzis);
}

// L-04 (ASVAuto), H-02: metryka nocy ≥ wartość tagu ustawień tej nocy − margines w ≥ k z n nocy (4.11, 5.12.4).
Wynik bliskoUstawienia(const WierszKatalogu& r, const Kontekst& k) {
  Wynik w = poczatek(r);
  const std::string met = parametr(r, "metryka"), tag = parametr(r, "ustawienie");
  const reguly::Prog* pm = progZ(k, parametr(r, "margines"));
  int kk = 0, nn = 0;
  if (met.empty() || tag.empty() || !pm || !kzn(progZ(k, parametr(r, "kzn")), kk, nn))
    return nieOceniam(w, "blad zestawu: parametr blisko_ustawienia");
  const bool zRodzajem = parametr(r, "z_rodzajem") == "tak";
  std::vector<const Noc*> okno = reguly::noceOkna(k, r.liczoneNoce, nn * 4);
  std::vector<const Noc*> noce;
  for (const Noc* n : okno)
    if (!zRodzajem || !std::isnan(n->oa)) noce.push_back(n);
  if (noce.size() > static_cast<size_t>(nn)) noce.erase(noce.begin(), noce.end() - nn);
  if (noce.size() < static_cast<size_t>(nn))
    return nieOceniam(w, "za malo nocy " + std::to_string(noce.size()) + "/" + std::to_string(nn));
  const double maks = tagUstawien(*noce.back(), tag);
  if (std::isnan(maks)) return nieOceniam(w, "brak ustawienia " + tag);
  int ile = 0;
  for (const Noc* n : noce) {
    const double m = reguly::metryka(*n, met), t = tagUstawien(*n, tag);
    if (!std::isnan(m) && !std::isnan(t) && m >= t - pm->wartosc) ++ile;
  }
  w.oceniona = true;
  w.spelniona = ile >= kk;
  w.zmienne["max"] = liczbaPl(maks);
  w.zmienne["n"] = std::to_string(ile);
  return w;
}

// H-02 (AutoSet): wartość tagu ustawień ostatniej nocy wobec progu, np. maksimum MPA ≥ 15 cmH2O (wariant „tryb
// terapii”, 5.12.9). Parametry: `ustawienie`, `prog`, `por`.
Wynik wartoscUstawienia(const WierszKatalogu& r, const Kontekst& k) {
  Wynik w = poczatek(r);
  const std::string tag = parametr(r, "ustawienie");
  const reguly::Prog* pp = progZ(k, parametr(r, "prog"));
  if (tag.empty() || !pp) return nieOceniam(w, "blad zestawu: parametr wartosc_ustawienia");
  const Noc* n = ostatnia(k);
  if (!n) return nieOceniam(w, "brak nocy");
  const double v = tagUstawien(*n, tag);
  if (std::isnan(v)) return nieOceniam(w, "brak ustawienia " + tag);
  w.oceniona = true;
  w.spelniona = porownaj(v, pp->wartosc, parametr(r, "por"));
  w.zmienne["max"] = liczbaPl(v);
  return w;
}

// M-19 (4.3): noce ≥ próg `noc` w ostatnich d dniach do `dzis` (albo 30 dni wcześniej), d = min(30, dni od pierwszej
// nocy z danymi); z `bez_przerwy=tak` bez nocy przerwy planowanej (R-SKI-35), d pomniejszone o te noce.
Wynik regularnosc(const WierszKatalogu& r, const Kontekst& k) {
  Wynik w = poczatek(r);
  const reguly::Prog* pn = progZ(k, parametr(r, "noc"));
  const reguly::Prog* pp = progZ(k, parametr(r, "prog"));
  if (!pn || !pp) return nieOceniam(w, "blad zestawu: parametr regularnosc");
  if (!k.historia || k.historia->empty()) return nieOceniam(w, "brak historii");
  const bool wczesniej = parametr(r, "odniesienie") == "miesiac_wczesniej";
  const std::string koniec = wczesniej ? dodajDni(k.dzis, -30) : k.dzis;
  std::string pierwsza;
  for (const Noc& n : *k.historia)
    if (pierwsza.empty() || n.data < pierwsza) pierwsza = n.data;
  if (pierwsza.empty() || pierwsza > koniec) return nieOceniam(w, "za malo dni historii");
  int d = std::min(30, dniMiedzy(pierwsza, koniec) + 1);
  // odniesienie „30 dni wcześniej” (L-08 szybki) tylko z pełnych 30 dni historii: krótsze to nie miesiąc
  if (wczesniej && d < 30) return nieOceniam(w, "odniesienie bez 30 dni historii");
  const double nocMin = pn->jednostka == "h" ? pn->wartosc * 60.0 : pn->wartosc;

  // noce przerwy planowanej z odpowiedzią „tak” (Odpowiedzi::przerwaPlanowana = daty nocy przerwy, jak w
  // src/rodzaje_e.cpp), R-SKI-35
  std::set<std::string> przerwa;
  if (parametr(r, "bez_przerwy") == "tak" && k.odpowiedzi) przerwa = k.odpowiedzi->przerwaPlanowana;
  int ile = 0, wyl = 0;
  for (int i = 0; i < d; ++i) {
    const std::string dz = dodajDni(koniec, -i);
    if (przerwa.count(dz)) {
      ++wyl;
      continue;
    }
    const Noc* n = nocZDnia(*k.historia, dz);
    if (n && n->m01 >= 0 && n->m01 >= nocMin) ++ile;
  }
  const int mian = d - wyl;
  if (mian <= 0) return nieOceniam(w, "brak dni poza przerwa");
  const double proc = 100.0 * ile / mian;
  w.oceniona = true;
  w.spelniona = porownaj(proc, pp->wartosc, parametr(r, "por"));
  w.zmienne[wczesniej ? "m" : "n"] = std::to_string(ile);
  w.zmienne["wartosc"] = liczbaPl(proc);
  return w;
}

// L-11 wariant „2 tygodnie” (R-SKI-29): odpowiedź `odpowiedz` w n kolejnych tygodniach z odpowiedzią (tygodnie
// z „brakiem odpowiedzi” pomijane, R-SKI-17), wszystkie w ostatnich `w_ostatnich` tygodniach.
Wynik odpowiedziKolejne(const WierszKatalogu& r, const Kontekst& k) {
  Wynik w = poczatek(r);
  const reguly::Prog* pn = progZ(k, parametr(r, "n"));
  const reguly::Prog* po = progZ(k, parametr(r, "w_ostatnich"));
  const std::string pyt = parametr(r, "pytanie");
  if (!pn || !po || (pyt != "sennosc" && pyt != "gorzej")) return nieOceniam(w, "blad zestawu: odpowiedzi_kolejne");
  if (!k.odpowiedzi) return nieOceniam(w, "brak odpowiedzi");
  const bool szukane = parametr(r, "odpowiedz") != "nie";
  const auto& mapa = pyt == "sennosc" ? k.odpowiedzi->sennosc : k.odpowiedzi->gorzej;
  const int dni = static_cast<int>(po->wartosc) * 7;
  const int potrzeba = static_cast<int>(pn->wartosc);
  int seria = 0, najdluzsza = 0, tak = 0;
  for (const auto& o : mapa) {  // std::map: po dacie rosnąco
    const int wstecz = dniMiedzy(o.first, k.dzis);
    if (wstecz < 0 || wstecz >= dni) continue;
    if (o.second) ++tak;
    seria = (o.second == szukane) ? seria + 1 : 0;
    najdluzsza = std::max(najdluzsza, seria);
  }
  w.oceniona = true;
  w.spelniona = potrzeba > 0 && najdluzsza >= potrzeba;
  w.zmienne["n"] = std::to_string(tak);
  return w;
}

Wynik statystykaOknaMin(const WierszKatalogu& r, const Kontekst& k) {
  Wynik w = poczatek(r);
  const std::string met = parametr(r, "metryka");
  const reguly::Prog* pn = progZ(k, parametr(r, "n"));
  const reguly::Prog* pm = progZ(k, parametr(r, "min"));  // opcjonalny: bez niego co najmniej 1 noc
  const reguly::Prog* pp = progZ(k, parametr(r, "prog"));
  if (met.empty() || !pn || !pp || !pn->tekst.empty() || (pm && !pm->tekst.empty()))
    return nieOceniam(w, "blad zestawu: parametr statystyka_okna_min");
  const int n = static_cast<int>(pn->wartosc), mn = pm ? static_cast<int>(pm->wartosc) : 1;
  std::vector<const Noc*> rek = reguly::noceOkna(k, "z_rekordem", 0);
  if (rek.size() > static_cast<size_t>(n)) rek.erase(rek.begin(), rek.end() - n);
  std::set<const Noc*> rodzaj;
  for (const Noc* x : reguly::noceOkna(k, r.liczoneNoce, 0)) rodzaj.insert(x);
  std::vector<double> v;
  for (const Noc* x : rek)
    if (rodzaj.count(x)) {
      const double m = reguly::metryka(*x, met);
      if (!std::isnan(m)) v.push_back(m);
    }
  if (static_cast<int>(v.size()) < mn)
    return nieOceniam(w, "za malo nocy " + std::to_string(v.size()) + "/" + std::to_string(mn));
  double s = NAN;
  const std::string stat = parametr(r, "stat");
  if (stat.empty() || stat == "mediana") {
    s = reguly::mediana(v);
  } else if (stat == "srednia") {
    double suma = 0;
    for (double x : v) suma += x;
    s = suma / static_cast<double>(v.size());
  } else {
    return nieOceniam(w, "nieznana statystyka " + stat);
  }
  w.oceniona = true;
  w.spelniona = porownaj(s, wJednostceMetryki(*pp, met), parametr(r, "por"));
  w.zmienne["x"] = liczbaPl(s);
  w.zmienne["wartosc"] = liczbaPl(s);
  w.zmienne["n"] = std::to_string(v.size());
  if (met == "M-01") w.zmienne["czas"] = czasMaski(s);
  return w;
}

// L-12 (R-SKI-27): statystyka metryki z `okno` pierwszych nocy rodzaju `liczone_noce` od ostatniej zmiany ustawień
// (4.8; brak odczytu nie jest zmianą) wobec `okno` nocy tuż przed nią: (po − przed) {por} delta. Mniej nocy po
// albo przed → „nie oceniam”; zmiany nie było → niespełniona. Parametry: metryka, stat (mediana, srednia), n
// (id_progu, liczba nocy w każdym oknie), delta, por. Opakowanie zamiast `porownanie_okien`, bo tu próg czasu z maską jest w godzinach.
Wynik porownaniePrzedPo(const WierszKatalogu& r, const Kontekst& k) {
  Wynik w = poczatek(r);
  const std::string met = parametr(r, "metryka");
  const reguly::Prog* po = progZ(k, parametr(r, "n"));
  const reguly::Prog* pd = progZ(k, parametr(r, "delta"));
  if (met.empty() || !po || !pd || !po->tekst.empty() || !pd->tekst.empty())
    return nieOceniam(w, "blad zestawu: porownanie_przed_po");
  const int n = static_cast<int>(po->wartosc);
  std::vector<const Noc*> noce = reguly::noceOkna(k, r.liczoneNoce, 0);
  // ostatnia zmiana ustawień: noc, której ustawienia różnią się od ostatnich znanych wcześniej
  size_t zmiana = noce.size();
  std::string ost;
  for (size_t i = 0; i < noce.size(); ++i) {
    if (noce[i]->ustawienia.empty()) continue;
    if (!ost.empty() && noce[i]->ustawienia != ost) zmiana = i;
    ost = noce[i]->ustawienia;
  }
  if (zmiana == noce.size()) {
    w.oceniona = true;
    return w;
  }
  if (noce.size() - zmiana < static_cast<size_t>(n) || zmiana < static_cast<size_t>(n))
    return nieOceniam(w, "za malo nocy przed albo po zmianie ustawien");
  auto stat = [&](size_t od, size_t ile) {
    std::vector<double> v;
    for (size_t i = od; i < od + ile; ++i) {
      const double m = reguly::metryka(*noce[i], met);
      if (!std::isnan(m)) v.push_back(m);
    }
    if (v.empty()) return static_cast<double>(NAN);
    if (parametr(r, "stat") == "srednia") {
      double s = 0;
      for (double x : v) s += x;
      return s / static_cast<double>(v.size());
    }
    return reguly::mediana(v);
  };
  const double przed = stat(zmiana - static_cast<size_t>(n), static_cast<size_t>(n));
  const double po_ = stat(zmiana, static_cast<size_t>(n));
  if (std::isnan(przed) || std::isnan(po_)) return nieOceniam(w, "brak wartosci " + met);
  w.oceniona = true;
  w.spelniona = porownaj(po_ - przed, wJednostceMetryki(*pd, met), parametr(r, "por"));
  // {metryka} nazwą z 4.3, {x} i {y} w formacie metryki (L-12: `{metryka} {x}, przed zmianą {y} ({data}).`)
  w.zmienne["metryka"] = met == "M-01" ? "Czas na masce" : met == "M-07" ? "Wyciek" : met == "M-02" ? "AHI" : met;
  w.zmienne["x"] = met == "M-01" ? czasMaski(po_) : liczbaPl(po_);
  w.zmienne["y"] = met == "M-01" ? czasMaski(przed) : liczbaPl(przed);
  w.zmienne["data"] = noce[zmiana]->data;
  return w;
}

Wynik odpowiedziNajwyzej(const WierszKatalogu& r, const Kontekst& k) {
  Wynik w = poczatek(r);
  int kk = 0, nn = 0;
  const std::string pyt = parametr(r, "pytanie");
  if ((pyt != "sennosc" && pyt != "gorzej") || !kzn(progZ(k, parametr(r, "kzn")), kk, nn))
    return nieOceniam(w, "blad zestawu: odpowiedzi_najwyzej");
  if (!k.odpowiedzi) return nieOceniam(w, "brak odpowiedzi");
  const bool szukane = parametr(r, "odpowiedz") != "nie";
  const auto& mapa = pyt == "sennosc" ? k.odpowiedzi->sennosc : k.odpowiedzi->gorzej;
  std::vector<int> tydzien(static_cast<size_t>(nn), -1);  // -1 brak, 0 nie, 1 tak (ostatnia odpowiedź tygodnia)
  for (const auto& o : mapa) {
    const int wstecz = dniMiedzy(o.first, k.dzis);
    if (wstecz < 0 || wstecz >= nn * 7) continue;
    tydzien[static_cast<size_t>(wstecz / 7)] = o.second ? 1 : 0;  // mapa po dacie rosnąco: późniejsza nadpisuje
  }
  int ile = 0, zOdp = 0;
  for (int t : tydzien) {
    if (t >= 0) ++zOdp;
    if (t >= 0 && (t == 1) == szukane) ++ile;
  }
  // tydzień bez odpowiedzi to „nie wiem”, nie „nie” (R-SKI-17): wyjście tylko przy co najmniej `min_odpowiedzi`
  // tygodniach z odpowiedzią, inaczej „nie oceniam”
  const reguly::Prog* pmin = progZ(k, parametr(r, "min_odpowiedzi"));
  if (pmin && zOdp < static_cast<int>(pmin->wartosc))
    return nieOceniam(w, "za malo tygodni z odpowiedzia " + std::to_string(zOdp) + "/" + std::to_string(nn));
  w.oceniona = true;
  w.spelniona = ile <= kk;
  w.zmienne["n"] = std::to_string(ile);
  return w;
}

Wynik trwanieTygodni(const WierszKatalogu& r, const Kontekst& k) {
  Wynik w = poczatek(r);
  const std::string reg = parametr(r, "regula");
  const reguly::Prog* pm = progZ(k, parametr(r, "min"));
  if (reg.empty() || !pm || !pm->tekst.empty()) return nieOceniam(w, "blad zestawu: trwanie_tygodni");
  if (!k.wczesniejsze) return nieOceniam(w, "brak wynikow wczesniejszych");
  auto it = k.wczesniejsze->find(reg);
  if (it == k.wczesniejsze->end()) return nieOceniam(w, "brak reguly " + reg + " przed ta regula");
  if (!it->second.oceniona) return nieOceniam(w, it->second.powodNieoceny);
  if (!k.pamiec) return nieOceniam(w, "brak pamieci regul");
  w.oceniona = true;
  if (!it->second.spelniona) return w;
  auto p = k.pamiec->find(reg);
  std::string od = (p != k.pamiec->end() && dataOk(p->second.aktywnaOd)) ? p->second.aktywnaOd : k.dzis;
  if (od > k.dzis) od = k.dzis;
  const int dni = dniMiedzy(od, k.dzis) + 1;
  w.spelniona = dni >= static_cast<int>(pm->wartosc) * 7;
  w.zmienne["n"] = std::to_string(dni / 7);
  w.zmienne["od"] = od;
  return w;
}

const NormaMetryki* normaMetryki(const Norma& n, const std::string& m) {
  if (m == "M-01") return &n.m01;
  if (m == "M-02") return &n.ahi;
  if (m == "M-03ca") return &n.ca;
  if (m == "M-03oa") return &n.oa;
  if (m == "M-05p") return &n.cp95;
  if (m == "M-06p") return &n.ep95;
  if (m == "M-07") return &n.wp95;
  if (m == "M-09p") return &n.csrp;
  if (m == "M-13") return &n.m13;
  return nullptr;
}

std::vector<std::string> dzielZnak(const std::string& s, char sep) {
  std::vector<std::string> v;
  std::string cur;
  for (char c : s) {
    if (c == sep) {
      if (!cur.empty()) v.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) v.push_back(cur);
  return v;
}

// Wynik składnika z wcześniej ocenionych (klucz `id` albo `id/wariant`); nullptr = brak.
const Wynik* skladnik(const Kontekst& k, const std::string& kl) {
  if (!k.wczesniejsze) return nullptr;
  auto it = k.wczesniejsze->find(kl);
  return it == k.wczesniejsze->end() ? nullptr : &it->second;
}

Wynik zlozenieL(const WierszKatalogu& r, const Kontekst& k) {
  Wynik w = poczatek(r);
  const std::string op = parametr(r, "op");
  std::vector<std::string> s = dzielZnak(parametr(r, "skladniki"), '+');
  if (s.empty() || (op != "i" && op != "lub" && op != "nie") || (op == "nie" && s.size() != 1))
    return nieOceniam(w, "blad zestawu: zlozenie_l");
  std::vector<const Wynik*> sk;
  for (const auto& x : s) {
    const Wynik* y = skladnik(k, x);
    if (!y) return nieOceniam(w, "brak skladnika " + x + " przed ta regula");
    const bool pominiety = y->zmienne.count("pominieta") > 0;
    if (pominiety && op == "nie") return nieOceniam(w, x + ": nie oceniam");
    if (!pominiety) sk.push_back(y);
  }
  if (sk.empty()) return nieOceniam(w, "wszystkie skladniki zablokowane albo spoza trybu");
  bool prawda = false, falsz = false;
  std::string nieznany;
  // `nieznane=falsz`: składnik „nie oceniam” liczy się jak niespełniony (E-12 bez 14 dni historii jest nieaktywne,
  // więc L-03 (≥ 10) działa w rozruchu, R-SKI-04)
  const bool nieznaneFalsz = parametr(r, "nieznane") == "falsz";
  for (const Wynik* y : sk) {
    if (!y->oceniona && !nieznaneFalsz) {
      if (nieznany.empty()) nieznany = (y->wariant.empty() ? y->id : y->id + "/" + y->wariant) + ": nie oceniam";
    } else if (y->oceniona && y->spelniona) {
      prawda = true;
    } else {
      falsz = true;  // niespełniona albo „nie oceniam” przy `nieznane=falsz`
    }
  }
  if (op == "nie") {
    if (!nieznany.empty()) return nieOceniam(w, nieznany);
    w.oceniona = true;
    w.spelniona = !prawda;
  } else if (op == "i") {
    if (falsz) {
      w.oceniona = true;
    } else if (!nieznany.empty()) {
      return nieOceniam(w, nieznany);
    } else {
      w.oceniona = true;
      w.spelniona = true;
    }
  } else {
    if (prawda) {
      w.oceniona = w.spelniona = true;
    } else if (!nieznany.empty()) {
      return nieOceniam(w, nieznany);
    } else {
      w.oceniona = true;
    }
  }
  // zmienne tekstów
  auto kopiujZ = [&](const Wynik& y) {
    for (const auto& kv : y.zmienne)
      if (kv.first != "pominieta" && !w.zmienne.count(kv.first)) w.zmienne[kv.first] = kv.second;
  };
  const std::string kop = parametr(r, "kopiuj");
  if (!kop.empty()) {
    const Wynik* pierwszy = nullptr;
    for (const auto& x : dzielZnak(kop, '+')) {
      const Wynik* y = skladnik(k, x);
      if (y && y->oceniona && y->spelniona && !pierwszy) pierwszy = y;
    }
    if (pierwszy) kopiujZ(*pierwszy);
  }
  for (const auto& wpis : dzielZnak(parametr(r, "zmienne"), '|')) {
    const size_t dw = wpis.find(':');
    if (dw == std::string::npos) return nieOceniam(w, "blad zestawu: zmienne " + wpis);
    const std::string nazwa = wpis.substr(0, dw), zrodlo = wpis.substr(dw + 1);
    if (zrodlo.compare(0, 6, "norma.") == 0) {
      const NormaMetryki* nm = k.norma ? normaMetryki(*k.norma, zrodlo.substr(6)) : nullptr;
      w.zmienne[nazwa] = (nm && nm->jest()) ? liczbaPl(nm->med) : "–";  // bez normy „–” (jak R-AW-09)
      continue;
    }
    const size_t kr = zrodlo.rfind('.');
    if (kr == std::string::npos) return nieOceniam(w, "blad zestawu: zmienne " + wpis);
    const Wynik* y = skladnik(k, zrodlo.substr(0, kr));
    if (!y) continue;
    auto it = y->zmienne.find(zrodlo.substr(kr + 1));
    if (it != y->zmienne.end()) w.zmienne[nazwa] = it->second;
  }
  if (kop.empty() && parametr(r, "zmienne").empty()) {
    for (const Wynik* y : sk)
      if (y->oceniona && y->spelniona) kopiujZ(*y);
    for (const Wynik* y : sk) kopiujZ(*y);
  }
  return w;
}

}  // namespace

namespace wzorce {
void zarejestrujRodzajeL() {
  static bool zrobione = false;
  if (zrobione) return;
  zrobione = true;
  reguly::zarejestrujRodzaj("blisko_ustawienia", bliskoUstawienia);
  reguly::zarejestrujRodzaj("wartosc_ustawienia", wartoscUstawienia);
  reguly::zarejestrujRodzaj("regularnosc", regularnosc);
  reguly::zarejestrujRodzaj("odpowiedzi_kolejne", odpowiedziKolejne);
  reguly::zarejestrujRodzaj("statystyka_okna_min", statystykaOknaMin);
  reguly::zarejestrujRodzaj("odpowiedzi_najwyzej", odpowiedziNajwyzej);
  reguly::zarejestrujRodzaj("trwanie_tygodni", trwanieTygodni);
  reguly::zarejestrujRodzaj("porownanie_przed_po", porownaniePrzedPo);
  reguly::zarejestrujRodzaj("zlozenie_l", zlozenieL);
}
}  // namespace wzorce
