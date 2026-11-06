# Narzędzia firmware (`tools/prototyp`)

Skrypty do budowania, testowania i obsługi panelu z komputera. Pliki danych z panelu (zrzuty, obrazy ekranów) trafiają poza repozytorium; to dane zdrowotne.

| Skrypt | Do czego |
|---|---|
| `testy.sh` | testy rdzenia firmware na komputerze (GNU g++, `-Werror`) i podgląd ekranów w PNG |
| `srodowisko-macos.sh` | przygotowanie GNU GCC, grep, xargs i nproc z Homebrew do `testy.sh` na Macu |
| `sekrety.sh` | tworzy `firmware/prototyp/include/secrets.h` z Pęku kluczy macOS |
| `generuj.py`, `generuj_slot.py`, `sceny_karty.py` | generatory zasobów (kroje, obrazy, sceny budzika) do `firmware/prototyp/include/` |
| `wgraj_wifi.sh` | wgranie firmware przez Wi-Fi (środowisko `e1001_ota`, zablokowane do przejścia na układ 32 MB) |
| `wgraj_pliki.py` | wgranie zrzutu plików do panelu przez port szeregowy (odwrotność `zrzut_plikow.py`) |
| `zrzut_plikow.py` | zrzut plików z pamięci panelu przez port szeregowy |
| `historia.py` | plik importu historii dla panelu z tabeli nocy (`tools/kalibracja`) |
| `podglad.py` | zrzut ekranu z panelu do PNG 800×480 |
| `przejscie_32mb.py` | przejście egzemplarza na układ partycji 32 MB |
| `zrodla.sh` | wspólna lista plików rdzenia i flag kompilacji (używana przez `testy.sh` i `tools/jakosc/sprawdz.sh`) |

Szczegóły i parametry: nagłówki skryptów oraz [`firmware/prototyp/README.md`](../../firmware/prototyp/README.md).
