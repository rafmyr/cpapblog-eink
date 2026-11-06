// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych magazynu danych i przywracania.
// Każdy test rNN_… ma przejść bez zmiany oczekiwań.
//
// Pomocnicze funkcje i atrapy (PanelP03, wybudz(), zrzut(), komendaW(), plik(), noceV0() …) bierzemy z testów autora
// przez dołączenie pliku z podmienioną nazwą main: bez kopiowania 2000 wierszy, te same dane syntetyczne
// (żadnych prawdziwych danych zdrowotnych). Testy autora nie są tu uruchamiane.
#define main testy_magazyn_danych_main
#include "testy_magazyn_danych.cpp"
#undef main

static const uint32_t LIMIT_W_TESTU_MS = 10000;  // tolerancja r09: 5 s limitu bajtu z zapasem

// Host, z którym połączy się HTTPClient rdzenia Arduino 3.3.12 dla `url` (libraries/HTTPClient/src/HTTPClient.cpp
// HTTPClient::beginInternal(): po `://` host do pierwszego `/`, potem wszystko do `@` to dane logowania, potem port po
// `:`). Odtworzone wiernie z kodu rdzenia, bez IPv6.
static std::string hostHttpClient(const std::string& url) {
  size_t i = url.find(':');
  if (i == std::string::npos) return "";
  std::string u = url.substr(i + 3);
  size_t s = u.find('/');
  std::string host = s == std::string::npos ? u : u.substr(0, s);
  size_t a = host.find('@');
  if (a != std::string::npos) host = host.substr(a + 1);
  size_t p = host.find(':');
  if (p != std::string::npos) host = host.substr(0, p);
  return host;
}

// r01, R-NF-27 (T-TEST-204): adres `http://192.168.1.60?@203.0.113.5/otoczenie` przechodzi adresWSieciLokalnej(), bo
// funkcja kończy host na `?` albo `#`, a HTTPClient kończy go dopiero na `/` i bierze host po `@`: żądanie z tokenem
// idzie do 203.0.113.5 (internet). To samo z `#`.
static void RNF27_hostZaZnakiemZapytania() {
  for (const char* u : {"http://192.168.1.60?@203.0.113.5/otoczenie", "http://192.168.1.60#@203.0.113.5/otoczenie",
                        "http://dom.local?x=@8.8.8.8/otoczenie"}) {
    SPRAWDZ(!adresWSieciLokalnej(u));  // pada: przyjęty
    ROWNE(std::string(adresWSieciLokalnej(u) ? hostHttpClient(u) : "odrzucony"), "odrzucony");
  }
  // dyrygent: żądanie do źródła otoczenia z tym adresem → HTTPClient łączy się z hostem w internecie, z tokenem
  PanelP03 s;
  zNocami(s, 30);
  s.konf.otoUrl = "http://192.168.1.60?@203.0.113.5/otoczenie";
  s.konf.otoToken = "TAJNY-TOKEN-R01";
  s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
  wybudz(s, ep(2026, 11, 4, 12, 0), PowodWybudzenia::START);
  int doInternetu = 0, zTokenem = 0;
  for (size_t i = 0; i < s.siec.zadania.size(); i++) {
    if (hostHttpClient(s.siec.zadania[i]) != "203.0.113.5") continue;
    doInternetu++;
    for (auto& n : s.siec.naglowki[i]) zTokenem += n.second.find("TAJNY-TOKEN-R01") != std::string::npos ? 1 : 0;
  }
  printf("  r01: żądań do 203.0.113.5: %d, z tokenem: %d\n", doInternetu, zTokenem);
  ROWNE_L(doInternetu, 0);  // pada
  ROWNE_L(zTokenem, 0);     // pada
}

