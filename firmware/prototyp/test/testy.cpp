// SPDX-License-Identifier: GPL-3.0-or-later
// Testy logiki prototypu bez Arduino: przypadki o znanej odpowiedzi, w większości z kolumny „Weryfikacja” spec.
// Uruchomienie: tools/prototyp/testy.sh
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <set>
#include <cstring>
#include "airbridge.h"
#include "bateria.h"
#include "cykl.h"
#include <string>
#include <vector>
#include "czynniki.h"
#include "dzien.h"
#include "ekrany.h"
#include "humor.h"
#include "kokpit.h"
#include "nawigacja.h"
#include "stany.h"
#include "sypialnia.h"
#include "noc.h"
#include "otoczenie.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
#define ROWNE(a, b)                                                                                     \
  do {                                                                                                  \
    std::string _a = (a), _b = (b);                                                                     \
    if (_a == _b) ok++;                                                                                 \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)

static Noc noc(const std::string& d, int m01, float ahi, float wp95 = NAN, float cmed = NAN, float cp95 = NAN) {
  Noc n;
  n.data = d;
  n.m01 = m01;
  n.wazna = m01 >= 240;
  n.ahi = ahi;
  n.wp95 = wp95;
  n.cmed = cmed;
  n.cp95 = cp95;
  n.zrodlo = 'K';
  return n;
}

static void testDaty() {
  SPRAWDZ(nrDnia("1970-01-01") == 0);
  SPRAWDZ(nrDnia("2000-03-01") == 11017);
  ROWNE(dataZNr(nrDnia("2026-10-29")), "2026-10-29");
  SPRAWDZ(dzienTygodnia("2026-10-29") == 4);  // czwartek
  SPRAWDZ(dzienTygodnia("2026-11-12") == 4);  // artboard K1: „czw 12.11”
  SPRAWDZ(dzienTygodnia("2026-11-01") == 0);
  ROWNE(dodajDni("2026-03-01", -1), "2026-02-28");
  ROWNE(dodajDni("2028-03-01", -1), "2028-02-29");
  ROWNE(dodajDni("2026-12-31", 1), "2027-01-01");
  SPRAWDZ(dniMiedzy("2026-10-01", "2026-10-29") == 28);
  SPRAWDZ(!dataOk("2026-13-01") && !dataOk("2026-1-01") && !dataOk("2026-00-10") && !dataOk("2026-05-00"));
  // liczba dni miesiąca i rok przestępny: 31 lutego nie przechodzi już po cichu jako 3 marca
  SPRAWDZ(!dataOk("2026-02-31") && !dataOk("2026-02-29") && dataOk("2028-02-29") && !dataOk("2026-04-31"));
  SPRAWDZ(dataOk("2026-02-28") && dataOk("2026-12-31") && dataOk("2000-02-29") && !dataOk("2100-02-29"));
  SPRAWDZ(nrDnia("2026-02-30") == -1 && dodajDni("2026-04-31", 1).empty());
  ROWNE(ddmm("2026-10-07"), "07.10");
  ROWNE(dzienMiesiac("2026-10-07"), "7 października");
  ROWNE(przecinek1(13.55), "13,6");
  ROWNE(przecinek1(12.7f), "12,7");
  ROWNE(przecinek1(NAN), "–");
  ROWNE(calkowita(8.5), "9");
  ROWNE(calkowita(5.49f), "5");
  ROWNE(godzMin(414), "6:54");
  ROWNE(godzMin(-1), "–");
  ROWNE(godzH(549), "9 h 09");
}

static void testNoc() {
  Noc r;
  SPRAWDZ(parsujNoc("2026-10-28;1;414;1.20;9.40;11.30;4.00;12.00;K", 'A', r));
  SPRAWDZ(r.wazna && r.m01 == 414 && std::fabs(r.ahi - 1.2f) < 1e-6 && r.zrodlo == 'K');
  ROWNE(zapiszNoc(r), "2026-10-28;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;K;0;0;;;;");  // zapis zawsze w 20 polach
  SPRAWDZ(parsujNoc("2026-10-28;0;;;;;;", 'A', r) && r.m01 == -1 && std::isnan(r.ahi) && r.zrodlo == 'A');
  SPRAWDZ(!parsujNoc("2026-10-28;1;414;1,20;;;;", 'A', r));  // przecinek dziesiętny = śmieci
  // nieskończoność, liczby ponad float i minuty spoza doby to błąd wiersza (nie UB w rzutowaniu)
  SPRAWDZ(!parsujNoc("2026-10-28;1;inf;;;;;", 'A', r));
  SPRAWDZ(!parsujNoc("2026-10-28;1;1e10;;;;;", 'A', r));
  SPRAWDZ(!parsujNoc("2026-10-28;1;414;1e39;;;;", 'A', r));
  SPRAWDZ(!parsujNoc("2026-10-28;1;414;-inf;;;;", 'A', r));
  SPRAWDZ(!parsujNoc("2026-10-28;1;1441;;;;;", 'A', r) && !parsujNoc("2026-10-28;1;-2;;;;;", 'A', r));
  SPRAWDZ(parsujNoc("2026-10-28;1;1440;;;;;", 'A', r) && r.m01 == 1440);
  SPRAWDZ(parsujNoc("2026-10-28;0;-1;;;;;", 'A', r) && r.m01 == -1);
  SPRAWDZ(!parsujNoc("2026-13-28;1;414;1.2;;;;", 'A', r));
  SPRAWDZ(!parsujNoc("data_poranka;wazna;m01", 'A', r));
  SPRAWDZ(parsujNoc("2026-10-28;1;414;1.2;;;;;K\r", 'A', r) && r.zrodlo == 'K');

  std::vector<double> v;
  for (int i = 1; i <= 20; i++) v.push_back(i);
  SPRAWDZ(percentyl(v, 10) == 2 && percentyl(v, 90) == 18 && mediana(v) == 10.5);

  // 30 nocy ważnych przed dziś: AHI 1..30 → mediana 15,5, P10 = 3 (ranga 3), P90 = 27 (ranga 27)
  std::vector<Noc> h;
  for (int i = 1; i <= 30; i++) h.push_back(noc(dodajDni("2026-10-29", -31 + i), 400, (float)i));
  h.push_back(noc("2026-10-29", 400, 99));          // noc oceniana nie wchodzi do własnej normy (A-10)
  h.push_back(noc("2026-06-01", 400, 50));          // poza 90 dniami
  Noc krotka = noc("2026-10-27", 100, 77);          // nieważna (< 4 h) — zastępuje 2026-10-27
  h[27 - 1] = krotka;  // noc 2026-10-27 (i = 27) staje się nieważna
  Norma nm = liczNorme(h, "2026-10-29", {});
  SPRAWDZ(nm.ahi.n == 29);
  // AHI 1..30 bez 27 → 29 wartości: mediana 15, P10 ranga 3 → 3, P90 ranga 27 → 28
  SPRAWDZ(nm.ahi.med == 15 && nm.ahi.p10 == 3 && nm.ahi.p90 == 28);
  Norma nm2 = liczNorme(h, "2026-10-29", {"2026-10-28"});  // R-CZY-05: noc z katarem poza normą
  SPRAWDZ(nm2.ahi.n == 28 && nm2.ahi.med == 14.5);
  std::vector<Noc> malo(h.begin(), h.begin() + 13);
  SPRAWDZ(!liczNorme(malo, "2026-10-29", {}).ahi.jest() && std::isnan(liczNorme(malo, "2026-10-29", {}).ahi.med));

  // R-CZY-01: norma AHI mediana 1,0, P90 2,5 → noc 4,5 ma odchylenie; Δ = +3 / h
  std::vector<Noc> n2;
  for (int i = 0; i < 20; i++) n2.push_back(noc(dodajDni("2026-10-28", -i), i < 4 ? 400 : 420, i < 4 ? 2.5f : 1.0f, 10, 9, 11));
  Norma nb = liczNorme(n2, "2026-10-29", {});
  SPRAWDZ(nb.ahi.med == 1.0 && std::fabs(nb.ahi.p90 - 2.5) < 1e-6);
  SPRAWDZ(odchylenieWNocy(noc("2026-10-29", 420, 4.5f), nb).ahi);
  SPRAWDZ(odchylenieWNocy(noc("2026-10-29", 420, 4.0f), nb).ahi);
  SPRAWDZ(!odchylenieWNocy(noc("2026-10-29", 420, 3.9f), nb).ahi);
  SPRAWDZ(!odchylenieWNocy(noc("2026-10-29", 200, 9.0f), nb).jakiekolwiek());  // noc nieważna
  // M-01 w dół: mediana 420, P10 400 → 320 min to odchylenie (−100), 340 nie (−80)
  SPRAWDZ(nb.m01.med == 420 && nb.m01.p10 == 400);
  Noc k1 = noc("2026-10-29", 320, 1.0f);
  Noc k2 = noc("2026-10-29", 340, 1.0f);
  SPRAWDZ(odchylenieWNocy(k1, nb).m01 && !odchylenieWNocy(k2, nb).m01);
  // M-07 P95: norma 10, P90 10 → 19 bez (Δ 9), 20 z odchyleniem
  SPRAWDZ(!odchylenieWNocy(noc("2026-10-29", 420, 1, 19), nb).wp95 && odchylenieWNocy(noc("2026-10-29", 420, 1, 20), nb).wp95);

  std::vector<Noc> s = {noc("2026-10-25", 200, 1), noc("2026-10-26", 300, 1), noc("2026-10-27", 300, 1), noc("2026-10-28", 241, 1)};
  SPRAWDZ(seria(s, "2026-10-28") == 3 && seria(s, "2026-10-29") == 0 && seria(s, "2026-10-25") == 0);
  SPRAWDZ(nocyPonad4h(s, "2026-10-28") == 3 && nocyPonad4h(s, "2026-10-26") == 1);
  SPRAWDZ(liczbaNocyWaznych(s, "2026-10-28") == 3);
  SPRAWDZ(ostatniaNoc(s, "2026-10-30")->data == "2026-10-28" && !ostatniaNoc(s, "2026-10-01"));
}

static void testCzynniki() {
  WpisCzynnikow w;
  SPRAWDZ(parsujWpis("2026-10-29;O;AS;O;660;1", w) && w.czynniki == (CZ_ALKOHOL | CZ_STRES) && w.warstwa == 'O' && w.pokazano);
  ROWNE(zapiszWpis(w), "2026-10-29;O;AS;O;660;1");
  SPRAWDZ(parsujWpis("2026-10-29;C;;K;900;0\r", w) && w.stan == 'C' && w.termin == 900);
  SPRAWDZ(!parsujWpis("2026-10-29;O;;O;660;1", w));   // odpowiedź bez czynników
  SPRAWDZ(!parsujWpis("2026-10-29;O;X;O;660;1", w));  // nieznany kod
  SPRAWDZ(!parsujWpis("2026-10-29;C;;O;660", w));

  // R-PIE-13: import do 09:00 → 11:00; później import + 2 h, najpóźniej 23:30
  SPRAWDZ(terminOdpowiedzi(6 * 60) == 660 && terminOdpowiedzi(9 * 60) == 660 && terminOdpowiedzi(10 * 60) == 720);
  SPRAWDZ(terminOdpowiedzi(13 * 60) == 900 && terminOdpowiedzi(22 * 60 + 30) == 1410 && terminOdpowiedzi(23 * 60) == 1410);

  // R-CZY-11: stres, potem „nic szczególnego” → tylko „nic”; potem stres → tylko stres
  uint8_t z = przelacz(0, 2);
  SPRAWDZ(z == CZ_STRES);
  z = przelacz(z, POZYCJA_NIC);
  SPRAWDZ(z == CZ_NIC);
  z = przelacz(z, 2);
  SPRAWDZ(z == CZ_STRES);
  z = przelacz(z, 0);
  SPRAWDZ(z == (CZ_STRES | CZ_ALKOHOL));
  SPRAWDZ(przelacz(z, 0) == CZ_STRES);  // odznaczenie
  SPRAWDZ(POZYCJE[POZYCJA_NIC].bit == CZ_NIC);  // R-CZY-18: kursor startuje na „nic szczególnego”

  std::vector<WpisCzynnikow> v;
  WpisCzynnikow c;
  c.data = "2026-10-29";
  c.stan = 'C';
  c.warstwa = 'O';
  c.termin = 660;
  v.push_back(c);
  SPRAWDZ(zaproszenieAktywne(wpisZDnia(v, "2026-10-29"), "2026-10-29", 659));
  SPRAWDZ(!zamknijPoTerminie(v, "2026-10-29", 659) && v[0].stan == 'C');
  SPRAWDZ(zamknijPoTerminie(v, "2026-10-29", 660) && v[0].stan == 'B');  // R-CZY-03
  SPRAWDZ(!zaproszenieAktywne(wpisZDnia(v, "2026-10-29"), "2026-10-29", 600));
  zapiszOdpowiedz(v, "2026-10-29", CZ_NIC);  // R-CZY-13: po terminie z K1 → zastępuje „brak”
  SPRAWDZ(v.size() == 1 && v[0].stan == 'O' && v[0].czynniki == CZ_NIC && v[0].warstwa == 'Z');  // poza O-12
  WpisCzynnikow c2 = c;
  c2.data = "2026-10-30";
  v.push_back(c2);
  zapiszOdpowiedz(v, "2026-10-30", CZ_STRES, true);  // z linii przed terminem: warstwa zostaje
  SPRAWDZ(wpisZDnia(v, "2026-10-30")->warstwa == 'O' && wpisZDnia(v, "2026-10-30")->stan == 'O');
  v.pop_back();
  zapiszOdpowiedz(v, "2026-10-29", CZ_ALKOHOL);
  SPRAWDZ(v[0].czynniki == CZ_ALKOHOL && romb(&v[0]));  // R-KOK-14
  pominPytanie(v, "2026-10-29");
  SPRAWDZ(v[0].czynniki == CZ_ALKOHOL);  // pominięcie nie kasuje odpowiedzi
  zapiszOdpowiedz(v, "2026-10-28", 0);   // R-CZY-14: zapis bez zaznaczeń = brak odpowiedzi
  SPRAWDZ(wpisZDnia(v, "2026-10-28")->stan == 'B' && wpisZDnia(v, "2026-10-28")->warstwa == 'Z');
  zapiszOdpowiedz(v, "2026-10-27", CZ_KATAR | CZ_STRES);
  zapiszOdpowiedz(v, "2026-10-26", CZ_POZA_DOMEM);
  auto wyk = wykluczoneZNormy(v);  // R-CZY-05
  SPRAWDZ(wyk.size() == 2 && wyk.count("2026-10-27") && wyk.count("2026-10-26"));
  SPRAWDZ(!romb(wpisZDnia(v, "2026-10-26")) && romb(wpisZDnia(v, "2026-10-27")));

  // R-CZY-02: 700 nocy, ustalone ziarno → 72–128 losowań (oczekiwane 100), ten sam wynik przy powtórzeniu
  int k = 0, k2 = 0;
  for (int i = 0; i < 700; i++) {
    std::string d = dodajDni("2025-01-01", i);
    k += losujKontrolna(d, 12345);
    k2 += losujKontrolna(d, 12345);
  }
  printf("  grupa kontrolna: %d z 700 nocy (oczekiwane 72–128)\n", k);
  SPRAWDZ(k >= 72 && k <= 128 && k == k2);
  int inne = 0;
  for (int i = 0; i < 700; i++) inne += losujKontrolna(dodajDni("2025-01-01", i), 777);
  SPRAWDZ(inne >= 72 && inne <= 128);

  // decyzja zaproszenia
  std::vector<Noc> h;
  for (int i = 1; i <= 20; i++) h.push_back(noc(dodajDni("2026-10-29", -i), 420, 1.0f, 10));
  WejscieDecyzji we;
  we.norma = liczNorme(h, "2026-10-29", {});
  Noc dzis = noc("2026-10-29", 420, 6.0f, 10);
  we.noc = &dzis;
  we.nocyWaznychPrzed = 20;
  we.ziarno = 1;
  SPRAWDZ(decyzjaZaproszenia(we) == 'O');
  we.prosbaNaEkranie = true;  // R-PIE-10: ekran dnia jest już prośbą
  SPRAWDZ(decyzjaZaproszenia(we) == 0);
  we.prosbaNaEkranie = false;
  we.nocyWaznychPrzed = 13;  // R-CZY-09: rozruch
  SPRAWDZ(decyzjaZaproszenia(we) == 0);
  we.nocyWaznychPrzed = 20;
  // oczekiwania z niezależnej implementacji w Pythonie (FNV-1a + mieszanie), ziarno 1, październik 2026:
  // wylosowane 02, 03, 04, 08, 10 i 27.10
  std::string wylosowane;
  for (int i = 0; i < 29; i++) {
    std::string dd = dodajDni("2026-10-01", i);
    if (losujKontrolna(dd, 1)) wylosowane += dd.substr(8, 2) + " ";
  }
  ROWNE(wylosowane, "02 03 04 08 10 27 ");
  Noc zwykla = noc("2026-10-29", 420, 1.0f, 10);  // 29.10 nie jest wylosowana → bez zaproszenia
  we.noc = &zwykla;
  SPRAWDZ(decyzjaZaproszenia(we) == 0);
  Noc kontrolna = noc("2026-10-27", 420, 1.0f, 10);  // 27.10 wylosowana → grupa kontrolna
  we.noc = &kontrolna;
  SPRAWDZ(decyzjaZaproszenia(we) == 'K');
  we.noc = nullptr;
  SPRAWDZ(decyzjaZaproszenia(we) == 0);
}

static Norma normaReczna() {
  Norma nm;
  nm.zegar = true;
  auto ustaw = [](NormaMetryki& m, double med, double p10, double p90) { m.n = 30; m.med = med; m.p10 = p10; m.p90 = p90; };
  ustaw(nm.m01, 420, 372, 456);
  ustaw(nm.ahi, 1.4, 0.8, 2.1);
  ustaw(nm.cmed, 9.5, 9.0, 10.0);
  ustaw(nm.cp95, 10.8, 10.2, 11.4);
  ustaw(nm.wp95, 9, 6, 12);
  return nm;
}

static void testKokpit() {
  std::vector<Noc> h;
  // 7 nocy 23–29.10 jak w weryfikacjach R-KOK-07 i R-KOK-08
  h.push_back(noc("2026-10-23", 414, 1.4f, 9, 9.4f, 10.8f));
  h.push_back(noc("2026-10-24", 432, 5.2f, 31, 9.5f, 10.6f));
  h.push_back(noc("2026-10-25", 366, 3.1f, 28, 9.6f, 11.0f));  // 6:06 → ramka (poza 6:12–7:36)
  h.push_back(noc("2026-10-26", 384, 5.0f, 24, 9.5f, 10.7f));  // 6:24 → bez ramki; AHI 5,0 → ponad próg; wyciek 24 → nie
  // 2026-10-27 brak rekordu → kreski (R-KOK-10)
  h.push_back(noc("2026-10-28", 360, 1.3f, 8, 9.4f, 11.6f));
  h.push_back(noc("2026-10-29", 348, 1.1f, 10, 9.5f, 11.4f));
  std::vector<WpisCzynnikow> cz;
  zapiszOdpowiedz(cz, "2026-10-28", CZ_ALKOHOL);
  Norma nm = normaReczna();
  DaneK2 k = budujK2(h, "2026-10-29", nm, cz);
  ROWNE(k.zakres, "23–29 października");  // T4: małymi literami; nagłówek `Ostatnie 7 nocy · …` · `2 / 2`
  ROWNE(k.kol[0], "pt 23");  // jednoznaczne skróty dni
  ROWNE(k.kol[6], "cz 29");
  SPRAWDZ(k.romb[5] && !k.romb[6]);  // R-KOK-14
  ROWNE(k.zdanie, "6 z 7 nocy z maską ponad 4 h.");  // 6 zapisanych nocy ≥ 4 h, 27.10 bez rekordu
  const WierszK2& ahi = k.w[0];
  SPRAWDZ(ahi.k[1].ponadProg && !ahi.k[2].ponadProg && ahi.k[3].ponadProg);  // 5,2 i 5,0 tak, 3,1 nie (R-KOK-07)
  SPRAWDZ(ahi.k[2].poza && !ahi.k[0].poza && !ahi.k[1].poza);             // 3,1 poza normą, ponad próg bez ramki
  ROWNE(ahi.k[1].tekst, "5,2");
  ROWNE(ahi.norma, "0,8–2,1");
  const WierszK2& wyc = k.w[6];
  SPRAWDZ(wyc.k[1].ponadProg && wyc.k[2].ponadProg && !wyc.k[0].ponadProg && !wyc.k[3].ponadProg);  // 31, 28 tak; 9, 24 nie
  const WierszK2& czas = k.w[4];
  SPRAWDZ(czas.k[2].poza && !czas.k[3].poza);  // R-KOK-08: 6:06 ramka, 6:24 bez
  ROWNE(czas.k[2].tekst, "6:06");
  ROWNE(czas.norma, "6:12–7:36");
  for (int r = 0; r < DaneK2::WIERSZE; r++) ROWNE(k.w[r].k[4].tekst, "–");  // R-KOK-10
  ROWNE(k.w[1].norma, "–");  // noce z karty bez OA (R-DANE-31): bez danych w 30 nocach = kreski

  Norma pusta;  // R-KOK-05: bez normy `uczę się`, bez ramek; „ponad próg” zostaje
  DaneK2 kp = budujK2(h, "2026-10-29", pusta, cz);
  ROWNE(kp.w[0].norma, "uczę się");
  SPRAWDZ(!kp.w[0].k[2].poza && kp.w[0].k[1].ponadProg);

  DaneK1 a = budujK1(h, "2026-10-29", nm, cz);
  ROWNE(a.naglowek, "Ostatnia noc · cz 29.10");
  ROWNE(a.liczby[0], "5:48");
  ROWNE(a.liczby[1], "1,1");
  ROWNE(a.liczby[2], "–");  // EPAP: noce testu bez EPAP (ASV, ADR-0016)
  ROWNE(a.liczby[3], "10");
  ROWNE(a.w[1].nazwa, "Czas z maską");  // A-65: nie „Sen”
  ROWNE(a.w[0].mediana, "2,3");         // mediana z 6 nocy: 1,1 1,3 1,4 3,1 5,0 5,2 → (1,4 + 3,1) / 2 = 2,25 → 2,3
  ROWNE(a.w[4].mediana, "17");          // wyciek: 8 9 10 24 28 31 → (10 + 24) / 2 = 17 (jednostka w opisie liczby)
  ROWNE(a.opisy[3], "wyciek l/min");     // jedna forma jednostki na wszystkich ekranach (`l/min` jak w spec)
  ROWNE(a.w[0].slowo, "uczę się");  // norma jest, bez stanów; 6 nocy w oknie 30 nocy: za mało na trend (R-KOK-16)
  ROWNE(a.opisy[0], "czas z maską");  // T1: ta sama nazwa co wiersz (było `godz. na masce`)
  // linia 30 nocy K1 z przerywaną medianą normy (SPEC-KOKPIT 3); przerwa w nocy bez rekordu (R-KOK-02)
  SPRAWDZ(a.w[0].linia.ma[29] && !a.w[0].linia.ma[27] && a.w[0].linia.medNormy == 1.4);
  DaneK1 b = budujK1(h, "2026-10-31", pusta, cz);  // R-KOK-04: ostatnia zapisana noc nie jest ubiegłą
  ROWNE(b.naglowek, "Noc 29.10");
  ROWNE(b.w[0].slowo, "uczę się");  // R-KOK-05
  DaneK1 c = budujK1(h, "2026-10-28", nm, cz);
  SPRAWDZ(c.romb);
  DaneK1 e = budujK1({}, "2026-10-29", pusta, cz);  // fail-path R-KOK-01: brak nocy
  ROWNE(e.naglowek, "Ostatnia noc");
  ROWNE(e.liczby[1], "–");
  ROWNE(e.w[4].mediana, "–");
  DaneK2 kr = budujK2(h, "2026-11-02", nm, cz);
  ROWNE(kr.zakres, "23–29 października");  // okno kończy się na ostatniej zapisanej nocy (29.10)
  h.push_back(noc("2026-11-02", 400, 1.0f));
  kr = budujK2(h, "2026-11-02", nm, cz);
  ROWNE(kr.zakres, "27.10–02.11");
  ROWNE(kr.kol[6], "pn 2");
}

static WpisHumoru wpis(const std::string& d, const std::string& t, const std::string& k, char tryb, const std::string& sc = "",
                       const std::string& z = "") {
  return {d, t, k, tryb, sc, z};
}

static void testHumor() {
  WpisHumoru w;
  SPRAWDZ(parsujHumor("2026-10-29;S2B;;K;M01;seria:0", w) && w.tryb == 'K' && w.scena == "M01");
  ROWNE(zapiszHumor(w), "2026-10-29;S2B;;K;M01;seria:0");
  SPRAWDZ(!parsujHumor("2026-10-29;S2B;;X;M01;seria:0", w));
  ROWNE(tekstZdania("kamien:0"), "Tyle nocy? Maska powinna dostać stażowe.");  // pula przyjęta
  ROWNE(tekstZdania("seria:0"), "Kolejna noc. Konsekwencja bywa natrętna.");   // S2b: spokojna pula
  for (int i = 0; i < 7; i++) {
    std::string z = tekstZdania("seria:" + std::to_string(i));
    SPRAWDZ(!z.empty() && z.find("Medal") == std::string::npos && z.find("Świeczki") == std::string::npos);
  }
  ROWNE(tekstZdania("dobra_noc:4"), "Wyniki dobre. Nie lubię być optymistą.");
  auto znaki = [](const std::string& s) {  // znaki UTF-8, nie bajty
    int n = 0;
    for (unsigned char c : s) n += (c & 0xC0) != 0x80;
    return n;
  };
  SPRAWDZ(znaki("Świeczki") == 8);
  int najdluzsze = 0;
  for (auto id : {"dobra_noc", "seria", "kamien", "brak_danych", "synchronizacja"}) {  // R-HUM-12: ≤ 45 znaków, obie kolumny
    const Pula* p = pula(id);
    for (auto z : p->kanwa) najdluzsze = std::max(najdluzsze, znaki(z));
    for (auto z : p->propozycje) najdluzsze = std::max(najdluzsze, znaki(z));
  }
  SPRAWDZ(najdluzsze <= 45 && najdluzsze > 30);

  // R-HUM-27: pula „dobra noc” (przyjęta, zmiana 09.11.2026) ma 9 zdań, 3 tylko po spokojnej nocy
  // (oznaczone w puli, ADR-0029 pkt 7): bez warunku uspokojenia 6 zdań → karta K22 ze zdaniem spoza listy;
  // z warunkiem 9. Flaga propozycji bez skutku (listy puste).
  KontekstHumoru k;
  k.dzis = "2026-10-29";
  k.sceny = sceny();
  SPRAWDZ(zdaniaDoLosowania(*pula("dobra_noc"), false, false) ==
          (std::vector<std::string>{"dobra_noc:0", "dobra_noc:3", "dobra_noc:5", "dobra_noc:6", "dobra_noc:7", "dobra_noc:8"}));
  SPRAWDZ(zdaniaDoLosowania(*pula("dobra_noc"), false, true).size() == 9);
  SPRAWDZ(zdaniaDoLosowania(*pula("dobra_noc"), true, false).size() == 6);
  // lista uspokajających = tylko trzy zdania oznaczone (S) (5.7.5)
  SPRAWDZ(uspokajajace("Raport dobry. Pobudka psuje statystyki.") && uspokajajace("Wszystko w normie. Podejrzanie nudne.") &&
          uspokajajace("Wyniki dobre. Nie lubię być optymistą."));
  SPRAWDZ(!uspokajajace("Noc bez sensacji. Wreszcie coś dla mnie.") && !uspokajajace("Przyzwoita noc. Można nie dramatyzować.") &&
          !uspokajajace("Bilans sporządzony. Cyfry mają alibi.") && !uspokajajace("Noc odnotowana. Pobudka niestety też."));
  EkranDnia e;
  e.typ = "S2A";
  e.kartaZTabeli = true;
  Wybor x = wybierzTryb(e, true, {}, k);
  SPRAWDZ(x.tryb == 'K' && x.scena == "D01" && x.zdanie == "dobra_noc:0");  // komplet D01–D08, pierwsza D01
  k.zPropozycjami = true;
  SPRAWDZ(wybierzTryb(e, true, {}, k).tryb == 'K');
  k.zPropozycjami = false;
  k.uspokojenie = true;
  x = wybierzTryb(e, true, {}, k);
  SPRAWDZ(x.tryb == 'K' && x.scena == "D01" && x.zdanie == "dobra_noc:0");
  // zdanie uspokajające tylko przy warunku: dziennik z użytymi zdaniami nieuspokajającymi → bez warunku nigdy (S)
  {
    std::vector<WpisHumoru> uzyte;
    for (int i : {0, 3, 5, 6, 7})
      uzyte.push_back({"2026-10-2" + std::to_string(i % 9), "S2A", "", 'K', "D0" + std::to_string(i % 8 + 1),
                       "dobra_noc:" + std::to_string(i)});
    KontekstHumoru k2 = k;
    k2.uspokojenie = false;
    Wybor y = wybierzTryb(e, true, uzyte, k2);
    SPRAWDZ(y.tryb != 'K' || !uspokajajace(tekstZdania(y.zdanie)));
  }
  k.uspokojenie = false;

  // S2b: pula „seria” (przyjęta) ma 7 przyjętych zdań → karta bez medalu (D02) także bez propozycji
  // R-HUM-17 linia uwagi → ekran danych; R-HUM-05 → ekran danych
  e.typ = "S2B";
  x = wybierzTryb(e, true, {}, k);
  SPRAWDZ(x.tryb == 'K' && x.scena == "D02" && x.zdanie == "seria:0");
  k.zPropozycjami = true;
  x = wybierzTryb(e, true, {}, k);
  SPRAWDZ(x.tryb == 'K' && x.scena == "D02" && x.zdanie == "seria:0");
  k.liniaUwagi = true;
  SPRAWDZ(wybierzTryb(e, true, {}, k).tryb == 'D');
  k.liniaUwagi = false;
  SPRAWDZ(wybierzTryb(e, false, {}, k).tryb == 'D');
  k.humor = Humor::WYLACZONY;  // R-HUM-03
  SPRAWDZ(wybierzTryb(e, true, {}, k).tryb == 'D');
  k.humor = Humor::OSZCZEDNY;  // R-HUM-06: S2b dane, kamień karta
  SPRAWDZ(wybierzTryb(e, true, {}, k).tryb == 'D');
  EkranDnia km;
  km.typ = "KM";
  km.klucz = "100";
  km.kartaZTabeli = km.kartaOszczedny = true;
  x = wybierzTryb(km, true, {}, k);
  SPRAWDZ(x.tryb == 'K' && x.scena == "M03");
  k.humor = Humor::NORMALNY;
  km.klucz = "365";  // M05 w pamięci (komplet scen), więc kamień 365 nocy jest kartą; R-HUM-11: testy_karty_z_budzikiem.cpp
  x = wybierzTryb(km, true, {}, k);
  SPRAWDZ(x.tryb == 'K' && x.scena == "M05");

  // R-HUM-02: dwie karty dziś → trzecia nie
  std::vector<WpisHumoru> log = {wpis("2026-10-29", "S5", "1", 'K', "B04", "brak_danych:0"),
                                 wpis("2026-10-29", "KM", "30", 'K', "M01", "kamien:1")};
  SPRAWDZ(wybierzTryb(e, true, log, k).tryb == 'D');

  // R-HUM-23: okno z 14 wyświetleniami (13 + to) i 6 kartami → karta dozwolona (7 ≤ 7); przy 7 kartach już nie
  log.clear();
  for (int i = 0; i < 13; i++) log.push_back(wpis(dodajDni("2026-10-29", -6 + i / 2), "S2A", std::to_string(i), i < 6 ? 'K' : 'D', i < 6 ? "X" : ""));
  x = wybierzTryb(e, true, log, k);
  SPRAWDZ(x.tryb == 'K');
  log.push_back(wpis("2026-10-28", "S2A", "x", 'K', "Y"));
  x = wybierzTryb(e, true, log, k);
  SPRAWDZ(x.tryb == 'D');

  // R-HUM-10: w pamięci tylko D02 i D05 (R-HUM-11), D05 3 dni temu i D02 6 dni temu → brak sceny → ekran danych;
  // D02 pokazana 7 dni temu wolno znowu
  k.sceny = {"D02", "D05"};
  log = {wpis("2026-10-26", "S2B", "", 'K', "D05", "seria:0"), wpis("2026-10-23", "S2B", "", 'K', "D02", "seria:1")};
  SPRAWDZ(wybierzTryb(e, true, log, k).tryb == 'D');
  log[1].data = "2026-10-22";
  x = wybierzTryb(e, true, log, k);
  SPRAWDZ(x.tryb == 'K' && x.scena == "D02");
  // R-HUM-20: zdanie najdawniej użyte; nieużyte pierwsze
  SPRAWDZ(x.zdanie == "seria:2");
  k.sceny = sceny();
  log = {wpis("2026-10-01", "S2B", "", 'K', "D02", "seria:0"), wpis("2026-10-05", "S2B", "", 'K', "D05", "seria:1"),
         wpis("2026-10-09", "S2B", "", 'K', "D07", "seria:2"), wpis("2026-10-13", "S2B", "", 'K', "D08", "seria:3"),
         wpis("2026-10-17", "S2B", "", 'K', "D02", "seria:4"), wpis("2026-10-21", "S2B", "", 'K', "D05", "seria:5")};
  x = wybierzTryb(e, true, log, k);
  SPRAWDZ(x.zdanie == "seria:6" && x.scena == "D07");  // pula przyjęta: seria:6 nieużyte, przed użytymi (R-HUM-20)
  {
    std::vector<std::string> sc2b = scenyTypu("S2B", "");
    SPRAWDZ(std::find(sc2b.begin(), sc2b.end(), "M01") == sc2b.end());  // medal tylko przy kamieniu (KM)
  }

  // wybór ekranu dnia
  StanDnia s;
  s.dzis = "2026-10-29";
  s.nocDzis = s.nocDzisPonad4h = true;
  s.nocyWaznych = 40;
  s.maNorme = true;
  s.seria = 8;  // S2b od 7 nocy z rzędu (R-EKR-16)
  s.nocyPonad4h = 40;
  log.clear();
  EkranDnia d1 = wybierzEkranDnia(s, log);
  SPRAWDZ(d1.typ == "S2A" && d1.klucz == "1" && !d1.kartaZTabeli);
  // R-HUM-09: rotacja S2a → S2b (S2c pominięte w prototypie); karta K22 przy 3. i 6. wyświetleniu S2a
  std::vector<std::string> typy;
  std::vector<bool> karty;
  std::string dz = "2026-11-02";  // poniedziałek
  for (int i = 0; i < 12; i++) {
    s.dzis = dodajDni(dz, i);
    EkranDnia e2 = wybierzEkranDnia(s, log);
    typy.push_back(e2.typ);
    karty.push_back(e2.kartaZTabeli);
    log.push_back(wpis(s.dzis, e2.typ, e2.klucz, 'D'));
    SPRAWDZ(wybierzEkranDnia(s, log).typ == e2.typ);  // ten sam dzień zostaje przy swoim wariancie
  }
  // 2.11 pn A, 3.11 B, 4.11 A, 5.11 B, 6.11 A, 7.11 B, 8.11 nd: A pominięte (R-EKR-16) → B, 9.11 A, 10.11 B, ...
  std::string sek;
  for (auto& t : typy) sek += t.substr(2) + " ";
  ROWNE(sek, "A B A B A B B A B A B A ");
  SPRAWDZ(karty[1] && karty[4] && !karty[0] && !karty[2]);  // S2B zawsze z tabeli; S2A #3 (dzień 5) karta
  SPRAWDZ(karty[4] && typy[4] == "S2A");
  s.seria = 6;  // S2b tylko przy M-20 ≥ 7 [PRÓG]: seria 2–6 nocy to nie wydarzenie
  s.dzis = "2026-11-20";
  log = {wpis("2026-11-19", "S2A", "4", 'D')};
  SPRAWDZ(wybierzEkranDnia(s, log).typ == "S2A");

  // S6: rozruch globalny
  s.nocyWaznych = 10;
  SPRAWDZ(wybierzEkranDnia(s, log).typ == "S6");
  s.nocyWaznych = 40;
  // S-K-28: kamień 100 nocy raz
  s.nocyPonad4h = 100;
  EkranDnia k100 = wybierzEkranDnia(s, log);
  SPRAWDZ(k100.typ == "KM" && k100.klucz == "100" && k100.kartaZTabeli);
  log.push_back(wpis("2026-11-18", "KM", "100", 'K'));
  SPRAWDZ(wybierzEkranDnia(s, log).typ != "KM");
  s.nocDzisPonad4h = false;
  s.nocyPonad4h = 30;
  SPRAWDZ(wybierzEkranDnia(s, log).typ != "KM");  // ostatnia noc < 4 h: liczba nie „osiąga” progu tej nocy

  // R-HUM-08: S5 karta przy pierwszym wyświetleniu braku danych, potem ekran danych
  s.brakDanych = true;
  s.kluczBraku = "1761700000";
  EkranDnia s5 = wybierzEkranDnia(s, log);
  SPRAWDZ(s5.typ == "S5" && s5.kartaZTabeli);
  log.push_back(wpis(s.dzis, "S5", s.kluczBraku, 'K', "B04", "brak_danych:0"));
  SPRAWDZ(!wybierzEkranDnia(s, log).kartaZTabeli);
  s.kluczBraku = "1761800000";  // nowy brak danych → znowu karta
  SPRAWDZ(wybierzEkranDnia(s, log).kartaZTabeli);
  s.kartaBraku = false;  // U6: brak Wi-Fi albo zły raport → S5 zawsze ekranem danych
  SPRAWDZ(!wybierzEkranDnia(s, log).kartaZTabeli);
}

