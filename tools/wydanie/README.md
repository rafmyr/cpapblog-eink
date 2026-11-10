# tools/wydanie: wydania firmware do aktualizacji z GitHub

Podstawa: [ADR-0032](../../docs/decisions/0032-aktualizacja-firmware-z-github.md), [ADR-0038](../../docs/decisions/0038-wydania-z-prywatnego-repo-i-token.md). Panel sprawdza najnowsze wydanie w repozytorium wydań (parametr budowy `REPO_WYDAN`; na czas testów wewnętrznych osobne prywatne repozytorium, ADR-0038) i pobiera je z tokenem z pamięci panelu. W tym publicznym repozytorium wartość domyślna wskazuje `rafmyr/cpapblog-eink`, ale wydań z gotowym obrazem nie ma w nim do opinii prawnika (ADR-0033 pkt 6).

| Plik | Co robi |
|---|---|
| `wydaj.py` | buduje obraz bez sekretów (`pio run -e e1001_wydanie`, flaga `BEZ_SEKRETOW`), sprawdza, że w obrazie nie ma wartości z lokalnego `include/secrets.h`, że mieści się w slocie 4 MB i ma nagłówek obrazu ESP32, liczy SHA-256, pisze `cpap-eink-{wersja}.bin`, `wydanie.json` i `nota.md` w `build/wydanie-{wersja}/`; z `--publikuj` tworzy wydanie `v{wersja}` (`gh release create`) |
| `certyfikaty.py` | z pakietu certyfikatów Mozilli (certifi) pisze `firmware/prototyp/src/certyfikaty_github.h`: certyfikaty główne serwerów GitHuba wkompilowane w firmware (HTTPS z weryfikacją) |

## Uwaga przed pierwszym wydaniem

Obraz wydania nie ma haseł ani adresów. **Pierwszą wersję z aktualizatorem buduje się z `secrets.h` (`pio run -e e1001`) i wgrywa kablem albo oknem serwisowym**: dopiero ona zapisuje w pamięci panelu hasło okna serwisowego, adres i token źródła otoczenia i współrzędne pogody. Obraz wydania instaluje się tylko na panel, który ma już te wartości; wgrany jako pierwszy (np. oknem serwisowym) wyłączyłby okno serwisowe, źródło otoczenia i pogodę. `wydaj.py` przypomina o tym po każdym wydaniu.

Kontrola obrazu: znacznik `cpap-eink:obraz-bez-sekretow:v1` (jest tylko w obrazie z `-DBEZ_SEKRETOW`), brak wartości przykładowych ze wzoru `secrets.example.h` i, gdy na maszynie jest `include/secrets.h`, brak jego wartości. Bez lokalnego `secrets.h` narzędzie pisze ostrzeżenie, że tego porównania nie zrobiło.

## Użycie

```
python3 tools/wydanie/wydaj.py 1.0.1                  # budowa i pliki, bez wysyłania
python3 tools/wydanie/wydaj.py 1.0.1 --publikuj       # to samo i wydanie na GitHubie (tylko na wyraźne polecenie właściciela)
python3 tools/wydanie/wydaj.py 1.0.1 --obraz plik.bin # z gotowego obrazu zbudowanego z BEZ_SEKRETOW
python3 tools/wydanie/certyfikaty.py                  # odświeżenie certyfikatów (gdy GitHub zmieni wystawcę)
```

Numer wersji: `MAJOR.MINOR.PATCH[-pre]`, wyższy od wersji na panelach (panel instaluje tylko wersję wyższą, R-NF-34). Narzędzie podaje go firmware flagą `-DWERSJA_WYDANIA=…`, a adres repo wydań `-DREPO_WYDAN=…` (parametr budowy, ADR-0038 pkt 5).

## `wydanie.json`

```
{"wersja":"1.0.1","plytka":"reterminal_e1001","uklad":"0x1a2b3c4d","rozmiar":1700000,"sha256":"…","plik":"cpap-eink-1.0.1.bin"}
```

`uklad` to identyfikator tablicy partycji (`firmware/prototyp/uklad_partycji.py`, ten sam algorytm co firmware). Panel instaluje tylko przy zgodnej płytce i układzie, rozmiarze do slotu i skrócie równym skrótowi pobranego pliku (`src/aktualizacja.cpp` `parsujOpis()`, `decyzjaInstalacji()`).

## Token panelu (ADR-0038 pkt 3)

GitHub → Settings → Developer settings → Fine-grained tokens: tylko repozytorium wydań, uprawnienie „Contents: read”, ważność 1 rok, jeden token na panel. Token wpisuje się w formularzu z telefonu (Ustawienia ▶ 2 s, sekcja „Aktualizacje”). Nie trafia do repo, logu ani obrazu.

## Testy

`python3 -m pytest -q tools/test_wydanie.py` (bez sieci i bez PlatformIO). Budowy `pio` i publikacji nie testuje się automatycznie.
