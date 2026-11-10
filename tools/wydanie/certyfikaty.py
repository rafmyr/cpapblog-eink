#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Certyfikaty główne serwerów GitHuba wkompilowane w firmware (ADR-0032 pkt 5).

Wyciąga z pakietu certyfikatów Mozilli (certifi albo plik podany w --pakiet) certyfikaty główne, którymi GitHub
podpisuje api.github.com i serwery plików wydań, i pisze firmware/prototyp/src/certyfikaty_github.h.

Użycie:
  python3 tools/wydanie/certyfikaty.py                 # pakiet z certifi, zapis do src/certyfikaty_github.h
  python3 tools/wydanie/certyfikaty.py --sprawdz       # tylko porównanie z plikiem w repo (kod 1 przy różnicy)
"""
import argparse
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CEL = os.path.join(REPO, "firmware", "prototyp", "src", "certyfikaty_github.h")
# api.github.com: Sectigo → USERTrust; serwery plików (objects / release-assets.githubusercontent.com): DigiCert.
# Zapas: nowsze korzenie obu wystawców, żeby zmiana pośredniego certyfikatu po stronie GitHuba nie wyłączyła aktualizacji.
ETYKIETY = [
    "USERTrust ECC Certification Authority",
    "USERTrust RSA Certification Authority",
    "Sectigo Public Server Authentication Root E46",
    "Sectigo Public Server Authentication Root R46",
    "DigiCert Global Root G2",
    "DigiCert Global Root G3",
]


def wytnij(pakiet, etykiety):
    """Słownik etykieta → PEM z pakietu w formacie certifi (`# Label: "…"` przed blokiem PEM)."""
    wynik, etykieta, blok = {}, None, None
    for linia in pakiet.splitlines():
        if linia.startswith("# Label: "):
            etykieta = linia[len("# Label: "):].strip().strip('"')
        elif linia.startswith("-----BEGIN CERTIFICATE-----"):
            blok = [linia]
        elif blok is not None:
            blok.append(linia)
            if linia.startswith("-----END CERTIFICATE-----"):
                if etykieta in etykiety:
                    wynik[etykieta] = "\n".join(blok) + "\n"
                blok = None
    brak = [e for e in etykiety if e not in wynik]
    if brak:
        raise ValueError("brak w pakiecie: " + ", ".join(brak))
    return wynik


# REUSE-IgnoreStart (znacznik SPDX jako tekst w kodzie, nie licencja tego pliku)
def naglowek(pemy):
    w = ["// SPDX-License-Identifier: MPL-2.0",
         "// Plik generowany: tools/wydanie/certyfikaty.py (pakiet certyfikatów Mozilli). Nie edytować ręcznie.",
         "// Certyfikaty główne serwerów GitHuba do weryfikacji HTTPS przy aktualizacji (ADR-0032 pkt 5, R-NF-32).",
         "#pragma once", "", "static const char CERTYFIKATY_GITHUB[] ="]
    # REUSE-IgnoreEnd
    for e in ETYKIETY:
        w.append("    // " + e)
        for l in pemy[e].splitlines():
            w.append('    "' + l + '\\n"')
    w[-1] += ";"
    return "\n".join(w) + "\n"


def main(argv=None):
    a = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    a.add_argument("--pakiet", help="plik PEM z etykietami (domyślnie certifi)")
    a.add_argument("--sprawdz", action="store_true")
    o = a.parse_args(argv)
    sciezka = o.pakiet
    if not sciezka:
        import certifi
        sciezka = certifi.where()
    with open(sciezka, encoding="utf-8") as f:
        tresc = naglowek(wytnij(f.read(), ETYKIETY))
    if o.sprawdz:
        with open(CEL, encoding="utf-8") as f:
            return 0 if f.read() == tresc else 1
    with open(CEL, "w", encoding="utf-8") as f:
        f.write(tresc)
    print("zapisano", os.path.relpath(CEL, REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main())