// U1: symulacja 48 h w krokach co 30 min, jak panel: noc z karty pojawia się w historii o 05:30.
// W slocie nocnym nie ma wyboru ekranu dnia ani wpisów w dzienniku, ekran nie jest przerysowywany po zegarze.
static void testDoba() {
  std::vector<Noc> h;
  for (int i = 1; i <= 40; i++) h.push_back(noc(dodajDni("2026-11-07", -i), 420, 1.0f, 10));  // do 06.11
  std::vector<WpisHumoru> log;
  std::vector<WpisCzynnikow> cz;
  int przerysowan = 0, nocnych = 0;
  std::string poranne;
  for (int krok = 0; krok < 4 * 48; krok++) {  // 4 doby od soboty 07.11 00:00
    std::string dzis = dodajDni("2026-11-07", krok / 48);
    int m = (krok % 48) * 30;
    if (m == 5 * 60 + 30 && !nocZDnia(h, dzis)) h.push_back(noc(dzis, 420, 1.0f, 10));  // import nocy
    WejscieDnia w;
    w.dzis = dzis;
    w.minuta = m;
    w.hist = &h;
    w.odczytOk = true;
    w.abData = dzis;
    w.abMinuta = m;
    Norma nm = liczNorme(h, dzis, wykluczoneZNormy(cz));
    w.maNorme = nm.m01.jest();
    poranekCzynnikow(cz, h, dzis, m, nm, 1);
    if (!przerysowacPoZegarze(m)) { nocnych++; continue; }
    przerysowan++;
    DecyzjaDnia d = decyzjaDnia(w, log);
    SPRAWDZ(d.nowyWybor);
    if (m < 5 * 60 + 30) SPRAWDZ(!d.zapisz);  // przed importem nocy: wybór tymczasowy
    if (!d.wpis && d.zapisz) log.push_back({dzis, d.e.typ, d.e.klucz, 'D', "", ""});
    if (m == 6 * 60) poranne += d.e.typ + " ";
  }
  // sobota S2A, niedziela S2B (S2a w niedzielę pominięte, R-EKR-16), poniedziałek S2A, wtorek S2B
  ROWNE(poranne, "S2A S2B S2A S2B ");
  SPRAWDZ(nocnych == 4 * 11 && przerysowan == 4 * 37);  // 23:30–04:30: 11 odświeżeń bez rysowania
  for (auto& x : log) SPRAWDZ(x.typ == "S2A" || x.typ == "S2B");
  // przed 05:30 (noc jeszcze nieznana z karty) wybór o 05:00 to rotacja bez serii; wpisy: najwyżej 2 na dobę
  SPRAWDZ(log.size() == 4);  // jeden wpis na dobę
  int s2aNiedz = 0;
  for (auto& x : log) s2aNiedz += x.data == "2026-11-08" && x.typ == "S2A";
  SPRAWDZ(s2aNiedz == 0);
  // w slocie nocnym decyzja bierze ostatni wpis i niczego nie wybiera
  WejscieDnia nocna;
  nocna.dzis = "2026-11-11";
  nocna.minuta = 2 * 60;
  nocna.hist = &h;
  DecyzjaDnia dn = decyzjaDnia(nocna, log);
  SPRAWDZ(!dn.nowyWybor && dn.wpis == &log.back() && dn.dataEkranu == log.back().data);
  // zaproszenia: najwyżej jedno na noc, nigdy z terminem w slocie nocnym
  for (auto& c : cz) SPRAWDZ(c.termin >= 11 * 60 && c.termin <= 23 * 60 + 30);
}

// U3: noc znana tylko z historii albo z odczytu AirBridge zrobionego dziś po 05:00
static void testNocZnana() {
  std::vector<Noc> h = {noc("2026-11-10", 420, 1)};
  WejscieDnia w;
  w.dzis = "2026-11-11";
  w.hist = &h;
  w.abData = "2026-11-11";
  w.abMinuta = 4 * 60 + 30;
  SPRAWDZ(!nocZnana(w) && brakDanych(w));  // odczyt o 04:30 to jeszcze nie noc z 11.11
  w.abMinuta = 5 * 60;
  SPRAWDZ(nocZnana(w) && !brakDanych(w));
  w.abData = "2026-11-10";
  w.abMinuta = 22 * 60;
  SPRAWDZ(!nocZnana(w));
  w.odczytOk = true;
  SPRAWDZ(!brakDanych(w));  // udany odczyt to nie S5
  // R-EKR-57 i E-01 od 11:00 (przypadek z egzemplarza: „awaria” rano, dane o 12:30): nieudany odczyt przed 11:00
  // to S10 „Synchronizuję się”, nie S5; od 11:00 S5; w slocie nocnym bez S10 (bez zmian)
  {
    WejscieDnia v;
    v.dzis = "2026-11-11";
    v.hist = &h;
    v.minuta = 8 * 60;
    SPRAWDZ(!brakDanych(v) && synchronizacja(v));
    SPRAWDZ(stanDnia(v).synchronizacja && wybierzEkranDnia(stanDnia(v), {}).typ == "S10");
    v.minuta = 11 * 60;
    SPRAWDZ(brakDanych(v) && !synchronizacja(v));
    v.odczytOk = true;  // po 11:00 AirBridge odpowiada, noc w budowie → S10
    SPRAWDZ(!synchronizacja(v));
    v.wBudowie = true;
    SPRAWDZ(!brakDanych(v) && synchronizacja(v));
    v.minuta = 8 * 60;
    v.terapia = true;  // maska jeszcze na twarzy: bez S10
    SPRAWDZ(!synchronizacja(v));
    v.terapia = false;
    v.e03 = true;  // nieczytelny raport: S5 z E-03, nie S10
    SPRAWDZ(brakDanych(v) && !synchronizacja(v));
    v.e03 = false;
    v.minuta = 60;  // slot nocny
    v.odczytOk = false;
    SPRAWDZ(!synchronizacja(v) && brakDanych(v));
    std::vector<Noc> zNoca = {noc("2026-11-11", 420, 1)};
    v.hist = &zNoca;
    v.minuta = 8 * 60;
    SPRAWDZ(!synchronizacja(v));  // noc już w historii
    // S10 to ekran przejściowy: bez wpisu w dzienniku (nie przesuwa rotacji ani limitów kart)
    v.hist = &h;
    DecyzjaDnia dd = decyzjaDnia(v, {});
    SPRAWDZ(dd.e.typ == "S10" && !dd.zapisz && dd.e.kartaZTabeli);
    // pula „synchronizacja” przyjęta (08.11.2026): zdanie także bez propozycji
    SPRAWDZ(!zdanieSynchronizacji("2026-11-11", true).empty() &&
            zdanieSynchronizacji("2026-11-11", false) == zdanieSynchronizacji("2026-11-11", true));
    SPRAWDZ(zdanieSynchronizacji("2026-11-11", true) != zdanieSynchronizacji("2026-11-12", true));
    SPRAWDZ(zdanieSynchronizacji("2026-11-11", true).find(':') == std::string::npos);  // tekst, nie ID zdania
    // awaria od wczoraj (odczyt 10.11 22:00, noc 11.11 bez danych) → 12.11 rano S5, nie S10 „zaraz”
    {
      WejscieDnia a;
      a.dzis = "2026-11-12";
      a.hist = &h;  // ostatnia noc w historii: 10.11
      a.abData = "2026-11-10";
      a.abMinuta = 22 * 60;
      a.minuta = 5 * 60;
      SPRAWDZ(brakDanych(a) && !synchronizacja(a));
      a.minuta = 10 * 60 + 59;
      SPRAWDZ(brakDanych(a) && !synchronizacja(a));
      a.dzis = "2026-11-11";  // dzień 1 tej samej awarii: rano S10, o 10:59 dalej S10, o 11:00 S5
      a.minuta = 10 * 60 + 59;
      SPRAWDZ(!brakDanych(a) && synchronizacja(a));
      a.minuta = 11 * 60;
      SPRAWDZ(brakDanych(a) && !synchronizacja(a));
    }
    // przegląd po 11:00 noc w budowie (status dziś po 05:00), potem jedno nieudane wybudzenie → nadal S10
    {
      WejscieDnia b;
      b.dzis = "2026-11-11";
      b.hist = &h;
      b.minuta = 12 * 60 + 30;
      b.wBudowie = true;
      b.abData = "2026-11-11";
      b.abMinuta = 12 * 60;
      b.odczytOk = false;
      SPRAWDZ(!brakDanych(b) && synchronizacja(b));
      SPRAWDZ(!decyzjaDnia(b, {}).zapisz);
    }
    SPRAWDZ(nocyOdOdczytu("2026-11-12", "2026-11-10", 22 * 60) == 2 && nocyOdOdczytu("2026-11-12", "2026-11-10", 4 * 60) == 3);
    SPRAWDZ(nocyOdOdczytu("2026-11-12", "", -1) == 0);
    std::vector<std::string> s10 = scenyTypu("S10", "");
    SPRAWDZ(s10.size() == 1 && s10[0] == "B05" && std::string(pulaTypu("S10")) == "synchronizacja");
  }
  w.odczytOk = false;
  w.dzis = "2026-11-10";
  SPRAWDZ(nocZnana(w));  // jest w historii
  SPRAWDZ(oknoDnia(5 * 60) && !oknoDnia(4 * 60 + 59) && !oknoDnia(23 * 60 + 30) && oknoDnia(23 * 60 + 29));
}

static void testNawigacja() {
  KontekstNaw k;
  auto P = [&](Ekran e, Przycisk p, bool d) { return nacisniecie(e, p, d, k); };
  // ▶ dalej (K1), ◀ wstecz (z ekranu dnia nic), ↻ dom (na ekranie dnia nic)
  SPRAWDZ(P(Ekran::DZIEN, Przycisk::PRAWY, false).ekran == Ekran::K1);
  SPRAWDZ(P(Ekran::DZIEN, Przycisk::SRODEK, false).akcja == Akcja::NIC && P(Ekran::DZIEN, Przycisk::SRODEK, false).ekran == Ekran::DZIEN);
  SPRAWDZ(P(Ekran::DZIEN, Przycisk::LEWY, false).akcja == Akcja::NIC && P(Ekran::DZIEN, Przycisk::LEWY, false).ekran == Ekran::DZIEN);
  SPRAWDZ(P(Ekran::DZIEN, Przycisk::SRODEK, true).akcja == Akcja::NIC);  // bez linii zaproszenia: nic
  SPRAWDZ(P(Ekran::DZIEN, Przycisk::LEWY, true).akcja == Akcja::NIC);
  k.zaproszenie = true;
  SPRAWDZ(P(Ekran::DZIEN, Przycisk::SRODEK, true).ekran == Ekran::PYTANIE);  // R-CZY-17
  k.s5 = true;
  SPRAWDZ(P(Ekran::DZIEN, Przycisk::SRODEK, false).akcja == Akcja::PONOW_POBRANIE);  // R-PRZ-06
  SPRAWDZ(P(Ekran::DZIEN, Przycisk::PRAWY, false).ekran == Ekran::K1);
  k = KontekstNaw();
  SPRAWDZ(P(Ekran::K1, Przycisk::SRODEK, false).ekran == Ekran::DZIEN);
  SPRAWDZ(P(Ekran::K1, Przycisk::SRODEK, true).akcja == Akcja::NIC);
  k.ostatniaNoc = true;
  SPRAWDZ(P(Ekran::K1, Przycisk::SRODEK, true).ekran == Ekran::PYTANIE);  // R-CZY-08
  SPRAWDZ(P(Ekran::K1, Przycisk::LEWY, false).ekran == Ekran::DZIEN);  // ◀ z K1 do ekranu dnia
  SPRAWDZ(P(Ekran::K1, Przycisk::PRAWY, false).ekran == Ekran::K2);    // ▶ z K1 do K2
  SPRAWDZ(P(Ekran::K2, Przycisk::LEWY, false).ekran == Ekran::K1);
  SPRAWDZ(P(Ekran::K2, Przycisk::LEWY, true).akcja == Akcja::NIC);     // U-01 to strona 6 / 6, nie przytrzymanie
  SPRAWDZ(P(Ekran::K2, Przycisk::SRODEK, false).ekran == Ekran::DZIEN);
  SPRAWDZ(P(Ekran::K2, Przycisk::SRODEK, true).akcja == Akcja::NIC);   // „Sprzęt” to strona 5 / 6
  SPRAWDZ(P(Ekran::PYTANIE, Przycisk::SRODEK, false).akcja == Akcja::PYT_ZAZNACZ);
  SPRAWDZ(P(Ekran::PYTANIE, Przycisk::SRODEK, true).akcja != Akcja::PYT_ZAPISZ);  // bez ruchu w siatce bez zapisu
  k.pytanieRuszone = true;
  Przejscie z = P(Ekran::PYTANIE, Przycisk::SRODEK, true);
  SPRAWDZ(z.akcja == Akcja::PYT_ZAPISZ && z.ekran == Ekran::DZIEN);  // R-CZY-10
  z = P(Ekran::PYTANIE, Przycisk::LEWY, true);
  SPRAWDZ(z.akcja == Akcja::PYT_POMIN && z.ekran == Ekran::DZIEN);   // R-CZY-14
  SPRAWDZ(P(Ekran::PYTANIE, Przycisk::LEWY, false).akcja == Akcja::PYT_WSTECZ);
  SPRAWDZ(P(Ekran::PYTANIE, Przycisk::PRAWY, false).akcja == Akcja::PYT_DALEJ);
  // R-PRZ-02, R-CZY-16, R-PRZ-18
  SPRAWDZ(sekundDoPowrotu(Ekran::K1, 0, 600) == 300 && sekundDoPowrotu(Ekran::K2, 400, 600) == 0);
  SPRAWDZ(sekundDoPowrotu(Ekran::PYTANIE, 10, 7 * 60) == 290);
  SPRAWDZ(sekundDoPowrotu(Ekran::DZIEN, 0, 600) == -1);
  SPRAWDZ(sekundDoPowrotu(Ekran::K1, 0, 23 * 60 + 40) == -1 && sekundDoPowrotu(Ekran::K1, 0, 2 * 60 + 10) == -1);
  SPRAWDZ(slotNocny(23 * 60 + 30) && !slotNocny(23 * 60 + 29) && !slotNocny(5 * 60) && slotNocny(4 * 60 + 59));
}

static void testEkrany() {
  TrescEkranu t;
  t.tytul = "17. noc z rzędu ponad 4 h.";
  t.powod = "";
  t.krok = "";
  SPRAWDZ(kartaMiesci(t));
  t.tytul = "Bardzo długi tytuł, który nie zmieści się nawet w dwóch liniach na karcie i musi przejść w tryb danych.";
  SPRAWDZ(!kartaMiesci(t));  // R-HUM-05
  t.tytul = "Brak danych z nocy 29.10.";
  t.krok = "→ Sprawdź zasilanie AirBridge.";
  t.powod = "AirBridge nie odpowiada od 05:42. Ponawiam co\xC2\xA0" "30\xC2\xA0" "min.";
  SPRAWDZ(kartaMiesci(t));
  SPRAWDZ(szer(F_P22B, "◀") > 10 && szer(F_P22B, "a◆") > szer(F_P22B, "a"));
  auto l = zawin(F_P22B, "w\xC2\xA0nocy w nocy w nocy w nocy", 60);
  SPRAWDZ(l.size() >= 2 && l[0].find("w\xC2\xA0nocy") == 0);  // twarda spacja nie łamie
  SPRAWDZ(scenaJest("B01") && scenaJest("B04") && scenaJest("B05") && scenaJest("M01") && scenaJest("M05") && sceny().size() == 70);  // komplet 5.7.4
  SPRAWDZ(scenaJest("D05") && scenaJest("D07") && scenaJest("D08"));  // sceny S2b bez medalu
}

static void testDzien() {
  std::vector<Noc> h;
  for (int i = 0; i < 20; i++) h.push_back(noc(dodajDni("2026-10-28", -i), 420, 1.0f, 10));
  h.push_back(noc("2026-10-29", 398, 4.5f, 31));
  DaneDnia d;
  d.dzis = "2026-10-29";
  d.hist = &h;
  d.norma = liczNorme(h, d.dzis, {});
  d.przyLozku = stopkaCzujnika(22.2f, 45.4f);
  // nagłówek wszystkich ekranów dnia i kart bez liczb nocy
  ROWNE(naglowekDniaLewo(d), "cz 29.10");
  TrescEkranu a = trescDnia("S2A", "1", d);
  ROWNE(a.tytul, "Noc: 6 h 38 z maską.");  // K22: tytuł bez liczb; noc z odchyleniem AHI (4,5 > P90 1,0 i ≥ mediana + 3)
  ROWNE(a.stopkaPrawo, "przy łóżku 22° · 45%");  // stopka: odczyt czujnika panelu, nie `sypialnia`
  ROWNE(a.stopkaLewo, "");  // R-AW-07: bez pogody stopka bez lewej części i bez komunikatu
  TrescEkranu b = trescDnia("S2B", "", d);
  ROWNE(b.tytul, "21 nocy z rzędu z maską.");  // bez „ponad 4 h” w tytule
  ROWNE(std::string(nocyZRzedu(7)), "nocy");
  ROWNE(std::string(nocyZRzedu(22)), "noce");
  ROWNE(std::string(nocyZRzedu(12)), "nocy");
  ROWNE(std::string(nocyZRzedu(104)), "noce");
  SPRAWDZ(b.kropka[29] && b.uklad == Uklad::SERIA && b.seria == 21);
  TrescEkranu k = trescDnia("KM", "100", d);
  ROWNE(k.tytul, "100 nocy z maską.");
  ROWNE(k.powod, "Pierwsza noc: 9.10.2026. Każda się liczy.");  // bez powtórzenia liczby, z rokiem
  ROWNE(k.krok, "→ Następny cel: 180 nocy.");
  ROWNE(trescDnia("KM", "365", d).krok, "→ Następny cel: kolejny rok.");
  SPRAWDZ(kartaMiesci(k));
  d.ostatnieDane = "05:42";
  // E-01, 1. noc (dziś 29.10, w historii bez nocy 29.10): łagodny „czekam na dane”, bez kroku (R-AW-32)
  auto bezOd = [&h](const std::string& od) {  // historia bez nocy od tej daty
    std::vector<Noc> w;
    for (auto& x : h)
      if (x.data < od) w.push_back(x);
    return w;
  };
  std::vector<Noc> h1 = bezOd("2026-10-29");  // bez nocy 29.10: brak danych dziś
  d.hist = &h1;
  TrescEkranu s = trescDnia("S5", "1", d);
  ROWNE(s.etykieta, "CZEKAM NA DANE");
  ROWNE(s.tytul, "Noc 29.10 jeszcze w drodze.");
  ROWNE(s.powod, "AirBridge milczy od 05:42. Pytam co\xC2\xA0" "30\xC2\xA0" "min.");
  SPRAWDZ(s.krok.empty());
  // 2. noc bez danych: wciąż łagodnie, zdanie o wyjeździe (R-AW-24)
  std::vector<Noc> h2 = bezOd("2026-10-28");
  d.hist = &h2;
  TrescEkranu s2 = trescDnia("S5", "1", d);
  ROWNE(s2.etykieta, "CZEKAM NA DANE");
  ROWNE(s2.tytul, "Brak danych od 2 nocy");
  ROWNE(s2.powod, "Wyjazd z aparatem? Dane wrócą same. Sprawdzam co\xC2\xA0" "3\xC2\xA0" "h.");
  SPRAWDZ(s2.krok.empty());
  // 3. noc: dopiero teraz „awaria” z krokiem (R-AW-24); `od` = dzień ostatniej nocy z danymi
  std::vector<Noc> h3 = bezOd("2026-10-27");
  d.hist = &h3;
  TrescEkranu s3n = trescDnia("S5", "1", d);
  ROWNE(s3n.etykieta, "BRAK DANYCH Z NOCY");
  ROWNE(s3n.tytul, "Brak danych od 3 nocy");
  ROWNE(s3n.powod, "AirBridge nie odpowiada od 26.10. Sprawdzam co\xC2\xA0" "3\xC2\xA0" "h.");  //
  ROWNE(s3n.krok, "→ Sprawdź AirBridge i sieć. Ustawienia: strona 6 / 6.");
  d.hist = &h;
  SPRAWDZ(nocyBezDanych(d) == 1);  // noc dziś jest: licznik dla S5 zaczyna się od 1 (nie ma ujemnych)
  std::vector<Noc> pusta;
  d.hist = &pusta;
  SPRAWDZ(nocyBezDanych(d) == 1);  // nowy panel bez historii: 1. noc, nie „od 366 nocy”
  // licznik od ostatniego udanego odczytu: 2 noce bez maski przy działającym AirBridge (brak rekordów
  // 27.10 i 28.10, odczyt 28.10 08:00) i milczenie od dziś → 1. noc, nie „od 3 nocy”
  d.hist = &h3;
  d.abData = "2026-10-28";
  d.abMinuta = 8 * 60;
  SPRAWDZ(nocyBezDanych(d) == 1);
  ROWNE(trescDnia("S5", "1", d).etykieta, "CZEKAM NA DANE");
  d.abData = "2026-10-26";  // ostatni odczyt 26.10 08:00 → noce 27, 28, 29 bez danych, `od 26.10`
  SPRAWDZ(nocyBezDanych(d) == 3);
  ROWNE(trescDnia("S5", "1", d).powod, "AirBridge nie odpowiada od 26.10. Sprawdzam co\xC2\xA0" "3\xC2\xA0" "h.");
  d.abMinuta = 4 * 60;  // odczyt przed 05:00 nie zna jeszcze nocy z 26.10
  SPRAWDZ(nocyBezDanych(d) == 3);  // ograniczone luką w historii (noc 26.10 jest w historii)
  d.abData = "2025-10-26";  // odczyt sprzed roku: data z rokiem
  d.abMinuta = 8 * 60;
  ROWNE(trescDnia("S5", "1", d).powod, "AirBridge nie odpowiada od 26.10.2025. Sprawdzam co\xC2\xA0" "3\xC2\xA0" "h.");
  d.abData.clear();
  d.abMinuta = -1;
  d.hist = &h1;
  // S10: synchronizacja, bez kroku, ↻ pobiera teraz
  TrescEkranu sy = trescDnia("S10", "", d);
  ROWNE(sy.etykieta, "CZEKAM NA DANE");
  ROWNE(sy.tytul, "Synchronizuję się z aparatem.");
  SPRAWDZ(sy.krok.empty() && sy.stopkaPrawo == "↻ pobierz teraz" && kartaMiesci(sy));
  SPRAWDZ(sy.powod.find("29.10") != std::string::npos);
  ROWNE(s.stopkaPrawo, "↻ ponów");  // R-PRZ-04: akcja niestandardowa S5
  // E-01 ma godzinę w powodzie, więc bez linii `ostatnie dane` i bez faktu na karcie
  SPRAWDZ(s.fakt.empty() && s.fakty.empty());
  d.httpKod = KOD_E03;
  TrescEkranu s3 = trescDnia("S5", "1", d);
  ROWNE(s3.fakt, "ostatnie dane: 05:42");  // E-03: godziny w powodzie nie ma, linia faktu zostaje
  SPRAWDZ(s3.powod.find("05:42") == std::string::npos);
  d.httpKod = 0;
  SPRAWDZ(kartaMiesci(s));
  d.linia = "Co było inaczej? ↻ 2 s";
  ROWNE(trescDnia("S2A", "1", d).linia, "Co było inaczej? ↻ 2 s");
  ROWNE(opisNocyPytania(h.back(), d.norma, true), "AHI 4,5 przy zwykłych 1,0–1,0, wyciek 31 l/min przy zwykłych 10–10");
  ROWNE(opisNocyPytania(h.back(), d.norma, false), "6:38 · AHI 4,5");
  std::vector<Noc> mal(h.end() - 5, h.end());
  d.hist = &mal;
  d.norma = liczNorme(mal, d.dzis, {});
  TrescEkranu r = trescDnia("S6", "", d);
  SPRAWDZ(r.postep == 4);  // 4 noce przed dziś w oknie normy
  ROWNE(r.postepLewo, "noc 4 z 14");
  ROWNE(r.postepPrawo, "pełna norma od ok. 08.11");
  // U8: 21 nocy ważnych, ale wszystkie starsze niż 90 dni → nie „noc 21 z 14”
  d.hist = &h;
  d.dzis = "2027-03-01";
  d.norma = liczNorme(h, d.dzis, {});
  r = trescDnia("S6", "", d);
  ROWNE(r.postepLewo, "brak nowych nocy");
  SPRAWDZ(r.postep == 0);
  // U7: godzina odczytu z datą, gdy odczyt nie jest z dziś
  ROWNE(kiedyOdczyt("2026-10-29", 342, "2026-10-29"), "05:42");
  ROWNE(kiedyOdczyt("2026-10-28", 342, "2026-10-29"), "28.10 05:42");
  ROWNE(kiedyOdczyt("", -1, "2026-10-29"), "");
  // U6: zły raport → prawdziwy powód
  d.dzis = "2026-10-29";
  d.httpKod = -200;
  ROWNE(trescDnia("S5", "1", d).powod, "Format raportu zmienił się.");
  d.abWersja = "v1.6.2";  // E-03: AirBridge spoza obsługiwanych wersji (Załącznik A)
  ROWNE(trescDnia("S5", "1", d).powod, "Format raportu zmienił się (AirBridge v1.6.2).");
  ROWNE(trescDnia("S5", "1", d).krok, "→ Daj znać osobie, która instalowała panel.");
  ROWNE(trescDnia("S5", "1", d).tytul, "Nie rozumiem danych AirBridge");
  d.abWersja.clear();
  // U11: K1 bez nocy nie podpisuje pytania
  SPRAWDZ(!budujK1({}, "2026-10-29", Norma(), {}).jestNoc && budujK1(h, "2026-10-29", Norma(), {}).jestNoc);
}

// ---------- tryb ASV: EPAP (M-06) i wsparcie PS (M-28) ----------
static Noc nocAsv(const std::string& d, int m01, float ahi, float emed, float ep95, float psmed, float psp95) {
  Noc n = noc(d, m01, ahi, 10, 13.5f, 15.0f);
  n.emed = emed;
  n.ep95 = ep95;
  n.psmed = psmed;
  n.psp95 = psp95;
  return n;
}

static void testAsv() {
  Noc r;
  // stary import (8 pól): EPAP i PS = brak danych
  SPRAWDZ(parsujNoc("2026-10-28;1;414;1.20;9.40;11.30;4.00;12.00", 'K', r));
  SPRAWDZ(std::isnan(r.emed) && std::isnan(r.ep95) && std::isnan(r.psmed) && std::isnan(r.psp95) && r.zrodlo == 'K');
  SPRAWDZ(std::fabs(r.wp95 - 12.0f) < 1e-6);
  // stary /noce.csv (9 pól, źródło na końcu)
  SPRAWDZ(parsujNoc("2026-10-28;1;414;1.20;9.40;11.30;4.00;12.00;A", 'K', r) && r.zrodlo == 'A' && std::isnan(r.emed));
  // nowy import z historia.py (12 pól)
  SPRAWDZ(parsujNoc("2026-10-28;1;414;1.20;14.10;16.20;4.00;12.00;7.20;8.10;5.40;9.30", 'K', r));
  SPRAWDZ(r.zrodlo == 'K' && std::fabs(r.emed - 7.2f) < 1e-6 && std::fabs(r.ep95 - 8.1f) < 1e-6 &&
          std::fabs(r.psmed - 5.4f) < 1e-6 && std::fabs(r.psp95 - 9.3f) < 1e-6 && std::fabs(r.cp95 - 16.2f) < 1e-6);
  // 12 pól z pustymi (brak EPAP/PS) i \r na końcu wiersza
  SPRAWDZ(parsujNoc("2026-10-28;0;;;;;;;;;;\r", 'K', r) && r.m01 == -1 && std::isnan(r.psp95) && r.zrodlo == 'K');
  SPRAWDZ(parsujNoc("2026-10-28;1;414;1.2;;;;;7.2;;;9.3\r", 'K', r) && std::isnan(r.ep95) && std::fabs(r.psp95 - 9.3f) < 1e-6);
  // import z ustawieniami (13 pól) i nowy /noce.csv (14 pól): zapis i odczyt bez zmian
  SPRAWDZ(parsujNoc("2026-10-28;1;414;1.2;;;;;7.2;;;9.3;ASVAuto/6/10/3/15\r", 'K', r));
  ROWNE(r.ustawienia, "ASVAuto/6/10/3/15");
  SPRAWDZ(r.zrodlo == 'K' && std::fabs(r.psp95 - 9.3f) < 1e-6);
  ROWNE(zapiszNoc(r), "2026-10-28;1;414;1.20;;;;;7.20;;;9.30;ASVAuto/6/10/3/15;K;0;0;;;;");
  Noc r2;
  SPRAWDZ(parsujNoc(zapiszNoc(r), 'A', r2) && r2.zrodlo == 'K' && zapiszNoc(r2) == zapiszNoc(r));
  SPRAWDZ(parsujNoc("2026-10-28;1;414;1.2;;;;;7.2;;;9.3;", 'K', r) && r.ustawienia.empty());  // puste = nieznane
  Noc stara;  // stary wiersz → nowy zapis: EPAP, PS i ustawienia puste
  SPRAWDZ(parsujNoc("2026-10-28;1;414;1.20;9.40;11.30;4.00;12.00;A", 'K', stara));
  ROWNE(zapiszNoc(stara), "2026-10-28;1;414;1.20;9.40;11.30;4.00;12.00;;;;;;A;0;0;;;;");
  // zła liczba pól, źródło inne niż A/K albo śmieci w polu EPAP/PS = błąd wiersza
  SPRAWDZ(!parsujNoc("2026-10-28;1;414;1.2;;;;;7.2", 'K', r));        // 9 pól: 9. pole to nie źródło A/K
  SPRAWDZ(!parsujNoc("2026-10-28;1;414;1.2;;;;;7.2;8.1", 'K', r));    // 10 pól
  SPRAWDZ(!parsujNoc("2026-10-28;1;414;1.2;;;;;7.2;8.1;5.4", 'K', r));  // 11 pól
  SPRAWDZ(!parsujNoc("2026-10-28;1;414;1.2;;;;;7,2;8.1;5.4;9.3", 'K', r));  // przecinek dziesiętny
  SPRAWDZ(!parsujNoc("2026-10-28;1;414;1.2;;;;;7.2;8.1;5.4;9.3;u;X", 'K', r));  // źródło X
  SPRAWDZ(!parsujNoc("2026-10-28;1;414;1.2;;;;;7.2;8.1;5.4;9.3;u;K;x", 'K', r));  // 15 pól
  SPRAWDZ(!parsujNoc("data_poranka;wazna;m01_min;ahi;cisnienie_med;cisnienie_p95;wyciek_med;wyciek_p95;epap_med;epap_p95;"
                     "ps_med;ps_p95;ustawienia",
                     'K', r));  // nagłówek importu

  // norma EPAP i PS: osobno dla każdej metryki, minimum 14 nocy z wartością
  std::vector<Noc> h;
  for (int i = 1; i <= 30; i++)  // EPAP 7,0..9,9 (krok 0,1); PS 5,0..7,9; P95 o 1 wyżej
    h.push_back(nocAsv(dodajDni("2026-10-29", -31 + i), 400, 1.0f, 7.0f + 0.1f * (i - 1), 8.0f + 0.1f * (i - 1),
                       5.0f + 0.1f * (i - 1), 6.0f + 0.1f * (i - 1)));
  Norma nm = liczNorme(h, "2026-10-29", {});
  SPRAWDZ(nm.emed.n == 30 && nm.ep95.n == 30 && nm.psmed.n == 30 && nm.psp95.n == 30);
  // 30 wartości: mediana = średnia 15. i 16. (7,0 + 1,4 + 7,0 + 1,5) / 2 = 8,45; P10 ranga 3 → 7,2; P90 ranga 27 → 9,6
  SPRAWDZ(std::fabs(nm.emed.med - 8.45) < 1e-4 && std::fabs(nm.emed.p10 - 7.2) < 1e-4 && std::fabs(nm.emed.p90 - 9.6) < 1e-4);
  SPRAWDZ(std::fabs(nm.ep95.med - 9.45) < 1e-4 && std::fabs(nm.psmed.med - 6.45) < 1e-4 && std::fabs(nm.psp95.med - 7.45) < 1e-4);
  std::vector<Noc> h13 = h;
  for (int i = 0; i < 17; i++) h13[i].psmed = NAN;  // PS tylko w 13 nocach → brak normy PS, EPAP dalej jest
  Norma nm2 = liczNorme(h13, "2026-10-29", {});
  SPRAWDZ(nm2.psmed.n == 13 && !nm2.psmed.jest() && std::isnan(nm2.psmed.med) && nm2.emed.jest() && nm2.m01.jest());
  // stary import bez EPAP: norma EPAP pusta, norma czasu i AHI bez zmian
  std::vector<Noc> stare;
  for (int i = 1; i <= 20; i++) stare.push_back(noc(dodajDni("2026-10-29", -i), 400, 1.0f, 10, 9, 11));
  Norma ns = liczNorme(stare, "2026-10-29", {});
  SPRAWDZ(ns.m01.jest() && ns.ahi.jest() && ns.emed.n == 0 && !ns.psp95.jest());
  // EPAP i PS nie wywołują odchylenia ani pytania (ADR-0002: bez reguł)
  Noc dziwna = nocAsv("2026-10-29", 420, 1.0f, 15.0f, 16.0f, 15.0f, 16.0f);
  SPRAWDZ(!odchylenieWNocy(dziwna, nm).jakiekolwiek());

  // K1: kafel EPAP, wiersze EPAP i Wsparcie
  h.push_back(nocAsv("2026-10-29", 420, 1.1f, 7.2f, 8.1f, 5.4f, 9.3f));
  DaneK1 k1 = budujK1(h, "2026-10-29", nm, {});
  ROWNE(k1.liczby[2], "7,2");
  ROWNE(k1.opisy[2], "EPAP");  // R-KOK-15: opis `EPAP`
  ROWNE(k1.w[2].nazwa, "EPAP");
  ROWNE(k1.w[3].nazwa, "Wsparcie");
  ROWNE(k1.w[4].nazwa, "Wyciek");
  SPRAWDZ(k1.w[3].linia.ma[29] && std::fabs(k1.w[3].linia.v[29] - 5.4f) < 1e-6);
  ROWNE(k1.w[2].slowo, "rośnie");  // EPAP bez stanu odchylenia; 7,0 → 9,9 w 30 nocach: trend R-KOK-16
  ROWNE(k1.w[3].slowo, "rośnie");  // PS z Δ 2 cmH2O jak EPAP: 23 noce 6,2, 7 nocy 7,6
  ROWNE(budujK1(h, "2026-10-29", nm2, {}).w[3].slowo, "uczę się");  // PS bez normy
  // noc bez EPAP (stary import z karty albo z AirBridge, np. doba tylko z podsumowania STR): kreska i podpis EPAP;
  // ciśnienie wdechowe nie zastępuje EPAP (bez podpisów „ciśnienie”)
  std::vector<Noc> ab = {noc("2026-10-29", 300, 1.0f, 12, 5.2f, 6.0f)};
  DaneK1 ka;
  for (char zr : {'A', 'K'}) {
    ab[0].zrodlo = zr;
    ka = budujK1(ab, "2026-10-29", Norma(), {});
    ROWNE(ka.liczby[2], "–");
    ROWNE(ka.opisy[2], "EPAP");
  }

  // K2: wiersze EPAP P95 i Wsparcie PS (mediana), bez „ponad próg”
  DaneK2 k2 = budujK2(h, "2026-10-29", nm, {});
  ROWNE(k2.w[7].etykieta, "EPAP P95");
  ROWNE(k2.w[8].etykieta, "Wsparcie PS");
  ROWNE(k2.w[9].etykieta, "Oddechy /min");  // T5: jednostka w etykiecie, bez `cmH₂O` (kolumna etykiet 148 px)
  ROWNE(k2.w[6].etykieta, "Wyciek l/min");
  ROWNE(k2.w[0].etykieta, "AHI");
  ROWNE(k2.w[7].k[6].tekst, "8,1");
  ROWNE(k2.w[8].k[6].tekst, "5,4");
  ROWNE(k2.w[7].norma, "8,2–10,6");
  ROWNE(k2.w[8].norma, "5,2–7,6");
  SPRAWDZ(k2.w[4].grupa && k2.w[6].grupa && !k2.w[0].grupa && !k2.w[5].grupa && !k2.w[7].grupa);  // 2 linie grup
  bool prog = false;
  for (int i = 0; i < 7; i++) prog = prog || k2.w[7].k[i].ponadProg || k2.w[8].k[i].ponadProg;
  SPRAWDZ(!prog);  // EPAP i PS bez progów (ADR-0002)
  for (int r = 0; r < DaneK2::WIERSZE; r++) SPRAWDZ(std::string(k2.w[r].etykieta).find("Ciśnienie") == std::string::npos);

  // ekran dnia S2a: wiersz EPAP z medianą 7 nocy i słowem porównania (5.4.2, R-KOK-15), bez linii technicznych
  DaneDnia d;
  d.dzis = "2026-10-29";
  d.hist = &h;
  d.norma = liczNorme(h, d.dzis, {});
  TrescEkranu t = trescDnia("S2A", "1", d);
  ROWNE(t.kolumny[2].et, "EPAP");
  ROWNE(t.kolumny[2].w, przecinek1(mediana(wartosci7(h, "2026-10-29", poleEmed))));
  SPRAWDZ(t.kolumny[2].p.find("lepiej") == std::string::npos);  // EPAP bez strony „lepiej” (ADR-0002)
  for (auto& x : {t.tytul, t.powod, t.kolumny[0].p, t.kolumny[1].p, t.kolumny[2].p, t.kolumny[3].p})
    SPRAWDZ(x.find("v1.6.2") == std::string::npos && x.find("Norma") == std::string::npos &&
            x.find("ostatnia sesja") == std::string::npos && x.find("cmH2O") == std::string::npos);
  // bez nocy w historii: tytuł bez liczb, kolumny bez wartości
  std::vector<Noc> bez;
  d.hist = &bez;
  d.norma = Norma();
  t = trescDnia("S2A", "1", d);
  ROWNE(t.tytul, "Brak zapisanych nocy.");
  ROWNE(t.kolumny[2].w, "–");
  ROWNE(t.kolumny[0].p, "uczę się");
  ROWNE(naglowekDniaLewo(d), "cz 29.10");

  // szerokości (px z krojów, nie na oko): nic nie wychodzi poza swoje miejsce (SPEC-KOKPIT 3–4, SPEC-B 4)
  for (int r = 0; r < DaneK2::WIERSZE; r++)  // K2: etykieta (z wcięciem 14 px) w kolumnie 20–164
    SPRAWDZ(20 + (k2.w[r].wciecie ? 14 : 0) + szer(F_P22B, k2.w[r].etykieta) <= 164);
  for (const char* e : {"AHI", "Czas z maską", "Wyciek l/min", "EPAP P95"})  // etykiety ze stanem odchylenia
    SPRAWDZ(20 + szer(F_P22B, e) + 12 <= 168 + 3);  // w ramce (R-KOK-09) do lewej krawędzi 1. komórki nocy
  for (int r = 0; r < DaneK2::WIERSZE; r++)
    for (int i = 0; i < 7; i++)
      if (szer(F_P22B, k2.w[r].k[i].tekst) > 64 - 7) { zle++; printf("BŁĄD komórka K2 %s: %s\n", k2.w[r].etykieta, k2.w[r].k[i].tekst.c_str()); }
      else ok++;  // komórka 64 px, tekst 5 px od prawej krawędzi
  SPRAWDZ(szer(F_P22B, "10:59") <= 64 - 7 && szer(F_P22B, "12,4") <= 64 - 7);  // najgorsze komórki (SPEC-KOKPIT 4)
  for (int r = 0; r < DaneK2::WIERSZE; r++) SPRAWDZ(szer(F_P22B, k2.w[r].norma) <= 780 - 647 - 2);  // kolumna normy
  SPRAWDZ(szer(F_P22B, "9:48–10:59") <= 780 - 647 - 2 && szer(F_P22B, "uczę się") <= 780 - 647 - 2);
  // nagłówek ostatniej nocy z rombem (romb P22B, dzień P24B) nie wchodzi na nagłówek poprzedniej nocy
  for (const char* dn : {"pn 30", "wt 30", "śr 30", "cz 30", "pt 30", "sb 30", "nd 30"})  // skróty 2-literowe
    SPRAWDZ(szer(F_P22B, dn) <= 68 - 6);  // nagłówek P22B w kolumnie 68 px, co najmniej 6 px przerwy do sąsiedniego
  // K1: opisy nad liczbami w kolumnach 28, 222, 398, 608; nazwy wierszy przed linią 30 nocy (196); ocena 522–772
  const int kolx[5] = {28, 222, 398, 608, 772};
  for (int i = 0; i < 4; i++) {
    SPRAWDZ(kolx[i] + szer(F_P22B, ka.opisy[i]) <= kolx[i + 1] - 12 && kolx[i] + szer(F_P22B, k1.opisy[i]) <= kolx[i + 1] - 12);
    SPRAWDZ(kolx[i] + szer(F_SN56, "10:59") <= kolx[i + 1] - 12);
  }
  SPRAWDZ(398 + szer(F_P22B, "EPAP (urządzenie)") <= 608 - 12);
  for (int i = 0; i < DaneK1::WIERSZE; i++) SPRAWDZ(28 + szer(F_P24B, k1.w[i].nazwa) <= 196 - 6);
  SPRAWDZ(522 + szer(F_P22B, "wyżej niż zwykle") <= 772 && 522 + szer(F_P22B, "od 999 nocy") <= 772);  // T3, 2 linie
  SPRAWDZ(522 + szer(F_P22B, "wyżej niż zwykle, od 12 nocy") > 772);  // dlatego 2 linie
  SPRAWDZ(szer(F_P28B, "10:59") <= 506 - 410);  // mediana do prawej na 506, za linią 30 nocy (196–406)
  d.hist = &h;
  d.norma = liczNorme(h, d.dzis, {});
  t = trescDnia("S2A", "1", d);
  // S2a: kolumna paska 186 px (nazwa B6, wartość B5 + strzałka, słowo B7; w ramce 2 px pole 184 px od kx − 5)
  for (auto& k : t.kolumny) SPRAWDZ(szer(F_P24B, k.et) <= 172 && szer(F_P22B, k.p) <= 173);
  for (const char* sl : {"lepiej niż zwykle", "wyżej niż zwykle", "niżej niż zwykle", "uczę się"}) SPRAWDZ(szer(F_P22B, sl) <= 173);
  SPRAWDZ(szer(F_P28B, "12:59") + 8 + szer(F_P28B, "↑") <= 172 && szer(F_P28B, "35,5°") + 8 + szer(F_P28B, "↑") <= 172);
  for (const char* n : {"czas z maską", "AHI", "wyciek l/min", "temperatura", "wilgotność"}) SPRAWDZ(szer(F_P24B, n) <= 172);
}

