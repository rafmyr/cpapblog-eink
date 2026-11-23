# ADR-0049: „Dobry start”, program porad na pierwsze 6 tygodni terapii

| | |
|---|---|
| Data | 21.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 22.11.2026); pkt 7 zmieniony [ADR-0050](0050-opcje-komfortu-w-poradach.md) |
| Źródło | zlecenie „Program pierwszych 6 tygodni” (21.11.2026); zasady działania w wersji 2 i 84 porady z przeglądu źródeł 21.11.2026 ([`docs/spec/katalog/porady.csv`](../spec/katalog/porady.csv), [`porady-ankieta.csv`](../spec/katalog/porady-ankieta.csv)); specyfikacja modułu: [spec funkcjonalna 5.8.14](../spec/specyfikacja-funkcjonalna.md) (R-DS-01…R-DS-53, S-K-48) |
| Zmienia | [ADR-0043](0043-belka-informacyjna.md) pkt 4 (belka karty programu z nazwą `Dobry start` zamiast `Poranek`) i pkt 5 (ekran slotu dziennego z belką); [ADR-0027](0027-ekran-dzienny.md) pkt 7 (karta z budzikiem w slocie dziennym) i listę ekranów zasłanianych przez wygaszacz ze zmiany 2026-11-16 (R-KON-75: S-K-48 nie jest zasłaniany); dotyka [ADR-0007](0007-dwa-tryby-ekranu.md) (limity kart) i [ADR-0045](0045-kolejnosc-i-widocznosc-ekranow-z-telefonu.md) (porada poza listą „Ekrany”) |
| Bez zmian | granice [ADR-0001](0001-przetwarzanie-lokalne.md), [ADR-0002](0002-panel-nie-zmienia-ustawien.md) (z wyjątkiem opcji komfortu w poradach programu, [ADR-0050](0050-opcje-komfortu-w-poradach.md)), [ADR-0029](0029-zakres-skierowan-w-wydaniu-1-0.md) pkt 7 |

## Kontekst

Panel dziś mówi o nocy i o sprzęcie, ale nie uczy, jak zacząć terapię. Osoba, która dostaje aparat, przez pierwsze tygodnie ma najwięcej kłopotów (wyciek, suchość, maska zdejmowana przez sen, uczucie zamknięcia, woda w rurze) i najmniej wiedzy, jak czytać liczby. To, czy aparat będzie używany, rozstrzyga się wcześnie: w 3.–4. dniu, w 1. tygodniu i w 1. miesiącu (poziom B; teza „najczęściej rezygnują w pierwszych tygodniach” potwierdzona tylko w połowie).

Przegląd źródeł 21.11.2026 dał porady ze źródłami (książka autora projektu, blog CPAPblog.pl, zalecenia towarzystw i producenta z poziomem dowodu). Gdzie źródła własne różnią się od zaleceń, porady idą za zaleceniami i ADR-0002; opcje komfortu (nawilżanie, temperatura rury, rampa) od 22.11.2026 według [ADR-0050](0050-opcje-komfortu-w-poradach.md). Pierwsza wersja zasad łamała ADR-0043 i ADR-0027 bez nowego ADR, zasłaniała okno wietrzenia i wymianę sprzętu przez cały dzień, za późno ostrzegała o senności za kierownicą i obiecywała, że „panel wskaże lekarza”; wersja 2 zasad i treści to poprawia.

Ograniczenia, które trzeba zachować:

- Slot dzienny jest bez belki (ADR-0043 pkt 5), a zlecenie chce belki z licznikiem programu.
- Budzik nic nie komunikuje, najwyżej 2 karty na dobę i połowa wyświetleń w tygodniu (ADR-0007, R-HUM-02, R-HUM-23); od 10.11.2026 budzik nie „mówi”.
- Ekrany, które ostrzegają albo każą coś zrobić teraz, nie mogą zostać zasłonięte (ADR-0045, R-KON-72, R-KON-75).
- Panel nie podaje wartości ustawień ani drogi do ich zmiany i nie diagnozuje (ADR-0002); „nie oceniam” nie znaczy „w porządku” (ADR-0029 pkt 7).
- Przyciski według jednej reguły B31 (spec 5.6.1).

## Decyzja

