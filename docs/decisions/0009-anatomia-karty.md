# ADR-0009: Anatomia karty

| | |
|---|---|
| Data | 22.10.2026 |
| Status | przyjęta |
| Źródło | specyfikacja 4.7 (v0.6c) |

## Kontekst
Pierwsza wersja karty miała za mało oddechu, a budzik w negatywie na ciemnym tle wyglądał źle (rysunek projektowany na jasne tło).

## Decyzja
Tytuł na całą szerokość ekranu, pod nim powód, fakty i krok po lewej, budzik 240×205 po prawej, odbity lustrzanie (patrzy na tekst). Budzik nigdy na czarnym tle: karta „problem” jest jasna z czarnym paskiem nagłówka i etykietą w czarnej ramce.

## Konsekwencje
Pełny negatyw zostaje dla ekranów danych bez budzika; ekran lekarski jedynym czarnym z ramką.

## Adnotacja 30.10.2026
Wyjątek dla ekranu nocnego S9 ([ADR-0018](0018-ekran-nocny.md)): wariant A2 ma dużą scenę budzika na środku (do ok. 600×340), bez nagłówka i stopki, a wariant A3 budzik ok. 230×250 po prawej. Wariant A1 zachowuje budowę karty. Budzik nigdy na czarnym tle także na S9.

**Zmiana 04.11.2026 (bez zmiany decyzji):** pełny ekran zdarzenia E w tonie „problem” (S3) jest ekranem danych w negatywie bez ramki i bez budzika, ze strefą linii uwagi pod nagłówkiem i wykresem 30 nocy z podpisami osi. Ramka zostaje tylko dla ekranu lekarskiego. Przerywniki S2d i karty S-K budują się według tej anatomii, budzik nigdy na czarnym tle.

**Zmiana 05.11.2026 (decyzja):** przerywnik S2d jest kartą bez etykiety: żaden napis nad tytułem, ani `Na dzień dobry`, ani nazwa sceny czy okazji. To wyjątek od wzoru karty (etykieta jest elementem anatomii pozostałych kart) tylko dla S2d. Teksty przerywników są dłuższe niż limity karty (30 + 70 znaków), bo napisał je autor projektu (do 86 znaków w tytule i 98 w powodzie); układ S2d dopasowuje się do tekstu, nie odwrotnie: tytuł wyżej (SB44, do 2 linii; pierwsze zdanie dłuższe niż 2 linie w P34B, 2 linie), powód P22B do 3 linii w kolumnie tekstu, fakty ostatniej nocy pod nim, budzik bez zmian. Szczegóły: spec 5.7.2 i 5.7.6, R-HUM-21, R-HUM-28.

**Zmiana 2026-11-10 (decyzja właściciela, „usunąć”; audyt UX 10.11.2026):** karta nie ma już zdania budzika w stopce. Lewa część stopki pokazuje to, co ekran danych tego samego ekranu (pogoda albo podpowiedź, R-EKR-10, albo podpis akcji), prawa bez zmian (`przy łóżku …` albo akcja); stopka bez treści po obu stronach znika. Rysunek budzika, etykieta, tytuł, powód, fakty i krok bez zmian. Pule zdań zostają w danych, bo dalej decydują o trybie karty jak dotąd (spec 5.7.5); budzik nadal nigdy na czarnym tle.
