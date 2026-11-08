// SPDX-License-Identifier: GPL-3.0-or-later
// Atrapa (tylko -fsyntax-only): wzór plus wartości zastępcze, nie prawdziwe hasła.
#pragma once
#include "../../../include/secrets.example.h"
// We wzorze wszystkie linie są zakomentowane; tu ustawione, żeby sprawdzenie składni objęło też gałęzie
// z siecią, AirBridge, oknem serwisowym, źródłem otoczenia i prognozą.
#ifndef WIFI_SSID
#define WIFI_SSID "atrapa"
#define WIFI_PASS "atrapa"
#endif
#ifndef AB_IP
#define AB_IP "192.0.2.1"
#define AB_USER "atrapa"
#define AB_PASS "atrapa"
#endif
#ifndef OTA_PASS
#define OTA_PASS "atrapa-hasla-okna-wgrywania-32zn"  // co najmniej 16 znaków (static_assert w main.cpp)
#endif
#ifndef OTO_URL
#define OTO_URL "http://192.0.2.1:8080/otoczenie"
#define OTO_TOKEN "atrapa"
#endif
#ifndef POGODA_LAT
#define POGODA_LAT 0.0
#define POGODA_LON 0.0
#endif
