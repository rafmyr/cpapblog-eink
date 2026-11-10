# ADR-0036: Noc liczona na panelu z plików sesji EDF (etap 2 przejścia na EDF)

| | |
|---|---|
| Data | 09.11.2026 |
| Status | przyjęta 09.11.2026 (decyzja właściciela projektu: wariant B) |
| Źródło | ADR-0035; pomiar 09.11.2026 na AirBridge egzemplarza referencyjnego (v2.3.1); `tools/kalibracja/noce.py` (`noc_z_katalogu`, `metryki`); WY9 |

## Kontekst

Etap 1 (ADR-0035) daje historię ze STR.edf, ale bez ciśnienia wdechowego i PS: STR ma tylko ciśnienie docelowe aparatu, a pomiar z 09.11.2026 pokazał, że różni się od ciśnienia z plików EDF.

| 10 nocy egzemplarza referencyjnego, ASVAuto | STR − EDF (raport AirBridge, `source = "edf"`) |
|---|---|
| IPAP mediana | +0,12 do +0,26 cmH2O |
| IPAP P95 | −0,8 do −1,2 cmH2O (za każdym razem) |
| EPAP mediana / P95 | 0 do −0,12 / −0,02 do −0,18 cmH2O |

Mieszanie STR z EDF przesunęłoby normę IPAP P95 o ok. 1 cmH2O (R-NOR-08 zostaje). Właściciel projektu wybrał 09.11.2026 wariant B: prawdziwe IPAP, EPAP i PS z próbek, liczone na panelu, zamiast ciśnienia ze STR (warianty A „pokazać ze STR jako urządzenie” i C „STR w normie z poprawką” odrzucone).

Przy okazji: raport JSON nie daje PS (tylko mediany `Press` i `EprPress`), a panel pobiera osobno przebiegi. Pliki sesji mają wszystko.

Pomiar 09.11.2026, jedna noc (ok. 8 h), `/api/storage/download` z komputera w tej samej sieci:

| Plik | Rozmiar | Czas | Zawartość |
|---|---|---|---|
| PLD | 363 KB | 0,8 s | próbki co 2 s: `Press`, `EprPress`, `Leak`, `RespRate`, `TidVol`, `MinVent`, `FlowLim`, `Snore`, … |
| SAD | 122 KB | 0,4 s | SpO2 i tętno co 1 s (gdy jest oksymetr) |
| EVE | 0,8 KB | 0,2 s | zdarzenia (bezdechy, spłycenia, RERA) |
| CSL | 0,8 KB | 0,1 s | oddychanie okresowe (CSR) |
| BRP | 3,0 MB | – | przepływ 25 Hz, **nie pobieramy** (GI: osobna decyzja) |

## Decyzja (propozycja do przyjęcia)

1. **Źródło nocy przy AirBridge od v2.3.1:** `GET /api/sessions?since=…` (od dnia ostatniej pełnej nocy w historii), sesje doby terapii (`day`). Doba jest gotowa, gdy wszystkie jej sesje mają `closed: true`; wtedy panel pobiera `PLD`, `SAD`, `EVE`, `CSL` każdej sesji (bez `BRP`) i liczy noc sam. Sesja niezamknięta = doba „częściowa” jak dziś (R-DANE-15).
2. **Metody liczenia jak `tools/kalibracja/noce.py`** (czytnik nocy z etapu 2 walidacji, ten sam, który liczy noce z karty i jest porównywany z AirBridge w WY9): M-01 z sesji, AHI i M-03 z EVE, M-09 z CSL, mediany i P95 kanałów z próbek PLD, M-08 z próbek `Leak`, **PS = `Press` − `EprPress` próbka po próbce (M-28, M-28p)**, M-17 = sesje − 1, M-18 ze startu pierwszej sesji, SpO2 z SAD. Kod C++ ma testy zgodności z `noce.py` na tych samych plikach syntetycznych.
3. **Noc z plików = noc z AirBridge** (`zrodlo = 'A'`, pełna): te same dane, liczone na panelu. Pierwszeństwo bez zmian (ADR-0017, ADR-0035).
4. **Ścieżka JSON zostaje jako zapasowa** dla AirBridge < 2.3.1 i gdy `/api/sessions` nie odpowiada. Wycofanie jej to osobna decyzja po okresie pracy etapu 2.
5. **Bramka przed przełączeniem:** narzędzie na komputerze porównuje noc liczoną kodem panelu z raportem AirBridge dla wszystkich nocy dostępnych na AirBridge egzemplarza referencyjnego (i kolejnych paneli), w tolerancjach WY9. Rozjazd poza tolerancją = metryka zostaje z raportu JSON do wyjaśnienia.
6. Przebiegi do wykresów i M-08 z tych samych próbek PLD (bez osobnych żądań o przebiegi), jeśli to nie zwiększa pamięci ponad budżet panelu.

