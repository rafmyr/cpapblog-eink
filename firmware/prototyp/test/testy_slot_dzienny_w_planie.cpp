// SPDX-License-Identifier: GPL-3.0-or-later
// Testy slotu dziennego w planie wybudzeń i w dyrygencie (wersja pełna).
// Pisane ze spec funkcjonalnej (R-KON-41, R-KON-42, R-KON-52, R-EKR-60,
// R-NF-02, R-NF-03, 5.8.12 i tabele przypadków granicznych) i technicznej (T-WYM-20, T-WYM-28, T-KOMP-24, 7.1), bez
// projektowania pod implementację. Slot dzienny ma w src/plan.h
// (bity POWOD_SLOT_DZIENNY_START, POWOD_SLOT_DZIENNY_GODZINA, POWOD_PROGNOZA_GODZINOWA, pola WejsciePlanu,
// poczatekSlotuDziennego()) deklaracje, które src/plan.cpp i dyrygent obsługują.
//
// Ustalenia, według których testy sprawdzają dyrygenta: przy powodzie slotu dziennego dyrygent robi
// to samo co przy przerysowaniu R-EKR-60 (ekran dnia, odświeżenie e-ink tylko przy zmianie klatki); koniec slotu
// dziennego = początek okna slotu wieczornego w wariancie kroku 2 (21:00, PLAN_POGODA_WIECZOR); wybudzenie o pełnej
// godzinie slotu ma dokładnie jedno żądanie `GET /api/status`, bez raportu nocy i bez pogody, Wi-Fi wyłączone przed
// rysowaniem.
//
// (a) Plan wybudzenia (src/plan.h, T-KOMP-24): terminy i powody slotu dziennego.
// (b) Dyrygent z atrapami (test/atrapy/adaptery_atrapy.h) i pomiarem jak w test/testy_plan_wybudzen.cpp.
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_slot_dzienny_w_planie, na końcu).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <set>
#include <string>
#include <vector>
#include "adaptery_atrapy.h"
#include "airbridge.h"
#include "dyrygent.h"
#include "nawigacja.h"
#include "plan.h"
#include "start.h"
#include "wspolne.h"
#include "wybudzenie.h"

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
#define ROWNE_L(a, b)                                                                                    \
  do {                                                                                                   \
    long long _a = (long long)(a), _b = (long long)(b);                                                  \
    if (_a == _b) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); }  \
  } while (0)

static const char* STREFA_PANELU = "CET-1CEST,M3.5.0,M10.5.0/3";  // jak test/testy_plan_wybudzen.cpp

static uint32_t ep(int r, int mi, int d, int h, int m, int s = 0) {
  std::tm t{};
  t.tm_year = r - 1900;
  t.tm_mon = mi - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  t.tm_isdst = -1;
  return (uint32_t)mktime(&t);
}
static int sd(int h, int m, int s = 0) { return h * 3600 + m * 60 + s; }

static const Powody SLOT_BITY = POWOD_SLOT_DZIENNY_START | POWOD_SLOT_DZIENNY_GODZINA;

// Stan planu z zegarem o hh:mm:ss.
static WejsciePlanu wp(int h, int m, int s = 5) {
  WejsciePlanu w;
  w.zegar = true;
  w.sekundaDoby = sd(h, m, s);
  return w;
}

// Noc kompletna dziś: koniec terapii (koniecH < 0 = nieznany) i import nocy; teraz = hh:mm:ss. Sprawdzenie o 11:00
// uznane za wykonane, gdy teraz jest po 11:00 (wybudzenie o 11:00 i tak było).
static WejsciePlanu poImporcie(int h, int m, int s, int koniecH, int koniecM, int importH, int importM) {
  WejsciePlanu w = wp(h, m, s);
  w.nocKompletna = true;
  w.sprawdzenie11 = w.sekundaDoby > sd(11, 0, 59);
  w.koniecTerapii = koniecH < 0 ? -1 : sd(koniecH, koniecM);
  w.importNocy = sd(importH, importM);
  return w;
}

// Minuta doby, na którą wypada termin (teraz + sen, po zapasie 5 s lub 2 s); po północy minuta + 1440.
static int minutaTerminu(const WejsciePlanu& w, const Termin& t) {
  long k = (long)w.sekundaDoby + (long)t.sekund;
  return (int)(k / 60);
}

struct TerminP02 {
  int minuta;
  Powody powody;
};
// Przejście planem od chwili w.sekundaDoby do północy. Stan doby zmienia się jak po wykonaniu wybudzenia: sprawdzenie
// o 11:00 zrobione, start slotu dziennego zapisany w slotDziennyOd (R-KON-41: najwyżej raz na dobę), prognoza
// godzinowa pobrana udanie przy powodzie R-KON-52 (gdy `prognozaUdana`).
static std::vector<TerminP02> planDoPolnocy(WejsciePlanu w, bool prognozaUdana = true) {
  std::vector<TerminP02> v;
  for (int i = 0; i < 200; i++) {
    Termin t = nastepnyTermin(w);
    if (t.sekund == 0) break;
    long k = (long)w.sekundaDoby + (long)t.sekund;
    if (k >= 86400) break;
    v.push_back({(int)(k / 60), t.powody});
    w.sekundaDoby = (int)k;
    w.obsluzoneZnane = true;
    w.obsluzoneDo = (int)k;
    if (t.powody & POWOD_SPRAWDZENIE_11) w.sprawdzenie11 = true;
    if ((t.powody & (POWOD_SLOT_DZIENNY_START | POWOD_SLOT_DZIENNY_BEZ_SIECI)) && w.slotDziennyOd < 0)
      w.slotDziennyOd = (int)k;
    if ((t.powody & POWOD_PROGNOZA_GODZINOWA) && prognozaUdana) w.prognozaGodzinowa = (int)k;
    if (t.powody & POWOD_POWROT) w.ekran = Ekran::DZIEN;
  }
  return v;
}
static std::vector<int> minutyZBitem(const std::vector<TerminP02>& v, Powody bit) {
  std::vector<int> m;
  for (auto& t : v)
    if (t.powody & bit) m.push_back(t.minuta);
  return m;
}
static std::vector<int> godziny(int od, int doH) {
  std::vector<int> m;
  for (int h = od; h <= doH; h++) m.push_back(h * 60);
  return m;
}

// ============================================================================================================
// (a) Plan wybudzenia
// ============================================================================================================

// R-KON-41, kolumna „Weryfikacja” i 5.8.12: start slotu dziennego 60 min po późniejszej z chwil koniec terapii, import
// nocy. Koniec terapii 06:40, import 06:55 → 07:55 z powodem R-KON-41; import 11:30 po S10 → 12:30; bez końca terapii
// → import + 60 min; slot nie startuje drugi raz tej doby.
static void testR_KON_41_startSlotuDziennego() {
  // koniec terapii 06:40, import 06:55 (wybudzenie, które zapisało noc, kończy się o 06:55:10)
  WejsciePlanu a = poImporcie(6, 55, 10, 6, 40, 6, 55);
  ROWNE_L(poczatekSlotuDziennego(a), sd(7, 55));
  Termin ta = nastepnyTermin(a);
  ROWNE_L(minutaTerminu(a, ta), 7 * 60 + 55);
  SPRAWDZ(ta.powody & POWOD_SLOT_DZIENNY_START);
  SPRAWDZ(sieciowy(ta.powody));
  ROWNE(powodWpisuBaterii(PowodWybudzenia::ZEGAR, ta.powody & POWOD_SLOT_DZIENNY_START), "zegar:slot_dzienny");
  // import 11:30 po S10 (noc w budowie do 11:30): start 12:30, bez ponowień po imporcie
  WejsciePlanu b = poImporcie(11, 30, 10, 6, 40, 11, 30);
  ROWNE_L(poczatekSlotuDziennego(b), sd(12, 30));
  Termin tb = nastepnyTermin(b);
  ROWNE_L(minutaTerminu(b, tb), 12 * 60 + 30);
  SPRAWDZ(tb.powody & POWOD_SLOT_DZIENNY_START);
  SPRAWDZ(!(tb.powody & POWOD_PONOWIENIE));
  // brak końca terapii w raporcie : import 07:00 → start 08:00 (przykład R-NF-02)
  WejsciePlanu c = poImporcie(7, 0, 10, -1, 0, 7, 0);
  ROWNE_L(poczatekSlotuDziennego(c), sd(8, 0));
  Termin tc = nastepnyTermin(c);
  ROWNE_L(minutaTerminu(c, tc), 8 * 60);
  SPRAWDZ(tc.powody & POWOD_SLOT_DZIENNY_START);
  // jeden start na dobę: przejście planem od 06:55:10 do północy → POWOD_SLOT_DZIENNY_START tylko o 07:55
  std::vector<TerminP02> v = planDoPolnocy(a);
  SPRAWDZ(minutyZBitem(v, POWOD_SLOT_DZIENNY_START) == std::vector<int>{7 * 60 + 55});
  // slot już wystartował o 07:55 (slotDziennyOd), noc zapisana jeszcze raz o 12:00 → bez drugiego startu
  WejsciePlanu d = poImporcie(12, 0, 10, 6, 40, 12, 0);
  d.slotDziennyOd = sd(7, 55, 5);
  ROWNE_L(poczatekSlotuDziennego(d), sd(7, 55, 5));
  SPRAWDZ(minutyZBitem(planDoPolnocy(d), POWOD_SLOT_DZIENNY_START).empty());
  // bez importu nocy dziś: bez slotu (plan bez slotu dziennego)
  WejsciePlanu e = wp(7, 0, 10);
  e.nocKompletna = true;
  ROWNE_L(poczatekSlotuDziennego(e), -1);
}

