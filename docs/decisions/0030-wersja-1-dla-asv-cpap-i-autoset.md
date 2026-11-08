# ADR-0030: Wersja 1 obsługuje ASV, CPAP i AutoSet

| | |
|---|---|
| Data | 04.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 04.11.2026); zastępuje ADR-0016 w pkt 1–2 (pkt 3–6 ADR-0016 bez zmian); zmienia zakres przeznaczenia z ADR-0015 (adnotacja); ADR-0029 bez zmiany decyzji (skutek w konsekwencjach); ADR-0002, ADR-0012 bez zmian |
| Źródło | decyzja 04.11.2026 |

## Kontekst
ADR-0016 (24.10.2026) ograniczył wydanie 1.x do jednego trybu terapii, ASVAuto (egzemplarz referencyjny: AirCurve 10 CS PaceWave (ASVAuto)). CPAP, AutoSet i ASV ze stałym EPAP miały przyjść w kolejnych wydaniach, każdy po własnych nocach strojenia, zamrożeniu progów i okresie testowym. Ich warunki i teksty zostały w spec 4.11 w kolumnie „następne wydanie”, a noc w takim trybie panel zapisywał bez oceny (R-DANE-29).

04.11.2026 autor projektu zmienił cel wersji 1: tryb raportu i pozostałe funkcje mają być gotowe dla ASV, CPAP i autoCPAP; ma to być pierwsza pełna wersja do publicznych testów; przed pełnym wydaniem nie będzie innych testów poza trwającym testem sprzętu i baterii; funkcjonalność ma być pełna jak dla ASV (wykorzystać to, co już jest).

Panel ma więc działać u osób, które śpią na innych trybach niż egzemplarz referencyjny, a przed wydaniem nie będzie ani nocy strojenia w tych trybach, ani testu rodzaju bezdechu, ani kalibracji (ADR-0029). Spec ma już większość materiału: kolumnę CPAP i AutoSet w 4.11, warunki i teksty z 5.12.x dla AutoSet, kolumnę ASV ze stałym EPAP.

## Decyzja
1. **Wydanie 1.x obsługuje cztery tryby terapii aparatów ResMed serii 10**: `CPAP`, `AutoSet`, `ASV` (stałe EPAP) i `ASVAuto` (wartości tagu `MOP`, spec 4.11). Każda noc w jednym z tych trybów jest oceniana wariantem swojego trybu z tabel 4.11; reguła spoza tabel działa tak samo w każdym trybie (ADR-0016 pkt 3 bez zmian).
2. **Ten sam zestaw funkcji co w ASV.** Rdzeń, moduły i stany reguł z 2.6 obowiązują w każdym trybie. Ze wzorców L aktywne z ekranem i pilnością są L-03 (≥ 10 / h), L-08, L-09 i L-11; reszta działa w stanie „tylko raport” (ADR-0029). L-01 wychodzi w wydaniu 1.0 w stanie „tylko raport” we wszystkich trybach, bo test rodzaju bezdechu nie odbędzie się przed wydaniem; aktywacja po teście zmianą zestawu danych (ADR-0029 pkt 4), w trybie bez nocy strojenia po kalibracji w tym trybie. „Wersja 1” w tym ADR to wydanie 1.0.
3. **Gdzie spec nie ma warunku dla trybu, reguła w tym trybie nie działa**; nie dopisujemy nowych warunków ani tekstów. Dotyczy to: L-04 i H-02 w CPAP (ustawienia bez maksimum) i w ASV ze stałym EPAP (wariant do wyboru przez lekarza PSG); H-01 w CPAP (ustawienia bez minimum), tak jak w ASV i ASVAuto. Dane tych reguł zostają w K2 i w raporcie.
4. **Tryb bieżący.** Reguły i ekrany, które zależą od trybu, ale nie od pojedynczej nocy (np. wiersze K1 i K2, kroki S8, karencja L-01), biorą tryb ostatniej nocy w jednym z czterech trybów. Wyjątek: zdanie o ASV w strefie 9 raportu stoi, gdy okres raportu ma choć jedną noc ASV (R-LEK-34), bo zdanie bezpieczeństwa nie ma znikać po zmianie trybu. Reguła z wariantem w tabelach 4.11 liczy swoje okno tylko z nocy w trybie bieżącym. Zmiana trybu między obsługiwanymi to zmiana ustawień (4.8), jak dotąd.
5. **R-DANE-29 tylko dla trybu nieznanego i trybów spoza listy** (np. APAP i VAuto, jeśli spec ich nie opisuje). Zachowanie takiej nocy bez zmian: zapis i liczby, bez oceny reguł zależnych od trybu, poza normą i oknami.
6. **Progi bez nocy strojenia.** Progi reguł w CPAP, AutoSet i ASV ze stałym EPAP zostają `[PRÓG]` z dopiskiem „bez nocy strojenia w tym trybie”. Nie dostają nowych wartości. Kalibracja, zamrożenie i okres testowy na tryb przechodzą na czas po wersji 1.

