#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Test tekstów „Dobry start” pod ADR-0002 z wyjątkami ADR-0050 (opcje komfortu) i ADR-0051 (menu pacjenta, odczucia).

Błąd (ADR-0002, wszędzie): liczba albo poziom ustawienia aparatu, droga w menu aparatu albo AirBridge,
polecenie dotyczące ciśnienia, zakresu, trybu albo poziomu EPR, diagnoza, lek z nazwy, słowo „przeciek”.
Dozwolone (ADR-0050, tylko w poradach programu): kierunek dla nawilżania, temperatury podgrzewanej rury
i rampy („więcej nawilżania”, „cieplejsza rura”, „skróć albo wyłącz rampę”) bez liczb i bez drogi w menu;
ulga na wydechu (EPR) tylko opisana, z odesłaniem do lekarza albo dostawcy.
Od ADR-0051 (22.11.2026, tylko porady programu): droga w menu pacjenta („Moje opcje” i nazwy jego pozycji) dla opcji
komfortu; ulga wydechowa: opis poziomów bez liczby i „możesz ją włączać i zmieniać”, gdy tekst mówi o Moich opcjach,
bez kierunku („zwiększ”, „obniż”); objaśnienie odczucia „za niskie / za wysokie ciśnienie” tylko z lekarzem w tym
samym tekście. Dalej błąd: liczby ustawień, menu kliniczne albo serwisowe, AirBridge, polecenie dotyczące ciśnienia,
zakresu, trybu albo kierunku ulgi wydechowej, diagnoza, lek z nazwy, słowa z rdzenia „przeciek”.

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
          r"|\b(możesz|można|warto|spróbuj|trzeba|należy|wolno)\s+((ją|je|go|ich|sam\w*|samodzielnie|też|także)\s+){0,2}"
          r"(ustawić|ustawiać|zmienić|zmieniać|podnieść|podnosić|obniżyć|obniżać|zwiększyć|zwiększać"
          r"|zmniejszyć|zmniejszać|włączyć|włączać|wyłączyć|wyłączać|przełączyć)\b")
# ADR-0051 pkt B: ulga wydechowa w Moich opcjach (włączać, zmieniać), bez kierunku
CEL_ULGA = r"(\bEPR\b|ulg\w* na wydech\w*|ulg\w* wydechow\w*)"
CEL_INNE = r"(ciśnien\w*|zakres\w*|\btryb\w*)"
MOJE_OPCJE = r"\b(Moich opcjach|Moje opcje|Moich opcji)\b"
KIERUNEK = (r"\b(zwiększ\w*|zmniejsz\w*|podnie\w*|podnosi\w*|obniż\w*|wyżej|niżej|wyższ\w*|niższ\w*|maksim\w*"
            r"|minim\w*|mocniej|słabiej|wyłącz\w*)\b")
# ADR-0051 pkt A: „za niskie / za wysokie ciśnienie” jako objaśnienie odczucia, zawsze z lekarzem w tym samym tekście
ODCZUCIE = r"\bza (nisk|wysok)\w*\s+ciśnien\w*"
POLECENIE = "polecenie: ciśnienie, zakres, tryb, EPR"
REG = [
    ("liczba ustawienia", r"\d+([,.]\d+)?\s*(cm\s?H2O|hPa|°C|stopni\w*)|\d+\s*%\s*(wilgot\w*|nawilż\w*)"
                          r"|(nawilż\w*|temperatur\w* (podgrzewanej )?rur\w*|ramp\w*|czas\w* narastania|poziom\w* wilg\w*|ciśnien\w*"
                          r"|\bEPR\b|ulg\w*)[^.!?\d]{0,25}\d"
                          r"|\bpoziom\w*\s+\d|\b(na|do)\s+poziom\w*\s+\d"),
    ("poziom EPR", r"\bEPR\s*\d|\b(poziom\w*|stopie\w*|wartoś\w*)\s+(EPR|ulgi)\s*(na\s+)?\d"),
    ("droga w menu", r"\b(menu\w*|My Options|pokrętł\w*|AirBridge|ustawieni\w* (klinicz|serwis|zaawansowan)\w*"
                     r"|tryb\w* (klinicz|serwis)\w*|przytrzymaj\w*\s+(dwa|oba))\b|w aparacie ustaw"),
    ("kierunek ciśnienia bez lekarza", None),  # ADR-0051 pkt A
    (POLECENIE, None),  # w jednym zdaniu: rozkaz albo „możesz zmienić” i cel (ciśnienie, zakres, tryb, EPR)
    ("diagnoza", r"\b(masz|cierpisz na|chorujesz na)\s+(\w+\s+)?(bezdech\w*|chorob\w*|niewydoln\w*|astm\w*|zapaleni\w*"
                 r"|zaburzeni\w*|nadciśnieni\w*|depresj\w*)|\bto (jest )?(objaw|oznaka)\w* \w+|oznacza (chorob|niewydoln)\w*"
                 r"|\b(rozpoznanie|diagnoza)\s*:|\bzdiagnozuj\w*"),
    ("lek z nazwy", r"\b(\d+\s*mg|tabletk\w*|melatonin\w*|ksylometazolin\w*|oksymetazolin\w*|steryd\w*|ibuprofen\w*"
                    r"|paracetamol\w*|omeprazol\w*|eszopiklon\w*|zolpidem\w*|IPP)\b"),
    ("słowo „przeciek”", r"\bprzeciek\w*"),
]


