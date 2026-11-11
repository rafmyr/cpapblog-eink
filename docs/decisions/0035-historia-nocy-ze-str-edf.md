# ADR-0035: Historia nocy z pliku STR.edf pobieranego z AirBridge (etap 1 przejścia na EDF)

| | |
|---|---|
| Data | 09.11.2026 |
| Status | przyjęta 09.11.2026 (decyzja właściciela projektu) |
| Źródło | propozycja autora AirBridge („dane z EDF zamiast raportów JSON”); odczyt AirBridge egzemplarza referencyjnego 09.11.2026 (v2.3.0, potem v2.3.1); kod AirBridge `m-kozlowski/airbridge` commit `8232e67` (07.11.2026): `docs/http_api.md`, `src/edf_catalog_json.cpp`, `src/edf_report.cpp` (`HistoricalStr`); [ADR-0017](0017-import-historii-z-karty-aparatu.md), [ADR-0001](0001-przetwarzanie-lokalne.md), [ADR-0022](0022-dane-trwale-nosnik-format-migracje.md) |

## Kontekst

Panel bierze dziś dane nocy z raportów JSON AirBridge (`/api/report?source=edf`, lista `action=days` i raport doby). To ma trzy luki:

- **Historia jest krótka.** AirBridge liczy raport tylko z sesji zapisanych na swojej karcie (na egzemplarzu referencyjnym od kilkunastu dni). Doby starsze zna tylko z podsumowania aparatu (STR), a dla takiej doby raport ma `used_seconds` = `null`, `ahi` = `null` i `events_known` = `false` (kod `edf_report.cpp`), więc panel nie ma z niej ani czasu z maską, ani AHI. Nowy panel startuje z 0 nocami i bez normy przez 14–30 nocy.
- **Import z karty (ADR-0017) jest ręczny**: kopia karty aparatu, skrypt na komputerze, wgranie pliku. Nowa osoba tego nie zrobi.
- **Raport JSON jest liczony przez AirBridge**, a nie przez panel; autor AirBridge zaproponował, żeby aplikacje zewnętrzne czytały pliki EDF.

Sprawdzone 09.11.2026 na AirBridge egzemplarza referencyjnego:

| Co | Wynik |
|---|---|
| `STR.edf` przez `GET /api/storage/download?path=/STR.edf` | ok. 134 KB, ok. 1,5 s, ok. 360 dób, w każdej czas terapii (`Duration`), `AHI`, `AI`, `HI`, przeciek, ciśnienia docelowe, oddechy, ustawienia (`S.*`) |
| `GET /api/sessions` (v2.3.1, wydany 07.11.2026) | lista sesji z `closed`, ścieżkami i rozmiarami plików oraz blok `str` z `size`, `modified` i `etag` (CRC32 całego pliku). Na v2.3.0: 404. Zaraz po restarcie AirBridge: 503 (`catalog_unavailable`), po kilkunastu sekundach 200 |
| `Range` przy `/api/storage/download` | nieobsługiwany (`Accept-Ranges: none`, zawsze cały plik) |
| pobieranie w trakcie terapii | zablokowane przez AirBridge (`docs/http_api.md`, „active therapy blocks … downloads”) |
| AirCurve 10 CS PaceWave (egzemplarz referencyjny) | `OAI`, `CAI`, `CSR` i ustawienia EPR w STR puste (`0xFFFF`); `UAI`, `HI`, `MaskEvents` wypełnione |

## Decyzja

Przejście na EDF w dwóch etapach. Ten ADR przyjmuje **etap 1**; etap 2 (noc z plików sesji `PLD`, `EVE`, `SAD`, `CSL` zamiast raportu JSON, `closed` z `/api/sessions`) dostanie własny ADR.

