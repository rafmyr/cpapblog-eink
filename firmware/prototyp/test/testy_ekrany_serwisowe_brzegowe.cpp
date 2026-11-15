// SPDX-License-Identifier: GPL-3.0-or-later
// Testy Przegląd, 14.11.2026).
// Od poprawek po przeglądzie wpięte w tools/prototyp/testy.sh jako bramka: każda usterka (RS-xx) przerywa
// testy. Uruchamiane z katalogu firmware/prototyp (przypadek [i] czyta src/dyrygent.cpp).
// Plik wczytuje test/testy_magazyn_danych.cpp (panel na atrapach PanelP03, naU05(), zNocami()) z przemianowanym main.
// Dane syntetyczne.
#define main main_testow_magazynu_niewolany
#include "testy_magazyn_danych.cpp"
#undef main

#include "aktualizacja.h"
#include "ekrany_konfiguracji.h"

namespace rec {
int usterki = 0;
void usterka(const char* id, const std::string& opis) {
  usterki++;
  printf("USTERKA %s: %s\n", id, opis.c_str());
}
std::string linia(const std::vector<std::string>& v, const std::string& pocz) {
  for (auto& l : v)
    if (l.rfind(pocz, 0) == 0) return l;
  return "";
}
struct Pl : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)(y * 800 + x)] = c;
  }
  int czarne(int x0, int y0, int x1, int y1) const {
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[(size_t)(y * 800 + x)];
    return n;
  }
};
}  // namespace rec

// ---------- U-05 z dyrygenta: werdykt przy wierszach, których nie da się ocenić (ADR-0029 pkt 7) ----------
static void recU05_wszystkoDzialaBezOceny() {
  {  // panel jak w testU05_wierszeZDanychFirmware: nigdy nie pobrał nocy (`–`), a werdykt `Wszystko działa`
    PanelP03 s;
    zNocami(s, 30);
    TekstyWybudzenia t = naU05(s);
    std::string pob = rec::linia(t.linieTestu, "Ostatnie pobranie: "), w = rec::linia(t.linieTestu, "Werdykt: ");
    printf("  [a] %s | %s | %s\n", pob.c_str(), rec::linia(t.linieTestu, "Karta: ").c_str(), w.c_str());
    if (pob == "Ostatnie pobranie: –" && w == "Werdykt: Wszystko działa")
      rec::usterka("", "U-05: nigdy nie pobrano nocy (`–`), karta nieobsługiwana (`brak karty`), a pas `Wszystko działa`");
  }
  {  // bez pomiaru napięcia i bez stanu zasilania: `Bateria: brak odczytu`, werdykt `Wszystko działa`
    PanelP03 s;
    zNocami(s, 30);
    s.p.czujniki.o.bateriaV = NAN;
    TekstyWybudzenia t = naU05(s);
    std::string b = rec::linia(t.linieTestu, "Bateria: "), w = rec::linia(t.linieTestu, "Werdykt: ");
    printf("  [b] %s | %s\n", b.c_str(), w.c_str());
    if (b == "Bateria: brak odczytu" && w == "Werdykt: Wszystko działa")
      rec::usterka("", "U-05: `Bateria: brak odczytu` (nie oceniam) przy pasie `Wszystko działa`");
  }
}

// ---------- U-05: procent baterii na kablu (do 14.11.2026 na ekranie) ----------
static void recU05_procentNaKablu() {
  PanelP03 s;
  zNocami(s, 30);
  s.p.czujniki.zas = Zasilanie::ZEWNETRZNE;
  TekstyWybudzenia t = naU05(s);
  std::string b = rec::linia(t.linieTestu, "Bateria: ");
  bool procent = false;
  for (auto& l : t.diagnostykaTestu) procent = procent || l.find("Bateria 3,95 V") != std::string::npos;
  printf("  [c] na kablu: `%s` (diagnostyka z napięciem: %s)\n", b.c_str(), procent ? "tak" : "nie");
  if (b.find('%') == std::string::npos)
    rec::usterka("", "U-05 na kablu: `" + b + "`, procent ładowania zniknął z ekranu (w bazie `Bateria: 70%`)");
}

