# ADR-0001: Całe przetwarzanie na panelu

| | |
|---|---|
| Data | 20.10.2026 |
| Status | przyjęta |
| Źródło | specyfikacja rozdz. 1, R-DANE-01 |

## Kontekst
Autor projektu chce systemu, który działa w domu bez chmury i bez zależności od zewnętrznych usług.

## Decyzja
Wszystkie obliczenia wykonuje panel (ESP32-S3). Dane terapii pobierane są wyłącznie z API AirBridge w sieci lokalnej. Z domu wychodzi tylko zapytanie o pogodę i pyłek (opcjonalne).

## Konsekwencje
Brak kosztów chmury i ryzyka wycieku danych. Ograniczona moc obliczeniowa: ciężkie analizy (Glasgow Index z surowego przepływu) zależą od dostępu do surowego przepływu (pomiar 21 w specyfikacji technicznej). Brak mDNS w AirBridge: adres konfigurowany ręcznie.

**Adnotacja 23.10.2026:** czas z serwera NTP dozwolony ([ADR-0013](0013-wyjatki-czas-sieciowy-i-leki-w-raporcie.md) pkt 1).

**Adnotacja 29.10.2026:** wyjątek w [ADR-0017](0017-import-historii-z-karty-aparatu.md): jednorazowy import historii nocy z kopii karty SD aparatu.

**Adnotacja 31.10.2026:** opcjonalne lokalne źródło danych otoczenia w sieci domowej ([ADR-0019](0019-lokalne-zrodlo-otoczenia.md)): dane otoczenia przychodzą do panelu z sieci lokalnej, panel niczego z danych terapii nie wysyła.

**Adnotacja 01.11.2026:** zdanie „Brak mDNS w AirBridge” jest nieaktualne: AirBridge v2.3.0 ogłasza się jako `airbridge.local`. Panel może rozwiązać adres AirBridge `.local` przez mDNS w sieci lokalnej (R-KONF-40); zalecany pozostaje stały adres IP (rezerwacja w routerze). Nic nie wychodzi poza sieć domową, więc decyzja bez zmian.

**Adnotacja 05.11.2026:** [ADR-0032](0032-aktualizacja-firmware-z-github.md): czwarty dozwolony ruch wychodzący, sprawdzenie i pobranie firmware z wydań GitHub (bez danych terapii i użytkownika).
