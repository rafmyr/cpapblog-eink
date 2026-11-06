# ADR-0005: Raport A4 dla lekarza przez kod QR

| | |
|---|---|
| Data | 22.10.2026 |
| Status | przyjęta warunkowo |
| Źródło | specyfikacja 7d |

## Kontekst
Raport musi trafić do lekarza bez chmury i bez modyfikacji AirBridge.

## Decyzja
Panel generuje PDF A4 (≤ 250 kB), pokazuje kod QR i na 15 min uruchamia lokalny serwer z jednorazowym tokenem; telefon w domowym Wi-Fi pobiera PDF. Kopia na microSD.

## Konsekwencje
Koszt 46–68 h. **Warunek:** przed budową PDF ankieta w grupie „Bezdech Senny” i pytanie do 2–3 lekarzy specjalistów; jeśli < 30% przeczyta wydruk, zamiast PDF ekran-ściąga i link do eksportu SleepHQ/ZIP (5–10 h).

**Zmiana 04.11.2026 (bez zmiany decyzji):** warunek z „Konsekwencji” (ankieta i pytanie do lekarzy specjalistów) nie jest rozstrzygnięty, więc wersja beta ma oba warianty. Ustawienie `raport=pdf` (domyślnie) albo `raport=sciaga` w pliku konfiguracji panelu `/konfiguracja.txt`; rozstrzygnięcie wydania zostaje przy WY8. PDF jednej strony A4 (ok. 45–55 kB) składa panel bez biblioteki, zapisuje w `/raporty/`, a serwer działa 15 min z tokenem 128 bitów i wyłącza się po 5 złych żądaniach albo po ◀ (R-LEK-33). Odchylenie od spec 5.13.3: krój nieosadzony (Helvetica), do decyzji. Kopii na microSD w wersji beta nie ma (R-LEK-22, R-LEK-23: karta nie jest czytana). Teksty raportu spoza tabel zatwierdził autor projektu (spec 5.13.3).