// R-NOR-07 w uproszczeniu: normy ciśnienia, wycieku i PS tylko z nocy z ustawieniami najnowszej nocy.
// Zmiana minimalnego wsparcia ok. 19 nocy temu → norma PS tylko z nocy nowych ustawień.
static void testUstawienia() {
  const std::string dzis = "2026-10-29", u4 = "ASVAuto/6/10/2/15", u6 = "ASVAuto/6/10/3/15";
  std::vector<Noc> h;
  for (int i = 60; i >= 0; i--) {  // 61 nocy; od dzis − 18 (19 nocy z dziś) nowych ustawień
    bool nowe = i <= 18;
    Noc n = nocAsv(dodajDni(dzis, -i), 420, 1.0f + (nowe ? 0 : 1), 7.0f, 8.0f, nowe ? 6.0f : 4.0f, nowe ? 8.0f : 6.0f);
    n.wp95 = nowe ? 20 : 10;
    n.ustawienia = nowe ? u6 : u4;
    if (i == 5 || i == 12) n.ustawienia.clear();  // puste = jak poprzednie znane (nowe ustawienia)
    if (i == 40) n.ustawienia.clear();            // puste wśród stare = stare
    h.push_back(n);
  }
  Norma nm = liczNorme(h, dzis, {});
  ROWNE(nm.ustawienia, u6);
  SPRAWDZ(nm.psmed.n == 18 && nm.psmed.jest() && nm.psmed.med == 6.0 && nm.psmed.p10 == 6.0 && nm.psmed.p90 == 6.0);
  SPRAWDZ(nm.psp95.n == 18 && nm.psp95.med == 8.0);
  SPRAWDZ(nm.wp95.n == 18 && nm.wp95.med == 20 && nm.emed.n == 18 && nm.ep95.n == 18 && nm.cmed.n == 18 && nm.cp95.n == 18);
  // czas z maską i AHI bez filtra ustawień: 30 ostatnich nocy przed dziś (18 z nowymi, 12 ze starymi ustawieniami)
  SPRAWDZ(nm.m01.n == 30 && nm.ahi.n == 30 && nm.ahi.med == 1.0);
  // świeża zmiana (10 nocy przed dziś z nowymi ustawieniami): normy ciśnienia i PS puste, czas i AHI są
  Norma nr = liczNorme(h, dodajDni(dzis, -8), {});
  SPRAWDZ(nr.psmed.n == 10 && !nr.psmed.jest() && !nr.emed.jest() && !nr.wp95.jest() && nr.m01.jest() && nr.ahi.jest());
  DaneK2 k2 = budujK2(h, dodajDni(dzis, -8), nr, {});
  ROWNE(k2.w[8].norma, "uczę się");  // R-KOK-05
  ROWNE(k2.w[6].norma, "uczę się");  // wyciek P95 też (R-NOR-07)
  ROWNE(budujK1(h, dodajDni(dzis, -8), nr, {}).w[3].slowo, "uczę się");
  // dzień przed zmianą: najnowsza noc ma stare ustawienia → norma z nocy ze starymi
  Norma ns = liczNorme(h, dodajDni(dzis, -19), {});
  SPRAWDZ(ns.ustawienia == u4 && ns.psmed.jest() && ns.psmed.med == 4.0);
  // historia bez żadnych ustawień: bez filtra
  for (auto& n : h) n.ustawienia.clear();
  Norma nb = liczNorme(h, dzis, {});
  SPRAWDZ(nb.ustawienia.empty() && nb.psmed.n == 30 && nb.psmed.med == 6.0);  // 18 × 6 i 12 × 4
  // noc przed pierwszymi znanymi ustawieniami nie pasuje do znanych
  h.back().ustawienia = u6;
  Norma np = liczNorme(h, dzis, {});
  SPRAWDZ(np.ustawienia == u6 && np.psmed.n == 0 && np.m01.n == 30);
}

// Ekran nocny „Dobranoc” (dobranoc.h): rotacja, decyzja nocy, godzina maski, teksty. Dane wymyślone (WY12).
static void testDobranocLogika() {
  ROWNE(wieczorNocy("2026-11-12", 23 * 60 + 30), "2026-11-12");
  ROWNE(wieczorNocy("2026-11-13", 4 * 60 + 59), "2026-11-12");
  ROWNE(wieczorNocy("2026-11-13", 5 * 60), "");
  ROWNE(wieczorNocy("2026-11-12", 23 * 60 + 29), "");
  ROWNE(wieczorNocy("", 0), "");
  // R-KON-32, kolumna Weryfikacja: n = dni od 1.01.1970 do daty poranka
  SPRAWDZ(numerPoranka("2026-10-30") == 20757);
  SPRAWDZ(numerPoranka("1970-01-01") == 1 && numerPoranka("zła") == -1);
  auto opis = [](const std::string& wieczor) {
    WyborNocy w = wyborNocy(numerPoranka(wieczor));
    return std::string("A") + std::to_string((int)w.wariant + 1) + " " + w.scena;
  };
  ROWNE(opis("2026-10-30"), "A1 A1-G14");  // noc 30/31.10.2026 (n = 20757)
  ROWNE(opis("2026-10-31"), "A2 A2-G13");  // 31.10/1.11
  ROWNE(opis("2026-11-01"), "A3 A3-W12");  // 1/2.11
  ROWNE(opis("2026-11-02"), "A1 A1-W12");  // 2/3.11 (bez G11, z listy dozwolonych)
  // restart panelu nie zmienia wariantu: wybór zależy tylko od daty, także po północy
  ROWNE(wieczorNocy("2026-10-31", 2 * 60), "2026-10-30");
  // pełny cykl: 9 nocy → każda scena z listy dozwolonych (A1 W12 G14, A2 G10 G13, A3 W12 G12)
  std::string sc;
  for (long n = 0; n < 9; n++) sc += wyborNocy(n).scena + " ";
  ROWNE(sc, "A1-W12 A2-G10 A3-G12 A1-G14 A2-G13 A3-W12 A1-W12 A2-G10 A3-G12 ");
  for (long n = 0; n < 18; n++) SPRAWDZ(scenaNocyJest(wyborNocy(n).scena));  // każda scena jest w obrazy.h
  SPRAWDZ(!scenaNocyJest("W12") && !scenaJest("A1-W12"));  // tabele scen dnia i nocy rozdzielne (R-HUM-11)
  SPRAWDZ(wyborNocy(-1).scena.empty());

  // T-TEST-342: S9 nigdy nie wybiera sceny spoza listy spokojnych,
  // rotacja R-KON-32 dalej działa (każda dozwolona scena wraca), pula wariantu nie jest pusta przez cały rok
  SPRAWDZ(SCENY_NOCY_DOZWOLONE.size() == 5);
  for (auto w : {WariantNocy::A1, WariantNocy::A2, WariantNocy::A3}) SPRAWDZ(!scenyNocyWariantu(w).empty());
  SPRAWDZ(scenaNocyDozwolona("A1-W12") && scenaNocyDozwolona("G14") && scenaNocyDozwolona("A2-G10") && scenaNocyDozwolona("A2-G13"));
  SPRAWDZ(!scenaNocyDozwolona("A1-G11") && !scenaNocyDozwolona("A2-G15") && scenaNocyDozwolona("A3-G12") && !scenaNocyDozwolona(""));
  {
    std::set<std::string> byly;
    long nrOd = numerPoranka("2026-11-05");
    for (long n = nrOd; n < nrOd + 366; n++) {
      WyborNocy w = wyborNocy(n);
      SPRAWDZ(!w.scena.empty() && scenaNocyDozwolona(w.scena) && scenaNocyJest(w.scena));
      SPRAWDZ(w.scena.compare(0, 2, std::string("A") + std::to_string((int)w.wariant + 1)) == 0);
      byly.insert(w.scena);
    }
    SPRAWDZ((byly == std::set<std::string>{"A1-W12", "A1-G14", "A2-G10", "A2-G13", "A3-W12", "A3-G12"}));
    for (long n = nrOd; n < nrOd + 6; n++) SPRAWDZ(wyborNocy(n).scena != wyborNocy(n + 3).scena || n % 3 == 2);  // A1, A2 zmieniają scenę co cykl, A3 ma jedną
  }

  // zdanie przypięte do sceny, każde ≤ 45 znaków (5.7.5)
  for (long n = 0; n < 18; n++) {
    WyborNocy w = wyborNocy(n);
    SPRAWDZ(!w.zdanie.empty() && w.zdanie == zdanieNocy(w.scena));  // także w A2 (linia pod `Dobranoc.`)
    ROWNE(wyborNocy(n, false).zdanie, w.zdanie);  // -DHUMOR_PROPOZYCJE=0: pula „noc” przyjęta (08.11.2026), zdanie jest
  }
  // R-KON-39: humor `wyłączony` → S9 bez sceny i bez zdania, ten sam wariant (rotacja) i ta sama treść tekstowa
  for (long n = 0; n < 9; n++) {
    WyborNocy w = wyborNocy(n), x = wyborNocy(n, true, Humor::WYLACZONY);
    SPRAWDZ(x.scena.empty() && x.zdanie.empty() && x.n == w.n && x.wariant == w.wariant);
    SPRAWDZ(wyborNocy(n, true, Humor::OSZCZEDNY).scena == w.scena);  // oszczędny jak normalny (tabela 5.7.3)
    DaneNocy dw, dx;
    dw.wybor = w;
    dx.wybor = x;
    dw.wieczor = dx.wieczor = "2026-10-30";
    dw.maskaOd = dx.maskaOd = n % 2 ? 23 * 60 + 5 : -1;  // A3 z godziną i bez
    TekstyNocy tw = tekstyNocy(dw), tx = tekstyNocy(dx);
    SPRAWDZ(tx.tytul == tw.tytul && tx.etykieta == tw.etykieta && tx.linie == tw.linie && tx.krok == tw.krok);
    SPRAWDZ(tx.stopkaLewo.empty());
  }
  {
    StanNocy s0;
    DecyzjaNocy d0 = decyzjaNocy("2026-10-30", 23 * 60 + 30, true, false, s0, true, Humor::WYLACZONY);
    SPRAWDZ(d0.nocny && d0.wybor.scena.empty() && d0.wybor.zdanie.empty() && d0.wybor.wariant == WariantNocy::A1);
  }
  ROWNE(zdanieNocy("A1-W12"), "Dobranoc. Skargi przyjmę we śnie.");
  ROWNE(zdanieNocy("A3-W12"), "Dobranoc. Skargi przyjmę we śnie.");
  ROWNE(zdanieNocy("A1-G14"), "Kapcie zdjęte. Reszta świata niech poczeka.");
  ROWNE(zdanieNocy("A1-G11"), "Drzemka się przedłużyła. Nie wnoszę skargi.");
  ROWNE(zdanieNocy("A2-G10"), "Owce policzone. Jedna się nie zgadza.");
  ROWNE(zdanieNocy("A2-G13"), "Księżyc gra na zwłokę. Ja na sen.");
  ROWNE(zdanieNocy("A2-G15"), "Cisza wystąpi dziś bez dyrygenta.");
  ROWNE(zdanieNocy("A3-G12"), "Dziś nurkuję tylko w poduszce.");
  ROWNE(zdanieNocy("A1-X99"), "");
  SPRAWDZ(ZDANIA_NOCY.size() == 7);
  for (auto& z : ZDANIA_NOCY) {
    std::string t = z.zdanie;
    size_t znaki = 0;
    for (unsigned char c : t) znaki += (c & 0xC0) != 0x80;  // znaki UTF-8, nie bajty
    SPRAWDZ(znaki <= 45);
  }

  // decyzja: pierwsze wybudzenie nocy rozstrzyga; S9 tylko przy ekranie dnia, który nie musi zostać (R-KON-35)
  StanNocy s;
  DecyzjaNocy d = decyzjaNocy("2026-10-30", 23 * 60 + 30, true, false, s);
  SPRAWDZ(d.nowa && d.nocny && d.rysujPoZegarze && d.wieczor == "2026-10-30" && d.wybor.scena == "A1-G14");
  s = poDecyzji(d, s);
  SPRAWDZ(s.wieczor == "2026-10-30" && s.s9);
  d = decyzjaNocy("2026-10-31", 2 * 60, true, false, s);  // kolejne wybudzenie: bez rysowania (R-KON-34)
  SPRAWDZ(!d.nowa && d.nocny && !d.rysujPoZegarze && d.wybor.scena == "A1-G14");
  d = decyzjaNocy("2026-10-31", 2 * 60, true, true, s);  // dzienTrwa liczy się tylko przy pierwszej decyzji
  SPRAWDZ(d.nocny);
  d = decyzjaNocy("2026-10-31", 2 * 60, false, false, s);  // K1 otwarty z S9
  SPRAWDZ(!d.nocny && !d.nowa);
  // S5 o 23:30 (R-KON-35, Weryfikacja: E-01 przez cały dzień → S5 zostaje): bez S9 tej nocy
  d = decyzjaNocy("2026-10-31", 23 * 60 + 30, true, true, s);
  SPRAWDZ(d.nowa && !d.nocny && !d.rysujPoZegarze);
  s = poDecyzji(d, s);
  SPRAWDZ(s.wieczor == "2026-10-31" && !s.s9);
  d = decyzjaNocy("2026-11-01", 3 * 60, true, false, s);  // dane wróciły w nocy: S9 i tak nie powstaje
  SPRAWDZ(!d.nocny && !d.rysujPoZegarze);
  // ekran z przycisku o 23:30 (R-KON-35): bez S9; powrót ↻ w nocy → ostatni ekran dnia, nie S9
  d = decyzjaNocy("2026-11-01", 23 * 60 + 30, false, false, s);
  SPRAWDZ(d.nowa && !d.nocny);
  s = poDecyzji(d, s);
  SPRAWDZ(!decyzjaNocy("2026-11-02", 40, true, false, s).nocny);
  // start zasilania w nocy przed decyzją: S9 od razu (wyjątek „start zasilania”, R-KON-31)
  d = decyzjaNocy("2026-11-03", 2 * 60, true, false, s);
  SPRAWDZ(d.nowa && d.nocny && d.wieczor == "2026-11-02" && d.wybor.scena == "A1-W12");
  d = decyzjaNocy("2026-11-03", 12 * 60, true, false, s);
  SPRAWDZ(!d.nocny && !d.nowa && d.wieczor.empty());
  d = decyzjaNocy("", 0, true, false, s);  // bez zegara: bez ekranu nocnego
  SPRAWDZ(!d.nocny && !d.nowa);

  // godzina założenia maski: bez startu sesji nigdy (ADR-0018 pkt 5, R-KON-33)
  SPRAWDZ(maskaOd(OdczytMaski()) == -1);
  OdczytMaski o;
  o.startSesji = 22 * 60 + 47;  // `edf_prefix` w terapii, przykład z Weryfikacji R-KON-33
  SPRAWDZ(maskaOd(o) == 22 * 60 + 47);
  o.startSesji = 24 * 60;
  SPRAWDZ(maskaOd(o) == -1);

  // fakty z pierwszego rysowania wracają po ↻ z K1 (R-KON-37, R-KON-34)
  FaktyNocy z;
  d = decyzjaNocy("2026-10-30", 23 * 60 + 30, true, false, StanNocy());
  DaneNocy dn = daneNocy(d, z, 21.4f, 44.6f, 7 * 60 + 25, -1);
  SPRAWDZ(dn.temp == 21.4f && dn.wilg == 44.6f && dn.zwyklySen == 7 * 60 + 25 && std::string(z.wieczor) == "2026-10-30");
  dn = daneNocy(d, z, 18.0f, 50.0f, 6 * 60, -1);  // 01:10, po spadku temperatury
  SPRAWDZ(dn.temp == 21.4f && dn.wilg == 44.6f && dn.zwyklySen == 7 * 60 + 25);
  d.wieczor = "2026-10-31";  // następna noc: nowe fakty
  dn = daneNocy(d, z, 18.0f, 50.0f, 6 * 60, -1);
  SPRAWDZ(dn.temp == 18.0f && dn.zwyklySen == 6 * 60);

  // teksty
  ROWNE(hhmm(23 * 60 + 41), "23:41");
  ROWNE(hhmm(5), "00:05");
  ROWNE(naglowekNocyA3("2026-11-12"), "Noc z czwartku na piątek");
  ROWNE(naglowekNocyA3("2026-11-14"), "Noc z soboty na niedzielę");
  ROWNE(naglowekNocyA3("2026-11-15"), "Noc z niedzieli na poniedziałek");
  ROWNE(naglowekNocyA3("2026-11-11"), "Noc ze środy na czwartek");
  ROWNE(naglowekNocyA3("2026-10-30"), "Noc z piątku na sobotę");
  dn = DaneNocy();
  dn.wieczor = "2026-10-30";
  dn.maskaOd = 22 * 60 + 47;  // po zamknięciu (R-KON-33)
  dn.temp = 21.4f;
  dn.wilg = 44.6f;
  dn.zwyklySen = 7 * 60 + 25;
  dn.wybor = wyborNocy(numerPoranka("2026-10-30"));  // A1, G14
  TekstyNocy t = tekstyNocy(dn);
  ROWNE(t.naglowekLewo, "30.10 · maska od 22:47");
  ROWNE(t.naglowekPrawo, "");  // T12: te same liczby stoją większe w faktach
  ROWNE(t.etykieta, "DOBRANOC");
  ROWNE(t.tytul, "Śpij dobrze.");
  SPRAWDZ(t.linie.size() == 2);
  ROWNE(t.linie[0], "Maska założona o 22:47.");
  ROWNE(t.linie[1], "Wynik nocy pokażę rano.");
  SPRAWDZ(t.fakty.size() == 3);
  ROWNE(t.fakty[0].first + "|" + t.fakty[0].second, "21°|temperatura");
  ROWNE(t.fakty[1].first + "|" + t.fakty[1].second, "45%|wilgotność");
  ROWNE(t.fakty[2].first + "|" + t.fakty[2].second, "7 h 25|zwykle z maską");  // T13
  ROWNE(t.krok, "Ekran zostaje do rana, żeby nie błyskać.");  // T14, bez `→`: informacja, nie czynność
  ROWNE(t.stopkaLewo, "");  // bez zdania budzika od 10.11.2026 (ADR-0009, ADR-0018)
  ROWNE(t.stopkaPrawo, "▶ kokpit");
  dn.maskaOd = -1;  // bez startu sesji (R-KON-33): nagłówek `30.10` bez `maska od`, powód tylko wynik rano
  dn.zwyklySen = -1;  // bez normy: bez faktu „Twój zwykły czas z maską”
  t = tekstyNocy(dn);
  ROWNE(t.naglowekLewo, "30.10");
  SPRAWDZ(t.linie.size() == 1 && t.fakty.size() == 2);
  ROWNE(t.linie[0], "Wynik nocy pokażę rano.");
  dn.wybor = wyborNocy(numerPoranka("2026-10-31"));  // A2, G13
  dn.maskaOd = 22 * 60 + 47;
  ROWNE(tekstyNocy(dn).liniaA2, "maska od 22:47 · przy łóżku 21° · 45% · wynik rano");
  ROWNE(tekstyNocy(dn).tytul, "Dobranoc.");
  SPRAWDZ(tekstyNocy(dn).stopkaLewo.empty());  // A2 bez zdania budzika pod tytułem (od 10.11.2026)
  dn.maskaOd = -1;
  ROWNE(tekstyNocy(dn).liniaA2, "przy łóżku 21° · 45% · wynik rano");
  dn.temp = dn.wilg = NAN;  // bez czujnika panelu
  ROWNE(tekstyNocy(dn).liniaA2, "wynik rano");
  dn.wybor = wyborNocy(numerPoranka("2026-10-31"), false);  // pula „noc” zostaje w danych, na ekranie zdania nie ma
  SPRAWDZ(!dn.wybor.zdanie.empty() && tekstyNocy(dn).stopkaLewo.empty());
  dn.temp = 21.4f;
  dn.wilg = 44.6f;
  dn.wybor = wyborNocy(numerPoranka("2026-11-01"));  // A3, W12
  dn.wieczor = "2026-11-01";
  dn.maskaOd = 22 * 60 + 47;
  t = tekstyNocy(dn);
  ROWNE(t.naglowekLewo, "Noc z niedzieli na poniedziałek");
  ROWNE(t.etykieta, "MASKA ZAŁOŻONA O");
  ROWNE(t.tytul, "22:47");
  SPRAWDZ(t.tytulGodzina && t.linie.size() == 2);
  ROWNE(t.linie[0], "Dobranoc. Wynik nocy rano.");
  ROWNE(t.linie[1], "Ekran zostaje do rana, żeby nie błyskać.");  // T15
  ROWNE(t.stopkaLewo, "");
  dn.maskaOd = -1;  // R-KON-33: bez startu sesji, noc A3
  t = tekstyNocy(dn);
  ROWNE(t.etykieta, "NA DOBRANOC");
  ROWNE(t.tytul, "Dobranoc.");
  SPRAWDZ(!t.tytulGodzina);
  ROWNE(t.linie[0], "Wynik nocy rano.");
  dn.wybor = wyborNocy(numerPoranka("2026-11-01"), false);
  ROWNE(tekstyNocy(dn).stopkaLewo, "");  // bez zdania budzika także bez propozycji
  // ADR-0002: na ekranie nocnym ani słowa o ustawieniach aparatu i ocenie nocy
  for (long n = 0; n < 18; n++)
    for (int g : {-1, 23 * 60 + 41}) {
      dn.wybor = wyborNocy(n);
      dn.maskaOd = g;
      TekstyNocy x = tekstyNocy(dn);
      std::string all = x.naglowekLewo + x.tytul + x.krok + x.liniaA2 + x.stopkaLewo;
      for (auto& l : x.linie) all += l;
      for (auto& f : x.fakty) all += f.second;
      for (const char* zakaz : {"ciśnieni", "EPR", "cmH", "ustawie", "AHI", "lekarz", "05:00"})
        SPRAWDZ(all.find(zakaz) == std::string::npos);
    }

  // przyciski z ekranu nocnego: ▶ → K1, ↻ i ◀ nic, przytrzymania nic (R-KON-37)
  KontekstNaw k;
  k.nocny = true;
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, k).akcja == Akcja::NIC);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::PRAWY, false, k).ekran == Ekran::K1);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, false, k).akcja == Akcja::NIC);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, true, k).akcja == Akcja::NIC);
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::LEWY, true, k).ekran == Ekran::DZIEN);
  SPRAWDZ(nacisniecie(Ekran::K1, Przycisk::SRODEK, false, k).ekran == Ekran::DZIEN);  // powrót: znów „Dobranoc”
  // S5 w nocy (S9 nie powstał): ↻ ponawia pobranie jak na S5 (R-PRZ-06)
  KontekstNaw k5;
  k5.s5 = true;
  SPRAWDZ(nacisniecie(Ekran::DZIEN, Przycisk::SRODEK, false, k5).akcja == Akcja::PONOW_POBRANIE);
}

// Symulacja 5 dób minuta po minucie, jak przebieg wybudzenia na panelu: wybudzenie zegarem co 30 min, wybudzenie po 5 min
// do powrotu z ekranu otwartego przyciskiem (spij()), naciśnięcia. Dane wymyślone (WY12).
// Noc 30/31.10: S9 o 23:30 (A1, G14), 00:40 ▶ → K1, 01:10 ↻ → S9 z tymi samymi faktami, 01:20 ↻ 2 s → bez rysowania.
// Noc 31.10/1.11: ▶ o 23:27 → K1, wybudzenia o 23:30 i 23:32 (5 min) → K1 zostaje, bez S9; 00:40 ↻ → ekran dnia.
// Noc 1/2.11: brak danych (S5) o 23:30 → bez S9, S5 do 05:00; 01:00 ↻ → ponowne pobranie.
// Noc 2/3.11: S9 (A1, W12). Noc 3/4.11: S9 (A2, G10).
static void testDobranocDoby() {
  StanNocy s;
  FaktyNocy fakty;
  Ekran ekran = Ekran::DZIEN;
  long ostatnie = -100000;  // minuta symulacji ostatniego naciśnięcia
  long powrotO = -1;        // minuta symulacji wybudzenia do powrotu (spij(): naciśnięcie + 5 min)
  std::vector<std::string> log;
  int pobranPrzyciskiem = 0;
  auto brak = [](const std::string& dzis, int m) {  // S5: brak danych od 1.11 18:00 do 2.11 01:00
    return (dzis == "2026-11-01" && m >= 18 * 60) || (dzis == "2026-11-02" && m < 60);
  };
  auto temperatura = [](int m) { return m >= 23 * 60 + 30 ? 21.4f : m < 5 * 60 ? 18.2f : 22.0f; };
  auto narysuj = [&](const std::string& dzis, int m, const DecyzjaNocy& dn, const char* jak) {
    std::string co;
    if (ekran != Ekran::DZIEN) co = ekran == Ekran::K1 ? "K1" : "K2";
    else if (dn.nocny) {
      DaneNocy d = daneNocy(dn, fakty, temperatura(m), 45.0f, 7 * 60 + 25, -1);
      co = "S9 " + dn.wybor.scena + " " + calkowita(d.temp) + "°";
    } else co = brak(dzis, m) ? "S5" : "dzień";
    log.push_back(dzis.substr(5) + " " + hhmm(m) + " " + jak + " " + co);
  };
  auto decyzja = [&](const std::string& dzis, int m, bool zapisz) {
    std::string w = wieczorNocy(dzis, m);
    bool dzien = ekran == Ekran::DZIEN;
    bool trwa = dzien && !w.empty() && s.wieczor != w && brak(dzis, m);
    DecyzjaNocy d = decyzjaNocy(dzis, m, dzien, trwa, s);
    if (zapisz) s = poDecyzji(d, s);
    return d;
  };
  for (long t = 12 * 60; t < 5 * 24 * 60 + 6 * 60; t++) {  // od 30.10 12:00 do 4.11 06:00
    std::string dzis = dodajDni("2026-10-30", (int)(t / (24 * 60)));
    int m = (int)(t % (24 * 60));
    // naciśnięcia (wybudzenie przyciskiem)
    Przycisk p = Przycisk::BRAK;
    bool dlugo = false;
    if (dzis == "2026-10-31" && m == 40) p = Przycisk::PRAWY;
    if (dzis == "2026-10-31" && m == 70) p = Przycisk::SRODEK;
    if (dzis == "2026-10-31" && m == 80) { p = Przycisk::SRODEK; dlugo = true; }
    if (dzis == "2026-10-31" && m == 23 * 60 + 27) p = Przycisk::PRAWY;
    if (dzis == "2026-11-01" && m == 40) p = Przycisk::SRODEK;
    if (dzis == "2026-11-02" && m == 0) p = Przycisk::SRODEK;
    if (p != Przycisk::BRAK) {
      DecyzjaNocy pod = decyzja(dzis, m, false);
      KontekstNaw k;
      k.nocny = !pod.nowa && pod.nocny;
      k.s5 = ekran == Ekran::DZIEN && !k.nocny && brak(dzis, m);
      Przejscie pr = nacisniecie(ekran, p, dlugo, k);
      bool zmiana = pr.akcja != Akcja::NIC || pr.ekran != ekran;
      if (pr.akcja == Akcja::PONOW_POBRANIE) pobranPrzyciskiem++;
      ekran = pr.ekran;
      ostatnie = t;
      DecyzjaNocy dn = decyzja(dzis, m, true);
      if (zmiana || !slotNocny(m) || dn.rysujPoZegarze) narysuj(dzis, m, dn, "p");
      else log.push_back(dzis.substr(5) + " " + hhmm(m) + " p bez rysowania");
      long r = sekundDoPowrotu(ekran, 0, m);
      powrotO = r >= 0 ? t + r / 60 : -1;
      continue;
    }
    // wybudzenie zegarem: co 30 min albo po 5 min do powrotu (spij() bierze wcześniejsze)
    if (m % 30 != 0 && t != powrotO) continue;
    if (t == powrotO) powrotO = -1;
    if (ekran != Ekran::DZIEN && sekundDoPowrotu(ekran, (t - ostatnie) * 60, m) == 0) ekran = Ekran::DZIEN;
    DecyzjaNocy dn = decyzja(dzis, m, true);
    if (przerysowacPoZegarze(m) || dn.rysujPoZegarze) narysuj(dzis, m, dn, "z");
    else if (m % 30 != 0) log.push_back(dzis.substr(5) + " " + hhmm(m) + " z bez rysowania");
  }
  // tylko noc (23:01–05:00): rysunki i wybudzenia poza pełną i wpół
  std::string wynik;
  for (auto& r : log) {
    int m = std::stoi(r.substr(6, 2)) * 60 + std::stoi(r.substr(9, 2));
    if (m > 23 * 60 || m <= 5 * 60) wynik += r + "\n";
  }
  ROWNE(wynik,
        "10-30 23:30 z S9 A1-G14 21°\n"     // noc 1: raz, o 23:30 (R-KON-31, R-KON-32)
        "10-31 00:40 p K1\n"                // ▶ na S9 → K1, zostaje (R-PRZ-18)
        "10-31 01:10 p S9 A1-G14 21°\n"     // ↻ w K1 → S9 z tą samą sceną i faktami z 23:30 (R-KON-37)
        "10-31 01:20 p bez rysowania\n"     // ↻ 2 s na S9: nic, bez błysku
        "10-31 05:00 z dzień\n"             // 05:00: ekran dnia (R-EKR-37)
        "10-31 23:27 p K1\n"                // noc 2: ▶ o 23:27 → K1
        "10-31 23:32 z bez rysowania\n"     // wybudzenie 5 min po ▶: w slocie nocnym K1 zostaje
        "11-01 00:40 p dzień\n"             // ↻ w K1: S9 tej nocy nie powstał → ostatni ekran dnia (R-KON-35)
        "11-01 05:00 z dzień\n"
        "11-02 00:00 p S5\n"                // noc 3: S5 o 23:30 bez rysowania; ↻ na S5 → ponowne pobranie
        "11-02 05:00 z dzień\n"
        "11-02 23:30 z S9 A1-W12 21°\n"     // noc 4 (2/3.11): A1, W12
        "11-03 05:00 z dzień\n"
        "11-03 23:30 z S9 A2-G10 21°\n"     // noc 5 (3/4.11): A2, G10
        "11-04 05:00 z dzień\n");
  SPRAWDZ(pobranPrzyciskiem == 1);
  SPRAWDZ(s.wieczor == "2026-11-03" && s.s9);
}

