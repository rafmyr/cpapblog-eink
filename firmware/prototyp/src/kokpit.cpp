// SPDX-License-Identifier: GPL-3.0-or-later
#include "kokpit.h"
#include "stany.h"
#include "wspolne.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace {

int liczba(const std::string& s) { return liczbaCalk(s.c_str()); }  // bez wyjątków std::stoi

enum Metryka : std::uint8_t { M_BRAK, M_M01, M_AHI, M_CMED, M_CP95, M_WP95, M_EMED, M_EP95, M_PSMED, M_OA, M_CA, M_H, M_M17, M_M13 };

// EPAP z EDF (R-NOR-08): wartość ze STR (EPAP docelowe urządzenia) nie stoi w linii 30 nocy, w medianach ani w K2
// (w K2 z dopiskiem `(urządzenie)`); K1 pokazuje ją tylko w liczbie ostatniej nocy z dopiskiem (R-KOK-15).
double wartosc(const Noc* n, Metryka m) {
  if (!n) return NAN;
  switch (m) {
    case M_M01: return n->m01 >= 0 ? (double)n->m01 : NAN;
    case M_AHI: return n->ahi;
    case M_CMED: return n->cmed;
    case M_CP95: return n->cp95;
    case M_WP95: return n->wp95;
    case M_EMED: return epapMed(*n);
    case M_EP95: return epapP95(*n);
    case M_PSMED: return n->psmed;
    case M_OA: return n->oa;
    case M_CA: return n->ca;
    case M_H: return n->h;
    case M_M17: return n->m17 >= 0 ? (double)n->m17 : NAN;  // zdjęcia maski
    case M_M13: return n->m13;                                  // oddechy na minutę
    default: return NAN;
  }
}

const NormaMetryki* normaMetryki(const Norma& nm, Metryka m) {
  switch (m) {
    case M_M01: return &nm.m01;
    case M_AHI: return &nm.ahi;
    case M_CMED: return &nm.cmed;
    case M_CP95: return &nm.cp95;
    case M_WP95: return &nm.wp95;
    case M_EMED: return &nm.emed;
    case M_EP95: return &nm.ep95;
    case M_PSMED: return &nm.psmed;
    case M_OA: return &nm.oa;
    case M_CA: return &nm.ca;
    case M_H: return &nm.h;
    case M_M17: return &nm.m17;
    default: return nullptr;  // M-13: panel nie liczy normy (src/noc.h)
  }
}

// wartość zaokrąglona tak jak na ekranie (format niżej): minuty, całe L/min, jedno miejsce po przecinku
double zaokr(Metryka m, double v) {
  if (m == M_M01 || m == M_WP95 || m == M_M17 || m == M_M13) return std::floor(v + 0.5 + 1e-4);
  return std::floor(v * 10 + 0.5 + 1e-4) / 10;
}

std::string format(Metryka m, double v) {
  switch (m) {
    case M_M01: return godzMin(v);
    case M_WP95:
    case M_M17:
    case M_M13: return calkowita(v);  // `9`, `1`, `14` (5.5.3)
    default: return przecinek1(v);
  }
}

const StanMetryki* stanMetryki(const Stany* st, Metryka m) {
  if (!st) return nullptr;
  switch (m) {
    case M_M01: return &st->m01;
    case M_AHI: return &st->ahi;
    case M_WP95: return &st->wp95;
    case M_EP95: return &st->ep95;
    case M_CA: return &st->ca;    // wiersz `centralne` (R-KOK-09)
    case M_M17: return &st->m17;  // wiersz `Zdjęcia maski`
    default: return nullptr;
  }
}

// ---------- tryb terapii (4.11, ADR-0030) ----------
// Tryb nocy z pierwszego pola ustawień (tag `MOP`). T_BRAK: bez rekordu albo bez znanych ustawień; T_OBCY: tryb spoza
// wydania albo nieznany (R-DANE-29).
enum TrybNocy : std::uint8_t { T_BRAK, T_CPAP, T_AUTOSET, T_ASV, T_ASVAUTO, T_OBCY };

TrybNocy trybNocyZUstawien(const std::string& u) {
  if (u.empty()) return T_BRAK;
  std::string m = u.substr(0, u.find('/'));
  if (m == "CPAP") return T_CPAP;
  if (m == "AutoSet") return T_AUTOSET;
  if (m == "ASV") return T_ASV;
  if (m == "ASVAuto") return T_ASVAUTO;
  return T_OBCY;
}

bool obslugiwany(TrybNocy t) { return t >= T_CPAP && t <= T_ASVAUTO; }

