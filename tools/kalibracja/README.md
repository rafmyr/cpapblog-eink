# Kalibracja: tabela nocy z karty SD aparatu

Skrypt czyta kopię karty SD aparatu ResMed (np. AirCurve 10 CS PaceWave) i robi z niej tabelę nocy: jeden wiersz na noc, z polami raportu nocy AirBridge (Załącznik A specyfikacji) i metrykami M-01…M-21 (rozdz. 4.3 i 4.11), a do tego M-06p (EPAP P95), M-28 i M-28p (wsparcie PS, mediana i P95) dla ekranów w trybie ASV. To wejście do kalibracji progów (WY3, WY16). Kolumna `tryb_obslugiwany` znaczy „noc w trybie strojenia” (`WY3.tryb_strojenia` w `progi.csv`, ASVAuto); od ADR-0030 wydanie ocenia też CPAP, AutoSet i ASV, ale bez nocy strojenia. Progi są w [`docs/spec/progi.csv`](../../docs/spec/progi.csv).

## Pliki

| Plik | Co robi |
|---|---|
| [`noce.py`](noce.py) | funkcja `noce` i wiersz poleceń, który zapisuje tabelę nocy do CSV |
| [`edf.py`](edf.py) | czytnik plików EDF i EDF+ (adnotacje) |
| [`syntetyczna.py`](syntetyczna.py) | generator syntetycznej karty ze znanymi nocami (do testów); `--str-panelu PLIK` tworzy sam STR.edf do testów panelu (`dni_str_panelu()`, ADR-0035); `--ps KATALOG` kartę PS (`utworz_ps()`); `--noce-panelu KATALOG` kartę do testu nocy z plików na panelu (`utworz_noce_panelu()`, ADR-0036) |
| [`str_wzorzec.py`](str_wzorzec.py) | wzorzec odczytu STR.edf klasą `Str` z `noce.py` (wartości dób, ustawienia, statystyki kanałów) dla testu zgodności panelu `firmware/prototyp/test/testy_str_edf.cpp` (uruchamia go `tools/prototyp/testy.sh`); tylko na plikach syntetycznych |
| [`noc_wzorzec.py`](noc_wzorzec.py) | wzorzec nocy z plików sesji (`noc_z_katalogu`, `metryki`) dla testu zgodności panelu `firmware/prototyp/test/testy_noc_pliki.cpp` (ADR-0036; uruchamia go `tools/prototyp/testy.sh`); tylko na kartach syntetycznych |
| [`noc_panelu.sh`](noc_panelu.sh), [`noc_panelu.cpp`](noc_panelu.cpp) | noc liczona **kodem panelu** (`firmware/prototyp/src/noc_pliki.cpp`) z plików jednej doby wobec raportu nocy AirBridge, tabela w tolerancjach WY9 (bramka ADR-0036 pkt 5, opis niżej); porównanie funkcją panelu `porownanie::porownaj()` (`firmware/prototyp/src/porownanie.cpp`), tą samą co tryb `noc_zrodlo=porownanie` na panelu (R-DANE-60) |
| [`test_noce.py`](test_noce.py) | testy ze znanymi odpowiedziami na syntetycznej karcie |
| [`test_noc_panelu.py`](test_noc_panelu.py) | karta nocy panelu (`syntetyczna.py --noce-panelu`) i narzędzie `noc_panelu.sh` (w tolerancji, rozjazd, złe wejście) |

Tylko biblioteka standardowa Pythona 3, bez numpy.

## Noc panelu wobec raportu AirBridge (ADR-0036 pkt 5)

Bramka przed ustawieniem `noc_zrodlo=plik` na panelu (od 11.11.2026 wartość domyślna: wszystkie doby dostępne na AirBridge egzemplarza referencyjnego w tolerancji WY9, bez rozjazdów; narzędzie zostaje do sprawdzania nowych aparatów i trybów): dla każdej nocy dostępnej na AirBridge porównać noc liczoną kodem panelu z plików sesji z raportem JSON tej doby. Wymaga `g++` (albo `CXX=clang++`); program kompiluje się sam przy pierwszym uruchomieniu do `$TMPDIR/cpap-eink-noc-panelu`. Pliki i raporty trzymać poza repozytorium (dane prywatne).

