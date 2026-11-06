// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include "raport_ekran.h"

#include "qr.h"

namespace {
const int X = 28, XP = 772;

void kreska(Plotno& p, int y, int g = 2, int x0 = 0, int x1 = 800) { p.prostokat(x0, y, x1 - x0, g, true); }
void naglowek(Plotno& p, const Naglowek& n) {  // jak naglowekDanych() w src/ekrany.cpp (SPEC-B 3)
  tekst(p, F_P24B, X, 35, n.lewo);
  if (!n.prawo.empty()) tekstPrawo(p, F_P24B, XP, 35, n.prawo);
  kreska(p, 52);
}
void stopka(Plotno& p, const std::string& lewo, std::vector<std::string> akcje) {  // jak stopkaAkcji()
  uporzadkujPrzyciski(akcje);
  kreska(p, 434);
  if (!lewo.empty()) tekst(p, F_P22B, X, 465, lewo);
  int w = 0;
  for (size_t i = 0; i < akcje.size(); i++) w += szer(F_P22B, akcje[i]) + (i ? 26 : 0);
  int x = XP - w;
  for (auto& a : akcje) x = tekst(p, F_P22B, x, 465, a) + 26;
}
bool tylkoCyfry(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || c == ',')) return false;
  return true;
}
}  // namespace

int rysujKodQr(Plotno& p, int x, int y, int bok, const std::string& tresc) {
  p.prostokat(x, y, bok, bok, false);
  qr::Kod k = qr::koduj(tresc);
  if (k.rozmiar == 0) return 0;
  int moduly = k.rozmiar + 2 * qr::STREFA_CISZY;
  int m = bok / moduly;
  if (m < 1) return 0;
  int x0 = x + (bok - k.rozmiar * m) / 2, y0 = y + (bok - k.rozmiar * m) / 2;
  for (int r = 0; r < k.rozmiar; r++)
    for (int c = 0; c < k.rozmiar; c++)
      if (k.ciemny(c, r)) p.prostokat(x0 + c * m, y0 + r * m, m, m, true);
  return m;
}

bool rysujRaportQr(Plotno& p, const DaneEkranuRaportu& d) {
  p.prostokat(0, 0, 800, 480, false);
  naglowek(p, d.naglowek);
  p.ramka(X, 68, 304, 304, 2);
  int m = rysujKodQr(p, X + 2, 70, 300, d.adresQr);
  const int XR = 360, WR = XP - XR;
  tekst(p, F_P22B, XR, 92, wersaliki("Raport dla lekarza"), true, 2);
  auto t = zawin(F_SB44, d.temat, WR);
  int y = 140;
  for (size_t i = 0; i < t.size() && i < 2; i++, y += 44) tekst(p, F_SB44, XR, y, t[i]);
  tekst(p, F_P22B, XR, y - 4, d.okres);
  y += 30;
  for (const char* k : {"1. Zeskanuj kod telefonem w domowym Wi-Fi.", "2. Pobierz PDF, wydrukuj lub wyślij.",
                        "3. Weź wydruk na wizytę."}) {
    for (auto& l : zawin(F_P22B, k, WR)) {
      tekst(p, F_P22B, XR, y, l);
      y += 25;
    }
    y += 3;
  }
  int yk = std::min(std::max(y + 6, 356), 370);
  tekst(p, F_P24B, XR, yk, d.kodWazny);
  if (!d.pobrano.empty()) tekst(p, F_P22B, XR, yk + 26, d.pobrano);  // osobno od `Starsze …`
  tekst(p, F_P22B, X, 422, "albo wpisz: " + d.adresReczny);
  if (d.starsze)
    stopka(p, d.stan, {"◀ wróć", "▶ dalej", "↻ ekran dnia"});
  else
    stopka(p, d.stan, {"◀ wróć", "▶ dalej", "↻ ekran dnia · 2 s starsze"});
  return m > 0;
}

bool rysujSciage(Plotno& p, const DaneSciagi& d) {
  p.prostokat(0, 0, 800, 480, false);
  naglowek(p, d.naglowek);
  tekst(p, F_P22B, X, 92, wersaliki("Raport dla lekarza"), true, 2);
  auto tt = zawin(F_SB44, d.temat, XP - X);
  if (tt.size() <= 1) {
    tekst(p, F_SB44, X, 140, tt.empty() ? "" : tt.front());
    auto q = zawin(F_P24B, d.pytanie, XP - X);
    int y = 182;
    for (size_t i = 0; i < q.size() && i < 2; i++, y += 31) tekst(p, F_P24B, X, y, q[i]);
  } else {  // temat w dwóch wierszach, pytanie mniejszym krojem
    tekst(p, F_SB44, X, 134, tt[0]);
    tekst(p, F_SB44, X, 176, tt[1]);
    auto q = zawin(F_P22B, d.pytanie, XP - X);
    int y = 206;
    for (size_t i = 0; i < q.size() && i < 2; i++, y += 24) tekst(p, F_P22B, X, y, q[i]);
  }
  kreska(p, 242, 2, X, 600);
  const int KX[3] = {X, 222, 446};
  for (int i = 0; i < 3; i++) {
    tekst(p, F_P22B, KX[i], 272, d.etykiety[i]);
    const GFXfont* f = tylkoCyfry(d.liczby[i]) ? F_SN96 : F_P34B;
    if (f == F_SN96 && szer(f, d.liczby[i]) > 186) f = F_SN56;
    tekst(p, f, KX[i], 356, d.liczby[i]);
  }
  tekst(p, F_P24B, X, 400, d.podpis);
  int m = 0;
  if (!d.adresQr.empty()) {
    p.ramka(624, 246, 148, 148, 2);
    m = rysujKodQr(p, 626, 248, 144, d.adresQr);
    tekstSrodek(p, F_P22B, 698, 422, "paczka EDF");
  }
  stopka(p, "", {"◀ wróć", "▶ dalej", "↻ ekran dnia"});
  return d.adresQr.empty() || m > 0;
}

void rysujRaportBezWifi(Plotno& p, const Naglowek& n, const std::string& temat, const std::string& sciezkaKarty) {
  p.prostokat(0, 0, 800, 480, false);
  naglowek(p, n);
  tekst(p, F_P22B, X, 92, wersaliki("Raport dla lekarza"), true, 2);
  tekst(p, F_SB44, X, 145, zawin(F_SB44, temat, XP - X).front());
  tekst(p, F_P28B, X, 195, "Nie mogę włączyć Wi-Fi");
  if (!sciezkaKarty.empty()) tekst(p, F_P22B, X, 235, sciezkaKarty);
  stopka(p, "", {"◀ wróć", "▶ dalej", "↻ ekran dnia"});
}
