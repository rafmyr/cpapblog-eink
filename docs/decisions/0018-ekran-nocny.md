# ADR-0018: Ekran nocny „Dobranoc” w rotacji trzech wariantów

| | |
|---|---|
| Data | 30.10.2026 |
| Status | przyjęta (decyzja właściciela projektu 30.10.2026); treść ekranu i zdania budzika do akceptacji; wyjątek od ADR-0007 i ADR-0009 dla wariantów A2 i A3 |
| Źródło | decyzja 30.10.2026; spec 5.8.11, R-KON-31…R-KON-40, R-KON-02, R-EKR-37 |

## Kontekst
Do tej pory ekran wybrany w slocie wieczornym (albo ekran z rana) stał od 23:30 do 05:00 bez zmian, bo w nocy panel nie odświeża się sam, żeby nie błyskać w ciemnej sypialni (R-KON-02, R-PRZ-18). Wieczorna wskazówka typu „Przewietrz sypialnię” albo ekran z rana z liczbami ostatniej nocy wisiały więc przez całą noc i rano, choć po założeniu maski nie są już potrzebne. Autor projektu chce, żeby ekran przy łóżku na noc mówił po prostu „dobranoc”, z budzikiem, i żeby było wiadomo, że wynik nocy będzie rano.

Rozważone warianty:

| Wariant | Dlaczego nie |
|---|---|
| duży zegar odświeżany co 30 min | wprowadza w błąd (e-ink pokazuje godzinę sprzed nawet 29 min) i każda zmiana to błysk w ciemnej sypialni; łamie zasadę braku odświeżeń w nocy (R-KON-02) |
| rysowanie ekranu w chwili założenia maski | panel nie wie dziś, kiedy zaczyna się sesja: Załącznik A zna tylko raporty zamkniętych nocy. Do sprawdzenia, czy AirBridge od v2.2.0 podaje start sesji na żywo; jeśli tak, zmiana momentu rysowania idzie nowym wpisem (CHANGELOG, a jeśli zmienia granicę albo budowę ekranu, nowy ADR) |
| jeden stały wygląd | Autor projektu wybrał rotację trzech wariantów, żeby ekran nie opatrzył się po tygodniu |

## Decyzja
1. **Trzeci rodzaj ekranu.** Obok ekranu danych i karty z budzikiem (ADR-0007) jest **ekran nocny** S9 „Dobranoc” z własną budową (spec 5.8.11). Nie jest kartą ani ekranem danych: nie liczy się do limitów kart, scen i zdań (7.4) i nie ma linii uwagi.
2. **Kiedy.** Panel rysuje S9 **raz o 23:30** (początek slotu nocnego, ostatnie odświeżenie dnia), pełnym odświeżeniem. Ekran stoi do pierwszego odświeżenia rano o 05:00, które jak dotąd rysuje ekran z rana (R-EKR-37). Kolejne wybudzenia w nocy niczego nie rysują.
3. **Kiedy nie.** S9 nie zastępuje ekranu, który ma zostać na noc: S5 (brak danych), ekranu lekarskiego L z pilnością A albo B, dnia z niewyciszonym wzorcem A (zasada „pilność A zawsze widoczna”), ekranu otwartego przyciskiem (R-PRZ-18) ani U-04. Wtedy zostaje bieżący ekran, jak przed tą decyzją.
4. **Trzy warianty w rotacji noc po nocy** (numer dnia poranka mod 3), w każdym rotacja scen budzika:
   - **A1 „karta”**: budowa karty z ADR-0009 (budzik 240×205 po prawej, odbity lustrzanie), etykieta `DOBRANOC`, tytuł `Śpij dobrze.`; sceny W12, G14, G11;
   - **A2 „scena”**: duża scena budzika na środku (do ok. 600×340), pod nią `Dobranoc.` i jedna linia faktów; bez nagłówka i stopki; sceny G10, G13, G15;
   - **A3 „godzina”**: wielka godzina założenia maski (serif ok. 140 px), budzik ok. 230×250 po prawej; sceny G12 (nurek w masce CPAP), W12.
