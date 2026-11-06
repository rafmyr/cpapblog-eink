# Wariant B: system ekranów dnia (skala B)

> **Stan dokumentu.** Projekt skali B z 30.10.2026. Makiety 1-bit: [`makiety/`](makiety/) (wybrane stany; wygenerowane narzędziem spoza repozytorium). Po audycie UX z 30.10.2026 (decyzja) obowiązuje spec, a nie ten plik, tam gdzie się różnią: ramka zamiast negatywu dla „poza normą” (kolumna paska, wiersz K1, etykieta K2), linia zaproszenia jako biały pasek w ramce, `AHI` jako jedyna nazwa M-02, nagłówek `{dzień} {data}` bez liczb, stopka `przy łóżku {t}° · {w}%`, S2c `Przy łóżku · ostatnia noc`, K2 z nagłówkiem `2 / 2` (spec 5.4.2, 5.4.3, 5.5).


Dotyczy: ekran dnia w trybie danych (szablon A, 5.4): S2a, S2b, S2c, S5, S6, kamień milowy S-K-28 i linia uwagi.
Nie dotyczy: karty z budzikiem (ADR-0007, ADR-0009), K1, K2, F08, S9. Ten dokument nie zmienia repo; zmiany w spec są wypisane w części 7 jako propozycje.

Makiety: [`makiety/`](makiety/) (wybrane stany, 1 bit, 800×480). Generator makiet i arkusze podglądu zostały poza repo; ten sam układ rysuje prototyp (`tools/prototyp/testy.sh` zapisuje podgląd `mak-<stan>.png`).

Render: Playwright w skali 4×, potem uśrednienie pola 4×4 i próg (jak `tools/prototyp/generuj.py`: raster SS = 4, uśrednienie, próg), więc obraz to to, co narysuje e-ink 1-bit. Tekst stoi na linii bazowej (SVG `<text x y>`), tak jak `tekst(p, krój, x, y, …)` w `ekrany.cpp`: współrzędne z tabel przechodzą do C++ wprost.

## 1. Zasady (sprawdzone na makietach)

| Zasada z założeń | Wynik | Uzasadnienie |
|---|---|---|
| Minimum 22 px na ekranie dnia | potwierdzona | Panel ma ok. 0,204 mm/px. Wersalik 22 px Public Sans to ok. 3,2 mm, czyli ok. 7,5′ kątowo z 1,5 m: na granicy ostrości wzroku 20/30. Z 1,5 m 22 px odczyta osoba z dobrym wzrokiem, z wysiłkiem. Dlatego 22 px tylko dla treści drugorzędnej (legenda, stopka, słowa w pasku, etykieta wersalikami), a wszystko, co ma być czytane z łóżka, ma co najmniej 34 px |
| Odmiana ≥ 600 poniżej 30 px | potwierdzona i zaostrzona | 24 i 28 px w 700, 22 px w 600 (P22B już jest). Cienka kreska 1–2 px na e-inku przy progu ginie (zdjęcie z 30.10: `↔ jak zwykle` 18 px regular jest ledwo widoczne) |
| Liczby nocy serif 600 | potwierdzona | Source Serif 4 600 w 96 px; tylko cyfry, `,`, `h`, `–`, `:` (krój podzbiór, 5 KB) |
| Brak tekstu 16–18 px regular | potwierdzona | Na ekranach dnia nie zostaje żaden P18/PI18/C18 |
| Każda liczba ma podpis | potwierdzona i rozszerzona | Noc i mediana różnią się na 4 sposoby naraz: (1) podpis nad liczbą: `Ostatnia noc` / `AHI` kontra tytuł strefy `OSTATNIE 7 NOCY · MEDIANA`; (2) krój: noc serifem 96 px, mediany bezszeryfowym 28 px; (3) miejsce: noc nad linią, mediany pod linią przy dole ekranu; (4) zapis: noc `9 h 09`, mediana `9:07` (zostaje jak w kodzie i K1) |
| Nowe: strzałki rysowane jako ikony | dodana | `↔` nie ma w Public Sans ani w Source Serif 4; firmware bierze ją z IBM Plex 500, więc wychodzi cienka i mała (zdjęcie). ↔ ↑ ↓ rysować prymitywami jak ↻ ◀ ▶ (`ikona()` w `ekrany.cpp`): wysokość = wersalik kroju, grubość = px/11 (min. 2), groty trójkątami |
| Nowe: „inaczej niż zwykle” w negatywie | dodana | Kolumna z `↑ wyżej niż zwykle` / `↓ niżej niż zwykle` ma czarne pole, jak wiersz K1 w stanie odchylenia (R-KOK-03). `jak zwykle` i `lepiej niż zwykle` zostają na bieli. Dzięki temu znaczenie nie zależy od słowa 22 px |
| Nowe: twarda spacja | zastosowana | R-EKR-08: `12 h 59`, `30 min`, `2 s`, `z maską` nie łamią się; dodatkowo `co 30 min.` w E-01 (inaczej `co` zostaje samo na końcu linii) |

