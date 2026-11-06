// SPDX-License-Identifier: GPL-3.0-or-later
// Rodzaje warunków rdzenia silnika reguł.
// ---------- rodzaje rdzenia i ich parametry (kontrakt 6; wartości liczbowe zawsze jako id_progu) ----------
// Wspólne: `metryka` (lista w metryka()), `por` = ge (domyślnie), gt, le, lt. Noce okna: pole `liczone_noce`
// (`kompletne`, `z_rekordem`, `z_terapia`, `z_terapia_bez_niepewnych`, `wazne` = domyślnie, `wazne_bez_niepewnych`,
// `wszystkie`), tylko noce do `dzis` w trybie obsługiwanym, i tylko noce z wartością metryki. Za mało takich nocy →
// „nie oceniam” z powodem `za malo nocy {jest}/{trzeba}`. Zmienne tekstów: `x` (wartość ostatniej nocy okna),
// `data` (jej data), `n`, `z`, `wartosc`, `norma` według rodzaju.
// prog_nocy metryka, prog, por; noc z datą `dzis` (zasada jednej nocy); próg z trybu tej nocy.
//                      Zmienne: x, wartosc, data.
//  k_z_n_nocy          metryka, prog, por, kzn (próg tekstowy `k/n`); ostatnie n nocy. Zmienne: n (spełnione), z, x.
//  statystyka_okna     metryka, stat (`mediana` domyślnie, `srednia`, `suma`, `odch_std` z próby, `udzial`), prog, por,
//                      n (liczba nocy; bez niego pole `okno`). `udzial`: odsetek nocy okna z warunkiem nocy
//                      `{metryka} {por_nocy} {prog_nocy}` wobec `prog` w % [parametry `prog_nocy`, `por_nocy` dopisane
//                      w silniku]. Zmienne: wartosc (statystyka), n (noce z wartością {por} prog, przy `udzial` noce
//                      z warunkiem nocy), z, x.
//  zlozenie            op (`i`, `lub`, `nie`), skladniki (`id` albo `id/wariant` rozdzielone `+`); logika trójwartościowa
//                      (fałsz I nieznane = fałsz, prawda LUB nieznane = prawda, reszta z nieznanym = „nie oceniam”).
// Fala 2 (opis parametrów niżej, nad funkcjami): kolejne_noce, okno_wobec_normy, odchylenie_nocy, stan_odchylenia,
// porownanie_okien, trwanie, od_zdarzenia, odpowiedzi_tygodni, zawsze, sygnal_komponentu, prog_biezacy.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "reguly.h"
#include "reguly_pomoc.h"
#include "reguly_sygnaly.h"
#include "reguly_wewn.h"
#include "stany.h"
#include "wspolne.h"

