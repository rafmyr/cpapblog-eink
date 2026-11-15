// SPDX-License-Identifier: GPL-3.0-or-later
#include "ekrany_konfiguracji.h"

#include <algorithm>
#include <cstdio>

#include "nawigacja.h"
#include "raport_ekran.h"

namespace {
const int X = 28, XP = 772;
}

const char* const U01_KAFELEK_TYTUL[2] = {"◀ Sieć panelu", "▶ Więcej na telefonie"};
const char* const U01_KAFELEK_OPIS[2][2] = {{"gdy zmieniasz Wi-Fi", "albo AirBridge"},
                                            {"zmiana ustawień,", "test sprzętu: tam ▶ 2 s"}};
const char* const U01_GRANICA = "Te ustawienia dotyczą tylko panelu, nie aparatu.";

namespace {
std::string hhmmU01(int m) {
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", (m / 60) % 24, m % 60);
  return b;
}
// skrócenie z `…` do szerokości w kroju `f` (całe znaki UTF-8)
std::string skrocZnakami(const GFXfont* f, std::string w, int maxW) {
  if (szer(f, w) <= maxW) return w;
  while (!w.empty() && szer(f, w + "…") > maxW) {
    size_t k = w.size() - 1;
    while (k > 0 && ((unsigned char)w[k] & 0xC0) == 0x80) k--;
    w.erase(k);
  }
  return w + "…";
}
// Układ U-01 (makieta 14.11.2026): nazwy kolumn P16 na 70, kreska 2 px na 76; wiersze co 36 px od 78 (linia bazowa
// +25, kreska 1 px pod wierszem poza ostatnim); `PRZYTRZYMAJ 2 S` P16 na 246; kafelki 254…349 (linie bazowe 282, 306,
// 330); zdanie granicy P22B na 392; stopka od 434.
const int U01_BAZA_KOLUMNY = 70, U01_KRESKA_KOLUMNY = 76, U01_WIERSZ0 = 78, U01_WIERSZ = 36, U01_BAZA_WIERSZA = 25;
const int U01_BAZA_PRZYTRZYMAJ = 246, U01_KAFELEK_Y = 254, U01_KAFELEK_H = 96, U01_KAFELEK_ODSTEP = 10;
const int U01_KAFELEK_WCIECIE = 8, U01_KAFELEK_BAZA = 28, U01_KAFELEK_LINIA = 24, U01_BAZA_GRANICY = 392;
int wnetrzeKafelka(int i) { return U01_KAFELEK_W[i] - 2 * (2 + U01_KAFELEK_WCIECIE); }

void kolumnaU01(Plotno& p, int x0, int w, const char* nazwa, const std::vector<WierszUstawien>& r, size_t od,
                size_t n) {
  tekst(p, F_P16, x0, U01_BAZA_KOLUMNY, nazwa, true, 1);
  p.prostokat(x0, U01_KRESKA_KOLUMNY, w, 2, true);
  for (size_t i = 0; i < n && od + i < r.size(); i++) {
    int y0 = U01_WIERSZ0 + (int)i * U01_WIERSZ;
    tekst(p, F_P20, x0, y0 + U01_BAZA_WIERSZA, r[od + i].nazwa);
    tekstPrawo(p, F_P20B, x0 + w, y0 + U01_BAZA_WIERSZA, r[od + i].wartosc);
    if (i + 1 < n) p.prostokat(x0, y0 + U01_WIERSZ - 1, w, 1, true);
  }
}
}  // namespace

const char* const MIASTO_ZE_WSPOLRZEDNYCH = "współrzędne z wgrania";

