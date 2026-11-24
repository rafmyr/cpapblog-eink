# ADR-0027: Slot dzienny, okno na wietrzenie i obrazek dnia

| | |
|---|---|
| Data | 03.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 03.11.2026); w kodzie wersji beta od 03.11.2026 |
| Źródło | spec 1.5 („Slot”), 5.8.2, 5.8.3, 5.8.4, 5.8.12, 5.9.3, 6.2, 7.2 (część „Dzień”), R-KON-41…R-KON-55, R-NF-02; S-K-33…S-K-38 |

## Kontekst

Ekran z rana (blok liczb ostatniej nocy, pasek 7 nocy) stał dotąd od importu nocy do slotu wieczornego, czyli przez cały dzień. Ocena autora projektu (03.11.2026): w dzień ta informacja „niewiele wnosi”, a duży czas snu na ekranie przy łóżku widzą też domownicy. Wyniki nocy mają zostać w stopce, a pole główne ma w dzień mówić coś praktycznego, najlepiej z efektem „wow”.

Rozważone treści pola głównego: pogoda, wiadomości, odczyty czujników, zegar, kalendarz, imieniny, wschód i zachód słońca, przypomnienia sprzętu, pyłki, okno na wietrzenie, obrazki.

Warunki autora projektu:
1. ekran dzienny godzinę po zakończeniu terapii albo po naciśnięciu dowolnego przycisku;
2. projekt niezależny od Mac mini: panel sam pobiera i liczy dane (spójne z ADR-0001; lokalne źródło otoczenia z ADR-0019 zostaje opcją);
3. w regule wietrzenia poza CO2 i smogiem liczy się temperatura: zimą wietrzymy, gdy na dworze najcieplej, latem, gdy na dworze chłodniej niż w domu;
4. gdy nie ma powodu do wietrzenia, śmieszne obrazki (dobiera je sam użytkownik);
5. w stopce wyniki nocy, z czasem snu, jeśli się mieści; bez imienin.

## Decyzja

1. **Nowy slot „dzień”.** Zaczyna się 60 min [PRÓG] po późniejszej z dwóch chwil: koniec terapii ostatniej nocy albo import nocy. Wcześniej zaczyna się po pierwszym naciśnięciu dowolnego przycisku na ekranie dnia slotu porannego. Trwa do początku slotu wieczornego (R-KON-41). Ekran z rana zostaje w dzień tylko wtedy, gdy jest nim S5, ekran lekarski L, problem poranny (R4, R5a–R5d) albo pytanie R0 bez odpowiedzi (D0 w 7.2); spokój, kamienie milowe, odkrycia i podsumowanie miesiąca ustępują ekranowi dziennemu.
2. **Kolejność w slocie dziennym** (7.2, część „Dzień”): S-K-33 „Okno na wietrzenie” > S-K-34 „Jutro rano” (od 16:00 [PRÓG] przed S-K-33, gdy okno nie trwa i nie zaczyna się w ciągu godziny) > S-K-35 „Wymiana sprzętu” > S-K-36 „Pyłek w dzień” (ustawienie, domyślnie wyłączone) > S-K-37 „Dzień i noc” (słońce); od 05.11.2026 w pierwszym bloku slotu przed całą kolejnością stoi S-K-38 „Obrazek dnia” (dopisek „Zmiana 05.11.2026”).
3. **Reguła wietrzenia liczy panel** z godzinowej prognozy temperatury i PM2.5 z serwisu pogody (M-26, M-27; ta sama droga co R-DANE-16, bez Mac mini), temperatury przy łóżku z czujnika panelu (M-22) i, gdy jest, CO2 sypialni (M-23). Tryb wynika z różnicy temperatur, nie z kalendarza (5.8.12). CO2 tylko podnosi pilność (`Przewietrz teraz.`); bez CO2 reguła działa na samym smogu i temperaturze.
4. **Obrazek dnia (S-K-38) to czwarty rodzaj ekranu** obok ekranu danych, karty i ekranu nocnego S9: obraz 1-bit z karty microSD na całą szerokość nad stopką, bez tytułu, bez oceny i bez budzika. Wyjątek od ADR-0007 („dwa tryby bez mieszania”): obrazek nie jest kartą, nie liczy się do limitów kart (7.4) i niczego nie komunikuje. Obrazki leżą na karcie w katalogu `/obrazki/`, poza repozytorium; dobiera je i przygotowuje użytkownik panelu.
5. **Stopka dzienna** na każdym ekranie slotu dziennego: lewo `noc {czas} · AHI {a} · wyciek {w}`, prawo `za oknem {t}° · przy łóżku {t}°` (R-KON-51). Nagłówek bez liczb nocy.
6. **Przypomnienie sprzętu w polu głównym.** S-K-35 pokazuje zbliżający się termin wymiany (7 dni [PRÓG] przed terminem, nie częściej niż co 2 dni [PRÓG]). To zmienia zasadę z 5.9.3, że sam upływ czasu nie daje ekranu w polu głównym: w slocie dziennym daje, w tonie „spokój”, bez negatywu; rano i wieczorem bez zmian.
7. **Bez nowych granic.** Ekrany dzienne nie oceniają nocy ani zdrowia i nie mówią nic o ustawieniach aparatu ani nawilżacza aparatu (ADR-0002). S-K-33…S-K-37 to ekrany danych w tonie „spokój”, bez budzika (ADR-0007, ADR-0009). Linia uwagi stoi tylko na ekranie danych, więc w dniu z linią uwagi zamiast S-K-38 jest S-K-37.

