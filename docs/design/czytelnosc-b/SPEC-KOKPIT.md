# Czytelny kokpit i ekrany pomocnicze: specyfikacja makiet

> **Stan dokumentu.** Projekt skali B z 30.10.2026. Makiety 1-bit: [`makiety/`](makiety/) (wybrane stany; wygenerowane narzędziem spoza repozytorium). Po audycie UX z 30.10.2026 (decyzja) obowiązuje spec, a nie ten plik, tam gdzie się różnią: ramka zamiast negatywu dla „poza normą” (kolumna paska, wiersz K1, etykieta K2), linia zaproszenia jako biały pasek w ramce, `AHI` jako jedyna nazwa M-02, nagłówek `{dzień} {data}` bez liczb, stopka `przy łóżku {t}° · {w}%`, S2c `Przy łóżku · ostatnia noc`, K2 z nagłówkiem `2 / 2` (spec 5.4.2, 5.4.3, 5.5).


Dotyczy: K1, K2, pytanie „Co było inaczej?” (F08), S9 „Dobranoc” (A1, A2, A3), stopki z akcjami, test sprzętu, teksty kart.
Nie dotyczy: ekranów dnia (projekt ekranów dnia: [SPEC-B.md](SPEC-B.md)), obrazów budzika, mapy przycisków.
Makiety: [`makiety/`](makiety/) (wybrane ekrany; generator poza repo; ten sam układ rysuje prototyp, podgląd `mak-<ekran>.png` z `tools/prototyp/testy.sh`).
Dane w makietach to najgorsze przypadki z zadania (AHI 12,4, czas 10:59, wyciek 112, EPAP 14,5, PS 10,0, normy 10,5–12,3 i 9:48–10:59, najdłuższa etykieta czynnika, najdłuższe zdanie budzika (wycofane z ekranu 2026-11-10) 44 zn.).

## 1. Skala typograficzna (założenia)

Skala zgodna z [SPEC-B.md](SPEC-B.md) (skala B, nazwy). Kokpit używa B3, B5, B6, B7 bez zmian i dokłada jeden krój: SN56 (liczby K1; B1 = SN96 jest za szeroki na 4 liczby w rzędzie). B4 (P34B) i B1 (SN96) są tylko na ekranach dnia.

| Rola | Krój | px | Waga | Nazwa w `fonty.h` | Stan |
|---|---|---|---|---|---|
| tekst podstawowy, podpisy, stopki, etykiety (wersaliki, rozstrzelenie 2 px), cała tabela K2 | Public Sans | 22 | 600 | `P22B` | jest |
| nagłówek ekranu (lewa strona), nazwy wierszy K1, nagłówek ostatniej nocy K2 | Public Sans | 24 | 700 | `P24B` | nowy, wspólny ze skalą B |
| wartości (mediany K1, fakty kart i S9, linia A3, tytuł O-12) | Public Sans | 28 | 700 | `P28B` | nowy, wspólny ze skalą B |
| zdanie K2 | Source Serif 4 | 24 | 700 | `SB24` | jest |
| tytuły (karta, pytanie, A1, A2, test) | Source Serif 4 | 44 | 700 | `SB44` | jest |
| 4 liczby ostatniej nocy w K1 | Source Serif 4 | 56 | 600 | `SN56` | nowy, podzbiór `0123456789,:–h` + spacja (nazwa jak SN96 w SPEC-B) |
| godzina A3 / `Dobranoc.` A3 | Source Serif 4 | 140 / 110 | 400 | `S140` / `S110` | są, bez zmian (kreska ≥ 10 px) |

Reguły: nic poniżej 22 px; poniżej 30 px tylko 600 i więcej; brak kursywy (cienka kreska kursywy znika w 1-bit, zdjęcie 30.10); szeryf 400 tylko od 110 px.

## 2. Elementy wspólne

