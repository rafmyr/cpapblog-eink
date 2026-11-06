// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: pełne ekrany zdarzeń E w tonie „problem” (S3, 6.4)
// z wyborem według tabeli 7.2, rytm „ten sam problem poranny” (R-KON-14, 7.4) liczony z rzeczywistych pokazań
// (R-PIE-08), linia `Wciąż: {tytuł}.` tylko po „wiem” i w dniach limitu (R-EKR-22, R-EKR-40), pilność A zawsze widoczna
// (5.12.2), karty „Otwartych spraw” (R-EKR-48), limity tekstów (5.4.4) i rysowanie S3 ze strefą linii uwagi.
// Dane syntetyczne. Uruchomienie: tools/prototyp/testy.sh (program testy_ekrany_zdarzen_e).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "dzien.h"
#include "ekrany.h"
#include "ekrany_zdarzen.h"
#include "kolejka.h"
#include "plan.h"
#include "sprawy.h"
#include "wspolne.h"
#include "zdarzenia_ekrany_e.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                       \
  do {                                                                   \
    if (w) ok++;                                                         \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); } \
  } while (0)
#define ROWNE(a, b)                                                                                              \
  do {                                                                                                           \
    std::string a_ = (a), b_ = (b);                                                                              \
    if (a_ == b_) ok++;                                                                                          \
    else { zle++; printf("BŁĄD %s:%d: %s = „%s”, ma być „%s”\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); } \
  } while (0)

using reguly::Pilnosc;
using sprawy::SprawaL;

static const char* const E_EKRANY[] = {"E-30", "E-31/obserwuj", "E-31/lekarz", "E-33", "E-36/obserwuj", "E-36/lekarz",
                                       "E-23", "E-22", "E-21", "E-10/problem", "E-12", "E-13", "E-14"};

static int znaki(const std::string& s) {  // punkty kodowe UTF-8 (limity 5.4.4 liczą znaki)
  int n = 0;
  for (unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}

// ---------- sprawy z wyników: każde E z poziomem „problem” ma S3 ----------
static void testSprawyE() {
  for (const char* kl : E_EKRANY) {
    SprawaL s = b14::sprawaE(kl);
    bool jest = s.klucz == kl;
    SPRAWDZ(jest);
    if (!jest) { printf("    brak sprawy %s\n", kl); continue; }
    SPRAWDZ(s.ton == sprawy::Ton::PROBLEM && sprawy::problemPoranny(s) && s.jestE());
    SPRAWDZ(!s.tytul.empty() && !s.powod.empty() && !s.krok.empty());  // teksty z zestawu (6.4)
    SPRAWDZ(!s.metrykaWiz.empty());
    SPRAWDZ(sprawy::pelnyEkranDzis(s, "2026-11-04"));  // dzień wejścia: pełny ekran
  }
  // poziom „linia” (E-11, E-20, E-10/linia, E-04) i E-37 (zablokowane) nie są sprawą S3
  for (const char* kl : {"E-11", "E-20", "E-10/linia", "E-04"}) SPRAWDZ(b14::sprawaE(kl).klucz.empty());
  // metryki i progi wykresu (5.4.2 S3: 30 nocy z progiem albo normą)
  SprawaL e23 = b14::sprawaE("E-23"), e12 = b14::sprawaE("E-12"), e22 = b14::sprawaE("E-22");
  ROWNE(e23.metrykaWiz, "M-07");
  SPRAWDZ(std::fabs(e23.progLinii - 27.0) < 1e-9);
  ROWNE(e12.metrykaWiz, "M-01");
  SPRAWDZ(std::fabs(e12.progLinii - 240.0) < 1e-9);  // 4 h w minutach M-01
  ROWNE(e22.metrykaWiz, "M-08");
  SprawaL e36 = b14::sprawaE("E-36/obserwuj");
  ROWNE(zdarzenia::metrykaWykresu(e36, reguly::Tryb::ASVAUTO), "M-06p");  // 4.11: EPAP w trybie ASV
  ROWNE(zdarzenia::metrykaWykresu(e36, reguly::Tryb::AUTOSET), "M-05p");
  SPRAWDZ(std::isnan(b14::sprawaE("E-21").progLinii));  // stan odchylenia: linia normy, nie progu
  // E-36 w CPAP: tytuł ze słowem trybu (4.11)
  ROWNE(b14::sprawaE("E-36/obserwuj", false, reguly::Tryb::CPAP).tytul, "Ciśnienie wyższe niż zwykle");
  ROWNE(e36.tytul, "EPAP wyższe niż zwykle");
}

// ---------- wykluczenia (7.2): E-31 przy E-30 trwa jako karta bez ekranu ----------
static void testWykluczenia() {
  std::vector<reguly::Wynik> w = {b14::wynikE("E-30", {{"n", "6"}, {"x", "8,1"}}),
                                  b14::wynikE("E-31/obserwuj", {{"x", "8,1"}, {"norma", "1,0"}}),
                                  b14::wynikE("E-31/lekarz", {{"x", "8,1"}, {"norma", "1,0"}})};
  w[1].spelniona = false;  // E-31/obserwuj wykluczone przez E-31/lekarz (ten sam problem): bez osobnej karty
  w[1].zmienne["wykluczona_przez"] = "E-31/lekarz";
  w[1].zmienne["spelniona_przed_wykluczeniem"] = "tak";
  w[2].spelniona = false;  // E-31/lekarz wykluczone przez E-30
  w[2].zmienne["wykluczona_przez"] = "E-30";
  w[2].zmienne["spelniona_przed_wykluczeniem"] = "tak";
  reguly::Pamiec p;
  std::vector<SprawaL> s = sprawy::sprawyOtwarte(b14::zestaw(), w, p, b14::funkcjaTekstu());
  SPRAWDZ(s.size() == 2);
  if (s.size() != 2) return;
  const SprawaL* e31 = s[0].id == "E-31" ? &s[0] : &s[1];
  SPRAWDZ(e31->klucz == "E-31/lekarz" && e31->wykluczona && e31->ton == sprawy::Ton::BRAK);
  SPRAWDZ(!sprawy::pelnyEkranDzis(*e31, "2026-11-04"));
  ROWNE(sprawy::sprawaEkranuDnia(s, "2026-11-04")->klucz, "E-30");
  sprawy::WejscieSpraw ws;
  ws.dzis = "2026-11-04";
  ws.sprawy = &s;
  ws.ekranDnia = sprawy::sprawaEkranuDnia(s, "2026-11-04");
  std::vector<sprawy::KartaSprawy> k = sprawy::kartySpraw(ws);
  SPRAWDZ(k.size() == 2);
  if (k.size() == 2) {
    ROWNE(k[0].dalej, "na ekranie dnia");  // E-30 (R5b, grupa (2)) zostaje na liście także jako ekran dnia
    ROWNE(k[1].dalej, "ustępuje innej sprawie, obserwacja trwa");
  }
  // wykluczona nie ma linii
  kolejka::WejscieKolejki wk;
  wk.dzis = "2026-11-04";
  wk.sprawy = &s;
  wk.ekranDnia = ws.ekranDnia;
  for (auto& l : kolejka::kolejkaLinii(wk)) SPRAWDZ(l.zrodlo != "E-31/lekarz");
}

// ---------- kolejność 7.2: R5b > R5c > R5d, w grupie według tabeli; A i B przed E ----------
static void testKolejnosc() {
  std::vector<SprawaL> s = {b14::sprawaE("E-10/problem"), b14::sprawaE("E-21"), b14::sprawaE("E-23"),
                            b14::sprawaE("E-30")};
  ROWNE(sprawy::sprawaEkranuDnia(s, "2026-11-04")->klucz, "E-30");  // E-30 > E-23 (utrwalone przed jednonocnym)
  s.pop_back();
  ROWNE(sprawy::sprawaEkranuDnia(s, "2026-11-04")->klucz, "E-23");  // R5b > R5c
  s.pop_back();
  ROWNE(sprawy::sprawaEkranuDnia(s, "2026-11-04")->klucz, "E-21");  // R5c > R5d
  std::vector<SprawaL> d = {b14::sprawaE("E-14"), b14::sprawaE("E-12"), b14::sprawaE("E-13")};
  ROWNE(sprawy::sprawaEkranuDnia(d, "2026-11-04")->klucz, "E-12");
  // E-22 przed E-21 (R5c)
  std::vector<SprawaL> c = {b14::sprawaE("E-21"), b14::sprawaE("E-22")};
  ROWNE(sprawy::sprawaEkranuDnia(c, "2026-11-04")->klucz, "E-22");
  // wzorzec B (R3) przed problemem porannym
  SprawaL b;
  b.klucz = b.id = "L-11";
  b.stan = reguly::StanReguly::AKTYWNA;
  b.pilnosc = Pilnosc::B;
  b.ton = sprawy::Ton::LEKARZ;
  b.tytul = "Senność mimo terapii";
  std::vector<SprawaL> lb = {b14::sprawaE("E-30"), b};
  ROWNE(sprawy::sprawaEkranuDnia(lb, "2026-11-04")->klucz, "L-11");
  // pilność A zawsze widoczna (5.12.2): niewyciszony A poza swoim dniem → linia A w grupie 0, a polem głównym kolejny
  // kandydat 7.2 ze strefą linii: S3 zdarzenia E; wzorzec B dalej czeka
  SprawaL a = b;
  a.klucz = a.id = "L-06/b";
  a.pilnosc = Pilnosc::A;
  a.tytul = "Saturacja: sprawa dla lekarza";
  a.pokazanoPelny = "2026-11-03";
  a.aktywnaOd = "2026-11-03";
  std::vector<SprawaL> la = {b14::sprawaE("E-30"), a};
  SPRAWDZ(sprawy::sprawaEkranuDnia(la, "2026-11-04") && sprawy::sprawaEkranuDnia(la, "2026-11-04")->klucz == "E-30");
  {
    std::vector<SprawaL> lab = {b, a};  // A poza swoim dniem i B: B bez ekranu (ekran L nie ma strefy linii)
    SPRAWDZ(sprawy::sprawaEkranuDnia(lab, "2026-11-04") == nullptr);
  }
  kolejka::WejscieKolejki wk;
  wk.dzis = "2026-11-04";
  wk.sprawy = &la;
  std::vector<kolejka::LiniaUwagi> k = kolejka::kolejkaLinii(wk);
  SPRAWDZ(!k.empty() && k.front().wzorzecA);
  // w dniu pełnego ekranu A ekranem jest A
  la[1].pokazanoPelny.clear();
  ROWNE(sprawy::sprawaEkranuDnia(la, "2026-11-04")->klucz, "L-06/b");
}

// ---------- rytm R-KON-14 i R-PIE-08 ----------
// Doba po dobie: pokazanie zapisane, gdy sprawa jest ekranem dnia (jak src/dyrygent.cpp rysujDzienB5()).
static std::vector<int> dniPokazan(SprawaL s, int dni, const std::vector<int>& przegrane = {}) {
  reguly::Pamiec p;
  std::vector<int> w;
  for (int i = 1; i <= dni; i++) {
    std::string dz = dodajDni("2026-11-01", i - 1);
    s.pokazanoPelny = sprawy::ostatniePokazanie(p, s.klucz);
    s.pokazanoPoprzednio = sprawy::ostatniePokazanie(p, s.klucz + "~poprz");
    bool przegral = false;
    for (int x : przegrane) przegral = przegral || x == i;
    if (!przegral && sprawy::pelnyEkranDzis(s, dz)) {
      w.push_back(i);
      sprawy::zapiszPokazanie(p, s.klucz, dz);
      s.pokazanoPelny = sprawy::ostatniePokazanie(p, s.klucz);
      s.pokazanoPoprzednio = sprawy::ostatniePokazanie(p, s.klucz + "~poprz");
      SPRAWDZ(sprawy::pelnyEkranDzis(s, dz));  // przez cały dzień pokazania (odświeżenia, powrót z K1)
    }
  }
  return w;
}
static std::string lista(const std::vector<int>& v) {
  std::string s;
  for (int x : v) s += (s.empty() ? "" : ",") + std::to_string(x);
  return s;
}
static void testRytm() {
  // przykład R-KON-14: E-21 aktywne 10 dni → ekran w dniach 1, 2, 8 i 9
  ROWNE(lista(dniPokazan(b14::sprawaE("E-21"), 10)), "1,2,8,9");
  // dzień przegrany z ekranem wyższej grupy nie zużywa limitu (R-PIE-08): przegrana w dniu 2 → 1, 3, 8, 10
  ROWNE(lista(dniPokazan(b14::sprawaE("E-30"), 10, {2})), "1,3,8,10");
  // L-09 też podlega R-KON-14 (7.4: „także L-09”), zamiast rytmu „co 7 dni”
  SprawaL l09;
  l09.klucz = l09.id = "L-09";
  l09.stan = reguly::StanReguly::AKTYWNA;
  l09.ton = sprawy::Ton::PROBLEM;
  l09.grupa = "R5c";
  l09.tytul = "Wyciek nie ustępuje";
  ROWNE(lista(dniPokazan(l09, 10)), "1,2,8,9");
  // „wiem” na zdarzeniu E: 7 dni bez pełnego ekranu (5.4.9), dzień naciśnięcia = dzień 1
  SprawaL e = b14::sprawaE("E-22");
  e.wiem = "2026-11-04";
  SPRAWDZ(sprawy::dniWiem(e) == 7);
  SPRAWDZ(!sprawy::pelnyEkranDzis(e, "2026-11-04") && !sprawy::pelnyEkranDzis(e, "2026-11-10"));
  SPRAWDZ(sprawy::pelnyEkranDzis(e, "2026-11-11"));  // R-EKR-24: po okresie wraca do wyboru
}

// ---------- linie `Wciąż: {tytuł}.` (R-EKR-22, R-EKR-40, 7.5 grupa 5) ----------
static std::vector<kolejka::LiniaUwagi> kol(const std::string& dzis, const std::vector<SprawaL>& s) {
  kolejka::WejscieKolejki w;
  w.dzis = dzis;
  w.sprawy = &s;
  w.ekranDnia = sprawy::sprawaEkranuDnia(s, dzis);
  return kolejka::kolejkaLinii(w);
}
static void testLinie() {
  SprawaL e30 = b14::sprawaE("E-30"), e21 = b14::sprawaE("E-21");
  // dzień wejścia: ekran, bez linii tej sprawy
  std::vector<SprawaL> s = {e30};
  SPRAWDZ(kol("2026-11-04", s).empty());
  // dzień limitu (dwa pokazania 1.11 i 2.11): linia zdrowia `Wciąż: …` w grupie 5 (R5b = ostrzeżenie)
  s[0].pokazanoPoprzednio = "2026-11-01";
  s[0].pokazanoPelny = "2026-11-02";
  SPRAWDZ(sprawy::limitProblemu(s[0], "2026-11-03"));
  std::vector<kolejka::LiniaUwagi> k = kol("2026-11-03", s);
  SPRAWDZ(k.size() == 1);
  if (!k.empty()) {
    ROWNE(k[0].tekst, "Wciąż: Noce z AHI powyżej 5.");
    SPRAWDZ(k[0].grupa == GrupaLinii::PRZYPOMNIENIE_R5B && k[0].zdrowie());
  }
  // E-21 (R5c) po „wiem”: przypomnienie bez `!`
  e21.wiem = "2026-11-03";
  std::vector<SprawaL> s2 = {e21};
  k = kol("2026-11-05", s2);
  SPRAWDZ(k.size() == 1 && k[0].tekst == "Wciąż: Sprawdź maskę." && k[0].grupa == GrupaLinii::PRZYPOMNIENIE &&
          !k[0].zdrowie());
  // sprawa przegrana w tabeli 7.2 (E-21 przy E-30 na ekranie) nie ma linii: czeka (R-EKR-03)
  std::vector<SprawaL> s3 = {b14::sprawaE("E-30"), b14::sprawaE("E-21")};
  SPRAWDZ(kol("2026-11-04", s3).empty());
  // dwie sprawy w grupie 5: R5b przed R5c (fail-path R-EKR-40)
  std::vector<SprawaL> s4 = {e21, s[0]};
  k = kol("2026-11-03", s4);
  SPRAWDZ(k.size() == 2 && k[0].zrodlo == "E-30" && k[1].zrodlo == "E-21");
  // „Otwarte sprawy”: limit i „wiem” w wierszu `Dalej:`, problem R5c na ekranie dnia bez karty (R-EKR-48 (4))
  sprawy::WejscieSpraw ws;
  ws.dzis = "2026-11-03";
  ws.sprawy = &s4;
  std::vector<sprawy::KartaSprawy> kr = sprawy::kartySpraw(ws);
  SPRAWDZ(kr.size() == 2);
  if (kr.size() == 2) {
    ROWNE(kr[0].dalej, "ekran najwyżej 2 razy w tygodniu, obserwacja trwa");
    ROWNE(kr[1].dalej, "wraca do wyboru 10.11, obserwacja trwa");
  }
  std::vector<SprawaL> s5 = {b14::sprawaE("E-21")};
  ws.sprawy = &s5;
  ws.ekranDnia = &s5[0];
  SPRAWDZ(sprawy::kartySpraw(ws).empty());
}

// ---------- limity tekstów 5.4.4 i szerokości w kroju ekranu ----------
static void testLimityTekstow() {
  for (int dl = 0; dl < 2; dl++)
    for (const char* kl : E_EKRANY)
      for (reguly::Tryb t : {reguly::Tryb::ASVAUTO, reguly::Tryb::CPAP}) {
        SprawaL s = b14::sprawaE(kl, dl == 1, t);
        bool tyt = znaki(s.tytul) <= 30, pow = znaki(s.powod) <= 70, krok = znaki(s.krok) <= 60;
        bool sz = zawin(F_SB44, s.tytul, szerTytuluWzorca(false)).size() == 1 &&
                  zawin(F_P28B, s.powod, szerTresciWzorca(false)).size() <= 2 && szer(F_P28B, s.krok) <= 744;
        bool pelne = s.tytul.find('{') == std::string::npos && s.powod.find('{') == std::string::npos &&
                     s.krok.find('{') == std::string::npos;  // każda zmienna wypełniona
        SPRAWDZ(tyt && pow && krok && sz && pelne);
        if (!(tyt && pow && krok && sz && pelne))
          printf("    %s: „%s” / „%s” / „%s”\n", kl, s.tytul.c_str(), s.powod.c_str(), s.krok.c_str());
      }
  // `Wciąż: {tytuł}.` mieści się w limicie linii uwagi (60 znaków, 5.4.4) dla każdego E
  for (const char* kl : E_EKRANY) SPRAWDZ(znaki("Wciąż: " + b14::sprawaE(kl, true).tytul + ".") <= 60);
}

// ---------- treść i rysowanie S3 ----------
struct Bufor : Plotno {
  std::vector<uint8_t> px = std::vector<uint8_t>(800 * 480, 0);
  void piksel(int x, int y, bool c) override {
    if (x >= 0 && x < 800 && y >= 0 && y < 480) px[(size_t)(y * 800 + x)] = c;
  }
  bool cz(int x, int y) const { return px[(size_t)(y * 800 + x)] != 0; }
};
static std::vector<Noc> noce(const std::string& dzis, int n) {
  std::vector<Noc> h;
  for (int i = n - 1; i >= 0; i--) {
    Noc x;
    x.data = dodajDni(dzis, -i);
    x.wazna = true;
    x.m01 = 420;
    x.ahi = 1.5f;
    x.wp95 = i == 0 ? 38.0f : 10.0f;
    x.ustawienia = "ASVAuto/6/10/3/15";
    h.push_back(x);
  }
  return h;
}
static void testTrescIRysowanie() {
  std::vector<Noc> h = noce("2026-11-04", 40);
  DaneDnia d;
  d.dzis = "2026-11-04";
  d.hist = &h;
  SprawaL s = b14::sprawaE("E-23");
  DaneWzorca w = trescWzorca(s, d);  // punkt wejścia z src/dzien.cpp
  SPRAWDZ(!w.lekarz && w.etykieta.empty() && w.wiz == WizWzorca::LINIA);
  SPRAWDZ(w.wartosci.size() == (size_t)zdarzenia::NOCY_WYKRESU);
  SPRAWDZ(!w.wartosci.empty() && std::fabs(w.wartosci.back() - 38.0f) < 1e-3f && std::fabs(w.prog - 27.0f) < 1e-3f);
  ROWNE(w.tytul, "Wyciek od 27 l/min w nocy");  // przegląd kliniczny
  std::vector<std::string> st = stopkaWzorca(false);
  // mapa 5.6.2: problem poranny; stopka podpisuje każdą akcję, także ▶ kokpit (K1)
  SPRAWDZ(st.size() == 2 && st[0] == "▶ kokpit" && st[1] == "↻ 2 s wiem");
  Bufor b;
  rysujWzorzec(b, w);
  SPRAWDZ(b.cz(5, 5) && b.cz(400, 470) && b.cz(400, 75));  // negatyw (5.4.3 ton „problem”), bez linii brak paska
  // z linią zdrowia: biały pasek pod nagłówkiem (54…102), tytuł niżej
  d.linia = "Wciąż: Noce z AHI powyżej 5.";
  d.liniaOstrzezenie = true;
  w = trescWzorca(s, d);
  ROWNE(w.linia, d.linia);
  Bufor b2;
  rysujWzorzec(b2, w);
  SPRAWDZ(!b2.cz(790, 60) && !b2.cz(790, 98) && b2.cz(790, 106));
  // linia informacyjna: biała ramka, wnętrze czarne
  d.liniaOstrzezenie = false;
  w = trescWzorca(s, d);
  Bufor b3;
  rysujWzorzec(b3, w);
  SPRAWDZ(!b3.cz(12, 80) && b3.cz(700, 80));
  // E-21 (stan odchylenia): linia przerywana = mediana normy, gdy norma jest gotowa
  Stany sty;
  sty.norma.wp95.n = 30;
  sty.norma.wp95.med = 9.5;
  d.stany = &sty;
  w = trescWzorca(b14::sprawaE("E-21"), d);
  SPRAWDZ(std::fabs(w.prog - 9.5f) < 1e-3f);
  // bez historii: bez wykresu, ekran nadal z tekstami
  d.hist = nullptr;
  w = trescWzorca(s, d);
  SPRAWDZ(w.wiz == WizWzorca::BRAK && !w.tytul.empty());
}

// R-KON-41, R-EKR-60, R-KON-09: koniec slotu dziennego i przerysowań = początek okna slotu wieczornego
static WejsciePlanu planDnia(int wieczorOd) {
  WejsciePlanu w;
  w.zegar = true;
  w.nocKompletna = true;
  w.sprawdzenie11 = true;
  w.wieczorOd = wieczorOd;
  return w;
}
static void testKoniecSlotuDziennego() {
  SPRAWDZ(koniecSlotuDziennego(21 * 60) == 21 * 60);
  SPRAWDZ(koniecSlotuDziennego(19 * 60) == 19 * 60);
  SPRAWDZ(koniecSlotuDziennego(21 * 60 + 10) == 21 * 60 + 10);  // przykład R-KON-41: slot wieczorny od 21:10
  SPRAWDZ(koniecSlotuDziennego(-1) == 23 * 60 + 30);            // okno puste: do 23:30
  // slot dzienny od 07:55 (import 06:55), okno od 19:00: oceny o pełnych godzinach do 18:00, bez 19:00 i 20:00
  WejsciePlanu w = planDnia(19 * 60);
  w.importNocy = 6 * 3600 + 55 * 60;
  w.slotDziennyOd = 7 * 3600 + 55 * 60;
  auto powod = [&](int m) {
    w.sekundaDoby = m * 60;
    w.obsluzoneZnane = true;
    w.obsluzoneDo = m * 60 - 1;
    return powodyTeraz(w);
  };
  SPRAWDZ((powod(18 * 60) & POWOD_SLOT_DZIENNY_GODZINA) != 0);
  SPRAWDZ((powod(19 * 60) & POWOD_SLOT_DZIENNY_GODZINA) == 0);
  SPRAWDZ((powod(20 * 60) & POWOD_SLOT_DZIENNY_GODZINA) == 0);
  // okno od 21:10: ocena o 21:00 jeszcze w slocie
  w.wieczorOd = 21 * 60 + 10;
  SPRAWDZ((powod(21 * 60) & POWOD_SLOT_DZIENNY_GODZINA) != 0);
  // import o 18:30, okno od 19:00: start 19:30 wypada po końcu slotu → brak slotu dziennego
  WejsciePlanu p = planDnia(19 * 60);
  p.importNocy = 18 * 3600 + 30 * 60;
  SPRAWDZ(poczatekSlotuDziennego(p) < 0);
  p.wieczorOd = 21 * 60;
  SPRAWDZ(poczatekSlotuDziennego(p) == 19 * 3600 + 30 * 60);
  // R-EKR-60 poza slotem dziennym (przypadek zastępczy z przykładu: bez slotu), ekran dnia narysowany o 11:00:
  // okno od 21:00 → 13:00, 15:00, 17:00, 19:00; okno od 19:00 → bez 19:00; okno puste → do 23:00 włącznie
  auto odswiezenia = [](int wieczorOd) {
    std::vector<int> r;
    WejsciePlanu q = planDnia(wieczorOd);
    q.slotDzienny = false;
    q.dzienRysowany = 11 * 3600;
    for (int i = 0; i < 8; i++) {
      q.sekundaDoby = q.dzienRysowany;
      Termin t = nastepnyTermin(q);
      int m = (q.sekundaDoby + (int)t.sekund) / 60;
      if (!(t.powody & POWOD_ODSWIEZENIE)) {
        if (m >= 24 * 60 || t.sekund == 0) break;
        q.sekundaDoby = m * 60;
        q.obsluzoneZnane = true;
        q.obsluzoneDo = m * 60;
        continue;
      }
      r.push_back(m);
      q.dzienRysowany = m * 60;
      q.obsluzoneZnane = true;
      q.obsluzoneDo = m * 60;
    }
    return r;
  };
  std::vector<int> o21 = odswiezenia(21 * 60), o19 = odswiezenia(19 * 60), oPuste = odswiezenia(-1);
  SPRAWDZ(o21 == std::vector<int>({13 * 60, 15 * 60, 17 * 60, 19 * 60}));
  SPRAWDZ(o19 == std::vector<int>({13 * 60, 15 * 60, 17 * 60}));
  SPRAWDZ(!oPuste.empty() && oPuste.back() == 23 * 60);
}

// krój P24 600 (kanwa X-otwarte-sprawy-*: nazwa wiersza 700, wartość 600)
static void testKrojP24S() {
  const std::string w = "wraca do wyboru 07.11, obserwacja trwa";
  SPRAWDZ(F_P24S != F_P24B && szer(F_P24S, w) > 0 && szer(F_P24S, w) <= szer(F_P24B, w));
}

// przegląd spełnione zdarzenie R5b (E-23) blokuje uspokojenie (5.4.13 pkt 2), R5c (E-21) nie
static void testUspokojenieR5b() {
  kolejka::OcenaUspokojenia o = kolejka::ocenUspokojenie({b14::wynikE("E-23", {})}, {});
  SPRAWDZ(!o.pkt2);
  o = kolejka::ocenUspokojenie({b14::wynikE("E-21", b14::zmienneE(false)["E-21"])}, {});
  SPRAWDZ(o.pkt2);
}

int main() {
  testSprawyE();
  testWykluczenia();
  testKolejnosc();
  testRytm();
  testLinie();
  testLimityTekstow();
  testTrescIRysowanie();
  testKoniecSlotuDziennego();
  testKrojP24S();
  testUspokojenieR5b();
  printf("testy_ekrany_zdarzen_e: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