5. **Bez godziny maski.** Gdy panel nie zna godziny założenia maski (maska jeszcze niezałożona o 23:30 albo AirBridge nie podaje startu sesji na żywo, np. v1.6.2), S9 nie ma części `maska od`, a A3 pokazuje zamiast godziny wielkie `Dobranoc.` z etykietą `NA DOBRANOC`. Godzina na żywo to `[DANE?]`.
6. **Granice bez zmian.** S9 nie ocenia nocy ani zdrowia i nie mówi nic o ustawieniach aparatu (ADR-0002); fakty to temperatura i wilgotność z czujnika panelu (M-22), godzina założenia maski i mediana czasu terapii z normy (`Twój zwykły czas z maską`, M-01; podpis zmieniony 30.10.2026). Budzik nigdy na czarnym tle (ADR-0009). Przyciski jak na ekranie dnia (↻ i ▶ → K1, ◀ → K2); ekran otwarty przyciskiem nie wraca sam do S9 (R-PRZ-18), a ↻ w K1 wraca do S9 w tej samej treści.

## Wyjątki od wcześniejszych decyzji
- **ADR-0007 (dwa tryby):** dochodzi trzeci rodzaj ekranu, ekran nocny, z budzikiem i z faktami, ale bez oceny nocy. Zasada „budzik nic nie komunikuje” zostaje: usunięcie sceny i zdania budzika nie zmienia informacji na S9.
- **ADR-0009 (anatomia karty):** wariant A2 łamie budowę karty (budzik duży, na środku, bez nagłówka i stopki), a A3 ma budzik w innym rozmiarze (ok. 230×250). Wyjątek dotyczy tylko S9. Zakaz budzika na czarnym tle obowiązuje także na S9.

## Konsekwencje
- Wskazówka wieczorna (np. `Przewietrz sypialnię`) znika o 23:30, więc kto wchodzi do sypialni po 23:30, już jej nie zobaczy (wcześniej stała do rana, 5.8.2). Koszt świadomy: o tej porze zwykle maska jest już założona albo zaraz będzie.
- Jedno dodatkowe pełne odświeżenie na dobę, o 23:30 (energia: R-NF-02; błysk w chwili, gdy zwykle nikt jeszcze nie śpi).
- Zmienione: R-KON-02 (wyjątek dla jednego rysowania o 23:30), R-EKR-37 (o 05:00 także z S9), opis slotu nocnego (1.5, 2.4, 5.8.2, 7.1, 7.2), mapa przycisków (5.6.2). Nowe: S9, R-KON-31…R-KON-40, pula zdań „noc” (5.7.5; przyjęta 08.11.2026).
- Nowe rozmiary bitmap budzika: A2 do 600×340, A3 230×250 (obok `karta-240x205`); do przygotowania przy budowie ekranu. Artboardów trzech wariantów nie ma w makietach (6.7).
- Godzina założenia maski w praktyce nie pojawi się, dopóki AirBridge nie potwierdzi startu sesji na żywo; do tego czasu S9 zawsze w wersji „bez godziny maski”.

Zmiana 2026-11-08: pula „noc” przyjęta przez właściciela projektu 08.11.2026 (5.7.5); S9 ma zdanie sceny także przy `HUMOR_PROPOZYCJE=0` (R-KON-40). Zmiana 2026-11-09: przy humorze `wyłączony` S9 bez sceny i bez zdania także w prototypie (R-KON-39).

## Adnotacja (31.10.2026, przegląd dokumentacji)
Zamknięte 30.10.2026: AirBridge v2.2.1 podaje w `GET /api/status` pole `edf_prefix` (start bieżącej sesji) i `rop` (1 = terapia), więc S9 pokazuje godzinę założenia maski przy `rop` = 1 (spec 5.8.11, R-KON-33, Załącznik A pkt 4). Pkt 5 decyzji i ostatni punkt „Konsekwencji” („S9 zawsze w wersji bez godziny maski”) są przez to nieaktualne. Zmiana momentu rysowania na chwilę założenia maski nadal wymaga osobnej decyzji.

Podpis faktu z pkt 6 od 30.10.2026 (audyt UX, decyzja): `zwykle z maską` zamiast `Twój zwykły czas z maską` (spec 5.8.11). Treść decyzji bez zmian; dopisek przeniesiony 31.10.2026 z pkt 6 do adnotacji.
