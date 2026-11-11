// SPDX-License-Identifier: GPL-3.0-or-later
// Magazyn danych.
// Rdzeń bez Arduino: wszystko na prostych operacjach adaptera Pliki (czytaj(), zapisz(), usun(), zmienNazwe(),
// listuj()). Formaty wersji 1 to pola prototypu (R2 karty), więc migracja 0 → 1 to parser wersji 0 z src/noc.h,
// src/czynniki.h, src/sypialnia.h, src/humor.h, src/bateria.h, src/airbridge.h i zapis tych samych pól z nagłówkiem;
// każdy następny krok n → n+1 dopisuje puste pola na końcu wiersza (T-WYM-57).
#include "magazyn.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include "airbridge.h"
#include "bateria.h"
#include "czynniki.h"
#include "humor.h"
#include "noc.h"
#include "obrazki.h"
#include "sypialnia.h"
#include "wspolne.h"

namespace {

const char* const NAGLOWEK = "#cpap-eink ";
const char* const KONIEC = "#koniec ";
const char* const RRRR = "{RRRR}";
const long MAKS_WERSJA = 9999;      // nagłówek z większą liczbą = nieczytelny
const long MAKS_WIERSZY = 1000000;  // `#koniec {n}` z większą liczbą = nieczytelny

std::vector<std::string> pola(const char* t) {
  std::vector<std::string> v;
  const char* a = t;
  for (;;) {
    const char* b = strchr(a, ';');
    v.emplace_back(a, b ? (size_t)(b - a) : strlen(a));
    if (!b) break;
    a = b + 1;
  }
  return v;
}

FormatPliku format(const char* nazwa, const char* sciezka, const char* v0, RodzajZapisu r, const char* p) {
  FormatPliku f;
  f.nazwa = nazwa;
  f.sciezka = sciezka;
  f.sciezkaV0 = v0;
  f.rodzaj = r;
  f.pola.push_back(pola(p));
  return f;
}

std::string zlacz(const std::vector<std::string>& v) {
  std::string t;
  for (size_t i = 0; i < v.size(); i++) t += (i ? ";" : "") + v[i];
  return t;
}

size_t liczbaPol(const std::string& w) { return (size_t)std::count(w.begin(), w.end(), ';') + 1; }

// Wiersz z co najmniej `ile` polami (puste pola dopisane na końcu, T-WYM-57).
std::string uzupelnij(const std::string& w, size_t ile) {
  size_t n = liczbaPol(w);
  return n >= ile ? w : w + std::string(ile - n, ';');
}

// Wiersze tekstu bez końców linii; `pelny` = tekst kończy się znakiem '\n' (ostatni wiersz nie urwany).
std::vector<std::string> wierszeTekstu(const std::string& t, bool& pelny) {
  std::vector<std::string> v;
  pelny = !t.empty() && t.back() == '\n';
  size_t a = 0;
  while (a < t.size()) {
    size_t b = t.find('\n', a);
    if (b == std::string::npos) b = t.size();
    std::string l = t.substr(a, b - a);
    if (!l.empty() && l.back() == '\r') l.pop_back();
    v.push_back(l);
    a = b + 1;
  }
  return v;
}

// Liczba dziesiętna bez znaku z całego tekstu, 0…maks; false przy czymkolwiek innym.
bool liczba(const std::string& t, long maks, long& w) {
  if (t.empty() || t.size() > 10) return false;
  for (char c : t)
    if (c < '0' || c > '9') return false;
  char* e = nullptr;
  long v = strtol(t.c_str(), &e, 10);
  if (!e || *e || v < 0 || v > maks) return false;
  w = v;
  return true;
}

// Wersja z wiersza nagłówka formatu `nazwa`; -1 = brak nagłówka albo nagłówek innego formatu.
int wersjaNaglowka(const std::string& nazwa, const std::string& l) {
  std::string p = std::string(NAGLOWEK) + nazwa + " ";
  if (l.compare(0, p.size(), p) != 0) return -1;
  long v = 0;
  if (!liczba(l.substr(p.size()), MAKS_WERSJA, v) || v < 1) return -1;
  return (int)v;
}

// Treść pliku: nagłówek wersji `wersja`, nazwy pól tej wersji, wiersze uzupełnione do jej liczby pól, przy
// przepisywanych `#koniec {n}`.
std::string tekstPliku(const FormatPliku& f, int wersja, const std::vector<std::string>& wiersze) {
  const std::vector<std::string>& p = f.pola[(size_t)wersja - 1];
  std::string t = std::string(NAGLOWEK) + f.nazwa + " " + std::to_string(wersja) + "\n" + zlacz(p) + "\n";
  for (auto& w : wiersze) t += uzupelnij(w, p.size()) + "\n";
  if (f.rodzaj == RodzajZapisu::PRZEPISANIE) t += std::string(KONIEC) + std::to_string(wiersze.size()) + "\n";
  return t;
}

// Treść pliku → odczyt (bez `.tmp`): nagłówek, nazwy pól zgodne z wersją, `#koniec` przy przepisywanych.
OdczytPliku parsuj(const FormatPliku& f, const std::string& t) {
  OdczytPliku o;
  o.stan = StanPliku::USZKODZONY;
  bool pelny = false;
  std::vector<std::string> l = wierszeTekstu(t, pelny);
  bool dopis = f.rodzaj == RodzajZapisu::DOPISANIE;
  if (dopis && !pelny && !l.empty()) l.pop_back();  // urwany ostatni wiersz pliku dopisywanego
  if (l.size() < 2) return o;
  int w = wersjaNaglowka(f.nazwa, l[0]);
  if (w < 1) return o;
  int znana = f.wersja();
  if (w <= znana) {
    if (l[1] != zlacz(f.pola[(size_t)w - 1])) return o;
  } else {  // nowsza: nazwy pól znanej wersji są początkiem listy (T-WYM-57)
    std::string p = zlacz(f.pola[(size_t)znana - 1]);
    if (l[1] != p && l[1].compare(0, p.size() + 1, p + ";") != 0) return o;
  }
  size_t od = 2, doW = l.size();
  if (!dopis) {
    if (!pelny || l.size() < 3 || l.back().compare(0, strlen(KONIEC), KONIEC) != 0) return o;
    long n = 0;
    if (!liczba(l.back().substr(strlen(KONIEC)), MAKS_WIERSZY, n) || (size_t)n != l.size() - 3) return o;
    doW = l.size() - 1;
  }
  size_t polZnanych = f.pola[(size_t)znana - 1].size();
  for (size_t i = od; i < doW; i++) {
    if (dopis && l[i].empty()) continue;
    o.wiersze.push_back(w > znana ? pierwszePola(l[i], polZnanych) : l[i]);
  }
  o.wersja = w;
  o.stan = w > znana ? StanPliku::NOWSZY : StanPliku::DOBRY;
  return o;
}

bool kompletny(const OdczytPliku& o) { return o.stan == StanPliku::DOBRY || o.stan == StanPliku::NOWSZY; }

// Plik prototypu (wersja 0): wiersze bez pustych; przy pliku dopisywanym urwany ostatni wiersz pominięty.
bool wierszeV0(Pliki& pl, const std::string& sciezka, bool dopisywany, std::vector<std::string>& w) {
  w.clear();
  std::string t;
  if (!pl.czytaj(sciezka.c_str(), t)) return false;
  bool pelny = false;
  std::vector<std::string> l = wierszeTekstu(t, pelny);
  if (dopisywany && !pelny && !l.empty()) l.pop_back();
  for (auto& x : l)
    if (!x.empty()) w.push_back(x);
  return true;
}

const char* const SYPIALNIA_CO2_V0 = "/sypialnia-co2.csv";  // drugi plik prototypu formatu `sypialnia`

// Format z kolejną wersją: pola poprzedniej wersji i `dodane` na końcu (T-WYM-57); `dodane` = "": wersja bez nowych
// pól (zmiana dozwolonych wartości, np. `noce` 4).
FormatPliku zWersja(FormatPliku f, const char* dodane) {
  std::vector<std::string> p = f.pola.back(), d = *dodane ? pola(dodane) : std::vector<std::string>();
  p.insert(p.end(), d.begin(), d.end());
  f.pola.push_back(p);
  return f;
}

}  // namespace