## Uzupełnienie 03.11.2026 wieczorem (decyzje: wynik drzemki z maską, wybór rodzajów, wskaźniki w drzemce)

8. **Drzemka z maską.** W trakcie drzemki (`rop` = 1 przy ocenie o pełnej godzinie) ekran stoi bez odświeżeń i bez buzzera (R-KON-56). Po drzemce panel rozpoznaje nową sesję po `edf_prefix`, pobiera raport doby bez zapisu rekordu nocy i dopisuje `drzemka {czas}` do stopki (R-KON-57). Wskaźników drzemki (AHI, ciśnienie, oddechy) panel nie pokazuje: z godziny snu są przypadkowe, a terapię ocenia noc. Wyjątek: wyciek powyżej progu S-K-10 daje S-K-39 „Drzemka: maska przeciekała.” (D0a w 7.2, R-KON-58), bo wymaga czynności przed nocą. Odrzucone: osobny ekran „Drzemka 1 h z maską” (duży ekran na jedną liczbę, zasłania okno wietrzenia) i ekran rysowany na start drzemki (AirBridge nie powiadamia o założeniu maski, więc ekran rysowałby się w środku drzemki). Odczyt z AirBridge: `edf_prefix` = początek ostatniej sesji, raport doby z drzemką gotowy kilka minut po zdjęciu maski, `partial` = false.
9. **Pyłki: wybór rodzajów.** Ustawienie `Pyłki w dzień` to zestaw rodzajów (brzoza, trawy, olcha, bylica), domyślnie pusty; S-K-36 i wyłączenie godzin z okna wietrzenia dotyczą tylko zaznaczonych rodzajów (R-KON-45).
10. **Obrazek dnia** ma rozmiar do 800×434 (np. kwadrat 430×430), wyśrodkowany na białym tle; selekcję i przygotowanie obrazków prowadzi osobny wątek.

## Uzupełnienie 03.11.2026 (20:04, decyzja: lista pełna obrazków, dobór kontekstowy tam, gdzie się da, „dopisz do specyfikacji”)

11. **Dobór obrazka dnia.** Rotacja S-K-37 / S-K-38 bez zmian (R-KON-50); który obrazek, rozstrzygają R-KON-59…R-KON-62. Obrazek wybierany wyłącznie z kalendarza i pogody, nigdy z danych terapii, więc nie komentuje nocy (ADR-0002, zasady humoru). Manifest `/obrazki/obrazki.json` z tagami `dni`, `pogoda`, `sezon`, `inne` (5.8.12); pula kontekstowa i neutralna na przemian, bez powtórzeń w puli (dziennik `/obrazki.csv`), wybór raz na dzień. Naprzemienność to zmiana względem polecenia („gdy pula kontekstowa nie jest pusta, losuje z niej”): w zestawie przykładowym w każdy dzień tygodnia pasuje kilka obrazków z tagiem `dni`, więc 21 z 47 obrazków bez tagów nie pokazałoby się nigdy. Lista pełna, bez filtra wulgaryzmów (decyzja; pole `wulgarne` w manifeście tylko informacyjne). Obrazki to cudzy utwór: w spec i dokumentacji publicznej bez autora, tytułów i plików, jako „własne obrazki użytkownika panelu”; pliki i manifest poza repozytorium (`.gitignore`).

