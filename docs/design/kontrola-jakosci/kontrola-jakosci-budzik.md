# Kontrola jakości: biblioteka budzika (68 scen)

Data: 22.10.2026 (po poprawkach).

**Po poprawkach:** poprawione 9 scen (B02, G05, M08, P09, S03, W03, W07, W08, W12). Wszystkie przeszły kontrolę: format, brak zabłąkanych znaków, brak dotykania krawędzi, czytelne w rozmiarze karty. Jedyna drobna uwaga: dzianina szalika w W03 ma wzór, który z bliska przypomina litery; w rozmiarze karty czyta się jako faktura.

## Wynik w skrócie
| Sprawdzenie | Jak | Wynik |
|---|---|---|
| Format | skrypt: rozmiar i tryb każdego pliku | 68/68: 800 × 480, czysty 1-bit (tylko czerń i biel) |
| Liczby i napisy w obrazkach | przegląd wzrokowy w pełnym i docelowym rozmiarze | brak; tarcze mają tylko kreski, medal i tort bez cyfr |
| Spójność postaci | przegląd wzrokowy wszystkich 68 | jeden rysunek budzika we wszystkich scenach |
| Zabłąkane znaki | skrypt: małe elementy daleko od rysunku | 2 artefakty (W07, W08), usunięte automatycznie przy obróbce; pozostałe trafienia to celowe iskry, kurz i kreski ruchu (D01, G04, S03, W05) |
| Przycięcie przy krawędzi | skrypt | 1 scena dotyka krawędzi (G05), bez utraty treści |
| Czytelność w rozmiarze karty (290 × 250 px, 1-bit) | render każdej sceny w docelowym rozmiarze i przegląd | 57 OK, 7 z małą postacią przez szeroki kadr, 4 do poprawy |

## Poprawione w rundzie 2 (4)
| ID | Problem | Poprawka |
|---|---|---|
| B02 | antena wygląda jak batuta, myli się z G15 | mały router z wysoką anteną i trzema łukami sygnału |
| W03 | szalik czyta się jak pędzel | długi pasiasty szalik owinięty kilka razy wokół rury, wyraźnie widoczny |
| S03 | filtr wygląda jak solniczka | płaski kwadratowy filtr w palcach, opadający kurz |
| P09 | kadr zatłoczony, postać mała | zbliżenie: postać na poduszce, mały wentylator, postać duża w kadrze |

## Szeroki kadr: postać na karcie wychodzi mała (5 po rundzie 2)
G03, G08, G10, G13, S04 (M08 i W12 poprawione w rundzie 2). Przy przerywnikach (G) to nie przeszkadza, scena jest „obrazkiem dnia”. M08 (najlepszy tydzień) i W12 (wieczór) poprawione w rundzie 2 ciaśniejszym kadrem.

## Uwagi projektowe
1. **Kierunek patrzenia.** Większość scen patrzy albo sięga w prawo, czyli na krawędź ekranu. Na karcie budzik stoi po prawej, więc lepiej, żeby patrzył w lewo, na tekst. W obrazkach nie ma napisów, więc lustrzane odbicie jest bezpieczne. **Rekomendacja:** odbijać wszystkie sceny na panelu (zero kosztu, jedna linia w kodzie).
2. **Karty „problem” w negatywie.** Odwrócony rysunek (białe kreski na czerni) jest cięższy i gęstszy niż jasny. Na podglądzie czytelny, ale to pierwsza rzecz do sprawdzenia na prawdziwym E1001.
3. **Ujednolicenie skali** robi obróbka (przycięcie do rysunku i dopasowanie do 290 × 250), więc różne rozmiary postaci w surowych plikach (np. D07, D08, M01–M05, W07–W11 są w oryginale mniejsze) nie są problemem.