std::vector<WierszUstawien> wierszeUstawien(const DaneUstawien& d) {
  std::string cisza = hhmmU01(d.ciszaOd) + "–" + hhmmU01(d.ciszaDo);
  std::string co2 = d.co2 ? "włączone" : "wyłączone";
  struct W {
    const char* nazwa;
    std::vector<std::string> warianty;
  };
  std::vector<W> w = {
      {"Sieć Wi-Fi", {d.ssid.empty() ? "nie ustawiona" : d.ssid}},
      {"AirBridge", {d.abAdres.empty() ? "nie ustawiony" : d.abAdres}},
      {"Miasto pogody",
       d.miasto == MIASTO_ZE_WSPOLRZEDNYCH ? std::vector<std::string>{d.miasto, "współrzędne"}
                                           : std::vector<std::string>{d.miasto.empty() ? "nie wybrano" : d.miasto}},
      {"Czujnik CO2", d.co2 < 0 ? std::vector<std::string>{"brak"}
                                : std::vector<std::string>{"jest, ekrany CO2 " + co2, "jest, ekrany " + co2}},
      {"Profil", {d.profilPubliczny ? "publiczny" : "prywatny"}},
      {"Humor", {d.humor == 0 ? "wyłączony" : d.humor == 1 ? "oszczędny" : "normalny"}},
      {"Dźwięk", d.dzwiek ? std::vector<std::string>{"włączony, cisza " + cisza, "cisza " + cisza}
                          : std::vector<std::string>{"wyłączony"}},
      {"Podpowiedź „co do poprawy”", {d.podpowiedz ? "włączona" : "wyłączona"}},
  };
  std::vector<WierszUstawien> r;
  for (size_t i = 0; i < w.size(); i++) {
    int kol = (int)i < U01_WIERSZY_LEWA ? U01_KOLUMNA_L : U01_KOLUMNA_P;
    r.push_back({w[i].nazwa, dopasujWartosc(w[i].nazwa, w[i].warianty, kol)});
  }
  return r;
}

std::vector<std::string> stopkaU01(bool sprawy, bool liczby, bool sypialnia) {
  // ◀ strona przed U-01 w ciągu (R-KOK-35: jedno miejsce prawdy w nawigacji)
  return {"◀ " + podpisCelu(Ekran::USTAWIENIA, false, kontekstStopki(sprawy, liczby, sypialnia)), "↻ ekran dnia"};
}

std::vector<std::string> linieKafelkaOprogramowania(const DaneUstawien& d, std::string& tytul) {
  const int m = wnetrzeKafelka(2);
  std::string w = d.wersja.empty() ? "–" : d.wersja;
  std::vector<std::string> l;
  tytul = "↻ Oprogramowanie " + w;
  if (szer(F_P20B, tytul) > m) {  // długa wersja: w drugiej linii, stan w jednej
    tytul = "↻ Oprogramowanie";
    l.push_back(skrocZnakami(F_P20B, w, m));
  }
  std::string st = d.stanOprogramowania.empty() ? "sprawdź aktualizację" : d.stanOprogramowania;
  int ms = m - (d.stanOdwrocony ? 8 : 0);
  // jedna linia na stan (długa wersja): `jest {X} · zainstaluj` → `jest {X} · instaluj`, gdy się nie mieści; czynność
  // nie znika, najwyżej skraca się wersja w stanie (`jest {X…} · instaluj`)
  const std::string zainstaluj = " · zainstaluj";
  if (!l.empty() && szer(F_P20, st) > ms && st.size() > zainstaluj.size() &&
      st.compare(st.size() - zainstaluj.size(), zainstaluj.size(), zainstaluj) == 0) {
    std::string pocz = st.substr(0, st.size() - zainstaluj.size());
    const std::string ogon = " · instaluj";
    if (szer(F_P20, pocz + ogon) > ms) {
      while (!pocz.empty() && szer(F_P20, std::string(pocz).append("…").append(ogon)) > ms) {
        size_t k = pocz.size() - 1;
        while (k > 0 && ((unsigned char)pocz[k] & 0xC0) == 0x80) k--;
        pocz.erase(k);
      }
      pocz += "…";
    }
    st = pocz + ogon;
  }
  std::vector<std::string> z = zawin(F_P20, st, ms);
  size_t wolne = 3 - 1 - l.size();
  for (size_t i = 0; i < z.size() && i < wolne; i++)
    l.push_back(i + 1 == wolne && i + 1 < z.size() ? skrocZnakami(F_P20, z[i] + "…", ms) : z[i]);
  return l;
}

