# ADR-0051: Menu pacjenta i odczucia oddechowe w poradach „Dobrego startu”

| | |
|---|---|
| Data | 22.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 22.11.2026) |
| Źródło | scenariusz wersji 2 programu „Dobry start”; instrukcje ResMed sprawdzone 22.11.2026 (menu pacjenta); książka, s. 44 („za niskie ciśnienie: duszność; za wysokie: trudny wydech; jasno komunikować lekarzowi”) |
| Rozszerza | [ADR-0050](0050-opcje-komfortu-w-poradach.md) (opcje komfortu w poradach) |
| Doprecyzowuje | [ADR-0002](0002-panel-nie-zmienia-ustawien.md) (tylko teksty programu „Dobry start”) |
| Bez zmian | ekrany S-K, E, L, H, K, poranek, raport dla lekarza, karty z budzikiem: ADR-0002 bez wyjątku |

## Kontekst

ADR-0050 pozwolił poradom programu nazwać opcję komfortu i podać kierunek, ale bez drogi w menu („pokaże instrukcja aparatu”) i bez kierunku przy uldze wydechowej. Przegląd scenariusza wersji 2 wskazał dwie luki:

- Panel zna tryb ostatniej nocy (CPAP, AutoSet, ASV), więc wie, czy to AirSense 10, czy AirCurve 10 CS PaceWave; instrukcja producenta sama opisuje pacjentowi menu „Moje opcje” krok po kroku. Porada może więc dać wskazówkę zamiast odsyłać do instrukcji.
- Dyskomfort oddechowy. Początkujący nie wie, że „mało powietrza” i „ciężki wydech” to dwa różne odczucia i że oba warto zgłosić lekarzowi; bez słów nie umie tego powiedzieć.

Weryfikacja w instrukcjach ResMed 22.11.2026 (AirSense 10 po polsku i po angielsku, przewodnik kliniczny, AirCurve 10 CS PaceWave):

- „Moje opcje”: „Czas narastania”, „Poziom Wilg.”, „Wykonaj dopasowanie maski”; „Więcej opcji” tylko wtedy, gdy udostępni je dostawca: „Dekompresja” (ulga wydechowa, po angielsku „Pressure Relief”), „SmartStart”, „Maska”, „Uruchomienie ogrzewania”.
- Ulga wydechowa (EPR) ma kilka poziomów i dwa typy („cała terapia” albo „tylko rampa”); pacjent widzi ją tylko wtedy, gdy dostęp ustawił dostawca.
- AirCurve 10 CS PaceWave (ASV): „Ramp Time” w My Options, bez ulgi wydechowej. Polskiej instrukcji nie znaleziono (pytanie otwarte).

## Decyzja

**A. Objaśnienie odczucia oddechowego.** Tylko w tekstach programu porada może opisać dwa odczucia z kierunkiem ciśnienia, jako objaśnienie, nie polecenie: duszność, „mało powietrza”, ciężko się oddycha: to może być za niskie ciśnienie; ciężko zrobić wydech, aparat za mocno dmucha: to może być za wysokie ciśnienie. Warunki: zawsze „może być” (nie diagnoza), zawsze odesłanie „powiedz to lekarzowi tymi słowami” **w tym samym tekście**, nigdy „zmień”, „ustaw”, „podnieś”, „obniż”. W trybie ASV bez kierunku: „to mogą być ustawienia do poprawy”. Porady: DS-S07, DS-S07.asv; słowa do lekarza także DS-D50.

**B. Droga w menu pacjenta.** Tylko w tekstach programu porada może podać drogę w **menu pacjenta aparatu („Moje opcje”)** i nazwy jego pozycji dla opcji komfortu: czas narastania (rampa), poziom wilgotności, temperatura podgrzewanej rury, dopasowanie maski, uruchomienie ogrzewania, ulga wydechowa. Ulga wydechowa: porada może opisać, co robi i że ma poziomy (wyższy daje większą ulgę), bez liczby i bez zalecenia poziomu, oraz dwa przypadki: „widzisz ją w Moich opcjach: możesz ją włączać i zmieniać samodzielnie” albo „nie ma jej tam: poproś lekarza albo dostawcę o włączenie”. Bez kierunku („zwiększ ulgę”, „ustaw na maksimum”). Teksty z drogą w menu mają warianty trybu tam, gdzie menu się różni (ASV: bez ulgi wydechowej, czas narastania bez Auto).

