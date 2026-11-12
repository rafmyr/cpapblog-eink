# Symulator doby: na Macu, bez panelu

Symulator uruchamia na Macu ten sam program, który działa w panelu, na kopii danych z panelu. Podajesz zrzut plików, okres i (jeśli chcesz) naciśnięcia przycisków. Dostajesz obrazki każdego ekranu, który panel by narysował, i log: o której panel się obudził, dlaczego, co narysował i który przycisk został naciśnięty.

Zrzut i wyniki to dane zdrowotne. Muszą leżeć poza repozytorium; symulator odmawia, gdy któryś z katalogów leży w tym albo w innym katalogu roboczym cpap-eink (także w innych katalogach roboczych tego projektu).

## Co jest potrzebne

- Mac z narzędziami programisty Apple (kompilator). Wystarczy zwykły `c++` (Apple clang) z narzędzi Apple: symulator sam wybiera pierwszy działający kompilator z `CXX`, `g++`, `clang++`, `c++` (Apple clang 21 z narzędzi Apple kompiluje i uruchamia symulator ze scenariuszami). GCC z Homebrew nie jest potrzebny (potrzebują go tylko testy firmware `tools/prototyp/testy.sh`). Jeśli w kroku 2 pojawi się komunikat o braku kompilatora, wklej w Terminalu `xcode-select --install` i zatwierdź instalację.
- Zrzut plików z panelu, czyli katalog z plikami `noce.csv`, `czynniki.csv`, `humor.csv`, `sypialnia.csv`, `sypialnia-co2.csv`, `bateria.csv` (robi go `tools/prototyp/zrzut_plikow.py`). W komendach niżej `<katalog-zrzutu>` oznacza katalog zrzutu poza repozytorium (ścieżkę ze spacją podaj w cudzysłowie). Zrzut z nowszego firmware (katalogi `dane/` i `log/`) też działa.

## Najkrótsza droga: wszystkie nowe ekrany na zrzucie z panelu

Noce w zrzucie są zwykle w większości dobre, a część z nich pochodzi z importu karty bez części metryk, więc zwykły przebieg pokazuje tylko ekran dnia i „Dobranoc”. Ta jedna komenda przechodzi po kolei wszystkie **scenariusze** (opis niżej): dopisuje po zrzucie z panelu wymyślone noce, które wywołują ekrany „pokaż lekarzowi”, linie uwagi, „Otwarte sprawy”, pytanie o senność, ekrany braku danych i CO2, i dokłada prognozę pogody oraz Twoje obrazki dnia.

1. Terminal, katalog z repozytorium:

   ```
   cd <katalog-repozytorium>
   ```

2. Wszystkie scenariusze z własnymi obrazkami dnia (18 scenariuszy, ok. 1300 obrazków, trwa do 2 minut, pierwszy raz dochodzi kompilacja):

   ```
   python3 tools/symulator/symuluj.py --dane "<katalog-zrzutu>" --pokaz-wszystko --obrazki <katalog-obrazkow-dnia>
   ```

3. Na końcu Terminal wypisze gotową komendę `open "…/pokaz-wszystko"` z katalogiem tego przebiegu. Skopiuj ją (zaznacz cały wiersz z `open`, ⌘C), wklej w Terminalu (⌘V) i naciśnij Enter. Otworzy się Finder; zacznij od `spis.txt`.

W katalogu jest podkatalog `scenariusz-NAZWA-RRRR-MM-DD/` (data pierwszego poranka scenariusza) na każdy scenariusz (obrazki, `log.txt`, `log-panelu.txt`, własny `spis.txt`) i wspólny `spis.txt`: scenariusz → co zapowiada (`jest` albo `BRAK`) → każdy obrazek z opisem ekranu i linią uwagi. Ten sam ekran dnia przerysowany kilka razy z rzędu w jednym dniu z identyczną treścią obrazka jest jednym wierszem `pierwszy … ostatni — opis (N obrazków: ten sam ekran przerysowany, godziny)`; ekrany z przycisków (K1, K2, „Sprzęt” …) zawsze po jednym wierszu. Na końcu spisu są dwie listy: `Nie pojawiło się` (zapowiedź, której nie było: do sprawdzenia) i `Ekrany, których nie pozwala wywołać` (z przyczyną w kodzie panelu). Raport dla lekarza w PDF, który panel zapisał po naciśnięciu ▶ na ekranie „pokaż lekarzowi” albo w K2, leży w katalogu scenariusza jako `SYN-raport-….pdf` (scenariusze `regularnosc`, `autoset`, `cpap`). Każda strona ma czarny pas `DANE SYNTETYCZNE` u góry i u dołu, a pole „Pacjent” jest zasłonięte: to nie jest raport do wydruku dla lekarza. Drugie uruchomienie trafia do `pokaz-wszystko-2`, nic się nie nadpisuje. Gdy któryś scenariusz skończy się błędem, pozostałe idą dalej, a błąd jest w `spis.txt`.

