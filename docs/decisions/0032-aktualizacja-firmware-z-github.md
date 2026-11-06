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
