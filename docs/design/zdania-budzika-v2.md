# Zdania budzika: zestaw v2 (propozycja)

> **Wycofane z ekranu 2026-11-10** (decyzja właściciela: „usunąć”, „bardziej przeszkadzają niż pomagają”; spec 5.7.5, ADR-0009 „Zmiana 2026-11-10”). Panel nie rysuje zdań budzika na kartach ani na S9; pule zostają w danych. Dokument zostaje jako historia doboru.

Data: 01.11.2026. Status: zestaw wybrany 01.11.2026; w specyfikacji jako **propozycje do akceptacji** (WY15), bo akceptacja pul to osobny krok.
Uzasadnienie wyboru dla zdań w [specyfikacji, rozdz. 5.7.5](../spec/specyfikacja-funkcjonalna.md). Powód zmiany: na panelu (S9, scena G13) stało `Szach-mat. Księżyc się zamyślił.`, ocenione jako słabe.

## Jak powstał
1. Trzy serie kandydatów w trzech stylach: komedia postaci, gra słów, suchy humor z ciekawostkami. Razem 432 zdania.
2. Filtr skryptem: najwyżej 45 znaków, bez cyfr, bez duplikatów. Zostało 429.
3. Ocena na ślepo, bez wiedzy o pochodzeniu zdań. Kryteria po 0–5 punktów: czy łapie się w 3 sekundy, błyskotliwość, zgodność z postacią, czy bawi za trzecim razem. Każde naruszenie reguł 4.7.2 albo ADR-0002 dyskwalifikowało zdanie. Odpadło 7.
4. Kontrola końcowa: każde zdanie zgodne z oryginałem co do znaku, najdłuższe ma 45 znaków, „punktualnie” jako puenta pojawia się najwyżej 2 razy.

Wynik: 42 zdania podstawowe i 30 rezerwowych. Style zwycięskich zdań: gra słów 16, postać 15, suchy humor 11.

## Postać budzika (stałe cechy, z których wynikają żarty)
- Punktualny do przesady i dumny z tego.
- Zazdrosny o aparat: aparat pracuje całą noc, budzik ma swoje pół minuty rano.
- Ma dzwonki, sprężynę, rękawiczki i buty, ale nie ma rąk do realnej pomocy.

## Ekrany (pula rotacyjna, po 4 zdania)

| Typ | Zdania | Rezerwa |
|---|---|---|
| dobra noc | Aparat pracował, a medal biorę ja. · Tak cicho, że słyszałem własne tykanie. · Szukałem dziury w całym. Nie ma. · Wzorowa noc. Wpiszę ją sobie do CV. | Piękna noc. Aparat udaje, że to on. · Wszystko gra. Ja tylko dzwonię. |
| seria / kamień milowy | Seria dłuższa niż moja lista spóźnień. · Jestem nakręcony tą serią. · Jubileusz! Wypolerowałem dzwonki na galę. · Kolejna noc. Mój mechanizm się wzruszył. | Tyle nocy, że muszę dokręcić sprężynę. · Licznik rośnie. Ja tykam z zazdrości. |
| przeciek | Maska puściła parę. Ja nikomu nie powiem. · Nawet ja raz zadzwoniłem w niedzielę. · Coś uciekało w nocy. Na pewno nie czas. · Syczało? Ja tylko tykam, przysięgam. | Gorsza noc? Mnie raz stanęły wskazówki. · Tym razem dziura w całym naprawdę była. |
| wieczór: wietrzenie | Powietrze stoi. Nie znoszę, jak coś stoi. · Do klamki nie sięgam. Ale kibicuję. · Duszno jak w szufladzie zegarmistrza. · Przeciąg mnie nie ruszy. Mam ciężkie buty. | Otworzyłbym okno, ale mam tylko wskazówki. · Powietrze stoi. Ja przynajmniej chodzę. |
| odkrycie | Wiedziałem, że coś tyka w tych danych. · Panel liczył, ja udawałem, że też liczę. · Zapisałem odkrycie. Nazwę je swoim imieniem. · Weekendy są inne. Ja weekendów nie mam. ¹ | Zaskoczyło mi w trybikach. Mamy trop. · Odkrycie! Kukułka aż wyszła przed czasem. |
| brak danych | Aparat milczy. Wreszcie mam ostatnie słowo. · Poszedłbym po dane. Mam nawet buty. · Dane się spóźniają. Wstyd mi za kolegów. · Most milczy. Zadzwoniłbym, tylko do kogo? | Nikt nie przyszedł. Ja byłem punktualnie. · Dane mają kwadrans akademicki. Czekam. |

¹ Pasuje tylko do odkrycia o weekendach (O03). Na pozostałych odkryciach zamiast niego: „Zaskoczyło mi w trybikach. Mamy trop.”

## Przerywniki