1. **Tryb i miejsce ekranu.** Program włącza się z telefonu (`start.program`, domyślnie wyłączony) i działa w trybach `CPAP`, `AutoSet`, `ASV` i `ASVAuto` (ASVAuto dostaje teksty ASV). Porada to nowy ekran S-K-48 „Porada dnia”: stoi przez pierwsze 180 min `[PRÓG]` slotu dziennego na pozycji **D0b** (po D0, D0a i ekranach z działaniem teraz: `Przewietrz teraz.`, trwające okno `Wietrz do …`, upał `Okna zamknięte …`, S-K-35), potem zwykła kolejność D1…D5; wraca na początku slotu wieczornego (**W3b**, po W1…W3) i stoi do S9. S-K-48 to **ekran danych z belką** (`Dobry start` · `dzień N z 42` / `po nocy {DD.MM}` / `z twojej ankiety` / `ponad program` · bateria): świadomy wyjątek od ADR-0043 pkt 5. Wygaszacz nie zasłania porady w jej bloku ani wieczorem. Porady nie da się ukryć na liście „Ekrany” (ADR-0045); wyłącza ją tylko przełącznik programu.
2. **Jedna nowa porada na dobę**, pierwszeństwo: z danych ostatniej nocy > z ankiety > z planu. Ekrany ostrzegawcze wygrywają z każdą poradą; w dniu z D0 porada stoi tylko wieczorem, a porada planu od 3. takiej doby z rzędu. Porady wyzwalane najwyżej 2 na 7 dni do 28. doby, potem 1 na 7 dni `[PRÓG]`; ta sama raz na 7 dni i 2 razy w programie `[PRÓG]`, wyciek po stopniach DS-Z01 → DS-Z02 → DS-Z03, DS-Z06 (przerwa) co 7 dni bez limitu. Porada wyzwalana nie wyprzedza porady planu, która objaśnia jej pojęcie (AHI, norma).
3. **Plan jako kolejka, nie kalendarz.** ID porady jest stałe, miejsce w kolejce daje kolumna `dzien`. Miejsce liczy się dopiero po narysowaniu porady; doba z poradą wyzwalaną przesuwa plan. Po 2 nocach bez terapii plan staje (DS-Z06). DS-D42 jest zawsze ostatnią poradą planu; program kończy się najpóźniej 56. doby od włączenia `[PRÓG]`, wydłużonej o przerwy; potem 11 porad „ponad program” (od 22.11.2026; wcześniej 9). Przy starcie od dnia N > 1 najpierw porady podstawowe (senność a auto, kiedy do lekarza, wyciek, suchość, AHI); po zmianie trybu aparatu warianty nowego trybu porad już pokazanych.
4. **Budzik tylko na 7 kartach bez treści zdrowotnej:** sześć podsumowań tygodnia i „Siedem nocy z rzędu” (DS-Z14, na istniejącej metryce M-20). Karta ma belkę `Dobry start` i stopkę tylko z temperaturami, liczy się do limitów kart (R-HUM-02, R-HUM-23); gdy nie może być kartą, ten sam tekst idzie jako ekran danych. Wszystko o objawach, kłopotach, liczbach, sprzęcie i lekarzu to ekran danych bez budzika.
5. **Ankieta tylko na telefonie.** Na start pytanie o nawilżacz (pole `start.nawilzacz`; `nie` wybiera warianty `.bez` i blokuje DS-Z10), po 3. nocy i co 7 dni 2–3 pytania (jak się śpi, co przeszkadza, od 2. tygodnia samopoczucie), do wypełnienia w każdej chwili, bez przypominania. Odpowiedzi zostają na panelu w `/dane/dobry_start.csv`, poza raportem dla lekarza, regułami L i pytaniem o senność (L-11). Pole `start.objawy` pozwala trzymać porady o kłopotach i objawach tylko na telefonie (panel pokazuje wtedy `Nowa porada na telefonie.`).
6. **Treści w pliku danych ze źródłami.** Porady i pytania w `docs/spec/katalog/porady.csv` i `porady-ankieta.csv` (id, rodzaj, miejsce, tryb, nawilżacz, ekran, wyzwalacz, `przy`, `zamiast`, teksty, źródła z książki, bloga i zaleceń, poziom dowodu, werdykt, znacznik zmiany tekstu po przeglądzie); w zestawie danych z wersją i skrótem (ADR-0023, ADR-0024), z testem długości krojami panelu (ikony przycisków jak na panelu) i testem tekstów pod ADR-0002 przy budowie zestawu.
7. **Granice.** ADR-0001 (wszystko na panelu) bez zmian. ADR-0002 (bez wartości i drogi zmiany ustawień, bez diagnozy) z jednym wyjątkiem od 22.11.2026 ([ADR-0050](0050-opcje-komfortu-w-poradach.md)): porada może nazwać opcję komfortu, którą pacjent zmienia sam według instrukcji aparatu (nawilżanie, temperatura podgrzewanej rury, rampa), podać kierunek i odesłać do instrukcji; ulgę na wydechu tylko opisuje i odsyła do lekarza albo dostawcy; ciśnienie, zakres, tryb i poziom EPR bez zmian (tylko lekarz). Przy objawach porada kieruje do lekarza albo dostawcy; żadna nie obiecuje, że „panel wskaże lekarza”. ADR-0029 pkt 7: DS-D43 i DS-Z13 mówią wprost, że „nie oceniam” nie znaczy „noc była dobra”.

