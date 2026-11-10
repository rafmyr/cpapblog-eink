# ADR-0038: Wydania firmware z prywatnego repozytorium, token w panelu (na czas testów wewnętrznych)

| | |
|---|---|
| Data | 09.11.2026 |
| Status | przyjęta 09.11.2026 (decyzje właściciela projektu 09.11.2026: wariant B, osobne prywatne repo na wydania, token wpisywany w formularzu z telefonu, ważny 1 rok) |
| Źródło | [ADR-0032](0032-aktualizacja-firmware-z-github.md) (mechanizm aktualizacji, zmieniany w pkt 1 i 5), [ADR-0033](0033-licencja-i-dwa-repozytoria.md) pkt 6 (bez zmian), [ADR-0001](0001-przetwarzanie-lokalne.md), [ADR-0011](0011-konfiguracja-przyciski-i-formularz-qr.md), [ADR-0024](0024-aktualizacja-oprogramowania-i-zestawu-danych.md) |

## Kontekst

ADR-0032 ustalił, że panel sam sprawdza i instaluje nowe wersje z wydań GitHub w publicznym `rafmyr/cpapblog-eink`. ADR-0033 pkt 6 zakazał tam wydań z gotowym obrazem do opinii prawnika od wyrobów medycznych. Skutek: mechanizm z ADR-0032 nie miałby czego instalować, a oba panele testowe dalej wymagają kabla albo okna serwisowego przy każdej poprawce.

Właściciel projektu 09.11.2026 zlecił budowę funkcji i wybrał drogę, która nie rusza ADR-0033: wydania w repozytorium prywatnym, dostępne tylko dla paneli z tokenem.

**Oświadczenie właściciela projektu 09.11.2026:** „panel nie będzie udostępniony na rynku ani sprzedawany, to nadal tylko wewnętrzne testy”.

## Decyzja

1. **Źródło wydań na czas testów wewnętrznych: osobne prywatne repozytorium GitHub tylko na wydania** (nazwa poza tym repozytorium). Zawiera wyłącznie wydania: obraz firmware, plik opisu wydania ze skrótem SHA-256, krótką notę. Bez kodu i dokumentów. Zmienia ADR-0032 pkt 1 i dopisek „Zmiana 2026-11-06” (źródłem było `cpapblog-eink`).
2. **Dlaczego nie wydania wprost w repozytorium roboczym autora:** token zapisany w panelu u drugiej osoby dawałby odczyt całego prywatnego repozytorium roboczego. Osobne repo ogranicza skutek wycieku tokenu do plików obrazu.
3. **Token dostępu:** token GitHub typu „fine-grained”, tylko to jedno repozytorium, tylko uprawnienie „Contents: read” (wymagane przez GitHub do listy i pobrania plików wydania), ważność 1 rok. Tworzy go właściciel projektu. Jeden token na panel, żeby dało się unieważnić jeden egzemplarz.
4. **Token trafia do panelu przez formularz z telefonu** (U-03, ADR-0011) i leży w pamięci panelu (NVS), nigdy w obrazie firmware, nigdy na ekranie, w logu, raporcie ani w kopii na karcie. W formularzu pole zachowuje się jak hasła (puste z napisem `ustawiony`).
5. **Adres repozytorium wydań i token to ustawienia, a nie stałe w kodzie.** Adres ma wartość domyślną wkompilowaną w firmware (zmienia ADR-0032 pkt 5 „adres wpisany na stałe”: zostaje stały w danym obrazie, ale jest jednym parametrem budowy). Po opinii prawnika przejście na publiczne wydania to zmiana wartości domyślnej i pusty token, bez przepisywania kodu.
6. **Bez tokenu panel nie pyta GitHuba wcale** (ani o wersję, ani o plik) i pokazuje, że aktualizacje są wyłączone. To zachowanie obrazu zbudowanego z publicznego kodu przez obcą osobę.
7. **Reszta ADR-0032 bez zmian:** sprawdzenie raz na dobę i na żądanie, instalacja po przycisku, opcjonalny automat z warunkami, tylko wersja wyższa, powrót do poprzedniej wersji, HTTPS z weryfikacją certyfikatu, SHA-256, zgodność płytki i układu partycji, obraz bez sekretów, hasło okna serwisowego poza obrazem, podpis obrazu przed panelem dla kolejnej osoby.
8. **ADR-0033 pkt 6 bez zmian:** żadnych wydań z gotowym obrazem w publicznym `cpapblog-eink` do opinii prawnika. Kod aktualizacji trafia do publicznego repo jak reszta kodu; bez tokenu nic nie pobiera (pkt 6).

