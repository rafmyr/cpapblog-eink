# SPDX-License-Identifier: GPL-3.0-or-later
# Scenariusz nocy do testu różnicowego normy tymczasowej (ADR-0048); ustawienia syntetyczne.
import sys, datetime as dt, random
import os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../symulator"))
import scenariusze as S
tryb, plik = sys.argv[1], sys.argv[2]
asv = tryb.startswith("ASV")
w = {"tryb": tryb, "ustawienia": "ASVAuto/6/11/3/15" if asv else "AutoSet/6/12/2", "pna": "AirSense 10", "srn": "123"}
out = []
los = random.Random(5)
for i in range(22):
    d = dt.date(2026, 11, 5) + dt.timedelta(days=i)
    zm = {"wp95": los.uniform(20, 34) if i % 6 else 46.0, "wmed": los.uniform(8, 12)}
    if asv: zm.update({"emed": los.uniform(7.8, 8.4), "ep95": los.uniform(9.0, 9.6), "psmed": los.uniform(4.6, 5.2)})
    else: zm.update({"cmed": los.uniform(8.0, 8.6), "cp95": los.uniform(10.5, 11.5)})
    if i in (9, 10, 11): zm.update({"ca": 6.5, "oa": 2.0})  # kilka nocy z wysokim AHI
    out.append(S.noc(d, w, **zm))
open(plik, "w").write("\n".join(out) + "\n")
