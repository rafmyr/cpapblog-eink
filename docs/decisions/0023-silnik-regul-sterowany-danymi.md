# ADR-0023: Silnik reguł sterowany danymi: progi, katalog reguł i teksty jako pliki

| | |
|---|---|
| Data | 01.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 01.11.2026): wariant A z rozumieniem „reguła = wiersz katalogu, rodzaj warunku = kod”; zachowanie z pkt 5 wymaga zmiany spec |
| Źródło | ADR-0020; ADR-0002, ADR-0004, ADR-0012, ADR-0016 (tabela wariantów zależnych od trybu); spec 4.11, 5.12, 6.4, 6.5, 7.2, WY16; [`docs/spec/progi.csv`](../spec/progi.csv) (338 progów), `tools/test_progi.py`; prototyp: progi w kodzie (np. `PROG_AHI = 5.0`, `PROG_WYCIEK = 24.0` w `src/kokpit.h`, `CO2_PROG_NOCY = 1000` w `src/sypialnia.h`), teksty ekranów jako napisy w `src/dzien.cpp`, `src/humor.cpp`, `src/dobranoc.cpp` |

## Kontekst

Etap 2 (kalibracja progów na nocach referencyjnych, przegląd przez lekarzy specjalistów) nie jest zamknięty i będzie zmieniał progi, a może też pilność wzorców L, hipotezy H i teksty. Po kalibracji ma się zmieniać plik, a nie kod: progi, reguły i teksty są danymi z wersją, wczytywanymi z pliku.

Dziś progi i teksty siedzą w kodzie prototypu, a każda zmiana to nowa kompilacja i wgranie. Spec ma już źródło progów: `docs/spec/progi.csv` (338 wierszy, kolumny `id_progu, regula, miejsce, wartosc, jednostka, opis, zrodlo, pochodzenie, tryb, status`), sprawdzane testem względem spec. WY16 każe zamrozić progi przed okresem testowym.

Panel ma granice z ADR-0002 i ADR-0012: nie podaje wartości ani drogi zmiany ustawień i nie diagnozuje. Im więcej zachowania przechodzi do plików, tym łatwiej zmienić je bez testów i bez przeglądu tej granicy, co ma też znaczenie dla analizy MDR.

Pytanie do rozstrzygnięcia: co dokładnie jest danymi, a co kodem. Przyjęte rozumienie: **reguła = wiersz katalogu** (co, kiedy, jak pilne, jakim tekstem), **rodzaj warunku = kod** (jak liczyć). Wymagany jest katalog reguł i tekstów jako tabela; wariant A to spełnia.

## Warianty

| Wariant | Co jest w plikach | Co zostaje w kodzie | Skutek i koszt | Rekomendacja |
|---|---|---|---|---|
| **A. Parametry, katalog i teksty w plikach; rodzaje warunków w kodzie** | progi (z `progi.csv`); katalog reguł: które wzorce L, zdarzenia E, hipotezy H, ekrany S-K i odkrycia O są włączone, ich pilność, okna nocy, tryby terapii (wariant z ADR-0016), który rodzaj warunku i z jakimi progami; teksty ekranów i zdania budzika | rodzaje warunków jako nazwane funkcje, np. „metryka poza normą w k z n nocy”, „wartość powyżej progu przez n nocy”, „trend 28 nocy” | zmiana progu, pilności, okna, tekstu, włączenie albo wyłączenie wzorca i przeniesienie wzorca na inny istniejący rodzaj warunku to zmiana pliku. Nowy rodzaj warunku to zmiana kodu z testem. Koszt: katalog reguł do zbudowania z tabel 5.12, 6.4, 6.5 i 7.2 oraz ok. 15–25 rodzajów warunków (szacunek, policzy sekcja 9) | **rekomendowany** |
| B. Reguły jako wyrażenia w pliku (własny mały język) | wszystko z A plus same warunki jako wyrażenia, np. `ahi_p95_7 > norma.p90 && noce >= 3` | interpreter wyrażeń | najwięcej elastyczności: lekarz specjalista mógłby zaproponować zupełnie nową regułę bez programisty. Ale interpreter na ESP32 to nowy kod do testów i nowa klasa błędów (literówka w pliku = cicha zła reguła). Nowa reguła omija testy jednostkowe, przegląd granicy ADR-0002 i analizę MDR, bo nie jest kodem | odrzucony |
| C. Tylko progi w pliku | wartości progów | katalog reguł, pilność, okna, teksty | najmniej pracy teraz. Każda zmiana tekstu, pilności albo włączenia wzorca po konsultacji klinicznej to nowy firmware; teksty dalej rozsiane po kodzie, więc limity długości i granicy ADR-0002 sprawdza się ręcznie. Nie spełnia założenia „reguły i teksty jako dane” | odrzucony |

