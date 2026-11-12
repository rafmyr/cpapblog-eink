# ADR-0040: Portret nocy: dziewiąta forma wizualizacji i pierwszy ekran poranka

| | |
|---|---|
| Data | 11.11.2026 |
| Status | przyjęta 11.11.2026 (decyzja właściciela projektu 11.11.2026); zmiany 2026-11-11 i 2026-11-12 niżej |
| Źródło | decyzja właściciela projektu 11.11.2026; makiety ekranu na komputerze; [ADR-0036](0036-noc-z-plikow-sesji-edf.md) (pliki sesji PLD, EVE), [ADR-0037](0037-asv-bez-rodzaju-bezdechu-i-csr.md) (ASV bez rodzaju bezdechu), [ADR-0034](0034-model-pasm-wycieku-p95.md) (15 L/min) |

## Kontekst

Ekran dnia pokazuje dziś noc liczbami (S2a: czas z maską i AHI, pasek mediany 7 nocy). Liczby odpowiadają na pytanie „ile”, ale nie „kiedy” i „jak wyglądała noc”: przerwa w środku nocy, wyciek nad ranem albo skupisko zdarzeń o 5:00 giną w średniej. Od ADR-0036 panel czyta pliki sesji aparatu (próbki co 2 s z `PLD`, zdarzenia z `EVE`), więc ma przebieg nocy w czasie, ale po przeliczeniu nocy nic z niego nie zostaje.

Właściciel projektu wybrał 11.11.2026 kierunek „Portret nocy”: rysunek ostatniej nocy na tarczy zegara 12-godzinnego. Makiety (ASV z nocy właściciela, CPAP / AutoSet z nocy zmyślonych) oglądano tylko na komputerze z zastępczym krojem. Makieta z prawdziwych nocy AutoSet i makieta na e-inku (test 5 sekund) zostały pominięte decyzją właściciela projektu z 11.11.2026; to przyjęte ryzyko.

Bufor ekranu ma dziś 8 form wizualizacji (spec 5.4.3) i prymitywy: prostokąt, ramkę, linię 1 px, koło (puste albo wypełnione) i wypełniony trójkąt (`src/ekrany.cpp` `Plotno::trojkat()`), bez wypełniania wielokątów i bez łuku.

## Decyzja

1. **Nowy ekran dnia S11 „Portret nocy”** (spec 5.4.16): rysunek ostatniej nocy na tarczy 12-godzinnej (godziny tam, gdzie na zwykłym zegarze), obok do 4 podpisów i legenda. Ekran danych bez budzika ([ADR-0007](0007-dwa-tryby-ekranu.md), [ADR-0009](0009-anatomia-karty.md)): nie ma wersji karty, humor go nie dotyczy, nagłówek poranny z `dane {hh:mm}` i dopiskiem `· ocena ograniczona` jak każdy ekran dnia (R-EKR-46).
2. **Dziewiąta forma wizualizacji: „pierścień z pikami”.** Język rysunku jeden dla wszystkich trybów:

   | Znak | Znaczenie | ASV, ASVAuto | CPAP, AutoSet |
   |---|---|---|---|
   | czarny pierścień równej grubości; dziura = przerwa | maska na twarzy (sesje aparatu) | tak | tak |
   | kropkowany ślad w całym kole | tarcza (tło), żeby dziura czytała się jako przerwa, nie jako koniec rysunku | tak | tak |
   | biały odcinek pierścienia | wyciek: P95 wycieku odcinka 5 min ≥ 15 L/min (ten sam próg co noc niepewna, ADR-0034) | tak | tak |
   | pik na zewnątrz, czubek zaokrąglony, podstawa ok. 10 min | ASV: „większe wsparcie oddechu” (wsparcie PS w odcinku ≥ zwykłe PS + 2 cmH2O); CPAP / AutoSet: zdarzenia w odcinku (wysokość rośnie z liczbą, do 7) | tak | tak |
   | pik do środka | zdarzenia centralne | nie (aparat nie zapisuje rodzaju, ADR-0037) | tak |
   | kropka przy pierścieniu | pojedyncze zdarzenie (bezdech albo spłycenie) | tak | nie |

   Nazwy: dla pacjenta „większe wsparcie oddechu”, w raporcie dla lekarza „PS” (to fakt z pomiaru, nie obserwacja panelu). Do środka idą tylko zdarzenia centralne; nieokreślone (`Apnea`) idą na zewnątrz.
