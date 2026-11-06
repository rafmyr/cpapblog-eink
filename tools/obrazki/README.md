# Obrazki dnia (S-K-38)

Obrazki dnia z ekranu S-K-38 (spec funkcjonalna 5.8.12, spec techniczna 8.6, ADR-0027). Własne obrazki i manifest użytkownika nie trafiają do repozytorium: skrypt odmawia zapisu w repo, a testy używają tylko obrazków syntetycznych.

## Przygotowanie

```
python3 tools/obrazki/przygotuj.py --wejscie <katalog z własnymi obrazkami> --manifest <obrazki.json> --wyjscie <katalog poza repo>
```

- Wynik: pliki PBM `P4` do 800×434 (większe są zmniejszane z zachowaniem stosunku boków, szarość progowana albo z `--rozpraszanie`) i `obrazki.json` tylko z polami, które czyta panel (`plik`, `dni`, `pogoda`, `sezon`, `inne`).
- Nazwy PBM w postaci, którą przyjmuje panel: małe litery, bez polskich znaków, inne znaki niż `[a-z0-9_-]` zamienione na `_`, do 40 znaków (`Żółty Kot.png` → `zolty_kot.pbm`); powtórzona nazwa dostaje `_2`, `_3`.
- Testy: `python3 -m pytest -q tools/obrazki`.

## Wgranie na panel (bez kasowania danych)

Kolejność: najpierw firmware, potem obrazki (np. egzemplarz testowy).

1. Firmware: `cd firmware/prototyp && pio run -e e1001 -t upload` (kablem) albo `tools/prototyp/wgraj_wifi.sh` bez `--fs`. Historia, czynniki i dziennik zostają.
2. Obrazki: `~/.platformio/penv/bin/python tools/prototyp/wgraj_pliki.py /dev/cu.usbserial-110 --obrazki <katalog poza repo>`.
   - Gdy skrypt pisze `pytam panel…` i nic się nie dzieje, naciśnij dowolny przycisk panelu. Panel słucha portu 20 s po naciśnięciu, a każda komenda `?` i `W` przedłuża to o kolejne 20 s.
   - Skrypt wysyła pliki `.pbm` po nazwie, na końcu `obrazki.json`, komendą `W` pod nazwą `obrazki/{plik}`. Panel odpowiada `OK obrazki/{plik}` albo `BLAD obrazki/{plik} {powód}` (`nazwa`, `rozmiar`, `skrot`, `tresc`, `zapis`) i zapisuje plik atomowo do `/obrazki/`.
   - Panel przyjmuje tylko `[a-z0-9_-]{1,40}.pbm` i `obrazki.json`, plik do 64 KB, PBM do 800×434 i manifest czytelny; inne pliki skrypt pomija z wynikiem `nazwa` albo `rozmiar`, dowiązania symboliczne z wynikiem `dowiazanie`.
   - Nazwę portu znajdziesz poleceniem `ls /dev/cu.*` (numer `usbserial` bywa inny na każdym komputerze); zły port kończy się jednym zdaniem z tą radą.
   - Nowy panel po pierwszym starcie ma okno przywracania i stoi do 10 min na ekranie testu. Obrazki wejdą też w oknie; dowolny przycisk panelu kończy je od razu.
   - `brak odpowiedzi`: panel zasnął, skrypt przerywa. Naciśnij przycisk panelu i uruchom skrypt ponownie (pliki już przyjęte nadpisze tą samą treścią).
   - `UWAGA: manifest nie ma pozycji`: panel pokaże tylko pliki z pozycjami; zrób katalog jeszcze raz przez `przygotuj.py`.
3. Obrazek pokaże się w dniu S-K-38 (rotacja R-KON-50).

Nie używaj do tego `pio run -t uploadfs`: obraz pamięci plików kasuje całą historię na panelu.