## Konsekwencje

- Każda nowa noc ma prawdziwe IPAP, EPAP i **PS na żywo** (bez zmian w AirBridge).
- Koszt na panelu: ok. 0,5 MB i kilka sekund Wi-Fi na noc (na ESP32 wolniej niż z komputera, do pomiaru), przetwarzanie strumieniowe albo w PSRAM.
- Noce sprzed instalacji AirBridge dalej bez ciśnienia ze STR (egzemplarz referencyjny ma je z importu karty, ADR-0017); nowi użytkownicy budują normę ciśnienia od pierwszej nocy z AirBridge.
- Dwie ścieżki danych (pliki i JSON) do czasu wycofania JSON: więcej testów.
- Szczegóły z implementacji (09.11.2026; R-DANE-42…R-DANE-45, Załącznik A pkt 10):
  - **Kod:** `src/noc_pliki.cpp` (`LicznikNocy`, `nocZWyniku()`, `m17m18()`, `scalZJson()`, lista sesji, ścieżki), adnotacje EDF+ w `src/edf.cpp`, w dyrygencie `Przebieg::dobaZPlikow()` i `Przebieg::strDoNocy()`; noc z plików wchodzi w to samo miejsce co raport JSON (`dobaZAirBridge()`), więc plan pobrań, próby dób, stany nocy i pierwszeństwo są te same.
  - **Pkt 1:** lista sesji per doba (`since` = doba, strony po 8 sesji, najwyżej 8 stron), nie jedna lista od ostatniej pełnej nocy; które doby pobrać, dalej mówi lista `action=days` i `planPobrania()` (najnowsze najpierw, ostatnia noc w odczycie, zaległe najwyżej 4 po rysowaniu w 90 s). Panel pobiera `PLD`, `EVE`, `CSL`; **`SAD` nie** (rekord nocy nie ma pól saturacji, R-SKI-42; `POBIERAJ_SAD` w `src/noc_pliki.h`, do decyzji). Granice sesji z nagłówków EDF, bez `start`/`end` z listy.
  - **Pkt 2:** zgodność z `noce.py` sprawdzona na 20 dobach syntetycznych pole po polu (`test/testy_noc_pliki.cpp`, wzorzec `tools/kalibracja/noc_wzorzec.py`). M-17 i M-18 liczone według R-DANE-27 (sesje od 18:00, wyjątek wczesnego snu, start najdłuższej sesji) jak `metryki()` w `noce.py`, nie uproszczeniem „sesje − 1, start pierwszej sesji” z pkt 2 tej decyzji (to było fail-path ścieżki JSON bez `sessions[]`); w zwykłej nocy z jedną sesją wynik ten sam. Ustawienia nocy z doby STR.edf (`ustawieniaStr()`), więc w wybudzeniu z nocą z plików STR jest pobierany raz (ok. 134 KB), także przy niezmienionym etagu; historia ze STR (ADR-0035) używa tego samego pliku.
  - **Pkt 4:** każdy problem z plikami (404, 503, lista nieczytelna, doba bez sesji, sesja niezamknięta albo `.part`, plik zniknął, ucięty, ponad 2 MiB, STR niepobrany, noc częściowa z plików, za mało czasu) = ta doba z raportu JSON w tym samym wybudzeniu, bez E-01 i E-03 z powodu plików; noc „częściowa” i noc w budowie zostają więc dokładnie jak dziś.
  - **Pkt 5:** bramka jako klucz `/konfiguracja.txt` `noc_zrodlo=plik|json`, **domyślnie `json`**: zgodność sprawdzona tylko na danych syntetycznych, a porównanie na prawdziwych nocach robi narzędzie `tools/kalibracja/noc_panelu.sh` (ten sam kod co panel, tabela w tolerancjach WY9). Metryki z rozjazdem: klucz `noc_json=` (lista ID), wtedy panel pobiera raport JSON i pliki, a wymienione metryki bierze z raportu (`scalZJson()`, ważność od nowa). Przełączenie domyślne do decyzji właściciela projektu.
  - **Pkt 6:** panel nie ma osobnych żądań o przebiegi nocy ani artefakty (M-08 przychodził z `leak_high_seconds` raportu), więc nie ma czego zastępować; M-08 z plików liczony z próbek `Leak` (> 24 L/min). Wykresów przebiegu w nocy panel dziś nie rysuje (trendy są z rekordów nocy).
  - **Pamięć i czas:** jeden plik naraz w PSRAM (limit 2 MiB, PLD ok. 44 KB na godzinę) plus STR (do 1 MiB, ok. 134 KB); liczniki nocy kilkadziesiąt KB, niezależnie od długości nocy. Z komputera ok. 1,2 s na noc 8 h plus STR 1,5 s raz; na ESP32 do pomiaru, szacunek 2–5 s. Przed każdym żądaniem co najmniej 15 s budżetu, inaczej raport JSON.
  - **Odkryte przy okazji i poprawione po przeglądzie (09.11.2026):** ścieżka JSON panelu przeliczała `sessions[].start_ms` raportu (czas cywilny aparatu) przez strefę panelu, więc M-18 było o 1–2 h za późno (błąd poprawiony; noce zapisane przed poprawką: do decyzji); teraz `nocZRaportu()` liczy M-17 i M-18 według R-DANE-27 tą samą funkcją co noc z plików (`m17m18Sesji()`). STR bez doby liczonej nocy = raport JSON (pkt 4, wcześniej ustawienia puste); plik z bajtami za rekordami i czas rekordu ≤ 0 = plik niepełny albo błędny (także w `tools/kalibracja/edf.py`); lista `noc_json` z nieznanym ID = cała noc z raportu JSON; etag STR zapisywany z listy sesji odczytanej przed pobraniem pliku.

