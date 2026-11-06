// SPDX-License-Identifier: GPL-3.0-or-later
#include "noc.h"
#include "wspolne.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>

static bool liczbaOk(const std::string& v, float& out) {  // puste = brak danych (NAN); śmieci = błąd wiersza
  if (v.empty() || v == "\r") { out = NAN; return true; }
  char* e;
  out = strtof(v.c_str(), &e);
  if (std::isinf(out)) return false;  // `inf` i liczby ponad zakres float; tekst `nan` = brak danych
  return e != v.c_str() && (*e == 0 || *e == '\r');
}

bool parsujNoc(const std::string& l, char domyslneZrodlo, Noc& r) {
  std::string p[21];
  int n = 0;
  size_t start = 0;
  for (size_t i = 0; i <= l.size() && n < 21; i++)
    if (i == l.size() || l[i] == ';') { p[n++] = l.substr(start, i - start); start = i + 1; }
  if (n != 8 && n != 9 && n != 12 && n != 13 && n != 14 && n != 20) return false;
  for (int i = 0; i < n; i++)
    if (!p[i].empty() && p[i].back() == '\r') p[i].pop_back();
  if (!dataOk(p[0]) || (p[1] != "0" && p[1] != "1")) return false;
  Noc w;
  w.data = p[0];
  w.wazna = p[1] == "1";
  float m01;
  if (!liczbaOk(p[2], m01) || !liczbaOk(p[3], w.ahi) || !liczbaOk(p[4], w.cmed) || !liczbaOk(p[5], w.cp95) ||
      !liczbaOk(p[6], w.wmed) || !liczbaOk(p[7], w.wp95))
    return false;
  if (n >= 12 && (!liczbaOk(p[8], w.emed) || !liczbaOk(p[9], w.ep95) || !liczbaOk(p[10], w.psmed) ||
                  !liczbaOk(p[11], w.psp95)))
    return false;
  // minuty terapii doby: -1 (brak, tak zapisuje zapiszNoc()) albo 0–1440; poza tym błąd wiersza, zanim rzutowanie
  // na int wyjdzie poza zakres typu
  if (!std::isnan(m01) && (m01 < -1 || m01 > 1440)) return false;
  w.m01 = std::isnan(m01) ? -1 : (int)m01;
  if (n >= 13) w.ustawienia = p[12];
  std::string z = n == 9 ? p[8] : n >= 14 ? p[13] : std::string();
  if (!z.empty() && z != "A" && z != "K") return false;
  w.zrodlo = z.empty() ? domyslneZrodlo : z[0];
  if (n == 20) {
    auto flaga = [](const std::string& v, bool& f) {
      if (v.empty() || v == "0") { f = false; return true; }
      if (v == "1") { f = true; return true; }
      return false;
    };
    if (!flaga(p[14], w.czesciowa) || !flaga(p[15], w.epapStr) || !liczbaOk(p[16], w.oa) || !liczbaOk(p[17], w.ca) ||
        !liczbaOk(p[18], w.h) || !liczbaOk(p[19], w.csr))
      return false;
  }
  r = w;
  return true;
}

static std::string pole(float v) {
  if (std::isnan(v)) return "";
  char b[24];
  snprintf(b, sizeof b, "%.2f", v);
  return b;
}

std::string zapiszNoc(const Noc& r) {
  return r.data + ";" + (r.wazna ? "1" : "0") + ";" + std::to_string(r.m01) + ";" + pole(r.ahi) + ";" + pole(r.cmed) +
         ";" + pole(r.cp95) + ";" + pole(r.wmed) + ";" + pole(r.wp95) + ";" + pole(r.emed) + ";" + pole(r.ep95) + ";" +
         pole(r.psmed) + ";" + pole(r.psp95) + ";" + czystePole(r.ustawienia) + ";" + std::string(1, r.zrodlo) + ";" +
         (r.czesciowa ? "1" : "0") + ";" + (r.epapStr ? "1" : "0") + ";" + pole(r.oa) + ";" + pole(r.ca) + ";" +
         pole(r.h) + ";" + pole(r.csr);
}

std::string wierszNocyV1(const Noc& r) { return zapiszNoc(r) + ";" + (r.import ? std::to_string(r.import) : ""); }