**Noce wymyślone są oznaczone wszędzie:** każdy katalog scenariusza ma plik `UWAGA-NOCE-SYNTETYCZNE.txt`, każdy obrazek z tymi nocami ma przedrostek `SYN-` w nazwie (np. `SYN-050-20261203-0500-dzien-wzorzec-L-03_ge10.png`) i napis **DANE SYNTETYCZNE** w prawym górnym rogu (także K1 i K2). Napis dokłada tylko symulator; panel go nie rysuje. Obrazki bez `SYN-` (scenariusz `zwykly-dzien`) pokazują wyłącznie prawdziwe noce ze zrzutu. Pliki zrzutu zostają nietknięte: symulator pracuje na kopii w pamięci, a każdy scenariusz na osobnej kopii.

Obrazki dnia: `--obrazki` przyjmuje katalog `assets/obrazki-dzienne-prywatne` (symulator bierze z niego gotowy podkatalog `panel/`), katalog z plikami `.pbm` z `tools/obrazki/przygotuj.py` albo katalog obrazów źródłowych (`.png`, `.jpg`), które symulator sam zamienia na format panelu w katalogu tymczasowym (do tego Python musi mieć bibliotekę Pillow; systemowy `python3` na Macu jej nie ma, dlatego najprościej wskazywać katalog z `panel/`). Obrazki trafiają tylko do pamięci symulowanego panelu i do obrazków wyniku poza repozytorium; do repozytorium nic nie jest kopiowane.

## Krok po kroku: jedna doba

1. Otwórz Terminal i przejdź do repozytorium:

   ```
   cd <katalog-repozytorium>
   ```

2. Uruchom dobę 4 listopada od 5:00 do 23:59 na swoim zrzucie (zrzut kończy się nocą z poranka 4.11, więc to ten poranek pokazuje ostatnią noc):

   ```
   python3 tools/symulator/symuluj.py --dane "<katalog-zrzutu>" --od 2026-11-04T05:00 --do 2026-11-04T23:59
   ```

   Pierwsze uruchomienie kompiluje program (do minuty), kolejne startują od razu. Na końcu symulator wypisze katalog z wynikami, domyślnie `~/cpap-eink-symulator/2026-11-04_0500--2026-11-04_2359/`.

3. Otwórz wyniki w Finderze: na końcu Terminal wypisze gotową komendę `open "…"` z katalogiem tego przebiegu (np. `~/cpap-eink-symulator/2026-11-04_0500--2026-11-04_2359`); skopiuj ją, wklej w Terminalu i naciśnij Enter.

   W katalogu są obrazki `001-20261104-0500-dzien-S2A.png`, `002-…-dzien-SK33.png` itd. (numer, data, godzina, nazwa ekranu), plik `spis.txt` (każdy obrazek z opisem po polsku i linią uwagi), `log.txt` (jeden wiersz na wybudzenie) i `log-panelu.txt` (wszystko, co panel wypisał, do szukania błędów). Drugie uruchomienie na ten sam okres trafia do katalogu z końcówką `-2`, nic się nie nadpisuje.

**Który dzień wybrać.** Panel rano pokazuje noc, która się właśnie skończyła. Symulator wypisuje na początku ostatnią noc w zrzucie (`Ostatnia noc z AirBridge w zrzucie: rano …`); żeby zobaczyć poranny ekran z tą nocą, ustaw `--od` na 5:00 tego poranka.

- **Dzień po ostatniej nocy** (np. `--od 2026-11-05T05:00` przy zrzucie z 4.11): symulator nie ma nowych danych z aparatu, więc rano panel pokazuje ekran S10 „Synchronizuję się” (`…-dzien-S10.png`) i co 30 minut sprawdza, czy noc doszła. To prawdziwe zachowanie panelu, gdy AirBridge nie oddał jeszcze nocy, a nie błąd symulatora.
- **Dzień przed ostatnią nocą** (np. `--od 2026-11-01T05:00`): panel widzi w historii noce „z przyszłości” i wynik nie odpowiada żadnej prawdziwej dobie. Symulator ostrzega o tym w `log.txt` i w Terminalu (`UWAGA: zrzut ma noce późniejsze niż --od`).

## Scenariusze: wymyślone noce po zrzucie z panelu

`--scenariusz NAZWA` dopisuje po ostatniej nocy zrzutu noce syntetyczne (wymyślone, z pełnymi metrykami: OA, CA, H, wyciek, M-08, M-13, M-17, tryb, ustawienia aparatu z ostatniej nocy zrzutu). Najpierw 21 zdrowych nocy rozbiegu (norma osobista potrzebuje co najmniej 14 nocy z metryką, a noce z karty ich nie mają) w normie ze zrzutu: czas z maską, ciśnienie, EPAP, wsparcie, oddechy na minutę i M-17 to mediana z ostatnich 30 ważnych nocy zrzutu (±3%), bezdechy i wyciek najwyżej jak w zrzucie i nie wyżej niż zakres zdrowy; inaczej sam rozbieg dawałby stan odchylenia i pełny ekran S3 (np. E-13) zamiast ekranów scenariusza. potem noce scenariusza; dlatego przy zrzucie z 4.11 ekrany scenariuszy mają daty od 26 listopada. Noc z poranka D dochodzi do pamięci panelu i na listę AirBridge o 04:30 dnia D, tak jak przyszłaby z aparatu. Bez `--od` i `--do` okres ustawia scenariusz: od pierwszego poranka, w którym jego ekran powinien się pojawić; inne `--od` daje ostrzeżenie. Wszystkie noce scenariusza są oznaczone w `log.txt` (nagłówek podaje osobno ostatnią prawdziwą noc ze zrzutu i osobno wiersz `Noce syntetyczne …`, `[doszła noc syntetyczna z poranka …]` przy wybudzeniu), a naciśnięcia dołożone przez scenariusz są w `przyciski-scenariusza.txt`. Większość scenariuszy o 7:30 naciska ▶ siedem razy i ↻ (ciąg stron), czyli pokazuje K1, K2 i do pięciu kart „Otwarte sprawy” (przy mniejszej liczbie kart dalej S8 i „Sprzęt”).

