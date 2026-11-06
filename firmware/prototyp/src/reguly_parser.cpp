// SPDX-License-Identifier: GPL-3.0-or-later
// Parser zestawu danych reguł (spec techniczna 9.3: nagłówek, pliki progów, katalogu i tekstów; T-WYM-64)
// i zestaw wbudowany (src/zestaw_wbudowany.h, generowany przez tools/zestaw/buduj_zestaw.py).
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "magazyn.h"  // sha256Hex()
#include "reguly.h"
#include "reguly_wewn.h"
#include "zestaw_wbudowany.h"

namespace reguly {
using namespace wewn;

namespace {

// ---------- parser zestawu ----------
struct Naglowek {
  std::string zestaw, pochodzenie, firmware;
};

bool czytajPlik(const std::string& tresc, const char* rodzaj, Naglowek& n, std::vector<std::string>& wiersze,
                std::string& powod) {
  std::string p = std::string(rodzaj) + ": ";
  size_t nl = tresc.find('\n');
  if (nl == std::string::npos) {
    powod = p + "brak naglowka";
    return false;
  }
  std::string pierwszy = przytnij(tresc.substr(0, nl));
  std::string cialo = tresc.substr(nl + 1);
  std::vector<std::string> t = dziel(pierwszy, ' ');
  if (t.size() < 3 || t[0] != "#cpap-eink") {
    powod = p + "zly naglowek";
    return false;
  }
  if (t[1] != rodzaj) {
    powod = p + "naglowek innego pliku (" + t[1] + ")";
    return false;
  }
  if (t[2] != "1") {
    powod = p + "nieznana wersja formatu " + t[2];
    return false;
  }
  std::map<std::string, std::string> pola;
  for (size_t i = 3; i < t.size(); i++) {
    size_t r = t[i].find('=');
    if (r == std::string::npos || r == 0) {
      powod = p + "zle pole naglowka " + t[i];
      return false;
    }
    pola[t[i].substr(0, r)] = t[i].substr(r + 1);
  }
  for (const char* k : {"zestaw", "pochodzenie", "firmware", "tryby", "zamrozenie", "zrodlo", "skrot"})
    if (!pola.count(k) || pola[k].empty()) {
      powod = p + "brak pola " + k + " w naglowku";
      return false;
    }
  if (pola["pochodzenie"] != "wbudowany" && pola["pochodzenie"] != "karta") {
    powod = p + "zle pochodzenie " + pola["pochodzenie"];
    return false;
  }
  if (pola["firmware"].find("..") == std::string::npos) {
    powod = p + "zly zakres firmware";
    return false;
  }
  const std::string& s = pola["skrot"];
  if (s.compare(0, 7, "sha256:") != 0 || s.substr(7) != sha256Hex(cialo)) {
    powod = p + "skrot niezgodny";
    return false;
  }
  n.zestaw = pola["zestaw"];
  n.pochodzenie = pola["pochodzenie"];
  n.firmware = pola["firmware"];
  wiersze.clear();
  for (auto& w : dziel(cialo, '\n')) {
    std::string x = w;
    if (!x.empty() && x.back() == '\r') x.pop_back();
    if (!x.empty()) wiersze.push_back(x);
  }
  if (wiersze.empty()) {
    powod = p + "brak wiersza nazw pol";
    return false;
  }
  return true;
}

// Parametry rdzenia, których wartość jest id_progu (kontrakt 6 i dopisany prog_nocy); parametry rodzajów paczek
// (docs/spec/katalog/rodzaje-*.csv) sprawdza skrypt budowy, a brak progu w nich daje „nie oceniam” (progParametru()).
bool parametrProgowy(const std::string& n) {
  static const char* const P[] = {"prog", "kzn",     "n",      "delta",   "razy",     "min",   "max",
                                  "okno_a", "okno_b", "prog_nocy", "prog_udzial"};
  for (const char* x : P)
    if (n == x) return true;
  return false;
}

bool liczoneNoceZnane(const std::string& s) {
  static const char* const L[] = {"",       "kompletne",           "z_rekordem", "z_terapia",
                                  "z_terapia_bez_niepewnych", "wazne", "wazne_bez_niepewnych", "wszystkie",
                                  "z_terapia_spo2"};
  for (const char* x : L)
    if (s == x) return true;
  return false;
}

bool takNie(const std::string& s, bool& v) {
  if (s == "tak") v = true;
  else if (s == "nie" || s.empty()) v = false;
  else return false;
  return true;
}

}  // namespace

// ---------- kontrakt ----------
Tryb trybZUstawien(const std::string& u) {
  std::string t = male(przytnij(u.substr(0, u.find('/'))));
  if (t == "cpap") return Tryb::CPAP;
  if (t == "autoset") return Tryb::AUTOSET;
  if (t == "asv") return Tryb::ASV;
  if (t == "asvauto") return Tryb::ASVAUTO;
  return Tryb::NIEZNANY;
}

const Prog* Zestaw::prog(const std::string& id, Tryb tryb) const {
  if (tryb != Tryb::NIEZNANY) {
    auto t = progi.find(id + KLUCZ_TRYBU + nazwaTrybu(tryb));
    if (t != progi.end()) return &t->second;
  }
  auto it = progi.find(id);
  if (it == progi.end()) return nullptr;
  const std::string& t = przytnij(it->second.tryb);
  if (t.empty() || t == "wszystkie" || trybNaLiscie(t, tryb)) return &it->second;
  return nullptr;
}

bool wczytajZestaw(const std::string& progi, const std::string& katalog, const std::string& teksty, Zestaw& wyj,
                   std::string& powod) {
  Naglowek np, nk, nt;
  std::vector<std::string> wp, wk, wt;
  if (!czytajPlik(progi, "progi", np, wp, powod) || !czytajPlik(katalog, "katalog", nk, wk, powod) ||
      !czytajPlik(teksty, "teksty", nt, wt, powod))
    return false;
  if (np.zestaw != nk.zestaw || np.zestaw != nt.zestaw) {
    powod = "rozne identyfikatory zestawu w plikach";
    return false;
  }
  if (np.pochodzenie != nk.pochodzenie || np.pochodzenie != nt.pochodzenie || np.firmware != nk.firmware ||
      np.firmware != nt.firmware) {
    powod = "rozne pochodzenie albo zakres firmware w plikach zestawu";
    return false;
  }
  Zestaw z;
  z.identyfikator = np.zestaw;
  // progi
  if (wp[0] != "id_progu;wartosc;jednostka;tryb") {
    powod = "progi: zle nazwy pol";
    return false;
  }
  for (size_t i = 1; i < wp.size(); i++) {
    std::vector<std::string> f = dziel(wp[i], ';');
    if (f.size() != 4 || f[0].empty()) {
      powod = "progi: wiersz " + std::to_string(i + 2) + " nieczytelny";
      return false;
    }
    Prog p;
    p.id = f[0];
    p.jednostka = f[2];
    p.tryb = f[3];
    double v = 0;
    if (liczbaZ(f[1], v)) p.wartosc = v;
    else p.tekst = f[1];
    // kilka wierszy jednego id_progu z różnym `tryb`. Wiersz `wszystkie` stoi pod
    // kluczem `id`; wiersz z listą trybów pod kluczem `id` + KLUCZ_TRYBU + tryb (każdy tryb z listy) i pod `id`, dopóki
    // nie przyjdzie wiersz `wszystkie` (żeby istnienie progu sprawdzało się po samym id).
    std::string tr = przytnij(p.tryb);
    if (tr.empty() || tr == "wszystkie") {
      auto it = z.progi.find(p.id);
      if (it != z.progi.end() && (przytnij(it->second.tryb).empty() || przytnij(it->second.tryb) == "wszystkie")) {
        powod = "progi: powtorzony " + p.id;
        return false;
      }
      z.progi[p.id] = p;
    } else {
      for (auto& t : lista(tr)) {
        Tryb tt = trybZUstawien(t);
        std::string kl = p.id + KLUCZ_TRYBU + nazwaTrybu(tt);
        if (tt == Tryb::NIEZNANY || z.progi.count(kl)) {
          powod = "progi: " + p.id + ": nieznany albo powtorzony tryb " + t;
          return false;
        }
        z.progi[kl] = p;
      }
      if (!z.progi.count(p.id)) z.progi[p.id] = p;
    }
  }
  // teksty
  if (wt[0] != "id_tekstu;rola;limit;tekst") {
    powod = "teksty: zle nazwy pol";
    return false;
  }
  for (size_t i = 1; i < wt.size(); i++) {
    const std::string& l = wt[i];
    size_t a = l.find(';'), b = a == std::string::npos ? a : l.find(';', a + 1),
           c = b == std::string::npos ? b : l.find(';', b + 1);
    double lim = 0;
    if (c == std::string::npos || a == 0 || !liczbaZ(l.substr(b + 1, c - b - 1), lim) || lim < 0) {
      powod = "teksty: wiersz " + std::to_string(i + 2) + " nieczytelny";
      return false;
    }
    Tekst t;
    t.id = l.substr(0, a);
    t.rola = l.substr(a + 1, b - a - 1);
    t.limit = (int)std::lround(lim);
    t.tekst = l.substr(c + 1);
    if (z.teksty.count(t.id)) {
      powod = "teksty: powtorzony " + t.id;
      return false;
    }
    z.teksty[t.id] = t;
  }
  // katalog
  std::vector<std::string> nazwy = dziel(wk[0], ';');
  std::map<std::string, size_t> kol;
  for (size_t i = 0; i < nazwy.size(); i++) kol[nazwy[i]] = i;
  static const char* const WYM[] = {"id_reguly", "wariant", "rodzaj", "parametry", "liczone_noce", "okno",
                                    "poziom",    "grupa",   "slot",   "tryby",     "wymaga_normy", "w_rozruchu",
                                    "wyklucza",  "wymaga",  "bramki", "limit",     "teksty"};
  for (const char* n : WYM)
    if (!kol.count(n)) {
      powod = std::string("katalog: brak pola ") + n;
      return false;
    }
  for (size_t i = 1; i < wk.size(); i++) {
    std::vector<std::string> f = dziel(wk[i], ';');
    std::string gdzie = "katalog: wiersz " + std::to_string(i + 2);
    if (f.size() != nazwy.size()) {
      powod = gdzie + ": " + std::to_string(f.size()) + " pol zamiast " + std::to_string(nazwy.size());
      return false;
    }
    auto pole = [&](const char* n) -> std::string { return kol.count(n) ? f[kol[n]] : std::string(); };
    WierszKatalogu w;
    w.id = pole("id_reguly");
    w.wariant = pole("wariant");
    w.rodzaj = pole("rodzaj");
    gdzie += " (" + jakoKlucz(w) + ")";
    if (w.id.empty() || w.rodzaj.empty()) {
      powod = gdzie + ": puste id albo rodzaj";
      return false;
    }
    for (auto& x : lista(pole("parametry"))) {
      size_t r = x.find('=');
      if (r == std::string::npos || r == 0) {
        powod = gdzie;
        powod += ": parametr bez = (";
        powod += x;
        powod += ")";
        return false;
      }
      std::string nz = przytnij(x.substr(0, r)), v = przytnij(x.substr(r + 1));
      if (parametrProgowy(nz) && !z.progi.count(v)) {
        powod = gdzie;
        powod += ": brak progu ";
        powod += v;
        return false;
      }
      w.parametry[nz] = v;
    }
    w.liczoneNoce = pole("liczone_noce");
    if (!liczoneNoceZnane(w.liczoneNoce)) {
      powod = gdzie + ": nieznane liczone_noce " + w.liczoneNoce;
      return false;
    }
    w.okno = pole("okno");
    if (!w.okno.empty() && !z.progi.count(w.okno)) {
      powod = gdzie + ": brak progu okna " + w.okno;
      return false;
    }
    w.poziom = pole("poziom");
    w.grupa = pole("grupa");
    w.slot = pole("slot");
    w.tryby = pole("tryby");
    if (!takNie(pole("wymaga_normy"), w.wymagaNormy) || !takNie(pole("w_rozruchu"), w.wRozruchu)) {
      powod = gdzie + ": wymaga_normy albo w_rozruchu inne niz tak/nie";
      return false;
    }
    w.wyklucza = lista(pole("wyklucza"));
    w.wymaga = lista(pole("wymaga"));
    w.bramki = pole("bramki");
    w.limit = pole("limit");
    w.teksty = lista(pole("teksty"));
    for (auto& t : w.teksty)
      if (!z.teksty.count(t)) {
        powod = gdzie;
        powod += ": brak tekstu ";
        powod += t;
        return false;
      }
    std::string st = pole("stan");
    if (st.empty() || st == "aktywna") w.stan = StanReguly::AKTYWNA;
    else if (st == "tylko_raport") w.stan = StanReguly::TYLKO_RAPORT;
    else if (st == "zablokowana") w.stan = StanReguly::ZABLOKOWANA;
    else {
      powod = gdzie;
      powod += ": nieznany stan ";
      powod += st;
      return false;
    }
    std::string pl = pole("pilnosc");
    if (pl.empty()) w.pilnosc = Pilnosc::BRAK;
    else if (pl == "A") w.pilnosc = Pilnosc::A;
    else if (pl == "B") w.pilnosc = Pilnosc::B;
    else if (pl == "C") w.pilnosc = Pilnosc::C;
    else {
      powod = gdzie;
      powod += ": nieznana pilnosc ";
      powod += pl;
      return false;
    }
    for (auto& x : z.katalog)
      if (x.id == w.id && x.wariant == w.wariant) {
        powod = gdzie + ": powtorzona regula";
        return false;
      }
    z.katalog.push_back(w);
  }
  wyj = z;
  powod.clear();
  return true;
}

const Zestaw& zestawWbudowany() {
  static const Zestaw z = [] {
    Zestaw w;
    std::string p;
    if (!wczytajZestaw(wbudowany::PROGI, wbudowany::KATALOG, wbudowany::TEKSTY, w, p)) w = Zestaw();
    return w;
  }();
  return z;
}

}  // namespace reguly