// r02, T-WYM-87 (weryfikacja: „przywrócenie z `niezgodna liczba` przy brakującym pliku nocy”) i 8.9: plik nocy
// odrzucony w oknie (zły skrót po zakłóceniu na kablu), pozostałe przyjęte, `K` → brak wpisu `niezgodna liczba`
// (przywrocZKatalogu() bez pliku nocy kończy się bez wpisu), potem zwykła praca pobiera noc z AirBridge i od
// następnego startu okno przywracania już się nie otwiera: poprawionego pliku nocy nie da się wgrać (`poza oknem`),
// choć 8.9 (ostatni akapit) i komunikat przejscie_32mb.py każą powtórzyć krok 4.
static void T_WYM_87_nocOdrzuconaPotemBezOkna() {
  PanelP03 s;
  partycjaSkasowana(s);
  std::map<std::string, std::string> z = zrzut(412);
  std::string zla = komendaW("noce.csv", z["noce.csv"]);
  zla[zla.size() - 7] = zla[zla.size() - 7] == '1' ? '2' : '1';  // jeden bajt zmieniony w drodze
  std::string b = zla;
  for (auto& kv : z)
    if (kv.first != "noce.csv") b += komendaW(kv.first, kv.second);
  doPortu(s, b + "K");
  odpowiedziDoby(s, ep(2026, 11, 4, 7, 0));
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "BLAD noce.csv skrot"));
  SPRAWDZ(logMa(s, "niezgodna liczba"));  // pada: T-WYM-87 weryfikacja
  int nocyPoPierwszym = rekordyNocy(s.p.pliki);
  printf("  r02: po pierwszym starcie nocy w plikach %d (pobrane z AirBridge)\n", nocyPoPierwszym);
  // drugi start (8.9: „przywrócić ponownie kablem, okno otwiera się przy partycji bez plików nocy”)
  doPortu(s, komendaW("noce.csv", z["noce.csv"]) + "K");
  blokada(s, 0);
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "OK noce.csv"));                          // pada: `BLAD noce.csv poza oknem`
  SPRAWDZ(zawiera(t.diagnostykaTestu, "historia 413 nocy"));  // pada: historia ze zrzutu nie wróci (integracja: log U-05)
  SPRAWDZ(!logMa(s, "BLAD noce.csv poza oknem"));            // pada
}

// r03, 8.9 krok 4 i 5: okno przywracania minęło bez plików (np. skrypt przejścia nie zobaczył wiersza okna, zob. pytest
// r10), panel pobrał noce z AirBridge → przy każdym następnym starcie komenda `W` jest `poza oknem`; zrzutu 412 nocy
// nie da się już wgrać (bez karty).
static void T_WYM_87_oknoPrzegapionePotemNigdy() {
  PanelP03 s;
  partycjaSkasowana(s);
  odpowiedziDoby(s, ep(2026, 11, 4, 7, 0));
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);  // 10 min okna bez komendy, potem zwykła praca
  SPRAWDZ(rekordyNocy(s.p.pliki) > 0);
  std::map<std::string, std::string> z = zrzut(412);
  std::string b;
  for (auto& kv : z) b += komendaW(kv.first, kv.second);
  // okno
  // zakończone bez żadnej komendy `W` stawia znacznik „bez przywracania” (nowy egzemplarz testowy: okno raz), więc przy
  // drugim starcie okna nie ma; komenda `W` poza oknem zdejmuje znacznik (wgraj_pliki.py robi to sam po odpowiedzi
  // `poza oknem, bez przywracania` i restartuje panel), a przy trzecim starcie okno wraca i zrzut wchodzi.
  // Wcześniej: `OK noce.csv` i `przywrócono 412` już przy drugim starcie.
  doPortu(s, komendaW("noce.csv", ""));
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 8, 0), PowodWybudzenia::START);  // wgraj_pliki.py --reset: `?` → poza oknem → `W` pusty
  SPRAWDZ(logMa(s, "BLAD noce.csv poza oknem"));
  SPRAWDZ(logMa(s, "znacznik „bez przywracania” usunięty"));
  doPortu(s, b + "K");
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 8, 10), PowodWybudzenia::START);  // wgraj_pliki.py: restart przez DTR/RTS
  SPRAWDZ(logMa(s, "OK noce.csv"));
  SPRAWDZ(logMa(s, "przywrócono 412"));
}