// ---------- U-05 bez Wi-Fi: długa sieć ze słabym sygnałem, długi adres z godziną ----------
static void recU05_dlugieWartosciZDyrygenta() {
  PanelP03 s;
  zNocami(s, 30);
  naU05(s);  // 10:00 połączenie
  snprintf(s.p.start.stan.wifiSsid, sizeof s.p.start.stan.wifiSsid, "%s", "Bardzo-Dluga-Nazwa-Sieci-Domowej");
  snprintf(s.p.start.stan.wifiIp, sizeof s.p.start.stan.wifiIp, "%s", "192.168.100.200");
  s.p.start.stan.wifiRssi = -90;
  s.siec.siecJest = false;
  s.p.start.budzace = BIT_PRAWY;
  s.p.przyciski.skrypt = {{0, BIT_PRAWY}, {2300, 0}};
  s.p.start.stan.ekran = (uint8_t)Ekran::FORMULARZ;
  TekstyWybudzenia t = wybudz(s, ep(2026, 11, 4, 12, 0), PowodWybudzenia::PRZYCISK);
  std::string wf = rec::linia(t.linieTestu, "Wi-Fi: "), ad = rec::linia(t.linieTestu, "Adres panelu: ");
  printf("  [d] %s | %s | %s\n", wf.c_str(), ad.c_str(), rec::linia(t.linieTestu, "Werdykt: ").c_str());
  if (wf.find("słaby") == std::string::npos)
    rec::usterka("", "U-05 Wi-Fi z usterką `słaby`, a na ekranie bez słowa `słaby`: " + wf);
  if (ad.find("(10:00)") == std::string::npos)
    rec::usterka("", "U-05 `Adres panelu` bez pełnej godziny ostatniego połączenia: " + ad);
}

// ---------- U-05: `Ostatnie pobranie` po dwóch nocach bez terapii ----------
static void recU05_dwieNoceBezTerapii() {
  PanelP03 s;
  zNocami(s, 30);
  // ostatnia pobrana noc 02.11 05:32, potem dwie noce bez terapii (nic do pobrania), U-05 04.11 10:00 = 52,5 h
  s.p.nvs.liczbyU[std::string("cpap/") + NVS_KLUCZ_POBRANIE_NOCY] = ep(2026, 11, 2, 5, 32);
  TekstyWybudzenia t = naU05(s);
  std::string p = rec::linia(t.linieTestu, "Ostatnie pobranie: "), w = rec::linia(t.linieTestu, "Werdykt: ");
  printf("  [e] %s | %s\n", p.c_str(), w.c_str());
  if (w.find("pobieranie nocy") != std::string::npos)
    rec::usterka("", "U-05: dwie noce bez terapii (AirBridge działa) → `" + w + "` (fałszywy alarm, próg 48 h)");
}

// ---------- U-07: każde zdanie stanu na ekranie, układ w każdym stanie i powodzie ----------
static void recU07_wszystkieStany() {
  using namespace aktualizacja;
  const StanU07 stany[] = {StanU07::SPRAWDZANIE, StanU07::AKTUALNA, StanU07::NOWSZA, StanU07::POBIERANIE,
                           StanU07::INSTALACJA, StanU07::SAMOSPRAWDZENIE, StanU07::PO_UDANEJ, StanU07::PO_NIEUDANEJ,
                           StanU07::WYLACZONE, StanU07::SPRAWDZENIE_NIEUDANE};
  int przypadkow = 0;
  for (StanU07 st : stany)
    for (int pw = 0; pw <= (int)POWOD_OSTATNI; pw++)
      for (bool dlugie : {false, true}) {
        DaneU07 d;
        d.stan = st;
        d.wlasna = dlugie ? "12.12.12-rc.12" : "1.0.0";
        d.nowa = dlugie ? "12.12.13-rc.12" : "1.1.0";
        d.sprawdzono = "30.12 23:59";
        d.powod = (Powod)pw;
        d.procentBaterii = 31;
        if (dlugie) d.coNowego = {std::string(40, 'W'), "Ąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąąą", "Zapis bez znaków"};
        TresciU07 t = trescU07(d);
        przypadkow++;
        // każde zdanie z `linie` na ekranie: zdanie główne, opis, pas albo wersje (pierwsze zdanie stanu „jest nowsza”)
        for (size_t i = 0; i < t.linie.size(); i++) {
          const std::string& l = t.linie[i];
          bool jest = l == t.glowne || l == t.pas;
          for (auto& o : t.opis) jest = jest || o == l;
          if (!jest && st == StanU07::NOWSZA && i == 0) jest = !t.wersjaZ.empty() && !t.wersjaNa.empty();
          if (!jest && (st == StanU07::POBIERANIE || st == StanU07::INSTALACJA) && i == 0) jest = false;
          if (!jest)
            rec::usterka("RS-x", "U-07 zdanie bez miejsca na ekranie: `" + l + "`");
        }
        // pas „Nie odłączaj zasilania.” dokładnie w stanach z karty
        bool pasOcz = st == StanU07::NOWSZA || st == StanU07::POBIERANIE || st == StanU07::INSTALACJA ||
                      st == StanU07::SAMOSPRAWDZENIE;
        if (pasOcz != !t.pas.empty()) rec::usterka("RS-x", "U-07 pas w złym stanie");
        // rysunek: nic poniżej pasa (poza pasem i stopką), nic za prawym marginesem w polu treści, nic w szczelinie
        // między rysunkiem a kolumną tekstu
        rec::Pl p;
        rysujAktualizacje(p, t);
        int dol = t.pas.empty() ? 434 : U07_PAS_Y;
        int zaMarg = p.czarne(773, 44, 800, dol);
        int szczelina = p.czarne(U07_OBRAZEK_X + U07_OBRAZEK_W + 4, 44, U07_X - 2, dol);
        int podTrescia = t.pas.empty() ? p.czarne(0, 425, 800, 434) : p.czarne(0, U07_PAS_Y - 2, 800, U07_PAS_Y);
        if (zaMarg || szczelina || podTrescia) {
          char b[200];
          snprintf(b, sizeof b, "U-07 stan %d powód %d dlugie %d: za marginesem %d, szczelina %d, przy pasie/stopce %d",
                   (int)st, pw, (int)dlugie, zaMarg, szczelina, podTrescia);
          rec::usterka("RS-x", b);
        }
      }
  printf("  [f] U-07: %d przypadków stan × powód × długość\n", przypadkow);
}

