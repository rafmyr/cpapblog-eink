// SPDX-License-Identifier: GPL-3.0-or-later
// Silnik reguł (kontrakt src/reguly.h, spec techniczna 9.3, ADR-0023, ADR-0029, ADR-0030). Reguła to wiersz katalogu, rodzaj warunku to funkcja.
// Moduły: src/reguly.cpp ocena i pomocnicze wspólne, src/reguly_parser.cpp parser zestawu,
// src/reguly_rodzaje.cpp rodzaje rdzenia (opis parametrów na górze tego pliku), src/reguly_pamiec.cpp pamięć reguł;
// wspólne deklaracje wewnętrzne: src/reguly_wewn.h.
//
// ---------- zachowanie ocen() ----------
// Wiersze w kolejności katalogu (skrypt tools/zestaw/buduj_zestaw.py układa składnik przed regułą, która go używa):
//  - stan `zablokowana`: nie liczy się i nie ma go w wyniku (dla `wymaga` i `zlozenie` = nieaktywna);
//  - tryb bieżący NIEZNANY: każda reguła „nie oceniam” (R-DANE-29), poza wyjątkami E-01, E-03, E-04, E-05, E-07, E-10, E-11, E-12, E-20, E-22; tryby obsługiwane: CPAP, AutoSet, ASV, ASVAuto
//    (4.11, ADR-0030); reguła, której pole `tryby` nie obejmuje trybu bieżącego, w tym trybie „nie działa”
//    (4.11, tabela „nie działa w trybie”): bez wiersza w wyniku, dla `wymaga` i `zlozenie` = nieaktywna;
//    `tryby` = `wszystkie` albo puste = cztery tryby;
//  - `wymaga_normy` = tak bez normy (Kontekst::norma nullptr albo mniej nocy niż R-NOR-10.min_noce) → „nie oceniam”;
//  - `w_rozruchu` = nie przy mniej nocy ważnych niż R-NOR-10.min_noce (rozruch globalny 5.2.2) → „nie oceniam”;
//  - nieznany rodzaj (nie zarejestrowany) → „nie oceniam” z powodem (T-WYM-64, T-WYM-66: reszta zestawu działa;
//    rodzaje rejestrują się jawną funkcją po starcie, więc parser ich nie sprawdza);
//  - `wymaga`: reguła spełniona tylko, gdy każda wymagana jest spełniona; wymagana „nie oceniam” → „nie oceniam”;
//  - stan `tylko_raport`: liczy się, `pilnosc` = BRAK, nie wyklucza innych (R-SKI-44); `aktywna`: pilność z wiersza;
//  - `wyklucza` (po ocenie wszystkich, w kolejności katalogu): spełniona reguła `aktywna` gasi spełnione reguły
//    z listy (`id` = wszystkie warianty, `id/wariant` = jeden); wykluczona ma `spelniona = false` i zmienne
//    `wykluczona_przez` = klucz reguły oraz `spelniona_przed_wykluczeniem` = `tak` (R-EKR-48: sprawa trwa, ekran nie);
//  - wpis w logu (OpcjeOceny::log) dla każdej reguły „nie oceniam”: `reguly: {klucz} nie oceniam: {powód}`.
// Odwołanie do innej reguły po samym `id`, gdy reguła ma warianty: spełniona, gdy którykolwiek wariant spełniony;
// niespełniona, gdy wszystkie ocenione i niespełnione; inaczej „nie oceniam”.
//#include "reguly.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "reguly_pomoc.h"
#include "reguly_sygnaly.h"
#include "reguly_wewn.h"
#include "stany.h"
#include "wspolne.h"

namespace reguly {

namespace {
const Sygnaly* gSygnaly = nullptr;
}  // namespace

namespace wewn {

std::vector<std::string> dziel(const std::string& s, char sep) {
  std::vector<std::string> v;
  size_t a = 0;
  for (;;) {
    size_t b = s.find(sep, a);
    if (b == std::string::npos) {
      v.push_back(s.substr(a));
      return v;
    }
    v.push_back(s.substr(a, b - a));
    a = b + 1;
  }
}

std::string przytnij(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\r' || s[a] == '\t')) a++;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\r' || s[b - 1] == '\t')) b--;
  return s.substr(a, b - a);
}