const std::vector<FormatPliku>& rejestrFormatow() {
  static const std::vector<FormatPliku> r = {
      // wersja 2 = pola wersji 1 i pola (src/noc.h
      // wierszNocyV2()); migracja 1 → 2 krokiem ogólnym migrujWMiejscu() (puste pola na końcu, kopia `.v1`)
      // wersja 3 = wersja 2 i `m18` (src/noc.h wierszNocyV3()), migracja 2 → 3 tym samym
      // krokiem ogólnym (puste pole na końcu)
      // wersja 4 = pola wersji 3 bez zmian i wartość `S` w polu `zrodlo` (noce ze STR.edf, src/noc.h WERSJA_NOCY);
      // migracja 3 → 4 tylko zmienia nagłówek (bez nowych pól), a firmware znający wersję 3 czyta plik tylko do odczytu
      // wersja 5 = wersja 4 i `m14;m15` (M-14 objętość oddechowa, M-15 wentylacja minutowa, src/noc.h
      // wierszNocyV5()), migracja 4 → 5 krokiem ogólnym (puste pola na końcu, kopia `.v4`)
      zWersja(zWersja(zWersja(zWersja(format("noce", "/dane/noce/{RRRR}.csv", "/noce.csv", RodzajZapisu::PRZEPISANIE,
                     "data;wazna;m01_min;ahi;cmed;cp95;wmed;wp95;emed;ep95;psmed;psp95;ustawienia;zrodlo;czesciowa;"
                     "epap_str;oa;ca;h;csr;import"),
              "m13;m17;pna;srn;m08;mpa;mpi;ipc;eep;mns;mxs;ua;m10;m10p;eai;eax;ans;axs;epr;tryb"),
              "m18"),
              ""),
              "m14;m15"),
      format("czynniki", "/dane/czynniki.csv", "/czynniki.csv", RodzajZapisu::PRZEPISANIE,
             "data;stan;kody;warstwa;termin;pokazano"),
      // wersja 2 = seria CO2 nocy do S-K-01 i S-K-13 (src/sypialnia.h NocSypialni::co2Ponad…)
      zWersja(format("sypialnia", "/dane/sypialnia/{RRRR}.csv", "/sypialnia.csv", RodzajZapisu::PRZEPISANIE,
                     "data;temp;wilg;co2"),
              "co2_ponad;co2_maks;co2_seria"),
      format("humor", "/dane/humor.csv", "/humor.csv", RodzajZapisu::DOPISANIE, "data;typ;klucz;tryb;scena;zdanie"),
      format("wybudzenia", "/log/wybudzenia.csv", "/bateria.csv", RodzajZapisu::DOPISANIE,
             "epoch;mV;wifi_ms;aktywny_ms;powod"),
      format("pominiete", "/dane/ab-pominiete.csv", "/ab_pominiete.txt", RodzajZapisu::PRZEPISANIE, "day"),
      format("proby", "/dane/ab-proby.csv", "/ab_proby.txt", RodzajZapisu::PRZEPISANIE, "day;proby;ostatnia;stale"),
      // pamięć reguł (src/reguly_pamiec.h): bez pliku prototypu
      format("reguly", "/dane/reguly.csv", "", RodzajZapisu::PRZEPISANIE,
             "id;wariant;aktywna_od;ostatnio;licznik;stan_wzorca;wiem;omowione"),
      // odpowiedzi tak/nie 5.12.8 (src/sprawy.h, pytanie sennosc|gorzej|przerwa, odpowiedź tak|nie|brak)
      format("odpowiedzi", "/dane/odpowiedzi.csv", "", RodzajZapisu::PRZEPISANIE, "data;pytanie;odpowiedz"),
      // prognoza godzinowa Open-Meteo (src/slot_dzienny.h, 48 wierszy: dziś i jutro) i dziennik obrazka dnia
      // (src/obrazki.h, R-KON-61); bez plików prototypu
      format("prognoza", "/dane/prognoza.csv", "", RodzajZapisu::PRZEPISANIE,
             "data;godz;temp;opad;chmury;porywy;pm25;brzoza;trawy;olcha;bylica;pobrano"),
      format("obrazki", "/dane/obrazki.csv", "", RodzajZapisu::DOPISANIE, "data;plik;pula"),
      // porównanie nocy z plików z raportem JSON (src/porownanie.h, ADR-0036 „Zmiana 2026-11-09”), ostatnie 30 dób
      format("porownanie", "/dane/porownanie.csv", "", RodzajZapisu::PRZEPISANIE,
             "data;werdykt;rozjazd;pominiete;przyczyna"),
  };
  return r;
}

const FormatPliku* formatPliku(const std::vector<FormatPliku>& rejestr, const std::string& nazwa) {
  for (auto& f : rejestr)
    if (f.nazwa == nazwa) return &f;
  return nullptr;
}

std::string nazwaPliku(const std::string& s) {
  if (s.compare(0, 6, "/dane/") == 0) return s.substr(6);
  if (!s.empty() && s[0] == '/') return s.substr(1);
  return s;
}

std::string pierwszePola(const std::string& w, size_t ile) {
  if (ile == 0) return "";
  size_t n = 0;
  for (size_t i = 0; i < w.size(); i++)
    if (w[i] == ';' && ++n == ile) return w.substr(0, i);
  return w;
}

Magazyn::Magazyn(Pliki& pliki, std::function<void(const std::string&)> log, const std::vector<FormatPliku>& rejestr)
    : pliki_(pliki), log_(std::move(log)), rejestr_(rejestr) {}

bool Magazyn::starszy(const std::string& s) const {
  return std::find(starsze_.begin(), starsze_.end(), s) != starsze_.end();
}
void Magazyn::bezStarszego(const std::string& s) {
  starsze_.erase(std::remove(starsze_.begin(), starsze_.end(), s), starsze_.end());
}

OdczytPliku Magazyn::czytaj(const std::string& nazwa, const std::string& sciezka) {
  OdczytPliku o;
  const FormatPliku* f = formatPliku(rejestr_, nazwa);
  if (!f || f->wersja() < 1) {
    loguj("magazyn: nieznany format " + nazwa);
    o.stan = StanPliku::USZKODZONY;
    return o;
  }
  std::string t, tt, tmp = sciezka + ".tmp";
  bool jest = pliki_.czytaj(sciezka.c_str(), t);
  if (jest) o = parsuj(*f, t);
  std::string krotka = nazwaPliku(sciezka);
  if (pliki_.czytaj(tmp.c_str(), tt)) {  // T-WYM-53
    OdczytPliku b = parsuj(*f, tt);
    if (!kompletny(o) && kompletny(b)) {
      if (pliki_.zmienNazwe(tmp.c_str(), sciezka.c_str()))
        loguj(krotka + ": plik docelowy " + (jest ? "uszkodzony" : "brak") + ", użyty kompletny plik tymczasowy");
      else
        loguj(krotka + ": plik tymczasowy kompletny, ale zmiana nazwy się nie udała");
      o = b;
      jest = true;
    } else {
      pliki_.usun(tmp.c_str());
      if (!kompletny(b)) loguj(krotka + ": plik tymczasowy niekompletny, usunięty");
    }
  }
  if (!jest) return o;  // BRAK
  if (o.stan == StanPliku::USZKODZONY) {
    std::string u = odlozUszkodzony(sciezka);
    loguj(krotka + ": plik uszkodzony" + (!u.empty() ? " (odłożony jako " + nazwaPliku(u) + ")" : "") +
          ", praca bez pliku");
    o.wiersze.clear();
    return o;
  }
  if (o.stan == StanPliku::NOWSZY) {
    if (!tylkoDoOdczytu(sciezka)) tylkoOdczyt_.push_back(sciezka);
    loguj(krotka + ": dane z nowszej wersji (" + std::to_string(o.wersja) + ", znana " + std::to_string(f->wersja()) +
          "), tylko do odczytu");
  } else if (o.wersja < f->wersja() && !starszy(sciezka)) {
    starsze_.push_back(sciezka);
  }
  return o;
}

// Kopie `.uszkodzony` przesuwane (najnowsza bez numeru, potem `.1`, `.2`);
// przy komplecie KOPII_USZKODZONYCH kopii usunięta najmniejsza (przy równych najstarsza), nie zawsze najstarsza, żeby
// powtarzane uszkodzenie małego pliku nie zabrało kopii z historią; przesuwane tylko kopie nowsze od usuniętej.
// Rozmiar z odczytu treści (zdarzenie rzadkie: czwarte uszkodzenie tego samego pliku). Zwraca nazwę kopii albo ""
// (zmiana nazwy się nie udała).
std::string Magazyn::odlozUszkodzony(const std::string& sciezka) {
  std::string u = sciezka + ".uszkodzony";
  auto nazwa = [&](int i) { return i == 0 ? u : u + "." + std::to_string(i); };
  int doUsuniecia = KOPII_USZKODZONYCH - 1;  // najstarsza, gdy nie ma kompletu (wtedy jej miejsce jest wolne)
  if (pliki_.istnieje(nazwa(KOPII_USZKODZONYCH - 1).c_str())) {
    size_t min = SIZE_MAX;
    for (int i = KOPII_USZKODZONYCH - 1; i >= 0; i--) {
      std::string t;
      if (!pliki_.czytaj(nazwa(i).c_str(), t)) t.clear();
      if (t.size() < min) {
        min = t.size();
        doUsuniecia = i;
      }
    }
    pliki_.usun(nazwa(doUsuniecia).c_str());
  }
  for (int i = doUsuniecia - 1; i >= 0; i--)
    if (pliki_.istnieje(nazwa(i).c_str())) pliki_.zmienNazwe(nazwa(i).c_str(), nazwa(i + 1).c_str());
  return pliki_.zmienNazwe(sciezka.c_str(), u.c_str()) ? u : std::string();
}

