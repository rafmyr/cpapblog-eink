// SPDX-License-Identifier: GPL-3.0-or-later
// liczbaCalk/liczbaDluga/liczbaRzecz (wspolne.h) muszą dawać dokładnie to samo co atoi/atol/atof na tabeli
// wejść z pasa danych i konfiguracji (puste, spacje, znak, zera, prefiks, 1e3, przecinek, duże wartości, NAN, inf).
// Nie zmienia semantyki parserów: zamiana atoi → liczbaCalk w src/ nie może zmienić żadnego wyniku. Dane syntetyczne.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "wspolne.h"

static int ok = 0, zle = 0;

int main() {
  const char* wejscia[] = {"",        " ",        "   42",     "\t7",       "+5",        "-3",         "007",      "12abc",
                           "abc",     "1e3",      "1.5",       "1,5",       "-0",        "0x1F",       "99999999999",
                           "-99999999999", "2147483647", "2147483648", "-2147483648", "-2147483649",
                           "99999999999999999999999", "NAN", "nan", "inf", "-inf", "INFINITY", "1e400", "-1e400", "1e-400",
                           ".5", "5.", "+", "-", "1 2", "12 ", "٣", "0.1e1", "1E3", "0x1p3", "2026-11-05", "05",
                           "09", "00:30"};
  for (const char* s : wejscia) {
    int a = atoi(s), b = liczbaCalk(s);
    long c = atol(s), d = liczbaDluga(s);
    double e = atof(s), f = liczbaRzecz(s);
    bool rowne = a == b && c == d && std::memcmp(&e, &f, sizeof e) == 0;  // memcmp: NAN == NAN, znak zera
    if (rowne) ok++;
    else { zle++; printf("ROZNICA dla \"%s\": int %d/%d long %ld/%ld double %g/%g\n", s, a, b, c, d, e, f); }
  }
  // wycinek tekstu jak w parserach (substr(...).c_str()) i przesunięcie wskaźnika (c_str() + n)
  const std::string t = "2026-11-05 x";
  if (liczbaCalk(t.substr(5, 2).c_str()) == 11 && liczbaCalk(t.c_str() + 8) == 5 && liczbaCalk(t.substr(0, 4).c_str()) == 2026) ok++;
  else { zle++; printf("ROZNICA: wycinki dat\n"); }
  printf("testy_parsowanie_liczb: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
