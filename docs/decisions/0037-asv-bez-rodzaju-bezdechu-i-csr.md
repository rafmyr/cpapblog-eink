# ADR-0037: Aparaty ResMed ASV nie zapisują rodzaju bezdechu ani CSR: reguły na stanie aparatu przy bezdechu (etap 3)

| | |
|---|---|
| Data | 09.11.2026 |
| Status | przyjęta 09.11.2026 jako zmiana spec (decyzja właściciela projektu); kod po etapie 2, w toku |
| Źródło | analiza 09.11.2026 na kopii karty aparatu egzemplarza referencyjnego (AirCurve 10 CS PaceWave), plikach AirBridge v2.3.1 i 19 aparatach ResMed z lokalnego katalogu OSCAR (zliczenia zbiorcze, bez danych osobowych, nic nie zapisane); fora użytkowników ResMed ASV (CPAPtalk); zmienia Załącznik A, [ADR-0017](0017-import-historii-z-karty-aparatu.md) (adnotacja 31.10.2026), [ADR-0029](0029-zakres-skierowan-w-wydaniu-1-0.md) pkt 2 (L-01), [ADR-0030](0030-wersja-1-dla-asv-cpap-i-autoset.md) |

## Kontekst

Specyfikacja zakładała (Załącznik A), że AirBridge przenosi z aparatu rodzaj bezdechu (obturacyjny, centralny, nieokreślony) i oddychanie okresowe (CSR) także w trybach ASV. Sprawdzenie 09.11.2026:

| Dowód | AirSense (AutoSet, CPAP, APAP), AirCurve VAuto/S | AirCurve ASV / ASVAuto |
|---|---|---|
| plik zdarzeń EVE z karty aparatu | `Obstructive Apnea`, `Central Apnea`, `Apnea` (nieokreślony, zwykle 0,1–7% bezdechów) | wyłącznie `Apnea`: egzemplarz referencyjny i drugi aparat ASV, żaden bezdech z rodzajem |
| podsumowanie dzienne aparatu (STR): `OAI`, `CAI` | wypełnione w 100% dni (17 aparatów, 2 446 dni) | **puste we wszystkich dniach** (oba aparaty) |
| podsumowanie dzienne STR: `CSR` | wypełnione w AutoSet/CPAP/APAP/S (VAuto: puste) | **puste w 100% dni** |
| pliki AirBridge v2.3.1 (10 nocy) | — | tylko `Apnea`, 0 z rodzajem, 0 CSR |
| stary AirBridge v1.6.x | — | `Obstructive Apnea` w nocach, w których aparat zapisał 0–1 bezdech: dane błędne, nie używać |
| społeczność OSCAR/ResScan | — | ResMed ASV nie wykonuje testu drożności (FOT) i oznacza wszystkie bezdechy jako nieokreślone (UA) |

Skutek: na ResMed ASV reguły oparte na OA/CA i CSR nigdy nie dostają danych: L-01, L-02, L-04 (składnik OA) z H-02, E-33, O-12 (M-03ca), komórki K2 i części raportu. Właściciel projektu (04.11.2026): nie ograniczamy funkcjonalności bez zgody; 09.11.2026: zaadresować w specyfikacji tak, żeby funkcja była zachowana.

Co jest dostępne: plik PLD (etap 2) ma co 2 s `Press` (IPAP), `EprPress` (EPAP), `MinVent` (wentylacja minutowa) i `TgtVent` (wentylacja docelowa, którą wyznacza ASV); EVE ma czas i długość każdego bezdechu; ustawienia (STR) mają granice EPAP i PS.

## Decyzja

1. **Zasada:** w trybach `ASV` i `ASVAuto` panel nie twierdzi, że zna rodzaj bezdechu ani CSR. OA, CA, CA/h i M-09 są „aparat nie rozróżnia” (nie 0, nie „brak danych z błędu”), we wszystkich nocach tych trybów, z każdego źródła (pliki, JSON, karta, STR). Zamiast rodzaju bezdechu panel opisuje **stan aparatu w chwili bezdechu**. To opis pracy urządzenia, nie rozpoznanie (ADR-0002 bez zmian: bez wartości ustawień, bez drogi zmiany, krok „omów z lekarzem”).
2. **Nowe metryki nocy** (liczone z EVE i PLD tej samej sesji; tylko noce z plików i z karty, nie z raportu JSON):
   - bezdechy na godzinę (wszystkie, UA + OA + CA);
   - **bezdechy przy pełnym wsparciu**: bezdech, w którego oknie [początek, koniec + 10 s `[PRÓG]`] PS (`Press` − `EprPress`) osiągnął maksimum PS z ustawień − 0,5 cmH2O `[PRÓG]` (ASVAuto: `AXS`; ASV: `MXS`); na godzinę i jako odsetek bezdechów;
   - **bezdechy przy maksymalnym EPAP** (tylko ASVAuto): w tym samym oknie EPAP ≥ `EAX` − 0,5 cmH2O `[PRÓG]`; na godzinę i odsetek;
   - **czas poniżej celu wentylacji**: minuty, w których `MinVent` < 90% `TgtVent` `[PRÓG]` (kontekst w raporcie; bez reguły w etapie 3).
