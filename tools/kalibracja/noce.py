#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Noce terapii z kopii karty SD aparatu ResMed (etap 2, kalibracja, część A).

Czyta pliki EDF z karty (STR.edf i DATALOG/RRRRMMDD/*.edf) i składa z nich
jeden rekord na noc: pola raportu nocy AirBridge z Załącznika A specyfikacji
oraz metryki M-01…M-21 według rozdz. 4.3 i 4.11, a w trybie ASV także M-06p (EPAP P95),
M-28 i M-28p (wsparcie PS, mediana i P95). Nie ocenia reguł (zdarzeń E,
wzorców L, hipotez H, odkryć O, ekranów S-K): to etap B.

Wzór definicji: AirBridge v2.3.0, `src/edf_report.cpp` (raport nocy liczony
z tych samych plików). Odstępstwa i domysły opisuje README.md w tym katalogu.

Użycie (na Macu, na kopii karty, wynik poza repozytorium):
  python3 -I tools/kalibracja/noce.py KARTA_DIR --od RRRR-MM-DD --do RRRR-MM-DD \\
      --wyjscie ~/Documents/noce.csv

Daty --od i --do to daty poranka (jak na ekranie panelu, 4.1).
Skrypt otwiera pliki karty tylko do odczytu i nigdy nie pisze do katalogu karty.
Tylko biblioteka standardowa Pythona 3.
"""

import argparse
import csv
import datetime as _dt
import os
import re
import sys
from collections import Counter

_TU = os.path.dirname(os.path.abspath(__file__))
if _TU not in sys.path:          # python3 -I nie dodaje katalogu skryptu do sys.path
    sys.path.insert(0, _TU)

import edf  # noqa: E402

REPO = os.path.dirname(os.path.dirname(_TU))
PLIK_PROGOW = os.path.join(REPO, "docs", "spec", "progi.csv")

# Próg AirBridge dla `leak_high_seconds` (stała LEAK_THRESHOLD w edf_report.cpp).
# To definicja pola M-08, nie próg reguły: firmware liczy zawsze przy 24 L/min.
PRZECIEK_AIRBRIDGE = 24.0

# id kanału, etykieta sygnału, plik, jednostka wyniku (jak CHANNELS w edf_report.cpp)
KANALY = [
    ("Press", "Press.2s", "PLD", "cmH2O"),
    ("EprPress", "EprPress.2s", "PLD", "cmH2O"),
    ("MaskPress", "MaskPress.2s", "PLD", "cmH2O"),
    ("Leak", "Leak.2s", "PLD", "L/min"),
    ("MinVent", "MinVent.2s", "PLD", "L/min"),
    ("RespRate", "RespRate.2s", "PLD", "bpm"),
    ("TidVol", "TidVol.2s", "PLD", "mL"),
    ("SpO2", "SpO2.1s", "SAD", "%"),
    ("Pulse", "Pulse.1s", "SAD", "bpm"),
    ("FlowLim", "FlowLim.2s", "PLD", ""),
    ("Snore", "Snore.2s", "PLD", ""),
    ("TgtVent", "TgtVent.2s", "PLD", "L/min"),
]
ID_KANALOW = [k[0] for k in KANALY]

# Zdarzenia z plików EVE i CSL (EventKind w report_edf.cpp)
RODZAJ_ZDARZENIA = {
    "Obstructive Apnea": "oa",
    "Central Apnea": "ca",
    "Apnea": "ua",
    "Hypopnea": "h",
    "Arousal": "rera",
    "RERA": "rera",
}
CSR_START, CSR_KONIEC = "CSR Start", "CSR End"

# Tagi ustawień → sygnał STR.edf (STR_SIGNALS i STR_TAGS w air10_edf.cpp)
TAG_STR = {
    "MOP": "Mode", "IPC": "S.C.Press", "STP": "S.C.StartPress",
    "MPA": "S.AS.MaxPress", "MPI": "S.AS.MinPress", "STU": "S.AS.StartPress", "AFC": "S.AS.Comfort",
    "IPP": "S.BL.IPAP", "EPP": "S.BL.EPAP",
    "MXI": "S.VA.MaxIPAP", "MNE": "S.VA.MinEPAP", "SPT": "S.VA.PS",
    "EEP": "S.AV.EPAP", "MNS": "S.AV.MinPS", "MXS": "S.AV.MaxPS", "STE": "S.AV.StartPress",
    "EAI": "S.AA.MinEPAP", "EAX": "S.AA.MaxEPAP", "ANS": "S.AA.MinPS", "AXS": "S.AA.MaxPS",
    "EAS": "S.AA.StartPress",
    "RMA": "S.RampEnable", "RMT": "S.RampTime", "EPA": "S.EPR.ClinEnable",
    "EPX": "S.EPR.EPREnable", "EPR": "S.EPR.Level", "EPT": "S.EPR.EPRType",
    "SST": "S.SmartStart", "MSK": "S.Mask", "TBT": "S.Tube", "CCO": "S.ClimateControl",
    "HMX": "S.HumEnable", "HMS": "S.HumLevel", "HTX": "S.TempEnable", "HTS": "S.Temp",
}
# Opcje tagów wyliczeniowych (VAR_CATALOG w settings_defs.h)
OPCJE = {
    "MOP": "CPAP,AutoSet,APAP,S,ST,T,VAuto,ASV,ASVAuto,iVAPS,PAC,AutoSet Her",
    "AFC": "Standard,Soft", "RMA": "Off,On,Auto", "EPA": "Off,On", "EPX": "Off,On",
    "EPT": "Ramp Only,Full Time", "SST": "Off,On", "MSK": "Pillows,Full Face,Nasal,Pediatric",
    "TBT": "SlimLine,Standard,3m", "CCO": "Auto,Manual", "HMX": "Off,On", "HTX": "Off,On,Auto",
}
# Kolejność tagów w `settings.fields[]` (MODE_LAYOUT, COMFORT_LAYOUT, EPR_VARS, ACCESSORY_VARS)
UKLAD_TRYBU = [
    ["IPC"], ["MPA", "MPI"], ["MPA", "MPI"],
    ["IPP", "EPP", "BKR", "EBE", "ITN", "ITX", "RST", "VTS", "VCS"],
    ["IPP", "EPP", "RRT", "ITN", "ITX", "RST", "VTS", "VCS"],
    ["IPP", "EPP", "RRT", "ITT", "RST"],
    ["MXI", "MNE", "SPT", "ITN", "ITX", "VTS", "VCS"],
    ["EEP", "MNS", "MXS"],
    ["EAI", "EAX", "ANS", "AXS"],
    ["PHT", "IBR", "WMV", "EPI", "WPM", "WPA", "ITN", "ITX", "RST", "VTS", "VCS"],
    ["IPP", "EPP", "RRT", "ITT", "RST", "VTS"],
    ["MPA", "MPI"],
]
UKLAD_KOMFORTU = [["STP"], ["STU", "AFC"], ["STU", "AFC"], ["EPS"], ["EPS"], ["EPS"], ["STV"],
                  ["STE"], ["EAS"], ["IVS"], ["EPS"], ["STU", "AFC"]]
TAGI_EPR = ["RMA", "RMT", "EPR", "EPA", "EPX", "EPT"]
TAGI_AKCESORIOW = ["MSK", "TBT", "CCO", "HMS", "HMX", "HTF", "HTS", "HTX"]

# Największa różnica prefiksów czasu plików BRP, PLD i SAD tej samej sesji (domysł:
# na karcie AirCurve widać różnice 1 s; AirBridge wymaga identycznego prefiksu, bo sam
# zapisuje komplet plików, edf_recorder.cpp: session_files_complete_at).
TOLERANCJA_SESJI_S = 10

PLIK_RE = re.compile(r"^(\d{8}_\d{6})_(BRP|PLD|SAD|EVE|CSL)\.edf$", re.IGNORECASE)
DZIEN_RE = re.compile(r"^(\d{4})(\d{2})(\d{2})$")
EPOKA_DATA = _dt.date(1970, 1, 1)


# ---------------------------------------------------------------------------
# Progi z docs/spec/progi.csv
# ---------------------------------------------------------------------------
def wczytaj_progi(sciezka=PLIK_PROGOW):
    with open(sciezka, encoding="utf-8", newline="") as f:
        return {w["id_progu"]: w for w in csv.DictReader(f)}


def _godzina_na_minuty_od_poludnia(tekst):
    g, m = (int(x) for x in tekst.split(":"))
    return (g * 60 + m - 12 * 60) % 1440


class Parametry:
    """Progi definicji nocy potrzebne loaderowi (bez progów reguł oceny)."""

    def __init__(self, progi=None):
        p = progi if progi is not None else wczytaj_progi()

        def w(id_):          # wartość z rozwiązaniem odwołania „=id_progu”
            v = p[id_]["wartosc"]
            return p[v[1:]]["wartosc"] if v.startswith("=") else v
        self.noc_wazna_s = float(w("R-DANE-13.m01_min")) * 3600
        self.start_nocy_min = _godzina_na_minuty_od_poludnia(w("R-DANE-27.start_nocy"))
        self.wczesny_sen_s = float(w("R-DANE-27.wczesny_sen_min")) * 3600
        self.przeciek_bardzo_duzy = float(w("E-23.przeciek_p95"))  # 27 L/min,
        # Pole `tryb_obslugiwany` w tabeli nocy znaczy „noc w trybie strojenia” (WY3): od ADR-0030
        # wydanie ocenia cztery tryby (R-DANE-29.tryb_obslugiwany), ale noce strojenia są tylko w ASVAuto.
        self.tryb_obslugiwany = w("WY3.tryb_strojenia")


# ---------------------------------------------------------------------------
# Pomocnicze
# ---------------------------------------------------------------------------
def dzien_z_daty(d):
    return (d - EPOKA_DATA).days


def data_z_dnia(n):
    return EPOKA_DATA + _dt.timedelta(days=n)


def _dzien_katalogu(nazwa):
    m = DZIEN_RE.match(nazwa)
    if not m:
        return None
    try:
        return dzien_z_daty(_dt.date(int(m.group(1)), int(m.group(2)), int(m.group(3))))
    except ValueError:
        return None


def _skala(id_kanalu, jednostka):
    if id_kanalu == "Leak" and jednostka == "L/s":
        return 60.0
    if id_kanalu == "TidVol" and jednostka == "L":
        return 1000.0
    return 1.0


def _liczba_pl(v, miejsca=1):
    if v is None:
        return "–"
    s = "%.*f" % (miejsca, v)
    if "." in s:
        s = s.rstrip("0").rstrip(".")
    return s.replace(".", ",")


def _godzina(minuty_od_poludnia):
    if minuty_od_poludnia is None:
        return None
    m = int(minuty_od_poludnia // 1) + 12 * 60      # godzina zegarowa: pełne minuty, bez zaokrąglania w górę
    return "%02d:%02d" % ((m // 60) % 24, m % 60)


def _fmt8g(v):
    return format(v, ".8g")


# ---------------------------------------------------------------------------
# STR.edf: ustawienia i statystyki dnia
# ---------------------------------------------------------------------------
class Str:
    """Rekordy dzienne STR.edf: {dzień: {etykieta: wartość fizyczna albo None}}."""

    def __init__(self, sciezka):
        self.dni = {}
        self.jednostki = {}
        self.blad = None
        if not os.path.isfile(sciezka):
            self.blad = "brak STR.edf"
            return
        try:
            plik = edf.PlikEDF(sciezka)
        except (edf.BladEDF, OSError) as e:
            self.blad = "STR.edf nieczytelny: %s" % e
            return
        wartosci = {}
        for s in plik.sygnaly:
            if s.crc or s.adnotacje:
                continue
            self.jednostki[s.etykieta] = s.jednostka
            wartosci[s.etykieta] = (s, plik.surowe(s, bez_znaku=True))
        start_dnia = plik.start_s // 86400
        for r in range(plik.rekordy):
            rek = {}
            for etykieta, (s, surowe) in wartosci.items():
                if s.probki != 1:
                    continue
                raw = surowe[r][0]
                if raw == 0xFFFF:                       # brak wartości (AirBridge)
                    rek[etykieta] = None
                    continue
                cyf = raw - 65536 if (s.cyf_min < 0 and raw >= 32768) else raw
                rek[etykieta] = s.fizyczna(cyf) if s.poprawna(cyf) else None
            data = rek.get("Date")
            dzien = int(round(data)) if data is not None else start_dnia + r
            obecny = rek.get("Mode") is not None or rek.get("Duration") is not None
            if obecny:
                self.dni[dzien] = rek

    def ustawienia(self, dzien):
        """`settings` jak w raporcie AirBridge: (available, fields[])."""
        rek = self.dni.get(dzien)
        if rek is None:
            return False, []
        pola = []

        def dodaj(tag):
            etykieta = TAG_STR.get(tag)
            if etykieta is None or etykieta not in rek:
                return
            v = rek[etykieta]
            if v is None:
                return
            opcja = None
            tekst = _fmt8g(v)
            if tag in OPCJE and v >= 0 and v == int(v):
                lista = OPCJE[tag].split(",")
                if int(v) < len(lista):
                    opcja = int(v)
                    tekst = lista[opcja]
            pola.append({"tag": tag, "value": tekst, "option": opcja, "liczba": v})

        dodaj("MOP")
        tryb = rek.get("Mode")
        if tryb is not None and tryb == int(tryb) and 0 <= int(tryb) < len(UKLAD_TRYBU):
            for tag in UKLAD_TRYBU[int(tryb)] + UKLAD_KOMFORTU[int(tryb)]:
                dodaj(tag)
        for tag in TAGI_EPR + ["SST"] + TAGI_AKCESORIOW:
            dodaj(tag)
        return True, pola

    def statystyka(self, dzien, id_kanalu, przyrostek):
        """Zapas ze STR, gdy kanału nie ma w plikach EDF (`source = "str"`)."""
        rek = self.dni.get(dzien)
        if rek is None:
            return None
        baza = {"Press": "TgtIPAP", "EprPress": "TgtEPAP"}.get(id_kanalu, id_kanalu)
        etykieta = "%s.%s" % (baza, przyrostek)
        if etykieta not in rek or rek[etykieta] is None:
            return None
        jedn = self.jednostki.get(etykieta, "")
        oczekiwana = dict((k[0], k[3]) for k in KANALY)[id_kanalu]
        skala = _skala(id_kanalu, jedn)
        if skala == 1.0 and jedn != oczekiwana:
            return None
        return rek[etykieta] * skala


# ---------------------------------------------------------------------------
# Jeden dzień terapii z DATALOG
# ---------------------------------------------------------------------------
def _otworz(sciezka, bledy):
    """Otwiera plik EDF; błąd odczytu albo plik niepełny dopisuje do `bledy` (noc częściowa)."""
    try:
        p = edf.PlikEDF(sciezka)
    except (edf.BladEDF, OSError, ValueError) as e:
        bledy.append("%s: %s" % (os.path.basename(sciezka), e))
        return None
    if p.niepelny:
        bledy.append("%s: plik niepełny" % os.path.basename(sciezka))
    if p.edf_plus_d and any(not s.adnotacje and not s.crc for s in p.sygnaly):
        # EDF+D z sygnałami: rekordy nieciągłe, czytnik zakłada ciągłość (czasy byłyby błędne)
        bledy.append("%s: EDF+D z sygnałami pomiarowymi (rekordy nieciągłe)" % os.path.basename(sciezka))
    return p


def _granice_zapisu(pld):
    """Pierwszy i ostatni rekord z poprawną próbką ciśnienia (recorded_bounds)."""
    sygn = None
    for nazwa in ("Press.2s", "EprPress.2s", "MaskPress.2s"):
        sygn = pld.sygnal(nazwa)
        if sygn is not None:
            break
    if sygn is None:
        return None
    pierwszy = ostatni = None
    for r, probki in enumerate(pld.rekordy_probek(sygn)):
        if any(sygn.poprawna(x) for x in probki):
            if pierwszy is None:
                pierwszy = r
            ostatni = r
    if pierwszy is None:
        return None
    return (pld.start_s + pierwszy * pld.czas_rekordu, pld.start_s + (ostatni + 1) * pld.czas_rekordu)


def _ma_wykres(plik, rodzaj):
    if rodzaj == "BRP":
        return plik.sygnal("Flow.40ms") is not None
    return any(plik.sygnal(k[1]) is not None for k in KANALY if k[2] == rodzaj)


def _pary_csr(zdarzenia):
    """Pary CSR Start/End w kolejności z pliku (CsrPairing). Zwraca (przedziały, czy_kompletne)."""
    przedzialy, start, kompletne = [], None, True
    for t, tekst in zdarzenia:
        if tekst == CSR_START:
            if start is not None:
                kompletne = False
            start = t
        elif tekst == CSR_KONIEC:
            if start is not None and t > start:
                przedzialy.append((start, t))
            else:
                kompletne = False
            start = None
    if start is not None:
        kompletne = False
    return przedzialy, kompletne


def noc_z_katalogu(katalog, dzien, str_, par):
    problemy = []      # błędy plików: noc częściowa
    uwagi = []         # informacje bez wpływu na `partial`
    # Pliki aparatu jednej sesji mają prefiksy czasu różne o kilka sekund (np. BRP 02:52:57,
    # PLD i SAD 02:52:58, EVE 02:52:52). Pliki BRP, PLD i SAD łączymy w sesję po czasie
    # z prefiksu: plik dołącza do sesji, która nie ma jeszcze pliku tego rodzaju i zaczęła się
    # najwyżej TOLERANCJA_SESJI_S wcześniej. EVE i CSL nie należą do sesji: ich zdarzenia
    # przypisujemy po czasie (niżej).
    numeryczne, eve_pliki, csl_pliki = [], [], []
    for nazwa in sorted(os.listdir(katalog)):
        m = PLIK_RE.match(nazwa)
        if not m:
            continue
        rodzaj, sciezka = m.group(2).upper(), os.path.join(katalog, nazwa)
        if rodzaj == "EVE":
            eve_pliki.append(sciezka)
        elif rodzaj == "CSL":
            csl_pliki.append(sciezka)
        else:
            try:
                t = _dt.datetime.strptime(m.group(1), "%Y%m%d_%H%M%S")
            except ValueError:
                uwagi.append("%s: zła nazwa pliku" % nazwa)
                continue
            numeryczne.append((t, m.group(1), rodzaj, sciezka))
    grupy = []
    for t, prefiks, rodzaj, sciezka in sorted(numeryczne):
        for g in grupy:
            if rodzaj not in g["pliki"] and 0 <= (t - g["t0"]).total_seconds() <= TOLERANCJA_SESJI_S:
                g["pliki"][rodzaj] = sciezka
                break
        else:
            grupy.append({"t0": t, "prefiks": prefiks, "pliki": {rodzaj: sciezka}})
    sesje = [g for g in grupy if "BRP" in g["pliki"] or "PLD" in g["pliki"]]
    sad_luzem = [g["pliki"]["SAD"] for g in grupy if g not in sesje]

    wartosci = {k: Counter() for k in ID_KANALOW}     # wartość fizyczna -> liczba próbek
    wsparcie = Counter()                              # PS = Press − EprPress, próbka po próbce (M-28)
    przeciek_wysoki = 0.0
    przeciek_bardzo_duzy = 0.0
    ma_przeciek = False
    brak_pld = False
    wszystkie_uzycia_znane = True
    gotowe_sesje = []

    def zbierz_kanaly(plik, rodzaj):
        nonlocal przeciek_wysoki, przeciek_bardzo_duzy, ma_przeciek
        for id_k, etykieta, plik_k, _ in KANALY:
            if plik_k != rodzaj:
                continue
            s = plik.sygnal(etykieta)
            if s is None:
                continue
            skala = _skala(id_k, s.jednostka)
            krok = plik.czas_rekordu / s.probki
            licznik = wartosci[id_k]
            for x, ile in plik.zliczenia(s).items():
                if not s.poprawna(x):
                    continue
                v = s.fizyczna(x, skala)
                licznik[v] += ile
                if id_k == "Leak":
                    ma_przeciek = True
                    if v > PRZECIEK_AIRBRIDGE:
                        przeciek_wysoki += krok * ile
                    if v > par.przeciek_bardzo_duzy:
                        przeciek_bardzo_duzy += krok * ile

    def zbierz_wsparcie(plik):
        """PS (M-28, M-28p) z PLD: `Press.2s` − `EprPress.2s` z tych samych chwil, tylko gdy obie
        próbki są poprawne. W ASVAuto `Press.2s` to IPAP, `EprPress.2s` to EPAP."""
        sp, se = plik.sygnal("Press.2s"), plik.sygnal("EprPress.2s")
        if sp is None or se is None:
            return
        if sp.probki != se.probki:
            uwagi.append("%s: Press.2s i EprPress.2s z różną liczbą próbek, PS nieliczone"
                            % os.path.basename(plik.sciezka))
            return
        pary = Counter()
        for rp, re_ in zip(plik.rekordy_probek(sp), plik.rekordy_probek(se)):
            pary.update(zip(rp, re_))
        for (a, b), ile in pary.items():
            if sp.poprawna(a) and se.poprawna(b):
                wsparcie[round(sp.fizyczna(a) - se.fizyczna(b), 6)] += ile

    for ses in sesje:
        g = ses["pliki"]
        zakres = []
        zapis = None
        uzycie_znane = False
        for rodzaj in ("BRP", "PLD", "SAD"):
            if rodzaj not in g:
                if rodzaj == "PLD":
                    brak_pld = True
                continue
            plik = _otworz(g[rodzaj], problemy)
            if plik is None:
                if rodzaj == "PLD":
                    brak_pld = True
                continue
            if _ma_wykres(plik, rodzaj) and plik.rekordy and plik.czas_rekordu > 0:
                zakres.append((plik.start_s, plik.koniec_s))
            if rodzaj == "PLD":
                zapis = _granice_zapisu(plik)
                uzycie_znane = plik.rekordy == 0 or zapis is not None
            if rodzaj in ("PLD", "SAD"):
                zbierz_kanaly(plik, rodzaj)
            if rodzaj == "PLD":
                zbierz_wsparcie(plik)
        wszystkie_uzycia_znane &= uzycie_znane
        gotowe_sesje.append({
            "prefiks": ses["prefiks"],
            "start_s": min(a for a, _ in zakres) if zakres else None,
            "koniec_s": max(b for _, b in zakres) if zakres else None,
            "zapis": zapis,
        })

    # SAD z innym prefiksem niż BRP/PLD: dołączamy do sesji, z którą nachodzi w czasie
    for sciezka in sad_luzem:
        plik = _otworz(sciezka, problemy)
        if plik is None:
            continue
        a, b = plik.start_s, plik.koniec_s
        najlepsza, wspolne = None, 0
        for s in gotowe_sesje:
            if s["start_s"] is None:
                continue
            w = min(b, s["koniec_s"]) - max(a, s["start_s"])
            if w > wspolne:
                najlepsza, wspolne = s, w
        if najlepsza is None:
            uwagi.append("%s: SAD bez sesji" % os.path.basename(sciezka))
            continue
        zbierz_kanaly(plik, "SAD")
        if _ma_wykres(plik, "SAD"):
            najlepsza["start_s"] = min(najlepsza["start_s"], a)
            najlepsza["koniec_s"] = max(najlepsza["koniec_s"], b)

    zapisy = [s["zapis"] for s in gotowe_sesje if s["zapis"]]
    sekundy = sum(b - a for a, b in zapisy)

    # Zdarzenia: wszystkie pliki EVE dnia, bez powtórzeń; liczone, gdy czas
    # zdarzenia (onset: chwila zapisu zdarzenia; AirBridge traktuje ją jako koniec zdarzenia,
    # początek = onset − czas trwania) leży w czasie zapisu którejś sesji [od, do).
    zdarzenia = set()
    eve_ok = bool(eve_pliki)
    for sciezka in eve_pliki:
        plik = _otworz(sciezka, problemy)
        if plik is None:
            eve_ok = False
            continue
        try:
            for onset, trwanie, tekst in plik.adnotacje():
                zdarzenia.add((round(plik.start_s + onset, 3), trwanie, tekst))
        except (edf.BladEDF, ValueError) as e:
            problemy.append("%s: %s" % (os.path.basename(sciezka), e))
            eve_ok = False
    # Pliki EVE bez rodzaju bezdechu (tylko „Apnea”, np. karta SD AirCurve) nie mają
    # „Obstructive Apnea” ani „Central Apnea”: wtedy OA i CA są nieznane, nie zerowe.
    klasyfikacja = any(tekst in ("Obstructive Apnea", "Central Apnea") for _, _, tekst in zdarzenia)
    liczniki = Counter()
    inne = Counter()
    for t, _, tekst in sorted(zdarzenia):
        rodzaj = RODZAJ_ZDARZENIA.get(tekst)
        if rodzaj is None:
            if tekst not in (CSR_START, CSR_KONIEC):
                inne[tekst] += 1
            continue
        if any(a <= t < b for a, b in zapisy):
            liczniki[rodzaj] += 1

    # Oddychanie okresowe: pary CSR Start/End z plików CSL (zapasowo z EVE),
    # przycięte do czasu zapisu sesji.
    przedzialy, csr_kompletne, csr_zrodlo = [], True, None
    zrodla_csr = []
    for sciezka in csl_pliki:
        plik = _otworz(sciezka, problemy)
        if plik is None:
            csr_kompletne = False
            continue
        try:
            zrodla_csr.append([(plik.start_s + on, tekst) for on, _, tekst in plik.adnotacje()])
            csr_zrodlo = "CSL"
        except (edf.BladEDF, ValueError) as e:
            problemy.append("%s: %s" % (os.path.basename(sciezka), e))
            csr_kompletne = False
    if csr_zrodlo is None:
        z_eve = sorted((t, tekst) for t, _, tekst in zdarzenia if tekst in (CSR_START, CSR_KONIEC))
        if z_eve:
            zrodla_csr.append(z_eve)
            csr_zrodlo = "EVE"
    for lista in zrodla_csr:
        p, k = _pary_csr(lista)
        przedzialy += p
        csr_kompletne &= k
    csr_sekundy = 0.0
    for a, b in przedzialy:
        for za, zb in zapisy:
            w = min(b, zb) - max(a, za)
            if w > 0:
                csr_sekundy += w

    sesje_obecne = len(gotowe_sesje) > 0
    events_known = eve_ok and wszystkie_uzycia_znane and sekundy > 0
    used = sekundy if (wszystkie_uzycia_znane and sekundy > 0) else None
    partial = (brak_pld or (sesje_obecne and not eve_pliki) or bool(problemy))
    dostepne, pola = str_.ustawienia(dzien)

    rek = {
        "day": dzien,
        "str_duration_min": (str_.dni.get(dzien) or {}).get("Duration"),
        "data_poranka": data_z_dnia(dzien + 1).isoformat(),
        "na_liscie": True,
        "present": sesje_obecne or dostepne,
        "partial": partial,
        "used_seconds": used,
        "events_known": events_known,
        "counts": {k: (liczniki[k] if events_known and (klasyfikacja or k not in ("oa", "ca")) else None)
                   for k in ("oa", "ca", "ua", "h", "rera")},
        "klasyfikacja_bezdechow": klasyfikacja if events_known else None,
        "csr_dostepne": csr_zrodlo is not None,
        "ahi": ((liczniki["oa"] + liczniki["ca"] + liczniki["ua"] + liczniki["h"]) * 3600.0 / sekundy
                if events_known else None),
        "rdi": (sum(liczniki[k] for k in ("oa", "ca", "ua", "h", "rera")) * 3600.0 / sekundy
                if events_known else None),
        "csr_seconds": (csr_sekundy if (csr_zrodlo and csr_kompletne and wszystkie_uzycia_znane
                                        and sekundy > 0) else None),
        "csr_zrodlo": csr_zrodlo,
        "leak_threshold": PRZECIEK_AIRBRIDGE,
        "leak_high_seconds": przeciek_wysoki if ma_przeciek else None,
        "leak_27_seconds": przeciek_bardzo_duzy if ma_przeciek else None,
        "session_count": len(gotowe_sesje),
        "sessions": [{"start_ms": None if s["start_s"] is None else int(s["start_s"] * 1000),
                      "end_ms": None if s["koniec_s"] is None else int(s["koniec_s"] * 1000),
                      "prefiks": s["prefiks"]} for s in gotowe_sesje],
        "channels": {},
        "settings": {"available": dostepne, "fields": pola},
        "adnotacje_inne": dict(inne),
        "problemy": problemy + uwagi,
    }
    for id_k in ID_KANALOW:
        v = sorted(wartosci[id_k].items())
        if v:
            rek["channels"][id_k] = {"median": edf.percentyl_z_licznikow(v, 50),
                                     "p95": edf.percentyl_z_licznikow(v, 95),
                                     "min": v[0][0], "max": v[-1][0], "source": "edf"}
        else:
            med = str_.statystyka(dzien, id_k, "50")
            p95 = str_.statystyka(dzien, id_k, "95")
            mx = str_.statystyka(dzien, id_k, "Max")
            zr = "str" if any(x is not None for x in (med, p95, mx)) else ""
            rek["channels"][id_k] = {"median": med, "p95": p95, "min": None, "max": mx, "source": zr}
    # PS tylko z plików PLD; bez nich (źródło STR) brak wartości, bez zapasu ze STR.edf
    v = sorted(wsparcie.items())
    rek["wsparcie"] = ({"median": edf.percentyl_z_licznikow(v, 50), "p95": edf.percentyl_z_licznikow(v, 95),
                        "source": "edf"} if v else {"median": None, "p95": None, "source": ""})
    return rek


# ---------------------------------------------------------------------------
# Metryki M-01…M-21 (4.3, 4.11)
# ---------------------------------------------------------------------------
def ustawienia_terapii(pola):
    """Tryb, tekst M-21, maksimum i minimum ciśnienia według 4.11."""
    tagi = {p["tag"]: p for p in pola}
    tryb = tagi["MOP"]["value"] if "MOP" in tagi else None

    def l(tag):
        p = tagi.get(tag)
        return None if p is None else p["liczba"]

    mx = mn = None
    if tryb == "ASVAuto":
        tekst = "ASVAuto · EPAP %s–%s · PS %s–%s" % (
            _liczba_pl(l("EAI")), _liczba_pl(l("EAX")), _liczba_pl(l("ANS")), _liczba_pl(l("AXS")))
        if l("EAX") is not None and l("AXS") is not None:
            mx = l("EAX") + l("AXS")
        mn = l("EAI")
    elif tryb == "ASV":
        tekst = "ASV · EPAP %s · PS %s–%s" % (_liczba_pl(l("EEP")), _liczba_pl(l("MNS")), _liczba_pl(l("MXS")))
        if l("EEP") is not None and l("MXS") is not None:
            mx = l("EEP") + l("MXS")
    elif tryb in ("AutoSet", "APAP", "AutoSet Her"):
        tekst = "%s–%s · EPR %s" % (_liczba_pl(l("MPI")), _liczba_pl(l("MPA")), _liczba_pl(l("EPR"), 0))
        mx, mn = l("MPA"), l("MPI")
    elif tryb == "CPAP":
        tekst = "CPAP %s · EPR %s" % (_liczba_pl(l("IPC")), _liczba_pl(l("EPR"), 0))
    elif tryb is None:
        tekst = None
    else:
        tekst = tryb
    return {"tryb": tryb, "m21": tekst, "cisnienie_max": mx, "cisnienie_min": mn,
            "maska": tagi["MSK"]["value"] if "MSK" in tagi else None}


def _sesje_nocne(rek, par):
    """Sesje liczone do M-17 i M-18 według R-DANE-27: lista (start_min, długość_s)."""
    poludnie = rek["day"] * 86400 + 12 * 3600
    sesje = []
    for s in rek["sessions"]:
        if s["start_ms"] is None:
            sesje.append((None, None))
            continue
        start_min = (s["start_ms"] / 1000.0 - poludnie) / 60.0
        dl = None if s["end_ms"] is None else (s["end_ms"] - s["start_ms"]) / 1000.0
        sesje.append((start_min, dl))
    po_18 = [x for x in sesje if x[0] is None or x[0] >= par.start_nocy_min]
    ma_dluga = any(d is not None and d >= par.wczesny_sen_s for st, d in po_18 if st is not None)
    if not ma_dluga:
        przed = [x for x in sesje if x[0] is not None and x[0] < par.start_nocy_min and x[1] is not None]
        if przed:
            najdl = max(przed, key=lambda x: x[1])
            if najdl[1] >= par.wczesny_sen_s:
                po_18 = [x for x in sesje if x[0] is None or x[0] >= najdl[0]]
    return po_18


def m17_m18(rek, par):
    """M-17 (przerwy) i M-18 (pora startu, minuty od 12:00 dnia `day`) według R-DANE-27."""
    if not rek["sessions"]:
        return max(0, rek["session_count"] - 1), None
    nocne = _sesje_nocne(rek, par)
    m17 = max(0, len(nocne) - 1)
    ze_startem = [x for x in nocne if x[0] is not None]
    if not ze_startem:
        return m17, None
    if all(x[1] is not None for x in ze_startem):
        wybrana = max(ze_startem, key=lambda x: (x[1], -x[0]))     # najdłuższa sesja
    else:
        wybrana = min(ze_startem, key=lambda x: x[0])              # bez długości: najwcześniejsza
    return m17, wybrana[0]


def metryki(rek, par):
    """Metryki jednej nocy (bez M-19 i M-20, które liczy `noce`)."""
    m = {}
    used = rek["used_seconds"]
    m["m01_h"] = None if used is None else used / 3600.0
    m["m02_ahi"] = rek["ahi"]
    c = rek["counts"]
    for k in ("oa", "ca", "ua", "h"):
        m["m03_%s_h" % k] = None if c[k] is None else c[k] * 3600.0 / used
    m["m03_obturacyjne_h"] = (None if c["oa"] is None else
                              (c["oa"] + c["ua"] + c["h"]) * 3600.0 / used)
    m["m04_rera_h"] = None if c["rera"] is None else c["rera"] * 3600.0 / used
    ch = rek["channels"]
    m["zrodlo_cisnienia"] = ch["Press"]["source"] or None
    m["m05_cisnienie_med"] = ch["Press"]["median"]
    m["m05p_cisnienie_p95"] = ch["Press"]["p95"]
    m["m06_wydechowe_med"] = ch["EprPress"]["median"]
    m["m06p_wydechowe_p95"] = ch["EprPress"]["p95"]
    ps = rek["wsparcie"] if ch["Press"]["source"] == "edf" else {"median": None, "p95": None}
    m["m28_wsparcie_med"] = ps["median"]
    m["m28p_wsparcie_p95"] = ps["p95"]
    m["m07_przeciek_med"] = ch["Leak"]["median"]
    m["m07_przeciek_p95"] = ch["Leak"]["p95"]
    lh = rek["leak_high_seconds"]
    m["m08_przeciek_wysoki_min"] = None if lh is None else lh / 60.0
    m["m08_przeciek_wysoki_proc"] = None if (lh is None or not used) else 100.0 * lh / used
    lt = rek["leak_27_seconds"]
    m["m08_27_min"] = None if lt is None else lt / 60.0
    csr = rek["csr_seconds"]
    m["m09_csr_min"] = None if csr is None else csr / 60.0
    m["m09_csr_proc"] = None if (csr is None or not used) else 100.0 * csr / used
    m["m10_flowlim_med"] = ch["FlowLim"]["median"]
    m["m10_flowlim_p95"] = ch["FlowLim"]["p95"]
    m["m13_oddechy_med"] = ch["RespRate"]["median"]
    m["m14_objetosc_med"] = ch["TidVol"]["median"]
    m["m15_wentylacja_med"] = ch["MinVent"]["median"]
    m["m16_chrapanie_p95"] = ch["Snore"]["p95"]
    m["m17_przerwy"], m["m18_start_min"] = m17_m18(rek, par)
    m["m18_start"] = _godzina(m["m18_start_min"])
    ust = ustawienia_terapii(rek["settings"]["fields"])
    m["tryb"] = ust["tryb"]
    m["tryb_obslugiwany"] = ust["tryb"] == par.tryb_obslugiwany
    m["m21_ustawienia"] = ust["m21"]
    m["cisnienie_max"] = ust["cisnienie_max"]
    m["cisnienie_min"] = ust["cisnienie_min"]
    m["maska"] = ust["maska"]
    m["m24_spo2_med"] = ch["SpO2"]["median"]
    m["m24_spo2_min"] = ch["SpO2"]["min"]
    m["m24_puls_med"] = ch["Pulse"]["median"]
    m["wazna"] = bool(rek["present"] and not rek["partial"] and rek["events_known"]
                      and used is not None and used >= par.noc_wazna_s)
    return m


# ---------------------------------------------------------------------------
# API
# ---------------------------------------------------------------------------
def noce(karta_dir, par=None):
    """Lista nocy z karty (jedna na katalog DATALOG z sesją), posortowana po `day`.

    Każdy rekord: pola raportu nocy AirBridge (Załącznik A) i klucz `metryki`
    z M-01…M-21 (bez M-11 i M-12: GI zablokowany).
    """
    par = par or Parametry()
    str_ = Str(os.path.join(karta_dir, "STR.edf"))
    datalog = os.path.join(karta_dir, "DATALOG")
    wynik = []
    if os.path.isdir(datalog):
        for nazwa in sorted(os.listdir(datalog)):
            dzien = _dzien_katalogu(nazwa)
            sciezka = os.path.join(datalog, nazwa)
            if dzien is None or not os.path.isdir(sciezka):
                continue
            if not any(PLIK_RE.match(n) and n.upper().split("_")[2][:3] in ("BRP", "PLD")
                       for n in os.listdir(sciezka)):
                continue           # dzień bez sesji nie jest na liście nocy
            rek = noc_z_katalogu(sciezka, dzien, str_, par)
            if str_.blad:
                rek["problemy"].append(str_.blad)
            rek["metryki"] = metryki(rek, par)
            wynik.append(rek)
    wynik.sort(key=lambda r: r["day"])
    _metryki_wielonocne(wynik, par)
    return wynik


def _metryki_wielonocne(lista, par):
    """M-19 (regularność) i M-20 (seria) z rekordów nocy (dni bez rekordu = brak nocy ≥ 4 h)."""
    if not lista:
        return
    dlugie = {r["day"] for r in lista
              if r["used_seconds"] is not None and r["used_seconds"] >= par.noc_wazna_s}
    pierwszy = lista[0]["day"]
    for r in lista:
        d = min(30, r["day"] - pierwszy + 1)
        n = sum(1 for x in range(r["day"] - d + 1, r["day"] + 1) if x in dlugie)
        r["metryki"]["m19_noce"] = n
        r["metryki"]["m19_dni"] = d
        r["metryki"]["m19_proc"] = 100.0 * n / d
        seria, x = 0, r["day"]
        while x in dlugie:
            seria += 1
            x -= 1
        r["metryki"]["m20_seria"] = seria


def sprawdz_str(karta_dir, lista, od=None, do=None, par=None):
    """Sprawdzian loadera na prawdziwej karcie: czas terapii z plików DATALOG wobec
    podsumowania aparatu w STR.edf (`Duration`, minuty).

    (a) liczba nocy z used_seconds ≥ 4 h wobec liczby dni STR z Duration ≥ 240 min: zgodne przy
        różnicy najwyżej 2; (b) used_seconds wobec Duration × 60 z dokładnością 5 min w co najmniej
        95% dni STR z Duration > 0 (noc bez rekordu albo bez used_seconds = niezgodna).
    Zakres: daty poranka [od, do]; bez nich wszystkie dni STR i nocy z karty.
    """
    par = par or Parametry()
    str_ = Str(os.path.join(karta_dir, "STR.edf"))
    d0 = dzien_z_daty(od) - 1 if od else None
    d1 = dzien_z_daty(do) - 1 if do else None

    def w_zakresie(d):
        return (d0 is None or d >= d0) and (d1 is None or d <= d1)
    po_dniu = {r["day"]: r for r in lista if w_zakresie(r["day"])}
    dni_str = {d: rek.get("Duration") for d, rek in str_.dni.items() if w_zakresie(d)}
    str_4h = sum(1 for v in dni_str.values() if v is not None and v >= 240)
    noce_4h = sum(1 for r in po_dniu.values()
                  if r["used_seconds"] is not None and r["used_seconds"] >= par.noc_wazna_s)
    z_terapia = [d for d, v in dni_str.items() if v is not None and v > 0]
    zgodne = sum(1 for d in z_terapia if d in po_dniu and po_dniu[d]["used_seconds"] is not None
                 and abs(po_dniu[d]["used_seconds"] - dni_str[d] * 60) <= 300)
    proc = 100.0 * zgodne / len(z_terapia) if z_terapia else None
    return {
        "str_blad": str_.blad,
        "dni_str_4h": str_4h, "noce_4h": noce_4h,
        "a_zgodne": str_.blad is None and abs(str_4h - noce_4h) <= 2,
        "dni_str_z_terapia": len(z_terapia), "zgodne_5min": zgodne, "zgodne_proc": proc,
        "b_zgodne": proc is not None and proc >= 95.0,
    }


def kalendarz(lista, od=None, do=None):
    """Noce w zakresie dat poranka [od, do], z pustymi wierszami dla dni bez nocy na liście."""
    if not lista and (od is None or do is None):
        return []
    po_dniu = {r["day"]: r for r in lista}
    d0 = dzien_z_daty(od) - 1 if od else lista[0]["day"]
    d1 = dzien_z_daty(do) - 1 if do else lista[-1]["day"]
    out = []
    for dzien in range(d0, d1 + 1):
        out.append(po_dniu.get(dzien) or {"day": dzien, "data_poranka": data_z_dnia(dzien + 1).isoformat(),
                                          "na_liscie": False})
    return out


KOLUMNY = [
    "data_poranka", "day", "na_liscie", "present", "partial", "events_known", "wazna",
    "tryb", "tryb_obslugiwany", "used_seconds", "m01_h", "m02_ahi",
    "oa", "ca", "ua", "h", "rera", "klasyfikacja_bezdechow", "csr_dostepne",
    "m03_oa_h", "m03_ca_h", "m03_ua_h", "m03_h_h", "m03_obturacyjne_h", "m04_rera_h",
    "zrodlo_cisnienia", "m05_cisnienie_med", "m05p_cisnienie_p95", "m06_wydechowe_med",
    "m06p_wydechowe_p95", "m28_wsparcie_med", "m28p_wsparcie_p95",
    "m07_przeciek_med", "m07_przeciek_p95", "leak_high_seconds", "m08_przeciek_wysoki_min",
    "m08_przeciek_wysoki_proc", "m08_27_min", "csr_seconds", "csr_zrodlo", "m09_csr_min",
    "m09_csr_proc", "m10_flowlim_med", "m10_flowlim_p95", "m13_oddechy_med", "m14_objetosc_med",
    "m15_wentylacja_med", "m16_chrapanie_p95", "session_count", "sesje", "m17_przerwy",
    "m18_start_min", "m18_start", "m19_noce", "m19_dni", "m19_proc", "m20_seria",
    "m21_ustawienia", "cisnienie_max", "cisnienie_min", "maska",
    "m24_spo2_med", "m24_spo2_min", "m24_puls_med", "tgtvent_med", "str_duration_min",
    "ustawienia_tagi", "adnotacje_inne", "problemy",
]


def _hhmm_ms(ms):
    if ms is None:
        return "?"
    t = edf.EPOKA + _dt.timedelta(milliseconds=ms)
    return t.strftime("%H:%M")


def wiersz(rek):
    """Płaski wiersz CSV z rekordu nocy."""
    w = {"data_poranka": rek["data_poranka"], "day": rek["day"], "na_liscie": rek["na_liscie"]}
    if not rek["na_liscie"]:
        return w
    m = rek["metryki"]
    w.update({k: rek[k] for k in ("present", "partial", "events_known", "used_seconds", "str_duration_min",
                                  "klasyfikacja_bezdechow", "csr_dostepne",
                                  "leak_high_seconds", "csr_seconds", "csr_zrodlo", "session_count")})
    w.update({k: rek["counts"][k] for k in ("oa", "ca", "ua", "h", "rera")})
    w.update(m)
    w["sesje"] = ";".join("%s-%s" % (_hhmm_ms(s["start_ms"]), _hhmm_ms(s["end_ms"])) for s in rek["sessions"])
    w["tgtvent_med"] = rek["channels"]["TgtVent"]["median"]
    w["ustawienia_tagi"] = ";".join("%s=%s" % (p["tag"], p["value"]) for p in rek["settings"]["fields"])
    w["adnotacje_inne"] = ";".join("%s:%d" % kv for kv in sorted(rek["adnotacje_inne"].items()))
    w["problemy"] = ";".join(rek["problemy"])
    return w


def _formatuj(v):
    if v is None:
        return ""
    if isinstance(v, bool):
        return "tak" if v else "nie"
    if isinstance(v, float):
        return ("%.4f" % v).rstrip("0").rstrip(".")
    return str(v)


def zapisz_csv(wiersze, sciezka):
    with open(sciezka, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=KOLUMNY, extrasaction="ignore", lineterminator="\n")
        w.writeheader()
        for r in wiersze:
            w.writerow({k: _formatuj(r.get(k)) for k in KOLUMNY})


def _wewnatrz(sciezka, katalog):
    """Czy `sciezka` (plik albo katalog, może jeszcze nie istnieć) leży w `katalog`.
    Porównanie przez samefile na przodkach, więc działa też przy innej wielkości liter
    na macOS i przy dowiązaniach."""
    if not os.path.exists(katalog):
        return False
    p = os.path.abspath(sciezka)
    while True:
        if os.path.exists(p) and os.path.samefile(p, katalog):
            return True
        rodzic = os.path.dirname(p)
        if rodzic == p:
            return False
        p = rodzic


def main(argv=None):
    ap = argparse.ArgumentParser(description="Tabela nocy z kopii karty SD aparatu ResMed (bez oceny reguł).")
    ap.add_argument("karta", help="katalog kopii karty (z STR.edf i DATALOG)")
    ap.add_argument("--od", type=_dt.date.fromisoformat, help="pierwsza data poranka RRRR-MM-DD")
    ap.add_argument("--do", type=_dt.date.fromisoformat, help="ostatnia data poranka RRRR-MM-DD")
    ap.add_argument("--wyjscie", required=True, help="plik CSV poza repozytorium i poza katalogiem karty")
    ap.add_argument("--sprawdz-str", action="store_true",
                    help="porównaj czas terapii z DATALOG z podsumowaniem STR.edf (tylko liczby zgodności)")
    a = ap.parse_args(argv)
    a.karta = os.path.expanduser(a.karta)
    a.wyjscie = os.path.expanduser(a.wyjscie)

    if not os.path.isdir(a.karta):
        print("Nie ma katalogu karty: %s" % a.karta, file=sys.stderr)
        return 2
    if not (os.path.isfile(os.path.join(a.karta, "STR.edf")) or os.path.isdir(os.path.join(a.karta, "DATALOG"))):
        print("W katalogu nie ma STR.edf ani DATALOG: to nie wygląda na kartę ResMed.", file=sys.stderr)
        return 2
    if _wewnatrz(a.wyjscie, a.karta):
        print("Odmowa: plik wyjściowy w katalogu karty. Skrypt nie pisze do karty.", file=sys.stderr)
        return 2
    if _wewnatrz(a.karta, REPO):
        print("Odmowa: katalog karty w repozytorium. Kopia karty nie może leżeć w repo (WY12).",
              file=sys.stderr)
        return 2
    if _wewnatrz(a.wyjscie, REPO):
        print("Odmowa: plik wyjściowy w repozytorium. Prawdziwe noce nie mogą trafić do repo (WY12).",
              file=sys.stderr)
        return 2
    if a.od and a.do and a.od > a.do:
        print("--od jest późniejsze niż --do.", file=sys.stderr)
        return 2
    katalog_wyj = os.path.dirname(os.path.abspath(a.wyjscie))
    if not os.path.isdir(katalog_wyj):
        print("Nie ma katalogu na plik wyjściowy: %s" % katalog_wyj, file=sys.stderr)
        return 2

    lista = noce(a.karta)
    wiersze = [wiersz(r) for r in kalendarz(lista, a.od, a.do)]
    zapisz_csv(wiersze, a.wyjscie)
    na_liscie = [w for w in wiersze if w["na_liscie"]]
    print("Zapisano %d wierszy (%d nocy z karty) do %s" % (len(wiersze), len(na_liscie), a.wyjscie))
    print("Noce ważne: %d; w trybie %s: %d; z problemami plików: %d" % (
        sum(1 for w in na_liscie if w.get("wazna")), Parametry().tryb_obslugiwany,
        sum(1 for w in na_liscie if w.get("tryb_obslugiwany")),
        sum(1 for w in na_liscie if w.get("problemy"))))
    inne = Counter()
    for r in lista:
        inne.update(r["adnotacje_inne"])
    if inne:
        print("Adnotacje bez znaczenia dla raportu (nie liczone): " +
              ", ".join("%s × %d" % kv for kv in inne.most_common()))
    if a.sprawdz_str:
        w = sprawdz_str(a.karta, lista, a.od, a.do)
        if w["str_blad"]:
            print("Sprawdzian STR: %s" % w["str_blad"])
            return 1
        print("Sprawdzian STR (a): noce ≥ 4 h z DATALOG %d, dni STR z Duration ≥ 240 min %d: %s" % (
            w["noce_4h"], w["dni_str_4h"], "ZGODNE" if w["a_zgodne"] else "NIEZGODNE (różnica > 2)"))
        print("Sprawdzian STR (b): czas terapii zgodny z Duration (±5 min) w %d z %d dni (%s%%): %s" % (
            w["zgodne_5min"], w["dni_str_z_terapia"],
            "–" if w["zgodne_proc"] is None else ("%.1f" % w["zgodne_proc"]),
            "ZGODNE" if w["b_zgodne"] else "NIEZGODNE (< 95%)"))
        return 0 if (w["a_zgodne"] and w["b_zgodne"]) else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
