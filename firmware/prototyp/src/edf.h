// SPDX-License-Identifier: GPL-3.0-or-later
// Czytnik plików EDF z bufora bajtów, bez zależności od Arduino, bez wyjątków i RTTI.
// Zasady jak w tools/kalibracja/edf.py (PlikEDF), a ten za AirBridge (`src/report_edf.cpp`):
//  - nagłówek stały 256 B, potem 256 B opisu na sygnał (etykieta 16, jednostka 8, fiz. min 8, fiz. max 8,
//    cyf. min 8, cyf. max 8, próbki w rekordzie 8, w kolumnach jak w normie EDF);
//  - próbka to liczba 16-bitowa little-endian ze znakiem; wartość fizyczna
//    = fiz_min + (cyfrowa − cyf_min) × (fiz_max − fiz_min) / (cyf_max − cyf_min);
//  - próbka poprawna, gdy cyfrowa mieści się w [cyf_min, cyf_max]; sygnały `Crc16` i `EDF Annotations` nie są
//    sygnałami pomiarowymi;
//  - data i godzina startu to czas lokalny aparatu bez strefy („sekundy cywilne” od 1970-01-01 00:00).
// Plik krótszy niż nagłówek i rekordy zapowiedziane w nagłówku (albo z liczbą rekordów -1, plik w trakcie zapisu) to
// błąd: panel nie czyta pliku w połowie (tools/kalibracja/edf.py w takim przypadku czyta tylko pełne rekordy).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace edf {

enum class Blad : uint8_t {
  BRAK = 0,
  KROTKI_NAGLOWEK,  // mniej niż 256 B albo mniej niż nagłówek z opisem sygnałów
  WERSJA,           // pole wersji inne niż "0"
  NAGLOWEK,         // pole liczbowe nieczytelne, zła data startu, zła długość nagłówka albo liczba sygnałów, czas
                    // rekordu ≤ 0 przy sygnałach pomiarowych
  SYGNAL,           // opis sygnału: cyf_min ≥ cyf_max, zakres cyfrowy poza [-32768, 65535], fiz_min = fiz_max, próbki < 1
  REKORDY,          // liczba rekordów ujemna (plik w trakcie zapisu) albo za duża
  UCIETY,           // bajtów mniej niż nagłówek + rekordy × rozmiar rekordu
};
const char* opisBledu(Blad b);

struct Sygnal {
  std::string etykieta, jednostka;
  double fizMin = 0, fizMax = 0;
  long cyfMin = 0, cyfMax = 0;
  int probki = 0;      // próbek w jednym rekordzie
  size_t offset = 0;   // przesunięcie sygnału w rekordzie, w bajtach
  bool adnotacje() const { return etykieta == "EDF Annotations"; }
  bool crc() const { return etykieta == "Crc16"; }
  bool poprawna(long cyfrowa) const { return !adnotacje() && !crc() && cyfMin <= cyfrowa && cyfrowa <= cyfMax; }
  double fizyczna(long cyfrowa) const {
    return fizMin + (double)(cyfrowa - cyfMin) * (fizMax - fizMin) / (double)(cyfMax - cyfMin);
  }
  // Wartość fizyczna z granicami fizycznymi pomnożonymi przez `skala` przed przeliczeniem (L/s → L/min, L → mL), jak
  // AirBridge i edf.py: próbka równa progowi nie przekracza go przez zaokrąglenie.
  double fizyczna(long cyfrowa, double skala) const {
    double lo = fizMin * skala, hi = fizMax * skala;
    return lo + (double)(cyfrowa - cyfMin) * (hi - lo) / (double)(cyfMax - cyfMin);
  }
};

// Najwięcej sygnałów w pliku i rekordów [stała techniczna]: STR.edf aparatu ma ok. 80 sygnałów i jeden rekord na
// dobę (kilkaset), pliki sesji kilka do kilkunastu sygnałów; granice chronią pamięć przy uszkodzonym nagłówku.
const int MAKS_SYGNALOW = 512;
const long MAKS_REKORDOW = 1000000;

// Nagłówek i widok na dane. `dane` wskazuje na bufor wołającego, który musi żyć dłużej niż ten obiekt.
struct Plik {
  long long startS = 0;     // sekundy cywilne startu (czas lokalny aparatu)
  double czasRekordu = 0;   // sekundy
  long rekordy = 0;
  bool edfPlus = false;
  bool edfPlusD = false;  // pole zastrzeżone zaczyna się od "EDF+D" (rekordy nieciągłe)
  bool niepelny = false;  // za zadeklarowanymi rekordami są jeszcze bajty (edf.py `niepelny`); czytane tylko rekordy
  size_t bajtyNaglowka = 0, bajtyRekordu = 0;
  std::vector<Sygnal> sygnaly;
  const uint8_t* dane = nullptr;  // początek pliku
  size_t rozmiar = 0;
  // Sygnał o etykiecie `etykieta` (bez `Crc16`); nullptr, gdy go nie ma.
  const Sygnal* sygnal(const std::string& etykieta) const;
  // Surowa próbka (16 bitów bez znaku, jak w pliku) rekordu `rekord`, próbki `i` sygnału `s`. Wołający pilnuje
  // zakresów (rekord < rekordy, i < s.probki); poza zakresem 0xFFFF.
  uint16_t surowa(const Sygnal& s, long rekord, int i) const;
  // Próbka jako liczba 16-bitowa ze znakiem (jak tablica `h` w edf.py `rekordy_probek()`, pliki sesji PLD, SAD).
  long probka(const Sygnal& s, long rekord, int i) const { return (long)(int16_t)surowa(s, rekord, i); }
  double koniecS() const { return (double)startS + (double)rekordy * czasRekordu; }
};

// Adnotacja EDF+ (edf.py `adnotacje()`): onset w sekundach od startu pliku, czas trwania (maTrwanie = false: brak
// pola) i tekst bez spacji na końcach.
struct Adnotacja {
  double onset = 0;
  bool maTrwanie = false;
  double trwanie = 0;
  std::string tekst;
};
// Wszystkie adnotacje z tekstem z sygnału `EDF Annotations` (bez niego: pusta lista, true). Blok rekordu dzielony na
// TAL po bajcie 0, TAL na części po 0x14; czas musi zaczynać się od '+' albo '-', czas trwania po 0x15 nie może być
// ujemny. false: zła adnotacja (jak BladEDF w edf.py), `wynik` bez zmian.
bool adnotacje(const Plik& p, std::vector<Adnotacja>& wynik);

// Nagłówek z bufora `bajty` (`rozmiar` bajtów) do `p`. Przy błędzie `p` bez zmian.
Blad czytaj(const uint8_t* bajty, size_t rozmiar, Plik& p);

// Liczba dni cywilnych od 1970-01-01 dla daty (algorytm dni od epoki, kalendarz gregoriański).
long long dniOdEpoki(int rok, int miesiac, int dzien);

}  // namespace edf
