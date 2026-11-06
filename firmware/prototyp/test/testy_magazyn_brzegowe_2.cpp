// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych: okno przywracania „do skutku”, scalWiersze(), znacznik migracji, rotacja
// `.uszkodzony`, narzędzia przejścia. Każdy test sNN_… ma przejść bez
// zmiany oczekiwań.
//
// Pomocnicze funkcje i atrapy (PanelP03, wybudz(), zrzut(), komendaW(), plik(), noceV1() …) bierzemy z testów autora
// przez dołączenie pliku z podmienioną nazwą main, jak test/testy_magazyn_brzegowe.cpp. Dane syntetyczne.
#define main testy_magazyn_danych_main
#include "testy_magazyn_danych.cpp"
#undef main

// Restart panelu w trakcie wybudzenia (odłączenie zasilania, reset przez DTR/RTS z wgraj_pliki.py --reset, strażnik):
// atrapa portu rzuca ten wyjątek, gdy minie `poMs` ms zegara atrapy od wyczerpania bajtów; dyrygent przerywa pracę
// w tym miejscu (nic po nim się nie wykonuje, jak przy prawdziwym resecie).
struct ResetPanelu {};
class PortZResetem : public AtrapaPortu {
 public:
  PortZResetem(Dziennik& d, AtrapaZegara& z) : AtrapaPortu(d), z_(z) {}
  uint32_t poMs = 30000;
  int znak() override {
    if (!znaki.empty()) return AtrapaPortu::znak();
    if (!pusto_) {
      pusto_ = true;
      od_ = z_.msTeraz;
    }
    if (z_.msTeraz - od_ >= poMs) throw ResetPanelu();
    return -1;
  }
 private:
  AtrapaZegara& z_;
  bool pusto_ = false;
  uint32_t od_ = 0;
};

// Start zasilania z portem, który po bajtach `b` i `poMs` ms ciszy resetuje panel (wyjątek złapany tu).
static bool startZResetem(PanelP03& s, uint32_t t, const std::string& b, uint32_t poMs) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  s.p.start.stan = StanRtc();
  s.p.start.przyczynaStartu = PrzyczynaStartu::ZASILANIE;
  s.p.start.powodWybudzenia = PowodWybudzenia::START;
  s.siec.polaczonaTeraz = false;
  s.siec.okno = false;
  s.siec.pierwszeMs = UINT32_MAX;
  s.p.d.z.clear();
  s.p.port.log.clear();
  PortZResetem port(s.p.d, s.p.zegar);
  port.poMs = poMs;
  for (char c : b) port.znaki.push_back((unsigned char)c);
  Adaptery ad{s.p.zegar, s.p.nvs, s.p.pliki, s.siec, s.p.ekran, s.p.przyciski, s.p.czujniki, s.p.dzwiek, s.p.start,
              s.p.straznik, s.p.sen, port, s.p.diag};
  bool reset = false;
  try {
    Dyrygent dy(ad, s.konf);
    dy.wybudzenie();
  } catch (const ResetPanelu&) {
    reset = true;
  }
  for (auto& l : port.log) s.p.port.log.push_back(l);
  return reset;
}

static std::string nvsCpap(PanelP03& s, const char* k) {
  auto i = s.p.nvs.teksty.find(std::string("cpap/") + k);
  return i == s.p.nvs.teksty.end() ? "" : i->second;
}

// Czy w `/dane/czynniki.csv` jest wpis z odpowiedzią (stan O) z dnia `data`.
static bool odpowiedzZDnia(PanelP03& s, const std::string& data) {
  for (auto& w : wiersze(s.p.pliki.tresc("/dane/czynniki.csv"))) {
    WpisCzynnikow x;
    if (parsujWpis(w, x) && x.data == data && x.stan == 'O') return true;
  }
  return false;
}