## Konsekwencje

- **Ruch wychodzący (ADR-0001, R-NF-07):** czwarty rodzaj ruchu z ADR-0032 idzie teraz do `api.github.com` i do serwera plików GitHuba, z tokenem w nagłówku. Zapytanie dalej nie zawiera danych terapii, użytkownika ani numeru aparatu. GitHub widzi adres IP domu, wersję panelu i to, który token pyta (czyli który panel).
- **Ryzyko tokenu:** kto wyjmie token z pamięci panelu (dostęp fizyczny i port szeregowy), może czytać repozytorium wydań do końca ważności tokenu albo do unieważnienia. Nie może nic opublikować (tylko odczyt). Ryzyko podmiany obrazu z ADR-0032 (przejęcie konta GitHub) zostaje bez zmian i dalej zamyka je dopiero podpis.
- **Koszt utrzymania:** token wygasa po roku; panel pokazuje wtedy, że nie może sprawdzić wersji, do wpisania nowego. Jedno repozytorium więcej. Część pracy (obsługa tokenu) po przejściu na publiczne wydania przestaje być potrzebna, ale zostaje w kodzie jako pusta ścieżka.
- **MDR (interpretacja autora, nie prawnika):** prywatne wydania dla dwóch paneli w testach wewnętrznych autor ocenia jako niebędące „udostępnianiem na rynku”; opiera się na oświadczeniu właściciela projektu z kontekstu. To domysł do potwierdzenia przez prawnika (pytanie otwarte).
- **Warunek wyjścia poza dwa panele:** bez zmian względem ADR-0032 i ADR-0033: podpis obrazu i opinia prawnika przed panelem dla kolejnej osoby.
- **Dokumenty:** ADR-0032 dostaje dopisek „Zmiana 2026-11-09”; spec funkcjonalna (R-NF-07, 5.6, nowy ekran U-07) i techniczna (8.8, 8.10) zmieniają się w tym samym scaleniu co kod.

## Zmiana 2026-11-09 (wdrożenie)

Kod po przeglądzie 10.11.2026; nic nie wgrane na panel: sprawdzenie i instalacja z wydań (`src/aktualizacja.cpp`, `src/dyrygent.cpp`, adapter `WydaniaEsp32`), ekran U-07, obraz wydania bez sekretów (`-DBEZ_SEKRETOW`, `[env:e1001_wydanie]`, `tools/wydanie/wydaj.py`), hasło okna serwisowego, adres i token źródła otoczenia i współrzędne pogody w pamięci panelu. Pierwszą wersję z aktualizatorem buduje się z `secrets.h` i wgrywa kablem albo oknem serwisowym; obraz wydania (bez sekretów) tylko na panel, który ma już te wartości w pamięci. W publicznym repozytorium `cpapblog-eink` wartością domyślną adresu repozytorium wydań (pkt 5) jest `rafmyr/cpapblog-eink`; wydań z gotowym obrazem tam nie ma (pkt 8), więc obraz zbudowany z publicznego kodu bez własnego parametru budowy niczego nie instaluje. Wymagania: R-NF-32…R-NF-36, R-KONF-43…R-KONF-46, R-EKR-61; spec techniczna T-WYM-93…T-WYM-96, T-INT-40.