// r04, 8.9 krok 4 i komunikat przejscie_32mb.py („nie wszystkie pliki przyjęte; … powtórz krok 4”): jeden plik inny
// niż noce odrzucony (zły skrót), `K` → przywrócenie nocy udane → noce na partycji → poprawionego pliku czynników
// (odpowiedzi „Co było inaczej?”, dane nieodtwarzalne) nie da się już wgrać.
static void T_WYM_87_powtorzenieKroku4Niemozliwe() {
  PanelP03 s;
  partycjaSkasowana(s);
  std::map<std::string, std::string> z = zrzut(412);
  std::string b;
  for (auto& kv : z) {
    std::string k = komendaW(kv.first, kv.second);
    if (kv.first == "czynniki.csv") k[k.size() - 5] = k[k.size() - 5] == '1' ? '2' : '1';
    b += k;
  }
  doPortu(s, b + "K");
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "BLAD czynniki.csv skrot"));
  SPRAWDZ(logMa(s, "przywrócono 412 z 412"));
  doPortu(s, komendaW("czynniki.csv", z["czynniki.csv"]) + "K");
  wybudz(s, ep(2026, 11, 4, 7, 30), PowodWybudzenia::START);  // wgraj_pliki.py --reset, jak radzi skrypt
  SPRAWDZ(logMa(s, "OK czynniki.csv"));  // pada: `BLAD czynniki.csv poza oknem`
  SPRAWDZ(!wiersze(s.p.pliki.tresc("/dane/czynniki.csv")).empty() &&
          s.p.pliki.tresc("/dane/czynniki.csv").find(dodajDni("2026-10-15", 0)) != std::string::npos);  // pada
}

// r05, T-WYM-43, T-WYM-56, R-NF-12 (D1): zanik zasilania w trakcie migracji `/noce.csv` (2025 i 2026) po zapisie
// `/dane/noce/2025.csv`, a przed `2026.csv`. Po restarcie migrujNaNoweSciezki() widzi plik w `/dane/noce/` i uznaje
// migrację za zrobioną: noce 2026 nie trafiają do nowych plików (dyrygent czyta tylko `/dane/noce/`).
static void T_WYM_43_zanikWTrakcieMigracjiRocznej() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  std::vector<std::string> noce = noceV0(400, "2026-11-03");  // 2025-09-30 … 2026-11-03
  pl.pliki["/noce.csv"] = plikV0(noce);
  int w2025 = 0;
  for (auto& w : noce) w2025 += w.compare(0, 4, "2025") == 0 ? 1 : 0;
  // bajty pliku 2025 w wersji 1 (nagłówek, pola, wiersze z pustym `import`, `#koniec`) + część pliku 2026
  std::vector<std::string> v2025;
  // pliki nocy w znanej wersji formatu (2: pola wersji 2 puste na końcu wiersza)
  const FormatPliku& fn = *formatPliku(rejestrFormatow(), "noce");
  std::string polaZnane;
  for (size_t k = 0; k < fn.pola.back().size(); k++) polaZnane += (k ? ";" : "") + fn.pola.back()[k];
  for (int i = 0; i < w2025; i++) v2025.push_back(noce[(size_t)i] + ";" + std::string(fn.pola.back().size() - 21, ';'));
  pl.zanikPoBajtach = (long)plik("noce", fn.wersja(), polaZnane, v2025, true).size() + 500;
  {
    Log lg;
    Magazyn m(pl, lg.f());
    m.migrujNaNoweSciezki();
  }
  SPRAWDZ(pl.pliki.count("/dane/noce/2025.csv") == 1);  // zapisany przed zanikiem
  // restart
  pl.zanik = false;
  pl.zanikPoBajtach = -1;
  Log lg;
  Magazyn m(pl, lg.f());
  m.migrujNaNoweSciezki();
  int wPlikach = 0;
  for (auto& s : m.sciezki("noce")) wPlikach += (int)m.czytaj("noce", s).wiersze.size();
  printf("  r05: nocy w /noce.csv 400, w /dane/noce/ po restarcie %d\n", wPlikach);
  ROWNE_L(wPlikach, 400);  // pada: tylko noce 2025
  // T-WYM-55: następna udana migracja (1 → 2) usuwa /noce.csv; noce 2026 znikają na stałe
  std::vector<FormatPliku> r2;
  SPRAWDZ(rejestrV2(r2, "noce"));
  Magazyn m2(pl, lg.f(), r2);
  m2.migrujWMiejscu();
  int razem = 0;
  for (auto& s : m2.sciezki("noce")) razem += (int)m2.czytaj("noce", s).wiersze.size();
  std::vector<std::string> v0;
  if (pl.pliki.count("/noce.csv")) razem += (int)wiersze(pl.pliki["/noce.csv"]).size();
  printf("  r05: po migracji 1 → 2 nocy gdziekolwiek na partycji: %d z 400\n", razem);
  SPRAWDZ(razem >= 400);  // pada
}

