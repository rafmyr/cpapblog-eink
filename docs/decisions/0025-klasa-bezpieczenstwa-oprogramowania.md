# ADR-0025: Klasa bezpieczeństwa oprogramowania według IEC 62304 jako wzorzec, bez deklaracji

| | |
|---|---|
| Data | 01.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 01.11.2026): klasa B jako wzorzec procesu, bez deklaracji |
| Źródło | ADR-0020 pkt 7 (IEC 62304 jako wzorzec, klasa jako propozycja w sekcji 10); ADR-0002, ADR-0012, ADR-0015, ADR-0016; analiza MDR (wersja robocza, otwarta); specyfikacja techniczna, sekcje 2, 10 i 11; ADR-0021, ADR-0023 |

## Kontekst

IEC 62304 dzieli oprogramowanie wyrobu medycznego na trzy klasy według najgorszej szkody, do której może doprowadzić błąd oprogramowania, po uwzględnieniu zabezpieczeń poza nim (lekarz, procedury, inne urządzenia):

| Klasa | Najgorsza możliwa szkoda | Co to znaczy dla pracy (w uproszczeniu) |
|---|---|---|
| A | żadna szkoda dla zdrowia | wymagania i testy całości; bez podziału na jednostki i bez ich osobnej weryfikacji |
| B | szkoda niepoważna | dodatkowo architektura z podziałem na jednostki, weryfikacja jednostek i integracji, wymagania wobec składników obcych (SOUP) i przegląd ich znanych błędów |
| C | śmierć albo poważna szkoda | dodatkowo szczegółowy projekt każdej jednostki i dodatkowe kryteria jej weryfikacji |

Projekt nie deklaruje zgodności z normą ani statusu wyrobu medycznego: status wobec MDR jest otwarty, a ADR-0015 opisuje tylko przeznaczenie. Klasa jest tu wzorcem staranności: mówi, ile testów, przeglądów i dokumentacji wkładamy w firmware. Klasa 62304 nie jest klasą MDR (I, IIa, IIb, III) i z niej nie wynika, ale obie opierają się na tym samym pytaniu o możliwą szkodę.

**Co mogłoby pójść źle** (skróty szkody jak w analizie MDR, część 3):

| Błąd oprogramowania | Przykład | Szkoda | Co ją ogranicza poza oprogramowaniem |
|---|---|---|---|
| panel milczy przy realnym problemie (FU, PP) | błąd w normie albo w regule L-01 i panel pokazuje `Noc w normie` przy utrwalonych bezdechach centralnych; zawieszony panel pokazuje starą noc jak ostatnią | opóźniona rozmowa z lekarzem. Analiza MDR (część 2 pkt 4): część osób na ASV ma niewydolność serca (SERVE-HF, poziom A), więc opóźnienie może dotyczyć poważnego pogorszenia | lekarz prowadzący i wizyty kontrolne; aparat sam zapisuje dane i ma własny podgląd; panel ocenia noce z opóźnieniem dni i mówi to wprost stałą ramką `Panel nie ostrzega o nagłych stanach…` (spec 1.3); saturacja (L-06) w 1.x wyłączona |
| panel każe iść do lekarza bez powodu (FA) | próg źle wczytany i L-03 pojawia się przy stabilnej terapii | niepokój, niepotrzebna wizyta; szkoda niepoważna | lekarz ocenia dane; budżet alarmów WY6 |
| panel wskazuje złą przyczynę (BP) | H-04 przy nocy, w której przyczyną był wyciek | pacjent mógłby szukać przyczyny w ustawieniach; ryzyko samodzielnej zmiany | ADR-0002 i ADR-0012: panel nie podaje wartości, kierunku ani drogi zmiany; tylko „obszar do omówienia z lekarzem” |
| błąd poza oceną nocy | pogoda, budzik, humor, kokpit bez reguł | brak szkody dla zdrowia | nie dotyczy |

Wyciek danych poza dom (R-NF-07) jest szkodą dla prywatności, nie dla zdrowia, więc 62304 go nie klasyfikuje; pilnują go scenariusze bezpieczeństwa z sekcji 2 (T-NF-26…T-NF-29).

## Warianty

| Wariant | Uzasadnienie | Skutek i koszt | Rekomendacja |
|---|---|---|---|
| A. Klasa A | panel tylko informuje, decyzje podejmuje lekarz; granica ADR-0002 zamyka drogę do zmiany ustawień | najmniej pracy: wystarczą testy całości. Trudno to obronić: analiza MDR uznaje funkcje L, H i raport za kandydatów na oprogramowanie będące wyrobem (klasa MDR co najmniej IIa), a FU w tabeli wyżej jest szkodą dla zdrowia, nie jej brakiem. Gdyby ocena statusu skończyła się wyrobem medycznym, klasy A nie dałoby się utrzymać i trzeba by dopisać testy jednostek wstecz | odrzucony |
| **B. Klasa B dla całego firmware** | najgorszy realny skutek błędu to opóźniona albo zbędna rozmowa z lekarzem. Poważną szkodę zatrzymują zabezpieczenia poza oprogramowaniem: lekarz prowadzący, dane w aparacie, opóźnienie oceny w dniach z ramką o stanach nagłych, granica ADR-0002 i ADR-0012 | jednostki = komponenty T-KOMP (ADR-0021) z testami na komputerze, testy integracji z atrapami, lista SOUP z wymaganiami i przeglądem znanych błędów, niezależny przegląd zmian w częściach z ryzykiem klinicznym. Większość już jest w planie etapu 3 (sekcje 6, 10, 11); dochodzą dwa kroki przy wydaniu (pkt 3 i 4 decyzji). Bez szczegółowego projektu każdej jednostki | **rekomendowany** |
| C. Klasa C dla całego firmware | ASV u osób z niewydolnością serca (SERVE-HF); analiza MDR pozwala na klasę IIb; FU mogłoby opóźnić rozmowę o poważnym pogorszeniu | dochodzi szczegółowy projekt każdej jednostki i dodatkowe kryteria jej weryfikacji: kilka razy więcej dokumentacji, którą przy jednym autorze trzeba utrzymywać przy każdej zmianie progu czy tekstu. Zysk wobec B: formalny, bo testy przypadków granicznych i przeglądy i tak obejmują reguły | odrzucony teraz; wraca, gdy zajdzie warunek z „Konsekwencji” |

