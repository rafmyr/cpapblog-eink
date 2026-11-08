# ADR-0034: Model pasm wycieku P95 (10 / 15 / 24 / 27 L/min)

| | |
|---|---|
| Data | 08.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 08.11.2026) (zmiana reguły L) |
| Źródło | lekarz medycyny snu (konsultacja 04.11.2026, przegląd CPAP i AutoSet, pkt 9); decyzja właściciela projektu 06.11.2026 (noc niepewna od 15 L/min); [ADR-0029](0029-zakres-skierowan-w-wydaniu-1-0.md) pkt 7, [ADR-0030](0030-wersja-1-dla-asv-cpap-i-autoset.md) (adnotacja z 06.11.2026) |

## Kontekst

Wyciek z maski (M-07, P95 nocy z raportu AirBridge) ma w wersji beta kilka granic, które powstawały po kolei w adnotacjach (04.11 i 06.11.2026), bez jednego zapisu decyzji:

- 06.11.2026 właściciel projektu zdecydował, że noc z P95 ≥ 15 L/min jest niepewna (4.9): jej AHI nie liczy się do E-30, L-03 ani do normy AHI. Powód (obserwacja własna, 1 osoba): przy takim wycieku aparat przestaje wiarygodnie rozpoznawać bezdechy. To próg ostrożny, bez źródła; producent aparatu uznaje za duży wyciek 24 L/min.
- Lekarz medycyny snu (konsultacja 04.11.2026, pkt 9) ostrzegał przed tym wprost: 15 L/min nie powinno unieważniać nocy, bo L-03 zamilknie u osób wymagających oceny.
- Ta decyzja weszła jako adnotacja „bez zmiany decyzji” w ADR-0030, bez nowego ADR.
- Przegląd 08.11.2026 pokazał skutek: L-09, jedyna reguła, która kieruje do dostawcy sprzętu, liczyła tylko noce z P95 > 24 L/min, a S-K-10 w paśmie 15–24 daje tylko samopomoc („Sprawdź poduszkę i paski”). Przy stałym wycieku 18 L/min i AHI 12 panel miesiącami nie ocenia bezdechów i nigdzie nikogo nie kieruje. Dotyczy to także drugiego użytkownika beta (AutoSet, maska nosowa, start bez historii nocy).
- Przy okazji wyszło, że linia R-SKI-24 `Noce mało czytelne: bezdechów nie oceniam.`, na którą powoływał się ADR-0030 jako „łagodzenie”, w wersji beta nie była zaimplementowana (zestaw miał tylko próg `R-SKI-24.noce_nieczytelne`).

Właściciel projektu wybrał 08.11.2026 wariant: nie podważać 15 L/min, dołożyć siatkę przez L-09.

## Decyzja

1. **Pasma wycieku P95 nocy (każda maska, M-07 P95 z raportu dnia):**

| Pasmo | Co robi panel | Próg |
|---|---|---|
| od 10 do poniżej 15 L/min | S-K-10 w wariancie opisowym `Wyciek widoczny` (ton „informacja”); dolna granica to ustawienie „Uwaga o masce od” (R-KONF-42, 10–15) | `S-K-10.uwaga_o_masce` = 10 |
| od 15 L/min | noc niepewna (4.9, AHI nie oceniane); S-K-10 w tonie „problem”; **L-09 liczy taką noc** | `R-NOR-05.niepewna_p95` = 15, `S-K-10.problem_p95` (= ten sam) |
| ponad 24 L/min | aparat nie kompensuje (granica producenta; na ekranach „duży wyciek” od 15 L/min, decyzja z 06.11.2026): M-08, E-20 (i przez nie E-22), S-K-14, S-K-39, K2 (komórka „ponad próg”), powód S-K-10 z minutami; linia przerywana 24 L/min na wykresach (np. S-K-31, którego warunek to wzrost wobec normy, `S-K-31.delta`, nie próg 24) | `S-K-10.przeciek_nosowa` = `R-DANE-23.przeciek_nosowa` = 24 |
| od 27 L/min | E-23 „Wyciek od 27 l/min w nocy” (R5b) | `E-23.przeciek_p95` = 27 |

