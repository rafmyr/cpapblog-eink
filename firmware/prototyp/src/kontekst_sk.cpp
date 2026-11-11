// SPDX-License-Identifier: GPL-3.0-or-later
// Pozostałe ekrany kontekstowe S-K (spec 5.8): opis w src/kontekst_sk.h.
#include "kontekst_sk.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "stany.h"
#include "wspolne.h"
#include "wzorce.h"

namespace reguly {
void zarejestrujRodzajeE();  // src/rodzaje_e.cpp: przed pierwszym zestawWbudowany
}

namespace sk {

std::string tekst(const std::string& id, const std::map<std::string, std::string>& z) {
  static bool rodzaje = false;  // zestaw odrzuca nieznany rodzaj, więc rodzaje spoza rdzenia przed pierwszym użyciem
  if (!rodzaje) {
    reguly::zarejestrujRodzajeE();
    wzorce::zarejestrujRodzajeL();
    rodzaje = true;
  }
  reguly::Wynik w;
  w.zmienne = z;
  return reguly::tekst(reguly::zestawWbudowany(), id, w, reguly::Tryb::NIEZNANY);
}

namespace {

using Z = std::map<std::string, std::string>;

const Noc* nocZ(const std::vector<Noc>* h, const std::string& d) {
  if (!h) return nullptr;
  for (auto& n : *h)
    if (n.data == d) return &n;
  return nullptr;
}
const NocSypialni* sypZ(const std::vector<NocSypialni>* s, const std::string& d) {
  if (!s) return nullptr;
  for (auto& n : *s)
    if (n.data == d) return &n;
  return nullptr;
}
bool zTerapia(const Noc* n) { return n && n->m01 >= SK12_MIN; }  // noc z terapią ≥ 15 min (5.12.3)
bool asv(reguly::Tryb t) { return t == reguly::Tryb::ASV || t == reguly::Tryb::ASVAUTO; }
float cisnienie(const Noc& n, bool a) { return a ? epapP95(n) : n.cp95; }

std::vector<const Noc*> wazneDo(const Wejscie& w, const std::string& dzis) {
  std::vector<const Noc*> v;
  if (!w.hist) return v;
  for (auto& n : *w.hist)
    if (n.data <= dzis && n.wazna) v.push_back(&n);
  return v;
}

std::string czasMin(double m) { return std::isnan(m) || m < 0 ? "–" : godzH((int)std::lround(m)); }
std::string ile(double m) {
  int x = (int)std::lround(m);
  if (x == 60) return "godzinę";
  if (x < 60) return std::to_string(x) + " min";
  return godzH(x);
}
std::string wielka(std::string s) {
  if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 'a' + 'A');
  if (s.compare(0, 2, "ś") == 0) s.replace(0, 2, "Ś");
  return s;
}
std::string mala(std::string s) {
  if (!s.empty() && s[0] >= 'A' && s[0] <= 'Z' && !(s.size() > 1 && s[1] >= 'A' && s[1] <= 'Z')) s[0] = (char)(s[0] - 'A' + 'a');
  return s;  // skróty wersalikami (`EPAP`) bez zmian
}
double odchStd(const std::vector<double>& v) {
  if (v.size() < 2) return NAN;
  double s = 0;
  for (double x : v) s += x;
  double m = s / (double)v.size(), q = 0;
  for (double x : v) q += (x - m) * (x - m);
  return std::sqrt(q / (double)v.size());
}

// ---------- dziennik pokazań (7.4) ----------
std::vector<std::string> dniTypu(const Wejscie& w, const std::string& typ, const std::string& od, const std::string& do_,
                                 const std::string& sprawaNie = "") {
  std::vector<std::string> d;
  if (!w.dziennik) return d;
  for (auto& x : *w.dziennik) {
    if (x.typ != typ || x.data < od || x.data > do_) continue;
    if (!sprawaNie.empty() && x.klucz.compare(0, sprawaNie.size(), sprawaNie) == 0) continue;
    if (std::find(d.begin(), d.end(), x.data) == d.end()) d.push_back(x.data);
  }
  return d;
}
bool dzisJest(const Wejscie& w, const std::string& typ) { return !dniTypu(w, typ, w.dzis, w.dzis).empty(); }
std::string ostatnio(const Wejscie& w, const std::string& typ) {
  std::string o;
  for (auto& d : dniTypu(w, typ, "0000-00-00", dodajDni(w.dzis, -1))) o = std::max(o, d);
  return o;
}
// najwyżej 3 różne ekrany S-K w dobie (7.4); ekran, który już był dziś, się nie liczy drugi raz
bool limitDoby(const Wejscie& w, const std::string& typ) {
  if (!w.dziennik || dzisJest(w, typ)) return false;
  std::set<std::string> t;
  for (auto& x : *w.dziennik)
    if (x.data == w.dzis && x.typ.compare(0, 2, "SK") == 0) t.insert(x.typ);
  return (int)t.size() >= EKRANOW_SK_NA_DOBE;
}
// R-KON-14: ten sam problem poranny 2 razy w 7 dniach, potem przerwa do 7 dni od pierwszego pokazania
bool limitProblemu(const Wejscie& w, const std::string& typ, const std::string& bezSprawy = "") {
  if (dzisJest(w, typ)) return false;
  return (int)dniTypu(w, typ, dodajDni(w.dzis, -6), dodajDni(w.dzis, -1), bezSprawy).size() >= PROBLEM_POKAZAN;
}
// R-KON-14 dla S-K-10 w tonie „problem”: klucz sprawy w dzienniku rozdziela pasma.
// Noc > 24 (`S-K-10`) liczy tylko pokazania nocy > 24, żeby wyciek 15–24 z poprzednich dni nie zabierał ekranu
// „terapia nie działa”; noc 15–24 (`S-K-10/15`) liczy pokazania jednonocne obu pasm (bez „od kilku nocy”, jak
// dotąd); „od kilku nocy” w paśmie 15–24 (`S-K-10/15/kilka`) liczy wszystkie pokazania SK10.
bool limitSk10(const Wejscie& w, const std::string& sprawa) {
  if (!w.dziennik || dzisJest(w, "SK10")) return false;
  std::set<std::string> d;
  for (auto& x : *w.dziennik) {
    if (x.typ != "SK10" || x.data < dodajDni(w.dzis, -6) || x.data > dodajDni(w.dzis, -1)) continue;
    std::string k = x.klucz.substr(0, x.klucz.find('#'));  // klucz karty `sprawa#n` (dziennik humoru)
    bool liczy = sprawa == "S-K-10/15/kilka" || k == "S-K-10" || (sprawa == "S-K-10/15" && k == "S-K-10/15");
    if (liczy) d.insert(x.data);
  }
  return (int)d.size() >= PROBLEM_POKAZAN;
}
bool limitCoDni(const Wejscie& w, const std::string& typ, int dni) {
  if (dzisJest(w, typ)) return false;
  std::string o = ostatnio(w, typ);
  return !o.empty() && dniMiedzy(o, w.dzis) < dni;
}
bool limitOdkrycia(const Wejscie& w, const std::string& typ, const std::string& sprawa) {
  if (dzisJest(w, typ)) return false;
  for (const char* t : {"S1", "SK21", "SK32"})
    if (!dniTypu(w, t, dodajDni(w.dzis, -(ODKRYCIA_CO_DNI - 1)), dodajDni(w.dzis, -1)).empty()) return true;
  if (w.dziennik && !sprawa.empty())  // to samo odkrycie raz na 30 dni
    for (auto& x : *w.dziennik)
      if (x.typ == typ && x.klucz.compare(0, sprawa.size(), sprawa) == 0 && x.data < w.dzis &&
          dniMiedzy(x.data, w.dzis) < 30)
        return true;
  return false;
}
bool probny(const Wejscie& w, const std::string& id) {
  if (!w.trybProbny) return false;
  // R-KON-24: S-K-08, S-K-18, S-K-27…S-K-32 zawsze; S-K-01…S-K-07, S-K-09…S-K-17, S-K-19…S-K-21 tylko do logu
  // ADR-0039: S-K-40…S-K-47 od pierwszego dnia, jak S-K-30 (sygnał z normy osobistej, nie wskazówka kontekstowa)
  static const char* const ZAWSZE[] = {"S-K-08", "S-K-18", "S-K-27", "S-K-28", "S-K-29", "S-K-30", "S-K-31", "S-K-32",
                                       "S-K-40", "S-K-41", "S-K-42", "S-K-43", "S-K-44", "S-K-45", "S-K-46", "S-K-47"};
  for (auto z : ZAWSZE)
    if (id == z) return false;
  return true;
}

Tresc nowa(const char* id, const char* typ, const char* grupa, int kol, bool problem, bool karta) {
  Tresc t;
  t.id = id;
  t.typ = typ;
  t.grupa = grupa;
  t.kolejnosc = kol;
  t.problem = problem;
  t.karta = karta;
  t.sprawa = id;
  return t;
}
void teksty(Tresc& t, const std::string& pref, const Z& z, const std::string& powod = "powod",
            const std::string& krok = "krok", const std::string& tytul = "tytul") {
  t.t.etykieta = tekst(pref + ".etykieta", z);
  t.t.tytul = tekst(pref + "." + tytul, z);
  t.t.powod = tekst(pref + "." + powod, z);
  t.t.krok = krok.empty() ? "" : tekst(pref + "." + krok, z);
}

// ---------- wieczór ----------
bool sezonGrz(const std::string& d) { return sezonGrzewczy(d); }

// R-KON-29 i „ten sam ekran wieczorny” (7.4)
bool limitWieczoru(const Wejscie& w, const std::string& typ, bool sezonowy) {
  if (dzisJest(w, typ)) return false;
  if (sezonowy && sezonGrz(w.dzis)) {
    if (limitCoDni(w, typ, SEZON_CO_DNI)) return true;
    // początek sezonu: 1.10 roku sezonu (sezon 1.10–30.04)
    std::string rok = w.dzis.substr(0, 4);
    std::string pocz = w.dzis.substr(5, 2) >= "10" ? rok + "-10-01" : std::to_string(liczbaCalk(rok.c_str()) - 1) + "-10-01";
    // po 4 pokazaniach w sezonie tylko przy osobistym związku (R-KON-29); log ocen wieczornych bez pokazania nie
    // jest zapisywany, więc grupa „noce z warunkiem” ma najwyżej 4 noce < 5: związek nieustalony = brak ekranu
    return (int)dniTypu(w, typ, pocz, dodajDni(w.dzis, -1)).size() >= SEZON_POKAZAN;
  }
  return limitCoDni(w, typ, TEN_SAM_WIECZOR_DNI);
}

void sk03(const Wejscie& w, std::vector<Tresc>& o) {
  if (std::isnan(w.tempTeraz) || !(w.tempTeraz <= SK03_TEMP)) return;
  if (w.tagNawilzania && !w.nawilzanie) return;  // bez nawilżania para się nie skrapla; bez tagu warunek pomijany
  if (limitWieczoru(w, "SK03", true)) return;
  Tresc t = nowa("S-K-03", "SK03", "W1", 3, false, true);
  teksty(t, "S-K-03", {{"t", calkowita(w.tempTeraz)}});
  t.wiz = Wiz::PARY;
  t.pary.push_back({calkowita(w.tempTeraz) + "°", "teraz"});
  if (const NocSypialni* s = sypZ(w.sypialnia, w.dzis)) t.pary.push_back({calkowita(s->t) + "°", "zeszła noc"});
  if (w.prognoza) {  // za oknem w nocy: minimum 23:00–06:00 z prognozy (R-KON-15: bez prognozy bez tej pary)
    float mn = NAN;
    for (int g = 23; g < 30; g++) {
      const GodzinaPrognozy* h = w.prognoza->godzina(g < 24 ? w.dzis : dodajDni(w.dzis, 1), g % 24);
      if (h && !std::isnan(h->temp)) mn = std::isnan(mn) ? h->temp : std::min(mn, h->temp);
    }
    if (!std::isnan(mn)) t.pary.push_back({calkowita(mn) + "°", "za oknem w nocy"});
  }
  t.wartosc = (int)std::lround(w.tempTeraz);
  o.push_back(t);
}

void sk04(const Wejscie& w, std::vector<Tresc>& o) {
  if (std::isnan(w.wilgTeraz) || !(w.wilgTeraz < SK04_WILG)) return;
  if (limitWieczoru(w, "SK04", true)) return;
  Tresc t = nowa("S-K-04", "SK04", "W1", 4, false, true);
  teksty(t, "S-K-04", {{"w", calkowita(w.wilgTeraz)}});
  // linia wilgotności zeszłej nocy: rekord sypialni ma tylko medianę nocy (R-KON-15: ekran bez linii)
  t.wartosc = (int)std::lround(w.wilgTeraz);
  o.push_back(t);
}

void sk05(const Wejscie& w, std::vector<Tresc>& o) {
  float mx = NAN;
  if (w.prognoza)
    for (int g = 0; g < 24; g++)
      if (const GodzinaPrognozy* h = w.prognoza->godzina(w.dzis, g))
        if (!std::isnan(h->temp)) mx = std::isnan(mx) ? h->temp : std::max(mx, h->temp);
  bool zPrognozy = !std::isnan(mx) && mx >= SK05_MAX;
  bool zSypialni = !std::isnan(w.tempTeraz) && w.tempTeraz >= SK05_SYP;
  if (!zPrognozy && !zSypialni) return;
  if (limitWieczoru(w, "SK05", false)) return;
  Tresc t = nowa("S-K-05", "SK05", "W1", 0, false, true);
  teksty(t, "S-K-05", {{"t", calkowita(zPrognozy ? mx : w.tempTeraz)}}, zPrognozy ? "powod" : "powod_sypialnia");
  t.wiz = Wiz::PARY;
  if (!std::isnan(mx)) t.pary.push_back({calkowita(mx) + "°", "maksimum dziś"});
  if (!std::isnan(w.tempTeraz)) t.pary.push_back({calkowita(w.tempTeraz) + "°", "sypialnia teraz"});
  t.wartosc = (int)std::lround(zPrognozy ? mx : w.tempTeraz);
  o.push_back(t);
}

void sk06(const Wejscie& w, std::vector<Tresc>& o) {
  if (!w.prognoza || !w.pylki || !w.hist || w.hist->empty()) return;
  // pierwszy sezon pylenia: rok kalendarzowy pierwszej nocy w rekordach (przybliżenie pierwszej nocy z prognozą
  // pyłku); od drugiego sezonu tylko przy osobistym związku pyłku z wyciekiem (R-KON-18), a historii pyłku nocy
  // panel nie zapisuje, więc wtedy S-K-06 milczy
  if (w.hist->front().data.substr(0, 4) != w.dzis.substr(0, 4)) return;
  static const char* const DOP[PYL_N] = {"brzozy", "traw", "olchy", "bylicy"};
  int rodzaj = -1;
  float naj = 0;
  for (int k = 0; k < PYL_N; k++) {
    if (std::isnan(w.pylki->prog[k])) continue;
    for (int g = 12; g < 24; g++)
      if (const GodzinaPrognozy* h = w.prognoza->godzina(w.dzis, g))
        if (!std::isnan(h->pylek[k]) && h->pylek[k] >= w.pylki->prog[k] && h->pylek[k] / w.pylki->prog[k] > naj) {
          naj = h->pylek[k] / w.pylki->prog[k];
          rodzaj = k;
        }
  }
  if (rodzaj < 0) return;
  if (limitCoDni(w, "SK06", 7) || limitWieczoru(w, "SK06", false)) return;
  Tresc t = nowa("S-K-06", "SK06", "W1", 5, false, true);
  teksty(t, "S-K-06", {{"rodzaj", DOP[rodzaj]}});
  t.sprawa = std::string("S-K-06/") + DOP[rodzaj];
  o.push_back(t);
}

void sk07(const Wejscie& w, std::vector<Tresc>& o) {
  // M-18 (start terapii) z 7 ostatnich nocy z terapią; bez M-18 w rekordzie ekran milczy (R-KON-06)
  std::vector<double> st;
  std::vector<double> st14;
  if (w.hist)
    for (auto it = w.hist->rbegin(); it != w.hist->rend(); ++it) {
      if (it->data > w.dzis || !zTerapia(&*it)) continue;
      auto m = w.startyM18.find(it->data);
      if (m == w.startyM18.end()) continue;
      if (st.size() < 7) st.push_back(m->second);
      if (st14.size() < 14) st14.push_back(m->second);
    }
  if (st.size() < 7) return;
  double sd = odchStd(st);
  if (!(sd > SK07_SD)) return;
  if (limitCoDni(w, "SK07", SK07_CO_DNI)) return;
  int med = (int)std::lround(mediana(st14)) + 12 * 60;
  char hh[8];
  snprintf(hh, sizeof hh, "%02d:%02d", (med / 60) % 24, med % 60);
  Tresc t = nowa("S-K-07", "SK07", "W3", 0, false, true);
  teksty(t, "S-K-07", {{"min", calkowita(sd)}, {"hh:mm", hh}});
  t.wartosc = (int)std::lround(sd);
  o.push_back(t);
}

void sk08(const Wejscie& w, std::vector<Tresc>& o) {
  if (!w.airbridge || !w.hist) return;
  const Noc* n = nocZ(w.hist, w.dzis);
  if (n && n->m01 >= SK08_M01) return;  // brak rekordu = noc bez terapii (E-10)
  if (w.hist->empty() || w.hist->front().data > w.dzis) return;  // przed pierwszą nocą w rekordach
  if (limitWieczoru(w, "SK08", false)) return;
  Tresc t = nowa("S-K-08", "SK08", "W2", 0, false, true);
  teksty(t, "S-K-08", {});
  t.wiz = Wiz::KROPKI;
  for (int i = 6; i >= 0; i--) {
    const Noc* x = nocZ(w.hist, dodajDni(w.dzis, -i));
    t.kropki.push_back(x && x->m01 >= SK08_M01 ? 1 : 0);
  }
  t.podpisLewo = "7 nocy";
  t.podpisPrawo = "pusta = noc bez maski";
  o.push_back(t);
}

void sk09(const Wejscie& w, std::vector<Tresc>& o) {
  if (dzienTygodnia(w.dzis) != 5 || !w.hist) return;  // tylko piątek
  std::vector<double> we, rob;
  std::string od = dodajDni(w.dzis, -7 * SK09_WEEKENDY + 1);
  for (auto& n : *w.hist) {
    if (n.data < od || n.data > w.dzis || n.m01 < 0) continue;
    int d = dzienTygodnia(n.data);
    (d == 6 || d == 0 ? we : rob).push_back(n.m01);
  }
  if ((int)we.size() < SK09_GRUPA || (int)rob.size() < SK09_GRUPA) return;
  double mw = mediana(we), mr = mediana(rob);
  if (!(mr - mw >= SK09_ROZNICA)) return;
  if (limitWieczoru(w, "SK09", false)) return;
  Tresc t = nowa("S-K-09", "SK09", "W3", 1, false, true);
  teksty(t, "S-K-09", {{"min", calkowita(mr - mw)}});
  t.wiz = Wiz::SLUPKI;
  t.slupki = {(float)mw, (float)mr};
  t.pary = {{czasMin(mw), "weekend"}, {czasMin(mr), "dni robocze"}};
  t.wartosc = (int)std::lround(mr - mw);
  o.push_back(t);
}

// ---------- rano ----------
// noce z warunkiem w 7 ostatnich z terapią: P95 > SK10_P95 (duży wyciek 24, „od kilku nocy” wykluczające E-23) albo,
// przy `od` ≥ 0, P95 ≥ `od` (od SK10_PROBLEM_P95: ton „problem” w paśmie 15–24; od ustawienia: wariant opisowy, R-KONF-42)
int noceSk10(const std::vector<Noc>* hist, const std::string& dzis, float od = -1) {
  int k = 0, z = 0;
  if (hist)
    for (auto it = hist->rbegin(); it != hist->rend() && z < 7; ++it) {
      if (it->data > dzis || !zTerapia(&*it)) continue;
      z++;
      if (!std::isnan(it->wp95) && (od >= 0 ? it->wp95 >= od - 1e-4f : it->wp95 > SK10_P95)) k++;
    }
  return k;
}
}  // namespace (anonimowa: funkcje publiczne niżej, potem dalej anonimowa)

