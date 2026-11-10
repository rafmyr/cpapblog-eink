// SPDX-License-Identifier: GPL-3.0-or-later
#include "ekrany_konfiguracji.h"

#include <algorithm>
#include <cstdio>

#include "nawigacja.h"
#include "raport_ekran.h"

namespace {
const int X = 28, XP = 772;
}

const char* const U01_PODPOWIEDZ[2] = {"Więcej na telefonie: ▶ 2 s (zmiana ustawień).",
                                       "Test sprzętu: na formularzu jeszcze raz ▶ 2 s."};
const char* const U01_GRANICA = "Te ustawienia dotyczą tylko panelu, nie aparatu.";

namespace {
std::string hhmmU01(int m) {
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", (m / 60) % 24, m % 60);
  return b;
}
// skrócenie z `…` do szerokości w P24B (całe znaki UTF-8)
std::string dopasujU01(std::string w) {
  if (szer(F_P24B, w) <= U01_SZER_WARTOSCI) return w;
  while (!w.empty() && szer(F_P24B, w + "…") > U01_SZER_WARTOSCI) {
    size_t k = w.size() - 1;
    while (k > 0 && ((unsigned char)w[k] & 0xC0) == 0x80) k--;
    w.erase(k);
  }
  return w + "…";
}
}  // namespace

const char* const MIASTO_ZE_WSPOLRZEDNYCH = "współrzędne z wgrania";

std::vector<WierszUstawien> wierszeUstawien(const DaneUstawien& d) {
  std::vector<WierszUstawien> w;
  w.push_back({"Sieć Wi-Fi", d.ssid.empty() ? "nie ustawiona" : d.ssid});
  w.push_back({"AirBridge", d.abAdres.empty() ? "nie ustawiony" : d.abAdres});
  w.push_back({"Miasto pogody", d.miasto.empty() ? "nie wybrano" : d.miasto});
  w.push_back({"Profil", d.profilPubliczny ? "publiczny" : "prywatny"});
  w.push_back({"Humor", d.humor == 0 ? "wyłączony" : d.humor == 1 ? "oszczędny" : "normalny"});
  w.push_back({"Dźwięk", d.dzwiek ? "włączony, cisza " + hhmmU01(d.ciszaOd) + "–" + hhmmU01(d.ciszaDo) : "wyłączony"});
  w.push_back({"Podpowiedź „co do poprawy”", d.podpowiedz ? "włączona" : "wyłączona"});
  w.push_back({"Czujnik CO2", d.co2 < 0 ? "brak" : d.co2 ? "jest, ekrany CO2 włączone" : "jest, ekrany CO2 wyłączone"});
  w.push_back({"Oprogramowanie", d.wersja.empty() ? "–" : d.wersja});
  for (auto& x : w) x.wartosc = dopasujU01(x.wartosc);
  return w;
}

std::vector<std::string> stopkaU01() {
  return {"◀ sprzęt · 2 s sieć", "▶ 2 s telefon", "↻ ekran dnia · 2 s aktualizacja"};
}

// Układ (skala B): nazwa P22B od x 28, wartość P24B od x 350, wiersze co 28 px od linii bazowej 84 (9 wierszy do 308);
// pod listą cztery linie P22B co 26 px: aktualizacja (338), podpowiedź (364, 390) i zdanie granicy (416), stopka od 434.
// Stopka z karty aktualizacji (`◀ sprzęt · 2 s sieć panelu …`) ma 790 px > 744 px, stąd `sieć`. Podpowiedź pod listą
// nazywa formularz `Więcej na telefonie`, bo tak odsyłają do niego wiersz aktualizacji i ekran aktualizacji.
void rysujUstawienia(Plotno& p, const DaneUstawien& d) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekEkranu(p, {"Ustawienia panelu", numerStronyTekst(Ekran::USTAWIENIA)});  // `6 / 6`
  std::vector<WierszUstawien> w = wierszeUstawien(d);
  for (size_t i = 0; i < w.size(); i++) {
    int y = 84 + 28 * (int)i;
    tekst(p, F_P22B, X, y, w[i].nazwa);
    tekst(p, F_P24B, U01_X_WARTOSCI, y, w[i].wartosc);
  }
  if (!d.aktualizacja.empty()) tekst(p, F_P22B, X, 338, d.aktualizacja);
  tekst(p, F_P22B, X, 364, U01_PODPOWIEDZ[0]);
  tekst(p, F_P22B, X, 390, U01_PODPOWIEDZ[1]);
  tekst(p, F_P22B, X, 416, U01_GRANICA);
  stopkaEkranu(p, stopkaU01(), "");
}

void rysujAktualizacje(Plotno& p, const std::vector<std::string>& linie, const std::vector<std::string>& stopka) {
  p.prostokat(0, 0, 800, 480, false);
  naglowekEkranu(p, {"Aktualizacja", ""});
  int y = 110;
  for (size_t i = 0; i < linie.size(); i++) {
    const GFXfont* f = i == 0 ? F_P28B : F_P24S;
    for (auto& w : zawin(f, linie[i], XP - X)) {
      if (y > 420) break;
      tekst(p, f, X, y, w);
      y += i == 0 ? 40 : 36;
    }
    y += 10;
  }
  if (!stopka.empty()) stopkaEkranu(p, stopka, "");
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
