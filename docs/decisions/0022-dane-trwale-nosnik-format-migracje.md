# ADR-0022: Dane trwałe: nośnik, format plików i migracje

| | |
|---|---|
| Data | 01.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 01.11.2026): wariant A; wymaga zmiany spec funkcjonalnej i adnotacji ADR-0011 pkt 4 |
| Źródło | ADR-0020; spec 2.4, 2.5, 8.1, 8.4, R-DANE-05, R-NF-06, R-NF-11, R-NF-19, R-AW-05, R-AW-05b, R-AW-16, R-AW-17, R-AW-23, ADR-0017; specyfikacja techniczna 3.1 (dane sprzętu z Seeed i Espressif); prototyp: `src/noc.h` (format `/noce.csv` i migracja w parserze), `platformio.ini` (`default_8MB.csv`, LittleFS) |

## Kontekst

Panel trzyma kilka rodzajów danych: historię nocy, odpowiedzi i czynniki, odczyty sypialni, liczniki ekranów, konfigurację, raporty PDF dla lekarza i stan między wybudzeniami (pełna lista w spec 8.4). Trzeba rozstrzygnąć trzy rzeczy: **gdzie** te dane leżą, **w jakim formacie** i **jak przechodzą** przez zmianę wersji firmware (R-NF-19).

Fakty, które to rozstrzygają (specyfikacja techniczna 3.1):

| Fakt | Źródło | Co z niego wynika |
|---|---|---|
| panel ma 32 MB pamięci flash; prototyp używa 8 MB, z czego 1,5 MB na pliki | schemat Seeed v1.2, `platformio.ini` | na dane w samym panelu jest dużo miejsca po zmianie układu partycji |
| rekord nocy prototypu ma 114 B (20 pól, zmierzone kodem `zapiszNoc` na pełnym rekordzie, 01.11.2026) | `src/noc.cpp` | sama historia nocy: 400 nocy ok. 46 KB, 2 000 nocy ok. 230 KB. Pozostałe dane (raporty PDF, wszystkie, R-LEK-23; log z 3 miesięcy, R-NF-06; serie czujników, R-DANE-09; czynniki; sceny budzika) nie są policzone: A-110 domyka sekcja 9 |
| karta microSD dzieli magistralę z ekranem; włożona karta przy źle ustawionych pinach potrafi zablokować ekran; karta ma własne zasilanie (`SD_EN`) | wiki Seeed (Arduino, peripherals) | piny karty trzeba ustawić przy każdym starcie, niezależnie od wariantu, bo karta zwykle jest włożona. Zapis na kartę to dodatkowo: włączenie zasilania, montowanie systemu plików, rozłączny dostęp z ekranem; czas i energia `[do zmierzenia]` |
| PSRAM gubi zawartość w głębokim uśpieniu; przeżywa pamięć RTC (dla aplikacji mniej niż 8 KB), NVS i pliki we flash | Espressif | stan z R-AW-05b („noce oceniane z pamięci przy awarii karty”) nie może leżeć w PSRAM, ale może w RTC (dziesiątki nocy w postaci binarnej) albo we flash |

Spec funkcjonalna zakłada kartę jako główny nośnik: rekordy, raporty, log i konfiguracja na karcie (R-NF-06), odtworzenie po zaniku zasilania z karty (R-NF-11), cały zestaw zachowań przy awarii karty (R-AW-05, R-AW-05b, R-AW-16, R-AW-17, R-AW-23). Wymagań, których wiersz mówi o karcie microSD albo o zapisie i odczycie z karty, jest 44 (lista z komendy w „Konsekwencjach”, do przejrzenia: część zmieni tylko słowo). Do tego tekst 5.3.3 („Awaria zapisu na kartę”), wiersz o pamięci stanu w 2.5, przypadki 12, 34 i 40 w 7.8 oraz przyjęty ADR-0011 pkt 4 (zapasowa droga konfiguracji: plik na karcie). Prototyp robi odwrotnie: wszystko w pamięci flash panelu (LittleFS), bez karty; import historii przez wgranie obrazu pamięci, który kasuje całą pamięć plików.

## Decyzja 1: nośnik

