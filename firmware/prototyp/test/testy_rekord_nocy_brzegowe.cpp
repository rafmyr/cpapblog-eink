// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych: „Rekord nocy”.
// Testy niezależne od testów podstawowych (testy_rekord_nocy.cpp). Sprawdzają zachowania, które mają trzymać, także po
// zmianach w kodzie.
//
// Pomocnicze funkcje i atrapy (PanelB1, wybudz(), plik(), noceV0(), nocTestowa(), raportB1() …) bierzemy z testów
// przez dołączenie pliku z podmienioną nazwą main() (wzór: testy_magazyn_brzegowe.cpp). Testy autora nie są tu
// uruchamiane. Dane wyłącznie syntetyczne.
#define main testy_rekord_nocy_main
#include "testy_rekord_nocy.cpp"
#undef main

static int r_ok = 0, r_zle = 0;
static void wynik(bool w, const char* co, int linia) {
  if (w) r_ok++;
  else { r_zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, linia, co); }
}
#define R(w) wynik((w), #w, __LINE__)
#define R_ROWNE(a, b)                                                                                     \
  do {                                                                                                    \
    std::string _a = (a), _b = (b);                                                                       \
    wynik(_a == _b, (std::string(#a) + " = \"" + _a + "\", oczekiwane \"" + _b + "\"").c_str(), __LINE__); \
  } while (0)

static std::string pola21(const std::string& w) { return pierwszePola(w, 21); }
static std::map<std::string, std::string> wierszeNocyWgDaty(const AtrapaPlikow& pl) {
  std::map<std::string, std::string> m;
  for (auto& kv : pl.pliki) {
    const std::string& s = kv.first;
    if (s.compare(0, 11, "/dane/noce/") != 0 || s.size() != 19 || s.compare(15, 4, ".csv") != 0) continue;
    std::vector<std::string> w = wiersze(kv.second);
    for (size_t i = 2; i < w.size(); i++)
      if (!w[i].empty() && w[i][0] != '#') m[w[i].substr(0, 10)] = w[i];
  }
  return m;
}
static bool wszystkieV2(const AtrapaPlikow& pl) {
  bool b = true;
  for (auto& kv : pl.pliki) {
    const std::string& s = kv.first;
    if (s.compare(0, 11, "/dane/noce/") != 0 || s.size() != 19 || s.compare(15, 4, ".csv") != 0) continue;
    // bieżąca wersja pliku nocy (od 11.11.2026 wersja 5: pola wersji 4 i `m14;m15`)
    b = b && kv.second.compare(0, 18, "#cpap-eink noce " + std::to_string(WERSJA_NOCY) + "\n") == 0;
  }
  return b;
}

// ---------- 1. migracja danych ----------

// v0 przez dyrygenta (prawdziwa ścieżka startu): płaski `/noce.csv` z układami 14 i 20 pól (wersje prototypowe),
// źródła K i A, puste M-03/M-09; 150 nocy w jednym roku i 412 w dwóch latach. Bez sieci (status nieosiągalny).
static void m01_v0PrzezDyrygenta(int ile) {
  PanelB1 s;
  std::vector<std::string> v0 = noceV0(ile, "2026-11-03");
  // co siódma noc w starszym układzie 14 pól (bez czesciowa;epap_str;oa;ca;h;csr), jak wiersze
  for (size_t i = 0; i < v0.size(); i += 7) v0[i] = pierwszePola(v0[i], 14);
  std::map<std::string, std::string> przed;
  for (auto& l : v0) {
    Noc n;
    if (parsujNoc(l, 'A', n)) przed[n.data] = zapiszNoc(n);
  }
  s.p.pliki.pliki["/noce.csv"] = plikV0(v0);
  s.p.siec.odpowiedzi.clear();
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  wybudz(s, ep(2026, 11, 4, 7, 30), PowodWybudzenia::ZEGAR);  // drugie wybudzenie: nic nie znika
  std::map<std::string, std::string> po = wierszeNocyWgDaty(s.p.pliki);
  printf("  m01: v0 %d nocy → w /dane/noce/ %d (wersja 2: %s)\n", ile, (int)po.size(), wszystkieV2(s.p.pliki) ? "tak" : "nie");
  R((int)po.size() == ile);
  R(wszystkieV2(s.p.pliki));
  int zgodne = 0;
  for (auto& kv : przed) {
    auto i = po.find(kv.first);
    Noc n;
    if (i != po.end() && parsujNocV2(i->second, n) && zapiszNoc(n) == kv.second && dziel(i->second, ';').size() == POLA_NOCY_V5)  /* 41 pól, 42 (M-18), 43–44 (M-14, M-15) */ zgodne++;
  }
  R(zgodne == ile);
}

// v1 → v2 przez dyrygenta, dwa pliki roczne (2025 poza oknem wczytywania 120 dni, 2026 w oknie).
static void m02_v1DwaLataPrzezDyrygenta() {
  PanelB1 s;
  std::vector<std::string> v0 = noceV0(160, "2026-11-03");
  std::vector<std::string> a, b;
  for (auto& l : v0) (l.compare(0, 4, "2025") == 0 ? a : b).push_back(l + ";1762149600");
  s.p.pliki.pliki["/dane/noce/2025.csv"] = plik("noce", 1, POLA_V1, a);
  s.p.pliki.pliki["/dane/noce/2026.csv"] = plik("noce", 1, POLA_V1, b);
  s.p.siec.odpowiedzi.clear();
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  std::map<std::string, std::string> po = wierszeNocyWgDaty(s.p.pliki);
  R(po.size() == 160);
  R(wszystkieV2(s.p.pliki));
  R(s.p.pliki.istnieje("/dane/noce/2025.csv.v1") && s.p.pliki.istnieje("/dane/noce/2026.csv.v1"));
  int zgodne = 0;
  for (auto& l : a) zgodne += po.count(l.substr(0, 10)) && pola21(po[l.substr(0, 10)]) == l ? 1 : 0;
  for (auto& l : b) zgodne += po.count(l.substr(0, 10)) && pola21(po[l.substr(0, 10)]) == l ? 1 : 0;
  R(zgodne == 160);  // `import` z wersji 1 zostaje
}

// Zanik zasilania w każdym miejscu migracji 1 → 2 (kopia `.v1`, `.tmp`, zamiana; dwa pliki roczne i `/noce.csv`
// ze znacznikiem `ok`): po restarcie (nowy Magazyn) migracja kończy się poprawnie, rekordów tyle samo, treść bez zmian.
static void m03_zanikWKazdymKroku() {
  std::vector<std::string> v0 = noceV0(150, "2026-11-03");
  std::vector<std::string> a, b;
  for (auto& l : v0) (l.compare(0, 4, "2025") == 0 ? a : b).push_back(l + ";1762149600");
  std::string ta = plik("noce", 1, POLA_V1, a), tb = plik("noce", 1, POLA_V1, b);
  long razem = (long)(2 * (ta.size() + tb.size()) + 3 * POLA_V2.size()) + 2000;
  int prob = 0, dobrych = 0, zgodnych = 0, zostalV1 = 0;
  for (long k = 0; k < razem; k += 97) {
    Dziennik d;
    AtrapaPlikow pl(d);
    montowana(pl);
    pl.pliki["/dane/noce/2025.csv"] = ta;
    pl.pliki["/dane/noce/2026.csv"] = tb;
    pl.zanikPoBajtach = k;
    {
      Log lg;
      Magazyn m(pl, lg.f());
      m.migrujWMiejscu();
    }
    pl.zanik = false;
    pl.zanikPoBajtach = -1;
    Log lg;
    Magazyn m2(pl, lg.f());
    bool ok2 = m2.migrujWMiejscu();
    prob++;
    std::map<std::string, std::string> po = wierszeNocyWgDaty(pl);
    bool v2 = wszystkieV2(pl);
    dobrych += ok2 && v2 && po.size() == 150 ? 1 : 0;
    int z = 0;
    for (auto& l : a) z += po.count(l.substr(0, 10)) && pola21(po[l.substr(0, 10)]) == l ? 1 : 0;
    for (auto& l : b) z += po.count(l.substr(0, 10)) && pola21(po[l.substr(0, 10)]) == l ? 1 : 0;
    zgodnych += z == 150 ? 1 : 0;
    zostalV1 += pl.istnieje("/dane/noce/2026.csv.v1") ? 1 : 0;
  }
  printf("  m03: zaników %d, po restarcie dobrych %d, treść zgodna %d, z kopią .v1 %d\n", prob, dobrych, zgodnych, zostalV1);
  R(dobrych == prob);
  R(zgodnych == prob);
}

// Plik wersji 2 z nieczytelnym wierszem w środku (liczba wierszy zgodna z `#koniec`, więc plik DOBRY), potem zwykłe
// pobranie nowej nocy: plik roczny przepisany z pamięci bez tego wiersza. Było: (stan sprzed tak samo
// parsujNocV1(); format wersji 2 go nie naprawia, a migracja 1 → 2 przenosi taki wiersz, który ginie przy pierwszym zapisie).
static void m04_uszkodzonyWierszWSrodku() {
  PanelB1 s;
  std::vector<std::string> v;
  for (auto& l : noceV0(30, "2026-11-03")) v.push_back(l + ";" + PUSTE_B1);
  std::string zly = "2026-10-20;1;4x0;" + std::string(31, ';');  // data czytelna, minuty nieczytelne
  v.insert(v.begin() + 15, zly);
  s.p.pliki.pliki["/dane/noce/2026.csv"] = plik("noce", 2, POLA_V2, v);
  s.p.siec.odpowiedzi.clear();
  s.p.siec.dodaj("/api/status", 200, STATUS_B1);
  s.p.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
  s.p.siec.dodaj(sciezkaNocy(DOBA_A), 200, raportB1(DOBA_A, "2", "16.2"));
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  std::string t = s.p.pliki.tresc("/dane/noce/2026.csv");
  bool jestNowa = !wierszZData(t, "2026-11-04").empty();
  bool zostal = zawiera(wiersze(t), zly) || s.p.pliki.istnieje("/dane/noce/2026.csv.uszkodzony");
  printf("  m04: nowa noc zapisana: %s, nieczytelny wiersz zachowany (w pliku albo kopii): %s\n", jestNowa ? "tak" : "nie",
         zostal ? "tak" : "nie");
  R(jestNowa);
  R(zostal);  // naprawione (wiersz zachowany dosłownie, dyrygent zapiszHistorie())
}

// Plik z nowszej wersji: tylko do odczytu, pobranie nocy nie zmienia pliku, pola wersji 2 z niego czytelne.
static void m05_plikWersji3() {
  PanelB1 s;
  std::vector<std::string> v;
  // wersja nowsza niż znana (WERSJA_NOCY + 1): pola do 41, m18, m14, m15, pole dopisane w tej nowszej wersji
  for (auto& l : noceV0(30, "2026-11-03")) v.push_back(l + ";;16.00;1;P;S" + std::string(POLA_NOCY_V2 - 25, ';') + ";;;;X");
  std::string t3 = plik("noce", WERSJA_NOCY + 1, POLA_V2 + ";m18;m14;m15;test_nowsza", v);
  s.p.pliki.pliki["/dane/noce/2026.csv"] = t3;
  s.p.siec.odpowiedzi.clear();
  s.p.siec.dodaj("/api/status", 200, STATUS_B1);
  s.p.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
  s.p.siec.dodaj(sciezkaNocy(DOBA_A), 200, raportB1(DOBA_A, "2", "16.2"));
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  R(s.p.pliki.tresc("/dane/noce/2026.csv") == t3);
  R(!s.p.pliki.istnieje("/dane/noce/2026.csv.v5") && !s.p.pliki.istnieje("/dane/noce/2026.csv.v4") &&
    !s.p.pliki.istnieje("/dane/noce/2026.csv.v3") &&
    !s.p.pliki.istnieje("/dane/noce/2026.csv.v2"));
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  pl.pliki["/dane/noce/2026.csv"] = t3;
  Log lg;
  Magazyn m(pl, lg.f());
  OdczytPliku o = m.czytaj("noce", "/dane/noce/2026.csv");
  Noc n;
  R(o.stan == StanPliku::NOWSZY && !o.wiersze.empty() && parsujNocV2(o.wiersze.back(), n));
  R(std::fabs(n.m13 - 16.0f) < 1e-3 && n.m17 == 1 && n.pna == "P" && n.srn == "S");
}

// Przywracanie `W` po migracji: noc częściowa z AirBridge we flash (z polami model, numer, M-13) i ta sama noc
// pełna w zrzucie wersji 0. Wygrywa noc zrzutu (U2 wariant A), ale pola wersji 2, których zrzut nie ma, giną.
// Było: (scalWiersze() pisało wierszNocyV1(nz) bez uzupełnienia z flash).
static void m06_przywracaniePolaB1Czesciowej() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  Noc a = nocTestowa("2026-11-03", 7);
  a.zrodlo = 'A';
  a.czesciowa = true;
  a.m13 = 16.2f;
  a.pna = "AirCurve_10_CS_PaceWave";
  a.srn = "SYN00000001";
  a.m08 = 3.0f;
  pl.pliki["/dane/noce/2026.csv"] = plik("noce", 2, POLA_V2, {wierszNocyV2(a)});
  pl.pliki["/przywracanie/noce.csv"] = plikV0(noceV0(5, "2026-11-03"));
  Log lg;
  Magazyn m(pl, lg.f());
  Magazyn::WynikPrzywracania w = m.przywrocZKatalogu("/przywracanie");
  R(w.ok);
  Noc n;
  R(parsujNocV2(wierszZData(pl.tresc("/dane/noce/2026.csv"), "2026-11-03"), n));
  R(!n.czesciowa);
  R(n.srn == "SYN00000001" && n.pna == "AirCurve_10_CS_PaceWave");  // naprawione (scalWiersze() z uzupelnijNoc())
}

// ---------- 2. wierność pól (Załącznik A) ----------

static std::string raport(const std::string& srodek, const std::string& kanaly = "", const std::string& ustaw = "") {
  return "{\"day\":20395,\"present\":true,\"partial\":false,\"used_seconds\":21600,\"ahi\":1.4,\"events_known\":true," +
         srodek + (srodek.empty() ? "" : ",") +
         "\"channels\":[{\"id\":\"Press\",\"unit\":\"cmH2O\",\"median\":9.8,\"p95\":13.6,\"source\":\"edf\"}" + kanaly +
         "],\"settings\":{\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"value\":\"ASVAuto\",\"option\":8}" + ustaw + "]}}";
}
static Noc nocZ(const std::string& r) { return nocZRaportu(r, 20395, "", "", "").noc; }

static void poleM08() {
  BLISKO(nocZ(raport("\"leak_high_seconds\":90")).m08, 1.5);
  BLISKO(nocZ(raport("\"leak_high_seconds\":0")).m08, 0.0);
  for (const char* x : {"null", "\"120\"", "-1", "true", "[1]"})
    R(std::isnan(nocZ(raport(std::string("\"leak_high_seconds\":") + x)).m08));
  R(std::isnan(nocZ(raport("")).m08));
}
static void poleUA() {
  BLISKO(nocZ(raport("\"counts\":{\"oa\":1,\"ca\":6,\"ua\":3,\"h\":2}")).ua, 0.5);  // 3 × 3600 / 21600 (czas z maską)
  R(std::isnan(nocZ(raport("\"counts\":{\"oa\":1,\"ca\":6,\"ua\":null,\"h\":2}")).ua));
  R(std::isnan(nocZ(raport("\"counts\":{\"oa\":1,\"ca\":6,\"h\":2}")).ua));
  R(std::isnan(nocZ(raport("\"counts\":{\"ua\":-2}")).ua));
  BLISKO(nocZ(raport("\"counts\":{\"oa\":1,\"ca\":6,\"ua\":3,\"h\":2}")).ca, 1.0);  // R-DANE-14 przykład
}
static void poleM17() {
  R(nocZ(raport("\"session_count\":0")).m17 == 0);
  R(nocZ(raport("\"session_count\":1")).m17 == 0);
  R(nocZ(raport("\"session_count\":4")).m17 == 3);
  for (const char* x : {"null", "\"2\"", "-1", "true"}) R(nocZ(raport(std::string("\"session_count\":") + x)).m17 == -1);
  R(nocZ(raport("")).m17 == -1);
  int m = nocZ(raport("\"session_count\":2.5")).m17;
  printf("  pole M-17: session_count 2.5 → M-17 = %d (liczba niecałkowita przyjęta jako %d)\n", m, m);
}
static void poleRespRateFlowLim() {
  BLISKO(nocZ(raport("", ",{\"id\":\"RespRate\",\"median\":15.24,\"p95\":19,\"source\":\"edf\"}")).m13, 15.24);
  R(std::isnan(nocZ(raport("", ",{\"id\":\"RespRate\",\"median\":null,\"p95\":19,\"source\":\"edf\"}")).m13));
  R(std::isnan(nocZ(raport("", ",{\"id\":\"RespRate\",\"median\":\"15\",\"source\":\"edf\"}")).m13));
  // wpis spoza EDF, potem z EDF: EDF wygrywa (R-NOR-08 w duchu, jak Press)
  BLISKO(nocZ(raport("", ",{\"id\":\"RespRate\",\"median\":12,\"source\":\"str\"},{\"id\":\"RespRate\",\"median\":16,\"source\":\"edf\"}")).m13, 16.0);
  Noc f = nocZ(raport("", ",{\"id\":\"FlowLim\",\"median\":0.05,\"p95\":0.31,\"source\":\"edf\"}"));
  BLISKO(f.m10, 0.05);
  BLISKO(f.m10p, 0.31);
  Noc g = nocZ(raport("", ",{\"id\":\"FlowLim\",\"median\":null,\"p95\":0.2,\"source\":\"edf\"}"));
  R(std::isnan(g.m10));
  BLISKO(g.m10p, 0.2);
  R(std::isnan(nocZ(raport("")).m10) && std::isnan(nocZ(raport("")).m10p));
  // zapis i odczyt: dwa miejsca po przecinku (skala 0–1, próg R-KON-30 +0,1)
  Noc z;
  R(parsujNocV2(wierszNocyV2(f), z));
  BLISKO(z.m10p, 0.31);
}
static void poleUstawienia() {
  Noc a = nocZ(raport("", "",
                      ",{\"tag\":\"MPA\",\"value\":\"15.0 cmH2O\"},{\"tag\":\"MPI\",\"value\":7},{\"tag\":\"EEP\",\"value\":\"8\"},"
                      "{\"tag\":\"MNS\",\"value\":\"3\"},{\"tag\":\"MXS\",\"value\":\"abc\"}"));
  BLISKO(a.mpa, 15.0);
  BLISKO(a.mpi, 7.0);
  BLISKO(a.eep, 8.0);
  BLISKO(a.mns, 3.0);
  R(std::isnan(a.mxs) && std::isnan(a.ipc));
  std::string r = raport("", "", ",{\"tag\":\"MPA\",\"value\":15}");
  r.replace(r.find("\"available\":true"), 16, "\"available\":false");
  R(std::isnan(nocZ(r).mpa));
  // tryb ASVAuto (jedyny tryb w wersji beta): ustawienia mają tagi EAI, EAX, ANS, AXS (spec 4.11),
  // a rekord nie ma na nie pól. Bez asercji: luka kontraktu, nie błąd implementacji.
  Noc b = nocZ(raport("", "", ",{\"tag\":\"EAI\",\"value\":\"5.0\"},{\"tag\":\"EAX\",\"value\":\"10.0\"},"
                              "{\"tag\":\"ANS\",\"value\":\"3.0\"},{\"tag\":\"AXS\",\"value\":\"15.0\"}"));
  int puste = 0;
  for (float v : {b.mpa, b.mpi, b.ipc, b.eep, b.mns, b.mxs}) puste += std::isnan(v) ? 1 : 0;
  printf("  pole ustawień: raport ASVAuto (EAI/EAX/ANS/AXS): pustych pól ustawień %d z 6\n", puste);
}

// ---------- 3. pełniejszy rekord wygrywa, R-DANE-24 ----------

// 11:00 (R-DANE-24): ten sam raport, ale `/api/status` tego wybudzenia bez `srn`. Spec 4.6 (R-NOR-25): „brak odczytu
// nie jest zmianą”; R-DANE-24: „raport bez zmian → rekord bez zmian, bez odświeżenia”. Było: (naprawione).
static void ponownePobranieBezSrn() {
  std::vector<Noc> hist;
  NocAB pierwsza = nocZRaportu(raportB1(DOBA_A, "1", "16.2"), DOBA_A, "", "AirCurve_10_CS_PaceWave", "SYN00000001");
  R(wpiszNocAB(hist, pierwsza.noc, false, pierwsza.pelna));
  NocAB druga = nocZRaportu(raportB1(DOBA_A, "1", "16.2"), DOBA_A, "", "AirCurve_10_CS_PaceWave", "");
  bool zmiana = wpiszNocAB(hist, druga.noc, true, druga.pelna);
  printf("  r01: ponowne pobranie bez `srn`: rekord zmieniony: %s, srn po: \"%s\"\n", zmiana ? "tak" : "nie",
         hist.empty() ? "" : hist[0].srn.c_str());
  R(!zmiana);                                        // naprawione
  R(!hist.empty() && hist[0].srn == "SYN00000001");  // naprawione
  // ten sam raport z tym samym statusem: bez zmian (R-DANE-24)
  std::vector<Noc> h2;
  wpiszNocAB(h2, pierwsza.noc, false, pierwsza.pelna);
  R(!wpiszNocAB(h2, pierwsza.noc, true, pierwsza.pelna));
  // druga sesja: session_count 1 → 2 przy tym samym used_seconds: rekord nadpisany (M-17), R-DANE-24
  NocAB trzecia = nocZRaportu(raportB1(DOBA_A, "2", "16.2"), DOBA_A, "", "AirCurve_10_CS_PaceWave", "SYN00000001");
  R(wpiszNocAB(h2, trzecia.noc, true, trzecia.pelna));
  R(h2[0].m17 == 1);
}
// Noc z karty po pełnej nocy z AirBridge: pola wersji 2 zostają, pola karty tylko uzupełniają.
static void kartaPoAirBridge() {
  std::vector<Noc> hist;
  NocAB a = nocZRaportu(raportB1(DOBA_A, "3", "16.2"), DOBA_A, "", "P", "S");
  wpiszNocAB(hist, a.noc, false, a.pelna);
  Noc k = nocTestowa(a.noc.data, 4);
  k.zrodlo = 'K';
  wpiszImport(hist, k);
  R(hist.size() == 1 && hist[0].zrodlo == 'A' && hist[0].m17 == 2 && hist[0].srn == "S" && !std::isnan(hist[0].m13) &&
    !std::isnan(hist[0].m08));
}

// ---------- 4. stany odchylenia M-03ca i M-17 ----------

static Noc nS(const std::string& d, float ca, int m17, const std::string& ust = "U1") {
  Noc n = nocStanu(d, ca, m17);
  n.ustawienia = ust;
  return n;
}
static std::vector<Noc> norma30(const std::string& od, const std::string& ust = "U1") {
  std::vector<Noc> h;
  for (int i = 0; i < 30; i++) h.push_back(nS(dodajDni(od, i), 1.0f + 0.1f * (float)(i % 3), i % 2, ust));
  return h;
}
static void s01_wejscie3z5NieKolejne() {
  std::vector<Noc> h = norma30("2026-09-01");
  std::string d = "2026-10-01";
  float ca[] = {3.0f, 1.1f, 3.0f, 1.0f, 3.0f};
  for (int i = 0; i < 5; i++) h.push_back(nS(dodajDni(d, i), ca[i], 0));
  R(!liczStany(h, dodajDni(d, 3), {}).ca.aktywny);
  R(liczStany(h, dodajDni(d, 4), {}).ca.aktywny);
}
// Noce niepewne (M-08) z wysokim CA/h: nie liczą się do 3 z 5 i nie wchodzą do normy CA (R-NOR-05, R-NOR-19).
static void s02_niepewneCa() {
  std::vector<Noc> h = norma30("2026-09-01");
  std::string d = "2026-10-01";
  h.push_back(nS(dodajDni(d, 0), 3.0f, 0));
  h.push_back(nS(dodajDni(d, 1), 3.0f, 0));
  Noc n = nS(dodajDni(d, 2), 3.0f, 0);
  n.m08 = 45.0f;
  h.push_back(n);
  R(!liczStany(h, dodajDni(d, 2), {}).ca.aktywny);
  std::vector<Noc> g = norma30("2026-09-01");
  for (int i = 0; i < 12; i++) {
    Noc x = nS(dodajDni("2026-10-01", i), 6.0f, 0);
    x.m08 = 60.0f;
    g.push_back(x);
  }
  Stany s = liczStany(g, "2026-10-13", {});
  R(s.norma.ca.jest() && std::fabs(s.norma.ca.med - 1.1) < 1e-3 && s.norma.ca.p90 < 1.3);
  R(!s.ca.aktywny);
}
// CA/h i M-17 bez zerowania przy zmianie ustawień (tabela 4.4: „nie”).
static void s03_bezFiltraUstawien() {
  std::vector<Noc> h = norma30("2026-09-01", "U1");
  h.push_back(nS("2026-10-01", 1.0f, 0, "U2"));
  Stany s = liczStany(h, "2026-10-01", {});
  R(s.norma.ca.jest() && s.norma.m17.jest());
}
// R-NOR-22: CA/h (zdrowotna) w stanie trzyma normę sprzed stanu, także gdy po 90 dniach nie ma nocy spoza stanu.
static void s04_zdrowotnaTrzymaNorme() {
  std::vector<Noc> h = norma30("2026-06-01");
  std::string d = "2026-07-01";
  for (int i = 0; i < 100; i++) h.push_back(nS(dodajDni(d, i), 3.0f, 0));
  Stany s = liczStany(h, dodajDni(d, 99), {});
  R(s.ca.aktywny && s.ca.nocy >= 95);
  // M-17 (techniczna): po 21 nocach nowy poziom, potem w normie
  std::vector<Noc> t = norma30("2026-06-01");
  for (int i = 0; i < 30; i++) t.push_back(nS(dodajDni(d, i), 1.1f, 4));
  Stany st = liczStany(t, dodajDni(d, 29), {});
  R(!st.m17.aktywny);
  R(st.norma.m17.jest() && std::fabs(st.norma.m17.med - 4.0) < 1e-3);
}
// Noc nieważna (< 4 h) nie liczy się do stanu ani normy CA/M-17.
static void s05_niewazne() {
  std::vector<Noc> h = norma30("2026-09-01");
  for (int i = 0; i < 5; i++) {
    Noc n = nS(dodajDni("2026-10-01", i), 5.0f, 6);
    n.wazna = false;
    n.m01 = 120;
    h.push_back(n);
  }
  Stany s = liczStany(h, "2026-10-05", {});
  R(!s.ca.aktywny && !s.m17.aktywny);
}

// ---------- 5. noc niepewna 4.9 ----------
static void n01_granice() {
  Noc n = nocStanu("2026-10-01", 1.0f, 0);  // m01 420, wmed 3
  n.m08 = 29.99f;
  R(!nocNiepewna(n));
  n.m08 = 30.0f;
  R(nocNiepewna(n));
  n.m01 = 200;
  n.m08 = 20.0f;
  R(nocNiepewna(n));  // = 10%
  n.m08 = 19.98f;
  R(!nocNiepewna(n));
  n.m01 = -1;
  n.m08 = 25.0f;
  R(!nocNiepewna(n));  // bez M-01 tylko warunek 30 min
  n.m01 = 0;
  R(!nocNiepewna(n));
  n.m08 = NAN;
  n.wmed = 30.0f;
  R(!nocNiepewna(n));
  n.wmed = 30.01f;
  R(nocNiepewna(n));
  // z raportu: 1800 s dużego wycieku przy 7 h = 30 min → niepewna (M-08 po zapisie i odczycie też)
  Noc r = nocZ(raport("\"leak_high_seconds\":1800"));
  R(nocNiepewna(r));
  Noc z;
  R(parsujNocV2(wierszNocyV2(r), z) && nocNiepewna(z));
  Noc q = nocZ(raport("\"leak_high_seconds\":1799"));  // 29,98 min, 21 600 s → 10% = 36 min
  R(!nocNiepewna(q));
}
// Interakcja z normą AHI: noc niepewna z M-08 poza normą AHI, w normie M-01 (R-NOR-19).
static void n02_normaAhi() {
  std::vector<Noc> h;
  for (int i = 0; i < 20; i++) h.push_back(nS(dodajDni("2026-09-01", i), 1.0f, 0));
  for (int i = 0; i < 20; i++) {
    Noc n = nS(dodajDni("2026-09-21", i), 1.0f, 0);
    n.ahi = 20.0f;
    n.m08 = 40.0f;
    n.m01 = 500;
    h.push_back(n);
  }
  Stany s = liczStany(h, "2026-10-11", {});
  R(s.norma.ahi.jest() && std::fabs(s.norma.ahi.med - 1.0) < 1e-3);
  R(s.norma.m01.n == 30);
  R(!s.ahi.aktywny);
}

// ---------- 7. zgodność wstecz ----------
static void z01_parsujV1NaV2() {
  Noc a = nocTestowa("2026-11-03", 8);
  a.import = 1762149600;
  a.m13 = 15.0f;
  a.pna = "P";
  Noc x, y;
  R(parsujNocV1(wierszNocyV2(a), x) && parsujNocV1(wierszNocyV1(a), y));
  R(zapiszNoc(x) == zapiszNoc(y) && x.import == y.import && std::isnan(x.m13) && x.pna.empty());
  R(POLA_NOCY_V5 == formatPliku(rejestrFormatow(), "noce")->pola.back().size());  // wersja 5
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  m01_v0PrzezDyrygenta(150);
  m01_v0PrzezDyrygenta(412);
  m02_v1DwaLataPrzezDyrygenta();
  m03_zanikWKazdymKroku();
  m04_uszkodzonyWierszWSrodku();
  m05_plikWersji3();
  m06_przywracaniePolaB1Czesciowej();
  poleM08();
  poleUA();
  poleM17();
  poleRespRateFlowLim();
  poleUstawienia();
  ponownePobranieBezSrn();
  kartaPoAirBridge();
  s01_wejscie3z5NieKolejne();
  s02_niepewneCa();
  s03_bezFiltraUstawien();
  s04_zdrowotnaTrzymaNorme();
  s05_niewazne();
  n01_granice();
  n02_normaAhi();
  z01_parsujV1NaV2();
  r_ok += ok;  // BLISKO() z testów autora liczy do ich liczników
  r_zle += zle;
  printf("testy_rekord_nocy_brzegowe: %d dobrze, %d źle\n", r_ok, r_zle);
  return r_zle ? 1 : 0;
}
