// SPDX-License-Identifier: GPL-3.0-or-later
// Dyrygent wybudzenia (T-KOMP-01, spec techniczna 6.4, 7.1; ADR-0021 pkt 1): wykonuje jedno wybudzenie, wołając
// rdzeń (src/wybudzenie.h, src/start.h i pliki logiki prototypu) i adaptery (src/adaptery.h) w ustalonej kolejności.
// Sam niczego nie liczy: każdy warunek to wywołanie funkcji rdzenia. Bez Arduino, więc ten sam kod biegnie na
// panelu (src/main.cpp tworzy adaptery ESP32 i woła wybudzenie() z setup()) i na komputerze z atrapami (T-WYM-78).
//
// Przebieg wybudzenia plus siatka aktualizacji:
//  1. strażnik zadania zapisany przed każdym innym adapterem (T-WYM-81), odnawiany na granicy kroków 7.1 i w każdym
//     obiegu pętli czekania (Wi-Fi, ponowienia R-AW-01, odpytywanie joba, zaległe doby, czuwanie, okno serwisowe);
//  2. decyzjaStartu() przed montowaniem plików i Wi-Fi (T-WYM-82); tryb awaryjny: bez zapisu plików, bez sieci poza
//     oknem serwisowym na U-05, U-06 raz, sen tylko z budzeniem przyciskami (T-WYM-83, R-AW-39);
//  3. przy StanWersji::DO_SPRAWDZENIA samosprawdzenie przed pierwszym snem, potem oznaczenie albo powrót (T-WYM-41);
//  4. wiersz wierszPamieci() w porcie serwisowym tuż przed snem, potem zerujPrzedSnem() i Sen::spij().
#pragma once
#include <cmath>
#include <string>
#include <vector>
#include "adaptery.h"
#include "magazyn.h"

// Adresy i sekrety: sieć i AirBridge z NVS (src/sekrety.h, secrets.h tylko jako wartość początkowa), źródło
// otoczenia i współrzędne z include/secrets.h (do kroku 7, T-KOMP-12); testy wypełniają je wartościami atrapy. Puste otoUrl = bez lokalnego źródła otoczenia (R-AW-07).
struct KonfiguracjaPanelu {
  std::string abAdres;   // AB_IP
  std::string abKonto;   // AB_USER
  std::string abHaslo;   // AB_PASS
  std::string otoUrl;    // OTO_URL albo ""
  std::string otoToken;  // OTO_TOKEN albo ""
  bool oknoZHaslem = false;  // OTA_PASS w secrets.h (linia okna serwisowego na ekranie testu; bez hasła okna nie ma)
  // Tylko wariant testowy symulatora: plan bez slotu dziennego
  // (src/plan.h WejsciePlanu::slotDzienny). src/main.cpp go nie ustawia, więc na panelu zawsze false.
  bool bezSlotuDziennego = false;
  // rejestr formatów plików tej wersji firmware (src/magazyn.h); nullptr = rejestrFormatow. Tylko testy podają
  // inny (np. „firmware znający wersję 2”, T-WYM-43, R-NF-28); src/main.cpp go nie ustawia.
  const std::vector<FormatPliku>* rejestr = nullptr;
  // (na końcu struktury: testy wypełniają ją inicjalizacją pozycyjną): współrzędne miasta do prognozy godzinowej Open-Meteo i S-K-37 (R-KONF-24); NAN = bez prognozy
  // godzinowej i bez wschodu i zachodu słońca (R-KON-53, R-KON-54). src/main.cpp: POGODA_LAT, POGODA_LON z secrets.h.
  double pogodaLat = NAN, pogodaLon = NAN;
  // (src/sekrety.h): sieć i AirBridge z NVS, secrets.h tylko jako wartość początkowa. true = brak sieci
  // albo adresu AirBridge (R-KONF-07, U-02); opisSekretow: wiersz do logu bez haseł (sekrety::opis()).
  bool pierwszaKonfiguracja = false;
  std::string opisSekretow{};
  // sieć domowa (do formularza U-03: nazwa w polu, hasło tylko jako „ustawione”, R-KONF-38, R-KONF-39)
  std::string ssid{}, hasloWifi{};
  // (ADR-0047) true = bez TekstyWybudzenia po przebiegu: na panelu sen nie wracał, więc tekstów nikt nie liczył;
  // przy czekaniu na kablu przebieg wraca, a teksty są tylko dla testów i symulatora. src/main.cpp ustawia true.
  bool bezTekstowKoncowych = false;
};