// R-KON-41, 5.8.12: naciśnięcie dowolnego przycisku na ekranie dnia slotu porannego po imporcie → slot dzienny od tej
// chwili, bez czekania 60 min (weryfikacja R-KON-41: ▶ o 07:10 → K1, po powrocie z K1 ekran slotu dziennego).
static void testR_KON_41_startPrzyciskiem() {
  // ▶ o 07:10 po imporcie 06:55: start slotu o 07:10
  WejsciePlanu a = poImporcie(7, 10, 0, 6, 40, 6, 55);
  a.nacisniecieRano = sd(7, 10);
  ROWNE_L(poczatekSlotuDziennego(a), sd(7, 10));
  // K1 otwarty o 07:10 → najpierw powrót o 07:15 (R-PRZ-02), bez osobnego startu o 07:55
  a.ekran = Ekran::K1;
  a.sekundOdNacisniecia = 0;
  Termin ta = nastepnyTermin(a);
  ROWNE_L(ta.powody, POWOD_POWROT);
  // dyrygent zapisał start przy naciśnięciu (slotDziennyOd); po powrocie o 07:15:02 następna jest ocena o 08:00
  WejsciePlanu b = poImporcie(7, 15, 2, 6, 40, 6, 55);
  b.nacisniecieRano = sd(7, 10);
  b.slotDziennyOd = sd(7, 10);
  Termin tb = nastepnyTermin(b);
  ROWNE_L(minutaTerminu(b, tb), 8 * 60);
  SPRAWDZ(tb.powody & POWOD_SLOT_DZIENNY_GODZINA);
  SPRAWDZ(!(tb.powody & POWOD_SLOT_DZIENNY_START));
  // samo naciśnięcie w stanie planu (slotDziennyOd jeszcze -1): też bez startu o 07:55, ocena o 08:00
  b.slotDziennyOd = -1;
  std::vector<TerminP02> v = planDoPolnocy(b);
  SPRAWDZ(minutyZBitem(v, POWOD_SLOT_DZIENNY_START).empty());
  SPRAWDZ(!v.empty() && v[0].minuta == 8 * 60 && (v[0].powody & POWOD_SLOT_DZIENNY_GODZINA));
  // naciśnięcie przed importem (06:50 przy imporcie 06:55) się nie liczy: start 07:55
  WejsciePlanu c = poImporcie(7, 0, 5, 6, 40, 6, 55);
  c.nacisniecieRano = sd(6, 50);
  ROWNE_L(poczatekSlotuDziennego(c), sd(7, 55));
  // R-KON-52, przypadek graniczny: start naciśnięciem, wybudzenie przyciskiem bez Wi-Fi; prognoza starsza
  // niż 3 h (z 04:00) i miasto ustawione → pobranie przy najbliższej ocenie o pełnej godzinie (08:00), jeden raz
  WejsciePlanu p = b;
  p.slotDziennyOd = sd(7, 10);
  p.miasto = true;
  p.prognozaGodzinowa = sd(4, 0);
  std::vector<TerminP02> vp = planDoPolnocy(p);
  SPRAWDZ(minutyZBitem(vp, POWOD_PROGNOZA_GODZINOWA) == std::vector<int>{8 * 60});
  SPRAWDZ(!vp.empty() && (vp[0].powody & POWOD_SLOT_DZIENNY_GODZINA) && (vp[0].powody & POWOD_PROGNOZA_GODZINOWA));
  // prognoza świeża przy ocenie 08:00 (z 05:40, 2 h 20 min) albo bez miasta → bez pobrania
  p.prognozaGodzinowa = sd(5, 40);
  SPRAWDZ(!vp.empty() && minutyZBitem(planDoPolnocy(p), POWOD_PROGNOZA_GODZINOWA).empty());
  p.prognozaGodzinowa = sd(4, 0);
  p.miasto = false;
  SPRAWDZ(minutyZBitem(planDoPolnocy(p), POWOD_PROGNOZA_GODZINOWA).empty());
}

// R-KON-41, przypadek graniczny „noc bez terapii albo brak nocy (E-01, E-10)”: slot
// dzienny od 11:00. E-10 (AirBridge odpowiada, nocy nie ma na liście): ocena godzinowa z `/api/status` jak zwykle.
// E-01 (AirBridge nie odpowiada): ocena o pełnych godzinach bez Wi-Fi (POWOD_SLOT_DZIENNY_BEZ_SIECI), zapytania do
// AirBridge tylko w rytmie R-DANE-18 (od drugiej nocy bez danych 14:00, 17:00, 20:00, 23:00).
// Po wdrożeniu mają dalej przechodzić testy T-TEST-279 w test/testy_plan_wybudzen.cpp (testR_DANE_18_symulacjaBezDanych:
// Wi-Fi doby ≤ 45 min, druga doba ≤ 25 min, wybudzenia z Wi-Fi po 11:00 tylko 14:00, 17:00, 20:00, 23:00, 23:30).
static void testR_KON_41_slotBezNocy() {
  // E-10: noc bez terapii, AirBridge działa; sprawdzenie o 11:00 jeszcze przed nami
  WejsciePlanu a = wp(10, 30, 5);
  a.nocBezTerapii = true;
  ROWNE_L(poczatekSlotuDziennego(a), sd(11, 0));
  Termin ta = nastepnyTermin(a);
  ROWNE_L(minutaTerminu(a, ta), 11 * 60);
  SPRAWDZ(ta.powody & POWOD_SLOT_DZIENNY_START);
  SPRAWDZ(sieciowy(ta.powody));
  std::vector<TerminP02> va = planDoPolnocy(a);
  SPRAWDZ(minutyZBitem(va, POWOD_SLOT_DZIENNY_START) == std::vector<int>{11 * 60});
  SPRAWDZ(minutyZBitem(va, POWOD_SLOT_DZIENNY_GODZINA) == godziny(12, 20));
  SPRAWDZ(minutyZBitem(va, POWOD_SLOT_DZIENNY_BEZ_SIECI).empty());
  for (auto& t : va)
    if (t.powody & POWOD_SLOT_DZIENNY_GODZINA) SPRAWDZ(sieciowy(t.powody));
  // E-01, druga noc bez danych (rytm co 3 h): start o 11:00, oceny 12:00…20:00 bez Wi-Fi poza 14:00, 17:00, 20:00
  WejsciePlanu b = wp(10, 30, 5);
  b.e01 = true;
  b.nocyBezDanych = 2;
  ROWNE_L(poczatekSlotuDziennego(b), sd(11, 0));
  std::vector<TerminP02> vb = planDoPolnocy(b);
  SPRAWDZ(minutyZBitem(vb, POWOD_SLOT_DZIENNY_GODZINA).empty());
  std::vector<int> pelne;
  for (auto& t : vb)
    if (t.powody & (SLOT_BITY | POWOD_SLOT_DZIENNY_BEZ_SIECI)) pelne.push_back(t.minuta);
  SPRAWDZ(pelne == godziny(11, 20));  // start o 11:00 i ocena o każdej pełnej godzinie do 20:00
  std::vector<int> zWifi, ocenBezWifi;
  for (auto& t : vb) {
    if (t.minuta < 11 * 60 || t.minuta > 20 * 60) continue;
    if (sieciowy(t.powody)) zWifi.push_back(t.minuta);
    else if (t.powody & POWOD_SLOT_DZIENNY_BEZ_SIECI) ocenBezWifi.push_back(t.minuta);
  }
  SPRAWDZ(zWifi == (std::vector<int>{11 * 60, 14 * 60, 17 * 60, 20 * 60}));  // sprawdzenie 11:00 i R-DANE-18
  SPRAWDZ(ocenBezWifi == (std::vector<int>{12 * 60, 13 * 60, 15 * 60, 16 * 60, 18 * 60, 19 * 60}));
  WejsciePlanu g = b;  // ocena o 12:00 sama: bez Wi-Fi, z rysowaniem (nie wybudzenie pomiarowe)
  g.sekundaDoby = sd(11, 0, 5);
  g.sprawdzenie11 = true;
  g.slotDziennyOd = sd(11, 0, 5);
  Termin tg = nastepnyTermin(g);
  ROWNE_L(tg.powody, POWOD_SLOT_DZIENNY_BEZ_SIECI);
  ROWNE_L(minutaTerminu(g, tg), 12 * 60);
  SPRAWDZ(!pobranieZegarem(tg.powody).pobierz);
  SPRAWDZ(!wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, tg.powody));
  // E-01, pierwsza noc bez danych: ponowienia co 30 min z Wi-Fi zostają, ocena o pełnych godzinach 12:00…20:00
  WejsciePlanu c = b;
  c.nocyBezDanych = 1;
  std::vector<int> pc;
  for (auto& t : planDoPolnocy(c))
    if (t.powody & (SLOT_BITY | POWOD_SLOT_DZIENNY_BEZ_SIECI)) pc.push_back(t.minuta);
  SPRAWDZ(pc == godziny(11, 20));
  // bez E-01 i bez E-10 (noc w budowie, S10): bez slotu do importu
  WejsciePlanu d = wp(10, 30, 5);
  ROWNE_L(poczatekSlotuDziennego(d), -1);
  SPRAWDZ(minutyZBitem(planDoPolnocy(d), POWOD_SLOT_DZIENNY_START).empty());
}

