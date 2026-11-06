# ADR-0008: Postać budzika i biblioteka scen

| | |
|---|---|
| Data | 22.10.2026 |
| Status | przyjęta |
| Źródło | docs/design/kontrola-jakosci/kontrola-jakosci-budzik.md |

## Kontekst
Postacie rysowane jako kod SVG zostały ocenione jako słabe. Autor projektu wybrał kierunek „budzik”, a grafiki powstały w generatorze obrazów AI.

## Decyzja
Postać: oryginalny budzik w stylu kreskówki z lat 30. (bez podobieństwa do znanych postaci). Biblioteka 68 scen 1-bit 800×480, przypisanych do ekranów, w tym 18 przerywników. Kontrola jakości: 2 przebiegi, 63 z 68 gotowe, 5 z szerokim kadrem (przerywniki).

## Konsekwencje
Obrazy z AI mogą nie podlegać ochronie prawnoautorskiej (istotne przy open source); autor zastrzega prawa w zakresie, w jakim mu przysługują (NOTICE). Bez liczb i napisów w obrazkach.

## Adnotacja 01.11.2026
Bez zmiany decyzji: biblioteka ma 70 scen (dodane B04 i B05 dla braku danych S5, grafiki autora projektu; kontrola jakości w dodatku do [kontroli jakości](../design/kontrola-jakosci/kontrola-jakosci-budzik.md)). Przydział 01.11.2026 (też bez zmiany decyzji): B04 zostaje na S5, B05 przechodzi na nowy ekran S10 „Synchronizuję się” (R-EKR-57); S2b bez medalu M01 (D02, D05, D07, D08), medal tylko przy kamieniach milowych.

## Zmiana 04.11.2026 (bez zmiany decyzji)
Biblioteka ma 70 scen (68 z decyzji oraz B04 i B05). 18 przerywników G01–G18 ma komplet tekstów (spec 5.7.6), a w wersji beta są losowane (04.11.2026): sceny z konkretną datą (G01, G04…G08) w swoim dniu, G02 w poniedziałek i G03 w piątek tylko kandydatami we wspólnej puli z G09–G18, scena losowana z ziarnem z daty, limity R-HUM-10 i R-HUM-14 bez zmian.
Uzupełnienie: przy aktywnym wzorcu B przerywnika nie ma (R-HUM-26), nowy przerywnik tylko w oknie porannym 05:00–11:00 (R-HUM-14), etykieta puli ogólnej `Na dzień dobry`. Przerywniki są włączone.