// s01, T-WYM-87 (U2 wariant A), 8.9 krok 4, komunikat przejscie_32mb.py i wgraj_pliki.py („nie wszystkie pliki przyjęte,
// K nie wysłane … powtórz krok 4 … --reset”): plik czynników odrzucony (zły skrót na kablu), narzędzie nie wysyła `K`
// i radzi restart; restart w trakcie okna → lista odrzuconych nie trafia do NVS (zapiszOdrzucone() dopiero na końcu
// okna); przy następnym starcie zasilania przywroc() przed oknem (wczytajPliki()) przywraca pliki z `/przywracanie/`
// bez czynników i stawia znacznik `przywr` → okno się nie otwiera, poprawny plik czynników: `BLAD … poza oknem`.
// Odpowiedzi „Co było inaczej?” (nieodtwarzalne) przepadają, choć narzędzie obiecuje, że okno wróci.
static void s01_T_WYM_87_resetWOknieStawiaZnacznikBezCzynnikow() {
  PanelP03 s;
  partycjaSkasowana(s);
  std::map<std::string, std::string> z = zrzut(40);
  std::string b;
  for (auto& kv : z) {
    std::string k = komendaW(kv.first, kv.second);
    if (kv.first == "czynniki.csv") k[k.size() - 5] = k[k.size() - 5] == '1' ? '2' : '1';
    b += k;
  }
  blokada(s, 0);
  bool reset = startZResetem(s, ep(2026, 11, 4, 7, 0), b, 30000);  // bez `K`, restart 30 s po ostatnim bajcie
  SPRAWDZ(reset);
  SPRAWDZ(logMa(s, "BLAD czynniki.csv skrot"));
  SPRAWDZ(logMa(s, "OK noce.csv"));
  printf("  s01: po resecie w oknie przywr_odrz w NVS: \"%s\"\n", nvsCpap(s, NVS_KLUCZ_PRZYWR_ODRZ).c_str());
  // wgraj_pliki.py … --reset: start zasilania, `?`, poprawny plik czynników, `K`
  doPortu(s, "?" + komendaW("czynniki.csv", z["czynniki.csv"]) + "K");
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 7, 20), PowodWybudzenia::START);
  printf("  s01: znacznik przywr po drugim starcie: \"%s\"\n", nvsCpap(s, NVS_KLUCZ_PRZYWR).c_str());
  SPRAWDZ(!logMa(s, "przywracanie: poza oknem"));     // pada: okno się nie otworzyło
  SPRAWDZ(logMa(s, "OK czynniki.csv"));               // pada: `BLAD czynniki.csv poza oknem`
  SPRAWDZ(odpowiedzZDnia(s, dodajDni("2026-10-15", 0)));  // pada: odpowiedzi ze zrzutu nie wróciły
}

// s02, T-WYM-87 i 8.9 krok 4: narzędzie nie wysłało jednego pliku (np. `brak odpowiedzi` po zakłóconym wierszu komendy,
// skrypt przerwany, plik ponad 4 MB pominięty przez wgraj_pliki.py), więc nie wysłało `K`; okno kończy się po 10 min,
// panel przywraca to, co przyszło, i stawia znacznik (plik, którego nie było, nie jest „odrzucony”). Brakujący plik
// już nie wejdzie, choć narzędzie pisze „okno wraca przy każdym starcie zasilania do udanego przywrócenia”.
static void s02_T_WYM_87_koniecCzasuBezKompletuStawiaZnacznik() {
  PanelP03 s;
  partycjaSkasowana(s);
  std::map<std::string, std::string> z = zrzut(40);
  std::string b;
  for (auto& kv : z)
    if (kv.first != "czynniki.csv") b += komendaW(kv.first, kv.second);  // czynniki nie dotarły, `K` nie wysłane
  doPortu(s, b);
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "okno przywracania: koniec czasu"));
  SPRAWDZ(logMa(s, "przywrócono 40 z 40"));
  doPortu(s, komendaW("czynniki.csv", z["czynniki.csv"]) + "K");
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "OK czynniki.csv"));                   // pada: `BLAD czynniki.csv poza oknem`
  SPRAWDZ(odpowiedzZDnia(s, dodajDni("2026-10-15", 0)));  // pada
}

