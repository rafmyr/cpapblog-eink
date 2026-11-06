#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Scenariusze symulatora doby: wymyślone noce dopisane po zrzucie, żeby zobaczyć ekrany,
których zdrowe noce nie wywołują (zdarzenia E, wzorce L, „pokaż lekarzowi”, linie uwagi, „Otwarte sprawy”, pytanie
o senność, CO2 wieczorem, tryb AutoSet).

Każdy scenariusz:
  - zaczyna od rozbiegu: ROZBIEG zdrowych nocy z pełnymi metrykami od dnia po ostatniej nocy zrzutu (norma osobista
    potrzebuje co najmniej 14 nocy z metryką, R-NOR-10; noce z importu karty nie mają M-03, M-08, M-13, M-17);
  - potem dopisuje noce, które wyzwalają jego zdarzenie albo wzorzec (pełne metryki: OA, CA, H, M-08, M-13, M-17, tryb);
  - ustawia okres symulacji: od pierwszego poranka, w którym ekran powinien się pojawić, do dnia, w którym widać ciąg
    dalszy;
  - dokłada naciśnięcia przycisków (K1, K2, „Otwarte sprawy”) i ewentualnie atrapę CO2 albo milczący AirBridge.
Noce są syntetyczne i oznaczone tak w log.txt (`[doszła noc syntetyczna …]` i wiersz w nagłówku). Ustawienia aparatu
(tryb, EPAP, PS) każda noc bierze z ostatniej nocy zrzutu, żeby nie udawać zmiany ustawień (R-NOR-07); wyjątkiem jest
scenariusz `zmiana-trybu`.

