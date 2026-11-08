// SPDX-License-Identifier: GPL-3.0-or-later
#include "wspolne.h"
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cerrno>
#include <cstdlib>

const char* const DNI_KROTKO[7] = {"nd", "pn", "wt", "śr", "cz", "pt", "sb"};
const char* const MIESIACE_D[12] = {"stycznia", "lutego", "marca", "kwietnia", "maja", "czerwca",
                                    "lipca", "sierpnia", "września", "października", "listopada", "grudnia"};
const char* const MIESIACE_DW[12] = {"STYCZNIA", "LUTEGO", "MARCA", "KWIETNIA", "MAJA", "CZERWCA",
                                     "LIPCA", "SIERPNIA", "WRZEŚNIA", "PAŹDZIERNIKA", "LISTOPADA", "GRUDNIA"};

static int pole(const std::string& d, int od, int n) {
  int v = 0;
  for (int i = od; i < od + n; i++) v = v * 10 + (d[i] - '0');
  return v;
}

bool dataOk(const std::string& d) {
  if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;
  for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
    if (!isdigit((unsigned char)d[i])) return false;
  int r = pole(d, 0, 4), m = pole(d, 5, 2), dz = pole(d, 8, 2);
  if (m < 1 || m > 12 || dz < 1) return false;
  static const int DNI[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool przestepny = (r % 4 == 0 && r % 100 != 0) || r % 400 == 0;
  return dz <= DNI[m - 1] + (m == 2 && przestepny ? 1 : 0);  // 2026-02-29 i 2026-04-31 odrzucone
}

// days_from_civil, H. Hinnant
long nrDnia(const std::string& d) {
  if (!dataOk(d)) return -1;
  long y = pole(d, 0, 4);
  unsigned m = pole(d, 5, 2), dd = pole(d, 8, 2);
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + dd - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

std::string dataZNr(long z) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long y = (long)yoe + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned d = doy - (153 * mp + 2) / 5 + 1;
  const unsigned m = mp + (mp < 10 ? 3 : -9);
  y += m <= 2;
  char b[48];
  snprintf(b, sizeof b, "%04ld-%02u-%02u", y, m, d);
  return b;
}

std::string dodajDni(const std::string& d, int k) {
  long n = nrDnia(d);
  return n < 0 ? std::string() : dataZNr(n + k);
}

int dzienTygodnia(const std::string& d) {
  long n = nrDnia(d);
  if (n < 0) return -1;
  long w = (n + 4) % 7;  // 1970-01-01 był czwartkiem
  return (int)(w < 0 ? w + 7 : w);
}

int dniMiedzy(const std::string& od, const std::string& do_) { return (int)(nrDnia(do_) - nrDnia(od)); }

std::string ddmm(const std::string& d) { return dataOk(d) ? d.substr(8, 2) + "." + d.substr(5, 2) : std::string("–"); }

std::string dataZRokiem(const std::string& d) {
  if (!dataOk(d)) return "–";
  return std::to_string(pole(d, 8, 2)) + "." + d.substr(5, 2) + "." + d.substr(0, 4);
}
std::string dzienMiesiac(const std::string& d) {
  if (!dataOk(d)) return "–";
  return std::to_string(pole(d, 8, 2)) + " " + MIESIACE_D[pole(d, 5, 2) - 1];
}

bool brak(double v) { return std::isnan(v); }

bool indeksZTekstu(const char* t, int ile, int& wynik) {
  if (!t || !*t || ile <= 0) return false;
  for (const char* c = t; *c; c++)
    if (!isdigit((unsigned char)*c)) return false;  // też znak, spacja i reszta po liczbie
  errno = 0;
  char* koniec = nullptr;
  long v = strtol(t, &koniec, 10);
  if (errno == ERANGE || *koniec != 0 || v < 0 || v >= ile) return false;
  wynik = (int)v;
  return true;
}

std::string przecinek1(double v) {
  if (std::isnan(v)) return "–";
  char b[24];
  snprintf(b, sizeof b, "%.1f", std::floor(v * 10 + 0.5 + 1e-4) / 10);
  std::string s(b);
  for (auto& c : s)
    if (c == '.') c = ',';
  if (s == "-0,0") s = "0,0";
  if (!s.empty() && s[0] == '-') s = "\xE2\x88\x92" + s.substr(1);  // minus U+2212
  return s;
}

std::string calkowita(double v) {
  if (std::isnan(v)) return "–";
  long n = (long)std::floor(v + 0.5 + 1e-4);
  return n < 0 ? "\xE2\x88\x92" + std::to_string(-n) : std::to_string(n);  // minus U+2212
}

std::string godzMin(double m) {
  if (std::isnan(m) || m < 0) return "–";
  long x = std::lround(m);
  char b[48];
  snprintf(b, sizeof b, "%ld:%02ld", x / 60, x % 60);
  return b;
}

std::string godzH(int m) {
  if (m < 0) return "–";
  char b[32];
  snprintf(b, sizeof b, "%d h %02d", m / 60, m % 60);
  return b;
}

const char* nocyZRzedu(int n) {
  if (n == 1) return "noc";
  int j = n % 10, d = n % 100;
  if (j >= 2 && j <= 4 && !(d >= 12 && d <= 14)) return "noce";
  return "nocy";
}

uint32_t crc32(const uint8_t* d, size_t n) {
  if (!d || n == 0) return 0;
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

int nocyOdOdczytu(const std::string& dzis, const std::string& abData, int abMinuta) {
  if (!dataOk(dzis) || !dataOk(abData) || abMinuta < 0 || abData > dzis) return 0;
  return dniMiedzy(abData, dzis) + (abMinuta < 5 * 60 ? 1 : 0);
}

std::string bezBom(const std::string& t) {
  return t.compare(0, 3, "\xEF\xBB\xBF") == 0 ? t.substr(3) : t;
}
void obetnijKoniec(std::string& l) {
  while (!l.empty() && (l.back() == '\r' || l.back() == ' ' || l.back() == '\t')) l.pop_back();
}
