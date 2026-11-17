// SPDX-License-Identifier: GPL-3.0-or-later
// Czekanie na zasilaniu zewnętrznym zamiast głębokiego snu (ADR-0047, decyzja właściciela projektu 16.11.2026: „Na kablu nie
// wygaszamy elektroniki, aby pierwszy ekran pojawiał się szybciej”; R-NF-45, T-WYM-10…T-WYM-14). Rdzeń bez
// Arduino: dyrygent (src/dyrygent.cpp `Przebieg::czekajNaKablu()`) pyta tu przed każdym snem, czy zamiast głębokiego
// snu czekać z żywą pamięcią (na panelu prosta pętla z delay(20), bez lekkiego uśpienia: wariant prostszy przy wydaniu
// bez nocy obserwacji, 16.11.2026; lekkie uśpienie odłożone); adapter snu (src/adaptery.h `Sen::czekaj()`) tylko wykonuje.
//
// Zasada: czekanie wyłącznie przy pewnym odczycie „zasilanie zewnętrzne” na początku przebiegu i tuż przed snem,
// przy `kabel_bez_snu` innym niż `nie` (T-WYM-16; wartość wpisana komendą `W` w tym przebiegu działa od razu), przy
// NVS `cpap` do zapisu, bez blokady po restarcie po błędzie (T-WYM-17), poza trybem awaryjnym, z pamięcią w budżecie
// (src/wybudzenie.h pamiecWBudzecie()), dużym wolnym blokiem PSRAM i pamięci wewnętrznej i przed planowym restartem.
// Każda wątpliwość = głęboki sen, czyli zwykła ścieżka (po wybudzeniu z głębokiego snu program startuje od zera).
#pragma once
#include <cstdint>
#include <string>

#include "adaptery.h"  // Zasilanie, PrzyczynaStartu