bool sk10Kilka(const std::vector<Noc>* hist, const std::string& dzis, bool okresInfekcji) {
  return !okresInfekcji && noceSk10(hist, dzis) >= SK10_KILKA;
}

void zastosujSk10Kilka(std::vector<reguly::Wynik>& wyniki, bool kilka) {
  for (auto& r : wyniki) {
    if (r.id != "E-23") continue;
    if (r.wariant == "bez_sk10_kilka") {
      r.oceniona = true;
      r.spelniona = !kilka;
      r.powodNieoceny.clear();
    } else if (r.wariant.empty() && kilka && r.oceniona && r.spelniona) {
      r.spelniona = false;
      r.zmienne["wykluczona_przez"] = "S-K-10/kilka";
      r.zmienne["spelniona_przed_wykluczeniem"] = "tak";
    }
  }
}

namespace {
// R-KONF-42 (5.8.4, R-KON-13): wariant opisowy S-K-10 `Wyciek widoczny`, gdy wariant „problem” nie jest spełniony:
// M-07 P95 ostatniej nocy od wartości ustawienia „Uwaga o masce od” (10–15) do poniżej 15 L/min (od 15 ton „problem”,
// model pasm: 10–15 „istotny, odczyty prawidłowe”), albo P95 co najmniej tyle w ≥ 3 z 7
// nocy z terapią („od kilku nocy”). Ton „informacja”, grupa R8, zawsze ekran danych (bez karty, bez limitu R-KON-14,
// bez wykluczania E-23); typ dziennika `SK10W`, żeby pokazania nie liczyły się do limitu wariantu „problem”. Ostatnia
// noc bez rekordu albo bez P95: brak ekranu (brak danych nie jest ani wyciekiem, ani jego brakiem). Ostatnia noc z
// warunkiem E-20 (M-08, duży wyciek 24 L/min): brak ekranu, bo rano jest już linia E-20 o tym samym wycieku (R4).
void sk10Opisowy(const Wejscie& w, const Noc* n, std::vector<Tresc>& o) {
  if (!n || std::isnan(n->wp95)) return;
  if (warunekE20(*n)) return;
  int od = std::max(UWAGA_O_MASCE_MIN, std::min(UWAGA_O_MASCE_MAX, w.uwagaOMasce));
  int k = noceSk10(w.hist, w.dzis, (float)od);
  bool kilka = k >= SK10_KILKA;
  bool jedna = n->wp95 >= (float)od - 1e-4f && n->wp95 < SK10_PROBLEM_P95 - 1e-4f;
  if (!jedna && !kilka) return;
  Tresc t = nowa("S-K-10", "SK10W", "R8", 0, false, false);
  Z zm = {{"próg", std::to_string(od)}, {"x", std::to_string((int)std::floor(n->wp95 + 1e-4f))},
          {"n", std::to_string(k)}};  // {x} w dół: P95 14,5–14,99 nie daje „15 l/min, bez dużego wycieku”
  t.wariant = kilka ? "opisowy_kilka" : "opisowy";
  t.sprawa = "S-K-10/" + t.wariant;
  teksty(t, "S-K-10." + t.wariant, zm);
  t.wiz = Wiz::LINIA;
  if (w.hist)
    for (auto& x : *w.hist)
      if (x.data <= w.dzis && x.data > dodajDni(w.dzis, -14) && zTerapia(&x)) t.linia.push_back(x.wp95);
  t.prog = (float)od;
  t.podpisLewo = "wyciek, ostatnie 14 nocy";
  t.podpisPrawo = "linia przerywana = " + std::to_string(od) + " l/min";
  t.wartosc = (int)std::lround(n->wp95);
  t.dlaczego = kilka ? "R-KONF-42: P95 od ustawienia w 3 z 7 nocy" : "R-KONF-42: P95 od ustawienia do poniżej 15 L/min";
  o.push_back(t);
}

// S-K-10 w tonie „problem” (5.8.4, model pasm wycieku): od M-07 P95 ≥ SK10_PROBLEM_P95
// (15 L/min, = próg nocy niepewnej) „AHI może być zafałszowane, sprawdź maskę”; powyżej SK10_P95 (24) „terapia nie
// działa”, powód z minutami ponad 24 jak dotąd. „Od kilku nocy”: ≥ 3 z 7 nocy > 24 (jak dotąd: bez limitu R-KON-14,
// wyklucza E-23, sk10Kilka()) albo ≥ 3 z 7 nocy ≥ 15 (pasmo 15–24): wtedy z limitem R-KON-14 liczonym ze wszystkich
// pokazań SK10 i bez wykluczania E-23, żeby stały wyciek 15–24 nie dawał ekranu problemu codziennie, a E-23 (27) nie
// znikał za ekranem z limitem. Noc > 24 zawsze wariantem jednonocnym albo „od kilku nocy” > 24, a jej
// limit R-KON-14 nie liczy pokazań z pasma 15–24 (limitSk10()).
void sk10(const Wejscie& w, std::vector<Tresc>& o) {
  const Noc* n = nocZ(w.hist, w.dzis);
  if (w.okresInfekcji) return;  // R-INF-02
  int k24 = noceSk10(w.hist, w.dzis);
  int k15 = noceSk10(w.hist, w.dzis, SK10_PROBLEM_P95);
  bool jedna = n && !std::isnan(n->wp95) && n->wp95 >= SK10_PROBLEM_P95 - 1e-4f;
  bool jedna24 = jedna && n->wp95 > SK10_P95;
  bool kilka24 = k24 >= SK10_KILKA;
  // noc > 24 bez 3 z 7 nocy > 24 idzie wariantem jednonocnym (powód z minutami), nie „od kilku nocy” 15–24
  bool kilka15 = !kilka24 && !jedna24 && k15 >= SK10_KILKA;
  bool kilka = kilka24 || kilka15;
  if (!jedna && !kilka) {
    sk10Opisowy(w, n, o);  // R-KONF-42: tylko bez wariantu „problem”
    return;
  }
  if (!kilka24 && w.e23) return;  // aktywne E-23 wyklucza wariant jednonocny i „od kilku nocy” w paśmie 15–24
  // klucz sprawy w dzienniku: `S-K-10` noc > 24, `S-K-10/15` noc 15–24, `S-K-10/kilka`, `S-K-10/15/kilka`
  std::string sprawa = kilka24 ? "S-K-10/kilka" : kilka15 ? "S-K-10/15/kilka" : jedna24 ? "S-K-10" : "S-K-10/15";
  if (!kilka24 && limitSk10(w, sprawa)) return;  // R-KON-14
  Tresc t = nowa("S-K-10", "SK10", "R5c", 6, true, true);
  t.sprawa = sprawa;
  Z zm = {{"próg", calkowita(kilka15 ? SK10_PROBLEM_P95 : SK10_P95)}, {"min", n ? calkowita(n->m08) : "–"},
          {"n", std::to_string(kilka15 ? k15 : k24)}, {"x", n ? calkowita(n->wp95) : "–"}};
  if (kilka) {
    t.wariant = "kilka";
    teksty(t, "S-K-10.kilka", zm, kilka15 ? "powod_od" : "powod");
  } else {
    teksty(t, "S-K-10", zm, jedna24 ? "powod" : "powod_p95");
  }
  bool prog24 = kilka24 || (!kilka && jedna24);
  // linia przebiegu wycieku w nocy: przebiegu nie ma w rekordzie (etap 3) → wyciek P95 z 14 nocy z linią progu
  t.wiz = Wiz::LINIA;
  if (w.hist)
    for (auto& x : *w.hist)
      if (x.data <= w.dzis && x.data > dodajDni(w.dzis, -14) && zTerapia(&x)) t.linia.push_back(x.wp95);
  t.prog = prog24 ? SK10_P95 : SK10_PROBLEM_P95;
  t.podpisLewo = "wyciek, ostatnie 14 nocy";
  t.podpisPrawo = "linia przerywana = " + calkowita(t.prog) + " l/min";
  t.wartosc = n ? (int)std::lround(n->wp95) : 0;
  o.push_back(t);
}

void sk12(const Wejscie& w, std::vector<Tresc>& o) {
  const Noc* n = nocZ(w.hist, w.dzis);
  if (!n) return;
  bool wyciek = !std::isnan(n->wmed) && n->wmed > SK12_WMED;
  bool krotka = n->m01 >= SK12_MIN && n->m01 < SK12_MAX_H * 60;
  if (!wyciek && !krotka) return;
  Tresc t = nowa("S-K-12", "SK12", "R4", 0, false, false);
  teksty(t, "S-K-12", {});
  t.wiz = Wiz::PARY;
  t.pary = {{czasMin(n->m01), "czas z maską"}, {calkowita(n->wmed), "wyciek, l/min"}, {"(" + przecinek1(n->ahi) + ")", "AHI"}};
  o.push_back(t);
}

void sk14(const Wejscie& w, std::vector<Tresc>& o) {
  if (w.okresInfekcji || w.e21 || w.e23 || w.pozaDomem.count(w.dzis)) return;  // R-INF-02, R-CZY-12
  const Noc* n = nocZ(w.hist, w.dzis);
  const NocSypialni* s = sypZ(w.sypialnia, w.dzis);
  // minimum wilgotności nocy: rekord sypialni ma medianę nocy (src/sypialnia.h), więc warunek liczy się z mediany
  // (mediana ≥ minimum: ekran rzadziej niż w spec; do decyzji, raport)
  if (!n || !s || std::isnan(s->w) || !(s->w < SK14_WILG) || std::isnan(n->wp95) || !(n->wp95 > SK10_P95)) return;
  if (limitProblemu(w, "SK14")) return;
  Tresc t = nowa("S-K-14", "SK14", "R5c", 4, true, false);
  bool zNorma = w.norma && w.norma->wp95.jest() && n->wp95 - w.norma->wp95.med >= DELTA_M07;  // R-KON-28
  teksty(t, "S-K-14", {{"w", calkowita(s->w)}, {"x", calkowita(n->wp95)}}, zNorma ? "powod_norma" : "powod");
  t.wiz = Wiz::PARY;
  t.pary = {{calkowita(s->w) + "%", "wilgotność"}, {calkowita(n->wp95), "wyciek, l/min"}};
  if (w.norma && w.norma->wp95.jest()) t.pary.push_back({calkowita(w.norma->wp95.med), "zwykle"});
  o.push_back(t);
}

void sk15(const Wejscie& w, std::vector<Tresc>& o) {
  const Noc* n = nocZ(w.hist, w.dzis);
  if (!zTerapia(n) || !(n->m01 < SK15_M01_H * 60)) return;
  const NocSypialni* s = sypZ(w.sypialnia, w.dzis);
  bool syp = s && !std::isnan(s->t) && s->t >= SK15_SYP;
  float mx = NAN;
  if (w.prognoza)
    for (int g = 0; g < 24; g++)
      if (const GodzinaPrognozy* h = w.prognoza->godzina(dodajDni(w.dzis, -1), g))
        if (!std::isnan(h->temp)) mx = std::isnan(mx) ? h->temp : std::max(mx, h->temp);
  bool okno = !std::isnan(mx) && mx >= SK15_OKNO;
  if (!syp && !okno) return;
  Tresc t = nowa("S-K-15", "SK15", "R8", 1, false, false);
  double roz = w.norma && w.norma->m01.jest() ? w.norma->m01.med - n->m01 : NAN;
  bool zNorma = !std::isnan(roz) && roz >= -DELTA_M01_H * 60;  // R-KON-28
  Z z = {{"t", s ? calkowita(s->t) : "–"}, {"czas", czasMin(n->m01)}};
  if (zNorma) z["ile"] = ile(roz);
  teksty(t, "S-K-15", z, zNorma ? "powod_norma" : "powod");
  t.wiz = Wiz::SLUPKI;
  t.slupki.push_back((float)n->m01);
  t.pary.push_back({czasMin(n->m01), "ta noc"});
  if (w.norma && w.norma->m01.jest()) {
    t.slupki.push_back((float)w.norma->m01.med);
    t.pary.push_back({czasMin(w.norma->m01.med), "zwykle"});
  }
  o.push_back(t);
}

void sk16(const Wejscie& w, std::vector<Tresc>& o) {
  if (w.okresInfekcji || w.e14) return;  // R-INF-02; E-14 wyklucza S-K-16 (7.2)
  const Noc* n = nocZ(w.hist, w.dzis);
  if (!n || n->m17 < SK16_M17) return;
  if (limitProblemu(w, "SK16")) return;
  Tresc t = nowa("S-K-16", "SK16", "R5d", 4, true, true);
  bool norma = w.norma && w.norma->m17.jest();
  teksty(t, "S-K-16", {{"n", std::to_string(n->m17)}, {"m", norma ? calkowita(w.norma->m17.med) : "–"}},
         norma ? "powod_norma" : "powod");
  t.wartosc = n->m17;
  o.push_back(t);
}

void sk17(const Wejscie& w, std::vector<Tresc>& o, bool sk30) {
  if (sk30 || !w.norma) return;  // S-K-30 wyklucza S-K-17 (R5a)
  if (w.okresInfekcji) return;   // R-INF-02, 7.3: w okresie infekcji S-K-17 wstrzymany;
                                 // od nocy 15 (okres zakończony, bramka zdjęta) działa normalnie
  bool a = asv(w.tryb);
  const Norma& nm = *w.norma;
  struct Syg {
    const char* id;
    double iloraz;
    double x;
  };
  auto sygnaly = [&](const Noc& n) {
    std::vector<Syg> s;
    if (nm.m13.jest() && !std::isnan(n.m13) && n.m13 > nm.m13.p90 && n.m13 - nm.m13.med >= DELTA_M13)
      s.push_back({"S-K-17.oddech", (n.m13 - nm.m13.med) / DELTA_M13, n.m13 - nm.m13.med});
    const NormaMetryki& nc = a ? nm.ep95 : nm.cp95;
    float c = cisnienie(n, a);
    double dc = a ? DELTA_M06P : DELTA_M05P;
    if (nc.jest() && !std::isnan(c) && c > nc.p90 && c - nc.med >= dc)
      s.push_back({a ? "S-K-17.epap" : "S-K-17.cisnienie", (c - nc.med) / dc, c - nc.med});
    if (nm.wp95.jest() && !std::isnan(n.wp95) && n.wp95 > nm.wp95.p90 && n.wp95 - nm.wp95.med >= DELTA_M07)
      s.push_back({"S-K-17.wyciek", (n.wp95 - nm.wp95.med) / DELTA_M07, n.wp95 - nm.wp95.med});
    // M-14, M-15: od 11.11.2026 w rekordzie nocy (`noce` wersja 5) i z normą, ale jeszcze nie jako sygnały S-K-17
    // (brak tekstów powodu w teksty.csv; do decyzji właściciela projektu): pomijane jak sygnał bez danych
    return s;
  };
  std::vector<const Noc*> v = wazneDo(w, w.dzis);
  if ((int)v.size() < SK17_NOCE || v.back()->data != w.dzis) return;
  std::vector<Syg> ost;
  for (size_t i = v.size() - (size_t)SK17_NOCE; i < v.size(); i++) {
    std::vector<Syg> s = sygnaly(*v[i]);
    if (s.size() < 2) return;  // co najmniej 2 z 5 sygnałów (S-K-17.sygnaly = 2/5)
    ost = s;
  }
  if (limitProblemu(w, "SK17")) return;
  std::sort(ost.begin(), ost.end(), [](const Syg& x, const Syg& y) { return x.iloraz > y.iloraz; });
  Tresc t = nowa("S-K-17", "SK17", "R5b", 5, true, false);
  teksty(t, "S-K-17", {}, "", "krok");
  std::string p;
  for (size_t i = 0; i < ost.size() && i < 2; i++) {
    std::string f = tekst(ost[i].id, {{"x", przecinek1(ost[i].x)}});
    p += i == 0 ? wielka(f) : ", " + mala(f);
  }
  t.t.powod = p + ".";
  t.wiz = Wiz::KOLKA;
  struct {
    const char* n;
    const char* id;
  } lista[] = {{"oddechy", "S-K-17.oddech"}, {a ? "EPAP" : "ciśnienie", a ? "S-K-17.epap" : "S-K-17.cisnienie"},
               {"wyciek", "S-K-17.wyciek"}};
  for (auto& l : lista) {
    bool odb = false;
    for (auto& s : ost) odb = odb || std::string(s.id) == l.id;
    t.kolka.push_back(l.n);
    t.kolkaPelne.push_back(!odb);
  }
  t.podpisPrawo = "pełne = jak zwykle, puste = odbiega";
  o.push_back(t);
}

void sk27(const Wejscie& w, std::vector<Tresc>& o) {
  if (!w.hist || w.hist->empty()) return;
  const std::string& pierwsza = w.hist->front().data;
  if (dniMiedzy(pierwsza, w.dzis) > SK27_OKRES) return;  // R-KON-21
  if (w.dziennik)
    for (auto& x : *w.dziennik)
      if (x.typ == "SK27" && x.data < w.dzis) return;  // kamień raz (R-KON-16)
  // pierwszy raz w historii: w ostatnich 30 dniach ≥ 70% nocy z M-01 ≥ 4 h; dzień, w którym próg był osiągnięty
  // wcześniej, pokazał już ekran (wpis w dzienniku) albo go przegapił: sprawdzam też dzień wcześniej
  auto ile4h = [&](const std::string& d) {
    int k = 0;
    for (auto& n : *w.hist)
      if (n.data > dodajDni(d, -30) && n.data <= d && n.m01 >= 4 * 60) k++;
    return k;
  };
  if (dniMiedzy(pierwsza, w.dzis) < 29) return;  // 30 dni historii
  int k = ile4h(w.dzis);
  if (k * 100 < SK27_ODSETEK * 30) return;
  bool dzisWpis = dzisJest(w, "SK27");
  if (!dzisWpis && dniMiedzy(pierwsza, dodajDni(w.dzis, -1)) >= 29 && ile4h(dodajDni(w.dzis, -1)) * 100 >= SK27_ODSETEK * 30)
    return;  // próg osiągnięty już wcześniej („pierwszy raz”)
  Tresc t = nowa("S-K-27", "SK27", "R7", 0, false, true);
  teksty(t, "S-K-27", {{"n", std::to_string(k)}});
  t.wiz = Wiz::KROPKI;
  for (int i = 29; i >= 0; i--) {
    const Noc* x = nocZ(w.hist, dodajDni(w.dzis, -i));
    t.kropki.push_back(x && x->m01 >= 4 * 60 ? 1 : 0);
  }
  t.podpisLewo = "30 nocy";
  t.podpisPrawo = "pełna = ponad 4 h";
  o.push_back(t);
}

void sk29(const Wejscie& w, std::vector<Tresc>& o) {
  const Noc* n = nocZ(w.hist, w.dzis);
  if (!n || n->m01 < 4 * 60 || !w.hist || w.hist->front().data >= w.dzis) return;
  int bez = 0;
  for (std::string d = dodajDni(w.dzis, -1); d >= w.hist->front().data; d = dodajDni(d, -1)) {
    const Noc* x = nocZ(w.hist, d);
    if (zTerapia(x)) break;
    bez++;
  }
  if (bez < SK29_NOCE_BEZ) return;
  Tresc t = nowa("S-K-29", "SK29", "R7", 2, false, true);
  teksty(t, "S-K-29", {{"n", std::to_string(bez)}, {"czas", czasMin(n->m01)}});
  t.sprawa = "S-K-29/" + w.dzis;  // raz po każdej przerwie (R-KON-16)
  t.wiz = Wiz::KROPKI;
  for (int i = 7; i >= 0; i--) {
    const Noc* x = nocZ(w.hist, dodajDni(w.dzis, -i));
    t.kropki.push_back(zTerapia(x) ? 1 : 0);
  }
  t.podpisLewo = "8 nocy";
  t.podpisPrawo = "pusta = przerwa";
  o.push_back(t);
}

void sk30(const Wejscie& w, std::vector<Tresc>& o, bool& jest) {
  jest = false;
  if (!w.hist || !w.norma || w.okresInfekcji) return;  // R-INF-09
  if (w.okresy && infekcja::nocWOkresie(*w.okresy, w.dzis)) return;
  if (w.okresy && infekcja::karencja(*w.okresy, w.dzis)) return;  // po 14 nocach zwykła ścieżka S-K-17
  infekcja::WynikSK30 r = infekcja::ocenSK30(*w.hist, *w.norma, w.dzis, asv(w.tryb));
  if (!r.warunek) return;
  jest = true;
  Tresc t = nowa("S-K-30", "SK30", "R5a", 0, false, false);
  teksty(t, "S-K-30", {{"x", przecinek1(r.x)}, {"norma", przecinek1(r.norma)}}, "powod", "");
  t.pytanie = tekst("S-K-30.pytanie");
  t.t.krok = t.pytanie + "  ◀ nie   ▶ tak";
  t.wiz = Wiz::LINIA;
  t.linia = r.linia;
  t.prog = r.norma;
  t.podpisLewo = "oddechy na minutę, 14 nocy";
  t.podpisPrawo = "linia przerywana = zwykle";
  t.stopkaPrawo = r.sygnal;
  t.sprawa = "S-K-30/" + r.pierwszaNoc;
  o.push_back(t);
}

void sk31(const Wejscie& w, std::vector<Tresc>& o) {
  if (!w.sprzet) return;
  sprzet::WejscieSprzetu ws;
  ws.dzis = w.dzis;
  ws.hist = w.hist;
  ws.norma = w.norma;
  ws.el = w.sprzet;
  ws.okresInfekcji = w.okresInfekcji;
  ws.dziennik = w.dziennik;
  sprzet::WynikSK31 r = sprzet::ocenSK31(ws);
  if (!r.warunek || r.limit) return;
  Tresc t = nowa("S-K-31", "SK31", "R5c", 0, true, true);
  teksty(t, "S-K-31", {{"m", std::to_string(r.miesiecy)}, {"t", std::to_string(r.tygodni)}}, "powod", "krok", "tytul");
  t.sprawa = r.sprawa;
  t.wiz = Wiz::LINIA;
  t.linia = r.linia;
  t.prog = SK10_P95;
  t.podpisLewo = "wyciek, ostatnie 28 nocy";
  t.podpisPrawo = "linia przerywana = " + calkowita(SK10_P95) + " l/min";
  t.wartosc = r.element;
  o.push_back(t);
}

void sk32(const Wejscie& w, std::vector<Tresc>& o) {
  if (!w.sprzet || rozruch(w)) return;  // odkrycie: w rozruchu wyłączone (7.3)
  sprzet::WejscieSprzetu ws;
  ws.dzis = w.dzis;
  ws.hist = w.hist;
  ws.norma = w.norma;
  ws.el = w.sprzet;
  ws.dziennik = w.dziennik;
  sprzet::WynikSK32 r = sprzet::ocenSK32(ws);
  if (!r.warunek || limitOdkrycia(w, "SK32", "")) return;
  Tresc t = nowa("S-K-32", "SK32", "R8", 3, false, true);
  teksty(t, "S-K-32", {{"x", calkowita(r.po)}, {"y", calkowita(r.przed)}, {"m", std::to_string(r.okresMies)}}, "powod",
         "krok", "tytul");
  t.sprawa = r.sprawa;
  t.wiz = Wiz::SLUPKI;
  t.slupki = {r.przed, r.po};
  t.pary = {{calkowita(r.przed), "przed wymianą"}, {calkowita(r.po), "po wymianie"}};
  o.push_back(t);
}

}  // namespace