```
# jedna doba; N = numer doby (dni od 1970-01-01, doba od południa), RRRRMMDD = ta sama doba
mkdir -p ~/Documents/noce-ab/RRRRMMDD
# pliki doby z AirBridge (ścieżki z /api/sessions?since=RRRRMMDD&order=asc) albo z kopii karty DATALOG/RRRRMMDD
# ~/.airbridge-netrc: jedna linia „machine ADRES login KONTO password HASŁO” (dane logowania AirBridge, poza repo)
curl --digest --netrc-file ~/.airbridge-netrc "http://AIRBRIDGE/api/report?source=edf&day=N" -o ~/Documents/noce-ab/RRRRMMDD.json
bash tools/kalibracja/noc_panelu.sh ~/Documents/noce-ab/RRRRMMDD ~/Documents/noce-ab/RRRRMMDD.json ~/Documents/noce-ab/STR.edf
```

Wynik: wiersze `metryka;panel;raport;różnica;tolerancja;wynik` (M-01 ≤ 1 min, M-02 i M-03 ≤ 0,5 / h, ciśnienia ≤ 0,5 cmH2O, M-07 ≤ 2 L/min, M-08 i M-09 ≤ 2 min, M-10 ≤ 0,02, M-13 ≤ 0,5 / min, M-14 ≤ 20 mL, M-15 ≤ 0,3 L/min, M-17 ta sama, M-18 ≤ 5 min, M-21 ten sam tekst), ostatnia linia `# wynik: …`; kod wyjścia 0 = w tolerancji, 1 = rozjazd, 2 = złe wejście. Domyślnie czyta pliki jak panel (bez `BRP` i `SAD`); `--pelny` czyta wszystkie jak `noce.py`. M-17 i M-18 strony raportu to wartości ścieżki JSON panelu (`nocZRaportu()`: R-DANE-27 z `sessions[]`, `start_ms` i `end_ms` jako czas aparatu zamknięte 09.11.2026). Kolumna `wynik`: `tak`, `NIE` (rozjazd), `brak w plikach` (raport ma wartość, pliki nie: też rozjazd), `brak w raporcie` (bez wpływu), `pominięte: aparat nie rozróżnia` (OA/h i CA/h nieznane w plikach przy 0 w raporcie, aparaty ResMed ASV), `tylko pliki` (M-28, PS: raport go nie ma), `brak (bez STR.edf)` (M-21 bez trzeciego argumentu); te same oceny zapisuje panel w trybie porównawczym (`/dane/porownanie.csv`). Wiele dób: pętla w powłoce, np. `for d in ~/Documents/noce-ab/2026*/; do n=$(basename $d); bash tools/kalibracja/noc_panelu.sh $d ~/Documents/noce-ab/$n.json ~/Documents/noce-ab/STR.edf | tail -1; done`. Metryka poza tolerancją: do `noc_json=` na panelu (R-DANE-45) i do wyjaśnienia.

## Pliki progów i zamrożenie

Progi są w dwóch plikach, objętych tym samym tagiem zamrożenia `progi-RRRRMMDD` (WY16):

| Plik | Co zawiera |
|---|---|
| [`docs/spec/progi.csv`](../../docs/spec/progi.csv) | każdy próg, który zmienia ocenę nocy albo liczbę pokazań liczoną w WY3, WY4 i WY6: progi reguł, definicje nocy i normy, limity pokazań (np. R-PIE-10, R-PIE-12, R-HUM-02, „wiem”, linie „Wciąż”), odczyt czujników; osobno progi samych kryteriów WY3–WY15 ze statusem `kryterium`, zamrażane razem z progami, żeby nie dało się ich zmienić po obejrzeniu wyniku |
| [`docs/spec/progi-poza-zakresem.csv`](../../docs/spec/progi-poza-zakresem.csv) | pozostałe miejsca z `[PRÓG]` bez wpływu na ocenę: czas interfejsu, limit tekstu, wygląd ekranu, stała AirBridge, parametr techniczny |

Przeniesienie progu z `progi.csv` do pliku poza zakresem po zamrożeniu to zmiana z WY16 (d), tak jak zmiana wartości.

W pliku poza zakresem są też cztery rodzaje wpisów, które nie są progami: `powtórzenie` (miejsce w spec powtarza wartość z `progi.csv`; opis wskazuje ten wiersz, test to sprawdza), `historia` (przegląd granicy 9.3 i historia wersji 10.3), `pytanie otwarte` (miejsce w spec, które tylko oznacza pytanie otwarte) i `opis` (zdania o samej zasadzie oznaczania `[PRÓG]`). Zostają, żeby test mógł wymagać pokrycia każdego `[PRÓG]` w spec.

