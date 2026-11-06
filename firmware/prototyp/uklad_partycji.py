# SPDX-License-Identifier: GPL-3.0-or-later
# Skrypt PlatformIO (extra_scripts = pre:uklad_partycji.py): identyfikator tablicy partycji wkompilowany w firmware.
# Czyta plik z `board_build.partitions`, liczy identyfikator tym samym
# algorytmem co rdzeń (src/magazyn.cpp identyfikatorUkladu(), tablicaZCsv()) i adapter ESP32 z tablicy panelu
# (src/adaptery_esp32.cpp PlikiEsp32::partycja()) i podaje go jako -DUKLAD_PARTYCJI=0x…. Firmware porównuje go przy
# starcie z tablicą, z której panel wystartował: inna tablica = stary układ, bez formatowania i migracji.
# Brak albo błąd pliku tablicy przerywa budowę (src/magazyn.cpp ma #error bez UKLAD_PARTYCJI na panelu).
#
# Bez PlatformIO moduł da się zaimportować (testy w tools/test_wgraj_pliki.py):
#   python3 firmware/prototyp/uklad_partycji.py firmware/partycje-e1001.csv   → identyfikator szesnastkowo
import os
import sys
import zlib

TYPY = {"app": 0, "data": 1}
PODTYPY_APP = {"factory": 0x00, "test": 0x20, "ota_0": 0x10, "ota_1": 0x11, "ota_2": 0x12, "ota_3": 0x13}
PODTYPY_DATA = {"ota": 0x00, "phy": 0x01, "nvs": 0x02, "coredump": 0x03, "nvs_keys": 0x04, "efuse": 0x05,
                "undefined": 0x06, "esphttpd": 0x80, "fat": 0x81, "spiffs": 0x82, "littlefs": 0x83}


def liczba(t):
    """Liczba z tablicy partycji: dziesiętna albo 0x…, z przyrostkiem K albo M (jak liczbaTablicy() w rdzeniu)."""
    t = t.strip()
    mn = 1
    if t[-1:] in ("K", "k"):
        mn, t = 1024, t[:-1]
    elif t[-1:] in ("M", "m"):
        mn, t = 1024 * 1024, t[:-1]
    if not t or len(t) > 10 or t[0] in "+-":
        raise ValueError("liczba: %r" % t)
    v = int(t, 16) if t.lower().startswith("0x") else (int(t, 8) if len(t) > 1 and t[0] == "0" else int(t, 10))
    v *= mn
    if v > 0xFFFFFFFF:
        raise ValueError("liczba poza zakresem: %r" % t)
    return v


def kod(t, nazwy):
    if t in nazwy:
        return nazwy[t]
    v = liczba(t)
    if v > 254:
        raise ValueError("kod poza zakresem: %r" % t)
    return v


def tablica_z_csv(tekst):
    """Wiersze (nazwa, typ, podtyp, przesunięcie, rozmiar); ValueError przy wierszu nieczytelnym albo pustej tablicy."""
    w = []
    for linia in tekst.splitlines():
        linia = linia.strip()
        if not linia or linia.startswith("#"):
            continue
        p = [x.strip() for x in linia.split(",")]
        if len(p) < 5 or not p[0] or len(p[0]) > 16:
            raise ValueError("wiersz tablicy: %r" % linia)
        typ = kod(p[1], TYPY)
        podtyp = kod(p[2], PODTYPY_APP if typ == 0 else PODTYPY_DATA)
        off, rozmiar = liczba(p[3]), liczba(p[4])
        if rozmiar == 0:
            raise ValueError("rozmiar 0: %r" % linia)
        w.append((p[0], typ, podtyp, off, rozmiar))
    if not w:
        raise ValueError("pusta tablica")
    return w


def identyfikator(tablica):
    """CRC-32 tekstu „{nazwa} {typ} {podtyp} 0x{adres} 0x{rozmiar}\\n” wierszy app i data po adresie (0 → 1)."""
    t = sorted((x for x in tablica if x[1] <= 1), key=lambda x: x[3])
    if not t:
        return 0
    s = "".join("%s %d %d 0x%x 0x%x\n" % x for x in t)
    c = zlib.crc32(s.encode("utf-8")) & 0xFFFFFFFF
    return c or 1


def identyfikator_pliku(sciezka):
    with open(sciezka, encoding="utf-8") as f:
        return identyfikator(tablica_z_csv(f.read()))


def _platformio(env):
    projekt = env.subst("$PROJECT_DIR")
    plik = env.GetProjectOption("board_build.partitions", "")
    sciezka = os.path.normpath(os.path.join(projekt, plik)) if plik else ""
    if not plik or not os.path.isfile(sciezka):
        sys.stderr.write("BŁĄD: uklad_partycji.py: brak pliku tablicy partycji %r (board_build.partitions)\n" % plik)
        env.Exit(1)
    try:
        u = identyfikator_pliku(sciezka)
    except ValueError as e:
        sys.stderr.write("BŁĄD: uklad_partycji.py: %s: %s\n" % (sciezka, e))
        env.Exit(1)
    print("tablica partycji %s: identyfikator układu 0x%08x" % (os.path.relpath(sciezka, projekt), u))
    env.Append(CPPDEFINES=[("UKLAD_PARTYCJI", "0x%08xu" % u)])


if __name__ == "__main__":
    for a in sys.argv[1:]:
        print("%s 0x%08x" % (a, identyfikator_pliku(a)))
else:
    try:
        Import("env")  # noqa: F821 (SCons: skrypt uruchomiony przez PlatformIO)
    except NameError:
        pass  # zwykły import modułu (testy)
    else:
        _platformio(env)  # noqa: F821
