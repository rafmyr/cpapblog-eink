# SPDX-License-Identifier: GPL-3.0-or-later
"""Testy skryptu budowy zestawu reguł tools/zestaw/buduj_zestaw.py (spec techniczna 9.3, kontrakt 2).

Uruchomienie: python3 -m pytest -q tools/test_buduj_zestaw.py
"""

import hashlib
import os
import re
import subprocess
import sys

import pytest

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TU)
SKRYPT = os.path.join(TU, "zestaw", "buduj_zestaw.py")
sys.path.insert(0, os.path.join(TU, "zestaw"))
import buduj_zestaw  # noqa: E402

POLA = ("id_reguly;wariant;rodzaj;parametry;liczone_noce;okno;poziom;grupa;slot;tryby;wymaga_normy;w_rozruchu;"
        "wyklucza;wymaga;bramki;limit;teksty;stan;pilnosc;zrodlo")
SPEC = """# spec testowa
| E-30 | ... | `Noce z AHI powyżej 5` / `AHI powyżej 5 w {n} z 7 nocy, ostatnio {x}.` / `→ Sprawdź wyciek i obserwuj kolejne noce.` |
Zdanie z liczbą: `Mediana 12 nocy.`
"""
PROGI = """id_progu,regula,miejsce,wartosc,jednostka,opis,zrodlo,pochodzenie,tryb,status
E-30.ahi,E-30,,5,/h,opis,brak,spec,wszystkie,do kalibracji
E-30.okno,E-30,,7,noce,opis,brak,spec,wszystkie,do kalibracji
E-30.ref,E-30,,=E-30.ahi,/h,opis,brak,spec,ASV;ASVAuto,do kalibracji
X.dwanascie,X,,12,noce,opis,brak,spec,wszystkie,do kalibracji
"""
E30 = ("E-30;;statystyka_okna;metryka=M-02,stat=mediana,prog=E-30.ahi,por=ge,n=E-30.okno;wazne_bez_niepewnych;"
       "E-30.okno;problem;R5b;rano;ASV,ASVAuto;nie;tak;E-31;;;;E-30.tytul,E-30.powod,E-30.krok;aktywna;;6.4")
TEKSTY = """id_tekstu;rola;limit;tekst
E-30.tytul;tytul;30;Noce z AHI powyżej {prog:E-30.ahi}
E-30.powod;powod;70;AHI powyżej {prog:E-30.ahi} w {n} z {prog:E-30.okno} nocy, ostatnio {x}.
E-30.krok;krok;;→ Sprawdź wyciek i obserwuj kolejne noce.
"""


def przygotuj(tmp, katalog_e=(E30,), teksty=TEKSTY, progi=PROGI, spec=SPEC):
    kat = tmp / "katalog"
    kat.mkdir(exist_ok=True)
    (kat / "E.csv").write_text(POLA + "\n" + "\n".join(katalog_e) + "\n", encoding="utf-8")
    (kat / "L.csv").write_text(POLA + "\n", encoding="utf-8")
    (kat / "teksty.csv").write_text(teksty, encoding="utf-8")
    (tmp / "progi.csv").write_text(progi, encoding="utf-8")
    (tmp / "spec.md").write_text(spec, encoding="utf-8")
    return ["--progi", str(tmp / "progi.csv"), "--katalog", str(kat), "--spec", str(tmp / "spec.md"),
            "--h", str(tmp / "zestaw.h"), "--wyjscie", str(tmp / "wyj"), "--zrodlo", "abc123"]