## Wszystkie sceny
| ID | Status | Uwagi |
|---|---|---|
| B01 | ✅ OK |  |
| B02 | ✅ poprawione w rundzie 2 | antena wygląda jak batuta dyrygenta (myli się z G15); brak skojarzenia z Wi-Fi |
| B03 | ✅ OK |  |
| D01 | ✅ OK |  |
| D02 | ✅ OK |  |
| D03 | ✅ OK |  |
| D04 | ✅ OK |  |
| D05 | ✅ OK |  |
| D06 | ✅ OK |  |
| D07_powitanie-zza-poduszki | ✅ OK |  |
| D08_dumny-z-siebie | ✅ OK |  |
| G01 | ✅ OK |  |
| G02 | ✅ OK |  |
| G03 | ⚠️ szeroki kadr, mała postać | hamak |
| G04 | ✅ OK |  |
| G05 | ✅ poprawione w rundzie 2 | czapka i buty dotykają krawędzi obrazka (brak marginesu); po przycięciu działa |
| G06 | ✅ OK |  |
| G07 | ✅ OK |  |
| G08 | ⚠️ szeroki kadr, mała postać | liście na całą szerokość |
| G09 | ✅ OK |  |
| G10 | ⚠️ szeroki kadr, mała postać | owce i płot |
| G11 | ✅ OK |  |
| G12 | ✅ OK |  |
| G13 | ⚠️ szeroki kadr, mała postać | stół z szachami |
| G14 | ✅ OK |  |
| G15 | ✅ OK |  |
| G16 | ✅ OK |  |
| G17 | ✅ OK |  |
| G18 | ✅ OK |  |
| M01_medal-konfetti | ✅ OK |  |
| M02_flaga-na-gorce | ✅ OK |  |
| M03_podium-laur | ✅ OK |  |
| M04_tort-swieczki | ✅ OK |  |
| M05_rocznica-terapii | ✅ OK |  |
| M06 | ✅ OK |  |
| M07 | ✅ OK |  |
| M08 | ✅ poprawione w rundzie 2 | tablica z wykresem |
| O01 | ✅ OK |  |
| O02 | ✅ OK |  |
| O03 | ✅ OK |  |
| O04 | ✅ OK |  |
| P01_maska-przecieka | ✅ OK |  |
| P02_przeciek-falami | ✅ OK |  |
| P03_przedmuchany-rura | ✅ OK |  |
| P04_zaplatany-w-rure | ✅ OK |  |
| P05_zuzyta-poduszka-maski | ✅ OK |  |
| P06 | ✅ OK |  |
| P07 | ✅ OK |  |
| P08 | ✅ OK |  |
| P09 | ✅ poprawione w rundzie 2 | łóżko i wentylator zjadają kadr, postać mała i zatłoczona |
| P10 | ✅ OK |  |
| P11 | ✅ OK |  |
| S01 | ✅ OK |  |
| S02 | ✅ OK |  |
| S03 | ✅ poprawione w rundzie 2 | filtr w palcach wygląda jak solniczka |
| S04 | ⚠️ szeroki kadr, mała postać | długa rura |
| W01 | ✅ OK |  |
| W02 | ✅ OK |  |
| W03 | ✅ poprawione w rundzie 2 | szalik na rurze czyta się jak pędzel albo miotełka |
| W04 | ✅ OK |  |
| W05 | ✅ OK |  |
| W06 | ✅ OK |  |
| W07_duzo-pylku | ✅ poprawione w rundzie 2 | usunięty zabłąkany znak przy lewej krawędzi |
| W08_stala-pora | ✅ poprawione w rundzie 2 | usunięty zabłąkany znak „1” po lewej |
| W09_weekend-przed-toba | ✅ OK |  |
| W10_maska-z-rura | ✅ OK |  |
| W11_plecak-i-maska | ✅ OK |  |
| W12_ziewanie-z-poduszka | ✅ poprawione w rundzie 2 | łóżko obok |

## Dodatek 01.11.2026: B04 i B05 (brak danych)

Dwie grafiki autora projektu (1619 × 971 px, RGB), zapisane jako źródła 800 × 480 1-bit (`assets/budzik/zrodlo/B04.png`, `B05.png`) i karty (`assets/budzik/karta-240x205/B04-240x205.png`, `B05-240x205.png`: przycięte do zawartości, zmniejszone, próg 1-bit po skalowaniu, odbite lustrzanie).

| Sprawdzenie | Jak | B04 (z lupą) | B05 (bez lupy) |
|---|---|---|---|
| Liczby i napisy | przegląd wzrokowy | brak (znak Wi-Fi to symbol) | brak |
| Spójność postaci | przegląd wzrokowy | ten sam budzik | ten sam budzik |
| Szarości przed progiem | skrypt: piksele 30–225 | 3,4 % (wygładzone krawędzie) | ok. 3 % |
| Szerokość sceny | skrypt: ramka zawartości | 92 % szerokości (założenie: postać z rekwizytami do 60 %) | 92 % |
| Rozmiar na karcie | skrypt | 240 × 133 px | 240 × 133 px |
| Czytelność w rozmiarze karty | render karty S5 z kodu panelu (`tools/prototyp/testy.sh`, `karta-s5.png`) | ✅ lupa, i kółko widoczne | ✅ |

Uwaga: obie sceny są szerokie, więc na karcie mają 133 px wysokości zamiast do 205 px; pierwsza wersja (szerszy kadr, 74 px) odrzucona z tego powodu. Do sprawdzenia na prawdziwym E1001 razem z pozostałymi kartami.