Progi, na które scenariusze celują (docs/spec/progi.csv): E-23 wyciek P95 ≥ 27 L/min, E-30 mediana AHI z 7 nocy ≥ 5/h,
L-03 mediana z 14 nocy ≥ 10/h, E-33 odchylenie CA i mediana CA z 5 nocy ≥ 3/h, L-01 średnia CA z 7 nocy ≥ 5/h, E-10
2 noce bez terapii (< 15 min), E-11 terapia 15 min – 4 h, L-11 senność „tak” 2 kolejne tygodnie, S-K-01 CO2 ≥ 1000 ppm.
"""
import datetime as dt
import glob
import os
import random

ROZBIEG = 21  # zdrowych nocy przed scenariuszem [stała symulatora]

# kolejność pól `noce` wersji 2 (src/noc.h wierszNocyV2(), src/magazyn.cpp rejestrFormatow())
POLA = ("data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;epap_str;oa;ca;h;"
        "csr;import;m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb").split(";")
USTAWIENIA_ASV = ("eai", "eax", "ans", "axs")
USTAWIENIA_AUTOSET = ("mpa", "mpi", "epr")
MODEL = "AirCurve_10_CS_PaceWave"  # jak STATUS_SPOCZYNEK w firmware/prototyp/test/symulator.cpp


class BladScenariusza(Exception):
    pass


# ---------- ostatnia noc zrzutu (wzór ustawień) ----------
def _wiersze_nocy(dane):
    """(data, pola) wszystkich nocy zrzutu: noce.csv (wersja 0) i dane/noce/RRRR.csv (wersja 1 i 2)."""
    pliki = [os.path.join(dane, "noce.csv")] + sorted(glob.glob(os.path.join(dane, "dane", "noce", "*.csv")))
    w = []
    for p in pliki:
        if not os.path.isfile(p):
            continue
        with open(p, encoding="utf-8-sig", errors="replace") as f:
            for l in f:
                l = l.rstrip("\r\n")
                if not l or l.startswith("#") or l.startswith("data;"):
                    continue
                pola = l.split(";")
                try:
                    dt.date.fromisoformat(pola[0])
                except ValueError:
                    continue
                w.append((pola[0], pola))
    return sorted(w)


def wzor(dane):
    """Ostatnia noc zrzutu i ustawienia do nocy scenariusza: {'data', 'tryb', 'ustawienia', 'pna', 'srn', eai…}."""
    w = _wiersze_nocy(dane)
    if not w:
        raise BladScenariusza("w zrzucie nie ma nocy, scenariusz nie ma od czego zacząć")
    data, pola = w[-1]
    z = {"data": dt.date.fromisoformat(data), "ustawienia": "", "tryb": "", "pna": MODEL, "srn": ""}
    # ustawienia z najnowszej nocy, która je ma (noce z karty bywają bez nich)
    for _, p in reversed(w):
        if len(p) > 12 and p[12]:
            z["ustawienia"] = p[12]
            break
    for _, p in reversed(w):
        if len(p) >= 41 and p[40]:
            z["tryb"] = p[40]
            for k in USTAWIENIA_ASV + USTAWIENIA_AUTOSET + ("pna", "srn"):
                z[k] = p[POLA.index(k)]
            z["pna"] = z["pna"] or MODEL
            break
    if not z["tryb"]:
        z["tryb"] = z["ustawienia"].split("/")[0] if z["ustawienia"] else "ASVAuto"
    if not z["ustawienia"]:
        z["ustawienia"] = "ASVAuto/6/10/3/15" if z["tryb"] == "ASVAuto" else z["tryb"]
    z["typowe"] = _typowe(w)
    z["ahi_wazne"] = _ahi_wazne(w)
    z["nocy_zrzutu"] = len({d for d, _ in w})
    if z["tryb"] in ("ASVAuto", "ASV") and not any(z.get(k) for k in USTAWIENIA_ASV):
        cz = z["ustawienia"].split("/")
        if len(cz) == 5:  # `ASVAuto/EPAP min/EPAP maks/PS min/PS maks` jak w zrzucie syntetycznym
            z.update(dict(zip(USTAWIENIA_ASV, cz[1:])))
    return z


# ---------- noce ----------
# Metryki rozbiegu brane z typowych wartości zrzutu (mediana z ostatnich TYPOWE_NOCY nocy ważnych). Poprawka po
# integracji rozbieg ze stałymi wartościami (np. czas z maską 6 h 40–7 h 40, EPAP P95 8–8,5) odbiegał od normy
# osobistej z prawdziwej historii, więc po 21 nocach rozbiegu panel słusznie widział stan odchylenia (4.6)
# pełny ekran S3 (E-13, E-36, E-14…, tabela 7.2 R5b–R5d) zasłaniał ekrany, które scenariusz zapowiada, a prośba S3
# (R-PIE-10) blokowała S-K-01. Rozbieg ma być „zdrowy dla tego pacjenta”: metryki bez progu zdrowotnego jak w jego
# normie; bezdechy i wyciek najwyżej jak w normie i nie wyżej niż zdrowy zakres stały (tu odchylenie w górę to sygnał).
TYPOWE_NOCY = 30
TYPOWE = ("m01_min", "cmed", "cp95", "emed", "ep95", "psmed", "psp95", "m13", "m17")
TYPOWE_ZDROWE = ("oa", "ca", "h", "wmed", "wp95", "m08")  # najwyżej mediana zrzutu


def _ahi_wazne(w_nocy):
    """[(data, AHI)] nocy ważnych zrzutu z AHI (rekord tygodnia O-03 liczony względem historii zrzutu)."""
    v = []
    for d, p in w_nocy:
        try:
            if len(p) > 3 and p[1] == "1":
                v.append((d, float(p[3])))
        except ValueError:
            pass
    return v


def min_mediana7(ahi):
    """Najniższa krocząca mediana 7 kolejnych wartości (jak O-03, src/odkrycia.cpp rRekordTygodnia()); None przy < 7."""
    m = None
    for i in range(len(ahi) - 6):
        o = sorted(ahi[i:i + 7])[3]
        m = o if m is None else min(m, o)
    return m


def _typowe(w_nocy):
    """{metryka: mediana} z ostatnich TYPOWE_NOCY nocy ważnych zrzutu (pola puste i nieliczbowe pominięte)."""
    wazne = [p for _, p in w_nocy if len(p) > 2 and p[1] == "1"][-TYPOWE_NOCY:]
    t = {}
    for k in TYPOWE + TYPOWE_ZDROWE:
        i = POLA.index(k)
        v = []
        for p in wazne:
            try:
                v.append(float(p[i]))
            except (IndexError, ValueError):
                pass
        if v:
            v.sort()
            t[k] = v[len(v) // 2] if len(v) % 2 else (v[len(v) // 2 - 1] + v[len(v) // 2]) / 2
    return t


def _import_ep(data):
    """Chwila importu nocy = chwila, w której symulator dokłada ją do pamięci panelu: 04:30 czasu polskiego w dniu
    poranka."""
    try:
        from zoneinfo import ZoneInfo
        strefa = ZoneInfo("Europe/Warsaw")
    except Exception:  # bez bazy stref: CET
        strefa = dt.timezone(dt.timedelta(hours=1))
    return int(dt.datetime(data.year, data.month, data.day, 4, 30, tzinfo=strefa).timestamp())


def _l(v):
    return "" if v is None else ("%.2f" % v if isinstance(v, float) else str(v))


def noc(data, w, **zm):
    """Wiersz nocy wersji 2 (41 pól): zdrowa noc z AirBridge z ustawieniami wzoru `w`, nadpisana polami `zm`
    (np. wp95=38.0, ca=7.5, m01_min=150, tryb='AutoSet'). AHI = OA + CA + H, gdy nie podano wprost."""
    los = random.Random(data.toordinal())
    # wyciek P95 zdrowej nocy 6–9,5 L/min (pomijalny): od 06.11.2026 (model pasm wycieku) P95 od 10 L/min przy
    # domyślnym „Uwaga o masce od” 10 daje rano `Wyciek widoczny` (R-KONF-42), a od 15 S-K-10 w tonie „problem”
    asv = w["tryb"] in ("ASVAuto", "ASV")
    r = {"data": data.isoformat(), "m01_min": los.randint(400, 460), "cmed": los.uniform(9.5, 10.5),
         "cp95": los.uniform(12.5, 13.5), "wmed": los.uniform(2.0, 4.0), "wp95": los.uniform(6.0, 9.5),
         "emed": los.uniform(6.5, 7.0) if asv else None, "ep95": los.uniform(8.0, 8.5) if asv else None,
         "psmed": los.uniform(3.5, 4.0) if asv else None, "psp95": los.uniform(6.5, 8.0) if asv else None,
         "ustawienia": w["ustawienia"], "zrodlo": "A", "czesciowa": 0, "epap_str": 0,
         "oa": los.uniform(0.3, 0.6), "ca": los.uniform(0.3, 0.8), "h": los.uniform(0.2, 0.6),
         "csr": los.uniform(0.0, 2.0), "import": _import_ep(data), "m13": los.uniform(14.0, 16.0),
         "m17": los.randint(0, 1), "pna": w["pna"], "srn": w["srn"], "m08": los.uniform(0.0, 2.0),
         "ua": los.uniform(0.1, 0.4), "m10": los.uniform(0.0, 0.1), "m10p": los.uniform(0.1, 0.3), "tryb": w["tryb"]}
    for k, med in w.get("typowe", {}).items():  # rozbieg w normie osobistej zrzutu (opis przy TYPOWE)
        if k not in r or (r[k] is None and k not in ("m13", "m17")):
            continue
        v = med * los.uniform(0.97, 1.03)
        if k in TYPOWE_ZDROWE:
            v = min(r[k], v)
        r[k] = int(round(v)) if k in ("m01_min", "m17") else v
    for k in USTAWIENIA_ASV + USTAWIENIA_AUTOSET:
        v = w.get(k, "")
        r[k] = float(v) if v not in ("", None) else None
    r.update(zm)
    if "ahi" not in zm:
        r["ahi"] = None if r["oa"] is None else r["oa"] + r["ca"] + r["h"]
    r["wazna"] = 1 if (r["m01_min"] or 0) >= 240 else 0
    return ";".join(_l(r.get(k)) for k in POLA)


def _bez_terapii(data, w):
    """Noc kompletna z kilkoma minutami maski (np. zdjęta zaraz po założeniu): bez metryk oddechowych."""
    return noc(data, w, m01_min=5, ahi=None, oa=None, ca=None, h=None, csr=None, m13=None, m10=None, m10p=None,
               ua=None, m08=0.0, m17=0, cmed=None, cp95=None, wmed=None, wp95=None, emed=None, ep95=None, psmed=None,
               psp95=None)


def _autoset(w):
    z = dict(w)
    z.update({"tryb": "AutoSet", "ustawienia": "AutoSet/7/15/2", "mpi": "7", "mpa": "15", "epr": "2",
              "eai": "", "eax": "", "ans": "", "axs": ""})
    return z


# ---------- przyciski ----------
def przejscie_ekranow(chwila):
    """Naciśnięcia, które pokazują K1, K2 i do pięciu kart „Otwarte sprawy”, potem powrót do ekranu dnia (mapa
    5.6.2): ▶ z ekranu dnia → K1 (także z ekranu L; raport dla lekarza z L to ▶ 2 s), ▶ → K2, ▶ → Otwarte sprawy,
    ▶ → następna karta (za ostatnią S8 i „Sprzęt”), ↻ → ekran dnia. Co 4 s, czyli w czuwaniu 20 s po poprzednim
    naciśnięciu."""
    k = [("prawy", 4 * i) for i in range(7)] + [("srodkowy", 28)]
    return [f"{chwila + dt.timedelta(seconds=s):%Y-%m-%dT%H:%M:%S} {p}" for p, s in k]


# ---------- scenariusze ----------
class Plan:
    """Wynik scenariusza: noce (wiersze wersji 2), okres, przyciski, atrapy i opis do log.txt i spis.txt."""

    def __init__(self):
        self.noce = []            # wiersze nocy wersji 2, rosnąco
        self.od = self.do = None  # datetime
        self.przyciski = []       # wiersze `RRRR-MM-DDTHH:MM:SS przycisk`
        self.otoczenie = []       # (HH:MM, ppm)
        self.otoczenie_od = self.otoczenie_do = None  # okres profilu CO2 (datetime); poza nim 650 ppm
        self.milczy_od = None     # datetime albo None
        self.konfiguracja = []    # wiersze `klucz=wartość` do /konfiguracja.txt (daty wymiany sprzętu)
        self.czynniki = []        # wiersze `czynniki` (odpowiedzi „Co było inaczej?”) nocy sprzed `od`
        self.znaleziska = []      # ekrany, których kod panelu nie pozwala wywołać (opis z przyczyną)
        self.aparat = None        # (pna, tryb) aparatu w atrapie AirBridge; None = jak w symulatorze
        self.przerwy = []         # (od, do) datetime: AirBridge milczy (wyjazd), noce dochodzą po powrocie
        self.opoznione = {}       # data poranka (RRRR-MM-DD) → HH:MM przyjścia nocy zamiast 04:30
        self.opis = []            # wiersze opisu
        # (rodzaj, wzorzec, co pokazuje, warunki): rodzaj `ekran` = przedrostek nazwy obrazka (`EKRAN …`
        # w log-panelu.txt), `linia` = tekst w linii uwagi klatki ekranu dnia, `regula` = reguła z poziomem w pamięci
        # panelu (wiersz `SYMULATOR reguły spełnione`, dopasowanie dokładne: `L-01` albo `L-01/wariant`). Warunki
        # muszą być spełnione dla tej samej klatki:
        # ("regula", id) w liście reguł spełnionych w chwili klatki, ("log", tekst) w wierszach tego wybudzenia przed
        # klatką, ("tryb", T) i ("m01_ponizej", min) dla ostatniej nocy w pamięci panelu, ("syntetyczna", True).
        self.oczekiwane = []


def _rano(d, h=5, m=0):
    return dt.datetime(d.year, d.month, d.day, h, m)


def _wyciek(p, s, w):
    p.noce.append(noc(s, w, wmed=14.0, wp95=38.0, m08=70.0))
    p.od, p.do = _rano(s), _rano(s, 23, 59)
    p.przyciski += przejscie_ekranow(_rano(s, 7, 30))
    p.oczekiwane += [("ekran", "dzien-zdarzenie-E-23", "pełny ekran „Wyciek od 27 l/min w nocy” (E-23, S3) od rana",
                      (("regula", "E-23"), ("log", "ekran dnia: zdarzenie E-23"))),
                     ("ekran", "sprawy", "Otwarte sprawy z kartą E-23", (("regula", "E-23"),))]


def _ahi_rosnie(p, s, w):
    for i in range(14):
        p.noce.append(noc(s + dt.timedelta(days=i), w, oa=8.5, ca=1.0, h=4.5))
    p.od, p.do = _rano(s + dt.timedelta(days=3)), _rano(s + dt.timedelta(days=13), 12)
    p.przyciski += przejscie_ekranow(_rano(s + dt.timedelta(days=13), 7, 30))
    p.oczekiwane += [("ekran", "dzien-zdarzenie-E-30", "pełny ekran „Noce z AHI powyżej 5” (E-30, S3) w dniu wejścia",
                      (("regula", "E-30"), ("log", "ekran dnia: zdarzenie E-30"))),
                     ("linia", "Wciąż: Noce z AHI powyżej 5.", "po dwóch pełnych ekranach w 7 dniach linia uwagi "
                      "`Wciąż: Noce z AHI powyżej 5.` (R-KON-14, R-EKR-40)", (("regula", "E-30"),)),
                     ("ekran", "dzien-wzorzec-L-03", "ekran „pokaż lekarzowi”: AHI z 14 nocy 10/h i więcej (L-03)",
                      (("regula", "L-03"), ("log", "ekran dnia: wzorzec L-03"))),
                     ("ekran", "sprawy", "Otwarte sprawy z L-03", (("regula", "L-03"),))]


def _centralne(p, s, w):
    for i in range(7):
        p.noce.append(noc(s + dt.timedelta(days=i), w, oa=0.4, ca=7.5, h=0.5))
    p.od, p.do = _rano(s + dt.timedelta(days=2)), _rano(s + dt.timedelta(days=6), 12)
    p.przyciski += przejscie_ekranow(_rano(s + dt.timedelta(days=6), 7, 30))
    p.oczekiwane += [("ekran", "dzien-zdarzenie-E-33", "pełny ekran „Więcej bezdechów centralnych” (E-33, S3)",
                      (("regula", "E-33"), ("log", "ekran dnia: zdarzenie E-33"))),
                     ("ekran", "dzien-zdarzenie-E-30", "pełny ekran „Noce z AHI powyżej 5” (E-30, S3)",
                      (("regula", "E-30"), ("log", "ekran dnia: zdarzenie E-30"))),
                     ("regula", "L-01", "L-01 spełnione w trybie „tylko raport” (bez ekranu, karta w sprawach)", ()),
                     ("ekran", "sprawy", "Otwarte sprawy z L-01", (("regula", "L-01"),))]


def _krotkie_noce(p, s, w):
    p.noce += [noc(s, w, m01_min=150), _bez_terapii(s + dt.timedelta(days=1), w),
               _bez_terapii(s + dt.timedelta(days=2), w)]
    p.od, p.do = _rano(s), _rano(s + dt.timedelta(days=2), 23, 59)
    p.przyciski += przejscie_ekranow(_rano(s + dt.timedelta(days=2), 7, 30))
    p.oczekiwane += [("linia", "Krótka noc: 2 h 30.", "linia uwagi „Krótka noc” po nocy 2,5 h (E-11, grupa „noc”)",
                      (("regula", "E-11"),)),
                     ("linia", "bez terapii.", "linia uwagi `Noc {data} bez terapii.` po pierwszej nocy bez terapii (E-10)",
                      (("regula", "E-10/linia"),)),
                     ("ekran", "dzien-zdarzenie-E-10_problem", "pełny ekran „Noce bez terapii: 2” po dwóch nocach (E-10, S3)",
                      (("regula", "E-10/problem"), ("log", "ekran dnia: zdarzenie E-10/problem"))),
                     ("ekran", "dzien-S2", "ekran dnia z krótką nocą (poniżej 4 h)", (("m01_ponizej", 240),)),
                     ("ekran", "dzien-S2", "ekran dnia z nocą bez terapii (poniżej 15 min)", (("m01_ponizej", 15),))]


def _brak_danych(p, s, w):
    p.milczy_od = _rano(s, 0)
    p.od, p.do = _rano(s), _rano(s + dt.timedelta(days=1), 23, 59)
    p.przyciski += przejscie_ekranow(_rano(s + dt.timedelta(days=1), 7, 30))
    p.oczekiwane += [("ekran", "dzien-S10", "rano „Synchronizuję się”: noc jeszcze w drodze (S10)",
                      (("log", "ekran dnia S10"),)),
                     ("ekran", "dzien-S5", "od 11:00 brak danych z aparatu (S5, E-01)", (("log", "S5: "),))]


def _sennosc(p, s, w):
    n1 = s + dt.timedelta(days=(6 - s.weekday()) % 7)  # pierwsza niedziela od początku scenariusza
    for i in range((n1 - s).days + 9):
        p.noce.append(noc(s + dt.timedelta(days=i), w))
    for n in (n1, n1 + dt.timedelta(days=7)):
        p.przyciski.append(f"{_rano(n, 17, 5):%Y-%m-%dT%H:%M:%S} prawy")  # ▶ tak (5.6.2)
    pon = n1 + dt.timedelta(days=8)
    p.od, p.do = _rano(n1), _rano(pon, 12)
    p.przyciski += przejscie_ekranow(_rano(pon, 7, 30))
    p.oczekiwane += [("ekran", "dzien-pytanie-sennosc", "pytanie o senność w niedzielę o 17:00",
                      (("log", "ekran dnia: pytanie sennosc"),)),
                     ("ekran", "zapisano", "potwierdzenie odpowiedzi ▶ tak z pliku przycisków", (("log", "sennosc;tak"),)),
                     ("linia", "Senność", "linia uwagi o senności po odpowiedzi „tak”", ()),
                     ("ekran", "dzien-wzorzec-L-11", "ekran „pokaż lekarzowi”: senność 2 tygodnie z rzędu (L-11)",
                      (("regula", "L-11"), ("log", "ekran dnia: wzorzec L-11")))]


def _co2(p, s, w):
    p.noce += [noc(s, w), noc(s + dt.timedelta(days=1), w)]
    p.otoczenie = [("00:00", 1350), ("06:30", 900), ("09:00", 650), ("17:00", 1080), ("21:00", 1250)]
    p.od, p.do = _rano(s), _rano(s + dt.timedelta(days=1), 23, 59)
    p.otoczenie_od, p.otoczenie_do = _rano(s, 0), _rano(s + dt.timedelta(days=1), 23, 59)
    p.oczekiwane.append(("ekran", "dzien-wieczor-SK01", "wieczorem „przewietrz sypialnię”: CO2 ponad 1000 ppm (S-K-01)",
                         (("log", "ekran wieczorny SK01"), ("log", "CO2 1"))))


def _zmiana_trybu(p, s, w):
    a = _autoset(w)
    for i in range(3):
        p.noce.append(noc(s + dt.timedelta(days=i), a))
    p.od, p.do = _rano(s), _rano(s + dt.timedelta(days=2), 23, 59)
    p.przyciski += przejscie_ekranow(_rano(s, 7, 30)) + przejscie_ekranow(_rano(s + dt.timedelta(days=2), 7, 30))
    p.oczekiwane += [("ekran", "K1", "K1 z wierszami trybu AutoSet", (("tryb", "AutoSet"),)),
                     ("ekran", "K2", "K2 z wierszami trybu AutoSet", (("tryb", "AutoSet"),))]


def _zwykly_dzien(p, s, w):
    """Bez nocy syntetycznych: ostatni poranek zrzutu z prognozą i obrazkami dnia."""
    d = w["data"]
    p.od, p.do = _rano(d), _rano(d, 23, 59)
    p.przyciski += przejscie_ekranow(_rano(d, 7, 30))
    p.oczekiwane += [("ekran", "dzien-SK3", "slot dzienny (S-K-33…S-K-38) z atrapą prognozy",
                      (("log", "slot dzienny: S-K-3"),)),
                     ("ekran", "K1", "kokpit K1 z Twoją ostatnią nocą", (("syntetyczna", False),)),
                     ("ekran", "sprawy", "Otwarte sprawy", ())]


# ---------- scenariusze ekranów ----------
def _dni(s, i):
    return s + dt.timedelta(days=i)


def _t(d, h, m, sek=0):
    return dt.datetime(d.year, d.month, d.day, h, m, sek)


def _typ(w, k, domyslna):
    """Mediana metryki z nocy zrzutu (rozbieg ją powtarza) albo wartość domyślna."""
    return w.get("typowe", {}).get(k, domyslna)


def _sekwencja(chwila, kroki, odstep=4):
    """Naciśnięcia co `odstep` s od `chwila` (w czuwaniu 20 s po poprzednim): kroki `lewy`, `srodkowy 2s` …"""
    return [f"{chwila + dt.timedelta(seconds=odstep * i):%Y-%m-%dT%H:%M:%S} {k}" for i, k in enumerate(kroki)]


def _rosnacy_wyciek(s, w, ile, od=24.5, do=26.6, m08=45.0):
    """`ile` nocy z wyciekiem P95 rosnącym liniowo od `od` do `do` L/min: ponad S-K-10 (24 L/min, warunek L-09/.sk10
    w 10 z 14 nocy), poniżej E-23 (27 L/min), o ponad 10 L/min ponad normę z rozbiegu (S-K-31 warunek 2), z dużym
    wyciekiem M-08 `m08` min (E-20)."""
    return [noc(_dni(s, i), w, wp95=od + (do - od) * i / max(1, ile - 1), wmed=4.0 + 3.0 * i / max(1, ile - 1), m08=m08)
            for i in range(ile)]


SPRZET_ROSNIE = 14  # nocy z rosnącym wyciekiem w scenariuszu `sprzet` [stała symulatora]


def _sprzet(p, s, w):
    n = SPRZET_ROSNIE
    # stary plik z dwiema datami: maska = późniejsza (poduszka, −240 dni), wiersz rury ignorowany bez błędu
    p.konfiguracja += [f"wymiana.poduszka={_dni(s, -240).isoformat()}", f"wymiana.maska={_dni(s, -250).isoformat()}",
                       f"wymiana.rura={_dni(s, -400).isoformat()}"]
    p.noce += _rosnacy_wyciek(s, w, n)
    # noce z wyciekiem niższym o ponad 8 L/min od mediany 14 poprzednich: pytanie `Wymieniona maska?` (R-MAS-12)
    # (po 5. takiej nocy L-09 już nie ma warunku: M-07 > 24 L/min w 9 z 14 nocy, więc ekran L nie wypiera pytania)
    p.noce += [noc(_dni(s, n + i), w, wp95=9.5, wmed=2.5, m08=0.0) for i in range(7)]
    d_sk31, d_pyt, d_akcja = _dni(s, n - 1), _dni(s, n + 4), _dni(s, n + 5)
    p.od, p.do = _rano(_dni(s, 3)), _rano(_dni(s, n + 6), 12)
    # dzień S-K-31: ▶ po stronach kokpitu do „Sprzęt” (`do:sprzet`, liczba kart spraw zależy od danych; maska po
    # terminie w negatywie) → ↻ ekran dnia
    p.przyciski += _sekwencja(_t(d_sk31, 7, 30), ["prawy do:sprzet", "srodkowy"])
    p.przyciski.append(f"{_t(d_pyt, 7, 30):%Y-%m-%dT%H:%M:%S} prawy")  # ▶ tak na `Wymieniona maska?`
    # dzień później: „Sprzęt” (kursor na masce po wejściu ▶, dwa elementy), ◀ 2 s wymienione dziś (ekran
    # `Wymiana zapisana.` z cofnięciem), ◀ 2 s cofnij wymianę, ↻ 2 s nowy model → R-MAS-14, ◀ 2 s cofnij, ↻ 2 s nowy
    # model jeszcze raz (zostaje), po 30 s panel sam wraca
    p.przyciski += _sekwencja(_t(d_akcja, 7, 30), ["prawy do:sprzet", "lewy 2s", "lewy 2s", "srodkowy 2s", "lewy 2s",
                                                   "srodkowy 2s"])
    p.oczekiwane += [
        ("ekran", "dzien-SK31", "„Maska się zużywa” (S-K-31) na ekranie dnia: maska po terminie, wyciek rośnie",
         (("log", "ekran S-K-31"),)),
        ("log", "L-09 wstrzymane: element po terminie", "L-09 ma warunek, ale czeka na S-K-31 (R-MAS-03: element po "
         "terminie, w log-panelu.txt `L-09 wstrzymane`)", ()),
        ("dni_przed", "dzien-SK31", "pełny ekran L-09 dopiero po dniach z S-K-31, gdy spadek wycieku kasuje warunek "
         "S-K-31", (("przed", "dzien-wzorzec-L-09"), ("min", 1))),
        ("ekran", "sprzet", "„Sprzęt” (strona 5 / 6, ▶ po stronach kokpitu): maska po terminie w negatywie, filtr bez "
         "daty, bez rury i komory", ()),
        ("ekran", "dzien-pytanie-poduszka", "pytanie „Wymieniona maska?” po spadku wycieku (R-MAS-12)",
         (("log", "ekran dnia: pytanie poduszka"),)),
        ("ekran", "zapisano", "▶ tak na „Wymieniona maska?”: „Zapisano”", (("log", "poduszka;tak"),)),
        ("log", "sprzęt: wymiana maska", "◀ 2 s w „Sprzęt”: maska „wymienione dziś” (R-MAS-01), ekran `Wymiana zapisana.`", ()),
        ("log", "sprzęt: wymiana maska cofnięta", "◀ 2 s cofnij na `Wymiana zapisana.`: data maski sprzed zapisu", ()),
        ("ekran", "sprzet-zapisano", "„Nowy model maski zapisany.” z ◀ 2 s cofnij przez 30 s (R-MAS-14)", ()),
        ("log", "sprzęt: nowy model maski cofnięty", "◀ 2 s cofnij na R-MAS-14 przywraca datę maski i normę", ()),
        ("linia", "Nowy model maski. Uczę się normy.", "następnego ranka linia `Nowy model maski. Uczę się normy.` "
         "(R-MAS-11)", ())]


ZUZYCIE_NOCE = 26  # nocy rosnącego wycieku w scenariuszu `sprzet-zuzycie` [stała symulatora]


def _sprzet_zuzycie(p, s, w):
    # R-MAS-10 w danych: maska po terminie, wyciek rośnie bez przerwy (24,5 → 26,9 L/min), S-K-31 raz na 7 dni.
    # Norma M-07 to 30 ostatnich nocy ważnych, więc po ok. 15 nocach wzrostu dogania wyciek: warunek (2) S-K-31 pada,
    # a zdejmuje blokadę L-09 po 3. pokazaniu, przed 4. (R-MAS-10 tymi danymi nie do pokazania)
    # stary plik z dwiema datami: maska = późniejsza (poduszka, −240 dni), wiersz rury ignorowany bez błędu
    p.konfiguracja += [f"wymiana.poduszka={_dni(s, -240).isoformat()}", f"wymiana.maska={_dni(s, -250).isoformat()}",
                       f"wymiana.rura={_dni(s, -400).isoformat()}"]
    p.noce += _rosnacy_wyciek(s, w, ZUZYCIE_NOCE, od=24.5, do=26.9)
    p.od, p.do = _rano(_dni(s, 3)), _rano(_dni(s, ZUZYCIE_NOCE - 1), 12)
    p.oczekiwane += [
        ("ekran", "dzien-SK31", "„Maska się zużywa” (S-K-31), raz na 7 dni", (("log", "ekran S-K-31"),)),
        ("dni_przed", "dzien-SK31", "L-09 „Wyciek nie ustępuje” wstrzymane przez 3 dni z S-K-31 (R-MAS-03), potem "
         "pokazane, gdy norma dogoni wyciek i S-K-31 traci warunek", (("przed", "dzien-wzorzec-L-09"), ("min", 3)))]
    p.znaleziska.append("R-MAS-10 (L-09 wraca po 4. pokazaniu S-K-31 bez zapisu wymiany): danymi nie do wywołania. S-K-31 "
                        "pokazuje się raz na 7 dni i potrzebuje wycieku z 7 nocy ≥ norma + 10 L/min; norma M-07 to 30 "
                        "ostatnich nocy ważnych (src/noc.cpp liczNorme), więc po ok. 15 nocach wzrostu ją dogania i "
                        "(src/sprzet.cpp blokadaL09) zdejmuje blokadę po 3. pokazaniu. Powyżej 27 L/min pełny "
                        "ekran E-23 wypiera S-K-31.")


INFEKCJA_NOCE = 20  # nocy z szybszym oddechem w scenariuszu `infekcja` [stała symulatora]
INFEKCJA_KARENCJA = 14  # dni karencji po okresie z limitem 14 nocy (progi.csv R-INF-04.karencja, src/infekcja.h)


def _infekcja(p, s, w):
    # M-13 o 4/min ponad normę z rozbiegu (S-K-30: ≥ norma + 2 w 2 nocach) i ciśnienie P95 wyżej o ponad
    # 2 cmH2O (sygnał towarzyszący: EPAP P95 w ASV, ciśnienie P95 w CPAP i AutoSet) przez 20 nocy: okres infekcji kończy
    # limit 14 nocy (R-INF-04), potem karencja 14 nocy bez S-K-30 i zwykła ścieżka S-K-17
    # 7. noc okresu z wyciekiem P95 25,5 L/min i dużym wyciekiem 45 min: poza okresem S-K-10 i linia E-20,
    # w okresie „nie oceniam” (R-INF-02), więc tego ranka ani S-K-10, ani linii o dużym wycieku
    d_wyc = _dni(s, 6)
    for i in range(INFEKCJA_NOCE):
        wyc = {"wp95": 25.5, "wmed": 8.0, "m08": 45.0} if _dni(s, i) == d_wyc else {}
        p.noce.append(noc(_dni(s, i), w, m13=_typ(w, "m13", 15.0) + 4.0, cp95=_typ(w, "cp95", 13.0) + 2.8,
                          ep95=_typ(w, "ep95", 8.25) + 2.6, **wyc))
    d_pyt = _dni(s, 1)
    p.od, p.do = _rano(d_pyt), _rano(_dni(s, INFEKCJA_NOCE - 1), 12)
    p.przyciski.append(f"{_t(d_pyt, 7, 30):%Y-%m-%dT%H:%M:%S} prawy")  # ▶ tak na „Katar albo infekcja?”
    p.przyciski += przejscie_ekranow(_t(d_wyc, 7, 30))  # w okresie, rano po nocy z wyciekiem: K1, K2, Otwarte sprawy
    koniec = _dni(s, 13)  # ostatnia noc okresu (od pierwszej z 2 nocy wyzwalacza + 13)
    p.oczekiwane += [
        ("ekran", "dzien-pytanie-infekcja", "„Oddychasz szybciej niż zwykle” z pytaniem „Katar albo infekcja?” (S-K-30)",
         (("log", "ekran dnia: pytanie infekcja"),)),
        ("ekran", "zapisano", "▶ tak: „Zapisano”, od tej nocy okres infekcji", (("log", "infekcja;tak"),)),
        ("ekran", "K1", f"{d_wyc:%d.%m} rano po nocy z wyciekiem P95 25,5 L/min: K1 z wyciekiem nocy, a zdarzenia wycieku "
         "E-20…E-22 „nie oceniam” w okresie infekcji (nie „w porządku”, R-INF-02)",
         (("log", "okres infekcji (R-INF-02)"), ("noc_dnia", d_wyc.isoformat()))),
        ("nie_ma", "dzien-SK10", f"{d_wyc:%d.%m}: bez ekranu „Duży wyciek” S-K-10 mimo P95 25,5 L/min (R-INF-02)",
         (("od", d_wyc.strftime("%Y%m%d")), ("do", d_wyc.strftime("%Y%m%d")))),
        ("bez_linii", "duży wyciek", f"{d_wyc:%d.%m}: bez linii o dużym wycieku (E-20) na ekranie dnia (R-INF-02)",
         (("od", d_wyc.strftime("%Y%m%d")),)),
        ("linia", "Gorączka? Skontaktuj się z lekarzem.", "linia `Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij "
         "pomoc.` w pierwszym dniu okresu i co 3 dni (R-INF-10)", ()),
        ("ekran", "dzien-pytanie-gorzej", "od 3. nocy okresu pytanie „gorzej niż zwykle?” (R-INF-07)",
         (("log", "ekran dnia: pytanie gorzej"),)),
        ("nie_ma", "dzien-pytanie-infekcja", f"po końcu okresu ({koniec:%d.%m}, limit 14 nocy) karencja: S-K-30 nie wraca "
         "mimo szybszego oddechu", (("od", _dni(koniec, 1).strftime("%Y%m%d")),)),
        # karencja w logu panelu, żeby „nie wraca” nie mylić z normą, która dogoniła oddechy
        ("log", f"infekcja: karencja do {_dni(koniec, INFEKCJA_KARENCJA):%Y-%m-%d}",
         f"po końcu okresu w logu panelu `infekcja: karencja do {_dni(koniec, INFEKCJA_KARENCJA):%d.%m}` "
         "(R-INF-04.karencja, 14 dni)", ())]


def _wyjazd(p, s, w):
    # dwa wyjazdy z aparatem: AirBridge milczy od 06:00 pierwszego dnia do 04:00 czwartego (ponad 2 doby po ostatnim
    # pełnym odczycie, R-CZY-19), panel z Wi-Fi próbuje co rano. Noc poranka przed wyjazdem i po powrocie dochodzi
    # o 05:20, więc o 05:00 panel czyta listę nocy AirBridge (pełny odczyt): przed wyjazdem zapamiętuje jego chwilę,
    # po powrocie oznacza noce wyjazdu „spanie poza domem” i tego ranka pyta o okres (R-CZY-22)
    for i in range(16):
        p.noce.append(noc(_dni(s, i), w))
    p.przerwy += [(_t(s, 6, 0), _t(_dni(s, 3), 4, 0)), (_t(_dni(s, 8), 6, 0), _t(_dni(s, 11), 4, 0))]
    for i in (0, 3, 8, 11):
        p.opoznione[_dni(s, i).isoformat()] = "05:20"
    p1, p2 = _dni(s, 3), _dni(s, 11)
    p.od, p.do = _rano(s), _rano(_dni(s, 13), 12)
    # 1. wyjazd: ◀ nie (oznaczenie zdjęte), na „Zapisano” ◀ 2 s cofnij (pytanie wraca), znowu ◀ nie
    p.przyciski += _sekwencja(_t(p1, 7, 30), ["lewy", "lewy 2s", "lewy"], odstep=5)
    p.przyciski.append(f"{_t(p2, 7, 30):%Y-%m-%dT%H:%M:%S} prawy")  # 2. wyjazd: ▶ tak (oznaczenie zostaje)
    p.oczekiwane += [
        ("log", "nocy „spanie poza domem” po przerwie AirBridge", "po powrocie noce wyjazdu z czynnikiem „spanie poza "
         "domem” (R-CZY-19)", ()),
        ("ekran", "dzien-pytanie-wyjazd", "rano pytanie o okres poza domem (R-CZY-22)",
         (("log", "ekran dnia: pytanie wyjazd"),)),
        ("ekran", "zapisano", "◀ nie: „Zapisano”, oznaczenie zdjęte z nocy wyjazdu (R-CZY-23)",
         (("log", "„nie” na R-CZY-22"),)),
        ("log", "odpowiedź cofnięta: ", "◀ 2 s na „Zapisano”: odpowiedź cofnięta, pytanie wraca (R-PRZ-11)", ()),
        ("ekran", "zapisano", "2. wyjazd: ▶ tak, oznaczenie „spanie poza domem” zostaje", (("log", "wyjazd;tak"),))]


def _wyciek_maski(p, s, w):
    # 16 nocy z wyciekiem P95 25–26 L/min (ponad S-K-10, poniżej E-23) i dużym wyciekiem M-08: L-09 z warunku S-K-10
    # w 10 z 14 nocy; sprzęt bez dat wymiany, więc moduł SPRZĘT L-09 nie wstrzymuje (R-MAS-06)
    for i in range(16):
        p.noce.append(noc(_dni(s, i), w, wp95=25.0 + 0.1 * (i % 3), wmed=6.0, m08=45.0))
    p.od, p.do = _rano(_dni(s, 8)), _rano(_dni(s, 13), 12)
    d = _dni(s, 9)  # 10. ranka L-09 (warunek S-K-10 w 10 z 14 nocy)
    # ▶ na problemie: K1; ↻ ekran dnia; ↻ 2 s „wiem” → przeniesione do Otwartych spraw; ◀ 2 s cofnij; ↻ 2 s „wiem”
    # jeszcze raz
    p.przyciski += _sekwencja(_t(d, 7, 30), ["prawy", "srodkowy", "srodkowy 2s", "lewy 2s", "srodkowy 2s"])
    p.przyciski += przejscie_ekranow(_t(_dni(s, 10), 7, 30))
    p.oczekiwane += [
        ("ekran", "dzien-wzorzec-L-09", "pełny ekran „Wyciek nie ustępuje” (L-09, problem R5c) z krokiem „zapytaj "
         "dostawcę o inną maskę”", (("regula", "L-09"), ("log", "ekran dnia: wzorzec L-09"))),
        ("ekran", "przeniesione", "↻ 2 s na ekranie L-09: „wiem”, przeniesione do Otwartych spraw (R-PRZ-16)",
         (("log", "„wiem”: L-09"),)),
        ("log", "„wiem” cofnięte: L-09", "◀ 2 s na „przeniesione”: cofnięcie „wiem” (R-PRZ-16)", ()),
        ("ekran", "sprawy", "Otwarte sprawy z kartą L-09 po „wiem”", (("regula", "L-09"),))]


def _regularnosc(p, s, w):
    # po rozbiegu 18 nocy po 2 h 50 min: odsetek nocy ≥ 4 h z 30 dni spada poniżej 50% przy ≥ 90% miesiąc wcześniej
    # (L-08 wariant szybki, lekarz, pilność B; E-12 terapia nieregularna)
    # wariant szybki porównuje z miesiącem wcześniej, więc przy krótkim zrzucie najpierw dodatkowe zdrowe noce
    dosyp = max(0, 32 - w.get("nocy_zrzutu", 0))
    for i in range(dosyp):
        p.noce.append(noc(_dni(s, i), w))
    s = _dni(s, dosyp)
    for i in range(18):
        p.noce.append(noc(_dni(s, i), w, m01_min=170))
    d = _dni(s, 15)  # 16. ranka L-08 wariant szybki
    p.od, p.do = _rano(_dni(s, 12)), _rano(_dni(s, 17), 12)
    # ekran „pokaż lekarzowi”: ▶ 2 s raport dla lekarza (ściąga z kodem QR; ▶ = K1, raport przytrzymaniem), ◀ wróć;
    # ◀ 2 s „Omówione z lekarzem?”, ▶ tak
    p.przyciski += _sekwencja(_t(d, 7, 30), ["prawy 2s", "lewy", "lewy 2s", "prawy"], odstep=5)
    p.przyciski += przejscie_ekranow(_t(d, 8, 0))
    p.oczekiwane += [
        ("ekran", "dzien-wzorzec-L-08", "ekran „pokaż lekarzowi”: terapia rzadziej niż 4 h (L-08, pilność)",
         (("regula", "L-08"), ("log", "ekran dnia: wzorzec L-08"))),
        ("ekran", "raport", "▶ 2 s na ekranie L: „Raport dla lekarza” (ściąga z kodem QR do raportu A4)", ()),
        ("plik", "raport-", "raport A4 w PDF, który panel zapisał (plik SYN-raport-….pdf w katalogu scenariusza)", ()),
        ("ekran", "omowione", "◀ 2 s na ekranie L: „Omówione z lekarzem?”", ()),
        ("log", "„Omówione z lekarzem”: L-08", "▶ tak: wzorzec omówiony z lekarzem (R-PRZ-05)", ()),
        ("ekran", "sprawy", "Otwarte sprawy z L-08", (("regula", "L-08"),))]


def _cpap(w):
    z = dict(w)
    z.update({"tryb": "CPAP", "ustawienia": "CPAP/10/3", "mpi": "", "mpa": "", "ipc": "10", "epr": "3", "eai": "", "eax": "", "ans": "",
              "axs": "", "pna": "AirSense_10_Elite"})  # aparat z trybem CPAP
    z["typowe"] = {k: v for k, v in w.get("typowe", {}).items() if k not in ("emed", "ep95", "psmed", "psp95")}
    return z


def _autoset_wzor(w):
    z = _autoset(w)
    z["pna"] = "AirSense_10_AutoSet"
    z["typowe"] = {k: v for k, v in w.get("typowe", {}).items() if k not in ("emed", "ep95", "psmed", "psp95")}
    return z


def _poranek_trybu(tryb):
    def f(p, s, w):
        # rozbieg i noce scenariusza w trybie `tryb` (wzór z SCENARIUSZE): 7 nocy z AHI 13/h (OA 8,5, H 4,5);
        # 4. ranka pełny ekran E-30, o 7:30 K1, K2 i Otwarte sprawy, o 7:45 K2 i ▶ 2 s raport ogólny dla lekarza,
        # ◀ wraca do K2
        for i in range(7):
            p.noce.append(noc(_dni(s, i), w, oa=8.5, ca=0.5, h=4.5))
        d = _dni(s, 3)
        p.od, p.do = _rano(d), _rano(_dni(s, 4), 12)
        p.przyciski += przejscie_ekranow(_t(d, 7, 30))
        p.przyciski += _sekwencja(_t(d, 7, 45), ["prawy do:K2", "prawy 2s", "lewy"], odstep=5)
        p.oczekiwane += [
            ("ekran", "dzien-zdarzenie-E-30", f"pełny ekran „Noce z AHI powyżej 5” (E-30) w trybie {tryb}",
             (("tryb", tryb), ("log", "ekran dnia: zdarzenie E-30"))),
            ("ekran", "K1", f"K1 z wierszami trybu {tryb}", (("tryb", tryb),)),
            ("ekran", "K2", f"K2 z wierszami trybu {tryb}", (("tryb", tryb),)),
            ("ekran", "raport", f"▶ 2 s w K2: raport ogólny dla lekarza (ściąga z kodem QR) w trybie {tryb}",
             (("tryb", tryb),)),
            ("plik", "raport-", "raport A4 w PDF (SYN-raport-….pdf w katalogu scenariusza)", ())]
    return f


# historia odkryć: (warstwa, czynnik) po kolei, jak w teście O-12 (firmware/prototyp/test/testy_czynniki_odkrycia.cpp,
# testO12Odkrycie): alkohol 6 nocy z zaproszeniem (O) i 8 kontrolnych (K), „nic szczególnego” 10 O i 35 K, przemieszane
ODKRYCIA_HISTORIA = ([("O", "A")] * 6 + [("K", "A")] * 8 + [("O", "N")] * 10 + [("K", "N")] * 35)
ODKRYCIA_DNI = 11  # dni pokazu po historii [stała symulatora]


def _odkrycia(p, s, w):
    # 59 nocy z odpowiedziami „Co było inaczej?” (plik czynników scenariusza): po alkoholu AHI 4,9/h, bez 1,8/h →
    # O-12 „Po alkoholu: wyższe AHI” w przeglądzie tygodnia; potem noce z AHI 0,3/h → O-03 „rekordowy tydzień”
    alk = [x for x in ODKRYCIA_HISTORIA if x[1] == "A"]
    nic = [x for x in ODKRYCIA_HISTORIA if x[1] == "N"]
    kolejno = []
    for i in range(len(ODKRYCIA_HISTORIA)):  # alkohol co 4. noc
        kolejno.append(alk.pop(0) if i % 4 == 1 and alk else nic.pop(0) if nic else alk.pop(0))
    # okno pokazu (ODKRYCIA_DNI dni po historii) zawsze z 1. dniem miesiąca (S7): zdrowe noce przed historią
    s0, h0 = s, len(kolejno)
    start = _dni(s0, h0)
    pierwszy = next(_dni(start, i) for i in range(32) if _dni(start, i).day == 1)
    dosyp = max(0, (pierwszy - _dni(start, ODKRYCIA_DNI - 3)).days)  # 1. dzień najpóźniej 3 dni przed końcem okna
    for i in range(dosyp):
        p.noce.append(noc(_dni(s0, i), w))
    s = _dni(s0, dosyp)
    for i, (war, cz) in enumerate(kolejno):
        d = _dni(s, i)
        ahi = 4.9 if cz == "A" else 1.8
        p.noce.append(noc(d, w, oa=ahi * 0.6, ca=ahi * 0.15, h=ahi * 0.25))
        p.czynniki.append(f"{d.isoformat()};O;{cz};{war};-1;1")
    h = len(kolejno)
    # tydzień rekordu poniżej najniższej mediany 7 nocy w całej historii panelu: noce ważne zrzutu, rozbieg (noc() jest
    # deterministyczna po dacie, więc te same noce co w planie) i historia scenariusza; najwyżej 0,3/h, nie mniej niż 0
    hist = [a for _, a in w.get("ahi_wazne", [])]
    hist += [float(x.split(";")[3]) for x in [noc(_dni(s0, -i), w) for i in range(ROZBIEG, 0, -1)] + p.noce]
    m = min_mediana7(hist)
    ahi = round(max(0.0, min(0.3, (m if m is not None else 0.5) * 0.6)), 2)
    if m is not None and not ahi < m:
        p.znaleziska.append(f"O-03: historia zrzutu ma tydzień z medianą AHI {m:.2f}/h, niższego rekordu nie da się "
                            "wymyślić (AHI nie spada poniżej 0).")
    for i in range(ODKRYCIA_DNI):
        p.noce.append(noc(_dni(s, h + i), w, ahi=ahi, oa=ahi * 0.6, ca=ahi * 0.25, h=ahi * 0.15))
    p.od, p.do = _rano(_dni(s, h)), _rano(_dni(s, h + ODKRYCIA_DNI - 1), 12)
    for i in range(ODKRYCIA_DNI):
        # 05:10 (odkrycie jest na ekranie dnia od 05:00 do slotu dziennego o 05:30, R-KON-56): ▶ K1 z pełnym tytułem
        # odkrycia (R-CZY-20), ↻ ekran dnia; 07:30: ▶ po stronach do S8 „Twoja norma” (`do:norma`), ▶ „Sprzęt”, ↻
        p.przyciski += _sekwencja(_t(_dni(s, h + i), 5, 10), ["prawy", "srodkowy"])
        p.przyciski += _sekwencja(_t(_dni(s, h + i), 7, 30), ["prawy do:norma", "prawy", "srodkowy"])
    p.oczekiwane += [
        ("ekran", "dzien-odkrycie-O-12", "odkrycie „Po alkoholu: wyższe AHI” (O-12, S1) z przeglądu tygodnia",
         (("log", "ekran dnia: odkrycie O-12"),)),
        ("ekran", "dzien-odkrycie-O-03", "odkrycie „rekordowy tydzień”: mediana AHI z 7 nocy najniższa w historii (O-03, S1)",
         (("log", "ekran dnia: odkrycie O-03"),)),
        ("ekran", "K1", "▶ z ekranu dnia z odkryciem O-12: K1 z pełnym tytułem odkrycia (R-CZY-20)",
         (("dzien_z", "dzien-odkrycie-O-12"),)),
        ("ekran", "dzien-S7", "podsumowanie poprzedniego miesiąca 1. dnia miesiąca (S7)", (("log", "ekran dnia: S7"),)),
        ("ekran", "norma", "„Twoja norma” (S8), strona 4 / 6 kokpitu (▶ po stronach)", ())]
    # „Co znaczą te liczby” (R-EKR-51) jest tylko w profilu publicznym; ten scenariusz zostaje w prywatnym
    # (▶ na S8 → „Sprzęt”), ekran pokazuje scenariusz `profil-publiczny` albo opcja --profil-publiczny


def _profil_publiczny(p, s, w):
    # `profil=publiczny` w /konfiguracja.txt scenariusza (5.6.4). 7 nocy z AHI 13/h: od 4. ranka pełny ekran E-30
    # z tymi samymi tekstami co w profilu prywatnym (AHI i PM2.5 w obu profilach, decyzje 04.11.2026, 5.4.14);
    # ostatniego ranka o 7:30 ▶ po stronach do S8 „Twoja norma”, ▶ „Co znaczą te liczby” (R-EKR-51), ◀ S8, ↻
    p.konfiguracja.append("profil=publiczny")
    for i in range(7):
        p.noce.append(noc(_dni(s, i), w, oa=8.5, ca=0.5, h=4.5))
    p.od, p.do = _rano(_dni(s, 3)), _rano(_dni(s, 6), 12)
    p.przyciski += _sekwencja(_t(_dni(s, 6), 7, 30), ["prawy do:norma", "prawy", "lewy", "srodkowy"])
    p.oczekiwane += [
        ("log", "konfiguracja: profil publiczny", "profil publiczny z /konfiguracja.txt (5.6.4)", ()),
        ("ekran", "dzien-zdarzenie-E-30", "pełny ekran E-30 w profilu publicznym: `Noce z AHI powyżej 5` jak w "
         "prywatnym (AHI w obu profilach, 5.4.14)", (("log", "ekran dnia: zdarzenie E-30"),
                                                     ("log", "konfiguracja: profil publiczny"))),
        ("ekran", "norma", "„Twoja norma” (S8, strona 4 / 6) ze stopką `▶ co znaczą liczby`", ()),
        ("ekran", "liczby", "▶ na S8: „Co znaczą te liczby” (R-EKR-51)", (("dzien_z", "norma"),))]


# okazje ze stałą datą, których karta z warunkiem jest pochwalna i wchodzi przy zdrowych nocach (5.7.6): G08 23.09,
# G05 25.12, G04 1.01. Scenariusz `przerywniki` sięga do najbliższej z nich, bo scena z limitu 1 na 7 dni jest losowana z daty
# (R-HUM-10) i na zrzucie użytkownika mogło nie trafić się ani jedno wylosowanie z warunkiem (zapowiedź dawała BRAK)
_OKAZJE_POCHWALNE = (((9, 23), "G08"), ((12, 25), "G05"), ((1, 1), "G04"))


def _najblizsza_okazja(od):
    """(data, scena) najbliższej okazji pochwalnej nie wcześniej niż `od`."""
    kandydaci = []
    for rok in (od.year, od.year + 1):
        for (m, d), scena in _OKAZJE_POCHWALNE:
            dzien = dt.date(rok, m, d)
            if dzien >= od:
                kandydaci.append((dzien, scena))
    return min(kandydaci)


def _przerywniki(p, s, w):
    """(decyzja 05.11.2026): zdrowe noce po rozbiegu (AHI 1,2, wyciek P95 8-13 L/min, 7 h 30 z maską) od
    28 nocy do najbliższej okazji ze stałą datą z kartą pochwalną (G08 23.09, G05 25.12, G04 1.01), w dniach dowodu spokoju
    co 7 dni przerywnik S2d: karta z warunkiem, gdy noc spełnia warunek wylosowanej sceny, albo neutralna (np. G13 przy AHI
    od 1, G10, G09, G11, G14, G17), bez etykiety nad tytułem. W dniu okazji scena jest wymuszona datą, więc karta z warunkiem
    jest zawsze, także gdy losowania z limitu trafią tylko w sceny neutralne. Wyciek ponad 24 L/min nie wchodzi do
    scenariusza: ekran problemu S-K-10 i S3 wygrywa z kartą przerywnika (R-HUM-29)."""
    okazja, scena = _najblizsza_okazja(s + dt.timedelta(days=27))
    dni = (okazja - s).days + 1
    for i in range(dni):
        p.noce.append(noc(_dni(s, i), w, m01_min=450, oa=0.5, ca=0.4, h=0.3))
    p.od, p.do = _rano(s), _rano(_dni(s, dni - 1), 12)
    # odpowiedź ◀ nie na niedzielne pytanie o senność (17:05): bez niej po 4 tygodniach linia R-SKI-39 zasłania kartę
    p.przyciski += [f"{_rano(_dni(s, i), 17, 5):%Y-%m-%dT%H:%M:%S} lewy" for i in range(dni - 1) if _dni(s, i).weekday() == 6]
    p.przyciski += przejscie_ekranow(_rano(_dni(s, dni - 1), 7, 30))
    p.oczekiwane += [("ekran", "dzien-S2D-", f"karta z budzikiem: przerywnik S2d {scena} (warunkowa) w dniu okazji {okazja:%d.%m}, "
                      "noc spełnia warunek sceny", (("log", f"S2D-{scena}-warunkowa"),)),
                     ("ekran", "dzien-S2D-", "karta z budzikiem: przerywnik S2d G.. (neutralna), warunek sceny niespełniony",
                      (("log", "-neutralna"),))]


def _nawigacja(p, s, w):
    """(nowa nawigacja, decyzja 05.11.2026, spec 5.6.1): trzy zdrowe noce po rozbiegu, trzeciego ranka cały
    ciąg stron kokpitu ▶ od ekranu dnia do „Ustawień” (`1 / 6` … `6 / 6`), potem ◀ po kolei z powrotem do ekranu dnia,
    ◀ i ↻ na ekranie dnia (nic), ▶ do „Sprzęt” i ↻ prosto do ekranu dnia, na koniec ▶ do K2, ▶ 2 s raport ogólny, ◀ wraca
    do K2, ↻ ekran dnia. Bez U-03 i U-05 (dochodzi się do nich tylko ▶ 2 s z U-01 i z U-03, bo włączają Wi-Fi;
    formularz i okno serwisowe trzymają panel w czuwaniu 10–15 min, pokazują je `testy_nawigacja` i podgląd). W `spis.txt` przy każdym obrazku przycisk, który go dał."""
    for i in range(3):
        p.noce.append(noc(_dni(s, i), w))
    d = _dni(s, 2)
    p.od, p.do = _rano(d), _rano(d, 12)
    p.przyciski += _sekwencja(_t(d, 7, 30), ["prawy do:ustawienia", "lewy do:dzien", "lewy", "srodkowy",
                                            "prawy do:sprzet", "srodkowy", "prawy do:K2", "prawy 2s", "lewy",
                                            "srodkowy"], odstep=5)
    p.oczekiwane += [
        ("ekran", "K1", "▶ z ekranu dnia: K1, strona 1 / 6", ()),
        ("ekran", "K2", "▶: K2, strona 2 / 6", ()),
        ("ekran", "norma", "▶ po stronach: „Twoja norma” (S8), strona 4 / 6", ()),
        ("ekran", "sprzet", "▶: „Sprzęt”, strona 5 / 6 (▶ przesuwa kursor, za filtrem U-01)", ()),
        ("ekran", "ustawienia", "▶ na filtrze: „Ustawienia panelu”, strona 6 / 6", ()),
        ("ekran", "raport", "▶ 2 s w K2: raport ogólny dla lekarza", ())]


# nazwa → (funkcja, opis jednym zdaniem: co ma się pojawić i kiedy; rozbieg?)
SCENARIUSZE = {
    "zwykly-dzien": (_zwykly_dzien, "Twój ostatni poranek ze zrzutu bez wymyślonych nocy: ekran dnia, slot dzienny z prognozą "
                     "od godziny po imporcie nocy, obrazek dnia (z --obrazki), o 7:30 K1, K2 i Otwarte sprawy (▶ po kolei).", False),
    "przerywniki": (_przerywniki, "Po rozbiegu zdrowe noce (AHI 1,2, 7 h 30) do najbliższej okazji z kartą pochwalną (23.09, 25.12, 1.01): co 7 dni w dniu dowodu spokoju przerywnik S2d bez etykiety, "
                    "karta z warunkiem (noc spełnia warunek sceny) albo neutralna, w dniu okazji karta z warunkiem; ekran problemu, wzorzec lekarski i linia "
                    "uwagi mają pierwszeństwo (R-HUM-29).", True),
    "wyciek": (_wyciek, "Pierwsza noc po rozbiegu ma wyciek P95 38 L/min (E-23 „Wyciek od 27 l/min w nocy”): od rana do "
               "wieczora pełny ekran E-23 (S3, slot dzienny go nie wypiera), o 7:30 K1, K2 i Otwarte sprawy "
               "z kartą E-23.", True),
    "ahi-rosnie": (_ahi_rosnie, "Od pierwszej nocy po rozbiegu AHI 14/h: 4. i 5. poranka pełny ekran E-30 „Noce z AHI "
                   "powyżej 5” (mediana z 7 nocy ≥ 5/h), potem przez 5 dni limit 2 razy w tygodniu (R-KON-14) i linia "
                   "`Wciąż: Noce z AHI powyżej 5.` (rano ustępuje zaproszeniu „Co było inaczej?”), 8. poranka ekran "
                   "„pokaż lekarzowi” L-03 (mediana z 14 nocy ≥ 10/h; wraca co 7 dni), 11. i 12. znowu E-30, ostatniego "
                   "ranka Otwarte sprawy.", True),
    "centralne": (_centralne, "Od pierwszej nocy po rozbiegu CA 7,5/h, AHI rośnie razem z CA: pełne ekrany S3 według "
                  "kolejności 7.2 (E-31, potem E-30 dwa razy, a w dniach limitu E-30 pełny ekran E-33 „Więcej bezdechów "
                  "centralnych” z linią `Wciąż: Noce z AHI powyżej 5.`), od 5. poranka L-01 w trybie „tylko raport” "
                  "(bez ekranu i pilności, karta w Otwartych sprawach); 7. ranka Otwarte sprawy.", True),
    "krotkie-noce": (_krotkie_noce, "Po rozbiegu noc 2,5 h (E-11): linia uwagi „Krótka noc”; potem noc 5 minut: linia "
                     "`Noc {data} bez terapii.` (E-10); po drugiej takiej nocy pełny ekran „Noce bez terapii: 2” "
                     "(E-10, S3).", True),
    "brak-danych": (_brak_danych, "Po rozbiegu AirBridge przestaje odpowiadać: rano S10 „Synchronizuję się”, od 11:00 "
                    "S5 „brak danych” (E-01), następnego dnia znowu S10 rano i od 11:00 S5 „Brak danych od 2 nocy”.", True),
    "sennosc": (_sennosc, "W niedzielę o 17:00 pytanie o senność, scenariusz odpowiada ▶ tak o 17:05 w dwie kolejne "
                "niedziele: po pierwszej linia uwagi o senności, zaraz po drugiej ekran „pokaż lekarzowi” L-11.", True),
    "co2": (_co2, "Atrapa źródła otoczenia podaje CO2 sypialni 1080–1350 ppm od 17:00: o 21:00 drugiego dnia ekran "
            "„przewietrz sypialnię” S-K-01 (pierwszego dnia panel już prosił o czynniki nocy, a prośba jest jedna na "
            "dzień, R-PIE-10).", True),
    "sprzet": (_sprzet, "Maska ponad 6 miesięcy od wymiany (/konfiguracja.txt scenariusza, z ignorowanym wierszem rury), po rozbiegu 14 nocy "
               "z wyciekiem P95 rosnącym od 24,5 do 26,6 L/min i dużym wyciekiem M-08: ekran „Maska się zużywa” "
               "(S-K-31), L-09 wstrzymane, dopóki S-K-31 ma warunek (R-MAS-03); ▶ po stronach do „Sprzęt” (5 / 6); potem 7 nocy "
               "po 9,5 L/min: L-09 wychodzi, bo spadek wycieku kasuje warunek S-K-31, a po 5. nocy pytanie "
               "„Wymieniona maska?” (▶ tak); dzień później w „Sprzęt” (kursor na masce) ◀ 2 s „wymienione dziś”, ↻ 2 s "
               "„nowy model maski” z ekranem R-MAS-14 i ◀ 2 s cofnij, jeszcze raz ↻ 2 s; następnego ranka linia "
               "`Nowy model maski. Uczę się normy.`", True),
    "sprzet-zuzycie": (_sprzet_zuzycie, "Maska po terminie, po rozbiegu 26 nocy z wyciekiem P95 rosnącym od 24,5 "
                       "do 26,9 L/min bez przerwy: S-K-31 „Maska się zużywa” raz na 7 dni (3 razy), L-09 „Wyciek nie "
                       "ustępuje” wstrzymane, a gdy norma dogoni wyciek, S-K-31 traci warunek i L-09 wychodzi; "
                       "4. pokazania S-K-31 (R-MAS-10) te dane nie dają.", True),
    "infekcja": (_infekcja, "Po rozbiegu 20 nocy z oddechem 19/min (norma ~15/min) i wyższym ciśnieniem P95: drugiego "
                 "ranka „Oddychasz szybciej niż zwykle” z pytaniem „Katar albo infekcja?” (S-K-30), ▶ tak o 7:30 otwiera "
                 "okres infekcji (7. noc z wyciekiem 25,5 L/min: bez S-K-10 i linii E-20, „nie oceniam”, nie „w porządku”, linia "
                 "`Gorączka? …`, od 3. nocy pytanie „gorzej”), 7. ranka K1, K2 i Otwarte sprawy w okresie; po 14 nocach "
                 "okres się kończy i przez 14 nocy karencji S-K-30 nie wraca.", True),
    "wyjazd": (_wyjazd, "Po rozbiegu dwa wyjazdy z aparatem: AirBridge milczy ponad 2 doby (Wi-Fi jest), po powrocie "
               "noce z wyjazdu dochodzą z czynnikiem „spanie poza domem”, a rano pytanie o ten okres "
               "(R-CZY-22). Pierwszy raz ◀ nie, ◀ 2 s cofnięcie na „Zapisano” (pytanie wraca) i znowu ◀ nie; drugi raz "
               "▶ tak. Uwaga: noc poranka przed wyjazdem i po powrocie symulator dokłada dopiero o 05:20 (żeby panel przeczytał "
               "listę nocy AirBridge), więc o 05:00 tych dni jest S10 „Synchronizuję się”, którego na panelu by nie było.",
               True),
    "wyciek-maski": (_wyciek_maski, "Po rozbiegu 16 nocy z wyciekiem P95 25 L/min i dużym wyciekiem (sprzęt bez dat wymiany): "
                     "ekran „Wyciek nie ustępuje” (L-09, problem), 10. ranka ▶ K1, ↻ 2 s „wiem” z cofnięciem ◀ 2 s i znowu "
                     "„wiem”, 11. ranka K1, K2 i Otwarte sprawy z L-09.", True),
    "regularnosc": (_regularnosc, "Po rozbiegu 18 nocy po 2 h 50 min: ekran „pokaż lekarzowi” L-08 (terapia rzadziej niż "
                    "4 h, wariant szybki), 16. ranka ▶ 2 s „Raport dla lekarza” (ściąga z kodem QR), ◀ wróć, ◀ 2 s "
                    "„Omówione z lekarzem?” i ▶ tak, o 8:00 K1, K2 i Otwarte sprawy.", True),
    "autoset": (_poranek_trybu("AutoSet"), "Rozbieg i noce w trybie AutoSet (bez EPAP i wsparcia PS): 7 nocy z AHI 13/h, "
                "4. ranka pełny ekran „Noce z AHI powyżej 5” (E-30), o 7:30 K1, K2 i Otwarte sprawy, o 7:45 K2 i ▶ 2 s raport "
                "dla lekarza z kodem QR (PDF w katalogu scenariusza).", True, _autoset_wzor),
    "cpap": (_poranek_trybu("CPAP"), "To samo w trybie CPAP (stałe ciśnienie): 4. ranka E-30, K1, K2, Otwarte sprawy i "
             "raport dla lekarza.", True, _cpap),
    "odkrycia": (_odkrycia, "Po rozbiegu 59 nocy z odpowiedziami „Co było inaczej?” (14 po alkoholu z AHI 4,9/h, 45 bez "
                 "z AHI 1,8/h; odpowiedzi wymyślone przez scenariusz), potem 11 dni pokazu z AHI poniżej najniższego tygodnia historii (najwyżej 0,3/h): odkrycie O-12 "
                 "„Po alkoholu: wyższe AHI” z przeglądu tygodnia, O-03 „rekordowy tydzień”, 1. dnia miesiąca S7; co rano "
                 "o 5:10 ▶ K1 (z odkryciem) i ↻, o 7:30 ▶ po stronach do „Twoja norma” (S8), ▶ (profil prywatny: „Sprzęt”; „Co znaczą te "
                 "liczby” pokazuje scenariusz profil-publiczny), ↻ ekran dnia.", True),
    "profil-publiczny": (_profil_publiczny, "Profil publiczny (`profil=publiczny` w /konfiguracja.txt, 5.6.4): 7 nocy z AHI "
                         "13/h, od 4. ranka pełny ekran E-30 `Noce z AHI powyżej 5` (teksty ekranów dnia jak w profilu "
                         "prywatnym, AHI i PM2.5 w obu profilach, 5.4.14); ostatniego ranka o 7:30 ▶ po stronach do „Twoja "
                         "norma” (S8), ▶ „Co znaczą te liczby” (R-EKR-51), ◀ S8, ↻ ekran dnia.", True),
    "zmiana-trybu": (_zmiana_trybu, "Po rozbiegu trzy noce w trybie AutoSet zamiast trybu ze zrzutu: K1 i K2 z wierszami "
                     "według trybu (bez EPAP i wsparcia PS), pierwszego i trzeciego ranka o 7:30.", True),
    "nawigacja": (_nawigacja, "Nowa nawigacja: trzeciego ranka po rozbiegu ▶ przez wszystkie strony kokpitu "
                  "1 / 6 … 6 / 6, ◀ z powrotem do ekranu dnia, ↻ ze strony „Sprzęt” prosto do ekranu "
                  "dnia, ▶ 2 s w K2 raport ogólny; w spisie przy każdym obrazku przycisk, który go dał.", True),
}
KOLEJNOSC = list(SCENARIUSZE)


OBRAZEK_DNIA = ("ekran", "dzien-SK38", "obrazek dnia z --obrazki wczytany i pokazany (S-K-38)",
                (("log", "slot dzienny: S-K-38 /obrazki/"),))


def plan(dane, nazwy, obrazki=False):
    """Plan dla scenariuszy `nazwy` po kolei: rozbieg raz na początku, każdy następny od dnia po ostatniej nocy
    poprzedniego. Okres: od pierwszego `od` do ostatniego `do`."""
    nieznane = [n for n in nazwy if n not in SCENARIUSZE]
    if nieznane:
        raise BladScenariusza(f"nieznany scenariusz: {', '.join(nieznane)}. Do wyboru: {', '.join(KOLEJNOSC)}")
    if len(set(nazwy)) != len(nazwy):
        raise BladScenariusza("ten sam scenariusz podany dwa razy")
    w = wzor(dane)
    p = Plan()
    dzien = w["data"] + dt.timedelta(days=1)
    rozbieg = False
    for n in nazwy:
        f, opis, z_rozbiegiem, *wzor_ = SCENARIUSZE[n]
        wn = wzor_[0](w) if wzor_ else w  # scenariusz w innym trybie aparatu (rozbieg też, gdy pierwszy)
        if n == "zwykly-dzien" and (len(nazwy) > 1):
            raise BladScenariusza("scenariusz zwykly-dzien działa tylko sam (to Twój ostatni poranek ze zrzutu)")
        if z_rozbiegiem and not rozbieg:
            for i in range(ROZBIEG):
                p.noce.append(noc(dzien + dt.timedelta(days=i), wn))
            p.opis.append(f"Rozbieg: {ROZBIEG} zdrowych nocy syntetycznych z pełnymi metrykami od poranka "
                          f"{dzien.isoformat()} (norma osobista potrzebuje co najmniej 14 nocy z metryką).")
            dzien += dt.timedelta(days=ROZBIEG)
            rozbieg = True
        q = Plan()
        f(q, dzien, wn)
        p.noce += q.noce
        p.przyciski += q.przyciski
        if q.otoczenie:
            p.otoczenie, p.otoczenie_od, p.otoczenie_do = q.otoczenie, q.otoczenie_od, q.otoczenie_do
        p.milczy_od = p.milczy_od or q.milczy_od
        p.konfiguracja += q.konfiguracja
        p.czynniki += q.czynniki
        p.przerwy += q.przerwy
        p.opoznione.update(q.opoznione)
        p.oczekiwane += q.oczekiwane
        p.znaleziska += q.znaleziska
        if wzor_:
            p.aparat = (wn["pna"], wn["tryb"])
        p.od = p.od or q.od
        p.do = q.do
        p.opis.append(f"Scenariusz {n} od poranka {dzien.isoformat()}: {opis}")
        # następny scenariusz od dnia po ostatniej nocy (albo po końcu okresu, gdy scenariusz nie ma nocy)
        ost = dt.date.fromisoformat(p.noce[-1][:10]) if q.noce else q.do.date()
        dzien = max(ost, q.do.date()) + dt.timedelta(days=1)
    if obrazki and "zwykly-dzien" in nazwy:
        p.oczekiwane.append(OBRAZEK_DNIA)
    if p.milczy_od and len(nazwy) > 1:
        raise BladScenariusza("scenariusz brak-danych działa tylko sam (AirBridge milczy do końca okresu)")
    return p
