# ADR-0020: Struktura specyfikacji technicznej, standardy i schemat ID

| | |
|---|---|
| Data | 01.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 01.11.2026) |
| Źródło | założenia etapu 3 (standardy, proponowana zawartość, metoda, kryteria zakończenia); ADR-0015; spec 1.6 (rodziny ID), 8.1 (budżet), 9.2 (macierz); `tools/spojnosc.py`, `tools/wycofane.csv` |

## Kontekst

Specyfikacja funkcjonalna (wersja 1.2, ok. 4 400 linii, ok. 460 wymagań EARS) mówi, **co** panel robi i mówi. Brakuje dokumentu, który mówi, **jak** panel jest zbudowany: z jakich części składa się firmware, gdzie i w jakim formacie trzyma dane, jak się go testuje, kompiluje, wgrywa i wydaje. Prototyp (z wariantem `/noce.csv` o 24 polach) dowodzi, że to się da zrobić, ale nie jest architekturą docelową: część logiki siedzi w stałych w kodzie, format `/noce.csv` (24 pola) i jego migracja istnieją tylko w kodzie, a tabela „Uproszczenia” w README prototypu wylicza rozjazdy ze spec.

Dokument ma dać się przeczytać obcej osobie (projekt może zostać otwarty), ma pilnować, żeby każde wymaganie funkcjonalne miało część firmware, która je realizuje, i test, który je sprawdza, i nie może po cichu zmieniać zachowania opisanego w spec funkcjonalnej. Przed pisaniem trzeba ustalić strukturę dokumentu, rolę standardów i schemat ID, bo od nich zależy skrypt spójności, macierz i praca nad kolejnymi sekcjami.

Stan przed decyzją (01.11.2026): ID z prefiksem `T-` nie występowały w repozytorium; `tools/wycofane.csv` nie miał wpisów `T-`; `tools/spojnosc.py` nie znał rodziny `T` (rodziny: S-K, R, E, L, H, O, M, P, U).

## Decyzja

### 1. Jeden plik, diagramy w tekście

- Specyfikacja techniczna to jeden plik `docs/spec/specyfikacja-techniczna.md`, jak specyfikacja funkcjonalna.
- Diagramy (kontekst, kontenery, komponenty, sekwencje) jako bloki Mermaid w tym samym pliku: GitHub rysuje je przy czytaniu, a źródło jest tekstem w repo. Używamy `flowchart` i `sequenceDiagram`, nie eksperymentalnej składni `C4Context` (gorzej się renderuje); poziom C4 mówi podpis diagramu.
- Pliki danych, które wyrastają z dokumentu, leżą obok: generowana macierz techniczna `docs/spec/macierz-techniczna.csv` (pkt 6), istniejący `docs/spec/progi.csv` jako wejście silnika reguł.

### 2. Struktura: arc42 skrócony i dopasowany (14 sekcji)

| Nr | Sekcja | Rozdział arc42 | Standard-wzorzec |
|---|---|---|---|
| 1 | Wprowadzenie: cel, czytelnicy i ich pytania, odwołanie do spec funkcjonalnej, słownik przez odwołanie do spec 1.5 | 1, 12 | 42010 (interesariusze, pytania) |
| 2 | Cele jakości i scenariusze (T-NF), powiązane z R-NF i budżetem 8.1 | 1.2, 10 | 25010 |
| 3 | Ograniczenia: sprzęt, ADR, licencje | 2 | |
| 4 | Kontekst (C4-1) | 3 | C4 |
| 5 | Strategia rozwiązania: kluczowe decyzje z odesłaniem do ADR | 4 | 42010 (uzasadnienia) |
| 6 | Widok bloków (C4-2/3): komponenty T-KOMP, interfejsy T-INT | 5 | C4, 62304 (jednostki testowalne) |
| 7 | Widok czasu wykonania: cykl snu, poranek, wieczór, noc S9, przyciski, AirBridge, awarie, OTA | 6 | |
| 8 | Widok wdrożenia: partycje, LittleFS, NVS, microSD, kompilacja i wgrywanie | 7 | 62304 (konfiguracja) |
| 9 | Koncepcje przekrojowe: model danych T-DANE z wersją i migracją, silnik reguł sterowany tabelą, czas, energia, błędy, log, bezpieczeństwo, punkty rozszerzenia, symulator ekranów | 8 | 29148 (T-WYM) |
| 10 | SOUP i licencje (T-SOUP), propozycja klasy bezpieczeństwa oprogramowania | | 62304 |
| 11 | Plan testów: poziomy, środowiska, kryteria, przypadki T-TEST, przypadki graniczne | | 29119-3 |
| 12 | Plan wydań: wersje, kryteria WY, zamrożenie progów WY16, droga od prototypu do 1.0 | | 62304 (konfiguracja) |
| 13 | Rozjazdy prototyp ↔ architektura docelowa | | |
| 14 | Decyzje (indeks nowych ADR) i otwarte kwestie | 9, 11 | 14971 (tylko odesłanie) |