// s03, T-WYM-87 i T-WYM-62: plik nocy zrzutu pusty (np. wgraj_pliki.py wywołany wprost na zrzucie, w którym zrzut
// starego firmware dał pustą sekcję `/noce.csv`; wgraj_pliki.py, w przeciwieństwie do przejscie_32mb.py, nie
// sprawdza liczby nocy) → `przywrócono 0 z 0`, przywrócenie „udane”, znacznik `przywr` z 0 nocy: okno już nigdy się
// nie otworzy, prawdziwego zrzutu nie da się wgrać.
static void s03_T_WYM_62_zeroNocyToNieSukces() {
  PanelP03 s;
  partycjaSkasowana(s);
  std::map<std::string, std::string> z = zrzut(40);
  doPortu(s, komendaW("noce.csv", "") + komendaW("czynniki.csv", z["czynniki.csv"]) + "K");
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  printf("  s03: znacznik przywr po zrzucie z pustym plikiem nocy: \"%s\"\n", nvsCpap(s, NVS_KLUCZ_PRZYWR).c_str());
  SPRAWDZ(nvsCpap(s, NVS_KLUCZ_PRZYWR).empty());  // pada: `2026-11-04 0`
  std::string b;
  for (auto& kv : z) b += komendaW(kv.first, kv.second);
  doPortu(s, b + "K");
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "przywrócono 40 z 40"));  // pada: `BLAD … poza oknem`
}

// s04, T-WYM-62 (scalenie, karta D40): czynniki z tą samą datą → zawsze wiersz z flash. Wiersz we flash `C` (czeka,
// zaproszenie po nocy pobranej w pracy bez historii) wygrywa z odpowiedzią `O` ze zrzutu z tej samej doby (zrzut
// zrobiony rano po odpowiedzi, przed przejściem): odpowiedź „Co było inaczej?” przepada (tak samo `B`, brak
// odpowiedzi po terminie). Ta sama reguła w scalZrodlo() po powrocie do poprzedniej wersji (odpowiedź udzielona
// w starej wersji, `C` w nowej).
static void s04_T_WYM_62_scalenieCzynnikowGubiOdpowiedz() {
  std::string czeka = zapiszWpis(WpisCzynnikow{"2026-11-03", 'C', 0, 'Z', 600, true});
  std::string brak = zapiszWpis(WpisCzynnikow{"2026-11-02", 'B', 0, 'Z', 600, true});
  std::string odp3 = zapiszWpis(WpisCzynnikow{"2026-11-03", 'O', CZ_KOLACJA, 'K', -1, true});
  std::string odp2 = zapiszWpis(WpisCzynnikow{"2026-11-02", 'O', CZ_STRES, 'K', -1, true});
  std::vector<std::string> w = scalWiersze("czynniki", {brak, czeka}, {odp2, odp3});
  ROWNE_L(w.size(), 2u);
  if (w.size() == 2) {
    ROWNE(w[0], odp2);  // pada: `B` z flash
    ROWNE(w[1], odp3);  // pada: `C` z flash
  }
  // to samo w magazynie: przywrócenie zrzutu na partycji z wpisem `C` z pracy bez historii
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  std::map<std::string, std::string> z = zrzut(30);
  z["czynniki.csv"] = plikV0({odp2, odp3});
  for (auto& kv : z) pl.pliki["/przywracanie/" + kv.first] = kv.second;
  pl.pliki["/dane/czynniki.csv"] = plik("czynniki", 1, POLA_CZYNNIKI, {brak, czeka}, true);
  Log lg;
  Magazyn m(pl, lg.f());
  SPRAWDZ(m.przywrocZKatalogu("/przywracanie").ok);
  std::vector<std::string> po = m.czytaj("czynniki", "/dane/czynniki.csv").wiersze;
  SPRAWDZ(zawiera(po, odp3));  // pada
}

// s05, T-WYM-62 (scalenie): dziennik wyświetleń (`humor`, kilka wierszy na dobę) scalany po samej dacie: gdy flash ma
// jakikolwiek wiersz z doby, wszystkie wiersze zrzutu z tej doby przepadają (rotacje i „bez powtórzeń” R-HUM liczą
// się z niepełnego dziennika).
static void s05_T_WYM_62_dziennikTejSamejDoby() {
  std::string f1 = zapiszHumor(WpisHumoru{"2026-11-03", "S2A", "7", 'D', "", ""});
  std::string z1 = zapiszHumor(WpisHumoru{"2026-11-03", "S2A", "3", 'D', "", ""});
  std::string z2 = zapiszHumor(WpisHumoru{"2026-11-03", "S5", "12", 'K', "noc-1", "zdanie 4"});
  std::vector<std::string> w = scalWiersze("humor", {f1}, {z1, z2});
  ROWNE_L(w.size(), 3u);  // pada: 1
  SPRAWDZ(zawiera(w, z2));  // pada
}