def uruchom(argv):
    r = subprocess.run([sys.executable, SKRYPT] + argv, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def test_buduje_trzy_pliki_ze_skrotem(tmp_path):
    kod, out = uruchom(przygotuj(tmp_path))
    assert kod == 0, out
    ident = set()
    for nazwa, rodzaj in (("progi.csv", "progi"), ("katalog.csv", "katalog"), ("teksty.csv", "teksty")):
        t = (tmp_path / "wyj" / nazwa).read_text(encoding="utf-8")
        pierwszy, cialo = t.split("\n", 1)
        assert pierwszy.startswith("#cpap-eink %s 1 zestaw=" % rodzaj)
        assert "pochodzenie=wbudowany firmware=1.0.0..1.99.99 tryby=CPAP,AutoSet,ASV,ASVAuto zamrozenie=brak" in pierwszy
        assert "zrodlo=abc123" in pierwszy
        m = re.search(r"skrot=sha256:([0-9a-f]{64})$", pierwszy)
        assert m and m.group(1) == hashlib.sha256(cialo.encode("utf-8")).hexdigest()
        ident.add(re.search(r"zestaw=(\S+)", pierwszy).group(1))
    assert len(ident) == 1
    progi = (tmp_path / "wyj" / "progi.csv").read_text(encoding="utf-8")
    assert "\nE-30.ref;5;/h;ASV,ASVAuto\n" in progi  # odwołanie rozwiązane, `;` w trybie zamienione na `,`
    teksty = (tmp_path / "wyj" / "teksty.csv").read_text(encoding="utf-8")
    assert "\nE-30.krok;krok;60;" in teksty  # limit z roli (5.4.4)
    h = (tmp_path / "zestaw.h").read_text(encoding="utf-8")
    assert 'static const char PROGI[] = R"zestaw(#cpap-eink progi 1' in h and "KATALOG" in h and "TEKSTY" in h


def test_ten_sam_wynik_z_tych_samych_zrodel(tmp_path):
    a = przygotuj(tmp_path)
    assert uruchom(a)[0] == 0
    p1 = (tmp_path / "zestaw.h").read_text(encoding="utf-8")
    assert uruchom(a)[0] == 0
    assert (tmp_path / "zestaw.h").read_text(encoding="utf-8") == p1


@pytest.mark.parametrize("zmiana,oczekiwany", [
    (lambda e, t: ([e[0].replace("prog=E-30.ahi", "prog=E-30.nie_ma")], t), "E-30.nie_ma"),
    (lambda e, t: ([e[0].replace(";E-30.okno;problem", ";E-30.brak;problem")], t), "E-30.brak"),
    (lambda e, t: ([e[0].replace("statystyka_okna", "wymyslony_rodzaj")], t), "nieznany rodzaj"),
    (lambda e, t: ([e[0].replace("stat=mediana", "stat=mediana,nowy=x")], t), "spoza kontraktu 6"),
    (lambda e, t: ([e[0].replace("E-30.krok;aktywna", "E-30.krok,E-30.zaginiony;aktywna")], t), "E-30.zaginiony"),
    (lambda e, t: (e, t.replace("obserwuj kolejne noce", "obniż EPR o 1")), "nie stoi dosłownie w spec"),
    (lambda e, t: (e, t.replace("{prog:E-30.ahi} w", "5 w")), "przez {prog:"),
    (lambda e, t: (e, t.replace("tytul;30;", "tytul;10;")), "limit 10"),
    (lambda e, t: ([e[0].replace(";aktywna;", ";wlaczona;")], t), "stan"),
    (lambda e, t: ([e[0].replace("wazne_bez_niepewnych", "noce_dowolne")], t), "liczone_noce"),
    (lambda e, t: ([e[0].replace("stat=mediana", "stat=7")], t), "liczba w katalogu"),
])
def test_bledy_zatrzymuja_budowe(tmp_path, zmiana, oczekiwany):
    e, t = zmiana([E30], TEKSTY)
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=e, teksty=t))
    assert kod != 0
    assert "BŁĄD zestawu reguł" in out and oczekiwany in out, out
    assert not (tmp_path / "zestaw.h").exists()


def test_zmienna_pasuje_do_dowolnego_ciagu_i_cykl(tmp_path):
    # {zmienna} w tekście pasuje do dowolnego ciągu spec; zależność zlozenie przed użyciem; cykl = błąd
    a = "A;;prog_nocy;metryka=M-02,prog=E-30.ahi;;;;;;wszystkie;nie;tak;;;;;;aktywna;;t"
    b = "B;;zlozenie;op=nie,skladniki=C;;;;;;wszystkie;nie;tak;;;;;;aktywna;;t"
    c = "C;;zlozenie;op=nie,skladniki=A;;;;;;wszystkie;nie;tak;;;;;;aktywna;;t"
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=(E30, b, c, a)))
    assert kod == 0, out
    kat = (tmp_path / "wyj" / "katalog.csv").read_text(encoding="utf-8").split("\n")[2:]
    ids = [x.split(";")[0] for x in kat if x]
    assert ids.index("A") < ids.index("C") < ids.index("B")
    a2 = "A;;zlozenie;op=nie,skladniki=B;;;;;;wszystkie;nie;tak;;;;;;aktywna;;t"
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=(b, c, a2)))
    assert kod != 0 and "cykl" in out


