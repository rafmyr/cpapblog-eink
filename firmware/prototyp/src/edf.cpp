// SPDX-License-Identifier: GPL-3.0-or-later
#include "edf.h"
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace edf {

const char* opisBledu(Blad b) {
  switch (b) {
    case Blad::BRAK: return "ok";
    case Blad::KROTKI_NAGLOWEK: return "plik krótszy niż nagłówek";
    case Blad::WERSJA: return "nieznana wersja EDF";
    case Blad::NAGLOWEK: return "zły nagłówek";
    case Blad::SYGNAL: return "zły opis sygnału";
    case Blad::REKORDY: return "zła liczba rekordów";
    case Blad::UCIETY: return "plik ucięty";
  }
  return "?";
}

// Pole tekstowe nagłówka bez spacji na końcach (latin-1 bajt w bajt, jak _tekst() w edf.py).
static std::string tekst(const uint8_t* b, size_t n) {
  size_t a = 0, z = n;
  while (a < z && (b[a] == ' ' || b[a] == 0)) a++;
  while (z > a && (b[z - 1] == ' ' || b[z - 1] == 0)) z--;
  return std::string((const char*)b + a, z - a);
}

static bool liczba(const uint8_t* b, size_t n, double& v) {
  std::string t = tekst(b, n);
  if (t.empty()) return false;
  char* e = nullptr;
  errno = 0;
  v = std::strtod(t.c_str(), &e);
  return e && *e == 0 && errno != ERANGE && std::isfinite(v);
}

static bool calkowita(const uint8_t* b, size_t n, long& v) {
  double d;
  if (!liczba(b, n, d) || d < -2e9 || d > 2e9) return false;
  v = (long)d;  // jak int(float(...)) w edf.py
  return true;
}

long long dniOdEpoki(int r, int m, int d) {  // H. Hinnant, days_from_civil
  r -= m <= 2;
  const long long era = (r >= 0 ? r : r - 399) / 400;
  const unsigned yoe = (unsigned)(r - era * 400);
  const unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long long)doe - 719468;
}

