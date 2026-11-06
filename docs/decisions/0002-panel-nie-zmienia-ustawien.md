# ADR-0002: Panel nie zmienia ustawień aparatu i nie diagnozuje

| | |
|---|---|
| Data | 22.10.2026 |
| Status | przyjęta |
| Źródło | specyfikacja rozdz. 1 p. 5, 7c; decyzja 22.10.2026 |

## Kontekst
AirBridge technicznie pozwala zapisywać ustawienia terapii (`/api/settings`, bez walidacji zakresów). Rozważano wskazania typu „podnieś EPAP”, ale na razie panel daje tylko wskazanie, że warto udać się do lekarza.

## Decyzja
Panel nigdy nie zmienia ustawień, nie podaje wartości do ustawienia ani drogi do zmiany (w tym ścieżki w AirBridge) i nie stawia diagnoz. Dozwolone: opis pomiaru, wskazanie konsultacji z lekarzem, kierunek „najpewniej do poprawy” (ADR-0004).

## Konsekwencje
Mniejsze ryzyko regulacyjne (MDR, reguła 11) i medyczne. Zmiana tej decyzji wymaga nowego ADR i analizy MDR.
