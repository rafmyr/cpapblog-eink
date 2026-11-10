// SPDX-License-Identifier: GPL-3.0-or-later
// Przypadki brzegowe konfiguracji w zrzucie panelu (T-WYM-92, spec 8.9): zrzut `Z` z `/konfiguracja.txt`
// na atrapach. Odtworzenie treści z wierszy jak tools/prototyp/zrzut_plikow.py zloz_konfiguracje() (wiersze z `\n`,
// wybór wariantu z końcowym `\n` albo bez po rozmiarze i skrócie). Błąd z nazwą przypadku.
// Dane syntetyczne.
#include <cstdio>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "dyrygent.h"
#include "magazyn.h"
#include "start.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w, nr)                                                                 \
  do {                                                                                 \
    if (w) ok++;                                                                       \
    else { zle++; printf("BŁĄD %s %s:%d: %s\n", nr, __FILE__, __LINE__, #w); }       \
  } while (0)

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};

static void zrzut(Panel& p) {
  for (char c : std::string("Z")) p.port.znaki.push_back((unsigned char)c);
  p.zegar.epoch = 1793800000u;  // 2026-11-04 w dzień
  p.zegar.msTeraz = 0;
  p.start.przyczynaStartu = PrzyczynaStartu::USPIENIE;
  p.start.powodWybudzenia = PowodWybudzenia::ZEGAR;
  zerujPrzedSnem(p.start.l, identyfikatorWersji(p.start.napisWersji));
  Dyrygent(p.adaptery(), KONF).wybudzenie();
}

// Jak zrzut_plikow.py: sekcja do pierwszego wiersza `PLIK_KONIEC`, rozmiar i skrót z wiersza przed sekcją.
static bool odtworz(const Panel& p, std::string& wynik, bool& zgodny) {
  std::string z, rozm, skr;
  bool w = false, jest = false;
  for (auto& l : p.port.log) {
    if (!w && l.compare(0, 24, "konfiguracja w zrzucie: ") == 0) {
      size_t b = l.find(" B, sha256 ");
      rozm = l.substr(24, b - 24);
      skr = l.substr(b + 11);
    }
    if (!w && l == "PLIK /konfiguracja.txt") { w = jest = true; continue; }
    if (w && l == "PLIK_KONIEC") break;
    if (w) z += l + "\n";
  }
  std::string bez = z.empty() ? z : z.substr(0, z.size() - 1);
  zgodny = false;
  wynik = z;
  for (const std::string& t : {z, bez})
    if (std::to_string(t.size()) == rozm && sha256Hex(t) == skr) { wynik = t; zgodny = true; break; }
  return jest;
}

static void obieg(const std::string& t, const char* nr) {
  Panel p;
  p.pliki.pliki["/noce.csv"] = "2026-11-03;1;414\n";
  p.pliki.pliki["/konfiguracja.txt"] = t;
  zrzut(p);
  std::string w;
  bool zg = false;
  SPRAWDZ(odtworz(p, w, zg), nr);
  SPRAWDZ(zg && w == t, nr);
  SPRAWDZ(p.pliki.tresc("/konfiguracja.txt") == t, nr);  // zrzut niczego nie zmienia
}

int main() {
  // Wierność: CRLF, bez końcowego \n, puste wiersze, UTF-8, tabulatory, wiersz-znacznik pliku
  obieg("a=1\r\nb=2\r\n", "wierność");
  obieg("a=1\r\nb=2", "wierność");
  obieg("\r\n\r\n", "wierność");
  obieg("\n", "wierność");
  obieg("\n\n\n", "wierność");
  obieg("", "wierność");
  obieg("maska.model=AirFit F20 \xc5\xbc\xc3\xb3\xc5\x82\xc4\x87\n\tklucz=\twartosc\t\n", "wierność");
  obieg("PLIK /noce.csv\nPLIKI_START\nkonfiguracja w zrzucie: 1 B, sha256 " + std::string(64, 'a') + "\n", "wierność");
  obieg("x\r", "wierność");
  {
    std::string duzy;
    while (duzy.size() + 20 < MAKS_KONFIGURACJI_W) duzy += "k" + std::to_string(duzy.size()) + "=w\r\n";
    duzy += std::string(MAKS_KONFIGURACJI_W - duzy.size(), 'y');
    SPRAWDZ(duzy.size() == MAKS_KONFIGURACJI_W, "wierność");
    obieg(duzy, "wierność");
    obieg(duzy + std::string(5000, 'z') + "\n", "wierność");  // większy niż W przyjmie (np. z uploadfs): zrzut w całości
  }
  // Wiersz `PLIK_KONIEC` w treści ucina sekcję czytaną wierszami; skrót to wykrywa (niezgodny).
  // Po poprawce panel nie przyjmuje takiej treści komendą W (`BLAD konfiguracja.txt znacznik`), a narzędzie czyta
  // sekcję według rozmiaru z wiersza skrótu (tools/test_zrzut_konfiguracja_brzegowe.py).
  {
    const std::string t = "a=1\nPLIK_KONIEC\nb=2\n";
    KomendaW k;
    SPRAWDZ(parsujKomendeW("W konfiguracja.txt " + std::to_string(t.size()) + " " + sha256Hex(t), k), "znacznik w treści");
    SPRAWDZ(sprawdzKonfiguracjeW(k, t) == "znacznik", "znacznik w treści");
    for (const std::string& z : {std::string("a=1\nPLIKI_START\n"), std::string("PLIKI_KONIEC"), std::string("PLIK /noce.csv\n"),
                                 std::string("a=1\r\nPLIK_KONIEC\r\n")}) {
      SPRAWDZ(parsujKomendeW("W konfiguracja.txt " + std::to_string(z.size()) + " " + sha256Hex(z), k), "znacznik w treści");
      SPRAWDZ(sprawdzKonfiguracjeW(k, z) == "znacznik", "znacznik w treści");
    }
    for (const std::string& z : {std::string("PLIKI=1\n"), std::string("# PLIK_KONIEC\n"), std::string("plik_koniec=1\n")}) {
      SPRAWDZ(parsujKomendeW("W konfiguracja.txt " + std::to_string(z.size()) + " " + sha256Hex(z), k), "znacznik w treści");
      SPRAWDZ(sprawdzKonfiguracjeW(k, z) == "", "znacznik w treści (zwykłe wiersze przyjęte)");
    }
    Panel p;
    p.pliki.pliki["/konfiguracja.txt"] = t;
    zrzut(p);
    std::string w;
    bool zg = true;
    odtworz(p, w, zg);
    SPRAWDZ(!zg, "znacznik w treści (niezgodność wykryta)");
  }
  // Log bez treści: wiersz z treścią tylko w sekcji, raz. Znany klucz: nieznane klucze parser
  // konfiguracji i tak wypisuje w logu jako „pominięty wiersz” (zachowanie wcześniejsze, poza zakresem tego testu).
  {
    Panel p;
    p.pliki.pliki["/konfiguracja.txt"] = "wymiana.maska=2026-10-17\n";
    zrzut(p);
    int ile = 0;
    for (auto& l : p.port.log) ile += l.find("2026-10-17") != std::string::npos;
    SPRAWDZ(ile == 1, "log bez treści");
  }
  printf("testy_zrzut_konfiguracja_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