## 2. Skala B

Ta sama skala obowiązuje projektanta K1, K2, F08 i S9 (nazwy).

| Nazwa | Krój | px | Waga | Krój w firmware | Do czego |
|---|---|---|---|---|---|
| B1 | Source Serif 4 | 96 | 600 | **SN96** (nowy, podzbiór `0123456789,h–:` + spacja) | liczby ostatniej nocy, duża liczba kamienia |
| B2 | Source Serif 4 | 120 | 400 | S120 (jest) | numer serii S2b |
| B3 | Source Serif 4 | 44 | 700 | SB44 (jest) | tytuł ekranu (S5, S6, rocznica) |
| B4 | Public Sans | 34 | 700 | **P34B** (nowy) | zdanie o tygodniu / sypialni, `noc z rzędu`, podpis dużej liczby, `nie oceniam: duży wyciek` |
| B5 | Public Sans | 28 | 700 | **P28B** (nowy) | wartości paska, powód, krok, linia wzorca A |
| B6 | Public Sans | 24 | 700 | **P24B** (nowy) | nagłówek, podpisy liczb nocy, nazwy kolumn, linia uwagi, podpisy postępu S6 |
| B7 | Public Sans | 22 | 600 | P22B (jest) | minimum: etykieta wersalikami (rozstrzelenie 2 px), tytuł paska, legenda, słowa pod wartościami, stopka, podpisy osi |

Wysokość wersalika do obliczeń: `cap = round(px × 0,70)` (B1 67, B2 84, B3 31, B4 24, B5 20, B6 17, B7 15).

### Kroje: koszt flash (policzony funkcją `font_c` z `generuj.py`, glify 0x20–0x9D)

| Krój | Status | Bitmapa | Razem z glifami i `adv16` |
|---|---|---|---|
| SN96 (podzbiór cyfr) | nowy | 3,7 KB | ok. 5,0 KB |
| P34B | nowy | 5,8 KB | ok. 7,0 KB |
| P28B | nowy | 3,9 KB | ok. 5,2 KB |
| P24B | nowy | 3,0 KB | ok. 4,2 KB |
| **Razem nowe** | | | **ok. 21,4 KB = 0,7% z 3,2 MB** (flash z ok. 51% na ok. 51,7%) |
| SB44, S120, P22B | są | | 0 |

Do dopisania w `FONTY` w `generuj.py`: `("SN96","serif",96,600,"0123456789,h–:")`, `("P34B","sans",34,700)`, `("P28B","sans",28,700)`, `("P24B","sans",24,700)`. Po przejściu ekranów dnia na skalę B przestają być na nich używane S42, S36, S48, P20, P20B, P18; nie kasować przed sprawdzeniem K1, K2, F08, S9, kart i testu sprzętu (część jest tam w użyciu).

## 3. Siatka

Marginesy: X = 28, XP = 772, szerokość treści W = 744 (bez zmian względem kodu). Pas 4 kolumn: `kx = 28 + i × 186`.

| Strefa | y (px) | Zawartość |
|---|---|---|
| Nagłówek | 0–52, linia 2 px na y = 52 | B6; lewo i prawo na linii bazowej 35 |
| Linia uwagi (gdy jest) | od 54: zwykła 48 px, wzorca A 60 px | czarny pasek na całą szerokość, `!` i tekst biały; linia bazowa: zwykła 87 (B6), A 94 (B5); tekst od X, po `!` odstęp 12 |
| Treść | od 54 + h_linii do 434 (ze stopką) albo do 480 (bez) | zależnie od ekranu, tabela 4 |
| Stopka (gdy jest treść) | linia 2 px na y = 434, tekst B7 na 465 | lewo pogoda, prawo sypialnia albo podpis akcji (`↻ ponów`) |

