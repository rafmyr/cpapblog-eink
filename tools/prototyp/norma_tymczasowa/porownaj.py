# SPDX-License-Identifier: GPL-3.0-or-later
# Porównanie dwóch przebiegów symulatora (ADR-0048): z normą tymczasową kontra wymuszony brak normy tymczasowej.
# Klatki mogą się różnić tylko na ekranach K1, K2 i „Co znaczą te liczby” (LICZBY); raporty PDF, log panelu
# i dziennik bez różnic.
# Kod wyjścia 0 = zgodnie z zasadą, 1 = różnica gdzie indziej albo brak różnic (test nic nie sprawdza).
import collections
import os
import sys

a, b = sys.argv[1], sys.argv[2]
fa = sorted(f for f in os.listdir(a) if f.endswith(".pbm"))
fb = sorted(f for f in os.listdir(b) if f.endswith(".pbm"))
blad = 0
if fa != fb:
    print("BŁĄD: inne nazwy klatek", len(fa), len(fb))
    blad = 1
roz = collections.Counter()
for x in fa:
    if x in fb and open(os.path.join(a, x), "rb").read() != open(os.path.join(b, x), "rb").read():
        roz[x.rsplit("-", 1)[-1][:-4]] += 1
dozwolone = {"K1", "K2", "LICZBY"}
inne = {k: v for k, v in roz.items() if k not in dozwolone}
print("klatek", len(fa), "różne wg ekranu:", dict(roz))
if inne:
    print("BŁĄD: różnice poza K1, K2, LICZBY:", inne)
    blad = 1
if not roz:
    print("BŁĄD: brak różnic, test nie sprawdza normy tymczasowej")
    blad = 1
pa = sorted(f for f in os.listdir(a) if f.endswith(".pdf"))
pb = sorted(f for f in os.listdir(b) if f.endswith(".pdf"))
print("raportów PDF", len(pa))
if pa != pb:
    print("BŁĄD: inne pliki PDF", pa, pb)
    blad = 1
for x in pa:
    if x in pb and open(os.path.join(a, x), "rb").read() != open(os.path.join(b, x), "rb").read():
        print("BŁĄD: różny raport", x)
        blad = 1
if not pa:
    print("BŁĄD: brak raportów PDF")
    blad = 1
for log in ("log.txt", "log-panelu.txt"):
    pa, pb = os.path.join(a, log), os.path.join(b, log)
    if os.path.exists(pa) and os.path.exists(pb) and open(pa).read() != open(pb).read():
        print("BŁĄD: różny", log)
        blad = 1
sys.exit(blad)