// r06, T-WYM-43 i D2: powrót do poprzedniej wersji przez bootloader (zanik zasilania, watchdog albo błąd programu przy
// stanie „do sprawdzenia”, ESP-IDF rollback) nie woła wycofajMigracje(); stara wersja dopisuje noc do `/noce.csv`,
// a ponowna próba nowej wersji pomija migrację (pliki w `/dane/noce/` już są): noc z czasu starej wersji przepada.
static void powrotPrzezBootloaderGubiNoc() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  std::vector<std::string> noce = noceV0(100, "2026-11-01");
  pl.pliki["/noce.csv"] = plikV0(noce);
  {
    Log lg;
    Magazyn m(pl, lg.f());
    SPRAWDZ(m.migrujNaNoweSciezki());
  }  // tu reset przed oznaczeniem wersji: bootloader wraca do poprzedniej, wycofajMigracje() nie zostało wywołane
  std::vector<std::string> nowe = noceV0(102, "2026-11-03");
  pl.pliki["/noce.csv"] = plikV0(nowe);  // poprzednia wersja przepisała /noce.csv z dwiema nowymi nocami
  Log lg;
  Magazyn m(pl, lg.f());
  m.migrujNaNoweSciezki();  // ponowne wgranie nowej wersji
  int wPlikach = 0;
  for (auto& s : m.sciezki("noce")) wPlikach += (int)m.czytaj("noce", s).wiersze.size();
  printf("  r06: nocy w /noce.csv 102, w /dane/noce/ %d\n", wPlikach);
  ROWNE_L(wPlikach, 102);  // pada: 100
}

// r07, D4 i T-WYM-53 (praca bez pliku): drugi uszkodzony plik pod tą samą ścieżką nadpisuje kopię `.uszkodzony`
// pierwszego; jedyna kopia rekordów z pierwszego uszkodzenia znika (bez karty).
static void drugieUszkodzenieNadpisujeKopie() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  const std::string s = "/dane/noce/2025.csv";
  std::vector<std::string> w;
  for (int i = 0; i < 300; i++) w.push_back(wierszFormatu("noce", i - 400));
  std::string pierwszy = plik("noce", 1, POLA_NOCE, w, false) + "#koniec 299\n";  // zła liczba = uszkodzony
  pl.pliki[s] = pierwszy;
  {
    Log lg;
    Magazyn m(pl, lg.f());
    SPRAWDZ(m.czytaj("noce", s).stan == StanPliku::USZKODZONY);
  }
  SPRAWDZ(pl.tresc(s + ".uszkodzony") == pierwszy);
  // później: nowy plik roku (np. noc 2025 z importu karty) i znów uszkodzony
  pl.pliki[s] = plik("noce", 1, POLA_NOCE, {wierszFormatu("noce", -300)}, false) + "#koniec 7\n";
  {
    Log lg;
    Magazyn m(pl, lg.f());
    m.czytaj("noce", s);
  }
  bool jest = false;
  for (auto& kv : pl.pliki) jest = jest || kv.second == pierwszy;
  SPRAWDZ(jest);  // pada: 300 rekordów z pierwszej kopii nadpisane
}