// ---------- „Co nowego”: parsujOpis i pole `nowosci` nigdy nie blokuje instalacji ----------
static void recCoNowego_opis() {
  using namespace aktualizacja;
  const std::string baza = "{\"wersja\":\"1.0.1\",\"plytka\":\"reterminal_e1001\",\"uklad\":\"0x1a2b3c4d\",\"rozmiar\":1700000,"
                           "\"sha256\":\"" + std::string(64, 'a') + "\",\"plik\":\"cpap-eink-1.0.1.bin\"";
  struct P {
    const char* opis;
    std::string pole;
  } pp[] = {
      {"bez pola", ""},
      {"tekst", ",\"nowosci\":\"x\""},
      {"null", ",\"nowosci\":null"},
      {"obiekt", ",\"nowosci\":{\"a\":1}"},
      {"liczba", ",\"nowosci\":3"},
      {"lista mieszana", ",\"nowosci\":[1,null,true,\"  \",\"a\",\"b\",\"c\",\"d\"]"},
      {"zagnieżdżenie 20", ",\"nowosci\":" + std::string(20, '[') + std::string(20, ']')},
      {"samotny surogat", ",\"nowosci\":[\"\\ud800\"]"},
      {"\\u0000", ",\"nowosci\":[\"a\\u0000b\"]"},
      {"surowy znak sterujący", std::string(",\"nowosci\":[\"a") + "\x01" + "b\"]"},
      {"długi tekst 3000 B", ",\"nowosci\":[\"" + std::string(3000, 'x') + "\"]"},
      {"nowosci przed wersja (pole w środku)", ""},
  };
  for (auto& x : pp) {
    std::string j = baza + x.pole + "}";
    if (std::string(x.opis) == "nowosci przed wersja (pole w środku)")
      j = "{\"nowosci\":[\"a\"]," + baza.substr(1) + "}";
    OpisWydania o;
    bool ok = parsujOpis(j, o);
    printf("  [g] opis z polem `%s`: %s, linii %zu\n", x.opis, ok ? "poprawny" : "ODRZUCONY", o.nowosci.size());
    if (!ok)
      rec::usterka("", std::string("opis wydania z polem nowosci `") + x.opis +
                                "` odrzucony (instalacja zablokowana jako „plik uszkodzony”)");
  }
  // UTF-8: linia 41 znaków z 4-bajtowym znakiem i z niepoprawnym bajtem
  aktualizacja::Json j;
  std::string t = "[\"" + std::string(39, 'a') + "\xF0\x9F\x98\x80" + "b\",\"x\xFFy\",\"\xC4\"]";
  if (parsujJson(t, j)) {
    std::vector<std::string> n = nowosciZJson(&j);
    for (auto& l : n) {
      std::string e = liniaCoNowego(l, 500);
      bool zle = false;
      for (size_t i = 0; i < e.size();) {  // poprawny UTF-8 po liniaCoNowego()
        unsigned char c = (unsigned char)e[i];
        int k = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        if (k < 0 || i + (size_t)k >= e.size() + (k ? 0 : 1)) { zle = true; break; }
        for (int q = 1; q <= k; q++) zle = zle || ((unsigned char)e[i + (size_t)q] & 0xC0) != 0x80;
        i += (size_t)k + 1;
      }
      printf("  [h] nowość `%s` → `%s`%s\n", l.c_str(), e.c_str(), zle ? " (niepoprawny UTF-8)" : "");
      if (zle) rec::usterka("", "liniaCoNowego() przepuszcza niepoprawny UTF-8 do rysowania");
    }
  } else {
    printf("  [h] parser JSON odrzuca niepoprawny UTF-8 w tekście (dobrze)\n");
  }
}