Na górze dokumentu tabela stanu sekcji: `do napisania` → `szkic` → `do przeglądu` → `przyjęta`. Etap kończy się, gdy żadna sekcja nie ma stanu `do napisania`.

### 3. Odwołanie do specyfikacji funkcjonalnej

- Nagłówek podaje wersję spec funkcjonalnej jako tag `spec-v1.2`; różnice względem tagu sprawdza skrypt (pkt 6: ID usunięte albo wycofane w tagu).
- Odwołania idą w jedną stronę: dokument techniczny wskazuje R-*, M-*, E-*, L-*, H-*, ekrany i ADR; specyfikacja funkcjonalna nie wskazuje ID `T-`. Dzięki temu `tools/spojnosc.py` i `macierz-sledzenia.csv` zostają bez zmian.
- Dokument techniczny nie zmienia zachowania. Gdy architektura wymaga innego zachowania, to zmiana spec funkcjonalnej (ID, CHANGELOG, decyzja), a dokument techniczny tylko na nią wskazuje.

### 4. Schemat ID

ID stabilne jak w spec 1.6: nie zmieniają znaczenia i nie są używane ponownie; zastąpione idą do `tools/wycofane.csv` z następcą.

| Prefiks | Co oznacza | Format | Gdzie zdefiniowane |
|---|---|---|---|
| T-KOMP-nn | komponent firmware (C4-3), jednostka testowalna | 2 cyfry | sekcja 6 |
| T-INT-nn | interfejs: między komponentami albo z zewnętrzem (AirBridge, źródło otoczenia, NTP, formularz QR, OTA) | 2 cyfry | sekcja 6 |
| T-DANE-nn | trwała struktura danych: plik (LittleFS, microSD), klucz albo przestrzeń NVS, stan w pamięci RTC przeżywający uśpienie | 2 cyfry | sekcja 9 |
| T-WYM-nn | wymaganie techniczne bez odpowiednika w spec funkcjonalnej (np. sprawdzenie wersji pliku przy starcie), EARS z kolumną „Weryfikacja” | 2 cyfry | sekcje 6–9 |
| T-NF-nn | scenariusz jakości (bodziec, warunki, mierzalna odpowiedź), wskazuje R-NF | 2 cyfry | sekcja 2 |
| T-SOUP-nn | biblioteka albo firmware obce: wersja, licencja, do czego, ryzyko | 2 cyfry | sekcja 10 |
| T-TEST-nnn | przypadek testowy: poziom, wejście, oczekiwany wynik, wymagania | 3 cyfry od początku | sekcja 11 |

Kryteria wydania zostają w istniejącej rodzinie `WY` ze spec; nowej rodziny dla wydań nie ma. Rozjazdy z sekcji 13 nie dostają własnych ID: wskazują T-KOMP i plik:linię prototypu.

### 5. Dowody i znacznik `[do zmierzenia]`