std::vector<std::string> lista(const std::string& s) {
  std::vector<std::string> v;
  for (auto& x : dziel(s, ','))
    if (!przytnij(x).empty()) v.push_back(przytnij(x));
  return v;
}

std::string male(const std::string& s) {
  std::string t = s;
  for (auto& c : t) c = (char)std::tolower((unsigned char)c);
  return t;
}

// Liczba dziesiętna z kropką albo przecinkiem, cała treść pola (bez spacji wewnątrz).
bool liczbaZ(const std::string& t0, double& v) {
  std::string t = przytnij(t0);
  if (t.empty()) return false;
  size_t i = 0;
  if (t[0] == '-' || t[0] == '+') i = 1;
  bool cyfra = false, kropka = false;
  for (size_t j = i; j < t.size(); j++) {
    if (std::isdigit((unsigned char)t[j])) cyfra = true;
    else if ((t[j] == '.' || t[j] == ',') && !kropka) {
      kropka = true;
      t[j] = '.';
    } else return false;
  }
  if (!cyfra) return false;
  v = std::strtod(t.c_str(), nullptr);
  return true;
}

const char* nazwaTrybu(Tryb t) {
  switch (t) {
    case Tryb::CPAP: return "CPAP";
    case Tryb::AUTOSET: return "AutoSet";
    case Tryb::ASV: return "ASV";
    case Tryb::ASVAUTO: return "ASVAuto";
    default: return "nieznany";
  }
}

// `lista` = "wszystkie" (albo puste) albo tryby po przecinku; tryb NIEZNANY nigdy nie jest na liście.
bool trybNaLiscie(const std::string& l, Tryb t) {
  if (t == Tryb::NIEZNANY) return false;
  std::string s = przytnij(l);
  if (s.empty() || s == "wszystkie") return true;
  for (auto& x : lista(s))
    if (male(x) == male(nazwaTrybu(t))) return true;
  return false;
}

std::vector<NocTrybu> nocePosortowane(const Kontekst& k, bool& znane) {
  znane = false;
  std::vector<NocTrybu> v;
  if (!k.historia) return v;
  std::map<std::string, const Noc*> wg;
  auto lepsza = [](const Noc* a, const Noc* b) {  // a pełniejszy niż b
    if (a->czesciowa != b->czesciowa) return !a->czesciowa;
    if (a->wazna != b->wazna) return a->wazna;
    return a->m01 > b->m01;
  };
  for (auto& n : *k.historia) {
    if (!dataOk(n.data) || n.data > k.dzis) continue;
    znane = znane || !n.ustawienia.empty();
    auto it = wg.find(n.data);
    if (it == wg.end() || lepsza(&n, it->second)) wg[n.data] = &n;
  }
  std::string ost;
  for (auto& kv : wg) {
    if (!kv.second->ustawienia.empty()) ost = kv.second->ustawienia;
    v.push_back({kv.second, trybZUstawien(ost)});
  }
  return v;
}

bool nocRodzaju(const Kontekst& k, const Noc& n, const std::string& liczone) {
  double terapia = NAN;
  if (k.zestaw) {
    const Prog* p = k.zestaw->prog("R-SKI-27.noc_z_terapia_m01", k.trybBiezacy);
    if (p && p->tekst.empty()) terapia = p->wartosc;
  }
  bool zTerapia = !n.czesciowa && n.m01 >= 0 && !std::isnan(terapia) && n.m01 >= terapia;
  bool wazna = n.wazna && !n.czesciowa;
  if (liczone.empty() || liczone == "wazne") return wazna;
  if (liczone == "wazne_bez_niepewnych") return wazna && !nocNiepewna(n);
  if (liczone == "z_terapia") return zTerapia;
  if (liczone == "z_terapia_bez_niepewnych") return zTerapia && !nocNiepewna(n);
  if (liczone == "kompletne") return !n.czesciowa;
  // L-06 (5.12.3, R-SKI-21): noc z terapią z pomiarem SpO2 ≥ 70% czasu; rekord nocy nie ma kanału SpO2,
  // więc takiej nocy nie ma i reguła daje „nie oceniam” (za mało nocy)
  if (liczone == "z_terapia_spo2") return false;
  return liczone == "z_rekordem" || liczone == "wszystkie";
}