**Nadal zakazane wszędzie, także w poradach:** menu kliniczne albo serwisowe i droga do niego (kombinacje przycisków), AirBridge, liczby ciśnienia i zakresu, liczby i poziomy opcji (np. „czas narastania 20 minut”, „EPR 3”), tryb pracy, każde polecenie dotyczące ciśnienia, zakresu, trybu albo kierunku ulgi wydechowej, diagnoza.

**Pilnowanie.** Test tekstów `tools/zestaw/adr2.py` od 22.11.2026: „Moje opcje” i nazwy pozycji przechodzą, menu kliniczne, serwisowe i AirBridge to błąd; „możesz ją włączać i zmieniać” dla ulgi wydechowej przechodzi tylko w tekście, który mówi o Moich opcjach, i bez słowa kierunku; „za niskie / za wysokie ciśnienie” bez słowa „lekarz” w tym samym tekście to błąd (reguła „kierunek ciśnienia bez lekarza”); liczba przy czasie narastania, poziomie wilgotności i uldze to błąd. Samotest ma przykłady obu stron (27 złych, 21 dobrych).

## Konsekwencje

- Porady wyzwalane danymi i porady z tej decyzji wchodzą do oceny wobec MDR (pytanie otwarte, ADR-0015). Powrót do ADR-0050 to zmiana tekstów porad (DS-D06, DS-D19, DS-D40, DS-D46, DS-S07, DS-S08, DS-Z09, DS-Z10, DS-A02) i odwrócenie testu, bez zmiany kodu panelu.
- **Nazwy menu mogą się różnić od aparatu.** Polska instrukcja AirSense 10 jest z 2014 r.; nowsze oprogramowanie może mieć inne nazwy (np. „EPR” albo „Ulga ciśnienia” zamiast „Dekompresja”). Do potwierdzenia na aparacie (pytanie otwarte); do tego czasu porada o uldze nie podaje polskiej nazwy pozycji, a o wilgotności podaje „Poziom wilg.”, jak na ekranie z instrukcji.
- **Test słów jest heurystyką.** Zaimek („możesz ją zmieniać”) w zdaniu bez słowa „ulga” nie jest łapany; dlatego reguła B jest też punktem przeglądu treści przez lekarza.
- Koszt: 9 porad z drogą w Moich opcjach, 2 z objaśnieniem odczucia, rozszerzony test i samotest; bez zmian w kodzie panelu (teksty są danymi).

## Odrzucone warianty

| Wariant | Dlaczego nie |
|---|---|
| zostać przy ADR-0050 (kierunek bez drogi w menu, „pokaże instrukcja aparatu”) | odrzucone przez właściciela: panel zna aparat, a odsyłanie do instrukcji, którą pacjent rzadko ma pod ręką, zostawia go bez wskazówki |
| droga w menu także dla ustawień klinicznych z ostrzeżeniem „tylko z lekarzem” | menu kliniczne zmienia ciśnienie i tryb; sama droga do niego to zachęta, ryzyko przy bezdechu centralnym i ASV; granica ADR-0002 |
| zalecany poziom ulgi wydechowej (np. „zacznij od 1”) | poziom zależy od ciśnienia i rodzaju bezdechu; przy bezdechach centralnych ulga bywa przeciwwskazana (książka, s. 45); liczba łamie ADR-0002 |
| kierunek ciśnienia w poradach wyzwalanych danymi („AHI wysokie: ciśnienie za niskie”) | to już ocena danych konkretnej osoby i diagnoza przyczyny; odczucie opisuje pacjent, dane ocenia lekarz |

## Zmiana 2026-11-22

Porady z tej decyzji są w kodzie panelu razem z wersją 2 programu „Dobry start”; treści bez zmian, test ADR-0002 (`tools/zestaw/adr2.py`) przy budowie zestawu.