| Element | Wymiar | Treść i reguły |
|---|---|---|
| Marginesy | 28 px (K2: tabela 20 px) | karty i S9 26 px jak dotąd (ADR-0009) |
| Nagłówek | ekran danych (K1, test): 0–52, linia 2 px na 52, linia bazowa 35 (jak SPEC-B); karta, S9, pytanie: linia na 46, bazowa 32–33 (ADR-0009) | lewo `P24B`, prawo `P22B` |
| Etykieta | `P22B`, wersaliki, rozstrzelenie 2 px | zamiast 18 px 400 |
| Stopka | linia 2 px na 434, tekst `P22B` na linii bazowej 465 (jak SPEC-B) | patrz 2.1 |
| Ikona przycisku | kwadrat w negatywie, bok = wysokość wersalika + 8 px (26 px przy `P22B`), min. 24 px | ◀ ▶ trójkąt, ↻ łuk 2,4 px ze strzałką; ◆ romb bez ramki |
| Strzałka `→` w kroku (A1, karty) | w makiecie z kroju zapasowego (szeryf), jak robi dziś `generuj.py` | wychodzi cienka; według SPEC-B strzałki rysować prymitywem jak ↔ ↑ ↓ |

### 2.1 Stopka z akcjami (jeden wzór dla wszystkich ekranów)

- Jedna grupa na przycisk, zawsze w kolejności **◀, ↻, ▶**; odstęp między grupami 26 px.
- Grupa: ikona, akcja krótkiego naciśnięcia, potem ` · 2 s ` i akcja przytrzymania, np. `◀ ostatnia noc · 2 s ustawienia`. Przycisk z samym przytrzymaniem: `◀ 2 s norma`.
- „2 s” zastępuje słowo „przytrzymaj”: krócej o 9 znaków na akcję i ta sama notacja co linia zaproszenia `Co było inaczej? ↻ 2 s` (5.11.3). Bez tego K2 nie mieści się w jednej linii 22 px.
- Bez zdania budzika grupy idą od lewego marginesu; ze zdaniem budzika (karta, S9) zdanie z lewej, grupy wyrównane do prawej. Zmiana 2026-11-10: zdanie budzika wycofane z ekranu (spec 5.7.5); karta i S9 mają z lewej to, co ekran danych (albo nic), z prawej odczyt albo akcję.
- Podpisy zgodne z mapą 5.6.2 (R-PRZ-04); mapa bez zmian. Prototyp nie ma jeszcze `▶ sprawy`, `◀ 2 s norma` (K1), `↻ 2 s sprzęt`, `▶ raport` (K2): do czasu ich wdrożenia stopka pokazuje tylko działające akcje, żeby nie podpisywać rzeczy, które nic nie robią.

| Ekran | Stopka | Szerokość / 744 px |
|---|---|---|
| K1 | `◀ 2 s norma` · `↻ ekran dnia · 2 s co było inaczej` · `▶ sprawy` | 649 |
| K2 | `◀ ostatnia noc · 2 s ustawienia` · `↻ ekran dnia · 2 s sprzęt` · `▶ raport` | 741 (limit) |
| pytanie | `◀ wstecz · 2 s pomiń` · `↻ zaznacz · 2 s zapisz ({k})` · `▶ dalej` | 629 |
| test | `◀ kokpit 7 nocy` · `↻ pobierz teraz` · `▶ ekran dnia` | 543 |
| Zapisano | `◀ 2 s cofnij zapis` | – |
| karta S5 | (do 10.11.2026 zdanie budzika) · `↻ ponów` | – |
| S9 A1, A3 | (do 10.11.2026 zdanie budzika) · `▶ kokpit` | – |

Arkusz: `makiety/stopki.png`.

## 3. K1 (ostatnia noc i 30 nocy)

Makiety: `k1.png` (tryb ASV, 5 wierszy, wiersz w negatywie, `EPAP (urządzenie)`), `k1-odkrycie.png` (O-12).

