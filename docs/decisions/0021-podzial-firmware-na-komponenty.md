# ADR-0021: Podział firmware na komponenty

| | |
|---|---|
| Data | 01.11.2026 |
| Status | przyjęta (decyzja właściciela projektu 01.11.2026: wariant A, z komponentami T-KOMP-22…24 z sekcji 6) |
| Źródło | ADR-0020 (sekcja 6 specyfikacji technicznej, T-KOMP); spec 2.3 (15 modułów), 2.6 (rdzeń i moduły), 8.2 (utrzymanie); prototyp `firmware/prototyp/src/` na `main` (`23a67b8`), `tools/prototyp/testy.sh`, `firmware/prototyp/test/podglad.cpp` |

## Kontekst

Specyfikacja techniczna musi dla każdego wymagania wskazać część firmware, która je realizuje (T-KOMP, kolumna „Realizuje”). Trzeba więc ustalić, jak firmware jest pocięty na części. Od tego zależą testy na komputerze, symulator ekranów i to, ile z prototypu da się zachować.

Stan prototypu (sprawdzone w kodzie na `main`):

| Plik | Linie | Zależy od Arduino i sprzętu |
|---|---|---|
| `src/main.cpp` | 1 761 | tak: Wi-Fi, HTTP, NVS, LittleFS, ekran, przyciski, sen, OTA, port serwisowy, plus część logiki wybudzenia |
| pozostałe 15 plików `src/*.cpp` (noc i norma, stany odchylenia, AirBridge: Digest i parsowanie, otoczenie, czynniki, kokpit, ekran dnia, humor, dobranoc, nawigacja, rysowanie, bateria, sypialnia, cykl, wspólne narzędzia) | 4 226 | nie: każdy plik ma w nagłówku „bez zależności od Arduino” |

Dzięki temu już dziś `tools/prototyp/testy.sh` kompiluje logikę zwykłym kompilatorem na komputerze (1 946 sprawdzeń na `main`, ok. 2 200 w wersji prototypowej), a `test/podglad.cpp` rysuje ekrany na komputerze tym samym kodem co panel. Słabe miejsce to `main.cpp`: miesza obsługę sprzętu z decyzjami (kiedy pobrać, co narysować, co zapisać), więc tej części nie da się testować bez panelu.

Spec funkcjonalna dzieli zachowanie na 15 modułów (5.1 DANE … 5.15 GI). Moduły opisują, co panel robi, a nie gdzie to siedzi w kodzie: np. AWARIE (5.3) dotyczy pobierania, zapisu, czasu i ekranu naraz.

## Warianty

| Wariant | Na czym polega | Skutek i koszt | Rekomendacja |
|---|---|---|---|
| **A. Rdzeń bez sprzętu + adaptery + dyrygent wybudzenia** | Cała logika (dane, norma, reguły, wybór ekranu, treść, rysowanie do bufora) w rdzeniu bez Arduino. Sprzęt i sieć za cienkimi adapterami z jednym interfejsem każdy (sieć i HTTP, pamięć plików, NVS, ekran, przyciski, zegar, czujnik, dźwięk). Dyrygent wykonuje jedno wybudzenie: pyta rdzeń, co zrobić, i woła adaptery. Na komputerze adaptery mają atrapy, więc cały dzień panelu (także awarie z 5.3) da się przetestować i narysować bez sprzętu. | 15 plików logiki prototypu zostaje. `main.cpp` trzeba rozłożyć na dyrygenta (decyzje do rdzenia, testowalne) i adaptery. To jedna większa przebudowa (kilka dni pracy), bez zmiany zachowania. Macierz R → T-KOMP wskazuje konkretne części zamiast `main.cpp`. | **rekomendowany**: najmniejszy koszt przy największym zysku z testów; przedłuża kierunek, który prototyp już ma |
| B. Komponent na moduł spec (15 części 1:1) | Każdy moduł spec to osobna część firmware z własnym dostępem do sprzętu. | Macierz najprostsza (moduł = komponent), ale moduły przecinają warstwy: AWARIE, DANE i EKRAN musiałyby dzielić dostęp do sieci, pamięci i ekranu, więc albo kod się powtarza, albo i tak powstają adaptery jak w A. Trzeba przepisać także dzisiejsze pliki logiki. | odrzucony |
| C. Zostawić układ prototypu, porządkować przy okazji | Bez decyzji o podziale; poprawki tam, gdzie akurat trwa praca. | Najtańsze teraz. Połowa wymagań w macierzy wskazywałaby `main.cpp`, a awarie, sloty i noc S9 dałoby się sprawdzić tylko na panelu. Kryterium etapu „każde R-* ma komponent i test” spełnione formalnie, ale testy na sprzęcie byłyby wąskim gardłem przy każdej kalibracji. | odrzucony |

## Decyzja (proponowana: wariant A)

1. **Trzy warstwy.** Rdzeń (bez Arduino, kompiluje się na komputerze), adaptery (jedyne miejsce z bibliotekami sprzętu i sieci), dyrygent wybudzenia (cienki, wywołuje rdzeń i adaptery w ustalonej kolejności, sam niczego nie liczy).
2. **Rdzeń nie wie, że jest na panelu.** Dostaje czas, odpowiedzi HTTP, zawartość plików i stan przycisków jako dane; zwraca polecenia (pobierz, zapisz, narysuj, zaśpij do). Dzięki temu test na komputerze może odtworzyć całą dobę, w tym zmianę czasu, brak sieci i awarię karty.
3. **Komponenty T-KOMP to części rdzenia i adaptery**, nie moduły spec. Sekcja 6 ma tabelę „moduł spec → komponenty”, a macierz techniczna łączy wymagania z komponentami. Wstępna lista do sekcji 6 (nazwy robocze, ID nadane w sekcji 6): dyrygent wybudzenia; czas i strefa; pobieranie z AirBridge; źródła otoczenia (lokalne, pogoda, Qingping, czujnik panelu); magazyn danych z wersjami i migracjami; norma i stany odchylenia; silnik reguł (L, E, H, S-K, O; ADR-0023); wybór ekranu (pierwszeństwo, bramki, limity, humor); treść i rysowanie; przyciski i nawigacja; serwery lokalne (raport, formularz); konfiguracja i sekrety; dźwięk; log i diagnostyka; aktualizacja oprogramowania.
4. **Symulator ekranów** (sekcja 9) to rdzeń skompilowany na komputerze z atrapami adapterów; obraz ekranu jest wynikiem tego samego kodu rysowania co na panelu. Porównanie pikselowe z wzorcami z makiet (`design/png/`) idzie do planu testów (sekcja 11).
5. **Kolejność wdrożenia** (sekcja 13): najpierw adaptery i dyrygent bez zmiany zachowania (testy prototypu muszą dalej przechodzić), potem nowe komponenty (silnik reguł, serwery lokalne, konfiguracja).

## Konsekwencje

- Zachowanie panelu się nie zmienia; to zmiana wnętrza. Spec funkcjonalna bez zmian.
- `main.cpp` znika jako jeden plik; jego decyzje (np. granica 12:00 z R-DANE-35, zapis nocy, wybór ekranu nocnego) przechodzą do rdzenia i dostają testy na komputerze.
- Każdy adapter ma atrapę w `test/atrapy/`; test integracyjny z atrapą AirBridge (plan testów, sekcja 11) korzysta z tego samego interfejsu.
- Koszt pamięci i energii warstw jest pomijalny wobec rysowania e-ink i Wi-Fi; do potwierdzenia pomiarem w sekcji 2 (`[do zmierzenia]`).