bool Magazyn::zapiszCaly(const FormatPliku& f, const std::string& sciezka, const std::vector<std::string>& wiersze,
                         int wersja) {
  if (!pliki_.zamontowane()) return false;
  std::string krotka = nazwaPliku(sciezka);
  for (auto& w : wiersze)
    if (w.find('\n') != std::string::npos || w.find('\r') != std::string::npos) {
      loguj("zapis " + krotka + ": wiersz ze znakiem końca linii, bez zapisu");
      return false;
    }
  std::string t = tekstPliku(f, wersja >= 1 && wersja <= f.wersja() ? wersja : f.wersja(), wiersze),
              tmp = sciezka + ".tmp";
  size_t n = pliki_.zapisz(tmp.c_str(), t);
  if (n != t.size()) {  // T-WYM-51: brak miejsca, błąd nośnika
    pliki_.usun(tmp.c_str());
    loguj("zapis " + krotka + ": zapisano " + std::to_string(n) + " z " + std::to_string(t.size()) +
          " B, zostaje poprzednia wersja");
    return false;
  }
  if (!pliki_.zmienNazwe(tmp.c_str(), sciezka.c_str())) {
    pliki_.usun(tmp.c_str());
    loguj("zapis " + krotka + ": zmiana nazwy się nie udała, zostaje poprzednia wersja");
    return false;
  }
  return true;
}

bool Magazyn::zapisz(const std::string& nazwa, const std::string& sciezka, const std::vector<std::string>& wiersze) {
  const FormatPliku* f = formatPliku(rejestr_, nazwa);
  if (!f || f->wersja() < 1) return false;
  if (tylkoDoOdczytu(sciezka)) return false;
  if (starszy(sciezka)) {
    odrzuconoStarszy_ = true;
    loguj("zapis " + nazwaPliku(sciezka) + ": plik czeka na migrację, bez zapisu");
    return false;
  }
  return zapiszCaly(*f, sciezka, wiersze);
}

bool Magazyn::dopisz(const std::string& nazwa, const std::string& sciezka, const std::string& wiersz) {
  const FormatPliku* f = formatPliku(rejestr_, nazwa);
  if (!f || f->wersja() < 1 || !pliki_.zamontowane()) return false;
  if (tylkoDoOdczytu(sciezka)) return false;
  if (starszy(sciezka)) {
    odrzuconoStarszy_ = true;
    return false;
  }
  if (wiersz.find('\n') != std::string::npos || wiersz.find('\r') != std::string::npos) return false;
  std::string krotka = nazwaPliku(sciezka);
  if (!pliki_.istnieje(sciezka.c_str())) return zapiszCaly(*f, sciezka, {wiersz});
  std::string l;
  int w = pliki_.pierwszyWiersz(sciezka.c_str(), l) ? wersjaNaglowka(f->nazwa, l) : -1;
  if (w < 1) {  // nagłówek nieczytelny: plik odłożony, nowy od tego wiersza
    std::string u = odlozUszkodzony(sciezka);
    loguj(krotka + ": plik bez czytelnego nagłówka, odłożony jako " + nazwaPliku(u.empty() ? sciezka : u) + ", nowy plik");
    return zapiszCaly(*f, sciezka, {wiersz});
  }
  if (w > f->wersja()) {
    tylkoOdczyt_.push_back(sciezka);
    loguj(krotka + ": dane z nowszej wersji, tylko do odczytu");
    return false;
  }
  if (w < f->wersja()) {
    starsze_.push_back(sciezka);
    odrzuconoStarszy_ = true;
    return false;
  }
  return pliki_.dopiszWiersz(sciezka.c_str(), uzupelnij(wiersz, f->pola.back().size()));
}

std::vector<std::string> Magazyn::sciezki(const std::string& nazwa) {
  std::vector<std::string> v;
  const FormatPliku* f = formatPliku(rejestr_, nazwa);
  if (!f || !pliki_.zamontowane()) return v;
  size_t r = f->sciezka.find(RRRR);
  if (r == std::string::npos) {
    v.push_back(f->sciezka);
    return v;
  }
  std::string kat = f->sciezka.substr(0, r), ogon = f->sciezka.substr(r + strlen(RRRR));
  std::vector<std::string> n;
  pliki_.listuj(kat.c_str(), n);
  for (auto& s : n) {
    if (s.size() != kat.size() + 4 + ogon.size() || s.compare(0, kat.size(), kat) != 0 ||
        s.compare(kat.size() + 4, std::string::npos, ogon) != 0)
      continue;
    bool cyfry = true;
    for (size_t i = kat.size(); i < kat.size() + 4; i++) cyfry = cyfry && s[i] >= '0' && s[i] <= '9';
    if (cyfry) v.push_back(s);
  }
  std::sort(v.begin(), v.end());
  return v;
}

std::string Magazyn::sciezkaRoku(const std::string& nazwa, const std::string& rok) const {
  const FormatPliku* f = formatPliku(rejestr_, nazwa);
  if (!f) return "";
  std::string s = f->sciezka;
  size_t r = s.find(RRRR);
  if (r != std::string::npos) s.replace(r, strlen(RRRR), rok);
  return s;
}

// ---------- migracje ----------

// Rekordy wersji 0 jednego formatu → wiersze wersji 1 pogrupowane po ścieżce docelowej; `przed` = liczba rekordów
// czytelnych parserem wersji 0. Pliki prototypu pod `{katalog}{sciezkaV0}` ("" = pliki na partycji, "/przywracanie" =
// zrzut wgrany komendą `W`). false = format bez plików prototypu.
static bool rekordyV0(Pliki& pl, const FormatPliku& f, const std::function<std::string(const std::string&)>& sciezka,
                      std::map<std::string, std::vector<std::string>>& wynik, int& przed, int& pominiete,
                      const std::string& katalog = "") {
  wynik.clear();
  przed = pominiete = 0;
  std::vector<std::string> w;
  bool dopis = f.rodzaj == RodzajZapisu::DOPISANIE;
  if (f.nazwa == "sypialnia") {
    std::vector<std::string> c;
    bool a = wierszeV0(pl, katalog + f.sciezkaV0, false, w),
         b = wierszeV0(pl, katalog + SYPIALNIA_CO2_V0, false, c);
    if (!a && !b) return false;
    std::map<std::string, NocSypialni> noce;  // po dacie
    for (auto& l : w) {
      NocSypialni n;
      if (!parsujSypialnie(l, n)) {
        pominiete++;
        continue;
      }
      noce[n.data] = n;  // 4 pola (wczesna wersja prototypowa): CO2 z wiersza
    }
    for (auto& l : c) {
      std::string d;
      float x;
      if (!parsujCo2Nocy(l, d, x)) {
        pominiete++;
        continue;
      }
      auto i = noce.find(d);
      if (i == noce.end()) i = noce.emplace(d, NocSypialni{d, NAN, NAN}).first;
      i->second.co2 = x;  // jak Przebieg::wczytajPliki() prototypu: plik CO2 ma pierwszeństwo
    }
    for (auto& kv : noce) {
      const NocSypialni& n = kv.second;
      std::string co2 = std::isnan(n.co2) ? "" : std::to_string((long)std::lround(n.co2));
      wynik[sciezka(n.data.substr(0, 4))].push_back(zapiszSypialnie(n) + ";" + co2 + polaCo2V2(n));  // wersja 2
      przed++;
    }
    return true;
  }
  if (!wierszeV0(pl, katalog + f.sciezkaV0, dopis, w)) return false;
  if (f.nazwa == "noce") {
    std::vector<Noc> v;
    for (auto& l : w) {
      Noc n;
      if (parsujNoc(l, 'A', n)) v.push_back(n);
      else pominiete++;
    }
    std::stable_sort(v.begin(), v.end(), [](const Noc& x, const Noc& y) { return x.data < y.data; });
    for (auto& n : v) wynik[sciezka(n.data.substr(0, 4))].push_back(wierszNocyV1(n));
    przed = (int)v.size();
  } else if (f.nazwa == "pominiete" || f.nazwa == "proby") {
    std::vector<std::string> nowe =
        f.nazwa == "pominiete" ? wierszePominietych(parsujPominiete(w)) : wierszeProb(parsujProby(w));
    pominiete = (int)w.size() - (int)nowe.size();
    if (pominiete < 0) pominiete = 0;
    przed = (int)nowe.size();
    wynik[sciezka("")] = nowe;
  } else {
    std::vector<std::string>& cel = wynik[sciezka("")];
    for (auto& l : w) {
      std::string n;
      if (f.nazwa == "czynniki") {
        WpisCzynnikow x;
        if (parsujWpis(l, x)) n = zapiszWpis(x);
      } else if (f.nazwa == "humor") {
        WpisHumoru x;
        if (parsujHumor(l, x)) n = zapiszHumor(x);
      } else if (f.nazwa == "wybudzenia") {
        WpisBaterii x;
        if (parsujBaterie(l, x)) n = zapiszBaterie(x);
      }
      if (n.empty()) pominiete++;
      else cel.push_back(n);
    }
    przed = (int)cel.size();
  }
  return true;
}

