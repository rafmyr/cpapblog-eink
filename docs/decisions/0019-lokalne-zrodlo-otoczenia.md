# ADR-0019: Opcjonalne lokalne źródło danych otoczenia

| | |
|---|---|
| Data | 31.10.2026 |
| Status | przyjęta (decyzja właściciela projektu 30.10.2026), z adnotacją 31.10.2026 |
| Źródło | spec 2.1, 2.4, 8.1, M-23, M-26, M-27, R-DANE-16, R-DANE-33, R-AW-25, R-AW-26, R-AW-27, R-EKR-10, R-EKR-54, R-NF-03, R-KONF-24, R-KONF-35, R-NF-22, R-NF-27, E-08, Załącznik C |

## Kontekst

Spec przewiduje trzy źródła danych otoczenia: serwis pogody i powietrza w internecie (M-26, M-27, R-DANE-16), czujnik Qingping po Bluetooth (CO2 i PM2.5 sypialni, M-23) i czujnik w obudowie panelu (temperatura i wilgotność przy łóżku, M-22). Prototyp miał tylko ostatni: pogody nie pobierał (lewa część stopki pusta, R-AW-07), a Qingping nie jest podłączony, bo nie wiadomo, czy po sparowaniu z inną aplikacją nadal nadaje po Bluetooth.

W wielu domach dane o pogodzie, powietrzu za oknem i CO2 w sypialni już zbiera jakieś urządzenie w sieci domowej (np. Home Assistant, Node-RED albo własny skrypt na serwerze domowym). Panel może je odczytać jednym żądaniem HTTP w sieci lokalnej, bez Bluetooth i bez własnego zapytania do internetu.

## Decyzja

1. **Lokalne źródło danych otoczenia jest opcjonalne.** To dowolne urządzenie w sieci domowej, które na `GET` pod adresem z konfiguracji podaje JSON według kontraktu wersji 1 (spec Załącznik C): bloki `pogoda`, `powietrze`, `sypialnia` (każdy może być `null`) i listę `ostrzezenia`. Opcjonalny nagłówek `X-Token` chroni źródło przed przypadkowymi odczytami w sieci domowej. Adres i token są w konfiguracji panelu (w prototypie `OTO_URL` i `OTO_TOKEN` w `include/secrets.h`, poza repozytorium).
2. **Co panel z niego bierze.** Pogodę do stopki (M-26, `{miasto} {temp}° · {opis}`), PM2.5 za oknem (M-27; na ekranie tylko tam, gdzie spec go używa, nie w stopce), CO2 sypialni (M-23: dopisek `· CO2 {ppm}` w stopce, mediana nocy w S2c) i ostrzeżenie `co2_milczy` (linia E-08). Temperatura i wilgotność przy łóżku zostają z czujnika panelu (M-22); te same pola ze źródła panel zapisuje, ale ich nie pokazuje.
3. **Druga droga do M-23.** Qingping po Bluetooth zostaje w spec jako alternatywa. Gdy panel ma oba, CO2 i PM2.5 sypialni biorą się z Qingpinga (pomiar bez pośrednika), a ze źródła lokalnego tylko to, czego Qingping nie daje.
4. **Pogoda: relacja do ADR-0013.** Bezpośrednie zapytanie panelu do serwisu pogody w internecie zostaje w spec dozwolone (R-DANE-16, R-NF-07). Gdy lokalne źródło jest skonfigurowane, panel bierze pogodę i powietrze z niego i nie pyta serwisu w internecie; gdy źródło nie poda pogody, stopka jest bez pogody (R-AW-07), bez zapasowego zapytania do internetu. Prototyp nie ma klienta serwisu pogody, więc tam pogoda jest tylko ze źródła lokalnego.
5. **Brak źródła albo zła odpowiedź = brak danych.** Bez adresu w konfiguracji funkcja jest wyłączona i panel działa jak dotąd. Wersja kontraktu inna niż 1, odpowiedź nieczytelna albo za duża, odmowa (401, 403) i brak odpowiedzi dają ten sam skutek na ekranie: elementy z tych danych milkną bez komunikatu (R-AW-06, R-AW-07, R-AW-25); powód jest w logu i na ekranie testu. Wartość `null` nigdy nie jest zerem.
6. **Kiedy panel pyta.** Przy każdym wybudzeniu z połączeniem Wi-Fi, po odczycie AirBridge, także w slocie nocnym 23:30–05:00 (R-DANE-33): panel i tak łączy się wtedy z AirBridge, a CO2 sypialni zbiera się z całej nocy (R-DANE-10). Ponowienia jak R-AW-01, z czasami dopasowanymi do kontraktu (odpowiedź poniżej 200 ms): do 3 prób co 2 s, razem najwyżej 15 s na wybudzenie; po wybudzeniu bez odpowiedzi (do pierwszej udanej) i w slocie nocnym tylko 1 próba, najwyżej 6 s (R-AW-26, budżet energii w spec 8.1). Nowych wybudzeń nie ma (R-NF-02).