namespace reguly {
using namespace wewn;

namespace {

// Metryki nocy, które zna metryka() (lista dla kontroli wiersza; wartości: metryka()).
const char* const METRYKI[] = {"M-01",  "M-02",  "M-03oa", "M-03ca", "M-03h", "M-03ua", "M-03ob", "M-05",
                               "M-05p", "M-06",  "M-06p",  "M-07",   "M-07m", "M-08",   "M-09",   "M-09p",
                               "M-10",  "M-10p", "M-13",   "M-17",   "M-28",  "M-28p",
                               // ustawienia aparatu (M-21) tylko do odczytu: porównania typu `blisko_ustawienia`,
                               // nigdy wartość docelowa (ADR-0002)
                               "M-21mpa", "M-21mpi", "M-21ipc", "M-21eep", "M-21mns", "M-21mxs"};

// ---------- pomocnicze rodzajów ----------

// Wartość liczbowa progu z parametru; false i powód przy braku.
bool wartoscProgu(const WierszKatalogu& w, const Kontekst& k, const char* nazwa, double& v, std::string& powod) {
  const Prog* p = progParametru(w, k, nazwa, powod);
  if (!p) return false;
  if (!p->tekst.empty()) {
    powod = "prog " + *parametr(w, nazwa) + " nie jest liczba";
    return false;
  }
  v = p->wartosc;
  return true;
}

// Jednostka metryki nocy ("" = bez jednostki, nullptr = metryka nieznana tej tabeli, np. odczyt bieżący).
const char* jednostkaMetryki(const std::string& m) {
  if (m == "M-01" || m == "M-08" || m == "M-09") return "min";
  if (m == "M-09p") return "%";
  if (m == "M-02" || m.compare(0, 4, "M-03") == 0) return "/h";
  if (m.compare(0, 4, "M-05") == 0 || m.compare(0, 4, "M-06") == 0 || m.compare(0, 4, "M-28") == 0 ||
      m.compare(0, 4, "M-21") == 0)
    return "cmH2O";
  if (m.compare(0, 4, "M-07") == 0) return "L/min";
  if (m == "M-13") return "/min";
  if (m == "M-17" || m == "M-10" || m == "M-10p") return "";
  return nullptr;
}

// Próg w jednostce metryki (jedno miejsce przeliczeń; progi w godzinach H-09, L-12,
// R-DANE-13 wobec M-01 w minutach). Jednostka progu z kolumny `jednostka` progi.csv: `h` → × 60 i `s` → / 60 dla
// metryk w minutach; ta sama jednostka bez zmian; jednostka fizyczna (min, h, s, /h, /min, L/min, cmH2O, %, mL, ppm,
// °C) inna niż jednostka metryki → „nie oceniam” (błąd katalogu); inne zapisy (noce, × norma, pkt proc. …) bez zmian.
bool przeliczNaMetryke(const Prog& p, const std::string& m, double& v, std::string& powod) {
  const char* jm = jednostkaMetryki(m);
  if (!jm) return true;
  std::string j = przytnij(p.jednostka);
  size_t nawias = j.find(" (");
  if (nawias != std::string::npos) j = j.substr(0, nawias);  // `cmH2O (wzór)`
  static const char* const FIZ[] = {"min", "h", "s", "/h", "/min", "L/min", "cmH2O", "%", "mL", "ppm", "°C"};
  bool fiz = false;
  for (const char* f : FIZ) fiz = fiz || j == f;
  if (!fiz || j == jm) return true;
  if (std::string(jm) == "min" && j == "h") {
    v *= 60;
    return true;
  }
  if (std::string(jm) == "min" && j == "s") {
    v /= 60;
    return true;
  }
  powod = "jednostka progu " + p.id + " (" + p.jednostka + ") nie pasuje do " + m + " (" + jm + ")";
  return false;
}

// Wartość progu z parametru w jednostce metryki `m`.
bool progMetryki(const WierszKatalogu& w, const Kontekst& k, const char* nazwa, const std::string& m, double& v,
                 std::string& powod) {
  const Prog* p = progParametru(w, k, nazwa, powod);
  if (!p) return false;
  if (!p->tekst.empty()) {
    powod = "prog " + *parametr(w, nazwa) + " nie jest liczba";
    return false;
  }
  v = p->wartosc;
  return przeliczNaMetryke(*p, m, v, powod);
}

// Liczba nocy z parametru `nazwa` (albo z pola `okno`, gdy `zOkna`).
bool liczbaNocy(const WierszKatalogu& w, const Kontekst& k, const char* nazwa, bool zOkna, int& n,
                std::string& powod) {
  double v = 0;
  if (parametr(w, nazwa)) {
    if (!wartoscProgu(w, k, nazwa, v, powod)) return false;
  } else if (zOkna && !w.okno.empty()) {
    const Prog* p = k.zestaw ? k.zestaw->prog(w.okno, k.trybBiezacy) : nullptr;
    if (!p || !p->tekst.empty()) {
      powod = "brak progu okna " + w.okno;
      return false;
    }
    v = p->wartosc;
  } else {
    powod = std::string("brak parametru ") + nazwa;
    return false;
  }
  if (v < 1 || v > 10000 || std::fabs(v - std::round(v)) > 1e-9) {
    powod = std::string("zla liczba nocy w ") + nazwa;
    return false;
  }
  n = (int)std::lround(v);
  return true;
}

// Próg `k/n` z parametru.
bool kZN(const WierszKatalogu& w, const Kontekst& k, const char* nazwa, int& kk, int& nn, std::string& powod) {
  const Prog* p = progParametru(w, k, nazwa, powod);
  if (!p) return false;
  std::vector<std::string> c = dziel(p->tekst, '/');
  double a = 0, b = 0;
  if (c.size() != 2 || !liczbaZ(c[0], a) || !liczbaZ(c[1], b) || a < 1 || b < 1 || a > b ||
      std::fabs(a - std::round(a)) > 1e-9 || std::fabs(b - std::round(b)) > 1e-9) {
    powod = "prog " + *parametr(w, nazwa) + " nie jest k/n";
    return false;
  }
  kk = (int)std::lround(a);
  nn = (int)std::lround(b);
  return true;
}

const std::string& metrykaWiersza(const WierszKatalogu& w) {
  static const std::string pusta;
  const std::string* m = parametr(w, "metryka");
  return m ? *m : pusta;
}

bool znanaMetryka(const std::string& m) {
  for (const char* x : METRYKI)
    if (m == x) return true;
  return false;
}

// Ostatnie `ile` nocy okna z wartością metryki (rosnąco po dacie); ile ≤ 0 = wszystkie.
struct Okno {
  std::vector<const Noc*> noce;
  std::vector<double> v;
};
Okno oknoMetryki(const WierszKatalogu& w, const Kontekst& k, const std::string& m, int ile) {
  Okno o;
  std::vector<const Noc*> wsz = noceOkna(k, w.liczoneNoce, 0);
  for (size_t i = wsz.size(); i-- > 0;) {
    if (ile > 0 && (int)o.noce.size() >= ile) break;
    double x = metryka(*wsz[i], m);
    if (std::isnan(x)) continue;
    o.noce.insert(o.noce.begin(), wsz[i]);
    o.v.insert(o.v.begin(), x);
  }
  return o;
}

std::string zaMalo(size_t jest, int trzeba) {
  return "za malo nocy " + std::to_string(jest) + "/" + std::to_string(trzeba);
}

double statystyka(const std::string& stat, const std::vector<double>& v, bool& ok) {
  ok = true;
  if (v.empty()) {
    ok = false;
    return NAN;
  }
  if (stat.empty() || stat == "mediana") return mediana(v);
  double s = 0;
  for (double x : v) s += x;
  if (stat == "suma") return s;
  if (stat == "srednia") return s / (double)v.size();
  if (stat == "odch_std") {
    if (v.size() < 2) return NAN;
    double sr = s / (double)v.size(), q = 0;
    for (double x : v) q += (x - sr) * (x - sr);
    return std::sqrt(q / (double)(v.size() - 1));
  }
  ok = false;
  return NAN;
}

bool wymaganaMetryka(const WierszKatalogu& w, std::string& m, std::string& powod) {
  m = metrykaWiersza(w);
  if (m.empty()) {
    powod = "brak parametru metryka";
    return false;
  }
  if (!znanaMetryka(m)) {
    powod = "nieznana metryka " + m;
    return false;
  }
  return true;
}

std::string porWiersza(const WierszKatalogu& w, const char* nazwa = "por") {
  const std::string* p = parametr(w, nazwa);
  return p ? *p : "ge";
}

bool porZnane(const std::string& p) { return p == "ge" || p == "gt" || p == "le" || p == "lt"; }

// ---------- rodzaje fali 1 ----------
Wynik rProgNocy(const WierszKatalogu& w, const Kontekst& k) {
  std::string m, powod;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  std::string por = porWiersza(w);
  if (!porZnane(por)) return nieoceniona(w, "zle por " + por);
  Tryb tn = Tryb::NIEZNANY;
  const Noc* pn = nocDzis(k, w.liczoneNoce, tn);
  if (!pn) return nieoceniona(w, "brak nocy " + k.dzis + " tego rodzaju");
  const Noc& n = *pn;
  double x = metryka(n, m);
  if (std::isnan(x)) return nieoceniona(w, "brak metryki " + m + " w nocy " + n.data);
  Kontekst kn = k;
  kn.trybBiezacy = tn;
  double prog = 0;
  if (!progMetryki(w, kn, "prog", m, prog, powod)) return nieoceniona(w, powod);
  Wynik r = oceniona(w, porownaj(x, por, prog));
  r.zmienne["x"] = przecinek1(x);
  r.zmienne["wartosc"] = przecinek1(x);
  r.zmienne["data"] = n.data;
  return r;
}

Wynik rKZNNocy(const WierszKatalogu& w, const Kontekst& k) {
  std::string m, powod;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  std::string por = porWiersza(w);
  if (!porZnane(por)) return nieoceniona(w, "zle por " + por);
  int kk = 0, nn = 0;
  if (!kZN(w, k, "kzn", kk, nn, powod)) return nieoceniona(w, powod);
  double prog = 0;
  if (!progMetryki(w, k, "prog", m, prog, powod)) return nieoceniona(w, powod);
  Okno o = oknoMetryki(w, k, m, nn);
  if ((int)o.v.size() < nn) return nieoceniona(w, zaMalo(o.v.size(), nn));
  int ile = 0;
  for (double x : o.v)
    if (porownaj(x, por, prog)) ile++;
  Wynik r = oceniona(w, ile >= kk);
  r.zmienne["n"] = std::to_string(ile);
  r.zmienne["z"] = std::to_string(nn);
  r.zmienne["x"] = przecinek1(o.v.back());
  r.zmienne["data"] = o.noce.back()->data;
  return r;
}

Wynik rStatystykaOkna(const WierszKatalogu& w, const Kontekst& k) {
  std::string m, powod;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  std::string por = porWiersza(w);
  if (!porZnane(por)) return nieoceniona(w, "zle por " + por);
  const std::string* st = parametr(w, "stat");
  std::string stat = st ? *st : "mediana";
  int n = 0;
  if (!liczbaNocy(w, k, "n", true, n, powod)) return nieoceniona(w, powod);
  double prog = 0;
  bool okProg = stat == "udzial" ? wartoscProgu(w, k, "prog", prog, powod) : progMetryki(w, k, "prog", m, prog, powod);
  if (!okProg) return nieoceniona(w, powod);
  Okno o = oknoMetryki(w, k, m, n);
  if ((int)o.v.size() < n) return nieoceniona(w, zaMalo(o.v.size(), n));
  double wart = NAN;
  int ile = 0;
  if (stat == "udzial") {
    double pn = 0;
    if (!progMetryki(w, k, "prog_nocy", m, pn, powod)) return nieoceniona(w, powod);
    std::string porN = porWiersza(w, "por_nocy");
    if (!porZnane(porN)) return nieoceniona(w, "zle por_nocy " + porN);
    for (double x : o.v)
      if (porownaj(x, porN, pn)) ile++;
    wart = 100.0 * ile / (double)o.v.size();
  } else {
    bool ok = false;
    wart = statystyka(stat, o.v, ok);
    if (!ok) return nieoceniona(w, "nieznana statystyka " + stat);
    if (std::isnan(wart)) return nieoceniona(w, "statystyka " + stat + " bez wartosci");
    for (double x : o.v)
      if (porownaj(x, por, prog)) ile++;
  }
  Wynik r = oceniona(w, porownaj(wart, por, prog));
  r.zmienne["wartosc"] = przecinek1(wart);
  r.zmienne["n"] = std::to_string(ile);
  r.zmienne["z"] = std::to_string(n);
  r.zmienne["x"] = przecinek1(o.v.back());
  r.zmienne["data"] = o.noce.back()->data;
  return r;
}

Wynik rZlozenie(const WierszKatalogu& w, const Kontekst& k) {
  const std::string* op = parametr(w, "op");
  const std::string* sk = parametr(w, "skladniki");
  if (!op || !sk) return nieoceniona(w, "brak parametru op albo skladniki");
  std::vector<std::string> s;
  for (auto& x : dziel(*sk, '+'))
    if (!przytnij(x).empty()) s.push_back(przytnij(x));
  if (s.empty()) return nieoceniona(w, "puste skladniki");
  if (*op != "i" && *op != "lub" && *op != "nie") return nieoceniona(w, "nieznane op " + *op);
  if (*op == "nie" && s.size() != 1) return nieoceniona(w, "op nie: jeden skladnik");
  bool prawda = false, falsz = false;
  std::string nieznany;
  for (auto& x : s)
    if (!regulaZnana(k, x)) return nieoceniona(w, "brak skladnika " + x + " przed ta regula");
  if (*op != "nie") {
    std::vector<std::string> licz;
    for (auto& x : s)
      if (!regulaPominieta(k, x)) licz.push_back(x);
    if (licz.empty()) return nieoceniona(w, "wszystkie skladniki zablokowane albo spoza trybu");
    s = licz;
  }
  for (auto& x : s) {
    std::string pw;
    Trzy t = stanReguly(k, x, pw);
    if (t == Trzy::PRAWDA) prawda = true;
    else if (t == Trzy::FALSZ) falsz = true;
    else if (nieznany.empty()) nieznany = pw;
  }
  if (*op == "nie") {
    if (!nieznany.empty()) return nieoceniona(w, nieznany);
    return oceniona(w, !prawda);
  }
  if (*op == "i") {
    if (falsz) return oceniona(w, false);
    if (!nieznany.empty()) return nieoceniona(w, nieznany);
    return oceniona(w, true);
  }
  if (prawda) return oceniona(w, true);
  if (!nieznany.empty()) return nieoceniona(w, nieznany);
  return oceniona(w, false);
}

// ---------- rodzaje fali 2 ----------
// Parametry (kontrakt 6; liczby jako id_progu):
//  kolejne_noce        metryka, por, n; warunek nocy: wobec `prog` albo wobec normy (`wzgledem` = mediana, p10, p90
//                      i `delta` albo `razy`); spełniona, gdy warunek trwa w każdej z n ostatnich nocy. Zmienne: z, x.
//  okno_wobec_normy    metryka, stat (mediana), n (albo pole `okno`), wzgledem (mediana), delta i/lub razy, por;
//                      statystyka okna {por} punkt normy + delta (i {por} punkt × razy, gdy oba). Norma z Kontekst::norma
//                      (norma sprzed odchylenia trzyma liczStany(), R-NOR-22). Zmienne: wartosc, norma, z, x.
// odchylenie_nocy metryka, delta, por (ge: powyżej P90 i ≥ mediana + delta; le: poniżej P10 i ≤ mediana − delta,
//                      4.5), kzn (k z n ostatnich nocy; bez kzn: noc z datą `dzis`), min (opcjonalnie: stan odchylenia
//                      tej metryki trwa co najmniej tyle nocy, R-NOR-23). Norma jedna (Kontekst::norma) dla całego okna
//                      [uproszczenie wobec liczStany(), które liczy normę noc po nocy]. Zmienne: n, z, x, norma.
//  stan_odchylenia     metryka (M-01, M-02, M-07, M-06p: stany z src/stany.h), min (opcjonalnie: liczba nocy stanu).
//                      Zmienne: od, nocy, x, norma.
//  porownanie_okien    metryka, stat (mediana), okno_a, okno_b, por, delta i/lub razy, zdarzenie (opcjonalnie
//                      `zmiana_ustawien`: A = pierwsze noce od ostatniej zmiany, B = noce tuż przed nią; bez niego
//                      A = ostatnie noce, B = noce tuż przed A, okna rozłączne). Zmienne: wartosc (A), wartosc_b, z.
//  trwanie             regula (`id` albo `id/wariant`, oceniana wcześniej), min (i/lub max), jednostka (`dni`
//                      domyślnie albo `noce` z pola `liczone_noce`); początek z pamięci (PamiecReguly::aktywnaOd,
//                      puste = od dziś). Bez pamięci w kontekście → „nie oceniam”. Zmienne: n, od.
//  od_zdarzenia        zdarzenie (`pierwsza_noc`, `zmiana_ustawien`, `poczatek_leczenia` z Kontekst::poczatekLeczenia;
//                      `nowy_poziom` → „nie oceniam”, bo komponent stanów
//                      go nie podaje), min i/lub max, jednostka (`noce` z pola `liczone_noce` albo `dni`); noc
//                      zdarzenia liczy się jako pierwsza. Zdarzenia nie było → niespełniona. Zmienne: n, data.
//  odpowiedzi_tygodni  pytanie (`sennosc`, `gorzej`), odpowiedz (`tak`, `nie`), kzn (k z n tygodni: tydzień 0 = 7 dni do
//                      `dzis`, ostatnia odpowiedź tygodnia; bez kzn: ostatnia odpowiedź do `dzis`). Zmienne: n, z.
//  zawsze              bez parametrów.
//  sygnal_komponentu   sygnal (nazwa w Sygnaly::stan); zmienne z Sygnaly::zmienne.
//  prog_biezacy        metryka (nazwa odczytu w Sygnaly::odczyty, np. M-25 bateria w %), prog, por. Zmienne: wartosc, x.

const NormaMetryki* normaMetryki(const Norma& n, const std::string& m) {
  if (m == "M-01") return &n.m01;
  if (m == "M-02") return &n.ahi;
  if (m == "M-03oa") return &n.oa;
  if (m == "M-03ca") return &n.ca;
  if (m == "M-03h") return &n.h;
  if (m == "M-05") return &n.cmed;
  if (m == "M-05p") return &n.cp95;
  if (m == "M-06") return &n.emed;
  if (m == "M-06p") return &n.ep95;
  if (m == "M-07") return &n.wp95;
  if (m == "M-28") return &n.psmed;
  if (m == "M-28p") return &n.psp95;
  if (m == "M-17") return &n.m17;
  return nullptr;
}

const StanMetryki* stanMetryki(const Stany& s, const std::string& m) {
  if (m == "M-01") return &s.m01;
  if (m == "M-02") return &s.ahi;
  if (m == "M-07") return &s.wp95;
  if (m == "M-06p") return &s.ep95;
  if (m == "M-03ca") return &s.ca;
  if (m == "M-17") return &s.m17;
  return nullptr;
}

const Norma* normaKontekstu(const Kontekst& k) { return k.norma ? k.norma : (k.stany ? &k.stany->norma : nullptr); }

// Norma osobista metryki, której nie ma w Norma (M-08, M-09, M-09p, M-10, M-10p, M-13): ta sama metoda co liczNorme()
// (noc.h, 4.4): ostatnie 30 nocy ważnych z wartością w 90 dniach przed `dzis` (noc oceniana poza normą), w trybie
// bieżącym, bez nocy z Kontekst::wykluczoneNoce (R-CZY-05), co najmniej 14 wartości; mediana i P10/P90 tymi
// samymi funkcjami. Bez wyłączenia nocy w stanie odchylenia (R-NOR-09), bo panel nie liczy stanu tych metryk.
NormaMetryki normaLokalna(const Kontekst& k, const std::string& m) {
  NormaMetryki nm;
  std::vector<double> v;
  std::string od = dodajDni(k.dzis, -90);
  std::vector<const Noc*> h = noceOkna(k, "wazne", 0);
  for (size_t i = h.size(); i-- > 0 && v.size() < 30;) {
    if (h[i]->data >= k.dzis || h[i]->data < od) continue;
    if (k.wykluczoneNoce && k.wykluczoneNoce->count(h[i]->data)) continue;  // R-CZY-05
    double x = metryka(*h[i], m);
    if (!std::isnan(x)) v.push_back(x);
  }
  nm.n = (int)v.size();
  if (!nm.jest()) return nm;
  nm.med = ::mediana(v);
  nm.p10 = percentyl(v, 10);
  nm.p90 = percentyl(v, 90);
  return nm;
}

// Norma metryki `m`: z kontekstu, a dla metryk bez pola w Norma lokalnie; false, gdy jej nie ma.
bool normaDla(const Kontekst& k, const std::string& m, NormaMetryki& wyj) {
  const Norma* n = normaKontekstu(k);
  const NormaMetryki* nm = n ? normaMetryki(*n, m) : nullptr;
  if (nm) {
    wyj = *nm;
  } else {
    if (!n && !k.historia) return false;
    if (!n) return false;  // bez normy w kontekście: rozruch albo wywołujący jej nie podał
    wyj = normaLokalna(k, m);
  }
  return wyj.jest() && !std::isnan(wyj.med);
}

// Punkt normy metryki według `wzgledem`; false i powód, gdy normy nie ma.
bool punktNormy(const WierszKatalogu& w, const Kontekst& k, const std::string& m, double& v, std::string& powod) {
  NormaMetryki nmw;
  if (!normaDla(k, m, nmw)) {
    powod = "brak normy " + m;
    return false;
  }
  const NormaMetryki* nm = &nmw;
  const std::string* wz = parametr(w, "wzgledem");
  std::string s = wz ? *wz : "mediana";
  if (s == "mediana") v = nm->med;
  else if (s == "p10") v = nm->p10;
  else if (s == "p90") v = nm->p90;
  else {
    powod = "nieznane wzgledem " + s;
    return false;
  }
  if (std::isnan(v)) {
    powod = "brak normy " + m;
    return false;
  }
  return true;
}

// Warunek `x {por} baza + delta` i/lub `x {por} baza × razy` (bez obu: `x {por} baza`).
bool wobecBazy(const WierszKatalogu& w, const Kontekst& k, const std::string& m, double x, const std::string& por,
               double baza, bool& wynik, std::string& powod) {
  bool jest = false;
  wynik = true;
  if (parametr(w, "delta")) {
    double d = 0;
    if (!progMetryki(w, k, "delta", m, d, powod)) return false;
    wynik = wynik && porownaj(x, por, baza + d);
    jest = true;
  }
  if (parametr(w, "razy")) {
    double r = 0;
    if (!wartoscProgu(w, k, "razy", r, powod)) return false;
    wynik = wynik && porownaj(x, por, baza * r);
    jest = true;
  }
  if (!jest) wynik = porownaj(x, por, baza);
  return true;
}

// Liczba nocy stanu odchylenia ≥ `min` (gdy parametr jest); false + powód przy braku stanu.
bool warunekMin(const WierszKatalogu& w, const Kontekst& k, const std::string& m, bool& wynik, std::string& powod) {
  wynik = true;
  if (!parametr(w, "min")) return true;
  double mn = 0;
  if (!wartoscProgu(w, k, "min", mn, powod)) return false;
  if (!k.stany) {
    powod = "brak stanow odchylenia";
    return false;
  }
  const StanMetryki* s = stanMetryki(*k.stany, m);
  if (!s) {
    powod = "stan " + m + " nie liczony";
    return false;
  }
  wynik = s->aktywny && porownaj(s->nocy, "ge", mn);
  return true;
}

Wynik rKolejneNoce(const WierszKatalogu& w, const Kontekst& k) {
  std::string m, powod;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  std::string por = porWiersza(w);
  if (!porZnane(por)) return nieoceniona(w, "zle por " + por);
  int n = 0;
  if (!liczbaNocy(w, k, "n", true, n, powod)) return nieoceniona(w, powod);
  bool wzgNormy = parametr(w, "wzgledem") || !parametr(w, "prog");
  double baza = 0;
  if (wzgNormy) {
    if (!punktNormy(w, k, m, baza, powod)) return nieoceniona(w, powod);
  } else if (!progMetryki(w, k, "prog", m, baza, powod)) {
    return nieoceniona(w, powod);
  }
  Okno o = oknoMetryki(w, k, m, n);
  if ((int)o.v.size() < n) return nieoceniona(w, zaMalo(o.v.size(), n));
  bool wszystkie = true;
  for (double x : o.v) {
    bool t = false;
    if (wzgNormy) {
      if (!wobecBazy(w, k, m, x, por, baza, t, powod)) return nieoceniona(w, powod);
    } else {
      t = porownaj(x, por, baza);
    }
    wszystkie = wszystkie && t;
  }
  Wynik r = oceniona(w, wszystkie);
  r.zmienne["z"] = std::to_string(n);
  r.zmienne["x"] = przecinek1(o.v.back());
  r.zmienne["data"] = o.noce.back()->data;
  return r;
}

Wynik rOknoWobecNormy(const WierszKatalogu& w, const Kontekst& k) {
  std::string m, powod;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  std::string por = porWiersza(w);
  if (!porZnane(por)) return nieoceniona(w, "zle por " + por);
  int n = 0;
  if (!liczbaNocy(w, k, "n", true, n, powod)) return nieoceniona(w, powod);
  double baza = 0;
  if (!punktNormy(w, k, m, baza, powod)) return nieoceniona(w, powod);
  Okno o = oknoMetryki(w, k, m, n);
  if ((int)o.v.size() < n) return nieoceniona(w, zaMalo(o.v.size(), n));
  const std::string* st = parametr(w, "stat");
  bool ok = false;
  double s = statystyka(st ? *st : "mediana", o.v, ok);
  if (!ok || std::isnan(s)) return nieoceniona(w, "zla statystyka");
  bool t = false;
  if (!wobecBazy(w, k, m, s, por, baza, t, powod)) return nieoceniona(w, powod);
  Wynik r = oceniona(w, t);
  r.zmienne["wartosc"] = przecinek1(s);
  r.zmienne["norma"] = przecinek1(baza);
  r.zmienne["z"] = std::to_string(n);
  r.zmienne["x"] = przecinek1(o.v.back());
  r.zmienne["data"] = o.noce.back()->data;
  return r;
}

// Norma metryki `m` dla nocy `data` (4.4, noc po nocy jak liczStany()): liczNorme() z wyłączeniami stanów odchylenia
// i nocy niepewnych z Kontekst::stany (R-NOR-09, R-NOR-19) i bez nocy z Kontekst::wykluczoneNoce (R-CZY-05), a dla
// metryk bez pola w Norma normaLokalna(). Bez zamrożenia normy z chwili wejścia w stan (R-NOR-22; uproszczenie).
bool normaNocy(const Kontekst& k, const Norma& nn, const std::string& m, const std::string& data, NormaMetryki& wyj) {
  const NormaMetryki* p = normaMetryki(nn, m);
  if (p) {
    wyj = *p;
  } else {
    Kontekst kd = k;
    kd.dzis = data;
    wyj = normaLokalna(kd, m);
  }
  return wyj.jest() && !std::isnan(wyj.med);
}

Norma normaDnia(const Kontekst& k, const std::string& data) {
  static const std::set<std::string> PUSTE;
  if (!k.historia) return Norma();
  return liczNorme(*k.historia, data, k.wykluczoneNoce ? *k.wykluczoneNoce : PUSTE,
                   k.stany ? &k.stany->wyl : nullptr);
}

// Odchylenie w jednej nocy (4.5): poza P90 i co najmniej Δ ponad medianę (`w górę`) albo poza P10 i co najmniej |Δ|
// poniżej (`w dół`: por = le albo Δ ujemne, np. M-01 −1,5 h).
bool odchylonaNoc(double x, const NormaMetryki& nm, bool wDol, double d) {
  double dd = std::fabs(d);
  return wDol ? (x < nm.p10 && porownaj(nm.med - x, "ge", dd)) : (x > nm.p90 && porownaj(x - nm.med, "ge", dd));
}

Wynik rOdchylenieNocy(const WierszKatalogu& w, const Kontekst& k) {
  std::string powod;
  // wariant „m z n sygnałów” (4.5, S-K-17): metryki=A+B+…, kzn = k z n sygnałów, delta_z = przedrostek id progu Δ
  // każdej metryki (np. R-NOR-12.delta → R-NOR-12.delta.M-13); noc z datą `dzis`. Sygnał bez wartości, normy albo
  // progu wypada z puli (R-DANE-28 zasada 2); gdy zostaje mniej niż k → „nie oceniam”.
  if (const std::string* lm = parametr(w, "metryki")) {
    const std::string* dz = parametr(w, "delta_z");
    if (!dz) return nieoceniona(w, "brak parametru delta_z");
    int kk = 0, nn = 0;
    if (!kZN(w, k, "kzn", kk, nn, powod)) return nieoceniona(w, powod);
    Tryb tn = Tryb::NIEZNANY;
    const Noc* n = nocDzis(k, w.liczoneNoce, tn);
    if (!n) return nieoceniona(w, "brak nocy " + k.dzis + " tego rodzaju");
    Norma nr = normaDnia(k, n->data);
    int pula = 0, ile = 0;
    std::string ktore;
    for (auto& m : dziel(*lm, '+')) {
      if (!znanaMetryka(m)) return nieoceniona(w, "nieznana metryka " + m);
      double x = metryka(*n, m);
      NormaMetryki nm;
      const Prog* pd = k.zestaw ? k.zestaw->prog(*dz + "." + m, k.trybBiezacy) : nullptr;
      if (std::isnan(x) || !normaNocy(k, nr, m, n->data, nm) || !pd || !pd->tekst.empty()) continue;
      double d = pd->wartosc;
      std::string pw;
      if (!przeliczNaMetryke(*pd, m, d, pw)) return nieoceniona(w, pw);
      pula++;
      if (odchylonaNoc(x, nm, d < 0, d)) {
        ile++;
        ktore += (ktore.empty() ? "" : ", ") + m;
      }
    }
    if (pula < kk) return nieoceniona(w, "za malo sygnalow " + std::to_string(pula) + "/" + std::to_string(kk));
    Wynik r = oceniona(w, ile >= kk);
    r.zmienne["n"] = std::to_string(ile);
    r.zmienne["z"] = std::to_string(pula);
    r.zmienne["sygnaly"] = ktore;
    return r;
  }
  std::string m;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  double d = 0;
  if (!progMetryki(w, k, "delta", m, d, powod)) return nieoceniona(w, powod);
  const std::string* pp = parametr(w, "por");
  std::string por = pp ? *pp : (d < 0 ? "le" : "ge");
  if (por != "ge" && por != "le") return nieoceniona(w, "odchylenie_nocy: por ge albo le");
  int kk = 1, nn = 1;
  if (parametr(w, "kzn") && !kZN(w, k, "kzn", kk, nn, powod)) return nieoceniona(w, powod);
  Okno o = oknoMetryki(w, k, m, nn);
  if ((int)o.v.size() < nn) return nieoceniona(w, zaMalo(o.v.size(), nn));
  if (!parametr(w, "kzn") && o.noce.back()->data != k.dzis) return nieoceniona(w, "brak nocy " + k.dzis);
  int ile = 0, bezNormy = 0;
  NormaMetryki ost;
  for (size_t i = 0; i < o.v.size(); i++) {
    NormaMetryki nm;
    if (!normaNocy(k, normaDnia(k, o.noce[i]->data), m, o.noce[i]->data, nm)) {
      bezNormy++;
      continue;
    }
    ost = nm;
    if (odchylonaNoc(o.v[i], nm, por == "le", d)) ile++;
  }
  // noce bez normy (rozruch metryki) to „nie wiem”: wynik pewny tylko, gdy nie mogą zmienić odpowiedzi
  if (ile < kk && ile + bezNormy >= kk) return nieoceniona(w, "brak normy " + m + " w " + std::to_string(bezNormy) + " nocach");
  bool mn = true;
  if (!warunekMin(w, k, m, mn, powod)) return nieoceniona(w, powod);
  Wynik r = oceniona(w, ile >= kk && mn);
  r.zmienne["n"] = std::to_string(ile);
  r.zmienne["z"] = std::to_string(nn);
  r.zmienne["x"] = przecinek1(o.v.back());
  r.zmienne["norma"] = ost.jest() ? przecinek1(ost.med) : "–";
  return r;
}

// ---------- rodzaje fali 3 ----------
//  trend               metryka, n, prog (nachylenie prostej MNK na noc, w jednostce metryki), por. Zmienne: wartosc.
//  rekord_historii     metryka, stat (mediana), n, por (le = najniższa, domyślnie; ge = najwyższa), min (opcjonalnie:
//                      tyle nocy tego rodzaju w historii co najmniej, np. O-03.historia). Statystyka ostatnich n nocy
//                      ściśle lepsza od każdego wcześniejszego okna n kolejnych nocy. Zmienne: wartosc, n (liczba okien).
//  kamien              prog (lista liczb `7,30,100`), liczone_noce (licznik nocy tego rodzaju do `dzis`), rocznica
//                      (`tak`: także rocznica pierwszej nocy w rekordach). Noc `dzis` musi być tego rodzaju.
//                      Zmienne: n (licznik albo liczba lat), data.
//  czas_poza_progiem   metryka (minuty poza progiem policzone przez AirBridge, np. M-08 duży wyciek), prog (minuty)
//                      i/lub prog_udzial (% czasu terapii M-01); noc `dzis`. Serie (CO2, SpO2) nie są w rekordzie nocy
//                      (T-DANE-02, T-DANE-03): metryka spoza rekordu → „nie oceniam”. Zmienne: x (minuty), wartosc (%).
//  percentyl_sezonu    metryka (odczyt w Sygnaly::odczyty), prog (x w %: górne x% sezonu); wartości sezonu
//                      w Sygnaly::serie (co najmniej 10). Spełniona, gdy odczyt ≥ percentyl (100 − x) sezonu
//                      (najbliższa ranga, ta sama funkcja co norma). Zmienne: wartosc.
//  porownanie_grup     metryka, cecha (`weekend`: noc z porankiem w sobotę albo niedzielę wobec pozostałych), n (okno
//                      nocy), min (najmniej nocy w grupie), delta (różnica średnich co najmniej), por (ge: grupa cechy
//                      wyżej). Test permutacyjny R-NF-24 według propozycji 9.3: PRZETASOWANIA przetasowań, ziarno
//                      = numer dnia `dzis` (wynik powtarzalny), poziom POZIOM_ISTOTNOSCI łącznie dla wszystkich
//                      wierszy tego rodzaju w jednym przebiegu z poprawką Holma–Bonferroniego (ocen(), po ocenie
//                      wszystkich). Zmienne: wartosc (różnica), p.

Wynik rTrend(const WierszKatalogu& w, const Kontekst& k) {
  std::string m, powod;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  std::string por = porWiersza(w);
  if (!porZnane(por)) return nieoceniona(w, "zle por " + por);
  int n = 0;
  if (!liczbaNocy(w, k, "n", true, n, powod)) return nieoceniona(w, powod);
  if (n < 2) return nieoceniona(w, "trend: co najmniej 2 noce");
  double prog = 0;
  if (!progMetryki(w, k, "prog", m, prog, powod)) return nieoceniona(w, powod);
  Okno o = oknoMetryki(w, k, m, n);
  if ((int)o.v.size() < n) return nieoceniona(w, zaMalo(o.v.size(), n));
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (int i = 0; i < n; i++) {
    double x = i, y = o.v[(size_t)i];
    sx += x;
    sy += y;
    sxx += x * x;
    sxy += x * y;
  }
  double nachylenie = (n * sxy - sx * sy) / (n * sxx - sx * sx);
  Wynik r = oceniona(w, porownaj(nachylenie, por, prog));
  r.zmienne["wartosc"] = liczbaTekst(std::round(nachylenie * 100) / 100);
  return r;
}

Wynik rRekordHistorii(const WierszKatalogu& w, const Kontekst& k) {
  std::string m, powod;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  const std::string* pp = parametr(w, "por");
  std::string por = pp ? *pp : "le";
  if (por != "le" && por != "ge") return nieoceniona(w, "rekord_historii: por le albo ge");
  int n = 0;
  if (!liczbaNocy(w, k, "n", true, n, powod)) return nieoceniona(w, powod);
  Okno o = oknoMetryki(w, k, m, 0);
  if (parametr(w, "min")) {
    double mn = 0;
    if (!wartoscProgu(w, k, "min", mn, powod)) return nieoceniona(w, powod);
    if ((double)o.v.size() < mn) return nieoceniona(w, zaMalo(o.v.size(), (int)mn) + " w historii");
  }
  if ((int)o.v.size() < n + 1) return nieoceniona(w, zaMalo(o.v.size(), n + 1));
  const std::string* st = parametr(w, "stat");
  std::string stat = st ? *st : "mediana";
  bool ok = false;
  std::vector<double> biez(o.v.end() - n, o.v.end());
  double teraz = statystyka(stat, biez, ok);
  if (!ok || std::isnan(teraz)) return nieoceniona(w, "zla statystyka");
  bool rekord = true;
  int okien = 0;
  for (size_t kon = (size_t)n; kon < o.v.size(); kon++) {  // okna kończące się przed ostatnią nocą
    std::vector<double> v(o.v.begin() + (long)(kon - (size_t)n), o.v.begin() + (long)kon);
    double s = statystyka(stat, v, ok);
    okien++;
    if (por == "le" ? !(teraz < s - 1e-9) : !(teraz > s + 1e-9)) rekord = false;
  }
  Wynik r = oceniona(w, rekord);
  r.zmienne["wartosc"] = przecinek1(teraz);
  r.zmienne["n"] = std::to_string(okien);
  return r;
}

Wynik rKamien(const WierszKatalogu& w, const Kontekst& k) {
  std::string powod;
  const Prog* p = progParametru(w, k, "prog", powod);
  if (!p) return nieoceniona(w, powod);
  std::vector<double> lista_;
  if (p->tekst.empty()) lista_.push_back(p->wartosc);  // jedna wartość (lista z jednej liczby)
  for (auto& x : lista(p->tekst)) {
    double v = 0;
    if (!liczbaZ(x, v)) return nieoceniona(w, "prog " + p->id + ": lista liczb");
    lista_.push_back(v);
  }
  std::vector<const Noc*> h = noceOkna(k, w.liczoneNoce, 0);
  bool znane = false;
  std::vector<NocTrybu> wsz = nocePosortowane(k, znane);
  if (wsz.empty()) return nieoceniona(w, "brak nocy w rekordach");
  if (h.empty() || h.back()->data != k.dzis) return oceniona(w, false);  // dziś bez nocy tego rodzaju
  int ile = (int)h.size();
  for (double v : lista_)
    if (std::fabs(v - ile) < 1e-9) {
      Wynik r = oceniona(w, true);
      r.zmienne["n"] = std::to_string(ile);
      r.zmienne["data"] = k.dzis;
      return r;
    }
  const std::string* ro = parametr(w, "rocznica");
  const std::string& pierwsza = wsz.front().n->data;
  if (ro && *ro == "tak" && pierwsza.substr(5) == k.dzis.substr(5) && pierwsza < k.dzis) {
    Wynik r = oceniona(w, true);
    r.zmienne["n"] = std::to_string(std::stoi(k.dzis.substr(0, 4)) - std::stoi(pierwsza.substr(0, 4)));
    r.zmienne["data"] = pierwsza;
    return r;
  }
  return oceniona(w, false);
}

Wynik rCzasPozaProgiem(const WierszKatalogu& w, const Kontekst& k) {
  std::string m, powod;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  if (std::string(jednostkaMetryki(m) ? jednostkaMetryki(m) : "") != "min")
    return nieoceniona(w, "czas_poza_progiem: metryka w minutach (seria nie jest w rekordzie nocy)");
  if (!parametr(w, "prog") && !parametr(w, "prog_udzial")) return nieoceniona(w, "brak parametru prog albo prog_udzial");
  Tryb tn = Tryb::NIEZNANY;
  const Noc* n = nocDzis(k, w.liczoneNoce, tn);
  if (!n) return nieoceniona(w, "brak nocy " + k.dzis + " tego rodzaju");
  double x = metryka(*n, m);
  if (std::isnan(x)) return nieoceniona(w, "brak metryki " + m);
  bool t = false;
  double udz = n->m01 > 0 ? 100.0 * x / n->m01 : NAN;
  if (parametr(w, "prog")) {
    double p = 0;
    if (!progMetryki(w, k, "prog", m, p, powod)) return nieoceniona(w, powod);
    t = t || porownaj(x, "ge", p);
  }
  if (parametr(w, "prog_udzial")) {
    double p = 0;
    if (!wartoscProgu(w, k, "prog_udzial", p, powod)) return nieoceniona(w, powod);
    if (std::isnan(udz) && !t) return nieoceniona(w, "brak czasu terapii");
    t = t || porownaj(udz, "ge", p);
  }
  Wynik r = oceniona(w, t);
  r.zmienne["x"] = calkowita(x);
  r.zmienne["wartosc"] = calkowita(udz);
  return r;
}

Wynik rPercentylSezonu(const WierszKatalogu& w, const Kontekst& k) {
  std::string m = metrykaWiersza(w), powod;
  if (m.empty()) return nieoceniona(w, "brak parametru metryka");
  const Sygnaly* sg = sygnalyOceny();
  if (!sg) return nieoceniona(w, "brak odczytow biezacych");
  auto o = sg->odczyty.find(m);
  auto se = sg->serie.find(m);
  if (o == sg->odczyty.end() || std::isnan(o->second)) return nieoceniona(w, "brak odczytu " + m);
  if (se == sg->serie.end() || se->second.size() < 10) return nieoceniona(w, "za malo wartosci sezonu " + m);
  double x = 0;
  if (!wartoscProgu(w, k, "prog", x, powod)) return nieoceniona(w, powod);
  if (x <= 0 || x >= 100) return nieoceniona(w, "prog procentu sezonu poza 0–100");
  double granica = percentyl(se->second, (int)std::lround(100 - x));
  Wynik r = oceniona(w, porownaj(o->second, "ge", granica));
  r.zmienne["wartosc"] = liczbaTekst(o->second);
  return r;
}

Wynik rPorownanieGrup(const WierszKatalogu& w, const Kontekst& k) {
  std::string m, powod;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  const std::string* ce = parametr(w, "cecha");
  if (!ce || *ce != "weekend") return nieoceniona(w, "porownanie_grup: cecha weekend (inne cechy bez danych w kontekscie)");
  std::string por = porWiersza(w);
  if (por != "ge" && por != "le") return nieoceniona(w, "porownanie_grup: por ge albo le");
  int n = 0;
  if (!liczbaNocy(w, k, "n", true, n, powod)) return nieoceniona(w, powod);
  double mn = 0, d = 0;
  if (!wartoscProgu(w, k, "min", mn, powod) || !progMetryki(w, k, "delta", m, d, powod)) return nieoceniona(w, powod);
  Okno o = oknoMetryki(w, k, m, n);
  std::vector<double> a, b;
  for (size_t i = 0; i < o.v.size(); i++) {
    int dt = dzienTygodnia(o.noce[i]->data);
    (dt == 0 || dt == 6 ? a : b).push_back(o.v[i]);
  }
  if ((double)a.size() < mn || (double)b.size() < mn)
    return nieoceniona(w, "grupy " + std::to_string(a.size()) + " i " + std::to_string(b.size()) + " nocy");
  auto srednia = [](const std::vector<double>& v) {
    double s = 0;
    for (double x : v) s += x;
    return s / (double)v.size();
  };
  double obs = srednia(a) - srednia(b);
  double kier = por == "ge" ? 1 : -1;
  std::vector<double> wsz = a;
  wsz.insert(wsz.end(), b.begin(), b.end());
  std::mt19937 los((uint32_t)nrDnia(k.dzis));
  int ileWiecej = 0;
  for (int t = 0; t < PRZETASOWANIA; t++) {
    // własny Fisher–Yates na surowym wyjściu mt19937 (std::shuffle zależy od biblioteki: libc++ na
    // komputerze i libstdc++ na panelu dawały różne przetasowania), jak odkrycia::testPermutacyjny
    for (size_t i = wsz.size(); i > 1; i--) std::swap(wsz[i - 1], wsz[(size_t)(los() % (uint32_t)i)]);
    std::vector<double> pa(wsz.begin(), wsz.begin() + (long)a.size()), pb(wsz.begin() + (long)a.size(), wsz.end());
    if (kier * (srednia(pa) - srednia(pb)) >= kier * obs - 1e-12) ileWiecej++;
  }
  double pw = (ileWiecej + 1.0) / (PRZETASOWANIA + 1.0);
  bool kandydat = porownaj(kier * obs, "ge", std::fabs(d));
  Wynik r = oceniona(w, kandydat);  // ostateczna decyzja po poprawce Holma w ocen()
  r.zmienne["wartosc"] = przecinek1(obs);
  r.zmienne["p"] = std::to_string(pw);
  r.zmienne["holm"] = "tak";
  return r;
}

Wynik rStanOdchylenia(const WierszKatalogu& w, const Kontekst& k) {
  std::string m = metrykaWiersza(w), powod;
  if (m.empty()) return nieoceniona(w, "brak parametru metryka");
  if (!k.stany) return nieoceniona(w, "brak stanow odchylenia");
  const StanMetryki* s = stanMetryki(*k.stany, m);
  if (!s) return nieoceniona(w, "stan " + m + " nie liczony");
  // przeglądu stan bez normy tej metryki to „nie wiem”, nie „bez odchylenia” (niezależnie od wymaga_normy)
  const NormaMetryki* nm = normaMetryki(k.stany->norma, m);
  if (!nm || !nm->jest()) return nieoceniona(w, "brak normy " + m);
  bool mn = true;
  if (!warunekMin(w, k, m, mn, powod)) return nieoceniona(w, powod);
  Wynik r = oceniona(w, s->aktywny && mn);
  r.zmienne["od"] = s->od;
  r.zmienne["nocy"] = std::to_string(s->nocy);
  r.zmienne["n"] = std::to_string(s->nocy);  // `{n}` w tekstach E-21
  r.zmienne["norma"] = przecinek1(nm->med);
  // `{x}` to średnia z 5 ostatnich nocy ważnych z wartością (teksty 6.4: `Średnio {x},
  // zwykle {norma} (ostatnie 5 nocy).` w E-13, `Średnio {x} / h` w E-31; okno wejścia w stan, 4.6), dla AHI bez nocy
  // niepewnych (R-NOR-19); format metryki (np. czas `7 h 12`) daje dopiero tekst(), zmienna zostaje liczbą
  std::vector<const Noc*> v = noceOkna(k, w.liczoneNoce.empty() ? "wazne" : w.liczoneNoce, 0);
  std::vector<double> ost;
  for (size_t i = v.size(); i > 0 && ost.size() < 5; i--) {
    double x = metryka(*v[i - 1], m);
    if (std::isnan(x) || (m == "M-02" && nocNiepewna(*v[i - 1]))) continue;
    ost.push_back(x);
  }
  double suma = 0;
  for (double x : ost) suma += x;
  r.zmienne["x"] = ost.empty() ? "–" : przecinek1(suma / (double)ost.size());
  return r;
}

// Data ostatniej zmiany ustawień do `dzis` ("" = nie było): noc z niepustymi ustawieniami innymi niż poprzednie znane.
std::string zmianaUstawien(const Kontekst& k) {
  if (!k.historia) return "";
  std::vector<const Noc*> h;
  for (auto& n : *k.historia)
    if (dataOk(n.data) && n.data <= k.dzis) h.push_back(&n);
  std::stable_sort(h.begin(), h.end(), [](const Noc* a, const Noc* b) { return a->data < b->data; });
  std::string ost, data;
  for (const Noc* n : h) {
    if (n->ustawienia.empty()) continue;
    if (!ost.empty() && n->ustawienia != ost) data = n->data;
    ost = n->ustawienia;
  }
  return data;
}

Wynik rPorownanieOkien(const WierszKatalogu& w, const Kontekst& k) {
  std::string m, powod;
  if (!wymaganaMetryka(w, m, powod)) return nieoceniona(w, powod);
  std::string por = porWiersza(w);
  if (!porZnane(por)) return nieoceniona(w, "zle por " + por);
  int a = 0, b = 0;
  if (!liczbaNocy(w, k, "okno_a", false, a, powod) || !liczbaNocy(w, k, "okno_b", false, b, powod))
    return nieoceniona(w, powod);
  Okno o = oknoMetryki(w, k, m, 0);
  std::vector<double> va, vb;
  const std::string* zd = parametr(w, "zdarzenie");
  if (zd) {
    if (*zd != "zmiana_ustawien") return nieoceniona(w, "porownanie_okien: zdarzenie " + *zd + " nieobslugiwane");
    std::string od = zmianaUstawien(k);
    if (od.empty()) return nieoceniona(w, "brak zmiany ustawien");
    size_t p = 0;
    while (p < o.noce.size() && o.noce[p]->data < od) p++;
    if (o.v.size() - p < (size_t)a) return nieoceniona(w, zaMalo(o.v.size() - p, a) + " po zmianie");
    if (p < (size_t)b) return nieoceniona(w, zaMalo(p, b) + " przed zmiana");
    va.assign(o.v.begin() + (long)p, o.v.begin() + (long)p + a);
    vb.assign(o.v.begin() + (long)p - b, o.v.begin() + (long)p);
  } else {
    if (o.v.size() < (size_t)a + (size_t)b) return nieoceniona(w, zaMalo(o.v.size(), a + b));
    va.assign(o.v.end() - a, o.v.end());
    vb.assign(o.v.end() - a - b, o.v.end() - a);
  }
  const std::string* st = parametr(w, "stat");
  bool ok1 = false, ok2 = false;
  double sa = statystyka(st ? *st : "mediana", va, ok1), sb = statystyka(st ? *st : "mediana", vb, ok2);
  if (!ok1 || !ok2 || std::isnan(sa) || std::isnan(sb)) return nieoceniona(w, "zla statystyka");
  bool t = false;
  if (!wobecBazy(w, k, m, sa, por, sb, t, powod)) return nieoceniona(w, powod);
  Wynik r = oceniona(w, t);
  r.zmienne["wartosc"] = przecinek1(sa);
  r.zmienne["wartosc_b"] = przecinek1(sb);
  r.zmienne["z"] = std::to_string(a);
  return r;
}

// Liczba od `od` do `dzis` włącznie w jednostce wiersza (`dni` albo `noce` rodzaju z `liczone_noce`).
bool ileOd(const WierszKatalogu& w, const Kontekst& k, const std::string& od, const char* domyslna, int& ile,
           std::string& powod) {
  const std::string* j = parametr(w, "jednostka");
  std::string jd = j ? *j : domyslna;
  if (jd == "dni") {
    ile = dniMiedzy(od, k.dzis) + 1;
    return true;
  }
  if (jd == "noce") {
    ile = 0;
    for (const Noc* n : noceOkna(k, w.liczoneNoce, 0))
      if (n->data >= od) ile++;
    return true;
  }
  powod = "nieznana jednostka " + jd;
  return false;
}

// min ≤ ile ≤ max (co najmniej jedna granica).
bool wGranicach(const WierszKatalogu& w, const Kontekst& k, int ile, bool& t, std::string& powod) {
  bool jest = false;
  t = true;
  for (const char* g : {"min", "max"}) {
    if (!parametr(w, g)) continue;
    double v = 0;
    if (!wartoscProgu(w, k, g, v, powod)) return false;
    t = t && porownaj(ile, g[1] == 'i' ? "ge" : "le", v);
    jest = true;
  }
  if (!jest) {
    powod = "brak parametru min albo max";
    return false;
  }
  return true;
}

Wynik rTrwanie(const WierszKatalogu& w, const Kontekst& k) {
  const std::string* reg = parametr(w, "regula");
  if (!reg) return nieoceniona(w, "brak parametru regula");
  if (!regulaZnana(k, *reg)) return nieoceniona(w, "brak reguly " + *reg + " przed ta regula");
  std::string powod;
  Trzy t = stanReguly(k, *reg, powod);
  if (t == Trzy::NIEZNANE) return nieoceniona(w, powod);
  if (!k.pamiec) return nieoceniona(w, "brak pamieci regul");
  if (t == Trzy::FALSZ) return oceniona(w, false);
  // początek: najwcześniejsze `aktywna_od` z pamięci; dla `regula=id` reguły z wariantami: najdłużej trwający
  // spełniony wariant
  std::string od = k.dzis;
  auto poczatek = [&](const std::string& klucz) {
    auto it = k.pamiec->find(klucz);
    if (it != k.pamiec->end() && dataOk(it->second.aktywnaOd) && it->second.aktywnaOd < od) od = it->second.aktywnaOd;
  };
  if (k.wczesniejsze->count(*reg)) {
    poczatek(*reg);
  } else {
    for (auto& kv : *k.wczesniejsze)
      if (kv.second.id == *reg && kv.second.oceniona && kv.second.spelniona) poczatek(kv.first);
  }
  int ile = 0;
  if (!ileOd(w, k, od, "dni", ile, powod)) return nieoceniona(w, powod);
  bool g = false;
  if (!wGranicach(w, k, ile, g, powod)) return nieoceniona(w, powod);
  Wynik r = oceniona(w, g);
  r.zmienne["n"] = std::to_string(ile);
  r.zmienne["od"] = od;
  return r;
}

Wynik rOdZdarzenia(const WierszKatalogu& w, const Kontekst& k) {
  const std::string* zd = parametr(w, "zdarzenie");
  if (!zd) return nieoceniona(w, "brak parametru zdarzenie");
  std::string data, powod;
  if (*zd == "pierwsza_noc") {
    if (k.historia)
      for (auto& n : *k.historia)
        if (dataOk(n.data) && n.data <= k.dzis && (data.empty() || n.data < data)) data = n.data;
    if (data.empty()) return nieoceniona(w, "brak nocy w rekordach");
  } else if (*zd == "zmiana_ustawien") {
    data = zmianaUstawien(k);
  } else if (*zd == "poczatek_leczenia") {
    // R-SKI-08: data z ustawienia „Początek leczenia”; pusta, nieczytelna albo późniejsza niż dziś = zdarzenia nie było
    // (bez karencji, fail-path 5.12.10)
    if (dataOk(k.poczatekLeczenia) && k.poczatekLeczenia <= k.dzis) data = k.poczatekLeczenia;
  } else if (*zd == "nowy_poziom") {
    return nieoceniona(w, "nowy_poziom: komponent stanow go nie podaje");
  } else {
    return nieoceniona(w, "nieznane zdarzenie " + *zd);
  }
  bool jestGranica = parametr(w, "min") || parametr(w, "max");
  if (!jestGranica) return nieoceniona(w, "brak parametru min albo max");
  if (data.empty()) return oceniona(w, false);
  int ile = 0;
  if (!ileOd(w, k, data, "noce", ile, powod)) return nieoceniona(w, powod);
  bool g = false;
  if (!wGranicach(w, k, ile, g, powod)) return nieoceniona(w, powod);
  Wynik r = oceniona(w, g);
  r.zmienne["n"] = std::to_string(ile);
  r.zmienne["data"] = data;
  return r;
}

Wynik rOdpowiedziTygodni(const WierszKatalogu& w, const Kontekst& k) {
  const std::string* py = parametr(w, "pytanie");
  const std::string* od = parametr(w, "odpowiedz");
  if (!py || !od) return nieoceniona(w, "brak parametru pytanie albo odpowiedz");
  if (*od != "tak" && *od != "nie") return nieoceniona(w, "zla odpowiedz " + *od);
  if (!k.odpowiedzi) return nieoceniona(w, "brak odpowiedzi w kontekscie");
  const std::map<std::string, bool>* m = nullptr;
  if (*py == "sennosc") m = &k.odpowiedzi->sennosc;
  else if (*py == "gorzej") m = &k.odpowiedzi->gorzej;
  else return nieoceniona(w, "nieznane pytanie " + *py);
  bool chce = *od == "tak";
  std::string powod;
  if (!parametr(w, "kzn")) {
    const std::pair<const std::string, bool>* ost = nullptr;
    for (auto& kv : *m)
      if (dataOk(kv.first) && kv.first <= k.dzis) ost = &kv;
    if (!ost) return nieoceniona(w, "brak odpowiedzi " + *py);
    Wynik r = oceniona(w, ost->second == chce);
    r.zmienne["data"] = ost->first;
    return r;
  }
  int kk = 0, nn = 0;
  if (!kZN(w, k, "kzn", kk, nn, powod)) return nieoceniona(w, powod);
  std::map<int, std::pair<std::string, bool>> tyg;  // tydzień wstecz → (data, odpowiedź) ostatniej odpowiedzi
  for (auto& kv : *m) {
    if (!dataOk(kv.first) || kv.first > k.dzis) continue;
    int t = dniMiedzy(kv.first, k.dzis) / 7;
    if (t >= nn) continue;
    auto it = tyg.find(t);
    if (it == tyg.end() || it->second.first < kv.first) tyg[t] = {kv.first, kv.second};
  }
  // przeglądu tydzień bez odpowiedzi nie jest „nie”; wynik pewny tylko, gdy znane tygodnie przesądzają
  int ile = 0, inne = 0;
  for (auto& kv : tyg) {
    if (kv.second.second == chce) ile++;
    else inne++;
  }
  int nieznane = nn - ile - inne;
  if (ile < kk && ile + nieznane >= kk)
    return nieoceniona(w, "za malo odpowiedzi " + *py + ": " + std::to_string(ile + inne) + " z " +
                              std::to_string(nn) + " tygodni");
  Wynik r = oceniona(w, ile >= kk);
  r.zmienne["n"] = std::to_string(ile);
  r.zmienne["z"] = std::to_string(nn);
  return r;
}

Wynik rZawsze(const WierszKatalogu& w, const Kontekst&) { return oceniona(w, true); }

Wynik rSygnalKomponentu(const WierszKatalogu& w, const Kontekst&) {
  const std::string* s = parametr(w, "sygnal");
  if (!s) return nieoceniona(w, "brak parametru sygnal");
  const Sygnaly* sg = sygnalyOceny();
  if (!sg) return nieoceniona(w, "brak sygnalow komponentow");
  auto it = sg->stan.find(*s);
  if (it == sg->stan.end()) return nieoceniona(w, "brak sygnalu " + *s);
  Wynik r = oceniona(w, it->second);
  for (auto& kv : sg->zmienne) r.zmienne[kv.first] = kv.second;
  return r;
}

Wynik rProgBiezacy(const WierszKatalogu& w, const Kontekst& k) {
  std::string m = metrykaWiersza(w), powod;
  if (m.empty()) return nieoceniona(w, "brak parametru metryka");
  std::string por = porWiersza(w);
  if (!porZnane(por)) return nieoceniona(w, "zle por " + por);
  const Sygnaly* sg = sygnalyOceny();
  if (!sg) return nieoceniona(w, "brak odczytow biezacych");
  auto it = sg->odczyty.find(m);
  if (it == sg->odczyty.end() || std::isnan(it->second)) return nieoceniona(w, "brak odczytu " + m);
  double p = 0;
  if (!wartoscProgu(w, k, "prog", p, powod)) return nieoceniona(w, powod);
  Wynik r = oceniona(w, porownaj(it->second, por, p));
  r.zmienne["wartosc"] = liczbaTekst(it->second);
  r.zmienne["x"] = liczbaTekst(it->second);
  return r;
}

}  // namespace

namespace wewn {
std::map<std::string, FunkcjaRodzaju> rdzen() {
  std::map<std::string, FunkcjaRodzaju> r;
  r["prog_nocy"] = rProgNocy;
  r["k_z_n_nocy"] = rKZNNocy;
  r["statystyka_okna"] = rStatystykaOkna;
  r["zlozenie"] = rZlozenie;
  r["kolejne_noce"] = rKolejneNoce;
  r["okno_wobec_normy"] = rOknoWobecNormy;
  r["odchylenie_nocy"] = rOdchylenieNocy;
  r["stan_odchylenia"] = rStanOdchylenia;
  r["porownanie_okien"] = rPorownanieOkien;
  r["trwanie"] = rTrwanie;
  r["od_zdarzenia"] = rOdZdarzenia;
  r["odpowiedzi_tygodni"] = rOdpowiedziTygodni;
  r["zawsze"] = rZawsze;
  r["sygnal_komponentu"] = rSygnalKomponentu;
  r["prog_biezacy"] = rProgBiezacy;
  r["trend"] = rTrend;
  r["rekord_historii"] = rRekordHistorii;
  r["kamien"] = rKamien;
  r["czas_poza_progiem"] = rCzasPozaProgiem;
  r["porownanie_grup"] = rPorownanieGrup;
  r["percentyl_sezonu"] = rPercentylSezonu;
  return r;
}
}  // namespace wewn

}  // namespace reguly
