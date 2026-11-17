# ADR-0045: Kolejność i widoczność ekranów z telefonu

| | |
|---|---|
| Data | 14.11.2026 |
| Status | przyjęta 14.11.2026 (decyzja właściciela projektu); zmienia [ADR-0041](0041-krotszy-kokpit.md) pkt 1 i [ADR-0044](0044-sypialnia-30-nocy-w-kokpicie.md) pkt 1 (kolejność stron z numerem ustawiana z telefonu), rozszerza [ADR-0011](0011-konfiguracja-przyciski-i-formularz-qr.md) pkt 2 (nowa sekcja formularza) |
| Źródło | spec funkcjonalna 5.5.7, 5.5.8, 5.5.9 (R-KOK-34…R-KOK-36), 5.6.1 pkt 2, 5.6.4, 5.6.6 (R-KONF-60), 5.8.12 i 7.2 część „Dzień” (R-KON-70…R-KON-72); [ADR-0029](0029-zakres-skierowan-w-wydaniu-1-0.md) pkt 3 i 7; [ADR-0002](0002-panel-nie-zmienia-ustawien.md) |

## Kontekst

właściciel projektu chce w aplikacji na telefonie wybierać kolejność ekranów i to, co panel pokazuje (decyzja 8, 14.11.2026). Dziś kolejność jest stała w kodzie: kokpit K1 ▶ K2 ▶ („Co znaczą te liczby”) ▶ („Otwarte sprawy”) ▶ („Sypialnia · 30 nocy”) ▶ U-01 (ADR-0041, ADR-0044), a w slocie dziennym D0 ▶ D0a ▶ D1 S-K-33 ▶ D2 S-K-34 ▶ D3 S-K-35 ▶ D4 S-K-36 ▶ D5 S-K-37 z obrazkiem dnia S-K-38 w pierwszym bloku (7.2 część „Dzień”, R-KON-50).

Ograniczenia, które trzeba zachować:

- Ekrany, które ostrzegają albo czekają na odpowiedź, nie mogą dać się wyłączyć (granica „nie ograniczasz funkcjonalności”, ADR-0029 pkt 6: wyciszyć można sposób powiadamiania, nie ocenę).
- „Nie oceniam” nigdy nie znaczy „w porządku” (ADR-0029 pkt 7). Od ADR-0041 pkt 4 jedyne miejsce w kokpicie, które mówi `ocena częściowa`, to belka K1.
- Wyniki reguł w stanie „tylko raport” widać tylko w K2 i w raporcie (ADR-0029 pkt 3); raport ogólny otwiera ▶ 2 s w K2 (R-LEK-02).
- Do formularza prowadzi tylko U-01 (▶ 2 s, R-KONF-12): bez U-01 nie ma drogi powrotu do ustawień.
- Kolejność poranka (Portret nocy pierwszy, potem reguły 5.4 i 7.2) jest decyzją z 11.11.2026 i zostaje poza zakresem.
- Po aktualizacji nic nie może się zmienić samo.

## Decyzja

1. **Formularz U-03 ma sekcję „Ekrany”** (R-KONF-60, układ jak sekcje R-KONF-59: zwijana, w nagłówku skrót `{n} strony · {m} ekranów dziennych`) z dwiema listami: strony kokpitu i ekrany w ciągu dnia. Pozycja przestawna ma pole „miejsce” (lista 1…n) i, gdy wolno ją wyłączyć, pole wyboru (pokazuj). Bez JavaScriptu działa samo pole „miejsce”: pozycja, której miejsce się zmieniło, idzie na to miejsce, reszta zachowuje kolejność. Ze skryptem wiersz dostaje strzałki ▲ ▼ (44 px), które przestawiają wiersze i numerują miejsca od nowa. Strzałki wysyłające formularz odrzucone: każde naciśnięcie zapisywałoby wszystkie inne niezapisane zmiany.
2. **Strony kokpitu (R-KOK-34…R-KOK-36):**
   - przestawne: „Ostatnia noc” (K1), „7 nocy” (K2), „Sypialnia · 30 nocy”; numer `n / N` liczony z włączonych stron w kolejności z telefonu, U-01 zawsze `N / N`;
   - przełącznik ma tylko „Sypialnia · 30 nocy” (dalej tylko z danymi z 30 nocy, R-KOK-29);
   - **zawsze włączone, bez przełącznika:** K1 (jedyne miejsce `ocena częściowa` w kokpicie, ADR-0041 pkt 4, ADR-0029 pkt 7, oraz pytanie „Co było inaczej?” ↻ 2 s po terminie linii zaproszenia, R-CZY-08), K2 (wyniki „tylko raport” poza raportem, ADR-0029 pkt 3, raport ogólny ▶ 2 s, R-LEK-02, i cel tekstów `Na kontroli: raport 2 / 3`), U-01 (zawsze ostatnia, droga do formularza);
   - **bez przełącznika, według swoich reguł, zawsze zaraz po K2:** „Co znaczą te liczby” (profil publiczny; objaśnia liczby K1 i K2; włącza ją pole „Profil publiczny”, lista do niego odsyła) i „Otwarte sprawy” (przy co najmniej jednej sprawie; ukrycie łamałoby pkt 6 i 7 ADR-0029). Miejsce zaraz po K2 z tych samych powodów, dla których ADR-0044 odrzucił sypialnię zaraz po K2;
   - Portret nocy pod ▶ (R-POR-24) nie należy do kokpitu: z niego ▶ dalej otwiera pierwszą stronę ciągu (`pierwszaStronaKokpitu()`, domyślnie K1; przy „7 nocy” albo sypialni na początku ta strona), a ◀ z tej strony wraca do niego; stopka Portretu podpisuje ▶ nazwą tej strony (`podpisCelu(Ekran::PORTRET, …)`, domyślnie `▶ ostatnia noc`), a pierwsza strona ma `◀ portret nocy` (tekst `S11.stopka_wstecz` bez zmian). Dzięki temu strony stojące przed K1 nie wypadają z drogi ▶ (zmiana 2026-11-14 po przeglądzie; wcześniej Portret ▶ zawsze otwierał K1);
   - jedno miejsce prawdy: `ciagKokpitu()` w `src/nawigacja.cpp`; stopki wszystkich stron kokpitu i strony Portretu biorą podpis celu ◀ i ▶ z `podpisCelu()`, więc ukryta strona nie jest celem żadnej stopki;
   - numery w tekstach (`raport 2 / 3`, `strona 3 / 3`) nazywają stronę (K2, U-01) i panel wstawia jej bieżący numer i liczbę stron dla każdej kolejności (R-KOK-36).