| Scenariusz | Co ma się pojawić i kiedy |
|---|---|
| `zwykly-dzien` | ostatni poranek ze zrzutu, bez wymyślonych nocy: ekran dnia, od godziny po imporcie nocy slot dzienny z prognozą (S-K-33 okno na wietrzenie, S-K-34 jutro rano, S-K-37 dzień i noc), wieczorem obrazek dnia S-K-38 (z `--obrazki`), o 7:30 K1, K2 i Otwarte sprawy. |
| `wyciek` | Pierwsza noc po rozbiegu ma wyciek P95 38 L/min (E-23, wyciek od 27 L/min): od rana do wieczora pełny ekran „Wyciek w nocy ponad próg” (S3, slot dzienny go nie wypiera), o 7:30 K1, K2 i Otwarte sprawy z kartą E-23. |
| `ahi-rosnie` | Od pierwszej nocy po rozbiegu AHI 14/h: 4. i 5. poranka pełny ekran E-30 „Noce z AHI powyżej 5” (mediana z 7 nocy ≥ 5/h), potem przez 5 dni limit 2 razy w tygodniu (R-KON-14) i linia `Wciąż: Noce z AHI powyżej 5.` (rano ustępuje zaproszeniu „Co było inaczej?”), 8. poranka ekran „pokaż lekarzowi” L-03 (mediana z 14 nocy ≥ 10/h; wraca co 7 dni), 11. i 12. znowu E-30, ostatniego ranka Otwarte sprawy. |
| `centralne` | Od pierwszej nocy po rozbiegu CA 7,5/h, AHI rośnie razem z CA: pełne ekrany S3 według kolejności 7.2 (E-31, potem E-30 dwa razy, a w dniach limitu E-30 pełny ekran E-33 „Więcej bezdechów centralnych” z linią `Wciąż: Noce z AHI powyżej 5.`), od 5. poranka L-01 w trybie „tylko raport” (bez ekranu i pilności, karta w Otwartych sprawach); 7. ranka Otwarte sprawy. |
| `krotkie-noce` | Po rozbiegu noc 2,5 h (E-11): linia uwagi „Krótka noc”; potem noc 5 minut: linia `Noc {data} bez terapii.` (E-10); po drugiej takiej nocy pełny ekran „Noce bez terapii: 2” (E-10, S3). |
| `brak-danych` | AirBridge przestaje odpowiadać (Wi-Fi jest): rano S10 „Synchronizuję się”, od 11:00 S5 brak danych (E-01), następnego dnia od 11:00 S5 „Brak danych od 2 nocy”. |
| `sennosc` | W niedzielę o 17:00 pytanie o senność, scenariusz odpowiada ▶ tak o 17:05 w dwie kolejne niedziele: po pierwszej „Zapisano” i linia uwagi o senności, zaraz po drugiej ekran „pokaż lekarzowi” L-11. |
| `co2` | Atrapa lokalnego źródła otoczenia podaje CO2 sypialni 1080–1350 ppm od 17:00 (tylko w dniach tego scenariusza, poza nimi 650 ppm): o 21:00 drugiego dnia ekran „Przewietrz sypialnię” S-K-01 (pierwszego dnia panel już prosił o czynniki nocy, a prośba jest jedna na dzień). |
| `zmiana-trybu` | Trzy noce w trybie AutoSet zamiast trybu ze zrzutu: K1 i K2 z wierszami według trybu (ciśnienie zamiast EPAP i PS, norma od nowa: „uczę się”), pierwszego i trzeciego ranka o 7:30. |
| `nawigacja` | (nowa nawigacja): trzeciego ranka po rozbiegu ▶ przez wszystkie strony kokpitu `1 / 3` … `3 / 3` (ADR-0041, bez S8 i „Sprzętu”), ◀ z powrotem do ekranu dnia, ↻ z U-01 prosto do ekranu dnia, ▶ 2 s w K2 raport ogólny; w `spis.txt` przy każdym obrazku przycisk, który go dał. |
| `sprzet` | Maska ponad 6 miesięcy od wymiany (daty w `/konfiguracja.txt` kopii zrzutu), 14 nocy z rosnącym wyciekiem: ekran „Maska się zużywa” (S-K-31), L-09 wstrzymane do 4. pokazania S-K-31; na S-K-31 ◀ 2 s „wymienione dziś” z ekranem „Wymiana zapisana.” i ◀ 2 s cofnij, potem ▶ po stronach kokpitu do U-01 `3 / 3`; po spadku wycieku pytanie „Wymieniona maska?” (▶ tak); dzień później kokpit do U-01. Od ADR-0041 bez strony „Sprzęt”: daty wymiany i „nowy model maski” są w formularzu U-03 (symulator go nie wypełnia) |
| `wymiana-filtra` | Trzy zdrowe noce, filtr z terminem wymiany drugiego dnia: pierwszego dnia w slocie dziennym S-K-35 `Wymiana: filtr.` ze stopką jak S-K-31 (`◀ 2 s: wymienione dziś` · `▶ kokpit`), ◀ 2 s (próby od 7:00 do 18:30) zapisuje wymianę przez `Wymiana zapisana.` (R-MAS-18, ADR-0041) |
| `sprzet-zuzycie` | Maska po terminie, 26 nocy z wyciekiem rosnącym bez przerwy: S-K-31 raz na 7 dni (3 razy), L-09 wstrzymane, potem pokazane, gdy norma dogoni wyciek. 4. pokazania S-K-31 (R-MAS-10) te dane nie dają: opis w `spis.txt` w „Ekrany, których nie pozwala wywołać”. |
| `infekcja` | 20 nocy z oddechem o 4/min szybszym i wyższym ciśnieniem (7. noc także z wyciekiem P95 25,5 L/min: w okresie bez S-K-10 i bez linii E-20): drugiego ranka „Oddychasz szybciej niż zwykle” z pytaniem „Katar albo infekcja?” (S-K-30), ▶ tak otwiera okres infekcji (zdarzenia wycieku „nie oceniam”, nie „w porządku”, linia `Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij pomoc.`, od 3. nocy pytanie „gorzej”), K1, K2 i Otwarte sprawy w okresie; po 14 nocach okres się kończy i przez 14 nocy karencji S-K-30 nie wraca, a log panelu ma wiersz `infekcja: karencja do {data}` (zapowiedź go sprawdza). S-K-17 pokazuje się także w okresie (R-INF-02, 7.3; do decyzji). |
| `wyjazd` | Dwa wyjazdy z aparatem: AirBridge milczy ponad 2 doby przy działającym Wi-Fi (S10, S5; S10 o 5:00 w dniu przed wyjazdem i po powrocie to skutek nocy dołożonej przez symulator o 5:20, na panelu go nie ma), po powrocie noce wyjazdu dochodzą z czynnikiem „spanie poza domem” i rano pytanie o ten okres (R-CZY-22). Pierwszy raz ◀ nie, ◀ 2 s na „Zapisano” cofa odpowiedź (pytanie wraca), znowu ◀ nie; drugi raz ▶ tak. |
| `wyciek-maski` | 16 nocy z wyciekiem P95 25 L/min i dużym wyciekiem, sprzęt bez dat wymiany: ekran „Wyciek nie ustępuje” (L-09, problem), ▶ K1, ↻ 2 s „wiem” z cofnięciem (◀ 2 s) i znowu „wiem”, następnego ranka K1, K2 i Otwarte sprawy z L-09. |
| `regularnosc` | 18 nocy po 2 h 50 min: ekran „pokaż lekarzowi” L-08 (terapia rzadziej niż 4 h), ▶ 2 s „Raport dla lekarza” (ściąga z kodem QR, PDF A4 w katalogu scenariusza), ◀ wróć, ◀ 2 s „Omówione z lekarzem?” i ▶ tak, potem K1, K2 i Otwarte sprawy. |
| `odkrycia` | 59 nocy z odpowiedziami „Co było inaczej?” (wymyślonymi: 14 po alkoholu z AHI 4,9/h, 45 bez z AHI 1,8/h), potem 11 dni z AHI poniżej najniższego tygodnia historii ze zrzutu (najwyżej 0,3/h): odkrycie O-12 „Po alkoholu: wyższe AHI” z przeglądu tygodnia, O-03 „rekordowy tydzień”, 1. dnia miesiąca podsumowanie S7; co rano o 5:10 ▶ K1 (z odkryciem) i o 7:30 ▶ po stronach do U-01 `3 / 3` („Co znaczą te liczby” jest tylko w profilu publicznym, R-KOK-22, pokazuje go scenariusz `profil-publiczny`). |
| `profil-publiczny` | Profil publiczny (`profil=publiczny` w `/konfiguracja.txt` scenariusza, 5.6.4): 7 nocy z AHI 13/h, od 4. ranka pełny ekran E-30 `Noce z AHI powyżej 5` (teksty ekranów dnia jak w profilu prywatnym: AHI i PM2.5 w obu profilach, decyzje 04.11.2026, 5.4.14); ostatniego ranka o 7:30 ▶ po stronach do „Twoja norma” (S8, stopka `▶ co znaczą liczby`), ▶ „Co znaczą te liczby” (R-EKR-51), ◀ S8, ↻ ekran dnia. |
| `przerywniki` | zdrowe noce po rozbiegu (AHI 1,2, 7 h 30), od 28 nocy do najbliższej okazji z kartą pochwalną (23.09, 25.12, 1.01; w dniu okazji scena jest wymuszona datą, więc karta z warunkiem jest zawsze; scenariusz odpowiada ◀ nie na niedzielne pytanie o senność): co 7 dni w dniu dowodu spokoju przerywnik S2d bez etykiety nad tytułem (decyzja 05.11.2026); w spisie „przerywnik S2d G.. (warunkowa)”, gdy noc spełnia warunek wylosowanej sceny, albo „(neutralna)”. Wyciek ponad 24 L/min w scenariuszu nie występuje: ekran problemu ma pierwszeństwo przed kartą (R-HUM-29) |
| `autoset` | Rozbieg i noce w trybie AutoSet (aparat AirSense 10 AutoSet w atrapie AirBridge): 7 nocy z AHI 13/h, 4. ranka pełny ekran E-30, o 7:30 K1, K2 i Otwarte sprawy z wierszami AutoSet, o 7:45 K2 i ▶ 2 s raport ogólny dla lekarza (kod QR, PDF). |
| `cpap` | To samo w trybie CPAP (stałe ciśnienie, aparat AirSense 10 Elite). |
| `nowy-ahi12` | Nowy pacjent bez rozbiegu (AutoSet, 30 nocy, AHI 12/h, wyciek 9 L/min): noce 1–6 rozruch S6, od 7. poranka E-30, od 10. L-03 „pokaż lekarzowi”. Tylko osobno (`--scenariusz nowy-ahi12`), bez `--pokaz-wszystko`; dzień po dniu sprawdza `tools/test_symulator_nowy_pacjent.py`. |
| `nowy-wyciek18` | To samo z wyciekiem P95 18 L/min (pasmo 15–24, każda noc niepewna): S-K-10, od 4. poranka linia `Noce mało czytelne: bezdechów nie oceniam.`, od 14. poranka L-09 „Wyciek nie ustępuje” do dostawcy sprzętu (ADR-0034); bez E-30, L-03 i „Noc w normie”. Tylko osobno. |
| `nowy-ahi7` | Nowy pacjent z AHI 7/h i wyciekiem 9 L/min: od 7. poranka E-30 w rytmie 2 razy w tygodniu z linią `Wciąż: …`, bez L-03 „pokaż lekarzowi” (świadomy koszt ADR-0029). Tylko osobno. |