## Granice bez zmian

- **ADR-0001.** Przetwarzanie dalej tylko na panelu. Dane przychodzą do panelu z sieci domowej; panel do źródła nie wysyła niczego poza samym żądaniem i tokenem: żadnych danych terapii, sypialni, czynników ani raportów. Ruch wychodzący poza dom się nie zwiększa (R-NF-07), a przy skonfigurowanym źródle maleje (pogoda nie idzie przez internet z panelu).
- **ADR-0002.** Dane otoczenia nie zmieniają granic: panel nie mówi nic o ustawieniach aparatu i nie łączy CO2 ze zdrowiem poza tym, co spec już ma (5.8.7, zakazy przy S-K-01 i S-K-13).
- **ADR-0007, ADR-0009.** Bez nowych rodzajów ekranu. Linia E-08 to linia uwagi grupy „dane” (biały pasek), więc w dniu z linią ekran dnia jest ekranem danych (R-HUM-17); budzik dalej nigdy na czarnym tle.

## Warianty odrzucone

| Wariant | Dlaczego nie |
|---|---|
| tylko Qingping po Bluetooth (stan spec do 30.10.2026) | zależy od którego nie da się rozstrzygnąć bez testu; skan Bluetooth kosztuje energię (do 5 min na noc, 8.1) |
| panel sam pyta serwis pogody w internecie | działa, ale to ruch wychodzący z panelu i kod klienta serwisu; gdy w domu jest już źródło danych, jedno żądanie w sieci lokalnej jest prostsze i nie wychodzi z domu |
| źródło lokalne zastępuje czujnik panelu także dla temperatury i wilgotności przy łóżku | czujnik źródła może stać gdzie indziej w sypialni; stopka `przy łóżku` mówi o odczycie w obudowie panelu (audyt UX 30.10.2026) |
| panel wysyła swoje odczyty do źródła (dwukierunkowo) | łamie ADR-0001 (dane z panelu poza panel) i nie jest potrzebne |
| ekran „Przewietrz sypialnię” (S-K-01) od razu w prototypie | warunek ze spec wymaga PM2.5 w sypialni (nie ma go w kontrakcie wersji 1), slotu wieczornego, limitów 7.4 i trybu próbnego (pakiet 3); uproszczony ekran bez nich byłby inną regułą niż spec |

## Konsekwencje

- Nowe w spec: Załącznik C (kontrakt wersji 1), R-DANE-33, R-AW-25, R-AW-26, R-AW-27, R-EKR-54, zdarzenie E-08 `Brak odczytu CO2 od {czas}.` (grupa „dane”, raz na epizod), wiersz w tabeli źródeł 2.1. Zmienione: M-23, M-26, M-27, R-DANE-16, R-NF-03, budżet energii 8.1, R-AW-06, R-AW-07, R-EKR-10 (stopka z dopiskiem CO2), R-EKR-17 (S2c pokazuje tylko kolumny z danymi ostatniej nocy).
- Otwarte kwestie: podpis CO2 w pasku `PRZY ŁÓŻKU · OSTATNIA NOC`, temperatura i wilgotność sypialni ze źródła, czy CO2 ze źródła jest dość częste i dokładne dla progów S-K-01, S-K-13, O-06.
- Panel zależy od zgodności źródła z kontraktem. Zmiana kontraktu wymaga nowej wersji (pole `wersja`); panel obsługuje tylko 1, a każdą inną traktuje jak brak źródła.
- Energia: jedno żądanie w sieci lokalnej na wybudzenie (poniżej 1 s radia przy działającym źródle); przy źródle wyłączonym pierwsze wybudzenie do 15 s Wi-Fi więcej, każde następne i każde nocne do 6 s, najwyżej ok. 5 min na dobę (spec 8.1, R-AW-26).
- Prototyp zapisuje medianę CO2 nocy w osobnym pliku `/sypialnia-co2.csv`; `/sypialnia.csv` zostaje w dawnym układzie, więc starszy firmware go czyta.
- CO2 nocy co najmniej 1000 ppm (próg S-K-01) nigdy nie daje w S2c zdania `Dobre warunki do snu.` (R-EKR-54).