// ---------- odchylenia od normy (ADR-0039, S-K-40…S-K-47) ----------
namespace {

float wartPs(const Noc& n) { return n.psmed; }
float wartM13(const Noc& n) { return n.m13; }
float wartCsrp(const Noc& n) { return n.m01 > 0 && !std::isnan(n.csr) ? n.csr / (float)n.m01 * 100.0f : NAN; }
float wartM10p(const Noc& n) { return n.m10p; }
float wartM14(const Noc& n) { return n.m14; }
float wartM15(const Noc& n) { return n.m15; }

std::string dwaMiejsca(double v) {  // 0,054 → "0,05" (M-10, skala 0–1)
  if (std::isnan(v)) return "–";
  char b[16];
  snprintf(b, sizeof b, "%.2f", v);
  std::string t = b;
  for (auto& c : t)
    if (c == '.') c = ',';
  return t;
}

enum class Format : unsigned char { JEDNO_MIEJSCE, CALKOWITA, DWA_MIEJSCA };
std::string formatuj(double v, Format f) {
  if (f == Format::CALKOWITA) return calkowita(v);
  if (f == Format::DWA_MIEJSCA) return dwaMiejsca(v);
  return przecinek1(v);
}

// Rodzaj `sk_odchylenie` (rodzaje-sk.csv): jedna karta = sygnał (stan, norma, wartość nocy) i kierunek.
// Wiersze w kolejności ID (remis najdłużej trwającego stanu: niższe ID). M-14 i M-15 (S-K-41…S-K-44) ze zmiany
// „wentylacja minutowa” (rekord nocy w formacie `noce` 5).
struct KartaOdchylenia {
  const char* id;
  const char* typ;
  StanMetryki Stany::*stan;
  NormaMetryki Norma::*norma;
  float (*wart)(const Noc&);
  Format format;
};
const KartaOdchylenia KARTY_ODCHYLEN[] = {
    {"S-K-40", "SK40", &Stany::ps, &Norma::psmed, wartPs, Format::JEDNO_MIEJSCE},        // PS ↑ (M-28)
    {"S-K-41", "SK41", &Stany::m15g, &Norma::m15, wartM15, Format::JEDNO_MIEJSCE},      // wentylacja ↑ (M-15)
    {"S-K-42", "SK42", &Stany::m15d, &Norma::m15, wartM15, Format::JEDNO_MIEJSCE},      // wentylacja ↓
    {"S-K-43", "SK43", &Stany::m14d, &Norma::m14, wartM14, Format::CALKOWITA},          // objętość oddechowa ↓ (M-14)
    {"S-K-44", "SK44", &Stany::m14g, &Norma::m14, wartM14, Format::CALKOWITA},          // objętość oddechowa ↑
    {"S-K-45", "SK45", &Stany::m13, &Norma::m13, wartM13, Format::JEDNO_MIEJSCE},        // oddechy ↑ (M-13)
    {"S-K-46", "SK46", &Stany::csr, &Norma::csrp, wartCsrp, Format::JEDNO_MIEJSCE},      // oddychanie okresowe ↑
    {"S-K-47", "SK47", &Stany::m10, &Norma::m10p, wartM10p, Format::DWA_MIEJSCA},         // ograniczenie przepływu ↑
};

bool kandydatJest(const std::vector<Tresc>& o, const char* id) {
  for (auto& t : o)
    if (t.id == id) return true;
  return false;
}

}  // namespace