Każda liczba o sprzęcie i każdy parametr wydajności ma jedno z trzech: link do źródła producenta (Seeed, Espressif, dokumentacja biblioteki), wynik pomiaru w przypadku T-TEST albo znacznik `[do zmierzenia]` z numerem pomiaru z planu pomiarów (`pomiar N`, Zmiana 2026-11-06: zamiast numeru pytania z listy pytań otwartych, która nie jest częścią repozytorium). Progi terapii zostają w `progi.csv` ze znacznikiem `[PRÓG]` (reguła projektu); dokument techniczny ich nie powtarza, tylko opisuje, jak plik trafia na panel.

### 6. Śledzenie i skrypt spójności

- Tabele komponentów mają kolumnę „Realizuje” (R-*, ewentualnie T-WYM), a tabele przypadków testowych kolumnę „Sprawdza” (R-*, T-WYM, T-NF).
- Nowy skrypt `tools/spojnosc_tech.py` (używa funkcji z `tools/spojnosc.py`, ale go nie zmienia) generuje `docs/spec/macierz-techniczna.csv`: wiersz = wymaganie → komponent → test, i zgłasza błędy: ID `T-` użyte, a niezdefiniowane; ID zdefiniowane dwa razy; R-* nieistniejące albo wycofane w spec; wymaganie R-* bez komponentu albo bez testu (chyba że wiersz w tabeli „Wymagania bez komponentu” podaje powód, np. „zablokowane do pomiaru 21”); `[do zmierzenia]` bez numeru pomiaru z planu pomiarów. Plik jest wynikiem skryptu i nie jest edytowany ręcznie, jak `macierz-sledzenia.csv`.
- Skrypt uruchamia się przed oddaniem każdej zmiany w dokumencie, a wynik podaje się w opisie zmiany.

### 7. Standardy: wzorzec, bez deklaracji zgodności

Standardy są wzorcem struktury i praktyki. Dokument nie deklaruje zgodności ani certyfikacji (status wobec MDR otwarty: ADR-0015). Odstępstwa od wzorca, każde z powodem:

| Standard | Bierzemy | Nie bierzemy i dlaczego |
|---|---|---|
| ISO/IEC/IEEE 42010 | interesariusze i ich pytania, widoki, uzasadnienia decyzji | formalnego katalogu punktów widzenia i modeli: przy jednym autorze i jednym panelu tabela „czytelnik → pytanie → sekcja” daje to samo |
| arc42 + C4 1–3 | szkielet i diagramy | C4-4 (poziom kodu): kod jest źródłem prawdy, diagram klas by się rozjechał; rozdziały 11 (ryzyka) i 12 (słownik): ryzyka są w analizie MDR, słownik jest w spec 1.5 |
| ISO/IEC/IEEE 29148 | EARS i kolumna „Weryfikacja” dla T-WYM | osobnego dokumentu wymagań systemowych: wymagania funkcjonalne już są w spec |
| ISO/IEC 25010 | cele jakości jako mierzalne scenariusze T-NF | pełnego modelu cech: tylko cechy, które spec 8.x już wymaga (energia, niezawodność, czas, utrzymywalność, bezpieczeństwo) |
| IEC 62304 (wzorzec) | SOUP, zarządzanie konfiguracją, podział na jednostki testowalne, rozwiązywanie problemów, propozycja klasy | planu rozwoju oprogramowania jako osobnego dokumentu i deklaracji klasy: klasa jest propozycją w sekcji 10, decyzja osobnym ADR po analizie MDR; rozwiązywanie problemów bez rejestru: błąd → przypadek T-TEST, który go odtwarza → poprawka → wpis w CHANGELOG |
| ISO/IEC/IEEE 29119-3 (lekko) | plan testów, poziomy, środowiska, kryteria wejścia i wyjścia, przypadki | dzienników wykonania i raportów incydentów jako osobnych dokumentów: wynik testów to wyjście skryptu w CI albo w opisie zmiany |
| ISO 14971 (odniesienie) | zagrożenia techniczne wskazuje analiza MDR | osobnego rejestru ryzyk |

### 8. Przegląd

