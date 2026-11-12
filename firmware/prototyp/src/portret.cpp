// SPDX-License-Identifier: GPL-3.0-or-later
#include "portret.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "airbridge.h"
#include "noc_pliki.h"
#include "str.h"
#include "wspolne.h"

namespace portret {

namespace {

const double EPS = 1e-9;
const char* const ZEWNETRZNE[] = {"Obstructive Apnea", "Apnea", "Hypopnea"};

bool zewnetrzne(const std::string& t) {
  for (const char* z : ZEWNETRZNE)
    if (t == z) return true;
  return false;
}
bool centralne(const std::string& t) { return t == "Central Apnea"; }

int zaokr(double x) { return (int)std::floor(x + 0.5); }

// Percentyl metodą najbliższej rangi (edf.percentyl w tools/kalibracja/edf.py); v posortowane rosnąco, niepuste.
double percentyl(const std::vector<double>& v, int procent) {
  size_t n = v.size(), p = (size_t)procent;
  size_t ranga = (n / 100) * p + ((n % 100) * p + 99) / 100;
  if (ranga < 1) ranga = 1;
  if (ranga > n) ranga = n;
  return v[ranga - 1];
}

int popcount5(uint8_t m) {
  int n = 0;
  for (int i = 0; i < 5; i++) n += (m >> i) & 1;
  return n;
}

std::string pole(int v) { return v == BRAK ? std::string() : std::to_string(v); }

std::string zlacz(const std::vector<std::string>& p) {
  std::string w;
  for (size_t i = 0; i < p.size(); i++) {
    if (i) w += ';';
    w += p[i];
  }
  return w;
}

std::vector<std::string> pola(const std::string& w) {
  std::vector<std::string> p;
  size_t a = 0;
  for (;;) {
    size_t b = w.find(';', a);
    p.push_back(w.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  return p;
}

// liczba całkowita z pola: false przy pustym tekście, znakach spoza cyfr albo wartości spoza [lo, hi]
bool liczba(const std::string& t, long lo, long hi, int& v) {
  if (t.empty() || t.size() > 7) return false;
  char* k = nullptr;
  long x = std::strtol(t.c_str(), &k, 10);
  if (!k || *k || x < lo || x > hi) return false;
  v = (int)x;
  return true;
}
bool liczbaLubBrak(const std::string& t, long lo, long hi, int& v) {
  if (t.empty()) {
    v = BRAK;
    return true;
  }
  return liczba(t, lo, hi, v);
}

}  // namespace

const Odcinek* Przebieg::odcinek(int k) const {
  auto it = std::lower_bound(odcinki.begin(), odcinki.end(), k * 5,
                             [](const Odcinek& o, int od) { return o.od < od; });
  return it != odcinki.end() && it->od == k * 5 ? &*it : nullptr;
}

Maksimum maksimumTrybu(const std::string& tryb, float mpa, float eax) {
  Maksimum m;
  m.potrzebne = tryb == "AutoSet" || tryb == "ASVAuto";
  if (tryb == "AutoSet" && !std::isnan(mpa)) {
    m.kanal = "Press.2s";
    m.prog = (double)mpa - MARGINES_MAKS;
  } else if (tryb == "ASVAuto" && !std::isnan(eax)) {
    m.kanal = "EprPress.2s";
    m.prog = (double)eax - MARGINES_MAKS;
  }
  return m;
}

Maksimum maksimumZeStr(const Str& st, long dzien, std::string* tryb) {
  auto it = st.doby.find(dzien);
  if (it == st.doby.end()) {
    if (tryb) tryb->clear();
    return Maksimum();
  }
  Noc n;
  bool maMop = false;
  std::string t = ustawieniaRaportu(n, ustawieniaStr(st, it->second), maMop);
  if (tryb) *tryb = t;
  return maksimumTrybu(t, n.mpa, n.eax);
}

Zbieracz::Zbieracz(long dzien, Maksimum m)
    : dzien_(dzien), t0_((long long)dzien * 86400LL + 12LL * 3600LL), maks_(m), a_((size_t)ODCINKI_DOBY) {
  leak_.v.reserve((size_t)ODCINEK_S / 2);
  ps_.v.reserve((size_t)ODCINEK_S / 2);
}

// Zamknięcie odcinka bufora: P95 wycieku (w dół do całości) albo mediana PS (0,1 cmH2O). Odcinek już zamknięty
// (plik wraca do wcześniejszego odcinka: pliki zachodzące w czasie) zostaje z pierwszą wartością.
void Zbieracz::zamknij(Bufor& b, bool leak) {
  if (b.k >= 0 && !b.v.empty()) {
    Akum& x = a_[(size_t)b.k];
    int16_t& cel = leak ? x.wyciek95 : x.ps;
    if (cel == BRAK) {
      std::sort(b.v.begin(), b.v.end());
      int v = leak ? (int)std::floor(percentyl(b.v, 95) + EPS) : zaokr(percentyl(b.v, 50) * 10);
      cel = (int16_t)std::max(-32767, std::min(32767, v));
    }
  }
  b.v.clear();
  b.k = -1;
}

void Zbieracz::dodaj(Bufor& b, int k, double v, bool leak) {
  if (k != b.k) {
    zamknij(b, leak);
    b.k = k;
  }
  if ((int)b.v.size() < BUFOR_PROBEK) b.v.push_back(v);
}

void Zbieracz::plikPld(const edf::Plik& p) {
  const double cr = p.czasRekordu;
  const double baza = (double)(p.startS - t0_);
  auto indeks = [](double s) { return (int)std::floor(s / ODCINEK_S); };
  auto bitMinuty = [](double s, int k) {
    int m = (int)std::floor(s / 60) - 5 * k;
    return (uint8_t)(m < 0 ? 1 : m > 4 ? 16 : 1 << m);
  };
  // ciśnienie: liczba poprawnych próbek i minuty z próbką (pierwszy z kanałów granic zapisu)
  const edf::Sygnal* sc = nullptr;
  for (const char* e : {"Press.2s", "EprPress.2s", "MaskPress.2s"}) {
    sc = p.sygnal(e);
    if (sc) break;
  }
  if (sc) {
    double krok = cr / sc->probki;
    for (long r = 0; r < p.rekordy; r++)
      for (int i = 0; i < sc->probki; i++) {
        if (!sc->poprawna(p.probka(*sc, r, i))) continue;
        double s = baza + (double)r * cr + (double)i * krok;
        int k = indeks(s);
        if (k < 0 || k >= ODCINKI_DOBY) continue;
        Akum& x = a_[(size_t)k];
        if (x.n < 0xFFFF) x.n++;
        x.minuty |= bitMinuty(s, k);
        uzyte_.set((size_t)k);
      }
  }
  // wyciek: P95 odcinka i sekundy ≥ 15 L/min
  if (const edf::Sygnal* sl = p.sygnal("Leak.2s")) {
    maLeak_ = true;
    double sk = sl->jednostka == "L/s" ? 60.0 : 1.0, krok = cr / sl->probki;
    for (long r = 0; r < p.rekordy; r++)
      for (int i = 0; i < sl->probki; i++) {
        long c = p.probka(*sl, r, i);
        if (!sl->poprawna(c)) continue;
        double s = baza + (double)r * cr + (double)i * krok;
        int k = indeks(s);
        if (k < 0 || k >= ODCINKI_DOBY) continue;
        double v = sl->fizyczna(c, sk);
        dodaj(leak_, k, v, true);
        if (v >= PROG_WYCIEKU - EPS) a_[(size_t)k].wycS += (float)krok;
        uzyte_.set((size_t)k);
      }
  }
  // wsparcie PS: pary próbek Press.2s i EprPress.2s z tych samych chwil
  const edf::Sygnal *sp = p.sygnal("Press.2s"), *se = p.sygnal("EprPress.2s");
  if (sp && se && sp->probki == se->probki) {
    double krok = cr / sp->probki;
    for (long r = 0; r < p.rekordy; r++)
      for (int i = 0; i < sp->probki; i++) {
        long a = p.probka(*sp, r, i), e = p.probka(*se, r, i);
        if (!sp->poprawna(a) || !se->poprawna(e)) continue;
        double s = baza + (double)r * cr + (double)i * krok;
        int k = indeks(s);
        if (k < 0 || k >= ODCINKI_DOBY) continue;
        dodaj(ps_, k, std::nearbyint((sp->fizyczna(a) - se->fizyczna(e)) * 1e6) / 1e6, false);
        maPs_ = true;
        uzyte_.set((size_t)k);
      }
  }
  // ciśnienie (AutoSet) albo EPAP (ASVAuto) na maksimum: minuty z próbką ≥ próg
  if (maks_.kanal)
    if (const edf::Sygnal* sm = p.sygnal(maks_.kanal)) {
      maMaks_ = true;
      double krok = cr / sm->probki;
      for (long r = 0; r < p.rekordy; r++)
        for (int i = 0; i < sm->probki; i++) {
          long c = p.probka(*sm, r, i);
          if (!sm->poprawna(c) || sm->fizyczna(c) < maks_.prog - EPS) continue;
          double s = baza + (double)r * cr + (double)i * krok;
          int k = indeks(s);
          if (k < 0 || k >= ODCINKI_DOBY) continue;
          a_[(size_t)k].maksMin |= bitMinuty(s, k);
          uzyte_.set((size_t)k);
        }
    }
}

size_t Zbieracz::bajty() const {
  return sizeof(*this) + a_.capacity() * sizeof(Akum) + (leak_.v.capacity() + ps_.v.capacity()) * sizeof(double);
}

Przebieg Zbieracz::wynik(const noc_pliki::WynikNocy& w,
                         const std::vector<std::pair<double, std::string>>& zdarzenia) {
  zamknij(leak_, true);
  zamknij(ps_, false);
  Przebieg p;
  p.data = dataPoranka(dzien_);
  std::vector<std::pair<double, double>> zapisy;
  for (auto& s : w.sesje)
    if (s.maZapis) {
      zapisy.push_back({s.zapisOd, s.zapisDo});
      Sesja x;
      x.od = std::max(0, std::min(SESJA_OD_MAKS, zaokr((s.zapisOd - (double)t0_) / 60.0)));
      x.do_ = std::max(x.od, std::min(SESJA_DO_MAKS, zaokr((s.zapisDo - (double)t0_) / 60.0)));
      p.sesje.push_back(x);
    }
  std::sort(p.sesje.begin(), p.sesje.end(),
            [](const Sesja& a, const Sesja& b) { return a.od != b.od ? a.od < b.od : a.do_ < b.do_; });
  const bool eveOk = w.zdarzeniaZnane;
  std::vector<int> zew((size_t)ODCINKI_DOBY, 0), centr((size_t)ODCINKI_DOBY, 0);
  std::bitset<288> uzyte = uzyte_;
  int bezdechy = 0;
  bool klasyfikacja = false;
  for (auto& z : zdarzenia) klasyfikacja = klasyfikacja || z.second == "Obstructive Apnea" || z.second == "Central Apnea";
  for (auto& z : zdarzenia) {
    bool ze = zewnetrzne(z.second), ce = centralne(z.second);
    if (!ze && !ce) continue;
    bool wZapisie = false;
    for (auto& a : zapisy) wZapisie = wZapisie || (a.first <= z.first && z.first < a.second);
    if (!wZapisie) continue;
    if (z.second != "Hypopnea") bezdechy++;
    double s = z.first - (double)t0_;
    int minuta = (int)std::floor(s / 60);
    // tylko zdarzenia w dobie (minuta 0…1439): poza nią ani wiersz `Z`, ani odcinek
    if (eveOk && minuta >= 0 && minuta <= MINUTA_ZDARZENIA_MAKS && (int)p.zdarzenia.size() < MAKS_ZDARZEN) {
      Zdarzenie e;
      e.minuta = minuta;
      e.zew = ze;
      e.centr = ce;
      p.zdarzenia.push_back(e);
    }
    int k = (int)std::floor(s / ODCINEK_S);
    if (eveOk && k >= 0 && k < ODCINKI_DOBY) {
      (ze ? zew : centr)[(size_t)k]++;
      uzyte.set((size_t)k);
    }
  }
  if (!maLeak_) p.braki += 'L';
  if (!maPs_) p.braki += 'P';
  if (!eveOk) p.braki += 'E';
  if (eveOk && bezdechy > 0 && !klasyfikacja) p.braki += 'R';
  if (maks_.potrzebne && (!maks_.kanal || !maMaks_)) p.braki += 'M';
  for (int k = 0; k < ODCINKI_DOBY; k++) {
    if (!uzyte.test((size_t)k)) continue;
    const Akum& x = a_[(size_t)k];
    bool liczy = x.n >= MIN_PROBEK;
    Odcinek o;
    o.od = k * 5;
    o.minuty = popcount5(x.minuty);
    if (liczy) {
      o.wyciek95 = x.wyciek95;
      o.ps = x.ps;
      if (eveOk) {
        o.zew = zew[(size_t)k];
        o.centr = centr[(size_t)k];
      }
      if (maks_.kanal) o.maks = popcount5(x.maksMin);
    }
    if (maLeak_) o.wycS = std::min(WYC_S_MAKS, zaokr((double)x.wycS));
    p.odcinki.push_back(o);
  }
  return p;
}

std::vector<std::string> wiersze(const Przebieg& p) {
  std::vector<std::string> w;
  w.reserve(p.sesje.size() + p.odcinki.size() + p.zdarzenia.size() + 1);
  const std::string& d = p.data;
  for (auto& s : p.sesje) w.push_back(zlacz({d, "S", std::to_string(s.od), std::to_string(s.do_), "", "", "", "", "", "", ""}));
  for (auto& o : p.odcinki)
    w.push_back(zlacz({d, "O", std::to_string(o.od), std::to_string(o.minuty), pole(o.wyciek95), pole(o.wycS),
                       pole(o.zew), pole(o.centr), pole(o.ps), pole(o.maks), ""}));
  for (auto& z : p.zdarzenia)
    w.push_back(zlacz({d, "Z", std::to_string(z.minuta), "", "", "", z.zew ? "1" : "0", z.centr ? "1" : "0", "", "", ""}));
  w.push_back(zlacz({d, "B", "", "", "", "", "", "", "", "", p.braki}));
  return w;
}

bool parsuj(const std::vector<std::string>& wiersze_, Przebieg& wynik, std::string& blad) {
  Przebieg p;
  bool b = false;
  auto zle = [&](const std::string& co) {
    blad = co;
    return false;
  };
  for (auto& w : wiersze_) {
    std::vector<std::string> f = pola(w);
    if (f.size() != 11) return zle("zła liczba pól");
    if (!dataOk(f[0])) return zle("zła data");
    if (p.data.empty()) p.data = f[0];
    else if (p.data != f[0]) return zle("różne daty");
    const std::string& r = f[1];
    if (r == "S") {
      Sesja s;
      if (!liczba(f[2], 0, SESJA_OD_MAKS, s.od) || !liczba(f[3], 0, SESJA_DO_MAKS, s.do_) || s.do_ < s.od) return zle("zła sesja");
      p.sesje.push_back(s);
    } else if (r == "O") {
      Odcinek o;
      if (!liczba(f[2], 0, 1435, o.od) || o.od % 5 || !liczba(f[3], 0, 5, o.minuty)) return zle("zły odcinek");
      if (!p.odcinki.empty() && p.odcinki.back().od >= o.od) return zle("odcinki nie rosną");
      if (!liczbaLubBrak(f[4], 0, 1000, o.wyciek95) || !liczbaLubBrak(f[5], 0, WYC_S_MAKS, o.wycS) ||
          !liczbaLubBrak(f[6], 0, 10000, o.zew) || !liczbaLubBrak(f[7], 0, 10000, o.centr) ||
          !liczbaLubBrak(f[8], -500, 1000, o.ps) || !liczbaLubBrak(f[9], 0, 5, o.maks))
        return zle("zła wartość odcinka");
      p.odcinki.push_back(o);
    } else if (r == "Z") {
      Zdarzenie z;
      int a = 0, c = 0;
      if (!liczba(f[2], 0, MINUTA_ZDARZENIA_MAKS, z.minuta) || !liczba(f[6], 0, 1, a) || !liczba(f[7], 0, 1, c) || a + c != 1)
        return zle("złe zdarzenie");
      z.zew = a == 1;
      z.centr = c == 1;
      if ((int)p.zdarzenia.size() >= MAKS_ZDARZEN) return zle("za dużo zdarzeń");
      p.zdarzenia.push_back(z);
    } else if (r == "B") {
      if (b) return zle("drugi wiersz B");
      for (char c : f[10])
        if (!std::strchr("LPERM", c)) return zle("nieznany brak");
      p.braki = f[10];
      b = true;
    } else {
      return zle("nieznany rodzaj wiersza");
    }
  }
  if (!b) return zle("brak wiersza B");
  wynik = p;
  return true;
}

}  // namespace portret