void odchylenia(const Wejscie& w, std::vector<Tresc>& o) {
  if (!w.stany || !w.norma || !w.hist) return;
  const Noc* ost = nocZ(w.hist, w.dzis);
  const KartaOdchylenia* wybor = nullptr;
  int najdluzej = -1;
  bool wyborDzis = false;
  for (const KartaOdchylenia& k : KARTY_ODCHYLEN) {
    const StanMetryki& st = w.stany->*(k.stan);
    if (!st.aktywny || st.nocy < 1) continue;
    // norma stanu: zamrożona przy wejściu (R-NOR-22), bez niej dzisiejsza; bez normy brak karty (R-NOR-10, R-NOR-07)
    if (!(st.zamrozona.jest() ? st.zamrozona : w.norma->*(k.norma)).jest()) continue;
    std::string id = k.id;
    if (id == "S-K-45" && (w.okresInfekcji || kandydatJest(o, "S-K-30") || kandydatJest(o, "S-K-17"))) continue;
    if (id == "S-K-46" && (!asv(w.tryb) || !ost || std::isnan(ost->csr))) continue;  // ADR-0037: tylko z wartością M-09
    if (limitCoDni(w, k.typ, ODCHYLENIE_CO_DNI)) continue;  // R-KON-64: ta sama karta wraca po 7 dniach
    bool dzis = dzisJest(w, k.typ);
    if (wyborDzis && !dzis) continue;
    if (dzis && !wyborDzis) {  // pokazana dziś: zostaje przy ponownej ocenie tego dnia (jedna karta dziennie)
      wybor = &k;
      najdluzej = st.nocy;
      wyborDzis = true;
      continue;
    }
    if (st.nocy > najdluzej) {
      wybor = &k;
      najdluzej = st.nocy;
    }
  }
  if (!wybor) return;
  // jedna karta dziennie: inna karta odchylenia pokazana dziś blokuje nową
  if (!wyborDzis)
    for (const KartaOdchylenia& k : KARTY_ODCHYLEN)
      if (dzisJest(w, k.typ)) return;
  const StanMetryki& st = w.stany->*(wybor->stan);
  const NormaMetryki& nm = st.zamrozona.jest() ? st.zamrozona : w.norma->*(wybor->norma);  // R-NOR-22
  std::vector<double> ost5;
  std::vector<const Noc*> v = wazneDo(w, w.dzis);
  for (auto it = v.rbegin(); it != v.rend() && ost5.size() < 5; ++it) {
    float x = wybor->wart(**it);
    if (!std::isnan(x)) ost5.push_back(x);
  }
  if (ost5.empty()) return;
  Tresc t = nowa(wybor->id, wybor->typ, "R5e", 0, false, false);
  std::string pref = wybor->id;
  Z z = {{"x", formatuj(mediana(ost5), wybor->format)}, {"norma", formatuj(nm.med, wybor->format)}};
  teksty(t, pref, z, "powod", st.nocy >= ODCHYLENIE_KROK_LEKARZ ? "krok_lekarz" : "krok");
  t.sprawa = pref + "/" + st.od;
  t.wartosc = st.nocy;
  t.wiz = Wiz::LINIA;
  for (int i = 13; i >= 0; i--) {
    const Noc* n = nocZ(w.hist, dodajDni(w.dzis, -i));
    t.linia.push_back(n && n->wazna ? wybor->wart(*n) : NAN);
  }
  t.prog = (float)nm.med;
  t.podpisLewo = tekst(pref + ".podpis_lewo", z);
  t.podpisPrawo = tekst(pref + ".podpis_prawo", z);
  t.dlaczego = "stan odchylenia (ADR-0039)";
  o.push_back(t);
}

