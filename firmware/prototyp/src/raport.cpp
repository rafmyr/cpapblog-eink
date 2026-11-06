// SPDX-License-Identifier: GPL-3.0-or-later
// Treść raportu A4 (spec 5.13.3, 5.13.4). Składanie PDF: src/raport_pdf.cpp.
#include "raport.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "wspolne.h"
#include "wzorce.h"

namespace raport {
namespace {

const int OKRES = 90;  // ostatnie 90 nocy (5.13.2), też okno wzorców w raporcie ogólnym (R-LEK-30)
const int LIMIT_WZORCOW = 6;  // [PRÓG] R-LEK-12, R-LEK-30
const int TERAPIA_MIN = 15;   // M-01 ≥ 15 min [PRÓG]: noc z terapią (1.5, R-LEK-31)

std::string dm(const std::string& d) {  // "2026-08-03" → "3.08" (przykłady R-LEK-30)
  if (!dataOk(d)) return "–";
  int dzien = (d[8] - '0') * 10 + (d[9] - '0');
  return std::to_string(dzien) + "." + d.substr(5, 2);
}
std::string okresTekst(const std::string& od, const std::string& do_) {  // `07.09–05.12.2026` (kanwa)
  if (!dataOk(od) || !dataOk(do_)) return "–";
  return ddmm(od) + "–" + ddmm(do_) + "." + do_.substr(0, 4);
}
std::string bezKropki(std::string s) {
  while (!s.empty() && (s.back() == '.' || s.back() == ' ')) s.pop_back();
  return s;
}
std::string bezKontroli(const std::string& l) {  // linia C bez `Na kontroli: raport z K2.` (5.13.2)
  static const std::string K = " Na kontroli: raport z K2.";
  if (l.size() >= K.size() && l.compare(l.size() - K.size(), K.size(), K) == 0) return l.substr(0, l.size() - K.size());
  return l;
}
std::string zamien(std::string s, const std::string& co, const std::string& na) {
  for (size_t p = s.find(co); p != std::string::npos; p = s.find(co, p + na.size())) s.replace(p, co.size(), na);
  return s;
}
// liczba bez zbędnego `,0` (ustawienia: `EPAP 6–10`)
std::string krotka(double v) {
  if (std::isnan(v)) return "–";
  if (std::fabs(v - std::round(v)) < 0.05) return calkowita(v);
  return przecinek1(v);
}
bool waznaDlaL(const Noc& n) { return n.wazna && !nocNiepewna(n); }  // 5.12.3 w uproszczeniu: ważna, bez niepewnych
// Noce liczone przez wzorzec (pole `liczone_noce` katalogu, 5.12.3, R-SKI-27, R-LEK-17): wartości jak w
// src/reguly.cpp nocRodzaju(), noc z terapią M-01 ≥ 15 min [PRÓG].
enum class Liczone : unsigned char { WAZNE_BEZ_NIEPEWNYCH, WAZNE, Z_TERAPIA, Z_TERAPIA_BEZ_NIEPEWNYCH };
Liczone liczoneZNazwy(const std::string& s) {
  if (s == "z_terapia") return Liczone::Z_TERAPIA;
  if (s == "z_terapia_bez_niepewnych") return Liczone::Z_TERAPIA_BEZ_NIEPEWNYCH;
  if (s == "wazne") return Liczone::WAZNE;
  return Liczone::WAZNE_BEZ_NIEPEWNYCH;
}
bool liczona(const Noc& n, Liczone l) {
  bool zTerapia = !n.czesciowa && n.m01 >= 15;
  bool wazna = n.wazna && !n.czesciowa;
  switch (l) {
    case Liczone::Z_TERAPIA: return zTerapia;
    case Liczone::Z_TERAPIA_BEZ_NIEPEWNYCH: return zTerapia && !nocNiepewna(n);
    case Liczone::WAZNE: return wazna;
    default: return wazna && !nocNiepewna(n);
  }
}
bool terapia(const Noc& n) { return n.m01 >= TERAPIA_MIN; }
bool trybAsv(const std::string& t) { return t == "ASV" || t == "ASVAuto"; }

double sredniaV(const std::vector<double>& v) {
  if (v.empty()) return NAN;
  double s = 0;
  for (double x : v) s += x;
  return s / (double)v.size();
}

// ---------- parametr kluczowy (5.13.4) ----------
struct Parametr {
  std::string metryka;   // id metryki dla reguly::metryka(); "M-01h" = godziny z maską
  std::string nazwa;     // tytuł wykresu strefy 4
  std::string krotko;    // podpis liczb ściągi i wiersz tabelki
  float prog = NAN;
  std::string podpisProgu;
  bool brak = false;     // metryki nie ma w danych panelu (T88, GI): `brak pomiaru`
};
double wartosc(const Noc& n, const std::string& m) {
  if (m == "M-01h") return n.m01 < 0 ? NAN : n.m01 / 60.0;
  if (m == "M-03ca" && !std::isnan(n.ca)) return n.ca;
  return reguly::metryka(n, m);
}
Parametr parametr(const std::string& idL, const std::string& wariant, const std::string& tryb, const Noc* ost) {
  Parametr p;
  bool asv = trybAsv(tryb);
  if (idL == "L-01") {
    p = {"M-03ca", "Bezdechy centralne (CA/h), 90 nocy", "Centralne / h", 5, "przerywana = 5 / h (Liu, Chest 2017)", false};
  } else if (idL == "L-02") {
    p = {"M-09", "Oddychanie okresowe (min na noc), 90 nocy", "Oddychanie okresowe, min", NAN, "", false};
    if (wariant == "a") {
      p.prog = 20;
      p.podpisProgu = "przerywana = 20 min na noc";
    }
  } else if (idL == "L-03" || idL == "L-11") {
    p = {"M-02", "AHI, 90 nocy", "AHI / h", wariant == "5_10" || idL == "L-11" ? 5.0f : 10.0f, "", false};
    p.podpisProgu = std::string("przerywana = ") + (std::isnan(p.prog) ? "" : calkowita(p.prog)) + " / h";
  } else if (idL == "L-04" || idL == "L-07") {
    p = {asv ? "M-06p" : "M-05p", asv ? "EPAP P95 (cmH2O), 90 nocy" : "Ciśnienie P95 (cmH2O), 90 nocy",
         asv ? "EPAP P95, cmH2O" : "Ciśnienie P95, cmH2O", NAN, "", false};
    if (idL == "L-04" && ost) {
      p.prog = (float)(asv ? (std::isnan(ost->eax) ? ost->eep : ost->eax) : ost->mpa);
      if (!std::isnan(p.prog)) p.podpisProgu = "przerywana = maksimum ustawień (odczyt)";
    }
  } else if (idL == "L-05") {
    p = {"M-13", "Oddechy na minutę, 90 nocy", "Oddechy / min", NAN, "", false};
  } else if (idL == "L-06") {
    p = {"", "Minuty poniżej 88% (T88), 90 nocy", "T88, min", 5, "przerywana = 5 min < 88%", true};
  } else if (idL == "L-08") {
    p = {"M-01h", "Godziny z maską na noc, 90 nocy", "Godziny z maską", 4, "przerywana = 4 h", false};
  } else if (idL == "L-09") {
    p = {"M-07", "Wyciek P95 (L/min), 90 nocy", "Wyciek P95, L/min", 24, "przerywana = 24 L/min", false};
  } else if (idL == "L-10") {
    p = {"M-10", "Ograniczenie przepływu (M-10), 90 nocy", "Ograniczenie przepływu", NAN, "", false};
  } else {  // raport ogólny, L-12 (metryka L-12 do doprecyzowania, w raporcie): AHI
    p = {"M-02", "AHI, 90 nocy", "AHI / h", NAN, "", false};
  }
  return p;
}

struct Dane {
  const Wejscie& we;
  std::vector<std::string> daty;            // 90 dat poranka do dzis
  std::map<std::string, const Noc*> noc;    // data → rekord
  const Noc* ostatnia = nullptr;
  std::string tryb;
  explicit Dane(const Wejscie& w) : we(w) {}
  const Noc* n(const std::string& d) const {
    auto i = noc.find(d);
    return i == noc.end() ? nullptr : i->second;
  }
};

std::string opisUstawien(const Noc& n) {  // strefa 6 i wykrywanie zmian: tylko odczyt (ADR-0002)
  std::string t = trybNocy(n);
  std::vector<double> u;  // z pola `ustawienia` (MOP/EAI/EAX/ANS/AXS) dla nocy z karty
  {
    std::string cur;
    std::vector<std::string> f;
    for (char c : n.ustawienia + "/") {
      if (c == '/') {
        f.push_back(cur);
        cur.clear();
      } else {
        cur += c;
      }
    }
    for (size_t i = 1; i < f.size(); i++) u.push_back(f[i].empty() ? NAN : liczbaRzecz(f[i].c_str()));
  }
  auto z = [&u](float pole, size_t i) { return !std::isnan(pole) ? (double)pole : (i < u.size() ? u[i] : NAN); };
  if (t == "ASVAuto")
    return "ASVAuto · EPAP " + krotka(z(n.eai, 0)) + "–" + krotka(z(n.eax, 1)) + " · PS " + krotka(z(n.ans, 2)) + "–" +
           krotka(z(n.axs, 3));
  if (t == "ASV") return "ASV · EPAP " + krotka(n.eep) + " · PS " + krotka(n.mns) + "–" + krotka(n.mxs);
  if (t == "AutoSet") return "AutoSet · ciśnienie " + krotka(n.mpi) + "–" + krotka(n.mpa) + " · EPR " + krotka(n.epr);
  if (t == "CPAP") return "CPAP · ciśnienie " + krotka(n.ipc) + " · EPR " + krotka(n.epr);
  return "";
}
// Zmiany ustawień w okresie: data poranka pierwszej nocy z nowymi ustawieniami, stare, nowe
struct Zmiana {
  std::string data, stare, nowe;
};
std::vector<Zmiana> zmianyUstawien(const Dane& d, const std::string& od) {
  std::vector<Zmiana> r;
  std::string pop;
  for (auto& kv : d.noc) {
    if (kv.first > d.we.dzis) break;
    std::string o = opisUstawien(*kv.second);
    if (o.empty()) continue;
    if (!pop.empty() && o != pop && kv.first >= od) r.push_back({kv.first, pop, o});
    pop = o;
  }
  return r;
}

const reguly::Wynik* glownyWynik(const Wejscie& w, const std::string& klucz) {
  if (!w.wyniki) return nullptr;
  for (auto& x : *w.wyniki)
    if (wzorce::wierszL(x) && !wzorce::wierszPomocniczy(x) && wzorce::kluczWzorca(*w.wyniki, x) == klucz) return &x;
  return nullptr;
}
const sprawy::SprawaL* sprawa(const Wejscie& w, const std::string& klucz) {
  if (!w.sprawy) return nullptr;
  for (auto& s : *w.sprawy)
    if (s.klucz == klucz) return &s;
  return nullptr;
}
std::string tekstRoli(const Wejscie& w, const std::string& klucz, sprawy::Rola rola) {
  const reguly::Wynik* x = glownyWynik(w, klucz);
  if (!x || !w.zestaw || !w.tekst) return "";
  const reguly::WierszKatalogu* k = sprawy::wierszKatalogu(*w.zestaw, *x);
  return k ? sprawy::tekstRoli(*w.zestaw, *k, *x, rola, w.tekst) : "";
}
std::string tytul(const Wejscie& w, const std::string& klucz) {
  if (const sprawy::SprawaL* s = sprawa(w, klucz))
    if (!s->tytul.empty()) return s->tytul;
  std::string t = tekstRoli(w, klucz, sprawy::Rola::TYTUL);
  return t.empty() ? klucz : t;
}
std::string linia(const Wejscie& w, const std::string& klucz) {
  if (const sprawy::SprawaL* s = sprawa(w, klucz))
    if (!s->linia.empty()) return bezKontroli(s->linia);
  return bezKontroli(tekstRoli(w, klucz, sprawy::Rola::LINIA));
}
// tekst roli z wiersza katalogu wyniku `x` (pytanie, ramka hipotezy)
std::string tekstWiersza(const Wejscie& w, const reguly::Wynik& x, const std::string& rola) {
  if (!w.zestaw || !w.tekst) return "";
  const reguly::WierszKatalogu* k = sprawy::wierszKatalogu(*w.zestaw, x);
  if (!k) return "";
  for (auto& id : k->teksty) {
    auto t = w.zestaw->teksty.find(id);
    if (t != w.zestaw->teksty.end() && t->second.rola == rola) return w.tekst(id, x);
  }
  return "";
}
// hipotezy H przy wzorcu (5.12.9): wiersz H z wariantem `idL` albo `idL~…`, AKTYWNA, oceniona i spełniona
std::vector<const reguly::Wynik*> hipotezy(const Wejscie& w, const std::string& idL) {
  std::vector<const reguly::Wynik*> r;
  if (!w.wyniki) return r;
  for (auto& x : *w.wyniki) {
    if (x.id.compare(0, 2, "H-") != 0 || x.wariant.find('.') != std::string::npos) continue;
    if (x.wariant != idL && x.wariant.compare(0, idL.size() + 1, idL + "~") != 0) continue;
    if (x.stan == reguly::StanReguly::AKTYWNA && x.oceniona && x.spelniona) r.push_back(&x);
  }
  return r;
}

// ---------- wykres i statystyki okien ----------
Wykres wykres(const Dane& d, const std::string& m, const std::string& m2) {
  Wykres k;
  k.daty = d.daty;
  for (auto& dt : d.daty) {
    const Noc* n = d.n(dt);
    k.w.push_back(n ? (float)wartosc(*n, m) : NAN);
    if (!m2.empty()) k.w2.push_back(n ? (float)wartosc(*n, m2) : NAN);
    char z = ' ';
    if (n && n->m01 > 0) z = !n->wazna ? 'x' : (nocNiepewna(*n) ? 'o' : 'p');
    k.znak.push_back(z);
  }
  return k;
}
// noce danego rodzaju z wartością metryki, od..do włącznie (daty poranka); `wykl` = noce wyłączone czynnikami
std::vector<double> wartosciOkna(const Dane& d, const std::string& m, const std::string& od, const std::string& do_,
                                 Liczone l = Liczone::WAZNE_BEZ_NIEPEWNYCH) {
  std::vector<double> v;
  for (auto& kv : d.noc) {
    if (kv.first < od || kv.first > do_) continue;
    double x = wartosc(*kv.second, m);
    if (liczona(*kv.second, l) && !std::isnan(x)) v.push_back(x);
  }
  return v;
}
// `od`: najwcześniejsza data okna ("" = bez granicy)
std::vector<double> ostatnieN(const Dane& d, const std::string& m, const std::string& do_, int n,
                              Liczone l = Liczone::WAZNE_BEZ_NIEPEWNYCH, const std::string& od = "",
                              const std::set<std::string>* wykl = nullptr) {
  std::vector<double> v;
  for (auto i = d.noc.rbegin(); i != d.noc.rend() && (int)v.size() < n; ++i) {
    if (i->first > do_) continue;
    if (!od.empty() && i->first < od) break;
    if (wykl && wykl->count(i->first)) continue;
    double x = wartosc(*i->second, m);
    if (liczona(*i->second, l) && !std::isnan(x)) v.push_back(x);
  }
  return v;
}

}  // namespace

std::string liczba(double v) { return przecinek1(v); }

Wariant wariantZKonfiguracji(const std::string& t0) {
  const std::string t = bezBom(t0);
  Wariant w = Wariant::PDF;
  size_t p = 0;
  while (p <= t.size()) {
    size_t e = t.find('\n', p);
    std::string l = t.substr(p, e == std::string::npos ? std::string::npos : e - p);
    p = e == std::string::npos ? t.size() + 1 : e + 1;
    obetnijKoniec(l);
    if (l == "raport=sciaga") w = Wariant::SCIAGA;
    if (l == "raport=pdf") w = Wariant::PDF;
  }
  return w;
}

uint8_t czynnikiZKonfiguracji(const std::string& t0, bool& ustawione) {
  const std::string t = bezBom(t0);
  ustawione = false;
  uint8_t b = 0;
  const std::string K = "raport.czynniki=";
  size_t p = 0;
  while (p <= t.size()) {
    size_t e = t.find('\n', p);
    std::string l = t.substr(p, e == std::string::npos ? std::string::npos : e - p);
    p = e == std::string::npos ? t.size() + 1 : e + 1;
    obetnijKoniec(l);
    if (l.compare(0, K.size(), K) != 0) continue;
    ustawione = true;
    b = 0;
    for (char c : l.substr(K.size()))
      for (auto& poz : POZYCJE)
        if (poz.kod == c && poz.bit != CZ_NIC) b = (uint8_t)(b | poz.bit);
  }
  return b;
}

Raport zbuduj(const Wejscie& we) {
  Raport r;
  Dane d(we);
  static const std::vector<Noc> PUSTA;
  const std::vector<Noc>& hist = we.historia ? *we.historia : PUSTA;
  for (auto& n : hist)
    if (n.data <= we.dzis && dataOk(n.data)) d.noc[n.data] = &n;
  d.ostatnia = d.noc.empty() ? nullptr : d.noc.rbegin()->second;
  d.tryb = d.ostatnia ? trybNocy(*d.ostatnia) : "";
  std::string pierwsza = d.noc.empty() ? we.dzis : d.noc.begin()->first;
  std::string od = dodajDni(we.dzis, -(OKRES - 1));
  if (od < pierwsza) od = pierwsza;
  for (std::string x = od; !x.empty() && x <= we.dzis; x = dodajDni(x, 1)) d.daty.push_back(x);
  r.od = od;
  r.do_ = d.ostatnia ? d.ostatnia->data : we.dzis;
  r.nocy = dataOk(r.do_) && r.do_ >= r.od ? dniMiedzy(r.od, r.do_) + 1 : 0;  // okres od..do z nagłówka
  for (auto& x : d.daty)
    if (const Noc* n = d.n(x)) r.waznych += n->wazna;
  bool asvWOkresie = false;
  for (auto& x : d.daty)
    if (const Noc* n = d.n(x)) asvWOkresie = asvWOkresie || trybAsv(trybNocy(*n));

  const sprawy::SprawaL* t = we.temat;
  r.rodzaj = t ? Rodzaj::TEMATYCZNY : Rodzaj::OGOLNY;
  if (t && t->id == "L-12") r.rodzaj = Rodzaj::PRZED_PO;
  r.klucz = t ? t->klucz : "";
  r.idL = t ? t->id : "";
  r.temat = t ? t->tytul : "Raport ogólny, 90 nocy";
  std::string wejscie = t ? (t->aktywnaOd.empty() ? we.dzis : t->aktywnaOd) : "";

  // 1 nagłówek
  std::string model = we.model;
  if (model.empty() && d.ostatnia) model = d.ostatnia->pna;
  model = zamien(model, "_", " ");
  r.s1tytul = model.empty() || d.tryb.empty() ? std::string("Dane z domowego aparatu do terapii snu")
                                              : "Dane z domowego aparatu " + model + ", tryb " + d.tryb;
  r.s1okres = okresTekst(r.od, r.do_) + " · " + std::to_string(r.nocy) + " nocy, " + std::to_string(r.waznych) +
              " ważnych";
  r.s1pacjent = "Pacjent: ______________________  ur.: ____________";

  // 2 dlaczego ten raport
  if (t) {
    std::string p = !t->powod.empty() ? t->powod : bezKontroli(t->linia);
    if (!p.empty()) r.s2.push_back(p);
  }
  if (we.odpowiedzi) {  // objawy zgłoszone na panelu (5.12.8)
    int tak = 0, ile = 0;
    std::vector<std::string> gorzej;
    std::string od28 = dodajDni(we.dzis, -27);
    for (auto& o : *we.odpowiedzi) {
      if (o.pytanie == sprawy::Pytanie::SENNOSC && o.data >= od28 && o.data <= we.dzis && o.odp != 'b') {
        ile++;
        tak += o.odp == 't';
      }
      if (o.pytanie == sprawy::Pytanie::GORZEJ && o.odp == 't' && o.data >= r.od && o.data <= we.dzis)
        gorzej.push_back(dm(o.data));
    }
    if (ile > 0)
      r.s2.push_back(std::string("„") + PYTANIE_SENNOSC + "”: „tak” w " + std::to_string(tak) + " z " +
                     std::to_string(ile) + (ile == 1 ? " ostatniego tygodnia." : " ostatnich tygodni."));
    if (!gorzej.empty()) {
      std::string z = "Odpowiedź „gorzej”: tak, ";
      for (size_t i = 0; i < gorzej.size(); i++) z += (i ? ", " : "") + gorzej[i];
      r.s2.push_back(z + ".");
    }
  }
  std::vector<Zmiana> zm = zmianyUstawien(d, r.od);
  if (!zm.empty()) {
    std::string z = zm.size() == 1 ? "Zmiana ustawień: " : "Zmiany ustawień: ";
    for (size_t i = 0; i < zm.size(); i++) z += (i ? ", " : "") + dm(zm[i].data);
    r.s2.push_back(z + ".");
  }
  // L-01 po zmianie ustawień (R-HIP-10): każdy tryb obsługiwany (także CPAP i AutoSet,
  // po przeglądzie klinicznym); zmiana w CPAP i AutoSet = ciśnienie albo EPR, w ASV i ASVAuto = EPAP albo PS
  const sprawy::SprawaL* l01 = t && t->id == "L-01" ? t : nullptr;  // także L-01 „tylko raport” (1.0)
  if (!l01 && we.sprawy)
    for (auto& s : *we.sprawy)
      if (s.id == "L-01") l01 = &s;
  if (l01 && trybObslugiwany(d.tryb)) {
    bool asvL01 = trybAsv(d.tryb);
    std::string wej = l01->aktywnaOd.empty() ? we.dzis : l01->aktywnaOd;
    std::string ch;
    for (auto& z : zmianyUstawien(d, "0000-00-00"))
      if (z.data <= wej) ch = z.data;
    const reguly::Wynik* po = nullptr;
    if (we.wyniki)
      for (auto& x : *we.wyniki)
        if (asvL01 ? (x.id == "L-01" && x.wariant == ".po_zmianie") : (x.id == "H-08" && x.wariant == "L-01~cpap"))
          po = &x;  // CPAP i AutoSet: H-08/L-01~cpap (56 nocy po zmianie ciśnienia albo EPR, katalog H)
    bool warunek = po ? (po->oceniona && po->spelniona) : (!ch.empty() && dniMiedzy(ch, wej) <= 56);  // R-HIP-10: 56 nocy;
    if (!ch.empty() && warunek)
      r.s2.push_back("Początek wzorca " + std::to_string(dniMiedzy(ch, wej)) + " nocy po zmianie ustawień " +
                     dm(ch) + ".");
  }
  if (t && we.pamiec) {  // R-LEK-16
    auto i = we.pamiec->find(t->klucz);
    std::string o = i == we.pamiec->end() ? "" : sprawy::dataWyciszenia(i->second.omowione);
    if (dataOk(o)) r.s2.push_back("Omówione z lekarzem: " + dm(o) + ".");
  }
  if (we.stany && we.historia)  // R-NOR-20: metryki zdrowotne w stanie odchylenia od ≥ 21 nocy
    for (auto& m : sprawy::sprawyMetryk(hist, *we.stany, we.dzis)) {
      if (m.nocy < 21) continue;
      std::string idM = m.nazwa == "AHI" ? "M-02" : "M-06p";
      if (we.niewalidowane.count(idM)) {  // R-LEK-19, R-DANE-28: bez liczb
        r.s2.push_back(m.nazwa + " poza normą osobistą od " + std::to_string(m.nocy) + " nocy.");
        continue;
      }
      std::string zw = m.p10 + "–" + m.p90;
      if (m.nazwa == "AHI" && we.stany->norma.ahi.jest()) zw = przecinek1(we.stany->norma.ahi.med);
      r.s2.push_back(m.nazwa + " poza normą osobistą od " + std::to_string(m.nocy) + " nocy: " + m.x + ", zwykle " +
                     zw + ".");
    }
  std::set<std::string> opisane;  // klucze już w strefie 2
  if (t) opisane.insert(t->klucz);
  if (r.rodzaj == Rodzaj::OGOLNY && we.pamiec) {  // R-LEK-30
    struct Z {
      bool trwa;
      std::string klucz, data, zdanie;
    };
    std::vector<Z> lista;
    std::string od90 = dodajDni(we.dzis, -(OKRES - 1));
    for (auto& kv : *we.pamiec) {
      if (kv.first.compare(0, 2, "L-") != 0) continue;
      wzorce::ZapisWzorca z = wzorce::odczytajZapis(kv.second.stanWzorca);
      if (z.pil != 'A' && z.pil != 'B' && z.pil != 'C' && z.pil != 'P') continue;
      bool trwa = z.stan != wzorce::StanWzorca::NIEAKTYWNY;
      std::string ti = tytul(we, kv.first);
      std::string zd;
      if (trwa) {
        if (z.pil == 'C' || z.pil == 'P') {
          std::string l = linia(we, kv.first);
          zd = (l.empty() ? ti + "." : l) + " Od " + dm(z.wejscie) + ".";
        } else {
          zd = ti + ": od " + dm(z.wejscie) + ", trwa.";
        }
      } else {
        if (z.pil == 'P' || !dataOk(z.wyjscie) || z.wyjscie < od90 || !dataOk(z.wejscie)) continue;
        zd = ti + ": od " + dm(z.wejscie) + " do " + dm(z.wyjscie) + ".";
      }
      std::string o = sprawy::dataWyciszenia(kv.second.omowione);
      if (dataOk(o) && o >= od90) zd += " Omówione z lekarzem: " + dm(o) + ".";
      lista.push_back({trwa, kv.first, trwa ? z.wejscie : z.wyjscie, zd});
      opisane.insert(kv.first);
    }
    std::stable_sort(lista.begin(), lista.end(), [](const Z& a, const Z& b) {
      if (a.trwa != b.trwa) return a.trwa;
      return a.data > b.data;
    });
    for (size_t i = 0; i < lista.size() && i < (size_t)LIMIT_WZORCOW; i++) r.s2.push_back(lista[i].zdanie);
    if (lista.size() > (size_t)LIMIT_WZORCOW)
      r.s2.push_back("i " + std::to_string(lista.size() - LIMIT_WZORCOW) + " starszych (w kokpicie)");
  }
  if (we.sprawy)
    for (auto& s : *we.sprawy) {
      if (!s.jestL() || opisane.count(s.klucz)) continue;
      if (s.tylkoRaport()) {  // ADR-0029, R-SKI-44
        std::string p = !s.powod.empty() ? s.powod : bezKontroli(s.linia);
        if (!p.empty()) r.s2.push_back(bezKropki(p) + " (bez oceny pilności).");
        opisane.insert(s.klucz);
      } else if (s.pilnosc == reguly::Pilnosc::C || s.ton == sprawy::Ton::PROBLEM) {  // C i L-09 (R-SKI-14, R-SKI-15)
        std::string l = bezKontroli(s.linia);
        if (!l.empty()) r.s2.push_back(l);
        opisane.insert(s.klucz);
      }
    }
  if (we.wyniki) {  // `{tytuł}: nie oceniam.` (R-SKI-44); nigdy jako „w porządku”
    std::string z;
    for (auto& kv : wzorce::nieocenione(*we.wyniki)) {
      if (opisane.count(kv.first)) continue;
      z += (z.empty() ? "" : " ") + tytul(we, kv.first) + ": nie oceniam.";
    }
    if (!z.empty()) r.s2.push_back(z);
  }
  if (we.czynniki) {  // R-CZY-06, 5.11.7
    uint8_t b = we.czynnikiUstawione ? we.czynnikiWlaczone : (r.idL == "L-01" ? (uint8_t)(CZ_KATAR | CZ_LEKI) : 0);
    std::string od14 = dodajDni(we.dzis, -13);
    for (auto& poz : POZYCJE) {
      if (!(b & poz.bit) || poz.bit == CZ_NIC) continue;
      int n = 0;
      for (auto& wp : *we.czynniki)
        if (wp.data >= od14 && wp.data <= we.dzis && wp.stan == 'O' && (wp.czynniki & poz.bit)) n++;
      if (n == 0) continue;
      if (poz.bit == CZ_POZA_DOMEM)
        r.s2.push_back("Bez danych z domu: " + std::to_string(n) + " z 14 nocy.");
      else
        r.s2.push_back(std::string("Zgłoszone przez pacjenta: ") + poz.etykieta + " w " + std::to_string(n) +
                       " z 14 nocy.");
    }
  }

  // 4 wykres kluczowy
  Parametr p = parametr(r.idL, t ? t->wariant : "", d.tryb, d.ostatnia);
  bool tylkoTrend = we.niewalidowane.count(p.metryka) > 0;
  // noce liczone według `liczone_noce` wzorca (L-08, L-09, L-11, L-12: noce z terapią, także < 4 h)
  Liczone lw = Liczone::WAZNE_BEZ_NIEPEWNYCH;
  if (t) {
    std::string ln;
    if (we.zestaw)
      for (auto& k : we.zestaw->katalog)
        if (k.id == t->id && k.wariant == t->wariant) ln = k.liczoneNoce;
    if (ln.empty() && (t->id == "L-08" || t->id == "L-09" || t->id == "L-11" || t->id == "L-12")) ln = "z_terapia";
    lw = liczoneZNazwy(ln);
  }
  r.s4 = wykres(d, p.metryka, "");
  r.s4.bezLiczb = tylkoTrend;
  r.s4.tytul = p.nazwa;
  if (!std::isnan(p.prog)) r.s4.linie.push_back(p.prog);
  for (auto& z : zm)
    for (size_t i = 0; i < d.daty.size(); i++)
      if (d.daty[i] == z.data) r.s4.zmiany.push_back((int)i);
  std::string przed = t ? dodajDni(wejscie, -1) : we.dzis;
  if (t)
    for (size_t i = 0; i < d.daty.size(); i++)
      if (d.daty[i] == wejscie) r.s4.poczatek = (int)i;
  std::vector<double> normaPrzed = ostatnieN(d, p.metryka, przed, 30, Liczone::WAZNE_BEZ_NIEPEWNYCH, "", we.wykluczone);
  if (normaPrzed.size() >= 14) {
    r.s4.pasmoOd = (float)percentyl(normaPrzed, 10);
    r.s4.pasmoDo = (float)percentyl(normaPrzed, 90);
  }
  bool jestWartosc = false;
  for (float x : r.s4.w) jestWartosc = jestWartosc || !std::isnan(x);
  r.s4.brakPomiaru = p.brak || !jestWartosc;
  r.s4.podpis = "kreski = norma pacjenta P10–P90 sprzed wzorca" + (p.podpisProgu.empty() ? "" : " · " + p.podpisProgu);
  if (!t) r.s4.podpis = "kreski = norma pacjenta P10–P90 (30 nocy)";
  auto komorka = [&](const std::vector<double>& v, bool trend) {
    if (trend) return std::string("trend");
    return v.empty() ? std::string("–") : przecinek1(sredniaV(v));
  };
  std::string od30 = dodajDni(we.dzis, -29);
  if (od30 < r.od) od30 = r.od;
  auto wiersz4 = [&](const std::string& nazwa, const std::string& m, Liczone l) {
    bool tr = we.niewalidowane.count(m) > 0;
    if (t) {
      return std::vector<std::string>{nazwa, komorka(ostatnieN(d, m, przed, 30, l), tr),
                                      komorka(wartosciOkna(d, m, wejscie, we.dzis, l), tr),
                                      komorka(ostatnieN(d, m, we.dzis, 7, l), tr)};
    }
    // raport ogólny tylko z okresu raportu (90 i 30 dni kalendarzowych, jak strefa 7)
    return std::vector<std::string>{nazwa, komorka(wartosciOkna(d, m, r.od, we.dzis, l), tr),
                                    komorka(wartosciOkna(d, m, od30, we.dzis, l), tr),
                                    komorka(ostatnieN(d, m, we.dzis, 7, l, r.od), tr)};
  };
  r.s4tab.kolumny = t ? std::vector<std::string>{"", "30 nocy przed", "od początku wzorca", "ostatnie 7"}
                      : std::vector<std::string>{"", "90 nocy", "30 nocy", "ostatnie 7"};
  if (r.s4.brakPomiaru) {
    r.s4tab.wiersze.push_back({p.krotko, "brak pomiaru", "brak pomiaru", "brak pomiaru"});
  } else {
    r.s4tab.wiersze.push_back(wiersz4(p.krotko, p.metryka, lw));
  }
  if (p.metryka != "M-02") r.s4tab.wiersze.push_back(wiersz4("AHI", "M-02", Liczone::WAZNE_BEZ_NIEPEWNYCH));
  r.s4legenda = "pełne kółko = noc ważna · puste kółko = noc niepewna (duży wyciek) · × = noc nieważna (krócej "
                "niż 4 h, częściowa albo bez zdarzeń); ";
  if (lw == Liczone::Z_TERAPIA)
    r.s4legenda += p.krotko + ": średnie ze wszystkich nocy z terapią, także pustych kółek i ×; AHI bez nich";
  else if (lw == Liczone::Z_TERAPIA_BEZ_NIEPEWNYCH)
    r.s4legenda += p.krotko + ": średnie z nocy z terapią, także ×, bez pustych kółek; AHI bez nich";
  else
    r.s4legenda += "puste i × nie wchodzą do średnich";
  if (tylkoTrend) r.s4legenda += " · liczby bez wartości: metryka niewalidowana, tylko trend (R-LEK-19)";

  // 3 pytanie do lekarza i 3b hipotezy (R-HIP-07, ADR-0012)
  std::vector<const reguly::Wynik*> h;
  const sprawy::SprawaL* tp = t;
  if (!tp && we.sprawy)
    for (auto& s : *we.sprawy)
      if (!tp && s.stan == reguly::StanReguly::AKTYWNA && s.jestL() &&
          (s.pilnosc == reguly::Pilnosc::A || s.pilnosc == reguly::Pilnosc::B))
        tp = &s;
  if (tp && tp->stan == reguly::StanReguly::AKTYWNA && we.podpowiedziH) h = hipotezy(we, tp->id);
  if (tp) {
    std::string q = h.empty() ? "" : tekstWiersza(we, *h.front(), "pytanie");
    if (q.empty()) {
      std::string wz = "{tytuł}: czy to wymaga korekty leczenia albo dalszej diagnostyki?";
      if (we.zestaw) {
        auto i = we.zestaw->teksty.find("L.pytanie_ogolne");
        if (i != we.zestaw->teksty.end()) wz = i->second.tekst;
      }
      q = zamien(wz, "{tytuł}", tp->tytul);
    }
    r.s3pytanie = q;
    r.s3dopisek = "Pytanie przygotowane automatycznie. Pacjent może je skreślić lub dopisać własne.";
  } else {
    r.s3dopisek = "Miejsce na pytania pacjenta.";
  }
  r.s3bJest = t && we.podpowiedziH && t->stan == reguly::StanReguly::AKTYWNA && !h.empty();  // R-HIP-07
  if (r.s3bJest) {
    for (size_t i = 0; i < h.size() && r.s3b.size() < 3; i++) {
      std::string l = tekstWiersza(we, *h[i], "ramka");
      if (!l.empty()) r.s3b.push_back(l);
    }
    r.s3bJest = !r.s3b.empty();
    r.s3bDopisek = "Hipoteza z danych urządzenia, do oceny lekarza.";
  }

  // 5 kontekst, te same 90 nocy
  {
    Wykres a = wykres(d, "M-02", "M-03ca");
    a.tytul = "AHI ze składowymi, / h";
    a.podpis = "ciągła = AHI, kropki = CA/h; poziome: 10 / h (AHI) i 5 / h (CA/h)";
    a.linie = {10, 5};
    r.s5.push_back(a);
    const Noc* o = d.ostatnia;
    if (trybAsv(d.tryb)) {
      Wykres e = wykres(d, "M-06", "M-06p");
      e.tytul = "EPAP, cmH2O";
      Wykres s = wykres(d, "M-28", "M-28p");
      s.tytul = "Wsparcie PS, cmH2O";
      if (o) {
        double emin = d.tryb == "ASVAuto" ? o->eai : o->eep, emax = d.tryb == "ASVAuto" ? o->eax : o->eep;
        double pmin = d.tryb == "ASVAuto" ? o->ans : o->mns, pmax = d.tryb == "ASVAuto" ? o->axs : o->mxs;
        for (double v : {emin, emax})
          if (!std::isnan(v)) e.linie.push_back((float)v);
        for (double v : {pmin, pmax})
          if (!std::isnan(v)) s.linie.push_back((float)v);
      }
      e.podpis = "ciągła = mediana, kropki = P95; przerywane = minimum i maksimum ustawień";
      s.podpis = e.podpis;
      r.s5.push_back(e);
      r.s5.push_back(s);
    } else {
      Wykres c = wykres(d, "M-05", "M-05p");
      c.tytul = "Ciśnienie, cmH2O";
      if (o)
        for (double v : {(double)o->mpi, (double)o->mpa, (double)o->ipc})
          if (!std::isnan(v)) c.linie.push_back((float)v);
      c.podpis = "ciągła = mediana, kropki = P95; przerywane = minimum i maksimum ustawień";
      r.s5.push_back(c);
    }
    Wykres l = wykres(d, "M-07", "");
    l.tytul = "Wyciek P95, L/min";
    l.linie = {24};
    l.podpis = "przerywana = 24 L/min";
    r.s5.push_back(l);
    static const char* const M5[][2] = {{"M-02", "M-03ca"}, {"M-06", "M-06p"}, {"M-28", "M-28p"}, {"M-07", "M-07"}};
    for (size_t i = 0; i < r.s5.size(); i++) {  // R-LEK-19: wykres metryki niewalidowanej bez liczb osi
      size_t j = i + 1 == r.s5.size() ? 3 : i;
      std::string m1 = M5[j][0], m2 = M5[j][1];
      if (!trybAsv(d.tryb) && i == 1) m1 = "M-05", m2 = "M-05p";
      r.s5[i].bezLiczb = we.niewalidowane.count(m1) || we.niewalidowane.count(m2);
    }
    for (auto& k : r.s5) {
      bool jest = false;
      for (float x : k.w) jest = jest || !std::isnan(x);
      k.brakPomiaru = !jest;
    }
    // R-LEK-35: udział centralnych i poziom bezwzględny, bez pilności i bez wykluczania nocy z czynnikami
    // udział: mediany z tych samych 14 ostatnich nocy ważnych dla L z CA/h i AHI
    std::vector<double> ca14, wsp14;
    for (auto i = d.noc.rbegin(); i != d.noc.rend() && ca14.size() < 14; ++i) {
      if (i->first > we.dzis || i->first < r.od || !waznaDlaL(*i->second)) continue;
      double c = wartosc(*i->second, "M-03ca"), ah = wartosc(*i->second, "M-02");
      if (std::isnan(c) || std::isnan(ah)) continue;
      ca14.push_back(c);
      wsp14.push_back(ah);
    }
    std::vector<double> ahi14 = ostatnieN(d, "M-02", we.dzis, 14, Liczone::WAZNE_BEZ_NIEPEWNYCH, r.od);
    std::vector<double> ca7 = ostatnieN(d, "M-03ca", we.dzis, 7, Liczone::WAZNE_BEZ_NIEPEWNYCH, r.od);
    double mca = mediana(ca14), mw = mediana(wsp14), mahi = mediana(ahi14), sca = sredniaV(ca7);
    std::string udz = std::isnan(mca) || std::isnan(mw) || mw <= 0 ? "–" : calkowita(100.0 * mca / mw);
    bool trCa = we.niewalidowane.count("M-03ca") > 0, trAhi = we.niewalidowane.count("M-02") > 0;
    if (trCa || trAhi)  // R-LEK-19, fail-path R-LEK-35: tylko trend, bez liczb
      r.s5zdania.push_back("centralne / AHI: tylko trend");
    else
      r.s5zdania.push_back("centralne " + (udz == "–" ? std::string("–") : udz + "%") + " AHI (mediany z " +
                           std::to_string(ca14.size()) + " nocy)");
    std::string zca = "CA/h, średnia z " + std::to_string(ca7.size()) + " nocy: " + (trCa ? "trend" : przecinek1(sca)) +
                      (ca7.empty() || trCa ? "" : " / h");
    if (!trCa && ca7.size() >= 7 && sca >= 5) zca += " · powyżej 5 / h";
    std::string zahi = "AHI, mediana z " + std::to_string(ahi14.size()) + " nocy: " +
                       (trAhi ? "trend" : przecinek1(mahi)) + (ahi14.empty() || trAhi ? "" : " / h");
    if (!trAhi && ahi14.size() >= 14 && mahi >= 10) zahi += " · powyżej 10 / h";
    r.s5zdania.push_back(zca);
    r.s5zdania.push_back(zahi);
  }

  // 6 ustawienia teraz (opis stanu, nie zalecenie; ADR-0002)
  std::string ust = d.ostatnia ? opisUstawien(*d.ostatnia) : "";
  r.s6.push_back(ust.empty() ? "–" : ust);
  r.s6.push_back("rampa – · maska – · nawilżanie –");
  r.s6zmiany.kolumny = {"data", "zmiana w okresie"};
  for (auto& z : zm) {  // ten sam tryb: bez powtarzania nazwy trybu po obu stronach
    std::string s = z.stare, n = z.nowe;
    size_t a = s.find(" · "), b = n.find(" · ");
    if (a != std::string::npos && a == b && s.compare(0, a, n, 0, b) == 0) {
      s = s.substr(a + 4);  // " · " ma 4 bajty (kropka środkowa w UTF-8 ma 2)
      n = n.substr(b + 4);
    }
    std::string zmiana = s;
    zmiana += " → ";
    zmiana += n;
    r.s6zmiany.wiersze.push_back({dm(z.data), zmiana});
  }
  if (zm.empty()) {
    r.s6zmiany.wiersze.push_back({"–", "Bez zmian w okresie raportu."});
    r.s6bezZmian = "Bez zmian w okresie raportu.";
  }

  // 7 regularność i podsumowanie (R-LEK-31): pule nocy jak w ResScan
  {
    r.s7.kolumny = {"", "30 nocy", "90 nocy"};
    struct Pula {
      int wszystkie = 0, ge4 = 0;
      long minut = 0;
      std::vector<const Noc*> terapia;
    };
    auto pula = [&](int dni) {
      Pula q;
      std::string od2 = dodajDni(we.dzis, -(dni - 1));
      if (od2 < pierwsza) od2 = pierwsza;
      for (std::string x = od2; !x.empty() && x <= we.dzis; x = dodajDni(x, 1)) {
        q.wszystkie++;
        const Noc* n = d.n(x);
        int m = n && n->m01 > 0 ? n->m01 : 0;
        q.minut += m;
        q.ge4 += m >= 240;
        if (n && terapia(*n)) q.terapia.push_back(n);
      }
      return q;
    };
    Pula p30 = pula(30), p90 = pula(90);
    auto med = [](const Pula& q, const std::string& m) {
      std::vector<double> v;
      for (const Noc* n : q.terapia) {
        double x = wartosc(*n, m);
        if (!std::isnan(x)) v.push_back(x);
      }
      return mediana(v);
    };
    auto proc = [](const Pula& q) {
      return q.wszystkie ? calkowita(100.0 * q.ge4 / q.wszystkie) + "%" : std::string("–");
    };
    auto sr = [](const Pula& q) { return q.wszystkie ? godzH((int)std::lround((double)q.minut / q.wszystkie)) : std::string("–"); };
    auto para = [&](const Pula& q, const std::string& a, const std::string& b) {
      bool tr = we.niewalidowane.count(a) || we.niewalidowane.count(b);
      return tr ? std::string("trend") : przecinek1(med(q, a)) + " / " + przecinek1(med(q, b));
    };
    std::string dop90 = p90.wszystkie < 90 ? " (z " + std::to_string(p90.wszystkie) + " nocy)" : "";
    r.s7.wiersze.push_back({"Noce ≥ 4 h", proc(p30), proc(p90) + dop90});
    r.s7.wiersze.push_back({"Średnio na noc", sr(p30), sr(p90)});
    if (trybAsv(d.tryb)) {
      r.s7.wiersze.push_back({"EPAP mediana / 95%", para(p30, "M-06", "M-06p"), para(p90, "M-06", "M-06p")});
      r.s7.wiersze.push_back({"Wsparcie PS mediana / 95%", para(p30, "M-28", "M-28p"), para(p90, "M-28", "M-28p")});
    } else {
      r.s7.wiersze.push_back({"Ciśnienie mediana / 95%", para(p30, "M-05", "M-05p"), para(p90, "M-05", "M-05p")});
    }
    auto jedna = [&](const Pula& q, const std::string& m) {
      return we.niewalidowane.count(m) ? std::string("trend") : przecinek1(med(q, m));
    };
    r.s7.wiersze.push_back({"Wyciek 95%", jedna(p30, "M-07"), jedna(p90, "M-07")});
    auto ahi = [&](const Pula& q) {
      if (we.niewalidowane.count("M-02")) return std::string("trend");
      return przecinek1(med(q, "M-02")) + " (" + przecinek1(med(q, "M-03oa")) + " / " + przecinek1(med(q, "M-03ca")) +
             " / " + przecinek1(med(q, "M-03ua")) + " / " + przecinek1(med(q, "M-03h")) + ")";
    };
    r.s7.wiersze.push_back({"AHI (OA / CA / UA / H)", ahi(p30), ahi(p90)});
    auto przerwy = [&](const Pula& q) { return calkowita(med(q, "M-17")); };
    r.s7.wiersze.push_back({"Przerwy w nocy", przerwy(p30), przerwy(p90)});
    bool spel = p30.wszystkie > 0 && p30.ge4 * 10 >= p30.wszystkie * 7;
    r.s7kryterium = std::string("Kryterium 4 h w 70% nocy: ") + (spel ? "spełnione" : "niespełnione");
    r.s7dopisek = "Noce ≥ 4 h i średnia: wszystkie noce okresu. Pozostałe wiersze: noce z terapią. Mediany nie sumują się do AHI.";
  }

  // 8 jakość danych
  {
    int wyciek = 0, krotkie = 0, brakD = 0;
    for (auto& x : d.daty) {
      const Noc* n = d.n(x);
      if (!n || n->m01 <= 0) {
        brakD++;
      } else if (!n->wazna) {
        krotkie++;
      } else if (nocNiepewna(*n)) {
        wyciek++;
      }
    }
    r.s8.push_back("Wyłączone z wyliczeń: " + std::to_string(wyciek + krotkie + brakD) + " nocy. " +
                   std::to_string(wyciek) + " × duży wyciek (P95 ≥ " + std::to_string((int)std::lround(NIEPEWNA_P95)) + " L/min albo ≥ 10% czasu terapii albo ≥ 30 min powyżej 24 L/min albo mediana > 30 L/min), " +
                   std::to_string(krotkie) + " × nieważna (krócej niż 4 h, częściowa albo bez zdarzeń), " + std::to_string(brakD) +
                   " × brak danych (noc bez terapii).");
    r.s8.push_back("Pełne przebiegi nocy: karta SD aparatu albo paczka EDF z panelu.");
  }

  // 9 czego te dane nie mówią (stała ramka; zdania o algorytmie aparatu: domysł P, do sprawdzenia w etapie 2)
  r.s9 = {"Zdarzenia liczy algorytm aparatu, nie polisomnografia; podział na centralne i obturacyjne jest przybliżony.",
          "Spłycenia oddechu aparat liczy bez desaturacji i wybudzeń, więc inaczej niż w zasadach AASM.",
          "Bezdechy centralne aparat oznacza także w czuwaniu.", "Aparat nie mierzy snu ani pozycji ciała.",
          "Bez oksymetru: brak oceny natlenienia krwi.", "Glasgow Index nie ma walidacji klinicznej.",
          "Norma to własne 30 nocy pacjenta, nie norma populacyjna.", ZDANIE_R_LEK_28};
  if (asvWOkresie) r.s9.push_back(ZDANIE_R_LEK_34);

  // 10 stopka
  std::string kiedy = dataZRokiem(we.dzis);
  if (kiedy.size() > 0 && kiedy[1] == '.') kiedy = "0" + kiedy;
  r.s10.push_back("Panel CPAP " + (we.wersjaPanelu.empty() ? std::string("–") : we.wersjaPanelu) + " · AirBridge " +
                  (we.wersjaAirBridge.empty() ? std::string("–") : we.wersjaAirBridge) + " · wygenerowano " + kiedy +
                  (we.czas.empty() ? "" : " " + we.czas));
  r.s10.push_back(ZDANIE_STOPKI);

  // ekran-ściąga
  r.sciagaPytanie = r.s3pytanie;
  r.sciagaPodpis = p.krotko + " · okres " + okresTekst(r.od, r.do_);
  for (int i = 0; i < 3; i++) r.sciagaLiczby[i] = r.s4tab.wiersze.empty() ? "–" : r.s4tab.wiersze[0][(size_t)i + 1];
  return r;
}

std::string nazwaPliku(const Raport& r, const std::string& dzis, const std::vector<std::string>& istniejace) {
  std::string t = r.rodzaj == Rodzaj::OGOLNY ? "ogolny" : r.rodzaj == Rodzaj::PRZED_PO ? "przed-po" : zamien(r.klucz, "/", "-");
  std::string t2;
  for (char c : t)
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') t2 += c;
  std::string baza = "/raporty/" + dzis + "-" + t2;
  std::string n = baza + ".pdf";
  for (int i = 2; std::find(istniejace.begin(), istniejace.end(), n) != istniejace.end(); i++)
    n = baza + "-" + std::to_string(i) + ".pdf";
  return n;
}

std::vector<std::string> wierszeTekstu(const Raport& r) {
  std::vector<std::string> w = {r.s1tytul, r.s1okres, r.s1pacjent, "DLACZEGO TEN RAPORT"};
  w.insert(w.end(), r.s2.begin(), r.s2.end());
  w.push_back(r.s4.tytul);
  w.push_back(r.s4.podpis);
  if (r.s4.brakPomiaru) w.push_back("brak pomiaru");
  for (auto& x : r.s4tab.wiersze)
    for (auto& c : x) w.push_back(c);
  w.push_back(r.s4legenda);
  w.push_back("PYTANIE DO LEKARZA");
  if (!r.s3pytanie.empty()) w.push_back(r.s3pytanie);
  w.push_back(r.s3dopisek);
  if (r.s3bJest) {
    w.push_back("CO NAJPEWNIEJ DO POPRAWY");
    w.insert(w.end(), r.s3b.begin(), r.s3b.end());
    w.push_back(r.s3bDopisek);
  }
  w.push_back("KONTEKST, TE SAME 90 NOCY");
  for (auto& k : r.s5) {
    w.push_back(k.tytul);
    if (k.brakPomiaru) w.push_back("brak pomiaru");
    w.push_back(k.podpis);
  }
  w.insert(w.end(), r.s5zdania.begin(), r.s5zdania.end());
  w.push_back("USTAWIENIA TERAZ");
  w.insert(w.end(), r.s6.begin(), r.s6.end());
  for (auto& x : r.s6zmiany.wiersze)
    for (auto& c : x) w.push_back(c);
  w.push_back("JAKOŚĆ DANYCH");
  w.insert(w.end(), r.s8.begin(), r.s8.end());
  w.push_back("REGULARNOŚĆ I PODSUMOWANIE");
  for (auto& x : r.s7.wiersze)
    for (auto& c : x) w.push_back(c);
  w.push_back(r.s7kryterium);
  w.push_back(r.s7dopisek);
  w.push_back("CZEGO TE DANE NIE MÓWIĄ");
  w.insert(w.end(), r.s9.begin(), r.s9.end());
  w.insert(w.end(), r.s10.begin(), r.s10.end());
  return w;
}

}  // namespace raport