| Wariant | Na czym polega | Skutek i koszt | Rekomendacja |
|---|---|---|---|
| **A. Pamięć flash panelu jako główna, karta jako kopia i wymiana** | Historia, odpowiedzi, liczniki, konfiguracja, raporty i sceny budzika w pamięci flash panelu (osobna partycja na pliki, kilkanaście MB z 32 MB). Karta: plik importu historii (ADR-0017), plik konfiguracji jako droga zapasowa (ADR-0011 pkt 4, zmieniony: panel wczytuje go z karty do flash), automatyczna kopia danych raz na dobę (po zapisie nocy), eksport. Karta jest opcjonalna. | Rdzeń działa bez karty; karta pracuje raz na dobę. Przy awarii karty panel liczy dalej także normę i nowy poziom (dziś R-AW-05b je wstrzymuje), a zostaje tylko „brak kopii”. Koszt: **zmiana spec funkcjonalnej** w ok. 35–45 wymaganiach (lista w „Konsekwencjach”), w 5.3.3, 2.5, 8.1, 8.4 i przypadkach 7.8 nr 12, 34, 40, oraz **zmiana przyjętego ADR-0011 pkt 4**; zmiana układu partycji (jednorazowo: zrzut danych, wgranie z nowym układem, przywrócenie). Pamięć flash ma ograniczoną liczbę zapisów; przy kilkudziesięciu zapisach na dobę z wyrównywaniem zużycia margines jest duży, ale trzeba go policzyć z danych W25Q256 w sekcji 9. | **rekomendowany** |
| B. Karta jako główna (jak spec dziś), magazyn awaryjny we flash | Rekordy, raporty, konfiguracja i sceny na karcie. Ostatnie noce i stan z 8.4, który nie może zginąć przy awarii karty, w magazynie awaryjnym we flash albo NVS (R-AW-05b). | Bez zmiany spec i ADR-0011. Każdy zapis to praca karty (zasilanie, montowanie, dzielona magistrala z ekranem). Przy awarii karty panel ocenia noce z magazynu awaryjnego, ale wstrzymuje normę i nowy poziom i pokazuje linię R-AW-05; trzeba zbudować i przetestować dwie ścieżki zapisu (karta i awaryjna) oraz synchronizację po powrocie karty. Kod pamięci prototypu (LittleFS) zostaje dla magazynu awaryjnego, główny zapis trzeba napisać od nowa na kartę. | dobry drugi wybór: mniej zmian w dokumentach, więcej kodu i testów |
| C. Zapis do obu przy każdej zmianie | Pamięć flash i karta zawsze równe. | Najwięcej kodu i energii; dwie kopie mogą się rozjechać i trzeba ustalić, która wygrywa. Zysk wobec A tylko taki, że kopia na karcie jest zawsze świeża, a nie sprzed doby. | odrzucony |

**Dlaczego A, mimo większej zmiany spec.** Zmiana dokumentów jest jednorazowa i odwracalna, a druga ścieżka zapisu z wariantu B to kod, który trzeba utrzymywać i testować do końca życia panelu. Karta jest najczęściej psującym się elementem zestawu, a w A jej awaria nie zmienia tego, co panel mówi rano. Jeśli spec ma pozostać bez zmian w trakcie etapu 2, B jest bezpieczny i nie blokuje niczego poza kosztem kodu.

## Decyzja 2: format plików

| Wariant | Skutek i koszt | Rekomendacja |
|---|---|---|
| **CSV z wierszem nagłówka, który podaje nazwę i wersję formatu** (np. `#cpap-eink noce 3`), separator `;`, puste pole = brak danych, nowe pola tylko na końcu wiersza | czytelny w każdym edytorze i w narzędziach Pythona projektu (`tools/kalibracja`, `tools/prototyp/historia.py`), mały. Prototyp już zapisuje CSV: `/noce.csv` przepisuje w całości przez plik tymczasowy i zmianę nazwy (`src/main.cpp`, `zapiszPlik`), a dopisuje wiersze tylko do dzienników (`/bateria.csv`, `/humor.csv`); brakuje jawnej wersji | **rekomendowany** |
| JSON w liniach (JSON Lines) | samoopisowy, ale większy (szacunek: 2–3 razy, nazwy pól w każdym wierszu), wolniejszy w czytaniu na panelu i nieczytelny w arkuszu | odrzucony |
| struktury binarne | najmniejszy i najszybszy, ale nieczytelny bez narzędzia, a każda zmiana układu to ryzyko cichego błędu | odrzucony |

## Decyzja 3: migracje i bezpieczny zapis (bez wariantów, dobra praktyka)

1. **Każdy plik i każda przestrzeń NVS ma wersję** (wiersz nagłówka w pliku, klucz `wersja` w NVS, pole „magia i wersja” w strukturach RTC). Prototyp ma dziś wersję tylko pośrednio, w liczbie pól (`src/noc.h`: układy 8, 9, 12, 13, 14, 20 pól); to staje się jawnym kontraktem.
2. **Migracja przy starcie, krok po kroku** (wersja 1 → 2 → 3, każdy krok osobną funkcją z testem na komputerze). Przed migracją kopia pliku obok (`noce.csv.v2`), po migracji sprawdzenie liczby rekordów, dopiero potem zamiana. Kopie starszych wersji usuwa dopiero następna udana migracja.
3. **Zapis atomowy**: nowa treść do pliku tymczasowego, potem zmiana nazwy. Zanik zasilania w trakcie zapisu zostawia starą albo nową wersję, nigdy połowę (R-NF-11).
4. **Starszy firmware, nowszy plik**: firmware, który widzi wersję wyższą niż zna, czyta tylko pola, które zna, i nie zapisuje tego pliku; na ekranie testu pokazuje „dane z nowszej wersji”. Powrót do starszej wersji firmware nie niszczy danych.
5. **Rozmiar i pojemność** każdego pliku: w sekcji 9, tabela T-DANE z kolumnami format, wersja, migracja, rozmiar rekordu, przyrost na dobę.