// Tryby nocy okna 30 nocy kończącego się na `koniec` (indeks 0 = koniec − 29) i tryb bieżący: tryb ostatniej nocy
// w trybie obsługiwanym do `koniec` włącznie. Tryb nocy: trybNocy() z src/noc.h (pole `tryb`, inaczej pierwszy człon
// `ustawienia`). Bez odczytu trybu: noc z importu karty (źródło K) ma tryb ostatniej znanej wcześniej nocy (jak R-NOR-07),
// a noc z AirBridge (źródło A) to tryb nieznany, czyli noc jak w trybie spoza wydania, poza oknami (R-DANE-29,
// R2).
struct TrybyOkna {
  TrybNocy biezacy = T_BRAK;  // T_BRAK: żadna noc nie ma trybu obsługiwanego → układ ASV bez filtra
  TrybNocy noc[30];
  // Czy wartość nocy `i` wchodzi do wiersza zależnego od trybu (ciśnienie, EPAP, wsparcie): tylko noce w trybie
  // bieżącym (fail-path R-KOK-15); bez znanego trybu bieżącego wszystkie.
  bool zgodna(int i) const { return biezacy == T_BRAK || noc[i] == biezacy; }
  bool asv() const { return biezacy == T_BRAK || biezacy == T_ASV || biezacy == T_ASVAUTO; }
};

TrybyOkna trybyOkna(const std::vector<Noc>& hist, const std::string& koniec) {
  TrybyOkna t;
  for (TrybNocy& n : t.noc) n = T_BRAK;
  if (!dataOk(koniec)) return t;
  HistoriaPos hp = posortujHistorie(hist);
  TrybNocy znany = T_BRAK;
  for (size_t i = 0; i < hp.noce.size(); i++) {
    const Noc* n = hp.noce[i];
    if (n->data > koniec) break;
    std::string tn = trybNocy(*n);
    TrybNocy m;
    if (!tn.empty()) {
      m = trybNocyZUstawien(tn);
      znany = m;
    } else {
      m = n->zrodlo == 'K' ? znany : T_OBCY;
    }
    if (obslugiwany(m)) t.biezacy = m;
    int k = 29 - dniMiedzy(n->data, koniec);
    if (k >= 0 && k < 30) t.noc[k] = m;
  }
  return t;
}

// bez żadnej nocy tryb bieżący z `/api/status` (`mode`), jeśli jest obsługiwany; inaczej układ ASV jak dotąd.
void trybZeStatusu(TrybyOkna& t, const std::string& status) {
  TrybNocy m = trybNocyZUstawien(status);
  if (t.biezacy == T_BRAK && obslugiwany(m)) t.biezacy = m;
}

// Metryka wiersza zależy od trybu (wiersze `Ciśnienie`, `EPAP`, `Wsparcie` w K1 i K2).
bool zaleznaOdTrybu(Metryka m) { return m == M_CMED || m == M_CP95 || m == M_EMED || m == M_EP95 || m == M_PSMED; }

// Δ minimalnego efektu z 4.5 (i 4.11 w trybie ASV) dla wierszy K1 [PRÓG]; NAN = metryka bez Δ. M-28 (wsparcie PS) nie
// ma Δ w 4.5, ale do słowa trendu bierze Δ EPAP (R-KOK-16.delta_ps).
// Ciśnienie (M-05 mediana) i EPAP (M-06 mediana) biorą Δ sygnału P95 (M-05p, M-06p).
double deltaTrendu(Metryka m) {
  switch (m) {
    case M_AHI: return 3.0;    // R-NOR-12.delta.M-02, / h
    case M_M01: return 90.0;   // R-NOR-12.delta.M-01, 1,5 h w minutach (kierunek bez znaczenia dla trendu)
    case M_CMED:               // R-NOR-12.delta.M-05p, cmH2O
    case M_EMED: return 2.0;   // R-NOR-12.delta.M-06p, cmH2O
    case M_WP95: return 10.0;  // R-NOR-12.delta.M-07, L/min
    case M_PSMED: return TREND_DELTA_PS;  // R-KOK-16.delta_ps, cmH2O jak EPAP
    default: return NAN;
  }
}

Linia30 linia(const std::vector<Noc>& hist, const std::string& koniec, Metryka m, const TrybyOkna* tr = nullptr) {
  Linia30 l;
  for (int i = 0; i < 30; i++) {
    double v = wartosc(nocZDnia(hist, dodajDni(koniec, i - 29)), m);
    if (tr && zaleznaOdTrybu(m) && !tr->zgodna(i)) v = NAN;  // fail-path R-KOK-15: inny tryb niż bieżący
    l.ma[i] = !std::isnan(v);
    l.v[i] = l.ma[i] ? (float)v : 0;
  }
  return l;
}

