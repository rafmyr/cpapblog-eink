# ADR-0043: Belka informacyjna na ekranach z danymi i w kokpicie

| | |
|---|---|
| Data | 14.11.2026 |
| Status | przyjęta 14.11.2026 (decyzja właściciela projektu: „wdrażaj belkę”, po konsultacji ze specjalistą UX/UI i akceptacji makiet) |
| Źródło | uwagi testera; rozmowa z właścicielem projektu 14.11.2026 i makiety belki; spec 5.4.17, R-EKR-63…R-EKR-66, R-KON-69; zmienia [ADR-0007](0007-dwa-tryby-ekranu.md), [ADR-0009](0009-anatomia-karty.md), dotyka [ADR-0027](0027-ekran-dzienny.md) i [ADR-0041](0041-krotszy-kokpit.md) |

## Kontekst

Każdy rodzaj ekranu miał własny nagłówek. Ekran dnia: data po lewej, `dane {hh:mm}` po prawej, linia na y 52. Karta z budzikiem: to samo mniejszym krojem, linia na y 46, w tonie „problem” czarny pasek. K1: `Ostatnia noc · {data} · dane {hh:mm}` po lewej i `ocena … · 1 / 3` po prawej. K2: linia na y 44. U-05: własne kroje 20 px.

Skutki z uwag testera i z przeglądu:

- tester nie umiał powiedzieć, czy panel ma już nowe dane, i uznał ekran za mało intuicyjny;
- trzy kwadraty proponowane jako wskaźnik strony właściciel projektu, oglądając makiety na telefonie, wziął za wskaźnik baterii;
- stan baterii panelu było widać tylko na ekranie testu U-05 (trzy naciśnięcia od ekranu dnia) albo dopiero przy linii E-05 od 15%;
- informacja „czego dotyczą dane” stała raz po lewej, raz po prawej.

Konsultacja ze specjalistą UX/UI: jedno stałe miejsce na „gdzie jestem” i jedno na „stan panelu”, w tym samym położeniu na każdym ekranie; bez liczb nocy w tym miejscu, bo dublują treść ekranu, a panel przy łóżku widzą domownicy.

## Decyzja

1. **Belka informacyjna** zastępuje nagłówek na ekranach z danymi, kartach, pytaniach, w kokpicie i na ekranach serwisowych: jasny pas 800 × 44 px, linia 3 px na y 41…43, tekst na linii bazowej 30. Trzy pola o stałej funkcji: lewe (nazwa ekranu, w kokpicie z numerem strony: `Ostatnia noc 1 / 3`), środek (czego dotyczą dane: data nocy, `odczyt {hh:mm}` z godziną importu nocy, `ocena częściowa` / `ocena ograniczona`), prawe (bateria panelu: ikona i procent, przed nią krótki tekst stanu, np. `jest aktualizacja ·`). Zawartość pól na każdym ekranie: spec 5.4.17.
2. **Bez czasu z maską i AHI w belce** i bez wskaźnika strony z kwadratów. Przy godzinie słowo `odczyt` zamiast `dane`.
3. **Belka jest jasna na każdym ekranie**, także nad negatywem (S3, ekran L, S-K w tonie „problem”) i na karcie w tonie „problem”. Budzik nadal nigdy na czarnym tle (ADR-0009).
4. **Karta z budzikiem dostaje belkę** z nazwą `Poranek`, datą i baterią. To zmienia granicę „ekran danych albo karta z budzikiem, bez mieszania” (ADR-0007) tylko o stan panelu: w belce karty nie ma danych terapii, a jedyny dopisek o nocy to `ocena ograniczona` albo `dane z {data}`, które karta miała już w dawnym nagłówku.
5. **Slot dzienny i ekran nocny S9 bez belki**, jak dotąd. Jedyna zmiana: przy baterii poniżej 20% `[PRÓG]` stopka ekranu slotu dziennego pokazuje w miejscu wyniku nocy ikonę baterii i `{x}% · podłącz kabel` (R-KON-69). S9 bez zmian (ostrzeżenie E-05 jak dotąd).
6. **Bateria bez kosztu energii.** Procent to M-25 z ostatniego odczytu napięcia, bez dodatkowych wybudzeń. Dopóki stoi ta sama klatka, belka rysuje stan baterii z tej klatki, więc zmiana procentu sama nie odświeża e-ink. Na zasilaniu zewnętrznym (R-DANE-65) słowo `kabel` zamiast procentu; procent nieznany: `–`.
7. **Kolejność ustępowania środka** przy braku miejsca: dzień tygodnia, potem data (przy nocy ubiegłej wynika z nazwy ekranu), na końcu godzina odczytu; w teście wszystkich dat, godzin, ocen i stanów baterii godzina zostaje zawsze. `ocena częściowa`, `ocena ograniczona`, `dane z {data}`, `noc {data}` i romb nie ustępują nigdy (ADR-0029 pkt 7).
8. **Jedno miejsce w kodzie:** `src/ekrany.cpp` `belka()`; wszystkie ekrany z niej korzystają, stałe geometrii nazwane w `src/ekrany.h`.