Stopka znika, gdy obie strony są puste (zdjęcie z 30.10: stopka pusta, a zajmowała 46 px). Wtedy dół strefy treści to 466 zamiast 422.

## 4. Elementy ekranów

`top` = 54 + wysokość linii uwagi (0, 48 albo 60).

### S2a i S2c

| Element | Skala | Pozycja (linia bazowa) | Uwagi |
|---|---|---|---|
| Podpis nocy | B6 | x 28, y top + 34 | `Ostatnia noc`; przy `dane z {data}`: `Noc {data}` (data poranka, jak etykieta F08 `NOC 30.10`) |
| Czas z maską | B1 | x 28, y top + 117 | `9 h 09`, brak = `–` (R-DANE-07) |
| Podpis i liczba AHI | B6 / B1 | x2 = max(328, koniec czasu + 56) | `AHI`; noc niepewna: zamiast liczby B4 w dwóch liniach `nie oceniam:` / `duży wyciek` (górna wyrównana do wersalika B1) |
| Zdanie | B4 | x 28, y = baza liczb + 50 | powód 5.4.2 (tydzień albo sypialnia); przyszły `W normie` jako przedrostek `W normie. ` |
| Pasek: linia | 2 px | od x 28 do 772, y = ptop | pasek przyklejony do dołu strefy: ptop = dół − wysokość paska |
| Pasek: tytuł i legenda | B7 | y ptop + 30 | lewo wersalikami z rozstrzeleniem 2 px: `OSTATNIE 7 NOCY · MEDIANA` (S2c: `SYPIALNIA OSTATNIEJ NOCY`); prawo `↔ jak zwykle` |
| Nazwa kolumny | B6 | x kx, y ptop + 68 | `czas z maską`, `bezdechy / h`, `EPAP`, `wyciek l/min`; S2c `temperatura`, `wilgotność`, `CO2 ppm`, `PM2.5` |
| Wartość + strzałka | B5 + ikona | x kx, y ptop + 104; ikona 8 px za liczbą | mediana 7 nocy; S2c mediana nocy |
| Słowo | B7 | x kx, y ptop + 132 | `lepiej niż zwykle`, `wyżej niż zwykle`, `niżej niż zwykle`, `uczę się`, S2c `zwykle {mediana}`; przy `jak zwykle` puste (mówi legenda), a w kolumnie wycieku powyżej 10 L/min `wyciek widoczny` (R-EKR-59) |
| Negatyw kolumny | prostokąt | x kx − 7, szer. 182, od (ptop + 68 − 26) do dołu paska | tylko `wyżej/niżej niż zwykle` i S2c poza pasmem; tekst biały, ta sama pozycja |

Wysokość paska: 142 ze słowami, 114 bez. Liczone y dla S2a (bez linii, ze stopką): podpis 88, liczby 171, zdanie 221, linia paska 280, tytuł 310, nazwy 348, wartości 384, słowa 412. Z linią 48 px: 136, 219, 269, pasek od 308 bez słów. Z linią A: 148, 231, 281, pasek od 308.

### S2b seria

| Element | Skala | Pozycja |
|---|---|---|
| Etykieta `SERIA` | B7 wersaliki | 28, top + 40 |
| `{n}.` | B2 | 28, top + 152 |
| `noc z rzędu` / `ponad 4 h` | B4 | koniec numeru + 20; górna linia na wersaliku numeru, dolna na jego linii bazowej |
| 30 kropek | koło r 10, pusta: biały środek r 7 (obwódka 3 px) | środki od x 38 co (744 − 20)/29, y = max(numer + 44, 372 ze stopką / 410 bez) |
| `30 nocy temu` / `dziś` | B7 | y kropek + 40 |

### Ekran ogólny: S5, S6, kamień

