# ADR-0033: Licencja i dwa repozytoria

| | |
|---|---|
| Data | 08.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 08.11.2026: żadnych wydań z gotowym obrazem do opinii prawnika) |
| Źródło | [ADR-0015](0015-przeznaczenie-panelu.md), [ADR-0032](0032-aktualizacja-firmware-z-github.md); [`NOTICE`](../../NOTICE), [`LICENSES/`](../../LICENSES/) |

## Kontekst

- Licencję wybrano w praktyce 06.11.2026 przy publikacji repozytorium, ale decyzja nie miała zapisu.
- Testy panelu odbywają się w zamkniętym gronie znanych osób.
- Status panelu wobec przepisów o wyrobach medycznych (MDR) nie jest rozstrzygnięty. Analiza regulacyjna autora rekomenduje opinię prawnika, zanim projekt zacznie być udostępniany jako gotowy produkt.
- **06.11.2026** repozytorium `github.com/rafmyr/cpapblog-eink` stało się publiczne z pełnym firmware (reguły L, hipotezy H, raport A4), bez plików binarnych firmware. README mówi, że firmware buduje i wgrywa się samemu. Zastrzeżenie w README podawało wtedy status prawny jako fakt („Not a medical device.”), choć ADR-0015 pkt 2 wycofał takie zdanie, i nie podawało przeznaczenia z ADR-0015 pkt 1.
- **ADR-0032** robi z publicznego repozytorium źródło wydań firmware: tag, obraz `.bin`, SHA-256, a panel sam sprawdza nowe wersje.
- Publikacja nastąpiła przed opinią prawnika, a żaden dokument nie zapisywał tej decyzji ani jej kosztu.

## Decyzja

1. **Kod, narzędzia i dokumentacja: `GPL-3.0-or-later`.** Pełny tekst: [`LICENSES/GPL-3.0-or-later.txt`](../../LICENSES/GPL-3.0-or-later.txt) (także jako `LICENSE`). Firmware linkuje GxEPD2 (GPL-3.0), więc obraz firmware jako całość i tak jest na GPL.
2. **Grafiki postaci budzika: własna licencja `LicenseRef-cpapblog-eink-budzik`**, poza GPL, treść w [`LICENSES/LicenseRef-cpapblog-eink-budzik.txt`](../../LICENSES/LicenseRef-cpapblog-eink-budzik.txt): wolno je rozpowszechniać bez zmian jako część firmware z tego repozytorium i jego forków, nie wolno ich używać poza projektem ani modyfikować. Pozostałe części na cudzych licencjach (kroje OFL-1.1, GeoNames CC BY 4.0, generator QR MIT, struktura fontów Adafruit BSD-2-Clause) opisuje [`NOTICE`](../../NOTICE).
3. **Prywatne obrazki dnia (`assets/obrazki-dzienne-prywatne/`) nigdy nie trafiają do repozytorium publicznego** ani do żadnego wydania; katalog jest w `.gitignore`, a każde przeniesienie zmian sprawdza skan.
4. **Dwa repozytoria na stałe:**

   | Repo | Widoczność | Rola |
   |---|---|---|
   | repozytorium robocze autora | prywatne | miejsce pracy: kod, specyfikacja, decyzje, materiały walidacji i dane procesu |
   | `rafmyr/cpapblog-eink` | publiczne od 06.11.2026 | to, co udostępniane: kod, narzędzia, specyfikacja, decyzje po oczyszczeniu; źródło wydań według ADR-0032 |

   Publiczne powstaje wyłącznie z prywatnego (lista dozwolonych plików, czyszczenie, skan); nigdy odwrotnie.
5. **Nazwa.** Publicznie projekt występuje jako `cpapblog-eink`: tak brzmi nazwa repozytorium i tytuł README, a w kodzie, nazwach sieci i plików zostaje wewnętrzny prefiks `cpap-eink`. README nie wprowadza żadnej innej nazwy produktu; nazwa produktu jest nierozstrzygnięta.
6. **Warunek do opinii prawnika:**
   - **żadnych wydań z gotowym obrazem** (`.bin`, scalony plik do wgrania, strona z instalatorem z przeglądarki) w `cpapblog-eink`; tag bez obrazu jest dozwolony;
   - **żadnej zapowiedzi w grupie pacjentów**;
   - **skan jako bramka:** każde przeniesienie zmian do publicznego repozytorium tylko po skanie z wynikiem 0 trafień (sekrety, dane z prawdziwych nocy, prywatne obrazki, adresy);
   - README dalej mówi „zbuduj i wgraj sam”, zastrzeżenie ma brzmienie z ADR-0015 (bez stwierdzania statusu wobec MDR), a przeznaczenie jest podane według ADR-0015 pkt 1 (zrobione 08.11.2026).
7. **Testy w zamkniętym gronie bez zmian:** panel u znanych osób z pełną funkcjonalnością, wgrywany przez autora; publikacja kodu tego nie rozszerza.

## Konsekwencje

- **MDR (interpretacja autora, nie prawnika):** publikacja kodu bez opłat to strefa szara: rozstrzyga prawnik, czy to dostarczanie „w ramach działalności gospodarczej” (art. 2 pkt 27). Gotowe obrazy do wgrania i mechanizm aktualizacji z ADR-0032 silniej przesuwają projekt w stronę „udostępniania na rynku”, bo z kodu robią produkt gotowy do użycia; stąd warunek z pkt 6. Repozytorium ma pełne funkcje zdrowotne (L, H, raport), a nie profil bez nich: to świadoma decyzja właściciela projektu.
- **Koszt:** (a) do opinii prawnika mechanizm z ADR-0032 nie ma czego instalować; panele aktualizuje się kablem albo w oknie serwisowym w sieci lokalnej; (b) kodu z historii publicznego repozytorium nie da się „odpublikować”: jeśli opinia prawnika będzie niekorzystna, zostaje archiwizacja repozytorium albo usunięcie funkcji zdrowotnych w kolejnych commitach, a kopie u innych osób zostają; (c) każde przeniesienie zmian do publicznego repozytorium to osobna praca z czyszczeniem i skanem.
- **ADR-0032:** dostaje dopisek „Zmiana 2026-11-08” (wydania z gotowym obrazem dopiero po opinii prawnika). Podpis obrazu (etap 2) i obraz bez sekretów zostają warunkami pierwszego wydania.
- **`NOTICE`** odsyła do `LICENSES/GPL-3.0-or-later.txt`.
- **Tag `spec-v1.2`** wskazuje kandydata specyfikacji 1.2 sprzed przeglądu zbiorczego, nie wydaną specyfikację.
- **Zmiana tego ADR** wymaga nowego ADR. Pierwsze wydanie z obrazem dodatkowo wymaga opinii prawnika.
