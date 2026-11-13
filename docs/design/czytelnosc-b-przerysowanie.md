# Kanwa do przerysowania po skali B czytelności (30.10.2026)

Decyzja 30.10.2026: wariant B ekranu dnia (skala B) i K2 w opcji A. Prototyp rysuje już według niej wszystkie ekrany (README prototypu, sekcja „Czytelność, skala B”); spec: 5.4.2, 5.4.3, 5.5.2, 5.5.3, 5.6.1 pkt 3, 5.7.2, 5.8.11, 5.11.4. Kanwa „Ekran CPAP: interfejs docelowy v1” jest od tej chwili w tych miejscach starsza niż spec i prototyp. Ta notatka mówi, które artboardy trzeba przerysować i według czego.

Wzorem są makiety z generatorów (1 bit, 800×480, tekst na linii bazowej, współrzędne jak w `src/ekrany.cpp`): ekrany dnia według specyfikacji projektowej wariantu B (makiety `s2a-*`, `s2b-*`, `s2c-*`, `s5-*`, `s6-*`, `kamien-*`), kokpit i reszta według specyfikacji kokpitu (makiety `k1`, `k2`, `pytanie`, `noc-a1`…`noc-a3`, `karta-k22`, `karta-s5`, `test`, `stopki`). Najprościej: zrzut podglądu prototypu (`tools/prototyp/testy.sh`, pliki `mak-*.png`), bo rysuje te same stany tym samym kodem co panel.

## Po audycie UX (30.10.2026, decyzja)

Te zmiany obowiązują przy przerysowaniu każdego artboardu (spec 1.5, 5.4.2, 5.4.3, 5.4.5, 5.5, 5.7.2, 5.11.4); wzorem jest podgląd prototypu `mak-<stan>.png` z `tools/prototyp/testy.sh`, a projekt skali B jest w [czytelnosc-b/](czytelnosc-b/):

- jedna legenda: ramka 2 px = poza twoją normą, czarne pole tylko „ponad próg” (K2) i linie uwagi grupy „zdrowie”;
- linia zaproszenia `Co było inaczej? ↻ 2 s` i inne linie poza grupą „zdrowie”: biały pasek w ramce, bez `!`;
- `AHI` jako jedyna nazwa M-02; bez `P95` na ekranach dnia i kartach;
- nagłówek ekranów dnia i kart `{dzień} {data}` bez liczb nocy, skróty dni `pn wt śr cz pt sb nd`;
- stopka ekranów dnia, kart i S9 `przy łóżku {t}° · {w}%` (czujnik panelu), `sypialnia` tylko dla Qingping;
- K22: tytuł `W normie.` (warunek uspokojenia), inaczej `Noc: {czas} z maską.`, bez czasu `Noc zapisana.`; każda liczba nocy raz; K1: wiersze 50 px; K2: nagłówek `Ostatnie 7 nocy · {zakres}` · `2 / 2`, romb nad nagłówkiem kolumny;
- pytanie: opis nocy pod etykietą, podpowiedź o terminie z prawej w rzędzie etykiety, stopka `↻ zaznacz · 2 s zapisz`.

## Skala B (wspólna dla wszystkich artboardów)

| Nazwa | Krój | Do czego |
|---|---|---|
| B1 | Source Serif 4, 96 px, 600 | liczby ostatniej nocy, duża liczba kamienia |
| B2 | Source Serif 4, 120 px, 400 | numer serii S2b |
| B3 | Source Serif 4, 44 px, 700 | tytuły |
| B4 | Public Sans 34 px, 700 | zdanie o tygodniu i sypialni, `noc z rzędu`, podpis dużej liczby |
| B5 | Public Sans 28 px, 700 | wartości paska, powód, krok, linia wzorca A, mediany K1, fakty kart |
| B6 | Public Sans 24 px, 700 | nagłówek, podpisy liczb, nazwy kolumn, linia uwagi, nazwy wierszy K1 |
| B7 | Public Sans 22 px, 600 | minimum: etykiety (wersaliki, rozstrzelenie 2 px), legenda, słowa, stopki, tabela K2, pytanie |
| (K1) | Source Serif 4, 56 px, 600 | 4 liczby ostatniej nocy w K1 |

Bez kursywy, bez IBM Plex, nic poniżej 22 px. Strzałki ↔ ↑ ↓ rysowane jako ikony (wysokość wersalika kroju, grubość px/11), nie z kroju. Stopka z akcjami: jedna grupa na przycisk w kolejności ◀ ↻ ▶, `2 s` zamiast „przytrzymaj” (5.6.1 pkt 3).

## Artboardy do przerysowania