`tools/test_progi.py` sprawdza, że wartość z pliku stoi w spec tą samą liczbą w tym samym kontekście: przy `k z n` cała para, przy pozostałych liczba ze słowem jednostki zaraz po niej (np. „3 kolejne noce”).

## Jak uruchomić na kopii karty

1. Skopiuj kartę SD do katalogu poza repozytorium (nie pracuj na samej karcie).
2. Uruchom (ścieżki ze spacjami w cudzysłowie):

```
python3 -I tools/kalibracja/noce.py "<katalog-zrzutu>/karta-SD-2026-10-25" \
    --od 2026-05-19 --do 2026-10-25 \
    --wyjscie "<katalog-zrzutu>/noce-2026-10-25.csv"
```

`--od` i `--do` to daty poranka (jak na ekranie panelu, 4.1). Bez nich tabela obejmuje wszystkie noce z karty. Dni bez nocy na karcie dostają wiersz z `na_liscie = nie`.

Skrypt otwiera pliki karty tylko do odczytu. Odmawia zapisu wyniku do katalogu karty i do repozytorium oraz czytania karty leżącej w repozytorium, bo prawdziwe noce nie mogą trafić do repo (WY12).

Skrypt należy do zamrożenia (WY16 (a)): commit skryptu zapisuje się w `WY16.skrypt_commit` w pliku progów, a zmiana skryptu, która zmienia wynik na nocach strojenia, oznacza nowy okres testowy.

Testy: `python3 -m unittest discover -s tools/kalibracja -p 'test_*.py'`.

## Czego skrypt nie robi

- Nie ocenia reguł: zdarzeń E, wzorców L, hipotez H, odkryć O, ekranów S-K, normy i nocy niepewnej. To etap B.
- Nie liczy GI (M-11, M-12): moduł zablokowany.
- Nie liczy T88, T90 ani pokrycia nocy pomiarem SpO2 i nie odrzuca artefaktów oksymetru (R-SKI-32): L-06 jest zablokowane do. Daje tylko medianę, minimum i maksimum kanałów `SpO2` i `Pulse`.
- Nie przelicza strefy czasu: godziny z plików to czas lokalny aparatu.

## Definicje wzięte z AirBridge

Wzór: AirBridge v2.3.0, `src/edf_report.cpp` i `src/report_edf.cpp` (repo m-kozlowski/airbridge).

| Pole | Jak liczone |
|---|---|
| `day` | nazwa katalogu `DATALOG/RRRRMMDD` (dzień terapii nadany przez aparat); data poranka = `day` + 1 |
| sesja | pliki BRP, PLD i SAD połączone po czasie z prefiksu (niżej, założenia); sesja ma plik BRP albo PLD; `start_ms`, `end_ms` = zasięg plików BRP, PLD i SAD sesji |
| `used_seconds` | suma czasu od pierwszego do ostatniego rekordu PLD z poprawną próbką ciśnienia (`Press.2s`, inaczej `EprPress.2s`, `MaskPress.2s`) |
| `counts`, `ahi` | zdarzenia z EVE, których chwila zapisu (onset; w AirBridge koniec zdarzenia, początek = onset − czas trwania) leży w czasie zapisu sesji (przedział półotwarty); `Apnea` = UA, `Arousal` i `RERA` = RERA; AHI = (OA + CA + UA + H) × 3600 / `used_seconds`; adnotacja z ujemnym albo nieczytelnym czasem trwania: noc częściowa, zdarzenia nieznane (jak AirBridge) |
| `klasyfikacja_bezdechow` | `tak`, gdy w plikach EVE dnia jest choć jedna adnotacja „Obstructive Apnea” albo „Central Apnea”; przy `nie` kolumny `oa`, `ca`, `m03_oa_h`, `m03_ca_h` i `m03_obturacyjne_h` są puste (brak danych, nie zero), a „Apnea” liczy się do UA i AHI  |
| `csr_dostepne` | `tak`, gdy jest plik CSL albo znaczniki CSR w EVE; przy `nie` `csr_seconds` i M-09 są puste  |
| `csr_seconds` | pary `CSR Start` i `CSR End` z plików CSL, przycięte do czasu zapisu sesji; brak CSL albo niesparowany znacznik = brak wartości |
| `leak_high_seconds` | czas próbek wycieku powyżej 24 L/min (stała AirBridge); obok czas powyżej 27 L/min, progu bardzo dużego wycieku E-23 z `progi.csv` (`m08_27_min`, do kalibracji) |
| kanały | mediana i P95 metodą najbliższej rangi z poprawnych próbek (wartość cyfrowa w zakresie z nagłówka); wyciek L/s → L/min, objętość L → mL; bez pliku PLD wartości ze STR.edf (`source = str`) |
| ciśnienia w ASV | w ASVAuto `Press.2s` to IPAP (M-05, M-05p), `EprPress.2s` to EPAP: `m06_wydechowe_med` (M-06) i `m06p_wydechowe_p95` (M-06p, P95 kanału jak wyżej, przy źródle STR z `TgtEPAP.95`, jeśli jest) |
| wsparcie PS | `m28_wsparcie_med` (M-28) i `m28p_wsparcie_p95` (M-28p): mediana i P95 metodą najbliższej rangi z różnic `Press.2s` − `EprPress.2s` liczonych próbka po próbce w tych samych chwilach pliku PLD, tylko gdy obie próbki są poprawne; nie różnica median kanałów. Tylko z plików PLD (`zrodlo_cisnienia = edf`); przy źródle STR puste |
| `settings` | rekord dnia ze STR.edf; tagi i kolejność jak w raporcie AirBridge (`MOP`, potem tagi trybu, np. `EAI`, `EAX`, `ANS`, `AXS`) |

