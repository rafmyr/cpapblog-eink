# Projekt wizualny

## Źródła ekranów

Źródłem prawdy wyglądu jest zestaw 75 ekranów w 9 sekcjach (szablony, rano, wieczór, niedziela, lekarz, kokpit i pytania, przerywniki, biblioteka). Jego źródła leżą w [`design/kanwa/`](../../design/kanwa/) (pliki `.dc.html` i `canvas.json`). Część z nich to szablony zależne od środowiska edytora, więc poza nim renderują się puste. **Podgląd bez edytora:** [`design/png/`](../../design/png/), 75 obrazów 800×480 w czerni i bieli, jak na panelu; po zmianie źródeł generuje się je od nowa skryptem [`tools/eksport_png.py`](../../tools/eksport_png.py) (`python3 tools/eksport_png.py`, wymaga Node, Playwright z Chromium i Pillow).

## Zasady (szczegóły: specyfikacja, 5.4.3 i 5.7.2)

- Dwa tryby: **ekran danych** (szablon A) i **karta z budzikiem**, bez mieszania ([ADR-0007](../decisions/0007-dwa-tryby-ekranu.md)).
- Karta: tytuł na całą szerokość, budzik 240×205 po prawej, odbity lustrzanie, nigdy na czarnym tle ([ADR-0009](../decisions/0009-anatomia-karty.md)).
- Typografia: Source Serif 4 (tytuły, liczby), Public Sans (tekst); od 30.10.2026 skala B czytelności (spec 5.4.3), bez IBM Plex i bez kursywy; część ekranów źródłowych jest jeszcze w starej typografii, do przerysowania: [czytelnosc-b-przerysowanie.md](czytelnosc-b-przerysowanie.md).
- Tylko czerń i biel (1-bit), tekst min. 18 px; na ekranach dnia, w kokpicie, pytaniu, S9 i kartach min. 22 px.

## Pliki

| Plik | Co to |
|---|---|
| [czytelnosc-b/SPEC-B.md](czytelnosc-b/SPEC-B.md), [czytelnosc-b/SPEC-KOKPIT.md](czytelnosc-b/SPEC-KOKPIT.md), [czytelnosc-b/makiety/](czytelnosc-b/makiety/) | projekt skali B (ekrany dnia, kokpit, pytanie, karty, S9) z 30.10.2026 i wybrane makiety; tam gdzie różni się od spec po audycie UX, wygrywa spec |
| [czytelnosc-b-przerysowanie.md](czytelnosc-b-przerysowanie.md) | które artboardy przerysować po skali B (wariant B ekranu dnia, K2 opcja A) i według których makiet |
| [kontrola-jakosci/kontrola-jakosci-budzik.md](kontrola-jakosci/kontrola-jakosci-budzik.md) | raport kontroli jakości 68 scen (2 przebiegi) |
| [kontrola-jakosci/](kontrola-jakosci/) | plansze kontrolne i porównania rozmiaru, tła i układu karty |
| [../../assets/budzik/zrodlo/](../../assets/budzik/zrodlo/) | 68 scen źródłowych 1-bit 800×480 |
| [../../assets/budzik/karta-240x205/](../../assets/budzik/karta-240x205/) | sceny użyte w kartach, przycięte, odbite i przeskalowane |