// Noc z datą `dzis` danego rodzaju w trybie obsługiwanym (zasada jednej nocy: tryb tej nocy, 4.11) albo nullptr.
const Noc* nocDzis(const Kontekst& k, const std::string& liczone, Tryb& tryb) {
  bool znane = false;
  std::vector<NocTrybu> h = nocePosortowane(k, znane);
  if (h.empty() || h.back().n->data != k.dzis) return nullptr;
  tryb = znane ? h.back().tryb : k.trybBiezacy;
  if (tryb == Tryb::NIEZNANY || !nocRodzaju(k, *h.back().n, liczone)) return nullptr;
  return h.back().n;
}

// ---------- odwołania do innych reguł ----------

Trzy stanReguly(const Kontekst& k, const std::string& ref, std::string& powod) {
  if (!k.wczesniejsze) {
    powod = "brak wynikow regul (" + ref + ")";
    return Trzy::NIEZNANE;
  }
  const auto& m = *k.wczesniejsze;
  auto it = m.find(ref);
  if (it != m.end()) {
    if (!it->second.oceniona) {
      powod = ref + ": nie oceniam";
      return Trzy::NIEZNANE;
    }
    return it->second.spelniona ? Trzy::PRAWDA : Trzy::FALSZ;
  }
  if (ref.find('/') == std::string::npos) {
    bool jest = false, nieznane = false;
    for (auto& kv : m) {
      if (kv.second.id != ref || kv.second.zmienne.count("pominieta")) continue;
      jest = true;
      if (kv.second.oceniona && kv.second.spelniona) return Trzy::PRAWDA;
      if (!kv.second.oceniona) nieznane = true;
    }
    if (jest && !nieznane) return Trzy::FALSZ;
    if (jest) {
      powod = ref + ": nie oceniam";
      return Trzy::NIEZNANE;
    }
  }
  powod = "brak reguly " + ref + " przed ta regula";
  return Trzy::NIEZNANE;
}

// Reguła `ref` nie liczy się w tym przebiegu: zablokowana albo spoza trybu (dla `id`: każdy wariant). przeglądu
// w `i` i `lub` taki składnik wypada (L-05: „jedno z L-01, L-02(b), L-06(c)” przy L-06 zablokowanym), a `nie`
// i `wymaga` na nim dają „nie oceniam” (panel nie twierdzi niczego o tym, czego nie liczy).
bool regulaPominieta(const Kontekst& k, const std::string& ref) {
  if (!k.wczesniejsze) return false;
  auto it = k.wczesniejsze->find(ref);
  if (it != k.wczesniejsze->end()) return it->second.zmienne.count("pominieta") > 0;
  if (ref.find('/') != std::string::npos) return false;
  bool jest = false;
  for (auto& kv : *k.wczesniejsze) {
    if (kv.second.id != ref) continue;
    jest = true;
    if (!kv.second.zmienne.count("pominieta")) return false;
  }
  return jest;
}

// Reguła `ref` (`id` albo `id/wariant`) była już oceniana w tym przebiegu (także zablokowana i spoza trybu).
bool regulaZnana(const Kontekst& k, const std::string& ref) {
  if (!k.wczesniejsze) return false;
  if (k.wczesniejsze->count(ref)) return true;
  if (ref.find('/') != std::string::npos) return false;
  for (auto& kv : *k.wczesniejsze)
    if (kv.second.id == ref) return true;
  return false;
}

