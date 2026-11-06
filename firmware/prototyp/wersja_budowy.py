# SPDX-License-Identifier: GPL-3.0-or-later
# Skrypt PlatformIO (extra_scripts = pre:wersja_budowy.py): skrót commitu do identyfikatora wersji firmware.
# Każda kompilacja z innego commitu ma inny identyfikator, więc naprawa wgrana z trybu
# awaryjnego nie jest „tą samą wersją” co zepsuta (src/start.h identyfikatorWersji()). Zmiany niezacommitowane
# w plikach śledzonych dają dopisek `-zm`. Bez gita (np. źródeł): `nieznany`. Działa tak samo w CI i na Macu.
import subprocess

Import("env")  # noqa: F821 (zmienna SCons)


def skrot(katalog):
    try:
        h = subprocess.run(["git", "rev-parse", "--short=7", "HEAD"], cwd=katalog, capture_output=True, text=True,
                           timeout=10)
        if h.returncode != 0 or not h.stdout.strip():
            return "nieznany"
        z = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"], cwd=katalog,
                           capture_output=True, text=True, timeout=10)
        return h.stdout.strip() + ("-zm" if z.returncode == 0 and z.stdout.strip() else "")
    except (OSError, subprocess.SubprocessError):
        return "nieznany"


wersja = skrot(env.subst("$PROJECT_DIR"))  # noqa: F821
print("wersja budowy: %s" % wersja)
env.Append(CPPDEFINES=[("WERSJA_BUDOWY", env.StringifyMacro(wersja))])  # noqa: F821