| Strefa | y (px) | Element | Krój | Uwagi |
|---|---|---|---|---|
| Nagłówek | 0–52 | `Ostatnia noc · czw 12.11 ◆` · `1 / 2` | P24B / P22B | |
| 4 liczby | 54–154 | opis **nad** liczbą: `czas z maską`, `bezdechy / h`, `EPAP` (`EPAP (urządzenie)`), `wyciek l/min` | P22B, linia bazowa 82 | kolumny od x = 28, 222, 398, 608 (nierówne: `EPAP (urządzenie)` ma 190 px) |
| | | liczba | SN56, linia bazowa 140 | `10:59` 150 px |
| Linia | 154 | 2 px | | |
| Nagłówki kolumn | 156–188 | `Ostatnie 30 nocy` · `mediana` (do prawej, x 506) · `ocena` (x 522) · legenda `↔ stabilnie` (do prawej, x 772; zmiana 2026-11-10) | P22B, l. bazowa 181 | rozstrzyga, co jest nocą (góra), a co medianą 30 nocy (dół); ten sam pomysł co tytuł paska `OSTATNIE 7 NOCY · MEDIANA` w SPEC-B; legenda jak `↔ jak zwykle` w pasku ekranu dnia |
| 5 wierszy | 188–428, 48 px | nazwa | P24B, x 28 | `Czas z maską` 155 px |
| | | linia 30 nocy | 210×32 px, 3 px, przerywana mediana normy | x 196–406; przerwy w nocach bez danych (R-KOK-02) |
| | | mediana 30 nocy | P28B, do prawej x 506 | |
| | | ocena | P22B, x 522, do 772 | negatyw (R-KOK-03): 2 linie `wyżej niż zwykle` / `od {n} nocy` (290 px w 1 linii nie mieści się w 250); trend `stabilnie` jako sam znak `↔` (słowo w legendzie nagłówka; zmiana 2026-11-10) |
| Stopka | 434–480 | 2.1 | | |

O-12 (`k1-odkrycie.png`): tytuł `P28B` i powód `P22B` stoją w miejscu 4 liczb, a czas i AHI nocy przechodzą do nagłówka (`Ostatnia noc · czw 12.11 · 10 h 59 · 12,4 / h ◆`). Powód: przy 22 px nie ma miejsca na oba bloki.

## 4. K2 (7 nocy): decyzja

**Problem.** Ograniczenie jest w szerokości, nie w wysokości. 7 kolumn × `10:59` (57 px przy 22/600) + etykieta (`Oddechy /min` 144 px) + norma (`9:48–10:59` 115 px) = 744 px, czyli cała szerokość. Na linię 30 nocy zostaje 0 px. Większe cyfry (24–28 px) nie mieszczą się w 7 kolumnach przy żadnym podziale na strony.

| Opcja | Co | Za | Przeciw | Ocena |
|---|---|---|---|---|
| **A (rekomendacja)** `k2.png` | 1 strona, 10 wierszy po 28 px, wszystko 22/600, bez kolumny `30 nocy`, legenda 22/600, marginesy tabeli 20 px | cały tydzień jednym spojrzeniem; mapa bez zmian; zero nowych krojów poza wspólnymi | trend 30 nocy znika z K2 (jest w K1 dla 5 głównych metryk; dla OA, CA, H, zdjęć maski i oddechów nie ma go nigdzie na panelu); jednostki cmH₂O i `P95` przy wycieku wypadają z etykiet | najlepszy stosunek zysku do kosztu |
| B `k2-opcja-B-plex-z-linia.png` | Plex Sans Condensed Bold 22, linia 30 nocy 80 px | zostaje linia 30 nocy | cyfry Plexa prawie nie są węższe (`10:59` 54 px zamiast 57): przy kolumnie 57 px `10:38` wchodzi na ramkę sąsiedniej komórki; wszystko pogrubione 700, więc ostatnia noc i ramki słabiej odstają; linia 80 px dla 30 nocy to 2,7 px na noc (szum z 1,5 m); nowy krój C22B 3,4 KB | odrzucam: nie przechodzi najgorszego przypadku |
| C `k2-opcja-C-strona-1.png`, `-2.png` | 2 strony (bezdechy i czas / maska i ciśnienie), wiersze 44 px | więcej powietrza | cyfry nadal 22 px (szerokość ta sama), więc zysk czytelności mały; tydzień w dwóch kawałkach; trzeba wolnego przycisku do zmiany strony, a w K2 wszystkie są zajęte (zmiana mapy: np. ▶ na stronie 1 = strona 2, raport dopiero ze strony 2); dodatkowe odświeżenie e-ink | odrzucam |
| D (bez makiety) | 5 nocy zamiast 7 | +136 px: linia 30 nocy wraca albo cyfry 24 px | to już nie jest „7 nocy”, zmiana R-KOK-06 i zdania podsumowania | odrzucam |
| E (bez makiety) | skróty etykiet (`obtur.`, `centr.`, `spłyc.`) | +30 px | skróty czyta się gorzej niż pełne słowa, a 30 px nie wystarcza na linię 30 nocy | odrzucam |