// ---------- scalenie ----------
namespace {
std::string kluczWiersza(const std::string& w) {
  size_t p = w.find(';');
  return p == std::string::npos ? w : w.substr(0, p);
}
// Klucze tej samej długości (daty, czasy unix) znak po znaku; krótszy (np. `day` AirBridge) najpierw, czyli liczbowo.
struct PorownajKlucz {
  bool operator()(const std::string& a, const std::string& b) const {
    return a.size() != b.size() ? a.size() < b.size() : a < b;
  }
};
// Stan wpisu czynników (drugie pole: C, O, B) albo 0 przy wierszu nieczytelnym.
char stanCzynnikow(const std::string& w) {
  WpisCzynnikow x;
  return parsujWpis(w, x) ? x.stan : 0;
}
bool maOdpowiedz(const std::vector<std::string>& w) {
  for (auto& x : w)
    if (stanCzynnikow(x) == 'O') return true;
  return false;
}
// Noc pusta (bez minut z maską) albo częściowa: do zastąpienia pełnym rekordem z drugiego źródła.
bool nocSlaba(const std::string& w, Noc& n) {
  if (!parsujNocV5(w, n)) return true;  // wiersze wersji 1…5
  return n.czesciowa || n.m01 < 0;
}
const char* const ZNACZNIK_W_TOKU = "w toku";
std::string znacznikOk(uint32_t crc) {
  char b[24];
  snprintf(b, sizeof b, "ok %08lx", (unsigned long)crc);
  return b;
}
std::string bezKoncaLinii(std::string t) {
  while (!t.empty() && (t.back() == '\n' || t.back() == '\r')) t.pop_back();
  return t;
}
}  // namespace

std::string znacznikMigracji(const std::string& nazwa) { return "/dane/.migracja-" + nazwa; }

std::vector<std::string> scalWiersze(const std::string& nazwa, const std::vector<std::string>& flash,
                                     const std::vector<std::string>& zrodlo, int* dodane) {
  std::map<std::string, std::pair<std::vector<std::string>, std::vector<std::string>>, PorownajKlucz> m;
  for (auto& w : flash) m[kluczWiersza(w)].first.push_back(w);
  for (auto& w : zrodlo) m[kluczWiersza(w)].second.push_back(w);
  std::vector<std::string> v;
  int d = 0;
  for (auto& kv : m) {
    const std::vector<std::string>& f = kv.second.first;
    const std::vector<std::string>& z = kv.second.second;
    if (f.empty()) {
      v.insert(v.end(), z.begin(), z.end());
      d += (int)z.size();
      continue;
    }
    Noc nf, nz;
    if (nazwa == "noce" && !z.empty() && nocSlaba(f.back(), nf) && !nocSlaba(z.back(), nz)) {
      nz.import = nf.import;  // R1: chwila importu z rekordu we flash (noc pełna dopiero teraz)
      // puste pola nocy z drugiego źródła (zrzut z karty nie ma pól wersji 2, M-03, M-09)
      // dostają wartości z rekordu we flash, jak wpiszImport() (ZASTAPIONA); wiersz w wersji 2
      uzupelnijNoc(nz, nf);
      v.push_back(wierszNocyV5(nz));  // wersja 5 (M-14, M-15)
      d++;
      continue;
    }
    // odpowiedź (`O`) wygrywa z wpisem bez odpowiedzi (`C` czeka, `B` brak odpowiedzi); dwa `O`: flash
    if (nazwa == "czynniki" && !z.empty() && !maOdpowiedz(f) && maOdpowiedz(z)) {
      for (auto& w : z)
        if (stanCzynnikow(w) == 'O') {
          v.push_back(w);
          d++;
          break;
        }
      continue;
    }
    v.insert(v.end(), f.begin(), f.end());
    // dziennik wyświetleń (kilka wierszy na dobę) = suma wierszy bez powtórzeń całego wiersza. Dziennik
    // wybudzeń nie: jego klucz (chwila wybudzenia) jest jednoznaczny, dwa różne wiersze z tą samą chwilą to ten sam
    // wpis, więc zostaje wiersz z flash
    if (nazwa == "humor") {
      std::set<std::string> sa(f.begin(), f.end());
      for (auto& w : z)
        if (sa.insert(w).second) {
          v.push_back(w);
          d++;
        }
    }
  }
  if (dodane) *dodane = d;
  return v;
}

// ---------- migracje na nowe ścieżki (znacznik migracji: U5, U6) ----------

bool Magazyn::crcZrodel(const FormatPliku& f, uint32_t& crc) {
  std::string a, b;
  bool ja = pliki_.czytaj(f.sciezkaV0.c_str(), a);
  bool jb = f.nazwa == "sypialnia" && pliki_.czytaj(SYPIALNIA_CO2_V0, b);
  if (!ja && !jb) return false;
  std::string t = std::string(ja ? "1" : "0") + a + "\n#co2 " + (jb ? "1" : "0") + b;
  crc = crc32((const uint8_t*)t.data(), t.size());
  return true;
}

bool Magazyn::zrodloPotwierdzone(const FormatPliku& f) {
  uint32_t crc = 0;
  std::string zn;
  if (!crcZrodel(f, crc)) return true;  // źródła nie ma: nic do potwierdzania
  return pliki_.czytaj(znacznikMigracji(f.nazwa).c_str(), zn) && bezKoncaLinii(zn) == znacznikOk(crc);
}

std::vector<std::string> Magazyn::plikiNowejSciezki(const FormatPliku& f) {
  std::vector<std::string> v;
  size_t r = f.sciezka.find(RRRR);
  if (r == std::string::npos) {
    for (const std::string& s : {f.sciezka, f.sciezka + ".tmp"})
      if (pliki_.istnieje(s.c_str())) v.push_back(s);
    return v;
  }
  std::string kat = f.sciezka.substr(0, r), ogon = f.sciezka.substr(r + strlen(RRRR));
  std::vector<std::string> n;
  pliki_.listuj(kat.c_str(), n);
  for (auto& s : n) {
    std::string b = s.size() > 4 && s.compare(s.size() - 4, 4, ".tmp") == 0 ? s.substr(0, s.size() - 4) : s;
    if (b.size() != kat.size() + 4 + ogon.size() || b.compare(0, kat.size(), kat) != 0 ||
        b.compare(kat.size() + 4, std::string::npos, ogon) != 0)
      continue;
    bool cyfry = true;
    for (size_t i = kat.size(); i < kat.size() + 4; i++) cyfry = cyfry && b[i] >= '0' && b[i] <= '9';
    if (cyfry) v.push_back(s);
  }
  return v;
}

bool Magazyn::migrujPlikNaNowa(const FormatPliku& f, WynikMigracji& wy, uint32_t crcZrodla) {
  wy.plik = nazwaPliku(f.sciezkaV0);
  const std::string zn = znacznikMigracji(f.nazwa);
  // U5: znacznik `w toku` przed pierwszym plikiem nowej ścieżki (zanik zasilania w trakcie = migracja od nowa)
  if (!zapiszSurowy(zn, std::string(ZNACZNIK_W_TOKU) + "\n")) {
    wy.powod = "znacznik migracji";
    loguj("migracja " + wy.plik + ": błąd (" + wy.powod + "), plik zostaje w wersji 0");
    return false;
  }
  std::map<std::string, std::vector<std::string>> cel;
  int pominiete = 0;
  auto sc = [&](const std::string& rok) { return sciezkaRoku(f.nazwa, rok); };
  rekordyV0(pliki_, f, sc, cel, wy.przed, pominiete);
  if (pominiete) loguj("migracja " + wy.plik + ": pominięte " + std::to_string(pominiete) + " wierszy nieczytelnych");
  std::vector<std::string> zapisane;
  bool ok = true;
  for (auto& kv : cel) {
    if (!zapiszCaly(f, kv.first, kv.second)) {
      ok = false;
      wy.powod = "zapis " + nazwaPliku(kv.first);
      break;
    }
    zapisane.push_back(kv.first);
  }
  if (ok) {  // porównanie liczby rekordów (T-WYM-55)
    for (auto& s : zapisane) {
      OdczytPliku o = czytaj(f.nazwa, s);
      if (o.stan != StanPliku::DOBRY) {
        ok = false;
        wy.powod = "odczyt " + nazwaPliku(s);
        break;
      }
      wy.po += (int)o.wiersze.size();
    }
    if (ok && wy.po != wy.przed) {
      ok = false;
      wy.powod = "liczba rekordów " + std::to_string(wy.przed) + " → " + std::to_string(wy.po);
    }
  }
  // migracja potwierdzona dopiero ze znacznikiem `ok`; bez niego pliki nowej ścieżki usunięte teraz, zanim cokolwiek
  // do nich dopisze (inaczej następny start usunąłby je razem z nowymi rekordami)
  if (ok && !zapiszSurowy(zn, znacznikOk(crcZrodla) + "\n")) {
    ok = false;
    wy.powod = "znacznik migracji";
  }
  if (!ok) {  // częściowe pliki wynikowe usunięte; źródło nietknięte (T-WYM-56)
    for (auto& s : zapisane) {
      pliki_.usun(s.c_str());
      pliki_.usun((s + ".tmp").c_str());
    }
    pliki_.usun(zn.c_str());
    loguj("migracja " + wy.plik + ": błąd (" + wy.powod + "), plik zostaje w wersji 0");
    return false;
  }
  utworzone_.insert(utworzone_.end(), zapisane.begin(), zapisane.end());
  utworzone_.push_back(zn);
  loguj("migracja " + wy.plik + ": wersja 0 → " + std::to_string(f.wersja()) + ", " + std::to_string(wy.po) +
        " rekordów");
  wy.ok = true;
  return true;
}

