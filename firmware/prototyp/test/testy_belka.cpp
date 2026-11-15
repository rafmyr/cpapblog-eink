// SPDX-License-Identifier: GPL-3.0-or-later
// Testy belki informacyjnej (spec 5.4.17, ADR-0043; decyzja właściciela projektu 14.11.2026).
// Pisane z wymagań R-EKR-63 (geometria i jedno miejsce rysowania), R-EKR-64 (zawartość pól na każdym rodzaju
// ekranu, ekrany bez belki), R-EKR-65 (kolejność ustępowania środka), R-EKR-66 (bateria: ikona, procent, kabel,
// procent nieznany) i R-KON-69 (niska bateria w stopce slotu dziennego). Liczbę odświeżeń e-ink w dobie przy
// zmiennym procencie baterii sprawdza test/testy_zasilanie_rytm.cpp (dyrygent z atrapami).
//
// Uruchomienie: tools/prototyp/testy.sh (program testy_belka).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "dobranoc.h"
#include "dzien.h"
#include "ekrany.h"
#include "ekrany_konfiguracji.h"
#include "ekrany_kontekst.h"
#include "kokpit.h"
#include "kontekst_sk.h"
#include "nawigacja.h"
#include "noc.h"
#include "obrazki.h"
#include "podsumowanie.h"
#include "raport_ekran.h"
#include "slot_dzienny.h"
#include "wspolne.h"

