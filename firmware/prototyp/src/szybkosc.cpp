// SPDX-License-Identifier: GPL-3.0-or-later
#include "szybkosc.h"

namespace szybkosc {
bool bezPamieci = false;
unsigned long przebiegiSilnika = 0;
void (*haczykFazy)(const char* nazwa) = nullptr;
}  // namespace szybkosc