**Siatka opcji A** (x w px):

| Kolumna | x | Krój | Uwagi |
|---|---|---|---|
| etykieta | 20–164 | P22B | wcięte (OA, CA, H) +14 px; negatyw etykiety (R-KOK-09) |
| 7 nocy | 168–644, co 68 px | P22B do prawej, 5 px od prawej krawędzi komórki | komórka 64×26: negatyw = ponad próg, ramka 2 px = poza normą; brak danych `–` **na środku komórki** (inaczej przy ostatniej kolumnie czyta się jak minus przy normie) |
| ostatnia noc | 576–644 | nagłówek P24B | wyróżnienie bez znaczenia kolorystycznego: większy nagłówek i pionowa linia 1 px przed kolumną (negatyw i ramka już coś znaczą) |
| norma | 647–780, do prawej | P22B | pionowa linia 1 px przed kolumną |

| Strefa | y | Element | Krój |
|---|---|---|---|
| Winieta | 0–46 | `Kokpit · 7 nocy` · `noce 6–12 listopada` (małe litery) | P24B / P22B |
| Zdanie | 74 | zdanie podsumowania | SB24 |
| Nagłówek tabeli | 110 (linia 3 px na 117) | dni nocy, `◆` przy nocy z czynnikiem, `norma` | P22B, ostatnia noc P24B |
| Wiersze | 119–403 | 10 × 28 px + 2 linie grup 1 px (po `spłycenia`, po `Zdjęcia maski`) | P22B |
| Linia `brak danych: …` (od 10.11.2026, R-KOK-18) | 21 px pod ostatnim wierszem, tylko gdy są wiersze bez danych w całym tygodniu (zwinięte z tabeli) | P22B |
| Legenda | 423 | czarny prostokąt `ponad próg` · ramka `poza Twoją normą` | P22B, bez kursywy, bez `mediana normy`; przy linii `brak danych` 28 px niżej, a tabela krótsza o 28 px na każdy zwinięty wiersz |
| Stopka | 434–480 | 2.1 | P22B |

Wiersze dodatkowe z oksymetrem (R-KOK-11, 12 wierszy) w opcji A się nie mieszczą (zostaje 0 px). Gdy oksymetr wejdzie do wydania, wtedy wraca pytanie o drugą stronę; dziś nie ma po co jej budować.

Uwaga wdrożeniowa: cyfry Public Sans są proporcjonalne (`1` ma 8,5 px, inne 12–13,7 px), więc przecinki w kolumnie nie stoją równo. Generator może brać glify cyfr z cechy `tnum` kroju (fontTools); to sprawa implementacji, nie decyzja.

## 5. Pytanie „Co było inaczej?” (F08)

Makieta: `pytanie.png` (zaznaczone `alkohol` i leki, kursor na `nic szczególnego`, opis z dwiema metrykami, termin 23:30).

| Strefa | y | Element | Krój | Uwagi |
|---|---|---|---|---|
| Winieta | 0–46 | `Pytanie poranne` · `poniedziałek, 30 listopada` | P24B / P22B | data małymi literami |
| Powód | 60–90 | ramka 2 px z `INNA NOC` (rozstrz. 1 px) + opis | P22B | `AHI 12,4 przy zwykłych 10,5–12,3, maska zdjęta 12 razy` 569/595 px |
| Tytuł | 136 | `Co było wczoraj inaczej?` | SB44 | |
| Podtytuł | 168 | `Zaznacz wszystko, co pasuje.` | P22B | skrócone (patrz 7) |
| Siatka | 181–389 | 2 kolumny (x 28–404, 412–772), 4 wiersze po 52 px | P22B | pole 28 px (ramka 3 px; zaznaczone: negatyw z białym ptaszkiem 4 px); kursor: ramka 3 px 50 px wysokości; etykieta leków w 2 liniach tym samym krojem (278 i 260 px) zamiast węższego Plexa 18 px |
| Podpowiedź | 412 | `Bez odpowiedzi pytanie zniknie o {hh:mm}.` | P22B | „pomiń” przechodzi do stopki |
| Stopka | 434–480 | 2.1 | | |

Zaznaczona pozycja nie dostaje pogrubienia etykiety (wszystko jest już 600); sygnałem jest czarne pole z ptaszkiem, widoczne z daleka lepiej niż różnica wagi.