## Konsekwencje
- **Reguły w CPAP i AutoSet nie dostaną przed wydaniem żadnej prawdziwej nocy**, tylko dane syntetyczne (przypadki 7.8). Dane referencyjne pochodzą wyłącznie z trybu ASVAuto. Ryzyko łagodzą granica ADR-0002 (panel najwyżej wskazuje rozmowę z lekarzem) i stan „tylko raport” dla wszystkich reguł bez kalibracji (ADR-0029).
- **Pola danych AirSense 10 w AirBridge nie są sprawdzone na egzemplarzu**: wartości `MOP`, tagi `MPI`, `MPA`, `IPC`, `EPR` i znaczenie kanałów `Press`, `EprPress` znamy z kodu AirBridge, nie z aparatu. Do sprawdzenia na egzemplarzu.
- **Publiczne testy to użytkownicy inni niż autor projektu.** Analiza MDR jest otwarta, a spec wymaga jej zamknięcia przed użyciem poza użytkiem osobistym (2.6, wiersz „przed publikacją poza użytek osobisty”; WY12). Ta decyzja tego nie rozstrzyga: przed oddaniem panelu pierwszej osobie z zewnątrz potrzebna jest decyzja.
- **Etap 2 nie spełni przed wydaniem swoich kryteriów zakończenia**: kalibracji (WY3), zamrożenia i okresu testowego (WY16), testu rodzaju bezdechu i kalibracji. Wersja 1 wychodzi z progami `[PRÓG]`.
- Przeznaczenie (ADR-0015) obejmuje cztery tryby w aparatach ResMed serii 10; zdanie „jeden tryb, dopóki kolejne nie przejdą walidacji” z ADR-0016 przestaje obowiązywać.
- Zmiany w spec (04.11.2026), najważniejsze: spec 1.2, 1.5, 2.1, 2.2, 2.5, 2.6, 3.6, 4.4, 4.11 (kolumny bez „następne wydanie”, tryb bieżący), R-DANE-29 z fail-path, R-DANE-32, R-KOK-15, R-SKI-08, R-SKI-43, R-HIP-10, R-LEK-34, przypadek 50, WY13, WY16, 10.1; `progi.csv` (kolumna `tryb`, wpis trybów obsługiwanych); w spec technicznej plik progów z polem `tryb`, nagłówek zestawu z listą trybów, testy T-TEST-150 i T-TEST-215.
- Przy okazji (zmiana drobna, poza decyzją): strefa 6 raportu dostała zapis `Bez zmian w okresie raportu.` dla okresu bez zmian ustawień (plansza `Raport-A4-L03-autoset`).
- Decyzja (04.11.2026): wyjątek dla bezdechów centralnych obowiązuje we wszystkich trybach; do zaliczenia testu rodzaju bezdechu bezdechy centralne (E-33, linie normy CA/h, krok S8 przy CA/h) nie kierują do lekarza w żadnym trybie, liczby zostają w K2 i w raporcie. Koszt: wzrost bezdechów centralnych w CPAP i AutoSet widać tylko w kokpicie i raporcie.
- Zmiana stanu reguły z „tylko raport” na aktywną w trybie bez nocy strojenia wymaga kalibracji w tym trybie, nie samej kalibracji na ASVAuto.
- Koszt: wersja 1 ocenia trzy tryby, których nie sprawdzono na prawdziwych nocach z panelem. Zysk: panel może trafić do osób testujących na innych aparatach, a ich noce dadzą materiał do kalibracji po wersji 1.

## Odrzucone warianty
- **CPAP i AutoSet tylko z raportem, bez ekranów** (tryb spoza listy z R-DANE-29 plus raport). Sprzeczne z „pełną funkcjonalnością jak dla ASV”.
- **Nowe warunki L-04 i H-01 dla CPAP i L-04 dla ASV ze stałym EPAP**, żeby żadna reguła nie milkła. To nowe reguły kliniczne bez źródła i bez przeglądu lekarza PSG; przekracza zakres decyzji „wykorzystać to, co już mamy”.
- **Przenieść progi strojone na ASVAuto na pozostałe tryby jako skalibrowane.** ADR-0016 pkt 2: progi z ASV się nie przenoszą; zostają `[PRÓG]` z dopiskiem.

