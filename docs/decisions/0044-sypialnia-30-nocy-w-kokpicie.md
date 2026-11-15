# ADR-0044: Strona „Sypialnia · 30 nocy” jako czwarta strona kokpitu

| | |
|---|---|
| Data | 14.11.2026 |
| Status | przyjęta 14.11.2026 (decyzja właściciela projektu: „zgoda na 4 ekran”); zmienia [ADR-0041](0041-krotszy-kokpit.md) pkt 1 (kokpit `n / 4` przy danych sypialni) |
| Źródło | makieta „Sypialnia: 30 nocy” (liczby przykładowe; górny pasek według belki z [ADR-0043](0043-belka-informacyjna.md)); spec funkcjonalna 5.5.8 (R-KOK-29…R-KOK-33), 5.5.7, 5.6.1 pkt 2; norma sypialni 4.4; S2c (5.4.2) |

## Kontekst

Dane sypialni (temperatura i wilgotność z czujnika w obudowie panelu, M-22; CO2 z lokalnego źródła otoczenia, M-23) panel zapisuje co noc w `/dane/sypialnia/{RRRR}.csv`, ale pokazuje je tylko jako jedną noc: pasek S2c (dowód spokoju, rotacja) i stopka `przy łóżku …`. Przebiegu z wielu nocy nie widać nigdzie. właściciel projektu zgodził się 14.11.2026 na czwartą stronę kokpitu z trzema wykresami z 30 nocy.

Od ADR-0041 kokpit ma trzy strony z numerem (`n / 3`: K1, K2, U-01) i dwie bez numeru, zależne od stanu („Co znaczą te liczby”, „Otwarte sprawy”). Teksty w zestawie danych i w kodzie odsyłają do numerów stron (`Na kontroli: raport 2 / 3.`, `Ustawienia: strona 3 / 3.`).

## Decyzja

1. **Nowa strona kokpitu z numerem** „Sypialnia · 30 nocy” (`Ekran::SYPIALNIA`, `src/sypialnia_ekran.cpp`): po „Otwartych sprawach” (a bez nich po K2 albo „Co znaczą te liczby”) i przed „Ustawieniami”, które zostają ostatnią stroną. Ciąg: K1 `1 / 4` ▶ K2 `2 / 4` ▶ („Co znaczą te liczby”) ▶ („Otwarte sprawy”) ▶ Sypialnia `3 / 4` ▶ U-01 `4 / 4`. Strony bez numeru bez zmian.
2. **Strona tylko z danymi.** W ciągu jest, gdy w 30 nocach (daty poranka od dziś − 29 do dziś) jest co najmniej jedna noc z co najmniej jednym odczytem (temperatura, wilgotność albo CO2). Bez danych strony nie ma, a kokpit zostaje `n / 3` jak w ADR-0041. Jedno miejsce prawdy: `ustawSypialnieWKokpicie()` w `src/nawigacja.cpp`, ustawiane przez dyrygenta z `stronaSypialniJest()`.
3. **Treść:** trzy wykresy liniowe jeden pod drugim (temperatura, wilgotność, CO2), przy każdym nazwa, duża wartość ostatniej nocy z jednostką, kropka na ostatniej nocy, dwie przerywane linie zwykłego pasma i słowo z prawej: `jak zwykle`, `niżej niż zwykle`, `wyżej niż zwykle`. Legenda jednej linii: `● ostatnia noc`, `┄ zwykłe pasmo z 30 nocy`.
4. **Zwykłe pasmo = P10–P90 normy sypialni z 4.4** (30 ostatnich nocy z odczytem przed ostatnią nocą w 90 dniach, bez nocy „spanie poza domem”, minimum 14 nocy jak każda norma, ADR-0003), to samo pasmo, które daje słowo `jak zwykle` w S2c. Dzięki temu obie strony nie mówią o tej samej nocy różnie. Mniej niż 14 nocy: wykres bez pasma i bez słowa oceny, z napisem `uczę się`.
5. **Strona opisuje, nie doradza** (ADR-0002): bez progów zdrowotnych, bez ocen „dobrze/źle”, bez zaleceń. CO2 ponad 1000 ppm nie dostaje tu czarnego pola „ponad próg” (S2c je ma, R-EKR-54); do decyzji: pytanie otwarte.
6. **Brak danych nigdy nie wygląda jak „jak zwykle”** (ADR-0029 pkt 7): pomiar bez żadnego odczytu w 30 nocach ma wykres z napisem `brak odczytu` w polu wykresu (nie puste pole); brak odczytu ostatniej nocy: linia z wcześniejszych nocy, wartość `–`, słowo `brak odczytu`, bez kropki. Noce bez odczytu przerywają linię (bez łączenia przez lukę); noc pojedyncza między lukami to kwadrat 5 px.
7. **Forma wizualizacji:** wariant formy „linia” z 5.4.3 („linia z pasmem”): dwie przerywane linie pasma zamiast jednej linii progu albo normy, kropka ostatniej nocy, linia 3 px, przerywane 2 px; bez nowej (dziesiątej) formy.
8. **Numery stron w tekstach** (`n / 3` w zestawie i w kodzie) panel przelicza w jednym miejscu, `numeryStronWTekscie()` (`src/nawigacja.cpp`, wołane w `reguly::tekst()` i przy tekstach składanych w kodzie): przy stronie sypialni `1 / 3` → `1 / 4`, `2 / 3` → `2 / 4`, `3 / 3` (U-01) → `4 / 4`; bez strony tekst bez zmian. Zestaw danych zostaje bez zmian.
9. **Przyciski:** reguła B31 i stopka `◀ ▶ ↻` bez zmian: `◀ sprawy` / `◀ liczby` / `◀ 7 nocy`, `▶ ustawienia`, `↻ ekran dnia`; przytrzymania na tej stronie nic. Strona przed nią podpisuje ▶ jako `sypialnia` (`▶ sypialnia · 2 s raport` w K2 bez spraw, `▶ sypialnia` na ostatniej karcie spraw i w „Co znaczą te liczby”), U-01 podpisuje ◀ jako `◀ sypialnia` (od 14.11.2026 stopka U-01 ma tylko krótkie naciśnięcia, przytrzymania w kafelkach). Powrót po restarcie z pamięci RTC jak dla innych stron (`ekranZRtc`); strona w pamięci RTC bez danych w 30 nocach daje ekran dnia.