| Artboard | Co zmienić | Makieta wzorcowa |
|---|---|---|
| `Main`, `F02-spokojny-tydzien` | zamiast tytułu i 4 wierszy: blok liczb ostatniej nocy (`Ostatnia noc` / `AHI`), zdanie B4, pasek `OSTATNIE 7 NOCY · MEDIANA` z legendą `↔ jak zwykle`, kolumna `wyżej/niżej niż zwykle` w ramce 2 px ze słowem (audyt UX), kolumna `AHI` (nie `bezdechy / h`); bez etykiety `Twój tydzień`, bez wiersza `Sen`, `Ciśnienie`, `5 L`; nagłówek tylko `{dzień} {data}` | `s2a-zwykly`, `s2a-odchylenie`, `s2a-najgorsze-wartosci`, `s2a-w-normie`, `s2a-dane-z`, `s2a-bez-czasu`, `s2a-niepewna` |
| `S2-linia-uwagi` | linia 48 px, 24 px, 700 (wzorca A 60 px, 28 px, 700); tytuł `Tydzień w normie mimo jednej gorszej nocy.` (spoza katalogu) zastąpiony zdaniem z 5.4.2; bez rzędu słów w pasku | `s2a-linia-najdluzsza`, `s2a-linia-wzorca-A`, `s2a-niepewna-linia` |
| brak (linia zaproszenia, 6.7) | nowy artboard: linia `Co było inaczej? ↻ 2 s` po nocy z odchyleniem | `s2a-linia-zaproszenia` |
| `S2-sypialnia` | blok liczb nocy jak S2a, pasek `PRZY ŁÓŻKU · OSTATNIA NOC`, słowo `zwykle {mediana}` i ramka poza pasmem, zdanie `Przy łóżku: różnice niżej.`; wariant bez Qingping (2 kolumny) | `s2c-sypialnia`, `s2c-bez-qingping` |
| `S2-seria` | `noc z rzędu` / `ponad 4 h` Public Sans 34/700 zamiast szeryfu 40 px | `s2b-seria`, `s2b-seria-linia` |
| `S6-rozruch` | tytuł B3, powód B5, pola postępu z ramką 3 px, podpisy B6 | `s6-rozruch-dzien1`, `s6-rozruch-najdluzszy`, `s6-brak-nowych` |
| `SK28` (tylko karta) | dodać wariant ekranu danych: duża liczba B1 i `nocy z maską` (forma „duża liczba”, 5.8.3) | `kamien-linia`, `kamien-rocznica` |
| `K16` / ekran S5 | ekran danych: tytuł B3, powód B5 w 2 liniach, krok B5, stopka `↻ ponów` | `s5-e01`, `s5-e01-3noc`, `s5-e03` |
| `K1` | opis 22 px nad liczbą, liczby 56/600, nagłówki kolumn `Ostatnie 30 nocy` · `mediana` · `ocena`, 5 wierszy po 48 px (ASV: `EPAP`, `Wsparcie`), wiersz w stanie odchylenia w ramce 2 px z oceną w 2 liniach, `AHI` zamiast `Bezdechy`, stopka z akcjami | `k1` |
| `F07-kokpit` | Public Sans 22/600 zamiast Plexa, bez kolumny `30 nocy` (opcja A), 10 wierszy po 28 px, etykiety bez `cmH₂O` (`Wyciek l/min`, `EPAP P95`, `Wsparcie PS`), legenda prosto bez `mediana normy`, `–` na środku komórki, stopka wzorem 5.6.1 pkt 3; zdanie K2 na makiecie dowolne | `k2` |
| `F08-pytanie` | pozycje Public Sans 22/600, zaznaczenie bez pogrubienia, kursor w ramce 3 px, podtytuł `Zaznacz wszystko, co pasuje.`, podpowiedź `Bez odpowiedzi pytanie zniknie o {hh:mm}.` (bez kursywy), stopka `◀ wstecz · 2 s pomiń` · `↻ zaznacz · 2 s zapisz ({k})` · `▶ dalej` | `pytanie` |
| `K22-dobra-noc`, `EkranD` (i wszystkie karty) | tytuł SB 44/700, fakty 28/700 nad 22/600, etykieta 22 px, zdanie budzika (wycofane z ekranu 2026-11-10) 22/600 w cudzysłowie bez kursywy | `karta-k22`, `karta-s5` |
| brak (S9 A1, A2, A3) | nowe artboardy według 5.8.11 w kroju skali B: A1 bez prawej strony nagłówka, fakt `zwykle z maską`, krok `→ Ekran zostaje do rana, żeby nie błyskać.`; A3 linia `Ekran zostaje do rana, żeby nie błyskać.` | `noc-a1`, `noc-a1-bez-godziny`, `noc-a2`, `noc-a3`, `noc-a3-bez-godziny` |
| wszystkie stopki z akcjami | wzór 5.6.1 pkt 3 | `stopki` |