## Konsekwencje

- Przy wariancie A: zmiana spec funkcjonalnej przed implementacją (ID, CHANGELOG, akceptacja autora projektu) i adnotacja ADR-0011 pkt 4. Wymagania do przejrzenia (wiersz wymagania mówi o karcie microSD albo o zapisie i odczycie z karty; komenda: `grep -E '^\| R-' docs/spec/specyfikacja-funkcjonalna.md | grep -iE 'microSD|na karci|na kartę|z karty (microSD|pamięci)|kart[aęy] pamięci|karta (jest )?nieczyteln|kartę microSD|plik konfiguracji'`, 01.11.2026): R-AW-05, R-AW-05b, R-AW-14, R-AW-16, R-AW-17, R-AW-23, R-CZY-10, R-DANE-04, R-DANE-05, R-DANE-06, R-DANE-26, R-EKR-10, R-EKR-32, R-EKR-33, R-EKR-46, R-EKR-54, R-HUM-01, R-HUM-11, R-HUM-13, R-KON-40, R-KONF-17, R-KONF-19, R-KONF-20, R-KONF-23, R-KONF-29, R-LEK-01, R-LEK-05, R-LEK-09, R-LEK-20, R-LEK-22, R-LEK-23, R-LEK-24, R-MAS-01, R-MAS-06, R-NF-06, R-NF-08, R-NF-11, R-NF-12, R-NF-22, R-PIE-09, R-PRZ-11, R-PRZ-13, R-SKI-21, R-SKI-23. Zmiana treści dotyczy głównie wymagań R-AW, R-NF i R-KONF; w pozostałych zmienia się tylko miejsce zapisu.
- Sceny budzika: przy A w partycji plików we flash (dziś wkompilowane w firmware, `include/obrazy.h`); przy B na karcie (R-HUM-11, R-KON-40).
- Zachowanie widoczne na ekranie zmienia się tylko przy awarii karty: zamiast linii `Błąd karty pamięci. Wyniki mogą być niepełne.` panel ocenia noce normalnie, a o braku kopii mówi łagodniej (treść i grupa linii do ustalenia w zmianie spec).
- Nowy układ partycji 32 MB (sekcja 8): aplikacja ×2 (wgrywanie przez Wi-Fi), NVS, partycja plików; liczby w sekcji 8. Przejście z prototypu: jednorazowo zrzut plików komendą serwisową `Z`, wgranie z nowym układem, przywrócenie z karty.
- Import historii (ADR-0017) przechodzi z wgrywania obrazu pamięci na plik na karcie, więc przestaje kasować dane panelu (dziś `uploadfs` kasuje historię, czynniki i dziennik ekranów).
- Karta jako nośnik danych zdrowotnych bez szyfrowania (spec 8.1) zostaje tematem także przy wariancie A, bo kopia leży na karcie; szyfrowanie kopii: sekcja 9 (bezpieczeństwo).
- Pytanie o pamięć stanu dostaje odpowiedź techniczną: PSRAM nie przeżywa uśpienia; stan między wybudzeniami leży w RTC, NVS albo we flash. Przy A stan z R-AW-05b przestaje być potrzebny w tej postaci, przy B leży w magazynie awaryjnym we flash.

## Zmiana 04.11.2026 (bez zmiany decyzji)

Decyzje 1–3 bez zmian. W wersji beta doszły: rekord `noce` wersja 2 i 3 (M-18) z migracją po kolei, `sypialnia` wersja 2 (seria CO2), raporty PDF w `/raporty/`, plik `/konfiguracja.txt` zapisywany atomowo przy „wymienione dziś” i „nowy model”. Nowe klucze NVS przestrzeni `cpap` mają wartość domyślną „brak” i nie wymagają migracji: `sn_noc`, `b9_wiecz`, `buz_a`, `cisza_od`, `cisza_do`, `dzwiek`, `wyj_cof`, `k1_odk`, `b16_sk31`, `sp_kur`, `sp_model`, `sp_cof`. Spec techniczna 9.

## Zmiana 2026-11-09 (ADR-0035; bez zmiany decyzji)

