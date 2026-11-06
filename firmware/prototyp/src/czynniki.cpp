// SPDX-License-Identifier: GPL-3.0-or-later
#include "czynniki.h"
#include "wspolne.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

const PozycjaPytania POZYCJE[8] = {
    {CZ_ALKOHOL, 'A', "alkohol"},
    {CZ_KATAR, 'K', "katar lub infekcja"},
    {CZ_STRES, 'S', "stres"},
    {CZ_KOLACJA, 'J', "późna, ciężka kolacja"},
    {CZ_LEKI, 'L', "leki nasenne, uspokajające albo silne przeciwbólowe"},
    {CZ_THC, 'T', "THC"},
    {CZ_POZA_DOMEM, 'P', "spanie poza domem"},
    {CZ_NIC, 'N', "nic szczególnego"},
};

std::string kody(uint8_t c) {
  std::string s;
  for (auto& p : POZYCJE)
    if (c & p.bit) s += p.kod;
  return s;
}

static bool zKodow(const std::string& s, uint8_t& c) {
  c = 0;
  for (char k : s) {
    bool ok = false;
    for (auto& p : POZYCJE)
      if (p.kod == k) { c |= p.bit; ok = true; }
    if (!ok) return false;
  }
  return true;
}

bool parsujWpis(const std::string& l, WpisCzynnikow& w) {
  std::string p[6];
  int n = 0;
  size_t start = 0;
  std::string linia = l;
  if (!linia.empty() && linia.back() == '\r') linia.pop_back();
  for (size_t i = 0; i <= linia.size() && n < 6; i++)
    if (i == linia.size() || linia[i] == ';') { p[n++] = linia.substr(start, i - start); start = i + 1; }
  if (n < 6 || !dataOk(p[0]) || p[1].size() != 1 || p[3].size() != 1) return false;
  WpisCzynnikow r;
  r.data = p[0];
  r.stan = p[1][0];
  if (r.stan != 'C' && r.stan != 'O' && r.stan != 'B') return false;
  if (!zKodow(p[2], r.czynniki)) return false;
  r.warstwa = p[3][0];
  if (r.warstwa != 'O' && r.warstwa != 'K' && r.warstwa != 'Z' && r.warstwa != 'A') return false;
  char* e;
  long t = strtol(p[4].c_str(), &e, 10);
  if (e == p[4].c_str() || *e) return false;
  r.termin = (int)t;
  if (p[5] != "0" && p[5] != "1") return false;
  r.pokazano = p[5] == "1";
  if (r.stan == 'O' && !r.czynniki) return false;
  w = r;
  return true;
}

std::string zapiszWpis(const WpisCzynnikow& w) {
  return w.data + ";" + w.stan + ";" + (w.stan == 'O' ? kody(w.czynniki) : std::string()) + ";" + w.warstwa + ";" +
         std::to_string(w.termin) + ";" + (w.pokazano ? "1" : "0");
}

WpisCzynnikow* wpisZDnia(std::vector<WpisCzynnikow>& v, const std::string& data) {
  for (auto& w : v)
    if (w.data == data) return &w;
  return nullptr;
}

const WpisCzynnikow* wpisZDnia(const std::vector<WpisCzynnikow>& v, const std::string& data) {
  for (auto& w : v)
    if (w.data == data) return &w;
  return nullptr;
}

bool losujKontrolna(const std::string& data, uint32_t ziarno) {
  uint32_t h = 2166136261u;
  auto dodaj = [&](uint8_t b) { h ^= b; h *= 16777619u; };
  for (char c : data) dodaj((uint8_t)c);
  for (int i = 0; i < 4; i++) dodaj((uint8_t)(ziarno >> (8 * i)));
  h ^= h >> 15;  // dokończenie mieszania: same ostatnie bity FNV słabo zależą od ostatnich bajtów
  h *= 0x2c1b3c6dU;
  h ^= h >> 12;
  return h % 7 == 0;
}

int terminOdpowiedzi(int minutaImportu) {
  if (minutaImportu <= 9 * 60) return 11 * 60;
  int t = minutaImportu + 120;
  return t > 23 * 60 + 30 ? 23 * 60 + 30 : t;
}

bool odchylenieCzynnikow(const Noc& n, const Norma& nm) {
  if (!n.wazna) return false;
  if (odchylenieWNocy(n, nm).jakiekolwiek()) return true;
  // M-13 (4.5: Δ 2 / min, kierunek w górę, P90), w wyzwalaczu R-CZY-01 i w warstwie „odchylenie” R-CZY-24
  return nm.m13.jest() && !std::isnan(n.m13) && n.m13 > nm.m13.p90 && n.m13 - nm.m13.med >= 2 - 1e-6;
}

