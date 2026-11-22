#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Test tekstów „Dobry start” pod ADR-0002 z wyjątkiem ADR-0050 (opcje komfortu).

Błąd (ADR-0002, wszędzie): liczba albo poziom ustawienia aparatu, droga w menu aparatu albo AirBridge,
polecenie dotyczące ciśnienia, zakresu, trybu albo poziomu EPR, diagnoza, lek z nazwy, słowo „przeciek”.
Dozwolone (ADR-0050, tylko w poradach programu): kierunek dla nawilżania, temperatury podgrzewanej rury
i rampy („więcej nawilżania”, „cieplejsza rura”, „skróć albo wyłącz rampę”) bez liczb i bez drogi w menu;
ulga na wydechu (EPR) tylko opisana, z odesłaniem do lekarza albo dostawcy.

Świadome wyjątki: adr2_wyjatki.csv (id;regula;trafienie;uzasadnienie). Trafienie z wyjątku nie jest błędem;
wyjątek, który niczego nie trafia, jest błędem (lista ma być aktualna).
Wynik: kod wyjścia 0 (brak błędów) albo 1. Przed testem treści skrypt sprawdza się na przykładach (samotest).

Wzorce i wyjątki są tu, przy budowie zestawu (tools/zestaw/porady.py używa trafienia(), samotest() i wyjątków), więc
budowa działa bez materiałów źródłowych. Uruchomiony jako skrypt sprawdza treści z `docs/spec/katalog/porady.csv`
(znaczniki `{prog:…}` z wartościami z `docs/spec/progi.csv`, jak przy budowie zestawu).
"""
import csv, os, re, sys

CEL = r"(ciśnien\w*|zakres\w*|\btryb\w*|\bEPR\b|ulg\w* na wydech\w*|ulg\w* wydechow\w*)"
ROZKAZ = (r"\b(ustaw|zmień|podnieś|obniż|zwiększ|zmniejsz|włącz|wyłącz|przełącz|skręć|podkręć|skróć|wydłuż"
          r"|ustawisz|zmienisz|podniesiesz|obniżysz|zwiększysz|zmniejszysz|włączysz|wyłączysz)\b"
          r"|\b(możesz|można|warto|spróbuj|trzeba|należy|wolno)\s+(sam\w*\s+)?"
          r"(ustawić|ustawiać|zmienić|zmieniać|podnieść|podnosić|obniżyć|obniżać|zwiększyć|zwiększać"
          r"|zmniejszyć|zmniejszać|włączyć|wyłączyć|przełączyć)\b")
POLECENIE = "polecenie: ciśnienie, zakres, tryb, EPR"
REG = [
    ("liczba ustawienia", r"\d+([,.]\d+)?\s*(cm\s?H2O|hPa|°C|stopni\w*)|\d+\s*%\s*(wilgot\w*|nawilż\w*)"
                          r"|(nawilż\w*|temperatur\w* (podgrzewanej )?rur\w*|ramp\w*|ciśnien\w*|\bEPR\b|ulg\w*)[^.!?\d]{0,25}\d"
                          r"|\bpoziom\w*\s+\d|\b(na|do)\s+poziom\w*\s+\d"),
    ("poziom EPR", r"\b(poziom\w*|stopie\w*|wartoś\w*)\s+(EPR|ulgi)\b|\bEPR\s*\d"),
    ("droga w menu", r"\b(menu|Moje opcje|My Options|pokrętł\w*|AirBridge|ustawieni\w* klinicz\w*|tryb\w* klinicz\w*"
                     r"|przytrzymaj\w*\s+(dwa|oba))\b|w aparacie ustaw"),
    (POLECENIE, None),  # w jednym zdaniu: rozkaz albo „możesz zmienić” i cel (ciśnienie, zakres, tryb, EPR)
    ("diagnoza", r"\b(masz|cierpisz na|chorujesz na)\s+(\w+\s+)?(bezdech\w*|chorob\w*|niewydoln\w*|astm\w*|zapaleni\w*"
                 r"|zaburzeni\w*|nadciśnieni\w*|depresj\w*)|\bto (jest )?(objaw|oznaka)\w* \w+|oznacza (chorob|niewydoln)\w*"
                 r"|\b(rozpoznanie|diagnoza)\s*:|\bzdiagnozuj\w*"),
    ("lek z nazwy", r"\b(\d+\s*mg|tabletk\w*|melatonin\w*|ksylometazolin\w*|oksymetazolin\w*|steryd\w*|ibuprofen\w*"
                    r"|paracetamol\w*|omeprazol\w*|eszopiklon\w*|zolpidem\w*|IPP)\b"),
    ("słowo „przeciek”", r"\bprzeciek(u|iem|i|ów|owi|ach|ami)?\b"),
]


def zdania(t):
    return [z for z in re.split(r"(?<=[.!?])\s+|\s*→\s*", t) if z.strip()]


def trafienia(tekst, podstaw=None):
    """Trafienia wzorców w tekście; `podstaw` (opcjonalnie) zamienia znaczniki `{prog:…}` przed testem."""
    if podstaw:
        tekst = podstaw(tekst)
    wynik = []
    for nazwa, rx in REG:
        if rx is None:
            for z in zdania(tekst):
                if re.search(ROZKAZ, z, re.I) and re.search(CEL, z, re.I):
                    wynik.append((nazwa, z.strip()))
            continue
        for m in re.finditer(rx, tekst, re.I):
            wynik.append((nazwa, m.group(0)))
    return wynik


ZLE = ["Ustaw EPR 2.", "Obniż EPR o 1.", "Ustaw minimum na 7 cmH2O.", "Zwiększ nawilżanie do poziomu 4.",
       "W menu Moje opcje wybierz rampę.", "Rampę ustaw na 20 minut.", "Możesz sam obniżyć ciśnienie.",
       "Zmień tryb na CPAP.", "Masz bezdech centralny.", "Weź melatoninę.", "Duży przeciek budzi.",
       "Podnieś zakres o jeden.", "Ustaw temperaturę rury na 27 °C.", "W AirBridge zmień ustawienia.",
       "Poziom EPR ustawia lekarz.", "Wyłącz ulgę na wydechu."]
DOBRE = ["Zwiększ trochę nawilżanie w aparacie.", "Woda w rurze: mniej nawilżania albo cieplejsza rura.",
         "Spróbuj ją skrócić albo wyłączyć.", "Ulga na wydechu (EPR) obniża ciśnienie przy wydechu.",
         "Zwykle włącza ją lekarz albo dostawca.", "Ciśnienie i tryb pracy aparatu ustawia lekarz.",
         "Twój aparat sam zmienia ciśnienie w zakresie ustawionym przez lekarza.", "Maska wciąż przecieka.",
         "Jak zmienić nawilżanie, pokaże instrukcja aparatu.", "Raport dla lekarza: przytrzymaj tu ▶ przez 2 s.",
         "Długość rampy ustawisz samodzielnie.", "Przy katarze i przeziębieniu nie przerywaj terapii."]


def samotest():
    bledy = [f"nie złapał: {z}" for z in ZLE if not trafienia(z)]
    bledy += [f"fałszywe trafienie: {d} → {trafienia(d)}" for d in DOBRE if trafienia(d)]
    return bledy


def porady_z_katalogu():
    """Porady z docs/spec/katalog/porady.csv i funkcja podstawiania progów (tools/zestaw/porady.py)."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import porady as pr  # noqa: E402
    katalog = os.path.join(pr.REPO, "docs", "spec", "katalog")
    with open(os.path.join(katalog, "porady.csv"), encoding="utf-8", newline="") as f:
        wiersze = list(csv.DictReader(f, delimiter=";"))
    progi = pr.wczytaj_progi()
    return wiersze, lambda t: pr.podstaw_progi(t, progi)


