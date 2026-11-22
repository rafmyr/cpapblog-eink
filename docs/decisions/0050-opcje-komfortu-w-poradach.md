# ADR-0050: Opcje komfortu w poradach „Dobrego startu”

| | |
|---|---|
| Data | 22.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 22.11.2026) |
| Źródło | [ADR-0049](0049-dobry-start.md) pkt 7 (wersja proponowana: zakaz bez wyjątku); instrukcje ResMed sprawdzone 22.11.2026: AirSense 10, AirCurve 10 CS PaceWave, ClimateLineAir |
| Doprecyzowuje | [ADR-0002](0002-panel-nie-zmienia-ustawien.md) (wyjątek tylko dla tekstów programu „Dobry start”) i [ADR-0027](0027-ekran-dzienny.md) pkt 7 oraz „Granice bez zmian” (zdanie o nawilżaczu i temperaturze rury) |
| Bez zmian | ekrany S-K, E, L, H, K, poranek, raport dla lekarza, karty z budzikiem: dalej ADR-0002 bez wyjątku |

## Kontekst

ADR-0002 zabrania podawania wartości ustawień aparatu i drogi do ich zmiany. Projekt „Dobrego startu” (ADR-0049, wersja proponowana) rozciągnął ten zakaz na nawilżanie, temperaturę rury, rampę i ulgę na wydechu: przy suchości albo wodzie w rurze porada mówiła tylko „zapytaj dostawcę”.

Właściciel projektu odrzucił ten zakaz 22.11.2026. Powody:

- Instrukcje producenta same każą pacjentowi zmieniać te opcje: AirSense 10 ma w menu pacjenta „Ramp Time” i „Humidity Level”, a w tabeli kłopotów radzi przy suchym nosie „Adjust the Humidity Level”, przy suchych ustach „Increase the Humidity Level”, przy kroplach w masce i rurze „Humidity level may be set too high”. ClimateLineAir: „Tube Temperature” i „Humidity Level” w menu pacjenta, przy skraplaniu najpierw cieplejsza rura, potem mniej nawilżania. AirCurve 10 CS PaceWave (ASV): „Ramp Time” w menu pacjenta, a przy „za mało powietrza” rada „turn Ramp Time off”.
- Ulgę na wydechu (EPR) AirSense 10 pokazuje w menu pacjenta tylko „when enabled by your care provider”; aparat ASV jej nie ma.
- Książka autora projektu (s. 44–45) i blog (tekst z 2026) traktują opcje komfortu jako narzędzie pacjenta: „W menu na start tylko opcje komfortu: rampa, ulga wydechowa, nawilżanie, temperatura rury. Ciśnienia terapeutyczne zostaw specjalistom.”
- Zalecenia: nawilżanie zmniejsza objawy suchości (poziom A), ulga na wydechu bez klinicznie istotnej różnicy w użyciu (poziom A), dowodu na rampę nie znaleziono; ryzyka dla pacjenta przy zmianie kierunku nawilżania, temperatury rury czy długości rampy nie wskazuje żadne źródło. Ryzyko jest przy ciśnieniu, zakresie, trybie i poziomie ulgi (bezdech centralny, ASV).

## Decyzja

1. **Gdzie.** Wyjątek dotyczy wyłącznie tekstów programu „Dobry start” (`docs/spec/katalog/porady.csv`, ekran S-K-48, strona „Wszystkie porady”). Ekrany S-K, E, L, H, kokpit, poranek, raport dla lekarza i karty z budzikiem są bez zmian (ADR-0002 bez wyjątku).
2. **Co wolno.** Porada może nazwać opcję komfortu, którą pacjent zmienia sam według instrukcji użytkownika aparatu: **poziom nawilżania, temperatura podgrzewanej rury, rampa (jej długość, wyłączenie)**; podać **kierunek** („zwiększ trochę nawilżanie”, „mniej nawilżania albo cieplejsza rura”, „skróć albo wyłącz rampę”) i **odesłać do instrukcji aparatu** („Jak zmienić nawilżanie, pokaże instrukcja aparatu.”).
3. **Ulga na wydechu (EPR).** Porada może opisać, czym jest, i odesłać do lekarza albo dostawcy („Zapytaj lekarza o ulgę.”). Bez kierunku, bez poziomu, bez polecenia włączenia albo wyłączenia.
4. **Nadal zakazane wszędzie** (także w poradach): liczby i poziomy ustawień (np. „nawilżanie na 4”, „27 °C”, „rampa 20 min”, „EPR 3”), droga w menu aparatu albo AirBridge (nazwy menu, kolejność przycisków, pokrętło), wszystko, co każe zmienić ciśnienie, zakres, tryb albo poziom EPR, diagnoza. Porada przypomina, że ciśnienie i tryb ustawia lekarz (DS-D40).
5. **Pilnowanie.** Test tekstów `tools/zestaw/adr2.py` rozróżnia opcje komfortu od ciśnienia: kierunek dla nawilżania, rury i rampy przechodzi, liczba ustawienia, droga w menu i polecenie dotyczące ciśnienia, zakresu, trybu albo EPR to błąd; wynik to kod wyjścia 0 albo 1, świadome wyjątki w `tools/zestaw/adr2_wyjatki.csv` (dziś pusta). Ten sam test biegnie przy budowie zestawu (R-DS-41).