// s06, T-WYM-87 (okno „do skutku”, U2 wariant A): panel sformatowany przez firmware (`fs_init`), który nigdy nie
// przywracał (nowy egzemplarz bez prototypu, egzemplarz po naprawie skasowanej pamięci, użytkownik zrezygnował ze
// zrzutu), otwiera okno przy każdym starcie zasilania na zawsze: po roku pobierania (365 nocy) restart po aktualizacji
// przez Wi-Fi, wymiana baterii, reset strażnika → 10 min bez Wi-Fi i bez pobrania. Przycisk kończy tylko to jedno okno.
static void s06_T_WYM_87_oknoNaZawszeBezPrzywracania() {
  PanelP03 s;
  zNocami(s, 365);
  ustawZnacznik(s);  // partycja sformatowana przez firmware, przywracania nigdy nie było
  blokada(s, 0);
  s.p.przyciski.skrypt = {{30000, BIT_SRODEK_P03}, {30300, 0}};  // użytkownik zamyka okno przyciskiem
  wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);
  SPRAWDZ(logMa(s, "okno przywracania: zakończone przyciskiem"));
  s.p.przyciski.skrypt.clear();
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 5, 9, 0), PowodWybudzenia::START);  // np. restart po aktualizacji
  printf("  s06: panel z 365 nocami, start zasilania: pierwsze użycie sieci po %u s\n",
         (unsigned)(s.siec.pierwszeMs / 1000));
  SPRAWDZ(!logMa(s, "okno przywracania do"));  // pada
  SPRAWDZ(s.siec.pierwszeMs < 60u * 1000);      // pada: 600 s
}

// s07, U7 rundy 1 i T-WYM-53: rotacja `.uszkodzony` usuwa najstarszą kopię bez względu na jej treść; plik roku
// z 300 nocami uszkodzony, potem trzy kolejne uszkodzenia nowego, małego pliku (ta sama przyczyna, np. błąd nośnika
// albo programu) → kopia z 300 nocami usunięta. Rozważany wariant: nie kasować najstarszej, gdy jest większa.
static void s07_U7_rotacjaUsuwaDuzaKopie() {
  Dziennik d;
  AtrapaPlikow pl(d);
  montowana(pl);
  const std::string sc = "/dane/noce/2025.csv";
  std::vector<std::string> w;
  for (int i = 0; i < 300; i++) w.push_back(wierszFormatu("noce", i - 400));
  std::string duzy = plik("noce", 1, POLA_NOCE, w, false) + "#koniec 299\n";
  pl.pliki[sc] = duzy;
  for (int i = 0; i < 4; i++) {
    Log lg;
    Magazyn m(pl, lg.f());
    SPRAWDZ(m.czytaj("noce", sc).stan == StanPliku::USZKODZONY);
    pl.pliki[sc] = plik("noce", 1, POLA_NOCE, {wierszFormatu("noce", -300 + i)}, false) + "#koniec 9\n";
  }
  bool jest = false;
  for (auto& kv : pl.pliki) jest = jest || kv.second == duzy;
  SPRAWDZ(jest);  // pada: 300 nocy usunięte przy czwartym uszkodzeniu
}

