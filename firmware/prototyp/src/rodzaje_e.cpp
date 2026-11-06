// SPDX-License-Identifier: GPL-3.0-or-later
// Rodzaje warunków zdarzeń E spoza rdzenia silnika.
// Rdzeń silnika (src/reguly.cpp) ma rodzaje z tabeli 9.3 spec technicznej; tu są tylko te, których zdarzenia E z rekordu
// nocy potrzebują, a których rdzeń nie ma. Każdy rodzaj to czysta funkcja wiersza katalogu i kontekstu (kontrakt 1):
// nie czyta plików ani sieci, liczby bierze wyłącznie z pliku progów (parametr = id_progu), a „nie oceniam”
// (oceniona = false) zwraca zawsze, gdy brakuje danych; nigdy nie udaje „w porządku”.
//
// Rodzaje i parametry (pole `parametry` katalogu docs/spec/katalog/E.csv):
//   noc_czesciowa     E-04: noc `dzis` zapisana z `partial = true` (Noc::czesciowa, R-DANE-15) i sygnał komponentu
//                     `po_11` (jest 11:00 albo później); bez sygnału „nie oceniam”. Bez parametrów. Zmienna {data}.
//   przedzial_nocy    E-11: metryka nocy `dzis` w przedziale [min, max). Parametry `metryka`, `min`, `max`
//                     (id_progu; tu granice wartości metryki, nie liczby nocy: nowe znaczenie parametrów z kontraktu 6).
//                     Próg w godzinach (`jednostka` = h) przy metryce w minutach (M-01) zamieniany na minuty.
//                     Noc częściowa = „nie oceniam”. Zmienna {czas} (M-01 jako „3 h 20”).
//   regularnosc_okna  E-12: M-19 (5.1): noce z M-01 ≥ `prog` w ostatnich d dniach, d = min(30, dni od pierwszej nocy
//                     z rekordem); spełnione, gdy noce < `odsetek` % z d, przy historii ≥ `min` dni; noce przerwy
//                     planowanej (Odpowiedzi::przerwaPlanowana: daty nocy przerwy) w ciągu `przerwa` dni wypadają
//                     z d i z licznika (R-SKI-35); wyjście po `wyjscie` kolejnych dniach z odsetkiem ≥ `odsetek`
//                     (liczone bez pamięci: zdarzenie trwa, gdy warunek wejścia był spełniony w którymś z ostatnich
//                     `wyjscie` dni). Nowe parametry: `odsetek`, `przerwa`, `wyjscie` (id_progu). Zmienne {n}, {d},
//                     {ceil(0,7·d)} (cel z tekstu 6.4).
//   odchylenie_nocy_normy  E-36: odchylenie metryki ciśnienia w jednej nocy (4.5) w co najmniej k z n ostatnich nocy
//                     ważnych trybu bieżącego, ze stanem odchylenia liczonym noc po nocy jak w src/stany.cpp
//                     (przebiegCisnienia(): R-NOR-03, R-NOR-04, R-NOR-09, norma zamrożona R-NOR-22). Parametry
//                     `metryka` (id metryki albo id_progu z nazwą zależną od trybu: E-36.sygnal_cisnienia = M-06p w ASV
//                     i ASVAuto, M-05p w CPAP i AutoSet, 4.11), `kzn` (k/n), opcjonalnie `min` (stan trwa co najmniej tyle
//                     nocy, R-NOR-23); Δ z progu `R-NOR-12.delta.{metryka}`. Zmienne {x}, {norma}, {n}.
//   stan_odchylenia_n E-21: stan odchylenia z src/stany.* jak `stan_odchylenia` rdzenia, z dodatkową zmienną {n}.
//   nie_reguly        E-23/bez_sk10_kilka: reguła z parametru `regula` niespełniona (brak w katalogu = „nie oceniam”).
//   noce_bez_terapii  E-10: noc bez terapii (5.1.4): rekord kompletny z M-01 < `prog`, a dla nocy `dzis` bez rekordu
//                     sygnał komponentu `noc_bez_rekordu` (AirBridge działa, po 11:00 nocy nadal brak na liście,
//                     R-DANE-20; bez sygnału „nie oceniam”, bo przed 11:00 noc jest „nieobecna”, R-DANE-22). Opcjonalnie
//                     `n`: tyle kolejnych dni bez terapii do `dzis` (poziom „problem”; dzień bez rekordu przed `dzis`
//                     liczy się jak noc bez terapii, bo raport AirBridge uzupełnia historię po powrocie łączności).
//                     Z `n` także `wyjscie` (R-DANE-13.m01_min, 4 h): „problem” trwa do pierwszej nocy ≥ 4 h, noc
//                     z terapią krótsza nie kończy (6.4). Zmienne {data} (`dzis` jako dd.mm; w „problem” ostatnia noc
//                     z terapią, „–”, gdy jej nie było), {n} (noce bez terapii), {data_terapii}.
//   duzy_wyciek       E-20, E-22: duży wyciek nocy (4.9): M-08 (minuty wycieku powyżej progu maski) ≥ `prog` minut albo
//                     ≥ `odsetek` % czasu terapii M-01; noce z terapią (M-01 ≥ R-SKI-27.noc_z_terapia_m01). Bez `kzn`: noc `dzis`. Z `kzn` (k/n) i
//                     `wyjscie`: wejście, gdy w k z n nocy, wyjście po `wyjscie` kolejnych nocach bez dużego wycieku
//                     (bez pamięci: przebieg po ostatnich 60 nocach z terapią). Zmienne {min} (M-08 nocy albo średnia
//                     nocy z wyciekiem w ostatnich n), {n} (noce z wyciekiem w ostatnich n). M-08 z reguly::metryka();
// do scalenia pola `m08` testy podają atrapę ustawAtrapeMetrykiE.
//   prog_biezacy_wyjscie  E-05: bieżący odczyt komponentu (Sygnaly::odczyty[`metryka`], np. M-25 bateria w %) ≤ `prog`;
//                     zdarzenie trwa, dopóki odczyt nie przekroczy `wyjscie` (6.4 „bateria > 20%”; poprzedni stan
//                     z pamięci reguł, PamiecReguly::aktywnaOd). `por` = le (domyślnie) albo lt; przy linii E-21
//                     `wyjscie` = `prog`. Brak odczytu → „nie oceniam”. Zmienne {x}, {w}.
//
// Rejestracja: zarejestrujRodzajeE() (wywołuje start silnika albo test przed zestawWbudowany()); bez statycznego
// inicjatora, żeby nie zależeć od kolejności inicjalizacji rejestru w src/reguly.cpp.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "noc.h"
#include "reguly.h"
#include "reguly_pomoc.h"
#include "reguly_sygnaly.h"
#include "stany.h"
#include "wspolne.h"

