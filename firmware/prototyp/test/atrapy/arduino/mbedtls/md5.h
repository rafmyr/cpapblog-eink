// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only), zob. Arduino.h; na panelu mbedtls z ESP-IDF 5 (mbedtls 3.x)
#pragma once
#include <cstddef>
int mbedtls_md5(const unsigned char* input, size_t ilen, unsigned char output[16]);