// U6: plik prototypu zmieniony po migracji (poprzednia wersja po powrocie przez bootloader) albo pliki nowej ścieżki bez
// znacznika: rekordy prototypu scalone z plikami nowej ścieżki (scalWiersze(), nic nie jest usuwane), potem znacznik.
bool Magazyn::scalZrodlo(const FormatPliku& f, WynikMigracji& wy, uint32_t crcZrodla) {
  wy.plik = nazwaPliku(f.sciezkaV0);
  std::map<std::string, std::vector<std::string>> cel;
  int pominiete = 0, dodane = 0;
  auto sc = [&](const std::string& rok) { return sciezkaRoku(f.nazwa, rok); };
  rekordyV0(pliki_, f, sc, cel, wy.przed, pominiete);
  bool ok = true;
  for (auto& kv : cel) {
    OdczytPliku o = czytaj(f.nazwa, kv.first);
    if (o.stan == StanPliku::NOWSZY) {
      ok = false;
      wy.powod = "plik " + nazwaPliku(kv.first) + " w wersji " + std::to_string(o.wersja);
      break;
    }
    int d = 0;
    std::vector<std::string> w = scalWiersze(f.nazwa, o.wiersze, kv.second, &d);
    if (!d && o.stan == StanPliku::DOBRY) continue;
    // (regresja wykryta testem symulatora): plik nowej ścieżki w wersji starszej niż znana (np. noce w wersji 1)
    // czeka na migrację w miejscu, której przed oznaczeniem wersji robić nie wolno (T-WYM-43). Scalenie zapisuje
    // go więc w jego własnej wersji (wiersze przycięte do jej pól; poprzednia wersja firmware dalej go czyta), a krok
    // 1 → 2 zrobi potem migrujWMiejscu()
    int wersjaZapisu = o.stan == StanPliku::DOBRY && o.wersja >= 1 && o.wersja < f.wersja() ? o.wersja : f.wersja();
    if (wersjaZapisu < f.wersja())
      for (auto& x : w) x = pierwszePola(x, f.pola[(size_t)wersjaZapisu - 1].size());
    if (!zapiszCaly(f, kv.first, w, wersjaZapisu)) {
      ok = false;
      wy.powod = "zapis " + nazwaPliku(kv.first);
      break;
    }
    dodane += d;
  }
  if (ok) {  // każdy rekord prototypu (po kluczu) jest w plikach nowej ścieżki
    for (auto& kv : cel) {
      std::set<std::string> klucze;
      for (auto& w : czytaj(f.nazwa, kv.first).wiersze) klucze.insert(kluczWiersza(w));
      for (auto& w : kv.second) wy.po += klucze.count(kluczWiersza(w)) ? 1 : 0;
    }
    if (wy.po != wy.przed) {
      ok = false;
      wy.powod = "liczba rekordów " + std::to_string(wy.przed) + " → " + std::to_string(wy.po);
    }
  }
  if (ok && !zapiszSurowy(znacznikMigracji(f.nazwa), znacznikOk(crcZrodla) + "\n")) {
    ok = false;  // pliki scalone zostają (nadzbiór); scalenie powtórzy się przy następnym starcie
    wy.powod = "znacznik migracji";
  }
  if (!ok) {
    loguj("migracja " + wy.plik + ": scalenie z plikami nowej ścieżki nieudane (" + wy.powod + ")");
    return false;
  }
  loguj("migracja " + wy.plik + ": plik prototypu zmieniony po migracji, scalono, dodane " + std::to_string(dodane) +
        " rekordów");
  wy.ok = true;
  return true;
}

bool Magazyn::migrujNaNoweSciezki(bool sprawdzCrc) {
  migracje_.clear();
  utworzone_.clear();
  if (!pliki_.zamontowane()) return true;
  bool wszystkie = true;
  for (auto& f : rejestr_) {
    if (f.sciezkaV0.empty() || f.wersja() < 1) continue;
    std::string zn;
    bool jestZn = pliki_.czytaj(znacznikMigracji(f.nazwa).c_str(), zn);
    zn = bezKoncaLinii(zn);
    // poza startem migracja potwierdzona (`ok …`) bez czytania plików prototypu
    if (!sprawdzCrc && jestZn && zn.compare(0, 3, "ok ") == 0) continue;
    uint32_t crc = 0;
    if (!crcZrodel(f, crc)) continue;  // bez pliku prototypu
    if (jestZn && zn == znacznikOk(crc)) continue;  // migracja potwierdzona, źródło bez zmian
    std::vector<std::string> nowe = plikiNowejSciezki(f);
    WynikMigracji w;
    bool udana;
    if (jestZn && zn == ZNACZNIK_W_TOKU) {  // U5: przerwana (zanik zasilania): od nowa ze źródła
      for (auto& s : nowe) pliki_.usun(s.c_str());
      loguj("migracja " + nazwaPliku(f.sciezkaV0) + ": przerwana wcześniej, pliki nowej ścieżki (" +
            std::to_string(nowe.size()) + ") usunięte, migracja od nowa");
      udana = migrujPlikNaNowa(f, w, crc);
    } else if (jestZn || !nowe.empty()) {
      udana = scalZrodlo(f, w, crc);  // U6
    } else {
      udana = migrujPlikNaNowa(f, w, crc);
    }
    if (!udana) wszystkie = false;
    migracje_.push_back(w);
  }
  return wszystkie;
}

void Magazyn::wycofajMigracje() {
  for (auto& s : utworzone_) {
    pliki_.usun(s.c_str());
    loguj("migracja wycofana: " + nazwaPliku(s) + " usunięty");
  }
  utworzone_.clear();
}


bool Magazyn::migrujPlikWMiejscu(const FormatPliku& f, const std::string& s, WynikMigracji& wy) {
  wy.plik = nazwaPliku(s);
  OdczytPliku o = czytaj(f.nazwa, s);
  wy.przed = (int)o.wiersze.size();
  std::string surowe;
  if (o.stan != StanPliku::DOBRY || !pliki_.czytaj(s.c_str(), surowe)) {
    wy.powod = "odczyt";
    loguj("migracja " + wy.plik + ": błąd (plik nieczytelny), zostaje w wersji " + std::to_string(o.wersja));
    return false;
  }
  std::string kopia = s + ".v" + std::to_string(o.wersja);
  if (pliki_.zapisz(kopia.c_str(), surowe) != surowe.size()) {
    pliki_.usun(kopia.c_str());
    wy.powod = "kopia " + nazwaPliku(kopia);
  } else {
    std::vector<std::string> w = o.wiersze;
    for (int n = o.wersja; n < f.wersja(); n++) {  // krok n → n+1: puste pole na końcu (T-WYM-57)
      size_t ile = f.pola[(size_t)n].size();
      for (auto& x : w) x = uzupelnij(x, ile);
    }
    if (!zapiszCaly(f, s, w)) {
      wy.powod = "zapis";
    } else {
      OdczytPliku po = czytaj(f.nazwa, s);
      wy.po = (int)po.wiersze.size();
      if (po.stan == StanPliku::DOBRY && po.wersja == f.wersja() && wy.po == wy.przed) {
        bezStarszego(s);
        for (int k = 1; k < o.wersja; k++) pliki_.usun((s + ".v" + std::to_string(k)).c_str());
        // kopia wersji 0 (plik prototypu) usunięta przy następnej udanej migracji (T-WYM-55), ale tylko po
        // potwierdzonej migracji 0 → 1 (znacznik `ok` z CRC obecnego pliku prototypu)
        if (!f.sciezkaV0.empty() && zrodloPotwierdzone(f)) {
          if (pliki_.istnieje(f.sciezkaV0.c_str())) pliki_.usun(f.sciezkaV0.c_str());
          if (f.nazwa == "sypialnia" && pliki_.istnieje(SYPIALNIA_CO2_V0)) pliki_.usun(SYPIALNIA_CO2_V0);
          if (pliki_.istnieje(znacznikMigracji(f.nazwa).c_str())) pliki_.usun(znacznikMigracji(f.nazwa).c_str());
        } else if (!f.sciezkaV0.empty()) {
          loguj("migracja " + wy.plik + ": plik prototypu " + nazwaPliku(f.sciezkaV0) +
                " zostaje (migracja 0 → 1 niepotwierdzona)");
        }
        loguj("migracja " + wy.plik + ": wersja " + std::to_string(o.wersja) + " → " + std::to_string(f.wersja()) +
              ", " + std::to_string(wy.po) + " rekordów, kopia " + nazwaPliku(kopia));
        wy.ok = true;
        return true;
      }
      wy.powod = "liczba rekordów " + std::to_string(wy.przed) + " → " + std::to_string(wy.po);
      std::string tmp = s + ".tmp";  // powrót do treści sprzed migracji (atomowo)
      if (pliki_.zapisz(tmp.c_str(), surowe) == surowe.size()) pliki_.zmienNazwe(tmp.c_str(), s.c_str());
      else pliki_.usun(tmp.c_str());
    }
  }
  if (!starszy(s)) starsze_.push_back(s);
  loguj("migracja " + wy.plik + ": błąd (" + wy.powod + "), zostaje w wersji " + std::to_string(o.wersja));
  return false;
}

