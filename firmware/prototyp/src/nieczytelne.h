// SPDX-License-Identifier: GPL-3.0-or-later
// Trwale nieczytelny plik danych (decyzja właściciela projektu 22.11.2026; T-WYM-19, R-AW-40): licznik kolejnych
// wybudzeń, w których ten sam plik był na nośniku, a odczytu się nie udało (Magazyn::plikiNieczytelne(),
// T-WYM-19). Jedno nieudane czytanie w jednym wybudzeniu nie robi sprawy; sprawa powstaje po WYBUDZEN_DO_SPRAWY
// kolejnych wybudzeniach i znika sama po pierwszym udanym odczycie tej ścieżki. Sprawę widać w wierszu `Pamięć
// panelu` ekranu U-05 i na karcie strony „Otwarte sprawy” (ADR-0041 „Zmiana 2026-11-22”).
//
// Gdzie żyje stan: licznik w StanRtc (pamięć RTC, przeżywa głęboki sen, zero zapisów flash przy usterce chwilowej),
// a gotowa sprawa (ścieżka i chwila początku serii) dodatkowo w NVS `cpap`/`nczyt`, zapisywana tylko przy zmianie
// listy spraw (powstanie, zniknięcie), żeby restart (aktualizacja, przycisk, zanik zasilania) nie chował sprawy na
// kolejne 3 wybudzenia i nie przesuwał daty „od”. Ani licznik, ani sprawa nie zależą od nieczytelnego pliku.
// Bez Arduino, bez wyjątków (`-fno-exceptions`).
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nieczytelne {

// [PRÓG] techniczny bez źródła: 3 kolejne wybudzenia z nieudanym odczytem tej samej ścieżki. Usterka chwilowa
// (brak pamięci na bufor przy Wi-Fi i TLS, limit otwartych plików) mija w następnym wybudzeniu; trzy z rzędu to już
// usterka trwała. Wybudzenie, w którym pliku nie czytano wcale, licznika nie zmienia.
const uint8_t WYBUDZEN_DO_SPRAWY = 3;
// [PRÓG] techniczny bez źródła: wpis, którego ścieżki panel nie czytał z błędem przez 14 dób (ani
// z powodzeniem), wygasa: panel przestał tego pliku używać. 14 dób zamiast 3: pliki czytane rzadko (lista pobrań przy
// pobieraniu, prognoza, obrazki, porównanie, Dobry start) między odczytami nie tracą serii, a sprawa nie migocze;
// plik czytany rzadziej niż co 14 dób zaczyna serię od nowa. Prostsze niż licznik odczytów bez błędu (jedna stała,
// bez nowego pola) i zgodne z plik sypialni spoza roku granicy kończy sprawę osobną regułą (plik nieużywany,
// Przebieg::sciezkaUzywana()), bez czekania na wygaśnięcie.
const uint32_t WYGASA_S = 14u * 86400u;
const int MIEJSC = 4;                  // tyle ścieżek naraz; przy komplecie nowa ścieżka wypiera najstarszy wpis poniżej
                                       // progu, a plik oceny także sprawę pliku spoza oceny
const size_t DLUGOSC_SCIEZKI = 28;     // najdłuższa ścieżka rejestru `/dane/sypialnia/RRRR.csv` ma 24 znaki
const uint32_t MAGIA = 0x4E435A02u;    // znacznik ważnego bloku w RTC (inny = odtworzenie z NVS)
const char* const NVS_PRZESTRZEN = "cpap";
const char* const NVS_KLUCZ = "nczyt";  // `{ścieżka}@{od}` w wierszach, tylko sprawy (licznik ≥ próg)

struct Wpis {
  char sciezka[DLUGOSC_SCIEZKI];  // "" = miejsce wolne
  uint32_t od;                    // czas unix pierwszego nieudanego odczytu w serii; 0 = zegar nieznany
  uint32_t wybudzenie;            // StanRtc::wybudzen ostatniego policzenia (0 = odtworzony z NVS)
  uint32_t ostatnio;              // czas unix ostatniego nieudanego odczytu; 0 = nieznany (bez zegara, z NVS)
  uint8_t kolejnych;              // kolejne wybudzenia z nieudanym odczytem, najwyżej WYBUDZEN_DO_SPRAWY
};
struct Stan {
  uint32_t magia;
  uint32_t skrotNvs;  // CRC-32 tekstu spraw, który jest w NVS (zapis tylko przy zmianie)
  Wpis w[MIEJSC];
};

struct Sprawa {
  std::string sciezka;
  uint32_t od = 0;
};

// Po starcie innym niż wybudzenie z uśpienia (magia ≠ MAGIA): stan od zera i sprawy z tekstu NVS (licznik = próg).
// Zwraca true, gdy stan był nieważny i został odtworzony.
bool odtworz(Stan& s, const std::string& tekstNvs);
// Jedno wybudzenie: `nieudane` = Magazyn::plikiNieczytelne(), `udany(ścieżka)` = Magazyn::dostepUdany() (odczyt, zapis
// albo dopisanie), `jest(ścieżka)` = Magazyn::naNosniku() (nullptr: nie sprawdzać, np. pamięć niezamontowana).
// Wolno wołać wiele razy w tym samym wybudzeniu (ten sam `wybudzenie`): każda ścieżka liczy się raz na wybudzenie.
// Ścieżka nieudana w tym wybudzeniu: licznik +1 (pierwsza: `od` = `teraz`; przy komplecie miejsc wypiera najstarszy
// wpis poniżej progu). Pozostałe wpisy: udany dostęp albo brak pliku na nośniku albo `teraz` późniejsze
// niż `ostatnio` o ponad WYGASA_S = wpis usunięty; inaczej bez zmian. Wpis bez daty (`od` = 0, seria zaczęta
// bez zegara) dostaje `teraz`, gdy zegar się pojawi. Zwraca true, gdy zmieniła się lista spraw albo ich daty.
bool aktualizuj(Stan& s, uint32_t wybudzenie, const std::vector<std::string>& nieudane,
                const std::function<bool(const std::string&)>& udany,
                const std::function<bool(const std::string&)>& jest, uint32_t teraz);
// Bez sprawdzania nośnika (`jest` puste): wersja bez sprawdzania nośnika, używana w testach rdzenia (także brzegowych).
inline bool aktualizuj(Stan& s, uint32_t wybudzenie, const std::vector<std::string>& nieudane,
                       const std::function<bool(const std::string&)>& udany, uint32_t teraz) {
  return aktualizuj(s, wybudzenie, nieudane, udany, std::function<bool(const std::string&)>(), teraz);
}
// Plik, z którego liczona jest ocena terapii (stany, norma, silnik reguł): noce `/dane/noce/RRRR.csv`
// (historia), `/dane/czynniki.csv` (noce wyłączone z normy, wykluczoneZNormy()), `/dane/odpowiedzi.csv` (odpowiedzi
// silnika i okresy infekcji), `/dane/reguly.csv` (pamięć reguł i stany wzorców). Tylko sprawa takiego pliku robi
// ocenę niepełną; pozostałe pliki (dzienniki, prognoza, obrazki, lista pobrań, portret, porównanie, Dobry start,
// sypialnia) są widoczne na U-05 i w „Otwartych sprawach”, a ocena zostaje bez zmian.
bool plikOceny(const std::string& sciezka);
// (ADR-0029 pkt 7): ocena niepełna, gdy jest sprawa pliku oceny albo plik oceny jest nieudany w tym wybudzeniu
// (`nieudaneTeraz`), a nie ma dla niego wpisu (wszystkie miejsca zajęte przez pliki oceny), więc nie zależy od tego,
// czy znalazło się miejsce. Plik oceny nieudany teraz z wpisem poniżej progu: jeszcze bez wymuszenia (usterka chwilowa).
bool ocenaNiepelna(const Stan& s, const std::vector<std::string>& nieudaneTeraz);
// Sprawy (licznik ≥ próg) od najstarszej serii; bez spraw pusta lista.
std::vector<Sprawa> sprawy(const Stan& s);
// Tekst NVS spraw (`{ścieżka}@{od}\n` …); "" = bez spraw.
std::string tekstNvs(const Stan& s);
// Rodzaj danych prostym słowem, bez ścieżki (`noce 2026`, `czynniki nocy`, `pamięć spraw` …; nieznana `plik danych`).
std::string rodzaj(const std::string& sciezka);

}  // namespace nieczytelne
