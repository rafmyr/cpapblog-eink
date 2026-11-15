# ADR-0032: Aktualizacja firmware pobierana z wydań GitHub

| | |
|---|---|
| Data | 05.11.2026 |
| Status | przyjęta (decyzje właściciela projektu 05.11.2026: nowy ADR, podpis nie musi być od pierwszego wydania, sprawdzanie wersji przez panel plus opcjonalny automat); wdrożenie: pierwsza wersja po wersji beta |
| Źródło | [ADR-0001](0001-przetwarzanie-lokalne.md), [ADR-0013](0013-wyjatki-czas-sieciowy-i-leki-w-raporcie.md), [ADR-0024](0024-aktualizacja-oprogramowania-i-zestawu-danych.md) (wariant D był odrzucony), [ADR-0025](0025-klasa-bezpieczenstwa-oprogramowania.md), R-NF-07 (sekrety poza obrazem: pkt 7) |

## Kontekst

Panel można dziś zaktualizować tylko kablem albo w oknie serwisowym w sieci lokalnej (ADR-0024 wariant A), czyli przy każdej poprawce potrzebny jest komputer przy panelu. Przy testach na sprzęcie w jeden dzień wyszły cztery błędy widoczne dopiero na panelu (zegar, czas z AirBridge, przyciski, zapis katalogów). Wariant D z ADR-0024 („pobieranie z internetu, własny serwer albo wydania GitHub”) odrzucono 01.11.2026, bo łamie ADR-0001 i R-NF-07: panel łączyłby się poza dom w innym celu niż pogoda, pyłek i czas.

## Decyzja

1. **Panel sam sprawdza, czy jest nowsza wersja firmware**, w wydaniach GitHub jednego, stałego repozytorium wydań. To czwarty dozwolony ruch wychodzący obok pogody, pyłku i czasu (zmiana ADR-0001 i ADR-0013, R-NF-07). Zapytanie nie zawiera danych terapii, użytkownika ani numeru seryjnego aparatu; serwer widzi tylko adres IP domu i wersję panelu.
2. **Sprawdzenie wersji: samo, rzadko.** Raz na dobę w oknie z Wi-Fi, które panel i tak otwiera (bez dodatkowego wybudzenia, budżet energii 8.1), oraz na żądanie z ekranu ustawień. Wynik widać na U-01 i U-05 („jest wersja X”).
3. **Instalacja: domyślnie po potwierdzeniu przyciskiem**, z opcjonalnym automatem włączanym w ustawieniach (formularz konfiguracji). Automat instaluje tylko poza nocą i terapią (nie w slocie nocnym, nie przy `rop` = 1), nigdy podczas ekranu z pilnością A albo B i nigdy przy baterii poniżej progu `[PRÓG]`.
4. **Bez cofania się:** panel instaluje tylko wersję wyższą od własnej; obraz trafia do drugiego slotu aplikacji, nowa wersja sama potwierdza, że działa, inaczej bootloader wraca do poprzedniej (ADR-0024 decyzja 3, powrót do poprzedniej wersji). Pliki danych bez zmian; migracje według ADR-0022.
5. **Weryfikacja obrazu, etap 1 (od pierwszej wersji):** połączenie HTTPS z weryfikacją certyfikatu serwera, adres repozytorium wpisany na stałe w firmware, obraz zgodny ze skrótem SHA-256 opublikowanym w tym samym wydaniu, zgodny model płytki i układ partycji.
6. **Weryfikacja obrazu, etap 2 (później, decyzja):** podpis obrazu kluczem trzymanym tylko przez autora projektu, panel sprawdza go kluczem publicznym wkompilowanym w firmware; obraz bez ważnego podpisu nie jest instalowany. Wejście etapu 2 nie wymaga nowego ADR.
7. **Sekrety poza obrazem**: jeden plik firmware dla wszystkich paneli; hasła Wi-Fi, AirBridge i okna serwisowego w pamięci panelu, nie w obrazie wydania. *Adnotacja 05.11.2026:* dwie rzeczy do zrobienia przed pierwszym obrazem wydania: (a) obraz zbudowany ze wzoru `secrets.example.h` (tak buduje CI) nadpisałby na panelu ze źródłem „k” prawdziwą sieć wartościami wzoru; wydanie musi mieć sekrety puste (flaga budowy, np. `-DBEZ_SEKRETOW`), które panel traktuje jak brak; (b) hasło okna serwisowego `OTA_PASS` jest dalej w obrazie i musi przejść do NVS. Odpowiedni test w `testy_konfiguracja_z_telefonu_brzegowe.cpp` jest oznaczony jako znany. Zmiana routera nie wymaga już kabla: sieć panelu (U-02, ◀ 2 s na U-01 albo U-05).