## 6. S9 „Dobranoc”

Makiety: `noc-a1.png`, `noc-a1-bez-godziny.png`, `noc-a2.png`, `noc-a3.png`, `noc-a3-bez-godziny.png`. Gdy AirBridge nie podaje startu sesji na żywo, panel rysuje wersje **bez godziny**.

| Wariant | Strefa | Element | Krój | Uwagi |
|---|---|---|---|---|
| A1 | nagłówek | `30.10 · maska od 22:47`; prawa strona pusta | P24B | `panel 21° · 45%` usunięte: to samo stoi większe w faktach |
| | etykieta | `DOBRANOC` | P22B rozstrz. 2 | |
| | tytuł | `Śpij dobrze.` | SB44, l. bazowa 134 | |
| | powód | `Maska założona o 22:47.` / `Wynik nocy pokażę rano.` | P22B, co 29 px | |
| | fakty | `21°` temperatura · `45%` wilgotność · `10 h 59` zwykle z maską | P28B nad P22B | 450/474 px |
| | krok | `→ Ekran zostaje do rana, żeby nie błyskać.` | P22B, l. bazowa 412 | 431/474 px, 1 linia; `→` z kroju zapasowego (szeryf), jak w generuj.py |
| | stopka | zdanie budzika (wycofane z ekranu 2026-11-10) w cudzysłowie, bez kursywy · `▶ kokpit` | P22B | najdłuższe 44 zn. 511 px |
| A2 | scena | bez zmian, 0–338 | | |
| | tytuł | `Dobranoc.` wyśrodkowany | SB44, l. bazowa 382 | |
| | zdanie | `„Szach-mat. Księżyc się zamyślił.”` | P22B, 418 | |
| | linia | `maska od 22:47 · panel 21° · 45% · wynik rano` | P22B, 452 | |
| A3 | nagłówek | `Noc z poniedziałku na wtorek` · `panel 21° · 45%` | P24B / P22B | najdłuższa nazwa dnia |
| | etykieta | `MASKA ZAŁOŻONA O` / `NA DOBRANOC` | P22B rozstrz. 2 | |
| | godzina | `22:47` / `Dobranoc.` | S140 / S110 | bez zmian |
| | linia 1 | `Dobranoc. Wynik nocy rano.` / `Wynik nocy rano.` | P28B, 290 | |
| | linia 2 | `Ekran zostaje do rana, żeby nie błyskać.` | P22B, 324 | 411/500 px |
| | stopka | jak A1 | | |

Budzik zawsze na bieli (ADR-0009); sceny wklejone 1:1, w tych samych miejscach.

## 7. Test sprzętu i karty

Test (`test.png`): nagłówek jak ekran danych (0–52, P24B / P22B), etykieta `TEST SPRZĘTU` P22B na 88, tytuł SB44 na 134, 7 linii P22B co 35 px od 172 (najdłuższa 640 px), stopka 2.1 w kolejności ◀ ↻ ▶.

Karty (`karta-k22.png`, `karta-s5.png`; układ ADR-0009 bez zmian, zmieniają się tylko kroje):

| Strefa karty (5.7.2) | Było | Jest |
|---|---|---|
| nagłówek | Public Sans 20 (600 / 400) | P24B / P22B |
| etykieta | 18 px 400 | P22B rozstrz. 2 |
| tytuł | Source Serif 4 46 px 400 | SB44 (700); `Brak danych z nocy 12.11` 487/748 px |
| powód | 22 px 400 | P22B (szerszy o ok. 5 %: limit 2 linii w 474 px sprawdzać tym krojem, `kartaMiesci`) |
| fakty | liczba szeryf 32 px 400, podpis 18 px | liczba P28B, podpis P22B |
| krok | P22B | bez zmian |
| stopka | zdanie kursywą 18 px 400 | P22B w cudzysłowie, bez kursywy; 44 zn. + `sypialnia 21° · 46%` = 725/744 px |

Ryzyko dla drugiego projektanta: jeśli prawa strona stopki karty urośnie (np. `sypialnia 21° · 45% · CO2 820` z jego pomiarów, ok. 300 px), zdanie budzika (wycofane z ekranu 2026-11-10) musi mieć ≤ 36 znaków albo prawa strona traci CO2.