Jeden scenariusz na zrzucie z panelu, np. ekran „pokaż lekarzowi” L-03:

```
python3 tools/symulator/symuluj.py --dane "<katalog-zrzutu>" --scenariusz ahi-rosnie
```

Ekran S1 z odkryciem stoi od 5:00 do slotu dziennego o 5:30 (później slot dzienny go zastępuje), dlatego scenariusz `odkrycia` naciska ▶ o 5:10.

Kilka scenariuszy w jednym przebiegu idzie po kolei (każdy następny od dnia po ostatniej nocy poprzedniego): `--scenariusz wyciek --scenariusz co2`. `zwykly-dzien` i `brak-danych` działają tylko same. Lista z opisami: `python3 tools/symulator/symuluj.py --dane "<katalog-zrzutu>" --lista-scenariuszy`.

## Prognoza pogody i miasto

Symulator udaje serwis pogody Open-Meteo, z którego panel bierze prognozę godzinową do slotu dziennego (S-K-33…S-K-39), i podaje panelowi współrzędne miasta (na panelu `POGODA_LAT`, `POGODA_LON` w `secrets.h`; w symulatorze bez `secrets.h`). Domyślnie Katowice i przykładowa prognoza listopadowa z pliku [`prognoza-katowice-listopad.json`](prognoza-katowice-listopad.json): rano smog, w południe czyściej i najcieplej, wieczorem znów smog, następnego ranka przymrozek i mokro. Symulator bierze z pliku 48 kolejnych godzin i podstawia je pod dzień wybudzenia i następny, więc daty w pliku nie mają znaczenia.