bool Magazyn::migrujWMiejscu() {
  migracje_.clear();
  if (!pliki_.zamontowane()) return true;
  bool wszystkie = true;
  for (auto& f : rejestr_) {
    if (f.wersja() < 2) continue;
    for (auto& s : sciezki(f.nazwa)) {
      std::string l;
      if (!pliki_.istnieje(s.c_str()) && !pliki_.istnieje((s + ".tmp").c_str())) continue;
      // wersja z nagłówka bez pełnego odczytu, gdy plik nie zostawił `.tmp` (przypadek zwykły)
      int w = -1;
      if (!pliki_.istnieje((s + ".tmp").c_str()) && pliki_.pierwszyWiersz(s.c_str(), l)) w = wersjaNaglowka(f.nazwa, l);
      if (w >= f.wersja()) continue;
      if (w < 0) {  // `.tmp` albo nagłówek nieczytelny: wersja z pełnego odczytu
        OdczytPliku o = czytaj(f.nazwa, s);
        if (o.stan != StanPliku::DOBRY || o.wersja >= f.wersja()) continue;
      }
      WynikMigracji m;
      if (!migrujPlikWMiejscu(f, s, m)) wszystkie = false;
      migracje_.push_back(m);
    }
  }
  return wszystkie;
}

const std::vector<WynikMigracji>& Magazyn::migracje() const { return migracje_; }

// ---------- przywracanie (T-WYM-62, T-WYM-87; implementacja ----------

bool Magazyn::zapiszSurowy(const std::string& sciezka, const std::string& tresc) {
  if (!pliki_.zamontowane()) return false;
  std::string tmp = sciezka + ".tmp", krotka = nazwaPliku(sciezka);
  size_t n = pliki_.zapisz(tmp.c_str(), tresc);
  if (n != tresc.size()) {  // T-WYM-51
    pliki_.usun(tmp.c_str());
    loguj("zapis " + krotka + ": zapisano " + std::to_string(n) + " z " + std::to_string(tresc.size()) +
          " B, zostaje poprzednia wersja");
    return false;
  }
  if (!pliki_.zmienNazwe(tmp.c_str(), sciezka.c_str())) {
    pliki_.usun(tmp.c_str());
    loguj("zapis " + krotka + ": zmiana nazwy się nie udała, zostaje poprzednia wersja");
    return false;
  }
  return true;
}

bool Magazyn::bezPlikowNocy() {
  if (!pliki_.zamontowane()) return false;
  const FormatPliku* f = formatPliku(rejestr_, "noce");
  if (!f) return false;
  if (!f->sciezkaV0.empty() && pliki_.istnieje(f->sciezkaV0.c_str())) return false;
  return sciezki("noce").empty();
}

Magazyn::WynikPrzywracania Magazyn::przywrocZKatalogu(const std::string& katalog) {
  WynikPrzywracania wy;
  const FormatPliku* fn = formatPliku(rejestr_, "noce");
  if (!fn || !pliki_.zamontowane()) return wy;
  bool plikZrzutu = false;
  const std::string kat = katalog + "/";
  for (auto& n : plikiZrzutu()) plikZrzutu = plikZrzutu || pliki_.istnieje((kat + n).c_str());
  if (!plikZrzutu) return wy;
  wy.proba = true;
  std::string zrodloNocy;
  if (!pliki_.czytaj((katalog + fn->sciezkaV0).c_str(), zrodloNocy)) {  // U4: T-WYM-87 weryfikacja
    wy.powod = "brak pliku nocy";
    loguj("przywracanie: niezgodna liczba nocy (0 z ?, brak pliku nocy), pliki w " + katalog +
          "/ bez zmian, bez zapisu");
    return wy;
  }
  bool pelny = false;
  for (auto& l : wierszeTekstu(zrodloNocy, pelny))
    if (!l.empty()) wy.oczekiwane++;
  if (wy.oczekiwane == 0) {  // nocy w zrzucie to niezgodność, nigdy sukces
    wy.powod = "brak nocy w pliku nocy";
    loguj("przywracanie: niezgodna liczba nocy (0 z 0, brak nocy w pliku nocy), pliki w " + katalog +
          "/ bez zmian, bez zapisu");
    return wy;
  }
  // wszystkie formaty z plikiem w zrzucie: wiersze wersji 1 w pamięci, przed jakimkolwiek zapisem
  struct Cel {
    const FormatPliku* f;
    std::map<std::string, std::vector<std::string>> pliki;
  };
  std::vector<Cel> cele;
  std::vector<std::string> datyZrzutu;  // klucze rekordów nocy zrzutu (z powtórzeniami)
  for (auto& f : rejestr_) {
    if (f.sciezkaV0.empty() || f.wersja() < 1) continue;
    Cel c{&f, {}};
    int przed = 0, pominiete = 0;
    auto sc = [&](const std::string& rok) { return sciezkaRoku(f.nazwa, rok); };
    if (!rekordyV0(pliki_, f, sc, c.pliki, przed, pominiete, katalog)) continue;
    if (pominiete)
      loguj("przywracanie " + nazwaPliku(f.sciezkaV0) + ": " + std::to_string(pominiete) + " wierszy nieczytelnych");
    if (f.nazwa == "noce") {
      wy.przywrocone = przed;
      for (auto& kv : c.pliki)
        for (auto& w : kv.second) datyZrzutu.push_back(kluczWiersza(w));
    }
    cele.push_back(c);
  }
  if (!zrodloNocy.empty() && !pelny) wy.powod = "ostatni wiersz pliku nocy urwany";
  else if (wy.przywrocone != wy.oczekiwane) wy.powod = "wiersze nieczytelne";
  // scalenie z plikami we flash (U2 wariant A): treść plików wynikowych w pamięci, bez zapisu
  struct Wynik {
    std::string sciezka, tresc;
  };
  std::vector<Wynik> wyniki;
  std::set<std::string> datyPrzed;
  for (size_t i = 0; wy.powod.empty() && i < cele.size(); i++) {
    const FormatPliku& f = *cele[i].f;
    for (auto& kv : cele[i].pliki) {
      OdczytPliku o = czytaj(f.nazwa, kv.first);
      if (o.stan == StanPliku::NOWSZY) {
        wy.powod = "plik " + nazwaPliku(kv.first) + " w wersji " + std::to_string(o.wersja);
        break;
      }
      if (f.nazwa == "noce")
        for (auto& w : o.wiersze) datyPrzed.insert(kluczWiersza(w));
      int d = 0;
      std::vector<std::string> w = scalWiersze(f.nazwa, o.wiersze, kv.second, &d);
      if (!d && o.stan == StanPliku::DOBRY) continue;  // wszystko już jest
      // plik czekający na migrację w miejscu zostaje w swojej wersji (jak scalZrodlo)
      int wz = o.stan == StanPliku::DOBRY && o.wersja >= 1 && o.wersja < f.wersja() ? o.wersja : f.wersja();
      if (wz < f.wersja())
        for (auto& x : w) x = pierwszePola(x, f.pola[(size_t)wz - 1].size());
      wyniki.push_back(Wynik{kv.first, tekstPliku(f, wz, w)});
    }
  }
  // U8: atomowo względem całego zestawu: wszystkie `.tmp`, dopiero potem zmiany nazw
  std::vector<std::string> tmp;
  for (size_t i = 0; wy.powod.empty() && i < wyniki.size(); i++) {
    std::string t = wyniki[i].sciezka + ".tmp";
    tmp.push_back(t);
    size_t n = pliki_.zapisz(t.c_str(), wyniki[i].tresc);
    if (n != wyniki[i].tresc.size()) {
      wy.powod = "zapis " + nazwaPliku(wyniki[i].sciezka);
      loguj("zapis " + nazwaPliku(t) + ": zapisano " + std::to_string(n) + " z " +
            std::to_string(wyniki[i].tresc.size()) + " B");
    }
  }
  if (!wy.powod.empty()) {
    for (auto& t : tmp) pliki_.usun(t.c_str());
    tmp.clear();
  }
  for (size_t i = 0; i < tmp.size(); i++) {
    if (pliki_.zmienNazwe(tmp[i].c_str(), wyniki[i].sciezka.c_str())) continue;
    wy.powod = "zmiana nazwy " + nazwaPliku(wyniki[i].sciezka);  // pliki już zamienione to nadzbiory poprzednich
    for (size_t k = i; k < tmp.size(); k++) pliki_.usun(tmp[k].c_str());
    break;
  }
  if (wy.powod.empty()) {  // liczba rekordów nocy zrzutu obecnych po zapisie (T-WYM-62, „przyjęte albo już obecne”)
    std::set<std::string> datyPo;
    for (auto& s : sciezki("noce")) {
      OdczytPliku o = czytaj("noce", s);
      if (kompletny(o))
        for (auto& w : o.wiersze) datyPo.insert(kluczWiersza(w));
    }
    wy.przywrocone = wy.nowych = 0;
    for (auto& d : datyZrzutu) {
      wy.przywrocone += datyPo.count(d) ? 1 : 0;
      wy.nowych += datyPrzed.count(d) ? 0 : 1;
    }
    if (wy.przywrocone != wy.oczekiwane) wy.powod = "liczba po zapisie";
  }
  if (!wy.powod.empty()) {
    loguj("przywracanie: niezgodna liczba nocy (" + std::to_string(wy.przywrocone) + " z " +
          std::to_string(wy.oczekiwane) + ", " + wy.powod + "), pliki w " + katalog + "/ bez zmian");
    return wy;
  }
  wy.ok = true;
  loguj("przywrócono " + std::to_string(wy.przywrocone) + " z " + std::to_string(wy.oczekiwane) + " nocy z " + katalog +
        "/ (nowych " + std::to_string(wy.nowych) + ", zapisanych plików " + std::to_string(wyniki.size()) + ")");
  return wy;
}

