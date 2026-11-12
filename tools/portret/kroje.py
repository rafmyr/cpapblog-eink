# SPDX-License-Identifier: GPL-3.0-or-later
"""Kroje panelu z firmware/prototyp/include/fonty.h (bitmapy GFX i szerokości w 1/16 px) dla wzorca Portretu.

Tekst stawiany jak `tekst()` i `szer()` w firmware/prototyp/src/ekrany.cpp: pozycja znaku w 1/16 px, zaokrąglana
dopiero przy stawianiu glifu, znaki spoza ASCII z listy FONT_EXTRA od kodu 0x7F, twarda spacja jak spacja. Ikony
przycisków (◀ ▶ ↻) i strzałki nie są potrzebne w treści Portretu, więc ich tu nie ma (znak spoza kroju = `?`).
"""

import os
import re

_REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FONTY_H = os.path.join(_REPO, "firmware", "prototyp", "include", "fonty.h")


class Kroj:
    def __init__(self, nazwa, bitmapa, glify, first, last, adv16):
        self.nazwa = nazwa
        self.bitmapa = bitmapa
        self.glify = glify      # lista (offset, w, h, xAdv, xOff, yOff)
        self.first = first
        self.last = last
        self.adv16 = adv16

    def glif(self, c):
        if c < self.first or c > self.last:
            return None
        return self.glify[c - self.first]

    def a16(self, c):
        if c < self.first or c > self.last:
            return 0
        return self.adv16[c - self.first] if self.adv16 else self.glify[c - self.first][3] * 16


_cache = {}
_extra = None


def _tablica(tekst, nazwa, typ):
    m = re.search(r"const %s %s\[\] PROGMEM = \{(.*?)\};" % (typ, re.escape(nazwa)), tekst, re.S)
    if not m:
        raise KeyError(nazwa)
    return m.group(1)


def kroj(nazwa):
    """Krój o nazwie z fonty.h (np. "P28B")."""
    global _extra
    if nazwa in _cache:
        return _cache[nazwa]
    with open(FONTY_H, encoding="utf-8") as f:
        t = f.read()
    if _extra is None:
        _extra = re.search(r'FONT_EXTRA\[\] = "(.*?)";', t).group(1)
    bmp = bytes(int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", _tablica(t, nazwa + "_bmp", "uint8_t")))
    gl = [tuple(int(v) for v in g.split(","))
          for g in re.findall(r"\{(-?\d+,-?\d+,-?\d+,-?\d+,-?\d+,-?\d+)\}", _tablica(t, nazwa + "_gl", "GFXglyph"))]
    m = re.search(r"const GFXfont %s PROGMEM = \{\(uint8_t\*\)\w+,\(GFXglyph\*\)\w+,(0x[0-9A-Fa-f]+),(0x[0-9A-Fa-f]+),"
                  % re.escape(nazwa), t)
    first, last = int(m.group(1), 16), int(m.group(2), 16)
    try:
        adv = [int(x) for x in re.findall(r"\d+", _tablica(t, nazwa + "_adv16", "uint16_t"))]
    except KeyError:
        adv = None
    k = Kroj(nazwa, bmp, gl, first, last, adv)
    _cache[nazwa] = k
    return k


def kod(ch):
    cp = ord(ch)
    if cp == 0xA0:
        return 0x20
    if 0x20 <= cp < 0x7F:
        return cp
    if _extra is None:
        kroj("P22B")
    i = _extra.find(ch)
    return 0x7F + i if i >= 0 else ord("?")


def szer(k, t):
    w16 = 0
    for ch in t:
        c = kod(ch)
        if k.glif(c) is not None:
            w16 += k.a16(c)
    return (w16 + 8) >> 4


def wersalik(k):
    g = k.glif(ord("H"))
    return g[2] if g and g[2] else 12


def tekst(obraz, k, x, y, t, czarny=True):
    """Tekst od x na linii bazowej y; zwraca x końca."""
    x16 = x * 16
    for ch in t:
        c = kod(ch)
        g = k.glif(c)
        if g is None:
            continue
        x0 = (x16 + 8) >> 4
        off, w, h, _, xo, yo = g
        bit = 0
        bits = 0
        for yy in range(h):
            for xx in range(w):
                if not (bit & 7):
                    bits = k.bitmapa[off]
                    off += 1
                bit += 1
                if bits & 0x80:
                    obraz.piksel(x0 + xo + xx, y + yo + yy, czarny)
                bits = (bits << 1) & 0xFF
        x16 += k.a16(c)
    return (x16 + 8) >> 4


def tekst_srodek(obraz, k, xs, y, t, czarny=True):
    tekst(obraz, k, xs - szer(k, t) // 2, y, t, czarny)


def tekst_prawo(obraz, k, xp, y, t, czarny=True):
    tekst(obraz, k, xp - szer(k, t), y, t, czarny)


def zawin(k, t, max_w):
    """Zawijanie słów jak zawin() panelu; twarda spacja nie łamie."""
    wiersze, linia = [], ""
    for slowo in t.split(" "):
        if not slowo:
            continue
        proba = slowo if not linia else linia + " " + slowo
        if szer(k, proba) > max_w and linia:
            wiersze.append(linia)
            linia = slowo
        else:
            linia = proba
    if linia:
        wiersze.append(linia)
    return wiersze