## Konsekwencje

- Informacje, które zmieniły miejsce: numer strony kokpitu (z prawej do nazwy po lewej), `ocena …` w K1 (z prawej do środka), nazwa „Otwarte sprawy” i data (osobne pola), `dane z {data}` i `ocena ograniczona` ekranu dnia (z prawej do środka), dopisek `· wieczór` (nazwa `Wieczór`), `raport gotowy` i `Wi-Fi włączone` (środek).
- Ekran dnia pokazuje w belce `odczyt {hh:mm}` z godziną importu nocy (ta sama co w K1 i K2, stała w ciągu dnia) zamiast dawnej godziny ostatniego odczytu AirBridge (`dane {hh:mm}`), która przesuwała się z każdym odczytem; ostatnie pobranie pokazuje U-05.
- Pełna nazwa dnia i miesiąca na pytaniu porannym (`poniedziałek, 30 listopada`) i zakres K2 słownie (`6–12 listopada`) mają postać skróconą (`pn 30.11`, `6–12.11`).
- Karta w tonie „problem” nie ma czarnego paska nagłówka; ton niesie etykieta w czarnym polu. Biała ramka ekranu L zaczyna się pod belką.
- Ekran U-05 ma belkę zamiast nagłówka z kanwy (kroje 24 i 22 px zamiast 20 px, linia na 41 zamiast 48).
- Treść pod belką zostaje na swoich miejscach: tam, gdzie nagłówek kończył się linią na y 52, pod belką jest 10 px więcej wolnego miejsca; K2 bez zmian (linia była na y 44).
- Procent w belce może być nieaktualny do następnego odświeżenia e-ink (najdłużej 24 h), a słowo `kabel` pojawia się przy następnym odświeżeniu. Przejście progu „niska bateria” dodaje najwyżej jedno odświeżenie.
- W K1 przy `ocena ograniczona` data ustępuje przy części dat, godzina zostaje zawsze.
- Ekrany slotu wieczornego i tygodniowego mają nazwę `Wieczór`; ekran dnia trzymany do wieczora zostaje z nazwą `Poranek`.
- Nowe pola w pamięci RTC: stan baterii ostatniej klatki i stan „niska bateria” (`StanRtc::klatkaBatProc`, `klatkaBatFlagi`, `bateriaNiska`).

## Odrzucone warianty

- **Trzy kwadraty jako wskaźnik strony:** właściciel projektu wziął je na makiecie za wskaźnik baterii.
- **Czas z maską i AHI w belce:** dublują treść ekranu i są widoczne dla domowników na każdym ekranie.
- **Belka także na slocie dziennym i S9:** obrazek dnia i scena nocna to ekrany bez danych; belka zabrałaby im 44 px i dodała odświeżenia.
- **Belka w kolorze ekranu (czarna na negatywie):** położenie i wygląd stanu panelu mają być stałe; czarna belka zlewa się z ekranem „problem”.
- **Odświeżanie e-ink przy każdej zmianie procentu:** do kilkunastu odświeżeń dziennie więcej bez nowej informacji o terapii.

## Zmiana 2026-11-14 (po przeglądzie)

- Godzina odczytu wraca na ekran dnia i Portret (` · odczyt {hh:mm}`, godzina importu nocy).
- Kolejność ustępowania: dzień tygodnia → data → godzina.
- `Wieczór` na wszystkich ekranach slotu wieczornego i tygodniowego (`src/kontekst_sk.cpp` `nazwaBelki()`), nie tylko S-K-01 i S-K-02.
- Stan „nie wiem” (zasilanie nieznane, brak pomiaru napięcia) nie zmienia stanu „niska bateria” ani stanu kabla i nie odświeża e-ink.
- Ostrzeżenie w stopce slotu dziennego tylko w paśmie między progiem linii E-05 (15%) a 20%; przy aktywnym E-05 stopka zostaje z wynikiem nocy.
- U-05: belka i wiersz `Bateria` pokazują to samo; tryb awaryjny pokazuje `–`.
- Punkt 5 decyzji czytać z tym pasmem; przykład `{x}% · podłącz kabel` to np. `18%`.

## Zmiana 2026-11-14 (ekrany serwisowe)

- U-05: środek belki `sprawdzono {DD.MM hh:mm}` (bez zegara pusty) zamiast `Ustawienia → Test sprzętu`; na kablu belka `kabel`, a wiersz `Bateria` `na zasilaniu · {x}%` (procent z pomiaru zostaje na ekranie, po przeglądzie; bez pomiaru sam `na zasilaniu`; do 14.11.2026 procent w obu miejscach); bez pomiaru belka `–`, wiersz `brak odczytu`. Zasada „belka i wiersz pokazują to samo” zostaje.
- U-07: środek belki ze stanem (`jest nowa wersja`, `krok 1 z 4`, `gotowe`, `nie udało się` …), pkt 1 decyzji (środek mówi, czego dotyczy ekran).

