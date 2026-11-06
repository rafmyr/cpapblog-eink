// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (kolejka linii uwagi, ekran L, granice bezpieczeństwa: ADR-0029, 5.12.2, 5.14).
// Testy „trzyma” pilnują granic bezpieczeństwa.
//
// Atrapy i pomocnicze funkcje (dzien(), wLogu(), ZESTAW_D, KLUCZE_D, wynikiD()) pochodzą z testów podstawowych
// (testy_kolejka_linii.cpp) przez dołączenie pliku z podmienioną nazwą main(), jak test/testy_magazyn_brzegowe.cpp. Dane wyłącznie syntetyczne.
#define main testy_kolejka_linii_main
#include "testy_kolejka_linii.cpp"
#undef main

namespace {

sprawy::SprawaL sprawa(const std::string& klucz, Pilnosc p, const std::string& aktywnaOd,
                       StanReguly st = StanReguly::AKTYWNA) {
  sprawy::SprawaL s;
  s.id = klucz.substr(0, klucz.find('/'));
  s.klucz = klucz;
  if (klucz.find('/') != std::string::npos) s.wariant = klucz.substr(klucz.find('/') + 1);
  s.stan = st;
  s.pilnosc = st == StanReguly::TYLKO_RAPORT ? Pilnosc::BRAK : p;
  s.ton = st == StanReguly::AKTYWNA && (p == Pilnosc::A || p == Pilnosc::B) ? sprawy::Ton::LEKARZ : sprawy::Ton::BRAK;
  s.tytul = "Tytuł " + klucz;
  s.aktywnaOd = aktywnaOd;
  return s;
}

}  // namespace

// (pkt 1 i 2): „Pilność A jest zawsze widoczna” (5.12.2, R-SKI-22). Wzorzec A w dniu bez pełnego ekranu
// (dzień 1 po wejściu) i wzorzec B w dniu wejścia: sprawaEkranuDnia() wybiera ekran L wzorca B, a ekran L
// (DaneWzorca) nie ma linii uwagi, więc linia `Wciąż: {tytuł A}.` z grupy 0 nie trafia na ekran. A znika na cały dzień.
static void pilnoscAWidocznaGdyEkranemJestB() {
  std::vector<sprawy::SprawaL> s = {sprawa("L-06/b", Pilnosc::A, dodajDni(DZIS, -1)), sprawa("L-11", Pilnosc::B, DZIS)};
  const sprawy::SprawaL* e = sprawy::sprawaEkranuDnia(s, DZIS);
  SPRAWDZ(e == nullptr || e->pilnosc == Pilnosc::A);  // pada: ekranem dnia jest L-11 (B), bez miejsca na linię A
  printf("  r01: ekran dnia %s przy A w dniu bez pełnego ekranu\n", e ? e->klucz.c_str() : "(zwykły z linią)");
}

// (pkt 3 i 5): data wejścia `aktywnaOd` żyje tylko w RAM-ie jednego wybudzenia (dyrygent wpisuje dziś, gdy
// puste, i nie zapisuje; aktualizujPamiec() nie jest wołane). Każde wybudzenie to nowy Dyrygent (sen głęboki),
// więc następnego dnia znacznik buzzera `klucz@aktywnaOd` jest nowy: 3 sygnały co dzień zamiast raz (R-BUZ-04).
static void buzzerRazNaWejscieTakzeNastepnegoDnia() {
  sprawy::zrodloTestowe.wyniki = wynikiD;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_D;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_D);
  KLUCZE_D = {"L-06/b"};
  Panel p;
  dzien(p, 2026, 11, 4, 8, 30);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  ROWNE_L(p.dzwiek.piskow, 3);
  p.siec.odpowiedzi.clear();  // odpowiedzi dnia 1 niezużyte przez atrapę sieci
  dzien(p, 2026, 11, 5, 8, 30);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  if (getenv("RV_LOG")) for (auto& l : p.port.log) printf("    %s\n", l.c_str());
  printf("  r02: sygnałów po 2 dniach tego samego wejścia w A: %d\n", p.dzwiek.piskow);
  ROWNE_L(p.dzwiek.piskow, 3);  // pada: 6
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

// (pkt 2 i 3): ten sam brak trwałej `aktywnaOd`: wzorzec B ma pełny ekran w dniu wejścia, potem co 7 dni
// (5.12.2), a dostaje go każdego dnia, bo każdy dzień jest „dniem wejścia”. Dla A to pełny ekran codziennie zamiast
// co 3 dni z linią w pozostałe dni.
static void rytmEkranuBPoRestarcie() {
  sprawy::zrodloTestowe.wyniki = wynikiD;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_D;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_D);
  KLUCZE_D = {"L-11"};
  Panel p;
  dzien(p, 2026, 11, 4, 9, 0);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(wLogu(p, "ekran dnia: wzorzec L-11"));
  p.port.log.clear();
  p.siec.odpowiedzi.clear();  // odpowiedzi dnia 1 niezużyte przez atrapę sieci
  dzien(p, 2026, 11, 5, 9, 0);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  SPRAWDZ(!wLogu(p, "ekran dnia: wzorzec L-11"));  // pada: dzień 2 znowu pełny ekran B
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

