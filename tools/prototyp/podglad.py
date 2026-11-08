#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Odbiera zrzut ekranu z panelu (port szeregowy) i zapisuje PNG 800x480.

Firmware z ZRZUT_EKRANU=1 po każdym narysowaniu wysyła bufor w base64 między
liniami ZRZUT_START i ZRZUT_KONIEC. Flagę ma tylko obraz deweloperski
(`pio run -e e1001_dev -t upload`, platformio.ini); zwykły `e1001` zrzutu nie wysyła.
Bez Pillow: PNG pisany ręcznie (zlib).
Użycie (pyserial jest w środowisku PlatformIO):
  ~/.platformio/penv/bin/python tools/prototyp/podglad.py /dev/cu.usbserial-110 [--reset] [--czas 90]
Wynik: firmware/prototyp/zrzuty/RRRRMMDD-GGMMSS.png (folder w .gitignore).
"""
import argparse, base64, os, struct, sys, time, zlib
import serial

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def png(sciezka, dane, w=800, h=480):
    wiersz = w // 8
    surowe = b"".join(b"\x00" + bytes(255 - b for b in dane[y * wiersz:(y + 1) * wiersz]) for y in range(h))
    def blok(typ, d):
        return struct.pack(">I", len(d)) + typ + d + struct.pack(">I", zlib.crc32(typ + d) & 0xFFFFFFFF)
    with open(sciezka, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + blok(b"IHDR", struct.pack(">IIBBBBB", w, h, 1, 0, 0, 0, 0))
                + blok(b"IDAT", zlib.compress(surowe, 9)) + blok(b"IEND", b""))


def rozpakuj(d):
    """PackBits: n < 128 → n+1 bajtów dosłownie, n > 128 → następny bajt 257-n razy."""
    out, i = bytearray(), 0
    while i < len(d):
        n = d[i]; i += 1
        if n < 128:
            out += d[i:i + n + 1]; i += n + 1
        elif n > 128:
            out += bytes([d[i]]) * (257 - n); i += 1
    return bytes(out)


def main():
    a = argparse.ArgumentParser()
    a.add_argument("port")
    a.add_argument("--reset", action="store_true", help="zrestartuj panel przed odbiorem")
    a.add_argument("--czas", type=int, default=90, help="ile sekund słuchać (zbiera wszystkie zrzuty)")
    a.add_argument("--jeden", action="store_true", help="zakończ po pierwszym zrzucie i zaśnięciu panelu")
    a = a.parse_args()
    s = serial.Serial(a.port, 115200, timeout=1)
    if a.reset:
        s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
    koniec = time.time() + a.czas
    zbieram, b64, zapisane, packbits = False, [], 0, False
    while time.time() < koniec:
        l = s.readline().decode("utf-8", "replace").rstrip()
        if not l:
            continue
        if l.startswith("ZRZUT_START"):
            zbieram, b64, packbits = True, [], "packbits" in l
        elif l == "ZRZUT_KONIEC" and zbieram:
            zbieram = False
            dane = base64.b64decode("".join(b64))
            if packbits:
                dane = rozpakuj(dane)
            os.makedirs(os.path.join(REPO, "firmware", "prototyp", "zrzuty"), exist_ok=True)
            p = os.path.join(REPO, "firmware", "prototyp", "zrzuty", time.strftime("%Y%m%d-%H%M%S") + f"-{zapisane}.png")
            png(p, dane)
            zapisane += 1
            print("zapisano", p, flush=True)
        elif zbieram:
            b64.append(l)
        else:
            print(l, flush=True)
            if l.startswith("sen na") and a.jeden and zapisane:  # --jeden: koniec po pierwszym zrzucie
                break
    sys.exit(0 if zapisane else 1)


if __name__ == "__main__":
    main()