// ---------- tablica partycji (T-WYM-38, T-WYM-84; implementacja ----------

uint32_t identyfikatorUkladu(std::vector<WpisPartycji> t) {
  t.erase(std::remove_if(t.begin(), t.end(), [](const WpisPartycji& w) { return w.typ > 1; }), t.end());
  if (t.empty()) return 0;
  std::stable_sort(t.begin(), t.end(),
                   [](const WpisPartycji& x, const WpisPartycji& y) { return x.przesuniecie < y.przesuniecie; });
  std::string s;
  for (auto& w : t) {
    char b[80];
    snprintf(b, sizeof b, " %u %u 0x%lx 0x%lx\n", (unsigned)w.typ, (unsigned)w.podtyp, (unsigned long)w.przesuniecie,
             (unsigned long)w.rozmiar);
    s += w.nazwa + b;
  }
  uint32_t c = crc32((const uint8_t*)s.data(), s.size());
  return c ? c : 1u;
}

namespace {
std::string bezSpacji(const std::string& t) {
  size_t a = t.find_first_not_of(" \t\r"), b = t.find_last_not_of(" \t\r");
  return a == std::string::npos ? std::string() : t.substr(a, b - a + 1);
}
// Liczba z tablicy partycji: dziesiętna albo 0x…, z przyrostkiem K albo M; false przy czymkolwiek innym.
bool liczbaTablicy(std::string t, uint32_t& w) {
  t = bezSpacji(t);
  if (t.empty()) return false;
  unsigned long mn = 1;
  char z = t.back();
  if (z == 'K' || z == 'k') mn = 1024;
  if (z == 'M' || z == 'm') mn = 1024ul * 1024;
  if (mn > 1) t.pop_back();
  if (t.empty() || t.size() > 10) return false;
  char* e = nullptr;
  unsigned long v = strtoul(t.c_str(), &e, 0);
  if (!e || *e || t[0] == '-' || t[0] == '+') return false;
  unsigned long long x = (unsigned long long)v * mn;
  if (x > 0xFFFFFFFFull) return false;
  w = (uint32_t)x;
  return true;
}
// Typ i podtyp słowami (ESP-IDF, gen_esp32part.py) albo liczbą 0…254.
bool kodPartycji(const std::string& t, const std::vector<std::pair<const char*, uint8_t>>& nazwy, uint8_t& w) {
  for (auto& p : nazwy)
    if (t == p.first) {
      w = p.second;
      return true;
    }
  uint32_t v = 0;
  if (!liczbaTablicy(t, v) || v > 254) return false;
  w = (uint8_t)v;
  return true;
}
}  // namespace

bool tablicaZCsv(const std::string& csv, std::vector<WpisPartycji>& tablica) {
  tablica.clear();
  static const std::vector<std::pair<const char*, uint8_t>> TYPY = {{"app", 0}, {"data", 1}};
  static const std::vector<std::pair<const char*, uint8_t>> PODTYPY_APP = {{"factory", 0x00}, {"test", 0x20},
                                                                           {"ota_0", 0x10},   {"ota_1", 0x11},
                                                                           {"ota_2", 0x12},   {"ota_3", 0x13}};
  static const std::vector<std::pair<const char*, uint8_t>> PODTYPY_DATA = {
      {"ota", 0x00},      {"phy", 0x01},   {"nvs", 0x02},    {"coredump", 0x03}, {"nvs_keys", 0x04},
      {"efuse", 0x05},    {"undefined", 0x06}, {"esphttpd", 0x80}, {"fat", 0x81},      {"spiffs", 0x82},
      {"littlefs", 0x83}};
  bool pelny = false;
  for (auto& l : wierszeTekstu(csv, pelny)) {
    std::string w = bezSpacji(l);
    if (w.empty() || w[0] == '#') continue;
    std::vector<std::string> p;
    size_t a = 0;
    for (;;) {
      size_t b = w.find(',', a);
      p.push_back(bezSpacji(w.substr(a, b == std::string::npos ? std::string::npos : b - a)));
      if (b == std::string::npos) break;
      a = b + 1;
    }
    if (p.size() < 5 || p[0].empty() || p[0].size() > 16) return false;
    WpisPartycji x;
    x.nazwa = p[0];
    if (!kodPartycji(p[1], TYPY, x.typ)) return false;
    if (!kodPartycji(p[2], x.typ == 0 ? PODTYPY_APP : PODTYPY_DATA, x.podtyp)) return false;
    if (!liczbaTablicy(p[3], x.przesuniecie) || !liczbaTablicy(p[4], x.rozmiar) || x.rozmiar == 0) return false;
    tablica.push_back(x);
  }
  return !tablica.empty();
}

DecyzjaPartycji decyzjaPartycji(const StanPartycji& s) {
  if (!s.ukladZgodny) return DecyzjaPartycji::STARY_UKLAD;
  if (s.zamontowana) return DecyzjaPartycji::ZAMONTOWANA;
  if (!s.znacznikNvs.empty() && s.znacznikNvs == s.znacznikBiezacy) return DecyzjaPartycji::ZNACZNIK;
  if (!s.blokiCzytelne || s.bloki.size() < 2 * BLOK_PARTYCJI) return DecyzjaPartycji::NIECZYTELNA;
  std::string b = s.bloki.substr(0, 2 * BLOK_PARTYCJI);
  if (b.find("littlefs") != std::string::npos) return DecyzjaPartycji::SUPERBLOK;
  bool skasowana = b.find_first_not_of('\xff') == std::string::npos;
  if (s.ukladNvs != ukladWkompilowany() || skasowana) return DecyzjaPartycji::FORMATUJ;
  return DecyzjaPartycji::NIE_NOWA;
}

const char* opisDecyzjiPartycji(DecyzjaPartycji d) {
  switch (d) {
    case DecyzjaPartycji::ZAMONTOWANA: return "zamontowana";
    case DecyzjaPartycji::STARY_UKLAD: return "tablica partycji panelu inna niż w firmware";
    case DecyzjaPartycji::FORMATUJ: return "partycja nowa";
    case DecyzjaPartycji::ZNACZNIK: return "znacznik fs_init tej partycji w NVS";
    case DecyzjaPartycji::NIECZYTELNA: return "bloki partycji nieczytelne";
    case DecyzjaPartycji::SUPERBLOK: return "superblok LittleFS w obszarze";
    case DecyzjaPartycji::NIE_NOWA: return "układ znany z NVS, obszar nie jest skasowany";
  }
  return "?";
}

// ---------- komenda `W` (R8 karty) ----------

const std::vector<std::string>& plikiZrzutu() {
  static const std::vector<std::string> p = {"noce.csv",      "czynniki.csv",      "humor.csv",
                                             "sypialnia.csv", "sypialnia-co2.csv", "bateria.csv"};
  return p;
}

bool parsujKomendeW(const std::string& w, KomendaW& k) {
  k = KomendaW();
  if (w.size() < 2 || w[0] != 'W' || w[1] != ' ') return false;
  std::vector<std::string> p;
  size_t a = 2;
  while (a < w.size() && p.size() < 4) {
    size_t b = w.find(' ', a);
    if (b == std::string::npos) b = w.size();
    if (b > a) p.push_back(w.substr(a, b - a));
    a = b + 1;
  }
  k.nazwa = p.empty() ? "?" : p[0].substr(0, 64);
  long r = 0;
  if (p.size() >= 2 && liczba(p[1], (long)MAKS_PLIKU_W, r)) {
    k.rozmiarOk = true;
    k.rozmiar = (size_t)r;
  }
  if (p.size() >= 3 && p[2].size() == 64 && p[2].find_first_not_of("0123456789abcdef") == std::string::npos)
    k.skrot = p[2];
  return true;
}

std::string sprawdzPlikW(const KomendaW& k, const std::string& tresc) {
  const std::vector<std::string>& d = plikiZrzutu();
  if (std::find(d.begin(), d.end(), k.nazwa) == d.end()) return "nazwa";  // także `..` i ukośnik: spoza listy
  if (k.skrot.empty() || sha256Hex(tresc) != k.skrot) return "skrot";
  return "";
}

// ---------- obrazki dnia komendą `W` ----------

bool nazwaObrazkaW(const std::string& nazwa) {
  const std::string pre = PREFIKS_OBRAZKOW_W;
  if (nazwa.compare(0, pre.size(), pre) != 0) return false;
  std::string n = nazwa.substr(pre.size());
  if (n == "obrazki.json") return true;
  if (n.size() < 5 || n.compare(n.size() - 4, 4, ".pbm") != 0) return false;
  std::string rdzen = n.substr(0, n.size() - 4);
  return !rdzen.empty() && rdzen.size() <= MAKS_NAZWY_OBRAZKA_W &&
         rdzen.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos;
}