1. **Źródło.** Przy AirBridge od v2.3.1 panel w wybudzeniu, w którym pobiera listę nocy (nie w slotach tylko ze statusem, R-KON-01, R-KON-42), czyta `GET /api/sessions?order=desc&limit=1` i porównuje `str.etag` z zapamiętanym. Tylko gdy etag jest inny (albo nieznany), pobiera `GET /api/storage/download?path=/STR.edf` i liczy wszystko lokalnie (ADR-0001 bez zmian). Najwyżej jedno pobranie STR na wybudzenie. Etag zapisuje dopiero po udanym przetworzeniu i zapisie historii.
2. **Bez v2.3.1 panel działa jak dotąd.** 404 na `/api/sessions` = funkcja niedostępna (log, bez kodu błędu na ekranie, bez E-03). 503 albo przerwane pobranie = ponowienie w następnym wybudzeniu z listą nocy. Zakres obsługiwanych wersji AirBridge (≥ 2.2.0, Załącznik A pkt 4) bez zmian.
3. **Noc ze STR (nowe źródło `S`).** Każda doba STR z `Duration` > 0 daje rekord nocy z datą poranka = `Date` + 1 (4.1, doba „od południa do południa”). Pola:
   - czas z maską M-01 = `Duration` (minuty); AHI = `AHI`; M-03: OA/h = `OAI`, CA/h = `CAI`, H/h = `HI`, UA/h = `UAI` (gdy w STR jest wartość; `0xFFFF` = brak);
   - kanały jak w raporcie AirBridge dla doby ze STR (`HistoricalStr::statistic`): `Press` ← `TgtIPAP`, `EprPress` ← `TgtEPAP`, pozostałe kanały ← etykieta kanału (`Leak.50`, `Leak.95`, `RespRate.50`, …), z tym samym przeliczeniem jednostek (`L/s` × 60, `L` × 1000). Panel stosuje do nich **te same reguły co do kanałów z `source = "str"`**: kanały tylko z EDF (`Press`, R-NOR-08) pomija, EPAP bierze z flagą „urządzenie” (`epapStr`, poza normą i regułami), pozostałe bierze;
   - zdjęcia maski M-17: brak danych. `MaskEvents` / 2 zgadza się z liczbą zamkniętych sesji z `/api/sessions` w 9 z 10 dób egzemplarza referencyjnego, czyli poniżej przyjętego progu 95%. Które liczenie jest właściwe, to pytanie otwarte;
   - ustawienia i tryb nocy z pól `S.*` i `Mode` tak jak `Str.ustawienia()` w `tools/kalibracja/noce.py` (ten sam układ co `settings.fields[]` raportu);
   - M-10 z `FlowLim.50`/`.95`, jeśli STR je ma, jak w raporcie JSON dla doby ze STR;
   - brak danych: M-08, M-09 (CSR, jednostka w STR niesprawdzona), M-18, wsparcie PS;
   - ważność nocy: ta sama reguła co dla nocy z AirBridge (≥ 4 h i znane AHI), bez `partial`.
4. **Pierwszeństwo:** pełna noc z AirBridge > noc z karty (`K`) > noc ze STR (`S`). Noc `S` wchodzi tylko na datę bez rekordu albo na miejsce rekordu z AirBridge, który nie jest pełny i nie ma czasu z maską (doba ze STR z raportu JSON). Nigdy nie nadpisuje `K` ani pełnej nocy z AirBridge; każda z nich nadpisuje `S`.
5. **Norma i reguły:** noce `S` liczą się do normy, okien reguł i serii jak noce z AirBridge; pola bez danych są pominięte tak jak dziś (NAN). Nie liczą się do okresu testowego WY16 ani do porównania WY9 (jak noce z karty w ADR-0017).
6. **Format danych:** nowa litera źródła `S` w pliku nocy. Jeśli starszy firmware odrzucałby takie wiersze, zmiana wersji formatu z migracją według ADR-0022.

## Konsekwencje

- Nowy panel ma po pierwszym porannym pobraniu tyle historii, ile pamięta aparat (na egzemplarzu referencyjnym ok. 360 dób), bez kopii karty i bez komputera. Norma czasu z maską, AHI i przecieku od pierwszego dnia.
- Doby, które AirBridge zna tylko ze STR (bez czasu i AHI), dostają czas z maską i AHI.
- Koszt na panelu: ok. 134 KB i ok. 1,5 s przy każdej zmianie STR (raz na dobę po terapii, też po drzemce); pamięć na czas przetwarzania w PSRAM. `Range` nie jest potrzebny.
- **Ograniczenie (do decyzji, nie zrobione w etapie 1):** noce `S` nie mają ciśnienia wdechowego (M-05) ani wsparcia PS, bo STR podaje tylko ciśnienie docelowe aparatu, a R-NOR-08 nie pozwala go mieszać z ciśnieniem z EDF. Normy ciśnienia budują się dalej z nocy z AirBridge i z karty. Zmiana tej reguły to osobny ADR.
- Import z karty (ADR-0017) zostaje bez zmian; dla nowych paneli przestaje być potrzebny.
- Wymaga AirBridge v2.3.1 u każdego użytkownika, który ma dostać historię ze STR (aktualizacja AirBridge przed wgraniem nowej wersji panelu).
- Dokumenty zmienione razem: spec funkcjonalna (R-DANE, Załącznik A), spec techniczna (T-INT-01, komponenty, pliki), ADR-0017 (adnotacja), `firmware/prototyp/README.md`, `README.md` („Stan”), `CHANGELOG.md`.

## Zmiana 2026-11-09 (implementacja etapu 1; bez zmiany decyzji)

