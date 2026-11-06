// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych „Karty z budzikiem”. Testy rNN_… oznaczone
// „PADA” sprawdzają zachowania, które mają przejść po poprawce bez zmiany oczekiwań.
// Testy „TRZYMA” pilnują rzeczy już sprawdzonych (regresja).
// Dane syntetyczne. Uruchomienie: tools/prototyp/testy.sh.
#include <cstdio>
#include <string>
#include <vector>
#include "ekrany.h"
#include "humor.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)

static WpisHumoru wpis(const std::string& d, const std::string& t, const std::string& k, char tryb,
                       const std::string& sc = "", const std::string& z = "") {
  return {d, t, k, tryb, sc, z};
}
static KontekstHumoru kontekst(const std::string& dzis) {
  KontekstHumoru k;
  k.dzis = dzis;
  k.sceny = sceny();
  k.zPropozycjami = true;
  return k;
}

// Przypadek 1: spec 5.7.8, wiersz R-HUM-17 „ekran z regułą karta 1 na 2 ustąpił linii uwagi:
// wyświetlenie liczy się w trybie, w jakim było (ekran danych); następne jest kartą (R-HUM-07)”. ekranKarty() liczy
// parzystość numeru wyświetlenia, a nie tryb poprzedniego wyświetlenia, więc karta, która ustąpiła linii, przepada na
// dwa wyświetlenia. Test autora testR_HUM_07() sprawdza odwrotnie niż jego własny komentarz (`!e.kartaZTabeli`).
static void R_HUM_17_problemPoLiniiNastepneKarta() {
  // S-K-10 (ton „problem”): #1 ekran danych z wykresem, #2 miało być kartą, ale była linia uwagi → zapisane 'D'
  std::vector<WpisHumoru> log = {wpis("2026-11-01", "SK10", "s#1", 'D'), wpis("2026-11-02", "SK10", "s#2", 'D')};
  EkranDnia e = ekranKarty("SK10", "s", "2026-11-03", log, "");
  SPRAWDZ(e.kartaZTabeli);  // następne po ekranie danych, który ustąpił linii, jest kartą
}
static void R_HUM_17_wieczorPoLiniiNastepneKarta() {
  // S-K-01: #1 miało być kartą, linia uwagi → 'D'; #2 ma być kartą
  std::vector<WpisHumoru> log = {wpis("2026-11-01", "SK01", "w#1", 'D')};
  EkranDnia e = ekranKarty("SK01", "w", "2026-11-02", log, "");
  SPRAWDZ(e.kartaZTabeli);
}

// Przypadek 2: bramka („do przyjęcia tekstów przerywniki nie są losowane”, 5.7.6, 5.7.8) siedzi tylko
// w wybierzPrzerywnik(). typyKart() ma S2D jako „karta zawsze” z pulą „przerywnik” (3 zdania z kanwy), więc ścieżka
// ekranKarty("S2D") → wybierzTryb() daje kartę przerywnika z dowolną sceną G i zdaniem nieprzypiętym do sceny, przy
// tekstach nieprzyjętych i przy aktywnym wzorcu B (R-HUM-26 w wybierzTryb() obejmuje tylko S2A i S2C).
static void S2DnieMozeBycKartaPozaPrzerywnikiem() {
  KontekstHumoru k = kontekst("2026-11-10");
  Wybor w = wybierzTryb(ekranKarty("S2D", "L", k.dzis, {}, ""), true, {}, k);
  SPRAWDZ(w.tryb == 'D');
  k.wzorzecB = true;
  w = wybierzTryb(ekranKarty("S2D", "L", k.dzis, {}, ""), true, {}, k);
  SPRAWDZ(w.tryb == 'D');
}

// Przypadek 3: wybierzTryb() nie jest stały w obrębie jednego wyświetlenia. Ponowna ocena tego samego
// wyświetlenia (ten sam typ i klucz, wpis z dziś już w dzienniku jako karta) liczy własny wpis do limitu R-HUM-02,
// więc druga karta doby przy odświeżeniu przechodzi w ekran danych. Dziś ratuje to planWyboru() tylko dla ekranu
// dnia; ekrany S-K z ekranKarty() (wieczór) nie mają tej ochrony.
static void r03_tenSamWybórPrzyOdswiezeniu() {
  KontekstHumoru k = kontekst("2026-11-10");
  std::vector<WpisHumoru> log = {wpis("2026-11-10", "KM", "30", 'K', "M01", "kamien:0")};
  EkranDnia e = ekranKarty("SK01", "w", k.dzis, log, "");
  Wybor w1 = wybierzTryb(e, true, log, k);
  SPRAWDZ(w1.tryb == 'K');
  log.push_back(wpis(k.dzis, e.typ, e.klucz, w1.tryb, w1.scena, w1.zdanie));
  Wybor w2 = wybierzTryb(ekranKarty("SK01", "w", k.dzis, log, ""), true, log, k);
  SPRAWDZ(w2.tryb == 'K' && w2.scena == w1.scena && w2.zdanie == w1.zdanie);
}

