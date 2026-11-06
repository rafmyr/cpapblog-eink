# ADR-0004: Skierowania do lekarza i hipoteza „najpewniej do poprawy”

| | |
|---|---|
| Data | 22.10.2026 |
| Status | przyjęta |
| Źródło | specyfikacja 7b, 7c |

## Kontekst
Autor projektu chciał listy wzorców, przy których warto iść do lekarza, z dedykowanym raportem i opcjonalnym wskazaniem, co najpewniej jest do poprawy.

## Decyzja
12 wzorców L-01…L-12 z pilnością A (kilka dni, buzzer) i B (tygodnie). 8 hipotez H-01…H-08 dołączanych do L, domyślnie włączonych, wyłączalnych. „Omówione z lekarzem” wycisza wzorzec na 56 nocy.

## Konsekwencje
Zastępuje eskalacje E-30/32/34/35/37/38 i ekrany S-K-22…25. Progi wymagają przeglądu przez lekarzy specjalistów (etap 2).

## Adnotacja (23.10.2026, spec 1.1)
Spec od v1.0 ma trzy poziomy pilności: A (kilka dni, buzzer), B (tygodnie) i C (bez pełnego ekranu: znacznik w K2, linia w niedzielnym S-K-18, wpis w raporcie; spec 5.12.2). L-09 („przeciek nie ustępuje”) nie jest skierowaniem do lekarza: to ekran „problem” z krokiem do dostawcy sprzętu, a lekarz dostaje go tylko jako wpis w raporcie, jak pilność C (R-SKI-15). Oba doprecyzowania mieszczą się w tej decyzji (wzorce L z pilnością i wyłączalne hipotezy H); nie zmieniają jej.

## Adnotacja (31.10.2026, przegląd dokumentacji)
Spec ma dziewiątą hipotezę H-09 (`Najpewniej do poprawy: czas z maską`, przy L-11), dodaną 23.10.2026. To obszar zachowania, nie ustawień aparatu (ADR-0012), więc mieści się w tej decyzji; liczba „8 hipotez” wyżej jest historyczna.

## Adnotacja (04.11.2026, ADR-0029)
Zakres skierowań w wydaniu 1.0 ustala [ADR-0029](0029-zakres-skierowan-w-wydaniu-1-0.md): aktywne z ekranem i pilnością są tylko L-03 (≥ 10 / h), L-08, L-09, L-11, a L-01 po teście na egzemplarzu, pozostałe wzorce działają w stanie „tylko raport” (K2 i raport, bez pilności). Pilność A daje tylko L-06(b); L-02 i L-05 mają najwyżej B. Hipoteza H-04 w trybie ASV wycofana (zamiast niej H-08), H-01 w trybie ASV nie działa (konsultacja kliniczna, spec 4.11).