// r08, T-WYM-62 i T-WYM-51 („zostawiać poprzednią wersję pliku”): przywrócenie przy starcie zasilania na partycji bez
// nocy, ale z odpowiedziami czynników zapisanymi w czasie pracy bez historii; błąd zapisu w połowie przywracania →
// przywrocZKatalogu() usuwa wszystkie pliki, które już nadpisała, więc wcześniejsze `/dane/czynniki.csv` znika.
static void T_WYM_62_bladZapisuKasujeIstniejacyPlik() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  std::map<std::string, std::string> z = zrzut(30);
  for (auto& kv : z) pl.pliki["/przywracanie/" + kv.first] = kv.second;
  std::string odpowiedzi = plik("czynniki", 1, POLA_CZYNNIKI, {wierszFormatu("czynniki", 40)}, true);
  pl.pliki["/dane/czynniki.csv"] = odpowiedzi;
  pl.odmawiaZapisuPod = "/dane/sypialnia";  // np. błąd nośnika przy trzecim formacie
  Log lg;
  Magazyn m(pl, lg.f());
  Magazyn::WynikPrzywracania w = m.przywrocZKatalogu("/przywracanie");
  SPRAWDZ(!w.ok);
  ROWNE(pl.tresc("/dane/czynniki.csv"), odpowiedzi);  // pada: plik usunięty
}

// Port, który po wierszu komendy podaje treść po jednym bajcie co `odstepMs` zegara atrapy (powolne zakłócone łącze
// albo zawieszony skrypt na komputerze).
class PortKapiacy : public AtrapaPortu {
 public:
  PortKapiacy(Dziennik& d, AtrapaZegara& z) : AtrapaPortu(d), z_(z) {}
  size_t odRazu = 0;          // tyle pierwszych znaków bez czekania (wiersz komendy)
  uint32_t odstepMs = 4900;   // potem bajt co tyle ms
  int znak() override {
    if (znaki.empty()) return -1;
    if (odRazu) {
      odRazu--;
      return AtrapaPortu::znak();
    }
    if (z_.msTeraz < nast_) return -1;
    nast_ = z_.msTeraz + odstepMs;
    return AtrapaPortu::znak();
  }
 private:
  AtrapaZegara& z_;
  uint32_t nast_ = 0;
};

// r09, T-WYM-87 („WHILE … nie minęło 10 min”) i limit czasu każdej pętli: czytajTresc()
// liczy 5 s od ostatniego bajtu, bez limitu całkowitego, a pętla okna sprawdza 10 min dopiero po komendzie. Treść
// 300 B po bajcie co 4,9 s trzyma okno (bez Wi-Fi, bez pobrania, CPU aktywne) ok. 25 min; przy 4 MB godzinami.
static void T_WYM_87_oknoBezLimituCalkowitego() {
  PanelP03 s;
  partycjaSkasowana(s);
  PortKapiacy port(s.p.d, s.p.zegar);
  std::string tresc(300, 'x');
  std::string w = "W humor.csv 300 " + sha256Hex(tresc) + "\n";
  port.odRazu = w.size();
  for (char c : w + tresc) port.znaki.push_back((unsigned char)c);
  s.p.zegar.epoch = ep(2026, 11, 4, 7, 0);
  s.p.zegar.msTeraz = 0;
  s.p.start.stan = StanRtc();
  s.p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  s.p.start.powodWybudzenia = PowodWybudzenia::START;
  s.siec.pierwszeMs = UINT32_MAX;
  Adaptery ad{s.p.zegar, s.p.nvs, s.p.pliki, s.siec, s.p.ekran, s.p.przyciski, s.p.czujniki, s.p.dzwiek, s.p.start,
              s.p.straznik, s.p.sen, port, s.p.diag};
  Dyrygent dy(ad, s.konf);
  dy.wybudzenie();
  printf("  r09: pierwsze użycie sieci po %u s (okno 600 s)\n", (unsigned)(s.siec.pierwszeMs / 1000));
  SPRAWDZ(s.siec.pierwszeMs <= 10u * 60 * 1000 + LIMIT_W_TESTU_MS);  // pada: ok. 1470 s
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  RNF27_hostZaZnakiemZapytania();
  T_WYM_87_nocOdrzuconaPotemBezOkna();
  T_WYM_87_oknoPrzegapionePotemNigdy();
  T_WYM_87_powtorzenieKroku4Niemozliwe();
  T_WYM_43_zanikWTrakcieMigracjiRocznej();
  powrotPrzezBootloaderGubiNoc();
  drugieUszkodzenieNadpisujeKopie();
  T_WYM_62_bladZapisuKasujeIstniejacyPlik();
  T_WYM_87_oknoBezLimituCalkowitego();
  printf("testy_magazyn_brzegowe: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