3. **Pierwszy ekran poranka:** po imporcie nocy Portret jest ekranem dnia przed grupami R5e–R9 tabeli 7.2; ekrany braku danych (S5), synchronizacji (S10), lekarskie (L), „noc mało czytelna” i problemy poranne (R4–R5d) oraz pytanie przed ekranem (R0) zachowują pierwszeństwo. W dni z ekranem z R2–R5d, który trzyma ekran dnia do wieczora, Portret jest pierwszą stroną pod ▶ przed K1 (decyzja właściciela projektu 11.11.2026). Obok tarczy stoją liczby nocy z S2a (czas z maską, AHI; decyzja właściciela projektu 11.11.2026). Zwycięzca R5e–R9 nie znika: pokazuje się po Portrecie w tym samym poranku (domyślnie po 30 min `[PRÓG]`). Portretu nie ma w kokpicie, „Portretu 30 nocy” nie ma.
4. **Podpisy tylko opisują** ([ADR-0002](0002-panel-nie-zmienia-ustawien.md)): pierwszy to zdanie o normie z reguł zdania S2a, dalej, zależnie od nocy: `Najgęściej {hh:mm}–{hh:mm}`, wyciek, przerwa, centralne, `Ciśnienie na maksimum {n} min` (AutoSet i ASVAuto) i `Drzemka poza tarczą`. Żadnej wartości ustawień ani drogi do zmiany; teksty Portretu wchodzą do przeglądu słów granicy (spec 9.3).
5. **Liczy i trzyma panel** ([ADR-0001](0001-przetwarzanie-lokalne.md)): przy jedynym przejściu po próbkach pliku `PLD` i zdarzeniach `EVE` (`src/noc_pliki.cpp`) panel zapisuje odcinki 5-minutowe ostatniej nocy w nowym pliku `/dane/portret.csv` (T-DANE-38, wersja 1, ADR-0022), bez dodatkowych pobrań z AirBridge. Trzyma tylko ostatnią noc; noce sprzed wgrania nie są dociągane.
6. **Bez plików sesji nie ma Portretu:** AirBridge starszy niż 2.3.1, noc z raportu JSON, ze STR albo z karty, pliki nieczytelne albo przebieg z innej nocy niż ostatnia: rano dotychczasowy ekran poranka (tabela 7.2), nigdy pusta tarcza (R-POR-03).
7. **„Nie oceniam” nie znaczy „w porządku”** ([ADR-0029](0029-zakres-skierowan-w-wydaniu-1-0.md) pkt 7; od 12.11.2026 jeden komunikat po nocy niepewnej, zob. Zmiana 2026-11-12): przedrostek `Noc w normie. ` pierwszego podpisu tylko przy warunku uspokojenia (5.4.13); noc niepewna dostaje `· ocena ograniczona` w nagłówku i `AHI nie oceniam: duży wyciek` w liczbach nocy; każdy brakujący pomiar ma podpis przed pozostałymi: ASV bez normy PS `Wsparcie oddechu: uczę się.`, bez próbek PS `Wsparcie oddechu: brak odczytu.`, bez kanału wycieku `Wyciek: brak odczytu.`, bez zdarzeń `Zdarzenia: brak odczytu.`, CPAP / AutoSet bez rodzaju bezdechu `Rodzaj bezdechów: brak zapisu.`; nigdy pusty, „spokojny” rysunek bez słowa.
8. **Rysowanie:** forma rysowana testem przynależności piksela we współrzędnych biegunowych (dla każdego piksela pierścienia od promienia ok. 96 do 172: promień i kąt → w pierścieniu, w piku, w białym odcinku), bez wielokątów, arytmetyką całkowitą albo stałoprzecinkową z tablicą kątów, żeby obraz był ten sam w symulatorze i na panelu (precedens testu piksela: ikona `↻` w `src/ekrany.cpp`); cyfry, kreski i tekst zwykłymi prymitywami. Czas rysowania jest liczbą z pomiaru i bramką testu, nie warunkiem w czasie działania. Wzorzec wyglądu: generator w Pythonie `tools/portret/`, z którym porównuje się obrazek z kodu panelu w ustalonej tolerancji pikseli.