| Opcja | Co robi |
|---|---|
| `--miasto Gliwice` | inne miasto (Katowice, Gliwice, Tychy, Sosnowiec, Bytom, Zabrze, Chorzów, Ruda Śląska, Kraków, Warszawa, Wrocław, Poznań, Gdańsk, Łódź, Nysa) |
| `--wspolrzedne 50.2597,19.0211` | współrzędne zamiast nazwy miasta |
| `--prognoza plik.json` | własna prognoza: odpowiedź Open-Meteo `/v1/forecast` z `forecast_days=2` i `timezone=Europe/Warsaw` (jak pyta panel; pierwsza godzina musi być północą czasu polskiego), np. zapisana z przeglądarki, albo plik jak przykładowy (`{"forecast": …, "air_quality": …}`) |
| `--bez-prognozy` | panel bez miasta, jak przed bez slotu dziennego z prognozą i bez S-K-37 |

## Naciśnięcia przycisków

Utwórz zwykły plik tekstowy, np. `~/przyciski.txt`. Najprościej w Terminalu: `open -e ~/przyciski.txt` po wcześniejszym `touch ~/przyciski.txt` (otworzy się TextEdit). W TextEdit wybierz **Format → Zmień na zwykły tekst** (jeśli menu pokazuje „Zmień na tekst sformatowany”, plik już jest zwykłym tekstem), wpisz wiersze i zapisz (⌘S). Plik sformatowany (RTF) albo w kodowaniu innym niż UTF-8 symulator odrzuci z komunikatem.