// ---------- „Co nowego” w NVS: zapis wersji przed liniami ----------
static void recCoNowego_nvsKolejnosc() {
  // Kolejność w zapiszCoNowego(): najpierw `u07_cnw` (wersja), potem `u07_cn` (linie). Gdy drugi zapis się nie uda
  // (pełna NVS), wersja jest nowa, a linie stare: U-07 pokaże nowości poprzedniej wersji pod nową.
  // Sprawdzenie na kodzie źródłowym (dyrygent nie ma atrapy błędu zapisu NVS dla jednego klucza).
  std::ifstream f("src/dyrygent.cpp");
  std::stringstream b;
  b << f.rdbuf();
  std::string s = b.str();
  size_t w = s.find("ustawTekst(aktualizacja::PRZESTRZEN, \"u07_cnw\""),
         l = s.find("ustawTekst(aktualizacja::PRZESTRZEN, \"u07_cn\"");
  printf("  [i] zapis u07_cnw na %zu, u07_cn na %zu\n", w, l);
  if (w != std::string::npos && l != std::string::npos && w < l)
    rec::usterka("", "zapiszCoNowego(): wersja zapisywana przed liniami i bez sprawdzenia wyniku zapisu");
}

// ---------- U-01: każdy dawny stan wiersza oprogramowania ma odpowiednik na ekranie ----------
static void recU01_stanOprogramowania() {
  using namespace aktualizacja;
  // bez tokenu: dawny wiersz mówił, gdzie to włączyć; kafelek tylko `aktualizacje wyłączone`
  std::string dawny = wierszU01(false, "", "1.0.0");
  KafelekOprogramowania k = kafelekU01(false, "", "1.0.0", "");
  printf("  [j] bez tokenu: dawniej `%s`, teraz `%s`\n", dawny.c_str(), k.stan.c_str());
  if (dawny.find("Więcej na telefonie") != std::string::npos && k.stan.find("telefon") == std::string::npos)
    rec::usterka("", "U-01 bez tokenu: zniknęła wskazówka `Ustaw je w „Więcej na telefonie”.` (jest tylko na U-07)");
  // długa wersja: `jest {X} · zainstaluj` traci `· zainstaluj`
  DaneUstawien d;
  d.wersja = "12.12.12-rc.12";
  k = kafelekU01(true, "12.12.13-rc.12", d.wersja, "");
  d.stanOprogramowania = k.stan;
  d.stanOdwrocony = k.odwrocony;
  std::string tytul;
  std::vector<std::string> l = linieKafelkaOprogramowania(d, tytul);
  std::string razem;
  for (auto& x : l) razem += x + " / ";
  printf("  [k] długa wersja: `%s` / %s\n", tytul.c_str(), razem.c_str());
  // po przeglądzie: skrót `jest {X} · instaluj`, czynność nie znika
  if (razem.find("instaluj") == std::string::npos)
    rec::usterka("", "U-01 kafelek ↻ z długą wersją: `jest {X}` bez `· zainstaluj` (droga do akcji tylko z tytułu ↻)");
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  recU05_wszystkoDzialaBezOceny();
  recU05_procentNaKablu();
  recU05_dlugieWartosciZDyrygenta();
  recU05_dwieNoceBezTerapii();
  recU07_wszystkieStany();
  recCoNowego_opis();
  recCoNowego_nvsKolejnosc();
  recU01_stanOprogramowania();
  printf("testy_ekrany_serwisowe_brzegowe: %d ok, %d błędów sprawdzeń, %d usterek z przeglądu\n", ok, zle, rec::usterki);
  return rec::usterki || zle ? 1 : 0;
}