## Konsekwencje

- Spec funkcjonalna: nowa 5.4.16, wiersz S11 w 5.4.2, 6.2, 1.6 i tabeli 7.2 (pozycja „RP” między R5d a R5e), wymagania R-POR-01…R-POR-24, teksty z limitami w px, progi rysunku w `docs/spec/progi-poza-zakresem.csv` (`[PRÓG]`, wygląd ekranu, obserwacja na makietach, 1 osoba w ASV). Spec techniczna: T-DANE-38, T-WYM-97, T-WYM-98, T-TEST-413…T-TEST-416, komponenty T-KOMP-03, T-KOMP-05, T-KOMP-08, T-KOMP-09.
- Nowy rodzaj danych w pamięci panelu: ok. 10 KB na noc, przepisywany co rano; w kopii na karcie i w zrzucie plików (dane zdrowotne, nigdy poza panelem).
- Rysowanie formy w buforze 1 bit trzeba dopisać (nie ma wypełniania wielokątów; jest tylko wypełniony trójkąt i koło) i zmierzyć jego czas.
- Spokojny pacjent widzi co rano równy pierścień (przyjęte, do oceny po 14 nocach).
- Portret widać tylko rano, do startu slotu dziennego (przyjęte przez właściciela projektu).
- Ryzyko przyjęte decyzją z 11.11.2026: piki i białe odcinki nie były oglądane na e-inku 1 bit, a wersja CPAP / AutoSet nie była rysowana z prawdziwych nocy. Pierwszy obrazek z prawdziwej nocy AutoSet i pierwszy obraz na panelu to kryteria sprawdzenia.
- Granica „większego wsparcia” (+2 cmH2O) dobrana na jednym pacjencie ASV.

## Odrzucone warianty

- **Strona „Portret nocy” w kokpicie:** Portret tylko rano.
- **„Portret 30 nocy” i mozaika 4 tygodni:** na danych właściciela rysunek 30 nocy niczego nie pokazywał; powrót po obejrzeniu prawdziwych nocy AutoSet.
- **Rysunek z raportu JSON** (bez plików sesji): raport nie ma przebiegu PS ani wycieku w odcinkach; pusta albo zgadywana tarcza myliłaby.

## Zmiana 2026-11-11: decyzje właściciela projektu i kod