std::string liniaDobrejZmiany(const Stany& st, bool asvTryb) {
  struct D {
    const StanMetryki* s;
    const char* nazwa;  // id tekstu wskaźnika
    bool tylkoAsv;
  };
  const D lista[] = {{&st.dobraAhi, "R-KON-68.ahi", false},   {&st.dobraPs, "R-KON-68.ps", true},
                     {&st.dobraWp95, "R-KON-68.wyciek", false}, {&st.dobraM17, "R-KON-68.przerwy", false},
                     {&st.dobraM10, "R-KON-68.przeplyw", false}, {&st.dobraM01, "R-KON-68.czas", false}};
  const D* wybor = nullptr;
  for (const D& d : lista) {
    if (!d.s->aktywny || d.s->odPierwszej < 1 || (d.tylkoAsv && !asvTryb)) continue;
    if (!wybor || d.s->odPierwszej > wybor->s->odPierwszej) wybor = &d;
  }
  if (!wybor) return "";
  return tekst("R-KON-68.linia", {{"wskaznik", tekst(wybor->nazwa)},
                                  {"kierunek", tekst(wybor->s->wyzej ? "R-KON-68.wyzej" : "R-KON-68.nizej")},
                                  {"n", std::to_string(wybor->s->odPierwszej)}});
}

