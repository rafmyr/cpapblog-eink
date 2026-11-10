# ADR-0029: Zakres skierowań do lekarza w wydaniu 1.0 i stan „tylko raport”

| | |
|---|---|
| Data | 04.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 04.11.2026); zmiana spec w kodzie wersji beta od 04.11.2026; uzupełnia ADR-0004 (skierowania i hipotezy) i ADR-0016 (tryb ASV); ADR-0002, ADR-0012 i ADR-0015 bez zmian |
| Źródło | uwagi z konsultacji klinicznej 04.11.2026; decyzja 04.11.2026, w wariancie „tylko K2 i raport” |

## Kontekst
Spec 1.2 uruchamia w wydaniu 1.x wszystkie wzorce L poza L-06 (zablokowane do walidacji oksymetru, R-SKI-42), część z pilnością A i buzzerem. Progi wzorców to w większości `[PRÓG]` bez kalibracji, a na ASV część sygnałów pochodzi wyłącznie z przepływu (CA/h, oznaczenie CSR, oddechy na minutę), które aparat liczy sam i które wyciek zakłóca.

Uwagi z konsultacji klinicznej: takie reguły mogą wzmacniać ten sam błąd pomiaru i dawać za dużo pilności A; zaproponowano węższy start: kilka reguł aktywnych, reszta jako dane w raporcie do czasu kalibracji. Pierwsze wydanie służy jednej osobie (egzemplarz referencyjny), ale ma też dać materiał do walidacji w etapie 2.

## Decyzja
1. **Aktywne w 1.0 (z ekranem i pilnością według spec):** L-03 w gałęzi ≥ 10 / h, L-08, L-09, L-11. L-11 dopiero z nowym brzmieniem pytania o senność (konsultacja kliniczna).
2. **L-01 aktywne warunkowo:** po wycofaniu gałęzi „mediana CA/h ≥ 50% mediany AHI” (pkt 1) i po teście, że AirBridge przenosi rodzaj bezdechu poprawnie w kolejnych nocach ważnych. Test jest kryterium zakończenia etapu 2. Bez zaliczonego testu L-01 działa w stanie „tylko raport”.
3. **Nowy stan reguły „tylko raport”** dla L-02 (oba warianty), L-03 w gałęziach 5–10 / h, L-04, L-05, L-07, L-10, L-12. Reguła w tym stanie:
   - liczy się i zapisuje w logu jak aktywna;
   - pokazuje wynik w K2 i w raporcie jako dane albo porównanie, z dopiskiem `bez oceny pilności`;
   - nie ma ekranu lekarskiego, pilności, buzzera, linii uwagi ani linii w niedzielnym S-K-18;
   - gdy jej warunek jest spełniony, nie pozwala na krok uspokajający (warunek uspokojenia, R-EKR-32) i trafia na listę „Otwarte sprawy” z wierszem `w raporcie, bez pilności`. Powód: brak przekroczenia albo przekroczenie niezwalidowanego progu nie znaczy „terapia stabilna” (konsultacja kliniczna); ten sam wzór spec stosuje już dla wsparcia PS w ramce.
