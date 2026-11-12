# ADR-0041: Krótszy kokpit: trzy strony, „Otwarte sprawy” tylko przy sprawie

| | |
|---|---|
| Data | 11.11.2026 |
| Status | przyjęta 11.11.2026 (decyzja właściciela projektu 11.11.2026); zmienia regułę kokpitu ze spec 5.6.1 pkt 2 (decyzja właściciela projektu 05.11.2026) bez zmiany samej reguły przycisków; kod razem z Portretem nocy ([ADR-0040](0040-portret-nocy.md)). Zmiana 2026-11-11: zrobione w kodzie: kokpit `n / 3` (K1, K2, „Co znaczą te liczby” i „Otwarte sprawy” bez numeru, U-01), S8 i „Sprzęt” poza kokpitem, nagłówek K1 z R-KOK-21 liczony ze stanów po ocenie silnika, `WejscieSpraw::ocenaPelna` ustawiane z tego samego stanu, formularz z terminem i polem `Nowy model maski od dziś`, ◀ 2 s także na S-K-35, wycofane ID R-EKR-35, R-EKR-50, R-MAS-11, R-MAS-14, R-MAS-15 (spec 10.2, `tools/wycofane.csv`) |
| Źródło | decyzja właściciela projektu 11.11.2026; spec 5.4.8 (S8), 5.4.15 („Otwarte sprawy”), 5.5, 5.6.1, 5.9.3 („Sprzęt”); [ADR-0029](0029-zakres-skierowan-w-wydaniu-1-0.md) pkt 7; [ADR-0011](0011-konfiguracja-przyciski-i-formularz-qr.md) (formularz) |

## Kontekst

Od 05.11.2026 kokpit to ciąg sześciu stron `n / 6`: K1, K2, „Otwarte sprawy”, S8 „Twoja norma”, „Sprzęt”, „Ustawienia” U-01. Przy Portrecie nocy jako pierwszym ekranie poranka właściciel projektu skrócił kokpit (11.11.2026): norma ma pasma w K2, daty wymiany mają formularz na telefonie, a pusta strona „Otwarte sprawy” mówi głównie, że nic nie jest otwarte.

Sprawdzenie specyfikacji pokazało trzy skutki do obsłużenia, zanim cokolwiek zniknie:

- S8 ma informacje, których kolumna `norma` w K2 nie ma: krok przy wysokim paśmie AHI (`→ Pasmo AHI wysokie. Obserwuj kolejne noce.`), ton „informacja”, a w ASVAuto wiersz `Bezdechy {P10}–{P90} / h` (M-30), którego K2 w tym trybie nie pokazuje.
- Wiersz `Ocena: częściowa` / `Części rzeczy jeszcze nie oceniam.` stoi tylko na pustym ekranie „Otwarte sprawy” (R-EKR-50); w kodzie bety ocena jest zawsze „częściowa”, bo nic nie ustawia `WejscieSpraw::ocenaPelna`. Ukrycie pustej strony usunęłoby jedyne miejsce, w którym panel to mówi (ADR-0029 pkt 7).
- Formularz U-03 ma daty wymiany maski i filtra oraz okresy, ale nie ma „nowego modelu maski”; S-K-35 (ekran dzienny „Wymiana”) prowadzi krokiem do strony „Sprzęt” (`→ Po wymianie: strona 5 / 6, ramka na {elemencie}, ◀ 2 s.`).

## Decyzja

1. **Ciąg stron kokpitu:** ekran dnia ▶ K1 (`1 / 3`) ▶ K2 (`2 / 3`) ▶ „Otwarte sprawy” (tylko przy co najmniej jednej sprawie z listy R-EKR-48, bez numeru strony) ▶ „Ustawienia” U-01 (`3 / 3`); z U-01 jak dotąd ▶ 2 s formularz U-03 i test sprzętu U-05. Numer `n / 3` stały, jak w decyzji właściciela projektu; wariant z numerem `n / 4` przy sprawach zostaje do decyzji. Reguła przycisków (▶ dalej, ◀ wstecz, ↻ ekran dnia, przytrzymanie tylko zapisuje, kolejność stopki `◀ ▶ ↻`) bez zmian. Wyjątek z ADR-0040: w dni z ekranem problemu Portret nocy stoi przed K1 jako strona bez numeru (R-POR-24).
2. **S8 „Twoja norma” wychodzi z kokpitu**, zostaje kamieniem milowym R7 po 30 nocach ważnych (R-EKR-33). Pasma normy zostają w kolumnie `norma` K2. Na żądanie przestają być dostępne: krok o wysokim paśmie AHI (`→ Pasmo AHI wysokie. Obserwuj kolejne noce.`) i ton „informacja” S8, a w ASVAuto pasmo wszystkich bezdechów (M-30), którego K2 w tym trybie nie ma (wiersze `pełne wsparcie` i `maks. EPAP`). To bezpośredni skutek decyzji, wypisany do decyzji właściciela projektu z kosztem dołożenia do K2; do tej decyzji spec przyjmuje decyzję bez dodatków.
3. **„Sprzęt” wychodzi z kokpitu.** Daty wymiany i okresy zostają w formularzu U-03 (są), dochodzi pole „nowy model maski” (R-MAS-16) i termin następnej wymiany przy każdym elemencie (R-MAS-17). Pytanie `Wymieniona maska?` (R-MAS-12) i „wymienione dziś” przytrzymaniem ◀ na S-K-31 (R-MAS-01) zostają; S-K-35 dostaje ten sam zapis przytrzymaniem ◀ 2 s (R-MAS-18), bo jego krok prowadził do usuniętej strony.
4. **„Otwarte sprawy” tylko przy sprawie.** Stan pusty (R-EKR-50) nie ma już strony; informację o ocenie częściowej albo ograniczonej panel pokazuje w prawej części nagłówka K1 przed numerem strony (`ocena częściowa · 1 / 3`, R-KOK-21), żeby ukrycie strony nie ukryło, że panel czegoś nie ocenia (ADR-0029 pkt 7).
5. **„Co znaczą te liczby”** (profil publiczny, dotąd po S8 jako `4 / 6`) stoi po K2, bez numeru strony (R-KOK-22).
6. **Teksty z numerem strony** zmieniają się według tabeli w spec 5.5.7 (R-KOK-23) (np. `→ Otwarte sprawy: strona 3 / 6.` → `→ Otwarte sprawy: za stroną 2 / 3.`, `Na kontroli: raport 2 / 6.` → `Na kontroli: raport 2 / 3.`, `Ustawienia: strona 6 / 6.` → `Ustawienia: strona 3 / 3.`), razem z `docs/spec/katalog/teksty.csv`, zestawem wbudowanym i tekstami w kodzie (`src/dzien.cpp`, `src/slot_dzienny.cpp`).

