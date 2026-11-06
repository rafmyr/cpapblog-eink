# ADR-0014: Nowy poziom nie obejmuje metryk zdrowotnych

| | |
|---|---|
| Data | 23.10.2026 |
| Status | przyjęta; doprecyzowuje ADR-0003 (nie zastępuje go) |
| Źródło | przegląd specyfikacji v1.0; lista zmian przyjęta 23.10.2026 |

## Kontekst
ADR-0003 opiera odchylenia na normie osobistej z histerezą. Spec v1.0 (4.7, R-NOR-06, R-NOR-13) dodała „nowy poziom”: metryka w stanie odchylenia przez 21 kolejnych nocy ważnych kończy stan, a te noce stają się nową normą, z linią `Nowy poziom: {metryka} {x}, to teraz norma.` Uzasadnienie (inna maska, zmiana, której nie da się cofnąć) pasuje do przecieku i czasu z maską. Reguła działała jednak dla każdej metryki z tabeli 4.5, także dla AHI, bezdechów centralnych, oddechów na minutę i ciśnienia P95. Audyt pokazał skutek: wzorce L-05 i L-07 liczą się od normy, więc po 21 nocach trwałe pogorszenie samo kończy wzorzec, a ekran mówi pacjentowi, że to „norma”. To mechanizm, który z czasem ucisza sprawę zdrowotną bez poprawy metryki i bez lekarza.

## Decyzja
1. Nowy poziom dotyczy tylko metryk technicznych (sprzęt i nawyk): M-01 czas na masce, M-07 wyciek, M-17 przerwy. Kolumna „Rodzaj” w tabeli 4.5 specyfikacji jest jedyną listą.
2. Dla metryk zdrowotnych (M-02, M-03, M-05p, M-11, M-13, M-14, M-15) stan odchylenia trwa tak długo, jak trwa odchylenie. Po 21 nocach [PRÓG] panel:
   - dopisuje metrykę do następnego raportu dla lekarza i do każdego kolejnego, dopóki stan trwa;
   - raz na 7 dni [PRÓG], nie codziennie, pokazuje linię uwagi w tonie informacyjnym, bez słowa „wciąż”: `Od {n} tyg.: {metryka} {x}. Pokaż lekarzowi.`;
   - liczy zdarzenia i wzorce L od normy sprzed odchylenia, także gdy w 90 nocach wstecz zabraknie nocy spoza stanu.
3. Granica ADR-0002 bez zmian: linia mówi „pokaż lekarzowi”, nie podaje wartości ustawień ani kierunku zmiany.

## Konsekwencje
- Spec 1.1: 4.4, 4.5 (kolumna „Rodzaj”), 4.7, R-NOR-06, R-NOR-10, R-NOR-13 zmienione; nowe R-NOR-20, R-NOR-21, R-NOR-22; L-05 i L-07 w 5.12.4; strefa 2 raportu (5.13.3); kolejka linii 7.5 (grupa „wzorce L”); przypadek 21 w 7.8; lista kontrolna „mechanizmy” w 9.3.
- Pacjent z trwałym pogorszeniem metryki zdrowotnej widzi raz w tygodniu linię i ma sprawę w raporcie, zamiast komunikatu „to teraz norma”. Koszt: jedna linia tygodniowo więcej przy długim odchyleniu; progi 21 nocy i 7 dni do kalibracji w etapie 2.
- Zmiana ustawień nadal zeruje normę ciśnienia i GI (R-NOR-07), także w stanie odchylenia; skutek zmiany pokazuje L-12.

## Adnotacja (04.11.2026, konsultacja kliniczna, bez nowego ADR)
Decyzja po konsultacji klinicznej (04.11.2026): linia z pkt 2 brzmi `Od {n} tyg.: {metryka} {x}. Liczby w raporcie.` zamiast `… Pokaż lekarzowi.`, a krok ekranu zdarzenia tej metryki po 21 nocach (spec R-NOR-23) to `→ Obserwuj kolejne noce.`. Powód (opinia eksperta): wzrost względem normy przez 21 dni może być zmianą z bardzo niskiego na nadal niski AHI; „Pokaż lekarzowi” ma wynikać z aktywnej reguły L albo z utrzymujących się objawów. Reszta decyzji bez zmian: brak nowego poziomu dla metryk zdrowotnych, zdanie w każdym raporcie, linia raz na 7 dni. **Pkt 3 jest uchylony** tą odpowiedzią lekarza (bez nowego ADR): linia i krok ze stanu odchylenia nie mówią już „pokaż lekarzowi”; część pkt 3 o granicy ADR-0002 (bez wartości ustawień i kierunku zmiany) obowiązuje dalej.