// ---------- AirBridge od v2.2.0: Digest, lista nocy, raport nocy → rekord, plan, start sesji ----------
// MD5 tylko do testu (na panelu mbedtls); RFC 1321, wektory z hashlib.
static std::string md5Test(const std::string& w) {
  static const uint32_t K[64] = {
      0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8,
      0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
      0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87,
      0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
      0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039,
      0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
      0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb,
      0xeb86d391};
  static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9,  14, 20, 5, 9,
                            14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                            4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
  std::string m = w;
  uint64_t bity = (uint64_t)w.size() * 8;
  m += (char)0x80;
  while (m.size() % 64 != 56) m += (char)0;
  for (int i = 0; i < 8; i++) m += (char)(bity >> (8 * i));
  uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
  for (size_t o = 0; o < m.size(); o += 64) {
    uint32_t x[16];
    for (int i = 0; i < 16; i++) {
      x[i] = 0;
      for (int b = 0; b < 4; b++) x[i] |= (uint32_t)(unsigned char)m[o + 4 * i + b] << (8 * b);
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    for (int i = 0; i < 64; i++) {
      uint32_t f;
      int g;
      if (i < 16) { f = (b & c) | (~b & d); g = i; }
      else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
      else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
      else { f = c ^ (b | ~d); g = (7 * i) % 16; }
      uint32_t t = d;
      d = c;
      c = b;
      uint32_t s = a + f + K[i] + x[g];
      b = b + ((s << R[i]) | (s >> (32 - R[i])));
      a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
  }
  char out[33];
  for (int i = 0; i < 16; i++) snprintf(out + 2 * i, 3, "%02x", (h[i / 4] >> (8 * (i % 4))) & 0xff);
  return out;
}

static std::string pole(const std::string& h, const std::string& klucz) {  // wartość klucz="…" z nagłówka
  size_t i = h.find(klucz + "=\"");
  if (i == std::string::npos) return "";
  i += klucz.size() + 2;
  return h.substr(i, h.find('"', i) - i);
}

// Raport nocy (syntetyczny, układ jak z modułu v2.2.1): 7 h 2 min, AHI 1,4, ASVAuto 6–10, PS 3–15.
static std::string raportNocy(long day, const std::string& partial = "false", const std::string& zmiany = "") {
  std::string r =
      "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + std::to_string(day) +
      ",\"first_day\":" + std::to_string(day) + ",\"last_day\":" + std::to_string(day) +
      ",\"sessions\":[{\"id\":1,\"day\":" + std::to_string(day) +
      ",\"prefix\":\"X\",\"start_ms\":1793403600000,\"end_ms\":1793428920000}],"
      "\"present\":true,\"partial\":" + partial +
      ",\"used_seconds\":25320,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
      "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,"
      "\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,\"rera\":2},"
      "\"channels\":["
      "{\"id\":\"Press\",\"label\":\"Ciśnienie\",\"unit\":\"cmH2O\",\"count\":12660,\"coverage_seconds\":25320,"
      "\"min\":6.2,\"mean\":10.1,\"median\":9.8,\"p95\":13.6,\"max\":16.0,\"source\":\"edf\"},"
      "{\"id\":\"EprPress\",\"label\":\"EPAP\",\"unit\":\"cmH2O\",\"count\":12660,\"coverage_seconds\":25320,"
      "\"min\":6.0,\"mean\":6.6,\"median\":6.4,\"p95\":8.4,\"max\":9.2,\"source\":\"edf\"},"
      "{\"id\":\"Leak\",\"label\":\"Wyciek\",\"unit\":\"L/min\",\"count\":12660,\"coverage_seconds\":25320,"
      "\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,\"source\":\"edf\"},"
      "{\"id\":\"RespRate\",\"label\":\"Oddechy\",\"unit\":\"bpm\",\"count\":12660,\"coverage_seconds\":25320,"
      "\"min\":8,\"mean\":15,\"median\":15.2,\"p95\":19,\"max\":36.6,\"source\":\"edf\"}],"
      "\"settings\":{\"day\":" + std::to_string(day) + ",\"available\":true,\"fields\":["
      "{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\",\"option\":8},"
      "{\"tag\":\"EAI\",\"label\":\"Min EPAP\",\"value\":\"6.0\",\"unit\":\"cmH2O\",\"option\":null},"
      "{\"tag\":\"EAX\",\"label\":\"Max EPAP\",\"value\":\"10.0\",\"unit\":\"cmH2O\",\"option\":null},"
      "{\"tag\":\"ANS\",\"label\":\"Min PS\",\"value\":3,\"unit\":\"cmH2O\",\"option\":null},"
      "{\"tag\":\"AXS\",\"label\":\"Max PS\",\"value\":\"15.0\",\"unit\":\"cmH2O\",\"option\":null},"
      "{\"tag\":\"EPR\",\"label\":\"EPR\",\"value\":\"0\",\"unit\":\"\",\"option\":null},"
      "{\"tag\":\"MSK\",\"label\":\"Maska\",\"value\":\"Pillows\",\"unit\":\"\",\"option\":2}]},"
      "\"history\":{\"days\":[1,2,3],\"x\":{\"y\":[{\"z\":null}]}}" + zmiany + "}";
  return r;
}

static void testAirBridge() {
  // --- MD5 (test) i Digest: wektor RFC 2617 (3.5) ---
  ROWNE(md5Test(""), "d41d8cd98f00b204e9800998ecf8427e");
  ROWNE(md5Test("The quick brown fox jumps over the lazy dog"), "9e107d9d372bb6826bd81d3542a419d6");
  WyzwanieDigest w = parsujWyzwanie(
      "Digest realm=\"testrealm@host.com\", qop=\"auth,auth-int\", nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", "
      "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"");
  SPRAWDZ(w.ok);
  ROWNE(w.realm, "testrealm@host.com");
  ROWNE(w.opaque, "5ccc069c403ebaf9f0171e9517f40e41");
  std::string h = naglowekDigest(w, "Mufasa", "Circle Of Life", "GET", "/dir/index.html", 1, "0a4f113b", md5Test);
  ROWNE(pole(h, "response"), "6629fae49393a05397450978507c4ef1");
  SPRAWDZ(h.find("qop=auth, nc=00000001, cnonce=\"0a4f113b\"") != std::string::npos);
  ROWNE(pole(h, "uri"), "/dir/index.html");
  ROWNE(pole(h, "opaque"), "5ccc069c403ebaf9f0171e9517f40e41");
  SPRAWDZ(h.rfind("Digest username=\"Mufasa\"", 0) == 0);
  // nagłówek jak z modułu (realm „Login Required”, kolejność pól inna, bez cudzysłowu przy qop), uri z jobem;
  // odpowiedź policzona niezależnie w Pythonie (hashlib)
  w = parsujWyzwanie("Digest realm=\"Login Required\",qop=auth,nonce=\"5d3b6f0a9c1e2f47\",opaque=\"o1\"");
  SPRAWDZ(w.ok && w.qop == "auth" && w.nonce == "5d3b6f0a9c1e2f47");
  ROWNE(sciezkaJoba("edf:8"), "/api/report?source=edf&job=edf%3A8");
  h = naglowekDigest(w, "admin", "tajne-haslo", "GET", sciezkaJoba("edf:8"), 12, "abcdef0123456789", md5Test);
  ROWNE(pole(h, "response"), "e8bc067f5a2f40e8e84cb1b24c2e152f");
  SPRAWDZ(h.find("nc=0000000c") != std::string::npos);
  // bez qop (RFC 2069): odpowiedź bez nc i cnonce
  w = parsujWyzwanie("Digest realm=\"Login Required\", nonce=\"n1\"");
  h = naglowekDigest(w, "admin", "tajne-haslo", "GET", "/api/status", 1, "c", md5Test);
  ROWNE(pole(h, "response"), "db12d75fed34cbd6762c59f1c2cbef76");
  SPRAWDZ(h.find("nc=") == std::string::npos && h.find("opaque") == std::string::npos);
  SPRAWDZ(!parsujWyzwanie("Basic realm=\"x\"").ok && !parsujWyzwanie("Digest realm=\"x\"").ok);

  // --- JSON: escape, unicode, liczby, błędy ---
  Json j;
  SPRAWDZ(parsujJson("{\"a\":\"x\\\"y\\u0105\\n\",\"b\":[1,-2.5e1,true,null],\"c\":{}}", j));
  ROWNE(j["a"].s, "x\"yą\n");
  double v;
  SPRAWDZ(j["b"][1].liczba(v) && v == -25 && j["b"][2].b && j["b"][3].typ == Json::NUL && !j["zzz"].jest());
  SPRAWDZ(!parsujJson("{\"a\":1,}", j) && !parsujJson("{\"a\":1} x", j) && !parsujJson("", j) && !parsujJson("[1", j));
  SPRAWDZ(parsujJson("{\"start_ms\":1793403600000}", j) && j["start_ms"].liczba(v) && (long long)v == 1793403600000LL);
  // gramatyka liczb JSON i surogaty UTF-16
  for (const char* z : {"1E5", "1e+5", "-1.5e-3", "0.5", "-0", "0", "[0,10,-0.0e0]", "\"\\ud83d\\ude00\""})
    SPRAWDZ(parsujJson(z, j));
  for (const char* z : {"01", "-01", "1.", "12.", "-0.", ".5", "-", "1e", "1e+", "+1", "1.5.2", "--1", "[01]", "[1.]",
                        "{\"a\":1.}", "\"\\ud800\"", "\"\\udc00\"", "\"\\ud800\\u0041\"", "\"\\ud800A\"",
                        "\"\\ude00\\ud83d\"", "NaN", "Infinity", "nul", "truex", "\"\t\""})
    SPRAWDZ(!parsujJson(z, j));
  SPRAWDZ(parsujJson("\"\\ud83d\\ude00\"", j) && j.s == "\xF0\x9F\x98\x80");  // 😀 jako 4 bajty UTF-8

  // --- status i wersja ---
  StatusAB st;
  SPRAWDZ(parsujStatus("{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"srn\":\"1\","
                       "\"system\":\"IDLE\",\"rop\":0,\"mode\":\"ASVAuto\",\"esp_time\":\"2026-10-30 14:49\","
                       "\"report_state\":\"ready\",\"report_available\":true}",
                       st));
  ROWNE(st.wersja, "v2.2.1-25-gebbd145");
  ROWNE(st.tryb, "ASVAuto");
  SPRAWDZ(st.rop == 0 && st.model == "AirCurve_10_CS_PaceWave" && st.raport == "ready");
  SPRAWDZ(parsujStatus("{}", st) && st.rop == -1 && !parsujStatus("nie json", st));
  SPRAWDZ(!raportyZablokowane(st));
  // terapia trwa (jak na urządzeniu w trakcie terapii): raporty zablokowane, start sesji z `edf_prefix`
  SPRAWDZ(parsujStatus("{\"version\":\"v2.2.1-25-gebbd145\",\"system\":\"THERAPY\",\"rop\":1,\"edf\":\"recording\","
                       "\"edf_prefix\":\"20261030_153114\",\"report_state\":\"blocked\",\"report_error\":\"not_idle\"}",
                       st));
  SPRAWDZ(st.rop == 1 && st.system == "THERAPY" && st.prefiks == "20261030_153114" && st.bladRaportu == "not_idle");
  SPRAWDZ(raportyZablokowane(st));
  SPRAWDZ(startZPrefiksu(st.rop, st.prefiks, "2026-10-30", 15 * 60 + 52) == 15 * 60 + 31);
  st.rop = 0;
  SPRAWDZ(raportyZablokowane(st));  // `report_state` = "blocked" bez terapii: też bez raportów
  st.raport = "ready";
  SPRAWDZ(!raportyZablokowane(st));
  st.rop = 1;
  SPRAWDZ(raportyZablokowane(st));  // rop = 1 wystarczy
  // po terapii (urządzenie tuż po sesji): `edf_prefix` zostaje, ale bez `rop` = 1 godziny maski nie ma; moduł
  // przetwarza pliki, a pusta lista `days` z tej chwili nie znaczy „brak nocy”
  SPRAWDZ(parsujStatus("{\"system\":\"IDLE\",\"rop\":0,\"edf\":\"post-processing\",\"edf_prefix\":\"20261030_153114\","
                       "\"report_state\":\"waiting_storage\"}", st));
  SPRAWDZ(st.rop == 0 && st.edf == "post-processing" && !st.prefiks.empty() && !raportyZablokowane(st));
  SPRAWDZ(startZPrefiksu(st.rop, st.prefiks, "2026-10-30", 17 * 60 + 6) == -1);
  SPRAWDZ(startZPrefiksu(1, st.prefiks, "2026-10-30", 17 * 60 + 6) == 15 * 60 + 31);
  SPRAWDZ(modulPrzetwarza(st));
  st.edf = "";
  st.raport = "storage_busy";
  SPRAWDZ(modulPrzetwarza(st));
  st.raport = "ready";
  SPRAWDZ(!modulPrzetwarza(st));
  {
    std::vector<DzienListy> pusta;
    SPRAWDZ(parsujListeNocy("{\"source\":\"edf\",\"days\":[],\"partial\":false}", pusta) && pusta.empty());
  }
  st.rop = 1;
  st.raport = "blocked";
  // poza terapią albo zły układ prefiksu: godziny nie ma (R-KON-33, bez zgadywania)
  {
    auto sp = [](int rop, const char* p) { return startZPrefiksu(rop, p, "2026-10-31", 60); };  // 31.10, 01:00
    SPRAWDZ(sp(0, "20261030_153114") == -1 && sp(1, "") == -1);
    SPRAWDZ(sp(1, "20261030_233000") == 23 * 60 + 30 && sp(1, "20261031_000500") == 5);
    SPRAWDZ(sp(1, "20261030-153114") == -1 && sp(1, "20261030_2531") == -1);
    SPRAWDZ(sp(1, "20261030_246000") == -1 && sp(1, "20261330_153114") == -1);
    SPRAWDZ(sp(1, "2026103a_153114") == -1 && sp(1, "20261030_15311") == -1);
    // data z prefiksu: tylko dziś albo wczoraj według zegara panelu, start dziś nie z przyszłości
    SPRAWDZ(sp(1, "20261031_010000") == 60 && sp(1, "20261031_010100") == -1);  // 01:01 > teraz (01:00)
    SPRAWDZ(sp(1, "20261029_233000") == -1 && sp(1, "20261101_000500") == -1);  // przedwczoraj, jutro
    SPRAWDZ(sp(1, "20260231_010000") == -1);                                      // 31.02: data nieistniejąca
    SPRAWDZ(startZPrefiksu(1, "20261031_000500", "", 60) == -1);                 // panel bez zegara
  }
  WersjaAB wr = parsujWersje("v2.2.1-25-gebbd145");
  SPRAWDZ(wr.a == 2 && wr.b == 2 && wr.c == 1 && wersjaObslugiwana(wr));
  SPRAWDZ(wersjaObslugiwana(parsujWersje("v2.2.0")) && wersjaObslugiwana(parsujWersje("3.0.0")));
  SPRAWDZ(!wersjaObslugiwana(parsujWersje("v1.6.2")) && !wersjaObslugiwana(parsujWersje("v2.1.9")));
  SPRAWDZ(!parsujWersje("dev").ok() && !parsujWersje("v2.2").ok());

  // --- odpowiedzi raportu: 202 z jobem, odpytywanie, 200, błędy ---
  OdpowiedzAB o = klasyfikujOdpowiedz(202, "{\"job\":\"edf:8\"}");
  SPRAWDZ(o.r == Odpowiedz::W_BUDOWIE && o.job == "edf:8");
  o = klasyfikujOdpowiedz(202, "{\"job\":\"edf:9\",\"pending\":true}");
  SPRAWDZ(o.r == Odpowiedz::W_BUDOWIE && o.job == "edf:9");
  SPRAWDZ(klasyfikujOdpowiedz(409, "").r == Odpowiedz::W_BUDOWIE);
  SPRAWDZ(klasyfikujOdpowiedz(200, "{\"job\":\"edf:9\",\"pending\":true}").r == Odpowiedz::W_BUDOWIE);
  SPRAWDZ(klasyfikujOdpowiedz(200, raportNocy(20746)).r == Odpowiedz::GOTOWA);
  SPRAWDZ(klasyfikujOdpowiedz(400, "{\"error\":\"report_invalid_selection\"}").r == Odpowiedz::BLAD);
  SPRAWDZ(klasyfikujOdpowiedz(401, "").r == Odpowiedz::BLAD && klasyfikujOdpowiedz(-1, "").r == Odpowiedz::BLAD);
  // 200 z ciałem urwanym albo nie-JSON: błąd tej próby (ponowienie), nie gotowy raport → nie E-03
  SPRAWDZ(klasyfikujOdpowiedz(200, "").r == Odpowiedz::BLAD && klasyfikujOdpowiedz(200, "<html>").r == Odpowiedz::BLAD);
  {
    std::string pelny = raportNocy(20746);
    int gotowe = 0;
    for (size_t cut = 1; cut < pelny.size(); cut++)
      if (klasyfikujOdpowiedz(200, pelny.substr(0, cut)).r == Odpowiedz::GOTOWA) gotowe++;
    SPRAWDZ(gotowe == 0);  // żadne urwane ciało raportu nie jest „gotowe”
    std::string krotki = "{\"day\":20755,\"present\":true,\"partial\":false,\"used_seconds\":25320,\"ahi\":1.4}";
    int zly = 0;
    for (size_t cut = 1; cut < krotki.size(); cut++) {
      NocAB x = nocZRaportu(krotki.substr(0, cut), 20755, "ASVAuto");
      if (x.stan != StanRaportu::NIECZYTELNY) zly++;
      if (poRaporcie(x.stan, "2026-10-30", "2026-10-30", 9 * 60, 1) == PoRaporcie::E03) zly++;   // ostatnia noc
      if (poRaporcie(x.stan, "2026-10-20", "2026-10-30", 9 * 60, 1) == PoRaporcie::POMIN) zly++;  // starsza doba
    }
    SPRAWDZ(zly == 0);
  }
  SPRAWDZ(klasyfikujOdpowiedz(202, "").r == Odpowiedz::W_BUDOWIE && klasyfikujOdpowiedz(202, "").job.empty());
  // kompletność ciała: Content-Length i wynik zapisu strumienia
  SPRAWDZ(cialoKompletne(100, 100, 100) && cialoKompletne(-1, 37, 37) && !cialoKompletne(100, 60, 60));
  SPRAWDZ(!cialoKompletne(100, -7, 100) && !cialoKompletne(-1, -4, 10));
  // R-AW-01: 3 próby co 10 s przy braku odpowiedzi, w budżecie wybudzenia; 401, 202, 409 i 200 bez ponowień
  SPRAWDZ(ponowicZadanie(-1, 1, 120000) && ponowicZadanie(-11, 2, 30000) && ponowicZadanie(500, 1, 60000));
  SPRAWDZ(ponowicZadanie(AB_KOD_URWANE, 1, 60000) && ponowicZadanie(404, 2, 25000));
  SPRAWDZ(!ponowicZadanie(AB_KOD_ZA_DUZO, 1, 120000) && brakOdpowiedzi(AB_KOD_ZA_DUZO));
  SPRAWDZ(!ponowicZadanie(-1, 3, 120000));                    // trzecia próba była ostatnia
  SPRAWDZ(!ponowicZadanie(-1, 1, 24999));                     // odstęp 10 s + odpowiedź 15 s nie mieści się w budżecie
  SPRAWDZ(!ponowicZadanie(401, 1, 120000) && !ponowicZadanie(200, 1, 120000) && !ponowicZadanie(202, 1, 120000));
  SPRAWDZ(!ponowicZadanie(409, 1, 120000) && brakOdpowiedzi(500) && brakOdpowiedzi(0) && !brakOdpowiedzi(401));
  SPRAWDZ(AB_ODPYTYWANIE_MS == 5000 && AB_LIMIT_JOBA_MS == 120000);  // R-DANE-11
  ROWNE(sciezkaNocy(20746), "/api/report?source=edf&day=20746");
  ROWNE(sciezkaListy(), "/api/report?source=edf&action=days");

  // --- lista nocy ---
  long d31 = nrDnia("2026-10-30");  // noc 30/31.10: dzień AirBridge 30.10, data poranka 31.10 (4.1)
  ROWNE(dataPoranka(d31), "2026-10-31");
  ROWNE(dataPoranka(20746), "2026-10-21");  // `publication.day` 20261020 = dzień 20746
  std::string lista = "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"dataset\":\"d\",\"partial\":false,"
                      "\"days\":[";
  for (long d = d31 - 20; d <= d31 + 2; d++)  // doby obce sprzed instalacji i trzy noce
    lista += std::string(d > d31 - 20 ? "," : "") + "{\"day\":" + std::to_string(d) +
             ",\"session_count\":1,\"publication\":{\"day\":\"x\",\"digest\":\"ab\",\"bytes\":6222},"
             "\"identity\":\"i\",\"stale\":false}";
  lista += "],\"periods\":[{\"from\":1,\"to\":2}]}";
  std::vector<DzienListy> dni;
  SPRAWDZ(parsujListeNocy(lista, dni) && dni.size() == 23 && dni[0].day == d31 - 20 && dni[0].sesje == 1);
  SPRAWDZ(!parsujListeNocy("{\"days\":[{\"session_count\":1}]}", dni) && dni.empty());  // brak `day`: E-03
  SPRAWDZ(!parsujListeNocy("{\"periods\":[]}", dni) && parsujListeNocy("{\"days\":[]}", dni) && dni.empty());
  parsujListeNocy(lista, dni);

  // --- raport nocy → rekord ---
  NocAB n = nocZRaportu(raportNocy(d31), -1, "");
  SPRAWDZ(n.stan == StanRaportu::KOMPLETNA && n.sesje == 1);
  // M-03 z `counts` (na godzinę: 1, 5, 4 zdarzenia w 25 320 s) i M-09 z `csr_seconds` (0 min)
  ROWNE(zapiszNoc(n.noc), "2026-10-31;1;422;1.40;9.80;13.60;2.40;9.60;6.40;8.40;;;ASVAuto/6/10/3/15;A;0;0;0.14;0.71;0.57;0.00");
  ROWNE(n.tryb, "ASVAuto");
  // ten sam format ustawień co import karty (historia.py), więc norma nie zeruje się po przejściu na AirBridge
  Noc kk;
  SPRAWDZ(parsujNoc("2026-10-30;1;400;1.0;9.6;13.0;2.0;8.0;6.3;8.2;3.4;7.9;ASVAuto/6/10/3/15", 'K', kk));
  ROWNE(kk.ustawienia, n.noc.ustawienia);
  // wyciek w L/s → L/min; kanały ciśnienia ze STR poza historią (R-NOR-08); kanał spoza tabeli pomijany (R-NF-17)
  std::string r = raportNocy(d31);
  r.replace(r.find("\"unit\":\"L/min\",\"count\""), 22, "\"unit\":\"L/s\",\"count\"");
  r.replace(r.find("\"median\":2.4,\"p95\":9.6"), 22, "\"median\":0.1,\"p95\":0.4");
  r.replace(r.find("\"max\":9.2,\"source\":\"edf\""), 24, "\"max\":9.2,\"source\":\"str\"");
  n = nocZRaportu(r, -1, "");
  SPRAWDZ(std::fabs(n.noc.wmed - 6.0f) < 1e-4 && std::fabs(n.noc.wp95 - 24.0f) < 1e-4);
  // EPAP ze STR: zapisane z flagą `epap_str` (dopisek `(urządzenie)`, R-KOK-15), poza normą i regułami (R-NOR-08)
  SPRAWDZ(n.noc.epapStr && std::fabs(n.noc.emed - 6.4f) < 1e-4 && std::isnan(epapMed(n.noc)) &&
          std::isnan(epapP95(n.noc)) && !std::isnan(n.noc.cmed));
  SPRAWDZ(!rekordPelny(Noc{}) && !nocZRaportu(raportNocy(d31), -1, "").noc.epapStr);
  // noc ważna według 4.2: każdy warunek osobno (R-DANE-13)
  SPRAWDZ(nocZRaportu(raportNocy(d31), -1, "").noc.wazna);
  r = raportNocy(d31);
  r.replace(r.find("\"used_seconds\":25320"), 20, "\"used_seconds\":14399.6");  // 3 h 59 min 59,6 s
  n = nocZRaportu(r, -1, "");
  // próg 4 h z tej samej zaokrąglonej wartości co M-01, seria i kamienie: 240 min = ważna i w serii
  SPRAWDZ(n.noc.wazna && n.noc.m01 == 240);
  SPRAWDZ(seria({n.noc}, n.noc.data) == 1 && nocyPonad4h({n.noc}, n.noc.data) == 1);
  r = raportNocy(d31);
  r.replace(r.find("\"used_seconds\":25320"), 20, "\"used_seconds\":14369");  // 239,48 min → 239
  n = nocZRaportu(r, -1, "");
  SPRAWDZ(!n.noc.wazna && n.noc.m01 == 239 && seria({n.noc}, n.noc.data) == 0);
  // liczba JSON poza ±1e15 (też `1e309` = nieskończoność) czyni raport nieczytelnym;
  // czas terapii ponad dobę to brak M-01, nie rzutowanie poza zakres int
  r = raportNocy(d31);
  r.replace(r.find("\"used_seconds\":25320"), 20, "\"used_seconds\":1e309");
  SPRAWDZ(nocZRaportu(r, -1, "").stan == StanRaportu::NIECZYTELNY);
  r = raportNocy(d31);
  r.replace(r.find("\"used_seconds\":25320"), 20, "\"used_seconds\":-2e15");
  SPRAWDZ(nocZRaportu(r, -1, "").stan == StanRaportu::NIECZYTELNY);
  r = raportNocy(d31);
  r.replace(r.find("\"used_seconds\":25320"), 20, "\"used_seconds\":1e12");
  n = nocZRaportu(r, -1, "");
  SPRAWDZ(n.stan == StanRaportu::KOMPLETNA && n.noc.m01 == -1 && !n.noc.wazna);
  r = raportNocy(d31);
  r.replace(r.find("\"used_seconds\":25320"), 20, "\"used_seconds\":86400");
  SPRAWDZ(nocZRaportu(r, -1, "").noc.m01 == 1440);
  // M-03 i UA/h z bardzo małego used_seconds poza zakresem float = brak danych, nie `inf` w pliku
  r = raportNocy(d31);
  r.replace(r.find("\"used_seconds\":25320"), 20, "\"used_seconds\":1e-35");
  n = nocZRaportu(r, -1, "");
  SPRAWDZ(std::isnan(n.noc.oa) && std::isnan(n.noc.ca) && std::isnan(n.noc.h));
  SPRAWDZ(wierszNocyV3(n.noc).find("inf") == std::string::npos);
  {
    std::vector<DzienListy> dni;
    SPRAWDZ(!parsujListeNocy("{\"days\":[{\"day\":1e309,\"session_count\":1}]}", dni));
    SPRAWDZ(!parsujListeNocy("{\"days\":[{\"day\":3e9,\"session_count\":1}]}", dni));  // poza long: element bez `day`, lista odrzucona
    SPRAWDZ(parsujListeNocy("{\"days\":[{\"day\":20759,\"session_count\":5e9}]}", dni) && dni.size() == 1 &&
            dni[0].day == 20759 && dni[0].sesje == -1);  // session_count poza int: brak pola
    StatusAB st;
    SPRAWDZ(parsujStatus("{\"version\":\"1\",\"rop\":3e9}", st) && st.rop == -1);  // rop poza int: brak pola
  }
  // limit ciała osobno dla trasy
  SPRAWDZ(limitOdpowiedziAB("/api/status") == AB_MAKS_BAJTOW_STATUSU);
  SPRAWDZ(limitOdpowiedziAB(sciezkaListy()) == AB_MAKS_BAJTOW_LISTY);
  SPRAWDZ(limitOdpowiedziAB(sciezkaNocy(20759)) == AB_MAKS_BAJTOW_RAPORTU);
  SPRAWDZ(AB_MAKS_BAJTOW_STATUSU < AB_MAKS_BAJTOW_RAPORTU && AB_MAKS_BAJTOW_RAPORTU < AB_MAKS_BAJTOW_LISTY);
  r = raportNocy(d31);
  r.replace(r.find("\"events_known\":true"), 19, "\"events_known\":null");
  SPRAWDZ(nocZRaportu(r, -1, "").stan == StanRaportu::KOMPLETNA && !nocZRaportu(r, -1, "").noc.wazna);
  r = raportNocy(d31);
  r.replace(r.find("\"present\":true"), 14, "\"present\":false");
  SPRAWDZ(!nocZRaportu(r, -1, "").noc.wazna);
  // partial = true (albo null): rekord „częściowa”, nieważny, z liczbami (R-DANE-15)
  n = nocZRaportu(raportNocy(d31, "true"), -1, "");
  SPRAWDZ(n.stan == StanRaportu::CZESCIOWA && n.noc.czesciowa && !n.noc.wazna && !n.pelna && n.noc.m01 == 422);
  SPRAWDZ(nocZRaportu(raportNocy(d31, "null"), -1, "").stan == StanRaportu::CZESCIOWA);
  // klucz krytyczny: brak albo zły typ → E-03; null w `ahi` to brak pomiaru (R-AW-02)
  r = raportNocy(d31);
  r.replace(r.find("\"ahi\":1.4,"), 10, "");
  SPRAWDZ(nocZRaportu(r, -1, "").stan == StanRaportu::ZLY_FORMAT);
  r = raportNocy(d31);
  r.replace(r.find("\"ahi\":1.4"), 9, "\"ahi\":\"1,4\"");
  SPRAWDZ(nocZRaportu(r, -1, "").stan == StanRaportu::ZLY_FORMAT);
  r = raportNocy(d31);
  r.replace(r.find("\"ahi\":1.4"), 9, "\"ahi\":null");
  n = nocZRaportu(r, -1, "");
  SPRAWDZ(n.stan == StanRaportu::KOMPLETNA && std::isnan(n.noc.ahi));
  SPRAWDZ(nocZRaportu("[]", 1, "").stan == StanRaportu::ZLY_FORMAT);
  // tryb: spoza wydania albo nieznany → noc zapisana, ale nie ważna (R-DANE-29); bez MOP tryb ze /api/status.
  // (ADR-0030): AutoSet jest trybem obsługiwanym, więc noc ważna; spoza wydania: iVAPS
  r = raportNocy(d31);
  r.replace(r.find("\"value\":\"ASVAuto\""), 17, "\"value\":\"AutoSet\"");
  n = nocZRaportu(r, -1, "ASVAuto");
  SPRAWDZ(n.stan == StanRaportu::KOMPLETNA && n.noc.wazna && n.tryb == "AutoSet");
  r = raportNocy(d31);
  r.replace(r.find("\"value\":\"ASVAuto\""), 17, "\"value\":\"iVAPS\"");
  n = nocZRaportu(r, -1, "ASVAuto");
  SPRAWDZ(n.stan == StanRaportu::KOMPLETNA && !n.noc.wazna && n.tryb == "iVAPS");
  r = raportNocy(d31);
  r.replace(r.find("\"tag\":\"MOP\""), 11, "\"tag\":\"XXX\"");
  n = nocZRaportu(r, -1, "ASVAuto");
  SPRAWDZ(n.noc.wazna && n.tryb == "ASVAuto");
  ROWNE(n.noc.ustawienia, "/6/10/3/15");
  SPRAWDZ(!nocZRaportu(r, -1, "").noc.wazna);              // tryb nieznany
  // model aparatu (`pna`) nie decyduje o ważności nocy (R-DANE-29, ADR-0030); wcześniej test „inny model = nieważna”
  // sprawdzał parametr modelOk, usunięty razem z AB_MODEL (AirSense egzemplarza testowego miałby każdą noc nieważną)
  r = raportNocy(d31);
  for (const char* pna : {"AirSense_10_AutoSet", "AirSense_10_Elite", "AirCurve_10_CS_PaceWave", ""}) {
    SPRAWDZ(nocZRaportu(r, -1, "ASVAuto", pna, "SN1").noc.wazna);
    ROWNE(nocZRaportu(r, -1, "ASVAuto", pna, "SN1").noc.pna, pna);
  }
  r = raportNocy(d31);
  r.replace(r.find("\"available\":true"), 16, "\"available\":false");
  n = nocZRaportu(r, -1, "ASVAuto");
  SPRAWDZ(n.noc.ustawienia.empty() && n.noc.wazna);
  // `day` z żądania, gdy raport go nie ma
  r = raportNocy(d31);
  r.replace(r.find("\"view\":\"day\",\"day\":"), 19, "\"view\":\"day\",\"dzien\":");
  ROWNE(nocZRaportu(r, d31 + 1, "").noc.data, "2026-11-01");

  // --- doba tylko z podsumowania STR: bez channels, counts i settings → rekord z tym, co jest, reszta pusta ---
  std::string str = "{\"source\":\"edf\",\"view\":\"day\",\"day\":20500,\"sessions\":[],\"present\":true,"
                    "\"partial\":false,\"used_seconds\":27000,\"ahi\":2.1,\"events_known\":false,"
                    "\"session_count\":2}";
  n = nocZRaportu(str, 20500, "ASVAuto");
  SPRAWDZ(n.stan == StanRaportu::KOMPLETNA);
  ROWNE(zapiszNoc(n.noc), "2026-02-17;0;450;2.10;;;;;;;;;;A;0;0;;;;");  // events_known = false: nieważna (4.2)
  // kanały tylko ze STR: ciśnienie i EPAP puste (R-NOR-08), wyciek jest
  str = "{\"day\":20500,\"present\":true,\"partial\":false,\"used_seconds\":27000,\"ahi\":null,"
        "\"events_known\":true,\"channels\":[{\"id\":\"Press\",\"median\":9,\"p95\":12,\"source\":\"str\"},"
        "{\"id\":\"Leak\",\"unit\":\"L/min\",\"median\":3,\"p95\":11,\"source\":\"str\"}]}";
  n = nocZRaportu(str, 20500, "ASVAuto");
  ROWNE(zapiszNoc(n.noc), "2026-02-17;1;450;;;;3.00;11.00;;;;;;A;0;0;;;;");

  // --- plan pobrania: pełna historia, partiami od najnowszej, z postępem ---
  std::vector<Noc> hh = {noc("2026-10-29", 400, 1), noc("2026-10-30", 410, 1), noc("2026-10-31", 420, 1)};  // import
  for (auto& x : hh) x.zrodlo = 'K';
  std::set<long> pom;
  // 02.11 rano, lista 23 dób (od 11.10 do 2.11 w datach poranka): wszystkie bez rekordu z AirBridge, bez filtra daty;
  // najnowsze najpierw, najwyżej `maks`
  std::vector<long> plan = planPobrania(dni, hh, pom, "2026-11-02", false, 100000);
  SPRAWDZ(plan.size() == 23 && plan[0] == d31 + 2 && plan[22] == d31 - 20);
  plan = planPobrania(dni, hh, pom, "2026-11-02", false, 4);
  SPRAWDZ(plan.size() == 4 && plan[3] == d31 - 1);
  plan = planPobrania(dni, hh, pom, "2026-11-01", false, 100000);  // noc z przyszłości (2.11) poza planem
  SPRAWDZ(plan.size() == 22 && plan[0] == d31 + 1);
  // symulacja pierwszego pobrania: 4 raporty na wybudzenie co 30 min, doba d31-5 z `partial = true`, doba d31-7 bez
  // klucza krytycznego; nieudana doba czeka 3 h i po 3 próbach przestaje wracać, nie zatrzymując pozostałych
  {
    std::vector<Noc> h = hh;
    std::set<long> p;
    ProbyDob pr;
    uint32_t t = 1793600000;
    int wybudzenia = 0;
    while (true) {
      std::vector<long> pl = planPobrania(dni, h, p, "2026-11-02", false, 4, &pr, t);
      Zalegle z = policzZalegle(dni, h, p, "2026-11-02", pr, t);
      if (pl.empty()) {
        if (!z.czekajace) break;
        SPRAWDZ(!potrzebnaLista(h, "2026-11-02", 14 * 60, false, z.gotowe, z.czekajDo, t, p) || t >= z.czekajDo);
        t += 1800;
        continue;
      }
      wybudzenia++;
      for (long day : pl) {
        std::string rap = day == d31 - 5 ? raportNocy(day, "true") : raportNocy(day);
        if (day == d31 - 7) rap.replace(rap.find("\"present\":true,"), 15, "");
        NocAB x = nocZRaportu(rap, day, "");
        int ile = (pr.count(day) ? pr[day].proby : 0) + 1;
        PoRaporcie co = poRaporcie(x.stan, dataPoranka(day), "2026-11-02", 14 * 60, ile);
        if (co == PoRaporcie::ZAPISZ) {
          wpiszNocAB(h, x.noc, false, x.pelna);
          if (x.noc.czesciowa) zapiszNieudanaProbe(pr, day, t);
        } else if (co == PoRaporcie::PONOW) {
          zapiszNieudanaProbe(pr, day, t);
        } else if (co == PoRaporcie::POMIN) {
          zapiszNieudanaProbe(pr, day, t);
          p.insert(day);
        } else {
          SPRAWDZ(false);
        }
      }
      t += 1800;
      SPRAWDZ(wybudzenia < 40);
      if (wybudzenia >= 40) break;
    }
    // 23 doby po 4 = 6 wybudzeń; potem każda z 2 nieudanych dób jeszcze 2 razy, po 3 h czekania (4 wybudzenia)
    SPRAWDZ(wybudzenia == 10);
    // doba z `partial`: rekord „częściowa” (nieważny), nie w pominiętych; doba nieczytelna: pominięta po 3 próbach
    SPRAWDZ(h.size() == 22 && p.size() == 1 && p.count(d31 - 7) && !p.count(d31 - 5));
    SPRAWDZ(nocZDnia(h, dataPoranka(d31 - 5))->czesciowa && !nocZDnia(h, dataPoranka(d31 - 5))->wazna);
    SPRAWDZ(pr[d31 - 7].proby == 3 && pr[d31 - 5].proby == 3);
    // noc z AirBridge zastąpiła noc z importu tej samej daty; noce tylko z importu zostają
    SPRAWDZ(nocZDnia(h, "2026-10-31")->zrodlo == 'A' && nocZDnia(h, "2026-10-29")->zrodlo == 'A');
    // postęp z plików: po restarcie (te same pliki) nic do pobrania
    ProbyDob pr2 = parsujProby(wierszeProb(pr));
    SPRAWDZ(pr2.size() == pr.size() && pr2[d31 - 7].proby == 3 && pr2[d31 - 7].ostatnia == pr[d31 - 7].ostatnia);
    SPRAWDZ(planPobrania(dni, h, parsujPominiete(wierszePominietych(p)), "2026-11-02", false, 4, &pr2, t).empty());
    // kompletny raport doby „częściowa” (np. po oznaczeniu `stale`) ją nadpisuje (R-AW-03)
    SPRAWDZ(wpiszNocAB(h, nocZRaportu(raportNocy(d31 - 5), d31 - 5, "").noc, false));
    SPRAWDZ(!nocZDnia(h, dataPoranka(d31 - 5))->czesciowa && nocZDnia(h, dataPoranka(d31 - 5))->wazna);
    // doba pominięta (np. niepełna przy rekordzie z karty) nie wraca, także jako ostatnia noc; bez pominięcia jest
    std::set<long> p2 = p;
    p2.insert(d31 + 3);
    std::vector<DzienListy> d2 = dni;
    d2.push_back({d31 + 3, 1, false});
    SPRAWDZ(planPobrania(d2, h, p2, "2026-11-03", false, 4).empty());
    plan = planPobrania(d2, h, p, "2026-11-03", false, 4);
    SPRAWDZ(plan.size() == 1 && plan[0] == d31 + 3);
    std::vector<Noc> tylkoImport = {noc("2026-09-01", 400, 1)};
    tylkoImport[0].zrodlo = 'K';
    SPRAWDZ(!wpiszNocAB(h, h.back(), false));  // noc już z AirBridge
    for (auto& t2 : tylkoImport) h.push_back(t2);
    SPRAWDZ(nocZDnia(h, "2026-09-01")->zrodlo == 'K');  // AirBridge tej doby nie ma: noc z importu zostaje
  }
  SPRAWDZ(parsujPominiete({"20746", "x", "", "-3", "20747"}) == std::set<long>({20746, 20747}));
  SPRAWDZ(wierszePominietych({20747, 20746}) == std::vector<std::string>({"20746", "20747"}));
  // co po raporcie: ostatnia noc z `partial` czeka do 11:00, potem rekord „częściowa” (R-DANE-15);
  // starsza doba z `partial` od razu rekord „częściowa”; nieczytelna albo bez klucza: ponowienie, po 3 próbach pominięta
  SPRAWDZ(poRaporcie(StanRaportu::KOMPLETNA, "2026-11-01", "2026-11-02") == PoRaporcie::ZAPISZ);
  SPRAWDZ(poRaporcie(StanRaportu::CZESCIOWA, "2026-11-02", "2026-11-02", 10 * 60 + 59) == PoRaporcie::CZEKAJ);
  SPRAWDZ(poRaporcie(StanRaportu::CZESCIOWA, "2026-11-02", "2026-11-02", 11 * 60) == PoRaporcie::ZAPISZ);
  SPRAWDZ(poRaporcie(StanRaportu::CZESCIOWA, "2026-11-01", "2026-11-02", 6 * 60) == PoRaporcie::ZAPISZ);
  SPRAWDZ(poRaporcie(StanRaportu::ZLY_FORMAT, "2026-11-02", "2026-11-02") == PoRaporcie::E03);
  SPRAWDZ(poRaporcie(StanRaportu::ZLY_FORMAT, "2026-10-01", "2026-11-02", 9 * 60, 1) == PoRaporcie::PONOW);
  SPRAWDZ(poRaporcie(StanRaportu::ZLY_FORMAT, "2026-10-01", "2026-11-02", 9 * 60, 3) == PoRaporcie::POMIN);
  SPRAWDZ(poRaporcie(StanRaportu::NIECZYTELNY, "2026-11-02", "2026-11-02", 9 * 60, 2) == PoRaporcie::PONOW);
  SPRAWDZ(poRaporcie(StanRaportu::NIECZYTELNY, "2026-11-02", "2026-11-02", 9 * 60, 3) == PoRaporcie::E03);
  SPRAWDZ(poRaporcie(StanRaportu::NIECZYTELNY, "2026-10-01", "2026-11-02", 9 * 60, 2) == PoRaporcie::PONOW);
  SPRAWDZ(poRaporcie(StanRaportu::NIECZYTELNY, "2026-10-01", "2026-11-02", 9 * 60, 3) == PoRaporcie::POMIN);

  // --- pełna noc a doba tylko z STR ---
  {
    SPRAWDZ(nocZRaportu(raportNocy(d31), -1, "").pelna);
    std::string strOnly = "{\"day\":" + std::to_string(d31) + ",\"present\":true,\"partial\":false,"
                          "\"used_seconds\":27000,\"ahi\":2.1,\"events_known\":false,\"session_count\":1}";
    NocAB s1 = nocZRaportu(strOnly, d31, "ASVAuto");
    SPRAWDZ(s1.stan == StanRaportu::KOMPLETNA && !s1.pelna);
    // events_known = true, ale kanały tylko ze STR: też niepełna
    r = raportNocy(d31);
    for (size_t i; (i = r.find("\"source\":\"edf\"")) != std::string::npos;) r.replace(i, 14, "\"source\":\"str\"");
    SPRAWDZ(!nocZRaportu(r, -1, "").pelna);
    // kanały EDF, ale events_known = false: niepełna
    r = raportNocy(d31);
    r.replace(r.find("\"events_known\":true"), 19, "\"events_known\":false");
    SPRAWDZ(!nocZRaportu(r, -1, "").pelna);
    Noc karta = noc("2026-10-31", 420, 1);
    karta.zrodlo = 'K';
    // pełna noc AB vs import → AB
    std::vector<Noc> h = {karta};
    NocAB pel = nocZRaportu(raportNocy(d31), -1, "");
    bool zost = true;
    SPRAWDZ(wpiszNocAB(h, pel.noc, false, pel.pelna, &zost) && !zost && h[0].zrodlo == 'A');
    // STR-only vs import → import zostaje (także przy ponownym sprawdzeniu o 11:00)
    h = {karta};
    SPRAWDZ(!wpiszNocAB(h, s1.noc, false, s1.pelna, &zost) && zost && h[0].zrodlo == 'K' && h[0].m01 == 420);
    SPRAWDZ(!wpiszNocAB(h, s1.noc, true, s1.pelna, &zost) && zost && h[0].zrodlo == 'K');
    // STR-only vs pełny rekord z AirBridge → pełny zostaje
    h = {pel.noc};
    SPRAWDZ(!wpiszNocAB(h, s1.noc, true, s1.pelna, &zost) && zost && !std::isnan(h[0].emed));
    // STR-only w luce → zapis
    h = {noc("2026-10-29", 400, 1)};
    SPRAWDZ(wpiszNocAB(h, s1.noc, false, s1.pelna, &zost) && !zost && h.size() == 2);
    SPRAWDZ(nocZDnia(h, "2026-10-31")->zrodlo == 'A' && !rekordPelny(*nocZDnia(h, "2026-10-31")));
    // potem pełna noc AB tej daty (np. przy sprawdzeniu o 11:00) zastępuje niepełną
    SPRAWDZ(wpiszNocAB(h, pel.noc, true, pel.pelna, &zost) && rekordPelny(*nocZDnia(h, "2026-10-31")));
    SPRAWDZ(rekordPelny(karta) && rekordPelny(pel.noc) && !rekordPelny(s1.noc));
  }

  // --- zapis do historii: zastępowanie importu, R-AW-03, R-DANE-24 ---
  n = nocZRaportu(raportNocy(d31), -1, "");
  SPRAWDZ(wpiszNocAB(hh, n.noc, false) && nocZDnia(hh, "2026-10-31")->zrodlo == 'A' && hh.size() == 3);
  Noc stara = nocZRaportu(raportNocy(d31 - 30), -1, "").noc;  // noc sprzed miesiąca: bez filtra daty
  SPRAWDZ(wpiszNocAB(hh, stara, false) && hh.size() == 4);
  SPRAWDZ(!wpiszNocAB(hh, n.noc, false));  // ta sama noc drugi raz: bez zmian (R-AW-03)
  plan = planPobrania(dni, hh, pom, "2026-11-02", false, 5);
  SPRAWDZ(plan.size() == 5 && plan[1] == d31 + 1 && plan[2] == d31 - 1);  // 31.10 już z AirBridge
  // R-DANE-24: noc z dziś zapisana o 05:30, o 11:00 jedno ponowne pobranie; druga sesja = rekord nadpisany
  Noc dzis = nocZRaportu(raportNocy(d31 + 2), -1, "").noc;
  SPRAWDZ(wpiszNocAB(hh, dzis, false));
  SPRAWDZ(!sprawdzenie11("2026-11-02", 10 * 60 + 59, "2026-11-02", ""));
  SPRAWDZ(sprawdzenie11("2026-11-02", 11 * 60, "2026-11-02", ""));
  SPRAWDZ(!sprawdzenie11("2026-11-02", 11 * 60 + 30, "2026-11-02", "2026-11-02"));  // już sprawdzona
  SPRAWDZ(!sprawdzenie11("2026-11-02", 11 * 60, "2026-11-01", ""));  // noc zapisana po 11:00: bez sprawdzenia
  {
    auto pl = [&](const char* d, int m, bool s11, int g, uint32_t cz = 0, uint32_t t = 0) {
      return potrzebnaLista(hh, d, m, s11, g, cz, t, pom);
    };
    SPRAWDZ(!pl("2026-11-02", 9 * 60, false, 0) && pl("2026-11-02", 11 * 60, true, 0));
    SPRAWDZ(pl("2026-11-02", 9 * 60, false, 17));  // zaległe doby z pierwszego pobrania
    SPRAWDZ(pl("2026-11-02", 9 * 60, false, -1));  // nie wiadomo (pierwszy start)
    SPRAWDZ(pl("2026-11-03", 6 * 60, false, 0) && !pl("2026-11-03", 23 * 60 + 30, false, 5));
    SPRAWDZ(!pl("2026-11-03", 4 * 60, false, -1));  // slot nocny: tylko /api/status
    // zostały tylko doby czekające po nieudanej próbie: lista nie co 30 min, tylko po końcu czekania
    SPRAWDZ(!pl("2026-11-02", 9 * 60, false, 0, 1000000, 999999) && pl("2026-11-02", 9 * 60, false, 0, 1000000, 1000000));
    // ostatnia noc w pominiętych (doba tylko z STR przy rekordzie z karty): bez listy co 30 min
    std::vector<Noc> hk = {noc("2026-11-04", 400, 1)};
    SPRAWDZ(potrzebnaLista(hk, "2026-11-04", 9 * 60, false, 0, 0, 0, pom));
    SPRAWDZ(!potrzebnaLista(hk, "2026-11-04", 9 * 60, false, 0, 0, 0, {nrDnia("2026-11-03")}));
    // ostatnia noc jako rekord „częściowa”: lista dalej co 30 min (R-DANE-18)
    Noc cz = noc("2026-11-04", 400, 1);
    cz.zrodlo = 'A';
    cz.czesciowa = true;
    SPRAWDZ(potrzebnaLista({cz}, "2026-11-04", 12 * 60, false, 0, 0, 0, pom));
    cz.czesciowa = false;
    SPRAWDZ(!potrzebnaLista({cz}, "2026-11-04", 12 * 60, false, 0, 0, 0, pom));
  }
  plan = planPobrania(dni, hh, pom, "2026-11-02", true, 1);
  SPRAWDZ(plan.size() == 1 && plan[0] == d31 + 2);
  r = raportNocy(d31 + 2);
  r.replace(r.find("\"used_seconds\":25320"), 20, "\"used_seconds\":30720");
  r.replace(r.find("\"session_count\":1"), 17, "\"session_count\":2");
  Noc druga = nocZRaportu(r, -1, "").noc;
  SPRAWDZ(!wpiszNocAB(hh, druga, false));  // bez sprawdzenia o 11:00 zapisana noc z AirBridge zostaje
  SPRAWDZ(wpiszNocAB(hh, druga, true) && nocZDnia(hh, "2026-11-02")->m01 == 512);
  SPRAWDZ(!wpiszNocAB(hh, druga, true));  // raport bez zmian: rekord bez zmian
}

// ---------- stany odchylenia (4.6), tytuł S2a, nagłówek, porównanie tygodnia, K1/K2, sypialnia, kroje ----------
// Historia: n nocy ważnych przed `dzis` z AHI 1,0 / 2,0 na zmianę (norma: mediana 1,5, P10 1,0, P90 2,0), czas 420 min,
// wyciek P95 10 / 12, EPAP P95 8,0 / 8,4; dzis − k: noc k dni przed ostatnią.
static std::vector<Noc> histStany(const std::string& ost, int n) {
  std::vector<Noc> h;
  for (int i = n - 1; i >= 0; i--) {
    Noc r = noc(dodajDni(ost, -i), 420, i % 2 ? 1.0f : 2.0f, i % 2 ? 10.0f : 12.0f);
    r.ep95 = i % 2 ? 8.0f : 8.4f;
    r.emed = 7.0f;
    r.wmed = 4;
    h.push_back(r);
  }
  return h;
}

static void testStany() {
  const std::string d0 = "2026-10-01";
  // R-NOR-03: odchylenie AHI w nocach 1, 3, 5 → stan od nocy 5, nie wcześniej
  std::vector<Noc> h = histStany(d0, 30);
  std::string n1 = dodajDni(d0, 1);
  for (int k = 1; k <= 5; k++) {
    Noc r = noc(dodajDni(d0, k), 420, k % 2 ? 6.0f : 1.5f, 11);
    r.wmed = 4;
    h.push_back(r);
  }
  Stany s4 = liczStany(h, dodajDni(d0, 4), {});
  SPRAWDZ(!s4.ahi.aktywny);
  Stany s5 = liczStany(h, dodajDni(d0, 5), {});
  SPRAWDZ(s5.ahi.aktywny && s5.ahi.od == dodajDni(d0, 5) && s5.ahi.nocy == 1 && s5.ahi.wyzej);
  SPRAWDZ(s5.wyl.ahi.count(dodajDni(d0, 5)) && !s5.wyl.ahi.count(dodajDni(d0, 3)));  // R-NOR-09: noc stanu poza normą
  SPRAWDZ(!s5.uspokojenie);  // 5.4.13 pkt 3: metryka zdrowotna w stanie
  // R-NOR-04: noce 6–8 w granicy Δ/2 = 1,5 od mediany (1,5) → stan kończy się po 3. nocy; noc z +2 / h zeruje licznik
  std::vector<Noc> h2 = h;
  h2.push_back(noc(dodajDni(d0, 6), 420, 2.5f, 11));
  h2.push_back(noc(dodajDni(d0, 7), 420, 3.5f, 11));  // mediana + 2 → od nowa
  h2.push_back(noc(dodajDni(d0, 8), 420, 2.5f, 11));
  h2.push_back(noc(dodajDni(d0, 9), 420, 2.0f, 11));
  SPRAWDZ(liczStany(h2, dodajDni(d0, 9), {}).ahi.aktywny);
  h2.push_back(noc(dodajDni(d0, 10), 420, 1.0f, 11));
  SPRAWDZ(!liczStany(h2, dodajDni(d0, 10), {}).ahi.aktywny);
  // R-NOR-05: noc niepewna (mediana wycieku > 30 L/min) nie liczy się do AHI; P95 12 < 15 (wariant C:
  // od P95 ≥ 15 L/min noc i tak byłaby niepewna), więc rozstrzyga tu sama mediana
  std::vector<Noc> h3 = histStany(d0, 30);
  for (int k = 1; k <= 5; k++) {
    Noc r = noc(dodajDni(d0, k), 420, 6.0f, 12);
    r.wmed = k <= 3 ? 35 : 4;
    h3.push_back(r);
  }
  Stany s3 = liczStany(h3, dodajDni(d0, 5), {});
  SPRAWDZ(!s3.ahi.aktywny && !s3.e30);  // tylko 2 pewne noce z AHI ≥ 5
  SPRAWDZ(nocNiepewna(h3[30]) && !nocNiepewna(h3[34]));
  // E-30: AHI ≥ 5 w 3 z 7 nocy ważnych (bez odchylenia: norma AHI wysoka 6–9)
  std::vector<Noc> h4;
  for (int i = 29; i >= 0; i--) h4.push_back(noc(dodajDni(d0, -i), 420, 6.0f + (i % 4), 10));
  for (int k = 1; k <= 3; k++) h4.push_back(noc(dodajDni(d0, k), 430, 8.2f, 10));
  Stany s6 = liczStany(h4, dodajDni(d0, 3), {});
  SPRAWDZ(s6.e30 && !s6.ahi.aktywny && !s6.uspokojenie);
  // E-30 jedna definicja (katalog E.csv: mediana AHI z 7 nocy ważnych bez niepewnych): noce 6, 6, 6, 2, 2, 2, 2
  // → mediana 2 → nie (dawna reguła „≥ 3 z 7 nocy z AHI ≥ 5” dałaby tak); 6, 6, 6, 6, 2, 2, 2 → mediana 6 → tak
  {
    std::vector<Noc> h7 = histStany(d0, 30);
    const float ahi[] = {6, 6, 6, 2, 2, 2, 2};
    for (int k = 1; k <= 7; k++) h7.push_back(noc(dodajDni(d0, k), 420, ahi[k - 1], 11));
    SPRAWDZ(!liczStany(h7, dodajDni(d0, 7), {}).e30);
    h7[h7.size() - 4].ahi = 6.0f;
    SPRAWDZ(liczStany(h7, dodajDni(d0, 7), {}).e30);
    // noc niepewna (P95 18 L/min) wypada z okna: 6, 6, 6, [6 niepewna], 2, 2, 2 → okno sięga nocy sprzed serii
    h7[h7.size() - 4].wp95 = 18.0f;
    SPRAWDZ(!liczStany(h7, dodajDni(d0, 7), {}).e30);
  }
  // R-NOR-06: wyciek (techniczna) +12 L/min przez 21 nocy → koniec stanu, noce wracają do normy (nowy poziom);
  // AHI (zdrowotna) +5 / h przez 21 nocy → stan trwa, norma sprzed stanu (R-NOR-22)
  std::vector<Noc> h5 = histStany(d0, 30);
  for (int k = 1; k <= 23; k++) {
    Noc r = noc(dodajDni(d0, k), 420, 6.5f, 23);
    r.wmed = 4;
    h5.push_back(r);
  }
  Stany s7 = liczStany(h5, dodajDni(d0, 20), {});
  // wariant C: noce z P95 23 ≥ 15 L/min są niepewne, więc ich AHI nie wchodzi do stanu (R-NOR-05);
  // stan AHI (zdrowotna, R-NOR-22) sprawdza niżej ta sama seria z wyciekiem P95 11 (h5a)
  SPRAWDZ(s7.wp95.aktywny && !s7.ahi.aktywny && nocNiepewna(h5.back()));
  std::vector<Noc> h5a = histStany(d0, 30);
  for (int k = 1; k <= 23; k++) {
    Noc r = noc(dodajDni(d0, k), 420, 6.5f, 11);
    r.wmed = 4;
    h5a.push_back(r);
  }
  SPRAWDZ(liczStany(h5a, dodajDni(d0, 20), {}).ahi.aktywny);
  SPRAWDZ(liczStany(h5, dodajDni(d0, 22), {}).wp95.nocy == 20);  // stan od 3. nocy (R-NOR-03)
  Stany s8 = liczStany(h5, dodajDni(d0, 23), {});
  SPRAWDZ(!s8.wp95.aktywny && s8.wyl.wp95.empty());   // 21. noc stanu: nowy poziom wycieku
  SPRAWDZ(std::fabs(s8.norma.wp95.med - 23) < 1e-6);  // norma wycieku z nowych nocy
  Stany s8a = liczStany(h5a, dodajDni(d0, 23), {});
  SPRAWDZ(s8a.ahi.aktywny && s8a.ahi.nocy == 21);       // AHI: stan trwa po 21 nocach
  SPRAWDZ(s8a.wyl.ahi.size() == 21 && !s8a.wyl.ahi.count(dodajDni(d0, 2)));  // noce stanu AHI poza normą
  // stan techniczny bez wpływu na uspokojenie (5.4.13 pkt 3 tylko metryki zdrowotne)
  std::vector<Noc> h6 = histStany(d0, 30);
  for (int k = 1; k <= 11; k++) {
    Noc r = noc(dodajDni(d0, k), 420, 1.5f, 30);
    r.wmed = 4;
    r.ep95 = 8.2f;
    h6.push_back(r);
  }
  Stany s9 = liczStany(h6, dodajDni(d0, 11), {});
  SPRAWDZ(s9.wp95.aktywny && s9.wp95.nocy == 9 && s9.uspokojenieOcenione);
  // przegląd punkty warunku bez reguły w panelu (wzorce L, E-33, E-37, S-K-17…) = ocena niepełna, bez uspokojenia
  SPRAWDZ(s9.ocenaNiepelna && !s9.uspokojenie && !s9.nieoceniane.empty());
  // R-KOK-03: wyciek w stanie od 9 nocy → wiersz K1 w negatywie, `wyżej niż zwykle, od 9 nocy`; R-KOK-09: etykieta K2
  DaneK1 k1 = budujK1(h6, dodajDni(d0, 11), s9.norma, {}, &s9);
  SPRAWDZ(k1.w[4].stan);
  ROWNE(k1.w[4].slowo + " / " + k1.w[4].slowo2, "wyżej niż zwykle / od 9 nocy");  // T3: 2 linie
  // wariant C: 11 nocy z P95 30 ≥ 15 L/min jest niepewnych, więc wypadają z normy AHI (R-NOR-19)
  // i wiersz AHI uczy się od nowa; bez negatywu jak wcześniej
  ROWNE(k1.w[0].slowo, "uczę się");
  SPRAWDZ(!k1.w[0].stan);
  DaneK2 k2 = budujK2(h6, dodajDni(d0, 11), s9.norma, {}, &s9);
  SPRAWDZ(k2.w[6].stan && !k2.w[0].stan && !k2.w[7].stan);
  // R-KOK-05: bez normy `uczę się`, bez negatywu
  DaneK1 k1b = budujK1(h6, dodajDni(d0, 11), Norma(), {}, &s9);
  ROWNE(k1b.w[4].slowo, "uczę się");
  SPRAWDZ(!k1b.w[4].stan);
  // R-KOK-06: wiersze bez pola w rekordzie nocy (M-17, M-13) i M-03 bez danych w 30 nocach (noce testu jak z karty):
  // kreski w komórkach i w kolumnie normy
  for (int r : {1, 2, 3, 5, 9}) {
    ROWNE(k2.w[r].norma, "–");
    ROWNE(k2.w[r].k[6].tekst, "–");
    SPRAWDZ(!k2.w[r].stan);
  }
  // R-KOK-08 w dokładności ekranu: wartość równa P90 po zaokrągleniu bez ramki
  std::vector<Noc> h7 = histStany(d0, 30);
  Noc r7 = noc(dodajDni(d0, 1), 420, 2.04f, 12.3f);
  r7.wmed = 4;
  h7.push_back(r7);
  Stany s10 = liczStany(h7, dodajDni(d0, 1), {});
  DaneK2 k2b = budujK2(h7, dodajDni(d0, 1), s10.norma, {}, &s10);
  ROWNE(k2b.w[0].k[6].tekst, "2,0");
  SPRAWDZ(!k2b.w[0].k[6].poza && !k2b.w[6].k[6].poza);
}

static void testTytulINaglowek() {
  const std::string dzis = "2026-10-20";
  std::vector<Noc> h = histStany(dodajDni(dzis, -1), 30);
  Noc ost = noc(dzis, 398, 1.1f, 11);
  ost.wmed = 4;
  ost.ep95 = 8.2f;
  ost.emed = 7.0f;
  h.push_back(ost);
  Stany st = liczStany(h, dzis, {});
  DaneDnia d;
  d.dzis = dzis;
  d.hist = &h;
  d.stany = &st;
  d.norma = st.norma;
  d.godzinaDanych = "07:12";
  // przegląd warunek uspokojenia oceniany niepełnie (bez wzorców L, E-33, E-37, S-K-17) → nigdy `W normie`
  TrescEkranu t = trescDnia("S2A", "1", d);
  SPRAWDZ(st.ocenaNiepelna && st.uspokojenieOcenione && !st.uspokojenie);
  ROWNE(t.tytul, "Noc: 6 h 38 z maską.");
  ROWNE(trescDnia("S2C", "1", d).tytul, "Noc: 6 h 38 z maską.");
  // R-EKR-41 / R-EKR-46 przy pełnej ocenie (jak po dodaniu reguł: uspokojenie spełnione) → `W normie: …`
  Stany pelna = st;
  pelna.ocenaNiepelna = false;
  pelna.nieoceniane.clear();
  pelna.uspokojenie = true;
  d.stany = &pelna;
  ROWNE(trescDnia("S2A", "1", d).tytul, "Noc w normie.");  // 5.4.2
  d.stany = &st;
  ROWNE(t.powod, "Tydzień jak zwykle.");
  ROWNE(naglowekDniaPrawo(d), "dane 07:12");
  SPRAWDZ(t.uklad == Uklad::TYDZIEN && t.kolumny.size() == 4);
  ROWNE(t.kolumny[0].et, "czas z maską");
  ROWNE(t.kolumny[0].s, "↔");
  ROWNE(t.kolumny[0].p, "");  // `jak zwykle` mówi legenda paska (wariant B)
  SPRAWDZ(!t.kolumny[0].poza);
  // mediana czasu z maską w pasku `6:38`, celowo innym zapisem niż noc `6 h 38` nad paskiem (5.4.2, SPEC-B)
  SPRAWDZ(t.kolumny[0].w.find(':') != std::string::npos && t.kolumny[0].w.find(" h ") == std::string::npos);
  ROWNE(t.kolumny[3].et, "wyciek l/min");
  ROWNE(t.kolumny[3].w, "11");  // mediana 7 nocy wycieku P95: 12, 10, 12, 10, 12, 10, 11 → 11
  // noc z odchyleniem AHI 4,1 (R-CZY-01) → `Noc: 6 h 38, AHI 4,1.`
  std::vector<Noc> h2 = h;
  h2.back().ahi = 4.6f;
  Stany s2 = liczStany(h2, dzis, {});
  d.hist = &h2;
  d.stany = &s2;
  ROWNE(trescDnia("S2A", "1", d).tytul, "Noc: 6 h 38 z maską.");
  // noc niepewna (duży wyciek, 4.9) → `Noc: 6 h 38.` i `· ocena ograniczona` (R-EKR-46)
  std::vector<Noc> h3 = h;
  h3.back().wmed = 34;
  Stany s3 = liczStany(h3, dzis, {});
  d.hist = &h3;
  d.stany = &s3;
  ROWNE(trescDnia("S2A", "1", d).tytul, "Noc: 6 h 38 z maską.");
  ROWNE(naglowekDniaPrawo(d), "dane 07:12 · ocena ograniczona");
  // R2: karta K22 po nocy niepewnej z P95 18 L/min (wariant C) bez liczby AHI,
  // w miejscu AHI `–` z podpisem `AHI niepewne`, jak faktyPrzerywnika() (R-EKR-46, ADR-0029 pkt 7)
  {
    std::vector<Noc> h3b = h;
    h3b.back().wp95 = 18;
    Stany s3b = liczStany(h3b, dzis, {});
    d.hist = &h3b;
    d.stany = &s3b;
    TrescEkranu k22 = trescDnia("S2A", "1", d);
    ROWNE(k22.tytul, "Noc: 6 h 38 z maską.");
    SPRAWDZ(k22.ahiNiepewne && k22.fakty.size() == 2);
    if (k22.fakty.size() == 2) {
      ROWNE(k22.fakty[0].w + "|" + k22.fakty[0].p, "–|AHI niepewne");
      ROWNE(k22.fakty[1].w + "|" + k22.fakty[1].p, "18|wyciek l/min");
    }
    d.hist = &h3;
    d.stany = &s3;
  }
  // R-EKR-39: brak nowej nocy, ostatnia z 18.10 → `dane z 18.10`, tytuł o tej nocy
  std::vector<Noc> h4(h.begin(), h.end() - 2);
  Stany s4 = liczStany(h4, dzis, {});
  d.hist = &h4;
  d.stany = &s4;
  ROWNE(naglowekDniaPrawo(d), "dane z 18.10");
  ROWNE(naglowekDniaLewo(d), "wt 20.10");
  // brak M-02 w ostatniej nocy (fail-path R-EKR-41) → `Noc: 6 h 38.` (przy pełnej ocenie `W normie: 6 h 38.`)
  std::vector<Noc> h5 = h;
  h5.back().ahi = NAN;
  Stany s5 = liczStany(h5, dzis, {});
  d.hist = &h5;
  d.stany = &s5;
  ROWNE(trescDnia("S2A", "1", d).tytul, "Noc: 6 h 38 z maską.");
  s5.ocenaNiepelna = false;
  s5.uspokojenie = true;
  ROWNE(trescDnia("S2A", "1", d).tytul, "Noc w normie.");  // 5.4.2
  // noc krótsza niż 4 h: bez oceny odchylenia, więc bez `W normie`
  std::vector<Noc> h6 = h;
  h6.back().m01 = 200;
  h6.back().wazna = false;
  Stany s6 = liczStany(h6, dzis, {});
  d.hist = &h6;
  d.stany = &s6;
  ROWNE(trescDnia("S2A", "1", d).tytul, "Noc: 3 h 20 z maską.");
  // E-30 aktywne: `Noc: …` (R-EKR-41, przykład z normą AHI 6–9 / h)
  std::vector<Noc> h7;
  for (int i = 29; i >= 0; i--) h7.push_back(noc(dodajDni(dzis, -3 - i), 420, 6.0f + (i % 4), 10));
  for (int k = 2; k >= 0; k--) h7.push_back(noc(dodajDni(dzis, -k), 430, 8.2f, 10));
  Stany s7 = liczStany(h7, dzis, {});
  d.hist = &h7;
  d.stany = &s7;
  ROWNE(trescDnia("S2A", "1", d).tytul, "Noc: 7 h 10 z maską.");
}

static void testPorownanie() {
  NormaMetryki nm;
  nm.n = 30;
  nm.med = 1.5;
  nm.p10 = 1.0;
  nm.p90 = 2.0;
  Porownanie p = porownajTydzien({1.2, 1.4, 1.9}, nm, Strona::NIZEJ_LEPIEJ);
  ROWNE(p.slowo, "↔ jak zwykle");
  SPRAWDZ(p.spokojne && std::fabs(p.mediana - 1.4) < 1e-9);
  p = porownajTydzien({0.6, 0.8, 0.9}, nm, Strona::NIZEJ_LEPIEJ);
  ROWNE(p.slowo, "↓ lepiej niż zwykle");
  SPRAWDZ(p.spokojne);
  p = porownajTydzien({2.6, 2.8, 2.9}, nm, Strona::NIZEJ_LEPIEJ);
  ROWNE(p.slowo, "↑ wyżej niż zwykle");
  SPRAWDZ(!p.spokojne);
  p = porownajTydzien({2.6, 2.8, 2.9}, nm, Strona::WYZEJ_LEPIEJ);
  ROWNE(p.slowo, "↑ lepiej niż zwykle");
  p = porownajTydzien({0.6}, nm, Strona::WYZEJ_LEPIEJ);
  ROWNE(p.slowo, "↓ niżej niż zwykle");
  p = porownajTydzien({0.6}, nm, Strona::BEZ_OCENY);  // EPAP: bez „lepiej” (ADR-0002)
  ROWNE(p.slowo, "↓ niżej niż zwykle");
  SPRAWDZ(!p.spokojne);
  ROWNE(porownajTydzien({2.0}, nm, Strona::BEZ_OCENY).slowo, "↔ jak zwykle");  // granica pasma w paśmie
  ROWNE(porownajTydzien({1.0}, NormaMetryki(), Strona::NIZEJ_LEPIEJ).slowo, "uczę się");
  ROWNE(porownajTydzien({}, nm, Strona::NIZEJ_LEPIEJ).slowo, "–");
}

static void testSypialnia() {
  BuforSypialni b;
  // 22:30 poza oknem; 23:00, 23:30 (noc z datą poranka jutro), 05:30; NAN pomijany (R-AW-13)
  dodajOdczyt(b, "2026-10-29", 22 * 60 + 30, 25, 50);
  SPRAWDZ(b.n == 0);
  dodajOdczyt(b, "2026-10-29", 23 * 60, 21.0f, 44);
  dodajOdczyt(b, "2026-10-29", 23 * 60 + 30, 20.6f, 46);
  dodajOdczyt(b, "2026-10-30", 2 * 60, NAN, 47);
  dodajOdczyt(b, "2026-10-30", 5 * 60 + 30, 20.2f, 48);
  SPRAWDZ(b.n == 3 && std::string(b.data) == "2026-10-30");
  NocSypialni n;
  SPRAWDZ(!zamknijNoc(b, "2026-10-30", 5 * 60 + 45, n));  // jeszcze okno nocy
  SPRAWDZ(zamknijNoc(b, "2026-10-30", 6 * 60, n) && b.n == 0);
  ROWNE(n.data, "2026-10-30");
  SPRAWDZ(std::fabs(n.t - 20.6f) < 1e-4 && std::fabs(n.w - 46) < 1e-4);
  ROWNE(zapiszSypialnie(n), "2026-10-30;20.6;46.0");
  NocSypialni r;
  SPRAWDZ(parsujSypialnie("2026-10-30;20.6;46.0", r) && r.data == "2026-10-30" && std::fabs(r.t - 20.6f) < 1e-4);
  SPRAWDZ(parsujSypialnie("2026-10-30;;46", r) && std::isnan(r.t));
  SPRAWDZ(!parsujSypialnie("2026-10-30;x;46", r));
  ROWNE(stopkaCzujnika(20.6f, 46.0f), "przy łóżku 21° · 46%");  // R-EKR-10: czujnik panelu
  ROWNE(stopkaCzujnika(NAN, 46.0f), "");                   // R-AW-13: brak odczytu, bez komunikatu
  // norma sypialni: 14 nocy minimum, bez nocy „spanie poza domem”
  std::vector<NocSypialni> s;
  for (int i = 1; i <= 14; i++) s.push_back({dodajDni("2026-10-30", -i), (float)(20 + i % 3), (float)(44 + i % 4)});
  NormaSypialni nm = liczNormeSypialni(s, "2026-10-30", {});
  SPRAWDZ(nm.t.jest() && std::fabs(nm.t.med - 21) < 1e-9);
  SPRAWDZ(!liczNormeSypialni(s, "2026-10-30", {dodajDni("2026-10-30", -1)}).t.jest());
  bool sp = false;
  ROWNE(slowoSypialni(21, nm.t, sp, "°"), "↔ jak zwykle");
  SPRAWDZ(sp);
  ROWNE(slowoSypialni(25, nm.t, sp, "°"), "↑ zwykle 21,0°");
  SPRAWDZ(!sp);
  // S2c (5.4.2, R-EKR-17): bez Qingping 2 kolumny; powód: propozycja
  std::vector<Noc> h = histStany("2026-10-30", 30);
  Stany st = liczStany(h, "2026-10-30", {});
  DaneDnia d;
  d.dzis = "2026-10-30";
  d.hist = &h;
  d.stany = &st;
  d.sypNoc = &n;
  d.sypNorma = nm;
  TrescEkranu t = trescDnia("S2C", "2", d);
  SPRAWDZ(t.kolumny.size() == 2 && t.uklad == Uklad::TYDZIEN);
  ROWNE(t.kolumny[0].w, "20,6°");
  ROWNE(t.kolumny[1].w, "46%");
  ROWNE(t.powod, "Dobre warunki do snu.");
  ROWNE(t.etykieta, "PRZY ŁÓŻKU · OSTATNIA NOC");
  // rotacja: S2c tylko przy StanDnia::sypialnia
  StanDnia sd;
  sd.dzis = "2026-10-30";  // piątek
  sd.nocyWaznych = 30;
  sd.maNorme = true;
  sd.seria = 1;  // bez S2b
  std::vector<WpisHumoru> log = {{"2026-10-29", "S2A", "1", 'D', "", ""}};
  ROWNE(wybierzEkranDnia(sd, log).typ, "S2A");
  sd.sypialnia = true;
  ROWNE(wybierzEkranDnia(sd, log).typ, "S2C");
}

static void testKroje() {
  // przyczyna „m aską” (widoczna na ekranie panelu): prawa nóżka „m” znikała przy progowaniu 1:1, a szerokość
  // znaku była zaokrąglana do piksela. Teraz glif „m” 18 px ma trzy nóżki (szerokość ≥ 12 px przy kroku ok. 15,6 px),
  // a tekst sumuje szerokości w 1/16 px. W skali B najmniejszy krój to P22B (22 px, 600).
  const GFXglyph* m = &F_P22B->glyph['m' - F_P22B->first];
  SPRAWDZ(m->width >= 15 && m->xAdvance - m->width - m->xOffset <= 3);
  SPRAWDZ(szer(F_P22B, "mmmmmmmmmm") == 194);  // 10 × 19,375 px
  SPRAWDZ(szer(F_P22B, "MASKA", 2) == szer(F_P22B, "MASKA") + 8);  // rozstrzelenie bez odstępu po ostatniej literze
  SPRAWDZ(szer(F_P22B, "→") > 5 && szer(F_P22B, "CO₂") > szer(F_P22B, "CO"));  // znaki spoza Public Sans z zapasu
}

// ---------- przypadki brzegowe ----------
// Dane syntetyczne (WY12): raportNocy() i noc() jak wyżej, bez realnych nocy.
static void testPrzypadkiBrzegowe() {
  const long d = nrDnia("2026-10-29");  // noc 29/30.10, data poranka 30.10
  const std::string dzis = "2026-10-30";
  // pełna noc z AirBridge zastępuje kartę, ale zostawia pola, których raport nie ma (PS)
  {
    Noc k;
    SPRAWDZ(parsujNoc("2026-10-30;1;420;1.20;9.80;13.60;2.40;9.60;6.40;8.40;3.40;6.10;ASVAuto/6/10/3/15", 'K', k));
    std::vector<Noc> h = {k};
    NocAB n = nocZRaportu(raportNocy(d), d, "ASVAuto");
    bool z = true;
    SPRAWDZ(wpiszNocAB(h, n.noc, false, n.pelna, &z) && !z);
    SPRAWDZ(h[0].zrodlo == 'A' && h[0].m01 == 422 && std::fabs(h[0].psmed - 3.4f) < 1e-4 &&
            std::fabs(h[0].psp95 - 6.1f) < 1e-4 && std::fabs(h[0].ca - 0.71f) < 0.01f);
    // sprawdzenie o 11:00 (nadpisz) z raportem bez zmian: PS nadal jest, rekord bez zmian
    SPRAWDZ(!wpiszNocAB(h, n.noc, true, n.pelna) && std::fabs(h[0].psmed - 3.4f) < 1e-4);
    // import karty po pełnej nocy z AirBridge: rekord A zostaje i dostaje z karty brakujące pola (PS)
    std::vector<Noc> ha = {n.noc};
    SPRAWDZ(wpiszImport(ha, k) == WynikImportu::UZUPELNIONA);
    SPRAWDZ(ha[0].zrodlo == 'A' && ha[0].m01 == 422 && std::fabs(ha[0].psmed - 3.4f) < 1e-4);
    SPRAWDZ(wpiszImport(ha, k) == WynikImportu::POMINIETA);  // drugi raz: nic do dopisania
    // import po dobie tylko z STR: karta zastępuje, M-03 z AirBridge zostaje
    std::string strOnly = "{\"day\":" + std::to_string(d) + ",\"present\":true,\"partial\":false,"
                          "\"used_seconds\":27000,\"ahi\":2.1,\"events_known\":false,\"counts\":{\"ca\":9}}";
    std::vector<Noc> hs = {nocZRaportu(strOnly, d, "ASVAuto").noc};
    SPRAWDZ(wpiszImport(hs, k) == WynikImportu::ZASTAPIONA);
    SPRAWDZ(hs[0].zrodlo == 'K' && hs[0].m01 == 420 && std::fabs(hs[0].ca - 1.2f) < 1e-4);
    std::vector<Noc> hp;
    SPRAWDZ(wpiszImport(hp, k) == WynikImportu::DODANA && hp.size() == 1);
  }
  // jednostka wycieku bez względu na wielkość liter i spacje; brak jednostki = brak danych (nie zgadujemy ×60)
  SPRAWDZ(mnoznikWycieku("L/min") == 1 && mnoznikWycieku("l / MIN") == 1 && mnoznikWycieku(" L/S ") == 60);
  SPRAWDZ(std::isnan(mnoznikWycieku("")) && std::isnan(mnoznikWycieku("LPM")) && std::isnan(mnoznikWycieku("mL/s")));
  {
    std::string r = raportNocy(d);
    r.replace(r.find("\"unit\":\"L/min\""), 14, "\"unit\":\"l/s\"");
    NocAB n = nocZRaportu(r, d, "");
    SPRAWDZ(std::fabs(n.noc.wmed - 144.0f) < 1e-3 && std::fabs(n.noc.wp95 - 576.0f) < 1e-3);
    r = raportNocy(d);
    r.replace(r.find("\"unit\":\"L/min\","), 15, "");
    n = nocZRaportu(r, d, "");
    SPRAWDZ(std::isnan(n.noc.wmed) && std::isnan(n.noc.wp95) && n.stan == StanRaportu::KOMPLETNA);
  }
  // MOP jako numer opcji (OPCJE w tools/kalibracja/noce.py: 8 = ASVAuto), także bez `option`
  {
    auto zMop = [&](const std::string& pole) {
      std::string r = raportNocy(d);
      size_t a = r.find("{\"tag\":\"MOP\"");
      size_t b = r.find('}', a);
      r.replace(a, b - a + 1, pole);
      return nocZRaportu(r, d, "AutoSet");
    };
    NocAB n = zMop("{\"tag\":\"MOP\",\"value\":8}");
    SPRAWDZ(n.tryb == "ASVAuto" && n.noc.wazna);
    ROWNE(n.noc.ustawienia, "ASVAuto/6/10/3/15");
    SPRAWDZ(zMop("{\"tag\":\"MOP\",\"value\":\"8\",\"option\":8}").tryb == "ASVAuto");
    SPRAWDZ(zMop("{\"tag\":\"MOP\",\"value\":null,\"option\":8}").tryb == "ASVAuto");
    SPRAWDZ(zMop("{\"tag\":\"MOP\",\"value\":\"ASVAuto\",\"option\":8}").tryb == "ASVAuto");
    SPRAWDZ(zMop("{\"tag\":\"MOP\",\"value\":1}").tryb == "AutoSet");
    // MOP nieczytelny: tryb nieznany (noc nieważna), bez zapasu ze statusu
    n = zMop("{\"tag\":\"MOP\",\"value\":99}");
    SPRAWDZ(n.tryb.empty() && !n.noc.wazna);
    std::string r = raportNocy(d);
    r.replace(r.find("\"value\":\"6.0\""), 13, "\"value\":\"6.0 cmH2O\"");
    ROWNE(nocZRaportu(r, d, "").noc.ustawienia, "ASVAuto/6/10/3/15");
  }
  // brak `used_seconds` = „–”, tytuł bez czasu, nigdy `0 h 00` (R-DANE-07, R-AW-22), na każdym ekranie dnia
  {
    std::string r = raportNocy(d);
    r.replace(r.find("\"used_seconds\":25320"), 20, "\"used_seconds\":null");
    NocAB n = nocZRaportu(r, d, "ASVAuto");
    SPRAWDZ(n.stan == StanRaportu::KOMPLETNA && n.noc.m01 == -1 && !n.noc.wazna && std::isnan(n.noc.ca));
    std::vector<Noc> h = histStany(dodajDni(dzis, -1), 30);
    h.push_back(n.noc);
    Stany st = liczStany(h, dzis, {});
    DaneDnia dd;
    dd.dzis = dzis;
    dd.hist = &h;
    dd.stany = &st;
    dd.norma = st.norma;
    ROWNE(tytulNocy(n.noc, st), "Noc zapisana.");
    ROWNE(naglowekDniaLewo(dd), "pt 30.10");
    for (const char* typ : {"S2A", "S2C", "S2B", "S6", "KM", "S5"}) {
      TrescEkranu t = trescDnia(typ, "100", dd);
      std::string wszystko = t.tytul + "|" + t.powod + "|" + t.krok;
      for (auto& f : t.fakty) wszystko += "|" + f.w;
      for (auto& k : t.kolumny) wszystko += "|" + k.w;
      SPRAWDZ(wszystko.find("0 h 00") == std::string::npos && wszystko.find("0:00") == std::string::npos);
    }
    ROWNE(trescDnia("S2A", "", dd).fakty[0].w, "–");
    Noc pusta = n.noc;
    pusta.ahi = NAN;
    ROWNE(tytulNocy(pusta, st), "Noc zapisana.");
    DaneK1 k1 = budujK1(h, dzis, st.norma, {}, &st);
    ROWNE(k1.liczby[0], "–");
  }
  // raport z innym `day` niż żądany: nic nie zapisuje; starsza doba po 3 próbach nie wraca, ostatnia noc wraca
  // co 30 min (R-DANE-18) aż do E-03 po 3 próbach
  {
    NocAB n = nocZRaportu(raportNocy(d - 1), d, "ASVAuto");
    SPRAWDZ(n.stan == StanRaportu::NIECZYTELNY && n.noc.data.empty());
    std::vector<Noc> h;
    SPRAWDZ(!wpiszNocAB(h, n.noc, false, n.pelna) && h.empty());
    std::vector<DzienListy> lista{{d, 1, false}, {d - 5, 1, false}};
    ProbyDob pr;
    uint32_t t = 1793600000;
    for (int i = 1; i <= 3; i++) {
      SPRAWDZ(poRaporcie(StanRaportu::NIECZYTELNY, dataPoranka(d - 5), dzis, 14 * 60, i) ==
              (i < 3 ? PoRaporcie::PONOW : PoRaporcie::POMIN));
      zapiszNieudanaProbe(pr, d - 5, t);
      zapiszNieudanaProbe(pr, d, t);
      t += AB_ODSTEP_DOBY_S;
    }
    std::vector<long> p = planPobrania(lista, h, {}, dzis, false, 10, &pr, t);
    SPRAWDZ(p.size() == 1 && p[0] == d);  // starsza doba wyczerpana; ostatnia noc co 30 min
    SPRAWDZ(poRaporcie(StanRaportu::NIECZYTELNY, dzis, dzis, 14 * 60, 3) == PoRaporcie::E03);
    // doba czeka po nieudanej próbie i nie blokuje kolejnych
    ProbyDob pr2;
    zapiszNieudanaProbe(pr2, d - 5, t);
    std::vector<DzienListy> l3{{d - 5, 1, false}, {d - 6, 1, false}};
    p = planPobrania(l3, h, {}, dzis, false, 10, &pr2, t + 60);
    SPRAWDZ(p.size() == 1 && p[0] == d - 6);
    SPRAWDZ(dobaCzeka(pr2, d - 5, t + AB_ODSTEP_DOBY_S - 1) && !dobaCzeka(pr2, d - 5, t + AB_ODSTEP_DOBY_S));
    Zalegle z = policzZalegle(l3, h, {}, dzis, pr2, t + 60);
    SPRAWDZ(z.gotowe == 1 && z.czekajace == 1 && z.czekajDo == t + AB_ODSTEP_DOBY_S);
    SPRAWDZ(parsujProby({"20746;2;1793600000;0", "x", "20747;1;5;1", "20748;-1;5;0", "20749;1;5;2"}).size() == 2);
    // bez sscanf: reszta po polu, brakujące albo nadmiarowe pole, przepełnienie i znak odrzucone (cert-err34-c)
    SPRAWDZ(parsujProby({"20750;1;5;0x", "20751;1;5", "20752;1;5;0;9", "20753;99999999999;5;0", "20754;1;4294967296;0",
                         "20755;+1;5;0", "20756; 1;5;0", ";1;5;0", "99999999999999999999999;1;5;0"}).empty());
    {
      ProbyDob g = parsujProby({"20757;3;4294967295;1"});
      SPRAWDZ(g.size() == 1 && g[20757].proby == 3 && g[20757].ostatnia == 4294967295u && g[20757].staleZrobione);
    }
    // brzegi: puste i ujemne pola, za długie liczby, reszta po liczbie, CR
    SPRAWDZ(parsujProby({";1;5;0", "20750;;5;0", "20750;1;;0", "20750;1;5;"}).empty());
    SPRAWDZ(parsujProby({"-1;1;5;0", "20750;-1;5;0", "20750;1;-5;0", "20750;1;5;-1"}).empty());
    SPRAWDZ(parsujProby({"123456789012345678901;1;5;0", "20750;1;5;00000000000000000000001"}).empty());
    SPRAWDZ(parsujProby({"20750x;1;5;0", "20750;1x;5;0", "20750;1;5x;0", "20750;1;5;0 ", "20750;1;5;0\r"}).empty());
    SPRAWDZ(parsujProby({"20750;1;5", "20750;1;5;0;1", "20750;1;5;0;"}).empty());
    {
      ProbyDob g = parsujProby({"020750;007;0;0", "0;0;4294967295;1", "20751;2147483647;1;1"});
      SPRAWDZ(g.size() == 3 && g[20750].proby == 7 && g[0].ostatnia == 4294967295u && g[20751].proby == 2147483647);
      SPRAWDZ(parsujProby({"20751;2147483648;1;1"}).empty());  // ponad INT_MAX
      ProbyDob d = parsujProby({"20750;1;5;0", "20750;2;6;1"});  // ostatni wiersz tej samej doby wygrywa
      SPRAWDZ(d.size() == 1 && d[20750].proby == 2 && d[20750].staleZrobione);
      std::mt19937 los(12345);  // zapis → odczyt → zapis bez zmian dla wierszy, które pisze firmware
      bool zgodne = true;
      for (int i = 0; i < 2000 && zgodne; i++) {
        ProbyDob q;
        int k = 1 + (int)(los() % 20);
        for (int j = 0; j < k; j++) q[(long)(los() % 40000)] = ProbyDoby{(int)(los() % 1000), (uint32_t)los(), (los() & 1) != 0};
        std::vector<std::string> w = wierszeProb(q);
        zgodne = wierszeProb(parsujProby(w)) == w;
      }
      SPRAWDZ(zgodne);
    }
    {  // dataOk() przyjmuje dokładnie te napisy, które daje dataZNr() (kalendarz gregoriański 1900–2200)
      std::set<std::string> prawdziwe;
      for (long n = nrDnia("1900-01-01"); n <= nrDnia("2200-12-31"); n++) prawdziwe.insert(dataZNr(n));
      int niezgodne = 0;
      char b[16];
      for (int r = 1900; r <= 2200; r++)
        for (int m = 0; m <= 13; m++)
          for (int dz = 0; dz <= 32; dz++) {
            snprintf(b, sizeof b, "%04d-%02d-%02d", r, m, dz);
            if (dataOk(b) != (prawdziwe.count(b) == 1)) niezgodne++;
          }
      SPRAWDZ(prawdziwe.size() > 100000 && niezgodne == 0);
    }
  }
  // doba oznaczona `stale` jest pobierana raz jeszcze (także z rekordem A i w pominiętych), potem nie
  {
    std::vector<Noc> h = {nocZRaportu(raportNocy(d - 3), d - 3, "ASVAuto").noc};
    std::vector<DzienListy> lista{{d - 3, 1, true}, {d - 4, 1, true}};
    ProbyDob pr;
    std::vector<long> p = planPobrania(lista, h, {d - 4}, dzis, false, 10, &pr, 0);
    SPRAWDZ(p.size() == 2 && p[0] == d - 3 && p[1] == d - 4);
    SPRAWDZ(planPobrania(lista, h, {d - 4}, dzis, false, 10, nullptr, 0).empty());  // bez licznika: bez `stale`
    pr[d - 3].staleZrobione = true;
    pr[d - 4].staleZrobione = true;
    SPRAWDZ(planPobrania(lista, h, {d - 4}, dzis, false, 10, &pr, 0).empty());
    SPRAWDZ(!odswiezStale(pr, lista));  // nadal `stale`: znaczniki zostają
    lista[0].stale = false;
    SPRAWDZ(odswiezStale(pr, lista) && !pr.count(d - 3) && pr.count(d - 4));
    lista[0].stale = true;  // nowe oznaczenie: znów raz
    p = planPobrania(lista, h, {d - 4}, dzis, false, 10, &pr, 0);
    SPRAWDZ(p.size() == 1 && p[0] == d - 3);
    SPRAWDZ(wierszeProb(pr) == std::vector<std::string>({std::to_string(d - 4) + ";0;0;1"}));
    // `stale` na liście: pole czytane z listy
    std::vector<DzienListy> dl;
    SPRAWDZ(parsujListeNocy("{\"days\":[{\"day\":5,\"stale\":true},{\"stale\":false,\"day\":6,\"x\":[{}]}]}", dl));
    SPRAWDZ(dl.size() == 2 && dl[0].stale && !dl[1].stale && dl[1].day == 6);
  }
  // 200 z jobem bez raportu → ponowny GET, nie raport bez kluczy (fałszywe E-03)
  SPRAWDZ(klasyfikujOdpowiedz(200, "{\"job\":\"edf:8\",\"pending\":false}").r == Odpowiedz::W_BUDOWIE);
  SPRAWDZ(klasyfikujOdpowiedz(200, "{\"job\":\"edf:8\",\"days\":[]}").r == Odpowiedz::GOTOWA);
  SPRAWDZ(klasyfikujOdpowiedz(200, raportNocy(d, "false", ",\"job\":\"edf:8\"")).r == Odpowiedz::GOTOWA);
  // lista urwana albo z elementem, który nie jest obiektem: nieczytelna (bez częściowego wyniku)
  {
    std::vector<DzienListy> dl;
    SPRAWDZ(!parsujListeNocy("{\"days\":[{\"day\":20750},{\"day\":2075", dl) && dl.empty());
    SPRAWDZ(!parsujListeNocy("{\"days\":[{\"day\":20750},7]}", dl) && dl.empty());
    SPRAWDZ(!parsujListeNocy("{\"days\":{}}", dl) && !parsujListeNocy("{\"days\":[{\"day\":01}]}", dl));
    SPRAWDZ(!parsujListeNocy("{\"days\":[{\"day\":null}]}", dl) && !parsujListeNocy("[]", dl));
    // duża lista (2000 dób, jak po latach terapii): czytana w całości, w kolejności
    std::string L = "{\"source\":\"edf\",\"days\":[";
    for (int i = 0; i < 2000; i++)
      L += std::string(i ? "," : "") + "{\"day\":" + std::to_string(18000 + i) +
           ",\"session_count\":1,\"publication\":{\"day\":20261030,\"digest\":\"0123456789abcdef\",\"bytes\":123456},"
           "\"identity\":\"abcdef\",\"stale\":false}";
    L += "]}";
    SPRAWDZ(parsujListeNocy(L, dl) && dl.size() == 2000 && dl[1999].day == 19999 && dl[0].sesje == 1);
  }
  // bez reguł L, E-33, E-37, S-K-17 ocena warunku uspokojenia jest niepełna: bez zdań uspokajających
  {
    std::vector<Noc> h = histStany(dzis, 31);
    Stany st = liczStany(h, dzis, {});
    SPRAWDZ(st.uspokojenieOcenione && st.ocenaNiepelna && !st.uspokojenie);
    bool l = false, e33 = false;
    for (auto& x : st.nieoceniane) {
      l = l || x.find("wzorce L") != std::string::npos;
      e33 = e33 || x.find("E-37") != std::string::npos;  // E-33 i S-K-17 ocenia silnik, E-37 dalej nie
    }
    SPRAWDZ(l && e33);
    const Pula* pl = pula("dobra_noc");  // pula S2a ze zdaniami uspokajającymi (5.7.5)
    SPRAWDZ(pl != nullptr);
    int usp = 0;
    for (auto& z : zdaniaDoLosowania(*pl, true, true)) usp += uspokajajace(tekstZdania(z));
    SPRAWDZ(usp > 0);  // przy pełnej ocenie i spełnionym warunku są w puli
    for (auto& z : zdaniaDoLosowania(*pl, true, st.uspokojenie)) SPRAWDZ(!uspokajajace(tekstZdania(z)));
  }
  // liczStany na 2000 nocach: norma każdej nocy z okna 90 dni (wynik jak pełne liczenie normy)
  {
    std::vector<Noc> h;
    for (int i = 0; i < 2000; i++) {
      Noc x = noc(dodajDni(dzis, i - 1999), 380 + (i * 37) % 120, 1 + (i % 7) * 0.4f, 10.0f + (i % 11));
      x.wazna = i % 9 != 0;
      x.ep95 = 8 + (i % 5) * 0.3f;
      x.ustawienia = "ASVAuto/6/10/3/15";
      h.push_back(x);
    }
    Stany st = liczStany(h, dzis, {});
    Norma pelna = liczNorme(h, dzis, {}, &st.wyl);
    SPRAWDZ(st.norma.ahi.med == pelna.ahi.med && st.norma.wp95.p90 == pelna.wp95.p90 && st.norma.n == 30);
  }
  // S5 bez kodów technicznych; 401 i brak Wi-Fi = tekst E-01
  {
    DaneDnia dd;
    dd.dzis = dzis;
    for (int kod : {-1, -11, 401, 500, KOD_WIFI, AB_KOD_URWANE}) {
      dd.httpKod = kod;
      TrescEkranu t = trescDnia("S5", "", dd);
      ROWNE(t.tytul, "Noc 30.10 jeszcze w drodze.");
      SPRAWDZ(t.powod.find("kod") == std::string::npos && t.powod.find(std::to_string(kod)) == std::string::npos);
      SPRAWDZ(t.krok.empty());  // 1. noc bez danych: bez kroku (R-AW-32)
    }
    dd.bladOd = "05:30";
    ROWNE(trescDnia("S5", "", dd).powod, "AirBridge milczy od 05:30. Pytam co\xC2\xA0" "30\xC2\xA0" "min.");
    dd.ostatnieDane = "29.10 05:42";
    ROWNE(trescDnia("S5", "", dd).powod, "AirBridge milczy od 29.10 05:42. Pytam co\xC2\xA0" "30\xC2\xA0" "min.");
  }
  // EPAP ze STR w K1 z dopiskiem `(urządzenie)`, poza normą (R-NOR-08), w K2 `–`
  {
    std::vector<Noc> h = histStany(dodajDni(dzis, -1), 30);
    for (auto& x : h) x.zrodlo = 'A';
    Noc o = noc(dzis, 420, 1.0f, 10);
    o.zrodlo = 'A';
    o.emed = 7.4f;
    o.ep95 = 9.9f;
    o.epapStr = true;
    h.push_back(o);
    Stany st = liczStany(h, dzis, {});
    DaneK1 k1 = budujK1(h, dzis, st.norma, {}, &st);
    ROWNE(k1.liczby[2], "7,4");
    ROWNE(k1.opisy[2], "EPAP (urządzenie)");
    ROWNE(k1.opisy[3], "wyciek l/min");
    DaneK2 k2 = budujK2(h, dzis, st.norma, {}, &st);
    ROWNE(k2.w[7].k[6].tekst, "–");
    SPRAWDZ(!st.odchylenie.ep95);  // wartość urządzenia nie wchodzi do reguł
    std::vector<Noc> bez(h.begin(), h.end() - 1);
    SPRAWDZ(liczNorme(h, dodajDni(dzis, 1), {}).ep95.n == liczNorme(bez, dodajDni(dzis, 1), {}).ep95.n);
  }
}

// ---------- dalsze przypadki brzegowe (stale, e03, mism, rt, rt2) ----------
static void testPrzypadkiBrzegoweDalsze() {
  const std::string dzis = "2026-10-31";
  const long d = nrDnia("2026-10-20");
  // N-1, N-8 (stale.cpp): doba `stale` z wiecznym błędem albo timeoutem nie wraca bez końca: limit prób i czekanie
  // obowiązują także ją; po wyczerpaniu nie jest gotowa i nie wymusza listy
  {
    std::vector<Noc> h;
    Noc n;
    n.data = dataPoranka(d);
    n.zrodlo = 'A';
    n.cmed = 9;
    h.push_back(n);
    std::vector<DzienListy> L{{d, 1, true}};
    ProbyDob p;
    uint32_t t = 1793600000;
    for (int i = 0; i < 3; i++) {
      zapiszNieudanaProbe(p, d, t);
      t += AB_ODSTEP_DOBY_S + 1;
    }
    SPRAWDZ(dobaWyczerpana(p, d) && planPobrania(L, h, {}, dzis, false, 10, &p, t).empty());
    Zalegle z = policzZalegle(L, h, {}, dzis, p, t);
    SPRAWDZ(z.gotowe == 0 && z.czekajace == 0);
    SPRAWDZ(!potrzebnaLista(h, dzis, 12 * 60, false, z.gotowe, z.czekajDo, t, std::set<long>{nrDnia(dzis) - 1}));
    ProbyDob p2;  // tuż po nieudanej próbie: czeka 3 h, potem wraca raz
    zapiszNieudanaProbe(p2, d, t);
    SPRAWDZ(planPobrania(L, h, {}, dzis, false, 10, &p2, t + 60).empty());
    z = policzZalegle(L, h, {}, dzis, p2, t + 60);
    SPRAWDZ(z.czekajace == 1 && z.czekajDo == t + AB_ODSTEP_DOBY_S);  // stale z rekordem też czeka, z końcem czekania
    SPRAWDZ(planPobrania(L, h, {}, dzis, false, 10, &p2, t + AB_ODSTEP_DOBY_S).size() == 1);
  }
  // N-2 (e03.cpp): E-03 ostatniej nocy trafia na ekran (S5), także gdy noc jest „znana” z udanego statusu dziś
  {
    std::vector<Noc> h;
    WejscieDnia w;
    w.dzis = dzis;
    w.minuta = 7 * 60;
    w.hist = &h;
    w.odczytOk = false;
    w.abData = dzis;
    w.abMinuta = 7 * 60;
    w.kartaBraku = false;
    SPRAWDZ(!brakDanych(w));  // bez E-03 (np. Wi-Fi po udanym statusie rano): noc znana, bez S5
    w.e03 = true;
    SPRAWDZ(brakDanych(w));
    w.minuta = 12 * 60;
    SPRAWDZ(brakDanych(w));
    w.odczytOk = true;  // E-03 przy udanym statusie (wersja AirBridge spoza zakresu też)
    SPRAWDZ(brakDanych(w));
    h.push_back(noc(dzis, 420, 1.0f));  // zapisany rekord ostatniej nocy zostaje na ekranie (R-DANE-24 fail-path)
    SPRAWDZ(!brakDanych(w));
    DaneDnia dd;
    dd.dzis = dzis;
    dd.httpKod = KOD_E03;
    ROWNE(trescDnia("S5", "", dd).tytul, "Nie rozumiem danych AirBridge");
  }
  // mism.cpp: raport innej doby: ponowienie, po 3 próbach starsza pominięta, ostatnia E-03
  {
    NocAB n = nocZRaportu("{\"day\":20749,\"present\":true,\"partial\":false,\"used_seconds\":25000,\"ahi\":1,"
                          "\"events_known\":true}", 20750, "ASVAuto");
    SPRAWDZ(n.stan == StanRaportu::NIECZYTELNY && n.noc.data.empty());
    for (int pr = 1; pr <= 3; pr++) {
      SPRAWDZ(poRaporcie(n.stan, dataPoranka(20750), dzis, 600, pr) == (pr < 3 ? PoRaporcie::PONOW : PoRaporcie::POMIN));
      SPRAWDZ(poRaporcie(n.stan, dataPoranka(20750), dataPoranka(20750), 600, pr) ==
              (pr < 3 ? PoRaporcie::PONOW : PoRaporcie::E03));
    }
  }
  // N-4: przy nadpisaniu rekordu A pola uzupełnia tylko karta; z wcześniejszego A tylko PS (którego AirBridge nie ma)
  {
    const long dn = nrDnia("2026-10-29");
    Noc stary = nocZRaportu(raportNocy(dn), dn, "ASVAuto").noc;
    stary.psmed = 3.4f;  // PS dopisany wcześniej z karty
    std::vector<Noc> h = {stary};
    std::string r = raportNocy(dn);
    r.replace(r.find("\"unit\":\"L/min\","), 15, "");  // nowy raport bez jednostki wycieku: wyciek nieznany
    r.replace(r.find("\"used_seconds\":25320"), 20, "\"used_seconds\":26000");
    NocAB nowy = nocZRaportu(r, dn, "ASVAuto");
    SPRAWDZ(wpiszNocAB(h, nowy.noc, true, nowy.pelna));
    SPRAWDZ(std::isnan(h[0].wmed) && std::isnan(h[0].wp95) && h[0].m01 == 433);  // stary wyciek A nie wraca
    SPRAWDZ(std::fabs(h[0].psmed - 3.4f) < 1e-4);                               // PS zostaje
    Noc k = noc("2026-10-30", 420, 1.2f, 12);  // karta: wszystkie brakujące pola
    k.wmed = 3;
    std::vector<Noc> hk = {k};
    SPRAWDZ(wpiszNocAB(hk, nowy.noc, false, nowy.pelna) && std::fabs(hk[0].wmed - 3) < 1e-4 && hk[0].zrodlo == 'A');
  }
  // N-5 (rt.cpp): `;` i znaki sterujące nie trafiają do `/noce.csv`; zapis i odczyt dają ten sam wiersz
  {
    Noc n;
    n.data = "2026-10-01";
    n.ustawienia = "ASVAuto/6;7\r\n/\t//";
    std::string l = zapiszNoc(n);
    Noc q;
    SPRAWDZ(parsujNoc(l, 'A', q) && zapiszNoc(q) == l);
    ROWNE(q.ustawienia, "ASVAuto/67///");
    ROWNE(czystePole(std::string("a;b\x01\x7f" "c")), "abc");
    std::string r = raportNocy(nrDnia("2026-10-29"));
    r.replace(r.find("\"value\":\"10.0\""), 14, "\"value\":\"10;\\n0\"");
    SPRAWDZ(nocZRaportu(r, nrDnia("2026-10-29"), "").noc.ustawienia.find_first_of(";\n") == std::string::npos);
    // rt.cpp: 20 000 losowych rekordów, zapis → odczyt → zapis bez zmian (prosty generator, stałe ziarno)
    uint32_t z = 5;
    auto los = [&]() { z = z * 1103515245u + 12345u; return (z >> 8) & 0xFFFFFF; };
    auto f = [&]() { return los() % 4 == 0 ? NAN : (float)((int)(los() % 100000) / 100.0); };
    int zle2 = 0;
    for (int i = 0; i < 20000; i++) {
      Noc x;
      x.data = "2026-10-" + std::to_string(10 + los() % 20);
      x.wazna = los() % 2;
      x.m01 = los() % 5 == 0 ? -1 : (int)(los() % 700);
      for (float Noc::*p : {&Noc::ahi, &Noc::cmed, &Noc::cp95, &Noc::wmed, &Noc::wp95, &Noc::emed, &Noc::ep95,
                            &Noc::psmed, &Noc::psp95, &Noc::oa, &Noc::ca, &Noc::h, &Noc::csr})
        x.*p = f();
      x.ustawienia = los() % 3 ? "ASVAuto/6/10/3/15" : "";
      x.zrodlo = los() % 2 ? 'A' : 'K';
      x.czesciowa = los() % 2;
      x.epapStr = los() % 2;
      Noc y;
      std::string l2 = zapiszNoc(x);
      if (!parsujNoc(l2, 'A', y) || zapiszNoc(y) != l2) zle2++;
    }
    SPRAWDZ(zle2 == 0);
    // rt2.cpp: układy 9, 14, 20 pól czytane; 19 pól to błąd wiersza
    SPRAWDZ(parsujNoc("2026-10-01;1;400;1.20;9.80;13.60;2.40;9.60;A", 'K', q) && q.zrodlo == 'A');
    SPRAWDZ(parsujNoc("2026-10-01;1;400;1.20;9.80;13.60;2.40;9.60;6.40;8.40;3.40;6.10;ASVAuto/6/10/3/15;K", 'A', q));
    SPRAWDZ(parsujNoc("2026-10-01;1;400;1.20;9.80;13.60;2.40;9.60;6.40;8.40;3.40;6.10;ASVAuto/6/10/3/15;A;1;0;0.5;1.2;0.3;4",
                      'K', q) && q.czesciowa && std::fabs(q.oa - 0.5f) < 1e-6);
    SPRAWDZ(!parsujNoc("2026-10-01;1;400;1.20;9.80;13.60;2.40;9.60;6.40;8.40;3.40;6.10;ASVAuto/6/10/3/15;A;1;0;0.5;1.2;0.3",
                       'K', q));
  }
  // N-7: przerwanie pobierania przyciskiem dopiero po puszczeniu przycisku, który je uruchomił (↻ na S5)
  {
    PrzerwaniePrzyciskiem p;
    SPRAWDZ(!p.krok(true) && !p.krok(true));  // ↻ jeszcze trzymany: bez przerwania
    SPRAWDZ(!p.krok(false));                  // puszczony
    SPRAWDZ(p.krok(true));                    // nowe naciśnięcie: przerwać
    PrzerwaniePrzyciskiem q;                  // partie historii: przycisk puszczony od początku
    SPRAWDZ(!q.krok(false) && q.krok(true));
  }
}

// ---------- skala B (wariant B ekranów dnia i K2 opcja A) ----------
// Szerokości w px z krojów (szer()), reguły ustępowania i rysowanie sprawdzane na płótnie w pamięci.
struct PlotnoTest : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[y * 800 + x] = c;
  }
  bool cz(int x, int y) const { return px[y * 800 + x]; }
  int czarne(int x0, int y0, int x1, int y1) const {  // [x0, x1) × [y0, y1)
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[y * 800 + x];
    return n;
  }
};

// wymaganie: T-WYM-39, R-KONF-41, T-WYM-83
// U-05 narysowany według kanwy
// design/kanwa/U05-test-sprzetu.dc.html. Kreski w pozycjach z kanwy co do piksela, a obwiednie tekstów z podglądu
// design/png/U05-test-sprzetu.png z tolerancją rasteryzacji kroju: ±1 px w pionie, ±6 px
// w poziomie (strzałka `→` nagłówka z innego kroju w przeglądarce).
struct Obwiednia { int x0, y0, x1, y1; };
static Obwiednia obwiednia(const PlotnoTest& p, int x0, int y0, int x1, int y1) {  // [x0, x1) × [y0, y1)
  Obwiednia o{9999, 9999, -1, -1};
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++)
      if (p.cz(x, y)) { o.x0 = std::min(o.x0, x); o.y0 = std::min(o.y0, y); o.x1 = std::max(o.x1, x); o.y1 = std::max(o.y1, y); }
  return o;
}
static bool blisko(const Obwiednia& a, const Obwiednia& k) {
  bool ok = std::abs(a.x0 - k.x0) <= 6 && std::abs(a.x1 - k.x1) <= 6 && std::abs(a.y0 - k.y0) <= 1 && std::abs(a.y1 - k.y1) <= 1;
  if (!ok) printf("  U-05: obwiednia (%d,%d,%d,%d), kanwa (%d,%d,%d,%d)\n", a.x0, a.y0, a.x1, a.y1, k.x0, k.y0, k.x1, k.y1);
  return ok;
}
static bool wierszCzarny(const PlotnoTest& p, int y, int x0, int x1) {  // [x0, x1]
  for (int x = x0; x <= x1; x++) if (!p.cz(x, y)) return false;
  return true;
}
static void testU05WedlugKanwy() {
  DaneTestu d;  // wartości z kanwy
  d.lewa = {{"Oprogramowanie", "1.0.1 (app1)"}, {"Zestaw reguł", "2026-12-01"}, {"Pamięć panelu", "wolne 18 MB"},
            {"Karta", "kopia 03.11 05:40"}, {"Źródło otoczenia", "OK 05:31"}};
  d.prawa = {{"Panel", "192.168.30.51"}, {"Wi-Fi", "Dom −61 dBm"}, {"AirBridge", "192.168.20.205 · v2.3.0"},
             {"Ostatnie pobranie", "03.11 05:32"}, {"Bateria", "64%"}};
  d.ramkaLewo = "Wgrywanie przez Wi-Fi do 10:14";
  d.ramkaPrawo = "hasła i dane terapii nie są pokazywane";
  PlotnoTest p;
  rysujTest(p, d);
  // kreski: nagłówek 2 px na 48, nazwy kolumn 2 px na 89, 5 wierszy po 1 px co 47 od 137, ramka 2 px 26..773 × 378..421,
  // stopka 2 px na 436
  SPRAWDZ(wierszCzarny(p, 48, 0, 799) && wierszCzarny(p, 49, 0, 799) && !p.cz(400, 47) && !p.cz(400, 50));
  SPRAWDZ(wierszCzarny(p, 436, 0, 799) && wierszCzarny(p, 437, 0, 799) && !p.cz(400, 435) && !p.cz(400, 438));
  for (int x0 : {26, 414}) {
    SPRAWDZ(wierszCzarny(p, 89, x0, x0 + 359) && wierszCzarny(p, 90, x0, x0 + 359) && !p.cz(x0 - 1, 89) && !p.cz(x0 + 360, 89));
    for (int i = 0; i < 5; i++) {
      int y = 137 + 47 * i;
      SPRAWDZ(wierszCzarny(p, y, x0, x0 + 359) && !p.cz(x0 + 200, y - 1) && !p.cz(x0 + 200, y + 1));
    }
  }
  SPRAWDZ(p.czarne(386, 89, 414, 326) == 0);  // odstęp 28 px między kolumnami
  for (int y : {378, 379, 420, 421}) SPRAWDZ(wierszCzarny(p, y, 26, 773));
  for (int y = 378; y <= 421; y++) SPRAWDZ(p.cz(26, y) && p.cz(27, y) && p.cz(772, y) && p.cz(773, y));
  SPRAWDZ(!p.cz(28, 400) && !p.cz(771, 400) && !p.cz(25, 400) && !p.cz(774, 400));
  SPRAWDZ(p.czarne(0, 326, 800, 378) == 0);  // pod ostatnim wierszem do ramki pusto (kanwa: ramka na dole)
  SPRAWDZ(p.czarne(0, 422, 800, 436) == 0 && p.czarne(0, 0, 800, 12) == 0);
  // teksty: obwiednie z podglądu kanwy
  SPRAWDZ(blisko(obwiednia(p, 0, 0, 400, 48), {26, 17, 140, 33}));     // `Test sprzętu`
  SPRAWDZ(blisko(obwiednia(p, 400, 0, 800, 48), {530, 17, 772, 33}));  // `Ustawienia → Test sprzętu`
  SPRAWDZ(blisko(obwiednia(p, 26, 52, 386, 89), {27, 64, 274, 81}));   // `OPROGRAMOWANIE I PAMIĘĆ`
  SPRAWDZ(blisko(obwiednia(p, 414, 52, 774, 89), {414, 64, 596, 81})); // `ŁĄCZNOŚĆ I BATERIA`
  const Obwiednia kan[5][4] = {
      {{27, 106, 183, 123}, {287, 106, 383, 123}, {416, 106, 465, 120}, {645, 106, 771, 120}},
      {{27, 153, 146, 170}, {281, 153, 383, 167}, {415, 154, 460, 167}, {648, 153, 772, 167}},
      {{28, 200, 159, 217}, {268, 200, 384, 214}, {415, 200, 559, 217}, {560, 200, 772, 214}},
      {{28, 248, 74, 261}, {224, 247, 384, 264}, {415, 247, 559, 264}, {560, 247, 772, 261}},
      {{27, 291, 179, 308}, {301, 294, 383, 308}, {416, 295, 478, 308}, {731, 294, 772, 308}}};
  for (int i = 0; i < 5; i++) {
    int y0 = 91 + 47 * i;
    SPRAWDZ(blisko(obwiednia(p, 26, y0, 200, y0 + 46), kan[i][0]));
    SPRAWDZ(blisko(obwiednia(p, 200, y0, 386, y0 + 46), kan[i][1]));
    SPRAWDZ(blisko(obwiednia(p, 414, y0, 560, y0 + 46), kan[i][2]));
    SPRAWDZ(blisko(obwiednia(p, 560, y0, 774, y0 + 46), kan[i][3]));
  }
  SPRAWDZ(blisko(obwiednia(p, 28, 380, 380, 420), {43, 392, 335, 409}));   // okno serwisowe
  SPRAWDZ(blisko(obwiednia(p, 380, 380, 772, 420), {406, 392, 756, 409})); // `hasła i dane terapii nie są pokazywane`
  // kolejność jak przyciski na obudowie, `◀ wróć` z lewej, `↻ odśwież · 2 s pakiet z karty` z prawej (kanwa miała odwrotnie)
  // `◀ wróć · 2 s sieć panelu` (było `◀ wróć` do 79 px)
  // trzy grupy od lewej `◀ ustawienia · 2 s sieć panelu`, `▶ odśwież`, `↻ ekran dnia`, całość do 780 px
  {
    Obwiednia o = obwiednia(p, 0, 438, 800, 480);
    SPRAWDZ(o.x0 == 20 && o.x1 <= 780 && o.y0 >= 448 && o.y1 <= 468);
  }
  SPRAWDZ(!p.cz(28, 470));  // ikony bez kwadratu (kanwa: znaki kroju, nie przyciski w ramce)

  // wiersz `Źródło otoczenia` (R-AW-25, R-NF-27): krótkie wartości stanu źródła, pełny opis w logu
  {
    Otoczenie o;
    wyczyscOtoczenie(o, false);
    ROWNE(wartosciTestuOtoczenia(o, "").front(), "wyłączone");
    wyczyscOtoczenie(o, true);
    ROWNE(wartosciTestuOtoczenia(o, "").front(), "jeszcze bez odczytu");
    nieudanaOtoczenia(o, 401, 1793395800u);
    ROWNE(wartosciTestuOtoczenia(o, "05:16").front(), "HTTP 401");
    o.stan = (int8_t)StanOto::POZA_SIECIA;
    ROWNE(dopasujWartosc("Źródło otoczenia", wartosciTestuOtoczenia(o, "")), "poza siecią domową");
    o.stan = (int8_t)StanOto::PRZEKIEROWANIE;
    ROWNE(dopasujWartosc("Źródło otoczenia", wartosciTestuOtoczenia(o, "")), "przekierowanie");
    o.stan = (int8_t)StanOto::OK;
    ROWNE(dopasujWartosc("Źródło otoczenia", wartosciTestuOtoczenia(o, "05:31")), "OK 05:31");
    o.stan = (int8_t)StanOto::BRAK_ODPOWIEDZI;
    ROWNE(wartosciTestuOtoczenia(o, "").front(), "brak odpowiedzi");
    o.stan = (int8_t)StanOto::ZLA_WERSJA;
    o.wersja = 2;
    ROWNE(wartosciTestuOtoczenia(o, "").front(), "wersja kontraktu 2");
    o.wersja = -1;
    ROWNE(wartosciTestuOtoczenia(o, "").front(), "brak wersji kontraktu");
    o.stan = (int8_t)StanOto::NIECZYTELNE;
    ROWNE(dopasujWartosc("Źródło otoczenia", wartosciTestuOtoczenia(o, "")), "nieczytelna");
    o.stan = (int8_t)StanOto::ZA_DUZE;
    ROWNE(dopasujWartosc("Źródło otoczenia", wartosciTestuOtoczenia(o, "")), "odpowiedź za duża");
    o.stan = 99;  // stan spoza listy (np. RTC z innej wersji): `–`
    ROWNE(wartosciTestuOtoczenia(o, "").front(), "–");
  }
  // wartości za długie na kolumnę: pierwszy wariant, który się mieści, a bez takiego skrót z `…`
  SPRAWDZ(miejsceNaWartosc("Pamięć panelu") == 360 - szer(F_P20, "Pamięć panelu") - 12);
  ROWNE(dopasujWartosc("Pamięć panelu", {"wolne 18 MB"}), "wolne 18 MB");
  ROWNE(dopasujWartosc("Pamięć panelu", {"przywrócono 412 z 412", "przywrócono 412/412"}), "przywrócono 412/412");
  ROWNE(dopasujWartosc("Źródło otoczenia", {"adres poza siecią domową", "poza siecią domową"}), "poza siecią domową");
  std::string w = dopasujWartosc("Wi-Fi", {"Bardzo-Dluga-Nazwa-Sieci-Domowej-Ponad-Kolumne −90 dBm"});
  SPRAWDZ(w.size() > 3 && w.compare(w.size() - 3, 3, "…") == 0 && szer(F_P20B, w) <= miejsceNaWartosc("Wi-Fi"));
  // każda wartość z wariantów firmware mieści się obok nazwy (najdłuższe wartości z liczbami i adresami)
  struct { const char* n; const char* w; } dl[] = {
      {"Oprogramowanie", "10.10.10 (app1)"}, {"Pamięć panelu", "nie zamontowana"}, {"Pamięć panelu", "układ: wgraj kablem"},
      {"Pamięć panelu", "niezgodna liczba nocy"}, {"Pamięć panelu", "migracja noce: błąd"},
      {"Pamięć panelu", "dane z nowszej wersji"}, {"Pamięć panelu", "wolne 999 MB"}, {"Karta", "brak karty"},
      {"Źródło otoczenia", "jeszcze bez odczytu"}, {"Źródło otoczenia", "OK 30.12 23:59"},
      {"Źródło otoczenia", "odpowiedź za duża"}, {"Źródło otoczenia", "wersja kontraktu 99"},
      {"Panel", "192.168.100.200 (30.12 23:59)"}, {"Panel", "brak połączenia z Wi-Fi"},
      {"AirBridge", "192.168.100.200 · v12.10.10"}, {"AirBridge", "brak odp. od 30.12 23:59"},
      {"AirBridge", "192.168.20.205 · bez wersji"}, {"Ostatnie pobranie", "30.12 23:59"}, {"Tryb awaryjny", "wybudzenie z uśpienia"}};
  for (auto& e : dl) {
    if (szer(F_P20B, e.w) > miejsceNaWartosc(e.n)) printf("  U-05: za długa wartość %s: %s\n", e.n, e.w);
    SPRAWDZ(szer(F_P20B, e.w) <= miejsceNaWartosc(e.n));
  }
  // ramka: najdłuższe lewe części obok prawej (716 px wnętrza: 748 − 2 × 2 − 2 × 14, odstęp co najmniej 12 px)
  for (const char* l : {"Wgrywanie przez Wi-Fi: wyłączone", "Wgrywanie przez Wi-Fi: brak sieci", "Wgrywanie przez Wi-Fi: zamknięte",
                        "Wgrywanie przez Wi-Fi: 10 min", "Okno przywracania do 23:59", "Okno przywracania: 10 min"})
    SPRAWDZ(szer(F_P20B, l) + 12 + szer(F_P20, "hasła i dane terapii nie są pokazywane") <= 716);

  // tryb awaryjny (T-WYM-83): jedna kolumna z 2 wierszami, prawa pusta, stopka tylko `◀ wróć`
  DaneTestu a;
  a.awaryjny = true;
  a.lewa = {{"Oprogramowanie", "1.0.0 (app1)"}, {"Tryb awaryjny", "błąd programu"}};
  a.ramkaLewo = "Wgrywanie przez Wi-Fi do 10:14";
  a.ramkaPrawo = "hasła i dane terapii nie są pokazywane";
  PlotnoTest q;
  rysujTest(q, a);
  SPRAWDZ(wierszCzarny(q, 89, 26, 385) && wierszCzarny(q, 137, 26, 385) && wierszCzarny(q, 184, 26, 385));
  SPRAWDZ(!q.cz(200, 231) && q.czarne(386, 52, 800, 378) == 0);  // bez 3. wiersza i bez prawej kolumny
  SPRAWDZ(q.czarne(0, 438, 400, 480) == 0 && blisko(obwiednia(q, 400, 438, 800, 480), {719, 451, 778, 467}));
  SPRAWDZ(blisko(obwiednia(q, 28, 380, 380, 420), {43, 392, 335, 409}));
}

