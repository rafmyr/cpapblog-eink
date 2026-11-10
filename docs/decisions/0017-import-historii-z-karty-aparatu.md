# ADR-0017: Jednorazowy import historii nocy z kopii karty SD aparatu

| | |
|---|---|
| Data | 29.10.2026 |
| Status | przyjęta (wyjątek od ADR-0001); zmieniona 30.10.2026 (sekcja „Zmiana 30.10.2026”) |
| Źródło | decyzja 29.10.2026; R-DANE-01, R-DANE-31 |

## Kontekst
Nowy moduł AirBridge (od v2.2.0) startuje bez żadnych nocy: raporty liczy dopiero z sesji zapisanych po instalacji. Norma osobista wymaga co najmniej 14 nocy ważnych, a pełna 30 (R-NOR-01, R-NOR-10), więc bez importu panel przez 2–4 tygodnie nie porównuje nocy z normą. Kopia karty SD aparatu (np. AirCurve 10 CS PaceWave) zawiera wiele miesięcy nocy, a czytnik nocy z etapu 2 ([`tools/kalibracja/`](../../tools/kalibracja/README.md)) liczy z niej te same metryki co AirBridge od v2.2.0.

Rozważone warianty:

| Wariant | Dlaczego nie |
|---|---|
| wgrać pliki nocy na pamięć nowego AirBridge, żeby liczył z nich raporty | nie wiadomo, czy moduł ma zapisywalną kartę; autor projektu wybrał import na panel |
| czekać 14–30 nocy | panel przez 2–4 tygodnie pokazuje liczby bez normy |
| historia z widgetu na Macu | ok. 19 nocy, część po wyzerowaniu ciśnienia i wycieku przez AirBridge v1.6.2 |

## Decyzja
Wyjątek od ADR-0001: panel może raz wczytać historię nocy z pliku zrobionego na komputerze z kopii karty SD aparatu (czytnik nocy z repo, bez zmian w metodzie liczenia). Plik leży wyłącznie lokalnie: na komputerze użytkownika (poza repo, WY12) i w pamięci panelu. Noce z importu:
- uzupełniają tylko okres sprzed pierwszej nocy pobranej z AirBridge; noc z AirBridge zawsze wygrywa (zmienione 30.10.2026: zob. „Zmiana 30.10.2026”; noc z AirBridge wygrywa tylko, gdy jest pełna; spec R-DANE-31 i R-DANE-34);
- liczą się do normy i okien reguł jak noce z AirBridge, z OA, CA, CA/h i M-09 jako brak danych (karta AirCurve nie zapisuje rodzaju bezdechu ani CSR, 4.11);
- nie liczą się do okresu testowego WY16 i nie zastępują porównania WY9.

Poza tym importem obowiązuje ADR-0001 bez zmian: dane terapii tylko z AirBridge w sieci lokalnej, bez chmury.

## Konsekwencje
- Norma od pierwszego dnia pracy panelu z nowym AirBridge.
- Drugie źródło danych terapii, ale jednorazowe i lokalne: nowe wymaganie R-DANE-31, wyjątek w R-DANE-01.
- Ryzyko: metryki z karty i z AirBridge mogą się różnić metodą (np. IPAP w części nocy); rozjazd poza tolerancją WY9 oznacza ponowne zbudowanie normy bez nocy z importu.
- Prototyp (`firmware/prototyp/`) wczytuje plik z pamięci flash panelu (LittleFS), nie z karty microSD; w specyfikacji technicznej (etap 3) miejsce pliku do ustalenia.

## Zmiana 30.10.2026
Decyzja 30.10.2026 (adnotacja zamiast nowego ADR). AirBridge v2.2.1 zaciągnął z pamięci aparatu ok. 181 dób, z dziurami i z dobami tylko z podsumowania STR, więc reguła „noce z importu tylko sprzed pierwszej nocy z AirBridge” gubiłaby noce z karty w dziurach albo zostawiała doby bez kanałów EDF zamiast pełnych nocy z karty. Teraz:
- pełna noc z AirBridge (kompletna, `events_known` = `true`, kanał z `source` = `"edf"`) zastępuje noc z importu tej samej daty w całej historii, a pola, których raport nie ma (np. wsparcie PS), zostają z karty;
- doba niepełna z AirBridge (tylko STR albo „częściowa”) nie wypiera nocy z karty; import zastępuje taką dobę nocą z karty;
- import do pełnego rekordu tej daty dopisuje tylko brakujące pola.
Skutek: norma i liczba nocy ważnych nie spadają przez pierwsze pobranie historii; noce strojenia mogą mieszać źródła. Wymaganie: R-DANE-31 w wersji 1.2.

