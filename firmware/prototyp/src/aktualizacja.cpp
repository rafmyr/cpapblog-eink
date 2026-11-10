// SPDX-License-Identifier: GPL-3.0-or-later
#include "aktualizacja.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "nawigacja.h"

namespace aktualizacja {

// ---------- wersje ----------
namespace {
bool cyfry(const std::string& t) {
  if (t.empty()) return false;
  for (char c : t)
    if (c < '0' || c > '9') return false;
  return true;
}
bool liczbaWersji(const std::string& t, long& v) {
  if (!cyfry(t) || t.size() > 6 || (t.size() > 1 && t[0] == '0')) return false;
  v = std::strtol(t.c_str(), nullptr, 10);
  return true;
}
bool znakIdentyfikatora(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
}
std::vector<std::string> podziel(const std::string& t, char z) {
  std::vector<std::string> w;
  size_t a = 0;
  for (;;) {
    size_t b = t.find(z, a);
    w.push_back(t.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  return w;
}
}  // namespace

std::string bezBudowy(const std::string& t) {
  std::string w = t;
  if (!w.empty() && (w[0] == 'v' || w[0] == 'V')) w.erase(0, 1);
  size_t p = w.find('+');
  return p == std::string::npos ? w : w.substr(0, p);
}

bool parsujWersje(const std::string& t0, Wersja& w) {
  if (t0.empty() || t0.size() > 64) return false;
  std::string t = t0;
  if (t[0] == 'v' || t[0] == 'V') t.erase(0, 1);
  size_t plus = t.find('+');
  if (plus != std::string::npos) {
    std::string b = t.substr(plus + 1);
    if (b.empty()) return false;
    for (auto& id : podziel(b, '.')) {
      if (id.empty()) return false;
      for (char c : id)
        if (!znakIdentyfikatora(c)) return false;
    }
    t.erase(plus);
  }
  std::string rdzen = t, przed;
  size_t minus = t.find('-');
  if (minus != std::string::npos) {
    rdzen = t.substr(0, minus);
    przed = t.substr(minus + 1);
    if (przed.empty()) return false;
  }
  std::vector<std::string> c = podziel(rdzen, '.');
  if (c.size() != 3) return false;
  Wersja n;
  if (!liczbaWersji(c[0], n.glowna) || !liczbaWersji(c[1], n.poboczna) || !liczbaWersji(c[2], n.poprawka)) return false;
  if (minus != std::string::npos) {
    for (auto& id : podziel(przed, '.')) {
      if (id.empty()) return false;
      for (char ch : id)
        if (!znakIdentyfikatora(ch)) return false;
      if (cyfry(id) && id.size() > 1 && id[0] == '0') return false;
      if (cyfry(id) && id.size() > 6) return false;
      n.przed.push_back(id);
    }
  }
  w = n;
  return true;
}

int porownaj(const Wersja& a, const Wersja& b) {
  const long x[3] = {a.glowna, a.poboczna, a.poprawka}, y[3] = {b.glowna, b.poboczna, b.poprawka};
  for (int i = 0; i < 3; i++)
    if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
  if (a.przed.empty() || b.przed.empty()) {
    if (a.przed.empty() && b.przed.empty()) return 0;
    return a.przed.empty() ? 1 : -1;  // wydanie wyżej od wersji przedpremierowej
  }
  size_t n = a.przed.size() < b.przed.size() ? a.przed.size() : b.przed.size();
  for (size_t i = 0; i < n; i++) {
    const std::string &p = a.przed[i], &q = b.przed[i];
    bool lp = cyfry(p), lq = cyfry(q);
    if (lp && lq) {
      long u = std::strtol(p.c_str(), nullptr, 10), v = std::strtol(q.c_str(), nullptr, 10);
      if (u != v) return u < v ? -1 : 1;
    } else if (lp != lq) {
      return lp ? -1 : 1;  // liczba niżej od tekstu
    } else if (p != q) {
      return p < q ? -1 : 1;
    }
  }
  if (a.przed.size() == b.przed.size()) return 0;
  return a.przed.size() < b.przed.size() ? -1 : 1;
}

bool nowsza(const std::string& kandydat, const std::string& wlasna) {
  Wersja a, b;
  if (!parsujWersje(kandydat, a) || !parsujWersje(wlasna, b)) return false;
  return porownaj(a, b) > 0;
}

// ---------- JSON ----------
const Json* Json::pole(const char* nazwa) const {
  if (typ != Typ::OBIEKT) return nullptr;
  for (auto& p : pola)
    if (p.first == nazwa) return &p.second;
  return nullptr;
}

namespace {
struct Parser {
  const std::string& t;
  size_t i = 0;
  explicit Parser(const std::string& s) : t(s) {}
  void spacje() {
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) i++;
  }
  bool slowo(const char* s) {
    size_t n = std::strlen(s);
    if (t.compare(i, n, s) != 0) return false;
    i += n;
    return true;
  }
  static void utf8(std::string& o, uint32_t c) {
    if (c < 0x80) {
      o += (char)c;
    } else if (c < 0x800) {
      o += (char)(0xC0 | (c >> 6));
      o += (char)(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
      o += (char)(0xE0 | (c >> 12));
      o += (char)(0x80 | ((c >> 6) & 0x3F));
      o += (char)(0x80 | (c & 0x3F));
    } else {
      o += (char)(0xF0 | (c >> 18));
      o += (char)(0x80 | ((c >> 12) & 0x3F));
      o += (char)(0x80 | ((c >> 6) & 0x3F));
      o += (char)(0x80 | (c & 0x3F));
    }
  }
  bool hex4(uint32_t& v) {
    if (i + 4 > t.size()) return false;
    v = 0;
    for (int k = 0; k < 4; k++) {
      char c = t[i++];
      uint32_t d = c >= '0' && c <= '9' ? (uint32_t)(c - '0')
                   : c >= 'a' && c <= 'f' ? (uint32_t)(c - 'a' + 10)
                   : c >= 'A' && c <= 'F' ? (uint32_t)(c - 'A' + 10)
                                          : 16u;
      if (d > 15) return false;
      v = v * 16 + d;
    }
    return true;
  }
  bool tekst(std::string& o) {
    if (i >= t.size() || t[i] != '"') return false;
    i++;
    while (i < t.size()) {
      unsigned char c = (unsigned char)t[i++];
      if (c == '"') return true;
      if (c < 0x20) return false;
      if (c != '\\') {
        o += (char)c;
        continue;
      }
      if (i >= t.size()) return false;
      char e = t[i++];
      switch (e) {
        case '"': o += '"'; break;
        case '\\': o += '\\'; break;
        case '/': o += '/'; break;
        case 'b': o += '\b'; break;
        case 'f': o += '\f'; break;
        case 'n': o += '\n'; break;
        case 'r': o += '\r'; break;
        case 't': o += '\t'; break;
        case 'u': {
          uint32_t u = 0;
          if (!hex4(u)) return false;
          if (u >= 0xD800 && u <= 0xDBFF) {  // para zastępcza
            uint32_t l = 0;
            if (!slowo("\\u") || !hex4(l) || l < 0xDC00 || l > 0xDFFF) return false;
            u = 0x10000 + ((u - 0xD800) << 10) + (l - 0xDC00);
          } else if (u >= 0xDC00 && u <= 0xDFFF) {
            return false;
          }
          utf8(o, u);
          break;
        }
        default: return false;
      }
    }
    return false;
  }
  bool liczba(std::string& o) {
    size_t a = i;
    if (i < t.size() && t[i] == '-') i++;
    if (i >= t.size()) return false;
    if (t[i] == '0') {
      i++;
    } else if (t[i] >= '1' && t[i] <= '9') {
      while (i < t.size() && t[i] >= '0' && t[i] <= '9') i++;
    } else {
      return false;
    }
    if (i < t.size() && t[i] == '.') {
      i++;
      size_t b = i;
      while (i < t.size() && t[i] >= '0' && t[i] <= '9') i++;
      if (i == b) return false;
    }
    if (i < t.size() && (t[i] == 'e' || t[i] == 'E')) {
      i++;
      if (i < t.size() && (t[i] == '+' || t[i] == '-')) i++;
      size_t b = i;
      while (i < t.size() && t[i] >= '0' && t[i] <= '9') i++;
      if (i == b) return false;
    }
    o = t.substr(a, i - a);
    return true;
  }
  bool wartosc(Json& j, size_t gl) {
    if (gl > JSON_MAKS_GLEBOKOSC) return false;
    spacje();
    if (i >= t.size()) return false;
    char c = t[i];
    if (c == '{') {
      i++;
      j.typ = Json::Typ::OBIEKT;
      spacje();
      if (i < t.size() && t[i] == '}') {
        i++;
        return true;
      }
      for (;;) {
        spacje();
        std::string k;
        if (!tekst(k)) return false;
        spacje();
        if (i >= t.size() || t[i] != ':') return false;
        i++;
        j.pola.emplace_back(k, Json());
        if (!wartosc(j.pola.back().second, gl + 1)) return false;
        spacje();
        if (i >= t.size()) return false;
        if (t[i] == ',') {
          i++;
          continue;
        }
        if (t[i] == '}') {
          i++;
          return true;
        }
        return false;
      }
    }
    if (c == '[') {
      i++;
      j.typ = Json::Typ::TABLICA;
      spacje();
      if (i < t.size() && t[i] == ']') {
        i++;
        return true;
      }
      for (;;) {
        j.elementy.emplace_back();
        if (!wartosc(j.elementy.back(), gl + 1)) return false;
        spacje();
        if (i >= t.size()) return false;
        if (t[i] == ',') {
          i++;
          continue;
        }
        if (t[i] == ']') {
          i++;
          return true;
        }
        return false;
      }
    }
    if (c == '"') {
      j.typ = Json::Typ::TEKST;
      return tekst(j.tekst);
    }
    if (slowo("true")) {
      j.typ = Json::Typ::LOGICZNA;
      j.logiczna = true;
      return true;
    }
    if (slowo("false")) {
      j.typ = Json::Typ::LOGICZNA;
      return true;
    }
    if (slowo("null")) {
      j.typ = Json::Typ::NUL;
      return true;
    }
    j.typ = Json::Typ::LICZBA;
    return liczba(j.tekst);
  }
};
}  // namespace

bool parsujJson(const std::string& t, Json& wynik) {
  Parser p(t);
  Json j;
  if (!p.wartosc(j, 0)) return false;
  p.spacje();
  if (p.i != t.size()) return false;
  wynik = j;
  return true;
}

bool liczbaZJson(const Json* j, uint64_t maks, uint64_t& wynik) {
  if (!j || (j->typ != Json::Typ::LICZBA && j->typ != Json::Typ::TEKST)) return false;
  const std::string& s = j->tekst;
  if (!cyfry(s) || s.size() > 19) return false;
  uint64_t v = 0;
  for (char c : s) v = v * 10 + (uint64_t)(c - '0');
  if (v > maks) return false;
  wynik = v;
  return true;
}

// ---------- GitHub ----------
std::string nazwaObrazu(const std::string& wersja) { return "cpap-eink-" + bezBudowy(wersja) + ".bin"; }

std::string hostZAdresu(const std::string& url) {
  const std::string p = "https://";
  if (url.size() <= p.size()) return "";
  for (size_t k = 0; k < p.size(); k++)
    if ((char)std::tolower((unsigned char)url[k]) != p[k]) return "";
  size_t a = p.size(), b = url.find_first_of("/?#", a);
  std::string h = url.substr(a, b == std::string::npos ? std::string::npos : b - a);
  if (h.find('@') != std::string::npos) return "";  // dane logowania w adresie: nie ufamy hostowi
  size_t dw = h.find(':');
  if (dw != std::string::npos) h.erase(dw);
  for (auto& c : h) c = (char)std::tolower((unsigned char)c);
  return h;
}

bool wolnoWyslacToken(const std::string& url) { return hostZAdresu(url) == HOST_API; }

std::string adresNajnowszego(const std::string& repo) {
  size_t s = repo.find('/');
  if (s == std::string::npos || s == 0 || s + 1 >= repo.size() || repo.find('/', s + 1) != std::string::npos ||
      repo.size() > 140)
    return "";
  for (char c : repo)
    if (!(c == '/' || c == '.' || c == '_' || znakIdentyfikatora(c))) return "";
  return std::string("https://") + HOST_API + "/repos/" + repo + "/releases/latest";
}

std::vector<std::pair<std::string, std::string>> naglowki(const std::string& url, const std::string& token,
                                                          bool binarne) {
  std::vector<std::pair<std::string, std::string>> h = {
      {"Accept", binarne ? "application/octet-stream" : "application/vnd.github+json"},
      {"User-Agent", "cpap-eink"},
      {"X-GitHub-Api-Version", "2022-11-28"},
  };
  if (!token.empty() && wolnoWyslacToken(url)) h.push_back({"Authorization", "Bearer " + token});
  return h;
}

bool tokenOk(const std::string& t) {
  if (t.size() < 20 || t.size() > 255) return false;
  for (char c : t)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')) return false;
  return true;
}

bool parsujWydanie(const std::string& json, Wydanie& w) {
  Json j;
  if (!parsujJson(json, j) || j.typ != Json::Typ::OBIEKT) return false;
  const Json* tag = j.pole("tag_name");
  if (!tag || tag->typ != Json::Typ::TEKST || tag->tekst.empty()) return false;
  Wydanie n;
  n.tag = tag->tekst;
  std::string obraz = nazwaObrazu(n.tag);
  const Json* as = j.pole("assets");
  if (as && as->typ == Json::Typ::TABLICA) {
    for (auto& e : as->elementy) {
      const Json* nm = e.pole("name");
      const Json* u = e.pole("url");
      if (!nm || !u || nm->typ != Json::Typ::TEKST || u->typ != Json::Typ::TEKST) continue;
      if (hostZAdresu(u->tekst) != HOST_API) continue;  // zasób tylko przez API (token, R-NF-32)
      Zasob z;
      z.nazwa = nm->tekst;
      z.adresApi = u->tekst;
      uint64_t r = 0;
      if (liczbaZJson(e.pole("size"), (uint64_t)1 << 40, r)) z.rozmiar = r;
      if (z.nazwa == NAZWA_OPISU) n.opis = z;
      else if (z.nazwa == obraz) n.obraz = z;
    }
  }
  if (n.opis.adresApi.empty()) return false;
  w = n;
  return true;
}

namespace {
bool hex64(const std::string& s) {
  if (s.size() != 64) return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}
bool tekstPola(const Json& j, const char* n, std::string& v, size_t maks) {
  const Json* p = j.pole(n);
  if (!p || p->typ != Json::Typ::TEKST || p->tekst.empty() || p->tekst.size() > maks) return false;
  v = p->tekst;
  return true;
}
bool ukladZTekstu(const std::string& s, uint32_t& v) {
  std::string h = s;
  if (h.size() > 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X')) h.erase(0, 2);
  if (h.empty() || h.size() > 8) return false;
  uint32_t r = 0;
  for (char c : h) {
    int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    if (d < 0) return false;
    r = r * 16 + (uint32_t)d;
  }
  v = r;
  return true;
}
}  // namespace

bool parsujOpis(const std::string& json, OpisWydania& o) {
  Json j;
  if (json.size() > MAKS_OPISU || !parsujJson(json, j) || j.typ != Json::Typ::OBIEKT) return false;
  OpisWydania n;
  std::string uk;
  if (!tekstPola(j, "wersja", n.wersja, 64) || !tekstPola(j, "plytka", n.plytka, 64) ||
      !tekstPola(j, "sha256", n.sha256, 64) || !tekstPola(j, "plik", n.plik, 128) || !tekstPola(j, "uklad", uk, 10))
    return false;
  Wersja w;
  if (!parsujWersje(n.wersja, w) || !hex64(n.sha256) || !ukladZTekstu(uk, n.uklad) || n.uklad == 0) return false;
  if (!liczbaZJson(j.pole("rozmiar"), 64ull * 1024 * 1024, n.rozmiar) || n.rozmiar == 0) return false;
  n.wersja = bezBudowy(n.wersja);
  o = n;
  return true;
}

std::string zapiszOpis(const OpisWydania& o) {
  char uk[11];
  static const char* const H = "0123456789abcdef";
  uk[0] = '0';
  uk[1] = 'x';
  for (int k = 0; k < 8; k++) uk[2 + k] = H[(o.uklad >> (28 - 4 * k)) & 0xF];
  uk[10] = 0;
  return "{\"wersja\":\"" + o.wersja + "\",\"plytka\":\"" + o.plytka + "\",\"uklad\":\"" + uk +
         "\",\"rozmiar\":" + std::to_string(o.rozmiar) + ",\"sha256\":\"" + o.sha256 + "\",\"plik\":\"" + o.plik +
         "\"}\n";
}

// ---------- SHA-256 ----------
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
inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
}  // namespace

Sha256::Sha256() : h_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}, bufor_{} {}

void Sha256::blok(const uint8_t* b) {
  uint32_t w[64];
  for (size_t i = 0; i < 16; i++)
    w[i] = (uint32_t)b[4 * i] << 24 | (uint32_t)b[4 * i + 1] << 16 | (uint32_t)b[4 * i + 2] << 8 | (uint32_t)b[4 * i + 3];
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = h_[0], b2 = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
  for (int i = 0; i < 64; i++) {
    uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    uint32_t ch = (e & f) ^ (~e & g);
    uint32_t t1 = h + S1 + ch + K256[i] + w[i];
    uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    uint32_t mj = (a & b2) ^ (a & c) ^ (b2 & c);
    uint32_t t2 = S0 + mj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b2;
    b2 = a;
    a = t1 + t2;
  }
  h_[0] += a;
  h_[1] += b2;
  h_[2] += c;
  h_[3] += d;
  h_[4] += e;
  h_[5] += f;
  h_[6] += g;
  h_[7] += h;
}

void Sha256::dodaj(const uint8_t* d, size_t n) {
  if (!d) return;
  bajtow_ += n;
  while (n > 0) {
    size_t k = 64 - wBuforze_;
    if (k > n) k = n;
    std::memcpy(bufor_ + wBuforze_, d, k);
    wBuforze_ += k;
    d += k;
    n -= k;
    if (wBuforze_ == 64) {
      blok(bufor_);
      wBuforze_ = 0;
    }
  }
}

std::string Sha256::hex() {
  uint64_t bity = bajtow_ * 8;
  uint8_t jedynka = 0x80;
  dodaj(&jedynka, 1);
  uint8_t zero = 0;
  while (wBuforze_ != 56) dodaj(&zero, 1);
  uint8_t dl[8];
  for (int i = 0; i < 8; i++) dl[i] = (uint8_t)(bity >> (56 - 8 * i));
  dodaj(dl, 8);
  static const char* const H = "0123456789abcdef";
  std::string s;
  for (uint32_t v : h_)
    for (int k = 28; k >= 0; k -= 4) s += H[(v >> k) & 0xF];
  return s;
}

std::string sha256(const std::string& dane) {
  Sha256 s;
  s.dodaj((const uint8_t*)dane.data(), dane.size());
  return s.hex();
}

// ---------- decyzje ----------
std::string tekstPowodu(Powod p, int proc, const std::string& wersja) {
  switch (p) {
    case Powod::BRAK_SIECI: return "Nie mogę sprawdzić wersji. Brak połączenia z internetem.";
    case Powod::BRAK_TOKENU: return "Aktualizacje wyłączone. Ustaw je w „Więcej na telefonie”.";
    case Powod::TOKEN_ODRZUCONY: return "GitHub odrzucił klucz wydań. Wpisz nowy w „Więcej na telefonie”.";
    case Powod::BATERIA:
      return "Bateria " + (proc >= 0 ? std::to_string(proc) : std::string("–")) +
             "%. Podłącz zasilanie i spróbuj ponownie.";
    case Powod::USZKODZONY: return "Pobrany plik jest uszkodzony. Spróbuj ponownie.";
    case Powod::INNY_PANEL: return "Ta wersja nie pasuje do tego panelu. Potrzebny kabel.";
    case Powod::NIE_WSTALA: return "Nowa wersja nie uruchomiła się poprawnie. Panel wrócił do " + wersja + ".";
    case Powod::NOC: return "Teraz nie instaluję: trwa noc. Spróbuj rano.";
    // tekst z 10.11.2026, do potwierdzenia przez właściciela projektu
    case Powod::BRAK_WYDAN: return "Nie widzę wydań. Sprawdź klucz w „Więcej na telefonie”.";
    case Powod::BRAK:
    case Powod::NIE_NOWSZA: break;
  }
  return "";
}

const char* nazwaPowodu(Powod p) {
  switch (p) {
    case Powod::BRAK: return "brak";
    case Powod::BRAK_SIECI: return "brak sieci";
    case Powod::BRAK_TOKENU: return "brak tokenu";
    case Powod::TOKEN_ODRZUCONY: return "token odrzucony";
    case Powod::BATERIA: return "bateria";
    case Powod::USZKODZONY: return "uszkodzony";
    case Powod::INNY_PANEL: return "inny panel";
    case Powod::NIE_WSTALA: return "nie wstała";
    case Powod::NOC: return "noc albo terapia";
    case Powod::NIE_NOWSZA: return "nie nowsza";
    case Powod::BRAK_WYDAN: return "brak wydań (404)";
  }
  return "?";
}

Powod decyzjaInstalacji(const OpisWydania& o, const DanePanelu& p) {
  if (!nowsza(o.wersja, p.wersja)) return Powod::NIE_NOWSZA;
  if (o.plytka != p.plytka || o.uklad != p.uklad || o.rozmiar > p.slot) return Powod::INNY_PANEL;
  if (!p.zKabla && p.procentBaterii >= 0 && p.procentBaterii < PROG_BATERII) return Powod::BATERIA;
  return Powod::BRAK;
}

bool nocAlboTerapia(int minutaDoby, int rop) { return rop == 1 || (minutaDoby >= 0 && slotNocny(minutaDoby)); }

// ---------- stan ----------
namespace {
void ustawT(Nvs& n, const char* k, const std::string& v) {
  if (n.tekst(PRZESTRZEN, k, "") != v) n.ustawTekst(PRZESTRZEN, k, v);
}
void ustawL(Nvs& n, const char* k, uint32_t v) {
  if (n.liczbaU(PRZESTRZEN, k, 0) != v) n.ustawLiczbeU(PRZESTRZEN, k, v);
}
}  // namespace

Stan wczytaj(Nvs& n) {
  Stan s;
  s.znana = n.tekst(PRZESTRZEN, "znana", "");
  s.sprawdzonoEp = n.liczbaU(PRZESTRZEN, "spr_ep", 0);
  s.sprawdzonoDnia = n.tekst(PRZESTRZEN, "spr_d", "");
  s.probaWersja = n.tekst(PRZESTRZEN, "pr_w", "");
  uint32_t pn = n.liczbaU(PRZESTRZEN, "pr_n", 0);
  s.probNieudanych = pn > 100 ? 100 : (int)pn;  // kontrola zakresu liczby z NVS
  s.probaDnia = n.tekst(PRZESTRZEN, "pr_d", "");
  s.instalujeZ = n.tekst(PRZESTRZEN, "in_z", "");
  s.instalujeNa = n.tekst(PRZESTRZEN, "in_na", "");
  s.automatyczna = n.flaga(PRZESTRZEN, "in_a", false);
  uint32_t w = n.liczbaU(PRZESTRZEN, "wynik", 0);
  s.ostatni = w <= (uint32_t)POWOD_OSTATNI ? (Powod)w : Powod::BRAK;
  s.odrzucona = n.tekst(PRZESTRZEN, "odrz", "");
  s.siecDnia = n.tekst(PRZESTRZEN, "au_d", "");
  uint32_t an = n.liczbaU(PRZESTRZEN, "au_n", 0);
  s.siecZapytan = an > 100 ? 100 : (int)an;
  s.siecKoniec = n.flaga(PRZESTRZEN, "au_k", false);
  return s;
}

void zapisz(Nvs& n, const Stan& s) {
  ustawT(n, "znana", s.znana);
  ustawL(n, "spr_ep", s.sprawdzonoEp);
  ustawT(n, "spr_d", s.sprawdzonoDnia);
  ustawT(n, "pr_w", s.probaWersja);
  ustawL(n, "pr_n", (uint32_t)(s.probNieudanych < 0 ? 0 : s.probNieudanych));
  ustawT(n, "pr_d", s.probaDnia);
  ustawT(n, "in_z", s.instalujeZ);
  ustawT(n, "in_na", s.instalujeNa);
  if (n.flaga(PRZESTRZEN, "in_a", false) != s.automatyczna) n.ustawFlage(PRZESTRZEN, "in_a", s.automatyczna);
  ustawL(n, "wynik", (uint32_t)s.ostatni);
  ustawT(n, "odrz", s.odrzucona);
  ustawT(n, "au_d", s.siecDnia);
  ustawL(n, "au_n", (uint32_t)(s.siecZapytan < 0 ? 0 : s.siecZapytan));
  if (n.flaga(PRZESTRZEN, "au_k", false) != s.siecKoniec) n.ustawFlage(PRZESTRZEN, "au_k", s.siecKoniec);
}

bool bladTrwaly(Powod p) {
  return p == Powod::TOKEN_ODRZUCONY || p == Powod::BRAK_WYDAN || p == Powod::USZKODZONY || p == Powod::INNY_PANEL ||
         p == Powod::NIE_NOWSZA || p == Powod::BRAK_TOKENU;
}

static void nowaDobaSieci(Stan& s, const std::string& dzis) {
  if (s.siecDnia == dzis) return;
  s.siecDnia = dzis;
  s.siecZapytan = 0;
  s.siecKoniec = false;
}

bool automatMozeSiec(const Stan& s, const std::string& dzis) {
  if (dzis.empty()) return false;
  if (s.siecDnia != dzis) return true;
  return !s.siecKoniec && s.siecZapytan < AUTOMAT_ZAPYTAN_NA_DOBE;
}

void poZapytaniuAutomatu(Stan& s, const std::string& dzis, Powod wynik, bool zapytanie) {
  nowaDobaSieci(s, dzis);
  if (zapytanie && s.siecZapytan < 100) s.siecZapytan++;
  if (bladTrwaly(wynik)) s.siecKoniec = true;
}

void poSprawdzeniuDobowym(Stan& s, const std::string& dzis, Powod wynik) {
  if (!bladTrwaly(wynik) || wynik == Powod::NIE_NOWSZA) return;  // NIE_NOWSZA: znana wersja i tak wyczyszczona
  nowaDobaSieci(s, dzis);
  s.siecKoniec = true;
}

bool sprawdzicDzis(const Stan& s, const std::string& dzis, bool wifi, bool token, bool czas, int minutaDoby, int rop) {
  if (!wifi || !token || !czas || dzis.empty() || minutaDoby < 0 || slotNocny(minutaDoby) || rop == 1) return false;
  return s.sprawdzonoDnia != dzis;
}

PowodAutomatu decyzjaAutomatu(const Stan& s, const std::string& wersja, const WejscieAutomatu& w) {
  if (!w.wlaczony) return PowodAutomatu::WYLACZONY;
  if (wersja.empty()) return PowodAutomatu::BRAK_WERSJI;
  if (nocAlboTerapia(w.minutaDoby, w.rop) || w.minutaDoby < 0) return PowodAutomatu::NOC;
  if (w.sprawaAB) return PowodAutomatu::SPRAWA_AB;
  if (s.probaWersja == wersja && s.probNieudanych >= PROBY_MAKS) return PowodAutomatu::POMINIETA;
  if (s.probaDnia == w.dzis && !w.dzis.empty()) return PowodAutomatu::DZIS_BYLA_PROBA;
  return PowodAutomatu::TAK;
}

const char* nazwaPowoduAutomatu(PowodAutomatu p) {
  switch (p) {
    case PowodAutomatu::TAK: return "instaluję";
    case PowodAutomatu::WYLACZONY: return "automat wyłączony";
    case PowodAutomatu::NOC: return "noc albo terapia";
    case PowodAutomatu::SPRAWA_AB: return "sprawa A albo B na ekranie";
    case PowodAutomatu::DZIS_BYLA_PROBA: return "dziś już była próba";
    case PowodAutomatu::POMINIETA: return "wersja pominięta po 3 nieudanych próbach";
    case PowodAutomatu::BRAK_WERSJI: return "brak nowszej wersji";
  }
  return "?";
}

void zacznijProbe(Stan& s, const std::string& wersja, const std::string& wlasna, const std::string& dzis, bool automat) {
  if (s.probaWersja != wersja) {
    s.probaWersja = wersja;
    s.probNieudanych = 0;
  }
  if (s.probNieudanych < 100) s.probNieudanych++;  // liczona z góry: udana próba czyści licznik po restarcie
  s.probaDnia = dzis;
  s.instalujeZ = bezBudowy(wlasna);
  s.instalujeNa = bezBudowy(wersja);
  s.automatyczna = automat;
  s.ostatni = Powod::BRAK;
}

void nieudanaProba(Stan& s, Powod p) {
  s.ostatni = p;
  s.instalujeZ.clear();
  s.instalujeNa.clear();
  s.automatyczna = false;
}

PoStarcie poStarcie(Stan& s, const std::string& wlasna0) {
  if (s.instalujeNa.empty()) return PoStarcie::NIC;
  std::string wlasna = bezBudowy(wlasna0);
  PoStarcie w = PoStarcie::NIC;
  if (wlasna == s.instalujeNa) {
    w = PoStarcie::ZAINSTALOWANO;
    s.probaWersja.clear();
    s.probNieudanych = 0;
    s.ostatni = Powod::BRAK;
    s.odrzucona.clear();
    if (!nowsza(s.znana, wlasna)) s.znana.clear();
  } else if (wlasna == s.instalujeZ) {
    w = PoStarcie::NIE_WSTALA;
    s.ostatni = Powod::NIE_WSTALA;
    s.odrzucona = s.instalujeNa;
  }
  s.instalujeZ.clear();
  s.instalujeNa.clear();
  return w;
}

void poSprawdzeniu(Stan& s, const std::string& najnowsza, const std::string& wlasna, uint32_t teraz,
                   const std::string& dzienAutomatu) {
  s.znana = nowsza(najnowsza, wlasna) ? bezBudowy(najnowsza) : "";
  if (teraz) s.sprawdzonoEp = teraz;
  if (!dzienAutomatu.empty()) s.sprawdzonoDnia = dzienAutomatu;
}

// ---------- teksty ----------
TresciU07 trescU07(const DaneU07& d) {
  TresciU07 t;
  const std::string w = d.wlasna, x = d.nowa;
  switch (d.stan) {
    case StanU07::SPRAWDZANIE:
      t.linie = {"Sprawdzam, czy jest nowa wersja…"};
      t.stopka = {"↻ ekran dnia"};
      break;
    case StanU07::AKTUALNA:
      t.linie = {"Masz aktualną wersję " + w + "."};
      if (!d.sprawdzono.empty()) t.linie.push_back("Sprawdzono " + d.sprawdzono + ".");
      t.stopka = {"◀ ustawienia", "↻ ekran dnia"};
      break;
    case StanU07::NOWSZA:
      t.linie = {"Jest wersja " + x + ". Masz " + w + ".", "Zainstalować teraz? Potrwa około 3 minut.",
                 "Nie odłączaj zasilania. Dane z nocy zostają."};
      t.stopka = {"◀ nie teraz", "▶ tak, instaluj", "↻ ekran dnia"};
      break;
    case StanU07::POBIERANIE:
      t.linie = {"Pobieram wersję " + x + "…", "Nie odłączaj zasilania."};
      break;
    case StanU07::INSTALACJA:
      t.linie = {"Instaluję. Panel zaraz uruchomi się ponownie."};
      break;
    case StanU07::SAMOSPRAWDZENIE:  // restart już był; tekst do potwierdzenia przez właściciela projektu
      t.linie = {"Sprawdzam nową wersję…"};
      break;
    case StanU07::PO_UDANEJ:
      t.linie = {"Zainstalowano wersję " + x + "."};
      t.stopka = {"◀ ustawienia", "↻ ekran dnia"};
      break;
    case StanU07::PO_NIEUDANEJ:
      t.linie = {"Aktualizacja się nie udała. Zostaje wersja " + w + "."};
      if (d.powod != Powod::BRAK && d.powod != Powod::NIE_NOWSZA)
        t.linie.push_back(tekstPowodu(d.powod, d.procentBaterii, w));
      t.stopka = {"◀ ustawienia", "▶ spróbuj ponownie", "↻ ekran dnia"};
      break;
    case StanU07::WYLACZONE:
      t.linie = {tekstPowodu(Powod::BRAK_TOKENU, -1, w)};
      t.stopka = {"◀ ustawienia", "↻ ekran dnia"};
      break;
    case StanU07::SPRAWDZENIE_NIEUDANE:  // bez „Aktualizacja się nie udała” (nic nie instalowano)
      t.linie = {tekstPowodu(powodSprawdzenia(d.powod) ? d.powod : Powod::BRAK_SIECI, d.procentBaterii, w)};
      t.stopka = {"◀ ustawienia", "▶ spróbuj ponownie", "↻ ekran dnia"};
      break;
  }
  return t;
}

std::string wierszU01(bool token, const std::string& znana, const std::string& wlasna) {
  if (!token) return tekstPowodu(Powod::BRAK_TOKENU, -1, wlasna);
  if (nowsza(znana, wlasna)) return "Jest wersja " + bezBudowy(znana) + " · ↻ 2 s Aktualizacja";
  return "Oprogramowanie " + bezBudowy(wlasna) + " · ↻ 2 s Sprawdź aktualizację";
}

std::string dopisekU05(const Stan& s) {
  if (!s.odrzucona.empty() && s.ostatni == Powod::NIE_WSTALA) return " · odrzucona " + s.odrzucona;
  if (!s.znana.empty()) return " · jest wersja " + s.znana;
  return "";
}

bool ikonaAktualizacji(const Stan& s, const std::string& wlasna, bool token) {
  return token && nowsza(s.znana, wlasna);
}

bool powodSprawdzenia(Powod p) {
  return p == Powod::BRAK_SIECI || p == Powod::TOKEN_ODRZUCONY || p == Powod::BRAK_WYDAN || p == Powod::USZKODZONY ||
         p == Powod::BRAK_TOKENU;
}

bool kodPrzekierowania(int kod) { return kod == 301 || kod == 302 || kod == 303 || kod == 307 || kod == 308; }

std::string nastepnyAdres(const std::string& obecny, const std::string& location, int krok) {
  if (krok > PRZEKIEROWAN_MAKS || location.empty() || location.size() > 2048) return "";
  for (char c : location)
    if ((unsigned char)c < 0x21 || c == 0x7F) return "";  // spacje i znaki sterujące: nie
  if (location[0] == '/') {
    if (location.size() > 1 && location[1] == '/') return "";  // `//host` bez schematu
    std::string h = hostZAdresu(obecny);
    if (h.empty()) return "";
    size_t a = 8, b = obecny.find_first_of("/?#", a);
    return obecny.substr(0, b == std::string::npos ? std::string::npos : b) + location;
  }
  return hostZAdresu(location).empty() ? "" : location;  // tylko https:// bez danych logowania
}

bool cialoKompletne(long contentLength, size_t odczytane, bool zamkniete, size_t maks) {
  if (maks && odczytane > maks) return false;
  if (contentLength >= 0) return odczytane == (size_t)contentLength;
  return zamkniete;
}

size_t ileZapisac(uint64_t zapisane, uint64_t rozmiar, size_t n) {
  if (zapisane >= rozmiar) return 0;
  uint64_t zostalo = rozmiar - zapisane;
  return zostalo < n ? (size_t)zostalo : n;
}

}  // namespace aktualizacja
