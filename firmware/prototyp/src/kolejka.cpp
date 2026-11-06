// SPDX-License-Identifier: GPL-3.0-or-later
// Kolejka linii uwagi, warunek uspokojenia z wyników, buzzer pilności A i pytania tak/nie (src/kolejka.h).
#include "kolejka.h"

#include <algorithm>

#include "wspolne.h"

namespace kolejka {

namespace {

using reguly::Pilnosc;
using reguly::StanReguly;
using sprawy::SprawaL;

// Wzory linii ogólnych z 7.5 i 5.12.5 („Linie”); `{tytuł}` = tytuł ekranu wzorca z zestawu
std::string wciaz(const SprawaL& s) { return "Wciąż: " + s.tytul + "."; }
std::string wciazOmowione(const SprawaL& s) { return "Wciąż: " + s.tytul + ". Omówione " + ddmm(s.omowione) + "."; }
std::string trwaPoWizycie(const SprawaL& s) { return s.tytul + ": trwa 8 tyg. po wizycie."; }
const char* const SENNOSC_TAK = "Senność w dzień: powiedz o tym lekarzowi.";            // R-SKI-28
const char* const GORZEJ_TAK = "Gorsze samopoczucie: skontaktuj się z lekarzem, jeśli trwa.";  // R-SKI-36
const int CO_ILE_B = 3;        // linia wzorca B w dni bez pełnego ekranu co 3 dni [PRÓG] (R-SKI-37)
const int CO_ILE_A_OM = 3;     // linia R-LEK-24 co 3 dni [PRÓG]
const int DNI_GORZEJ = 3;      // linia R-SKI-36 przez 3 dni [PRÓG]
// linie z 7.5 spoza wyników silnika i rytmy według rzeczywistych pokazań (R-PIE-08)
std::string naKontroli(const SprawaL& s) { return "Wciąż: " + s.tytul + ". Pokaż na kontroli."; }  // R-SKI-33, R-SKI-13
const char* const BEZ_ODP_SENNOSC = "Bez odpowiedzi o senność nie oceniam części spraw.";  // R-SKI-39
const char* const INFEKCJA = "Gorączka? Skontaktuj się z lekarzem. Duszność: wezwij pomoc.";  // R-INF-10
const int CO_ILE_NOR21 = 7;    // R-NOR-21: najwyżej raz na 7 dni [PRÓG]
const int NOCY_NOR21 = 21;     // R-NOR-21: od 21 kolejnych nocy ważnych stanu [PRÓG]
const int CO_ILE_INF = 3;      // R-INF-10: najwyżej raz na 3 dni [PRÓG]
const int PYTAN_SKI39 = 4;     // R-SKI-39: 4 ostatnie pokazane pytania [PRÓG]

std::string pokazanaLinia(const WejscieKolejki& w, const std::string& zrodlo) {
  return w.pamiec ? sprawy::ostatniePokazanie(*w.pamiec, "linia:" + zrodlo) : "";
}
// Linia z rytmem „najwyżej raz na n dni” liczonym od rzeczywistego pokazania: należy się, gdy nie była pokazana od
// `od` ("" = nigdy), dziś już stała (przerysowanie tego samego dnia) albo minęło co najmniej n dni.
bool rytmLinii(const WejscieKolejki& w, const std::string& zrodlo, int n, const std::string& od = "") {
  std::string p = pokazanaLinia(w, zrodlo);
  if (p.empty() || !dataOk(p) || !dataOk(w.dzis) || (!od.empty() && p < od)) return true;
  int d = dniMiedzy(p, w.dzis);
  return d == 0 || d >= n;
}
// Wzorzec, który dziś nie ma linii dziennej: wygaszony B (linia tylko w niedzielę i 1. dnia miesiąca), wstrzymany
// (R-INF-02), karencja (R-SKI-08)
bool bezLiniiDziennej(const SprawaL& s) { return s.wygaszonyB() || s.wstrzymany() || s.karencja(); }

bool aktywnaL(const SprawaL& s) { return s.jestL() && s.stan == StanReguly::AKTYWNA; }

void dodaj(std::vector<LiniaUwagi>& k, const std::string& t, GrupaLinii g, const std::string& zr, bool a = false) {
  if (t.empty()) return;
  LiniaUwagi l;
  l.tekst = t;
  l.grupa = g;
  l.wzorzecA = a;
  l.zrodlo = zr;
  k.push_back(l);
}

}  // namespace

std::string kluczPokazaniaLinii(const LiniaUwagi& l) { return "linia:" + l.zrodlo; }

int rangaGrupy(GrupaLinii g) {
  switch (g) {
    case GrupaLinii::PILNOSC_A_SENNOSC: return 0;
    case GrupaLinii::AWARIE: return 1;
    case GrupaLinii::WZORCE_L: return 2;
    case GrupaLinii::NOC_R_SKI_24: return 3;  // przed zaproszeniem (7.5, grupa 2a)
    case GrupaLinii::PYTANIA: return 4;
    case GrupaLinii::DANE: return 5;
    case GrupaLinii::NORMA: return 6;
    case GrupaLinii::PRZYPOMNIENIE:
    case GrupaLinii::PRZYPOMNIENIE_R5B: return 7;
    case GrupaLinii::NOC: return 8;
  }
  return 9;
}

bool aktywnyB(const std::vector<SprawaL>& s) {
  for (auto& x : s)
    if (aktywnaL(x) && x.pilnosc == Pilnosc::B) return true;
  return false;
}

bool aktywnyA(const std::vector<SprawaL>& s) {
  for (auto& x : s)
    if (aktywnaL(x) && x.pilnosc == Pilnosc::A) return true;
  return false;
}

bool niewyciszonyA(const std::vector<SprawaL>& s, const std::string& dzis) {
  for (auto& x : s)
    if (aktywnaL(x) && x.pilnosc == Pilnosc::A && !sprawy::omowioneAktywne(x, dzis)) return true;
  return false;
}

std::vector<LiniaUwagi> kolejkaLinii(const WejscieKolejki& w) {
  std::vector<LiniaUwagi> k;
  const std::vector<SprawaL> pusto;
  const std::vector<SprawaL>& s = w.sprawy ? *w.sprawy : pusto;
  bool niedziela = dataOk(w.dzis) && dzienTygodnia(w.dzis) == 0;
  // slot tygodniowy (5.8.2, R-SKI-12, R-SKI-33): niedziela od 17:00; minuta nieznana = cała niedziela
  bool tydzien = niedziela && (w.minuta < 0 || w.minuta >= SLOT_TYGODNIOWY_OD);
  bool pierwszyDzienMiesiaca = dataOk(w.dzis) && w.dzis.compare(8, 2, "01") == 0;
  auto naEkranie = [&](const SprawaL& x) { return w.ekranDnia && w.ekranDnia->klucz == x.klucz; };
  // grupa 0: niewyciszony wzorzec A w dni bez pełnego ekranu, także po „wiem” (R-SKI-22, R-EKR-22)
  for (auto& x : s)
    if (aktywnaL(x) && x.pilnosc == Pilnosc::A && !sprawy::omowioneAktywne(x, w.dzis) && !naEkranie(x) &&
        !x.wstrzymany())
      dodaj(k, wciaz(x), GrupaLinii::PILNOSC_A_SENNOSC, x.klucz, true);
  if (w.odpowiedzi) {
    auto it = w.odpowiedzi->sennosc.find(w.dzis);
    if (it != w.odpowiedzi->sennosc.end() && it->second) dodaj(k, SENNOSC_TAK, GrupaLinii::PILNOSC_A_SENNOSC, "sennosc");
  }
  // grupa 1: awarie; E-05 (bateria) przed pamięcią i plikiem reguł (7.5 grupa 1), krytyczna jako ostrzeżenie
  if (!w.bateriaKrytyczna.empty()) {
    dodaj(k, w.bateriaKrytyczna, GrupaLinii::AWARIE, "E-05/krytyczne");
    k.back().ostrzezenie = true;  // R-EKR-52
  }
  if (w.linieE)
    for (auto& l : *w.linieE)
      if (l.grupa == "awarie" && l.id.compare(0, 4, "E-05") == 0) dodaj(k, l.tekst, GrupaLinii::AWARIE, l.id);
  dodaj(k, w.awaria, GrupaLinii::AWARIE, "awaria");
  // grupa 2: wzorce L, w kolejności z 7.5
  for (auto& x : s) {  // B po „wiem” i co 3 dni bez pełnego ekranu
    if (!aktywnaL(x) || x.pilnosc != Pilnosc::B || x.ton != sprawy::Ton::LEKARZ || naEkranie(x)) continue;
    if (bezLiniiDziennej(x)) continue;  // wygaszony B ma linię R-SKI-33 i R-SKI-13 niżej
    if (sprawy::omowioneAktywne(x, w.dzis)) continue;
    int d = sprawy::dniOdWejscia(x, w.dzis);
    if (sprawy::wiemAktywne(x, w.dzis) || d % CO_ILE_B == 0) dodaj(k, wciaz(x), GrupaLinii::WZORCE_L, x.klucz);
  }
  for (auto& x : s)  // 56. dzień po „Omówione”, gdy warunek trwa (R-SKI-11), raz
    if (aktywnaL(x) && !x.omowione.empty() && dataOk(x.omowione) && dataOk(w.dzis) &&
        dniMiedzy(x.omowione, w.dzis) == sprawy::NOCY_OMOWIONE)
      dodaj(k, trwaPoWizycie(x), GrupaLinii::WZORCE_L, x.klucz);
  // wygaszony B (R-SKI-09): linia `Wciąż: {tytuł}. Pokaż na kontroli.` w slocie tygodniowym przed linią C (R-SKI-33)
  // i 1. dnia miesiąca (R-SKI-13); przy kilku wzorcach ten najwyżej w kolejności (lista spraw ma kolejność R-EKR-48)
  // R-SKI-13: 1. dnia miesiąca każdy wygaszony B; R-SKI-33: w slocie tygodniowym jeden, najwyżej w kolejności
  if (tydzien || pierwszyDzienMiesiaca)
    for (auto& x : s)
      if (aktywnaL(x) && x.pilnosc == Pilnosc::B && x.wygaszonyB() && !sprawy::omowioneAktywne(x, w.dzis)) {
        dodaj(k, naKontroli(x), GrupaLinii::WZORCE_L, x.klucz);
        if (!pierwszyDzienMiesiaca) break;
      }
  if (tydzien) {  // pilność C w slocie tygodniowym (R-SKI-12): jedna linia według rotacji R-SKI-41
    // wybór raz na niedzielę: linia C pokazana dziś zostaje do końca dnia; inaczej najdawniej
    // pokazana albo nigdy; remis wśród niepokazanych: wyżej w kolejności (przykład R-SKI-41: L-08, potem L-07);
    // remis pokazanych tego samego dnia: nie ta, którą panel wtedy wybrał (zapis `linia:C`), a bez zapisu niżej
    // w kolejności, bo wyższa była wyborem tamtego dnia
    std::string wybor, wyborDnia;
    if (w.pamiec) {
      auto it = w.pamiec->find(sprawy::kluczPokazania("linia:C"));
      if (it != w.pamiec->end()) { wybor = it->second.stanWzorca; wyborDnia = it->second.ostatnio; }
    }
    const SprawaL* c = nullptr;
    std::string cp;
    for (auto& x : s) {
      if (!aktywnaL(x) || x.pilnosc != Pilnosc::C || x.linia.empty() || sprawy::omowioneAktywne(x, w.dzis)) continue;
      std::string p = pokazanaLinia(w, x.klucz);
      if (p == w.dzis) { c = &x; break; }  // pokazana dziś: ta sama do końca dnia
      bool lepsza = !c || p < cp;
      if (c && p == cp && !p.empty()) lepsza = wyborDnia == p ? c->klucz == wybor : true;
      if (lepsza) { c = &x; cp = p; }
    }
    if (c) dodaj(k, c->linia, GrupaLinii::WZORCE_L, c->klucz);
  }
  // R-NOR-21: metryka zdrowotna w stanie odchylenia od 21 nocy, raz na 7 dni od pokazania, jedna linia na dzień
  // (kolejność tabeli 4.5: AHI przed EPAP P95); CA/h (M-03) do zaliczenia bez linii (lista metryk jej nie ma)
  if (w.metryki)
    for (auto& m : *w.metryki) {
      if (m.nocy < NOCY_NOR21 || m.x7.empty() || m.x7 == "–") continue;
      std::string zr = "R-NOR-21:" + m.id;
      if (!rytmLinii(w, zr, CO_ILE_NOR21)) continue;
      // {n}: pełne tygodnie od daty poranka pierwszej nocy stanu, z nią włącznie (R-NOR-21: po 21. nocy `Od 3 tyg.`);
      // bez daty z liczby nocy
      int tyg = dataOk(m.od) && dataOk(w.dzis) ? (dniMiedzy(m.od, w.dzis) + 1) / 7 : m.nocy / 7;
      // `… Liczby w raporcie.` (brzmienie po przeglądzie klinicznym)
      dodaj(k, "Od " + std::to_string(tyg) + " tyg.: " + m.nazwa + " " + m.x7 + ". Liczby w raporcie.",
            GrupaLinii::WZORCE_L, zr);
      break;
    }
  // R-INF-10: okres infekcji, pierwszego dnia i potem najwyżej raz na 3 dni od pokazania
  if (!w.okresInfekcjiOd.empty() && dataOk(w.okresInfekcjiOd) && dataOk(w.dzis) && w.okresInfekcjiOd <= w.dzis &&
      rytmLinii(w, "R-INF-10", CO_ILE_INF, w.okresInfekcjiOd))
    dodaj(k, INFEKCJA, GrupaLinii::WZORCE_L, "R-INF-10");
  if (w.odpowiedzi)
    for (auto& g : w.odpowiedzi->gorzej) {
      int d = dataOk(g.first) && dataOk(w.dzis) ? dniMiedzy(g.first, w.dzis) : -1;
      if (g.second && d >= 0 && d < DNI_GORZEJ) {
        dodaj(k, GORZEJ_TAK, GrupaLinii::WZORCE_L, "gorzej");
        break;
      }
    }
  // A po „Omówione”: od dnia po potwierdzeniu, potem co 3 dni od rzeczywistego pokazania (R-LEK-24, R-PIE-08;
  // przykład: „Omówione” 10.11 → 11.11, 14.11, 17.11; linia przegrana 14.11 wraca 15.11); w dzień pełnego ekranu
  // po trzecim potwierdzeniu (R-LEK-27) linii nie ma, bo sprawa stoi w polu głównym
  for (auto& x : s)
    if (aktywnaL(x) && x.pilnosc == Pilnosc::A && sprawy::omowioneAktywne(x, w.dzis) && !naEkranie(x) &&
        dataOk(x.omowione) && dataOk(w.dzis) && dniMiedzy(x.omowione, w.dzis) > 0 &&
        rytmLinii(w, x.klucz, CO_ILE_A_OM, dodajDni(x.omowione, 1)))
      dodaj(k, wciazOmowione(x), GrupaLinii::WZORCE_L, x.klucz);
  // grupa 2a: zaproszenie, nie przy niewyciszonym wzorcu A (R-PIE-06)
  if (!niewyciszonyA(s, w.dzis)) dodaj(k, w.zaproszenie, GrupaLinii::PYTANIA, "zaproszenie");
  // grupa 3: dane
  dodaj(k, w.e08, GrupaLinii::DANE, "E-08");
  // linie zdarzeń E: grupa z katalogu; E-10 i E-11, E-20, E-21 w grupie „noc”, E-04, E-07 w „danych”
  if (w.linieE)
    for (auto& l : *w.linieE) {
      if (l.grupa == "awarie" && l.id.compare(0, 4, "E-05") == 0) continue;  // wyżej, przed awarią pamięci
      GrupaLinii g = l.grupa == "awarie" ? GrupaLinii::AWARIE : l.grupa == "dane" ? GrupaLinii::DANE
                   : l.grupa == "norma"  ? GrupaLinii::NORMA  // R-MAS-11 (7.5, grupa 4)
                                         : GrupaLinii::NOC;
      dodaj(k, l.tekst, g, l.id);
    }
  // grupa „noc”: R-SKI-39, 4 ostatnie pokazane pytania o senność bez odpowiedzi (tygodnie bez pytania się nie liczą)
  if (w.odpowiedziPelne) {
    int brak = 0, pokazane = 0;
    for (auto it = w.odpowiedziPelne->rbegin(); it != w.odpowiedziPelne->rend() && pokazane < PYTAN_SKI39; ++it) {
      if (it->pytanie != sprawy::Pytanie::SENNOSC || it->data > w.dzis) continue;
      pokazane++;
      brak += it->odp == 'b';
    }
    if (pokazane == PYTAN_SKI39 && brak == PYTAN_SKI39) dodaj(k, BEZ_ODP_SENNOSC, GrupaLinii::NOC, "R-SKI-39");
  }
  // grupa 5: `Wciąż: {tytuł}.` problemu porannego (zdarzenia E z grup R5b–R5d i L-09), który dziś nie jest
  // ekranem dnia, bo jest wyciszony „wiem” (R-EKR-22) albo wyczerpał limit R-KON-14 (R-EKR-40); w kolejności R5b > R5c
  // > R5d (fail-path R-EKR-40). Linia zdarzenia R5b należy do grupy „zdrowie” (ostrzeżenie), reszta to przypomnienie.
  // Sprawa, która przegrała z inną w tabeli 7.2 albo jest wykluczona, linii nie ma (R-EKR-03: czeka w kolejce ekranów).
  {
    std::vector<const SprawaL*> p;
    for (auto& x : s)
      if (sprawy::problemPoranny(x) && x.stan == StanReguly::AKTYWNA && !x.wykluczona && !x.tytul.empty() &&
          !naEkranie(x) && (sprawy::wiemAktywne(x, w.dzis) || sprawy::limitProblemu(x, w.dzis)))
        p.push_back(&x);
    std::stable_sort(p.begin(), p.end(), [](const SprawaL* a, const SprawaL* b) {
      return sprawy::rangaProblemu(*a) < sprawy::rangaProblemu(*b);
    });
    for (auto* x : p)
      dodaj(k, wciaz(*x), x->grupa == "R5b" ? GrupaLinii::PRZYPOMNIENIE_R5B : GrupaLinii::PRZYPOMNIENIE, x->klucz);
  }
  std::stable_sort(k.begin(), k.end(),
                   [](const LiniaUwagi& a, const LiniaUwagi& b) { return rangaGrupy(a.grupa) < rangaGrupy(b.grupa); });
  // R-PIE-09: trwała linia awarii stała wczoraj na ekranie, a czeka linia grupy „wzorce L”: dziś linia L przed awarią
  if (dataOk(w.dzis) && pokazanaLinia(w, "awaria") == dodajDni(w.dzis, -1)) {
    auto aw = std::find_if(k.begin(), k.end(), [](const LiniaUwagi& l) { return l.zrodlo == "awaria"; });
    auto l = std::find_if(k.begin(), k.end(), [](const LiniaUwagi& x) { return x.grupa == GrupaLinii::WZORCE_L; });
    if (aw != k.end() && l != k.end() && aw < l) std::rotate(aw, aw + 1, l + 1);
  }
  return k;
}

void ustawLinie(DaneDnia& d, const std::vector<LiniaUwagi>& k) {
  d.linia.clear();
  d.liniaA = d.liniaOstrzezenie = false;
  if (k.empty()) return;
  d.linia = k.front().tekst;
  d.liniaA = k.front().wzorzecA;
  d.liniaOstrzezenie = k.front().zdrowie();
}

std::vector<std::string> linieZdrowiaBezSprawy(const std::vector<LiniaUwagi>& k) {
  std::vector<std::string> w;
  for (auto& l : k)
    if (l.zdrowie() && l.zrodlo.compare(0, 2, "L-") != 0 && l.zrodlo.compare(0, 2, "E-") != 0) w.push_back(l.tekst);
  return w;
}

// ---------- uspokojenie ----------
bool powodBrakuDanych(const std::string& p) {
  static const char* const DANE[] = {"za malo ", "brak nocy", "brak historii", "brak normy", "brak odczytu", "brak odczytow",
                                     "brak metryki", "brak odpowiedzi", "brak wynikow wczesniejszych", "brak ustawienia",
                                     "rozruch", "tryb aparatu nieznany", "trend: co najmniej 2 noce"};
  for (auto* d : DANE)
    if (p.compare(0, std::string(d).size(), d) == 0) return true;
  return false;
}

OcenaUspokojenia ocenUspokojenie(const std::vector<reguly::Wynik>& wyniki, const std::vector<LiniaUwagi>& k) {
  OcenaUspokojenia o;
  o.silnik = !wyniki.empty();
  o.wszystkieOcenione = o.silnik;
  o.pkt1 = true;
  o.pkt2 = true;
  for (auto& w : wyniki) {
    if (w.stan == StanReguly::ZABLOKOWANA) continue;  // L-06 zablokowany: nie liczy się wcale
    // wiersz pomocniczy bez poziomu (np. `E-23/bez_sk10_kilka`, `E-33/mediana`) nie jest regułą pokazywaną pacjentowi:
    // jego „nie oceniam” i spełnienie liczą się tylko przez regułę, która go składa
    if (w.poziom.empty()) continue;
    if (!w.oceniona) {
      o.wszystkieOcenione = false;
      if (!powodBrakuDanych(w.powodNieoceny)) o.awaria = true;  //
      o.nieOceniam.push_back(sprawy::klucz(w) + ": " + (w.powodNieoceny.empty() ? "nie oceniam" : w.powodNieoceny));
      continue;
    }
    if (w.spelniona && w.id.compare(0, 2, "L-") == 0) o.pkt1 = false;  // także wyciszony i „tylko raport”
    if (w.spelniona && w.grupa == "R5b") o.pkt2 = false;  // każde zdarzenie R5b (E-23 także przegrane w 7.2)
  }
  o.pkt4 = true;
  for (auto& l : k)
    if (l.zdrowie()) o.pkt4 = false;
  return o;
}

void zastosujUspokojenie(Stany& st, const OcenaUspokojenia& o) {
  if (!o.silnik) return;  // bez wyników punkty 1, 4 i 7 zostają nieoceniane
  if (o.wszystkieOcenione) {
    auto ocenione = [](const std::string& p) {
      return p.compare(0, 6, "pkt 1:") == 0 || p.compare(0, 6, "pkt 4:") == 0 || p.compare(0, 6, "pkt 7:") == 0;
    };
    st.nieoceniane.erase(std::remove_if(st.nieoceniane.begin(), st.nieoceniane.end(), ocenione), st.nieoceniane.end());
  }
  st.ocenaNiepelna = !st.nieoceniane.empty();
  bool sprawa = !st.uspokojenieOcenione || !o.pkt1 || !o.pkt2 || !o.pkt4;  // punkty liczone przez panel i silnik
  st.bezSpraw = !sprawa;
  st.uspokojenieOcenione = st.uspokojenieOcenione && o.wszystkieOcenione && o.pkt1 && o.pkt2 && o.pkt4;
  st.uspokojenie = !st.ocenaNiepelna && st.uspokojenieOcenione;
  // (R-EKR-32): krok zamienny bez pustego ekranu „Otwarte sprawy”
  st.niespokoj = st.uspokojenie ? Stany::Niespokoj::BRAK
                 : sprawa       ? Stany::Niespokoj::SPRAWA
                 : !o.wszystkieOcenione ? (o.awaria ? Stany::Niespokoj::REGULY : Stany::Niespokoj::DANE)
                                        : Stany::Niespokoj::OCENY;
}

// ---------- buzzer ----------
bool ciszaNocna(int m, int od, int do_) {
  if (od == do_) return false;
  return od < do_ ? (m >= od && m < do_) : (m >= od || m < do_);
}

std::string znacznikWejscia(const SprawaL& s, const std::string& dzis) {
  return s.klucz + "@" + (s.aktywnaOd.empty() ? dzis : s.aktywnaOd);
}

DecyzjaBuzzera buzzerPilnosci(const std::vector<SprawaL>& s, const std::set<std::string>& zagrane,
                              const std::string& dzis, int minutaDoby, bool wlaczony, int ciszaOd, int ciszaDo) {
  DecyzjaBuzzera d;
  if (!wlaczony) return d;
  std::vector<std::string> nowe;
  for (auto& x : s) {
    if (!aktywnaL(x) || x.pilnosc != Pilnosc::A) continue;  // „tylko raport” ma pilność BRAK (sprawy.cpp)
    if (sprawy::wiemAktywne(x, dzis) || sprawy::omowioneAktywne(x, dzis)) continue;  // R-BUZ-05
    std::string z = znacznikWejscia(x, dzis);
    if (!zagrane.count(z)) nowe.push_back(z);
  }
  if (nowe.empty()) return d;
  if (ciszaNocna(minutaDoby, ciszaOd, ciszaDo)) {
    d.odlozony = true;
    return d;
  }
  d.graj = true;
  d.znaczniki = nowe;
  return d;
}

// ---------- pytania ----------
namespace {
bool terapia(const std::vector<Noc>& h, const std::string& data) {
  for (auto& n : h)
    if (n.data == data) return n.m01 >= 15;  // M-01 ≥ 15 min [PRÓG] (1.5)
  return false;
}
}  // namespace

PytanieDnia pytanieDnia(const WejsciePytan& w) {
  PytanieDnia p;
  if (!dataOk(w.dzis)) return p;
  const std::vector<SprawaL> pusto;
  const std::vector<SprawaL>& s = w.sprawy ? *w.sprawy : pusto;
  const std::vector<sprawy::Odpowiedz> brakOdp;
  const std::vector<sprawy::Odpowiedz>& o = w.odpowiedzi ? *w.odpowiedzi : brakOdp;
  if (niewyciszonyA(s, w.dzis)) return p;  // R-PIE-06, R-SKI-16
  bool poCzasie = w.pokazanoOd >= 0 && w.minuta >= w.pokazanoOd + 30;
  if (dzienTygodnia(w.dzis) == 0 && w.minuta >= 17 * 60 && w.minuta < 23 * 60 + 30 && !poCzasie &&
      !sprawy::jestOdpowiedz(o, w.dzis, sprawy::Pytanie::SENNOSC)) {
    p.jest = true;
    p.pytanie = sprawy::Pytanie::SENNOSC;
    p.data = w.dzis;
    return p;
  }
  if (w.minuta >= w.terminRano || w.minuta < 5 * 60) return p;
  bool gorzejWarunek = false;
  for (auto& x : s)
    if ((x.id == "L-02" && x.wariant == "b") || x.id == "L-05" || (x.id == "L-06" && x.wariant == "c"))
      gorzejWarunek = true;
  if (w.gorzejInfekcja) gorzejWarunek = true;  // R-INF-07 (limit raz na 3 dni jak wyżej)
  bool gorzejNiedawno = false;
  for (auto& x : o)
    if (x.pytanie == sprawy::Pytanie::GORZEJ && dataOk(x.data)) {
      int d = dniMiedzy(x.data, w.dzis);
      if (d >= 1 && d < 3) gorzejNiedawno = true;  // najwyżej raz na 3 dni [PRÓG]
    }
  if (gorzejWarunek && !gorzejNiedawno && !sprawy::jestOdpowiedz(o, w.dzis, sprawy::Pytanie::GORZEJ)) {
    p.jest = true;
    p.pytanie = sprawy::Pytanie::GORZEJ;
    p.data = w.dzis;
    return p;
  }
  if (gorzejWarunek && !gorzejNiedawno) return p;  // dzień pytania „gorzej”: przerwa czeka do jutra
  if (!w.hist) return p;
  // powrót z przerwy: ostatnia noc z terapią w ciągu 7 dni [PRÓG], przed nią ≥ 3 [PRÓG] noce bez terapii
  for (int powrot = 0; powrot < 7; powrot++) {
    std::string dp = dodajDni(w.dzis, -powrot);
    if (!terapia(*w.hist, dp)) continue;
    std::string najstarsza = dp;
    for (auto& x : *w.hist)
      if (x.data < najstarsza) najstarsza = x.data;
    int n = 0;  // noce bez terapii przed powrotem; przerwa sięgająca początku historii to nie przerwa
    while (dodajDni(dp, -(n + 1)) >= najstarsza && !terapia(*w.hist, dodajDni(dp, -(n + 1)))) n++;
    bool przedHistoria = dodajDni(dp, -(n + 1)) < najstarsza;
    if (n >= 3 && !przedHistoria && !sprawy::jestOdpowiedz(o, dp, sprawy::Pytanie::PRZERWA)) {
      p.jest = true;
      p.pytanie = sprawy::Pytanie::PRZERWA;
      p.data = dp;
      p.przerwaNocy = n;
    }
    break;  // tylko ostatni powrót
  }
  return p;
}

}  // namespace kolejka