// "dd.mm.yy" i "hh.mm.ss" → sekundy cywilne; rok yy ≥ 85 → 19yy, inaczej 20yy (jak _start() w edf.py).
static bool start(const std::string& data, const std::string& godz, long long& s) {
  auto trzy = [](const std::string& t, int w[3]) {
    if (t.size() != 8 || t[2] != '.' || t[5] != '.') return false;
    for (int i = 0; i < 3; i++) {
      char a = t[(size_t)i * 3], b = t[(size_t)i * 3 + 1];
      if (a < '0' || a > '9' || b < '0' || b > '9') return false;
      w[i] = (a - '0') * 10 + (b - '0');
    }
    return true;
  };
  int d[3], g[3];
  if (!trzy(data, d) || !trzy(godz, g)) return false;
  int rok = d[2] >= 85 ? 1900 + d[2] : 2000 + d[2];
  static const int DNI[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (d[1] < 1 || d[1] > 12 || d[0] < 1 || d[0] > DNI[d[1] - 1]) return false;
  bool przestepny = (rok % 4 == 0 && rok % 100 != 0) || rok % 400 == 0;
  if (d[1] == 2 && d[0] == 29 && !przestepny) return false;
  if (g[0] > 23 || g[1] > 59 || g[2] > 59) return false;
  s = dniOdEpoki(rok, d[1], d[0]) * 86400LL + (long long)g[0] * 3600 + (long long)g[1] * 60 + g[2];
  return true;
}

Blad czytaj(const uint8_t* b, size_t n, Plik& wynik) {
  if (!b || n < 256) return Blad::KROTKI_NAGLOWEK;
  if (tekst(b, 8) != "0") return Blad::WERSJA;
  Plik p;
  if (!start(tekst(b + 168, 8), tekst(b + 176, 8), p.startS)) return Blad::NAGLOWEK;
  long bajtyNag, rekordy, ns;
  double czas;
  if (!calkowita(b + 184, 8, bajtyNag) || !calkowita(b + 236, 8, rekordy) || !liczba(b + 244, 8, czas) ||
      !calkowita(b + 252, 4, ns))
    return Blad::NAGLOWEK;
  if (ns < 1 || ns > MAKS_SYGNALOW || bajtyNag != 256 + 256 * ns) return Blad::NAGLOWEK;
  if (n < (size_t)bajtyNag) return Blad::KROTKI_NAGLOWEK;
  if (rekordy < 0 || rekordy > MAKS_REKORDOW) return Blad::REKORDY;
  std::string zastrz = tekst(b + 192, 44);
  p.edfPlus = zastrz.compare(0, 4, "EDF+") == 0;
  p.edfPlusD = zastrz.compare(0, 5, "EDF+D") == 0;
  p.czasRekordu = czas;
  const uint8_t* opis = b + 256;
  auto kol = [&](size_t poczatek, size_t szer, long i) { return opis + poczatek * (size_t)ns + (size_t)i * szer; };
  size_t offset = 0;
  p.sygnaly.reserve((size_t)ns);
  for (long i = 0; i < ns; i++) {
    Sygnal s;
    s.etykieta = tekst(kol(0, 16, i), 16);
    s.jednostka = tekst(kol(96, 8, i), 8);
    long pr;
    if (!liczba(kol(104, 8, i), 8, s.fizMin) || !liczba(kol(112, 8, i), 8, s.fizMax) ||
        !calkowita(kol(120, 8, i), 8, s.cyfMin) || !calkowita(kol(128, 8, i), 8, s.cyfMax) ||
        !calkowita(kol(216, 8, i), 8, pr))
      return Blad::NAGLOWEK;
    // próbka EDF ma 16 bitów: ze znakiem przy cyf_min < 0, inaczej bez znaku (aparat ResMed ma sygnały 0…65534,
    // np. `PatientHours`; jak noce.py); zakres poza [-32768, 65535] to uszkodzony opis
    if (s.cyfMin >= s.cyfMax || s.cyfMin < -32768 || s.cyfMax > 65535 || s.fizMin == s.fizMax || pr < 1 ||
        pr > 1000000)
      return Blad::SYGNAL;
    s.probki = (int)pr;
    s.offset = offset;
    offset += (size_t)pr * 2;
    p.sygnaly.push_back(s);
  }
  p.bajtyNaglowka = (size_t)bajtyNag;
  p.bajtyRekordu = offset;
  // rekordy × rozmiar rekordu bez przepełnienia: porównanie przez dzielenie
  size_t naDane = n - p.bajtyNaglowka;
  if (p.bajtyRekordu && (size_t)rekordy > naDane / p.bajtyRekordu) return Blad::UCIETY;
  p.rekordy = rekordy;
  // bajty za zadeklarowanymi rekordami (np. nagłówek sprzed dopisania): plik niepełny jak `niepelny` w edf.py; czytane
  // są tylko zadeklarowane rekordy
  p.niepelny = naDane != (size_t)rekordy * p.bajtyRekordu;
  // czas rekordu ≤ 0 przy sygnałach pomiarowych (uszkodzony nagłówek): czasy próbek byłyby ujemne albo zerowe; plik
  // z samymi adnotacjami (EVE, CSL) może mieć czas rekordu 0
  if (!(p.czasRekordu > 0))
    for (auto& s : p.sygnaly)
      if (!s.adnotacje() && !s.crc()) return Blad::NAGLOWEK;
  p.dane = b;
  p.rozmiar = n;
  wynik = std::move(p);
  return Blad::BRAK;
}

const Sygnal* Plik::sygnal(const std::string& e) const {
  for (auto& s : sygnaly)
    if (s.etykieta == e && !s.crc()) return &s;
  return nullptr;
}

uint16_t Plik::surowa(const Sygnal& s, long rekord, int i) const {
  if (!dane || rekord < 0 || rekord >= rekordy || i < 0 || i >= s.probki) return 0xFFFF;
  size_t o = bajtyNaglowka + (size_t)rekord * bajtyRekordu + s.offset + (size_t)i * 2;
  if (o + 1 >= rozmiar) return 0xFFFF;
  return (uint16_t)(dane[o] | (dane[o + 1] << 8));
}

// Liczba z tekstu TAL (np. "+3600", "-0.5", "12"): cały tekst, skończona (jak float() w edf.py, bez spacji).
static bool liczbaTal(const std::string& t, double& v) {
  if (t.empty()) return false;
  char* e = nullptr;
  errno = 0;
  v = std::strtod(t.c_str(), &e);
  return e && *e == 0 && e != t.c_str() && errno != ERANGE && std::isfinite(v);
}

bool adnotacje(const Plik& p, std::vector<Adnotacja>& wynik) {
  std::vector<Adnotacja> w;
  const Sygnal* s = nullptr;
  for (auto& x : p.sygnaly)
    if (x.adnotacje()) { s = &x; break; }
  if (!s) {
    wynik.swap(w);
    return true;
  }
  const size_t dl = (size_t)s->probki * 2;
  for (long r = 0; r < p.rekordy; r++) {
    size_t o = p.bajtyNaglowka + (size_t)r * p.bajtyRekordu + s->offset;
    if (!p.dane || o > p.rozmiar || dl > p.rozmiar - o) return false;
    const char* blok = (const char*)p.dane + o;
    size_t a = 0;
    while (a < dl) {
      size_t b = a;
      while (b < dl && blok[b] != 0) b++;
      if (b > a) {  // TAL [a, b)
        std::string tal(blok + a, b - a);
        std::vector<std::string> czesci;
        size_t c = 0;
        for (;;) {
          size_t d = tal.find('\x14', c);
          czesci.push_back(tal.substr(c, d == std::string::npos ? std::string::npos : d - c));
          if (d == std::string::npos) break;
          c = d + 1;
        }
        const std::string& czas = czesci[0];
        if (czas.empty() || (czas[0] != '+' && czas[0] != '-')) return false;
        Adnotacja ad;
        size_t k = czas.find('\x15');
        std::string on = k == std::string::npos ? czas : czas.substr(0, k);
        if (k != std::string::npos) {
          if (!liczbaTal(czas.substr(k + 1), ad.trwanie) || ad.trwanie < 0) return false;
          ad.maTrwanie = true;
        }
        if (!liczbaTal(on, ad.onset)) return false;
        for (size_t i = 1; i < czesci.size(); i++) {
          if (czesci[i].empty()) continue;
          Adnotacja x = ad;
          const std::string& t = czesci[i];  // bez białych znaków na końcach (str.strip() w edf.py)
          size_t p0 = 0, p1 = t.size();
          while (p0 < p1 && std::isspace((unsigned char)t[p0])) p0++;
          while (p1 > p0 && std::isspace((unsigned char)t[p1 - 1])) p1--;
          x.tekst = t.substr(p0, p1 - p0);
          w.push_back(std::move(x));
        }
      }
      a = b + 1;
    }
  }
  wynik.swap(w);
  return true;
}

}  // namespace edf