static void testSkalaB() {
  // reguła ustępowania 2 (SPEC-B 5): linia uwagi przy obecnej stopce chowa rząd słów pod wartościami
  TrescEkranu t;
  t.uklad = Uklad::TYDZIEN;
  t.podpisNocy = "Ostatnia noc";
  t.czas = "12 h 59";
  t.ahi = "12,4";
  t.powod = "Tydzień: różnice zaznaczone niżej.";
  t.tytulPaska = "Ostatnie 7 nocy · mediana";
  Kolumna k;
  k.et = "czas z maską";
  k.w = "12:59";
  k.s = "↑";
  k.p = "lepiej niż zwykle";
  t.kolumny = {k, k, k, k};
  t.stopkaPrawo = "przy łóżku 21° · 45%";
  SPRAWDZ(slowaWPasku(t) && gornaPaska(t) == 280 && wysokoscPaska(t) == 142);  // bez linii pasek od 280, słowa na 412
  PlotnoTest p;
  rysujEkranDanych(p, {"cz 30.11", "dane 23:59"}, t);
  SPRAWDZ(p.czarne(28, 398, 200, 414) > 50);  // rząd słów jest
  // reguła ustępowania 2: przy linii uwagi i stopce słowa zostają w kolumnach innych niż `jak zwykle`;
  // pasek się zagęszcza (128 px, od 294); zdanie (dół 219 + 58 = 277 przy linii 48 px) nad paskiem
  t.linia = "Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij pomoc.";
  t.liniaOstrzezenie = true;
  SPRAWDZ(slowaWPasku(t) && gornaPaska(t) == 294 && wysokoscPaska(t) == 128);
  PlotnoTest q;
  rysujEkranDanych(q, {"cz 30.11", "dane 23:59"}, t);
  SPRAWDZ(q.czarne(28, 404, 200, 420) > 50);  // rząd słów jest (słowo na 294 + 119 = 413)
  SPRAWDZ(q.czarne(28, 424, 200, 434) == 0);  // nic nie wchodzi między pasek a linię stopki
  SPRAWDZ(q.czarne(28, 370, 200, 392) > 50);  // wartości (B5 na 294 + 94 = 388)
  t.liniaA = true;  // linia wzorca A 60 px: zdanie kończy się na 231 + 58 = 289, pasek od 294
  SPRAWDZ(slowaWPasku(t) && gornaPaska(t) == 294 && 54 + 60 + 117 + 50 + 8 <= gornaPaska(t));
  t.liniaA = false;
  // tylko gdy w żadnej kolumnie nie ma słowa (same `jak zwykle`), pasek traci rząd słów (114 px, od 308)
  TrescEkranu bez = t;
  for (auto& kk : bez.kolumny) { kk.p.clear(); kk.s = "↔"; }
  SPRAWDZ(!slowaWPasku(bez) && gornaPaska(bez) == 308);
  // słowo przy kolumnie poza normą (ramka) zostaje także z linią uwagi: rysowane w ramce, czarne na białym
  TrescEkranu zr = t;
  zr.kolumny[1].p = "wyżej niż zwykle";
  zr.kolumny[1].s = "↑";
  zr.kolumny[1].poza = true;
  PlotnoTest qz;
  rysujEkranDanych(qz, {"cz 30.11", "dane 23:59"}, zr);
  int kx = 28 + 186;
  SPRAWDZ(qz.czarne(kx, 400, kx + szer(F_P22B, "wyżej niż zwykle"), 418) > 50);  // słowo jest
  SPRAWDZ(qz.cz(kx - 5, 360) && qz.cz(kx - 4, 360) && !qz.cz(kx - 3, 360) && !qz.cz(kx - 6, 360));  // lewa krawędź 2 px
  SPRAWDZ(qz.cz(kx + 20, 423) && !qz.cz(kx + 20, 421) && !qz.cz(kx + 20, 300));  // dół ramki 422–423, środek biały
  SPRAWDZ(kx + szer(F_P22B, "wyżej niż zwykle") < kx - 5 + 184 - 2);              // słowo nie dotyka prawej krawędzi
  SPRAWDZ(qz.czarne(0, 424, 800, 434) == 0);                                     // między ramką a stopką pusto
  printf("  kolumna w ramce: %d czarnych px\n", qz.czarne(kx, 330, kx + 170, 418));
  SPRAWDZ(qz.czarne(kx, 330, kx + 170, 418) < 5000);  // nie pole czarne (negatyw: ok. 15 000 px z 14 960)
  t.stopkaPrawo.clear();  // reguła 0: bez stopki strefa do 466, słowa wracają
  SPRAWDZ(slowaWPasku(t) && gornaPaska(t) == 324);
  PlotnoTest r;
  rysujEkranDanych(r, {"cz 30.11", "dane 23:59"}, t);
  SPRAWDZ(r.czarne(0, 434, 800, 436) == 0);  // bez linii stopki

  // najdłuższe teksty mieszczą się w strefach (px z krojów, SPEC-B 6, SPEC-KOKPIT 2–7)
  SPRAWDZ(28 + szer(F_P24B, "!") + 12 + szer(F_P24B, "Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij pomoc.") <= 772);
  // R-EKR-42: najdłuższa linia R-LEK-24 w jednym wierszu na 760 px (z `!`), na ekranie w strefie do 772
  SPRAWDZ(szer(F_P28B, "!") + 12 + szer(F_P28B, "Wciąż: Ciśnienie przy górnej granicy. Omówione 30.11.") <= 760);
  SPRAWDZ(28 + szer(F_P28B, "!") + 12 + szer(F_P28B, "Wciąż: Ciśnienie przy górnej granicy. Omówione 30.11.") <= 772);
  int x2 = std::max(328, 28 + szer(F_SN96, "12 h 59") + 56);
  SPRAWDZ(x2 + szer(F_SN96, "12,4") <= 772 && x2 + szer(F_P34B, "nie oceniam:") <= 772 && x2 + szer(F_P34B, "duży wyciek") <= 772);
  SPRAWDZ(28 + szer(F_SN96, "–") + 56 <= 328);  // brak czasu: AHI na 328
  // przedrostek dosłownie ze spec (5.4.2, R-EKR-41) `Noc w normie. `
  for (const char* z : {"Noc w normie. Tydzień: różnice niżej.", "Noc w normie. Przy łóżku: różnice niżej.",
                        "Noc w normie. Sypialnia: różnice niżej.",
                        "Noc w normie. Dobre warunki do snu."})
    SPRAWDZ(szer(F_P34B, z) <= 744 - 20);  // zapas co najmniej 20 px
  // nagłówek ekranów dnia i kart: `{dzień} {data}` i najdłuższy stan danych
  SPRAWDZ(szer(F_P24B, "cz 30.11") + szer(F_P24B, "dane 23:59 · ocena ograniczona") + 24 <= 744);
  SPRAWDZ(szer(F_P22B, wersaliki("Przy łóżku · ostatnia noc"), 2) + szer(F_P22B, "↔ jak zwykle") + 24 <= 744);
  ROWNE(wersaliki("Przy łóżku · ostatnia noc ąćęńśźż"), "PRZY ŁÓŻKU · OSTATNIA NOC ĄĆĘŃŚŹŻ");
  SPRAWDZ(28 + szer(F_S120, "1000") + 20 + szer(F_P34B, "nocy z rzędu") <= 772);  // S2b, seria 4-cyfrowa
  SPRAWDZ(szer(F_SB44, "Synchronizuję się z aparatem.") <= 728 && szer(F_SB44, "Noc 30.11 jeszcze w drodze.") <= 728);
  SPRAWDZ(28 + szer(F_SN96, "365") + 20 + szer(F_P34B, "nocy z maską") <= 772);  // kamień
  SPRAWDZ(szer(F_P24B, "noc 13 z 14") + szer(F_P24B, "pełna norma od ok. 30.11") + 24 <= 744);  // S6
  SPRAWDZ(szer(F_SB44, "Nie rozumiem danych AirBridge") <= 728 && szer(F_SB44, "Brak danych z nocy 30.11") <= 728);  // S5 w ramce
  SPRAWDZ(szer(F_P28B, "→ Sprawdź AirBridge i sieć. Ustawienia: strona 6 / 6.") <= 728);
  // reguła 4: powód S5 zawija się najwyżej na 2 linie, `co 30 min.` się nie rozdziela (R-EKR-08)
  for (const char* od : {"29.10 05:42", "05:42"}) {
    DaneDnia dd;
    dd.dzis = "2026-10-30";
    dd.ostatnieDane = od;
    auto l = zawin(F_P28B, trescDnia("S5", "", dd).powod, 728);
    SPRAWDZ(l.size() <= 2 && l.back().find("co\xC2\xA0" "30") != std::string::npos);
  }
  SPRAWDZ(zawin(F_P28B, "Wyjazd z aparatem? Dane wrócą same. Sprawdzam co\xC2\xA0" "3\xC2\xA0" "h.", 728).size() <= 2);  //
  SPRAWDZ(zawin(F_P28B, "Ostatnia noc: 12 h 59 z maską, wyciek 112 l/min.", 744).size() <= 2);  // S6, R-EKR-45, bez P95
  // stopki (wzór 2.1): grupy w jednej linii 744 px
  // (stopka U-05 ma krój P18B i własny układ według kanwy: testU05WedlugKanwy)
  SPRAWDZ(szerStopki(stopkaK2()) <= 744 && szerStopki(stopkaK1(true)) <= 744 && szerStopki(stopkaPytania()) <= 744);
  SPRAWDZ(szerStopki({"◀ ostatnia noc · 2 s ustawienia", "↻ ekran dnia · 2 s sprzęt", "▶ raport"}) <= 744);  // pełna mapa K2
  // stopka karty bez zdania budzika (od 10.11.2026, ADR-0009 „Zmiana 2026-11-10”): lewo jak na ekranie danych
  // (pogoda albo pusto), prawo bez zmian; najdłuższy odczyt czujnika stoi w całości, lewa część stopki pusta
  {
    TrescEkranu kt;
    kt.tytul = "Noc: 6 h 38 z maską.";
    kt.stopkaPrawo = "przy łóżku 35° · 100%";
    PlotnoTest kp;
    rysujKarte(kp, {"cz 30.11", "dane 05:46"}, kt, "");
    int x0 = 772 - szer(F_P22B, kt.stopkaPrawo);
    SPRAWDZ(kp.czarne(x0, 444, 772, 472) > 50);   // odczyt czujnika w całości
    SPRAWDZ(kp.czarne(0, 440, x0 - 4, 480) == 0);  // bez zdania budzika z lewej
    // bez obu stron stopka znika (5.4.3): bez kreski na 434
    TrescEkranu pusta;
    pusta.tytul = kt.tytul;
    PlotnoTest kq;
    rysujKarte(kq, {"cz 30.11", "dane 05:46"}, pusta, "");
    SPRAWDZ(kq.czarne(0, 433, 800, 480) == 0);
  }
  // S9 A1: fakty i krok w kolumnie tekstu 474 px; A3: linia 2 obok budzika (≤ 500)
  SPRAWDZ(szer(F_P22B, "temperatura") + 22 + szer(F_P22B, "wilgotność") + 22 + szer(F_P28B, "10 h 59") <= 474);
  SPRAWDZ(szer(F_P22B, "Ekran zostaje do rana, żeby nie błyskać.") <= 474);
  SPRAWDZ(szer(F_P22B, "Ekran zostaje do rana, żeby nie błyskać.") <= 500 && szer(F_P28B, "Dobranoc. Wynik nocy rano.") <= 500);
  // pytanie: opis nocy obok etykiety, etykieta leków w 2 liniach lewej kolumny (376 − 58)
  SPRAWDZ(28 + szer(F_P22B, "INNA NOC", 1) + 32 + szer(F_P22B, "AHI 12,4 przy zwykłych 10,5–12,3, maska zdjęta 12 razy") <= 772);
  SPRAWDZ(zawin(F_P22B, "leki nasenne, uspokajające albo silne przeciwbólowe", 376 - 58).size() == 2);
  // ekran testu zastąpiony przez U-05 według kanwy: testU05WedlugKanwy
  // opis nocy na pytaniu pod etykietą, na 744 px, najwyżej 2 linie; najgorsze pary metryk
  for (const char* o : {"AHI 112,5 przy zwykłych 10,5–12,3, maska 12:59 przy zwykłych 10:48–11:59",
                        "AHI 112,5 przy zwykłych 10,5–12,3, wyciek 112 l/min przy zwykłych 100–112",
                        "maska 12:59 przy zwykłych 10:48–11:59, wyciek 112 l/min przy zwykłych 100–112",
                        "AHI 12,4 przy zwykłych 10,5–12,3, maska zdjęta 12 razy"})
    SPRAWDZ(zawin(F_P22B, o, 744).size() <= 2);
  {
    DanePytania q;
    q.dataPoranka = "2026-11-30";
    q.innaNoc = true;
    q.opisNocy = "AHI 112,5 przy zwykłych 10,5–12,3, maska 12:59 przy zwykłych 10:48–11:59";
    q.zaznaczone = 0xFF;
    q.kursor = 7;
    q.termin = 23 * 60 + 30;
    PlotnoTest pq;
    rysujPytanie(pq, q);
    SPRAWDZ(pq.czarne(773, 48, 800, 433) == 0);  // nic poza prawym marginesem
    SPRAWDZ(pq.czarne(0, 424, 800, 434) == 0);   // siatka kończy się nad stopką
    SPRAWDZ(28 + szer(F_P22B, "INNA NOC", 1) + 18 + 24 + szer(F_P22B, "Bez odpowiedzi pytanie zniknie o 23:30.") <= 772);
  }
  // P3: powód szablonu A najwyżej 2 linie (inaczej jawne `…`), najdłuższe powody z logiki
  {
    DaneDnia dd;
    dd.dzis = "2026-11-30";
    dd.httpKod = KOD_E03;
    dd.abWersja = "2.10.13-beta.4+g1a2b3c4";
    dd.ostatnieDane = "28.11 23:59";
    SPRAWDZ(zawin(F_P28B, trescDnia("S5", "1", dd).powod, 728).size() <= 2);
    SPRAWDZ(zawin(F_P28B, "Pierwsza noc: 30.10.2026. Każda się liczy.", 744).size() == 1);
    TrescEkranu dl;
    dl.etykieta = "TEST";
    dl.tytul = "Tytuł";
    dl.powod = "Bardzo długi powód, który na pewno nie mieści się w dwóch liniach tekstu, bo ma dużo słów i jeszcze "
               "więcej słów, a potem jeszcze kilka słów na koniec.";
    PlotnoTest po;
    rysujEkranDanych(po, {"cz 30.11", ""}, dl);
    SPRAWDZ(po.czarne(0, 54 + 40 + 51 + 14 + 3 * 36 - 20, 800, 54 + 40 + 51 + 14 + 3 * 36 + 8) == 0);  // bez 3. linii
  }

  // strzałki ↔ ↑ ↓ jako ikony (SPEC-B 1): szerokość z wysokości wersalika kroju, nie z glifu Plexa; grubość px/11
  SPRAWDZ(szer(F_P22B, "↔") == 19 + 4 && szer(F_P28B, "↔") == 25 + 4);  // round(0,7 × px × 1,25) + 4
  SPRAWDZ(szer(F_P28B, "↑") == 15 + 4 && szer(F_P28B, "↓") == 15 + 4);  // round(0,7 × 28 × 0,75) + 4
  PlotnoTest a;
  tekst(a, F_P28B, 10, 40, "↑");  // c = 20, w = 15, grot h = 8, oś x = 18, grubość 3
  SPRAWDZ(a.cz(17, 32) && a.cz(18, 32) && a.cz(19, 32) && !a.cz(15, 32) && !a.cz(21, 32));  // trzon 3 px
  SPRAWDZ(a.cz(18, 21) && a.cz(18, 39) && !a.cz(18, 18));  // grot u góry (od 20), trzon do linii bazowej
  PlotnoTest b;
  tekst(b, F_P28B, 10, 40, "↓");
  SPRAWDZ(b.cz(18, 39) && !b.cz(10, 22) && b.cz(18, 21));
  PlotnoTest c;
  tekst(c, F_P22B, 10, 40, "↔");  // c = 15, środek y = 40 − 8 = 32, grubość 2
  SPRAWDZ(c.cz(19, 31) && c.cz(19, 32) && !c.cz(19, 29) && !c.cz(19, 35) && c.cz(10, 32) && c.cz(29, 32));

  // ---------- przegląd czytelności ekranów (jedna nazwa, jedna legenda, reguły ustępowania) ----------
  {
    // pkt 1: jedna nazwa M-02 `AHI` w K1 (liczba i wiersz), K2 i pasku S2a
    std::vector<Noc> ha;
    for (int i = 0; i < 40; i++) ha.push_back(noc(dodajDni("2026-10-29", -i), 400 + i % 7 * 9, 1.0f + (i % 5) * 0.4f, 8 + i % 6));
    Norma na = liczNorme(ha, "2026-10-29", {});
    DaneK1 a1 = budujK1(ha, "2026-10-29", na, {});
    DaneK2 a2 = budujK2(ha, "2026-10-29", na, {});
    ROWNE(a1.opisy[1], "AHI");
    ROWNE(std::string(a1.w[0].nazwa), "AHI");
    ROWNE(std::string(a2.w[0].etykieta), "AHI");
    DaneDnia da;
    da.dzis = "2026-10-29";
    da.hist = &ha;
    da.norma = na;
    ROWNE(trescDnia("S2A", "1", da).kolumny[1].et, "AHI");
    // pkt 2: K1 wiersz w stanie odchylenia w ramce 2 px (nie negatyw), K2 etykieta w ramce, `ponad próg` czarne
    DaneK1 s1 = a1;
    s1.w[0].stan = true;
    s1.w[0].slowo = "wyżej niż zwykle";
    s1.w[0].slowo2 = "od 9 nocy";
    PlotnoTest k1p;
    rysujK1(k1p, s1);
    SPRAWDZ(k1p.cz(12, 210) && k1p.cz(13, 210) && !k1p.cz(14, 210) && k1p.cz(400, 181) && !k1p.cz(450, 200));
    printf("  K1 wiersz w ramce: %d px\n", k1p.czarne(12, 188, 788, 235));
    SPRAWDZ(k1p.czarne(12, 188, 788, 235) < 12000);  // negatyw miałby ok. 35 000 px
    DaneK2 s2 = a2;
    s2.w[0].stan = true;
    s2.w[0].k[3].ponadProg = true;
    PlotnoTest k2p;
    rysujK2(k2p, s2);
    SPRAWDZ(k2p.cz(20, 132) && !k2p.cz(23, 132));                     // etykieta `AHI` w ramce 2 px, środek biały
    SPRAWDZ(k2p.czarne(168 + 3 * 68 + 4, 122, 168 + 4 * 68 - 2, 146) > 600);  // komórka ponad próg: czarne pole
    // nagłówek K2 jak K1: `Ostatnie 7 nocy · {zakres}` i `2 / 2`
    SPRAWDZ(k2p.czarne(772 - szer(F_P22B, "2 / 2"), 15, 773, 36) > 30);
    // pkt 3: zaproszenie = biały pasek w ramce bez `!`; linia zdrowia = czarny pasek z `!`
    TrescEkranu z;
    z.uklad = Uklad::TYDZIEN;
    z.podpisNocy = "Ostatnia noc";
    z.czas = "6 h 38";
    z.ahi = "4,1";
    z.kolumny = trescDnia("S2A", "1", da).kolumny;
    z.linia = "Co było inaczej? ↻ 2 s";
    PlotnoTest zp;
    rysujEkranDanych(zp, {"wt 27.10", "dane 05:38"}, z);
    SPRAWDZ(zp.cz(12, 80) && zp.cz(13, 80) && !zp.cz(14, 80) && zp.cz(400, 57) && !zp.cz(400, 70));  // ramka 2 px
    printf("  linia zaproszenia: %d px\n", zp.czarne(0, 54, 800, 102));
    SPRAWDZ(zp.czarne(0, 54, 800, 102) < 8000);  // nie czarny pasek (pasek: 38 400 px)
    SPRAWDZ(zp.czarne(28, 66, 28 + szer(F_P24B, "C"), 88) > 20);  // tekst od 28 px: bez `!` przed nim
    z.liniaOstrzezenie = true;
    PlotnoTest zo;
    rysujEkranDanych(zo, {"wt 27.10", "dane 05:38"}, z);
    SPRAWDZ(zo.czarne(0, 54, 800, 102) > 30000);
    // pkt 6: jeden nagłówek ekranów dnia i kart, bez liczb nocy
    for (const char* typ : {"S2A", "S2B", "S2C", "S6", "KM", "S5"}) {
      std::string l = naglowekDniaLewo(da);
      SPRAWDZ(l == "cz 29.10" && trescDnia(typ, "100", da).naglowekLewo.empty());
    }
    // pkt 7 i druga przegląd K22 `Noc: {czas} z maską.` bez oceny, czas nie powtarza się w faktach
    TrescEkranu k22 = trescDnia("S2A", "1", da);
    SPRAWDZ(k22.tytul.rfind("Noc: ", 0) == 0 && k22.tytul.find(" z maską.") != std::string::npos);
    SPRAWDZ(k22.fakty.size() == 2 && k22.fakty[0].p == "AHI" && k22.fakty[1].p == "wyciek l/min");
    {
      Noc bez = ha.back();
      bez.m01 = -1;
      Stany sb = liczStany(ha, "2026-10-29", {});
      ROWNE(tytulNocy(bez, sb), "Noc zapisana.");  // bez czasu z maską
      Stany pel = sb;  // jak po dodaniu reguł: uspokojenie spełnione
      pel.ocenaNiepelna = false;
      pel.uspokojenie = true;
      Noc dobra = ha.back();
      dobra.data = sb.ostatnia;
      if (!pel.odchylenie.jakiekolwiek()) ROWNE(tytulNocy(dobra, pel), "Noc w normie.");
    }
    // druga przegląd linia awarii nigdy nie jest ostrzeżeniem; zaproszenie też nie
    SPRAWDZ(!liniaJestOstrzezeniem(GrupaLinii::AWARIE) && !liniaJestOstrzezeniem(GrupaLinii::PYTANIA) &&
            !liniaJestOstrzezeniem(GrupaLinii::DANE) && !liniaJestOstrzezeniem(GrupaLinii::NOC));
    SPRAWDZ(liniaJestOstrzezeniem(GrupaLinii::WZORCE_L) && liniaJestOstrzezeniem(GrupaLinii::PILNOSC_A_SENNOSC) &&
            liniaJestOstrzezeniem(GrupaLinii::PRZYPOMNIENIE_R5B) && liniaJestOstrzezeniem(GrupaLinii::NOC_R_SKI_24));
    // druga przegląd K1, 5 wierszy po 50 px, dwie linie oceny w ramce: co najmniej 2 px odstępu od ramki
    // i między liniami (najdłuższe słowa oceny, liczba nocy 2-cyfrowa), także w wierszu bez ramki
    for (int wzor = 0; wzor < 2; wzor++) {
      DaneK1 kk = a1;
      for (int r = 0; r < DaneK1::WIERSZE; r++) {
        kk.w[r].stan = wzor == 0 || r % 2 == 0;
        kk.w[r].slowo = r % 2 ? "niżej niż zwykle" : "wyżej niż zwykle";
        kk.w[r].slowo2 = "od 99 nocy";
      }
      PlotnoTest kq;
      rysujK1(kq, kk);
      for (int r = 0; r < DaneK1::WIERSZE; r++) {
        int y = 182 + r * 50, zle0 = zle;
        if (kk.w[r].stan) {
          SPRAWDZ(kq.cz(12, y + 10) && kq.cz(400, y - 1) && kq.cz(400, y + 50));  // ramka y − 1 … y + 50
          for (int yy : {y + 1, y + 2, y + 47, y + 48}) SPRAWDZ(kq.czarne(14, yy, 786, yy + 1) == 0);  // 2 px od ramki
        } else {
          for (int yy : {y + 1, y + 2, y + 47, y + 48}) SPRAWDZ(kq.czarne(522, yy, 786, yy + 1) == 0);
        }
        SPRAWDZ(kq.czarne(522, y + 24, 786, y + 26) == 0);  // 2 px między linią 1 a linią 2
        SPRAWDZ(kq.czarne(522, y + 3, 786, y + 24) > 100 && kq.czarne(522, y + 26, 786, y + 47) > 100);  // obie linie są
        if (zle != zle0) printf("  K1 wiersz %d (wzór %d): tekst styka się z ramką albo z drugą linią\n", r, wzor);
      }
      SPRAWDZ(kq.czarne(0, 433, 800, 434) == 0);  // ostatni wiersz kończy się nad stopką (linia na 434)
    }
    // pkt 8 i 10: teksty ekranów dnia i kart bez `P95` i bez wielkiej litery formy grzecznościowej w środku zdania
    std::string wszystko;
    for (const char* typ : {"S2A", "S2B", "S2C", "S6", "KM", "S5"}) {
      TrescEkranu t = trescDnia(typ, "100", da);
      wszystko += t.tytul + "|" + t.powod + "|" + t.powodKarty + "|" + t.krok + "|" + t.etykieta + "|" + t.tytulPaska + "|";
      for (auto& f : t.fakty) wszystko += f.w + " " + f.p + "|";
      for (auto& k : t.kolumny) wszystko += k.et + " " + k.p + "|";
    }
    for (const char* pula : {"dobra_noc", "seria", "brak_danych", "powrot"})
      for (int i = 0; i < 12; i++) wszystko += tekstZdania(std::string(pula) + ":" + std::to_string(i)) + "|";
    for (long nr = 0; nr < 9; nr++) wszystko += wyborNocy(nr).zdanie + "|";
    SPRAWDZ(wszystko.find(" cię ") != std::string::npos);  // pule naprawdę przejrzane (`Prawie cię nie poznałem.`)
    SPRAWDZ(wszystko.find("P95") == std::string::npos);
    for (const char* f : {" Ciebie", " Tobie", " Tobą", " Twoj", " Twój", " Ci ", " Cię"}) {
      size_t i = wszystko.find(f);
      // wielka litera dozwolona tylko na początku zdania (po `. `)
      SPRAWDZ(i == std::string::npos || (i >= 1 && wszystko[i - 1] == '.'));
    }
  }

  // K2 opcja A: bez kolumny `30 nocy`: na prawo od linii przed normą (x 647) tylko tekst normy wyrównany do 780
  std::vector<Noc> h;
  for (int i = 0; i < 40; i++) {
    Noc n = noc(dodajDni("2026-10-29", -i), 400 + i % 7 * 9, 1.0f + (i % 5) * 0.4f, 8 + i % 6);
    n.oa = 0.3f;
    n.ca = 0.4f;
    n.h = 0.5f;
    n.emed = 8;
    n.ep95 = 9 + (i % 3) * 0.3f;
    n.psmed = 6;
    n.m13 = 14;  // każdy wiersz z wartością: bez zwinięcia do `brak danych: …` (R-KOK-18), pozycje wierszy stałe
    n.m17 = 1;
    h.push_back(n);
  }
  Norma nm = liczNorme(h, "2026-10-29", {});
  DaneK2 k2 = budujK2(h, "2026-10-29", nm, {});
  PlotnoTest m;
  rysujK2(m, k2);
  int y = 120, zle0 = zle;
  for (int r = 0; r < DaneK2::WIERSZE; r++) {
    if (k2.w[r].grupa) y += 2;
    int lewa = 780 - szer(F_P22B, k2.w[r].norma) - 1;  // tekst normy zaczyna się najwcześniej tutaj
    SPRAWDZ(m.czarne(649, y + 2, lewa, y + 26) == 0);
    y += 28;
  }
  SPRAWDZ(m.czarne(649, 95, 780 - szer(F_P22B, "norma") - 1, 112) == 0);  // w nagłówku tabeli nie ma `30 nocy`
  if (zle != zle0) printf("  K2: piksele w miejscu kolumny 30 nocy\n");
}