Porady, które z tego korzystają (22.11.2026): DS-D06, DS-D45, DS-D46 i DS-D46.asv, DS-D19, DS-Z09, DS-Z10, DS-A02, DS-D32, DS-Z11, DS-D40.

## Konsekwencje

- **Ocena wobec MDR.** Porady wyzwalane danymi i porady z kierunkiem dla opcji komfortu wchodzą do oceny przeznaczenia panelu wobec MDR (ADR-0015, pytanie otwarte). Program jest domyślnie wyłączony; do rozstrzygnięcia DS-Z08, DS-Z08.asv i DS-A08 tylko odsyłają do lekarza. Powrót do zakazu to zmiana tekstów porad i odwrócenie testu, bez zmiany kodu panelu.
- **Test słów musi odróżniać komfort od ciśnienia** (pkt 5); błąd testu w jedną stronę przepuściłby polecenie zmiany ciśnienia, w drugą blokowałby dozwolone porady. Samotest na przykładach w `adr2.py` pilnuje obu stron.
- **Instrukcja aparatu zamiast drogi w menu.** Panel nie wie, jaki model aparatu i rury ma osoba (Climate Control Auto albo Manual, rura zwykła albo podgrzewana), więc nie podaje kroków; odsyła do instrukcji.
- **Koszt:** 3 nowe wiersze porad (DS-D45, DS-D46, DS-D46.asv), 11 zmienionych tekstów, nowy test i lista wyjątków; plan zachowuje 42 miejsca (DS-D29 i DS-D36 przeszły do „ponad program”, ADR-0049).
- Przegląd lekarza PSG przed pokazaniem porad osobom spoza testów (pytanie otwarte) obejmuje porady z tej listy.

## Odrzucone warianty

| Wariant | Dlaczego nie |
|---|---|
| zakaz bez wyjątku: przy suchości i wodzie w rurze tylko „zapytaj dostawcę” (ADR-0049, wersja proponowana) | odrzucony przez właściciela projektu 22.11.2026; porada była ostrożniejsza niż instrukcja aparatu, którą pacjent ma w ręku, a sprawa wraca do dostawcy, choć pacjent może ją rozwiązać sam w kilka nocy |
| opcje komfortu z wartościami (np. nawilżanie 85% przy 27 °C, jak książka s. 44–45) | wartości zależą od modelu, rury i trybu Climate Control; liczba ustawienia na panelu łamie ADR-0002 |
| ulga na wydechu z kierunkiem („włącz”, „zwiększ”) | dostęp do niej daje dostawca, a ulga przy bezdechu centralnym i u osób na ASV bywa przeciwwskazana (książka s. 45, blog); zostaje opis i odesłanie |

## Zmiana 2026-11-22 (wdrożenie)

- Krok DS-D46 brzmi „→ Wydech pod ciśnieniem męczy? Zapytaj lekarza o ulgę.” (decyzja właściciela projektu 22.11.2026): poprzednie „Zapytaj o ulgę na wydechu.” nie mówiło, kogo pytać, i mogło czytać się jak droga do zmiany EPR; pkt 3 bez zmian w treści.
- Test tekstów (pkt 5) jest od wdrożenia przy budowie zestawu: `tools/zestaw/adr2.py` i `tools/zestaw/adr2_wyjatki.csv`. Wzorce i samotest bez zmian.