## 8. Teksty do zmiany w specyfikacji (brzmienie i miejsce)

| # | Miejsce w spec | Było | Ma być | Powód |
|---|---|---|---|---|
| T1 | 5.5.2, wiersz „4 liczby” | `godz. na masce`, opis 18 px pod liczbą | `czas z maską`, opis 22 px nad liczbą | ta sama nazwa co wiersz `Czas z maską`; układ wariantu B |
| T2 | 5.5.2, nowy wiersz tabeli | (brak) | nagłówki kolumn `Ostatnie 30 nocy` · `mediana` · `ocena` | jednoznaczne: noc u góry, mediana 30 nocy niżej |
| T3 | 5.5.2, „Negatyw wiersza”; R-KOK-03 | jedna linia | ta sama treść w 2 liniach: `wyżej niż zwykle` / `od {n} nocy` | 290 px nie mieści się w 250 |
| T4 | 5.5.3, „Winieta” | prawo wersalikami 18 px | `noce 6–12 listopada` małymi, 22 px | wersaliki z rozstrzeleniem czyta się wolniej |
| T5 | 5.5.3, tabela wierszy; R-KOK-09 (weryfikacja); R-KOK-15 | `Czas z maską h`, `Wyciek P95 l/min`, `Ciśnienie P95 cmH₂O`, `EPAP P95 cmH₂O`, `Wsparcie PS cmH₂O` | `Czas z maską`, `Wyciek l/min`, `Ciśnienie P95`, `EPAP P95`, `Wsparcie PS` | szerokość kolumny etykiet (148 px); K1 też pokazuje wyciek P95 jako `wyciek l/min` |
| T6 | 5.5.3, „Kolumny” (`30 nocy`), legenda; R-KOK-02 („w K1 i K2”), R-KOK-06 („linia 30 nocy”) | kolumna `30 nocy`, legenda z `mediana normy`, kursywa 18 px | bez kolumny; legenda `ponad próg` · `poza Twoją normą`, 22 px prosto | opcja A (rozdz. 4) |
| T7 | 5.5.3, „Kolumny” (7 nocy) | `–` | `–` na środku komórki | inaczej czyta się jak minus przy normie |
| T8 | 5.11.4, „Podtytuł” | `Zaznacz wszystko, co pasuje. Po kilku takich nocach pokażę, co się powtarza.` | `Zaznacz wszystko, co pasuje.` | 77 zn. = 820 px, nie mieści się w 1 linii 22/600 |
| T9 | 5.11.4, „Podpowiedź” | `Przytrzymaj ◀, żeby pominąć. Bez odpowiedzi pytanie zniknie o {hh:mm}.` | `Bez odpowiedzi pytanie zniknie o {hh:mm}.` | pominięcie jest w stopce |
| T10 | 5.11.4, „Stopka” | `◀ ▶ wybór` · `↻ zaznacz` · `przytrzymaj ↻ zapisz ({k})` | `◀ wstecz · 2 s pomiń` · `↻ zaznacz · 2 s zapisz ({k})` · `▶ dalej` | wzór 2.1; ◀ 2 s „pomiń” było dotąd tylko w podpowiedzi |
| T11 | 5.6.1 pkt 3 (zasada stopki) | brak wzoru zapisu | dopisać wzór 2.1: grupa na przycisk, kolejność ◀ ↻ ▶, `2 s` zamiast „przytrzymaj” | jedna reguła dla wszystkich stopek |
| T12 | 5.8.11, A1 „Nagłówek” | prawo `panel {t}° · {w}%` | prawa strona pusta | te same liczby stoją większe w faktach |
| T13 | 5.8.11, A1 „Fakty” | `Twój zwykły czas z maską` | `zwykle z maską` | 3 fakty w 474 px przy 22/600 |
| T14 | 5.8.11, A1 „Krok” | `→ Do rana bez odświeżania, żeby nie błyskać.` (44 zn., 2 linie) | `→ Ekran zostaje do rana, żeby nie błyskać.` (42 zn., 1 linia) | prościej i mieści się; usuwa też uwagę o 2 liniach |
| T15 | 5.8.11, A3 „Krok albo mała linia” | `Do rana ekran się nie zmieni, żeby nie błyskać w sypialni.` | `Ekran zostaje do rana, żeby nie błyskać.` | 640 px nie mieści się obok budzika; to samo zdanie co A1 |
| T16 | 5.7.2, „Tytuł”, „Pod tytułem”, „Stopka” | Source Serif 4 46 px; fakty liczba 32 px nad podpisem 18 px; zdanie budzika (wycofane z ekranu 2026-11-10) kursywą | SB44 700; fakty P28B nad P22B; zdanie w cudzysłowie bez kursywy, 22/600 | skala B |