3. **Reguły w trybach ASV i ASVAuto** (AutoSet i CPAP bez zmian):
   - **L-01** wariant ASV „Bezdechy mimo pełnego wsparcia”: średnia bezdechów/h z 7 nocy ważnych dla L ≥ 5 `[PRÓG]`, ≥ 5/h w ≥ 3 z 7 nocy i odsetek bezdechów przy pełnym wsparciu ≥ 50% `[PRÓG]`; wyjście jak L-01 (średnia < 3/h przez 7 nocy);
   - **L-04 / H-02** w ASVAuto: składnik „mediana OA/h ≥ 5” zastąpiony „mediana bezdechów przy maksymalnym EPAP / h ≥ 5” `[PRÓG]`; warunek EPAP przy `EAX` bez zmian; okno L-04 bez wymogu „noce z rodzajem bezdechu” (R-SKI-02 zmienione);
   - **E-33** wariant ASV: stan odchylenia „bezdechy przy pełnym wsparciu / h” wobec normy i mediana z 5 nocy ≥ 3/h `[PRÓG]`;
   - **L-02**: w ASV i ASVAuto linia „aparat nie oznacza oddychania okresowego”, bez oceny (ADR-0029 pkt 7: „nie oceniam” ≠ „w porządku”); wykrywanie z falowania `MinVent`/`TgtVent` to osobne pytanie otwarte;
   - **O-12**: w ASV i ASVAuto metryka bezdechów/h zamiast CA/h;
   - wszystkie powyższe w ASV/ASVAuto: stan „tylko raport” (ADR-0029), bez pilności, do przeglądu lekarza PSG i konsultacji klinicznej.
4. **K2 i raport w ASV/ASVAuto:** zamiast OA, CA i udziału CA: „rodzaj bezdechu: aparat ASV nie rozróżnia” oraz podział bezdechów: przy pełnym wsparciu / przy maksymalnym EPAP / pozostałe; w raporcie także czas poniżej celu wentylacji. *(Dopisek 09.11.2026 po przeglądzie: to nie jest podział, tylko osobne wskaźniki, bo jeden bezdech może spełniać oba warunki; drukowane zdanie mówi to wprost: „nie sumują się”.)*
5. **Tryby z rodzajem bezdechu (AutoSet, CPAP i inne):** OA i CA z aparatu; bezdechy nieokreślone osobno (UA); OA i CA „nie wiem” dopiero, gdy bezdechów nieokreślonych ≥ 1/h `[PRÓG]` *(dopisek 09.11.2026 po przeglądzie: „nie wiem” = dolna granica, liczy się do wejścia reguł, nie do wyjścia, normy ani uspokojenia, R-DANE-52)* (na danych 17 aparatów: 0–4% nocy, jeden 18%). Zastępuje regułę „OA/CA nieznane, gdy w nocy nie ma bezdechu z rodzajem” z `tools/kalibracja/noce.py`, która na dobrych nocach dawała „nie wiem”.
6. **Noce z importu karty (ADR-0017) w ASV:** te same metryki z PLD i EVE z kopii karty (czytnik nocy), żeby noce z karty wróciły do L-04 i nowych wariantów; adnotacja 31.10.2026 w ADR-0017 zastąpiona.
7. **Poprawki faktów:** Załącznik A (rodzaj bezdechu i CSR „w danych AirBridge”), wcześniejsze pytanie o przenoszenie rodzaju bezdechu (zamknięte z błędnym wnioskiem dla trybów ASV), test kalibracji przenoszenia rodzaju bezdechu (w ASV bezprzedmiotowy).

## Konsekwencje

