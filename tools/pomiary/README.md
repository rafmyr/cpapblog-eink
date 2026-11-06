# Analiza plików pomiarowych z panelu

Skrypt `analiza.py` liczy z plików ściągniętych z panelu liczby do specyfikacji technicznej, sekcja 2.4
(plan pomiarów). Wypisuje tylko liczby zbiorcze w markdown (tabele), gotowe do wklejenia do notatki
w notatce pomiarowej (poza repozytorium). Surowe pliki z panelu nie trafiają do repozytorium (reguła WY12).

| Tryb | Plik z panelu | Co liczy |
|---|---|---|
| `dryf` | `/pomiary/dryf.csv` (firmware testowe pomiaru 12, gałąź `etap3-firmware-testowy`) | zegar panelu: tempo dryfu na odcinkach między wybudzeniami (%, s/dobę), min, mediana, max, chwila bezpiecznika 2 min; PCF8563: regresja przesunięcia w czasie (ppm, s/dobę, błąd w 72 h), reszty, rozdzielczość z przelotu NTP; ocena wobec kryterium ≤ 5 min w 72 h `[PRÓG]` ((d)) i wniosek do |
| `bateria` | `/bateria.csv` (dziennik baterii prototypu, wiersz na każde wybudzenie) | na każdą dobę lokalną (Europe/Warsaw): wybudzenia według powodu, wybudzenia z Wi-Fi, suma Wi-Fi wobec 10 min i 45 min; czas aktywności bez Wi-Fi (pomiar 2, ≤ 2 s) i Wi-Fi na wybudzenie (pomiar 3, ≤ 8 s, T-NF-05); ładowania i spadek napięcia z ekstrapolacją do 3,5 V wobec 30 dni (pomiar 9, R-NF-01), zawsze jako `[szacunek]` |

Pomiar 16 (pamięć flash powyżej 16 MB) nie ma skryptu na Macu: to szkic na panel w osobnym środowisku pio, [`firmware/pomiar16/`](../../firmware/pomiar16/platformio.ini) (źródło `src/main.cpp`). Używa tej samej płytki 32 MB (`firmware/boards/reterminal_e1001.json`) i tablicy partycji (`firmware/partycje-e1001.csv`) co firmware `e1001`, ale nie wchodzi do jego budowy. Wynik czyta się z portu szeregowego (wiersz ` WYNIK: ZALICZONE` albo `NIE ZALICZONE: {powód}`), wgranie jak firmware (`pio run -e pomiar16 -t upload` w `firmware/pomiar16`). Budowa bez wgrywania: `cd firmware/pomiar16 && pio run -e pomiar16`. Skompilowany, na sprzęcie jeszcze nie uruchomiony.

Tylko biblioteka standardowa Pythona, wersja 3.9 lub nowsza. Nic nie trzeba instalować.

## Jak uruchomić na Macu

1. Ściągnij pliki z panelu w oknie serwisowym (`tools/prototyp/zrzut_plikow.py` albo serwer `:8080/pomiary`
   firmware testowego) do katalogu poza repozytorium:
   `<katalog-pomiarow>/` (np. podkatalog z datą ściągnięcia).
2. W Terminalu, w katalogu repozytorium:

```
python3 tools/pomiary/analiza.py dryf "<katalog-pomiarow>/firmware-testowy/dryf.csv"
python3 tools/pomiary/analiza.py bateria "<katalog-pomiarow>/bateria.csv"
python3 tools/pomiary/analiza.py bateria "<katalog-pomiarow>/bateria.csv" --od 2026-11-04 --do 2026-11-10
```

3. Wynik pojawia się w Terminalu; zapis do pliku: dopisz na końcu `> wynik.md`. Ten wynik (same liczby) można
   wkleić do notatki w notatce pomiarowej (poza repozytorium).

`--od` i `--do` (doby lokalne, włącznie) wycinają okres, np. tylko doby po wgraniu nowego firmware.

Przy złym pliku (nie ma go, to nie ten format, żadnego poprawnego wiersza) skrypt pisze komunikat po polsku
i kończy z kodem 2. Puste linie i urwany ostatni wiersz są pomijane, a ich liczba jest w wyniku.

## Czego skrypt nie wie

- `dryf`: czy panel między dwoma wybudzeniami spał. Odcinek z tempem poniżej 25% mediany jest liczony jako
  czas bez uśpienia (okno serwisowe) i nie wchodzi do statystyki. Po bezpieczniku tempo zegara panelu jest
  przybliżeniem (zakłada synchronizację z NTP w poprzednim wybudzeniu).
- `bateria`: która doba była bez danych z AirBridge (dlatego ocena wobec obu progów Wi-Fi) i które wybudzenie
  z Wi-Fi miało import nocy (T-NF-05 dotyczy wybudzeń bez nowych danych). Krzywa napięcia baterii nie jest
  liniowa, a krzywej tej baterii jeszcze nie ma (pomiar 10), więc liczba dni to `[szacunek]`.

Testy: `python3 -m pytest -q tools/pomiary` (w CI razem z całym katalogiem `tools`).
