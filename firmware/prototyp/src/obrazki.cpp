// SPDX-License-Identifier: GPL-3.0-or-later
// Obrazek dnia: opis w src/obrazki.h. Bez wyjątków i RTTI.
#include "obrazki.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "wspolne.h"

const char* const KATALOG_OBRAZKOW = "/obrazki";
const char* const MANIFEST_OBRAZKOW = "/obrazki/obrazki.json";
const char* const DZIENNIK_OBRAZKOW = "/obrazki.csv";

namespace {

// ---------- mały czytnik JSON (obiekty, tablice, teksty z ucieczkami, liczby, true/false/null) ----------
struct Czytnik {
  const std::string& s;
  size_t p = 0;
  int glebokosc = 0;
  explicit Czytnik(const std::string& t) : s(t) {}
  void spacje() {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\r' || s[p] == '\t')) p++;
  }
  bool znak(char c) {
    spacje();
    if (p < s.size() && s[p] == c) { p++; return true; }
    return false;
  }
  bool tekst(std::string& w) {
    spacje();
    if (p >= s.size() || s[p] != '"') return false;
    p++;
    w.clear();
    while (p < s.size()) {
      char c = s[p++];
      if (c == '"') return true;
      if (c == '\\') {
        if (p >= s.size()) return false;
        char e = s[p++];
        if (e == 'u') {  // \uXXXX → UTF-8 (bez par zastępczych: znaki spoza BMP nie są potrzebne)
          if (p + 4 > s.size()) return false;
          unsigned cp = (unsigned)strtoul(s.substr(p, 4).c_str(), nullptr, 16);
          p += 4;
          if (cp < 0x80) w += (char)cp;
          else if (cp < 0x800) { w += (char)(0xC0 | (cp >> 6)); w += (char)(0x80 | (cp & 0x3F)); }
          else { w += (char)(0xE0 | (cp >> 12)); w += (char)(0x80 | ((cp >> 6) & 0x3F)); w += (char)(0x80 | (cp & 0x3F)); }
        } else {
          w += e == 'n' ? '\n' : e == 't' ? '\t' : e;
        }
        continue;
      }
      w += c;
    }
    return false;
  }
  bool pomin() {  // dowolna wartość
    spacje();
    if (p >= s.size() || ++glebokosc > 20) return false;
    bool ok = true;
    char c = s[p];
    if (c == '"') { std::string t; ok = tekst(t); }
    else if (c == '{') {
      p++;
      if (!znak('}')) {
        do {
          std::string k;
          if (!tekst(k) || !znak(':') || !pomin()) { ok = false; break; }
        } while (znak(','));
        if (ok && !znak('}')) ok = false;
      }
    } else if (c == '[') {
      p++;
      if (!znak(']')) {
        do {
          if (!pomin()) { ok = false; break; }
        } while (znak(','));
        if (ok && !znak(']')) ok = false;
      }
    } else {
      size_t q = p;
      while (q < s.size() && s[q] != ',' && s[q] != '}' && s[q] != ']' && s[q] != ' ' && s[q] != '\n' && s[q] != '\r') q++;
      ok = q > p;
      p = q;
    }
    glebokosc--;
    return ok;
  }
  // tablica tekstów; inne elementy pomijane
  bool teksty(std::vector<std::string>& w) {
    w.clear();
    if (!znak('[')) return pomin();
    if (znak(']')) return true;
    do {
      spacje();
      if (p < s.size() && s[p] == '"') {
        std::string t;
        if (!tekst(t)) return false;
        w.push_back(t);
      } else if (!pomin()) {
        return false;
      }
    } while (znak(','));
    return znak(']');
  }
};

bool pozycja(Czytnik& c, PozycjaManifestu& poz) {
  if (!c.znak('{')) return c.pomin() && false;
  if (c.znak('}')) return true;
  do {
    std::string k;
    if (!c.tekst(k) || !c.znak(':')) return false;
    bool ok = true;
    if (k == "plik") {
      c.spacje();
      if (c.p < c.s.size() && c.s[c.p] == '"') ok = c.tekst(poz.plik);
      else ok = c.pomin();
    } else if (k == "dni") ok = c.teksty(poz.dni);
    else if (k == "pogoda") ok = c.teksty(poz.pogoda);
    else if (k == "sezon") ok = c.teksty(poz.sezon);
    else if (k == "inne") ok = c.teksty(poz.inne);
    else ok = c.pomin();  // pozostałe pola manifestu (tekst, wulgarne, …) ignorowane
    if (!ok) return false;
  } while (c.znak(','));
  return c.znak('}');
}

const char* const DNI_TAGU[7] = {"nd", "pn", "wt", "sr", "cz", "pt", "so"};
bool znany(const std::string& t, std::initializer_list<const char*> l) {
  for (auto x : l)
    if (t == x) return true;
  return false;
}
bool znanyTag(const std::string& rodzaj, const std::string& v) {
  if (rodzaj == "dni") return znany(v, {"pn", "wt", "sr", "cz", "pt", "so", "nd"});
  if (rodzaj == "sezon") return znany(v, {"zima", "listopad"});
  if (rodzaj == "inne") return znany(v, {"poczatek_miesiaca"});
  if (rodzaj == "pogoda") return znany(v, {"deszcz", "zimno", "brzydka", "zmienna"});
  return false;
}

std::string bezRozszerzenia(const std::string& s) {
  size_t k = s.rfind('.');
  size_t u = s.rfind('/');
  if (k == std::string::npos || (u != std::string::npos && k < u)) return s;
  return s.substr(0, k);
}
std::string nazwaZeSciezki(const std::string& s) {
  size_t u = s.rfind('/');
  return u == std::string::npos ? s : s.substr(u + 1);
}
bool konczySie(const std::string& s, const char* k) {
  std::string e = k;
  return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0;
}

uint32_t mieszaj(uint32_t x) {  // liczba z numeru dnia (R-KON-59), stały algorytm bez generatora losowego
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

}  // namespace

bool parsujManifest(const std::string& json, std::vector<PozycjaManifestu>& wynik) {
  wynik.clear();
  std::string j = json;
  if (j.size() >= 3 && (uint8_t)j[0] == 0xEF && (uint8_t)j[1] == 0xBB && (uint8_t)j[2] == 0xBF) j = j.substr(3);
  Czytnik c(j);
  if (!c.znak('{')) return false;
  bool lista = false;
  if (!c.znak('}')) {
    do {
      std::string k;
      if (!c.tekst(k) || !c.znak(':')) return false;
      if (k == "obrazki") {
        if (!c.znak('[')) return false;
        lista = true;
        if (!c.znak(']')) {
          do {
            PozycjaManifestu p;
            if (!pozycja(c, p)) return false;
            if (!p.plik.empty()) wynik.push_back(p);
          } while (c.znak(','));
          if (!c.znak(']')) return false;
        }
      } else if (!c.pomin()) {
        return false;
      }
    } while (c.znak(','));
    if (!c.znak('}')) return false;
  }
  if (!lista) wynik.clear();
  return lista;
}

std::vector<std::string> tagiDzis(const KontekstObrazka& k) {
  std::vector<std::string> t;
  int dt = dzienTygodnia(k.dzis);
  if (dt < 0) return t;
  t.push_back(DNI_TAGU[dt]);
  int m = liczbaCalk(k.dzis.substr(5, 2).c_str()), d = liczbaCalk(k.dzis.substr(8, 2).c_str());
  if (m == 12 || m == 1 || m == 2) t.push_back("zima");
  if (m == 11) t.push_back("listopad");
  if (d == 1) t.push_back("poczatek_miesiaca");
  if (k.prognoza && k.prognoza->jest()) {  // pogoda z horyzontu reguły wietrzenia (5.8.12)
    int n = 0, zOpadem = 0, brzydkich = 0;
    float maxT = NAN;
    for (int h = std::max(0, k.godzina); h < WIETRZ_HORYZONT_DO; h++) {
      const GodzinaPrognozy* g = k.prognoza->godzina(k.dzis, h);
      if (!g) continue;
      n++;
      bool opad = !std::isnan(g->opad) && g->opad >= JUTRO_DESZCZ_OPAD;
      if (opad) zOpadem++;
      if (opad || (!std::isnan(g->chmury) && g->chmury >= 90)) brzydkich++;
      if (!std::isnan(g->temp) && (std::isnan(maxT) || g->temp > maxT)) maxT = g->temp;
    }
    if (n > 0) {
      if (zOpadem > 0) t.push_back("deszcz");
      if (!std::isnan(maxT) && maxT <= 5) t.push_back("zimno");
      if (brzydkich * 2 > n) t.push_back("brzydka");
      if (zOpadem > 0 && zOpadem < n) t.push_back("zmienna");
    }
  }
  return t;
}

bool maTagKontekstu(const PozycjaManifestu& p) {
  for (auto& v : p.dni) if (znanyTag("dni", v)) return true;
  for (auto& v : p.pogoda) if (znanyTag("pogoda", v)) return true;
  for (auto& v : p.sezon) if (znanyTag("sezon", v)) return true;
  for (auto& v : p.inne) if (znanyTag("inne", v)) return true;
  return false;
}

bool pasujeDzis(const PozycjaManifestu& p, const std::vector<std::string>& tagi) {
  auto jest = [&](const std::string& rodzaj, const std::string& v) {
    return znanyTag(rodzaj, v) && std::find(tagi.begin(), tagi.end(), v) != tagi.end();
  };
  for (auto& v : p.dni) if (jest("dni", v)) return true;
  for (auto& v : p.pogoda) if (jest("pogoda", v)) return true;
  for (auto& v : p.sezon) if (jest("sezon", v)) return true;
  for (auto& v : p.inne) if (jest("inne", v)) return true;
  return false;
}

bool parsujWpisObrazka(const std::string& l, WpisObrazka& w) {
  size_t a = l.find(';');
  if (a == std::string::npos) return false;
  size_t b = l.find(';', a + 1);
  if (b == std::string::npos) return false;
  std::string d = l.substr(0, a), pl = l.substr(a + 1, b - a - 1), pu = l.substr(b + 1);
  if (!dataOk(d) || pl.empty() || (pu != "k" && pu != "n")) return false;
  w.data = d;
  w.plik = pl;
  w.pula = pu[0];
  return true;
}
std::string zapiszWpisObrazka(const WpisObrazka& w) { return w.data + ";" + w.plik + ";" + std::string(1, w.pula); }

WyborObrazka wybierzObrazek(const std::vector<std::string>& pliki, const std::vector<PozycjaManifestu>* manifest,
                            const std::vector<WpisObrazka>& dziennik, const KontekstObrazka& k,
                            const std::function<bool(const std::string&)>& czytelny) {
  WyborObrazka r;
  long n = nrDnia(k.dzis);
  if (n < 0) return r;
  // obrazki .pbm z katalogu po nazwie pliku (bez katalogu)
  std::vector<std::string> pbm;
  for (auto& p : pliki)
    if (konczySie(p, ".pbm")) pbm.push_back(p);
  auto sciezka = [&](const std::string& plikManifestu) -> std::string {
    std::string rdzen = bezRozszerzenia(nazwaZeSciezki(plikManifestu));
    for (auto& p : pbm)
      if (bezRozszerzenia(nazwaZeSciezki(p)) == rdzen) return p;
    return "";
  };
  // R-KON-61: obrazek wybrany dziś zostaje do końca dnia (także po restarcie), dopóki plik jest
  for (auto& w : dziennik)
    if (w.data == k.dzis) {
      std::string p = sciezka(w.plik);
      if (!p.empty()) {
        r.plik = p;
        r.pula = w.pula;
        return r;
      }
    }
  std::vector<std::string> kontekst, neutralna;
  if (!manifest) {
    r.log.push_back("obrazki: bez manifestu");
    neutralna = pbm;
  } else {
    std::vector<std::string> tagi = tagiDzis(k);
    bool zPlikiem = false;
    for (auto& poz : *manifest) {
      std::string p = sciezka(poz.plik);
      if (p.empty()) continue;  // pozycja manifestu bez pliku pominięta (R-KON-62)
      zPlikiem = true;
      if (!maTagKontekstu(poz)) neutralna.push_back(p);
      else if (pasujeDzis(poz, tagi)) kontekst.push_back(p);
    }
    // manifest pusty albo bez pozycji dla plików z katalogu: bez obrazka, ale ze śladem w logu
    if (!zPlikiem && !pbm.empty()) r.log.push_back("obrazki: manifest bez pozycji z plikiem");
  }
  int licznik = 0;  // dni S-K-38 przed dziś
  for (auto& w : dziennik)
    if (w.data < k.dzis) licznik++;
  auto ostatnio = [&](const std::string& p) {
    std::string o;
    std::string nazwa = bezRozszerzenia(nazwaZeSciezki(p));
    for (auto& w : dziennik)
      if (bezRozszerzenia(nazwaZeSciezki(w.plik)) == nazwa && w.data > o) o = w.data;
    return o;  // "" = nigdy
  };
  auto zPuli = [&](std::vector<std::string> pula, char id) -> bool {
    while (!pula.empty()) {
      std::string naj = "~";
      for (auto& p : pula) naj = std::min(naj, ostatnio(p));
      std::vector<std::string> kand;
      for (auto& p : pula)
        if (ostatnio(p) == naj) kand.push_back(p);
      std::sort(kand.begin(), kand.end());
      std::string wyb = kand[mieszaj((uint32_t)n) % kand.size()];
      if (!czytelny || czytelny(wyb)) {
        r.plik = wyb;
        r.pula = id;
        r.nowy = true;
        return true;
      }
      r.log.push_back("obrazki: pominięty " + nazwaZeSciezki(wyb) + ": plik nieczytelny albo większy niż 800×434");
      pula.erase(std::remove(pula.begin(), pula.end(), wyb), pula.end());
    }
    return false;
  };
  bool najpierwKontekst = !kontekst.empty() && licznik % 2 == 0;
  if (najpierwKontekst) {
    if (!zPuli(kontekst, 'k')) zPuli(neutralna, 'n');
  } else {
    if (!zPuli(neutralna, 'n')) zPuli(kontekst, 'k');
  }
  return r;
}

bool parsujPbm(const std::string& d, ObrazPbm& o, std::string& powod) {
  o = ObrazPbm();
  if (d.size() < 3 || d[0] != 'P' || d[1] != '4') { powod = "nie PBM P4"; return false; }
  size_t p = 2;
  int liczby[2] = {0, 0};
  for (int i = 0; i < 2; i++) {
    while (p < d.size()) {  // spacje i komentarze `#`
      if (d[p] == '#') { while (p < d.size() && d[p] != '\n') p++; }
      else if (d[p] == ' ' || d[p] == '\n' || d[p] == '\r' || d[p] == '\t') p++;
      else break;
    }
    size_t q = p;
    long v = 0;
    while (q < d.size() && d[q] >= '0' && d[q] <= '9' && q - p < 6) v = v * 10 + (d[q++] - '0');
    if (q == p) { powod = "zły nagłówek"; return false; }
    liczby[i] = (int)v;
    p = q;
  }
  if (p >= d.size()) { powod = "brak danych"; return false; }
  p++;  // jeden biały znak po wysokości
  int w = liczby[0], h = liczby[1];
  if (w <= 0 || h <= 0 || w > OBRAZEK_MAX_W || h > OBRAZEK_MAX_H) { powod = "rozmiar " + std::to_string(w) + "×" + std::to_string(h); return false; }
  size_t bajtow = (size_t)(w + 7) / 8 * h;
  if (d.size() - p < bajtow) { powod = "za krótkie dane"; return false; }
  o.w = w;
  o.h = h;
  o.bity.assign(d.begin() + (long)p, d.begin() + (long)(p + bajtow));
  return true;
}