4. **Przejście ze stanu „tylko raport” do pilności** dla każdej reguły osobno, po kalibracji na archiwum nocy, wpisem w tabeli „Gotowość funkcji w wydaniu 1.x”. Pilność docelowa reguł opartych wyłącznie na przepływie (L-02, L-05) to najwyżej B (pkt 15). A zostaje dla L-06(b) po odblokowaniu oksymetru.
5. **L-02(a) w trybie ASV przechodzi do rdzenia** (pkt 18), w 1.0 w stanie „tylko raport”. Rdzeń znaczy, że nie da się go wyłączyć plikiem konfiguracji, a nie, że jest alarmem.
6. **Wyciszyć można sposób powiadamiania, nie ocenę.** Użytkownik ma buzzer, „wiem” i „Omówione z lekarzem”; nie ma przełącznika, który wyłącza zapis albo ocenę reguł rdzenia i reguł z listy obowiązkowej. Lista reguł obowiązkowych (R-AW-30) dostaje dla każdej reguły stan minimalny: „aktywna” albo „tylko raport”. Zestaw reguł ze stanem niższym niż minimalny jest odrzucany. Wyjątek (decyzja 04.11.2026): moduł z regułami z listy obowiązkowej wyłącza tylko plik konfiguracji K osoby wydającej panel (R-KONF-32), nie przyciski ani formularz; spec 9.3 wymienia to w liście „mechanizmy”.
7. **Brak danych albo dane niepewne = „nie oceniam”, nigdy wynik prawidłowy.** To już zasada spec (R-DANE-28, R-AW-31, R-EKR-46); przegląd wszystkich reguł L w stanie „tylko raport” pod tym kątem jest częścią zmiany.

## Konsekwencje
- Rano panel zgłasza ekranem tylko AHI ≥ 10, spadek regularności, wyciek i senność; bezdechy centralne dopiero po teście rodzaju bezdechu. Wzrost CSR, EPAP przy maksimum, zmiany oddechu, wzrost EPAP, spłaszczone wdechy i pogorszenie po zmianie ustawień widać tylko w K2 i w raporcie.
- Po zmianie ustawień przez lekarza panel nie zgłosi sam „gorzej od zmiany” (L-12). Porównanie przed / po jest w raporcie.
- Zmiana spec: 2.6 (rdzeń, lista obowiązkowa ze stanem minimalnym), tabela „Gotowość funkcji w wydaniu 1.x” (nowy wiersz „tylko raport”), 1.5 (definicja stanu), 5.12.2 (pilność max B dla L-02, L-05), 5.12.4 (L-01, L-02, L-04), 5.12.6 (reguła „tylko raport” nie wchłania i nie wyklucza), 5.12.12, nowe R-SKI-44, R-EKR-32 i 5.4.13 pkt 1, R-EKR-36, R-EKR-48 i 5.4.15 (karta `Sprawa w raporcie`), 5.5.4 (znacznik w K2), R-AW-30, raport (strefa 2, zdania z dopiskiem `bez oceny pilności`), kryteria zakończenia etapu 2; w spec technicznej lista reguł obowiązkowych ze stanem minimalnym (T-WYM-86).
- Ponowne włączenie pilności to zmiana zestawu danych reguł (ADR-0023); zmiana stanu minimalnego na liście obowiązkowej wymaga aktualizacji oprogramowania (ADR-0024).
- Teksty ekranów L-02, L-04, L-05, L-07, L-10, L-12 (oraz L-01 do testu rodzaju bezdechu i L-03 w gałęziach 5–10 / h) zostają w spec z ID bez zmian, oznaczone „nieaktywne w 1.0, stan tylko raport” (spec 5.12.5).
- Zysk: mniej fałszywych alarmów z tego samego błędu pomiaru, prostsza walidacja w etapie 2, mniej funkcji wyglądających na ocenę kliniczną przy analizie MDR.
- Koszt: panel w 1.0 jest wyraźnie cichszy; sygnały ważne na ASV (CSR, bezdechy centralne do czasu testu) użytkownik musi sam zobaczyć w kokpicie albo raporcie.
- Rozstrzygnięte decyzją (04.11.2026): wyjątek dla pliku K (pkt 6): na ASV przed testem rodzaju bezdechu E-33 i linie normy CA/h bez kroku do lekarza (spec 4.7, R-NOR-21, R-NOR-23, katalog E-33). Koszt: wzrost bezdechów centralnych do zaliczenia testu widać tylko w kokpicie i raporcie.

