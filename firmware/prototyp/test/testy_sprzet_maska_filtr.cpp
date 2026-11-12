// SPDX-License-Identifier: GPL-3.0-or-later
// uproszczenie modułu SPRZĘT. Rura i komora nawilżacza nie są pilnowane, poduszka
// i maska to jeden element „Maska” (6 miesięcy), filtr ma okres w tygodniach (4). Testy: migracja dat, 6 miesięcy,
// filtr 4 tygodnie, klucze ignorowane, formularz bez rury i komory, jeden licznik pokazań S-K-31 i L-09.
// Dane wyłącznie syntetyczne.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "formularz.h"
#include "humor.h"
#include "kontekst_sk.h"
#include "slot_dzienny.h"
#include "sprzet.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                                         \
  do {                                                                                     \
    if (w) ok++;                                                                           \
    else { zle++; printf("BŁĄD (%s:%d): %s\n", __FILE__, __LINE__, #w); }                  \
  } while (0)

static const std::string D0 = "2026-09-01";
static std::string d(int i) { return dodajDni(D0, i); }

static Noc noc(int i, float wp95) {
  Noc n;
  n.data = d(i);
  n.wazna = true;
  n.m01 = 420;
  n.ahi = 1.0f;
  n.wmed = 4;
  n.wp95 = wp95;
  n.cp95 = 10;
  n.ep95 = 8;
  n.m13 = 14;
  n.ustawienia = "AutoSet/5/12";
  return n;
}

// migracja dat: maska = późniejsza z `wymiana.maska` i `wymiana.poduszka`
static void migracjaDat() {
  UstawieniaDnia u = parsujUstawieniaDnia("wymiana.poduszka=2026-05-01\nwymiana.maska=2026-03-01\n");
  SPRAWDZ(u.sprzet[EL_MASKA].wymieniono == "2026-05-01");
  u = parsujUstawieniaDnia("wymiana.maska=2026-06-01\nwymiana.poduszka=2026-05-01\n");
  SPRAWDZ(u.sprzet[EL_MASKA].wymieniono == "2026-06-01");
  u = parsujUstawieniaDnia("wymiana.poduszka=2026-05-01\n");
  SPRAWDZ(u.sprzet[EL_MASKA].wymieniono == "2026-05-01");
  u = parsujUstawieniaDnia("wymiana.maska=2026-03-01\n");
  SPRAWDZ(u.sprzet[EL_MASKA].wymieniono == "2026-03-01");
  u = parsujUstawieniaDnia("");
  SPRAWDZ(u.sprzet[EL_MASKA].wymieniono.empty() && u.bledy.empty());
  // zła data poduszki: pominięta z błędem, maska bez zmian
  u = parsujUstawieniaDnia("wymiana.maska=2026-03-01\nwymiana.poduszka=31-12\n");
  SPRAWDZ(u.sprzet[EL_MASKA].wymieniono == "2026-03-01" && u.bledy.size() == 1);
  // zapis wymiany maski nadpisuje scaloną datę: nowy wiersz maski jest późniejszy od starego wiersza poduszki
  std::string n = sprzet::zapiszWymiane("wymiana.poduszka=2026-05-01\n", EL_MASKA, "2026-11-05");
  SPRAWDZ(parsujUstawieniaDnia(n).sprzet[EL_MASKA].wymieniono == "2026-11-05");
}

// maska: 6 miesięcy od ostatniej wymiany
static void szescMiesiecy() {
  UstawieniaDnia u = parsujUstawieniaDnia("wymiana.maska=2026-05-10\n");
  SPRAWDZ(u.sprzet[EL_MASKA].okres == 6 && !u.sprzet[EL_MASKA].tygodnie);
  SPRAWDZ(terminWymiany(u.sprzet[EL_MASKA]) == "2026-11-10");
  SPRAWDZ(!sprzet::poTerminie(u.sprzet[EL_MASKA], "2026-11-09") && sprzet::poTerminie(u.sprzet[EL_MASKA], "2026-11-10"));
  SPRAWDZ(terminWymiany(parsujUstawieniaDnia("wymiana.maska=2026-08-31\n").sprzet[EL_MASKA]) == "2027-02-28");
  // okres maski do zmiany w pliku, `okres.poduszka` (stary) ignorowany
  u = parsujUstawieniaDnia("wymiana.maska=2026-05-10\nokres.poduszka=1\nokres.maska=12\n");
  SPRAWDZ(u.sprzet[EL_MASKA].okres == 12 && u.bledy.empty());
  u = parsujUstawieniaDnia("okres.poduszka=1\npoduszka=pianka\n");
  SPRAWDZ(u.sprzet[EL_MASKA].okres == 6 && u.bledy.empty());  // pianka (1 mies.) już nie skraca okresu
  // dwa elementy (od ADR-0041 bez ekranu „Sprzęt”: wiek, okres i termin w formularzu U-03, R-MAS-17)
  sprzet::Elementy e;
  e.el[EL_MASKA] = {"2026-04-01", 6, false};
  e.el[EL_FILTR] = {"2026-10-01", 4, true};
  SPRAWDZ(wiekElementu(e.el[EL_MASKA], "2026-11-05") == "7 mies." && sprzet::poTerminie(e.el[EL_MASKA], "2026-11-05"));
  SPRAWDZ(wiekElementu(e.el[EL_FILTR], "2026-11-05") == "5 tyg." && sprzet::poTerminie(e.el[EL_FILTR], "2026-11-05"));
  SPRAWDZ(NAZWY_ELEMENTU[EL_MASKA] == std::string("maska") && NAZWY_ELEMENTU[EL_FILTR] == std::string("filtr") && EL_N == 2);
}

// filtr: 4 tygodnie, okres w tygodniach, zgodność ze starym `okres.filtr` (miesiące × 4)
static void filtrTygodnie() {
  UstawieniaDnia u = parsujUstawieniaDnia("wymiana.filtr=2026-10-13\n");
  SPRAWDZ(u.sprzet[EL_FILTR].okres == 4 && u.sprzet[EL_FILTR].tygodnie);
  SPRAWDZ(terminWymiany(u.sprzet[EL_FILTR]) == "2026-11-10");  // 28 dni
  SPRAWDZ(wiekElementu(u.sprzet[EL_FILTR], "2026-11-03") == "3 tyg.");
  SPRAWDZ(wiekElementu(u.sprzet[EL_FILTR], "2026-12-22") == "10 tyg.");  // zawsze tygodnie, nie „2 mies.”
  SPRAWDZ(wiekElementu(ElementWymiany{"", 4, true}, "2026-12-20") == "–");
  u = parsujUstawieniaDnia("okres.filtr_tyg=6\n");
  SPRAWDZ(u.sprzet[EL_FILTR].okres == 6 && u.bledy.empty());
  // stary klucz w miesiącach: × 4 (2 → 8 tygodni), bez błędu
  u = parsujUstawieniaDnia("okres.filtr=2\n");
  SPRAWDZ(u.sprzet[EL_FILTR].okres == 8 && u.sprzet[EL_FILTR].tygodnie && u.bledy.empty());
  // nowy klucz wygrywa w obu kolejnościach wierszy
  u = parsujUstawieniaDnia("okres.filtr=2\nokres.filtr_tyg=5\n");
  SPRAWDZ(u.sprzet[EL_FILTR].okres == 5);
  u = parsujUstawieniaDnia("okres.filtr_tyg=5\nokres.filtr=2\n");
  SPRAWDZ(u.sprzet[EL_FILTR].okres == 5);
  // złe wartości: pominięty wiersz, okres domyślny
  u = parsujUstawieniaDnia("okres.filtr_tyg=0\nokres.filtr_tyg=105\nokres.filtr=x\n");
  SPRAWDZ(u.sprzet[EL_FILTR].okres == 4 && u.bledy.size() == 3);
  u = parsujUstawieniaDnia("okres.filtr_tyg=104\n");
  SPRAWDZ(u.sprzet[EL_FILTR].okres == 104 && u.bledy.empty());
  // 60 mies. × 4 = 240 → obcięte do 104 tygodni
  u = parsujUstawieniaDnia("okres.filtr=60\n");
  SPRAWDZ(u.sprzet[EL_FILTR].okres == 104);
}

// klucze rury, komory i typu poduszki: czytane bez błędu i ignorowane
static void ignorowane() {
  UstawieniaDnia u = parsujUstawieniaDnia("wymiana.rura=2026-01-01\nokres.rura=12\nwymiana.komora=2026-01-01\n"
                                          "okres.komora=12\nokres.poduszka=1\npoduszka=pianka\npoduszka=cokolwiek\n"
                                          "okres.rura=zle\n");
  SPRAWDZ(u.bledy.empty());
  SPRAWDZ(u.sprzet[EL_MASKA].wymieniono.empty() && u.sprzet[EL_FILTR].wymieniono.empty());
  SPRAWDZ(u.sprzet[EL_MASKA].okres == 6 && u.sprzet[EL_FILTR].okres == 4);
  // rura po terminie nie daje S-K-35 (bez dat maski i filtra)
  SPRAWDZ(wymianaSprzetu(u.sprzet, "2026-11-05", "").element < 0);
}

// formularz: tylko maska i filtr, okres filtra w tygodniach, ignorowane klucze zostają w pliku
static void formularzBezRuryIKomory() {
  sekrety::Dane s;
  formularz::Pola p = formularz::zPliku("wymiana.poduszka=2026-05-01\nwymiana.maska=2026-03-01\nwymiana.rura=2026-01-01\n"
                                        "okres.filtr=2\nokres.maska=5\npoduszka=pianka\n", s);
  SPRAWDZ(formularz::ELEMENTY == 2);
  SPRAWDZ(p.wymiana[0] == "2026-05-01" && p.okres[0] == "5");  // maska: późniejsza data
  SPRAWDZ(p.okres[1] == "8");                                  // stary `okres.filtr` 2 mies. → 8 tygodni
  p = formularz::zPliku("okres.filtr=2\nokres.filtr_tyg=6\n", s);
  SPRAWDZ(p.okres[1] == "6");
  std::string h = formularz::strona(p, "/u/abc/", "", {});
  SPRAWDZ(h.find("Maska: wymieniona") != std::string::npos && h.find("Filtr: wymieniona") != std::string::npos);
  SPRAWDZ(h.find("Filtr: okres wymiany (tygodnie, puste = domyślny)") != std::string::npos);
  SPRAWDZ(h.find("Maska: okres wymiany (miesiące, puste = domyślny)") != std::string::npos);
  SPRAWDZ(h.find("name=\"okres.filtr_tyg\"") != std::string::npos);
  for (const char* x : {"Rura", "rura", "Komora", "komora", "Poduszka", "poduszka", "poduszki", "pianka"})
    SPRAWDZ(h.find(x) == std::string::npos);
  // zapis: pole filtra usuwa stary wiersz `okres.filtr`, data maski usuwa stary wiersz poduszki
  formularz::Wynik w = formularz::waliduj({{"wymiana.maska", "2026-05-01"}, {"okres.filtr_tyg", "6"}}, s, "2026-11-05");
  SPRAWDZ(w.ok() && w.plik["okres.filtr_tyg"] == "6" && w.plik.count("okres.filtr") && w.plik["okres.filtr"].empty());
  SPRAWDZ(w.plik["wymiana.maska"] == "2026-05-01" && w.plik.count("wymiana.poduszka") && w.plik["wymiana.poduszka"].empty());
  std::string pl = "okres.filtr=2\nwymiana.poduszka=2026-05-01\nwymiana.rura=2026-01-01\nokres.komora=12\npoduszka=pianka\n";
  std::string n = formularz::scal(pl, w.plik);
  SPRAWDZ(n == "wymiana.rura=2026-01-01\nokres.komora=12\npoduszka=pianka\nokres.filtr_tyg=6\nwymiana.maska=2026-05-01\n");
  UstawieniaDnia u = parsujUstawieniaDnia(n);
  SPRAWDZ(u.bledy.empty() && u.sprzet[EL_MASKA].wymieniono == "2026-05-01" && u.sprzet[EL_FILTR].okres == 6);
  // puste pole filtra = domyślne 4 tygodnie, stary wiersz też znika
  w = formularz::waliduj({{"okres.filtr_tyg", ""}}, s, "2026-11-05");
  n = formularz::scal("okres.filtr=2\n", w.plik);
  SPRAWDZ(n.empty() && parsujUstawieniaDnia(n).sprzet[EL_FILTR].okres == 4);
  // błędne wartości z komunikatami
  w = formularz::waliduj({{"okres.filtr_tyg", "105"}, {"okres.maska", "61"}}, s, "2026-11-05");
  SPRAWDZ(w.bledy["okres.filtr_tyg"] == "Liczba tygodni od 1 do 104." && w.bledy["okres.maska"] == "Liczba miesięcy od 1 do 60.");
  // pola rury, komory i typu poduszki w ciele są ignorowane
  w = formularz::waliduj({{"wymiana.rura", "2026-01-01"}, {"okres.komora", "9"}, {"poduszka", "zle"}}, s, "2026-11-05");
  SPRAWDZ(w.ok() && w.plik.empty());
}

// S-K-31 i L-09: jeden licznik pokazań maski
static void jedenLicznik() {
  std::vector<Noc> h;
  for (int i = 0; i < 80; i++) h.push_back(noc(i, i < 50 ? 10.0f : 10.0f + (float)(i - 49)));
  Norma nm;
  nm.wp95.med = 10;
  nm.wp95.n = 30;
  sprzet::Elementy e;
  e.el[EL_MASKA] = {"2026-01-01", 6, false};
  sprzet::WejscieSprzetu ws;
  ws.dzis = d(79);
  ws.hist = &h;
  ws.norma = &nm;
  ws.el = &e;
  std::vector<WpisHumoru> dz;
  ws.dziennik = &dz;
  sprzet::WynikSK31 r = sprzet::ocenSK31(ws);
  if (!r.warunek) { printf("  (S-K-31: %s)\n", r.dlaczego); }
  SPRAWDZ(r.warunek && r.element == EL_MASKA && r.sprawa == "maska@2026-01-01");
  SPRAWDZ(sprzet::blokadaL09(ws));  // 0 pokazań: S-K-31 może się pokazać, L-09 wstrzymane
  auto wpis = [](const std::string& data, const std::string& klucz) {
    WpisHumoru x;
    x.data = data;
    x.typ = "SK31";
    x.klucz = klucz;
    return x;
  };
  // 3 pokazania `maska@…` + 1 starsze `poduszka@…` tej samej daty wymiany = 4: L-09 wraca (R-MAS-10)
  dz = {wpis(d(50), "maska@2026-01-01"), wpis(d(57), "maska@2026-01-01"), wpis(d(64), "maska@2026-01-01")};
  SPRAWDZ(sprzet::blokadaL09(ws));
  dz.push_back(wpis(d(71), "poduszka@2026-01-01"));
  SPRAWDZ(!sprzet::blokadaL09(ws));
  // 4 pokazania `maska@…`
  dz.back() = wpis(d(71), "maska@2026-01-01");
  SPRAWDZ(!sprzet::blokadaL09(ws));
  // pokazania poprzedniej wymiany (inna data w sprawie) nie liczą się
  dz = {wpis(d(50), "maska@2025-07-01"), wpis(d(57), "maska@2025-07-01"), wpis(d(64), "poduszka@2025-07-01"),
        wpis(d(71), "maska@2025-07-01")};
  SPRAWDZ(sprzet::blokadaL09(ws));
  // wymiana: nowa sprawa, blokada 14 nocy ważnych (R-MAS-08)
  e.el[EL_MASKA].wymieniono = d(70);
  SPRAWDZ(sprzet::blokadaL09(ws));
  e.el[EL_MASKA].wymieniono = d(60);
  SPRAWDZ(!sprzet::blokadaL09(ws));
  // maska w terminie, filtr po terminie: filtr nie blokuje L-09 ani nie daje S-K-31
  e.el[EL_MASKA].wymieniono = "2026-08-01";
  e.el[EL_FILTR] = {"2026-01-01", 4, true};
  SPRAWDZ(!sprzet::ocenSK31(ws).warunek && !sprzet::blokadaL09(ws));
}

// teksty: pytanie i ekrany o masce
static void teksty() {
  SPRAWDZ(sk::tekst("R-MAS-12.tytul") == "Wymieniona maska?");
  SPRAWDZ(sk::tekst("S-K-31.tytul") == "Maska się zużywa" && sk::tekst("S-K-32.tytul") == "Nowa maska działa");
}

int main() {
  migracjaDat();
  szescMiesiecy();
  filtrTygodnie();
  ignorowane();
  formularzBezRuryIKomory();
  jedenLicznik();
  teksty();
  printf("testy_sprzet_maska_filtr: maska i filtr): %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
