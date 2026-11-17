// SPDX-License-Identifier: GPL-3.0-or-later
// Pamięć plików między przebiegami bez głębokiego snu (ADR-0047, T-WYM-14). Warstwa nad adapterem plików
// (src/adaptery.h Pliki): po włączeniu (wlacz(true), src/main.cpp po pierwszym czekaniu) pamięta w RAM (PSRAM) tylko
// dwie rzeczy: pełną treść z udanego czytaj() i istnieje() == true. Klucz to ścieżka bez ukośnika na końcu.
// Nie pamięta (zmiana 2026-11-17): listuj() ani pierwszyWiersz(), bo lista
// katalogu zależy od każdego pliku w nim, a adapter na panelu potrafi zwrócić true z wynikiem niepełnym (lista urwana
// przy braku pamięci w openNextFile(), obcięty wiersz), którego nie da się odróżnić od poprawnego; obie idą zawsze na
// nośnik, jak po głębokim śnie. Nieudanego odczytu ani „braku pliku” nie pamięta nigdy (na panelu LittleFS.exists() i
// open() zawodzą też chwilowo, np. bez pamięci na bufor), więc taki plik następny przebieg
// czyta z nośnika jak po głębokim śnie. Każda operacja zmieniająca nośnik przez tę warstwę (zapisz, zapiszAtomowo,
// dopiszWiersz, usun, zmienNazwe, czytajWiersze z przywróceniem `.tmp`) unieważnia ścieżkę, jej `.tmp`, wszystko pod
// nią (katalog) i istnienie katalogów nad nią; montowanie, odmontowanie i formatowanie czyszczą wszystko. Poprawne tylko
// wtedy, gdy nic nie zmienia plików z pominięciem tej warstwy: na panelu robi to jedynie wgranie obrazu pamięci plików
// w oknie serwisowym (SiecEsp32, odmontuj), po którym zawsze jest restart. Wyłączona: przepuszcza wszystko bez zmian.
#pragma once
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "adaptery.h"

class PlikiZPamiecia : public Pliki {
 public:
  // [PRÓG] T-WYM-14: najwięcej bajtów treści w pamięci; plik, który się nie mieści, czytany zawsze z nośnika.
  static const size_t LIMIT_B = static_cast<size_t>(1024) * 1024;

  explicit PlikiZPamiecia(Pliki& p, size_t limit = LIMIT_B) : p_(p), limit_(limit) {}
  void wlacz(bool w) {
    if (!w) wyczysc();
    wl_ = w;
  }
  bool wlaczona() const { return wl_; }
  size_t bajtow() const { return rozmiar_; }
  // liczniki (testy T-TEST-500): odczyty treści z nośnika przez czytaj() i trafienia w pamięć (pierwszyWiersz() i
  // listuj() nie są liczone: zawsze idą na nośnik)
  unsigned long odczytowNosnika = 0, trafien = 0;

  bool zamontuj() override;
  bool zamontowane() override { return p_.zamontowane(); }
  bool istnieje(const char* nazwa) override;
  bool czytajWiersze(const char* nazwa, std::vector<std::string>& wiersze) override;
  bool zapiszAtomowo(const char* nazwa, const std::string& tresc) override;
  bool dopiszWiersz(const char* nazwa, const std::string& wiersz) override;
  bool pierwszyWiersz(const char* nazwa, std::string& wiersz) override;
  bool usun(const char* nazwa) override;
  bool zmienNazwe(const char* z, const char* na) override;
  void odmontuj() override;
  bool czytaj(const char* nazwa, std::string& tresc) override;
  size_t zapisz(const char* nazwa, const std::string& tresc) override;
  bool listuj(const char* katalog, std::vector<std::string>& nazwy) override;
  OpisPartycji partycja() override { return p_.partycja(); }
  bool czytajPartycje(size_t ile, std::string& bajty) override { return p_.czytajPartycje(ile, bajty); }
  bool formatuj() override;
  long long wolneMiejsce() override { return p_.wolneMiejsce(); }

 private:
  Pliki& p_;
  size_t limit_;
  bool wl_ = false;
  size_t rozmiar_ = 0;
  std::map<std::string, std::string> tresc_;
  std::set<std::string> istnieje_;  // tylko „plik jest”, zob. istnieje()
  static std::string klucz(const char* nazwa);
  void niewazne(const char* nazwa);
  void wyczysc();
};