int nocyWaznych(const Wejscie& w) { return w.hist ? liczbaNocyWaznych(*w.hist, w.dzis) : 0; }
bool rozruch(const Wejscie& w) { return nocyWaznych(w) < ROZRUCH_NOCY; }

std::vector<Tresc> trendyTygodnia(const Wejscie& w0, const std::string& nd) {
  std::vector<Tresc> o;
  if (!w0.hist || rozruch(w0)) return o;
  Wejscie w = w0;
  w.dzis = nd;
  bool a = asv(w.tryb);
  std::vector<const Noc*> v = wazneDo(w, nd);
  // S-K-19, wariant tymczasowy (R-KON-30): mediana M-10 P95 z 7 nocy ważnych ≥ P90 normy i ≥ mediana normy + 0,1;
  // norma M-10 P95 (4.4: do 30 nocy ważnych z 90 dni przed oknem) tylko z nocy z ustawieniami ostatniej nocy
  if (w.giTymczasowy && v.size() >= 7) {
    const std::string& ust = v.back()->ustawienia;
    std::vector<double> ok, baza;
    bool zmiana = false;
    for (size_t i = v.size() - 7; i < v.size(); i++) {
      if (!std::isnan(v[i]->m10p)) ok.push_back(v[i]->m10p);
      if (v[i]->ustawienia != ust) zmiana = true;
    }
    std::string granica = dodajDni(v[v.size() - 7]->data, -90);
    for (size_t i = v.size() - 7; i-- > 0 && baza.size() < 30;) {
      if (v[i]->data < granica) break;
      if (v[i]->ustawienia != ust) { zmiana = zmiana || baza.size() < (size_t)SK19_PO_ZMIANIE; break; }
      if (!std::isnan(v[i]->m10p)) baza.push_back(v[i]->m10p);
    }
    if (!zmiana && ok.size() == 7 && baza.size() >= 14) {
      double m = mediana(ok), med = mediana(baza), p90 = percentyl(baza, 90);
      if (m >= p90 && m >= med + SK19_DODATEK) {
        Tresc t = nowa("S-K-19", "SK19", "R8", 5, false, false);
        teksty(t, "S-K-19", {});
        t.wiz = Wiz::LINIA;
        for (auto* n : v)
          if (n->data > dodajDni(nd, -42)) t.linia.push_back(n->m10p);
        t.prog = (float)med;
        t.podpisLewo = "spłaszczenie wdechów, 6 tygodni";
        t.podpisPrawo = "linia przerywana = zwykle";
        t.sprawa = "S-K-19/" + nd;
        o.push_back(t);
      }
    }
  }
  // S-K-20: ekran tygodnia L-07 (w wersji beta L-07 jest „tylko raport”, ADR-0029, więc bez ekranu)
  if (w.l07Aktywne && w.norma && v.size() >= 7) {
    const NormaMetryki& nc = a ? w.norma->ep95 : w.norma->cp95;
    std::vector<double> x;
    for (size_t i = v.size() - 7; i < v.size(); i++) {
      float c = cisnienie(*v[i], a);
      if (!std::isnan(c)) x.push_back(c);
    }
    if (nc.jest() && !x.empty()) {
      Tresc t = nowa("S-K-20", "SK20", "R8", 6, false, false);
      Z z = {{"x", przecinek1(mediana(x) - nc.med)}};
      teksty(t, a ? "S-K-20.asv" : "S-K-20", z);
      if (a) t.t.etykieta = tekst("S-K-20.etykieta");
      if (a) t.t.krok = tekst("S-K-20.krok");
      t.wiz = Wiz::LINIA;
      for (size_t i = v.size() > 90 ? v.size() - 90 : 0; i < v.size(); i++) t.linia.push_back(cisnienie(*v[i], a));
      t.prog = (float)nc.med;
      t.sprawa = "S-K-20/" + nd;
      o.push_back(t);
    }
  }
  // S-K-21: odkrycie, 60 nocy, ≥ 5 nocy ważnych w każdej grupie, różnica median M-01 ≥ 30 min
  struct Wariant {
    const char* id;
    int grupa;  // 1 = z warunkiem, 0 = bez, -1 = brak danych
  };
  auto porownaj = [&](const char* war, auto grupa) {
    std::vector<double> z, b;
    for (auto* n : v) {
      if (n->data <= dodajDni(nd, -SK21_OKNO) || n->m01 < 0) continue;
      int g = grupa(*n);
      if (g == 1) z.push_back(n->m01);
      if (g == 0) b.push_back(n->m01);
    }
    if ((int)z.size() < SK21_GRUPA || (int)b.size() < SK21_GRUPA) return std::make_pair(std::string(), 0.0);
    double d = mediana(b) - mediana(z);
    return d >= SK21_ROZNICA ? std::make_pair(std::string(war), d) : std::make_pair(std::string(), 0.0);
  };
  std::vector<std::pair<std::string, double>> war;
  war.push_back(porownaj("co2", [&](const Noc& n) {
    const NocSypialni* s = sypZ(w.sypialnia, n.data);
    return s && !std::isnan(s->co2) ? (s->co2 > CO2_PROG_NOCY ? 1 : 0) : -1;
  }));
  war.push_back(porownaj("upal", [&](const Noc& n) {
    const NocSypialni* s = sypZ(w.sypialnia, n.data);
    return s && !std::isnan(s->t) ? (s->t >= SK15_SYP ? 1 : 0) : -1;
  }));
  // wariant pyłek: panel nie zapisuje pyłku nocy (historia prognoz), więc grupy są puste
  war.push_back(porownaj("weekend", [&](const Noc& n) {
    int d = dzienTygodnia(n.data);
    return d == 6 || d == 0 ? 1 : 0;
  }));
  std::pair<std::string, double> naj;
  for (auto& x : war)
    if (!x.first.empty() && x.second > naj.second) naj = x;
  if (!naj.first.empty() && !limitOdkrycia(w0, "SK21", "S-K-21/" + naj.first)) {
    Tresc t = nowa("S-K-21", "SK21", "R8", 7, false, true);
    t.wariant = naj.first;
    t.sprawa = "S-K-21/" + naj.first;
    teksty(t, "S-K-21." + naj.first, {{"min", calkowita(naj.second)}});
    t.t.etykieta = tekst("S-K-21.etykieta");
    t.wiz = Wiz::SLUPKI;
    o.push_back(t);
  }
  return o;
}