## Granice bez zmian

- **ADR-0001.** Panel pyta serwis pogody bezpośrednio, jak w R-DANE-16 (zapytanie bez danych terapii; wyjątek z ADR-0013 bez zmian). Przy skonfigurowanym lokalnym źródle otoczenia prognozę godzinową panel bierze z serwisu, bo kontrakt wersji 1 jej nie ma (ADR-0019, adnotacja 31.10.2026, pkt 4: z serwisu tylko pola, których kontrakt nie ma).
- **ADR-0002.** Teksty mówią o oknie, pogodzie, słońcu i sprzęcie. Żadnego „podnieś nawilżanie”, „zmień temperaturę rury” ani innej wartości ustawienia aparatu.
- **ADR-0018.** S9 o 23:30 bez zmian; slot dzienny kończy się najpóźniej z początkiem slotu wieczornego.

## Warianty odrzucone

| Wariant | Dlaczego nie |
|---|---|
| pogoda jako główna informacja | dubluje telefon i pulpit E1002; jest wejściem reguły wietrzenia i S-K-34, nie osobnym ekranem |
| wiadomości | e-ink odświeżany kilka razy na dobę pokazuje rzeczy nieaktualne; dodatkowy ruch z panelu do internetu bez związku z terapią |
| same odczyty czujników | liczba bez decyzji, ten sam problem co dotychczasowy ekran w dzień |
| zegar | pełne odświeżanie co minutę na baterii |
| kalendarz spotkań | konto i logowanie w internecie; prywatne sprawy na widoku dla domowników |
| imieniny | decyzja (03.11.2026) |
| prognoza ze źródła na Mac mini (kontrakt wersji 2) | projekt ma być niezależny od dodatkowych urządzeń w sieci domowej |
| obrazek jako karta z budzikiem | budzik nic nie komunikuje i ma limit kart; obrazek ma stać w każdy spokojny dzień, więc nie mieści się w limitach 7.4 |

## Konsekwencje

- Nowe w spec: 5.8.12, S-K-33…S-K-39 (5.8.3, 5.8.4, 6.2), R-KON-41…R-KON-62, część „Dzień” tabeli 7.2, wiersz „Dzień” w 5.8.2, ustawienie `Pyłki w dzień` (5.6.4). Zmienione: słownik 1.5 („Slot”), 5.9.3, R-NF-02 (dwa nowe powody wybudzenia: R-KON-41, R-KON-42), M-26 i M-27 (prognoza godzinowa).
- Otwarte kwestie: prognoza godzinowa Open-Meteo i trafność PM2.5, energia slotu dziennego, kalibracja progów reguły wietrzenia, obrazki (format, przygotowanie, prawa), współrzędne do S-K-37, doba z drzemką rano, udziały i progi doboru obrazka.
- Energia: wybudzenie co 60 min [PRÓG] w slocie dziennym z krótkim odczytem stanu aparatu przez Wi-Fi (drzemka), pełne odświeżenie tylko przy zmianie treści, najwyżej jedno dodatkowe pobranie prognozy na dobę (R-KON-52). Bilans w spec 8.1 do potwierdzenia pomiarem.
- Przy publikacji kodu jako open source katalog `/obrazki/` nie trafia do repozytorium; panel bez obrazków pokazuje zawsze S-K-37.

## Zmiana 04.11.2026 (bez zmiany decyzji)

Pkt 1 doprecyzowany w kodzie (R-KON-41): slot dzienny i przerysowania R-EKR-60 kończą się na początku okna slotu wieczornego z mediany M-18, nie stale o 21:00; przy pustym oknie wieczornym do 23:30. Ekrany slotu dziennego nie liczą się do limitu 3 S-K na dobę. Pkt 4: w wersji beta obrazki leżą w `/obrazki/` w pamięci panelu, nie na karcie microSD (karta będzie czytana w późniejszej wersji); drogę wgrania obrazków na panel opisuje README prototypu, sekcja „Obrazki dnia na panel”.