bool parsujNocV1(const std::string& w, Noc& r) {
  size_t i = 0;
  for (int k = 0; k < 20; k++) {  // pozycja 20. średnika: koniec pól wersji 0
    i = w.find(';', i);
    if (i == std::string::npos) return false;
    if (k < 19) i++;
  }
  size_t j = w.find(';', i + 1);
  std::string imp = w.substr(i + 1, j == std::string::npos ? std::string::npos : j - i - 1);
  uint32_t ep = 0;  // nieczytelne `import` = nieznane (rekord nocy zostaje, jak puste pole)
  bool cyfry = !imp.empty() && imp.size() <= 10;
  for (char c : imp) cyfry = cyfry && c >= '0' && c <= '9';
  if (cyfry) {
    unsigned long long v = strtoull(imp.c_str(), nullptr, 10);
    if (v <= 4294967295ULL) ep = (uint32_t)v;
  }
  Noc n;
  if (!parsujNoc(w.substr(0, i), 'A', n)) return false;
  n.import = ep;
  r = n;
  return true;
}

// pola `m13;m17;pna;srn;m08` (po polu `import` w wersji 2, po polu `csr` w trescNocy)
// Pola liczbowe wersji 2 po `srn`, w kolejności pliku: M-08, ustawienia, UA/h, M-10 (mediana, P95).
static float Noc::* const LICZBY_B1[] = {&Noc::m08, &Noc::mpa, &Noc::mpi, &Noc::ipc, &Noc::eep, &Noc::mns, &Noc::mxs,
                                         &Noc::ua,  &Noc::m10, &Noc::m10p, &Noc::eai, &Noc::eax, &Noc::ans, &Noc::axs,
                                         &Noc::epr};
static const int N_LICZB_B1 = (int)(sizeof(LICZBY_B1) / sizeof(LICZBY_B1[0]));
static std::string polaB1(const Noc& r) {
  std::string w = pole(r.m13) + ";" + (r.m17 >= 0 ? std::to_string(r.m17) : std::string()) + ";" + czystePole(r.pna) +
                  ";" + czystePole(r.srn);
  for (auto p : LICZBY_B1) w += ";" + pole(r.*p);
  return w + ";" + czystePole(r.tryb);
}

std::string wierszNocyV2(const Noc& r) { return wierszNocyV1(r) + ";" + polaB1(r); }

bool trybObslugiwany(const std::string& t) { return t == "CPAP" || t == "AutoSet" || t == "ASV" || t == "ASVAuto"; }

std::string trybNocy(const Noc& n) {
  if (!n.tryb.empty()) return n.tryb;
  size_t i = n.ustawienia.find('/');
  return n.ustawienia.substr(0, i);
}

std::string trescNocy(const Noc& r) {
  return zapiszNoc(r) + ";" + polaB1(r) + ";" + (r.m18 >= 0 ? std::to_string(r.m18) : std::string());
}

std::string wierszNocyV3(const Noc& r) {
  return wierszNocyV2(r) + ";" + (r.m18 >= 0 ? std::to_string(r.m18) : std::string());
}

bool parsujNocV3(const std::string& w, Noc& r) {
  Noc n;
  if (!parsujNocV2(w, n)) return false;
  size_t i = 0;  // pole 42 (po 41. średniku); brak albo nieczytelne = brak (-1)
  for (int k = 0; k < 41 && i != std::string::npos; k++) {
    i = w.find(';', i);
    if (i != std::string::npos) i++;
  }
  if (i != std::string::npos) {
    size_t j = w.find(';', i);
    std::string p = w.substr(i, j == std::string::npos ? std::string::npos : j - i);
    if (!p.empty() && p.back() == '\r') p.pop_back();
    bool cyfry = !p.empty() && p.size() <= 4;
    for (char c : p) cyfry = cyfry && c >= '0' && c <= '9';
    if (cyfry) {
      int v = (int)strtol(p.c_str(), nullptr, 10);
      if (v < 24 * 60) n.m18 = v;
    }
  }
  r = n;
  return true;
}

int m18ZChwili(long long epoch, const std::string& dataPoranka) {
  if (epoch <= 0 || !dataOk(dataPoranka)) return -1;
  time_t t = (time_t)epoch;
  std::tm l{};
  if (!localtime_r(&t, &l)) return -1;
  char b[40];
  snprintf(b, sizeof b, "%04d-%02d-%02d", l.tm_year + 1900, l.tm_mon + 1, l.tm_mday);
  int m = l.tm_hour * 60 + l.tm_min;
  if (dodajDni(dataPoranka, -1) == b && m >= 12 * 60) return m - 12 * 60;  // wieczór, od 12:00
  if (dataPoranka == b && m < 12 * 60) return m + 12 * 60;                   // po północy, do 12:00
  return -1;
}

