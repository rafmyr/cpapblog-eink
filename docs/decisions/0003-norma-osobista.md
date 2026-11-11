# ADR-0003: Norma osobista zamiast progów podręcznikowych

| | |
|---|---|
| Data | 20.10.2026 |
| Status | przyjęta |
| Źródło | specyfikacja rozdz. 3 |

## Kontekst
Dla wielu parametrów z aparatu nie ma progów w literaturze, a pacjenci różnią się między sobą.

## Decyzja
Odchylenie liczone jest względem własnych 30 ważnych nocy (P10–P90), z minimalnym efektem Δ, wejściem w stan przy 3 z 5 nocy i histerezą. Progi bezwzględne tylko tam, gdzie mają źródło.

## Konsekwencje
Wymaga rozruchu (pierwsze 14 nocy ograniczona funkcjonalność). Progi oznaczone [PRÓG] wymagają kalibracji (etap 2).

## Zmiana 2026-11-11

Zerowanie normy przy zmianie ustawień (spec 4.8, R-NOR-07) obejmuje w trybach ASV i ASVAuto także M-13, M-14 i M-15 (decyzja właściciela projektu 11.11.2026; obserwacja własna, 1 osoba, tryb ASVAuto: po zmianie ustawień poziom oddechu się przesuwa, a metryka zdrowotna nie dostaje nowego poziomu). Zerowanie kończy stan odchylenia tych metryk (fail-path R-NOR-22). CPAP i AutoSet bez tego zerowania (do decyzji). Wdrożenie razem z alertami odchyleń ([ADR-0039](0039-alerty-odchylen-od-normy.md)).
