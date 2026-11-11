# ADR-0039: Alert rano dla każdego wskaźnika z normą, który wyraźnie odbiega od normy

| | |
|---|---|
| Data | 11.11.2026 |
| Status | przyjęta 11.11.2026 (decyzja właściciela projektu: zakres, warunek, forma, interpretacja, kierunek, teksty); zmienia ADR-0014 pkt 2 dla metryk z kartą |
| Źródło | zgłoszenie właściciela projektu 11.11.2026 („aktywne alerty, gdy jakiś wskaźnik znacząco rośnie albo spada, z krótką interpretacją; główną funkcją panelu ma być informowanie o odchyleniach od normy”); [ADR-0003](0003-norma-osobista.md), [ADR-0014](0014-nowy-poziom-bez-metryk-zdrowotnych.md), [ADR-0002](0002-panel-nie-zmienia-ustawien.md), [ADR-0029](0029-zakres-skierowan-w-wydaniu-1-0.md), [ADR-0037](0037-asv-bez-rodzaju-bezdechu-i-csr.md) |

## Kontekst

Panel ma normę osobistą (ADR-0003, 4.4) i stan odchylenia (4.6: 3 z 5 nocy ważnych poza granicą i co najmniej Δ od mediany). Kartę rano z interpretacją dostaje jednak tylko część wskaźników (AHI E-31, bezdechy przy pełnym wsparciu E-33, EPAP E-36, wyciek E-21, czas z maską E-13, przerwy E-14). Wsparcie PS (M-28) ma tylko ramkę w K2, wentylacja i objętość oddechowa (M-15, M-14) są sygnałem pomocniczym S-K-17 i S-K-30, oddychanie okresowe (M-09) idzie do raportu, a według ADR-0014 pkt 2 metryka zdrowotna w stanie odchylenia daje po 21 nocach tylko raport i linię raz w tygodniu. Właściciel projektu chce, żeby główną funkcją panelu było informowanie o odchyleniach od normy.

## Decyzja

1. **Zakres:** każdy wskaźnik z normą (tabela 4.4), który nie ma dziś własnej karty rano, dostaje kartę: wsparcie PS ↑ (M-28), wentylacja ↑ i ↓ (M-15), objętość oddechowa ↑ i ↓ (M-14), oddechy na minutę ↑ (M-13, gdy tego ranka nie ma S-K-30 ani S-K-17), oddychanie okresowe ↑ (M-09 jako % nocy), ograniczenie przepływu ↑ (M-10 P95). Wskaźniki z istniejącą kartą zostają przy niej.
2. **Warunek:** ten sam co stan odchylenia (4.6, R-NOR-03, R-NOR-12): 3 z 5 ostatnich nocy ważnych poza granicą i co najmniej Δ od mediany normy w danym kierunku. Brak normy (rozruch, zerowanie po zmianie ustawień R-NOR-07): bez karty. Progi Δ i granice nowych kierunków `[PRÓG]`.
3. **Forma:** karta poranna w tonie „informacja” (ekran danych, wykres 14 nocy z linią normy), najwyżej jedna nowa karta tego rodzaju dziennie, pod wszystkimi istniejącymi problemami porannymi w kolejności slotu; gdy stan trwa, ta sama karta wraca po 7 dniach `[PRÓG]`. Krok przez pierwsze 7 nocy stanu: `→ Obserwuj kolejne noce.`, od 7. nocy: `→ Trwa od tygodnia. Omów to z lekarzem.`
4. **Interpretacja:** tytuł, powód z liczbami (dziś i zwykle) i jednym zdaniem o częstych zwykłych przyczynach („Bywa przy…”), bez nazw chorób, bez czynników wrażliwych (alkohol, leki; R-CZY-20), bez wartości i kierunku zmiany ustawień (ADR-0002). Zdania „Bywa przy…” to domysł autora, do przeglądu lekarza PSG w etapie 2. Teksty przyjęte 11.11.2026 są w katalogu tekstów (`docs/spec/katalog/teksty.csv`).
5. **Kierunek:** karta w kierunku niepożądanym; zmiana w dobrą stronę (AHI ↓, PS ↓, wyciek ↓, przerwy ↓, ograniczenie przepływu ↓, czas z maską ↑) jako jedna linia uwagi `Dobra zmiana: {wskaźnik} {niżej|wyżej} niż zwykle od {n} nocy.`, raz na 7 dni `[PRÓG]`.
6. **Zmiana ADR-0014 pkt 2:** dla metryk z kartą z pkt 1 karta co 7 dni zastępuje linię `Od {n} tyg.: … Pokaż lekarzowi.`; dopisywanie do raportu i liczenie od normy sprzed odchylenia bez zmian. ADR-0029 bez zmian: karty z pkt 1 nie są skierowaniami L i nie mają pilności.

## Konsekwencje

- Nowe ekrany S-K-40…S-K-47 (kolejność jak w pkt 1) i linia „dobra zmiana”; nowe progi `[PRÓG]` dla Δ w nowych kierunkach; katalog reguł, teksty, testy, macierze.
- Więcej kart rano przy niestabilnych nocach. Ogranicza to warunek 3 z 5, jedna karta dziennie i powrót co 7 dni.
- M-14 i M-15 istnieją w historii nocy dopiero od zmiany „wentylacja minutowa” (noc z plików EDF); noce z raportu JSON nie mają tych wartości, więc karty M-14 i M-15 działają po zebraniu normy (co najmniej 14 nocy z wartością, R-NOR-10).

## Zmiana 2026-11-11 (wdrożenie)

- Pozycja w tabeli 7.2: nowa grupa R5e między R5d a R6; karta w tonie „informacja” ustępuje slotowi dziennemu jak R6…R9 i nie jest prośbą (7.4). Wymagania R-KON-64…R-KON-68 (spec 5.8.9, 5.8.13).
- Δ według kompletu progów przyjętego 11.11.2026 (decyzja właściciela projektu; pochodzenie: przegląd literatury i obserwacja własna): M-28 ±1 cmH2O, M-28p +2, M-09 (% nocy) +5 pp, M-10 P95 ±0,15, M-14 i M-15 ±15% mediany normy, M-06p +1,5, M-31 +2 / h; dobra zmiana z Δ kierunku przeciwnego. Wszystkie `[PRÓG]`.
- Decyzja właściciela projektu 11.11.2026 w tej samej zmianie: R-NOR-07 zeruje w trybach ASV i ASVAuto także normę M-13, M-14 i M-15, a zerowanie kończy stan odchylenia (inaczej po zmianie ustawień karty S-K-41…S-K-45 stałyby miesiącami).