bool pasujeDoRef(const Wynik& w, const std::string& ref) {
  if (ref.find('/') != std::string::npos) return kluczWyniku(w.id, w.wariant) == ref;
  return w.id == ref;
}

std::string jakoKlucz(const WierszKatalogu& w) { return kluczWyniku(w.id, w.wariant); }

std::map<std::string, FunkcjaRodzaju>& rejestr() {
  static std::map<std::string, FunkcjaRodzaju> r = rdzen();
  return r;
}

}  // namespace wewn

using namespace wewn;

void zarejestrujRodzaj(const std::string& nazwa, FunkcjaRodzaju f) {
  if (f) rejestr()[nazwa] = f;
}
bool znanyRodzaj(const std::string& nazwa) { return rejestr().count(nazwa) > 0; }

std::vector<Wynik> ocen(const Zestaw& z, const Kontekst& k) { return ocen(z, k, OpcjeOceny{}); }

// Zdarzenia oceniane także w trybie nieznanym i spoza wydania (R-DANE-29, lista w 4.11 „Noc w trybie spoza wydania”).
static bool wyjatekTrybuNieznanego(const std::string& id) {
  static const char* const ID[] = {"E-01", "E-03", "E-04", "E-05", "E-07", "E-10", "E-11", "E-12", "E-20", "E-22"};
  for (const char* x : ID)
    if (id == x) return true;
  return false;
}