// R-KON-41, tabela przypadków granicznych: panel uśpiony w chwili startu slotu dziennego (np. bateria) → slot dzienny
// zaczyna się przy najbliższym wybudzeniu (R-KON-42 albo przycisk).
static void testR_KON_41_panelUspiony() {
  // ostatnie wybudzenie 06:55:10 (import), start 07:55 przepadł; następne wybudzenie zegarem o 09:00:05
  WejsciePlanu a = poImporcie(9, 0, 5, 6, 40, 6, 55);
  a.obsluzoneZnane = true;
  a.obsluzoneDo = sd(6, 55, 10);
  Powody pa = powodyTeraz(a);
  SPRAWDZ(pa & POWOD_SLOT_DZIENNY_START);
  SPRAWDZ(pa & POWOD_SLOT_DZIENNY_GODZINA);
  SPRAWDZ(sieciowy(pa));
  // bez znanej chwili obsłużonej (np. po starcie bez RTC): też start przy tym wybudzeniu
  a.obsluzoneZnane = false;
  SPRAWDZ(powodyTeraz(a) & POWOD_SLOT_DZIENNY_START);
  // po starcie zapisanym w tym wybudzeniu (slotDziennyOd = 09:00:05) kolejne terminy bez startu, ocena o 10:00
  WejsciePlanu b = a;
  b.obsluzoneZnane = true;
  b.obsluzoneDo = sd(9, 0, 5);
  b.slotDziennyOd = sd(9, 0, 5);
  Termin tb = nastepnyTermin(b);
  ROWNE_L(minutaTerminu(b, tb), 10 * 60);
  SPRAWDZ(tb.powody & POWOD_SLOT_DZIENNY_GODZINA);
  SPRAWDZ(!(tb.powody & POWOD_SLOT_DZIENNY_START));
  // panel przespał cały slot: wybudzenie o 21:30 już bez startu i bez oceny (slot dzienny trwa do 21:00)
  WejsciePlanu c = poImporcie(21, 30, 5, 6, 40, 6, 55);
  c.obsluzoneZnane = true;
  c.obsluzoneDo = sd(6, 55, 10);
  ROWNE_L(powodyTeraz(c) & SLOT_BITY, 0);
}

// R-KON-42, 5.8.12: ocena o każdej pełnej godzinie slotu dziennego; slot od 07:55 → 08:00…20:00 co godzinę, każda
// z Wi-Fi (tylko `/api/status`, sprawdza testR_KON_42_wifiTylkoStatus); koniec o 21:00 (okno stałe kroku 2).
static void testR_KON_42_pelneGodziny() {
  WejsciePlanu w = poImporcie(7, 55, 10, 6, 40, 6, 55);
  w.slotDziennyOd = sd(7, 55, 5);
  std::vector<TerminP02> v = planDoPolnocy(w);
  std::vector<int> oczekiwane = godziny(8, 20);
  SPRAWDZ(minutyZBitem(v, POWOD_SLOT_DZIENNY_GODZINA) == oczekiwane);
  ROWNE_L(minutyZBitem(v, POWOD_SLOT_DZIENNY_GODZINA).size(), 13);
  for (auto& t : v)
    if (t.powody & POWOD_SLOT_DZIENNY_GODZINA) SPRAWDZ(sieciowy(t.powody));
  // od 20:00:05 następny termin bez oceny: 21:00 to już slot wieczorny (bez źródła otoczenia: ocena slotu
  // wieczornego bez Wi-Fi o 21:00, czujniki 23:00)
  WejsciePlanu k = w;
  k.sekundaDoby = sd(20, 0, 5);
  k.sprawdzenie11 = true;
  Termin tk = nastepnyTermin(k);
  ROWNE_L(tk.powody & SLOT_BITY, 0);
  ROWNE_L(minutaTerminu(k, tk), 21 * 60);
  ROWNE_L(tk.powody, POWOD_SLOT_WIECZORNY_BEZ_SIECI);
  // co godzinę, nie częściej: z 09:00:05 sen do 10:00:05
  WejsciePlanu g = w;
  g.sekundaDoby = sd(9, 0, 5);
  Termin tg = nastepnyTermin(g);
  ROWNE_L(tg.sekund, 3600);
  ROWNE_L(tg.powody, POWOD_SLOT_DZIENNY_GODZINA);
  // start o pełnej godzinie (import 07:00 → 08:00, R-NF-02): godziny oceny 09:00…20:00 po starcie
  WejsciePlanu p = poImporcie(7, 0, 10, -1, 0, 7, 0);
  std::vector<TerminP02> u = planDoPolnocy(p);
  std::vector<int> slot;
  for (auto& t : u)
    if (t.powody & SLOT_BITY) slot.push_back(t.minuta);
  SPRAWDZ(slot == godziny(8, 20));
}

// R-KON-52, kolumna „Weryfikacja”: pobranie poranne 05:40, start slotu 07:55 → bez pobrania; start 12:30 → pobranie
// o 12:30; bez miasta → bez zapytania (R-KON-53); ze źródłem lokalnym → zapytanie do serwisu o pola godzinowe.
static void testR_KON_52_prognozaPrzyStarcie() {
  // start 07:55, prognoza z 05:40 (2 h 15 min) → bez pobrania
  WejsciePlanu a = poImporcie(6, 55, 10, 6, 40, 6, 55);
  a.miasto = true;
  a.prognozaGodzinowa = sd(5, 40);
  Termin ta = nastepnyTermin(a);
  ROWNE_L(minutaTerminu(a, ta), 7 * 60 + 55);
  SPRAWDZ(ta.powody & POWOD_SLOT_DZIENNY_START);
  SPRAWDZ(!(ta.powody & POWOD_PROGNOZA_GODZINOWA));
  // start 12:30 (import 11:30), prognoza z 05:40 starsza niż 3 h → pobranie o 12:30
  WejsciePlanu b = poImporcie(11, 30, 10, 6, 40, 11, 30);
  b.miasto = true;
  b.prognozaGodzinowa = sd(5, 40);
  Termin tb = nastepnyTermin(b);
  ROWNE_L(minutaTerminu(b, tb), 12 * 60 + 30);
  SPRAWDZ(tb.powody & POWOD_SLOT_DZIENNY_START);
  SPRAWDZ(tb.powody & POWOD_PROGNOZA_GODZINOWA);
  SPRAWDZ(sieciowy(POWOD_PROGNOZA_GODZINOWA));
  SPRAWDZ(pobranieZegarem(POWOD_PROGNOZA_GODZINOWA).pobierz);
  // bez prognozy dziś → pobranie przy starcie 07:55
  WejsciePlanu n = a;
  n.prognozaGodzinowa = -1;
  SPRAWDZ(nastepnyTermin(n).powody & POWOD_PROGNOZA_GODZINOWA);
  // prognoza z 04:54 przy starcie 07:55 (3 h 1 min) → pobranie
  n.prognozaGodzinowa = sd(4, 54);
  SPRAWDZ(nastepnyTermin(n).powody & POWOD_PROGNOZA_GODZINOWA);
  // bez miasta → bez zapytania, start slotu bez zmian
  WejsciePlanu c = b;
  c.miasto = false;
  Termin tc = nastepnyTermin(c);
  SPRAWDZ(tc.powody & POWOD_SLOT_DZIENNY_START);
  SPRAWDZ(!(tc.powody & POWOD_PROGNOZA_GODZINOWA));
  // lokalne źródło otoczenia skonfigurowane: prognoza godzinowa dalej z serwisu (kontrakt wersji 1 jej nie ma)
  WejsciePlanu z = b;
  z.zrodloOtoczenia = true;
  SPRAWDZ(nastepnyTermin(z).powody & POWOD_PROGNOZA_GODZINOWA);
  // tylko raz, przy starcie: pobranie nieudane o 12:30 → oceny 13:00…20:00 bez R-KON-52
  WejsciePlanu f = b;
  std::vector<TerminP02> v = planDoPolnocy(f, false);
  SPRAWDZ(minutyZBitem(v, POWOD_PROGNOZA_GODZINOWA) == std::vector<int>{12 * 60 + 30});
}