bool parsujNocV2(const std::string& w, Noc& r) {
  Noc n;
  if (!parsujNocV1(w, n)) return false;
  // pola 22–41 (indeksy 21–40); brakujące = brak danych, dalsze (nowsza wersja) pominięte
  std::string p[5 + N_LICZB_B1];
  size_t i = 0;
  for (int k = 0; k < 21 && i != std::string::npos; k++) {
    i = w.find(';', i);
    if (i != std::string::npos) i++;
  }
  for (int k = 0; k < 5 + N_LICZB_B1 && i != std::string::npos; k++) {
    size_t j = w.find(';', i);
    p[k] = w.substr(i, j == std::string::npos ? std::string::npos : j - i);
    if (!p[k].empty() && p[k].back() == '\r') p[k].pop_back();
    i = j == std::string::npos ? j : j + 1;
  }
  float m13;
  if (liczbaOk(p[0], m13) && (std::isnan(m13) || std::isfinite(m13))) n.m13 = m13;  // nieczytelne = brak (NAN)
  bool cyfry = !p[1].empty() && p[1].size() <= 6;
  for (char c : p[1]) cyfry = cyfry && c >= '0' && c <= '9';
  if (cyfry) n.m17 = (int)strtol(p[1].c_str(), nullptr, 10);  // nieczytelne albo ujemne = brak (-1)
  n.pna = czystePole(p[2]);
  n.srn = czystePole(p[3]);
  for (int k = 0; k < N_LICZB_B1; k++) {  // nieczytelne, nieskończone albo ujemne = brak (NAN)
    float v;
    if (liczbaOk(p[4 + k], v) && std::isfinite(v) && v >= 0) n.*LICZBY_B1[k] = v;
  }
  n.tryb = czystePole(p[4 + N_LICZB_B1]);
  r = n;
  return true;
}

float epapMed(const Noc& n) { return n.epapStr ? NAN : n.emed; }
float epapP95(const Noc& n) { return n.epapStr ? NAN : n.ep95; }

std::string czystePole(const std::string& t) {
  std::string w;
  for (unsigned char ch : t)
    if (ch != ';' && ch >= 0x20 && ch != 0x7F) w += (char)ch;
  return w;
}

bool uzupelnijNoc(Noc& c, const Noc& z, PolaUzupelnienia zakres) {
  bool zm = false;
  auto f = [&](float Noc::*pole) {
    if (std::isnan(c.*pole) && !std::isnan(z.*pole)) { c.*pole = z.*pole; zm = true; }
  };
  if (zakres == PolaUzupelnienia::TYLKO_SPOZA_AIRBRIDGE) {
    f(&Noc::psmed); f(&Noc::psp95);
    return zm;
  }
  f(&Noc::cmed); f(&Noc::cp95); f(&Noc::wmed); f(&Noc::wp95);
  // EPAP: wartość z EDF wypiera EPAP docelowe urządzenia (STR), odwrotnie nie
  if (!std::isnan(z.emed) && (std::isnan(c.emed) || (c.epapStr && !z.epapStr))) {
    c.emed = z.emed; c.ep95 = z.ep95; c.epapStr = z.epapStr; zm = true;
  }
  f(&Noc::psmed); f(&Noc::psp95); f(&Noc::oa); f(&Noc::ca); f(&Noc::h); f(&Noc::csr);
  if (c.ustawienia.empty() && !z.ustawienia.empty()) { c.ustawienia = z.ustawienia; zm = true; }
  // M-13, M-17, model i numer aparatu, M-08, ustawienia, UA/h, M-10 (pola z AirBridge; noc z karty ich nie ma)
  f(&Noc::m13);
  for (auto p : LICZBY_B1) f(p);
  if (c.m17 < 0 && z.m17 >= 0) { c.m17 = z.m17; zm = true; }
  if (c.m18 < 0 && z.m18 >= 0) { c.m18 = z.m18; zm = true; }  // M-18
  if (c.pna.empty() && !z.pna.empty()) { c.pna = z.pna; zm = true; }
  if (c.srn.empty() && !z.srn.empty()) { c.srn = z.srn; zm = true; }
  if (c.tryb.empty() && !z.tryb.empty()) { c.tryb = z.tryb; zm = true; }
  return zm;
}

const Noc* nocZDnia(const std::vector<Noc>& hist, const std::string& data) {
  for (auto& r : hist)
    if (r.data == data) return &r;
  return nullptr;
}

