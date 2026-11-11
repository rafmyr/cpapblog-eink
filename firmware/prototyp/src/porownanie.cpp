// SPDX-License-Identifier: GPL-3.0-or-later
#include "porownanie.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

#include "wspolne.h"

namespace porownanie {

const char* opisOceny(Ocena o) {
  switch (o) {
    case Ocena::TAK: return "tak";
    case Ocena::NIE: return "NIE";
    case Ocena::BRAK_W_PLIKACH: return "brak w plikach";
    case Ocena::BRAK_W_RAPORCIE: return "brak w raporcie";
    case Ocena::POMINIETE_APARAT: return "pominięte: aparat nie rozróżnia";
    case Ocena::TYLKO_PLIKI: return "tylko pliki";
    case Ocena::BEZ_STR: return "brak (bez STR.edf)";
  }
  return "?";
}
bool rozjazd(Ocena o) { return o == Ocena::NIE || o == Ocena::BRAK_W_PLIKACH; }
bool pominieta(Ocena o) { return o == Ocena::POMINIETE_APARAT || o == Ocena::TYLKO_PLIKI || o == Ocena::BEZ_STR; }

std::string liczba(double v, int miejsca) {
  if (std::isnan(v)) return "–";
  char b[32];
  snprintf(b, sizeof b, "%.*f", miejsca, v);
  return b;
}

namespace {
double f(float v) { return (double)v; }
double calk(int v) { return v < 0 ? NAN : (double)v; }

Ocena ocenaLiczby(const std::string& id, double p, double r, double tol) {
  const bool bp = std::isnan(p), br = std::isnan(r);
  if (bp && br) return Ocena::TAK;
  // OA i CA: EVE bez `Obstructive Apnea` i `Central Apnea` (ResMed ASV zapisuje tylko `Apnea`) daje w plikach
  // „nieznane”, a raport AirBridge 0; to nie rozjazd liczenia, tylko brak rozróżnienia w aparacie
  if (bp && (id == "m03oa" || id == "m03ca") && r == 0) return Ocena::POMINIETE_APARAT;
  if (br && (id == "m28" || id == "m28p")) return Ocena::TYLKO_PLIKI;  // raport JSON nie ma PS
  if (bp) return Ocena::BRAK_W_PLIKACH;
  if (br) return Ocena::BRAK_W_RAPORCIE;
  // margines względny: wartości przychodzą z float (np. 7.8f − 7.5f = 0.3000002), więc granica tolerancji ma
  // dawać „tak”, a nie rozjazd z powodu zapisu liczby
  const double eps = 1e-6 * std::max({1.0, std::fabs(p), std::fabs(r)});
  return std::fabs(p - r) <= tol + eps ? Ocena::TAK : Ocena::NIE;
}
const char* takNie(bool b) { return b ? "tak" : "nie"; }
}  // namespace

Wynik porownaj(const NocAB& panel, const NocAB& raport, bool zUstawieniami) {
  Wynik w;
  const Noc& p = panel.noc;
  const Noc& r = raport.noc;
  auto tekst = [&](const char* id, const char* opis, const std::string& tp, const std::string& tr) {
    Wiersz x;
    x.id = id;
    x.opis = opis;
    x.tekst = true;
    x.tPanel = tp;
    x.tRaport = tr;
    x.ocena = tp == tr ? Ocena::TAK : Ocena::NIE;
    w.wiersze.push_back(x);
  };
  auto num = [&](const char* id, const char* opis, double vp, double vr, double tol) {
    Wiersz x;
    x.id = id;
    x.opis = opis;
    x.panel = vp;
    x.raport = vr;
    x.tolerancja = tol;
    x.ocena = ocenaLiczby(id, vp, vr, tol);
    w.wiersze.push_back(x);
  };
  tekst("czesciowa", "częściowa", takNie(p.czesciowa), takNie(r.czesciowa));
  tekst("pelna", "pełna", takNie(panel.pelna), takNie(raport.pelna));
  tekst("wazna", "ważna", takNie(p.wazna), takNie(r.wazna));
  num("sesje", "liczba sesji", calk(panel.sesje), calk(raport.sesje), 0);
  num("m01", "M-01 min", calk(p.m01), calk(r.m01), 1);
  num("m02", "M-02 AHI", f(p.ahi), f(r.ahi), 0.5);
  num("m03oa", "M-03 OA/h", f(p.oa), f(r.oa), 0.5);
  num("m03ca", "M-03 CA/h", f(p.ca), f(r.ca), 0.5);
  num("m03h", "M-03 H/h", f(p.h), f(r.h), 0.5);
  num("m03ua", "M-03 UA/h", f(p.ua), f(r.ua), 0.5);
  num("m05", "M-05 IPAP mediana", f(p.cmed), f(r.cmed), 0.5);
  num("m05p", "M-05p IPAP P95", f(p.cp95), f(r.cp95), 0.5);
  num("m06", "M-06 EPAP mediana", f(p.emed), f(r.emed), 0.5);
  num("m06p", "M-06p EPAP P95", f(p.ep95), f(r.ep95), 0.5);
  num("m07", "M-07 wyciek mediana", f(p.wmed), f(r.wmed), 2);
  num("m07p", "M-07 wyciek P95", f(p.wp95), f(r.wp95), 2);
  num("m08", "M-08 min", f(p.m08), f(r.m08), 2);
  num("m09", "M-09 min", f(p.csr), f(r.csr), 2);
  num("m10", "M-10 mediana", f(p.m10), f(r.m10), 0.02);
  num("m10p", "M-10 P95", f(p.m10p), f(r.m10p), 0.02);
  num("m13", "M-13", f(p.m13), f(r.m13), 0.5);
  num("m14", "M-14 objętość mediana", f(p.m14), f(r.m14), 20);  // WY9.tol.M-14
  num("m15", "M-15 wentylacja mediana", f(p.m15), f(r.m15), 0.3);  // WY9.tol.M-15
  num("m17", "M-17 (R-DANE-27)", calk(p.m17), calk(r.m17), 0);
  num("m18", "M-18 min od 12:00 (R-DANE-27)", calk(p.m18), calk(r.m18), 5);
  if (zUstawieniami) {
    tekst("m21", "M-21 ustawienia", p.ustawienia, r.ustawienia);
  } else {
    Wiersz x;
    x.id = "m21";
    x.opis = "M-21 ustawienia";
    x.tekst = true;
    x.tRaport = r.ustawienia;
    x.ocena = Ocena::BEZ_STR;
    w.wiersze.push_back(x);
  }
  num("m28", "M-28 PS mediana (raport nie ma)", f(p.psmed), f(r.psmed), 0.5);
  num("m28p", "M-28p PS P95 (raport nie ma)", f(p.psp95), f(r.psp95), 0.5);
  for (auto& x : w.wiersze) w.rozjazdy += rozjazd(x.ocena);
  return w;
}

// ---------- wpisy ----------
const char* nazwaWerdyktu(Werdykt w) {
  switch (w) {
    case Werdykt::ZGODNA: return "zgodna";
    case Werdykt::ROZJAZD: return "rozjazd";
    case Werdykt::BRAK: return "brak";
  }
  return "?";
}

const char* opisPrzyczyny(Przyczyna p) {
  switch (p) {
    case Przyczyna::BRAK: return "";
    case Przyczyna::STARY_AIRBRIDGE: return "AirBridge < 2.3.1";
    case Przyczyna::BLAD_LISTY: return "błąd listy sesji";
    case Przyczyna::BEZ_SESJI: return "brak plików";
    case Przyczyna::ZA_DUZO_SESJI: return "za dużo sesji";
    case Przyczyna::NIEZAMKNIETA: return "sesja niezamknięta";
    case Przyczyna::BEZ_STR: return "brak STR.edf";
    case Przyczyna::STR_BEZ_DOBY: return "STR bez tej doby";
    case Przyczyna::PLIK_NIEPOBRANY: return "plik niepobrany";
    case Przyczyna::PLIK_NIECZYTELNY: return "plik nieczytelny";
    case Przyczyna::CZESCIOWA: return "noc z plików częściowa";
    case Przyczyna::BRAK_CZASU: return "brak czasu";
    case Przyczyna::CZEKA: return "czeka";
  }
  return "?";
}

namespace {
// wartość do pola wpisu: bez separatorów pliku i wpisu
std::string czyste(const std::string& t) {
  std::string r = t;
  for (auto& c : r)
    if (c == ';' || c == '|' || c == '~' || c == '=' || c == ':' || c == '\n' || c == '\r') c = '_';
  return r;
}
std::string czysteTekst(const std::string& t) {
  std::string r;
  for (char c : t)
    if (c != ';' && c != '\n' && c != '\r') r += c;
  return r;
}
std::vector<std::string> dziel(const std::string& s, char z) {
  std::vector<std::string> v;
  size_t a = 0;
  for (;;) {
    size_t b = s.find(z, a);
    v.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  return v;
}
}  // namespace

Wpis wpisZWyniku(const std::string& data, const Wynik& w) {
  Wpis e;
  e.data = data;
  e.werdykt = w.rozjazdy ? Werdykt::ROZJAZD : Werdykt::ZGODNA;
  for (auto& x : w.wiersze) {
    if (rozjazd(x.ocena)) {
      std::string tp = x.tekst ? x.tPanel : liczba(x.panel), tr = x.tekst ? x.tRaport : liczba(x.raport);
      e.rozjazd += (e.rozjazd.empty() ? "" : "|") + x.id + "=" + czyste(tp) + "~" + czyste(tr);
    } else if (pominieta(x.ocena)) {
      const char* k = x.ocena == Ocena::POMINIETE_APARAT ? "aparat" : x.ocena == Ocena::TYLKO_PLIKI ? "pliki" : "bez_str";
      e.pominiete += (e.pominiete.empty() ? "" : "|") + x.id + ":" + k;
    }
  }
  return e;
}

bool czekaPonowienia(const std::string& p) {
  return p == opisPrzyczyny(Przyczyna::CZEKA) || p == opisPrzyczyny(Przyczyna::BRAK_CZASU) ||
         p == opisPrzyczyny(Przyczyna::NIEZAMKNIETA);
}

Wpis wpisBraku(const std::string& data, Przyczyna p) {
  Wpis e;
  e.data = data;
  e.werdykt = Werdykt::BRAK;
  e.przyczyna = opisPrzyczyny(p);
  return e;
}

std::string wierszWpisu(const Wpis& w) {
  return w.data + ";" + nazwaWerdyktu(w.werdykt) + ";" + czysteTekst(w.rozjazd) + ";" + czysteTekst(w.pominiete) + ";" +
         czysteTekst(w.przyczyna);
}

bool parsujWpis(const std::string& wiersz, Wpis& w) {
  std::vector<std::string> p = dziel(wiersz, ';');
  if (p.size() != 5 || nrDnia(p[0]) < 0 || p[0].size() != 10) return false;
  Wpis e;
  e.data = p[0];
  if (p[1] == "zgodna") e.werdykt = Werdykt::ZGODNA;
  else if (p[1] == "rozjazd") e.werdykt = Werdykt::ROZJAZD;
  else if (p[1] == "brak") e.werdykt = Werdykt::BRAK;
  else return false;
  e.rozjazd = p[2];
  e.pominiete = p[3];
  e.przyczyna = p[4];
  w = e;
  return true;
}

static void porzadkuj(std::vector<Wpis>& v) {
  std::stable_sort(v.begin(), v.end(), [](const Wpis& a, const Wpis& b) { return a.data < b.data; });
  if (v.size() > (size_t)MAKS_WPISOW) v.erase(v.begin(), v.end() - MAKS_WPISOW);
}

std::vector<Wpis> parsujWpisy(const std::vector<std::string>& wiersze) {
  std::vector<Wpis> v;
  for (auto& x : wiersze) {
    Wpis e;
    if (!parsujWpis(x, e)) continue;
    bool zast = false;
    for (auto& y : v)
      if (y.data == e.data) {
        y = e;  // późniejszy wiersz tej daty wygrywa
        zast = true;
      }
    if (!zast) v.push_back(e);
  }
  porzadkuj(v);
  return v;
}

void dopisz(std::vector<Wpis>& wpisy, const Wpis& w) {
  for (auto& x : wpisy)
    if (x.data == w.data) {
      x = w;
      porzadkuj(wpisy);
      return;
    }
  wpisy.push_back(w);
  porzadkuj(wpisy);
}

bool gotowe(const std::vector<Wpis>& wpisy, const std::string& data) {
  for (auto& x : wpisy)
    if (x.data == data && x.werdykt != Werdykt::BRAK) return true;
  return false;
}

// ---------- U-05 ----------
Licznik policz(const std::vector<Wpis>& wpisy) {
  Licznik l;
  l.wpisy = !wpisy.empty();
  std::vector<Wpis> v = wpisy;
  porzadkuj(v);
  if (!v.empty() && v.back().werdykt == Werdykt::BRAK) l.przyczyna = v.back().przyczyna;
  for (auto it = v.rbegin(); it != v.rend() && l.porownane < DOB_DO_PRZELACZENIA; ++it) {
    if (it->werdykt == Werdykt::BRAK) continue;
    l.porownane++;
    if (it->werdykt == Werdykt::ZGODNA) {
      l.zgodne++;
      continue;
    }
    for (auto& r : dziel(it->rozjazd, '|')) {
      std::string id = r.substr(0, r.find('='));
      // m05p → M-05, m03oa → M-03; flagi (wazna, sesje, …) bez zmian
      std::string m = id.size() >= 3 && id[0] == 'm' && std::isdigit((unsigned char)id[1]) ? "M-" + id.substr(1, 2) : id;
      if (!m.empty() && std::find(l.metryki.begin(), l.metryki.end(), m) == l.metryki.end()) l.metryki.push_back(m);
    }
  }
  return l;
}

std::vector<std::string> wartosciU05(const Licznik& l, Zrodlo z) {
  if (z == Zrodlo::JSON) return {"wyłączone (json)", "wyłączone"};
  if (z == Zrodlo::PLIK) return {"włączone (plik)", "włączone"};
  if (l.porownane == 0) {
    if (!l.wpisy) return {"jeszcze bez porównania", "bez porównania"};
    std::string p = l.przyczyna.empty() ? std::string("–") : l.przyczyna;
    return {"brak porównania: " + p, "brak: " + p, "brak porównania"};
  }
  std::string n = std::to_string(l.porownane), k = std::to_string(l.zgodne);
  if (l.zgodne == l.porownane) {
    std::string z7 = "zgodne " + k + " z " + n;
    if (l.moznaPrzelaczyc())
      return {z7 + ", można przełączyć na pliki", z7 + ", można przełączyć", k + "/" + n + " zgodne, można przełączyć",
              k + "/" + n + ", można na pliki"};
    return {z7};
  }
  std::string r = "rozjazd " + std::to_string(l.porownane - l.zgodne) + " z " + n;
  std::string m;
  for (auto& x : l.metryki) m += (m.empty() ? "" : ", ") + x;
  std::vector<std::string> w;
  if (!m.empty()) w.push_back(r + " (" + m + ")");
  if (!l.metryki.empty()) w.push_back(r + " (" + l.metryki.front() + (l.metryki.size() > 1 ? "…)" : ")"));
  w.push_back(r);
  return w;
}

std::string liniaDiagnostyki(const std::vector<Wpis>& wpisy, Zrodlo z) {
  const char* tryb = z == Zrodlo::JSON ? "json" : z == Zrodlo::PLIK ? "plik" : "porownanie";
  Licznik l = policz(wpisy);
  std::string t = std::string("Noce z plików: noc_zrodlo=") + tryb + " · wpisów " + std::to_string(wpisy.size()) +
                  " · ostatnie z porównaniem: " + std::to_string(l.porownane) + ", zgodne " + std::to_string(l.zgodne);
  for (auto it = wpisy.rbegin(); it != wpisy.rend(); ++it)
    if (it->werdykt == Werdykt::ROZJAZD) {
      t += " · ostatni rozjazd " + it->data + ": " + it->rozjazd;
      break;
    }
  for (auto it = wpisy.rbegin(); it != wpisy.rend(); ++it)
    if (it->werdykt == Werdykt::BRAK) {
      t += " · ostatni brak " + it->data + ": " + it->przyczyna;
      break;
    }
  if (l.moznaPrzelaczyc()) t += " · można przełączyć na pliki (noc_zrodlo=plik)";
  return t;
}

}  // namespace porownanie