Rozważony i odrzucony podwariant B: klasa B tylko dla części oceniających noce, a A dla reszty (budzik, pogoda, humor). 62304 pozwala na różne klasy części, gdy są od siebie oddzielone. Firmware to jeden program na jednym układzie bez ochrony pamięci między komponentami, więc błąd w części „A” (np. zapis za tablicą) może zepsuć stan części „B”. Uzasadnienie oddzielenia kosztowałoby więcej niż testy całości w klasie B.

## Decyzja (proponowana: wariant B)

1. **Klasa B jako wzorzec dla całego firmware panelu**, bez deklaracji zgodności z IEC 62304 i bez twierdzenia o statusie wobec MDR. W dokumentach piszemy „proces według klasy B normy IEC 62304 jako wzorca”.
2. **Jednostki i testy.** Jednostką jest komponent T-KOMP (ADR-0021). Każdy komponent rdzenia ma testy na komputerze; adaptery mają atrapy. Komponenty z ryzykiem klinicznym (pobieranie z AirBridge T-KOMP-03, magazyn danych T-KOMP-05, norma T-KOMP-06, silnik reguł T-KOMP-07, wybór ekranu T-KOMP-08, treść i rysowanie T-KOMP-09, raport A4 T-KOMP-20) mają w sekcji 11 kryteria zaliczenia testu jednostki i przypadki graniczne dla każdego wzorca L.
3. **SOUP.** Lista T-SOUP (sekcja 10) z przypiętymi wersjami. Dla składników, od których zależy ocena nocy i ekran (rdzeń Arduino, `WiFi` i `HTTPClient`, `LittleFS`, ESP-IDF, mbedTLS, GxEPD2, Adafruit GFX) zapisane wymagania: co składnik musi robić (np. MD5 zgodne z RFC 1321 dla logowania Digest; pełne i częściowe odświeżenie UC8179) i czym to sprawdzamy.
4. **Przed każdym wydaniem** przegląd opublikowanych list zmian i znanych błędów tych składników dla przypiętej wersji, z wnioskiem w notatce wydania („brak błędów dotyczących panelu” albo lista z oceną).
5. **Rozwiązywanie problemów** bez rejestru (ADR-0020 pkt 7): błąd → przypadek T-TEST, który go odtwarza → poprawka → wpis w CHANGELOG.
6. **Przegląd.** Zmiany w komponentach z pkt 2 i w danych reguł (progi, katalog reguł, teksty, ADR-0023) przechodzą niezależny przegląd przed scaleniem; pozostałe zmiany zwykły przegląd.
7. **Ryzyko kliniczne** zostaje w analizie MDR (ISO 14971 tylko jako odniesienie), bez osobnego rejestru ryzyk. Szkody z tabeli „Co mogłoby pójść źle” mają testy: reguła nieoceniona blokuje uspokojenie (R-EKR-32, ADR-0023 pkt 5), dane nieświeże widać na ekranie (R-NF-26), budżet alarmów (WY6), żadnych tekstów z wartością albo drogą zmiany ustawień (WY7).

## Konsekwencje

- Zachowanie panelu się nie zmienia; to decyzja o procesie. Spec funkcjonalna bez zmian.
- Koszt wobec dzisiejszego planu etapu 3: dwa kroki przy każdym wydaniu (pkt 3 i 4) i kryteria zaliczenia testów jednostek w sekcji 11. Przy jednym autorze to ok. jeden dzień pracy na wydanie (`[szacunek]`).
- Sekcja 12 (plan wydań) dostaje do kryteriów wydania: lista SOUP z przypiętymi wersjami i przeglądem znanych błędów, wynik testów jednostek i integracji.
- **Ponowna ocena klasy** (nowy ADR) przy każdym z warunków: opinia o statusie MDR uzna panel za wyrób medyczny klasy IIb albo wyższej; zmiana granicy ADR-0002 (wartości, kierunek, droga zmiany); włączenie oceny saturacji (L-06) albo innego monitorowania bliskiego czasowi rzeczywistemu; kolejny tryb terapii (ADR-0016); publikacja z gotowymi binarkami.
- Do odnotowania w analizie MDR przy przeglądzie etapu: klasa oprogramowania jako wzorzec procesu i jej uzasadnienie (tabela „Co mogłoby pójść źle”).