## Konsekwencje

- Spec funkcjonalna: nowa 5.5.7 z wymaganiami R-KOK-19…R-KOK-23 i R-MAS-16…R-MAS-18; dopiski „Zmiana 2026-11-11 (ADR-0041)” przy 5.4.8, 5.4.15, 5.5.1, 5.6.1 pkt 2 i 3, 5.6.2, 5.9.3, R-EKR-35, R-EKR-36, R-EKR-47, R-EKR-50, R-EKR-51, R-MAS-11, R-MAS-14, R-MAS-15, S-K-35. Wycofanie i przepisanie ID według jednej listy w spec 5.5.7 (do wycofania R-EKR-35, R-EKR-50, R-MAS-11, R-MAS-14, R-MAS-15; do przepisania R-EKR-47, R-EKR-51 i 26 wymagań z numerami stron albo stroną „Sprzęt”) oraz przepisanie tekstów z ` / 6` (11.11.2026: 148 wystąpień w spec funkcjonalnej, w tym tabela 5.5.7 z brzmieniem dziś, 10 w technicznej, 7 w `teksty.csv`, 22 w `firmware/prototyp/README.md`, 93 w `src/`) idą razem z kodem i testami; do tego czasu 5.5.7 ma pierwszeństwo przed starszym brzmieniem.
- Kod: `src/nawigacja.cpp` `stronaPo()`, `stronaPrzed()`, `numerStrony()`, `STRON_KOKPITU`; `KontekstNaw::sprawyDostepne` liczy się z liczby spraw, nie z samego istnienia wyników silnika; usunięcie stron `NORMA`, `SPRZET` z ciągu (ekran S8 zostaje jako ekran dnia); `src/formularz.cpp` pole nowego modelu; testy `test/testy_nawigacja*.cpp`.
- Mniej naciśnięć do Ustawień (z 5 do 2 albo 3 ▶ z K1); mniej informacji na żądanie: pasma normy tylko w K2 (bez kroku pasma i bez M-30 w ASVAuto), terminy wymiany tylko na telefonie i w ekranie dziennym.
- Zmiana 2026-11-11 (decyzja właściciela projektu): krok S8 przy wysokim paśmie AHI wraca na żądanie jako linia pod legendą K2 w CPAP i AutoSet (R-KOK-24). W ASV i ASVAuto nie ma miejsca (brakuje 26 px), więc tam krok i pasmo M-30 nadal tylko raz na S8.
- Zmiana 2026-11-11: cofnięcie nowego modelu maski zostaje, tylko inną drogą: pole formularza `Cofnij nowy model z dziś` w dniu zapisu (R-MAS-19) zamiast ekranu `Nowy model maski zapisany.` z ◀ 2 s przez 30 s (dawne R-MAS-14). Koszt: pomyłkę cofa się tylko telefonem i tylko tego samego dnia (następnego dnia norma M-07 już się uczy od nowa, a datę maski poprawia się polem daty); bez telefonu pomyłka nieodwracalna do 14 nocy nauki normy. Stopka S-K-35 taka jak S-K-31 (`◀ 2 s: wymienione dziś` · `▶ kokpit`).
- Zapis wymiany filtra bez telefonu: tylko S-K-35, S-K-31 albo pytanie; dziś także kursor na stronie „Sprzęt” (koszt przyjęty, do oceny po 14 nocach).
- Wymaga zmiany w instrukcji obsługi (kartka z przyciskami), w `firmware/prototyp/README.md` i w spec technicznej (T-KOMP-10, T-KOMP-11).

## Odrzucone warianty

- **Zostawić pustą stronę „Otwarte sprawy”** jako nośnik `Ocena: częściowa`: wbrew decyzji; w becie strona byłaby widoczna zawsze, bo ocena jest zawsze częściowa.
- **Przenieść S8 i „Sprzęt” za Ustawienia** (`4 / 6`, `5 / 6` po U-01): nie skraca drogi do informacji, którą właściciel projektu chce mieć z przodu, i miesza ekrany serwisowe z danymi.

## Zmiana 2026-11-11: strona Portretu pod ▶

Właściciel projektu zdecydował 11.11.2026: w dni z ekranem problemu z R2–R5d Portret nocy stoi przed K1 jako strona bez numeru (R-POR-24), więc pkt 1 obowiązuje jako decyzja, nie propozycja. Kod: `Ekran::PORTRET` w `src/nawigacja.h`, przejścia strony i ◀ z K1 otwartego z niej w `src/dyrygent.cpp` `przejsciePortretu()`, flaga ekranu problemu z Portretem w `StanRtc::portretStrona`; numeracja `n / 3` jej nie dotyczy.