std::vector<Wynik> ocen(const Zestaw& z, const Kontekst& k0, const OpcjeOceny& o) {
  struct Przywroc {
    const Sygnaly* s;
    ~Przywroc() { gSygnaly = s; }
  } przywroc{gSygnaly};
  gSygnaly = o.sygnaly;
  std::map<std::string, Wynik> wcz;
  Kontekst k = k0;
  if (!k.zestaw) k.zestaw = &z;
  k.wczesniejsze = &wcz;
  int wazne = 0;
  {
    bool znane = false;
    for (auto& x : nocePosortowane(k, znane))
      if (x.n->wazna && !x.n->czesciowa) wazne++;
  }
  const Prog* pr = z.prog("R-NOR-10.min_noce", k.trybBiezacy);
  std::vector<Wynik> wyn;
  std::vector<const WierszKatalogu*> wiersze;
  for (const WierszKatalogu& w : z.katalog) {
    std::string kl = jakoKlucz(w);
    Wynik r;
    if (w.stan == StanReguly::ZABLOKOWANA) {
      r = nieoceniona(w, "zablokowana");
      r.zmienne["pominieta"] = "zablokowana";
      wcz[kl] = r;
      continue;
    }
    // R-DANE-29 (4.11): przy trybie nieznanym działają tylko zdarzenia bez normy, których warunek nie zależy od trybu
    bool nieznanyBezWyjatku = k.trybBiezacy == Tryb::NIEZNANY && !wyjatekTrybuNieznanego(w.id);
    if (nieznanyBezWyjatku) {
      r = nieoceniona(w, "tryb aparatu nieznany");
    } else if (k.trybBiezacy != Tryb::NIEZNANY && !trybNaLiscie(w.tryby, k.trybBiezacy)) {
      r = nieoceniona(w, std::string("nie dziala w trybie ") + nazwaTrybu(k.trybBiezacy));
      r.zmienne["pominieta"] = "tryb";
      wcz[kl] = r;
      continue;
    } else if ((w.wymagaNormy || !w.wRozruchu) && (!pr || !pr->tekst.empty())) {
      r = nieoceniona(w, "brak progu R-NOR-10.min_noce");
    } else if (w.wymagaNormy && (!k.norma || k.norma->n < (int)std::lround(pr->wartosc))) {
      r = nieoceniona(w, "brak normy");
    } else if (!w.wRozruchu && wazne < (int)std::lround(pr->wartosc)) {
      r = nieoceniona(w, "rozruch: " + std::to_string(wazne) + " nocy waznych");
    } else {
      auto it = rejestr().find(w.rodzaj);
      if (it == rejestr().end()) r = nieoceniona(w, "nieznany rodzaj " + w.rodzaj);
      else r = it->second(w, k);
    }
    if (r.oceniona && r.spelniona) {
      for (auto& x : w.wymaga) {
        std::string pw;
        Trzy t = stanReguly(k, x, pw);
        if (t == Trzy::FALSZ) {
          r.spelniona = false;
          break;
        }
        if (t == Trzy::NIEZNANE) {
          r.oceniona = false;
          r.spelniona = false;
          r.powodNieoceny = "wymaga " + pw;
          break;
        }
      }
    }
    if (!r.oceniona) r.spelniona = false;
    r.id = w.id;
    r.wariant = w.wariant;
    r.poziom = w.poziom;
    r.grupa = w.grupa;
    r.stan = w.stan;
    r.pilnosc = w.stan == StanReguly::AKTYWNA ? w.pilnosc : Pilnosc::BRAK;
    wcz[kl] = r;
    wyn.push_back(r);
    wiersze.push_back(&w);
  }
  // Holm–Bonferroni na wszystkich wynikach z testem permutacyjnym (`holm` = tak, zmienna `p`), R-NF-24 i 9.3
  {
    std::vector<std::pair<double, size_t>> pv;
    for (size_t i = 0; i < wyn.size(); i++)
      if (wyn[i].oceniona && wyn[i].zmienne.count("holm")) pv.push_back({liczbaRzecz(wyn[i].zmienne["p"].c_str()), i});
    std::sort(pv.begin(), pv.end());
    bool dalej = true;
    for (size_t j = 0; j < pv.size(); j++) {
      dalej = dalej && pv[j].first <= POZIOM_ISTOTNOSCI / (double)(pv.size() - j);
      Wynik& r = wyn[pv[j].second];
      r.spelniona = r.spelniona && dalej;
      wcz[kluczWyniku(r.id, r.wariant)] = r;
    }
  }
  for (size_t i = 0; i < wyn.size(); i++) {
    if (!wyn[i].oceniona || !wyn[i].spelniona || wiersze[i]->stan != StanReguly::AKTYWNA) continue;
    for (auto& ex : wiersze[i]->wyklucza)
      for (size_t j = 0; j < wyn.size(); j++) {
        if (j == i || !wyn[j].spelniona || !pasujeDoRef(wyn[j], ex)) continue;
        wyn[j].spelniona = false;
        wyn[j].zmienne["wykluczona_przez"] = jakoKlucz(*wiersze[i]);
        wyn[j].zmienne["spelniona_przed_wykluczeniem"] = "tak";
      }
  }
  if (o.log)
    for (auto& r : wyn)
      if (!r.oceniona) o.log("reguly: " + kluczWyniku(r.id, r.wariant) + " nie oceniam: " + r.powodNieoceny);
  return wyn;
}

// metryka wiersza katalogu (parametr `metryka`, przy id progu metryka trybu) i
// zapis wartości `{x}`, `{norma}`, `{wartosc}` w jej jednostce: czas z maską (M-01, minuty) `7 h 12`, liczby całkowite
// z natury (przerwy M-17, minuty dużego wycieku M-08) bez `,0`. Zmienne wyniku zostają liczbami (wzorce::wartoscX()).
namespace {
std::string metrykaWyniku(const Zestaw& z, const Wynik& w, Tryb tryb) {
  for (auto& k : z.katalog) {
    if (k.id != w.id || k.wariant != w.wariant) continue;
    auto p = k.parametry.find("metryka");
    if (p == k.parametry.end()) return "";
    if (p->second.compare(0, 2, "M-") == 0) return p->second;
    const Prog* pr = z.prog(p->second, tryb);
    return pr ? pr->tekst : "";
  }
  return "";
}

std::string wartoscWJednostce(const std::string& m, const std::string& s) {
  if (m != "M-01" && m != "M-17" && m != "M-08") return s;
  std::string t = s;
  std::replace(t.begin(), t.end(), ',', '.');
  char* kon = nullptr;
  double v = std::strtod(t.c_str(), &kon);
  if (kon == t.c_str() || *kon != '\0' || std::isnan(v)) return s;
  if (m == "M-01") return godzH((int)std::lround(v));
  return liczbaTekst(v);
}
}  // namespace