## Zmiana 05.11.2026 (decyzja po obejrzeniu wersji beta): obrazek dnia codziennie od rana

Rotacja S-K-37 / S-K-38 według numeru dnia (pkt 2 i 11) zastąpiona regułą: w pierwszym bloku slotu dziennego każdego dnia, od startu slotu przez 120 min [PRÓG], stoi obrazek dnia S-K-38 przed zwykłymi S-K-33, S-K-34, S-K-35, S-K-36 i S-K-37. Powód: przy rotacji obrazek wychodził co drugi dzień, a okno wietrzenia (S-K-33) zajmowało prawie cały dzień, więc obrazek był widoczny dopiero ok. 18:00–21:00 (symulator: 23 obrazki z 1368 ekranów, wietrzenie 679). Pierwszeństwo przed obrazkiem zachowują S-K-39 (D0a), `Przewietrz teraz.` (R-KON-44), ekran z rana trzymany przez D0 (problem, S5, wzorzec L) i linia uwagi (R-KON-54). Po bloku obowiązuje dotychczasowa kolejność D1…D5, a w D5 stoi słońce S-K-37 (bez współrzędnych miasta zastępstwo S-K-38). Dobór obrazka i „raz na dzień” (R-KON-59…R-KON-62) bez zmian. Budżet odświeżeń bez zmian: jedna zmiana ekranu na końcu bloku (R-KON-42). Granice (ADR-0001, ADR-0002) bez zmian.

**Zmiana 2026-11-11 (ADR-0040; propozycja domyślna do decyzji właściciela projektu):** w poranku z Portretem nocy naciśnięcie na ekranie dnia zaczyna slot dzienny dopiero na drugim ekranie sekwencji poranka albo na Portrecie po 30 min (R-POR-05, R-KON-41); do tej chwili ↻ wraca do bieżącego ekranu poranka. Bez Portretu (AirBridge < 2.3.1) bez zmian.

**Zmiana 2026-11-14 ([ADR-0043](0043-belka-informacyjna.md)):** ekrany slotu dziennego i obrazek dnia nie dostają belki informacyjnej; nagłówek S-K-33…S-K-39 zostaje (data, `prognoza {hh:mm}` albo `drzemka …`, linia na y 52). Stopka dzienna (pkt 5): przy baterii panelu poniżej 20% `[PRÓG]` w miejscu wyniku nocy stoi ikona baterii i `{x}% · podłącz kabel`, do powrotu do 25% `[PRÓG]` albo do podłączenia kabla (R-KON-69); podpisy przycisków na S-K-35 zostają.

**Zmiana 2026-11-16 (decyzje właściciela projektu z 16.11.2026: „z ekranów z ptaszkiem zrobić »wygaszacz ekranu« i pokazywać go znacznie później; dzisiaj zamienia on ekran dnia”, „dodajmy możliwość wyłączenia wygaszacza ekranu przez telefon”, „wygaszacz można wyłączyć tylko przez stronę mobilną, domyślnie jest włączony dla każdego zasilania”): obrazek dnia jako wygaszacz ekranu.**

