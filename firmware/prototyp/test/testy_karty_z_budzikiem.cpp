// SPDX-License-Identifier: GPL-3.0-or-later
// Testy „Karty z budzikiem” (spec funkcjonalna 5.7.3–5.7.8,
// R-HUM-*): komplet scen 5.7.4 w pamięci, tabela 5.7.3 dla wszystkich ekranów z kartą (src/humor.h typyKart(),
// ekranKarty()), limity kart (R-HUM-02, R-HUM-23), scen (R-HUM-10, R-HUM-11) i pul (R-HUM-27), przerywnik S2d
// (wybierzPrzerywnik, R-HUM-14…R-HUM-16, R-HUM-21, R-HUM-22, R-HUM-26) i dziennik `/dane/humor.csv` (magazyn).
// Przypadki z kolumny „Weryfikacja” spec, gdzie się da dosłownie. Uruchomienie: tools/prototyp/testy.sh.
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "ekrany.h"
#include "humor.h"
#include "magazyn.h"
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

static WpisHumoru wpis(const std::string& d, const std::string& t, const std::string& k, char tryb,
                       const std::string& sc = "", const std::string& z = "") {
  return {d, t, k, tryb, sc, z};
}

static int znaki(const std::string& s) {  // znaki UTF-8, nie bajty
  int n = 0;
  for (unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}

static KontekstHumoru kontekst(const std::string& dzis, bool propozycje) {
  KontekstHumoru k;
  k.dzis = dzis;
  k.sceny = sceny();
  k.zPropozycjami = propozycje;
  return k;
}

// Wybór karty jak dyrygent: ekran, tryb, wpis w dzienniku.
static Wybor pokaz(const EkranDnia& e, std::vector<WpisHumoru>& log, const KontekstHumoru& k) {
  Wybor w = wybierzTryb(e, true, log, k);
  log.push_back(wpis(k.dzis, e.typ, e.klucz, w.tryb, w.scena, w.zdanie));
  return w;
}

// 5.7.4: biblioteka 70 scen (68 + B04, B05) w pamięci panelu; każda scena z tabeli 5.7.3 i przerywników wczytywalna.
static void testKompletScen() {
  const std::pair<char, int> GRUPY[] = {{'B', 5}, {'D', 8}, {'G', 18}, {'M', 8}, {'O', 4}, {'P', 11}, {'S', 4}, {'W', 12}};
  int n = 0, brak = 0;
  for (auto& g : GRUPY)
    for (int i = 1; i <= g.second; i++) {
      char id[4] = {g.first, (char)('0' + i / 10), (char)('0' + i % 10), 0};
      n++;
      if (!scenaJest(id)) { brak++; printf("  brak sceny %s\n", id); }
    }
  SPRAWDZ(n == 70 && brak == 0);
  SPRAWDZ(sceny().size() == 70);
  for (auto& t : typyKart()) {
    SPRAWDZ(pula(t.pula) != nullptr);
    for (auto s : t.sceny) SPRAWDZ(scenaJest(s));
  }
  for (auto kl : {"7", "30", "100", "180", "365"})
    for (auto& s : scenyTypu("KM", kl)) SPRAWDZ(scenaJest(s));
  ROWNE(scenyTypu("KM", "365")[0], "M05");
  for (auto o : {"O-03", "O-04", "O-06", "O-07", "O-08", "O-09", "O-12/S"}) {
    auto s = scenyTypu("S1", o);
    SPRAWDZ(s.size() == 1 && scenaJest(s[0]));
  }
  ROWNE(scenyTypu("S1", "O-04")[0], "O02");
  ROWNE(scenyTypu("S1", "O-09")[0], "O03");
  ROWNE(scenyTypu("S1", "O-06")[0], "O04");
  ROWNE(scenyTypu("S1", "O-03")[0], "M08");
  ROWNE(scenyTypu("S1", "O-12/S")[0], "O01");
  ROWNE(scenyTypu("SK27", "")[0], "M02");
  // skutki z README znikają: K22 ma 8 scen, kamień 365 nocy ma M05
  SPRAWDZ(scenyTypu("S2A", "").size() == 8 && scenyTypu("S2C", "").size() == 8);
}

// 5.7.5, R-HUM-12: każda pula typu ekranu z kartą istnieje, zdania ≤ 45 znaków, numery z dziennika wracają do tekstu.
static void testPule() {
  int pul = 0;
  for (auto id : {"dobra_noc", "seria", "kamien", "synchronizacja", "tydzien", "powrot", "wyciek", "przerwy", "zuzycie",
                  "po_wymianie", "wietrzenie", "smog", "chlod", "suche_powietrze", "upal", "pylek", "stala_pora",
                  "z_maska", "weekend", "odkrycie", "brak_danych", "przerywnik", "przerywnik_sceny"}) {
    const Pula* p = pula(id);
    SPRAWDZ(p != nullptr);
    if (!p) continue;
    pul++;
    int i = 0;
    for (auto z : p->kanwa) { SPRAWDZ(znaki(z) <= 45); ROWNE(tekstZdania(std::string(id) + ":" + std::to_string(i++)), z); }
    for (auto z : p->propozycje) { SPRAWDZ(znaki(z) <= 45); ROWNE(tekstZdania(std::string(id) + ":" + std::to_string(i++)), z); }
  }
  SPRAWDZ(pul == 23);
  SPRAWDZ(pula("przerywnik_sceny")->propozycje.size() == 18 && pula("przerywnik_sceny")->kanwa.empty());
  ROWNE(tekstZdania("przerywnik_sceny:11"), "Przebrałem się za konkurencję. Wyszło lepiej.");  // G12
  ROWNE(tekstZdania("przerywnik:2"), "Maska trzyma. Płetwy zbędne.");                     // K20, G12
  ROWNE(tekstZdania("wyciek:0"), "Syczy. To nie ja, sprawdzałem.");
  // istniejące numery z dziennika (dobra_noc, seria, kamien, brak_danych, synchronizacja) bez zmian
  ROWNE(tekstZdania("dobra_noc:0"), "Kawa dla mnie, wyniki dla ciebie.");
  ROWNE(tekstZdania("synchronizacja:4"), "Aparat oddaje noc. Ja podaję rękę.");
}

// R-HUM-27 (weryfikacja): przed akceptacją pul S-K-18 (1 zdanie) → ekran danych; S-K-07 (0 zdań) → ekran danych;
// S-K-10 (4 zdania) → karta 1 na 2 (R-HUM-07); po akceptacji (propozycje) S-K-18 kartą.
static void testR_HUM_27() {
  KontekstHumoru k = kontekst("2026-11-15", false);  // niedziela
  std::vector<WpisHumoru> log;
  EkranDnia e = ekranKarty("SK18", "2026-11-15", k.dzis, log, "");
  SPRAWDZ(e.kartaZTabeli);
  Wybor w = wybierzTryb(e, true, log, k);
  SPRAWDZ(w.tryb == 'D');
  ROWNE(w.dlaczego, "pula < 3 zdania (R-HUM-27)");
  e = ekranKarty("SK07", "2026-11-15", k.dzis, log, "");
  SPRAWDZ(e.kartaZTabeli && wybierzTryb(e, true, log, k).tryb == 'D');
  e = ekranKarty("SK10", "2026-11-10", k.dzis, {wpis("2026-11-14", "SK10", "2026-11-10#1", 'D')}, "");
  SPRAWDZ(e.klucz == "2026-11-10#2" && e.kartaZTabeli);
  w = wybierzTryb(e, true, log, k);
  SPRAWDZ(w.tryb == 'K' && w.scena == "P01" && w.zdanie == "wyciek:0");
  k.zPropozycjami = true;
  e = ekranKarty("SK18", "2026-11-15", k.dzis, log, "");
  w = wybierzTryb(e, true, log, k);
  SPRAWDZ(w.tryb == 'K' && w.scena == "M06");
  // przerwy S-K-16 bez zdań z kanwy: tylko z propozycjami
  e = ekranKarty("SK16", "x", k.dzis, {wpis("2026-11-14", "SK16", "x#1", 'D')}, "");
  SPRAWDZ(wybierzTryb(e, true, log, k).tryb == 'K');
  k.zPropozycjami = false;
  SPRAWDZ(wybierzTryb(e, true, log, k).tryb == 'D');
}

// R-HUM-07 (weryfikacja): S-K-10 trzy razy w ciągu 10 dni przy trwającym warunku → dane, karta, dane; S-K-01 trzy
// razy → karta, dane, karta; nowa sprawa → licznik od nowa; ten sam dzień zostaje przy swoim numerze.
static void testR_HUM_07() {
  KontekstHumoru k = kontekst("2026-11-01", true);
  std::vector<WpisHumoru> log;
  std::string tryby;
  for (int d : {0, 4, 9}) {
    k.dzis = dodajDni("2026-11-01", d);
    EkranDnia e = ekranKarty("SK10", "2026-10-31", k.dzis, log, "");
    tryby += pokaz(e, log, k).tryb;
    SPRAWDZ(ekranKarty("SK10", "2026-10-31", k.dzis, log, "").klucz == e.klucz);  // ten sam dzień: ten sam numer
  }
  ROWNE(tryby, "DKD");
  k.dzis = "2026-11-30";  // sprawa wygasła, nowy warunek = nowa sprawa: znowu ekran danych
  SPRAWDZ(!ekranKarty("SK10", "2026-11-29", k.dzis, log, "").kartaZTabeli);
  log.clear();
  tryby.clear();
  for (int d : {0, 4, 9}) {  // jedna scena W01: karta znowu dopiero po 6 dniach (R-HUM-10)
    k.dzis = dodajDni("2026-11-01", d);
    tryby += pokaz(ekranKarty("SK01", "2026-11-01", k.dzis, log, ""), log, k).tryb;
  }
  ROWNE(tryby, "KDK");
  // R-HUM-17 fail-path: wyświetlenie z linią uwagi liczy się jako ekran danych, następne jest kartą
  log = {wpis("2026-11-01", "SK10", "s#1", 'D'), wpis("2026-11-02", "SK10", "s#2", 'D')};  // #2 ustąpiło linii
  EkranDnia e = ekranKarty("SK10", "s", "2026-11-03", log, "");
  SPRAWDZ(e.klucz == "s#3" && e.kartaZTabeli);  // następne po ekranie danych jest kartą
  // klucz z separatorem pól nie trafia do dziennika
  SPRAWDZ(ekranKarty("SK10", "a;b", "2026-11-03", log, "").klucz.empty());
  SPRAWDZ(!ekranKarty("SK10", "a;b", "2026-11-03", log, "").kartaZTabeli);
}

// R-HUM-25 (weryfikacja): krok do lekarza i odkrycia wrażliwe → ekran danych; O-12 przy stresie → karta K13.
static void testR_HUM_25() {
  SPRAWDZ(krokDoLekarza("→ Powiedz o tym lekarzowi."));
  SPRAWDZ(krokDoLekarza("→ Pokaż to na kontroli."));
  SPRAWDZ(krokDoLekarza("→ Zatkany nos? Powiedz lekarzowi."));
  SPRAWDZ(!krokDoLekarza("→ Sam zdecyduj, co z tym zrobisz."));
  SPRAWDZ(!krokDoLekarza("→ Uchyl okno na noc."));
  std::vector<WpisHumoru> log;
  KontekstHumoru k = kontekst("2026-11-10", true);
  SPRAWDZ(!ekranKarty("S1", "O-12/T", k.dzis, log, "").kartaZTabeli);  // THC
  SPRAWDZ(!ekranKarty("S1", "O-12/A", k.dzis, log, "").kartaZTabeli);  // alkohol
  SPRAWDZ(!ekranKarty("S1", "O-12/L", k.dzis, log, "").kartaZTabeli);  // leki
  SPRAWDZ(!ekranKarty("S1", "O-12/K", k.dzis, log, "").kartaZTabeli);  // katar
  SPRAWDZ(!ekranKarty("S1", "O-10", k.dzis, log, "").kartaZTabeli);
  SPRAWDZ(!ekranKarty("S1", "O-11", k.dzis, log, "").kartaZTabeli);
  SPRAWDZ(!ekranKarty("S1", "O-05", k.dzis, log, "→ Pokaż to na kontroli.").kartaZTabeli);
  EkranDnia e = ekranKarty("S1", "O-12/S", k.dzis, log, "→ Sam zdecyduj, co z tym zrobisz.");
  Wybor w = wybierzTryb(e, true, log, k);
  SPRAWDZ(w.tryb == 'K' && w.scena == "O01");
  SPRAWDZ(!ekranKarty("S1", "O-12/S", k.dzis, log, "→ Powiedz o tym lekarzowi.").kartaZTabeli);
  w = wybierzTryb(ekranKarty("S1", "O-03", k.dzis, log, ""), true, log, k);
  SPRAWDZ(w.tryb == 'K' && w.scena == "M08");
  SPRAWDZ(!ekranKarty("SK21", "pylek", k.dzis, log, "").kartaZTabeli);
  w = wybierzTryb(ekranKarty("SK21", "co2", k.dzis, log, ""), true, log, k);
  SPRAWDZ(w.tryb == 'K' && w.scena == "O04");
}

// R-HUM-06 (weryfikacja): humor `oszczędny`, tydzień z S-K-18, S-K-01 i S-K-28 → karta tylko dla S-K-28.
static void testR_HUM_06() {
  KontekstHumoru k = kontekst("2026-11-15", true);
  k.humor = Humor::OSZCZEDNY;
  std::vector<WpisHumoru> log;
  SPRAWDZ(pokaz(ekranKarty("SK18", "2026-11-15", k.dzis, log, ""), log, k).tryb == 'D');
  k.dzis = "2026-11-16";
  SPRAWDZ(pokaz(ekranKarty("SK01", "2026-11-16", k.dzis, log, ""), log, k).tryb == 'D');
  k.dzis = "2026-11-17";
  EkranDnia km;
  km.typ = "KM";
  km.klucz = "100";
  km.kartaZTabeli = km.kartaOszczedny = true;
  SPRAWDZ(pokaz(km, log, k).tryb == 'K');
  SPRAWDZ(ekranKarty("S1", "O-04", k.dzis, log, "").kartaOszczedny);
  SPRAWDZ(ekranKarty("SK29", "x", k.dzis, log, "").kartaOszczedny && ekranKarty("SK27", "x", k.dzis, log, "").kartaOszczedny);
  SPRAWDZ(!ekranKarty("SK10", "x", k.dzis, log, "").kartaOszczedny);
  k.humor = Humor::WYLACZONY;  // R-HUM-03
  SPRAWDZ(wybierzTryb(km, true, log, k).tryb == 'D');
}

// R-HUM-02, R-HUM-23, R-HUM-10, R-HUM-11, R-HUM-17 dla ekranów S-K.
static void testLimity() {
  KontekstHumoru k = kontekst("2026-11-10", true);
  EkranDnia e = ekranKarty("SK29", "2026-11-10", k.dzis, {}, "");
  // R-HUM-02: dwie karty dziś → trzecia nie
  std::vector<WpisHumoru> log = {wpis("2026-11-10", "KM", "30", 'K', "M01", "kamien:0"),
                                 wpis("2026-11-10", "SK01", "a#1", 'K', "W01", "wietrzenie:0")};
  Wybor w = wybierzTryb(e, true, log, k);
  SPRAWDZ(w.tryb == 'D');
  ROWNE(w.dlaczego, "2 karty na dobę (R-HUM-02)");
  log.pop_back();
  SPRAWDZ(wybierzTryb(e, true, log, k).tryb == 'K');
  // R-HUM-23: 14 wyświetleń (13 + to) i 6 kart → karta (7 ≤ 7); przy 7 kartach już nie
  log.clear();
  for (int i = 0; i < 13; i++)
    log.push_back(wpis(dodajDni(k.dzis, -6 + i / 2), "S2A", std::to_string(i), i < 6 ? 'K' : 'D', i < 6 ? "X" : ""));
  SPRAWDZ(wybierzTryb(e, true, log, k).tryb == 'K');
  log.push_back(wpis(dodajDni(k.dzis, -1), "S2A", "x", 'K', "Y"));
  w = wybierzTryb(e, true, log, k);
  SPRAWDZ(w.tryb == 'D');
  ROWNE(w.dlaczego, "połowa wyświetleń w 7 dniach (R-HUM-23)");
  // R-HUM-23: poniedziałek po 7 dniach bez kart → karta dozwolona (1 ≤ 3,5)
  SPRAWDZ(wybierzTryb(e, true, {}, k).tryb == 'K');
  // R-HUM-10 (weryfikacja): S-K-10 jako karta dwa razy w 5 dni → raz raz 
  log = {wpis("2026-11-06", "SK10", "s#2", 'K', "P01", "wyciek:0"), wpis("2026-11-08", "SK10", "s#3", 'D')};
  e = ekranKarty("SK10", "s", k.dzis, log, "");
  w = wybierzTryb(e, true, log, k);
  SPRAWDZ(e.klucz == "s#4" && w.tryb == 'K' && w.scena == "P03" && w.zdanie == "wyciek:1");
  // R-HUM-10: S-K-18 co niedzielę → M06, D03, D06, M06 (M06 sprzed 7 dni wolno pokazać znowu)
  log.clear();
  std::string sek;
  for (int t = 0; t < 4; t++) {
    k.dzis = dodajDni("2026-11-01", 7 * t);  // niedziele
    sek += pokaz(ekranKarty("SK18", k.dzis, k.dzis, log, ""), log, k).scena + " ";
  }
  ROWNE(sek, "M06 D03 D06 M06 ");
  // R-HUM-11: jedyna scena S-K-29 (M07) pokazana 3 dni temu → ekran danych; scena spoza pamięci → ekran danych
  k.dzis = "2026-11-10";
  log = {wpis("2026-11-07", "SK29", "x", 'K', "M07", "powrot:0")};
  w = wybierzTryb(ekranKarty("SK29", "y", k.dzis, log, ""), true, log, k);
  SPRAWDZ(w.tryb == 'D');
  ROWNE(w.dlaczego, "brak sceny (R-HUM-10, R-HUM-11)");
  // R-HUM-17: linia uwagi → ekran danych także przy „karta zawsze”
  k.liniaUwagi = true;
  SPRAWDZ(wybierzTryb(ekranKarty("SK29", "y", k.dzis, {}, ""), true, {}, k).tryb == 'D');
  k.liniaUwagi = false;
  // R-HUM-05: tekst się nie mieści → ekran danych
  SPRAWDZ(wybierzTryb(ekranKarty("SK29", "y", k.dzis, {}, ""), false, {}, k).tryb == 'D');
}

// R-HUM-11 i 5.7.8 „brak pliku sceny w pamięci panelu i w zapasie wbudowanym”: ekran danych.
static void testBrakPlikuSceny() {
  KontekstHumoru k = kontekst("2026-11-10", true);
  std::vector<std::string> bez;
  for (auto& s : sceny())
    if (s != "B04" && s != "M05" && s != "P01" && s != "P03") bez.push_back(s);
  k.sceny = bez;
  EkranDnia s5{"S5", "1", true, false};
  Wybor w = wybierzTryb(s5, true, {}, k);  // K16 bez B04
  SPRAWDZ(w.tryb == 'D');
  ROWNE(w.dlaczego, "brak sceny (R-HUM-10, R-HUM-11)");
  EkranDnia km{"KM", "365", true, true};
  SPRAWDZ(wybierzTryb(km, true, {}, k).tryb == 'D');
  EkranDnia e = ekranKarty("SK10", "s", k.dzis, {wpis("2026-11-09", "SK10", "s#1", 'D')}, "");
  SPRAWDZ(e.kartaZTabeli && wybierzTryb(e, true, {}, k).tryb == 'D');
  k.sceny = sceny();  // z kompletem: karta
  SPRAWDZ(wybierzTryb(s5, true, {}, k).tryb == 'K' && wybierzTryb(km, true, {}, k).tryb == 'K');
  // R-HUM-08 (weryfikacja): K16 pokazaną w ostatnich 7 dniach, nowy brak danych → ekran danych
  std::vector<WpisHumoru> log = {wpis("2026-11-07", "S5", "0", 'K', "B04", "brak_danych:0")};
  SPRAWDZ(wybierzTryb(s5, true, log, k).tryb == 'D');
}

// R-HUM-26: aktywny wzorzec B → S2a i S2c bez K22, S2b dalej K01; bez przerywnika.
static void testR_HUM_26() {
  KontekstHumoru k = kontekst("2026-11-10", true);
  k.wzorzecB = true;
  EkranDnia a{"S2A", "3", true, false}, b{"S2B", "", true, false}, c{"S2C", "6", true, false};
  Wybor w = wybierzTryb(a, true, {}, k);
  SPRAWDZ(w.tryb == 'D');
  ROWNE(w.dlaczego, "wzorzec B (R-HUM-26)");
  SPRAWDZ(wybierzTryb(c, true, {}, k).tryb == 'D');
  SPRAWDZ(wybierzTryb(b, true, {}, k).tryb == 'K');
  UstawieniaPrzerywnikow u;
  u.tekstyPrzyjete = true;
  SPRAWDZ(!wybierzPrzerywnik(a, {}, k, u).jest);
  k.wzorzecB = false;
  SPRAWDZ(wybierzTryb(a, true, {}, k).tryb == 'K' && wybierzPrzerywnik(a, {}, k, u).jest);
}

// S10 (R-EKR-57): ekran przejściowy poza limitami kart i limitem scen.
static void testS10PozaLimitami() {
  KontekstHumoru k = kontekst("2026-11-10", true);
  std::vector<WpisHumoru> log = {wpis("2026-11-10", "KM", "30", 'K', "M01"), wpis("2026-11-10", "SK29", "x", 'K', "M07"),
                                 wpis("2026-11-08", "S10", "", 'K', "B05")};
  EkranDnia s10{"S10", "", true, true};
  Wybor w = wybierzTryb(s10, true, log, k);
  SPRAWDZ(w.tryb == 'K' && w.scena == "B05");
}

// Przerywnik S2d: (teksty nieprzyjęte) → nigdy; mechanizm po przyjęciu tekstów według 5.7.6.
static void testPrzerywnik() {
  KontekstHumoru k = kontekst("2026-11-02", true);  // poniedziałek
  EkranDnia dowod{"S2A", "1", false, false};
  UstawieniaPrzerywnikow u;  // domyślnie: teksty nieprzyjęte
  Przerywnik p = wybierzPrzerywnik(dowod, {}, k, u);
  SPRAWDZ(!p.jest);
  ROWNE(p.dlaczego, "teksty przerywników nieprzyjęte");
  u.tekstyPrzyjete = true;
  // R-HUM-14 (weryfikacja), wydanie 1.x bez scen z datami: 14 spokojnych dni od poniedziałku → przerywnik w dniach 1
  // i 8, scena z puli G10, G12, G13, G14, w dniu 8 inna niż w dniu 1
  u.ogolne = {"G10", "G12", "G13", "G14"};
  std::vector<WpisHumoru> log;
  std::vector<int> dni;
  std::vector<std::string> sc;
  for (int d = 0; d < 14; d++) {
    k.dzis = dodajDni("2026-11-02", d);
    p = wybierzPrzerywnik(dowod, log, k, u);
    if (p.jest) {
      dni.push_back(d + 1);
      sc.push_back(p.scena);
      log.push_back(wpis(k.dzis, "S2D", p.klucz, 'K', p.scena, p.zdanie));
      Przerywnik znowu = wybierzPrzerywnik(dowod, log, k, u);  // ten sam dzień: ten sam przerywnik
      SPRAWDZ(znowu.jest && znowu.scena == p.scena && znowu.zdanie == p.zdanie);
    } else {
      log.push_back(wpis(k.dzis, "S2A", std::to_string(d), 'D'));
      SPRAWDZ(!wybierzPrzerywnik(dowod, log, k, u).jest);  // dzień z dowodem spokoju zostaje dowodem spokoju
    }
  }
  SPRAWDZ(dni.size() == 2 && dni[0] == 1 && dni[1] == 8);
  // scena losowana z dozwolonych dziś, więc bez stałej kolejności; dwie różne sceny z puli
  SPRAWDZ(sc.size() == 2 && sc[0] != sc[1] && sc[0] >= "G10" && sc[1] >= "G10" && sc[0] <= "G14" && sc[1] <= "G14");
  SPRAWDZ(!tekstZdania(log[0].zdanie).empty());  // zdanie przypięte do wylosowanej sceny
  // wydanie ze scenami z datami: przerywnik w dniu 1 i 8, G02 tylko kandydatem we wspólnej puli,
  // więc dwie różne sceny (R-HUM-10)
  u.zDatami = true;
  log.clear();
  sc.clear();
  for (int d = 0; d < 14; d++) {
    k.dzis = dodajDni("2026-11-02", d);
    p = wybierzPrzerywnik(dowod, log, k, u);
    if (p.jest) {
      sc.push_back(std::to_string(d + 1) + p.scena);
      log.push_back(wpis(k.dzis, "S2D", p.klucz, 'K', p.scena, p.zdanie));
    } else {
      log.push_back(wpis(k.dzis, "S2A", std::to_string(d), 'D'));
    }
  }
  SPRAWDZ(sc.size() == 2 && sc[0][0] == '1' && sc[1][0] == '8' && sc[0].substr(1) != sc[1].substr(1));
  SPRAWDZ(sc.size() == 2 && sc[0].substr(1) != "G03" && sc[1].substr(1) != "G03");
  // R-HUM-15 (weryfikacja): 1 stycznia, przerywnik 3 dni wcześniej → G04; 25 grudnia → G05; 23 września → G08;
  // 31.10.2027 (jesienna zmiana czasu) → G01;: 28.03.2027 (wiosenna zmiana czasu i Wielkanoc) → G06, a wiosenna
  // zmiana czasu sama (26.03.2028) bez okazji, bo neutralna G01 mówi „zyskaliśmy godzinę”
  ROWNE(wielkanoc(2026), "2026-04-05");
  ROWNE(wielkanoc(2027), "2027-03-28");
  ROWNE(okazjaDnia("2027-03-28"), "G01");  // Wielkanoc i zmiana czasu: wygrywa niższe ID (5.7.6)
  ROWNE(okazjaDnia("2028-03-26"), "G01");
  ROWNE(okazjaDnia("2028-04-16"), "G06");
  ROWNE(okazjaDnia("2027-10-31"), "G01");
  ROWNE(okazjaDnia("2026-10-25"), "G01");
  ROWNE(okazjaDnia("2026-10-18"), "");
  ROWNE(okazjaDnia("2026-04-05"), "G06");
  log = {wpis("2026-12-29", "S2D", "L", 'K', "G10", "przerywnik_sceny:9")};
  k.dzis = "2027-01-01";
  p = wybierzPrzerywnik(dowod, log, k, u);
  SPRAWDZ(p.jest && p.scena == "G04" && p.klucz == "O");
  ROWNE(p.okazja, "Nowy Rok");
  k.dzis = "2026-12-25";
  SPRAWDZ(wybierzPrzerywnik(dowod, {}, k, u).scena == "G05");
  k.dzis = "2026-09-23";
  SPRAWDZ(wybierzPrzerywnik(dowod, {}, k, u).scena == "G08");
  k.dzis = "2027-03-28";
  SPRAWDZ(wybierzPrzerywnik(dowod, {}, k, u).scena == "G01");
  k.dzis = "2027-10-31";
  SPRAWDZ(wybierzPrzerywnik(dowod, {}, k, u).scena == "G01");
  // okazja w limitach kart: 2 karty dziś → bez przerywnika, okazja przepada
  k.dzis = "2027-01-01";
  log = {wpis(k.dzis, "KM", "30", 'K', "M01"), wpis(k.dzis, "SK29", "x", 'K', "M07")};
  SPRAWDZ(!wybierzPrzerywnik(dowod, log, k, u).jest);
  // wydanie bez scen z datami: 1 stycznia → przerywnik z limitu, nie G04
  u.zDatami = false;
  p = wybierzPrzerywnik(dowod, {}, k, u);
  SPRAWDZ(p.jest && p.scena != "G04" && p.klucz == "L");
  // R-HUM-22: linia uwagi → bez przerywnika (dowód spokoju z linią); R-HUM-16: oszczędny i wyłączony → bez przerywnika
  k.liniaUwagi = true;
  SPRAWDZ(!wybierzPrzerywnik(dowod, {}, k, u).jest);
  k.liniaUwagi = false;
  k.humor = Humor::OSZCZEDNY;
  SPRAWDZ(!wybierzPrzerywnik(dowod, {}, k, u).jest);
  k.humor = Humor::WYLACZONY;
  SPRAWDZ(!wybierzPrzerywnik(dowod, {}, k, u).jest);
  k.humor = Humor::NORMALNY;
  // przerywnik tylko w miejscu dowodu spokoju: nie wypiera S5, kamienia ani rozruchu
  for (auto t : {"S5", "KM", "S6", "S10"}) {
    EkranDnia inny{t, "", true, true};
    SPRAWDZ(!wybierzPrzerywnik(inny, {}, k, u).jest);
  }
  SPRAWDZ(wybierzPrzerywnik(EkranDnia{"S2B", "", true, false}, {}, k, u).jest);
  // R-HUM-27: bez propozycji tylko sceny z kanwy (K20 dla G12) → G12; bez sceny ze zdaniem → dowód spokoju
  k.zPropozycjami = false;
  p = wybierzPrzerywnik(dowod, {}, k, u);
  SPRAWDZ(p.jest && p.scena == "G12" && p.zdanie == "przerywnik:2");
  u.ogolne = {"G10", "G13", "G14"};
  p = wybierzPrzerywnik(dowod, {}, k, u);
  SPRAWDZ(!p.jest);
  ROWNE(p.dlaczego, "brak sceny ze zdaniem (R-HUM-10, R-HUM-11, R-HUM-27)");
  // domyślna pula ogólna G09–G18 (wszystkie sceny w pamięci), scena spoza 6 poprzednich dni
  k.zPropozycjami = true;
  u.ogolne.clear();
  log = {wpis("2026-12-20", "SK01", "a#1", 'K', "G09")};  // G09 pokazana 6 dni temu (dowolny ekran)
  k.dzis = "2026-12-26";
  p = wybierzPrzerywnik(dowod, log, k, u);
  SPRAWDZ(p.jest && p.scena != "G09" && p.scena >= "G10");  // losowana z dozwolonych, bez G09
  // scena przerywnika poza pamięcią → inna z dozwolonych
  std::vector<std::string> bez;
  for (auto& s : sceny())
    if (s != "G09" && s != "G10") bez.push_back(s);
  k.sceny = bez;
  std::string inna = wybierzPrzerywnik(dowod, {}, k, u).scena;
  SPRAWDZ(inna >= "G11" && inna <= "G18");
}

// Archiwum 30 dni (R-HUM-02, R-HUM-23 weryfikacja): rotacja dnia, przerywnik, wieczór S-K-01 i rano S-K-10 co drugi
// dzień; w każdej dobie ≤ 2 karty, w żadnym oknie 7 dni karty nie przekraczają 0,5 × max(wyświetlenia, 7);
// scena karty nie wraca w 6 dni (R-HUM-10), poza S10.
static void testArchiwum30Dni() {
  KontekstHumoru k = kontekst("2026-11-02", true);
  UstawieniaPrzerywnikow u;
  u.tekstyPrzyjete = true;
  StanDnia s;
  s.nocDzis = s.nocDzisPonad4h = true;
  s.nocyWaznych = 60;
  s.maNorme = true;
  s.seria = 12;
  s.nocyPonad4h = 50;
  s.sypialnia = true;
  std::vector<WpisHumoru> log;
  for (int d = 0; d < 30; d++) {
    k.dzis = s.dzis = dodajDni("2026-11-02", d);
    EkranDnia e = wybierzEkranDnia(s, log);
    Przerywnik p = wybierzPrzerywnik(e, log, k, u);
    if (p.jest) log.push_back(wpis(k.dzis, "S2D", p.klucz, 'K', p.scena, p.zdanie));
    else pokaz(e, log, k);
    if (d % 2 == 0) pokaz(ekranKarty("SK10", "2026-11-01", k.dzis, log, ""), log, k);
    pokaz(ekranKarty("SK01", "2026-11-02", k.dzis, log, ""), log, k);
  }
  std::map<std::string, int> kart, wysw;
  for (auto& x : log) {
    wysw[x.data]++;
    if (x.tryb == 'K') kart[x.data]++;
  }
  bool dobaOk = true, oknoOk = true, scenaOk = true;
  for (auto& kv : kart) dobaOk = dobaOk && kv.second <= 2;
  for (int d = 0; d < 30; d++) {
    std::string dzis = dodajDni("2026-11-02", d), od = dodajDni(dzis, -6);
    int k7 = 0, w7 = 0;
    for (auto& x : log)
      if (x.data >= od && x.data <= dzis) { w7++; k7 += x.tryb == 'K'; }
    oknoOk = oknoOk && k7 <= 0.5 * std::max(w7, 7);
  }
  for (size_t i = 0; i < log.size(); i++)
    for (size_t j = 0; j < i; j++)
      if (log[i].tryb == 'K' && log[j].tryb == 'K' && log[i].scena == log[j].scena && log[i].data != log[j].data &&
          nrDnia(log[i].data) - nrDnia(log[j].data) < 7)
        scenaOk = false;
  SPRAWDZ(dobaOk);
  SPRAWDZ(oknoOk);
  SPRAWDZ(scenaOk);
  int przer = 0, karty = 0;
  for (auto& x : log) { przer += x.typ == "S2D"; karty += x.tryb == 'K'; }
  SPRAWDZ(przer >= 3 && przer <= 5);  // 1 na 7 dni, gdy limity pozwalają
  SPRAWDZ(karty > 10);
}

// Dziennik wyświetleń w magazynie `/dane/humor.csv`, dopisywany; nowe typy i klucze przechodzą przez format.
static void testDziennik() {
  const FormatPliku* f = formatPliku(rejestrFormatow(), "humor");
  SPRAWDZ(f != nullptr);
  if (!f) return;
  ROWNE(f->sciezka, "/dane/humor.csv");
  SPRAWDZ(f->rodzaj == RodzajZapisu::DOPISANIE);
  for (auto& w : {wpis("2026-11-02", "S2D", "L", 'K', "G10", "przerywnik_sceny:9"),
                  wpis("2026-11-03", "SK10", "2026-11-01#2", 'K', "P01", "wyciek:0"),
                  wpis("2026-11-04", "S1", "O-12/S", 'K', "O01", "odkrycie:0"),
                  wpis("2026-11-05", "SK18", "2026-11-05", 'D', "", "")}) {
    WpisHumoru r;
    SPRAWDZ(parsujHumor(zapiszHumor(w), r));
    SPRAWDZ(r.typ == w.typ && r.klucz == w.klucz && r.tryb == w.tryb && r.scena == w.scena && r.zdanie == w.zdanie);
  }
}

// Poprawki ton „problem” (R-HUM-24) w wyniku, S2D i S5 bez karty z ekranKarty, wybór powtarzalny.
static void testTonProblemBezKartyIPowtarzalnosc() {
  SPRAWDZ(ekranKarty("SK10", "s", "2026-11-10", {}, "").tonProblem);
  SPRAWDZ(ekranKarty("SK16", "s", "2026-11-10", {}, "").tonProblem && ekranKarty("SK31", "s", "2026-11-10", {}, "").tonProblem);
  SPRAWDZ(!ekranKarty("SK01", "s", "2026-11-10", {}, "").tonProblem && !ekranKarty("SK29", "s", "2026-11-10", {}, "").tonProblem);
  for (auto& t : typyKart()) {
    std::string n = t.typ;
    SPRAWDZ(t.problem == (n == "SK10" || n == "SK16" || n == "SK31"));
  }
  SPRAWDZ(!ekranKarty("S2D", "L", "2026-11-10", {}, "").kartaZTabeli);
  SPRAWDZ(!ekranKarty("S5", "1", "2026-11-10", {}, "").kartaZTabeli);  // S5 tylko z wybierzEkranDnia
  KontekstHumoru k = kontekst("2026-11-10", true);
  EkranDnia s2d{"S2D", "L", true, false};
  Wybor w = wybierzTryb(s2d, true, {}, k);
  SPRAWDZ(w.tryb == 'D');
  // ponowna ocena wyświetlenia z wpisem z dziś: ten sam tryb, scena i zdanie (także ze zdaniem najdawniej użytym)
  std::vector<WpisHumoru> log = {wpis("2026-11-10", "KM", "30", 'K', "M01", "kamien:0")};
  EkranDnia e = ekranKarty("SK29", "x", k.dzis, log, "");
  Wybor w1 = wybierzTryb(e, true, log, k);
  log.push_back(wpis(k.dzis, e.typ, e.klucz, w1.tryb, w1.scena, w1.zdanie));
  Wybor w2 = wybierzTryb(e, true, log, k);
  SPRAWDZ(w1.tryb == 'K' && w2.tryb == 'K' && w1.scena == w2.scena && w1.zdanie == w2.zdanie);
}

int main() {
  testTonProblemBezKartyIPowtarzalnosc();
  testKompletScen();
  testPule();
  testR_HUM_27();
  testR_HUM_07();
  testR_HUM_25();
  testR_HUM_06();
  testLimity();
  testBrakPlikuSceny();
  testR_HUM_26();
  testS10PozaLimitami();
  testPrzerywnik();
  testArchiwum30Dni();
  testDziennik();
  printf("testy_karty_z_budzikiem: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
