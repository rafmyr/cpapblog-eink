# ADR-0012: Hipotezy „najpewniej do poprawy” bez kierunku zmiany ustawień

| | |
|---|---|
| Data | 22.10.2026 |
| Status | przyjęta; doprecyzowuje ADR-0004 (nie zastępuje go) |
| Źródło | przegląd specyfikacji v1.0; decyzja 22.10.2026 |

## Kontekst
ADR-0004 wprowadził hipotezy H-01…H-08: panel może wskazać, co w danych najbardziej pasuje do przyczyny wzorca L. Przegląd wykazał dwie rzeczy:
1. Warunek H-04 „EPR ≥ 2” to typowe ustawienie, więc hipoteza „ustawienia ciśnienia lub EPR” pokazywałaby się przy prawie każdym L-01 i nic nie różnicowała.
2. Pytania w raporcie (np. „czy obniżyć ciśnienie lub EPR”, „czy rozważyć wyższe minimum”, „podnieść maksimum”) podsuwają kierunek zmiany. Raport trafia na telefon pacjenta; pacjent może sam zmienić ustawienie, objaw zniknie z ekranu, a przyczyna (np. leki, choroba serca przy bezdechach centralnych) zostanie nieomówiona. Litera ADR-0002 (bez wartości i drogi zmiany) była zachowana, ale jego cel nie.

## Decyzja
1. **Hipotezy na ekranie i w raporcie wskazują obszar, nigdy kierunek zmiany.** Dozwolone: „Najpewniej do poprawy: ustawienia ciśnienia lub EPR”, „…: minimalne ciśnienie”. Zakazane: „wyższe”, „niższe”, „podnieść”, „obniżyć”, „zwiększyć”, „zmniejszyć” w odniesieniu do ustawień aparatu.
2. **Pytania do lekarza w raporcie są otwarte**, np. „Czy ustawienia ciśnienia lub EPR mogą mieć z tym związek, czy szukać innej przyczyny?”. Dotyczy H-01…H-07.
3. **H-04 tylko po zmianie ustawień:** warunek „L-01 pojawił się w ciągu 56 nocy po zmianie ciśnienia lub EPR”. Warunek „EPR ≥ 2” usunięty. Bez zmiany ustawień przy L-01 panel nie pokazuje hipotezy o ustawieniach.
4. Wyłączenie podpowiedzi „co do poprawy” w ustawieniach panelu usuwa hipotezy z ekranu i z raportu.

## Konsekwencje
- Lekarz dostaje mniej sugestii w raporcie; dane, na których opiera się hipoteza, zostają.
- Zmniejsza ryzyko samodzielnej zmiany ustawień przez pacjenta i ryzyko regulacyjne (MDR).
- Słowa z pkt 1 dochodzą do przeglądu ADR-0002 w spec (rozdz. 9.3) jako wzorce zakazane dla tekstów hipotez.

## Adnotacja (04.11.2026, konsultacja kliniczna, bez nowego ADR)
Decyzja po konsultacji klinicznej (04.11.2026): **H-04 wycofane w wydaniu 1.x we wszystkich trybach** (na ASV i w CPAP z AutoSet): przy L-01 po zmianie ustawień ramka `Przyczynę oceni lekarz.` (H-08) i zdanie z datą zmiany w raporcie (R-HIP-10). Także H-01 (`minimalne ciśnienie`) i H-02 w AutoSet (`maksymalne ciśnienie`, `tryb terapii`) wycofane (pkt 4 i 6). Pkt 1–2 i 4 tej decyzji bez zmian; pkt 3 zostaje jako historia warunku H-04. Przykład „…: minimalne ciśnienie” w pkt 1 jest dalej przykładem obszaru, ale żadna hipoteza wydania 1.x go nie używa.