## Adnotacja 31.10.2026 (decyzja; zastąpiona 09.11.2026, zob. „Zmiana 2026-11-09 (ADR-0037)” niżej)
Decyzja bez zmian; doprecyzowanie, jak noce z importu wchodzą do okien reguł w trybie ASV. Noc z importu bez rodzaju bezdechu liczy każdy bezdech, także centralny, do UA, czyli do zdarzeń obturacyjnych. W trybie ASV i ASVAuto taka noc nie liczy się do L-04 ani H-02: okno 7 nocy L-04 bierze tylko noce z rodzajem bezdechu, więc po imporcie L-04 milczy do 7 nocy z AirBridge (spec 4.11, R-DANE-31, przypadek 53 w 7.8). Pozostałe reguły liczą noce z importu jak dotąd.

## Zmiana 04.11.2026 (bez zmiany decyzji)

Import historii z karty aparatu z metrykami EDF (M-03 OA/CA/H, M-08, M-13, M-17) zostaje w kolejności „na koniec”, nie przed wersją beta (decyzja 04.11.2026). Skutek: noce z importu nie mają tych metryk, więc większość wzorców L i część zdarzeń E daje „nie oceniam”, dopóki panel nie zbierze nocy z AirBridge. Ekrany lekarskie wersji beta pokazują scenariusze symulatora z nocami syntetycznymi.

## Zmiana 2026-11-09 (ADR-0035; bez zmiany decyzji)

Obok importu z karty panel ma drugie, automatyczne źródło historii: noce z pliku STR.edf aparatu pobieranego z AirBridge od v2.3.1 (źródło `S`, R-DANE-38…R-DANE-41). Import z karty zostaje bez zmian; dla nowych paneli przestaje być potrzebny, bo historia czasu z maską, AHI i przecieku przychodzi z pierwszym porannym pobraniem. Pierwszeństwo: pełna noc z AirBridge > noc z karty > noc ze STR; noc z karty zastępuje noc ze STR tej samej daty tak jak niepełną dobę z AirBridge i dostaje jej pola, których karta nie ma (`src/airbridge.cpp` `wpiszImport()`). Noce ze STR, jak noce z karty, nie liczą się do okresu testowego WY16 ani do porównania WY9.

## Zmiana 2026-11-09 (ADR-0037; zastępuje adnotację 31.10.2026, decyzja importu bez zmian)

Adnotacja z 31.10.2026 zakładała, że noc z AirBridge ma w trybie ASV rodzaj bezdechu, a noc z karty go nie ma. Sprawdzenie 09.11.2026 ([ADR-0037](0037-asv-bez-rodzaju-bezdechu-i-csr.md)) pokazało, że aparaty ResMed ASV nie zapisują rodzaju bezdechu ani CSR w żadnym źródle, więc różnicy między nocą z karty i z AirBridge nie ma. Od zmiany spec 09.11.2026 (zmiana do akceptacji właściciela projektu, kod po etapie 2 przejścia na EDF): w trybach ASV i ASVAuto OA, CA, CA/h i M-09 każdej nocy, także z karty, mają stan „aparat nie rozróżnia” (R-DANE-50), a czytnik nocy liczy z PLD i EVE kopii karty metryki stanu aparatu przy bezdechu M-30…M-33 (R-DANE-51), więc noce z karty liczą się do L-04, H-02 i wariantów ASV z spec 4.11 jak noce z plików. Wymóg „okno L-04 tylko z nocy z rodzajem bezdechu” wycofany (R-SKI-02, R-DANE-31, przypadek 53 przepisany). W trybach CPAP i AutoSet noc z karty bez rodzaju bezdechu ma OA, CA i CA/h jako brak danych jak dotąd.
