# SPDX-License-Identifier: GPL-3.0-or-later
# Dane do testu różnicowego normy tymczasowej (ADR-0048); ustawienia syntetyczne.
# Przerabia zrzut syntetyczny: ostatnie k nocy z nowymi ustawieniami (wyciek +10, PS +1); tryb AutoSet opcjonalnie.
import sys
plik, k, tryb = sys.argv[1], int(sys.argv[2]), sys.argv[3]
w = open(plik).read().splitlines()
out = []
for i, l in enumerate(w):
    p = l.split(";")
    nowa = i >= len(w) - k
    if tryb == "AutoSet":
        p[8] = p[9] = p[10] = p[11] = ""
        p[12] = "AutoSet/6/12/2" if nowa else "AutoSet/6/11/2"
        if nowa: p[4] = "%.2f" % (float(p[4]) - 1.5); p[5] = "%.2f" % (float(p[5]) - 1.5)
    else:
        p[12] = "ASVAuto/6/11/3/15" if nowa else "ASVAuto/6/10/3/15"
        if nowa: p[10] = "%.2f" % (float(p[10]) + 1.0); p[8] = "%.2f" % (float(p[8]) + 1.0)
    if nowa: p[7] = "%.2f" % (float(p[7]) + 12.0)
    out.append(";".join(p))
open(plik, "w").write("\n".join(out) + "\n")