char decyzjaZaproszenia(const WejscieDecyzji& we) {
  if (!we.noc || !we.noc->wazna) return 0;
  if (we.nocyWaznychPrzed < 14) return 0;  // R-CZY-09: rozruch globalny, tylko na żądanie
  if (we.prosbaNaEkranie) return 0;        // R-PIE-10, R-PIE-11: linia przepada tego dnia
  if (we.wzorzecA) return 0;               // R-CZY-01, R-PIE-06: tylko na żądanie
  if (we.sk30) return 0;                   // R-CZY-15: S-K-30 ma własne pytanie o infekcję
  if (odchylenieCzynnikow(*we.noc, we.norma)) return 'O';
  if (we.sk17) return 'Z';                 // R-CZY-07: zaproszenie poza warstwami O-12 (R-CZY-24)
  if (losujKontrolna(we.noc->data, we.ziarno)) return 'K';
  return 0;
}

bool zaproszenieAktywne(const WpisCzynnikow* w, const std::string& dzis, int minutaTeraz) {
  return w && w->stan == 'C' && w->data == dzis && minutaTeraz < w->termin;
}

bool zamknijPoTerminie(std::vector<WpisCzynnikow>& v, const std::string& dzis, int minutaTeraz) {
  bool zmiana = false;
  for (auto& w : v)
    if (w.stan == 'C' && (w.data < dzis || (w.data == dzis && minutaTeraz >= w.termin))) {
      w.stan = 'B';
      w.czynniki = 0;
      zmiana = true;
    }
  return zmiana;
}

uint8_t przelacz(uint8_t z, int poz) {
  if (poz < 0 || poz > 7) return z;
  uint8_t b = POZYCJE[poz].bit;
  if (z & b) return z & ~b;
  if (b == CZ_NIC) return CZ_NIC;
  return (z & ~CZ_NIC) | b;
}

void zapiszOdpowiedz(std::vector<WpisCzynnikow>& v, const std::string& data, uint8_t c, bool zLinii) {
  WpisCzynnikow* w = wpisZDnia(v, data);
  // odpowiedź z K1 po terminie albo po pominięciu nie wynika z pokazanej linii zaproszenia, więc do decyzji
  // trafia do warstwy Z (bez O-12), nie do warstwy O ani K
  if (w && !zLinii && w->stan == 'B') w->warstwa = 'Z';
  // odpowiedź użytkownika zastępuje oznaczenie automatyczne (fail-path R-CZY-19, R-CZY-13); bez warstwy O-12
  if (w && w->warstwa == 'A') {
    w->warstwa = 'Z';
    w->termin = -1;
  }
  if (!w) {
    WpisCzynnikow n;
    n.data = data;
    n.warstwa = 'Z';
    v.push_back(n);
    w = &v.back();
  }
  w->stan = c ? 'O' : 'B';
  w->czynniki = c;
}

void pominPytanie(std::vector<WpisCzynnikow>& v, const std::string& data) {
  WpisCzynnikow* w = wpisZDnia(v, data);
  if (w && w->stan == 'O') return;  // pominięcie nie kasuje wcześniejszej odpowiedzi tej nocy (ani oznaczenia A)
  zapiszOdpowiedz(v, data, 0, true);
}

std::set<std::string> wykluczoneZNormy(const std::vector<WpisCzynnikow>& v) {
  std::set<std::string> s;
  for (auto& w : v)
    if (w.stan == 'O' && (w.czynniki & (CZ_KATAR | CZ_POZA_DOMEM))) s.insert(w.data);
  return s;
}

std::set<std::string> nocePozaDomem(const std::vector<WpisCzynnikow>& v) {
  std::set<std::string> s;
  for (auto& w : v)
    if (w.stan == 'O' && (w.czynniki & CZ_POZA_DOMEM)) s.insert(w.data);
  return s;
}

bool romb(const WpisCzynnikow* w) {
  return w && w->stan == 'O' && (w->czynniki & (CZ_ALKOHOL | CZ_STRES | CZ_KOLACJA | CZ_LEKI | CZ_THC));
}

// ---------- wagi O-12 (R-CZY-24) ----------
bool wWarstwieO12(const WpisCzynnikow& w) {
  return w.stan == 'O' && w.pokazano && (w.warstwa == 'O' || w.warstwa == 'K') && w.czynniki != 0;
}

double odsetekOdpowiedzi(const std::vector<WpisCzynnikow>& v, const std::string& dzis, char warstwa, int dni) {
  std::string od = dodajDni(dzis, -(dni - 1));
  int pok = 0, odp = 0;
  for (auto& w : v) {
    if (w.warstwa != warstwa || !w.pokazano || w.data < od || w.data > dzis) continue;
    if (w.stan == 'C') continue;  // czeka na odpowiedź do terminu
    pok++;
    if (w.stan == 'O') odp++;
  }
  return pok ? (double)odp / pok : NAN;
}