// R-EKR-60 (tylko poza slotem dziennym, w nim ocena R-KON-42): w slocie dziennym brak terminów R-EKR-60;
// przed startem slotu (np. 11:00 bez importu znanego planowi) R-EKR-60 jak w planie bez slotu dziennego.
static void testR_EKR_60_milczyWSlocieDziennym() {
  // weryfikacja R-EKR-60: koniec terapii 10:00, import 10:20, sprawdzenie o 11:00 z rysowaniem ekranu dnia → slot
  // dzienny od 11:20, bez przerysowań R-EKR-60 (ocena R-KON-42 o pełnych godzinach)
  WejsciePlanu a = poImporcie(11, 0, 8, 10, 0, 10, 20);
  a.sprawdzenie11 = true;
  a.dzienRysowany = sd(11, 0, 8);
  Termin ta = nastepnyTermin(a);
  ROWNE_L(minutaTerminu(a, ta), 11 * 60 + 20);
  SPRAWDZ(ta.powody & POWOD_SLOT_DZIENNY_START);
  SPRAWDZ(!(ta.powody & POWOD_ODSWIEZENIE));
  std::vector<TerminP02> v = planDoPolnocy(a);
  SPRAWDZ(minutyZBitem(v, POWOD_ODSWIEZENIE).empty());
  SPRAWDZ(minutyZBitem(v, POWOD_SLOT_DZIENNY_GODZINA) == godziny(12, 20));
  // w trakcie slotu, ekran dnia narysowany o 12:00:08 → 13:00 ocena, nie przerysowanie o 14:00
  WejsciePlanu b = poImporcie(12, 0, 8, 10, 0, 10, 20);
  b.slotDziennyOd = sd(11, 20, 5);
  b.dzienRysowany = sd(12, 0, 8);
  Termin tb = nastepnyTermin(b);
  ROWNE_L(minutaTerminu(b, tb), 13 * 60);
  ROWNE_L(tb.powody, POWOD_SLOT_DZIENNY_GODZINA);
  // przed startem slotu: noc kompletna, ale plan nie zna chwili importu (importNocy -1) → R-EKR-60 jak w planie bez slotu dziennego
  WejsciePlanu c = wp(11, 0, 8);
  c.nocKompletna = true;
  c.sprawdzenie11 = true;
  c.dzienRysowany = sd(11, 0, 8);
  Termin tc = nastepnyTermin(c);
  ROWNE_L(tc.powody, POWOD_ODSWIEZENIE);
  ROWNE_L(minutaTerminu(c, tc), 13 * 60);
}

// R-NF-02, R-NF-03: nazwy nowych powodów w /bateria.csv i ich klasyfikacja sieciowa.
static void testR_NF_02_powodySlotuDziennego() {
  // bity: niezerowe, różne, poza bitami kroku 2 (0…9)
  SPRAWDZ(POWOD_SLOT_DZIENNY_START && POWOD_SLOT_DZIENNY_GODZINA && POWOD_PROGNOZA_GODZINOWA);
  SPRAWDZ((POWOD_SLOT_DZIENNY_START & POWOD_SLOT_DZIENNY_GODZINA) == 0);
  SPRAWDZ(((POWOD_SLOT_DZIENNY_START | POWOD_SLOT_DZIENNY_GODZINA) & POWOD_PROGNOZA_GODZINOWA) == 0);
  SPRAWDZ(((POWOD_SLOT_DZIENNY_START | POWOD_SLOT_DZIENNY_GODZINA | POWOD_PROGNOZA_GODZINOWA) & 0x3FFu) == 0);
  ROWNE(nazwyPowodow(POWOD_SLOT_DZIENNY_START), "slot_dzienny");
  ROWNE(nazwyPowodow(POWOD_SLOT_DZIENNY_GODZINA), "slot_dzienny_godzina");
  ROWNE(nazwyPowodow(POWOD_PROGNOZA_GODZINOWA), "prognoza_godzinowa");
  ROWNE(powodWpisuBaterii(PowodWybudzenia::ZEGAR, POWOD_SLOT_DZIENNY_START), "zegar:slot_dzienny");
  ROWNE(powodWpisuBaterii(PowodWybudzenia::ZEGAR, POWOD_SLOT_DZIENNY_GODZINA), "zegar:slot_dzienny_godzina");
  ROWNE(powodWpisuBaterii(PowodWybudzenia::ZEGAR, POWOD_PROGNOZA_GODZINOWA), "zegar:prognoza_godzinowa");
  ROWNE(nazwyPowodow(POWOD_SLOT_DZIENNY_START | POWOD_PROGNOZA_GODZINOWA), "slot_dzienny+prognoza_godzinowa");
  ROWNE(nazwyPowodow(POWOD_SPRAWDZENIE_11 | POWOD_SLOT_DZIENNY_GODZINA), "sprawdzenie_11+slot_dzienny_godzina");
  // E-01: ocena slotu dziennego bez Wi-Fi (R-KON-41, przypadek graniczny)
  SPRAWDZ(POWOD_SLOT_DZIENNY_BEZ_SIECI &&
          (POWOD_SLOT_DZIENNY_BEZ_SIECI & (SLOT_BITY | POWOD_PROGNOZA_GODZINOWA | 0x3FFu)) == 0);
  ROWNE(nazwyPowodow(POWOD_SLOT_DZIENNY_BEZ_SIECI), "slot_dzienny_bez_sieci");
  ROWNE(powodWpisuBaterii(PowodWybudzenia::ZEGAR, POWOD_SLOT_DZIENNY_BEZ_SIECI), "zegar:slot_dzienny_bez_sieci");
  ROWNE(nazwyPowodow(POWOD_PONOWIENIE | POWOD_SLOT_DZIENNY_BEZ_SIECI), "ponowienie+slot_dzienny_bez_sieci");
  SPRAWDZ(!sieciowy(POWOD_SLOT_DZIENNY_BEZ_SIECI));
  SPRAWDZ(!pobranieZegarem(POWOD_SLOT_DZIENNY_BEZ_SIECI).pobierz);
  SPRAWDZ(!wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, POWOD_SLOT_DZIENNY_BEZ_SIECI | POWOD_CZUJNIKI));
  // R-NF-03: odczyt `/api/status` (R-KON-42, R-KON-56) i prognoza z serwisu (R-KON-52) to pobieranie danych
  for (Powody b : {POWOD_SLOT_DZIENNY_START, POWOD_SLOT_DZIENNY_GODZINA, POWOD_PROGNOZA_GODZINOWA}) {
    SPRAWDZ(sieciowy(b));
    SPRAWDZ(pobranieZegarem(b).pobierz);
    SPRAWDZ(!wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, b));
    SPRAWDZ(!wybudzeniePomiarowe(PowodWybudzenia::ZEGAR, (Powody)(b | POWOD_CZUJNIKI)));
  }
  // kroku 2 bez zmian
  SPRAWDZ(!sieciowy(POWOD_ODSWIEZENIE) && !sieciowy(POWOD_CZUJNIKI));
}

// ============================================================================================================
// (b) Dyrygent z atrapami i pomiarem (wzór: test/testy_plan_wybudzen.cpp, sekcja (b))
// ============================================================================================================

static const KonfiguracjaPanelu KONF = {"192.168.1.20", "cpap", "haslo", "", "", true};
static const char* STATUS_BLOKADA = "{\"version\":\"v2.2.1\",\"rop\":1,\"report_state\":\"blocked\"}";
static const char* STATUS_SPOCZYNEK =
    "{\"version\":\"v2.2.1-25-gebbd145\",\"pna\":\"AirCurve_10_CS_PaceWave\",\"mode\":\"ASVAuto\",\"system\":\"IDLE\","
    "\"rop\":0,\"edf\":\"idle\",\"edf_prefix\":\"\",\"report_state\":\"idle\"}";

static const long DOBA_A = nrDnia("2026-11-03");  // noc z poranka 4.11.2026

// Raport doby jak w test/testy_plan_wybudzen.cpp raport(), z sesją od `startEp` do `koniecEp` (czas unix): koniec terapii
// ostatniej nocy jako `end_ms` ostatniej sesji (Załącznik A).
static std::string raport(long day, uint32_t startEp, uint32_t koniecEp) {
  std::string d = std::to_string(day);
  std::string s = std::to_string((unsigned long long)startEp * 1000ULL);
  std::string k = std::to_string((unsigned long long)koniecEp * 1000ULL);
  return "{\"source\":\"edf\",\"revision\":154,\"data_revision\":11,\"view\":\"day\",\"day\":" + d +
         ",\"first_day\":" + d + ",\"last_day\":" + d + ",\"sessions\":[{\"id\":1,\"day\":" + d +
         ",\"prefix\":\"X\",\"start_ms\":" + s + ",\"end_ms\":" + k + "}],\"present\":true,\"partial\":false"
         ",\"used_seconds\":25200,\"rdi\":1.6,\"csr_seconds\":0,\"leak_threshold\":24,\"leak_high_seconds\":0,"
         "\"session_count\":1,\"ahi\":1.4,\"events_known\":true,\"counts\":{\"oa\":1,\"ca\":5,\"ua\":0,\"h\":4,"
         "\"rera\":2},\"channels\":[{\"id\":\"Press\",\"label\":\"C\",\"unit\":\"cmH2O\",\"count\":12600,"
         "\"coverage_seconds\":25200,\"min\":6.2,\"mean\":10.1,\"median\":9.8,\"p95\":13.6,\"max\":16.0,"
         "\"source\":\"edf\"},{\"id\":\"Leak\",\"label\":\"W\",\"unit\":\"L/min\",\"count\":12600,"
         "\"coverage_seconds\":25200,\"min\":0,\"mean\":3.1,\"median\":2.4,\"p95\":9.6,\"max\":30.2,"
         "\"source\":\"edf\"}],\"settings\":{\"day\":" + d +
         ",\"available\":true,\"fields\":[{\"tag\":\"MOP\",\"label\":\"Tryb\",\"value\":\"ASVAuto\",\"unit\":\"\","
         "\"option\":8}]}}";
}
static std::string lista(long od, long doDnia) {
  std::string l = "{\"source\":\"edf\",\"days\":[";
  for (long x = od; x <= doDnia; x++)
    l += std::string(x > od ? "," : "") + "{\"day\":" + std::to_string(x) + ",\"session_count\":1,\"stale\":false}";
  return l + "]}";
}