// TRZYMA: numery zdań istniejących pul (dziennik na panelu) bez zmian względem 187b2ab.
static void numeracjaStarychPul() {
  SPRAWDZ(tekstZdania("dobra_noc:8") == "Noc za nami. Wskazówki w komplecie.");
  SPRAWDZ(tekstZdania("seria:5") == "Noc za nocą. Nudno i dobrze.");
  SPRAWDZ(tekstZdania("kamien:3") == "Świeczki zdmuchnąłem. Maski nie.");
  SPRAWDZ(tekstZdania("kamien:7") == "Kolejna noc. Mój mechanizm się wzruszył.");
  SPRAWDZ(tekstZdania("brak_danych:7") == "Most milczy. Zadzwoniłbym, tylko do kogo?");
  SPRAWDZ(tekstZdania("synchronizacja:0") == "Wciągam noc z aparatu. Nie poganiaj.");
}

// TRZYMA: dziennik z wersji sprzed (typy S2A, S2B, S2C, S5, KM, sceny z 27) wczytany po restarcie: rotacja
// i licznik K22 idą dalej, a stare karty z D02 i D05 blokują te sceny przez 6 dni (R-HUM-10).
static void dziennikWersji0() {
  std::vector<std::string> linie = {"2026-11-01;S2A;3;K;D02;dobra_noc:0", "2026-11-02;S2B;;K;D05;seria:2",
                                    "2026-11-03;S2C;4;D;;", "2026-11-04;KM;30;D;;",
                                    "2026-11-05;S5;2026-11-04T06:00;D;;"};
  std::vector<WpisHumoru> log;
  for (auto& l : linie) {
    WpisHumoru w;
    SPRAWDZ(parsujHumor(l, w));
    log.push_back(w);
  }
  StanDnia s;
  s.dzis = "2026-11-06";  // piątek
  s.nocDzis = s.nocDzisPonad4h = true;
  s.nocyWaznych = 40;
  s.maNorme = true;
  s.seria = 3;
  s.nocyPonad4h = 31;
  s.sypialnia = true;
  EkranDnia e = wybierzEkranDnia(s, log);
  SPRAWDZ(e.typ == "S2A" && e.klucz == "5" && !e.kartaZTabeli);
  log.push_back(wpis(s.dzis, "S2A", "5", 'D'));
  s.dzis = "2026-11-07";
  e = wybierzEkranDnia(s, log);  // S2b bez serii → S2c, numer 6 = karta (R-HUM-09)
  SPRAWDZ(e.typ == "S2C" && e.klucz == "6" && e.kartaZTabeli);
  KontekstHumoru k = kontekst(s.dzis);
  Wybor w = wybierzTryb(e, true, log, k);
  SPRAWDZ(w.tryb == 'K' && w.scena != "D02" && w.scena != "D05");  // D02, D05 z 01–02.11: 6 poprzednich dni
}

// TRZYMA: przy domyślnych ustawieniach (nieprzyjęte) przez 60 dni nie ma przerywnika, a wybierzEkranDnia
// nigdy nie daje S2D.
static void przerywnikNicSieNieLosuje() {
  UstawieniaPrzerywnikow u;
  StanDnia s;
  s.nocDzis = s.nocDzisPonad4h = true;
  s.nocyWaznych = 60;
  s.maNorme = true;
  s.seria = 12;
  s.nocyPonad4h = 50;
  s.sypialnia = true;
  std::vector<WpisHumoru> log;
  int przer = 0, s2d = 0;
  for (int d = 0; d < 60; d++) {
    s.dzis = dodajDni("2026-11-02", d);
    KontekstHumoru k = kontekst(s.dzis);
    EkranDnia e = wybierzEkranDnia(s, log);
    s2d += e.typ == "S2D";
    przer += wybierzPrzerywnik(e, log, k, u).jest;
    Wybor w = wybierzTryb(e, true, log, k);
    log.push_back(wpis(s.dzis, e.typ, e.klucz, w.tryb, w.scena, w.zdanie));
  }
  SPRAWDZ(przer == 0 && s2d == 0);
}

int main() {
  R_HUM_17_problemPoLiniiNastepneKarta();
  R_HUM_17_wieczorPoLiniiNastepneKarta();
  S2DnieMozeBycKartaPozaPrzerywnikiem();
  r03_tenSamWybórPrzyOdswiezeniu();
  numeracjaStarychPul();
  dziennikWersji0();
  przerywnikNicSieNieLosuje();
  printf("testy_karty_z_budzikiem_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