`cisnienie_max` w ASVAuto = `EAX` + `AXS` bez ograniczenia do górnej granicy aparatu.

M-17 i M-18 liczone według R-DANE-27 z progami 18:00 i 4 h z `progi.csv`. M-19 i M-20 liczone z nocy ≥ 4 h na karcie. Noc ważna (4.2): `present`, bez `partial`, `events_known`, M-01 ≥ 4 h.

## Sprawdzian na prawdziwej karcie

Opcja `--sprawdz-str` porównuje czas terapii z plików DATALOG z podsumowaniem aparatu w STR.edf (`Duration`) i wypisuje tylko liczby zgodności, bez wartości zdrowotnych. Loader przechodzi sprawdzian, gdy:

| Część | Warunek |
|---|---|
| (a) | liczba nocy z `used_seconds` ≥ 4 h zgadza się (±2) z liczbą dni STR z `Duration` ≥ 240 min |
| (b) | `used_seconds` zgadza się z `Duration` × 60 (±5 min) w co najmniej 95% dni STR z terapią (noc bez rekordu albo bez `used_seconds` liczy się jako niezgodna) |

Kod wyjścia 0 = obie części zgodne, 1 = co najmniej jedna niezgodna. Zakres jak `--od` i `--do`.

## Założenia (domysł, do sprawdzenia na prawdziwej karcie i w WY9)