// Pomiar jednego wybudzenia (jak test/testy_plan_wybudzen.cpp Pomiar): czas z radiem, rysowanie przy radiu, chwila snu.
struct Pomiar {
  bool radio = false;
  uint32_t radioOdMs = 0, wifiMs = 0, msPrzySnie = 0;
  int polaczen = 0, zadan = 0, rysowanZRadiem = 0, odswiezen = 0;
  bool radioPrzySnie = false;
  void zeruj() { *this = Pomiar(); }
  void zamknijRadio(uint32_t teraz) {
    if (!radio) return;
    wifiMs += teraz - radioOdMs;
    radio = false;
  }
};

class SiecPomiar : public AtrapaSieci {
 public:
  SiecPomiar(Dziennik& d, AtrapaZegara& z, Pomiar& m) : AtrapaSieci(d, z), z_(z), m_(m) {}
  bool polacz(uint32_t limitMs, const std::function<bool()>& przerwij, bool& przerwane) override {
    if (!m_.radio) { m_.radio = true; m_.radioOdMs = z_.msTeraz; }
    m_.polaczen++;
    return AtrapaSieci::polacz(limitMs, przerwij, przerwane);
  }
  OdpowiedzHttp get(const ZadanieHttp& z) override {
    m_.zadan++;
    return AtrapaSieci::get(z);
  }
  void wylacz() override {
    m_.zamknijRadio(z_.msTeraz);
    AtrapaSieci::wylacz();
  }
 private:
  AtrapaZegara& z_;
  Pomiar& m_;
};

class EkranPomiar : public AtrapaWyswietlacza {
 public:
  EkranPomiar(Dziennik& d, Pomiar& m) : AtrapaWyswietlacza(d), m_(m) {}
  void odswiez() override {
    m_.odswiezen++;
    if (m_.radio) m_.rysowanZRadiem++;
    AtrapaWyswietlacza::odswiez();
  }
 private:
  Pomiar& m_;
};

class SenPomiar : public AtrapaSnu {
 public:
  SenPomiar(Dziennik& d, AtrapaUruchomienia& u, AtrapaStraznika& s, AtrapaZegara& z, Pomiar& m)
      : AtrapaSnu(d, u, s), z_(z), m_(m) {}
  void spij(const PlanSnu& p) override {
    m_.msPrzySnie = z_.msTeraz;
    m_.radioPrzySnie = m_.radio;
    m_.zamknijRadio(z_.msTeraz);
    AtrapaSnu::spij(p);
  }
 private:
  AtrapaZegara& z_;
  Pomiar& m_;
};

struct PanelSym {
  Panel p;
  Pomiar m;
  SiecPomiar siec{p.d, p.zegar, m};
  EkranPomiar ekran{p.d, m};
  SenPomiar sen{p.d, p.start, p.straznik, p.zegar, m};
  Adaptery adaptery() {
    return Adaptery{p.zegar, p.nvs, p.pliki, siec, ekran, p.przyciski, p.czujniki, p.dzwiek, p.start, p.straznik, sen,
                    p.port, p.diag};
  }
};

struct Wyb {
  uint32_t ep = 0;
  PowodWybudzenia powod = PowodWybudzenia::ZEGAR;
  int minuta = 0;
  std::string data;
  Pomiar m;
  bool spal = false;
  std::vector<std::string> zdarzenia;
};
static int minutaLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  return l.tm_hour * 60 + l.tm_min;
}
static std::string dataLok(uint32_t t) {
  time_t tt = (time_t)t;
  std::tm l{};
  localtime_r(&tt, &l);
  char b[16];
  strftime(b, sizeof b, "%Y-%m-%d", &l);
  return b;
}

static Wyb wybudz(PanelSym& s, uint32_t t, PowodWybudzenia pw, const KonfiguracjaPanelu& k) {
  s.p.zegar.epoch = t;
  s.p.zegar.msTeraz = 0;
  s.p.start.przyczynaStartu = pw == PowodWybudzenia::START ? PrzyczynaStartu::ZASILANIE : PrzyczynaStartu::USPIENIE;
  s.p.start.powodWybudzenia = pw;
  s.siec.polaczonaTeraz = false;
  s.siec.okno = false;
  s.p.d.z.clear();
  s.m.zeruj();
  int snow = s.sen.snow;
  Dyrygent(s.adaptery(), k).wybudzenie();
  Wyb w;
  w.ep = t;
  w.powod = pw;
  w.minuta = minutaLok(t);
  w.data = dataLok(t);
  w.m = s.m;
  w.spal = s.sen.snow == snow + 1;
  w.zdarzenia = s.p.d.z;
  return w;
}

using Odpowiedzi = void (*)(PanelSym&, uint32_t);

// Kolejne wybudzenia od startu zasilania o `od` do `doT`, każde po śnie z planu dyrygenta. `zmiennaTemp`: czujnik
// w obudowie co godzinę inny (stopka `przy łóżku` się zmienia), inaczej stały odczyt atrapy.
static std::vector<Wyb> symuluj(PanelSym& s, uint32_t od, uint32_t doT, Odpowiedzi odp, bool zmiennaTemp = false,
                                const KonfiguracjaPanelu& k = KONF) {
  std::vector<Wyb> v;
  zerujPrzedSnem(s.p.start.l, identyfikatorWersji(s.p.start.napisWersji));
  uint32_t t = od;
  PowodWybudzenia pw = PowodWybudzenia::START;
  for (int i = 0; i < 600 && t < doT; i++) {
    odp(s, t);
    if (zmiennaTemp) s.p.czujniki.o.temp = 15.0f + (float)(minutaLok(t) / 60);
    Wyb w = wybudz(s, t, pw, k);
    v.push_back(w);
    if (!w.spal) break;
    uint32_t snu = s.sen.plan.budzZegarem ? s.sen.plan.sekund : OKRES_SNU_S;
    if (snu == 0) snu = 1;
    t += w.m.msPrzySnie / 1000 + snu;
    pw = PowodWybudzenia::ZEGAR;
  }
  return v;
}

// Pole `powod` wybudzenia z logu dyrygenta („…, wybudzenie zegar:a+b, licznik …”); "" gdy brak wiersza.
// szuka „, wybudzenie ” w wierszu wybudzenia, a nie
// pierwszego „wybudzenie ” w dzienniku (wiersz `start: przyczyna wybudzenie z uśpienia, …` stoi wcześniej).
static std::string powodWyb(const Wyb& w) {
  for (auto& z : w.zdarzenia) {
    size_t a = z.find(", wybudzenie ");
    if (a == std::string::npos) continue;
    a += 2;
    size_t b = z.find(',', a);
    if (b == std::string::npos) continue;
    return z.substr(a + 11, b - a - 11);
  }
  return "";
}
// Czy wybudzenie ma powód o nazwie `n` (jedna z nazw złączonych „+” po „zegar:”).
static bool maPowod(const Wyb& w, const std::string& n) {
  std::string p = powodWyb(w);
  if (p.compare(0, 6, "zegar:") != 0) return false;
  std::string r = "+" + p.substr(6) + "+";
  return r.find("+" + n + "+") != std::string::npos;
}
static bool maPowodSlotu(const Wyb& w) { return maPowod(w, "slot_dzienny") || maPowod(w, "slot_dzienny_godzina"); }
static std::vector<int> minutyZPowodem(const std::vector<Wyb>& v, const std::string& n) {
  std::vector<int> m;
  for (auto& w : v)
    if (w.powod == PowodWybudzenia::ZEGAR && maPowod(w, n)) m.push_back(w.minuta);
  return m;
}
// Żądania GET jednego wybudzenia (URL-e z dziennika atrap) i liczba tych, których ścieżka zaczyna się od `p`.
static std::vector<std::string> zadaniaWyb(const Wyb& w) {
  std::vector<std::string> u;
  for (auto& z : w.zdarzenia)
    if (z.compare(0, 9, "siec.get ") == 0) u.push_back(z.substr(9));
  return u;
}
static int zadanZeSciezka(const Wyb& w, const std::string& p) {
  int n = 0;
  for (auto& u : zadaniaWyb(w)) n += AtrapaSieci::sciezka(u).compare(0, p.size(), p) == 0;
  return n;
}
static int indeks(const Wyb& w, const std::string& p) {
  for (size_t i = 0; i < w.zdarzenia.size(); i++)
    if (w.zdarzenia[i].compare(0, p.size(), p) == 0) return (int)i;
  return -1;
}
static const Wyb* wybudzenieO(const std::vector<Wyb>& v, int minuta) {
  for (auto& w : v)
    if (w.minuta == minuta && w.powod == PowodWybudzenia::ZEGAR) return &w;
  return nullptr;
}

// Doba 4.11.2026: terapia 3.11 23:20 – 4.11 06:40; AirBridge ma raport od 06:40, więc okno poranne zapisuje noc
// o 07:00 (import 07:00, koniec terapii 06:40) → slot dzienny od 08:00 (przykład R-NF-02).
static void odpowiedziSlotu(PanelSym& s, uint32_t t) {
  s.siec.odpowiedzi.clear();
  if (t >= ep(2026, 11, 4, 6, 40) && t < ep(2026, 11, 5, 0, 0)) {
    s.siec.dodaj("/api/status", 200, STATUS_SPOCZYNEK);
    s.siec.dodaj(sciezkaListy(), 200, lista(DOBA_A, DOBA_A));
    s.siec.dodaj(sciezkaNocy(DOBA_A), 200, raport(DOBA_A, ep(2026, 11, 3, 23, 20), ep(2026, 11, 4, 6, 40)));
  } else {
    s.siec.dodaj("/api/status", 200, STATUS_BLOKADA);
  }
}
static const char* OTO_ZWYKLE = "{\"wersja\":1,\"ostrzezenia\":[]}";
static void odpowiedziSlotuZOtoczeniem(PanelSym& s, uint32_t t) {
  odpowiedziSlotu(s, t);
  s.siec.dodaj("/otoczenie", 200, OTO_ZWYKLE);
}