namespace reguly {

void zarejestrujRodzajeE();
// Atrapa metryk dla testów (np. M-08 przed scaleniem pola `m08`); nullptr = reguly::metryka().
using AtrapaMetryki = double (*)(const Noc&, const std::string&);
void ustawAtrapeMetrykiE(AtrapaMetryki f);

namespace {

AtrapaMetryki gAtrapa = nullptr;

double metrykaE(const Noc& n, const std::string& id) {
  if (gAtrapa != nullptr) {
    double v = gAtrapa(n, id);
    if (!std::isnan(v)) return v;
  }
  return metryka(n, id);
}

Wynik poczatek(const WierszKatalogu& w) {
  Wynik r;
  r.id = w.id;
  r.wariant = w.wariant;
  r.stan = w.stan;
  r.pilnosc = w.pilnosc;
  r.poziom = w.poziom;
  r.grupa = w.grupa;
  return r;
}

Wynik nieOceniam(const WierszKatalogu& w, const std::string& powod) {
  Wynik r = poczatek(w);
  r.oceniona = false;
  r.spelniona = false;
  r.powodNieoceny = powod;
  return r;
}

std::string param(const WierszKatalogu& w, const std::string& nazwa) {
  auto it = w.parametry.find(nazwa);
  return it == w.parametry.end() ? std::string() : it->second;
}

// Próg z parametru: wiersz trybu, inaczej "wszystkie" (Zestaw::prog). nullptr = brak parametru albo progu.
const Prog* progZParametru(const WierszKatalogu& w, const Kontekst& k, const std::string& nazwa, Tryb tryb) {
  std::string id = param(w, nazwa);
  if (id.empty() || k.zestaw == nullptr) return nullptr;
  return k.zestaw->prog(id, tryb);
}

// Wartość k/n z progu ("5/7"); false, gdy zapis nie jest k/n.
bool kzN(const Prog& p, int& kk, int& nn) {
  const std::string& t = p.tekst;
  size_t ukos = t.find('/');
  if (ukos == std::string::npos) return false;
  kk = liczbaCalk(t.substr(0, ukos).c_str());
  nn = liczbaCalk(t.substr(ukos + 1).c_str());
  return kk > 0 && nn > 0 && kk <= nn;
}

// Wartość progu w jednostce metryki: próg w godzinach przy metryce w minutach (M-01) → minuty.
double wJednostceMetryki(const Prog& p, const std::string& idMetryki) {
  if (idMetryki == "M-01" && p.jednostka == "h") return p.wartosc * 60.0;
  return p.wartosc;
}


// ---------- noc_czesciowa (E-04) ----------
Wynik nocCzesciowa(const WierszKatalogu& w, const Kontekst& k) {
  if (k.historia == nullptr) return nieOceniam(w, "brak historii");
  const Noc* n = nocZDnia(*k.historia, k.dzis);
  if (n == nullptr) return nieOceniam(w, "brak nocy " + k.dzis);
  // „o 11:00 raport nadal partial” (6.4): godzinę zna komponent, sygnał `po_11` (przed 11:00 noc częściowa to jeszcze
  // raport w budowie, S10)
  const Sygnaly* s = sygnalyOceny();
  auto it = s ? s->stan.find("po_11") : std::map<std::string, bool>::const_iterator();
  if (s == nullptr || it == s->stan.end()) return nieOceniam(w, "brak sygnalu po_11");
  Wynik r = poczatek(w);
  r.oceniona = true;
  r.spelniona = n->czesciowa && it->second;
  r.zmienne["data"] = ddmm(n->data);
  return r;
}

// ---------- przedzial_nocy (E-11) ----------
Wynik przedzialNocy(const WierszKatalogu& w, const Kontekst& k) {
  if (k.historia == nullptr) return nieOceniam(w, "brak historii");
  const Noc* n = nocZDnia(*k.historia, k.dzis);
  if (n == nullptr) return nieOceniam(w, "brak nocy " + k.dzis);
  if (n->czesciowa) return nieOceniam(w, "noc czesciowa");
  std::string idMetryki = param(w, "metryka");
  Tryb tryb = trybNocy(k, *n);
  const Prog* pMin = progZParametru(w, k, "min", tryb);
  const Prog* pMax = progZParametru(w, k, "max", tryb);
  if (idMetryki.empty() || pMin == nullptr || pMax == nullptr) return nieOceniam(w, "brak parametru");
  double v = metrykaE(*n, idMetryki);
  if (std::isnan(v)) return nieOceniam(w, "brak metryki " + idMetryki);
  double lo = wJednostceMetryki(*pMin, idMetryki), hi = wJednostceMetryki(*pMax, idMetryki);
  Wynik r = poczatek(w);
  r.oceniona = true;
  r.spelniona = v >= lo - 1e-6 && v < hi - 1e-6;
  r.zmienne["czas"] = idMetryki == "M-01" ? godzH((int)std::lround(v)) : przecinek1(v);
  return r;
}

// ---------- regularnosc_okna (E-12) ----------
struct Regularnosc {
  bool ok = false;  // dzień oceniany (historia ≥ min dni, d > 0)
  int n = 0, d = 0;
  bool ponizej = false;
};

Regularnosc regularnoscDnia(const std::vector<Noc>& hist, const std::string& pierwsza, const std::string& dzien,
                            double nocMin, double odsetek, int minDni, int przerwaDni, const Odpowiedzi* odp) {
  Regularnosc r;
  int dniHistorii = dniMiedzy(pierwsza, dzien) + 1;
  if (dniHistorii < minDni) return r;
  int d = std::min(30, dniHistorii);
  int dEf = 0, n = 0;
  for (int i = 0; i < d; i++) {
    std::string x = dodajDni(dzien, -i);
    bool przerwa = odp != nullptr && odp->przerwaPlanowana.count(x) > 0 && dniMiedzy(x, dzien) < przerwaDni;
    if (przerwa) continue;  // R-SKI-35: noc przerwy planowanej poza d i poza licznikiem
    const Noc* noc = nocZDnia(hist, x);
    if (noc != nullptr && noc->czesciowa) continue;  // noc częściowa (R-DANE-15): czas niepewny, poza d i licznikiem
    dEf++;
    if (noc != nullptr && noc->m01 >= 0 && noc->m01 + 1e-6 >= nocMin) n++;
  }
  if (dEf <= 0) return r;
  r.ok = true;
  r.n = n;
  r.d = dEf;
  r.ponizej = (double)n * 100.0 < odsetek * (double)dEf - 1e-6;  // „mniej niż 70%”
  return r;
}

Wynik regularnoscOkna(const WierszKatalogu& w, const Kontekst& k) {
  if (k.historia == nullptr || k.historia->empty()) return nieOceniam(w, "brak historii");
  Tryb tryb = k.trybBiezacy;
  const Prog* pNoc = progZParametru(w, k, "prog", tryb);
  const Prog* pOdsetek = progZParametru(w, k, "odsetek", tryb);
  const Prog* pMin = progZParametru(w, k, "min", tryb);
  const Prog* pPrzerwa = progZParametru(w, k, "przerwa", tryb);
  const Prog* pWyjscie = progZParametru(w, k, "wyjscie", tryb);
  if (!pNoc || !pOdsetek || !pMin || !pPrzerwa || !pWyjscie) return nieOceniam(w, "brak parametru");
  std::string pierwsza;
  for (const Noc& n : *k.historia)
    if (n.data <= k.dzis && (pierwsza.empty() || n.data < pierwsza)) pierwsza = n.data;
  if (pierwsza.empty()) return nieOceniam(w, "brak nocy");
  double nocMin = wJednostceMetryki(*pNoc, "M-01");
  int minDni = (int)std::lround(pMin->wartosc), przerwaDni = (int)std::lround(pPrzerwa->wartosc);
  int wyjscie = std::max(1, (int)std::lround(pWyjscie->wartosc));
  Regularnosc dzis = regularnoscDnia(*k.historia, pierwsza, k.dzis, nocMin, pOdsetek->wartosc, minDni, przerwaDni,
                                     k.odpowiedzi);
  if (!dzis.ok)
    return nieOceniam(w, "za malo dni historii " + std::to_string(dniMiedzy(pierwsza, k.dzis) + 1) + "/" +
                             std::to_string(minDni));
  bool aktywne = false;
  for (int i = 0; i < wyjscie && !aktywne; i++) {
    Regularnosc r = regularnoscDnia(*k.historia, pierwsza, dodajDni(k.dzis, -i), nocMin, pOdsetek->wartosc, minDni,
                                    przerwaDni, k.odpowiedzi);
    aktywne = r.ok && r.ponizej;
  }
  Wynik r = poczatek(w);
  r.oceniona = true;
  r.spelniona = aktywne;
  r.zmienne["n"] = std::to_string(dzis.n);
  r.zmienne["d"] = std::to_string(dzis.d);
  r.zmienne["ceil(0,7·d)"] =
      std::to_string((int)std::ceil(pOdsetek->wartosc / 100.0 * (double)dzis.d - 1e-9));
  return r;
}

// ---------- odchylenie_nocy_normy (E-36) ----------
// Rodzina trybów metryki ciśnienia (4.11): ASV i ASVAuto liczą M-06p, CPAP i AutoSet M-05p; okno tylko z nocy tej
// rodziny (reguła z wariantem w 4.11 liczy okno z nocy w trybie bieżącym; noc spoza wydania nie liczy się, R-DANE-29).
bool rodzinaAsv(Tryb t) { return t == Tryb::ASV || t == Tryb::ASVAUTO; }
bool tejSamejRodziny(Tryb a, Tryb b) {
  if (a == Tryb::NIEZNANY || b == Tryb::NIEZNANY) return false;
  return rodzinaAsv(a) == rodzinaAsv(b);
}

const NormaMetryki* normaMetryki(const Norma& nm, const std::string& idMetryki) {
  if (idMetryki == "M-06p") return &nm.ep95;
  if (idMetryki == "M-05p") return &nm.cp95;
  return nullptr;
}

// Przebieg jednej metryki ciśnienia noc po nocy, tą samą metodą co src/stany.cpp (liczStany): norma nocy z nocy
// ważnych przed nią (4.4), odchylenie 4.5, wejście w stan przy 3 z 5 (R-NOR-03), wyjście po 3 nocach w granicy Δ/2
// (R-NOR-04), metryka zdrowotna z normą zamrożoną od wejścia (R-NOR-22), noce stanu poza normą (R-NOR-09).
// M-06p: wyłączenia jak Stany::wyl.ep95, więc wynik jest ten sam co Stany::e36 i Stany::ep95 na nocach jednego trybu.
// M-05p: Stany nie liczy stanu M-05p (jeszcze nie liczony), a WykluczeniaNormy nie ma pola M-05p, więc noce stanu zostają
// w normie poza okresem stanu (w stanie norma i tak zamrożona) [uproszczenie]. Wyłączeń czynników
// R-CZY-05 nie ma w kontekście (Kontekst ich nie niesie): norma bez nich.
struct PrzebiegCisnienia {
  bool aktywny = false;
  std::string od;
  int nocy = 0;
  std::vector<bool> odch;  // odchylenia kolejnych nocy z wartością (najnowsza na końcu)
  double x = NAN, norma = NAN;
  bool normaDzis = false;
};

PrzebiegCisnienia przebiegCisnienia(const Kontekst& k, const std::string& m, double delta) {
  PrzebiegCisnienia p;
  WykluczeniaNormy wyl;
  if (k.stany != nullptr) wyl = k.stany->wyl;
  if (m == "M-06p") wyl.ep95.clear();  // liczone tu od nowa, jak w liczStany()
  const std::set<std::string> bezCzynnikow;
  std::vector<bool> ost5;
  NormaMetryki zamrozona;
  int spokojne = 0;
  for (const Noc& n : *k.historia) {
    if (n.data > k.dzis) break;
    if (!n.wazna || !tejSamejRodziny(trybNocy(k, n), k.trybBiezacy)) continue;
    double v = metrykaE(n, m);
    if (std::isnan(v)) continue;
    Norma nm = liczNorme(*k.historia, n.data, bezCzynnikow, &wyl);
    const NormaMetryki& biez = *normaMetryki(nm, m);
    const NormaMetryki& norma = p.aktywny ? zamrozona : biez;
    bool o = norma.jest() && v > norma.p90 && v - norma.med >= delta - 1e-6;
    p.odch.push_back(o);
    ost5.push_back(o);
    if (ost5.size() > 5) ost5.erase(ost5.begin());
    if (!p.aktywny) {
      if (std::count(ost5.begin(), ost5.end(), true) >= 3 && norma.jest()) {
        p.aktywny = true;
        p.od = n.data;
        p.nocy = 0;
        zamrozona = norma;
        spokojne = 0;
      }
    } else if (std::fabs(v - norma.med) <= delta / 2 + 1e-6) {
      spokojne++;
    } else {
      spokojne = 0;
    }
    if (p.aktywny) {
      p.nocy++;
      if (m == "M-06p") wyl.ep95.insert(n.data);
      if (spokojne >= 3) {
        p.aktywny = false;
        ost5.clear();
        p.od.clear();
        p.nocy = 0;
      }
    }
    const NormaMetryki& pokaz = p.aktywny ? zamrozona : biez;
    p.x = v;
    p.normaDzis = pokaz.jest();
    p.norma = pokaz.med;
  }
  return p;
}

Wynik odchylenieNocy(const WierszKatalogu& w, const Kontekst& k) {
  if (k.historia == nullptr) return nieOceniam(w, "brak historii");
  Tryb tryb = k.trybBiezacy;
  // `metryka`: id metryki albo id_progu z nazwą metryki zależną od trybu (E-36.sygnal_cisnienia, 4.11)
  std::string idMetryki = param(w, "metryka");
  if (k.zestaw != nullptr) {
    const Prog* pm = k.zestaw->prog(idMetryki, tryb);
    if (pm != nullptr && !pm->tekst.empty()) idMetryki = pm->tekst;
  }
  Norma pusta;
  if (idMetryki.empty() || normaMetryki(pusta, idMetryki) == nullptr)
    return nieOceniam(w, "metryka bez odchylenia " + idMetryki);
  const Prog* pKzn = progZParametru(w, k, "kzn", tryb);
  const Prog* pDelta = k.zestaw ? k.zestaw->prog("R-NOR-12.delta." + idMetryki, tryb) : nullptr;  // Δ 4.5
  int kk = 0, nn = 0;
  if (pKzn == nullptr || pDelta == nullptr || !kzN(*pKzn, kk, nn)) return nieOceniam(w, "brak parametru");
  PrzebiegCisnienia p = przebiegCisnienia(k, idMetryki, pDelta->wartosc);
  if ((int)p.odch.size() < nn)
    return nieOceniam(w, "za malo nocy " + std::to_string(p.odch.size()) + "/" + std::to_string(nn));
  if (!p.normaDzis) return nieOceniam(w, "brak normy " + idMetryki);
  int odch = (int)std::count(p.odch.end() - nn, p.odch.end(), true);
  bool spelniona = odch >= kk;
  if (!param(w, "min").empty()) {
    const Prog* pMin = progZParametru(w, k, "min", tryb);
    if (pMin == nullptr) return nieOceniam(w, "brak parametru min");
    spelniona = spelniona && p.aktywny && p.nocy >= (int)std::lround(pMin->wartosc);  // R-NOR-23
  }
  Wynik r = poczatek(w);
  r.oceniona = true;
  r.spelniona = spelniona;
  r.zmienne["x"] = przecinek1(p.x);
  r.zmienne["norma"] = przecinek1(p.norma);
  r.zmienne["n"] = std::to_string(odch);
  return r;
}

// ---------- stan_odchylenia_n (E-21) ----------
// Jak `stan_odchylenia` rdzenia (Stany z src/stany.*), z dodatkową zmienną {n} = noce stanu: tekst E-21
// „Wyciek rośnie od {n} nocy” (6.4) jej potrzebuje, a rdzeń podaje tylko {nocy} [do zgrania z gdy rdzeń poda
// {n}, wiersz E-21 wraca na `stan_odchylenia`]. Metryki: M-01, M-02, M-07, M-06p, M-03ca, M-17.
const StanMetryki* stanZeStanow(const Stany& s, const std::string& m, const NormaMetryki*& nm) {
  if (m == "M-01") return nm = &s.norma.m01, &s.m01;
  if (m == "M-02") return nm = &s.norma.ahi, &s.ahi;
  if (m == "M-07") return nm = &s.norma.wp95, &s.wp95;
  if (m == "M-06p") return nm = &s.norma.ep95, &s.ep95;
  return nm = nullptr, nullptr;
}

Wynik stanOdchyleniaN(const WierszKatalogu& w, const Kontekst& k) {
  std::string m = param(w, "metryka");
  if (k.stany == nullptr || k.historia == nullptr) return nieOceniam(w, "brak stanow odchylenia");
  const NormaMetryki* nm = nullptr;
  const StanMetryki* st = stanZeStanow(*k.stany, m, nm);
  if (st == nullptr) return nieOceniam(w, "stan " + m + " nie liczony");
  if (nm == nullptr || !nm->jest()) return nieOceniam(w, "brak normy " + m);
  Wynik r = poczatek(w);
  r.oceniona = true;
  r.spelniona = st->aktywny;
  r.zmienne["n"] = std::to_string(st->nocy);
  r.zmienne["nocy"] = std::to_string(st->nocy);
  r.zmienne["od"] = st->od;
  r.zmienne["norma"] = przecinek1(nm->med);
  const Noc* ost = ostatniaNoc(*k.historia, k.dzis);
  r.zmienne["x"] = ost ? przecinek1(metrykaE(*ost, m)) : std::string("–");
  return r;
}

// ---------- noce_bez_terapii (E-10) ----------
Wynik noceBezTerapii(const WierszKatalogu& w, const Kontekst& k) {
  if (k.historia == nullptr) return nieOceniam(w, "brak historii");
  const Prog* pProg = progZParametru(w, k, "prog", k.trybBiezacy);
  if (pProg == nullptr) return nieOceniam(w, "brak parametru prog");
  double prog = wJednostceMetryki(*pProg, "M-01");
  int ile = 1;
  double pelna = 0;
  if (!param(w, "n").empty()) {
    const Prog* pN = progZParametru(w, k, "n", k.trybBiezacy);
    const Prog* pW = progZParametru(w, k, "wyjscie", k.trybBiezacy);
    if (pN == nullptr || pW == nullptr) return nieOceniam(w, "brak parametru n albo wyjscie");
    ile = std::max(1, (int)std::lround(pN->wartosc));
    pelna = wJednostceMetryki(*pW, "M-01");  // „znika po nocy ≥ 4 h” (6.4)
  }
  // noc dzis: rekord kompletny albo sygnał komponentu (o 11:00 nocy nadal brak na liście, R-DANE-20)
  enum Dzien : std::uint8_t { BEZ, KROTKA, PELNA, NIEZNANA };
  const Noc* dzis = nocZDnia(*k.historia, k.dzis);
  Dzien stanDzis = NIEZNANA;
  if (dzis != nullptr) {
    if (dzis->czesciowa || dzis->m01 < 0) return nieOceniam(w, "noc niekompletna");
    stanDzis = dzis->m01 + 1e-6 < prog ? BEZ : (ile > 1 && dzis->m01 + 1e-6 >= pelna ? PELNA : KROTKA);
  } else {
    const Sygnaly* s = sygnalyOceny();
    auto it = s ? s->stan.find("noc_bez_rekordu") : std::map<std::string, bool>::const_iterator();
    if (s == nullptr || it == s->stan.end()) return nieOceniam(w, "brak nocy " + k.dzis + " i sygnalu noc_bez_rekordu");
    stanDzis = it->second ? BEZ : NIEZNANA;  // przed 11:00 noc „nieobecna” (R-DANE-22): nie kończy i nie zaczyna
  }
  Wynik r = poczatek(w);
  r.oceniona = true;
  if (ile == 1) {
    r.spelniona = stanDzis == BEZ;
    r.zmienne["data"] = ddmm(k.dzis);
    r.zmienne["n"] = r.spelniona ? "1" : "0";
    return r;
  }
  // „problem”: od n kolejnych nocy bez terapii do pierwszej nocy ≥ 4 h (noc z terapią krótsza niż 4 h nie kończy)
  std::string pierwsza;
  for (const Noc& n : *k.historia)
    if (n.data <= k.dzis && (pierwsza.empty() || n.data < pierwsza)) pierwsza = n.data;
  int seria = 0, najdluzsza = 0, bezTerapii = 0, dni = 0;
  bool doPelnej = false;
  std::string dataTerapii;
  for (int i = 0; i < 60; i++) {
    std::string d = dodajDni(k.dzis, -i);
    Dzien st = stanDzis;
    if (i > 0) {
      if (pierwsza.empty() || d < pierwsza) break;
      const Noc* n = nocZDnia(*k.historia, d);
      if (n == nullptr) st = BEZ;  // raport AirBridge uzupełnia historię po powrocie łączności
      else if (n->czesciowa || n->m01 < 0) st = NIEZNANA;
      else st = n->m01 + 1e-6 < prog ? BEZ : (n->m01 + 1e-6 >= pelna ? PELNA : KROTKA);
      if (st != BEZ && st != NIEZNANA && dataTerapii.empty()) dataTerapii = d;
    }
    if (st == PELNA) {
      doPelnej = true;
      break;
    }
    dni++;
    if (st == BEZ) {
      seria++;
      bezTerapii++;
      najdluzsza = std::max(najdluzsza, seria);
    } else {
      seria = 0;
    }
  }
  if (najdluzsza < ile && !doPelnej && dni < ile)
    return nieOceniam(w, "za malo nocy " + std::to_string(dni) + "/" + std::to_string(ile));
  r.spelniona = najdluzsza >= ile;
  r.zmienne["n"] = std::to_string(bezTerapii);
  // „Ostatnia terapia: {data}.”: bez nocy z terapią w historii „–”
  r.zmienne["data_terapii"] = dataTerapii.empty() ? std::string("–") : ddmm(dataTerapii);
  r.zmienne["data"] = r.zmienne["data_terapii"];
  return r;
}

// ---------- duzy_wyciek (E-20, E-22) ----------
Wynik duzyWyciek(const WierszKatalogu& w, const Kontekst& k) {
  if (k.historia == nullptr) return nieOceniam(w, "brak historii");
  Tryb tryb = k.trybBiezacy;
  const Prog* pMin = progZParametru(w, k, "prog", tryb);
  const Prog* pOds = progZParametru(w, k, "odsetek", tryb);
  if (pMin == nullptr || pOds == nullptr) return nieOceniam(w, "brak parametru");
  auto duzy = [&](const Noc& n, double& m08) {
    m08 = metrykaE(n, "M-08");
    return !std::isnan(m08) && (m08 + 1e-6 >= pMin->wartosc || m08 * 100.0 + 1e-6 >= pOds->wartosc * (double)n.m01);
  };
  const Prog* pTer = k.zestaw ? k.zestaw->prog("R-SKI-27.noc_z_terapia_m01", tryb) : nullptr;  // noc z terapią (1.5)
  if (pTer == nullptr) return nieOceniam(w, "brak progu R-SKI-27.noc_z_terapia_m01");
  double terapiaMin = wJednostceMetryki(*pTer, "M-01");
  auto zTerapia = [&](const Noc& n) { return !n.czesciowa && n.m01 >= 0 && n.m01 + 1e-6 >= terapiaMin && n.data <= k.dzis; };
  if (param(w, "kzn").empty()) {
    const Noc* n = nocZDnia(*k.historia, k.dzis);
    if (n == nullptr || !zTerapia(*n)) return nieOceniam(w, "brak nocy z terapia " + k.dzis);
    double m08 = NAN;
    bool t = duzy(*n, m08);
    if (std::isnan(m08)) return nieOceniam(w, "brak metryki M-08");
    Wynik r = poczatek(w);
    r.oceniona = true;
    r.spelniona = t;
    r.zmienne["min"] = calkowita(m08);
    return r;
  }
  const Prog* pKzn = progZParametru(w, k, "kzn", tryb);
  const Prog* pWy = progZParametru(w, k, "wyjscie", tryb);
  int kk = 0, nn = 0;
  if (pKzn == nullptr || pWy == nullptr || !kzN(*pKzn, kk, nn)) return nieOceniam(w, "brak parametru");
  int wyjscie = std::max(1, (int)std::lround(pWy->wartosc));
  std::vector<const Noc*> noce;  // od najnowszej, do 60 nocy z terapią i z M-08
  for (auto it = k.historia->rbegin(); it != k.historia->rend() && noce.size() < 60; ++it)
    if (zTerapia(*it) && !std::isnan(metrykaE(*it, "M-08"))) noce.push_back(&*it);
  if ((int)noce.size() < nn) return nieOceniam(w, "za malo nocy " + std::to_string(noce.size()) + "/" + std::to_string(nn));
  std::vector<bool> d(noce.size());
  std::vector<double> m(noce.size());
  for (size_t i = 0; i < noce.size(); i++) d[i] = duzy(*noce[i], m[i]);
  // przebieg od najstarszej nocy: wejście, gdy k z n ostatnich nocy ma duży wyciek; wyjście po `wyjscie` kolejnych
  // nocach bez dużego wycieku (6.4)
  bool aktywne = false;
  int bez = 0;
  for (size_t i = noce.size(); i-- > 0;) {
    bez = d[i] ? 0 : bez + 1;
    bool wejscie = false;
    if (i + (size_t)nn <= noce.size()) {
      int c = 0;
      for (size_t j = i; j < i + (size_t)nn; j++) c += d[j] ? 1 : 0;
      wejscie = c >= kk;
    }
    if (wejscie) aktywne = true;
    else if (bez >= wyjscie) aktywne = false;
  }
  int c = 0;
  double suma = 0;
  for (size_t j = 0; j < (size_t)nn; j++)
    if (d[j]) {
      c++;
      suma += m[j];
    }
  Wynik r = poczatek(w);
  r.oceniona = true;
  r.spelniona = aktywne;
  r.zmienne["n"] = std::to_string(c);
  r.zmienne["min"] = c > 0 ? calkowita(suma / c) : std::string("–");
  return r;
}

// ---------- prog_biezacy_wyjscie (E-05) ----------
Wynik progBiezacyWyjscie(const WierszKatalogu& w, const Kontekst& k) {
  std::string idMetryki = param(w, "metryka");
  const Prog* pProg = progZParametru(w, k, "prog", k.trybBiezacy);
  const Prog* pWy = progZParametru(w, k, "wyjscie", k.trybBiezacy);
  if (idMetryki.empty() || pProg == nullptr || pWy == nullptr) return nieOceniam(w, "brak parametru");
  const Sygnaly* s = sygnalyOceny();
  if (s == nullptr) return nieOceniam(w, "brak odczytu " + idMetryki);
  auto it = s->odczyty.find(idMetryki);
  if (it == s->odczyty.end() || std::isnan(it->second)) return nieOceniam(w, "brak odczytu " + idMetryki);
  double v = it->second;
  bool bylo = false;
  if (k.pamiec != nullptr) {
    auto p = k.pamiec->find(kluczWyniku(w.id, w.wariant));
    bylo = p != k.pamiec->end() && !p->second.aktywnaOd.empty();
  }
  Wynik r = poczatek(w);
  r.oceniona = true;
  std::string por = param(w, "por").empty() ? std::string("le") : param(w, "por");
  r.spelniona = porownaj(v, por, pProg->wartosc) || (bylo && v <= pWy->wartosc + 1e-6);
  r.zmienne["x"] = calkowita(v);
  r.zmienne["w"] = calkowita(v);  // E-21, linia o suchym powietrzu: `{w}` = minimum wilgotności nocy (5.8.6)
  return r;
}
// ---------- nie_reguly (E-23/bez_sk10_kilka) ----------
// Reguła `regula` (`id` albo `id/wariant`) oceniona wcześniej w tym przebiegu jest niespełniona. Reguły nie ma
// w katalogu albo jest „nie oceniam” → „nie oceniam” (nigdy „w porządku”): warunek 6.4 „E-23 nie wchodzi przy
// S-K-10 od kilku nocy” do czasu wiersza S-K-10.
Wynik nieReguly(const WierszKatalogu& w, const Kontekst& k) {
  std::string ref = param(w, "regula");
  if (ref.empty()) return nieOceniam(w, "brak parametru regula");
  if (k.wczesniejsze == nullptr) return nieOceniam(w, "brak wynikow wczesniejszych");
  auto it = k.wczesniejsze->find(ref);
  if (it == k.wczesniejsze->end()) return nieOceniam(w, "brak reguly " + ref + " w katalogu");
  if (!it->second.oceniona) return nieOceniam(w, ref + " nie oceniam");
  Wynik r = poczatek(w);
  r.oceniona = true;
  r.spelniona = !it->second.spelniona;
  return r;
}

}  // namespace

void ustawAtrapeMetrykiE(AtrapaMetryki f) { gAtrapa = f; }

void zarejestrujRodzajeE() {
  zarejestrujRodzaj("noc_czesciowa", nocCzesciowa);
  zarejestrujRodzaj("przedzial_nocy", przedzialNocy);
  zarejestrujRodzaj("regularnosc_okna", regularnoscOkna);
  zarejestrujRodzaj("odchylenie_nocy_normy", odchylenieNocy);
  zarejestrujRodzaj("noce_bez_terapii", noceBezTerapii);
  zarejestrujRodzaj("stan_odchylenia_n", stanOdchyleniaN);
  zarejestrujRodzaj("nie_reguly", nieReguly);
  zarejestrujRodzaj("duzy_wyciek", duzyWyciek);
  zarejestrujRodzaj("prog_biezacy_wyjscie", progBiezacyWyjscie);
}

}  // namespace reguly
