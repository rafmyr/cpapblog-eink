# ADR-0007: Dwa tryby ekranu: dane i karta

| | |
|---|---|
| Data | 22.10.2026 |
| Status | przyjęta |
| Źródło | specyfikacja 4.7 |

## Kontekst
Porównano 4 kierunki (A minimalny, B tarcza nocy, C gazeta, D komiks) i 3 sposoby łączenia postaci z A (karta, podpis, wychylenie). Decyzja: utrzymać minimalizm A i „ożywić” go budzikiem, ale nie na każdym ekranie.

## Decyzja
Ekran danych (szablon A, bez budzika) albo karta z budzikiem (W1), bez mieszania. Z kierunku C: tytuł-zdanie, tabela kokpitu 7 nocy, obsługa pytania. Wzorce L, kokpit, pytania i raport zawsze jako ekran danych.

## Konsekwencje
Budzik nic nie komunikuje: usunięcie go nie zmienia informacji. Limity: max 2 karty na dobę, połowa wyświetleń w tygodniu.

## Adnotacja 30.10.2026
Dochodzi trzeci rodzaj ekranu: ekran nocny S9 „Dobranoc” z własną budową, rysowany raz o 23:30 ([ADR-0018](0018-ekran-nocny.md)). Ma budzik i fakty, ale nie ocenia nocy; nie jest kartą ani ekranem danych i nie liczy się do limitów kart. Zasada „budzik nic nie komunikuje” obowiązuje także na nim.

**Zmiana 04.11.2026 (bez zmiany decyzji):** nowe ekrany wersji beta trzymają podział „dane albo karta”. Pełne ekrany zdarzeń E (S3), K1 z odkryciem, „Twoja norma” S8, „Co znaczą te liczby” (profil publiczny), „Sprzęt” i ekran raportu z kodem QR to ekrany danych bez budzika. Odkrycia S1 i ekrany S-K mają kartę albo ekran danych według tabeli 5.7.3. Przerywniki S2d G01–G18 to karty w miejscu dowodu spokoju, losowane w wersji beta ([ADR-0008](0008-budzik-postac.md) „Zmiana 04.11.2026”).

**Adnotacja 08.11.2026:** [ADR-0027](0027-ekran-dzienny.md) pkt 4 dodaje czwarty rodzaj ekranu, obrazek dnia (S-K-38): bez tytułu, oceny i budzika, nie jest kartą ani ekranem danych. To wyjątek od zasady „dwa tryby, bez mieszania”; budzik nigdy na czarnym tle (ADR-0009) obowiązuje bez zmian. W kodzie wersji beta rodzajów ekranu jest więc cztery: ekran danych, karta, S9, obrazek dnia.

**Zmiana 2026-11-11 (ADR-0040; bez zmiany decyzji):** Portret nocy (S11, spec 5.4.16), nowy pierwszy ekran poranka z rysunkiem nocy na tarczy zegara, jest ekranem danych bez budzika i nie ma wersji karty; humor i limity kart go nie dotyczą (R-POR-01, R-POR-20).
