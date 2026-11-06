# Specyfikacje i dane reguł

Ten katalog opisuje, co panel robi i jak jest zbudowany. Wersja specyfikacji w repozytorium to 1.2 (opisuje wydanie `1.0.0-beta.1`). Wcześniejsze wersje nie są częścią publicznego repozytorium.

| Plik | Co to jest | Kto go zmienia |
|---|---|---|
| [specyfikacja-funkcjonalna.md](specyfikacja-funkcjonalna.md) | co panel pokazuje i robi: słownik, dane i norma osobista, wymagania w notacji EARS z kolumną „Weryfikacja”, katalog ekranów, pierwszeństwo ekranów, kryteria wydania (WY), załączniki | ręcznie |
| [specyfikacja-techniczna.md](specyfikacja-techniczna.md) | jak panel jest zbudowany: komponenty firmware (`T-KOMP`), interfejsy, dane trwałe, wymagania techniczne, budżety, testy, licencje składników obcych, wydawanie | ręcznie |
| [macierz-sledzenia.csv](macierz-sledzenia.csv) | które wymaganie odwołuje się do których ID, ekranów i przypadków testowych | generowany przez `python3 tools/spojnosc.py` |
| [macierz-techniczna.csv](macierz-techniczna.csv) | które wymaganie ma komponent firmware i test | generowany przez `python3 tools/spojnosc_tech.py --macierz docs/spec/macierz-techniczna.csv` |
| [progi.csv](progi.csv) | wartości progów używane przez reguły (kolumny: `id_progu`, `regula`, `wartosc`, `jednostka`, `opis`, `zrodlo`, `tryb`, `status` …) | ręcznie; wkompilowane w firmware przez `tools/zestaw/buduj_zestaw.py` |
| [progi-poza-zakresem.csv](progi-poza-zakresem.csv) | liczby oznaczone `[PRÓG]`, które nie dotyczą oceny nocy (czas interfejsu, limit tekstu, parametr techniczny) oraz wpisy, które nie są progami | ręcznie |
| [katalog/](katalog/) | katalog reguł jako dane (zdarzenia E, wzorce L, hipotezy H, odkrycia O, ekrany S-K) i teksty ekranów ([ADR-0023](../decisions/0023-silnik-regul-sterowany-danymi.md)) | ręcznie; wkompilowane tym samym skryptem |

Znaczniki w tekście:

| Znacznik | Znaczenie |
|---|---|
| `[PRÓG]` | liczba bez źródła albo do kalibracji; nie jest ustaloną wartością |
| `[DANE?]` | pole albo sygnał, którego obecność w danych trzeba potwierdzić |
| `[do zmierzenia]` | wartość technicznie mierzalna, z numerem pomiaru z planu pomiarów (spec techniczna 2.4) |
| `[szacunek]` | liczba z rachunku, nie z pomiaru |

Identyfikatory są stabilne: `R-…` wymagania, `M-…` metryki, `E-…` zdarzenia, `L-…` wzorce do rozmowy z lekarzem, `H-…` hipotezy, `O-…` odkrycia, `S-K-…` ekrany kontekstowe, `T-…` elementy specyfikacji technicznej. Identyfikator nie jest używany ponownie; wycofane trafiają do `tools/wycofane.csv`. Przy odwołaniach `v0.6c` oznacza wcześniejszą, nieopublikowaną wersję specyfikacji; służy tylko do wskazania pochodzenia wymagania.

Wersję specyfikacji funkcjonalnej do czytania (bez kolumn „Źródło” i dopisków o przeglądach) generuje `python3 tools/wersja_do_czytania.py`; wynik trafia do `build/` i nie jest przechowywany w repozytorium.

Sprawdzenia: `python3 tools/spojnosc.py --strict` (ID, notacja EARS, ekrany, długości tekstów), `python3 tools/spojnosc_tech.py` (spec techniczna i odwołania do kodu), `python3 -m pytest -q tools`. Decyzje projektowe: [docs/decisions/](../decisions/README.md).