// R-KON-42 („odświeżać ekran tylko wtedy, gdy zmienia się wybrany ekran, jego tekst albo wizualizacja”
// jak przerysowanie R-EKR-60): ocena bez zmiany klatki → 0 odświeżeń e-ink; ze zmianą stopki → odświeżenie.
static void testR_KON_42_bezOdswiezeniaBezZmiany() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 13, 30), odpowiedziSlotu);
  int ocen = 0;
  for (int h : {9, 10, 12, 13}) {
    const Wyb* w = wybudzenieO(v, h * 60);
    SPRAWDZ(w != nullptr);
    if (!w) continue;
    SPRAWDZ(maPowod(*w, "slot_dzienny_godzina"));
    ROWNE_L(w->m.odswiezen, 0);  // odczyt czujnika stały: klatka jak wyświetlona
    bool bezZmian = false;
    for (auto& z : w->zdarzenia) bezZmian = bezZmian || z.find("bez zmian: klatka jak wyświetlona") != std::string::npos;
    SPRAWDZ(bezZmian);  // ocena była (ekran narysowany do bufora), tylko bez odświeżenia
    ocen++;
  }
  ROWNE_L(ocen, 4);
  // czujnik co godzinę inny: od 06.11.2026 stopka `przy łóżku` przy ocenie o pełnej godzinie
  // z klatki, więc sama zmiana odczytu nie odświeża e-ink (wcześniej: jedno odświeżenie przy każdej ocenie)
  PanelSym q;
  std::vector<Wyb> u = symuluj(q, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 13, 30), odpowiedziSlotu, true);
  for (int h : {9, 10, 12, 13}) {
    const Wyb* w = wybudzenieO(u, h * 60);
    SPRAWDZ(w != nullptr && maPowod(*w, "slot_dzienny_godzina") && w->m.odswiezen == 0);
  }
}

// R-KON-42, karta wybudzenie o pełnej godzinie slotu: dokładnie jedno żądanie `GET /api/status`, bez raportu
// nocy i bez pogody, Wi-Fi wyłączone przed rysowaniem (T-WYM-20, 7.1 krok 9).
static void testR_KON_42_wifiTylkoStatus() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 10, 30), odpowiedziSlotu, true);
  for (int h : {9, 10}) {
    const Wyb* w = wybudzenieO(v, h * 60);
    SPRAWDZ(w != nullptr);
    if (!w) continue;
    SPRAWDZ(maPowod(*w, "slot_dzienny_godzina"));
    ROWNE_L(w->m.polaczen, 1);
    ROWNE_L(zadanZeSciezka(*w, "/api/status"), 1);
    ROWNE_L(zadaniaWyb(*w).size(), 1);  // bez listy nocy, raportu nocy i pogody
    ROWNE_L(zadanZeSciezka(*w, "/api/report"), 0);
    ROWNE_L(w->m.rysowanZRadiem, 0);
    SPRAWDZ(!w->m.radioPrzySnie);
    int wyl = indeks(*w, "siec.wylacz"), ekr = indeks(*w, "ekran.");
    SPRAWDZ(wyl >= 0);
    SPRAWDZ(ekr < 0 || wyl < ekr);
    SPRAWDZ(w->m.wifiMs > 0 && w->m.wifiMs <= 8000);  // krótki odczyt (T-NF-05: ≤ 8 s bez nowych danych)
  }
  // ze skonfigurowanym źródłem otoczenia (R-DANE-33, 5.8.12): dokładnie jedno GET do źródła w tym samym
  // połączeniu, dalej bez serwisu pogody i bez raportu nocy
  KonfiguracjaPanelu k = KONF;
  k.otoUrl = "http://192.168.1.60:8080/otoczenie";
  k.otoToken = "t";
  PanelSym q;
  std::vector<Wyb> u = symuluj(q, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 10, 30), odpowiedziSlotuZOtoczeniem, true, k);
  const Wyb* w9 = wybudzenieO(u, 9 * 60);
  SPRAWDZ(w9 != nullptr && maPowod(*w9, "slot_dzienny_godzina"));
  if (w9) {
    ROWNE_L(zadanZeSciezka(*w9, "/api/status"), 1);
    ROWNE_L(zadanZeSciezka(*w9, "/otoczenie"), 1);
    ROWNE_L(zadaniaWyb(*w9).size(), 2);
    ROWNE_L(zadanZeSciezka(*w9, "/api/report"), 0);
    ROWNE_L(w9->m.polaczen, 1);
    ROWNE_L(w9->m.rysowanZRadiem, 0);
  }
}

// R-NF-02, kolumna „Weryfikacja”: noc pobrana o 07:00, okno slotu wieczornego od 21:00 → start slotu
// dziennego o 08:00 z powodem R-KON-41, wybudzenia o 09:00…20:00 z powodem R-KON-42, każde z Wi-Fi tylko na
// `/api/status`, bez wybudzeń z powodem R-EKR-60; każde wybudzenie z powodem z R-NF-02; bez Wi-Fi 23:31–04:59
// (R-DANE-18; poza startem zasilania). Liczba wybudzeń i czas Wi-Fi atrapy to dane do, nie próg.
static void testR_NF_02_symulacjaDobyZeSlotemDziennym() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 5), odpowiedziSlotu, true);
  SPRAWDZ(!v.empty());
  // każde wybudzenie z powodem z R-NF-02 (nazwy kroku 2 i) i wiersz /bateria.csv dla każdego
  static const std::set<std::string> NAZWY = {"okno_poranne", "sprawdzenie_11", "ponowienie", "pogoda",
                                              "poczatek_nocy", "czujniki", "powrot", "pytanie", "czas",
                                              "odswiezenie", "slot_dzienny", "slot_dzienny_godzina",
                                              "prognoza_godzinowa", "slot_dzienny_bez_sieci",
                                              "slot_wieczorny_bez_sieci"};  // (R-KON-01)
  int bezPowodu = 0, zWifi = 0, wifiNoca = 0;
  uint32_t wifi = 0;
  for (auto& w : v) {
    wifi += w.m.wifiMs;
    zWifi += w.m.polaczen > 0;
    if (w.powod == PowodWybudzenia::ZEGAR) {
      std::string p = powodWyb(w);
      if (p.compare(0, 6, "zegar:") != 0 || p.size() == 6) {
        bezPowodu++;
      } else {
        std::string r = p.substr(6) + "+";
        size_t a = 0;
        while (a < r.size()) {
          size_t b = r.find('+', a);
          if (!NAZWY.count(r.substr(a, b - a))) { bezPowodu++; break; }
          a = b + 1;
        }
      }
      bool noc = w.minuta >= 23 * 60 + 31 || w.minuta < 5 * 60;
      if (noc && (w.m.polaczen > 0 || w.m.wifiMs > 0)) wifiNoca++;
    }
  }
  ROWNE_L(bezPowodu, 0);
  ROWNE_L(wifiNoca, 0);
  // dziennik wybudzeń w /log/wybudzenia.csv z nagłówkiem i wierszem nazw pól (2 wiersze ponad wpisy)
  const std::string& wyb = s.p.pliki.pliki["/log/wybudzenia.csv"];
  ROWNE_L(std::count(wyb.begin(), wyb.end(), '\n') - 2, v.size());
  // start slotu dziennego o 08:00 (R-KON-41) i oceny 09:00…20:00 (R-KON-42): 13 wybudzeń slotu
  SPRAWDZ(minutyZPowodem(v, "slot_dzienny") == std::vector<int>{8 * 60});
  std::vector<int> slot;
  for (auto& w : v)
    if (w.powod == PowodWybudzenia::ZEGAR && maPowodSlotu(w)) slot.push_back(w.minuta);
  SPRAWDZ(slot == godziny(8, 20));
  ROWNE_L(slot.size(), 13);
  std::vector<int> g = minutyZPowodem(v, "slot_dzienny_godzina");
  for (int h = 9; h <= 20; h++) SPRAWDZ(std::find(g.begin(), g.end(), h * 60) != g.end());
  // każda ocena: jedno `/api/status`; gdy slot to jedyny powód sieciowy, bez innych żądań
  int ocenZJednymStatusem = 0, ocenTylkoStatus = 0, ocenSamegoSlotu = 0;
  for (auto& w : v) {
    if (w.powod != PowodWybudzenia::ZEGAR || !maPowodSlotu(w)) continue;
    ocenZJednymStatusem += zadanZeSciezka(w, "/api/status") == 1;
    bool samSlot = !maPowod(w, "sprawdzenie_11") && !maPowod(w, "okno_poranne") && !maPowod(w, "ponowienie") &&
                   !maPowod(w, "pogoda") && !maPowod(w, "prognoza_godzinowa");
    if (!samSlot) continue;
    ocenSamegoSlotu++;
    ocenTylkoStatus += zadaniaWyb(w).size() == 1 && w.m.rysowanZRadiem == 0 && !w.m.radioPrzySnie;
  }
  ROWNE_L(ocenZJednymStatusem, 13);
  SPRAWDZ(ocenSamegoSlotu >= 12);  // 08:00…20:00 bez 11:00 (sprawdzenie R-DANE-24 w tym samym wybudzeniu)
  ROWNE_L(ocenTylkoStatus, ocenSamegoSlotu);
  // bez przerysowań R-EKR-60 w slocie dziennym
  SPRAWDZ(minutyZPowodem(v, "odswiezenie").empty());
  // budżet doby 8.1 i 13.1 wiersz 2 na dobie ze slotem dziennym (Wi-Fi ≤ 10 min, ≤ 80 wybudzeń, ≤ 30 pełnych odświeżeń)
  int odswiezen = 0;
  for (auto& w : v) odswiezen += w.m.odswiezen;
  SPRAWDZ(wifi <= 10u * 60u * 1000u);
  SPRAWDZ(v.size() <= 80);
  SPRAWDZ(odswiezen <= 30);
  // dane do (log doby ze slotem dziennym; nie próg)
  printf("  doba ze slotem dziennym (atrapy, 4.11.2026): wybudzeń %zu, z Wi-Fi %d, Wi-Fi razem %u ms (%.1f min)\n",
         v.size(), zWifi, wifi, wifi / 60000.0);
}