void rysujUstawienia(Plotno& p, const DaneUstawien& d) {
  p.prostokat(0, 0, 800, 480, false);
  // `Ustawienia 3 / 3`; znana nowsza wersja: znak nowej wersji w prawym polu belki, jak na każdym ekranie z belką
  // (R-EKR-69, od 14.11.2026 zamiast tekstu `jest aktualizacja ·`; znakiBelki() ustawia dyrygent)
  belka(p, {nazwaStronyKokpitu("Ustawienia", numerStronyTekst(Ekran::USTAWIENIA)), ""});
  std::vector<WierszUstawien> w = wierszeUstawien(d);
  kolumnaU01(p, X, U01_KOLUMNA_L, "POŁĄCZENIA", w, 0, U01_WIERSZY_LEWA);
  kolumnaU01(p, XP - U01_KOLUMNA_P, U01_KOLUMNA_P, "PANEL", w, U01_WIERSZY_LEWA, w.size() - U01_WIERSZY_LEWA);
  // pionowa kreska między kolumnami, od nazw kolumn do dołu ostatniego wiersza
  p.prostokat(U01_KRESKA_X, U01_BAZA_KOLUMNY - 14, U01_KRESKA_W,
              U01_WIERSZ0 + U01_WIERSZY_LEWA * U01_WIERSZ - (U01_BAZA_KOLUMNY - 14), true);
  // `PRZYTRZYMAJ` krojem nazw kolumn (P16 ma tylko wersaliki), `2 s` jak w stopkach (P20)
  int xp = tekst(p, F_P16, X, U01_BAZA_PRZYTRZYMAJ, "PRZYTRZYMAJ", true, 1);
  tekst(p, F_P20, xp + 6, U01_BAZA_PRZYTRZYMAJ, "2 s");
  int x = X;
  for (int i = 0; i < 3; i++) {
    int kw = U01_KAFELEK_W[i], xt = x + 2 + U01_KAFELEK_WCIECIE, b = U01_KAFELEK_Y + U01_KAFELEK_BAZA;
    p.ramka(x, U01_KAFELEK_Y, kw, U01_KAFELEK_H, 2);
    if (i < 2) {
      tekst(p, F_P20B, xt, b, U01_KAFELEK_TYTUL[i]);
      for (int k = 0; k < 2; k++) tekst(p, F_P20, xt, b + (k + 1) * U01_KAFELEK_LINIA, U01_KAFELEK_OPIS[i][k]);
    } else {
      std::string tytul;
      std::vector<std::string> l = linieKafelkaOprogramowania(d, tytul);
      tekst(p, F_P20B, xt, b, tytul);
      bool dlugaWersja = tytul == "↻ Oprogramowanie" && !l.empty();
      for (size_t k = 0; k < l.size(); k++) {
        int y = b + (int)(k + 1) * U01_KAFELEK_LINIA;
        bool stan = !(dlugaWersja && k == 0);
        if (stan && d.stanOdwrocony) {  // `jest {X} · zainstaluj` w odwróconym polu (wymaga uwagi)
          p.prostokat(xt, y - 17, szer(F_P20, l[k]) + 8, 23, true);
          tekst(p, F_P20, xt + 4, y, l[k], false);
        } else {
          tekst(p, stan ? F_P20 : F_P20B, xt, y, l[k]);
        }
      }
    }
    x += kw + U01_KAFELEK_ODSTEP;
  }
  tekst(p, F_P22B, X, U01_BAZA_GRANICY, U01_GRANICA);
  stopkaEkranu(p, stopkaU01(d.sprawy, d.profilPubliczny), "");
}