## Adnotacja (04.11.2026, konsultacja kliniczna, bez nowego ADR)
Decyzja po konsultacji klinicznej (04.11.2026): lista z pkt 3 dostaje H-02 w AutoSet (zastąpione H-08), a H-01 i H-04 nie działają w żadnym trybie (wycofane w wydaniu 1.x). Uwagi z konsultacji klinicznej uwzględniono przy zestawie reguł dla CPAP i AutoSet; w CPAP nie dodajemy reguły „ciśnienie za niskie”. Karencja L-01 w CPAP i AutoSet liczy się tylko od znanej daty rozpoczęcia leczenia (R-SKI-08), duży wyciek zostaje 24 L/min na stałe (wariant A), L-04 w AutoSet liczy tylko OA, L-12 ma pilność docelową C, a stan odchylenia i pasmo normy S8 nie kierują do lekarza (R-NOR-23, R-EKR-36).

## Zmiana 04.11.2026 (bez zmiany decyzji)

Pierwotne zawężenie wersji beta do ASV i ASVAuto wycofano 04.11.2026 według zasady autora projektu, że funkcjonalności panelu nie ogranicza się bez jego wyraźnej zgody: wersja beta działa we wszystkich czterech trybach (4.11). CPAP i AutoSet sprawdzone tylko na danych syntetycznych (scenariusze `autoset` i `cpap` symulatora z aparatem AirSense 10 w atrapie). Doprecyzowania trybowe: tytuł L-07 w CPAP i AutoSet `Ciśnienie rośnie od tygodni`, K1 i K2 według trybu (R-KOK-15), E-36 poza ASV na M-05p, trend `Wsparcie` z Δ 2 cmH2O jak EPAP.

## Zmiana 2026-11-05 (bez zmiany decyzji)

Adnotacja konsultacji klinicznej dla CPAP i AutoSet (wyżej) weszła do wersji beta: katalog reguł ma H-01, H-04 i H-02 w AutoSet jako wycofane (blokada w spec), H-08 przy L-04 w AutoSet i przy L-01 po zmianie ustawień w CPAP i AutoSet, L-04 w AutoSet tylko z OA, karencję L-01 tylko od klucza `poczatek_leczenia`. Tryby `CPAP`, `AutoSet` i `ASV` nadal sprawdzone tylko na danych syntetycznych. Pozostałe uproszczenia: `firmware/prototyp/README.md`, sekcja „Uproszczenia”.

## Zmiana 2026-11-06 (decyzje, bez zmiany decyzji)

Adnotacja z 04.11.2026 („duży wyciek zostaje 24 L/min na stałe, wariant A”) zostaje dla E-20, M-08, S-K-10 w tonie „problem”, S-K-14, S-K-31, L-09 i K2, ale noc niepewna (4.9) liczy się od 06.11.2026 także przy M-07 P95 ≥ 15 L/min (wariant C, decyzja, świadome odejście od konsultacji klinicznej; kalibracja po okresie testowym). W wersji beta zrobione: R-KONF-42 (klucz `uwaga_o_masce`, pole formularza U-03) i wariant opisowy S-K-10 `Wyciek widoczny`, krok L-02 `→ Omów wynik z lekarzem w najbliższych tygodniach.`. Zdanie o karencji L-01 w strefie 6 raportu: po wersji beta. Dopisek tego samego dnia (model pasm wycieku): od 15 L/min wyciek nazywany jest „dużym” na ekranach (`nie oceniam: duży wyciek`, `ostatnia noc z dużym wyciekiem`) i od tej wartości wchodzi S-K-10 w tonie „problem”; E-20, M-08, S-K-14, S-K-31, L-09 i K2 zostają przy 24 L/min, E-23 przy 27; „Uwaga o masce od” 10–15, domyślnie 10, steruje tylko `Wyciek widoczny` poniżej 15. **Dopisek 08.11.2026:** decyzja o 15 L/min i model pasm wycieku mają od 08.11.2026 własny zapis w [ADR-0034](0034-model-pasm-wycieku-p95.md); L-09 liczy od tego dnia noce z P95 ≥ 15 L/min (siatka dla pasma 15–24), więc zdanie „L-09 zostaje przy 24 L/min” wyżej jest nieaktualne.

## Zmiana 2026-11-08 (ADR-0034)

Model pasm wycieku P95 i siatka L-09 dla pasma 15–24 L/min: [ADR-0034](0034-model-pasm-wycieku-p95.md). Adnotacja z 06.11.2026 wyżej dostała odesłanie.
