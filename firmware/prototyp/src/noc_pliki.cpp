// SPDX-License-Identifier: GPL-3.0-or-later
#include "noc_pliki.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "wspolne.h"

namespace noc_pliki {

namespace {

// id kanału, etykieta sygnału, plik, jednostka wyniku (KANALY w noce.py, CHANNELS w edf_report.cpp)
struct KanalPliku {
  const char* id;
  const char* etykieta;
  Rodzaj plik;
  const char* jednostka;
};
const KanalPliku KANALY[] = {
    {"Press", "Press.2s", Rodzaj::PLD, "cmH2O"},     {"EprPress", "EprPress.2s", Rodzaj::PLD, "cmH2O"},
    {"MaskPress", "MaskPress.2s", Rodzaj::PLD, "cmH2O"}, {"Leak", "Leak.2s", Rodzaj::PLD, "L/min"},
    {"MinVent", "MinVent.2s", Rodzaj::PLD, "L/min"}, {"RespRate", "RespRate.2s", Rodzaj::PLD, "bpm"},
    {"TidVol", "TidVol.2s", Rodzaj::PLD, "mL"},      {"SpO2", "SpO2.1s", Rodzaj::SAD, "%"},
    {"Pulse", "Pulse.1s", Rodzaj::SAD, "bpm"},       {"FlowLim", "FlowLim.2s", Rodzaj::PLD, ""},
    {"Snore", "Snore.2s", Rodzaj::PLD, ""},          {"TgtVent", "TgtVent.2s", Rodzaj::PLD, "L/min"},
};

// Zdarzenia z plików EVE (EventKind w report_edf.cpp): tekst → rodzaj
struct RodzajZdarzenia {
  const char* tekst;
  const char* rodzaj;
};
const RodzajZdarzenia RODZAJE[] = {{"Obstructive Apnea", "oa"}, {"Central Apnea", "ca"}, {"Apnea", "ua"},
                                   {"Hypopnea", "h"},          {"Arousal", "rera"},     {"RERA", "rera"}};
const char* const CSR_START = "CSR Start";
const char* const CSR_KONIEC = "CSR End";

const char* rodzajZdarzenia(const std::string& t) {
  for (auto& r : RODZAJE)
    if (t == r.tekst) return r.rodzaj;
  return nullptr;
}

double skala(const std::string& id, const std::string& jednostka) {
  if (id == "Leak" && jednostka == "L/s") return 60.0;
  if (id == "TidVol" && jednostka == "L") return 1000.0;
  return 1.0;
}

bool maWykres(const edf::Plik& p, Rodzaj r) {
  if (r == Rodzaj::BRP) return p.sygnal("Flow.40ms") != nullptr;
  for (auto& k : KANALY)
    if (k.plik == r && p.sygnal(k.etykieta)) return true;
  return false;
}

// (przedziały, kompletne) z par CSR Start/End w kolejności z listy (CsrPairing, _pary_csr w noce.py)
bool paryCsr(const std::vector<std::pair<double, std::string>>& z, std::vector<std::pair<double, double>>& przedzialy) {
  bool kompletne = true, jestStart = false;
  double start = 0;
  for (auto& e : z) {
    if (e.second == CSR_START) {
      if (jestStart) kompletne = false;
      start = e.first;
      jestStart = true;
    } else if (e.second == CSR_KONIEC) {
      if (jestStart && e.first > start) przedzialy.push_back({start, e.first});
      else kompletne = false;
      jestStart = false;
    }
  }
  if (jestStart) kompletne = false;
  return kompletne;
}

double runda(double x, double mn) { return std::nearbyint(x * mn) / mn; }  // round(x, n) w Pythonie

Json tekstJ(const std::string& s) {
  Json j;
  j.typ = Json::TEKST;
  j.s = s;
  return j;
}
Json liczbaJ(double v) {
  Json j;
  if (std::isnan(v)) {
    j.typ = Json::NUL;
    return j;
  }
  j.typ = Json::LICZBA;
  j.n = v;
  return j;
}
void dodaj(Json& o, const char* k, Json v) {
  o.klucze.push_back(k);
  o.el.push_back(std::move(v));
}

std::string opis(const std::string& nazwa, const char* co) { return nazwa + ": " + co; }

}  // namespace

// ---------- nazwy ----------
static bool cyfry(const std::string& t, size_t a, size_t n) {
  for (size_t i = a; i < a + n; i++)
    if (t[i] < '0' || t[i] > '9') return false;
  return true;
}

Rodzaj rodzajPliku(const std::string& nazwa, std::string* prefiks, long long* czas) {
  // ^\d{8}_\d{6}_(BRP|PLD|SAD|EVE|CSL)\.edf$ bez względu na wielkość liter
  if (nazwa.size() != 23 || !cyfry(nazwa, 0, 8) || nazwa[8] != '_' || !cyfry(nazwa, 9, 6) || nazwa[15] != '_' ||
      nazwa[19] != '.')
    return Rodzaj::INNY;
  std::string r, e;
  for (size_t i = 16; i < 19; i++) r += (char)std::toupper((unsigned char)nazwa[i]);
  for (size_t i = 20; i < 23; i++) e += (char)std::tolower((unsigned char)nazwa[i]);
  if (e != "edf") return Rodzaj::INNY;
  Rodzaj w = r == "BRP" ? Rodzaj::BRP : r == "PLD" ? Rodzaj::PLD : r == "SAD" ? Rodzaj::SAD
           : r == "EVE" ? Rodzaj::EVE : r == "CSL" ? Rodzaj::CSL : Rodzaj::INNY;
  if (w == Rodzaj::INNY) return w;
  if (prefiks) *prefiks = nazwa.substr(0, 15);
  if (czas) {
    // same cyfry (sprawdzone wyżej), więc strtol bez błędów zakresu
    auto n = [&](size_t a, size_t k) { return std::strtol(nazwa.substr(a, k).c_str(), nullptr, 10); };
    long rok = n(0, 4), mi = n(4, 2), d = n(6, 2), g = n(9, 2), m = n(11, 2), s = n(13, 2);
    char b[16];
    snprintf(b, sizeof b, "%04ld-%02ld-%02ld", rok, mi, d);
    *czas = dataOk(b) && g < 24 && m < 60 && s < 60
                ? (long long)nrDnia(b) * 86400LL + (long long)g * 3600LL + (long long)m * 60LL + (long long)s
                : -1;
  }
  return w;
}

const char* nazwaRodzaju(Rodzaj r) {
  switch (r) {
    case Rodzaj::BRP: return "BRP";
    case Rodzaj::PLD: return "PLD";
    case Rodzaj::SAD: return "SAD";
    case Rodzaj::EVE: return "EVE";
    case Rodzaj::CSL: return "CSL";
    case Rodzaj::INNY: break;
  }
  return "?";
}

std::string nazwaZeSciezki(const std::string& s) {
  size_t k = s.rfind('/');
  return k == std::string::npos ? s : s.substr(k + 1);
}

double percentylZLicznikow(const std::map<double, uint64_t>& m, int procent) {
  uint64_t n = 0;
  for (auto& x : m) n += x.second;
  if (!n) return NAN;
  uint64_t p = (uint64_t)procent;
  uint64_t ranga = (n / 100) * p + ((n % 100) * p + 99) / 100;
  if (ranga < 1) ranga = 1;
  if (ranga > n) ranga = n;
  for (auto& x : m) {
    if (ranga <= x.second) return x.first;
    ranga -= x.second;
  }
  return m.rbegin()->first;
}

// ---------- licznik ----------
bool LicznikNocy::Zdarzenie::operator<(const Zdarzenie& o) const {
  if (t != o.t) return t < o.t;
  if (maTrwanie != o.maTrwanie) return !maTrwanie;
  if (maTrwanie && trwanie != o.trwanie) return trwanie < o.trwanie;
  return tekst < o.tekst;
}

LicznikNocy::LicznikNocy(long dzien, const Parametry& p) : dzien_(dzien), par_(p) {}

void LicznikNocy::dodajNazwe(const std::string& nazwa) {
  std::string pref;
  long long t = -1;
  Rodzaj r = rodzajPliku(nazwa, &pref, &t);
  if (r == Rodzaj::INNY || indeks(nazwa) >= 0) return;
  Plik f;
  f.nazwa = nazwa;
  f.rodzaj = r;
  f.czas = t;
  if ((r == Rodzaj::BRP || r == Rodzaj::PLD || r == Rodzaj::SAD) && t < 0) {
    uwagi_.push_back(opis(nazwa, "zła nazwa pliku"));
    return;
  }
  pliki_.push_back(f);
  zgrupowane_ = false;
}

int LicznikNocy::indeks(const std::string& nazwa) const {
  for (size_t i = 0; i < pliki_.size(); i++)
    if (pliki_[i].nazwa == nazwa) return (int)i;
  return -1;
}

// Sesje po prefiksie czasu (noc_z_katalogu w noce.py): plik BRP, PLD albo SAD dołącza do grupy, która nie ma jeszcze
// pliku tego rodzaju i zaczęła się najwyżej TOLERANCJA_SESJI_S wcześniej; kolejność po (czas, prefiks, rodzaj, nazwa).
void LicznikNocy::grupuj() {
  if (zgrupowane_) return;
  grupy_.clear();
  std::vector<int> num;
  for (size_t i = 0; i < pliki_.size(); i++) {
    pliki_[i].sesja = -1;
    if (pliki_[i].rodzaj == Rodzaj::BRP || pliki_[i].rodzaj == Rodzaj::PLD || pliki_[i].rodzaj == Rodzaj::SAD)
      num.push_back((int)i);
  }
  std::sort(num.begin(), num.end(), [&](int a, int b) {
    const Plik &x = pliki_[(size_t)a], &y = pliki_[(size_t)b];
    if (x.czas != y.czas) return x.czas < y.czas;
    std::string px = x.nazwa.substr(0, 15), py = y.nazwa.substr(0, 15);
    if (px != py) return px < py;
    if (x.rodzaj != y.rodzaj) return std::strcmp(nazwaRodzaju(x.rodzaj), nazwaRodzaju(y.rodzaj)) < 0;
    return x.nazwa < y.nazwa;
  });
  for (int i : num) {
    Plik& f = pliki_[(size_t)i];
    int k = f.rodzaj == Rodzaj::BRP ? 0 : f.rodzaj == Rodzaj::PLD ? 1 : 2;
    bool dolaczony = false;
    for (size_t g = 0; g < grupy_.size(); g++) {
      long long d = f.czas - grupy_[g].t0;
      if (grupy_[g].pliki[k] < 0 && d >= 0 && d <= TOLERANCJA_SESJI_S) {
        grupy_[g].pliki[k] = i;
        f.sesja = (int)g;
        dolaczony = true;
        break;
      }
    }
    if (!dolaczony) {
      Grupa g;
      g.t0 = f.czas;
      g.prefiks = f.nazwa.substr(0, 15);
      g.pliki[k] = i;
      f.sesja = (int)grupy_.size();
      grupy_.push_back(g);
    }
  }
  zgrupowane_ = true;
}

std::vector<std::string> LicznikNocy::kolejnosc(bool zBrp, bool zSad) const {
  const_cast<LicznikNocy*>(this)->grupuj();
  std::vector<std::string> w;
  for (auto& g : grupy_) {
    if (zBrp && g.pliki[0] >= 0) w.push_back(pliki_[(size_t)g.pliki[0]].nazwa);
    if (g.pliki[1] >= 0) w.push_back(pliki_[(size_t)g.pliki[1]].nazwa);
    if (zSad && g.pliki[2] >= 0) w.push_back(pliki_[(size_t)g.pliki[2]].nazwa);
  }
  for (Rodzaj r : {Rodzaj::EVE, Rodzaj::CSL}) {
    std::vector<std::string> v;
    for (auto& f : pliki_)
      if (f.rodzaj == r) v.push_back(f.nazwa);
    std::sort(v.begin(), v.end());
    w.insert(w.end(), v.begin(), v.end());
  }
  return w;
}

void LicznikNocy::brakPliku(const std::string& nazwa, const std::string& powod) {
  int i = indeks(nazwa);
  if (i < 0) return;
  Plik& f = pliki_[(size_t)i];
  if (f.przeczytany || f.blad) return;
  f.blad = true;
  problemy_.push_back(nazwa + ": " + powod);
  if (f.rodzaj == Rodzaj::EVE) eveBlad_ = true;
  if (f.rodzaj == Rodzaj::CSL) cslBlad_ = true;
}

void LicznikNocy::zbierzKanaly(const edf::Plik& p, Rodzaj r, std::map<std::string, std::map<double, uint64_t>>& cel,
                               double& wysoki, double& b27, bool& maPrzeciek) {
  for (auto& k : KANALY) {
    if (k.plik != r) continue;
    const edf::Sygnal* s = p.sygnal(k.etykieta);
    if (!s) continue;
    double sk = skala(k.id, s->jednostka);
    double krok = p.czasRekordu / s->probki;
    std::map<long, uint64_t> cyf;  // wartość cyfrowa → liczba próbek (zliczenia() w edf.py)
    for (long rek = 0; rek < p.rekordy; rek++)
      for (int i = 0; i < s->probki; i++) cyf[p.probka(*s, rek, i)]++;
    std::map<double, uint64_t>& licznik = cel[k.id];
    bool leak = std::strcmp(k.id, "Leak") == 0;
    for (auto& x : cyf) {
      if (!s->poprawna(x.first)) continue;
      double v = s->fizyczna(x.first, sk);
      licznik[v] += x.second;
      if (leak) {
        maPrzeciek = true;
        if (v > PRZECIEK_AIRBRIDGE) wysoki += krok * (double)x.second;
        if (v > par_.przeciekBardzoDuzy) b27 += krok * (double)x.second;
      }
    }
  }
}

void LicznikNocy::zbierzWsparcie(const edf::Plik& p, const std::string& nazwa) {
  const edf::Sygnal *sp = p.sygnal("Press.2s"), *se = p.sygnal("EprPress.2s");
  if (!sp || !se) return;
  if (sp->probki != se->probki) {
    uwagi_.push_back(nazwa + ": Press.2s i EprPress.2s z różną liczbą próbek, PS nieliczone");
    return;
  }
  std::map<std::pair<long, long>, uint64_t> pary;
  for (long r = 0; r < p.rekordy; r++)
    for (int i = 0; i < sp->probki; i++) pary[{p.probka(*sp, r, i), p.probka(*se, r, i)}]++;
  for (auto& x : pary)
    if (sp->poprawna(x.first.first) && se->poprawna(x.first.second))
      wsparcie_[runda(sp->fizyczna(x.first.first) - se->fizyczna(x.first.second), 1e6)] += x.second;
}

bool LicznikNocy::dodajPlik(const std::string& nazwa, const uint8_t* b, size_t n) {
  grupuj();
  int i = indeks(nazwa);
  if (i < 0) return false;
  Plik& f = pliki_[(size_t)i];
  if (f.przeczytany || f.blad) return false;  // ten sam plik drugi raz: bez podwójnego liczenia
  edf::Plik p;
  edf::Blad bl = edf::czytaj(b, n, p);
  if (bl != edf::Blad::BRAK) {
    f.blad = true;
    problemy_.push_back(opis(nazwa, edf::opisBledu(bl)));
    if (f.rodzaj == Rodzaj::EVE) eveBlad_ = true;
    if (f.rodzaj == Rodzaj::CSL) cslBlad_ = true;
    return false;
  }
  if (p.niepelny) problemy_.push_back(opis(nazwa, "plik niepełny"));  // jak _otworz() w noce.py: noc częściowa
  if (p.edfPlusD)
    for (auto& s : p.sygnaly)
      if (!s.adnotacje() && !s.crc()) {
        problemy_.push_back(opis(nazwa, "EDF+D z sygnałami pomiarowymi (rekordy nieciągłe)"));
        break;
      }
  f.przeczytany = true;
  if (f.rodzaj == Rodzaj::EVE || f.rodzaj == Rodzaj::CSL) {
    std::vector<edf::Adnotacja> ad;
    if (!edf::adnotacje(p, ad)) {
      problemy_.push_back(opis(nazwa, "zła adnotacja"));
      if (f.rodzaj == Rodzaj::EVE) eveBlad_ = true;
      else cslBlad_ = true;
      return false;
    }
    if (f.rodzaj == Rodzaj::EVE) {
      for (auto& a : ad)
        zdarzenia_.insert({runda((double)p.startS + a.onset, 1e3), a.maTrwanie, a.trwanie, a.tekst});
    } else {
      std::vector<std::pair<double, std::string>> l;
      l.reserve(ad.size());
      for (auto& a : ad) l.push_back({(double)p.startS + a.onset, a.tekst});
      csl_.push_back(std::move(l));
    }
    return true;
  }
  if (maWykres(p, f.rodzaj) && p.rekordy && p.czasRekordu > 0) {
    f.maZakres = true;
    f.startS = (double)p.startS;
    f.koniecS = p.koniecS();
  }
  bool wSesji = f.sesja >= 0 && (grupy_[(size_t)f.sesja].pliki[0] >= 0 || grupy_[(size_t)f.sesja].pliki[1] >= 0);
  if (f.rodzaj == Rodzaj::PLD) {
    // granice zapisu: pierwszy i ostatni rekord z poprawną próbką ciśnienia (recorded_bounds)
    const edf::Sygnal* s = nullptr;
    for (const char* e : {"Press.2s", "EprPress.2s", "MaskPress.2s"}) {
      s = p.sygnal(e);
      if (s) break;
    }
    long pierwszy = -1, ostatni = -1;
    if (s)
      for (long r = 0; r < p.rekordy; r++)
        for (int k = 0; k < s->probki; k++)
          if (s->poprawna(p.probka(*s, r, k))) {
            if (pierwszy < 0) pierwszy = r;
            ostatni = r;
            break;
          }
    bool jest = pierwszy >= 0;
    zapisy_[i] = {jest, {(double)p.startS + (double)pierwszy * p.czasRekordu,
                         (double)p.startS + (double)(ostatni + 1) * p.czasRekordu}};
    uzycieZnane_[i] = p.rekordy == 0 || jest;
    zbierzKanaly(p, f.rodzaj, wartosci_, przeciekWysoki_, przeciek27_, maPrzeciek_);
    zbierzWsparcie(p, nazwa);
  } else if (f.rodzaj == Rodzaj::SAD) {
    if (wSesji) {
      zbierzKanaly(p, f.rodzaj, wartosci_, przeciekWysoki_, przeciek27_, maPrzeciek_);
    } else {  // SAD bez BRP i PLD z tym prefiksem: dołączany w wynik() do sesji, z którą nachodzi w czasie
      bool mp = false;
      zbierzKanaly(p, f.rodzaj, sadLuzem_[i], sadLuzemWysoki_[i], sadLuzem27_[i], mp);
      f.startS = (double)p.startS;
      f.koniecS = p.koniecS();
    }
  }
  return true;
}

size_t LicznikNocy::bajtyLicznikow() const {
  size_t n = wsparcie_.size();
  for (auto& k : wartosci_) n += k.second.size();
  for (auto& s : sadLuzem_)
    for (auto& k : s.second) n += k.second.size();
  return n * 48 + zdarzenia_.size() * 64 + pliki_.size() * 96;  // węzeł mapy ok. 48 B
}

WynikNocy LicznikNocy::wynik() const {
  const_cast<LicznikNocy*>(this)->grupuj();
  WynikNocy w;
  w.dzien = dzien_;
  std::vector<std::string> problemy = problemy_, uwagi = uwagi_;
  for (auto& f : pliki_)  // PLD, EVE i CSL bez treści i bez opisanego błędu: plik nie przetworzony
    if (!f.przeczytany && !f.blad && (f.rodzaj == Rodzaj::PLD || f.rodzaj == Rodzaj::EVE || f.rodzaj == Rodzaj::CSL))
      problemy.push_back(opis(f.nazwa, "plik nie przeczytany"));
  std::map<std::string, std::map<double, uint64_t>> wartosci = wartosci_;
  double wysoki = przeciekWysoki_, b27 = przeciek27_;
  bool brakPld = false, wszystkieUzycia = true;
  std::vector<std::pair<double, double>> zapisy;
  std::vector<int> sesjaGrupy;
  for (size_t g = 0; g < grupy_.size(); g++) {
    const Grupa& gr = grupy_[g];
    if (gr.pliki[0] < 0 && gr.pliki[1] < 0) continue;
    SesjaNocy s;
    s.prefiks = gr.prefiks;
    bool uzycie = false;
    for (int k = 0; k < 3; k++) {
      if (gr.pliki[k] < 0) {
        if (k == 1) brakPld = true;
        continue;
      }
      const Plik& f = pliki_[(size_t)gr.pliki[k]];
      if (k == 1 && !f.przeczytany) brakPld = true;
      if (!f.przeczytany || !f.maZakres) continue;
      if (!s.maZakres) {
        s.startS = f.startS;
        s.koniecS = f.koniecS;
      } else {
        s.startS = std::min(s.startS, f.startS);
        s.koniecS = std::max(s.koniecS, f.koniecS);
      }
      s.maZakres = true;
    }
    if (gr.pliki[1] >= 0 && pliki_[(size_t)gr.pliki[1]].przeczytany) {
      auto z = zapisy_.find(gr.pliki[1]);
      if (z != zapisy_.end() && z->second.first) {
        s.maZapis = true;
        s.zapisOd = z->second.second.first;
        s.zapisDo = z->second.second.second;
        zapisy.push_back({s.zapisOd, s.zapisDo});
      }
      auto u = uzycieZnane_.find(gr.pliki[1]);
      uzycie = u != uzycieZnane_.end() && u->second;
    }
    wszystkieUzycia = wszystkieUzycia && uzycie;
    w.sesje.push_back(s);
    sesjaGrupy.push_back((int)g);
  }
  // SAD luzem: do sesji z największą częścią wspólną (w kolejności grup)
  for (size_t g = 0; g < grupy_.size(); g++) {
    const Grupa& gr = grupy_[g];
    if (gr.pliki[0] >= 0 || gr.pliki[1] >= 0 || gr.pliki[2] < 0) continue;
    int i = gr.pliki[2];
    const Plik& f = pliki_[(size_t)i];
    if (!f.przeczytany) continue;
    SesjaNocy* najlepsza = nullptr;
    double wspolne = 0;
    for (auto& s : w.sesje) {
      if (!s.maZakres) continue;
      double x = std::min(f.koniecS, s.koniecS) - std::max(f.startS, s.startS);
      if (x > wspolne) {
        najlepsza = &s;
        wspolne = x;
      }
    }
    if (!najlepsza) {
      uwagi.push_back(opis(f.nazwa, "SAD bez sesji"));
      continue;
    }
    auto sl = sadLuzem_.find(i);
    if (sl != sadLuzem_.end())
      for (auto& k : sl->second)
        for (auto& v : k.second) wartosci[k.first][v.first] += v.second;
    if (f.maZakres) {
      najlepsza->startS = std::min(najlepsza->startS, f.startS);
      najlepsza->koniecS = std::max(najlepsza->koniecS, f.koniecS);
    }
  }
  double sekundy = 0;
  for (auto& z : zapisy) sekundy += z.second - z.first;

  // zdarzenia w czasie zapisu którejś sesji [od, do)
  bool jestEve = false;
  for (auto& f : pliki_) jestEve = jestEve || f.rodzaj == Rodzaj::EVE;
  bool eveOk = jestEve && !eveBlad_;
  for (auto& f : pliki_)
    if (f.rodzaj == Rodzaj::EVE && !f.przeczytany) eveOk = false;
  bool klasyfikacja = false;
  for (auto& z : zdarzenia_) klasyfikacja = klasyfikacja || z.tekst == "Obstructive Apnea" || z.tekst == "Central Apnea";
  std::map<std::string, long> liczniki;
  for (auto& z : zdarzenia_) {
    const char* r = rodzajZdarzenia(z.tekst);
    if (!r) {
      if (z.tekst != CSR_START && z.tekst != CSR_KONIEC) w.adnotacjeInne[z.tekst]++;
      continue;
    }
    for (auto& p : zapisy)
      if (p.first <= z.t && z.t < p.second) {
        liczniki[r]++;
        break;
      }
  }
  // oddychanie okresowe: pary z CSL (zapasowo z EVE), przycięte do czasu zapisu
  std::vector<std::vector<std::pair<double, std::string>>> zrodla = csl_;
  bool csrKompletne = !cslBlad_;
  for (auto& f : pliki_)
    if (f.rodzaj == Rodzaj::CSL && !f.przeczytany) csrKompletne = false;
  std::string csrZrodlo = csl_.empty() ? "" : "CSL";
  if (csrZrodlo.empty()) {
    std::vector<std::pair<double, std::string>> z;
    for (auto& e : zdarzenia_)
      if (e.tekst == CSR_START || e.tekst == CSR_KONIEC) z.push_back({e.t, e.tekst});
    std::sort(z.begin(), z.end());
    if (!z.empty()) {
      zrodla.push_back(z);
      csrZrodlo = "EVE";
    }
  }
  std::vector<std::pair<double, double>> przedzialy;
  for (auto& l : zrodla) csrKompletne = paryCsr(l, przedzialy) && csrKompletne;
  double csr = 0;
  for (auto& a : przedzialy)
    for (auto& z : zapisy) {
      double x = std::min(a.second, z.second) - std::max(a.first, z.first);
      if (x > 0) csr += x;
    }

  w.sesjeObecne = !w.sesje.empty();
  w.zdarzeniaZnane = eveOk && wszystkieUzycia && sekundy > 0;
  w.sekundy = wszystkieUzycia && sekundy > 0 ? sekundy : NAN;
  w.czesciowa = brakPld || (w.sesjeObecne && !jestEve) || !problemy.empty();
  w.klasyfikacja = w.zdarzeniaZnane && klasyfikacja;
  if (w.zdarzeniaZnane) {
    auto l = [&](const char* k) { return liczniki.count(k) ? liczniki[k] : 0L; };
    w.oa = klasyfikacja ? l("oa") : -1;
    w.ca = klasyfikacja ? l("ca") : -1;
    w.ua = l("ua");
    w.h = l("h");
    w.rera = l("rera");
    w.ahi = (double)(l("oa") + l("ca") + l("ua") + l("h")) * 3600.0 / sekundy;
    w.rdi = (double)(l("oa") + l("ca") + l("ua") + l("h") + l("rera")) * 3600.0 / sekundy;
  }
  w.csrZrodlo = csrZrodlo;
  if (!csrZrodlo.empty() && csrKompletne && wszystkieUzycia && sekundy > 0) w.csrS = csr;
  if (maPrzeciek_) {
    w.przeciekWysokiS = wysoki;
    w.przeciek27S = b27;
  }
  for (auto& k : KANALY) {
    auto it = wartosci.find(k.id);
    if (it == wartosci.end() || it->second.empty()) continue;
    Statystyka s;
    s.jest = true;
    s.med = percentylZLicznikow(it->second, 50);
    s.p95 = percentylZLicznikow(it->second, 95);
    s.min = it->second.begin()->first;
    s.max = it->second.rbegin()->first;
    w.kanaly[k.id] = s;
  }
  if (!wsparcie_.empty()) {
    w.ps.jest = true;
    w.ps.med = percentylZLicznikow(wsparcie_, 50);
    w.ps.p95 = percentylZLicznikow(wsparcie_, 95);
    w.ps.min = wsparcie_.begin()->first;
    w.ps.max = wsparcie_.rbegin()->first;
  }
  w.problemy = problemy;
  w.uwagi = uwagi;
  return w;
}

// ---------- M-17, M-18 (R-DANE-27) ----------
void m17m18(const WynikNocy& w, const Parametry& par, int& m17, double& m18) {
  std::vector<SesjaCywilna> v;
  v.reserve(w.sesje.size());
  for (auto& s : w.sesje) {
    SesjaCywilna x;
    if (s.maZakres) {  // ms jak int(start_s * 1000) w noce.py
      x.start = x.koniec = true;
      x.startS = (double)(long long)(s.startS * 1000) / 1000.0;
      x.koniecS = (double)(long long)(s.koniecS * 1000) / 1000.0;
    }
    v.push_back(x);
  }
  m17m18Sesji(v, w.dzien, par.startNocyMin, par.wczesnySenS, m17, m18);
}

// ---------- rekord nocy ----------
NocAB nocZWyniku(const WynikNocy& w, const Str* str, const std::string& trybZapasowy, const std::string& pna,
                 const std::string& srn, const Parametry& par) {
  NocAB r;
  Noc& n = r.noc;
  long rekStr = -1;
  if (str) {
    auto it = str->doby.find(w.dzien);
    if (it != str->doby.end()) rekStr = it->second;
  }
  n.data = dataPoranka(w.dzien);
  n.zrodlo = 'A';
  n.czesciowa = w.czesciowa;
  n.m01 = !std::isnan(w.sekundy) && w.sekundy >= 0 && w.sekundy <= 86400 ? (int)std::lround(w.sekundy / 60.0) : -1;
  n.ahi = std::isnan(w.ahi) ? NAN : (float)w.ahi;
  // kanały: z próbek EDF, bez nich statystyki STR (`source` = "str"), dalej wspólne reguły raportu
  Json kanaly;
  kanaly.typ = Json::TABLICA;
  for (auto& k : KANALY) {
    auto it = w.kanaly.find(k.id);
    Json c;
    c.typ = Json::OBIEKT;
    dodaj(c, "id", tekstJ(k.id));
    dodaj(c, "unit", tekstJ(k.jednostka));
    if (it != w.kanaly.end() && it->second.jest) {
      dodaj(c, "median", liczbaJ(it->second.med));
      dodaj(c, "p95", liczbaJ(it->second.p95));
      dodaj(c, "source", tekstJ("edf"));
    } else if (rekStr >= 0) {
      double med = statystykaStr(*str, rekStr, k.id, "50"), p95 = statystykaStr(*str, rekStr, k.id, "95"),
             mx = statystykaStr(*str, rekStr, k.id, "Max");
      if (std::isnan(med) && std::isnan(p95) && std::isnan(mx)) continue;
      dodaj(c, "median", liczbaJ(med));
      dodaj(c, "p95", liczbaJ(p95));
      dodaj(c, "source", tekstJ("str"));
    } else {
      continue;
    }
    kanaly.el.push_back(std::move(c));
  }
  bool kanalEdf = false;
  kanalyRaportu(n, kanaly, kanalEdf);
  auto press = w.kanaly.find("Press");
  if (press != w.kanaly.end() && press->second.jest && w.ps.jest) {  // PS tylko przy ciśnieniu z EDF (R-DANE-32)
    n.psmed = (float)w.ps.med;
    n.psp95 = (float)w.ps.p95;
  }
  if (!std::isnan(w.sekundy) && w.sekundy > 0) {
    auto naGodz = [&](long c) { return c >= 0 ? (float)((double)c * 3600.0 / w.sekundy) : NAN; };
    n.oa = naGodz(w.oa);
    n.ca = naGodz(w.ca);
    n.h = naGodz(w.h);
    n.ua = naGodz(w.ua);
  }
  if (!std::isnan(w.csrS)) n.csr = (float)(w.csrS / 60.0);
  if (!std::isnan(w.przeciekWysokiS)) n.m08 = (float)(w.przeciekWysokiS / 60.0);
  int m17 = 0;
  double m18 = NAN;
  m17m18(w, par, m17, m18);
  n.m17 = m17;
  n.m18 = !std::isnan(m18) && m18 >= 0 && m18 < 1440 ? (int)std::floor(m18) : -1;
  r.sesje = (int)w.sesje.size();
  Json ust;
  if (rekStr >= 0) {
    ust = ustawieniaStr(*str, rekStr);
  } else {
    ust.typ = Json::OBIEKT;
    Json f;
    f.typ = Json::BOOL;
    f.b = false;
    dodaj(ust, "available", f);
  }
  bool maMop = false;
  r.tryb = ustawieniaRaportu(n, ust, maMop);
  if (!maMop) r.tryb = trybZapasowy;
  n.pna = czystePole(pna);
  n.srn = czystePole(srn);
  bool obecna = w.sesjeObecne || rekStr >= 0;
  n.wazna = !n.czesciowa && obecna && w.zdarzeniaZnane && n.m01 >= 240 && trybObslugiwany(r.tryb);
  n.tryb = czystePole(r.tryb);
  r.pelna = !n.czesciowa && w.zdarzeniaZnane && kanalEdf;
  r.stan = n.czesciowa ? StanRaportu::CZESCIOWA : StanRaportu::KOMPLETNA;
  return r;
}

// ---------- metryki z JSON ----------
static const char* const METRYKI_JSON[] = {"m01", "m02", "m03", "m05", "m06", "m07", "m08",
                                           "m09", "m10", "m13", "m14", "m15", "m17", "m18", "m21", "m28"};

bool metrykaJsonOk(const std::string& id) {
  for (const char* m : METRYKI_JSON)
    if (id == m) return true;
  return false;
}

bool parsujMetrykiJson(const std::string& t, std::set<std::string>& wynik) {
  std::set<std::string> w;
  size_t a = 0;
  if (t.empty()) {
    wynik.clear();
    return true;
  }
  for (;;) {
    size_t b = t.find(',', a);
    std::string x = t.substr(a, b == std::string::npos ? std::string::npos : b - a);
    while (!x.empty() && x[0] == ' ') x.erase(0, 1);
    while (!x.empty() && x.back() == ' ') x.pop_back();
    for (auto& c : x) c = (char)std::tolower((unsigned char)c);
    if (x.size() == 4 && x[0] == 'm' && x[1] == '-') x.erase(1, 1);  // `M-08` też
    if (!metrykaJsonOk(x)) return false;
    w.insert(x);
    if (b == std::string::npos) break;
    a = b + 1;
  }
  wynik = w;
  return true;
}

void scalZJson(NocAB& p, const NocAB& j, const std::set<std::string>& m) {
  Noc& n = p.noc;
  const Noc& z = j.noc;
  const bool zdarzeniaPlikow = !std::isnan(n.ahi);  // AHI z plików jest tylko przy `events_known`
  auto ma = [&](const char* id) { return m.count(id) > 0; };
  if (ma("m01")) n.m01 = z.m01;
  if (ma("m02")) n.ahi = z.ahi;
  if (ma("m03")) { n.oa = z.oa; n.ca = z.ca; n.h = z.h; n.ua = z.ua; }
  if (ma("m05")) { n.cmed = z.cmed; n.cp95 = z.cp95; }
  if (ma("m06")) { n.emed = z.emed; n.ep95 = z.ep95; n.epapStr = z.epapStr; }
  if (ma("m07")) { n.wmed = z.wmed; n.wp95 = z.wp95; }
  if (ma("m08")) n.m08 = z.m08;
  if (ma("m09")) n.csr = z.csr;
  if (ma("m10")) { n.m10 = z.m10; n.m10p = z.m10p; }
  if (ma("m13")) n.m13 = z.m13;
  if (ma("m14")) n.m14 = z.m14;
  if (ma("m15")) n.m15 = z.m15;
  if (ma("m17")) n.m17 = z.m17;
  if (ma("m18")) n.m18 = z.m18;
  if (ma("m21")) {
    n.ustawienia = z.ustawienia;
    n.mpa = z.mpa; n.mpi = z.mpi; n.ipc = z.ipc; n.eep = z.eep; n.mns = z.mns; n.mxs = z.mxs;
    n.eai = z.eai; n.eax = z.eax; n.ans = z.ans; n.axs = z.axs; n.epr = z.epr;
    n.tryb = z.tryb;
    p.tryb = j.tryb;
  }
  if (ma("m28")) { n.psmed = z.psmed; n.psp95 = z.psp95; }
  // ważność od nowa (4.2): noc z plików kompletna ze znanymi zdarzeniami, AHI po scaleniu znane, M-01 ≥ 240 min,
  // tryb obsługiwany (M-01, M-02 i ustawienia mogą przyjść z raportu)
  n.wazna = !n.czesciowa && zdarzeniaPlikow && !std::isnan(n.ahi) && n.m01 >= 240 && trybObslugiwany(p.tryb);
}

// ---------- /api/sessions ----------
bool parsujListeSesji(const std::string& t, ListaSesji& wynik) {
  static const FiltrJson PLIK[] = {{"path", nullptr}, {"size", nullptr}, {nullptr, nullptr}};
  static const FiltrJson SES[] = {{"day", nullptr}, {"closed", nullptr}, {"files", PLIK}, {nullptr, nullptr}};
  static const FiltrJson F[] = {{"sessions", SES}, {"next_cursor", nullptr}, {nullptr, nullptr}};
  Json j;
  if (!parsujJson(t, j, F) || j.typ != Json::OBIEKT || j["sessions"].typ != Json::TABLICA) return false;
  ListaSesji l;
  for (auto& s : j["sessions"].el) {
    if (s.typ != Json::OBIEKT || s["day"].typ != Json::TEKST) return false;
    SesjaAB x;
    x.day = s["day"].s;
    x.closed = s["closed"].typ == Json::BOOL && s["closed"].b;
    for (auto& f : s["files"].el) {
      if (f.typ != Json::OBIEKT || f["path"].typ != Json::TEKST) return false;
      PlikAB p;
      p.sciezka = f["path"].s;
      double v;
      if (f["size"].liczba(v) && v >= 0) p.rozmiar = v;
      x.pliki.push_back(p);
    }
    l.sesje.push_back(x);
  }
  if (j["next_cursor"].typ == Json::TEKST) l.kursor = j["next_cursor"].s;
  wynik = l;
  return true;
}

std::string dzienAB(long dzien) {
  std::string d = dataZNr(dzien), w;
  for (char c : d)
    if (c != '-') w += c;
  return w;
}

static std::string koduj(const std::string& t, bool ukosnik) {
  std::string e;
  for (unsigned char c : t) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || (ukosnik && c == '/')) {
      e += (char)c;
    } else {
      char b[4];
      snprintf(b, sizeof b, "%%%02X", c);
      e += b;
    }
  }
  return e;
}

std::string sciezkaSesjiDoby(long dzien, const std::string& kursor) {
  std::string s = "/api/sessions?since=" + dzienAB(dzien) + "&limit=" + std::to_string(SESJI_NA_STRONE) + "&order=asc";
  if (!kursor.empty()) s += "&cursor=" + koduj(kursor, false);
  return s;
}

std::string sciezkaPobraniaPliku(const std::string& sciezka) {
  return "/api/storage/download?path=" + koduj(sciezka, true);
}

}  // namespace noc_pliki