Rekord `noce` wersja 4: te same pola co wersja 3, nowa dozwolona wartość `S` w polu `zrodlo` (noc z pliku STR.edf aparatu). Wersja rośnie bez nowych pól, bo firmware znający wersję 3 odrzuca wiersz z `S`; dzięki pkt 4 tej decyzji czyta plik wersji 4 tylko do odczytu i niczego nie gubi ani nie podwaja. Migracja 3 → 4 krokiem ogólnym `Magazyn::migrujWMiejscu()` (kopia `.v3`, sam nowy nagłówek). Nowy klucz NVS `cpap`/`str_etag` z wartością domyślną „brak”, bez migracji. Źródło: `src/magazyn.cpp` `rejestrFormatow()`, `src/noc.h` `WERSJA_NOCY`, CHANGELOG 09.11.2026.

## Zmiana 2026-11-09 (ADR-0036; bez zmiany decyzji)

Noc z plików sesji (ADR-0036) zapisuje się w tym samym rekordzie `noce` wersji 4 co noc z raportu JSON (źródło `A`), bez nowych pól i bez migracji. Nowe klucze `/konfiguracja.txt` `noc_zrodlo` i `noc_json` mają wartość domyślną „brak” (raport JSON); NVS bez nowych kluczy.

## Zmiana 2026-11-09 (ADR-0036 „Zmiana 2026-11-09”, tryb porównawczy; bez zmiany decyzji)

Klucz `noc_zrodlo` ma nową wartość `porownanie`, która jest domyślna (bez wiersza i przy złej wartości); wcześniejsze zdanie o wartości domyślnej „brak” (raport JSON) dotyczy dziś tylko zapisu nocy, który dalej idzie z raportu JSON. Nowy format `porownanie` w wersji 1: `/dane/porownanie.csv`, przepisanie atomowe z `#koniec {n}`, pola `data;werdykt;rozjazd;pominiete;przyczyna`, bez wersji 0 i bez migracji, 30 ostatnich dób (T-DANE-40). Plik z nowszej wersji jest tylko do odczytu (pkt 4), a panel wtedy w ogóle nie porównuje (bez pobrań plików doby). Plik nie należy do plików przywracania (`plikiZrzutu()`), ale jest w zrzucie `Z`. NVS bez nowych kluczy. Źródło: `src/magazyn.cpp` `rejestrFormatow()`, `src/porownanie.h`, CHANGELOG 09.11.2026.

## Zmiana 2026-11-11 (ADR-0036 „Zmiana 2026-11-11”, domyślnie `plik`; bez zmiany decyzji)

Wartość domyślna klucza `noc_zrodlo` (bez wiersza i przy złej wartości) to od 11.11.2026 `plik`: noc z plików sesji zapisuje się w tym samym rekordzie `noce` wersji 4 (źródło `A`), bez nowych pól, bez migracji i bez przeliczania nocy zapisanych wcześniej z raportu JSON. `/dane/porownanie.csv` zostaje bez zmian formatu; przy `plik` nie przybywa wpisów. NVS bez nowych kluczy.

## Zmiana 2026-11-09 (konfiguracja w zrzucie; bez zmiany decyzji)

Zrzut `Z` obejmuje też `/konfiguracja.txt` (ustawienia i dane wpisane na panelu: daty wymiany, model maski, formularz), z rozmiarem i skrótem SHA-256 przed sekcją pliku; przywracanie z katalogu zrzutu wysyła ją komendą `W konfiguracja.txt` przed `K`. Plik nie należy do plików przywracania (`plikiZrzutu()`), nie ma wersji formatu ani migracji, NVS bez zmian. Źródło: spec techniczna T-WYM-92, 8.6, `src/dyrygent.cpp` `Przebieg::zrzutKonfiguracji()`, CHANGELOG 09.11.2026.

## Zmiana 2026-11-11 (M-14 i M-15 w rekordzie nocy; bez zmiany decyzji)

Rekord `noce` wersja 5 = pola wersji 4 i na końcu `m14;m15` (M-14 objętość oddechowa w mL, M-15 wentylacja minutowa w L/min, mediany kanałów `TidVol` i `MinVent`; decyzja właściciela projektu 11.11.2026, M-14 dołożone w tej samej zmianie formatu). Zgodnie z pkt o wersjach: nowe pola tylko na końcu, migracja 4 → 5 krokiem ogólnym `migrujWMiejscu()` (puste pola na końcu, kopia `.v4` przed zmianą; pliki wersji 1–3 krok po kroku), firmware znający wersję 4 czyta plik wersji 5 tylko do odczytu (pierwsze 42 pola). Noce z karty, ze starszych plików i z raportu bez tych kanałów mają pola puste (brak danych, nigdy 0).

