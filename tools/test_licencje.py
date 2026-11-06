# SPDX-License-Identifier: GPL-3.0-or-later
"""Nagłówki licencji: SPDX w plikach źródłowych, testach i narzędziach, nagłówki plików generowanych zgodne
z generatorami, teksty licencji w LICENSES/ dla każdego użytego identyfikatora (REUSE, REUSE.toml).

Uruchomienie: python3 -m pytest -q tools/test_licencje.py
"""
import importlib.util
import os
import re
import subprocess

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TU)
# REUSE-IgnoreStart (znacznik SPDX jako tekst w kodzie, nie licencja tego pliku)
GPL = "SPDX-License-Identifier: GPL-3.0-or-later"
# REUSE-IgnoreEnd
GENEROWANE = {
    "firmware/prototyp/include/fonty.h": "LICENCJA_FONTY",
    "firmware/prototyp/include/fonty_slot.h": "LICENCJA_FONTY_SLOT",
    "firmware/prototyp/include/obrazy.h": "LICENCJA_OBRAZY",
}
# Pliki kodu z licencją inną niż sama GPL-3.0-or-later (NOTICE): wyrażenie SPDX w pierwszej linii.
INNA_LICENCJA = {
    "firmware/prototyp/src/qr.cpp": "GPL-3.0-or-later AND MIT",  # według Project Nayuki
    "firmware/prototyp/src/qr.h": "GPL-3.0-or-later AND MIT",
    "firmware/prototyp/test/atrapy/gfxfont.h": "BSD-2-Clause",  # definicje struktur Adafruit GFX
}
KOD = (".cpp", ".h", ".c", ".py", ".sh", ".js")


def generuj():
    spec = importlib.util.spec_from_file_location("generuj_lic", os.path.join(TU, "prototyp", "generuj.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def sledzone(*katalogi):
    w = subprocess.run(["git", "-C", REPO, "ls-files", *katalogi], stdout=subprocess.PIPE, text=True)
    if w.returncode != 0:  # bez gita: wszystkie pliki z dysku
        wynik = []
        for k in katalogi:
            for d, _, pliki in os.walk(os.path.join(REPO, k)):
                wynik += [os.path.relpath(os.path.join(d, p), REPO) for p in pliki]
        return wynik
    return w.stdout.split()


def pierwsze_linie(sciezka, n=3):
    with open(os.path.join(REPO, sciezka), encoding="utf-8") as f:
        return [next(f, "").rstrip("\n") for _ in range(n)]


def linia_spdx(p):
    """Pierwsza linia pliku kodu po shebangu, bez znaku komentarza."""
    linie = pierwsze_linie(p, 2)
    linia = linie[1] if linie[0].startswith("#!") else linie[0]
    return re.sub(r"^(//|#)\s?", "", linia)


def test_firmware_i_testy_maja_spdx():
    """Każdy plik kodu w firmware/ (źródła, testy, atrapy, skrypty PlatformIO) ma SPDX w pierwszej linii."""
    braki = []
    for p in sledzone("firmware"):
        if not p.endswith(KOD) or p in GENEROWANE:
            continue
        oczekiwane = GPL.replace("GPL-3.0-or-later", INNA_LICENCJA.get(p, "GPL-3.0-or-later"))
        if linia_spdx(p) != oczekiwane:
            braki.append(p)
    assert braki == [], "bez właściwego SPDX w pierwszej linii (po shebangu): %s" % braki


def test_wyjatki_licencji_istnieja():
    for p in INNA_LICENCJA:
        assert os.path.exists(os.path.join(REPO, p)), p


def test_licencje_maja_tekst_w_LICENSES():
    """Każdy identyfikator z nagłówków SPDX i z REUSE.toml ma plik LICENSES/{id}.txt (REUSE)."""
    uzyte = set()
    for p in sledzone("firmware", "tools") + ["REUSE.toml"]:
        if not (p.endswith(KOD) or p == "REUSE.toml"):
            continue
        with open(os.path.join(REPO, p), encoding="utf-8") as f:
            tekst = f.read()
        tekst = re.sub(r"REUSE-IgnoreStart.*?REUSE-IgnoreEnd", "", tekst, flags=re.S)
        for wyr in re.findall(r"^\s*(?://|#)?\s*SPDX-License-Identifier\s*[:=]\s*\"?([A-Za-z0-9.+\- ()]+?)\"?\s*$",
                              tekst, flags=re.M):
            uzyte |= {t for t in re.split(r"\s+(?:AND|OR|WITH)\s+|[()]", wyr.strip()) if t}
    brak = sorted(i for i in uzyte if not os.path.exists(os.path.join(REPO, "LICENSES", i + ".txt")))
    assert brak == [], "brak tekstu licencji w LICENSES/: %s" % brak
    assert {"GPL-3.0-or-later", "OFL-1.1", "LicenseRef-cpapblog-eink-budzik"} <= uzyte


def test_reuse_toml_obejmuje_grafiki_budzika():
    with open(os.path.join(REPO, "REUSE.toml"), encoding="utf-8") as f:
        t = f.read()
    assert 'path = "**"' in t and 'path = "assets/budzik/**"' in t
    assert 'SPDX-License-Identifier = "LicenseRef-cpapblog-eink-budzik"' in t


def test_narzedzia_maja_spdx_po_shebangu():
    braki = []
    for p in sledzone("tools"):
        if not p.endswith((".py", ".sh")):
            continue
        linie = pierwsze_linie(p, 2)
        oczekiwana = linie[1] if linie[0].startswith("#!") else linie[0]
        if oczekiwana != "# " + GPL:
            braki.append(p)
    assert braki == [], "bez `# %s` (po shebangu): %s" % (GPL, braki)


def test_naglowki_generowanych_jak_w_generatorach():
    g = generuj()
    for plik, stala in GENEROWANE.items():
        naglowek = getattr(g, stala).split("\n")
        assert pierwsze_linie(plik, len(naglowek)) == naglowek, plik


def test_zestaw_wbudowany_gpl():
    assert pierwsze_linie("firmware/prototyp/src/zestaw_wbudowany.h", 1)[0] == "// " + GPL