int nocyBezDanychZRzedu(const std::string& dzis, const std::string& dataOdczytu, int minutaOdczytu,
                        const std::string& dataBledu, const std::vector<Noc>* hist) {
  if (!dataOk(dzis)) return 1;
  long n = -1;  // -1 = nieznane
  if (!dataOdczytu.empty()) {
    if (!dataOk(dataOdczytu)) return 1;
    if (dataOdczytu > dzis) return 0;
    n = nocyOdOdczytu(dzis, dataOdczytu, minutaOdczytu < 0 ? 5 * 60 : minutaOdczytu);
  } else if (!dataBledu.empty()) {
    if (!dataOk(dataBledu)) return 1;
    n = std::max(0L, (long)dniMiedzy(dataBledu, dzis) + 1);
  }
  if (hist) {
    std::string najstarsza;
    for (auto& r : *hist)
      if (r.data < dzis && (najstarsza.empty() || r.data < najstarsza)) najstarsza = r.data;
    if (!najstarsza.empty()) {  // luka w historii: od wczoraj wstecz do pierwszej nocy w historii
      long luka = 1;
      while (luka < 1000) {
        std::string dzien = dodajDni(dzis, -(int)luka);
        if (dzien < najstarsza || nocZDnia(*hist, dzien)) break;
        luka++;
      }
      n = n < 0 ? luka : std::min(n, luka);
    }
  }
  if (n < 0) return 1;
  return (int)std::min(n, 1000L);
}

const Noc* ostatniaNoc(const std::vector<Noc>& hist, const std::string& najpozniej) {
  const Noc* w = nullptr;
  for (auto& r : hist)
    if (r.data <= najpozniej && (!w || r.data > w->data)) w = &r;
  return w;
}

double mediana(std::vector<double> v) {
  if (v.empty()) return NAN;
  std::sort(v.begin(), v.end());
  size_t k = v.size() / 2;
  return v.size() % 2 ? v[k] : (v[k - 1] + v[k]) / 2;
}

double percentyl(std::vector<double> v, int proc) {
  if (v.empty()) return NAN;
  std::sort(v.begin(), v.end());
  long n = (long)v.size();
  long ranga = (n / 100) * proc + ((n % 100) * proc + 99) / 100;
  ranga = std::max(1L, std::min(n, ranga));
  return v[ranga - 1];
}

static NormaMetryki zbierz(const std::vector<double>& v) {
  NormaMetryki m;
  m.n = (int)v.size();
  if (m.n < 14) return m;
  m.med = mediana(v);
  m.p10 = percentyl(v, 10);
  m.p90 = percentyl(v, 90);
  return m;
}

HistoriaPos posortujHistorie(const std::vector<Noc>& hist) {
  static const std::string pusty;
  HistoriaPos h;
  h.noce.reserve(hist.size());
  for (auto& r : hist) h.noce.push_back(&r);
  std::stable_sort(h.noce.begin(), h.noce.end(), [](const Noc* a, const Noc* b) { return a->data < b->data; });
  // R-NOR-07: ustawienia każdej nocy z przeniesieniem ostatnich znanych na noce z pustym polem
  h.ust.resize(h.noce.size());
  const std::string* znane = &pusty;
  for (size_t i = 0; i < h.noce.size(); i++) {
    if (!h.noce[i]->ustawienia.empty()) znane = &h.noce[i]->ustawienia;
    h.ust[i] = znane;
  }
  return h;
}