- **Decyzje właściciela projektu 11.11.2026:** ekran problemu z R2–R5d zostaje pierwszy, Portret jest stroną pod ▶ przed K1 (R-POR-24); `Wyciek przez {n} min` z rzeczywistych próbek ≥ 15 L/min (pole `wyc_s`); wiersz liczb nocy obok tarczy; `EPAP na maksimum {n} min` w ASVAuto jako rozszerzenie pkt 4 (bez zmiany jego treści).
- **Wzorzec:** `tools/portret/` (Python, geometria w liczbach zmiennoprzecinkowych, kroje z `fonty.h`), test T-TEST-413 na syntetycznej karcie `tools/portret/karta_portretu.py`.
- **Dane:** `src/portret.h` (`portret::Zbieracz` dołączany do `LicznikNocy` w tym samym przejściu po próbkach `PLD`, pkt 5), plik `/dane/portret.csv` wersja 1 (T-DANE-38), zapis w `src/dyrygent.cpp` `dobaZPlikow()` tylko dla doby, która jest ostatnią nocą w historii; odcinki równe wzorcowi na syntetycznej karcie i na 11 dobach egzemplarza referencyjnego (T-TEST-414); pamięć robocza zbieracza stała ok. 6 KB (jeden bufor odcinka na kanał; najgorszy przypadek 40 krótkich sesji zmierzony, 6032 B), plik ok. 4–19 KB.
- **Ekran:** `src/portret_ekran.h` (forma „pierścień z pikami”, pkt 8: test biegunowy z tablicą `atan` w sekundach tarczy, pierwiastek całkowity, bez liczb zmiennoprzecinkowych w wyniku), `Uklad::PORTRET` w `rysujEkranDanych()`, wybór poranka w `src/dyrygent.cpp` `rysujPortretB40()` (pkt 3), drugi ekran po 30 min z planu (`WejsciePlanu::drugiEkranPortretu`), stan poranka w RTC z kopią w NVS (`por_db`, `por_od`), strona `Ekran::PORTRET` pod ▶ (R-POR-24). Teksty z zestawu (`S11.*` w `docs/spec/katalog/teksty.csv`). Tolerancja wzorca (T-TEST-415): najwyżej 0,5% czarnych pikseli strefy treści, kolumna tekstu piksel w piksel; zmierzone 0–3 piksele na syntetycznej karcie i 22–36 na nocach ASV egzemplarza referencyjnego. Czas rysowania na PC 0,6–3 ms (288 odcinków, 500 zdarzeń); na panelu w logu wybudzenia (`ekran dnia S11 … rysowanie {n} ms`).

## Zmiana 2026-11-11: poprawki okna tarczy, podpisów i danych

- **Okno tarczy:** okno obejmuje całą najdłuższą sesję (od jej startu minus 2 h, koniec w oknie; sesja dłuższa niż 12 h od startu); `Część nocy poza tarczą` przed `Drzemka poza tarczą`; mediana zwykłej pory bez nocy `A` z porankiem do 09.11.2026 (M-18 sprzed zmiany jej definicji liczone ze startu pierwszej sesji doby).
- **Pierwszy podpis po nocy niepewnej (decyzja właściciela projektu 11.11.2026):** `Tej nocy nie oceniam: duży wyciek.` zamiast zdania o tygodniu (R-POR-25; pkt 7 bez zmiany zasady).
- **Maksimum nieznane:** AutoSet albo ASVAuto bez `MPA`/`EAX` → litera `M` w pliku i podpis `Maksimum ciśnienia: nieznane.` / `Maksimum EPAP: nieznane.` (R-POR-26).
- **Dane (pkt 5):** pamięć zbieracza stała (jeden bufor odcinka na kanał), zakresy pliku w jednym miejscu, `Centralne: {n}` z odcinków, zapis po Wi-Fi jak historia, plik uszkodzony w logu.
- **Ekran (pkt 3, 8):** legenda wycieku tym samym warunkiem co rysunek, tolerancja wzorca z limitem 60 px i bez bloku 3×3 różnic, start slotu dziennego czeka na drugi ekran poranka, strona Portretu pod ▶ tylko do startu slotu dziennego, a ▶ otwierające ją slotu nie zaczyna (R-POR-24 „rano”), rozruch pierwszego podpisu jak S6.

## Zmiana 2026-11-12: wyciek z jednego źródła, jeden komunikat po nocy niepewnej, podpis zbiorczy braków, Portret pod ▶

Poprawki przyjęte przez właściciela projektu 12.11.2026; zasady pkt 1–8 bez zmian, poniżej to, co zmienia się w ich wykonaniu.