def test_ostrzezenie_granicy_nie_blokuje(tmp_path):
    spec = SPEC + "\n`EPAP wyższe niż zwykle.`\n"
    t = TEKSTY + "E-36.linia;linia;;EPAP wyższe niż zwykle.\n"
    e = E30 + "\nE-36;;zawsze;;;;linia;;rano;wszystkie;nie;tak;;;;;E-36.linia;aktywna;;t"
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=(e,), teksty=t, spec=spec))
    assert kod == 0, out
    assert "OSTRZEŻENIE" in out and "EPAP" in out


def test_zestaw_wbudowany_w_repo_aktualny():
    """firmware/prototyp/src/zestaw_wbudowany.h odpowiada bieżącym źródłom (po zmianie źródeł: uruchom skrypt)."""
    h = os.path.join(REPO, "firmware", "prototyp", "src", "zestaw_wbudowany.h")
    with open(h, encoding="utf-8") as f:
        jest = f.read()
    zrodlo = re.search(r"zrodlo=(\S+)", jest).group(1)
    cial = buduj_zestaw.buduj(os.path.join(REPO, "docs", "spec", "progi.csv"),
                              os.path.join(REPO, "docs", "spec", "katalog"),
                              os.path.join(REPO, "docs", "spec", "specyfikacja-funkcjonalna.md"))
    zestaw = "z-" + hashlib.sha256("".join(cial).encode("utf-8")).hexdigest()[:12]
    pliki = buduj_zestaw.z_naglowkami(cial, zestaw, "wbudowany", "1.0.0", "1.99.99", "brak", zrodlo)
    assert buduj_zestaw.naglowek_cpp(pliki) == jest, "zestaw_wbudowany.h nieaktualny: python3 tools/zestaw/buduj_zestaw.py"


def test_prog_kilka_wierszy_trybow_i_odwolanie_tego_trybu(tmp_path):
    # (9.3): jeden id_progu z wierszami dla trybów; odwołanie `=id` bierze wiersz tego samego trybu
    progi = PROGI + ("X.tr,X,,4,/h,opis,brak,spec,ASVAuto,do kalibracji\n"
                     "X.tr,X,,6,/h,opis,brak,spec,wszystkie,do kalibracji\n"
                     "X.ref,X,,=X.tr,/h,opis,brak,spec,ASVAuto,do kalibracji\n")
    kod, out = uruchom(przygotuj(tmp_path, progi=progi))
    assert kod == 0, out
    p = (tmp_path / "wyj" / "progi.csv").read_text(encoding="utf-8")
    assert "\nX.tr;4;/h;ASVAuto\n" in p and "\nX.tr;6;/h;wszystkie\n" in p and "\nX.ref;4;/h;ASVAuto\n" in p
    progi2 = PROGI + ("X.tr,X,,4,/h,opis,brak,spec,ASVAuto,do kalibracji\n"
                      "X.tr,X,,5,/h,opis,brak,spec,ASV;ASVAuto,do kalibracji\n")
    kod, out = uruchom(przygotuj(tmp_path, progi=progi2))
    assert kod == 1 and "powtórzony id_progu" in out


def test_kzn_poza_zakresem_i_granica(tmp_path):
    progi = PROGI + "X.kzn,X,,0/7,noce (k z n),opis,brak,spec,wszystkie,do kalibracji\n"
    e = E30 + "\nX;;k_z_n_nocy;metryka=M-02,prog=E-30.ahi,kzn=X.kzn;;;;;;wszystkie;nie;tak;;;;;;aktywna;;t"
    kod, out = uruchom(przygotuj(tmp_path, progi=progi, katalog_e=(e,)))
    assert kod == 1 and "1 ≤ k ≤ n" in out
    # dozwolony opis zmierzonej wartości przechodzi, polecenie zmiany nie (także gdy stoi w spec)
    spec = SPEC + "\n`EPAP wyższe niż zwykle.` `Zwiększ EPAP o 1 cmH2O.`\n"
    t = TEKSTY + "X.l;linia;;Zwiększ EPAP o 1 cmH2O.\n"
    kod, out = uruchom(przygotuj(tmp_path, teksty=t, spec=spec))
    assert kod == 1 and "ADR-0002" in out