// ---------- lokalne źródło danych otoczenia (ADR-0019, spec Załącznik C) ----------
// Syntetyczne odpowiedzi według kontraktu wersji 1; adresy i dane nie są prawdziwe.
static const char* const OTO_PRZYKLAD = R"({
  "wersja": 1,
  "ts": "2026-10-30T22:30:00+01:00",
  "pogoda": {"temp": 8.2, "opis": "deszcz", "kod": 61, "tmin": 6, "tmax": 10, "opady_proc": 80,
             "miasto": "Katowice", "ts": "2026-10-30T22:15:00+01:00"},
  "powietrze": {"pm25": 12, "ts": "2026-10-30T22:15:00+01:00"},
  "sypialnia": {"co2": 967, "temp": null, "wilg": null, "ts": "2026-10-30T22:25:00+01:00",
                "od": "2026-10-30T22:25:00+01:00"},
  "ostrzezenia": [{"kod": "co2_milczy", "od": "2026-10-20T13:00:00+02:00"}, {"kod": "nowy_kod", "od": null}],
  "inne_pole": {"x": [1, 2, 3]}
})";

static void testOtoczenie() {
  const uint32_t T = 1793395800;  // 2026-10-30 22:30:00 +01:00
  // czas ISO 8601 z przesunięciem
  uint32_t e = 0;
  SPRAWDZ(epochZIso("2026-10-30T22:15:00+01:00", e) && e == 1793394900u);
  SPRAWDZ(epochZIso("2026-10-30T21:15:00Z", e) && e == 1793394900u);
  SPRAWDZ(epochZIso("2026-10-30T22:15+01:00", e) && e == 1793394900u);       // bez sekund
  SPRAWDZ(epochZIso("2026-10-30T22:15:00.250+01:00", e) && e == 1793394900u);  // ułamek sekundy
  SPRAWDZ(epochZIso("2026-10-20T13:00:00+02:00", e) && e == 1792494000u);
  SPRAWDZ(epochZIso("2026-10-30T22:15:00+0100", e) && e == 1793394900u);  // strefa bez dwukropka
  uint32_t a = 0, b = 0;  // zmiana czasu 25.10: ta sama godzina lokalna z dwiema strefami różni się o 1 h
  SPRAWDZ(epochZIso("2026-10-25T02:30:00+02:00", a) && epochZIso("2026-10-25T02:30:00+01:00", b) && b - a == 3600);
  SPRAWDZ(!epochZIso("2026-10-30T22:15:00", e));         // bez strefy: niejednoznaczne
  SPRAWDZ(!epochZIso("2026-02-31T10:00:00Z", e));        // zła data
  SPRAWDZ(!epochZIso("2026-10-30T24:00:00Z", e) && !epochZIso("", e) && !epochZIso("2026-10-30T22:15:00+01:00x", e));

  // pełna odpowiedź z kontraktu: wszystkie bloki, nieznane klucze i nieznany kod ostrzeżenia pominięte
  Otoczenie o;
  wyczyscOtoczenie(o, true);
  SPRAWDZ(parsujOtoczenie(OTO_PRZYKLAD, T, o) == StanOto::OK && o.stan == (int8_t)StanOto::OK && o.pobrano == T);
  ROWNE(pogodaTeraz(o, T), "Katowice 8° · deszcz");
  SPRAWDZ(pm25Teraz(o, T) == 12 && co2Teraz(o, T) == 967);
  SPRAWDZ(std::isnan(o.tempSyp) && std::isnan(o.wilgSyp));  // null w sypialni: brak, nie 0
  SPRAWDZ(co2MilczyTeraz(o, T) && o.co2MilczyOd == 1792494000u);
  ROWNE(liniaTestuOtoczenia(o, T, "22:30"), "Otoczenie: OK 22:30 · 8° · PM2.5 12 · CO2 967 · CO2 milczy");
  // świeżość kopii na panelu: CO2 do 30 min od pomiaru, pogoda do 9 h, ostrzeżenia do 60 min od odpowiedzi
  SPRAWDZ(co2Teraz(o, 1793395500u + 30 * 60) == 967 && co2Teraz(o, 1793395500u + 30 * 60 + 1) == -1);
  SPRAWDZ(!pogodaTeraz(o, 1793394900u + 9 * 3600).empty() && pogodaTeraz(o, 1793394900u + 9 * 3600 + 1).empty());
  SPRAWDZ(!co2MilczyTeraz(o, T + 3601));
  SPRAWDZ(co2Teraz(o, 0) == -1 && pogodaTeraz(o, 0).empty());  // bez zegara nie oceniamy świeżości: nic
  SPRAWDZ(co2Teraz(o, 1793395500u - 11 * 60) == -1);           // pomiar z przyszłości o > 10 min: zegar nie zgadza się

  // wszystkie bloki null: odpowiedź poprawna, nic do pokazania, wygląd jak bez źródła
  Otoczenie n;
  wyczyscOtoczenie(n, true);
  SPRAWDZ(parsujOtoczenie(R"({"wersja":1,"ts":"2026-10-30T22:30:00+01:00","pogoda":null,"powietrze":null,)"
                          R"("sypialnia":null,"ostrzezenia":[]})", T, n) == StanOto::OK);
  SPRAWDZ(pogodaTeraz(n, T).empty() && co2Teraz(n, T) == -1 && pm25Teraz(n, T) == -1 && !co2MilczyTeraz(n, T));
  ROWNE(liniaTestuOtoczenia(n, T, "22:30"), "Otoczenie: OK 22:30 · pogoda – · PM2.5 – · CO2 –");
  // nowa odpowiedź z blokami null kasuje stare wartości (źródło mówi „brak”)
  SPRAWDZ(parsujOtoczenie(R"({"wersja":1,"pogoda":null,"powietrze":null,"sypialnia":null})", T, o) == StanOto::OK);
  SPRAWDZ(pogodaTeraz(o, T).empty() && co2Teraz(o, T) == -1 && !co2MilczyTeraz(o, T));

  // częściowe: tylko pogoda, bez miasta i opisu; bloki brakujące jak null; blok bez `ts` = chwila odebrania
  Otoczenie c;
  wyczyscOtoczenie(c, true);
  SPRAWDZ(parsujOtoczenie(R"({"wersja":1,"pogoda":{"temp":-12.4}})", T, c) == StanOto::OK);
  ROWNE(pogodaTeraz(c, T), "−12°");  // minus U+2212
  SPRAWDZ(c.tsPogody == T && co2Teraz(c, T) == -1);
  SPRAWDZ(parsujOtoczenie(R"({"wersja":1.0,"pogoda":{"temp":0,"opis":"mgła","miasto":"Nowy Sącz"}})", T, c) == StanOto::OK);
  ROWNE(pogodaTeraz(c, T), "Nowy Sącz 0° · mgła");  // 0 °C to wartość, nie brak

  // null nigdy jako 0: temp null = brak pogody, pm25 i co2 null = brak, co2 0 (spoza zakresu) = brak, zły typ = brak
  SPRAWDZ(parsujOtoczenie(R"({"wersja":1,"pogoda":{"temp":null,"opis":"deszcz","miasto":"Katowice"},)"
                          R"("powietrze":{"pm25":null},"sypialnia":{"co2":null}})", T, c) == StanOto::OK);
  SPRAWDZ(pogodaTeraz(c, T).empty() && pm25Teraz(c, T) == -1 && co2Teraz(c, T) == -1);
  SPRAWDZ(parsujOtoczenie(R"({"wersja":1,"pogoda":{"temp":"8"},"sypialnia":{"co2":0},"powietrze":{"pm25":"12"}})", T, c) ==
          StanOto::OK);
  SPRAWDZ(pogodaTeraz(c, T).empty() && co2Teraz(c, T) == -1 && pm25Teraz(c, T) == -1);
  SPRAWDZ(parsujOtoczenie(R"({"wersja":1,"pogoda":"deszcz","sypialnia":[967]})", T, c) == StanOto::OK);  // blok złego typu
  SPRAWDZ(pogodaTeraz(c, T).empty() && co2Teraz(c, T) == -1);
  // ostrzeżenie bez `od` (źródło nigdy nie miało pomiaru): jest, bez czasu
  SPRAWDZ(parsujOtoczenie(R"({"wersja":1,"ostrzezenia":[{"kod":"co2_milczy","od":null}]})", T, c) == StanOto::OK);
  SPRAWDZ(co2MilczyTeraz(c, T) && c.co2MilczyOd == 0);
  // opis: najwyżej 20 znaków, bez znaków sterujących
  SPRAWDZ(parsujOtoczenie(R"({"wersja":1,"pogoda":{"temp":5,"opis":"przelotny\u0007 śnieg z deszczem i wiatrem"}})", T, c) ==
          StanOto::OK);
  ROWNE(pogodaTeraz(c, T), "5° · przelotny śnieg z de");  // 20 znaków po usunięciu znaku sterującego

  // wersja 2, brak wersji: jak brak źródła (dane znikają), w logu i na ekranie testu
  Otoczenie v;
  wyczyscOtoczenie(v, true);
  SPRAWDZ(parsujOtoczenie(OTO_PRZYKLAD, T, v) == StanOto::OK);
  std::string v2 = OTO_PRZYKLAD;
  v2.replace(v2.find("\"wersja\": 1"), 11, "\"wersja\": 2");
  SPRAWDZ(parsujOtoczenie(v2, T, v) == StanOto::ZLA_WERSJA && v.wersja == 2);
  SPRAWDZ(pogodaTeraz(v, T).empty() && co2Teraz(v, T) == -1 && !co2MilczyTeraz(v, T));
  ROWNE(liniaTestuOtoczenia(v, T, "22:30"), "Otoczenie: wersja 2, panel zna 1");
  SPRAWDZ(parsujOtoczenie(R"({"pogoda":{"temp":8}})", T, v) == StanOto::ZLA_WERSJA && v.wersja == -1);
  SPRAWDZ(parsujOtoczenie(R"({"wersja":"1","pogoda":{"temp":8}})", T, v) == StanOto::ZLA_WERSJA);

  // uszkodzony JSON i za duża odpowiedź: odrzucone, stare dane zostają (z ich czasem)
  Otoczenie u;
  wyczyscOtoczenie(u, true);
  SPRAWDZ(parsujOtoczenie(OTO_PRZYKLAD, T, u) == StanOto::OK);
  std::string urwane = std::string(OTO_PRZYKLAD).substr(0, 120);
  SPRAWDZ(parsujOtoczenie(urwane, T + 60, u) == StanOto::NIECZYTELNE && u.pobrano == T);
  SPRAWDZ(co2Teraz(u, T + 60) == 967);
  SPRAWDZ(parsujOtoczenie("[1,2,3]", T, u) == StanOto::NIECZYTELNE && parsujOtoczenie("", T, u) == StanOto::NIECZYTELNE);
  SPRAWDZ(parsujOtoczenie(R"({"wersja":1} {"wersja":1})", T, u) == StanOto::NIECZYTELNE);
  std::string duze = std::string(R"({"wersja":1,"x":")") + std::string(5000, 'a') + "\"}";
  SPRAWDZ(parsujOtoczenie(duze, T, u) == StanOto::ZA_DUZE);
  ROWNE(liniaTestuOtoczenia(u, T, "22:30"), "Otoczenie: odpowiedź za duża · zostają dane z 22:30");

  // nieudane próby HTTP: 401 (zły token), czas, inny kod; ponowienia tylko przy braku odpowiedzi (R-AW-26)
  nieudanaOtoczenia(u, 401, T);
  ROWNE(liniaTestuOtoczenia(u, T, "22:30"), "Otoczenie: HTTP 401 (zły OTO_TOKEN) · zostają dane z 22:30");
  nieudanaOtoczenia(u, -11, T);
  ROWNE(liniaTestuOtoczenia(u, T, ""), "Otoczenie: brak odpowiedzi (kod -11) · brak wcześniejszych danych");
  ROWNE(liniaTestuOtoczenia(u, T, "30.10 05:16"), "Otoczenie: brak odpowiedzi (kod -11) · zostają dane z 30.10 05:16");
  nieudanaOtoczenia(u, 404, T);
  ROWNE(liniaTestuOtoczenia(u, T, ""), "Otoczenie: HTTP 404 · brak wcześniejszych danych");
  SPRAWDZ(ponowicOtoczenie(-1, 1, 15000) && ponowicOtoczenie(503, 2, 6000));
  SPRAWDZ(!ponowicOtoczenie(-1, 3, 15000) && !ponowicOtoczenie(-1, 1, 4999));  // limit prób i budżet wybudzenia
  SPRAWDZ(!ponowicOtoczenie(401, 1, 15000) && !ponowicOtoczenie(403, 1, 15000) && !ponowicOtoczenie(200, 1, 15000) &&
          !ponowicOtoczenie(404, 1, 15000));
  Otoczenie w;
  wyczyscOtoczenie(w, false);
  ROWNE(liniaTestuOtoczenia(w, T, ""), "Otoczenie: wyłączone, brak OTO_URL w secrets.h");
  SPRAWDZ(pogodaTeraz(w, T).empty() && co2Teraz(w, T) == -1);
  wyczyscOtoczenie(w, true);
  ROWNE(liniaTestuOtoczenia(w, T, ""), "Otoczenie: jeszcze bez odczytu");

  // przypadki brzegowe źródła otoczenia
  {
    const uint32_t T6 = 1793422800;  // 2026-10-31 06:00 +01:00
    auto parsuj = [&](const std::string& j, Otoczenie& x) { wyczyscOtoczenie(x, true); return parsujOtoczenie(j, T6, x); };
    Otoczenie x;
    // `ts` bez strefy, nieczytelny albo złego typu: blok odrzucony (bit w zleTs), bez czasu odebrania
    SPRAWDZ(parsuj(R"({"wersja":1,"sypialnia":{"co2":900,"ts":"2026-10-29T10:00:00"}})", x) == StanOto::OK);
    SPRAWDZ(co2Teraz(x, T6) == -1 && (x.zleTs & 4));
    SPRAWDZ(parsuj(R"({"wersja":1,"pogoda":{"temp":8,"ts":"wczoraj"},"powietrze":{"pm25":5,"ts":5}})", x) == StanOto::OK);
    SPRAWDZ(pogodaTeraz(x, T6).empty() && pm25Teraz(x, T6) == -1 && x.zleTs == 3);
    SPRAWDZ(parsuj(R"({"wersja":1,"sypialnia":{"co2":900,"ts":"2026-02-31T06:00:00+01:00"}})", x) == StanOto::OK);
    SPRAWDZ(co2Teraz(x, T6) == -1);
    // `ts` = null albo brak: czas odebrania; `+0100` czytelne; świeżość od `ts`
    SPRAWDZ(parsuj(R"({"wersja":1,"sypialnia":{"co2":900,"ts":null}})", x) == StanOto::OK && co2Teraz(x, T6) == 900 && !x.zleTs);
    SPRAWDZ(parsuj(R"({"wersja":1,"sypialnia":{"co2":900,"ts":"2026-10-31T05:45:00+0100"}})", x) == StanOto::OK &&
            co2Teraz(x, T6) == 900);
    SPRAWDZ(parsuj(R"({"wersja":1,"sypialnia":{"co2":900,"ts":"2026-10-31T04:20:00Z"}})", x) == StanOto::OK &&
            co2Teraz(x, T6) == -1);  // 40 min: nieświeże
    SPRAWDZ(parsuj(R"({"wersja":1,"sypialnia":{"co2":1e3}})", x) == StanOto::OK && co2Teraz(x, T6) == 1000);
    SPRAWDZ(parsuj(R"({"wersja":1,"pogoda":{"temp":-0.4}})", x) == StanOto::OK);
    ROWNE(pogodaTeraz(x, T6), "0°");  // bez „−0”
    SPRAWDZ(parsuj(R"({"wersja":1,"ostrzezenia":{"kod":"co2_milczy"}})", x) == StanOto::OK && !co2MilczyTeraz(x, T6));
    // opis i miasto: znaki spoza kroju pominięte, sterujące pominięte, urwany UTF-8 obcięty; BOM tolerowany
    SPRAWDZ(parsuj("{\"wersja\":1,\"pogoda\":{\"temp\":8,\"opis\":\"\xE2\x98\x80 słońce\",\"miasto\":\"Kędzierzyn-Koźle\"}}", x) ==
            StanOto::OK);
    ROWNE(pogodaTeraz(x, T6), "Kędzierzyn-Koźle 8° · słońce");
    SPRAWDZ(parsuj("{\"wersja\":1,\"pogoda\":{\"temp\":8,\"opis\":\"ab\xC5\"}}", x) == StanOto::OK);
    ROWNE(pogodaTeraz(x, T6), "8° · ab");
    SPRAWDZ(parsuj(R"({"wersja":1,"pogoda":{"temp":8,"opis":"de\nszcz\u0000x","miasto":""}})", x) == StanOto::OK);
    ROWNE(pogodaTeraz(x, T6), "8° · deszczx");
    SPRAWDZ(parsuj("\xEF\xBB\xBF{\"wersja\":1,\"pogoda\":{\"temp\":8}}", x) == StanOto::OK);
    ROWNE(pogodaTeraz(x, T6), "8°");
    SPRAWDZ(parsuj("[1]", x) == StanOto::NIECZYTELNE);
    // energia: po nieudanym wybudzeniu i w slocie nocnym 1 próba; ponowienia w limicie prób
    SPRAWDZ(probOtoczenia(false, false) == 3 && probOtoczenia(true, false) == 1 && probOtoczenia(false, true) == 1);
    SPRAWDZ(!ponowicOtoczenie(-1, 1, 15000, 1) && ponowicOtoczenie(-1, 1, 15000, 3));
    SPRAWDZ(brakOdpowiedziOtoczenia(OTO_KOD_CZAS) && !brakOdpowiedziOtoczenia(401));
    // ten sam pomiar CO2 (to samo `ts`) nie wchodzi do bufora dwa razy
    SPRAWDZ(parsuj(R"({"wersja":1,"sypialnia":{"co2":900,"ts":"2026-10-31T05:50:00+01:00"}})", x) == StanOto::OK);
    SPRAWDZ(co2DoBufora(x, T6) == 900 && co2DoBufora(x, T6 + 60) == -1);
    SPRAWDZ(parsujOtoczenie(R"({"wersja":1,"sypialnia":{"co2":910,"ts":"2026-10-31T05:58:00+01:00"}})", T6, x) == StanOto::OK);
    SPRAWDZ(co2DoBufora(x, T6) == 910);
    // inny adres źródła = inne dane w RTC
    SPRAWDZ(hashAdresu("http://192.168.1.60:8080/otoczenie") != hashAdresu("http://192.168.1.61:8080/otoczenie") &&
            hashAdresu("") != 0);
    // długie miasto i opis: ustępują opis, potem miasto
    std::string l = "Kędzierzyn-Koźle −12° · zachmurzenie duże", pr = stopkaCzujnika(35, 99, 4999);
    dopasujStopke(l, pr);
    SPRAWDZ(szer(F_P22B, l) + szer(F_P22B, pr) + ODSTEP_STOPKI <= SZER_STOPKI && pr == stopkaCzujnika(35, 99, 4999));
  }

  // stopka (R-EKR-10): prawo `przy łóżku … · CO2 …`, lewo pogoda; najdłuższe wartości mieszczą się w 744 px
  ROWNE(stopkaCzujnika(20.6f, 45.4f, 770), "przy łóżku 21° · 45% · CO2 770");
  ROWNE(stopkaCzujnika(20.6f, 45.4f), "przy łóżku 21° · 45%");  // bez źródła: jak dotąd
  ROWNE(stopkaCzujnika(NAN, 45.4f, 770), "");  // bez czujnika panelu prawa część pusta, bez samotnego CO2 (R-AW-13)
  ROWNE(stopkaCzujnika(NAN, NAN), "");
  {
    std::string l = "Katowice −12° · śnieg", p = stopkaCzujnika(35, 99, 4999);
    ROWNE(p, "przy łóżku 35° · 99% · CO2 4999");
    printf("  stopka z pogodą i CO2: %d + %d px\n", szer(F_P22B, l), szer(F_P22B, p));
    SPRAWDZ(szer(F_P22B, l) + szer(F_P22B, p) + ODSTEP_STOPKI <= SZER_STOPKI);
    std::string l2 = l, p2 = p;
    dopasujStopke(l2, p2);
    SPRAWDZ(l2 == l && p2 == p);  // mieści się bez skracania
    // najdłuższe z kontraktu: miasto 32 znaki, opis 20 znaków (szerokie litery): ustępują opis, potem miasto
    std::string ml = "Wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww −12° · WWWWWWWWWWWWWWWWWWWW", mp = p;
    dopasujStopke(ml, mp);
    SPRAWDZ(szer(F_P22B, ml) + szer(F_P22B, mp) + ODSTEP_STOPKI <= SZER_STOPKI);
    SPRAWDZ(mp == p);  // CO2 zostaje, gdy wystarczy skrócić pogodę
    // rysunek: lewa i prawa część nie nachodzą (24 px przerwy), stopka na swoim miejscu
    std::vector<Noc> h = histStany("2026-10-30", 30);
    Stany st = liczStany(h, "2026-10-30", {});
    DaneDnia d;
    d.dzis = "2026-10-30";
    d.hist = &h;
    d.stany = &st;
    d.pogoda = l;
    d.przyLozku = p;
    TrescEkranu t = trescDnia("S2A", "1", d);
    ROWNE(t.stopkaLewo, l);
    ROWNE(t.stopkaPrawo, p);
    PlotnoTest pt;
    rysujEkranDanych(pt, {"pt 30.10", "dane 05:40"}, t);
    int kl = 28 + szer(F_P22B, l), pp = 772 - szer(F_P22B, p);
    SPRAWDZ(pp - kl >= ODSTEP_STOPKI && pt.czarne(kl + 1, 440, pp - 1, 480) == 0);
    SPRAWDZ(pt.czarne(28, 444, kl, 472) > 50 && pt.czarne(pp, 444, 772, 472) > 50);
    // regresja: bez źródła stopka i ekran jak bez źródła (tylko odczyt czujnika panelu, bez lewej części)
    DaneDnia bez = d;
    bez.pogoda = pogodaTeraz(w, T);
    bez.przyLozku = stopkaCzujnika(22.2f, 45.4f, co2Teraz(w, T));
    TrescEkranu tb = trescDnia("S2A", "1", bez);
    ROWNE(tb.stopkaLewo, "");
    ROWNE(tb.stopkaPrawo, "przy łóżku 22° · 45%");
    DaneDnia stary = d;
    stary.pogoda = "";
    stary.przyLozku = stopkaCzujnika(22.2f, 45.4f);
    PlotnoTest p1, p2r;
    rysujEkranDanych(p1, {"pt 30.10", "dane 05:40"}, tb);
    rysujEkranDanych(p2r, {"pt 30.10", "dane 05:40"}, trescDnia("S2A", "1", stary));
    SPRAWDZ(p1.px == p2r.px);  // piksel w piksel
    // karta (bez zdania budzika od 10.11.2026): odczyt z dopiskiem CO2 stoi w całości z prawej, pogoda z lewej
    TrescEkranu kt;
    kt.tytul = "Noc: 6 h 38 z maską.";
    kt.stopkaLewo = "Katowice 7° · pochmurno";
    kt.stopkaPrawo = "przy łóżku 35° · 99% · CO2 4999";
    PlotnoTest kp2;
    rysujKarte(kp2, {"cz 30.11", "dane 05:46"}, kt, "");
    int x0 = 772 - szer(F_P22B, "przy łóżku 35° · 99% · CO2 4999");
    SPRAWDZ(kp2.czarne(x0, 444, 772, 472) > 50);
    SPRAWDZ(kp2.czarne(28, 444, 28 + szer(F_P22B, kt.stopkaLewo), 472) > 50);
  }

  // E-08 (`co2_milczy`): linia grupy „dane”, biały pasek w ramce, raz na epizod
  ROWNE(tekstE08("2026-10-20", 13 * 60, "2026-10-31"), "Brak odczytu CO2 od 20.10 13:00.");
  ROWNE(tekstE08("2026-10-31", 5 * 60 + 2, "2026-10-31"), "Brak odczytu CO2 od 05:02.");
  ROWNE(tekstE08("", -1, "2026-10-31"), "Brak odczytu CO2.");
  SPRAWDZ(!liniaJestOstrzezeniem(GrupaLinii::DANE));
  {
    StanE08 se;
    bool zm = false;
    SPRAWDZ(!pokazE08(false, false, 0, "2026-10-31", se, zm) && !zm);
    SPRAWDZ(pokazE08(true, false, 1792494000u, "2026-10-31", se, zm) && zm);    // nowy epizod: dziś linia, zapis
    SPRAWDZ(pokazE08(true, false, 1792494000u, "2026-10-31", se, zm) && !zm);   // ten sam dzień: dalej (bez zapisu)
    SPRAWDZ(!pokazE08(true, false, 1792494000u, "2026-11-01", se, zm) && !zm);  // następny dzień: milczy
    SPRAWDZ(!pokazE08(false, false, 0, "2026-11-01", se, zm) && !zm);           // brak świeżej odpowiedzi: stan bez zmian
    SPRAWDZ(!pokazE08(true, false, 1792494000u, "2026-11-02", se, zm));         // ten sam epizod dalej milczy
    SPRAWDZ(!pokazE08(false, true, 0, "2026-11-02", se, zm) && zm);             // CO2 wróciło: koniec epizodu
    SPRAWDZ(pokazE08(true, false, 1792500000u, "2026-11-03", se, zm) && zm);    // nowy epizod (inne `od`)
    SPRAWDZ(pokazE08(true, false, 0, "2026-11-04", se, zm) && zm);              // `od` null to też epizod, raz
    SPRAWDZ(!pokazE08(true, false, 0, "2026-11-05", se, zm));
    SPRAWDZ(!pokazE08(false, true, 0, "2026-11-06", se, zm) && zm);             // dane wróciły
    SPRAWDZ(pokazE08(true, false, 0, "2026-11-07", se, zm) && zm);              // znowu `od` null: nowy epizod
    // to samo znane `od` po jednej odpowiedzi bez ostrzeżenia nie wraca
    StanE08 fl;
    SPRAWDZ(pokazE08(true, false, 1000, "2026-10-31", fl, zm));
    SPRAWDZ(!pokazE08(false, true, 0, "2026-11-01", fl, zm) && zm);
    SPRAWDZ(!pokazE08(true, false, 1000, "2026-11-01", fl, zm) && zm && !fl.koniec);
    SPRAWDZ(!pokazE08(true, false, 1000, "2026-11-02", fl, zm));
    SPRAWDZ(pokazE08(true, false, 2000, "2026-11-02", fl, zm));  // inne `od`: nowy epizod
    std::vector<Noc> h = histStany("2026-10-30", 30);
    Stany st = liczStany(h, "2026-10-30", {});
    DaneDnia d;
    d.dzis = "2026-10-30";
    d.hist = &h;
    d.stany = &st;
    d.linia = tekstE08("2026-10-20", 13 * 60, "2026-10-30");
    d.liniaOstrzezenie = liniaJestOstrzezeniem(GrupaLinii::DANE);
    TrescEkranu t = trescDnia("S2A", "1", d);
    ROWNE(t.linia, "Brak odczytu CO2 od 20.10 13:00.");
    SPRAWDZ(!t.liniaOstrzezenie);
    SPRAWDZ(szer(F_P24B, t.linia) <= 744);
    PlotnoTest lp;
    rysujEkranDanych(lp, {"pt 30.10", "dane 05:40"}, t);
    SPRAWDZ(lp.cz(12, 80) && lp.cz(13, 80) && !lp.cz(14, 80));  // ramka 2 px
    SPRAWDZ(lp.czarne(0, 54, 800, 102) < 8000);                 // biały pasek, nie czarny
  }

  // sypialnia z CO2 (M-23): bufor nocy, mediana do /sypialnia.csv (czwarte pole tylko przy CO2), norma, S2c
  {
    BuforSypialni b;
    dodajOdczyt(b, "2026-10-30", 23 * 60, 21.0f, 44);
    dodajCo2(b, "2026-10-30", 23 * 60, 900);
    dodajCo2(b, "2026-10-30", 23 * 60 + 30, -1);  // brak odczytu: pomijany, nie 0
    dodajCo2(b, "2026-10-31", 2 * 60, 1100);
    dodajCo2(b, "2026-10-31", 4 * 60, 1000);
    dodajCo2(b, "2026-10-31", 7 * 60, 3000);      // poza oknem nocy
    SPRAWDZ(b.n == 1 && b.nCo2 == 3);
    NocSypialni r;
    SPRAWDZ(zamknijNoc(b, "2026-10-31", 6 * 60, r) && b.nCo2 == 0);
    SPRAWDZ(std::fabs(r.co2 - 1000) < 1e-4 && std::fabs(r.t - 21) < 1e-4);
    ROWNE(zapiszSypialnie(r), "2026-10-31;21.0;44.0");  // /sypialnia.csv zawsze 3 pola (starszy firmware)
    ROWNE(zapiszCo2Nocy(r), "2026-10-31;1000");          // CO2 osobno w /sypialnia-co2.csv
    std::string dc;
    float vc = 0;
    SPRAWDZ(parsujCo2Nocy("2026-10-31;1000", dc, vc) && dc == "2026-10-31" && std::fabs(vc - 1000) < 1e-4);
    SPRAWDZ(parsujCo2Nocy("2026-10-31;812\r", dc, vc) && std::fabs(vc - 812) < 1e-4);
    SPRAWDZ(!parsujCo2Nocy("2026-10-31;", dc, vc) && !parsujCo2Nocy("2026-10-31;0", dc, vc) &&
            !parsujCo2Nocy("2026-10-31;nan", dc, vc) && !parsujCo2Nocy("2026-10-31;1;2", dc, vc) &&
            !parsujCo2Nocy("31.10;900", dc, vc));
    NocSypialni q;
    // migracja: wiersz 4-polowy z wczesnej wersji prototypowej czytany z CO2 (dyrygent przenosi je do /sypialnia-co2.csv)
    SPRAWDZ(parsujSypialnie("2026-10-31;21.0;44.0;1000", q) && std::fabs(q.co2 - 1000) < 1e-4 && std::fabs(q.w - 44) < 1e-4);
    SPRAWDZ(parsujSypialnie("2026-10-30;21.5;45.0;812\r", q) && std::fabs(q.co2 - 812) < 1e-4);
    SPRAWDZ(parsujSypialnie("2026-10-30;21.5;45.0\r", q) && std::fabs(q.w - 45) < 1e-4 && std::isnan(q.co2));
    SPRAWDZ(parsujSypialnie("2026-10-31;21.0;44.0", q) && std::isnan(q.co2));  // dawny układ
    SPRAWDZ(parsujSypialnie("2026-10-31;21.0;44.0;", q) && std::isnan(q.co2));
    SPRAWDZ(!parsujSypialnie("2026-10-31;21.0;44.0;x", q) && !parsujSypialnie("2026-10-31;21.0;44.0;1000;5", q));
    NocSypialni bezCo2{"2026-10-31", 21.0f, 44.0f};
    ROWNE(zapiszSypialnie(bezCo2), "2026-10-31;21.0;44.0");  // bez źródła plik bez CO2
    BuforSypialni tylkoCo2;  // noc tylko z CO2 (czujnik panelu bez odczytu): wiersz z pustą temperaturą
    dodajCo2(tylkoCo2, "2026-10-31", 1 * 60, 800);
    SPRAWDZ(zamknijNoc(tylkoCo2, "2026-10-31", 9 * 60, r) && std::isnan(r.t));
    ROWNE(zapiszSypialnie(r), "2026-10-31;;");  // panel takiego wiersza nie zapisuje (tylko CO2: /sypialnia-co2.csv)
    ROWNE(zapiszCo2Nocy(r), "2026-10-31;800");
    BuforSypialni pelny;  // bufor 24 odczytów: nadmiar pomijany
    for (int i = 0; i < 30; i++) dodajCo2(pelny, "2026-10-31", 60 + i, 900);
    SPRAWDZ(pelny.nCo2 == 24);
    // norma CO2: osobno z nocy z odczytem CO2, minimum 14
    std::vector<NocSypialni> s;
    for (int i = 1; i <= 20; i++) {
      NocSypialni x{dodajDni("2026-10-31", -i), (float)(20 + i % 3), (float)(44 + i % 4)};
      if (i <= 13) x.co2 = (float)(700 + 10 * i);
      s.push_back(x);
    }
    NormaSypialni nm = liczNormeSypialni(s, "2026-10-31", {});
    SPRAWDZ(nm.t.jest() && !nm.co2.jest() && nm.co2.n == 13);
    std::vector<Noc> h = histStany("2026-10-31", 30);
    Stany st = liczStany(h, "2026-10-31", {});
    DaneDnia d;
    d.dzis = "2026-10-31";
    d.hist = &h;
    d.stany = &st;
    NocSypialni dzis{"2026-10-31", 21.0f, 45.0f};
    d.sypNoc = &dzis;
    d.sypNorma = nm;
    TrescEkranu bez = trescDnia("S2C", "2", d);
    SPRAWDZ(bez.kolumny.size() == 2);  // R-EKR-17: bez CO2 ostatniej nocy dwie kolumny
    dzis.co2 = 950;
    TrescEkranu t = trescDnia("S2C", "2", d);
    SPRAWDZ(t.kolumny.size() == 3);
    ROWNE(t.kolumny[2].et, "CO2 ppm");
    ROWNE(t.kolumny[2].w, "950");
    ROWNE(t.kolumny[2].p, "uczę się");  // bez normy CO2 (13 nocy)
    SPRAWDZ(!t.kolumny[2].poza);
    ROWNE(t.powod, bez.powod);  // `uczę się` przy CO2 nie zmienia zdania
    s[13].co2 = 840;            // 14. noc z CO2: norma jest
    nm = liczNormeSypialni(s, "2026-10-31", {});
    SPRAWDZ(nm.co2.jest());
    d.sypNorma = nm;
    t = trescDnia("S2C", "2", d);
    ROWNE(t.kolumny[2].p, "zwykle " + calkowita(nm.co2.med));
    SPRAWDZ(t.kolumny[2].poza && !t.kolumny[2].ponadProg && t.kolumny[2].s == "↑");
    ROWNE(t.powod, "Sypialnia: różnice niżej.");  // różnica tylko w CO2 (czujnik sypialni)
    ROWNE(t.powodKarty, "Sypialnia: inaczej niż zwykle.");
    dzis.co2 = (float)nm.co2.med;
    t = trescDnia("S2C", "2", d);
    SPRAWDZ(t.kolumny[2].p.empty() && !t.kolumny[2].poza);  // w paśmie: `jak zwykle` mówi legenda
    // CO2 nocy ≥ 1000 ppm (próg S-K-01) nigdy nie daje `Dobre warunki do snu.`, kolumna
    // CO2 z czarnym polem „ponad próg”, także bez normy i przy normie wysokiej
    {
      std::vector<NocSypialni> sw;
      for (int i = 1; i <= 30; i++) sw.push_back({dodajDni("2026-10-31", -i), 21.0f, 45.0f});
      NormaSypialni bezCo2Normy = liczNormeSypialni(sw, "2026-10-31", {});
      NocSypialni n1600{"2026-10-31", 21.0f, 45.0f};
      n1600.co2 = 1600;
      DaneDnia a = d;
      a.sypNoc = &n1600;
      a.sypNorma = bezCo2Normy;
      TrescEkranu ta = trescDnia("S2C", "2", a);
      SPRAWDZ(ta.powod != "Dobre warunki do snu." && ta.powodKarty != "Dobre warunki do snu.");
      ROWNE(ta.powod, "Sypialnia: różnice niżej.");
      SPRAWDZ(ta.kolumny.size() == 3 && ta.kolumny[2].ponadProg && !ta.kolumny[2].poza);
      ROWNE(ta.kolumny[2].p, "ponad 1000");
      ROWNE(ta.powodKarty, "Sypialnia: CO2 ponad 1000.");  // karta K22: samo przekroczenie progu
      SPRAWDZ(kartaMiesci(ta));
      NocSypialni goraco{"2026-10-31", 29.0f, 45.0f};  // temperatura poza normą i CO2 ponad próg: zdanie o łóżku
      goraco.co2 = 1600;
      a.sypNoc = &goraco;
      TrescEkranu tg = trescDnia("S2C", "2", a);
      ROWNE(tg.powod, "Przy łóżku: różnice niżej.");
      ROWNE(tg.powodKarty, "Przy łóżku: inaczej niż zwykle.");
      for (int i = 0; i < 30; i++) sw[i].co2 = (float)(1400 + (i % 10) * 10);  // 1400–1490 przez 30 nocy: norma wysoka
      NormaSypialni wysoka = liczNormeSypialni(sw, "2026-10-31", {});
      SPRAWDZ(wysoka.co2.jest());
      NocSypialni n1450{"2026-10-31", 21.0f, 45.0f};
      n1450.co2 = 1450;  // w paśmie normy, ale ponad próg
      a.sypNoc = &n1450;
      a.sypNorma = wysoka;
      TrescEkranu tb = trescDnia("S2C", "2", a);
      SPRAWDZ(tb.powod != "Dobre warunki do snu." && tb.kolumny[2].ponadProg);
      NocSypialni n999{"2026-10-31", 21.0f, 45.0f};
      n999.co2 = 999;  // tuż pod progiem, bez normy: zdanie spokojne
      a.sypNoc = &n999;
      a.sypNorma = bezCo2Normy;
      TrescEkranu tc = trescDnia("S2C", "2", a);
      SPRAWDZ(!tc.kolumny[2].ponadProg);
      ROWNE(tc.powod, "Dobre warunki do snu.");
      PlotnoTest pp;  // czarne pole kolumny CO2 z białym tekstem (jedna legenda: czarne = ponad próg)
      rysujEkranDanych(pp, {"so 31.10", "dane 06:30"}, ta);
      int kx = 28 + 2 * 186, top = gornaPaska(ta);
      SPRAWDZ(pp.czarne(kx - 5, top + 40, kx + 179, top + 120) > 70 * 184 * 8 / 10);
    }
    d.linia = "Brak odczytu CO2 od 20.10 13:00.";
    d.przyLozku = "przy łóżku 35° · 99% · CO2 4999";
    d.pogoda = "Katowice −12° · śnieg";
    dzis.co2 = 4999;
    t = trescDnia("S2C", "2", d);
    PlotnoTest sp;
    rysujEkranDanych(sp, {"so 31.10", "dane 06:30"}, t);
    SPRAWDZ(szer(F_P28B, "4999") + 8 + szer(F_P28B, "↑") < 186 && szer(F_P22B, "zwykle 4999") < 180);  // kolumna 186 px
    SPRAWDZ(sp.czarne(0, 424, 800, 434) == 0);  // pasek kończy się nad stopką także z linią uwagi
  }
}


