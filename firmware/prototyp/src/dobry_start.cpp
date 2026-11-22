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

const char* const POLA_PORAD =
    "id;rodzaj;dzien;tryb;wymaga;ekran;przy;zamiast;warunek;kol;kol_ankiety;flagi;wymaga_porady;stopien_wyzej;miejsce_bez;"
    "tytul;"
    "tresc;krok";
const char* const POLA_ANKIETY = "id;typ;od_tygodnia;wylaczna;klucz;wartosci;pytanie;odpowiedzi";
const size_t N_POL_PORAD = 18, N_POL_ANKIETY = 8;

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
      {"podstawowa", FL_PODSTAWOWA}, {"koncowa", FL_KONCOWA}, {"objaw", FL_OBJAW}};
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
  ponadIle = k.ponadIle;
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
    else if (r == "ponad_program") x.rodzaj = Rodzaj::PONAD;
    else { pomin("nieznany rodzaj"); continue; }
    if (x.rodzaj == Rodzaj::PLAN) {
      x.dzien = liczba(f[2], DZIEN_MAKS);
      if (x.dzien < 1) { pomin("złe miejsce planu"); continue; }
    }
    x.tryby = maskaTrybu(f[3]);
    if (!x.tryby) { pomin("nieznany tryb"); continue; }
    if (f[4] == "nawilżacz") x.nawilzacz = 1;
    else if (f[4] == "bez nawilżacza") x.nawilzacz = -1;
    else if (!f[4].empty()) { pomin("nieznane wymaga"); continue; }
    if (f[5] == "karta") x.karta = true;
    else if (f[5] != "dane") { pomin("nieznany ekran"); continue; }
    x.przy = f[6];
    x.zamiast = f[7];
    for (auto& t : dziel(f[8], '|')) {
      if (t.empty()) continue;
      if (t[0] == '!') x.weta.push_back(t.substr(1));
      else x.warunek.push_back(t);
    }
    x.kol = f[9].empty() ? 0 : liczba(f[9], 999);
    x.kolAnkiety = f[10].empty() ? 0 : liczba(f[10], 999);
    if (x.kol < 0 || x.kolAnkiety < 0) { pomin("zła kolejność"); continue; }
    bool zlaFlaga = false;
    for (auto& fl : dziel(f[11], ',')) {
      if (fl.empty()) continue;
      uint16_t v = flagaZNazwy(fl);
      if (!v) zlaFlaga = true;
      x.flagi = (uint16_t)(x.flagi | v);
    }
    if (zlaFlaga) { pomin("nieznana flaga"); continue; }
    x.wymagaPorady = f[12];
    x.stopienWyzej = f[13];
    x.miejsceBez = f[14].empty() ? 0 : liczba(f[14], DZIEN_MAKS);
    if (x.miejsceBez < 0 || (x.miejsceBez > 0 && x.rodzaj == Rodzaj::PLAN)) { pomin("złe miejsce_bez"); continue; }
    if (f[15].empty()) { pomin("brak tytułu"); continue; }
    if (f[16].empty()) { pomin("brak treści"); continue; }
    if (f[15].size() > 0xFFFF || f[16].size() > 0xFFFF || f[17].size() > 0xFFFF) { pomin("za długi tekst"); continue; }
    uint32_t pole = od;
    for (size_t i = 0; i < 15; i++) pole += (uint32_t)f[i].size() + 1;
    x.tytul = {pole, (uint16_t)f[15].size()};
    pole += (uint32_t)f[15].size() + 1;
    x.tresc = {pole, (uint16_t)f[16].size()};
    pole += (uint32_t)f[16].size() + 1;
    x.krok = {pole, (uint16_t)f[17].size()};
    if (ids.count(id)) { pomin("powtórzone ID"); continue; }
    bool zajete = false;
    // `wszystkie` zajmuje miejsce we wszystkich trybach (para `wszystkie` + konkretny tryb to dwie porady na miejscu)
    auto bity = [](uint8_t t) -> uint8_t { return (t & TR_WSZYSTKIE) ? (uint8_t)(TR_CPAP | TR_AUTOSET | TR_ASV) : t; };
    if (x.rodzaj == Rodzaj::PLAN)
      for (auto& y : k.porady)
        if (y.rodzaj == Rodzaj::PLAN && y.dzien == x.dzien && (bity(y.tryby) & bity(x.tryby)) != 0 &&
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
  std::set<std::string> ponad;
  for (auto& x : k.porady) {
    if (x.rodzaj == Rodzaj::PLAN) k.miejsc = std::max(k.miejsc, x.dzien);
    if (x.rodzaj == Rodzaj::PONAD) ponad.insert(x.baza);
  }
  k.ponadIle = (int)ponad.size();
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
  } else if (kl == "start.ponad") {
    if (v == "tak") u.ponad = true;
    else if (v == "nie") u.ponad = false;
    else { u.ponad = true; blad = "start.ponad: zła wartość"; }
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
    case Zrodlo::PONAD: return "ponad";
    default: return "plan";
  }
}
static Zrodlo zrodloZNazwy(const std::string& n) {
  if (n == "dane") return Zrodlo::DANE;
  if (n == "ankieta") return Zrodlo::ANKIETA;
  if (n == "podstawowa") return Zrodlo::PODSTAWOWA;
  if (n == "tryb") return Zrodlo::TRYB;
  if (n == "ponad") return Zrodlo::PONAD;
  return Zrodlo::PLAN;
}