// ---------- U-07 „Aktualizacja” ----------
namespace {
// gruba linia: odcinki przesunięte w kwadracie g × g (1 bit, bez wygładzania)
void liniaGruba(Plotno& p, int x0, int y0, int x1, int y1, int g, bool cz = true) {
  for (int dy = -g / 2; dy < g - g / 2; dy++)
    for (int dx = -g / 2; dx < g - g / 2; dx++) p.linia(x0 + dx, y0 + dy, x1 + dx, y1 + dy, cz);
}
void pierscien(Plotno& p, int cx, int cy, int r, int g) {
  p.kolo(cx, cy, r, true, true);
  p.kolo(cx, cy, r - g, true, false);
}
const uint32_t ZLY_ZNAK = 0xFFFFFFFFu;
// UTF-8 → punkt kodowy; niepoprawny bajt (bez kontynuacji, ucięta sekwencja, zapis nadmiarowy, surogat, > U+10FFFF)
// = ZLY_ZNAK i przesunięcie o 1 bajt
uint32_t nastepnyZnak(const std::string& t, size_t& i) {
  unsigned char c = (unsigned char)t[i];
  int n = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
  if (n < 0 || i + (size_t)n >= t.size() + (n ? 0 : 1)) {
    i++;
    return ZLY_ZNAK;
  }
  uint32_t cp = n == 0 ? c : n == 1 ? (c & 0x1Fu) : n == 2 ? (c & 0x0Fu) : (c & 0x07u);
  for (int k = 1; k <= n; k++) {
    unsigned char d = (unsigned char)t[i + (size_t)k];
    if ((d & 0xC0) != 0x80) {
      i++;
      return ZLY_ZNAK;
    }
    cp = (cp << 6) | (d & 0x3Fu);
  }
  static const uint32_t MIN[4] = {0, 0x80, 0x800, 0x10000};
  if (cp < MIN[n] || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
    i++;
    return ZLY_ZNAK;
  }
  i += (size_t)n + 1;
  return cp;
}
}  // namespace

std::string liniaCoNowego(const std::string& t, int maxW) {
  std::string w;
  for (size_t i = 0; i < t.size();) {
    size_t a = i;
    uint32_t cp = nastepnyZnak(t, i);
    if (cp == ZLY_ZNAK) continue;  // niepoprawny bajt UTF-8 pominięty, nie trafia do rysowania
    // znak spoza krojów pominięty; ikony przycisków (◀ ▶ ↻) i romb też: w opisie wydania nie są przyciskami
    if (cp < 0x20 || !znakJestWKroju(cp) || cp == 0x25C0 || cp == 0x25B6 || cp == 0x21BB || cp == 0x25C6) continue;
    if (cp == ' ' && (w.empty() || w.back() == ' ')) continue;  // bez podwójnych spacji po pominiętych znakach
    w += t.substr(a, i - a);
  }
  while (!w.empty() && w.back() == ' ') w.pop_back();
  if (szer(F_P22B, w) <= maxW) return w;
  std::string s = skrocDoSlow(F_P22B, w, maxW - szer(F_P22B, "…"));
  return s.empty() ? skrocZnakami(F_P22B, w, maxW) : s + "…";
}

