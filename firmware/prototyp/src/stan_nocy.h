// SPDX-License-Identifier: GPL-3.0-or-later
// Stan ostatniej nocy przy pobieraniu (spec 5.1.4): jedna maszyna stanów od pierwszego
// sprawdzenia w oknie porannym do zapisu rekordu, bez zależności od Arduino. Dyrygent (src/dyrygent.cpp) podaje, co
// zobaczył w tym wybudzeniu (Obserwacja), i zapisuje wynik w NVS (`cpap`/`sn_noc`, zapisStanu()), bo stan musi
// przeżyć sen: wejście w budowę po 11:00 (R-DANE-25) i noc bez terapii z braku na liście (R-DANE-20) zależą od tego,
// co było wcześniej tej doby.
//
// Co z tego bierze reszta panelu:
//   - sygnał `noc_bez_rekordu` silnika reguł (E-10 dla nocy bez rekordu, src/rodzaje_e.cpp noce_bez_terapii): tylko
//     w stanie BEZ_TERAPII (AirBridge odpowiada, po 11:00 dnia nie ma na liście, status bez stanu z R-DANE-35); nigdy
//     przy E-01 (AirBridge milczy) ani w budowie (R-DANE-35, R-DANE-22);
//   - zapis rekordu „częściowa” ostatniej nocy (R-DANE-15, R-DANE-25): od 11:00, a dla nocy, która weszła w budowę po
//     11:00, dopiero o 23:30 (czesciowaTeraz());
//   - plan wybudzeń (src/plan.h WejsciePlanu::nocBezTerapii): ponawianie co 30 min do 23:30 (R-DANE-18).
// Noc w stanie NIEOBECNA, BRAK_DANYCH, W_BUDOWIE i W_BUDOWIE_STATUS nie jest jeszcze nocą: nie liczy się do żadnej
// reguły (5.1.4, akapit pod tabelą). Rekord w historii ma pierwszeństwo: z rekordem stan to KOMPLETNA albo CZESCIOWA.
#pragma once
#include <cstdint>
#include <string>

#include "noc.h"

namespace stan_nocy {

enum class Stan : uint8_t {
  NIEOBECNA,         // dnia nie ma na liście albo jeszcze nie było sprawdzenia (przed 11:00 to nie E-10, R-DANE-22)
  BRAK_DANYCH,       // „nieobecna (brak danych)”: od 11:00 AirBridge nie odpowiada (E-01, R-DANE-20)
  W_BUDOWIE,         // dzień na liście, raport z `partial = true` (R-DANE-03)
  W_BUDOWIE_STATUS,  // `/api/status` w stanie z R-DANE-35 (`rop` = 1, blocked, post-processing, storage)
  KOMPLETNA,         // rekord kompletny w historii (M-01 < 15 min: noc bez terapii liczy reguła E-10 z rekordu)
  CZESCIOWA,         // rekord „częściowa” w historii (R-DANE-15, R-DANE-25)
  BEZ_TERAPII        // AirBridge odpowiada, po 11:00 dnia nie ma na liście (E-10, R-DANE-20)
};
const char* nazwa(Stan s);  // "nieobecna", "brak_danych", "w_budowie", "w_budowie_status", "kompletna", …

struct FazaNocy {
  std::string data;          // data poranka nocy (4.1); "" = nieznana
  Stan stan = Stan::NIEOBECNA;
  bool budowaPo11 = false;   // wejście w „w budowie” po 11:00 (R-DANE-25): rekord „częściowa” dopiero o 23:30
  bool naLiscie = false;     // dzień był na liście z `partial = true` (R-DANE-35 o 23:30: rekord „częściowa”)
  bool operator==(const FazaNocy& o) const {
    return data == o.data && stan == o.stan && budowaPo11 == o.budowaPo11 && naLiscie == o.naLiscie;
  }
  bool operator!=(const FazaNocy& o) const { return !(*this == o); }
};

// Co dyrygent zobaczył w tym wybudzeniu w sprawie ostatniej nocy.
enum class Obserwacja : uint8_t {
  ZEGAR,             // wybudzenie bez odczytu albo odczyt bez rozstrzygnięcia (lista 202/409, raport nieczytelny)
  BRAK_ODPOWIEDZI,   // Wi-Fi albo AirBridge nie odpowiada, odrzucone logowanie (E-01, R-AW-34)
  STATUS_BUDOWA,     // `/api/status` ze stanem z R-DANE-35
  LISTA_BEZ_DNIA,    // AirBridge odpowiada listą `action=days` bez dnia ostatniej nocy
  RAPORT_CZESCIOWY,  // dzień na liście, raport ma `partial = true`
  RAPORT_KOMPLETNY   // raport `partial = false` (rekord zapisany)
};

// Przejście 5.1.4. `rekord` = rekord ostatniej nocy w historii (nullptr = brak); ma pierwszeństwo przed obserwacją.
// Nowa data poranka zaczyna od NIEOBECNA. Po 23:30 [PRÓG] W_BUDOWIE_STATUS bez dnia na liście wraca do NIEOBECNA
// (bez E-10 i bez rekordu, R-DANE-35).
FazaNocy przejdz(const FazaNocy& s, const std::string& dzis, int minuta, Obserwacja o, const Noc* rekord);

// E-10 dla nocy bez rekordu (sygnał `noc_bez_rekordu`): stan BEZ_TERAPII tej daty.
bool bezTerapii(const FazaNocy& s, const std::string& dzis);
// Czy raport ostatniej nocy z `partial = true` zapisać teraz jako rekord „częściowa”: od 11:00 (R-DANE-15), a przy
// wejściu w budowę po 11:00 od 23:30 (R-DANE-25). Przed tym czasem noc czeka (sprawdzenie co 30 min, R-DANE-18).
bool czesciowaTeraz(const FazaNocy& s, const std::string& dzis, int minuta);
// O 23:30 noc, która weszła w budowę po 11:00, wymaga jednego pobrania raportu w slocie nocnym (R-DANE-25; lista nocy
// nie jest wtedy pobierana): stan W_BUDOWIE z flagą budowaPo11 albo W_BUDOWIE_STATUS z dniem na liście, bez rekordu.
bool pobranieO2330(const FazaNocy& s, const std::string& dzis, int minuta, const Noc* rekord);

// Zapis w NVS: `data;stan;flagi` (flagi: `p` = budowaPo11, `l` = naLiscie). Nieczytelny zapis = stan domyślny.
std::string zapisStanu(const FazaNocy& s);
FazaNocy odczytStanu(const std::string& t);

const int GODZINA_11 = 11 * 60;           // [PRÓG] R-DANE-15, R-DANE-20
const int KONIEC_SPRAWDZEN = 23 * 60 + 30;  // [PRÓG] R-DANE-18, R-DANE-25, R-DANE-35

}  // namespace stan_nocy