// ---------- pomiar baterii ----------
static void testBateria() {
  const uint32_t T0 = 1793422800;  // 2026-10-31 06:00 +01:00
  WpisBaterii w;
  SPRAWDZ(parsujBaterie("1793422800;3950;1840;31200;zegar", w) && w.mV == 3950 && w.wifiMs == 1840 &&
          w.aktywnyMs == 31200 && w.powod == "zegar");
  ROWNE(zapiszBaterie(w), "1793422800;3950;1840;31200;zegar");
  SPRAWDZ(parsujBaterie("1793422800;3950;0;-1;start\r", w) && w.aktywnyMs == -1);
  SPRAWDZ(!parsujBaterie("1793422800;3950;0;-1", w) && !parsujBaterie("x;3950;0;0;zegar", w) &&
          !parsujBaterie("1793422800;3,95;0;0;zegar", w) && !parsujBaterie("1793422800;3950;0;0;", w));
  // mało danych: `zbieram dane`
  std::vector<WpisBaterii> b;
  for (int i = 0; i < 11; i++) b.push_back({T0 + i * 1800u, 3950 - i, 1500, 30000, "zegar"});
  TrendBaterii t = trendBaterii(b);
  SPRAWDZ(!t.jest && t.n == 11);
  ROWNE(liniaBaterii(t, NAN), "Bateria 3,94 V · zbieram dane (5 h)");
  ROWNE(liniaBaterii(trendBaterii({}), 3.951), "Bateria 3,95 V");
  ROWNE(liniaBaterii(trendBaterii({}), NAN), "Bateria – V");
  // regresja: 2,1 doby co 30 min, spadek 40 mV na dobę, szum ±5 mV → ok. −0,04 V/dobę, dni do 3,5 V
  b.clear();
  for (int i = 0; i <= 101; i++) b.push_back({T0 + i * 1800u, (int)lround(4040 - 40.0 * i / 48 + (i % 3 - 1) * 5), 1500, 30000, "zegar"});
  t = trendBaterii(b);
  SPRAWDZ(t.jest && std::fabs(t.vNaDobe + 0.04) < 0.003 && t.doby > 2.0 && t.doby <= 101 / 48.0);
  SPRAWDZ(t.dniDoProgu > 10 && t.dniDoProgu < 13);  // (3,961 − 3,5) / 0,04 ≈ 11,5
  SPRAWDZ(b.back().mV == 3961);
  ROWNE(liniaBaterii(t, NAN), "Bateria 3,96 V · −0,04 V/dobę z " + przecinek1(t.doby) + " doby · ok. " +
                                  calkowita(t.dniDoProgu) + " dni");
  // wykrycie ładowania: skok w górę ≥ 60 mV → trend tylko od ostatniego skoku (tu za mało danych po nim)
  std::vector<WpisBaterii> c = b;
  c.push_back({b.back().ep + 1800, 4150, 1500, 30000, "zegar"});
  for (int i = 1; i <= 10; i++) c.push_back({b.back().ep + 1800 + i * 1800u, 4150 - i, 1500, 30000, "zegar"});
  TrendBaterii tc = trendBaterii(c);
  SPRAWDZ(!tc.jest && tc.n <= 11 && std::fabs(tc.v - 4.14) < 1e-9);
  ROWNE(liniaBaterii(tc, NAN), "Bateria 4,14 V · zbieram dane (4 h)");  // od pierwszego spadku po ładowaniu
  // wpisy w złej kolejności: sortowanie po czasie; brak spadku: bez szacunku dni
  std::vector<WpisBaterii> d;
  for (int i = 0; i <= 60; i++) d.push_back({T0 + (60 - i) * 1800u, 4000, 1000, 20000, "zegar"});
  TrendBaterii td = trendBaterii(d);
  SPRAWDZ(td.jest && td.dniDoProgu < 0);
  ROWNE(liniaBaterii(td, NAN), "Bateria 4,00 V · 0,00 V/dobę z 1,3 doby · bez spadku");
  ROWNE(liniaBaterii(td, 3.912), "Bateria 3,91 V · 0,00 V/dobę z 1,3 doby · bez spadku");  // bieżący odczyt
  // przypadki brzegowe baterii: wolne ładowanie, stałe napięcie przy kablu, jeden zaszumiony odczyt
  auto W = [](uint32_t ep, int mv) { return WpisBaterii{ep, mv, 5000, 25000, "zegar"}; };
  std::vector<WpisBaterii> wolne;  // +20 mV na wybudzenie przez 10 wybudzeń, potem −1 mV
  int v = 3700;
  for (int i = 0; i < 10; i++) { wolne.push_back(W(T0 + i * 1800u, v)); v += 20; }
  for (int i = 10; i < 110; i++) { wolne.push_back(W(T0 + i * 1800u, v)); v -= 1; }
  TrendBaterii tw = trendBaterii(wolne);
  SPRAWDZ(tw.jest && std::fabs(tw.vNaDobe + 0.048) < 0.003);  // tylko faza spadku: −1 mV co 30 min
  std::vector<WpisBaterii> kabel;  // 12 h na 4200 mV, potem odłączenie i spadek 1 mV co 30 min od 4100
  for (int i = 0; i < 24; i++) kabel.push_back(W(T0 + i * 1800u, 4200));
  for (int i = 24; i < 120; i++) kabel.push_back(W(T0 + i * 1800u, 4100 - (i - 24)));
  TrendBaterii tk = trendBaterii(kabel);
  SPRAWDZ(tk.jest && std::fabs(tk.vNaDobe + 0.048) < 0.003);  // plateau przy kablu poza oknem
  std::vector<WpisBaterii> rowny;  // 2 doby po −40 mV na dobę, jeden odczyt −70 mV
  for (int i = 0; i <= 96; i++) rowny.push_back(W(T0 + i * 1800u, 4000 - i * 40 / 48));
  TrendBaterii t0 = trendBaterii(rowny);
  rowny[90].mV -= 70;
  TrendBaterii ts = trendBaterii(rowny);
  SPRAWDZ(ts.jest && std::fabs(ts.vNaDobe - t0.vNaDobe) < 0.002 && std::fabs(ts.doby - t0.doby) < 1e-9);  // okno bez resetu
  std::vector<WpisBaterii> rowneEp;  // powtórzone chwile: bez dzielenia przez zero
  for (int i = 0; i < 5; i++) rowneEp.push_back(W(T0, 4000));
  rowneEp.push_back(W(T0 + 90000, 3990));
  SPRAWDZ(!trendBaterii(rowneEp).jest);
  // odmiana: 1 dzień, 2 dni
  TrendBaterii od;
  od.jest = true;
  od.n = 10;
  od.v = 3.6;
  od.vNaDobe = -0.1;
  od.doby = 2;
  od.dniDoProgu = 1;
  ROWNE(liniaBaterii(od, 3.6), "Bateria 3,60 V · −0,10 V/dobę z 2,0 doby · ok. 1 dzień");
  od.dniDoProgu = 2;
  ROWNE(liniaBaterii(od, 3.6), "Bateria 3,60 V · −0,10 V/dobę z 2,0 doby · ok. 2 dni");
  od.vNaDobe = -1.23;
  od.dniDoProgu = 0.4;
  SPRAWDZ(szer(F_P22B, liniaBaterii(od, 4.2) + " · Wi-Fi śr. 120,3 s · aktywny 600 s") > 744);  // idzie do linii Wi-Fi
  SPRAWDZ(szer(F_P22B, "Czynniki: 120 nocy · ekrany: 400 · czujnik panelu −10,5° · 100%") <= 744);
  // przycinanie do 30 dni (jak /humor.csv)
  // z zapasem: przy 31 dniach jeszcze nic, przy wpisie starszym niż 32 dni do 30
  std::vector<WpisBaterii> p = {{T0 - 31 * 86400u, 4000, 0, 0, "zegar"}, {T0 - 29 * 86400u, 3990, 0, 0, "zegar"},
                                {T0, 3950, 0, 0, "start"}};
  SPRAWDZ(!przytnijBaterie(p, T0) && p.size() == 3);
  p.insert(p.begin(), WpisBaterii{T0 - 33 * 86400u, 4010, 0, 0, "zegar"});
  SPRAWDZ(przytnijBaterie(p, T0) && p.size() == 2 && !przytnijBaterie(p, T0));
  SPRAWDZ(!wymagaPrzyciecia(T0 - 31 * 86400u, T0) && wymagaPrzyciecia(T0 - 33 * 86400u, T0) && !wymagaPrzyciecia(0, T0));
  SPRAWDZ(!przytnijBaterie(p, 0));  // bez zegara bez zmian
  // średnie z ostatniej doby
  std::vector<WpisBaterii> s = {{T0 - 2 * 86400u, 3900, 99000, 99000, "zegar"}, {T0 - 3600, 3900, 2000, 30000, "zegar"},
                                {T0, 3900, 1000, -1, "start"}};
  ROWNE(sredniaDoby(s, T0), "Wi-Fi śr. 1,5 s · aktywny 30 s");
  ROWNE(sredniaDoby({}, T0), "");
  // ekran testu: najdłuższe linie w 744 px (9 linii, test w testSkalaB)
  std::string naj = "Bateria 4,20 V · −0,25 V/dobę z 29,9 doby · ok. 999+ dni";
  SPRAWDZ(szer(F_P22B, naj) <= 744);
  // średnie z doby nie mieszczą się w linii baterii z trendem: idą do linii Wi-Fi (liniePomiaruBaterii)
  SPRAWDZ(szer(F_P22B, "Bateria 3,95 V · −0,04 V/dobę z 2,1 doby · ok. 25 dni · Wi-Fi śr. 8,2 s · aktywny 31 s") > 744);
  SPRAWDZ(szer(F_P22B, "Wi-Fi: 192.168.100.200 · RSSI -90 dBm · śr. 18,2 s · aktywny 131 s") <= 744);
  SPRAWDZ(sredniaDoby(s, T0).substr(0, 6) == "Wi-Fi ");
  SPRAWDZ(szer(F_P22B, "Czynniki: 120 nocy · ekrany: 400 · czujnik panelu 22,1° · 45%") <= 744);
}