namespace czekanie {

// [PRÓG] T-WYM-13: planowy restart (głęboki sen zamiast czekania) po tylu sekundach pracy programu bez restartu,
// w oknie OKNO_RESTARTU_OD_MIN…OKNO_RESTARTU_DO_MIN (pora snu terapii, bez naciśnięć; do decyzji).
const uint32_t PLANOWY_RESTART_PO_S = 20u * 3600u;
const int OKNO_RESTARTU_OD_MIN = 2 * 60;  // 02:00 [PRÓG]
const int OKNO_RESTARTU_DO_MIN = 4 * 60;  // 04:00, bez tej minuty [PRÓG]
// [PRÓG] T-WYM-13: najpóźniej po tylu sekundach albo przebiegach restart bez względu na porę (zegar nieznany,
// brak przebiegu w oknie).
const uint32_t RESTART_NAJPOZNIEJ_PO_S = 48u * 3600u;
const uint32_t PRZEBIEGOW_NAJWIECEJ = 1000u;
// [PRÓG] T-WYM-13: najmniejszy wolny ciągły blok PSRAM, przy którym wolno czekać (fragmentacja po dniach pracy);
// pamięć wewnętrzna i stos: budżet z src/wybudzenie.h (PROG_STERTY_B, PROG_STOSU_B).
const uint32_t PSRAM_BLOK_MIN_B = 1024u * 1024u;
// [PRÓG] T-WYM-13 (zmiana 2026-11-17): najmniejszy wolny ciągły blok pamięci wewnętrznej
// (MALLOC_CAP_INTERNAL), przy którym wolno czekać. Bufory Wi-Fi, TLS i LittleFS biorą pamięć wewnętrzną w jednym
// kawałku, a budżet sterty (PROG_STERTY_B) liczy tylko sumę wolnej pamięci, nie jej rozdrobnienie. Szacunek: około
// 1/3 bloku na jednym egzemplarzu panelu przy starcie (16.11.2026: wolna wewnętrzna 202 KB, największy blok 135 KB, sterta_min
// około 160 KB); obserwacja własna, 1 panel.
const uint32_t WEWN_BLOK_MIN_B = 48u * 1024u;
// Największy blok pamięci wewnętrznej nieznany (bez sprawdzenia); Diagnostyka::najwiekszyBlokWewn() domyślnie.
const uint32_t BLOK_NIEZNANY = UINT32_MAX;
// [PRÓG] T-WYM-12: co tyle sekund adapter (pętla z delay(20), strażnik odnawiany w każdym obiegu) woła sprawdzenie
// zasilania (odłączony kabel = głęboki sen na resztę planu).
const uint32_t KAWALEK_CZEKANIA_S = 10u;

enum class Wybor : uint8_t { SEN, CZEKANIE };
// Powód wyboru (wiersz logu `sen: głęboki mimo zasilania zewnętrznego: …`).
enum class Powod : uint8_t {
  CZEKANIE,         // czekanie
  BEZ_OBSLUGI,      // adapter snu nie umie czekać albo wołający nie prowadzi kolejnych przebiegów
  NIE_KABEL,        // na początku przebiegu albo tuż przed snem zasilanie inne niż zewnętrzne (bateria, nieznane)
  WYLACZONE,        // `kabel_bez_snu=nie` w /konfiguracja.txt (wyłącznik awaryjny, T-WYM-16)
  USTAWIENIE_NIEZNANE,  // `kabel_bez_snu` nieznane: pamięć plików niezamontowana albo plik jest, a odczyt zawiódł;
                        // w tym przebiegu głęboki sen (zmiana 2026-11-17)
  NVS_NOWSZY,       // NVS `cpap` z nowszej wersji, tylko do odczytu (T-WYM-54): blokada po błędzie nie zapisze się
                    // (T-WYM-17), więc zawsze głęboki sen (zmiana 2026-11-17)
  PO_BLEDZIE,       // restart po błędzie w trybie bez snu: do końca doby głęboki sen (T-WYM-17)
  AWARYJNY,         // tryb awaryjny (R-AW-39): zawsze głęboki sen
  BEZ_BUDZIKA,      // plan bez budzenia zegarem
  PAMIEC,           // sterta albo stos poza budżetem (pamiecWBudzecie())
  PSRAM,            // największy wolny blok PSRAM poniżej PSRAM_BLOK_MIN_B
  WEWN_BLOK,        // największy wolny blok pamięci wewnętrznej poniżej WEWN_BLOK_MIN_B (zmiana 2026-11-17)
  PLANOWY_RESTART,  // T-WYM-13
};

struct Wejscie {
  bool obsluga = false;                    // Sen::czekanieDostepne() i wołający prowadzi kolejne przebiegi
  Zasilanie naStarcie = Zasilanie::NIEZNANE;  // odczyt z początku przebiegu (R-DANE-65)
  Zasilanie przedSnem = Zasilanie::NIEZNANE;  // świeży odczyt tuż przed snem
  bool awaryjny = false;
  bool wylaczone = false;                  // `kabel_bez_snu=nie`
  bool ustawienieNieznane = false;         // `kabel_bez_snu` nieznane (konfiguracja nieczytelna, T-WYM-16)
  bool nvsNowszy = false;                  // NVS `cpap` tylko do odczytu (wersja wyższa niż znana, T-WYM-54)
  bool poBledzie = false;                  // blokada po restarcie po błędzie trwa (ta doba albo data nieznana)
  bool budzZegarem = true;                 // PlanSnu::budzZegarem
  bool pamiecWBudzecie = true;             // src/wybudzenie.h pamiecWBudzecie()
  // PSRAM; BLOK_NIEZNANY = bez sprawdzenia. 0 to pomiar: brak wolnego bloku, więc głęboki sen (zmiana 2026-11-17,
  // jak pamięć wewnętrzna)
  uint32_t najwiekszyBlokPsram = BLOK_NIEZNANY;
  // pamięć wewnętrzna; BLOK_NIEZNANY = bez sprawdzenia. 0 to pomiar: brak wolnego bloku, więc głęboki sen
  // (zmiana 2026-11-17; na ESP32 heap_caps_get_largest_free_block() zwraca 0 bez bloku).
  uint32_t najwiekszyBlokWewn = BLOK_NIEZNANY;
  uint32_t sekundOdStartu = 0;             // od startu programu, z czasem czekania; 0 = nieznane
  uint32_t przebiegow = 0;                 // przebiegi od startu programu, z bieżącym
  int minutaDoby = -1;                     // -1 = zegar nieznany
};

struct Decyzja {
  Wybor wybor = Wybor::SEN;
  Powod powod = Powod::BEZ_OBSLUGI;
};

// T-WYM-10, T-WYM-11, T-WYM-13: kolejność sprawdzeń jak w polach Powod (pierwszy niespełniony warunek wygrywa).
Decyzja decyzja(const Wejscie& w);
// Tekst powodu do logu (`bateria albo stan nieznany`, `pamięć poza budżetem` …).
const char* opis(Powod p);
// T-WYM-12: czy przerwać czekanie przy tym odczycie zasilania (wszystko poza pewnym „zewnętrzne”).
bool przerwacCzekanie(Zasilanie teraz);

// ---------- T-WYM-12: przycisk kończący czekanie (adapter SenEsp32::czekaj(), zmiana 2026-11-17)
// ----------
// Stan klawisza z próbkowania adaptera (co 5 ms, po odsianiu drgań) i naciśnięcie już puszczone z kolejki.
struct KlawiszCzekania {
  bool wcisniety = false;
  bool pomin = false;  // już obsłużony (przycisk budzący) albo wyczyszczony: nie budzi drugi raz
  int64_t odUs = 0;    // chwila wciśnięcia (esp_timer_get_time()), 0 = nieznana
};
struct NacisniecieCzekania {
  uint8_t przycisk = 0;      // (int)Przycisk
  uint32_t trwaloMs = 0;     // od wciśnięcia do puszczenia
  int64_t puszczonyUs = 0;   // chwila puszczenia, 0 = nieznana
};
struct PrzyciskKonczacy {
  uint8_t bit = 0;           // (1 << (int)Przycisk), 0 = żaden
  int zdjacZKolejki = 0;     // tyle naciśnięć z początku kolejki zdjąć (spoza maski planu przepadają, jak dotąd)
  bool zKolejki = false;     // budzi naciśnięcie już puszczone (krótsze niż obieg czekania)
  int klawisz = 0;           // !zKolejki: indeks klawisza (oznaczyć `pomin`)
  uint32_t trwaloMs = 0;     // zKolejki: czas trzymania (przytrzymanie zostaje przytrzymaniem)
  int64_t odUs = 0;          // chwila wciśnięcia budzącego (ms() rdzenia liczy od niej), 0 = nieznana
};
// Wybór: kandydat trzymany teraz (pierwszy z lewej, w masce, bez `pomin`) i pierwsze naciśnięcie z kolejki w masce.
// Oba: wygrywa wciśnięte wcześniej (z kolejki: puszczonyUs − trwaloMs; chwila nieznana = z kolejki pierwsze, bo
// zostało puszczone, zanim pętla je zobaczyła). Naciśnięcia z kolejki przed tym w masce: zdjęte bez obsługi.
PrzyciskKonczacy przyciskKonczacy(uint8_t maska, const KlawiszCzekania klawisze[4], const NacisniecieCzekania* kolejka,
                                  int ile);

// ---------- T-WYM-17: samoczynny powrót do głębokiego snu po restarcie po błędzie ----------
// NVS `cpap`: `cz_akt` (flaga: program był w trybie bez snu, ustawiana przy pierwszym czekaniu programu, kasowana przy
// każdym głębokim śnie po czekaniu i przy starcie innym niż wybudzenie) i `cz_blok` (data doby blokady, `?` = data
// nieznana w chwili restartu; brak klucza = bez blokady).
extern const char* const NVS_KLUCZ_AKTYWNE;  // `cz_akt`
extern const char* const NVS_KLUCZ_BLOKADA;  // `cz_blok`
// Start po błędzie programu (panika, strażnik zadania, strażnik przerwań, inny strażnik): wtedy, przy fladze
// `cz_akt`, blokada do końca doby.
bool startPoBledzie(PrzyczynaStartu p);
// Stan blokady przy decyzji: `zapisana` = wartość `cz_blok` ("" = brak), `dzis` = data doby ("" = zegar nieznany).
enum class Blokada : uint8_t {
  BRAK,       // bez blokady
  TRWA,       // blokada tej doby (albo data nieznana i zegar nadal nieznany)
  USTAL_DZIS, // blokada z datą nieznaną, zegar już znany: zapisać `dzis` i traktować jak TRWA
  KONIEC,     // blokada z wcześniejszej doby: usunąć klucz, bez blokady
  PRZYSZLA,   // data blokady dalej niż 1 doba po `dzis` (zegar był w przyszłości w chwili restartu albo zła data):
              // traktować jak wygasłą, usunąć klucz, bez blokady (zmiana 2026-11-17)
};
Blokada blokada(const std::string& zapisana, const std::string& dzis);

}  // namespace czekanie