| ID | Zdanie | Rezerwa |
|---|---|---|
| G01 zmiana czasu | Ktoś przesuwa czas, a pocę się ja. | Mam wszystko na czas. Tylko nie wiem który. |
| G02 poniedziałek | Punktualność mam w umowie. Entuzjazmu nie. | Sam siebie budzę. Kto budzi budzika? |
| G03 piątek | Odpoczywam od punktualności. Punktualnie. | Hamak to dla mnie wahadło na urlopie. |
| G04 Nowy Rok | Postanowienie: więcej dzwonić. Przepraszam. | Odliczanie? Robię to cały rok za darmo. |
| G05 Święta | Kupiłem aparatowi prezent. Z bólem sprężyny. | Byłem grzeczny. Nigdy się nie spóźniłem. |
| G06 Wielkanoc | Jajko czy budzik? Ja zdążyłem przed kurą. | Wyklułem się punktualnie. Kura w szoku. |
| G07 wiosna | Zakwitnie, kiedy zadzwonię. Taki mamy układ. | Podlewam ostrożnie. Nie jestem wodoszczelny. |
| G08 jesień | Czas leci. Ja prawie też. | Liście lecą, ja się trzymam. Dzwonków. |
| G09 śnieg | Ulepiłem siebie. Wyszedłem na bałwana. | Zrobiłem sobie fana. Topnieje z zachwytu. |
| G10 owce | Ja liczę owce, owce liczą na mnie. | Owca spóźniona. Zgłaszam do pasterza. |
| G11 drzemka | Drzemka? Po moim mechanizmie. | Wciskam sobie drzemkę. Konflikt interesów. |
| G12 nurek | Przebrałem się za konkurencję. Wyszło lepiej. | Pod wodą dzwonię bąbelkami. Słychać słabo. |
| G13 szachy | Księżyc ma przewagę. Gra nocą u siebie. | Mat o świcie. Wtedy ja wygrywam. |
| G14 kapcie | Puchate kapcie. Wizerunek twardziela padł. | Chodzę jak w zegarku. Puchatym. |
| G15 dyrygent | Dyryguję ciszą. Swoją partię zagram rano. | Moja orkiestra nie fałszuje. Nie gra. |
| G16 siłownia | Hantle? Ja codziennie podnoszę ludzi. | Pracuję nad formą. Okrągłą. |
| G17 prysznic | Wodoodporny? Zaraz się okaże. | Śpiewam pod prysznicem. Wychodzi dzwonienie. |
| G18 kucharz | Kucharz ze mnie żaden. Minutnik świetny. | Gotuję na czas. Smak to inna sprawa. |

## Odrzucone w ocenie (dla porządku)
| Zdanie | Powód |
|---|---|
| Najwyższy czas wietrzyć. Znam się na czasie. | zalecenie |
| Ja się nakręcam, pokój niech się wietrzy. | zalecenie w trybie rozkazującym |
| Dwutlenek węgla to nie ja. Ja nie oddycham. | aluzja do bezdechu, sugeruje winę użytkownika |
| Aparat miał gorszą noc. Nie cieszę się. Tak. | drwina na karcie problemu |
| Sobie oklasków. Reszcie wszystkiego dobrego. | niezręczna polszczyzna |
| Ciągnie mnie do łóżka. Ja ciągnę za dzwonek. | senność |
| Wybiła Twoja godzina. Wybiłem ją osobiście. | idiom o czyimś końcu, wobec pacjenta nietaktowny |

## Wnioski na przyszłe zdania
- Najlepiej działa idiom przerobiony pod scenę: „po moim mechanizmie”, „wyszedłem na bałwana”, „podnoszę ludzi”, „puściła parę”.
- Zazdrość o aparat bawi, gdy kończy się małym zwycięstwem budzika, a nie samą skargą.
- Na kartach problemu działa solidarność (własna wpadka budzika) i dyskrecja, nie dowcip.
- Ciekawostki encyklopedyczne (zegar cezowy, zegary wodne, noc polarna) bez puenty przegrały z kretesem: wymagają wiedzy, a rano nikt jej nie ma.

## Jak trafiło do specyfikacji (5.7.5) i kodu

Zestaw powstał na starszym podziale pul (6 typów z v0.6c). W specyfikacji 1.2 pule mają 22 typy, więc zdania trafiły tam, gdzie pasują, z tymi odstępstwami:

| Miejsce w spec | Co weszło | Odstępstwo od zestawu wyżej i powód |
|---|---|---|
| dobra noc (S2a, S2c, K22), kolumna „Propozycje” | 4 zwycięzców + zostają `Wstałem pierwszy. Jak zawsze.` i `Noc za nami. Wskazówki w komplecie.` | wszystkie 4 nowe są oceniające („medal”, „wzorowa”, „nie ma dziury”), więc trafiły na listę zdań uspokajających (R-EKR-32); dwa stare neutralne zostają, żeby bez warunku uspokojenia pula miała co najmniej 3 zdania (R-HUM-27); pula ma 9 zdań |
| seria i kamień milowy | 3 zwycięzców + rezerwa `Tyle nocy, że muszę dokręcić sprężynę.` | `Jubileusz! …` pasuje tylko do kamienia milowego, a pula obsługuje też zwykłą serię (S2b) |
| wyciek (S-K-10) | 4 zwycięzców | brak |
| wieczór: wietrzenie (S-K-01) | 4 zwycięzców + 2 rezerwowe + zostaje `Okno otwarte, ja na straży.` | do 8 zdań |
| odkrycie (S1, S-K-21) | 3 zwycięzców + rezerwa `Zaskoczyło mi w trybikach. Mamy trop.` | `Weekendy są inne. …` pasuje tylko do odkrycia o weekendach |
| brak danych (S5) | 4 zwycięzców | brak |
| pula „noc” (S9) | zdania scen G10–G15 | W12 bez zmian (poza zestawem) |
| nowa tabela „Zdania scen przerywników” (S2d) | G01–G18 | G15 rano dostaje rezerwę `Moja orkiestra nie fałszuje. Nie gra.`, bo „zagram rano” pasuje tylko wieczorem (S9) |

Kolumna „Z v0.6c i kanwy” bez zmian: to zdania, które panel losuje do akceptacji pul.
