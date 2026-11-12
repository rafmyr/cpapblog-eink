# Portret nocy: wzorzec rysunku

Generator w Pythonie, który z plików sesji aparatu (`PLD`, `EVE`) robi przebieg ostatniej nocy w odcinkach 5 min (plik `/dane/portret.csv`, T-DANE-38) i obrazek Portretu 800×480, 1 bit (spec 5.4.16, ADR-0040). Jest wzorcem dla kodu panelu: odcinki z `firmware/prototyp/src/portret.cpp` porównuje z nim T-TEST-414, obrazek z `firmware/prototyp/src/portret_ekran.cpp` T-TEST-415 (w tolerancji pikseli).

## Pliki

| Plik | Co robi |
|---|---|
| [`przebieg.py`](przebieg.py) | odcinki 5 min, sesje, zdarzenia i braki pomiaru z plików jednej doby; zapis i odczyt `portret.csv` (wersja 1) |
| [`rysunek.py`](rysunek.py) | tarcza (test biegunowy w liczbach zmiennoprzecinkowych), znacznik startu okna `rysuj_znacznik()` (kreska przez pierścień i grot zgodnie ze wskazówkami zegara; od 12.11.2026 zamiast napisu zakresu okna w środku tarczy, R-POR-06; przy wycieku w pierwszym albo ostatnim odcinku okna kreska omija tor pierścienia), podpisy, legenda, kolumna; `Obraz` z zapisem PBM i PNG |
| [`kroje.py`](kroje.py) | kroje panelu czytane z `firmware/prototyp/include/fonty.h` (litery piksel w piksel jak na panelu) |
| [`portret.py`](portret.py) | wiersz poleceń: karta → `portret-RRRR-MM-DD.{csv,pbm,png}` |
| [`karta_portretu.py`](karta_portretu.py) | syntetyczna karta ze znanymi nocami (8 dób, opis w nagłówku pliku) |
| [`test_portret.py`](test_portret.py) | T-TEST-413 (43 testy): pik w znanej godzinie, biały odcinek w znanym przedziale, pusta noc, braki pomiaru, ASV bez normy PS, limity tekstów, okno na najdłuższej sesji, centralne ponad 500 zdarzeń, legenda wycieku, wyciek z jednego źródła (rozproszony bez białego odcinka, 60 s → `Wyciek przez 1 min`), noc niepewna (jeden komunikat), braki pomiaru po wszystkich kombinacjach (podpis zbiorczy `Nieznane: {lista}.`), maksimum nieznane, odcinek dzielony przez dwa pliki, środek tarczy bez napisu okna i znacznik startu okna pod znanym kątem (okna 21:00, 21:30, 23:20, 00:00; na śladzie i na pierścieniu; każdy start okna w strefie tarczy), wyciek na starcie okna widoczny w nietkniętym obrysie (288 startów × 2 skale), legenda wycieku zgodna z rysunkiem (noce losowe), szerokość kreski 4–5 px |

Pillow tylko do zapisu PNG (bez niego zostaje PBM). Reszta: biblioteka standardowa i `tools/kalibracja/` (`edf.py`, `noce.py`).

## Użycie

```
# syntetyczna karta i wszystkie jej Portrety (wolno w repozytorium tylko z --syntetyczna)
python3 -I tools/portret/karta_portretu.py /tmp/karta-portretu
python3 -I tools/portret/portret.py /tmp/karta-portretu --wszystkie --wyjscie /tmp/portrety --norma-ps 5.0

# prawdziwa karta albo pliki z AirBridge ułożone jak karta (KATALOG/STR.edf, KATALOG/DATALOG/RRRRMMDD/): wynik poza repo
python3 -I tools/portret/portret.py /sciezka/do/kopii-karty --rano RRRR-MM-DD \
    --wyjscie /sciezka/poza/repozytorium
```

Opcje: `--zwykla hh:mm` (zwykła pora startu; domyślnie mediana M-18 z 14 wcześniejszych nocy, co najmniej 3), `--norma-ps` (mediana normy M-28 w ASV; domyślnie z 30 wcześniejszych nocy ważnych, co najmniej 14, inaczej „uczę się”), `--pierwszy` (zdanie o normie; narzędzie nie liczy reguł S2a), `--linia` (linia uwagi: tarcza 0,85, najwyżej 2 podpisy).

Testy: `python3 -m pytest -q tools/portret`.

## Definicje (te same w `src/portret.h`)

Odcinek 5 min na siatce od 12:00 dnia `day` (minuty od południa, jak `m18`); odcinek liczy się do pików, wycieku i maksimum od 30 poprawnych próbek ciśnienia. P95 wycieku i mediana PS metodą najbliższej rangi; P95 zapisany w L/min w dół do całości, PS w 0,1 cmH2O (zaokrąglenie połówki w górę). Czas wycieku z próbek ≥ 15 L/min (decyzja właściciela projektu 11.11.2026). Biały odcinek od 60 s takich próbek w odcinku, który liczy się do wycieku (pole `wyc_s`, `rysunek.bialy_odcinek()`); podpis `Wyciek przez {n} min` sumuje tylko odcinki białe na rysunku, legenda tym samym warunkiem (`rysunek.bialy_na_rysunku()`; od 12.11.2026). Ciśnienie na maksimum: minuty z próbką `Press` ≥ MPA − 0,5 (AutoSet) albo `EprPress` ≥ EAX − 0,5 (ASVAuto, decyzja właściciela projektu 11.11.2026). Zdarzenia z `EVE` w czasie zapisu sesji: na zewnątrz OA, `Apnea`, spłycenie; do środka CA; RERA nie. Braki pomiaru: `L`, `P`, `E`, `R` i `M` (AutoSet albo ASVAuto bez maksimum z ustawień, `Maksimum ciśnienia: nieznane.`). Okno tarczy obejmuje całą najdłuższą sesję (od jej startu minus 2 h, gdy nie mieści się w oknie od zwykłej pory). Po nocy niepewnej pierwszy podpis `Tej nocy nie oceniam: duży wyciek.`, a liczby nocy to sam `{czas} z maską`. Podpisów o brakującym pomiarze więcej niż miejsc: nadmiar w podpisie zbiorczym `Nieznane: {lista}.` z nazwami, w kolumnie najwyżej 2 linie (`rysunek.podpisy_brakow()`, R-POR-27). Zakresy pliku: sesja `od` do 1440, minuta zdarzenia 0–1439, `wyc_s` ≤ 300 (te same w `src/portret.h`).

## Czego narzędzie nie robi

Nie liczy reguł zdania S2a ani normy panelu (pierwszy podpis i norma PS z opcji albo przybliżenia); nagłówek i stopka obrazka są uproszczone (porównanie z panelem dotyczy strefy treści y 54–434).