const Porada* wariant(const Katalog& k, const std::string& baza, Tryb t, int n) {
  for (auto& p : k.porady)
    if (p.baza == baza && trybPasuje(p, t) && nawilzaczPasuje(p, n)) return &p;
  return nullptr;
}
const Porada* wariantMiejsca(const Katalog& k, int miejsce, Tryb t, int n) {
  for (auto& p : k.porady)
    if (p.rodzaj == Rodzaj::PLAN && p.dzien == miejsce && trybPasuje(p, t) && nawilzaczPasuje(p, n)) return &p;
  if (n == 0)  // R-DS-29: bez nawilżacza na miejscu porady z nawilżaczem porada z kolumną `miejsce_bez`
    for (auto& p : k.porady)
      if (p.miejsceBez == miejsce && trybPasuje(p, t)) return &p;
  return nullptr;
}
int miejscePorady(const Porada& p, int n) {
  if (p.rodzaj == Rodzaj::PLAN) return p.dzien;
  return n == 0 ? p.miejsceBez : 0;
}
static std::vector<std::string> bazyPonadZ(const Katalog& k, const Stan& s, int n, const Tryb* t) {
  std::vector<std::string> b;
  for (auto& p : k.porady) {
    if (p.rodzaj != Rodzaj::PONAD || std::find(b.begin(), b.end(), p.baza) != b.end()) continue;
    if (t && !wariant(k, p.baza, *t, n)) continue;  // bez wariantu w tym trybie nie liczy się do `n z {m}`
    if (p.miejsceBez > 0) {  // stoi w planie bez nawilżacza albo już w nim stała (R-DS-29)
      bool wPlanie = false;
      for (auto& x : s.p)
        if (bazaId(x.id) == p.baza && x.zrodlo != "ponad") wPlanie = true;
      if (n == 0 && !wPlanie) {  // w planie tylko, gdy jej miejsce jeszcze czeka (fail-path R-DS-29: „niepokazana
        // wraca tam”): miejsce zajęte inną poradą (np. porada z nawilżaczem przed zmianą `start.nawilzacz`) albo
        // minięte w kolejce = porada idzie do „ponad program”
        bool zajete = s.kolejka > p.miejsceBez;
        for (auto& z : s.zaliczone) zajete = zajete || z.first == p.miejsceBez;
        for (auto& x : s.p) {
          const Porada* q = k.porada(x.id);
          zajete = zajete || (q && q->rodzaj == Rodzaj::PLAN && q->dzien == p.miejsceBez);
        }
        wPlanie = !zajete;
      }
      if (wPlanie) continue;
    }
    b.push_back(p.baza);
  }
  return b;
}
std::vector<std::string> bazyPonad(const Katalog& k, const Stan& s, int n) { return bazyPonadZ(k, s, n, nullptr); }
std::vector<std::string> bazyPonad(const Katalog& k, const Stan& s, int n, Tryb t) { return bazyPonadZ(k, s, n, &t); }

