# SPDX-License-Identifier: GPL-3.0-or-later
"""Porady programu „Dobry start” w zestawie danych (tools/zestaw/porady.py, ADR-0049; R-DS-36…R-DS-41, R-DS-52).

Uruchomienie: python3 -m pytest -q tools/test_porady_zestaw.py
"""

import csv
import io
import os
import shutil
import subprocess
import sys

import pytest

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TU)
KATALOG = os.path.join(REPO, "docs", "spec", "katalog")
sys.path.insert(0, os.path.join(TU, "zestaw"))
import porady  # noqa: E402

K = porady.wczytaj_kroje()


def wiersze():
    with open(os.path.join(KATALOG, "porady.csv"), encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter=";"))


def kopia(tmp_path, zmien=None, zmien_ankiete=None):
    kat = tmp_path / "katalog"
    kat.mkdir(parents=True)
    for n in ("porady.csv", "porady-ankieta.csv"):
        shutil.copy(os.path.join(KATALOG, n), kat / n)
    for n, f in (("porady.csv", zmien), ("porady-ankieta.csv", zmien_ankiete)):
        if not f:
            continue
        t = (kat / n).read_text(encoding="utf-8")
        r = list(csv.DictReader(io.StringIO(t), delimiter=";"))
        pola = list(r[0].keys())
        r = f(r)
        out = io.StringIO()
        w = csv.DictWriter(out, fieldnames=pola, delimiter=";", lineterminator="\n")
        w.writeheader()
        w.writerows(r)
        (kat / n).write_text(out.getvalue(), encoding="utf-8")
    return str(kat)


def test_porady_z_repo_przechodza_wszystkie_kontrole():
    p, a = porady.buduj_porady(KATALOG, K)
    pola = p.splitlines()[0].split(";")
    assert pola == porady.POLA_PANELU
    for zrodlo in ("ksiazka", "blog", "zalecenia", "poziom", "werdykt", "wyzwalacz"):
        assert zrodlo not in pola, "kolumna źródeł nie idzie do firmware"
    # odnośniki źródeł nie idą do firmware; adres w firmware tylko w kolumnie `qr` (wersja 2, R-DS-62)
    iq = pola.index("qr")
    for w in p.splitlines()[1:]:
        f = w.split(";")
        assert all("https://" not in x for i, x in enumerate(f) if i != iq), f[0]
    assert len(p.splitlines()) - 1 == len(wiersze())
    assert len(a.splitlines()) >= 2


def test_R_DS_39_tresc_za_dluga_odrzucona_z_id(tmp_path):
    def f(r):
        for x in r:
            if x["id"] == "DS-D02":
                x["tresc"] = (x["tresc"] + " ") * 3
        return r
    with pytest.raises(porady.BladPorad, match="DS-D02: treść"):
        porady.buduj_porady(kopia(tmp_path, f), K)


def test_R_DS_39_tytul_i_krok_za_dlugie(tmp_path):
    def f(r):
        for x in r:
            if x["id"] == "DS-D03":
                x["tytul"] = "Bardzo długi tytuł porady, który na pewno nie zmieści się w jednej linii"
            if x["id"] == "DS-D07":  # karta: krok w kolumnie 490 px
                x["krok"] = "→ Krok karty tygodnia dłuższy niż kolumna tekstu karty obok budzika"
        return r
    with pytest.raises(porady.BladPorad) as e:
        porady.buduj_porady(kopia(tmp_path, f), K)
    assert "DS-D03: tytuł" in str(e.value) and "DS-D07: krok" in str(e.value)


def test_R_DS_52_ikona_przycisku_szersza_niz_znak():
    # ▶ jest ikoną (kwadrat wersalik + 6 px i odstęp 6 px), nie znakiem `>` z renderu
    f = K["P24B"]
    assert f.szer("▶") == f.wersalik() + 12 == 30
    assert f.szer("▶") > f.szer(">")
    # tekst, który z `>` mieści się w 748 px, a z ikoną nie
    t = "x" * 10
    while f.szer(t + ">") <= 740:
        t += "x"
    assert f.szer(t + ">") <= 748 < f.szer(t + "▶")