Norma liczNorme(const HistoriaPos& hp, const std::string& dzis, const std::set<std::string>& wykluczone,
                 const WykluczeniaNormy* wm) {
  Norma nm;
  if (!dataOk(dzis)) return nm;
  nm.zegar = true;
  std::string granica = dodajDni(dzis, -90);
  auto mniej = [](const Noc* a, const std::string& d) { return a->data < d; };
  // najnowsza noc ≤ dziś (jej ustawienia filtrują normy ciśnienia i wycieku) i pierwsza noc ≥ dziś
  size_t doDzis = std::lower_bound(hp.noce.begin(), hp.noce.end(), dzis, mniej) - hp.noce.begin();
  size_t poDzis = doDzis;
  while (poDzis < hp.noce.size() && hp.noce[poDzis]->data == dzis) poDzis++;
  if (poDzis > 0) nm.ustawienia = *hp.ust[poDzis - 1];
  std::vector<double> m01, ahi, cmed, cp95, wp95, emed, ep95, psmed, psp95, oa, ca, hh, m17, m13, csrp;
  auto dodaj = [](std::vector<double>& v, float x) { if (!std::isnan(x) && v.size() < 30) v.push_back(x); };
  auto bez = [&](const std::set<std::string> WykluczeniaNormy::*pole, const std::string& d) {
    return wm && (wm->*pole).count(d);
  };
  // tylko okno 90 dni przed dziś (4.4): koszt nie rośnie z długością historii
  for (size_t i = doDzis; i-- > 0;) {
    const Noc* r = hp.noce[i];
    if (r->data < granica) break;
    if (!r->wazna || wykluczone.count(r->data)) continue;
    if (r->m01 >= 0 && m01.size() < 30 && !bez(&WykluczeniaNormy::m01, r->data)) {
      m01.push_back(r->m01);
      if (r->zrodlo == 'K') nm.zKarty++;
    }
    if (!bez(&WykluczeniaNormy::ahi, r->data)) dodaj(ahi, r->ahi);
    if (!bez(&WykluczeniaNormy::m03, r->data)) {  // R-NOR-19: bez nocy niepewnych
      dodaj(oa, r->oa);
      if (!bez(&WykluczeniaNormy::ca, r->data)) dodaj(ca, r->ca);  // noce stanu CA/h (R-NOR-09)
      dodaj(hh, r->h);
    }
    if (r->m17 >= 0 && !bez(&WykluczeniaNormy::m17, r->data)) dodaj(m17, (float)r->m17);
    dodaj(m13, r->m13);  // M-13 bez wyłączeń ponad 4.4 (tabela sygnałów 4.4)
    if (r->m01 > 0 && !std::isnan(r->csr)) dodaj(csrp, r->csr / (float)r->m01 * 100.0f);  // CSR% = M-09 / M-01
    if (!nm.ustawienia.empty() && *hp.ust[i] != nm.ustawienia) continue;  // R-NOR-07: inne ustawienia
    dodaj(cmed, r->cmed);
    if (!bez(&WykluczeniaNormy::cp95, r->data)) dodaj(cp95, r->cp95);
    if (!bez(&WykluczeniaNormy::wp95, r->data)) dodaj(wp95, r->wp95);
    dodaj(emed, epapMed(*r));  // R-NOR-08: EPAP docelowe urządzenia (STR) poza normą
    if (!bez(&WykluczeniaNormy::ep95, r->data)) dodaj(ep95, epapP95(*r));
    dodaj(psmed, r->psmed);
    dodaj(psp95, r->psp95);
  }
  nm.n = (int)m01.size();
  nm.m01 = zbierz(m01);
  nm.ahi = zbierz(ahi);
  nm.cmed = zbierz(cmed);
  nm.cp95 = zbierz(cp95);
  nm.wp95 = zbierz(wp95);
  nm.emed = zbierz(emed);
  nm.ep95 = zbierz(ep95);
  nm.psmed = zbierz(psmed);
  nm.psp95 = zbierz(psp95);
  nm.oa = zbierz(oa);
  nm.ca = zbierz(ca);
  nm.h = zbierz(hh);
  nm.m17 = zbierz(m17);
  nm.m13 = zbierz(m13);
  nm.csrp = zbierz(csrp);
  return nm;
}

Norma liczNorme(const std::vector<Noc>& hist, const std::string& dzis, const std::set<std::string>& wykluczone,
                 const WykluczeniaNormy* wm) {
  return liczNorme(posortujHistorie(hist), dzis, wykluczone, wm);
}

Odchylenie odchylenieWNocy(const Noc& n, const Norma& nm) {
  Odchylenie o;
  if (!n.wazna) return o;
  if (nm.m01.jest() && n.m01 >= 0) o.m01 = n.m01 < nm.m01.p10 && n.m01 - nm.m01.med <= -90 + 1e-9;
  if (nm.ahi.jest() && !std::isnan(n.ahi)) o.ahi = n.ahi > nm.ahi.p90 && n.ahi - nm.ahi.med >= 3 - 1e-6;
  if (nm.wp95.jest() && !std::isnan(n.wp95)) o.wp95 = n.wp95 > nm.wp95.p90 && n.wp95 - nm.wp95.med >= 10 - 1e-6;
  return o;
}

int liczbaNocyWaznych(const std::vector<Noc>& hist, const std::string& najpozniej) {
  int k = 0;
  for (auto& r : hist)
    if (r.wazna && r.data <= najpozniej) k++;
  return k;
}

int seria(const std::vector<Noc>& hist, const std::string& dzis) {
  int k = 0;
  std::string d = dzis;
  for (;;) {
    const Noc* r = nocZDnia(hist, d);
    if (!r || r->m01 < 240) return k;
    k++;
    d = dodajDni(d, -1);
  }
}

int nocyPonad4h(const std::vector<Noc>& hist, const std::string& dzis) {
  int k = 0;
  for (auto& r : hist)
    if (r.data <= dzis && r.m01 >= 240) k++;
  return k;
}