int doba(const Stan& s, const std::string& dzis) {
  if (!dataOk(s.start) || !dataOk(dzis)) return 0;
  return dniMiedzy(s.start, dzis) + 1;
}

// ---------- stan ----------
Stan nowyStan(const Ustawienia& u, const Katalog& k) {
  Stan s;
  s.kolejka = std::max(1, std::min(u.dzien, std::max(1, k.miejsc)));
  s.dzienUstawiony = u.dzien;
  s.zestaw = k.skrot;
  s.odNowa = "?";  // R-DS-33: nowy program od najbliższej doby (datę „od nowa” dopisuje wywołujący, gdy ją zna)
  return s;
}

Stan stanZWierszy(const std::vector<std::string>& wiersze, const Katalog& k) {
  Stan s;
  bool kolejka = false;
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
      else if (kl == "kolejka") {  // zła wartość: jak bez wiersza (kolejka z dziennika, R-DS-43)
        int x = liczba(v, DZIEN_MAKS + 1);
        if (x >= 1) {
          s.kolejka = x;
          kolejka = true;
        } else {
          zle();
        }
      }
      else if (kl == "od_nowa") s.odNowa = v;
      else if (kl == "stan") {
        if (v == "aktywny" || v == "przerwa" || v == "zatrzymany" || v == "zakonczony") s.stan = v;
        else zle();
      }
      else if (kl == "przerwa_dni") n(s.przerwaDni, 9999);
      else if (kl == "d0_z_rzedu") n(s.d0ZRzedu, 9999);
      else if (kl == "ponad") n(s.ponad, 999);
      else if (kl == "noce_terapii") n(s.noceTerapii, 9999);
      else if (kl == "tryb") s.tryb = v;
      else if (kl == "zestaw") s.zestaw = v;
      else if (kl == "ankieta_od") { if (dataOk(v)) s.ankietaOd = v; else zle(); }
      else if (kl == "doba") { if (dataOk(v)) s.doba = v; else zle(); }
      else if (kl == "dzien_ust") n(s.dzienUstawiony, DZIEN_MAKS);
      else if (kl == "plan_koniec") s.planKoniec = v == "1";
      else zle();
    } else if (r == "D" && f.size() >= 7 && dataOk(f[1]) && k.porada(f[2])) {
      s.wDzis = f[1];
      s.wId = f[2];
      s.wZrodlo = f[3];
      s.wRegula = f[4];
      s.wWieczor = f[5] == "1";
      s.wBezWpisu = f[6] == "1";
    } else if (r == "P" && f.size() >= 7 && dataOk(f[1]) && !f[2].empty()) {
      s.p.push_back({f[1], f[2], f[3], f[4], f[5], f[6]});
    } else if (r == "Z" && f.size() >= 4 && dataOk(f[1]) && !f[2].empty()) {
      s.z.push_back({f[1], f[2], f[3]});
    } else if (r == "O" && f.size() >= 5 && dataOk(f[1])) {
      s.o.push_back({f[1], f[2], f[3], f[4]});
    } else if (r == "K" && f.size() >= 3 && liczba(f[1], DZIEN_MAKS) >= 1) {
      s.zaliczone.push_back({liczba(f[1], DZIEN_MAKS), f[2]});
    } else if (r == "W" && f.size() >= 2 && dataOk(f[1])) {
      s.wyciek.push_back(f[1]);
    } else if (r == "T" && f.size() >= 2 && !f[1].empty()) {
      s.trybPoZmianie.push_back(f[1]);
    } else {
      zle();
    }
  }
  if (!kolejka && !s.start.empty()) {  // R-DS-43: kolejka z dziennika (ostatnie miejsce planu + 1)
    int m = 0;
    for (auto& x : s.p) {
      const Porada* p = k.porada(x.id);
      if (p && p->rodzaj == Rodzaj::PLAN && x.zrodlo == "plan") m = std::max(m, p->dzien);
    }
    s.kolejka = m + 1;
    s.bledy.push_back("dobry_start: brak kolejki, odtworzona z dziennika: " + std::to_string(s.kolejka));
  }
  return s;
}