## Adnotacja 31.10.2026 (decyzja)

Decyzja bez zmian; doprecyzowania.

- **Pkt 6, kiedy panel pyta.** „Przy każdym wybudzeniu z połączeniem Wi-Fi” znaczy: przy wybudzeniach, w których panel i tak włącza Wi-Fi (R-NF-03: AirBridge, chwile pobrania pogody z R-DANE-16, zegar, serwer raportu albo formularza), także w slocie nocnym. Przy samych zapisach czujników (R-DANE-09, R-DANE-10) panel nie łączy się z siecią tylko po to, żeby zapytać źródło (R-DANE-33). Skutek: w spec nocna seria CO2 ze źródła ma tyle odczytów, ile nocnych wybudzeń z Wi-Fi (w prototypie co 30 min); czy to wystarcza, rozstrzygnie okres testowy.
- **Pkt 4, pogoda i serwis w internecie.** Przy skonfigurowanym źródle panel bierze ze źródła wszystko, co kontrakt wersji 1 podaje (także `tmax`, `tmin`, `opady_proc`), a z serwisu pogody tylko pola, których kontrakt nie ma: pyłek (S-K-06, grupa pyłku w S-K-21), gdy miasto jest ustawione (R-DANE-16, R-KONF-24). Pole kontraktu z wartością `null` to brak danych bez zapytania zastępczego. Lista „Pola a ekrany” w Załączniku C. Zdanie z „Granic bez zmian” o ADR-0001 czytać tak: przy skonfigurowanym źródle ruch wychodzący maleje do zapytania o pyłek (R-NF-07 bez zmian).
- **Pkt 1, adres i token.** Adres i token są ustawieniem panelu (spec 5.6.4, formularz albo plik, R-KONF-35); token jest w pliku konfiguracji zapisany w postaci nieczytelnej bez panelu, jak hasło Wi-Fi (R-NF-22). „Tylko w sieci lokalnej” jest wymaganiem z weryfikacją (R-NF-27): adres spoza sieci lokalnej (poza adresami prywatnymi RFC 1918, link-local i nazwami `.local`) i przekierowanie panel odrzuca bez wysłania żądania i tokenu. Dzięki temu ADR-0001 trzyma także przy złej konfiguracji (także dla nazw bez `.local` i IPv6).
- **Co działa z samym źródłem.** Do rozstrzygnięcia, czy CO2 ze źródła wystarcza, S-K-01, S-K-02, S-K-13 i O-06 wymagają Qingpinga także w spec, nie tylko w prototypie; ze źródłem bez Qingpinga CO2 trafia do stopki, S2c i E-08.
- **Próg CO2 w S2c.** Ostatni punkt „Konsekwencji” czytać: mediana CO2 nocy **powyżej** 1000 ppm (jak S-K-01 i S-K-13, nie „co najmniej”) nigdy nie daje w S2c zdania `Dobre warunki do snu.` (R-EKR-54). Próg mediany ma w `docs/spec/progi.csv` osobny wiersz `R-EKR-54.co2_mediana`, więc kalibracja S-K-01 nie przesuwa go po cichu.
- **Listy w nagłówku i w „Konsekwencjach”** uzupełnione o wymagania, które ta decyzja zmieniła albo dodała (R-AW-27, R-EKR-54, R-DANE-16, R-NF-03, 8.1, a także R-KONF-35, R-NF-22, R-NF-27).

## Adnotacja 04.11.2026 (decyzja)

Decyzja bez zmian. Ekrany CO2 (S-K-01, S-K-02, S-K-13, wariant CO2 S-K-33) i ocena slotu wieczornego co 30 min z Wi-Fi (R-KON-01) działają tylko w egzemplarzu z włączonymi ekranami CO2: ustawienie egzemplarza `co2` (`tak` albo `nie`, plik konfiguracji panelu), bez niego według źródła. Z adresem źródła (egzemplarz referencyjny) włączone, bez trybu próbnego; bez źródła (egzemplarz testowy) albo z `co2=nie` wyłączone i bez wybudzeń z Wi-Fi wieczorem dla oceny slotu (budżet baterii 8.1). Spec 5.8.8, R-KON-01; kod `src/slot_dzienny.h` `co2Wlaczone()`, `src/plan.h` `WejsciePlanu::co2`.