- **Wymagania:** R-DANE-38 (pobranie STR z etagiem), R-DANE-39 (fail-path), R-DANE-40 (doba STR → noc `S`), R-DANE-41 (pierwszeństwo, norma i reguły); Załącznik A pkt 9.
- **Pkt 3, M-10:** tekst pkt 3 poprawiony 09.11.2026 po przeglądzie: M-10 z `FlowLim` ze STR jak w raporcie JSON dla doby ze STR (wspólna reguła kanałów `src/airbridge.cpp` `kanalyRaportu()`).
- **Plan pobrań:** starsza doba z nocą `S`, dla której lista `action=days` nie podaje sesji (`session_count` 0 albo brak), nie jest pobierana drugi raz (raport byłby niepełny); doba z sesjami jest pobierana i pełny raport z EDF zastępuje noc `S` (`src/airbridge.cpp` `planPobrania()`).
- **Odczyt:** zakres cyfrowy sygnału poza [-32768, 65535] = błąd pliku (próbka ze znakiem tylko przy cyfrowym minimum < 0, jak `tools/kalibracja/noce.py`; STR AirCurve ma sygnały bez znaku 0…65534, np. `PatientHours`); doba z `Date` spoza 1990–2099 pominięta.
- **Pkt 4, częściowa noc z AirBridge:** rekord „częściowa” z czasem z maską zostaje (noc `S` wchodzi tylko w miejsce rekordu bez czasu z maską), a noc „częściowa” i niepełna doba z AirBridge nie wypierają nocy `S` (`src/airbridge.cpp` `wpiszNocAB()`, `src/str.cpp` `wpiszNocS()`).
- **Pkt 6, format:** prototyp do wersji 3 odrzucał wiersz z `S` w polu `zrodlo` (zachowywał go dosłownie jako nieczytelny, a dla tej daty pobierał dobę z AirBridge, więc plik dostawał dwa wiersze jednej daty). Dlatego plik `noce` ma wersję 4: te same pola co wersja 3, nowa jest tylko wartość `S`; migracja 3 → 4 zmienia sam nagłówek (kopia `.v3`), a firmware znający wersję 3 czyta plik wersji 4 tylko do odczytu (ADR-0022 pkt 4).
- **Etag:** w NVS `cpap`, klucz `str_etag` jako `{etag}|{liczba nocy S w historii}` (nowy klucz z wartością domyślną „brak”, bez migracji), zapisywany po udanym zapisie plików nocy; ten sam etag przy historii bez nocy `S`, choć plik je dawał (zgubione pliki nocy), = etag nieznany, plik pobierany od nowa.

## Zmiana 2026-11-09 (ADR-0036, noc z plików sesji; bez zmiany decyzji)

Przy `noc_zrodlo=plik` (ADR-0036 pkt 5) panel pobiera STR.edf w wybudzeniu z nocą z plików także przy niezmienionym etagu, bo z doby STR bierze ustawienia nocy (`Przebieg::strDoNocy()`); pkt 1 („najwyżej jedno pobranie STR na wybudzenie”) zostaje: `strZAirBridge()` używa wtedy tego samego pliku bez drugiego pobrania. Bez `noc_zrodlo=plik` bez zmian.

## Zmiana 2026-11-09 (ADR-0036 „Zmiana 2026-11-09”: tryb porównawczy domyślny)

Domyślnym źródłem nocy jest teraz `noc_zrodlo=porownanie` (R-DANE-60): noc z raportu JSON, a na końcu wybudzenia porównanie z nocą z plików, które też bierze ustawienia z doby STR (`Przebieg::strDoNocy()`). STR.edf jest więc pobierany niezależnie od źródła nocy: przy zmianie etagu przez historię ze STR (pkt 1, jak dotąd), a przy `plik` i w porównaniu także przy niezmienionym etagu, gdy w tym wybudzeniu nie był jeszcze pobrany (najwyżej jedno pobranie na wybudzenie zostaje). Zdanie wyżej „Bez `noc_zrodlo=plik` bez zmian” jest nieaktualne: bez zmian jest tylko `noc_zrodlo=json`. Noce 'S' z pliku pobranego do porównania wpisują się według tych samych zasad (R-DANE-41, pierwszeństwo bez zmian).

## Zmiana 2026-11-11 (ADR-0036 „Zmiana 2026-11-11”: domyślnie `plik`; bez zmiany decyzji)

Domyślnym źródłem nocy jest od 11.11.2026 `plik`: STR.edf pobierany w wybudzeniu z nocą z plików także przy niezmienionym etagu, najwyżej raz (jak wyżej przy `plik`). Tryb porównawczy tylko jawnie (`noc_zrodlo=porownanie`); bez dodatkowych pobrań STR zostaje tylko `noc_zrodlo=json`.