- Pierwszy blok obrazka (120 min od startu slotu, zmiana z 05.11.2026, R-KON-50) znika: od startu slotu obowiązuje zwykła kolejność D1…D5. R-KON-50 wycofane, zob. R-KON-73.
- Obrazek S-K-38 zostaje na pozycji D5 tak jak dotąd: zastępstwo S-K-37, gdy S-K-37 nie ma (brak współrzędnych miasta albo „Dzień i noc” wyłączony z telefonu, R-KON-54, R-KON-71). Rotacja S-K-37 / S-K-38 według numeru dnia nie wraca (niżej, wariant W3).
- Dochodzi rola wygaszacza (R-KON-73…R-KON-76): po 3 h `[PRÓG]` bez naciśnięcia przycisku (licząc od późniejszej z chwil: start slotu dziennego, ostatnie naciśnięcie) obrazek zasłania spokojny ekran dnia: S-K-37 na D5 i ekran z rana, gdy slot nic nie wybrał. Wejście przy najbliższej zwykłej ocenie slotu o pełnej godzinie (R-KON-42), bez dodatkowych wybudzeń. Ekrany, które każą coś zrobić albo ostrzegają, zostają na wierzchu; gdy taki ekran pojawi się w trakcie wygaszacza, wraca na wierzch przy najbliższej ocenie. Dokładna lista: niżej, wariant W3.
- Przycisk na wygaszaczu działa jak na każdym ekranie slotu dziennego: ▶ pierwsza strona kokpitu, ↻ ekran dnia; nie ma kroku „pierwsze naciśnięcie tylko zdejmuje wygaszacz”. Każde naciśnięcie zeruje licznik bezczynności.
- Czas bezczynności ustawiany z telefonu: klucz `dzien.wygaszacz_min` (30–720 min, domyślnie 180, R-KON-74). Wyłącznik samego wygaszacza: klucz `dzien.wygaszacz=nie`, pole „Wygaszacz ekranu: włączony / wyłączony” w formularzu na telefonie; to jedyne miejsce wyłączenia (bez przycisków i bez U-01), domyślnie włączony, działa tak samo na baterii i na kablu (R-KON-77). `obrazek` w `dzien.ukryte` (pole „Obrazek dnia”, R-KON-71, ADR-0045) dalej ukrywa obrazek w obu rolach.
- Ten sam obrazek danego dnia w obu rolach (R-KON-61: wybór raz na dzień, jeden wpis w dzienniku). Dobór (R-KON-59…R-KON-62), budowa ekranu S-K-38 (własny rodzaj ekranu, bez belki, stopka dzienna) i granice (ADR-0001, ADR-0002, ADR-0007, ADR-0009) bez zmian.

**Zmiana 2026-11-16, wariant W3 (decyzja właściciela projektu 16.11.2026 22:08; zastępuje część punktów wyżej):**

- **Co wygaszacz zasłania.** Oprócz S-K-37 i spokojnego ekranu z rana zasłania ekrany czysto informacyjne: `Wietrz od {hh}:00`, dopóki okno wietrzenia się nie zaczęło, `Dziś bez wietrzenia.`, S-K-34 „Jutro rano”, S-K-36 pyłek. Na wierzchu zostają: `Przewietrz teraz.`, trwające okno `Wietrz do …`, upał `Okna zamknięte …` (krok „zasłoń okna” teraz), S-K-35 (akcja ◀ 2 s), S-K-39, ekran z rana trzymany przez D0, linia uwagi, S5, S10 i ekrany spoza zwykłej stopki, ekran otwarty przyciskiem i informacja po aktualizacji samodzielnej (R-KONF-61). Gdy okno się zaczyna, `Wietrz do …` wraca na wierzch przy najbliższej ocenie (R-KON-75). Powód: zimą S-K-33 stoi często przez większość dnia, więc wygaszacz zasłaniający tylko S-K-37 pokazywałby obrazek głównie wieczorem.
- **Niska bateria nie blokuje wygaszacza.** Obrazek rysuje tę samą stopkę z ikoną baterii i `{x}% · podłącz kabel` (R-KON-69 bez zmian), a brak wygaszacza kosztował ok. 9 odświeżeń e-ink na dobę więcej przy słabej baterii (doba testowa w symulatorze: 17 zamiast 8). E-05 jako linia uwagi albo ekran S5 nadal wygrywa z wygaszaczem.
- **Naprzemienność S-K-37 / S-K-38 według numeru dnia nie wraca.** Zastępstwo D5, gdy S-K-37 nie ma, zostaje.
- **Informacja po aktualizacji i ikona nowej wersji.** Wygaszacz nie wchodzi, dopóki czeka informacja po aktualizacji samodzielnej; na obrazku w roli wygaszacza ikona nowej wersji zostaje w stopce (R-EKR-61; obrazek jako zastępstwo D5 bez ikony jak dotąd), bo wygaszacz nie może ukrywać informacji o nowej wersji na resztę dnia.
- **Liczby (symulator, 30 dni pogody z listopada, bez naciśnięć):** stan sprzed zmiany (blok 120 min) obrazek 30 z 30 dni, mediana 2,0 h na dobę, 17 odświeżeń e-ink; wygaszacz bez zasłaniania S-K-33…S-K-36: 30 z 30, mediana 2,0 h, 16 odświeżeń; W3: 30 z 30, mediana 5,0 h (średnio 6,1 h), 15 odświeżeń.