- **Grupowanie plików w sesje.** Na karcie AirCurve pliki jednej sesji mają prefiksy czasu różne o kilka sekund (np. EVE 02:52:52, BRP 02:52:57, PLD i SAD 02:52:58), a EVE i CSL są zwykle tylko przy pierwszej sesji dnia. AirBridge tego nie obsługuje: łączy wyłącznie pliki z identycznym prefiksem i wymaga kompletu 5 plików z `.crc` (`edf_recorder.cpp`, `session_files_complete_at`), bo sam zapisuje pliki z jednym prefiksem. Skrypt łączy BRP, PLD i SAD w sesję, gdy prefiks pliku jest najwyżej 10 s (`TOLERANCJA_SESJI_S`) późniejszy od pierwszego pliku sesji, a sesja nie ma jeszcze pliku tego rodzaju.
- EVE i CSL nie należą do sesji: zdarzenia ze wszystkich plików EVE dnia (bez powtórzeń) i znaczniki CSR z CSL przypisuje się do sesji po czasie. Plik EVE przy pierwszej sesji obejmuje cały dzień.
- Plik SAD bez BRP i PLD w swojej sesji dołączany jest do sesji, z którą nachodzi w czasie.
- `partial` = sesja bez pliku PLD, dzień z sesją bez żadnego pliku EVE, plik nieczytelny albo niepełny. Brak EVE przy drugiej i kolejnej sesji dnia nie czyni nocy częściową. `events_known` = jest co najmniej jeden czytelny plik EVE dnia i znany jest czas terapii każdej sesji.
- Bez pliku CSL skrypt bierze znaczniki CSR z EVE, jeśli tam są (`csr_zrodlo = EVE`).
- Katalog `DATALOG` przypisuje sesje do dni z granicą o 12:00 (jak 4.1).
- Długość sesji w wyjątku wczesnego snu (R-DANE-27) = `end_ms` − `start_ms`.
- **Bezdechy bez podziału i brak CSR na karcie AirCurve** (odczyt karty AirCurve): pliki EVE mają tylko „Recording starts”, „Apnea” i „Hypopnea”, a plików CSL nie ma. Skrypt nie udaje wtedy zera: OA, CA i M-09 są puste, `klasyfikacja_bezdechow = nie`, `csr_dostepne = nie`. Aparat rozróżnia rodzaj bezdechu i CSR w strumieniu, który czyta AirBridge; nie ma ich tylko na karcie SD. Dlatego L-01, L-02, E-33 i H-04 stroi się na nocach z AirBridge, nie z karty (WY3). Klasyfikację skrypt ocenia dla całego dnia, więc dzień aparatu, który dzieli bezdechy, ale tej nocy nie zapisał żadnego OA ani CA, też dostanie `nie` (domysł; dla kart SD AirCurve bez znaczenia). **Zmiana spec 09.11.2026 ([ADR-0037](../../docs/decisions/0037-asv-bez-rodzaju-bezdechu-i-csr.md), do akceptacji właściciela projektu; skrypt jeszcze bez zmian):** zdanie „aparat rozróżnia rodzaj bezdechu i CSR w strumieniu, który czyta AirBridge” było błędne dla ResMed ASV: AirBridge v2.3.1 też podaje same `Apnea`, a STR ma puste `OAI`, `CAI`, `CSR`. Docelowo: w trybach ASV i ASVAuto OA, CA i M-09 „aparat nie rozróżnia” (R-DANE-50) i metryki M-30…M-33 z EVE i PLD (R-DANE-51); w pozostałych trybach reguła „brak bezdechu z rodzajem w dniu = OA i CA nieznane” zastąpiona progiem UA ≥ 1 / h, od którego OA i CA są tylko dolną granicą (R-DANE-52), bo na dobrych nocach bez bezdechów dawała „nie wiem”. Zmiana `noce.py` razem z kodem panelu po etapie 2.
- Plik danych EDF+D (rekordy nieciągłe) skrypt zgłasza w `problemy` i uznaje noc za częściową, bo czytnik zakłada ciągłe rekordy.
- Próbka spoza zakresu cyfrowego z nagłówka jest niepoprawna; jeśli aparat zapisuje brak oksymetru inną wartością (np. 0), mediana SpO2 bez oksymetru będzie błędna.
- Adnotacje spoza listy zdarzeń są zliczane w kolumnie `adnotacje_inne` i nie wchodzą do wyników; wiersz poleceń wypisuje je na końcu.

## Niejasności w specyfikacji

Trzy punkty niżej to pytania otwarte do specyfikacji.

1. R-DANE-27: przykład „sesje 17:30–01:00 i 01:20–05:30 → M-17 = 1, M-18 = 17:30” przeczy treści wymagania, bo:20–05:30 trwa 4 h 10 min i zaczyna się po 18:00, więc wyjątku wczesnego snu nie ma. Fail-path R-DANE-27 mówi jeszcze inaczej („najdłuższa sesja ≥ 4 h liczy się niezależnie od godziny startu”). Skrypt stosuje treść wymagania (wynik: M-17 = 0, M-18 = 01:20).
2. `sessions[].start_ms` w AirBridge to początek najwcześniejszego pliku sesji, nie pierwsza poprawna próbka, więc M-18 może wypaść minutę przed założeniem maski. AirBridge v2.3.0 podaje też `end_ms` sesji, a Załącznik A ma koniec sesji jako `[DANE?]` .
3. Definicja `partial` w AirBridge zależy od obecności plików CSL, SAD i `.crc` (punkt wyżej); noc ważna (4.2) dziedziczy tę zależność.
