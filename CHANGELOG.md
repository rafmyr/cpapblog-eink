# Changelog

The history of versions before this repository is not published. The specification in this repository is version 1.2 ([docs/spec/](docs/spec/README.md)); the first tag is `spec-v1.2` together with `v1.0.0-beta.1`.

## Unreleased

### Added

- Fast button presses (R-PRZ-20): presses made while the screen is being drawn are no longer lost. A 5 ms sampling task debounces the buttons (15 ms stable), beeps at the moment of the press and queues it; navigation presses (short presses on the day screen and on cockpit pages `n / 6`) are replayed with a single refresh at the end. Questions, confirmations and long presses stop the queue, so nothing is answered on a screen the user has not seen.
- Hidden shortcut ◀ + ▶ held together for 2 s (R-PRZ-21): opens the hardware test U-05 with the Wi-Fi update window from any screen, also from sleep.
- Line `Noce mało czytelne: bezdechów nie oceniam.` (R-SKI-24): when 4 of the last 7 nights with therapy are not valid for the L patterns, the panel says so in the attention line (warning, before the "What was different?" invitation). The threshold was already in the rule set, the line was not implemented.
- Replacement step `→ Bezdechów nie oceniam: noce z wyciekiem.` (`S-K-18.krok_dane_wyciek`, spec 5.4.13 item 7) on the weekly card S-K-18 and on O-03 when assessments are missing because of leak (uncertain nights), not because of too few nights.
- PlatformIO environment `e1001_dev`: the only image that sends the screen buffer to the serial port for `tools/prototyp/podglad.py` (`-DZRZUT_EKRANU=1`).
- Simulator scenarios `nowy-ahi12`, `nowy-wyciek18`, `nowy-ahi7` (new user without history, AutoSet, 30 nights) and `tools/test_symulator_nowy_pacjent.py`, which checks the day screen and attention line of every morning.
- Decisions [ADR-0033](docs/decisions/0033-licencja-i-dwa-repozytoria.md) (license and two repositories; no releases with a ready-to-flash image until a legal opinion on the medical device status) and [ADR-0034](docs/decisions/0034-model-pasm-wycieku-p95.md) (leak P95 bands 10 / 15 / 24 / 27 L/min).
- README: "Intended use" section (ADR-0015 item 1).

### Changed

- Day slot hourly check (R-KON-42): the footer readings (outside temperature from the hourly forecast or the local source, bedside temperature) are taken from the last displayed frame, so a forecast change of one degree no longer refreshes the e-ink every hour.
- The button that wakes the panel beeps as the first action of the program.
- L-09 "Wyciek nie ustępuje" (spec 5.12.4, ADR-0034): counts nights with leak P95 ≥ 15 L/min (the uncertain-night threshold `R-NOR-05.niepewna_p95`) in ≥ 10 of the last 14 nights with therapy, instead of P95 > 24 L/min. A steady leak of 15–24 L/min, which makes every night uncertain and silences L-03, now leads to the equipment supplier instead of months of "nie oceniam". Exit: 7 consecutive nights with therapy without E-20 and with P95 below 15 L/min. A night without P95 counts as "not met" for entry and does not break the exit series (new rule-engine parameter `brak`). The L-09 chart in the doctor's report has its dashed line at 15 L/min.
- E-30 in the day-screen state (`src/stany.cpp`) uses the same definition as the rule engine: median AHI of the last 7 valid nights without uncertain nights ≥ 5 / h (was "≥ 3 of 7").
- Weekly card S-K-18: title `{n} z 7 nocy ponad 4 h` (was `Twój tydzień: {n} z 7 nocy`, which repeated the label), reason `Noc z maską: średnio {czas}.`, without nights with therapy `W tym tygodniu bez nocy z maską.`; the bedtime step reads `→ Stała pora snu, też w weekend.` and appears only when fewer than 7 of 7 nights exceeded 4 h; with fewer than 5 of 7 nights over 4 h [PRÓG] the step is `→ Każda godzina z maską się liczy.` instead of `→ Nic do zrobienia. Tak trzymaj.`.
- EDF package: the phone page shows `Paczka EDF: niedostępna w tej wersji.` instead of a link, `/edf` answers 501 with the same sentence (was 503 `AirBridge nie odpowiada.`), zone 8 of the A4 report ends with `Pełne przebiegi nocy: karta SD aparatu.`, and the cheat-sheet variant (`raport=sciaga`) draws no QR code for the package and starts neither Wi-Fi nor the server. Switch: `EDF_DOSTEPNA` in `src/dyrygent.cpp`.
- Wi-Fi update password: `OTA_PASS` shorter than 16 characters (also empty) stops compilation (`static_assert` in `src/main.cpp`), and the adapter does not open the update window with such a password (log line `OTA: okno wgrywania wyłączone: słabe hasło …`). `tools/prototyp/sekrety.sh` generates 32 characters.
- The regular images `e1001` and `e1001_ota` no longer send the screen buffer to the serial port: a button wake-up is about 1 s shorter and screen content does not leave the panel over the cable.
- Quality gates: clang-tidy baseline reduced from 20 to 2 warnings (`sscanf` replaced by `strtoull` with end and range checks, `reserve()` in vector loops, string comparison); `tools/jakosc/porownaj_bazy.py` compares gate baselines with a base branch (`--galaz`, `BAZA_GALAZ`, default `origin/main`) and also accepts a `zmiana bazy:` line added to `CHANGELOG.md` in the branch.
- README: the disclaimer no longer states the legal status as a fact; it says the status under medical device regulations has not been determined and that until then there are no ready-to-flash images and no certification. Code on `main` includes these changes; no new release, no new tag.
- Specification and ADR texts updated for the above (spec 4.9, 5.4.13, 5.8.4, 5.12.4, 5.12.5, 5.13.4; technical spec 2, 3.1, 6, 7, 8, 9, 11, 12.5, 13.1, 14; ADR-0007, 0013, 0023, 0027, 0029, 0030, 0032; results of measurement 12 (clock drift) and measurement 16 (memory above 16 MB) entered).

### Fixed

- Panel with a few months of night history crashed on start (`Stack canary watchpoint triggered (loopTask)` while drawing the day screen S8) and entered the safe mode U-06 after three failed starts. The loop task stack is now 48 KB (was 24 KB).
- Internal RAM ran out with a long history: rule-engine state and night records (mostly small map and set nodes) were allocated in internal RAM although the board has 8 MB PSRAM. Ordinary allocations from 8 bytes now go to PSRAM (`heap_caps_malloc_extmem_enable(8)`); the minimum free internal RAM went from 452 B to 162 KB. The boot log has a new line `pamiec start: PSRAM …`.
- `tools/prototyp/wgraj_pliki.py`: after `--reset` the first file could arrive truncated, because the panel does not read the serial port while drawing. The tool now waits until the screen is drawn and keeps the panel awake with `?` meanwhile.
- Calendar dates: `dataOk()` checks the number of days in the month and leap years, so `2026-02-31` is no longer accepted silently as 3 March. After the update a date outside the calendar saved earlier by the form (for example `wymiana.maska=2026-02-31` in `/konfiguracja.txt`) is skipped with a log line and has to be entered again. Night records are not affected.
- The screen number restored from RTC memory is range-checked (`ekranZRtc()`); an unknown value opens the day screen.
- Atomic file write on the panel compares the size of the `.tmp` file on flash with the expected size before renaming it.

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