**Zmiana 2026-11-16, doprecyzowanie wariantu W3:**

- **Zasłanianie dosłowne.** Gdy po progu bezczynności ekran, który wygrałby bez wygaszacza, jest na liście „zasłania”, wynikiem jest wygaszacz S-K-38, bez sięgania po dalszych kandydatów D1…D4. Inaczej zasłonięty ekran wypadałby z kolejki i wygrywałby następny niezasłonięty, czyli ekran, którego bez wygaszacza w tej chwili by nie było (np. S-K-34 → S-K-35, co zużywałoby licznik `b9_sk35` i blokowało S-K-35 następnego dnia; S-K-34 po 16:00 → upał). W tabeli testów brzegowych (120 000 losowych wejść) dotyczyło to 9 872 przypadków; teraz w każdym z nich S-K-38. Własność: wynik z wygaszaczem to ten sam ekran co bez wygaszacza albo S-K-38, nigdy trzeci; liczniki i dzienniki (`b9_sk35`, limity 7.4, `/obrazki.csv`) nie zmieniają się przez ekrany, których nie pokazano.
- **Upał na wierzchu.** `Okna zamknięte do …` i `Okna zamknięte cały dzień.` zostają na wierzchu: to polecenie do wykonania teraz (trzymać okna zamknięte), zgodne z zasadą „ekrany, które każą coś zrobić”.
- **Po końcu okna wietrzenia wygaszacz wraca od razu, bez nowego progu.** Bezczynność liczy się od ostatniego naciśnięcia (albo startu slotu), a naciśnięcia nie było; ekran, który wyszedł na wierzch w trakcie wygaszacza, licznika nie zeruje (R-KON-73, R-KON-75).
- **Zimowe okno przesuwane bez zmian.** Reguła S-K-33 (5.8.10) w zimowe dni przy czystym powietrzu przesuwa koniec okna co godzinę, więc `Wietrz do …` stoi do wieczora i wygaszacza prawie nie ma. To cecha reguły sprzed wygaszacza; ewentualna zmiana to osobna decyzja.

**Zmiana 2026-11-22 ([ADR-0050](0050-opcje-komfortu-w-poradach.md), decyzja właściciela projektu 22.11.2026):** pkt 7 i „Granice bez zmian” (ADR-0002: „żadnego »podnieś nawilżanie«, »zmień temperaturę rury«”) obowiązują dalej dla S-K-33…S-K-39 i obrazka dnia. Wyjątek ma tylko porada programu „Dobry start” (S-K-48, ADR-0049): może podać kierunek dla nawilżania, temperatury podgrzewanej rury i rampy bez liczb i bez drogi w menu i odesłać do instrukcji aparatu.

**Zmiana 2026-11-22 ([ADR-0049](0049-dobry-start.md), wdrożenie):** pkt 7: w slocie dziennym może stać karta z budzikiem (7 kart S-K-48). Nowa pozycja D0b: przy włączonym programie S-K-48 stoi pierwsze 180 min `[PRÓG]` slotu dziennego po D0, D0a i ekranach z działaniem teraz (`Przewietrz teraz.`, `Wietrz do …`, `Okna zamknięte …`, S-K-35), a wieczorem W3b po W0…W3. Lista wygaszacza (R-KON-75, zmiana 2026-11-16): wygaszacz nie zasłania S-K-48 w bloku ani wieczorem. Program wyłączony: slot dzienny bez zmian (R-DS-01). Spec 7.2, 5.8.14.4.

**Zmiana 2026-11-24 (pkt 8, R-DANE-69, R-KON-57):** drzemką nie jest sesja wliczona do nocy: gdy po imporcie nocy rano terapia wróciła, a panel po jej końcu pobrał noc ponownie (R-DANE-69 przed 11:00 albo R-DANE-24 o 11:00, przy statusie bez terapii), sesje zaczęte przed tą chwilą są w rekordzie nocy i nie trafiają do stopki jako drzemka (NVS `cpap`/`ds_ep`). Sesja, która w chwili pobrania trwała (np. 10:30–12:30), zostaje drzemką jak dotąd; drzemka liczy sesje od tej chwili, nie od samego importu. Sesja zaczęta przed 11:00 i trwająca po 11:00 nie może przepaść: albo wchodzi do nocy, albo zostaje drzemką.