## Konsekwencje

- **Ryzyko do świadomej akceptacji w etapie 1:** bez podpisu skrót SHA-256 chroni przed uszkodzonym plikiem, ale nie przed podmianą: kto przejmie konto GitHub z prawem publikacji wydań, może wgrać dowolny kod na panele w obiegu, z dostępem do ich Wi-Fi i AirBridge. Łagodzą to: domyślna instalacja po przycisku, dwuetapowe logowanie na koncie GitHub i osobne repozytorium wydań z minimalnymi uprawnieniami. Rekomendacja: etap 2 przed pierwszym panelem poza najbliższym otoczeniem. **Ryzyko zaakceptowane (05.11.2026)** na czas, gdy istnieją tylko 2 panele (drugi egzemplarz). Warunek: etap 2 (podpis) wchodzi przed wydaniem panelu komukolwiek innemu; to bramka wydania, nie opcja.
- ADR-0001 dostaje adnotację: czwarty rodzaj ruchu wychodzącego (sprawdzenie i pobranie firmware).
- ADR-0024 wariant D: zmieniony na przyjęty przez ten ADR; wariant A (okno serwisowe) i kabel zostają jako drogi zapasowe i ratunkowe.
- Wydania: proces budowania i publikacji obrazu ze skrótem (później z podpisem), numer wersji rosnący; opis w specyfikacji technicznej 8.8 i 8.10 przy wdrożeniu.
- ADR-0025 (klasa B): każde wydanie nadal przechodzi bramki i przegląd; automat nie skraca drogi wydania, tylko dostarczenie.
- Prywatność: instrukcja dla osób testujących i spec 1.3 mówią wprost, że panel raz na dobę pyta GitHub o nową wersję.

## Zmiana 2026-11-08

Repozytorium wydań z pkt 1 to publiczne `rafmyr/cpapblog-eink` (od 06.11.2026). Wydania z gotowym obrazem (`.bin`, SHA-256 do pobrania przez panel) dopiero po opinii prawnika od wyrobów medycznych, zob. [ADR-0033](0033-licencja-i-dwa-repozytoria.md) pkt 6 (decyzja właściciela projektu 08.11.2026). Do tego czasu panele aktualizuje się kablem albo w oknie serwisowym w sieci lokalnej (ADR-0024 wariant A); kod sprawdzania wersji może powstać, ale bez wydania z obrazem panel nie ma czego zainstalować. Pozostałe warunki pierwszego wydania (obraz bez sekretów, adres repozytorium w firmware, podpis przed panelem poza dwoma egzemplarzami) bez zmian.

## Zmiana 2026-11-09

[ADR-0038](0038-wydania-z-prywatnego-repo-i-token.md) (decyzja właściciela projektu 09.11.2026): na czas testów wewnętrznych źródłem wydań jest osobne prywatne repozytorium tylko na wydania, a panel pobiera je z tokenem trzymanym w pamięci panelu (zmienia pkt 1 i dopisek „Zmiana 2026-11-06”). Adres repozytorium jest parametrem budowy z wartością domyślną, nie stałą w kodzie (doprecyzowuje pkt 5). Bez tokenu panel nie pyta GitHuba. Wydania w publicznym `cpapblog-eink` dalej dopiero po opinii prawnika (ADR-0033 pkt 6). Reszta decyzji bez zmian. Wdrożenie:.

## Zmiana 2026-11-09 (wdrożenie)