static int ok = 0, zle = 0;
#define SPRAWDZ(w)                                                         \
  do {                                                                     \
    if (w) ok++;                                                           \
    else { zle++; printf("BŁĄD %s:%d: %s\n", __FILE__, __LINE__, #w); }   \
  } while (0)
#define ROWNE(a, b)                                                                                              \
  do {                                                                                                           \
    std::string _a = (a), _b = (b);                                                                              \
    if (_a == _b) ok++;                                                                                          \
    else { zle++; printf("BŁĄD %s:%d: %s = \"%s\", oczekiwane \"%s\"\n", __FILE__, __LINE__, #a, _a.c_str(), _b.c_str()); } \
  } while (0)
#define ROWNE_L(a, b)                                                                                    \
  do {                                                                                                   \
    long long _a = (long long)(a), _b = (long long)(b);                                                  \
    if (_a == _b) ok++;                                                                                  \
    else { zle++; printf("BŁĄD %s:%d: %s = %lld, oczekiwane %lld\n", __FILE__, __LINE__, #a, _a, _b); }  \
  } while (0)

struct PlotnoT : Plotno {
  std::vector<unsigned char> px = std::vector<unsigned char>(800 * 480, 0);
  int poza = 0;
  void piksel(int x, int y, bool c) override {
    if (x < 0 || y < 0 || x >= 800 || y >= 480) { poza++; return; }
    px[(size_t)(y * 800 + x)] = c ? 1 : 0;
  }
  bool cz(int x, int y) const { return px[(size_t)(y * 800 + x)] != 0; }
  int czarne(int x0, int y0, int x1, int y1) const {  // [x0, x1) × [y0, y1)
    int n = 0;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) n += px[(size_t)(y * 800 + x)];
    return n;
  }
  bool wiersz(int y, int x0 = 0, int x1 = 799) const {
    for (int x = x0; x <= x1; x++)
      if (!cz(x, y)) return false;
    return true;
  }
};
// Czy pas belki (wiersze 0…43) jest piksel w piksel taki sam w obu buforach.
static bool tenSamPas(const PlotnoT& a, const PlotnoT& b, int y0 = 0, int y1 = BELKA_WYSOKOSC) {
  for (int y = y0; y < y1; y++)
    for (int x = 0; x < 800; x++)
      if (a.cz(x, y) != b.cz(x, y)) return false;
  return true;
}
// Czy ekran ma belkę z dokładnie tą zawartością: pas 0…43 równy belce narysowanej osobno.
static bool maBelke(const PlotnoT& ekran, const Belka& oczekiwana) {
  PlotnoT wzor;
  belka(wzor, oczekiwana);
  return tenSamPas(ekran, wzor);
}
static BateriaEkranu bat(int procent, bool kabel = false, bool niska = false) {
  BateriaEkranu b;
  b.procent = procent;
  b.kabel = kabel;
  b.niska = niska;
  return b;
}
static Noc noc(const std::string& data, int m01 = 420, float ahi = 1.2f, float wmed = 4) {
  Noc n;
  n.data = data;
  n.wazna = true;
  n.m01 = m01;
  n.ahi = ahi;
  n.wmed = wmed;
  n.wp95 = 8;
  return n;
}

// ---------- R-EKR-63: geometria, jedno miejsce rysowania, belka jasna na każdym tle ----------
// wymaganie: R-EKR-63
static void testR_EKR_63_geometria() {
  ROWNE_L(BELKA_WYSOKOSC, 44);
  ROWNE_L(BELKA_LINIA_Y, 41);
  ROWNE_L(BELKA_LINIA_GRUBOSC, 3);
  ROWNE_L(BELKA_BAZA, 30);
  ustawBaterieEkranu(bat(78));
  PlotnoT p;
  p.prostokat(0, 0, 800, 480, true);  // czarne tło (ekran w negatywie): belka i tak jasna
  const Belka b{"Ostatnia noc 1 / 3", "pt 13.11 · odczyt 07:05"};
  belka(p, b);
  SPRAWDZ(p.wiersz(41) && p.wiersz(42) && p.wiersz(43));         // linia 3 px na 41…43 przez całą szerokość
  SPRAWDZ(!p.cz(5, 40) && !p.cz(400, 40) && !p.cz(795, 40));     // nad linią jasno
  SPRAWDZ(p.cz(400, 44) && p.cz(5, 100));                        // pod belką tło ekranu nietknięte
  SPRAWDZ(p.czarne(0, 0, BELKA_X, 41) == 0 && p.czarne(BELKA_XP + 1, 0, 800, 41) == 0);  // marginesy puste
  SPRAWDZ(p.czarne(0, 0, 800, 8) == 0 && p.czarne(0, 38, 800, 41) == 0);  // tekst między 8 a 37 (wydłużniki)
  // lewe pole od x 28, środek 28 px za nazwą, prawe pole kończy się na 772
  const int xs = xSrodkaBelki(b);
  ROWNE_L(xs, 28 + szer(F_P24B, b.nazwa) + 28);
  SPRAWDZ(p.czarne(28, 8, 28 + szer(F_P24B, b.nazwa) + 1, 38) > 0);
  SPRAWDZ(p.czarne(28 + szer(F_P24B, b.nazwa) + 2, 0, xs - 1, 41) == 0);  // odstęp między nazwą a środkiem
  SPRAWDZ(p.czarne(xs, 8, xs + szer(F_P22B, b.srodek) + 1, 38) > 0);
  const int xp = xPrawegoPolaBelki(b);
  ROWNE_L(xp, 772 - szer(F_P22B, "78%") - BATERIA_ODSTEP - BATERIA_NOSEK_W - BATERIA_W);
  SPRAWDZ(p.czarne(xs + szer(F_P22B, b.srodek) + 2, 0, xp, 41) == 0);  // między środkiem a prawym polem pusto
  SPRAWDZ(p.cz(xp, 12) && p.cz(xp + BATERIA_W - 1, 29) && !p.cz(xp, 11) && !p.cz(xp, 30));  // obrys 34 × 18 na 12…29
  int ostatni = 0;
  for (int x = 0; x < 800; x++)
    if (p.czarne(x, 0, x + 1, 41) > 0) ostatni = x;
  SPRAWDZ(ostatni <= 772 && ostatni >= 768);  // procent dosunięty do prawego marginesu
  ROWNE_L(p.poza, 0);
  // pusta belka (ekran bez nazwy i środka): sama bateria i linia
  PlotnoT q;
  belka(q, Belka{"", ""});
  SPRAWDZ(q.czarne(0, 0, xPrawegoPolaBelki(Belka{"", ""}), 41) == 0 && q.wiersz(42));
  ustawBaterieEkranu(BateriaEkranu());
}

// ---------- R-EKR-64: zawartość pól na każdym rodzaju ekranu ----------
// wymaganie: R-EKR-64
static void testR_EKR_64_zawartoscTresci() {
  // ekran dnia i karta: `Poranek`, data poranka; noc nieubiegła: wiek danych; noc niepewna: zakres oceny
  std::vector<Noc> h = {noc("2026-10-18"), noc("2026-10-19"), noc("2026-10-20")};
  DaneDnia d;
  d.dzis = "2026-10-20";
  d.hist = &h;
  d.godzinaDanych = "07:12";
  Belka b = belkaDnia(d);
  ROWNE(b.nazwa, "Poranek");
  ROWNE(b.srodek, "wt 20.10");  // chwila importu nocy nieznana: sama data, bez słowa `odczyt`; bez czasu z maską i AHI
  d.godzinaImportu = "05:42";   // R-EKR-04: godzina importu nocy (nie ostatniego odczytu `godzinaDanych`)
  ROWNE(belkaDnia(d).srodek, "wt 20.10 · odczyt 05:42");
  std::vector<Noc> hn = h;
  hn.back().wmed = 34;  // noc niepewna (4.9)
  d.hist = &hn;
  ROWNE(belkaDnia(d).srodek, "wt 20.10 · odczyt 05:42 · ocena ograniczona");
  std::vector<Noc> hs(h.begin(), h.end() - 2);  // ostatnia zapisana noc z 18.10 (R-EKR-39)
  d.hist = &hs;
  ROWNE(belkaDnia(d).srodek, "wt 20.10 · dane z 18.10");  // noc nieubiegła: bez godziny dzisiejszego odczytu
  SPRAWDZ(belkaDnia(d).ustepuje.size() == 1 && belkaDnia(d).ustepuje[0] == "20.10 · dane z 18.10");
  std::vector<Noc> pusta;
  d.hist = &pusta;  // brak nocy: sama data
  ROWNE(belkaDnia(d).srodek, "wt 20.10");
  d.hist = nullptr;
  ROWNE(belkaDnia(d).srodek, "wt 20.10");
  DaneDnia bezZegara;  // bez zegara nie ma daty
  ROWNE(belkaDnia(bezZegara).srodek, "bez zegara");
  ROWNE(belkaDnia(bezZegara).nazwa, "Poranek");
  // Portret nocy
  d.hist = &h;
  ROWNE(belkaPortretu(d).nazwa, "Portret nocy");
  ROWNE(belkaPortretu(d).srodek, "noc na wt 20.10 · odczyt 05:42");
  d.hist = &hn;
  ROWNE(belkaPortretu(d).srodek, "noc na wt 20.10 · odczyt 05:42 · ocena ograniczona");
  d.godzinaImportu.clear();
  ROWNE(belkaPortretu(d).srodek, "noc na wt 20.10 · ocena ograniczona");
  // pytania tak / nie: `Pytanie`, data; senność raz w tygodniu z dopiskiem
  d.hist = &h;
  d.godzinaImportu.clear();
  ROWNE(trescPytaniaTN(sprawy::Pytanie::GORZEJ, d, 660, 0).naglowek.nazwa, "Pytanie");
  ROWNE(trescPytaniaTN(sprawy::Pytanie::GORZEJ, d, 660, 0).naglowek.srodek, "wt 20.10");
  ROWNE(trescPytaniaTN(sprawy::Pytanie::SENNOSC, d, 660, 0).naglowek.srodek, "wt 20.10 · tydzień");
  // kokpit: nazwa z numerem strony przy nazwie, strony bez numeru bez numeru
  DaneK1 k1 = budujK1(h, "2026-10-20", Norma(), {});
  ROWNE(belkaK1(k1).nazwa, "Ostatnia noc 1 / 3");
  DaneK2 k2 = budujK2(h, "2026-10-20", Norma(), {});
  ROWNE(belkaK2(k2).nazwa, "7 nocy 2 / 3");
  ROWNE(k2.zakres, "14–20.10");
  k2.godzinaDanych = "05:46";
  ROWNE(belkaK2(k2).srodek, "14–20.10 · odczyt 05:46");
  DaneK2 kp = budujK2({noc("2026-11-03")}, "2026-11-03", Norma(), {});  // przełom miesięcy
  ROWNE(kp.zakres, "28.10–03.11");
  ROWNE(nazwaStronyKokpitu("Ustawienia", numerStronyTekst(Ekran::USTAWIENIA)), "Ustawienia 3 / 3");
  ROWNE(nazwaStronyKokpitu("Otwarte sprawy", numerStronyTekst(Ekran::SPRAWY)), "Otwarte sprawy");
  ROWNE(nazwaStronyKokpitu("Co znaczą te liczby", numerStronyTekst(Ekran::LICZBY)), "Co znaczą te liczby");
  sprawy::WejscieSpraw ws;
  ws.dzis = "2026-11-05";
  DaneSpraw sp = trescSpraw({}, 1, ws);
  ROWNE(sp.naglowek.nazwa, "Otwarte sprawy");
  ROWNE(sp.naglowek.srodek, "cz 05.11");
  ROWNE(podsumowanie::DaneLiczb().naglowek, "Co znaczą te liczby");
}

// wymaganie: R-EKR-64
static void testR_EKR_64_kazdyEkranRysujeBelke() {
  ustawBaterieEkranu(bat(78));
  const Belka dzien{"Poranek", "pt 30.10"};
  {
    TrescEkranu t;
    t.uklad = Uklad::TYDZIEN;
    t.czas = "9 h 09";
    t.ahi = "0,1";
    PlotnoT p;
    rysujEkranDanych(p, dzien, t);
    SPRAWDZ(maBelke(p, dzien) && p.poza == 0);
    t.linia = "Wczoraj duży wyciek: 42 min. Bezdechy mniej pewne.";
    PlotnoT q;
    rysujEkranDanych(q, dzien, t);
    SPRAWDZ(maBelke(q, dzien) && q.czarne(0, 44, 800, 54) == 0);  // linia uwagi zaczyna się pod belką, bez nachodzenia
    TrescEkranu s5;  // S5 brak danych
    s5.ramkaPrzerywana = true;
    s5.tytul = "Brak danych z nocy 30.11";
    PlotnoT r;
    rysujEkranDanych(r, Belka{"Poranek", "cz 30.11 · dane z 29.11"}, s5);
    SPRAWDZ(maBelke(r, Belka{"Poranek", "cz 30.11 · dane z 29.11"}));
  }
  {  // karta z budzikiem: belka z datą i baterią, także w tonie „problem” (belka jasna, etykieta w czarnym polu)
    TrescEkranu k;
    k.etykieta = "OSTATNIA NOC";
    k.tytul = "Noc: 7 h 30 z maską.";
    PlotnoT p, q;
    rysujKarte(p, {"Poranek", "cz 12.11"}, k, "");
    SPRAWDZ(maBelke(p, {"Poranek", "cz 12.11"}));
    k.tonProblem = true;
    rysujKarte(q, {"Poranek", "cz 12.11"}, k, "");
    SPRAWDZ(maBelke(q, {"Poranek", "cz 12.11"}) && !q.cz(400, 5) && !q.cz(400, 46));
  }
  {  // kokpit
    DaneK1 k1;
    k1.ubiegla = true;
    k1.dzienNocy = "pt";
    k1.dataNocy = "13.11";
    k1.godzinaDanych = "07:05";
    PlotnoT p;
    rysujK1(p, k1);
    SPRAWDZ(maBelke(p, Belka{"Ostatnia noc 1 / 3", "pt 13.11 · odczyt 07:05"}));
    k1.odkrycieTytul = "Po alkoholu: wyższe AHI";  // K1 z pasem odkrycia: ta sama belka
    k1.odkryciePowod = "AHI 4,9 / h po alkoholu, 1,8 / h bez.";
    PlotnoT po;
    rysujK1(po, k1);
    SPRAWDZ(maBelke(po, Belka{"Ostatnia noc 1 / 3", "pt 13.11 · odczyt 07:05"}));
    DaneK2 k2;
    k2.zakres = "6–12.11";
    k2.godzinaDanych = "05:46";
    k2.zdanie = "Tydzień w normie.";
    PlotnoT q;
    rysujK2(q, k2);
    SPRAWDZ(maBelke(q, Belka{"7 nocy 2 / 3", "6–12.11 · odczyt 05:46"}));
    SPRAWDZ(q.czarne(0, 44, 800, 48) == 0);  // zdanie K2 nie nachodzi na belkę
    DaneUstawien u;
    PlotnoT r, ra;
    rysujUstawienia(r, u);
    SPRAWDZ(maBelke(r, Belka{"Ustawienia 3 / 3", ""}));
    u.jestAktualizacja = true;  // panel zna nowszą wersję: od 14.11.2026 znak na belce z znakiBelki() (R-EKR-69)
    ZnakiBelki zw;
    zw.nowaWersja = true;
    ustawZnakiBelki(zw);
    rysujUstawienia(ra, u);
    SPRAWDZ(maBelke(ra, Belka{"Ustawienia 3 / 3", ""}));  // bez tekstu `jest aktualizacja ·`
    ustawZnakiBelki(ZnakiBelki());
    SPRAWDZ(!tenSamPas(r, ra));
    DaneSpraw s;
    s.naglowek = {"Otwarte sprawy", "cz 12.11"};
    s.i = s.n = 1;
    PlotnoT sp;
    rysujSprawy(sp, s);
    SPRAWDZ(maBelke(sp, Belka{"Otwarte sprawy", "cz 12.11"}));
    podsumowanie::DaneLiczb l;
    PlotnoT pl;
    podsumowanie::rysujLiczby(pl, Belka{l.naglowek, ""}, l);
    SPRAWDZ(maBelke(pl, Belka{"Co znaczą te liczby", ""}));
  }
  {  // pytanie poranne (siatka „Co było inaczej?”) i pytanie tak / nie
    DanePytania q;
    q.dataPoranka = "2026-11-30";
    PlotnoT p;
    rysujPytanie(p, q);
    SPRAWDZ(maBelke(p, Belka{"Pytanie", "pn 30.11"}) && p.czarne(0, 44, 800, 58) == 0);
    DanePytaniaTN tn;
    tn.naglowek = {"Pytanie", "pn 30.11"};
    PlotnoT r;
    rysujPytanieTN(r, tn);
    SPRAWDZ(maBelke(r, Belka{"Pytanie", "pn 30.11"}));
  }
  {  // ekrany w negatywie: wzorzec L (ton „lekarz” z ramką, ton „problem”), S-K w tonie „problem”
    DaneWzorca w;
    w.naglowek = dzien;
    w.tytul = "Saturacja: sprawa dla lekarza";
    PlotnoT p, q;
    rysujWzorzec(p, w);
    SPRAWDZ(maBelke(p, dzien) && p.cz(2, 46) && !p.cz(6, 50) && p.cz(20, 300));  // ramka zaczyna się pod belką
    w.lekarz = false;
    rysujWzorzec(q, w);
    SPRAWDZ(maBelke(q, dzien) && q.cz(5, 50));
    sk::Tresc t;
    t.problem = true;
    PlotnoT r;
    sk::rysuj(r, dzien, t, "", "");
    SPRAWDZ(maBelke(r, dzien) && r.cz(5, 50) && r.cz(400, 300));
    TrescKontekstu k;
    PlotnoT s;
    const Belka wiecz{"Wieczór", "śr 21.10 · sypialnia 21:30 · CO2 1240"};
    rysujEkranKontekstu(s, wiecz, k, "", "");
    SPRAWDZ(maBelke(s, wiecz));
  }
  {  // potwierdzenia, raport, ekrany serwisowe i formularzowe
    DanePotwierdzenia z;
    z.naglowek = dzien;
    PlotnoT p;
    rysujPotwierdzenie(p, z);
    SPRAWDZ(maBelke(p, dzien));
    DaneOmowione o;
    o.naglowek = {"Po wizycie", "cz 12.11 · przytrzymano ◀ 2 s"};
    PlotnoT q;
    rysujOmowione(q, o);
    SPRAWDZ(maBelke(q, o.naglowek));
    DaneRaportu rp;
    rp.naglowek = {"Raport", "cz 12.11"};
    PlotnoT r;
    rysujRaport(r, rp);
    SPRAWDZ(maBelke(r, rp.naglowek));
    PlotnoT rb;
    rysujRaportBezWifi(rb, {"Raport", "cz 12.11"}, "Bezdechy centralne utrwalone", "");
    SPRAWDZ(maBelke(rb, Belka{"Raport", "cz 12.11"}));
    DaneTestu t;
    PlotnoT u5;
    rysujTest(u5, t);
    SPRAWDZ(maBelke(u5, Belka{"Test sprzętu", ""}));  // bez zegara: środek pusty
    t.sprawdzono = "14.11 14:06";
    PlotnoT u5b;
    rysujTest(u5b, t);
    SPRAWDZ(maBelke(u5b, Belka{"Test sprzętu", "sprawdzono 14.11 14:06"}));
    PlotnoT u7;
    aktualizacja::DaneU07 d7;
    d7.stan = aktualizacja::StanU07::NOWSZA;
    d7.wlasna = "1.0.0";
    d7.nowa = "1.1.0";
    rysujAktualizacje(u7, aktualizacja::trescU07(d7));
    SPRAWDZ(maBelke(u7, Belka{"Aktualizacja", "jest nowa wersja"}));  // stan w środku belki (R-KONF-57)
    DaneFormularzaQr f;
    f.bezWifi = true;
    PlotnoT u3;
    rysujFormularzQr(u3, f);
    SPRAWDZ(maBelke(u3, Belka{"Ustawienia panelu", ""}));
    DaneSieciPanelu sp;
    PlotnoT u2;
    rysujSiecPanelu(u2, sp);
    SPRAWDZ(maBelke(u2, Belka{"Sieć panelu", ""}));
  }
  ustawBaterieEkranu(BateriaEkranu());
}

// Slot dzienny (ekrany S-K i obrazek dnia) oraz ekran nocny S9 nie mają belki i nie zależą od procentu baterii.
// wymaganie: R-EKR-64
static void testR_EKR_64_ekranyBezBelki() {
  auto bezLiniiBelki = [](const PlotnoT& p) { return !p.wiersz(41) && !p.wiersz(42) && !p.wiersz(43); };
  ObrazPbm o;
  o.w = 430;
  o.h = 430;
  o.bity.assign((size_t)((o.w + 7) / 8) * (size_t)o.h, 0);
  Noc n = noc("2026-11-03", 472, 1.2f, 4);
  const StopkaDzienna st = stopkaDzienna(&n, true, 0, 6, 21);
  WejscieSlotu we;
  we.dzis = "2026-11-03";
  we.minuta = 12 * 60;
  we.przyLozku = 21;
  const WyborSlotu w = wybierzSlot(we);
  const TekstySlotu ts = tekstySlotu(w, we);
  DaneNocy dn[3];
  dn[0].wybor.wariant = WariantNocy::A1;
  dn[1].wybor.wariant = WariantNocy::A2;
  dn[2].wybor.wariant = WariantNocy::A3;
  for (auto& x : dn) {
    x.wieczor = "2026-11-03";
    x.maskaOd = 22 * 60 + 47;
    x.temp = 21;
    x.wilg = 45;
  }
  PlotnoT obraz[2], slot[2], nocny[2][3];
  int i = 0;
  for (int proc : {78, 21}) {  // inny procent, ten sam stan (nie „niska”): klatki identyczne
    ustawBaterieEkranu(bat(proc));
    rysujObrazekDnia(obraz[i], o, st);
    rysujSlotDzienny(slot[i], NaglowekSlotu{"wt 03.11", ""}, w, ts, we, st);
    for (int v = 0; v < 3; v++) rysujDobranoc(nocny[i][v], dn[v]);
    i++;
  }
  SPRAWDZ(obraz[0].czarne(0, 0, 800, 44) == 0);  // obrazek dnia: nic u góry
  SPRAWDZ(bezLiniiBelki(slot[0]) && slot[0].wiersz(52) && slot[0].wiersz(53));  // nagłówek slotu jak dotąd (linia na 52)
  for (int v = 0; v < 3; v++) SPRAWDZ(bezLiniiBelki(nocny[0][v]));
  SPRAWDZ(obraz[0].px == obraz[1].px && slot[0].px == slot[1].px);
  for (int v = 0; v < 3; v++) SPRAWDZ(nocny[0][v].px == nocny[1][v].px);
  // S9 nie zmienia się także przy stanie „niska bateria” (ostrzeżenie E-05 bez zmian) ani na kablu
  PlotnoT niska, kabel;
  ustawBaterieEkranu(bat(15, false, true));
  rysujDobranoc(niska, dn[1]);
  ustawBaterieEkranu(bat(100, true));
  rysujDobranoc(kabel, dn[1]);
  SPRAWDZ(niska.px == nocny[0][1].px && kabel.px == nocny[0][1].px);
  ustawBaterieEkranu(BateriaEkranu());
}

// ---------- R-EKR-65: środek ustępuje w ustalonej kolejności, `ocena …` nigdy ----------
// wymaganie: R-EKR-65
static void testR_EKR_65_ustepowanieSrodka() {
  DaneK1 k;
  k.ubiegla = true;
  k.dzienNocy = "śr";
  k.dataNocy = "30.12";
  k.godzinaDanych = "07:12";
  k.ocena = "ocena ograniczona";
  ustawBaterieEkranu(bat(78));
  Belka b = belkaK1(k);
  ROWNE(b.srodek, "śr 30.12 · odczyt 07:12 · ocena ograniczona");
  SPRAWDZ(b.ustepuje.size() == 4);
  ROWNE(b.ustepuje[0], "30.12 · odczyt 07:12 · ocena ograniczona");  // 1. bez dnia tygodnia
  ROWNE(b.ustepuje[1], "odczyt 07:12 · ocena ograniczona");          // 2. bez daty (wynika z nazwy `Ostatnia noc`)
  ROWNE(b.ustepuje[2], "30.12 · ocena ograniczona");                   // 3. dopiero teraz bez godziny
  ROWNE(b.ustepuje[3], "ocena ograniczona");                           // 4. sama ocena
  // przy `78%` na środek jest 410 px: `30.12 · odczyt 07:12 · ocena ograniczona` ma 412 px, więc ustępuje data,
  // a godzina zostaje; z węższymi cyframi (`12.11`, `07:05`, 408 px) zostaje i data
  ROWNE_L(miejsceSrodkaBelki(b), 410);
  ROWNE(srodekBelki(b), "odczyt 07:12 · ocena ograniczona");
  {
    DaneK1 w = k;
    w.dzienNocy = "cz";
    w.dataNocy = "12.11";
    w.godzinaDanych = "07:05";
    ROWNE(srodekBelki(belkaK1(w)), "12.11 · odczyt 07:05 · ocena ograniczona");  // ustąpił tylko dzień tygodnia
  }
  k.romb = true;  // z rombem R-KOK-14 ustępuje data, godzina i romb zostają
  b = belkaK1(k);
  ROWNE(b.srodek, "śr 30.12 · odczyt 07:12 ◆ · ocena ograniczona");
  ROWNE(srodekBelki(b), "odczyt 07:12 ◆ · ocena ograniczona");
  k.ocena = "ocena częściowa";
  k.romb = false;
  ROWNE(srodekBelki(belkaK1(k)), "30.12 · odczyt 07:12 · ocena częściowa");
  k.ocena.clear();  // ocena pełna: nic nie ustępuje
  ROWNE(srodekBelki(belkaK1(k)), "śr 30.12 · odczyt 07:12");
  // noc nieubiegła (R-KOK-04): `noc {data}`, bez godziny dzisiejszego odczytu; data nie ustępuje (brak listy)
  DaneK1 s;
  s.dataNocy = "18.10";
  s.godzinaDanych = "07:12";
  s.ocena = "ocena częściowa";
  s.romb = true;
  ROWNE(srodekBelki(belkaK1(s)), "noc 18.10 ◆ · ocena częściowa");
  SPRAWDZ(belkaK1(s).ustepuje.empty());
  // brak nocy: sama ocena
  DaneK1 e;
  e.ocena = "ocena częściowa";
  ROWNE(srodekBelki(belkaK1(e)), "ocena częściowa");

  // Godzina odczytu jest zawsze, gdy noc jest ubiegła i chwila importu znana.
  // (a) tekst bez daty, do którego belka może ustąpić, mieści się dla każdej minuty doby × oceny × rombu × stanu
  //     baterii na każdym z trzech ekranów (K1, ekran dnia, Portret)
  const BateriaEkranu stany[] = {bat(5), bat(78), bat(100), bat(100, true), bat(-1)};
  const char* oceny[] = {"", "ocena częściowa", "ocena ograniczona"};
  int minBezDaty = 0, zleBezDaty = 0;
  for (const BateriaEkranu& stan : stany) {
    ustawBaterieEkranu(stan);
    for (int m = 0; m < 1440; m++) {
      char g[8];
      snprintf(g, sizeof g, "%02d:%02d", m / 60, m % 60);
      for (const char* ocena : oceny)
        for (bool romb : {false, true}) {
          SrodekNocy sn;
          sn.dzien = "śr";
          sn.data = "30.12";
          sn.odczyt = g;
          sn.romb = romb;
          sn.dopisek = ocena;
          for (const char* nazwa : {"Ostatnia noc 1 / 3", "Poranek", "Portret nocy"}) {
            if (romb && std::string(nazwa) != "Ostatnia noc 1 / 3") continue;  // romb tylko w K1
            Belka bx(nazwa, "");
            sn.przed = std::string(nazwa) == "Portret nocy" ? "noc na " : "";
            ustawSrodekNocy(bx, sn);
            const std::string sr = srodekBelki(bx);
            minBezDaty++;
            if (sr.find(std::string("odczyt ") + g) == std::string::npos) zleBezDaty++;
          }
        }
    }
  }
  ROWNE_L(zleBezDaty, 0);
  // (b) wszystkie daty roku (z prawdziwym dniem tygodnia) × godziny co 30 min i z najszerszymi cyframi × ocena × romb
  //     × stan baterii w K1: godzina obecna w 100% przypadków, ocena i romb też; licznik, który tekst wybrano
  long razem = 0, zGodzina = 0, pelny = 0, bezDnia = 0, bezDaty = 0, bezGodziny = 0;
  std::vector<std::string> godziny;
  for (int m = 0; m < 1440; m += 30) {
    char g[8];
    snprintf(g, sizeof g, "%02d:%02d", m / 60, m % 60);
    godziny.push_back(g);
  }
  for (const char* g : {"08:08", "05:55", "00:00", "20:08", "23:59"}) godziny.push_back(g);
  for (const BateriaEkranu& stan : stany) {
    ustawBaterieEkranu(stan);
    std::string data = "2026-01-01";
    for (int dzienRoku = 0; dzienRoku < 365; dzienRoku++, data = dodajDni(data, 1))
      for (const std::string& godz : godziny)
        for (const char* ocena : oceny)
          for (bool romb : {false, true}) {
            DaneK1 x;
            x.ubiegla = true;
            x.dzienNocy = DNI_KROTKO[dzienTygodnia(data)];
            x.dataNocy = ddmm(data);
            x.godzinaDanych = godz;
            x.ocena = ocena;
            x.romb = romb;
            const Belka bx = belkaK1(x);
            const std::string sr = srodekBelki(bx);
            const std::string oc = ocena;
            razem++;
            const bool jestGodzina = sr.find("odczyt " + godz) != std::string::npos;
            zGodzina += jestGodzina;
            if (sr == bx.srodek) pelny++;
            else if (sr == bx.ustepuje[0]) bezDnia++;
            else if (jestGodzina) bezDaty++;
            else bezGodziny++;
            bool dobrze = jestGodzina && sr.size() >= oc.size() &&
                          sr.compare(sr.size() - oc.size(), oc.size(), oc) == 0 &&
                          (!romb || sr.find("◆") != std::string::npos) &&
                          xSrodkaBelki(bx) + szer(F_P22B, sr) + BELKA_ODSTEP_PRAWEGO <= xPrawegoPolaBelki(bx);
            if (!dobrze && razem - zGodzina < 5) printf("  bez godziny albo za szeroko: „%s” przy „%s”\n", sr.c_str(), tekstBaterii(stan).c_str());
            if (!dobrze) zle++;
          }
  }
  ok++;
  printf("  K1, godzina odczytu w belce: %ld z %ld przypadków (%.1f%%); pełny środek %ld, bez dnia tygodnia %ld, bez daty %ld, "
         "bez godziny %ld; tekst bez daty dla każdej minuty doby: %d sprawdzeń, %d bez godziny\n",
         zGodzina, razem, 100.0 * (double)zGodzina / (double)razem, pelny, bezDnia, bezDaty, bezGodziny, minBezDaty, zleBezDaty);
  ROWNE_L(zGodzina, razem);
  ROWNE_L(bezGodziny, 0);
  // ekran dnia i Portret: najdłuższy środek z godziną i oceną przy każdym stanie baterii zachowuje godzinę i ocenę
  for (const BateriaEkranu& stan : stany) {
    ustawBaterieEkranu(stan);
    std::vector<Noc> h = {noc("2026-12-30", 420, 1.2f, 34)};  // noc niepewna
    DaneDnia d;
    d.dzis = "2026-12-30";
    d.hist = &h;
    d.godzinaImportu = "08:08";
    for (const Belka& bx : {belkaDnia(d), belkaPortretu(d), belkaDnia(d, BELKA_PYTANIE), belkaDnia(d, BELKA_WIECZOR)}) {
      const std::string sr = srodekBelki(bx);
      SPRAWDZ(sr.find("odczyt 08:08") != std::string::npos && sr.find("ocena ograniczona") != std::string::npos &&
              szer(F_P22B, sr) <= miejsceSrodkaBelki(bx));
    }
    SPRAWDZ(srodekBelki(belkaDnia(d)) == "śr 30.12 · odczyt 08:08 · ocena ograniczona");  // ekran dnia: nic nie ustępuje
  }
  // najdłuższe środki pozostałych ekranów mieszczą się w całości przy `100%` (nic nie ustępuje)
  ustawBaterieEkranu(bat(100));
  struct { const char* nazwa; const char* srodek; const char* stan; } pelne[] = {
      {"Poranek", "śr 30.12 · odczyt 08:08 · ocena ograniczona", ""},
      {"Poranek", "śr 30.12 · dane z 28.08", ""},
      {"7 nocy 2 / 3", "28.08–03.09 · odczyt 08:08", ""},
      {"Otwarte sprawy", "śr 30.12", ""},
      {"Pytanie", "śr 30.12 · tydzień", ""},
      {"Wieczór", "śr 30.12 · sypialnia 20:08 · CO2 5000", ""},
      {"Raport", "śr 30.12 · raport gotowy · Wi-Fi włączone", ""},
      {"Po wizycie", "śr 30.12 · przytrzymano ◀ 2 s", ""},
      {"Test sprzętu", "Ustawienia → Test sprzętu", ""},
      {"Ustawienia panelu", "Wi-Fi włączone", ""},
      {"Sieć panelu", "Wi-Fi panelu włączone", ""},
      {"Ustawienia 3 / 3", "", ""},
      {"Co znaczą te liczby", "", ""},
  };
  for (auto& x : pelne) {
    const Belka bx{x.nazwa, x.srodek, x.stan};
    bool dobrze = srodekBelki(bx) == x.srodek && miejsceSrodkaBelki(bx) >= 0;
    if (!dobrze) printf("  nie mieści się: %s / %s\n", x.nazwa, x.srodek);
    SPRAWDZ(dobrze);
  }
  // tekst środka dłuższy niż miejsce i bez listy ustępowania: skrócony do pełnych słów, nie wchodzi na prawe pole
  const Belka dluga{"Ostatnia noc 1 / 3", "bardzo długi tekst środka belki, który nie mieści się przed baterią wcale"};
  const std::string skr = srodekBelki(dluga);
  SPRAWDZ(!skr.empty() && skr.size() < dluga.srodek.size() && szer(F_P22B, skr) <= miejsceSrodkaBelki(dluga));
  PlotnoT p;
  belka(p, dluga);
  SPRAWDZ(p.czarne(xPrawegoPolaBelki(dluga) - BELKA_ODSTEP_PRAWEGO, 0, xPrawegoPolaBelki(dluga), 41) == 0);
  ustawBaterieEkranu(BateriaEkranu());
}

// ---------- R-EKR-66: ikona baterii, procent, kabel, procent nieznany ----------
// wymaganie: R-EKR-66
static void testR_EKR_66_ikonaBaterii() {
  // stopnie wypełnienia: ≥ 88% 4, ≥ 63% 3, ≥ 38% 2, ≥ 13% 1, niżej 0
  const int progi[][2] = {{100, 4}, {88, 4}, {87, 3}, {63, 3}, {62, 2}, {38, 2}, {37, 1}, {13, 1}, {12, 0}, {0, 0}, {-1, 0}};
  for (auto& x : progi) ROWNE_L(polaBaterii(x[0]), x[1]);
  ROWNE(tekstBaterii(bat(78)), "78%");
  ROWNE(tekstBaterii(bat(0)), "0%");
  ROWNE(tekstBaterii(bat(100)), "100%");
  ROWNE(tekstBaterii(bat(-1)), "–");           // procent nieznany
  ROWNE(tekstBaterii(bat(55, true)), "kabel");  // zasilanie zewnętrzne: słowo zamiast procentu
  ROWNE(tekstBaterii(bat(-1, true)), "kabel");
  // piksele ikony: obrys 34 × 18 kreską 2 px, nosek 3 × 8 z prawej, pola 5 × 10
  int czarnePol[5];
  for (int pola = 0; pola <= 4; pola++) {
    PlotnoT p;
    int xk = ikonaBaterii(p, 100, 50, pola, false);
    ROWNE_L(xk, 100 + 34 + 3);
    SPRAWDZ(p.wiersz(50, 100, 133) && p.wiersz(51, 100, 133) && p.wiersz(66, 100, 133) && p.wiersz(67, 100, 133));
    for (int y = 50; y <= 67; y++) SPRAWDZ(p.cz(100, y) && p.cz(101, y) && p.cz(132, y) && p.cz(133, y));
    SPRAWDZ(p.czarne(134, 50, 137, 68) == 3 * 8 && p.cz(134, 55) && p.cz(136, 62) && !p.cz(134, 54) && !p.cz(134, 63));
    SPRAWDZ(p.czarne(0, 0, 800, 50) == 0 && p.czarne(0, 68, 800, 480) == 0 && p.czarne(0, 50, 100, 68) == 0 &&
            p.czarne(137, 50, 800, 68) == 0);  // nic poza ikoną
    czarnePol[pola] = p.czarne(102, 52, 132, 66);  // wnętrze obrysu
    ROWNE_L(czarnePol[pola], pola * 5 * 10);
    for (int i = 0; i < 4; i++) SPRAWDZ(p.cz(104 + 7 * i + 2, 59) == (i < pola));  // pola od lewej
  }
  {  // na kablu: bez pól, pionowa kreska w środku
    PlotnoT p;
    ikonaBaterii(p, 100, 50, 4, true);
    SPRAWDZ(p.czarne(102, 52, 132, 66) == 2 * 10 && p.cz(116, 59) && !p.cz(106, 59) && !p.cz(127, 59));
  }
  // w belce: ikona i tekst według stanu; prawe pole kończy się na 772 przy każdym stanie
  for (const BateriaEkranu& stan : {bat(100), bat(78), bat(40), bat(15), bat(5), bat(-1), bat(60, true)}) {
    ustawBaterieEkranu(stan);
    PlotnoT p;
    const Belka b{"Poranek", "pt 30.10"};
    belka(p, b);
    const int xi = xPrawegoPolaBelki(b);
    ROWNE_L(xi, 772 - szer(F_P22B, tekstBaterii(stan)) - BATERIA_ODSTEP - BATERIA_NOSEK_W - BATERIA_W);
    SPRAWDZ(p.cz(xi, 12) && p.cz(xi + 33, 29));
    ROWNE_L(p.czarne(xi + 2, 14, xi + 32, 28), stan.kabel ? 20 : polaBaterii(stan.procent) * 50);
    SPRAWDZ(p.czarne(xi + 37 + BATERIA_ODSTEP, 8, 773, 38) > 0 && p.czarne(773, 0, 800, 41) == 0);
    SPRAWDZ(p.czarne(xi + 37, 0, xi + 37 + BATERIA_ODSTEP - 1, 41) == 0);  // odstęp między ikoną a tekstem
  }
  // tekst stanu przed ikoną z ` ·` (U-01 przy znanej nowszej wersji), 8 px przed ikoną
  ustawBaterieEkranu(bat(78));
  const Belka u{"Ustawienia 3 / 3", "", "jest aktualizacja"};
  const Belka bez{"Ustawienia 3 / 3", ""};
  ROWNE_L(xPrawegoPolaBelki(bez) - xPrawegoPolaBelki(u), szer(F_P22B, "jest aktualizacja ·") + BELKA_ODSTEP_STANU);
  PlotnoT p;
  belka(p, u);
  SPRAWDZ(p.czarne(xPrawegoPolaBelki(u), 8, xPrawegoPolaBelki(bez) - BELKA_ODSTEP_STANU, 38) > 0);
  SPRAWDZ(p.cz(xPrawegoPolaBelki(bez), 12));  // ikona na swoim stałym miejscu
  ustawBaterieEkranu(BateriaEkranu());
  // bez ustawienia stanu (rysowanie poza dyrygentem): procent nieznany
  ROWNE(tekstBaterii(bateriaEkranu()), "–");
}

// ---------- R-KON-69: niska bateria w stopce slotu dziennego ----------
// wymaganie: R-KON-69
static void testR_KON_69_niskaBateriaWStopceDnia() {
  ROWNE_L(BATERIA_NISKA, 20);
  // histereza: wchodzi poniżej 20%, wychodzi od 25% albo na kablu
  SPRAWDZ(!bateriaNiska(20, false, false) && bateriaNiska(19, false, false) && bateriaNiska(0, false, false));
  SPRAWDZ(bateriaNiska(24, false, true) && !bateriaNiska(25, false, true) && !bateriaNiska(24, false, false));
  SPRAWDZ(!bateriaNiska(5, true, true) && !bateriaNiska(5, true, false));
  // procent nieznany („nie wiem”): stan się nie zmienia, ani w jedną, ani w drugą stronę
  SPRAWDZ(bateriaNiska(-1, false, true) && !bateriaNiska(-1, false, false));
  ROWNE(tekstNiskiejBaterii(bat(15)), "15% · podłącz kabel");
  ObrazPbm o;
  o.w = 430;
  o.h = 430;
  o.bity.assign((size_t)((o.w + 7) / 8) * (size_t)o.h, 0);
  Noc n = noc("2026-11-03", 472, 1.2f, 4);
  StopkaDzienna st = stopkaDzienna(&n, true, 0, 6, 21);
  ROWNE(st.lewo, "noc 7 h 52 · AHI 1,2 · wyciek 8");
  SPRAWDZ(!st.bateriaNiska);
  ustawBaterieEkranu(bat(15, false, true));
  PlotnoT zwykla, niska;
  rysujObrazekDnia(zwykla, o, st);  // stan baterii bez flagi w stopce: stopka jak dotąd
  st.bateriaNiska = true;
  rysujObrazekDnia(niska, o, st);
  // ikona baterii od lewego marginesu na linii stopki (obrys 447…464), za nią tekst; wynik nocy ustąpił
  SPRAWDZ(niska.cz(28, 447) && niska.cz(28 + 33, 464) && !zwykla.cz(28 + 33, 447));
  ROWNE_L(niska.czarne(30, 449, 60, 463), 50);  // 15%: jedno pole
  const int xt = 28 + 34 + 3 + BATERIA_ODSTEP, wt = szer(F_P22B, "15% · podłącz kabel");
  SPRAWDZ(niska.czarne(xt, 440, xt + wt + 1, 472) > 0 && niska.czarne(xt + wt + 2, 438, 480, 480) == 0);
  // prawa część stopki, linia stopki i obraz bez zmian; belki nadal nie ma
  SPRAWDZ(tenSamPas(zwykla, niska, 0, 438));
  bool prawoTeSame = true;
  for (int y = 438; y < 480; y++)
    for (int x = 480; x < 800; x++) prawoTeSame = prawoTeSame && zwykla.cz(x, y) == niska.cz(x, y);
  SPRAWDZ(prawoTeSame && niska.czarne(480, 440, 800, 472) > 0);
  // to samo na ekranie S-K slotu dziennego
  WejscieSlotu we;
  we.dzis = "2026-11-03";
  we.minuta = 12 * 60;
  we.przyLozku = 21;
  const WyborSlotu w = wybierzSlot(we);
  PlotnoT sk;
  rysujSlotDzienny(sk, NaglowekSlotu{"wt 03.11", ""}, w, tekstySlotu(w, we), we, st);
  SPRAWDZ(sk.cz(28, 447) && sk.czarne(30, 449, 60, 463) == 50 && sk.wiersz(52) && !sk.wiersz(42));
  // najdłuższy tekst (`100%` nie wystąpi poniżej progu; `24%` przy histerezie) mieści się obok najdłuższej prawej części
  SPRAWDZ(28 + 37 + BATERIA_ODSTEP + szer(F_P22B, "24% · podłącz kabel") + 26 +
              szer(F_P22B, "za oknem −12° · przy łóżku 35°") <= 772);
  ustawBaterieEkranu(BateriaEkranu());
}

// ---------- znaki stanu danych na belce (R-EKR-67…R-EKR-69) ----------
static std::vector<Noc> nocDo(const std::string& ostatnia, int n = 3) {
  std::vector<Noc> h;
  for (int i = n - 1; i >= 0; i--) {
    Noc x;
    x.data = dodajDni(ostatnia, -i);
    x.wazna = true;
    x.m01 = 420;
    x.ahi = 2.0f;
    h.push_back(x);
  }
  return h;
}
// wysokość wiersza czarnych pikseli rysunku (pierwszy i ostatni wiersz z czarnym pikselem w pasie x0…x1)
static int wysokoscRysunku(const PlotnoT& p, int x0, int x1, int y0 = 0, int y1 = 41) {
  int g = -1, d = -1;
  for (int y = y0; y < y1; y++)
    if (p.czarne(x0, y, x1, y + 1)) {
      if (g < 0) g = y;
      d = y;
    }
  return g < 0 ? 0 : d - g + 1;
}
// czy środek (tekst, pole alarmu, część po polu, klepsydra) kończy się co najmniej 16 px przed prawym polem
static bool srodekMiesciSie(const Belka& b) {
  UkladSrodka u = ukladSrodkaBelki(b);
  int w = szer(F_P22B, u.tekst);
  bool cos = !u.tekst.empty();
  if (!b.alarm.empty()) { w += (cos ? BELKA_ODSTEP_POLA : 0) + szerPolaAlarmu(b.alarm); cos = true; }
  if (!b.poPolu.empty()) { w += (cos ? BELKA_ODSTEP_POLA : 0) + szer(F_P22B, b.poPolu); cos = true; }
  if (u.klepsydra) w += (cos ? BELKA_ODSTEP_POLA : 0) + szerKlepsydry(u.slowo);
  return w <= miejsceSrodkaBelki(b);
}

static void testR_EKR_67_definicjaNieaktualnych() {
  const std::string d = "2026-11-14";
  // noc z dziś w historii: nigdy nieaktualne
  SPRAWDZ(!daneNieaktualne(nocDo(d), d, 15 * 60, "2026-11-14", 6 * 60));
  // przed 11:00 oczekiwana noc z wczoraj: noc z 13.11 jest, nie ma jeszcze nocy z 14.11 → aktualne (okno trwa)
  SPRAWDZ(!daneNieaktualne(nocDo("2026-11-13"), d, 10 * 60 + 59, "2026-11-14", 6 * 60));
  ROWNE(nocOczekiwana(d, 10 * 60 + 59), "2026-11-13");
  ROWNE(nocOczekiwana(d, 11 * 60), "2026-11-14");
  ROWNE(nocOczekiwana("", 12 * 60), "");
  // od 11:00 bez nocy z dziś, ostatni kontakt z AirBridge 06:00 (potem milczy, E-01) → nieaktualne
  SPRAWDZ(daneNieaktualne(nocDo("2026-11-13"), d, 11 * 60, "2026-11-14", 6 * 60));
  SPRAWDZ(daneNieaktualne(nocDo("2026-11-13"), d, 23 * 60 + 59, "2026-11-14", 10 * 60 + 59));
  // AirBridge odpowiedział o 11:00 albo później (noc bez terapii E-10, R-DANE-20, albo w budowie): stan nocy znany
  SPRAWDZ(!daneNieaktualne(nocDo("2026-11-13"), d, 12 * 60, "2026-11-14", 11 * 60));
  SPRAWDZ(!daneNieaktualne(nocDo("2026-11-13"), d, 23 * 60, "2026-11-14", 14 * 60 + 30));
  // następnego ranka przed 11:00: noc z wczoraj była bez terapii (kontakt wczoraj 11:00) → aktualne
  SPRAWDZ(!daneNieaktualne(nocDo("2026-11-13"), "2026-11-15", 7 * 60, "2026-11-14", 11 * 60));
  // AirBridge milczy od 14.11 06:00: 15.11 07:00 noc z 14.11 brakuje po zamknięciu jej okna → nieaktualne
  SPRAWDZ(daneNieaktualne(nocDo("2026-11-13"), "2026-11-15", 7 * 60, "2026-11-14", 6 * 60));
  // kilka dni bez kontaktu
  SPRAWDZ(daneNieaktualne(nocDo("2026-11-10"), d, 9 * 60, "2026-11-10", 7 * 60));
  // bez historii i bez kontaktu po 11:00 → nieaktualne (`brak danych`); bez historii, kontakt po 11:00 → nie
  SPRAWDZ(daneNieaktualne({}, d, 12 * 60, "", -1));
  SPRAWDZ(!daneNieaktualne({}, d, 12 * 60, "2026-11-14", 11 * 60 + 30));
  // bez zegara: nigdy (belka mówi `bez zegara`)
  SPRAWDZ(!daneNieaktualne({}, "", 12 * 60, "", -1));
  // noc z przyszłości nie liczy się (ostatniaNoc ≤ dziś), zła data kontaktu = brak kontaktu
  SPRAWDZ(daneNieaktualne(nocDo("2026-11-20"), d, 12 * 60, "zła", 12 * 60) ==
          daneNieaktualne(nocDo("2026-11-13"), d, 12 * 60, "", -1));
  ZnakiBelki z;
  ROWNE(tekstNieaktualnych(z), "brak danych");
  z.daneZ = "13.11";
  ROWNE(tekstNieaktualnych(z), "dane z 13.11");
}

static void testR_EKR_67_wygladNieaktualnych() {
  ustawBaterieEkranu(bat(78));
  const std::string d = "2026-11-14";  // sobota
  std::vector<Noc> h = nocDo("2026-11-13");
  DaneDnia dd;
  dd.dzis = d;
  dd.hist = &h;
  // dane aktualne (przed 11:00 albo noc bez terapii): belka jak przed 14.11.2026, `dane z` bez pola
  ustawZnakiBelki(ZnakiBelki());
  Belka swieza = belkaDnia(dd);
  ROWNE(swieza.srodek, "sb 14.11 · dane z 13.11");
  SPRAWDZ(swieza.alarm.empty() && swieza.poPolu.empty());
  PlotnoT ps;
  belka(ps, swieza);
  SPRAWDZ(ps.czarne(0, BELKA_POLE_Y, 800, BELKA_POLE_Y + 3) == 0);  // nigdzie odwróconego pola
  // nieaktualne: `sb 14.11` i pole `! dane z 13.11`
  ZnakiBelki z;
  z.nieaktualne = true;
  z.daneZ = "13.11";
  ustawZnakiBelki(z);
  Belka b = belkaDnia(dd);
  ROWNE(b.srodek, "sb 14.11");
  ROWNE(b.alarm, "dane z 13.11");
  ROWNE(b.poPolu, "");
  PlotnoT p;
  belka(p, b);
  const int xa = xSrodkaBelki(b) + szer(F_P22B, "sb 14.11") + BELKA_ODSTEP_POLA, wa = szerPolaAlarmu(b.alarm);
  // pole czarne od y 5 do 36 na całej szerokości (lewa i prawa krawędź), biały tekst w środku, 4 px jasne nad linią
  SPRAWDZ(p.czarne(xa, BELKA_POLE_Y, xa + 1, BELKA_POLE_Y + BELKA_POLE_H) == BELKA_POLE_H);
  SPRAWDZ(p.czarne(xa + wa - 1, BELKA_POLE_Y, xa + wa, BELKA_POLE_Y + BELKA_POLE_H) == BELKA_POLE_H);
  SPRAWDZ(p.czarne(xa, BELKA_POLE_Y + BELKA_POLE_H, xa + wa, BELKA_LINIA_Y) == 0);
  SPRAWDZ(p.czarne(xa, 0, xa + wa, BELKA_POLE_Y) == 0);
  const int pole = wa * BELKA_POLE_H, czarnych = p.czarne(xa, BELKA_POLE_Y, xa + wa, BELKA_POLE_Y + BELKA_POLE_H);
  SPRAWDZ(czarnych < pole && czarnych > pole * 7 / 10);  // tekst biały na czarnym
  SPRAWDZ(!tenSamPas(p, ps));  // ADR-0029 pkt 7: stan „nie wiem” nigdy nie wygląda jak dane świeże
  // przy nocy niepewnej z ubiegłej nocy nie ma alarmu (noc jest), a dopisek z oceną zostaje jak dotąd
  // Portret: samo pole, bez `noc na sb 14.11` (data poranka obok `dane z` innej nocy myliłaby)
  Belka bp = belkaPortretu(dd);
  ROWNE(bp.srodek, "");
  SPRAWDZ(srodekBelki(bp).empty());
  ROWNE(bp.alarm, "dane z 13.11");
  // bez historii: `! brak danych`
  ZnakiBelki zb;
  zb.nieaktualne = true;
  ustawZnakiBelki(zb);
  DaneDnia pusty;
  std::vector<Noc> brak;
  pusty.dzis = d;
  pusty.hist = &brak;
  Belka b0 = belkaDnia(pusty);
  ROWNE(b0.srodek, "sb 14.11");
  ROWNE(b0.alarm, "brak danych");
  // bez zegara: `bez zegara`, bez pola (daneNieaktualne() i tak daje false)
  DaneDnia bz;
  bz.hist = &h;
  ROWNE(belkaDnia(bz).srodek, "bez zegara");
  SPRAWDZ(belkaDnia(bz).alarm.empty());
  // K1: noc nieubiegła `noc 13.11 ◆ · ocena ograniczona` → pole `! dane z 13.11`, za nim `◆ · ocena ograniczona`
  ustawZnakiBelki(z);
  DaneK1 k;
  k.dataNocy = "13.11";
  k.ubiegla = false;
  k.romb = true;
  k.ocena = "ocena ograniczona";
  Belka bk = belkaK1(k);
  ROWNE(bk.srodek, "");
  ROWNE(bk.alarm, "dane z 13.11");
  ROWNE(bk.poPolu, "◆ · ocena ograniczona");
  // K2: zakres zostaje, godziny odczytu nie ma
  DaneK2 k2;
  k2.zakres = "7–13.11";
  k2.godzinaDanych = "05:46";
  Belka b2 = belkaK2(k2);
  ROWNE(b2.srodek, "7–13.11");
  ROWNE(b2.alarm, "dane z 13.11");
  // ekran w negatywie (S3) i karta „problem”: belka jasna, pole czarne, pas równy belce narysowanej osobno
  {
    PlotnoT n;
    n.prostokat(0, 0, 800, 480, true);
    TrescEkranu t = trescDnia("S3", "", dd);
    rysujEkranDanych(n, b, t);
    SPRAWDZ(maBelke(n, b));
    SPRAWDZ(n.czarne(xa, BELKA_POLE_Y, xa + 1, BELKA_POLE_Y + BELKA_POLE_H) == BELKA_POLE_H);
  }
  // najdłuższe teksty: każda data roku, ekran dnia, Portret, K1 (z rombem i oceną), K2, przy każdym stanie baterii,
  // z klepsydrą i bez, ze znakiem nowej wersji i bez → pole i część po polu zawsze w całości, 16 px przed prawym polem
  int przypadki = 0, zle0 = zle, bezKlepsydry = 0, bezZnaku = 0;
  const BateriaEkranu stany[] = {bat(5), bat(78), bat(100), bat(100, true), BateriaEkranu()};
  for (int i = 0; i < 365; i++) {
    const std::string dz = dodajDni("2026-01-01", i), wcz = dodajDni(dz, -1);
    std::vector<Noc> hh = nocDo(dodajDni(dz, -2));
    DaneDnia x;
    x.dzis = dz;
    x.hist = &hh;
    for (const BateriaEkranu& st : stany)
      for (int pob = 0; pob < 2; pob++)
        for (int wer = 0; wer < 2; wer++) {
          ZnakiBelki zz;
          zz.nieaktualne = true;
          zz.daneZ = ddmm(wcz);
          zz.pobieram = pob;
          zz.nowaWersja = wer;
          ustawZnakiBelki(zz);
          ustawBaterieEkranu(st);
          DaneK1 kk;
          kk.dataNocy = ddmm(wcz);
          kk.romb = true;
          kk.ocena = "ocena ograniczona";
          DaneK2 kz;
          kz.zakres = "28.10–03.11";
          for (const Belka& bb : {belkaDnia(x), belkaPortretu(x), belkaK1(kk), belkaK2(kz)}) {
            przypadki++;
            bool dobrze = srodekMiesciSie(bb) && bb.alarm == "dane z " + ddmm(wcz) &&
                          (pob == 0 || ukladSrodkaBelki(bb).klepsydra || bb.nazwa.rfind("Ostatnia noc", 0) == 0);
            if (pob && !ukladSrodkaBelki(bb).klepsydra) bezKlepsydry++;
            if (wer && !znakWersjiNaBelce(bb)) bezZnaku++;
            // ekran dnia, Portret i K2 mają miejsce na oba znaki zawsze
            if (bb.nazwa.rfind("Ostatnia noc", 0) != 0) SPRAWDZ(pob == (int)ukladSrodkaBelki(bb).klepsydra &&
                                                               wer == (int)znakWersjiNaBelce(bb));
            if (!dobrze && zle - zle0 < 5) printf("  nie mieści się: %s %s %s\n", bb.nazwa.c_str(), dz.c_str(), bb.srodek.c_str());
            SPRAWDZ(dobrze);
          }
        }
  }
  printf("  R-EKR-67: %d przypadków (365 dat × 5 stanów baterii × klepsydra × znak wersji × 4 belki); K1 z oceną "
         "ograniczoną: znak wersji ustąpił w %d, klepsydra w %d\n", przypadki, bezZnaku, bezKlepsydry);
  ustawZnakiBelki(ZnakiBelki());
  ustawBaterieEkranu(BateriaEkranu());
}

static void testR_EKR_68_klepsydra() {
  ustawBaterieEkranu(bat(78));
  ZnakiBelki z;
  z.pobieram = true;
  ustawZnakiBelki(z);
  SPRAWDZ(!klepsydraNarysowana());
  const Belka b{"Poranek", "sb 14.11 · dane z 13.11"};
  UkladSrodka u = ukladSrodkaBelki(b);
  SPRAWDZ(u.klepsydra && u.slowo && u.tekst == b.srodek);
  PlotnoT p;
  belka(p, b);
  SPRAWDZ(klepsydraNarysowana());
  const int xk = xSrodkaBelki(b) + szer(F_P22B, b.srodek) + BELKA_ODSTEP_POLA;
  // znak co najmniej tak wysoki jak małe litery kroju środka (`x` w P22B), w pasie belki, nad linią
  PlotnoT lx;
  tekst(lx, F_P22B, 100, BELKA_BAZA, "x");
  const int hk = wysokoscRysunku(p, xk, xk + KLEPSYDRA_W), hx = wysokoscRysunku(lx, 90, 130);
  SPRAWDZ(hk == KLEPSYDRA_H && hk >= hx);
  SPRAWDZ(p.czarne(xk, KLEPSYDRA_Y, xk + KLEPSYDRA_W, KLEPSYDRA_Y + 2) == 2 * KLEPSYDRA_W);   // górna podstawa
  SPRAWDZ(p.czarne(xk, KLEPSYDRA_Y + KLEPSYDRA_H - 2, xk + KLEPSYDRA_W, KLEPSYDRA_Y + KLEPSYDRA_H) == 2 * KLEPSYDRA_W);
  SPRAWDZ(p.czarne(xk + KLEPSYDRA_W, 0, xk + KLEPSYDRA_W + KLEPSYDRA_ODSTEP, 41) == 0);  // odstęp przed słowem
  SPRAWDZ(p.czarne(xk + KLEPSYDRA_W + KLEPSYDRA_ODSTEP, 8, xk + szerKlepsydry(true) + 1, 38) > 0);  // `pobieram`
  // bez pobierania: klatka jak przed 14.11.2026
  ustawZnakiBelki(ZnakiBelki());
  PlotnoT q, w;
  belka(q, b);
  SPRAWDZ(!klepsydraNarysowana());
  SPRAWDZ(q.czarne(xk, 0, xk + szerKlepsydry(true), 41) == 0);
  // ciasno (K1, ocena ograniczona z rombem, kabel): najpierw ustępuje słowo `pobieram`, znak zostaje
  ustawBaterieEkranu(bat(100, true));
  ustawZnakiBelki(z);
  Belka k{"Ostatnia noc 1 / 3", ""};
  SrodekNocy s;
  s.przed = "noc ";
  s.data = "28.08";
  s.dataStala = true;
  s.romb = true;
  s.dopisek = "ocena ograniczona";
  ustawSrodekNocy(k, s);
  UkladSrodka uk = ukladSrodkaBelki(k);
  SPRAWDZ(uk.klepsydra && uk.tekst == k.srodek && srodekMiesciSie(k));
  printf("  R-EKR-68: K1 `%s` przy kablu: klepsydra %s słowa\n", uk.tekst.c_str(), uk.slowo ? "ze" : "bez");
  // klepsydra na ekranie w negatywie: belka jasna, znak czarny
  {
    std::vector<Noc> h = nocDo("2026-11-13");
    DaneDnia dd;
    dd.dzis = "2026-11-14";
    dd.hist = &h;
    PlotnoT n;
    rysujEkranDanych(n, b, trescDnia("S3", "", dd));
    SPRAWDZ(maBelke(n, b));
  }
  // R-EKR-65 przy znakach: godzina odczytu zostaje zawsze; przy braku miejsca ustępują znak nowej wersji i klepsydra
  {
    int n = 0, bezGodziny = 0, bezKl = 0, bezZn = 0;
    const BateriaEkranu st[] = {bat(5), bat(78), bat(100), bat(100, true), BateriaEkranu()};
    for (int i = 0; i < 365; i++) {
      const std::string dz = dodajDni("2026-01-01", i);
      for (const BateriaEkranu& bb : st)
        for (int romb = 0; romb < 2; romb++)
          for (const char* oc : {"", "ocena częściowa", "ocena ograniczona"}) {
            ZnakiBelki zz;
            zz.pobieram = zz.nowaWersja = true;
            ustawZnakiBelki(zz);
            ustawBaterieEkranu(bb);
            DaneK1 kk;
            kk.ubiegla = true;
            kk.dzienNocy = DNI_KROTKO[dzienTygodnia(dz)];
            kk.dataNocy = ddmm(dz);
            kk.godzinaDanych = "08:08";
            kk.romb = romb;
            kk.ocena = oc;
            Belka bk = belkaK1(kk);
            UkladSrodka uu = ukladSrodkaBelki(bk);
            n++;
            bezGodziny += uu.tekst.find("odczyt 08:08") == std::string::npos;
            bezKl += !uu.klepsydra;
            bezZn += !znakWersjiNaBelce(bk);
            SPRAWDZ(srodekMiesciSie(bk) && (!*oc || uu.tekst.find(oc) != std::string::npos));
          }
    }
    printf("  R-EKR-65 z klepsydrą i znakiem wersji (K1, %d przypadków): bez godziny %d, klepsydra ustąpiła %d, znak "
           "wersji %d\n", n, bezGodziny, bezKl, bezZn);
    ROWNE_L(bezGodziny, 0);
    ustawBaterieEkranu(bat(78));
  }
  // ustawZnakiBelki() zeruje pamięć klatki
  ustawZnakiBelki(ZnakiBelki());
  SPRAWDZ(!klepsydraNarysowana());
  ustawBaterieEkranu(BateriaEkranu());
}

static void testR_EKR_69_znakNowejWersji() {
  ustawBaterieEkranu(bat(78));
  const Belka b{"Poranek", "sb 14.11 · odczyt 07:12"};
  const int xbBez = xPrawegoPolaBelki(b);
  ZnakiBelki z;
  z.nowaWersja = true;
  ustawZnakiBelki(z);
  SPRAWDZ(xbBez - xPrawegoPolaBelki(b) == ZNAK_WERSJI_D + BELKA_ODSTEP_ZNAKU);
  PlotnoT p;
  belka(p, b);
  const int xz = xPrawegoPolaBelki(b);
  // koło 22 px, od y 10 do 31, przed ikoną baterii; ikona baterii na swoim stałym miejscu
  SPRAWDZ(wysokoscRysunku(p, xz, xz + ZNAK_WERSJI_D) == ZNAK_WERSJI_D);
  SPRAWDZ(p.cz(xz + ZNAK_WERSJI_D / 2, ZNAK_WERSJI_Y) && p.cz(xz, ZNAK_WERSJI_Y + ZNAK_WERSJI_D / 2));
  SPRAWDZ(p.cz(xz + ZNAK_WERSJI_D / 2, ZNAK_WERSJI_Y + ZNAK_WERSJI_D / 2));  // trzon strzałki w środku
  SPRAWDZ(p.czarne(xz + ZNAK_WERSJI_D, 0, xz + ZNAK_WERSJI_D + BELKA_ODSTEP_ZNAKU, 41) == 0);
  SPRAWDZ(p.cz(xbBez, 12));  // obrys baterii w tym samym miejscu co bez znaku
  ROWNE(srodekBelki(b), b.srodek);
  // jedno miejsce: przy belce stopka nie rysuje ikony aktualizacji; bez belki (slot dzienny) tak
  {
    IkonaStopki ik(true);
    SPRAWDZ(ikonaAktualizacjiWlaczona());
    PlotnoT q;
    belka(q, b);
    SPRAWDZ(!ikonaAktualizacjiWlaczona());
    SPRAWDZ(prawyBrzegStopki() == 772);
    SPRAWDZ(!ikonaAktualizacji(q, false));
  }
  {
    IkonaStopki ik(true);  // nowa klatka bez belki (ekran slotu dziennego)
    PlotnoT q;
    SPRAWDZ(ikonaAktualizacjiWlaczona() && ikonaAktualizacji(q, false));
  }
  // na ekranie dnia z belką: znak na belce, w stopce nic
  {
    std::vector<Noc> h = nocDo("2026-11-14");
    DaneDnia d;
    d.dzis = "2026-11-14";
    d.hist = &h;
    d.pogoda = "Katowice 8° · deszcz bardzo długi opis pogody";
    d.przyLozku = "przy łóżku 21° · 46%";
    IkonaStopki ik(true);
    PlotnoT q;
    rysujEkranDanych(q, belkaDnia(d), trescDnia("S2A", "", d));
    SPRAWDZ(!ikonaAktualizacji(q, false));
    SPRAWDZ(q.czarne(772 - BOK_IKONY_AKTUALIZACJI, 441, 772, 466) == 0 ||
            q.czarne(772 - BOK_IKONY_AKTUALIZACJI, 441, 772, 466) < BOK_IKONY_AKTUALIZACJI * BOK_IKONY_AKTUALIZACJI / 2);
    SPRAWDZ(wysokoscRysunku(q, xPrawegoPolaBelki(belkaDnia(d)), xPrawegoPolaBelki(belkaDnia(d)) + ZNAK_WERSJI_D) ==
            ZNAK_WERSJI_D);
  }
  ustawZnakiBelki(ZnakiBelki());
  ustawBaterieEkranu(BateriaEkranu());
}

int main() {
  testR_EKR_63_geometria();
  testR_EKR_64_zawartoscTresci();
  testR_EKR_64_kazdyEkranRysujeBelke();
  testR_EKR_64_ekranyBezBelki();
  testR_EKR_65_ustepowanieSrodka();
  testR_EKR_66_ikonaBaterii();
  testR_KON_69_niskaBateriaWStopceDnia();
  testR_EKR_67_definicjaNieaktualnych();
  testR_EKR_67_wygladNieaktualnych();
  testR_EKR_68_klepsydra();
  testR_EKR_69_znakNowejWersji();
  printf("testy_belka: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