// R-KOK-16: słowo trendu z linii 30 nocy (te same noce); "" = za mało nocy w którymś oknie albo metryka bez Δ.
std::string slowoTrendu(const std::vector<Noc>& hist, const std::string& koniec, const Linia30& l, Metryka m,
                        const TrybyOkna& tr) {
  double delta = deltaTrendu(m);
  if (std::isnan(delta)) return "";  // M-28 bez Δ: bez reguły trendu, więc bez słowa
  std::vector<double> stare, nowe;
  for (int i = 0; i < 30; i++) {
    if (!l.ma[i] || tr.noc[i] == T_OBCY) continue;  // R-DANE-29: noc w trybie spoza wydania bez oceny
    if (m == M_AHI) {  // R-NOR-19: noc niepewna poza AHI jak w normie
      const Noc* n = nocZDnia(hist, dodajDni(koniec, i - 29));
      if (n && nocNiepewna(*n)) continue;
    }
    (i >= 23 ? nowe : stare).push_back(l.v[i]);
  }
  if ((int)nowe.size() < TREND_NOCY_7 || (int)stare.size() < TREND_NOCY_23) return "";
  double roznica = mediana(nowe) - mediana(stare), prog = TREND_CZESC_DELTY * delta;
  if (roznica >= prog - 1e-4) return "rośnie";
  if (roznica <= -prog + 1e-4) return "spada";
  return "stabilnie";
}

}  // namespace

reguly::Tryb trybBiezacy(const std::vector<Noc>& hist, const std::string& koniec) {
  switch (trybyOkna(hist, koniec).biezacy) {
    case T_CPAP: return reguly::Tryb::CPAP;
    case T_AUTOSET: return reguly::Tryb::AUTOSET;
    case T_ASV: return reguly::Tryb::ASV;
    case T_ASVAUTO: return reguly::Tryb::ASVAUTO;
    default: return reguly::Tryb::NIEZNANY;
  }
}

DaneK1 budujK1(const std::vector<Noc>& hist, const std::string& dzis, const Norma& nm,
               const std::vector<WpisCzynnikow>& cz, const Stany* st, const std::string& trybStatusu) {
  DaneK1 d;
  const Noc* ost = ostatniaNoc(hist, dzis);
  std::string koniec = ost ? ost->data : dzis;
  if (!ost) d.naglowek = "Ostatnia noc";
  else if (ost->data == dzis) d.naglowek = std::string("Ostatnia noc · ") + DNI_KROTKO[dzienTygodnia(dzis)] + " " + ddmm(dzis);
  else d.naglowek = "Noc " + ddmm(ost->data);  // R-KOK-04
  d.romb = ost && romb(wpisZDnia(cz, ost->data));
  d.jestNoc = ost != nullptr;
  TrybyOkna tr = trybyOkna(hist, koniec);
  if (!ost) trybZeStatusu(tr, trybStatusu);
  bool asv = tr.asv();
  // R-KOK-15: w trybie ASV EPAP (M-06) zamiast ciśnienia (M-05)
  const Metryka liczby[4] = {M_M01, M_AHI, asv ? M_EMED : M_CMED, M_WP95};
  // jednostka wycieku jak na pozostałych ekranach i w spec (`l/min`: S6, K2, teksty kart)
  // T1: `czas z maską` (ta sama nazwa co wiersz), opis nad liczbą; R-KOK-15: opis `EPAP`
  // jedna nazwa metryki M-02 na całym panelu: `AHI`; AHI to z definicji liczba na godzinę
  const char* opisy[4] = {"czas z maską", "AHI", asv ? "EPAP" : "ciśnienie", "wyciek l/min"};
  for (int i = 0; i < 4; i++) {
    d.liczby[i] = format(liczby[i], wartosc(ost, liczby[i]));
    d.opisy[i] = opisy[i];
  }
  // R-KOK-15 fail-path: EPAP z `source = "str"` (docelowe urządzenia) z dopiskiem `(urządzenie)`, jak ciśnienie (4.3)
  if (asv && ost && ost->epapStr && !std::isnan(ost->emed)) {
    d.liczby[2] = format(M_EMED, ost->emed);
    d.opisy[2] = "EPAP (urządzenie)";
  }
  struct Def { const char* n; Metryka m; };
  static const Def W_ASV[5] = {
      {"AHI", M_AHI}, {"Czas z maską", M_M01}, {"EPAP", M_EMED}, {"Wsparcie", M_PSMED}, {"Wyciek", M_WP95}};
  static const Def W_CPAP[4] = {{"AHI", M_AHI}, {"Czas z maską", M_M01}, {"Ciśnienie", M_CMED}, {"Wyciek", M_WP95}};
  const Def* wiersze = asv ? W_ASV : W_CPAP;
  d.wierszy = asv ? 5 : 4;
  for (int i = 0; i < d.wierszy; i++) {
    WierszK1& w = d.w[i];
    Metryka m = wiersze[i].m;
    w.nazwa = wiersze[i].n;
    w.linia = dataOk(koniec) ? linia(hist, koniec, m, &tr) : Linia30{};
    if (!dataOk(koniec))
      for (bool& b : w.linia.ma) b = false;
    std::vector<double> v;
    for (int k = 0; k < 30; k++)
      if (w.linia.ma[k]) v.push_back(w.linia.v[k]);
    double med = mediana(v);
    w.mediana = format(m, med);
    if (normaMetryki(nm, m)->jest()) w.linia.medNormy = normaMetryki(nm, m)->med;  // przerywana
    // bez jednostki przy medianie (jak pozostałe wiersze): `l/min` stoi w opisie liczby nad trendem, a w 30 px
    // nie mieści się obok linii 30 nocy
    const StanMetryki* sm = stanMetryki(st, m);
    if (!normaMetryki(nm, m)->jest()) {
      w.slowo = "uczę się";  // R-KOK-05
    } else if (sm && sm->aktywny) {  // R-KOK-03: stan odchylenia ma pierwszeństwo przed trendem
      w.stan = true;
      w.slowo = std::string(sm->wyzej ? "wyżej" : "niżej") + " niż zwykle";  // T3: w 2 liniach
      w.slowo2 = "od " + std::to_string(sm->nocy) + " nocy";
    } else {
      w.slowo = slowoTrendu(hist, koniec, w.linia, m, tr);  // R-KOK-16
      // za mało nocy w oknie 7 albo 23: `uczę się`; metryka bez Δ (M-28): kolumna oceny pusta
      if (w.slowo.empty() && !std::isnan(deltaTrendu(m))) w.slowo = "uczę się";
    }
    // R-EKR-59: wyciek widoczny (informacja, nie ocena), w dokładności ekranu (mediana `10` nie daje słowa)
    if (m == M_WP95 && !w.stan && !std::isnan(med) && zaokr(m, med) > WYCIEK_WIDOCZNY + 1e-9) w.slowo2 = "wyciek widoczny";
  }
  return d;
}

