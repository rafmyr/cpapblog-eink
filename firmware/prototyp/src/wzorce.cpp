// SPDX-License-Identifier: GPL-3.0-or-later
// Stany wzorca L, „wiem”, „Omówione z lekarzem”, hipoteza i „Otwarte sprawy”. Opis i konwencje: wzorce.h.
#include "wzorce.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "sprawy.h"
#include "wspolne.h"

namespace wzorce {
namespace {

using reguly::Pilnosc;
using reguly::StanReguly;
using reguly::Wynik;

int ranga(char p) {
  switch (p) {
    case 'A': return 4;
    case 'B': return 3;
    case 'P': return 2;
    case 'C': return 1;
    case 'R': return 0;
    default: return -1;
  }
}

// Pilność wiersza do zapisu: „tylko raport” = R (bez pilności), AKTYWNA bez pilności = P (L-09, ekran „problem”).
char znakPilnosci(const Wynik& w) {
  if (w.stan == StanReguly::TYLKO_RAPORT) return 'R';
  switch (w.pilnosc) {
    case Pilnosc::A: return 'A';
    case Pilnosc::B: return 'B';
    case Pilnosc::C: return 'C';
    case Pilnosc::BRAK: return 'P';
  }
  return 'P';
}

Pilnosc pilnoscZeZnaku(char p) {
  switch (p) {
    case 'A': return Pilnosc::A;
    case 'B': return Pilnosc::B;
    case 'C': return Pilnosc::C;
    default: return Pilnosc::BRAK;
  }
}

// Wartość progu z zestawu (pierwszy wiersz o tym id); NAN, gdy brak zestawu albo progu.
double prog(const reguly::Zestaw* z, const std::string& id) {
  if (!z) return NAN;
  auto it = z->progi.find(id);
  if (it != z->progi.end()) return it->second.wartosc;
  for (const auto& p : z->progi)
    if (p.second.id == id) return p.second.wartosc;
  return NAN;
}

double liczba(const std::string& t) {
  if (t.empty()) return NAN;
  std::string s = t;
  std::replace(s.begin(), s.end(), ',', '.');
  char* kon = nullptr;
  double v = std::strtod(s.c_str(), &kon);
  return (kon == s.c_str()) ? NAN : v;
}

// Parametr kluczowy wiersza głównego: zmienna {x} (teksty 5.12.5), zapasowo {wartosc} (kontrakt 1).
double wartoscX(const Wynik& w) {
  auto it = w.zmienne.find("x");
  if (it == w.zmienne.end()) it = w.zmienne.find("wartosc");
  return it == w.zmienne.end() ? NAN : liczba(it->second);
}

// Δ powrotu L (5.12.4, „Pogorszenie o kolejne Δ”) dla wzorców, których parametr kluczowy ma Δ w spec; reszta bez Δ.
std::string progDelty(const std::string& id, const std::string& galaz) {
  if (id == "L-01") return "R-LEK-10.delta_ca";
  if (id == "L-02" && galaz == "b") return "R-LEK-10.delta_csr";
  if (id == "L-03") return "R-LEK-10.delta_ahi";
  if (id == "L-06") return "R-LEK-10.delta_t88";
  // pozostałe metryki: Δ z tabeli 4.5 (5.12.4, „dla pozostałych metryk Δ z tabeli 4.5”)
  if (id == "L-05") return "R-NOR-12.delta.M-13";
  if (id == "L-07") return "R-NOR-12.delta.M-06p";  // M-05p w CPAP i AutoSet ma ten sam Δ (4.5)
  if (id == "L-09") return "R-NOR-12.delta.M-07";
  return "";
}

struct Grupa {
  std::string klucz, id, galaz;
  std::vector<const Wynik*> glowne;
  const Wynik* wyjscie = nullptr;
};

std::vector<Grupa> grupy(const std::vector<Wynik>& wyniki) {
  std::vector<Grupa> g;
  std::map<std::string, size_t> poz;
  for (const auto& w : wyniki) {
    if (!wierszL(w) || wierszPomocniczy(w) || w.stan == StanReguly::ZABLOKOWANA) continue;
    std::string k = kluczWzorca(wyniki, w);
    auto it = poz.find(k);
    if (it == poz.end()) {
      Grupa n;
      n.klucz = k;
      n.id = w.id;
      size_t uk = k.find('/');
      n.galaz = uk == std::string::npos ? "" : k.substr(uk + 1);
      poz[k] = g.size();
      g.push_back(n);
      it = poz.find(k);
    }
    g[it->second].glowne.push_back(&w);
  }
  for (auto& gr : g) {
    std::string wariantWyjscia = gr.galaz + ".wyjscie";
    for (const auto& w : wyniki)
      if (w.id == gr.id && w.wariant == wariantWyjscia) gr.wyjscie = &w;
  }
  return g;
}

bool spelnionyPomocniczy(const std::vector<Wynik>& wyniki, const std::string& id, const std::string& wariant) {
  for (const auto& w : wyniki)
    if (w.id == id && w.wariant == wariant) return w.oceniona && w.spelniona;
  return false;
}

StanWzorca stanZNazwy(const std::string& n) {
  static const StanWzorca wszystkie[] = {StanWzorca::AKTYWNY, StanWzorca::WIEM, StanWzorca::OMOWIONE,
                                         StanWzorca::WYGASZONY_B, StanWzorca::WSTRZYMANY};
  for (StanWzorca s : wszystkie)
    if (n == nazwaStanu(s)) return s;
  return StanWzorca::AKTYWNY;
}

bool wyciszony(StanWzorca s) {
  return s == StanWzorca::WIEM || s == StanWzorca::OMOWIONE || s == StanWzorca::WYGASZONY_B;
}

// Czy po dacie `od` (do `dzis` włącznie) ustawienia nocy różnią się od ostatnich znanych z dnia `od` albo wcześniej
// (4.8; brak odczytu ustawień nie jest zmianą, R-NOR-15).
bool zmianaUstawienPo(const std::vector<Noc>* hist, const std::string& od, const std::string& dzis) {
  if (!hist || od.empty()) return false;
  std::vector<const Noc*> n;
  for (const auto& x : *hist) n.push_back(&x);
  std::sort(n.begin(), n.end(), [](const Noc* a, const Noc* b) { return a->data < b->data; });
  std::string ostatnie;
  for (const Noc* x : n) {
    if (x->data > dzis) break;
    if (x->ustawienia.empty()) continue;
    if (x->data > od && !ostatnie.empty() && x->ustawienia != ostatnie) return true;
    ostatnie = x->ustawienia;
  }
  return false;
}

}  // namespace

const char* nazwaStanu(StanWzorca s) {
  switch (s) {
    case StanWzorca::NIEAKTYWNY: return "nieaktywny";
    case StanWzorca::AKTYWNY: return "aktywny";
    case StanWzorca::KARENCJA: return "karencja";
    case StanWzorca::WIEM: return "wiem";
    case StanWzorca::OMOWIONE: return "omowione";
    case StanWzorca::WYGASZONY_B: return "wygaszony_b";
    case StanWzorca::WSTRZYMANY: return "wstrzymany";
  }
  return "nieaktywny";
}

ZapisWzorca odczytajZapis(const std::string& pole) {
  ZapisWzorca z;
  std::vector<std::string> f;
  std::string cur;
  for (char c : pole) {
    if (c == ',') {
      f.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  f.push_back(cur);
  if (f.size() != 15) return z;
  static const StanWzorca wszystkie[] = {StanWzorca::NIEAKTYWNY, StanWzorca::AKTYWNY,     StanWzorca::KARENCJA,
                                         StanWzorca::WIEM,       StanWzorca::OMOWIONE,    StanWzorca::WYGASZONY_B,
                                         StanWzorca::WSTRZYMANY};
  bool znany = false;
  for (StanWzorca s : wszystkie)
    if (f[0] == nazwaStanu(s)) {
      z.stan = s;
      znany = true;
    }
  if (!znany || f[1].size() != 1 || (f[1][0] != '-' && ranga(f[1][0]) < 0)) return ZapisWzorca{};
  z.pil = f[1][0];
  z.wejscie = f[2];
  z.odPilnosci = f[3];
  z.ref = liczba(f[4]);
  z.wyjscie = f[5];
  z.licznik = liczbaCalk(f[6].c_str());
  z.przed = f[7];
  z.biez = liczba(f[8]);
  z.potw = liczbaCalk(f[9].c_str());
  z.refPotw = liczba(f[10]);
  z.raport = f[11];
  z.wiemAnul = f[12];
  z.omowAnul = f[13];
  z.dzien = f[14];
  return z;
}

namespace {
std::string liczbaZapisu(double v) {
  if (std::isnan(v)) return "";
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", v);
  return b;
}
}  // namespace

std::string zapiszZapis(const ZapisWzorca& z) {
  return std::string(nazwaStanu(z.stan)) + "," + std::string(1, z.pil) + "," + z.wejscie + "," + z.odPilnosci + "," +
         liczbaZapisu(z.ref) + "," + z.wyjscie + "," + std::to_string(z.licznik) + "," + z.przed + "," +
         liczbaZapisu(z.biez) + "," + std::to_string(z.potw) + "," + liczbaZapisu(z.refPotw) + "," + z.raport + "," + z.wiemAnul + "," + z.omowAnul + "," + z.dzien;
}

bool wierszL(const Wynik& w) { return w.id.size() > 2 && w.id.compare(0, 2, "L-") == 0; }
bool wierszPomocniczy(const Wynik& w) { return w.wariant.find('.') != std::string::npos; }

std::string kluczWzorca(const std::vector<Wynik>& wyniki, const Wynik& glowny) {
  if (glowny.wariant.empty()) return glowny.id;
  std::string wyj = glowny.wariant + ".wyjscie";
  for (const auto& w : wyniki)
    if (w.id == glowny.id && w.wariant == wyj) return glowny.id + "/" + glowny.wariant;
  return glowny.id;
}

void aktualizuj(const std::vector<Wynik>& wyniki, reguly::Pamiec& pamiec, const std::string& dzis,
                const reguly::Zestaw* z, const Okolicznosci& o) {
  for (const Grupa& g : grupy(wyniki)) {
    reguly::PamiecReguly& pr = pamiec[g.klucz];
    ZapisWzorca s = odczytajZapis(pr.stanWzorca);
    bool wej = false;
    char pilN = '-';
    const Wynik* gl = nullptr;
    for (const Wynik* w : g.glowne) {
      if (!(w->oceniona && w->spelniona)) continue;
      wej = true;
      char c = znakPilnosci(*w);
      if (ranga(c) > ranga(pilN)) {
        pilN = c;
        gl = w;
      }
    }
    if (!gl)
      for (const Wynik* w : g.glowne)
        if (w->oceniona && !gl) gl = w;
    const bool wyj = g.wyjscie && g.wyjscie->oceniona && g.wyjscie->spelniona;
    const double x = gl ? wartoscX(*gl) : NAN;
    if (!std::isnan(x)) s.biez = x;
    const bool tenSamDzien = s.dzien == dzis;  // drugi przebieg tej samej daty (restart, import): liczniki nocy bez zmian
    s.dzien = dzis;
    // „wiem” i „Omówione” zapisuje (src/sprawy.h, pole `RRRR-MM-DD@P`); tu tylko odczyt daty
    const std::string wiemD = sprawy::dataWyciszenia(pr.wiem), omowD = sprawy::dataWyciszenia(pr.omowione);

    bool weszloDzis = false;
    if (s.stan == StanWzorca::NIEAKTYWNY) {
      if (!wej) {
        pr.stanWzorca = zapiszZapis(s);
        continue;
      }
      weszloDzis = true;
      const double ponowne = prog(z, "L-01.ponowne_wejscie");
      const bool powrot = g.id == "L-01" && !s.przed.empty() && !s.wyjscie.empty() && !std::isnan(ponowne) &&
                          dniMiedzy(s.wyjscie, dzis) <= static_cast<int>(ponowne);
      if (powrot) {
        // R-SKI-31: stan sprzed wyjścia z pilnością, karencją, datą wejścia i wyciszeniem z pierwotną datą
        s.stan = stanZNazwy(s.przed);
        if (s.przed == "karencja") s.stan = StanWzorca::KARENCJA;
      } else {
        // nowy wzorzec (fail-path R-SKI-03): bez wyciszeń sprzed, licznik potwierdzeń od zera
        const std::string gal = gl ? gl->wariant : "";
        s.stan = spelnionyPomocniczy(wyniki, g.id, gal + ".karencja") ? StanWzorca::KARENCJA : StanWzorca::AKTYWNY;
        s.licznik = s.stan == StanWzorca::KARENCJA ? 1 : 0;
        s.pil = pilN;
        s.wejscie = dzis;
        s.odPilnosci = dzis;
        s.ref = x;
        s.potw = 0;
        s.refPotw = NAN;
        s.raport.clear();
        s.wiemAnul = wiemD;
        s.omowAnul = omowD;
      }
      s.przed.clear();
    }

    // wyjście (5.12.12: wejście i wyjście w nocy wejścia → wejście wygrywa, (c))
    if (wyj && !weszloDzis && s.wejscie != dzis) {
      if (g.id == "L-01") {
        // R-SKI-31: stan sprzed wyjścia zostaje na L-01.ponowne_wejscie nocy (wyciszenia i liczniki bez kasowania)
        s.przed = nazwaStanu(s.stan);
      } else {
        s.pil = '-';
        s.ref = NAN;
        s.potw = 0;
        s.refPotw = NAN;
        s.raport.clear();
        s.przed.clear();
        s.wiemAnul = wiemD;  // wyciszenia znikają razem z wzorcem (fail-path R-SKI-03)
        s.omowAnul = omowD;
      }
      s.stan = StanWzorca::NIEAKTYWNY;
      s.wyjscie = dzis;
      pr.stanWzorca = zapiszZapis(s);
      continue;
    }

    // karencja L-01 (R-SKI-08): licznik nocy trwania warunku liczy noce (daty) ze spełnionym warunkiem
    if (s.stan == StanWzorca::KARENCJA) {
      const std::string gal = gl ? gl->wariant : "";
      if (!weszloDzis && wej && !tenSamDzien) ++s.licznik;
      const double noc = prog(z, "L-01.karencja_noc");
      if (spelnionyPomocniczy(wyniki, g.id, gal + ".karencja_ca") ||
          (!std::isnan(noc) && s.licznik >= static_cast<int>(noc))) {
        s.stan = StanWzorca::AKTYWNY;
        s.odPilnosci = dzis;
      }
      pr.stanWzorca = zapiszZapis(s);
      continue;
    }

    // zmiana pilności
    if (wej && pilN != s.pil && ranga(pilN) >= 0) {
      const bool wzrost = ranga(pilN) > ranga(s.pil);
      const char stara = s.pil;
      s.pil = pilN;
      s.odPilnosci = dzis;
      if (wzrost && wyciszony(s.stan)) {  // R-LEK-10, R-EKR-23: wzrost przerywa wyciszenie
        s.stan = StanWzorca::AKTYWNY;
        s.wiemAnul = wiemD;
        s.omowAnul = omowD;
      }
      if (!wzrost && stara == 'A' && s.stan == StanWzorca::OMOWIONE) {  // f
        s.stan = StanWzorca::AKTYWNY;
        s.omowAnul = omowD;
      }
      if (stara == 'A' || pilN == 'A') {  // licznik potwierdzeń liczy się od wejścia w A (R-LEK-26)
        s.potw = 0;
        s.refPotw = NAN;
      }
    }

    // pogorszenie o kolejne Δ (R-LEK-10, R-EKR-23, fail-path R-SKI-09)
    const double d = prog(z, progDelty(g.id, g.galaz));
    if (wyciszony(s.stan) && !std::isnan(d) && !std::isnan(s.ref) && !std::isnan(x) && x >= s.ref + d) {
      s.stan = StanWzorca::AKTYWNY;
      s.odPilnosci = dzis;
      s.wiemAnul = wiemD;
      s.omowAnul = omowD;
    }

    // potwierdzenia z zapisu późniejsze z „wiem” i „Omówione”, jeśli nie skasowane w tym wzorcu
    const bool wiemW = !wiemD.empty() && wiemD != s.wiemAnul;
    const bool omowW = !omowD.empty() && omowD != s.omowAnul;
    if (s.stan == StanWzorca::AKTYWNY || s.stan == StanWzorca::WIEM || s.stan == StanWzorca::OMOWIONE) {
      if (omowW && (!wiemW || omowD >= wiemD) && (s.pil == 'A' || s.pil == 'B'))
        s.stan = StanWzorca::OMOWIONE;
      else if (wiemW && (s.pil == 'A' || s.pil == 'B' || s.pil == 'P'))
        s.stan = StanWzorca::WIEM;
      else
        s.stan = StanWzorca::AKTYWNY;  // pole skasowane przez (np. cofnięcie „wiem”, R-PRZ-16)
    }

    // koniec wyciszeń i wygaszenie B
    if (s.stan == StanWzorca::WIEM) {
      const double okres = prog(z, s.pil == 'A' ? "R-PRZ-03.wiem_a" : "R-PRZ-03.wiem_problem");
      if (!std::isnan(okres) && dniMiedzy(wiemD, dzis) >= static_cast<int>(okres)) {
        s.stan = StanWzorca::AKTYWNY;
        s.wiemAnul = wiemD;
      }
    } else if (s.stan == StanWzorca::OMOWIONE) {
      // A: po drugim bez poprawy 28 nocy (R-LEK-25); po trzecim i kolejnym bez poprawy wyciszenie trwa, a pełny
      // ekran co 14 nocy (R-LEK-27, pelnyEkranDzisA()); inaczej 56 nocy (R-LEK-09)
      const bool coCzternascie = s.pil == 'A' && s.potw >= 3;
      const double okres = coCzternascie ? NAN
                           : (s.pil == 'A' && s.potw == 2) ? prog(z, "R-LEK-25.drugie")
                                                           : prog(z, "R-LEK-09.wyciszenie");
      const bool minelo = !std::isnan(okres) && dniMiedzy(omowD, dzis) >= static_cast<int>(okres);
      if (minelo || zmianaUstawienPo(o.historia, omowD, dzis)) {  // R-SKI-11, R-LEK-09
        s.stan = StanWzorca::AKTYWNY;
        s.omowAnul = omowD;
      }
    }
    if (s.stan == StanWzorca::AKTYWNY && s.pil == 'B') {
      const double okres = prog(z, "R-SKI-09.b_dni");
      if (!std::isnan(okres) && dniMiedzy(s.odPilnosci, dzis) >= static_cast<int>(okres)) s.stan = StanWzorca::WYGASZONY_B;
    }

    // okres infekcji wstrzymuje L-03 i L-09 (R-INF-02), element po terminie L-09 (R-MAS-03, R-MAS-08, R-MAS-10)
    const bool wstrzymaj = (o.okresInfekcji && (g.id == "L-03" || g.id == "L-09")) || (o.l09Sprzet && g.id == "L-09");
    if (wstrzymaj && s.stan == StanWzorca::AKTYWNY) s.stan = StanWzorca::WSTRZYMANY;
    if (!wstrzymaj && s.stan == StanWzorca::WSTRZYMANY) s.stan = StanWzorca::AKTYWNY;

    if (s.stan == StanWzorca::AKTYWNY && !std::isnan(x)) s.ref = x;
    pr.stanWzorca = zapiszZapis(s);
  }
}

bool wiem(reguly::Pamiec& pamiec, const std::string& klucz, const std::string& dzis) {
  auto it = pamiec.find(klucz);
  if (it == pamiec.end()) return false;
  ZapisWzorca s = odczytajZapis(it->second.stanWzorca);
  if (s.stan != StanWzorca::AKTYWNY || (s.pil != 'A' && s.pil != 'B' && s.pil != 'P')) return false;
  sprawy::zapiszWiem(pamiec, klucz, dzis, pilnoscZeZnaku(s.pil));  // zapis:
  s.stan = StanWzorca::WIEM;
  s.wiemAnul.clear();
  it->second.stanWzorca = zapiszZapis(s);
  return true;
}

namespace {
// A po trzecim i kolejnym „Omówione” bez poprawy: pełny ekran co 14 nocy od nocy potwierdzenia (R-LEK-27)
bool pelnyEkranDzisA(const ZapisWzorca& s, const reguly::PamiecReguly& pr, const std::string& dzis,
                     const reguly::Zestaw* z) {
  const std::string od = sprawy::dataWyciszenia(pr.omowione);
  if (s.stan != StanWzorca::OMOWIONE || s.pil != 'A' || s.potw < 3 || od.empty()) return false;
  const double co = prog(z, "R-LEK-27.pelny_ekran");
  const int dni = dniMiedzy(od, dzis);
  return !std::isnan(co) && co >= 1 && dni > 0 && dni % static_cast<int>(co) == 0;
}

std::string idZKlucza(const std::string& klucz) { return klucz.substr(0, klucz.find('/')); }
std::string galazZKlucza(const std::string& klucz) {
  size_t uk = klucz.find('/');
  return uk == std::string::npos ? "" : klucz.substr(uk + 1);
}
}  // namespace

WariantOmowienia wariantOmowienia(const reguly::Pamiec& pamiec, const std::string& klucz, const std::string& dzis,
                                  const reguly::Zestaw* z) {
  auto it = pamiec.find(klucz);
  if (it == pamiec.end()) return WariantOmowienia::NIEDOSTEPNE;
  const ZapisWzorca s = odczytajZapis(it->second.stanWzorca);
  const bool ekranL = s.stan == StanWzorca::AKTYWNY || pelnyEkranDzisA(s, it->second, dzis, z);
  if (!ekranL) return WariantOmowienia::NIEDOSTEPNE;
  if (s.pil == 'B') return WariantOmowienia::ZWYKLE;
  if (s.pil != 'A') return WariantOmowienia::NIEDOSTEPNE;
  if (s.potw == 0) return WariantOmowienia::A_PIERWSZE;
  const double d = prog(z, progDelty(idZKlucza(klucz), galazZKlucza(klucz)));
  const bool poprawa = !std::isnan(d) && !std::isnan(s.biez) && !std::isnan(s.refPotw) && s.biez <= s.refPotw - d;
  if (poprawa) return WariantOmowienia::A_PIERWSZE;
  const bool nowyRaport = !s.raport.empty();  // kasowany przy każdym potwierdzeniu A (omowione())
  if (!nowyRaport) return WariantOmowienia::NAJPIERW_RAPORT;
  return s.potw == 1 ? WariantOmowienia::A_DRUGIE_BEZ_POPRAWY : WariantOmowienia::A_KOLEJNE_BEZ_POPRAWY;
}

bool omowione(reguly::Pamiec& pamiec, const std::string& klucz, const std::string& dzis, const reguly::Zestaw* z) {
  const WariantOmowienia wo = wariantOmowienia(pamiec, klucz, dzis, z);
  if (wo == WariantOmowienia::NIEDOSTEPNE || wo == WariantOmowienia::NAJPIERW_RAPORT) return false;
  auto it = pamiec.find(klucz);
  ZapisWzorca s = odczytajZapis(it->second.stanWzorca);
  s.stan = StanWzorca::OMOWIONE;
  if (s.pil == 'A') {
    s.potw = wo == WariantOmowienia::A_PIERWSZE ? 1 : s.potw + 1;
    s.refPotw = s.biez;
    s.raport.clear();  // raport sprzed tego potwierdzenia nie liczy się do następnego (R-LEK-26)
  }
  s.ref = s.biez;
  sprawy::zapiszOmowione(pamiec, klucz, dzis, pilnoscZeZnaku(s.pil));  // zapis:
  s.omowAnul.clear();
  s.wiemAnul = sprawy::dataWyciszenia(it->second.wiem);
  it->second.stanWzorca = zapiszZapis(s);
  return true;
}

void raportZEkranuL(reguly::Pamiec& pamiec, const std::string& klucz, const std::string& dzis) {
  auto it = pamiec.find(klucz);
  if (it == pamiec.end()) return;
  ZapisWzorca s = odczytajZapis(it->second.stanWzorca);
  if (s.stan == StanWzorca::NIEAKTYWNY) return;
  s.raport = dzis;
  it->second.stanWzorca = zapiszZapis(s);
}

std::vector<std::string> klucze(const std::vector<Wynik>& wyniki) {
  std::vector<std::string> k;
  for (const Grupa& g : grupy(wyniki)) k.push_back(g.klucz);
  return k;
}

InfoWzorca info(const std::vector<Wynik>& wyniki, const reguly::Pamiec& pamiec, const std::string& klucz,
                const std::string& dzis, const reguly::Zestaw* z) {
  InfoWzorca sp;
  sp.klucz = klucz;
  for (const Grupa& g : grupy(wyniki)) {
    if (g.klucz != klucz) continue;
    ZapisWzorca s;
    auto it = pamiec.find(g.klucz);
    if (it != pamiec.end()) s = odczytajZapis(it->second.stanWzorca);
    const Wynik* gl = nullptr;
    char pilN = '-';
    for (const Wynik* w : g.glowne)
      if (w->oceniona && w->spelniona && ranga(znakPilnosci(*w)) > ranga(pilN)) {
        pilN = znakPilnosci(*w);
        gl = w;
      }
    if (s.stan == StanWzorca::NIEAKTYWNY && gl) {  // pamięć sprzed aktualizuj(): wejście dziś
      s.stan = StanWzorca::AKTYWNY;
      s.pil = pilN;
      s.wejscie = dzis;
    }
    if (!gl) gl = g.glowne.front();
    sp.id = g.id;
    sp.galaz = g.galaz;
    sp.stanReguly = gl->stan;
    sp.stan = s.stan;
    sp.wejscie = s.wejscie;
    sp.wynik = gl;
    sp.pilnosc = pilnoscZeZnaku(s.pil);
    switch (s.pil) {
      case 'R': sp.karta = Karta::W_RAPORCIE; break;
      case 'P': sp.karta = Karta::PROBLEM; break;
      case 'C': sp.karta = Karta::NA_KONTROLE; break;
      default: sp.karta = Karta::PELNY_EKRAN; break;
    }
    std::string tytul, powod, linia;
    if (z)
      for (const auto& wk : z->katalog) {
        if (wk.id != gl->id || wk.wariant != gl->wariant) continue;
        for (const auto& t : wk.teksty) {
          auto tt = z->teksty.find(t);
          std::string rola = tt != z->teksty.end() ? tt->second.rola : "";
          if ((rola == "tytul" || t.find(".tytul") != std::string::npos) && tytul.empty()) tytul = t;
          if ((rola == "powod" || t.find(".powod") != std::string::npos) && powod.empty()) powod = t;
          if ((rola == "linia" || t.find(".linia") != std::string::npos) && linia.empty()) linia = t;
        }
      }
    if (powod.empty() && linia.empty()) powod = gl->id + ".powod";
    const bool bezEkranu = sp.karta == Karta::NA_KONTROLE || sp.karta == Karta::W_RAPORCIE;
    sp.idTytulu = sp.karta == Karta::NA_KONTROLE ? "" : tytul;
    sp.idPowodu = (bezEkranu && !linia.empty()) ? linia : powod;
    if (it != pamiec.end()) {
      const std::string wiemD = sprawy::dataWyciszenia(it->second.wiem);
      if (s.stan == StanWzorca::WIEM && !wiemD.empty()) {
        const double okres = prog(z, s.pil == 'A' ? "R-PRZ-03.wiem_a" : "R-PRZ-03.wiem_problem");
        if (!std::isnan(okres)) sp.wiemDo = dodajDni(wiemD, static_cast<int>(okres));
      }
      if (s.stan == StanWzorca::OMOWIONE) sp.omowioneOd = sprawy::dataWyciszenia(it->second.omowione);
      sp.pelnyEkranDzis = pelnyEkranDzisA(s, it->second, dzis, z);
    }
    sp.potwierdzenia = s.potw;
    if (sp.karta == Karta::PELNY_EKRAN || sp.karta == Karta::PROBLEM) sp.hipoteza = hipoteza(g.id, wyniki);
    return sp;
  }
  return sp;
}

std::string hipoteza(const std::string& idL, const std::vector<Wynik>& wyniki) {
  bool aktywnyL = false;
  for (const auto& w : wyniki)
    if (w.id == idL && !wierszPomocniczy(w) && w.stan == StanReguly::AKTYWNA && w.oceniona && w.spelniona)
      aktywnyL = true;  // którakolwiek gałąź główna (L-11 i L-11 „2 tygodnie”)
  if (!aktywnyL) return "";
  for (const auto& w : wyniki)
    if (w.id.compare(0, 2, "H-") == 0 && (w.wariant == idL || w.wariant.compare(0, idL.size() + 1, idL + "~") == 0) &&
        w.stan == StanReguly::AKTYWNA && w.oceniona &&
        w.spelniona)
      return w.id;
  return "";
}

std::vector<std::pair<std::string, std::string>> nieocenione(const std::vector<Wynik>& wyniki) {
  std::vector<std::pair<std::string, std::string>> r;
  for (const Grupa& g : grupy(wyniki)) {
    bool ktoras = false;
    for (const Wynik* w : g.glowne) ktoras = ktoras || w->oceniona;
    if (!ktoras) r.push_back({g.klucz, g.glowne.front()->powodNieoceny});
  }
  return r;
}

}  // namespace wzorce
