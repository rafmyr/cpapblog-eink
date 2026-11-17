// SPDX-License-Identifier: GPL-3.0-or-later
// Pamięć między przebiegami bez głębokiego snu (ADR-0047, T-WYM-14): jeden obiekt na cały program, tworzony przez
// wołającego dyrygenta (src/main.cpp, testy). Każdy przebieg to nowy `Przebieg` (stan jednego wybudzenia od zera,
// jak po głębokim śnie); tu zostaje tylko to, co jest czystą funkcją treści danych i ma klucz z tej treści, więc
// po każdej zmianie danych samo się unieważnia: przeczytane i rozłożone noce (klucz: wiersze plików nocy i ich
// ścieżki) oraz ostatnia ocena silnika reguł (klucz reguly::kluczOceny(), T-WYM-99). Bez wołającego (nullptr)
// dyrygent działa dokładnie jak przed 16.11.2026: nigdy nie czeka, pamięci tylko na jedno wybudzenie.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "noc.h"
#include "reguly.h"

struct PamiecPrzebiegow {
  // ---------- prowadzenie przebiegów ----------
  bool czekano = false;     // ostatni przebieg skończył się czekaniem: wołający uruchamia następny
  uint32_t przebiegow = 0;  // przebiegi od startu programu (z bieżącym)
  bool czekanieAktywne = false;  // ten program czekał; NVS `cpap`/`cz_akt` ustawione (T-WYM-17)

  // ---------- noce z plików (Przebieg::wczytajDane()) ----------
  struct Historia {
    bool jest = false;
    std::vector<std::string> wiersze;  // wiersze danych plików nocy (pola wersji 5) w kolejności odczytu
    std::vector<std::string> sciezki;  // ścieżka pliku każdego wiersza
    std::vector<uint8_t> czytelne;     // 1 = parsujNocV5() przyjął wiersz
    std::vector<Noc> noce;             // noce z wierszy czytelnych, w tej samej kolejności
  } historia;

  // ---------- ostatnia ocena silnika reguł (Przebieg::ocenSilnikiem(), T-WYM-99) ----------
  struct Silnik {
    bool jest = false;
    uint64_t klucz = 0;
    std::vector<reguly::Wynik> wyniki;
    std::vector<std::string> log;  // wiersze `reguly: … nie oceniam` z tej oceny, w kolejności
  } silnik;

  // ---------- liczniki (testy T-TEST-500) ----------
  unsigned long parsowanHistorii = 0;  // przebiegi z rozkładem wierszy nocy od nowa
  unsigned long trafienHistorii = 0;   // przebiegi z nocami z pamięci
};
