// SPDX-License-Identifier: GPL-3.0-or-later
#include "ekrany_konfiguracji.h"

#include <algorithm>

#include "nawigacja.h"
#include "raport_ekran.h"

namespace {
const int X = 28, XP = 772;
}

void rysujUstawienia(Plotno& p, const std::vector<std::string>& poz, int kursor) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekEkranu(p, {"Ustawienia panelu", numerStronyTekst(Ekran::USTAWIENIA)});  // `6 / 6`
  const int Y0 = 80, H = 44, KROK = 56;
  const int n = (int)std::min<size_t>(poz.size(), 5);
  for (int i = 0; i < n; i++) {
    int y = Y0 + KROK * i;
    if (i == kursor) p.ramka(6, y - 6, 788, H + 12, 3, true);
    else if (i < n - 1 && i + 1 != kursor) p.prostokat(12, y + H + 5, 776, 1, true);
    tekst(p, F_P24S, X, y + 30, poz[(size_t)i]);
  }
  tekst(p, F_P22B, X, 406, "Te ustawienia dotyczą tylko panelu, nie aparatu.");
  stopkaEkranu(p, {"◀ sprzęt · 2 s sieć panelu", "▶ 2 s telefon", "↻ ekran dnia"}, "");
}

bool rysujFormularzQr(Plotno& p, const DaneFormularzaQr& d) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekEkranu(p, {"Ustawienia panelu", d.bezWifi || d.adresQr.empty() ? "" : "Wi-Fi włączone"});
  const int XR = 360, WR = XP - XR;
  int m = 1;
  if (!d.adresQr.empty()) {
    p.ramka(X, 68, 304, 304, 2);
    m = rysujKodQr(p, X + 2, 70, 300, d.adresQr);
  }
  int xr = d.adresQr.empty() ? X : XR, wr = d.adresQr.empty() ? XP - X : WR;
  tekst(p, F_P22B, xr, 92, wersaliki("Ustawienia"), true, 2);
  tekst(p, F_SB44, xr, 140, "Na telefonie");
  int y = 180;
  std::vector<std::string> kroki;
  if (d.bezWifi) kroki = {"Nie mogę włączyć Wi-Fi."};
  else if (!d.adresQr.empty())
    kroki = {"1. Zeskanuj kod telefonem w domowym Wi-Fi.", "2. Zmień ustawienia i naciśnij Zapisz."};
  for (auto& k : kroki) {
    for (auto& l : zawin(F_P22B, k, wr)) {
      tekst(p, F_P22B, xr, y, l);
      y += 25;
    }
    y += 3;
  }
  if (!d.stan.empty()) {
    y += 6;
    auto l = zawin(F_P24B, d.stan, wr);
    for (size_t i = 0; i < l.size() && i < 4; i++, y += 28) tekst(p, F_P24B, xr, y, l[i]);
  }
  int yk = std::min(std::max(y + 6, 356), 370);
  if (!d.kodWazny.empty()) tekst(p, F_P24B, xr, yk, d.kodWazny);
  if (!d.zapisano.empty()) tekst(p, F_P22B, xr, yk + 26, d.zapisano);
  if (!d.adresReczny.empty()) tekst(p, F_P22B, X, 422, "albo wpisz: " + d.adresReczny);
  stopkaEkranu(p, {"◀ ustawienia", "▶ 2 s test sprzętu", "↻ ekran dnia"}, "");  // U-03 i U-05 tylko przytrzymaniem ▶ (Wi-Fi)
  return m > 0;
}

std::string kodWifi(const std::string& ssid, const std::string& haslo) {
  auto ucieczka = [](const std::string& t) {
    std::string r;
    for (char c : t) {
      if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') r += '\\';
      r += c;
    }
    return r;
  };
  return "WIFI:T:WPA;S:" + ucieczka(ssid) + ";P:" + ucieczka(haslo) + ";;";
}

bool rysujSiecPanelu(Plotno& p, const DaneSieciPanelu& d) {
  p.prostokat(0, 0, 800, 480, false);
  bool kody = !d.nazwa.empty() && !d.adresQr.empty();
  naglowekEkranu(p, {"Sieć panelu", kody ? "Wi-Fi panelu włączone" : ""});
  if (!kody) {  // instrukcja pierwszego uruchomienia, łączenie z nową siecią albo radio bez sieci panelu
    tekst(p, F_P22B, X, 92, wersaliki("Konfiguracja sieci"), true, 2);
    tekst(p, F_SB44, X, 150, d.instrukcja ? "Panel nie ma sieci" : "Sieć panelu");
    int y = 200;
    std::vector<std::string> l;
    if (d.instrukcja)
      l = {"Przytrzymaj ◀ przez 2 s, żeby skonfigurować sieć.",
           "Panel utworzy własną sieć Wi-Fi i pokaże kody do zeskanowania telefonem."};
    if (d.bezRadia) l.push_back("Nie mogę włączyć sieci panelu. Spróbuj jeszcze raz za chwilę.");
    if (!d.stan.empty()) l.push_back(d.stan);
    for (auto& t : l) {
      for (auto& w : zawin(F_P24B, t, XP - X)) {
        tekst(p, F_P24B, X, y, w);
        y += 30;
      }
      y += 8;
    }
    if (d.instrukcja) stopkaEkranu(p, {"◀ 2 s konfiguracja", "▶ kokpit"}, "");
    else stopkaEkranu(p, {"◀ anuluj", "↻ ekran dnia"}, "");  // ↻ dom (też anuluje)
    return true;
  }
  const int B = 214, Y = 66, X2 = X + B + 18, XR = X2 + B + 22, WR = XP - XR;
  p.ramka(X, Y, B + 4, B + 4, 2);
  int m1 = rysujKodQr(p, X + 2, Y + 2, B, kodWifi(d.nazwa, d.haslo));
  p.ramka(X2, Y, B + 4, B + 4, 2);
  int m2 = rysujKodQr(p, X2 + 2, Y + 2, B, d.adresQr);
  tekst(p, F_P22B, X, Y + B + 30, "1. Dołącz do sieci");
  tekst(p, F_P22B, X2, Y + B + 30, "2. Otwórz formularz");
  int y = 92;
  tekst(p, F_P22B, XR, y, "Sieć:");
  tekst(p, F_P24B, XR, y + 30, d.nazwa);
  tekst(p, F_P22B, XR, y + 70, "Hasło:");
  tekst(p, F_P28B, XR, y + 104, d.haslo);
  y += 146;
  for (auto& w : zawin(F_P22B, d.wazne, WR)) {
    tekst(p, F_P22B, XR, y, w);
    y += 25;
  }
  int yd = Y + B + 62;  // pod kodami: wynik nieudanej próby (najwyżej 3 wiersze) albo zdanie o zapisie
  if (!d.stan.empty()) {
    auto l = zawin(F_P22B, d.stan, XP - X);
    for (size_t i = 0; i < l.size() && i < 3; i++, yd += 25) tekst(p, F_P22B, X, yd, l[i]);
  } else {
    tekst(p, F_P22B, X, yd, "Po zapisie panel wyłączy swoją sieć i połączy się z nową.");
    yd += 28;
  }
  tekst(p, F_P22B, X, yd + 2, "albo wpisz: " + d.adresReczny);
  stopkaEkranu(p, {"◀ anuluj", "↻ ekran dnia · 2 s nowe hasło"}, "");
  return m1 > 0 && m2 > 0;
}