def test_R_DS_41_test_adr2_odrzuca_wartosc_ustawienia(tmp_path):
    def f(r):
        for x in r:
            if x["id"] == "DS-D16":
                x["krok"] = "→ Ustaw EPR 2."
        return r
    with pytest.raises(porady.BladPorad, match="DS-D16: test ADR-0002"):
        porady.buduj_porady(kopia(tmp_path, f), K)


def test_R_DS_41_samotest_adr2_i_wyjatki_z_uzasadnieniem():
    a = porady.adr2()
    assert a.samotest() == []
    for k, uz in porady.wyjatki_adr2().items():
        assert uz.strip(), k


def test_R_DS_53_znacznik_progu(tmp_path):
    progi = porady.wczytaj_progi()
    assert porady.podstaw_progi("Od {prog:R-DANE-23.przeciek_nosowa} L/min", progi) == "Od 24 L/min"
    # ustawienie „Uwaga o masce od”: długość z górną granicą zakresu (R-KONF-42)
    assert porady.podstaw_progi("{prog:S-K-10.uwaga_o_masce}", progi, najdluzsze=True) == progi["R-KONF-42.zakres_max"]

    def f(r):
        for x in r:
            if x["id"] == "DS-D02":
                x["tresc"] = x["tresc"] + " {prog:X-99.brak}"
        return r
    with pytest.raises(porady.BladPorad, match="DS-D02: znacznik"):
        porady.buduj_porady(kopia(tmp_path, f), K)


def test_R_DS_29_doba_bez_nawilzacza(tmp_path):
    # wersja 2 (R-DS-29 zmienione, R-DS-71): każda doba planu ma wariant `.bez` albo poradę bez wody; bez DS-D19.bez
    # doba 23 adaptacji nie ma porady bez nawilżacza
    with pytest.raises(porady.BladPorad, match="adaptacja doba 23 .* bez nawilżacza"):
        porady.buduj_porady(kopia(tmp_path, lambda r: [x for x in r if x["id"] != "DS-D19.bez"]), K)


def test_R_DS_37_powtorzone_id_i_brak_tresci(tmp_path):
    with pytest.raises(porady.BladPorad, match="powtórzone ID"):
        porady.buduj_porady(kopia(tmp_path, lambda r: r + [dict(r[5])]), K)


def test_R_DS_37_brak_tresci(tmp_path):
    def f(r):
        for x in r:
            if x["id"] == "DS-D09":
                x["tresc"] = ""
        return r
    with pytest.raises(porady.BladPorad, match="DS-D09: brak pola tresc"):
        porady.buduj_porady(kopia(tmp_path, f), K)


def test_plan_kazde_miejsce_w_kazdym_trybie_i_obu_stanach_nawilzacza(tmp_path):
    # bez DS-D06.bez doba 12 adaptacji nie ma porady bez nawilżacza (wersja 2: doby faz zamiast miejsc 1…42)
    with pytest.raises(porady.BladPorad, match="adaptacja doba 12"):
        porady.buduj_porady(kopia(tmp_path, lambda r: [x for x in r if x["id"] != "DS-D06.bez"]), K)


def test_plan_brak_wariantu_trybu(tmp_path):
    with pytest.raises(porady.BladPorad, match="w trybie ASV"):
        porady.buduj_porady(kopia(tmp_path, lambda r: [x for x in r if x["id"] != "DS-D01.asv"]), K)


def test_nieznany_tryb_i_odwolanie(tmp_path):
    def f(r):
        for x in r:
            if x["id"] == "DS-Z04":
                x["tryb"] = "VAuto"
        return r
    with pytest.raises(porady.BladPorad, match="nieznany tryb"):
        porady.buduj_porady(kopia(tmp_path, f), K)

    def g(r):
        for x in r:
            if x["id"] == "DS-Z01":
                x["stopien_wyzej"] = "DS-Z99"
        return r
    with pytest.raises(porady.BladPorad, match="nie ma takiej porady"):
        porady.buduj_porady(kopia(tmp_path / "b", g), K)


def test_ankieta_zla_odpowiedz_wylaczna(tmp_path):
    def f(r):
        for x in r:
            if x["id"] == "DS-Q04":
                x["wylaczna"] = "99"
        return r
    with pytest.raises(porady.BladPorad, match="wyłączna"):
        porady.buduj_porady(kopia(tmp_path, zmien_ankiete=f), K)


def test_twarda_spacja_po_jednoliterowym_slowie():
    assert porady.twarde("Maska i rura w 4 h") == "Maska i rura w 4 h"


