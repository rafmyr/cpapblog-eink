# ADR-0016: Wydanie 1.x obsługuje jeden tryb terapii: ASV na AirCurve 10 CS PaceWave (zastąpiona częściowo przez ADR-0030)

| | |
|---|---|
| Data | 24.10.2026 |
| Status | **zastąpiona w pkt 1–2 przez [ADR-0030](0030-wersja-1-dla-asv-cpap-i-autoset.md) (04.11.2026)**; pkt 3–6 przyjęte; zawęża ADR-0015 (przeznaczenie) do jednego trybu terapii w wydaniu 1.x; ADR-0002 i ADR-0012 bez zmian |
| Źródło | sprawdzenie archiwum na starcie etapu 2 (24.10.2026); decyzja 24.10.2026 |

## Kontekst
Specyfikacja do wersji 1.2 zakładała aparat ResMed AirSense 10 AutoSet: AutoSet sam podnosi ciśnienie po zdarzeniach, a ustawienia to minimum, maksimum i EPR. Na starcie etapu 2 sprawdzenie egzemplarza referencyjnego (24.10.2026) pokazało coś innego:

| Co | Stan |
|---|---|
| aparat, do którego wpięty jest AirBridge | ResMed AirCurve 10 CS PaceWave (`GET /api/status` → `pna`), aparat z trybem ASV (adaptacyjna wentylacja wspomagana) |
| tryb egzemplarza referencyjnego | ASV (decyzja 24.10.2026) |
| firmware AirBridge | v1.6.2: brak raportu nocy z Załącznika A; lista nocy i raport dnia są od v2.1.0, a pola ustawień (z trybem), oddychania okresowego i dużego wycieku od v2.2.0 |

ASV działa inaczej niż AutoSet. Aparat ma ciśnienie wydechowe (EPAP, stałe w trybie ASV albo zmienne w ASVAuto) i dokłada wsparcie ciśnieniem (PS) oddech po oddechu, żeby utrzymać wentylację blisko docelowej. Jego głównym celem są bezdechy centralne i oddychanie okresowe. Przez to część reguł spec znaczy na ASV coś innego (L-04, H-01, H-02, H-04, sygnał ciśnienia P95 w E-36, L-07, S-K-17) albo nie ma podstawy (H-01 przy stałym EPAP). Kalibracja progów na nocach ASV dla reguł opisanych pod AutoSet dałaby progi, które nie pasują do żadnego z tych aparatów.

Kontekst bezpieczeństwa ASV: w badaniu SERVE-HF (Cowie i in., NEJM 2015, poziom A) ASV zwiększało śmiertelność sercowo-naczyniową u osób z niewydolnością serca z obniżoną frakcją wyrzutową (LVEF ≤ 45%) i przewagą bezdechów centralnych. Kwalifikacja do ASV należy do lekarza; panel jej nie ocenia, ale pakiet dla lekarzy specjalistów i analiza MDR mają to uwzględnić.

## Decyzja
1. **Wydanie 1.x obsługuje jeden tryb:** tryb ASV aparatu ResMed AirCurve 10 CS PaceWave (egzemplarz referencyjny: AirCurve 10 CS PaceWave (ASVAuto); ASV albo ASVAuto rozróżnia odczyt tagu `MOP` po aktualizacji AirBridge). Kalibracja, zamrożenie progów i okres testowy (WY16) dotyczą tylko tego trybu.
2. **Pełna ścieżka zostaje w planie.** CPAP, AutoSet, APAP i druga odmiana ASV dochodzą w kolejnych wydaniach. Ich warunki i teksty zostają w spec (z ID bez zmian), oznaczone jako „następne wydanie”. Każdy kolejny tryb dostaje własne noce strojenia, zamrożenie i okres testowy, bo progi z ASV się nie przenoszą.
3. **Reguły zależne od trybu mają wariant na tryb** w jednej tabeli (spec 4.11): pola ustawień, znaczenie kanałów ciśnienia, maksimum i minimum, teksty hipotez. Reguła spoza tej tabeli działa tak samo w każdym trybie. Dodanie trybu później to dopisanie kolumny i kalibracja, bez przebudowy modułów.
4. **Noc w trybie spoza wydania albo w trybie nieznanym panel zapisuje, ale nie ocenia** (R-DANE-29): pokazuje liczby, ocenia tylko zdarzenia bez normy niezależne od trybu, nie wlicza nocy do okien reguł, normy, zmiany ustawień ani okresu testowego i mówi to linią uwagi.
5. **Granica ADR-0002 i ADR-0012 bez zmian.** Hipotezy na ASV wskazują obszar („ustawienia ciśnienia lub wsparcia”, „zakres ciśnienia i wsparcia”), nigdy wartość ani kierunek. Lista słów 9.3 dostaje `EPAP`, `PS` i `wsparci`.
6. **AirBridge od v2.2.0.** Raport nocy z polami Załącznika A wymaga firmware v2.2.0 albo nowszego (lista nocy od v2.1.0, `settings`, `csr_seconds`, `leak_high_seconds` od v2.2.0); v1.6.2, v2.0.x i v2.1.x nie są obsługiwane (sprawdzone 24.10.2026 w kodzie i na egzemplarzu referencyjnym). Pełny zakres wersji ustala test WY14. Aktualizacja dotyczy tylko AirBridge, nie firmware aparatu.