// ============================================================================================================
// (c) Testy przypadków brzegowych slotu dziennego (z asercją budżetu doby)
// ============================================================================================================

// [R1] Import nocy z sekundami przed pełną godziną (np. pobranie po naciśnięciu o 07:59:30): start slotu 08:59:30.
// R-KON-41/R-KON-42 dają jedną ocenę przy starcie i jedną o 09:00; plan budzi panel dwa razy z Wi-Fi w ciągu minuty
// (08:59:05 przed chwilą startu i 09:00:05).
static void dwaWybudzeniaZWifiWMinucie() {
  WejsciePlanu w = poImporcie(7, 59, 40, -1, 0, 7, 59);
  w.importNocy = sd(7, 59, 30);
  std::vector<TerminP02> v = planDoPolnocy(w);
  int sieciowychPrzed0905 = 0;
  for (auto& t : v)
    if (t.minuta >= 8 * 60 + 59 && t.minuta <= 9 * 60 && sieciowy(t.powody)) sieciowychPrzed0905++;
  printf("  import 07:59:30 → wybudzeń z Wi-Fi 08:59–09:00: %d (pierwsze o %02d:%02d, przed chwilą startu)\n",
         sieciowychPrzed0905, v.empty() ? 0 : v[0].minuta / 60, v.empty() ? 0 : v[0].minuta % 60);
  ROWNE_L(sieciowychPrzed0905, 1);  // oczekiwane: jedno wybudzenie z Wi-Fi (start i ocena 09:00 razem)
}

// [R2] Zanik zasilania między importem (07:00) a startem slotu (08:00), powrót prądu o 07:30. R-KON-41 nie ma wyjątku
// dla zaniku zasilania; T-WYM-23: po starcie innym niż wybudzenie stan z NVS i plików. Oczekiwane: slot dzienny
// (start albo ocena przy najbliższym wybudzeniu, tabela R-KON-41), w dobie ocen z `/api/status` o pełnych godzinach.
static void zanikZasilaniaPrzedStartemSlotu() {
  PanelSym s;
  symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 7, 20), odpowiedziSlotu);
  PanelSym q;  // nowe RTC (zanik zasilania), pliki i NVS z panelu przed zanikiem
  q.p.pliki.pliki = s.p.pliki.pliki;
  q.p.nvs.teksty = s.p.nvs.teksty;
  q.p.nvs.liczbyU = s.p.nvs.liczbyU;
  q.p.nvs.liczby = s.p.nvs.liczby;
  q.p.nvs.flagi = s.p.nvs.flagi;
  q.p.nvs.bajtyMap = s.p.nvs.bajtyMap;
  std::vector<Wyb> v = symuluj(q, ep(2026, 11, 4, 7, 30), ep(2026, 11, 4, 21, 0), odpowiedziSlotu);
  int slotu = 0, odswiezen = 0;
  for (auto& w : v) {
    slotu += maPowodSlotu(w);
    odswiezen += maPowod(w, "odswiezenie");
  }
  printf("  zanik zasilania 07:20–07:30 → wybudzeń z powodem slotu %d, z powodem R-EKR-60 %d\n", slotu, odswiezen);
  SPRAWDZ(slotu > 0);
  ROWNE_L(odswiezen, 0);
}

// [R3] Zegar panelu spieszy się 2 min, NTP cofa go w wybudzeniu startu slotu (08:00:05 panelu = 07:58:05). Chwila
// startu zapisana przed NTP wypada „w przyszłości”: następny termin to znowu start o 08:00, a to wybudzenie nie ma
// żadnego powodu (R-NF-02: zero wybudzeń bez powodu).
static void ntpCofaZegarWStarcieSlotu() {
  PanelSym s;
  symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 7, 30), odpowiedziSlotu);
  // wybudzenie zegarem o 08:00:05 czasu panelu; NTP podaje 07:58:05 (zegar o 120 s do przodu)
  uint32_t t = ep(2026, 11, 4, 8, 0, 5);
  odpowiedziSlotu(s, t);
  s.siec.czasNtp = t - 120;
  Wyb w1 = wybudz(s, t, PowodWybudzenia::ZEGAR, KONF);
  s.siec.czasNtp = 0;
  uint32_t snu = s.sen.plan.sekund;
  uint32_t t2 = s.p.zegar.teraz() + snu;
  odpowiedziSlotu(s, t2);
  Wyb w2 = wybudz(s, t2, PowodWybudzenia::ZEGAR, KONF);
  printf("  1. wybudzenie `%s`, sen %u s; 2. wybudzenie %02d:%02d `%s`\n", powodWyb(w1).c_str(), snu,
         w2.minuta / 60, w2.minuta % 60, powodWyb(w2).c_str());
  SPRAWDZ(powodWyb(w2).size() > 6);  // wybudzenie z powodem z R-NF-02
}

// [R3b] To samo co R3 dla terminu kroku 2 (sprawdzenie o 11:00, plan bez slotu): czy wybudzenie bez powodu po cofnięciu
// zegara przez NTP jest też w planie bez slotu dziennego (wtedy tylko mnoży okazje: 13 wybudzeń z NTP dziennie więcej).
static void ntpCofaZegarPoStarcieSlotu() {
  PanelSym s;
  symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 7, 30), odpowiedziSlotu);
  s.p.start.stan.importNocyEp = 0;  // chwila importu nieznana (po zastępnik z NVS, więc slot dalej trwa)
  uint32_t t = ep(2026, 11, 4, 11, 0, 5);
  odpowiedziSlotu(s, t);
  s.siec.czasNtp = t - 120;
  Wyb w1 = wybudz(s, t, PowodWybudzenia::ZEGAR, KONF);
  s.siec.czasNtp = 0;
  uint32_t t2 = s.p.zegar.teraz() + s.sen.plan.sekund;
  odpowiedziSlotu(s, t2);
  Wyb w2 = wybudz(s, t2, PowodWybudzenia::ZEGAR, KONF);
  printf("  1. wybudzenie `%s`; 2. wybudzenie %02d:%02d `%s`\n", powodWyb(w1).c_str(), w2.minuta / 60,
         w2.minuta % 60, powodWyb(w2).c_str());
}

// [R3c] Zegar spieszy się o 35 s na godzinę (generator RC bez kwarcu): w każdym wybudzeniu z Wi-Fi NTP cofa
// zegar. Doba ze slotem dziennym: ile wybudzeń bez powodu (R-NF-02: zero).
static void odpowiedziZDryfem(PanelSym& s, uint32_t t) {
  odpowiedziSlotu(s, t);
  s.siec.czasNtp = t - 35;  // NTP: prawdziwy czas 35 s wcześniej niż zegar panelu w chwili wybudzenia
}
static void dryfZegaraDobaZeSlotem() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 21, 0), odpowiedziZDryfem);
  int bezPowodu = 0;
  std::string gdzie;
  for (auto& w : v)
    if (w.powod == PowodWybudzenia::ZEGAR && powodWyb(w).size() <= 6) {
      bezPowodu++;
      char b[32];
      snprintf(b, sizeof b, " %02d:%02d", w.minuta / 60, w.minuta % 60);
      gdzie += b;
    }
  printf("  NTP cofa zegar o 35 s w każdym wybudzeniu z Wi-Fi → wybudzeń bez powodu %d:%s\n", bezPowodu,
         gdzie.c_str());
  ROWNE_L(bezPowodu, 0);
}

