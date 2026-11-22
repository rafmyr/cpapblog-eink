# SPDX-License-Identifier: GPL-3.0-or-later
"""Miejsce w obrazie firmware (T-TEST-518, 22.11.2026, spec techniczna 8.x „Budżet rozmiaru”).

1. Dane wkompilowane bez kopii: żadne dwie tablice w include/obrazy.h, include/fonty.h i include/fonty_slot.h nie mają
   tej samej treści (generator tools/prototyp/generuj.py zostawia scenę nocną identyczną z kartą jako wskaźnik).
2. Kod projektu bez wyjątków C++: platformio.ini kompiluje src/ z -fno-exceptions, więc w src/ nie ma try/catch/throw
   (testy na komputerze kompilują z wyjątkami i tego nie złapią; na panelu byłby błąd kompilacji).
"""
import hashlib
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FW = os.path.join(REPO, "firmware", "prototyp")
TABLICA = re.compile(r"const\s+(?:uint8_t|uint16_t|GFXglyph)\s+(\w+)\[\]\s*(?:PROGMEM)?\s*=\s*\{(.*?)\};", re.S)


def tablice(plik):
    with open(os.path.join(FW, "include", plik), encoding="utf-8") as f:
        t = f.read()
    return {m.group(1): re.sub(r"\s+", "", m.group(2)) for m in TABLICA.finditer(t)}


def test_dane_bez_kopii():
    for plik in ("obrazy.h", "fonty.h", "fonty_slot.h"):
        tab = tablice(plik)
        assert len(tab) > 0, plik
        po_tresci = {}
        for n, c in tab.items():
            po_tresci.setdefault(hashlib.sha256(c.encode()).hexdigest(), []).append(n)
        kopie = [v for v in po_tresci.values() if len(v) > 1]
        assert kopie == [], f"{plik}: tablice o tej samej treści {kopie}"


def test_sceny_nocne_wspolne_z_karta():
    with open(os.path.join(FW, "include", "obrazy.h"), encoding="utf-8") as f:
        t = f.read()
    a = t.index("static const SceneBudzika SCENY_NOCY[] = {")
    wiersze = re.findall(r'\{"(A\d-[A-Z]\d\d)", (\w+), (\w+)_W, (\w+)_H\}', t[a:t.index("};", a)])
    assert len(wiersze) == 8
    for ident, bmp, w, h in wiersze:
        assert w == h == "NOC_" + ident.replace("-", "_"), ident
        assert bmp in (w, "BUDZIK_" + ident[3:]), ident  # własna tablica albo karta tej samej sceny
        assert re.search(r"const uint8_t %s\[\]" % bmp, t), (ident, bmp)


def test_src_bez_wyjatkow():
    with open(os.path.join(FW, "platformio.ini"), encoding="utf-8") as f:
        ini = f.read()
    assert re.search(r"^build_src_flags\s*=\s*\n\s+-fno-exceptions", ini, re.M)
    zle = []
    for n in sorted(os.listdir(os.path.join(FW, "src"))):
        if not n.endswith((".cpp", ".h")):
            continue
        with open(os.path.join(FW, "src", n), encoding="utf-8") as f:
            for i, w in enumerate(f, 1):
                kod = re.sub(r'"(?:\\.|[^"\\])*"', '""', w.split("//")[0])  # bez literałów (JavaScript w HTML) i komentarzy
                if re.search(r"\b(try|catch|throw)\b", kod):
                    zle.append(f"{n}:{i}")
    assert zle == [], zle
