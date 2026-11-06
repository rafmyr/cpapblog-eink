// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h
#pragma once
typedef unsigned int UBaseType_t;
typedef struct {
  int owner;
} portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED {0}
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
#define portENTER_CRITICAL_ISR(m) ((void)(m))
#define portEXIT_CRITICAL_ISR(m) ((void)(m))