- Na ResMed ASV wracają reguły, które dotąd zawsze milczały, w wersji opartej na pracy aparatu; noce z karty wracają do L-04.
- To nowe heurystyki bez badań walidacyjnych: progi `[PRÓG]`, stan „tylko raport”, pytanie do lekarza przed aktywacją pilności. Ryzyko: „pełne wsparcie” bywa osiągane także bez bezdechów (na egzemplarzu referencyjnym P95 PS bywa tuż poniżej maksimum), dlatego warunek liczy bezdechy w oknie zdarzenia, nie sam czas przy maksimum.
- Metryki zdarzeniowe wymagają plików nocy (etap 2) albo karty; noc z raportu JSON w ASV ma tylko bezdechy/h i „aparat nie rozróżnia”.
- Teksty w ASV nie używają słów „centralne” ani „obturacyjne”.
- Dokumenty: spec funkcjonalna (4.3, 4.11, 5.12, K2, raport 5.13, R-SKI-02, Załącznik A, 7.8, 10.1), katalog `L.csv`, `E.csv`, `O.csv`, `H.csv`, `teksty.csv`, `progi.csv`, ADR-0017/0029/0030 (dopiski), CHANGELOG.

## Wprowadzenie do specyfikacji (09.11.2026, do akceptacji właściciela projektu)

| Punkt | Gdzie w spec funkcjonalnej |
|---|---|
| 1 (zasada „aparat nie rozróżnia”) | 4.3 (M-03, M-09), 4.11 (wiersz „rodzaj bezdechu”), R-DANE-50, Załącznik A (`counts`, `csr_seconds`) |
| 2 (metryki) | 4.3: M-30 (bezdechy / h), M-31 i M-31p (przy pełnym wsparciu), M-32 i M-32p (przy maksymalnym EPAP), M-33 (czas poniżej celu wentylacji); R-DANE-51; 4.4 i 4.5 (norma, Δ M-31); progi w `docs/spec/progi.csv` (`M-31.*`, `M-32.*`, `M-33.*`) |
| 3 (reguły) | 4.11 (warianty L-01, L-02, L-04, H-02, E-33, O-12), 5.12.4, 5.12.5, 6.4, 6.5, R-SKI-02 (fail-path); teksty w `docs/spec/katalog/teksty.csv` (`*_asv`, `*_m32`); progi `L-01.asv_*`, `L-04.*bezdechy_max_epap`, `E-33.asv_mediana` |
| 4 (K2 i raport) | 5.5.3, R-KOK-15, 5.13.3 (strefy 5 i 7), 5.13.4, R-LEK-40 (osobne wskaźniki, nie podział); **odstępstwo:** w ASVAuto K2 ma `pełne wsparcie` i `maks. EPAP`, a wszystkie bezdechy / h i „bez żadnego z nich” tylko w raporcie, bo 11. wiersz się nie mieści (do decyzji właściciela projektu) |
| 5 (UA poza ASV) | R-DANE-52 (dolna granica) |
| 6 (import karty) | R-DANE-31, R-SKI-02, adnotacja ADR-0017 |
| 7 (fakty) | Załącznik A, test kalibracji, przypadki 51 i 53 przepisane, nowe 70–75 |

Teksty wycofane po wdrożeniu (zostają w `teksty.csv`, dopóki katalog ich używa): `L-04.powod_asvauto` (wycofane, zob. `L-04.powod_asvauto_m32`), `H-02.pytanie_asvauto` (wycofane, zob. `H-02.pytanie_asvauto_m32`).

Poprawki po przeglądzie 09.11.2026: uspokojenie w ASV (5.4.13, R-SKI-50: stałe ograniczenie aparatu nie blokuje „Noc w normie”, brak plików nocy tak), maksimum PS ograniczone zakresem aparatu 25 cmH2O (M-31), definicja okna, M-30 ze STR (`AI`) i bez raportów AirBridge < v2.2.0, L-01 ASV z odsetkiem z puli, Δ M-30 dla O-12, S8 z wierszem `Bezdechy`.

Katalog reguł (`L.csv`, `E.csv`, `O.csv`, `H.csv`) bez zmian do implementacji: silnik reguł nie zna metryk M-30…M-33 (lista `METRYKI` w `src/reguly_rodzaje.cpp`), więc nowe warianty dostaną wiersze katalogu razem z kodem po etapie 2.

## Zmiana 2026-11-11 (ADR-0040; bez zmiany decyzji)

Portret nocy w trybach ASV i ASVAuto nie rysuje pików „do środka” (zdarzeń centralnych), bo aparat nie zapisuje rodzaju bezdechu; zamiast tego pik oznacza „większe wsparcie oddechu” (pomiar PS), a każde zdarzenie to kropka (spec 5.4.16, R-POR-10).