bool problemNaEkranieRano(const std::vector<Tresc>& v) {
  for (const Tresc& t : v) {
    if (t.doLogu || t.id == "S-K-30") continue;
    return t.problem && t.id != "S-K-31" && ranga(t.grupa) <= ranga("R5d");
  }
  return false;
}

std::vector<Tresc> rano(const Wejscie& w) {
  std::vector<Tresc> o;
  if (!dataOk(w.dzis)) return o;
  bool r = rozruch(w);
  sk12(w, o);
  bool k30 = false;
  if (!r) sk30(w, o, k30);
  if (!r) sk17(w, o, k30);
  sk31(w, o);
  sk14(w, o);
  sk10(w, o);
  sk16(w, o);
  sk27(w, o);
  sk29(w, o);
  sk15(w, o);
  sk32(w, o);
  if (!r) odchylenia(w, o);  // ADR-0039: po S-K-30 i S-K-17 (S-K-45 je sprawdza), grupa R5e
  if (dzienTygodnia(w.dzis) == 1)  // poniedziałek: trendy tygodnia T3 w R8 (R-KON-27)
    for (auto& t : trendyTygodnia(w, dodajDni(w.dzis, -1))) o.push_back(t);
  for (auto& t : o) t.doLogu = probny(w, t.id);
  std::vector<Tresc> wy;
  for (auto& t : o)
    if (!limitDoby(w, t.typ)) wy.push_back(t);
  std::stable_sort(wy.begin(), wy.end(), [](const Tresc& a, const Tresc& b) {
    return ranga(a.grupa) * 10 + a.kolejnosc < ranga(b.grupa) * 10 + b.kolejnosc;
  });
  return wy;
}

std::vector<Tresc> wieczor(const Wejscie& w) {
  std::vector<Tresc> o;
  if (!dataOk(w.dzis)) return o;
  if (w.prosbaDzis || w.wieczorowZKrokiem7 >= 4) return o;  // R-PIE-10, R-PIE-12
  sk05(w, o);
  sk03(w, o);
  sk04(w, o);
  sk06(w, o);
  sk08(w, o);
  sk07(w, o);
  sk09(w, o);
  for (auto& t : o) t.doLogu = probny(w, t.id);
  std::vector<Tresc> wy;
  for (auto& t : o)
    if (!limitDoby(w, t.typ)) wy.push_back(t);
  std::stable_sort(wy.begin(), wy.end(), [](const Tresc& a, const Tresc& b) {
    return ranga(a.grupa) * 10 + a.kolejnosc < ranga(b.grupa) * 10 + b.kolejnosc;
  });
  return wy;
}

bool tydzien(const Wejscie& w, Tresc& t) {
  if (!w.hist || !dataOk(w.dzis)) return false;
  int rek = 0, n4 = 0, zt = 0;
  double suma = 0;
  std::vector<int> kr;
  std::vector<double> m01;
  for (int i = 6; i >= 0; i--) {
    const Noc* n = nocZ(w.hist, dodajDni(w.dzis, -i));
    kr.push_back(n && n->m01 >= 4 * 60 ? 1 : 0);
    if (!n) continue;
    rek++;
    if (n->m01 >= 4 * 60) n4++;
    if (zTerapia(n)) {
      zt++;
      suma += n->m01;
      m01.push_back(n->m01);
    }
  }
  if (rek < SK18_REKORDY) return false;
  t = nowa("S-K-18", "SK18", "T2", 0, false, true);
  Z z = {{"n", std::to_string(n4)}, {"czas", zt ? czasMin(suma / zt) : "–"}};
  teksty(t, "S-K-18", z, zt ? "powod" : "powod_bez", "");  // bez nocy z terapią: bez średniej „–” (5.8.4)
  // krok: pierwszy spełniony składnik (R-KON-23); metryka bez normy pomijana
  auto kzn = [&](auto war) {
    int k = 0;
    for (int i = 0; i < 7; i++)
      if (const Noc* n = nocZ(w.hist, dodajDni(w.dzis, -i)))
        if (war(*n)) k++;
    return k;
  };
  const Norma* nm = w.norma;
  std::string krok;
  if (w.sennoscTak) krok = "S-K-18.krok_sennosc";
  else if (nm && nm->wp95.jest() && kzn([&](const Noc& n) { return !std::isnan(n.wp95) && n.wp95 > nm->wp95.p90; }) >= 3)
    krok = "S-K-18.krok_wyciek";
  else if (nm && nm->m17.jest() && kzn([&](const Noc& n) { return n.m17 >= 0 && n.m17 > nm->m17.med; }) >= 3)
    krok = "S-K-18.krok_przerwy";
  else {
    std::vector<double> st;
    for (int i = 0; i < 7; i++) {
      auto m = w.startyM18.find(dodajDni(w.dzis, -i));
      if (m != w.startyM18.end()) st.push_back(m->second);
    }
    // pora tylko przy niepełnym tygodniu: 7 z 7 nocy ponad 4 h bez pouczania o porze (5.8.4, zmiana 08.11.2026)
    if (n4 < 7 && st.size() >= 2 && odchStd(st) > SK07_SD) krok = "S-K-18.krok_pora";
    // czas z maską także przy mniej niż 5 z 7 nocy ponad 4 h [PRÓG]: słaby tydzień nie dostaje „Tak trzymaj”
    else if (n4 < SK18_CZAS_N4 || (!m01.empty() && mediana(m01) < 6 * 60)) krok = "S-K-18.krok_czas";
    else if (w.uspokojenie) krok = "S-K-18.krok_nic";
    else if (w.liniaZdrowiaNaEkranie) krok = "S-K-18.krok_linia";
    else if (w.tylkoRegulyBezOceny) krok = "S-K-18.krok_reguly";  // R-EKR-32, 5.4.13 pkt 7
    else if (w.tylkoOcenyWylaczone) krok = "S-K-18.krok_oceny";   // R-EKR-32, 5.4.13 pkt 5
    else if (w.brakDanychWyciek) krok = "S-K-18.krok_dane_wyciek";  // brak ocen z powodu wycieku, nie liczby nocy
    else if (w.tylkoBrakDanych) krok = "S-K-18.krok_dane";        // brak danych, nie awaria
    else krok = "S-K-18.krok_sprawy";
  }
  t.t.krok = tekst(krok);
  t.sprawa = "S-K-18/" + w.dzis;
  t.wiz = Wiz::KROPKI;
  t.kropki = kr;
  t.podpisLewo = "pn – nd";
  t.podpisPrawo = "pełna = ponad 4 h";
  return true;
}

