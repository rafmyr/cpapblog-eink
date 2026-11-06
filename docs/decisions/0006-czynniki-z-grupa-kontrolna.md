# ADR-0006: Czynniki nocy z grupą kontrolną

| | |
|---|---|
| Data | 22.10.2026 |
| Status | przyjęta |
| Źródło | specyfikacja 7e.1 |

## Kontekst
Autor projektu chciał opcjonalnie monitorować czynniki (alkohol, stres, infekcja, kolacja, THC itd.). Panel nie ma dotyku, a wieczorny dzienniczek przyciskami umiera po 2 tygodniach.

## Decyzja
Panel pyta rano o wczorajszy wieczór tylko po nietypowej nocy oraz po losowej 1 na 7 nocy bez odchylenia (grupa kontrolna). Odkrycia liczone wyłącznie z nocy z odpowiedzią. Wrażliwe czynniki domyślnie nie trafiają do raportu.

## Konsekwencje
Bez grupy kontrolnej odkrycia („po alkoholu gorzej”) byłyby przekłamane. Częstość pytań do oceny w trybie próbnym.

**Adnotacja 23.10.2026:** leki nasenne/przeciwbólowe domyślnie w raporcie przy L-01 ([ADR-0013](0013-wyjatki-czas-sieciowy-i-leki-w-raporcie.md) pkt 2).

**Zmiana 04.11.2026 (bez zmiany decyzji):** odkrycia O-06…O-10 i O-12 liczą się z 90 ostatnich nocy ważnych z odpowiedzią, z testem permutacyjnym i poprawką Holma. Oznaczenie „spanie poza domem” po przerwie AirBridge ≥ 2 doby ustawia panel sam (R-CZY-19, warstwa `A`), a pytanie `Spanie poza domem od {data}?` pozwala je zdjąć (R-CZY-22, R-CZY-23); takie noce wypadają z normy, jak noce z odpowiedzią „spanie poza domem”. O-12 zostaje także dla CA/h (decyzja). Czynniki w raporcie: ustawienie `raport.czynniki=` w `/konfiguracja.txt`, bez niego domyślne 5.13.7, więc wrażliwe czynniki dalej domyślnie poza raportem.