std::string sprawdzObrazekW(const KomendaW& k, const std::string& tresc) {
  if (!nazwaObrazkaW(k.nazwa)) return "nazwa";
  if (!k.rozmiarOk || k.rozmiar > MAKS_OBRAZKA_W || tresc.size() != k.rozmiar) return "rozmiar";
  if (k.skrot.empty() || sha256Hex(tresc) != k.skrot) return "skrot";
  if (k.nazwa.compare(k.nazwa.size() - 4, 4, ".pbm") == 0) {
    ObrazPbm o;
    std::string powod;
    if (!parsujPbm(tresc, o, powod)) return "tresc";
  } else {
    std::vector<PozycjaManifestu> m;
    if (!parsujManifest(tresc, m)) return "tresc";
  }
  return "";
}

std::string sprawdzKonfiguracjeW(const KomendaW& k, const std::string& tresc) {
  if (k.nazwa != NAZWA_KONFIGURACJI_W) return "nazwa";
  if (!k.rozmiarOk || k.rozmiar > MAKS_KONFIGURACJI_W || tresc.size() != k.rozmiar) return "rozmiar";
  if (k.skrot.empty() || sha256Hex(tresc) != k.skrot) return "skrot";
  if (tresc.find('\0') != std::string::npos) return "tresc";
  // wiersz równy znacznikowi zrzutu `Z` (także z `\r` na końcu) psułby sekcję pliku w narzędziach czytających wiersze
  size_t p = 0;
  while (p <= tresc.size()) {
    size_t e = tresc.find('\n', p);
    if (e == std::string::npos) e = tresc.size();
    std::string w = tresc.substr(p, e - p);
    if (!w.empty() && w.back() == '\r') w.pop_back();
    if (w == "PLIK_KONIEC" || w == "PLIKI_KONIEC" || w == "PLIKI_START" || w.compare(0, 5, "PLIK ") == 0)
      return "znacznik";
    p = e + 1;
  }
  return "";
}

bool Magazyn::tylkoDoOdczytu(const std::string& sciezka) const {
  for (auto& s : tylkoOdczyt_)
    if (s == sciezka) return true;
  return false;
}

// ---------- NVS z zapisem tylko przy zmianie (T-WYM-58) ----------
// Klucz jest i ma wartość `v` ⇔ odczyt z domyślną `v` i z domyślną różną od `v` daje `v` (brak klucza = różnica).
void NvsTylkoZmiany::tylkoDoOdczytu(const char* p) {
  if (!zablokowana(p)) zablokowane_.emplace_back(p);
}
bool NvsTylkoZmiany::zablokowana(const char* p) const {
  return std::find(zablokowane_.begin(), zablokowane_.end(), std::string(p)) != zablokowane_.end();
}
std::string NvsTylkoZmiany::tekst(const char* p, const char* k, const std::string& d) { return n_.tekst(p, k, d); }
bool NvsTylkoZmiany::ustawTekst(const char* p, const char* k, const std::string& v) {
  if (zablokowana(p)) return false;
  if (n_.tekst(p, k, v) == v && n_.tekst(p, k, v + "#") == v) return true;
  return n_.ustawTekst(p, k, v);
}
uint32_t NvsTylkoZmiany::liczbaU(const char* p, const char* k, uint32_t d) { return n_.liczbaU(p, k, d); }
void NvsTylkoZmiany::ustawLiczbeU(const char* p, const char* k, uint32_t v) {
  if (zablokowana(p)) return;
  if (n_.liczbaU(p, k, v) == v && n_.liczbaU(p, k, v ^ 1u) == v) return;
  n_.ustawLiczbeU(p, k, v);
}
int32_t NvsTylkoZmiany::liczba(const char* p, const char* k, int32_t d) { return n_.liczba(p, k, d); }
void NvsTylkoZmiany::ustawLiczbe(const char* p, const char* k, int32_t v) {
  if (zablokowana(p)) return;
  if (n_.liczba(p, k, v) == v && n_.liczba(p, k, v ^ 1) == v) return;
  n_.ustawLiczbe(p, k, v);
}
bool NvsTylkoZmiany::flaga(const char* p, const char* k, bool d) { return n_.flaga(p, k, d); }
void NvsTylkoZmiany::ustawFlage(const char* p, const char* k, bool v) {
  if (zablokowana(p)) return;
  if (n_.flaga(p, k, v) == v && n_.flaga(p, k, !v) == v) return;
  n_.ustawFlage(p, k, v);
}
size_t NvsTylkoZmiany::bajty(const char* p, const char* k, void* c, size_t r) { return n_.bajty(p, k, c, r); }
void NvsTylkoZmiany::ustawBajty(const char* p, const char* k, const void* z, size_t r) {
  if (zablokowana(p)) return;
  std::string b(r, '\0');
  if (r && n_.bajty(p, k, &b[0], r) == r && memcmp(b.data(), z, r) == 0) return;
  n_.ustawBajty(p, k, z, r);
}
void NvsTylkoZmiany::usun(const char* p, const char* k) {
  if (zablokowana(p)) return;
  n_.usun(p, k);
}

// ---------- układ partycji (T-WYM-38, T-WYM-84) ----------
// Na panelu identyfikator tablicy z pliku `board_build.partitions` liczy skrypt budowy (firmware/prototyp/
// uklad_partycji.py, -DUKLAD_PARTYCJI=…). Na komputerze ta sama tablica 8.2 (firmware/partycje-e1001.csv) wpisana
// niżej; test testT_WYM_38_identyfikatorUkladu pilnuje, że jest taka sama jak plik w repozytorium.
#if defined(ARDUINO) && !defined(UKLAD_PARTYCJI)
#error "brak UKLAD_PARTYCJI: skrypt budowy uklad_partycji.py (extra_scripts w platformio.ini) nie zadziałał"
#endif
#ifndef UKLAD_PARTYCJI
static const char* const TABLICA_8_2 =
    "nvs,      data, nvs,      0x9000,   0x5000\n"
    "otadata,  data, ota,      0xE000,   0x2000\n"
    "app0,     app,  ota_0,    0x10000,  0x400000\n"
    "app1,     app,  ota_1,    0x410000, 0x400000\n"
    "coredump, data, coredump, 0x810000, 0x10000\n"
    "spiffs,   data, spiffs,   0x820000, 0x17E0000\n";
#endif
uint32_t ukladWkompilowany() {
#ifdef UKLAD_PARTYCJI
  return (uint32_t)(UKLAD_PARTYCJI);
#else
  static const uint32_t u = []() {
    std::vector<WpisPartycji> t;
    return tablicaZCsv(TABLICA_8_2, t) ? identyfikatorUkladu(t) : 1u;
  }();
  return u;
#endif
}

std::string znacznikPartycji(const OpisPartycji& p) {
  char b[32];
  snprintf(b, sizeof b, "%lx:%lx", (unsigned long)p.przesuniecie, (unsigned long)p.rozmiar);
  return b;
}

// ---------- SHA-256 (FIPS 180-4) ----------
namespace {
const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
    0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
    0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2};
inline uint32_t obrot(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
void blokSha(uint32_t h[8], const uint8_t* b) {
  uint32_t w[64];
  for (size_t i = 0; i < 16; i++) {
    const uint8_t* q = b + 4 * i;
    w[i] = (uint32_t)q[0] << 24 | (uint32_t)q[1] << 16 | (uint32_t)q[2] << 8 | (uint32_t)q[3];
  }
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = obrot(w[i - 15], 7) ^ obrot(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = obrot(w[i - 2], 17) ^ obrot(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
  for (int i = 0; i < 64; i++) {
    uint32_t t1 = hh + (obrot(e, 6) ^ obrot(e, 11) ^ obrot(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
    uint32_t t2 = (obrot(a, 2) ^ obrot(a, 13) ^ obrot(a, 22)) + ((a & bb) ^ (a & c) ^ (bb & c));
    hh = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = bb;
    bb = a;
    a = t1 + t2;
  }
  h[0] += a;
  h[1] += bb;
  h[2] += c;
  h[3] += d;
  h[4] += e;
  h[5] += f;
  h[6] += g;
  h[7] += hh;
}
}  // namespace

std::string sha256Hex(const std::string& dane) {
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const uint8_t* p = (const uint8_t*)dane.data();
  size_t n = dane.size(), i = 0;
  for (; i + 64 <= n; i += 64) blokSha(h, p + i);
  uint8_t ost[128] = {0};
  size_t r = n - i;
  if (r) memcpy(ost, p + i, r);
  ost[r] = 0x80;
  size_t dl = r < 56 ? 64 : 128;
  uint64_t bity = (uint64_t)n * 8u;
  for (int k = 0; k < 8; k++) ost[dl - 1 - (size_t)k] = (uint8_t)(bity >> (8 * k));
  blokSha(h, ost);
  if (dl == 128) blokSha(h, ost + 64);
  static const char* const HEX = "0123456789abcdef";
  std::string s;
  s.reserve(64);
  for (uint32_t x : h)
    for (int k = 28; k >= 0; k -= 4) s += HEX[(x >> k) & 0xF];
  return s;
}