## Zmiana 2026-11-09: tryb porównawczy domyślny (pkt 5)

- **Nowa wartość `noc_zrodlo=porownanie`, domyślna** (także bez wiersza i przy złej wartości klucza; R-DANE-60). Noc do historii zawsze z raportu JSON, jak przy `json`, a wybudzenie do ekranu poranka, zapis nocy i raporty zaległych dób są identyczne jak przy `json` (po przeglądzie). Na samym końcu wybudzenia (po rysowaniu i czuwaniu, przed snem) panel liczy najwyżej jedną dobę `[PRÓG]` techniczny (najnowszą czekającą) także z plików sesji (ta sama ścieżka co przy `plik`, `Przebieg::dobaZPlikow()`) i porównuje obie noce w tolerancjach WY9 funkcją `porownanie::porownaj()` (`src/porownanie.cpp`), tą samą, której używa narzędzie `tools/kalibracja/noc_panelu.sh` (jedna logika porównania). Bramka z pkt 5 działa więc na panelu na każdej nowej nocy, nie tylko jednorazowo narzędziem na komputerze.
- **Wynik doby:** `zgodna`, `rozjazd` (lista metryk z wartościami z plików i z raportu; także metryka, którą ma tylko raport) albo `brak` z przyczyną (AirBridge < 2.3.1, błąd listy sesji, brak plików, sesja niezamknięta, brak STR.edf, STR bez doby, plik niepobrany albo nieczytelny, noc z plików częściowa, brak czasu). OA/h i CA/h „nieznane” w plikach przy 0 w raporcie: `pominięte: aparat nie rozróżnia` (aparaty ResMed ASV zapisują w EVE tylko `Apnea`, ADR-0037); PS: `tylko pliki`. Oba poza werdyktem, zapisane.
- **Zapis:** `/dane/porownanie.csv` (format `porownanie` wersji 1, ADR-0022; 30 ostatnich dób; T-DANE-40). Plik nie wychodzi z panelu (ADR-0001) poza zrzutem plików przez port serwisowy (komenda `Z`, `tools/prototyp/zrzut_plikow.py`, który zapisuje każdy plik rejestru formatów); nie należy do plików przywracania.
- **U-05:** wiersz `Noce z plików` pod kolumnami: `zgodne {n} z {n}` z ostatnich najwyżej 7 dób z porównaniem, `rozjazd {k} z {n} ({metryki})`, `brak porównania: {przyczyna}`; przy 7 z 7 dopisek `można przełączyć na pliki`. **Panel nie przełącza się sam:** przełączenie to `noc_zrodlo=plik` w konfiguracji, decyzja właściciela panelu.
- **Koszt:** pliki doby najwyżej raz (doba z wynikiem `zgodna` albo `rozjazd` nie jest pobierana drugi raz); na dobę lista sesji, STR.edf raz w wybudzeniu, `PLD`, `EVE`, `CSL` (ok. 0,5 MB na noc 8 h, na ESP32 `[szacunek]` 2–5 s) i ponowne połączenie Wi-Fi po rysowaniu. Porównanie korzysta tylko z budżetu żądań, który został po raportach i zaległych dobach (najmniej 45 s na start `[stała techniczna]`, 15 s przed każdym żądaniem), inaczej `brak czasu` i ponowienie w kolejnym wybudzeniu z odczytem (doby z przyczyną `czeka`, `brak czasu`, `sesja niezamknięta` z ostatnich 7 dób; raport JSON wtedy jeszcze raz); noc z JSON zapisana zawsze wcześniej i normalnie, bez E-01 i E-03 z powodu plików.
- **Bez zmian:** `json` (bez pobierania plików), `plik` i `noc_json` jak w etapie 2; ekrany pacjenta, reguły i linie uwagi nie czytają wyniku porównania; porównanie nigdy nie mówi pacjentowi nic o nocy (ADR-0029 pkt 7).