| Element | Skala | Pozycja |
|---|---|---|
| Ramka przerywana (S5) | 2 px, kreska 6, przerwa 6 | (12, 62 + h) – (788, 432); tekst od x 36 |
| Etykieta | B7 wersaliki | top + 40 |
| Tytuł | 1 linia (2 tylko wyjątkowo) | top + 91 |
| Duża liczba kamienia | B1 + podpis B4 obok (koniec + 20, ta sama linia bazowa) | top + 129 |
| Powód | najwyżej 2 linie co 36 | pierwsza linia tytuł + 50 |
| Postęp S6 | 14 pól (ramka 3 px / pełne), wys. 32 | y 336; podpisy B6 na 398 (lewo i prawo) |
| Fakt (S5, z kodu) | B7 | powód + 34 |
| Krok | B5 | 410 (w ramce S5: 408) |

## 5. Reguły ustępowania (gdy brakuje miejsca)

W kolejności; każdy krok tylko wtedy, gdy poprzedni nie wystarczył. Generator wypisuje, który zadziałał.

| # | Co ustępuje | Kiedy | Co zostaje |
|---|---|---|---|
| 0 | Stopka bez treści znika | pogoda i sypialnia puste | +44 px na dole |
| 1 | Pasek przykleja się do dołu, wolne miejsce idzie między zdanie a pasek | zawsze | stałe położenie wartości dzień po dniu |
| 2 | Rząd słów pod wartościami znika | linia uwagi (każda) przy obecnej stopce | strzałka, negatyw, legenda `↔ jak zwykle`, zdanie `Tydzień: różnice zaznaczone niżej.` |
| 3 | Odstęp linia uwagi → podpis nocy z 34 do 30 | linia A (60 px) | jak wyżej |
| 4 | Powód zawija się na 2 linie | S5, S6 przy powodzie > 744 px | krok zostaje na 410 |
| 5 | Tytuł na 2 linie | tylko gdy tytuł > 744 px (w katalogu nie ma takiego: najdłuższy `Nie rozumiem danych AirBridge` ma 611 px) | |

Nigdy nie ustępują: liczby nocy B1, podpisy liczb, zdanie B4, tytuł paska, linia uwagi, nagłówek z `ocena ograniczona`. Nie zmniejszamy też kroju liczb, bo dzień z linią to często dzień z gorszą nocą, czyli ten, kiedy liczby są najważniejsze.

Wynik na makietach: wszystkie stany, także najgorsze (`12 h 59`, `12,4`, `14,5`, `112`, nagłówek `czw 30.11` / `dane 23:59 · ocena ograniczona`, najdłuższa linia uwagi 60 znaków, linia wzorca A 53 znaki) mieszczą się bez ostrzeżeń. Rezerwa: najdłuższa linia zwykła (`Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij pomoc.`) ma 655 px z 744; linia A `Wciąż: Ciśnienie przy górnej granicy. Omówione 30.11.` w 28 px ma 662 px, więc R-EKR-42 (jedna linia na 760 px) jest spełnione bez korekty w etapie 3.

## 6. Szerokości najgorszych tekstów (zmierzone)

| Tekst | Skala | px | Limit |
|---|---|---|---|
| `12 h 59` + odstęp + `12,4` | B1 | 293 + 56 + 168 = 517 | 744 |
| `niedziela 27.12 · 12 h 59 · 12,4 / h` + `dane 23:59 · ocena ograniczona` (R-EKR-46) | B6 | 338 + 321 + 24 = 683 | 744 |
| `Tydzień: szczegóły w wierszach niżej.` (obecne) / `Tydzień: różnice zaznaczone niżej.` | B4 | 540 / 500 | 744 |
| `W normie. Tydzień: różnice zaznaczone niżej.` | B4 | 662 | 744 |
| `wyżej niż zwykle` | B7 | 171 | 173 w negatywie |
| `wyciek widoczny` (R-EKR-59, od 01.11.2026; tylko na bieli, kolumna w paśmie normy) | B7 (P22B) | 173,8 | 186 (kolumna); przy braku miejsca ustępuje jako pierwsze (spec 5.4.3, reguła (2)) |
| `Wyjazd z aparatem? Dane wrócą same. Sprawdzam co 3 h.` (od 02.11.2026, R-AW-24; wcześniej `… Ponawiam co 30 min.`, 734 px) | B5 | do przeliczenia (3 znaki krótszy) | 728 w ramce: do sprawdzenia, czy mieści się w 1 linii |
| `Do tego czasu pokazuję tylko sprawy, które nie wymagają normy.` | B5 | 781 | 2 linie |

## 7. Teksty i miejsca w spec do zmiany

