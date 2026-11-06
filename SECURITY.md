# Security

## Reporting a vulnerability

Please do not open a public issue for a security problem. Use GitHub private vulnerability reporting for this repository (tab "Security", "Report a vulnerability"). Describe the problem, the affected version (shown on the hardware test screen) and, if you can, steps to reproduce. You will get an answer as soon as possible; this is a one-person project, so there is no fixed response time.

Supported version: only the latest beta.

## Security model

The panel is meant for one home network. It reads therapy data from an AirBridge module in that network and computes everything locally ([ADR-0001](docs/decisions/0001-przetwarzanie-lokalne.md)). Outside the home network it only asks for time (NTP) and, when city coordinates are configured, for weather and air quality (Open-Meteo). No therapy data is sent anywhere.

## Known limitations of 1.0.0-beta.1

- **Firmware images are not signed.** The ESP32 secure boot and flash encryption are not enabled. Anyone with physical access and a USB cable can read and replace the firmware and the data stored on the panel.
- **Wi-Fi upload (service window):** ArduinoOTA with a password, only in the local network and only for 10 minutes after it is opened from the hardware test screen. The password is currently compiled into the firmware image and the upload tool passes it on the command line. Wi-Fi upload is also locked in `platformio.ini` until a panel has the 32 MB partition layout. The example `secrets.example.h` does not enable Wi-Fi upload. An empty `OTA_PASS` would open the window with an empty password.
- **Local web pages** (setup form, report for the doctor): plain HTTP in the home network, protected by a random 128-bit token in the address, open for 15 minutes, closed after 5 bad requests, only on a private address. Passwords are never sent back to the browser. Known trade-off: five requests without a valid token from any device in the home network close the report or form session. This protects against guessing the token, at the cost that a neighbour device on the same network can close the session.
- **Setup network:** a WPA2 network `cpap-eink-XXXX` with a random password, started only by hand or on the first power-up of a panel with no network saved; at most 2 clients; closes after 15 minutes.
- **AirBridge** is read over plain HTTP with HTTP Digest login, as AirBridge provides it.
- **Weather and time** (Open-Meteo, NTP) are not authenticated, and Open-Meteo requests go over plain HTTP, not TLS. A wrong answer can only change the weather line or the clock, not therapy data.
- **Credentials** for home Wi-Fi and AirBridge are stored in the panel (NVS), not in the files that can be dumped or copied to the card.
- Automatic firmware updates from GitHub releases (HTTPS, SHA-256, later signed images) are planned ([ADR-0032](docs/decisions/0032-aktualizacja-firmware-z-github.md)) and are **not** part of this beta.

## Secrets in the repository

`firmware/prototyp/include/secrets.h` is ignored by git. Only `secrets.example.h` with all values commented out is in the repository. Do not commit real network names, passwords, tokens or addresses, and never commit data downloaded from a panel: it is health data.