W każdym wierszu godzina i przycisk:

```
# poranek: ekran dnia, potem kokpit (od ▶ dalej, ◀ wstecz, ↻ ekran dnia)
2026-11-04T07:15 prawy
2026-11-04T07:16 prawy
2026-11-04T07:17 prawy 2s
2026-11-04T07:30 prawy do:sprzet
2026-11-04T21:00 prawy
```

- przyciski: `lewy` (◀), `srodkowy` (↻), `prawy` (▶); można też wpisać `środkowy` albo same symbole;
- `2s` na końcu = przytrzymanie przez 2 sekundy;
- `do:{ekran}` na końcu = naciskaj ten przycisk co 4 s, aż panel pokaże ten ekran (najwyżej 15 razy), np. `prawy do:sprzet`; ekrany: `K1`, `K2`, `sprawy`, `norma`, `liczby`, `sprzet`, `ustawienia`, `formularz`, `test`, `raport`, `dzien`. Dalsze wiersze w ciągu 30 min przesuwają się razem z powtórzeniami, więc idą po dojściu do celu; `formularz` i `test` nie są osiągalne krótkim ▶ (U-03 i U-05 włączają Wi-Fi: `prawy 2s` na U-01, potem `prawy 2s` na U-03), więc dochodzi się do nich wierszami `prawy do:ustawienia`, `prawy 2s`, `prawy 2s`;
- można podać sekundy (`2026-11-05T07:15:10`), wtedy naciśnięcie trafia w 20-sekundowe czuwanie panelu po poprzednim naciśnięciu;
- wiersze z `#` to komentarze;
- naciśnięcie przed `--od` albo po `--do` symulator pomija i pisze o tym w `log.txt`.

Uruchomienie z przyciskami:

```
python3 tools/symulator/symuluj.py --dane "<katalog-zrzutu>" --od 2026-11-04T05:00 --do 2026-11-04T23:59 --przyciski ~/przyciski.txt
```