## Konsekwencje
- L-04 (i H-02) w trybie ASV nie działa do przeglądu przez lekarza specjalistę: warunek AutoSet nie przenosi się wprost, bo na zdarzenia obturacyjne w ASV odpowiada EPAP. (Nieaktualne od 25.10.2026, zob. adnotację niżej.)
- Spec 1.2: przeznaczenie (1.2), słownik (1.5), kontekst (2.1), użytkownik (2.2), założenie o aparacie (2.5), M-05, M-05p, M-06, M-21 (4.3), nowe 4.11, nowe R-DANE-29 z fail-path, L-04, H-01, H-02, H-04, znacznik K2 i R-KOK-12, R-SKI-02, strefa 6 raportu, WY2, WY3, WY13, 9.3, Załącznik A, przypadki 50–52.
- Etap 2: noce strojenia liczą się od pierwszej nocy ASV z raportem nocy (po aktualizacji AirBridge do v2.x). Starsze noce z archiwum aparatu (OSCAR; tylko noce w trybie obsługiwanym) służą jako tło do sprawdzenia, czy progi mają sens, a nie jako noce strojenia WY3.
- Zmiany L i H na ASV wymagają przeglądu przez drugą osobę i przez lekarza specjalistę.
- Koszt: pół dnia pracy w spec teraz; dłuższa ścieżka do „pełnego” produktu, bo każdy tryb to osobna kalibracja i okres testowy. Zysk: progi zamrożone w etapie 2 odpowiadają aparatowi, na którym są mierzone.
- Przy publikacji jako open source przeznaczenie (ADR-0015) mówi o jednym trybie, dopóki kolejne nie przejdą walidacji.

## Adnotacja (29.10.2026, decyzja, bez nowego ADR)
Doprecyzowanie pkt 3 (wariant na tryb), bez zmiany decyzji: w trybie ASV ekrany nie pokazują jednego „ciśnienia”. Kafel `Ciśnienie` w K1 i na S2a zastępuje EPAP (M-06), w K2 stoją wiersze `EPAP P95` (nowe M-06p) i `Wsparcie PS` (nowe M-28; w raporcie także P95, M-28p), a ciśnienie wdechowe (M-05, M-05p) zostaje tylko wewnętrznie do reguł o maksimum (L-04, H-02, znacznik K2, R-KOK-12, R-SKI-02). Sygnały dla użytkownika, które liczyły się na M-05p (E-36, L-07, S-K-17, S-K-20, S-K-30), w trybie ASV liczą się na M-06p. Wsparcie PS nie ma reguły, bo próg nie ma źródła (pakiet dla lekarza specjalisty); skąd panel weźmie próbki do PS, jest do ustalenia. Powód: na nocach referencyjnych (ASVAuto) zmienność ciśnienia wdechowego między nocami pochodzi prawie cała ze wsparcia, a EPAP i PS znaczą co innego (liczby w CHANGELOG 29.10.2026). Granica ADR-0002 bez zmian: pokazanie zmierzonych wartości EPAP i PS to opis pomiaru, jak dotąd ciśnienia (spec 9.3). Spec: 4.11, R-DANE-32, R-KOK-15.

## Adnotacja (25.10.2026, decyzja, bez nowego ADR; zapisana 31.10.2026 w przeglądzie dokumentacji)
L-04 i H-02 w ASVAuto działają w wariancie EPAP (P95 EPAP przy `EAX`), spec 4.11; punkt „Konsekwencji” o L-04 jest nieaktualny. Próg wariantu EPAP zostaje do przeglądu przez lekarza specjalistę.

## Adnotacja (04.11.2026, konsultacja kliniczna i ADR-0029)
Warianty ASV zmienione po konsultacji klinicznej (spec 4.11): karencja L-01 z CPAP nie obowiązuje na ASV (R-SKI-43), L-04 w ASVAuto liczy tylko OA, H-01 nie działa, H-04 zastępuje H-08, S8 nie ma kroku przy P90 CA/h, raport ma stałe zdanie o wskazaniu do ASV (R-LEK-34). Zakres skierowań w wydaniu 1.0 i stan „tylko raport”: [ADR-0029](0029-zakres-skierowan-w-wydaniu-1-0.md); L-02(a) na ASV należy do rdzenia.

## Adnotacja (04.11.2026, ADR-0030)
Punkty 1–2 zastąpione przez [ADR-0030](0030-wersja-1-dla-asv-cpap-i-autoset.md): wersja 1 obsługuje tryby `CPAP`, `AutoSet`, `ASV` i `ASVAuto`; kalibracja, zamrożenie i okres testowy na tryb przechodzą na czas po wersji 1. Punkt 4 (noc bez oceny) dotyczy od tej daty tylko trybu nieznanego i trybów spoza tej listy (R-DANE-29). Punkty 3, 5 i 6 bez zmian. Konsekwencja „przeznaczenie mówi o jednym trybie, dopóki kolejne nie przejdą walidacji” nieaktualna.