## Odrzucone warianty
- **Zostawić wszystkie wzorce aktywne do czasu kalibracji.** Wbrew opinii lekarza PSG; pilność A z niezwalidowanych sygnałów przepływu.
- **Stan „tylko raport” jako dzisiejsze C** (znacznik w K2 i linia w niedzielę). Niedzielna linia „Na kontroli” to łagodny alarm z reguł, których lekarz PSG nie uznał jeszcze za wiarygodne.
- **Wyłączyć reguły nieaktywne całkowicie.** Tracimy dane do kalibracji i informację w raporcie, której lekarz PSG chce.

## Adnotacja (04.11.2026, ADR-0030)
Decyzja bez zmian. Skutek [ADR-0030](0030-wersja-1-dla-asv-cpap-i-autoset.md): test rodzaju bezdechu i kalibracja nie odbędą się przed wydaniem wersji 1, więc w całej wersji 1 L-01 i reguły z pkt 3 zostają w stanie „tylko raport”. Ten sam zestaw obowiązuje w trybach `CPAP` i `AutoSet` (i w `ASV` ze stałym EPAP): z ekranem L-03 (≥ 10 / h), L-08, L-09, L-11, reszta „tylko raport”. Przejście reguły do pilności w trybie bez nocy strojenia wymaga kalibracji w tym trybie. Pkt 5 (L-02(a) w rdzeniu) dotyczy obu odmian ASV.

## Zmiana 04.11.2026 (bez zmiany decyzji)

Pkt 7 na pustym ekranie „Otwarte sprawy”: gdy którakolwiek reguła daje „nie oceniam” albo punkt 5 warunku uspokojenia nie jest policzony, panel pisze `Ocena: częściowa` i `Część reguł nie ma jeszcze danych.` zamiast `Ocena: pełna` (R-EKR-50). Warunek uspokojenia 5.4.13 ma w wersji beta nieoceniane pkt 3 (część) i 5, więc `Noc w normie.` i kroki uspokajające nie pojawiają się; zamiast drogi na pusty ekran panel daje krok zamienny R-EKR-32. Reguły „tylko raport” mają w raporcie A4 dopisek `(bez oceny pilności)`.

**Zmiana 2026-11-10** (audyt UX, bez zmiany decyzji): zdanie pod `Ocena: częściowa` brzmi `Części spraw jeszcze nie oceniam.` zamiast `Część reguł nie ma jeszcze danych.` (bez słowa roboczego „reguł”; przyczyną bywa też brak odpowiedzi albo czujnika, nie tylko brak danych). „Nie oceniam” nadal nie znaczy „w porządku” (pkt 7). Po przeglądzie tego samego dnia: przy ocenie niepełnej tytuł `Nie widzę otwartych spraw.`, powód `W tym, co oceniam, nic nie jest w toku.` i zdanie `Części rzeczy jeszcze nie oceniam.` (wcześniej `Brak otwartych spraw.` nad `Części spraw jeszcze nie oceniam.` przeczyły sobie); przy ocenie `pełna` bez zmian.

## Zmiana 2026-11-09 (ADR-0037; bez zmiany decyzji)

Pkt 2 (L-01 i test kalibracji) dotyczy od 09.11.2026 tylko trybów CPAP i AutoSet: aparaty ResMed ASV nie zapisują rodzaju bezdechu ([ADR-0037](0037-asv-bez-rodzaju-bezdechu-i-csr.md)), więc w ASV i ASVAuto test kalibracji jest bezprzedmiotowy, a L-01 ma wariant „Bezdechy mimo pełnego wsparcia” (spec 4.11). Ten wariant, L-04 i H-02 w ASVAuto na bezdechach przy maksymalnym EPAP, E-33 na bezdechach przy pełnym wsparciu i O-12 na bezdechach na godzinę działają w wersji 1 w stanie „tylko raport” według pkt 3, do przeglądu lekarza PSG i konsultacji klinicznej i kalibracji. L-02 w ASV nie ma oceny: zostaje linia w raporcie `Oddychanie okresowe: aparat ASV go nie oznacza.`; zgodnie z pkt 7 to „nie oceniam”, nie „w porządku”, więc zdania uspokajające nie obejmują oddychania okresowego.
