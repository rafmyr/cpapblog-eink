# Zrzut AirBridge

`zrzut_airbridge.py`: zapisuje poza repozytorium odpowiedzi `/api/status`, listy nocy i ostatnich nocy, tak jak czyta je panel (logowanie Digest, dopytywanie o zadanie). Wyłącznie GET. `podsumowanie.txt` ma same nazwy pól, bez wyników i bez numeru seryjnego. Testy na atrapie HTTP: `python3 -m pytest -q tools/airbridge`. Sprawdzony na AirBridge v2.3.0 (AirCurve, logowanie Digest); z aparatem AirSense jeszcze nie.
