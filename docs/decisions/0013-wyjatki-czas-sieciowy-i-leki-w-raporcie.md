# ADR-0013: Dwa wyjątki: czas z sieci (do ADR-0001) i leki w raporcie (do ADR-0006)

| | |
|---|---|
| Data | 23.10.2026 |
| Status | przyjęta; doprecyzowuje ADR-0001 i ADR-0006 (nie zastępuje ich) |
| Źródło | decyzja 23.10.2026 |

## Kontekst
1. **Czas.** Panel ocenia świeżość danych, okna slotów i ciszę nocną według zegara. Spec v1.0 (R-AW-08, R-AW-08b) zakłada synchronizację czasu z sieci, a ADR-0001 mówi, że z domu wychodzi tylko zapytanie o pogodę i pyłek.
2. **Leki.** ADR-0006: wrażliwe czynniki nocy domyślnie nie trafiają do raportu dla lekarza. Spec (z v0.6c 7e.1) domyślnie włącza do raportu „leki nasenne/przeciwbólowe” przy L-01, bo leki uspokajające i opioidowe zmieniają ocenę bezdechów centralnych.

## Decyzja
1. **Panel może pobierać czas z serwera czasu w internecie (NTP).** Zapytanie nie zawiera żadnych danych terapii ani danych użytkownika. To trzeci dozwolony ruch wychodzący obok pogody i pyłku. Bez sieci panel działa według R-AW-08b.
2. **Czynnik „leki nasenne/przeciwbólowe” jest domyślnie włączony do raportu wyłącznie przy wzorcu L-01** (bezdechy centralne). Użytkownik może go wyłączyć w konfiguracji. Pozostałe wrażliwe czynniki (alkohol, THC, stres, kolacja, poza domem) zostają domyślnie wyłączone, zgodnie z ADR-0006.

## Konsekwencje
- ADR-0001: lista ruchu wychodzącego to pogoda, pyłek i czas. Spec rozdz. 1.3 i 2.4 wymienia wszystkie trzy.
- ADR-0006: wyjątek dotyczy jednego czynnika i jednego wzorca; uzasadnienie medyczne zostaje w spec z poziomem dowodu z v0.6c.

## Adnotacja (23.10.2026, spec 1.1)
Spec rozdz. 1.3 i 1.4 wymienia cztery rodzaje zapytań: pogoda, powietrze, pyłek i czas. To ta sama lista co tutaj: powietrze za oknem (PM2.5) i pyłek pobiera ten sam serwis pogody dla miasta z konfiguracji, w tym samym zapytaniu albo zapytaniach do tego samego serwisu, bez danych terapii i użytkownika (R-NF-07). Decyzja się nie zmienia; adnotacja tylko uzgadnia nazewnictwo.

## Adnotacja (31.10.2026, ADR-0019)
Gdy skonfigurowane jest lokalne źródło danych otoczenia ([ADR-0019](0019-lokalne-zrodlo-otoczenia.md)), panel bierze pogodę i powietrze z niego i nie pyta serwisu pogody w internecie. Zapytanie do serwisu zostaje dozwolone; lista ruchu wychodzącego się nie zmienia.

## Adnotacja (08.11.2026, ADR-0032)
Lista dozwolonego ruchu wychodzącego z tej decyzji (pogoda, powietrze, pyłek, czas) ma od 05.11.2026 czwarty rodzaj: sprawdzenie i pobranie firmware z wydań GitHub ([ADR-0032](0032-aktualizacja-firmware-z-github.md), zmiana ADR-0001 i ADR-0013; bez danych terapii i użytkownika). Decyzja o czasie z serwera NTP i o lekach w raporcie bez zmian.