## Konsekwencje

- Jedno naciśnięcie więcej z K2 do Ustawień, gdy są dane sypialni (z 2 albo 3 do 3 albo 4 ▶ z K1).
- Teksty z numerem strony zmieniają liczby w zależności od danych sypialni (np. `Na kontroli: raport 2 / 4.`); test zestawu i spec pokazują brzmienie `n / 3`, a przeliczenie robi panel (R-KOK-30). Teksty przechowane już złożone (np. w raporcie) mogą mieć numer z dnia złożenia.
- Panel przy łóżku pokazuje domownikom przebieg temperatury, wilgotności i CO2 z 30 nocy (te same dane co stopka i S2c, bez danych terapii).
- Kod: `src/sypialnia_ekran.cpp`, `src/nawigacja.cpp` (`Ekran::SYPIALNIA` = 19, `KontekstNaw::sypialniaDostepna`, `ustawSypialnieWKokpicie()`, `stronKokpitu()`, `numeryStronWTekscie()`), `src/dyrygent.cpp` (`sypialniaWKokpicieB44()`, `rysujSypialnieB44()`), stopki K2, „Otwartych spraw”, „Co znaczą te liczby” i U-01; testy `test/testy_sypialnia_30.cpp` (T-TEST-430), podglądy `sypialnia-30-*`.
- Strona jest widoczna w obu profilach (prywatnym i publicznym): pokazuje tylko warunki w pomieszczeniu, bez danych terapii; w profilu publicznym stoi po „Co znaczą te liczby”. Czy „Co znaczą te liczby” ma objaśniać „zwykłe pasmo”: pytanie otwarte.
- Część F zmiany UX (kolejność i widoczność ekranów z telefonu) dostaje stronę sypialni na liście stron kokpitu.

## Odrzucone warianty