2. **Siatka L-09:** warunek `L-09/.sk10` liczy noce z terapią, w których P95 ≥ `R-NOR-05.niepewna_p95` (odwołanie do progu nocy niepewnej, nie liczba wpisana na sztywno), w ≥ 10 z 14 ostatnich nocy z terapią (`L-09.sk10_noce`, bez zmian); noc bez P95 liczy się jako „nie spełnia”. Wyjście: 7 kolejnych nocy z terapią bez E-20 i bez P95 ≥ 15 L/min (noc bez P95 nie przerywa serii). Okno i noce bez P95: rozstrzygnięte 08.11.2026, do potwierdzenia przez właściciela projektu. Alternatywa „E-22 aktywne ≥ 21 nocy” bez zmian. Teksty L-09 bez zmian (`Wyciek nie ustępuje`, `→ Zapytaj dostawcę sprzętu o inną maskę.`). Próg nocy niepewnej jest w dwóch miejscach: w zestawie danych reguł (`R-NOR-05.niepewna_p95`, czyta go L-09) i w kodzie (stała `NIEPEWNA_P95`, liczy noc niepewną). Test (`test/testy_wzorce_l_hipotezy_h.cpp`) pilnuje ich równości w zestawie wbudowanym; zestaw z karty (ADR-0023) z inną wartością rozjechałby oba progi, dlatego zmiana tej liczby wymaga nowego firmware.
3. **Panel mówi, dlaczego nie ocenia:** linia R-SKI-24 `Noce mało czytelne: bezdechów nie oceniam.` (4 z 7 nocy z terapią nie są ważne dla L) wchodzi do kodu, a krok zamienny warunku uspokojenia przy braku ocen z powodu wycieku brzmi `→ Bezdechów nie oceniam: noce z wyciekiem.` (`S-K-18.krok_dane_wyciek`), nie `… za mało nocy.` (5.4.13 pkt 7).
4. **Granice ADR-0002 bez zmian:** L-09 kieruje do dostawcy sprzętu po czynność (inna maska), nie podaje wartości ustawień ani drogi do nich i nie diagnozuje.

## Konsekwencje

- **Koszt dla L-03 zostaje** (świadomy, od 06.11.2026): u osoby ze stałym wyciekiem 15–24 L/min L-03 nadal milczy, bo jej noce są niepewne. Zmiana nie przywraca oceny bezdechów; zapewnia tylko, że po 10 takich nocach w 14 panel kieruje do dostawcy sprzętu (L-09, ekran „problem”, wpis w raporcie dla lekarza, R-SKI-15). Do tego czasu mówi wprost „nie oceniam” (R-SKI-24) i podaje przyczynę (wyciek).
- **Więcej ekranów L-09:** pasmo 15–24 L/min daje L-09 tam, gdzie wcześniej był tylko S-K-10. Wejście wymaga 10 z 14 nocy, więc pojedyncze złe noce go nie dają; limit „ten sam problem poranny” (R-KON-14) i „wiem” (R-PRZ-03) działają jak dotąd. Przy masce po terminie pierwszeństwo ma dalej S-K-31 (R-MAS-03, R-MAS-10).
- **Maska ustno-nosowa (twarzowa):** próg 15 L/min jest jeden dla każdej maski, a ATS 2013 (W) podaje dla maski twarzowej 36 L/min jako duży wyciek. U jej użytkownika P95 15–24 L/min może być stanem zwykłym: dostanie trwały `Wyciek nie ustępuje` z krokiem `→ Zapytaj dostawcę sprzętu o inną maskę.` i linię R-SKI-24, bez wyjścia, dopóki P95 ≥ 15. Koszt: częstsze skierowania do dostawcy i możliwa niepotrzebna wymiana maski. Drugi użytkownik beta ma maskę nosową, więc beta tego nie pokaże. Przed udostępnieniem panelu innym osobom: kalibracja osobno dla maski twarzowej i pytanie o typ maski do lekarza PSG.
- **Noce bez P95** (import z karty, raport bez P95): okno wejścia L-09 to 14 ostatnich nocy z terapią, noc bez P95 nie spełnia warunku, więc L-09 nie trzyma się starych nocy; wyjście po 7 nocach bez P95 ≥ 15, noc bez pomiaru nie przerywa serii (spec 5.12.11).
- **Wykres L-09 w raporcie** (parametr kluczowy, 5.13.4) ma linię przerywaną 15 L/min zamiast 24; wykres wycieku w strefie 5 raportu ogólnego zostaje z 24 L/min.
- **Kryterium przed wydaniem 1.0:** kalibracja progu 15 L/min (ile nocy jest niepewnych przy 15 L/min i co to robi z L-03, R-SKI-24 i L-09) musi być zamknięta przed 1.0. Do tego czasu 15 L/min to `[PRÓG]` „do kalibracji”.
- **Pytanie do lekarza PSG:** czy 15 L/min to dobra granica nocy niepewnej i czy siatka L-09 od tej granicy jest właściwa. Do odpowiedzi to opinia właściciela projektu wbrew opinii lekarza medycyny snu z konsultacji 04.11.2026.
- **Odwracalność:** zmiana progu nocy niepewnej to liczba w `progi.csv` (`R-NOR-05.niepewna_p95`) i stała `NIEPEWNA_P95` w kodzie, razem, w nowym firmware (test pilnuje równości); L-09 czyta próg z zestawu. Powrót do „L-09 tylko > 24” wymaga nowego ADR, bo znów zostawiłby pasmo bez wyjścia.
- Dokumenty zmienione razem: spec funkcjonalna 4.9, 5.4.13, 5.12.4, 5.12.5, 5.13.4, 10.1; `progi.csv` (`L-09.sk10_noce`, `R-NOR-05.niepewna_p95`); katalog `L.csv`, `teksty.csv`, `SK.csv`; ADR-0030 (odesłanie).