std::vector<std::string> wierszeStanu(const Stan& s) {
  std::vector<std::string> w;
  auto S = [&](const char* kl, const std::string& v) { w.push_back(std::string("S;") + kl + ";" + v); };
  S("start", s.start);
  S("kolejka", std::to_string(s.kolejka));
  S("stan", s.stan);
  S("przerwa_dni", std::to_string(s.przerwaDni));
  S("d0_z_rzedu", std::to_string(s.d0ZRzedu));
  S("tryb", s.tryb);
  S("ponad", std::to_string(s.ponad));
  S("zestaw", s.zestaw);
  S("noce_terapii", std::to_string(s.noceTerapii));
  if (!s.ankietaOd.empty()) S("ankieta_od", s.ankietaOd);
  if (!s.doba.empty()) S("doba", s.doba);
  S("dzien_ust", std::to_string(s.dzienUstawiony));
  S("plan_koniec", s.planKoniec ? "1" : "0");
  if (!s.odNowa.empty()) S("od_nowa", s.odNowa);
  if (!s.wDzis.empty())
    w.push_back("D;" + s.wDzis + ";" + s.wId + ";" + s.wZrodlo + ";" + s.wRegula + ";" + (s.wWieczor ? "1" : "0") +
                ";" + (s.wBezWpisu ? "1" : "0"));
  for (auto& x : s.p) w.push_back("P;" + x.data + ";" + x.id + ";" + x.zrodlo + ";" + x.regula + ";" + x.pora + ";" + x.gdzie);
  for (auto& x : s.z) w.push_back("Z;" + x.data + ";" + x.id + ";" + x.regula);
  for (auto& x : s.o) w.push_back("O;" + x.data + ";" + x.tydzien + ";" + x.pytanie + ";" + x.kody);
  for (auto& x : s.zaliczone) w.push_back("K;" + std::to_string(x.first) + ";" + x.second);
  for (auto& x : s.wyciek) w.push_back("W;" + x);
  for (auto& x : s.trybPoZmianie) w.push_back("T;" + x);
  return w;
}

