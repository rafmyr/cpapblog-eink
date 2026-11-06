// SPDX-License-Identifier: GPL-3.0-or-later
// Testy przypadków brzegowych (raport A4 dla lekarza, serwer raportu, kod QR). Każdy test pada z numerem
// R13-nn, dopóki usterka nie zostanie poprawiona.
// Dane syntetyczne. Drugi argument: katalog na kody QR do odczytu dekoderem (OpenCV w testy.sh).
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "qr.h"
#include "raport.h"
#include "serwer_raportu.h"
#include "sprawy.h"
#include "wspolne.h"
#include "zestaw_kolejki.h"

static int ok = 0, zle = 0;
#define PRZYPADEK(nr, w)                                                        \
  do {                                                                           \
    if (w) ok++;                                                                 \
    else { zle++; printf("BŁĄD %s (%s:%d): %s\n", nr, __FILE__, __LINE__, #w); } \
  } while (0)

static const std::string DZIS = "2026-11-04";

static Noc noc(const std::string& d, const std::string& tryb) {
  Noc n;
  n.data = d;
  n.wazna = true;
  n.m01 = 420;
  n.ahi = 2.0f;
  n.ca = 0.5f;
  n.oa = 0.6f;
  n.h = 0.8f;
  n.ua = 0.1f;
  n.cmed = 9.8f;
  n.cp95 = 12.4f;
  n.emed = 7.2f;
  n.ep95 = 8.4f;
  n.psmed = 5;
  n.psp95 = 9;
  n.wp95 = 12;
  n.m17 = 1;
  n.m08 = 0;
  n.tryb = tryb;
  n.pna = "AirSense_10_AutoSet";
  n.eai = 6, n.eax = 10, n.ans = 3, n.axs = 15, n.eep = 7, n.mns = 3, n.mxs = 12, n.mpi = 6, n.mpa = 14, n.ipc = 9;
  n.epr = 2;
  return n;
}
static std::vector<Noc> historia(int ile, const std::string& tryb = "AutoSet", const std::string& koniec = DZIS) {
  std::vector<Noc> h;
  for (int i = 0; i < ile; i++) h.push_back(noc(dodajDni(koniec, -(ile - 1 - i)), tryb));
  return h;
}
static raport::Raport zbuduj(const std::vector<Noc>& h, const sprawy::SprawaL* t,
                             const std::vector<sprawy::SprawaL>* sp = nullptr, std::set<std::string> niew = {}) {
  raport::Wejscie we;
  we.dzis = DZIS;
  we.czas = "06:04";
  we.historia = &h;
  we.temat = t;
  we.sprawy = sp;
  we.niewalidowane = niew;
  return raport::zbuduj(we);
}
static double num(const std::string& s) {  // "2,0" → 2.0
  std::string t = s;
  for (auto& c : t)
    if (c == ',') c = '.';
  return std::atof(t.c_str());
}
static size_t ile(const std::string& s, const std::string& f) {
  size_t n = 0;
  for (size_t p = s.find(f); p != std::string::npos; p = s.find(f, p + 1)) n++;
  return n;
}

// L-08 liczy noce z terapią, także krótsze niż 4 h (R-SKI-27, wyjątek R-LEK-17), a tabelka strefy 4 i
// trzy liczby ściągi liczą godziny z maską tylko z nocy ważnych (≥ 4 h). Pacjent śpi 2 h od 10 nocy, raport pokazuje 7 h.
static void l08NoceKrotkieAGodzinyZNocyWaznych() {
  std::vector<Noc> h = historia(90);
  for (int i = 80; i < 90; i++) {
    h[(size_t)i].m01 = 120;
    h[(size_t)i].wazna = false;
  }
  sprawy::SprawaL s;
  s.klucz = s.id = "L-08";
  s.pilnosc = reguly::Pilnosc::B;
  s.tytul = "Regularność niższa";
  s.powod = "Krótkie noce od 10 dni.";
  s.aktywnaOd = dodajDni(DZIS, -9);
  raport::Raport r = zbuduj(h, &s);
  PRZYPADEK("L-08 liczy noce z terapią, także krótsze niż 4 h", r.s4tab.wiersze.size() > 0 && std::fabs(num(r.s4tab.wiersze[0][3]) - 2.0) < 0.05);  // ostatnie 7
  PRZYPADEK("L-08 liczy noce z terapią, także krótsze niż 4 h", r.s4tab.wiersze.size() > 0 && std::fabs(num(r.s4tab.wiersze[0][2]) - 2.0) < 0.05);  // od początku
  PRZYPADEK("L-08 liczy noce z terapią, także krótsze niż 4 h", std::fabs(num(r.sciagaLiczby[2]) - 2.0) < 0.05);
}

// raport ogólny, kolumna „90 nocy” tabelki strefy 4 bierze 90 ostatnich nocy ważnych z całej historii,
// także sprzed okresu raportu (nagłówek: 90 nocy). Tu 60 nocy ważnych w okresie (AHI 2) i stare noce z AHI 20.
static void kolumna90NocyZCalejHistorii() {
  std::vector<Noc> h = historia(150);
  for (int i = 0; i < 60; i++) h[(size_t)i].ahi = 20;
  for (int i = 60; i < 150; i++)
    if (i % 3 == 0) h[(size_t)i].wazna = false, h[(size_t)i].m01 = 100;
  raport::Raport r = zbuduj(h, nullptr);
  PRZYPADEK("raport ogólny, kolumna „90 nocy” tabelki strefy 4 bierze 90", r.s4tab.kolumny.size() == 4 && r.s4tab.kolumny[1] == "90 nocy");
  PRZYPADEK("raport ogólny, kolumna „90 nocy” tabelki strefy 4 bierze 90", std::fabs(num(r.s4tab.wiersze[0][1]) - 2.0) < 0.05);
  // ta sama sprawa w kolumnie „30 nocy”: 30 nocy ważnych sięga 45 dni wstecz, strefa 7 liczy 30 dni kalendarzowych
  for (int i = 0; i < 150; i++) h[(size_t)i].ahi = i >= 120 ? 2.0f : 9.0f;
  r = zbuduj(h, nullptr);
  PRZYPADEK("raport ogólny, kolumna „90 nocy” tabelki strefy 4 bierze 90", std::fabs(num(r.s4tab.wiersze[0][2]) - 2.0) < 0.05);
}

// R-LEK-19 i fail-path R-LEK-35: przy CA/h i AHI na liście niewalidowanych udział `centralne {p}% AHI`
// zostaje liczbą (tylko trend, bez liczb i bez oznaczenia).
static void udzialCentralnychPrzyNiewalidowanych() {
  std::vector<Noc> h = historia(90);
  raport::Raport r = zbuduj(h, nullptr, nullptr, {"M-02", "M-03ca"});
  PRZYPADEK("R-LEK-19 i fail-path R-LEK-35", r.s5zdania.size() > 0 && r.s5zdania[0].find("% AHI") == std::string::npos);
}

// R-LEK-19 „wykresy bez opisu wartości osi”: wykres CA/h i AHI w PDF ma liczby osi i linii progu.
static void osieWykresowPdf() {
  std::vector<Noc> h = historia(90);
  sprawy::SprawaL s;
  s.klucz = s.id = "L-01";
  s.pilnosc = reguly::Pilnosc::B;
  s.tytul = "Bezdechy centralne utrwalone";
  s.powod = "Centralne.";
  s.aktywnaOd = dodajDni(DZIS, -5);
  std::string b;
  std::string p = raport::pdf(zbuduj(h, &s, nullptr, {"M-02", "M-03ca"}), b);
  PRZYPADEK("R-LEK-19 „wykresy bez opisu wartości osi”", b.empty() && ile(p, "(5) Tj") == 0 && ile(p, "(10) Tj") == 0);
}

// fail-path R-HIP-07: wzorzec bez hipotezy przy włączonej podpowiedzi = pytanie ogólne, bez strefy 3b.
// Raport L-01 w CPAP bez zmian ustawień (R-HIP-01: „bez ramki”) drukuje strefę 3b z `Przyczynę oceni lekarz.` (H-08).
static void wzorzecBezHipotezyBezStrefy3b() {
  std::vector<Noc> h = historia(90, "CPAP");
  sprawy::SprawaL s;
  s.klucz = s.id = "L-01";
  s.pilnosc = reguly::Pilnosc::B;
  s.tytul = "Bezdechy centralne utrwalone";
  s.powod = "Centralne.";
  s.aktywnaOd = dodajDni(DZIS, -5);
  raport::Raport r = zbuduj(h, &s);
  PRZYPADEK("fail-path R-HIP-07", !r.s3bJest);
}

// R-HIP-10: okno 56 nocy po zmianie EPAP/PS; zapas w raporcie (bez wyniku `.po_zmianie`) liczy 30.
// R-HIP-10 w wydaniu 1.0 (L-01 „tylko raport”): zdanie w strefie 2 zostaje; raport nie ma go poza
// raportem tematycznym L-01, którego w 1.0 nie ma.
static void oknoPoZmianieIZdanieStrefy2() {
  std::vector<Noc> h = historia(90, "ASVAuto");
  for (int i = 0; i < 45; i++) h[(size_t)i].eax = 9;  // zmiana EAX 9 → 10 w nocy 45
  std::string zmiana = h[45].data;
  sprawy::SprawaL s;
  s.klucz = s.id = "L-01";
  s.pilnosc = reguly::Pilnosc::B;
  s.tytul = "Bezdechy centralne utrwalone";
  s.powod = "Centralne.";
  s.aktywnaOd = dodajDni(zmiana, 40);
  raport::Raport r = zbuduj(h, &s);
  bool jest = false;
  for (auto& z : r.s2) jest = jest || z.find("Początek wzorca 40 nocy po zmianie ustawień") == 0;
  PRZYPADEK("R-HIP-10", jest);
  s.aktywnaOd = dodajDni(zmiana, 20);
  s.stan = reguly::StanReguly::TYLKO_RAPORT;
  std::vector<sprawy::SprawaL> sp = {s};
  r = zbuduj(h, nullptr, &sp);
  jest = false;
  for (auto& z : r.s2) jest = jest || z.find("Początek wzorca 20 nocy po zmianie ustawień") == 0;
  PRZYPADEK("R-HIP-10 w wydaniu 1.0", jest);
}

// R-HIP-10 we wszystkich trybach obsługiwanych (przegląd kliniczny;
// T-TEST-115 przypadek 15): w CPAP i AutoSet zmiana EPR, L-01 w 30. nocy → zdanie z datą zmiany;
// bez zmiany ustawień → bez zdania; wynik H-08/L-01~cpap niespełniony → bez zdania.
static void rsmHip10() {
  for (const char* tryb : {"CPAP", "AutoSet"}) {
    std::vector<Noc> h = historia(90, tryb);
    for (int i = 45; i < 90; i++) h[(size_t)i].epr = 3;  // zmiana EPR 2 → 3 w nocy 45
    std::string zmiana = h[45].data;
    sprawy::SprawaL s;
    s.klucz = s.id = "L-01";
    s.pilnosc = reguly::Pilnosc::B;
    s.tytul = "Bezdechy centralne utrwalone";
    s.powod = "Centralne.";
    s.aktywnaOd = dodajDni(zmiana, 30);
    std::string oczek = "Początek wzorca 30 nocy po zmianie ustawień " + zmiana.substr(8, 2) + "." +
                        zmiana.substr(5, 2) + ".";
    raport::Raport r = zbuduj(h, &s);
    bool jest = false;
    for (auto& z : r.s2) jest = jest || z == oczek;
    PRZYPADEK("R-HIP-10 we wszystkich trybach obsługiwanych", jest);
    // wynik H-08/L-01~cpap z oceny reguł ma pierwszeństwo przed zapasem z historii
    std::vector<reguly::Wynik> wy(1);
    wy[0].id = "H-08";
    wy[0].wariant = "L-01~cpap";
    wy[0].oceniona = true;
    wy[0].spelniona = false;
    raport::Wejscie we;
    we.dzis = DZIS;
    we.czas = "06:04";
    we.historia = &h;
    we.temat = &s;
    we.wyniki = &wy;
    r = raport::zbuduj(we);
    jest = false;
    for (auto& z : r.s2) jest = jest || z.find("Początek wzorca") == 0;
    PRZYPADEK("R-HIP-10 we wszystkich trybach obsługiwanych", !jest);
    wy[0].spelniona = true;
    r = raport::zbuduj(we);
    jest = false;
    for (auto& z : r.s2) jest = jest || z == oczek;
    PRZYPADEK("R-HIP-10 we wszystkich trybach obsługiwanych", jest);
    // bez zmiany ustawień: bez zdania
    std::vector<Noc> b = historia(90, tryb);
    r = zbuduj(b, &s);
    jest = false;
    for (auto& z : r.s2) jest = jest || z.find("Początek wzorca") == 0;
    PRZYPADEK("R-HIP-10 we wszystkich trybach obsługiwanych", !jest);
  }
}

// telefon sam pyta o `/favicon.ico` (i iOS o `/apple-touch-icon*.png`) bez tokenu; każde takie żądanie
// liczy się do 5 złych (R-LEK-03b), więc 2–3 otwarcia strony wyłączają serwer w trakcie pobierania.
static void faviconBezTokenuNieWylaczaSerwera() {
  serwer::Sesja s;
  s.otworz("t0k", 0, "192.168.1.47", serwer::Tryb::PDF, "/raporty/2026-11-04-L-01.pdf", {}, DZIS, "T", "", false);
  for (int i = 0; i < 3; i++) {
    s.obsluz("/r/t0k/", 1000);
    s.obsluz("/favicon.ico", 1000);
    s.obsluz("/apple-touch-icon.png", 1000);
  }
  PRZYPADEK("telefon sam pyta o `/favicon.ico`", s.aktywna(2000));
  PRZYPADEK("telefon sam pyta o `/favicon.ico`", s.obsluz("/r/t0k/", 1000).cialo.find("rel=\"icon\"") != std::string::npos);
}

// okres w nagłówku kończy się na ostatniej nocy, a liczba nocy liczy dni do dziś (3 dni bez danych).
static void okresWNaglowkuDoDzis() {
  std::vector<Noc> h = historia(90, "AutoSet", dodajDni(DZIS, -3));
  raport::Raport r = zbuduj(h, nullptr);
  PRZYPADEK("okres w nagłówku kończy się na ostatniej nocy, a liczba", r.nocy == dniMiedzy(r.od, r.do_) + 1 || r.do_ == DZIS);
}

// R-LEK-35: `centralne {p}% AHI` = mediana CA/h do mediany AHI „z tych nocy”; przy CA/h bez wartości
// w ostatnich nocach mediany liczą się z różnych okien nocy.
static void udzialCentralnychZTychSamychNocy() {
  std::vector<Noc> h = historia(90);
  for (int i = 0; i < 90; i++) h[(size_t)i].ahi = i >= 76 ? 10.0f : 4.0f, h[(size_t)i].ca = i >= 76 ? 5.0f : 2.0f;
  for (int i = 83; i < 90; i++) h[(size_t)i].ca = NAN;  // 7 nocy z importu karty
  raport::Raport r = zbuduj(h, nullptr);
  // wspólne noce z CA/h i AHI: 14 ostatnich z CA = noce 69…82: CA 2 (7) i 5 (7), AHI 4 (7) i 10 (7) → 3,5/7 = 50%
  PRZYPADEK("R-LEK-35", r.s5zdania.size() > 0 && r.s5zdania[0].find("centralne 50% AHI") == 0);
}

// wiersz `Starsze raporty widoczne do {hh:mm}.` (R-LEK-33) ginie po pobraniu PDF, bo dyrygent trzyma
// jedno pole stanu (`rap.stan`). Test w rdzeniu nie sięga dyrygenta: sprawdzenie przez kod, tu tylko znacznik.

// QR: kody do odczytu dekoderem (testy.sh, OpenCV): adresy z różnymi IP i tokenami, wersje 3–5
static void kodyQr(const std::string& kat) {
  const char* adresy[] = {"http://192.168.1.47/r/f1b384krart14y2p6g97gnhm28/",
                          "http://10.0.0.2/r/0000000000000000000000000z/edf",
                          "http://192.168.100.200/r/zzzzzzzzzzzzzzzzzzzzzzzzzz/",
                          "http://172.16.254.254/r/abcdefghjkmnpqrstvwxyz0123/edf"};
  int i = 0;
  for (const char* a : adresy) {
    qr::Kod k = qr::koduj(a);
    PRZYPADEK("QR", k.rozmiar > 0);
    std::ofstream f(kat + "/qr-" + std::to_string(i++) + ".pbm");
    int m = 6, b = qr::STREFA_CISZY, n = (k.rozmiar + 2 * b) * m;
    f << "P1\n" << n << " " << n << "\n";
    for (int y = 0; y < n; y++) {
      for (int x = 0; x < n; x++) f << (k.ciemny(x / m - b, y / m - b) ? '1' : '0');
      f << "\n";
    }
    std::ofstream(kat + "/qr-" + std::to_string(i - 1) + ".txt") << a;
  }
}

int main(int argc, char** argv) {
  l08NoceKrotkieAGodzinyZNocyWaznych();
  kolumna90NocyZCalejHistorii();
  udzialCentralnychPrzyNiewalidowanych();
  osieWykresowPdf();
  wzorzecBezHipotezyBezStrefy3b();
  oknoPoZmianieIZdanieStrefy2();
  rsmHip10();
  faviconBezTokenuNieWylaczaSerwera();
  okresWNaglowkuDoDzis();
  udzialCentralnychZTychSamychNocy();
  kodyQr(argc > 1 ? argv[1] : "/tmp");
  printf("testy_raport_a4_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
