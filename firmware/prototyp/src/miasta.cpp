// SPDX-License-Identifier: GPL-3.0-or-later
#include "miasta.h"

namespace miasta {

// Źródło: GeoNames, plik PL.txt (https://download.geonames.org/export/dump/PL.zip, licencja CC BY 4.0), pobrany
// 05.11.2026; 18 miast wojewódzkich (siedziby wojewodów i sejmików, czyli w województwach
// kujawsko-pomorskim i lubuskim oba miasta), współrzędne centrum miasta według GeoNames zaokrąglone do 0,01°. Nazwy po polsku (GeoNames: `Warsaw` = Warszawa).
const Miasto MIASTA[] = {
    {"Białystok", 53.13f, 23.16f},
    {"Bydgoszcz", 53.12f, 18.01f},
    {"Gdańsk", 54.35f, 18.65f},
    {"Gorzów Wielkopolski", 52.73f, 15.23f},
    {"Katowice", 50.26f, 19.02f},
    {"Kielce", 50.87f, 20.63f},
    {"Kraków", 50.06f, 19.94f},
    {"Lublin", 51.25f, 22.57f},
    {"Łódź", 51.77f, 19.47f},
    {"Olsztyn", 53.78f, 20.49f},
    {"Opole", 50.67f, 17.93f},
    {"Poznań", 52.41f, 16.93f},
    {"Rzeszów", 50.04f, 22.00f},
    {"Szczecin", 53.43f, 14.55f},
    {"Toruń", 53.01f, 18.60f},
    {"Warszawa", 52.23f, 21.01f},
    {"Wrocław", 51.10f, 17.03f},
    {"Zielona Góra", 51.94f, 15.51f},
};
const int MIASTA_N = (int)(sizeof(MIASTA) / sizeof(MIASTA[0]));

const Miasto* znajdz(const std::string& nazwa) {
  for (int i = 0; i < MIASTA_N; i++)
    if (nazwa == MIASTA[i].nazwa) return &MIASTA[i];
  return nullptr;
}

bool wPolsce(double szer, double dl) {
  return szer >= 48.9 && szer <= 55.0 && dl >= 14.0 && dl <= 24.3;
}

}  // namespace miasta