Kod po przeglądzie 10.11.2026; nic nie wgrane na panel: sprawdzenie i instalacja z wydań (`src/aktualizacja.cpp`, `src/dyrygent.cpp`, adapter `WydaniaEsp32`), ekran U-07, obraz wydania bez sekretów (`-DBEZ_SEKRETOW`, `[env:e1001_wydanie]`, `tools/wydanie/wydaj.py`), hasło okna serwisowego, adres i token źródła otoczenia i współrzędne pogody w pamięci panelu. Pierwszą wersję z aktualizatorem buduje się z `secrets.h` i wgrywa kablem albo oknem serwisowym; obraz wydania (bez sekretów) tylko na panel, który ma już te wartości w pamięci. Wymagania: R-NF-32…R-NF-36, R-KONF-43…R-KONF-46, R-EKR-61; spec techniczna T-WYM-93…T-WYM-96, T-INT-40.

## Zmiana 2026-11-14 (ekrany serwisowe, decyzja właściciela projektu)

- **Opis wydania ma opcjonalne pole `nowosci`** (lista do 3 linii po najwyżej 40 znaków, `[PRÓG]`), podawane przy wydaniu (`tools/wydanie/wydaj.py --nowosc`, pytanie w `wydaj.sh`). Pole stoi na końcu `wydanie.json`; panele sprzed tej zmiany je pomijają, panele po zmianie bez pola pomijają blok „Co nowego” (R-KONF-56). Pole nie wpływa na decyzję o instalacji, gdy reszta opisu jest poprawna: zły typ pola albo elementu daje pustą listę, a pole z błędem, którego parser JSON nie przyjmuje (samotny surogat, surowy znak sterujący, zagnieżdżenie ponad 16 poziomów), panel wycina i czyta opis bez niego (po przeglądzie). Opis dłuższy niż 4096 B (także przez długie `nowosci`) albo z błędem poza tym polem jest odrzucany jak dotąd. Narzędzie odrzuca linie ze znakami spoza krojów panelu i z wartościami albo czynnościami na ustawieniach aparatu (granica ADR-0002; od filtr na tekście bez ogonków i wielkich liter, z liczbą obok słowa o aparacie, `cm` z liczbą, czasownikami o ustawieniu w każdej odmianie i trybem jako celem ustawienia; `tools/wydanie/README.md`). Linie „Co nowego” panel zapisuje z wersją jednym zapisem NVS (`u07_cnv`), więc po nieudanym zapisie nie pokaże nowości innej wersji.
- **U-07** ma rysunek panelu, wersje `{własna} → {nowa}`, cztery kroki i pas „Nie odłączaj zasilania.” (R-KONF-57); w toku rysunek bez skali (strzałka wchodząca w ekran panelu), żeby nie udawał postępu; stany, teksty i powody bez zmian, pas dochodzi także w instalacji i samosprawdzeniu. Postępu pobierania w procentach nie ma: odświeżanie e-ink w trakcie zapisu obrazu do slotu to ryzyko, którego bez pomiaru na panelu nie da się wykluczyć (do decyzji w karcie zmiany).
- **U-01**: stan aktualizacji w kafelku ↻ zamiast wiersza pod listą (`jest {X} · zainstaluj` w odwróconym polu, przy długiej wersji `jest {X} · instaluj`; bez tokenu `wyłączone: włącz na telefonie`), wersja raz na ekranie; wiersz z pkt 2 tej decyzji zostaje w logu `U-01 · …`.
- **U-05**: dopisek `· jest {X}` / `· odrz. {X}` bez zmian, slot (`app0`, `app1`) tylko w trybie awaryjnym i w logu.

## Zmiana 2026-11-14 (informacja po automacie, decyzja właściciela projektu)

Do tej zmiany automat z pkt 3 kończył się bez żadnego ekranu (wynik tylko w logu i na U-05), a panel po restarcie z instalacji ręcznej sam pokazywał U-07.

