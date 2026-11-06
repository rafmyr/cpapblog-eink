# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy przypadków brzegowych: sprawdzenia styków modułów, których nie da się wygodnie zrobić w C++.

Uruchomione wprost (`python3 tools/test_styki_modulow_brzegowe.py`,
z `tools/prototyp/testy.sh`) wypisuje `BŁĄD …` i kończy się kodem 1, dopóki usterka trwa. W `pytest`
przypadki z usterką są oznaczone `xfail` (bramka zostaje zielona, poprawka daje XPASS do zdjęcia znacznika), a sprawdzenia
„trzyma” pilnują regresów normalnie. `CPAP_STYKI_PIO=1` dokłada kompilację `pio run -e e1001` (ok. 1 min).
"""
import csv
import os
import re
import shutil
import subprocess
import sys
import tempfile

import pytest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KATALOG = os.path.join(REPO, "docs", "spec", "katalog")
SRC = os.path.join(REPO, "firmware", "prototyp", "src")


def _klucze():
    klucze, ids = set(), set()
    for plik in ("E.csv", "L.csv", "H.csv", "O.csv", "SK.csv"):
        with open(os.path.join(KATALOG, plik), encoding="utf-8") as f:
            for r in csv.reader(f, delimiter=";"):
                if not r or r[0].startswith("#") or r[0] in ("id_reguly", "id"):
                    continue
                ids.add(r[0])
                klucze.add(r[0] + ("/" + r[1] if len(r) > 1 and r[1] else ""))
    return klucze, ids


def odwolania_bez_celu():
    """Odwołania do reguł w kolumnach `wyklucza`, `wymaga` i parametrach `regula`, `skladniki` katalogów E, L, H, O,
    które nie wskazują żadnego wiersza (id albo id/wariant) w E, L, H, O, SK."""
    klucze, ids = _klucze()
    zle = []
    for plik in ("E.csv", "L.csv", "H.csv", "O.csv"):
        with open(os.path.join(KATALOG, plik), encoding="utf-8") as f:
            for r in csv.DictReader(f, delimiter=";"):
                refs = []
                for kol in ("wyklucza", "wymaga"):
                    refs += [(kol, x) for x in re.split(r"[,+]", r.get(kol) or "")]
                for p in (r.get("parametry") or "").split(","):
                    if "=" in p:
                        k, v = p.split("=", 1)
                        if k in ("regula", "skladniki"):
                            refs += [(k, x) for x in v.split("+")]
                for kol, x in refs:
                    x = x.strip()
                    if x and x not in klucze and x not in ids:
                        zle.append(f"{plik} {r['id_reguly']}/{r['wariant']} {kol}={x}")
    return zle


def include_w_komentarzu():
    """`#include` sklejone z wierszem komentarza `// …` (preprocesor go nie widzi)."""
    zle = []
    for nazwa in sorted(os.listdir(SRC)):
        if not nazwa.endswith((".cpp", ".h")):
            continue
        with open(os.path.join(SRC, nazwa), encoding="utf-8") as f:
            for i, w in enumerate(f, 1):
                if re.search(r"//.*#include\s*<", w):
                    zle.append(f"{nazwa}:{i}: {w.strip()[-60:]}")
    return zle


# E-23 odwołuje się do `S-K-10/jednonocny` (wyklucza) i `S-K-10/od_kilku_nocy` (wiersz pomocniczy
# E-23/bez_sk10_kilka), a SK.csv ma warianty `` i `kilka`. Wiersz pomocniczy jest więc zawsze „nie oceniam”
# (log: `brak reguly S-K-10/od_kilku_nocy w katalogu`), a warunek 6.4 „E-23 nie wchodzi, gdy spełniony jest wariant
# od kilku nocy S-K-10” nie działa nigdzie: przy wycieku kilka nocy z rzędu ekranem jest E-23 (R5b), nie S-K-10.
# poprawione: odwołania E-23 do `S-K-10` i `S-K-10/kilka`, kontrola w buduj_zestaw.py
def test_odwolania_katalogu():
    assert odwolania_bez_celu() == []


# (przed wgraniem): `#include <algorithm>` w src/reguly_rodzaje.cpp sklejony z komentarzem (4f8c7a9).
# Na PC <algorithm> przychodzi pośrednio, na ESP32-S3 nie: `pio run -e e1001` kończy się błędem `'shuffle' is not a
# member of 'std'` i `'stable_sort' …`. Żadna bramka nie kompiluje firmware na panel.
# poprawione w 5c9d0d6
def test_include_w_komentarzu():
    assert include_w_komentarzu() == []


@pytest.mark.skipif(os.environ.get("CPAP_STYKI_PIO") != "1" or not shutil.which("pio"), reason="CPAP_STYKI_PIO=1 i pio")
def test_kompilacja_esp32():
    with tempfile.TemporaryDirectory() as t:
        shutil.copytree(os.path.join(REPO, "firmware"), os.path.join(t, "firmware"),
                        ignore=shutil.ignore_patterns(".pio"))
        proto = os.path.join(t, "firmware", "prototyp")
        shutil.copy(os.path.join(proto, "include", "secrets.example.h"), os.path.join(proto, "include", "secrets.h"))
        r = subprocess.run(["pio", "run", "-e", "e1001"], cwd=proto, capture_output=True, text=True)
        assert r.returncode == 0, r.stdout[-2000:]


# ---------- trzyma ----------
def test_teksty_katalogu_bez_wartosci_do_ustawienia():
    """ADR-0002: teksty katalogu nie mówią, na ile ustawić ani jak zmienić (lista kierunku 9.3 w buduj_zestaw.py
    sprawdza słowa; tu drugi, prostszy sito: żadnego czasownika zmiany przy ustawieniu i żadnej ścieżki AirBridge)."""
    zakazane = re.compile(r"(podnie[śs]|obniż|zwiększ|zmniejsz|ustaw\s+na|zmień\s+(ciśn|EPAP|EPR|tryb)|/api/|menu)",
                          re.I)
    with open(os.path.join(KATALOG, "teksty.csv"), encoding="utf-8") as f:
        zle = [r for r in csv.reader(f, delimiter=";") if len(r) >= 4 and zakazane.search(r[3])]
    assert zle == []


def main():
    znaleziska = []
    for nr, zle in (("odwołanie bez celu", odwolania_bez_celu()), ("include w komentarzu", include_w_komentarzu())):
        for z in zle:
            print(f"BŁĄD {nr}: {z}")
        znaleziska += zle
    print(f"test_styki_modulow_brzegowe: {len(znaleziska)} błędów")
    return 1 if znaleziska else 0


if __name__ == "__main__":
    sys.exit(main())
