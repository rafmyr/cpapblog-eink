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

## Zmiana 2026-11-22

Wyjątek tylko dla tekstów programu „Dobry start” ([ADR-0050](0050-opcje-komfortu-w-poradach.md), decyzja właściciela projektu 22.11.2026): porada może nazwać opcję komfortu, którą pacjent zmienia sam według instrukcji aparatu (nawilżanie, temperatura podgrzewanej rury, rampa), podać kierunek i odesłać do instrukcji; ulgę na wydechu tylko opisuje i odsyła do lekarza albo dostawcy. Liczby i poziomy ustawień, droga w menu aparatu albo AirBridge, ciśnienie, zakres, tryb i poziom EPR oraz diagnoza pozostają zakazane wszędzie; pozostałe ekrany i raport bez zmian.
