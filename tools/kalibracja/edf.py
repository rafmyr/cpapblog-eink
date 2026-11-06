# SPDX-License-Identifier: GPL-3.0-or-later
"""Czytnik plików EDF i EDF+ (z adnotacjami) z karty SD aparatu ResMed.

Tylko biblioteka standardowa Pythona 3. Plik otwieramy wyłącznie do odczytu.

Zasady przyjęte za AirBridge (`src/report_edf.cpp`, repo m-kozlowski/airbridge):
- data i godzina startu z nagłówka to czas lokalny aparatu bez strefy;
  liczymy je jako „sekundy cywilne” od 1970-01-01 00:00 (bez przeliczania strefy),
  tak jak `Air10Clock::civil_seconds`;
- próbka jest poprawna, gdy jej wartość cyfrowa mieści się w zakresie
  [cyfrowe minimum, cyfrowe maksimum] sygnału;
- wartość fizyczna = fiz_min + (cyfrowa − cyf_min) × (fiz_max − fiz_min) / (cyf_max − cyf_min);
- sygnały „Crc16” i „EDF Annotations” nie są sygnałami pomiarowymi.
"""

import datetime as _dt
import os
import struct
import sys
from array import array
from collections import Counter

EPOKA = _dt.datetime(1970, 1, 1)


class BladEDF(Exception):
    """Plik nie jest poprawnym plikiem EDF."""


class Sygnal:
    __slots__ = ("indeks", "etykieta", "jednostka", "fiz_min", "fiz_max", "cyf_min",
                 "cyf_max", "probki", "offset")

    def __init__(self, indeks, etykieta, jednostka, fiz_min, fiz_max, cyf_min, cyf_max, probki):
        self.indeks = indeks
        self.etykieta = etykieta
        self.jednostka = jednostka
        self.fiz_min = fiz_min
        self.fiz_max = fiz_max
        self.cyf_min = cyf_min
        self.cyf_max = cyf_max
        self.probki = probki          # próbek w jednym rekordzie danych
        self.offset = 0               # przesunięcie sygnału w rekordzie, w bajtach

    @property
    def adnotacje(self):
        return self.etykieta == "EDF Annotations"

    @property
    def crc(self):
        return self.etykieta == "Crc16"

    def poprawna(self, cyfrowa):
        return not self.adnotacje and not self.crc and self.cyf_min <= cyfrowa <= self.cyf_max

    def fizyczna(self, cyfrowa, skala=1.0):
        """Wartość fizyczna; `skala` mnoży granice fizyczne przed przeliczeniem, jak AirBridge
        (L/s → L/min, L → mL), żeby próbka równa progowi nie przekroczyła go przez zaokrąglenie."""
        lo, hi = self.fiz_min * skala, self.fiz_max * skala
        return lo + (cyfrowa - self.cyf_min) * (hi - lo) / (self.cyf_max - self.cyf_min)


def _tekst(b):
    return b.decode("latin-1").strip()


def _liczba(b, nazwa):
    t = _tekst(b)
    try:
        return float(t)
    except ValueError:
        raise BladEDF("pole %s nie jest liczbą: %r" % (nazwa, t))


def _start(data, godzina):
    """Data „dd.mm.yy” i godzina „hh.mm.ss” z nagłówka → datetime bez strefy."""
    try:
        d, m, r = (int(x) for x in data.split("."))
        g, mi, s = (int(x) for x in godzina.split("."))
    except ValueError:
        raise BladEDF("zła data albo godzina startu: %r %r" % (data, godzina))
    rok = 1900 + r if r >= 85 else 2000 + r
    return _dt.datetime(rok, m, d, g, mi, s)