void rysujObrazekU07(Plotno& p, int x, int y, aktualizacja::ObrazekU07 o) {
  using aktualizacja::ObrazekU07;
  // panel: obudowa 160 × 110 kreską 5 px, ekran 132 × 66 kreską 2 px, trzy przyciski
  p.ramka(x + 10, y + 76, 160, 110, 5);
  p.ramka(x + 24, y + 90, 132, 66, 2);
  for (int k = 0; k < 3; k++) p.kolo(x + 36 + 14 * k, y + 172, 3, true);
  const int cx = x + 90;
  switch (o) {
    case ObrazekU07::PANEL:
    case ObrazekU07::NOWA:
      for (int k = 0; k < 3; k++) {  // linie tekstu na ekranie panelu
        int dl[3] = {74, 110, 50};
        p.prostokat(x + 36, y + 104 + 16 * k, dl[k], 5, true);
      }
      if (o == ObrazekU07::NOWA) {  // strzałka w dół nad panelem
        p.prostokat(cx - 3, y + 4, 7, 42, true);
        p.trojkat(cx - 22, y + 40, cx + 22, y + 40, cx, y + 64, true);
      }
      break;
    case ObrazekU07::W_TOKU:  // „w toku” bez skali: strzałka z góry wchodzi w ekran panelu, grot na ekranie
      // trzon przez obudowę (przerwa w ramce obudowy i ekranu pod strzałką), grot na ekranie, trzy kropki pod nim
      p.prostokat(cx - 9, y + 76, 19, 5, false);
      p.prostokat(cx - 9, y + 90, 19, 2, false);
      p.prostokat(cx - 3, y + 4, 7, 96, true);
      p.trojkat(cx - 22, y + 96, cx + 22, y + 96, cx, y + 122, true);
      for (int k = -1; k <= 1; k++) p.kolo(cx + 20 * k, y + 140, 4, true);
      break;
    case ObrazekU07::UDANA:  // promienie nad panelem i odhaczenie na ekranie
      liniaGruba(p, cx, y + 8, cx, y + 40, 5);
      liniaGruba(p, x + 48, y + 24, x + 66, y + 50, 5);
      liniaGruba(p, x + 132, y + 24, x + 114, y + 50, 5);
      liniaGruba(p, x + 18, y + 56, x + 48, y + 68, 5);
      liniaGruba(p, x + 162, y + 56, x + 132, y + 68, 5);
      liniaGruba(p, x + 66, y + 122, x + 84, y + 140, 8);
      liniaGruba(p, x + 84, y + 140, x + 118, y + 102, 8);
      break;
    case ObrazekU07::NIEUDANA:  // przerwana kreska nad panelem i krzyżyk na ekranie
      p.prostokat(cx - 3, y + 6, 7, 16, true);
      p.prostokat(cx - 3, y + 32, 7, 16, true);
      liniaGruba(p, x + 70, y + 102, x + 110, y + 144, 8);
      liniaGruba(p, x + 110, y + 102, x + 70, y + 144, 8);
      break;
  }
}

