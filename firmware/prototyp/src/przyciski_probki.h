// SPDX-License-Identifier: GPL-3.0-or-later
// Próbkowanie przycisków i kolejka naciśnięć (R-PRZ-20, T-KOMP-19), rdzeń bez Arduino (
// 22.11.2026): ten sam kod wykonuje adapter panelu (src/adaptery_esp32.cpp: zadanie esp_timer co 5 ms
// i metody PrzyciskiEsp32, wszystko pod jednym muxem) i atrapa w testach na PC (AtrapaPrzyciskowProbkowanych).
// Stan przycisku zmienia się po PROBEK_STANU zgodnych próbkach (15 ms), więc drgania styku nie dają fałszywych
// naciśnięć ani zgubionych puszczeń. Wciśnięcie: pisk (adapter, poza sekcją krytyczną, według wyniku probka());
// puszczenie: naciśnięcie do kolejki z czasem trzymania.
#pragma once
#include <cstdint>

namespace przyciski_probki {

const int MIEJSC_KOLEJKI = 8;              // R-PRZ-20 „najwyżej 8 [PRÓG]”; nadmiar (najnowsze) przepada
const int64_t OKRES_PROBKI_US = 5000;      // próbka co 5 ms
const uint8_t PROBEK_STANU = 3;            // 15 ms zgodnych próbek
const int64_t PUSZCZENIE_ZDJECIE_US = 50000;  // obsluzone(): puszczenie najwyżej tyle temu = to samo naciśnięcie

struct Klawisz {
  bool wcisniety = false;  // stan po odsianiu drgań
  uint8_t zgodnych = 0;    // kolejne próbki różne od `wcisniety`
  int64_t odUs = 0;        // chwila wciśnięcia (0 = nieznana, np. przycisk wciśnięty przy starcie programu)
  bool pomin = false;      // puszczenie nie trafia do kolejki (przycisk budzący, wyczysc(), obsluzone())
};
struct Nacisniecie {
  uint8_t przycisk = 0;     // (int)Przycisk: 1 lewy, 2 środkowy, 3 prawy
  uint32_t trwaloMs = 0;    // od wciśnięcia do puszczenia
  int64_t puszczonyUs = 0;  // chwila puszczenia (ADR-0047: kolejność przy końcu czekania)
};
// Wynik jednej próbki, do wykonania przez adapter poza sekcją krytyczną: bity (1 << przycisk).
struct ZmianyProbki {
  uint8_t piski = 0;      // wciśnięcia bez `pomin`: pisk w chwili naciśnięcia (R-PRZ-20: każde naciśnięcie raz)
  uint8_t puszczone = 0;  // puszczenia bez `pomin` ze znaną chwilą wciśnięcia (znacznik obrazu pomiarowego)
};

// Chwila wciśnięcia `odUs` (esp_timer, µs) w ms przebiegu, który zaczął się w `poczatekMs` (millis(), ta sama
// arytmetyka co ZegarEsp32::ms(): obcięcie do 32 bitów i odejmowanie modulo). Wciśnięty przed początkiem przebiegu
// (przycisk trzymany przez czekanie na kablu, który go nie zakończył) = 0, czyli przed każdym rysowaniem tego
// przebiegu. false = chwila nieznana (odUs = 0).
bool chwilaWPrzebiegu(int64_t odUs, uint32_t poczatekMs, uint32_t& odMs);

class Model {
 public:
  // Start programu: przycisk wciśnięty teraz (budzący) bez pisku i bez kolejki po puszczeniu; kolejka pusta.
  void start(uint8_t surowe);
  // Próbka stanu pinów (`surowe`: bity wciśniętych, 1 << przycisk) w chwili `tUs`.
  ZmianyProbki probka(uint8_t surowe, int64_t tUs);
  // Najstarsze naciśnięcie z kolejki; false = pusta.
  bool zKolejki(int& przycisk, uint32_t& trwaloMs);
  // Kolejka od nowa i przyciski trzymane teraz bez kolejki po puszczeniu (naciśnięcie, które przerwało pobieranie).
  void wyczysc();
  // Tylko kolejka od nowa (przed czekaniem na kablu, ADR-0047).
  void oproznijKolejke();
  // Rdzeń obsłużył ze stanu naciśnięcie przycisku `przycisk` (odczytane z wcisniete()): nie wraca z kolejki. Trzymany:
  // `pomin`. Już puszczony (puszczenie wypadło między odczytem stanu a tym wywołaniem, najwyżej
  // PUSZCZENIE_ZDJECIE_US temu): jego wpis zdjęty z końca kolejki.
  void obsluzone(int przycisk, int64_t tUs);
  // Bity przycisków wciśniętych po odsianiu drgań (droga ze stanu też bez drgań).
  uint8_t wcisniete() const;
  // Chwila wciśnięcia trzymanego przycisku; false = nie wciśnięty albo nieznana.
  bool wcisnietyOd(int przycisk, int64_t& odUs) const;
  // Dostęp dla wyboru przycisku kończącego czekanie (czekanie::przyciskKonczacy()) i dla testów.
  int ile() const { return ile_; }
  const Nacisniecie& wpis(int i) const { return q_[(glowa_ + i) % MIEJSC_KOLEJKI]; }
  void zdejmij(int n);  // n najstarszych z kolejki
  bool dopisz(const Nacisniecie& n);  // na koniec; false = pełna (tylko testy i atrapa)
  Klawisz& klawisz(int i) { return k_[i]; }
  const Klawisz& klawisz(int i) const { return k_[i]; }

 private:
  Klawisz k_[4];  // indeks = (int)Przycisk, 0 nieużywany
  Nacisniecie q_[MIEJSC_KOLEJKI];
  int glowa_ = 0, ile_ = 0;
};

}  // namespace przyciski_probki