## Decyzja (proponowana: wariant A)

1. **Trzy pliki danych z wersją**, generowane skryptem z repozytorium wyłącznie ze spec funkcjonalnej i `progi.csv` (nie z makiet: część tekstów makiet jest niedozwolona, spec 9.3), nie pisane ręcznie:
   - **plik progów**: wartości z `docs/spec/progi.csv` dla trybu obsługiwanego (ASVAuto w 1.x), z nagłówkiem: wersja formatu, wersja progów, tag zamrożenia WY16, skrót (hash) zawartości;
   - **katalog reguł**: jeden wiersz na regułę (ID L, E, H, S-K, O), pole „rodzaj warunku” z listy w kodzie, odwołania do progów po `id_progu`, okna, pilność, tryby, grupa linii uwagi, odwołanie do tekstów;
   - **plik tekstów**: teksty ekranów, kroków, linii uwagi i zdań budzika po ID, z polami `{zmienna}` jak w spec (punkt rozszerzenia z przeglądu specyfikacji: inny język albo inne brzmienie bez zmiany kodu).
2. **Rodzaj warunku to kod.** Lista rodzajów jest zamknięta i każdy ma testy jednostkowe na przypadkach granicznych (tuż pod progiem i na progu). Katalog może wskazać tylko rodzaj z tej listy.
3. **Kontrola przed wgraniem, nie na panelu.** Skrypt w `tools/` buduje pliki i sprawdza: każdy próg ma źródło w spec; każdy tekst mieści się w limicie 5.4.4 (te same reguły co `tools/spojnosc.py`); każdy znak tekstu jest w krojach pisma wgranych w panel (kroje są przycinane do potrzebnych znaków, R-NF-14); katalog wskazuje tylko istniejące rodzaje warunków. **Granica ADR-0002**: słowa kluczowe z przeglądu 9.3 (`cmH2O`, `EPR`, `EPAP`, `PS`, `wyższ`, `niższ` i inne) trafiają też w teksty dozwolone, więc skrypt sam nie rozstrzyga. Każdy tekst z trafieniem skrypt zgłasza ostrzeżeniem, a sam tekst musi stać dosłownie w spec funkcjonalnej; nowy albo zmieniony tekst z trafieniem przechodzi przegląd ręczny według 9.3 przed wydaniem (Zmiana 2026-11-06: opis dopasowany do skryptu, który ostrzega, a nie trzyma osobnej listy tekstów przejrzanych). Dwie reguły 9.3 bez słowa kluczowego sprawdza się czytaniem przy każdej zmianie tekstów L, H i raportu. Zmiana katalogu (pilność, okno, wyłączenie wzorca) przechodzi listę „mechanizmy” z 9.3.
4. **Panel sprawdza przy starcie** wersję formatu i skrót (hash) każdego pliku oraz to, że trzy pliki pochodzą z jednej wersji zestawu. Skrót chroni przed uszkodzeniem pliku, nie przed ręczną zmianą: panel nie odróżni pliku zbudowanego skryptem od poprawionego ręcznie, więc granica ADR-0002 w plikach trzyma się procesu z pkt 3.
5. **Zachowanie przy złym pliku albo regule bez progu** (to nowe zachowanie, więc przy implementacji idzie do spec funkcjonalnej jako wymagania z ID, nie tylko do tego dokumentu): zły albo brakujący zestaw → panel używa całego zestawu wbudowanego w firmware (jednej wersji, nigdy mieszanki), pokazuje to na ekranie testu i w linii uwagi grupy „awarie”, a noce z tego czasu nie liczą się do okresu testowego WY16. Reguła, której nie da się policzyć (nieznany rodzaj, brak progu) → nie działa i jest traktowana jak nieoceniona: blokuje warunek uspokojenia (5.4.13), więc panel nie powie „w normie” o tym, czego nie ocenił (ten sam mechanizm, którego prototyp już używa w `PUNKTY_USPOKOJENIA`).
6. **Jak zmiana po kalibracji trafia na panel.** Zmiana w `progi.csv` (z wpisem w CHANGELOG i, po WY16, nowym tagiem zamrożenia) → skrypt buduje zestaw → wgranie zestawu bez kasowania danych panelu → panel po starcie pokazuje nową wersję progów na ekranie testu. Kod się nie zmienia. Droga wgrania zależy od ADR-0022 (przy wariancie A: plik z karty) i od decyzji o aktualizacji oprogramowania (sekcja 8); dzisiejsze `uploadfs` z prototypu odpada, bo kasuje historię, czynniki i dziennik ekranów.
7. **Zmiana zachowania przez plik to nadal zmiana specyfikacji.** Nowy próg, inna pilność, nowe okno albo nowy tekst najpierw trafia do spec funkcjonalnej (ID, CHANGELOG, przegląd granicy ADR-0002 z pkt 3), potem do pliku. Silnik niczego nie dodaje poza tym, co jest w spec.