Rodzaj zmiany według tabeli procesu: „treść wymagania, ekranu, tekstu komunikatu” (CHANGELOG, bez ADR). Żadna nie dotyka granic ADR-0002, ADR-0001, ADR-0007, ADR-0009.

| Miejsce w spec | Teraz | Propozycja B |
|---|---|---|
| 5.4.2 „Tytuł S2a i S2c”, R-EKR-41, R-EKR-32 | tytuł zdaniem `Noc: {czas}, AHI {a}.` / `W normie: {czas}, AHI {a}.` | werdykt jako blok: podpis `Ostatnia noc` + `{czas}`, podpis `AHI` + `{a}` (B1); `W normie` jako początek zdania: `W normie. Tydzień jak zwykle.` (ten sam warunek 5.4.13) |
| 5.4.2, R-EKR-41 (powód S2a) | `Tydzień: szczegóły w wierszach niżej.` | `Tydzień: różnice zaznaczone niżej.` (w B nie ma wierszy, jest pasek z negatywem) |
| 5.4.2 (powód S2c), kod `s2c()` | `Sypialnia: szczegóły w wierszach niżej.` | `Sypialnia: różnice zaznaczone niżej.` |
| 5.4.2 noc niepewna, R-EKR-46 | `Noc: {czas}.` bez AHI | blok z czasem; w miejscu AHI `nie oceniam:` / `duży wyciek` (B4) |
| 5.4.2 / 5.4.3 nagłówek, R-EKR-46 (test) | lewo `{dzień} {data} · {M-01} · {M-02} / h` | na S2a i S2c tylko `{dzień} {data}` (liczby są obok, duże); na S2b, S5, S6, kamieniu bez zmian |
| 5.4.2 S2a, artboard Main | etykieta `Twój tydzień`, 4 wiersze z nazwą, medianą, strzałką i słowem | brak etykiety; tytuł paska `Ostatnie 7 nocy · mediana`; słowo `jak zwykle` przeniesione do legendy; `wyżej/niżej niż zwykle` w negatywie |
| kolumny S2a (kod `s2a()`) | `Czas z maską`, `Bezdechy / h`, `EPAP`, `Wyciek` + wartość `8 l/min` | `czas z maską`, `bezdechy / h`, `EPAP`, `wyciek l/min` + wartość `8` (jednostka w nazwie) |
| 5.4.3 układ | nagłówek 48 px / 20 px; linia uwagi 44 px / 20 px; etykieta 18 px; tytuł 48 px; powód 24 px; krok 24 px; stopka 42 px / 18 px | nagłówek 52 / B6; linia 48 / B6 (A bez zmian: 60 / 28 px); etykieta B7 wersaliki; tytuł B3; powód i krok B5; stopka 46 / B7, znika przy braku treści |
| R-EKR-06 (test: nagłówek 48 px, stopka 42 px) | | 52 i 46 (tak już jest w kodzie: rozjazd kod–spec istniał wcześniej) |
| R-EKR-42 | „inaczej rozmiar do korekty w etapie 3” | sprawdzone: najdłuższa linia A w 28 px 662 px, mieści się |
| 5.4.2 S2b | `noc z rzędu` / `ponad 4 h` 40 px serif | B4 (34 px bez szeryfów, 700) |
| R-EKR-09 | minimum 18 px | na ekranach dnia minimum 22 px (R-EKR-09 zostaje jako minimum dla całego panelu) |

## 8. Rozjazdy z kanwą, kodem i spec (znalezione przy pracy)