- **Po instalacji z automatu panel informuje sam (R-KONF-61):** po potwierdzeniu nowej wersji jako dobrej (po samosprawdzeniu, pkt 4) zapisuje w NVS jednorazową informację i rysuje ją przy pierwszym ekranie dnia bez sprawy, na jego miejscu, jako U-07 „po udanej” z tekstem `Zaktualizowano do {X}.`, zdaniem `Panel zaktualizował się sam.` i listą „Co nowego” z opisu wydania (bez pola `nowosci` sama wersja). Po powrocie do poprzedniej wersji (pkt 4) tym samym mechanizmem U-07 „po nieudanej” z istniejącymi tekstami R-NF-35.
- **Rodzaj ekranu:** ekran serwisowy U-07 w nowym wariancie (bez budzika, belka `Aktualizacja` ze stanem, bez danych terapii, ADR-0007, ADR-0043), nie karta; w nawigacji panel stoi na ekranie dnia, więc ▶ działa jak na ekranie dnia pod spodem, ◀ i ↻ wracają do ekranu dnia, przytrzymania tylko zamykają informację.
- **Pierwszeństwo (ADR-0029):** ekran problemu, ekran lekarski L (A, B), brak danych i synchronizacja, pytanie, Portret nocy rano, ekran slotu ze sprawą albo z linią uwagi, niska bateria i nieaktualne dane idą przed informacją; informacja czeka, nie w slocie nocnym i nie w trakcie terapii. (Zmiana 2026-11-15 niżej: tylko ekran slotu dziennego bez sprawy, nigdy rano.)
- **Koszt:** stan w jednym kluczu NVS `u07_inf` (najwyżej 3 zapisy na instalację: uzbrojenie, pierwsze pokazanie, zamknięcie), bez dodatkowego wybudzenia i bez odświeżenia tylko dla informacji (rysowana w klatce, która i tak się rysuje; najwyżej +1, gdy pierwsze pokazanie wypada w klatce, która inaczej by się nie zmieniła). Teksty nowości przechodzą filtr ADR-0002 przy wydaniu; nic nie wychodzi poza panel (ADR-0001).
- Pytania otwarte: poranny dowód spokoju zasłonięty do przycisku, informacja do końca doby, wariant nieudany bez słowa „sam”.

## Zmiana 2026-11-15 (po przeglądzie zmiany)

Po przeglądzie zmiany (zgoda właściciela projektu 15.11.2026 na tekst nieudanej aktualizacji; rano zawsze wynik nocy).

- **Gdzie staje informacja:** tylko na ekranie slotu dziennego bez sprawy: S-K-33 (bez wariantu CO2), S-K-34, S-K-36, S-K-37, bez linii uwagi. Nigdy rano: nie na dowodzie spokoju S2a…S2d, przerywniku, rozruchu S6, Portrecie ani drugim ekranie poranka i nie przed startem slotu dziennego. Koszt: informacja pojawia się później (od startu slotu, zwykle 60 min po imporcie nocy albo po naciśnięciu na ekranie dnia); panel bez ekranu slotu bez sprawy (bez miasta i obrazków, R-KON-54) czeka bez zapisu na kolejną dobę i do zmiany ustawień jej nie pokaże.
- **Ostrzeżenia zawsze przed informacją (ADR-0045 R-KON-72, ADR-0029):** `Przewietrz teraz.` (S-K-33 w wariancie CO2) i S-K-39 nie są „bez sprawy”; ekrany zużywane przy pokazaniu (S-K-35, S-K-38) też nie, więc nic nie jest liczone jako pokazane pod informacją. Ostrzeżenie powstałe w trakcie doby zdejmuje informację przy najbliższym rysowaniu.
- **Przycisk na informacji:** zamyka ją; krótkie ▶ otwiera pierwszą stronę kokpitu, ◀, ↻ i przytrzymania pokazują ekran dnia; żaden nie zaczyna slotu dziennego ani akcji ekranu pod spodem.
- **Tekst nieudanej (decyzja 12):** po automacie druga linia `Panel próbował zaktualizować się sam.` przed powodem R-NF-35 (pytanie otwarte).
- **Porządki w NVS:** wpis innej wersji albo pokazany w innej dobie (także z przyszłości po złym zegarze) kasowany jednym zapisem przy najbliższym rysowaniu ekranu dnia; najwyżej 4 zapisy klucza `u07_inf` na instalację.
- **Belka:** znak nowej wersji zdjęty tylko na czas rysowania informacji, potem wraca.
- **Koszt e-ink (pytanie otwarte bez zmiany decyzji 11):** w slocie dziennym 08:00–21:00 z informacją 1 odświeżenie e-ink wobec 13 bez niej (pogoda, wietrzenie, „jutro rano” stoją pod informacją do przycisku albo końca doby); ostrzeżenia mimo to ją zdejmują.
