// SPDX-License-Identifier: GPL-3.0-or-later
// Program „Dobry start”: opis w src/dobry_start.h.
#include "dobry_start.h"

#include <algorithm>
#include <cstring>

#include "magazyn.h"  // sha256Hex()
#include "wspolne.h"
#include "zestaw_wbudowany.h"

namespace dobry_start {

const char* const NAZWA_BELKI = "Dobry start";
const char* const TYTUL_TELEFON = "Nowa porada na telefonie.";
const char* const POLA_PLIKU = "rodzaj;a;b;c;d;e;f";

namespace {

// kolumny pliku porad czytane przez panel (narzędzie budowy zestawu pomija kolumny, których panel nie czyta)
const char* const POLA_PORAD =
    "id;rodzaj;dzien;tryb;wymaga;ekran;zamiast;warunek;kol;kol_ankiety;flagi;wymaga_porady;stopien_wyzej;tytul;tresc;krok;"
    "faza;qr;fakty";
const char* const POLA_ANKIETY = "id;typ;od_tygodnia;wylaczna;klucz;wartosci;pytanie;odpowiedzi";
const size_t N_POL_PORAD = 19, N_POL_ANKIETY = 8;

std::vector<std::string> dziel(const std::string& s, char sep) {
  std::vector<std::string> w;
  size_t p = 0;
  for (;;) {
    size_t q = s.find(sep, p);
    w.push_back(s.substr(p, q == std::string::npos ? std::string::npos : q - p));
    if (q == std::string::npos) break;
    p = q + 1;
  }
  return w;
}

// liczba całkowita 0…maks z samych cyfr; -1 przy błędzie
int liczba(const std::string& t, int maks) {
  int w = -1;
  if (t.empty() || t.size() > 6 || !indeksZTekstu(t.c_str(), maks + 1, w)) return -1;
  return w;
}

std::string bazaId(const std::string& id) { return id.substr(0, id.find('.')); }

uint8_t maskaTrybu(const std::string& t) {
  if (t == "wszystkie") return TR_WSZYSTKIE;
  if (t == "CPAP+AutoSet") return TR_CPAP | TR_AUTOSET;
  if (t == "CPAP") return TR_CPAP;
  if (t == "AutoSet") return TR_AUTOSET;
  if (t == "ASV") return TR_ASV;
  return 0;
}

uint16_t flagaZNazwy(const std::string& f) {
  static const struct { const char* n; uint16_t f; } t[] = {
      {"przerwa", FL_PRZERWA}, {"poza_limitem", FL_POZA_LIMITEM}, {"bez_limitu", FL_BEZ_LIMITU}, {"wyciek", FL_WYCIEK},
      {"bez_wycieku_7", FL_BEZ_WYCIEKU_7}, {"pierwsza_odpowiedz", FL_PIERWSZA}, {"dwa_razy", FL_DWA_RAZY},
      {"podstawowa", FL_PODSTAWOWA}, {"koncowa", FL_KONCOWA}, {"objaw", FL_OBJAW}, {"pochwala", FL_POCHWALA},
      {"raz", FL_RAZ}, {"koncowa_fazy", FL_KONCOWA_FAZY}};
  for (auto& x : t)
    if (f == x.n) return x.f;
  return 0;
}

bool trybPasuje(const Porada& p, Tryb t) {
  if (p.tryby & TR_WSZYSTKIE) return true;
  switch (t) {
    case Tryb::CPAP: return (p.tryby & TR_CPAP) != 0;
    case Tryb::AUTOSET: return (p.tryby & TR_AUTOSET) != 0;
    case Tryb::ASV: return (p.tryby & TR_ASV) != 0;
    default: return false;  // R-DS-38: tryb nieznany: tylko porady `wszystkie`
  }
}

bool nawilzaczPasuje(const Porada& p, int n) {
  if (p.nawilzacz == 0) return true;
  return p.nawilzacz == 1 ? n != 0 : n == 0;  // R-DS-29: `nie wiem` i bez wiersza jak `tak`
}

// Nagłówek `#cpap-eink {rodzaj} 1 … skrot=sha256:{hex}` i ciało; false przy złym nagłówku albo skrócie.
bool naglowek(const char* t, const char* rodzaj, const char*& cialo, std::string& skrot, std::string& powod) {
  if (!t || !*t) {
    powod = std::string(rodzaj) + ": brak pliku";
    return false;
  }
  const char* nl = std::strchr(t, '\n');
  if (!nl) {
    powod = std::string(rodzaj) + ": brak wiersza nagłówka";
    return false;
  }
  std::string n(t, (size_t)(nl - t));
  std::string pocz = std::string("#cpap-eink ") + rodzaj + " 1 ";
  size_t s = n.find(" skrot=sha256:");
  if (n.compare(0, pocz.size(), pocz) != 0 || s == std::string::npos) {
    powod = std::string(rodzaj) + ": zły nagłówek";
    return false;
  }
  skrot = n.substr(s + 14);
  cialo = nl + 1;
  if (sha256Hex(std::string(cialo)) != skrot) {
    powod = std::string(rodzaj) + ": zły skrót";
    return false;
  }
  return true;
}

bool tokenAnkiety(const std::string& t) { return t.compare(0, 4, "DS-Q") == 0; }

}  // namespace

// ---------- katalog ----------
std::string Katalog::tekst(const Wycinek& w) const {
  if (!zrodlo || (size_t)w.od + w.dl > dlugosc) return "";
  return std::string(zrodlo + w.od, w.dl);
}
const Porada* Katalog::porada(const std::string& id) const {
  for (auto& p : porady)
    if (p.id == id) return &p;
  return nullptr;
}
const Pytanie* Katalog::pytanieId(const std::string& id) const {
  for (auto& q : pytania)
    if (q.id == id) return &q;
  return nullptr;
}
Katalog::Katalog(const Katalog& k) { *this = k; }
Katalog& Katalog::operator=(const Katalog& k) {
  if (this == &k) return *this;
  wlasny = k.wlasny;
  zrodlo = k.zrodlo == k.wlasny.c_str() ? wlasny.c_str() : k.zrodlo;
  dlugosc = k.dlugosc;
  skrot = k.skrot;
  porady = k.porady;
  pytania = k.pytania;
  log = k.log;
  miejsc = k.miejsc;
  koniecStartu = k.koniecStartu;
  koniecAdaptacji = k.koniecAdaptacji;
  utrwalenieIle = k.utrwalenieIle;
  return *this;
}

bool wczytajKatalog(const char* tp, const char* ta, Katalog& wyj, std::string& powod, bool kopiuj) {
  Katalog k;
  if (kopiuj && tp) {
    k.wlasny = tp;
    tp = k.wlasny.c_str();
  }
  const char* cialo = nullptr;
  if (!naglowek(tp, "porady", cialo, k.skrot, powod)) return false;
  k.zrodlo = tp;
  k.dlugosc = std::strlen(tp);
  // wiersze porad: pola bez kopii tekstów (wycinki od początku pliku)
  const char* p = cialo;
  bool pierwszy = true;
  std::set<std::string> ids;
  while (*p) {
    const char* nl = std::strchr(p, '\n');
    size_t dl = nl ? (size_t)(nl - p) : std::strlen(p);
    std::string w(p, dl);
    uint32_t od = (uint32_t)(p - tp);
    p += dl + (nl ? 1 : 0);
    if (pierwszy) {
      pierwszy = false;
      if (w != POLA_PORAD) {
        powod = "porady: złe nazwy pól";
        return false;
      }
      continue;
    }
    if (w.empty()) continue;
    std::vector<std::string> f = dziel(w, ';');
    std::string id = f.empty() ? "" : f[0];
    auto pomin = [&](const char* dlaczego) { k.log.push_back((id.empty() ? std::string("?") : id) + ": " + dlaczego); };
    if (f.size() != N_POL_PORAD) { pomin("zła liczba pól"); continue; }
    Porada x;
    x.id = id;
    x.baza = bazaId(id);
    const std::string& r = f[1];
    if (r == "plan") x.rodzaj = Rodzaj::PLAN;
    else if (r == "zdarzenie") x.rodzaj = Rodzaj::ZDARZENIE;
    else if (r == "ankieta") x.rodzaj = Rodzaj::ANKIETA;
    else if (r == "pochwala") x.rodzaj = Rodzaj::POCHWALA;
    else { pomin("nieznany rodzaj"); continue; }
    if (x.rodzaj == Rodzaj::PLAN) {
      x.dzien = liczba(f[2], DZIEN_MAKS);
      if (x.dzien < 1) { pomin("zła doba nominalna"); continue; }
      const std::string& fz = f[16];
      x.faza = fz == "start_rano" ? Faza::START_RANO : fz == "start_wieczor" ? Faza::START_WIECZOR
               : fz == "adaptacja" ? Faza::ADAPTACJA : fz == "utrwalenie" ? Faza::UTRWALENIE : Faza::BRAK;
      if (x.faza == Faza::BRAK) { pomin("nieznana faza"); continue; }
    }
    x.tryby = maskaTrybu(f[3]);
    if (!x.tryby) { pomin("nieznany tryb"); continue; }
    if (f[4] == "nawilżacz") x.nawilzacz = 1;
    else if (f[4] == "bez nawilżacza") x.nawilzacz = -1;
    else if (!f[4].empty()) { pomin("nieznane wymaga"); continue; }
    if (f[5] == "karta") x.karta = true;
    else if (f[5] != "dane") { pomin("nieznany ekran"); continue; }
    x.zamiast = f[6];
    for (auto& t : dziel(f[7], '|')) {
      if (t.empty()) continue;
      if (t[0] == '!') x.weta.push_back(t.substr(1));
      else x.warunek.push_back(t);
    }
    x.kol = f[8].empty() ? 0 : liczba(f[8], 999);
    x.kolAnkiety = f[9].empty() ? 0 : liczba(f[9], 999);
    if (x.kol < 0 || x.kolAnkiety < 0) { pomin("zła kolejność"); continue; }
    bool zlaFlaga = false;
    for (auto& fl : dziel(f[10], ',')) {
      if (fl.empty()) continue;
      uint16_t v = flagaZNazwy(fl);
      if (!v) zlaFlaga = true;
      x.flagi = (uint16_t)(x.flagi | v);
    }
    if (zlaFlaga) { pomin("nieznana flaga"); continue; }
    x.wymagaPorady = f[11];
    x.stopienWyzej = f[12];
    if (f[13].empty()) { pomin("brak tytułu"); continue; }
    if (f[14].empty()) { pomin("brak treści"); continue; }
    if (f[13].size() > 0xFFFF || f[14].size() > 0xFFFF || f[15].size() > 0xFFFF) { pomin("za długi tekst"); continue; }
    uint32_t pole = od;
    for (size_t i = 0; i < 13; i++) pole += (uint32_t)f[i].size() + 1;
    x.tytul = {pole, (uint16_t)f[13].size()};
    pole += (uint32_t)f[13].size() + 1;
    x.tresc = {pole, (uint16_t)f[14].size()};
    pole += (uint32_t)f[14].size() + 1;
    x.krok = {pole, (uint16_t)f[15].size()};
    pole += (uint32_t)f[15].size() + 1 + (uint32_t)f[16].size() + 1;
    if (f[17].size() > 0xFF || f[18].size() > 0xFFFF) { pomin("za długi adres albo fakty"); continue; }
    x.qr = {pole, (uint16_t)f[17].size()};
    pole += (uint32_t)f[17].size() + 1;
    x.fakty = {pole, (uint16_t)f[18].size()};
    if (ids.count(id)) { pomin("powtórzone ID"); continue; }
    bool zajete = false;
    // `wszystkie` zajmuje miejsce we wszystkich trybach (para `wszystkie` + konkretny tryb to dwie porady na miejscu)
    auto bity = [](uint8_t t) -> uint8_t { return (t & TR_WSZYSTKIE) ? (uint8_t)(TR_CPAP | TR_AUTOSET | TR_ASV) : t; };
    if (x.rodzaj == Rodzaj::PLAN)
      for (auto& y : k.porady)
        if (y.rodzaj == Rodzaj::PLAN && y.faza == x.faza && y.dzien == x.dzien && (bity(y.tryby) & bity(x.tryby)) != 0 &&
            (y.nawilzacz == 0 || x.nawilzacz == 0 || y.nawilzacz == x.nawilzacz))
          zajete = true;
    if (zajete) { pomin("dwie porady na tym samym miejscu i w tym samym wariancie"); continue; }
    ids.insert(id);
    k.porady.push_back(x);
  }
  if (k.porady.empty()) {
    powod = "porady: brak porad";
    return false;
  }
  std::set<int> utrw;
  for (auto& x : k.porady) {
    if (x.rodzaj != Rodzaj::PLAN) continue;
    k.miejsc = std::max(k.miejsc, x.dzien);
    if (x.faza == Faza::START_RANO || x.faza == Faza::START_WIECZOR) k.koniecStartu = std::max(k.koniecStartu, x.dzien);
    if (x.faza == Faza::ADAPTACJA) k.koniecAdaptacji = std::max(k.koniecAdaptacji, x.dzien);
    if (x.faza == Faza::UTRWALENIE) utrw.insert(x.dzien);
  }
  k.utrwalenieIle = (int)utrw.size();
  // ankieta (mały plik, w całości w pamięci); bez pliku ankiety program działa bez porad z ankiety
  const char* ca = nullptr;
  std::string sa, pa;
  if (ta && *ta && naglowek(ta, "ankieta", ca, sa, pa)) {
    std::vector<std::string> wa = dziel(ca, '\n');
    if (!wa.empty() && wa[0] == POLA_ANKIETY)
      for (size_t i = 1; i < wa.size(); i++) {
        if (wa[i].empty()) continue;
        std::vector<std::string> f = dziel(wa[i], ';');
        if (f.size() != N_POL_ANKIETY || f[0].empty() || (f[1] != "jedna" && f[1] != "wiele")) {
          k.log.push_back((f.empty() ? std::string("?") : f[0]) + ": zły wiersz ankiety");
          continue;
        }
        Pytanie q;
        q.id = f[0];
        q.typ = f[1];
        q.odTygodnia = liczba(f[2], 99);
        q.wylaczna = f[3].empty() ? 0 : liczba(f[3], 99);
        q.klucz = f[4];
        if (!f[5].empty()) q.wartosci = dziel(f[5], '|');
        q.pytanie = f[6];
        for (auto& o : dziel(f[7], '|')) {
          size_t a = o.find_first_not_of(' '), b = o.find_last_not_of(' ');
          if (a != std::string::npos) q.odpowiedzi.push_back(o.substr(a, b - a + 1));
        }
        if (q.odTygodnia < 0 || q.wylaczna < 0 || q.odpowiedzi.empty()) {
          k.log.push_back(q.id + ": zły wiersz ankiety");
          continue;
        }
        k.pytania.push_back(q);
      }
  } else if (ta && *ta) {
    k.log.push_back("ankieta: " + pa);
  }
  wyj = k;
  return true;
}

const Katalog& katalogWbudowany() {
  static Katalog k;
  static bool wczytany = false;
  if (!wczytany) {
    wczytany = true;
    std::string p;
    if (!wczytajKatalog(reguly::wbudowany::PORADY, reguly::wbudowany::ANKIETA, k, p)) k = Katalog();
  }
  return k;
}

// ---------- ustawienia ----------
bool ustawienie(Ustawienia& u, const std::string& kl, const std::string& v, std::string& blad) {
  if (kl.compare(0, 6, "start.") != 0) return false;
  blad.clear();
  if (kl == "start.program") {
    if (v == "tak") u.program = true;
    else if (v == "nie") u.program = false;
    else { u.program = false; blad = "start.program: zła wartość"; }
  } else if (kl == "start.dzien") {
    int n = liczba(v, DZIEN_MAKS);
    if (n < 1) { u.dzien = 1; blad = "start.dzien: zła wartość"; }
    else u.dzien = n;
  } else if (kl == "start.nawilzacz") {
    if (v == "tak") u.nawilzacz = 1;
    else if (v == "nie") u.nawilzacz = 0;
    else if (v == "nie_wiem") u.nawilzacz = -1;
    else { u.nawilzacz = -1; blad = "start.nawilzacz: zła wartość"; }
  } else if (kl == "start.objawy") {
    if (v == "panel") u.telefon = false;
    else if (v == "telefon") u.telefon = true;
    else { u.telefon = false; blad = "start.objawy: zła wartość"; }
  } else if (kl == "start.ponad") {  // R-DS-67: klucz wersji 1 wycofany („ponad program” w utrwaleniu)
    (void)v;
    blad = "start.ponad: klucz wycofany (program 12 tygodni), pominięty";
  } else {
    blad = kl + ": nieznany klucz";
  }
  return true;
}

// ---------- tryb ----------
Tryb trybZNazwy(const std::string& n) {
  if (n == "CPAP") return Tryb::CPAP;
  if (n == "AutoSet") return Tryb::AUTOSET;
  if (n == "ASV" || n == "ASVAuto") return Tryb::ASV;
  return Tryb::NIEZNANY;
}
const char* nazwaTrybu(Tryb t) {
  switch (t) {
    case Tryb::CPAP: return "CPAP";
    case Tryb::AUTOSET: return "AutoSet";
    case Tryb::ASV: return "ASV";
    default: return "";
  }
}
const char* nazwaZrodla(Zrodlo z) {
  switch (z) {
    case Zrodlo::DANE: return "dane";
    case Zrodlo::ANKIETA: return "ankieta";
    case Zrodlo::PODSTAWOWA: return "podstawowa";
    case Zrodlo::TRYB: return "tryb";
    case Zrodlo::POCHWALA: return "pochwala";
    default: return "plan";
  }
}
static Zrodlo zrodloZNazwy(const std::string& n) {
  if (n == "dane") return Zrodlo::DANE;
  if (n == "ankieta") return Zrodlo::ANKIETA;
  if (n == "podstawowa") return Zrodlo::PODSTAWOWA;
  if (n == "tryb") return Zrodlo::TRYB;
  if (n == "pochwala") return Zrodlo::POCHWALA;
  return Zrodlo::PLAN;
}

const Porada* wariant(const Katalog& k, const std::string& baza, Tryb t, int n) {
  for (auto& p : k.porady)
    if (p.baza == baza && trybPasuje(p, t) && nawilzaczPasuje(p, n)) return &p;
  return nullptr;
}
const Porada* wariantDoby(const Katalog& k, Faza f, int dzien, Tryb t, int n) {
  for (auto& p : k.porady)
    if (p.rodzaj == Rodzaj::PLAN && p.faza == f && p.dzien == dzien && trybPasuje(p, t) && nawilzaczPasuje(p, n))
      return &p;
  return nullptr;
}
const char* nazwaFazy(Faza f) {
  switch (f) {
    case Faza::START_RANO: return "start_rano";
    case Faza::START_WIECZOR: return "start_wieczor";
    case Faza::ADAPTACJA: return "adaptacja";
    case Faza::UTRWALENIE: return "utrwalenie";
    default: return "";
  }
}
Etap Katalog::etap(int d) const {
  if (d <= koniecStartu) return Etap::START;
  if (d <= koniecAdaptacji) return Etap::ADAPTACJA;
  if (d <= miejsc) return Etap::UTRWALENIE;
  return Etap::KONIEC;
}

int doba(const Stan& s, const std::string& dzis) {
  if (!dataOk(s.start) || !dataOk(dzis)) return 0;
  return dniMiedzy(s.start, dzis) + 1;
}
int dobaProgramu(const Stan& s, const std::string& dzis) {
  int d = doba(s, dzis);
  if (d < 1) return 0;
  return std::max(1, d - s.przerwaDni + std::max(1, s.dzienUstawiony) - 1);
}

// ---------- stan ----------
int dzienStartu(const Ustawienia& u, const Katalog& k) {
  return u.dzien >= 1 && u.dzien <= std::max(1, k.miejsc) ? u.dzien : 1;
}
Stan nowyStan(const Ustawienia& u, const Katalog& k) {
  Stan s;
  // fail-path R-DS-65: `start.dzien` spoza 1…miejsc (ręczna edycja pliku) → 1 z wpisem w logu
  s.dzienUstawiony = dzienStartu(u, k);
  if (s.dzienUstawiony != u.dzien) s.bledy.push_back("dobry start: start.dzien=" + std::to_string(u.dzien) + " spoza 1…" + std::to_string(k.miejsc) +
                         ", program od doby 1 (R-DS-65)");
  s.zestaw = k.skrot;
  s.odNowa = "?";  // R-DS-33: nowy program od najbliższej doby (datę „od nowa” dopisuje wywołujący, gdy ją zna)
  return s;
}

Stan stanZWierszy(const std::vector<std::string>& wiersze, const Katalog& k) {
  Stan s;
  bool v1 = false;
  for (auto& w : wiersze) {
    std::vector<std::string> f = dziel(w, ';');
    const std::string& r = f[0];
    auto zle = [&]() { s.bledy.push_back("dobry_start: wiersz nieczytelny: " + w.substr(0, 40)); };
    if (r == "S" && f.size() >= 3) {
      const std::string &kl = f[1], &v = f[2];
      auto n = [&](int& cel, int maks) {
        int x = liczba(v, maks);
        if (x < 0) zle();
        else cel = x;
      };
      if (kl == "start") { if (dataOk(v)) s.start = v; else zle(); }
      else if (kl == "kolejka" || kl == "ponad") v1 = true;  // wersja 1: kolejka i „ponad program” (R-DS-67)
      else if (kl == "stan") {
        if (v == "aktywny" || v == "przerwa" || v == "zatrzymany" || v == "wstrzymany" || v == "zakonczony") s.stan = v;
        else zle();
      }
      else if (kl == "przerwa_dni") n(s.przerwaDni, 9999);
      else if (kl == "d0_z_rzedu") n(s.d0ZRzedu, 9999);
      else if (kl == "noce_terapii") n(s.noceTerapii, 9999);
      else if (kl == "tryb") s.tryb = v;
      else if (kl == "zestaw") s.zestaw = v;
      else if (kl == "ankieta_od") { if (dataOk(v)) s.ankietaOd = v; else zle(); }
      else if (kl == "doba") { if (dataOk(v)) s.doba = v; else zle(); }
      else if (kl == "dzien_ust") n(s.dzienUstawiony, DZIEN_MAKS);
      else if (kl == "plan_koniec") s.planKoniec = v == "1";
      else if (kl == "od_nowa") s.odNowa = v;
      else if (kl == "skok") { if (dataOk(v)) s.skokData = v; else zle(); }
      else if (kl == "skok_ile") n(s.skokIle, 9999);
      else zle();
    } else if (r == "D" && f.size() >= 7 && dataOk(f[1]) && k.porada(f[2])) {
      s.wDzis = f[1];
      s.wId = f[2];
      s.wZrodlo = f[3];
      s.wRegula = f[4];
      s.wWieczor = f[5] == "1";
      s.wBezWpisu = f[6] == "1";
    } else if (r == "E" && f.size() >= 3 && dataOk(f[1]) && k.porada(f[2])) {
      s.eDzis = f[1];
      s.eId = f[2];
    } else if (r == "P" && f.size() >= 7 && dataOk(f[1]) && !f[2].empty()) {
      s.p.push_back({f[1], f[2], f[3], f[4], f[5], f[6]});
    } else if (r == "Z" && f.size() >= 4 && dataOk(f[1]) && !f[2].empty()) {
      s.z.push_back({f[1], f[2], f[3]});
    } else if (r == "O" && f.size() >= 5 && dataOk(f[1])) {
      s.o.push_back({f[1], f[2], f[3], f[4]});
    } else if (r == "K" && f.size() >= 3 && !f[1].empty()) {
      if (f[1].find_first_not_of("0123456789") == std::string::npos) v1 = true;  // wersja 1: miejsce planu z liczbą
      else s.zaliczone.push_back({f[1], f[2]});
    } else if (r == "W" && f.size() >= 2 && dataOk(f[1])) {
      s.wyciek.push_back(f[1]);
    } else if (r == "T" && f.size() >= 2 && !f[1].empty()) {
      s.trybPoZmianie.push_back(f[1]);
    } else {
      zle();
    }
  }
  if (v1)  // wiersze `K;{miejsce}` wersji 1 odtworzone z dziennika P (porady z `zamiast`, R-DS-09)
    for (auto& x : s.p) {
      const Porada* p = (x.zrodlo == "dane" || x.zrodlo == "ankieta") ? k.porada(x.id) : nullptr;
      const Porada* z = p && !p->zamiast.empty() ? k.porada(p->zamiast) : nullptr;
      if (!z || z->rodzaj != Rodzaj::PLAN) continue;
      bool jest = false;
      for (auto& y : s.zaliczone) jest = jest || y.first == z->baza;
      for (auto& y : s.p) jest = jest || bazaId(y.id) == z->baza;
      if (!jest) s.zaliczone.push_back({z->baza, x.data});
    }
  if (v1)
    s.bledy.push_back("dobry_start: stan programu wersji 1 (kolejka 1…42, „ponad program”): porady pokazane z wpisów P, "
                      "kolejki faz od doby programu (R-DS-67)");
  return s;
}

std::vector<std::string> wierszeStanu(const Stan& s) {
  std::vector<std::string> w;
  auto S = [&](const char* kl, const std::string& v) { w.push_back(std::string("S;") + kl + ";" + v); };
  S("start", s.start);
  S("stan", s.stan);
  S("przerwa_dni", std::to_string(s.przerwaDni));
  S("d0_z_rzedu", std::to_string(s.d0ZRzedu));
  S("tryb", s.tryb);
  S("zestaw", s.zestaw);
  S("noce_terapii", std::to_string(s.noceTerapii));
  if (!s.ankietaOd.empty()) S("ankieta_od", s.ankietaOd);
  if (!s.doba.empty()) S("doba", s.doba);
  S("dzien_ust", std::to_string(s.dzienUstawiony));
  S("plan_koniec", s.planKoniec ? "1" : "0");
  if (!s.odNowa.empty()) S("od_nowa", s.odNowa);
  if (!s.skokData.empty()) {
    S("skok", s.skokData);
    S("skok_ile", std::to_string(s.skokIle));
  }
  if (!s.wDzis.empty())
    w.push_back("D;" + s.wDzis + ";" + s.wId + ";" + s.wZrodlo + ";" + s.wRegula + ";" + (s.wWieczor ? "1" : "0") +
                ";" + (s.wBezWpisu ? "1" : "0"));
  if (!s.eDzis.empty()) w.push_back("E;" + s.eDzis + ";" + s.eId);
  for (auto& x : s.p) w.push_back("P;" + x.data + ";" + x.id + ";" + x.zrodlo + ";" + x.regula + ";" + x.pora + ";" + x.gdzie);
  for (auto& x : s.z) w.push_back("Z;" + x.data + ";" + x.id + ";" + x.regula);
  for (auto& x : s.o) w.push_back("O;" + x.data + ";" + x.tydzien + ";" + x.pytanie + ";" + x.kody);
  for (auto& x : s.zaliczone) w.push_back("K;" + x.first + ";" + x.second);
  for (auto& x : s.wyciek) w.push_back("W;" + x);
  for (auto& x : s.trybPoZmianie) w.push_back("T;" + x);
  return w;
}

// ---------- pomocnicze wyboru ----------
namespace {

bool wieczornaStartu(const Porada* p) { return p && p->rodzaj == Rodzaj::PLAN && p->faza == Faza::START_WIECZOR; }
// wpis porady wieczornej startu (z kolejki `start_wieczor`); ta sama porada jako zaległa podstawowa rano to porada dnia
bool wpisWieczorny(const Katalog& k, const WpisP& x) { return x.zrodlo == "plan" && wieczornaStartu(k.porada(x.id)); }

struct Liczniki {
  const Katalog& k;
  const Stan& s;
  int pokazania(const std::string& baza) const {
    int n = 0;
    for (auto& x : s.p)
      if (bazaId(x.id) == baza) n++;
    return n;
  }
  std::string ostatnio(const std::string& baza) const {
    std::string d;
    for (auto& x : s.p)
      if (bazaId(x.id) == baza && x.data > d) d = x.data;
    return d;
  }
  // porada planu pokazana: wpis P tej porady (dowolny wariant) albo porady tej samej fazy i doby nominalnej, albo
  // zaliczona poza kolejką (`zamiast`, R-DS-09)
  bool planPokazany(const Porada& p) const {
    for (auto& z : s.zaliczone)
      if (z.first == p.baza) return true;
    for (auto& x : s.p) {
      if (bazaId(x.id) == p.baza) return true;
      const Porada* q = k.porada(x.id);
      if (q && q->rodzaj == Rodzaj::PLAN && q->faza == p.faza && q->dzien == p.dzien) return true;
    }
    return false;
  }
  // R-DS-08: raz na 7 dni
  bool oknoOk(const std::string& baza, const std::string& dzis) const {
    std::string o = ostatnio(baza);
    return o.empty() || dniMiedzy(o, dzis) >= OKNO_DNI;
  }
  // R-DS-07: porady wyzwalane w oknie 7 dni (bez porad z flagą `poza_limitem`)
  int wyzwalaneWOknie(const std::string& dzis) const {
    int n = 0;
    for (auto& x : s.p) {
      if (x.zrodlo != "dane" && x.zrodlo != "ankieta") continue;
      const Porada* p = k.porada(x.id);
      if (p && p->ma(FL_POZA_LIMITEM)) continue;
      int d = dniMiedzy(x.data, dzis);
      if (d >= 0 && d < OKNO_DNI) n++;
    }
    return n;
  }
  int limit(int dp) const { return dp <= LIMIT_GRANICA ? LIMIT_POCZATEK : LIMIT_POZNIEJ; }
  // R-DS-59: ostatnia pochwała, która nie jest kartą (karty serii poza limitem 1 na 7 dni)
  std::string ostatniaPochwala() const {
    std::string d;
    for (auto& x : s.p) {
      const Porada* p = k.porada(x.id);
      if (p && p->rodzaj == Rodzaj::POCHWALA && !p->karta && x.data > d) d = x.data;
    }
    return d;
  }
};

int limitPochwaly(const Porada& p) { return p.ma(FL_RAZ) ? 1 : p.ma(FL_DWA_RAZY) ? 2 : POCHWALA_LIMIT; }

struct Kandydat {
  const Porada* p = nullptr;
  std::string regula, dataNocy;
  int kol = 0;
  bool dzisiejszy = true;
};

// warunek porady z danych: któryś token bez `!` aktywny i żaden z `!`
bool aktywnyDane(const Porada& p, const std::set<std::string>& t, std::string& regula) {
  for (auto& w : p.weta)
    if (t.count(w)) return false;
  for (auto& w : p.warunek)
    if (!tokenAnkiety(w) && t.count(w)) {
      regula = w;
      return true;
    }
  return false;
}
bool weto(const Porada& p, const std::set<std::string>& t) {
  for (auto& w : p.weta)
    if (t.count(w)) return true;
  return false;
}
// token `id` albo jego wariant (`id/…`)
bool tokenZ(const std::set<std::string>& t, const std::string& id) {
  for (auto it = t.lower_bound(id); it != t.end() && it->compare(0, id.size(), id) == 0; ++it)
    if (it->size() == id.size() || (*it)[id.size()] == '/') return true;
  return false;
}
// warunek porady z tokenem `pochwala/…` (zachowanie tej nocy; karta serii `M-20/n` go nie ma)
bool zaNoc(const Porada& p) {
  for (auto& t : p.warunek)
    if (t.compare(0, 9, "pochwala/") == 0) return true;
  return false;
}
// R-DS-60: żadnej pochwały, także zaległej, po nocy niepewnej, częściowej, z E-20 albo S-K-12, w dniu z ekranem
// problemu albo lekarskim (`ekran/problem`) i bez danych tej nocy
bool wetoPochwal(const Doba& d) {
  if (d.bezNocy || !d.nocZTerapia) return true;
  for (const char* x : WETO_POCHWAL)
    if (tokenZ(d.tokeny, x)) return true;
  return false;
}

void dodajZalegla(Stan& s, const Kandydat& c, const std::string& dzis) {
  for (auto& z : s.z)
    if (bazaId(z.id) == c.p->baza) return;
  s.z.push_back({c.dzisiejszy ? dzis : c.dataNocy, c.p->id, c.regula});
}

// tokeny doby z fazą (R-DS-68): w starcie `faza/start` i każdy token także jako `{token}@start`
std::set<std::string> tokenyFazy(const std::set<std::string>& t, Etap e) {
  if (e != Etap::START) return t;
  std::set<std::string> w = t;
  for (auto& x : t) w.insert(x + "@start");
  w.insert("faza/start");
  return w;
}
// nowa doba poranka: liczniki, przerwa, zaległe, wyciek, zmiana trybu i `start.dzien`
void nowaDoba(const Katalog& k, Stan& s, const Ustawienia& u, const Doba& d, std::vector<std::string>& log) {
  if (s.doba == d.dzis) return;
  bool pierwsza = s.doba.empty();
  // R-DS-06: porada z danych wybrana w poprzedniej dobie, ale nienarysowana (blok zajęty ekranami z działaniem,
  // wieczór przez W1…W3), czeka jako zaległa od daty tamtej doby
  const Porada* wcz = s.wDzis.empty() || s.wDzis >= d.dzis || s.wZrodlo != "dane" ? nullptr : k.porada(s.wId);
  if (wcz && wcz->rodzaj == Rodzaj::ZDARZENIE && !wcz->ma(FL_PRZERWA)) {
    bool jest = false;
    for (auto& x : s.p) jest = jest || x.data == s.wDzis;
    for (auto& z : s.z) jest = jest || bazaId(z.id) == wcz->baza;
    if (!jest) {
      s.z.push_back({s.wDzis, s.wId, s.wRegula});
      log.push_back("dobry start: " + s.wId + " z " + s.wDzis + " nienarysowana, czeka jako zaległa (R-DS-06)");
    }
  }
  s.doba = d.dzis;
  s.wDzis.clear();
  s.d0ZRzedu = d.d0 ? s.d0ZRzedu + 1 : 0;
  if (d.dzis > s.start && d.nocZTerapia) s.noceTerapii++;
  if (s.ankietaOd.empty() && s.noceTerapii >= ANKIETA_NOCE) s.ankietaOd = d.dzis;  // R-DS-26
  // R-DS-11: przerwa w terapii (porada z flagą `przerwa` ma warunek przerwy) wstrzymuje plan
  bool przerwa = false;
  for (auto& p : k.porady) {
    std::string r;
    if (p.ma(FL_PRZERWA) && aktywnyDane(p, d.tokeny, r)) przerwa = true;
  }
  if (s.stan == "przerwa" && d.nocZTerapia) {
    s.stan = "aktywny";
    log.push_back("dobry start: przerwa w terapii skończona, doba programu " + std::to_string(dobaProgramu(s, d.dzis)));
  }
  if (przerwa && s.stan == "aktywny") {
    s.stan = "przerwa";
    log.push_back("dobry start: przerwa w terapii, plan wstrzymany");
  }
  if (s.stan == "przerwa") s.przerwaDni++;
  // R-DS-06: zaległe najwyżej 2 dni, pochwały 1 dobę (R-DS-59)
  s.z.erase(std::remove_if(s.z.begin(), s.z.end(),
                           [&](const WpisZ& z) {
                             const Porada* p = k.porada(z.id);
                             int ile = p && p->rodzaj == Rodzaj::POCHWALA ? POCHWALA_CZEKA_DNI : ZALEGLA_DNI;
                             return !p || dniMiedzy(z.data, d.dzis) > ile;
                           }),
            s.z.end());
  // doby z wyciekiem w danych (flaga `bez_wycieku_7` porad z ankiety)
  for (auto& p : k.porady) {
    std::string r;
    if (p.ma(FL_WYCIEK) && aktywnyDane(p, d.tokeny, r)) {
      s.wyciek.push_back(d.dzis);
      break;
    }
  }
  s.wyciek.erase(std::remove_if(s.wyciek.begin(), s.wyciek.end(),
                                [&](const std::string& x) { return dniMiedzy(x, d.dzis) > 2 * OKNO_DNI; }),
                 s.wyciek.end());
  // R-DS-45: zmiana trybu aparatu w trakcie programu
  std::string t = nazwaTrybu(d.tryb);
  if (!t.empty() && !s.tryb.empty() && t != s.tryb) {
    for (auto& x : s.p) {
      const Porada* p = k.porada(x.id);
      if (!p || p->rodzaj != Rodzaj::PLAN || (p->tryby & TR_WSZYSTKIE)) continue;
      const Porada* n = wariantDoby(k, p->faza, p->dzien, d.tryb, u.nawilzacz);
      if (n && n->id != p->id &&
          std::find(s.trybPoZmianie.begin(), s.trybPoZmianie.end(), n->id) == s.trybPoZmianie.end())
        s.trybPoZmianie.push_back(n->id);
    }
    log.push_back("dobry start: tryb " + s.tryb + " → " + t + ", porady w nowym wariancie: " +
                  std::to_string(s.trybPoZmianie.size()));
  }
  if (!t.empty()) s.tryb = t;
  // R-DS-65: `start.dzien` zmieniony w trakcie: ta doba to doba programu N (pokazane zostają)
  int n = dzienStartu(u, k);
  if (!pierwsza && n != s.dzienUstawiony) {
    s.dzienUstawiony = n;
    s.start = d.dzis;
    s.przerwaDni = 0;
    log.push_back("dobry start: start.dzien zmieniony, doba programu " + std::to_string(n));
  }
}

// tokeny ostatniej ankiety tygodniowej: (data ostatniego zapisu, tydzień)
struct Ankieta {
  std::string data, tydzien;
  std::set<std::string> tokeny;
};
Ankieta ostatniaAnkieta(const Stan& s) {
  Ankieta a;
  for (auto& o : s.o)
    if (o.tydzien != "start" && o.data >= a.data) {
      a.data = o.data;
      a.tydzien = o.tydzien;
    }
  for (auto& o : s.o)
    if (o.tydzien == a.tydzien && !a.tydzien.empty())
      for (auto& kod : dziel(o.kody, ','))
        if (!kod.empty()) a.tokeny.insert(o.pytanie + "=" + kod);
  return a;
}
int nrTygodnia(const std::string& t) { return t.size() > 1 && t[0] == 't' ? liczba(t.substr(1), 999) : -1; }

}  // namespace

std::string tydzienAnkiety(const Stan& s, const std::string& dzis) {
  if (s.ankietaOd.empty() || !dataOk(dzis) || dzis < s.ankietaOd) return "t1";
  return "t" + std::to_string(1 + dniMiedzy(s.ankietaOd, dzis) / ANKIETA_CO_DNI);
}
bool ankietaCzeka(const Stan& s, const std::string& dzis) {
  if (s.ankietaOd.empty() || !dataOk(dzis) || dzis < s.ankietaOd || s.stan == "zakonczony") return false;
  std::string t = tydzienAnkiety(s, dzis);
  for (auto& o : s.o)
    if (o.tydzien == t) return false;
  return true;
}

void zapiszAnkiete(const Katalog& k, Stan& s, const std::string& dzis,
                   const std::vector<std::pair<std::string, std::vector<int>>>& odp) {
  std::string t = tydzienAnkiety(s, dzis);
  for (auto& x : odp) {
    const Pytanie* q = k.pytanieId(x.first);
    if (!q) continue;
    std::string tyd = q->odTygodnia == 0 ? "start" : t;
    std::vector<int> kody;
    for (int c : x.second)
      if (c >= 1 && c <= (int)q->odpowiedzi.size() && std::find(kody.begin(), kody.end(), c) == kody.end()) kody.push_back(c);
    if (q->typ == "jedna" && kody.size() > 1) kody.resize(1);
    if (q->wylaczna && std::find(kody.begin(), kody.end(), q->wylaczna) != kody.end()) kody = {q->wylaczna};
    std::sort(kody.begin(), kody.end());
    std::string kt;
    for (int c : kody) kt += (kt.empty() ? "" : ",") + std::to_string(c);
    s.o.erase(std::remove_if(s.o.begin(), s.o.end(),
                             [&](const WpisO& o) { return o.tydzien == tyd && o.pytanie == q->id; }),
              s.o.end());
    s.o.push_back({dzis, tyd, q->id, kt});
  }
}

// ---------- wybór ----------
// tryb doby (dyrygent przy wyborze z dziennika nie podaje trybu nocy; bez nocy tryb z programu, R-DS-44)
static Tryb trybDnia(const Stan& s, const Doba& d) {
  if (d.tryb != Tryb::NIEZNANY) return d.tryb;
  if (d.bezNocy) return s.tryb.empty() ? Tryb::CPAP : trybZNazwy(s.tryb);
  return Tryb::NIEZNANY;
}
static Faza fazaPlanu(Etap e) {
  return e == Etap::START ? Faza::START_RANO : e == Etap::ADAPTACJA ? Faza::ADAPTACJA : Faza::UTRWALENIE;
}
static int poczatekEtapu(const Katalog& k, Etap e) {
  return e == Etap::START ? 1 : e == Etap::ADAPTACJA ? k.koniecStartu + 1 : k.koniecAdaptacji + 1;
}
// pierwsza niepokazana porada kolejki fazy `f` z dobą nominalną ≤ `dp` (i ≥ doby startu programu, R-DS-65)
static const Porada* zKolejki(const Katalog& k, const Stan& s, Faza f, int dp, Tryb t, int n) {
  Liczniki L{k, s};
  const Porada* best = nullptr;
  for (auto& p : k.porady) {
    if (p.rodzaj != Rodzaj::PLAN || p.faza != f || p.dzien > dp || p.dzien < s.dzienUstawiony) continue;
    if (best && best->dzien <= p.dzien) continue;
    if (!trybPasuje(p, t) || !nawilzaczPasuje(p, n) || L.planPokazany(p)) continue;
    best = &p;
  }
  return best;
}

WyborPorady wybierz(const Katalog& k, Stan& s, const Ustawienia& u, const Doba& d) {
  WyborPorady w;
  if (!k.jest()) {
    w.log.push_back("dobry start: brak pliku porad, program czeka (R-DS-36)");
    return w;
  }
  if (!u.program) {  // R-DS-01, R-DS-32: wyłączony; stan zostaje
    if (!s.start.empty() && (s.stan == "aktywny" || s.stan == "przerwa")) {
      s.stan = "zatrzymany";
      w.log.push_back("dobry start: program wyłączony w dobie programu " + std::to_string(dobaProgramu(s, d.dzis)));
    }
    return w;
  }
  if (!dataOk(d.dzis)) return w;  // R-DS-42: bez ważnego czasu
  if (s.start.empty() && !s.odNowa.empty() && (s.odNowa == d.dzis || !dataOk(s.odNowa))) {
    // R-DS-33: „Zacznij od nowa” w tej dobie: dzień włączenia jutro, dziś bez porady (także po poradzie z rana)
    s.start = dodajDni(d.dzis, 1);
    s.odNowa.clear();
    s.tryb = nazwaTrybu(d.tryb);
    w.log.push_back("dobry start: od nowa, dzień włączenia " + s.start + " (R-DS-33)");
    return w;
  }
  if (s.start.empty()) {
    Stan n = nowyStan(u, k);
    n.o = s.o;  // pytanie startowe zapisane przed pierwszą dobą
    s = n;
    for (auto& b : s.bledy) w.log.push_back(b);
    s.bledy.clear();
    s.odNowa.clear();
    s.start = d.dzis;
    s.tryb = nazwaTrybu(d.tryb);
    w.log.push_back("dobry start: włączony, dzień włączenia " + d.dzis + ", doba programu " +
                    std::to_string(s.dzienUstawiony));
  }
  if (s.start > d.dzis) return w;  // po „od nowa”: program rusza od dnia włączenia
  if (s.stan == "zatrzymany") {
    // R-DS-56: doby z wyłączonym programem nie liczą się do doby programu (jak przerwa w terapii, R-DS-11)
    int wyl = dataOk(s.doba) && s.doba < d.dzis ? dniMiedzy(s.doba, d.dzis) - 1 : 0;
    if (wyl > 0) s.przerwaDni += wyl;
    s.stan = "aktywny";
    w.log.push_back("dobry start: włączony ponownie, dalej od doby programu " + std::to_string(dobaProgramu(s, d.dzis)));
  }
  if (s.stan == "zakonczony") return w;
  // fail-path R-DS-69: skok daty w przód o więcej niż SKOK_DNI dób między dwoma porankami (zły czas z sieci) nie kończy
  // programu: `wstrzymany`; rusza po powrocie daty albo po SKOK_POTWIERDZ kolejnych dobach z nową datą
  if (s.stan == "wstrzymany") {
    if (dataOk(s.doba) && d.dzis >= s.doba && dniMiedzy(s.doba, d.dzis) <= SKOK_DNI) {
      s.stan = "aktywny";
      s.skokData.clear();
      s.skokIle = 0;
      w.log.push_back("dobry start: data poranka wróciła (" + d.dzis + "), program dalej");
    } else if (d.dzis != s.skokData) {
      s.skokIle = dataOk(s.skokData) && dodajDni(s.skokData, 1) == d.dzis ? s.skokIle + 1 : 1;
      s.skokData = d.dzis;
      if (s.skokIle >= SKOK_POTWIERDZ) {
        s.stan = "aktywny";
        s.skokData.clear();
        s.skokIle = 0;
        w.log.push_back("dobry start: nowa data " + d.dzis + " przez " + std::to_string(SKOK_POTWIERDZ) +
                        " doby, przyjęta jako prawdziwa");
      }
    }
    if (s.stan == "wstrzymany") return w;
  } else if ((s.stan == "aktywny" || s.stan == "przerwa") && dataOk(s.doba) && d.dzis > s.doba &&
             dniMiedzy(s.doba, d.dzis) > SKOK_DNI) {
    s.stan = "wstrzymany";
    s.skokData = d.dzis;
    s.skokIle = 1;
    w.log.push_back("dobry start: skok daty poranka z " + s.doba + " na " + d.dzis + ", program wstrzymany");
    return w;
  }
  for (auto& x : s.p)
    if (x.data > d.dzis) {  // R-DS-42: zegar cofnięty
      w.log.push_back("dobry start: data poranka przed ostatnim wpisem, bez nowej porady");
      return w;
    }
  // R-DS-43: porada dnia z dziennika (porada wieczorna startu ma osobny wpis)
  for (auto& x : s.p)
    if (x.data == d.dzis && !wpisWieczorny(k, x)) {
      w.porada = k.porada(x.id);
      w.zrodlo = zrodloZNazwy(x.zrodlo);
      w.regula = x.regula;
      w.dataNocy = d.dzis;
      w.narysowana = true;
      w.telefon = x.gdzie == "telefon";
      for (auto& z : s.z)
        if (z.id == x.id) w.dataNocy = z.data;
      return w;
    }
  if (s.wDzis == d.dzis) {
    w.porada = k.porada(s.wId);
    w.zrodlo = zrodloZNazwy(s.wZrodlo);
    w.regula = s.wRegula;
    w.tylkoWieczor = s.wWieczor;
    w.bezWpisu = s.wBezWpisu;
    w.dataNocy = d.dzis;
    for (auto& z : s.z)
      if (z.id == s.wId) w.dataNocy = z.data;
    w.telefon = w.porada && u.telefon && w.porada->ma(FL_OBJAW) && !w.porada->karta;
    return w;
  }
  {  // fail-path R-DS-69: panel bez poranków (wyłączony) dłużej niż do końca programu → koniec, bez porad
    if (!s.planKoniec && dataOk(s.doba) && s.doba < d.dzis && dobaProgramu(s, s.doba) <= k.miejsc &&
        dobaProgramu(s, d.dzis) > k.miejsc) {
      s.stan = "zakonczony";
      s.doba = d.dzis;
      s.wDzis.clear();
      w.log.push_back("dobry start: pierwszy poranek po końcu programu (doba programu " +
                      std::to_string(dobaProgramu(s, d.dzis)) + "), program zakończony");
      return w;
    }
  }
  nowaDoba(k, s, u, d, w.log);
  Liczniki L{k, s};
  int nawilz = u.nawilzacz;
  Tryb tryb = trybDnia(s, d);
  int dp = dobaProgramu(s, d.dzis);
  Etap e = k.etap(dp);
  if (s.planKoniec || e == Etap::KONIEC) {  // R-DS-69: po poradzie końcowej albo po ostatniej dobie programu
    s.stan = "zakonczony";
    w.log.push_back("dobry start: program zakończony (doba programu " + std::to_string(dp) + ")");
    return w;
  }
  std::set<std::string> tk = tokenyFazy(d.tokeny, e);
  if (s.dzienUstawiony > 1)  // `start.dzien` > 1: pierwsze noce były przed programem (DS-P01, DS-P02, DS-P06 zużyte)
    for (const char* x : {"pochwala/pierwsza_noc", "pochwala/pierwsze_4h", "pochwala/cala_noc"}) {
      tk.erase(x);
      tk.erase(std::string(x) + "@start");
    }
  auto ustaw = [&](const Porada* p, Zrodlo z, const std::string& reg, const std::string& noc) {
    w.porada = p;
    w.zrodlo = z;
    w.regula = reg;
    w.dataNocy = noc.empty() ? d.dzis : noc;
  };
  auto koniec = [&]() {
    if (w.porada) {
      w.tylkoWieczor = d.d0;
      w.telefon = u.telefon && w.porada->ma(FL_OBJAW) && !w.porada->karta;
      s.wDzis = d.dzis;
      s.wId = w.porada->id;
      s.wZrodlo = nazwaZrodla(w.zrodlo);
      s.wRegula = w.regula;
      s.wWieczor = w.tylkoWieczor;
      s.wBezWpisu = w.bezWpisu;
      w.log.push_back(std::string("dobry start: porada dnia ") + w.porada->id + " (" + nazwaZrodla(w.zrodlo) +
                      (w.regula.empty() ? "" : ", " + w.regula) + (w.tylkoWieczor ? ", tylko wieczorem" : "") +
                      ", doba programu " + std::to_string(dp) + ")");
    }
    return w;
  };
  // R-DS-11: przerwa w terapii: porada z flagą `przerwa` jest poradą dnia, nowe pokazanie co 7 dni
  if (s.stan == "przerwa") {
    for (auto& p : k.porady)
      if (p.ma(FL_PRZERWA) && trybPasuje(p, tryb) && nawilzaczPasuje(p, nawilz)) {
        ustaw(&p, Zrodlo::DANE, p.warunek.empty() ? "" : p.warunek[0], "");
        w.bezWpisu = !L.oknoOk(p.baza, d.dzis);
        return koniec();
      }
    return w;
  }
  // R-DS-58, R-DS-60: pochwały z danych tej nocy; wspólne weto (noc niepewna, E-20, S-K-12, ekran problemu albo
  // lekarski, noc częściowa, brak danych nocy) dla wszystkich pochwał, także zaległych
  const bool bezPochwal = wetoPochwal(d);
  std::vector<Kandydat> kp;
  if (!bezPochwal)
    for (auto& p : k.porady) {
      std::string r;
      if (p.rodzaj != Rodzaj::POCHWALA || !aktywnyDane(p, tk, r)) continue;
      const Porada* v = wariant(k, p.baza, tryb, nawilz);
      if (!v) continue;
      Kandydat c;
      c.p = v;
      c.regula = r;
      c.kol = p.kol;
      kp.push_back(c);
    }
  // fail-path R-DS-59: pochwała, która przegrała z poradą z danych albo z ankiety, czeka 1 dobę (karty serii też)
  auto odlozPochwaly = [&]() {
    for (auto& c : kp)
      if (L.pokazania(c.p->baza) < limitPochwaly(*c.p)) dodajZalegla(s, c, d.dzis);
  };
  // porady z danych ostatniej nocy i zaległe (R-DS-05, R-DS-06)
  std::vector<Kandydat> kand;
  auto dodaj = [&](const Kandydat& c) {
    for (auto& x : kand)
      if (x.p->baza == c.p->baza) return;
    kand.push_back(c);
  };
  for (auto& p : k.porady) {
    std::string r;
    if (p.rodzaj != Rodzaj::ZDARZENIE || !aktywnyDane(p, tk, r)) continue;
    const Porada* v = wariant(k, p.baza, tryb, nawilz);  // R-DS-38, R-DS-29, R-DS-46
    if (!v) continue;
    Kandydat c;
    c.p = v;
    c.regula = r;
    c.dataNocy = d.dzis;
    c.kol = p.kol;
    dodaj(c);
  }
  for (auto& z : s.z) {
    const Porada* p = k.porada(z.id);
    const Porada* v = p && p->rodzaj == Rodzaj::ZDARZENIE ? wariant(k, p->baza, tryb, nawilz) : nullptr;
    if (!v) continue;
    Kandydat c;
    c.p = v;
    c.regula = z.regula;
    c.dataNocy = z.data;
    c.kol = v->kol;
    c.dzisiejszy = false;
    dodaj(c);
  }
  std::stable_sort(kand.begin(), kand.end(), [](const Kandydat& a, const Kandydat& b) { return a.kol < b.kol; });
  if (d.w0) {  // R-DS-04: W0 trzyma ekran i wieczorem; porady wyzwalane czekają jako zaległe
    for (auto& c : kand)
      if (c.dzisiejszy) dodajZalegla(s, c, d.dzis);
    w.log.push_back("dobry start: ekran z rana trzyma także wieczór (W0), bez porady");
    return w;
  }
  if (e == Etap::START && d.d0) {  // fazie start w dniu z D0 rano bez porady; porada z danych czeka (rozdz. 3)
    for (auto& c : kand)
      if (c.dzisiejszy) dodajZalegla(s, c, d.dzis);
    w.log.push_back("dobry start: start, ekran z rana trzyma dzień, porada poranna czeka");
    return w;
  }
  // R-DS-56, R-DS-69: ostatnia doba fazy albo programu: porada końcowa przed poradą wyzwalaną
  if (e != Etap::START) {
    int ost = e == Etap::ADAPTACJA ? k.koniecAdaptacji : k.miejsc;
    Flaga fl = e == Etap::ADAPTACJA ? FL_KONCOWA_FAZY : FL_KONCOWA;
    if (dp >= ost)
      for (auto& p : k.porady)
        if (p.rodzaj == Rodzaj::PLAN && p.faza == fazaPlanu(e) && p.ma(fl) && !L.planPokazany(p)) {
          const Porada* v = wariantDoby(k, p.faza, p.dzien, tryb, nawilz);
          if (!v) break;
          ustaw(v, Zrodlo::PLAN, "", "");
          w.log.push_back("dobry start: doba programu " + std::to_string(dp) + ", koniec fazy (R-DS-56)");
          return koniec();
        }
  }
  // R-DS-07, R-DS-68: limit porad wyzwalanych (w starcie bez limitu 2 na 7, najwyżej jedna rano)
  bool grupaOk = e == Etap::START || L.wyzwalaneWOknie(d.dzis) < L.limit(dp);
  // R-DS-08: stopnie (porada wyczerpała pokazania → stopień wyżej bez jego warunku), okno 7 dni
  auto dopuszczalny = [&](const Porada* p) -> const Porada* {
    for (int i = 0; p && i < 8; i++) {
      if (p->ma(FL_BEZ_LIMITU) || L.pokazania(p->baza) < RAZY_W_PROGRAMIE) break;
      p = p->stopienWyzej.empty() ? nullptr : k.porada(p->stopienWyzej);
      if (p) p = wariant(k, p->baza, tryb, nawilz);
    }
    if (p && !p->ma(FL_BEZ_LIMITU) && L.pokazania(p->baza) >= RAZY_W_PROGRAMIE) return nullptr;
    if (p && !L.oknoOk(p->baza, d.dzis)) return nullptr;
    return p;
  };
  for (size_t i = 0; i < kand.size(); i++) {
    const Kandydat& c = kand[i];
    if (!grupaOk && !c.p->ma(FL_POZA_LIMITEM)) continue;
    const Porada* p = dopuszczalny(c.p);
    if (!p) continue;
    // R-DS-49: pojęcie z porady planu jeszcze niepokazanej → najpierw porada objaśniająca, kandydat zaległy
    const Porada* obj = p->wymagaPorady.empty() ? nullptr : k.porada(p->wymagaPorady);
    if (obj && obj->rodzaj == Rodzaj::PLAN && !L.planPokazany(*obj)) {
      const Porada* ov = wariantDoby(k, obj->faza, obj->dzien, tryb, nawilz);
      if (ov) {
        Kandydat zc = c;
        zc.p = p;
        dodajZalegla(s, zc, d.dzis);
        for (size_t j = 0; j < kand.size(); j++)
          if (j != i && kand[j].dzisiejszy) dodajZalegla(s, kand[j], d.dzis);
        odlozPochwaly();
        ustaw(ov, Zrodlo::PLAN, "", "");
        w.log.push_back("dobry start: " + p->id + " czeka na objaśnienie " + ov->id + " (R-DS-49)");
        return koniec();
      }
    }
    for (size_t j = 0; j < kand.size(); j++)
      if (j != i && kand[j].dzisiejszy) dodajZalegla(s, kand[j], d.dzis);
    odlozPochwaly();
    ustaw(p, Zrodlo::DANE, c.regula, c.dataNocy);
    return koniec();
  }
  for (auto& c : kand)
    if (c.dzisiejszy && !grupaOk) dodajZalegla(s, c, d.dzis);  // R-DS-07: limit wyczerpany
  // porady z ankiety (R-DS-27, R-DS-28), te same limity
  if (grupaOk) {
    Ankieta a = ostatniaAnkieta(s);
    std::vector<Kandydat> ka;
    for (auto& p : k.porady) {
      if (a.tokeny.empty()) break;
      std::string tok;
      for (auto& t : p.warunek)
        if (tokenAnkiety(t) && a.tokeny.count(t)) tok = t;
      if (tok.empty() || !p.kolAnkiety) continue;
      bool weto = false;
      for (auto& t : p.weta)
        if (a.tokeny.count(t) || d.tokeny.count(t)) weto = true;
      if (weto) continue;
      const size_t rowny = tok.find('=');
      std::string pyt = tok.substr(0, rowny);
      // kod odpowiedzi z tokenu „pytanie=kod”; bez znaku „=” żaden kod nie pasuje
      const bool maKod = rowny != std::string::npos;
      const std::string kodTok = maKod ? tok.substr(rowny + 1) : std::string();
      if (p.ma(FL_PIERWSZA)) {  // pierwsza taka odpowiedź w programie
        bool wczesniej = false;
        for (auto& o : s.o)
          if (o.pytanie == pyt && o.tydzien != a.tydzien)
            for (auto& kd : dziel(o.kody, ','))
              if (maKod && kd == kodTok) wczesniej = true;
        if (wczesniej) continue;
      }
      if (p.ma(FL_DWA_RAZY)) {  // ta sama odpowiedź w poprzedniej wypełnionej ankiecie
        int nr = nrTygodnia(a.tydzien), best = -1;
        bool jest = false;
        for (auto& o : s.o) {
          int t = nrTygodnia(o.tydzien);
          if (o.pytanie != pyt || o.kody.empty() || t < 0 || t >= nr || t < best) continue;
          if (t > best) jest = false;
          best = t;
          for (auto& kd : dziel(o.kody, ','))
            if (maKod && kd == kodTok) jest = true;
        }
        if (!jest) continue;
      }
      if (p.ma(FL_BEZ_WYCIEKU_7)) {
        bool wyc = false;
        for (auto& x : s.wyciek) {
          int r = dniMiedzy(x, a.data);
          if (r >= 0 && r <= OKNO_DNI) wyc = true;
        }
        if (wyc) continue;
      }
      std::string ost = L.ostatnio(p.baza);
      if (!ost.empty() && ost >= a.data) continue;  // już pokazana po tej ankiecie
      const Porada* v = wariant(k, p.baza, tryb, nawilz);
      if (!v) continue;
      Kandydat c;
      c.p = v;
      c.regula = tok;
      c.kol = p.kolAnkiety;
      ka.push_back(c);
    }
    std::stable_sort(ka.begin(), ka.end(), [](const Kandydat& x, const Kandydat& y) { return x.kol < y.kol; });
    for (auto& c : ka) {
      const Porada* p = dopuszczalny(c.p);
      if (p != c.p) continue;  // z ankiety bez stopni
      odlozPochwaly();
      ustaw(p, Zrodlo::ANKIETA, c.regula, "");
      return koniec();
    }
  }
  // R-DS-58, R-DS-59: pochwała z danych nocy (najwyżej jedna na dobę; poza startem 1 na 7 dni poza kartami serii)
  if (!bezPochwal) {
    // pochwała, która wczoraj nie stanęła (czeka 1 dobę), bez dnia z ekranem problemu; powtarzalna pochwała za
    // zachowanie nocy (`pochwala/…` bez flagi `raz`) tylko, gdy warunek spełnia też ta noc (nie przeczy ostatniej
    // nocy); jednorazowa („pierwsza …”) i karta serii czekają bez tego, z liczbami nocy, której dotyczą
    for (auto& z : s.z) {
      const Porada* p = k.porada(z.id);
      if (!p || p->rodzaj != Rodzaj::POCHWALA || weto(*p, tk) || (zaNoc(*p) && !p->ma(FL_RAZ))) continue;
      bool jest = false;
      for (auto& c : kp) jest = jest || c.p->baza == p->baza;
      if (jest) continue;
      Kandydat c;
      c.p = p;
      c.regula = z.regula;
      c.dataNocy = z.data;
      c.kol = p->kol;
      c.dzisiejszy = false;
      kp.push_back(c);
    }
    std::stable_sort(kp.begin(), kp.end(), [](const Kandydat& a, const Kandydat& b) { return a.kol < b.kol; });
    std::string ostP = L.ostatniaPochwala();
    bool oknoP = e == Etap::START || ostP.empty() || dniMiedzy(ostP, d.dzis) >= POCHWALA_OKNO_DNI;
    for (size_t i = 0; i < kp.size(); i++) {
      const Kandydat& c = kp[i];
      if (L.pokazania(c.p->baza) >= limitPochwaly(*c.p) || (!c.p->karta && !oknoP)) continue;
      for (size_t j = 0; j < kp.size(); j++)
        if (j != i && kp[j].dzisiejszy && L.pokazania(kp[j].p->baza) < limitPochwaly(*kp[j].p)) dodajZalegla(s, kp[j], d.dzis);
      ustaw(c.p, Zrodlo::POCHWALA, c.regula, c.dataNocy);
      return koniec();
    }
    for (auto& c : kp)
      if (c.dzisiejszy && L.pokazania(c.p->baza) < limitPochwaly(*c.p)) dodajZalegla(s, c, d.dzis);
  }
  // dzień z D0 (poza startem): porada planu dopiero od D0_DOBY. kolejnej doby, wtedy wieczorem (R-DS-04)
  if (d.d0 && s.d0ZRzedu < D0_DOBY) {
    w.log.push_back("dobry start: ekran z rana trzyma dzień, porada planu czeka (D0 " + std::to_string(s.d0ZRzedu) + ")");
    return w;
  }
  // R-DS-56, R-DS-65: niepokazane porady podstawowe z dób przed startem programu albo z wcześniejszych faz, po jednej
  {
    int pocz = poczatekEtapu(k, e);
    const Porada* best = nullptr;
    for (auto& p : k.porady) {
      if (p.rodzaj != Rodzaj::PLAN || !p.ma(FL_PODSTAWOWA) || L.planPokazany(p)) continue;
      if (p.dzien >= s.dzienUstawiony && p.dzien >= pocz) continue;
      const Porada* v = wariantDoby(k, p.faza, p.dzien, tryb, nawilz);
      if (v && (!best || v->kol < best->kol)) best = v;
    }
    if (best) {
      ustaw(best, Zrodlo::PODSTAWOWA, "", "");
      return koniec();
    }
  }
  // R-DS-45: warianty nowego trybu porad już pokazanych w starym
  while (!s.trybPoZmianie.empty()) {
    const Porada* p = k.porada(s.trybPoZmianie.front());
    if (p && trybPasuje(*p, tryb)) {
      bool juz = false;
      for (auto& x : s.p)
        if (x.id == p->id) juz = true;
      if (!juz) {
        ustaw(p, Zrodlo::TRYB, "", "");
        return koniec();
      }
    }
    s.trybPoZmianie.erase(s.trybPoZmianie.begin());
  }
  // plan bieżącej fazy (R-DS-54, R-DS-69): pierwsza niepokazana porada z dobą nominalną ≤ doby programu
  const Porada* p = zKolejki(k, s, fazaPlanu(e), dp, tryb, nawilz);
  if (p) {
    ustaw(p, Zrodlo::PLAN, "", "");
    return koniec();
  }
  w.log.push_back("dobry start: doba programu " + std::to_string(dp) + " bez porady dnia");
  return w;
}

WyborPorady wybierzWieczor(const Katalog& k, Stan& s, const Ustawienia& u, const Doba& d) {
  WyborPorady dz = wybierz(k, s, u, d);
  if (!k.jest() || !u.program || !dataOk(d.dzis) || s.start.empty() || s.start > d.dzis || s.stan == "zakonczony" ||
      s.stan == "wstrzymany")
    return dz;
  int dp = dobaProgramu(s, d.dzis);
  if (k.etap(dp) != Etap::START || s.stan == "przerwa") return dz;  // poza startem ta sama porada co w bloku
  WyborPorady w;
  if (d.w0) return w;  // fail-path R-DS-55: W0 trzyma wieczór, porada czeka do następnego wieczoru
  for (auto& x : s.p)
    if (x.data == d.dzis && wpisWieczorny(k, x)) {
      w.porada = k.porada(x.id);
      w.zrodlo = zrodloZNazwy(x.zrodlo);
      w.dataNocy = d.dzis;
      w.narysowana = true;
      w.telefon = x.gdzie == "telefon";
      return w;
    }
  Tryb tryb = trybDnia(s, d);
  const Porada* p = s.eDzis == d.dzis ? k.porada(s.eId) : zKolejki(k, s, Faza::START_WIECZOR, dp, tryb, u.nawilzacz);
  if (!p) return w;
  s.eDzis = d.dzis;
  s.eId = p->id;
  w.porada = p;
  w.zrodlo = Zrodlo::PLAN;
  w.dataNocy = d.dzis;
  w.telefon = u.telefon && p->ma(FL_OBJAW) && !p->karta;
  w.log.push_back("dobry start: porada wieczorna " + p->id + " (doba programu " + std::to_string(dp) + ")");
  return w;
}

void zapiszPokazanie(const Katalog& k, Stan& s, const WyborPorady& w, const std::string& dzis, bool wieczor, bool telefon) {
  if (!w.porada || w.bezWpisu || w.narysowana || !dataOk(dzis)) return;
  bool wst = wieczornaStartu(w.porada) && w.zrodlo == Zrodlo::PLAN;
  for (auto& x : s.p)  // jedna porada dnia i (w starcie) jedna wieczorna na dobę (R-DS-05, R-DS-55)
    if (x.data == dzis && wpisWieczorny(k, x) == wst) return;
  const Porada& p = *w.porada;
  s.p.push_back({dzis, p.id, nazwaZrodla(w.zrodlo), w.regula, wieczor ? "wieczor" : "dzien", telefon ? "telefon" : "panel"});
  s.z.erase(std::remove_if(s.z.begin(), s.z.end(), [&](const WpisZ& z) { return bazaId(z.id) == p.baza; }), s.z.end());
  if (p.rodzaj == Rodzaj::PLAN && p.ma(FL_KONCOWA)) s.planKoniec = true;
  if ((w.zrodlo == Zrodlo::DANE || w.zrodlo == Zrodlo::ANKIETA) && !p.zamiast.empty()) {  // R-DS-09
    const Porada* z = k.porada(p.zamiast);
    if (z && z->rodzaj == Rodzaj::PLAN) {
      Liczniki L{k, s};
      if (!L.planPokazany(*z)) s.zaliczone.push_back({z->baza, dzis});
    }
  }
  if (w.zrodlo == Zrodlo::TRYB)
    s.trybPoZmianie.erase(std::remove(s.trybPoZmianie.begin(), s.trybPoZmianie.end(), p.id), s.trybPoZmianie.end());
  if (wst) s.eDzis.clear();
}

// ---------- teksty ----------
OpisPorady opis(const Katalog& k, const Stan& s, const WyborPorady& w) {
  (void)s;
  OpisPorady o;
  if (!w.porada) return o;
  const Porada& p = *w.porada;
  o.tygodni = k.tygodni();
  switch (w.zrodlo) {
    case Zrodlo::DANE:
      o.srodekBelki = "po nocy " + ddmm(w.dataNocy);
      o.etykieta = "porada do ostatniej nocy";
      break;
    case Zrodlo::POCHWALA:
      o.srodekBelki = "po nocy " + ddmm(w.dataNocy);
      o.etykieta = "za ostatnią noc";
      break;
    case Zrodlo::ANKIETA:
      o.srodekBelki = "z twojej ankiety";
      o.etykieta = "porada do ankiety";
      break;
    default: {  // plan (R-DS-57, tabela 6.1): napisy z fazy porady i jej doby nominalnej
      int dz = p.dzien;
      o.tydzien = dz > 0 ? (dz - 1) / 7 + 1 : 0;
      o.wTygodniu = dz > 0 ? (dz - 1) % 7 + 1 : 0;
      if (p.faza == Faza::START_RANO || p.faza == Faza::START_WIECZOR) {
        o.srodekBelki = "start · dzień " + std::to_string(dz) + " z " + std::to_string(k.koniecStartu);
        o.etykieta = p.faza == Faza::START_WIECZOR ? "na dziś wieczór" : "na dziś";
      } else if (p.faza == Faza::ADAPTACJA) {
        o.srodekBelki = "adaptacja · dzień " + std::to_string(dz) + " z " + std::to_string(k.koniecAdaptacji);
        o.etykieta = "tydzień " + std::to_string(o.tydzien) + " z " + std::to_string((k.koniecAdaptacji + 6) / 7);
      } else {
        std::set<int> doby;
        for (auto& x : k.porady)
          if (x.rodzaj == Rodzaj::PLAN && x.faza == Faza::UTRWALENIE) doby.insert(x.dzien);
        int n = (int)std::distance(doby.begin(), doby.lower_bound(dz)) + 1;
        o.srodekBelki = "utrwalenie · tydzień " + std::to_string(o.tydzien) + " z " + std::to_string(o.tygodni);
        o.etykieta = "porada " + std::to_string(n) + " z " + std::to_string(k.utrwalenieIle);
      }
    }
  }
  if (w.telefon) {
    o.tytul = TYTUL_TELEFON;
    o.tydzien = 0;
    return o;
  }
  o.tytul = k.tekst(p.tytul);
  o.tresc = k.tekst(p.tresc);
  o.krok = k.tekst(p.krok);
  o.qr = k.tekst(p.qr);
  o.fakty = fakty(k, p);
  return o;
}

std::vector<std::pair<std::string, std::string>> fakty(const Katalog& k, const Porada& p) {
  std::vector<std::pair<std::string, std::string>> w;
  std::string f = k.tekst(p.fakty);
  size_t a = 0;
  while (a < f.size()) {  // `liczba::podpis | liczba::podpis`
    size_t b = f.find(" | ", a);
    std::string para = f.substr(a, b == std::string::npos ? std::string::npos : b - a);
    size_t c = para.find("::");
    if (c != std::string::npos) w.push_back({para.substr(0, c), para.substr(c + 2)});
    if (b == std::string::npos) break;
    a = b + 3;
  }
  return w;
}

void faktyNaEkran(std::vector<std::pair<std::string, std::string>>& f) {
  for (auto& x : f) {
    // fail-path R-DS-60: brak nocy w oknie (`–`) albo 0 nocy → karta bez rzędu faktów
    const std::string& l = x.first;
    if (l.find("–") != std::string::npos || l == "0" || l.compare(0, 2, "0 ") == 0) {
      f.clear();
      return;
    }
    // sama liczba nad podpisem `nocy …`: odmiana (1 noc, 2–4 noce, 5 nocy, 12–14 nocy)
    if (!l.empty() && l.size() <= 6 && l.find_first_not_of("0123456789") == std::string::npos &&
        x.second.compare(0, 4, "nocy") == 0 && (x.second.size() == 4 || x.second[4] == ' '))
      x.second = std::string(nocyZRzedu(liczba(l, 999999))) + x.second.substr(4);
  }
}

std::string znacznik(const Katalog& k, const Stan& s, const Porada& p) {
  std::string d, gdzie;
  for (auto& x : s.p)
    if (x.id == p.id && x.data >= d) {
      d = x.data;
      gdzie = x.gdzie;
    }
  if (d.empty() && p.rodzaj == Rodzaj::PLAN) {
    bool inny = false;  // pokazana w innym wariancie: znacznik tylko przy tamtym wariancie
    for (auto& x : s.p) {
      const Porada* q = k.porada(x.id);
      if (q && q->baza == p.baza) inny = true;
    }
    for (auto& z : s.zaliczone)
      if (z.first == p.baza && !inny) d = z.second;
  }
  if (d.empty()) return "jeszcze nie";
  return (gdzie == "telefon" ? "na telefonie " : "pokazana ") + ddmm(d);
}

std::string naglowekSekcji(const Katalog& k, const Stan& s, const Ustawienia& u, const std::string& dzis) {
  if (!u.program) return "wyłączony";
  if (s.stan == "zakonczony") return "zakończony";
  std::string w;
  int dp = s.start.empty() || dzis < s.start ? dzienStartu(u, k) : dobaProgramu(s, dzis);
  Etap e = k.etap(dp);
  if (s.stan == "przerwa") w = "przerwa w terapii";
  else if (e == Etap::START) w = "start · dzień " + std::to_string(dp) + " z " + std::to_string(k.koniecStartu);
  else if (e == Etap::ADAPTACJA) w = "adaptacja · dzień " + std::to_string(dp) + " z " + std::to_string(k.koniecAdaptacji);
  else if (e == Etap::UTRWALENIE)
    w = "utrwalenie · tydzień " + std::to_string((dp - 1) / 7 + 1) + " z " + std::to_string(k.tygodni());
  else return "zakończony";
  if (ankietaCzeka(s, dzis)) w += " · ankieta czeka";
  return w;
}

// ---------- dane nocy ----------
namespace {
const NocProgramu* nocZ(const std::vector<NocProgramu>& noce, const std::string& data) {
  for (auto& n : noce)
    if (n.data == data) return &n;
  return nullptr;
}
const NocProgramu* poprzedniaZTerapia(const std::vector<NocProgramu>& noce, const std::string& dzis, int prog) {
  const NocProgramu* w = nullptr;
  for (auto& n : noce)
    if (n.data < dzis && n.m01 >= prog && (!w || n.data > w->data)) w = &n;
  return w;
}
}  // namespace

void tokenyPochwal(const std::vector<NocProgramu>& noce, const std::string& dzis, int seria, const ProgiPochwal& p,
                   std::set<std::string>& t) {
  const NocProgramu* n = nocZ(noce, dzis);
  if (!n || n->m01 < 0) return;
  // „pierwsza” z rekordów nocy, nie z samego limitu (po migracji z wersji 1, przy `start.dzien` > 1 i przy
  // terapii sprzed włączenia programu wcześniejsza taka noc w historii zużywa DS-P01, DS-P02, DS-P06)
  bool bylaTerapia = false, byly4h = false, bylaCala = false;
  for (auto& x : noce) {
    if (x.data >= dzis || x.m01 < 0) continue;
    bylaTerapia = bylaTerapia || x.m01 >= p.terapia;
    byly4h = byly4h || x.m01 >= p.m01Min;
    bylaCala = bylaCala || (x.m01 >= CALA_NOC_MIN && x.m17 == 0);
  }
  if (n->m01 >= p.terapia && !bylaTerapia) t.insert("pochwala/pierwsza_noc");
  if (n->m01 >= p.m01Min && !byly4h) t.insert("pochwala/pierwsze_4h");
  if (n->m01 >= CALA_NOC_MIN && n->m17 == 0 && !bylaCala) t.insert("pochwala/cala_noc");
  const NocProgramu* pop = poprzedniaZTerapia(noce, dzis, std::max(1, p.terapia));
  if (pop && pop->m01 < p.m01Min && n->m01 >= pop->m01 + DLUZEJ_MIN) t.insert("pochwala/dluzej");
  if (n->m01 >= p.m01Min && n->wyciek >= 0 && n->wyciek < p.wyciekUwaga) {
    t.insert("pochwala/szczelna");
    if (pop && pop->wyciek >= p.wyciekUwaga) t.insert("pochwala/szczelniej");
  }
  int tydz = 0;
  for (int i = 0; i < 7; i++) {
    const NocProgramu* x = nocZ(noce, dodajDni(dzis, -i));
    if (x && x->m01 >= p.terapia) tydz++;
  }
  if (tydz == 7 && seria < 7) t.insert("pochwala/tydzien_co_noc");
  if (n->m01 >= p.terapia && p.noceProblem > 0) {  // powrót po co najmniej E-10.noce_problem nocach bez terapii
    int bez = 0;
    for (int i = 1; i <= 60; i++) {
      const NocProgramu* x = nocZ(noce, dodajDni(dzis, -i));
      if (x && x->m01 >= p.terapia) break;
      bez++;
    }
    if (bez >= p.noceProblem && bez < 60) t.insert("pochwala/powrot");
  }
}

Znaczniki znacznikiNocy(const std::vector<NocProgramu>& noce, const std::string& dzis, const std::string& start,
                        int seria, const ProgiPochwal& p) {
  Znaczniki z;
  const NocProgramu* n = nocZ(noce, dzis);
  if (n && n->m01 >= 0) z.czas = n->m01;
  const NocProgramu* pop = poprzedniaZTerapia(noce, dzis, std::max(1, p.terapia));
  if (n && n->m01 >= 0 && pop && n->m01 >= pop->m01) z.oIleDluzej = n->m01 - pop->m01;
  z.nocyZRzedu = seria;
  if (n && n->wyciek >= 0) z.wyciek = n->wyciek;
  int zm = 0, z4 = 0, suma = 0, ile = 0, zmP = 0, sumaP = 0, ileP = 0;
  for (auto& x : noce) {
    if (x.data > dzis || x.m01 < 0) continue;
    bool tydz = dniMiedzy(x.data, dzis) < 7;
    if (tydz && x.m01 >= p.terapia) zm++;
    if (tydz && x.m01 >= p.m01Min) z4++;
    if (tydz && x.m01 >= p.terapia) { suma += x.m01; ile++; }
    if (dataOk(start) && x.data >= start && x.m01 >= p.terapia) { zmP++; sumaP += x.m01; ileP++; }
  }
  bool sa7 = false;
  for (auto& x : noce) sa7 = sa7 || (x.data <= dzis && dniMiedzy(x.data, dzis) < 7);
  if (sa7) {
    z.nocyZMaska = zm;
    z.nocy4h = z4;
  }
  if (ile > 0) z.srednio = (suma + ile / 2) / ile;
  if (dataOk(start)) z.nocyZMaskaProg = zmP;
  if (ileP > 0) z.srednioProg = (sumaP + ileP / 2) / ileP;
  return z;
}

std::string opisZnacznikow(const std::string& t) {
  // strona „Wszystkie porady” bez liczb dzisiejszej nocy (porada mogła dotyczyć innej nocy albo jeszcze nie stała)
  static const struct { const char* id; const char* opis; } O[] = {
      {"czas", "[czas z maską tej nocy]"},  {"o_ile_dluzej", "[ile]"},           {"noce_z_rzedu", "[liczba]"},
      {"wyciek", "[wyciek tej nocy]"},       {"noce_z_maska", "[liczba]"},       {"noce_4h", "[liczba]"},
      {"srednio", "[średni czas z maską]"},  {"noce_z_maska_prog", "[liczba]"}, {"srednio_prog", "[średni czas z maską]"}};
  std::string w;
  size_t p = 0;
  for (;;) {
    size_t a = t.find('{', p);
    if (a == std::string::npos) break;
    size_t b = t.find('}', a);
    if (b == std::string::npos) break;
    std::string id = t.substr(a + 1, b - a - 1), v = t.substr(a, b - a + 1);
    for (auto& x : O)
      if (id == x.id) v = x.opis;
    w += t.substr(p, a - p) + v;
    p = b + 1;
  }
  return w + t.substr(p);
}

std::string podstawZnaczniki(const std::string& t, const Znaczniki& z) {
  auto czas = [](int m) -> std::string { return m < 0 ? "–" : m < 60 ? std::to_string(m) + " min" : godzH(m); };
  auto licz = [](int v) -> std::string { return v < 0 ? "–" : std::to_string(v); };
  std::string w;
  size_t p = 0;
  for (;;) {
    size_t a = t.find('{', p);
    if (a == std::string::npos) break;
    size_t b = t.find('}', a);
    if (b == std::string::npos) break;
    std::string id = t.substr(a + 1, b - a - 1), v;
    bool zn = true;
    if (id == "czas") v = godzH(z.czas);
    else if (id == "o_ile_dluzej") v = czas(z.oIleDluzej);
    else if (id == "noce_z_rzedu") v = licz(z.nocyZRzedu);
    else if (id == "wyciek") v = licz(z.wyciek);
    else if (id == "noce_z_maska") v = licz(z.nocyZMaska);
    else if (id == "noce_4h") v = licz(z.nocy4h);
    else if (id == "srednio") v = godzH(z.srednio);
    else if (id == "noce_z_maska_prog") v = licz(z.nocyZMaskaProg);
    else if (id == "srednio_prog") v = godzH(z.srednioProg);
    else zn = false;
    w += t.substr(p, a - p) + (zn ? v : t.substr(a, b - a + 1));
    p = b + 1;
  }
  return w + t.substr(p);
}

}  // namespace dobry_start