std::string tekst(const Zestaw& z, const std::string& idTekstu, const Wynik& w, Tryb tryb) {
  auto it = z.teksty.find(idTekstu);
  if (it == z.teksty.end()) return "";
  const std::string& t = it->second.tekst;
  const std::string m = metrykaWyniku(z, w, tryb);
  std::string o;
  for (size_t i = 0; i < t.size();) {
    if (t[i] != '{') {
      o += t[i++];
      continue;
    }
    size_t e = t.find('}', i);
    if (e == std::string::npos) {
      o += t.substr(i);
      break;
    }
    std::string nazwa = t.substr(i + 1, e - i - 1);
    if (nazwa.compare(0, 5, "prog:") == 0) {
      const Prog* p = z.prog(nazwa.substr(5), tryb);
      o += p ? (p->tekst.empty() ? liczbaProgu(p->wartosc) : p->tekst) : "–";
    } else {
      auto v = w.zmienne.find(nazwa);
      if (v == w.zmienne.end()) o += "–";
      else if (nazwa == "x" || nazwa == "norma" || nazwa == "wartosc") o += wartoscWJednostce(m, v->second);
      else o += v->second;
    }
    i = e + 1;
  }
  return o;
}

double metryka(const Noc& n, const std::string& m) {
  if (m == "M-01") return n.m01 < 0 ? NAN : (double)n.m01;
  if (m == "M-02") return n.ahi;
  if (m == "M-03oa") return n.oa;
  if (m == "M-03ca") return n.ca;
  if (m == "M-03h") return n.h;
  // zdarzenia obturacyjne OA + UA + H (5.12.3); noc bez UA (z karty): OA + H
  if (m == "M-03ob") return (double)n.oa + (std::isnan(n.ua) ? 0.0 : (double)n.ua) + (double)n.h;
  if (m == "M-05") return n.cmed;
  if (m == "M-05p") return n.cp95;
  if (m == "M-06") return epapMed(n);
  if (m == "M-06p") return epapP95(n);
  if (m == "M-07") return n.wp95;  // wyciek P95 (4.5, stan odchylenia, S-K-10, E-23)
  if (m == "M-07m") return n.wmed;  // wyciek, mediana (4.9)
  if (m == "M-09") return n.csr;
  if (m == "M-09p") return n.m01 > 0 ? (double)n.csr * 100.0 / n.m01 : NAN;  // CSR% czasu terapii
  if (m == "M-28") return n.psmed;
  if (m == "M-28p") return n.psp95;
  // pola (kontrakt 3)
  if (m == "M-13") return n.m13;
  if (m == "M-17") return n.m17 < 0 ? NAN : (double)n.m17;
  if (m == "M-08") return n.m08;
  if (m == "M-03ua") return n.ua;
  if (m == "M-10") return n.m10;
  if (m == "M-10p") return n.m10p;
  if (m == "M-21mpa") return n.mpa;
  if (m == "M-21mpi") return n.mpi;
  if (m == "M-21ipc") return n.ipc;
  if (m == "M-21eep") return n.eep;
  if (m == "M-21mns") return n.mns;
  if (m == "M-21mxs") return n.mxs;
  return NAN;
}

std::vector<const Noc*> noceOkna(const Kontekst& k, const std::string& liczone, int ile) {
  // okno reguły liczy tylko noce w trybie bieżącym (jak norma po zmianie ustawień,
  // R-NOR-07); noce w trybie spoza obsługiwanych wypadają (R-DANE-29). Bez żadnych ustawień w historii: bez filtra.
  std::vector<const Noc*> v;
  bool znane = false;
  for (auto& x : nocePosortowane(k, znane)) {
    if (znane && x.tryb != k.trybBiezacy) continue;
    if (nocRodzaju(k, *x.n, liczone)) v.push_back(x.n);
  }
  if (ile > 0 && (int)v.size() > ile) v.erase(v.begin(), v.end() - ile);
  return v;
}