3. **Ekrany w ciągu dnia (R-KON-70…R-KON-72):**
   - przestawne z przełącznikiem (kolejność D1…D4): „Wietrzenie” S-K-33 (bez wariantu `Przewietrz teraz.`), „Jutro rano” S-K-34, „Wymiana sprzętu” S-K-35, „Pyłek w dzień” S-K-36 (rodzaje pyłków dalej tylko w pliku, `pylki=`; bez nich ekranu nie ma); R-KON-46 (po 16:00 S-K-34 przed S-K-33) działa dalej, gdy S-K-33 stoi wyżej;
   - z przełącznikiem, bez kolejności: „Dzień i noc” S-K-37 (koniec listy, D5) i „Obrazek dnia” S-K-38 (pierwszy blok slotu i zastępstwo S-K-37, R-KON-50, R-KON-54); bez obu zostaje ekran z rana, jak dziś bez współrzędnych i obrazków;
   - **zawsze włączone, przed listą, bez przełącznika:** ekran z rana trzymany na cały dzień (D0: pytanie R0, brak danych S5, lekarz L, noc mało czytelna S-K-12, problem poranny R5a…R5d), „Maska przeciekała w drzemce” S-K-39 (D0a, ton „problem”) i `Przewietrz teraz.` (CO2 ≥ 1000 ppm, R-KON-44; włącza go istniejące pole „Ekrany o CO2 w sypialni”, lista do niego odsyła); linia uwagi (ostrzeżenia, bateria E-05) i stopka z baterią stoją na każdym ekranie dziennym jak dotąd;
   - ekrany wieczorne, tygodniowe, nocne i kolejność poranka bez zmian (poza zakresem decyzji 8).
4. **Plik konfiguracji:** klucze `kokpit.kolejnosc`, `kokpit.ukryte`, `dzien.kolejnosc`, `dzien.ukryte` w `/konfiguracja.txt` (5.6.4), więc kopia konfiguracji i zrzut plików obejmują je bez zmian w narzędziach. Brak wiersza = układ sprzed tej decyzji; formularz przy układzie domyślnym usuwa wiersz (R-KONF-20). Wersja formatu to nazwa klucza (jak `okres.filtr_tyg`): inne znaczenie wartości dostanie nowy klucz. Wartość uszkodzona: nazwy nieznane, powtórzone i nazwy ekranów zawsze włączonych pominięte z wpisem w logu, brakujące pozycje kolejności dochodzą na końcu w kolejności domyślnej; kokpit nigdy nie jest pusty, a ekranu zawsze włączonego nie wyłącza żaden wpis.
5. **Panel:** U-01 bez nowych wierszy (spec nie wymaga pokazania układu na panelu; widać go w kolejności stron i numerach). Strona ukryta zapisana w pamięci RTC (restart) nie jest rysowana: przy rysowaniu panel daje ekran dnia, jak przy stronie sypialni bez danych (R-KOK-29).

## Konsekwencje