// ---------- spanie poza domem po przerwie AirBridge ----------
int oznaczPozaDomem(std::vector<WpisCzynnikow>& v, const std::vector<std::string>& noce, const std::string& dataOst,
                    int minutaOst, const std::string& dataTeraz, int minutaTeraz, const ProgiWyjazdu& p) {
  if (!dataOk(dataOst) || !dataOk(dataTeraz) || minutaOst < 0 || minutaTeraz < 0) return 0;
  long przerwa = (nrDnia(dataTeraz) - nrDnia(dataOst)) * 1440L + minutaTeraz - minutaOst;
  if (przerwa < (long)p.przerwaDob * 1440L) return 0;
  bool ostatnia = minutaTeraz >= p.oknoOd && minutaTeraz <= p.oknoDo;
  int ile = 0;
  for (auto& d : noce) {
    if (!dataOk(d) || d <= dataOst || d > dataTeraz) continue;
    if (d == dataTeraz && !ostatnia) continue;  // powrót do domu: pierwsza odpowiedź rano, ostatnia noc w domu
    WpisCzynnikow* w = wpisZDnia(v, d);
    if (w && w->stan == 'O' && w->warstwa != 'A') continue;  // odpowiedź użytkownika ma pierwszeństwo (R-CZY-13)
    if (!w) {
      WpisCzynnikow n;
      n.data = d;
      v.push_back(n);
      w = &v.back();
    }
    w->stan = 'O';
    w->czynniki = CZ_POZA_DOMEM;
    w->warstwa = 'A';
    w->termin = (int)nrDnia(dataTeraz);
    w->pokazano = false;
    ile++;
  }
  return ile;
}

std::vector<OkresWyjazdu> okresyWyjazdu(const std::vector<WpisCzynnikow>& v) {
  std::vector<const WpisCzynnikow*> a;
  for (auto& w : v)
    if (w.warstwa == 'A' && w.stan == 'O') a.push_back(&w);
  std::sort(a.begin(), a.end(), [](const WpisCzynnikow* x, const WpisCzynnikow* y) { return x->data < y->data; });
  std::vector<OkresWyjazdu> o;
  for (auto* w : a) {
    if (!o.empty() && dodajDni(o.back().do_, 1) == w->data) {
      o.back().do_ = w->data;
      o.back().nocy++;
      o.back().zapis = std::max(o.back().zapis, (long)w->termin);
      continue;
    }
    OkresWyjazdu n;
    n.od = n.do_ = w->data;
    n.nocy = 1;
    n.zapis = w->termin;
    o.push_back(n);
  }
  return o;
}

const OkresWyjazdu* okresDoPytania(const std::vector<OkresWyjazdu>& o, const std::set<std::string>& zapytane,
                                   const std::string& dzis, int dni) {
  long d = nrDnia(dzis);
  for (auto& x : o) {
    if (zapytane.count(x.od)) continue;
    if (x.zapis >= 0 && d - x.zapis > dni) continue;  // przepada po 7 dniach (fail-path R-CZY-22)
    return &x;
  }
  return nullptr;
}

std::vector<WpisCzynnikow> zdejmijPozaDomem(std::vector<WpisCzynnikow>& v, const OkresWyjazdu& o) {
  std::vector<WpisCzynnikow> zdjete, zostaja;
  for (auto& w : v) {
    if (w.warstwa == 'A' && w.stan == 'O' && w.data >= o.od && w.data <= o.do_) zdjete.push_back(w);
    else zostaja.push_back(w);
  }
  v.swap(zostaja);
  return zdjete;
}

void przywrocWpisy(std::vector<WpisCzynnikow>& v, const std::vector<WpisCzynnikow>& zdjete) {
  for (auto& z : zdjete)
    if (!wpisZDnia(v, z.data)) v.push_back(z);
}

TekstyWyjazdu tekstyWyjazdu(const OkresWyjazdu& o, int termin) {
  TekstyWyjazdu t;
  t.etykieta = "Noce bez AirBridge: " + std::to_string(o.nocy);
  t.tytul = "Spanie poza domem od " + ddmm(o.od) + "?";
  t.opis = "Panel nie miał wtedy połączenia z AirBridge.";
  char b[8];
  int m = termin >= 0 ? termin : 11 * 60;
  snprintf(b, sizeof b, "%02d:%02d", m / 60 % 24, m % 60);
  t.stopka = std::string("Bez odpowiedzi do ") + b + " = zostaje „poza domem”.";
  return t;
}

// ---------- raport ----------
std::vector<std::string> zdaniaRaportu(const std::vector<WpisCzynnikow>& v, const std::string& dzis, uint8_t wl,
                                       bool raportL01) {
  std::vector<std::string> z;
  std::string od = dodajDni(dzis, -13);
  for (auto& p : POZYCJE) {
    if (p.bit == CZ_NIC || !(wl & p.bit)) continue;
    if (p.bit == CZ_LEKI && !raportL01) continue;  // ADR-0013 pkt 2: leki tylko w raporcie o L-01
    int n = 0;
    for (auto& w : v)
      if (w.stan == 'O' && w.warstwa != 'A' && (w.czynniki & p.bit) && w.data >= od && w.data <= dzis) n++;
    if (n) z.push_back(std::string("Zgłoszone przez pacjenta: ") + p.etykieta + " w " + std::to_string(n) + " z 14 nocy.");
  }
  if (wl & CZ_POZA_DOMEM) {
    int n = 0;
    for (auto& w : v)
      if (w.stan == 'O' && w.warstwa == 'A' && w.data >= od && w.data <= dzis) n++;
    if (n) z.push_back("Bez danych z domu: " + std::to_string(n) + " z 14 nocy.");
  }
  return z;
}