- **Wyciek z jednego źródła (pkt 2, tabela „Język rysunku”):** biały odcinek pierścienia to odcinek z co najmniej 60 s `[PRÓG]` próbek `Leak` ≥ 15 L/min (pole `wyc_s`, odcinek liczy się do wycieku, czyli ma co najmniej 30 poprawnych próbek), a nie P95 wycieku odcinka ≥ 15 L/min; podpis `Wyciek przez {n} min` sumuje sekundy tylko odcinków białych na rysunku (w oknie, z częścią w sesji), legenda `wyciek od 15 l/min` tym samym warunkiem. Wcześniej podpis sumował wszystkie odcinki, więc stał `Wyciek przez 11 min` bez białego łuku albo biały łuk 60 min przy `8 min`. Plik `/dane/portret.csv` bez zmian (pole `wyciek95` zostaje). Próg 60 s i noc niepewna z wyciekiem rozproszonym (bez białego łuku) zostają do obserwacji.
- **Jeden komunikat po nocy niepewnej (pkt 7):** liczby nocy obok tarczy to wtedy sam `{czas} z maską` (bez AHI i bez drugiego wiersza `AHI nie oceniam: duży wyciek`); o nocy niepewnej mówi jeden komunikat, pierwszy podpis `Tej nocy nie oceniam: duży wyciek.` i dopisek `· ocena ograniczona` w nagłówku. Zasada pkt 7 („nie oceniam” nie znaczy „w porządku”) bez zmian; tekst `S11.ahi_niepewne` wycofany (spec 10.2, `tools/wycofane.csv`), kolumna zyskuje 34 px.
- **Brak pomiaru nigdy nie wypada z podpisów (pkt 7):** gdy podpisów o brakującym pomiarze (także `Wsparcie oddechu: uczę się.`) jest więcej niż miejsc (4, z linią uwagi 2, pierwszy podpis zawsze zostaje), pierwsze stoją pełnym podpisem, a pozostałe w jednym podpisie zbiorczym `Nieznane: {lista}.` z nazwami (`wsparcie`, `norma wsparcia`, `wyciek`, `zdarzenia`, `rodzaj bezdechów`, `maksimum`), w kolumnie najwyżej 2 linie po 382 px (nowe R-POR-27). Podpis zbiorczy zawsze z nazwami, bez formy z samą liczbą (`Nieznane: {n} pomiary.`, tekst `S11.braki_n` wycofany), nazwy bez skrótów (`norma wsparcia` zamiast `norma PS`, bez PS na ekranie pacjenta; `rodzaj bezdechów` zamiast `centralne`), bilans kolumny 377 px (z linią uwagi 313 px). Wcześniej przy linii uwagi drugi brak wypadał, sprzecznie z pkt 7 i ADR-0029 pkt 7.
- **Portret pod ▶ także w zwykły dzień (pkt 3):** gdy po 30 min Portretu ekranem dnia jest drugi ekran poranka, ▶ z niego otwiera stronę Portretu przed K1 tak samo jak w dni problemu (stopka strony `◀ ekran dnia   ▶ ostatnia noc   ↻ ekran dnia`, ▶ → K1, ◀ z K1 → Portret, podpis `▶ portret` przed odczytem czujnika na drugim ekranie), do startu slotu dziennego; to ▶ slotu nie zaczyna, inne naciśnięcia na drugim ekranie zaczynają go jak dotąd (R-POR-05; naciśnięcia na stronie Portretu i w kokpicie slotu dziennego nie zaczynają). Gdy start slotu przypada razem z drugim ekranem, strony pod ▶ nie ma. Konsekwencja „Portret widać tylko rano, do startu slotu dziennego” zostaje, ale w zwykły dzień Portret nie znika po 30 min. Portretu dalej nie ma w ciągu stron kokpitu. Test podwójnego `▶ portret` na drugim ekranie S-K: `tools/test_symulator.py`.
- **Portret w podglądzie:** `test/podglad.cpp` rysuje pełny ekran Portretu (nagłówek, tarcza, kolumna, stopka) na przebiegach zbudowanych w kodzie (obrazki `s11-*` z `tools/prototyp/testy.sh`): AutoSet z pikami, centralnymi i wyciekiem, ASVAuto z kropkami i wsparciem, noc niepewna, noc spokojna, linia uwagi (także z podpisem zbiorczym), strona pod ▶. Symulator doby pokazuje Portret tylko z pliku `/dane/portret.csv` w zrzucie panelu, a w scenariuszach nie (atrapa AirBridge bez plików sesji).

