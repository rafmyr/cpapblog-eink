// SPDX-License-Identifier: GPL-3.0-or-later
// Próbkowanie przycisków i kolejka naciśnięć (src/przyciski_probki.h, R-PRZ-20).
#include "przyciski_probki.h"

namespace przyciski_probki {

bool chwilaWPrzebiegu(int64_t odUs, uint32_t poczatekMs, uint32_t& odMs) {
  if (odUs == 0) return false;
  uint32_t wzglednie = (uint32_t)(odUs / 1000) - poczatekMs;
  odMs = (int32_t)wzglednie < 0 ? 0u : wzglednie;
  return true;
}

void Model::start(uint8_t surowe) {
  ile_ = glowa_ = 0;
  for (int i = 1; i <= 3; i++) {
    k_[i] = Klawisz();
    k_[i].wcisniety = k_[i].pomin = ((surowe >> i) & 1u) != 0;
  }
}

ZmianyProbki Model::probka(uint8_t surowe, int64_t tUs) {
  ZmianyProbki z;
  for (int i = 1; i <= 3; i++) {
    Klawisz& k = k_[i];
    bool teraz = ((surowe >> i) & 1u) != 0;
    if (teraz == k.wcisniety) {
      k.zgodnych = 0;
      continue;
    }
    if (++k.zgodnych < PROBEK_STANU) continue;
    k.zgodnych = 0;
    k.wcisniety = teraz;
    const uint8_t bit = (uint8_t)(1u << i);
    if (teraz) {
      k.odUs = tUs;
      if (!k.pomin) z.piski = (uint8_t)(z.piski | bit);
      continue;
    }
    if (!k.pomin && k.odUs != 0) {
      z.puszczone = (uint8_t)(z.puszczone | bit);
      Nacisniecie n;
      n.przycisk = (uint8_t)i;
      n.trwaloMs = (uint32_t)((tUs - k.odUs) / 1000);
      n.puszczonyUs = tUs;
      dopisz(n);
    }
    k.odUs = 0;
    k.pomin = false;
  }
  return z;
}

bool Model::zKolejki(int& przycisk, uint32_t& trwaloMs) {
  if (ile_ == 0) return false;
  przycisk = q_[glowa_].przycisk;
  trwaloMs = q_[glowa_].trwaloMs;
  zdejmij(1);
  return true;
}

void Model::wyczysc() {
  oproznijKolejke();
  for (int i = 1; i <= 3; i++)
    if (k_[i].wcisniety) k_[i].pomin = true;
}

void Model::oproznijKolejke() { ile_ = glowa_ = 0; }

void Model::obsluzone(int przycisk, int64_t tUs) {
  if (przycisk < 1 || przycisk > 3) return;
  Klawisz& k = k_[przycisk];
  if (k.wcisniety) {
    k.pomin = true;  // puszczenie (także zaciętego) nie trafi do kolejki
    return;
  }
  if (ile_ == 0) return;
  const Nacisniecie& ost = wpis(ile_ - 1);
  if (ost.przycisk == przycisk && tUs - ost.puszczonyUs <= PUSZCZENIE_ZDJECIE_US) ile_--;
}

uint8_t Model::wcisniete() const {
  uint8_t b = 0;
  for (int i = 1; i <= 3; i++)
    if (k_[i].wcisniety) b = (uint8_t)(b | (1u << i));
  return b;
}

bool Model::wcisnietyOd(int przycisk, int64_t& odUs) const {
  if (przycisk < 1 || przycisk > 3 || !k_[przycisk].wcisniety || k_[przycisk].odUs == 0) return false;
  odUs = k_[przycisk].odUs;
  return true;
}

void Model::zdejmij(int n) {
  if (n <= 0) return;
  if (n >= ile_) {
    oproznijKolejke();
    return;
  }
  glowa_ = (glowa_ + n) % MIEJSC_KOLEJKI;
  ile_ -= n;
}

bool Model::dopisz(const Nacisniecie& n) {
  if (ile_ >= MIEJSC_KOLEJKI) return false;
  q_[(glowa_ + ile_) % MIEJSC_KOLEJKI] = n;
  ile_++;
  return true;
}

}  // namespace przyciski_probki