def zdania(t):
    return [z for z in re.split(r"(?<=[.!?])\s+|\s*→\s*", t) if z.strip()]


def ulga_w_moich_opcjach(z, tekst):
    """ADR-0051 pkt B: „możesz ją włączać i zmieniać” dla ulgi wydechowej, gdy tekst mówi o Moich opcjach, bez kierunku."""
    return (re.search(CEL_ULGA, z, re.I) and not re.search(CEL_INNE, z, re.I) and re.search(MOJE_OPCJE, tekst)
            and not re.search(KIERUNEK, z, re.I))


def trafienia(tekst, podstaw=None):
    """Trafienia wzorców w tekście; `podstaw` (opcjonalnie) zamienia znaczniki `{prog:…}` przed testem."""
    if podstaw:
        tekst = podstaw(tekst)
    wynik = []
    for nazwa, rx in REG:
        if rx is None and nazwa == POLECENIE:
            for z in zdania(tekst):
                if re.search(ROZKAZ, z, re.I) and re.search(CEL, z, re.I) and not ulga_w_moich_opcjach(z, tekst):
                    wynik.append((nazwa, z.strip()))
            continue
        if rx is None:  # kierunek ciśnienia bez lekarza (ADR-0051 pkt A)
            m = re.search(ODCZUCIE, tekst, re.I)
            if m and not re.search(r"\blekarz\w*", tekst, re.I):
                wynik.append((nazwa, m.group(0)))
            continue
        for m in re.finditer(rx, tekst, re.I):
            wynik.append((nazwa, m.group(0)))
    return wynik


ZLE = ["Ustaw EPR 2.", "Obniż EPR o 1.", "Ustaw minimum na 7 cmH2O.", "Zwiększ nawilżanie do poziomu 4.",
       "W menu Moje opcje wybierz rampę.", "Rampę ustaw na 20 minut.", "Możesz sam obniżyć ciśnienie.",
       "Zmień tryb na CPAP.", "Masz bezdech centralny.", "Weź melatoninę.", "Duży przeciek budzi.",
       "Podnieś zakres o jeden.", "Ustaw temperaturę rury na 27 °C.", "W AirBridge zmień ustawienia.",
       "Wyłącz ulgę na wydechu.", "Duży wyciek: maska przecieka.",
       # ADR-0051: kierunek ulgi, menu kliniczne, liczby, odczucie bez lekarza, polecenie zmiany ciśnienia
       "Zwiększ ulgę wydechową w Moich opcjach.", "W Moich opcjach możesz sam obniżyć ulgę wydechową.",
       "Ustaw EPR na 3 w Moich opcjach.", "Poziom ulgi na 2 wystarczy.", "Wejdź do menu klinicznego.",
       "W ustawieniach klinicznych zmień tryb.", "Mało powietrza? To za niskie ciśnienie.",
       "Za mało powietrza? Podnieś ciśnienie w Moich opcjach.", "Możesz je zmienić: ciśnienie jest za niskie.",
       "Czas narastania ustaw na 20 minut.", "W Moich opcjach możesz włączyć ulgę wydechową na maksimum."]
DOBRE = ["Zwiększ trochę nawilżanie w aparacie.", "Woda w rurze: mniej nawilżania albo cieplejsza rura.",
         "Spróbuj ją skrócić albo wyłączyć.", "Ulga na wydechu (EPR) obniża ciśnienie przy wydechu.",
         "Zwykle włącza ją lekarz albo dostawca.", "Ciśnienie i tryb pracy aparatu ustawia lekarz.",
         "Twój aparat sam zmienia ciśnienie w zakresie ustawionym przez lekarza.",
         "Jak zmienić nawilżanie, pokaże instrukcja aparatu.", "Raport dla lekarza: przytrzymaj tu ▶ przez 2 s.",
         "Długość rampy ustawisz samodzielnie.", "Przy katarze i przeziębieniu nie przerywaj terapii.",
         "Maska wciąż nieszczelna.", "Poziom EPR ustawia lekarz.",
         # ADR-0051
         "W Moich opcjach podnieś trochę „Poziom wilg.”.", "Znajdziesz ją w Moich opcjach jako „Czas narastania”.",
         "Ulga wydechowa ma kilka poziomów: wyższy daje większą ulgę.",
         "Widzisz ulgę wydechową w Moich opcjach? Możesz ją włączać i zmieniać samodzielnie.",
         "W Moich opcjach możesz włączyć ulgę wydechową.",
         "Ciężko zrobić wydech? To może być za wysokie ciśnienie. Powiedz to lekarzowi tymi słowami.",
         "Nie ma jej? Poproś lekarza albo dostawcę.", "Ciśnienie zmienia tylko lekarz."]


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