## Zmiana 2026-11-12 (2): znacznik startu okna zamiast napisu okna

Decyzja właściciela projektu 12.11.2026: napis okna usunąć, a na kole pokazać, kiedy zaczyna się okno tarczy, samym znacznikiem. Powód: napis zakresu okna `{hh:mm}–{hh:mm}` w środku tarczy (np. `21:30–09:30`) był brany za godziny snu, a to tylko 12 godzin pokazanych na tarczy. Zasady pkt 1–8 bez zmian; zmienia się wykonanie pkt 8 (rysowanie) i tabela „Język rysunku” w 5.4.16.

- **Środek tarczy pusty:** bez napisu i bez liczb, także przy linii uwagi (wcześniej zakres w dwóch wierszach); cyfry `12`, `3`, `6`, `9` i kreski godzin zostają.
- **Znacznik startu okna** (R-POR-06) pod kątem początku okna tarczy: kreska promieniowa przez tor pierścienia (promienie 110–150, 8 px pod pierścieniem do 16 px nad nim, szerokość 4–5 px), w obrębie pierścienia z białą przerwą 2 px po obu stronach, żeby była czytelna na czarnym łuku maski, oraz grot (pełny trójkąt, podstawa 14 px, czubek 15 px od osi kreski) na zewnątrz pierścienia przy końcu kreski, po stronie ruchu wskazówek zegara (kierunek czytania). Bez liczby i bez napisu; legenda bez nowej pozycji (strzałka tłumaczy się sama). Rysowany po pierścieniu i pikach, przed kropkami ASV i cyframi; na piku czarny na czarnym, więc kształt piku zostaje, a grot w tym rzadkim przypadku (sesja przez start okna z pikiem w pierwszych minutach) zlewa się z pikiem.
- **Rysowanie (pkt 8):** test przynależności piksela w układzie „wzdłuż promienia / w poprzek” z tą samą tablicą sinusów co reszta tarczy, arytmetyka całkowita (`src/portret_ekran.cpp` `rysujZnacznikStartu()`); wzorzec `tools/portret/rysunek.py` `rysuj_znacznik()` w liczbach zmiennoprzecinkowych, porównanie T-TEST-415 bez zmiany tolerancji. Granice w ćwiartkach i połówkach piksela, żeby piksel nie leżał dokładnie na granicy w żadnej z dwóch arytmetyk.
- Podpis `Część nocy poza tarczą`, okno tarczy i podpisy z godzinami (`Najgęściej`, `Przerwa`) bez zmian. Napis okna nie miał tekstu w katalogu (składany z godzin w kodzie), więc nie ma ID do wycofania.
- Do obserwacji na 14 nocach: czy znacznik jest zrozumiały bez liczby, czy grot nie jest mylony ze znakiem piku, a przerwa wokół kreski z przerwą w terapii.
- **Wyciek ma pierwszeństwo przed znacznikiem.** Przerwa wokół kreski (±4,25 px) jest szersza niż łuk odcinka 5 min (ok. 5,5 px, z linią uwagi 4,7 px), więc biały odcinek wycieku na starcie okna znikał (8 z 288 okien bez widocznej bieli, z linią uwagi 96 z 288), a legenda `wyciek od 15 l/min` i podpis `Wyciek przez {n} min` zostawały (sprzeczne z R-POR-08 i R-POR-13). Reguła: gdy pierwszy albo ostatni odcinek okna jest biały na rysunku (ten sam warunek co rysunek, podpis i legenda), kreska i przerwa nie wchodzą w tor pierścienia; biały odcinek z czarnym obrysem rysuje się cały, a znacznik to część kreski pod pierścieniem, część nad nim i grot. Przerwa sięga mniej niż 5 min łuku w obu skalach, więc innych odcinków to nie dotyczy. Zmierzone najmniej 48 widocznych pikseli bieli w pierwszym odcinku (z linią uwagi 27) przy 288 startach okna; czas na tarczy bez zmian (znacznik stoi pod tym samym kątem).
