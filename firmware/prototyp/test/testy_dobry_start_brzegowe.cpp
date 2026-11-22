// SPDX-License-Identifier: GPL-3.0-or-later
// Testy: program „Dobry start” z dyrygentem na atrapach (spec 5.8.14, ADR-0049): S-K-48 w bloku dziennym (D0b) i wieczorem
// (W3b), program wyłączony bez zmian w panelu, restart w środku dnia, `dzien.ukryte=porada`, plik stanu, rysunek ekranu
// (belka, pasek tygodni, karta z budzikiem, linia uwagi, „tylko na telefonie”) i strony formularza (ankieta, wszystkie
// porady, od nowa). Harness doby z test/testy_slot_dzienny_dyrygent.cpp (dołączony, jego main() przemianowany).
#define main main_testy_slot_dzienny_dyrygent
#include "testy_slot_dzienny_dyrygent.cpp"
#undef main

#include "dobry_start.h"
#include "ekrany_kontekst.h"
#include "formularz.h"

namespace {

struct PlotnoT : Plotno {
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

const std::string PLIK = "/dane/dobry_start.csv";

// wpisy P z pliku stanu (wiersze `P;…`)
int wpisyP(const std::map<std::string, std::string>& pliki, const char* pora = "") {
  auto i = pliki.find(PLIK);
  if (i == pliki.end()) return 0;
  int n = 0;
  size_t p = 0;
  while ((p = i->second.find("\nP;", p)) != std::string::npos) {
    size_t k = i->second.find('\n', p + 1);
    std::string w = i->second.substr(p + 1, k - p - 1);
    if (!*pora || w.find(pora) != std::string::npos) n++;
    p++;
  }
  return n;
}

// R-DS-01: wyłączony (bez wiersza i `start.program=nie`): te same klatki i ten sam dziennik ekranów, bez pliku stanu
void testR_DS_01_wylaczonyBezZmian() {
  auto st = stanStaly(V78, Zasilanie::BATERIA);
  DobaBaterii bez = dobaBaterii(true, st, "");
  DobaBaterii nie = dobaBaterii(true, st, "start.program=nie\n");
  SPRAWDZ(bez.klatka == nie.klatka && bez.odswiezen == nie.odswiezen && bez.wybudzen == nie.wybudzen);
  SPRAWDZ(!bez.pliki.count(PLIK) && !nie.pliki.count(PLIK));
  SPRAWDZ(!logMa(bez.v, "S-K-48") && !logMa(nie.v, "S-K-48"));
  bool takieSame = bez.v.size() == nie.v.size();
  for (size_t i = 0; takieSame && i < bez.v.size(); i++) takieSame = bez.v[i].ep == nie.v[i].ep;
  SPRAWDZ(takieSame);  // te same chwile wybudzeń
  printf("  R-DS-01: wyłączony: %d wybudzeń, %d odświeżeń, plik stanu: brak\n", bez.wybudzen, bez.odswiezen);
}

// R-DS-03, R-DS-20, R-DS-21: porada w bloku 180 min od startu slotu, po bloku zwykły slot, wieczorem znowu porada
void testR_DS_03_blokIWieczor() {
  auto st = stanStaly(V78, Zasilanie::BATERIA);
  DobaBaterii d = dobaBaterii(true, st, "start.program=tak\n", ep(2026, 11, 4, 23, 0));
  int pierwsza = -1, ostatniaDzien = -1;
  for (auto& w : d.v)
    for (auto& z : w.zdarzenia)
      if (z.find("porada dnia: S-K-48") != std::string::npos && z.find("blok dzienny") != std::string::npos) {
        if (pierwsza < 0) pierwsza = w.minuta;
        ostatniaDzien = w.minuta;
      }
  printf("  R-DS-03: S-K-48 w bloku od %02d:%02d do %02d:%02d, wieczorem: %s, wpisy P: %d\n", pierwsza / 60, pierwsza % 60,
         ostatniaDzien / 60, ostatniaDzien % 60, logMa(d.v, "S-K-48 DS-", 17 * 60, 24 * 60) ? "tak" : "nie",
         wpisyP(d.pliki));
  SPRAWDZ(pierwsza >= 0 && ostatniaDzien - pierwsza < dobry_start::BLOK_MIN);
  SPRAWDZ(logMa(d.v, "slot dzienny: S-K-3", ostatniaDzien + 1, 21 * 60));  // po bloku zwykły slot dzienny
  SPRAWDZ(logMa(d.v, "(wieczór", 17 * 60, 24 * 60));                       // W3b
  SPRAWDZ(wpisyP(d.pliki) == 1 && wpisyP(d.pliki, ";dzien;") == 1);       // jedna porada na dobę
  SPRAWDZ(d.pliki.count(PLIK) && d.pliki.at(PLIK).compare(0, 25, "#cpap-eink dobry_start 1\n") == 0);
}

// R-DS-22: `porada` w `dzien.ukryte` to nazwa nieznana; poradę wyłącza tylko `start.program=nie`
void testR_DS_22_dzienUkryte() {
  DobaBaterii d = dobaBaterii(true, stanStaly(V78, Zasilanie::BATERIA), "start.program=tak\ndzien.ukryte=porada\n");
  SPRAWDZ(logMa(d.v, "porada dnia: S-K-48"));
}

// R-DS-43: restart w środku dnia (zanik zasilania o 10:00): ta sama porada, bez drugiego wpisu
void testR_DS_43_restartWSrodkuDnia() {
  PanelSym s;
  s.p.pliki.pliki["/konfiguracja.txt"] = "start.program=tak\n";
  std::vector<Wyb> a = symuluj(s, ep(2026, 11, 4, 0, 5), ep(2026, 11, 4, 10, 0), odpowiedziB9);
  int przed = wpisyP(s.p.pliki.pliki);
  PanelSym r;
  r.p.pliki.pliki = s.p.pliki.pliki;
  r.p.nvs.teksty = s.p.nvs.teksty;
  std::vector<Wyb> b = symuluj(r, ep(2026, 11, 4, 10, 5), ep(2026, 11, 4, 12, 0), odpowiedziB9);
  std::string ida, idb;
  for (auto* v : {&a, &b})
    for (auto& w : *v)
      for (auto& z : w.zdarzenia) {
        size_t i = z.find("porada dnia: S-K-48 ");
        if (i != std::string::npos) (v == &a ? ida : idb) = z.substr(i + 20, z.find(' ', i + 20) - i - 20);
      }
  printf("  R-DS-43: przed restartem %s, po restarcie %s, wpisy P %d → %d\n", ida.c_str(), idb.c_str(), przed,
         wpisyP(r.p.pliki.pliki));
  SPRAWDZ(!ida.empty() && ida == idb && przed == 1 && wpisyP(r.p.pliki.pliki) == 1);
}

// R-DS-15, R-DS-16, R-DS-18, R-DS-50: rysunek ekranu
void testR_DS_15_rysunek() {
  const dobry_start::Katalog& k = dobry_start::katalogWbudowany();
  dobry_start::Stan s;
  dobry_start::Ustawienia u;
  u.program = true;
  u.dzien = 18;  // trzeci tydzień, czwarte miejsce
  s = dobry_start::nowyStan(u, k);
  s.start = "2026-12-01";
  s.dzienUstawiony = 1;
  dobry_start::WyborPorady w;
  w.porada = dobry_start::wariantMiejsca(k, 18, dobry_start::Tryb::CPAP, -1);
  SPRAWDZ(w.porada != nullptr);
  if (!w.porada) return;
  DanePorady dp;
  dp.o = dobry_start::opis(k, s, w);
  dp.belka = Belka(dobry_start::NAZWA_BELKI, dp.o.srodekBelki);
  dp.stopka = StopkaDzienna{"noc 6 h 38 · AHI 1,1 · wyciek 4", "za oknem 3° · przy łóżku 20°"};
  PlotnoT p;
  rysujPorade(p, dp);
  int x0 = PORADA_PASEK_PRAWO - 5 * PORADA_PASEK_KROK - PORADA_PASEK_W;
  // tygodnie 1 i 2 pełne, trzeci z obrysem i wypełnieniem 4/7, 4…6 obrys
  SPRAWDZ(p.czarne(x0, 62, x0 + 20, 75) == 20 * 13 && p.czarne(x0 + 26, 62, x0 + 46, 75) == 20 * 13);
  int trzeci = p.czarne(x0 + 52, 62, x0 + 72, 75), czwarty = p.czarne(x0 + 78, 62, x0 + 98, 75);
  SPRAWDZ(trzeci > czwarty && trzeci < 20 * 13 && czwarty == 2 * 20 + 2 * 11);
  SPRAWDZ(p.czarne(28, 100, 776, 140) > 0 && p.czarne(28, 160, 776, 360) > 0);  // tytuł i treść
  SPRAWDZ(p.czarne(0, 41, 800, 44) == 800 * 3);                                 // linia belki
  SPRAWDZ(p.czarne(28, 440, 400, 470) > 0);                                     // lewa stopka z wynikiem nocy
  // linia uwagi: bez etykiety i paska
  PlotnoT pl;
  dp.linia = "Bateria 15%. Podłącz kabel.";
  dp.liniaOstrzezenie = true;
  rysujPorade(pl, dp);
  SPRAWDZ(pl.czarne(x0, 62, PORADA_PASEK_PRAWO, 75) > 20 * 13 * 2);  // na pasku stoi linia (czarny pasek ostrzeżenia)
  // karta: budzik w prawym dolnym rogu, lewa stopka pusta
  dobry_start::WyborPorady wk;
  for (auto& x : k.porady)
    if (x.karta && x.rodzaj == dobry_start::Rodzaj::PLAN) wk.porada = &x;
  DanePorady dk;
  dk.o = dobry_start::opis(k, s, wk);
  dk.belka = Belka(dobry_start::NAZWA_BELKI, dk.o.srodekBelki);
  dk.scena = "M06";
  dk.stopka = StopkaDzienna{"", "za oknem 3° · przy łóżku 20°"};
  PlotnoT pk;
  rysujPorade(pk, dk);
  SPRAWDZ(scenaJest("M06") && pk.czarne(534, 220, 775, 426) > 500 && pk.czarne(28, 440, 400, 470) == 0);
  // „tylko na telefonie”: sam tytuł
  dobry_start::WyborPorady wt = w;
  wt.telefon = true;
  DanePorady dt;
  dt.o = dobry_start::opis(k, s, wt);
  dt.belka = Belka(dobry_start::NAZWA_BELKI, dt.o.srodekBelki);
  PlotnoT pt;
  rysujPorade(pt, dt);
  SPRAWDZ(dt.o.tytul == dobry_start::TYTUL_TELEFON && pt.czarne(28, 160, 776, 420) == 0 && pt.czarne(x0, 62, 776, 75) == 0);
}

// R-DS-31, R-DS-25, R-DS-35, R-DS-33: strony formularza
std::string podstaw(const std::string& t, void*) {
  return dobry_start::podstawProgi(t, [](const std::string&) -> std::string { return "15"; });
}
void testR_DS_35_stronyFormularza() {
  const dobry_start::Katalog& k = dobry_start::katalogWbudowany();
  dobry_start::Stan s;
  dobry_start::Ustawienia u;
  u.program = true;
  size_t maks = 0;
  for (int t = 1; t <= k.tygodni() + 3; t++) {
    std::string h = formularz::stronaPorad(k, s, u, dobry_start::Tryb::ASV, t, "/u/abc/", podstaw, nullptr);
    maks = std::max(maks, h.size());
    SPRAWDZ(h.find("http") == std::string::npos && h.find("<script") == std::string::npos && h.find("{prog:") == std::string::npos);
    SPRAWDZ(h.find("jeszcze nie") != std::string::npos);
  }
  printf("  R-DS-35: najdłuższa strona „Wszystkie porady” %zu B (próg %zu B)\n", maks, formularz::MAKS_STRONY_PORAD);
  SPRAWDZ(maks <= formularz::MAKS_STRONY_PORAD);
  std::string t1 = formularz::stronaPorad(k, s, u, dobry_start::Tryb::ASV, 1, "/u/abc/", podstaw, nullptr);
  const dobry_start::Porada* asv = dobry_start::wariantMiejsca(k, 1, dobry_start::Tryb::ASV, -1);
  SPRAWDZ(asv && t1.find(formularz::html(k.tekst(asv->tresc))) != std::string::npos);
  // ankieta: pytanie startowe tylko bez odpowiedzi; zapis bez odpowiedzi nie blokuje (R-DS-48)
  u.nawilzacz = -1;
  std::string a = formularz::stronaAnkiety(k, s, u, "2026-12-05", "/u/abc/", "");
  int pytan = 0;
  for (auto& q : k.pytania) pytan += a.find(formularz::html(q.pytanie)) != std::string::npos;
  SPRAWDZ(pytan >= 3 && a.find("Zapisz odpowiedzi") != std::string::npos && a.find("Pomiń") != std::string::npos);
  auto odp = formularz::odpowiedziAnkiety(k, {{"ankieta_DS-Q03", "1"}, {"ankieta_DS-Q04", "1"}, {"q_DS-Q04_2", "1"}});
  SPRAWDZ(odp.size() == 2 && odp[0].second.empty() && odp[1].second == std::vector<int>{2});
  // sesja: podstrony pod tym samym tokenem, inne ścieżki 404
  formularz::Sesja ss;
  ss.otworz("tok", 0, "1.2.3.4");
  SPRAWDZ(ss.obsluz("GET", "/u/tok/porady?t=3", "", 1, "").podstrona == "porady");
  SPRAWDZ(ss.obsluz("GET", "/u/tok/porady?t=3", "", 1, "").zakladka == 3);
  SPRAWDZ(ss.obsluz("POST", "/u/tok/ankieta", "q_DS-Q03=2", 1, "").podstrona == "ankieta");
  SPRAWDZ(ss.obsluz("GET", "/u/tok/inne", "", 1, "").kod == 404);
  SPRAWDZ(formularz::stronaOdNowa("/u/tok/").find("Tak, od nowa") != std::string::npos);
  // formularz: sekcja po „Ekrany”, przed „Dźwięk i humor”, nagłówek, pola, walidacja dnia
  formularz::Pola pf = formularz::zPliku("start.program=tak\nstart.dzien=15\n", sekrety::Dane());
  pf.startNaglowek = "dzień 15 z 42";
  pf.startMiejsc = k.miejsc;
  pf.startPonadIle = k.ponadIle;
  std::string f = formularz::strona(pf, "/u/tok/", "", {});
  size_t e = f.find("<b>Ekrany</b>"), d = f.find("<b>Dobry start</b>"), h = f.find("<b>Dźwięk i humor</b>");
  SPRAWDZ(e < d && d < h && f.find("dzień 15 z 42") != std::string::npos);
  SPRAWDZ(f.find("Pierwsze dni i tygodnie decydują, czy terapia się przyjmie.") != std::string::npos);
  formularz::Wynik wy = formularz::waliduj({{"start_program", "tak"}, {"start_dzien", "50"}}, sekrety::Dane(), "2026-12-01");
  SPRAWDZ(wy.plik["start.program"] == "tak" && wy.bledy.count("start_dzien") &&
          wy.bledy["start_dzien"] == "Wpisz liczbę od 1 do " + std::to_string(k.miejsc) + ".");
  formularz::Wynik wd = formularz::waliduj({{"start_program", "nie"}, {"start_dzien", "1"}, {"start_wznow", "od_nowa"}},
                                           sekrety::Dane(), "2026-12-01");
  // „od nowa” przy wyłączanym programie nie zeruje stanu (R-DS-32: wyłączenie zachowuje stan), tylko przy `tak`
  SPRAWDZ(wd.plik.count("start.program") && wd.plik["start.program"].empty() && !wd.startOdNowa);
  formularz::Wynik wt = formularz::waliduj({{"start_program", "tak"}, {"start_wznow", "od_nowa"}}, sekrety::Dane(), "2026-12-01");
  SPRAWDZ(wt.plik["start.program"] == "tak" && wt.startOdNowa);
  // pytanie startowe ankiety i formularz: ta sama konwencja zapisu `start.nawilzacz` (`nie wiem` = bez wiersza)
  SPRAWDZ(formularz::wartoscDoPliku("start.nawilzacz", "nie_wiem").empty() &&
          formularz::wartoscDoPliku("start.nawilzacz", "nie") == "nie");
  // ankieta po odpowiedzi `nie wiem` na pytanie startowe już go nie zadaje
  const dobry_start::Pytanie* q2 = nullptr;
  for (auto& q : k.pytania)
    if (q.odTygodnia == 0) q2 = &q;
  SPRAWDZ(q2 != nullptr);
  if (q2) {
    dobry_start::Stan sa;
    sa.start = "2026-12-01";
    dobry_start::Ustawienia ua;
    ua.program = true;
    std::string przed = formularz::stronaAnkiety(k, sa, ua, "2026-12-05", "/u/t/", "");
    dobry_start::zapiszAnkiete(k, sa, "2026-12-05", {{q2->id, {(int)q2->odpowiedzi.size()}}});
    std::string po = formularz::stronaAnkiety(k, sa, ua, "2026-12-05", "/u/t/", "");
    SPRAWDZ(przed.find(formularz::html(q2->pytanie)) != std::string::npos &&
            po.find(formularz::html(q2->pytanie)) == std::string::npos);
  }
}

}  // namespace

int main() {
  setenv("TZ", STREFA_PANELU, 1);
  tzset();
  testR_DS_01_wylaczonyBezZmian();
  testR_DS_03_blokIWieczor();
  testR_DS_22_dzienUkryte();
  testR_DS_43_restartWSrodkuDnia();
  testR_DS_15_rysunek();
  testR_DS_35_stronyFormularza();
  printf("testy_dobry_start_brzegowe: %d ok, %d błędów\n", ok, zle);
  return zle ? 1 : 0;
}