class PlikEDF:
    """Nagłówek pliku EDF i dostęp do danych.

    Atrybuty: sciezka, start (datetime), start_s (sekundy cywilne), czas_rekordu (s),
    rekordy, sygnaly (lista Sygnal), edf_plus (bool), niepelny (bool: rozmiar pliku nie
    zgadza się z nagłówkiem, np. plik w trakcie zapisu).
    """

    def __init__(self, sciezka):
        self.sciezka = sciezka
        rozmiar = os.path.getsize(sciezka)
        with open(sciezka, "rb") as f:
            stale = f.read(256)
            if len(stale) < 256:
                raise BladEDF("plik krótszy niż nagłówek")
            if _tekst(stale[0:8]) != "0":
                raise BladEDF("nieznana wersja EDF")
            self.start = _start(_tekst(stale[168:176]), _tekst(stale[176:184]))
            self.start_s = int((self.start - EPOKA).total_seconds())
            bajty_naglowka = int(_liczba(stale[184:192], "długość nagłówka"))
            zastrzezone = _tekst(stale[192:236])
            self.edf_plus = zastrzezone.startswith("EDF+")
            self.edf_plus_d = zastrzezone.startswith("EDF+D")
            rekordy = int(_liczba(stale[236:244], "liczba rekordów"))
            self.czas_rekordu = _liczba(stale[244:252], "czas rekordu")
            ns = int(_liczba(stale[252:256], "liczba sygnałów"))
            if ns < 1 or bajty_naglowka != 256 + 256 * ns:
                raise BladEDF("zły nagłówek: %d sygnałów, %d bajtów" % (ns, bajty_naglowka))
            opis = f.read(256 * ns)
            if len(opis) < 256 * ns:
                raise BladEDF("plik krótszy niż opis sygnałów")

        def kol(poczatek, szer, i):
            o = poczatek * ns + i * szer
            return opis[o:o + szer]

        self.sygnaly = []
        offset = 0
        for i in range(ns):
            s = Sygnal(
                i,
                _tekst(kol(0, 16, i)),
                _tekst(kol(96, 8, i)),
                _liczba(kol(104, 8, i), "fiz. min"),
                _liczba(kol(112, 8, i), "fiz. max"),
                int(_liczba(kol(120, 8, i), "cyf. min")),
                int(_liczba(kol(128, 8, i), "cyf. max")),
                int(_liczba(kol(216, 8, i), "próbki w rekordzie")),
            )
            if s.cyf_min >= s.cyf_max or s.fiz_min == s.fiz_max or s.probki < 1:
                raise BladEDF("zły opis sygnału %s" % s.etykieta)
            s.offset = offset
            offset += s.probki * 2
            self.sygnaly.append(s)
        self.bajty_naglowka = bajty_naglowka
        self.bajty_rekordu = offset
        w_pliku = (rozmiar - bajty_naglowka) // self.bajty_rekordu if self.bajty_rekordu else 0
        if rekordy < 0:            # -1: plik w trakcie zapisu
            self.rekordy = max(0, w_pliku)
            self.niepelny = True
        else:
            self.rekordy = min(rekordy, max(0, w_pliku))
            self.niepelny = rozmiar != bajty_naglowka + rekordy * self.bajty_rekordu
        self._dane = None

    # -- dane ---------------------------------------------------------------
    def _wczytaj(self):
        if self._dane is None:
            with open(self.sciezka, "rb") as f:
                f.seek(self.bajty_naglowka)
                self._dane = f.read(self.rekordy * self.bajty_rekordu)
        return self._dane

    def sygnal(self, etykieta):
        for s in self.sygnaly:
            if s.etykieta == etykieta and not s.crc:
                return s
        return None

    @property
    def koniec_s(self):
        return self.start_s + self.rekordy * self.czas_rekordu

    def surowe(self, s, bez_znaku=False):
        """Wszystkie próbki sygnału jako liczby cyfrowe (lista list: jeden rekord = lista)."""
        dane = self._wczytaj()
        fmt = "<%d%s" % (s.probki, "H" if bez_znaku else "h")
        out = []
        for r in range(self.rekordy):
            o = r * self.bajty_rekordu + s.offset
            out.append(struct.unpack_from(fmt, dane, o))
        return out

    def _tablica(self):
        """Cały obszar danych jako tablica 16-bitowych liczb ze znakiem (little-endian)."""
        if getattr(self, "_tab", None) is None:
            t = array("h")
            dane = self._wczytaj()
            t.frombytes(dane[:len(dane) - len(dane) % 2])
            if sys.byteorder != "little":
                t.byteswap()
            self._tab = t
        return self._tab

    def rekordy_probek(self, s):
        """Próbki sygnału rekord po rekordzie (każdy element: tablica liczb cyfrowych)."""
        t = self._tablica()
        slowa = self.bajty_rekordu // 2
        o = s.offset // 2
        for r in range(self.rekordy):
            p = r * slowa + o
            yield t[p:p + s.probki]

    def zliczenia(self, s):
        """Licznik wartości cyfrowych sygnału w całym pliku (jak histogram AirBridge)."""
        c = Counter()
        for rek in self.rekordy_probek(s):
            c.update(rek)
        return c

    def adnotacje(self):
        """Adnotacje EDF+: lista (onset_s, czas_trwania_s albo None, tekst).

        Pomija adnotacje bez tekstu (znaczniki czasu rekordu). Onset liczony od startu pliku.
        """
        s = self.sygnal("EDF Annotations")
        if s is None:
            return []
        dane = self._wczytaj()
        out = []
        for r in range(self.rekordy):
            o = r * self.bajty_rekordu + s.offset
            blok = dane[o:o + s.probki * 2]
            for tal in blok.split(b"\x00"):
                if not tal:
                    continue
                czesci = tal.split(b"\x14")
                czas = czesci[0]
                if not czas or czas[:1] not in (b"+", b"-"):
                    raise BladEDF("zła adnotacja w %s: %r" % (self.sciezka, tal[:40]))
                if b"\x15" in czas:
                    on, dur = czas.split(b"\x15", 1)
                    try:
                        trwanie = float(dur)
                    except ValueError:
                        raise BladEDF("zły czas trwania adnotacji w %s: %r" % (self.sciezka, dur))
                    if trwanie < 0:      # AirBridge odrzuca taki rekord (report_edf.cpp)
                        raise BladEDF("ujemny czas trwania adnotacji w %s" % self.sciezka)
                else:
                    on, trwanie = czas, None
                onset = float(on)
                for tekst in czesci[1:]:
                    if tekst:
                        out.append((onset, trwanie, tekst.decode("latin-1").strip()))
        return out


def percentyl_z_licznikow(pary, procent):
    """Jak `percentyl`, ale z par (wartość, liczba próbek) posortowanych rosnąco."""
    n = sum(k for _, k in pary)
    if not n:
        return None
    ranga = (n // 100) * procent + ((n % 100) * procent + 99) // 100
    ranga = max(1, min(n, ranga))
    for v, k in pary:
        if ranga <= k:
            return v
        ranga -= k
    return pary[-1][0]


def percentyl(wartosci_posortowane, procent):
    """Percentyl metodą najbliższej rangi, jak `ReportEdf::percentile` w AirBridge:
    ranga = ceil(n × p / 100), wartość z tej pozycji w kolejności rosnącej."""
    n = len(wartosci_posortowane)
    if not n:
        return None
    ranga = (n // 100) * procent + ((n % 100) * procent + 99) // 100
    ranga = max(1, min(n, ranga))
    return wartosci_posortowane[ranga - 1]