double mediana(std::vector<double> v) { return ::mediana(std::move(v)); }

// ---------- reguly_pomoc.h ----------
const Sygnaly* sygnalyOceny() { return gSygnaly; }

const std::string* parametr(const WierszKatalogu& w, const char* nazwa) {
  auto it = w.parametry.find(nazwa);
  return it == w.parametry.end() ? nullptr : &it->second;
}

const Prog* progParametru(const WierszKatalogu& w, const Kontekst& k, const char* nazwa, std::string& powod) {
  const std::string* id = parametr(w, nazwa);
  if (!id) {
    powod = std::string("brak parametru ") + nazwa;
    return nullptr;
  }
  if (!k.zestaw) {
    powod = "brak zestawu w kontekscie";
    return nullptr;
  }
  const Prog* p = k.zestaw->prog(*id, k.trybBiezacy);
  if (!p) powod = "brak progu " + *id + " w trybie " + nazwaTrybu(k.trybBiezacy);
  return p;
}

Wynik nieoceniona(const WierszKatalogu& w, const std::string& powod) {
  Wynik r = oceniona(w, false);
  r.oceniona = false;
  r.powodNieoceny = powod;
  return r;
}

Wynik oceniona(const WierszKatalogu& w, bool spelniona) {
  Wynik r;
  r.id = w.id;
  r.wariant = w.wariant;
  r.poziom = w.poziom;
  r.grupa = w.grupa;
  r.stan = w.stan;
  r.oceniona = true;
  r.spelniona = spelniona;
  return r;
}

bool porownaj(double a, const std::string& por, double b) {
  if (std::isnan(a) || std::isnan(b)) return false;
  // przeglądu wartości nocy są float (src/noc.h), progi double; „dokładnie na progu” (AHI 5,1 wobec 5,1)
  // ma być na progu, więc tolerancja względna 1e-6 (float ma ok. 7 cyfr znaczących, wartości 1–2 miejsca po przecinku)
  const double e = 1e-6 * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
  if (por == "gt") return a > b + e;
  if (por == "le") return a <= b + e;
  if (por == "lt") return a < b - e;
  return a >= b - e;
}

Tryb trybNocy(const Kontekst& k, const Noc& n) {
  if (!n.ustawienia.empty()) return trybZUstawien(n.ustawienia);
  if (!k.historia) return Tryb::NIEZNANY;
  const Noc* ost = nullptr;
  for (auto& x : *k.historia)
    if (x.data < n.data && !x.ustawienia.empty() && (!ost || x.data > ost->data)) ost = &x;
  return ost ? trybZUstawien(ost->ustawienia) : Tryb::NIEZNANY;
}

std::string kluczWyniku(const std::string& id, const std::string& wariant) {
  return wariant.empty() ? id : id + "/" + wariant;
}

std::string liczbaProgu(double v) {
  if (std::isnan(v)) return "–";
  char b[32];
  std::snprintf(b, sizeof b, "%.3f", v);
  std::string t = b;
  while (!t.empty() && t.back() == '0') t.pop_back();
  if (!t.empty() && t.back() == '.') t.pop_back();
  for (auto& c : t)
    if (c == '.') c = ',';
  return t == "-0" ? "0" : t;
}

std::string liczbaTekst(double v) {
  if (std::isnan(v)) return "–";
  if (std::fabs(v - std::round(v)) < 1e-9) return calkowita(v);
  return przecinek1(v);
}

std::vector<std::string> rodzajeRdzenia() {
  std::vector<std::string> v;
  for (auto& kv : rdzen()) v.push_back(kv.first);
  return v;
}

}  // namespace reguly
