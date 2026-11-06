# ADR-0024: Aktualizacja oprogramowania i zestawu danych reguł

| | |
|---|---|
| Data | 01.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 01.11.2026): wariant A; wymaga zmiany spec funkcjonalnej (R-NF-02, R-NF-03, ekran testu sprzętu) |
| Źródło | ADR-0001, ADR-0013, ADR-0020, ADR-0022 (wariant A), ADR-0023 (pkt 4–6); spec R-NF-02, R-NF-03, R-NF-07, R-NF-11, R-NF-18, R-NF-19, WY16; specyfikacja techniczna 3.1, 3.4, 4.2 (T-INT-08), 7.8, 8.2, 8.8; prototyp: `firmware/prototyp/src/dyrygent.cpp` (ArduinoOTA, okno 10 min), `platformio.ini` (`[env:e1001_ota]`), `tools/prototyp/wgraj_wifi.sh`, `tools/prototyp/sekrety.sh`, README prototypu „Wgrywanie przez Wi-Fi”; [ESP-IDF: OTA, App rollback](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html); rdzeń Arduino-ESP32 [`esp32-hal-misc.c`](https://raw.githubusercontent.com/espressif/arduino-esp32/master/cores/esp32/esp32-hal-misc.c), [`ArduinoOTA.cpp`](https://raw.githubusercontent.com/espressif/arduino-esp32/master/libraries/ArduinoOTA/src/ArduinoOTA.cpp), [`defconfig.common`](https://raw.githubusercontent.com/espressif/esp32-arduino-lib-builder/master/configs/defconfig.common) (gałąź master, sprawdzone 01.11.2026) |

## Kontekst

Panel trzeba aktualizować z dwóch powodów i w dwóch rytmach:

| Co | Jak często | Skąd się bierze | Co nie może zginąć |
|---|---|---|---|
| **firmware** (kod) | rzadko: poprawki błędów, nowe funkcje, wydania 1.x | repozytorium, kompilacja na Macu (spec techniczna 8.7) | historia nocy, odpowiedzi, konfiguracja, raporty i stan z 8.4 (R-NF-19) |
| **zestaw danych reguł** (progi, katalog reguł, teksty; ADR-0023) | częściej: po kalibracji w etapie 2, po konsultacjach klinicznych, przy zamrożeniu WY16 | skrypt w `tools/` z `progi.csv` i spec | to samo; zestaw zmienia tylko trzy pliki zestawu (ADR-0023 pkt 6) |

Jak jest dziś w prototypie:

- firmware kablem USB (115 200 b/s, kilka minut) albo przez Wi-Fi (ArduinoOTA z hasłem, nazwa `cpap-eink.local` przez mDNS), tylko gdy panel stoi na ekranie testu sprzętu, przez 10 min od ostatniego naciśnięcia (`src/main.cpp:1505-1524`, `:1713`); po wybudzeniu zegarem ekran testu wraca do ekranu dnia, więc okno nigdy nie otwiera się samo (`:1705`);
- pamięć plików przez `uploadfs` (kablem albo przez Wi-Fi): **kasuje** historię, czynniki i dziennik ekranów (`tools/prototyp/wgraj_wifi.sh --fs`, `src/main.cpp:1513-1515`), dlatego przed nim jest zrzut `Z`;
- powrotu do poprzedniej wersji praktycznie nie ma: ESP-IDF ma taki mechanizm (nowa wersja startuje jako „do sprawdzenia” i sama musi się oznaczyć jako dobra, inaczej bootloader wraca do poprzedniej), biblioteki Arduino go włączają (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`), ale rdzeń Arduino oznacza nową wersję jako dobrą od razu przy starcie, chyba że program nadpisze `verifyRollbackLater()`; prototyp tego nie robi.

Granice: ADR-0001 i R-NF-07 pozwalają panelowi wysyłać poza dom tylko zapytania o pogodę i czas. R-NF-03 pozwala włączać Wi-Fi tylko na pobieranie danych, czas i serwer raportu albo formularza; okna aktualizacji tam nie ma.

## Decyzja 1: droga firmware

| Wariant | Na czym polega | Skutek i koszt | Rekomendacja |
|---|---|---|---|
| **A. Wi-Fi w sieci lokalnej, tylko w oknie serwisowym, z hasłem i powrotem** | Jak prototyp: okno otwiera się ręcznie na ekranie testu sprzętu, trwa 10 min, nasłuch i mDNS tylko wtedy (T-WYM-40). Hasło okna generowane przy pierwszym wgraniu kablem i trzymane w Pęku kluczy (jak dziś `tools/prototyp/sekrety.sh`); bez hasła okno się nie otwiera (T-WYM-45). Obraz przechodzi sprawdzenie (T-WYM-44), trafia do drugiego slotu aplikacji, a po restarcie nowa wersja robi samosprawdzenie przed pierwszym snem (T-WYM-41) i dopiero wtedy oznacza się jako dobra; migracje plików po oznaczeniu (T-WYM-43). Kabel zostaje drogą ratunkową i jedyną drogą zmiany układu partycji. | Aktualizacja bez kabla i bez ryzyka utraty danych. Zła wersja (nie startuje, nie łączy się z Wi-Fi, nie czyta plików) cofa się sama. Koszt: samosprawdzenie i obsługa powrotu (ok. jeden dzień pracy [szacunek]), test powrotu na egzemplarzu, zmiana spec: R-NF-03 i R-NF-02 dostają okno serwisowe, a ekran testu sprzętu ID w spec. Ryzyko: ktoś w sieci domowej z hasłem może wgrać własny obraz; hasło nie jest podpisem (Decyzja 3). | **rekomendowany** |
| B. Plik firmware na karcie microSD | Obraz i plik ze skrótem na karcie; panel przy starcie sprawdza skrót i wgrywa obraz do drugiego slotu, dalej jak w A. | Działa, gdy Wi-Fi panelu nie działa, i bez okna serwisowego, więc bez zmiany R-NF-03. Ale karta jest w ADR-0022 opcjonalna, a każda aktualizacja to wyjęcie karty, czytnik w Macu i włożenie z powrotem; dochodzi kod czytania obrazu z karty (dzielona magistrala z ekranem, sekcja 3.1). | dobra droga zapasowa po wydaniu 1.0, nie główna |
| C. Tylko kabel USB | Każda aktualizacja jak pierwsze wgranie. | Zero nowego kodu i zero nasłuchu w sieci. Każda aktualizacja to Mac przy łóżku i kilka minut kabla; bez powrotu do poprzedniej wersji, więc zły obraz wymaga drugiego wgrania kablem. Przy częstych poprawkach w okresie testowym WY16 to realny koszt czasu. | odrzucony jako jedyna droga; zostaje jako ratunkowa |
| D. Pobieranie z internetu (własny serwer albo wydania GitHub) | Panel sam sprawdza i pobiera nowe wersje. | **Łamie ADR-0001 i R-NF-07**: panel łączyłby się poza dom w innym celu niż pogoda i czas. Dodatkowo wymaga serwera, podpisu i polityki wydań dla każdego, kto zbuduje panel. | **odrzucony** (granica projektu; zmiana tylko nowym ADR) |
| E. Strona wgrywania w przeglądarce (serwer HTTP panelu z formularzem pliku) | Jak serwer raportu, ale z przyjmowaniem obrazu. | Wygodne bez PlatformIO, ale to drugi serwer przyjmujący dane z sieci, z większą powierzchnią ataku niż ArduinoOTA, i nowy kod do testów. Przydatne dopiero, gdy panel trafi do osób bez PlatformIO (publikacja). | odrzucony na 1.0; do rozważenia przy publikacji |

## Decyzja 2: droga zestawu danych reguł

| Wariant | Na czym polega | Skutek i koszt | Rekomendacja |
|---|---|---|---|
| **A. Zestaw z karty microSD** | Skrypt z `tools/` buduje pakiet: trzy pliki zestawu, każdy z nagłówkiem z wersją formatu, identyfikatorem zestawu i skrótem SHA-256 (specyfikacja techniczna 9.3, ADR-0023 pkt 1 i 4), bez osobnego manifestu, oraz katalog scen budzika ze spisem i skrótem każdej sceny (T-DANE-36). Osoba wydająca panel kopiuje pakiet na kartę. Panel przy starcie albo z ekranu testu sprawdza nagłówki i skróty (T-WYM-64) i wgrywa pakiet, gdy jego identyfikator jest inny niż zestawu we flash, także starszy, więc złą kalibrację da się cofnąć poprzednim pakietem; kopiuje pliki do partycji plików jako całość i zachowuje poprzedni zestaw; przy błędzie zostaje obecny zestaw, a pakiet nie jest kopiowany (T-WYM-67). Historia, czynniki i dziennik bez zmian. | Zgodne z ADR-0022 A (karta jako wymiana) i ADR-0023 pkt 6; nie wymaga sieci ani okna serwisowego. Koszt: kod kopiowania z karty i sprawdzania nagłówków; karta musi być w panelu przy zmianie zestawu. Nowy zestaw działa od tego startu, a nowa wersja progów jest na ekranie testu (T-WYM-39). | **rekomendowany** |
| B. Zestaw przez Wi-Fi w oknie serwisowym | Osobne polecenie przesyłające pliki zestawu (nie obraz partycji). | Bez karty, ale potrzebny własny protokół albo serwer przyjmujący pliki: ArduinoOTA umie tylko obraz firmware albo obraz całej partycji plików, a ten kasuje dane. | do rozważenia po 1.0 |
| C. Zestaw tylko wkompilowany w firmware | Każda zmiana progu to nowy obraz. | Najprościej i zawsze spójnie, ale każda kalibracja to kompilacja i aktualizacja firmware, czyli dokładnie to, czego ADR-0023 chce uniknąć. Zestaw wbudowany zostaje jako zapas przy złym albo brakującym pliku (ADR-0023 pkt 5). | odrzucony jako jedyna droga |
| D. Obraz pamięci plików (`uploadfs`) | Jak prototyp. | Kasuje historię, czynniki i dziennik ekranów: łamie R-NF-19. W architekturze docelowej firmware odrzuca obraz partycji plików przysłany przez Wi-Fi (T-WYM-46). | **odrzucony** |

## Decyzja 3: sprawdzenie obrazu i zestawu (bez wariantów na 1.0, z jednym krokiem odłożonym)

| Co | Sprawdzenie w 1.0 | Przed czym chroni | Czego nie daje |
|---|---|---|---|
| obraz firmware przez Wi-Fi | hasło okna serwisowego; suma MD5 całego przesyłu, którą komputer podaje przy zaproszeniu (`ArduinoOTA.cpp`); poprawność obrazu sprawdzana przez ESP-IDF przy zakończeniu zapisu; rozmiar nie większy niż slot (T-WYM-44) | przypadkowej osobie w sieci, uszkodzeniu przy przesyłaniu, obrazowi na inny układ | pewności, kto zbudował obraz: hasło i MD5 to nie podpis |
| zestaw danych z karty | skrót SHA-256 każdego pliku w jego nagłówku, ten sam identyfikator zestawu w trzech plikach, wersja formatu znana firmware (T-WYM-64); skrót każdej sceny w spisie scen (T-WYM-67) | uszkodzeniu pliku, mieszance plików z dwóch wersji | odróżnienia pliku zbudowanego skryptem od ręcznie poprawionego (ADR-0023 pkt 4: granica ADR-0002 trzyma się procesu przed wgraniem) |
| **odłożone: podpis** | podpis obrazu (ESP-IDF `CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT`, sprawdzanie podpisu bez sprzętowego secure boot) i podpis pakietu zestawu | podmianie obrazu przez kogoś w sieci domowej, kto zna hasło; przy publikacji: obrazom od osób trzecich | wymaga własnej konfiguracji ESP-IDF zamiast bibliotek Arduino (przebudowa bibliotek, dłuższa kompilacja) i decyzji, kto trzyma klucz; dla jednego panelu w domu koszt większy niż zysk, przy publikacji do decyzji |

## Decyzja 4: powrót do poprzedniej wersji (bez wariantów, dobra praktyka)

1. Firmware nadpisuje `verifyRollbackLater()`, żeby rdzeń Arduino nie oznaczał nowej wersji jako dobrej od razu.
2. Pierwsze wybudzenie nowej wersji robi samosprawdzenie **przed pierwszym snem**: partycja plików się montuje, zestaw danych reguł się wczytuje, ekran się rysuje, Wi-Fi się łączy (T-WYM-41). Przed pierwszym snem, bo każde wybudzenie z głębokiego uśpienia to start przez bootloader, a niepotwierdzona wersja przy następnym starcie jest porzucana; do potwierdzenia na egzemplarzu.
3. Wynik dobry: wersja oznaczona jako dobra, potem migracje plików (T-WYM-43). Wynik zły albo restart w trakcie: powrót do poprzedniej wersji, wpis w logu i na ekranie testu (T-WYM-42).
4. Migracje po oznaczeniu, bo powrót do poprzedniej wersji ma zastać pliki w formacie, który ta wersja zna. Gdy nowsza wersja już zmigrowała pliki, a potem użytkownik sam wgra starszą, działa reguła ADR-0022 decyzja 3 pkt 4 (starsza wersja czyta znane pola i nie zapisuje nowszego pliku), a kopia sprzed migracji leży obok do następnej udanej migracji.
5. Bez blokady wgrania starszej wersji (anti-rollback ESP-IDF): przy jednym autorze powrót do starszej wersji jest narzędziem, nie zagrożeniem.

## Konsekwencje

- **Zmiana spec funkcjonalnej przed implementacją** (ID, CHANGELOG, akceptacja autora projektu): R-NF-03 (Wi-Fi także w oknie serwisowym), R-NF-02 (okno trzyma panel bez snu do 10 min od ostatniego naciśnięcia), ekran testu sprzętu z ID w katalogu 6.2 i wierszem w mapie 5.6.2 (dziś wspomniany tylko w R-AW-25). Bez tej zmiany test R-NF-03 („poza tymi czynnościami Wi-Fi wyłączone”) nie przejdzie.
- **Spec techniczna**: T-KOMP-15 realizuje T-WYM-39…T-WYM-46, T-WYM-49 i T-WYM-64…T-WYM-67 (T-WYM-47 i T-WYM-48 wycofane, zob. T-WYM-64, T-WYM-65, T-WYM-67); przebieg w 7.8, drogi wgrywania w 8.8, układ z dwoma slotami w 8.2. T-INT-08 (sekcja 4.2) dostaje opis okna serwisowego i odrzucania obrazu plików.
- **Prototyp**: dzisiejsze ArduinoOTA zostaje podstawą; do dopisania samosprawdzenie z `verifyRollbackLater()`, odrzucanie obrazu partycji plików, kopiowanie zestawu z karty. `uploadfs` znika z instrukcji, gdy jest import i przywracanie z karty (ADR-0022).
- **Zależności**: Decyzja 2 zakłada kartę z ADR-0022 A. Przy ADR-0022 B (karta jako główny nośnik) zestaw i tak leży na karcie, więc Decyzja 2 A działa bez kopiowania do flash, a zmienia się tylko miejsce poprzedniego zestawu.
- **Okres testowy WY16**: wgranie zestawu z inną wersją progów jest widoczne na ekranie testu i w logu (T-WYM-39, T-WYM-49), więc noce sprzed i po zmianie da się rozdzielić.
- **Otwarte**: powrót na egzemplarzu i w przypiętej wersji rdzenia, czas wgrania przez Wi-Fi, sposób uwierzytelnienia w przypiętej wersji ArduinoOTA, podpis przy publikacji.
- **Granica ADR-0001 bez zmian**: żaden wariant przyjęty nie łączy panelu poza dom.

## Adnotacja 01.11.2026 (decyzja 4)

Decyzja 01.11.2026. Pkt 3 i 4 zmienione, reszta decyzji bez zmian:

- **Migracje na nowe ścieżki przed oznaczeniem.** Migracja, która zapisuje wynik pod nową ścieżką i nie rusza pliku źródłowego (np. `/noce.csv` w wersji 0 → `/dane/noce/RRRR.csv` w wersji 1), idzie w samosprawdzeniu, przed oznaczeniem wersji jako dobrej, więc jej błąd daje powrót do poprzedniej wersji, a ta zastaje swoje pliki nietknięte. Migracja w miejscu (ten sam plik) i migracja NVS zostają po oznaczeniu, jak w pkt 4. Skutek: po ręcznym wgraniu starszej wersji już po oznaczeniu starsza wersja czyta stary plik bez nocy dopisanych przez nowszą; te noce wracają z AirBridge jako luka (R-DANE-06).
- **Siatka także po oznaczeniu.** Powrót z pkt 3 działa tylko w stanie „do sprawdzenia”. Dochodzą: strażnik zadania, który zawieszenie kończy restartem; licznik startów po błędzie; po kilku startach z rzędu tryb awaryjny z ekranem U-06, bez sieci poza oknem serwisowym i bez zapisu danych (spec funkcjonalna R-AW-39; spec techniczna 7.8, T-WYM-81…T-WYM-83).
- **Kolejność.** Powrót, samosprawdzenie, strażnik i licznik wchodzą w pierwszym kroku wdrożenia (spec techniczna 13.1), bo kolejne wersje są wgrywane przez Wi-Fi.

## Adnotacja 01.11.2026 (decyzja 2)

Decyzja 01.11.2026. **Zestaw reguł przychodzi także z nowym firmware przez Wi-Fi; karta zostaje drogą dla pakietów spoza firmware.** Bez tego po pierwszym starcie zestaw we flash wygrywał zawsze, więc poprawiony próg albo tekst w nowej wersji firmware nie zmieniał niczego na ekranie, a zmiana progu w okresie kalibracji wymagała sterownika karty, którego jeszcze nie ma.

- Nagłówek każdego pliku zestawu ma pola `pochodzenie=wbudowany|karta` i `firmware={od}..{do}` (zakres wersji firmware, dla których zestaw zbudowano).
- Po aktualizacji firmware (i po powrocie do starszej wersji) zestaw wbudowany z innym identyfikatorem zastępuje we flash zestaw, który pochodzi z wbudowanego, bez linii uwagi.
- Zestaw z karty zostaje, dopóki jego zakres obejmuje bieżący firmware; poza zakresem panel używa zestawu wbudowanego z linią R-AW-30.
- Panel sprawdza też, czy zestaw ma teksty, których potrzebuje sam firmware (ekrany systemowe, linie awarii).
- Wariant B tej decyzji (osobny protokół przesyłania plików zestawu przez Wi-Fi) dalej odłożony: zestaw idzie w obrazie firmware, przez to samo okno serwisowe.

Treść wymagań: spec funkcjonalna R-AW-33, R-AW-30, R-NF-30; spec techniczna 7.8, 9.3, T-WYM-64, T-WYM-65, T-WYM-67, T-WYM-85.

**Zmiana 05.11.2026:** wariant D (pobieranie z wydań GitHub) przyjęty w [ADR-0032](0032-aktualizacja-firmware-z-github.md); wariant A i kabel zostają jako drogi zapasowe.