def test_zawijanie_jak_na_panelu_pierwsze_slowo_zostaje():
    # słowo szersze niż kolumna zostaje w swojej linii (zawin() w src/ekrany.cpp), ale kontrola je zgłasza
    assert porady.zawin(K["P24S"], "a " + "w" * 80, 748) == ["a", "w" * 80]


def test_buduj_zestaw_bez_porady_csv_ma_puste_porady(tmp_path):
    sys.path.insert(0, os.path.join(TU, "zestaw"))
    import buduj_zestaw
    h = buduj_zestaw.naglowek_cpp(["a\n", "b\n", "c\n"])
    assert 'PORADY[] = R"zestaw()zestaw"' in h and 'ANKIETA[] = R"zestaw()zestaw"' in h


def test_buduj_zestaw_konczy_sie_bledem_przy_zlej_poradzie(tmp_path):
    # pełny przebieg skryptu na katalogu repo z wydłużoną poradą: kod wyjścia 1 i ID porady w komunikacie
    kat = tmp_path / "kat"
    shutil.copytree(KATALOG, kat)
    t = (kat / "porady.csv").read_text(encoding="utf-8").replace("Pierwsze noce bywają dziwne;",
                                                                  "Pierwsze noce bywają dziwne i bardzo długie dla wielu osób;")
    (kat / "porady.csv").write_text(t, encoding="utf-8")
    r = subprocess.run([sys.executable, os.path.join(TU, "zestaw", "buduj_zestaw.py"), "--katalog", str(kat),
                        "--bez-h"], capture_output=True, text=True)
    assert r.returncode == 1 and "DS-D02" in r.stderr


def test_budowa_zestawu_bez_materialow_zrodlowych(tmp_path):
    """Materiały źródłowe porad nie są w repo: budowa zestawu w kopii z samymi
    tools/zestaw, katalogiem, progami, spec i krojami daje ten sam zestaw_wbudowany.h (test ADR-0002 z tools/zestaw)."""
    kopia_repo = tmp_path / "repo"
    shutil.copytree(os.path.join(TU, "zestaw"), kopia_repo / "tools" / "zestaw",
                    ignore=shutil.ignore_patterns("__pycache__"))
    shutil.copytree(KATALOG, kopia_repo / "docs" / "spec" / "katalog")
    for n in ("progi.csv", "specyfikacja-funkcjonalna.md"):
        shutil.copy(os.path.join(REPO, "docs", "spec", n), kopia_repo / "docs" / "spec" / n)
    for d, n in (("include", "fonty.h"), ("src", "zestaw_wbudowany.h")):
        (kopia_repo / "firmware" / "prototyp" / d).mkdir(parents=True, exist_ok=True)
        shutil.copy(os.path.join(REPO, "firmware", "prototyp", d, n), kopia_repo / "firmware" / "prototyp" / d / n)
    assert not (kopia_repo / "docs" / "research").exists()
    r = subprocess.run([sys.executable, "-I", str(kopia_repo / "tools" / "zestaw" / "buduj_zestaw.py")],
                       cwd=str(kopia_repo), capture_output=True, text=True, timeout=300)
    assert r.returncode == 0, r.stderr[-2000:]
    assert "porady: " in r.stdout
    with open(os.path.join(REPO, "firmware", "prototyp", "src", "zestaw_wbudowany.h"), encoding="utf-8") as f:
        assert (kopia_repo / "firmware" / "prototyp" / "src" / "zestaw_wbudowany.h").read_text(encoding="utf-8") == f.read()


# ---------- wersja 2 (spec 5.8.14, R-DS-54…R-DS-71) ----------
def _zmien(id_, **pola):
    def f(r):
        for x in r:
            if x["id"] == id_:
                x.update(pola)
        return r
    return f


def test_R_DS_62_adres_qr_dluzszy_niz_42_bajty(tmp_path):
    with pytest.raises(porady.BladPorad, match="DS-S07: adres QR"):
        porady.buduj_porady(kopia(tmp_path, _zmien("DS-S07", qr="https://cpapblog.pl/?p=" + "1" * 21)), K)