def test_liczby_przyklejone_i_inna_jednostka_nie_sa_progiem(tmp_path):
    progi = PROGI + "E-30.dwa,E-30,,2,cmH2O,opis,brak,spec,wszystkie,do kalibracji\n"
    e = E30.replace("n=E-30.okno", "n=E-30.okno,delta=E-30.dwa")
    spec = SPEC + "\n`Od 2 tygodni, zobacz K2 i M-07.`\n`Wzrost o 2 cmH2O.`\n"
    t = TEKSTY + "E-30.linia;linia;;Od 2 tygodni, zobacz K2 i M-07.\n"
    e2 = e.replace("E-30.krok;aktywna", "E-30.krok,E-30.linia;aktywna")
    kod, out = uruchom(przygotuj(tmp_path, progi=progi, katalog_e=(e2,), teksty=t, spec=spec))
    assert kod == 0, out
    t2 = TEKSTY + "E-30.linia;linia;;Wzrost o 2 cmH2O.\n"
    kod, out = uruchom(przygotuj(tmp_path, progi=progi, katalog_e=(e2,), teksty=t2, spec=spec))
    assert kod == 1 and "przez {prog:" in out


def test_numer_strony_kokpitu_nie_jest_progiem(tmp_path):
    # `raport 2 / 3` to numer strony kokpitu w linii pilności C (5.5.7), nie wartość progu 2; sama `2` dalej jest błędem
    progi = PROGI + "E-30.dwa,E-30,,2,noce,opis,brak,spec,wszystkie,do kalibracji\n"
    e = E30.replace("n=E-30.okno", "n=E-30.okno,delta=E-30.dwa")
    e2 = e.replace("E-30.krok;aktywna", "E-30.krok,E-30.linia;aktywna")
    spec = SPEC + "\n`Na kontroli: raport 2 / 3.`\n`Od 2 nocy.`\n`→ Otwarte sprawy: za stroną 2 / 3.`\n"
    t = TEKSTY + "E-30.linia;linia;;Na kontroli: raport 2 / 3.\n"
    kod, out = uruchom(przygotuj(tmp_path, progi=progi, katalog_e=(e2,), teksty=t, spec=spec))
    assert kod == 0, out
    t3 = TEKSTY + "E-30.linia;linia;;→ Otwarte sprawy: za stroną 2 / 3.\n"
    kod, out = uruchom(przygotuj(tmp_path, progi=progi, katalog_e=(e2,), teksty=t3, spec=spec))
    assert kod == 0, out
    t2 = TEKSTY + "E-30.linia;linia;;Od 2 nocy.\n"
    kod, out = uruchom(przygotuj(tmp_path, progi=progi, katalog_e=(e2,), teksty=t2, spec=spec))
    assert kod == 1 and "przez {prog:" in out


def test_stary_numer_strony_to_blad(tmp_path):
    # R-KOK-23 (ADR-0041): tekst zestawu z numerem `n / 6` sprzed krótszego kokpitu nie przechodzi budowy zestawu
    spec = SPEC + "\n`Na kontroli: raport 2 / 6.`\n"
    t = TEKSTY + "E-30.linia;linia;;Na kontroli: raport 2 / 6.\n"
    e2 = E30.replace("E-30.krok;aktywna", "E-30.krok,E-30.linia;aktywna")
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=(e2,), teksty=t, spec=spec))
    assert kod == 1 and "R-KOK-23" in out, out


def test_odwolanie_do_wariantu_bez_celu_zatrzymuje_budowe(tmp_path):
    """`wyklucza` albo `regula` z wariantem, którego reguła nie ma (tu E-30/brak), jest błędem zestawu;
    odwołanie do istniejącego wariantu i do reguły spoza katalogu (E-31) nie."""
    zle = E30.replace(";E-31;;;;", ";E-31,E-30/brak;;;;")
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=(zle,)))
    assert kod != 0 and "E-30/brak" in out and "odwołanie" in out, out
    kod, out = uruchom(przygotuj(tmp_path, katalog_e=(E30,)))
    assert kod == 0, out


def test_zrodlo_z_naglowka_tylko_przy_tej_samej_tresci(tmp_path):
    # ponowna budowa bez zmiany treści nie zmienia `zrodlo`, więc plik w repo jest powtarzalny
    h = tmp_path / "zestaw_wbudowany.h"
    h.write_text('static const char PROGI[] = R"zestaw(#cpap-eink progi 1 zestaw=z-abc pochodzenie=wbudowany '
                 'zamrozenie=brak zrodlo=1234567890ab skrot=sha256:00\n', encoding="utf-8")
    assert buduj_zestaw.zrodlo_z_naglowka(str(h), "z-abc") == "1234567890ab"
    assert buduj_zestaw.zrodlo_z_naglowka(str(h), "z-inny") is None
    assert buduj_zestaw.zrodlo_z_naglowka(str(tmp_path / "brak.h"), "z-abc") is None