T1–T16 to zmiany treści ekranu: wpis w CHANGELOG i macierz śledzenia, bez ADR (granice i mapa przycisków bez zmian).

## 9. Kroje i pamięć flash

Szacunek funkcją `font_c` z `tools/prototyp/generuj.py` (bitmapa + tabela glifów + szerokości):

| Krój | Rozmiar | Uwagi |
|---|---|---|
| P24B | 4,1 KB | nowy, wspólny ze skalą B |
| P28B | 5,0 KB | nowy, wspólny ze skalą B |
| SN56 | 2,5 KB | nowy, tylko kokpit; tylko cyfry i `,:–h` (pełny: 13,8 KB) |
| razem kokpit | ok. 12 KB, z czego P24B i P28B liczone już w SPEC-B | jedyny koszt ponad SPEC-B: SN56, 2,5 KB; razem ze skalą B ok. 24 KB nowych krojów, flash z ok. 51 % na ok. 51,8 % |

Po przejściu kokpitu, pytania, S9, testu i kart na nową skalę te ekrany przestają używać: `S22`, `SB22`, `SI18`, `SI20`, `PI18`, `C18`, `C18B`, `C20`, `C20B`, `S30`, `S44` (razem ok. 33 KB). Usunąć można te, których nie używają też ekrany dnia; `P18`, `P20`, `P20B`, `P22`, `S32`, `S36`, `S42`, `S46`, `S48` zależą od SPEC-B.

## 10. Ocena z daleka (35 %, ok. 1,5 m)

| Ekran | Czytelne | Na granicy | Nieczytelne |
|---|---|---|---|
| K1 | 4 liczby nocy (SN56), wiersz w negatywie, kształt linii | nazwy wierszy (P24B), mediany (P28B) | ocena, opisy liczb, stopka (22 px) |
| K2 | czarne komórki i ramki jako wzór, który wiersz jest „czarny” | nagłówek, zdanie | pojedyncze cyfry 22 px |
| pytanie | tytuł, które pola są zaznaczone, gdzie kursor | etykiety pozycji | podpowiedź, stopka |
| S9 | A3: godzina; A1, A2: tytuł, fakty (P28B), budzik | zdanie budzika (wycofane z ekranu 2026-11-10) | – |
| karty | tytuł, fakty | powód, krok | stopka |
| test | tytuł | – | linie (ekran serwisowy, z bliska) |

Wniosek: zysk względem obecnego renderu jest wyraźny (waga 600–700 zamiast 400–500, brak kursywy i 18 px), ale K2 i stopki pozostają ekranem „z bliska”. To pasuje do obsługi: do K2 i tak sięga się ręką (przycisk).

## 11. Rozjazdy z kanwą

| Artboard | Rozjazd |
|---|---|
| `K1` | opis pod liczbą 18 px → nad liczbą 22 px; liczby szeryf 44/400 → 56/600; brak nagłówków kolumn → `mediana`, `ocena`; `Sen` / `Ciśnienie` na kanwie (stare) |
| `F07-kokpit` | Plex 18–20 → Public Sans 22/600; kolumna `30 nocy` usunięta; legenda kursywą → prosto, bez `norma: P10–P90…`; stopka `◀ tydzień wcześniej`, `▶ 30 nocy` (już niezgodna z mapą) → wzór 2.1; 9 → 10 wierszy (ASV) |
| `F08-pytanie` | pozycje szeryf 22/400 (zaznaczone 700) → Public Sans 22/600; podtytuł i podpowiedź kursywą → prosto, skrócone (T8, T9); stopka Plex → wzór 2.1 |
| `K22-dobra-noc`, `EkranD` | tytuł 46/400 → SB44; fakty szeryf 32 → P28B; stopka kursywą 18 → P22B; etykieta 18 → 22 |
| S9 A1–A3 | brak artboardu (6.7); makiety stąd mogą nim zostać |