Sekcje 6 i 9 (komponenty, model danych, silnik reguł) i całość przed v1.0 przechodzą przez niezależny przegląd, który dostaje tylko dokument, ADR i kryteria; istotne uwagi są rozstrzygnięte przed akceptacją. Pozostałe sekcje: przegląd zbiorczy całości.

## Warianty rozważone

| Temat | Wariant | Skutek i koszt | Wybór |
|---|---|---|---|
| struktura | **arc42 skrócony i dopasowany (14 sekcji)** | dokument mówi tylko to, czego brak w spec funkcjonalnej; sekcje 10–13 (SOUP, testy, wydania, rozjazdy) są tam, gdzie obca osoba ich szuka | **wybrany** |
| | arc42 pełny (12 rozdziałów 1:1) | łatwiejsze porównanie z innymi projektami na arc42, ale rozdział ryzyk dubluje analizę MDR, słownik dubluje spec 1.5, a SOUP, testy i wydania trzeba wciskać w „Koncepcje przekrojowe”; ok. 20–30% tekstu więcej do utrzymania | odrzucony |
| macierz | **osobny plik generowany `macierz-techniczna.csv`** | macierz funkcjonalna i jej skrypt (dziś 0 błędów `--strict`) zostają nietknięte; koszt: nowy skrypt i drugi plik do wygenerowania po zmianie | **wybrany** |
| | nowe kolumny w `macierz-sledzenia.csv` | jeden plik, ale trzeba przebudować generator macierzy funkcjonalnej i każda zmiana dokumentu technicznego zmienia plik etapu 2; ryzyko, że poprawka techniczna zepsuje sprawdzenia spec | odrzucony |
| | tabela ręczna w dokumencie | najmniej pracy na starcie, ale przy ok. 460 wymaganiach nikt nie sprawdzi kompletności; kryterium „każde R-* ma komponent i test” nie dałoby się wykazać | odrzucony |
| układ plików | **jeden plik** | jedna rzecz do czytania i do wydruku (jak spec funkcjonalna, PDF), jeden skrypt; różne osoby mogą pisać różne sekcje, a konflikty przy scalaniu są rzadkie \| **wybrany** |
| | folder z plikiem na sekcję | zero konfliktów przy pracy równoległej, ale czytelnik skacze po 14 plikach, odsyłacze są dłuższe, a skrypt i wersja do druku muszą je składać | odrzucony |

## Konsekwencje

- Pliki: `docs/spec/specyfikacja-techniczna.md` (14 sekcji), `tools/spojnosc_tech.py` z testami, `docs/spec/macierz-techniczna.csv` (generowana). Zmiana 2026-11-06: pliki istnieją.
- `tools/spojnosc.py`, `docs/spec/macierz-sledzenia.csv` i spec funkcjonalna bez zmian.
- Nowe rodziny ID `T-KOMP`, `T-INT`, `T-DANE`, `T-WYM`, `T-NF`, `T-SOUP`, `T-TEST`; `tools/wycofane.csv` obejmuje także je. Przed nadaniem nowego ID: `git fetch origin main` i sprawdzenie, że ID jest wolne na `main`, na otwartych gałęziach i w `tools/wycofane.csv`.
- Duże decyzje techniczne dostają własne ADR (od 0021): podział na komponenty, format plików i migracje, silnik reguł, strategia OTA, klasa bezpieczeństwa oprogramowania.
- Zmiany wzorców L i hipotez H po konsultacjach klinicznych oraz progi po kalibracji (etap 2 otwarty) trafiają do danych silnika reguł, nie do struktury dokumentu; ten ADR ich nie blokuje.

## Adnotacja 01.11.2026 (przegląd sekcji 3 i 4)

Decyzja bez zmian. Doprecyzowanie pkt 4: interfejsy zewnętrzne (T-INT z kierunkiem „panel ↔ coś poza panelem”) są definiowane w sekcji 4 (kontekst), a interfejsy między komponentami w sekcji 6. Kontekst ADR wspomina `/noce.csv` z 24 polami: to wariant prototypu; w bieżącej wersji plik ma 20 pól (`firmware/prototyp/src/noc.h`).

