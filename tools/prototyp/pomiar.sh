#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Odczyt pomiaru czasu zmiany ekranu z panelu z obrazem `e1001_pomiar` (firmware/prototyp/README.md,
# „Pomiar czasu zmiany ekranu”). Zapisuje cały log portu szeregowego z godziną każdego wiersza do pliku poza repo
# i pokazuje na bieżąco wiersze `POMIAR` oraz start procesora (`ESP-ROM`, `rst:`), czyli chwilę wybudzenia przyciskiem.
# Czyta port wprost (pyserial z PlatformIO), działa też bez terminala; gdy port zniknie, czeka i otwiera go ponownie
# (wiersze `# port otwarty`, `# port zniknął` w logu).
# Użycie: tools/prototyp/pomiar.sh [port] [plik_logu]     koniec: Ctrl+C
set -euo pipefail
PORT="${1:-/dev/cu.usbserial-110}"
LOG="${2:-$HOME/cpap-pomiar-$(date +%Y%m%d-%H%M%S).log}"
PY="${PY:-$HOME/.platformio/penv/bin/python}"
echo "port: $PORT, log: $LOG (Ctrl+C kończy)"
exec "$PY" -u - "$PORT" "$LOG" <<'PYEOF'
import sys, time, datetime, serial
port, log = sys.argv[1], sys.argv[2]
def teraz():
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
with open(log, "a", encoding="utf-8") as f:
    def zapisz(tekst, pokaz=False):
        w = f"{teraz()} {tekst}"
        f.write(w + "\n")
        f.flush()
        if pokaz:
            print(w, flush=True)
    while True:  # port znika, gdy mostek USB panelu się rozłącza: czekamy i otwieramy ponownie
        try:
            s = serial.Serial(port, 115200, timeout=1)
        except (serial.SerialException, OSError):
            time.sleep(0.05)
            continue
        zapisz("# port otwarty", True)
        try:
            while True:
                w = s.readline()
                if not w:
                    continue
                tekst = w.decode("utf-8", "replace").rstrip("\r\n")
                zapisz(tekst, any(k in tekst for k in ("POMIAR", "ESP-ROM", "rst:")))
        except (serial.SerialException, OSError) as e:
            zapisz(f"# port zniknął: {e}", True)
            try:
                s.close()
            except Exception:
                pass
PYEOF