def test_R_DS_62_qr_na_karcie_i_kolumna_513(tmp_path):
    with pytest.raises(porady.BladPorad, match="DS-D07: kod QR na karcie"):
        porady.buduj_porady(kopia(tmp_path / "a", _zmien("DS-D07", qr="https://cpapblog.pl/?p=1")), K)
    # tekst, który mieści się w 748 px w 5 liniach, a z kodem QR (513 px) już nie
    def f(r):
        for x in r:
            if x["id"] == "DS-D09":
                t = "Słowo"
                while len(porady.zawin(K["P24S"], t + " słowo", porady.KOLUMNA)) <= 5:
                    t += " słowo"
                x["tresc"], x["qr"] = t, "https://cpapblog.pl/?p=1"
        return r
    with pytest.raises(porady.BladPorad, match=r"DS-D09: treść \d+ linii > 5"):
        porady.buduj_porady(kopia(tmp_path / "b", f), K)


def test_R_DS_64_fakty_poza_karta_i_za_szerokie(tmp_path):
    with pytest.raises(porady.BladPorad, match="DS-D09: fakty poza kartą"):
        porady.buduj_porady(kopia(tmp_path / "a", _zmien("DS-D09", fakty="{czas}::z maską")), K)
    szerokie = " | ".join(["{srednio}::bardzo długi podpis faktu"] * 3)
    with pytest.raises(porady.BladPorad, match="DS-D07: rząd faktów"):
        porady.buduj_porady(kopia(tmp_path / "b", _zmien("DS-D07", fakty=szerokie)), K)


def test_R_DS_61_ahi_na_karcie_i_w_pochwale(tmp_path):
    with pytest.raises(porady.BladPorad, match="DS-D28: AHI na karcie"):
        porady.buduj_porady(kopia(tmp_path / "a", _zmien("DS-D28", krok="→ AHI niskie.")), K)
    with pytest.raises(porady.BladPorad, match="DS-P01: AHI"):
        porady.buduj_porady(kopia(tmp_path / "b", _zmien("DS-P01", tytul="AHI w normie")), K)


def test_R_DS_60_nieznany_znacznik_danych(tmp_path):
    with pytest.raises(porady.BladPorad, match=r"DS-P01: nieznany znacznik danych \{ahi\}"):
        porady.buduj_porady(kopia(tmp_path, _zmien("DS-P01", tresc="Ta noc: {ahi}.")), K)


def test_R_DS_56_koncowa_fazy_i_zla_faza(tmp_path):
    with pytest.raises(porady.BladPorad, match="DS-D34: `koncowa_fazy`"):  # doba 41, ostatnia doba adaptacji to 42
        porady.buduj_porady(kopia(tmp_path / "a", _zmien("DS-D34", flagi="podstawowa,objaw,koncowa_fazy")), K)
    with pytest.raises(porady.BladPorad, match="DS-D09: zła faza"):
        porady.buduj_porady(kopia(tmp_path / "b", _zmien("DS-D09", faza="ponad")), K)
    with pytest.raises(porady.BladPorad, match="DS-P01: zła pora"):
        porady.buduj_porady(kopia(tmp_path / "c", _zmien("DS-P01", pora="wieczor")), K)


def test_R_DS_71_pochwala_bez_wariantu_trybu(tmp_path):
    with pytest.raises(porady.BladPorad, match="DS-P01: brak wariantu w trybie ASV"):
        porady.buduj_porady(kopia(tmp_path, _zmien("DS-P01", tryb="CPAP+AutoSet")), K)


def test_werdykt_bez_kodow_i_numerow_pytan():
    """Kolumna `werdykt` w porady.csv idzie 1:1 do publicznego repo: bez odsyłaczy [Snn] do notatek, bez kodów wpisów
    bloga (A1, B2…) i bez numerów pytań otwartych P-nnn (zapis neutralny jak w `cpapblog-eink`)."""
    import csv
    import re
    with open(os.path.join(KATALOG, "porady.csv"), encoding="utf-8") as f:
        wiersze = list(csv.DictReader(f, delimiter=";"))
    assert wiersze
    zle = []
    for w in wiersze:
        t = w["werdykt"]
        if (re.search(r"\[S\d+", t) or re.search(r"\bblog\w*\s+[A-L]\d", t)
                or re.search(r"(?<![-\w])[A-L]\d{1,2}(?![\w])", t) or re.search(r"\bP-\d{3}\b", t)):
            zle.append((w["id"], t))
    assert not zle, zle