## Konsekwencje

- Nowe w spec: moduł 5.8.14 „Dobry start” z R-DS-01…R-DS-53, ekran S-K-48 (5.8.3, 5.8.4, 6.2), wiersze D0b i „D1 (z działaniem)” w 7.2 część „Dzień” i W3b w części „Wieczór”, wiersz belki w 5.4.17, pięć kluczy `start.*` w 5.6.4, sekcja formularza „Dobry start” (5.6.6, po „Ekrany”) i strony „Ankieta” oraz „Wszystkie porady”, plik `/dane/dobry_start.csv` (spec techniczna T-DANE), progi w `progi-poza-zakresem.csv`; zmiany R-KON-75 i R-KONF-61.
- Przez 6 tygodni pierwsze 3 godziny slotu dziennego i początek wieczoru zajmuje porada; ekrany z działaniem teraz zostają na wierzchu, ale `Wietrz od …`, „Jutro rano” i pyłek wychodzą później w ciągu dnia. Wieczorne W1…W3 bez zmian.
- Wieczór dostaje nowy ekran (W3b), więc w dni bez ekranu wieczornego jest jedno odświeżenie e-ink więcej (porada zamiast ekranu z rana), a koniec bloku dziennego to jedna ocena slotu więcej, jeśli nie wypada o pełnej godzinie (do zmierzenia).
- Informacja po aktualizacji samodzielnej (R-KONF-61) staje na miejscu porady przy pierwszym rysowaniu, inaczej czekałaby do końca bloku.
- Porady wyzwalane danymi wchodzą do oceny wobec MDR (pytanie otwarte, ADR-0015). Do rozstrzygnięcia DS-Z08, DS-Z08.asv i DS-A08 tylko odsyłają do lekarza (R-DS-40).
- Treści medyczne przed pokazaniem pacjentom spoza testów potwierdza lekarz PSG (pytanie otwarte); kroki DS-D44 (raport to ▶ 2 s) i DS-D26 (tabela 7 nocy to druga strona kokpitu) poprawione 21.11.2026 w `porady.csv`.
- Strona formularza rośnie o sekcję (ok. 2 KB, do pomiaru); „Wszystkie porady” i „Ankieta” to osobne strony podawane partiami, żeby nie zwiększać `REZERWACJA_STRONY`.

## Odrzucone warianty

| Wariant | Dlaczego nie |
|---|---|
| budzik jako przewodnik na każdej poradzie | porada codziennie na karcie to budzik w 100% dni programu: łamie limit kart (2 na dobę, połowa wyświetleń w tygodniu, ADR-0007, R-HUM-23); większość porad mówi o objawach albo odsyła do lekarza, a tam budzika nie ma (5.7.3, R-HUM-25); od 10.11.2026 budzik nie mówi, więc nie może być narratorem |
| porada rano zamiast Portretu nocy | rano panel pokazuje noc i sprawy (ADR-0040, kolejność poranka z 11.11.2026); porada rano wypierałaby Portret albo ekran problemu, a czyta się ją przy zakładaniu maski |
| porada przez cały slot dzienny (wersja 1 zasad) | przez 6 tygodni zasłaniałaby okno wietrzenia, upał, „Jutro rano” i przypomnienie wymiany sprzętu |
| plan kalendarzowy z przepadaniem dni | w dni z ekranem ostrzegawczym, poradą wyzwalaną albo bez terapii porady planu przepadałyby, a właśnie te dni są trudne; kolejka niczego nie gubi |
| dwie porady dziennie | dwa ekrany porad to dodatkowe odświeżenia e-ink i więcej czytania; jedna porada na dobę wystarcza, a pierwszeństwo rozstrzyga, która |
| ankieta przyciskami panelu | trzy przyciski i reguła B31 nie mieszczą pytania wielokrotnego wyboru z 10 odpowiedziami (jedyny wyjątek, siatka „Co było inaczej?”, już istnieje); odpowiedzi o samopoczuciu na ekranie przy łóżku widzą domownicy |