DaneK2 budujK2(const std::vector<Noc>& hist, const std::string& dzis, const Norma& nm,
               const std::vector<WpisCzynnikow>& cz, const Stany* st, const std::string& trybStatusu) {
  DaneK2 d;
  const Noc* ost = ostatniaNoc(hist, dzis);
  std::string koniec = ost ? ost->data : dzis;
  if (!dataOk(koniec)) {  // bez zegara i bez nocy: pusta tabela
    d.zdanie = "Brak zegara i brak zapisanych nocy.";
    return d;
  }
  TrybyOkna tr = trybyOkna(hist, koniec);
  if (!ost) trybZeStatusu(tr, trybStatusu);
  bool asv = tr.asv();
  std::string od = dodajDni(koniec, -6);
  if (od.substr(5, 2) == koniec.substr(5, 2))
    d.zakres = std::to_string(liczba(od.substr(8, 2))) + "–" + std::to_string(liczba(koniec.substr(8, 2))) + " " +
               MIESIACE_D[liczba(koniec.substr(5, 2)) - 1];
  else
    d.zakres = ddmm(od) + "–" + ddmm(koniec);
  const Noc* noce[7];
  int ponad4 = 0, zapisane = 0;
  for (int i = 0; i < 7; i++) {
    std::string data = dodajDni(koniec, i - 6);
    noce[i] = nocZDnia(hist, data);
    d.kol[i] = std::string(DNI_KROTKO[dzienTygodnia(data)]) + " " + std::to_string(liczba(data.substr(8, 2)));
    d.romb[i] = romb(wpisZDnia(cz, data));
    if (noce[i]) {
      zapisane++;
      if (noce[i]->m01 >= 240) ponad4++;
    }
  }
  d.zdanie = zapisane ? std::to_string(ponad4) + " z 7 nocy z maską ponad 4 h." : std::string("Brak zapisanych nocy w tych 7 dniach.");
  // etykiety T5 (SPEC-KOKPIT 8): jednostka w etykiecie, bez `cmH₂O` i `P95` przy wycieku (kolumna etykiet 148 px)
  struct Def { const char* e; double prog; Metryka m; bool gr, wc, progRowny; };  // kolejność pól według rozmiaru (clang-tidy Padding)
  static const Def AHI_DO_WYCIEKU[7] = {
      {"AHI", PROG_AHI, M_AHI, false, false, true},  // jedna nazwa M-02 na panelu
      {"obturacyjne", NAN, M_OA, false, true, false},  // M-03: OA / h (bez UA)
      {"centralne", NAN, M_CA, false, true, false},    // M-03: CA / h
      {"spłycenia", NAN, M_H, false, true, false},     // M-03: H / h
      {"Czas z maską", NAN, M_M01, true, false, false},
      {"Zdjęcia maski", NAN, M_M17, false, false, false},  // M-17
      {"Wyciek l/min", PROG_WYCIEK, M_WP95, true, false, false},  // wyciek P95 (M-07)
  };
  static const Def CISNIENIE_ASV[2] = {
      {"EPAP P95", NAN, M_EP95, false, false, false},      // ADR-0016: zamiast ciśnienia (IPAP) P95
      {"Wsparcie PS", NAN, M_PSMED, false, false, false},  // mediana PS; bez progu (ADR-0002)
  };
  static const Def CISNIENIE_CPAP = {"Ciśnienie P95", NAN, M_CP95, false, false, false};  // M-05p (5.5.3, wiersz 8)
  static const Def ODDECHY = {"Oddechy /min", NAN, M_M13, false, false, false};  // M-13, bez normy w panelu
  Def def[DaneK2::WIERSZE];
  int n = 0;
  for (const Def& x : AHI_DO_WYCIEKU) def[n++] = x;
  if (asv) {
    def[n++] = CISNIENIE_ASV[0];
    def[n++] = CISNIENIE_ASV[1];
  } else {
    def[n++] = CISNIENIE_CPAP;
  }
  def[n++] = ODDECHY;
  d.wierszy = n;
  for (int r = 0; r < d.wierszy; r++) {
    WierszK2& w = d.w[r];
    w.etykieta = def[r].e;
    w.grupa = def[r].gr;
    w.wciecie = def[r].wc;
    if (def[r].m == M_BRAK) continue;  // brak danych tej metryki w prototypie: kreski
    bool zAirBridge = def[r].m == M_OA || def[r].m == M_CA || def[r].m == M_H || def[r].m == M_M17 || def[r].m == M_M13;
    if (zAirBridge) {  // M-03, M-13, M-17 tylko z AirBridge: bez żadnej wartości w 30 nocach (np. same noce z karty) jak
                       // wiersz bez danych
      Linia30 l = linia(hist, koniec, def[r].m);
      if (std::find(std::begin(l.ma), std::end(l.ma), true) == std::end(l.ma)) continue;
    }
    static const NormaMetryki BEZ_NORMY;
    const NormaMetryki* nr = normaMetryki(nm, def[r].m);
    if (!nr) nr = &BEZ_NORMY;  // M-13: normy panel nie liczy, kolumna `norma` `–` (nie `uczę się`, bo jej nie przybędzie)
    w.norma = nr->jest() ? format(def[r].m, nr->p10) + "–" + format(def[r].m, nr->p90)
                         : std::string(normaMetryki(nm, def[r].m) ? "uczę się" : "–");
    const StanMetryki* sm = stanMetryki(st, def[r].m);
    w.stan = nr->jest() && sm && sm->aktywny;  // R-KOK-09 (bez normy bez ramki, R-KOK-05)
    for (int i = 0; i < 7; i++) {
      if (zaleznaOdTrybu(def[r].m) && !tr.zgodna(23 + i)) continue;  // fail-path R-KOK-15: noc w innym trybie, `–`
      double v = wartosc(noce[i], def[r].m);
      if (std::isnan(v)) continue;  // R-KOK-10, R-DANE-07
      KomorkaK2& k = w.k[i];
      k.tekst = format(def[r].m, v);
      if (!std::isnan(def[r].prog)) k.ponadProg = def[r].progRowny ? v >= def[r].prog - 1e-9 : v > def[r].prog + 1e-9;
      // R-KOK-08 na liczbach w dokładności ekranu (komórka `9,0` przy normie `7,8–9,0` nie ma ramki);
      // R-KOK-05: bez normy bez ramek
      if (nr->jest() && !k.ponadProg) {
        double vz = zaokr(def[r].m, v);
        k.poza = vz < zaokr(def[r].m, nr->p10) - 1e-9 || vz > zaokr(def[r].m, nr->p90) + 1e-9;
      }
    }
  }
  return d;
}