Co robi który przycisk na którym ekranie: tabela w [`firmware/prototyp/README.md`](../../firmware/prototyp/README.md#przyciski-mapa-562-ze-specyfikacji).

## Pozostałe opcje

| Opcja | Co robi |
|---|---|
| `--wyjscie ~/jakis-katalog` | wyniki w podanym katalogu zamiast domyślnego (poza repozytorium); gdy katalog nie jest pusty, wynik trafia do jego podkatalogu z okresem w nazwie, nigdy obok starych obrazków |
| `--bez-sieci` | panel bez Wi-Fi (jak po awarii routera) |
| `--bez-ntp` | internet zablokowany w routerze (R-AW-18): NTP milczy, a `/api/status` atrapy AirBridge podaje `esp_time` (czas lokalny symulacji, do minuty). Panel bez zegara bierze czas z AirBridge, w `log-panelu.txt` wiersz `źródło czasu: AirBridge`, i w tym samym wybudzeniu rysuje dane. Bez tej opcji NTP odpowiada zawsze |
| `--zegar-znany` | panel zna godzinę od pierwszego wybudzenia, jak panel, który już chodził przed okresem symulacji. Bez tej opcji symulator robi to, co panel po włączeniu zasilania: godzina jest nieznana, aż panel pobierze ją z internetu (NTP); z Wi-Fi dzieje się to w pierwszym wybudzeniu, z `--bez-sieci` wcale, więc panel cały okres pracuje „bez zegara” (w `log.txt` znacznik `[zegar panelu nieznany]`). `--bez-sieci --zegar-znany` pokazuje panel, któremu padło Wi-Fi, ale zegar dalej chodzi |
| `--obrazki KATALOG` | obrazki dnia S-K-38 w pamięci panelu (sekcja „Najkrótsza droga”) |
| `--profil-publiczny` | panel w profilu publicznym: symulator dopisuje `profil=publiczny` do `/konfiguracja.txt` symulowanego panelu (5.6.4), kod panelu bez zmian; ▶ w K2 otwiera „Co znaczą te liczby” (R-KOK-22), teksty AHI i PM2.5 jak w profilu prywatnym (decyzje 04.11.2026). Działa też z `--pokaz-wszystko`: wtedy każdy scenariusz idzie w profilu publicznym, a `spis.txt` to mówi w nagłówku. Bez opcji profil prywatny, jak u Ciebie |
| `--scenariusz NAZWA` | noce syntetyczne po zrzucie (sekcja „Scenariusze”); można kilka razy |
| `--pokaz-wszystko` | wszystkie scenariusze po kolei, każdy w `scenariusz-NAZWA-RRRR-MM-DD/`, plus `spis.txt` |

Scenariusze ekranów korzystają jeszcze z opcji programu symulatora (`firmware/prototyp/test/symulator.cpp`), które ustawia `symuluj.py`: `--konfiguracja` (daty wymiany sprzętu w `/konfiguracja.txt` kopii zrzutu), `--czynniki-scenariusza` (odpowiedzi „Co było inaczej?” na noce historii scenariusza), `--airbridge-przerwa OD,DO` (wyjazd: AirBridge milczy, Wi-Fi jest, noce dochodzą po powrocie) i `--noc-opozniona DATA,GG:MM` (noc dochodzi później niż 04:30, więc panel czyta listę nocy AirBridge). Atrapa gniazda raportu pozwala pokazać ściągę z kodem QR (nikt nie pobiera raportu, więc PDF zostaje w pamięci panelu i symulator kopiuje go do wyniku).

## Jak czytać `log.txt`

Przykładowy wiersz:

```
2026-11-04 07:15:00 · przycisk ▶ prawy · Wi-Fi nie · narysowano 002-20261104-0715-K1.png (po ▶ prawy) · sen do 2026-11-04 07:20:02 CET · strefa CET
```

| Część | Znaczenie |
|---|---|
| godzina | kiedy panel się obudził (czas polski); `[zegar panelu nieznany]` = panel sam nie zna jeszcze godziny |
| powód | `start` (włączenie zasilania, pierwsze wybudzenie), `przycisk …`, albo `zegar:` i powód z planu panelu: `czujniki` (pomiar przy łóżku, bez rysowania), `okno_poranne` i `sprawdzenie_11` (sprawdzanie nowej nocy), `slot_dzienny`, `slot_dzienny_godzina` (ocena w ciągu dnia), `powrot` (powrót do ekranu dnia po 5 min bez naciśnięcia), `poczatek_nocy` (ekran „Dobranoc” o 23:30), `odswiezenie`, `pytanie`, `pogoda` |
| Wi-Fi | czy panel łączył się z siecią |
| narysowano … | nazwa obrazka, w nawiasie przycisk, który go dał (ten sam podpis stoi w `spis.txt`); `ekran bez zmian` = panel policzył ekran, ale wyszedł taki sam jak wyświetlony, więc e-ink się nie odświeżył |
| sen do | kiedy panel obudzi się następnym razem, z datą i strefą (CET zimą, CEST latem) |
| strefa | strefa czasu wybudzenia; przy zmianie czasu w nocy z 25 na 26 października godzina 2:00 występuje dwa razy (najpierw CEST, potem CET) |

## Co symulator robi, a czego nie

- Liczy wszystko tym samym kodem co panel (`firmware/prototyp/src/`), więc pokaże też nowe ekrany, gdy trafią do tego katalogu.
- Nazwę ekranu w nazwie obrazka bierze z panelu: K1, K2, pytanie, test, `sprawy` (Otwarte sprawy), `zapisano`, `omowione`, `przeniesione`, `raport` z ekranu zapamiętanego przez panel, ekran dnia z wpisu panelu tuż przed rysowaniem (`dzien-S2A`, `dzien-wzorzec-L-03_ge10` ekran „pokaż lekarzowi”, `dzien-pytanie-sennosc`, `dzien-SK33` slot dzienny, `dzien-wieczor-SK01`) albo z dziennika ekranów; gdy nie ma żadnego z nich, obrazek ma w nazwie `dzien-nieznany` zamiast zgadywania. Każdy obrazek ma też wiersz `EKRAN …` w `log-panelu.txt`, a po każdej zmianie listy reguł spełnionych w pamięci panelu jest wiersz `SYMULATOR reguły spełnione …` (także reguły „tylko raport” i zdarzenia bez ekranu).
- Na początku robi to, co panel po włączeniu: przenosi pliki ze zrzutu w starym formacie do nowego (migracja). Zrzut na dysku zostaje nietknięty: symulator pracuje na kopii w pamięci.
- AirBridge jest udawany: terapia nie trwa. Bez scenariusza nie ma nowych nocy, więc symulator sprawdza decyzje panelu na zapisanej historii, nie pobieranie z aparatu. Ze scenariuszem każda noc syntetyczna dochodzi o 04:30 swojego poranka do pamięci panelu i na listę nocy AirBridge (jako już pobrana, z chwilą importu 04:30), więc panel widzi ją tak, jakby właśnie przyszła z aparatu; raportu nocy symulator nie podaje. W scenariuszu `brak-danych` AirBridge od północy w ogóle nie odpowiada, choć Wi-Fi działa.
- Czujnik przy łóżku podaje stale 21,5 °C i 45 %, bateria 3,95 V. Serwis pogody jest udawany (sekcja „Prognoza pogody i miasto”); lokalne źródło otoczenia (CO2 sypialni) jest tylko w scenariuszu `co2`, więc bez niego symulowany panel jest jak egzemplarz bez źródła otoczenia: bez ekranów CO2 i bez ocen wieczornych z Wi-Fi (5.8.8); w scenariuszu `co2` jak egzemplarz ze źródłem. Przerywniki S2d (karty G01–G18) pojawiają się rano w miejscu dowodu spokoju najwyżej raz na 7 dni, więc widać je w przebiegach dłuższych niż tydzień (np. rozbieg scenariusza); w `spis.txt` i w nazwie obrazka stoją jako „karta z budzikiem: przerywnik S2d G..” (`dzien-S2D-G..`).
- Portret nocy (S11, spec 5.4.16) pojawia się tylko z pliku `/dane/portret.csv` w zrzucie panelu, gdy jest z ostatniej nocy zrzutu i symulacja zaczyna się tego poranka (obrazek `dzien-S11`, potem drugi ekran poranka z `▶ portret` w stopce; sprawdzone 12.11.2026 na zrzucie syntetycznym z dopisanym plikiem). W scenariuszach z dopisanymi nocami Portretu nie ma: atrapa AirBridge nie podaje plików sesji (`/api/sessions`), więc panel nie liczy nocy z plików i nie zapisuje przebiegu. Obrazki Portretu na wielu rodzajach nocy daje podgląd `tools/prototyp/testy.sh` (`s11-*`, dane syntetyczne zbudowane w kodzie, `test/podglad.cpp` `podgladPortretu()`).
- Plan wybudzeń bierze z panelu: symulator niczego nie zgaduje, tylko przesuwa zegar do chwili, o którą panel prosi przed snem, albo do naciśnięcia przycisku.

## Zrzut syntetyczny (do prób i dokumentacji)

Wymyślone dane (60 nocy, ASVAuto) można wygenerować i pokazywać bez ograniczeń:

```
python3 tools/symulator/generuj_zrzut.py ~/cpap-eink-syntetyczny
python3 tools/symulator/symuluj.py --dane ~/cpap-eink-syntetyczny --od 2026-11-04T05:00 --do 2026-11-04T23:59
```

`--wersja 1` daje zrzut w nowym formacie (`dane/`, `log/`). Testy automatyczne: `python3 -m pytest -q tools/test_symulator.py tools/test_symulator_scenariusze.py` (drugi przechodzi wszystkie scenariusze na zrzucie syntetycznym z obrazkami syntetycznymi).

## Gdy coś nie działa

| Komunikat | Co zrobić |
|---|---|
| `nie znalazłem działającego kompilatora C++` | `xcode-select --install`, zatwierdź instalację, potem powtórz komendę |
| `plik przycisków … nie jest zwykłym tekstem` albo `… dokument RTF` | w TextEdit: Format → Zmień na zwykły tekst, zapisz jeszcze raz |
| `UWAGA: zrzut ma noce późniejsze niż --od` | ustaw `--od` na dzień podany w komunikacie |
| `Uwaga: pominięte … nieczytelne wiersze pliku nocy` | te noce panel też pominie; numery wierszy są w komunikacie |
| `zrzut leży w repozytorium` | przenieś katalog zrzutu np. do `~/cpap-eink-dane/` i podaj nową ścieżkę |
| `--wyjscie wskazuje plik` | podaj katalog, nie plik |
| `katalog wyniku leży w katalogu zrzutu` | wynik nie może trafić do katalogu z danymi panelu; pomiń `--wyjscie` albo podaj inny katalog |
| `nie ma pliku nocy …, są za to zrzuty w podkatalogach: a, b` | dopisz do ścieżki `/a` |
| `nie rozumiem daty` | wzór daty: `2026-11-05T07:15` (z literą T) |
| `kompilacja … się nie udała` | kod w repozytorium jest w trakcie zmian; zgłoś wypisany błąd |
| `nieznany scenariusz` | nazwę scenariusza wpisz jak w tabeli „Scenariusze” (np. `ahi-rosnie`, z myślnikiem) |
| `UWAGA: scenariusz pokazuje swój ekran od …` | pomiń `--od`, albo ustaw je na podaną chwilę |
| `nie znam współrzędnych miasta` | podaj `--wspolrzedne 50.26,19.02` |
| `obrazki trzeba zamienić na format panelu, a do tego potrzebna jest biblioteka Pillow` | wskaż `assets/obrazki-dzienne-prywatne` (z podkatalogiem `panel/`) albo katalog z plikami `.pbm` |
| `nie ma obrazków` | wskaż katalog z plikami `.pbm`, `.png` albo `.jpg` |
| `nie ma katalogu obrazków` | sprawdź ścieżkę po `--obrazki` (w cudzysłowie, gdy ma spację) |
| `scenariusz zwykly-dzien działa tylko sam` / `scenariusz brak-danych działa tylko sam` | uruchom go bez innych `--scenariusz` |
| `ten sam scenariusz podany dwa razy` | każdą nazwę podaj raz |
| `podaj --od i --do (albo --scenariusz, który sam ustawia okres)` | dopisz `--od` i `--do` albo `--scenariusz` |
| `--pokaz-wszystko sam ustawia scenariusze, okresy i przyciski` | uruchom `--pokaz-wszystko` bez `--scenariusz`, `--od`, `--do`, `--przyciski` |
| `plik prognozy … nie jest poprawnym JSON-em` / `brak 48 godzin` | podaj odpowiedź Open-Meteo z `forecast_days=2` |
| `plik prognozy …: strefa …` / `pierwsza godzina …` | pobierz prognozę z `&timezone=Europe%2FWarsaw`: panel pyta w czasie polskim, a symulator bierze 48 godzin od północy |
| `--wspolrzedne: nie rozumiem` | wzór: `--wspolrzedne 50.2597,19.0211` (kropka w liczbach, przecinek między nimi) |
| `w zrzucie nie ma nocy, scenariusz nie ma od czego zacząć` | wskaż zrzut z plikiem `noce.csv` |
