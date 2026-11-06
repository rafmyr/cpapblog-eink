# SPDX-License-Identifier: GPL-3.0-or-later
# Skrypt PlatformIO środowiska e1001_ota: blokada wgrywania przez Wi-Fi
# do czasu przejścia egzemplarza na tablicę partycji 32 MB. Wgranie przez Wi-Fi zapisuje tylko slot aplikacji, więc
# panel zostałby ze starą tablicą (firmware wtedy nie formatuje, nie migruje i wraca do poprzedniej wersji, R7), a
# pierwsze wgranie nowego układu musi iść kablem (`pio run -e e1001 -t upload`) z przejściem z 8.9.
# Samo budowanie (`pio run -e e1001_ota`) działa; przerywa tylko `-t upload` i `-t uploadfs`.
# Zdjęcie blokady (po przejściu egzemplarza): usunąć `blokada_ota.py` z extra_scripts w [env:e1001_ota].
import sys

from SCons.Script import COMMAND_LINE_TARGETS  # noqa: E402 (moduł SCons z PlatformIO)

Import("env")  # noqa: F821 (zmienna SCons)

ZABLOKOWANE = {"upload", "uploadfs", "uploadfsota"}
if ZABLOKOWANE & set(COMMAND_LINE_TARGETS):
    sys.stderr.write(
        "\nBŁĄD: wgrywanie przez Wi-Fi (e1001_ota) jest zablokowane do przejścia egzemplarza na układ 32 MB "
        ".\n"
        "Nowa tablica partycji wgrywa się tylko kablem: zrzut, potem `pio run -e e1001 -t upload` i przywrócenie\n"
        "(tools/prototyp/przejscie_32mb.py, README prototypu „Przejście na układ 32 MB”).\n\n")
    env.Exit(1)  # noqa: F821