## Konsekwencje

- Somnolodzy i kalibracja w etapie 2 nie blokują budowy firmware: zmieniają pliki, nie kod.
- Rodzaje warunków trzeba wydzielić z tabel 5.12, 6.4, 6.5 i 7.2 (sekcja 9); reguła, której nie da się zapisać istniejącym rodzajem, dostaje nowy rodzaj w kodzie i test.
- Punkt rozszerzenia z przeglądu specyfikacji („inny język bez zmiany kodu”) działa tylko częściowo: nowe znaki wymagają przebudowy krojów pisma, czyli nowej wersji firmware.
- Teksty wychodzą z kodu prototypu (`src/dzien.cpp`, `src/humor.cpp`, `src/dobranoc.cpp` i inne) do pliku tekstów; dzisiejsze sprawdzenie tekstów w `tools/prototyp/testy.sh` przechodzi na plik.
- Format plików (CSV czy inny, nośnik, migracja) rozstrzyga ADR o danych trwałych (sekcja 9); ten ADR ustala tylko, co jest danymi.
- Względem wariantu B ryzyko MDR jest mniejsze: każda nowa logika przechodzi przez kod, testy i przegląd. Pliki zmieniają parametry i teksty, a ich zgodność z granicą zależy od procesu z pkt 3, nie od panelu. Do odnotowania w analizie MDR (analiza MDR) przy przeglądzie etapu.

## Adnotacja 01.11.2026 (pkt 3)

Decyzja 01.11.2026. **Pkt 3 zmieniony w jednym miejscu:** obok kontroli przed wgraniem panel sprawdza listę reguł obowiązkowych, wkompilowaną w firmware. Reguły rdzenia (spec funkcjonalna 2.6: „nie da się go wyłączyć”, w tym S7 i S8) i skierowania do lekarza L-01…L-12 muszą mieć w katalogu wiersz włączony, z poziomem i pilnością nie niższymi niż w spec (pozostałe parametry, np. okna i progi, pilnuje dalej skrypt budowy i przegląd przed wgraniem); zestaw, który to łamie, panel odrzuca (przy starcie zestaw wbudowany z linią R-AW-30, przy wgraniu z karty pakiet nie wgrany). Wyłączenie modułu ustawieniem użytkownika (R-KONF-32) działa jak dotąd.

Bez zmian: filtr tekstów (słowa kluczowe granicy ADR-0002) zostaje w skrypcie budowy, nie na panelu, bo filtr po słowach odrzucałby też teksty dozwolone z EPAP i cmH2O. Pkt 4 dalej obowiązuje dla tekstów: ręcznie poprawionego pliku tekstów panel nie odróżni od zbudowanego skryptem. Specyfikacja techniczna sekcja 5 (wiersz „tylko odczyt z aparatu”) mówi to wprost. Treść wymagań: spec funkcjonalna 2.6, R-AW-30, R-NF-30; spec techniczna 9.3 („Lista reguł obowiązkowych”), T-WYM-86. Lista reguł spoza kryterium, których tekst kieruje do lekarza albo po pomoc, jest do ustalenia po konsultacji klinicznej.

## Zmiana 2026-11-08 (bez zmiany decyzji)

Źródłem katalogu reguł są ręcznie pisane pliki `docs/spec/katalog/*.csv` z kolumną `zrodlo`; skrypt `tools/zestaw/buduj_zestaw.py` buduje z nich zestaw wbudowany i sprawdza, że każdy tekst stoi dosłownie w spec funkcjonalnej (granica ADR-0002). Pkt 1 („generowane skryptem … wyłącznie ze spec funkcjonalnej i `progi.csv`, nie pisane ręcznie”) działa więc inaczej niż zapisano: katalog pisze się ręcznie, a zgodność ze spec pilnuje skrypt budowy; zestaw na panelu nadal powstaje tylko ze skryptu. Tryby terapii są cztery: `CPAP`, `AutoSet`, `ASV`, `ASVAuto` ([ADR-0030](0030-wersja-1-dla-asv-cpap-i-autoset.md)); zdanie o samym ASVAuto w 1.x jest nieaktualne.