struct PamiecPrzebiegow;  // src/przebiegi.h (ADR-0047)

// Teksty ostatniego wybudzenia do testów i symulatora (ekran testu jest tylko w pikselach).
struct TekstyWybudzenia {
  // Teksty U-05 w kolejności: najpierw każdego U-05 narysowanego w tym wybudzeniu, na końcu U-05 według stanu tuż
  // przed snem (także gdy widoczny był inny ekran). Każdy U-05 daje 15 tekstów (od 14.11.2026, makieta
  // projekt ekranu): 12 wierszy kolumn `PANEL` i `ŁĄCZNOŚĆ` jako `{nazwa}: {wartość}` (np. `Pamięć panelu: nie
  // zamontowana`, `Pogoda i powietrze: przekierowanie`), `Werdykt: {werdykt}` (R-KONF-58) i obie części ramki (np.
  // `Okno przywracania do 07:10`, T-WYM-87; `Wgrywanie przez Wi-Fi: wyłączone`, T-WYM-45).
  std::vector<std::string> linieTestu;
  // Diagnostyka U-05: blok wierszy, który dyrygent wypisuje w logu portu serwisowego przy każdym
  // rysowaniu U-05 (prefiks `U-05 · `), w tej samej kolejności co linieTestu: `Wersja: {identyfikator} · slot {s}`,
  // `Wybudzenie: …`, `Wi-Fi: …`, `AirBridge: …`, `Ostatni odczyt: … · historia {n} nocy`, `Czynniki: {n} nocy …`,
  // bateria w V, `Otoczenie: …`, `Pamięć: min. sterta …`, `Dane: …` (pełna lista problemów, T-WYM-39).
  std::vector<std::string> diagnostykaTestu;
  // Kolejka linii uwagi ekranu dnia (R-AW-05, R-AW-16) według stanu tuż przed snem, np.
  // `Błąd pamięci panelu. Wyniki mogą być niepełne.`
  std::vector<std::string> linieUwagi;
};

class Dyrygent {
 public:
  Dyrygent(const Adaptery& a, const KonfiguracjaPanelu& k) : a_(a), k_(k) {}
  // (ADR-0047) `p`: pamięć między przebiegami bez głębokiego snu, którą wołający trzyma przez cały program
  // (src/main.cpp). Z nią dyrygent na zasilaniu zewnętrznym może czekać zamiast głębokiego snu (T-WYM-10) i bierze
  // noce oraz ocenę silnika z poprzedniego przebiegu, gdy klucz z treści się zgadza (T-WYM-14). nullptr = jak dotąd.
  Dyrygent(const Adaptery& a, const KonfiguracjaPanelu& k, PamiecPrzebiegow* p) : a_(a), k_(k), pp_(p) {}
  // Jedno wybudzenie (przebieg) od startu programu albo końca czekania do snu. Na panelu po głębokim śnie nie wraca
  // (Sen::spij(), restart po wgraniu albo powrót do poprzedniej wersji); z atrapami wraca po Sen::spij() albo
  // Uruchomienie::wrocDoPoprzedniej(). Zwraca true, gdy przebieg skończył się czekaniem na kablu (Sen::czekaj(),
  // ADR-0047): wołający uruchamia wtedy następny przebieg nowym Dyrygentem z tą samą pamięcią przebiegów.
  bool wybudzenie();
  // Teksty ostatniego wybudzenia: linie każdego narysowanego ekranu testu i linie stanu tuż przed snem
  // (TekstyWybudzenia); puste po trybie awaryjnym.
  const TekstyWybudzenia& teksty() const { return teksty_; }

 private:
  Adaptery a_;
  KonfiguracjaPanelu k_;
  TekstyWybudzenia teksty_;
  PamiecPrzebiegow* pp_ = nullptr;
};