## Zmiana 2026-11-14 (znaki stanu danych)

Decyzja 6 właściciela projektu 14.11.2026 („aby człowiek nie musiał myśleć, czy dane są OK, tylko widział od razu”; według projektu ekranu). Spec 5.4.17 „Znaki stanu danych”, R-EKR-67…R-EKR-69.

- **Dane nieaktualne (R-EKR-67):** na belkach ekranów o nocy (ekran dnia z kartą, pytaniami i potwierdzeniami, Portret, K1, K2) odwrócone pole `! dane z {data}` (bez historii `! brak danych`), biały tekst na czarnym, jedyne czarne pole belki. Próg z decyzji: „brak ostatniej nocy po zamknięciu okna porannego”, bez nowej liczby: koniec okna 11:00 [PRÓG] (R-DANE-19, R-DANE-20); przed 11:00 oczekiwana jest noc z wczoraj. Pole tylko bez udanego odczytu AirBridge po 11:00 dnia nocy oczekiwanej. Noc bez terapii przy działającym AirBridge (E-10) to nie dane nieaktualne: zostaje `dane z {data}` bez pola i linia E-10. Pkt 2 decyzji („belka jasna”) bez zmian: pole stoi na jasnej belce także nad negatywem.
- **Pobieram (R-EKR-68):** (zmienione po przeglądzie, niżej) klepsydra 14 × 20 px i `pobieram` w środku, gdy panel pobiera noc zaraz po narysowaniu klatki (tylko R-PRZ-22); po pobraniu bez nowej nocy na ekranie dnia panel przerysowuje ekran bez klepsydry (jedno odświeżenie e-ink więcej na takie pobranie).
- **Nowa wersja (R-EKR-69):** koło ze strzałką w dół w prawym polu przed baterią na każdym ekranie z belką poza U-07 i ekranem L z pilnością A albo B; zastępuje tekst `jest aktualizacja ·` na U-01 i ikonę w stopce na ekranach z belką (R-EKR-61: ikona w stopce zostaje na ekranach slotu dziennego bez belki). Pkt 1 decyzji (prawe pole: stan panelu) bez zmian.
- **Ustępowanie (pkt 7):** pole, romb, ocena i godzina odczytu nie ustępują; przy braku miejsca kolejno słowo `pobieram`, środek według pkt 7, znak nowej wersji, klepsydra (tylko K1 z rombem albo oceną). Pole ma 6 px wcięcia i 10 px odstępu, żeby `! dane z 03.03` z `◆ · ocena ograniczona` zmieściło się w K1 przy kablu.
- Konsekwencja: stan pokazany wyjątkiem, przy świeżych danych belka bez zmian; bez nowych pól w RTC i NVS.

## Zmiana 2026-11-14 (znaki stanu danych po przeglądzie)

- **Dane nieaktualne:** wyjątek „bez pola” tylko wtedy, gdy panel naprawdę zna stan nocy oczekiwanej: po 11:00 jej dnia odczytał listę nocy bez zaległych dób nowszych od ostatniej zapisanej nocy, a noc oczekiwana jest nieobecna (bez terapii) albo w budowie; albo po 11:00 terapia trwa (noc w budowie) przy znanym stanie nocy z wczoraj. Sam `/api/status` nie wystarcza: lista urwana, E-03, nieudany raport nocy i zaległe doby dają pole. Chwila poznania w osobnym polu `StanRtc::nocZnanaEp` (kopia NVS `cpap`/`ab_znn`, zapis raz na noc), nie w `StanAB::pobrano`; chwila z przyszłości liczy się jak brak. Poprawia zdanie „bez nowych pól w RTC i NVS” wyżej: jedno pole w RTC i jeden klucz NVS.
- **Pobieram (wariant oszczędny):** żadnego odświeżenia e-ink tylko dla klepsydry, a klepsydra nie może zostać na e-inku po pobraniu. Odświeżenie po pobraniu po przycisku przychodzi tylko przy nowej nocy, czego panel przed pobraniem nie wie, więc dziś klepsydry nie widać w żadnym przebiegu; odświeżenia jak przed częścią C (◀ bez akcji 0, sześć ◀ co 125 s 0, ▶ → K1 1). Rysunek 18 × 22 px z prostą szyjką zostaje na przebieg z pewnym odświeżeniem po pobraniu.
- **Portret:** przy polu nieaktualnych bez `noc na {dzień} {data}`.
- **Strona „Sypialnia · 30 nocy” (ADR-0044):** znak nowej wersji tak, pole nieaktualnych nie (strona opisuje sypialnię). Znak wersji na ciasnej belce K1 ustępuje (zostaje).