- **Strona sypialni po „Ustawieniach”** (`4 / 4` za U-01): Ustawienia przestałyby być końcem ciągu, a za nimi są tylko ekrany serwisowe (przytrzymanie); trudniej znaleźć U-01.
- **Strona sypialni zaraz po K2:** rozdzielałaby K2 od „Co znaczą te liczby” (objaśnienie liczb K1 i K2) i od „Otwartych spraw” (sprawy terapii).
- **Pasmo z kwartyli (P25–P75) ostatnich 30 dni kalendarzowych:** byłby to drugi, inny „zwykły” zakres niż w S2c dla tych samych danych; nowe progi bez źródła.
- **Zawsze `n / 4`, a bez danych strona z samymi „brak odczytu”:** strona bez treści w ciągu u osób bez czujnika CO2 i przed pierwszą nocą; dodatkowe naciśnięcie bez informacji.
- **Zmiana tekstów zestawu na numery bez liczby stron** (`raport w 7 nocach`): zmiana treści zestawu i zatwierdzonych tekstów poza zakresem tej zmiany.

## Zmiana 2026-11-14

10. **Wykres (decyzja 9 właściciela projektu 14.11.2026 17:32, „obie naraz”; zastępuje rysunek linii z pkt 3, 6 i 7):**
    - **oś Y:** przy znanym paśmie oś = pasmo rozszerzone o swoją szerokość w górę i w dół, więc zwykłe pasmo zajmuje środkową trzecią część pola wykresu (najmniejszy zakres osi bez zmian, `[PRÓG]`); wartość poza osią (kropka albo punkt średniej) przycięta do krawędzi pola z małą strzałką „poza skalą”; bez pasma („uczę się”) oś jak dotąd;
    - **linia:** średnia krocząca z 7 nocy (noc i 6 poprzednich, także sprzed okna; mniej nocy: średnia z tylu, ile jest; noce bez odczytu nie wchodzą do średniej; punkt tylko w nocy z odczytem); luka dłuższa niż 2 noce `[PRÓG]` przerywa linię, krótsza jest mostkowana;
    - **kropka:** prawdziwa wartość ostatniej nocy (może leżeć poza linią); duża liczba i słowo `jak zwykle / niżej / wyżej niż zwykle` nadal dla ostatniej nocy wobec pasma, ta sama reguła co S2c;
    - **legenda:** `● ostatnia noc`, `— średnia z 7 nocy`, `┄ zwykłe pasmo` (pytanie otwarte).
    **Koszt przyjęty przez właściciela projektu: linia nie pokazuje pojedynczych nocy** (jedna odstająca noc rozmywa się w linii na 7 punktów); pojedynczą noc widać tylko jako kropkę ostatniej nocy, a noc poza skalą jako strzałkę przy krawędzi.
11. **Przed zamknięciem nocy** (przed 06:00, wiersz nocy powstaje przy jej zamknięciu) w miejscu wartości `noc trwa`, bez słowa i bez kropki; wartość poprzedniej nocy nie udaje dzisiejszej (R-KOK-31).
12. **Dane z pliku:** przy powtórzonej dacie strona bierze pierwszy wiersz, jak S2c; `inf` i `nan` odrzucane, wartości skrajne przycinane przy wczytaniu (temperatura ±99,9 °C, wilgotność 0…100 %, CO2 do 9999 ppm).
13. **Numery stron w tekstach** (pkt 8) panel przelicza tylko zaraz po słowie strony (`strona`, `stronie`, `stroną`, `strony`, `stronę`, `raport`, `raporcie`, `raportu`), także z twardą spacją; budowa zestawu odrzuca `{1, 2, 3} / 3` bez takiego słowa.

## Zmiana 2026-11-14 ([ADR-0045](0045-kolejnosc-i-widocznosc-ekranow-z-telefonu.md))

Pkt 1 i 2: miejsce strony w ciągu ustawia właściciel w formularzu U-03 (sekcja „Ekrany”, klucz `kokpit.kolejnosc`; domyślnie przed U-01 jak w pkt 1), a pole „pokazuj” (`kokpit.ukryte=sypialnia`) ukrywa ją także przy danych z 30 nocy; jedno miejsce prawdy dalej w nawigacji (`sypialniaWKokpicie()` = dane i „pokazuj”, `ciagKokpitu()`). Stopki z pkt 9 biorą podpisy z nawigacji (`podpisCelu()`), więc przy innej kolejności strona podpisuje swoich prawdziwych sąsiadów (np. `◀ ekran dnia`, `▶ 7 nocy` jako pierwsza strona). Pkt 8 i 13: numery w tekstach liczone dla dowolnej kolejności (R-KOK-36). Strona ukryta zapisana w pamięci RTC: ekran dnia jak strona bez danych (log `strona sypialni poza ciągiem (bez danych z 30 nocy albo ukryta z telefonu): ekran dnia`).