// Numer zdania z `/humor.csv` to liczba z zewnątrz (plik na flash,
// także ręcznie poprawiony albo uszkodzony). Indeks ujemny, nieliczbowy, z resztą po liczbie albo spoza
// puli daje pusty tekst, nigdy odczytu spoza tablicy (przypadek z ASan: `seria:-1`).
static void testNumerZdaniaZPliku() {
  ROWNE(tekstZdania("seria:-1"), "");
  ROWNE(tekstZdania("seria:-2147483648"), "");
  ROWNE(tekstZdania("seria:-99999999999999999999"), "");
  ROWNE(tekstZdania("seria:99999999999999999999"), "");
  ROWNE(tekstZdania("seria:2147483647"), "");
  ROWNE(tekstZdania("seria:4294967295"), "");
  ROWNE(tekstZdania("seria:abc"), "");
  ROWNE(tekstZdania("seria:"), "");
  ROWNE(tekstZdania("seria:1x"), "");
  ROWNE(tekstZdania("seria: 1"), "");
  ROWNE(tekstZdania("seria:+1"), "");
  ROWNE(tekstZdania("seria:1000"), "");
  ROWNE(tekstZdania("nieznana:0"), "");
  // poprawne numery bez zmian
  ROWNE(tekstZdania("seria:0"), "Kolejna noc. Konsekwencja bywa natrętna.");
  ROWNE(tekstZdania("kamien:0"), "Tyle nocy? Maska powinna dostać stażowe.");
  // łańcuch jak na panelu: wpis z pliku → numer zdania → tekst karty
  WpisHumoru w;
  SPRAWDZ(parsujHumor("2026-11-02;S2A;;K;D01;seria:-1", w));
  ROWNE(tekstZdania(w.zdanie), "");
  // granica puli: `seria` ma 7 zdań przyjętych (08.11.2026) i pustą listę propozycji
  const Pula* ps = pula("seria");
  SPRAWDZ(ps && ps->kanwa.size() == 7 && ps->propozycje.empty());
  if (ps && ps->kanwa.size() == 7) {
    ROWNE(tekstZdania("seria:2"), ps->kanwa[2]);
    ROWNE(tekstZdania("seria:6"), ps->kanwa[6]);
  }
  ROWNE(tekstZdania("seria:7"), "");
  // indeksZTekstu() wprost: kontrakt z wspolne.h
  int i = -7;
  SPRAWDZ(indeksZTekstu("3", 5, i) && i == 3);
  SPRAWDZ(indeksZTekstu("00", 5, i) && i == 0);
  SPRAWDZ(indeksZTekstu("4", 5, i) && i == 4);
  i = -7;
  SPRAWDZ(!indeksZTekstu("5", 5, i) && i == -7);  // wynik bez zmian przy błędzie
  SPRAWDZ(!indeksZTekstu("0", 0, i) && !indeksZTekstu("0", -1, i) && !indeksZTekstu(nullptr, 5, i));
  SPRAWDZ(indeksZTekstu("2147483646", 2147483647, i) && i == 2147483646);
  SPRAWDZ(!indeksZTekstu("2147483647", 2147483647, i) && !indeksZTekstu("-0", 5, i) && !indeksZTekstu("3 ", 5, i));
}

// Pole `klucz` z `/humor.csv` to też liczba z zewnątrz. Klucz poza
// zakresem (2147483647, bardzo długi, ujemny, nieliczbowy) nie wchodzi do licznika wyświetleń S2a/S2c ani do
// tytułu kamienia: bez przepełnienia `int` (UBSan: humor.cpp `licznikAC + 1`) i bez liczby spoza pliku na ekranie.
static void testKluczZPliku() {
  StanDnia s;
  s.dzis = "2026-11-03";
  s.nocyWaznych = 30;
  s.maNorme = true;
  s.nocDzis = true;
  s.nocDzisPonad4h = true;
  s.nocyPonad4h = 50;
  for (const char* zly : {"2147483647", "99999999999999999999", "-5", "abc"}) {
    WpisHumoru w;
    SPRAWDZ(parsujHumor(std::string("2026-11-01;S2A;") + zly + ";D;D01;seria:0", w));
    EkranDnia e = wybierzEkranDnia(s, {w});
    ROWNE(e.typ, "S2A");
    ROWNE(e.klucz, "1");  // zły klucz pominięty: pierwsze wyświetlenie
    WpisHumoru dzis;  // ten sam dzień i typ z kluczem spoza zakresu: nowy numer, nie przepełnienie
    SPRAWDZ(parsujHumor(std::string("2026-11-03;S2A;") + zly + ";D;D01;seria:0", dzis));
    ROWNE(wybierzEkranDnia(s, {w, dzis}).klucz, "1");
  }
  WpisHumoru dobry;  // poprawne klucze liczą się jak dotąd
  SPRAWDZ(parsujHumor("2026-11-01;S2A;4;D;D01;seria:0", dobry));
  ROWNE(wybierzEkranDnia(s, {dobry}).klucz, "5");

  std::vector<Noc> h;
  for (int i = 0; i < 20; i++) h.push_back(noc(dodajDni("2026-10-28", -i), 420, 1.0f, 10));
  DaneDnia d;
  d.dzis = "2026-10-28";
  d.hist = &h;
  d.norma = liczNorme(h, d.dzis, {});
  ROWNE(trescDnia("KM", "100", d).tytul, "100 nocy z maską.");
  std::string zwykly = trescDnia("S2A", "", d).tytul;
  for (const char* zly : {"2147483648", "-5", "abc", ""})  // kamień z uszkodzonym kluczem: zwykły ekran dnia
    ROWNE(trescDnia("KM", zly, d).tytul, zwykly);
}

int main() {
  testDaty();
  testNoc();
  testCzynniki();
  testKokpit();
  testHumor();
  testNawigacja();
  testEkrany();
  testDzien();
  testAsv();
  testUstawienia();
  testDoba();
  testNocZnana();
  testDobranocLogika();
  testDobranocDoby();
  testAirBridge();
  testStany();
  testTytulINaglowek();
  testPorownanie();
  testSypialnia();
  testKroje();
  testPrzypadkiBrzegowe();
  testPrzypadkiBrzegoweDalsze();
  testSkalaB();
  testU05WedlugKanwy();
  testOtoczenie();
  testBateria();
  testNumerZdaniaZPliku();
  testKluczZPliku();
  printf("testy: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