| Co | Gdzie | Uwaga |
|---|---|---|
| Main, S2-linia-uwagi: wiersz `Sen`, `Ciśnienie`, `5 L`, zdanie o tygodniu w tytule | `design/kanwa/Main.dc.html`, `S2-linia-uwagi.dc.html` | już na liście 6.7; B zastępuje oba artboardy (do przerysowania z makiet `s2a-*`) |
| S2-linia-uwagi: tytuł `Tydzień w normie mimo jednej gorszej nocy.` | kanwa | tekst spoza katalogu; w B zdanie z 5.4.2 |
| Linia uwagi biała na czarnym 20 px regular | kod `rysujEkranDanych`, kanwa | cienkie białe litery na czarnym zalewa e-ink; w B 24/700 |
| Stopka S5 `ostatnie dane` (fakt) | kod `s5()` | tekst spoza katalogu 6.4 (R-EKR-05); w makiecie zostawiony B7, do decyzji przy etapie 3 |
| S6 `brak nowych nocy` / `norma po 14 nocach` | kod `s6()` | tekst spoza 5.4.8 / R-EKR-21; dopisać do spec albo usunąć z kodu |
| Kamień S-K-28 jako ekran danych | kod `kamien()` (układ OGOLNY, tytuł S46) | spec 5.8.3: forma „duża liczba”; w B duża liczba B1 + `nocy z maską`; artboard SK28 jest tylko kartą |
| Brak artboardu linii zaproszenia | 6.7 | makieta `s2a-linia-zaproszenia.png` |
| Nagłówek R-EKR-46 z pełną nazwą dnia (`niedziela 27.12`) | spec | kod pisze skrót (`nd`); w B mieszczą się obie formy |
| Najdłuższy czas | brief: `10 h 59`; spec R-EKR-41: `12 h 59` | makiety użyły `12 h 59` (ta sama szerokość) |
| Noc niepewna i linia E-20 | 7.5 | w makiecie `s2a-niepewna-linia` linia E-20 i AHI `nie oceniam` mówią to samo dwa razy; dozwolone (różne strefy), ale do oceny |
| `CO2` zamiast `CO₂` | stopka, S2c | glif `₂` jest w FONT_EXTRA; drobiazg typograficzny |

## 9. Czytelność z daleka (wersje 35%)

| Element | Z 1,5 m (35%) |
|---|---|
| Liczby nocy B1, numer serii B2, duża liczba kamienia | czytelne bez wysiłku |
| Zdanie B4, tytuł B3 | czytelne |
| Wartości paska B5, powód i krok B5, linia A | czytelne; negatyw kolumn widać od razu |
| Nagłówek, podpisy B6, linia zwykła | do odczytania z wysiłkiem |
| Legenda, słowa w pasku, stopka B7 (22 px) | nieczytelne z 1,5 m; to świadomie treść „z bliska” |

Wniosek: z łóżka widać odpowiedź na „jak minęła noc” (liczby), „jak tydzień” (zdanie) i „czy coś odstaje” (czarne pola w pasku). Szczegóły wymagają podejścia, co odpowiada R-NF-23 (werdykt czytelny z 1,5 m).

## 10. Lista makiet

| Plik | Stan |
|---|---|
| `s2a-zwykly` | S2a, dziś (noc `Noc:`) |
| `s2a-najgorsze-wartosci` | `12 h 59`, `12,4`, mediany dwucyfrowe, wszystkie słowa porównania, `ocena ograniczona` |
| `s2a-odchylenie` | AHI 4,1, kolumna bezdechów w negatywie, wyciek `lepiej` |
| `s2a-niepewna`, `s2a-niepewna-linia` | noc niepewna, AHI `nie oceniam`, z linią E-20 i bez |
| `s2a-bez-czasu` | czas `–`, EPAP `uczę się` |
| `s2a-w-normie`, `s2a-w-normie-najgorsze` | przyszły wariant `W normie` |
| `s2a-dane-z` | `dane z 29.10`, podpis `Noc 29.10` (stan ze zdjęcia) |
| `s2a-linia-najdluzsza` | linia 60 znaków + najgorsze wartości |
| `s2a-linia-zaproszenia` | `Co było inaczej? ↻ 2 s` po nocy z odchyleniem |
| `s2a-linia-wzorca-A` | pasek 60 px, 28 px, najdłuższa linia R-LEK-24 |
| `s2c-sypialnia`, `s2c-bez-qingping` | 4 kolumny z najgorszymi wartościami; 2 kolumny, stopka tylko z pogodą |
| `s2b-seria`, `s2b-seria-linia` | seria 1000 (4 cyfry); seria 12 z linią E-04 |
| `s6-rozruch-dzien1`, `s6-rozruch-najdluzszy`, `s6-brak-nowych` | rozruch |
| `kamien-linia`, `kamien-rocznica` | S-K-28 jako ekran danych (dzień z linią E-05, R-HUM-17); rocznica |
| `s5-e01`, `s5-e01-3noc`, `s5-e03` | brak danych: 2. noc (najdłuższy powód), 3. noc (krok z ◀), E-03 |