int ranga(const std::string& g) {
  static const char* const RANO[] = {"R0",  "R1",  "R2", "R3", "R4", "R5a", "R5b",
                                     "R5c", "R5d", "R5e", "R6", "R7", "R8", "R9"};
  for (int i = 0; i < 14; i++)
    if (g == RANO[i]) return i;
  if (g == "W1") return 20;
  if (g == "W2") return 21;
  if (g == "W3") return 22;
  if (g == "T2") return 30;
  return 99;
}

int rangaTypu(const std::string& typ) {
  if (typ == "S5" || typ == "S10") return ranga("R1");
  if (typ == "S7") return ranga("R6");
  if (typ == "KM") return ranga("R7");
  if (typ == "SK13" || typ == "S1" || (typ.size() > 2 && typ.compare(0, 2, "O-") == 0)) return ranga("R8");
  if (typ == "S6" || typ == "S2A" || typ == "S2B" || typ == "S2C" || typ == "S2D" || typ == "S8") return ranga("R9");
  return 0;  // nieznany: zostaje (lepiej nie zasłonić)
}

bool typWieczorny(const std::string& t) { return t.size() == 4 && t.compare(0, 3, "SK0") == 0 && t[3] >= '3' && t[3] <= '9'; }
bool typB16(const std::string& t) {
  static const char* const T[] = {"SK03", "SK04", "SK05", "SK06", "SK07", "SK08", "SK09", "SK10", "SK10W", "SK12", "SK14",
                                  "SK15", "SK16", "SK17", "SK18", "SK19", "SK20", "SK21", "SK27", "SK29", "SK30",
                                  "SK31", "SK32", "SK40", "SK41", "SK42", "SK43", "SK44", "SK45", "SK46", "SK47"};
  for (auto x : T)
    if (t == x) return true;
  return false;
}
const WpisHumoru* wpisDzis(const std::vector<WpisHumoru>& d, const std::string& dzis, bool wieczorny) {
  const WpisHumoru* o = nullptr;
  for (auto& x : d)
    if (x.data == dzis && typB16(x.typ) && typWieczorny(x.typ) == wieczorny) o = &x;
  return o;
}

// ---------- rysowanie ----------
namespace {
const int X = 28, XP = 772, W = XP - X;


void przerywana(Plotno& p, int x0, int x1, int y, bool cz) {
  for (int x = x0; x < x1; x += 14) p.prostokat(x, y - 1, std::min(8, x1 - x), 2, cz);
}
void gruba(Plotno& p, int x0, int y0, int x1, int y1, bool cz) {
  for (int d = -1; d <= 1; d++) p.linia(x0, y0 + d, x1, y1 + d, cz);
}

void wizualizacja(Plotno& p, const Tresc& t, bool cz) {
  const int Y0 = 262, Y1 = 335;  // pas wizualizacji szablonu A (artboardy SK10, SK13: 262–335, podpisy 376)
  switch (t.wiz) {
    case Wiz::LINIA: {
      std::vector<float> v;
      v.reserve(t.linia.size());
      for (float x : t.linia) v.push_back(x);
      int n = 0;
      float lo = NAN, hi = NAN;
      for (float x : v)
        if (!std::isnan(x)) { lo = std::isnan(lo) ? x : std::min(lo, x); hi = std::isnan(hi) ? x : std::max(hi, x); n++; }
      if (n < 2) break;
      if (!std::isnan(t.prog)) { lo = std::min(lo, t.prog); hi = std::max(hi, t.prog); }
      float m = (hi - lo) * 0.15f + 0.5f;
      lo -= m;
      hi += m;
      auto py = [&](float x) { return Y1 - (int)std::lround((x - lo) / (hi - lo) * (Y1 - Y0)); };
      auto px = [&](size_t i) { return X + (int)std::lround((double)i * W / (double)(v.size() - 1)); };
      int px0 = -1, py0 = 0;
      for (size_t i = 0; i < v.size(); i++) {
        if (std::isnan(v[i])) { px0 = -1; continue; }
        if (px0 >= 0) gruba(p, px0, py0, px(i), py(v[i]), cz);
        px0 = px(i);
        py0 = py(v[i]);
      }
      if (!std::isnan(t.prog)) przerywana(p, X, XP, py(t.prog), cz);
      break;
    }
    case Wiz::KROPKI: {
      int n = (int)t.kropki.size();
      if (n < 1) break;
      int r = n > 10 ? 8 : 11;
      for (int i = 0; i < n; i++) {
        int cx = n == 1 ? X + r : X + r + (int)std::lround((double)i * (W - 2 * r) / (n - 1));
        p.kolo(cx, 318, r, t.kropki[(size_t)i] == 1, cz);
        if (t.kropki[(size_t)i] != 1) p.kolo(cx, 318, r - 2, true, !cz);
      }
      break;
    }
    case Wiz::SLUPKI: {
      float mx = 0;
      for (float x : t.slupki)
        if (!std::isnan(x)) mx = std::max(mx, x);
      for (size_t i = 0; i < t.slupki.size() && i < 3; i++) {
        int x = X + 250 * (int)i;
        if (!std::isnan(t.slupki[i]) && mx > 0) {
          int h = (int)std::lround(t.slupki[i] / mx * 60);
          p.prostokat(x, 300 - h, 120, h, cz);
        }
        if (i < t.pary.size()) {
          tekst(p, F_P24B, x + 132, 300, t.pary[i].w, cz);
          tekst(p, F_P22B, x, 336, t.pary[i].p, cz);
        }
      }
      if (t.slupki.empty())  // wariant bez wartości słupków: same pary
        for (size_t i = 0; i < t.pary.size() && i < 3; i++) {
          tekst(p, F_P28B, X + 248 * (int)i, 300, t.pary[i].w, cz);
          tekst(p, F_P22B, X + 248 * (int)i, 336, t.pary[i].p, cz);
        }
      break;
    }
    case Wiz::PARY: {
      int x = X;
      for (auto& f : t.pary) {
        tekst(p, F_P34B, x, 320, f.w, cz);
        tekst(p, F_P24B, x, 358, f.p, cz);
        x += 248;
      }
      return;  // pary mają własne podpisy
    }
    case Wiz::KOLKA: {
      for (size_t i = 0; i < t.kolka.size(); i++) {
        int x = X + 250 * (int)i;
        p.kolo(x + 11, 310, 11, t.kolkaPelne[i], cz);
        if (!t.kolkaPelne[i]) p.kolo(x + 11, 310, 9, true, !cz);
        tekst(p, F_P24B, x + 32, 318, t.kolka[i], cz);
      }
      break;
    }
    case Wiz::BRAK: return;
  }
  if (!t.podpisLewo.empty()) tekst(p, F_P22B, X, 376, t.podpisLewo, cz);
  if (!t.podpisPrawo.empty()) tekstPrawo(p, F_P22B, XP, 376, t.podpisPrawo, cz);
}
}  // namespace

void rysuj(Plotno& p, const Naglowek& n, const Tresc& t, const std::string& sl, const std::string& sp) {
  bool neg = t.problem;
  p.prostokat(0, 0, 800, 480, neg);
  bool cz = !neg;
  tekst(p, F_P24B, X, 35, n.lewo, cz);
  if (!n.prawo.empty()) tekstPrawo(p, F_P24B, XP, 35, n.prawo, cz);
  p.prostokat(0, 52, 800, 2, cz);
  // linia uwagi (5.4.5) jak na każdym ekranie danych (liniaUwagiEkranu(), R-EKR-52, R-EKR-62): w negatywie tonu
  // „problem” ostrzeżenie na białym pasku z `!`, informacja albo prośba w białej ramce 2 px bez `!`
  int dy = liniaUwagiEkranu(p, t.t.linia, t.t.liniaA, t.t.liniaOstrzezenie, neg);
  tekst(p, F_P22B, X, 94 + dy, wersaliki(t.t.etykieta), cz, 2);
  auto ty = zawin(F_SB44, t.t.tytul, W);
  tekst(p, F_SB44, X, 145 + dy, ty.empty() ? "" : ty[0], cz);
  auto l = zawin(F_P28B, t.t.powod, W);
  for (size_t i = 0; i < l.size() && i < 2; i++) tekst(p, F_P28B, X, 195 + dy + 34 * (int)i, l[i], cz);
  if (dy == 0) wizualizacja(p, t, cz);  // z linią uwagi wizualizacja ustępuje (brak miejsca nad krokiem)
  if (!t.t.krok.empty()) tekst(p, F_P28B, X, 418, t.t.krok, cz);
  p.prostokat(0, 434, 800, 2, cz);
  std::string prawo = t.stopkaPrawo.empty() ? sp : t.stopkaPrawo;
  std::string lewo = !t.pytanie.empty() ? std::string("↻ pomiń") : sl;  // S-K-30: przycisk pytania (SK30-oddech)
  if (ikonaAktualizacjiWlaczona()) {  // R-EKR-61: pełne słowa przed ikoną aktualizacji, jak w stopce dziennej
    prawo = skrocDoSlow(F_P22B, prawo, prawyBrzegStopki() - X);
    lewo = skrocDoSlow(F_P22B, lewo, prawyBrzegStopki() - X - (prawo.empty() ? 0 : szer(F_P22B, prawo) + 26));
  }
  if (!lewo.empty()) tekst(p, F_P22B, X, 465, lewo, cz);
  if (!prawo.empty()) tekstPrawo(p, F_P22B, prawyBrzegStopki(), 465, prawo, cz);
}

}  // namespace sk