## Zmiana 2026-11-22 (decyzje właściciela projektu, „wdrażaj”)

- **Przyjęta** w kształcie z pkt 1–6: budzik tylko na 7 kartach, blok porady 180 min w dzień i wieczorem, porady wyzwalane danymi zostają; wchodzą do oceny wobec MDR (pytanie otwarte, ADR-0015).
- **Pkt 7 zmieniony:** opcje komfortu w poradach według [ADR-0050](0050-opcje-komfortu-w-poradach.md); nowe porady DS-D45 (nawilżanie i ciepła rura) i DS-D46 / DS-D46.asv (rampa i ulga na wydechu; w ASV tylko rampa).
- **Treści:** infekcja według książki (nie przerywać, więcej nawilżania; DS-D32, DS-Z11), wyciek według pasm panelu z ADR-0034 (DS-D22 z `{prog:…}`, R-DS-53), mycie rury co 2–3 tygodnie i filtr co 6–8 tygodni według książki (DS-D17, DS-D20); pozostałe zgodnie z propozycjami projektu.
- **Plan:** nowa kolejność 42 miejsc (DS-D45 na 9., DS-D46 na 16.); DS-D29 (drzemki) i DS-D36 (kiedy poczuję różnicę) przechodzą do „ponad program”, który ma teraz 11 porad; bez nawilżacza miejsce DS-D45 zajmuje DS-D29 (R-DS-29).

## Zmiana 2026-11-22 (2): wersja 2 programu (decyzja właściciela projektu)

Specyfikacja wersji 2: spec funkcjonalna 5.8.14.14 (R-DS-54…R-DS-71), treści w `docs/spec/katalog/porady.csv`.

- **Trzy fazy, 12 tygodni (84 doby) zamiast 42 miejsc w 56 dobach** (zmienia pkt 3): start (doby 1…10) do 2 porad na dobę: rano reakcja na noc (pochwała albo porada do zdarzenia, bez nich porada poranna planu), wieczorem porada przygotowująca do nocy; adaptacja (doby 11…42) 1 porada dziennie; utrwalenie (tygodnie 7…12) 2 porady w tygodniu `[PRÓG]`, na wejściu kontrola po 6 tygodniach (DS-U01). „Ponad program” wchłonięty do utrwalenia. Zmienia odrzucony wariant „dwie porady dziennie”: w starcie tak, bo to tam brakuje wskazówek i wsparcia; dalej jedna.
- **Pochwały z danych** (nowa rodzina DS-P01…DS-P12, zmienia pkt 2 i 4): chwalą zachowanie (czas z maską, regularność, szczelność), nigdy AHI ani zdrowie (ADR-0029 pkt 7); najwyżej jedna na dobę, nie w dniu z ekranem problemu, szczelność nie po nocy niepewnej. DS-Z14 wycofane, zob. DS-P04. Karty z budzikiem: karty tygodnia, DS-U02 i trzy pochwały serii (DS-P04, DS-P09, DS-P10), wszystkie bez treści zdrowotnej, z rzędem faktów z danych (do 3 par liczba i podpis, bez AHI); limity kart R-HUM-02 i R-HUM-23 bez zmian.
- **Kod QR** na 27 poradach (15 artykułów CPAPblog.pl i grupa pacjentów), ekran danych z węższą kolumną (513 px), moduł 7 px jak na ekranie raportu; nigdy na karcie.
- **Granice:** opcje komfortu i odczucia oddechowe według [ADR-0051](0051-menu-pacjenta-i-odczucia-w-poradach.md) (droga w Moich opcjach, ulga wydechowa, „za niskie / za wysokie ciśnienie” z lekarzem); pozostałe granice pkt 7 bez zmian.

## Zmiana 2026-11-22 (3): wersja 2 w kodzie

Wersja 2 wdrożona w firmware: fazy, pochwały, znaczniki danych, kod QR i fakty na S-K-48, formularz „12 tygodni”, spec funkcjonalna 5.8.14.14. Trzy rozstrzygnięcia przy wdrożeniu, przyjęte przez właściciela projektu: kolejki faz liczone z wpisów `P` dziennika (bez wierszy kolejki w pliku stanu), kopia przed migracją `/dane/dobry_start.csv.v1` (konwencja magazynu), fakty karty na ekranie danych jedną linią „liczba podpis”.