// (pkt 1): na S5 (brak danych) dyrygent nie woła silnika (`e.typ != "S5"`), więc linia `Wciąż: {tytuł}.`
// wzorca A nie stoi na S5, wbrew 5.12.2 („także na ekranie braku danych S5”) i przypadkowi 1 z 7.8 (S5 z linią A).
static void liniaANaS5() {
  sprawy::zrodloTestowe.wyniki = wynikiD;
  sprawy::zrodloTestowe.zestaw = &ZESTAW_D;
  sprawy::zrodloTestowe.tekst = atrapa_b5::funkcja(ZESTAW_D);
  KLUCZE_D = {"L-06/b"};
  Panel p;
  dzien(p, 2026, 11, 4, 9, 0);
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  p.port.log.clear();
  p.siec.odpowiedzi.clear();  // odpowiedzi dnia 1 niezużyte przez atrapę sieci
  dzien(p, 2026, 11, 5, 9, 0);
  p.siec.siecJest = false;  // brak Wi-Fi: rano S10, po terminie importu S5 (E-01)
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  bool s10 = wLogu(p, "ekran dnia S10"), silnik = wLogu(p, "silnik reguł 2026-11-05");
  p.port.log.clear();
  dzien(p, 2026, 11, 5, 13, 0);
  p.siec.odpowiedzi.clear();
  p.siec.siecJest = false;
  Dyrygent(p.adaptery(), KONF).wybudzenie();
  if (getenv("RV_LOG")) for (auto& l : p.port.log) printf("    %s\n", l.c_str());
  bool s5 = wLogu(p, "S5:");
  printf("  r04: 09:00 S10 %s (silnik %s), 13:00 S5 %s (silnik %s)\n", s10 ? "tak" : "nie", silnik ? "tak" : "nie",
         s5 ? "tak" : "nie", wLogu(p, "silnik reguł") ? "tak" : "nie");
  SPRAWDZ(!s10 || silnik);                      // pada: S10 bez wyników silnika, więc bez linii A
  SPRAWDZ(!s5 || wLogu(p, "silnik reguł"));     // pada, gdy S5: bez wyników silnika, więc bez linii A
  sprawy::zrodloTestowe = sprawy::ZrodloWynikow();
}

// trzyma (pkt 1): „tylko raport” z pilnością A w wyniku (gdyby silnik reguł przepisał pilność docelową) nie daje pilności,
// tonu, pełnego ekranu, linii ani buzzera; karta `w raporcie, bez pilności`; blokuje uspokojenie.
static void tylkoRaportNigdyAlarmu() {
  reguly::Zestaw z = atrapa_b5::zestaw();
  Wynik w = atrapa_b5::wynik(z, "L-02", "a", true, true);
  w.pilnosc = Pilnosc::A;
  reguly::Pamiec pam;
  pam["L-02/a"].aktywnaOd = DZIS;
  std::vector<sprawy::SprawaL> s = sprawy::sprawyOtwarte(z, {w}, pam, atrapa_b5::funkcja(z));
  SPRAWDZ(s.size() == 1 && s[0].pilnosc == Pilnosc::BRAK && s[0].ton == sprawy::Ton::BRAK);
  SPRAWDZ(sprawy::sprawaEkranuDnia(s, DZIS) == nullptr);
  kolejka::WejscieKolejki wk;
  wk.dzis = NIEDZIELA;
  wk.sprawy = &s;
  SPRAWDZ(kolejka::kolejkaLinii(wk).empty());
  SPRAWDZ(!kolejka::buzzerPilnosci(s, {}, DZIS, 12 * 60).graj);
  SPRAWDZ(!kolejka::niewyciszonyA(s, DZIS));
  sprawy::WejscieSpraw ws;
  ws.dzis = DZIS;
  ws.sprawy = &s;
  std::vector<sprawy::KartaSprawy> k = sprawy::kartySpraw(ws);
  SPRAWDZ(k.size() == 1 && k[0].dalej == "w raporcie, bez pilności");
  kolejka::OcenaUspokojenia o = kolejka::ocenUspokojenie({w}, {});
  SPRAWDZ(!o.pkt1);
}

// trzyma (pkt 1): „nie oceniam” (także jedynej reguły, przy pustej liście punktów nieocenianych) nie daje uspokojenia.
static void nieOceniamToNieWNormie() {
  reguly::Zestaw z = atrapa_b5::zestaw();
  Stany st;
  st.nieoceniane.clear();
  st.ocenaNiepelna = false;
  st.uspokojenieOcenione = true;
  st.uspokojenie = true;
  kolejka::zastosujUspokojenie(st, kolejka::ocenUspokojenie({atrapa_b5::wynik(z, "L-03", "10", false, false)}, {}));
  SPRAWDZ(!st.uspokojenie);
}

// trzyma (pkt 1, 3): „wiem” na A nie chowa A: linia grupy 0 codziennie (60 px), bez buzzera ponownie.
static void wiemNaANieChowa() {
  sprawy::SprawaL a = sprawa("L-06/b", Pilnosc::A, dodajDni(DZIS, -3));
  a.wiem = dodajDni(DZIS, -1);
  std::vector<sprawy::SprawaL> s = {a};
  SPRAWDZ(sprawy::sprawaEkranuDnia(s, DZIS) == nullptr);
  kolejka::WejscieKolejki wk;
  wk.dzis = DZIS;
  wk.sprawy = &s;
  std::vector<kolejka::LiniaUwagi> k = kolejka::kolejkaLinii(wk);
  SPRAWDZ(!k.empty() && k[0].wzorzecA && k[0].zdrowie());
}

int main() {
  pilnoscAWidocznaGdyEkranemJestB();
  buzzerRazNaWejscieTakzeNastepnegoDnia();
  rytmEkranuBPoRestarcie();
  liniaANaS5();
  tylkoRaportNigdyAlarmu();
  nieOceniamToNieWNormie();
  wiemNaANieChowa();
  printf("testy_kolejka_linii_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