void rysujAktualizacje(Plotno& p, const aktualizacja::TresciU07& t) {
  using aktualizacja::KrokU07;
  p.prostokat(0, 0, 800, 480, false);
  belka(p, {"Aktualizacja", t.stanBelki});
  const int dol = t.pas.empty() ? 432 : U07_PAS_Y - 4;
  const int W = XP - U07_X;
  // prawa kolumna: najpierw policz wysokość, potem wyśrodkuj w pasie 44…dol
  const GFXfont* fw = nullptr;
  std::string wersje;
  if (!t.wersjaZ.empty() && !t.wersjaNa.empty()) {
    wersje = t.wersjaZ + " → " + t.wersjaNa;
    for (const GFXfont* f : {F_P34B, F_P28B, F_P24B})
      if (!fw && szer(f, wersje) <= W) fw = f;
  }
  std::vector<std::string> glowne = t.glowne.empty() ? std::vector<std::string>{} : zawin(F_P28B, t.glowne, W);
  std::vector<std::string> opis;
  for (auto& z : t.opis)
    for (auto& l : zawin(F_P24S, z, W)) opis.push_back(l);
  std::vector<std::string> nowe;
  for (auto& z : t.coNowego) {
    std::string l = liniaCoNowego(z, W - 22);
    if (!l.empty()) nowe.push_back(l);
  }
  int h = 0;
  if (!wersje.empty()) h += fw ? 44 : 84;
  h += 36 * (int)glowne.size();
  if (h && !opis.empty()) h += 14;
  h += 32 * (int)opis.size();
  if (!nowe.empty()) h += (h ? 18 : 0) + 26 + 28 * (int)nowe.size();
  if (t.kroki) h += (h ? 22 : 0) + 64;
  int y = 44 + std::max(8, (dol - 44 - h) / 2);
  rysujObrazekU07(p, U07_OBRAZEK_X, 44 + std::max(0, (dol - 44 - U07_OBRAZEK_H) / 2), t.obrazek);
  bool cos = false;
  if (!wersje.empty()) {
    if (fw) {
      tekst(p, fw, U07_X, y + 34, wersje);
      y += 44;
    } else {  // bardzo długie wersje: dwie linie
      tekst(p, F_P28B, U07_X, y + 30, t.wersjaZ);
      tekst(p, F_P28B, U07_X, y + 70, "→ " + t.wersjaNa);
      y += 84;
    }
    cos = true;
  }
  for (auto& l : glowne) {
    tekst(p, F_P28B, U07_X, y + 28, l);
    y += 36;
    cos = true;
  }
  if (cos && !opis.empty()) y += 14;
  for (auto& l : opis) {
    tekst(p, F_P24S, U07_X, y + 24, l);
    y += 32;
    cos = true;
  }
  if (!nowe.empty()) {
    if (cos) y += 18;
    tekst(p, F_P16, U07_X, y + 14, "CO NOWEGO", true, 1);
    p.prostokat(U07_X, y + 20, W, 2, true);
    y += 26;
    for (auto& l : nowe) {
      p.prostokat(U07_X + 2, y + 12, 7, 7, true);
      tekst(p, F_P22B, U07_X + 22, y + 22, l);
      y += 28;
    }
    cos = true;
  }
  if (t.kroki) {  // cztery kroki: koło 36 px (pierścień 4 px teraz, 2 px czeka; pełne z odhaczeniem albo krzyżykiem)
    if (cos) y += 22;
    const int kol = W / aktualizacja::KROKI_U07, r = 18;
    for (int k = 0; k < aktualizacja::KROKI_U07; k++) {
      int cx = U07_X + kol * k + kol / 2, cy = y + r;
      KrokU07 st = t.krok[k];
      if (st == KrokU07::ZROBIONY) {
        p.kolo(cx, cy, r, true, true);
        liniaGruba(p, cx - 8, cy, cx - 3, cy + 6, 3, false);
        liniaGruba(p, cx - 3, cy + 6, cx + 8, cy - 7, 3, false);
      } else if (st == KrokU07::NIEUDANY) {
        p.prostokat(cx - r, cy - r, 2 * r, 2 * r, true);
        liniaGruba(p, cx - 8, cy - 8, cx + 8, cy + 8, 3, false);
        liniaGruba(p, cx + 8, cy - 8, cx - 8, cy + 8, 3, false);
      } else {
        pierscien(p, cx, cy, r, st == KrokU07::TERAZ ? 4 : 2);
        tekstSrodek(p, F_P20B, cx, cy + 7, std::to_string(k + 1));
      }
      bool wyr = st == KrokU07::TERAZ || st == KrokU07::NIEUDANY;
      tekstSrodek(p, wyr ? F_P20B : F_P20, cx, y + 2 * r + 24, aktualizacja::NAZWY_KROKOW_U07[k]);
    }
  }
  if (!t.pas.empty()) {  // pas w negatywie: wymaga uwagi przez cały czas tego stanu
    p.prostokat(0, U07_PAS_Y, 800, U07_PAS_H, true);
    tekstSrodek(p, F_P22B, 400, U07_PAS_Y + 27, t.pas, false);
  }
  if (!t.stopka.empty()) stopkaEkranu(p, t.stopka, "");
  else if (!t.pas.empty()) p.prostokat(0, 434, 800, 2, true);  // pusta stopka: sama kreska (przyciski nie działają)
}

bool rysujFormularzQr(Plotno& p, const DaneFormularzaQr& d) {
  p.prostokat(0, 0, 800, 480, false);
  belka(p, {"Ustawienia panelu", d.bezWifi || d.adresQr.empty() ? "" : "Wi-Fi włączone"});
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
  belka(p, {"Sieć panelu", kody ? "Wi-Fi panelu włączone" : ""});
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
