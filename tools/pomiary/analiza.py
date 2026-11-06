#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Analiza plików pomiarowych ściągniętych z panelu (spec techniczna 2.4).

Dwa tryby:
  python3 tools/pomiary/analiza.py dryf ŚCIEŻKA
      plik /pomiary/dryf.csv z firmware testowego pomiaru 12 (dryf zegara, T-NF-19)
  python3 tools/pomiary/analiza.py bateria ŚCIEŻKA [--od RRRR-MM-DD] [--do RRRR-MM-DD]
      dziennik /bateria.csv prototypu (pomiary 2, 3 i 9, T-NF-01, T-NF-02, T-NF-05)

Wynik: markdown na standardowe wyjście, same liczby zbiorcze do wklejenia do notatki w docs/research/.
Surowe pliki nie trafiają do repozytorium (WY12); skrypt czyta je z podanej ścieżki i niczego nie zapisuje.
Kod wyjścia: 0 przy wyniku, 2 przy złym pliku albo złych argumentach (komunikat po polsku na stderr).

Tylko biblioteka standardowa, Python 3.9+ (strefa czasu z modułu zoneinfo).
"""

import argparse
import datetime as dt
import math
import os
import statistics
import sys
from typing import Dict, List, Optional, Tuple

# ---------- stałe (źródło w komentarzu) ----------

# dryf.csv: firmware testowe, gałąź etap3-firmware-testowy, src/pomiary.h, src/pomiary.cpp naglowekDryfu()
NAGLOWEK_DRYFU = "wybudzenie;ntp_ms;od_startu_h;panel_minus_ntp_ms;pcf_minus_ntp_ms;rtt_ms;pcf_vl"
BEZPIECZNIK_MS = 2 * 60 * 1000  # src/pomiary.h BEZPIECZNIK_12_MS [stała testu]
CZAS_POMIARU_H = 72.0  # src/pomiary.h CZAS_POMIARU_12_MS, spec 2.4 pomiar 12
KRYTERIUM_DRYFU_S = 5 * 60  # (d): dopuszczalny dryf ≤ 5 min w 72 h [PRÓG]
# Odcinek zegara panelu z tempem poniżej tej części mediany: panel w tym czasie najpewniej nie spał (okno
# serwisowe, czuwanie), a zegar panelu dryfuje tylko w głębokim uśpieniu (generator RC). [stała narzędzia]
CZESC_MEDIANY_BEZ_SNU = 0.25

# bateria.csv: firmware/prototyp/src/bateria.h
BAT_SKOK_LADOWANIA_MV = 60  # [PRÓG]
BAT_OKNO_LADOWANIA = 12  # [stała prototypu]
BAT_PROG_ROZLADOWANIA_MV = 3500  # [PRÓG]
BAT_MIN_OKNO_S = 24 * 3600
CEL_DNI = 30  # R-NF-01
BUDZET_WIFI_MIN = 10.0  # spec 8.1, zwykła doba
LIMIT_WIFI_BEZ_DANYCH_MIN = 45.0  # spec 8.1, doba bez danych (od 02.11.2026)
BUDZET_WYBUDZEN = 80  # spec 8.1, zwykła doba
LIMIT_WYBUDZEN_BEZ_DANYCH = 110  # T-NF-02, pierwsza doba bez danych
KRYTERIUM_BEZ_WIFI_S = 2.0  # pomiar 2 (spec 2.4)
KRYTERIUM_WIFI_S = 8.0  # pomiar 3, T-NF-05
STREFA = "Europe/Warsaw"

KOD_BLEDU = 2


class BladPliku(Exception):
    """Plik nie do analizy: komunikat dla człowieka po polsku."""


# ---------- liczby po polsku ----------

def liczba(x: Optional[float], miejsca: int = 1) -> str:
    """Przecinek dziesiętny, spacja co trzy cyfry, minus U+2212; None jako „–”."""
    if x is None or (isinstance(x, float) and math.isnan(x)):
        return "–"
    s = "{:,.{}f}".format(x, miejsca)
    if s.lstrip("-").strip("0,.") == "":  # −0,0 → 0,0
        s = s.lstrip("-")
    s = s.replace(",", " ").replace(".", ",")
    if s.startswith("-"):
        s = "−" + s[1:]
    return s


def calk(x: Optional[float]) -> str:
    return liczba(x, 0)


def mediana(v: List[float]) -> Optional[float]:
    return statistics.median(v) if v else None


def percentyl(v: List[float], p: float) -> Optional[float]:
    """Metoda najbliższej rangi: najmniejsza wartość, od której co najmniej p% danych jest nie większe."""
    if not v:
        return None
    s = sorted(v)
    k = max(1, math.ceil(p / 100.0 * len(s)))
    return s[k - 1]


def regresja(xs: List[float], ys: List[float]) -> Optional[Dict[str, float]]:
    """Regresja liniowa y = a + b·x metodą najmniejszych kwadratów; None, gdy za mało punktów albo x stałe."""
    n = len(xs)
    if n < 2:
        return None
    sx, sy = sum(xs), sum(ys)
    sxx = sum(x * x for x in xs)
    sxy = sum(x * y for x, y in zip(xs, ys))
    mian = n * sxx - sx * sx
    if abs(mian) <= 1e-12:
        return None
    b = (n * sxy - sx * sy) / mian
    a = (sy - b * sx) / n
    reszty = [y - (a + b * x) for x, y in zip(xs, ys)]
    wynik = {"a": a, "b": b, "n": n, "reszta_max": max(abs(r) for r in reszty),
             "reszta_rms": math.sqrt(sum(r * r for r in reszty) / n), "blad_b": float("nan")}
    if n >= 3:
        sr = math.sqrt(sum(r * r for r in reszty) / (n - 2))
        sxx_c = sxx - sx * sx / n
        wynik["blad_b"] = sr / math.sqrt(sxx_c) if sxx_c > 0 else float("nan")
    return wynik


def czytaj_tekst(sciezka: str) -> str:
    if not os.path.isfile(sciezka):
        raise BladPliku("Nie ma pliku: {}".format(sciezka))
    try:
        with open(sciezka, "rb") as f:
            dane = f.read()
    except OSError as e:
        raise BladPliku("Nie mogę przeczytać pliku {}: {}".format(sciezka, e.strerror))
    try:
        return dane.decode("utf-8-sig")
    except UnicodeDecodeError:
        raise BladPliku("Plik {} nie jest tekstem UTF-8 (to nie plik z panelu?).".format(sciezka))


# =====================================================================================================
# Pomiar 12: dryf zegara (dryf.csv)
# =====================================================================================================

class WierszDryfu:
    __slots__ = ("wybudzenie", "ntp_ms", "od_startu_h", "panel_ms", "pcf_ms", "rtt_ms", "vl", "faza")

    def __init__(self, wybudzenie, ntp_ms, od_startu_h, panel_ms, pcf_ms, rtt_ms, vl, faza):
        self.wybudzenie = wybudzenie
        self.ntp_ms = ntp_ms
        self.od_startu_h = od_startu_h
        self.panel_ms = panel_ms  # panel − NTP, ms
        self.pcf_ms = pcf_ms  # PCF8563 − NTP, ms; None = brak odczytu
        self.rtt_ms = rtt_ms
        self.vl = vl
        self.faza = faza  # 1: zegar panelu bez synchronizacji; 2: po bezpieczniku, zegar panelu z NTP, dalej PCF


def _int(s: str) -> int:
    s = s.strip()
    if not s or not (s.lstrip("-").isdigit()):
        raise ValueError(s)
    return int(s)


def parsuj_dryf(tekst: str) -> Tuple[List[WierszDryfu], List[str], List[str]]:
    """Wiersze, komentarze (bez „#”) i opisy pominiętych linii.

    Format z firmware (src/pomiary.cpp wierszDryfu()): pole pcf_minus_ntp_ms puste = brak odczytu PCF8563; wartość
    0 firmware zapisuje jako 1 (main.cpp: `x ? x : 1`), więc 0 w pliku też traktujemy jako brak odczytu.
    Faza 2 zaczyna się po komentarzu o bezpieczniku albo „v2:” albo po wierszu z |panel − NTP| ≥ 2 min w fazie 1
    (gdyby komentarz się urwał). Po bezpieczniku firmware w każdym wybudzeniu najpierw mierzy, a potem
    synchronizuje zegar panelu z NTP, więc panel − NTP w fazie 2 to dryf od synchronizacji w poprzednim wybudzeniu.
    """
    wiersze: List[WierszDryfu] = []
    komentarze: List[str] = []
    pominiete: List[str] = []
    jest_naglowek = False
    faza = 1
    koniec_fazy_1 = False
    for nr, linia in enumerate(tekst.splitlines(), 1):
        l = linia.strip()
        if not l:
            continue
        if l.startswith("#"):
            k = l[1:].strip()
            komentarze.append(k)
            kl = k.lower()
            if "bezpiecznik" in kl or kl.startswith("v2:") or kl.startswith("koniec po"):
                koniec_fazy_1 = True
                faza = 2
            continue
        if l == NAGLOWEK_DRYFU:
            jest_naglowek = True
            continue
        p = l.split(";")
        if len(p) != 7:
            pominiete.append("linia {}: {} pól zamiast 7 (urwany wiersz?)".format(nr, len(p)))
            continue
        try:
            wyb = _int(p[0])
            ntp = _int(p[1])
            od = float(p[2])
            pan = _int(p[3])
            pcf = _int(p[4]) if p[4].strip() else None
            rtt = _int(p[5])
            vl = _int(p[6])
        except ValueError:
            pominiete.append("linia {}: pole nie jest liczbą".format(nr))
            continue
        if ntp <= 0 or rtt < 0 or vl not in (0, 1) or not math.isfinite(od):
            pominiete.append("linia {}: wartość poza zakresem".format(nr))
            continue
        if pcf == 0:
            pcf = None
        wiersze.append(WierszDryfu(wyb, ntp, od, pan, pcf, rtt, vl == 1, faza))
        if faza == 1 and not koniec_fazy_1 and abs(pan) >= BEZPIECZNIK_MS:
            faza = 2  # następne wiersze po bezpieczniku, nawet gdy komentarz się urwał
    if not jest_naglowek:
        raise BladPliku("Brak nagłówka `{}`: to nie wygląda na plik dryf.csv z pomiaru 12.".format(NAGLOWEK_DRYFU))
    if not wiersze:
        raise BladPliku("W pliku dryf.csv nie ma żadnego poprawnego wiersza z pomiarem.")
    return wiersze, komentarze, pominiete


def analiza_dryfu(wiersze: List[WierszDryfu], komentarze: List[str]) -> Dict:
    w0 = wiersze[0]
    t0_ms = w0.ntp_ms - w0.od_startu_h * 3600000.0  # chwila startu pomiaru (od_startu_h = 0)

    def godz(w: WierszDryfu) -> float:
        return (w.ntp_ms - t0_ms) / 3600000.0

    wynik: Dict = {"n": len(wiersze), "start_ms": t0_ms, "czas_h": godz(wiersze[-1]),
                   "od_h": godz(w0), "komentarze": komentarze}

    # --- zegar panelu, faza 1: odcinki między kolejnymi wierszami ---
    f1 = [w for w in wiersze if w.faza == 1]
    odcinki = []
    for a, b in zip(f1, f1[1:]):
        dt_ms = b.ntp_ms - a.ntp_ms
        if dt_ms <= 0:
            continue
        przyrost = b.panel_ms - a.panel_ms
        odcinki.append({"od": a.wybudzenie, "do": b.wybudzenie, "h": dt_ms / 3600000.0, "przyrost_ms": przyrost,
                        "tempo": przyrost / dt_ms})  # ułamek: s na s
    med_wszystkich = mediana([o["tempo"] for o in odcinki])
    for o in odcinki:
        o["bez_snu"] = med_wszystkich is not None and med_wszystkich > 0 and \
            o["tempo"] < CZESC_MEDIANY_BEZ_SNU * med_wszystkich
    wynik["odcinki"] = odcinki
    sen = [o["tempo"] for o in odcinki if not o["bez_snu"]]
    wynik["tempo_snu"] = {"n": len(sen), "min": min(sen) if sen else None, "mediana": mediana(sen),
                          "max": max(sen) if sen else None}
    if len(f1) >= 2 and f1[-1].ntp_ms > f1[0].ntp_ms:
        wynik["tempo_srednie"] = (f1[-1].panel_ms - f1[0].panel_ms) / (f1[-1].ntp_ms - f1[0].ntp_ms)
        wynik["faza1_h"] = (f1[-1].ntp_ms - f1[0].ntp_ms) / 3600000.0
    else:
        wynik["tempo_srednie"] = None
        wynik["faza1_h"] = 0.0
    bezp = [w for w in f1 if abs(w.panel_ms) >= BEZPIECZNIK_MS]
    wynik["bezpiecznik"] = [{"wybudzenie": w.wybudzenie, "h": godz(w), "panel_ms": w.panel_ms} for w in bezp]

    # --- zegar panelu, faza 2: dryf od synchronizacji w poprzednim wybudzeniu (przybliżenie) ---
    tempa2 = []
    for a, b in zip(wiersze, wiersze[1:]):
        if b.faza != 2:
            continue
        dt_ms = b.ntp_ms - a.ntp_ms
        if dt_ms > 0:
            tempa2.append(b.panel_ms / dt_ms)
    wynik["tempo_fazy2"] = {"n": len(tempa2), "min": min(tempa2) if tempa2 else None, "mediana": mediana(tempa2),
                            "max": max(tempa2) if tempa2 else None}

    # --- PCF8563: regresja przesunięcia w czasie ---
    pcf = [w for w in wiersze if w.pcf_ms is not None and not w.vl]
    wynik["pcf_bez_odczytu"] = sum(1 for w in wiersze if w.pcf_ms is None)
    wynik["pcf_vl"] = sum(1 for w in wiersze if w.vl)
    xs = [godz(w) for w in pcf]
    ys = [float(w.pcf_ms) for w in pcf]
    r = regresja(xs, ys)
    wynik["pcf_n"] = len(pcf)
    wynik["pcf_h"] = (xs[-1] - xs[0]) if len(xs) >= 2 else 0.0
    wynik["pcf"] = r  # a w ms, b w ms na godzinę
    rtt = [float(w.rtt_ms) for w in wiersze]
    wynik["rtt"] = {"min": min(rtt), "mediana": mediana(rtt), "max": max(rtt)}
    return wynik


def _tempo_na_dobe_s(tempo: Optional[float]) -> Optional[float]:
    return None if tempo is None else tempo * 86400.0


def _proc(tempo: Optional[float]) -> str:
    return "–" if tempo is None else liczba(tempo * 100.0, 3) + "%"


def raport_dryfu(w: Dict, nazwa: str) -> str:
    o: List[str] = []
    start = dt.datetime.fromtimestamp(w["start_ms"] / 1000.0, dt.timezone.utc)
    o.append("## Pomiar 12: dryf zegara (`{}`)".format(nazwa))
    o.append("")
    o.append("| Co | Wartość |")
    o.append("|---|---|")
    o.append("| start pomiaru (UTC) | {} |".format(start.strftime("%d.%m.%Y %H:%M")))
    o.append("| wierszy z pomiarem | {} |".format(w["n"]))
    o.append("| całkowity czas trwania | {} h (od {} h do {} h od startu) |".format(
        liczba(w["czas_h"] - w["od_h"], 2), liczba(w["od_h"], 2), liczba(w["czas_h"], 2)))
    o.append("| przelot NTP (RTT) min / mediana / max | {} / {} / {} ms |".format(
        calk(w["rtt"]["min"]), calk(w["rtt"]["mediana"]), calk(w["rtt"]["max"])))
    o.append("")

    # zegar panelu
    o.append("### Zegar panelu (generator RC w głębokim uśpieniu)")
    o.append("")
    if w["odcinki"]:
        o.append("| Odcinek (wybudzenia) | Czas | Przyrost panel − NTP | Tempo | s/dobę | Uwaga |")
        o.append("|---|---|---|---|---|---|")
        for x in w["odcinki"]:
            o.append("| {} → {} | {} h | {} ms | {} | {} | {} |".format(
                x["od"], x["do"], liczba(x["h"], 3), calk(x["przyrost_ms"]), _proc(x["tempo"]),
                liczba(_tempo_na_dobe_s(x["tempo"]), 0),
                "panel najpewniej nie spał (okno serwisowe), poza statystyką" if x["bez_snu"] else ""))
        o.append("")
        t = w["tempo_snu"]
        o.append("| Tempo na odcinkach uśpienia | min | mediana | max |")
        o.append("|---|---|---|---|")
        o.append("| % | {} | {} | {} |".format(_proc(t["min"]), _proc(t["mediana"]), _proc(t["max"])))
        o.append("| s/dobę | {} | {} | {} |".format(*(liczba(_tempo_na_dobe_s(t[k]), 0)
                                                   for k in ("min", "mediana", "max"))))
        o.append("")
        o.append("- Odcinków uśpienia: {} z {}; odcinek z tempem poniżej {}% mediany liczony jako czas bez "
                 "uśpienia (zegar panelu dryfuje tylko w głębokim uśpieniu).".format(
                     t["n"], len(w["odcinki"]), calk(CZESC_MEDIANY_BEZ_SNU * 100)))
        if w["tempo_srednie"] is not None:
            o.append("- Średnio od startu do ostatniego wiersza bez synchronizacji ({} h, z czasem bez uśpienia): "
                     "{} ({} s/dobę).".format(liczba(w["faza1_h"], 2), _proc(w["tempo_srednie"]),
                                              liczba(_tempo_na_dobe_s(w["tempo_srednie"]), 0)))
    else:
        o.append("- Za mało wierszy bez synchronizacji na odcinek (potrzebne co najmniej 2).")
    if w["bezpiecznik"]:
        for b in w["bezpiecznik"]:
            o.append("- Bezpiecznik (|panel − NTP| ≥ 2 min): wybudzenie {}, {} h od startu, panel − NTP {} s.".format(
                b["wybudzenie"], liczba(b["h"], 2), liczba(b["panel_ms"] / 1000.0, 1)))
    else:
        o.append("- Bezpiecznik 2 min nie zadziałał w danych.")
    t2 = w["tempo_fazy2"]
    if t2["n"]:
        o.append("- Po bezpieczniku (zegar panelu z NTP w każdym wybudzeniu, panel − NTP = dryf od synchronizacji w "
                 "poprzednim wybudzeniu, przybliżenie): {} odcinków, tempo min / mediana / max {} / {} / {}.".format(
                     t2["n"], _proc(t2["min"]), _proc(t2["mediana"]), _proc(t2["max"])))
    o.append("")

    # PCF8563
    o.append("### PCF8563 (kwarc 32 768 Hz)")
    o.append("")
    r = w["pcf"]
    o.append("| Co | Wartość |")
    o.append("|---|---|")
    o.append("| punktów w regresji | {} (pominięte: bez odczytu {}, z bitem VL {}) |".format(
        w["pcf_n"], w["pcf_bez_odczytu"], w["pcf_vl"]))
    o.append("| czas objęty regresją | {} h |".format(liczba(w["pcf_h"], 2)))
    if r:
        ppm = r["b"] / 3.6  # ms na godzinę → ppm (1 ppm = 3,6 ms na godzinę)
        blad_ppm = r["blad_b"] / 3.6 if not math.isnan(r["blad_b"]) else None
        o.append("| przesunięcie na starcie (z ustawiania) | {} ms |".format(calk(r["a"])))
        o.append("| dryf | {} ppm{} |".format(liczba(ppm, 2), "" if blad_ppm is None else
                                               " (błąd standardowy ± {} ppm)".format(liczba(blad_ppm, 2))))
        o.append("| dryf na dobę | {} s/dobę |".format(liczba(ppm * 0.0864, 3)))
        o.append("| błąd w 72 h z samego dryfu | {} s |".format(liczba(r["b"] * CZAS_POMIARU_H / 1000.0, 2)))
        o.append("| reszty regresji RMS / max | {} / {} ms |".format(liczba(r["reszta_rms"], 1),
                                                                    liczba(r["reszta_max"], 1)))
    else:
        o.append("| dryf | za mało punktów (potrzebne co najmniej 2 w różnych chwilach) |")
    o.append("")
    rozdz = w["rtt"]["mediana"] / 2.0 + 4.0
    o.append("- Rozdzielczość jednego punktu w najgorszym razie ok. ± {} ms: połowa przelotu NTP (mediana {} ms, "
             "niesymetria drogi) plus ok. 4 ms na wykrycie zmiany sekundy PCF8563 (odpytywanie co 3 ms przez I2C, "
             "main.cpp `roznicaPcf()`); reszty regresji wyżej pokazują rozrzut faktyczny.".format(
                 calk(rozdz), calk(w["rtt"]["mediana"])))
    if w["pcf_h"] > 0:
        min_ppm = 2 * rozdz / (w["pcf_h"] * 3600000.0) * 1e6
        o.append("- Przy {} h danych dryf mniejszy niż ok. {} ppm ginie w tej rozdzielczości; wynik pewniejszy "
                 "z każdą godziną.".format(liczba(w["pcf_h"], 2), liczba(min_ppm, 1)))
    o.append("")
    if w["komentarze"]:
        o.append("Komentarze z pliku: " + "; ".join("`{}`".format(k) for k in w["komentarze"]) + ".")
        o.append("")

    o.append("### Wniosek")
    o.append("")
    o.append(wniosek_dryfu(w))
    o.append("")
    return "\n".join(o)


def wniosek_dryfu(w: Dict) -> str:
    zd = []
    czas = w["czas_h"] - w["od_h"]
    if czas < CZAS_POMIARU_H:
        zd.append("Przebieg trwał {} h, krócej niż 72 h: liczby dla 72 h są ekstrapolacją liniową{}.".format(
            liczba(czas, 1), ", wstępne" if czas < 24 else ""))
    panel = w["tempo_snu"]["mediana"]
    panel_ok = None
    if panel is not None:
        blad72 = abs(panel) * CZAS_POMIARU_H * 3600.0
        panel_ok = blad72 <= KRYTERIUM_DRYFU_S
        txt = ("Zegar panelu: mediana tempa w uśpieniu {} ({} s/dobę), czyli ok. {} min w 72 h wobec kryterium "
               "≤ 5 min w 72 h [PRÓG] ((d)): {}.").format(
            _proc(panel), liczba(_tempo_na_dobe_s(panel), 0), liczba(blad72 / 60.0, 1),
            "spełnione" if panel_ok else "niespełnione")
        if not panel_ok and panel != 0:
            txt += " Błąd 5 min zegar panelu łapie po ok. {} h bez synchronizacji.".format(
                liczba(KRYTERIUM_DRYFU_S / abs(panel) / 3600.0, 1))
        if w["bezpiecznik"]:
            txt += " Bezpiecznik 2 min zadziałał po {} h.".format(liczba(w["bezpiecznik"][0]["h"], 2))
        zd.append(txt)
    else:
        zd.append("Zegar panelu: za mało danych na tempo.")
    r = w["pcf"]
    pcf_ok = None
    if r:
        blad72 = abs(r["b"]) * CZAS_POMIARU_H / 1000.0 + abs(r["a"]) / 1000.0
        pcf_ok = blad72 <= KRYTERIUM_DRYFU_S
        txt = ("PCF8563: dryf {} ppm ({} s/dobę), w 72 h ok. {} s razem z przesunięciem z ustawiania "
               "({} ms): kryterium {}").format(
            liczba(r["b"] / 3.6, 2), liczba(r["b"] / 3.6 * 0.0864, 3), liczba(blad72, 1), calk(r["a"]),
            "spełnione" if pcf_ok else "niespełnione")
        if pcf_ok and blad72 > 0:
            txt += " z zapasem ok. {} razy".format(calk(KRYTERIUM_DRYFU_S / blad72))
        zd.append(txt + ".")
    else:
        zd.append("PCF8563: za mało punktów na regresję.")
    if pcf_ok and panel_ok is False:
        zd.append("Rekomendacja: po każdym wybudzeniu brać czas z PCF8563 (zegar panelu ustawiany z "
                  "PCF8563), a NTP tylko do korekty PCF8563 przy połączeniu; sam zegar panelu nie utrzyma R-AW-19.")
    elif pcf_ok and panel_ok:
        zd.append("Rekomendacja: oba zegary mieszczą się w kryterium; PCF8563 dokładniejszy, więc "
                  "jako źródło czasu bez sieci nadal PCF8563, zegar panelu jako zapas.")
    elif pcf_ok is False:
        zd.append("Rekomendacja: żaden zegar nie spełnia kryterium; skrócić okres z R-AW-19 "
                  "albo dodać korektę dryfu.")
    else:
        zd.append("Rekomendacja: brak rozstrzygnięcia, za mało danych PCF8563.")
    return " ".join(zd)


# =====================================================================================================
# Bateria (bateria.csv)
# =====================================================================================================

class WpisBaterii:
    __slots__ = ("ep", "mV", "wifi_ms", "aktywny_ms", "powod", "aktywny_wlasny_ms")

    def __init__(self, ep, mV, wifi_ms, aktywny_ms, powod):
        self.ep = ep
        self.mV = mV
        self.wifi_ms = wifi_ms
        self.aktywny_ms = aktywny_ms  # dotyczy POPRZEDNIEGO wybudzenia (bateria.h)
        self.powod = powod
        self.aktywny_wlasny_ms = None  # czas aktywności TEGO wybudzenia (z następnego wiersza)


def _liczba_c(s: str) -> Optional[int]:
    """Jak strtol z kontrolą końca w bateria.cpp: cała treść to liczba całkowita (znak dozwolony)."""
    if not s:
        return None
    t = s[1:] if s[0] in "+-" else s
    if not t.isdigit() or not t.isascii():
        return None
    return int(s)


def parsuj_linie_baterii(linia: str) -> Optional[WpisBaterii]:
    """Wzór: firmware/prototyp/src/bateria.cpp parsujBaterie(): 5 pól, liczby całkowite, epoch > 0,
    0 < mV ≤ 6000, powód niepusty. None przy złym wierszu."""
    l = linia[:-1] if linia.endswith("\r") else linia
    p = l.split(";")
    if len(p) != 5:
        return None
    ep, mv, wf, ak = (_liczba_c(x) for x in p[:4])
    if ep is None or mv is None or wf is None or ak is None:
        return None
    if ep <= 0 or mv <= 0 or mv > 6000 or not p[4]:
        return None
    return WpisBaterii(ep, mv, wf, ak, p[4])


def parsuj_baterie(tekst: str) -> Tuple[List[WpisBaterii], List[str]]:
    wpisy: List[WpisBaterii] = []
    pominiete: List[str] = []
    for nr, linia in enumerate(tekst.split("\n"), 1):
        if not linia.strip():
            continue
        w = parsuj_linie_baterii(linia)
        if w is None:
            pominiete.append("linia {}".format(nr))
        else:
            wpisy.append(w)
    if not wpisy:
        raise BladPliku("W pliku nie ma żadnego poprawnego wiersza `epoch;mV;wifi_ms;aktywny_ms;powod` "
                        "(to nie wygląda na bateria.csv z panelu).")
    wpisy.sort(key=lambda x: x.ep)  # stabilnie, jak std::stable_sort w trendBaterii()
    # aktywny_ms wiersza i+1 to czas wybudzenia i; po starcie -1 (nieznany)
    for a, b in zip(wpisy, wpisy[1:]):
        if b.aktywny_ms >= 0 and b.powod != "start":
            a.aktywny_wlasny_ms = b.aktywny_ms
    return wpisy, pominiete


def rodzaj_powodu(powod: str) -> str:
    if powod in ("start", "przycisk"):
        return powod
    if powod == "zegar" or powod.startswith("zegar:"):
        return "zegar"
    return "inny"


def nazwa_powodu(powod: str) -> str:
    return "zegar: (bez powodu)" if powod == "zegar:" else powod


def mediana3(v: List[int]) -> List[int]:
    n = len(v)
    m = list(v)
    for i in range(1, n - 1):
        a, b, c = v[i - 1], v[i], v[i + 1]
        m[i] = max(min(a, b), min(max(a, b), c))
    return m


def ladowania(m: List[int]) -> List[int]:
    """Indeksy wpisów z wykrytym ładowaniem, jak bateria.cpp trendBaterii(): napięcie wygładzone rośnie i jest
    ≥ BAT_SKOK_LADOWANIA_MV powyżej minimum z BAT_OKNO_LADOWANIA poprzednich wpisów."""
    wynik = []
    for i in range(1, len(m)):
        mn = min(m[max(0, i - BAT_OKNO_LADOWANIA):i])
        if m[i] > m[i - 1] and m[i] - mn >= BAT_SKOK_LADOWANIA_MV:
            wynik.append(i)
    return wynik


def trend_baterii(w: List[WpisBaterii]) -> Dict:
    """Port bateria.cpp trendBaterii() z dodatkami: epizody ładowania, początek okna, dopasowanie na starcie okna."""
    t: Dict = {"jest": False, "n": 0, "ladowania": []}
    n = len(w)
    if not n:
        return t
    m = mediana3([x.mV for x in w])
    lad_idx = ladowania(m)
    # epizody: kolejne wykrycia bliżej niż okno ładowania to jedno ładowanie
    epizody: List[List[int]] = []
    for i in lad_idx:
        if epizody and i - epizody[-1][-1] <= BAT_OKNO_LADOWANIA:
            epizody[-1].append(i)
        else:
            epizody.append([i])
    for e in epizody:
        pocz = max(0, e[0] - BAT_OKNO_LADOWANIA)
        przed = min(m[pocz:e[0]]) if e[0] > 0 else m[0]
        k = e[-1]
        while k + 1 < n and m[k + 1] >= m[k]:
            k += 1
        t["ladowania"].append({"ep": w[e[0]].ep, "od_mV": przed, "do_mV": m[k], "szczyt_ep": w[k].ep})
    lad = lad_idx[-1] if lad_idx else -1
    k = 0 if lad < 0 else lad
    while k + 1 < n and m[k + 1] >= m[k]:
        k += 1
    if k + 1 >= n:
        od = 0 if lad < 0 else n - 1
    elif lad < 0 and k == 0:
        od = 0
    else:
        od = k + 1
    t["v"] = w[-1].mV / 1000.0
    t["n"] = n - od
    t["od_ep"] = w[od].ep
    okno = w[-1].ep - w[od].ep
    t["godzin"] = okno / 3600.0
    if okno < BAT_MIN_OKNO_S or t["n"] < 3:
        return t
    xs = [(w[i].ep - w[od].ep) / 86400.0 for i in range(od, n)]
    ys = [m[i] / 1000.0 for i in range(od, n)]
    r = regresja(xs, ys)
    if r is None:
        return t
    t["jest"] = True
    t["v_na_dobe"] = r["b"]
    t["v_start"] = r["a"]
    t["reszta_max_mV"] = r["reszta_max"] * 1000.0
    t["doby"] = okno / 86400.0
    nad = t["v"] - BAT_PROG_ROZLADOWANIA_MV / 1000.0
    t["dni_do_progu"] = (nad / -r["b"] if nad > 0 else 0.0) if r["b"] < -1e-4 else None
    return t


def strefa():
    try:
        from zoneinfo import ZoneInfo  # Python 3.9+
        return ZoneInfo(STREFA)
    except Exception:  # brak modułu albo bazy stref
        raise BladPliku("Brak bazy stref czasu dla {} (zoneinfo). Na Macu działa od razu; inaczej: "
                        "python3 -m pip install tzdata.".format(STREFA))


def doba_lokalna(ep: int, tz) -> dt.date:
    return dt.datetime.fromtimestamp(ep, tz).date()


def godzin_doby(d: dt.date, tz) -> float:
    a = dt.datetime(d.year, d.month, d.day, tzinfo=tz)
    nast = d + dt.timedelta(days=1)
    b = dt.datetime(nast.year, nast.month, nast.day, tzinfo=tz)
    return (b.timestamp() - a.timestamp()) / 3600.0


def analiza_baterii(wpisy: List[WpisBaterii], od: Optional[dt.date], do: Optional[dt.date]) -> Dict:
    tz = strefa()
    wsz = [x for x in wpisy if (od is None or doba_lokalna(x.ep, tz) >= od) and
           (do is None or doba_lokalna(x.ep, tz) <= do)]
    if not wsz:
        raise BladPliku("W wybranym okresie nie ma żadnego wpisu.")
    doby: Dict[dt.date, Dict] = {}
    for x in wsz:
        d = doba_lokalna(x.ep, tz)
        s = doby.setdefault(d, {"n": 0, "powody": {}, "z_wifi": 0, "wifi_ms": 0})
        s["n"] += 1
        s["powody"][x.powod] = s["powody"].get(x.powod, 0) + 1
        if x.wifi_ms > 0:
            s["z_wifi"] += 1
            s["wifi_ms"] += x.wifi_ms
    # niepełna: pierwsza albo ostatnia doba całego pliku (wycięcie --od/--do nie robi doby niepełną)
    pierwsza, ostatnia = doba_lokalna(wpisy[0].ep, tz), doba_lokalna(wpisy[-1].ep, tz)
    for d, s in doby.items():
        s["godzin"] = godzin_doby(d, tz)
        s["niepelna"] = d in (pierwsza, ostatnia)

    def stat(v: List[float]) -> Dict:
        return {"n": len(v), "min": min(v) if v else None, "mediana": mediana(v),
                "p90": percentyl(v, 90), "max": max(v) if v else None}

    akt: Dict[str, List[float]] = {"zegar": [], "przycisk": [], "start": [], "inny": [], "wifi": []}
    for x in wsz:
        if x.aktywny_wlasny_ms is None:
            continue
        s = x.aktywny_wlasny_ms / 1000.0
        if x.wifi_ms > 0:
            akt["wifi"].append(s)
        elif x.wifi_ms == 0:
            akt[rodzaj_powodu(x.powod)].append(s)
    wifi = [x.wifi_ms / 1000.0 for x in wsz if x.wifi_ms > 0]
    wifi_powod: Dict[str, List[float]] = {}
    for x in wsz:
        if x.wifi_ms > 0:
            wifi_powod.setdefault(x.powod, []).append(x.wifi_ms / 1000.0)
    return {
        "n": len(wsz), "od_ep": wsz[0].ep, "do_ep": wsz[-1].ep, "doby": doby,
        "aktywnosc": {k: stat(v) for k, v in akt.items()},
        "bez_wifi_ponad": sum(1 for s in akt["zegar"] if s > KRYTERIUM_BEZ_WIFI_S),
        "wifi": stat(wifi), "wifi_ponad": sum(1 for s in wifi if s > KRYTERIUM_WIFI_S),
        "wifi_powod": {k: stat(v) for k, v in wifi_powod.items()},
        "trend": trend_baterii(wsz), "tz": tz,
    }


def _s(x: Optional[float], miejsca: int = 1) -> str:
    """Sekundy z jednostką; brak wartości jako „–”."""
    return "–" if x is None else liczba(x, miejsca) + " s"


def raport_baterii(w: Dict, nazwa: str, pominiete: int, od: Optional[dt.date], do: Optional[dt.date]) -> str:
    tz = w["tz"]
    o: List[str] = []

    def czas(ep: int) -> str:
        return dt.datetime.fromtimestamp(ep, tz).strftime("%d.%m.%Y %H:%M")

    o.append("## Bateria i wybudzenia (`{}`)".format(nazwa))
    o.append("")
    o.append("| Co | Wartość |")
    o.append("|---|---|")
    okres = "cały plik" if od is None and do is None else "{} – {}".format(
        od.strftime("%d.%m.%Y") if od else "początek", do.strftime("%d.%m.%Y") if do else "koniec")
    o.append("| okres (doby lokalne {}) | {} |".format(STREFA, okres))
    o.append("| pierwszy i ostatni wpis | {} – {} |".format(czas(w["od_ep"]), czas(w["do_ep"])))
    o.append("| wpisów | {} (pominięte złe wiersze w całym pliku: {}) |".format(w["n"], pominiete))
    o.append("")

    o.append("### Doby")
    o.append("")
    o.append("| Doba | Godzin | Wybudzeń (≤ {} / ≤ {}) | Z Wi-Fi | Wi-Fi, min (budżet {} / limit {}) | Powody |".format(
        BUDZET_WYBUDZEN, LIMIT_WYBUDZEN_BEZ_DANYCH, calk(BUDZET_WIFI_MIN), calk(LIMIT_WIFI_BEZ_DANYCH_MIN)))
    o.append("|---|---|---|---|---|---|")
    for d in sorted(w["doby"]):
        s = w["doby"][d]
        wmin = s["wifi_ms"] / 60000.0
        ocena = "w budżecie" if wmin <= BUDZET_WIFI_MIN else (
            "ponad budżet, w limicie doby bez danych" if wmin <= LIMIT_WIFI_BEZ_DANYCH_MIN else "ponad limit")
        powody = ", ".join("{} {}".format(nazwa_powodu(p), c) for p, c in
                           sorted(s["powody"].items(), key=lambda pc: (-pc[1], pc[0])))
        o.append("| {}{} | {} | {} | {} | {} ({}) | {} |".format(
            d.strftime("%d.%m.%Y"), " (niepełna)" if s["niepelna"] else "", calk(s["godzin"]), s["n"],
            s["z_wifi"], liczba(wmin, 1), ocena, powody))
    o.append("")
    o.append("- „Niepełna”: pierwsza albo ostatnia doba w danych, liczby nie obejmują całej doby. Która doba "
             "była bez danych (limit 45 min Wi-Fi, ≤ 110 wybudzeń), plik nie mówi: ocena wobec obu progów.")
    o.append("")

    o.append("### Czas aktywności i Wi-Fi na wybudzenie")
    o.append("")
    o.append("| Grupa | n | min | mediana | P90 | max | Kryterium |")
    o.append("|---|---|---|---|---|---|---|")
    a = w["aktywnosc"]
    wiersze = [
        ("aktywność, zegar bez Wi-Fi (pomiar 2)", a["zegar"],
         "≤ {} s: ponad {} z {}".format(calk(KRYTERIUM_BEZ_WIFI_S), w["bez_wifi_ponad"], a["zegar"]["n"])),
        ("aktywność, przycisk bez Wi-Fi (z czuwaniem)", a["przycisk"], "informacyjnie"),
        ("aktywność, start bez Wi-Fi", a["start"], "informacyjnie"),
        ("aktywność, wybudzenia z Wi-Fi", a["wifi"], "informacyjnie"),
        ("Wi-Fi na wybudzenie (pomiar 3)", w["wifi"],
         "≤ {} s (T-NF-05): ponad {} z {}".format(calk(KRYTERIUM_WIFI_S), w["wifi_ponad"], w["wifi"]["n"])),
    ]
    if a["inny"]["n"]:
        wiersze.insert(3, ("aktywność, inny powód bez Wi-Fi", a["inny"], "informacyjnie"))
    for nazwa_g, st, kryt in wiersze:
        o.append("| {} | {} | {} | {} | {} | {} | {} |".format(
            nazwa_g, st["n"], _s(st["min"]), _s(st["mediana"]), _s(st["p90"]), _s(st["max"]), kryt))
    o.append("")
    o.append("- Czas aktywności wybudzenia pochodzi z pola `aktywny_ms` następnego wiersza (bateria.h); ostatnie "
             "wybudzenie w pliku i wybudzenie przed startem nie mają czasu. P90: metoda najbliższej rangi.")
    o.append("- T-NF-05 dotyczy wybudzeń z Wi-Fi bez nowych danych; plik nie odróżnia ich od importu nocy, więc "
             "wybudzenia z importem mogą przekraczać 8 s zgodnie z planem. Podział według powodu niżej.")
    o.append("")
    if w["wifi_powod"]:
        o.append("| Powód (z Wi-Fi) | n | Wi-Fi mediana | P90 | max |")
        o.append("|---|---|---|---|---|")
        for p, st in sorted(w["wifi_powod"].items(), key=lambda ps: (-ps[1]["n"], ps[0])):
            o.append("| {} | {} | {} | {} | {} |".format(nazwa_powodu(p), st["n"], _s(st["mediana"]),
                                                               _s(st["p90"]), _s(st["max"])))
        o.append("")

    o.append("### Napięcie baterii `[szacunek]`")
    o.append("")
    t = w["trend"]
    if t["ladowania"]:
        o.append("| Ładowanie wykryte | Napięcie przed (minimum z 12 wpisów) | Szczyt | Szczyt o |")
        o.append("|---|---|---|---|")
        for l in t["ladowania"]:
            o.append("| {} | {} V | {} V | {} |".format(czas(l["ep"]), liczba(l["od_mV"] / 1000.0, 3),
                                                        liczba(l["do_mV"] / 1000.0, 3), czas(l["szczyt_ep"])))
        o.append("")
    else:
        o.append("- Ładowania nie wykryto (wzrost ≥ {} mV względem minimum z {} poprzednich wpisów, jak "
                 "`bateria.cpp`).".format(BAT_SKOK_LADOWANIA_MV, BAT_OKNO_LADOWANIA))
    o.append("- Napięcia w tej sekcji są wygładzone medianą z 3 sąsiednich wpisów, jak w `bateria.cpp`.")
    o.append("")
    if t.get("jest"):
        o.append("| Co | Wartość |")
        o.append("|---|---|")
        o.append("| odcinek bez ładowania | od {}, {} doby, {} wpisów |".format(
            czas(t["od_ep"]), liczba(t["doby"], 2), t["n"]))
        o.append("| ostatnie napięcie | {} V |".format(liczba(t["v"], 3)))
        o.append("| spadek (regresja na medianie z 3 wpisów) | {} V/dobę |".format(liczba(t["v_na_dobe"], 4)))
        o.append("| największa reszta regresji | {} mV |".format(calk(t["reszta_max_mV"])))
        if t["dni_do_progu"] is None:
            o.append("| dni do {} V | brak spadku |".format(liczba(BAT_PROG_ROZLADOWANIA_MV / 1000.0, 1)))
        else:
            razem = t["doby"] + t["dni_do_progu"]
            o.append("| dni od ostatniego wpisu do {} V | {} `[szacunek]` |".format(
                liczba(BAT_PROG_ROZLADOWANIA_MV / 1000.0, 1), liczba(t["dni_do_progu"], 1)))
            o.append("| razem od początku odcinka | {} dni wobec celu {} dni (R-NF-01): {} `[szacunek]` |".format(
                liczba(razem, 1), CEL_DNI, "powyżej celu" if razem >= CEL_DNI else "poniżej celu"))
        o.append("")
    else:
        o.append("- Za krótki odcinek bez ładowania na trend: {} h, {} wpisów (potrzeba ≥ 24 h i ≥ 3 wpisów).".format(
            liczba(t.get("godzin", 0.0), 1), t.get("n", 0)))
    o.append("- Zastrzeżenie: krzywa napięcia baterii litowo-jonowej nie jest liniowa (płaska w środku, stroma "
             "pod koniec), a krzywej dla tej baterii jeszcze nie ma (pomiar 10 nie zrobiony). Ekstrapolacja "
             "liniowa do {} V to `[szacunek]`, nie pomiar pojemności; z danych z początku rozładowania zwykle "
             "zawyża liczbę dni.".format(liczba(BAT_PROG_ROZLADOWANIA_MV / 1000.0, 1)))
    o.append("")
    return "\n".join(o)


# =====================================================================================================
# wywołanie
# =====================================================================================================

class Parser(argparse.ArgumentParser):
    def error(self, message):  # kod 2 i komunikat po polsku zamiast angielskiego z argparse
        sys.stderr.write("Błąd argumentów: {}\n\n{}".format(message, self.format_usage()))
        sys.exit(KOD_BLEDU)


def data_arg(s: str) -> dt.date:
    try:
        return dt.date.fromisoformat(s)
    except ValueError:
        raise argparse.ArgumentTypeError("data ma mieć postać RRRR-MM-DD, jest: {}".format(s))


def main(argv: Optional[List[str]] = None) -> int:
    p = Parser(prog="analiza.py", description="Liczby z plików pomiarowych panelu (spec techniczna 2.4).")
    sub = p.add_subparsers(dest="tryb", parser_class=Parser)
    pd = sub.add_parser("dryf", help="dryf.csv z pomiaru 12")
    pd.add_argument("sciezka")
    pb = sub.add_parser("bateria", help="bateria.csv z panelu")
    pb.add_argument("sciezka")
    pb.add_argument("--od", type=data_arg, help="pierwsza doba lokalna RRRR-MM-DD (włącznie)")
    pb.add_argument("--do", type=data_arg, help="ostatnia doba lokalna RRRR-MM-DD (włącznie)")
    a = p.parse_args(argv)
    if not a.tryb:
        p.error("podaj tryb: dryf albo bateria")
    try:
        tekst = czytaj_tekst(a.sciezka)
        nazwa = os.path.basename(a.sciezka)
        if a.tryb == "dryf":
            wiersze, komentarze, pominiete = parsuj_dryf(tekst)
            wynik = analiza_dryfu(wiersze, komentarze)
            out = raport_dryfu(wynik, nazwa)
            if pominiete:
                out += "Pominięte linie ({}): {}.\n".format(len(pominiete), "; ".join(pominiete[:10]) +
                                                           ("; …" if len(pominiete) > 10 else ""))
        else:
            if a.od and a.do and a.od > a.do:
                raise BladPliku("Data --od jest późniejsza niż --do.")
            wpisy, pominiete_b = parsuj_baterie(tekst)
            wynik = analiza_baterii(wpisy, a.od, a.do)
            out = raport_baterii(wynik, nazwa, len(pominiete_b), a.od, a.do)
    except BladPliku as e:
        sys.stderr.write("Błąd: {}\n".format(e))
        return KOD_BLEDU
    sys.stdout.write(out)
    if not out.endswith("\n"):
        sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
