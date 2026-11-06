# ADR-0015: Przeznaczenie panelu

| | |
|---|---|
| Data | 23.10.2026 |
| Status | przyjęta; doprecyzowuje ADR-0002 (nie zastępuje go); adnotacja 24.10.2026: [ADR-0016](0016-tryb-asv-w-wydaniu-1x.md) zawęża przeznaczenie w wydaniu 1.x do trybu ASV aparatu AirCurve 10 CS PaceWave („osoba leczona aparatem CPAP” w pkt 1 czytać jako „osoba leczona aparatem w trybie obsługiwanym w wydaniu”); adnotacja 04.11.2026: [ADR-0030](0030-wersja-1-dla-asv-cpap-i-autoset.md) rozszerza wydanie 1.x do czterech trybów (`CPAP`, `AutoSet`, `ASV`, `ASVAuto`) aparatów ResMed serii 10; progi w trybach innych niż ASVAuto nie mają nocy strojenia |
| Źródło | przegląd specyfikacji v1.0; decyzja 23.10.2026 |

## Kontekst
ADR-0002 mówi, czego panel nie robi: nie zmienia ustawień aparatu, nie podaje wartości ani drogi do ich zmiany i nie diagnozuje. Spec v1.0 dopisała do tego oświadczenie „panel nie jest wyrobem medycznym” (1.3) i taką samą stopkę raportu. Przegląd specyfikacji wskazał dwie rzeczy:
1. Status prawny podany jako fakt, choć pytanie o status (czy skierowania, hipotezy i raport czynią z panelu wyrób medyczny, MDR reguła 11) jest otwarte, a 2.4 sama mówi, że te funkcje „mogą zbliżać panel do wyrobu medycznego”.
2. O kwalifikacji decyduje przeznaczenie określone przez wytwórcę (MDR art. 2 pkt 12), a spec nie miała zdania, dla kogo panel jest, czego nie ocenia i do czego nie służy.

Raport 05 (uwaga 17) dodał, że progi spec są dla dorosłych, a spec nic nie mówi o dzieciach ani o osobach, które nie odpowiadają na pytania przyciskami.

## Decyzja
1. **Spec ma akapit „Przeznaczenie” w 1.2.** Panel jest dla dorosłej osoby leczonej aparatem CPAP pod opieką lekarza, która sama obsługuje aparat i panel; nie jest przeznaczony dla dzieci. Panel zestawia dane zapisane przez aparat z własną historią użytkownika i wskazuje, kiedy porozmawiać z lekarzem. Nie ocenia natlenienia krwi bez oksymetru. Nie służy do stanów nagłych.
2. **Spec nie stwierdza statusu prawnego.** W 1.3 uzasadnienie „panel nie jest wyrobem medycznym” zastępuje „panel nie diagnozuje; status wobec MDR: otwarty”.
3. **Stopka raportu dla lekarza:** `Zestawienie danych z aparatu, nie dokumentacja medyczna. Ocena należy do lekarza.`
4. Granica ADR-0002 bez zmian. Ta decyzja dopisuje przeznaczenie, nie zmienia żadnej funkcji panelu.

## Konsekwencje
- Spec 1.1: 1.2 (akapit „Przeznaczenie”), 1.3, 2.2, strefa 10 raportu w 5.13.3; pytanie o status MDR zostaje otwarte (etap 2), a analiza MDR ma gotowe przeznaczenie do oceny.
- Przy publikacji jako open source przeznaczenie trzeba przejrzeć razem z oceną statusu MDR i profilem „publiczny”; opis w README i w instrukcji ma być z nim zgodny, bo przeznaczenie wynika też z materiałów i deklaracji (MDR art. 2 pkt 12).
- Koszt: jeden akapit i dwa zdania; żadna funkcja nie znika. Przeznaczenie nie rozstrzyga kwalifikacji wobec MDR (to robi analiza MDR w etapie 2), tylko przestaje jej przeczyć.