def main(grupy=None, podstaw=None):
    """`grupy`: listy porad (słowniki z `id`, `tytul`, `tresc`, `krok`); bez nich porady z katalogu."""
    if grupy is None:
        wiersze, podstaw = porady_z_katalogu()
        grupy = [wiersze]
    st = samotest()
    if st:
        print("samotest: BŁĄD")
        for b in st:
            print("  ", b)
        return 1
    sciezka = os.path.join(os.path.dirname(os.path.abspath(__file__)), "adr2_wyjatki.csv")
    wyj = {(w["id"], w["regula"], w["trafienie"]): w["uzasadnienie"]
           for w in csv.DictReader(open(sciezka, encoding="utf-8"), delimiter=";")}
    uzyte, bledy, n = set(), [], 0
    for grupa in grupy:
        for p in grupa:
            for pole in ("tytul", "tresc", "krok"):
                for nazwa, frag in trafienia(p.get(pole, ""), podstaw):
                    n += 1
                    k = (p["id"], nazwa, frag)
                    if k in wyj:
                        uzyte.add(k)
                        print(f"wyjątek  {p['id']:14} {nazwa:40} «{frag}»")
                    else:
                        bledy.append(k)
                        print(f"BŁĄD     {p['id']:14} {nazwa:40} «{frag}»")
    for k in sorted(set(wyj) - uzyte):
        bledy.append(k)
        print(f"BŁĄD     wyjątek nieużywany: {k}")
    print(f"samotest: {len(ZLE)} złych i {len(DOBRE)} dobrych przykładów OK; trafień: {n}, "
          f"wyjątków: {len(uzyte)}, błędów: {len(bledy)}")
    return 1 if bledy else 0


if __name__ == "__main__":
    sys.exit(main())