## Do decyzji przy przerysowaniu (nie blokują)

- S5: linia `ostatnie dane: {data godzina}` jest spoza katalogu 6.4 (do decyzji w etapie 3).
- S6: `brak nowych nocy` / `norma po 14 nocach` spoza 5.4.8 i R-EKR-21: dopisać do spec albo usunąć z kodu.
- Noc niepewna z linią E-20: linia i `nie oceniam: duży wyciek` mówią to samo w dwóch strefach (dozwolone, do oceny).
- `CO2` zamiast `CO₂` w stopce i S2c (glif jest, drobiazg typograficzny).

## Po audycie UX 10.11.2026

Rozbieżności prototypu z kanwą do przerysowania (prototyp i spec są już po zmianie):

- **wszystkie karty z budzikiem (K01…K22, K05, K16, przerywniki K17…K20) i S9 A1, A2, A3:** bez zdania budzika; lewa część stopki karty jak na ekranie danych (pogoda albo podpowiedź, albo podpis akcji), prawa bez zmian; S9 A2 bez linii pod `Dobranoc.` (spec 5.7.2, 5.7.5, 5.8.11; ADR-0009, ADR-0018 „Zmiana 2026-11-10”);
- **K1:** wiersz ze spokojnym trendem ma sam znak `↔`, legenda `↔ stabilnie` z prawej w wierszu nagłówków kolumn (R-KOK-17); **zastąpione 13.11.2026:** K1 do przerysowania według R-KOK-17, R-KOK-25…R-KOK-28 (znak i słowo w każdym wierszu, bez legendy, linia 30 nocy 270 px, ocena od x 570, ` · dane {hh:mm}` w nagłówku, `duży wyciek` nad liczbą wycieku przy nocy niepewnej; wzorzec: podgląd `mak-k1.png` z `tools/prototyp/testy.sh`);
- **„Sprzęt” (artboard `Sprzet`):** kolumny `element` · `wiek` · `następna wymiana` (od x 28, 180, 300), wartość `15.01.2027 · co 6 mies.`, bez daty `brak daty · co 4 tyg.` i podpowiedź pod tabelą; stopka na pierwszym wierszu `◀ norma` (R-MAS-15);
- **U-01 „Ustawienia panelu”** (bez artboardu; makieta `docs/design/ekrany-2026-11-03`): lista bieżących ustawień do odczytu zamiast pozycji z kursorem (R-KONF-55);
- **SK33-okno, SK36-pylek, SK37-slonce:** wariant z linią uwagi z niższym wykresem (słupki 66 px, pas doby 30 px); podpisy osi `HH:00` (R-KON-63);
- **K05 (S-K-01):** powód `Powietrze w sypialni jest teraz zastałe.` bez liczby (5.8.4).

Po decyzjach właściciela z 10.11.2026:

- **S8-norma*.dc.html (S8 otwarte jako strona `4 / 6`):** nagłówek `Twoja norma` · `4 / 6` zamiast daty, bez tytułu `Twoja norma` w treści, `Odchylenia liczę od tego.` bez `→` (5.4.8, R-EKR-35); kanwy pokazują jeszcze wariant „z K1” z datą i tytułem; S8 jako ekran dnia (kamień) bez zmian;
- **P-przeniesione*.dc.html:** tekst `Do tego czasu przypomnę na pasku u góry: {tytuł}.` poprawiony w kanwie 10.11.2026 bez przerysowania układu (szerokość linii do sprawdzenia przy przerysowaniu);
- **X-otwarte-sprawy-pusto*.dc.html:** przy ocenie niepełnej (`częściowa`, `ograniczona`) tytuł `Nie widzę otwartych spraw.`, powód `W tym, co oceniam, nic nie jest w toku.`, zdanie `Części rzeczy jeszcze nie oceniam.` (R-EKR-50); przy `pełna` bez zmian; karta sprawy z paska: `Koniec: gdy dane przestaną to pokazywać`;
- **SK35-wymiana:** krok `→ Po wymianie: strona 5 / 6, ramka na filtrze, ◀ 2 s.` (tekst w kanwie poprawiony);
- **F07-kokpit (K2):** wariant z wierszami bez danych zwiniętymi do linii `brak danych: zdjęcia maski, oddechy` pod tabelą, nad legendą (R-KOK-18); makieta z prototypu: symulator, strona `2 / 6` na zrzucie z nocami bez M-13 i M-17.
