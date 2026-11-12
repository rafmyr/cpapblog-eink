// SPDX-License-Identifier: GPL-3.0-or-later
// Sprawy z wyników silnika reguł (src/sprawy.h).
#include "sprawy.h"

#include <algorithm>
#include <cmath>

#include "wspolne.h"
#include "wzorce.h"  // stan wzorca 5.12.12 do kart i rytmu ekranu

namespace sprawy {

ZrodloWynikow zrodloTestowe;

namespace {

// wpis `pokaz~{klucz}~poprz` (ostatnio = przedostatnie pokazanie pełnego ekranu; R-KON-14)
const char* const POPRZEDNIE = "~poprz";

using reguly::Pilnosc;
using reguly::StanReguly;

// rola z pliku tekstów (spec techniczna 9.3: tytuł, powód, krok, linia); skrypt budowy zestawu może pisać z ogonkami albo bez
bool rolaPasuje(const std::string& r, Rola co) {
  switch (co) {
    case Rola::TYTUL: return r == "tytul" || r == "tytuł";
    case Rola::POWOD: return r == "powod" || r == "powód";
    case Rola::KROK: return r == "krok";
    case Rola::LINIA: return r == "linia";
    case Rola::PODPIS_LEWO: return r == "podpis_lewo";
    case Rola::PODPIS_PRAWO: return r == "podpis_prawo";
    case Rola::RZAD_1: return r == "rzad_1";
    case Rola::RZAD_2: return r == "rzad_2";
  }
  return false;
}

// kolejność R-EKR-48: A, B, zdarzenia R5b, C, pozostałe L (także „tylko raport”), L-09 (problem poranny, (4))
int rangaSprawy(const SprawaL& s) {
  if (s.id == "L-09") return 5;
  if (!s.jestL()) return s.grupa == "R5b" ? 2 : 5;  // R5c i R5d to problem poranny, grupa (4) R-EKR-48
  if (s.tylkoRaport()) return 4;
  switch (s.pilnosc) {
    case Pilnosc::A: return 0;
    case Pilnosc::B: return 1;
    case Pilnosc::C: return 3;
    case Pilnosc::BRAK: return 4;
  }
  return 4;
}

// „Na kontroli: raport 2 / 3.” z linii pilności C (5.12.5, „Linie”): karta sprawy i raport mają zdanie bez tej części
std::string bezKontroli(const std::string& l) {
  // starsze postacie z zestawu danych z karty (ADR-0023): numer sprzed ADR-0041 i nazwa robocza sprzed 10.11.2026
  static const char* const K[] = {" Na kontroli: raport 2 / 3.", " Na kontroli: raport 2 / 6.", " Na kontroli: raport z K2."};
  for (const char* k : K) {
    const std::string s(k);
    if (l.size() >= s.size() && l.compare(l.size() - s.size(), s.size(), s) == 0) return l.substr(0, l.size() - s.size());
  }
  return l;
}

// mediana z n ostatnich nocy ważnych do `dzis` z wartością (AHI bez nocy niepewnych)
double medianaOstatnich(const std::vector<Noc>& hist, const std::string& dzis, int n, bool ahi) {
  std::vector<const Noc*> h;
  for (auto& x : hist)
    if (x.data <= dzis && x.wazna) h.push_back(&x);
  std::sort(h.begin(), h.end(), [](const Noc* a, const Noc* b) { return a->data < b->data; });
  std::vector<double> v;
  for (int i = (int)h.size() - 1; i >= 0 && (int)v.size() < n; i--) {
    const Noc& x = *h[(size_t)i];
    float w = ahi ? x.ahi : epapP95(x);
    if (std::isnan(w) || (ahi && nocNiepewna(x))) continue;
    v.push_back(w);
  }
  return mediana(v);
}

}  // namespace

std::string klucz(const reguly::Wynik& w) { return w.wariant.empty() ? w.id : w.id + "/" + w.wariant; }

const reguly::WierszKatalogu* wierszKatalogu(const reguly::Zestaw& z, const reguly::Wynik& w) {
  for (auto& k : z.katalog)
    if (k.id == w.id && k.wariant == w.wariant) return &k;
  return nullptr;
}

std::string tekstRoli(const reguly::Zestaw& z, const reguly::WierszKatalogu& k, const reguly::Wynik& w, Rola r,
                      const FunkcjaTekstu& f) {
  for (auto& id : k.teksty) {
    auto it = z.teksty.find(id);
    if (it != z.teksty.end() && rolaPasuje(it->second.rola, r)) return f ? f(id, w) : it->second.tekst;
  }
  return "";
}

std::vector<SprawaL> sprawyOtwarte(const reguly::Zestaw& z, const std::vector<reguly::Wynik>& wyniki,
                                   const reguly::Pamiec& pamiec, const FunkcjaTekstu& f) {
  std::vector<SprawaL> s;
  for (auto& w : wyniki) {
    // „nie oceniam” nie jest sprawą ani „w porządku” (ADR-0029 pkt 7): liczy się w warunku uspokojenia (kolejka.h)
    if (!w.oceniona || w.stan == StanReguly::ZABLOKOWANA) continue;
    // wiersz pomocniczy bez poziomu (np. `L-09/.wyjscie_e20`, `E-33/mediana`): składnik reguły, nie sprawa pacjenta
    if (w.poziom.empty()) continue;
    bool L = w.id.compare(0, 2, "L-") == 0;
    // zdarzenia E z poziomem „problem” (S3) we wszystkich grupach R5b–R5d (6.4); linie E idą przez linieZdarzen
    if (!L && (w.poziom != "problem" || (w.grupa != "R5b" && w.grupa != "R5c" && w.grupa != "R5d"))) continue;
    // R-EKR-48: zdarzenie wykluczone przez inne (np. E-31 przy E-30) trwa jako karta „ustępuje innej sprawie”, bez
    // ekranu i linii; wariant wykluczony przez inny wariant tego samego zdarzenia (E-31/obserwuj przy E-31/lekarz) nie
    bool wykl = false;
    if (!w.spelniona) {
      auto pw = w.zmienne.find("wykluczona_przez");
      if (L || w.zmienne.count("spelniona_przed_wykluczeniem") == 0 || pw == w.zmienne.end()) continue;
      if (pw->second.compare(0, w.id.size(), w.id) == 0 &&
          (pw->second.size() == w.id.size() || pw->second[w.id.size()] == '/'))
        continue;
      wykl = true;
    }
    SprawaL x;
    x.klucz = klucz(w);
    x.id = w.id;
    x.wariant = w.wariant;
    x.stan = w.stan;
    x.pilnosc = w.stan == StanReguly::TYLKO_RAPORT ? Pilnosc::BRAK : w.pilnosc;  // „tylko raport” bez pilności
    x.grupa = w.grupa;
    if (x.stan == StanReguly::AKTYWNA && L) {
      if (w.id == "L-09" || w.grupa == "R5c") x.ton = Ton::PROBLEM;
      else if (x.pilnosc == Pilnosc::A || x.pilnosc == Pilnosc::B) x.ton = Ton::LEKARZ;
    }
    x.wykluczona = wykl;
    if (x.stan == StanReguly::AKTYWNA && !L && !wykl) x.ton = Ton::PROBLEM;  // S3, ekran danych „problem”
    if (const reguly::WierszKatalogu* k = wierszKatalogu(z, w)) {
      x.tytul = tekstRoli(z, *k, w, Rola::TYTUL, f);
      x.powod = tekstRoli(z, *k, w, Rola::POWOD, f);
      x.krok = tekstRoli(z, *k, w, Rola::KROK, f);
      x.linia = tekstRoli(z, *k, w, Rola::LINIA, f);
      x.podpisLewo = tekstRoli(z, *k, w, Rola::PODPIS_LEWO, f);
      x.podpisPrawo = tekstRoli(z, *k, w, Rola::PODPIS_PRAWO, f);
      x.rzad1 = tekstRoli(z, *k, w, Rola::RZAD_1, f);
      x.rzad2 = tekstRoli(z, *k, w, Rola::RZAD_2, f);
      // metryka wykresu S3; próg w godzinach przy czasie z maską (M-01 w minutach, E-12.noc_min = 4 h)
      auto pm = k->parametry.find("metryka");
      if (pm != k->parametry.end()) x.metrykaWiz = pm->second;
      else if (k->rodzaj == "duzy_wyciek") x.metrykaWiz = "M-08";
      else if (k->rodzaj == "noce_bez_terapii" || k->rodzaj == "regularnosc_okna") x.metrykaWiz = "M-01";
      x.metrykaWizCpap = x.metrykaWiz;
      if (!x.metrykaWiz.empty() && x.metrykaWiz.compare(0, 2, "M-") != 0) {  // id progu z nazwą metryki trybu
        const reguly::Prog* a = z.prog(x.metrykaWiz, reguly::Tryb::ASV);
        const reguly::Prog* c = z.prog(x.metrykaWiz, reguly::Tryb::CPAP);
        x.metrykaWiz = a ? a->tekst : "";
        x.metrykaWizCpap = c ? c->tekst : "";
      }
      // linia przerywana S3: pierwszy próg liczbowy z `prog_linii` albo `prog` (godziny przy M-01 w minutach)
      for (const char* nazwa : {"prog_linii", "prog"}) {
        auto pr = k->parametry.find(nazwa);
        if (pr == k->parametry.end()) continue;
        auto pz = z.progi.find(pr->second);
        if (pz != z.progi.end() && pz->second.tekst.empty()) {
          bool godziny = pz->second.jednostka == "h" && x.metrykaWiz == "M-01";
          x.progLinii = godziny ? pz->second.wartosc * 60.0 : pz->second.wartosc;
          break;
        }
      }
    }
    if (L) {  // stan wzorca 5.12.12 (klucz wzorca) i ostatnie rzeczywiste pokazanie pełnego ekranu (R-PIE-08)
      auto pw = pamiec.find(wzorce::kluczWzorca(wyniki, w));
      if (pw != pamiec.end() && !pw->second.stanWzorca.empty())
        x.stanWzorca = wzorce::nazwaStanu(wzorce::odczytajZapis(pw->second.stanWzorca).stan);
      x.pokazanoPelny = ostatniePokazanie(pamiec, x.klucz);
      x.kluczWzorca = wzorce::kluczWzorca(wyniki, w);
    } else {  // zdarzenia E: pokazania pełnego ekranu S3 (R-PIE-08, R-KON-14)
      x.pokazanoPelny = ostatniePokazanie(pamiec, x.klucz);
    }
    x.pokazanoPoprzednio = ostatniePokazanie(pamiec, x.klucz + POPRZEDNIE);
    auto p = pamiec.find(x.klucz);
    if (p != pamiec.end()) {  // wzrost pilności od potwierdzenia kończy „wiem” i „Omówione” (5.12.7)
      x.aktywnaOd = p->second.aktywnaOd;
      if (!pilnoscWzrosla(p->second.wiem, x.pilnosc)) x.wiem = dataWyciszenia(p->second.wiem);
      if (!pilnoscWzrosla(p->second.omowione, x.pilnosc)) x.omowione = dataWyciszenia(p->second.omowione);
    }
    auto zm = w.zmienne.find("pominiete");
    if (zm != w.zmienne.end()) x.pominiete = zm->second;
    zm = w.zmienne.find("okno");
    if (zm != w.zmienne.end()) x.okno = zm->second;
    s.push_back(x);
  }
  std::stable_sort(s.begin(), s.end(), [](const SprawaL& a, const SprawaL& b) {
    int ra = rangaSprawy(a), rb = rangaSprawy(b);
    if (ra != rb) return ra < rb;
    return !a.aktywnaOd.empty() && (b.aktywnaOd.empty() || a.aktywnaOd < b.aktywnaOd);
  });
  return s;
}

int dniWiem(const SprawaL& s) { return s.pilnosc == Pilnosc::A ? 3 : 7; }

bool wiemAktywne(const SprawaL& s, const std::string& dzis) {
  if (s.wiem.empty() || !dataOk(s.wiem) || !dataOk(dzis)) return false;
  int d = dniMiedzy(s.wiem, dzis);
  if (!(d >= 0 && d < dniWiem(s))) return false;
  return s.stanWzorca.empty() || s.stanWzorca == "wiem" || d == 0;  // stan wzorca (powrót przed czasem)
}

bool omowioneAktywne(const SprawaL& s, const std::string& dzis) {
  if (s.omowione.empty() || !dataOk(s.omowione) || !dataOk(dzis)) return false;
  int d = dniMiedzy(s.omowione, dzis);
  if (d < 0) return false;
  // stan wzorca decyduje o końcu (56 albo 28 nocy, zmiana ustawień, Δ); bez stanu 56 dób
  if (!s.stanWzorca.empty()) return s.stanWzorca == "omowione" || d == 0;
  return d < NOCY_OMOWIONE;
}

void stanyWzorcow(std::vector<SprawaL>& s, const std::vector<reguly::Wynik>& wyniki, const reguly::Pamiec& pamiec,
                  const std::string& dzis, const reguly::Zestaw* z) {
  for (auto& x : s)
    if (x.jestL() && !x.kluczWzorca.empty())
      x.pelnyMimoOmowienia = wzorce::info(wyniki, pamiec, x.kluczWzorca, dzis, z).pelnyEkranDzis;
}

int dniOdWejscia(const SprawaL& s, const std::string& dzis) {
  if (s.aktywnaOd.empty() || !dataOk(s.aktywnaOd) || !dataOk(dzis)) return 0;
  return std::max(0, dniMiedzy(s.aktywnaOd, dzis));
}

bool problemPoranny(const SprawaL& s) {
  return s.ton == Ton::PROBLEM && (s.jestE() || s.id == "L-09");
}

bool limitProblemu(const SprawaL& s, const std::string& dzis) {
  const std::string &o = s.pokazanoPelny, &p = s.pokazanoPoprzednio;
  if (o.empty() || p.empty() || !dataOk(o) || !dataOk(p) || !dataOk(dzis) || o == dzis) return false;
  int d = dniMiedzy(p, dzis);  // dwa pokazania (p < o) w oknie 7 dni: blokada do 7 dni od pierwszego z nich
  return d >= 0 && d < LIMIT_PROBLEMU_DNI;
}

int rangaProblemu(const SprawaL& s) {
  // kolejność z tabeli 7.2 (R5b, R5c, R5d); ekranów S-K tu nie ma, ale stoją na swoich miejscach dla porządku
  static const char* const KOLEJNOSC[] = {"E-30", "E-31", "E-33", "E-36", "E-37", "S-K-17", "E-23",
                                          "S-K-31", "L-09", "E-22", "E-21", "S-K-14", "S-K-11", "S-K-10",
                                          "E-10", "E-12", "E-13", "E-14", "S-K-16"};
  const int N = (int)(sizeof(KOLEJNOSC) / sizeof(KOLEJNOSC[0]));
  for (int i = 0; i < N; i++)
    if (s.id == KOLEJNOSC[i]) return i;
  return N + (s.grupa == "R5b" ? 0 : s.grupa == "R5c" ? 1 : 2);
}

bool pelnyEkranDzis(const SprawaL& s, const std::string& dzis) {
  if (s.stan != StanReguly::AKTYWNA || s.ton == Ton::BRAK || s.wykluczona) return false;
  if (s.jestE()) {  // zdarzenie E (S3): każdego dnia trwania, poza „wiem” (7 dni) i limitem R-KON-14
    if (wiemAktywne(s, dzis)) return false;
    return !limitProblemu(s, dzis);
  }
  if (s.wygaszonyB() || s.wstrzymany() || s.karencja()) return false;  // R-SKI-09, R-INF-02, R-SKI-08
  if (wiemAktywne(s, dzis)) return false;
  if (omowioneAktywne(s, dzis)) return s.pelnyMimoOmowienia;  // R-LEK-27: A po 3. „Omówione” co 14 nocy
  // L-09 to problem poranny (R5c): limit R-KON-14 (2 razy w 7 dniach) zamiast rytmu „co 7 dni”
  if (problemPoranny(s)) return !limitProblemu(s, dzis);
  // R-PIE-08: od ostatniego rzeczywistego pokazania; pokazanie sprzed wejścia (poprzednie wejście) się nie liczy
  const std::string& p = s.pokazanoPelny;
  if (p.empty() || !dataOk(p) || !dataOk(dzis) || (!s.aktywnaOd.empty() && p < s.aktywnaOd)) return true;
  int d = dniMiedzy(p, dzis);
  if (d <= 0) return d == 0;
  return d >= (s.pilnosc == Pilnosc::A ? 3 : 7);
}

std::vector<LiniaZdarzenia> linieZdarzen(const reguly::Zestaw& z, const std::vector<reguly::Wynik>& wyniki,
                                         const FunkcjaTekstu& f) {
  std::vector<LiniaZdarzenia> r;
  for (auto& w : wyniki) {
    if (!w.oceniona || !w.spelniona || w.stan != StanReguly::AKTYWNA || w.poziom != "linia") continue;
    if (w.id.compare(0, 2, "E-") != 0 || w.id == "E-08") continue;
    const reguly::WierszKatalogu* k = wierszKatalogu(z, w);
    if (!k) continue;
    LiniaZdarzenia l;
    l.id = klucz(w);
    l.grupa = w.grupa;
    l.tekst = tekstRoli(z, *k, w, Rola::LINIA, f);
    if (!l.tekst.empty()) r.push_back(l);
  }
  return r;
}

std::string kluczPokazania(const std::string& co) { return "pokaz~" + co; }
bool kluczPomocniczy(const std::string& k) { return k.compare(0, 6, "pokaz~") == 0; }
bool zapiszPokazanie(reguly::Pamiec& p, const std::string& co, const std::string& dzis) {
  if (co.empty() || !dataOk(dzis)) return false;
  reguly::PamiecReguly& r = p[kluczPokazania(co)];
  if (r.ostatnio == dzis) return false;
  // (R-KON-14): przedostatnie pokazanie pełnego ekranu; linie (`linia:…`) liczą tylko ostatnie
  if (!r.ostatnio.empty() && co.compare(0, 6, "linia:") != 0) p[kluczPokazania(co + POPRZEDNIE)].ostatnio = r.ostatnio;
  if (r.aktywnaOd.empty()) r.aktywnaOd = dzis;
  r.ostatnio = dzis;
  r.licznik++;
  return true;
}
std::string ostatniePokazanie(const reguly::Pamiec& p, const std::string& co) {
  auto it = p.find(kluczPokazania(co));
  return it == p.end() ? "" : it->second.ostatnio;
}

const SprawaL* sprawaEkranuDnia(const std::vector<SprawaL>& s, const std::string& dzis) {
  // Pilność A jest zawsze widoczna (5.12.2, R-SKI-22): przy niewyciszonym wzorcu A (także po „wiem”) polem głównym
  // jest on w swoim dniu pełnego ekranu; w pozostałe dni linia `Wciąż: {tytuł}.` stoi na pierwszym miejscu kolejki
  // (7.5, grupa 0), a polem głównym jest kolejny kandydat z 7.2 z ekranem ze strefą linii: problem poranny (S3 zdarzeń
  // E i L-09, ze strefą linii uwagi, przegląd), inaczej ekran danych. Ekran L wzorca B nie ma
  // strefy linii, więc w tym czasie czeka (B dostaje linię co 3 dni w grupie 2, pełny ekran po wyciszeniu A
  // „Omówione” albo jego końcu).
  bool niewyciszonyA = false;
  for (auto& x : s)
    if (x.stan == StanReguly::AKTYWNA && x.jestL() && x.pilnosc == Pilnosc::A && !omowioneAktywne(x, dzis))
      niewyciszonyA = true;
  if (niewyciszonyA) {
    for (auto& x : s)
      if (x.pilnosc == Pilnosc::A && pelnyEkranDzis(x, dzis)) return &x;
  }
  const SprawaL* naj = nullptr;
  // A > B (R2, R3) > problem poranny (R5b > R5c > R5d, kolejność w grupie z 7.2)
  auto ranga = [](const SprawaL& x) {
    return x.ton == Ton::PROBLEM ? 100 + rangaProblemu(x) : x.pilnosc == Pilnosc::A ? 0 : 1;
  };
  for (auto& x : s)
    if (pelnyEkranDzis(x, dzis) && (!niewyciszonyA || x.ton == Ton::PROBLEM) && (!naj || ranga(x) < ranga(*naj)))
      naj = &x;
  return naj;
}

namespace {
char znakPilnosci(Pilnosc p) { return p == Pilnosc::A ? 'A' : p == Pilnosc::B ? 'B' : p == Pilnosc::C ? 'C' : '-'; }
int rangaPilnosci(char c) { return c == 'A' ? 3 : c == 'B' ? 2 : c == 'C' ? 1 : 0; }
}  // namespace
std::string dataWyciszenia(const std::string& pole) { return pole.substr(0, std::min<size_t>(pole.size(), 10)); }
bool pilnoscWzrosla(const std::string& pole, Pilnosc teraz) {
  if (pole.size() < 12 || pole[10] != '@' || pole[11] == '-') return false;  // pilność nieznana: wyciszenie trwa
  return rangaPilnosci(znakPilnosci(teraz)) > rangaPilnosci(pole[11]);
}
void zapiszWiem(reguly::Pamiec& p, const std::string& k, const std::string& dzis, Pilnosc pil) {
  p[k].wiem = dzis + "@" + znakPilnosci(pil);
}
void zapiszOmowione(reguly::Pamiec& p, const std::string& k, const std::string& dzis, Pilnosc pil) {
  p[k].omowione = dzis + "@" + znakPilnosci(pil);
  p[k].wiem.clear();  // potwierdzenie rozmowy zastępuje krótsze wyciszenie
}
void cofnijWiem(reguly::Pamiec& p, const std::string& k) {
  auto it = p.find(k);
  if (it != p.end()) it->second.wiem.clear();
}

std::vector<SprawaMetryki> sprawyMetryk(const std::vector<Noc>& hist, const Stany& st, const std::string& dzis) {
  std::vector<SprawaMetryki> m;
  auto dodaj = [&](const StanMetryki& s, const NormaMetryki& nm, const char* nazwa, bool ahi) {
    if (!s.aktywny) return;
    SprawaMetryki x;
    x.nazwa = nazwa;
    x.nocy = s.nocy;
    x.x = przecinek1(medianaOstatnich(hist, dzis, 5, ahi));
    x.p10 = przecinek1(nm.p10);
    x.p90 = przecinek1(nm.p90);
    x.x7 = przecinek1(medianaOstatnich(hist, dzis, 7, ahi));
    x.id = ahi ? "M-02" : "M-06p";
    x.od = s.od;
    m.push_back(x);
  };
  dodaj(st.ahi, st.norma.ahi, "AHI", true);
  dodaj(st.ep95, st.norma.ep95, "EPAP w szczycie", false);  // bez `P95` na ekranach dnia (R-EKR-53)
  return m;
}

// ---------- „Otwarte sprawy” ----------
namespace {

std::string wierszTy(const SprawaL& s, const WejscieSpraw& w) {
  if (wiemAktywne(s, w.dzis)) return "wiem do " + ddmm(dodajDni(s.wiem, dniWiem(s) - 1));
  if (omowioneAktywne(s, w.dzis)) return "omówione z lekarzem " + ddmm(s.omowione);
  if (w.odpowiedzi) {  // odpowiedź „gorzej” = tak w mocy: 3 dni [PRÓG] (R-SKI-36)
    for (auto& g : w.odpowiedzi->gorzej) {
      int d = dataOk(g.first) && dataOk(w.dzis) ? dniMiedzy(g.first, w.dzis) : -1;
      if (g.second && d >= 0 && d < 3) return "gorzej: tak, " + ddmm(g.first);
    }
  }
  return "bez potwierdzeń";
}

std::string wierszDalej(const SprawaL& s, const WejscieSpraw& w) {
  if (s.tylkoRaport()) return "w raporcie, bez pilności";
  if (w.ekranDnia && w.ekranDnia->klucz == s.klucz) return "na ekranie dnia";
  // stany wzorca 5.12.12 (R-EKR-49); „wstrzymane do wymiany elementu” (L-09, 5.9.6) przyjdzie z modułem SPRZĘT
  if (s.wstrzymany()) return "wstrzymane do końca okresu infekcji";
  if (s.karencja()) return "czekam: ekran po 14 nocach trwania";
  if (wiemAktywne(s, w.dzis)) return "wraca do wyboru " + ddmm(dodajDni(s.wiem, dniWiem(s))) + ", obserwacja trwa";
  if (s.wykluczona) return "ustępuje innej sprawie, obserwacja trwa";  // 7.2
  if (problemPoranny(s) && limitProblemu(s, w.dzis)) return "ekran najwyżej 2 razy w tygodniu, obserwacja trwa";
  bool om = omowioneAktywne(s, w.dzis);
  if (s.jestL() && s.pilnosc == Pilnosc::A && om) return "przypomnę co 3 dni, obserwacja trwa";
  if (s.jestL() && om)
    return "cisza przez " + std::to_string(NOCY_OMOWIONE) + " nocy od " + ddmm(s.omowione) + ", obserwacja trwa";
  if (s.jestL() && s.wygaszonyB()) return "przypomnę 1. dnia miesiąca, obserwacja trwa";  // R-SKI-09, R-SKI-13
  if (s.jestL() && s.pilnosc == Pilnosc::B && s.ton == Ton::LEKARZ) return "przypomnę co 3 dni, obserwacja trwa";
  if (s.jestL() && s.pilnosc == Pilnosc::A) return "przypominam codziennie, obserwacja trwa";
  if (s.jestL() && s.pilnosc == Pilnosc::C) return "w niedzielnym podsumowaniu, strona 2 / 3 i raport";
  return "dziś ekranem jest inna sprawa, obserwacja trwa";
}

std::string wierszOcena(const SprawaL& s) {
  if (!s.pominiete.empty() && s.pominiete != "0" && !s.okno.empty())
    return "ograniczona: " + s.pominiete + " z " + s.okno + " nocy pominięte (duży wyciek)";  // od P95 15 L/min też duży
  return "pełna";
}

}  // namespace

std::vector<KartaSprawy> kartySpraw(const WejscieSpraw& w) {
  std::vector<KartaSprawy> k;
  std::vector<KartaSprawy> c, reszta, problem;  // kolejność R-EKR-48: A, B, R5b | C | pozostałe L | (3) | (4) | (5)
  if (w.sprawy) {
    for (auto& s : *w.sprawy) {
      // R-EKR-48 (4): problem poranny R5c i R5d (L-09, E-21, E-22, E-10 …) tylko wtedy, gdy nie jest ekranem dnia
      if (rangaSprawy(s) == 5 && w.ekranDnia && w.ekranDnia->klucz == s.klucz) continue;
      KartaSprawy x;
      if (s.tylkoRaport()) {  // 5.4.15 „Treść karty”: wiersz „tylko raport” wygrywa z wierszem pilności C
        x.tytul = "Sprawa w raporcie";
        x.powod = !s.powod.empty() ? s.powod : bezKontroli(s.linia);
      } else if (s.jestL() && s.pilnosc == Pilnosc::C) {
        x.tytul = "Sprawa na kontrolę";
        x.powod = bezKontroli(s.linia);
      } else {
        x.tytul = s.tytul;
        x.powod = s.powod;
      }
      x.ocena = wierszOcena(s);
      x.ty = wierszTy(s, w);
      x.dalej = wierszDalej(s, w);
      x.koniec = s.jestL() ? "gdy kolejne noce przestaną to pokazywać" : "gdy dane przestaną to pokazywać";
      int r = rangaSprawy(s);
      if (r <= 2) k.push_back(x);
      else if (r == 3) c.push_back(x);
      else if (r == 4) reszta.push_back(x);
      else problem.push_back(x);
    }
  }
  k.insert(k.end(), c.begin(), c.end());
  k.insert(k.end(), reszta.begin(), reszta.end());
  if (w.metryki) {
    for (auto& m : *w.metryki) {
      KartaSprawy x;
      x.tytul = "Poza normą od " + std::to_string(m.nocy) + " nocy";
      x.powod = m.nazwa + " " + m.x + " przy zwykłych " + m.p10 + "–" + m.p90 + ".";
      x.ocena = "pełna";
      x.ty = "bez potwierdzeń";
      x.dalej = m.nocy >= 21 ? "bez pełnego ekranu: strona 2 / 3 i raport" : "bez pełnego ekranu: strona 2 / 3";
      x.koniec = "po powrocie do normy";
      k.push_back(x);
    }
  }
  k.insert(k.end(), problem.begin(), problem.end());
  for (auto& l : w.linieZdrowia) {
    KartaSprawy x;
    x.tytul = "Sprawa z paska u góry";
    x.powod = l;
    x.ocena = "pełna";
    x.ty = "bez potwierdzeń";
    x.dalej = "przypominam na pasku u góry";
    x.koniec = "gdy dane przestaną to pokazywać";  // warunek linii z jej modułu (6.4), jak przy zdarzeniach
    k.push_back(x);
  }
  return k;
}

KartaSprawy kartaPusta(const WejscieSpraw& w) {
  KartaSprawy x;
  // przy ocenie niepełnej tytuł i powód nie twierdzą, że spraw nie ma, tylko że panel ich nie widzi w tym, co ocenia
  x.tytul = w.ocenaPelna && !w.ostatniaNiepewna ? "Brak otwartych spraw." : "Nie widzę otwartych spraw.";
  x.powod = w.ocenaPelna && !w.ostatniaNiepewna ? "Nie ma spraw w toku." : "W tym, co oceniam, nic nie jest w toku.";
  // R-EKR-50;: `pełna` tylko przy pewnej pełnej ocenie; 
  // inaczej `częściowa` z wierszem pod spodem, bo „nie oceniam” nie znaczy „w porządku” (ADR-0029 pkt 7)
  if (w.ostatniaNiepewna) {
    x.ocena = "ograniczona: ostatnia noc z dużym wyciekiem";  // także P95 ≥ 15 L/min
  } else if (w.ocenaPelna) {
    x.ocena = "pełna";
  } else {
    x.ocena = "częściowa";
    x.dopisek = "Części rzeczy jeszcze nie oceniam.";
  }
  return x;
}

// ---------- odpowiedzi tak/nie ----------
const char* nazwaPytania(Pytanie p) {
  switch (p) {
    case Pytanie::SENNOSC: return "sennosc";
    case Pytanie::GORZEJ: return "gorzej";
    case Pytanie::PRZERWA: return "przerwa";
    case Pytanie::INFEKCJA: return "infekcja";
    case Pytanie::PODUSZKA: return "poduszka";
    case Pytanie::WYJAZD: return "wyjazd";
  }
  return "";
}

bool parsujOdpowiedz(const std::string& wiersz, Odpowiedz& o) {
  size_t a = wiersz.find(';');
  if (a == std::string::npos) return false;
  size_t b = wiersz.find(';', a + 1);
  if (b == std::string::npos) return false;
  std::string d = wiersz.substr(0, a), p = wiersz.substr(a + 1, b - a - 1), x = wiersz.substr(b + 1);
  if (!x.empty() && x.back() == '\r') x.pop_back();
  if (!dataOk(d)) return false;
  Odpowiedz r;
  r.data = d;
  if (p == "sennosc") r.pytanie = Pytanie::SENNOSC;
  else if (p == "gorzej") r.pytanie = Pytanie::GORZEJ;
  else if (p == "przerwa") r.pytanie = Pytanie::PRZERWA;
  else if (p == "infekcja") r.pytanie = Pytanie::INFEKCJA;
  else if (p == "poduszka") r.pytanie = Pytanie::PODUSZKA;
  else if (p == "wyjazd") r.pytanie = Pytanie::WYJAZD;
  else return false;
  if (x == "tak") r.odp = 't';
  else if (x == "nie") r.odp = 'n';
  else if (x == "brak") r.odp = 'b';
  else return false;
  o = r;
  return true;
}

std::string wierszOdpowiedzi(const Odpowiedz& o) {
  return o.data + ";" + nazwaPytania(o.pytanie) + ";" + (o.odp == 't' ? "tak" : o.odp == 'n' ? "nie" : "brak");
}

void zapiszOdpowiedz(std::vector<Odpowiedz>& lista, const Odpowiedz& o) {
  cofnijOdpowiedz(lista, o.data, o.pytanie);
  lista.push_back(o);
  std::stable_sort(lista.begin(), lista.end(), [](const Odpowiedz& a, const Odpowiedz& b) { return a.data < b.data; });
}

void cofnijOdpowiedz(std::vector<Odpowiedz>& lista, const std::string& data, Pytanie p) {
  lista.erase(std::remove_if(lista.begin(), lista.end(),
                             [&](const Odpowiedz& x) { return x.data == data && x.pytanie == p; }),
              lista.end());
}

bool jestOdpowiedz(const std::vector<Odpowiedz>& lista, const std::string& data, Pytanie p) {
  for (auto& x : lista)
    if (x.data == data && x.pytanie == p) return true;
  return false;
}

reguly::Odpowiedzi odpowiedziSilnika(const std::vector<Odpowiedz>& lista) {
  reguly::Odpowiedzi r;
  for (auto& x : lista) {
    if (x.odp == 'b') continue;  // brak odpowiedzi to nie „nie” (R-SKI-17)
    bool tak = x.odp == 't';
    switch (x.pytanie) {
      case Pytanie::SENNOSC: r.sennosc[x.data] = tak; break;
      case Pytanie::GORZEJ: r.gorzej[x.data] = tak; break;
      case Pytanie::PRZERWA:
        if (tak) r.przerwaPlanowana.insert(x.data);
        break;
      case Pytanie::INFEKCJA:  // okres infekcji liczy src/infekcja.h z pliku odpowiedzi
      case Pytanie::PODUSZKA:  // data wymiany z odpowiedzi „tak” (src/sprzet.h)
      case Pytanie::WYJAZD:    // R-CZY-23: skutek w czynnikach nocy (dyrygent), nie w silniku
        break;
    }
  }
  return r;
}

}  // namespace sprawy
