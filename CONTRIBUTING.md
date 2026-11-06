# Contributing

Thank you for your interest. This is a small beta project with one maintainer; issues with clear steps to reproduce are the most helpful contribution. Before larger changes please open an issue first.

## Ground rules

- The panel never changes therapy device settings, never shows setting values or how to change them, and does not diagnose ([ADR-0002](docs/decisions/0002-panel-nie-zmienia-ustawien.md)). Pull requests that cross this line will not be merged.
- All processing stays on the panel ([ADR-0001](docs/decisions/0001-przetwarzanie-lokalne.md)).
- Never attach or commit data from a real panel or therapy device (dumps, screenshots with your nights, logs). Use the synthetic data generators in `tools/`.
- Requirement IDs (`R-…`, `T-…`, screens `S-K-…`, `L-…`) are stable: do not reuse or change the meaning of an ID; withdrawn IDs go to `tools/wycofane.csv`.

## Development setup

- Firmware: C++17, PlatformIO, environment `e1001` in `firmware/prototyp/platformio.ini`. Copy `include/secrets.example.h` to `include/secrets.h` before building; never commit `secrets.h`.
- Tests on a PC need GNU GCC (not clang). On macOS: `brew install gcc grep findutils coreutils`, then `eval "$(bash tools/prototyp/srodowisko-macos.sh)"` (GNU g++, grep, xargs and nproc first on `PATH`; `testy.sh` checks this and says what is missing).
- PlatformIO: `python3 -m pip install platformio`; the first build downloads the platform and toolchain (about a gigabyte), so it needs internet. The example secrets file is enough to build; no panel is needed.
- Python 3 with the packages from `requirements-dev.txt` (`pytest`, `pillow`, `numpy`): `python3 -m pip install -r requirements-dev.txt`.
- Generated files: `firmware/prototyp/include/fonty.h`, `fonty_slot.h`, `obrazy.h` (from `tools/prototyp/generuj.py`) and `firmware/prototyp/src/zestaw_wbudowany.h` (from `tools/zestaw/buduj_zestaw.py`). Change the source and regenerate; do not edit them by hand.

## Before a pull request

```
bash tools/prototyp/testy.sh                  # about 3 minutes
python3 -m pytest -q tools                    # about 4 minutes
python3 tools/spojnosc.py --strict
python3 tools/spojnosc_tech.py
python3 tools/zestaw/buduj_zestaw.py          # after changing thresholds or the rule catalogue
python3 tools/prototyp/generuj.py --tylko-obrazy   # then `git status` must be clean
reuse lint                                    # optional; licences of files without a header are in REUSE.toml
cd firmware/prototyp && pio run -e e1001
```

CI runs the same steps plus sanitizers, clang-tidy and coverage against the baselines in `tools/jakosc/`. A pull request should keep CI green. To see the same result locally run `bash tools/jakosc/sprawdz.sh` (optional; needs bash 4 or newer, g++ 9 or newer, gcov and clang-tidy 18).

## Style

- The code core (`firmware/prototyp/src/`, except the `*_esp32.cpp` adapters and `main.cpp`) has no Arduino dependencies, no exceptions and no RTTI, so it can be tested on a PC.
- New behaviour comes with a test in `firmware/prototyp/test/` that names the requirement it checks.
- Keep the specification in `docs/spec/` and the README files up to date in the same pull request as the code.
- Screen texts are in Polish, short and plain; length limits are checked by `tools/spojnosc.py`.
- Code, identifiers and documentation are in Polish; English is welcome in issues and pull request descriptions.
- License: contributions are accepted under GPL-3.0-or-later (see `LICENSE` and `NOTICE`). New source files in `firmware/prototyp/src`, `firmware/prototyp/include` and `tools` start with `SPDX-License-Identifier: GPL-3.0-or-later` (C++ `//`, Python and shell `#`, after the shebang); the generators add it to the files they write.