// [R4] Noc zapisana o 07:00, AirBridge milknie od 07:30 (gaśnie z aparatem). Każda ocena R-KON-42 włącza Wi-Fi
// i ponawia `/api/status` (R-AW-01). Pomiar: wybudzenia z Wi-Fi i czas Wi-Fi atrapy w slocie (dane do).
static void odpowiedziMilczyOd0730(PanelSym& s, uint32_t t) {
  if (t < ep(2026, 11, 4, 7, 30)) { odpowiedziSlotu(s, t); return; }
  s.siec.odpowiedzi.clear();
}
static void airbridgeMilczyWSlocie() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 5), odpowiedziMilczyOd0730);
  uint32_t wifi = 0, wifiSlot = 0;
  int zWifi = 0, slotZWifi = 0, bezPowodu = 0;
  for (auto& w : v) {
    wifi += w.m.wifiMs;
    zWifi += w.m.polaczen > 0;
    if (w.powod == PowodWybudzenia::ZEGAR && powodWyb(w).size() <= 6) bezPowodu++;
    if (maPowodSlotu(w) && w.m.polaczen > 0) {
      slotZWifi++;
      wifiSlot += w.m.wifiMs;
    }
  }
  printf("  AirBridge milczy od 07:30 → wybudzeń %zu, z Wi-Fi %d (slot %d), Wi-Fi %u ms (slot %u ms)\n", v.size(),
         zWifi, slotZWifi, wifi, wifiSlot);
  ROWNE_L(bezPowodu, 0);
  SPRAWDZ(wifi <= 10u * 60u * 1000u);  // budżet doby z 13.1 wiersz 2 (Wi-Fi < 10 min) na realnym przebiegu
  // po poprawce (jedna próba `/api/status`, LIMIT_STATUSU_SLOTU_MS): doba daleko poniżej budżetu, a ocena samego
  // slotu najwyżej połączenie, NTP i jeden limit odpowiedzi (≤ 8 s, T-NF-05)
  SPRAWDZ(wifi <= 5u * 60u * 1000u);
  for (auto& w : v)
    if (w.powod == PowodWybudzenia::ZEGAR && maPowodSlotu(w) && !maPowod(w, "sprawdzenie_11")) SPRAWDZ(w.m.wifiMs <= 8000);
  // (R-KON-56 przypadek graniczny): brak odpowiedzi w ocenach 08:00…10:00 nie liczy się do E-01
  PanelSym q;
  symuluj(q, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 10, 30), odpowiedziMilczyOd0730);
  ROWNE_L(q.p.start.stan.ab.bladOd, 0);
}

// [R5] Doba ze slotem dziennym (realny przebieg, bez wariantu BezSlotuDziennego): kryterium 13.1 wiersz 2
// „Wi-Fi poniżej 10 min” i ≤ 80 wybudzeń sprawdzone asercją (testR_NF_02_symulacjaDobyZeSlotemDziennym tylko drukuje).
static void budzetDobyZeSlotem() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 5, 0, 5), odpowiedziSlotu, true);
  uint32_t wifi = 0;
  int odswiezen = 0;
  for (auto& w : v) {
    wifi += w.m.wifiMs;
    odswiezen += w.m.odswiezen;
  }
  printf("  doba ze slotem (czujnik co godzinę inny): wybudzeń %zu, Wi-Fi %u ms, pełnych odświeżeń %d\n", v.size(),
         wifi, odswiezen);
  SPRAWDZ(odswiezen <= 30);  // 8.1, T-TEST-281: pełne odświeżenia ≤ 30 na dobę
  SPRAWDZ(v.size() <= 80);
  SPRAWDZ(wifi < 10u * 60u * 1000u);
}

// [R6] E-01 rano, slot bez nocy od 11:00 (ocena bez Wi-Fi), AirBridge wraca i noc zapisana o 13:30. R-KON-41: slot
// dzienny 60 min po imporcie (14:30); kod: slot „już wystartował” o 11:00, ocena z Wi-Fi o 14:00 (30 min po imporcie).
static void importPoStarcieSlotuBezNocy() {
  WejsciePlanu w = poImporcie(13, 30, 10, -1, 0, 13, 30);
  w.slotDziennyOd = sd(11, 0, 5);  // start z POWOD_SLOT_DZIENNY_BEZ_SIECI o 11:00 (E-01 bez nocy)
  Termin t = nastepnyTermin(w);
  printf("  import 13:30 po slocie bez nocy od 11:00 → następny termin %02d:%02d `%s`\n", minutaTerminu(w, t) / 60,
         minutaTerminu(w, t) % 60, nazwyPowodow(t.powody).c_str());
  SPRAWDZ(minutaTerminu(w, t) == 14 * 60 + 30 || !(t.powody & POWOD_SLOT_DZIENNY_GODZINA));  // [spec niejasna]
}

// (sonda E): E-01 rano, AirBridge wraca o 11:00 z nocą (sprawdzenie 11:00 z oceną bez Wi-Fi
// w tym samym wybudzeniu) → start bez sieci nie zapisany, bo noc już jest: slot dzienny 60 min po imporcie, od 12:00.
static void odpE01Do11(PanelSym& s, uint32_t t) {
  if (t < ep(2026, 11, 4, 11, 0)) { s.siec.odpowiedzi.clear(); return; }
  odpowiedziSlotu(s, t);
}
static void e01NocO11() {
  PanelSym s;
  std::vector<Wyb> v = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 15, 5), odpE01Do11);
  SPRAWDZ(minutyZPowodem(v, "slot_dzienny") == std::vector<int>{12 * 60});
  std::vector<int> g = minutyZPowodem(v, "slot_dzienny_godzina");
  SPRAWDZ(std::find(g.begin(), g.end(), 12 * 60) == g.end() && std::find(g.begin(), g.end(), 13 * 60) != g.end());
}

// (sonda F): zanik zasilania, prąd wraca o 14:00 → zastępnik importu z NVS sprzed pobrania
// (odczyt z 11:00), start slotu (12:00) już minął → slot od startu zasilania, o 15:00 ocena, nie start; potem
// pobranie po naciśnięciu (◀ 2 s na K2 → ekran testu) nie przesuwa chwili importu i nie startuje slotu drugi raz.
static void pradWracaO14() {
  PanelSym s;
  symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 13, 50), odpowiedziSlotu);
  PanelSym q;
  q.p.pliki.pliki = s.p.pliki.pliki;
  q.p.nvs.teksty = s.p.nvs.teksty;
  q.p.nvs.liczbyU = s.p.nvs.liczbyU;
  q.p.nvs.liczby = s.p.nvs.liczby;
  q.p.nvs.flagi = s.p.nvs.flagi;
  q.p.nvs.bajtyMap = s.p.nvs.bajtyMap;
  std::vector<Wyb> v = symuluj(q, ep(2026, 11, 4, 14, 0), ep(2026, 11, 4, 17, 5), odpowiedziSlotu);
  SPRAWDZ(minutyZPowodem(v, "slot_dzienny").empty());
  SPRAWDZ(minutyZPowodem(v, "slot_dzienny_godzina") == (std::vector<int>{15 * 60, 16 * 60, 17 * 60}));
  // pobranie po naciśnięciu nie startuje slotu drugi raz
  PanelSym r;
  r.p.pliki.pliki = s.p.pliki.pliki;
  r.p.nvs.teksty = s.p.nvs.teksty;
  r.p.nvs.liczbyU = s.p.nvs.liczbyU;
  r.p.nvs.liczby = s.p.nvs.liczby;
  r.p.nvs.flagi = s.p.nvs.flagi;
  r.p.nvs.bajtyMap = s.p.nvs.bajtyMap;
  symuluj(r, ep(2026, 11, 4, 14, 0), ep(2026, 11, 4, 14, 1), odpowiedziSlotu);
  uint32_t imp = r.p.start.stan.importNocyEp, slot = r.p.start.stan.slotDziennyEp;
  SPRAWDZ(imp != 0 && slot != 0);
  uint32_t t = ep(2026, 11, 4, 14, 30);
  odpowiedziSlotu(r, t);
  r.p.start.stan.ekran = (uint8_t)Ekran::FORMULARZ;  // U-05 z U-03 przez ▶ 2 s
  r.p.start.budzace = BIT_PRAWY;
  r.p.przyciski.skrypt = {{0, BIT_PRAWY}, {2300, 0}};
  Wyb n = wybudz(r, t, PowodWybudzenia::PRZYCISK, KONF);
  r.p.start.budzace = 0;
  r.p.przyciski.skrypt.clear();
  SPRAWDZ(n.m.polaczen > 0 && zadanZeSciezka(n, "/api/status") >= 1);  // pobranie po naciśnięciu było
  SPRAWDZ(r.p.start.stan.importNocyEp == imp && r.p.start.stan.slotDziennyEp == slot);
  uint32_t t2 = r.p.zegar.teraz() + r.sen.plan.sekund;
  odpowiedziSlotu(r, t2);
  Wyb z = wybudz(r, t2, PowodWybudzenia::ZEGAR, KONF);
  SPRAWDZ(!maPowod(z, "slot_dzienny") && r.p.start.stan.slotDziennyEp == slot);
}

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  // (a)
  testR_KON_41_startSlotuDziennego();
  testR_KON_41_startPrzyciskiem();
  testR_KON_41_panelUspiony();
  testR_KON_41_slotBezNocy();
  testR_KON_42_pelneGodziny();
  testR_KON_52_prognozaPrzyStarcie();
  testR_EKR_60_milczyWSlocieDziennym();
  testR_NF_02_powodySlotuDziennego();
  // (b)
  testR_KON_42_bezOdswiezeniaBezZmiany();
  testR_KON_42_wifiTylkoStatus();
  testR_NF_02_symulacjaDobyZeSlotemDziennym();
  // (c)
  dwaWybudzeniaZWifiWMinucie();
  zanikZasilaniaPrzedStartemSlotu();
  ntpCofaZegarWStarcieSlotu();
  ntpCofaZegarPoStarcieSlotu();
  dryfZegaraDobaZeSlotem();
  airbridgeMilczyWSlocie();
  budzetDobyZeSlotem();
  importPoStarcieSlotuBezNocy();
  e01NocO11();
  pradWracaO14();
  printf("testy_slot_dzienny_w_planie: %d dobrze, %d źle\n", ok, zle);
  return zle ? 1 : 0;
}