// ---------- pomocnicze wyboru ----------
namespace {

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
  bool miejscePokazane(int m) const {
    for (auto& z : s.zaliczone)
      if (z.first == m) return true;
    for (auto& x : s.p) {
      const Porada* p = k.porada(x.id);
      if (p && ((p->rodzaj == Rodzaj::PLAN && p->dzien == m) || (p->miejsceBez == m && x.zrodlo != "ponad")))
        return true;
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
  int limit(const std::string& dzis) const { return doba(s, dzis) <= LIMIT_GRANICA ? LIMIT_POCZATEK : LIMIT_POZNIEJ; }
};

struct Kandydat {
  const Porada* p = nullptr;
  std::string regula, dataNocy;
  int kol = 0;
  bool dzisiejszy = true;
};

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

void dodajZalegla(Stan& s, const Kandydat& c, const std::string& dzis) {
  for (auto& z : s.z)
    if (bazaId(z.id) == c.p->baza) return;
  s.z.push_back({c.dzisiejszy ? dzis : c.dataNocy, c.p->id, c.regula});
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
    log.push_back("dobry start: przerwa w terapii skończona, plan od miejsca " + std::to_string(s.kolejka));
  }
  if (przerwa && s.stan == "aktywny") {
    s.stan = "przerwa";
    log.push_back("dobry start: przerwa w terapii, plan wstrzymany");
  }
  if (s.stan == "przerwa") s.przerwaDni++;
  // R-DS-06: zaległe najwyżej 2 dni
  s.z.erase(std::remove_if(s.z.begin(), s.z.end(),
                           [&](const WpisZ& z) { return dniMiedzy(z.data, d.dzis) > ZALEGLA_DNI || !k.porada(z.id); }),
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
      const Porada* n = wariantMiejsca(k, p->dzien, d.tryb, u.nawilzacz);
      if (n && n->id != p->id &&
          std::find(s.trybPoZmianie.begin(), s.trybPoZmianie.end(), n->id) == s.trybPoZmianie.end())
        s.trybPoZmianie.push_back(n->id);
    }
    log.push_back("dobry start: tryb " + s.tryb + " → " + t + ", porady w nowym wariancie: " +
                  std::to_string(s.trybPoZmianie.size()));
  }
  if (!t.empty()) s.tryb = t;
  // R-DS-34 (fail-path): `start.dzien` zmieniony w trakcie: nowa kolejka od tej doby
  if (!pierwsza && u.dzien != s.dzienUstawiony) {
    s.dzienUstawiony = u.dzien;
    s.kolejka = std::max(1, std::min(u.dzien, std::max(1, k.miejsc)));
    log.push_back("dobry start: start.dzien zmieniony, kolejka od " + std::to_string(s.kolejka));
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
// tryb doby do etykiety „ponad program” przy wyborze z dziennika (dyrygent nie podaje wtedy trybu nocy)
static Tryb trybDnia(const Stan& s, const Doba& d) {
  if (d.tryb != Tryb::NIEZNANY) return d.tryb;
  if (!s.tryb.empty()) return trybZNazwy(s.tryb);
  return d.bezNocy ? Tryb::CPAP : Tryb::NIEZNANY;
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
      w.log.push_back("dobry start: program wyłączony na miejscu " + std::to_string(s.kolejka));
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
    s.odNowa.clear();
    s.start = d.dzis;
    s.tryb = nazwaTrybu(d.tryb);
    w.log.push_back("dobry start: włączony, dzień włączenia " + d.dzis + ", kolejka " + std::to_string(s.kolejka));
  }
  if (s.start > d.dzis) return w;  // po „od nowa”: program rusza od dnia włączenia
  if (s.stan == "zatrzymany") {
    // R-DS-12: doby z wyłączonym programem nie liczą się do terminu (jak przerwa w terapii, R-DS-11)
    int wyl = dataOk(s.doba) && s.doba < d.dzis ? dniMiedzy(s.doba, d.dzis) - 1 : 0;
    if (wyl > 0) s.przerwaDni += wyl;
    s.stan = "aktywny";
    w.log.push_back("dobry start: włączony ponownie, dalej od miejsca " + std::to_string(s.kolejka));
  }
  if (s.stan == "zakonczony") return w;
  for (auto& x : s.p)
    if (x.data > d.dzis) {  // R-DS-42: zegar cofnięty
      w.log.push_back("dobry start: data poranka przed ostatnim wpisem, bez nowej porady");
      return w;
    }
  // R-DS-43: porada dnia z dziennika
  for (auto& x : s.p)
    if (x.data == d.dzis) {
      w.porada = k.porada(x.id);
      w.zrodlo = zrodloZNazwy(x.zrodlo);
      w.regula = x.regula;
      w.dataNocy = d.dzis;
      w.narysowana = true;
      w.telefon = x.gdzie == "telefon";
      w.ponadIle = (int)bazyPonad(k, s, u.nawilzacz, trybDnia(s, d)).size();
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
    w.ponadIle = (int)bazyPonad(k, s, u.nawilzacz, trybDnia(s, d)).size();
    return w;
  }
  {  // R-DS-12, fail-path: panel bez poranków (wyłączony) dłużej niż do terminu programu → koniec, bez porad
    int termin = KONIEC_DOBA + s.przerwaDni;
    if (!s.planKoniec && dataOk(s.doba) && s.doba < d.dzis && doba(s, s.doba) < termin && doba(s, d.dzis) > termin) {
      s.stan = "zakonczony";
      s.doba = d.dzis;
      s.wDzis.clear();
      w.log.push_back("dobry start: pierwszy poranek po terminie programu (doba " + std::to_string(doba(s, d.dzis)) +
                      "), program zakończony (R-DS-12)");
      return w;
    }
  }
  nowaDoba(k, s, u, d, w.log);
  Liczniki L{k, s};
  int nawilz = u.nawilzacz;
  Tryb tryb = d.tryb;
  if (tryb == Tryb::NIEZNANY && d.bezNocy) tryb = s.tryb.empty() ? Tryb::CPAP : trybZNazwy(s.tryb);  // R-DS-44
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
                      (w.regula.empty() ? "" : ", " + w.regula) + (w.tylkoWieczor ? ", tylko wieczorem" : "") + ")");
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
  if (d.w0) {  // R-DS-04: W0 trzyma ekran i wieczorem; porady wyzwalane czekają jako zaległe
    for (auto& p : k.porady) {
      std::string r;
      if (p.rodzaj == Rodzaj::ZDARZENIE && aktywnyDane(p, d.tokeny, r) && wariant(k, p.baza, tryb, nawilz)) {
        Kandydat c;
        c.p = wariant(k, p.baza, tryb, nawilz);
        c.regula = r;
        dodajZalegla(s, c, d.dzis);
      }
    }
    w.log.push_back("dobry start: ekran z rana trzyma także wieczór (W0), bez porady");
    return w;
  }
  int db = doba(s, d.dzis);
  // R-DS-12: ostatnia doba programu: ostatnia porada planu przed poradą wyzwalaną
  if (!s.planKoniec && db >= KONIEC_DOBA + s.przerwaDni)
    for (int m = k.miejsc; m >= 1; m--) {
      const Porada* p = wariantMiejsca(k, m, tryb, nawilz);
      if (p && p->ma(FL_KONCOWA)) {
        ustaw(p, Zrodlo::PLAN, "", "");
        w.log.push_back("dobry start: doba " + std::to_string(db) + ", koniec planu (R-DS-12)");
        return koniec();
      }
    }
  // porady z danych ostatniej nocy i zaległe (R-DS-05, R-DS-06)
  std::vector<Kandydat> kand;
  auto dodaj = [&](const Kandydat& c) {
    for (auto& x : kand)
      if (x.p->baza == c.p->baza) return;
    kand.push_back(c);
  };
  for (auto& p : k.porady) {
    std::string r;
    if (p.rodzaj != Rodzaj::ZDARZENIE || !aktywnyDane(p, d.tokeny, r)) continue;
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
    const Porada* v = p ? wariant(k, p->baza, tryb, nawilz) : nullptr;
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
  bool grupaOk = L.wyzwalaneWOknie(d.dzis) < L.limit(d.dzis);
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
    if (obj && obj->rodzaj == Rodzaj::PLAN && !L.miejscePokazane(obj->dzien)) {
      const Porada* ov = wariantMiejsca(k, obj->dzien, tryb, nawilz);
      if (ov) {
        Kandydat zc = c;
        zc.p = p;
        dodajZalegla(s, zc, d.dzis);
        for (size_t j = 0; j < kand.size(); j++)
          if (j != i && kand[j].dzisiejszy) dodajZalegla(s, kand[j], d.dzis);
        ustaw(ov, Zrodlo::PLAN, "", "");
        w.log.push_back("dobry start: " + p->id + " czeka na objaśnienie " + ov->id + " (R-DS-49)");
        return koniec();
      }
    }
    for (size_t j = 0; j < kand.size(); j++)
      if (j != i && kand[j].dzisiejszy) dodajZalegla(s, kand[j], d.dzis);
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
      ustaw(p, Zrodlo::ANKIETA, c.regula, "");
      return koniec();
    }
  }
  // dzień z D0: porada planu dopiero od D0_DOBY. kolejnej doby (R-DS-04)
  if (d.d0 && s.d0ZRzedu < D0_DOBY) {
    w.log.push_back("dobry start: ekran z rana trzyma dzień, porada planu czeka (D0 " + std::to_string(s.d0ZRzedu) + ")");
    return w;
  }
  // R-DS-34: porady podstawowe sprzed miejsca startu
  if (s.dzienUstawiony > 1) {
    const Porada* best = nullptr;
    for (auto& p : k.porady)
      if (p.ma(FL_PODSTAWOWA) && p.dzien < s.dzienUstawiony && !L.miejscePokazane(p.dzien)) {
        const Porada* v = wariantMiejsca(k, p.dzien, tryb, nawilz);
        if (v && v->ma(FL_PODSTAWOWA) && (!best || v->kol < best->kol)) best = v;
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
  // plan (R-DS-10, R-DS-37, R-DS-38): miejsca zaliczone i bez wariantu przesuwają kolejkę bez rysowania
  if (!s.planKoniec) {
    while (s.kolejka <= k.miejsc) {
      bool zal = false;
      for (auto& z : s.zaliczone)
        if (z.first == s.kolejka) zal = true;
      const Porada* p = zal ? nullptr : wariantMiejsca(k, s.kolejka, tryb, nawilz);
      if (p) {
        ustaw(p, Zrodlo::PLAN, "", "");
        return koniec();
      }
      w.log.push_back("dobry start: miejsce " + std::to_string(s.kolejka) + (zal ? " zaliczone" : " bez wariantu") +
                      ", kolejka dalej");
      s.kolejka++;
    }
    s.planKoniec = true;
  }
  // „ponad program” (R-DS-14)
  if (u.ponad) {
    std::vector<std::string> bazy = bazyPonad(k, s, nawilz, tryb);
    w.ponadIle = (int)bazy.size();
    for (auto& b : bazy) {
      bool juz = false;
      for (auto& x : s.p)
        if (bazaId(x.id) == b && x.zrodlo == "ponad") juz = true;
      const Porada* p = juz ? nullptr : wariant(k, b, tryb, nawilz);
      if (p) {
        ustaw(p, Zrodlo::PONAD, "", "");
        return koniec();
      }
    }
  }
  s.stan = "zakonczony";
  w.log.push_back("dobry start: program zakończony");
  return w;
}

void zapiszPokazanie(const Katalog& k, Stan& s, const WyborPorady& w, const std::string& dzis, bool wieczor, bool telefon) {
  if (!w.porada || w.bezWpisu || w.narysowana || !dataOk(dzis)) return;
  for (auto& x : s.p)
    if (x.data == dzis) return;
  const Porada& p = *w.porada;
  s.p.push_back({dzis, p.id, nazwaZrodla(w.zrodlo), w.regula, wieczor ? "wieczor" : "dzien", telefon ? "telefon" : "panel"});
  s.z.erase(std::remove_if(s.z.begin(), s.z.end(), [&](const WpisZ& z) { return bazaId(z.id) == p.baza; }), s.z.end());
  auto zalicz = [&](int m) {
    for (auto& z : s.zaliczone)
      if (z.first == m) return;
    s.zaliczone.push_back({m, dzis});
  };
  int m = p.rodzaj == Rodzaj::PLAN ? p.dzien : (w.zrodlo == Zrodlo::PLAN ? p.miejsceBez : 0);
  if (m > 0) {
    if (w.zrodlo == Zrodlo::PLAN && m == s.kolejka) s.kolejka = m + 1;
    else if (w.zrodlo != Zrodlo::TRYB) zalicz(m);
    if (p.ma(FL_KONCOWA)) s.planKoniec = true;
  }
  if ((w.zrodlo == Zrodlo::DANE || w.zrodlo == Zrodlo::ANKIETA) && !p.zamiast.empty()) {  // R-DS-09
    const Porada* z = k.porada(p.zamiast);
    if (z && z->rodzaj == Rodzaj::PLAN && z->dzien >= s.kolejka) zalicz(z->dzien);
  }
  if (w.zrodlo == Zrodlo::PONAD) s.ponad++;
  if (w.zrodlo == Zrodlo::TRYB)
    s.trybPoZmianie.erase(std::remove(s.trybPoZmianie.begin(), s.trybPoZmianie.end(), p.id), s.trybPoZmianie.end());
}

// ---------- teksty ----------
OpisPorady opis(const Katalog& k, const Stan& s, const WyborPorady& w) {
  OpisPorady o;
  if (!w.porada) return o;
  const Porada& p = *w.porada;
  o.tygodni = k.tygodni();
  std::string m = std::to_string(k.miejsc);
  switch (w.zrodlo) {
    case Zrodlo::DANE:
      o.srodekBelki = "po nocy " + ddmm(w.dataNocy);
      o.etykieta = "porada do ostatniej nocy";
      break;
    case Zrodlo::ANKIETA:
      o.srodekBelki = "z twojej ankiety";
      o.etykieta = "porada do ankiety";
      break;
    case Zrodlo::PONAD: {
      int n = w.narysowana ? s.ponad : s.ponad + 1;
      o.srodekBelki = "ponad program";
      o.etykieta = "ponad program · " + std::to_string(n) + " z " +
                   std::to_string(w.ponadIle > 0 ? w.ponadIle : k.ponadIle);
      break;
    }
    default: {
      int mp = p.rodzaj == Rodzaj::PLAN ? p.dzien : p.miejsceBez;
      o.srodekBelki = "dzień " + std::to_string(mp) + " z " + m;
      o.tydzien = (mp - 1) / 7 + 1;
      o.wTygodniu = (mp - 1) % 7 + 1;
      o.etykieta = "tydzień " + std::to_string(o.tydzien) + " z " + std::to_string(o.tygodni);
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
  return o;
}

std::string znacznik(const Katalog& k, const Stan& s, const Porada& p) {
  std::string d, gdzie;
  for (auto& x : s.p)
    if (x.id == p.id && x.data >= d) {
      d = x.data;
      gdzie = x.gdzie;
    }
  if (d.empty() && p.rodzaj == Rodzaj::PLAN)
    for (auto& z : s.zaliczone) {
      bool inny = false;  // miejsce zaliczone, ale pokazane w innym wariancie: pokazana z datą
      for (auto& x : s.p)
        if (x.id == p.id) inny = true;
      if (z.first == p.dzien && !inny) d = z.second;
    }
  (void)k;
  if (d.empty()) return "jeszcze nie";
  return (gdzie == "telefon" ? "na telefonie " : "pokazana ") + ddmm(d);
}

std::string naglowekSekcji(const Katalog& k, const Stan& s, const Ustawienia& u, const std::string& dzis) {
  if (!u.program) return "wyłączony";
  std::string w;
  if (s.stan == "zakonczony") return "zakończony";
  if (s.stan == "przerwa") w = "przerwa w terapii";
  else if (s.planKoniec) w = "ponad program " + std::to_string(s.ponad) + " z " + std::to_string(k.ponadIle);
  else {
    int n = s.start.empty() ? u.dzien : s.kolejka;
    n = std::max(1, std::min(n, std::max(1, k.miejsc)));
    w = "dzień " + std::to_string(n) + " z " + std::to_string(k.miejsc);
  }
  if (ankietaCzeka(s, dzis)) w += " · ankieta czeka";
  return w;
}

}  // namespace dobry_start