// s08, T-WYM-43 (migracja „przy starcie”), R-NF-01 (energia): po U5/U6 migrujNaNoweSciezki() przy każdym wybudzeniu
// (także zegarem co kilkadziesiąt minut) czyta w całości wszystkie pliki prototypu, które zostają na partycji do
// migracji 1 → 2 (`/noce.csv`, `/humor.csv`, `/bateria.csv` …), i liczy z nich CRC, żeby porównać ze znacznikiem.
// Przed poprawką wystarczało sprawdzenie, czy plik istnieje. Plik prototypu może się zmienić tylko w poprzedniej wersji
// firmware, czyli przed startem zasilania (powrót przez bootloader albo wgranie), więc wybudzenie z uśpienia nie
// musi go czytać. [hipoteza co do kosztu energii na egzemplarzu: do zmierzenia]
static void s08_T_WYM_43_plikiPrototypuCzytaneCoWybudzenie() {
  PanelP03 s;
  s.p.pliki.pliki["/noce.csv"] = plikV0(noceV0(400, "2026-11-03"));
  std::vector<std::string> h;
  for (int i = 0; i < 2000; i++)
    h.push_back(zapiszHumor(WpisHumoru{dodajDni("2021-01-01", i), "S2A", std::to_string(i % 40), 'D', "", ""}));
  s.p.pliki.pliki["/humor.csv"] = plikV0(h);
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 9, 0), PowodWybudzenia::START);  // migracja 0 → 1, znacznik `ok`
  SPRAWDZ(s.p.pliki.pliki.count(znacznikMigracji("noce")) == 1);
  s.p.pliki.odczyty.clear();
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 9, 30), PowodWybudzenia::ZEGAR);
  int noce = s.p.pliki.odczyty["/noce.csv"], humor = s.p.pliki.odczyty["/humor.csv"];
  printf("  s08: wybudzenie zegarem po migracji: odczytów /noce.csv %d, /humor.csv %d (%zu B i %zu B)\n", noce, humor,
         s.p.pliki.pliki["/noce.csv"].size(), s.p.pliki.pliki["/humor.csv"].size());
  ROWNE_L(noce, 0);   // pada
  ROWNE_L(humor, 0);  // pada
}

// s09, T-WYM-39 (`Pamięć panelu` przy problemie: `odrzucone {n} pliki`) i 8.9 krok 5 („sprawdź U-05”): lista
// odrzuconych plików jest wczytywana z NVS tylko w oknie przywracania albo w przywroc(), więc U-05 otwarte zwykłym
// przyciskiem (w dzień po przejściu) pokazuje `wolne {n} MB`, choć przywracanie czeka na plik czynników.
static void s09_T_WYM_39_odrzuconeNiewidocznePozaStartem() {
  PanelP03 s;
  partycjaSkasowana(s);
  std::map<std::string, std::string> z = zrzut(40);
  std::string b;
  for (auto& kv : z) {
    std::string k = komendaW(kv.first, kv.second);
    if (kv.first == "czynniki.csv") k[k.size() - 5] = k[k.size() - 5] == '1' ? '2' : '1';
    b += k;
  }
  doPortu(s, b + "K");
  blokada(s, 0);
  wybudz(s, ep(2026, 11, 4, 7, 0), PowodWybudzenia::START);
  SPRAWDZ(!nvsCpap(s, NVS_KLUCZ_PRZYWR_ODRZ).empty());
  TekstyWybudzenia t = naU05(s);
  std::string pam = "?";
  for (auto& l : t.linieTestu)
    if (l.compare(0, 15, "Pamięć panelu") == 0) pam = l;
  printf("  s09: U-05 przyciskiem po przejściu z odrzuconym plikiem: `%s`\n", pam.c_str());
  SPRAWDZ(zawiera(t.linieTestu, "Pamięć panelu: odrzucone 1 plik"));  // pada: `wolne … MB`
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  s01_T_WYM_87_resetWOknieStawiaZnacznikBezCzynnikow();
  s02_T_WYM_87_koniecCzasuBezKompletuStawiaZnacznik();
  s03_T_WYM_62_zeroNocyToNieSukces();
  s04_T_WYM_62_scalenieCzynnikowGubiOdpowiedz();
  s05_T_WYM_62_dziennikTejSamejDoby();
  s06_T_WYM_87_oknoNaZawszeBezPrzywracania();
  s07_U7_rotacjaUsuwaDuzaKopie();
  s08_T_WYM_43_plikiPrototypuCzytaneCoWybudzenie();
  s09_T_WYM_39_odrzuconeNiewidocznePozaStartem();
  printf("testy_magazyn_brzegowe_2: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
