# ADR-0011: Konfiguracja przyciskami i formularzem na telefonie przez QR

| | |
|---|---|
| Data | 22.10.2026 |
| Status | przyjęta |
| Źródło | decyzja 22.10.2026 |

## Kontekst
Panel nie ma dotyku, a v0.6c nie opisywał, jak użytkownik zmienia konfigurację (adres AirBridge, Wi-Fi, humor, buzzer, podpowiedź „co do poprawy”, typ poduszki, okresy wymiany, czynniki w raporcie, miasto do pogody). Założenie autora projektu: panel ma 3 przyciski, użyteczne do prostych rzeczy, a reszta ustawień to proste formularze wypełniane na telefonie po otwarciu strony kodem QR.

## Decyzja
1. **Proste przełączniki przyciskami:** ekran „Ustawienia” (wejście: ◀ przytrzymane 2 s w kokpicie 2) z humorem, buzzerem i podpowiedzią „co do poprawy”.
2. **Reszta przez formularz na telefonie** (opcjonalny w wydaniu): panel pokazuje kod QR, uruchamia lokalny serwer na ograniczony czas z jednorazowym tokenem (ten sam mechanizm co raport dla lekarza, ADR-0005), telefon w domowym Wi-Fi otwiera formularz.
3. **Pierwsze uruchomienie:** panel tworzy własną sieć Wi-Fi i pokazuje QR do niej; formularz pierwszej konfiguracji (Wi-Fi, adres AirBridge) otwiera się na telefonie. Ta część formularza jest obowiązkowa.
4. **Zanim powstaną pozostałe formularze:** panel używa wartości domyślnych ze specyfikacji, a zapasową drogą zmiany jest plik konfiguracji na karcie microSD, czytany przy starcie.

## Konsekwencje
- Lokalny serwer służy nie tylko raportowi, więc wymagania bezpieczeństwa (token, limit czasu, wyłączenie po błędnych żądaniach) obejmują też formularz.
- Formularz pierwszej konfiguracji trafia do pierwszego wydania; formularze pozostałych ustawień mogą przyjść później bez zmiany specyfikacji funkcjonalnej.
- Zgodne z ADR-0001: wszystko w sieci lokalnej, nic nie wychodzi z domu.
- Koszt oszacuje etap 3 (spec techniczna); plik na karcie ok. 1–2 h [domysł].

## Adnotacja 01.11.2026 (ADR-0022, wariant A)

Pkt 4 zmieniony: plik konfiguracji jest w pamięci flash panelu. Plik na karcie microSD zostaje drogą zapasową, ale panel go wczytuje (kopiuje do pamięci flash), a nie czyta z karty przy każdym starcie. Reszta decyzji bez zmian. Zmiana treści wymagań: osobna zmiana spec funkcjonalnej (ADR-0022, „Konsekwencje”).


## Adnotacja 01.11.2026

Formularz pierwszej konfiguracji z pkt 3 ma też pola logowania do AirBridge: nazwę konta (wpisane `admin`, bo nazwa konta jest ustawieniem AirBridge) i hasło. Hasło jest opcjonalne: AirBridge domyślnie wymaga logowania HTTP Digest (konto `admin`, hasło `airbridge`), ale logowanie wyłączają puste konto i hasło w AirBridge, a lista zaufanych adresów `auth_whitelist` (od v2.2.0) je pomija. Puste hasło w panelu oznacza żądania bez logowania. Hasło jest sekretem jak hasło sieci domowej (R-NF-22). Decyzja bez zmian. Treść wymagań: spec funkcjonalna 5.6.4, 5.6.5, R-KONF-09, R-KONF-37, R-KONF-38, R-KONF-39.

## Zmiana 04.11.2026 (bez zmiany decyzji)

Ekranu „Ustawienia” (U-01) i formularza z pkt 2 w pierwszej wersji beta nie było (miały być na koniec); K2 ◀ 2 s otwierało ekran „Test sprzętu” (U-05). Wtedy drogą z pkt 4 jest plik `/konfiguracja.txt` w pamięci panelu (wiersze `klucz=wartość`): daty i okresy wymiany sprzętu (`wymiana.*`, `okres.*`, `poduszka=`), pyłki (`pylki=`, `pylek.*`), `profil=publiczny|prywatny`, `co2=tak|nie`, `raport=pdf|sciaga` i `raport.czynniki=`. Panel sam dopisuje do niego tylko daty wymiany (R-MAS-01, R-MAS-11). Zmiana sieci Wi-Fi na egzemplarzu testowym: kontakt z osobą wydającą panel, nie przycisk. Lista kluczy z wartościami domyślnymi: `firmware/prototyp/README.md`.

## Dopisek 05.11.2026 (bez zmiany decyzji)

Sieć i AirBridge są w NVS panelu (przestrzeń `sekrety`, `firmware/prototyp/src/sekrety.h`); `include/secrets.h` daje tylko wartość początkową. K2 ◀ 2 s otwiera U-01 „Ustawienia panelu” z pozycjami `Test sprzętu` (U-05) i `Więcej na telefonie` (U-03, pkt 2): formularz w domowym Wi-Fi z polami sieci, AirBridge, profilu i dat oraz okresów sprzętu, ten sam mechanizm co raport (token, 15 min, 5 złych żądań, tylko adres prywatny). Nowe dane sieci i AirBridge panel sprawdza przed zapisem; przy błędzie zostają poprzednie. Przełączniki z pkt 1 i pierwsza konfiguracja przez własną sieć panelu (pkt 3) są opisane w specyfikacji 5.6.