- Domyślnie nic się nie zmienia: test porównuje nawigację (każdy ekran × przycisk × kontekst, 1 474 560 przypadków) i wybór ekranu dziennego (20 000 losowych wejść) z kodem sprzed zmiany (commit `e7bb4d4`, `test/baza/*.inc`): 0 różnic. Droga przez stronę Portretu w układzie domyślnym jest taka jak przed zmianą (stopka `▶ ostatnia noc`, K1 z `◀ portret nocy`, testy R-POR-24 w `test/testy_noc_pliki.cpp` bez zmian); w każdej z 6 kolejności sprawdza ją `test/testy_kolejnosc_portret.cpp`.
- Strona formularza rośnie o ok. 3,3 KB sekcji i 0,9 KB stylu i skryptu: zwykła 15 002 B (było 10 459 B), najgorszy realny przypadek 18 680 B (zapas 1 800 B); rezerwacja `REZERWACJA_STRONY` z 16 KB na 20 KB. Strona idzie do odpowiedzi bez kopii (`std::move`), więc szczyt sterty przy podawaniu strony to sama rezerwacja 20 KB (wcześniej rezerwacja i kopia: do ok. 39 KB). Ciało POST całej strony w realnym najgorszym przypadku ma 3 984 B, więc limit `MAKS_CIALA` rośnie z 4 KB na 8 KB. Pomiar sterty ESP32 w chwili podawania strony: do zrobienia na panelu.
- Przy innej kolejności teksty z numerami stron mówią numer bieżący (`raport 1 / 3`, gdy „7 nocy” stoi pierwsza); `numeryStronWTekscie()` nie jest idempotentna przy zamienionych K1 i K2, więc woła się ją raz na tekście z zestawu albo z kodu (pytanie otwarte bez zmian dla tekstów przechowanych).
- Ekran dnia ▶ i strona Portretu ▶ otwierają pierwszą stronę ciągu (domyślnie K1); podpis `▶ kokpit` na ekranie dnia zostaje, a stopka Portretu nazywa tę stronę.
- Układ kokpitu z pliku ustawia się na początku obsługi każdego naciśnięcia (przed silnikiem reguł), więc teksty z numerem strony liczą się z bieżącego układu także po zapisie formularza w tym samym wybudzeniu i przy przycisku w czuwaniu wybudzenia pomiarowego. Wartość uszkodzona daje jeden wpis w logu na klucz z liczbą pominiętych nazw i najwyżej 3 przykładami; czytane jest najwyżej 160 B wartości.
- Wyłączenia, których ta decyzja nie daje: K1 albo K2 z przełącznikiem (właściciel projektu potwierdził 14.11.2026 23:10, że zostaje tak: przestawne, bez przełącznika; pytanie otwarte), kolejność ekranów wieczornych i rotacji poranka (do decyzji z kosztem).

**Zmiana 2026-11-14 (po przeglądzie zmiany):** Portret ▶ otwiera pierwszą stronę ciągu zamiast zawsze K1, z powrotem ◀ z niej na Portret (pkt 2); układ na początku obsługi przycisku; log układu jeden na klucz; strona formularza bez kopii, `MAKS_CIALA` 8 KB; teksty sekcji „Ekrany” prostym językiem, nagłówek `3 strony` albo `do 4 stron`.


**Zmiana 2026-11-16 ([ADR-0027](0027-ekran-dzienny.md) „Zmiana 2026-11-16”, R-KON-73…R-KON-77):** obrazek dnia nie ma już bloku od startu slotu; ma dwie role: zastępstwo S-K-37 (jak dotąd) i wygaszacz ekranu po progu bezczynności. `obrazek` w `dzien.ukryte` (pole „Obrazek dnia”) dalej ukrywa obrazek w obu rolach. Dochodzą dwa klucze poza układem: `dzien.wygaszacz=tak|nie` (pole „Wygaszacz ekranu: włączony / wyłączony”, wyłącza tylko rolę wygaszacza) i `dzien.wygaszacz_min` (czas bezczynności), oba w sekcji „Ekrany” pod polem „Obrazek dnia”. Przestawianie kolejności dotyczy dalej tylko D1…D4; obrazek nie ma miejsca na liście (wygaszacz to reguła czasu, a zastępstwo to reguła braku S-K-37), więc formularz bez zmian w tej części. Rezerwacja strony formularza 21 KB zamiast 20 KB (scenariusz „wszystkie pola z błędem” miał 24 B zapasu); razem z limitem ładowania (ADR-0046, pole `ladowanie_limit`) 23 KB, scenariusz 21 749 B.

## Odrzucone warianty

- **Przełącznik przy każdej stronie z numerem (także K1 i K2):** wyłączenie K1 usuwa z kokpitu jedyne `ocena częściowa` (ADR-0029 pkt 7) i drogę do „Co było inaczej?” po terminie, wyłączenie K2 usuwa wyniki „tylko raport” poza raportem i raport ogólny na żądanie. Do decyzji z kosztem przeniesienia tych informacji.
- **„Otwarte sprawy” i „Co znaczą te liczby” jako osobne pozycje do przestawiania:** rozdzielają objaśnienie i sprawy od liczb, których dotyczą; „Otwarte sprawy” nie mogą być wyłączone.
- **Strzałki jako przyciski wysyłające formularz bez JavaScriptu:** każde przesunięcie zapisuje też inne niezapisane pola i przeładowuje stronę; pole „miejsce” robi to samo bez tych skutków.
- **Przestawianie S-K-37 i S-K-38 na liście:** S-K-37 jest kandydatem prawie zawsze, więc postawiony wyżej zasłaniałby resztę (to samo co wyłączenie reszty); obrazek ma regułę czasu (od 16.11.2026 wygaszacz, R-KON-73) i regułę zastępstwa (R-KON-54), nie miejsce na liście.
