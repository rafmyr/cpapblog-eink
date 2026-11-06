# Changelog

The history of versions before this repository is not published. The specification in this repository is version 1.2 ([docs/spec/](docs/spec/README.md)); the first tag is `spec-v1.2` together with `v1.0.0-beta.1`.

## Unreleased

### Added

- Fast button presses (R-PRZ-20): presses made while the screen is being drawn are no longer lost. A 5 ms sampling task debounces the buttons (15 ms stable), beeps at the moment of the press and queues it; navigation presses (short presses on the day screen and on cockpit pages `n / 6`) are replayed with a single refresh at the end. Questions, confirmations and long presses stop the queue, so nothing is answered on a screen the user has not seen.
- Hidden shortcut ◀ + ▶ held together for 2 s (R-PRZ-21): opens the hardware test U-05 with the Wi-Fi update window from any screen, also from sleep.

### Changed

- Day slot hourly check (R-KON-42): the footer readings (outside temperature from the hourly forecast or the local source, bedside temperature) are taken from the last displayed frame, so a forecast change of one degree no longer refreshes the e-ink every hour.
- The button that wakes the panel beeps as the first action of the program.

### Fixed

- Panel with a few months of night history crashed on start (`Stack canary watchpoint triggered (loopTask)` while drawing the day screen S8) and entered the safe mode U-06 after three failed starts. The loop task stack is now 48 KB (was 24 KB).
- Internal RAM ran out with a long history: rule-engine state and night records (mostly small map and set nodes) were allocated in internal RAM although the board has 8 MB PSRAM. Ordinary allocations from 8 bytes now go to PSRAM (`heap_caps_malloc_extmem_enable(8)`); the minimum free internal RAM went from 452 B to 162 KB. The boot log has a new line `pamiec start: PSRAM …`.
- `tools/prototyp/wgraj_pliki.py`: after `--reset` the first file could arrive truncated, because the panel does not read the serial port while drawing. The tool now waits until the screen is drawn and keeps the panel awake with `?` meanwhile.

## 1.0.0-beta.1 (2026-11-06)

First public version. Beta for testing on a small number of panels; not a release for the general public.

### Features

- Morning day screen: the last night (time with mask, AHI, leak) against the personal norm of the last 30 valid nights; rotation of calm screens, streaks and milestones; "uncertain night" when the leak is too high to judge the night.
- Cockpit of six pages (`1 / 6` … `6 / 6`): 30-night trends with the norm, last night details, open matters, equipment, settings.
- Therapy modes `CPAP`, `AutoSet`, `ASV` and `ASVAuto` (AirSense 10 and AirCurve 10 through AirBridge v2.2.0 or newer); in ASV modes EPAP and pressure support instead of pressure.
- Deviation states per metric (enter after 3 of 5 nights, leave after 3 nights back in range) and context screens for leak, mask removals, short nights, bedroom conditions.
- Doctor patterns: a screen that says whether and how soon to talk to a doctor, with "I know" and "Discussed with the doctor" confirmations; other patterns are only collected for the report.
- A4 report for the doctor as PDF, downloadable with a QR code from the panel's local server for 15 minutes, or a cheat-sheet screen.
- Morning question "What was different yesterday?" after an unusual night and on a random control night; answers exclude nights with infection or sleeping away from home from the norm.
- Night screen "Dobranoc" from 23:30 to 05:00 and a day slot: airing window, tomorrow morning, equipment, sunrise, optional daily picture from the card.
- Mask and filter replacement reminders.
- Optional local environment source (outdoor weather, bedroom CO2) and optional weather and air quality from Open-Meteo for a chosen city.
- Configuration from a phone: the panel's own WPA2 setup network with QR codes, a form for home Wi-Fi and AirBridge, a settings form later from the settings page.
- Battery level on the test screen, a warning line at 15% and a "charge the screen" screen at 5%.
- Hardware test screen with a Wi-Fi service window for firmware upload in the local network.
- Day simulator and screen previews on a PC.

### Known limitations

- Only `ASVAuto` on an AirCurve 10 CS has been checked on a real device. The modes `CPAP`, `AutoSet` and `ASV` were checked on synthetic data only ([ADR-0030](docs/decisions/0030-wersja-1-dla-asv-cpap-i-autoset.md)); data fields of AirSense 10 in AirBridge are known from the AirBridge code, not from a device.
- Thresholds marked `[PRÓG]` are not calibrated yet.
- Uploading over Wi-Fi is locked until a panel is moved by cable to the 32 MB partition layout.
- Firmware images are not signed; see [SECURITY.md](SECURITY.md).
- An image built from the example secrets file can replace network settings on a panel whose settings came from `secrets.h`; release images need empty secrets.
- A Qingping sensor is not read directly; bedroom CO2 comes only from the local environment source.

### License and internal changes

- License: GNU GPL v3.0 or later (the firmware links GxEPD2, GPL-3.0); SPDX headers in source files and generated headers; `NOTICE` lists third-party components with versions and licenses and the exception for the alarm clock artwork (all rights reserved).
- The example secrets file has every line commented out: an image built from it starts in first configuration and Wi-Fi upload is disabled. An empty `OTA_PASS` is documented as unsafe.
- Library versions are pinned in `platformio.ini` (GxEPD2 1.6.9, Adafruit GFX 1.13.0, Adafruit BusIO 1.17.4).
- `requirements-dev.txt` lists the Python packages for tests and generators.
- CI: actions pinned by commit SHA, minimal permissions, and a check that regenerated files (rule set, alarm clock images) match the repository.
- REUSE 3.3: licence texts in `LICENSES/`, licences of files without a header in `REUSE.toml`; `NOTICE` lists exact copyright notes of fonts and the QR code generator.
- `testy.sh` checks the environment (GNU g++, grep, xargs, nproc) and says what is missing; `tools/prototyp/srodowisko-macos.sh` prepares it on macOS.
- Test files and test functions are named by topic (`testy_rekord_nocy.cpp`, `testy_silnik_regul_brzegowe.cpp`, `test_symulator_brzegowe.py` …); comments and test messages describe what is checked and why.
- `tools/spojnosc_tech.py`: every `[do zmierzenia]` marker needs a measurement reference `pomiar N` from the measurement plan (table "Pomiary" in 2.4); a test without a component does not warn in "Wymagania bez komponentu".
- `tools/jakosc/porownaj_bazy.py`: a worse quality baseline is accepted with a `zmiana bazy: <file>: reason` line in a commit message.
- `tools/zestaw/buduj_zestaw.py`: rebuilding the embedded rule set without source changes keeps the `zrodlo` field, so `zestaw_wbudowany.h` is reproducible.
